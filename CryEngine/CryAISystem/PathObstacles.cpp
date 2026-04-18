#include "StdAfx.h"
#include "PathObstacles.h"
#include "AICollision.h"
#include "AIDebugDrawHelpers.h"
#include "Puppet.h"

#include "I3DEngine.h"

#include <numeric>

static float criticalTopAlt = 0.5f;
static float criticalBaseAlt = 2.0f;

// NOTE Jun 4, 2007: <pvl> the comparison operators are used just for debugging ATM.
inline bool operator== (const SPathObstacleCircle2D & lhs, const SPathObstacleCircle2D & rhs)
{
	return lhs.center == rhs.center && lhs.radius == rhs.radius;
}

inline bool operator== (const SPathObstacleShape2D & lhs, const SPathObstacleShape2D & rhs)
{
	return IsEquivalent (lhs.aabb, rhs.aabb, 0.0f) && lhs.pts == rhs.pts;
}

bool operator== (const CPathObstacle & lhs, const CPathObstacle & rhs)
{
	if (lhs.GetType () != rhs.GetType ())
		return false;

	CPathObstacle::EPathObstacleType type = lhs.GetType ();

	switch (type)
	{
	case CPathObstacle::ePOT_Circle2D:
		return lhs.GetCircle2D () == rhs.GetCircle2D ();
		break;
	case CPathObstacle::ePOT_Shape2D:
		return lhs.GetShape2D () == rhs.GetShape2D ();
		break;
	default:
		AIAssert (0 && "Unhandled obstacle type in operator==(CPathObstacle, CPathObstacle)");
		break;
	}
	return false;
}

inline bool operator== (const TPathObstacles & lhs, const TPathObstacles & rhs)
{
	if (lhs.size () != rhs.size ())
		return false;

	const int numElems = lhs.size	();

	for (int i=0; i < numElems; ++i)
		if (lhs[i] != rhs[i])
			return false;

	return true;
}

// NOTE Jun 4, 2007: <pvl> for consistency checking while debugging
unsigned int SPathObstacleShape2D::GetHash () const
{
	unsigned int hash = 0;
	TVectorOfVectors::const_iterator it = pts.begin ();
	TVectorOfVectors::const_iterator end = pts.end ();
	for ( ; it != end; ++it)
		hash += HashFromVec3 (*it, 0.0f);
	return hash;
}

unsigned int SPathObstacleCircle2D::GetHash () const
{
	return HashFromVec3 (center, 0.0f) + HashFromFloat (radius, 0.0f);
}

unsigned int CPathObstacle::GetHash () const
{
	unsigned int hash = HashFromFloat (float (m_type), 0.0f);

	switch (m_type)
	{
	case CPathObstacle::ePOT_Circle2D:
		return hash + GetCircle2D ().GetHash ();
		break;
	case CPathObstacle::ePOT_Shape2D:
		return hash + GetShape2D ().GetHash ();
		break;
	default:
		AIAssert (0 && "Unhandled obstacle type in CPathObstacle::GetHash()");
		break;
	}
	return 0;
}

static unsigned int GetHash (const TPathObstacles & obstacles)
{
	unsigned int hash = 0;

	TPathObstacles::const_iterator it = obstacles.begin ();
	TPathObstacles::const_iterator end = obstacles.end ();
	for ( ; it != end; ++it)
		hash += (*it)->GetHash ();
	return hash;
}

static void ClearObstacles (TPathObstacles & obstacles)
{
	obstacles.resize(0);
}

static void DeepCopyObstacles (TPathObstacles & dst, const TPathObstacles & src)
{
	for (int i=0; i < src.size(); ++i)
	{
		dst.push_back (new CPathObstacle (*src[i].get()));
	}
}

static bool ObstacleDrawingIsOnForPuppet (const CPuppet * puppet)
{
	if (GetAISystem()->m_cvDebugDraw->GetIVal() > 0)
	{
		const char *pathName = GetAISystem()->m_cvDrawPathAdjustment->GetString();
		if (!strcmp(puppet->GetName(), pathName) || !strcmp(pathName, "all") )
			return true;
	}
	return false;
}

//===================================================================
// CPathObstacle
//===================================================================
CPathObstacle::CPathObstacle(EPathObstacleType type) : m_type(type) 
{
  switch (m_type)
  {
  case ePOT_Circle2D: m_pData = new SPathObstacleCircle2D; break;
  case ePOT_Shape2D: m_pData = new SPathObstacleShape2D; break;
  case ePOT_Unset: m_pData = 0; break;
  default: m_pData = 0; AIError("CPathObstacle: Unhandled type: %d", type);
  }
}

//===================================================================
// CPathObstacle
//===================================================================
CPathObstacle::~CPathObstacle() 
{
	Free();
}

void CPathObstacle::Free()
{
	if (m_pData)
	{
		switch (m_type)
		{
    case ePOT_Circle2D: delete &GetCircle2D(); break;
    case ePOT_Shape2D: delete &GetShape2D(); break;
		default: AIError("~CPathObstacle Unhandled type: %d", m_type); break;
		}
		m_type = ePOT_Unset;
		m_pData = 0;
	}
}

