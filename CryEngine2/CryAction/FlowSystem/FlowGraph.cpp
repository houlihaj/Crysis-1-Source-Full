#include "StdAfx.h"
#include "IAIAction.h"

#include "FlowGraph.h"
#include "FlowSystem.h"
#include "StringUtils.h"

// Debug disabled edges in FlowGraphs
#define FLOW_DEBUG_DISABLED_EDGES
#undef FLOW_DEBUG_DISABLED_EDGES

// Debug mapping from NodeIDs (==char* Name) to internal IDs (index in std::vector)
#define FLOW_DEBUG_ID_MAPPING
#undef FLOW_DEBUG_ID_MAPPING

// #define FLOW_DEBUG_PENDING_UPDATES is defined in header file FlowGraph.h

#define NODE_NAME_ATTR "ID"
#define NODE_TYPE_ATTR "Class"

CFlowGraphBase * CFlowGraphBase::m_pUpdateRoot = 0;

struct SFGProfile
{
	int graphsUpdated;
	int nodeActivations;
	int nodeUpdates;

	void Reset()
	{
		memset( this, 0, sizeof(*this) );
	}
};

static SFGProfile g_profile_data;

class CFlowGraphBase::CNodeIterator : public IFlowNodeIterator
{
public:
	CNodeIterator( CFlowGraphBase * pParent )  : 
		m_refs(0),
		m_pParent(pParent)
		{
			m_begin = pParent->m_nodeNameToId.begin();
			m_iter = m_begin;
			m_pParent->AddRef();
		}
			~CNodeIterator()
			{
				m_pParent->Release();
			}

			void AddRef()
			{
				++m_refs;
			}

			void Release()
			{
				if (0 == --m_refs)
					delete this;
			}

			IFlowNodeData * Next( TFlowNodeId &id )
			{
				IFlowNodeData * out = NULL;
				if (m_iter != m_pParent->m_nodeNameToId.end())
				{
					id = m_iter->second;
					out = &m_pParent->m_flowData[id];
					++m_iter;
				}
				return out;
			}

private:
	int m_refs;
	CFlowGraphBase * m_pParent;
	std::map<string, TFlowNodeId>::iterator m_begin;
	std::map<string, TFlowNodeId>::iterator m_iter;
};

class CFlowGraphBase::CEdgeIterator : public IFlowEdgeIterator
{
public:
	CEdgeIterator( CFlowGraphBase * pParent )  : 
		m_refs(0),
		m_pParent(pParent),
		m_iter(pParent->m_edges.begin())
	{
		m_pParent->AddRef();
	}
	~CEdgeIterator()
	{
		m_pParent->Release();
	}

	void AddRef()
	{
		++m_refs;
	}

	void Release()
	{
		if (0 == --m_refs)
			delete this;
	}

	bool Next( Edge& edge )
	{
		if (m_iter == m_pParent->m_edges.end())
			return false;
		edge.fromNodeId = m_iter->fromNode;
		edge.toNodeId = m_iter->toNode;
		edge.fromPortId = m_iter->fromPort;
		edge.toPortId = m_iter->toPort;
		++m_iter;
		return true;
	}

private:
	int m_refs;
	CFlowGraphBase * m_pParent;
	std::vector<SEdge>::const_iterator m_iter;
};

class CFlowGraphBase::SEdgeHasNode
{
public:
	ILINE SEdgeHasNode( TFlowNodeId id ) : m_id(id) {}
	ILINE bool operator()( const SEdge& edge ) const
	{
		return edge.fromNode == m_id || edge.toNode == m_id;
	}

private:
	TFlowNodeId m_id;
};

CFlowGraphBase::CFlowGraphBase( CFlowSystem* pSys )
{
	m_pSys = pSys;
	m_pEntitySystem = gEnv->pEntitySystem;
	m_firstModifiedNode = END_OF_MODIFIED_LIST;
	m_firstActivatingNode = END_OF_MODIFIED_LIST;
	m_firstFinalActivatingNode = END_OF_MODIFIED_LIST;
	m_bEdgesSorted = true; // no edges => sorted :)
	m_nRefs = 0;
	m_bNeedsInitialize = true;

	for (int i = 0; i < MAX_GRAPH_ENTITIES; i++)
		m_graphEntityId[i] = 0;

	m_pUpdateNext = FLOWGRAPH_NOT_IN_LIST;
	m_bInUpdate = false;
	m_bEnabled = true;
	m_bActive = true;
	m_bSuspended = false;
	m_bIsAIAction = false;
	m_pAIAction = NULL;

	m_pSys->RegisterGraph(this);

#if defined (FLOW_DEBUG_PENDING_UPDATES)
	CreateDebugName();
#endif
}

void CFlowGraphBase::SetEnabled( bool bEnable )
{
	if ( m_bEnabled != bEnable )
	{
		m_bEnabled = bEnable;
		if (bEnable) NeedUpdate();
	}
}

void CFlowGraphBase::SetActive( bool bActive )
{
	if ( m_bActive != bActive )
	{
		m_bActive = bActive;
		if (bActive) NeedUpdate();
	}
}

void CFlowGraphBase::RegisterFlowNodeActivationListener(SFlowNodeActivationListener *listener)
{
	if(!stl::find(m_flowNodeActivationListeners, listener))
		m_flowNodeActivationListeners.push_back(listener);
}

void CFlowGraphBase::RemoveFlowNodeActivationListener(SFlowNodeActivationListener *listener)
{
	std::vector<SFlowNodeActivationListener*>::iterator it = std::find(m_flowNodeActivationListeners.begin(), m_flowNodeActivationListeners.end(), listener);
	if(it != m_flowNodeActivationListeners.end())
		m_flowNodeActivationListeners.erase(it);
}

void CFlowGraphBase::NotifyFlowNodeActivationListeners(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, CryStringT<char> value)
{
	NotifyFlowNodeActivationListeners(srcNode, srcPort, toNode, toPort, value.c_str());
}

void CFlowGraphBase::NotifyFlowNodeActivationListeners(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const char * value)
{
	std::vector<SFlowNodeActivationListener*>::iterator it = m_flowNodeActivationListeners.begin();
	std::vector<SFlowNodeActivationListener*>::iterator end = m_flowNodeActivationListeners.end();
	for(; it != end; ++it)
		(*it)->OnFlowNodeActivation(srcNode, srcPort, toNode, toPort, value);
}

template<class T>
void CFlowGraphBase::NotifyFlowNodeActivationListeners(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, T &value)
{
	string val = CryStringUtils::toString(value);
	NotifyFlowNodeActivationListeners(srcNode, srcPort, toNode, toPort, val.c_str());
}

void CFlowGraphBase::SetSuspended( bool suspend )
{
	if ( m_bSuspended != suspend )
	{
		m_bSuspended = false;

		IFlowNode::SActivationInfo activationInfo(this, 0);
		for ( std::vector< CFlowData >::iterator it = m_flowData.begin(); it != m_flowData.end(); ++it )
		{
			if ( !it->IsValid() )
				continue;

			activationInfo.myID = it - m_flowData.begin();
			activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
			it->SetSuspended( &activationInfo, suspend );
		}

		if ( suspend )
			m_bSuspended = true;
		else
			NeedUpdate();
	}
}

