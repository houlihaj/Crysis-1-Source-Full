#include "StdAfx.h"
#include "TriangularNavRegion.h"
#include "AICollision.h"
#include "CAISystem.h"
#include "ISystem.h"
#include "I3DEngine.h"
#include "ISerialize.h"
#include "IConsole.h"
#include "IRenderer.h"

#include <limits>

//====================================================================
// CTriangularNavRegion
//====================================================================
CTriangularNavRegion::CTriangularNavRegion(CGraph* pGraph)
{
  AIAssert(pGraph);
  m_pGraph = pGraph;
  m_calculateGraphNodesInDynamicAreas = true;
  m_currentGraphNodeIndex = 0;
}

//====================================================================
// UglifyPath
//====================================================================
void CTriangularNavRegion::UglifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath, 
                                      const Vec3& startPos, const Vec3& startDir, 
                                      const Vec3& endPos, const Vec3 & endDir)
{
  outPath.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, startPos));
  for(VectorConstNodeIndices::const_iterator itrCurNode=inPath.begin() ; itrCurNode != inPath.end() ; ++itrCurNode)
  {
    const GraphNode& curNode=*m_pGraph->GetNodeManager().GetNode(*itrCurNode);
    outPath.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, curNode.GetPos()));
  }
  outPath.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, endPos));
}

//====================================================================
// SimplifiedLink
//====================================================================
class SimplifiedLink
{
public:
  Vec3	left;
  Vec3	right;
  SimplifiedLink(const Vec3& _left, const Vec3& _right) : left(_left), right(_right) {}
  SimplifiedLink() {}
};

typedef std::vector< SimplifiedLink > tLinks;
static tLinks debugLinks;

