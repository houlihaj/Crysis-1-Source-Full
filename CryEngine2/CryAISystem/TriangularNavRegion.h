#ifndef TRIANGULAR_NAVREGION_H
#define TRIANGULAR_NAVREGION_H

#if _MSC_VER > 1000
#pragma once
#endif

#include "NavRegion.h"

class CGraph;

/// Handles all graph operations that relate to the outdoor/triangulation aspect
class CTriangularNavRegion : public CNavRegion
{
public:
  CTriangularNavRegion(CGraph* pGraph);
  virtual ~CTriangularNavRegion() {}

  /// inherited
  virtual void BeautifyPath(
    const VectorConstNodeIndices& inPath, TPathPoints& outPath, 
    const Vec3& startPos, const Vec3& startDir, 
    const Vec3& endPos, const Vec3 & endDir,
    float radius,
    const AgentMovementAbility & movementAbility,
    const NavigationBlockers& navigationBlockers);

  /// inherited
  virtual void UglifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath, 
    const Vec3& startPos, const Vec3& startDir, 
    const Vec3& endPos, const Vec3 & endDir);

  /// inherited
  virtual unsigned GetEnclosing(const Vec3 &pos, float passRadius = 0.0f, unsigned startIndex = 0, 
    float range = 0.0f, Vec3 * closestValid = 0, bool returnSuspect = false, const char *requesterName = "");

  /// inherited
  virtual bool CheckPassability(const Vec3& from, const Vec3& to, float radius, const NavigationBlockers& navigationBlockers) const;

  virtual bool GetSingleNodePath(const GraphNode *pNode, const Vec3 &startPos, const Vec3 &endPos, float radius, 
    const NavigationBlockers &navigationBlockers, std::vector<PathPointDescriptor> &points) const
  {
    points.push_back(PathPointDescriptor(pNode->navType, startPos));
    points.push_back(PathPointDescriptor(pNode->navType, endPos));
    return true;
  }

  /// Serialise the _modifications_ since load-time
  virtual void Serialize(TSerialize ser, class CObjectTracker& objectTracker);

  /// inherited
  virtual void Clear();

  /// inherited
  virtual bool GetTeleportPosition(const Vec3 &curPos, Vec3 &teleportPos, const char *requesterName);

  /// inherited
  virtual void Update(class CCalculationStopper& stopper);

  /// inherited
  virtual void Reset(IAISystem::EResetReason reason);

  /// inherited
  virtual float AttemptStraightPath(TPathPoints &straightPathOut,
    const CHeuristic* pHeuristic, const Vec3& start, const Vec3& end,
    unsigned startHintIndex,
    float maxCost, const NavigationBlockers& navigationBlockers, bool returnPartialPath);

  /// inherited
  size_t MemStats();

  /// Should be called during triangulation to block out triangles that
  /// are occluded by obstacles in dynamic nav regions
  void DisableOccludedDynamicTriangles();

  void DebugDrawPathLinks(IRenderer *pRenderer);

private:
  // when it works this should be quicker than GreedyStep2... but it sometimes fails us
  unsigned GreedyStep1(unsigned beginIndex, const Vec3 & pos);
  unsigned GreedyStep2(unsigned beginIndex, const Vec3 & pos);

  /// If the point lies inside the obstacles at the triangle corners then it gets moved
  /// back into the triangle body
  Vec3 GetGoodPositionInTriangle(const GraphNode *pNode, const Vec3 &pos) const;

  bool IsNodeOccluded(const GraphNode *pNode, EAICollisionEntities collisionEntities, float triScale) const;
  void UpdateDynamicNode(GraphNode *pNode, EAICollisionEntities collisionEntities, bool modifyOnly, float triScale);

  CGraph * m_pGraph;

  std::vector<GraphNode *> m_graphNodesInDynamicAreas;
  bool m_calculateGraphNodesInDynamicAreas;
  unsigned m_currentGraphNodeIndex;
};

#endif
