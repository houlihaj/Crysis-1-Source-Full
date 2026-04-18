#ifndef __FLOWGRAPH_H__
#define __FLOWGRAPH_H__

#pragma once

#include "FlowSystem.h"
#include "FlowData.h"

// class CFlowSystem;

#define FLOW_DEBUG_PENDING_UPDATES
//#undef  FLOW_DEBUG_PENDING_UPDATES

#define FLOWGRAPH_NOT_IN_LIST ((CFlowGraphBase *)(~INT_PTR(0)))

#define MAX_GRAPH_ENTITIES 2

//////////////////////////////////////////////////////////////////////////
class CFlowGraphBase : public IFlowGraph
{
public:
	CFlowGraphBase( CFlowSystem* pSys );
	virtual ~CFlowGraphBase();

	// IFlowGraph
	virtual void AddRef();
	virtual void Release();
	virtual IFlowGraphPtr Clone();
	virtual void Clear();
	virtual void RegisterHook( IFlowGraphHookPtr );
	virtual void UnregisterHook( IFlowGraphHookPtr );
	virtual IFlowNodeIteratorPtr CreateNodeIterator();
	virtual IFlowEdgeIteratorPtr CreateEdgeIterator();
	virtual void SetUserData(TFlowNodeId id, const XmlNodeRef& data );
	virtual XmlNodeRef GetUserData(TFlowNodeId id );
	virtual void SetGraphEntity( EntityId id,int nIndex=0 );
	virtual EntityId GetGraphEntity( int nIndex );
	virtual SFlowAddress ResolveAddress( const char * addr, bool isOutput );
	virtual TFlowNodeId ResolveNode( const char * name );
	virtual void GetNodeConfiguration( TFlowNodeId id, SFlowNodeConfig& );
	virtual bool LinkNodes( SFlowAddress from, SFlowAddress to );
	virtual void UnlinkNodes( SFlowAddress from, SFlowAddress to );
	virtual TFlowNodeId CreateNode( TFlowNodeTypeId typeId, const char *name, void *pUserData=0 );
	virtual TFlowNodeId CreateNode( const char* typeName, const char *name, void *pUserData=0 );
	virtual void SetNodeName( TFlowNodeId id,const char *sName );
	virtual IFlowNodeData* GetNodeData( TFlowNodeId id );
	virtual const char* GetNodeName( TFlowNodeId id );
	virtual TFlowNodeTypeId GetNodeTypeId( TFlowNodeId id );
	virtual const char* GetNodeTypeName( TFlowNodeId id );
	virtual void RemoveNode( const char * name );
	virtual void RemoveNode( TFlowNodeId id );
	virtual void SetEnabled(bool bEnabled);
	virtual bool IsEnabled() const { return m_bEnabled; }
	virtual void SetActive(bool bActive); 
	virtual bool IsActive() const { return m_bActive; }

	virtual void RegisterFlowNodeActivationListener(SFlowNodeActivationListener *listener);
	virtual void RemoveFlowNodeActivationListener(SFlowNodeActivationListener *listener);

	virtual void Update();
	virtual void InitializeValues();
	virtual bool SerializeXML( const XmlNodeRef& root, bool reading );
	virtual void Serialize( TSerialize ser );
	virtual void SetRegularlyUpdated( TFlowNodeId id, bool regularly );
	virtual void RequestFinalActivation( TFlowNodeId );
	virtual void ActivateNode( TFlowNodeId id ) { ActivateNodeInt(id); }
	virtual void ActivatePortAny( SFlowAddress output, const TFlowInputData& );
	virtual bool SetInputValue( TFlowNodeId node, TFlowPortId port, const TFlowInputData& );
	virtual bool IsOutputConnected( SFlowAddress output );
	virtual const TFlowInputData* GetInputValue( TFlowNodeId node, TFlowPortId port );
	virtual void SetEntityId(TFlowNodeId, EntityId);
	virtual EntityId GetEntityId(TFlowNodeId);
	// ~IFlowGraph

	static void UpdateAll();

	virtual void GetMemoryStatistics(ICrySizer * s);

	// temporary solutions [ ask Dejan ]

	// Suspended flow graphs were needed for AI Action flow graphs.
	// Suspended flow graphs aren't updated... 
	// Nodes in suspended flow graphs should ignore OnEntityEvent notifications!
	virtual void SetSuspended( bool suspend = true );
	virtual bool IsSuspended() const;

	// AI action related
	virtual void SetAIAction( IAIAction* pAIAction );
	virtual IAIAction* GetAIAction() const;

	//////////////////////////////////////////////////////////////////////////
	IEntity* GetIEntityForNode( TFlowNodeId id );

	// Called only from CFlowSystem::~CFlowSystem()
	void NotifyFlowSystemDestroyed();


	void RegisterInspector(IFlowGraphInspectorPtr pInspector);
	void UnregisterInspector(IFlowGraphInspectorPtr pInspector);
	const std::vector<IFlowGraphInspectorPtr>& GetInspectors() const { return m_inspectors; }

	CFlowSystem* GetSys() const { return m_pSys; }