//===================================================================
// operator=
//===================================================================
CPathObstacle & CPathObstacle::operator=(const CPathObstacle &other)
{
  if (this == &other)
    return *this;
  Free();
  m_type = other.GetType();
  switch (m_type)
  {
  case ePOT_Circle2D:
    m_pData = new SPathObstacleCircle2D; 
    *((SPathObstacleCircle2D *) m_pData) = other.GetCircle2D();
    break;
  case ePOT_Shape2D: 
    m_pData = new SPathObstacleShape2D; 
    *((SPathObstacleShape2D *) m_pData) = other.GetShape2D();
    break;
  default: AIError("CPathObstacle::operator= Unhandled type: %d", m_type); return *this;
  }
  return *this;
}

//===================================================================
// CPathObstacle
//===================================================================
CPathObstacle::CPathObstacle(const CPathObstacle &other)
{
  m_type = ePOT_Unset;
  m_pData = 0;
  *this = other;
}


//===================================================================
// SCachedObstacle
//===================================================================
//#define CHECK_CACHED_OBST_CONSISTENCY
struct SCachedObstacle
{
  void Reset() {
		entity = 0; entityHash = 0; extraRadius = 0.0f; shapes.clear();
#ifdef CHECK_CACHED_OBST_CONSISTENCY
		shapesHash = 0;
#endif
		debugBoxes.clear();
	}

  IPhysicalEntity *entity;
  unsigned entityHash;
  float extraRadius;
  TPathObstacles shapes;
#ifdef CHECK_CACHED_OBST_CONSISTENCY
  unsigned shapesHash;
#endif
  std::vector<CPathObstacles::SDebugBox> debugBoxes;
};

/// In a suit_demonstration level 32 results in about 80% cache hits
int CPathObstacles::s_obstacleCacheSize = 32;
CPathObstacles::TCachedObstacles CPathObstacles::s_cachedObstacles;

//===================================================================
// GetOrClearCachedObstacle
//===================================================================
SCachedObstacle * CPathObstacles::GetOrClearCachedObstacle(IPhysicalEntity *entity, float extraRadius)
{
  if (s_obstacleCacheSize == 0)
    return 0;

  unsigned entityHash = GetHashFromEntities(&entity, 1);

  const TCachedObstacles::reverse_iterator itEnd = s_cachedObstacles.rend();
  const TCachedObstacles::reverse_iterator itBegin = s_cachedObstacles.rbegin();
  for (TCachedObstacles::reverse_iterator it = s_cachedObstacles.rbegin() ; it != itEnd ; ++it)
  {
    SCachedObstacle &cachedObstacle = **it;
    if (cachedObstacle.entity != entity || cachedObstacle.extraRadius != extraRadius)
      continue;
    if (cachedObstacle.entityHash == entityHash)
    {
      s_cachedObstacles.erase(it.base()-1);
      s_cachedObstacles.push_back(&cachedObstacle);
#ifdef CHECK_CACHED_OBST_CONSISTENCY
      if (cachedObstacle.shapesHash != GetHash (cachedObstacle.shapes))
        AIError ("corrupted obstacle!");
#endif
      return &cachedObstacle;
    }
    s_cachedObstacles.erase(it.base()-1);
    return 0;
  }
  return 0;
}

//===================================================================
// AddCachedObstacle
//===================================================================
void CPathObstacles::AddCachedObstacle(struct SCachedObstacle *obstacle)
{
  if (s_cachedObstacles.size() == s_obstacleCacheSize)
  {
    delete s_cachedObstacles.front();
    s_cachedObstacles.erase(s_cachedObstacles.begin());
  }
  if (s_obstacleCacheSize > 0)
    s_cachedObstacles.push_back(obstacle);
}

//===================================================================
// PopOldestCachedObstacle
//===================================================================
struct SCachedObstacle *CPathObstacles::GetNewCachedObstacle()
{
  if (s_obstacleCacheSize == 0)
    return 0;
  if (s_cachedObstacles.size() < s_obstacleCacheSize)
    return new SCachedObstacle();
  SCachedObstacle *obs = s_cachedObstacles.front();
  s_cachedObstacles.erase(s_cachedObstacles.begin());
  obs->Reset();
  return obs;
}


//===================================================================
// CombineObstacleShape2DPair
//===================================================================
static bool CombineObstacleShape2DPair(SPathObstacleShape2D &shape1, SPathObstacleShape2D  &shape2)
{
  if (!Overlap::Polygon_Polygon2D<TVectorOfVectors>(shape1.pts, shape2.pts, &shape1.aabb, &shape2.aabb))
    return false;

  shape2.pts.insert(shape2.pts.end(), shape1.pts.begin(), shape1.pts.end());
  ConvexHull2D(shape1.pts, shape2.pts);
  shape1.CalcAABB();
  return true;
}

//===================================================================
// CombineObstaclePair
// If possible adds the second obstacle (which may become unuseable) to 
// the first and returns true.
// If not possible nothing gets modified and returns false
//===================================================================
static bool CombineObstaclePair(CPathObstaclePtr ob1, CPathObstaclePtr ob2)
{
  if (ob1->GetType() != ob2->GetType())
    return false;
  if (ob1->GetType() == CPathObstacle::ePOT_Shape2D)
    return CombineObstacleShape2DPair(ob1->GetShape2D(), ob2->GetShape2D());
  return false;
}

