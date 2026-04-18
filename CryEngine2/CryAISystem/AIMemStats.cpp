#include "StdAfx.h"
#include <CrySizer.h>
#include "CAISystem.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include "AIPlayer.h"
#include "GoalPipe.h"
#include "GoalOp.h"
#include "AIAutoBalance.h"
#include "AStarSolver.h"
#include "TriangularNavRegion.h"
#include "WaypointHumanNavRegion.h"
#include "Waypoint3DSurfaceNavRegion.h"
#include "FlightNavRegion.h"
#include "VolumeNavRegion.h"
#include "RoadNavRegion.h"
#include "Free2DNavRegion.h"
#include "SmartObjectNavRegion.h"
#include "WorldOctree.h"

void CAISystem::GetMemoryStatistics(ICrySizer *pSizer)
{
//sizeof(bool) + sizeof(void*)*3 + sizeof(MapType::value_type)
  size_t size = 0;

  size = sizeof(*this);
  size += lstSectors.size() * sizeof(IVisArea*);
  if (m_pAutoBalance)
    size += sizeof(*m_pAutoBalance);

//  size+= (m_vWaitingToBeUpdated.size()+m_vAlreadyUpdated.size())*sizeof(CAIObject*);
	size += m_disabledPuppetsSet.size()*sizeof(CPuppet*);
	size += m_enabledPuppetsSet.size()*sizeof(CPuppet*);
  size += m_lstPathQueue.size()*(sizeof(PathFindRequest*)+sizeof(PathFindRequest));

  pSizer->AddObject( this, size); 



  {
//    char str[255];
//    sprintf(str,"%d AIObjects",m_Objects.size());
    SIZER_SUBCOMPONENT_NAME(pSizer,"AIObjects");			// string is used to identify component, it should not be dynamic

    AIObjects::iterator curObj = m_Objects.begin();

    for(;curObj!=m_Objects.end();curObj++)
    {
      CAIObject *pObj = (CAIObject *) curObj->second;

      size+=strlen(pObj->GetName());

      if(CPuppet* pPuppet = pObj->CastToCPuppet())
      {
        size += pPuppet->MemStats();
      }
      else if(CAIPlayer* pPlayer = pObj->CastToCAIPlayer())
      {
        size += sizeof *pPlayer;
      }
      else if(CPuppet* pVehicle = pObj->CastToCAIVehicle())
      {
        size += sizeof *pVehicle;
      }
      else
      {
        size += sizeof *pObj;
      }
    }
    pSizer->AddObject( &m_Objects, size );
  }

  {
    SIZER_SUBCOMPONENT_NAME(pSizer,"NavGraph");
    if(m_pGraph)
      pSizer->AddObject( m_pGraph, m_pGraph->MemStats());

		{
			SIZER_SUBCOMPONENT_NAME(pSizer,"NavRegions");
			if(m_pTriangularNavRegion)
			{
				SIZER_SUBCOMPONENT_NAME(pSizer,"Triangular");
				pSizer->AddObject( m_pTriangularNavRegion, m_pTriangularNavRegion->MemStats());
			}
			if(m_pWaypointHumanNavRegion)
			{
				SIZER_SUBCOMPONENT_NAME(pSizer,"WaypointHuman");
				pSizer->AddObject( m_pWaypointHumanNavRegion, m_pWaypointHumanNavRegion->MemStats());
			}
			if(m_pWaypoint3DSurfaceNavRegion)
			{
				SIZER_SUBCOMPONENT_NAME(pSizer,"Waypoint3DSurface");
				pSizer->AddObject( m_pWaypoint3DSurfaceNavRegion, m_pWaypoint3DSurfaceNavRegion->MemStats());
			}
			if(m_pVolumeNavRegion)
			{
				SIZER_SUBCOMPONENT_NAME(pSizer,"Volume");
				pSizer->AddObject( m_pVolumeNavRegion, m_pVolumeNavRegion->MemStats());
			}
			if(m_pFlightNavRegion)
			{
				SIZER_SUBCOMPONENT_NAME(pSizer,"Flight");
				pSizer->AddObject( m_pFlightNavRegion, m_pFlightNavRegion->MemStats());
			}
			if(m_pRoadNavRegion)
			{
				SIZER_SUBCOMPONENT_NAME(pSizer,"Road");
				pSizer->AddObject( m_pRoadNavRegion, m_pRoadNavRegion->MemStats());
			}
			if(m_pFree2DNavRegion)
			{
				SIZER_SUBCOMPONENT_NAME(pSizer,"Free2D");
				pSizer->AddObject( m_pFree2DNavRegion, m_pFree2DNavRegion->MemStats());
			}
			if(m_pSmartObjectNavRegion)
			{
				SIZER_SUBCOMPONENT_NAME(pSizer,"Smart");
				pSizer->AddObject( m_pSmartObjectNavRegion, m_pSmartObjectNavRegion->MemStats());
			}
		}
  }

	{
		SIZER_SUBCOMPONENT_NAME(pSizer,"HideGraph");
		if(m_pHideGraph)
			pSizer->AddObject( m_pHideGraph, m_pHideGraph->MemStats() + m_pHideGraph->NodeMemStats(IAISystem::NAVMASK_ALL));
	}

  {
    SIZER_SUBCOMPONENT_NAME(pSizer,"AStar");
    if(m_pAStarSolver)
      pSizer->AddObject( m_pAStarSolver, m_pAStarSolver->MemStats());
  }

  size = 0;
  {
    char str[255];
    sprintf(str,"%d GoalPipes",m_PipeManager.m_mapGoals.size());
    SIZER_SUBCOMPONENT_NAME(pSizer,"Goals");
    GoalMap::iterator gItr=m_PipeManager.m_mapGoals.begin();
    for(;gItr!=m_PipeManager.m_mapGoals.end();gItr++)
    {
      size += (gItr->first).capacity();
      size += (gItr->second)->MemStats() + sizeof(*(gItr->second));
    }
    pSizer->AddObject( &m_PipeManager.m_mapGoals, size);
  }


  size = 0;
  FormationDescriptorMap::iterator fItr=m_mapFormationDescriptors.begin();
  for(;fItr!=m_mapFormationDescriptors.end();fItr++)
  {
    size += (fItr->first).capacity();
    size += sizeof((fItr->second));
    size += (fItr->second).m_sName.capacity();
    size += (fItr->second).m_Nodes.size()*sizeof(FormationNode);
  }
  pSizer->AddObject( &m_mapFormationDescriptors, size);

  size = 0;
  PatternDescriptorMap::iterator pItr=m_mapTrackPatternDescriptors.begin();
  for(;pItr!=m_mapTrackPatternDescriptors.end();pItr++)
  {
    size += (pItr->first).capacity();
    size += (pItr->second).MemStats();
  }
  pSizer->AddObject( &m_mapTrackPatternDescriptors, size);

  size=0;
  for(ShapeMap::iterator itr=m_mapDesignerPaths.begin(); itr!=m_mapDesignerPaths.end(); itr++)
  {
    size += (itr->first).capacity();
    size += itr->second.MemStats();
  }
  pSizer->AddObject( &m_mapDesignerPaths, size);

  pSizer->AddObject( &m_forbiddenAreas, m_forbiddenAreas.MemStats());
	pSizer->AddObject( &m_designerForbiddenAreas, m_designerForbiddenAreas.MemStats());
	pSizer->AddObject( &m_forbiddenBoundaries, m_forbiddenBoundaries.MemStats());

  size=0;
  for(SpecialAreaMap::iterator sit=m_mapSpecialAreas.begin(); sit!=m_mapSpecialAreas.end(); sit++)
  {
    size += (sit->first).capacity();
    size += sizeof(SpecialArea);
  }
  pSizer->AddObject( &m_mapSpecialAreas, size);


  size = m_mapSpecies.size()*(sizeof(unsigned short)+sizeof(CAIObject*));
  pSizer->AddObject( &m_mapSpecies, size );

  size = m_mapGroups.size()*(sizeof(unsigned short)+sizeof(CAIObject*));
  pSizer->AddObject( &m_mapGroups, size );


  {
    SIZER_SUBCOMPONENT_NAME(pSizer,"Triangle vertex data");
    size = m_VertexList.GetCapacity()*sizeof(ObstacleData);
    pSizer->AddObject( &m_VertexList, size );
    size = m_vTriangles.capacity() * sizeof(Tri*) + m_vTriangles.size() * sizeof(Tri);
		pSizer->AddObject( &m_VertexList, size );
    size = 0;
    for(std::vector<Vtx>::iterator vtx=m_vVertices.begin(); vtx!=m_vVertices.end(); vtx++)
      size += sizeof(Vtx) + sizeof(int)*vtx->m_lstTris.size();
    pSizer->AddObject( &m_vVertices, size);
  }

  for(SetAIObjects::iterator curObj = m_setDummies.begin();curObj!=m_setDummies.end();curObj++)
  {
    CAIObject *pObj = (CAIObject *) (*curObj);
    size += sizeof *pObj;
    size+=strlen(pObj->GetName());
  }
  pSizer->AddObject( &m_setDummies, size );
}

