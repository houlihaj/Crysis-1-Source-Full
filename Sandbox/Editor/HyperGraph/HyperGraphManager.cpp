////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   HyperGraphManager.h
//  Version:     v1.00
//  Created:     8/4/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "HyperGraphManager.h"
#include "HyperGraphNode.h"
#include "HyperGraph.h"
#include "HyperGraphDialog.h"
#include "CommentNode.h"
#include "GameEngine.h"

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::Init()
{
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::OpenView( IHyperGraph *pGraph )
{
	CWnd *pWnd = GetIEditor()->OpenView( "Flow Graph" );
	if (pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
	{
		((CHyperGraphDialog*)pWnd)->SetGraph( (CHyperGraph*)pGraph );
	}
}

//////////////////////////////////////////////////////////////////////////
CHyperNode* CHyperGraphManager::CreateNode( CHyperGraph* pGraph,const char *sNodeClass,HyperNodeID nodeId, const Gdiplus::PointF& /* pos */, CBaseObject* pObj)
{
	// AlexL: ignore pos, as a SetPos would screw up Undo. And it will be set afterwards anyway
	CHyperNode *pNode = NULL;
	CHyperNode *pPrototype = stl::find_in_map( m_prototypes,sNodeClass,NULL );
	if (pPrototype)
	{
		pNode = pPrototype->Clone();
		pNode->m_id = nodeId;
		pNode->SetGraph( pGraph );
		pNode->Init();
		return pNode;
	}

	//////////////////////////////////////////////////////////////////////////
	// Hack for naming changes
	//////////////////////////////////////////////////////////////////////////
	string newtype;
	// Hack for Math: to Vec3: change.
	if (strncmp(sNodeClass,"Math:",5) == 0)
		newtype = string("Vec3:") + (sNodeClass + 5);

	if(newtype.size() > 0)
	{
		pPrototype = stl::find_in_map( m_prototypes,newtype,NULL );
		if (pPrototype)
		{
			pNode = pPrototype->Clone();
			pNode->m_id = nodeId;
			pNode->SetGraph( pGraph );
			pNode->Init();
			return pNode;
		}
	}
	//////////////////////////////////////////////////////////////////////////



	if (0 == strcmp(sNodeClass, CCommentNode::GetClassType()))
	{
		pNode = new CCommentNode();
		pNode->m_id = nodeId;
		pNode->SetGraph( pGraph );
		pNode->Init();
		return pNode;
	}

	ICVar	*pCVarFlowGraphWarnings=GetIEditor()->GetGameEngine()->GetSystem()->GetIConsole()->GetCVar("g_display_fg_warnings");
	if (!pCVarFlowGraphWarnings || (pCVarFlowGraphWarnings && pCVarFlowGraphWarnings->GetIVal()==1))
		Warning( "Loading Graph '%s': Node of class '%s' doesn't exist -> Node creation failed", pGraph->GetName(), sNodeClass );
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::GetPrototypes( std::vector<CString> &prototypes,bool bForUI, NodeFilterFunctor filterFunc )
{
	prototypes.clear();
	for (NodesPrototypesMap::iterator it = m_prototypes.begin(); it != m_prototypes.end(); ++it)
	{
		CHyperNode *pNode = it->second;
		if (filterFunc && filterFunc (pNode) == false)
			continue;
		if (bForUI && (pNode->CheckFlag(EHYPER_NODE_HIDE_UI)))
			continue;
		prototypes.push_back( pNode->GetClassName() );
	}
#if 0
	if (bForUI)
		prototypes.push_back( CCommentNode::GetClassType() );
#endif
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::GetPrototypesEx( std::vector<_smart_ptr<CHyperNode> > &prototypes, bool bForUI, NodeFilterFunctor filterFunc /* = NodeFilterFunctor( */)
{
	prototypes.clear();
	for (NodesPrototypesMap::iterator it = m_prototypes.begin(); it != m_prototypes.end(); ++it)
	{
		CHyperNode *pNode = it->second;
		if (filterFunc && filterFunc (pNode) == false)
			continue;
		if (bForUI && (pNode->CheckFlag(EHYPER_NODE_HIDE_UI)))
			continue;
		prototypes.push_back (it->second);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::SendNotifyEvent( EHyperGraphEvent event )
{
	Listeners::iterator next;
	for (Listeners::iterator it = m_listeners.begin(); it != m_listeners.end(); it = next)
	{
		next = it; next++;
		(*it)->OnHyperGraphManagerEvent( event );
	}
}


//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::AddListener( IHyperGraphManagerListener *pListener )
{
	stl::push_back_unique( m_listeners,pListener );
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphManager::RemoveListener( IHyperGraphManagerListener *pListener )
{
	stl::find_and_erase( m_listeners,pListener );
}