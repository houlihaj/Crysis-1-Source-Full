////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowGraph.h
//  Version:     v1.00
//  Created:     8/4/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
//  Modified:    22/2/2008 Visual FG Debugging by Jan Müller
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include <IAIAction.h>

#include "GameEngine.h"
#include "FlowGraph.h"
#include "FlowGraphNode.h"
#include "FlowGraphManager.h"
#include "FlowGraphMigrationHelper.h"
#include "CommentNode.h"
#include "Objects\Entity.h"
#include "ErrorReport.h"

#define FG_INTENSIVE_DEBUG
#undef  FG_INTENSIVE_DEBUG

//////////////////////////////////////////////////////////////////////////
CFlowGraph::CFlowGraph( CHyperGraphManager *pManager,const char *sGroupName )
: CHyperGraph( pManager )
{
#ifdef FG_INTENSIVE_DEBUG
	CryLogAlways("CFlowGraph: Creating 0x%p", this);
#endif
	m_bEditable = true;
	m_pEntity = 0;
	m_pAIAction = 0;
	m_pGameFlowGraph = 0;
	m_mpType = eMPT_ServerOnly;

	SetGroupName(sGroupName);
	IFlowSystem *pFlowSystem = GetIEditor()->GetGameEngine()->GetIFlowSystem();
	if (pFlowSystem)
	{
		m_pGameFlowGraph = pFlowSystem->CreateFlowGraph();
		m_pGameFlowGraph->RegisterFlowNodeActivationListener(this);
	}
	((CFlowGraphManager*)pManager)->RegisterGraph(this);
}

//////////////////////////////////////////////////////////////////////////
CFlowGraph::~CFlowGraph()
{
#ifdef FG_INTENSIVE_DEBUG
	CryLogAlways("CFlowGraph: About to delete 1 0x%p", this);
#endif

	if(m_pGameFlowGraph)
		m_pGameFlowGraph->RemoveFlowNodeActivationListener(this);

	// first check is the manager still there!
	// could be deleted in which case we don't
	// care about unregistering.
	CFlowGraphManager* pManager = (CFlowGraphManager*) GetManager();
	if ( pManager )
		pManager->UnregisterGraph(this);

#ifdef FG_INTENSIVE_DEBUG
	CryLogAlways("CFlowGraph: About to delete 2 0x%p", this);
#endif
}

//////////////////////////////////////////////////////////////////////////
CFlowGraph::CFlowGraph( CHyperGraphManager *pManager,IFlowGraph *pIFlowGraph )
: CHyperGraph( pManager )
{
	m_bEditable = true;
	m_pEntity = 0;
	m_pAIAction = 0;
	m_pGameFlowGraph = pIFlowGraph;
	if(m_pGameFlowGraph)
		m_pGameFlowGraph->RegisterFlowNodeActivationListener(this);
}

