/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   ObstacleAllocator.h
$Id$
Description: 

-------------------------------------------------------------------------
History:
- 2:6:2005   16:22 : Created by Kirill Bulatsev

*********************************************************************/
#ifndef __ObstacleAllocator_H__
#define __ObstacleAllocator_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include "platform.h"
#include "Graph.h"

#include <set>
#include <map>

class CAIObject;
class CAISystem;
class CLeader;

class CObstacleAllocator
{
public:
  typedef std::set< CAIObject* > SetUnits;
  typedef std::map< CAIObject*, CObstacleRef > MapUnitObstacles;

public:
  CObstacleAllocator(CLeader* pLeader)
    : m_pLeader(pLeader)
    , m_fSearchDistance(0)
    , m_fInitSearchDistance(0)
    , m_bKnownEnemyPosition(false) {}

    ~CObstacleAllocator()
    {
      MapUnitObstacles::iterator it, itEnd = m_UnitObstacles.end();
      for (it = m_UnitObstacles.begin(); it != itEnd; ++it)
        CGraph::FreeObstacle(it->second);
    }

    void SetSearchDistance(float dist, bool bIsEnemyPosKnown = true, float initDistance = 0.0f)
    {
      m_fSearchDistance = dist;
      m_bKnownEnemyPosition = bIsEnemyPosKnown;
      if (initDistance)
        m_fInitSearchDistance = initDistance;
      else
        m_fInitSearchDistance = m_fSearchDistance;
    }	

    void AddUnit(CAIObject* unit)
    {
      m_Units.insert(unit);
    }

    void RemoveUnit(CAIObject* unit)
    {
      FreeObstacle(unit);
      m_Units.erase(unit);
    }

    const ObstacleData	GetObstacleData(int index) const;
    CObstacleRef	GetUnitObstacle(CAIObject* unit) const;
    CObstacleRef	GetOrAllocUnitObstacle(CAIObject* unit, const Vec3& destination);
    CObstacleRef	ReallocUnitObstacle(CAIObject* unit, const Vec3& destination, bool allowSame = true, float preferredDistance=-1.0f, const Vec3& searchPosition=ZERO, bool bCollidableOnly=true);
    void		FreeObstacle(CAIObject* unit);
    void		Serialize( TSerialize ser, CObjectTracker& objectTracker );

protected:
  CObstacleRef	AllocObstacle(CAIObject* unit, const Vec3& destination, bool bCollidableOnly=true, float preferredDistance=-1.0f);
  CObstacleRef	AllocObstacle(const Vec3& from, const Vec3& to, bool bCollidableOnly=true, float preferredDistance=-1.0f);

  CLeader*				m_pLeader;
  SetUnits				m_Units;
  MapUnitObstacles	m_UnitObstacles;
  float				m_fInitSearchDistance;
  float				m_fSearchDistance;
  bool				m_bKnownEnemyPosition;
};

class CObstacleCostFtor
{
protected:
  const Vec3&	m_from;
  Vec3		m_to;
  const Vec3&	m_leader;
  Vec3		m_enemyDir;
  float		m_preferedDistance;
  float		m_searchDistance2;
  float		m_initialDistance;
  float		m_initialLeaderDistance;
  bool		m_bRandom;

public:

  CObstacleCostFtor(const Vec3& from, const Vec3& to, const Vec3& leader, float preferedDistance = -1.0f)
    : m_from(from), m_to(to), m_leader(leader)
  {
    m_bRandom = preferedDistance == -1.0f;
    m_enemyDir = to - from;
    m_initialDistance = m_enemyDir.GetLength();
    m_initialLeaderDistance = (m_leader - from).GetLength();
    if (preferedDistance < 0.0f)
    {
      if (m_from == m_to)
      {
        m_to = leader;
        m_preferedDistance = 0.0f;
      }
      else
        m_preferedDistance = 25.0f;
    }
    else
      m_preferedDistance = preferedDistance;
    m_searchDistance2 = 50.0f * 50.0f;
  }
  const Vec3& GetStartingPosition() const
  {
    return m_from;
  }

  void SetSearchDistance(float dist)
  {
    m_searchDistance2 = dist*dist;
  }	

