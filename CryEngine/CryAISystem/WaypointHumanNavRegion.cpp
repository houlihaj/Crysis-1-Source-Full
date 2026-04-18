#include "StdAfx.h"
#include "WaypointHumanNavRegion.h"
#include "AICollision.h"
#include "CAISystem.h"
#include "ISerialize.h"
#include "WorldOctree.h"
#include "ISystem.h"
#include "IPhysics.h"
#include "I3DEngine.h"
#include "IRenderAuxGeom.h"	
#include "ITimer.h"
#include <limits>

static float zWaypointScale = 1.0f;

// Structures used by ConnectNodes
struct SNode;
struct SLink
{
  SLink(SNode* pDestNode, float origRadius, float radius) : pDestNode(pDestNode), origRadius(origRadius), radius(radius), writtenToGraph(false), simple(false), pCachedPassabilityResult(0) {}
  SLink() {}
  SNode* pDestNode;
  float origRadius;
  float radius;
  bool writtenToGraph;
  bool simple;
  SCachedPassabilityResult *pCachedPassabilityResult;
};

struct SNode
{
  SNode(unsigned nodeIndex = 0) : nodeIndex(nodeIndex), mark(false) {}

  unsigned nodeIndex;
  std::vector<SLink> links;

  struct MarkClearer
  {
    MarkClearer() {ClearMarks();}
    ~MarkClearer() {ClearMarks();}
  };
  static void MarkNode(const SNode& node) {if (!node.mark) {node.mark = true; markedNodes.push_back(&node);}}
  static bool IsNodeMarked(const SNode& node) {return node.mark;}
  static void ClearMarks() {
    for (unsigned i = 0 ; i < markedNodes.size() ; ++i)
      markedNodes[i]->mark = false;
    markedNodes.resize(0);
  }
private:
  static std::vector<const SNode*> markedNodes;
  mutable bool mark;

};

std::vector<const SNode*> SNode::markedNodes;

struct SNodeDistPair
{
  SNodeDistPair(const SNode *pNode, float dist) : pNode(pNode), dist(dist) {}
  const SNode *pNode;
  float dist;
  bool operator<(const SNodeDistPair &rhs) const {return this->pNode < rhs.pNode;}
};

struct SNodePointerEquals
{
  bool operator() (const SNode &n1, const SNode &n2) const {return n1.nodeIndex == n2.nodeIndex;}
};

struct SNodePointerLess
{
  bool operator() (const SNode &n1, const SNode &n2) const {return n1.nodeIndex < n2.nodeIndex;}
};

//====================================================================
// SNodePair
//====================================================================
struct SNodePair
{
  SNodePair(SNode* _pNode1, SNode* _pNode2, float distSq) 
    : pNode1(min(_pNode1, _pNode2)), pNode2(max(_pNode1, _pNode2)), distSq(distSq) {}
    SNode* pNode1;
    SNode* pNode2;
    float distSq;
};

static inline bool NodePairDistLess(const SNodePair& np1, const SNodePair& np2) {
  return np1.distSq < np2.distSq;}

struct SNodePairCompare
{
  bool operator()(const SNodePair& np1, const SNodePair& np2) const 
  {
    if (np1.pNode1 < np2.pNode1)
      return true;
    else if (np1.pNode1 > np2.pNode1)
      return false;
    else return (np1.pNode2 < np2.pNode2);
  }
};

typedef std::vector<SNodePair> NodePairVector;
typedef std::set<SNodePair, SNodePairCompare> NodePairSet;

// for caching the nodes associated with a building ID
typedef std::vector<SNode> BuildingNodeCache;

//====================================================================
// WouldLinkIntersectBoundary
//====================================================================
inline bool WouldLinkIntersectBoundary(const Vec3& pos1, const Vec3& pos2, const ListPositions& boundary) 
{
  return Overlap::Lineseg_Polygon2D(Lineseg(pos1, pos2), boundary);
}

//====================================================================
// CWaypointHumanNavRegion
//====================================================================
CWaypointHumanNavRegion::CWaypointHumanNavRegion(CGraph* pGraph)
{
  AIAssert(pGraph);
  m_pGraph = pGraph;
  m_currentNodeIt = 0;
}

//====================================================================
// ~CWaypointHumanNavRegion
//====================================================================
CWaypointHumanNavRegion::~CWaypointHumanNavRegion()
{
  delete m_currentNodeIt;
  m_currentNodeIt = 0;
}

//===================================================================
// ResetUpdateStatus
//===================================================================
void CWaypointHumanNavRegion::ResetUpdateStatus()
{
  delete m_currentNodeIt;
  m_currentNodeIt = 0;
}

//====================================================================
// UglifyPath
//====================================================================
void CWaypointHumanNavRegion::UglifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath, 
                                         const Vec3& startPos, const Vec3& startDir, 
                                         const Vec3& endPos, const Vec3 & endDir)
{
  outPath.push_back(PathPointDescriptor(IAISystem::NAV_WAYPOINT_HUMAN, startPos));
  for(VectorConstNodeIndices::const_iterator itrCurNode=inPath.begin() ; itrCurNode != inPath.end() ; ++itrCurNode)
  {
		const GraphNode& curNode=*m_pGraph->GetNodeManager().GetNode(*itrCurNode);
    outPath.push_back(PathPointDescriptor(IAISystem::NAV_WAYPOINT_HUMAN, curNode.GetPos()));
  }
  outPath.push_back(PathPointDescriptor(IAISystem::NAV_WAYPOINT_HUMAN, endPos));
}

//====================================================================
// BeautifyPath
//====================================================================
void CWaypointHumanNavRegion::BeautifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath, 
                                           const Vec3& startPos, const Vec3& startDir, 
                                           const Vec3& endPos, const Vec3 & endDir,
                                           float radius,
                                           const AgentMovementAbility & movementAbility,
                                           const NavigationBlockers& navigationBlockers)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );
  if (inPath.empty())
    return;

  AIAssert(m_pGraph->GetNodeManager().GetNode(inPath.front())->navType == IAISystem::NAV_WAYPOINT_HUMAN);
  int nBuildingID = m_pGraph->GetNodeManager().GetNode(inPath.front())->GetWaypointNavData()->nBuildingID;
  if (nBuildingID < 0)
  {
    AIWarning("CWaypointHumanNavRegion::BeautifyPath Unable to get building ID from node %d", inPath.front());
    return;
  }
  const SpecialArea* sa = GetAISystem()->GetSpecialArea(nBuildingID);
  if (!sa)
  {
    AIWarning("CWaypointHumanNavRegion::BeautifyPath Unable to get special area for building ID %d", nBuildingID);
    return;
  }
  const ListPositions	& buildingPolygon = sa->GetPolygon();

  bool allowStartSkip = !startPos.IsEquivalent(m_pGraph->GetNodeManager().GetNode(inPath.front())->GetPos(), 0.01f);
  bool allowEndSkip = !endPos.IsEquivalent(m_pGraph->GetNodeManager().GetNode(inPath.back())->GetPos(), 0.01f);

  if (inPath.size() == 1)
  {
    if (CheckWalkability(startPos, endPos, 0.0f, false, buildingPolygon, AICE_ALL) && 
      allowStartSkip && allowEndSkip)
    {
      // we can walk directly - don't need to go via the node.
      return;
    }
  }

  VectorConstNodeIndices inPathCopy(inPath);
  // see if we can skip the last one
  unsigned inPathSize = inPathCopy.size();
  if (inPathSize > 1 && allowEndSkip)
  {
    if (CheckWalkability(endPos, m_pGraph->GetNodeManager().GetNode(inPath[inPathSize-2])->GetPos(), 0.0f, false, buildingPolygon, AICE_ALL))
    {
      inPathCopy.pop_back();
      --inPathSize;
    }
  }
  // see if we can skip the first one
  if (inPathSize > 1 && allowStartSkip)
  {
    if (CheckWalkability(startPos, m_pGraph->GetNodeManager().GetNode(inPath[1])->GetPos(), 0.0f, false, buildingPolygon, AICE_ALL))
    {
      inPathCopy.erase(inPathCopy.begin());
      --inPathSize;
    }
  }
  else if (inPathSize == 1 && allowStartSkip)
  {
    if (CheckWalkability(startPos, endPos, 0.0f, false, buildingPolygon, AICE_ALL))
    {
      // we can walk directly - don't need to go via the node.
      return;
    }
  }

  // Just go through all the nodes
  for(VectorConstNodeIndices::const_iterator itrCurNode=inPathCopy.begin() ; itrCurNode != inPathCopy.end() ; ++itrCurNode)
  {
    const GraphNode& curNode=*m_pGraph->GetNodeManager().GetNode(*itrCurNode);
    AIAssert(curNode.navType == IAISystem::NAV_WAYPOINT_HUMAN);
    outPath.push_back(PathPointDescriptor(IAISystem::NAV_WAYPOINT_HUMAN, curNode.GetPos()));
  }
}

