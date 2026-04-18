#include "StdAfx.h"
#include "Waypoint3DSurfaceNavRegion.h"
#include "VolumeNavRegion.h"
#include "AutoTypeStructs.h"
#include "CAISystem.h"

#define BAI_WAYPT3DSFC_FILE_VERSION 1

//====================================================================
// CWaypoint3DSurfaceNavRegion
//====================================================================
CWaypoint3DSurfaceNavRegion::CWaypoint3DSurfaceNavRegion(CGraph* pGraph)
{
  AIAssert(pGraph);
  m_pGraph = pGraph;
}

//====================================================================
// ~CWaypoint3DSurfaceNavRegion
//====================================================================
CWaypoint3DSurfaceNavRegion::~CWaypoint3DSurfaceNavRegion()
{
}

//====================================================================
// UglifyPath
//====================================================================
void CWaypoint3DSurfaceNavRegion::UglifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath, 
                                             const Vec3& startPos, const Vec3& startDir, 
                                             const Vec3& endPos, const Vec3 & endDir)
{
  outPath.push_back(PathPointDescriptor(IAISystem::NAV_WAYPOINT_3DSURFACE, startPos));
  for(VectorConstNodeIndices::const_iterator itrCurNode=inPath.begin() ; itrCurNode != inPath.end() ; ++itrCurNode)
  {
    const GraphNode& curNode=*m_pGraph->GetNodeManager().GetNode(*itrCurNode);
    outPath.push_back(PathPointDescriptor(IAISystem::NAV_WAYPOINT_3DSURFACE, curNode.GetPos()));
  }
  outPath.push_back(PathPointDescriptor(IAISystem::NAV_WAYPOINT_3DSURFACE, endPos));
}

//====================================================================
// BeautifyPath
//====================================================================
void CWaypoint3DSurfaceNavRegion::BeautifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath, 
                                               const Vec3& startPos, const Vec3& startDir, 
                                               const Vec3& endPos, const Vec3 & endDir,
                                               float radius,
                                               const AgentMovementAbility & movementAbility,
                                               const NavigationBlockers& navigationBlockers)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );
  UglifyPath(inPath, outPath, startPos, startDir, endPos, endDir);
}


//====================================================================
// GetEnclosing
//====================================================================
unsigned CWaypoint3DSurfaceNavRegion::GetEnclosing(const Vec3 &pos, float passRadius, unsigned startIndex, 
                                                     float range, Vec3 * closestValid, bool returnSuspect, const char *requesterName)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  IVisArea *pGoalArea; 
  int	nBuildingID;
  GetAISystem()->CheckNavigationType(pos, nBuildingID, pGoalArea, IAISystem::NAV_WAYPOINT_3DSURFACE);

  if (nBuildingID < 0)
  {
    AIWarning("CWaypoint3DSurfaceNavRegion::GetEnclosing found bad building ID = %d (%s) for %s (%5.2f, %5.2f, %5.2f)", 
      nBuildingID, GetAISystem()->GetNavigationShapeName(nBuildingID), requesterName, pos.x, pos.y, pos.z);
    return 0;
  }

  const SpecialArea *sa = GetAISystem()->GetSpecialArea(nBuildingID);
  if (!sa)
  {
    AIWarning("CWaypoint3DSurfaceNavRegion::GetEnclosing found no area for %s (%5.2f, %5.2f, %5.2f)", 
      requesterName, pos.x, pos.y, pos.z);
    return 0;
  }

  return GetBestNodeForPosInInRegion(pos, *sa);
}

//====================================================================
// CheckPassability
//====================================================================
bool CWaypoint3DSurfaceNavRegion::CheckPassability(const Vec3& from, const Vec3& to, float radius, const NavigationBlockers& navigationBlockers) const
{
  return true;
}


//====================================================================
// Serialize
//====================================================================
void CWaypoint3DSurfaceNavRegion::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
  ser.BeginGroup("WaypointNavRegion");

  ser.EndGroup();
}