//===================================================================
// CombineObstacles
//===================================================================
static void CombineObstacles(TPathObstacles &combinedObstacles, const TPathObstacles &obstacles)
{
	FUNCTION_PROFILER(GetAISystem()->m_pSystem, PROFILE_AI);
  combinedObstacles = obstacles;

  for (TPathObstacles::iterator it = combinedObstacles.begin() ; it != combinedObstacles.end() ; ++it)
  {
    if ((*it)->GetType() == CPathObstacle::ePOT_Shape2D)
      (*it)->GetShape2D().CalcAABB();
  }
StartAgain:
  for (TPathObstacles::iterator it = combinedObstacles.begin() ; it != combinedObstacles.end() ; ++it)
  {
    for (TPathObstacles::iterator itOther = it+1 ; itOther != combinedObstacles.end() ; ++itOther)
    {
      if (CombineObstaclePair(*it, *itOther))
      {
        combinedObstacles.erase(itOther);
        goto StartAgain;
      }
    }
  }
}

//===================================================================
// SimplifyObstacle
//===================================================================
static void SimplifyObstacle(CPathObstaclePtr ob)
{
  if (ob->GetType() == CPathObstacle::ePOT_Circle2D)
  {
    static const float diagScale = 1.0f / sqrtf(2.0f);
    const SPathObstacleCircle2D &circle = ob->GetCircle2D();
    CPathObstacle newOb(CPathObstacle::ePOT_Shape2D);
    SPathObstacleShape2D &shape2D = newOb.GetShape2D();
    shape2D.pts.push_back(circle.center + diagScale * Vec3(-circle.radius, -circle.radius, 0.0f));
    shape2D.pts.push_back(circle.center +             Vec3( 0.0f, -circle.radius, 0.0f));
    shape2D.pts.push_back(circle.center + diagScale * Vec3( circle.radius,  -circle.radius, 0.0f));
    shape2D.pts.push_back(circle.center +             Vec3(circle.radius, 0.0f, 0.0f));
    shape2D.pts.push_back(circle.center + diagScale * Vec3(circle.radius, circle.radius, 0.0f));
    shape2D.pts.push_back(circle.center +             Vec3( 0.0f, circle.radius, 0.0f));
    shape2D.pts.push_back(circle.center + diagScale * Vec3( -circle.radius,  circle.radius, 0.0f));
    shape2D.pts.push_back(circle.center +             Vec3(-circle.radius,  0.0f, 0.0f));
    *ob = newOb;
  }
}

//===================================================================
// SimplifyObstacles
//===================================================================
static void SimplifyObstacles(TPathObstacles &obstacles)
{
  for (TPathObstacles::iterator it = obstacles.begin() ; it != obstacles.end() ; ++it)
    SimplifyObstacle(*it);
}

//===================================================================
// DebugDraw
//===================================================================
void CPathObstacles::DebugDraw(IRenderer *pRenderer) const
{
  for (unsigned i = 0 ; i < m_debugPathAdjustmentBoxes.size() ; ++i)
  {
    const SDebugBox &box = m_debugPathAdjustmentBoxes[i];
    Matrix34 mat34;
    mat34.SetRotation33(Matrix33(box.q));
    mat34.SetTranslation(box.pos);
    pRenderer->GetIRenderAuxGeom()->DrawOBB(box.obb, mat34, false, ColorB(0, 1, 0), eBBD_Extremes_Color_Encoded);
	}

  if (GetAISystem()->m_cvDebugDraw->GetIVal() != 0)
  {
    for (TPathObstacles::const_iterator it = m_simplifiedObstacles.begin() ; it != m_simplifiedObstacles.end() ; ++it)
    {
      ColorF col(1, 1, 0);
      const CPathObstacle &obstacle = **it;
      if (obstacle.GetType() == CPathObstacle::ePOT_Shape2D && !obstacle.GetShape2D().pts.empty())
      {
        const TVectorOfVectors &pts = obstacle.GetShape2D().pts;
        for (unsigned i = 0 ; i < pts.size() ; ++i)
        {
          unsigned j = (i + 1) % pts.size();
          Vec3 v1 = pts[i];
          Vec3 v2 = pts[j];
          v1.z = GetDebugDrawZ(v1, true);
          v2.z = GetDebugDrawZ(v2, true);
          pRenderer->GetIRenderAuxGeom()->DrawLine( v1, col, v2, col );
        }
      }
    }
    for (TPathObstacles::const_iterator it = m_combinedObstacles.begin() ; it != m_combinedObstacles.end() ; ++it)
    {
      ColorF col(1, 1, 1);
      const CPathObstacle &obstacle = **it;
      if (obstacle.GetType() == CPathObstacle::ePOT_Shape2D && !obstacle.GetShape2D().pts.empty())
      {
        const TVectorOfVectors &pts = obstacle.GetShape2D().pts;
        for (unsigned i = 0 ; i < pts.size() ; ++i)
        {
          unsigned j = (i + 1) % pts.size();
          Vec3 v1 = pts[i];
          Vec3 v2 = pts[j];
          v1.z = GetDebugDrawZ(v1, true);
          v2.z = GetDebugDrawZ(v2, true);
          pRenderer->GetIRenderAuxGeom()->DrawLine( v1, col, v2, col );
        }
      }
    }
  }
}