//====================================================================
// FillGreedyMap
//====================================================================
void CWaypointHumanNavRegion::FillGreedyMap(unsigned nodeIndex, const Vec3 &pos, 
                                            IVisArea *pTargetArea, bool bStayInArea, CandidateIdMap& candidates)
{
	GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);
  m_pGraph->MarkNode(nodeIndex);

  if (!pNode->firstLinkIndex)
    return;

  GraphNode *pNext = 0;
  for (unsigned link = pNode->firstLinkIndex; link; link = m_pGraph->GetLinkManager().GetNextLink(link))
  { 
		unsigned nowIndex = m_pGraph->GetLinkManager().GetNextNode(link);
    GraphNode *pNow = m_pGraph->GetNodeManager().GetNode(nowIndex);
    if ( pNow->mark || pNow->navType != IAISystem::NAV_WAYPOINT_HUMAN || pNow->GetWaypointNavData()->nBuildingID != pNode->GetWaypointNavData()->nBuildingID) 
      continue;
    if (bStayInArea && (pNow->GetWaypointNavData()->pArea != pTargetArea))
      continue;

    Vec3 delta = (pos - pNow->GetPos());
    delta.z *= zWaypointScale; // scale vertical to prefer waypoints on the same level
    float thisdist = delta.GetLengthSquared();
    candidates.insert(CandidateIdMap::iterator::value_type(thisdist,nowIndex));

    // this snippet will make sure we only check all points inside the target area - not the whole building
    if (pTargetArea && pNow->GetWaypointNavData()->pArea == pTargetArea)
      FillGreedyMap(nowIndex,pos,pTargetArea,true, candidates);
    else
      FillGreedyMap(nowIndex,pos,pTargetArea,false, candidates);
  }

}

struct SWrongBuildingRadius
{
  SWrongBuildingRadius(CGraphLinkManager& linkManager, CGraphNodeManager& nodeManager, int ID, float radius)
		: linkManager(linkManager), nodeManager(nodeManager), ID(ID), radius(radius) {}
  bool operator()(const std::pair<float, unsigned> &node) 
  {
    if (nodeManager.GetNode(node.second)->GetWaypointNavData()->nBuildingID != ID)
      return true;
    if (GetAISystem()->ExitNodeImpossible(linkManager, nodeManager.GetNode(node.second), radius))
      return true;
    return false;
  }
	CGraphLinkManager& linkManager;
	CGraphNodeManager& nodeManager;
  int ID;
  float radius;
};

//====================================================================
// GetEnclosing
//====================================================================
unsigned CWaypointHumanNavRegion::GetEnclosing(const Vec3 &pos, float passRadius, unsigned startIndex, 
                                                 float range, Vec3 * closestValid, bool returnSuspect, const char *requesterName)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  IVisArea *pGoalArea; 
  int	nBuildingID;
  GetAISystem()->CheckNavigationType(pos, nBuildingID, pGoalArea, IAISystem::NAV_WAYPOINT_HUMAN);

  if (nBuildingID < 0)
  {
    AIWarning("CWaypointHumanNavRegion::GetEnclosing found bad building ID = %d (%s) for %s (%5.2f, %5.2f, %5.2f)", 
      nBuildingID, GetAISystem()->GetNavigationShapeName(nBuildingID), requesterName, pos.x, pos.y, pos.z);
    return 0;
  }

  const SpecialArea *sa = GetAISystem()->GetSpecialArea(nBuildingID);
  if (!sa)
  {
    AIWarning("CWaypointHumanNavRegion::GetEnclosing found no area for %s (%5.2f, %5.2f, %5.2f)", 
      requesterName, pos.x, pos.y, pos.z);
    return 0;
  }

  // get a floor position
  Vec3 floorPos;
  static float maxFloorDist = 15.0f; // big value because the target might be huge - e.g. hunter
  if (false == GetFloorPos(floorPos, pos, walkabilityFloorUpDist, maxFloorDist, walkabilityDownRadius, AICE_ALL))
  {
    AIWarning("CWaypointHumanNavRegion::GetEnclosing Could not find floor position from (%5.2f, %5.2f, %5.2f) %s",
      pos.x, pos.y, pos.z, requesterName);
    return 0;
  }

  if (sa->fNodeAutoConnectDistance < 0.0001f)
    AIWarning("CWaypointHumanNavRegion::GetEnclosing AI nav modifier %s has NodeAutoConnectDistance=0 - increase to a sensible value [design bug]", GetAISystem()->GetNavigationShapeName(nBuildingID));

  float totalDist = 1.5 * sa->fNodeAutoConnectDistance + range;

  typedef std::vector< std::pair<float, unsigned> > TNodes;
  static TNodes nodes;
  nodes.resize(0);

  // Most times we'll find the desired node close by
  CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
  allNodes.GetAllNodesWithinRange(nodes, floorPos, totalDist, IAISystem::NAV_WAYPOINT_HUMAN);
  std::vector< std::pair<float, unsigned> >::iterator it = std::remove_if(nodes.begin(), nodes.end(), SWrongBuildingRadius(m_pGraph->GetLinkManager(), m_pGraph->GetNodeManager(), nBuildingID, passRadius));
  nodes.erase(it, nodes.end());

  static float zScale = 2.0f;
  if (nodes.empty())
  {
    AILogComment("Unable to find node close to %s (%5.2f, %5.2f, %5.2f) - checking all nodes from building id %d (%s)",
      requesterName, pos.x, pos.y, pos.z, nBuildingID, GetAISystem()->GetNavigationShapeName(nBuildingID));

    CAllNodesContainer::Iterator nit(allNodes, IAISystem::NAV_WAYPOINT_HUMAN);
    while (unsigned currentIndex = nit.Increment())
    {
			GraphNode* pCurrent = m_pGraph->GetNodeManager().GetNode(currentIndex);
      if (pCurrent->GetWaypointNavData()->nBuildingID == nBuildingID && !GetAISystem()->ExitNodeImpossible(m_pGraph->GetLinkManager(), pCurrent, passRadius))
      {
        nodes.push_back(std::make_pair(0.0f, currentIndex)); // dist calculated later
      }
    }
  }

  if (nodes.empty())
  {
    AIWarning("CWaypointHumanNavRegion::GetEnclosing No nodes found for area containing %s (%5.2f, %5.2f, %5.2f) building id %d (%s) [design bug]", 
      requesterName, pos.x, pos.y, pos.z, nBuildingID, GetAISystem()->GetNavigationShapeName(nBuildingID));
    return 0;
  }

  unsigned closestIndex = 0;
  float closestDist = std::numeric_limits<float>::max();

  for (unsigned i = 0 ; i < nodes.size() ; ++i)
  {
		Vec3 delta = m_pGraph->GetNodeManager().GetNode(nodes[i].second)->GetPos() - floorPos;
    delta.z *= 5;
    float distSq = delta.GetLengthSquared();
    nodes[i].first = distSq;
    if (distSq < closestDist)
    {
      closestDist = distSq;
      closestIndex = nodes[i].second;
    }
  }

  // Assume that if the requester wants a full check then they've passed in a closest position
  // pointer - and if they haven't they're doing a quick test. The code gets called from 
  // COPTrace::Execute2D - Danny todo see if we can avoid calling this code from there, or call
  // a more specific function
//  if (!closestValid)
//    return pClosest;

  std::sort(nodes.begin(), nodes.end());

  const ListPositions	& buildingPolygon = sa->GetPolygon();

  int nRejected = 0;
  static int maxRejected =30;
  for (TNodes::iterator nit = nodes.begin() ; nit != nodes.end() && nRejected < maxRejected; ++nit )
  {
    unsigned nodeIndex = nit->second;

    if (nRejected < 15)
    {
      if (CheckWalkabilitySimple(SWalkPosition (floorPos, true), m_pGraph->GetNodeManager().GetNode(nodeIndex)->GetPos(), 0.0f, AICE_ALL))
        return nodeIndex;
    }
    else
    {
      if (CheckWalkability(SWalkPosition (floorPos, true), m_pGraph->GetNodeManager().GetNode(nodeIndex)->GetPos(), 0.0f, false, buildingPolygon, AICE_ALL))
        return nodeIndex;
    }
    ++nRejected;
  }

	// NOTE Jun 16, 2007: <pvl> draw the requester pos and pos of nodes that had
	// their walkability from 'pos' checked and failed
	float timeVisible = GetAISystem()->m_cvDrawGetEnclosingFailures->GetFVal();
	if (timeVisible > 0.0f)
	{
		TNodes::iterator nit = nodes.begin();
		TNodes::iterator nend = nodes.end();
		for (int i=0; nit != nend && i < maxRejected; ++nit, ++i )
		{
			unsigned nodeIndex = nit->second;
			Vec3 nodePos = m_pGraph->GetNodeManager().GetNode(nodeIndex)->GetPos();
			GetAISystem()->AddDebugLine (pos, nodePos, 255, 255, 0, timeVisible);
			GetAISystem()->AddDebugSphere (nodePos, 0.15f, 255, 255, 0, timeVisible);
		}
	}

  unsigned nodeIndex = returnSuspect ? closestIndex : 0;
  if (returnSuspect)
    AIWarning("CWaypointHumanNavRegion::GetEnclosing %d nodes rejected for reaching %s (%5.2f, %5.2f, %5.2f): returning %d (in %d (%s))", 
    nRejected, requesterName, pos.x, pos.y, pos.z, nodeIndex, nBuildingID, GetAISystem()->GetNavigationShapeName(nBuildingID));
  return nodeIndex;
}