//====================================================================
// BeautifyPath
//====================================================================
void CTriangularNavRegion::BeautifyPath(const VectorConstNodeIndices& inPath, TPathPoints& outPath, 
                                        const Vec3& startPos, const Vec3& startDir, 
                                        const Vec3& endPos, const Vec3 & endDir,
                                        float radius,
                                        const AgentMovementAbility & movementAbility,
                                        const NavigationBlockers& navigationBlockers)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );
  float extraR = GetAISystem()->m_cvExtraRadiusDuringBeautification->GetFVal();
  float extraFR = GetAISystem()->m_cvExtraForbiddenRadiusDuringBeautification->GetFVal();
  radius += extraR;

  static float extraNonCollRadius = 0.0f;
  if (movementAbility.pathfindingProperties.minWaterDepth > 0.0f)
    extraNonCollRadius = 20.0f; // for boats etc
  radius += extraNonCollRadius;

  if (inPath.size() <= 1)
  {
    return;
  }

  static tLinks links;
  links.resize(0);

  Vec3 _start(startPos);
  _start.z = 0;
  links.push_back(SimplifiedLink(_start, _start));
  VectorConstNodeIndices::const_iterator itNodes, itNodesEnd = inPath.end();
  for (itNodes = inPath.begin(); itNodes != itNodesEnd; ++itNodes)
  {
    unsigned nodeIndex = *itNodes;
		GraphNode* node = m_pGraph->GetNodeManager().GetNode(nodeIndex);

    AIAssert(node->navType == IAISystem::NAV_TRIANGULAR);

    // _start is _supposed_ to be inside the first node anyway. If it's not, then
    // if we assumed that it is, it would generate a backwards segment, and this
    // messes up the subsequent beautification. So - skip the first node anyway - the link to
    // the next node will always be safe.
    if (itNodes == inPath.begin())
      continue;

    VectorConstNodeIndices::const_iterator itNext = itNodes;
    ++itNext;
    if (itNext == itNodesEnd)
      break;

    unsigned nodeNextIndex = *itNext;
    if (m_pGraph->GetNodeManager().GetNode(nodeNextIndex)->GetTriangularNavData()->vertices.empty())
      continue;

		unsigned link;
    for (link = node->firstLinkIndex; link; link = m_pGraph->GetLinkManager().GetNextLink(link))
      if (m_pGraph->GetLinkManager().GetNextNode(link) == nodeNextIndex)
        break;
    if (node->GetTriangularNavData()->vertices.empty() ||
      (m_pGraph->GetLinkManager().GetEndIndex(link) >= node->GetTriangularNavData()->vertices.size()) ||
      (m_pGraph->GetLinkManager().GetStartIndex(link) >= node->GetTriangularNavData()->vertices.size()) )
    {
      AIWarning("Error during path beautification - check for errors during AI loading");
      continue;
    }
    // obstacles seem to be backwards...
    const ObstacleData &rightObst = GetAISystem()->m_VertexList.GetVertex(node->GetTriangularNavData()->vertices[m_pGraph->GetLinkManager().GetEndIndex(link)]);
    const ObstacleData &leftObst = GetAISystem()->m_VertexList.GetVertex(node->GetTriangularNavData()->vertices[m_pGraph->GetLinkManager().GetStartIndex(link)]);
    Vec3 left = leftObst.vPos;
    Vec3 right = rightObst.vPos;
    Vec3 nodePos = node->GetPos();
    nodePos.z = 0.0f;
    left.z = 0.0f;
    right.z = 0.0f;
    Vec3 dir = left - right;
    Vec3 center = m_pGraph->GetLinkManager().GetEdgeCenter(link);
    center.z = 0.0f;
    Vec3 delta = center - nodePos;
    float cross = dir.y*delta.x - dir.x*delta.y;
    // if this triangle is next to forbidden, then increase the radius, but if neither is 
    // collidable then set radius = 0 so that we "hug" it
    float workingRadius = radius;
    float linkRadius= m_pGraph->GetLinkManager().GetRadius(link);
    float ptDist = Distance::Point_Point2D(left, right);
    if (linkRadius > ptDist)
      linkRadius = ptDist;

    if (!leftObst.bCollidable && !rightObst.bCollidable)
    {
      workingRadius = extraNonCollRadius;
      dir.SetLength(linkRadius - workingRadius);
    }
    else
    {
      // Danny todo - this check must be very slow - speed it up!
      if (GetAISystem()->IsPointOnForbiddenEdge(left, 0.0001f, 0, 0, false) || GetAISystem()->IsPointOnForbiddenEdge(right, 0.0001f, 0, 0, false))
        workingRadius += extraFR;
      if (workingRadius > linkRadius)
        workingRadius = linkRadius;
      dir.SetLength(linkRadius - workingRadius);
    }
    left = center + dir;
    right = center - dir;

    // The radius of normal collidable things will have been incorporated into the link radius. For non-collidable things
    // we want to avoid them now (e.g. to avoid bushes) even though they shouldn't affect the unbeautified path - i.e.
    // they weren't included in the pass actually - radius. So, tweak the radius/pass center.
    Vec3 dirDir = dir.GetNormalizedSafe();
    if (!leftObst.bCollidable && leftObst.fApproxRadius > 0.0f)
    {
      left = leftObst.vPos - leftObst.fApproxRadius * dirDir;
    }
    if (!rightObst.bCollidable && rightObst.fApproxRadius > 0.0f)
    {
      right = rightObst.vPos + rightObst.fApproxRadius * dirDir;
    }

    if (cross > 0.f)
      links.push_back(SimplifiedLink(left, right));
    else
      links.push_back(SimplifiedLink(right, left));
    _start = center;
  }
  _start = endPos;
  _start.z = 0;
  links.push_back(SimplifiedLink(_start, _start));

  bool more = true;
  while (more)
  {
    more = false;
    tLinks::iterator i1, i2, i3, iEnd = links.end();
    for (i1 = links.begin(); i1 != iEnd; )
    {
      i2 = i1;
      ++i2;
      i3 = i2;
      ++i3;
      if (i3 == iEnd)
        break;

      Vec3& i2left = i2->left;
      Vec3& i2right = i2->right;
      if (i2left == i2right)
      {
        ++i1;
        continue;
      }
      else if ((i2left-i2right).GetLengthSquared() < 0.5f)
      {
        i2left = i2right = (i2left+i2right)*0.5f;
        ++i1;
        continue;
      }

      Vec3& i1left = i1->left;
      Vec3& i1right = i1->right;
      Vec3& i3left = i3->left;
      Vec3& i3right = i3->right;

      Vec3 segAStart = i1left;
      Vec3 segBStart = i2left;
      Vec3 segADir = i3left - segAStart;
      Vec3 segBDir = i2right - segBStart;

      bool shortenLeft = false;

      Vec3 delta = segBStart - segAStart;
      float crossD = segADir.x*segBDir.y - segADir.y*segBDir.x;

      if ( fabs(crossD) > 0.01f )
      {
        // there is an intersection
        float crossDelta2 = delta.x*segADir.y - delta.y*segADir.x;
        float cut = crossDelta2/crossD;
        if (cut > 0.001f)
        {
          if (cut > 0.999f)
          {
            i2left = i2right;
            more = true;
            ++i1;
            continue;
          }
          else
          {
            i2left += segBDir * cut * 0.99f;
            more = true;
            shortenLeft = true;
          }
        }
      }

      segAStart = i1right;
      segBStart = i2right;
      segADir = i3right - segAStart;
      segBDir = i2left - segBStart;

      delta = segBStart - segAStart;
      crossD = segADir.x*segBDir.y - segADir.y*segBDir.x;

      if ( fabs(crossD) > 0.01f )
      {
        // there is an intersection
        float crossDelta2 = delta.x*segADir.y - delta.y*segADir.x;
        float cut = crossDelta2/crossD;
        if (cut > 0.001f)
        {
          if (cut > 0.999f)
          {
            i2right = i2left;
            more = true;
          }
          else if (shortenLeft)
          {
            // discard whole segment if both sides should be shortened
            links.erase(i2);
            continue;
          }
          else
          {
            i2right += segBDir * cut * 0.99f;
            more = true;
          }
        }
      }
      ++i1;
    }
  }

  static bool doTidyup = true;
  if (doTidyup)
  {
    // At this point we can still have "corridors" where there is a sequence that starts with a 
    // co-located pair, and then after some spans finishes with a co-located pair.
    // If we identify those sequences then we can use the algorithm in AIPW3 (p 134) since (a)
    // the "corridor" will be basically a straight line (from the start to end of the sequence, which
    // may be unrelated to the start/end of the whole path) and (b) the AIPW algorithm only fails
    // if the path turns significfiantly away from the start/end.
    tLinks::iterator iEnd = links.end();
    for (tLinks::iterator iSeqStart= links.begin(); iSeqStart != iEnd; ++iSeqStart)
    {
      const SimplifiedLink &slStart = *iSeqStart;
      if (slStart.left.IsEquivalent(slStart.right))
        continue;

      int numDiff = 0;
      tLinks::iterator iSeqEnd;
      for (iSeqEnd = iSeqStart; iSeqEnd != iEnd; ++iSeqEnd)
      {
        const SimplifiedLink &slEnd = *iSeqEnd;
        if (slEnd.left.IsEquivalent(slEnd.right))
          break;
        ++numDiff;
      }

      AIAssert(iSeqEnd != iEnd);
      const SimplifiedLink &slEnd = *iSeqEnd;
      if (numDiff == 0)
        continue;

      Vec3 posEnd = slEnd.left;
      // slStart and slEnd represent the start and end of a corridor sequence
      for (tLinks::iterator i = iSeqStart-1 ; i != iSeqEnd ; ++i)
      {
        tLinks::iterator iNext = i+1;
        if (iNext == iSeqEnd)
          break;

        Lineseg pathSeg(i->left, posEnd);
        Lineseg line(iNext->left, iNext->right);

        float a, b;
        Intersect::Lineseg_Lineseg2D(pathSeg, line, a, b);
        Limit(b, 0.0f, 1.0f);

        iNext->left = line.GetPoint(b);
        iNext->right = iNext->left;
      }
      iSeqStart = iSeqEnd;
    }
  }

  const tLinks::iterator itLinksEnd = links.end();
  for (tLinks::iterator itLinks = links.begin() + 1; itLinks != itLinksEnd; ++itLinks)
  {
    const SimplifiedLink &link = *itLinks;
    Vec3	thePoint=(link.left+link.right) * 0.5f;
    thePoint.z = GetAISystem()->m_pSystem->GetI3DEngine()->GetTerrainElevation(thePoint.x, thePoint.y);
    outPath.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, thePoint));
  }