	// get some more stats
	void GetGraphStats(int& nodeCount, int& edgeCount);

protected:

	// helper to broadcast an activation
	template <class T> 
	void PerformActivation( const SFlowAddress, const T& value );

	void CloneInner( CFlowGraphBase * pClone );

private:
	class CNodeIterator;
	class CEdgeIterator;

	void FlowLoadError( const char *format,... ) PRINTF_PARAMS(2, 3);

	ILINE void EnsureSortedEdges()
	{
		if (!m_bEdgesSorted)
			SortEdges();
	}
	ILINE void NeedUpdate()
	{
		if (m_pUpdateNext == FLOWGRAPH_NOT_IN_LIST)
		{
			m_pUpdateNext = m_pUpdateRoot;
			m_pUpdateRoot = this;
		}
	}
	ILINE void ActivateNodeInt( TFlowNodeId id )
	{
		if (m_modifiedNodes[id] == NOT_MODIFIED)
		{
			m_modifiedNodes[id] = m_firstModifiedNode;
			m_firstModifiedNode = id;
		}
		if (!m_bInUpdate)
			NeedUpdate();
	}
	IFlowNodePtr CreateNodeOfType( IFlowNode::SActivationInfo* pInfo, TFlowNodeTypeId typeId );
	void SortEdges();
	TFlowNodeId AllocateId();
	void DeallocateId( TFlowNodeId );
	bool ValidateAddress( SFlowAddress );
	bool ValidateNode( TFlowNodeId );
	bool ValidateLink( SFlowAddress& from, SFlowAddress& to );
	static void RemoveNodeFromActivationArray( TFlowNodeId id, TFlowNodeId& front, std::vector<TFlowNodeId>& array );
	void Cleanup();
	bool ReadXML( const XmlNodeRef& root );
	bool WriteXML( const XmlNodeRef& root );
	std::pair<CFlowData *, TFlowNodeId> CreateNodeInt( TFlowNodeTypeId typeId, const char * name,void *pUserData=0 );
	string PrettyAddress( SFlowAddress addr );
	SFlowAddress ResolveAddress( string node, string port, bool isOutput );
	void DoUpdate( IFlowNode::EFlowEvent event );
	void NeedInitialize()
	{
		m_bNeedsInitialize = true;
		NeedUpdate();
	}

	void NotifyFlowNodeActivationListeners(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, CryStringT<char> value);
	void NotifyFlowNodeActivationListeners(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const char *value);
	template <class T>
	void NotifyFlowNodeActivationListeners(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, T &value);

	const char* InternalGetDebugName();

#if defined (FLOW_DEBUG_PENDING_UPDATES)
	void DebugPendingActivations();
	void CreateDebugName();
	// a more or less descriptive name
	string m_debugName;
#endif

	// the set of modified nodes
	// not modified marker
	static const TFlowNodeId NOT_MODIFIED
#if !defined(LINUX)
	  = ~TFlowNodeId(0)
#endif
	  ;
	// end of modified list marker
	static const TFlowNodeId END_OF_MODIFIED_LIST
#if !defined(LINUX)
	  = NOT_MODIFIED-1
#endif
	  ;
	// PerformActivation works with this
	std::vector<TFlowNodeId> m_modifiedNodes;
	// This list is used for flowgraph debugging
	std::vector<SFlowNodeActivationListener*> m_flowNodeActivationListeners;
	// and Update swaps modifiedNodes and this so that we don't get messed
	// up during the activation sweep
	std::vector<TFlowNodeId> m_activatingNodes;
	// and this is the head of m_modifiedNodes
	TFlowNodeId m_firstModifiedNode;
	// and this is the head of m_activatingNodes
	TFlowNodeId m_firstActivatingNode;
	// are we in an update loop?
	bool m_bInUpdate;
	// Activate may request a final activation; these get inserted here, and we
	// sweep through it at the end of the update process
	std::vector<TFlowNodeId> m_finalActivatingNodes;
	TFlowNodeId m_firstFinalActivatingNode;

	// all of the node data
	std::vector<CFlowData> m_flowData;
	// deallocated id's waiting to be reused
	std::vector<TFlowNodeId> m_deallocatedIds;	

	// a link between nodes
	struct SEdge
	{
		ILINE SEdge() : fromNode(InvalidFlowNodeId), toNode(InvalidFlowNodeId), fromPort(InvalidFlowPortId), toPort(InvalidFlowPortId) {}
		ILINE SEdge( SFlowAddress from, SFlowAddress to ) : fromNode(from.node), toNode(to.node), fromPort(from.port), toPort(to.port) 
		{
			assert( from.isOutput );
			assert( !to.isOutput );
		}
		ILINE SEdge( TFlowNodeId fromNode_, TFlowPortId fromPort_, TFlowNodeId toNode_, TFlowPortId toPort_ ) : fromNode(fromNode_), toNode(toNode_), fromPort(fromPort_), toPort(toPort_) {}

		TFlowNodeId fromNode;
		TFlowNodeId toNode;
		TFlowPortId fromPort;
		TFlowPortId toPort;