//====================================================================
// CheckPassability
//====================================================================
bool CWaypointHumanNavRegion::CheckPassability(const Vec3& from, const Vec3& to, float radius, const NavigationBlockers& navigationBlockers) const
{
  ListPositions boundary;
  return CheckWalkability(from, to, 0.0f, false, boundary, AICE_ALL);
}

//====================================================================
// GetClosestNode
//====================================================================
unsigned CWaypointHumanNavRegion::GetClosestNode(const Vec3 &pos, int nBuildingID)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );
  std::vector<unsigned> entrances;
  if (!m_pGraph->GetEntrances(nBuildingID, pos, entrances))
  {
    AIWarning("CWaypointHumanNavRegion::GetClosestNode No entrance for navigation area nr %d (%s). The position is (%.3f,%.3f,%.3f)",
      nBuildingID, GetAISystem()->GetNavigationShapeName(nBuildingID), pos.x, pos.y, pos.z);
    return 0;
  }

  IVisArea *pGoalArea = 0; 
  m_pGraph->ClearMarks();
  CandidateIdMap candidates;
  for (unsigned i = 0 ; i < entrances.size() ; ++i)
    FillGreedyMap(entrances[i], pos, pGoalArea, false, candidates);
  m_pGraph->ClearMarks();

  if (candidates.empty())
  {
    AIWarning("CWaypointHumanNavRegion::GetClosestNode No nodes found for this navigation modifier %d (%s) apart from entrance",
      nBuildingID, GetAISystem()->GetNavigationShapeName(nBuildingID));
    return 0;
  }
  else
  {
    return candidates.begin()->second;
  }
}

//====================================================================
// Serialize
//====================================================================
void CWaypointHumanNavRegion::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
  ser.BeginGroup("WaypointNavRegion");

  ser.EndGroup();
}

//====================================================================
// Reset
//====================================================================
void CWaypointHumanNavRegion::Clear()
{
  delete m_currentNodeIt;
  m_currentNodeIt = 0;
}

//====================================================================
// CheckAndDrawBody
//====================================================================
bool CWaypointHumanNavRegion::CheckAndDrawBody(const Vec3& pos, IRenderer *pRenderer)
{
  bool result = false;
  bool gotFloor = false;
  Vec3 floorPos = pos;
  if (GetFloorPos(floorPos, pos, walkabilityFloorUpDist, walkabilityFloorDownDist, walkabilityDownRadius, AICE_ALL))
    gotFloor = true;

  Vec3 segStart = floorPos + walkabilityTorsoBaseOffset + Vec3(0, 0, walkabilityRadius);
  Vec3 segEnd = floorPos + Vec3(0, 0, walkabilityTotalHeight - walkabilityRadius); 
  Lineseg torsoSeg(segStart, segEnd);

  if (gotFloor && !OverlapCapsule(torsoSeg, walkabilityRadius, AICE_ALL))
    result = true;

  if (pRenderer)
  {
    ColorF col;
    if (result)
      col.Set(0, 1, 0);
    else
      col.Set(1, 0, 0);
    pRenderer->GetIRenderAuxGeom()->DrawSphere(segStart, walkabilityRadius, col);
    pRenderer->GetIRenderAuxGeom()->DrawSphere(segEnd, walkabilityRadius, col);
    pRenderer->GetIRenderAuxGeom()->DrawSphere(segStart, walkabilityDownRadius, col);
    pRenderer->GetIRenderAuxGeom()->DrawSphere(floorPos, walkabilityDownRadius, col);
    pRenderer->GetIRenderAuxGeom()->DrawLine(floorPos, col, segStart, col);
  }

  return result;
}

//====================================================================
// SNodeDistSorter
//====================================================================
struct SNodeDistSorter
{
  SNodeDistSorter(const Vec3& focusPos) : m_focusPos(focusPos) {}
  bool operator()(const GraphNode* pNode0, const GraphNode* pNode1) const
  {
    float d0 = (pNode0->GetPos() - m_focusPos).GetLengthSquared();
    float d1 = (pNode1->GetPos() - m_focusPos).GetLengthSquared();
    return d0 < d1;
  }
private:
  const Vec3 m_focusPos;
};

//====================================================================
// RemoveAutoLinks
//====================================================================
void CWaypointHumanNavRegion::RemoveAutoLinks(unsigned nodeIndex)
{
	GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);

  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );
  for (unsigned link = pNode->firstLinkIndex; link;)
  {
		unsigned nextLink = m_pGraph->GetLinkManager().GetNextLink(link);

		if (m_pGraph->GetNodeManager().GetNode(m_pGraph->GetLinkManager().GetNextNode(link))->navType == IAISystem::NAV_WAYPOINT_HUMAN)
		{
			m_pGraph->GetLinkManager().RestoreLink(link);
			EWaypointLinkType linkType = SWaypointNavData::GetLinkTypeFromRadius(m_pGraph->GetLinkManager().GetRadius(link));
			if (linkType == WLT_AUTO_PASS || linkType == WLT_AUTO_IMPASS )
				m_pGraph->Disconnect(nodeIndex, link);
		}

		link = nextLink;
  }
}

//====================================================================
// RemoveAutoLinksInBuilding
//====================================================================
void CWaypointHumanNavRegion::RemoveAutoLinksInBuilding(int nBuildingID)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );
  AILogProgress("Removing waypoint nodes for building ID %d", nBuildingID);
  CAllNodesContainer& allNodes = m_pGraph->GetAllNodes();
  CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_WAYPOINT_HUMAN);
  while (unsigned nodeIndex = it.Increment())
  {
		GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);
    if (pNode->navType == IAISystem::NAV_WAYPOINT_HUMAN && pNode->GetWaypointNavData()->nBuildingID == nBuildingID)
      RemoveAutoLinks(nodeIndex);
  }
}

//====================================================================
// RecursivelyFindNodeWithinRadius
//====================================================================
bool RecursivelyFindNodeWithinRadius(const SNode *curNode, const SNode *const destNode, 
                                                              std::set<SNodeDistPair> &checkedNodes, 
                                                              float curDist, float maxDist)
{
	CGraphNodeManager& nodeManager = ((CGraph*)GetAISystem()->GetGraph())->GetNodeManager();

  unsigned nLinks = curNode->links.size();
  for (unsigned iLink = 0 ; iLink < nLinks ; ++iLink)
  {
    const SLink& link = curNode->links[iLink];
    const SNode* nextNode = link.pDestNode;

    float linkDist = Distance::Point_Point2D(nodeManager.GetNode(curNode->nodeIndex)->GetPos(), nodeManager.GetNode(nextNode->nodeIndex)->GetPos());
    if (linkDist < 0.00001f)
      linkDist = 0.00001f;
    float totalDist = curDist + linkDist;
    if (totalDist > maxDist)
      continue;

    if (nextNode == destNode)
      return true;

    std::set<SNodeDistPair>::iterator it = checkedNodes.find(SNodeDistPair(nextNode, 0.0f));

    if (it != checkedNodes.end())
    {
      const SNodeDistPair &ndp = *it;
      if (ndp.dist < totalDist)
        continue; // already done better
      else
        checkedNodes.erase(it); // will reinsert
    }

    checkedNodes.insert(SNodeDistPair(nextNode, totalDist));

    bool foundIt = RecursivelyFindNodeWithinRadius(nextNode, destNode, checkedNodes, totalDist, maxDist);
    if (foundIt)
      return true;
  }
  return false;
}

