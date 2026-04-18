////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowGraphHelpers.cpp
//  Version:     v1.00
//  Created:     03-11-2005 by AlexL.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FlowGraphHelpers.h"

#include <Objects\Entity.h>
#include <IAIAction.h>
#include <HyperGraph/FlowGraphManager.h>
#include <HyperGraph/FlowGraph.h>

namespace {
	void GetRealName(CFlowGraph* pFG, CString& outName) {
		CEntity *pEntity = pFG->GetEntity();
		IAIAction *pAIAction = pFG->GetAIAction();
		outName = pFG->GetGroupName();
		if (!outName.IsEmpty()) outName+=":";
		if (pEntity)
		{
			outName+=pEntity->GetName();
		} 
		else if (pAIAction)
		{
			outName+=pAIAction->GetName();
			outName+=" <AI>";
		} 
		else
		{
			outName+=pFG->GetName();
		}
	}

	struct CmpByName {
		bool operator() (CFlowGraph* lhs, CFlowGraph* rhs) const
		{
			CString lName;
			CString rName;
			GetRealName(lhs,lName);
			GetRealName(rhs,rName);
			return lName.CompareNoCase(rName) < 0;
		}
	};
}

//////////////////////////////////////////////////////////////////////////
namespace FlowGraphHelpers
{
	void GetHumanName(CFlowGraph* pFlowGraph, CString& outName)
	{
		GetRealName(pFlowGraph, outName);
	}

	void FindGraphsForEntity(CEntity* pEntity, std::vector<CFlowGraph*>& outFlowGraphs, CFlowGraph*& outEntityFG)
	{
		if (pEntity)
		{
			typedef std::vector<TFlowNodeId> TNodeIdVec;
			typedef std::map<CFlowGraph*, TNodeIdVec, std::less<CFlowGraph*> > TFGMap;
			TFGMap fgMap;
			CFlowGraph* pEntityGraph = NULL;

			IEntitySystem* pEntSys = gEnv->pEntitySystem;
			EntityId myId = pEntity->GetEntityId();
			CFlowGraphManager* pFGMgr = GetIEditor()->GetFlowGraphManager();
			int numFG = pFGMgr->GetFlowGraphCount();
			for (int i=0; i<numFG; ++i)
			{
				CFlowGraph* pFG = pFGMgr->GetFlowGraph(i);
				
				// AI Action may not reference actual entities
				if (pFG->GetAIAction() != 0)
					continue;

				IFlowGraphPtr pGameFG = pFG->GetIFlowGraph();
				if (pGameFG == 0)
				{
					CryLogAlways("FlowGraphHelpers::FindGraphsForEntity: No Game FG for FlowGraph 0x%p", pFG);
				}
				else
				{
					if (pGameFG->GetGraphEntity(0) == myId ||
						pGameFG->GetGraphEntity(1) == myId)
					{
						pEntityGraph = pFG;
						fgMap[pFG].push_back(InvalidFlowNodeId);
						//					CryLogAlways("entity as graph entity: %p\n",pFG);
					}
					IFlowNodeIteratorPtr nodeIter (pGameFG->CreateNodeIterator());
					TFlowNodeId nodeId;
					while (IFlowNodeData* pNodeData = nodeIter->Next(nodeId))
					{
						EntityId id = pGameFG->GetEntityId(nodeId);
						if (myId == id && nodeId != InvalidFlowNodeId)
						{
							//					CryLogAlways("  node entity for id %d: %p\n",nodeId, pEntSys->GetEntity(id));
							fgMap[pFG].push_back(nodeId);
						}
					}
				}
			}

			//		CryLogAlways("found %d unique graphs",fgMap.size());

			typedef std::vector<CFlowGraph*> TFGVec;
			TFGVec fgSortedVec;
			fgSortedVec.reserve(fgMap.size());

			// if there's an entity graph, put it in front
			if (pEntityGraph != NULL)
			{
				fgSortedVec.push_back(pEntityGraph);
			}

			// fill in the rest
			for (TFGMap::const_iterator iter = fgMap.begin(); iter != fgMap.end(); ++iter) {
				if ((*iter).first != pEntityGraph)
					fgSortedVec.push_back((*iter).first);
			}

			// sort rest of list by name
			if (fgSortedVec.size() > 1)
			{
				if (pEntityGraph != NULL)
					std::sort(fgSortedVec.begin()+1,fgSortedVec.end(),CmpByName());
				else
					std::sort(fgSortedVec.begin(),fgSortedVec.end(),CmpByName());
			}
			outFlowGraphs.swap(fgSortedVec);
			outEntityFG = pEntityGraph;
		}
		else
		{
			outFlowGraphs.resize(0);
			outEntityFG = 0;
		}
	}
};