		ILINE bool operator<(const SEdge& rhs) const
		{
			if (fromNode < rhs.fromNode)
				return true;
			else if (fromNode > rhs.fromNode)
				return false;
			else if (fromPort < rhs.fromPort)
				return true;
			else if (fromPort > rhs.fromPort)
				return false;
			else if (toNode < rhs.toNode)
				return true;
			else if (toNode > rhs.toNode)
				return false;
			else if (toPort < rhs.toPort)
				return true;
			else
				return false;
		}
		ILINE bool operator==(const SEdge& rhs) const
		{
			return fromNode == rhs.fromNode && fromPort == rhs.fromPort &&
				toNode == rhs.toNode && toPort == rhs.toPort;
		}
	};
	class SEdgeHasNode;
	std::vector<SEdge> m_edges;
	bool m_bEnabled; 
	bool m_bActive;   
	bool m_bEdgesSorted;
	bool m_bNeedsInitialize;
	CFlowSystem* m_pSys;

	// all of the regularly updated nodes (there aught not be too many)
	typedef std::vector<TFlowNodeId> RegularUpdates;
	RegularUpdates m_regularUpdates;
	RegularUpdates m_activatingUpdates;
	EntityId m_graphEntityId[MAX_GRAPH_ENTITIES];

	// reference count
	int m_nRefs;

	// nodes -> id resolution
	std::map<string, TFlowNodeId> m_nodeNameToId;
	// hooks
	std::vector<IFlowGraphHookPtr> m_hooks;
	// user data for editor
	std::map<TFlowNodeId, XmlNodeRef> m_userData;
	// inspectors
	std::vector<IFlowGraphInspectorPtr> m_inspectors;

	IEntitySystem *m_pEntitySystem;

	// temporary solutions [ ask Dejan ]
	bool m_bSuspended;
	bool m_bIsAIAction; // flag that this FlowGraph is an AIAction
	//                     first and only time set in SetAIAction call with an action != 0
	//                     it is never reset. needed when activations are pending which is o.k. for Actiongraphs
	IAIAction* m_pAIAction;

	static CFlowGraphBase * m_pUpdateRoot;
	CFlowGraphBase * m_pUpdateNext;
};

// this function is only provided to assist implementation of ActivatePort()
// force it inline for code size
template <class T>
ILINE void CFlowGraphBase::PerformActivation( const SFlowAddress addr, const T& value )
{
	assert( ValidateAddress( addr ) );

	if (m_bActive == false || m_bEnabled==false)
		return;

	if (addr.isOutput)
	{
		EnsureSortedEdges();

		/*
		SEdge firstEdge(addr.node, addr.port, 0, 0);
		std::vector<SEdge>::const_iterator iter = std::lower_bound( m_edges.begin(), m_edges.end(), firstEdge );
		*/
		std::vector<SEdge>::const_iterator iter = m_edges.begin() + m_flowData[addr.node].GetOutputFirstEdge(addr.port);
		while (iter != m_edges.end() && iter->fromNode == addr.node && iter->fromPort == addr.port)
		{
			m_flowData[iter->toNode].ActivateInputPort( iter->toPort, value );
			// see if we need to insert this node into the modified list
			ActivateNodeInt( iter->toNode );
			if(gEnv->bEditorGameMode && CCryAction::GetCryAction()->IsGameStarted() && !m_bNeedsInitialize)
			{
				ICVar *pToggle = gEnv->pConsole->GetCVar("fg_iEnableFlowgraphNodeDebugging");
				if(pToggle && pToggle->GetIVal() > 0)
					NotifyFlowNodeActivationListeners(iter->fromNode, iter->fromPort, iter->toNode, iter->toPort, value);
			}

			if (m_pSys != 0 && m_pSys->IsInspectingEnabled()) {
				m_pSys->NotifyFlow(this, addr, SFlowAddress(iter->toNode, iter->toPort, false));
			}
			// move next
			++iter;
		}
	}
	else
	{
		m_flowData[addr.node].ActivateInputPort( addr.port, value );
		ActivateNodeInt( addr.node );
	}
}

// this class extends CFlowGraph with type dependent operations
template <class TL>
class CFlowGraphStub;

template <class H, class T>
class CFlowGraphStub< NTypelist::CNode<H,T> > : public CFlowGraphStub<T>
{
public:
	CFlowGraphStub( CFlowSystem* pSys ) : CFlowGraphStub<T>(pSys) {}
	virtual void DoActivatePort( const SFlowAddress addr, const H& value )
	{
		PerformActivation( addr, value );
	}
};

template <>
class CFlowGraphStub< NTypelist::CEnd > : public CFlowGraphBase
{
public:
	CFlowGraphStub( CFlowSystem* pSys ) : CFlowGraphBase(pSys) {}
};

class CFlowGraph : public CFlowGraphStub<TFlowSystemDataTypes>
{
public:
	CFlowGraph( CFlowSystem* pSys ) : CFlowGraphStub<TFlowSystemDataTypes>(pSys) {}

	virtual void GetMemoryStatistics(ICrySizer * s);
};

#endif