//  if (GetAISystem()->m_cvDebugDraw->GetIVal() != 0)
//    debugLinks = links;

  return;
}

//===================================================================
// DebugDrawPathLinks
//===================================================================
void CTriangularNavRegion::DebugDrawPathLinks(IRenderer *pRenderer)
{
  const tLinks::iterator itLinksEnd = debugLinks.end();
  ColorB col(24, 56, 129);
  for (tLinks::iterator itLinks = debugLinks.begin(); itLinks != itLinksEnd; ++itLinks)
  {
    SimplifiedLink link = *itLinks;
    link.left.z = GetISystem()->GetI3DEngine()->GetTerrainElevation(link.left.x, link.left.y);
    link.right.z = GetISystem()->GetI3DEngine()->GetTerrainElevation(link.right.x, link.right.y);
    pRenderer->GetIRenderAuxGeom()->DrawLine(link.left, col, link.right, col);
    pRenderer->GetIRenderAuxGeom()->DrawSphere(link.left, 0.2f, col);
    pRenderer->GetIRenderAuxGeom()->DrawSphere(link.right, 0.2f, col);
  }
}

//====================================================================
// GreedyStep1
// iterative function to quickly converge on the target position in the graph
//====================================================================
unsigned CTriangularNavRegion::GreedyStep1(unsigned beginIndex, const Vec3 & pos)
{
	GraphNode* pBegin = m_pGraph->GetNodeManager().GetNode(beginIndex);
  AIAssert(pBegin && pBegin->navType == IAISystem::NAV_TRIANGULAR);

  if (m_pGraph->PointInTriangle(pos,pBegin))
    return beginIndex;		// we have arrived

  m_pGraph->MarkNode(beginIndex);

  Vec3r triPos = pBegin->GetPos();
  for (unsigned link = pBegin->firstLinkIndex; link; link = m_pGraph->GetLinkManager().GetNextLink(link))
  {
		unsigned nextNodeIndex = m_pGraph->GetLinkManager().GetNextNode(link);
    GraphNode *pNextNode = m_pGraph->GetNodeManager().GetNode(nextNodeIndex);

    if (pNextNode->mark || pNextNode->navType != IAISystem::NAV_TRIANGULAR)
      continue;

    // cross to the next triangle if this edge splits space such that the current triangle and the
    // destination are on opposite sides.
    const ObstacleData &startObst = GetAISystem()->m_VertexList.GetVertex(pBegin->GetTriangularNavData()->vertices[m_pGraph->GetLinkManager().GetStartIndex(link)]);
    const ObstacleData &endObst = GetAISystem()->m_VertexList.GetVertex(pBegin->GetTriangularNavData()->vertices[m_pGraph->GetLinkManager().GetEndIndex(link)]);
    Vec3r edgeStart = startObst.vPos;
    Vec3r edgeEnd = endObst.vPos;
    Vec3r edgeDelta = edgeEnd - edgeStart;
    Vec3r edgeNormal(edgeDelta.y, -edgeDelta.x, 0.0f); // could point either way

    real dotDest = edgeNormal.Dot(pos - edgeStart);
    real dotTri = edgeNormal.Dot(triPos - edgeStart);
    if (sgn(dotDest) != sgn(dotTri))
    {
      return nextNodeIndex;
    }
  }
  return 0;
}