//===================================================================
// AddEntityBoxesToObstacles
//===================================================================
bool CPathObstacles::AddEntityBoxesToObstacles(IPhysicalEntity *entity, TPathObstacles &obstacles, float extraRadius, const CPuppet *pPuppet, float terrainZ)
{
  static int cacheHits = 0;
  static int cacheMisses = 0;

  static int numPerOutput = 100;
  if (cacheHits + cacheMisses > numPerOutput)
  {
    AILogComment("PathObstacles: cache hits = %d misses = %d", cacheHits, cacheMisses);
    cacheMisses = cacheHits = 0;
  }

  SCachedObstacle *cachedObstacle = GetOrClearCachedObstacle(entity, extraRadius);
  if (cachedObstacle)
  {
    if (pPuppet)
      m_debugPathAdjustmentBoxes.insert(m_debugPathAdjustmentBoxes.end(), cachedObstacle->debugBoxes.begin(), cachedObstacle->debugBoxes.end());

    obstacles.insert(obstacles.end(), cachedObstacle->shapes.begin(), cachedObstacle->shapes.end());
    ++cacheHits;
    return true;
  }
  ++cacheMisses;

  // put the obstacles into a temporary and then combine/copy at the end
  static TPathObstacles pathObstacles;
	ClearObstacles (pathObstacles);

  pe_status_nparts statusNParts;
  int nParts = entity->GetStatus(&statusNParts);
  pe_status_pos statusPos;
  if (0 == entity->GetStatus(&statusPos))
    return false;

  unsigned origDebugBoxesSize = m_debugPathAdjustmentBoxes.size();

  pe_params_part paramsPart;
  for (statusPos.ipart = 0, paramsPart.ipart = 0 ; statusPos.ipart < nParts ; ++statusPos.ipart, ++paramsPart.ipart)
  {
    if (0 == entity->GetParams(&paramsPart))
      continue;
    if (!(paramsPart.flagsAND & geom_colltype_player))
      continue;

    if (0 == entity->GetStatus(&statusPos))
      continue;
    if (!statusPos.pGeomProxy)
      continue;

    primitives::box box;
    statusPos.pGeomProxy->GetBBox(&box);

    box.center *= statusPos.scale;
    box.size *= statusPos.scale;

    Vec3 worldC = statusPos.pos + statusPos.q * box.center;
    Matrix33 worldM = Matrix33(statusPos.q) * box.Basis.GetTransposed();

    static std::vector<Vec3> pts;
    pts.resize(0);
    pts.push_back(worldC + worldM * Vec3(box.size.x, box.size.y, box.size.z));
    pts.push_back(worldC + worldM * Vec3(box.size.x, box.size.y, -box.size.z));
    pts.push_back(worldC + worldM * Vec3(box.size.x, -box.size.y, box.size.z));
    pts.push_back(worldC + worldM * Vec3(box.size.x, -box.size.y, -box.size.z));
    pts.push_back(worldC + worldM * Vec3(-box.size.x, box.size.y, box.size.z));
    pts.push_back(worldC + worldM * Vec3(-box.size.x, box.size.y, -box.size.z));
    pts.push_back(worldC + worldM * Vec3(-box.size.x, -box.size.y, box.size.z));
    pts.push_back(worldC + worldM * Vec3(-box.size.x, -box.size.y, -box.size.z));

    if (pPuppet)
    {
      SDebugBox debugBox;
      debugBox.obb.c.Set(0, 0, 0);
      debugBox.obb.h = box.size;
      debugBox.obb.m33.SetIdentity();
      debugBox.q = Quat(worldM);
      debugBox.pos = worldC;
    }

    float minZ = pts[0].z;
    float maxZ = pts[0].z;
    unsigned nPts = pts.size();
    bool onePointOutside = false;
    for (unsigned i = 0 ; i < nPts ; ++i)
    {
      // if all OBB points are inside FA then ignore this object. TODO cache this result (also the entire shape?)
      // alongside the transformation matrix as the FA checks or not very cheap.
      if (!onePointOutside && !GetAISystem()->IsPointInForbiddenRegion(pts[i], 0, false))
        onePointOutside = true;

      if (pts[i].z > maxZ)
        maxZ = pts[i].z;
      if (pts[i].z < minZ)
        minZ = pts[i].z;
      pts.push_back(pts[i] + Vec3(extraRadius, 0.0f, 0.0f));
      pts.push_back(pts[i] + Vec3(-extraRadius, 0.0f, 0.0f));
      pts.push_back(pts[i] + Vec3(0.0f, extraRadius, 0.0f));
      pts.push_back(pts[i] + Vec3(0.0f, -extraRadius, 0.0f));
    }
    if (!onePointOutside)
      continue;

/*    float extra = 0.5f * min(min(box.size.x, box.size.y), box.size.z);
    const float rScale = sqrt(2.0f);
    minZ += rScale * extra;
    maxZ -= rScale * extra;*/
    if (minZ > terrainZ + criticalBaseAlt)
      continue;
    if (maxZ < terrainZ + criticalTopAlt)
      continue;

    nPts = pts.size();
    for (unsigned i = 0 ; i < nPts ; ++i)
      pts[i].z = terrainZ + 0.1f;

    pathObstacles.push_back(new CPathObstacle(CPathObstacle::ePOT_Shape2D));
    ConvexHull2D(pathObstacles.back()->GetShape2D().pts, pts);
  }

  if (pathObstacles.empty())
    return false;

  static bool combine = true;
  if (combine && pathObstacles.size() > 1)
  {
    static TPathObstacles tempObstacles;
    CombineObstacles(tempObstacles, pathObstacles);
    pathObstacles.swap(tempObstacles);
    ClearObstacles (tempObstacles);
  }
  AIAssert(!pathObstacles.empty());

  obstacles.insert(obstacles.end(), pathObstacles.begin(), pathObstacles.end());

  // update cache
  cachedObstacle = GetNewCachedObstacle();
  if (cachedObstacle)
  {
    cachedObstacle->entity = entity;
    cachedObstacle->entityHash = GetHashFromEntities(&entity, 1);
    cachedObstacle->extraRadius = extraRadius;

    cachedObstacle->shapes.insert(cachedObstacle->shapes.end(), pathObstacles.begin(), pathObstacles.end());
#ifdef CHECK_CACHED_OBST_CONSISTENCY
    cachedObstacle->shapesHash = GetHash (cachedObstacle->shapes);
#endif
    if (pPuppet)
      cachedObstacle->debugBoxes.insert(cachedObstacle->debugBoxes.end(), m_debugPathAdjustmentBoxes.begin() + origDebugBoxesSize, m_debugPathAdjustmentBoxes.end());
    AddCachedObstacle(cachedObstacle);
  }
  // NOTE Jun 3, 2007: <pvl> mostly for debugging - so that pathObstacles
  // doesn't confuse reference counts.  Shouldn't cost more than clearing an
  // already cleared container.  Can be removed if necessary.
  ClearObstacles (pathObstacles);
  return true;
}