//====================================================================
// CheckForExistingNodeLink
// returns true if there is a reasonable link (direct or otherwise) from node to hide
// walks through the graph looking for a reasonably direct route
//====================================================================
bool CheckForExistingLink(const SNode& node1, const SNode& node2)
{
	CGraphNodeManager& nodeManager = ((CGraph*)GetAISystem()->GetGraph())->GetNodeManager();
	GraphNode* pGraphNode1 = nodeManager.GetNode(node1.nodeIndex);
	GraphNode* pGraphNode2 = nodeManager.GetNode(node2.nodeIndex);

	if (node1.nodeIndex == node2.nodeIndex)
    return true;

  SNode::MarkClearer markClearer;

  static int method = 1;
  if (!method)
  {
    return false;
  }
  else if (method == 1)
  {
    static float maxIndirectDistScale = 1.1f;
    float directDist = Distance::Point_Point2D(pGraphNode1->GetPos(), pGraphNode2->GetPos());
    if (directDist < 0.0001f)
    {
      if (fabs(pGraphNode1->GetPos().z - pGraphNode2->GetPos().z) < 0.1f)
      {
        AIWarning("Two nodes on top of each other at (%5.2f, %5.2f, %5.2f)", 
          pGraphNode1->GetPos().x, pGraphNode1->GetPos().y, pGraphNode1->GetPos().z);
        return true;
      }
      else
      {
        return true;
      }
    }
    float maxIndirectDist = maxIndirectDistScale * directDist;

    /// pointer to node, distance from pNode1
    std::set<SNodeDistPair> checkedNodes;
    checkedNodes.insert(SNodeDistPair(&node1, 0.0f));

    return RecursivelyFindNodeWithinRadius(&node1, &node2, checkedNodes, 0.0f, maxIndirectDist);
  }
  else
  {
    // The smaller this is the mode node1->dest links are suppressed
    static float criticalDot = 0.95f;
    const SNode* pNode1 = &node1;

    int sanityCounter = 100;
    while (sanityCounter--)
    {
      SNode::MarkNode(*pNode1);

      unsigned nLinks = pNode1->links.size();
      const SNode* pBestNextNode = 0;
      float bestDot = -1.0f;
      // do everything in 2D
      Vec3 currentToNode2 = pGraphNode2->GetPos() - pGraphNode1->GetPos();
      currentToNode2.z = 0.0f;
      float currentToNode2Dist = currentToNode2.GetLength();
      if (currentToNode2Dist < 0.0001f)
        return true;
      currentToNode2 /= currentToNode2Dist;
      for (unsigned iLink = 0 ; iLink < nLinks ; ++iLink)
      {
        const SLink& link = pNode1->links[iLink];
        const SNode* pNext = link.pDestNode;
				const GraphNode* pNextGraphNode = nodeManager.GetNode(pNext->nodeIndex);
        if (pNext == &node2)
          return true;
        if (pNextGraphNode == pGraphNode2)
          return true;
        if (pNextGraphNode->navType != IAISystem::NAV_WAYPOINT_HUMAN)
          continue;
        if (SNode::IsNodeMarked(*pNext))
          continue;
        if (pNextGraphNode->GetPos().IsEquivalent(pGraphNode2->GetPos(), 0.01f))
          return true;

        Vec3 nextToNode2 = pGraphNode2->GetPos() - pNextGraphNode->GetPos();
        nextToNode2.z = 0.0f;
        float nextToNode2Dist = nextToNode2.GetLength();
        if (nextToNode2Dist >= currentToNode2Dist)
          continue;
        if (nextToNode2Dist < 0.0001f)
          return false; // must be directly above/below but not at because of the IsEquivalent test above
        nextToNode2 /= nextToNode2Dist;
        float dot = currentToNode2.Dot(nextToNode2);
        if (dot > bestDot)
        {
          bestDot = dot;
          pBestNextNode = pNext;
        }
      }

      if (bestDot < criticalDot || !pBestNextNode)
        return false;

      pNode1 = pBestNextNode;
    }
    AIWarning("Excessive searching for existing link (bug an AI programmer)");
    return false;
  }
}

//====================================================================
// Connect
//====================================================================
void Connect(SNode& node1, SNode& node2, float radius, SLink *&linkOneTwo, SLink *&linkTwoOne)
{
  bool link12 = true;
  bool link21 = true;
  for (unsigned i = 0 ; i < node1.links.size() ; ++i)
  {
    if (node1.links[i].pDestNode == &node2)
    {
      link12 = false;
      linkOneTwo = &node1.links[i];
      node1.links[i].radius = radius;
      break;
    }
  }
  for (unsigned i = 0 ; i < node2.links.size() ; ++i)
  {
    if (node2.links[i].pDestNode == &node1)
    {
      link21 = false;
      linkTwoOne = &node2.links[i];
      node2.links[i].radius = radius;
      break;
    }
  }
  if (link12)
  {
    node1.links.push_back(SLink(&node2, radius, radius));
    linkOneTwo = &node1.links.back();
  }

  if (link21)
  {
    node2.links.push_back(SLink(&node1, radius, radius));
    linkTwoOne = &node2.links.back();
  }
}

//====================================================================
// ClearNodeLinkMarks
//====================================================================
void ClearNodeLinkMarks(BuildingNodeCache& nodes)
{
  for (unsigned i = 0 ; i < nodes.size() ; ++i)
  {
    SNode& node = nodes[i];
    std::vector<SLink> &links = node.links;
    for (int iLink = 0 ; iLink < node.links.size() ; ++iLink)
      node.links[iLink].writtenToGraph = false;
  }
}

//====================================================================
// UpdateGraph
/// actually applies the results and updates the graph. If modifyRadiusOnly then for existing links 
/// the radius is modified - otherwise the original link radius gets changed
// Make this much nicer - only change stuff that's changed
//====================================================================
void UpdateGraph(BuildingNodeCache& nodes, bool modifyRadiusOnly)
{
	CGraphLinkManager& linkManager = ((CGraph*)GetAISystem()->GetGraph())->GetLinkManager();
	CGraphNodeManager& nodeManager = ((CGraph*)GetAISystem()->GetGraph())->GetNodeManager();

	FUNCTION_PROFILER( GetISystem(), PROFILE_AI );
  for (unsigned i = 0 ; i < nodes.size() ; ++i)
  {
    SNode& node = nodes[i];
    std::vector<SLink> &links = node.links;

    GraphNode *pNode = nodeManager.GetNode(node.nodeIndex);

    // walk through the existing links. If the link is auto and not in our new set of links,
    // then remove it. If it is in our new set then just update the radius and mark the new
    // links. At the end we'll walk through any unmarked links and create them from new
    for (unsigned link = pNode->firstLinkIndex; link;)
    {
      unsigned nextLink = linkManager.GetNextLink(link);

			if (nodeManager.GetNode(linkManager.GetNextNode(link))->navType == IAISystem::NAV_WAYPOINT_HUMAN)
			{
				linkManager.RestoreLink(link);
				EWaypointLinkType origLinkType = SWaypointNavData::GetLinkTypeFromRadius(linkManager.GetOrigRadius(link));
				if (origLinkType == WLT_AUTO_PASS || origLinkType == WLT_AUTO_IMPASS )
				{
					bool foundLink = false;
					for (unsigned iNewLink = 0 ; iNewLink < links.size() ; ++iNewLink)
					{
						SLink &newLink = links[iNewLink];
						if (newLink.pDestNode->nodeIndex == linkManager.GetNextNode(link))
						{
							foundLink = true;
							if (newLink.writtenToGraph)
								continue;
							unsigned linkOneTwo;
							unsigned linkTwoOne;
							GetAISystem()->GetGraph()->Connect(node.nodeIndex, linkManager.GetNextNode(link), newLink.radius, newLink.radius, &linkOneTwo, &linkTwoOne);
							if (linkOneTwo && linkTwoOne)
							{
								if (!modifyRadiusOnly)
								{
									linkManager.SetRadius(linkOneTwo, newLink.radius);
									linkManager.SetRadius(linkTwoOne, newLink.radius);
								}
								linkManager.SetSimple(linkOneTwo, newLink.simple);
								linkManager.SetSimple(linkTwoOne, newLink.simple);
							}
							newLink.writtenToGraph = true;
							break;
						}
					}
					if (!foundLink)
					{
						GetAISystem()->GetGraph()->Disconnect(node.nodeIndex, link);
					}
				}
				else if (origLinkType == WLT_EDITOR_IMPASS)
				{
					// don't overwrite editor-marked impassable links
					for (unsigned iNewLink = 0 ; iNewLink < links.size() ; ++iNewLink)
					{
						SLink &newLink = links[iNewLink];
						if (newLink.pDestNode->nodeIndex == linkManager.GetNextNode(link))
						{
							newLink.writtenToGraph = true;
						}
					}
				}
			}

			link = nextLink;
		}

    for (unsigned iLink = 0 ; iLink < node.links.size() ; ++iLink)
    {
      const SLink& link = node.links[iLink];
      if (link.writtenToGraph)
        continue;
      EWaypointLinkType linkType = SWaypointNavData::GetLinkTypeFromRadius(link.radius);
      if (linkType == WLT_AUTO_PASS || linkType == WLT_AUTO_IMPASS ||
        linkType == WLT_EDITOR_PASS || linkType == WLT_EDITOR_IMPASS)
      {
        // For auto links this will create new ones since we just deleted the old links
        // for non-auto links this will modify them where necessary, but the old radius
        // can be restored
        unsigned linkOneTwo;
        unsigned linkTwoOne;
        GetAISystem()->GetGraph()->Connect(node.nodeIndex, link.pDestNode->nodeIndex, 
          link.radius, link.radius, &linkOneTwo, &linkTwoOne);
        if (linkOneTwo && linkTwoOne)
        {
          if (!modifyRadiusOnly)
          {
						EWaypointLinkType origLinkType = SWaypointNavData::GetLinkTypeFromRadius(linkManager.GetOrigRadius(linkOneTwo));
            if (origLinkType != WLT_EDITOR_PASS && origLinkType != WLT_EDITOR_IMPASS)
            {
              linkManager.SetRadius(linkOneTwo, link.radius);
              linkManager.SetRadius(linkTwoOne, link.radius);
            }
						linkManager.SetSimple(linkOneTwo, link.simple);
						linkManager.SetSimple(linkTwoOne, link.simple);
          }
        }
      }
    }
  }
}