//===================================================================
// GreedyStep2
//===================================================================
unsigned CTriangularNavRegion::GreedyStep2(unsigned beginIndex, const Vec3 & pos)
{
	GraphNode* pBegin = m_pGraph->GetNodeManager().GetNode(beginIndex);
  AIAssert(pBegin && pBegin->navType == IAISystem::NAV_TRIANGULAR);

  if (m_pGraph->PointInTriangle(pos,pBegin))
    return beginIndex;		// we have arrived

  m_pGraph->MarkNode(beginIndex);

  real bestDot = -1.0;
  unsigned bestLink = 0;

  Vec3r dirToPos = pos - pBegin->GetPos();
  dirToPos.z = 0.0f;
  dirToPos.NormalizeSafe();

  for (unsigned link = pBegin->firstLinkIndex; link; link = m_pGraph->GetLinkManager().GetNextLink(link))
  {
		unsigned nextNodeIndex = m_pGraph->GetLinkManager().GetNextNode(link);
    const GraphNode *pNextNode = m_pGraph->GetNodeManager().GetNode(nextNodeIndex);

    if (pNextNode->mark || pNextNode->navType != IAISystem::NAV_TRIANGULAR)
      continue;

    const ObstacleData &startObst = GetAISystem()->m_VertexList.GetVertex(pBegin->GetTriangularNavData()->vertices[m_pGraph->GetLinkManager().GetStartIndex(link)]);
    const ObstacleData &endObst = GetAISystem()->m_VertexList.GetVertex(pBegin->GetTriangularNavData()->vertices[m_pGraph->GetLinkManager().GetEndIndex(link)]);
    Vec3r edgeStart = startObst.vPos;
    Vec3r edgeEnd = endObst.vPos;
    Vec3r edgeDelta = (edgeEnd - edgeStart);
    Vec3r edgeNormal(edgeDelta.y, -edgeDelta.x, 0.0f); // points out - assumes correct winding
    edgeNormal.NormalizeSafe();
    if (edgeNormal.Dot(edgeStart - pBegin->GetPos()) < 0.0f)
      edgeNormal *= -1.0f;
    else
      int junk = 0;

    real thisDot = dirToPos.Dot(edgeNormal);
    if (thisDot > bestDot)
    {
      bestLink = link;
      bestDot = thisDot;
    }
  }

  if (bestLink)
  {
    return m_pGraph->GetLinkManager().GetNextNode(bestLink);
  }
  else
  {
    return 0;
  }
}

//====================================================================
// GetGoodPositionInTriangle
//====================================================================
Vec3 CTriangularNavRegion::GetGoodPositionInTriangle(const GraphNode *pNode, const Vec3 &pos) const
{
  Vec3 newPos(pos);
  for (unsigned iVertex = 0 ; iVertex < pNode->GetTriangularNavData()->vertices.size() ; ++iVertex)
  {
    int obIndex = pNode->GetTriangularNavData()->vertices[iVertex];
    const ObstacleData& od = GetAISystem()->m_VertexList.GetVertex(obIndex);
    float distToObSq = Distance::Point_Point2DSq(newPos, od.vPos);
    if (distToObSq > 0.0001f && distToObSq < square(max(od.fApproxRadius, 0.0f)))
    {
      Vec3 dirToMove = newPos - od.vPos;
      dirToMove.z = 0.0f;
      dirToMove.Normalize();
      newPos = od.vPos + max(od.fApproxRadius, 0.0f) * dirToMove;
    }
  }
  return newPos;
}