size_t GraphNode::MemStats()
{
	size_t size = 0;

	switch (navType)
	{
	case IAISystem::NAV_UNSET:
		size += sizeof(GraphNode_Unset);
		break;
	case IAISystem::NAV_TRIANGULAR:
		size += sizeof(GraphNode_Triangular);
		size += (GetTriangularNavData()->vertices.capacity() ? GetTriangularNavData()->vertices.capacity() * sizeof(int) + 2 * sizeof(int): 0);
		break;
	case IAISystem::NAV_WAYPOINT_HUMAN:
		size += sizeof(GraphNode_WaypointHuman);
		break;
	case IAISystem::NAV_WAYPOINT_3DSURFACE:
		size += sizeof(GraphNode_Waypoint3DSurface);
		break;
	case IAISystem::NAV_FLIGHT:
		size += sizeof(GraphNode_Flight);
		break;
	case IAISystem::NAV_VOLUME:
		size += sizeof(GraphNode_Volume);
		break;
	case IAISystem::NAV_ROAD:
		size += sizeof(GraphNode_Road);
		break;
	case IAISystem::NAV_SMARTOBJECT:
		size += sizeof(GraphNode_SmartObject);
		break;
	case IAISystem::NAV_FREE_2D:
		size += sizeof(GraphNode_Free2D);
		break;
	default:
		break;
	}

	//size += links.capacity()*sizeof(unsigned);

	return size;
}

