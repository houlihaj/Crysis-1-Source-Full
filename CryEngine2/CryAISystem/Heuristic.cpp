// Heuristic.cpp: implementation of the CHeuristic class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "IAgent.h"
#include "Heuristic.h"
#include "Graph.h"
#include "AIObject.h"
#include "Cry_Math.h"
#include "AILog.h"
#include "CAISystem.h"
#include "VolumeNavRegion.h"

#include <limits>

/// For debugging - over-ride all the complicated stuff and just use pure distance
static bool gUseSimpleHeuristic = false;

//====================================================================
// CStandardHeuristic
//====================================================================
CStandardHeuristic::CStandardHeuristic()
{
  m_reqProperties.radius = 0.0f;
  m_reqProperties.properties.SetToDefaults();

  SetRequesterProperties(m_reqProperties, 0);
  SetStartEndData(0, ZERO, 0, ZERO, ExtraConstraints());
}

//====================================================================
// SetStartEndData
//====================================================================
void CStandardHeuristic::SetStartEndData(const GraphNode *pStartNode, const Vec3 &startPos, const GraphNode *pEndNode, const Vec3 &endPos, const ExtraConstraints &extraConstraints)
{
  m_pStartNode = pStartNode;
  m_pEndNode = pEndNode;
  m_startPos = startPos;
  m_endPos = endPos;
  m_extraConstraints = extraConstraints;
}


//====================================================================
// GetNodePos
//====================================================================
const Vec3 &CStandardHeuristic::GetNodePos(const GraphNode &node) const
{
  if (&node == m_pStartNode)
    return m_startPos;
  else if (&node == m_pEndNode)
    return m_endPos;
  else return node.GetPos();
}


//====================================================================
// SetRequesterProperties
//====================================================================
void CStandardHeuristic::SetRequesterProperties(const CPathfinderRequesterProperties& reqProperties, const CAIObject *pRequester) 
{
  m_pRequester = pRequester;

  m_reqProperties = reqProperties;

  m_fMinExtraFactor = std::numeric_limits<float>::max();
  if ( (reqProperties.properties.navCapMask & IAISystem::NAV_TRIANGULAR) &&
    reqProperties.properties.triangularResistanceFactor < m_fMinExtraFactor)
    m_fMinExtraFactor = reqProperties.properties.triangularResistanceFactor;

  if ( (reqProperties.properties.navCapMask & IAISystem::NAV_WAYPOINT_HUMAN) &&
    reqProperties.properties.waypointResistanceFactor < m_fMinExtraFactor)
    m_fMinExtraFactor = reqProperties.properties.waypointResistanceFactor;

  if ( (reqProperties.properties.navCapMask & IAISystem::NAV_WAYPOINT_3DSURFACE) &&
    reqProperties.properties.waypointResistanceFactor < m_fMinExtraFactor)
    m_fMinExtraFactor = reqProperties.properties.waypointResistanceFactor;

  if ( (reqProperties.properties.navCapMask & IAISystem::NAV_FLIGHT) &&
    reqProperties.properties.flightResistanceFactor < m_fMinExtraFactor)
    m_fMinExtraFactor = reqProperties.properties.flightResistanceFactor;

  if ( (reqProperties.properties.navCapMask & IAISystem::NAV_VOLUME) &&
    reqProperties.properties.volumeResistanceFactor < m_fMinExtraFactor)
    m_fMinExtraFactor = reqProperties.properties.volumeResistanceFactor;

  if ( (reqProperties.properties.navCapMask & IAISystem::NAV_ROAD) &&
    reqProperties.properties.roadResistanceFactor < m_fMinExtraFactor)
    m_fMinExtraFactor = reqProperties.properties.roadResistanceFactor;

}

//====================================================================
// EstimateCost
//====================================================================
float CStandardHeuristic::EstimateCost(const GraphNode & node0, const GraphNode & node1) const
{
  if (gUseSimpleHeuristic)
  {
    return Distance::Point_Point(GetNodePos(node1), GetNodePos(node0));
  }
  else
  {
    Vec3 delta = (GetNodePos(node1) - GetNodePos(node0));
    delta.z *= m_reqProperties.properties.zScale;
    return (1.0f + m_fMinExtraFactor) * delta.GetLength();
  }
}