//====================================================================
// GetEnclosing
//====================================================================
unsigned CTriangularNavRegion::GetEnclosing(const Vec3 &pos, float passRadius, unsigned startIndex, 
                                              float range, Vec3 * closestValid, bool returnSuspect, const char *requesterName)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  if (!m_pGraph->InsideOfBBox(pos))
  {
    AIWarning("CTriangularNavRegion::GetEnclosing (%5.2f %5.2f %5.2f) %s is outside of navigation bounding box",
      pos.x, pos.y, pos.z, requesterName);
    return 0;
  }

  m_pGraph->ClearMarks();

  if (!startIndex || (m_pGraph->GetNodeManager().GetNode(startIndex)->navType != IAISystem::NAV_TRIANGULAR))
  {
    typedef std::vector< std::pair<float, unsigned> > TNodes;
    static TNodes nodes;
    nodes.resize(0);
    // Most times we'll find the desired node close by
    CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
    allNodes.GetAllNodesWithinRange(nodes, pos, 15.0f, IAISystem::NAV_TRIANGULAR);
    if (!nodes.empty())
    {
      startIndex = nodes[0].second;
    }
    else
    {
      CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_TRIANGULAR);
      startIndex = it.Increment();
      if (!startIndex)
      {
        AIWarning("CTriangularNavRegion::GetEnclosing No triangular nodes in navigation");
        return 0;
      }
    }
  }

  AIAssert(startIndex && m_pGraph->GetNodeManager().GetNode(startIndex)->navType == IAISystem::NAV_TRIANGULAR);

  unsigned nextNodeIndex = startIndex;
  unsigned prevNodeIndex = 0;
  int iterations = 0;
  while (nextNodeIndex && prevNodeIndex != nextNodeIndex)
  {
    ++iterations;
    prevNodeIndex = nextNodeIndex;
    nextNodeIndex = GreedyStep1(prevNodeIndex, pos);
  }
  m_pGraph->ClearMarks();
  // plan B - should not get called very much
  if (!nextNodeIndex)
  {
    AILogProgress("CTriangularNavRegion::GetEnclosing (%5.2f %5.2f %5.2f %s radius = %5.2f) Resorting to slower algorithm", 
      pos.x, pos.y, pos.z, requesterName, passRadius);
    static int planBTimes = 0;
    ++planBTimes;
    nextNodeIndex = startIndex;
    prevNodeIndex = 0;
    iterations = 0;
    while (nextNodeIndex && prevNodeIndex != nextNodeIndex)
    {
      ++iterations;
      prevNodeIndex = nextNodeIndex;
      nextNodeIndex = GreedyStep2(prevNodeIndex, pos);
    }
    m_pGraph->ClearMarks();
  }
  // plan C - bad news if this gets called
  if(!nextNodeIndex)
  {
    AILogProgress("CTriangularNavRegion::GetEnclosing (%5.2f %5.2f %5.2f %s radius = %5.2f) Resorting to brute-force algorithm",
      pos.x, pos.y, pos.z, requesterName, passRadius);
    CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
    CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_TRIANGULAR);
    while (nextNodeIndex = it.Increment())
    {
      if (m_pGraph->PointInTriangle(pos, m_pGraph->GetNodeManager().GetNode(nextNodeIndex)))
        break;
      else
        nextNodeIndex = 0;
    }
  }

  if (nextNodeIndex && m_pGraph->PointInTriangle(pos, m_pGraph->GetNodeManager().GetNode(nextNodeIndex)))
  {
    if (closestValid)
      *closestValid = GetGoodPositionInTriangle(m_pGraph->GetNodeManager().GetNode(nextNodeIndex), pos);
    return nextNodeIndex; // or pPrevNode, they are the same
  }
  else
  {
    // silently fail if there's no navigation at all
    if (nextNodeIndex == m_pGraph->m_safeFirstIndex)
      return 0;
    AIWarning("CTriangularNavRegion::GetEnclosing DebugWalk bad result from looking for node containing %s position (%5.2f, %5.2f, %5.2f)", 
      requesterName, pos.x, pos.y, pos.z);
    return 0;
  }
}

//====================================================================
// CheckPassability
//====================================================================
bool CTriangularNavRegion::CheckPassability(const Vec3& from, const Vec3& to, float radius, const NavigationBlockers& navigationBlockers) const
{
  ListPositions boundary;
  return CheckWalkability(from, to, 0.0f, false, boundary, AICE_ALL);
}

//====================================================================
// Serialize
//====================================================================
void CTriangularNavRegion::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
  ser.BeginGroup("TriangularNavRegion");

  ser.EndGroup();
}

//====================================================================
// Reset
//====================================================================
void CTriangularNavRegion::Clear()
{
}

