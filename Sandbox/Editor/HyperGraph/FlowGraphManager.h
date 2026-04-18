////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowGraphManager.h
//  Version:     v1.00
//  Created:     8/4/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FlowGraphManager_h__
#define __FlowGraphManager_h__
#pragma once

#include "HyperGraphManager.h"
#include "FlowGraphMigrationHelper.h"

struct IAIAction;
struct IFlowNode;
struct IFlowNodeData;
struct IFlowGraph;
struct SInputPortConfig;

class CEntity;
class CFlowGraph;
class CFlowNode;

//////////////////////////////////////////////////////////////////////////
class CFlowGraphManager : public CHyperGraphManager
{
public:
	//////////////////////////////////////////////////////////////////////////
	// CHyperGraphManager implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual ~CFlowGraphManager();

	virtual void Init();
	virtual void ReloadClasses();

	virtual CHyperNode* CreateNode( CHyperGraph* pGraph,const char *sNodeClass,HyperNodeID nodeId, const Gdiplus::PointF& pos, CBaseObject* pObj = NULL);
	virtual CHyperGraph* CreateGraph();
	//////////////////////////////////////////////////////////////////////////

	void OpenFlowGraphView( IFlowGraph *pIFlowGraph );

	// KLUDGE: unregisters flowgraphs a sets view to 0 
	//         currently only called from CEntity::RemoveFlowGraph
	void UnregisterAndResetView(CFlowGraph *pFlowGraph);

	// Create graph for specific entity.
	CFlowGraph* CreateGraphForEntity( CEntity *pEntity,const char *sGroupName="" );

	// Create graph for specific AI Action.
	CFlowGraph* CreateGraphForAction( IAIAction* pAction );

	CFlowGraph* FindGraphForAction( IAIAction* pAction );

	// Deletes all graphs created for AI Actions
	void FreeGraphsForActions();

	//////////////////////////////////////////////////////////////////////////
	// Create special nodes.
	CFlowNode* CreateSelectedEntityNode( CFlowGraph* pFlowGraph, CBaseObject *pSelObj);
	CFlowNode* CreateEntityNode( CFlowGraph* pFlowGraph,CEntity *pEntity );

	int GetFlowGraphCount() const { return m_graphs.size(); }
	CFlowGraph* GetFlowGraph( int nIndex ) { return m_graphs[nIndex]; };
	void GetAvailableGroups(std::set<CString>& outGroups, bool bActionGraphs=false);

	CFlowGraphMigrationHelper& GetMigrationHelper()
	{
		return m_migrationHelper;
	}

private:
	friend class CFlowGraph;
	void RegisterGraph( CFlowGraph *pGraph );
	void UnregisterGraph( CFlowGraph *pGraph );

	void LoadAssociatedBitmap( class CFlowNode *pFlowNode );

	void GetNodeConfig( IFlowNodeData *pSrcNode,CFlowNode *pFlowNode );
	IVariable* MakeInVar(const SInputPortConfig* pConfig);
	IVariable* MakeSimpleVarFromFlowType(int type );

private:
	// All graphs currently created.
	std::vector<CFlowGraph*> m_graphs;
	CFlowGraphMigrationHelper m_migrationHelper;
	int fg_iEnableFlowgraphNodeDebugging;
};

#endif // __FlowGraphManager_h__
