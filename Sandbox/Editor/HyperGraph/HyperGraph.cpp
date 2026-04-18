////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   HyperGraph.h
//  Version:     v1.00
//  Created:     21/3/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "HyperGraph.h"
#include "HyperGraphManager.h"
#include "GameEngine.h"

/*
//////////////////////////////////////////////////////////////////////////
// Undo object for CHyperNode.
class CUndoHyperNodeCreate : public IUndoObject
{
public:
	CUndoHyperNodeCreate( CHyperNode *node )
	{
		// Stores the current state of this object.
		assert( node != 0 );
		m_node = node;
		m_redo = 0;
		m_pGraph = node->GetGraph();
		m_node = node;
	}
protected:
	virtual int GetSize() { return sizeof(*this); }
	virtual const char* GetDescription() { return "HyperNodeUndoCreate"; };

	virtual void Undo( bool bUndo )
	{
		if (bUndo)
		{
			if (m_node)
			{
				m_redo = CreateXmlNode("Redo");
				CHyperGraphSerializer serializer(m_pGraph);
				serializer.SaveNode(m_node,true);
				serializer.Serialize( m_redo,false );
			}
		}
		// Undo object state.
		if (m_node)
		{
			m_node->Invalidate();
			m_pGraph->RemoveNode( m_node );
		}
		m_node = 0;
	}
	virtual void Redo()
	{
		if (m_redo)
		{
			CHyperGraphSerializer serializer(m_pGraph);
			serializer.SelectLoaded(true);
			serializer.Serialize( m_redo,true );
			m_node = serializer.GetFirstNode();
		}
	}
private:
	_smart_ptr<CHyperGraph> m_pGraph;
	_smart_ptr<CHyperNode> m_node;
	XmlNodeRef m_redo;
};
*/