//====================================================================
// AttemptStraightPath
//====================================================================
float CTriangularNavRegion::AttemptStraightPath(TPathPoints &straightPathOut,
                                                const CHeuristic* pHeuristic, const Vec3& start, const Vec3& end, 
                                                unsigned startHintIndex,
                                                float maxCost,
                                                const NavigationBlockers& navigationBlockers, bool returnPartialPath)
{
  if (!pHeuristic)
    return -1.0f;

  const CPathfinderRequesterProperties* reqProperties = pHeuristic->GetRequesterProperties();
  if (!reqProperties)
    return -1.0f;

  unsigned startIndex = GetEnclosing(start, 0, startHintIndex);
	GraphNode* pStart = m_pGraph->GetNodeManager().GetNode(startIndex);
  if (!pStart || pStart->navType != IAISystem::NAV_TRIANGULAR)
    return -1.0f;
  if (pStart->GetTriangularNavData()->isForbidden)
    return -1.0f;

  unsigned endIndex = GetEnclosing(end, 0, startHintIndex);
	GraphNode* pEnd = m_pGraph->GetNodeManager().GetNode(endIndex);
  if (!pEnd || pEnd->navType != IAISystem::NAV_TRIANGULAR)
    return -1.0f;
  if (pEnd->GetTriangularNavData()->isForbidden)
    return -1.0f;

  if (pStart == pEnd)
  {
    straightPathOut.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, end));
    return (end - start).GetLength();
  }

  float extraR = GetAISystem()->m_cvExtraRadiusDuringBeautification->GetFVal();
  float extraFR = GetAISystem()->m_cvExtraForbiddenRadiusDuringBeautification->GetFVal();

  // so - start and end are triangular - need to walk from one to the other
  straightPathOut.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, start));
  unsigned currentNodeIndex = startIndex;
  SMarkClearer markClearer(m_pGraph);

  float pathCost = 0.0f;

  float heuristicZScale = 1.0f;
  if (pHeuristic->GetRequesterProperties())
    heuristicZScale *= pHeuristic->GetRequesterProperties()->properties.zScale;
  Vec3 heuristicZScaleVec(1, 1, heuristicZScale);

  while (true)
  {
    const Vec3& currentPos = straightPathOut.back().vPos;

    m_pGraph->MarkNode(currentNodeIndex);

    if (currentNodeIndex == startIndex)
    {
      Vec3 delta = start - currentPos;
      pathCost += delta.CompMul(heuristicZScaleVec).GetLength();
    }

    // check if we're there
    if (currentNodeIndex == endIndex)
    {
      Vec3 delta = end - currentPos;
      pathCost += delta.CompMul(heuristicZScaleVec).GetLength();
      straightPathOut.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, end));
      return pathCost;
    }

    // currentNode != pEnd

    // Find the triangle edge that we would cross if going to the destination
		unsigned link;
    for (link = m_pGraph->GetNodeManager().GetNode(currentNodeIndex)->firstLinkIndex; link; link = m_pGraph->GetLinkManager().GetNextLink(link))
    {
      if (m_pGraph->GetNodeManager().GetNode(m_pGraph->GetLinkManager().GetNextNode(link))->mark)
        continue;

      float heuristicCost = pHeuristic->CalculateCost(m_pGraph, *m_pGraph->GetNodeManager().GetNode(currentNodeIndex), link, *m_pGraph->GetNodeManager().GetNode(m_pGraph->GetLinkManager().GetNextNode(link)), navigationBlockers);
      if (heuristicCost < 0.0f)
        continue;
      float heuristicDist = (m_pGraph->GetNodeManager().GetNode(m_pGraph->GetLinkManager().GetNextNode(link))->GetPos() - m_pGraph->GetNodeManager().GetNode(currentNodeIndex)->GetPos()).CompMul(heuristicZScaleVec).GetLength();
      if (heuristicDist < 0.01f)
        continue;

      // Now find the point on the edge containing the link that intersects the straight path
      // in 2D.
      unsigned vertexStartIndex = m_pGraph->GetLinkManager().GetStartIndex(link);
      unsigned vertexEndIndex = m_pGraph->GetLinkManager().GetEndIndex(link);
      if (vertexStartIndex == vertexEndIndex)
        continue;
      const ObstacleData &startObst = GetAISystem()->m_VertexList.GetVertex(m_pGraph->GetNodeManager().GetNode(currentNodeIndex)->GetTriangularNavData()->vertices[vertexStartIndex]);
      const ObstacleData &endObst = GetAISystem()->m_VertexList.GetVertex(m_pGraph->GetNodeManager().GetNode(currentNodeIndex)->GetTriangularNavData()->vertices[vertexEndIndex]);
      Vec3 edgeStart = startObst.vPos;
      Vec3 edgeEnd = endObst.vPos;
      Vec3 edgeDir = (edgeEnd - edgeStart).GetNormalizedSafe(Vec3(1, 0, 0));
      Lineseg edgeSegment(edgeStart, edgeEnd);
      Lineseg segmentToEnd(currentPos, end);
      float linkRadius= m_pGraph->GetLinkManager().GetRadius(link);
      float ptDist = Distance::Point_Point2D(edgeStart, edgeEnd);
      if (linkRadius > ptDist)
        linkRadius = ptDist;
      float tA, tB;
      if (Intersect::Lineseg_Lineseg2D(segmentToEnd, edgeSegment, tA, tB))
      {
        Vec3 edgePt = edgeSegment.start + tB * (edgeSegment.end - edgeSegment.start);
        // constrain this edge point to be in a valid region
        float workingRadius = reqProperties->radius + extraR;
        if (!startObst.bCollidable && !endObst.bCollidable)
        {
          workingRadius = 0.0f;
        }
        else if (GetAISystem()->IsPointOnForbiddenEdge(edgeStart, 0.0001f, 0, 0, false) || GetAISystem()->IsPointOnForbiddenEdge(edgeEnd, 0.0001f, 0, 0, false))
        {
            workingRadius += extraFR;
        }
        if (workingRadius > linkRadius)
          workingRadius = linkRadius;
        Vec3 edgeStartOK = m_pGraph->GetLinkManager().GetEdgeCenter(link) - edgeDir * (linkRadius - workingRadius);
        Vec3 edgeEndOK = m_pGraph->GetLinkManager().GetEdgeCenter(link) + edgeDir * (linkRadius - workingRadius);

        if (!startObst.bCollidable && startObst.fApproxRadius > 0.0f)
        {
          edgeStartOK = startObst.vPos + startObst.fApproxRadius * edgeDir;
        }
        if (!endObst.bCollidable && endObst.fApproxRadius > 0.0f)
        {
          edgeEndOK = endObst.vPos - endObst.fApproxRadius * edgeDir;
        }

        if ( ( (edgePt - edgeStartOK) | edgeDir ) < 0.0f)
          edgePt = edgeStartOK;
        else if ( ( (edgePt - edgeEndOK) | edgeDir ) > 0.0f)
          edgePt = edgeEndOK;

        // Estimate the cost of getting there
        Vec3 pathSegment = edgePt - currentPos;
        float pathSegmentDist = pathSegment.CompMul(heuristicZScaleVec).GetLength();
        float pathSegmentCost = heuristicCost * (pathSegmentDist / heuristicDist);

        // OK - move!
        straightPathOut.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, edgePt));
        float oldCost = pathCost;
        pathCost += pathSegmentCost;
        currentNodeIndex = m_pGraph->GetLinkManager().GetNextNode(link);

        if (pathCost > maxCost)
        {
          if (pathCost == oldCost)
            return pathCost;
          unsigned nPts = straightPathOut.size();
          if (nPts < 2)
            return pathCost;
          // fraction where to truncate the path
          float frac = (maxCost - oldCost) / (pathCost - oldCost);
          TPathPoints::reverse_iterator it1 = straightPathOut.rbegin();
          TPathPoints::reverse_iterator it2 = it1;
          ++it2;
          Vec3 endPos = frac * it1->vPos + (1.0f - frac) * it2->vPos;
          it1->vPos = endPos;
          return maxCost;
        }

        break;
      }
    }
    if (!link)
      return returnPartialPath ? pathCost : -1.0f;
  }

  return pathCost;
}

