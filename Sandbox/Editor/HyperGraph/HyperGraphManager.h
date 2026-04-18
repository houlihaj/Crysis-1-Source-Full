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

#ifndef __HyperGraphManager_h__
#define __HyperGraphManager_h__
#pragma once

#include "HyperGraphNode.h"

struct IHyperGraph;

class CHyperGraph;
class CHyperNode;

//////////////////////////////////////////////////////////////////////////
// Manages collection of hyper node classes that can be created for hyper graphs.
//////////////////////////////////////////////////////////////////////////
class CHyperGraphManager
{
public:
	typedef Functor1wRet<CHyperNode*, bool> NodeFilterFunctor;

	virtual ~CHyperGraphManager() {}

	//////////////////////////////////////////////////////////////////////////
	// Initialize graph manager.
	// Must be called after full game initialization.
	virtual void Init();
	
	// Reload prototype node classes.
	virtual void ReloadClasses() = 0;

	void GetPrototypes( std::vector<CString> &prototypes,bool bForUI, NodeFilterFunctor filterFunc = NodeFilterFunctor() );
	void GetPrototypesEx ( std::vector<_smart_ptr<CHyperNode> > &prototypes, bool bForUI, NodeFilterFunctor filterFunc = NodeFilterFunctor() );

	virtual CHyperGraph* CreateGraph() = 0;
	virtual CHyperNode* CreateNode( CHyperGraph* pGraph,const char *sNodeClass,HyperNodeID nodeId, const Gdiplus::PointF& pos=Gdiplus::PointF(0.0f, 0.0f), CBaseObject* pObj = NULL );
	//////////////////////////////////////////////////////////////////////////

	virtual void AddListener( IHyperGraphManagerListener *pListener );
	virtual void RemoveListener( IHyperGraphManagerListener *pListener );

	// Opens view of the specified hyper graph.
	void OpenView( IHyperGraph *pGraph );

protected:
	void SendNotifyEvent( EHyperGraphEvent event );

protected:
	typedef std::map<string,_smart_ptr<CHyperNode> > NodesPrototypesMap;
	NodesPrototypesMap m_prototypes;

	typedef std::list<IHyperGraphManagerListener*> Listeners;
	Listeners m_listeners;
};

#endif // __HyperGraphManager_h__