//====================================================================
// GetBestNodeForPosInInRegion
//====================================================================
unsigned CWaypoint3DSurfaceNavRegion::GetBestNodeForPosInInRegion(const Vec3 &pos, const struct SpecialArea &sa)
{
  typedef std::vector< std::pair<float, GraphNode*> > TNodes;
  static TNodes nodes;
  nodes.resize(0);

  // Get all the nodes that hav pos < fNodeAutoConnectDistance in their up dir
  // and sort by the "sideways" distance
  CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
  CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_WAYPOINT_3DSURFACE);
  unsigned bestNodeIndex = 0;
  unsigned ANodeIndex = 0;
  float bestDistSq = std::numeric_limits<float>::max();
  while (unsigned currentNodeIndex = it.Increment())
  {
		GraphNode* pCurrent = m_pGraph->GetNodeManager().GetNode(currentNodeIndex);

    if (pCurrent->GetWaypointNavData()->nBuildingID == -1)
      if (CAISystem::IsPointInSpecialArea(pCurrent->GetPos(), sa))
        pCurrent->GetWaypointNavData()->nBuildingID = sa.nBuildingID;

    if (pCurrent->GetWaypointNavData()->nBuildingID == sa.nBuildingID)
    {
      ANodeIndex = currentNodeIndex;

      Vec3 delta = pos - pCurrent->GetPos();
      const Vec3 &upDir = pCurrent->GetWaypointNavData()->up;

      float upDist = delta.Dot(upDir);
      if (upDist < sa.fNodeAutoConnectDistance && upDist > -sa.fNodeAutoConnectDistance)
      {
        float distSq = Distance::Point_PointSq(pos, pCurrent->GetPos() - upDist * upDir);
        if (distSq < bestDistSq)
        {
          bestDistSq = distSq;
          bestNodeIndex = currentNodeIndex;
        }
      }
    }
  }

  if (!ANodeIndex)
  {
    AIWarning("CWaypoint3DSurfaceNavRegion::GetBestNodeForPosInInRegion No nodes found for area %d (%s)containing point (%5.2f, %5.2f, %5.2f)", 
      sa.nBuildingID, GetAISystem()->GetNavigationShapeName(sa.nBuildingID), pos.x, pos.y, pos.z);
    return 0;
  }

  return bestNodeIndex;
}

//====================================================================
// NodeCreated
//====================================================================
void CWaypoint3DSurfaceNavRegion::NodeCreated(unsigned nodeIndex)
{
  UpdateExternalLinks(nodeIndex);
}

//====================================================================
// NodeMoved
//====================================================================
void CWaypoint3DSurfaceNavRegion::NodeMoved(unsigned nodeIndex)
{
  UpdateExternalLinks(nodeIndex);
}

//====================================================================
// ClearNode
//====================================================================
void CWaypoint3DSurfaceNavRegion::ClearNode(unsigned nodeIndex)
{
	GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);
  for (unsigned link = pNode->firstLinkIndex; link;)
  {
		unsigned nextLink = m_pGraph->GetLinkManager().GetNextLink(link);

    const GraphNode *pOther = m_pGraph->GetNodeManager().GetNode(m_pGraph->GetLinkManager().GetNextNode(link));
    AIAssert(pOther);
    if (pOther->navType & (IAISystem::NAV_FLIGHT | IAISystem::NAV_VOLUME))
    {
      m_pGraph->Disconnect(nodeIndex, link);
    }

		link = nextLink;
  }
}

//====================================================================
// Clear
//====================================================================
void CWaypoint3DSurfaceNavRegion::Clear()
{
  CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
  CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_WAYPOINT_3DSURFACE);
  while (unsigned currentIndex = it.Increment())
    ClearNode(currentIndex);
}

//====================================================================
// UpdateExternalLinks
//====================================================================
void CWaypoint3DSurfaceNavRegion::UpdateExternalLinks(unsigned nodeIndex)
{
	GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);
  // update our area/id
  pNode->GetWaypointNavData()->nBuildingID = -1;

  const SpecialAreaMap & areas = GetAISystem()->GetSpecialAreas();
  for (SpecialAreaMap::const_iterator it = areas.begin() ; it != areas.end() ; ++it)
  {
    const string &name = it->first;
    const SpecialArea &area = it->second;
    if (area.type == SpecialArea::TYPE_WAYPOINT_3DSURFACE)
    {
      if (GetAISystem()->IsPointInSpecialArea(pNode->GetPos(), area))
      {
        pNode->GetWaypointNavData()->nBuildingID = area.nBuildingID;
        break;
      }
    }
  }

  // first remove any existing links to flight/volume
  ClearNode(nodeIndex);

  unsigned volumeNodeIndex = m_pGraph->GetEnclosing(pNode->GetPos(), IAISystem::NAV_VOLUME);

  // connect one way - so the troopers can get back to the surface if pushed
  if (volumeNodeIndex)
    m_pGraph->Connect(nodeIndex, volumeNodeIndex, -1, 100);
}

//====================================================================
// OnMissionLoaded
//====================================================================
void CWaypoint3DSurfaceNavRegion::OnMissionLoaded()
{
  CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
  CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_WAYPOINT_3DSURFACE);
  while (unsigned currentIndex = it.Increment())
    UpdateExternalLinks(currentIndex);
}

