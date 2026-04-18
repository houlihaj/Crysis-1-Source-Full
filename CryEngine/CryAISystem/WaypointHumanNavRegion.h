#ifndef WAYPOINT_NAVREGION_H
#define WAYPOINT_NAVREGION_H

#if _MSC_VER > 1000
#pragma once
#endif

#include "NavRegion.h"
#include "AICollision.h"
#include <set>

class CGraph;
class CCalculationStopper;

#ifdef STATUS_PENDING
#undef STATUS_PENDING
#endif

/// Handles all graph operations that relate to the indoor/waypoint aspect
class CWaypointHumanNavRegion : public CNavRegion
{
public:
  CWaypointHumanNavRegion(CGraph* pGraph);
  virtual ~CWaypointHumanNavRegion();

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

  /// inherited
  virtual bool GetTeleportPosition(const Vec3 &curPos, Vec3 &teleportPos, const char *requesterName);

  /// inherited
  virtual void Update(CCalculationStopper& stopper);

  /// Serialise the _modifications_ since load-time
  virtual void Serialize(TSerialize ser, class CObjectTracker& objectTracker);

  /// inherited
  virtual void Clear();

  /// inherited
  virtual void Reset(IAISystem::EResetReason reason);

  /// inherited
  virtual size_t MemStats();

  // Functions specific to this navigation region
  unsigned GetClosestNode(const Vec3 &pos, int nBuildingID);

  /// As IGraph. If flushQueue then the pending update queue is processed.
  /// If false then this reconnection request gets queued
  void ReconnectWaypointNodesInBuilding(int nBuildingID);

  /// Removes all automatically generated links (not affecting
  /// hand-generated links)
  void RemoveAutoLinksInBuilding(int nBuildingID);

  /// Indicates if it would be possible to walk (using sensible default params)
  /// between the two positions. the last safe foot position is also returned.
  /// This is done using physical checking, nothing to do with waypoints etc
  /// Also checks that it doesn't cross the boundary passed in
  bool CheckPassability(const Vec3& from, const Vec3& to, const ListPositions& boundary, Vec3& lastSafePos);

  /// Finds the floor position then checks if a body will fit there, using the passability
  /// parameters. If pRenderer != 0 then the body gets drawn (green if it's OK, red if not)
  bool CheckAndDrawBody(const Vec3& pos, IRenderer *pRenderer);

  // should be called when anything happens that might upset the dynamic updates (e.g. node created/destroyed)
  void ResetUpdateStatus();

  //void UpdateLinkPassability(const CGraph * graph, unsigned node1, unsigned linkIndex0, unsigned node2, CCalculationStopper& stopper);

	//void CheckLinkLengths() const;

  // TODO Mai 21, 2007: <pvl> if there are links connecting two waypoints
  // in both directions, this iterator currently returns only one of them,
  // picked up at random essentially.  This means that it's only useful
  // for retrieving unidirectional data (that are the same in both directions).
  // It would be fairly easy make the iterator configurable so it can
  // optionally return both directions for any two given nodes.

  /// Iterates over all links in a graph that connect two human waypoints.
  class LinkIterator {
    const CGraph * m_pGraph;
    std::auto_ptr <CAllNodesContainer::Iterator> m_currNodeIt;
    unsigned int m_currLinkIndex;

    void FindNextWaypointLink();
  public:
    LinkIterator (const CWaypointHumanNavRegion * );
    LinkIterator (const LinkIterator & );

    operator bool () const { return m_currLinkIndex	!= 0; }
    LinkIterator & operator++ ();
    unsigned int operator* () const { return m_currLinkIndex; }
  };
  // TODO Mai 21, 2007: <pvl> doesn't really need to be a friend, just needs to access
  // region's m_pGraph member ...
  friend class LinkIterator;

  LinkIterator CreateLinkIterator () const { return LinkIterator (this); }

private:
  void FillGreedyMap(unsigned nodeIndex, const Vec3 &pos, 
    IVisArea *pTargetArea, bool bStayInArea, CandidateIdMap& candidates);

  /// Returns true if the link between two nodes would intersect the boundary (in 2D)
  bool WouldLinkIntersectBoundary(const Vec3& pos1, const Vec3& pos2, const ListPositions& boundary) const;

  /// just disconnects automatically created links
  void RemoveAutoLinks(unsigned nodeIndex);

  /// Connect all nodes - the two cache lists can overlap.
  /// Links are connected in order of increasing distance. Linking continues
  /// until (1) dist > maxDistNormal and either (2.1) a node has > nLinksForMaxDistMax
  /// or (2.2) dist > maxDistMax
  void ConnectNodes(const struct SpecialArea *pArea, EAICollisionEntities collisionEntities, float maxDistNormal, float maxDistMax, unsigned nLinksForMaxDistMax);

  /// First gets obtains the links from the graph, then checks each one
  /// to see if the pass radius should be modified (made +ve or -ve)
  /// Only links that might be interesting (e.g. near player etc) get checked.
  void ModifyConnections(const CCalculationStopper& stopper, EAICollisionEntities collisionEntities);

  /// Modify all connections within an area
  void ModifyConnections(const struct SpecialArea *pArea, EAICollisionEntities collisionEntities, bool adjustOriginalEditorLinks);

  /// checks and modifies a single connection coming from a node. 
  void ModifyNodeConnections(unsigned nodeIndex, EAICollisionEntities collisionEntities, unsigned iLink, bool adjustOriginalEditorLinks);

	/// Checks walkability using a faster algorithm than CheckWalkability() if possible.
	bool CheckPassability(unsigned linkIndex, Vec3 from, Vec3 to,
		float paddingRadius, bool checkStart,
		const ListPositions& boundary, 
		EAICollisionEntities aiCollisionEntities, 
		SCachedPassabilityResult *pCachedResult,
		SCheckWalkabilityState *pCheckWalkabilityState);

	/// Finds out if a link is eligible for simplified passability check.  The
	/// first return value says if the link is passable at all.  If not, the second
	/// one is not meaningful, otherwise it indicates if the link is also "simple".
	/// Note that all of the above is only valid if pCheckWalkabilityState->state
	/// is RESULT on return - otherwise the computation hasn't completed yet
	/// and passability information isn't available at all in the first place.
	std::pair<bool,bool> CheckIfLinkSimple(Vec3 from, Vec3 to,
		float paddingRadius, bool checkStart,
		const ListPositions& boundary, 
		EAICollisionEntities aiCollisionEntities, 
		SCheckWalkabilityState *pCheckWalkabilityState);

  /// Iterator over all nodes for distributing the cxn modifications over multiple updates
  CAllNodesContainer::Iterator *m_currentNodeIt;

  /// link counter for distributing the cxn modifications over multiple updates
  unsigned m_currentLink;

  /// Used for distributing walkability checks over multiple updates
  SCheckWalkabilityState m_checkWalkabilityState;

  CGraph* m_pGraph;
};

#endif