std::pair<bool,bool> CWaypointHumanNavRegion::CheckIfLinkSimple(Vec3 from, Vec3 to,
										float paddingRadius, bool checkStart,
										const ListPositions& boundary, 
										EAICollisionEntities aiCollisionEntities, 
										SCheckWalkabilityState *pCheckWalkabilityState)
{
	std::vector<float> & heightAlongTheLink = pCheckWalkabilityState->heightAlongTheLink;
	if (pCheckWalkabilityState->state == SCheckWalkabilityState::CWS_NEW_QUERY)
	{
		pCheckWalkabilityState->computeHeightAlongTheLink = true;
		heightAlongTheLink.resize (0);
		heightAlongTheLink.reserve (50);
	} else {
		assert (pCheckWalkabilityState->computeHeightAlongTheLink);
	}

	bool walkable = CheckWalkability (from, to, paddingRadius, checkStart, boundary,
		aiCollisionEntities, 0, pCheckWalkabilityState);

	if (pCheckWalkabilityState->state != SCheckWalkabilityState::CWS_RESULT) {
		// NOTE Apr 27, 2007: <pvl> AFAIK 'walkable' should be 'false' here
		return std::make_pair (walkable,false);
	}

	if ( ! walkable)
		return std::make_pair (false,false);

	float avg = std::accumulate (
		heightAlongTheLink.begin (),
		heightAlongTheLink.end (),
		0.0f, std::plus<float> ()) / heightAlongTheLink.size ();
	std::vector<float>::const_iterator it = heightAlongTheLink.begin ();
	std::vector<float>::const_iterator end = heightAlongTheLink.end ();
	bool linkIsSimple = true;
	for ( ; it != end; ++it)
	{
		if ( fabs (*it - avg) > 0.10)
		{
			linkIsSimple = false;
			break;
		}
	}

	return std::make_pair (true, linkIsSimple);
}

/**
 * If simplified walkability computation isn't requested by a console variable,
 * this function just falls back to generic CheckWalkability() test.  If a link
 * is classified as "simple", its walkability is tested using
 * CheckWalkabilitySimple() which should be faster.  Conversely, if the link
 * fails the "simplicity" test, generic CheckWalkability() is invoked.
 *
 * If "simplicity" of the link hasn't been established yet, it is computed now.
 * The test involves gathering floor heights along the link and seeing if the
 * sequence is straight or "linear" enough.
 */
bool CWaypointHumanNavRegion::CheckPassability(unsigned linkIndex,
											Vec3 from, Vec3 to,
											float paddingRadius, bool checkStart,
											const ListPositions& boundary, 
											EAICollisionEntities aiCollisionEntities, 
											SCachedPassabilityResult *pCachedResult,
											SCheckWalkabilityState *pCheckWalkabilityState)
{
	if ( ! GetAISystem()->m_cvSimpleWayptPassability->GetIVal()) {
		return CheckWalkability (from, to, paddingRadius, checkStart, boundary,
						aiCollisionEntities, pCachedResult, pCheckWalkabilityState);
	}

	if (m_pGraph->GetLinkManager().SimplicityComputed(linkIndex))
	{
		if (m_pGraph->GetLinkManager().IsSimple(linkIndex))
			return CheckWalkabilitySimple (from, to, paddingRadius,	aiCollisionEntities);
		else
			return CheckWalkability (from, to, paddingRadius, checkStart, boundary,
							aiCollisionEntities, pCachedResult, pCheckWalkabilityState);
	}

	std::pair<bool,bool> result = CheckIfLinkSimple (from, to, paddingRadius, checkStart,
							boundary, aiCollisionEntities, pCheckWalkabilityState);
	bool walkable = result.first;

	if (pCheckWalkabilityState->state == SCheckWalkabilityState::CWS_RESULT)
	{
		if (walkable)
		{
			bool simple = result.second;
			CGraphLinkManager& linkManager = ((CGraph*)GetAISystem()->GetGraph())->GetLinkManager();
			linkManager.SetSimple (linkIndex, simple);
		}

		// NOTE Apr 25, 2007: <pvl> we're done with this link, reset the flag
		pCheckWalkabilityState->computeHeightAlongTheLink = false;
	}

	return walkable;
}

//====================================================================
// ConnectNodes
// We form a list of all node pairs, then sort by distance.
// We try to connect starting with the shortest distance so that
// subsequently attempts to link can look for an indirect route
//====================================================================
void CWaypointHumanNavRegion::ConnectNodes(const SpecialArea *sa, 
                                           EAICollisionEntities collisionEntities,
                                           float maxDistNormal,
                                           float maxDistMax, 
                                           unsigned nLinksForMaxDistMax)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  float maxDistMaxSq = square(maxDistMax);
  float maxDistNormalSq = square(maxDistNormal);
  const ListPositions	& buildingPolygon = sa->GetPolygon();

  // get all the nodes...
  BuildingNodeCache buildingNodeCache;
  CAllNodesContainer& allNodes = m_pGraph->GetAllNodes();
  CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_WAYPOINT_HUMAN);
  while (unsigned nodeIndex = it.Increment())
  {
    if (m_pGraph->GetNodeManager().GetNode(nodeIndex)->GetWaypointNavData()->nBuildingID == sa->nBuildingID)
    {
      buildingNodeCache.push_back(SNode(nodeIndex	));
    }
  }

  // make pairs and sort the pairs
  NodePairVector nodePairVector;
  for (unsigned iNode1 = 0 ; iNode1 < buildingNodeCache.size() ; ++iNode1)
  {
    SNode& node1 = buildingNodeCache[iNode1];
    for (unsigned iNode2 = iNode1 + 1 ; iNode2 < buildingNodeCache.size() ; ++iNode2)
    {
      SNode& node2 = buildingNodeCache[iNode2];
      if (node1.nodeIndex == node2.nodeIndex)
        continue;

      float distSq = (m_pGraph->GetNodeManager().GetNode(node2.nodeIndex)->GetPos() - m_pGraph->GetNodeManager().GetNode(node1.nodeIndex)->GetPos()).GetLengthSquared();
      if (distSq < maxDistMaxSq)
          nodePairVector.push_back(SNodePair(&node1, &node2, distSq));
    }
  }
  std::sort(nodePairVector.begin(), nodePairVector.end(), NodePairDistLess);

  // make connections
  unsigned nPairs = nodePairVector.size();
  for (unsigned iCurrentPair = 0 ; iCurrentPair < nPairs ; ++iCurrentPair)
  {
    const SNodePair& np = nodePairVector[iCurrentPair];

    float distSq = (m_pGraph->GetNodeManager().GetNode(np.pNode2->nodeIndex)->GetPos() - m_pGraph->GetNodeManager().GetNode(np.pNode1->nodeIndex)->GetPos()).GetLengthSquared();
    unsigned minNLinks = min(np.pNode1->links.size(), np.pNode2->links.size());
    if (distSq > maxDistNormalSq && minNLinks > nLinksForMaxDistMax)
      continue;

    if (CheckForExistingLink(*np.pNode1, *np.pNode2))
      continue;

    Vec3 nodeOffset(0, 0, 0.5f);
		m_checkWalkabilityState.state = SCheckWalkabilityState::CWS_NEW_QUERY;
		m_checkWalkabilityState.numIterationsPerCheck = 1000000;
		m_checkWalkabilityState.computeHeightAlongTheLink = true;
		m_checkWalkabilityState.heightAlongTheLink.resize(0);
		std::pair<bool,bool> result = CheckIfLinkSimple(m_pGraph->GetNodeManager().GetNode(np.pNode1->nodeIndex)->GetPos() + nodeOffset, 
			m_pGraph->GetNodeManager().GetNode(np.pNode2->nodeIndex)->GetPos() + nodeOffset, 0.0f, true, buildingPolygon, collisionEntities, & m_checkWalkabilityState);
    bool walkabilityFromTo = result.first;
    if (walkabilityFromTo)
    {
      bool linkIsSimple = result.second;
      SLink *junk1, *junk2;
      Connect(*np.pNode1, *np.pNode2, WLT_AUTO_PASS, junk1, junk2);
      junk1->simple = junk2->simple = linkIsSimple;
    }
  }

  // apply the connections
  UpdateGraph(buildingNodeCache, false);
}

//====================================================================
// GetNodeFromSortedNodes
//====================================================================
SNode * GetNodeFromSortedNodes(unsigned nodeIndex, BuildingNodeCache &nodes)
{
  BuildingNodeCache::iterator it = std::lower_bound(nodes.begin(), nodes.end(), SNode(nodeIndex), SNodePointerLess());
  return it == nodes.end() || it->nodeIndex != nodeIndex ? 0 : &(*it);
}