//====================================================================
// MemStats
//====================================================================
size_t CGraph::MemStats()
{
	size_t size = sizeof *this;
  size += sizeof(GraphNode*) * m_taggedNodes.capacity();
  size += sizeof(GraphNode*) * m_markedNodes.capacity();

  size += m_vNodeDescs.capacity()*sizeof(NodeDescriptor);
  size += m_vLinkDescs.capacity()*sizeof(LinkDescriptor);

	size += m_allNodes.MemStats();
	//size += NodesPool.MemStats();
	size += mBadGraphData.capacity()*sizeof(SBadGraphData);
	size += m_lstNodesInsideSphere.size()*sizeof(GraphNode*);
	size += m_mapRemovables.size()*sizeof(bool) + sizeof(void*)*3 + sizeof(EntranceMap::value_type);
	size += m_mapEntrances.size()*sizeof(bool) + sizeof(void*)*3 + sizeof(EntranceMap::value_type);
	size += m_mapExits.size()*sizeof(bool) + sizeof(void*)*3 + sizeof(EntranceMap::value_type);

	size += static_AllocatedObstacles.size()*sizeof(CObstacleRef);

  return size;
}

//====================================================================
// NodeMemStats
//====================================================================
size_t CGraph::NodeMemStats(unsigned navTypeMask)
{
  size_t nodesSize = 0;
  size_t linksSize = 0;

  CAllNodesContainer::Iterator it(m_allNodes, navTypeMask);
  while (unsigned nodeIndex = it.Increment())
  {
		GraphNode* pNode = GetNodeManager().GetNode(nodeIndex);

    if (navTypeMask & pNode->navType)
    {
			nodesSize += pNode->MemStats();
/*
      nodesSize += sizeof(GraphNode);
      linksSize += pNode->links.capacity() * sizeof(GraphLink);
      for (unsigned i = 0 ; i < pNode->links.size() ; ++i)
      {
        if (pNode->links[i].GetCachedPassabilityResult())
          linksSize += sizeof(GraphLink::SCachedPassabilityResult);
      }
      switch (pNode->navType)
      {
        case IAISystem::NAV_TRIANGULAR: 
          nodesSize += sizeof(STriangularNavData);
          nodesSize += pNode->GetTriangularNavData()->vertices.capacity() * sizeof(int);
          break;
        case IAISystem::NAV_WAYPOINT_3DSURFACE:
        case IAISystem::NAV_WAYPOINT_HUMAN: 
          nodesSize += sizeof(SWaypointNavData); 
          break;
        case IAISystem::NAV_SMARTOBJECT: nodesSize += sizeof(SSmartObjectNavData); 
          break;
        default: 
          break;
      }
*/
    }
  }
  return nodesSize + linksSize;
}


//====================================================================
// MemStats
//====================================================================
size_t CGoalPipe::MemStats()
{
  size_t size=sizeof(*this);

  VectorOGoals::iterator itr = m_qGoalPipe.begin();

  for(; itr!=m_qGoalPipe.end(); itr++)
  {
    size += sizeof( QGoal );
    size += itr->pipeName.capacity();
    size += sizeof (*(itr->pGoalOp));
    if(!itr->params.szString.empty())
      size += itr->params.szString.capacity();

    /*
    QGoal *curGoal = itr;
    size += sizeof *curGoal;
    size += curGoal->name.capacity();
    size += sizeof (*curGoal->pGoalOp);
    size += strlen(curGoal->params.szString);
    */
  }
  size += m_sName.capacity();
  return size;
}

