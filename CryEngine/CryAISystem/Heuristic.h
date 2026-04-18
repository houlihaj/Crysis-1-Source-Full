// Heuristic.h: interface for the CHeuristic class.
//
//////////////////////////////////////////////////////////////////////

#ifndef HEURISTIC_H
#define HEURISTIC_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "AStarSolver.h"

class CGraph;
struct GraphNode;

struct CPathfinderRequesterProperties
{
  CPathfinderRequesterProperties(float radius, const AgentPathfindingProperties& properties) : radius(radius), properties(properties) {}
  CPathfinderRequesterProperties() {};
  float radius;
  AgentPathfindingProperties properties;
};

/// Just uses distance
class CStandardHeuristic : public CHeuristic
{
public:
  struct SExtraConstraint
  {
    enum EExtraConstraintType {ECT_MAXCOST, ECT_MINDISTFROMPOINT, ECT_AVOIDSPHERE, ECT_AVOIDCAPSULE};
    EExtraConstraintType type;
    union UConstraint
    {
      struct SConstraintMaxCost
      {
        float maxCost;
      };
      struct SConstraintMinDistFromPoint
      {
        float px, py, pz; // can't use Vec3 as it has a constructor
        float minDistSq;
      };
			struct SConstraintAvoidSphere
			{
				float px, py, pz; // can't use Vec3 as it has a constructor
				float minDistSq;
			};
			struct SConstraintAvoidCapsule
			{
				float px, py, pz;
				float qx, qy, qz;
				float minDistSq;
			};
      SConstraintMaxCost maxCost;
      SConstraintMinDistFromPoint minDistFromPoint;
			SConstraintAvoidSphere avoidSphere;
			SConstraintAvoidCapsule avoidCapsule;
    };
    UConstraint constraint;
  };
  typedef std::vector<SExtraConstraint> ExtraConstraints;

  CStandardHeuristic();

  /// Allow using a different position than the node position for start/end
  void SetStartEndData(const GraphNode *pStartNode, const Vec3 &startPos, const GraphNode *pEndNode, const Vec3 &endPos, const ExtraConstraints &extraConstraints);

  void ClearConstraints() {m_extraConstraints.resize(0);}

  virtual float EstimateCost(const GraphNode & node0, const GraphNode & node1) const;
  virtual float CalculateCost(const CGraph * graph,
    const GraphNode & node0, unsigned linkIndex0,
    const GraphNode & node1,
    const NavigationBlockers& navigationBlockers) const;
  virtual const CPathfinderRequesterProperties* GetRequesterProperties() const {return &m_reqProperties;}
  void SetRequesterProperties(const CPathfinderRequesterProperties& reqProperties, const class CAIObject *pRequester);

private:
  /// returns either the node position, or the start/end position if it's a start/end node (from the requester)
  const Vec3 &GetNodePos(const GraphNode &node) const;

  CPathfinderRequesterProperties m_reqProperties;
  /// cache the minimum of all our extra cost factors
  float m_fMinExtraFactor;
  const class CAIObject *m_pRequester;

  const GraphNode *m_pStartNode, *m_pEndNode;
  Vec3 m_startPos, m_endPos;

  ExtraConstraints m_extraConstraints;
};

#endif