bool CFlowGraphBase::IsSuspended() const
{
	return m_bSuspended;
}

void CFlowGraphBase::SetAIAction( IAIAction* pAIAction )
{
#if !defined(FLOW_DEBUG_PENDING_UPDATES)
	m_pAIAction = pAIAction;
	if (pAIAction) 
		m_bIsAIAction = true; // once an AIAction, always an AIAction
#else
	if (pAIAction)
	{
		m_bIsAIAction = true; // once an AIAction, always an AIAction
		m_pAIAction = pAIAction;
		CreateDebugName();
	}
	else
		m_pAIAction = 0;
#endif
}

IAIAction* CFlowGraphBase::GetAIAction() const
{
	return m_pAIAction;
}

void CFlowGraphBase::Cleanup()
{
	m_firstModifiedNode = END_OF_MODIFIED_LIST;
	m_firstActivatingNode = END_OF_MODIFIED_LIST;
	m_firstFinalActivatingNode = END_OF_MODIFIED_LIST;
	m_bEdgesSorted = true; // no edges => sorted :)
	m_modifiedNodes.resize(0);
	m_activatingNodes.resize(0);
	m_finalActivatingNodes.resize(0);
	m_flowData.resize(0);
	m_deallocatedIds.resize(0);
	m_edges.resize(0);
	m_nodeNameToId.clear();
	m_regularUpdates.resize(0);
	m_activatingUpdates.resize(0);
	m_inspectors.resize(0);
}

CFlowGraphBase::~CFlowGraphBase()
{
//	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if ( m_pSys )
		m_pSys->UnregisterGraph(this);

	if (m_pUpdateNext != FLOWGRAPH_NOT_IN_LIST)
	{
#if defined (FLOW_DEBUG_PENDING_UPDATES)
		DebugPendingActivations();
#else
		if (m_bIsAIAction == false)
		{
			GameWarning("[flow] FlowGraph 0x%p [non AIAction] was destroyed with pending updates or activations; these operations will not be performed.", this);
		}
#endif

		if (m_pUpdateRoot == this)
			m_pUpdateRoot = m_pUpdateNext;
		else
		{
			CFlowGraphBase * pCur = m_pUpdateRoot;
			while (pCur->m_pUpdateNext != this)
				pCur = pCur->m_pUpdateNext;
			pCur->m_pUpdateNext = m_pUpdateNext;
		}
	}
	
	Cleanup();
}

#if defined (FLOW_DEBUG_PENDING_UPDATES)

namespace {
	string GetPrettyNodeName(IFlowGraph * pGraph, TFlowNodeId id)
	{
		const char *typeName = pGraph->GetNodeTypeName(id);
		const char *nodeName  = pGraph->GetNodeName(id);
		string human;
		human+=typeName;
		human+=" Node:";

		if (nodeName == 0)
		{
			IEntity *pEntity = ((CFlowGraph*)pGraph)->GetIEntityForNode(id);
			if (pEntity)
				human+=pEntity->GetName();
		}
		else
			human+=nodeName;
		return human;
	}
};

const char* CFlowGraphBase::InternalGetDebugName()
{
	static char buf[128];
	int count;

	if (m_pAIAction != 0)
		count = _snprintf(buf, sizeof(buf), "FG-0x%p-AIAction '%s'", this, m_pAIAction->GetName());
	else 
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(GetGraphEntity(0));
		if (pEntity != 0)
			count = _snprintf(buf,sizeof(buf),"FG-0x%p-Entity '%s'", this, pEntity->GetName());
		else
			count = _snprintf(buf,sizeof(buf),"FG-0x%p", this);
	}

	if (count == -1) {
		buf[sizeof(buf)-4] = '.';
		buf[sizeof(buf)-3] = '.';
		buf[sizeof(buf)-2] = '.';
		buf[sizeof(buf)-1] = '\0';
	}

	return buf;
}

void CFlowGraphBase::CreateDebugName()
{
	m_debugName = InternalGetDebugName();
}

void CFlowGraphBase::DebugPendingActivations()
{
	if (m_bIsAIAction == true)
		return;

	GameWarning("[flow] %s was destroyed with pending updates/activations. Report follows...", m_debugName.c_str());

	if (m_pSys == 0)
	{
		GameWarning("[flow] FlowSystem already shutdown.");
		return;
	}

	GameWarning("[flow] Pending nodes:");

	TFlowNodeId id = m_firstModifiedNode;
	// traverse the list of modified nodes
	while (id != END_OF_MODIFIED_LIST)
	{
		assert (ValidateNode(id) == true);
		bool bIsReg (std::find(m_regularUpdates.begin(),m_regularUpdates.end(), id) != m_regularUpdates.end());
		GameWarning ("[flow] %s regular=%d", GetPrettyNodeName(this, id).c_str(),bIsReg);
		TFlowNodeId nextId = m_modifiedNodes[id];
		m_modifiedNodes[id] = NOT_MODIFIED;
		id = nextId;
	}

	id = m_firstFinalActivatingNode;

	if (id != m_firstFinalActivatingNode)
		GameWarning("[flow] Pending nodes [final activations]:");

	while (id != END_OF_MODIFIED_LIST)
	{
		assert (ValidateNode(id) == true);
		bool bIsReg (std::find(m_regularUpdates.begin(),m_regularUpdates.end(), id) != m_regularUpdates.end());
		GameWarning ("[flow] %s regular=%d", GetPrettyNodeName(this, id).c_str(),bIsReg);
		TFlowNodeId nextId = m_finalActivatingNodes[id];
		m_finalActivatingNodes[id] = NOT_MODIFIED;
		id = nextId;
	}
}

#endif


void CFlowGraphBase::UpdateAll()
{
	CFlowGraphBase * pCur = m_pUpdateRoot;
	m_pUpdateRoot = 0;
	while (pCur)
	{
		CFlowGraphBase * pFG = pCur;
		pCur = pFG->m_pUpdateNext;
		pFG->m_pUpdateNext = FLOWGRAPH_NOT_IN_LIST;
		pFG->Update();
	}

	if (CFlowSystemCVars::Get().m_profile != 0)
	{
		IRenderer * pRend = gEnv->pRenderer;
		float white[4] = {1,1,1,1};
		static const size_t BUFSZ = 512;

		pRend->Draw2dLabel( 10, 100, 2, white, false, "Number of Flow Graphs Updated: %d", g_profile_data.graphsUpdated );
		pRend->Draw2dLabel( 10, 120, 2, white, false, "Number of Flow Graph Nodes Updated: %d", g_profile_data.nodeUpdates );
		pRend->Draw2dLabel( 10, 140, 2, white, false, "Number of Flow Graph Nodes Activated: %d", g_profile_data.nodeActivations );
	}
	g_profile_data.Reset();
}

void CFlowGraphBase::AddRef()
{
	m_nRefs ++;
}