//====================================================================
// Reset
//====================================================================
void CTriangularNavRegion::Reset(IAISystem::EResetReason reason)
{
  m_calculateGraphNodesInDynamicAreas = true;
  m_currentGraphNodeIndex = 0;
}

//====================================================================
// IsNodeOccluded
//====================================================================
bool CTriangularNavRegion::IsNodeOccluded(const GraphNode *pNode, EAICollisionEntities collisionEntities, float triScale) const
{
  if (pNode->GetTriangularNavData()->vertices.size() != 3)
    return false;
  Vec3 p0 = GetAISystem()->m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[0]).vPos;
  Vec3 p1 = GetAISystem()->m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[1]).vPos;
  Vec3 p2 = GetAISystem()->m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[2]).vPos;

  static float zOffset = 0.5f;
  p0.z += zOffset;
  p1.z += zOffset;
  p2.z += zOffset;

  AABB triAABB(AABB::RESET);
  triAABB.Add(p0);
  triAABB.Add(p1);
  triAABB.Add(p2);

#if 0
  primitives::triangle prim;
  prim.pt[0] = p0;
  prim.pt[1] = p1;
  prim.pt[2] = p2;
  prim.n = (p1 - p0).Cross(p2 - p0).GetNormalizedSafe();
#else
  static float collHeight = 1.8f;
  primitives::box prim;
  triAABB.max.z = triAABB.min.z;
  prim.center = triAABB.GetCenter();
  prim.size = triScale * 0.5f * triAABB.GetSize();
  prim.size.z = collHeight * 0.5f;
  prim.center.z += collHeight * 0.5f;
  prim.Basis.SetIdentity();
  prim.bOriented = 0;
#endif
  IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
  float d = pPhysics->PrimitiveWorldIntersection(prim.type, &prim, Vec3(ZERO), 
    collisionEntities, 0, 0, geom_colltype0);
  return (d != 0.0f);
}

//====================================================================
// UpdateDynamicNode
// disable links into this node if (a) it's occluded and (b) the incoming
// node isn't occluded (to avoid completely blocking groups of nodes - e.g.
// underneath vehicles, stopping them navigating!)
// Danny todo - evaluate if this will be used - if not maybe remove it
//====================================================================
void CTriangularNavRegion::UpdateDynamicNode(GraphNode *pNode, EAICollisionEntities collisionEntities, bool modifyOnly, float triScale)
{
  bool occluded = IsNodeOccluded(pNode, collisionEntities, triScale);

//  IPhysicalEntity **entities = 0;
//  unsigned nEnts = GetEntitiesFromAABB(entities, triAABB, AICE_DYNAMIC);
  // Danny todo do the tri/ent test
//  bool disable = nEnts > 0;
  for (unsigned link = pNode->firstLinkIndex; link; link = m_pGraph->GetLinkManager().GetNextLink(link))
  {
    GraphNode *pOther = m_pGraph->GetNodeManager().GetNode(m_pGraph->GetLinkManager().GetNextNode(link));
    if (pOther->navType != IAISystem::NAV_TRIANGULAR)
      continue;

    unsigned incomingLink = pOther->GetLinkTo(m_pGraph->GetNodeManager(), m_pGraph->GetLinkManager(), pNode);

    if (m_pGraph->GetLinkManager().GetOrigRadius(incomingLink) < 0.0f)
      continue;

    // only do the expensive other test if there's a chance we might use the result.
    bool otherOccluded = occluded ? IsNodeOccluded(pOther, collisionEntities, triScale) : false;

    bool disable = occluded && !otherOccluded;

    if (disable && m_pGraph->GetLinkManager().GetRadius(incomingLink) > 0.0f)
      if (modifyOnly)
        m_pGraph->GetLinkManager().ModifyRadius(incomingLink, -2.0f);
      else
        m_pGraph->GetLinkManager().SetRadius(incomingLink, -2.0f);
    else if (!disable && m_pGraph->GetLinkManager().GetRadius(incomingLink) < 0.0f)
      if (modifyOnly)
        m_pGraph->GetLinkManager().ModifyRadius(incomingLink, m_pGraph->GetLinkManager().GetOrigRadius(link));
      else
        m_pGraph->GetLinkManager().SetRadius(incomingLink, m_pGraph->GetLinkManager().GetOrigRadius(link));
  }
}