//====================================================================
// CalculateCost
//====================================================================
float CStandardHeuristic::CalculateCost(const CGraph * graph,
                                        const GraphNode & node0, unsigned linkIndex0,
                                        const GraphNode & node1,
                                        const NavigationBlockers& navigationBlockers) const
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

	static int maxBlockers = 0;
	if (navigationBlockers.size() > maxBlockers)
	{
		maxBlockers = navigationBlockers.size();
		if (maxBlockers > 10)
			AIWarning ("Too many (%d) navigation blockers passed to CalculateCost()", maxBlockers);
	}

  unsigned link = linkIndex0;
  if (graph->GetLinkManager().GetRadius(link) < m_reqProperties.radius)
    return -1.0f;

  if (!(node1.navType & m_reqProperties.properties.navCapMask))
    return -1.0f;

  if (gUseSimpleHeuristic)
    return (GetNodePos(node1) - GetNodePos(node0)).GetLength();

  if (graph->GetLinkManager().GetMaxWaterDepth(link) > m_reqProperties.properties.maxWaterDepth)
    return -1.0f;
  if (graph->GetLinkManager().GetMinWaterDepth(link) < m_reqProperties.properties.minWaterDepth)
    return -1.0f;

  for (unsigned iExtraConstraint = 0 ; iExtraConstraint < m_extraConstraints.size() ; ++iExtraConstraint)
  {
    const SExtraConstraint &extraConstraint = m_extraConstraints[iExtraConstraint];
    switch (extraConstraint.type)
    {
    case SExtraConstraint::ECT_MAXCOST:
      {
        if (extraConstraint.constraint.maxCost.maxCost < node0.fCostFromStart)
          return -1.0f;
      }
      break;
    case SExtraConstraint::ECT_MINDISTFROMPOINT:
      {
        Vec3 point(extraConstraint.constraint.minDistFromPoint.px,
          extraConstraint.constraint.minDistFromPoint.py,
          extraConstraint.constraint.minDistFromPoint.pz);
        float distToPointSq = Distance::Point_PointSq(point, GetNodePos(node0));
        if (distToPointSq > extraConstraint.constraint.minDistFromPoint.minDistSq)
          return -1.0f;
      }
      break;
		case SExtraConstraint::ECT_AVOIDSPHERE:
			{
				Vec3 p(extraConstraint.constraint.avoidSphere.px,
					extraConstraint.constraint.avoidSphere.py,
					extraConstraint.constraint.avoidSphere.pz);
				Lineseg lineseg(GetNodePos(node0), GetNodePos(node1));
				float distToPointSq;
				float t;
				if (node0.navType == IAISystem::NAV_TRIANGULAR)
					distToPointSq = Distance::Point_Lineseg2DSq(p, lineseg, t);
				else
					distToPointSq = Distance::Point_LinesegSq(p, lineseg, t);
				if (distToPointSq < extraConstraint.constraint.avoidSphere.minDistSq)
					return -1.0f;
			}
			break;
		case SExtraConstraint::ECT_AVOIDCAPSULE:
			{
				Vec3 p(extraConstraint.constraint.avoidCapsule.px,
					extraConstraint.constraint.avoidCapsule.py,
					extraConstraint.constraint.avoidCapsule.pz);
				Vec3 q(extraConstraint.constraint.avoidCapsule.qx,
					extraConstraint.constraint.avoidCapsule.qy,
					extraConstraint.constraint.avoidCapsule.qz);

				Lineseg lineseg(GetNodePos(node0), GetNodePos(node1));

				float distToLineSegSq;
				if (node0.navType == IAISystem::NAV_TRIANGULAR)
				{
					p.z = 0;
					q.z = 0;
					lineseg.start.z = 0;
					lineseg.end.z = 0;
				}
				distToLineSegSq = Distance::Lineseg_LinesegSq(lineseg, Lineseg(p, q), (float*)0, (float*)0);
				if (distToLineSegSq < extraConstraint.constraint.avoidCapsule.minDistSq)
					return -1.0f;
			}
			break;
		default:
      AIWarning("Unhandled constraint type %d", extraConstraint.type);
      break;
    }
  }

  Vec3 delta = (GetNodePos(node1) - GetNodePos(node0));

  // TODO Danny should these really be 0?! It overrides the params in the lua files, and
  // also maybe invalidates the calculation in EstimateCost
  static float fResistanceFactorTriangularNoWater = 0.5f;
  static float fResistanceFactorTriangularWater = 1.0f;
  static float fResistanceFactorWaypoint = 0.5f;
  static float fResistanceFactorFlight = 0.0f;
  static float fResistanceFactorVolume = 0.0f;
  static float fResistanceFactorRoad = 0.0f;
  static float fResistanceFactorSmartObject = 0.0f;

  const GraphNode* nodes[2] = {&node0, &node1};
  float resistanceFactor[2] = {0.0f, 0.0f};
  for (unsigned i = 0 ; i < 2 ; ++i)
  {
    const GraphNode* node = nodes[i];
    switch (node->navType)
    {
    case IAISystem::NAV_TRIANGULAR: 
      resistanceFactor[i] = fResistanceFactorTriangularNoWater * m_reqProperties.properties.triangularResistanceFactor;
      if (graph->GetLinkManager().GetMaxWaterDepth(link) > 1.5f)
        resistanceFactor[i] += fResistanceFactorTriangularWater * (m_reqProperties.properties.triangularResistanceFactor + 2);
      else if (graph->GetLinkManager().GetMaxWaterDepth(link) > 0.0f)
        resistanceFactor[i] += fResistanceFactorTriangularWater * (m_reqProperties.properties.triangularResistanceFactor + 1);
      break;
    case IAISystem::NAV_WAYPOINT_HUMAN: 
      resistanceFactor[i] = fResistanceFactorWaypoint * m_reqProperties.properties.waypointResistanceFactor;
      break;
    case IAISystem::NAV_WAYPOINT_3DSURFACE: 
      resistanceFactor[i] = fResistanceFactorWaypoint * m_reqProperties.properties.waypointResistanceFactor;
      break;
    case IAISystem::NAV_FLIGHT: 
      resistanceFactor[i] = fResistanceFactorFlight * m_reqProperties.properties.flightResistanceFactor;
      break;
    case IAISystem::NAV_VOLUME: 
      resistanceFactor[i] = fResistanceFactorVolume * m_reqProperties.properties.volumeResistanceFactor;
      break;
    case IAISystem::NAV_ROAD: 
      resistanceFactor[i] = fResistanceFactorRoad * m_reqProperties.properties.roadResistanceFactor;
      break;
    case IAISystem::NAV_SMARTOBJECT: 
      resistanceFactor[i] = fResistanceFactorSmartObject * 1.0f;
      break;
    }
  }
  float resFactor = max(resistanceFactor[0], resistanceFactor[1]);

  int nBuildingID0 = nodes[0]->navType & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE) ? nodes[0]->GetWaypointNavData()->nBuildingID : -2;
  int nBuildingID1 = nodes[1]->navType & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE) ? nodes[1]->GetWaypointNavData()->nBuildingID : -2;

  static float deltaZ = 1.0f;
  float blockerCost = 0.0f;
  float blockerMult = 0.0f;

  if (nodes[0]->navType == IAISystem::NAV_SMARTOBJECT && nodes[1]->navType == IAISystem::NAV_SMARTOBJECT)
  {
    resFactor = GetAISystem()->GetSmartObjectLinkCostFactor(nodes, m_pRequester);
    if ( resFactor < 0 )
      return -1;
  }
  else if (nBuildingID0 >= 0 || nBuildingID1 >= 0)
  {
    // human or 3dsurface waypoint
    Lineseg lineseg(GetNodePos(node0), GetNodePos(node1));
    unsigned nBlockers = navigationBlockers.size();
    for (unsigned i = 0 ; i < nBlockers ; ++i)
    {
      const SNavigationBlocker& navBlocker = navigationBlockers[i];
      bool useBlocker = false;
      if (navBlocker.restrictedLocation)
      {
        if (!(navBlocker.navType & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE)))
          continue;
        if (navBlocker.location.waypoint.nBuildingID == nBuildingID0 ||
          navBlocker.location.waypoint.nBuildingID == nBuildingID1)
          useBlocker = true;
      }
      else
      {
        useBlocker = true;
      }
      if (useBlocker)
      {
        float t = 0.0f;
        float d = Distance::Point_LinesegSq(navBlocker.sphere.center, lineseg, t);
        Vec3 pos = lineseg.GetPoint(t);
        if (pos.z > navBlocker.sphere.center.z - deltaZ && pos.z < navBlocker.sphere.center.z + deltaZ)
        {
          d = navBlocker.navType == IAISystem::NAV_WAYPOINT_HUMAN ? Distance::Point_Point2DSq(pos, navBlocker.sphere.center) : Distance::Point_PointSq(pos, navBlocker.sphere.center);
          if (d < square(navBlocker.sphere.radius))
          {
            if (navBlocker.costMultMod < 0.0f)
              return -1.0f;
            float scale = navBlocker.radialDecay ? square(square(1.0f - cry_sqrtf(d) / navBlocker.sphere.radius)) : 1.0f;
            if (navBlocker.directional)
            {
              /// reduce costs that go away from point
              Vec3 segDir = (lineseg.end - lineseg.start).GetNormalizedSafe();
              Vec3 dirToBlocker = (navBlocker.sphere.center - 0.5f * (lineseg.start + lineseg.end)).GetNormalizedSafe();
              float dirScale = 1.0f - abs(segDir.Dot(dirToBlocker));
              scale *= dirScale;
            }
            blockerCost += scale * navBlocker.costAddMod * (1.0f +  m_reqProperties.properties.waypointResistanceFactor);
            blockerMult += scale * navBlocker.costMultMod;
          }
        }
      }
    }
  }
  else if (nodes[0]->navType == IAISystem::NAV_VOLUME || nodes[1]->navType == IAISystem::NAV_VOLUME)
  {
#ifdef DYNAMIC_3D_NAVIGATION
    // do a collision test to handle dynamic/moving obstacles. Assume that static geometry has already been 
    // accounted for so we just need to check dynamic objects (cheaper). The question is whether it's better
    // to do this here or in background processing. If here, then we should really cache the result.
    bool intersect = 
      GetAISystem()->GetVolumeNavRegion()->PathSegmentWorldIntersection(node0.GetPos(), link.vEdgeCenter, m_reqProperties.radius, false, true, AICE_DYNAMIC) ||
      GetAISystem()->GetVolumeNavRegion()->PathSegmentWorldIntersection(link.vEdgeCenter, node1.GetPos(), m_reqProperties.radius, false, true, AICE_DYNAMIC);
    if (intersect)
      return -1;
#endif

    Lineseg lineseg(GetNodePos(node0), GetNodePos(node1));
    unsigned nBlockers = navigationBlockers.size();
    for (unsigned i = 0 ; i < nBlockers ; ++i)
    {
      const SNavigationBlocker& navBlocker = navigationBlockers[i];
      bool useBlocker = false;
      if (navBlocker.restrictedLocation)
      {
        if (navBlocker.navType != IAISystem::NAV_VOLUME)
          continue;
      }
      else
      {
        useBlocker = true;
      }
      if (useBlocker)
      {
        float t = 0.0f;
        float d = Distance::Point_LinesegSq(navBlocker.sphere.center, lineseg, t);
        if (d < square(navBlocker.sphere.radius))
        {
          float scale = navBlocker.radialDecay ? square(square(1.0f - cry_sqrtf(d) / navBlocker.sphere.radius)) : 1.0f;
          if (navBlocker.directional)
          {
            /// reduce costs that go away from point
            Vec3 segDir = (lineseg.end - lineseg.start).GetNormalizedSafe();
            Vec3 dirToBlocker = (navBlocker.sphere.center - 0.5f * (lineseg.start + lineseg.end)).GetNormalizedSafe();
            float dirScale = 1.0f - abs(segDir.Dot(dirToBlocker));
            scale *= dirScale;
          }
          blockerCost += scale * navBlocker.costAddMod * (1.0f +  m_reqProperties.properties.volumeResistanceFactor);
          blockerMult += scale * navBlocker.costMultMod;
        }
      }
    }
  }
  else if (nodes[0]->navType == IAISystem::NAV_TRIANGULAR || nodes[1]->navType == IAISystem::NAV_TRIANGULAR)
  {
    Lineseg lineseg(GetNodePos(node0), GetNodePos(node1));
    unsigned nBlockers = navigationBlockers.size();
    for (unsigned i = 0 ; i < nBlockers ; ++i)
    {
      const SNavigationBlocker& navBlocker = navigationBlockers[i];
      if (!navBlocker.restrictedLocation)
      {
        float t = 0.0f;
        float d = Distance::Point_LinesegSq(navBlocker.sphere.center, lineseg, t);
        if (d < square(navBlocker.sphere.radius))
        {
          if (navBlocker.costMultMod < 0.0f)
            return -1.0f;
          float scale = navBlocker.radialDecay ? square(square(1.0f - cry_sqrtf(d) / navBlocker.sphere.radius)) : 1.0f;
          if (navBlocker.directional)
          {
            /// reduce costs that go away from point
            Vec3 segDir = (lineseg.end - lineseg.start).GetNormalizedSafe();
            Vec3 dirToBlocker = (navBlocker.sphere.center - 0.5f * (lineseg.start + lineseg.end)).GetNormalizedSafe();
            float dirScale = 1.0f - abs(segDir.Dot(dirToBlocker));
            scale *= dirScale;
          }
          blockerCost += scale * navBlocker.costAddMod * (1.0f +  m_reqProperties.properties.triangularResistanceFactor);
          blockerMult += scale * navBlocker.costMultMod;
        }
      }
    }
  }

  // increase slightly the cost of paths that deviate from the direct line - really just to bias the result where
  // there are multiple routes that are equivalent before beautification
  Lineseg line(m_startPos, m_endPos);
  float junk;
  float offsetSq = Distance::Point_LinesegSq(GetNodePos(node0), line, junk);
  static float maxTweak = 0.1f;
  static float maxTweakDistSq = square(50.0f);
  Limit(offsetSq, 0.0f, maxTweakDistSq);
  float offsetFactor = maxTweak * offsetSq / maxTweakDistSq;

	// TODO/HACK! Ideally the following call would call GetNodePos(node0) instead of using the nodepos directly.
	// The change here is to solve some specific problems in one of the Crysis levels where extra link cost modifiers
	// were used too much. If the start location is inside an extra link cost modifier no path could be found.
	// This is especially problem with the volume navigation.
	// The volume nav GetEnclosing() is changed so that it tries to find the node outside nav cost modifier.
  float extraLinkFactor = 0;
	if (nodes[0]->navType == IAISystem::NAV_VOLUME || nodes[1]->navType == IAISystem::NAV_VOLUME)
		extraLinkFactor = GetAISystem()->GetExtraLinkCost(node0.GetPos(), node1.GetPos());
	else
		extraLinkFactor = GetAISystem()->GetExtraLinkCost(GetNodePos(node0), GetNodePos(node1));
	// End hack.
  if (extraLinkFactor < 0.0f)
    return -1.0f;

  float exposureFactor = m_reqProperties.properties.exposureFactor * graph->GetLinkManager().GetExposure(link);
  delta.z *= m_reqProperties.properties.zScale;
  float length = delta.GetLength();
  float totalCost = blockerCost + (1.0f + resFactor + exposureFactor + extraLinkFactor + blockerMult + offsetFactor) * length;

  return totalCost;
}