//===================================================================
// SSphereSorter
// Arbitrary sort order - but ignores z.
//===================================================================
struct SSphereSorter
{
  bool operator()(const Sphere &lhs, const Sphere &rhs) const
  {
    if (lhs.center.x < rhs.center.x)
      return true;
    else if (lhs.center.x > rhs.center.x)
      return false;
    else if (lhs.center.y < rhs.center.y)
      return true;
    else if (lhs.center.y > rhs.center.y)
      return false;
    else if (lhs.radius < rhs.radius)
      return true;
    else if (lhs.radius > rhs.radius)
      return false;
    else 
      return false;
  }
};

//===================================================================
// SSphereEq
//===================================================================
struct SSphereEq
{
  bool operator()(const Sphere &lhs, const Sphere &rhs) const
  {
    return IsEquivalent(lhs.center, rhs.center, 0.1f) && fabs(lhs.radius - rhs.radius) < 0.1f;
  }
};

//===================================================================
// IsTriangularPt
//===================================================================
inline bool IsTriangularPt(const Vec3 &pt)
{
  int nBuildingID;
  IVisArea *pArea;
  IAISystem::ENavigationType navType = GetAISystem()->CheckNavigationType(pt, nBuildingID, pArea, IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_TRIANGULAR);
  return (navType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD)) != 0;
}