  const Vec3& GetEnemyPosition() const
  {
    return m_to;
  }
  float GetSearchDistanceSquared() const
  {
    return m_searchDistance2;
  }
  float operator ()(Vec3 pos) const
  {
    float oldDistance = abs(m_preferedDistance - m_initialDistance); //(m_from - m_to).GetLength();
    float newDistance = abs(m_preferedDistance - (pos - m_to).GetLength());
    float benefit = (oldDistance - newDistance) * 2.0f;

    float closerToLeader = 3.0f * (m_initialLeaderDistance - (m_leader - pos).GetLength());

    Vec3 runDir = m_from - pos;
    float runDistance = runDir.GetLength();

    // run longer if too far!
    if (m_initialDistance > 80.0f)
    {
      benefit *= 2.0f;
      closerToLeader *= 2.0f;
    }

    if (!m_enemyDir.IsZero())
    {
      // don't get too close or behind the enemy

			Vec3 enemyToObstacle = m_to - pos;
			Vec3 HiderToObstacle = pos - m_from;
			float distObstacleHider = HiderToObstacle.GetLength();
			float distEnemyObstacle = enemyToObstacle.GetLength();
			// check if obstacle is behind enemy
			if(distObstacleHider > m_initialDistance && m_enemyDir.GetNormalizedSafe().Dot(HiderToObstacle.GetNormalizedSafe())>0)
				return -1;
			
			if(distObstacleHider > distEnemyObstacle)
				runDistance *= 1.5f + 2*(distObstacleHider - distEnemyObstacle)/m_initialDistance;
      // dislike running with enemy on his back!
//      if (runDir.Dot(m_enemyDir) < 0.5f)
//        runDistance *= 2.0f;
    }

    float rnd = m_bRandom ? 0.8f + 0.4f * float(ai_rand()) / 32767.0f : 1.0f;
    return rnd * (runDistance - benefit - closerToLeader);
  }
};

#if defined(__GNUC__)
// Proto required for GCC.
CAISystem *GetAISystem();
#endif

template < class FCost = CObstacleCostFtor >
class CIncrementalObstacleFinder
{
  typedef std::set< GraphNode* > SetNodes;
  typedef std::set< unsigned > SetNodeIds;
  typedef std::list< GraphNode* > ListNodes;
  typedef std::list< unsigned > ListNodeIds;
  typedef std::pair< float, CObstacleRef >	PairCostObstacle;
  typedef std::set< PairCostObstacle > SetObstacles;
  typedef std::map< CObstacleRef, float > MapObstacles;

  SetObstacles	m_candidateObstacles;	// Contains next obstacles sorted by cost
  MapObstacles	m_passedObstacles;		// Contains passed obstacles and their costs

  ListNodeIds		m_candidateNodes;		// Contains nodes that should be processed next
  SetNodeIds		m_passedNodes;			// Contains processed nodes
	CGraphLinkManager* m_pLinkManager;
	CGraphNodeManager* m_pNodeManager;

  const FCost&	m_costFtor;				// Cost functor

  SetObstacles	m_candidateDynamic;		// Contains next dynamic obstacles sorted by cost

  IAISystem::ENavigationType m_navType;	// navigation type at starting node

public:
  bool			m_bKnownEnemyPosition;
  bool			m_bCollidableOnly;

  CIncrementalObstacleFinder(const FCost& costFtor,bool bCollidableOnly=true)
    : m_costFtor(costFtor),m_bCollidableOnly(bCollidableOnly)
  {
    int nBuilding;
    IVisArea *pArea;
    // TODO: Use correct navigation cap! [Mikko]
    m_navType = GetAISystem()->CheckNavigationType(m_costFtor.GetStartingPosition(),nBuilding,pArea,IAISystem::NAVMASK_SURFACE);
    if ( m_navType == IAISystem::NAV_TRIANGULAR )
    {
			m_pLinkManager = &((CGraph*)GetAISystem()->GetHideGraph())->GetLinkManager();
			m_pNodeManager = &((CGraph*)GetAISystem()->GetHideGraph())->GetNodeManager();
      unsigned nodeIndex = GetAISystem()->GetHideGraph()->GetEnclosing(m_costFtor.GetStartingPosition(), IAISystem::NAVMASK_SURFACE);
      if (nodeIndex)
        m_candidateNodes.push_back(nodeIndex);
    }
    else if ( m_navType & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE) )
    {
      // use navigation graph (not hide graph) for indoors!!!
			m_pLinkManager = &((CGraph*)GetAISystem()->GetGraph())->GetLinkManager();
			m_pNodeManager = &((CGraph*)GetAISystem()->GetGraph())->GetNodeManager();
      unsigned nodeIndex = GetAISystem()->GetGraph()->GetEnclosing(m_costFtor.GetStartingPosition(), IAISystem::NAVMASK_SURFACE);
      if (nodeIndex)
        m_candidateNodes.push_back(nodeIndex);
    }