//////////////////////////////////////////////////////////////////////////
IFlowGraph* CFlowGraph::GetIFlowGraph()
{
	if (!m_pGameFlowGraph)
	{
		IFlowSystem *pFlowSystem = GetIEditor()->GetGameEngine()->GetIFlowSystem();
		if (pFlowSystem)
		{
			m_pGameFlowGraph = pFlowSystem->CreateFlowGraph();
			m_pGameFlowGraph->RegisterFlowNodeActivationListener(this);
		}
	}

	return m_pGameFlowGraph;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::OnFlowNodeActivation(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const char *value)
{
	NodesMap::iterator it = m_nodesMap.begin();
	NodesMap::iterator end = m_nodesMap.end();
	for(; it != end; ++it)
	{
		CFlowNode *pNode = static_cast<CFlowNode*>(it->second.get());
		TFlowNodeId id = pNode->GetFlowNodeId();
		if(id == toNode)
		{
			pNode->DebugPortActivation(toPort, value);
			return;
		}
		//else if(id == srcNode)
		//	pNode->DebugPortActivation(srcPort, "out");
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::ClearAll()
{
	GetIFlowGraph()->Clear();
	__super::ClearAll();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::RemoveEdge( CHyperEdge *pEdge )
{
	// Remove link in flow system.
	CFlowEdge *pFlowEdge = (CFlowEdge*)pEdge;
	GetIFlowGraph()->UnlinkNodes( pFlowEdge->addr_out,pFlowEdge->addr_in );

	CHyperGraph::RemoveEdge(pEdge);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::EnableEdge(CHyperEdge *pEdge, bool bEnable )
{
	RecordUndo();
	SetModified();

	CFlowEdge *pFlowEdge = (CFlowEdge*)pEdge;
	pEdge->enabled = bEnable;
	if (!bEnable)
	{
		GetIFlowGraph()->UnlinkNodes( pFlowEdge->addr_out,pFlowEdge->addr_in );
	}
	else
	{
		GetIFlowGraph()->LinkNodes( pFlowEdge->addr_out,pFlowEdge->addr_in );
	}
}

//////////////////////////////////////////////////////////////////////////
CHyperEdge* CFlowGraph::MakeEdge( CHyperNode *pNodeOut,CHyperNodePort *pPortOut,CHyperNode *pNodeIn,CHyperNodePort *pPortIn, bool bEnabled, bool fromSpecialDrag )
{
	assert(pNodeIn);
	assert(pNodeOut);
	assert(pPortIn);
	assert(pPortOut);
	CFlowEdge *pEdge = new CFlowEdge;

	pEdge->nodeIn = pNodeIn->GetId();
	pEdge->nodeOut = pNodeOut->GetId();
	pEdge->portIn = pPortIn->GetName();
	pEdge->portOut = pPortOut->GetName();
	pEdge->enabled = bEnabled;

	pEdge->nPortIn = pPortIn->nPortIndex;
	pEdge->nPortOut = pPortOut->nPortIndex;

	++pPortOut->nConnected;
	++pPortIn->nConnected;

	RegisterEdge( pEdge, fromSpecialDrag );

	SetModified();

	// Create link in flow system.
	pEdge->addr_in = SFlowAddress( ((CFlowNode*)pNodeIn)->GetFlowNodeId(),pPortIn->nPortIndex,false );
	pEdge->addr_out = SFlowAddress( ((CFlowNode*)pNodeOut)->GetFlowNodeId(),pPortOut->nPortIndex,true );
	if (bEnabled) {
		m_pGameFlowGraph->LinkNodes( pEdge->addr_out,pEdge->addr_in );
		if (!m_bLoadingNow)
			m_pGameFlowGraph->InitializeValues();
	}
	return pEdge;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::SetEntity( CEntity *pEntity, bool bAdjustGraphNodes )
{
	assert( pEntity );
	m_pEntity = pEntity;
	
	if (m_pGameFlowGraph!=NULL && m_pEntity!=NULL && m_pEntity->GetIEntity())
	{
		m_pGameFlowGraph->SetGraphEntity( m_pEntity->GetEntityId(),0 );
	}
	if (bAdjustGraphNodes) {
		IHyperGraphEnumerator *pEnum = GetNodesEnumerator();
		for (IHyperNode *pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			CFlowNode *pNode = (CFlowNode*)pINode;
			if (pNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY))
				pNode->SetEntity(pEntity);
		}
		pEnum->Release();
	}
	((CFlowGraphManager*)GetManager())->SendNotifyEvent(EHG_GRAPH_OWNER_CHANGE); // we're friends
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::SetAIAction( IAIAction *pAIAction )
{
	m_pAIAction = pAIAction;
	if (0 != m_pGameFlowGraph)
	{
		m_pGameFlowGraph->SetAIAction(pAIAction); // KLUDE to make game flowgraph aware that this is an AI action
		m_pGameFlowGraph->SetAIAction(0);         // KLUDE cont'd: for safety we re-set afterwards (game flowgraph remembers)
	}
	((CFlowGraphManager*)GetManager())->SendNotifyEvent(EHG_GRAPH_OWNER_CHANGE); // we're friends
}

//////////////////////////////////////////////////////////////////////////
IHyperGraph* CFlowGraph::Clone()
{
	CFlowGraph *pGraph = new CFlowGraph( GetManager() );

	if (!m_bEditable)
	{
		pGraph->m_pGameFlowGraph = m_pGameFlowGraph->Clone();
		pGraph->m_pGameFlowGraph->RegisterFlowNodeActivationListener(pGraph);
		pGraph->SetModified();
		return pGraph;
	}

	pGraph->m_bLoadingNow = true;

	IHyperGraphEnumerator *pEnum = GetNodesEnumerator();
	for (IHyperNode *pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
	{
		CHyperNode *pSrcNode = (CHyperNode*)pINode;

		// create a clone of the node
		CHyperNode *pNewNode = pSrcNode->Clone();

		// comment is the only node which is no CFlowNode argh...
		if (strcmp(pNewNode->GetClassName(), CCommentNode::GetClassType()) == 0)
		{
			pNewNode->SetGraph(pGraph);
			pNewNode->Init();
		}
		else
		{
			// clone the node, this creates only our Editor shallow node
			// also assigns the m_pEntity (which we might have to correct!)
			CFlowNode *pNode = static_cast<CFlowNode*> (pNewNode);

			// set the graph of the newly created node
			pNode->SetGraph(pGraph);

			// create a real flowgraph node (note: inputs are not yet set!)
			pNode->Init();

			// set the inputs of the base FG node (shallow already has correct values
			// because pVars are copied by pSrcNode->Clone()
			pNode->SetInputs(false, true);  // Make sure entities in normal ports are set!

			// WE CAN'T DO IT HERE, BECAUSE THE pNode->GetDefaultEntity returns always 0
			// THE ENTITY IS ADJUSTED IN THE SetEntity call to the CFlowGraph
			// finally we have to set the correct entity for this node
			// e.g. GraphEntity has to be updated set to new graph entity 
			if (pSrcNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY)) {
				pNode->SetEntity(0);
				pNode->SetFlag(EHYPER_NODE_GRAPH_ENTITY, true);
			}
			else if (pSrcNode->CheckFlag(EHYPER_NODE_ENTITY)) {
				CFlowNode *pSrcFlowNode = static_cast<CFlowNode*> (pSrcNode);
				pNode->SetEntity(pSrcFlowNode->GetEntity());
			}
		} 

		// and add the node
		pGraph->AddNode( pNewNode );
	}
	pEnum->Release();

	std::vector<CHyperEdge*> edges;
	GetAllEdges( edges );
	for (int i = 0; i < edges.size(); i++)
	{
		CHyperEdge &edge = *(edges[i]);
		
		CHyperNode *pNodeIn = (CHyperNode*)pGraph->FindNode(edge.nodeIn);
		CHyperNode *pNodeOut = (CHyperNode*)pGraph->FindNode(edge.nodeOut);
		if (!pNodeIn || !pNodeOut)
			continue;

		CHyperNodePort *pPortIn = pNodeIn->FindPort(edge.portIn,true);
		CHyperNodePort *pPortOut = pNodeOut->FindPort(edge.portOut,false);
		if (!pPortIn || !pPortOut)
			continue;

		pGraph->MakeEdge( pNodeOut,pPortOut,pNodeIn,pPortIn, edge.enabled, false );
	}

	pGraph->m_bLoadingNow = false;

	OnPostLoad();

	pGraph->SetEnabled(IsEnabled());
	pGraph->SetModified();
	
	return pGraph;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::Serialize( XmlNodeRef &node,bool bLoading, CObjectArchive* ar )
{
	if (!m_bEditable)
	{
		m_pGameFlowGraph->SerializeXML( node,bLoading );
	}
	else
	{
		// when loading an entity flowgraph assign a name in advance, so
		// error messages are bit more descriptive
		if (bLoading && m_pEntity)
			SetName(m_pEntity->GetName());

		__super::Serialize( node, bLoading, ar );
		if (bLoading)
		{
			bool enabled;
			if (node->getAttr("enabled", enabled) == false)
				enabled = true;
			SetEnabled(enabled);

			EMultiPlayerType mpType = eMPT_ServerOnly;
			const char* mpTypeAttr = node->getAttr("MultiPlayer");
			if (mpTypeAttr)
			{
				if (strcmp("ClientOnly", mpTypeAttr) == 0)
					mpType = eMPT_ClientOnly;
				else if (strcmp("ClientServer", mpTypeAttr) == 0)
					mpType = eMPT_ClientServer;
			}
			SetMPType(mpType);
		} 
		else 
		{
			node->setAttr("enabled", IsEnabled());
			EMultiPlayerType mpType = GetMPType();
			if (mpType == eMPT_ClientOnly)
				node->setAttr("MultiPlayer", "ClientOnly");
			else if (mpType == eMPT_ClientServer)
				node->setAttr("MultiPlayer", "ClientServer");
			else
				node->setAttr("MultiPlayer", "ServerOnly");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFlowGraph::Migrate(XmlNodeRef &node )
{
	CFlowGraphMigrationHelper& migHelper = GetIEditor()->GetFlowGraphManager()->GetMigrationHelper();

	bool bChanged = migHelper.Substitute(node);

	const std::vector<CFlowGraphMigrationHelper::ReportEntry>& report = migHelper.GetReport();
	if (report.size() > 0)
	{
		for (int i=0;i<report.size(); ++i)
		{
			const CFlowGraphMigrationHelper::ReportEntry& entry = report[i];
			CErrorRecord err;
			err.module = VALIDATOR_MODULE_FLOWGRAPH;
			err.severity = CErrorRecord::ESEVERITY_WARNING;
			err.error = entry.description;
			err.pItem = 0;
			GetIEditor()->GetErrorReport()->ReportError( err );
			CryLogAlways("CFlowGraph::Migrate: FG '%s': %s", GetName(), entry.description.GetString());
		}
	}
	return bChanged;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::OnPostLoad()
{
	if (m_pGameFlowGraph)
	{
		m_pGameFlowGraph->InitializeValues();
	}
}

void CFlowGraph::PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx )
{
	// all flownodes need to have a post clone
	__super::PostClone(pFromObject, ctx);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::SetGroupName(const CString &sName )
{
	// we have to override so everybody knows that we changed the group. maybe do this in CHyperGraph instead
	__super::SetGroupName(sName);
	((CFlowGraphManager*)GetManager())->SendNotifyEvent(EHG_GRAPH_OWNER_CHANGE); // we're friends
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::BeginEditing()
{
	if (!m_bEditable)
	{
		m_bEditable = true;
		InitializeFromGame();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::EndEditing()
{
	m_bEditable = false;
	CHyperGraph::ClearAll();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::SetEnabled( bool bEnable )
{
	__super::SetEnabled(bEnable);
	// Enable/Disable game flow graph.
	if(m_pGameFlowGraph)
		m_pGameFlowGraph->SetEnabled(bEnable);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraph::InitializeFromGame()
{
}

//////////////////////////////////////////////////////////////////////////
// Undo object for CFlowGraph.
// Remarks [AlexL]: The CUndoFlowGraph serializes the graph to XML to store
//                  the current state. If it is an entity FlowGraph we use
//                  the entity's GUID to lookup the FG for the entity, because
//                  when an entity is deleted and then re-created from undo,
//                  the FG is also recreated and hence the FG pointer in the Undo
//                  object is garbage! AIAction's may not suffer from this, but
//                  maybe we should reference these by names then or something else...
//                  Currently for simple node changes [Select, PosChange] we create a
//                  full graph XML as well. The Undo hypernode suffers from the same
//                  problem as Graphs, because nodes are deleted and recreated, so pointer
//                  storing is BAD. Maybe use the nodeID [are these surely be the same on recreation?]
//////////////////////////////////////////////////////////////////////////
class CUndoFlowGraph : public IUndoObject
{
public:
	CUndoFlowGraph( CHyperGraph *pGraph )
	{
		// Stores the current state of given graph.
		assert( pGraph != 0 );

		m_pGraph = pGraph;
		m_entityGUID = GuidUtil::NullGuid;

		// if it's an AIAction, we store the pointer to the graph
		if (pGraph->IsFlowGraph())
		{
			CFlowGraph* pFG = static_cast<CFlowGraph*> (pGraph);
			CEntity* pEntity = pFG->GetEntity();
			if (pEntity)
			{
				m_entityGUID = pEntity->GetId();
				m_pGraph = 0;
			}
		}

		m_redo = 0;
		m_undo = CreateXmlNode("HyperGraph");
		pGraph->Serialize( m_undo,false );
#ifdef FG_INTENSIVE_DEBUG
		CryLogAlways("CUndoFlowGraph 0x%p:: Serializing from graph 0x%p", this, pGraph);
#endif
	}

protected:
	CHyperGraph* GetGraph()
	{
		if (GuidUtil::IsEmpty(m_entityGUID))
		{
			assert (m_pGraph != 0);
			return m_pGraph;
		}
		CBaseObject *pObj = GetIEditor()->GetObjectManager()->FindObject(m_entityGUID);
		assert (pObj != 0);
		if (pObj && pObj->IsKindOf(RUNTIME_CLASS(CEntity)))
		{
			CEntity* pEntity = static_cast<CEntity*> (pObj);
			CFlowGraph* pFG = pEntity->GetFlowGraph();
			if (pFG == 0)
			{
				pFG = GetIEditor()->GetFlowGraphManager()->CreateGraphForEntity( pEntity );
				pEntity->SetFlowGraph( pFG );
			}
			assert (pFG != 0);
			return pFG;
		}
		return 0;
	}

	virtual int GetSize() { return sizeof(*this); }
	virtual const char* GetDescription() { return "FlowGraph Undo"; };

	virtual void Undo( bool bUndo )
	{
		CHyperGraph* pGraph = GetGraph();
		assert (pGraph != 0);

		if (bUndo)
		{
#ifdef FG_INTENSIVE_DEBUG
			CryLogAlways("CUndoFlowGraph 0x%p::Undo 1: Serializing to graph 0x%p", this, pGraph);
#endif

			m_redo = CreateXmlNode("HyperGraph");
			if (pGraph)
				pGraph->Serialize( m_redo,false );
		}

#ifdef FG_INTENSIVE_DEBUG
		CryLogAlways("CUndoFlowGraph 0x%p::Undo 2: Serializing to graph 0x%p", this, pGraph);
#endif

		if (pGraph) 
			pGraph->Serialize( m_undo,true );
	}
	virtual void Redo()
	{
		if (m_redo)
		{
			CHyperGraph* pGraph = GetGraph();
			assert (pGraph != 0);
			if (pGraph)
			{
#ifdef FG_INTENSIVE_DEBUG
				CryLogAlways("CUndoFlowGraph 0x%p::Redo: Serializing to graph 0x%p", this, pGraph);
#endif
				pGraph->Serialize( m_redo,true );
			}
			else
			{
				CryLogAlways("CUndoFlowGraph 0x%p: Got 0 graph", this);
			}
		}
	}
private:
	GUID m_entityGUID;
	_smart_ptr<CHyperGraph> m_pGraph;
	XmlNodeRef m_undo;
	XmlNodeRef m_redo;
};

//////////////////////////////////////////////////////////////////////////
IUndoObject* CFlowGraph::CreateUndo()
{
	// create undo object
	return new CUndoFlowGraph(this);
}

//////////////////////////////////////////////////////////////////////////
bool CFlowGraph::CanConnectPorts(CHyperNode *pSrcNode, CHyperNodePort *pSrcPort, CHyperNode *pTrgNode, CHyperNodePort *pTrgPort)
{
	bool bCanConnect = __super::CanConnectPorts(pSrcNode, pSrcPort, pTrgNode, pTrgPort);
	if (bCanConnect && m_pGameFlowGraph)
	{
		CFlowNode* pSrcFlowNode = static_cast<CFlowNode*> (pSrcNode);
		TFlowNodeId srcId = pSrcFlowNode->GetFlowNodeId();
		SFlowNodeConfig srcConfig;
		m_pGameFlowGraph->GetNodeConfiguration(srcId, srcConfig);

		CFlowNode* pTrgFlowNode = static_cast<CFlowNode*> (pTrgNode);
		TFlowNodeId trgId = pTrgFlowNode->GetFlowNodeId();
		SFlowNodeConfig trgConfig;
		m_pGameFlowGraph->GetNodeConfiguration(trgId, trgConfig);

		if (srcConfig.pOutputPorts && trgConfig.pInputPorts)
		{
			const SOutputPortConfig *pSrcPortConfig = srcConfig.pOutputPorts + pSrcPort->nPortIndex;
			const SInputPortConfig  *pTrgPortConfig = trgConfig.pInputPorts + pTrgPort->nPortIndex;
			int srcType = pSrcPortConfig->type;
			int trgType = pTrgPortConfig->defaultData.GetType();
			if (srcType == eFDT_EntityId)
			{
				bCanConnect = true;
			}
			else if (trgType == eFDT_EntityId)
			{
				bCanConnect = (srcType == eFDT_EntityId || srcType == eFDT_Int || srcType == eFDT_Any);
			}
			if (!bCanConnect)
			{
				Warning("An Entity port can only be connected from an Entity or an Int port!");
			}
		}
	}
	return bCanConnect;
}