//===================================================================
// GetPathObstacles
//===================================================================
void CPathObstacles::GetPathObstacles(TPathObstacles &obstacles, const CPuppet *pPuppet)
{
	FUNCTION_PROFILER(GetAISystem()->m_pSystem, PROFILE_AI);
  AIAssert(pPuppet);

  ClearObstacles(obstacles);
  m_AIObstacles.clear();

  // at the end we will add the static objects if they are near the dynamic ones - to speed
  // this up store approx circles of all dynamic objects
  static std::vector<Sphere> dynamicObstacleCircles;
  dynamicObstacleCircles.resize(0);

  static float minPuppetAvRadius = 0.5f;
  const float minVehicleAvRadius = GetAISystem()->m_extraVehicleAvoidanceRadiusBig->GetFVal();
	const float obstacleSizeThreshold = GetAISystem()->m_obstacleSizeThreshold->GetFVal();
	const float extraVehicleAvoidanceRadiusSmall = GetAISystem()->m_extraVehicleAvoidanceRadiusSmall->GetFVal();

  float minAvRadius = minPuppetAvRadius;

  if (pPuppet->m_Path.Empty())
	  return;

  IAISystem::ENavigationType navType = pPuppet->m_Path.GetPath().front().navType;
  if (navType & (IAISystem::NAV_FLIGHT | IAISystem::NAV_VOLUME))
	  return;

  const int puppetType = pPuppet->GetType();
  if (puppetType != AIOBJECT_PUPPET)
    minAvRadius = minVehicleAvRadius;

//  float extraRadius = max( pPuppet->m_movementAbility.avoidanceRadius, minAvRadius);
  float extraRadius = minAvRadius;

  // debug
  const CPuppet *pPuppetForDebug = 0;
  m_debugPathAdjustmentBoxes.resize(0);
  if (ObstacleDrawingIsOnForPuppet (pPuppet))
    pPuppetForDebug = pPuppet;

  float maxDistToCheckAheadSq = std::numeric_limits<float>::max();
  if (GetAISystem()->m_cvCrowdControlInPathfind->GetIVal() != 0 && 
      pPuppet->m_movementAbility.pathRegenIntervalDuringTrace > 0.01f)
  {
    static float maxSpeedScale = 1.0f;
		float maxSpeed = pPuppet->m_movementAbility.movementSpeeds.GetRange(AgentMovementSpeeds::AMS_COMBAT, AgentMovementSpeeds::AMU_RUN).max;
    maxDistToCheckAheadSq = square(maxSpeedScale * maxSpeed * pPuppet->m_movementAbility.pathRegenIntervalDuringTrace);
  }
  else
  {
	  return;
  }
  float maxDistToCheckAhead = sqrtf(maxDistToCheckAheadSq);

  static float maxPathDeviation = 15.0f;
  const float maxPathDeviationSq = square(maxPathDeviation);

  Vec3 puppetPos = pPuppet->GetPos();

  AABB obstacleAABB = pPuppet->m_Path.GetAABB(maxDistToCheckAhead);
  obstacleAABB.min -= Vec3(maxPathDeviation, maxPathDeviation, maxPathDeviation);
  obstacleAABB.max += Vec3(maxPathDeviation, maxPathDeviation, maxPathDeviation);

	unsigned lastNodeIndex = pPuppet->m_lastNavNodeIndex;
  std::set<IPhysicalEntity*> checkedPhysicsEntities;
  AABB foundObjectsAABB(AABB::RESET);

	// TODO: [mikko] Does not include player currently, might not be necessary at all.
	const VectorSet<CPuppet*>& enabledPuppetsSet = GetAISystem()->GetEnabledPuppetSet();
	for (VectorSet<CPuppet*>::const_iterator it = enabledPuppetsSet.begin(), itend = enabledPuppetsSet.end(); it != itend; ++it)
	{
		const CAIObject* object = (CAIObject*)*it;

    IPhysicalEntity *physEnt = object->GetPhysics();
    if (object == pPuppet)
    {
      if (physEnt)
        checkedPhysicsEntities.insert(physEnt);
      continue;
    }

    unsigned short type = object->GetType();
    static float maxSpeed = 0.3f;

    if (type == AIOBJECT_VEHICLE)
    {
      // vehicles will get treated as normal physics objects, but we also have to make
      // sure that we won't steer around them dynamically unless they're moving
      if (object->GetVelocity().GetLengthSquared() < square(maxSpeed))
        m_AIObstacles.insert(object);
    }
    else if (type == AIOBJECT_PLAYER || type == AIOBJECT_PUPPET)
    {
			const CAIActor* pActor = object->CastToCAIActor();
      // do this here to make sure this object doesn't come up/get used again later
      if (physEnt)
        checkedPhysicsEntities.insert(physEnt);

      CPuppet::ENavInteraction navInteraction = CPuppet::GetNavInteraction(pPuppet, object);
      if (navInteraction != CPuppet::NI_STEER)
        continue;

      if (object->GetVelocity().GetLengthSquared() > square(maxSpeed))
        continue;

      const Vec3 objectPos = object->GetPhysicsPos();

      Vec3 pathPos;
      float distAlongPath;
      float distToPath = pPuppet->m_Path.GetDistToPath(pathPos, distAlongPath, objectPos, maxDistToCheckAhead, true);
      if (distToPath < 0.0f)
        return;
      if (distToPath > maxPathDeviation)
        continue;

      if (!IsTriangularPt(objectPos))
        continue;

      if (type == AIOBJECT_PLAYER || type == AIOBJECT_PUPPET)
      {
        obstacles.push_back(new CPathObstacle(CPathObstacle::ePOT_Circle2D));
        obstacles.back()->GetCircle2D().center = objectPos;
        obstacles.back()->GetCircle2D().radius = extraRadius + 0.5f;
        dynamicObstacleCircles.push_back(Sphere(obstacles.back()->GetCircle2D().center, obstacles.back()->GetCircle2D().radius));
      }
      else
      {
	      Vec3 FL, FR, BL, BR;
	      pActor->GetProxy()->GetWorldBoundingRect(FL, FR, BL, BR, extraRadius);

        obstacles.push_back(new CPathObstacle(CPathObstacle::ePOT_Shape2D));
        obstacles.back()->GetShape2D().pts.push_back(FL);
        obstacles.back()->GetShape2D().pts.push_back(BL);
        obstacles.back()->GetShape2D().pts.push_back(BR);
        obstacles.back()->GetShape2D().pts.push_back(FR);

        Vec3 mid = 0.25f * (FL + FR + BL + BR);
        float radiusSq = max(max(max(Distance::Point_Point2DSq(mid, FL), Distance::Point_Point2DSq(mid, FR)), 
          Distance::Point_Point2DSq(mid, BL)), Distance::Point_Point2DSq(mid, BR));
        dynamicObstacleCircles.push_back(Sphere(mid, sqrtf(radiusSq)));
      }
      foundObjectsAABB.Add(objectPos, extraRadius + object->GetRadius());
      m_AIObstacles.insert(object);
    }
  }
  // general physics entities
  IPhysicalEntity** entities;
  unsigned nEntities = GetEntitiesFromAABB(entities, obstacleAABB, AICE_DYNAMIC);
	I3DEngine *pEngine = gEnv->p3DEngine;

  for (unsigned iEntity = 0 ; iEntity < nEntities ; ++iEntity)
  {
    IPhysicalEntity *entity = entities[iEntity];

    if (checkedPhysicsEntities.find(entity) != checkedPhysicsEntities.end())
      continue;

    pe_params_bbox params;
    entity->GetParams(&params);

    Vec3 mid = 0.5f * (params.BBox[0] + params.BBox[1]);
    if (!IsTriangularPt(mid))
      continue;

    float terrainZ = pEngine->GetTerrainElevation(mid.x, mid.y);
    float altTop = params.BBox[1].z - terrainZ;
    float altBase = params.BBox[0].z - terrainZ;

    if (altTop < criticalTopAlt)
      continue;
    if (altBase > criticalBaseAlt)
      continue;

    float boxRadius = 0.5f * Distance::Point_Point(params.BBox[0], params.BBox[1]);

    Vec3 pathPos;
    float distAlongPath;
    float distToPath = pPuppet->m_Path.GetDistToPath(pathPos, distAlongPath, mid, maxDistToCheckAhead, true);
    if (distToPath < 0.0f)
      return;
    if (distToPath > maxPathDeviation + boxRadius)
      continue;

    bool obstacleIsSmall = boxRadius < obstacleSizeThreshold;
    float er = extraRadius;
    if (puppetType == AIOBJECT_VEHICLE && obstacleIsSmall)
      er = extraVehicleAvoidanceRadiusSmall;

    bool addedOne = AddEntityBoxesToObstacles(entity, obstacles, er, pPuppetForDebug, terrainZ);
    if (!addedOne)
      continue;
    foundObjectsAABB.Add(mid, extraRadius + boxRadius);
    dynamicObstacleCircles.push_back(Sphere(mid, boxRadius));
  }

  static float safetyRadius = 2.0f;

  const CAISystem::TDamageRegions& damageRegions = GetAISystem()->GetDamageRegions();
	if (!damageRegions.empty())
	{
		for (CAISystem::TDamageRegions::const_iterator it = damageRegions.begin() ; it != damageRegions.end() ; ++it)
		{
			const Sphere &sphere = it->second;

			Vec3 pathPos;
			float distAlongPath;
			float distToPath = pPuppet->m_Path.GetDistToPath(pathPos, distAlongPath, sphere.center, maxDistToCheckAhead, true);
			if (distToPath < 0.0f)
				return;
			if (distToPath > maxPathDeviation)
				continue;

			float r = safetyRadius + sphere.radius * 1.5f; // game code actually uses AABB
			obstacles.push_back(new CPathObstacle(CPathObstacle::ePOT_Circle2D));
			obstacles.back()->GetCircle2D().center = sphere.center;
			obstacles.back()->GetCircle2D().radius = r;
			foundObjectsAABB.Add(sphere.center, r);
			dynamicObstacleCircles.push_back(Sphere(sphere.center, sphere.radius * 1.5f));
		}
	}

  // obstacles close to the entity obstacles
  const float passRadius = pPuppet->GetParameters().m_fPassRadius;
  if (!foundObjectsAABB.IsReset())
  {
    Vec3 terrainPos = foundObjectsAABB.GetCenter();
    terrainPos.z = gEnv->p3DEngine->GetTerrainElevation(terrainPos.x, terrainPos.y);
    foundObjectsAABB.min.z = foundObjectsAABB.max.z = terrainPos.z;

		static MapConstNodesDistance nodesInRange;
		nodesInRange.clear();
    GetAISystem()->GetGraph()->GetNodesInRange(nodesInRange, terrainPos, foundObjectsAABB.GetRadius(), IAISystem::NAV_TRIANGULAR, passRadius, lastNodeIndex, 0);

		static std::vector<int> addedObstacleIndices;
		addedObstacleIndices.clear();

		const MapConstNodesDistance::iterator itEnd = nodesInRange.end();
		for (MapConstNodesDistance::iterator it = nodesInRange.begin() ; it != itEnd ; ++it)
		{
			const GraphNode *pNode = it->first;
			float range = it->second;

			if (pNode->navType != IAISystem::NAV_TRIANGULAR)
				continue;

			const ObstacleIndexVector::const_iterator oiEnd = pNode->GetTriangularNavData()->vertices.end();
			for (ObstacleIndexVector::const_iterator oi=pNode->GetTriangularNavData()->vertices.begin() ; oi!=oiEnd; ++oi)
			{
				int index = *oi;
				const ObstacleData& od = GetAISystem()->m_VertexList.GetVertex(index);
				if (od.fApproxRadius <= 0.0f || ! od.bCollidable)
					continue;
				addedObstacleIndices.push_back(index);
			}
		}

		std::sort(addedObstacleIndices.begin(), addedObstacleIndices.end());

		int prev = -1;
		for (unsigned i = 0, ni = addedObstacleIndices.size(); i < ni; ++i)
		{
			int index = addedObstacleIndices[i];
			if (index != prev)
			{
				const ObstacleData& od = GetAISystem()->m_VertexList.GetVertex(index);

				Vec3 pathPos;
				float distAlongPath;
				float distToPath = pPuppet->m_Path.GetDistToPath(pathPos, distAlongPath, od.vPos, maxDistToCheckAhead, true);
				if (distToPath < 0.0f)
					return;
				if (distToPath > maxPathDeviation)
					continue;

				const unsigned nDOC(dynamicObstacleCircles.size());
				unsigned iDOC;
				for (iDOC = 0 ; iDOC < nDOC ; ++iDOC)
				{
					const Sphere &sphere = dynamicObstacleCircles[iDOC];
					float distSq = Distance::Point_Point2DSq(sphere.center, od.vPos);
					if (distSq < square(sphere.radius + od.fApproxRadius + 2 * extraRadius))
						break;
				}
				if (iDOC == nDOC)
					continue;

				if (GetAISystem()->IsPointInForbiddenRegion(od.vPos, 0, false))
					continue;
				obstacles.push_back(new CPathObstacle(CPathObstacle::ePOT_Circle2D));
				obstacles.back()->GetCircle2D().center.Set(od.vPos.x, od.vPos.y, 0.0f);
				obstacles.back()->GetCircle2D().radius = od.fApproxRadius + extraRadius;
			}
			prev = index;
		}
  }
}


