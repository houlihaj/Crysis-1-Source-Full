////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   IHyperGraph.h
//  Version:     v1.00
//  Created:     21/3/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: Declares HyperGraph interfaces.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __IHyperGraph_h__
#define __IHyperGraph_h__
#pragma once

struct IHyperNode;
struct IAIAction;
class CHyperEdge;

enum EHyperGraphEvent
{
	EHG_GRAPH_ADDED,
	EHG_GRAPH_REMOVED,
	EHG_GRAPH_INVALIDATE,
	EHG_GRAPH_OWNER_CHANGE,
	EHG_NODE_ADD,
	EHG_NODE_DELETE,
	EHG_NODE_CHANGE,
	EHG_NODE_CHANGE_DEBUG_PORT,
	EHG_NODE_SELECT,
	EHG_NODE_UNSELECT,
};

typedef uint32 HyperNodeID;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Callback class to intercept item creation and deletion events.
//////////////////////////////////////////////////////////////////////////
struct IHyperGraphListener
{
	virtual void OnHyperGraphEvent( IHyperNode *pNode,EHyperGraphEvent event ) = 0;
	virtual void OnLinkEdit( CHyperEdge * pEdge ) {}
};

//////////////////////////////////////////////////////////////////////////
// Description:
//    Callback class to recieve hyper graph manager notification event.
//////////////////////////////////////////////////////////////////////////
struct IHyperGraphManagerListener
{
	virtual void OnHyperGraphManagerEvent( EHyperGraphEvent event ) = 0;
};

//////////////////////////////////////////////////////////////////////////
// Description:
//    his interface is used to enumerate al items registered to the database manager.
//////////////////////////////////////////////////////////////////////////
struct IHyperGraphEnumerator
{
	// Use Release when you dont need enumerator anymore to release it.
	virtual void Release() = 0;
	virtual IHyperNode* GetFirst() = 0;
	virtual IHyperNode* GetNext() = 0;
};

//////////////////////////////////////////////////////////////////////////
// IHyperClass is the main interface to the hyper graph subsystem
//////////////////////////////////////////////////////////////////////////
struct IHyperGraph : public _i_reference_target_t
{
	virtual IHyperGraphEnumerator* GetNodesEnumerator() = 0;

	virtual void AddListener( IHyperGraphListener *pListener ) = 0;
	virtual void RemoveListener( IHyperGraphListener *pListener ) = 0;

	virtual IHyperNode* CreateNode( const char *sNodeClass, Gdiplus::PointF& pos=Gdiplus::PointF(0.0f, 0.0f) ) = 0;
	virtual IHyperNode* CloneNode( IHyperNode *pFromNode ) = 0;
	virtual void RemoveNode( IHyperNode *pNode ) = 0;

	virtual void Serialize( XmlNodeRef &node,bool bLoading,CObjectArchive* ar ) = 0;
	virtual bool Save( const char *filename ) = 0;
	virtual bool Load( const char *filename ) = 0;
	// Migrate needed because not every serialize should migrate.
	// return false if nothing has been changed, true otherwise
	virtual bool Migrate( XmlNodeRef &node ) = 0;

	// Return current hyper graph name.
	virtual const char* GetName() const = 0;
	// Assign new name to hyper graph.
	virtual void SetName( const char *sName ) = 0;

	virtual IAIAction* GetAIAction() const = 0;
};

//////////////////////////////////////////////////////////////////////////
// IHyperNode is an interface to the single hyper graph node.
//////////////////////////////////////////////////////////////////////////
struct IHyperNode : public _i_reference_target_t
{
	// Return the graph of this hyper node.
	virtual IHyperGraph* GetGraph() const = 0;

	// Get node name.
	virtual const char* GetName() const = 0;
	// Change node name.
	virtual void SetName( const char *sName ) = 0;

	// Get node id.
	virtual HyperNodeID GetId() const = 0;
};

#endif // __IHyperGraph_h__