//====================================================================
// WriteToFile
//====================================================================
bool CWaypoint3DSurfaceNavRegion::WriteToFile(const char *pName)
{
  AILogProgress("CWaypoint3DSurfaceNavRegion::WriteToFile %s", pName);

  OnMissionLoaded();

  CCryFile file;
  if( false != file.Open( pName, "wb" ) )
  {
    int fileVersion = BAI_WAYPT3DSFC_FILE_VERSION;
    file.Write(&fileVersion, sizeof(fileVersion));

    std::vector<unsigned> nodes;
    std::vector<SLinkRecord> linkedVolumeRecords;
    std::vector<SLinkRecord> linkedFlightRecords;

    CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
    CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_WAYPOINT_3DSURFACE);
    while (unsigned currentIndex = it.Increment())
      nodes.push_back(currentIndex);

    uint32 nNodes = nodes.size();
    file.Write(&nNodes, sizeof(nNodes));

    for (uint32 iNode = 0 ; iNode < nNodes ; ++iNode)
    {
      linkedVolumeRecords.resize(0);
      linkedFlightRecords.resize(0);

			unsigned nodeIndex = nodes[iNode];
      const GraphNode *pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);
      for (unsigned link = pNode->firstLinkIndex; link; link = m_pGraph->GetLinkManager().GetNextLink(link))
      {
        const GraphNode *pOther = m_pGraph->GetNodeManager().GetNode(m_pGraph->GetLinkManager().GetNextNode(link));
        AIAssert(pOther);
        float radiusOut = m_pGraph->GetLinkManager().GetRadius(link);
        unsigned linkIn = pOther->GetLinkIndex(m_pGraph->GetNodeManager(), m_pGraph->GetLinkManager(), pNode);
        if (!linkIn)
        {
          AIWarning("CWaypoint3DSurfaceNavRegion::WriteToFile Bad link index");
          continue;
        }
        float radiusIn = m_pGraph->GetLinkManager().GetRadius(linkIn);
        if (pOther->navType == IAISystem::NAV_VOLUME)
        {
          int vid = pOther->GetVolumeNavData()->nVolimeIdx;
          linkedVolumeRecords.push_back(SLinkRecord(vid, radiusOut, radiusIn));
        }
        else if (pOther->navType == IAISystem::NAV_FLIGHT)
          linkedFlightRecords.push_back(SLinkRecord(0, radiusOut, radiusIn));
      }
      uint32 nRecords = linkedVolumeRecords.size();
      file.Write(&nRecords, sizeof(nRecords));
      if (nRecords > 0)
        file.Write(&linkedVolumeRecords[0], nRecords * sizeof(linkedVolumeRecords[0]));

      nRecords = linkedFlightRecords.size();
      file.Write(&nRecords, sizeof(nRecords));
      if (nRecords > 0)
        file.Write(&linkedFlightRecords[0], nRecords * sizeof(linkedFlightRecords[0])); // danny todo fix the index!!
    }
    return true;
  }
  else
  {
    AIWarning("Unable to open 3D surface waypoint file %s", pName);
    return false;
  }
}

//====================================================================
// ReadFromFile
//====================================================================
bool CWaypoint3DSurfaceNavRegion::ReadFromFile(const char *pName)
{
  AILogLoading("CWaypoint3DSurfaceNavRegion::ReadFromFile %s", pName);
  if (!GetISystem()->IsEditor())
    Clear();

  CCryFile file;
  if( false != file.Open( pName, "rb" ) )
  {
    int iNumber;
    file.ReadType(&iNumber);
    if (iNumber != BAI_WAYPT3DSFC_FILE_VERSION)
    {
      AIError("CWaypoint3DSurfaceNavRegion Wrong AI 3D surface waypoint file version - found %d expected %d - regenerate navigation [Design bug]", iNumber, BAI_WAYPT3DSFC_FILE_VERSION);
      return false;
    }

    if (GetISystem()->IsEditor())
    {
      OnMissionLoaded();
      return true;
    }

    std::vector<unsigned> nodes;
    std::vector<SLinkRecord> linkedVolumeRecords;
    std::vector<SLinkRecord> linkedFlightRecords;

    CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
    CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_WAYPOINT_3DSURFACE);
    while (unsigned currentIndex = it.Increment())
      nodes.push_back(currentIndex);

    uint32 nNodes = 0;
    file.ReadType(&nNodes);
    if (nNodes != nodes.size())
    {
      AIWarning("CWaypoint3DSurfaceNavRegion::ReadFromFile have %d nodes, trying to read %d from file!",
        nodes.size(), nNodes);
      Clear();
      return false;
    }

    for (uint32 iNode = 0 ; iNode < nNodes ; ++iNode)
    {
      uint32 nRecords = 0;
      file.ReadType(&nRecords);
      linkedVolumeRecords.resize(nRecords);
      if (nRecords > 0)
        file.ReadType(&linkedVolumeRecords[0], nRecords);

      file.ReadType(&nRecords);
      linkedFlightRecords.resize(nRecords);
      if (nRecords > 0)
        file.ReadType(&linkedFlightRecords[0], nRecords);

      unsigned nodeIndex = nodes[iNode];

      for (uint32 iLink = 0 ; iLink < linkedVolumeRecords.size() ; ++iLink)
      {
        SLinkRecord &record = linkedVolumeRecords[iLink];
        unsigned otherIndex = GetAISystem()->GetVolumeNavRegion()->GetNodeFromIndex(record.nodeIndex);
        if (otherIndex)
          m_pGraph->Connect(nodeIndex, otherIndex, record.radiusOut, record.radiusIn);
        else
          AIWarning("CWaypoint3DSurfaceNavRegion::ReadFromFile read volume index %d that is out of bounds", record.nodeIndex);
      }
      // danny todo flight
    }

    return true;
  }
  else
  {
    AIWarning("Unable to open 3D surface waypoint file %s", pName);
    Clear();
    return false;
  }
}