//===================================================================
// CPathObstacles
//===================================================================
CPathObstacles::CPathObstacles()
: m_lastCalculateTime(0.0f), m_lastCalculatePos(ZERO), m_lastCalculatePathVersion(-1)
{
}

//===================================================================
// CPathObstacles
//===================================================================
CPathObstacles::~CPathObstacles()
{
}

//===================================================================
// CalculateObstaclesAroundPuppet
//===================================================================
void CPathObstacles::CalculateObstaclesAroundPuppet(const CPuppet *pPuppet)
{
  static float criticalDist = 2.0f;
  static float criticalTime = 2.0f;
  Vec3 puppetPos = pPuppet->GetPos();
  float dt = (GetAISystem()->GetFrameStartTime() - m_lastCalculateTime).GetSeconds();
  // Danny todo: this is/would be a nice optimisation to make - however when finding hidespots
  // we get here before a path is calculated, so the path obstacle list is invalid (since it 
  // depends on the path).
  if (dt < criticalTime && pPuppet->m_Path.GetVersion() == m_lastCalculatePathVersion && 
    puppetPos.IsEquivalent(m_lastCalculatePos, criticalDist))
    return;

  m_lastCalculatePos = puppetPos;
  m_lastCalculateTime = GetAISystem()->GetFrameStartTime();
  m_lastCalculatePathVersion = pPuppet->m_Path.GetVersion();

  ClearObstacles (m_combinedObstacles);
  ClearObstacles (m_simplifiedObstacles);

  GetPathObstacles(m_simplifiedObstacles, pPuppet);
  SimplifyObstacles(m_simplifiedObstacles);

  // NOTE Mai 24, 2007: <pvl> CombineObstacles() messes up its second argument
  // so if we're going to debug draw simplified obstacles later, we need to
  // keep m_simplifiedObstacles intact by passing a copy to CombineObstacles().
  // TPathObstacles is a container of smart pointers so we need to perform
  // a proper deep copy.
  // UPDATE Jun 4, 2007: <pvl> in fact, both arguments of CombineObstacleShape2DPair()
  // can get changed (the first one gets the convex hull of both and the second one
  // contains all the points).  So if any obstacle in m_simplifiedObstacles
  // comes from the cache and overlaps with anything else, it will be corrupted.
  // So let's just perform a deep copy here every time.
  if (true/*ObstacleDrawingIsOnForPuppet (pPuppet)*/)
  {
    TPathObstacles tempSimplifiedObstacles;
    DeepCopyObstacles (tempSimplifiedObstacles, m_simplifiedObstacles);
    // ATTN Jun 1, 2007: <pvl> costly!  Please remind me to remove this if I forget somehow! :)
    //AIAssert (tempSimplifiedObstacles == m_simplifiedObstacles);
    CombineObstacles(m_combinedObstacles, tempSimplifiedObstacles);
  } else {
    CombineObstacles(m_combinedObstacles, m_simplifiedObstacles);
  }
}