//===================================================================
// ModifyNodeConnections
//===================================================================
void CWaypointHumanNavRegion::ModifyNodeConnections(unsigned nodeIndex1, EAICollisionEntities collisionEntities, unsigned link, bool adjustOriginalEditorLinks)
{
	GraphNode* pNode1 = m_pGraph->GetNodeManager().GetNode(nodeIndex1);
	unsigned nodeIndex2 = m_pGraph->GetLinkManager().GetNextNode(link);
  GraphNode *pNode2 = m_pGraph->GetNodeManager().GetNode(nodeIndex2);
  if (pNode2 < pNode1)
  {
    return;
  }
  else
  {
    float origRadius = m_pGraph->GetLinkManager().GetOrigRadius(link);
    EWaypointLinkType origType = SWaypointNavData::GetLinkTypeFromRadius(origRadius);
    float curRadius = m_pGraph->GetLinkManager().GetRadius(link);
    EWaypointLinkType curType = SWaypointNavData::GetLinkTypeFromRadius(curRadius);

    bool walkabilityFromTo;

    if (origType == WLT_EDITOR_IMPASS)
    {
      // don't over-ride editor impassability
      walkabilityFromTo = false;
    }
    else
    {
      const SpecialArea *sa = GetAISystem()->GetSpecialArea(pNode1->GetWaypointNavData()->nBuildingID);
      if (!sa)
        return;
      const ListPositions	& buildingPolygon = sa->GetPolygon();

      Vec3 nodeOffset(0, 0, 0.5f);
      walkabilityFromTo = CheckWalkability(
        pNode1->GetPos() + nodeOffset, 
        pNode2->GetPos() + nodeOffset, 
        0.0f, true, buildingPolygon, collisionEntities,
				m_pGraph->GetCachedPassabilityResult(link, true));
    }

    if ( (walkabilityFromTo && curRadius < 0.0f) || (!walkabilityFromTo && curRadius > 0.0f) )
    {
      float newRadius = 0.0f;

      if (curType == WLT_AUTO_IMPASS || curType == WLT_AUTO_PASS)
      {
        newRadius = walkabilityFromTo ? WLT_AUTO_PASS : WLT_AUTO_IMPASS;
        unsigned linkOneTwo = 0;
        unsigned linkTwoOne = 0;
        m_pGraph->Connect(nodeIndex1, nodeIndex2, newRadius, newRadius, &linkOneTwo, &linkTwoOne);
        if (origType == WLT_AUTO_PASS && adjustOriginalEditorLinks)
        {
          if (linkOneTwo)
            m_pGraph->GetLinkManager().SetRadius(linkOneTwo, newRadius);
          if (linkTwoOne)
            m_pGraph->GetLinkManager().SetRadius(linkTwoOne, newRadius);
        }
      }
      else if (curType == WLT_EDITOR_IMPASS || curType == WLT_EDITOR_PASS)
      {
        newRadius = walkabilityFromTo ? WLT_EDITOR_PASS : WLT_EDITOR_IMPASS;
        m_pGraph->Connect(nodeIndex1, nodeIndex2, newRadius, newRadius);
      }
    } // walkability has changed 
  } // link pointer
}

//====================================================================
// ModifyConnections
//====================================================================
void CWaypointHumanNavRegion::ModifyConnections(const struct SpecialArea *pArea, EAICollisionEntities collisionEntities, bool adjustOriginalEditorLinks)
{
  CAllNodesContainer& allNodes = m_pGraph->GetAllNodes();
  CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_WAYPOINT_HUMAN);
  while (unsigned nodeIndex = it.Increment())
  {
		GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);
    if (pNode->GetWaypointNavData()->nBuildingID == pArea->nBuildingID)
    {
      for (unsigned link = pNode->firstLinkIndex; link; link = m_pGraph->GetLinkManager().GetNextLink(link))
      {
				unsigned otherIndex = m_pGraph->GetLinkManager().GetNextNode(link);
				ModifyNodeConnections(nodeIndex, collisionEntities, link, adjustOriginalEditorLinks);
      }
    }
  }
}

// NOTE Mai 6, 2007: <pvl> my dbg stuff - keeping it in for now, might be
// useful in future
/*
#include <cstdio>

class ModConnTimer {
		CTimeValue mStart;
		static float sAccumTime;
		static int sInvocations;
		static FILE * sFile;
public:
		ModConnTimer()
		{
			mStart = gEnv->pTimer->GetAsyncTime();
			++sInvocations;
			if (sFile == 0) sFile = fopen ("mrd", "w");
		}
		~ModConnTimer()
		{
			CTimeValue end = gEnv->pTimer->GetAsyncTime();
			sAccumTime += (end - mStart).GetMilliSeconds();

			fprintf (sFile, "modconn ran for %f ms\n", (end-mStart).GetMilliSeconds());
			fflush (sFile);
		}
		static void Reset ()
		{
			fprintf (sFile, "%f - %d\n", sAccumTime, sInvocations);
			fflush (sFile);
			sAccumTime = 0.0f;
			sInvocations = 0;
		}
};

float ModConnTimer::sAccumTime = 0.0f;
int ModConnTimer::sInvocations = 0;
FILE * ModConnTimer::sFile = 0;
*/

//====================================================================
// ModifyConnections
//====================================================================
void CWaypointHumanNavRegion::ModifyConnections(const CCalculationStopper& stopper, EAICollisionEntities collisionEntities)
{
//  ModConnTimer c;

  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  static int numIterPerWalkabilityCheck = 4;

//  static unsigned firstNode;
//  static bool firstNodeLeft = false;

  CAllNodesContainer& allNodes = m_pGraph->GetAllNodes();
  if (!m_currentNodeIt)
  {
    m_currentNodeIt = new CAllNodesContainer::Iterator(allNodes, IAISystem::NAV_WAYPOINT_HUMAN);
		m_currentLink = (m_currentNodeIt->GetNode() ? m_pGraph->GetNodeManager().GetNode(m_currentNodeIt->GetNode())->firstLinkIndex : 0);
    m_checkWalkabilityState.state = SCheckWalkabilityState::CWS_NEW_QUERY;
    m_checkWalkabilityState.numIterationsPerCheck = numIterPerWalkabilityCheck;

//    firstNode = m_currentNodeIt->GetNode();
  }

  unsigned origNodeIndex = 0;
  bool origNodeLeft = false;

  do
  {
/*
		if (m_currentNodeIt->GetNode() == firstNode && firstNodeLeft) {
			c.Reset();
			firstNodeLeft = false;
		}
*/

    unsigned nodeIndex1 = m_currentNodeIt->GetNode();
    if (!nodeIndex1)
    {
      m_checkWalkabilityState.state = SCheckWalkabilityState::CWS_NEW_QUERY;
      m_currentNodeIt->Reset();
      nodeIndex1 = m_currentNodeIt->GetNode();
			m_currentLink = (nodeIndex1 ? m_pGraph->GetNodeManager().GetNode(nodeIndex1)->firstLinkIndex : 0);
      if (!nodeIndex1)
        return;
    }

    // prevent sitting in a wasteful loop if there's no more to do
    if (!origNodeIndex)
      origNodeIndex = nodeIndex1;
    else if (origNodeIndex == nodeIndex1 && origNodeLeft)
      return;

		GraphNode* pNode1 = m_pGraph->GetNodeManager().GetNode(nodeIndex1);

    int nBuildingID1 = pNode1->GetWaypointNavData()->nBuildingID;
		const SpecialArea *sa1 = (nBuildingID1!=-1) ? GetAISystem()->GetSpecialArea(nBuildingID1) : 0;

    if (!m_currentLink || !sa1)
    {
      m_checkWalkabilityState.state = SCheckWalkabilityState::CWS_NEW_QUERY;
      (void) m_currentNodeIt->Increment(); // may return 0
			if (m_currentNodeIt->GetNode())
	      m_currentLink = m_pGraph->GetNodeManager().GetNode(m_currentNodeIt->GetNode())->firstLinkIndex;;
      origNodeLeft = true;
//			firstNodeLeft = true;
    }
    else
    {
      unsigned link = m_currentLink;
			unsigned nodeIndex2 = m_pGraph->GetLinkManager().GetNextNode(link);
      GraphNode *pNode2 = m_pGraph->GetNodeManager().GetNode(nodeIndex2);

      if (pNode2->navType != IAISystem::NAV_WAYPOINT_HUMAN)
      {
        m_currentLink = m_pGraph->GetLinkManager().GetNextLink(m_currentLink);
        continue;
      }

      int nBuildingID2 = pNode2->GetWaypointNavData()->nBuildingID;
			const SpecialArea *sa2 = (nBuildingID2!=-1) ? GetAISystem()->GetSpecialArea(nBuildingID2) : 0;
      if (!sa2)
      {
        m_currentLink = m_pGraph->GetLinkManager().GetNextLink(m_currentLink);
        continue;
      }

      bool auto1 = sa1->waypointConnections == WPCON_DESIGNER_PARTIAL || sa1->waypointConnections == WPCON_AUTO_PARTIAL;
      bool auto2 = sa2->waypointConnections == WPCON_DESIGNER_PARTIAL || sa2->waypointConnections == WPCON_AUTO_PARTIAL;

      // only check one way for each pair
      // also only check if at least one is auto
      if (pNode2 < pNode1 || (!auto1 && !auto2))
      {
        m_currentLink = m_pGraph->GetLinkManager().GetNextLink(m_currentLink); // only check one way for each pair
        continue;
      }
      else
      {
        float origRadius = m_pGraph->GetLinkManager().GetOrigRadius(link);
        EWaypointLinkType origType = SWaypointNavData::GetLinkTypeFromRadius(origRadius);
        float curRadius = m_pGraph->GetLinkManager().GetRadius(link);
        EWaypointLinkType curType = SWaypointNavData::GetLinkTypeFromRadius(curRadius);

        bool walkabilityFromTo;

        if (origType == WLT_EDITOR_IMPASS)
        {
          // don't over-ride editor impassability
          walkabilityFromTo = false;
        }
        else
        {
          float distToPlayerSq = 0.0f;
          if (GetAISystem()->GetPlayer())
          {
            Vec3 playerPos = GetAISystem()->GetPlayer()->GetPos();
            Vec3 delta1 = pNode1->GetPos() - playerPos;
            Vec3 delta2 = pNode2->GetPos() - playerPos;
            delta1.z *= 3.0f;
            delta2.z *= 3.0f;
            distToPlayerSq = min(delta1.GetLengthSquared(), delta2.GetLengthSquared());
          }
          static float criticalDistToPlayerSq = square(50.0f);
          if (distToPlayerSq > criticalDistToPlayerSq)
          {
            walkabilityFromTo = m_pGraph->GetLinkManager().GetRadius(link) > 0;
            m_checkWalkabilityState.state = SCheckWalkabilityState::CWS_NEW_QUERY;
          }
          else
          {
            const ListPositions	& buildingPolygon = sa1->GetPolygon();

            // the state should be OK from the last call
moreCalcs:
            Vec3 nodeOffset(0, 0, 0.5f);
            AIAssert(m_checkWalkabilityState.state != SCheckWalkabilityState::CWS_RESULT);
            walkabilityFromTo = CheckPassability(link,
              pNode1->GetPos() + nodeOffset, 
              pNode2->GetPos() + nodeOffset, 
              0.0f, true, buildingPolygon, collisionEntities,
              m_pGraph->GetCachedPassabilityResult(link, true),
              &m_checkWalkabilityState);
            if (m_checkWalkabilityState.state == SCheckWalkabilityState::CWS_PROCESSING)
            {
              if (stopper.ShouldCalculationStop())
                return;
              goto moreCalcs;
            }
            m_checkWalkabilityState.state = SCheckWalkabilityState::CWS_NEW_QUERY;
          }
        }

        if ( (walkabilityFromTo && curRadius < 0.0f) || (!walkabilityFromTo && curRadius > 0.0f) )
        {
          float newRadius = 0.0f;

          if (curType == WLT_AUTO_IMPASS || curType == WLT_AUTO_PASS)
          {
            newRadius = walkabilityFromTo ? WLT_AUTO_PASS : WLT_AUTO_IMPASS;
            m_pGraph->Connect(nodeIndex1, nodeIndex2, newRadius, newRadius);
          }
          else if (curType == WLT_EDITOR_IMPASS || curType == WLT_EDITOR_PASS)
          {
            newRadius = walkabilityFromTo ? WLT_EDITOR_PASS : WLT_EDITOR_IMPASS;
            m_pGraph->Connect(nodeIndex1, nodeIndex2, newRadius, newRadius);
          }
        } // link needs changing
      } // check other node pointer
      m_currentLink = m_pGraph->GetLinkManager().GetNextLink(m_currentLink);
    } // link index is valid
  } while (!stopper.ShouldCalculationStop());
}

