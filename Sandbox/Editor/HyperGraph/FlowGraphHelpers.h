////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   PropertyCtrlExt.h
//  Version:     v1.00
//  Created:     03-11-2005 by AlexL.
//  Compilers:   Visual Studio.NET
//  Description: some helpers for flowgraphs
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FlowGraphHelpers_h__
#define __FlowGraphHelpers_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include <vector>

class CEntity;
class CFlowGraph;

namespace FlowGraphHelpers
{
	// Description:
	//     Get human readable name for the FlowGraph
	//     incl. group, entity, AI action
	// Arguments:
	//     pFlowGraph - Ptr to flowgraph
	//     outName    - CString with human name 
	// Return Value:
	//     none
	void GetHumanName(CFlowGraph* pFlowGraph, CString& outName);

	// Description:
	//     Get all flowgraphs an entity is used in
	// Arguments:
	//     pEntity            - Entity 
	//     outFlowGraphs      - Vector of all flowgraphs (sorted by HumanName)
	//     outEntityFlowGraph - If the entity is owner of a flowgraph this is the pointer to
	// Return Value:
	//     none
	void FindGraphsForEntity(CEntity* pEntity, std::vector<CFlowGraph*>& outFlowGraphs, CFlowGraph*& outEntityFlowGraph);
};

#endif
