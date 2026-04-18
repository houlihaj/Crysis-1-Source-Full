#ifndef ROAD_NAVREGION_H
#define ROAD_NAVREGION_H

#if _MSC_VER > 1000
#pragma once
#endif

#include "NavRegion.h"

class CGraph;

struct SRoadNode
{
  SRoadNode() : nodeIndex(0), pos(0.0f, 0.0f, 0.0f), width(0.0f), offset(0.0f) {}
  SRoadNode(const Vec3& pos, float width, float offset) : nodeIndex(0), pos(pos), width(width), offset(offset) {}
  SRoadNode(unsigned nodeIndex, GraphNode* pNode) 
    : nodeIndex(nodeIndex)
  {
    pos = pNode->GetPos();
    width = pNode->GetRoadNavData()->fRoadWidth;
    offset = 0.0f/*node->GetRoadNavData()->fRoadOffset*/;
  }
	unsigned nodeIndex;
  Vec3 pos;
  float width;
  float offset;
};

/// Handles all graph operations that relate to the road aspect
class CRoadNavRegion : public CNavRegion
{
public:
  CRoadNavRegion(CGraph* pGraph);
  virtual ~CRoadNavRegion();

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

  /// Serialise the _modifications_ since load-time
  virtual void Serialize(TSerialize ser, class CObjectTracker& objectTracker);

  /// inherited
  virtual void Clear();

  /// inherited
  virtual void OnMissionLoaded();

  /// inherited
  virtual size_t MemStats();

  // Functions specific to this navigation region
  bool CreateRoad(const char* roadName, const std::vector<Vec3>& points, float roadWidth, float roadOffset);

  /// Delete the road (and unlink it) with this name - returns true/false to indicate if the road was found
  bool DeleteRoad(const char* roadName);

	bool DoesRoadExists(const char* roadName);

  /// Write out to file returns true on success
  bool WriteToFile(const char* roadsFileName);

  /// Reads from file (creates nodes etc) - returns true on success
  bool ReadFromFile(const char* roadsFileName);

  /// This removes all graph nodes and recreates them, also creating links from the road
  /// into the rest of the graph
  void ReinsertIntoGraph();

private:
  typedef std::vector<SRoadNode> tNodes;
  typedef std::map<string, tNodes> tRoads;

  /// Write a single road (the original list of road nodes)
  void WriteRoad(class CCryFile & file, const string & name, const tNodes& nodes) const;
  /// Read a single road
  void ReadRoad(class CCryFile & file, string & name, tNodes& nodes);

  /// returns points between p0 and p1 that lie in each triangle from the graph
  void GetSegments(const Vec3& p0, const Vec3& p1, std::vector<Vec3>& pts) const;

  /// This removes nodes for just the single road. Returns the next iterator from m_roads
  tRoads::iterator RemoveRoadNodes(const string& roadName);

  /// This removes nodes for just the single road
  void CreateRoadNodes(const string& roadName);

  CGraph* m_pGraph;

  /// When we get a road added, it goes into m_originalRoads. Then, everytime
  /// the roads get stitched into the graph m_roads is created by cutting
  /// m_originalRoads against the triangulation.
  tRoads m_originalRoads;
  /// The actual working roads. This is the one that contains actual graph node
  /// pointers
  tRoads m_roads;
};

#endif