#if 0
// UPDATE Apr 26, 2007: <pvl> not used ATM but I'm leaving it here for now,
// it could be useful if we decide to time-slice CalculateCost() after all
// (and get rid of ModifyConnections() altogether).
void CWaypointHumanNavRegion::UpdateLinkPassability(const CGraph * graph,
				unsigned nodeIndex1, unsigned link, unsigned nodeIndex2,
				CCalculationStopper& stopper)
{
	const GraphNode * node1 = graph->GetNodeManager().GetNode(nodeIndex1);
	const GraphNode * node2 = graph->GetNodeManager().GetNode(nodeIndex2);

	int nBuildingID1 = node1->GetWaypointNavData()->nBuildingID;
	const SpecialArea *sa1 = (nBuildingID1!=-1) ? GetAISystem()->GetSpecialArea(nBuildingID1) : 0;
	if (!sa1)
	{
		// TODO Apr 19, 2007: <pvl> this is probably a fatal inconsistency in
		// pathfinding data - if so, how to deal with it?  Is there a better way
		// than to silently ignore it?
		return;
	}

	int nBuildingID2 = node2->GetWaypointNavData()->nBuildingID;
	const SpecialArea *sa2 =  (nBuildingID2!=-1) ?GetAISystem()->GetSpecialArea(nBuildingID2) : 0;
	if (!sa2)
	{
		// TODO Apr 19, 2007: <pvl> this is probably a fatal inconsistency in
		// pathfinding data - if so, how to deal with it?  Is there a better way
		// than to silently ignore it?
		return;
	}

	bool auto1 = sa1->waypointConnections == WPCON_DESIGNER_PARTIAL || sa1->waypointConnections == WPCON_AUTO_PARTIAL;
	bool auto2 = sa2->waypointConnections == WPCON_DESIGNER_PARTIAL || sa2->waypointConnections == WPCON_AUTO_PARTIAL;

	// only check one way for each pair
	// also only check if at least one is auto
	if (node2 < node1 || (!auto1 && !auto2))
	{
		return;
	}

	float origRadius = m_pGraph->GetLinkManager().GetOrigRadius(link);
	EWaypointLinkType origType = SWaypointNavData::GetLinkTypeFromRadius(origRadius);
	float curRadius = m_pGraph->GetLinkManager().GetRadius(link);
	EWaypointLinkType curType = SWaypointNavData::GetLinkTypeFromRadius(curRadius);

	bool walkabilityFromTo;

	if (origType == WLT_EDITOR_IMPASS)
	{
		// don't over-ride editor impassability
		walkabilityFromTo = false;
	}
	else
	{
		float distToPlayerSq = 0.0f;
		if (GetAISystem()->GetPlayer())
		{
			Vec3 playerPos = GetAISystem()->GetPlayer()->GetPos();
			Vec3 delta1 = node1->GetPos() - playerPos;
			Vec3 delta2 = node2->GetPos() - playerPos;
			delta1.z *= 3.0f;
			delta2.z *= 3.0f;
			distToPlayerSq = min(delta1.GetLengthSquared(), delta2.GetLengthSquared());
		}
		static float criticalDistToPlayerSq = square(50.0f);
		if (distToPlayerSq > criticalDistToPlayerSq)
		{
			walkabilityFromTo = m_pGraph->GetLinkManager().GetRadius(link) > 0;
			m_checkWalkabilityState.state = SCheckWalkabilityState::CWS_NEW_QUERY;
		}
		else
		{
			Vec3 nodeOffset(0, 0, 0.5f);
			if (m_pGraph->GetLinkManager().IsSimple(link))
			{
				walkabilityFromTo = CheckWalkabilitySimple (
					node1->GetPos() + nodeOffset,
					node2->GetPos() + nodeOffset,
					0.0f,
					EAICollisionEntities (ent_static | ent_sleeping_rigid | ent_rigid | ent_ignore_noncolliding | ent_living)
					);
			}
			else
			{
				const ListPositions	& buildingPolygon = sa1->GetPolygon();

				// the state should be OK from the last call
moreCalcs:
				AIAssert(m_checkWalkabilityState.state != SCheckWalkabilityState::CWS_RESULT);
				walkabilityFromTo = CheckWalkability(
					node1->GetPos() + nodeOffset, 
					node2->GetPos() + nodeOffset, 
					0.0f, true, buildingPolygon, EAICollisionEntities (ent_static | ent_sleeping_rigid | ent_rigid | ent_ignore_noncolliding | ent_living),
					m_pGraph->GetCachedPassabilityResult(link, true),
					&m_checkWalkabilityState);
				if (m_checkWalkabilityState.state == SCheckWalkabilityState::CWS_PROCESSING)
				{
					if (stopper.ShouldCalculationStop())
						return;
					goto moreCalcs;
				}
				m_checkWalkabilityState.state = SCheckWalkabilityState::CWS_NEW_QUERY;
			}
		}
	}

	if ( (walkabilityFromTo && curRadius < 0.0f) || (!walkabilityFromTo && curRadius > 0.0f) )
	{
		float newRadius = 0.0f;

		if (curType == WLT_AUTO_IMPASS || curType == WLT_AUTO_PASS)
		{
			newRadius = walkabilityFromTo ? WLT_AUTO_PASS : WLT_AUTO_IMPASS;
			m_pGraph->Connect(nodeIndex1, nodeIndex2, newRadius, newRadius);
		}
		else if (curType == WLT_EDITOR_IMPASS || curType == WLT_EDITOR_PASS)
		{
			newRadius = walkabilityFromTo ? WLT_EDITOR_PASS : WLT_EDITOR_IMPASS;
			m_pGraph->Connect(nodeIndex1, nodeIndex2, newRadius, newRadius);
		}
	}
}
#endif