//////////////////////////////////////////////////////////////////////////
CHyperGraphSerializer::CHyperGraphSerializer( CHyperGraph* pGraph, CObjectArchive* ar )
{
	m_pGraph = pGraph;
	m_pAR = ar;
	m_bLoading = false;
	m_bSelectLoaded = false;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphSerializer::SaveNode( CHyperNode *pNode,bool bSaveEdges )
{
	m_nodes.insert(pNode);
	if (bSaveEdges)
	{
		std::vector<CHyperEdge*> edges;
		if (m_pGraph->FindEdges( pNode,edges ))
		{
			for (int i = 0; i < edges.size(); i++)
			{
				SaveEdge( edges[i] );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphSerializer::SaveEdge( CHyperEdge *pEdge )
{
	m_edges.insert(pEdge);
}

namespace {
	struct sortNodesById
	{
		bool operator()(const _smart_ptr<CHyperNode>& lhs, const _smart_ptr<CHyperNode>& rhs) const
		{
			return lhs->GetId() < rhs->GetId();
		}
	};

	struct sortEdges
	{
		bool operator() (const _smart_ptr<CHyperEdge>& lhs, const _smart_ptr<CHyperEdge>& rhs) const
		{
			if (lhs->nodeOut < rhs->nodeOut)
				return true;
			else if (lhs->nodeOut > rhs->nodeOut)
				return false;
			else if (lhs->nodeIn < rhs->nodeIn)
				return true;
			else if (lhs->nodeIn > rhs->nodeIn)
				return false;
			else if (lhs->portOut < rhs->portOut)
				return true;
			else if (lhs->portOut > rhs->portOut)
				return false;
			else if (lhs->portIn < rhs->portIn)
				return true;
			else
				return false;
		}
	};
};

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphSerializer::Serialize( XmlNodeRef &node,bool bLoading,bool bLoadEdges,bool bIsPaste)
{
	static ICVar* pCVarFlowGraphWarnings=gEnv->pConsole->GetCVar("g_display_fg_warnings");

	bool bHasChanges = false;
	m_bLoading = bLoading;
	if (!m_bLoading)
	{
		// Save
		node->setAttr( "Description",m_pGraph->GetDescription() );
		node->setAttr( "Group",m_pGraph->GetGroupName() );

		// sort nodes by their IDs to make a pretty output file (suitable for merging)
		typedef std::set<_smart_ptr<CHyperNode>, sortNodesById > TSortedNodesSet;
		TSortedNodesSet sortedNodes (m_nodes.begin(), m_nodes.end());
		
		// sizes must match otherwise we had ID duplicates which is VERY BAD
		assert (sortedNodes.size() == m_nodes.size());

		// sort edges (don't use a set, because then our compare operation must be unique)
		typedef std::vector<_smart_ptr<CHyperEdge> > TEdgeVec;
		TEdgeVec sortedEdges (m_edges.begin(), m_edges.end());
		std::sort (sortedEdges.begin(), sortedEdges.end(), sortEdges());

		// Serialize nodes and edges into the xml.
		XmlNodeRef nodesXml = node->newChild( "Nodes" );
		for (TSortedNodesSet::iterator nit = sortedNodes.begin(); nit != sortedNodes.end(); ++nit)
		{
			CHyperNode *pNode = *nit;
			XmlNodeRef nodeXml = nodesXml->newChild("Node");
			nodeXml->setAttr( "Id",pNode->GetId() );
			pNode->Serialize( nodeXml,bLoading, m_pAR );
		}

		XmlNodeRef edgesXml = node->newChild( "Edges" );
		for (TEdgeVec::iterator eit = sortedEdges.begin(); eit != sortedEdges.end(); ++eit)
		{
			CHyperEdge *pEdge = *eit;
			XmlNodeRef edgeXml = edgesXml->newChild("Edge");

			edgeXml->setAttr( "nodeIn",pEdge->nodeIn );
			edgeXml->setAttr( "nodeOut",pEdge->nodeOut );
			edgeXml->setAttr( "portIn",pEdge->portIn );
			edgeXml->setAttr( "portOut",pEdge->portOut );
			edgeXml->setAttr( "enabled", pEdge->enabled);
		}
	}
	else
	{
		// Load
		m_pGraph->RecordUndo();
		GetIEditor()->SuspendUndo();

		m_pGraph->m_bLoadingNow = true;

		CHyperGraphManager *pManager = m_pGraph->GetManager();

		if (!bIsPaste)
		{
			m_pGraph->SetDescription( node->getAttr( "Description" ) );
			m_pGraph->SetGroupName( node->getAttr( "Group" ) );
		}

		if (!bIsPaste && gSettings.bFlowGraphMigrationEnabled)
		{
			bHasChanges = m_pGraph->Migrate(node);
		}

		XmlNodeRef nodesXml = node->findChild( "Nodes" );
		if (nodesXml)
		{
			HyperNodeID nodeId;
			for (int i = 0; i < nodesXml->getChildCount(); i++)
			{
				XmlNodeRef nodeXml = nodesXml->getChild(i);
				CString nodeclass;
				if (!nodeXml->getAttr( "Class",nodeclass ))
					continue;
				if (!nodeXml->getAttr( "Id",nodeId ))
					continue;

				// Check if node id is already occupied.
				if (m_pGraph->FindNode(nodeId))
				{
					// If taken, allocate new id, and remap old id.
					HyperNodeID newId = m_pGraph->AllocateId();
					RemapId( nodeId,newId );
					nodeId = newId;
				}

				CHyperNode *pNode = pManager->CreateNode( m_pGraph,nodeclass,nodeId );
				if (!pNode)
					continue;
				pNode->Serialize( nodeXml,bLoading,m_pAR );
				m_pGraph->AddNode( pNode );

				m_nodes.insert(pNode);

				if (m_bSelectLoaded)
					pNode->SetSelected(true);
			}
		}

		if (bLoadEdges)
		{
			XmlNodeRef edgesXml = node->findChild( "Edges" );
			if (edgesXml)
			{
				HyperNodeID nodeIn,nodeOut;
				CString portIn,portOut;
				bool bEnabled;
				for (int i = 0; i < edgesXml->getChildCount(); i++)
				{
					XmlNodeRef edgeXml = edgesXml->getChild(i);
	
					edgeXml->getAttr( "nodeIn",nodeIn );
					edgeXml->getAttr( "nodeOut",nodeOut );
					edgeXml->getAttr( "portIn",portIn );
					edgeXml->getAttr( "portOut",portOut );
					if (edgeXml->getAttr( "enabled", bEnabled) == false)
						bEnabled = true;

					nodeIn = GetId(nodeIn);
					nodeOut = GetId(nodeOut);

					CHyperNode *pNodeIn = (CHyperNode*)m_pGraph->FindNode(nodeIn);
					CHyperNode *pNodeOut = (CHyperNode*)m_pGraph->FindNode(nodeOut);
					if (!pNodeIn || !pNodeOut)
					{
						CString cause;
						if (!pNodeIn && !pNodeOut)
							cause.Format("source node %d and target node %d do not exist", nodeOut, nodeIn);
						else if (!pNodeIn)
							cause.Format("target node %d does not exist", nodeIn);
						else if (!pNodeOut)
							cause.Format("source node %d does not exist", nodeOut);

						CErrorRecord err;
						err.module = VALIDATOR_MODULE_FLOWGRAPH;
						err.severity = CErrorRecord::ESEVERITY_ERROR;
						err.error.Format("Loading Graph '%s': Can't connect edge <%d,%s> -> <%d,%s> because %s", 
							m_pGraph->GetName(), nodeOut, portOut.GetString(), nodeIn, portIn.GetString(),
							cause.GetString());
						err.pItem = 0;
						GetIEditor()->GetErrorReport()->ReportError( err );

						if (!pCVarFlowGraphWarnings || (pCVarFlowGraphWarnings && pCVarFlowGraphWarnings->GetIVal()==1))
							Warning( "Loading Graph '%s': Can't connect edge <%d,%s> -> <%d,%s> because %s", 
								m_pGraph->GetName(), nodeOut, portOut.GetString(), nodeIn, portIn.GetString(),
								cause.GetString());
						continue;
					}
					CHyperNodePort *pPortIn = pNodeIn->FindPort(portIn,true);
					CHyperNodePort *pPortOut = pNodeOut->FindPort(portOut,false);
					if (!pPortIn || !pPortOut)
					{
						CString cause;
						if (!pPortIn && !pPortOut)
							cause.Format("source port '%s' and target port '%s' do not exist", portOut.GetString(), portIn.GetString());
						else if (!pPortIn)
							cause.Format("target port '%s' does not exist", portIn.GetString());
						else if (!pPortOut)
							cause.Format("source port '%s' does not exist", portOut.GetString());

						CErrorRecord err;
						err.module = VALIDATOR_MODULE_FLOWGRAPH;
						err.severity = CErrorRecord::ESEVERITY_ERROR;
						err.error.Format("Loading Graph '%s': Can't connect edge <%d,%s> -> <%d,%s> because %s", 
							m_pGraph->GetName(), nodeOut, portOut.GetString(), nodeIn, portIn.GetString(),
							cause.GetString());
						err.pItem = 0;
						GetIEditor()->GetErrorReport()->ReportError( err );

						if (!pCVarFlowGraphWarnings || (pCVarFlowGraphWarnings && pCVarFlowGraphWarnings->GetIVal()==1))
							Warning( "Loading Graph '%s': Can't connect edge <%d,%s> -> <%d,%s> because %s", 
								m_pGraph->GetName(), nodeOut, portOut.GetString(), nodeIn, portIn.GetString(),
								cause.GetString());
						continue;
					}
						
					if (!bIsPaste || (pPortIn->bAllowMulti==false && m_pGraph->FindEdge(pNodeIn, pPortIn) == 0))
					{
						m_pGraph->MakeEdge( pNodeOut,pPortOut,pNodeIn,pPortIn, bEnabled, false );
					}
				}
			}
		}
		m_pGraph->m_bLoadingNow = false;
		// m_pGraph->SendNotifyEvent( 0,EHG_GRAPH_INVALIDATE );
		m_pGraph->OnPostLoad();
		
		GetIEditor()->ResumeUndo();
	}
	return bHasChanges;
}

//////////////////////////////////////////////////////////////////////////
HyperNodeID CHyperGraphSerializer::GetId( HyperNodeID id )
{
	id = stl::find_in_map( m_remap,id,id );
	return id;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphSerializer::RemapId( HyperNodeID oldId,HyperNodeID newId )
{
	m_remap[oldId] = newId;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphSerializer::GetLoadedNodes( std::vector<CHyperNode*> &nodes ) const
{
	stl::set_to_vector( m_nodes,nodes );
}

//////////////////////////////////////////////////////////////////////////
CHyperNode* CHyperGraphSerializer::GetFirstNode()
{
	if (m_nodes.empty())
		return 0;
	return *m_nodes.begin();
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Undo object for CHyperGraph.
//////////////////////////////////////////////////////////////////////////
class CUndoHyperGraph : public IUndoObject
{
public:
	CUndoHyperGraph( CHyperGraph *pGraph )
	{
		// Stores the current state of given graph.
		assert( pGraph != 0 );
		m_pGraph = pGraph;
		m_redo = 0;
		m_undo = CreateXmlNode("HyperGraph");
		m_pGraph->Serialize( m_undo,false );
	}
protected:
	virtual int GetSize() { return sizeof(*this); }
	virtual const char* GetDescription() { return "HyperNodeUndoCreate"; };

	virtual void Undo( bool bUndo )
	{
		if (bUndo)
		{
			m_redo = CreateXmlNode("HyperGraph");
			m_pGraph->Serialize( m_redo,false );
		}
		m_pGraph->Serialize( m_undo,true );
	}
	virtual void Redo()
	{
		if (m_redo)
		{
			m_pGraph->Serialize( m_redo,true );
		}
	}
private:
	_smart_ptr<CHyperGraph> m_pGraph;
	XmlNodeRef m_undo;
	XmlNodeRef m_redo;
};


//////////////////////////////////////////////////////////////////////////
template <class TMap>
class CHyperGraphNodeEnumerator : public IHyperGraphEnumerator
{
	TMap *m_pMap;
	typename TMap::iterator m_iterator;

public:
	CHyperGraphNodeEnumerator( TMap *pMap )
	{
		assert(pMap);
		m_pMap = pMap;
		m_iterator = m_pMap->begin();
	}
	virtual void Release() { delete this; };
	virtual IHyperNode* GetFirst()
	{
		m_iterator = m_pMap->begin();
		if (m_iterator == m_pMap->end())
			return 0;
		return m_iterator->second;
	}
	virtual IHyperNode* GetNext()
	{
		if (m_iterator != m_pMap->end())
			m_iterator++;
		if (m_iterator == m_pMap->end())
			return 0;
		return m_iterator->second;
	}
};

//////////////////////////////////////////////////////////////////////////
IHyperGraphEnumerator* CHyperGraph::GetNodesEnumerator()
{
	return new CHyperGraphNodeEnumerator<NodesMap>(&m_nodesMap);
}

//////////////////////////////////////////////////////////////////////////
CHyperGraph::CHyperGraph( CHyperGraphManager *pManager )
{
	m_bEnabled = true;
	m_bModified = false;
	m_bLoadingNow = false;
	m_pManager = pManager;
	m_name = "Default";

	m_viewOffset.SetPoint(0,0);
	m_fViewZoom = 1;
	m_bViewPosInitialized = false;

	m_nextNodeId = 1;
}

//////////////////////////////////////////////////////////////////////////
CHyperGraph::~CHyperGraph()
{
	m_pManager = 0;
	SendNotifyEvent( 0,EHG_GRAPH_REMOVED );
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::SetModified( bool bModified )
{
	m_bModified = bModified;
}


//////////////////////////////////////////////////////////////////////////
void CHyperGraph::AddListener( IHyperGraphListener *pListener )
{
	stl::push_back_unique( m_listeners,pListener );
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::RemoveListener( IHyperGraphListener *pListener )
{
	stl::find_and_erase( m_listeners,pListener );
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::SendNotifyEvent( IHyperNode *pNode,EHyperGraphEvent event )
{
	Listeners::iterator next;
	for (Listeners::iterator it = m_listeners.begin(); it != m_listeners.end(); it = next)
	{
		next = it; next++;
		(*it)->OnHyperGraphEvent( pNode,event );
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::InvalidateNode( IHyperNode *pNode )
{
	SendNotifyEvent( pNode,EHG_NODE_CHANGE );
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::AddNode( CHyperNode *pNode )
{
	RecordUndo();

	HyperNodeID id = pNode->GetId();
	m_nodesMap[id] = pNode;
	if (id >= m_nextNodeId)
		m_nextNodeId = id+1;
	SendNotifyEvent( pNode,EHG_NODE_ADD );
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::RemoveNode( IHyperNode *pNode )
{
	RecordUndo();

	SendNotifyEvent( pNode,EHG_NODE_DELETE );

	// Delete all connected edges.
	std::vector<CHyperEdge*> edges;
	if (FindEdges( (CHyperNode*)pNode,edges ))
	{
		for (int i = 0; i < edges.size(); i++)
		{
			RemoveEdge( edges[i] );
		}
	}
	((CHyperNode*)pNode)->Done();
	m_nodesMap.erase( pNode->GetId() );
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::RemoveNodeKeepLinks( IHyperNode *pNode )
{
	RecordUndo();

	SendNotifyEvent( pNode,EHG_NODE_DELETE );

	((CHyperNode*)pNode)->Done();
	m_nodesMap.erase( pNode->GetId() );
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::RemoveEdge( CHyperEdge *pEdge )
{
	RecordUndo();

	CHyperNode *nodeIn = (CHyperNode*)FindNode(pEdge->nodeIn);
	if (nodeIn)
	{
		CHyperNodePort *portIn = nodeIn->FindPort( pEdge->portIn,true );
		if (portIn)
			--portIn->nConnected;
		nodeIn->Unlinked( true );
	}
	CHyperNode *nodeOut = (CHyperNode*)FindNode(pEdge->nodeOut);
	if (nodeOut)
	{
		CHyperNodePort *portOut = nodeOut->FindPort( pEdge->portOut,false );
		if (portOut)
			--portOut->nConnected;
		nodeOut->Unlinked( false );
	}

	// Remove NodeIn link.
	EdgesMap::iterator it = m_edgesMap.lower_bound( pEdge->nodeIn );
	while (it != m_edgesMap.end() && it->first == pEdge->nodeIn)
	{
		if (it->second == pEdge)
		{
			m_edgesMap.erase(it);
			break;
		}
		it++;
	}
	// Remove NodeOut link.
	it = m_edgesMap.lower_bound( pEdge->nodeOut );
	while (it != m_edgesMap.end() && it->first == pEdge->nodeOut)
	{
		if (it->second == pEdge)
		{
			m_edgesMap.erase(it);
			break;
		}
		it++;
	}
	
	SendNotifyEvent( 0,EHG_GRAPH_INVALIDATE );
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::ClearAll()
{
	m_nodesMap.clear();
	m_edgesMap.clear();
}

//////////////////////////////////////////////////////////////////////////
CHyperEdge* CHyperGraph::CreateEdge()
{
	return new CHyperEdge();
}

//////////////////////////////////////////////////////////////////////////
IHyperNode* CHyperGraph::CreateNode( const char *sNodeClass, Gdiplus::PointF& pos )
{
	CHyperNode *pNode = NULL;
	if (strcmp(sNodeClass,"selected_entity") == 0)
	{
		CSelectionGroup *pSelection = GetIEditor()->GetSelection();
		for(int o = 0; o < pSelection->GetCount() && o < 20; ++o)
		{
			pNode = m_pManager->CreateNode( this, sNodeClass, AllocateId(), pos, pSelection->GetObject(o));
			if (pNode)
			{
				AddNode( pNode );
				pNode->SetPos(pos);
				pos.Y += 25.0f;
			}
		}
	}
	else
	{
		pNode = m_pManager->CreateNode( this,sNodeClass,AllocateId(), pos);
		if (pNode)
		{
			AddNode( pNode );
			pNode->SetPos(pos);
		}
	}
	return pNode;
}

//////////////////////////////////////////////////////////////////////////
IHyperNode* CHyperGraph::CloneNode( IHyperNode *pFromNode )
{
	CHyperNode *pNode = ((CHyperNode*)pFromNode)->Clone();
	pNode->m_id = AllocateId();
	AddNode( pNode );
	SetModified();
	return pNode;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx )
{
	for (NodesMap::iterator it = m_nodesMap.begin(); it != m_nodesMap.end(); ++it)
	{
		CHyperNode *pNode = it->second;
		pNode->PostClone(pFromObject, ctx);
	}
}

//////////////////////////////////////////////////////////////////////////
IHyperNode* CHyperGraph::FindNode( HyperNodeID id ) const
{
	return stl::find_in_map( m_nodesMap,id,NULL );
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::UnselectAll()
{
	for (NodesMap::iterator it = m_nodesMap.begin(); it != m_nodesMap.end(); ++it)
	{
		CHyperNode *pNode = it->second;
		if (pNode->IsSelected())
			pNode->SetSelected(false);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraph::CanConnectPorts( CHyperNode *pSrcNode,CHyperNodePort *pSrcPort,CHyperNode *pTrgNode,CHyperNodePort *pTrgPort )
{
	assert(pSrcNode);
	assert(pSrcPort);
	assert(pTrgNode);
	assert(pTrgPort);
	if (pSrcPort->pVar->GetType() != pTrgPort->pVar->GetType())
	{
		// All types can be connected?
		//return false;
	}
	if (pSrcPort->bInput == pTrgPort->bInput)
	{
		return false;
	}
	if (pSrcNode == pTrgNode)
		return false;

	CHyperNode *nodeIn = pSrcNode;
	CHyperNode *nodeOut = pTrgNode;
	CHyperNodePort *portIn = pSrcPort;
	CHyperNodePort *portOut = pTrgPort;
	if (pSrcPort->bInput)
	{
		nodeIn = pSrcNode;
		nodeOut = pTrgNode;
		portIn = pSrcPort;
		portOut = pTrgPort;
	}

	// Check for duplicates.
	CHyperEdge *pInEdge = FindEdge( nodeIn,portIn );
	if (pInEdge)
	{
		if (pInEdge->nodeOut == nodeOut->GetId() && stricmp(pInEdge->portOut,portOut->GetName())==0)
			return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraph::GetAllEdges( std::vector<CHyperEdge*> &edges ) const
{
	edges.clear();
	edges.reserve(m_edgesMap.size());
	for (EdgesMap::const_iterator it = m_edgesMap.begin(); it != m_edgesMap.end(); ++it)
	{
		edges.push_back(it->second);
	}
	std::sort( edges.begin(),edges.end() );
	edges.erase( std::unique(edges.begin(), edges.end()), edges.end() );
	return !edges.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraph::FindEdges( CHyperNode *pNode,std::vector<CHyperEdge*> &edges ) const
{
	edges.clear();

	HyperNodeID nodeId = pNode->GetId();
	EdgesMap::const_iterator it = m_edgesMap.lower_bound( nodeId );
	while (it != m_edgesMap.end() && it->first == nodeId)
	{
		edges.push_back(it->second);
		it++;
	}

	return !edges.empty();
}

//////////////////////////////////////////////////////////////////////////
CHyperEdge* CHyperGraph::FindEdge( CHyperNode *pNode,CHyperNodePort *pPort ) const
{
	HyperNodeID nodeId = pNode->GetId();
	const char *portname = pPort->GetName();
	EdgesMap::const_iterator it = m_edgesMap.lower_bound( nodeId );
	while (it != m_edgesMap.end() && it->first == nodeId)
	{
		if (pPort->bInput)
		{
			if (it->second->nodeIn == nodeId && stricmp(it->second->portIn,portname)==0)
				return it->second;
		}
		else
		{
			if (it->second->nodeOut == nodeId && stricmp(it->second->portOut,portname)==0)
				return it->second;
		}
		it++;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraph::ConnectPorts( CHyperNode *pSrcNode,CHyperNodePort *pSrcPort,CHyperNode *pTrgNode,CHyperNodePort *pTrgPort, bool specialDrag )
{
	if (!CanConnectPorts(pSrcNode,pSrcPort,pTrgNode,pTrgPort))
		return false;

	CHyperNode *nodeIn = pTrgNode;
	CHyperNode *nodeOut = pSrcNode;
	CHyperNodePort *portIn = pTrgPort;
	CHyperNodePort *portOut = pSrcPort;
	if (pSrcPort->bInput)
	{
		nodeIn = pSrcNode;
		nodeOut = pTrgNode;
		portIn = pSrcPort;
		portOut = pTrgPort;
	}

	RecordUndo();

	// check if input port already connected.
	if (!portIn->bAllowMulti)
	{
		CHyperEdge *pOldEdge = FindEdge( nodeIn,portIn );
		if (pOldEdge)
		{
			// Disconnect old edge.
			RemoveEdge( pOldEdge );
		}
	}

	MakeEdge( nodeOut,portOut,nodeIn,portIn, true, specialDrag );
	SendNotifyEvent( 0,EHG_GRAPH_INVALIDATE );

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::RegisterEdge( CHyperEdge *pEdge, bool fromSpecialDrag )
{
	m_edgesMap.insert( EdgesMap::value_type(pEdge->nodeIn,pEdge) );
	m_edgesMap.insert( EdgesMap::value_type(pEdge->nodeOut,pEdge) );
}

//////////////////////////////////////////////////////////////////////////
CHyperEdge* CHyperGraph::MakeEdge( CHyperNode *pNodeOut,CHyperNodePort *pPortOut,CHyperNode *pNodeIn,CHyperNodePort *pPortIn, bool bEnabled, bool fromSpecialDrag)
{
	assert(pNodeIn);
	assert(pNodeOut);
	assert(pPortIn);
	assert(pPortOut);
	CHyperEdge *pNewEdge = CreateEdge();

	pNewEdge->nodeIn = pNodeIn->GetId();
	pNewEdge->nodeOut = pNodeOut->GetId();
	pNewEdge->portIn = pPortIn->GetName();
	pNewEdge->portOut = pPortOut->GetName();
	pNewEdge->enabled = bEnabled;

	++pPortOut->nConnected;
	++pPortIn->nConnected;

	// All connected ports must be made visible
	pPortOut->bVisible = true;
	pPortIn->bVisible = true;
	
	pNewEdge->nPortIn = pPortIn->nPortIndex;
	pNewEdge->nPortOut = pPortOut->nPortIndex;

	RegisterEdge( pNewEdge, fromSpecialDrag );

	SetModified();

	return pNewEdge;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::EnableEdge( CHyperEdge *pEdge, bool bEnable )
{
	pEdge->enabled = bEnable;
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraph::IsNodeActivationModified()
{
	bool retVal = false;
	NodesMap::iterator it = m_nodesMap.begin();
	NodesMap::iterator end = m_nodesMap.end();
	for (; it != end; ++it)
	{
		if(it->second->IsPortActivationModified())
		{
			it->second->Invalidate(true);
			retVal = true;
		}
	}
	return retVal;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::ClearDebugPortActivation()
{
	NodesMap::iterator it = m_nodesMap.begin();
	NodesMap::iterator end = m_nodesMap.end();
	for (; it != end; ++it)
		it->second->ClearDebugPortActivation();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::OnPostLoad()
{
	
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::Serialize( XmlNodeRef &node,bool bLoading, CObjectArchive* ar )
{
	CHyperGraphSerializer serializer( this, ar );
	if (bLoading)
	{
		ClearAll();

		bool bHasChanges = serializer.Serialize( node, bLoading );
		SetModified(bHasChanges);
		SendNotifyEvent( 0,EHG_GRAPH_INVALIDATE );
	}
	else
	{
		for (NodesMap::iterator nit = m_nodesMap.begin(); nit != m_nodesMap.end(); ++nit)
		{
			serializer.SaveNode( nit->second );
		}
		for (EdgesMap::const_iterator eit = m_edgesMap.begin(); eit != m_edgesMap.end(); ++eit)
		{
			serializer.SaveEdge( eit->second );
		}
		serializer.Serialize( node,bLoading );
		SetModified(false);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraph::Migrate( XmlNodeRef & /* node */)
{
	// we changed nothing
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraph::Save( const char *filename )
{
	bool bWasModified = IsModified();

	m_filename = Path::GamePathToFullPath( filename );
	m_filename = Path::GetRelativePath( m_filename );

	XmlNodeRef graphNode = CreateXmlNode( "Graph" );
	Serialize( graphNode,false );

	bool success = SaveXmlNode( graphNode,filename );
	if (!success)
		SetModified(bWasModified);
	return success;
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraph::Load( const char *filename )
{
	m_filename = Path::GamePathToFullPath( filename );
	m_filename = Path::GetRelativePath( m_filename );

	if (m_name.IsEmpty())
	{
		m_name = m_filename;
		m_name = Path::RemoveExtension(m_name);
	}

	XmlParser parser;
	XmlNodeRef graphNode = parser.parse( filename );
	if (graphNode != NULL && graphNode->isTag("Graph"))
	{
		Serialize( graphNode,true );
		return true;
	}
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraph::Import( const char *filename )
{
	XmlParser parser;
	XmlNodeRef graphNode = parser.parse( filename );
	if (graphNode != NULL && graphNode->isTag("Graph"))
	{
		CUndo undo( "Import HyperGraph"  );
		
		UnselectAll();
		CHyperGraphSerializer serializer( this, 0);
		serializer.SelectLoaded(false);
		bool bHasChanges = serializer.Serialize( graphNode,true );
		SetModified(bHasChanges);
		SendNotifyEvent( 0,EHG_GRAPH_INVALIDATE );
		return true;
	}
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::RecordUndo()
{
	if (CUndo::IsRecording())
	{
		IUndoObject* pUndo = CreateUndo(); // derived classes can create undo for the Graph
		assert (pUndo != 0);
		CUndo::Record( pUndo );
	}
}

//////////////////////////////////////////////////////////////////////////
IUndoObject* CHyperGraph::CreateUndo()
{
	return new CUndoHyperGraph(this);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::EditEdge( CHyperEdge * pEdge )
{
	RecordUndo();
	for (Listeners::const_iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		(*iter)->OnLinkEdit( pEdge );
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraph::SetViewPosition( CPoint point,float zoom )
{
	m_bViewPosInitialized = true;
	m_viewOffset = point;
	m_fViewZoom = zoom;
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraph::GetViewPosition( CPoint &point,float &zoom )
{
	point = m_viewOffset;
	zoom = m_fViewZoom;
	return m_bViewPosInitialized;
}
