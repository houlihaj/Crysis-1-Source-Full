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

#ifndef __FlowGraph_h__
#define __FlowGraph_h__
#pragma once

#include "HyperGraph.h"

#include <IEntitySystem.h>
#include <IFlowSystem.h>

class CEntity;
struct IAIAction;

//////////////////////////////////////////////////////////////////////////
class CFlowEdge : public CHyperEdge
{
public:
	SFlowAddress addr_in;
	SFlowAddress addr_out;
};

//////////////////////////////////////////////////////////////////////////
// Specialization of HyperGraph to handle logical Flow Graphs.
//////////////////////////////////////////////////////////////////////////
class CFlowGraph : public CHyperGraph, SFlowNodeActivationListener
{
public:
	CFlowGraph( CHyperGraphManager *pManager,const char *sGroupName="" );
	CFlowGraph( CHyperGraphManager *pManager,IFlowGraph *pIFlowGraph );

	//////////////////////////////////////////////////////////////////////////
	// From CHyperGraph
	//////////////////////////////////////////////////////////////////////////
	virtual IHyperGraph* Clone();
	virtual void Serialize( XmlNodeRef &node,bool bLoading, CObjectArchive* ar );
	virtual void BeginEditing();
	virtual void EndEditing();
	virtual void SetEnabled( bool bEnable );
	virtual bool IsFlowGraph() { return true; }
	virtual bool Migrate( XmlNodeRef &node );
	virtual void SetGroupName( const CString &sName );
	virtual void PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx );
	virtual bool CanConnectPorts( CHyperNode *pSrcNode,CHyperNodePort *pSrcPort,CHyperNode *pTrgNode,CHyperNodePort *pTrgPort );
	//////////////////////////////////////////////////////////////////////////


	//////////////////////////////////////////////////////////////////////////
	IFlowGraph* GetIFlowGraph();

	// SFlowNodeActivationListener
	virtual void OnFlowNodeActivation(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const char *value);
	// ~SFlowNodeActivationListener

	//////////////////////////////////////////////////////////////////////////
	// Assigns current entity of the flow graph.
	// if bAdjustGraphNodes==true assigns this entity to all nodes which have t
	void SetEntity( CEntity *pEntity, bool bAdjustGraphNodes=false );
	CEntity* GetEntity() const { return m_pEntity; }
	//////////////////////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////////////////////
	// Assigns AI Action to the flow graph.
	void SetAIAction( IAIAction* pAIAction );
	virtual IAIAction* GetAIAction() const { return m_pAIAction; }
	//////////////////////////////////////////////////////////////////////////
	
	enum EMultiPlayerType
	{
		eMPT_ServerOnly = 0,
		eMPT_ClientOnly,
		eMPT_ClientServer
	};

	void SetMPType(EMultiPlayerType mpType)
	{
		m_mpType = mpType;
	}

	EMultiPlayerType GetMPType()
	{
		return m_mpType;
	}

protected:
	virtual ~CFlowGraph();

	virtual void RemoveEdge( CHyperEdge *pEdge );
	virtual CHyperEdge* MakeEdge( CHyperNode *pNodeOut,CHyperNodePort *pPortOut,CHyperNode *pNodeIn,CHyperNodePort *pPortIn, bool bEnabled, bool fromSpecialDrag);
	virtual void OnPostLoad();
	virtual void EnableEdge(CHyperEdge *pEdge, bool bEnable );
	virtual void ClearAll();
	virtual IUndoObject* CreateUndo();

	void InitializeFromGame();

protected:
	IFlowGraphPtr m_pGameFlowGraph;
	CEntity *m_pEntity;
	IAIAction* m_pAIAction;

	// When set to true this flow graph is editable.
	bool m_bEditable;

	// MultiPlayer Type of this flowgraph (default ServerOnly)
	EMultiPlayerType m_mpType;
};

#endif // __FlowGraph_h__