//===================================================================
// IsPointInsideObstacles
//===================================================================
bool CPathObstacles::IsPointInsideObstacles(const Vec3 &pt) const
{
  for (TPathObstacles::const_iterator it = m_combinedObstacles.begin() ; it != m_combinedObstacles.end() ; ++it)
  {
    const CPathObstacle &ob = **it;
    if (ob.GetType() != CPathObstacle::ePOT_Shape2D)
      continue;

    const SPathObstacleShape2D &shape2D = ob.GetShape2D();
    if (shape2D.pts.empty())
      continue;

    if (Overlap::Point_Polygon2D(pt, shape2D.pts, &shape2D.aabb))
      return true;
  }
  return false;
}

//===================================================================
// GetPointOutsideObstacles
//===================================================================
Vec3 CPathObstacles::GetPointOutsideObstacles(const Vec3 &pt, float extraDist) const
{
  Vec3 newPos = pt;

  for (TPathObstacles::const_iterator it = m_combinedObstacles.begin() ; it != m_combinedObstacles.end() ; ++it)
  {
    const CPathObstacle &ob = **it;
    if (ob.GetType() != CPathObstacle::ePOT_Shape2D)
      continue;

    const SPathObstacleShape2D &shape2D = ob.GetShape2D();
    if (shape2D.pts.empty())
      continue;

    if (Overlap::Point_Polygon2D(newPos, shape2D.pts, &shape2D.aabb))
    {
      (void) Distance::Point_Polygon2DSq(newPos, shape2D.pts, newPos);
      newPos.z = pt.z;
      Vec3 polyMidPoint = std::accumulate(shape2D.pts.begin(), shape2D.pts.end(), Vec3(ZERO)) / shape2D.pts.size();
      Vec3 dir = (newPos - polyMidPoint);
      dir.z = 0.0f;
      dir.NormalizeSafe();
      newPos += extraDist * dir;
    }
  }
  return newPos;
}

//===================================================================
// Reset
//===================================================================
void CPathObstacles::Reset()
{
  while (!s_cachedObstacles.empty())
  {
    delete s_cachedObstacles.back();
    s_cachedObstacles.pop_back();
  }
}