size_t CPuppet::MemStats()
{
  size_t size=sizeof(*this);

  if(m_pCurrentGoalPipe)
    size += m_pCurrentGoalPipe->MemStats();

  /*
  GoalMap::iterator itr=m_mapAttacks.begin();
  for(; itr!=m_mapAttacks.end();itr++ )
  {
  size += (itr->first).capacity();
  size += sizeof(CGoalPipe *);
  }
  itr=m_mapRetreats.begin();
  for(; itr!=m_mapRetreats.end();itr++ )
  {
  size += (itr->first).capacity();
  size += sizeof(CGoalPipe *);
  }
  itr=m_mapWanders.begin();
  for(; itr!=m_mapWanders.end();itr++ )
  {
  size += (itr->first).capacity();
  size += sizeof(CGoalPipe *);
  }
  itr=m_mapIdles.begin();
  for(; itr!=m_mapIdles.end();itr++ )
  {
  size += (itr->first).capacity();
  size += sizeof(CGoalPipe *);
  }
  */
  //	if(m_mapVisibleAgents.size() < 1000)
  //	size += (sizeof(CAIObject*)+sizeof(VisionSD))*m_mapVisibleAgents.size();
  //	if(m_mapMemory.size()<1000)
  //	size += (sizeof(CAIObject*)+sizeof(MemoryRecord))*m_mapMemory.size();
  if(m_mapDevaluedPoints.size()<1000)
    size += (sizeof(CAIObject*)+sizeof(float))*m_mapDevaluedPoints.size();
  //	if(m_mapPotentialTargets.size()<1000)
  //	size += (sizeof(CAIObject*)+sizeof(float))*m_mapPotentialTargets.size();
  //	if(m_mapSoundEvents.size()<1000)
  //	size += (sizeof(CAIObject*)+sizeof(SoundSD))*m_mapSoundEvents.size();

  return size;
}
//
//----------------------------------------------------------------------------------

//====================================================================
// MemStats
//====================================================================
size_t CTriangularNavRegion::MemStats()
{
  size_t size=sizeof(*this);
  if (m_pGraph)
    size += m_pGraph->NodeMemStats(IAISystem::NAV_TRIANGULAR);
  return size;
}

//====================================================================
// MemStats
//====================================================================
size_t CWaypointHumanNavRegion::MemStats()
{
  size_t size=sizeof(*this);
  if (m_pGraph)
    size += m_pGraph->NodeMemStats(IAISystem::NAV_WAYPOINT_HUMAN);
  return size;
}

//====================================================================
// MemStats
//====================================================================
size_t CWaypoint3DSurfaceNavRegion::MemStats()
{
  size_t size=sizeof(*this);
  if (m_pGraph)
    size += m_pGraph->NodeMemStats(IAISystem::NAV_WAYPOINT_3DSURFACE);
  return size;
}

//====================================================================
// MemStats
//====================================================================
size_t CRoadNavRegion::MemStats()
{
  size_t size=sizeof(*this);
  if (m_pGraph)
    size += m_pGraph->NodeMemStats(IAISystem::NAV_ROAD);
  return size;
}

//====================================================================
// MemStats
//====================================================================
size_t CFlightNavRegion::MemStats()
{
  size_t size=sizeof(*this);
  size += sizeof(SSpan) * m_spans.capacity();
  if (m_pGraph)
    size += m_pGraph->NodeMemStats(IAISystem::NAV_FLIGHT);
  return size;
}

//====================================================================
// MemStats
//====================================================================
size_t CVolumeNavRegion::MemStats()
{
  size_t size=sizeof(*this);
  if (m_pGraph)
    size += m_pGraph->NodeMemStats(IAISystem::NAV_VOLUME);

  size += sizeof(CVolumeNavRegion::CVolume) * m_volumes.capacity();
  size += sizeof(CVolumeNavRegion::CPortal) * m_portals.capacity();

  for (unsigned i = 0 ; i < m_volumes.size() ; ++i)
  {
    const CVolume * vol = m_volumes[i];
    size += sizeof(vol);
    if (vol)
    {
      size += vol->m_portalIndices.capacity() * sizeof(int);
    }
  }

  return size;
}

//====================================================================
// MemStats
//====================================================================
size_t CAStarSolver::MemStats()
{
  size_t size=sizeof(*this);

  size += (sizeof(GraphNode*) + 4) * m_pathNodes.size();
  size += m_openList.MemStats();

  return size;
}

//====================================================================
// MemStats
//====================================================================
size_t CWorldOctree::MemStats()
{
  size_t size = sizeof(*this);

  size += m_cells.capacity() * sizeof(COctreeCell);
  size += m_triangles.capacity() * sizeof(CTriangle);

  for (unsigned i = 0 ; i < m_cells.size() ; ++i)
  {
    const COctreeCell & cell = m_cells[i];
    size += sizeof(cell.m_aabb);
    size += sizeof(cell.m_childCellIndices);
    size += cell.m_triangleIndices.capacity() * sizeof(int);
  }

  return size;
}

