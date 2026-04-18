#ifndef WAYPOINT_3DSURFACE_NAVREGION_H
#define WAYPOINT_3DSURFACE_NAVREGION_H

#if _MSC_VER > 1000
#pragma once
#endif

#include "NavRegion.h"

class CGraph;

/// Handles all graph operations that relate to the indoor/waypoint aspect
class CWaypoint3DSurfaceNavRegion : public CNavRegion
{
public:
	CWaypoint3DSurfaceNavRegion(CGraph* pGraph);
	virtual ~CWaypoint3DSurfaceNavRegion();

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

  /// Serialise the _modifications_ since load-time
	virtual void Serialize(TSerialize ser, class CObjectTracker& objectTracker);

	/// inherited
	virtual void Clear();

	/// inherited
	virtual size_t MemStats();

  /// inherited
  virtual void NodeCreated(unsigned nodeIndex);

  /// inherited
  virtual void NodeMoved(unsigned nodeIndex);

  /// inherited
  virtual void OnMissionLoaded();

  /// Returns the best node that is within sa for the position. May return 0 (even if pos is
  /// within the region)
  unsigned GetBestNodeForPosInInRegion(const Vec3 &pos, const struct SpecialArea &sa);

  /// Just needs to write the external links (e.g. to volume nav) to file
  bool WriteToFile(const char *pName);

  /// Just needs to read the external lins (e.g. to volume nav) to file
  bool ReadFromFile(const char *pName);

private:
  /// Makes sure the node is connected to volume, flight etc nodes
  void UpdateExternalLinks(unsigned nodeIndex);

  /// Removes external links from the node
  void ClearNode(unsigned nodeIndex);

	CGraph* m_pGraph;
};

#endif