//====================================================================
// ReconnectWaypointNodesInBuilding
//====================================================================
void CWaypointHumanNavRegion::ReconnectWaypointNodesInBuilding(int nBuildingID)
{
	assert(nBuildingID!=-1);

  const SpecialArea* sa = GetAISystem()->GetSpecialArea(nBuildingID);
  if (!sa)
    return;

  // first remove all auto-links - leave designer links behind
  switch (sa->waypointConnections)
  {
  case WPCON_DESIGNER_NONE:
  case WPCON_DESIGNER_PARTIAL:
  case WPCON_AUTO_NONE:
  case WPCON_AUTO_PARTIAL:
    RemoveAutoLinksInBuilding(nBuildingID);
    break;
  default:
    AIError("Unhandled waypointConnections value %d", sa->waypointConnections);
    return;
  }

  // add our connections - these will set the original radius
  float maxDistNormal = sa->fNodeAutoConnectDistance;
  float maxDistMax = 2.0f * maxDistNormal;
  static unsigned nLinksForMaxDist = 2;
  switch (sa->waypointConnections)
  {
  case WPCON_AUTO_NONE:
      ConnectNodes(sa, AICE_STATIC, maxDistNormal, maxDistMax, nLinksForMaxDist);
      break;
  case WPCON_AUTO_PARTIAL:
      ConnectNodes(sa, AICE_STATIC, maxDistNormal, maxDistMax, nLinksForMaxDist);
      break;
  default: break;
  }

  // now maybe modify the radius
  switch (sa->waypointConnections)
  {
  case WPCON_DESIGNER_PARTIAL:
  case WPCON_AUTO_PARTIAL:
    ModifyConnections(sa, /*AICE_ALL_EXCEPT_TERRAIN_AND_STATIC*/AICE_ALL, true);
    break;
  default: break;
  }
}

//====================================================================
// Update
//====================================================================
void CWaypointHumanNavRegion::Update(CCalculationStopper& stopper)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  ModifyConnections(stopper, /*AICE_ALL_EXCEPT_TERRAIN_AND_STATIC*/AICE_ALL);
}

//====================================================================
// Reset
//====================================================================
void CWaypointHumanNavRegion::Reset(IAISystem::EResetReason reason)
{
}

//===================================================================
// GetTeleportPosition
//===================================================================
bool CWaypointHumanNavRegion::GetTeleportPosition(const Vec3 &pos, Vec3 &teleportPos, const char *requesterName)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  teleportPos.zero();

  IVisArea *pGoalArea; 
  int	nBuildingID;
  GetAISystem()->CheckNavigationType(pos, nBuildingID, pGoalArea, IAISystem::NAV_WAYPOINT_HUMAN);

  if (nBuildingID < 0)
  {
    AIWarning("CWaypointHumanNavRegion::GetTeleportPosition found bad building ID = %d (%s) for %s (%5.2f, %5.2f, %5.2f)", 
      nBuildingID, GetAISystem()->GetNavigationShapeName(nBuildingID), requesterName, pos.x, pos.y, pos.z);
    return false;
  }

  const SpecialArea *sa = GetAISystem()->GetSpecialArea(nBuildingID);
  if (!sa)
  {
    AIWarning("CWaypointHumanNavRegion::GetTeleportPosition found no area for %s (%5.2f, %5.2f, %5.2f)", 
      requesterName, pos.x, pos.y, pos.z);
    return false;
  }

  if (sa->fNodeAutoConnectDistance < 0.0001f)
    AIWarning("CWaypointHumanNavRegion::GetTeleportPosition AI nav modifier %s has NodeAutoConnectDistance=0 - increase to a sensible value [design bug]", GetAISystem()->GetNavigationShapeName(nBuildingID));

  float totalDist = 5 * sa->fNodeAutoConnectDistance + 5.0f;

  typedef std::vector< std::pair<float, unsigned> > TNodes;
  static TNodes nodes;
  nodes.resize(0);

  // Most times we'll find the desired node close by
  CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
  allNodes.GetAllNodesWithinRange(nodes, pos, totalDist, IAISystem::NAV_WAYPOINT_HUMAN);
  std::vector< std::pair<float, unsigned> >::iterator it = std::remove_if(nodes.begin(), nodes.end(), SWrongBuildingRadius(m_pGraph->GetLinkManager(), m_pGraph->GetNodeManager(), nBuildingID, 0.4f));
  nodes.erase(it, nodes.end());

  static float zScale = 2.0f;
  if (nodes.empty())
  {
    AILogComment("CWaypointHumanNavRegion::GetTeleportPosition Unable to find node close to %s (%5.2f, %5.2f, %5.2f) - checking all nodes from building id %d (%s)",
      requesterName, pos.x, pos.y, pos.z, nBuildingID, GetAISystem()->GetNavigationShapeName(nBuildingID));

    CAllNodesContainer::Iterator nit(allNodes, IAISystem::NAV_WAYPOINT_HUMAN);
    while (unsigned currentIndex = nit.Increment())
    {
			GraphNode* pCurrent = m_pGraph->GetNodeManager().GetNode(currentIndex);
			if (pCurrent->GetWaypointNavData()->nBuildingID == nBuildingID && !GetAISystem()->ExitNodeImpossible(m_pGraph->GetLinkManager(), pCurrent, 0.4f))
      {
        float dist = Distance::Point_Point(pos, pCurrent->GetPos());
        nodes.push_back(std::make_pair(dist, currentIndex)); // dist calculated later
      }
    }
  }

  if (nodes.empty())
  {
    AIWarning("CWaypointHumanNavRegion::GetTeleportPosition No nodes found for waypoint area containing %s (%5.2f, %5.2f, %5.2f) building id %d (%s) [design bug]", 
      requesterName, pos.x, pos.y, pos.z, nBuildingID, GetAISystem()->GetNavigationShapeName(nBuildingID));
    return false;
  }

  std::sort(nodes.begin(), nodes.end());

  for (TNodes::const_iterator it = nodes.begin() ; it != nodes.end() ; ++it)
  {
		unsigned nodeIndex = it->second;
    const GraphNode *pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);
    Vec3 nodePos = pNode->GetPos();
    if (!CheckBodyPos(nodePos, AICE_ALL))
      continue;
    if (GetAISystem()->WouldHumanBeVisible(nodePos, false))
      continue;
    teleportPos = nodePos;
    return true;
  }
  return false;
}

/*
void CWaypointHumanNavRegion::CheckLinkLengths() const
{
	LinkIterator linkIt = CreateLinkIterator();
	for ( ; linkIt; ++linkIt)
	{
		unsigned int linkIndex = *linkIt;
		unsigned nodeIndex0 = m_pGraph->GetLinkManager().GetPrevNode(linkIndex);
		unsigned nodeIndex1 = m_pGraph->GetLinkManager().GetNextNode(linkIndex);
		GraphNode * node0 = m_pGraph->GetNodeManager().GetNode(nodeIndex0);
		GraphNode * node1 = m_pGraph->GetNodeManager().GetNode(nodeIndex1);
		AIWarning ("Link length %f\n", (node0->GetPos() - node1->GetPos()).len());
	}
}
*/

CWaypointHumanNavRegion::LinkIterator::LinkIterator (const CWaypointHumanNavRegion * wayptRegion) :
		m_pGraph (wayptRegion->m_pGraph),
		m_currNodeIt(new CAllNodesContainer::Iterator(m_pGraph->GetAllNodes(), IAISystem::NAV_WAYPOINT_HUMAN)),
		m_currLinkIndex (m_pGraph->GetNodeManager().GetNode(m_currNodeIt->GetNode())->firstLinkIndex)
{
	FindNextWaypointLink();
}

CWaypointHumanNavRegion::LinkIterator::LinkIterator (const LinkIterator & rhs) :
		m_pGraph (rhs.m_pGraph),
		m_currNodeIt (new CAllNodesContainer::Iterator(*rhs.m_currNodeIt)),
		m_currLinkIndex (rhs.m_currLinkIndex)
{
}

CWaypointHumanNavRegion::LinkIterator & CWaypointHumanNavRegion::LinkIterator::operator++ ()
{
	m_currLinkIndex = m_pGraph->GetLinkManager().GetNextLink(m_currLinkIndex);
	FindNextWaypointLink();
	return *this;
}

/**
 * This function finds the next waypoint link, starting with and including the
 * current value of m_currLinkIndex.  m_currLinkIndex should denote
 * a valid link upon the call even though not necessarily a waypoint one.
 * Note that if m_currLinkIndex does point to a waypoint link, this function
 * does nothing.
 */
void CWaypointHumanNavRegion::LinkIterator::FindNextWaypointLink ()
{
	unsigned int currNodeIndex = m_currNodeIt->GetNode();
	bool gotSuitableLink = false;

	while ( ! gotSuitableLink)
	{
		while (m_currLinkIndex == 0)
		{
			m_currNodeIt->Increment();
			currNodeIndex = m_currNodeIt->GetNode();
			if (0 == currNodeIndex)
				return;		// reached the final node, the iteration is over
			m_currLinkIndex = m_pGraph->GetNodeManager().GetNode(currNodeIndex)->firstLinkIndex;
		}

		unsigned nextNodeIndex = m_pGraph->GetLinkManager().GetNextNode(m_currLinkIndex);

		const GraphNode * currNode = m_pGraph->GetNodeManager().GetNode(currNodeIndex);
		const GraphNode * nextNode = m_pGraph->GetNodeManager().GetNode(nextNodeIndex);

		if (nextNode->navType != IAISystem::NAV_WAYPOINT_HUMAN || currNode > nextNode /*check in one direction only*/)
		{
			m_currLinkIndex = m_pGraph->GetLinkManager().GetNextLink(m_currLinkIndex);
			continue;
		}

		gotSuitableLink = true;
	}
}