    AIObjects::iterator it = GetAISystem()->m_Objects.find(AIANCHOR_COMBAT_HIDESPOT), itEnd = GetAISystem()->m_Objects.end();
    while (it != itEnd && it->second->GetType() == AIANCHOR_COMBAT_HIDESPOT)
    {
      CAIObject* pAnchor = it->second;
      if (pAnchor->IsEnabled())
      {
        Vec3 pos = pAnchor->GetPos();

        if ((pos - costFtor.GetStartingPosition()).GetLengthSquared() <= costFtor.GetSearchDistanceSquared())
				{
					float cost = costFtor(pos);
					if(cost>=0)
						m_candidateDynamic.insert( PairCostObstacle(cost, CObstacleRef(pAnchor)) );
				}
      }
      ++it;
    }
  }

  CObstacleRef GetNextObstacle()
  {
    CObstacleRef obstacle;
    while (!obstacle)
    {
      if (m_candidateObstacles.empty() && !PassNextCandidate() && m_candidateDynamic.empty())
        return CObstacleRef();

      SetObstacles::iterator first = m_candidateObstacles.begin();
      SetObstacles::iterator firstDynamic = m_candidateDynamic.begin();
      float cost = first != m_candidateObstacles.end() ? first->first : -1;
      float costDynamic = firstDynamic != m_candidateDynamic.end() ? firstDynamic->first : -1;


      if (first != m_candidateObstacles.end() && (cost <= costDynamic || firstDynamic == m_candidateDynamic.end()))
      {
        obstacle = first->second;
        m_candidateObstacles.erase(first);
        m_passedObstacles[obstacle] = cost;
      }
      else if (firstDynamic != m_candidateDynamic.end())
      {
        obstacle = firstDynamic->second;
        m_candidateDynamic.erase(firstDynamic);
        m_passedObstacles[obstacle] = costDynamic;
      }
      else
        AIAssert(0);

      if (m_bCollidableOnly && m_bKnownEnemyPosition && (obstacle.GetAnchor() || obstacle.GetNode()))
      {
        // ignore the spot if the enemy is outside of hidespot's cone
        const Vec3& enemyPos = m_costFtor.GetEnemyPosition();
        const Vec3 hidePos = obstacle.GetPos();
        const Vec3& hideDir = obstacle.GetAnchor() ? obstacle.GetAnchor()->GetBodyDir() : obstacle.GetNode()->GetWaypointNavData()->dir;
        Vec3 enemyDir = (enemyPos - hidePos).GetNormalized();
        if (hideDir.Dot( enemyDir ) < HIDESPOT_COVERAGE_ANGLE_COS ||
          obstacle.GetAnchor() && GetAISystem()->CheckPointsVisibility( hidePos, enemyPos, 5.0f )) // extra check for anchors
            obstacle = CObstacleRef(); // this invalidates the current obstacle
      }
    }
    return obstacle;
  }

protected:
  bool PassNextCandidate()
  { 
    int found = 0;
    while (!m_candidateNodes.empty())
    {
			unsigned int nodeIndex = m_candidateNodes.front();
      GraphNode* node = m_pNodeManager->GetNode(nodeIndex);
      m_candidateNodes.pop_front();

      if (m_passedNodes.insert(nodeIndex).second) 
      {
        bool inRange = false;

        if (m_navType == IAISystem::NAV_TRIANGULAR && node->navType == IAISystem::NAV_TRIANGULAR)
        {
          ObstacleIndexVector::iterator itObstacles, itObstaclesEnd = node->GetTriangularNavData()->vertices.end();
          for (itObstacles = node->GetTriangularNavData()->vertices.begin(); itObstacles != itObstaclesEnd; ++itObstacles)
          {
            int index = *itObstacles;
            ObstacleData obstacle = GetAISystem()->GetObstacle(index);

            const Vec3& pos = obstacle.vPos;
            CGraph* pGraph = (CGraph*) GetAISystem()->GetHideGraph();
            if (pGraph->InsideOfBBox(pos))
            {
              float distance2 = (pos - m_costFtor.GetStartingPosition()).GetLengthSquared();
              if (distance2 <= m_costFtor.GetSearchDistanceSquared())
              {
                inRange = true;

                if(obstacle.bCollidable || !m_bCollidableOnly)
                {
                  if (m_passedObstacles.find(index) == m_passedObstacles.end())
                  {
                    float cost = m_costFtor(pos);
                    if (m_candidateObstacles.insert( PairCostObstacle(cost, index) ).second)
                    {
                      ++found;
                      m_passedObstacles[index] = cost;
                    }
                  }
                }
              }
            }
          }
        }
        else if (m_navType & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE))
        {
          const Vec3& pos = node->GetPos();
          float distance2 = (pos - m_costFtor.GetStartingPosition()).GetLengthSquared();
          if (distance2 <= m_costFtor.GetSearchDistanceSquared())
          {
            inRange = true;
            if (node->GetWaypointNavData()->type == WNT_HIDE)
            {

              if (m_passedObstacles.find(CObstacleRef(nodeIndex, node)) == m_passedObstacles.end())
              {
                float cost = m_costFtor(pos);
                if (m_candidateObstacles.insert( PairCostObstacle(cost, CObstacleRef(nodeIndex, node)) ).second)
                {
                  ++found;
                  m_passedObstacles[CObstacleRef(nodeIndex, node)] = cost;
                }
              }
            }
          }
        }

        if (found >= 20)
          return true;

        if (inRange)
        {
          for (unsigned link = node->firstLinkIndex; link; link = m_pLinkManager->GetNextLink(link))
          {
            unsigned candidate = m_pLinkManager->GetNextNode(link);
            if (candidate && m_pNodeManager->GetNode(candidate)->navType == m_navType)
              m_candidateNodes.push_back(candidate);
          }
        }
      }
    }
    return found != 0;
  }
};



#endif __ObstacleAllocator_H__