void CFlowGraphBase::Release()
{
	assert( m_nRefs > 0 );
	if (0 == --m_nRefs)
		delete this;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::RegisterHook( IFlowGraphHookPtr p )
{
	stl::push_back_unique( m_hooks, p );
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::UnregisterHook( IFlowGraphHookPtr p )
{
	stl::find_and_erase( m_hooks, p );
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::SetGraphEntity( EntityId id,int nIndex )
{
	if (nIndex >= 0 && nIndex < MAX_GRAPH_ENTITIES)
		m_graphEntityId[nIndex] = id;
#if defined (FLOW_DEBUG_PENDING_UPDATES)
	CreateDebugName();
#endif
}

//////////////////////////////////////////////////////////////////////////
EntityId CFlowGraphBase::GetGraphEntity( int nIndex )
{
	if (nIndex >= 0 && nIndex < MAX_GRAPH_ENTITIES)
		return m_graphEntityId[nIndex];
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IFlowGraphPtr CFlowGraphBase::Clone()
{
	if (m_pUpdateNext != FLOWGRAPH_NOT_IN_LIST)
		UpdateAll();

	CFlowGraphBase * pClone = new CFlowGraph( m_pSys );
	CloneInner( pClone );

	return pClone;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::CloneInner( CFlowGraphBase * pClone )
{
	pClone->m_activatingNodes = m_activatingNodes;
	pClone->m_activatingUpdates = m_activatingUpdates;
	pClone->m_bEdgesSorted = m_bEdgesSorted;
	pClone->m_deallocatedIds = m_deallocatedIds;
	pClone->m_edges = m_edges;
	pClone->m_firstActivatingNode = m_firstActivatingNode;
	pClone->m_firstModifiedNode = m_firstModifiedNode;
	pClone->m_firstFinalActivatingNode = m_firstFinalActivatingNode;
	pClone->m_flowData = m_flowData;
	pClone->m_modifiedNodes = m_modifiedNodes;
	pClone->m_finalActivatingNodes = m_finalActivatingNodes;
	pClone->m_nodeNameToId = m_nodeNameToId;
	pClone->m_regularUpdates = m_regularUpdates;
	pClone->m_hooks = m_hooks;
	pClone->m_bEnabled = m_bEnabled;
	// pClone->m_bActive = m_bActive;
	// pClone->m_bSuspended = m_bSuspended;
#if defined (FLOW_DEBUG_PENDING_UPDATES)
	pClone->CreateDebugName();
#endif

	IFlowNode::SActivationInfo info;
	info.pGraph = pClone;
	for (std::vector<CFlowData>::iterator iter = pClone->m_flowData.begin(); iter != pClone->m_flowData.end(); ++iter)
	{
		info.myID = iter - pClone->m_flowData.begin();
		if (iter->IsValid())
			iter->CloneImpl( &info );
	}

	// 
	/*
	std::vector<SEdge>::const_iterator itEdges, itEdgesEnd = m_edges.end();
	for ( itEdges = m_edges.begin(); itEdges != itEdgesEnd; ++itEdges )
	{
		IFlowNode::SActivationInfo info(this);
		info.myID = itEdges->fromNode;
		info.connectPort = itEdges->fromPort;
		m_flowData[itEdges->fromNode].GetNode()->ProcessEvent( IFlowNode::eFE_ConnectOutputPort, &info );
		info.myID = itEdges->toNode;
		info.connectPort = itEdges->toPort;
		m_flowData[itEdges->toNode].GetNode()->ProcessEvent( IFlowNode::eFE_ConnectInputPort, &info );
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::Clear()
{
	Cleanup();
}

//////////////////////////////////////////////////////////////////////////
SFlowAddress CFlowGraphBase::ResolveAddress( const char * addr, bool isOutput )
{
	SFlowAddress flowAddr;
	const char * colonPos = strchr( addr, ':' );
	if (colonPos)
	{
		flowAddr = ResolveAddress( string(addr,colonPos), colonPos+1, isOutput );
	}
	return flowAddr;
}

SFlowAddress CFlowGraphBase::ResolveAddress( string node, string port, bool isOutput )
{
	SFlowAddress flowAddr;
	flowAddr.node = stl::find_in_map( m_nodeNameToId, node, InvalidFlowNodeId );
	if (flowAddr.node == InvalidFlowNodeId)
		return SFlowAddress();
	if (!m_flowData[flowAddr.node].ResolvePort( port, flowAddr.port, isOutput ))
		return SFlowAddress();
	flowAddr.isOutput = isOutput;
	return flowAddr;
}

TFlowNodeId CFlowGraphBase::ResolveNode( const char * name )
{
	return stl::find_in_map( m_nodeNameToId, name, InvalidFlowNodeId );
}

void CFlowGraphBase::GetNodeConfiguration( TFlowNodeId id, SFlowNodeConfig& config )
{
	assert( ValidateNode(id) );
	m_flowData[id].GetConfiguration( config );
}

EntityId CFlowGraphBase::GetEntityId(TFlowNodeId id)
{
	assert( ValidateNode(id) );
	return m_flowData[id].GetEntityId();
}

void CFlowGraphBase::SetEntityId(TFlowNodeId nodeId, EntityId entityId)
{
	assert( ValidateNode(nodeId) );
	if (m_flowData[nodeId].SetEntityId(entityId))
		ActivateNodeInt(nodeId);
}

string CFlowGraphBase::PrettyAddress( SFlowAddress addr )
{
	CFlowData& data = m_flowData[addr.node];
	return data.GetName() + ':' + data.GetPortName( addr.port, addr.isOutput );
}

bool CFlowGraphBase::ValidateNode( TFlowNodeId id )
{
	if (id == InvalidFlowNodeId)
		return false;
	if (id >= m_flowData.size())
		return false;
	return m_flowData[id].IsValid();
}

bool CFlowGraphBase::ValidateAddress( const SFlowAddress from )
{
	if (!ValidateNode(from.node))
		return false;
	if (!m_flowData[from.node].ValidatePort(from.port, from.isOutput))
		return false;
	return true;
}

bool CFlowGraphBase::ValidateLink( SFlowAddress& from, SFlowAddress& to )
{
	// can't link output->output, or input->input
	if (from.isOutput == to.isOutput)
	{
		const char * type = from.isOutput? "output" : "input";
		GameWarning( "[flow] Attempt to link an %s node to an %s node", type, type );
		return false;
	}
	// check order is correct, and fix it up if not
	if (!from.isOutput)
	{
		GameWarning( "[flow] Attempt to link from an input node to an output node: reversing" );
		std::swap( from, to );
	}
	// validate that the addresses are correct
	if (!ValidateAddress(from) || !ValidateAddress(to))
	{
		GameWarning( "[flow] Trying to link an invalid node" );
		return false;
	}

	return true;
}

bool CFlowGraphBase::LinkNodes( SFlowAddress from, SFlowAddress to )
{
	if (!ValidateLink( from, to ))
		return false;

	// add this link to the edges collection
	m_edges.push_back( SEdge(from, to) );
	// and make sure that we re-sort soon
	m_bEdgesSorted = false;

	IFlowNode::SActivationInfo info(this);
	info.myID = from.node;
	info.connectPort = from.port;
	m_flowData[from.node].GetNode()->ProcessEvent( IFlowNode::eFE_ConnectOutputPort, &info );
	info.myID = to.node;
	info.connectPort = to.port;
	m_flowData[to.node].GetNode()->ProcessEvent( IFlowNode::eFE_ConnectInputPort, &info );

	NeedInitialize();

	return true;
}

void CFlowGraphBase::UnlinkNodes( SFlowAddress from, SFlowAddress to )
{
	if (!ValidateLink( from, to ))
		return;

	EnsureSortedEdges();

	SEdge findEdge(from, to);
	std::vector<SEdge>::iterator iter = std::lower_bound( m_edges.begin(), m_edges.end(), findEdge );
	if (iter != m_edges.end() && *iter == findEdge)
	{
		IFlowNode::SActivationInfo info(this);
		info.myID = from.node;
		info.connectPort = from.port;
		m_flowData[from.node].GetNode()->ProcessEvent( IFlowNode::eFE_DisconnectOutputPort, &info );
		info.myID = to.node;
		info.connectPort = to.port;
		m_flowData[to.node].GetNode()->ProcessEvent( IFlowNode::eFE_DisconnectInputPort, &info );

		m_edges.erase( iter );
		m_bEdgesSorted = false;
	}
	else
	{
		GameWarning( "[flow] Link not found" );
	}
}

//////////////////////////////////////////////////////////////////////////
TFlowNodeId CFlowGraphBase::CreateNode( TFlowNodeTypeId typeId, const char *name, void *pUserData )
{
	std::pair<CFlowData *, TFlowNodeId> nd = CreateNodeInt( typeId,name,pUserData );
	return nd.second;
}

//////////////////////////////////////////////////////////////////////////
TFlowNodeId CFlowGraphBase::CreateNode( const char* typeName, const char *name, void *pUserData )
{
	std::pair<CFlowData *, TFlowNodeId> nd = CreateNodeInt( m_pSys->GetTypeId(typeName),name,pUserData );
	return nd.second;
}

//////////////////////////////////////////////////////////////////////////
IFlowNodeData* CFlowGraphBase::GetNodeData( TFlowNodeId id )
{
	assert(ValidateNode(id));
	if (id < m_flowData.size())
	{
		return &m_flowData[id];
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
std::pair<CFlowData *, TFlowNodeId> CFlowGraphBase::CreateNodeInt( TFlowNodeTypeId typeId, const char * name, void *pUserData )
{
	typedef std::pair<CFlowData *, TFlowNodeId> R;

	// make sure we only allocate for the name and type once...
	string sName = name;

	if (m_nodeNameToId.find(sName) != m_nodeNameToId.end())
	{
		GameWarning( "[flow] Trying to create a node with the same name as an existing name: %s", sName.c_str() );
		return R(NULL, InvalidFlowNodeId);
	}

	// allocate a node id
	TFlowNodeId id = AllocateId();
	if (id == InvalidFlowNodeId)
	{
		GameWarning( "[flow] Unable to create node id for node named %s", sName.c_str() );
		return R(NULL, InvalidFlowNodeId);
	}

	// create the actual node
	IFlowNode::SActivationInfo activationInfo(this, id, pUserData);
	IFlowNodePtr pNode = CreateNodeOfType(&activationInfo, typeId);
	if (!pNode)
	{
		const char* typeName = m_pSys->GetTypeName(typeId);
		GameWarning( "[flow] Couldn't create node of type: %s (node was to be %s)", typeName, sName.c_str() );
		DeallocateId( id );
		return R(NULL, InvalidFlowNodeId);
	}

	for (size_t i = 0; i != m_hooks.size(); ++i)
	{
		if (!m_hooks[i]->CreatedNode( id, sName.c_str(), typeId, pNode ))
		{
			for (size_t j=0; j<i; j++)
				m_hooks[j]->CancelCreatedNode( id, sName.c_str(), typeId, pNode );

			DeallocateId( id );
			return R(NULL, InvalidFlowNodeId);
		}
	}

	m_nodeNameToId.insert( std::make_pair(sName, id) );
	m_flowData[id] = CFlowData( pNode, sName, typeId );
	m_bEdgesSorted = false; // need to regenerate link data

	return R( &m_flowData[id], id );
}

void CFlowGraphBase::RemoveNode( const char * name )
{
	std::map<string, TFlowNodeId>::iterator iter = m_nodeNameToId.find(name);
	if (iter == m_nodeNameToId.end())
	{
		GameWarning( "[flow] No node named %s", name );
		return;
	}
	RemoveNode( iter->second );
}

void CFlowGraphBase::RemoveNode( TFlowNodeId id )
{
	if (!ValidateNode(id))
	{
		GameWarning("[flow] Trying to remove non-existant flow node %d", id);
		return;
	}

	// remove any referring edges
	EnsureSortedEdges();
	std::vector<SEdge>::iterator firstRemoveIter = std::remove_if(m_edges.begin(), m_edges.end(), SEdgeHasNode(id));
	for (std::vector<SEdge>::iterator iter = firstRemoveIter; iter != m_edges.end(); ++iter)
	{
		IFlowNode::SActivationInfo info(this);
		info.myID = iter->fromNode;
		info.connectPort = iter->fromPort;
		m_flowData[iter->fromNode].GetNode()->ProcessEvent( IFlowNode::eFE_DisconnectOutputPort, &info );
		info.myID = iter->toNode;
		info.connectPort = iter->toPort;
		m_flowData[iter->toNode].GetNode()->ProcessEvent( IFlowNode::eFE_DisconnectInputPort, &info );
	}
	m_edges.erase( firstRemoveIter, m_edges.end() );

	DeallocateId( id );

	RemoveNodeFromActivationArray( id, m_firstModifiedNode, m_modifiedNodes );
	RemoveNodeFromActivationArray( id, m_firstActivatingNode, m_activatingNodes );
	RemoveNodeFromActivationArray( id, m_firstFinalActivatingNode, m_finalActivatingNodes );
	SetRegularlyUpdated( id, false );
	m_bEdgesSorted = false;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::SetNodeName( TFlowNodeId id,const char *sName )
{
	if (id < m_flowData.size())
	{
		return m_flowData[id].SetName( sName );
	}
}

//////////////////////////////////////////////////////////////////////////
const char* CFlowGraphBase::GetNodeName( TFlowNodeId id )
{
	if (id < m_flowData.size())
	{
		return m_flowData[id].GetName().c_str();
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
TFlowNodeTypeId CFlowGraphBase::GetNodeTypeId( TFlowNodeId id )
{
	if (id < m_flowData.size())
	{
		return m_flowData[id].GetTypeId();
	}
	return InvalidFlowNodeTypeId;
}

//////////////////////////////////////////////////////////////////////////
const char*  CFlowGraphBase::GetNodeTypeName( TFlowNodeId id )
{
	if (id < m_flowData.size())
	{
		TFlowNodeTypeId typeId = m_flowData[id].GetTypeId();
		return m_pSys->GetTypeInfo(typeId).name.c_str();
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
TFlowNodeId CFlowGraphBase::AllocateId()
{
	TFlowNodeId id = InvalidFlowNodeId;

	if (m_deallocatedIds.empty())
	{
		if (m_flowData.size() < InvalidFlowNodeId)
		{
			id = TFlowNodeId(m_flowData.size());
			m_flowData.resize( m_flowData.size() + 1 );
			m_activatingNodes.resize( m_flowData.size(), NOT_MODIFIED );
			m_modifiedNodes.resize( m_flowData.size(), NOT_MODIFIED );
			m_finalActivatingNodes.resize( m_flowData.size(), NOT_MODIFIED );
		}
	}
	else
	{
		id = m_deallocatedIds.back();
		m_deallocatedIds.pop_back();
	}

	return id;
}

void CFlowGraphBase::DeallocateId( TFlowNodeId id )
{
	if (!ValidateNode(id))
		return;

	m_nodeNameToId.erase( m_flowData[id].GetName() );

	m_flowData[id] = CFlowData();
	m_userData.erase( id );
	m_deallocatedIds.push_back( id );
}

void CFlowGraphBase::SetUserData(TFlowNodeId id, const XmlNodeRef& data )
{
	m_userData[id] = data;
}

XmlNodeRef CFlowGraphBase::GetUserData(TFlowNodeId id )
{
	return m_userData[id];
}

void CFlowGraphBase::Update()
{
//	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	// Disabled or Deactivated or Suspended flow graphs shouldn't be updated!
	if ( m_bEnabled == false || m_bActive == false || m_bSuspended )
		return;

	if (m_bNeedsInitialize)
		InitializeValues();

	g_profile_data.graphsUpdated ++;

	m_bInUpdate = true;

	IFlowNode::SActivationInfo activationInfo(this, 0);

	// initially, we need to send an "Update yourself" event to anyone that
	// has asked for it
	// we explicitly check if empty to save STL the hassle of deallocating
	// memory that we'll use again quite soon ;)
	if (!m_regularUpdates.empty())
	{
		m_activatingUpdates = m_regularUpdates;
		for (RegularUpdates::const_iterator iter = m_activatingUpdates.begin(); 
			iter != m_activatingUpdates.end(); ++iter)
		{
			g_profile_data.nodeUpdates ++;
			assert( ValidateNode(*iter) );
			activationInfo.myID = *iter;
			activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
			m_flowData[*iter].Update( &activationInfo );
		}
	}

	DoUpdate(IFlowNode::eFE_Activate);

	if (!m_regularUpdates.empty())
		NeedUpdate();

	m_bInUpdate = false;
}

void CFlowGraphBase::InitializeValues()
{
//	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	// flush activation list
	DoUpdate( IFlowNode::eFE_DontDoAnythingWithThisPlease );

	// Initially suspended flow graphs should never be initialized nor updated!
	if ( m_bSuspended || m_bActive==false || m_bEnabled == false)
		return;

	for (std::vector<CFlowData>::iterator iter = m_flowData.begin(); iter != m_flowData.end(); ++iter)
	{
		if (!iter->IsValid())
			continue;

		iter->FlagInputPorts();
		ActivateNodeInt( iter - m_flowData.begin() );
	}

	m_bNeedsInitialize = true;

	DoUpdate(IFlowNode::eFE_Initialize);

	m_bNeedsInitialize = false;
}

void CFlowGraphBase::DoUpdate( IFlowNode::EFlowEvent event )
{
	IFlowNode::SActivationInfo activationInfo(this, 0);

	assert( m_firstFinalActivatingNode == END_OF_MODIFIED_LIST );

	// we repeat updating until there have been too many iterations (in which
	// case we emit a warning) or until all possible activations have 
	// completed
	int numLoops = 0;
	static const int MAX_LOOPS = 256;
	while (m_firstModifiedNode != END_OF_MODIFIED_LIST && numLoops++ < MAX_LOOPS)
	{
		// swap the two sets of modified nodes -- ensures that we are reentrant
		// wrt being able to call PerformActivation in response to being 
		// activated
		m_activatingNodes.swap( m_modifiedNodes );
		assert( m_firstActivatingNode == END_OF_MODIFIED_LIST );
		m_firstActivatingNode = m_firstModifiedNode;
		m_firstModifiedNode = END_OF_MODIFIED_LIST;

		// traverse the list of modified nodes
		while (m_firstActivatingNode != END_OF_MODIFIED_LIST)
		{
			activationInfo.myID = m_firstActivatingNode;
			activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
			m_flowData[m_firstActivatingNode].Activated( &activationInfo, event );

			TFlowNodeId nextId = m_activatingNodes[m_firstActivatingNode];
			m_activatingNodes[m_firstActivatingNode] = NOT_MODIFIED;
			m_firstActivatingNode = nextId;

			g_profile_data.nodeActivations ++;
		}
	}
	if (numLoops >= MAX_LOOPS)
	{
		CryLogAlways("[flow] CFlowGraphBase::DoUpdate: %s -> Reached maxLoops %d during event %d",	m_debugName.c_str(), MAX_LOOPS, event);
		if (event == IFlowNode::eFE_Initialize)
		{
			// flush pending activations when we exceed the number of maximum loops
			// because then there are still nodes which have pending activations due to
			// the Initialize event. So they would wrongly be activated during the next update
			// round on a different event. That's why we flush
			DoUpdate( IFlowNode::eFE_DontDoAnythingWithThisPlease );
		}
	}

	while (m_firstFinalActivatingNode != END_OF_MODIFIED_LIST)
	{
		activationInfo.myID = m_firstFinalActivatingNode;
		activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
		m_flowData[m_firstFinalActivatingNode].Activated( &activationInfo, (IFlowNode::EFlowEvent)(event+1) );

		TFlowNodeId nextId = m_finalActivatingNodes[m_firstFinalActivatingNode];
		m_finalActivatingNodes[m_firstFinalActivatingNode] = NOT_MODIFIED;
		m_firstFinalActivatingNode = nextId;

		g_profile_data.nodeActivations ++;
	}

	// AlexL 02/06/06: When there are activations in a eFE_FinalActivate/eFE_FinalInitialize update
	// these activations are stored in the list, but the FlowGraph is not scheduled for a next update round
	// the normal update loop continues until all activations are handled or MAX_LOOPS is reached
	// the final activation update only runs once. this means, that activations from the final activation round
	// are delayed to the next frame!
	if (m_firstModifiedNode != END_OF_MODIFIED_LIST)
	{
		NeedUpdate();
	}
}

void CFlowGraphBase::RequestFinalActivation( TFlowNodeId id )
{
	assert( m_bInUpdate );
	if (m_finalActivatingNodes[id] == NOT_MODIFIED)
	{
		m_finalActivatingNodes[id] = m_firstFinalActivatingNode;
		m_firstFinalActivatingNode = id;
	}
}

void CFlowGraphBase::SortEdges()
{
	std::sort( m_edges.begin(), m_edges.end() );
	m_edges.erase( std::unique(m_edges.begin(), m_edges.end()), m_edges.end() );

	for (std::vector<CFlowData>::iterator iter = m_flowData.begin(); iter != m_flowData.end(); ++iter)
	{
		if (!iter->IsValid())
			continue;
		for (int i=0; i<iter->GetNumOutputs(); i++)
		{
			SEdge firstEdge(iter - m_flowData.begin(), i, 0, 0);
			std::vector<SEdge>::const_iterator iterEdge = std::lower_bound( m_edges.begin(), m_edges.end(), firstEdge );
			iter->SetOutputFirstEdge( i, iterEdge - m_edges.begin() );
		}
	}

	m_bEdgesSorted = true;
}

void CFlowGraphBase::RemoveNodeFromActivationArray( TFlowNodeId id, TFlowNodeId& front, std::vector<TFlowNodeId>& array )
{
	if (front == id)
	{
		front = array[id];
		array[id] = NOT_MODIFIED;
	}
	else if (array[id] == NOT_MODIFIED)
	{
		// nothing to do
	}
	else
	{
		// the really tricky case... the node that was removed is midway through
		// the activation list... so we'll need to remove it the hard way
		TFlowNodeId current = front;
		TFlowNodeId previous = NOT_MODIFIED;
		while (current != id)
		{
			assert( current != END_OF_MODIFIED_LIST );
			assert( current != NOT_MODIFIED );
			previous = current;
			current = array[current];
		}
		assert( previous != NOT_MODIFIED );
		assert( current == id );
		array[previous] = array[current];
		array[current] = NOT_MODIFIED;
	}

	assert( array[id] == NOT_MODIFIED );
}

void CFlowGraphBase::SetRegularlyUpdated( TFlowNodeId id, bool bUpdated )
{
	if (bUpdated)
	{
		stl::push_back_unique( m_regularUpdates, id );
		NeedUpdate();
	}
	else
		stl::find_and_erase( m_regularUpdates, id );
}

void CFlowGraphBase::ActivatePortAny( SFlowAddress addr, const TFlowInputData& value )
{
	PerformActivation( addr, value );
}

//////////////////////////////////////////////////////////////////////////
bool CFlowGraphBase::SetInputValue( TFlowNodeId node, TFlowPortId port, const TFlowInputData& value )
{
	if (!ValidateNode(node))
		return false;
	CFlowData& data = m_flowData[node];
	if (!data.ValidatePort(port, false))
		return false;
	return data.SetInputPort( port, value );
}

//////////////////////////////////////////////////////////////////////////
const TFlowInputData* CFlowGraphBase::GetInputValue( TFlowNodeId node, TFlowPortId port )
{
	if (!ValidateNode(node))
		return 0;
	CFlowData& data = m_flowData[node];
	if (!data.ValidatePort(port, false))
		return 0;
	return data.GetInputPort( port );
}

//////////////////////////////////////////////////////////////////////////
bool CFlowGraphBase::SerializeXML( const XmlNodeRef& root, bool reading )
{
	if (reading)
		return ReadXML( root );
	else
		return WriteXML( root );
}


void CFlowGraphBase::FlowLoadError( const char *format,... )
{
	if (!format)
		return;
	char buffer[MAX_WARNING_LENGTH];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, MAX_WARNING_LENGTH-1, format, args);
	buffer[MAX_WARNING_LENGTH-1] = '\0';
	va_end(args);
	IEntity* pEnt = gEnv->pEntitySystem->GetEntity(GetGraphEntity(0));

	// fg_abortOnLoadError:
	// 2 --> abort
	// 1 [!=0 && !=2] --> dialog
	// 0 --> log only
	if (CFlowSystemCVars::Get().m_abortOnLoadError == 2 && GetISystem()->IsEditor() == false)
	{
		if (m_pAIAction != 0)
			CryError("[flow] %s : %s", m_pAIAction->GetName(), buffer);
		else
			CryError("[flow] %s : %s", pEnt ? pEnt->GetName() : "<noname>", buffer);
	}
	else if (CFlowSystemCVars::Get().m_abortOnLoadError != 0 && GetISystem()->IsEditor() == false && !GetISystem()->IsUIApplication())
	{
		if (m_pAIAction != 0)
		{
			string msg("[flow] ");
			msg.append(m_pAIAction->GetName());
			msg.append(" : ");
			msg.append(buffer);
			CryMessageBox(msg.c_str(), "FlowSystem Warning", 0);
			GameWarning("[flow] %s : %s", m_pAIAction->GetName(), buffer);
		}
		else
		{
			string msg("[flow] ");
			msg.append(pEnt ? pEnt->GetName() : "<noname>");
			msg.append(" : ");
			msg.append(buffer);
			CryMessageBox(msg.c_str(), "FlowSystem Warning", 0);
			GameWarning("[flow] %s : %s", pEnt ? pEnt->GetName() : "<noname>", buffer);
		}
	}
	else
	{
		if (m_pAIAction != 0)
			GameWarning("[flow] %s : %s", m_pAIAction->GetName(), buffer);
		else
			GameWarning("[flow] %s : %s", pEnt ? pEnt->GetName() : "<noname>", buffer);
	}
}

bool CFlowGraphBase::ReadXML( const XmlNodeRef& root )
{
	Cleanup();

	if (!root)
		return false;

	bool ok = false;

	while (true)
	{
		int i;

		XmlNodeRef nodes = root->findChild( "Nodes" );
		if (!nodes)
		{
			FlowLoadError("No [Nodes] XML child");
			break;
		}

#ifdef FLOW_DEBUG_ID_MAPPING
		{
			IEntity* pEnt = gEnv->pEntitySystem->GetEntity(GetGraphEntity(0));
			if (m_pAIAction != 0)
				CryLogAlways("[flow] %s 0x%p: Loading FG", m_pAIAction->GetName(), this);
			else
				CryLogAlways("[flow] %s 0x%p: Loading FG", pEnt ? pEnt->GetName() : "<noname>", this);
		}
#endif

		int nNodes = nodes->getChildCount();
		IFlowNode::SActivationInfo actInfo(this, 0);
		for (i = 0; i < nNodes; i++)
		{
			XmlNodeRef node = nodes->getChild(i);
			const char * type = node->getAttr(NODE_TYPE_ATTR);
			const char * name = node->getAttr(NODE_NAME_ATTR);
			if (0 == strcmp(type, "_comment"))
				continue;
			TFlowNodeTypeId typeId = m_pSys->GetTypeId(type);
			std::pair<CFlowData *, TFlowNodeId> info = CreateNodeInt( typeId, name );
			if (!info.first || info.second == InvalidFlowNodeId)
			{
				FlowLoadError( "Failed to create node '%s' of type '%s' - FLOWGRAPH DISCARDED", name, type );
				break;
			}
			actInfo.myID = info.second;

#ifdef FLOW_DEBUG_ID_MAPPING
			{
				IEntity* pEnt = gEnv->pEntitySystem->GetEntity(GetGraphEntity(0));
				if (m_pAIAction != 0)
					CryLogAlways("[flow] %s : Mapping ID=%s to internal ID=%d", m_pAIAction->GetName(), name, actInfo.myID);
				else
					CryLogAlways("[flow] %s : Mapping ID=%s to internal ID=%d", pEnt ? pEnt->GetName() : "<noname>", name, actInfo.myID);
			}
#endif

			if (!info.first->SerializeXML( &actInfo, node, true ))
			{
				FlowLoadError( "Failed to load node %s of type %s - FLOWGRAPH DISCARDED", name, type );
				DeallocateId( info.second );
				break;
			}
		}
		if (i != nNodes) // didn't load all of the nodes
		{
			FlowLoadError("Did not load all nodes (%d/%d nodes) - FLOWGRAPH DISCARDED", i, nNodes);
			break;
		}

		XmlNodeRef edges = root->findChild( "Edges" );
		if (!edges)
		{
			FlowLoadError("No [Edges] XML child");
			break;
		}
		int nEdges = edges->getChildCount();
		for (i = 0; i < nEdges; i++)
		{
			XmlNodeRef edge = edges->getChild(i);
			if (strcmp(edge->getTag(), "Edge"))
				break;
			SFlowAddress from = ResolveAddress( edge->getAttr("nodeOut"), edge->getAttr("portOut"), true );
			SFlowAddress to = ResolveAddress( edge->getAttr("nodeIn"), edge->getAttr("portIn"), false );
			bool edgeEnabled;
			if (edge->getAttr("enabled", edgeEnabled) == false)
				edgeEnabled = true;

			if (edgeEnabled)
			{
				if (!LinkNodes( from, to )) {
					FlowLoadError("Can't link edge <%s,%s> to <%s,%s> - FLOWGRAPH DISCARDED",
							edge->getAttr("nodeOut"), edge->getAttr("portOut"),
							edge->getAttr("nodeIn"), edge->getAttr("portIn"));
					break;
				}
			}
			else
			{
#if defined (FLOW_DEBUG_DISABLED_EDGES)
				IEntity* pEnt = gEnv->pEntitySystem->GetEntity(GetGraphEntity(0));
				if (m_pAIAction != 0) {
					CryLogAlways("[flow] Disabled edge %d for AI action graph '%s' <%s,%s> to <%s,%s>", i, m_pAIAction->GetName(),
						edge->getAttr("nodeOut"), edge->getAttr("portOut"),
						edge->getAttr("nodeIn"), edge->getAttr("portIn"));
				}
				else
				{
					GameWarning("[flow] Disabled edge %d for entity %d '%s' <%s,%s> to <%s,%s>", i, GetGraphEntity(0),
						pEnt ? pEnt->GetName() : "<NULL>",
						edge->getAttr("nodeOut"), edge->getAttr("portOut"),
						edge->getAttr("nodeIn"), edge->getAttr("portIn"));
				}
#endif
			}
		}
		if (i != nEdges) // didn't load all edges
		{
			FlowLoadError("Did not load all edges (%d/%d edges) - FLOWGRAPH DISCARDED", i, nEdges);
			break;
		}
		ok = true;
		break;
	}

	if (root->getAttr("enabled", m_bEnabled) == false)
		m_bEnabled = true;

	if (!ok)
		Cleanup();
	else
		NeedInitialize();

	return ok;
}

bool CFlowGraphBase::WriteXML( const XmlNodeRef& root )
{
	XmlNodeRef nodes = root->createNode( "Nodes" );
	for (std::vector<CFlowData>::iterator iter = m_flowData.begin(); iter != m_flowData.end(); ++iter)
	{
		XmlNodeRef node = nodes->createNode( m_pSys->GetTypeName(iter->GetNodeTypeId()) );
		nodes->addChild( node );

		node->setAttr( NODE_NAME_ATTR, iter->GetName().c_str() );
		IFlowNode::SActivationInfo info(this, iter - m_flowData.begin());
		if (!iter->SerializeXML( &info, node, false ))
			return false;
	}

	XmlNodeRef edges = root->createNode( "Edges" );
	for (std::vector<SEdge>::const_iterator iter = m_edges.begin(); iter != m_edges.end(); ++iter)
	{
		string from = PrettyAddress( SFlowAddress(iter->fromNode, iter->fromPort, true) );
		string to = PrettyAddress( SFlowAddress(iter->toNode, iter->toPort, false) );
	
		XmlNodeRef edge = edges->createNode( "Edge" );
		edges->addChild( edge );
		edge->setAttr( "from", from.c_str() );
		edge->setAttr( "to", to.c_str() );
	}

	root->addChild( nodes );
	root->addChild( edges );
	root->setAttr("enabled", m_bEnabled);
	return true;
}

IFlowNodePtr CFlowGraphBase::CreateNodeOfType( IFlowNode::SActivationInfo * pActInfo, TFlowNodeTypeId typeId )
{
	IFlowNodePtr pPtr;
	for (size_t i=0; i<m_hooks.size() && !pPtr; ++i)
		pPtr = m_hooks[i]->CreateNode( pActInfo, typeId );
	if (!pPtr)
		pPtr = m_pSys->CreateNodeOfType( pActInfo, typeId );
	return pPtr;
}

IFlowNodeIteratorPtr CFlowGraphBase::CreateNodeIterator()
{
	return new CNodeIterator(this);
}

IFlowEdgeIteratorPtr CFlowGraphBase::CreateEdgeIterator()
{
	return new CEdgeIterator(this);
}

bool CFlowGraphBase::IsOutputConnected( SFlowAddress addr )
{
	assert( ValidateAddress( addr ) );

	bool connected = false;
	if (addr.isOutput)
	{
		EnsureSortedEdges();

		/*
		SEdge firstEdge(addr.node, addr.port, 0, 0);
		std::vector<SEdge>::const_iterator iter = std::lower_bound( m_edges.begin(), m_edges.end(), firstEdge );
		*/
		std::vector<SEdge>::const_iterator iter = m_edges.begin() + m_flowData[addr.node].GetOutputFirstEdge(addr.port);
		connected = iter != m_edges.end() && iter->fromNode == addr.node && iter->fromPort == addr.port;
	}

	return connected;
}

void CFlowGraphBase::Serialize( TSerialize ser )
{
	// When reading, we clear the regular updates before nodes are serialized
	// because their serialization could request to be scheduled as regularUpdate
	if (ser.IsReading())
	{
		// clear regulars
		m_regularUpdates.resize(0);
	}

	ser.Value("needsInitialize", m_bNeedsInitialize);
	ser.Value("enabled", m_bEnabled);
	ser.Value("suspended", m_bSuspended);
	ser.Value("active", m_bActive);

	std::vector<string> activatedNodes;
	if (ser.IsWriting())
	{
		// get activations
		for (TFlowNodeId id = m_firstModifiedNode; id != END_OF_MODIFIED_LIST; id = m_modifiedNodes[id])
			activatedNodes.push_back( m_flowData[id].GetName() );
	}
	ser.Value( "activatedNodes", activatedNodes );
	if (ser.IsReading())
	{
		// flush modified nodes
		m_firstModifiedNode = END_OF_MODIFIED_LIST;
		m_modifiedNodes.resize(0);
		m_modifiedNodes.resize(m_flowData.size(), NOT_MODIFIED);
		// reactivate
		for (std::vector<string>::const_iterator iter = activatedNodes.begin(); iter != activatedNodes.end(); ++iter)
		{
			TFlowNodeId id = stl::find_in_map( m_nodeNameToId, *iter, InvalidFlowNodeId );
			if (id != InvalidFlowNodeId)
				ActivateNodeInt( id );
			else
				GameWarning( "[flow] Flow graph has changed between save-games" );
		}
	}

	IFlowNode::SActivationInfo activationInfo(this, 0);
	if (ser.IsWriting())
	{
		size_t nodeCount = 0;
		for (std::vector<CFlowData>::iterator iter = m_flowData.begin(); iter != m_flowData.end(); ++iter)
			nodeCount += iter->IsValid();
		ser.Value( "nodeCount", nodeCount );
		for (std::vector<CFlowData>::iterator iter = m_flowData.begin(); iter != m_flowData.end(); ++iter)
		{
			if (iter->IsValid())
			{
				activationInfo.myID = iter - m_flowData.begin();
				ser.BeginGroup("Node");
				ser.Value( "id", iter->GetName() );
				iter->Serialize( &activationInfo, ser );
				ser.EndGroup();
			}
		}
	}
	else
	{
		size_t nodeCount;
		ser.Value( "nodeCount", nodeCount );
		if (nodeCount != m_flowData.size())
		{
			// there are nodes in the MAP which are not in the SaveGame
			// or there are nodes in the SaveGame which are not in the map
			// can happen if level.pak got changed after savegame creation!
			// this comment is just to remind, that this case exists.
			// GameSerialize.cpp will send an eFE_Initialize after level load, but before serialize,
			// so these nodes get at least initialized
			// AlexL: 18/04/2007
		}

		while (nodeCount--)
		{
			ser.BeginGroup("Node");
			string name;
			ser.Value( "id", name );
			TFlowNodeId id = stl::find_in_map( m_nodeNameToId, name, InvalidFlowNodeId );
			/*
			if (id == InvalidFlowNodeId) {
				std::map<string, TFlowNodeId>::const_iterator iter = m_nodeNameToId.begin();
				CryLogAlways("Map Contents:");
				while (iter != m_nodeNameToId.end())
				{
					CryLogAlways("Map: %s", iter->first.c_str());
					++iter;
				}
			}
			*/
			if (id != InvalidFlowNodeId)
			{
				activationInfo.myID = id;
				activationInfo.pEntity = GetIEntityForNode(activationInfo.myID);
				m_flowData[id].Serialize( &activationInfo, ser );
			}
			else
				GameWarning( "[flow] Flowgraph '%s' has changed between save-games. Can't resolve node named '%s'", InternalGetDebugName(), name.c_str() );
			ser.EndGroup();
		}
	}

	// regular updates
	std::vector<string> regularUpdates;
	if (ser.IsWriting())
	{
		RegularUpdates::const_iterator iter (m_regularUpdates.begin());
		while (iter != m_regularUpdates.end()) {
			regularUpdates.push_back(m_flowData[*iter].GetName());
			++iter;
		}
	}

	ser.Value( "regularUpdates", regularUpdates );
	if (ser.IsReading())
	{
		// reserve some space in the m_regularUpdates vector
		// as there might have already been added some this is somewhat conservative
		// regularUpdates.size()+m_regularUpdates.size() would be maximum, but most of the time
		// too much
		m_regularUpdates.reserve(regularUpdates.size());

		// re-fill regular updates
		for (std::vector<string>::const_iterator iter = regularUpdates.begin(); iter != regularUpdates.end(); ++iter)
		{
			TFlowNodeId id = stl::find_in_map( m_nodeNameToId, *iter, InvalidFlowNodeId );
			if (id != InvalidFlowNodeId)
				SetRegularlyUpdated(id, true);
			else
				GameWarning( "[flow] Flow graph has changed between save-games" );
		}
	}

	// when we load a flowgraph and it didn't have activations so far
	// it is still in the initialized state
	// so we need to mark this graph to be updated, so it gets initialized
	if (ser.IsReading() && m_bNeedsInitialize)
	{
		NeedUpdate();
	}
}

//////////////////////////////////////////////////////////////////////////
IEntity* CFlowGraphBase::GetIEntityForNode( TFlowNodeId flowNodeId )
{
	assert( ValidateNode(flowNodeId) );
	EntityId id = m_flowData[flowNodeId].GetEntityId();
	if (id == 0)
		return 0;

	if (id == EFLOWNODE_ENTITY_ID_GRAPH1)
		id = m_graphEntityId[0];
	else if (id == EFLOWNODE_ENTITY_ID_GRAPH2)
		id = m_graphEntityId[1];

	IEntity *pEntity = m_pEntitySystem->GetEntity(id);
	return pEntity;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::NotifyFlowSystemDestroyed()
{
	m_pSys = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::RegisterInspector(IFlowGraphInspectorPtr pInspector)
{
	stl::push_back_unique(m_inspectors, pInspector);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::UnregisterInspector(IFlowGraphInspectorPtr pInspector)
{
	stl::find_and_erase(m_inspectors, pInspector);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::GetGraphStats(int &nodeCount, int &edgeCount)
{
	nodeCount = m_flowData.size();
	edgeCount = m_edges.size();
	size_t cool = m_nodeNameToId.size();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphBase::GetMemoryStatistics(ICrySizer * s)
{
	{
		SIZER_SUBCOMPONENT_NAME(s, "FlowGraphLocal");
		s->AddContainer(m_modifiedNodes);
		s->AddContainer(m_activatingNodes);
		s->AddContainer(m_finalActivatingNodes);
		s->AddContainer(m_deallocatedIds);
		s->AddContainer(m_edges);
		s->AddContainer(m_regularUpdates);
//		s->AddContainer(m_activatingNodes);
		s->AddContainer(m_nodeNameToId);
		s->AddContainer(m_hooks);
		s->AddContainer(m_userData);
		s->AddContainer(m_inspectors);
	}

	{
		SIZER_SUBCOMPONENT_NAME(s, "FlowData-FlowData-Struct");
		s->AddContainer(m_flowData);
	}

	{
		SIZER_SUBCOMPONENT_NAME(s, "FlowData-VariantData");
		int nSize = 0;
		for (size_t i=0; i<m_flowData.size(); i++)
			nSize += 	m_flowData[i].GetMemoryStatistics();
		s->AddObject(&m_flowData,nSize);
	}

	{
		SIZER_SUBCOMPONENT_NAME(s, "NodeNameToIdMap");
		int nSize = m_nodeNameToId.size() * sizeof(std::map<string, TFlowNodeId>::value_type);
		for (std::map<string, TFlowNodeId>::iterator iter = m_nodeNameToId.begin(); iter != m_nodeNameToId.end(); ++iter)
			nSize += iter->first.size();
		s->AddObject( &m_nodeNameToId,nSize );
	}
}

void CFlowGraph::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	CFlowGraphBase::GetMemoryStatistics(s);
}

#if defined(LINUX)
const TFlowNodeId CFlowGraphBase::NOT_MODIFIED = ~TFlowNodeId(0);
const TFlowNodeId CFlowGraphBase::END_OF_MODIFIED_LIST = NOT_MODIFIED-1;
#endif