//====================================================================
// Update
//====================================================================
void CTriangularNavRegion::Update(class CCalculationStopper& stopper)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  if (m_calculateGraphNodesInDynamicAreas)
  {
    m_graphNodesInDynamicAreas.clear();
    m_calculateGraphNodesInDynamicAreas = false;

    CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
    CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_TRIANGULAR);
    while (GraphNode* pCurrent = m_pGraph->GetNodeManager().GetNode(it.Increment()))
    {
      const Vec3 &pos = pCurrent->GetPos();
      if (GetAISystem()->IsPointInDynamicTriangularAreas(pos))
      {
        m_graphNodesInDynamicAreas.push_back(pCurrent);
      }
    }
  }
  if (m_graphNodesInDynamicAreas.empty())
    return;

  if (m_currentGraphNodeIndex >= m_graphNodesInDynamicAreas.size())
    m_currentGraphNodeIndex = 0;

  unsigned origIndex = m_currentGraphNodeIndex;
  static float triScale = 0.4f;
  do 
  {
    UpdateDynamicNode(m_graphNodesInDynamicAreas[m_currentGraphNodeIndex], AICE_DYNAMIC, true, triScale);
    m_currentGraphNodeIndex = (m_currentGraphNodeIndex + 1) % m_graphNodesInDynamicAreas.size();
  } while (m_currentGraphNodeIndex != origIndex && !stopper.ShouldCalculationStop());

  static int loopNum = 0;
  // todo remove this log
//  if (m_currentGraphNodeIndex <= origIndex)
//    AILogComment("Completed triangular loop %d", loopNum++);
}

//====================================================================
// DisableOccludedDynamicTriangles
//====================================================================
void CTriangularNavRegion::DisableOccludedDynamicTriangles()
{
  CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
  CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_TRIANGULAR);
  static float triScale = 0.05f;
  while (GraphNode* pCurrent = m_pGraph->GetNodeManager().GetNode(it.Increment()))
  {
    Vec3 pos = pCurrent->GetPos();
    if (GetAISystem()->IsPointInDynamicTriangularAreas(pos))
    {
      UpdateDynamicNode(pCurrent, AICE_STATIC_EXCEPT_TERRAIN, false, triScale);
    }
  }
}

//===================================================================
// GetTeleportPosition
// Implement this by trying random positions away from curPos - checking
// the offsets against physics and forbidden areas etc
//===================================================================
bool CTriangularNavRegion::GetTeleportPosition(const Vec3 &curPos, Vec3 &teleportPos, const char *requesterName)
{
  Vec3 startPos = curPos; // may want some height offset too
  static int numRays = 10;
  static float minDist = 3.0f;
  static float maxDist = 25.0f;
  for (int iRay = 0 ; iRay != numRays ; ++iRay)
  {
    float angle = ai_frand() * gf_PI2;
    Vec3 dir(ZERO);
    cry_sincosf(angle, &dir.x, &dir.y);
    float dist = minDist + (maxDist - minDist) * iRay / (numRays - 1.0f);
    Vec3 delta = dist * dir;

    Lineseg seg(startPos, startPos+delta);

    // check forbidden
    Vec3 newPt = seg.end;
    if (GetAISystem()->IntersectsForbidden(seg.start, seg.end, newPt))
    {
      seg.end = newPt;
      dist = Distance::Point_Point(seg.start, seg.end);
      float offset = min(dist, 0.5f);
      seg.end = newPt - dir * offset;
    }

    // check physics
    Vec3 hitPos(seg.end);
    if (IntersectSegment(hitPos, seg, AICE_ALL))
    {
      float offset = min(dist, 0.5f);
      hitPos -= dir * offset;
    }

    Vec3 floorPos;
    if (!GetFloorPos(floorPos, hitPos, 1.0f, 4.0f, walkabilityDownRadius, AICE_STATIC))
      continue;

    if (!CheckBodyPos(floorPos, AICE_ALL))
      continue;

    if (GetAISystem()->WouldHumanBeVisible(floorPos, false))
      continue;

    if (!GetEnclosing(floorPos))
      continue;

    teleportPos = floorPos;
    return true;
  }

  return false;
}
