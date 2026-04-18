/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   CAISystem.cpp
	$Id$
  Description: 
  
 -------------------------------------------------------------------------
  History:
	- 2001				: Created by Petar Kotevski
	- 2003				: Taken over by Kirill Bulatsev

*********************************************************************/





#include "StdAfx.h"

#include <stdio.h>

#include <limits>
#include <map>
#include <numeric>
#include <algorithm>

#include "IPhysics.h"
#include "IEntitySystem.h"
#include "IScriptSystem.h"
#include "I3DEngine.h"
#include "ILog.h"
#include "../CryAction/IGameRulesSystem.h"
#include "CryFile.h"
#include "Cry_Math.h"
#include "ISystem.h"
#include "IEntityRenderState.h"
#include "IRenderer.h"
#include "ITimer.h"
#include "IConsole.h"
#include "ISerialize.h"
//#include "ICryPak.h"
#include "IAgent.h"
#include "VectorSet.h"

#include "CAISystem.h"
#include "CryAISystem.h"
#include "AILog.h"
#include "CTriangulator.h"
#include "TriangularNavRegion.h"
#include "WaypointHumanNavRegion.h"
#include "Waypoint3DSurfaceNavRegion.h"
#include "VolumeNavRegion.h"
#include "FlightNavRegion.h"
#include "RoadNavRegion.h"
#include "SmartObjectNavRegion.h"
#include "Free2DNavRegion.h"
#include "Graph.h"
#include "AStarSolver.h"
#include "PolygonSetOps/Polygon2d.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include "GoalOp.h"
#include "AIPlayer.h"
#include "PipeUser.h"
#include "AIAutoBalance.h"
#include "Leader.h"
#include "ObjectTracker.h"
#include "VehicleFinder.h"
#include "SmartObjects.h"
#include "AIActions.h"
#include "AICollision.h"
#include "TickCounter.h"
#include "AIRadialOcclusion.h"
#include "GraphNodeManager.h"
#include "CentralInterestManager.h"


static const float g_POINT_DIST_PRESISION = 0.0000000015f;
static const real epsilon = 0.0000001;
//const float epsilon = 0.001f;

#define BAI_AREA_FILE_VERSION_READ 15
#define BAI_AREA_FILE_VERSION_WRITE 18



template <class F>
inline bool IsEquivalent2D(const Vec3_tpl<F>& v0, const Vec3_tpl<F>& v1, f32 epsilon=VEC_EPSILON )
{
	return fabs_tpl(v0.x-v1.x) <= epsilon && fabs_tpl(v0.y-v1.y) <= epsilon;
}


static CStandardHeuristic sStandardHeuristic;

bool CCalculationStopper::m_neverStop = false;
bool CCalculationStopper::m_useCounter;
#ifdef CALIBRATE_STOPPER
std::map< string, std::pair<unsigned, float> > CCalculationStopper::m_mapCallRate;
#endif

// counter used to ensure automatic forbidden names are unique
int forbiddenNameCounter = 0;

// flag used for debugging/profiling the improvement obtained from using a
// QuadTree for the forbidden shapes. Currently it's actually faster not
// using quadtree - need to experiment more
// kirill - enabling quadtree - is faster on low-spec
bool useForbiddenQuadTree = true;//false;

// used to determine if a forbidden area should be rasterised
static float criticalForbiddenSize = 30.0f;
static float forbiddenMaskGranularity = 2.0f;
static unsigned forbiddenMaskMaxDimension = 256; // max side of the mask (256->16KB)
static bool useForbiddenMask = false;


//===================================================================
// CCalculationStopper
//===================================================================
CCalculationStopper::CCalculationStopper(const char * name, float dt, float callRate) 
: m_stopCounter(0),  m_callRate(callRate)
{
  m_endTime = gEnv->pTimer->GetAsyncTime() + CTimeValue(dt);
  if (m_useCounter)
  {
    m_stopCounter = (unsigned)(dt * callRate);
    if (m_stopCounter < 1)
      m_stopCounter = 1;
  }
#ifdef CALIBRATE_STOPPER
  m_name = name;
  m_calls = 0;
  m_dt = dt;
#endif
}

//===================================================================
// SShape
//===================================================================
SShape::SShape(const ListPositions& shape, bool allowMask, IAISystem::ENavigationType navType, int type,
							 float height, EAILightLevel lightLevel, bool temp) :
               shape(shape), navType(navType), type(type), devalueTime(0), height(height),
							 temporary(temp), enabled(true), shapeMask(0), lightLevel(lightLevel)
{
  RecalcAABB();
  if (allowMask)
    BuildMask(forbiddenMaskGranularity);
}

//===================================================================
// BuildMask
//===================================================================
void SShape::BuildMask(float granularity)
{
  Vec3 delta = aabb.max - aabb.min;
  if (useForbiddenMask && (delta.x > criticalForbiddenSize || delta.y > criticalForbiddenSize))
  {
    if (!shapeMask)
      shapeMask = new CShapeMask;
    shapeMask->Build(this, forbiddenMaskGranularity);
  }
  else
  {
    ReleaseMask();
  }
}

//===================================================================
// CShapeMask
//===================================================================
CShapeMask::CShapeMask() : m_nx(0), m_ny(0), m_dx(0), m_dy(0)
{
}

//===================================================================
// CShapeMask
//===================================================================
CShapeMask::~CShapeMask()
{
}

//===================================================================
// GetBox
//===================================================================
void CShapeMask::GetBox(Vec3 &min, Vec3 &max, int i, int j)
{
  min = m_aabb.min + Vec3(i * m_dx, j * m_dy, 0.0f);
  max = min + Vec3(m_dx, m_dy, 0.0f);
}

//===================================================================
// GetType
//===================================================================
CShapeMask::EType CShapeMask::GetType(const struct SShape *pShape, const Vec3 &min, const Vec3 &max) const
{
  Vec3 delta = max - min;
  if (Overlap::Lineseg_Polygon2D(Lineseg(min, min+Vec3(delta.x, 0.0f, 0.0f)), pShape->shape, &pShape->aabb))
    return TYPE_EDGE;
  if (Overlap::Lineseg_Polygon2D(Lineseg(min+Vec3(0.0f, delta.y, 0.0f), min+Vec3(delta.x, delta.y, 0.0f)), pShape->shape, &pShape->aabb))
    return TYPE_EDGE;
  if (Overlap::Lineseg_Polygon2D(Lineseg(min, min+Vec3(0.0f, delta.y, 0.0f)), pShape->shape, &pShape->aabb))
    return TYPE_EDGE;
  if (Overlap::Lineseg_Polygon2D(Lineseg(min+Vec3(delta.x, 0.0f, 0.0f), min+Vec3(delta.x, delta.y, 0.0f)), pShape->shape, &pShape->aabb))
    return TYPE_EDGE;

  Vec3 mid = 0.5f * (min + max);
  if (Overlap::Point_Polygon2D(mid, pShape->shape, &pShape->aabb))
    return TYPE_IN;
  else
    return TYPE_OUT;
}

//===================================================================
// MemStats
//===================================================================
size_t CShapeMask::MemStats() const
{
  size_t size = sizeof(this);
  size += m_container.capacity() * sizeof(unsigned char);
  return size;
}


//===================================================================
// Build
//===================================================================
void CShapeMask::Build(const struct SShape *pShape, float granularity)
{
  AIAssert(pShape);
  AIAssert(granularity > 0.0f);
  m_aabb = pShape->aabb;
  Vec3 delta = m_aabb.max - m_aabb.min;
  m_nx = 1 + (unsigned)(delta.x / granularity);
  m_ny = 1 + (unsigned)(delta.y / granularity);

  if (m_nx > forbiddenMaskMaxDimension)
    m_nx = forbiddenMaskMaxDimension;
  if (m_ny > forbiddenMaskMaxDimension)
    m_ny = forbiddenMaskMaxDimension;

  m_dx = delta.x / m_nx;
  m_dy = delta.y / m_ny;

  m_bytesPerRow = (m_nx + 3) >> 2;

  m_container.resize(m_ny * m_bytesPerRow);

  for (unsigned j = 0 ; j != m_ny ; ++j)
  {
    for (unsigned i = 0 ; i != m_nx ; ++i)
    {
      unsigned index = (i >> 2) + m_bytesPerRow * j; assert(index < m_container.size());
      unsigned char &byte = m_container[index];

      unsigned offset = (i & 3) * 2;

      Vec3 min, max;
      GetBox(min, max, i, j);

      unsigned char value = GetType(pShape, min, max) << offset;
      unsigned char mask = 3 << offset;
      byte = (byte & ~mask) + value;
    }
  }
}

//===================================================================
// GetType
//===================================================================
CShapeMask::EType CShapeMask::GetType(const Vec3 pt) const
{
  if (!useForbiddenMask)
    return TYPE_EDGE;
  int i = (int) ((pt.x - m_aabb.min.x) / m_dx);
  if (i < 0 || i >= m_nx)
    return TYPE_OUT;
  int j = (int) ((pt.x - m_aabb.min.y) / m_dx);
  if (j < 0 || j >= m_ny)
    return TYPE_OUT;

  unsigned index = (i >> 2) + m_bytesPerRow * j; assert(index < m_container.size());
  const unsigned char &byte = m_container[index];

  unsigned offset = (i & 2) * 2;
  unsigned char mask = 3 << offset;

  unsigned char value = byte & mask;
  value = value >> offset;
  return (EType) value ;
}

//====================================================================
// SAIObjectMapIter
// Specialized next for iterating over the whole container.
//====================================================================
struct SAIObjectMapIter : public IAIObjectIter
{
	SAIObjectMapIter(AIObjects::iterator first, AIObjects::iterator end) :
		m_it(first), m_end(end), m_ai(0)
	{
	}

	virtual IAIObject* GetObject()
	{
		if (!m_ai && m_it != m_end)
			NextValid();
		return m_ai;
	}

  virtual void Release() {pool.push_back(this);}
	virtual void Next()
	{
		if(m_it != m_end)
			++m_it;
		NextValid();
	}

	virtual void	NextValid()
	{
		m_ai = (m_it == m_end) ? 0 : m_it->second;
	}

  static SAIObjectMapIter* Allocate(AIObjects::iterator first, AIObjects::iterator end)
  {
    if (pool.empty())
    {
      return new SAIObjectMapIter(first, end);
    }
    else
    {
      SAIObjectMapIter *res = pool.back(); pool.pop_back();
      return new(res) SAIObjectMapIter(first, end);
    }
  }

	IAIObject*	m_ai;
	AIObjects::iterator	m_it;
	AIObjects::iterator	m_end;
  static std::vector<SAIObjectMapIter*> pool;
};

//====================================================================
// SAIObjectMapIterOfType
// Iterator base for iterating over 'AIObjects' container.
// Returns only objects of specified type.
//====================================================================
struct SAIObjectMapIterOfType : public SAIObjectMapIter
{
	SAIObjectMapIterOfType(short n, AIObjects::iterator first, AIObjects::iterator end) :
		m_n(n), SAIObjectMapIter(first, end) {}

  virtual void Release() {pool.push_back(this);}

  static SAIObjectMapIterOfType* Allocate(short n, AIObjects::iterator first, AIObjects::iterator end)
  {
    if (pool.empty())
    {
      return new SAIObjectMapIterOfType(n, first, end);
    }
    else
    {
      SAIObjectMapIterOfType *res = pool.back(); pool.pop_back();
      return new(res) SAIObjectMapIterOfType(n, first, end);
    }
  }

	virtual void	NextValid()
	{
		// Constraint to type
		if(m_it != m_end && m_it->first != m_n)
			m_it = m_end;

		m_ai = (m_it == m_end) ? 0 : m_it->second;
	}

	short	m_n;
  static std::vector<SAIObjectMapIterOfType*> pool;
};

//====================================================================
// SAIObjectMapIterInRange
// Iterator base for iterating over 'AIObjects' container.
// Returns only objects which are enclosed by the specified sphere.
//====================================================================
struct SAIObjectMapIterInRange : public SAIObjectMapIter
{
	SAIObjectMapIterInRange(AIObjects::iterator first, AIObjects::iterator end, const Vec3& center, float rad, bool check2D) :
		m_center(center), m_rad(rad), m_check2D(check2D), SAIObjectMapIter(first, end) {}

  virtual void Release() {pool.push_back(this);}

  static SAIObjectMapIterInRange* Allocate(AIObjects::iterator first, AIObjects::iterator end, const Vec3& center, float rad, bool check2D)
  {
    if (pool.empty())
    {
      return new SAIObjectMapIterInRange(first, end, center, rad, check2D);
    }
    else
    {
      SAIObjectMapIterInRange *res = pool.back(); pool.pop_back();
      return new(res) SAIObjectMapIterInRange(first, end, center, rad, check2D);
    }
  }

  virtual void	NextValid()
	{
		while(m_it != m_end)
		{
			// Constraint to sphere
			if(m_check2D)
			{
				if(Distance::Point_Point2DSq(m_center, m_it->second->GetPos()) < sqr(m_it->second->GetRadius() + m_rad)) break;
			}
			else
			{
				if(Distance::Point_PointSq(m_center, m_it->second->GetPos()) < sqr(m_it->second->GetRadius() + m_rad)) break;
			}
			++m_it;
		}
		m_ai = (m_it == m_end) ? 0 : m_it->second;
	}

	Vec3	m_center;
	float	m_rad;
	bool	m_check2D;
  static std::vector<SAIObjectMapIterInRange*> pool;
};

//====================================================================
// SAIObjectMapIterOfTypeInRange
// Specialized next for iterating over the whole container.
// Returns only objects which are enclosed by the specified sphere and of specified type.
//====================================================================
struct SAIObjectMapIterOfTypeInRange : public SAIObjectMapIter
{
	SAIObjectMapIterOfTypeInRange(short n, AIObjects::iterator first, AIObjects::iterator end, const Vec3& center, float rad, bool check2D) :
		m_n(n), m_center(center), m_rad(rad), m_check2D(check2D), SAIObjectMapIter(first, end) {}

  virtual void Release() {pool.push_back(this);}

  static SAIObjectMapIterOfTypeInRange* Allocate(short n, AIObjects::iterator first, AIObjects::iterator end, const Vec3& center, float rad, bool check2D)
  {
    if (pool.empty())
    {
      return new SAIObjectMapIterOfTypeInRange(n, first, end, center, rad, check2D);
    }
    else
    {
      SAIObjectMapIterOfTypeInRange *res = pool.back(); pool.pop_back();
      return new(res) SAIObjectMapIterOfTypeInRange(n, first, end, center, rad, check2D);
    }
  }

  virtual void	NextValid()
	{
		while(m_it != m_end)
		{
			// Constraint to type
			if(m_it->first != m_n)
			{
				m_it = m_end;
				break;
			}
			// Constraint to sphere
			if(m_check2D)
			{
				if(Distance::Point_Point2DSq(m_center, m_it->second->GetPos()) < sqr(m_it->second->GetRadius() + m_rad)) break;
			}
			else
			{
				if(Distance::Point_PointSq(m_center, m_it->second->GetPos()) < sqr(m_it->second->GetRadius() + m_rad)) break;
			}
			// This one did not match, try next.
			++m_it;
		}
		m_ai = (m_it == m_end) ? 0 : m_it->second;
	}

	short	m_n;
	Vec3	m_center;
	float	m_rad;
	bool	m_check2D;
  static std::vector<SAIObjectMapIterOfTypeInRange*> pool;
};

//====================================================================
// SAIObjectMapIterInShape
// Iterator base for iterating over 'AIObjects' container.
// Returns only objects which are enclosed by the specified shape.
//====================================================================
struct SAIObjectMapIterInShape : public SAIObjectMapIter
{
	SAIObjectMapIterInShape(AIObjects::iterator first, AIObjects::iterator end, const SShape& shape, bool checkHeight) :
		m_shape(shape), m_checkHeight(checkHeight), SAIObjectMapIter(first, end) {}

  virtual void Release() {pool.push_back(this);}

  static SAIObjectMapIterInShape* Allocate(AIObjects::iterator first, AIObjects::iterator end, const SShape& shape, bool checkHeight)
  {
    if (pool.empty())
    {
      return new SAIObjectMapIterInShape(first, end, shape, checkHeight);
    }
    else
    {
      SAIObjectMapIterInShape *res = pool.back(); pool.pop_back();
      return new(res) SAIObjectMapIterInShape(first, end, shape, checkHeight);
    }
  }

  virtual void	NextValid()
	{
		while(m_it != m_end)
		{
			// Constraint to sphere
			if(m_shape.IsPointInsideShape(m_it->second->GetPos(), m_checkHeight))
				break;
			// This one did not match, try next.
			++m_it;
		}
		m_ai = (m_it == m_end) ? 0 : m_it->second;
	}

	const SShape& m_shape;
	bool	m_checkHeight;
  static std::vector<SAIObjectMapIterInShape*> pool;
};

//====================================================================
// SAIObjectMapIterOfTypeInRange
// Specialized next for iterating over the whole container.
// Returns only objects which are enclosed by the specified shape and of specified type.
//====================================================================
struct SAIObjectMapIterOfTypeInShape : public SAIObjectMapIter
{
	SAIObjectMapIterOfTypeInShape(short n, AIObjects::iterator first, AIObjects::iterator end, const SShape& shape, bool checkHeight) :
		m_n(n), m_shape(shape), m_checkHeight(checkHeight), SAIObjectMapIter(first, end) {}

  virtual void Release() {pool.push_back(this);}

  static SAIObjectMapIterOfTypeInShape* Allocate(short n, AIObjects::iterator first, AIObjects::iterator end, const SShape& shape, bool checkHeight)
  {
    if (pool.empty())
    {
      return new SAIObjectMapIterOfTypeInShape(n, first, end, shape, checkHeight);
    }
    else
    {
      SAIObjectMapIterOfTypeInShape *res = pool.back(); pool.pop_back();
      return new(res) SAIObjectMapIterOfTypeInShape(n, first, end, shape, checkHeight);
    }
  }

  virtual void	NextValid()
	{
		while(m_it != m_end)
		{
			// Constraint to type
			if(m_it->first != m_n)
			{
				m_it = m_end;
				break;
			}
			// Constraint to sphere
			if(m_shape.IsPointInsideShape(m_it->second->GetPos(), m_checkHeight))
				break;
			// This one did not match, try next.
			++m_it;
		}
		m_ai = (m_it == m_end) ? 0 : m_it->second;
	}

	short	m_n;
	const SShape& m_shape;
	bool	m_checkHeight;
  static std::vector<SAIObjectMapIterOfTypeInShape*> pool;
};

std::vector<SAIObjectMapIter*> SAIObjectMapIter::pool;
std::vector<SAIObjectMapIterOfType*> SAIObjectMapIterOfType::pool;
std::vector<SAIObjectMapIterInRange*> SAIObjectMapIterInRange::pool;
std::vector<SAIObjectMapIterOfTypeInRange*> SAIObjectMapIterOfTypeInRange::pool;
std::vector<SAIObjectMapIterInShape*> SAIObjectMapIterInShape::pool;
std::vector<SAIObjectMapIterOfTypeInShape*> SAIObjectMapIterOfTypeInShape::pool;


void DeleteAIObjectMapIter(SAIObjectMapIter *ptr) {delete ptr;}
//===================================================================
// ClearAIObjectIteratorPools
//===================================================================
void ClearAIObjectIteratorPools()
{
  std::for_each(SAIObjectMapIter::pool.begin(), SAIObjectMapIter::pool.end(), DeleteAIObjectMapIter);
  std::for_each(SAIObjectMapIterOfType::pool.begin(), SAIObjectMapIterOfType::pool.end(), DeleteAIObjectMapIter);
  std::for_each(SAIObjectMapIterInRange::pool.begin(), SAIObjectMapIterInRange::pool.end(), DeleteAIObjectMapIter);
  std::for_each(SAIObjectMapIterOfTypeInRange::pool.begin(), SAIObjectMapIterOfTypeInRange::pool.end(), DeleteAIObjectMapIter);
  std::for_each(SAIObjectMapIterInShape::pool.begin(), SAIObjectMapIterInShape::pool.end(), DeleteAIObjectMapIter);
  std::for_each(SAIObjectMapIterOfTypeInShape::pool.begin(), SAIObjectMapIterOfTypeInShape::pool.end(), DeleteAIObjectMapIter);
  SAIObjectMapIter::pool.clear();
  SAIObjectMapIterOfType::pool.clear();
  SAIObjectMapIterInRange::pool.clear();
  SAIObjectMapIterOfTypeInRange::pool.clear();
  SAIObjectMapIterInShape::pool.clear();
  SAIObjectMapIterOfTypeInShape::pool.clear();
}

//====================================================================
// AISIGNAL_EXTRA_DATA 
//====================================================================
AISignalExtraData::AISignalExtraData()
{
	point.zero();
	point2.zero();
	fValue = 0.0f;
	nID = 0;
	iValue = 0;
	iValue2 = 0;
	sObjectName = NULL;
}

AISignalExtraData::~AISignalExtraData()
{
	if ( sObjectName )
		delete [] sObjectName;
}

void AISignalExtraData::SetObjectName( const char* objectName )
{
	if ( sObjectName )
	{
		delete [] sObjectName;
		sObjectName = NULL;
	}
	if ( objectName && *objectName )
	{
		sObjectName = new char[strlen( objectName ) + 1];
		strcpy( sObjectName, objectName );
	}
}

void AISignalExtraData::Serialize( TSerialize ser )
{
	ser.Value("point", point);
	ser.Value("point2", point2);
	//	ScriptHandle nID;
	ser.Value("fValue", fValue);
	ser.Value("iValue", iValue);
	ser.Value("iValue2", iValue2);
	string	objectNameString(sObjectName);
	ser.Value("sObjectName", objectNameString);
	if(ser.IsReading())
		SetObjectName(objectNameString.c_str());
}

AISignalExtraData& AISignalExtraData:: operator = ( const AISignalExtraData& other )
{
	point = other.point;
	point2 = other.point2;
	nID = other.nID;
	fValue = other.fValue;
	iValue = other.iValue;
	iValue2 = other.iValue2;
	SetObjectName( other.sObjectName );
	return *this;
};

//===================================================================
// AddTheCut
//===================================================================
static void AddTheCut( int vIdx1, int vIdx2, NewCutsVector &newCutsVector )
{
	AIAssert( vIdx1>=0 && vIdx2>=0 );
	if( vIdx1 == vIdx2 )
		return;
	newCutsVector.push_back(CutEdgeIdx(vIdx1,vIdx2));
}




//====================================================================
// CAISystem
//====================================================================
CAISystem::CAISystem(ISystem *pSystem)
: m_nNumPuppets(0),
m_fLastPathfindTimeStart(0.f),
m_bCollectingAllowed(true),
m_fAutoBalanceCollectingTimeout(0.f),
m_pAutoBalance(0),
m_nNumBuildings(0),
m_nTickCount(0),
m_pTriangulator(0),
m_pSystem(pSystem),
m_pGraph(0),
m_pHideGraph(0),
m_navDataState(NDS_UNSET),
m_pAStarSolver(0),
m_pCurrentRequest(0),
m_bInitialized(false),
m_cvSoundPerception(0),
m_pSmartObjects(NULL),
m_bUpdateSmartObjects(false),
m_pActionManager(NULL),
m_recorderDrawStart(0),
m_recorderDrawEnd(0),
m_recorderDrawCursor(0),
m_pProfileTickCounter(new TickCounterManager),
m_ExplosionSpotLifeTime(5.0f),
m_lastAmbientFireUpdateTime(-10.0f),
m_lastVisBroadPhaseTime(-10.0f),
m_lastExpensiveAccessoryUpdateTime(-10.0f),
m_lastGroupUpdateTime(-10.0f),
m_DEBUG_screenFlash(0),
m_IsEnabled(true),
m_visChecks(0),
m_visChecksMax(0),
m_visChecksRays(0),
m_visChecksRaysMax(0),
m_visChecksHistoryHead(0),
m_visChecksHistorySize(0),
m_enabledPuppetsUpdateError(0),
m_enabledPuppetsUpdateHead(0),
m_totalPuppetsUpdateCount(0),
m_disabledPuppetsUpdateError(0),
m_disabledPuppetsHead(0),
m_nRaysThisUpdateFrame(0),
m_pathFindIdGen(1)
{
	g_crcSignals.Init(pSystem->GetCrc32Gen());

	m_pGraph = new CGraph(this);
	m_pHideGraph = new CGraph(this);

	m_pAStarSolver = new CAStarSolver(m_pGraph->GetNodeManager());

	m_pTriangularNavRegion = new CTriangularNavRegion(m_pGraph);
	m_pWaypointHumanNavRegion = new CWaypointHumanNavRegion(m_pGraph);
	m_pWaypoint3DSurfaceNavRegion = new CWaypoint3DSurfaceNavRegion(m_pGraph);
	m_pFlightNavRegion = new CFlightNavRegion(pSystem->GetIPhysicalWorld(), m_pGraph);
	m_pVolumeNavRegion = new CVolumeNavRegion(m_pGraph);
	m_pRoadNavRegion = new CRoadNavRegion(m_pGraph);
  m_pFree2DNavRegion = new CFree2DNavRegion(m_pGraph);
	m_pSmartObjectNavRegion = new CSmartObjectNavRegion(m_pGraph);

	m_pTriangularHideRegion = new CTriangularNavRegion(m_pHideGraph);

	m_pVehicleFinder = new CVehicleFinder();

	InitConsoleVars();

	m_Recorder.Init();


	// Don't Init() here! It will be called later after creation of the Entity System
//	Init();
}

//====================================================================
// GetNavRegion
//====================================================================
CNavRegion *CAISystem::GetNavRegion(IAISystem::ENavigationType type, const class CGraph *pGraph)
{
  switch (type)
  {
  case IAISystem::NAV_TRIANGULAR: return GetTriangularNavRegion(pGraph);
  case IAISystem::NAV_WAYPOINT_HUMAN: return m_pWaypointHumanNavRegion;
  case IAISystem::NAV_WAYPOINT_3DSURFACE: return m_pWaypoint3DSurfaceNavRegion;
  case IAISystem::NAV_FLIGHT: return m_pFlightNavRegion;
  case IAISystem::NAV_VOLUME: return m_pVolumeNavRegion;
  case IAISystem::NAV_ROAD: return m_pRoadNavRegion;
  case IAISystem::NAV_SMARTOBJECT: return m_pSmartObjectNavRegion;
  case IAISystem::NAV_FREE_2D: return m_pFree2DNavRegion;
  case IAISystem::NAV_UNSET: return 0;
  }
  return 0;
}


//====================================================================
// IsPartOfAnyLeadersUnits
//====================================================================
CLeader * CAISystem::IsPartOfAnyLeadersUnits(CAIObject * pObj)
{
	for (AIObjects::const_iterator iter = m_Objects.begin(); iter != m_Objects.end(); ++iter)
	{
		if (!iter->second)
			continue;
		CLeader* pLeader = iter->second->CastToCLeader();
		if(pLeader)
		{
			if (pLeader->GetAIGroup()->IsMember(pObj))
				return pLeader;
		}
	}
	return NULL;
}

//
//-----------------------------------------------------------------------------------------------------------
CAISystem::~CAISystem()
{
	ShutDown();

	delete m_pTriangularHideRegion;
	m_pTriangularHideRegion = 0;

	delete m_pTriangularNavRegion;
	m_pTriangularNavRegion = 0;
	delete m_pWaypointHumanNavRegion;
	m_pWaypointHumanNavRegion = 0;
	delete m_pWaypoint3DSurfaceNavRegion;
	m_pWaypoint3DSurfaceNavRegion = 0;
	delete m_pFlightNavRegion;
	m_pFlightNavRegion = 0;
	delete m_pVolumeNavRegion;
	m_pVolumeNavRegion = 0;
	delete m_pRoadNavRegion;
	m_pRoadNavRegion = 0;
	delete m_pSmartObjectNavRegion;
	m_pSmartObjectNavRegion = 0;

	delete m_pAStarSolver;
	m_pAStarSolver = 0;
	delete m_pGraph;
	m_pGraph = 0;
	delete m_pHideGraph;
	m_pHideGraph = 0;
	if(m_pVehicleFinder)
		delete m_pVehicleFinder;
	m_pVehicleFinder = 0;
}

// Defined in AIDebugDrawHelpers.cpp
extern void SetDebugDrawOffset(ICVar* pCVar);

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::InitConsoleVars()
{
	AILogProgress("[AISYSTEM] Initialization: Creating console vars");

	IConsole *pConsole = m_pSystem->GetIConsole();

  m_cvUpdateAllAlways = pConsole->RegisterInt("ai_UpdateAllAlways",0,VF_CHEAT,
    "If non-zero then over-rides the auto-disabling of invisible/distant AI");
	m_cvUseCalculationStopperCounter = pConsole->RegisterInt("ai_UseCalculationStopperCounter",0,VF_CHEAT,
    "Uses a (calibrated) counter instead of time in AI updates");
  /*	m_cvAILeaderDies = pConsole->RegisterInt("ai_LeaderDies", 1, VF_CHEAT,
		"Enables changing of behaviors of group members \n"
		"to non-group behavior when group leader dies \n"
		"Usage: ai_LeaderDies <0/1> \n"
		"Default is 1. Means changing is enabled \n");
*/
	// is not cheat protected because it changes during game, depending on your settings
	m_cvAIUpdateInterval = pConsole->RegisterFloat("ai_UpdateInterval",0.13f,0,
		"In seconds the amount of time between two full updates for AI  \n"
		"Usage: ai_UpdateInterval <number>\n"
		"Default is 0.1. Number is time in seconds\n");
	m_cvDynamicWaypointUpdateTime = pConsole->RegisterFloat("ai_DynamicWaypointUpdateTime",0.0005f,0,
		"How long (max) to spend updating dynamic waypoint regions per AI update (in sec)\n"
		"0 disables dynamic updates. 0.0005 is a sensible value\n");
	m_cvDynamicTriangularUpdateTime = pConsole->RegisterFloat("ai_DynamicTriangularUpdateTime",0.002f,0,
		"How long (max) to spend updating triangular waypoint regions per AI update (in sec)\n"
		"0 disables dynamic updates. 0.002 is a sensible value\n");
	m_cvDynamicVolumeUpdateTime = pConsole->RegisterFloat("ai_DynamicVolumeUpdateTime",0.001f,0,
		"How long (max) to spend updating dynamic volume regions per AI update (in sec)\n"
		"0 disables dynamic updates. 0.002 is a sensible value\n");
  m_cvSmartObjectUpdateTime = pConsole->RegisterFloat("ai_SmartObjectUpdateTime",0.002f,0,
		"How long (max) to spend updating smart objects per AI update (in sec)\n"
		"default value is 0.002");
  m_cvAdjustPathsAroundDynamicObstacles = pConsole->RegisterInt("ai_AdjustPathsAroundDynamicObstacles",1,VF_CHEAT,
		"Set to 1/0 to enable/disable AI path adjustment around dynamic obstacles");
	// is not cheat protected because it changes during game, depending on your settings
	m_cvAIVisRaysPerFrame= pConsole->RegisterInt("ai_MaxVisRaysPerFrame",100,0,
		"Maximum allowed visibility rays per frame - the rest are all assumed to succeed \n"
		"Usage: ai_MaxVisRaysPerFrame <number>\n"
		"Default is 100. \n");
	m_cvRunAccuracyMultiplier = pConsole->RegisterInt("ai_Autobalance",0,0,
		"Set to 1 to enable autobalancing.");
	m_cvTargetMovingAccuracyMultiplier = pConsole->RegisterInt("ai_AllowAccuracyIncrease",0,VF_SAVEGAME,
		"Set to 1 to enable AI accuracy increase when target is standing still.");
	m_cvLateralMovementAccuracyDecay = pConsole->RegisterInt("ai_AllowAccuracyDecrease",1,VF_SAVEGAME,
		"Set to 1 to enable AI accuracy decrease when target is moving lateraly.");
	m_cvSOMSpeedRelaxed = pConsole->RegisterFloat("ai_SOMSpeedRelaxed",1.5f,VF_SAVEGAME,
		"Multiplier for the speed of increase of the Stealth-O-Meter before the AI has seen the enemy.\n"
		"Usage: ai_SOMSpeedRelaxed 1.5\n"
		"Default is 4.5. A lower value causes the AI to react to the enemy\n"
		"to more slowly during combat.");	
	m_cvSOMSpeedCombat = pConsole->RegisterFloat("ai_SOMSpeedCombat",4.5f,VF_SAVEGAME,
		"Multiplier for the speed of increase of the Stealth-O-Meter after the AI has seen the enemy.\n"
		"Usage: ai_SOMSpeedCombat 4.5\n"
		"Default is 4.5. A lower value causes the AI to react to the enemy\n"
		"to more slowly during combat.");	
	m_cvSightRangeDarkIllumMod = pConsole->RegisterFloat("ai_SightRangeDarkIllumMod", 0.5f, VF_SAVEGAME,
		"Multiplier for sightrange when the target is in dark light condition.");
	m_cvSightRangeMediumIllumMod = pConsole->RegisterFloat("ai_SightRangeMediumIllumMod", 0.8f, VF_SAVEGAME,
		"Multiplier for sightrange when the target is in medium light condition.");
	m_cvMovementSpeedDarkIllumMod = pConsole->RegisterFloat("ai_MovementSpeedDarkIllumMod", 0.6f, VF_SAVEGAME,
		"Multiplier for movement speed when the target is in dark light condition.");
	m_cvMovementSpeedMediumIllumMod = pConsole->RegisterFloat("ai_MovementSpeedMediumIllumMod", 0.8f, VF_SAVEGAME,
		"Multiplier for movement speed when the target is in medium light condition.");
	m_cvAllowedTimeForPathfinding = pConsole->RegisterFloat("ai_PathfindTimeLimit",2,0,
		"Specifies how many seconds an individual AI can hold the pathfinder blocked\n"
		"Usage: ai_PathfindTimeLimit 1.5\n"
		"Default is 2. A lower value will result in more path requests that end in NOPATH -\n"
		"although the path may actually exist.");	
	m_cvPathfinderUpdateTime = pConsole->RegisterFloat("ai_PathfinderUpdateTime",0.005f,0,
		"Maximum pathfinder time per AI update\n");
	m_cvDrawAgentFOV  = pConsole->RegisterFloat("ai_DrawagentFOV",0,VF_CHEAT,
		"Toggles the vision cone of the AI agent.\n"
		"Usage: ai_DrawagentFOV [0..1]\n"
		"Default is 0 (off), value 1 will draw the cone all the way to\n"
		"the sight range, value 0.1 will draw the cone to distance of 10%\n"
		"of the sight range, etc. ai_DebugDraw must be enabled before\n"
		"this tool can be used.");
	m_cvAgentStatsDist  = pConsole->RegisterInt("ai_AgentStatsDist",150,VF_CHEAT|VF_DUMPTODISK,
		"Sets agent statistics draw distance, such as current goalpipe, command and target.\n"
		"Usage: ai_AgentStatsDist [view distance]\n"
		"Default is 20 meters. Works with ai_DebugDraw enabled.");
  m_cvDebugDraw  = pConsole->RegisterInt("ai_DebugDraw",0,VF_CHEAT|VF_DUMPTODISK,
		"Toggles the AI debugging view.\n"
		"Usage: ai_DebugDraw [0/1]\n"
		"Default is 0 (off). ai_DebugDraw displays AI rays and targets \n"
		"and enables the view for other AI debugging tools.");
	m_cvDebugDrawVegetationCollisionDist  = pConsole->RegisterInt("ai_DebugDrawVegetationCollisionDist",
    0,VF_CHEAT|VF_DUMPTODISK,
		"Enables drawing vegetation collision closer than a distance projected onto the terrain.");
  m_cvDebugDrawHideSpotRange = pConsole->RegisterInt("ai_DebugDrawHidespotRange",0,VF_CHEAT|VF_DUMPTODISK,
		"Sets the range for drawing hidespots around the player (needs ai_DebugDraw > 0).");
	m_cvDebugDrawDynamicHideObjectsRange = pConsole->RegisterInt("ai_DebugDrawDynamicHideObjectsRange",0,VF_CHEAT|VF_DUMPTODISK,
		"Sets the range for drawing dynamic hide objects around the player (needs ai_DebugDraw > 0).");
	m_cvDebugDrawDeadBodies = pConsole->RegisterInt("ai_DebugDrawDeadBodies",0,VF_CHEAT|VF_DUMPTODISK,
		"Draws the location of dead bodies to avoid during pathfinding (needs ai_DebugDraw > 0).");
  m_cvDebugDrawVolumeVoxels = pConsole->RegisterInt("ai_DebugDrawVolumeVoxels",0,VF_CHEAT|VF_DUMPTODISK,
		"Toggles the AI debugging drawing of voxels in volume generation.\n"
		"Usage: ai_DebugDrawVolumeVoxels [0, 1, 2 etc]\n"
    "Default is 0 (off)\n"
		"+n draws all voxels with original value >= n"
    "-n draws all voxels with original value =  n");
  m_cvDebugPathFinding = pConsole->RegisterInt("ai_DebugPathfinding", 0, 0,
    "Toggles output of pathfinding information [default 0 is off]\n");
	m_cvDebugDrawAStarOpenList = pConsole->RegisterString("ai_DebugDrawAStarOpenList", "0", VF_CHEAT|VF_DUMPTODISK,
		"Draws the A* open list for the specified AI agent.\n"
		"Usage: ai_DebugDrawAStarOpenList [AI agent name]\n"
		"Default is 0, which disables the debug draw. Requires ai_DebugPathfinding=1 to be activated.");
	m_cvDebugDrawBannedNavsos = pConsole->RegisterInt("ai_DebugDrawBannedNavsos", 0, 0,
		"Toggles drawing banned navsos [default 0 is off]\n");
	m_cvDrawNodeLinkType  = pConsole->RegisterInt("ai_DrawNodeLinkType",0,VF_CHEAT,
		"Sets the link parameter to draw with ai_DrawNode.\n"
		"Values are:\n"
		"0 - pass radius (default)\n"
		"1 - exposure\n"
		"2 - water max depth\n"
		"3 - water min depth");
	m_cvDrawNodeLinkCutoff  = pConsole->RegisterFloat("ai_DrawNodeLinkCutoff",0.0f,VF_CHEAT,
		"Sets the link cutoff value in ai_DrawNodeLinkType. If the link value is more than ai_DrawNodeLinkCutoff the number gets displayed in green, otherwise red.");
	m_cvAiSystem = pConsole->RegisterInt("ai_SystemUpdate",1,VF_CHEAT,
		"Toggles the regular AI system update.\n"
		"Usage: ai_SystemUpdate [0/1]\n"
		"Default is 1 (on). Set to 0 to disable ai system updating.");
	m_cvSoundPerception = pConsole->RegisterInt("ai_SoundPerception",1,VF_CHEAT,
		"Toggles AI sound perception.\n"
		"Usage: ai_SoundPerception [0/1]\n"
		"Default is 1 (on). Used to prevent AI from hearing sounds for\n"
		"debugging purposes. Works with ai_DebugDraw enabled.");
	m_cvIgnorePlayer = pConsole->RegisterInt("ai_IgnorePlayer",0,VF_CHEAT,
		"Makes AI ignore the player.\n"
		"Usage: ai_IgnorePlayer [0/1]\n"
		"Default is 0 (off). Set to 1 to make AI ignore the player.\n"
		"Used with ai_DebugDraw enabled.");
  m_cvIgnoreVisibilityChecks = pConsole->RegisterInt("ai_IgnoreVisibilityChecks",0,VF_CHEAT,
    "Makes certain visibility checks (for teleporting etc) return false.");
  m_cvDrawNode	= pConsole->RegisterString("ai_DrawNode","none",VF_CHEAT|VF_DUMPTODISK,
		"Toggles visibility of named agent's position on AI triangulation.\n"
		"Usage: ai_DrawNode [ai agent's name]\n"
		"Default is 0. Set to 1 to show the current triangle on terrain level\n"
		"and closest vertex to player.");
	m_cvDrawLocate	= pConsole->RegisterString("ai_Locate","none",VF_CHEAT|VF_DUMPTODISK,
		"\n");
	m_cvDrawPath				= pConsole->RegisterString("ai_DrawPath","none",VF_CHEAT|VF_DUMPTODISK,
		"Draws the generated paths of the AI agents.\n"
		"Usage: ai_DrawPath [name]\n"
		"Default is none (nobody).");
  m_cvDrawPathAdjustment = pConsole->RegisterString("ai_DrawPathAdjustment","none",VF_CHEAT|VF_DUMPTODISK,
		"Draws the path adjustment for the AI agents.\n"
		"Usage: ai_DrawPathAdjustment [name]\n"
		"Default is none (nobody).");
	m_cvDrawFormations			= pConsole->RegisterInt("ai_DrawFormations",0,VF_CHEAT|VF_DUMPTODISK,
		"Draws all the currently active formations of the AI agents.\n"
		"Usage: ai_DrawFormations [0/1]\n"
		"Default is 0 (off). Set to 1 to draw the AI formations.");
	m_cvDrawPatterns				= pConsole->RegisterInt("ai_DrawPatterns",0,VF_CHEAT|VF_DUMPTODISK,
		"Draws all the currently active track patterns of the AI agents.\n"
		"Usage: ai_DrawPatterns [0/1]\n"
		"Default is 0 (off). Set to 1 to draw the AI track patterns.");
	m_cvDrawSmartObjects		= pConsole->RegisterInt("ai_DrawSmartObjects",0,VF_CHEAT|VF_DUMPTODISK,
		"Draws smart object debug information.\n"
		"Usage: ai_DrawSmartObjects [0/1]\n"
		"Default is 0 (off). Set to 1 to draw the smart objects.");
	m_cvDrawReadibilities		= pConsole->RegisterInt("ai_DrawReadibilities",0,VF_CHEAT|VF_DUMPTODISK,
		"Draws all the currently active readibilities of the AI agents.\n"
		"Usage: ai_DrawReadibilities [0/1]\n"
		"Default is 0 (off). Set to 1 to draw the AI readibilities.");
	m_cvDrawGoals		= pConsole->RegisterInt("ai_DrawGoals",0,VF_CHEAT|VF_DUMPTODISK,
		"Draws all the active goal ops debug info.\n"
		"Usage: ai_DrawGoals [0/1]\n"
		"Default is 0 (off). Set to 1 to draw the AI goal op debug info.");
	m_cvAllTime		= pConsole->RegisterInt("ai_AllTime",0,VF_CHEAT,
		"Displays the update times of all agents, in milliseconds.\n"
		"Usage: ai_AllTime [0/1]\n"
		"Default is 0 (off). Times all agents and displays the time used updating\n"
		"each of them. The name is colour coded to represent the update time.\n"
		"	Green: less than 1 ms (ok)\n"
		"	White: 1 ms to 5 ms\n"
		"	Red: more than 5 ms\n"
		"You must enable ai_DebugDraw before you can use this tool.");
	m_cvDrawHide		= pConsole->RegisterInt("ai_HideDraw",0,VF_CHEAT,
		"Toggles the triangulation display.\n"
		"Usage: ai_HideDraw [0/1]\n"
		"Default is 0 (off). Set to 1 to display the triangulation\n"
		"which the AI uses to hide (objects made 'hideable' affect\n"
		"AI triangulation). When ai_HideDraw is off, the normal\n"
		"obstacle triangulation is displayed instead.\n"
		"Used with ai_DebugDraw and other AI path tools.");
	m_cvProfileGoals		= pConsole->RegisterInt("ai_ProfileGoals",0,VF_CHEAT,
		"Toggles timing of AI goal execution.\n"
		"Usage: ai_ProfileGoals [0/1]\n"
		"Default is 0 (off). Records the time used for each AI goal (like\n"
		"approach, run or pathfind) to execute. The longest execution time\n"
		"is displayed on screen. Used with ai_DebugDraw enabled.");
	m_cvBeautifyPath = pConsole->RegisterInt("ai_BeautifyPath",1,VF_CHEAT,
		"Toggles AI optimisation of the generated path.\n"
		"Usage: ai_BeautifyPath [0/1]\n"
		"Default is 1 (on). Optimisation is on by default. Set to 0 to\n"
		"disable path optimisation (AI uses non-optimised path).");
  m_cvAttemptStraightPath = pConsole->RegisterInt("ai_AttemptStraightPath",1,VF_CHEAT,
		"Toggles AI attempting a simple straight path when possible.\n"
		"Default is 1 (on).");
  m_cvCrowdControlInPathfind = pConsole->RegisterInt("ai_CrowdControlInPathfind",1,VF_CHEAT,
		"Toggles AI using crowd control in pathfinding.\n"
		"Usage: ai_CrowdControlInPathfind [0/1]\n"
		"Default is 1 (on).");
	m_cvDebugDrawCrowdControl = pConsole->RegisterInt("ai_DebugDrawCrowdControl",0,VF_CHEAT,
		"Draws crowd control debug information. 0=off, 1=on");
  m_cvPredictivePathFollowing = pConsole->RegisterInt("ai_PredictivePathFollowing",1,VF_CHEAT,
    "Sets if AI should use the predictive path following if allowed by the type's config.");
  m_cvSteepSlopeUpValue = pConsole->RegisterFloat("ai_SteepSlopeUpValue",1.0f,VF_CHEAT,
		"Indicates slope value that is borderline-walkable up.\n"
		"Usage: ai_SteepSlopeUpValue 0.5\n"
		"Default is 1.0 Zero means flat. Infinity means vertical. Set it smaller than ai_SteepSlopeAcrossValue");
  m_cvSteepSlopeAcrossValue = pConsole->RegisterFloat("ai_SteepSlopeAcrossValue",0.6f,VF_CHEAT,
		"Indicates slope value that is borderline-walkable across.\n"
		"Usage: ai_SteepSlopeAcrossValue 0.8\n"
		"Default is 0.6 Zero means flat. Infinity means vertical. Set it greater than ai_SteepSlopeUpValue\n");
  m_cvUpdateProxy = pConsole->RegisterInt("ai_UpdateProxy",1,VF_CHEAT,
		"Toggles update of AI proxy (model).\n"
		"Usage: ai_UpdateProxy [0/1]\n"
		"Default is 1 (on). Updates proxy (AI representation in game)\n"
		"set to 0 to disable proxy updating.");
	m_cvDrawType = pConsole->RegisterInt("ai_DrawType",-1,VF_CHEAT|VF_DUMPTODISK,
		"Toggles drawing AI objects of particular type for debugging AI.\n");
  m_cvDrawRefPoints = pConsole->RegisterString("ai_DrawRefPoints","",VF_CHEAT|VF_DUMPTODISK,
		"Toggles reference points view for debugging AI.\n"
		"Usage: ai_DrawRefPoints [0/1]\n"
		"Default is 0 (off). Indicates the AI reference points by drawing\n"
		"cyan balls at their positions.");	
  m_cvStatsTarget = pConsole->RegisterString("ai_StatsTarget","none",VF_CHEAT|VF_DUMPTODISK,
		"Focus debugging information on a specific AI\n"
		"Usage: ai_StatsTarget AIName\n"
		"Default is 'none'. AIName is the name of the AI\n"
		"on which to focus.");
	m_cvDrawModifiers			= pConsole->RegisterInt("ai_DrawModifiers",0,VF_CHEAT,
		"Toggles the AI debugging view of navigation modifiers.");
	m_cvDrawOffset			= pConsole->RegisterFloat("ai_DrawOffset",0.1f,VF_CHEAT,
		"vertical offset during debug drawing", SetDebugDrawOffset);
	m_cvDrawTargets			= pConsole->RegisterInt("ai_DrawTargets",0,VF_CHEAT|VF_DUMPTODISK, "");
	m_cvDrawBadAnchors = pConsole->RegisterInt("ai_DrawBadAnchors",-1,VF_CHEAT|VF_DUMPTODISK,
		"Toggles drawing out of bounds AI objects of particular type for debugging AI.");
	m_cvDrawStats			= pConsole->RegisterInt("ai_DrawStats",00,VF_CHEAT|VF_DUMPTODISK,
		"Toggles drawing stats for AI objects withing range.");
	m_cvExtraRadiusDuringBeautification = pConsole->RegisterFloat("ai_ExtraRadiusDuringBeautification",
		0.2f,VF_CHEAT,
		"Extra radius added to agents during beautification.");
  m_cvRadiusForAutoForbidden = pConsole->RegisterFloat("ai_RadiusForAutoForbidden",
		1.0f,VF_CHEAT, 
		"If object/vegetation radius is more than this then an automatic forbidden area is created during triangulation.");
	m_cvExtraForbiddenRadiusDuringBeautification = pConsole->RegisterFloat("ai_ExtraForbiddenRadiusDuringBeautification",
		1.0f,VF_CHEAT,
		"Extra radius added to agents close to forbidden edges during beautification.");
	m_cvRecordLog			= pConsole->RegisterInt("ai_RecordLog",0,VF_CHEAT|VF_DUMPTODISK,
		"log all the AI state changes on stats_target");
	m_cvRecordFilter			= pConsole->RegisterInt("ai_RecordFilter",0,VF_CHEAT|VF_DUMPTODISK, "");
	m_cvDrawGroups			= pConsole->RegisterInt("ai_DrawGroup",-2,VF_CHEAT|VF_DUMPTODISK,
		"draw groups: positive value - group ID to draw; -1 - all groups; -2 - nothing\n");
	m_cvDrawGroupTactic	= pConsole->RegisterInt("ai_DrawGroupTactic",0,VF_CHEAT|VF_DUMPTODISK,
		"draw group tactic: 0 = disabled, 1 = draw simple, 2 = draw complex.");
	m_cvDrawTrajectory	= pConsole->RegisterInt("ai_DrawTrajectory",0,VF_CHEAT|VF_DUMPTODISK,
		"Records and draws the trajectory of the stats target: 0=do not record, 1=record.");
	m_cvDrawHideSpots	= pConsole->RegisterInt("ai_DrawHidespots",0,VF_CHEAT|VF_DUMPTODISK,
		"Draws latest hide-spot positions for all agents withing specified range.");
	m_cvDrawRadar	= pConsole->RegisterInt("ai_DrawRadar",0,VF_CHEAT|VF_DUMPTODISK,
		"Draws AI radar: 0=no radar, >0 = size of the radar on screen");
	m_cvDrawRadarDist	= pConsole->RegisterInt("ai_DrawRadarDist",20,VF_CHEAT|VF_DUMPTODISK,
		"AI radar draw distance in meters, default=20m.");
	m_cvTickCounter	= pConsole->RegisterInt("ai_TickCounter",0,VF_CHEAT|VF_DUMPTODISK,
		"Enables AI tick counter\n");
	// should be off for release
	m_cvDebugRecord	= pConsole->RegisterInt("ai_Recorder",0,VF_CHEAT|VF_DUMPTODISK,
		"Enables AI debug recording\n");
	m_cvDebugRecordBuffer	= pConsole->RegisterInt("ai_Recorder_Buffer",1024,VF_CHEAT|VF_DUMPTODISK,
		"Set the size of the AI debug recording buffer\n");
	m_cvDrawShooting = pConsole->RegisterString("ai_DrawShooting","none",VF_CHEAT,
		"Name of puppet to show fire stats");
	m_cvDrawExplosions = pConsole->RegisterInt("ai_DrawExplosions",0,VF_CHEAT|VF_DUMPTODISK,
		"enable displaying all the registered explosion-spots/craters");
	m_cvDrawDistanceLUT = pConsole->RegisterInt("ai_DrawDistanceLUT",0,VF_CHEAT,
		"Draws the distance lookup table graph overlay.");
  m_cvPuppetDirSpeedControl = pConsole->RegisterInt("ai_PuppetDirSpeedControl", 1, 0,
    "Does puppet speed control based on their current move dir");
	m_cvDrawAreas = pConsole->RegisterInt("ai_DrawAreas", 0, VF_CHEAT|VF_DUMPTODISK,
		"Enables/Disables drawing behavior related areas.");
	m_cvDrawProbableTarget = pConsole->RegisterInt("ai_DrawProbableTarget", 0, VF_CHEAT|VF_DUMPTODISK,
		"Enables/Disables drawing the position of probable target.");
	m_cvIncludeNonColEntitiesInNavigation = pConsole->RegisterInt("ai_IncludeNonColEntitiesInNavigation", 1, 0,
		"Includes/Excludes noncolliding objects from navigation.");
	m_cvDebugDrawDamageParts = pConsole->RegisterInt("ai_DebugDrawDamageParts", 0, VF_CHEAT|VF_DUMPTODISK,
		"Draws the damage parts of puppets and vehicles.");
	m_cvDebugDrawStanceSize = pConsole->RegisterInt("ai_DebugDrawStanceSize", 0, VF_CHEAT|VF_DUMPTODISK,
		"Draws the game logic representation of the stance size of the AI agents.");
	m_cvDebugDrawObstrSpheres = pConsole->RegisterInt("ai_DebugDrawObstrSpheres", 0, VF_CHEAT|VF_DUMPTODISK,
		"Draws all the existing obstruction spheres.");
	m_cvUseObjectPosWithExactPos = pConsole->RegisterInt("ai_UseObjectPosWithExactPos", 0, VF_CHEAT|VF_DUMPTODISK,
		"Use object position when playing exact positioning.");

	m_cvDebugTargetSilhouette = pConsole->RegisterInt("ai_DebugTargetSilhouette", 0, VF_CHEAT|VF_DUMPTODISK,
		"Draws the silhouette used for missing the target while shooting.");

	m_cvRODAliveTime = pConsole->RegisterFloat("ai_RODAliveTime", 6.0f, VF_CHEAT|VF_DUMPTODISK,
		"The base level time the player can survive under fire.");
	m_cvRODMoveInc = pConsole->RegisterFloat("ai_RODMoveInc", 3.0f, VF_CHEAT|VF_DUMPTODISK,
		"Increment how the speed of the target affects the alive time (the value is doubled for supersprint). 0=disable");
	m_cvRODStanceInc = pConsole->RegisterFloat("ai_RODStanceInc", 2.0f, VF_CHEAT|VF_DUMPTODISK,
		"Increment how the stance of the target affects the alive time, 0=disable.\n"
		"The base value is for crouch, and it is doubled for prone.\n"
		"The crouch inc is disable in kill-zone and prone in kill and combat-near -zones");
	m_cvRODDirInc = pConsole->RegisterFloat("ai_RODDirInc", 0.0f, VF_CHEAT|VF_DUMPTODISK,
		"Increment how the orientation of the target affects the alive time. 0=disable");
	m_cvRODKillZoneInc = pConsole->RegisterFloat("ai_RODKillZoneInc", -4.0f, VF_CHEAT|VF_DUMPTODISK,
		"Increment how the target is within the kill-zone of the target.");
	m_cvRODAmbientFireInc = pConsole->RegisterFloat("ai_RODAmbientFireInc", 3.0f, VF_CHEAT|VF_DUMPTODISK,
		"Increment for the alive time when the target is within the kill-zone of the target.");

	m_cvRODKillRangeMod = pConsole->RegisterFloat("ai_RODKillRangeMod", 0.15f, VF_CHEAT|VF_DUMPTODISK,
		"Kill-zone distance = attackRange * killRangeMod.");
	m_cvRODCombatRangeMod = pConsole->RegisterFloat("ai_RODCombatRangeMod", 0.55f, VF_CHEAT|VF_DUMPTODISK,
		"Combat-zone distance = attackRange * combatRangeMod.");

	m_cvRODCoverFireTimeMod = pConsole->RegisterFloat("ai_RODCoverFireTimeMod", 1.0f, VF_CHEAT|VF_DUMPTODISK,
		"Multiplier for cover fire times set in weapon descriptor.");

	m_cvRODReactionTime = pConsole->RegisterFloat("ai_RODReactionTime", 0.2f, VF_CHEAT|VF_DUMPTODISK,
		"Uses rate of death as damage control method.");
	m_cvRODReactionDistInc = pConsole->RegisterFloat("ai_RODReactionDistInc", 0.1f, VF_CHEAT|VF_DUMPTODISK,
		"Increase for the reaction time when the target is in combat-far-zone or warn-zone.\n"
		"In warn-zone the increase is doubled.");
	m_cvRODReactionDirInc = pConsole->RegisterFloat("ai_RODReactionDirInc", 1.0f, VF_CHEAT|VF_DUMPTODISK,
		"Increase for the reaction time when the enemy is outside the players FOV or near the edge of the FOV.\n"
		"The increment is doubled when the target is behind the player.");
	m_cvRODReactionLeanInc = pConsole->RegisterFloat("ai_RODReactionLeanInc", 0.2f, VF_CHEAT|VF_DUMPTODISK,
		"Increase to the reaction to when the target is leaning.");
	m_cvRODReactionDarkIllumInc = pConsole->RegisterFloat("ai_RODReactionDarkIllumInc", 0.3f, VF_CHEAT|VF_DUMPTODISK,
		"Increase for reaction time when the target is in dark light condition.");
	m_cvRODReactionMediumIllumInc = pConsole->RegisterFloat("ai_RODReactionMediumIllumInc", 0.2f, VF_CHEAT|VF_DUMPTODISK,
		"Increase for reaction time when the target is in medium light condition.");

	m_cvRODLowHealthMercyTime = pConsole->RegisterFloat("ai_RODLowHealthMercyTime", 4.0f, VF_CHEAT|VF_DUMPTODISK,
		"The amount of time the AI will not hit the target when the target crosses the low health threshold.");

	m_cvAmbientFireUpdateInterval = pConsole->RegisterFloat("ai_AmbientFireUpdateInterval", 2.0f, VF_CHEAT|VF_DUMPTODISK,
		"Ambient fire update interval. Controls how often puppet's ambient fire status is updated.");
	m_cvAmbientFireQuota = pConsole->RegisterInt("ai_AmbientFireQuota", 2, VF_CHEAT|VF_DUMPTODISK,
		"Number of units allowed to hit the player at a time.");
	m_cvDebugDrawAmbientFire = pConsole->RegisterInt("ai_DebugDrawAmbientFire", 0, VF_CHEAT|VF_DUMPTODISK,
		"Displays fire quota on puppets.");

	m_cvDebugDrawExpensiveAccessoryQuota = pConsole->RegisterInt("ai_DebugDrawExpensiveAccessoryQuota", 0, VF_CHEAT|VF_DUMPTODISK,
		"Displays expensive accessory usage quota on puppets.");

	m_cvDebugDrawDamageControl = pConsole->RegisterInt("ai_DebugDrawDamageControl", 0, VF_CHEAT|VF_DUMPTODISK,
		"Debugs the damage control system 0=disabled, 1=collect, 2=collect&draw.");

	m_cvDrawFakeTracers = pConsole->RegisterInt("ai_DrawFakeTracers", 0, VF_CHEAT|VF_DUMPTODISK,
		"Draws fake tracers around the player.");
	m_cvDrawFakeHitEffects = pConsole->RegisterInt("ai_DrawFakeHitEffects", 0, VF_CHEAT|VF_DUMPTODISK,
		"Draws fake hit effects the player.");
	m_cvDrawFakeDamageInd = pConsole->RegisterInt("ai_DrawFakeDamageInd", 0, VF_CHEAT|VF_DUMPTODISK,
		"Draws fake damage indicators on the player.");

	m_cvProtoROD = pConsole->RegisterInt("ai_ProtoROD", 0, VF_CHEAT|VF_DUMPTODISK, "Proto");
	m_cvProtoRODFireRange = pConsole->RegisterFloat("ai_ProtoRODFireRange", 35.0f, VF_CHEAT|VF_DUMPTODISK, "Proto");
	m_cvProtoRODAliveTime = pConsole->RegisterFloat("ai_ProtoRODAliveTime", 10.0f, VF_CHEAT|VF_DUMPTODISK, "Proto");
	m_cvProtoRODRegenTime = pConsole->RegisterFloat("ai_ProtoRODRegenTime", 8.0f, VF_CHEAT|VF_DUMPTODISK, "Proto");
	m_cvProtoRODReactionTime = pConsole->RegisterFloat("ai_ProtoRODReactionTime", 2.0f, VF_CHEAT|VF_DUMPTODISK, "Proto");
	m_cvProtoRODLogScale = pConsole->RegisterInt("ai_ProtoRODLogScale", 0, VF_CHEAT|VF_DUMPTODISK, "Proto");
	m_cvProtoRODAffectMove = pConsole->RegisterInt("ai_ProtoRODAffectMove", 0, VF_CHEAT|VF_DUMPTODISK, "Proto");
	m_cvProtoRODHealthGraph = pConsole->RegisterInt("ai_ProtoRODHealthGraph", 0, VF_CHEAT|VF_DUMPTODISK, "Proto");
	m_cvProtoRODSilhuette = pConsole->RegisterInt("ai_ProtoRODSilhuette", 0, VF_CHEAT|VF_DUMPTODISK, "Proto");
	m_cvProtoRODSpeedMod = pConsole->RegisterInt("ai_ProtoRODSpeedMod", 1, VF_CHEAT|VF_DUMPTODISK, "Proto");
	m_cvProtoRODGrenades = pConsole->RegisterInt("ai_ProtoRODGrenades", 1, VF_CHEAT|VF_DUMPTODISK, "Proto");

	m_cvForceStance = pConsole->RegisterInt("ai_ForceStance", -1, VF_CHEAT,
    "Forces all AI characters to specified stance: disable = -1, stand = 0, crouch = 1, prone = 2, relaxed = 3, stealth = 4, swim = 5, zero-g = 6");
  m_cvForceAllowStrafing = pConsole->RegisterInt("ai_ForceAllowStrafing", -1, VF_CHEAT,
    "Forces all AI characters to use/not use strafing (-1 disables)");
  m_cvForceLookAimTarget = pConsole->RegisterString("ai_ForceLookAimTarget", "none", VF_CHEAT,
    "Forces all AI characters to use/not use a fixed look/aim target\n"
    "none disables\n"
    "x, y, xz or yz sets it to the appropriate direction\n"
    "otherwise it forces looking/aiming at the entity with this name (no name -> (0, 0, 0))");
	m_cvInterestSystem = pConsole->RegisterInt("ai_InterestSystem", 0, VF_CHEAT|VF_DUMPTODISK,
		"Enable interest system");
	m_cvDebugInterest = pConsole->RegisterInt("ai_DebugInterestSystem", 0, VF_CHEAT|VF_DUMPTODISK,
		"Display debugging information on interest system");
	m_cvEnableInterestScan = pConsole->RegisterInt("ai_InterestEnableScan", 0, VF_CHEAT|VF_DUMPTODISK,
		"Enable interest system scan mode");
	//m_cvInterestEntryBoost
	m_cvInterestSwitchBoost = pConsole->RegisterFloat("ai_InterestSwitchBoost", 2.0f, VF_CHEAT|VF_DUMPTODISK,
		"Multipler applied when we switch to an interest item; higher values maintain interest for longer (always > 1)");
	//m_cvInterestSpeedMultiplier;
	m_cvInterestListenMovement = pConsole->RegisterInt("ai_InterestDetectMovement", 0, VF_CHEAT|VF_DUMPTODISK,
		"Enable movement detection in interest system");
	m_cvInterestScalingScan = pConsole->RegisterFloat("ai_InterestScalingScan", 50.0f, VF_CHEAT|VF_DUMPTODISK,
		"Scale the interest value given to passively scanning the environment");
	m_cvInterestScalingAmbient = pConsole->RegisterFloat("ai_InterestScalingAmbient", 1.0f, VF_CHEAT|VF_DUMPTODISK,
		"Scale the interest value given to Ambient interest items (e.g. static/passive objects)");
	m_cvInterestScalingMovement = pConsole->RegisterFloat("ai_InterestScalingMovement", 1.0f, VF_CHEAT|VF_DUMPTODISK,
		"Scale the interest value given to Pure Movement interest items (e.g. something rolling, falling, swinging)");
	m_cvInterestScalingEyeCatching = pConsole->RegisterFloat("ai_InterestScalingEyeCatching", 14.0f, VF_CHEAT|VF_DUMPTODISK,
		"Scale the interest value given to Eye Catching interest items (e.g. moving vehicles, birds, people)");
	m_cvInterestScalingView = pConsole->RegisterFloat("ai_InterestScalingView", 1.0f, VF_CHEAT|VF_DUMPTODISK,
		"Scale the interest value given to View interest items (e.g. a pretty castle, the horizon)");

	m_cvBannedNavSoTime = pConsole->RegisterFloat("ai_BannedNavSoTime", 20.0f, VF_CHEAT|VF_DUMPTODISK,
		"Time indicating how long invalid navsos should be banned.");

	m_cvDebugDrawAdaptiveUrgency = pConsole->RegisterInt("ai_DebugDrawAdaptiveUrgency", 0, VF_CHEAT|VF_DUMPTODISK,
		"Enables drawing the adaptive movement urgency.");
	m_cvDebugDrawReinforcements = pConsole->RegisterInt("ai_DebugDrawReinforcements", -1, VF_CHEAT|VF_DUMPTODISK,
		"Enables debug draw for reinforcement logic for specified group.\n"
		"Usage: ai_DebugDrawReinforcements <groupid>, or -1 to disable.");
	m_cvDebugDrawCollisionEvents = pConsole->RegisterInt("ai_DebugDrawCollisionEvents", 0, VF_CHEAT|VF_DUMPTODISK,
		"Debug draw the collision events the AI system processes. 0=disable, 1=enable.");
	m_cvDebugDrawBulletEvents = pConsole->RegisterInt("ai_DebugDrawBulletEvents", 0, VF_CHEAT|VF_DUMPTODISK,
		"Debug draw the bullet events the AI system processes. 0=disable, 1=enable.");
	m_cvDebugDrawSoundEvents = pConsole->RegisterInt("ai_DebugDrawSoundEvents", 0, VF_CHEAT|VF_DUMPTODISK,
		"Debug draw the sound events the AI system processes. 0=disable, 1=enable.");
	m_cvDebugDrawGrenadeEvents = pConsole->RegisterInt("ai_DebugDrawGrenadeEvents", 0, VF_CHEAT|VF_DUMPTODISK,
		"Debug draw the grenade events the AI system processes. 0=disable, 1=enable.");
	m_cvDebugDrawPlayerActions = pConsole->RegisterInt("ai_DebugDrawPlayerActions", 0, VF_CHEAT|VF_DUMPTODISK,
		"Debug draw special player actions.");

	m_cvWaterOcclusion = pConsole->RegisterFloat("ai_WaterOcclusion", .5f, VF_CHEAT|VF_DUMPTODISK,
		"scales how much water hides player from AI");

	m_cvCloakMinDist = pConsole->RegisterFloat("ai_CloakMinDist", 1.f, VF_CHEAT|VF_DUMPTODISK,
		"closer than that - cloak not effective");
	m_cvCloakMaxDist = pConsole->RegisterFloat("ai_CloakMaxDist", 5.f, VF_CHEAT|VF_DUMPTODISK,
		"closer than that - cloak starts to fade out");
	m_cvCloakIncrement = pConsole->RegisterFloat("ai_CloakIncrementMod", 1.f, VF_CHEAT|VF_DUMPTODISK,
		"how fast cloak fades out");

	m_cvBigBrushLimitSize = pConsole->RegisterFloat("ai_BigBrushCheckLimitSize", 15.f, VF_CHEAT|VF_DUMPTODISK,
		"to be used for finding big objects not enclosed into forbidden areas");

	m_cvDebugDrawUpdate	= pConsole->RegisterInt("ai_DrawUpdate",0,VF_CHEAT|VF_DUMPTODISK,
		"list of AI forceUpdated entities");

	m_cvDebugDrawLightLevel	= pConsole->RegisterInt("ai_DebugDrawLightLevel",0,VF_CHEAT|VF_DUMPTODISK,
		"Debug AI light level manager");

	m_cvDebugDrawHashSpaceAround =  pConsole->RegisterString("ai_DebugDrawHashSpaceAround","0",VF_CHEAT|VF_DUMPTODISK,
		"Validates and draws the navigation node hash space around specified entity.");

	m_cvDebugDrawVisChecQueue	= pConsole->RegisterInt("ai_DrawVisCheckQueue",0,VF_CHEAT|VF_DUMPTODISK,
		"list of pending vis-check trace requests");

	m_cvSimpleWayptPassability = pConsole->RegisterInt("ai_SimpleWayptPassability",1,VF_CHEAT|VF_DUMPTODISK,
		"Use simplified and faster passability recalculation for human waypoint links where possible.");

	m_cvUseAlternativeReadability = pConsole->RegisterInt("ai_UseAlternativeReadability", 0, VF_SAVEGAME,
		"Switch between normal and alternative SoundPack for AI readability.");

	m_threadedVolumeNavPreprocess = pConsole->RegisterInt("ai_ThreadedVolumeNavPreprocess",1,VF_CHEAT|VF_DUMPTODISK,
		"Parallelizes volume navigation preprocessing by running it on multiple threads.\n"
		"If you experience freezes during volume nav export or corrupted volume nav data, try turning this off. ;)"
	);
	m_extraVehicleAvoidanceRadiusBig = pConsole->RegisterFloat("ai_ExtraVehicleAvoidanceRadiusBig", 4.0f, VF_CHEAT|VF_DUMPTODISK,
		"Value in meters to be added to a big obstacle's own size while computing obstacle size for purposes of vehicle steering."
		"See also ai_ObstacleSizeThreshold.");
	m_extraVehicleAvoidanceRadiusSmall = pConsole->RegisterFloat("ai_ExtraVehicleAvoidanceRadiusSmall", 0.5f, VF_CHEAT|VF_DUMPTODISK,
		"Value in meters to be added to a big obstacle's own size while computing obstacle size for purposes of vehicle steering."
		"See also ai_ObstacleSizeThreshold.");
	m_obstacleSizeThreshold = pConsole->RegisterFloat("ai_ObstacleSizeThreshold", 1.2f, VF_CHEAT|VF_DUMPTODISK,
		"Obstacle size in meters that differentiates small obstacles from big ones so that vehicles can ignore the small ones");
	m_cvDrawGetEnclosingFailures = pConsole->RegisterFloat("ai_DrawGetEnclosingFailures", 0.0f, VF_CHEAT|VF_DUMPTODISK,
		"Set to the number of seconds you want GetEnclosing() failures visualized.  Set to 0 to turn visualization off.");

	pConsole->AddCommand("ai_CheckGoalpipes", (ConsoleCommandFunc)CheckGoalpipes, VF_CHEAT, "Checks goalpipes and dumps report to console.");
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::Init()
{
	AILogProgress("[AISYSTEM] Initialization started.");

	Reset(IAISystem::RESET_INTERNAL);

	m_pAutoBalance = new CAIAutoBalance;
	m_nTickCount = 0;
	m_nNumBuildings = 0;
	m_pWorld = m_pSystem->GetIPhysicalWorld();
	
	m_fFrameStartTime = m_pSystem->GetITimer()->GetFrameStartTime();
	m_fLastPuppetUpdateTime = m_fFrameStartTime;
	m_fFrameDeltaTime = 0.0f;

	//m_vWaitingToBeUpdated.reserve(100);
	//m_vAlreadyUpdated.reserve(100);
	//m_vWaitingToBeUpdated.clear();
	//m_vAlreadyUpdated.clear();

	// Register fire command factories.
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("instant", CFireCommandInstant));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("instant_single", CFireCommandInstantSingle));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("projectile_slow", CFireCommandProjectileSlow));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("projectile_fast", CFireCommandProjectileFast));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("projectile_fast_xp", CFireCommandProjectileXP));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("strafing", CFireCommandStrafing));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("hurricane", CFireCommandHurricane));
	// TODO: move this to game.dll
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("fast_light_moar", CFireCommandFastLightMOAR));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("hunter_moar", CFireCommandHunterMOAR));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("hunter_sweep_moar", CFireCommandHunterSweepMOAR));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("hunter_singularity_cannon", CFireCommandHunterSingularityCannon));
	RegisterFirecommandHandler(CREATE_FIRECOMMAND_DESC("grenade", CFireCommandGrenade));

	m_bInitialized = true;

	AILogProgress("[AISYSTEM] Initialization finished.");

	return true;
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::InitSmartObjects()
{
	AILogProgress("[AISYSTEM] Initializing AI Actions.");

	m_pActionManager = new CAIActionManager();
	m_pActionManager->LoadLibrary( AI_ACTIONS_PATH );

	AILogProgress("[AISYSTEM] Initializing Smart Objects.");

	m_pSmartObjects = new CSmartObjects();
	m_pSystem->GetIEntitySystem()->AddSink( m_pSmartObjects );

	LoadSmartObjectsLibrary();

	return true;
}

//====================================================================
// GetUpdateInterval
//====================================================================
float CAISystem::GetUpdateInterval() const 
{
	if (m_cvAIUpdateInterval)
		return m_cvAIUpdateInterval->GetFVal();
	else
		return 0.1f;
}

//====================================================================
// PointInBrokenRegion
//====================================================================
bool CAISystem::PointInBrokenRegion(const Vec3 & point) const
{
	for (std::list<CGraph::CRegion>::const_iterator regionIt = m_brokenRegions.m_regions.begin() ; 
				regionIt != m_brokenRegions.m_regions.end() ; 
				++regionIt)
	{
		if (Overlap::Point_Polygon2D(point, regionIt->m_outline))
		{
			return true;
		}
	}
	return false;
}

//====================================================================
// InvalidatePathsThroughArea
//====================================================================
void CAISystem::InvalidatePathsThroughArea(const ListPositions& areaShape)
{
	for (AIObjects::iterator it = m_Objects.begin() ; it != m_Objects.end() ; ++it)
	{
		CAIObject* pObject = it->second;
		CPipeUser* pPiper = pObject->CastToCPipeUser();
		if (pPiper)
		{
			const CNavPath & path = pPiper->m_Path;
			const TPathPoints & listPath = path.GetPath();
			Vec3 pos = pPiper->GetPhysicsPos();
			for (TPathPoints::const_iterator pathIt = listPath.begin() ; pathIt != listPath.end() ; ++pathIt)
			{
				Lineseg pathSeg(pos, pathIt->vPos);
				pos = pathIt->vPos;
				if (Overlap::Lineseg_Polygon2D(pathSeg, areaShape))
				{
					pPiper->PathIsInvalid();
					goto nextPiper;
				}
			}
		}
nextPiper:
	int junkForMS = 0;
	} // loop over objects
}

//====================================================================
// InvalidatePathsThroughPendingBrokenRegions
//====================================================================
void CAISystem::InvalidatePathsThroughPendingBrokenRegions()
{
	for (std::list<CGraph::CRegion>::const_iterator regionIt = m_pendingBrokenRegions.m_regions.begin() ; 
				regionIt != m_pendingBrokenRegions.m_regions.end() ; 
				++regionIt)
	{
		InvalidatePathsThroughArea(regionIt->m_outline);
	} // loop over pending broken regions
}


//====================================================================
// RescheduleCurrentPathfindRequest
//====================================================================
void CAISystem::RescheduleCurrentPathfindRequest()
{
	if (m_pCurrentRequest)
	{
		m_pAStarSolver->AbortAStar();
		m_lstPathQueue.push_back(m_pCurrentRequest);
		m_pCurrentRequest = 0;
	}
	m_nPathfinderResult = PATHFINDER_POPNEWREQUEST;
}



//
//-----------------------------------------------------------------------------------------------------------
int CAISystem::GetAlertness() const
{
	int maxAlertness = 0;
	for (int i = 3; i; i--)
		if (m_AlertnessCounters[i])
		{
			maxAlertness = i;
			break;
		}
	return	maxAlertness;
}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::ShutDown()
{
	Reset(IAISystem::RESET_INTERNAL);

	if (!m_setDummies.empty())
	{
		for (SetAIObjects::iterator li=m_setDummies.begin();li!=m_setDummies.end();++li)
		{
			CAIObject *pObject = (*li);
			delete pObject;
		}
		m_setDummies.clear();
	}


	if (m_pTriangulator)
	{
		delete m_pTriangulator;
		m_pTriangulator = 0;
	}
	
	m_PipeManager.ShutDown();

	if (!m_Objects.empty())
	{
		AIObjects::iterator ai;
		for (ai=m_Objects.begin();ai!=m_Objects.end();++ai)
		{
			CAIObject *pObject = ai->second;
			pObject->Release();
		}

		m_Objects.clear();
	}

	delete m_pAutoBalance;
	m_pAutoBalance = 0;
		
	m_mapSpecies.clear();
	m_mapGroups.clear();
	m_mapBeacons.clear();
//	m_mapLeaders.clear();

	for(AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
		delete it->second;
	m_mapAIGroups.clear();

	FlushAllAreas();

	// no need to remove sink! EntitySystem is already deleted.
	//	m_pSystem->GetIEntitySystem()->RemoveSink( m_pSmartObjects );
	delete m_pSmartObjects;
	delete m_pActionManager;

	// Release fire command factory.
	for(std::list<IFireCommandDesc*>::iterator it = m_firecommandDescriptors.begin(); it != m_firecommandDescriptors.end(); ++it)
		(*it)->Release();
	m_firecommandDescriptors.clear();

}


//
//-----------------------------------------------------------------------------------------------------------
IPhysicalWorld * CAISystem::GetPhysicalWorld()
{
	return m_pWorld;
}

//
//-----------------------------------------------------------------------------------------------------------
CGraph * CAISystem::GetGraph(bool hide)
{
  return hide ? m_pHideGraph : m_pGraph;
}

//
//-----------------------------------------------------------------------------------------------------------
IAIObject *CAISystem::CreateAIObject(unsigned short type, IAIObject *pAssociation)
{
	if(!IsEnabled())
		return 0;
	if (!m_bInitialized)
	{
		AIError("CAISystem::CreateAIObject called on an uninitialized AI system [Code bug]");
		return 0;
	}

	CAIObject *pObject;
	

	switch (type)
	{
	case AIOBJECT_PUPPET:
		pObject = new CPuppet(this);
		break;
	case AIOBJECT_2D_FLY:
		type = AIOBJECT_PUPPET;
		pObject = new CPuppet(this);
		pObject->SetSubType(CAIObject::STP_2D_FLY);
		break;
	case AIOBJECT_LEADER:
		{
			int iGroup = pAssociation ? static_cast< CPuppet* >(pAssociation)->GetGroupId() : -1;
			CLeader *pLeader(new CLeader(this,iGroup));
			pObject = pLeader;
			if (pAssociation)
				AddToGroup(pLeader);
		}
		break;
	case AIOBJECT_ATTRIBUTE:
		pObject = new CAIObject(this);
		break;
	case AIOBJECT_BOAT:
		type = AIOBJECT_VEHICLE;
		pObject = new CAIVehicle(this);
		pObject->SetSubType(CAIObject::STP_BOAT);
		//				pObject->m_bNeedsPathOutdoor = false;
		break;
	case AIOBJECT_CAR:
		type = AIOBJECT_VEHICLE;
		pObject = new CAIVehicle(this);
		pObject->SetSubType(CAIObject::STP_CAR);
		break;
	case AIOBJECT_HELICOPTER:
		type = AIOBJECT_VEHICLE;
		pObject = new CAIVehicle(this);
		pObject->SetSubType(CAIObject::STP_HELI);
		break;
	case AIOBJECT_PLAYER:
		{
			pObject = new CAIPlayer(this); // just a dummy for the player	
			
			pObject->Event(AIEVENT_ENABLE, NULL);
			//pObject->m_bEnabled = true;
/*
			CAIObject *pPlayerLeader = GetLeader((int)0);
			if(pPlayerLeader)
				RemoveObject(pPlayerLeader);
			//				if(!GetLeader((int)0))
			//				{
			CAIObject* pLeader = (CAIObject*)CreateAIObject(AIOBJECT_LEADER, pObject);
			pLeader->SetAssociation(pObject);
			//				}
*/
		}
		break;
	case AIOBJECT_RPG:
		pObject = new CAIActor(this);
		break;
	case AIOBJECT_GRENADE:
		pObject = new CAIActor(this);
		break;
	case AIOBJECT_WAYPOINT:
	case AIOBJECT_HIDEPOINT:
	case AIOBJECT_SNDSUPRESSOR:
		pObject = new CAIObject(this);
		break;
	default:
		// try to create an object of user defined type
		pObject = new CAIObject(this);
		break;
	}

	if ( type != AIOBJECT_PUPPET && type!=AIOBJECT_PLAYER) //&& type != AIOBJECT_VEHICLE )
		pObject->m_bUpdatedOnce = true;

	pObject->SetType(type);
	//FP_Vehicle pObject->SetAISystem(this);
	pObject->SetAssociation((CAIObject*)pAssociation);
	// insert object into map under key type
	// this is a multimap
	m_Objects.insert(AIObjects::iterator::value_type(type,pObject));
	unsigned int nNumPuppets = m_Objects.count(AIOBJECT_PUPPET);
	nNumPuppets += m_Objects.count(AIOBJECT_VEHICLE);

	m_nNumPuppets = nNumPuppets;


#if !defined(USER_dejan) && !defined(USER_mikko)
  AILogComment("CAISystem::CreateAIObject %p of type %d", pObject, type);
#endif

	return pObject;
}

//
//-----------------------------------------------------------------------------------------------------------
IGoalPipe *CAISystem::CreateGoalPipe(const char *pName)
{
	return m_PipeManager.CreateGoalPipe( pName );
}

//
//-----------------------------------------------------------------------------------------------------------
IGoalPipe *CAISystem::OpenGoalPipe(const char *pName)
{
	return m_PipeManager.OpenGoalPipe( pName );
}


//
//-----------------------------------------------------------------------------------------------------------
CGoalPipe *CAISystem::IsGoalPipe(const string &name)
{
	return m_PipeManager.IsGoalPipe(name);
}

//
//-----------------------------------------------------------------------------------------------------------

//====================================================================
// CheckUnusedGoalpipes
//====================================================================
void CAISystem::CheckGoalpipes(IConsoleCmdArgs *)
{
	GetAISystem()->m_PipeManager.CheckGoalpipes();
}

//
//-----------------------------------------------------------------------------------------------------------
CAIObject * CAISystem::GetPlayer() const
{
	AIObjects::const_iterator ai;
	if ((ai = m_Objects.find(AIOBJECT_PLAYER)) != m_Objects.end())
		return ai->second;
	return 0;
}

//
//-----------------------------------------------------------------------------------------------------------
IAIObject * CAISystem::GetAIObjectByName(unsigned short type, const char *pName) const
{
	AIObjects::const_iterator ai;

	if (m_Objects.empty()) return 0;

	if (!type) 
		return (IAIObject *) GetAIObjectByName(pName);

  if ((ai = m_Objects.find(type)) != m_Objects.end())
  {
    for (;ai!=m_Objects.end();)
    {
      if (ai->first != type)
        break;
      CAIObject *pObject = ai->second;

      if (strcmp(pName, pObject->GetName()) == 0)
        return (IAIObject *) pObject;
      ++ai;
    }
  }
	return 0;

}


//====================================================================
// PathFind
//====================================================================
void CAISystem::QueuePathFindRequest(const PathFindRequest &request)
{
	// all previous paths for this requester should be cancelled
	if (request.type == PathFindRequest::TYPE_ACTOR)
		CancelAnyPathsFor(request.pRequester->CastToCPipeUser());

	PathFindRequest *pNewRequest = new PathFindRequest(request);
	pNewRequest->bSuccess = false;
	m_lstPathQueue.push_back(pNewRequest);
}

//====================================================================
// CheckForAndHandleShortPath
// Danny todo - handle start/end dir
//====================================================================
bool CAISystem::CheckForAndHandleShortPath(const PathFindRequest &request)
{
	AIAssert(request.endIndex && request.startIndex);
	if (request.startIndex == request.endIndex)
	{
	  // backup the original since it might be being used (e.g. between beautifying and reporting found)
	  CNavPath origNavPath = m_navPath;

		GraphNode* pStart = m_pGraph->GetNodeManager().GetNode(request.startIndex);
		CNavRegion *pRegion = GetNavRegion(pStart->navType, m_pGraph);
		std::vector<PathPointDescriptor> points;
		if (pRegion->GetSingleNodePath(pStart, request.startPos, request.endPos, request.pRequester->m_Parameters.m_fPassRadius, m_navigationBlockers, points) &&
      !points.empty())
    {
      if (m_cvDebugPathFinding->GetIVal())
        AILogAlways("CAISystem::CheckForAndHandleShortPath %s Using single node path", request.pRequester->GetName());

      m_navPath.Clear("CAISystem::CheckForAndHandleShortPath");
      int nPoints = (int) points.size();
      for (int i = 0 ; i < nPoints - 1 ; ++i)
        m_navPath.PushBack(points[i]);
			if (m_navPath.GetPath().size() < 2)
				m_navPath.PushBack(points[nPoints-1], true);
			else
				m_navPath.PushBack(points[nPoints-1], false);

		  SAIEVENT event;
		  event.bPathFound = true;
      event.vPosition = m_navPath.GetLastPathPos(ZERO);
		  m_navPath.SetEndDir( request.endDir );
      m_navPath.SetParams(SNavPathParams(request.startPos, request.endPos, request.startDir, request.endDir,
				request.nForceTargetBuildingId, request.allowDangerousDestination, request.endDistance, false, request.isDirectional));
		  request.pRequester->Event(AIEVENT_ONPATHDECISION,&event);
		  m_navPath = origNavPath;
		  return true;

    }
    else
    {
      if (m_cvDebugPathFinding->GetIVal())
        AILogAlways("CAISystem::CheckForAndHandleShortPath %s Single node path but it is unpassable", request.pRequester->GetName());
		  SAIEVENT event;
		  event.bPathFound = false;
		  request.pRequester->Event(AIEVENT_ONPATHDECISION,&event);
		  return true;
    }
	}
	return false;
}



//====================================================================
// TidyUpEndOfPath
// Shortens the "end" of the path as much as possible by checking to see
// if it's possible to reach (checking passability) the path end "early". If
// so subsequent path points are removed - though the pathEnd is added 
// to the end.
//====================================================================
static void TidyUpEndOfPath(TPathPoints& path, float radius, const Vec3 pathEnd, 
                            float criticalDistForRemoval,
                            IAISystem::tNavCapMask allowedTypeMask, const NavigationBlockers& navBlockers,
                            const CAIObject *pObject)
{
  float criticalDistZ = criticalDistForRemoval;
  if (path.size() <= 1)
    return;
  const PathPointDescriptor end = path.back();
  if (!(end.navType & allowedTypeMask))
    return;

  // walk back from end to find find the start of the last sequence of potential cutting points
  TPathPoints::iterator it = path.end();
  for (--it; it != path.begin(); --it)
  {
    if (!(it->navType & allowedTypeMask))
    {
      ++it;
      break;
    }
  }

  for ( ; it != path.end() ; ++it)
  {
    TPathPoints::iterator itNext = it; ++itNext;
    if (itNext == path.end())
      return;

    const Vec3& pt = it->vPos;
    const Vec3& ptNext = itNext->vPos;
    float fT = 0.0f;
    float dist = Distance::Point_Lineseg2D(pathEnd, Lineseg(pt, ptNext), fT);
    if (dist < criticalDistForRemoval)
    {
      bool ok = true;
      if (it->navType == IAISystem::NAV_TRIANGULAR)
      {
        Lineseg seg(pathEnd, it->vPos);
        Vec3 closest;
        if (GetAISystem()->IntersectsForbidden(pathEnd, pt, closest))
          ok = false;
      }
      if (ok)
      {
        bool threeD = (it->navType & (IAISystem::NAV_FLIGHT | IAISystem::NAV_WAYPOINT_3DSURFACE | IAISystem::NAV_VOLUME)) != 0;
        Vec3 segPos = pt + fT * (ptNext - pt);
        if ( threeD || fabsf(segPos.z - pathEnd.z) < criticalDistZ)
        {
          // TODO Danny commented this test out - it causes problems for vehicles where when the frame rate is low
          // the distance can be very large, so the walkability test fails even though it's OK. However, we need to check
          // walkability to avoid dynamic obstacles.
  //        if (!GetAISystem()->GetNavRegion(it->navType, GetAISystem()->GetGraph())->CheckPassability(pt, end.vPos, radius, navBlockers))
  //          return;
          if (it->navType == IAISystem::NAV_TRIANGULAR && pObject->GetType() == AIOBJECT_PUPPET)
          {
            if (!GetAISystem()->GetNavRegion(it->navType, GetAISystem()->GetGraph())->CheckPassability(pt, end.vPos, radius, navBlockers))
              return;
          }

          // easier to erase right up to the end and then add the end back again
          path.erase(itNext, path.end());
          path.push_back(end);
          path.back().vPos = pathEnd;
          return;
        }
      }
    }
  }
}

//====================================================================
// GetNavigationBlocker
//====================================================================
void CAISystem::GetNavigationBlocker(NavigationBlockers& navigationBlockers, 
                                     const Vec3 &requesterPos, const Vec3 &requesterVel,
                                     Vec3 blockerPos, const Vec3& blockerVel, CAIActor* pAIActor)
{
  int nBuildingID = -1;
  IVisArea* pArea = 0;

  // Don't consider triangular - generally there's too many, and the path beautification will 
  // make it irrelevant anyway.
  ENavigationType navType = CheckNavigationType(
    blockerPos, nBuildingID, pArea, pAIActor ? pAIActor->GetMovementAbility().pathfindingProperties.navCapMask : 
  /*IAISystem::NAV_TRIANGULAR | */ IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_VOLUME);

  // if dist to blocker < criticalDist then the blocker position is predicted. Otherwise the blocker
  // is only considered if he's pretty well stationary (vel < criticalVel)
  static float criticalDistSq = square(20.0f);
  static float criticalSpeedSq = square(0.5f);
  static float predictionTime = 2.0f;

  float blockerSpeedSq = blockerVel.GetLengthSquared();
  float requesterSpeedSq = requesterVel.GetLengthSquared();

  /// This represents the extra distance that a path follower would be prepared to divert
  /// in order to avoid the blocker
  static float costMod2D = 10.0f;
  static float costMod3D = 20.0f;

  switch (navType)
  {
  case IAISystem::NAV_TRIANGULAR:
    {
      bool gotFloor = GetFloorPos(blockerPos, blockerPos, 0.05f, 2.0f, 0.1f, AICE_ALL);
      if (!gotFloor)
        return;

      float distSq = Distance::Point_PointSq(requesterPos, blockerPos);
      if (distSq > criticalDistSq && blockerSpeedSq > criticalSpeedSq)
        return;
      else if (distSq < criticalDistSq)
        blockerPos += predictionTime * blockerVel;

      float radius = 1.0f;

      navigationBlockers.push_back(SNavigationBlocker(blockerPos, radius, costMod2D, 0.0f, false, false));
      navigationBlockers.back().navType = navType;
      navigationBlockers.back().restrictedLocation = true;
    }
    break;
  case IAISystem::NAV_WAYPOINT_HUMAN:
    {
      bool gotFloor = GetFloorPos(blockerPos, blockerPos, 0.05f, 2.0f, 0.1f, AICE_ALL);
      if (!gotFloor)
        return;

      float distSq = Distance::Point_PointSq(requesterPos, blockerPos);
      if (distSq > criticalDistSq && blockerSpeedSq > criticalSpeedSq)
        return;
      else if (distSq < criticalDistSq)
        blockerPos += predictionTime * blockerVel;

      float radius = 1.0f;

      navigationBlockers.push_back(SNavigationBlocker(blockerPos, radius, costMod2D, 0.0f, false, false));
      navigationBlockers.back().navType = navType;
      navigationBlockers.back().location.waypoint.nBuildingID = nBuildingID;
      navigationBlockers.back().restrictedLocation = true;
    }
    break;
  case IAISystem::NAV_WAYPOINT_3DSURFACE:
    {
      float distSq = Distance::Point_PointSq(requesterPos, blockerPos);
      if (distSq > criticalDistSq && blockerSpeedSq > criticalSpeedSq)
        return;
      else if (distSq < criticalDistSq)
        blockerPos += predictionTime * blockerVel;

      float radius = 1.0f;

      navigationBlockers.push_back(SNavigationBlocker(blockerPos, radius, costMod2D, 0.0f, false, false));
      navigationBlockers.back().navType = navType;
      navigationBlockers.back().location.waypoint.nBuildingID = nBuildingID;
      navigationBlockers.back().restrictedLocation = true;
    }
    break;
  case IAISystem::NAV_FLIGHT:
    {
    }
    break;
  case IAISystem::NAV_VOLUME:
    {
      if (!pAIActor)
      {
        float distSq = Distance::Point_PointSq(requesterPos, blockerPos);
        if (distSq > criticalDistSq && blockerSpeedSq > criticalSpeedSq)
          return;
        else if (distSq < criticalDistSq)
          blockerPos += predictionTime * blockerVel;

        float radius = 2.5f;
        navigationBlockers.push_back(SNavigationBlocker(blockerPos, radius, costMod3D, 0.0f, false, false));
        navigationBlockers.back().navType = navType;
        navigationBlockers.back().restrictedLocation = true;
      }
    }
    break;
  case IAISystem::NAV_ROAD:
    {
      // don't steer off a road...
    }
    break;
  default:
    break;
  }
}


//====================================================================
// GetNavigationBlockers
//====================================================================
void CAISystem::GetNavigationBlockers(NavigationBlockers& navigationBlockers, CAIActor *pRequester, const PathFindRequest *pfr)
{
  const Vec3 requesterPos = pRequester->GetPhysicsPos();
  const Vec3 requesterVel = pRequester->GetVelocity();

  navigationBlockers.resize(0);
  if (m_cvCrowdControlInPathfind->GetIVal() == 0)
    return;

  // Danny: this is/looks expensive and could result in a lot of blockers (also expensive in the pathfinder)
  // So - maybe it is an OK compromise that triangular is disabled from GetNavigationBlocker.
  static bool doEveryone = false;
  if (doEveryone && m_pCurrentRequest)
  {
    const AIObjects::iterator aiEnd = GetAISystem()->m_Objects.end();
    for (AIObjects::iterator ai = GetAISystem()->m_Objects.find(AIOBJECT_PUPPET); 
      ai != aiEnd && ai->first == AIOBJECT_PUPPET ; ++ai)
    {
      CAIActor* pAI = (CAIActor*) ai->second;
      if (pAI != m_pCurrentRequest->pRequester)
  // FIXME - GetNavigationBlocker should accept CAIObjects, not CAIActors?
        GetNavigationBlocker(navigationBlockers, requesterPos, requesterVel, pAI->GetPhysicsPos(), pAI->GetVelocity(), pAI);
    }
  }

  if (GetPlayer())
    GetNavigationBlocker(navigationBlockers, requesterPos, requesterVel, GetPlayer()->GetPhysicsPos(), GetPlayer()->GetVelocity(), 0);

  pRequester->AddNavigationBlockers(navigationBlockers, pfr);
#ifdef _DEBUG
	IEntity* ent = m_pSystem->GetIEntitySystem()->FindEntityByName("TestNavBlocker");
	if (ent)
	{
    if (pfr)
    {
      static float radius = 50.0f;
      static float cost = 1000.0f;
      static bool radialDecay = true;
      static bool directional = true;
      static float extra = 1.5f;
      float d1 = extra * Distance::Point_Point(ent->GetPos(), pfr->startPos);
      float d2 = extra * Distance::Point_Point(ent->GetPos(), pfr->endPos);
      float r = min(min(d1, d2), radius);
      navigationBlockers.push_back(SNavigationBlocker(ent->GetPos(), r, 0.0f, cost, radialDecay, directional));
    }
    else
    {
      static float radius = 70.0f;
      static float cost = 20.0f;
      static bool radialDecay = true;
      static bool directional = true;
      navigationBlockers.push_back(SNavigationBlocker(ent->GetPos(), radius, 0.0f, cost, radialDecay, directional));
    }
  }
#endif
}

//===================================================================
// AttemptPathInDir
//===================================================================
float CAISystem::AttemptPathInDir(TPathPoints &path, const Vec3 &curPos, const Vec3 &dir, const PathFindRequest &request, 
                                  const CHeuristic* pHeuristic, float maxCost, const NavigationBlockers& navigationBlockers)
{
  FUNCTION_PROFILER( m_pSystem,PROFILE_AI );
  // potential path seg Dot with dir must be > criticalDot to continue;
  static float criticalDot = 0.7f;

  float inputDirLenSq = dir.GetLengthSquared();
  AIAssert(inputDirLenSq > 0.8f && inputDirLenSq < 1.2f);

	unsigned currentNodeIndex = request.startIndex;
	if (!currentNodeIndex)
		return -1.0f;

  if (!pHeuristic)
    return -1.0f;

  const CPathfinderRequesterProperties* reqProperties = pHeuristic->GetRequesterProperties();
  if (!reqProperties)
    return -1.0f;

	IAISystem::ENavigationType navType = m_pGraph->GetNodeManager().GetNode(request.startIndex)->navType;
  if (navType != IAISystem::NAV_TRIANGULAR)
    return -1.0f;

  float extraR = GetAISystem()->m_cvExtraRadiusDuringBeautification->GetFVal();
  float extraFR = GetAISystem()->m_cvExtraForbiddenRadiusDuringBeautification->GetFVal();

  float pathCost = 0.0f;

  float heuristicZScale = 1.0f;
//  if (pHeuristic->GetRequesterProperties())
//    heuristicZScale *= pHeuristic->GetRequesterProperties()->properties.zScale;
  Vec3 heuristicZScaleVec(1, 1, heuristicZScale);

  SMarkClearer markClearer(m_pGraph);

  path.clear();
  path.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, request.startPos));
  while (pathCost < maxCost)
  {
		const GraphNode* currentNode = m_pGraph->GetNodeManager().GetNode(currentNodeIndex);

		m_pGraph->MarkNode(currentNodeIndex);
		const Vec3 &currentPos = path.back().vPos;

		float bestDot = -std::numeric_limits<float>::max();
		Vec3 bestEdgePos(ZERO);
		unsigned bestNextNodeIndex = 0;
		Vec3 bestPathSegment(ZERO);
		float bestHeuristicCost = 0.0f;
		float bestHeuristicDist = 0.0f;

		for (unsigned linkId = currentNode->firstLinkIndex ; linkId ; linkId = m_pGraph->GetLinkManager().GetNextLink(linkId))
		{
			unsigned nextNodeIndex = m_pGraph->GetLinkManager().GetNextNode(linkId);
			const GraphNode *nextNode = m_pGraph->GetNodeManager().GetNode(nextNodeIndex);

			if (nextNode->mark)
				continue;
			if (nextNode->navType != IAISystem::NAV_TRIANGULAR)
				continue;
			float heuristicCost = pHeuristic->CalculateCost(m_pGraph, *currentNode, linkId, *nextNode, navigationBlockers);
			if (heuristicCost < 0.0f)
				continue;
			float heuristicDist = (nextNode->GetPos() - currentNode->GetPos()).CompMul(heuristicZScaleVec).GetLength();
			if (heuristicDist < 0.01f)
				continue;

			// Now find the point on the edge containing the link that intersects the straight path
			// in 2D.
			unsigned vertexStartIndex = m_pGraph->GetLinkManager().GetStartIndex(linkId);
			unsigned vertexEndIndex = m_pGraph->GetLinkManager().GetEndIndex(linkId);
			if (vertexStartIndex == vertexEndIndex)
				continue;
			const ObstacleData &startObst = GetAISystem()->m_VertexList.GetVertex(currentNode->GetTriangularNavData()->vertices[vertexStartIndex]);
			const ObstacleData &endObst = GetAISystem()->m_VertexList.GetVertex(currentNode->GetTriangularNavData()->vertices[vertexEndIndex]);
			Vec3 edgeStart = startObst.vPos;
			Vec3 edgeEnd = endObst.vPos;
			Vec3 edgeDir = (edgeEnd - edgeStart).GetNormalizedSafe(Vec3(1, 0, 0));
			Lineseg edgeSegment(edgeStart, edgeEnd);
			Lineseg segmentToEnd(currentPos, currentPos + dir * 100.0f);
			float tA, tB;
			// ignore the return value - use as a line-line test
			Intersect::Lineseg_Lineseg2D(segmentToEnd, edgeSegment, tA, tB);
			{
				Vec3 edgePt = edgeSegment.start + tB * (edgeSegment.end - edgeSegment.start);
				// constrain this edge point to be in a valid region
				float workingRadius = reqProperties->radius + extraR;
				if (!startObst.bCollidable && !endObst.bCollidable)
				{
					workingRadius = 0.0f;
				}
				else if (IsPointOnForbiddenEdge(edgeStart, 0.0001f, 0, 0, false) || 
					IsPointOnForbiddenEdge(edgeEnd, 0.0001f, 0, 0, false))
				{
					workingRadius += extraFR;
				}
				if (workingRadius > m_pGraph->GetLinkManager().GetRadius(linkId))
					workingRadius = m_pGraph->GetLinkManager().GetRadius(linkId);
				Vec3 edgeStartOK = m_pGraph->GetLinkManager().GetEdgeCenter(linkId) - edgeDir * (m_pGraph->GetLinkManager().GetRadius(linkId) - workingRadius);
				Vec3 edgeEndOK = m_pGraph->GetLinkManager().GetEdgeCenter(linkId) + edgeDir * (m_pGraph->GetLinkManager().GetRadius(linkId) - workingRadius);

				if ( ( (edgePt - edgeStartOK) | edgeDir ) < 0.0f)
					edgePt = edgeStartOK;
				else if ( ( (edgePt - edgeEndOK) | edgeDir ) > 0.0f)
					edgePt = edgeEndOK;

				// Estimate the cost of getting there
				Vec3 pathSegment = edgePt - currentPos;
				Vec3 pathSegmentDir = pathSegment.GetNormalizedSafe();

				float dirDot = pathSegmentDir.Dot(dir);

				if (dirDot <= bestDot)
					continue;
				if (dirDot < criticalDot)
					continue;

				bestDot = dirDot;
				bestEdgePos = edgePt;
				bestNextNodeIndex = nextNodeIndex;
				bestPathSegment = pathSegment;
				bestHeuristicCost = heuristicCost;
				bestHeuristicDist = heuristicDist;
			}
		}
		if (bestDot < criticalDot)
		{
			// no exit from this triangle - just go as far as we can.
			static std::vector<Vec3> poly(3);
			const STriangularNavData *pTriNavData = currentNode->GetTriangularNavData();
			const ObstacleIndexVector::const_iterator oiEnd = pTriNavData->vertices.end();
			unsigned iVert = 0;
			for (ObstacleIndexVector::const_iterator oi = pTriNavData->vertices.begin(); oi != oiEnd ; ++oi)
				poly[iVert++] = m_VertexList.GetVertex(*oi).vPos;
			Vec3 startPos = currentPos;
			float segLen = maxCost - pathCost;
			segLen += extraR + extraFR; // gets subtracted afterwards

			// Assumes that the startPos is valid and can be outside the triangle.
			// Find the furthest point on the target triangle and clamp the movement there.
			// Since the lineseg poly intersection always returns the nearest hit,
			// check in both directions.

			Lineseg lineseg0(startPos, startPos + dir * segLen);
			Lineseg lineseg1(lineseg0.end, lineseg0.start);

			Vec3 intPt, tmp;
			float distSq = -1.0f;

			if (Intersect::Lineseg_Polygon2D(lineseg0, poly, tmp))
			{
				distSq = Distance::Point_PointSq(startPos, tmp);
				intPt = tmp;
			}
			if (Intersect::Lineseg_Polygon2D(lineseg1, poly, tmp))
			{
				float d = Distance::Point_PointSq(startPos, tmp);
				if (d > distSq)
				{
					distSq = d;
					intPt = tmp;
				}
			}
			if (distSq > sqr(extraR + extraFR))
			{
				intPt -= (extraR + extraFR) * dir;
				pathCost += (intPt - currentPos).GetLength();
				path.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, intPt));
			}

			return pathCost;
		}

		float pathSegmentDist = bestPathSegment.CompMul(heuristicZScaleVec).GetLength();
		float pathSegmentCost = bestHeuristicCost * (pathSegmentDist / bestHeuristicDist);

		float newCost = pathCost + pathSegmentCost;
		if (newCost < maxCost)
		{
			path.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, bestEdgePos));
			pathCost = newCost;
		}
		else 
		{
			float frac = (maxCost - pathCost) / (newCost - pathCost);
			Vec3 finalPos = frac * bestEdgePos + (1.0f - frac) * currentPos;
			path.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, finalPos));
			return maxCost;
		}
		currentNodeIndex = bestNextNodeIndex;
	}

	path.push_back(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, request.startPos + dir * maxCost));

	return maxCost;
}

//====================================================================
// AttemptStraightPath
//====================================================================
float CAISystem::AttemptStraightPath(TPathPoints& straightPath, 
                                     const PathFindRequest &request,
                                     const Vec3 &curPos,
                                     const CHeuristic* pHeuristic,
                                     float maxCost,
                                     const NavigationBlockers& navigationBlockers, 
                                     bool returnPartialPath)
{
  FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	GraphNode* pStart = m_pGraph->GetNodeManager().GetNode(request.startIndex);
	GraphNode* pEnd = m_pGraph->GetNodeManager().GetNode(request.endIndex);
	IAISystem::ENavigationType navType = pStart->navType;

	if (navType != pEnd->navType)
		return -1.0f;

	float cost = -1.0f;
	CNavRegion *pRegion = GetNavRegion(navType, m_pGraph);
	if (pRegion)
		cost = pRegion->AttemptStraightPath(straightPath, pHeuristic, request.startPos, request.endPos, 
		request.startIndex, maxCost, navigationBlockers, returnPartialPath);

	if (straightPath.size() < 2 || cost < 0.0f)
		return -1.0f;

	Vec3 deltaToEnd = request.endPos - curPos;

	// tidy up the start of the path here
	bool cutOne = false;
	while (straightPath.size() > 1)
	{
		const PathPointDescriptor& ppd = straightPath.front();
		const Vec3& pt = ppd.vPos;
		if ( ( (curPos - pt) | deltaToEnd) > 0.0f)
		{
			straightPath.erase(straightPath.begin());
			cutOne = true;
		}
		else
		{
			break;
		}
	}
	if (cutOne)
		straightPath.push_front(PathPointDescriptor(navType, curPos));

	return cost;
}


//====================================================================
// BeautifyPath
//====================================================================
void CAISystem::BeautifyPath(const VectorConstNodeIndices & pathNodes)
{
  FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	if (!m_pCurrentRequest->pRequester)
		return;

	Vec3  startPos = m_pCurrentRequest->pRequester->GetPhysicsPos();
	CPipeUser *pPiper = (CPipeUser *) m_pCurrentRequest->pRequester;
	if (!pPiper->m_forcePathGenerationFrom.IsZero())
		startPos = pPiper->m_forcePathGenerationFrom;
	const Vec3& startDir = m_pCurrentRequest->startDir;
	const Vec3& endPos = m_pCurrentRequest->endPos;
	const Vec3& endDir = m_pCurrentRequest->endDir;

  bool bBeautify = m_cvBeautifyPath->GetIVal() != 0;

  m_navPath.Clear("CAISystem::BeautifyPath");
  if (pathNodes.empty())
    return;

	VectorConstNodeIndices::const_iterator nodeBeginIt = pathNodes.begin();
	VectorConstNodeIndices::const_iterator nodeEndIt = nodeBeginIt;

	TPathPoints totalPath;
	totalPath.push_back(PathPointDescriptor(m_pGraph->GetNodeManager().GetNode(pathNodes.front())->navType, startPos));

	while (nodeEndIt != pathNodes.end())
	{
		const GraphNode* nodeBegin = m_pGraph->GetNodeManager().GetNode(*nodeBeginIt);
		const GraphNode* nodeEnd = m_pGraph->GetNodeManager().GetNode(*nodeEndIt);

		VectorConstNodeIndices::const_iterator nodeNextIt = nodeEndIt;
		++nodeNextIt;

		bool beautify = false;
		if (nodeNextIt == pathNodes.end())
		{
			// last one
			beautify = true;
		}
		else
		{
			const GraphNode* nodeNext = m_pGraph->GetNodeManager().GetNode(*nodeNextIt);

			if (nodeNext->navType != nodeBegin->navType)
				beautify = true;
		}

		if (beautify)
		{
			TPathPoints segmentPath;
			VectorConstNodeIndices inPath(nodeBeginIt, nodeNextIt);

			Vec3 thisStartPos = nodeBegin->GetPos();
			if (!totalPath.empty())
				thisStartPos = totalPath.back().vPos;

			Vec3 thisEndPos = nodeEnd->GetPos();
			Vec3 thisStartDir(ZERO);
			Vec3 thisEndDir(ZERO);
			if (nodeBeginIt == pathNodes.begin())
			{
				thisStartPos = startPos;
				thisStartDir = startDir;
			}
			if (nodeNextIt == pathNodes.end())
			{
				thisEndPos = endPos;
				thisEndDir = endDir;
			}
			else if (m_pGraph->GetNodeManager().GetNode(*nodeNextIt)->navType == IAISystem::NAV_SMARTOBJECT)
			{
				thisEndPos = m_pGraph->GetNodeManager().GetNode(*nodeNextIt)->GetPos();
			}
			else if (nodeBegin->navType == IAISystem::NAV_TRIANGULAR && 
				( m_pGraph->GetNodeManager().GetNode(*nodeNextIt)->navType & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE | IAISystem::NAV_ROAD) ) )
			{
				thisEndPos = m_pGraph->GetNodeManager().GetNode(*nodeNextIt)->GetPos();
			}

			CNavRegion* region = GetNavRegion(nodeBegin->navType, m_pGraph);
			if (nodeBegin->navType == IAISystem::NAV_UNSET)
				region = m_pWaypointHumanNavRegion; 

			if (region)
			{
				if (bBeautify)
				{
					region->BeautifyPath(inPath, segmentPath, 
						thisStartPos, thisStartDir, thisEndPos, thisEndDir, 
						m_pCurrentRequest->pRequester->m_Parameters.m_fPassRadius, 
						m_pCurrentRequest->pRequester->m_movementAbility,
						m_navigationBlockers);
				}
				else
				{
					region->UglifyPath(inPath, segmentPath, 
						thisStartPos, thisStartDir, thisEndPos, thisEndDir);
				}
			}

			// splice the result in
			for (TPathPoints::iterator it = segmentPath.begin() ; it != segmentPath.end() ; ++it)
				totalPath.push_back(*it);

			nodeBeginIt = nodeEndIt;
			++nodeBeginIt;
		}
		++nodeEndIt;
	}

	totalPath.push_back(PathPointDescriptor(m_pGraph->GetNodeManager().GetNode(pathNodes.back())->navType, endPos));
	if ( totalPath.back().navType == IAISystem::NAV_SMARTOBJECT )
		totalPath.back().navType = IAISystem::NAV_UNSET;

	// For now disable this (Danny) so that the end direction is simply stored in the path
	// and has no effect on it.
	/*
	// Adjust for start/end directions
	if (!totalPath.empty())
	{
	if (startDir.GetLengthSquared() > 0.01f)
	{
	PathPointDescriptor newFirst = totalPath.front();
	newFirst.vPos -= startDir;
	totalPath.push_front(newFirst);
	}
	if (endDir.GetLengthSquared() > 0.01f)
	{
	PathPointDescriptor newLast = totalPath.back();
	newLast.vPos += endDir;
	totalPath.push_back(newLast);
	}
	}
	*/

	static bool doEnds = true;
	if (bBeautify && doEnds)
	{
		// avoid backtracking at the start of the path by looking for the last path segment that
		// passes close to the start position. Also do this for the end by temporarily reversing 
		// the path (actually, do end first since it's easier to traverse)
		if (totalPath.size() > 1)
		{
			IAISystem::ENavigationType endType = totalPath.back().navType;
			IAISystem::tNavCapMask allowedTypeMask = IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD;

			if (!m_pCurrentRequest->pRequester->m_movementAbility.b3DMove)
				allowedTypeMask |= IAISystem::NAV_WAYPOINT_3DSURFACE;

			float criticalDistForRemoval = m_pCurrentRequest->pRequester->m_movementAbility.pathRadius;

			if (endType & allowedTypeMask)
			{
				TidyUpEndOfPath(totalPath, m_pCurrentRequest->pRequester->m_Parameters.m_fPassRadius, 
					totalPath.back().vPos, criticalDistForRemoval, allowedTypeMask, m_navigationBlockers, m_pCurrentRequest->pRequester);
			}

			if (totalPath.size() > 1)
			{
				std::reverse(totalPath.begin(), totalPath.end());
				IAISystem::ENavigationType startType = totalPath.back().navType;
				if (startType & allowedTypeMask)
				{
					TidyUpEndOfPath(totalPath, m_pCurrentRequest->pRequester->m_Parameters.m_fPassRadius, 
						startPos, criticalDistForRemoval, allowedTypeMask, m_navigationBlockers, m_pCurrentRequest->pRequester);
				}
				std::reverse(totalPath.begin(), totalPath.end());
			}
		}
	}

	// copy the result, and eliminate duplicates
	PathPointDescriptor last(IAISystem::NAV_UNSET, Vec3(-9999, 9999, -99999));
	for (TPathPoints::iterator it = totalPath.begin() ; it != totalPath.end() ; ++it)
	{
		const PathPointDescriptor &ppd = *it;
		if (!ppd.IsEquivalent(last))
			m_navPath.PushBack(ppd);
		last = *it;
	}
	if (IsAIInDevMode())
	{
		ValidateSONavData( m_navPath );
	}
}

bool CAISystem::ValidateSONavData( const CNavPath& navPath )
{
	const TPathPoints& path = navPath.GetPath();
	if ( !path.size() )
		return true;

	if ( path.front().navType == IAISystem::NAV_SMARTOBJECT )
	{
		AIAssert( !"The nav. type of the first path point is NAV_SMARTOBJECT!" );
		return false;
	}

	int counter = 0;
	TPathPoints::const_iterator it, itEnd = path.end();
	for ( it = path.begin(); it != itEnd; ++it )
	{
		const PathPointDescriptor& point = *it;
		if ( point.navType == IAISystem::NAV_SMARTOBJECT )
		{
			switch ( ++counter )
			{
			case 1:
				if ( !point.pSONavData )
				{
					if ( &*it != &path.back() )
					{
						AIAssert( !"Path point of nav. type NAV_SMARTOBJECT in middle of path has no pSONavData!" );
						return false;
					}
				}
				break;
			case 2:
				if ( point.pSONavData )
				{
					AIAssert( !"The second path point of nav. type NAV_SMARTOBJECT has pSONavData!" );
					return false;
				}
				counter = 0;
				break;
			}
		}
		else if ( counter )
		{
			if ( counter == 1 )
			{
				AIAssert( !"Single path point of nav. type NAV_SMARTOBJECT in middle of path!" );
				return false;
			}
			counter = 0;
		}
	}
	return true;
}

//====================================================================
// RequestPathInDirection
//====================================================================
void CAISystem::RequestPathInDirection(const Vec3 &start, const Vec3 &pos, float maxDist,
                               CPipeUser *pRequester, float endDistance)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  if (m_cvDebugPathFinding->GetIVal())
    AILogAlways("CAISystem::RequestPathInDirection %s from (%5.2f, %5.2f, %5.2f) towards pos (%5.2f, %5.2f, %5.2f) up to dist %5.2f",
      pRequester->GetName(), start.x, start.y, start.z, pos.x, pos.y, pos.z, maxDist);

	PathFindRequest pfr(PathFindRequest::TYPE_ACTOR);
	pfr.startPos = start;
	pfr.startDir = ZERO;
	pfr.endPos = pos;
	pfr.endDir = ZERO;
	pfr.pRequester = pRequester;
	pfr.startIndex = pfr.endIndex = 0;
	pfr.allowDangerousDestination = false;
	pfr.nForceTargetBuildingId = false;
	pfr.bPathEndIsAsRequested = true;
	pfr.isDirectional = true;
	pfr.endDistance = endDistance;

	// Now get the graph nodes for start/end
	unsigned startNodeIndex = m_pGraph->GetEnclosing(pfr.startPos, pRequester->m_movementAbility.pathfindingProperties.navCapMask,
		pRequester->m_Parameters.m_fPassRadius, pRequester->m_lastNavNodeIndex, 5.0f, &pfr.startPos, true, pRequester->GetName());
	pfr.startIndex = startNodeIndex;

	if (pfr.startIndex && ExitNodeImpossible(m_pGraph->GetLinkManager(), m_pGraph->GetNodeManager().GetNode(pfr.startIndex),pRequester->m_Parameters.m_fPassRadius) )
	{
		Vec3 vStartPos = m_pGraph->GetNodeManager().GetNode(pfr.startIndex)->GetPos();
		AILogComment("CAISystem::RequestPathInDirection %s Start node (%5.2f, %5.2f, %5.2f) has no useable links", 
			pRequester->GetName(), vStartPos.x, vStartPos.y, vStartPos.z);
		m_navPath.Clear("CAISystem::RequestPathInDirection");
		SAIEVENT sai;
		sai.bPathFound = false;
		pRequester->Event(AIEVENT_ONPATHDECISION,&sai);
		return;
	}

	unsigned endNodeIndex = m_pGraph->GetEnclosing(pfr.endPos, pRequester->m_movementAbility.pathfindingProperties.navCapMask,
		pRequester->m_Parameters.m_fPassRadius, pRequester->m_lastNavNodeIndex, 5.0f, &pfr.endPos, false, pRequester->GetName());
	pfr.endIndex = endNodeIndex;
//	pfr.endIndex = 0;

	if (!pfr.startIndex)
	{
		AIWarning("CAISystem::RequestPathInDirection pts not available for %s: %s %s (from (%5.2f %5.2f %5.2f) to (%5.2f %5.2f %5.2f)", 
			pRequester->GetName(), 
			pfr.startIndex == 0 ? "start" : "", 
			pfr.endIndex == 0 ? "end" : "",
			pfr.startPos.x, pfr.startPos.y, pfr.startPos.z,
			pfr.endPos.x, pfr.endPos.y, pfr.endPos.z);
		SAIEVENT sai;
		sai.bPathFound = false;
		pRequester->Event(AIEVENT_ONPATHDECISION,&sai);
		return;
	}

	pRequester->m_lastNavNodeIndex = pfr.startIndex;

	if (!pfr.endIndex)
	{
		// if there's no end node then use a fake node - it has no inward connections so we'll 
		// never reach it but that doesn't matter
		m_pGraph->MoveNode(m_pGraph->m_safeFirstIndex, pfr.endPos);
		pfr.endIndex = m_pGraph->m_safeFirstIndex;
	}

	// Stop searching when far enough from the starting position (euclidean distance).
	pfr.extraConstraints.push_back(CStandardHeuristic::SExtraConstraint());
	pfr.extraConstraints.back().type = CStandardHeuristic::SExtraConstraint::ECT_MINDISTFROMPOINT;
	pfr.extraConstraints.back().constraint.minDistFromPoint.minDistSq = sqr(maxDist);
	pfr.extraConstraints.back().constraint.minDistFromPoint.px = pfr.startPos.x;
	pfr.extraConstraints.back().constraint.minDistFromPoint.py = pfr.startPos.y;
	pfr.extraConstraints.back().constraint.minDistFromPoint.pz = pfr.startPos.z;

	// Stop searching when far enough from the starting position (distance along the nodes).
//	pfr.extraConstraints.push_back(CStandardHeuristic::SExtraConstraint());
//	pfr.extraConstraints.back().type = CStandardHeuristic::SExtraConstraint::ECT_MAXCOST;
//	pfr.extraConstraints.back().constraint.maxCost.maxCost = maxDist * (1.0f + pRequester->m_movementAbility.pathfindingProperties.triangularResistanceFactor);

  QueuePathFindRequest(pfr);
}

//====================================================================
// RequestPathTo
//====================================================================
void CAISystem::RequestPathTo(const Vec3 &start, const Vec3 &end, const Vec3 &endDir, CPipeUser *pRequester, 
                          bool allowDangerousDestination, int forceTargetBuildingId, float endTol, float endDistance)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  if (m_cvDebugPathFinding->GetIVal())
    AILogAlways("CAISystem::RequestPathTo %s from (%5.2f, %5.2f, %5.2f) to (%5.2f, %5.2f, %5.2f)",
      pRequester->GetName(), start.x, start.y, start.z, end.x, end.y, end.z);

	// if this is some no pathfinding agent
	if(!pRequester->m_movementAbility.bUsePathfinder)
	{
		if (m_cvDebugPathFinding->GetIVal())
			AILogAlways("CAISystem::RequestPathTo %s not using path finder", pRequester->GetName());
		CNavPath origNavPath = m_navPath;
		SAIEVENT sai;
		sai.bPathFound = true;
		sai.vPosition = end;
		m_navPath.Clear("CAISystem::RequestPathTo");
		m_navPath.SetParams(SNavPathParams(start, end, ZERO, endDir, forceTargetBuildingId, allowDangerousDestination, endDistance));
		m_navPath.PushBack(PathPointDescriptor(IAISystem::NAV_UNSET, end));
		pRequester->Event(AIEVENT_ONPATHDECISION,&sai);
		m_navPath = origNavPath;
		return;
	}

	PathFindRequest pfr(PathFindRequest::TYPE_ACTOR);
	pfr.startPos = start;
	pfr.startDir = Vec3(0.0f, 0.0f, 0.0f);
	pfr.endPos = end;
	pfr.endDir = endDir;
	pfr.pRequester = pRequester;
	pfr.allowDangerousDestination = allowDangerousDestination;
	pfr.nForceTargetBuildingId = forceTargetBuildingId;
	pfr.bPathEndIsAsRequested = true;
	pfr.endTol = endTol;
	pfr.endDistance = endDistance;

  if (end.IsZero())
  {
	  AIWarning("CAISystem::RequestPathTo passed zero position for %s (%s) returning no path", 
      pRequester->GetName(), pRequester->GetActiveGoalPipeName());
    SAIEVENT sai;
    sai.bPathFound = false;
    pRequester->Event(AIEVENT_ONPATHDECISION,&sai);
    return;
  }

  if (pfr.pRequester->m_movementAbility.pathFindPrediction > 0.0f)
  {
    // Predict the point slightly in future and use that in pathfinding to avoid traversing back when tracing the path.
    int nBuildingID;
    IVisArea *pAreaID;
    IAISystem::ENavigationType navType = CheckNavigationType(pfr.startPos, nBuildingID, pAreaID, pRequester->m_movementAbility.pathfindingProperties.navCapMask & IAISystem::NAV_TRIANGULAR);
    if (navType == IAISystem::NAV_TRIANGULAR)
    {
      // todo Danny: clip against forbidden
    }

    EAICollisionEntities aice = pfr.pRequester->GetType() == AIOBJECT_VEHICLE ? AICE_STATIC : AICE_ALL;
    float radius = min(1.0f, pfr.pRequester->m_Parameters.m_fPassRadius);
    if (pfr.pRequester->GetType() == AIOBJECT_VEHICLE)
      pfr.startPos.z += radius + 0.2f; // bit hacky but the vehicle position tends to be on the bottom
    Vec3	vel = pRequester->GetVelocity();
    Vec3	newStartPos = pfr.startPos + vel * pfr.pRequester->m_movementAbility.pathFindPrediction;
    float	dist;
		bool	bSameStartPos((pfr.startPos-newStartPos).IsZero(.1f));
    if( (!bSameStartPos && !IntersectSweptSphere( 0, dist, Lineseg( pfr.startPos, newStartPos ), radius, aice )) ||
				(bSameStartPos && !OverlapSphere(pfr.startPos, radius, aice)) )
      pfr.startPos = newStartPos;
    pfr.startDir.Set(0, 0, 0);

  }

	// Now get the graph nodes for start/end
	// if a vehicle then avoid extra tests based on human walkability
	bool isVehicle = pRequester->GetType()==AIOBJECT_VEHICLE;
	unsigned startNodeIndex = m_pGraph->GetEnclosing(pfr.startPos, pRequester->m_movementAbility.pathfindingProperties.navCapMask,
		pRequester->m_Parameters.m_fPassRadius, pRequester->m_lastNavNodeIndex, 5.0f, isVehicle ? 0 : &pfr.startPos, true, pRequester->GetName());
	pfr.startIndex = startNodeIndex;
	if (forceTargetBuildingId >= 0)
	{
		pfr.bPathEndIsAsRequested = false;
		Vec3 newEndPos = pfr.endPos;
		unsigned endIndex = m_pWaypointHumanNavRegion->GetEnclosing(pfr.endPos, pRequester->m_Parameters.m_fPassRadius, 
			pRequester->m_lastNavNodeIndex, 5.0f, isVehicle ? 0 : &newEndPos, endTol > 0.0f, pRequester->GetName());
		pfr.endIndex = endIndex;
		const GraphNode* pEnd = m_pGraph->GetNodeManager().GetNode(pfr.endIndex);
		if (!pfr.endIndex || pEnd->navType != IAISystem::NAV_WAYPOINT_HUMAN || pEnd->GetWaypointNavData()->nBuildingID != forceTargetBuildingId)
		{
			// horrible hack because the formation points are at eye height
			pfr.endIndex = m_pWaypointHumanNavRegion->GetClosestNode(pfr.endPos - Vec3(0, 0, 1.3f), forceTargetBuildingId);
			if (pfr.endIndex)
				pfr.endPos = m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->GetPos();
		}
	}
	else
	{
		unsigned endNodeIndex = m_pGraph->GetEnclosing(pfr.endPos, pRequester->m_movementAbility.pathfindingProperties.navCapMask,
			pRequester->m_Parameters.m_fPassRadius, pRequester->m_lastNavNodeIndex, 5.0f, isVehicle ? 0 : &pfr.endPos, endTol > 0.0f, pRequester->GetName());
		pfr.endIndex = endNodeIndex;
	}

	// move the start out of forbidden if it's on the edge
	if (pfr.startIndex && m_pGraph->GetNodeManager().GetNode(pfr.startIndex)->navType == IAISystem::NAV_TRIANGULAR)
	{
		Vec3 origPos = pfr.startPos;
		float edgeTol = pRequester->GetParameters().m_fPassRadius;
		for (int nTries = 0 ; nTries < 2 ; ++nTries)
		{
			Vec3 newPos = GetPointOutsideForbidden(pfr.startPos, edgeTol);
			float distSq = Distance::Point_Point2DSq(newPos, pfr.startPos);
			if (distSq <= 0.0f)
				break;
			else
				pfr.startPos = newPos;
		}
		// maybe have to re-evaluate start node
		float distSq = Distance::Point_Point(pfr.startPos, origPos);
		if (distSq > 0.1f)
		{
			unsigned startNodeIndex = m_pGraph->GetEnclosing(pfr.startPos, pRequester->m_movementAbility.pathfindingProperties.navCapMask,
				pRequester->m_Parameters.m_fPassRadius, pRequester->m_lastNavNodeIndex, 5.0f, isVehicle ? 0 : &pfr.startPos, true, pRequester->GetName());
			pfr.startIndex = startNodeIndex;
		}
	}

	// move the end out of forbidden if it's on the edge
	if (pfr.endIndex && m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->navType == IAISystem::NAV_TRIANGULAR)
	{
		Vec3 origPos = pfr.endPos;
		float edgeTol = pRequester->GetParameters().m_fPassRadius;
		for (int nTries = 0 ; nTries < 2 ; ++nTries)
		{
			Vec3 newPos = GetPointOutsideForbidden(pfr.endPos, edgeTol);
			float distSq = Distance::Point_Point2DSq(newPos, pfr.endPos);
			if (distSq <= 0.0f)
				break;
			else
				pfr.endPos = newPos;
		}
		// maybe have to re-evaluate end node
		float distSq = Distance::Point_Point(pfr.endPos, origPos);
		if (distSq > 0.1f)
		{
			if (forceTargetBuildingId >= 0)
			{
				pfr.bPathEndIsAsRequested = false;
				Vec3 newEndPos = pfr.endPos;
				unsigned endIndex = m_pWaypointHumanNavRegion->GetEnclosing(pfr.endPos, pRequester->m_Parameters.m_fPassRadius, 
					pRequester->m_lastNavNodeIndex, 5.0f, isVehicle ? 0 : &newEndPos, endTol > 0.0f, pRequester->GetName());
				pfr.endIndex = endIndex;
				const GraphNode* pEnd = m_pGraph->GetNodeManager().GetNode(pfr.endIndex);
				if (!pfr.endIndex || pEnd->navType != IAISystem::NAV_WAYPOINT_HUMAN || pEnd->GetWaypointNavData()->nBuildingID != forceTargetBuildingId)
				{
					// horrible hack because the formation points are at eye height
					pfr.endIndex = m_pWaypointHumanNavRegion->GetClosestNode(pfr.endPos - Vec3(0, 0, 1.3f), forceTargetBuildingId);
					if (pfr.endIndex)
						pfr.endPos = m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->GetPos();
				}
			}
			else
			{
				unsigned endNodeIndex = m_pGraph->GetEnclosing(pfr.endPos, pRequester->m_movementAbility.pathfindingProperties.navCapMask,
					pRequester->m_Parameters.m_fPassRadius, pRequester->m_lastNavNodeIndex, 5.0f, isVehicle ? 0 : &pfr.endPos, endTol > 0.0f, pRequester->GetName());
				pfr.endIndex = endNodeIndex;
			}
		}
	}

	// never trace into a forbidden area - and in some cases move the destination outside of forbidden
	if (pfr.endIndex && m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->navType == IAISystem::NAV_TRIANGULAR)
	{
		float edgeTol = pRequester->GetParameters().m_fPassRadius;
		if (!allowDangerousDestination)
			edgeTol += m_cvExtraForbiddenRadiusDuringBeautification->GetFVal();

		bool rejectPath = false;
		Vec3 origPos = pfr.endPos;

		if (endTol > 0.0f)
		{
			for (int nTries = 0 ; nTries < 2 ; ++nTries)
			{
				Vec3 newPos = GetPointOutsideForbidden(pfr.endPos, edgeTol);
				float distSq = Distance::Point_PointSq(origPos, newPos);
				pfr.endPos = newPos;

				if (distSq < square(0.1f))
					break;

				if (distSq > square(endTol))
				{
					rejectPath = true;
					break;
				}
			}

			// maybe have to re-evaluate end node
			if (!rejectPath)
			{
				float distSq = Distance::Point_PointSq(pfr.endPos, origPos);
				if (distSq > 0.1f)
				{
					if (forceTargetBuildingId >= 0)
					{
						pfr.bPathEndIsAsRequested = false;
						Vec3 newEndPos = pfr.endPos;
						unsigned endIndex = m_pWaypointHumanNavRegion->GetEnclosing(pfr.endPos, pRequester->m_Parameters.m_fPassRadius, 
							pRequester->m_lastNavNodeIndex, 5.0f, isVehicle ? 0 : &newEndPos, false, pRequester->GetName());
						pfr.endIndex = endIndex;
						GraphNode* pEnd = m_pGraph->GetNodeManager().GetNode(pfr.endIndex);
						if (!pfr.endIndex || pEnd->navType != IAISystem::NAV_WAYPOINT_HUMAN || pEnd->GetWaypointNavData()->nBuildingID != forceTargetBuildingId)
						{
							// horrible hack because the formation points are at eye height
							pfr.endIndex = m_pWaypointHumanNavRegion->GetClosestNode(pfr.endPos - Vec3(0, 0, 1.3f), forceTargetBuildingId);
							if (pfr.endIndex)
								pfr.endPos = m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->GetPos();
						}
					}
					else
					{
						unsigned endNodeIndex = m_pGraph->GetEnclosing(pfr.endPos, pRequester->m_movementAbility.pathfindingProperties.navCapMask,
							pRequester->m_Parameters.m_fPassRadius, pRequester->m_lastNavNodeIndex, 5.0f, isVehicle ? 0 : &pfr.endPos, false, pRequester->GetName());
						pfr.endIndex = endNodeIndex;
					}
				}
			} // !rejectPath

			float distSq = Distance::Point_Point(origPos, pfr.endPos);
			if (distSq > square(endTol))
				rejectPath = true;
		} // endTol > 0
		else if (m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->GetTriangularNavData()->isForbiddenDesigner || IsPointOnForbiddenEdge(pfr.endPos, edgeTol, 0, 0, false))
		{
			rejectPath = true;
		}

    if (rejectPath)
    {
      AILogComment("CAISystem::RequestPathTo Path destination (%5.2f, %5.2f, %5.2f) is unreachable (tol was %5.2f) - returning no path",
        pfr.endPos.x, pfr.endPos.y, pfr.endPos.z, endTol);
      SAIEVENT sai;
      sai.bPathFound = false;
      pRequester->Event(AIEVENT_ONPATHDECISION,&sai);
      return;
    }
  }

	pRequester->m_lastNavNodeIndex = pfr.startIndex;

	if( !pfr.startIndex || !pfr.endIndex )
	{
		if (!pfr.startIndex)
		{
			AIWarning("CAISystem::RequestPathTo pts not available for %s: %s %s (from (%5.2f %5.2f %5.2f) to (%5.2f %5.2f %5.2f) Trying straight path", 
				pRequester->GetName(), 
				pfr.startIndex == 0 ? "start" : "", 
				pfr.endIndex == 0 ? "end" : "",
				pfr.startPos.x, pfr.startPos.y, pfr.startPos.z,
				pfr.endPos.x, pfr.endPos.y, pfr.endPos.z);
			CNavPath origNavPath = m_navPath;
			m_navPath.Clear("CAISystem::RequestPathTo no start node");
			// just try a straight line path - nothing to loose
			m_navPath.SetParams(SNavPathParams(start, end, ZERO, endDir, forceTargetBuildingId, allowDangerousDestination, endDistance));
			Vec3 startPos = pRequester->GetPhysicsPos();
			CPipeUser *pPiper = (CPipeUser *) pRequester;
			if (!pPiper->m_forcePathGenerationFrom.IsZero())
				startPos = pPiper->m_forcePathGenerationFrom;
			IAISystem::ENavigationType navType = pfr.endIndex ? m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->navType : IAISystem::NAV_UNSET;
			m_navPath.PushBack(PathPointDescriptor(navType, startPos));
			m_navPath.PushBack(PathPointDescriptor(navType, end));
			m_navPath.SetPathEndIsAsRequested(false);
			SAIEVENT sai;
			sai.bPathFound = true;
			sai.vPosition = end;
			pRequester->Event(AIEVENT_ONPATHDECISION,&sai);
			m_navPath = origNavPath;
			return;
		}
		else
		{
			// not having an end is interesting, but shouldn't be a problem
			AILogComment("CAISystem::RequestPathTo points not available for %s: %s %s (from (%5.2f %5.2f %5.2f) to (%5.2f %5.2f %5.2f)", 
				pRequester->GetName(), 
				pfr.startIndex == 0 ? "start" : "", 
				pfr.endIndex == 0 ? "end" : "",
				pfr.startPos.x, pfr.startPos.y, pfr.startPos.z,
				pfr.endPos.x, pfr.endPos.y, pfr.endPos.z);
			SAIEVENT sai;
			sai.bPathFound = false;
			pRequester->Event(AIEVENT_ONPATHDECISION,&sai);
			return;
		}
	}

	// To use the directional params then the start/end positions are adjusted so long as 
	// there is LoS between the original and adjusted positions. Then the extra points 
	// must be added at the end - this is done in CAISystem::BeautifyPath.
	// We do this here because we need the node pointer... and assume that the extra offset
	// doesn't take us significantly out of the node.
	/*	if (m_cvBeautifyPath->GetIVal() != 0 &&
	pfr.pEnd->navType == IAISystem::NAV_VOLUME && 
	pfr.endDir.GetLengthSquared() > 0.01f)
	{
	// Danny TODO - for now just set the length to be an arbitrary amount
	pfr.endDir.SetLength(7.0f);
	if (m_pVolumeNavRegion->SweptSphereWorldIntersection(pfr.endPos, pfr.endPos - pfr.endDir, pfr.pRequester->m_Parameters.m_fPassRadius, true, true))
	{
	AILogComment("CAISystem::RequestPathTo Using endDir (%5.2f, %5.2f, %5.2f) from endPos (%5.2f, %5.2f, %5.2f)"
	"would generate bad path for %s", 
	pfr.endDir.x, pfr.endDir.y, pfr.endDir.z, 
	pfr.endPos.x, pfr.endPos.y, pfr.endPos.z,
	pfr.pRequester->GetName());
	pfr.endDir.Set(0.0f, 0.0f, 0.0f);
	}
	else
	{
	// the real endPos will get added onto the path after it's been created, safe
	// in the knowledge that there's line of sight between the real and "fake" end
	// position.
	pfr.endPos -= pfr.endDir;
	}
	}
	else*/
	//  {
	//    pfr.endDir.Set(0, 0, 0);
	//  }

	if ( pfr.startIndex != pfr.endIndex && ExitNodeImpossible(m_pGraph->GetLinkManager(), m_pGraph->GetNodeManager().GetNode(pfr.startIndex),pRequester->m_Parameters.m_fPassRadius) )
	{
		CNavPath origNavPath = m_navPath;
		GraphNode* pStart = m_pGraph->GetNodeManager().GetNode(pfr.startIndex);
		const Vec3& pos = pStart->GetPos();
		AIWarning("CAISystem::RequestPathTo Node at position (%.3f,%.3f,%.3f) (type %d) rejected as path start due to pass radius (%5.2f) - trying straight path.",
			pos.x,pos.y,pos.z, pStart->navType, pRequester->m_Parameters.m_fPassRadius);
		m_navPath.Clear("CAISystem::RequestPathTo no exit from start node");
		// just try a straight line path - nothing to loose
		m_navPath.SetParams(SNavPathParams(start, end, ZERO, endDir, forceTargetBuildingId, allowDangerousDestination, endDistance));
		Vec3 startPos = pRequester->GetPhysicsPos();
		CPipeUser *pPiper = (CPipeUser *) pRequester;
		if (!pPiper->m_forcePathGenerationFrom.IsZero())
			startPos = pPiper->m_forcePathGenerationFrom;
		IAISystem::ENavigationType navType = pfr.endIndex ? m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->navType : pStart->navType;
		m_navPath.PushBack(PathPointDescriptor(navType, startPos));
		if (pStart->navType == IAISystem::NAV_FLIGHT)
			m_navPath.PushBack(PathPointDescriptor(navType, end + Vec3(0.0f, 0.0f, (end - startPos).GetLength()))); // almost always OK if we just go up!
		else
			m_navPath.PushBack(PathPointDescriptor(navType, end));
		m_navPath.SetPathEndIsAsRequested(false);
		SAIEVENT sai;
		sai.bPathFound = true;
		sai.vPosition = end;

		pRequester->Event(AIEVENT_ONPATHDECISION,&sai);
		m_navPath = origNavPath;
		return;
	}

	if (pfr.startIndex != pfr.endIndex && EnterNodeImpossible(m_pGraph->GetNodeManager(), m_pGraph->GetLinkManager(), m_pGraph->GetNodeManager().GetNode(pfr.endIndex), pRequester->m_Parameters.m_fPassRadius))
	{
		const Vec3& pos = m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->GetPos();
		if (pfr.endTol > 0.0f)
		{
			AILogComment("CAISystem::RequestPathTo Node at position (%.3f,%.3f,%.3f) rejected as path destination due to pass radius - calculating partial path.",pos.x,pos.y,pos.z);
		}
		else
		{
			AILogComment("CAISystem::RequestPathTo Node at position (%.3f,%.3f,%.3f) rejected as path destination due to pass radius - no path.",pos.x,pos.y,pos.z);
			SAIEVENT sai;
			sai.bPathFound = false;
			pRequester->Event(AIEVENT_ONPATHDECISION,&sai);
			return;
		}
	}

	// check for a very short path - fii.e. one that has start/end nodes either the same or adjacent
	if (CheckForAndHandleShortPath(pfr))
		return;

	QueuePathFindRequest(pfr);
}

//====================================================================
// RegisterPathFinderListener
//====================================================================
void CAISystem::RegisterPathFinderListener(IAIPathFinderListerner* pListener)
{
	m_pathFindListeners.insert(pListener);
}

//====================================================================
// UnregisterPathFinderListener
//====================================================================
void CAISystem::UnregisterPathFinderListener(IAIPathFinderListerner* pListener)
{
	m_pathFindListeners.erase(pListener);
}

//====================================================================
// NotifyPathFinderListeners
//====================================================================
void CAISystem::NotifyPathFinderListeners(int id, const std::vector<unsigned>* pathNodes)
{
	for (unsigned i = 0, ni = m_pathFindListeners.size(); i < ni; ++i)
	{
		if (m_pathFindListeners[i])
			m_pathFindListeners[i]->OnPathResult(id, pathNodes);
	}
}

//====================================================================
// RequestRawPathTo
//====================================================================
int CAISystem::RequestRawPathTo(const Vec3 &start, const Vec3 &end,
										float passRadius, IAISystem::tNavCapMask navCapMask, unsigned& lastNavNode,
										bool allowDangerousDestination, float endTol, const CStandardHeuristic::ExtraConstraints& constraints,
										CPipeUser *pReference)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

	if (m_cvDebugPathFinding->GetIVal())
		AILogAlways("CAISystem::RequestRawPathTo from (%5.2f, %5.2f, %5.2f) to (%5.2f, %5.2f, %5.2f)",
			start.x, start.y, start.z, end.x, end.y, end.z);

	PathFindRequest pfr(PathFindRequest::TYPE_RAW);
	pfr.startPos = start;
	pfr.startDir.zero();
	pfr.endPos = end;
	pfr.endDir.zero();
	pfr.pRequester = pReference;	// used for checking smart objects
	pfr.allowDangerousDestination = allowDangerousDestination;
	pfr.nForceTargetBuildingId = -1;
	pfr.bPathEndIsAsRequested = true;
	pfr.endTol = endTol;
	pfr.endDistance = 0.0f;
	pfr.id = m_pathFindIdGen++;
	pfr.navCapMask = navCapMask;
	pfr.passRadius = passRadius;
	pfr.extraConstraints = constraints;

	if (end.IsZero())
	{
		AIWarning("CAISystem::RequestRawPathTo passed zero position returning no path");
		return 0;
	}

	// Now get the graph nodes for start/end
	// if a vehicle then avoid extra tests based on human walkability
	unsigned startNodeIndex = m_pGraph->GetEnclosing(pfr.startPos, pfr.navCapMask, passRadius, lastNavNode, 5.0f, &pfr.startPos, true, "Raw Path");
	pfr.startIndex = startNodeIndex;

	unsigned endNodeIndex = m_pGraph->GetEnclosing(pfr.endPos, pfr.navCapMask, passRadius, lastNavNode, 5.0f, &pfr.endPos, endTol > 0.0f, "Raw Path");
	pfr.endIndex = endNodeIndex;

	// move the start out of forbidden if it's on the edge
	if (pfr.startIndex && m_pGraph->GetNodeManager().GetNode(pfr.startIndex)->navType == IAISystem::NAV_TRIANGULAR)
	{
		Vec3 origPos = pfr.startPos;
		float edgeTol = passRadius;
		for (int nTries = 0 ; nTries < 2 ; ++nTries)
		{
			Vec3 newPos = GetPointOutsideForbidden(pfr.startPos, edgeTol);
			float distSq = Distance::Point_Point2DSq(newPos, pfr.startPos);
			if (distSq <= 0.0f)
				break;
			else
				pfr.startPos = newPos;
		}
		// maybe have to re-evaluate start node
		float distSq = Distance::Point_Point(pfr.startPos, origPos);
		if (distSq > 0.1f)
		{
			unsigned startNodeIndex = m_pGraph->GetEnclosing(pfr.startPos, navCapMask, passRadius, lastNavNode, 5.0f, &pfr.startPos, true, "Raw Path");
			pfr.startIndex = startNodeIndex;
		}
	}


	// never trace into a forbidden area - and in some cases move the destination outside of forbidden
	if (pfr.endIndex && m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->navType == IAISystem::NAV_TRIANGULAR)
	{
		float edgeTol = passRadius;
		if (!allowDangerousDestination)
			edgeTol += m_cvExtraForbiddenRadiusDuringBeautification->GetFVal();

		bool rejectPath = false;
		Vec3 origPos = pfr.endPos;

		if (endTol > 0.0f)
		{
			for (int nTries = 0 ; nTries < 2 ; ++nTries)
			{
				Vec3 newPos = GetPointOutsideForbidden(pfr.endPos, edgeTol);
				float distSq = Distance::Point_PointSq(origPos, newPos);
				pfr.endPos = newPos;

				if (distSq < square(0.1f))
					break;

				if (distSq > square(endTol))
				{
					rejectPath = true;
					break;
				}
			}

			// maybe have to re-evaluate end node
			if (!rejectPath)
			{
				float distSq = Distance::Point_PointSq(pfr.endPos, origPos);
				if (distSq > 0.1f)
				{
					unsigned endNodeIndex = m_pGraph->GetEnclosing(pfr.endPos, navCapMask, passRadius, lastNavNode, 5.0f, &pfr.endPos, false, "Raw Path");
					pfr.endIndex = endNodeIndex;
				}
			} // !rejectPath

			float distSq = Distance::Point_Point(origPos, pfr.endPos);
			if (distSq > square(endTol))
				rejectPath = true;
		} // endTol > 0
		else if (m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->GetTriangularNavData()->isForbiddenDesigner || IsPointOnForbiddenEdge(pfr.endPos, edgeTol, 0, 0, false))
		{
			rejectPath = true;
		}

		if (rejectPath)
		{
			AILogComment("CAISystem::RequestRawPathTo Path destination (%5.2f, %5.2f, %5.2f) is unreachable (tol was %5.2f) - returning no path",
				pfr.endPos.x, pfr.endPos.y, pfr.endPos.z, endTol);
			return -1;
		}
	}

	lastNavNode = pfr.startIndex;

	if (!pfr.startIndex || !pfr.endIndex)
		return -1;

	if (pfr.startIndex != pfr.endIndex && ExitNodeImpossible(m_pGraph->GetLinkManager(), m_pGraph->GetNodeManager().GetNode(pfr.startIndex), passRadius))
	{
		CNavPath origNavPath = m_navPath;
		GraphNode* pStart = m_pGraph->GetNodeManager().GetNode(pfr.startIndex);
		const Vec3& pos = pStart->GetPos();
		AIWarning("CAISystem::RequestRawPathTo Node at position (%.3f,%.3f,%.3f) (type %d) rejected as path start due to pass radius (%5.2f).",
			pos.x, pos.y, pos.z, pStart->navType, passRadius);
		return -1;
	}

	if (pfr.startIndex != pfr.endIndex && EnterNodeImpossible(m_pGraph->GetNodeManager(), m_pGraph->GetLinkManager(), m_pGraph->GetNodeManager().GetNode(pfr.endIndex), passRadius))
	{
		const Vec3& pos = m_pGraph->GetNodeManager().GetNode(pfr.endIndex)->GetPos();
		if (pfr.endTol > 0.0f)
		{
			AILogComment("CAISystem::RequestRawPathTo Node at position (%.3f,%.3f,%.3f) rejected as path destination due to pass radius - calculating partial path.",pos.x,pos.y,pos.z);
		}
		else
		{
			AILogComment("CAISystem::RequestRawPathTo Node at position (%.3f,%.3f,%.3f) rejected as path destination due to pass radius - no path.",pos.x,pos.y,pos.z);
			return -1;
		}
	}

	QueuePathFindRequest(pfr);

	return pfr.id;
}


//====================================================================
// UpdatePathFinder
//====================================================================
void CAISystem::UpdatePathFinder()
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	if (!m_pCurrentRequest)
		m_nPathfinderResult = PATHFINDER_POPNEWREQUEST;

	if (m_pCurrentRequest)
	{
		if ((m_fFrameStartTime - m_fLastPathfindTimeStart).GetSeconds() > m_cvAllowedTimeForPathfinding->GetFVal()) 
		{
			AIWarning( "Pathfinder time out %f s (max: %f s) by %s.", (m_fFrameStartTime - m_fLastPathfindTimeStart).GetSeconds(),
				m_cvAllowedTimeForPathfinding->GetFVal(), m_pCurrentRequest->GetRequesterName() );
			m_nPathfinderResult = PATHFINDER_ABORT;
		}
	}

	// Beware that outputting the timeout warning above can use up all
	// our time if we start the stopper at the beginning of this function!
	CCalculationStopper stopper("Pathfinder", m_cvPathfinderUpdateTime->GetFVal(), 70000);
		
	while (!stopper.ShouldCalculationStop())
	{
		switch (m_nPathfinderResult)
		{
		case PATHFINDER_STILLFINDING:
			switch (m_pAStarSolver->ContinueAStar(stopper))
			{
			case ASTAR_NOPATH: 
				if (m_cvDebugPathFinding->GetIVal())
					AILogAlways("A* ContinueAStar returned no path for %s", m_pCurrentRequest->GetRequesterName());
				m_nPathfinderResult = PATHFINDER_NOPATH; 
				break;
			case ASTAR_STILLSOLVING: 
				m_nPathfinderResult = PATHFINDER_STILLFINDING; 
				break;
			case ASTAR_PATHFOUND: 
				if (m_cvDebugPathFinding->GetIVal())
				{
					AILogAlways("A* ContinueAStar returned path for %s", m_pCurrentRequest->GetRequesterName());
				}
				if (m_pCurrentRequest->type == PathFindRequest::TYPE_RAW)
				{
					// Raw path, return result.
					m_nPathfinderResult = PATHFINDER_PATHFOUND;
				}
				else
				{
					// Agent path, beautify.
					m_nPathfinderResult = PATHFINDER_BEAUTIFYINGPATH; 
				}
				break;
			}
			break;

		case PATHFINDER_BEAUTIFYINGPATH:
			{
				if (m_pCurrentRequest->type == PathFindRequest::TYPE_RAW)
				{
					// Should not happen, just in case.
					m_nPathfinderResult = PATHFINDER_NOPATH;
					AIWarning("Cannot beautify path for %s", m_pCurrentRequest->GetRequesterName());
					break;
				}
				if (m_cvDebugPathFinding->GetIVal())
					AILogAlways("Beautifying path for %s", m_pCurrentRequest->GetRequesterName());
				m_navPath.Clear("CAISystem::UpdatePathFinder - PATHFINDER_BEAUTIFYINGPATH");
				const CAStarSolver::tPathNodes & pathNodes = m_pAStarSolver->GetPathNodes();
				if (pathNodes.empty())
				{
					AIWarning("No path nodes before beautifying path for %s", m_pCurrentRequest->GetRequesterName());
					m_nPathfinderResult = PATHFINDER_NOPATH;
				}
				else if (m_cvBeautifyPath->GetIVal() != 0)
				{
					// See if a straight (well, almost straight because we need to handle the pass-radius)
					// path would actually be cheaper
					if (m_cvAttemptStraightPath->GetIVal() != 0)
					{
						Vec3  startPos = m_pCurrentRequest->pRequester->GetPhysicsPos();
						CPipeUser *pPiper = (CPipeUser *) m_pCurrentRequest->pRequester;
						if (!pPiper->m_forcePathGenerationFrom.IsZero())
							startPos = pPiper->m_forcePathGenerationFrom;
						TPathPoints straightPath;
						float straightCost;
						if (m_pCurrentRequest->isDirectional)
						{
							Vec3 dir = (m_pCurrentRequest->endPos - startPos);
							float maxDist = dir.NormalizeSafe();
							sStandardHeuristic.ClearConstraints();
							straightCost = AttemptPathInDir(straightPath, startPos, dir, *m_pCurrentRequest, 
								m_pAStarSolver->GetHeuristic(), maxDist, m_navigationBlockers);
						}
						else
						{
							straightCost = AttemptStraightPath(straightPath, *m_pCurrentRequest, startPos,
								m_pAStarSolver->GetHeuristic(), std::numeric_limits<float>::max(), m_navigationBlockers, false);
						}
						if (!straightPath.empty() && straightCost > 0.0f && straightCost < m_pGraph->GetNodeManager().GetNode(m_pCurrentRequest->endIndex)->fCostFromStart)
						{
							m_navPath.Clear("CAISystem::UpdatePathFinder - PATHFINDER_BEAUTIFYINGPATH straight path");
							m_navPath.SetPathEndIsAsRequested(m_pCurrentRequest->bPathEndIsAsRequested);
							// just convert the straight path
							for (TPathPoints::iterator it = straightPath.begin() ; it != straightPath.end() ; ++it)
								m_navPath.PushBack(*it);
						}
						else
						{
							// Use the A* path after beautifying it
							BeautifyPath(pathNodes);
						}
					}
					else
					{
						// Use the A* path after beautifying it
						BeautifyPath(pathNodes);
					}
					m_nPathfinderResult = m_navPath.GetPath().empty() ? PATHFINDER_NOPATH : PATHFINDER_PATHFOUND;
				}
				else
				{
					BeautifyPath(pathNodes);
					m_nPathfinderResult = m_navPath.GetPath().empty() ? PATHFINDER_NOPATH : PATHFINDER_PATHFOUND;
				}
			}
			break;

		case PATHFINDER_PATHFOUND:
			{
				if (m_pCurrentRequest->type == PathFindRequest::TYPE_RAW)
				{
					// Raw path was requested.
					NotifyPathFinderListeners(m_pCurrentRequest->id, &m_pAStarSolver->GetPathNodes());
					AILogComment("Pathfinding for #%d took %5.2f seconds", m_pCurrentRequest->id, (m_fFrameStartTime - m_fLastPathfindTimeStart).GetSeconds());
					delete m_pCurrentRequest;
					m_pCurrentRequest = 0;
					m_nPathfinderResult = PATHFINDER_POPNEWREQUEST;
					m_navPath.Clear("CAISystem::UpdatePathFinder - RawPath");
				}
				else
				{
					// danny bug report crysis-735 says it hit this assert (when it was AIAssert). It shouldn't be possible, 
					// according to the logic here, unless m_navPath got cleared in between one update and the next. So, 
					// convert it to assert (so if you hit it please tell me), and handle it.
					assert(!m_navPath.GetPath().empty());
					SAIEVENT event;
					if (m_navPath.GetPath().empty())
					{
						event.bPathFound = false;
					}
					else
					{
						event.bPathFound = true;
						m_navPath.SetEndDir( m_pCurrentRequest->endDir );
						if (m_navPath.GetPath().size() == 1)
							m_navPath.PushFront(PathPointDescriptor(IAISystem::NAV_UNSET, m_pCurrentRequest->pRequester->GetPhysicsPos()), true);
					}
					AILogComment("Pathfinding for %s took %5.2f seconds", m_pCurrentRequest->GetRequesterName(), (m_fFrameStartTime - m_fLastPathfindTimeStart).GetSeconds());
					m_pCurrentRequest->pRequester->Event(AIEVENT_ONPATHDECISION,&event);
					delete m_pCurrentRequest;
					m_pCurrentRequest = 0;
					m_nPathfinderResult = PATHFINDER_POPNEWREQUEST;
				}
			}

			break;

		case PATHFINDER_NOPATH:
			{
				if (m_pCurrentRequest->type == PathFindRequest::TYPE_RAW)
				{
					// Raw path was requested.
					const CAStarSolver::tPathNodes & pathNodes = m_pAStarSolver->CalculateAndGetPartialPath();
					if (!pathNodes.empty())
					{
						AILogComment("Using partial path for #%d", m_pCurrentRequest->id);
						NotifyPathFinderListeners(m_pCurrentRequest->id, &pathNodes);
					}
					else
					{
						AILogComment("Path failed for #%d", m_pCurrentRequest->id);
						NotifyPathFinderListeners(m_pCurrentRequest->id, 0);
					}
					AILogComment("Pathfinding for #%d took %5.2f seconds", m_pCurrentRequest->id, (m_fFrameStartTime - m_fLastPathfindTimeStart).GetSeconds());
					delete m_pCurrentRequest;
					m_pCurrentRequest = 0;
					m_nPathfinderResult = PATHFINDER_POPNEWREQUEST;
					m_navPath.Clear("CAISystem::UpdatePathFinder - RawPath");
				}
				else
				{
					AILogComment("Path failed for %s", m_pCurrentRequest->GetRequesterName());
					// if the reason for no path was that the destination was a little bit too tight - well
					// just find the closest
					if (m_pCurrentRequest->endTol > 0.0f && !PointInBrokenRegion(m_pCurrentRequest->startPos))
					{
						AILogComment("Attempting partial path for %s", m_pCurrentRequest->GetRequesterName());
						m_navPath.Clear("CAISystem::UpdatePathFinder - PATHFINDER_NOPATH before partial");
						m_navPath.SetPathEndIsAsRequested(false);
						// could use either the partial path, or straight/directional path
						const CAStarSolver::tPathNodes & pathNodes = m_pAStarSolver->CalculateAndGetPartialPath();
						if (!pathNodes.empty())
						{
							// bear in mind that the beautifier adds the final destination - since we don't want that
							// then force it not to
							Vec3 origEndPos = m_pCurrentRequest->endPos;
							m_pCurrentRequest->endPos = m_pGraph->GetNodeManager().GetNode(pathNodes.back())->GetPos();
							m_pCurrentRequest->endDir.Set(0, 0, 0);
							BeautifyPath(pathNodes);
							m_pCurrentRequest->endPos = origEndPos;
						}

						if (m_cvAttemptStraightPath->GetIVal() != 0)
						{
							Vec3  startPos = m_pCurrentRequest->pRequester->GetPhysicsPos();
							CPipeUser *pPiper = (CPipeUser *) m_pCurrentRequest->pRequester;
							if (!pPiper->m_forcePathGenerationFrom.IsZero())
								startPos = pPiper->m_forcePathGenerationFrom;
							TPathPoints straightPath;
							float straightCost;
							if (m_pCurrentRequest->isDirectional)
							{
								Vec3 dir = (m_pCurrentRequest->endPos - startPos);
								float maxDist = dir.NormalizeSafe();
								sStandardHeuristic.ClearConstraints();
								straightCost = AttemptPathInDir(straightPath, startPos, dir, *m_pCurrentRequest, 
									m_pAStarSolver->GetHeuristic(), maxDist, m_navigationBlockers);
							}
							else
							{
								straightCost = AttemptStraightPath(straightPath, *m_pCurrentRequest, startPos,
									m_pAStarSolver->GetHeuristic(), std::numeric_limits<float>::max(), m_navigationBlockers, true);
							}

							if (!straightPath.empty() && straightCost >= 0.0f)
							{
								float straightDist = Distance::Point_Point(straightPath.back().vPos, m_pCurrentRequest->endPos);
								float pathDist = Distance::Point_Point(m_navPath.GetLastPathPos(), m_pCurrentRequest->endPos);
								if (straightDist < pathDist)
								{
									AILogComment("CAISystem::UpdatePathFinder - PATHFINDER_NOPATH using straight path instead of partial for %s", m_pCurrentRequest->GetRequesterName());
									m_navPath.Clear("CAISystem::UpdatePathFinder - PATHFINDER_NOPATH using straight path instead of partial");
									// just convert the straight path
									for (TPathPoints::iterator it = straightPath.begin() ; it != straightPath.end() ; ++it)
										m_navPath.PushBack(*it);
									m_nPathfinderResult = m_navPath.GetPath().empty() ? PATHFINDER_NOPATH : PATHFINDER_PATHFOUND;
									break;
								}
								else
								{
									AILogComment("CAISystem::UpdatePathFinder - PATHFINDER_NOPATH using partial path instead of straight for %s", m_pCurrentRequest->GetRequesterName());
								}
							}
						}
						if (!m_navPath.GetPath().empty())
						{
							AILogComment("Using partial path for %s", m_pCurrentRequest->GetRequesterName());
							m_nPathfinderResult = PATHFINDER_PATHFOUND;
							break; // finish the switch case
						}
						SAIEVENT event;
						event.bPathFound = false;
						m_navPath.Clear("CAISystem::UpdatePathFinder - PATHFINDER_NOPATH");
						AILogComment("Pathfinding for %s took %5.2f seconds", m_pCurrentRequest->GetRequesterName(), (m_fFrameStartTime - m_fLastPathfindTimeStart).GetSeconds());
						m_pCurrentRequest->pRequester->Event(AIEVENT_ONPATHDECISION,&event);
						delete m_pCurrentRequest;
						m_pCurrentRequest = 0;
						m_nPathfinderResult = PATHFINDER_POPNEWREQUEST;
						break; // finish the switch case
					}
					else
					{
						SAIEVENT event;
						event.bPathFound = false;
						m_navPath.Clear("CAISystem::UpdatePathFinder - PATHFINDER_NOPATH no partial");

						// if the requestor was inside a "broken" region then just try a straight line path
						if (PointInBrokenRegion(m_pCurrentRequest->startPos))
						{
							Vec3 fakeDir = m_pCurrentRequest->endPos - m_pCurrentRequest->startPos;
							if (fakeDir.GetLengthSquared() > 1.0f)
							{
								AILogComment("Trying straight path for %s", m_pCurrentRequest->GetRequesterName());
								Vec3 fakePos = m_pCurrentRequest->startPos + 3.0f * (fakeDir / fakeDir.GetLength());
								m_navPath.PushBack(PathPointDescriptor(IAISystem::NAV_TRIANGULAR, fakePos));
								m_navPath.SetPathEndIsAsRequested(m_pCurrentRequest->bPathEndIsAsRequested);
								event.bPathFound = true;
								event.vPosition = fakePos;
							}
						}
						AILogComment("Pathfinding for %s took %5.2f seconds", m_pCurrentRequest->GetRequesterName(), (m_fFrameStartTime - m_fLastPathfindTimeStart).GetSeconds());
						m_pCurrentRequest->pRequester->Event(AIEVENT_ONPATHDECISION,&event);
						delete m_pCurrentRequest;
						m_pCurrentRequest = 0;
						m_nPathfinderResult = PATHFINDER_POPNEWREQUEST;
					}
				}
			}
			break;
		case PATHFINDER_ABORT:
			{
				if (m_pCurrentRequest->type == PathFindRequest::TYPE_RAW)
				{
					// Raw path was requested.
					const CAStarSolver::tPathNodes & pathNodes = m_pAStarSolver->CalculateAndGetPartialPath();
					if (!pathNodes.empty())
					{
						AILogComment("Pathfinding aborted but using partial path for #%d", m_pCurrentRequest->id);
						NotifyPathFinderListeners(m_pCurrentRequest->id, &pathNodes);
					}
					else
					{
						AILogComment("Pathfinding aborted #%d", m_pCurrentRequest->id);
						NotifyPathFinderListeners(m_pCurrentRequest->id, 0);
					}
					delete m_pCurrentRequest;
					m_pCurrentRequest = 0;
					m_nPathfinderResult = PATHFINDER_POPNEWREQUEST;
					m_navPath.Clear("CAISystem::UpdatePathFinder - RawPath");
				}
				else
				{
					if (GetAISystem()->m_cvCrowdControlInPathfind->GetIVal() != 0 && 
						m_pCurrentRequest->pRequester->m_movementAbility.pathRegenIntervalDuringTrace > 0.0f)
					{
						AILogComment("Attempting partial path for %s", m_pCurrentRequest->GetRequesterName());
						m_navPath.Clear("CAISystem::UpdatePathFinder - PATHFINDER_ABORT before partial");
						m_navPath.SetPathEndIsAsRequested(false);
						// could use either the partial path, or straight/directional path
						const CAStarSolver::tPathNodes & pathNodes = m_pAStarSolver->CalculateAndGetPartialPath();
						if (!pathNodes.empty())
						{
							// bear in mind that the beautifier adds the final destination, which is what we want (even though the
							// final path segment will be bad... just hope we regen before we get there)
							BeautifyPath(pathNodes);
						}

						if (!m_navPath.GetPath().empty())
						{
							AILogComment("Pathfinding aborted but using partial path for %s", m_pCurrentRequest->GetRequesterName());
							m_nPathfinderResult = PATHFINDER_PATHFOUND;
							m_fLastPathfindTimeStart = m_fFrameStartTime;
							break; // finish the switch case
						}
					}

					SAIEVENT event;
					event.bPathFound = false;
					m_navPath.Clear("CAISystem::UpdatePathFinder - PATHFINDER_ABORT");
					AILogComment("Pathfinding (aborted, no partial path) for %s took %5.2f seconds", m_pCurrentRequest->GetRequesterName(), (m_fFrameStartTime - m_fLastPathfindTimeStart).GetSeconds());
					m_pCurrentRequest->pRequester->Event(AIEVENT_ONPATHDECISION,&event);
					delete m_pCurrentRequest;
					m_pCurrentRequest = 0;
					m_nPathfinderResult = PATHFINDER_POPNEWREQUEST;
				}
			}
			break;
		case PATHFINDER_POPNEWREQUEST:
			{
				if (!m_pCurrentRequest)
				{
					// get request if any waiting in queue
					if (m_lstPathQueue.empty()) 
						return;
					m_pCurrentRequest = (*m_lstPathQueue.begin());
					m_lstPathQueue.pop_front();

					if (m_cvDebugPathFinding->GetIVal())
						AILogAlways("Processing path request for %s", m_pCurrentRequest->GetRequesterName());

					// set the params here since various things can get tweaked subsequently. However, this does
					// mean that m_navPath must not get "corrupted" between now and when the path is found (e.g.
					// by a trivial path)
					m_navPath.SetParams(SNavPathParams(
						m_pCurrentRequest->startPos, m_pCurrentRequest->endPos, 
						m_pCurrentRequest->startDir, m_pCurrentRequest->endDir,
						m_pCurrentRequest->nForceTargetBuildingId, m_pCurrentRequest->allowDangerousDestination,
						m_pCurrentRequest->endDistance, false, m_pCurrentRequest->isDirectional));

					// do A*
					m_nPathfinderResult = PATHFINDER_NOPATH;
					m_fLastPathfindTimeStart = m_fFrameStartTime;

					sStandardHeuristic.SetStartEndData(
						m_pGraph->GetNodeManager().GetNode(m_pCurrentRequest->startIndex),
						m_pCurrentRequest->startPos,
						m_pGraph->GetNodeManager().GetNode(m_pCurrentRequest->endIndex),
						m_pCurrentRequest->endPos,
						m_pCurrentRequest->extraConstraints);

					if (m_pCurrentRequest->type == PathFindRequest::TYPE_ACTOR)
					{
						CPathfinderRequesterProperties requesterProperties;
						requesterProperties.properties = m_pCurrentRequest->pRequester->m_movementAbility.pathfindingProperties;
						requesterProperties.radius = m_pCurrentRequest->pRequester->m_Parameters.m_fPassRadius;
						sStandardHeuristic.SetRequesterProperties(requesterProperties, m_pCurrentRequest->pRequester);
						GetNavigationBlockers(m_navigationBlockers, m_pCurrentRequest->pRequester, m_pCurrentRequest);
					}
					else
					{
						CPathfinderRequesterProperties requesterProperties;
						requesterProperties.properties.SetToDefaults();
						requesterProperties.properties.navCapMask = m_pCurrentRequest->navCapMask;
						requesterProperties.radius = m_pCurrentRequest->passRadius;
						sStandardHeuristic.SetRequesterProperties(requesterProperties, m_pCurrentRequest->pRequester);
						m_navigationBlockers.clear();
					}

					bool bDebugDrawOpenList = false;
					if (m_cvDebugPathFinding->GetIVal() > 0)
					{
						const char* szName = m_cvDebugDrawAStarOpenList->GetString();
						if (szName && strcmp(szName, "0") != 0)
						{
							if (strcmp(m_pCurrentRequest->GetRequesterName(), szName) == 0)
								bDebugDrawOpenList = true;
						}
					}

					switch(m_pAStarSolver->SetupAStar(stopper, m_pGraph, &sStandardHeuristic,
						m_pCurrentRequest->startIndex, m_pCurrentRequest->endIndex,
						m_navigationBlockers, bDebugDrawOpenList))
					{
					case ASTAR_NOPATH: 
						if (m_cvDebugPathFinding->GetIVal())
							AILogAlways("A* SetupAStar returned no path for %s", m_pCurrentRequest->GetRequesterName());
						m_nPathfinderResult = PATHFINDER_NOPATH; 
						break;
					case ASTAR_STILLSOLVING: 
						m_nPathfinderResult = PATHFINDER_STILLFINDING; 
						break;
					case ASTAR_PATHFOUND: 
						if (m_cvDebugPathFinding->GetIVal())
							AILogAlways("Got immediate ASTAR_PATHFOUND for %s", m_pCurrentRequest->GetRequesterName());
						if (m_pCurrentRequest->type == PathFindRequest::TYPE_RAW)
							m_nPathfinderResult = PATHFINDER_PATHFOUND; 
						else
							m_nPathfinderResult = PATHFINDER_BEAUTIFYINGPATH; 
						break;
					}
				}
			}
			break;
		}//end switch
	}// end while
}


//
//-----------------------------------------------------------------------------------------------------------
CAIObject * CAISystem::GetAIObjectByName(const char *pName) const
{
	AIObjects::const_iterator ai;

	ai = m_Objects.begin();
	while (ai!=m_Objects.end())
	{
		CAIObject *pObject = ai->second;

		if (strcmp(pName, pObject->GetName()) == 0)
			return pObject;
		++ai;
	}

	return 0;
}


//====================================================================
// IsAIObjectRegistered
//====================================================================
bool CAISystem::IsAIObjectRegistered(const CAIObject* pObject) const
{
	for (AIObjects::const_iterator ai = m_Objects.begin() ; ai != m_Objects.end() ; ++ai)
	{
		if (ai->second == pObject)
		{
			return true;
		}
	}
	const CAIObject *pConstObject = (const CAIObject*) pObject;
	//  SetAIObjects::const_iterator it = m_setDummies.find(pConstObject);
	SetAIObjects::const_iterator it = std::find(m_setDummies.begin(), m_setDummies.end(), pConstObject);
	if (it != m_setDummies.end())
		return true;
	return false;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::RemoveDummyObject(CAIObject *pObject)
{
FUNCTION_PROFILER( m_pSystem, PROFILE_AI );
	SetAIObjects::iterator it = m_setDummies.find(pObject);
	if (it == m_setDummies.end())
	{
		if (pObject)
			AIError("CAISystem::RemoveDummyObject Invalid object pointer %p", pObject);
		return;
	}
#ifndef USER_dejan
	AILogComment("CAISystem::RemoveDummyObject  %s (%p)", pObject->GetName(), pObject);
#endif
	m_setDummies.erase(it);

	AIObjects::iterator ai;
	// tell all puppets that this object is invalid
	if ( (ai=m_Objects.find(AIOBJECT_PUPPET))!= m_Objects.end() )
	{
		for (;ai!=m_Objects.end();)
		{
			if (ai->first != AIOBJECT_PUPPET)
				break;
			(ai->second)->OnObjectRemoved(pObject);
			++ai;
		}
	}

	// tell all vehicles that this object is invalid
	if ( (ai=m_Objects.find(AIOBJECT_VEHICLE))!= m_Objects.end() )
	{
		for (;ai!=m_Objects.end();)
		{
			if (ai->first != AIOBJECT_VEHICLE)
				break;
			(ai->second)->OnObjectRemoved(pObject);
			++ai;
		}
	}

	for(AIGroupMap::iterator git = m_mapAIGroups.begin(); git != m_mapAIGroups.end(); ++git)
		git->second->OnObjectRemoved(pObject);

	// make sure we dont delete any beacon
	BeaconMap::iterator bi;
	for (bi=m_mapBeacons.begin();bi!=m_mapBeacons.end();++bi)
	{
		if ((bi->second).pBeacon == pObject)
		{
			m_mapBeacons.erase(bi);
			break;
		}
	}

	m_lightManager.OnObjectRemoved(pObject);

	delete pObject;
}

//
//-----------------------------------------------------------------------------------------------------------
CAIObject * CAISystem::CreateDummyObject(const string &name, CAIObject::ESubTypes type)
{
	CAIObject *pObject = 0;
	if( type == CAIObject::STP_TRACKDUMMY )
		pObject = new CTrackDummy(this);
	else
		pObject = new CAIObject(this);

	pObject->SetType(AIOBJECT_DUMMY);
	pObject->SetSubType(type);

	pObject->SetAssociation(0);
	if(!name.empty() && name.length() != 0)
		pObject->SetName(name);

	std::pair<SetAIObjects::iterator, bool> status = m_setDummies.insert(pObject);
	AIAssert(status.second);

#ifndef USER_dejan
	AILogComment("CAISystem::CreateDummyObject %s (%p)", pObject->GetName(), pObject);
#endif

	return pObject;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::RemoveObject(IAIObject *pObject)
{
	AIObjects::iterator ai;
	CAIObject *pAIObject = (CAIObject*)pObject;

	if (!pObject)
		return;

#ifndef USER_dejan
	AILogComment("CAISystem::RemoveObject  %s (%p)", pObject->GetName(), pObject);
#endif

	unsigned short nType=pAIObject->GetType();
	if ( (ai=m_Objects.find(nType)) != m_Objects.end())
	{
		for (;ai!=m_Objects.end();)
		{
			if (ai->first != nType)
				break;
			if (ai->second == pObject)
			{
				pAIObject = ai->second;
				m_Objects.erase(ai);
				break;
			}

			++ai;
		}
	}
	else	// AIObject with invalid/mismatching type - make sure it's removed anyway
	{
		if(pObject->GetEntity() && pObject->GetEntity()->GetClass())
			AIWarning( "CAISystem::RemoveObject Trying to remove object with invalid type %d %s entuityClass %s", nType, pObject->GetName(), pObject->GetEntity()->GetClass()->GetName());
		else
			AIWarning( "CAISystem::RemoveObject Trying to remove object with invalid type %d %s ", nType, pObject->GetName());
		for( ai=m_Objects.begin(); ai!= m_Objects.end(); ++ai)
			if (ai->second == pObject)
			{
				pAIObject = ai->second;
				m_Objects.erase(ai);
				break;
			}
	}

	RemoveObjectFromAllOfType(AIOBJECT_PUPPET,pAIObject);
	RemoveObjectFromAllOfType(AIOBJECT_VEHICLE,pAIObject);
	RemoveObjectFromAllOfType(AIOBJECT_ATTRIBUTE,pAIObject);
	RemoveObjectFromAllOfType(AIOBJECT_LEADER,pAIObject);

	// take care of smart objects
	// smart object is entity listener, no need to remove it here
	/*
	if(gEnv->pEntitySystem && m_pSmartObjects)
	{
	IEntity *pEntity(gEnv->pEntitySystem->GetEntity(pAIObject->GetEntityID()));
	if(pEntity)
	m_pSmartObjects->RemoveEntity(pEntity);
	}
	*/
	// check if the player has the object as attention target
	CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
	if(pPlayer)
	{
		if(pPlayer->GetAttentionTarget()==pAIObject)
			pPlayer->UpdateAttentionTarget(NULL);
	}

	for(AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
		it->second->OnObjectRemoved(pAIObject);


	AIObjects::iterator	oi;

	for (oi=m_mapSpecies.begin();oi!=m_mapSpecies.end();++oi)
	{
		if (oi->second == pAIObject)
		{
			m_mapSpecies.erase(oi);
			break;
		}
	}

	for (oi=m_mapGroups.begin();oi!=m_mapGroups.end();++oi)
	{
		if (oi->second == pAIObject)
		{
			m_mapGroups.erase(oi);
			break;
		}
	}

	CLeader* pLeader = pAIObject->CastToCLeader();
	if(pLeader)
		pLeader->GetAIGroup()->SetLeader(0);

	FormationMap::iterator fi;
	for (fi=m_mapActiveFormations.begin();fi!=m_mapActiveFormations.end();++fi)
		fi->second->OnObjectRemoved(pAIObject);

	//remove this object from any pending paths generated for him
	CPuppet* pPuppet = pObject->CastToCPuppet();
	if (pPuppet)
		CancelAnyPathsFor(pPuppet, true);	// Remove from actor and raw path requests.

/*	VectorPuppets::iterator delItr=std::find(m_vWaitingToBeUpdated.begin(),m_vWaitingToBeUpdated.end(), pAIObject);
	while(delItr!=m_vWaitingToBeUpdated.end())
	{
		m_vWaitingToBeUpdated.erase( delItr );
		delItr=std::find(m_vWaitingToBeUpdated.begin(),m_vWaitingToBeUpdated.end(), pAIObject);
	}*/

/*	delItr=std::find(m_vAlreadyUpdated.begin(),m_vAlreadyUpdated.end(), pAIObject);
	while (delItr!=m_vAlreadyUpdated.end())
	{
		m_vAlreadyUpdated.erase( delItr );
		delItr=std::find(m_vAlreadyUpdated.begin(),m_vAlreadyUpdated.end(), pAIObject);
	}*/

	// check if this object owned any beacons and remove them if so
	if (!m_mapBeacons.empty())
	{
		BeaconMap::iterator bi,biend = m_mapBeacons.end();
		for (bi=m_mapBeacons.begin();bi!=biend;)
		{
			if ((bi->second).pOwner == pAIObject)
			{
				BeaconMap::iterator eraseme = bi;
				++bi;
				RemoveDummyObject((eraseme->second).pBeacon);
			}
			else
				++bi;
		}
	}

	m_lightManager.OnObjectRemoved(pObject);

	if (pPuppet)
	{
		for (unsigned i = 0; i < m_delayedExpAccessoryUpdates.size(); )
		{
			if (m_delayedExpAccessoryUpdates[i].pPuppet = pPuppet)
			{
				m_delayedExpAccessoryUpdates[i] = m_delayedExpAccessoryUpdates.back();
				m_delayedExpAccessoryUpdates.pop_back();
			}
			else
				++i;
		}

		m_disabledPuppetsSet.erase(pPuppet);
		m_enabledPuppetsSet.erase(pPuppet);
	}

	// finally release the object itself
	pObject->Release();
	m_nNumPuppets = m_Objects.count(AIOBJECT_PUPPET);
	m_nNumPuppets += m_Objects.count(AIOBJECT_VEHICLE);
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::CollisionEvent(const Vec3& pos, const Vec3& vel, float mass, float radius, float impulse, IEntity* pCollider, IEntity* pTarget)
{
	FUNCTION_PROFILER( m_pSystem, PROFILE_AI );

	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;

	// Too small object to be concerned with.
	if (mass < 0.3f || impulse < 0.01f)
		return;

	CAIActor* pColliderAI = pCollider ? CastToCAIActorSafe(pCollider->GetAI()) : 0;
	CAIActor* pTargetAI = pTarget ? CastToCAIActorSafe(pTarget->GetAI()) : 0;

	// Do not report AI collisions
	if (pColliderAI || pTargetAI)
		return;

	// Ignore player collisions.
//	if (pColliderAI && pColliderAI->GetType() == AIOBJECT_PLAYER)
//		return;

//	if (pColliderAI && pTargetAI && pColliderAI->IsHostile(pTargetAI))
//		return;

	if (m_cvDebugDrawCollisionEvents->GetIVal() > 0)
	{
		const char* pTargetName = "<unknown>";
		const char* pColliderName = "<unknown>";
		if (pTarget) pTargetName = pTarget->GetName();
		if (pCollider) pColliderName = pCollider->GetName();
		AILogComment("CAISystem::CollisionEvent %s with %s impulse=%f mass=%f speed=%f rad=%f", pColliderName, pTargetName, impulse, mass, vel.GetLength(), radius);
	}

	const float collisionEventTime = 7.0f;

	// Filter out recent collisions by the same object.
	if (pCollider)
	{
		EntityId colliderId = pCollider->GetId();
		for (unsigned i = 0, ni = m_collisionEvents.size(); i < ni; ++i)
		{
			SAICollisionEvent& ce = m_collisionEvents[i];
			if (ce.id == colliderId)
			{
				if (ce.t < 1.0f || Distance::Point_PointSq(pos, ce.pos) < sqr(2.0f))
				{
					if (!ce.signalled)
						ce.pos = pos;
					ce.t = 0;
					return;
				}
			}
		}
	}

	// Skip collisions that are close to explosions as long as the explosion spot is "hot".
	// update existing explosion if overlaps, or add new one
	bool affectedByExplosion = false;
	for (unsigned i = 0, ni = m_explosionSpots.size(); i < ni; ++i)
	{
		float	distSq = Distance::Point_PointSq(m_explosionSpots[i].pos, pos);
		if (distSq < sqr(m_explosionSpots[i].radius*2.5f))
		{
			affectedByExplosion = true;
			break;
		}
	}
	if (affectedByExplosion)
		return;

	CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());

	SAICollisionObjClassification type = AICO_LARGE;

	if (radius < 0.3f && mass < 5.0f)
		type = AICO_SMALL;
	else if (radius < 2.0f && mass < 100.0f)
		type = AICO_MEDIUM;
	else
		type = AICO_LARGE;

	bool thrownByPlayer = false;

	if (pCollider && pPlayer)
		thrownByPlayer = pPlayer->IsThrownByPlayer(pCollider->GetId());

	const float impactSpeed = vel.GetLength();

	const float reactionRadSize = clamp(mass / 250.0f, 0.0f, 1.0f);
	const float speedScale = clamp(impactSpeed / 25.0f, 0.0f, 1.0f);
	const float reactionRadius = (5.0f + reactionRadSize * 30.0f) * speedScale;

	float soundRadius = 10.0f + sqrtf(reactionRadSize) * 90.0f;
	if (thrownByPlayer)
		soundRadius *= 1.5f * clamp(speedScale*2, 0.0f, 1.0f); // * (0.5f * speedScale * 0.5f);
	else
		soundRadius *= speedScale;

	// Filter out and merge similar close by events
	float invReactionRadius = 1.0f / reactionRadius;
	for (unsigned i = 0, ni = m_collisionEvents.size(); i < ni; ++i)
	{
		SAICollisionEvent& ce = m_collisionEvents[i];
		if (ce.t < 2.0f)
		{
			if (Distance::Point_PointSq(pos, ce.pos) < sqr(3.0f))
			{
				ce.signalled = false;	// Allow to send signals again.
				ce.pos = pos;
				ce.t = 0.0f;
				ce.radius = max(ce.radius, radius);
				ce.affectRadius = max(ce.affectRadius, reactionRadius);
				ce.soundRadius = max(ce.soundRadius, soundRadius);
				ce.thrownByPlayer = ce.thrownByPlayer || thrownByPlayer;
				return;
			}
		}
	}

	m_collisionEvents.push_back(SAICollisionEvent(pCollider ? pCollider->GetId() : 0,
		pos, radius, reactionRadius, soundRadius, thrownByPlayer, type, collisionEventTime));
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::BreakEvent(const Vec3& pos, float breakSize, float radius, float impulse, IEntity* pCollider)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;

	// Too small object to be concerned with.
//	if (mass < 0.3f || impulse < 0.01f)
//		return;

	if (m_cvDebugDrawCollisionEvents->GetIVal() > 0)
	{
		const char* pColliderName = "<unknown>";
		if (pCollider) pColliderName = pCollider->GetName();
		AILogComment("CAISystem::BreakEvent %s breakSize=%f radius=%f impulse=%f", pColliderName, breakSize, radius, impulse);
	}

	const float collisionEventTime = 7.0f;

	// Filter out recent collisions by the same object.
	if (pCollider)
	{
		EntityId colliderId = pCollider->GetId();
		for (unsigned i = 0, ni = m_collisionEvents.size(); i < ni; ++i)
		{
			SAICollisionEvent& ce = m_collisionEvents[i];
			if (ce.id == colliderId)
			{
				if (ce.t < 1.0f || Distance::Point_PointSq(pos, ce.pos) < sqr(2.0f))
				{
					if (!ce.signalled)
						ce.pos = pos;
					ce.t = 0;
					return;
				}
			}
		}
	}

	// Skip collisions that are close to explosions as long as the explosion spot is "hot".
	// update existing explosion if overlaps, or add new one
	bool affectedByExplosion = false;
	for (unsigned i = 0, ni = m_explosionSpots.size(); i < ni; ++i)
	{
		float	distSq = Distance::Point_PointSq(m_explosionSpots[i].pos, pos);
		if (distSq < sqr(m_explosionSpots[i].radius*2.5f))
		{
			affectedByExplosion = true;
			break;
		}
	}
	if (affectedByExplosion)
		return;

	CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());

	SAICollisionObjClassification type = AICO_LARGE;
	if (breakSize < 0.5f)
		type = AICO_SMALL;
	else if (breakSize < 2.0f)
		type = AICO_MEDIUM;
	else
		type = AICO_LARGE;


	const float reactionRadius = breakSize * 2.0f;
	const float soundRadius = 10.0f + sqrtf(clamp(impulse/800.0f, 0.0f, 1.0f)) * 60.0f;

	// Filter out and merge similar close by events
	float invReactionRadius = 1.0f / reactionRadius;
	for (unsigned i = 0, ni = m_collisionEvents.size(); i < ni; ++i)
	{
		SAICollisionEvent& ce = m_collisionEvents[i];
		if (ce.t < 2.0f)
		{
			if (Distance::Point_PointSq(pos, ce.pos) < sqr(3.0f))
			{
				if (!ce.signalled)
					ce.pos = pos;
				ce.t = 0.0f;
				ce.radius = max(ce.radius, radius);
				ce.affectRadius = max(ce.affectRadius, reactionRadius);
				ce.soundRadius = max(ce.soundRadius, soundRadius);
				return;
			}
		}
	}

	m_collisionEvents.push_back(SAICollisionEvent(pCollider ? pCollider->GetId() : 0,
		pos, radius, reactionRadius, soundRadius, false, type, collisionEventTime));
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::IgnoreCollisionEventsFrom(EntityId ent, float time)
{
	MapIgnoreCollisionsFrom::iterator it = m_ignoreCollisionEventsFrom.find(ent);
	if (it != m_ignoreCollisionEventsFrom.end())
		it->second = max(it->second, time);
	else
		m_ignoreCollisionEventsFrom[ent] = time;
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::SoundEvent(const Vec3& pos, float radius, EAISoundEventType type, IAIObject *pSender)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;

	if (m_cvSoundPerception && !m_cvSoundPerception->GetIVal())
		return;	// sound perception of enemies is off

	// perform suppression of the sound from any nearby suppressors
	SupressSoundEvent(pos,radius);

	CAIActor* pSenderActor = CastToCAIActorSafe(pSender);

	for (unsigned i = 0, ni = m_enabledPuppetsSet.size(); i < ni; ++i)
	{
		CPuppet* pReceiverPuppet = m_enabledPuppetsSet[i];

		if (((IAIObject*)pReceiverPuppet) == pSender)
			continue;	// do not report sounds to the guy who created the sound

		// no explosion sounds for same group
		if (type == AISE_EXPLOSION && pSenderActor && !pSenderActor->IsHostile(pReceiverPuppet) &&
				pReceiverPuppet->GetGroupId() == pSenderActor->GetGroupId())
			continue;

		// no vehicle sounds for same species - destructs convoys
		if (pSenderActor && !pSenderActor->IsHostile(pReceiverPuppet) && pSenderActor->GetType() == AIOBJECT_VEHICLE)
			continue;

		if (pReceiverPuppet->IsPointInAudibleRange(pos, radius) && IsSoundHearable(pReceiverPuppet, pos, radius))
		{
			SAIEVENT event;
			event.pSeen = pSender;
/*			if (pSender && type == AISE_WEAPON)
				event.vPosition = pSender->GetPos();
			else*/
				event.vPosition =  pos;
			event.fThreat = radius;
			event.nType = (int)type;
			pReceiverPuppet->Event(AIEVENT_ONSOUNDEVENT, &event);
		}
	}

	if (m_cvDebugDrawSoundEvents->GetIVal() > 0)
	{
		m_soundEvents.push_back(SAISoundEvent(pSender ? pSender->GetEntityID() : 0, pos, radius, type, 3.0f));
	}

	float threat = 1.0f;
	if (type == AISE_WEAPON)
		threat = 5.0f;
	else if (type == AISE_EXPLOSION)
		threat = 10.0f;
	else if (type == AISE_MOVEMENT)
		threat = 2.0f;

	NotifyAIEventListeners(AIEVENT_SOUND, pos, radius, threat, pSender ? pSender->GetEntityID() : 0);
}


// Sends a signal using the desired filter to the desired agents
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::SendSignal(unsigned char cFilter, int nSignalId,const char *szText, IAIObject *pSenderObject, IAISignalExtraData* pData)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	// This deletes the passed pData parameter if it wasn't set to NULL
	struct DeleteBeforeReturning
	{
		IAISignalExtraData** _p;
		DeleteBeforeReturning( IAISignalExtraData** p ) : _p(p) {}
		~DeleteBeforeReturning()
		{ 
			if ( *_p )
				delete (AISignalExtraData*)*_p;
		}
	} autoDelete( &pData );

	CAIActor* pSender = pSenderObject->CastToCAIActor();
	if(!pSender)
		return;

	//filippo: can be that sender is null, for example when you send this singal in multiplayer.
	if (pSender==NULL)
		return;

	float fRange = pSender->GetParameters().m_fCommRange;
	fRange*=pSender->GetParameters().m_fCommRange;
	Vec3 pos = pSender->GetPos();
	IEntity	*pSenderEntity(pSender->GetEntity());

	//AILogComment("Enemy %s sending signal %s",pSender->GetName(),szText);

	switch (cFilter)
	{

	case SIGNALFILTER_SENDER:
		{
			pSender->SetSignal(nSignalId, szText, pSenderEntity, pData);
			pData = NULL;
			break;
		}
	case SIGNALFILTER_READIBILITYAT:
	case SIGNALFILTER_READIBILITY:
	case SIGNALFILTER_READIBILITYRESPONSE:
		{
			// Check if someone in the group is already playing this readability, and prevent it. Skip responses.
			// Priorities over 100 are always played.
			bool highPriority = pData && pData->iValue >= 100;
			if (cFilter != SIGNALFILTER_READIBILITYRESPONSE && !highPriority)
			{
				// Get the duration of blocking from the readability manager.
				IPuppetProxy *pPuppetProxy =  0;
				if(pSender->GetProxy() && pSender->GetProxy()->QueryProxy(AIPROXY_PUPPET, (void**)&pPuppetProxy))
				{
					int groupId = pSender->GetGroupId();
					IAIGroup* pGroup = GetIAIGroup(groupId);
					float	t;
					int	id;
					pPuppetProxy->GetReadabilityBlockingParams(szText, t, id);
					if(pGroup && !pGroup->RequestReadabilitySound(id, t))
					{
						string	name(szText);
						name += "[blocked]";
						Record(pSender, IAIRecordable::E_SIGNALRECIEVEDAUX, name.c_str());	
						IAIRecordable::RecorderEventData recorderEventData(name.c_str());
						pSender->RecordEvent(IAIRecordable::E_SIGNALRECIEVEDAUX, &recorderEventData);
						return;
					}
				}
			}


			unsigned int groupid = pSender->GetGroupId();
			AuxSignalDesc asd;
			asd.fTimeout = 5.f;//3.0f
			asd.strMessage = string(szText);
			m_mapAuxSignalsFired.insert(MapSignalStrings::iterator::value_type(groupid,asd));

			pSender->m_State.nAuxSignal = cFilter;
			pSender->m_State.szAuxSignalText = szText;
			if( pData )
			{
				pSender->m_State.nAuxPriority = pData->iValue;
				pSender->m_State.fAuxDelay = pData->fValue;
			}
			else
			{
				pSender->m_State.nAuxPriority = 0;
				pSender->m_State.fAuxDelay = 0;
			}
			Record(pSender, IAIRecordable::E_SIGNALRECIEVEDAUX, szText);	
			IAIRecordable::RecorderEventData recorderEventData(szText);
			pSender->RecordEvent(IAIRecordable::E_SIGNALRECIEVEDAUX, &recorderEventData);

			break;
		}
		case SIGNALFILTER_HALFOFGROUP:
			{
				int groupid = pSender->GetGroupId();
				AIObjects::iterator ai;
		
				if ( (ai=m_mapGroups.find(groupid)) != m_mapGroups.end() )
				{
					int groupmembers=m_mapGroups.count(groupid);
					groupmembers/=2;

					for (;ai!=m_mapGroups.end();)
					{
						if ( (ai->first != groupid) || (!groupmembers--) )
							break;
						CAIActor* pReciever = ai->second->CastToCAIActor();
						if (ai->second != pSenderObject && pReciever)
						{
							pReciever->SetSignal(nSignalId,szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
						}
						else
							++groupmembers;	// dont take into account sender
						++ai;
					}
					// don't delete pData!!! - it will be deleted at the end of this function
				}
			}
			break;
		case SIGNALFILTER_NEARESTGROUP:
			{
				int groupid = pSender->GetGroupId();
				AIObjects::iterator ai;
				CAIActor *pNearest= 0;
				float mindist = 2000;
				if ( (ai=m_mapGroups.find(groupid)) != m_mapGroups.end() )
				{
					for (;ai!=m_mapGroups.end();)
					{
						if (ai->first != groupid)
							break;
						if (ai->second != pSenderObject)
						{
							float dist = (ai->second->GetPos() - pSender->GetPos()).GetLength();
							if (dist < mindist)
							{
								pNearest = ai->second->CastToCAIActor();
								if(pNearest)
									mindist = (ai->second->GetPos() - pSender->GetPos()).GetLength();
							}
						}
						++ai;
					}

					if (pNearest)
					{
						pNearest->SetSignal(nSignalId,szText,pSenderEntity,pData);
						pData = NULL;
					}
				}
			}
			break;
			 
		case SIGNALFILTER_NEARESTINCOMM:
			{
				int groupid = pSender->GetGroupId();
				AIObjects::iterator ai;
				CAIActor *pNearest= 0;
				float mindist = pSender->GetParameters().m_fCommRange;
				if ( (ai=m_mapGroups.find(groupid)) != m_mapGroups.end() )
				{
					for (;ai!=m_mapGroups.end();)
					{
						if (ai->first != groupid)
							break;
						if (ai->second != pSenderObject)
						{
							float dist = (ai->second->GetPos() - pSender->GetPos()).GetLength();
							if (dist < mindist)
							{
								pNearest = ai->second->CastToCAIActor();
								if(pNearest)
									mindist = (ai->second->GetPos() - pSender->GetPos()).GetLength();
							}
						}
						++ai;
					}

					if (pNearest)
					{
						pNearest->SetSignal(nSignalId,szText,pSenderEntity,pData);
						pData = NULL;
					}
				}
			}
			break;
		case SIGNALFILTER_NEARESTINCOMM_LOOKING:
			{
				int groupid = pSender->GetGroupId();
				AIObjects::iterator ai;
				CAIActor *pSenderActor = pSenderObject->CastToCAIActor();
				CAIActor *pNearest = 0;
				float mindist = pSender->GetParameters().m_fCommRange;
				float	closestDist2(std::numeric_limits<float>::max());
				if ( pSenderActor && (ai=m_mapGroups.find(groupid)) != m_mapGroups.end() )
				{
					for (;ai!=m_mapGroups.end();)
					{
						if (ai->first != groupid)
							break;
						if (ai->second != pSenderObject)
						{
							CPuppet *pCandidatePuppet=ai->second->CastToCPuppet();
							float dist = (ai->second->GetPos() - pSender->GetPos()).GetLength();
							if (pCandidatePuppet && dist < mindist)
								if (CheckVisibilityToBody(pCandidatePuppet, pSenderActor, closestDist2))
									pNearest = pCandidatePuppet;
						}
						++ai;
					}
					if (pNearest)
					{
						pNearest->SetSignal(nSignalId,szText,pSenderEntity,pData);
						pData = NULL;
					}
				}
			}
			break;
		case SIGNALFILTER_NEARESTINCOMM_SPECIES:
			{
				int species = pSender->GetParameters().m_nSpecies;
				AIObjects::iterator ai;
				CAIActor *pNearest= 0;
				float mindist = pSender->GetParameters().m_fCommRange;
				if ( (ai=m_mapSpecies.find(species)) != m_mapSpecies.end() )
				{
					for (;ai!=m_mapSpecies.end();)
					{
						if (ai->first != species)
							break;
						if (ai->second != pSenderObject && ai->second->IsEnabled())
						{
							float dist = (ai->second->GetPos() - pSender->GetPos()).GetLength();
							if (dist < mindist)
							{
								pNearest = ai->second->CastToCAIActor();
								if(pNearest)
									mindist = (ai->second->GetPos() - pSender->GetPos()).GetLength();
							}
						}
						++ai;
					}

					if (pNearest)
					{
						pNearest->SetSignal(nSignalId,szText,pSenderEntity,pData);
						pData = NULL;
					}
				}
			}
			break;
		case SIGNALFILTER_SUPERGROUP:
			{
				int groupid = pSender->GetGroupId();
				AIObjects::iterator ai;
				if ( (ai=m_mapGroups.find(groupid)) != m_mapGroups.end() )
				{
					for (;ai!=m_mapGroups.end();)
					{
						if (ai->first != groupid)
							break;
						CAIActor* pReciever = ai->second->CastToCAIActor();
						if(pReciever)
							pReciever->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
						++ai;
					}
					// don't delete pData!!! - it will be deleted at the end of this function
				}
				// send message also to player
				CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
				if(pPlayer)
				{
					if(pSender->GetParameters().m_nGroup == pPlayer->GetParameters().m_nGroup )
						pPlayer->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
				}
			}
			break;
		case SIGNALFILTER_SUPERSPECIES:
			{
				int speciesid = pSender->GetParameters().m_nSpecies;
				AIObjects::iterator ai;

				if ( (ai=m_mapSpecies.find(speciesid)) != m_mapSpecies.end() )
				{
					for (;ai!=m_mapSpecies.end();)
					{
						if (ai->first != speciesid)
							break;
						CAIActor* pReciever = ai->second->CastToCAIActor();
						if(pReciever)
							pReciever->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
						++ai;
					}
					// don't delete pData!!! - it will be deleted at the end of this function
				}
				// send message also to player
				CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
				if(pPlayer)
				{
					pPlayer->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
				}
			}
			break;

		case SIGNALFILTER_GROUPONLY:
		case SIGNALFILTER_GROUPONLY_EXCEPT:
			{
				int groupid = pSender->GetGroupId();
				AIObjects::iterator ai;
				if ( (ai=m_mapGroups.find(groupid)) != m_mapGroups.end() )
				{
					for (;ai!=m_mapGroups.end();)
					{
						if (ai->first != groupid)
							break;

						CAIActor* pReciever = ai->second->CastToCAIActor();
						if (pReciever && Distance::Point_PointSq(pReciever->GetPos(),pos) < fRange && !(cFilter==SIGNALFILTER_GROUPONLY_EXCEPT && pReciever==pSender) )
						{
							pReciever->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
						}

						++ai;
					}
					// don't delete pData!!! - it will be deleted at the end of this function
				}
//				else
//					AIWarning("Group ID %d NOT FOUND",groupid);

				// send message also to player
				CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
				if(pPlayer)
				{
					if(pSender->GetParameters().m_nGroup == pPlayer->GetParameters().m_nGroup && Distance::Point_PointSq(pPlayer->GetPos(),pos) < fRange && !(cFilter==SIGNALFILTER_GROUPONLY_EXCEPT && pPlayer==pSender) )
						pPlayer->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
				}
			}
			break;
		case SIGNALFILTER_SPECIESONLY:
			{
				int speciesid = pSender->GetParameters().m_nSpecies;
				AIObjects::iterator ai;

				if ( (ai=m_mapSpecies.find(speciesid)) != m_mapSpecies.end() )
				{
					for (;ai!=m_mapSpecies.end();)
					{
						if (ai->first != speciesid)
							break;
						CAIActor* pReciever = ai->second->CastToCAIActor();
						if(pReciever)
						{
							Vec3 mypos = pReciever->GetPos();
							mypos -=pos;

							if (mypos.GetLengthSquared() < fRange)
								pReciever->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
						}
						++ai;
					}
					// don't delete pData!!! - it will be deleted at the end of this function
				}
				// send message also to player
				CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
				if(pPlayer)
				{
					if(pSender->GetParameters().m_nSpecies == pPlayer->GetParameters().m_nSpecies && Distance::Point_PointSq(pPlayer->GetPos(),pos) < fRange  )
						pPlayer->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
				}
			}
			break;
		case SIGNALFILTER_ANYONEINCOMM:
		case SIGNALFILTER_ANYONEINCOMM_EXCEPT:
			{
				// send to puppets and aware objects in the communications range of the sender

				// Added the evaluation of AIOBJECT_VEHICLE so that vehicles can get signals.
				// 20/12/05 Tetsuji
				AIObjects::iterator ai;

				std::vector<int> objectKinds;
				objectKinds.push_back(AIOBJECT_PUPPET);
				objectKinds.push_back(AIOBJECT_VEHICLE);

				std::vector<int>::iterator ki,ki2,kiend=objectKinds.end();

				for (ki=objectKinds.begin();ki!=kiend;++ki)
				{
					int objectKind =*ki;
					// first look for all the puppets
					if ( (ai=m_Objects.find(objectKind)) != m_Objects.end() )
					{
						for (;ai!=m_Objects.end();)
						{
							for (ki2=objectKinds.begin();ki2!=kiend;++ki2)
								if (ai->first==*ki)
								{
									CAIActor* pReciever = ai->second->CastToCAIActor();
									if (pReciever && Distance::Point_PointSq(pReciever->GetPos(),pos) < fRange && !(cFilter==SIGNALFILTER_ANYONEINCOMM_EXCEPT && pReciever==pSender) )
										pReciever->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
								}
							++ai;
						}
					}
					// don't delete pData!!! - it will be deleted at the end of this function
				}

				// now look for aware objects
				if ( (ai=m_Objects.find(AIOBJECT_AWARE)) != m_Objects.end() )
				{
					for (;ai!=m_Objects.end();)
					{
						if (ai->first != AIOBJECT_AWARE)
							break;
						CAIActor* pReciever = ai->second->CastToCAIActor();
						if(pReciever)
						{
							Vec3 mypos = pReciever->GetPos();
							mypos -=pos;
							if (mypos.GetLengthSquared() < fRange) 
								pReciever->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
						}
						++ai;
					}
				}
				// send message also to player

				CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
				if(pPlayer)
				{
					if( Distance::Point_PointSq(pPlayer->GetPos(),pos) < fRange && !(cFilter==SIGNALFILTER_ANYONEINCOMM_EXCEPT && pPlayer==pSender) )
						pPlayer->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
				}
				// don't delete pData!!! - it will be deleted at the end of this function
			}
			break;
		case SIGNALFILTER_LEADER:
		case SIGNALFILTER_LEADERENTITY:
			{
				int groupid = pSender->GetGroupId();
				AIObjects::iterator ai;
				if ( (ai=m_mapGroups.find(groupid)) != m_mapGroups.end() )
				{
					for (;ai!=m_mapGroups.end();)
					{
						if (ai->first != groupid)
							break;
						CLeader* pLeader = ai->second->CastToCLeader();
						if(pLeader)
						{
							CAIObject* pRecipientObj = (cFilter ==SIGNALFILTER_LEADER ? pLeader : (CAIObject*)pLeader->GetAssociation());
							if(pRecipientObj)
							{
								CAIActor* pRecipient = pRecipientObj->CastToCAIActor();
								if(pRecipient)
									pRecipient->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
							}
							break;
						}
						++ai;
					}
					// don't delete pData!!! - it will be deleted at the end of this function
				}
			}
			break;

	case SIGNALFILTER_FORMATION:
	case SIGNALFILTER_FORMATION_EXCEPT:
		{
			for(FormationMap::iterator fi(m_mapActiveFormations.begin()); fi!=m_mapActiveFormations.end(); ++fi)
			{
				CFormation *pFormation = fi->second;
				if(pFormation && pFormation->GetFormationPoint(pSender))
				{
					int s = pFormation->GetSize();
					for(int i=0; i<s; i++)
					{
						CAIObject* pMember = pFormation->GetPointOwner(i);
						if(pMember && (cFilter==SIGNALFILTER_FORMATION || pMember != pSender))
						{
							CAIActor* pRecipient = pMember->CastToCAIActor();
							if(pRecipient)
								pRecipient->SetSignal(nSignalId, szText,pSenderEntity, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
						}
					}
					break;
				}
			}
			break;
		}
	}
}

// adds an object to a group
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::AddToGroup(CAIActor * pObject, int nGroupId)
{
	if(!pObject->CastToCPipeUser() && !pObject->CastToCLeader() && !pObject->CastToCAIPlayer())
		return;

	if (nGroupId >= 0)
		pObject->SetGroupId(nGroupId);
	else
		nGroupId = pObject->GetGroupId();

	AIObjects::iterator gi;
	bool	found = false;
	if((gi=m_mapGroups.find(nGroupId)) != m_mapGroups.end())
	{
		// check whether it was added before
		while ( (gi!=m_mapGroups.end()) &&  (gi->first == nGroupId) )
		{
			if (gi->second == pObject)
			{
				found = true;
				break;
			}
			++gi;
		}
	}

	// in the map - do nothing
	if (found)
	{
		// Update the group count status and create the group object if necessary.
		UpdateGroupStatus(nGroupId);
		return;
	}

	m_mapGroups.insert(AIObjects::iterator::value_type(nGroupId,pObject));

	// Update the group count status and create the group object if necessary.
	UpdateGroupStatus(nGroupId);

	//group leaders related stuff
	AIGroupMap::iterator	it = m_mapAIGroups.find(nGroupId);
	if(it != m_mapAIGroups.end())
	{
		if (CLeader* pLeader = pObject->CastToCLeader())
		{
			//it's a team leader - add him to leaders map
			pLeader->SetAIGroup(it->second);
			it->second->SetLeader(pLeader);
		}
		else if (CPipeUser* pPipeuser = pObject->CastToCPipeUser())
		{
			// it's a soldier - find the leader of hi's team 
			it->second->AddMember(pObject);
		}
	}
}

// removes specified object from group
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::RemoveFromGroup(int nGroupID, CAIObject * pObject)
{
	AIObjects::iterator gi;

	for (gi = m_mapGroups.find(nGroupID);gi!=m_mapGroups.end();)
	{
		if (gi->first!=nGroupID)
			break;

		if ( gi->second == pObject)
		{
			m_mapGroups.erase(gi);
			break;
		}
		++gi;
	}

	if (m_mapAIGroups.find(nGroupID) == m_mapAIGroups.end())
		return;

	UpdateGroupStatus(nGroupID);

	AIGroupMap::iterator	it = m_mapAIGroups.find(nGroupID);
	if(it != m_mapAIGroups.end())
	{
		if (pObject->CastToCLeader())
			it->second->SetLeader(0);
		else
			it->second->RemoveMember(pObject->CastToCAIActor());
	}
}

void CAISystem::UpdateGroupStatus(int nGroupID)
{
	AIGroupMap::iterator	it = m_mapAIGroups.find(nGroupID);
	if (it == m_mapAIGroups.end())
	{
		CAIGroup*	pNewGroup = new CAIGroup(nGroupID);
		m_mapAIGroups.insert(AIGroupMap::iterator::value_type(nGroupID, pNewGroup));
		it = m_mapAIGroups.find(nGroupID);
	}

	CAIGroup* pGroup = it->second;
	AIAssert(pGroup);
	pGroup->UpdateGroupCountStatus();
}

// adds an object to a species
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::AddToSpecies(CAIObject * pObject, unsigned short nSpecies)
{
	AIObjects::iterator gi;

	if ( (gi=m_mapSpecies.find(nSpecies)) != m_mapSpecies.end())
		// check whether it was added before
		while ((gi!=m_mapSpecies.end()) && (gi->first == nSpecies) )
		{
			if (gi->second == pObject)
				return;
			++gi;
		}

	// make sure it is not in with another species already
	AIObjects::iterator	oi(m_mapSpecies.begin());
	for (;oi!=m_mapSpecies.end();++oi)
		if (oi->second == pObject)
		{
			m_mapSpecies.erase(oi);
			break;
		}

	// it has not been added, add it now
	m_mapSpecies.insert(AIObjects::iterator::value_type(nSpecies,pObject));
}

// creates a formation and associates it with a group of agents
//
//-----------------------------------------------------------------------------------------------------------
CFormation *CAISystem::CreateFormation(CAIObject* pOwner, const char * szFormationName, Vec3 vTargetPos)
{
	FormationMap::iterator fi;
	fi = m_mapActiveFormations.find(pOwner);
	if (fi==m_mapActiveFormations.end())
	{
		FormationDescriptorMap::iterator di;
		di = m_mapFormationDescriptors.find(szFormationName);
		if (di!=m_mapFormationDescriptors.end())
		{
			CFormation *pFormation = new CFormation();
			pFormation->Create(di->second,pOwner,vTargetPos);
			m_mapActiveFormations.insert(FormationMap::iterator::value_type(pOwner,pFormation));
			return pFormation;
		}
	}
	else
		return (fi->second);
	return 0;
}

// change the current formation for the given group
//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::ChangeFormation(IAIObject* pOwner, const char * szFormationName,float fScale, bool force)
{
/*	FormationMap::iterator fi;
	fi = m_mapActiveFormations.find(nGroupID);
	if (fi==m_mapActiveFormations.end())
	*/
	if(pOwner)
	{
		CFormation *pFormation = ((CAIObject*)pOwner)->m_pFormation;

		if (!pFormation)
		{
			if (force)
				return pOwner->CreateFormation(szFormationName);
			else
				return false;
		}
		else
		{
			FormationDescriptorMap::iterator di;
			di = m_mapFormationDescriptors.find(szFormationName);
			if (di!=m_mapFormationDescriptors.end())
			{
				//CFormation *pFormation = fi->second;
				pFormation->Change(di->second,fScale);
				return true;
			}
		}
	}
	return false;
}

// scale the current formation for the given group
//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::ScaleFormation(IAIObject* pOwner, float fScale)
{
	FormationMap::iterator fi;
	fi = m_mapActiveFormations.find(static_cast<CAIObject*>(pOwner));
	if (fi != m_mapActiveFormations.end())
	{
		CFormation *pFormation = fi->second;
		if(pFormation)
		{
			pFormation->SetScale(fScale);
			return true;
		}
	}
	return false;
}

// scale the current formation for the given group
//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::SetFormationUpdate(IAIObject* pOwner, bool bUpdate)
{
	FormationMap::iterator fi;
	fi = m_mapActiveFormations.find(static_cast<CAIObject*>(pOwner));
	if (fi!=m_mapActiveFormations.end())
	{
		CFormation *pFormation = fi->second;
		if(pFormation)
		{
			pFormation->SetUpdate(bUpdate);
			return true;
		}
	}
	return false;
}

// check if puppet and vehicle are in the same formation
//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::SameFormation(const CPuppet* pHuman, const CAIVehicle* pVehicle)
{
	if(pHuman->IsHostile(pVehicle))
		return false;
	for(FormationMap::iterator fi(m_mapActiveFormations.begin()); fi!=m_mapActiveFormations.end(); ++fi)
	{
		CFormation *pFormation = fi->second;
		if(pFormation->GetFormationPoint(pHuman))
		{
			CAIObject* pFormationOwner(pFormation->GetPointOwner(0));
			if(!pFormationOwner)
				return false;
			if(pFormationOwner == pVehicle->GetAttentionTarget())
				return true;
			return false;
		}
	}
	return false;
}
/*
bool CAISystem::FreeFormationPoint(CAIObject * pAIObject)
{
	if(!pAIObject)
		return false;
	bool bFormationFound = false;

	for(FormationMap::iterator fi(m_mapActiveFormations.begin()); fi!=m_mapActiveFormations.end(); ++fi)
	{
		CFormation *pFormation = fi->second;
		if(pFormation->GetFormationPoint(pAIObject) )
		{
			if(pAIObject == pFormation->GetPointOwner(0))
			{
				// TO DO: use this instead: ReleaseFormation(pAIObject,true);
				pAIObject->ReleaseFormation();
			}
			else
			{
				pFormation->FreeFormationPoint(pAIObject);
				bFormationFound = true;
			}
		}
	}
	return bFormationFound;
}
*/

// retrieves the next available formation point if a formation exists for the group of the requester
//
//-----------------------------------------------------------------------------------------------------------
/*
CAIObject * CAISystem::GetNewFormationPoint(CAIObject * pRequester, int index)
{
	CLeader* pLeader = GetLeader(pRequester);
	if(pLeader && pLeader->m_pFormationOwner)
	{ 
		CFormation* pFormation =pLeader->m_pFormationOwner->m_pFormation;
		if(pFormation)
			return pFormation->GetNewFormationPoint((CAIObject *)pRequester, index);
	}
	
	return NULL;
}



// retrieves the next available formation point if a formation exists for the group of the requester
//
//-----------------------------------------------------------------------------------------------------------
CAIObject * CAISystem::GetFormationPointSight(CPipeUser * pRequester)
{
	CLeader* pLeader = GetLeader(pRequester);
	if(pLeader && pLeader->m_pFormationOwner)
	{ 
		CFormation* pFormation =pLeader->m_pFormationOwner->m_pFormation;
		if(pFormation)
			return pFormation->GetFormationDummyTarget((CAIObject *)pRequester);
	}
	return NULL;
}
*/

//====================================================================
// StoppingAIUpdates
//====================================================================
void CAISystem::StoppingAIUpdates()
{
  m_pGraph->RestoreAllNavigation();
}


// Resets all agent states to initial
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::Reset(IAISystem::EResetReason reason)
{
	if(!IsEnabled())
		return;
  AILogEvent("CAISystem::Reset %d", reason);
	m_bUpdateSmartObjects = false;

	// Signal the AI recorder system (Editor mode only)
	if (GetISystem()->IsEditor())
	{
		switch(reason)
		{
		case RESET_INTERNAL:
			// Signals basic Init call on AI system
		case RESET_INTERNAL_LOAD:
			// Called by the AI system itself
			m_Recorder.Stop();
			m_Recorder.Reset();
			break;
		case RESET_EXIT_GAME:
			m_Recorder.Stop();
			break;
		case RESET_ENTER_GAME:
			// Called on entering game mode
			m_Recorder.Reset();
			m_Recorder.Start();
			m_validationErrorMarkers.clear();
			break;
		}
	}

	//reset obstructing spheres 
	m_ObstructSpheres.clear();

	// Reset Interest System
	GetCentralInterestManager()->Reset();

	m_lightManager.Reset();

	// reset the groups.
	for(AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
		it->second->Reset();

	m_CurrentGlobalAlertness = -1; // to force initial change to 0
	for (int i = 0; i < 4; i++)
		m_AlertnessCounters[i] = 0;

	if (!m_bInitialized)
		return;

	Record(NULL, IAIRecordable::E_RESET, NULL);

	if ( m_pSmartObjects )
	{
		m_pActionManager->Reset();

		// If CAISystem::Reset() was called from CAISystem::Shutdown() IEntitySystem is already deleted
		if ( gEnv->pEntitySystem )
		{
			if ( reason == IAISystem::RESET_ENTER_GAME )
			{
				m_pSmartObjects->SoftReset();
			}
			if ( reason == IAISystem::RESET_INTERNAL_LOAD )
			{
				ReloadSmartObjectRules();
			}
		}
	}
	else
	{
		// If CAISystem::Reset() was called from CAISystem::Shutdown() IEntitySystem is already deleted
		if ( gEnv->pEntitySystem )
			InitSmartObjects();
	}

	if (m_Objects.empty())
		return;

	m_mapDEBUGTiming.clear();

	m_pendingBrokenRegions.m_regions.clear();
	m_brokenRegions.m_regions.clear();
/*
	if (!m_mapActiveFormations.empty()) //Luciano - Moved before resetting of AIObjects because player's CLEader reset
	{									//creates a formation
		FormationMap::iterator fi;
		for (fi=m_mapActiveFormations.begin();fi!=m_mapActiveFormations.end();++fi)
			fi->second->Reset();
	}
*/
	if (!m_mapActiveFormations.empty())
	{
		FormationMap::iterator fi;
		for (fi=m_mapActiveFormations.begin();fi!=m_mapActiveFormations.end();++fi)
		{
			CFormation* pFormation = fi->second;
			if(pFormation)
			{
				//pFormation->ReleasePoints();
				delete pFormation;
			}
		}
		m_mapActiveFormations.clear();
	}

	AIObjects::iterator ai;

	EObjectResetType objectResetType = AIOBJRESET_INIT;
	switch (reason)
	{
	case RESET_EXIT_GAME:
		objectResetType = AIOBJRESET_SHUTDOWN;
		break;
	case RESET_INTERNAL:
	case RESET_INTERNAL_LOAD:
	case RESET_ENTER_GAME:
	default:
		objectResetType = AIOBJRESET_INIT;
		break;
	}

	for (ai=m_Objects.begin();ai!=m_Objects.end();++ai)
	{
		(ai->second)->Reset(objectResetType);

		// Reset the AI recordable stuff when entering game mode.
		if(reason == IAISystem::RESET_ENTER_GAME)
		{
			CPipeUser* pPipeUser = ai->second->CastToCPipeUser();
			if (pPipeUser)
			{
				if(pPipeUser->m_pMyRecord)
					pPipeUser->m_pMyRecord->ResetStreams(GetFrameStartTime());
			}
		}
	}

	m_explosionSpots.clear();
	m_deadBodies.clear();
//	m_vWaitingToBeUpdated.clear();
//	m_vAlreadyUpdated.clear();

  ClearAIObjectIteratorPools();

	while (!m_lstPathQueue.empty())
	{
		PathFindRequest *pReq = m_lstPathQueue.front();
		m_lstPathQueue.pop_front();
		delete pReq;
	}

	if (m_pCurrentRequest)
	{
		delete m_pCurrentRequest;
		m_pCurrentRequest = 0;
	}

	m_nPathfinderResult = PATHFINDER_POPNEWREQUEST;
	m_pGraph->ClearMarks();
	m_pGraph->ClearTags();
	m_pGraph->Reset();

  m_isPointInForbiddenRegionQueue.Clear();
  m_isPointOnForbiddenEdgeQueue.Clear();

  GetTriangularNavRegion(m_pGraph)->Reset(reason);
  GetTriangularNavRegion(m_pHideGraph)->Reset(reason);
  GetVolumeNavRegion()->Reset(reason);
  GetWaypoint3DSurfaceNavRegion()->Reset(reason);
  GetWaypointHumanNavRegion()->Reset(reason);
  GetFlightNavRegion()->Reset(reason);
  GetRoadNavRegion()->Reset(reason);
  GetSmartObjectsNavRegion()->Reset(reason);
  CPathObstacles::Reset();

	m_dynHideObjectManager.Reset();

  for (ExtraLinkCostShapeMap::iterator it = m_mapExtraLinkCosts.begin() ; it != m_mapExtraLinkCosts.end() ; ++it)
  {
    const string &name = it->first;
    SExtraLinkCostShape &shape = it->second;
    shape.costFactor = shape.origCostFactor;
  }
/*
	if (!m_mapActiveFormations.empty())
	{
		FormationMap::iterator fi;
		for (fi=m_mapActiveFormations.begin();fi!=m_mapActiveFormations.end();++fi)
				delete (fi->second);
	}
	m_mapActiveFormations.clear();
*/

	m_mapAuxSignalsFired.clear();

	if (!m_mapBeacons.empty())
	{
		BeaconMap::iterator iend = m_mapBeacons.end();
		BeaconMap::iterator binext;
		for (BeaconMap::iterator bi=m_mapBeacons.begin();bi!=iend;)
		{
			binext = bi;
			++binext;
			RemoveDummyObject((bi->second).pBeacon);
			bi = binext;
		}
		m_mapBeacons.clear();
	}

	// enable all nav modifyers
	SpecialAreaMap::iterator si = m_mapSpecialAreas.begin();
	while (si!=m_mapSpecialAreas.end())
	{
		(si->second).bAltered = false;
		++si;
	}

	if(m_pVehicleFinder)
		m_pVehicleFinder->Reset();

	SetAIObjects::iterator iNext;
	for(SetAIObjects::iterator iDummy(m_setDummies.begin()); iDummy!=m_setDummies.end(); iDummy = iNext)
	{
		iNext = iDummy; ++iNext;
		CAIObject* pObject = *iDummy;
		IAIObject::ESubTypes stype = pObject->GetSubType();
		if(stype != CAIObject::STP_REFPOINT && stype != CAIObject::STP_LOOKAT)
    {
      AIWarning("CAISystem::Reset removing dummy %s (%p) - should already be deleted", pObject->GetName(), pObject);
			m_setDummies.erase(iDummy);
    }
	}

	// Reset the recorded trajectory.
	m_lastStatsTargetTrajectoryPoint.Set(0,0,0);
	m_lstStatsTargetTrajectory.clear();

	// Reset path devalue stuff.
	for(ShapeMap::iterator it = m_mapDesignerPaths.begin(); it != m_mapDesignerPaths.end(); ++it)
		it->second.devalueTime = 0;

	// Remove temporary shapes and re-enable all shapes.
	for(ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end();)
	{
		// Enable
		it->second.enabled = true;
		// Remove temp
		if(it->second.temporary)
			m_mapGenericShapes.erase(it++);
		else
			++it;
	}

  m_VertexList.Reset();

	m_bannedNavSmartObjects.clear();

	m_lastAmbientFireUpdateTime.SetSeconds(-10.0f);
	m_lastExpensiveAccessoryUpdateTime.SetSeconds(-10.0f);
	m_lastVisBroadPhaseTime.SetSeconds(-10.0f);
	m_lastGroupUpdateTime.SetSeconds(-10.0f);

	m_delayedExpAccessoryUpdates.clear();

	m_DEBUG_fakeTracers.clear();
	m_DEBUG_fakeHitEffect.clear();
	m_DEBUG_fakeDamageInd.clear();
	m_DEBUG_screenFlash = 0.0f;

	m_grenadeEvents.clear();
	m_collisionEvents.clear();
	m_bulletEvents.clear();
	m_soundEvents.clear();

	m_visChecks = 0;
	m_visChecksMax = 0;
	m_visChecksRays = 0;
	m_visChecksRaysMax = 0;
	m_visChecksHistoryHead = 0;
	m_visChecksHistorySize = 0;
	m_nRaysThisUpdateFrame = 0;

	m_enabledPuppetsUpdateError = 0;
	m_enabledPuppetsUpdateHead = 0;
	m_totalPuppetsUpdateCount = 0;
	m_disabledPuppetsUpdateError = 0;
	m_disabledPuppetsHead = 0;

	DebugOutputObjects("End of reset");

#ifdef CALIBRATE_STOPPER
  AILogAlways("Calculation stopper calibration:");
  for (CCalculationStopper::TMapCallRate::const_iterator it = CCalculationStopper::m_mapCallRate.begin() ; it != CCalculationStopper::m_mapCallRate.end() ; ++it)
  {
    const std::pair<unsigned, float> &result = it->second;
    const string &name = it->first;
    float rate = result.second > 0.0f ? result.first / result.second : -1.0f;
    AILogAlways("%s calls = %d time = %6.2f sec call-rate = %7.2f",f
      name.c_str(), result.first, result.second, rate);
  }
#endif
}

// copies a designer path into provided list if a path of such name is found
//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::GetDesignerPath(const char * szName, SShape & path) const
{
	ShapeMap::const_iterator di = m_mapDesignerPaths.find(szName);
	if (di == m_mapDesignerPaths.end())
		return false;
	path = di->second;
	return true;
}

// gets how many agents are in a specified group
//
//-----------------------------------------------------------------------------------------------------------
int CAISystem::GetGroupCount(int nGroupID, int flags, int type)
{
	if (type == 0)
	{
		AIGroupMap::iterator	it = m_mapAIGroups.find(nGroupID);
		if(it == m_mapAIGroups.end())
			return 0;

		return it->second->GetGroupCount(flags);
	}
	else
	{
		int count = 0;
		int	countEnabled = 0;
		for (AIObjects::iterator it = m_mapGroups.find(nGroupID); it != m_mapGroups.end() && it->first == nGroupID; ++it)
		{
			CAIObject* pObject = it->second;
			if (pObject->GetType() == type)
			{
				if (pObject->IsEnabled())
					countEnabled++;
				count++;
			}
		}

		if (flags & IAISystem::GROUP_MAX)
		{
			AIWarning("GetGroupCount does not support specified type and max count to be used at the same time."/*, type*/);
			flags &= ~IAISystem::GROUP_MAX;
		}

		if (flags == 0 || flags == IAISystem::GROUP_ALL)
			return count;
		else if (flags == IAISystem::GROUP_ENABLED)
			return countEnabled;
	}

	return 0;
}

// gets the I-th agent in a specified group
//
//-----------------------------------------------------------------------------------------------------------
IAIObject* CAISystem::GetGroupMember(int nGroupID, int nIndex, int flags, int type)
{
	if (nIndex < 0)
		return NULL;

	bool	bOnlyEnabled = false;
	if (flags & IAISystem::GROUP_ENABLED)
		bOnlyEnabled = true;

	AIObjects::iterator gi = m_mapGroups.find(nGroupID);
	for (; gi!=m_mapGroups.end() && nIndex >= 0; ++gi)
	{
		if ( gi->first!=nGroupID )
			return NULL;
		if ( gi->second->GetType() != AIOBJECT_LEADER && // skip leaders
			(!type || gi->second->GetType() == type) &&
			(!bOnlyEnabled || gi->second->IsEnabled()) )
		{
			if (0 == nIndex)
				return gi->second;
			--nIndex;
		}
	}
	return NULL;
}

//====================================================================
// GetLeader
//====================================================================
CLeader* CAISystem::GetLeader(int nGroupID)
{
	AIGroupMap::iterator	it = m_mapAIGroups.find(nGroupID);
	if(it != m_mapAIGroups.end())
		return it->second->GetLeader();
	return 0;
}

//====================================================================
ILeader* CAISystem::GetILeader(int nGroupID)
{
	return (ILeader*)GetLeader(nGroupID);
}

//====================================================================
CLeader* CAISystem::GetLeader(const CAIActor* pSoldier)
{
	return (pSoldier ? GetLeader(pSoldier->GetGroupId()): NULL);
}

//====================================================================
// SetLeader
//====================================================================
void CAISystem::SetLeader(IAIObject* pObject)
{
	// can't make disabled/dead puppet a leader
	if(!pObject->IsEnabled() || pObject->GetProxy()&&pObject->GetProxy()->GetActorHealth()<1 )
		return;
	CAIActor* pActor = pObject->CastToCAIActor();
	if(!pActor)
		return;
	int groupid = pActor->GetGroupId();
	CLeader* pLeader = GetLeader(groupid);
	if(!pLeader)
	{
		pLeader  = (CLeader*) CreateAIObject(AIOBJECT_LEADER, pObject);
		CAIGroup* pGroup = GetAIGroup(groupid);
		if(pGroup)
			pGroup->SetLeader(pLeader);
	}
	else
		pLeader->SetAssociation((CAIObject*)pObject);

	return;
}

//====================================================================
// SetLeader
//====================================================================
CLeader*  CAISystem::CreateLeader(int nGroupID)
{
	CLeader* pLeader = GetLeader(nGroupID);
	if(pLeader)
		return NULL;
	CAIGroup* pGroup = GetAIGroup(nGroupID);
	if(!pGroup)
		return NULL;
	TUnitList &unitsList=pGroup->GetUnits();
	if(unitsList.empty())
		return NULL;

	CAIActor* pLeaderActor=(unitsList.begin())->m_pUnit;
	pLeader  = (CLeader*) CreateAIObject(AIOBJECT_LEADER, pLeaderActor);
	pGroup->SetLeader(pLeader);
	return pLeader;
}



//====================================================================
// GetAIGroup
//====================================================================
CAIGroup* CAISystem::GetAIGroup(int nGroupID)
{
	AIGroupMap::iterator	it = m_mapAIGroups.find(nGroupID);
	if(it != m_mapAIGroups.end())
		return it->second;
	return 0;
}

IAIGroup* CAISystem::GetIAIGroup(int nGroupID)
{
	return (IAIGroup*)GetAIGroup(nGroupID);
}

int CAISystem::GetAlertStatus(const IAIObject* pObject)
{
	const CAIActor* pActor = pObject->CastToCAIActor();
	CLeader *pLeader = GetLeader(pActor);
	if(pLeader)
		return pLeader->GetAlertStatus();
	else 
		return -1;
}

//====================================================================
// WriteArea
//====================================================================
static void WriteArea(CCryFile & file, const string & name, const SpecialArea & sa)
{
	unsigned nameLen = name.length();
	file.Write(&nameLen, sizeof(nameLen));
	file.Write((char *) name.c_str(), nameLen * sizeof(char));
	int64 type = sa.type;
	file.Write((void *) &type, sizeof(type));
  int64 waypointConnections = sa.waypointConnections;
	file.Write((void *) &waypointConnections, sizeof(waypointConnections));
  char altered = sa.bAltered;
	file.Write((void *) &altered, sizeof(altered));
	file.Write((void *) &sa.fHeight, sizeof(sa.fHeight));
	file.Write((void *) &sa.fNodeAutoConnectDistance, sizeof(sa.fNodeAutoConnectDistance));
	file.Write((void *) &sa.fMaxZ, sizeof(sa.fMaxZ));
	file.Write((void *) &sa.fMinZ, sizeof(sa.fMinZ));
	file.Write((void *) &sa.nBuildingID, sizeof(sa.nBuildingID));
	unsigned char lightLevel = (int)sa.lightLevel;
	file.Write((void *) &lightLevel, sizeof(lightLevel));

	// now the area itself
	const ListPositions & pts = sa.GetPolygon();
	unsigned ptsSize = pts.size();
	file.Write(&ptsSize, sizeof(ptsSize));
	ListPositions::const_iterator it;
	for (it = pts.begin() ; it != pts.end() ; ++it)
	{
		// only works so long as *it is a contiguous object
		const Vec3 & pt = *it;
		file.Write((void *) &pt, sizeof(pt));
	}
}

//====================================================================
// ReadArea
//====================================================================
static void ReadArea(CCryFile & file, int version, string & name, SpecialArea & sa)
{
	unsigned nameLen;
	file.ReadType(&nameLen);
	char tmpName[1024];
	file.ReadType(&tmpName[0], nameLen);
	tmpName[nameLen] = '\0';
	name = tmpName;

	int64 type = 0;
	file.ReadType(&type);
	sa.type = (SpecialArea::EType) type;

	int64 waypointConnections = 0;
	file.ReadType(&waypointConnections);
	sa.waypointConnections = (EWaypointConnections) waypointConnections;

  char altered = 0;
	file.ReadType(&altered);
  sa.bAltered = altered != 0;
	file.ReadType(&sa.fHeight);
  if (version <= 16)
  {
    float junk;
  	file.ReadType(&junk);
  }
	file.ReadType(&sa.fNodeAutoConnectDistance);
	file.ReadType(&sa.fMaxZ);
	file.ReadType(&sa.fMinZ);
	file.ReadType(&sa.nBuildingID);
	if (version >= 18)
	{
		unsigned char lightLevel = 0;
		file.ReadType(&lightLevel);
		sa.lightLevel = (EAILightLevel)lightLevel;
	}

	// now the area itself
	ListPositions pts;
	unsigned ptsSize;
	file.ReadType(&ptsSize);

	for (unsigned iPt = 0 ; iPt < ptsSize ; ++iPt)
	{
		Vec3 pt;
		file.ReadType(&pt);
		pts.push_back(pt);
	}
  sa.SetPolygon(pts);
}

static const int maxForbiddenNameLen = 1024;

//====================================================================
// WriteForbiddenArea
//====================================================================
static void WritePolygonArea(CCryFile & file, const string & name, const ListPositions & pts)
{
	unsigned nameLen = name.length();
	if (nameLen >= maxForbiddenNameLen)
		nameLen = maxForbiddenNameLen - 1;
	file.Write(&nameLen, sizeof(nameLen));
	file.Write((char *) name.c_str(), nameLen * sizeof(char));

	unsigned ptsSize = pts.size();
	file.Write(&ptsSize, sizeof(ptsSize));
	ListPositions::const_iterator it;
	for (it = pts.begin() ; it != pts.end() ; ++it)
	{
		// only works so long as *it is a contiguous object
		const Vec3 & pt = *it;
		file.Write((void *) &pt, sizeof(pt));
	}
}

//====================================================================
// ReadForbiddenArea
//====================================================================
static bool ReadPolygonArea(CCryFile & file, int version, string & name, ListPositions & pts)
{
	unsigned nameLen = maxForbiddenNameLen;
	file.ReadType(&nameLen);
	if (nameLen >= maxForbiddenNameLen)
	{
		AIWarning("Excessive forbidden area name length - AI loading failed");
		return false;
	}
	char tmpName[maxForbiddenNameLen];
	file.ReadRaw(&tmpName[0], nameLen);
	tmpName[nameLen] = '\0';
	name = tmpName;

	unsigned ptsSize;
	file.ReadType(&ptsSize);

	for (unsigned iPt = 0 ; iPt < ptsSize ; ++iPt)
	{
		Vec3 pt;
		file.ReadType(&pt);
		pts.push_back(pt);
	}
	return true;
}

//====================================================================
// WriteForbiddenArea
//====================================================================
static void WriteForbiddenArea(CCryFile & file, const CAIShape* shape)
{
	unsigned nameLen = shape->GetName().length();
  if (nameLen >= maxForbiddenNameLen)
    nameLen = maxForbiddenNameLen - 1;
	file.Write(&nameLen, sizeof(nameLen));
	file.Write((char*)shape->GetName().c_str(), nameLen * sizeof(char));

	const ShapePointContainer& pts = shape->GetPoints();
	unsigned ptsSize = pts.size();
	file.Write(&ptsSize, sizeof(ptsSize));
	for (unsigned i = 0; i < ptsSize; ++i)
	{
		// only works so long as *it is a contiguous object
		const Vec3& pt = pts[i];
		file.Write((void *) &pt, sizeof(pt));
	}
}

//====================================================================
// ReadForbiddenArea
//====================================================================
static bool ReadForbiddenArea(CCryFile & file, int version, CAIShape* shape)
{
	unsigned nameLen = maxForbiddenNameLen;
	file.ReadType(&nameLen);
  if (nameLen >= maxForbiddenNameLen)
  {
    AIWarning("Excessive forbidden area name length - AI loading failed");
    return false;
  }
	char tmpName[maxForbiddenNameLen];
	file.ReadRaw(&tmpName[0], nameLen);
	tmpName[nameLen] = '\0';
	shape->SetName(tmpName);

	unsigned ptsSize;
	file.ReadType(&ptsSize);

	ShapePointContainer& pts = shape->GetPoints();
	pts.resize(ptsSize);

	for (unsigned i = 0; i < ptsSize; ++i)
		file.ReadType(&pts[i]);

	// Call build AABB since the point container was filled directly.
	shape->BuildAABB();

	return true;
}

//====================================================================
// WriteExtraLinkCostArea
//====================================================================
static void WriteExtraLinkCostArea(CCryFile & file, const string & name, const SExtraLinkCostShape &shape)
{
	unsigned nameLen = name.length();
	file.Write(&nameLen, sizeof(nameLen));
	file.Write((char *) name.c_str(), nameLen * sizeof(char));

  file.Write(&shape.origCostFactor, sizeof(shape.origCostFactor));
  file.Write(&shape.aabb.min, sizeof(shape.aabb.min));
  file.Write(&shape.aabb.max, sizeof(shape.aabb.max));

	unsigned ptsSize = shape.shape.size();
	file.Write(&ptsSize, sizeof(ptsSize));
	ListPositions::const_iterator it;
	for (it = shape.shape.begin() ; it != shape.shape.end() ; ++it)
	{
		// only works so long as *it is a contiguous object
		const Vec3 & pt = *it;
		file.Write((void *) &pt, sizeof(pt));
	}
}

//====================================================================
// ReadExtraLinkCostArea
//====================================================================
static void ReadExtraLinkCostArea(CCryFile & file, int version, string & name, SExtraLinkCostShape &shape)
{
	unsigned nameLen;
	file.ReadType(&nameLen);
	char tmpName[1024];
	file.ReadRaw(&tmpName[0], nameLen);
	tmpName[nameLen] = '\0';
	name = tmpName;

  file.ReadType(&shape.origCostFactor);
  shape.costFactor = shape.origCostFactor;
  file.ReadType(&shape.aabb.min);
  file.ReadType(&shape.aabb.max);

  unsigned ptsSize;
	file.ReadType(&ptsSize);

	for (unsigned iPt = 0 ; iPt < ptsSize ; ++iPt)
	{
		Vec3 pt;
		file.ReadType(&pt);
		shape.shape.push_back(pt);
	}
}

//====================================================================
// WriteAreasIntoFile
//====================================================================
void CAISystem::WriteAreasIntoFile(const char * fileNameAreas)
{
	CCryFile file;
	if( false != file.Open( fileNameAreas, "wb" ) )
	{
		int fileVersion = BAI_AREA_FILE_VERSION_WRITE;
		file.Write(&fileVersion, sizeof(fileVersion));

		{
			const CAIShapeContainer::ShapeVector& shapes = m_forbiddenAreas.GetShapes();
			unsigned numAreas = shapes.size();
			file.Write(&numAreas, sizeof(numAreas));
			for (unsigned i = 0; i < numAreas; ++i)
				WriteForbiddenArea(file, shapes[i]);
		}

		{
			unsigned numAreas = m_mapSpecialAreas.size(); 
			file.Write(&numAreas, sizeof(numAreas));
			SpecialAreaMap::const_iterator it;
			for (it = m_mapSpecialAreas.begin() ; it != m_mapSpecialAreas.end() ; ++it)
				WriteArea(file, it->first, it->second);
		}

		{
			const CAIShapeContainer::ShapeVector& shapes = m_designerForbiddenAreas.GetShapes();
			unsigned numAreas = shapes.size();
			file.Write(&numAreas, sizeof(numAreas));
			for (unsigned i = 0; i < numAreas; ++i)
				WriteForbiddenArea(file, shapes[i]);
		}

		{
			const CAIShapeContainer::ShapeVector& boundaries = m_forbiddenBoundaries.GetShapes();
			unsigned numBoundaries = boundaries.size();
			file.Write(&numBoundaries, sizeof(numBoundaries));
			for (unsigned i = 0; i < numBoundaries; ++i)
				WriteForbiddenArea(file, boundaries[i]);
		}

		{
			unsigned numExtraLinkCosts = m_mapExtraLinkCosts.size();
			file.Write(&numExtraLinkCosts, sizeof(numExtraLinkCosts));
			ExtraLinkCostShapeMap::const_iterator it;
			for (it = m_mapExtraLinkCosts.begin() ; it != m_mapExtraLinkCosts.end() ; ++it)
				WriteExtraLinkCostArea(file, it->first, it->second);
		}
    
		{
			unsigned numTriangleAreas = m_mapDynamicTriangularNavigation.size();
			file.Write(&numTriangleAreas, sizeof(numTriangleAreas));
			DynamicTriangulationShapeMap::const_iterator it;
			for (it = m_mapDynamicTriangularNavigation.begin() ; it != m_mapDynamicTriangularNavigation.end() ; ++it)
      {
				WritePolygonArea(file, it->first, it->second.shape);
        file.Write(&it->second.triSize, sizeof(float));
      }
		}

		{
			unsigned numDesignerPaths = m_mapDesignerPaths.size();
			file.Write(&numDesignerPaths, sizeof(numDesignerPaths));
			ShapeMap::const_iterator it;
			for (it = m_mapDesignerPaths.begin() ; it != m_mapDesignerPaths.end() ; ++it)
			{
				WritePolygonArea(file, it->first, it->second.shape);
				int	navType = it->second.navType;
				file.Write(&navType, sizeof(navType));
				file.Write(&it->second.type, sizeof(int));
			}
		}

		{
			unsigned numGenericShapes = m_mapGenericShapes.size();
			file.Write(&numGenericShapes, sizeof(numGenericShapes));
			ShapeMap::const_iterator it;
			for (it = m_mapGenericShapes.begin() ; it != m_mapGenericShapes.end() ; ++it)
			{
				WritePolygonArea(file, it->first, it->second.shape);
				int	navType = it->second.navType;
				file.Write(&navType, sizeof(navType));
				file.Write(&it->second.type, sizeof(int));
				float height = it->second.height;
				int lightLevel = (int)it->second.lightLevel;
				file.Write(&height, sizeof(height));
				file.Write(&lightLevel, sizeof(lightLevel));
			}
		}

    file.Close();
	}
	else
	{
		AIWarning("Unable to open areas file %s", fileNameAreas);
	}

}

//====================================================================
// ReadAreasFromFile
// only actually reads if not editor - in editor just checks the version
//====================================================================
void CAISystem::ReadAreasFromFile(const char * fileNameAreas)
{
  // If editor then only load forbidden areas from file - the rest get created
  // by editor
	if (!m_pSystem->IsEditor())
		FlushAllAreas();
	else
		m_forbiddenAreas.Clear();

	CCryFile file;
	if( false != file.Open( fileNameAreas, "rb" ) )
	{
		int iNumber;
		unsigned numAreas;
		file.ReadType(&iNumber);
		if (iNumber < BAI_AREA_FILE_VERSION_READ)
		{
			AIError("Wrong AI area file version - found %d expected %d - regenerate triangulation [Design bug]", iNumber, BAI_AREA_FILE_VERSION_WRITE);
			return;
		}

		{
			file.ReadType(&numAreas);
			// vague sanity check
			AIAssert(numAreas < 1000000);
			for (unsigned iArea = 0 ; iArea < numAreas ; ++iArea)
			{
				CAIShape* pShape = new CAIShape;
        if (!ReadForbiddenArea(file, iNumber, pShape))
          return;
				if (m_forbiddenAreas.FindShape(pShape->GetName()) != 0)
				{
					AIError("CAISystem::ReadAreasFromFile: Forbidden area '%s' already exists, please rename the shape and reexport.", pShape->GetName().c_str());
					delete pShape;
				}
				else
					m_forbiddenAreas.InsertShape(pShape);
			}
		}

		if (m_pSystem->IsEditor())
		{
			return;
		}

		{
			file.ReadType(&numAreas);
			// vague sanity check
			AIAssert(numAreas < 1000000);
			for (unsigned iArea = 0 ; iArea < numAreas ; ++iArea)
			{
				SpecialArea sa;
				string name;
				ReadArea(file, iNumber, name, sa);
				sa.nBuildingID = m_BuildingIDManager.GetId();
				if (m_mapSpecialAreas.find(name) != m_mapSpecialAreas.end())
				{
					AIError("CAISystem::ReadAreasFromFile: Navigation modifier '%s' already exists, please rename the shape and reexport.", name.c_str());
				}
				else
				{
					InsertSpecialArea (name, sa);
				}
			}
		}

		{
			file.ReadType(&numAreas);
			// vague sanity check
			AIAssert(numAreas < 1000000);
			for (unsigned iArea = 0 ; iArea < numAreas ; ++iArea)
			{
				CAIShape* pShape = new CAIShape;
				ReadForbiddenArea(file, iNumber, pShape);
				if (m_designerForbiddenAreas.FindShape(pShape->GetName()) != 0)
				{
					AIError("CAISystem::ReadAreasFromFile: Forbidden boundary '%s' already exists, please rename the shape and reexport.", pShape->GetName().c_str());
					delete pShape;
				}
				else
					m_designerForbiddenAreas.InsertShape(pShape);
			}
		}

    {
			file.ReadType(&numAreas);
			// vague sanity check
			AIAssert(numAreas < 1000000);
			for (unsigned iArea = 0 ; iArea < numAreas ; ++iArea)
			{
				CAIShape* pShape = new CAIShape;
				ReadForbiddenArea(file, iNumber, pShape);
				if (m_forbiddenBoundaries.FindShape(pShape->GetName()) != 0)
				{
					AIError("CAISystem::ReadAreasFromFile: Forbidden boundary '%s' already exists, please rename the shape and reexport.", pShape->GetName().c_str());
					delete pShape;
				}
				else
					m_forbiddenBoundaries.InsertShape(pShape);
			}
		}

		{
			file.ReadType(&numAreas);
			// vague sanity check
			AIAssert(numAreas < 1000000);
			for (unsigned iArea = 0 ; iArea < numAreas ; ++iArea)
			{
				SExtraLinkCostShape shape(ListPositions(), 0.0f);
				string name;
				ReadExtraLinkCostArea(file, iNumber, name, shape);
				if (m_mapExtraLinkCosts.find(name) != m_mapExtraLinkCosts.end())
				{
					AIError("CAISystem::ReadAreasFromFile: Extra link cost modifier '%s' already exists, please rename the shape and reexport.", name.c_str());
				}
				else
				{
					m_mapExtraLinkCosts.insert(ExtraLinkCostShapeMap::iterator::value_type(name,shape));
				}
			}
		}

		{
			file.ReadType(&numAreas);
			// vague sanity check
			AIAssert(numAreas < 1000000);
			for (unsigned iArea = 0 ; iArea < numAreas ; ++iArea)
			{
				ListPositions lp;
				string name;
				ReadPolygonArea(file, iNumber, name, lp);
        float triSize = 0.0f;
        file.ReadType(&triSize);

				if (m_mapDynamicTriangularNavigation.find(name) != m_mapDynamicTriangularNavigation.end())
				{
					AIError("CAISystem::ReadAreasFromFile: Dynamic triangulation area '%s' already exists, please rename the shape and reexport.", name.c_str());
				}
				else
				{
					m_mapDynamicTriangularNavigation.insert(
						DynamicTriangulationShapeMap::iterator::value_type(name, 
						SDynamicTriangulationShape(lp, triSize)));
				}
			}
		}

		{
			file.ReadType(&numAreas);
			// vague sanity check
			AIAssert(numAreas < 1000000);
			for (unsigned iArea = 0 ; iArea < numAreas ; ++iArea)
			{
				ListPositions lp;
				string name;
				ReadPolygonArea(file, iNumber, name, lp);

				int	navType, type;
				file.ReadType(&navType);
				file.ReadType(&type);

				if (m_mapDesignerPaths.find(name) != m_mapDesignerPaths.end())
					AIError("CAISystem::ReadAreasFromFile: Designer path '%s' already exists, please rename the path and reexport.", name.c_str());
				else
					m_mapDesignerPaths.insert(ShapeMap::iterator::value_type(name, SShape(lp, false, (IAISystem::ENavigationType)navType, type)));
			}
		}

		if(iNumber > 15)
		{
			file.ReadType(&numAreas);
			// vague sanity check
			AIAssert(numAreas < 1000000);
			for (unsigned iArea = 0 ; iArea < numAreas ; ++iArea)
			{
				ListPositions lp;
				string name;
				ReadPolygonArea(file, iNumber, name, lp);

				int	navType, type;
				file.ReadType(&navType);
				file.ReadType(&type);
				float height = 0;
				int lightLevel = 0;
				if (iNumber >= 18)
				{
					file.ReadType(&height);
					file.ReadType(&lightLevel);
				}

				if (m_mapGenericShapes.find(name) != m_mapGenericShapes.end())
					AIError("CAISystem::ReadAreasFromFile: Shape '%s' already exists, please rename the path and reexport.", name.c_str());
				else
					m_mapGenericShapes.insert(ShapeMap::iterator::value_type(name, SShape(lp, false, (IAISystem::ENavigationType)navType, type, height, (EAILightLevel)lightLevel)));
			}
		}
	}
	else
	{
		AIWarning("Unable to open areas file %s", fileNameAreas);
	}

}


// parses ai information into file
//
//-----------------------------------------------------------------------------------------------------------
#ifdef AI_FP_FAST
#pragma float_control(precise, on)
#pragma fenv_access(on)
#endif
void CAISystem::ParseIntoFile(const char * szFileName, CGraph *pGraph, bool bForbidden)
{
  CTimeValue absoluteStartTime = gEnv->pTimer->GetAsyncCurTime();

	for (unsigned int index=0;index<(int)m_vTriangles.size();++index)
	{
		Tri *tri = m_vTriangles[index];
	// make vertices know which triangles contain them
			m_vVertices[tri->v[0]].m_lstTris.push_back(index);
			m_vVertices[tri->v[1]].m_lstTris.push_back(index);
			m_vVertices[tri->v[2]].m_lstTris.push_back(index);
	}

	Vec3 tbbmin, tbbmax;
	tbbmin(m_pTriangulator->m_vtxBBoxMin.x,m_pTriangulator->m_vtxBBoxMin.y,m_pTriangulator->m_vtxBBoxMin.z);
	tbbmax(m_pTriangulator->m_vtxBBoxMax.x,m_pTriangulator->m_vtxBBoxMax.y,m_pTriangulator->m_vtxBBoxMax.z);

	pGraph->SetBBox(tbbmin,tbbmax);

	//I3DEngine *pEngine = m_pSystem->GetI3DEngine();
	unsigned int cnt=0;
	std::vector<Tri*>::iterator ti;
	//for ( unsigned int i=0;i<m_vTriangles.size();++i)
		
	for (ti=m_vTriangles.begin();ti!=m_vTriangles.end();++ti,++cnt)
	{
//		Tri *tri = m_vTriangles[i];
		Tri *tri = (*ti);

		if (!tri->graphNodeIndex)
		{
			// create node for this tri
			unsigned nodeIndex = pGraph->CreateNewNode(IAISystem::NAV_TRIANGULAR, Vec3(ZERO));
			tri->graphNodeIndex = nodeIndex;
		}

		Vtx *v1,*v2,*v3;
		v1 = &m_vVertices[tri->v[0]];
		v2 = &m_vVertices[tri->v[1]];
		v3 = &m_vVertices[tri->v[2]];

		GraphNode *pNode = pGraph->GetNodeManager().GetNode(tri->graphNodeIndex);

		// add the triangle vertices... for outdoors only
		ObstacleData odata;
		odata.vPos = Vec3(v1->x, v1->y, v1->z);
		odata.bCollidable = v1->bCollidable;
		pNode->GetTriangularNavData()->vertices.push_back(m_VertexList.AddVertex(odata));
		odata.vPos = Vec3(v2->x, v2->y, v2->z);
		odata.bCollidable = v2->bCollidable;
		pNode->GetTriangularNavData()->vertices.push_back(m_VertexList.AddVertex(odata));
		odata.vPos = Vec3(v3->x, v3->y, v3->z);
		odata.bCollidable = v3->bCollidable;
		pNode->GetTriangularNavData()->vertices.push_back(m_VertexList.AddVertex(odata));

		pGraph->FillGraphNodeData(tri->graphNodeIndex);

		// test first edge
		std::vector<int>::iterator u,v;
		for (u = v1->m_lstTris.begin();u!=v1->m_lstTris.end();++u)
			for (v = v2->m_lstTris.begin();v!=v2->m_lstTris.end();++v)
			{
				int au,av;
				au = (*u);
				av = (*v);
				if (au==av && au!=cnt)
				{
					Tri *other = m_vTriangles[au];
					if (!other->graphNodeIndex)
					{
						// create node for this tri
						unsigned nodeIndex = pGraph->CreateNewNode(IAISystem::NAV_TRIANGULAR, Vec3(ZERO));
						other->graphNodeIndex = nodeIndex;
					}
					if (-1 == pGraph->GetNodeManager().GetNode(tri->graphNodeIndex)->GetLinkIndex(pGraph->GetNodeManager(), pGraph->GetLinkManager(), pGraph->GetNodeManager().GetNode(other->graphNodeIndex)))
						pGraph->Connect(tri->graphNodeIndex, other->graphNodeIndex);
					pGraph->ResolveLinkData(pGraph->GetNodeManager().GetNode(tri->graphNodeIndex), pGraph->GetNodeManager().GetNode(other->graphNodeIndex));
				}
			}

			for (u = v2->m_lstTris.begin();u!=v2->m_lstTris.end();++u)
				for (v = v3->m_lstTris.begin();v!=v3->m_lstTris.end();++v)
				{
					int au,av;
					au = (*u);
					av = (*v);
					if (au==av && au!=cnt)
					{
						Tri *other = m_vTriangles[au];
						if (!other->graphNodeIndex)
						{
							// create node for this tri
							unsigned nodeIndex = pGraph->CreateNewNode(IAISystem::NAV_TRIANGULAR, Vec3(ZERO));
							other->graphNodeIndex = nodeIndex;
						}
						if (-1 == pGraph->GetNodeManager().GetNode(tri->graphNodeIndex)->GetLinkIndex(pGraph->GetNodeManager(), pGraph->GetLinkManager(), pGraph->GetNodeManager().GetNode(other->graphNodeIndex)))
							pGraph->Connect(tri->graphNodeIndex, other->graphNodeIndex);
						pGraph->ResolveLinkData(pGraph->GetNodeManager().GetNode(tri->graphNodeIndex), pGraph->GetNodeManager().GetNode(other->graphNodeIndex));
					}
				}

				for (u = v3->m_lstTris.begin();u!=v3->m_lstTris.end();++u)
					for (v = v1->m_lstTris.begin();v!=v1->m_lstTris.end();++v)
					{
						int au,av;
						au = (*u);
						av = (*v);
						if (au==av && au!=cnt)
						{
							Tri *other = m_vTriangles[au];
							if (!other->graphNodeIndex)
							{
								// create node for this tri
								unsigned nodeIndex = pGraph->CreateNewNode(IAISystem::NAV_TRIANGULAR, Vec3(ZERO));
								other->graphNodeIndex = nodeIndex;
							}
							if (-1 == pGraph->GetNodeManager().GetNode(tri->graphNodeIndex)->GetLinkIndex(pGraph->GetNodeManager(), pGraph->GetLinkManager(), pGraph->GetNodeManager().GetNode(other->graphNodeIndex)))
								pGraph->Connect(tri->graphNodeIndex, other->graphNodeIndex);
							pGraph->ResolveLinkData(pGraph->GetNodeManager().GetNode(tri->graphNodeIndex), pGraph->GetNodeManager().GetNode(other->graphNodeIndex));
						}
					}

	}

	for (ti=m_vTriangles.begin();ti!=m_vTriangles.end();++ti)
	{
		GraphNode *pCurrent = (GraphNode*) pGraph->GetNodeManager().GetNode((*ti)->graphNodeIndex);
		for (unsigned link = pCurrent->firstLinkIndex; link; link = pGraph->GetLinkManager().GetNextLink(link))
			pGraph->ResolveLinkData(pCurrent,pGraph->GetNodeManager().GetNode(pGraph->GetLinkManager().GetNextNode(link)));
	}

	pGraph->Validate("CAISystem::ParseIntoFile before adding forbidden", false);

	if (bForbidden)
	{
		AILogProgress(" Checking forbidden area validity.");
		if (!ValidateAreas())
		{
			AIError("Problems with AI areas - aborting triangulation [Design bug]");
			return;
		}

		AILogProgress("Adding forbidden areas.");
		AddForbiddenAreas();

		AILogProgress("Calculating link properties.");
		CalculateLinkProperties();

    AILogProgress("Disabling thin triangles near forbidden");
    DisableThinNodesNearForbidden();

    AILogProgress("Marking forbidden triangles");
    MarkForbiddenTriangles();
	}

	// Get the vertex/obstacle radius from vegetation data for soft cover objects.
	I3DEngine *p3DEngine = m_pSystem->GetI3DEngine();
	for(int i = 0; i < m_VertexList.GetSize(); ++i)
	{
		ObstacleData&	vert = m_VertexList.ModifyVertex(i);
		Vec3 vPos = vert.vPos;
		vPos.z = p3DEngine->GetTerrainElevation(vPos.x, vPos.y);

		Vec3 bboxsize(1.f, 1.f, 1.f);
		IPhysicalEntity *pPhys=0;

		IPhysicalEntity **pEntityList;
		int nCount = m_pWorld->GetEntitiesInBox(vPos-bboxsize, vPos+bboxsize, pEntityList, ent_static);

		int j=0;
		while (j<nCount)
		{
			pe_status_pos ppos;
			pEntityList[j]->GetStatus(&ppos);
			ppos.pos.z = vPos.z;
			if (IsEquivalent(ppos.pos, vPos, 0.001f))
			{
				pPhys = pEntityList[j];
				break;
			}
			++j;
		}

		if(!pPhys)
			continue;

		float overrideRadius = -1.0f;
		IRenderNode * pRN = (IRenderNode*)pPhys->GetForeignData(PHYS_FOREIGN_ID_STATIC);
		if (pRN)
		{
			IStatObj *pStatObj = pRN->GetEntityStatObj(0);
			if (pStatObj)
			{
        pe_status_pos status;
        status.ipart = 0;
        pPhys->GetStatus( &status );
				float r = pStatObj->GetAIVegetationRadius();
				if (r > 0.0f)
					overrideRadius = r * status.scale;
				else
					overrideRadius = -2.0f;
				vert.SetApproxHeight((status.pos.z + status.BBox[1].z) - vert.vPos.z);
			}
		}
		vert.fApproxRadius = max(vert.fApproxRadius, overrideRadius);
	}

	AILogProgress(" Now writing to %s.",szFileName);
	pGraph->WriteToFile(szFileName);

	pGraph->Validate("End of CAISystem::ParseIntoFile", false);

  CTimeValue absoluteEndTime = gEnv->pTimer->GetAsyncCurTime();

  AILogProgress("Finished CAISystem::ParseIntoFile in %f seconds", (absoluteEndTime - absoluteStartTime).GetSeconds());
}
#ifdef AI_FP_FAST
#pragma fenv_access(off)
#pragma float_control(precise, off)
#endif

//===================================================================
// PushFront 
// Given the list and the vector, inserts the whole list in front of
// the vector's elements, widening the vector accordingly
//===================================================================
static void PushFront (std::vector<Tri*>& arrTri, const std::list<Tri*>& lstTri)
{
	arrTri.insert(arrTri.begin(),lstTri.begin(),lstTri.end());
}

//
//-----------------------------------------------------------------------------------------------------------
IGraph * CAISystem::GetHideGraph(void)
{
	return m_pHideGraph;
}

static void CheckShapeNameClashes(const CAIShapeContainer& shapeCont, const char* szContainer)
{
	AILogComment("Dumping names in %s", szContainer);
	const CAIShapeContainer::ShapeVector& shapes = shapeCont.GetShapes();
	for (unsigned i = 0; i < shapes.size(); ++i)
	{
		unsigned n = 1;
		AILogComment(" '%s'", shapes[i]->GetName().c_str());
		for (unsigned j = i+1; j < shapes.size(); ++j)
		{
			if (shapes[i]->GetName().compare(shapes[j]->GetName()) == 0)
				++n;
		}
		if (n > 1)
			AIError("%s containes %d duplicates of shapes called '%s'", szContainer, n, shapes[i]->GetName().c_str());
	}
}

// NOTE Jun 7, 2007: <pvl> not const anymore because ForbiddenAreasOverlap()
// isn't const anymore
//===================================================================
// ValidateAreas
//===================================================================
bool CAISystem::ValidateAreas()
{
#ifdef _DEBUG
  // if enabled for debug slows things down too much
  return true;
#endif

#ifdef USER_dejan
	return true;
#endif

  if (!AIGetWarningErrorsEnabled())
  {
    AILogEvent("CAISystem::ValidateAreas Skipping area validation"); 
    return true;
  }


	bool result = true;
	for (ShapeMap::const_iterator it = m_mapDesignerPaths.begin() ; it != m_mapDesignerPaths.end() ; ++it)
	{
		if (it->second.shape.size() < 2)
		{
			AIWarning("AI Path %s has only %d points", it->first.c_str(), it->second.shape.size());
			result = false;
		}
	}
	for (ShapeMap::const_iterator it = m_mapGenericShapes.begin() ; it != m_mapGenericShapes.end() ; ++it)
	{
		if (it->second.shape.size() < 2)
		{
			AIWarning("AI Path %s has only %d points", it->first.c_str(), it->second.shape.size());
			result = false;
		}
	}
	for (ShapeMap::const_iterator it = m_mapOcclusionPlanes.begin() ; it != m_mapOcclusionPlanes.end() ; ++it)
	{
		if (it->second.shape.size() < 2)
		{
			AIWarning("AI Occlusion Plane %s has only %d points", it->first.c_str(), it->second.shape.size());
			result = false;
		}
	}
	for (unsigned i = 0, ni = m_designerForbiddenAreas.GetShapes().size(); i < ni; ++i)
	{
		CAIShape* pShape = m_designerForbiddenAreas.GetShapes()[i];
		if (pShape->GetPoints().size() < 3)
		{
			AIWarning("AI Designer Forbidden Area %s has only %d points", pShape->GetName().c_str(), pShape->GetPoints().size());
			result = false;
		}
	}
	for (unsigned i = 0, ni = m_forbiddenAreas.GetShapes().size(); i < ni; ++i)
	{
		CAIShape* pShape = m_forbiddenAreas.GetShapes()[i];
		if (pShape->GetPoints().size() < 3)
		{
			AIWarning("AI Forbidden Area %s has only %d points", pShape->GetName().c_str(), pShape->GetPoints().size());
			result = false;
		}
	}
	for (unsigned i = 0, ni = m_forbiddenBoundaries.GetShapes().size(); i < ni; ++i)
	{
		CAIShape* pShape = m_forbiddenBoundaries.GetShapes()[i];
		if (pShape->GetPoints().size() < 3)
		{
			AIWarning("AI Forbidden Boundary %s has only %d points", pShape->GetName().c_str(), pShape->GetPoints().size());
			result = false;
		}
	}
	for (SpecialAreaMap::const_iterator it = m_mapSpecialAreas.begin() ; it != m_mapSpecialAreas.end() ; ++it)
	{
		if (it->second.GetPolygon().size() < 3)
		{
			AIWarning("AI Area %s has only %d points", it->first.c_str(), it->second.GetPolygon().size());
			result = false;
		}
	}

	if (ForbiddenAreasOverlap())
		result = false;

	// Check name clashes.
	CheckShapeNameClashes(m_forbiddenBoundaries, "Forbidden Boundaries");
	CheckShapeNameClashes(m_designerForbiddenAreas, "Forbidden Areas (editor)");
	CheckShapeNameClashes(m_forbiddenAreas, "Forbidden Areas (automatic)");

	return result;
}

// // loads the triangulation for this level and mission
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::LoadNavigationData(const char * szLevel, const char * szMission)
{
	if(!IsEnabled())
		return;
	CTimeValue startTime = m_pSystem->GetITimer()->GetAsyncCurTime();
	if ( m_bInitialized )
		Reset(IAISystem::RESET_INTERNAL_LOAD);

	if (!szLevel || !szMission)
		return;
	while (!m_lstPathQueue.empty())
	{
		AIWarning( "GRAPHRELOADWARNING: Agents were waiting to generate paths, but graph changed. May cause incorrect behaviour.");
		PathFindRequest *pRequest = m_lstPathQueue.front();
		delete pRequest;
		m_lstPathQueue.pop_front();
	}

	m_VertexList.Clear();

  m_pGraph->Clear(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE);
	m_pHideGraph->Clear(IAISystem::NAVMASK_ALL);

	m_pTriangularNavRegion->Clear();
	m_pWaypointHumanNavRegion->Clear();
  m_pWaypoint3DSurfaceNavRegion->Clear();
	m_pVolumeNavRegion->Clear();
	m_pFlightNavRegion->Clear();
	//m_pRoadNavRegion->Clear();
	m_pSmartObjectNavRegion->Clear();
	m_pTriangularHideRegion->Clear();
  m_pFree2DNavRegion->Clear();

  m_pGraph->CheckForEmpty(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE | IAISystem::NAV_VOLUME | IAISystem::NAV_FLIGHT | IAISystem::NAV_ROAD | IAISystem::NAV_FREE_2D);
	m_pHideGraph->CheckForEmpty(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE | IAISystem::NAV_VOLUME | IAISystem::NAV_FLIGHT | IAISystem::NAV_ROAD | IAISystem::NAV_FREE_2D);

	char fileName[1024], fileNameHide[1024], fileNameVolume[1024],
    fileNameFlight[1024], fileNameAreas[1024], fileNameRoads[1024],
    fileNameWaypoint3DSurface[1024], fileNameFree2D[1024], fileNameVerts[1024];
	sprintf(fileName, "%s/net%s.bai",szLevel,szMission);
	sprintf(fileNameVerts, "%s/verts%s.bai",szLevel,szMission);
	sprintf(fileNameHide, "%s/hide%s.bai",szLevel,szMission);
	sprintf(fileNameVolume, "%s/v3d%s.bai",szLevel,szMission);
	sprintf(fileNameFlight, "%s/fnav%s.bai",szLevel,szMission);
	sprintf(fileNameRoads, "%s/roadnav%s.bai",szLevel,szMission);
	sprintf(fileNameAreas, "%s/areas%s.bai",szLevel,szMission);
	sprintf(fileNameWaypoint3DSurface, "%s/waypt3Dsfc%s.bai",szLevel,szMission);
	sprintf(fileNameFree2D, "%s/free2d%s.bai",szLevel,szMission);

  // basic test - if there's not triangular navigation file then assume there's no navigation/AI
  // in this level
  {
    CCryFile file;;
    if (!file.Open( fileName,"rb"))
    {
      AILogLoading("No navigation data available - disabling AI");
      m_navDataState = NDS_UNSET;
      return;
    }
  }

	// editor will have already loaded in the special areas - for the editor we just check it was 
	// the right version
	m_nNumBuildings = 0;
	m_navDataState = NDS_OK;
	ReadAreasFromFile(fileNameAreas);
	AILogLoading("Reading vertex list");
	if( !m_VertexList.ReadFromFile(fileNameVerts))
		m_navDataState = NDS_BAD;
	else
	{
		AILogLoading("Reading triangles.");
		if (!m_pGraph->ReadFromFile(fileName))
			m_navDataState = NDS_BAD;
		m_pHideGraph->ReadFromFile(fileNameHide);
	}
	AILogLoading("Reading 3D volumes.");
	m_pVolumeNavRegion->ReadFromFile(fileNameVolume);
	AILogLoading("Reading Flight navigation.");
	m_pFlightNavRegion->ReadFromFile(fileNameFlight);
	AILogLoading("Reading Road navigation");
	m_pRoadNavRegion->ReadFromFile(fileNameRoads);
	AILogLoading("Reading 3D surface navigation");
	m_pWaypoint3DSurfaceNavRegion->ReadFromFile(fileNameWaypoint3DSurface);
  AILogLoading("Building forbidden QuadTrees");
  RebuildForbiddenQuadTrees();
  if (::IsAIInDevMode())
  {
  	AILogLoading("Validating navigation.");
  	m_pGraph->Validate("CAISystem::LoadNavigationData nav graph after VolumeNavRegion read", true);
	  m_pHideGraph->Validate("CAISystem::LoadNavigationData hide graph after reading", false);
  	ValidateAreas();
  }
  CTimeValue endTime = m_pSystem->GetITimer()->GetAsyncCurTime();
	AILogLoading("Navigation Data Loaded in %5.2f sec", (endTime - startTime).GetSeconds());
}

//====================================================================
// OnMissionLoaded
//====================================================================
void CAISystem::OnMissionLoaded()
{
  m_pWaypoint3DSurfaceNavRegion->OnMissionLoaded();
  m_pWaypointHumanNavRegion->OnMissionLoaded();
  m_pFlightNavRegion->OnMissionLoaded();
  m_pVolumeNavRegion->OnMissionLoaded();
  m_pRoadNavRegion->OnMissionLoaded();
  m_pTriangularHideRegion->OnMissionLoaded();
  m_pTriangularNavRegion->OnMissionLoaded();
}


//====================================================================
// CExtraPoint
// Simple structure to keep track of points added for breakable regions
// so we don't get duplicates
//====================================================================
struct CExtraPoint
{
	// how close together two points have to be to consider them equal
	static float m_tol;
	Vec3 m_pt;
};
float CExtraPoint::m_tol = 0.1f;

//====================================================================
// operator<
//====================================================================
bool operator<(const CExtraPoint& lhs, const CExtraPoint& rhs)
{
	if (lhs.m_pt.x < (rhs.m_pt.x - CExtraPoint::m_tol))
		return true;
	else if (lhs.m_pt.x > (rhs.m_pt.x + CExtraPoint::m_tol))
		return false;
	if (lhs.m_pt.y < (rhs.m_pt.y - CExtraPoint::m_tol))
		return true;
	return false;
}

//====================================================================
// AddBreakableRegionToPointSet
//====================================================================
void AddBreakableRegionToPointSet(std::set<CExtraPoint> & points, IPhysicalEntity * pEntity, float gridDelta)
{
	pe_status_pos status;
	pEntity->GetStatus( &status );
	AABB aabb(status.BBox[0] + status.pos, status.BBox[1] + status.pos);
	float minX = gridDelta * cry_floorf(aabb.min.x / gridDelta) - gridDelta;
	float minY = gridDelta * cry_floorf(aabb.min.y / gridDelta) - gridDelta;
	float maxX = gridDelta * cry_ceilf(aabb.max.x / gridDelta) + gridDelta;
	float maxY = gridDelta * cry_ceilf(aabb.max.y / gridDelta) + gridDelta;
	float z = status.pos.z;
	for (float x = minX ; x <= maxX ; x += gridDelta)
	{
		for (float y = minY ; y <= maxY ; y += gridDelta)
		{
			CExtraPoint pt;
			pt.m_pt = Vec3(x, y, z);
			points.insert(pt);
		}
	}
}

//====================================================================
// GetDynamicTriangulationValue
//====================================================================
float CAISystem::GetDynamicTriangulationValue(const Vec3 &pos) const
{
  float bestValue = std::numeric_limits<float>::max();

  for (DynamicTriangulationShapeMap::const_iterator it = m_mapDynamicTriangularNavigation.begin() ; 
    it != m_mapDynamicTriangularNavigation.end() ; ++it)
  {
    const SDynamicTriangulationShape &shape = it->second;
    if (shape.triSize > bestValue)
      continue;
    const AABB &aabb = shape.aabb;
    if (!Overlap::Point_Polygon2D(pos, shape.shape, &aabb))
      continue;
    bestValue = shape.triSize;
  }
  if (bestValue == std::numeric_limits<float>::max())
    return 0.0f;
  else
    return bestValue;
}

//====================================================================
// AddDynamicTrianglesToTriangulator
//====================================================================
void CAISystem::AddDynamicTrianglesToTriangulator(const AABB & worldAABB)
{
	CExtraPoint::m_tol = 0.1f;
	std::set<CExtraPoint> extraPoints;

  int tweakY = 1;
  for (DynamicTriangulationShapeMap::const_iterator it = m_mapDynamicTriangularNavigation.begin() ; 
    it != m_mapDynamicTriangularNavigation.end() ; ++it)
  {
    const SDynamicTriangulationShape &shape = it->second;

    float gridDelta = shape.triSize;
    const AABB &aabb = shape.aabb;

    float minX = gridDelta * cry_floorf(aabb.min.x / gridDelta) - gridDelta;
	  float minY = gridDelta * cry_floorf(aabb.min.y / gridDelta) - gridDelta;
	  float maxX = gridDelta * cry_ceilf(aabb.max.x / gridDelta) + gridDelta;
	  float maxY = gridDelta * cry_ceilf(aabb.max.y / gridDelta) + gridDelta;
	  float z = 0.0f;
	  for (float x = minX ; x <= maxX ; x += gridDelta)
	  {
      tweakY = -tweakY;
		  for (float y = minY ; y <= maxY ; y += gridDelta)
		  {
			  CExtraPoint pt;
			  pt.m_pt = Vec3(x, y + 0.25f * tweakY * gridDelta, z);

        if (!Overlap::Point_Polygon2D(pt.m_pt, shape.shape, &shape.aabb))
          continue;
        if (IsPointInForbiddenRegion(pt.m_pt))
          continue;

        float triSize = GetDynamicTriangulationValue(pt.m_pt);
        if (triSize != gridDelta)
          continue;

        extraPoints.insert(pt);
		  }
	  }
  }

  // add the points
  float tol = 0.5f;
	for (std::set<CExtraPoint>::const_iterator it = extraPoints.begin(); it != extraPoints.end() ; ++it)
	{
    if (worldAABB.IsContainSphere(it->m_pt, 1.0f) && 
        !m_pTriangulator->DoesVertexExist2D(it->m_pt.x, it->m_pt.y, tol) &&
        !GetAISystem()->IsPointOnForbiddenEdge(it->m_pt, tol) &&
        IsPointInTriangulationAreas(it->m_pt))
			m_pTriangulator->AddVertex(it->m_pt.x, it->m_pt.y, it->m_pt.z, false);
	}
}


//====================================================================
// AddBreakableRegionsToTriangulator
//====================================================================
void CAISystem::AddBreakableRegionsToTriangulator(float gridDelta, const AABB & worldAABB)
{
  // this just creates trouble... the pass radius calculation is pretty bogus when there are objects/
  // not objects at the triangle corners. Rather than doing it automatically it should be done using 
  // designer placed shapes.
  // todo - Danny remove/change
  return;

	CExtraPoint::m_tol = gridDelta * 0.1f;
	std::set<CExtraPoint> extraPoints;

	IPhysicalEntity **pEntityList;
	int nCount = m_pWorld->GetEntitiesInBox(worldAABB.min, worldAABB.max, pEntityList, ent_static|ent_ignore_noncolliding);
	if (nCount>0) 
	{
		for (int iEntity = 0 ; iEntity < nCount ; ++iEntity)
		{
			IPhysicalEntity * pEntity = pEntityList[iEntity];

      static bool useNew = true;
      if (useNew)
      {
		    pe_params_part pp; 
        pp.ipart=0;
        if (pEntity->GetParams(&pp)!=0 && pp.idmatBreakable>=0)
        {
          AddBreakableRegionToPointSet(extraPoints, pEntity, gridDelta);
        }
      }
      else
      {
			  int type = pEntity->GetiForeignData();
			  if (type != PHYS_FOREIGN_ID_STATIC)
				  continue;
			  IRenderNode *pRenderNode = (IRenderNode*)pEntity->GetForeignData(PHYS_FOREIGN_ID_STATIC);
			  if (!pRenderNode)
				  continue;
			  IMaterial * pMaterial = pRenderNode->GetMaterial();
			  if (!pMaterial) 
				  continue;
			  int nMats = pMaterial->GetSubMtlCount();
			  for (int iMat = 0 ; iMat < nMats ; ++iMat)
			  {
				  IMaterial * pMat = pMaterial->GetSubMtl(iMat);
				  if (!pMat)
					  continue;
					if (pMat->GetSurfaceType()->GetBreakable2DParams())
					  AddBreakableRegionToPointSet(extraPoints, pEntity, gridDelta);
        }
			}
		}
	}

	// add the points
	for (std::set<CExtraPoint>::const_iterator it = extraPoints.begin(); it != extraPoints.end() ; ++it)
	{
		if (worldAABB.IsContainSphere(it->m_pt, 1.0f) && IsPointInTriangulationAreas(it->m_pt))
			m_pTriangulator->AddVertex(it->m_pt.x, it->m_pt.y, it->m_pt.z, false);
	}
}

//====================================================================
// IsPointInWaterAreas
//====================================================================
inline bool IsPointInWaterAreas(const Vec3& pt, const std::vector<ListPositions>& areas)
{
	unsigned nAreas = areas.size();
	for (unsigned i = 0 ; i < nAreas ; ++i)
	{
		const ListPositions& area = areas[i];
		if (Overlap::Point_Polygon2D(pt, area))
			return true;
	}
	return false;
}

//===================================================================
// IsPointInWaterAreas
//===================================================================
bool CAISystem::IsPointInWaterAreas(const Vec3 &pt) const
{
	for (SpecialAreaMap::const_iterator it = m_mapSpecialAreas.begin() ; it != m_mapSpecialAreas.end() ; ++it)
	{
    const string &name = it->first;
    const SpecialArea &sa = it->second;
		if (sa.type == SpecialArea::TYPE_WATER)
    {
      if (Overlap::Point_AABB2D(pt, sa.GetAABB()))
        if (Overlap::Point_Polygon2D(pt, sa.GetPolygon()))
          return true;
    }
	}
  return false;
}

//====================================================================
// AddBeachPointsToTriangulator
//====================================================================
void CAISystem::AddBeachPointsToTriangulator(const AABB & worldAABB)
{
	std::vector<ListPositions> areas;
  AABB aabb(AABB::RESET);
	for (SpecialAreaMap::iterator it = m_mapSpecialAreas.begin() ; it != m_mapSpecialAreas.end() ; ++it)
	{
		if (it->second.type == SpecialArea::TYPE_WATER)
    {
      const ListPositions &pts = it->second.GetPolygon();
			areas.push_back(pts);
      for (ListPositions::const_iterator ptIt = pts.begin() ; ptIt != pts.end() ; ++ptIt)
      {
        Vec3 pt = *ptIt;
        aabb.Add(pt);
      }
    }
	}
  
  if (areas.empty())
    return;

  int terrainMinX = (int) aabb.min.x;
  int terrainMinY = (int) aabb.min.y;
  int terrainMaxX = (int) aabb.max.x;
  int terrainMaxY = (int) aabb.max.y;

  static float criticalDepth = 0.1f;
	static float criticalDeepDepth = 5.0f;
	static unsigned delta = 4;
	I3DEngine *pEngine = m_pSystem->GetI3DEngine();

	int terrainArraySize = pEngine->GetTerrainSize();
	unsigned edge = 16;

  Limit(terrainMinX, 0, terrainArraySize);
  Limit(terrainMinY, 0, terrainArraySize);
  Limit(terrainMaxX, 0, terrainArraySize);
  Limit(terrainMaxY, 0, terrainArraySize);

	for (int ix = terrainMinX ; ix + delta < terrainMaxX ; ix += delta)
	{
		for (int iy = terrainMinY ; iy + delta < terrainMaxY ; iy += delta)
		{
			Vec3 v00(ix, iy, 0.0f);
			Vec3 v10(ix+delta, iy, 0.0f);
			Vec3 v01(ix, iy+delta, 0.0f);
			v00.z = pEngine->GetTerrainElevation(v00.x, v00.y);
			v10.z = pEngine->GetTerrainElevation(v10.x, v00.y);
			v01.z = pEngine->GetTerrainElevation(v01.x, v00.y);
			float water00Z = pEngine->GetWaterLevel(&v00);
			float water10Z = pEngine->GetWaterLevel(&v10);
			float water01Z = pEngine->GetWaterLevel(&v01);
			float depth00 = water00Z - v00.z;
			float depth10 = water10Z - v10.z;
			float depth01 = water01Z - v01.z;
			if ( (depth00 > criticalDepth) != (depth10 > criticalDepth) )
			{
				Vec3 pt = 0.5f * (v00 + v10);
        if (worldAABB.IsContainSphere(pt, 1.0f) && ::IsPointInWaterAreas(pt, areas) && IsPointInTriangulationAreas(pt))
					m_pTriangulator->AddVertex(pt.x, pt.y, pt.z, false);
			}
			if ( (depth00 > criticalDepth) != (depth01 > criticalDepth) )
			{
				Vec3 pt = 0.5f * (v00 + v01);
        if (worldAABB.IsContainSphere(pt, 1.0f) && ::IsPointInWaterAreas(pt, areas) && IsPointInTriangulationAreas(pt))
					m_pTriangulator->AddVertex(pt.x, pt.y, pt.z, false);
			}
		}
	}

	// also add points in the main water body
	static int waterDelta = 100;
	for (unsigned ix = terrainMinX ; ix + waterDelta < terrainMaxX ; ix += waterDelta)
	{
		for (unsigned iy = terrainMinY ; iy + waterDelta < terrainMaxY ; iy += waterDelta)
		{
			Vec3 v(ix, iy, 0.0f);
			v.z = pEngine->GetTerrainElevation(v.x, v.y);
			float waterZ = pEngine->GetWaterLevel(&v);
			float depth = waterZ - v.z;
			if ( depth > criticalDeepDepth )
			{
        if (worldAABB.IsContainSphere(v, 1.0f) && ::IsPointInWaterAreas(v, areas) && IsPointInTriangulationAreas(v))
					m_pTriangulator->AddVertex(v.x, v.y, v.z, false);
			}
		}
	}
}


//====================================================================
// GenerateTriangulation
// generate the triangulation for this level and mission
//====================================================================
#ifdef AI_FP_FAST
#pragma float_control(precise, on)
#pragma fenv_access(on)
#endif
void CAISystem::GenerateTriangulation(const char * szLevel, const char * szMission)
{
	if (!szLevel || !szMission)
		return;

//ValidateBigObstacles();
//return;

  float absoluteStartTime = gEnv->pTimer->GetAsyncCurTime();

	char fileName[1024], fileNameHide[1024], fileNameAreas[1024], fileNameVerts[1024];
	sprintf(fileName, "%s/net%s.bai",szLevel,szMission);
	sprintf(fileNameHide, "%s/hide%s.bai",szLevel,szMission);
	sprintf(fileNameAreas, "%s/areas%s.bai",szLevel,szMission);
	sprintf(fileNameVerts, "%s/verts%s.bai",szLevel,szMission);

	while (!m_lstPathQueue.empty())
	{
		AIWarning( "GRAPHRELOADWARNING: Agents were waiting to generate paths, but graph changed. May cause incorrect behaviour.");
		PathFindRequest *pRequest = m_lstPathQueue.front();
		delete pRequest;
		m_lstPathQueue.pop_front();
	}

	AILogProgress("Clearing old graph data...");
	m_pGraph->Clear(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE);
	m_pHideGraph->Clear(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE);
	m_pGraph->CheckForEmpty(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE);
	m_pHideGraph->CheckForEmpty(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE);

	ClearForbiddenQuadTrees();

  AILogProgress("Generating triangulation: forbidden boundaries:");
	for (unsigned i = 0, ni = m_forbiddenBoundaries.GetShapes().size(); i < ni; ++i)
	{
		CAIShape* pShape = m_forbiddenBoundaries.GetShapes()[i];
		AILogProgress("%s, %d points", pShape->GetName().c_str(), pShape->GetPoints().size());
	}

  AILogProgress("Generating triangulation: designer-created forbidden areas:");
	for (unsigned i = 0, ni = m_designerForbiddenAreas.GetShapes().size(); i < ni; ++i)
	{
		CAIShape* pShape = m_designerForbiddenAreas.GetShapes()[i];
		AILogProgress("%s, %d points", pShape->GetName().c_str(), pShape->GetPoints().size());
	}

  if (!CalculateForbiddenAreas())
    return;

  AILogProgress("Generating triangulation: forbidden areas:");
	for (unsigned i = 0, ni = m_forbiddenAreas.GetShapes().size(); i < ni; ++i)
	{
		CAIShape* pShape = m_forbiddenAreas.GetShapes()[i];
		AILogProgress("%s, %d points", pShape->GetName().c_str(), pShape->GetPoints().size());
	}

  AILogProgress("Building forbidden area QuadTrees");
  RebuildForbiddenQuadTrees();
//  m_forbiddenAreasQuadTree.Dump("m_forbiddenAreasQuadTree");
//  m_designerForbiddenAreasQuadTree.Dump("m_designerForbiddenAreasQuadTree");
//  m_forbiddenBoundariesQuadTree.Dump("m_forbiddenBoundariesQuadTree");
  AILogProgress("Finished building forbidden area QuadTrees");

  m_VertexList.Clear();

	Vec3 min,max;
	float fTSize = (float) m_pSystem->GetI3DEngine()->GetTerrainSize();
	AILogProgress(" Triangulation started. Terrain size = %.0f",fTSize);

	min.Set(0,0,-5000);
	max.Set(fTSize,fTSize,5000.0f);

	if (m_pTriangulator) 
		delete m_pTriangulator;
	m_pTriangulator = new CTriangulator();
	CTriangulator *pHideTriangulator = new CTriangulator();

	// get only static physical entities (trees, buildings etc...)
	IPhysicalEntity** pObstacles;
	int	flags = ent_static;
	if(m_cvIncludeNonColEntitiesInNavigation->GetIVal() == 0)
		flags |= ent_ignore_noncolliding;
	int count = m_pWorld->GetEntitiesInBox(min,max,pObstacles,flags);
	
	m_pTriangulator->m_vVertices.reserve(count);

	AILogProgress("Processing >>> %d objects >>> box size [%.0f x %.0f %.0f]",count, max.x, max.y, max.z);

	AABB worldAABB(min, max);

	m_pTriangulator->AddVertex(0.0f, 0.0f ,0.1f, false);
	m_pTriangulator->AddVertex(fTSize, 0.0f ,0.1f,false);
	m_pTriangulator->AddVertex(fTSize, fTSize, 0.1f,false);
	m_pTriangulator->AddVertex(0.0f, fTSize, 0.1f,false);

	pHideTriangulator->AddVertex(0.0f, 0.0f, 0.1f, false);
	pHideTriangulator->AddVertex(fTSize, 0.0f, 0.1f, false);
	pHideTriangulator->AddVertex(fTSize, fTSize, 0.1f, false);
	pHideTriangulator->AddVertex(0.0f, fTSize, 0.1f, false);

	if (!pHideTriangulator->PrepForTriangulation())
		return;
	if (!m_pTriangulator->PrepForTriangulation())
		return;

	I3DEngine *pEngine = m_pSystem->GetI3DEngine();
	int vertexCounter(0);

	for (int i = 0 ; i < count ; ++i)
	{
		IPhysicalEntity *pCurrent = pObstacles[i];

		// don't add entities (only triangulate brush-like objects, and brush-like
		// objects should not be entities!)
		IEntity * pEntity = (IEntity*) pCurrent->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
		if (pEntity)
			continue;

		pe_params_foreign_data pfd;
		if(pCurrent->GetParams(&pfd)==0)
			continue;

		pe_status_pos sp; 
		sp.ipart = -1;
		sp.partid = -1;
		if(pCurrent->GetStatus(&sp)==0)
			continue;

		pe_status_pos status;
		if(pCurrent->GetStatus(&status)==0)
			continue;

		Vec3 calc_pos = status.pos;
		// if too flat and too close to the terrain
		float zpos = pEngine->GetTerrainElevation(calc_pos.x,calc_pos.y);
		float ftop = status.pos.z + status.BBox[1].z;

		if(zpos > ftop)	// skip underground stuff
			continue;
		if(fabs(ftop - zpos) < 0.4f)	// skip stuff too close to the terrain
			continue;

		bool	collidable = true;

		// Only include high non-collidables.
		if((sp.flagsOR & geom_colltype_solid) == 0)
		{
      static float criticalZ = 0.9f;
			if(fabs(ftop - zpos) < criticalZ)
				continue;

      IRenderNode * pRN = (IRenderNode*)pCurrent->GetForeignData(PHYS_FOREIGN_ID_STATIC);
      if (pRN)
      {
        IStatObj *pStatObj = pRN->GetEntityStatObj(0);
        if (pStatObj)
        {
          float r = pStatObj->GetAIVegetationRadius();
          if (r < 0.0f)
            continue;
        }
      }

			collidable = false;
		}

		// don't triangulate objects that your feet wouldn't touch when walking in water
		float waterZ = pEngine->GetWaterLevel(&calc_pos, 0);
		if  (ftop < (waterZ - 1.5f))
			continue;

		IVisArea *pArea;
		int buildingID;
		if (CheckNavigationType(calc_pos,buildingID,pArea,IAISystem::NAV_WAYPOINT_HUMAN) == IAISystem::NAV_WAYPOINT_HUMAN)
			continue;

    if (!IsPointInTriangulationAreas(calc_pos))
      continue;

		if (IsPointInForbiddenRegion(calc_pos, true))
			continue;

    static float edgeTol = 0.3f;
		if (IsPointOnForbiddenEdge(calc_pos, edgeTol))
			continue;

		if (!pCurrent->GetForeignData() && !(pfd.iForeignFlags & PFF_EXCLUDE_FROM_STATIC) && (worldAABB.IsContainSphere(calc_pos, 1.0f)))
		{
			m_pTriangulator->AddVertex(calc_pos.x,calc_pos.y,calc_pos.z, collidable);
			++vertexCounter;
		}
	}

	AILogProgress("Processing >>> %d vertises added  >>> ",vertexCounter);
	vertexCounter = 0;

	// luciano - build hide graph 
	// get static physical entities including also non collidable for hidegraph
	IPhysicalEntity** pHideObstacles;
	int hideCount = m_pWorld->GetEntitiesInBox(min,max,pHideObstacles,ent_static);
	
	pHideTriangulator->m_vVertices.reserve(hideCount);
    
	for (int i = 0 ; i < hideCount ; ++i)
	{
		IPhysicalEntity *pCurrent = pHideObstacles[i];

		pe_params_foreign_data pfd;
		if(pCurrent->GetParams(&pfd)==0)
			continue;
		bool bHidable( (pfd.iForeignFlags & PFF_HIDABLE)!=0 || (pfd.iForeignFlags & PFF_HIDABLE_SECONDARY)!=0 );
		if(!bHidable)
			continue;

		pe_status_pos sp; 
		sp.ipart = -1;
		sp.partid = -1;
		if(pCurrent->GetStatus(&sp)==0)
			continue;

		pe_status_pos status;
		pCurrent->GetStatus(&status);

		Vec3 calc_pos = status.pos;
		// if too flat and too close to the terrain
		float zpos = pEngine->GetTerrainElevation(calc_pos.x,calc_pos.y);
		float ftop = calc_pos.z + status.BBox[1].z;

		if (zpos>ftop)	// skip underground stuff
			continue;
		if (fabs(ftop - zpos) < 0.6f)	// skip stuff too close to the terrain
			continue;

		// don't triangulate objects that your feet wouldn't touch when walking in water
		float waterZ = pEngine->GetWaterLevel(&calc_pos, 0);
		if (ftop < (waterZ - 1.5f))
			continue;

		IVisArea *pArea;
		int buildingID;
		if (CheckNavigationType(calc_pos,buildingID,pArea,IAISystem::NAV_WAYPOINT_HUMAN) == IAISystem::NAV_WAYPOINT_HUMAN)
			continue;

    if (!IsPointInTriangulationAreas(calc_pos))
      continue;

    // Don't discard hide points if they're inside auto-generated forbidden...
    // Note that this might cause problems finding hide spots
		if (IsPointInForbiddenRegion(calc_pos, false))
			continue;

		static float edgeTol = 1.0f;
		if (IsPointOnForbiddenEdge(calc_pos, edgeTol, 0, 0, false))
			continue;

		bool bCollidable( (sp.flagsOR & geom_colltype_solid)!=0 && (pfd.iForeignFlags & PFF_HIDABLE_SECONDARY)==0);
		if (worldAABB.IsContainSphere(calc_pos, 1.0f))
		{
			pHideTriangulator->AddVertex(calc_pos.x, calc_pos.y, zpos,bCollidable); // use terrain z to help debugging
			++vertexCounter;
		}
	}

	AILogProgress("Processing >>> %d hideVertises added  >>> ",vertexCounter);

	for (unsigned i = 0, ni = m_forbiddenAreas.GetShapes().size(); i < ni; ++i)
	{
		const CAIShape* pShape = m_forbiddenAreas.GetShapes()[i];
		const ShapePointContainer& pts = pShape->GetPoints();
		for (unsigned j = 0, nj = pts.size(); j < nj; ++j)
		{
			const Vec3& pos = pts[j];
			if (worldAABB.IsContainSphere(pos, 1.0f))
				m_pTriangulator->AddVertex(pos.x, pos.y, pos.z, true);
		}
	}

	for (unsigned i = 0, ni = m_forbiddenBoundaries.GetShapes().size(); i < ni; ++i)
	{
		const CAIShape* pShape = m_forbiddenBoundaries.GetShapes()[i];
		const ShapePointContainer& pts = pShape->GetPoints();
		for (unsigned j = 0, nj = pts.size(); j < nj; ++j)
		{
			const Vec3& pos = pts[j];
			if (worldAABB.IsContainSphere(pos, 1.0f))
				m_pTriangulator->AddVertex(pos.x, pos.y, pos.z, true);
		}
	}

	// Add some extra points along the beaches to help divide land from sea
  AILogProgress("Adding beach points");
	AddBeachPointsToTriangulator(worldAABB);

	// Generate extra triangles for navigation on breakable surfaces (e.g. ice)
  AILogProgress("Adding breakable region points");
	AddBreakableRegionsToTriangulator(3.0f, worldAABB);

  AILogProgress("Adding dynamic triangle points");
  AddDynamicTrianglesToTriangulator(worldAABB);

	AILogProgress("Starting basic triangulation");

	if (!m_pTriangulator->TriangulateNew())
		return;
	if (!pHideTriangulator->TriangulateNew())
		return;

	TARRAY lstTriangles = m_pTriangulator->GetTriangles();
	m_vVertices = m_pTriangulator->GetVertices();
	m_vTriangles.clear();
	PushFront(m_vTriangles,lstTriangles);

	AILogProgress(" Creation of graph and parsing into file.");
	ParseIntoFile(fileName,m_pGraph,true);

	if (!m_vTriangles.empty())
	{
		std::vector<Tri *>::iterator ti;
		for (ti=m_vTriangles.begin();ti!=m_vTriangles.end(); ++ti)
		{
			Tri *tri = (*ti);
			delete tri;
		}
		m_vTriangles.clear();
	}

	m_vVertices.clear();

	m_pGraph->Validate("CAISystem::GenerateTriangulation after writing nav graph to file", true);
	if (!m_pGraph->mBadGraphData.empty())
	{
		AIWarning("CAISystem: To remove \"invalid passable\" warnings in nav graph, enable \"ai_DebugDraw 72\", disable vegetation etc, and tweak the forbidden region areas close to where you see yellow circles... (for more info see Danny)");
	}

	// now do hide graph

	lstTriangles = pHideTriangulator->GetTriangles();
	m_vVertices = pHideTriangulator->GetVertices();
	m_vTriangles.clear();
	PushFront(m_vTriangles, lstTriangles);

	AILogProgress(" Creation of hide graph and parsing into file.");
	ParseIntoFile(fileNameHide,m_pHideGraph);

	if (!m_vTriangles.empty())
	{
		std::vector<Tri *>::iterator ti;
		for (ti=m_vTriangles.begin();ti!=m_vTriangles.end();++ti)
		{
			Tri *tri = (*ti);
			delete tri;
		}
		m_vTriangles.clear();
	}
	m_vVertices.clear();

	m_pHideGraph->Validate("CAISystem::GenerateTriangulation after writing hide graph to file", false);

	WriteAreasIntoFile(fileNameAreas);

	m_VertexList.WriteToFile(fileNameVerts);

	delete pHideTriangulator;
	delete m_pTriangulator;
	m_pTriangulator = 0;

  // adjust triangulation for occluded triangles
  m_pTriangularNavRegion->DisableOccludedDynamicTriangles();

	m_nNumBuildings = 0;

	ValidateBigObstacles();

  float endTime = gEnv->pTimer->GetAsyncCurTime();
	AILogProgress(" Triangulation finished in %6.1f seconds.", endTime - absoluteStartTime);

	m_navDataState = NDS_OK;
}
#ifdef AI_FP_FAST
#pragma fenv_access(off)
#pragma float_control(precise, off)
#endif

//====================================================================
// ValidateNavigation
//====================================================================
void CAISystem::ValidateNavigation()
{
	m_validationErrorMarkers.clear();
	ValidateAreas();
	ValidateBigObstacles();
}

//====================================================================
// Generate3DDebugVoxels
//====================================================================
void CAISystem::Generate3DDebugVoxels()
{
  m_pVolumeNavRegion->Generate3DDebugVoxels();
}

//====================================================================
// Generate3DVolumes
//====================================================================
void CAISystem::Generate3DVolumes(const char * szLevel, const char * szMission)
{
	if (!szLevel || !szMission)
		return;

  // Danny todo get these from the entities/spawners in the level
  m_3DPassRadii.clear();
  m_3DPassRadii.push_back(3.0f);
  m_3DPassRadii.push_back(1.0f);

	char fileNameVolume[1024];
	sprintf(fileNameVolume, "%s/v3d%s.bai",szLevel,szMission);

	while (!m_lstPathQueue.empty())
	{
    AIWarning( "CAISystem::Generate3DVolumes: Agents were waiting to generate paths, but graph changed. May cause incorrect behaviour.");
		PathFindRequest *pRequest = m_lstPathQueue.front();
		delete pRequest;
		m_lstPathQueue.pop_front();
	}

	m_pVolumeNavRegion->Clear();
	m_pGraph->CheckForEmpty(IAISystem::NAV_VOLUME);
	m_pHideGraph->CheckForEmpty(IAISystem::NAV_VOLUME);

	// let's generate 3D nav volumes
	AILogProgress(" Volume Generation started.");
  m_pVolumeNavRegion->Generate(szLevel, szMission);
	AILogProgress(" Volume Generation complete.");
	// volumes generation over ---------------------
	m_pVolumeNavRegion->WriteToFile(fileNameVolume);
	m_nNumBuildings = 0;
	AILogProgress(" Volumes written to file finished.");
}

//====================================================================
// GenerateFlightNavigation
//====================================================================
void CAISystem::GenerateFlightNavigation(const char * szLevel, const char * szMission)
{
	if (!szLevel || !szMission)
		return;

	char fileNameFlight[1024];
	sprintf(fileNameFlight, "%s/fnav%s.bai",szLevel,szMission);

	while (!m_lstPathQueue.empty())
	{
		AIWarning( "GRAPHRELOADWARNING: Agents were waiting to generate paths, but graph changed. May cause incorrect behaviour.");
		PathFindRequest *pRequest = m_lstPathQueue.front();
		delete pRequest;
		m_lstPathQueue.pop_front();
	}

	m_pFlightNavRegion->Clear();
	m_pGraph->CheckForEmpty(IAISystem::NAV_FLIGHT);
	m_pHideGraph->CheckForEmpty(IAISystem::NAV_FLIGHT);

	AILogProgress(" FlightNavRegion started.");

  bool bypass = true;
	for (SpecialAreaMap::iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
	{
		SpecialArea &sa = si->second;
		if(sa.type == SpecialArea::TYPE_FLIGHT)
      bypass = false;
  }

	if (bypass)
	{
		AILogProgress("Bypassing flight nav region processing");
		IPhysicalEntity**	pObstacles = 0;
		int obstacleCount = 0;
		std::list<SpecialArea*>	shortcuts;
		static size_t childSubDiv = 16;
		static size_t terrainDownSample = 256;
		m_pFlightNavRegion->Process( m_pSystem->GetI3DEngine(), pObstacles, obstacleCount, shortcuts, childSubDiv, terrainDownSample );
	}
	else
	{
		// get only static physical entities (trees, buildings etc...)
		Vec3 min,max;
		IPhysicalEntity**	pObstacles = 0;
		int obstacleCount = 0;
		float fTSize = (float) m_pSystem->GetI3DEngine()->GetTerrainSize();
		
		min.Set(0,0,-100000);
		max.Set(fTSize,fTSize,100000);

		obstacleCount= m_pWorld->GetEntitiesInBox( min, max, pObstacles, ent_static|ent_ignore_noncolliding );

		// Collect shortcuts.
		std::list<SpecialArea*>	shortcuts;
		for (SpecialAreaMap::iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
		{
			SpecialArea &sa = si->second;
			if(sa.type == SpecialArea::TYPE_FLIGHT)
				shortcuts.push_back( &sa );
		}

		m_pFlightNavRegion->Process( m_pSystem->GetI3DEngine(), pObstacles, obstacleCount, shortcuts );
	}
	m_pFlightNavRegion->WriteToFile( fileNameFlight );

	AILogProgress(" FlightNavRegion done.");
}

//====================================================================
// GetVolumeRegionFiles
//====================================================================
const std::vector<string> & CAISystem::GetVolumeRegionFiles(const char * szLevel, const char * szMission)
{
  static std::vector<string> fileNames;
  fileNames.clear();

  VolumeRegions volumeRegions;
  GetVolumeRegions(volumeRegions);

  string name ;
  unsigned nRegions = volumeRegions.size();
  for (unsigned i = 0 ; i < nRegions ; ++i)
  {
    const string& regionName = volumeRegions[i].first;
    /// match what's done in CVolumeNavRegion
    if (regionName.length() > 17)
    {
      name = string(regionName.c_str() + 17);
    }
    else
    {
      AIWarning("CAISystem::GetVolumeRegionFiles region name is too short %s", regionName.c_str());
      continue;
    }

    char fileName[1024];
    sprintf(fileName, "%s/v3d%s-region-%s.bai",szLevel,szMission, name.c_str());

    fileNames.push_back(fileName);
  }
  return fileNames;
}

//====================================================================
// ExportData
//====================================================================
void CAISystem::ExportData(const char *navFileName, const char *hideFileName, 
                           const char *areasFileName, const char *roadsFileName, 
                           const char *waypoint3DSurfaceFileName)
{
	if (navFileName)
	{
	  AILogProgress("[AISYSTEM] Exporting AI Graph to %s", navFileName );
	  m_pGraph->WriteToFile( navFileName );
	}
	if (hideFileName)
	{
	  AILogProgress("[AISYSTEM] Exporting Hide Graph to %s", hideFileName );
	  m_pHideGraph->WriteToFile( hideFileName);
	}
	if (areasFileName)
	{
	  AILogProgress("[AISYSTEM] Exporting Areas to %s", areasFileName );
	  WriteAreasIntoFile(areasFileName);
	}
	if (roadsFileName && m_pRoadNavRegion)
	{
	  AILogProgress("[AISYSTEM] Exporting Roads to %s", roadsFileName );
	  m_pRoadNavRegion->WriteToFile(roadsFileName);
	}
	if (waypoint3DSurfaceFileName && m_pWaypoint3DSurfaceNavRegion)
	{
	  AILogProgress("[AISYSTEM] Exporting 3D surface waypoints to %s", waypoint3DSurfaceFileName );
	  m_pWaypoint3DSurfaceNavRegion->WriteToFile(waypoint3DSurfaceFileName);
	}
  AILogProgress("[AISYSTEM] Finished Exporting data" );
}


//====================================================================
// GetBuildingInfo
//====================================================================
bool CAISystem::GetBuildingInfo(int nBuildingID, SBuildingInfo& info)
{
	const SpecialArea* sa = GetSpecialArea(nBuildingID);
	if (sa)
	{
		info.fNodeAutoConnectDistance = sa->fNodeAutoConnectDistance;
		info.waypointConnections = sa->waypointConnections;
		return true;
	}
	else
	{
		return false;
	}
}

//====================================================================
// ReconnectWaypointNode
//====================================================================
bool CAISystem::IsPointInBuilding(const Vec3& pos, int nBuildingID)
{
	const SpecialArea* sa = GetSpecialArea(nBuildingID);
	if (!sa) return false;
	return IsPointInSpecialArea(pos, *sa);
}

//====================================================================
// ReconnectWaypointNode
//====================================================================
void CAISystem::ReconnectWaypointNodesInBuilding(int nBuildingID)
{
	GetWaypointHumanNavRegion()->ReconnectWaypointNodesInBuilding(nBuildingID);
}

//====================================================================
// ReconnectAllWaypointNodes
//====================================================================
void CAISystem::ReconnectAllWaypointNodes()
{
  ClearBadFloorRecords();

  float startTime = gEnv->pTimer->GetAsyncCurTime();

	// go through all nodes and reevaluate their building ID, and if they're an entrance/exit then also 
	// reset the pass radius
	CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
	CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE);
	I3DEngine *p3dEngine=m_pSystem->GetI3DEngine();
	while (unsigned currentNodeIndex = it.Increment())
	{
		GraphNode* pCurrent = m_pGraph->GetNodeManager().GetNode(currentNodeIndex);

		pCurrent->GetWaypointNavData()->nBuildingID = -1;
		for (SpecialAreaMap::iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
		{
			SpecialArea &sa = si->second;
      if ( ( (sa.type == SpecialArea::TYPE_WAYPOINT_HUMAN && pCurrent->navType == IAISystem::NAV_WAYPOINT_HUMAN) ||
             (sa.type == SpecialArea::TYPE_WAYPOINT_3DSURFACE && pCurrent->navType == IAISystem::NAV_WAYPOINT_3DSURFACE) ) &&
          IsPointInSpecialArea(pCurrent->GetPos(), sa))
      {
				pCurrent->GetWaypointNavData()->nBuildingID = sa.nBuildingID;
        // adjust the entrance/exit radius
        if (pCurrent->navType == IAISystem::NAV_WAYPOINT_HUMAN && 
            pCurrent->GetWaypointNavData()->type == WNT_ENTRYEXIT)
        {
          float radius = 100.0f;
          if (!sa.bVehiclesInHumanNav)
            radius = 2.0f;

					for (unsigned linkg = pCurrent->firstLinkIndex; linkg; linkg = m_pGraph->GetLinkManager().GetNextLink(linkg))
					{
						unsigned nextNodeIndex = m_pGraph->GetLinkManager().GetNextNode(linkg);
						GraphNode* pNextNode = m_pGraph->GetNodeManager().GetNode(nextNodeIndex);
						if ( pNextNode->navType == IAISystem::NAV_TRIANGULAR )
						{
							GraphNode *pOutNode = pNextNode;
							for (unsigned linki=pOutNode->firstLinkIndex; linki; linki = m_pGraph->GetLinkManager().GetNextLink(linki))
							{
								if (m_pGraph->GetNodeManager().GetNode(m_pGraph->GetLinkManager().GetNextNode(linki)) == pCurrent)
									m_pGraph->GetLinkManager().SetRadius(linki, radius);
							}
						}
					} // loop over links
				} // if it's waypoint
				break;
			}
		}
		if (pCurrent->GetWaypointNavData()->nBuildingID == -1)
			AIWarning("Cannot find nav modifier for AI node at (%5.2f, %5.2f, %5.2f)", pCurrent->GetPos().x, pCurrent->GetPos().y, pCurrent->GetPos().z);
	}

  SpecialAreaMap::iterator si;
	for (si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
	{
		SpecialArea &sa = si->second;
		if (sa.waypointConnections > WPCON_DESIGNER_PARTIAL)
		{
			AILogProgress("Reconnecting waypoints in %s", si->first.c_str());
			ReconnectWaypointNodesInBuilding(sa.nBuildingID);
		}
		else
		{
			AILogProgress("Just disconnecting auto-waypoints in %s", si->first.c_str());
      GetWaypointHumanNavRegion()->RemoveAutoLinksInBuilding(sa.nBuildingID);
		}
	}

  AILogProgress("Inserting roads into navigation");
  m_pRoadNavRegion->ReinsertIntoGraph();

  float endTime = gEnv->pTimer->GetAsyncCurTime();

  const TBadFloorRecords &badFloorRecords = GetBadFloorRecords();
  for (TBadFloorRecords::const_iterator bit = badFloorRecords.begin() ; bit != badFloorRecords.end() ; ++bit)
  {
    AIWarning("Bad AI point position at (%5.2f, %5.2f, %5.2f)", bit->pos.x, bit->pos.y, bit->pos.z);
  }


  AILogProgress("Reconnected all waypoints in %5.2f seconds", endTime - startTime);
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::ReleaseFormation(CAIObject* pOwner, bool bDelete)
{
	FormationMap::iterator fi;
	fi = m_mapActiveFormations.find(pOwner);
	if (fi!=m_mapActiveFormations.end())
	{
		CFormation* pFormation = fi->second;
		if(bDelete && pFormation)
		{
			//pFormation->ReleasePoints();
			delete pFormation;
		}
		m_mapActiveFormations.erase(fi);
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::FreeFormationPoint(CAIObject* pOwner)
{
	FormationMap::iterator fi;
	fi = m_mapActiveFormations.find(pOwner);
	if (fi!=m_mapActiveFormations.end())
		(fi->second)->FreeFormationPoint(pOwner);
}

//
//-----------------------------------------------------------------------------------------------------------
IGraph * CAISystem::GetNodeGraph(void)
{
	return (IGraph*) m_pGraph;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::FlushSystem(void)
{
  AILogEvent("CAISystem::FlushSystem");
	FlushSystemNavigation();
	// remove all the leaders first;
	ClearAllLeaders( );
	m_lightManager.Reset();

  m_isPointInForbiddenRegionQueue.Clear();
  m_isPointOnForbiddenEdgeQueue.Clear();

	m_bannedNavSmartObjects.clear();
	m_dynHideObjectManager.Reset();

	if (!m_lstPathQueue.empty())
	{
//		AIWarning("Clearing path request queue during serialisation - should already have been cleared");
		for (PathQueue::iterator it = m_lstPathQueue.begin() ; it != m_lstPathQueue.end() ; ++it)
			delete *it;
		m_lstPathQueue.clear();
	}

	m_mapSpecies.clear();
	m_mapGroups.clear();
  if (!m_mapBeacons.empty())
  {
    BeaconMap::iterator iend = m_mapBeacons.end();
    BeaconMap::iterator binext;
    for (BeaconMap::iterator bi=m_mapBeacons.begin();bi!=iend;)
    {
      binext = bi;
      ++binext;
      RemoveDummyObject((bi->second).pBeacon);
      bi = binext;
    }
    m_mapBeacons.clear();
  }
//	m_mapLeaders.clear();
/*
int sizeObj = m_Objects.size();
	for (AIObjects::iterator it = m_Objects.begin() ; it != m_Objects.end() ; )
	{
CAIObject *pObj(it->second);
		if (0 == it->second->GetEntityID())
		{
			AIWarning("non-entity object during serialisation - should aready have been cleared? <%s>",it->second->GetName());
			RemoveObject(it->second);
			it = m_Objects.begin(); 
		}
		else
		{
			AIWarning("entity object during serialisation - should aready have been cleared? <%s>",it->second->GetName());
			++it;
		}
	}
//*/


	if (!m_Objects.empty())
	{
		AIWarning("CAISystem::FlushSystem: %d objects left after AI system flush, dump follows", m_Objects.size());
		for (AIObjects::iterator it = m_Objects.begin() ; it != m_Objects.end() ; ++it)
		{
			const CAIObject* pAI = it->second;
			AIWarning("  - %s type:%d subtype:%d entId:%x", pAI->GetName(), pAI->GetType(), pAI->GetSubType(), pAI->GetEntityID());
		}
	}
	if (!m_setDummies.empty())
	{
		AIWarning("CAISystem::FlushSystem: %d dummies left after AI system flush, dump follows", m_setDummies.size());
		for (SetAIObjects::iterator it = m_setDummies.begin() ; it != m_setDummies.end() ; ++it)
		{
			const CAIObject* pAI = *it;
			AIWarning("  - %s type:%d subtype:%d entId:%x", pAI->GetName(), pAI->GetType(), pAI->GetSubType(), pAI->GetEntityID());
		}
	}

}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::FlushSystemNavigation(void)
{
  AILogEvent("CAISystem::FlushSystemNavigation");
//	m_vWaitingToBeUpdated.clear();
//	m_vAlreadyUpdated.clear();

	// clear pathfinder
	if (m_pCurrentRequest)
		delete m_pCurrentRequest;
	m_pCurrentRequest =0;

	while(!m_lstPathQueue.empty())
	{
		PathFindRequest *pRequest = (*m_lstPathQueue.begin());
		m_lstPathQueue.pop_front();

		delete pRequest;
	}

	// clear any paths in the puppets
	AIObjects::iterator ai;
	for (ai = m_Objects.find(AIOBJECT_PUPPET);ai!=m_Objects.end();)
	{
		if (ai->first != AIOBJECT_PUPPET) 
			break;
		CPuppet* pPuppet = ai->second->CastToCPuppet();
		if (pPuppet)
			pPuppet->Reset(AIOBJRESET_INIT);
		++ai;
	} 

	for (ai = m_Objects.begin();ai!=m_Objects.end();++ai)
	{
		CAIObject *pObject = ai->second;
		pObject->Reset(AIOBJRESET_INIT);
	} 

	// clear areas id's
	SpecialAreaMap::iterator si = m_mapSpecialAreas.begin();
	while (si!=m_mapSpecialAreas.end())
	{
		m_BuildingIDManager.FreeId((si->second).nBuildingID);
		(si->second).nBuildingID = -1;
		(si->second).bAltered = false;
		++si;
	}
	m_BuildingIDManager.FreeAll();
}



void CAISystem::GetDetectionLevels(IAIObject *pObject, SAIDetectionLevels& levels)
{
	levels.Reset();

	if (!pObject)
		pObject = GetPlayer();
	if (!pObject)
		return;

	if (CAIPlayer* pPlayer = pObject->CastToCAIPlayer())
	{
		levels = pPlayer->GetDetectionLevels();

		int groupId = pPlayer->GetGroupId();

		CLeader* pLeader = GetLeader(groupId);
		if (pLeader && pLeader->GetFollowState() == FS_FOLLOW)
		{
			AIObjects::iterator itObjects = m_mapGroups.find(groupId), itEnd = m_mapGroups.end();
			while (itObjects != itEnd && itObjects->first == groupId)
			{
				CAIObject* pAIObject = itObjects->second;
				++itObjects;

				CPuppet* pPuppet = pAIObject->CastToCPuppet();
				if (pPuppet)
				{
					levels.Append(pPuppet->GetDetectionLevels());
				}
			}
		}
	}
	else if (CPuppet* pPuppet = pObject->CastToCPuppet())
	{
		levels = pPuppet->GetDetectionLevels();
	}
}

/*
//
//-----------------------------------------------------------------------------------------------------------
float CAISystem::GetDetectionValue(IAIObject* pObject)
{
	if (!pObject)
		pObject = GetPlayer();
	if (!pObject)
		return 0;

	float p = 0;
	if (CAIPlayer* pPlayer = pObject->CastToCAIPlayer())
	{
		p = pPlayer->GetDetectionValue();
		int groupId = pPlayer->GetGroupId();

		CLeader* pLeader = GetLeader(groupId);
		if (pLeader && pLeader->GetFollowState() == FS_FOLLOW)
		{
			AIObjects::iterator itObjects = m_mapGroups.find(groupId), itEnd = m_mapGroups.end();
			while (itObjects != itEnd && itObjects->first == groupId)
			{
				CAIObject* pAIObject = itObjects->second;
				++itObjects;

				CPuppet* pPuppet = pAIObject->CastToCPuppet();
				if (pPuppet)
				{
					float q = pPuppet->GetDetectionValue();
					if (p < q)
						p = q;
				}
			}
		}
	}
	else if (CPuppet* pPuppet = pObject->CastToCPuppet())
	{
		p = pPuppet->GetDetectionValue();
	}

	return p;
}

//
//-----------------------------------------------------------------------------------------------------------
float CAISystem::GetAmbientDetectionValue(IAIObject * pObject)
{
	if (!pObject)
		pObject = GetPlayer();
	if (!pObject)
		return 0;

	if (CAIPlayer* pPlayer = pObject->CastToCAIPlayer())
		return pPlayer->GetAmbientDetectionValue();

	return 1.0f;
}
*/

//
//-----------------------------------------------------------------------------------------------------------
int CAISystem::GetAITickCount(void)
{
	return m_nTickCount;
}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::SendAnonymousSignal(int nSignalID, const char * szText, const Vec3 & vPos, float fRadius, IAIObject *pObject, IAISignalExtraData* pData)
{
	AIObjects::iterator ai;
	fRadius*=fRadius;

	// find sending puppet (we need this to know which group to skip when we send the signal)
	int groupid = -1;
	if (pObject)
	{
		CPuppet* pPuppet = pObject->CastToCPuppet();
		if (pPuppet)
			groupid = pPuppet->GetGroupId();
	}

	// go trough all the puppets and vehicles in the surrounding area.
	// still makes precise sq radius check inside because the grid might return
	// objects in a bigger radius 
	SEntityProximityQuery query;	
	query.nEntityFlags = ENTITY_FLAG_HAS_AI;
	query.pEntityClass = NULL;
	float dim = fRadius;
	query.box = AABB(Vec3(vPos.x-dim,vPos.y-dim,vPos.z-dim), Vec3(vPos.x+dim,vPos.y+dim,vPos.z+dim));
	gEnv->pEntitySystem->QueryProximity(query);
	
	//gEnv->pLog->Log("--**--radius=%0.2f,Entities=%d\n",fRadius,query.nCount);
	
	for(int i=0; i<query.nCount; ++i)
	{
		IEntity* pEntity = query.pEntities[i];
		if (pEntity)
		{
			IAIObject *pAI=pEntity->GetAI();
			if (!pAI)
				continue;			

			CPuppet* pPuppet = pAI->CastToCPuppet();
			if ( pPuppet ) 
			{
				if( pPuppet->GetParameters().m_PerceptionParams.perceptionScale.visual>.01f &&
					pPuppet->GetParameters().m_PerceptionParams.perceptionScale.audio>.01f &&
					Distance::Point_PointSq(pPuppet->GetPos(),vPos) < fRadius )
				{
					if (pObject)
						// is sender is not actor - probably will not have entity or scriptTable, so use pPuppet
						pPuppet->SetSignal(nSignalID, szText, pObject->CastToCAIActor() ? pObject->GetEntity() : pPuppet->GetEntity(),
							pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
					else
						pPuppet->SetSignal(nSignalID, szText, NULL, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
				}
			}
		}
	} //i

	/*
	// go trough all the puppets
	for (ai=m_Objects.find(AIOBJECT_PUPPET);ai!=m_Objects.end();)
	{
		if (ai->first != AIOBJECT_PUPPET)
			break;
		CPuppet* pPuppet = ai->second->CastToCPuppet();
		if ( pPuppet ) 
		{
			if( pPuppet->GetParameters().m_PerceptionParams.perceptionScale.visual>.01f &&
					pPuppet->GetParameters().m_PerceptionParams.perceptionScale.audio>.01f &&
					Distance::Point_PointSq(pPuppet->GetPos(),vPos) < fRadius )
			{
				if (pObject)
					pPuppet->SetSignal(nSignalID, szText,pObject->GetEntity(), pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
				else
					pPuppet->SetSignal(nSignalID, szText, NULL, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
			}
		}
		++ai;
	}

	// go trough all the vehicles
	for (ai=m_Objects.find(AIOBJECT_VEHICLE);ai!=m_Objects.end();)
	{
		if (ai->first != AIOBJECT_VEHICLE)
			break;
		CAIVehicle* pVehicle = ai->second->CastToCAIVehicle();
		if ( pVehicle ) 
		{
			if ( Distance::Point_PointSq(pVehicle->GetPos(),vPos) < fRadius )
			{
				if (pObject)
					pVehicle->SetSignal(nSignalID, szText,pObject->GetEntity(), pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
				else
					pVehicle->SetSignal(nSignalID, szText, NULL, pData ? new AISignalExtraData(*(AISignalExtraData*)pData) : NULL );
			}
		}
		++ai;
	}
	*/

	// finally delete pData since all recipients have their own copies
	if ( pData )
		delete (AISignalExtraData*)pData;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::ReleaseFormationPoint(CAIObject * pReserved)
{
	if (m_mapActiveFormations.empty())
		return;

	FormationMap::iterator fi;
	CAIActor* pActor = pReserved->CastToCAIActor();
	CLeader* pLeader = GetLeader(pActor);
	if(pLeader)
		pLeader->FreeFormationPoint(pReserved);
	else
		for (fi=m_mapActiveFormations.begin();fi!=m_mapActiveFormations.end();++fi)
			(fi->second)->FreeFormationPoint(pReserved);

}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::UpdateBeacon(unsigned short nGroupID, const Vec3 & vPos, IAIObject *pOwner)
{
	Vec3 pos = vPos;
	ray_hit hit;
	
	CAIActor* pOwnerActor = CastToCAIActorSafe(pOwner);
//FIXME: this should never happen! 
	if(!pOwner)
	{
		AIAssert(false);
		return;
	}
	/*
	int building;
	IVisArea *pArea;
	IAISystem::ENavigationType navType(IAISystem::NAV_TRIANGULAR);
	bool b2D = true;
	if(pOwnerActor && pOwnerActor->GetMovementAbility().b3DMove)
	{
		navType = CheckNavigationType(vPos,building,pArea,pOwnerActor->GetMovementAbility().pathfindingProperties.navCapMask );
		b2D = 0 != (navType &(IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN));
	}
	if( b2D )
	{
		int rayresult(0);
TICK_COUNTER_FUNCTION
TICK_COUNTER_FUNCTION_BLOCK_START
		rayresult = m_pWorld->RayWorldIntersection(pos, Vec3(0,0,-20), ent_terrain|ent_static, rwi_stop_at_pierceable, &hit, 1);
TICK_COUNTER_FUNCTION_BLOCK_END
		if (rayresult && hit.dist > 0)
			pos = hit.pt;
	}
	*/
	BeaconMap::iterator bi;
	bi = m_mapBeacons.find(nGroupID);

	if ( bi== m_mapBeacons.end())
	{
		BeaconStruct bs;
    char name[32];
    sprintf(name,"BEACON_%d", nGroupID);
		CAIObject *pObject = CreateDummyObject(name);
		pObject->SetPos(pos);
    pObject->SetMoveDir(pOwner->GetMoveDir());
    pObject->SetBodyDir(pOwner->GetBodyDir());
		pObject->SetViewDir(pOwner->GetViewDir());
		pObject->SetType(AIOBJECT_WAYPOINT);
		pObject->SetSubType(IAIObject::STP_BEACON);

		pObject->SetGroupId(nGroupID);
		bs.pBeacon = pObject;
		bs.pOwner = NULL;//pOwner;
		m_mapBeacons.insert(BeaconMap::iterator::value_type(nGroupID,bs));
	}
	else
	{
		// beacon found, update its position
		(bi->second).pBeacon->SetPos(pos);
    (bi->second).pBeacon->SetBodyDir(pOwner->GetBodyDir());
    (bi->second).pBeacon->SetMoveDir(pOwner->GetMoveDir());
		(bi->second).pBeacon->SetViewDir(pOwner->GetViewDir());
		(bi->second).pOwner = NULL;//pOwner; 
	}
}

//
//-----------------------------------------------------------------------------------------------------------
IAIObject * CAISystem::GetBeacon(unsigned short nGroupID)
{
	BeaconMap::iterator bi;
	bi = m_mapBeacons.find(nGroupID);
	if ( bi== m_mapBeacons.end())
		return NULL;

	return (bi->second).pBeacon;
}

//
//-----------------------------------------------------------------------------------------------------------
int CAISystem::GetBeaconGroupId(CAIObject* pBeacon)
{
	BeaconMap::iterator bi, biEnd = m_mapBeacons.end();
	
	for(bi = m_mapBeacons.begin(); bi != biEnd; ++bi)
		if ( bi->second.pBeacon == pBeacon)
			return bi->first;
	return -1;
}

//====================================================================
// CancelAnyPathsFor
//====================================================================
void CAISystem::CancelAnyPathsFor(CPipeUser* pRequester, bool actorRemoved)
{
  AILogComment("Cancelling paths for %s", pRequester->GetName());
  if (m_pCurrentRequest)
  {
    if ((actorRemoved || m_pCurrentRequest->type == PathFindRequest::TYPE_ACTOR) &&
				m_pCurrentRequest->pRequester == pRequester)
    {
			if (m_pCurrentRequest->id)
				NotifyPathFinderListeners(m_pCurrentRequest->id, 0);
      delete m_pCurrentRequest;
      m_pCurrentRequest = 0;
      m_pAStarSolver->AbortAStar();
    }
  }

  PathQueue::iterator pi;
  for (pi=m_lstPathQueue.begin();pi!=m_lstPathQueue.end();)
  {
		PathFindRequest* req = *pi;
    if ((actorRemoved || req->type == PathFindRequest::TYPE_ACTOR) && req->pRequester == pRequester)
		{
			if (req->id)
				NotifyPathFinderListeners(req->id, 0);
			delete *pi;
      pi = m_lstPathQueue.erase(pi);
		}
    else
		{
      ++pi;
		}
  }
}

//====================================================================
// CancelPath
//====================================================================
void CAISystem::CancelPath(int id)
{
	if (id <= 0)
		return;

	AILogComment("Cancelling path #%d", id);
	if (m_pCurrentRequest)
	{
		if (m_pCurrentRequest->id == id)
		{
			delete m_pCurrentRequest;
			m_pCurrentRequest = 0;
			m_pAStarSolver->AbortAStar();
		}
	}

	PathQueue::iterator pi;
	for (pi=m_lstPathQueue.begin();pi!=m_lstPathQueue.end();)
	{
		if ((*pi)->id == id)
		{
			delete *pi;
			pi = m_lstPathQueue.erase(pi);
		}
		else
		{
			++pi;
		}
	}
}

//====================================================================
// IsFindingPathFor
//====================================================================
bool CAISystem::IsFindingPathFor(const CAIObject* pRequester) const
{
	if (m_pCurrentRequest && m_pCurrentRequest->type == PathFindRequest::TYPE_ACTOR &&
			m_pCurrentRequest->pRequester == pRequester)
    return true;

  PathQueue::const_iterator pi;
  for (pi=m_lstPathQueue.begin();pi!=m_lstPathQueue.end();++pi)
  {
    if ((*pi)->type == PathFindRequest::TYPE_ACTOR && (*pi)->pRequester == pRequester)
      return true;
  }
  return false;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::SetAssesmentMultiplier(unsigned short type, float fMultiplier)
{
	if (fMultiplier <= 0.f)
		fMultiplier = 0.01f;
	MapMultipliers::iterator mi;
	
	mi = m_mapMultipliers.find(type);
	if (mi == m_mapMultipliers.end())
		m_mapMultipliers.insert(MapMultipliers::iterator::value_type(type, fMultiplier));
	else
		mi->second = fMultiplier;

	if (std::find(m_priorityObjectTypes.begin(), m_priorityObjectTypes.end(), type) == m_priorityObjectTypes.end())
		m_priorityObjectTypes.push_back(type);
}




//
//-----------------------------------------------------------------------------------------------------------
IAIObject * CAISystem::GetNearestToObjectInRange(IAIObject * pRef, unsigned short nType, float fRadiusMin, 
																								 float fRadiusMax, float inCone, bool bFaceAttTarget, bool bSeesAttTarget, bool bDevalue )
{
	AIObjects::iterator ai;
	ai = m_Objects.find(nType);

	float	fRadiusMinSQR = fRadiusMin*fRadiusMin;
	float	fRadiusMaxSQR = fRadiusMax*fRadiusMax;

	if (ai == m_Objects.end()) 
		return NULL;

	CPipeUser* pPipeUser = pRef->CastToCPipeUser();
	CAIObject *pAttTarget = 0;
	if (pPipeUser)
		pAttTarget = (CAIObject*)pPipeUser->GetAttentionTarget();
	
	IAIObject *pRet = 0;
	CPuppet* pPuppet = pRef->CastToCPuppet();
	float	eyeOffset = 0.f;
	if(pPuppet && bSeesAttTarget)
	{
		SAIBodyInfo bi;
		pPuppet->GetProxy()->QueryBodyInfo(STANCE_CROUCH, 0.f, true, bi);
		eyeOffset = bi.vEyePos.z - pPuppet->GetEntity()->GetWorldPos().z;
	}
	float maxdot = -1;
	float maxdist = std::numeric_limits<float>::max();
	Vec3 pos = pRef->GetPos();
	for (;ai!=m_Objects.end();++ai)
	{
		if (ai->first != nType)
			break;
		if(!(ai->second)->IsEnabled())
			continue;
		Vec3 ob_pos = (ai->second)->GetPos();
		Vec3 ob_dir = ob_pos-pos;
		float f = ob_dir.GetLengthSquared();

		// only consider objects in cone
		if(inCone>0.0f)
		{
			ob_dir.Normalize();
			float	dot = pRef->GetMoveDir().Dot(ob_dir);
			if( dot < inCone )
				continue;
			// Favor points which have tighter hide spot.
			float	a = 0.5f + 0.5f * (1.0f - max(0.0f, dot));
			f *= a * a;
		}

		if( bFaceAttTarget && pAttTarget )
		{
			Vec3	dirToAtt = pAttTarget->GetPos() - ob_pos;
			Vec3	coverDir = (ai->second)->GetMoveDir();
			dirToAtt.NormalizeSafe();
			if( dirToAtt.Dot( coverDir ) < 0.3f )	// let's make it 70 degree threshold
				continue;
		}

/*
		float fdot = 1;
		float fFrontDot=-1;
		if (pAttTarget)
		{
			Vec3 dir = pAttTarget->GetPos() - ob_pos;
			dir.Normalize();

			Matrix44 m;
			m.SetIdentity();
			//m.RotateMatrix_fix((ai->second)->GetAngles());
			m=GetRotationZYX44( - (ai->second)->GetAngles() * gf_DEGTORAD )*m; //NOTE: angles in radians and negated 
		  //POINT_CHANGED_BY_IVO 
			//Vec3 orie = m.TransformPoint(Vec3(0,-1,0));
			Vec3 orie = GetTransposed44(m) * Vec3(0,-1,0);

			fdot = dir.Dot(orie);

			Vec3 my_dir = GetNormalized(ob_pos - pos);
			fFrontDot = my_dir.Dot(GetNormalized(pAttTarget->GetPos()-pos));
		}

		if (fFrontDot<0.001f)
			continue;
*/
		if (pPuppet)
		{
			if (pPuppet->IsDevalued(ai->second))
				continue;
		}
		//use anchor radius ---------------------
		float	fCurRadiusMaxSQR( (ai->second)->GetRadius()<0.01f ? fRadiusMaxSQR : (fRadiusMax+(ai->second)->GetRadius())*(fRadiusMax+(ai->second)->GetRadius()));
		if ( f>fRadiusMinSQR && f<fCurRadiusMaxSQR)//&& (fdot>maxdot))
		{
			if ((f < maxdist))
			{
				if(bSeesAttTarget && pAttTarget)
				{

					IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();
					int rwiResult(0);
					ray_hit hit;
					TickCountStartStop();
					float terrainZ = gEnv->p3DEngine->GetTerrainElevation(ob_pos.x, ob_pos.y);
					Vec3	startTracePos( ob_pos.x, ob_pos.y, terrainZ+eyeOffset );
					Vec3	ob_at_dir = pAttTarget->GetPos() - startTracePos;
					std::vector<IPhysicalEntity*> skipList;
					pPuppet->GetPhysicsEntitiesToSkip(skipList);
					if(pAttTarget && pAttTarget->CastToCAIActor())
						pAttTarget->CastToCAIActor()->GetPhysicsEntitiesToSkip(skipList);
					rwiResult = pPhysWorld->RayWorldIntersection( startTracePos, ob_at_dir, ent_static|ent_terrain|ent_sleeping_rigid, rwi_stop_at_pierceable, &hit, 1,
																												skipList.empty() ? 0 : &skipList[0], skipList.size());
					TickCountStartStop(false);
					if(rwiResult)
						continue;
				}
				pRet = ai->second;
				maxdist = f;
//				maxdot = fdot;
			}
		}
	}
	if (pRet)
	{
		if(!bDevalue )
			Devalue( pRef, pRet, true, .05f );	// no devalue - just make sure it's not used in the same update again
		else
			Devalue( pRef, pRet, true );
	}
	return pRet; 
} 


//
//-----------------------------------------------------------------------------------------------------------
IAIObject * CAISystem::GetRandomObjectInRange(IAIObject * pRef, unsigned short nType, float fRadiusMin, float fRadiusMax, bool bFaceAttTarget)
{
	// Make sure there is at least one object of type present.
	AIObjects::iterator ai;
	ai = m_Objects.find(nType);
	if (ai == m_Objects.end()) 
		return NULL;

	CPipeUser* pPipeUser = pRef->CastToCPipeUser();
	IAIObject *pAttTarget = 0;
	if (bFaceAttTarget && pPipeUser)
			pAttTarget = pPipeUser->GetAttentionTarget();

	// Collect all the points withing the given range.
	const float	fRadiusMinSQR = fRadiusMin*fRadiusMin;
	const float	fRadiusMaxSQR = fRadiusMax*fRadiusMax;
	std::list<IAIObject*>	lstObjectsInRange;
	Vec3 pos = pRef->GetPos();
	CPuppet* pPuppet = pRef->CastToCPuppet();
	for (;ai!=m_Objects.end();++ai)
	{
		// Skip objects of wrong type.
		if (ai->first != nType)
			break;
		// Skip disable objects.
		if(!(ai->second)->IsEnabled())
			continue;
		// Skip devalued objects.
		if (pPuppet)
		{
			if (pPuppet->IsDevalued(ai->second))
				continue;
		}
		// check if facing target
		if(pAttTarget)
		{
			Vec3 candidate2Target(pAttTarget->GetPos()-ai->second->GetPos());
			float	dotS(ai->second->GetMoveDir()*candidate2Target);
			if(dotS<0.f)
				continue;
		}
		// Skip objects out of range.
		Vec3 ob_pos = (ai->second)->GetPos();
		Vec3 ob_dir = ob_pos-pos;
		float f = ob_dir.GetLengthSquared();
		//use anchor radius ---------------------
		float	fCurRadiusMaxSQR( (ai->second)->GetRadius()<0.01f ? fRadiusMaxSQR : (fRadiusMax+(ai->second)->GetRadius())*(fRadiusMax+(ai->second)->GetRadius()));
		if ( f>fRadiusMinSQR && f<fCurRadiusMaxSQR )
			lstObjectsInRange.push_back( ai->second );
	}

	// Choose random object.
	IAIObject *pRet = 0;
	if( !lstObjectsInRange.empty() )
	{
		int	choice = ai_rand()%lstObjectsInRange.size();
		std::list<IAIObject*>::iterator randIt = lstObjectsInRange.begin();
		std::advance( randIt, choice );
		if( randIt != lstObjectsInRange.end() )
			pRet = (*randIt);
	}

	if (pRet && pPuppet)
		pPuppet->Devalue( pRet, false, 2.f);
//		Devalue( pRef, pRet, true );

	return pRet; 
} 

//-----------------------------------------------------------------------------------------------------------
IAIObject * CAISystem::GetBehindObjectInRange(IAIObject * pRef, unsigned short nType, float fRadiusMin, float fRadiusMax)
{
	// Find an Object to escape from a target. 04/11/05 tetsuji

	// Make sure there is at least one object of type present.
	AIObjects::iterator ai;
	ai = m_Objects.find(nType);
	if (ai == m_Objects.end()) 
		return NULL;

	CPipeUser* pPipeUser = pRef->CastToCPipeUser();
	CAIObject *pAttTarget = 0;
	if (pPipeUser)
		pAttTarget = (CAIObject*)pPipeUser->GetAttentionTarget();

	// Collect all the points withing the given range.
	const float	fRadiusMinSQR = fRadiusMin*fRadiusMin;
	const float	fRadiusMaxSQR = fRadiusMax*fRadiusMax;
	std::list<IAIObject*>	lstObjectsInRange;

	Vec3 pos = pRef->GetPos();
	Vec3 vForward = pRef->GetMoveDir();

	vForward.SetLength(10.0);

	// If no att target, I assume the target is the front point of the object.
	// 20/12/05 Tetsuji
	Vec3 vTargetPos = pAttTarget ? pAttTarget->GetPos() : pos + vForward;
	Vec3 vTargetToPos = pos - vTargetPos;

	if(!pPipeUser->GetMovementAbility().b3DMove)
		vTargetToPos.z = 0.f;
	vTargetToPos.NormalizeSafe();

	for (;ai!=m_Objects.end();++ai)
	{
		// Skip objects of wrong type.
		if (ai->first != nType)
			break;
		// Skip disable objects.
		if(!(ai->second)->IsEnabled())
			continue;
		// Skip devalued objects.
		CPuppet* pPuppet = pRef->CastToCPuppet();
		if (pPuppet)
		{
			if (pPuppet->IsDevalued(ai->second))
				continue;
		}
		// Skip objects out of range.
		Vec3 ob_pos = (ai->second)->GetPos();
		Vec3 ob_dir = ob_pos-pos;
		float f = ob_dir.GetLengthSquared();
		//use anchor radius ---------------------
		float	fCurRadiusMaxSQR( (ai->second)->GetRadius()<0.01f ? fRadiusMaxSQR : (fRadiusMax+(ai->second)->GetRadius())*(fRadiusMax+(ai->second)->GetRadius()));
		if ( f>fRadiusMinSQR && f<fCurRadiusMaxSQR )
			lstObjectsInRange.push_back( ai->second );
	}

	// Choose object. 
	IAIObject *pRet = 0;
	float maxdot = -10.0f;

	if( !lstObjectsInRange.empty() )
	{
		std::list<IAIObject*>::iterator choiceIt = lstObjectsInRange.begin();
		std::list<IAIObject*>::iterator choiceItEnd = lstObjectsInRange.end();
		for (;choiceIt!=choiceItEnd;++choiceIt)
		{
			IAIObject *pObj =*choiceIt;
			Vec3 vPosToObj =pObj->GetPos() - pos;
			CPuppet* pPuppet = pRef->CastToCPuppet();
			if(pPuppet && !pPuppet->GetMovementAbility().b3DMove)
				vPosToObj.z = 0.f;

			if (pPuppet && !pPuppet->CheckFriendsInLineOfFire(vPosToObj, true))
					continue;

			float dot =vPosToObj.Dot(vTargetToPos);
			if(dot < .2f)
				continue;
			if(dot>maxdot)
			{
				maxdot =dot;
				pRet =pObj;
			}
		}
	}

	if (pRet)
		Devalue( pRef, pRet, true );

	return pRet; 
} 


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::Devalue( IAIObject* pRef, IAIObject* pObject, bool group, float fDevalueTime )
{
	if( !pRef || !pObject )
		return;

	CPipeUser* pPipeUser = pRef->CastToCPipeUser();
	if (!pPipeUser)
		return;

	if( group )
	{
		// Devalue the object for whole group.
		int myGroup = pPipeUser->GetGroupId();
		AIObjects::iterator gri = m_mapGroups.find(myGroup);
		for( ; gri != m_mapGroups.end(); ++gri )
		{
			if (gri->first != myGroup)
				break;
			CPuppet* pGroupMember = gri->second->CastToCPuppet();
			if (pGroupMember)
				pGroupMember->Devalue((CAIObject*)pObject, false, fDevalueTime);
		}
	}
	else
	{
		// Devalue the object for one puppet
		if ( CPuppet *pPuppet = pRef->CastToCPuppet() )
			pPuppet->Devalue((CAIObject*)pObject, false, fDevalueTime);
	}

}


int debugForbiddenCounter = 0;
bool doForbiddenDebug = false;

//
//-----------------------------------------------------------------------------------------------------------
#ifdef AI_FP_FAST
#pragma float_control(precise, on)
#pragma fenv_access(on)
#endif
void CAISystem::AddForbiddenArea(CAIShape* pShape)
{
	ListNodeIds lstNewNodes;
	ListNodeIds lstOldNodes;
	NewCutsVector	newCutsVector;

	const ShapePointContainer& pts = pShape->GetPoints();
	for (unsigned i = 0, ni = pts.size(); i < ni; ++i)
	{
		++debugForbiddenCounter;
		Vec3r vStart = pts[i];

		lstNewNodes.clear();
		lstOldNodes.clear();

		newCutsVector.resize(0);
		// create edge  
		Vec3r vEnd = pts[(i+1)%ni];
		vEnd.z = vStart.z = 0;

		ListNodeIds nodesToRefine;	
		CreatePossibleCutList(vStart, vEnd, nodesToRefine);

		if (doForbiddenDebug)
			AILogAlways("%d: CreatePossibleCutList generates %d", debugForbiddenCounter, nodesToRefine.size());

		for (ListNodeIds::iterator nI = nodesToRefine.begin(); nI != nodesToRefine.end(); ++nI)
			RefineTriangle(m_pGraph->GetNodeManager(), m_pGraph->GetLinkManager(), (*nI), vStart, vEnd, lstNewNodes, lstOldNodes, newCutsVector);

		if (doForbiddenDebug)
			AILogAlways("%d: RefineTriangle generates %d %d %d", debugForbiddenCounter, lstNewNodes.size(), lstOldNodes.size(), newCutsVector.size());

		static int cutOff = 1;
		if( newCutsVector.size()==1 && lstNewNodes.size()<cutOff )	// nothing was really changed
			continue;

		m_pGraph->ClearTags();

		ListNodeIds::iterator di;

		// Danny - to be honest I don't understand exactly how these lists
		// are used, but I found that sometimes the same node could be in both 
		// the old and new lists. During Disconnect the node could get invalidated,
		// resulting in much badness - crashes and incorrect graph. So, if it's 
		// in both lists removing it from the "old" list seems to work and generates
		// a correct graph
		unsigned origOldSize = lstOldNodes.size();
		for (di = lstNewNodes.begin() ; di != lstNewNodes.end() ; ++di)
			lstOldNodes.remove(*di);
		unsigned newOldSize = lstOldNodes.size();

		for (di = lstNewNodes.begin() ; di != lstNewNodes.end() ; )
		{
			GraphNode *pNode = m_pGraph->GetNodeManager().GetNode(*di);
			if (pNode->navType != IAISystem::NAV_TRIANGULAR)
				di = lstNewNodes.erase(di); // should never happen - but the safe first node (NAV_UNSET) was getting in?
			else
				++di;
		}

		// delete old triangles
		for (di=lstOldNodes.begin() ; di != lstOldNodes.end() ; ++di)
			m_pGraph->Disconnect((*di));

		m_pGraph->ConnectNodes(lstNewNodes);
	}
}
#ifdef AI_FP_FAST
#pragma fenv_access(off)
#pragma float_control(precise, off)
#endif

//
//-----------------------------------------------------------------------------------------------------------
#ifdef AI_FP_FAST
#pragma float_control(precise, on)
#pragma fenv_access(on)
#endif
void CAISystem::AddForbiddenAreas()
{ 
  debugForbiddenCounter = 0;

	for (unsigned i = 0, ni = m_forbiddenAreas.GetShapes().size(); i < ni; ++i)
	{
		++debugForbiddenCounter;
		AddForbiddenArea(m_forbiddenAreas.GetShapes()[i]);
	}

	for (unsigned i = 0, ni = m_forbiddenBoundaries.GetShapes().size(); i < ni; ++i)
	{
		++debugForbiddenCounter;
		AddForbiddenArea(m_forbiddenBoundaries.GetShapes()[i]);
	}

}
#ifdef AI_FP_FAST
#pragma fenv_access(off)
#pragma float_control(precise, off)
#endif

//
//-----------------------------------------------------------------------------------------------------------
#ifdef AI_FP_FAST
#pragma float_control(precise, on)
#pragma fenv_access(on)
#endif
void CAISystem::RefineTriangle(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, unsigned nodeIndex, const Vec3r &start, const Vec3r &end, ListNodeIds &lstNewNodes, ListNodeIds &lstOldNodes, NewCutsVector &newCutsVector)
{
	GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);

	if (doForbiddenDebug)
		AILogAlways("%d: RefineTriangle start (%7.4f, %7.4f) end (%7.4f, %7.4f) for tri at (%5.2f %5.2f)", 
		debugForbiddenCounter, start.x, start.y, end.x, end.y, pNode->GetPos().x, pNode->GetPos().y);

	int	newNodesCount = lstNewNodes.size();

	Vec3r D0,P0;

	Vec3r newStart = start;
	real maximum_s = -epsilon;
	real aux_maximum_s = -epsilon;

	// parametric equation of new edge
	P0 = start;
	D0 = end-start;

	Vec3r vCut1,vCut2;

	int FirstCutStart=-1, FirstCutEnd=-1;
	int SecondCutStart=-1, SecondCutEnd=-1;
	int StartCutStart=-1, StartCutEnd=-1;
	int EndCutStart=-1, EndCutEnd=-1;
	int TouchStart=-1, TouchEnd=-1;

	unsigned int index,next_index;
	for (index=0;index<pNode->GetTriangularNavData()->vertices.size();++index)
	{
		Vec3r P1,D1;
		next_index = index+1;
		if (next_index == pNode->GetTriangularNavData()->vertices.size())		// make sure we wrap around correctly
			next_index = 0;

		//get the triangle edge
		P1=m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[index]).vPos;
		D1=m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[next_index]).vPos - P1;

		// calculate intersection parameters
		real s=-1;	// parameter on new edge
		real t=-1;	// parameter on triangle edge
		{
			Vec3r delta = P1-P0;
			real crossD = D0.x*D1.y-D0.y*D1.x;
			real crossDelta1 = delta.x*D1.y-delta.y*D1.x;
			real crossDelta2 = delta.x*D0.y-delta.y*D0.x;

			if ( fabs(crossD) > epsilon )
			{
				// intersection
				s=crossDelta1/crossD;
				t=crossDelta2/crossD;
			}
			else
			{
				TouchStart =index;
				TouchEnd = next_index;
			}

		}

		if (doForbiddenDebug)
			AILogAlways("%d: index = %d, s = %5.2f t = %5.2f", debugForbiddenCounter, index, s, t);

		// for any reasonable calculation both s and t must be between 0 and 1
		// everything else is a non desired situation
		if ((t>-epsilon) && (t<1.f+epsilon) )
		{
			// calculate the point of intersection
			Vec3r result = P0 + D0*s;
			if (s>maximum_s)
				maximum_s = s;

			// s < 0	
			if (s < -epsilon)
			{
				if (doForbiddenDebug)
					AILogAlways("%d: start clean triangle", debugForbiddenCounter);
				// a clean start triangle
				StartCutStart = index;		
				StartCutEnd	  = next_index;		
			}

			// s == 0  or s == 1
			if ( (fabs(s) < epsilon) || ((s>1.f-epsilon) && (s<1.f+epsilon)) )
			{
				if (doForbiddenDebug)
					AILogAlways("%d: s == 0 or s == 1", debugForbiddenCounter);
				// the start coincides with a triangle vertex or lies on a triangle side
				if ( (t>epsilon) && (t<1.f-epsilon) )
				{
					// the start lies on a triangle side
					if (FirstCutStart<0)
					{
						FirstCutStart = index;
						FirstCutEnd   = next_index;
						vCut1 = result;
					}
					else
					{
						SecondCutStart= index;
						SecondCutEnd	= next_index;
						vCut2 = result;
					}
				} 
				// if its in the triangle vertex then just skip
			}


			// s between 0 and 1
			if ( (s>epsilon) && (s<1.f-epsilon))
			{
				// a normal cut or new edge coincides with a vertex on the triangle
				if ( (fabs(t) < epsilon) || (t>1.f-epsilon) )
				{
					//the edge coincides with a triangle vertex
					// skip this case
					int a=5;
				}
				else
				{
					// a normal cut
					if (FirstCutStart<0)
					{
						FirstCutStart = index;
						FirstCutEnd   = next_index;
						vCut1 = result;
					}
					else
					{
						SecondCutStart= index;
						SecondCutEnd	= next_index;
						vCut2 = result;
					}
				}
			}

			// s bigger then 1
			if ( s>1.f+epsilon )
			{
				// a clear end situation
				EndCutStart = index;
				EndCutEnd		= next_index;
			}

		}

		aux_maximum_s=s;

	} // end for

	ObstacleData od1,od2;

	if (doForbiddenDebug)
		AILogAlways("%d: creating new triangles StartCutStart = %d EndCutStart = %d", debugForbiddenCounter, StartCutStart, EndCutStart);


	// now create the new triangles
	if (StartCutStart >= 0)
	{
		++debugForbiddenCounter;
		// start is in this triangle
		// triangle: start vertex and edge that it cuts
		od1.vPos = start; 
		od2.vPos=end;
		CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[StartCutStart]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[StartCutEnd]), false, lstNewNodes);

		int notStart	= 3-(StartCutStart+StartCutEnd);

		if (EndCutStart >= 0)
		{
			++debugForbiddenCounter;
			// and end also
			// triangle: end vertex and edge that it cuts
			CreateNewTriangle(od2,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[EndCutStart]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[EndCutEnd]), false, lstNewNodes);
			// triangle: start vertex end vertex and both end vertices
			CreateNewTriangle(od1,od2,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[EndCutStart]), true, lstNewNodes);
			CreateNewTriangle(od1,od2,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[EndCutEnd]), true, lstNewNodes);
			AddTheCut( m_VertexList.FindVertex(od1), m_VertexList.FindVertex(od2), newCutsVector );

			int notEnd		= 3-(EndCutStart+EndCutEnd);
			CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notStart]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notEnd]), false, lstNewNodes);
		}
		else
		{
			++debugForbiddenCounter;
			if (FirstCutStart >= 0)
			{
				++debugForbiddenCounter;
				// simple start-cut case
				od2.vPos = vCut1;
				// triangle: start vertex, cut pos and both cut vertices
				CreateNewTriangle(od1,od2,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[FirstCutStart]), true, lstNewNodes);
				CreateNewTriangle(od1,od2,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[FirstCutEnd]), true, lstNewNodes);
				AddTheCut( m_VertexList.FindVertex(od1), m_VertexList.FindVertex(od2), newCutsVector );
				// find index of vertex that does not lie on cut edge
				int notCut = 3-(FirstCutEnd+FirstCutStart);
				// triangle: start vertex, notcut and notstart
				CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notStart]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notCut]), false, lstNewNodes);
			}
			else
			{
				++debugForbiddenCounter;
				// nasty: start ok but end or cut into a vertex
				// two more triangles
				if (od1.vPos != m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notStart]).vPos)
				{
					++debugForbiddenCounter;
					// od1 and notStart
					CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[StartCutStart]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notStart]), true, lstNewNodes);
					CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[StartCutEnd]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notStart]), true, lstNewNodes);
					AddTheCut( m_VertexList.FindVertex(od1), pNode->GetTriangularNavData()->vertices[notStart], newCutsVector );
				}
			}
		}
	}
	else
	{
		++debugForbiddenCounter;

		// start is not in this triangle

		if (EndCutStart >= 0)
		{
			++debugForbiddenCounter;
			// but end is
			od1.vPos = end; 
			od2.vPos = vCut1;

			CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[EndCutStart]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[EndCutEnd]), false, lstNewNodes);
			int notEnd = 3 - (EndCutStart + EndCutEnd);
			if (FirstCutStart >= 0)
			{
				++debugForbiddenCounter;
				// simple cut-end case
				CreateNewTriangle(od1,od2,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[FirstCutStart]), true, lstNewNodes);
				CreateNewTriangle(od1,od2,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[FirstCutEnd]), true, lstNewNodes);
				AddTheCut( m_VertexList.FindVertex(od1), m_VertexList.FindVertex(od2), newCutsVector );
				int notCut = 3 - (FirstCutStart + FirstCutEnd);
				CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notEnd]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notCut]), false, lstNewNodes);
			}
			else
			{
				++debugForbiddenCounter;
				// od1 and notEnd
				// end ok but cut in vertex
				CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notEnd]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[EndCutStart]), true, lstNewNodes);
				CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notEnd]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[EndCutEnd]), true, lstNewNodes);
				AddTheCut( m_VertexList.FindVertex(od1), pNode->GetTriangularNavData()->vertices[notEnd], newCutsVector );
			}
		}
		else
		{
			++debugForbiddenCounter;
			// this triangle contains no start and no end
			if (FirstCutStart >= 0)
			{
				++debugForbiddenCounter;
				od1.vPos = vCut1;
				int notCut1 = 3-(FirstCutStart+FirstCutEnd);
				if (SecondCutStart >= 0)
				{
					++debugForbiddenCounter;
					od2.vPos = vCut2;
					// simple cut-cut case
					// find shared vertex
					int SHARED;
					if (FirstCutStart == SecondCutStart)
						SHARED = FirstCutStart;
					else
					{
						if (FirstCutStart == SecondCutEnd)
							SHARED = FirstCutStart;
						else
							SHARED = FirstCutEnd;
					}

					int notCut2 = 3-(SecondCutStart+SecondCutEnd);

					CreateNewTriangle(od1,od2,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[SHARED]), true, lstNewNodes);
					CreateNewTriangle(od1,od2,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notCut1]), true, lstNewNodes);
					AddTheCut(m_VertexList.FindVertex(od1), m_VertexList.FindVertex(od2), newCutsVector);
					CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notCut1]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notCut2]), false, lstNewNodes);
				}
				else
				{
					++debugForbiddenCounter;
					// od and NotCut
					// a cut is ok but otherwise the other edge hits a vertex
					CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[FirstCutStart]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notCut1]), true, lstNewNodes);
					CreateNewTriangle(od1,m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[FirstCutEnd]),m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[notCut1]), true, lstNewNodes);
					AddTheCut(m_VertexList.FindVertex(od1), pNode->GetTriangularNavData()->vertices[notCut1], newCutsVector);
				}
			}
			else
			{
				++debugForbiddenCounter;
				// nasty case when nothing was cut... possibly the edge touches a triangle
				// skip - no new triangles added.
				if (TouchStart>=0)
				{
					++debugForbiddenCounter;
					Vec3r junk;
					real d1 = Distance::Point_Line<real>(m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[TouchStart]).vPos, start, end, junk);
					real d2 = Distance::Point_Line<real>(m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[TouchEnd]).vPos, start, end, junk);
					//					AIWarning("!TOUCHED EDGE. Distances are %.6f and %.6f and aux_s %.6f and maximum_s:%.6f",d1,d2,aux_maximum_s,maximum_s);
					if ((d1<epsilon && d2<epsilon) &&
						(aux_maximum_s>epsilon && aux_maximum_s<1-epsilon))
					{
						// an edge has been touched
						//	AIWarning("!Nasty case. Plugged %.4f into maximum_s.",aux_maximum_s);
						maximum_s = aux_maximum_s;
					}
					else
						maximum_s = -1;

					AddTheCut(pNode->GetTriangularNavData()->vertices[TouchStart], pNode->GetTriangularNavData()->vertices[TouchEnd], newCutsVector);
					// we don't create new triangle - just add it to new nodes list
					lstNewNodes.push_back(nodeIndex);
				}
				else
				{
					++debugForbiddenCounter;
					//					AIWarning("!TOUCHED vertices. SKIP! Aux_s %.6f and maximum :%.6f",aux_maximum_s,maximum_s);
					///					if (aux_maximum_s>epsilon && aux_maximum_s<1-epsilon)
					//						maximum_s = aux_maximum_s;
					//					else
					maximum_s = 0.01f;
				}
			}
		}
	}

	bool bAddedTriangles = (newNodesCount != lstNewNodes.size());
	if ( bAddedTriangles ) 
	{
		// now push old triangle for disconnection later
		lstOldNodes.push_back(nodeIndex);
		for (unsigned linkId=pNode->firstLinkIndex;linkId;linkId=linkManager.GetNextLink(linkId))
		{
			unsigned nextNodeIndex = linkManager.GetNextNode(linkId);
			GraphNode *pNextNode = nodeManager.GetNode(nextNodeIndex);
			if( pNextNode->tag || pNextNode->navType != IAISystem::NAV_TRIANGULAR )	// this one is in candidatesForCutting list (nodes2refine)
				continue;
			if ((std::find(lstNewNodes.begin(),lstNewNodes.end(),nextNodeIndex) == lstNewNodes.end())) // only push if its not the next triangle for refinement
				lstNewNodes.push_back(nextNodeIndex);
		}
	}
}
#ifdef AI_FP_FAST
#pragma fenv_access(off)
#pragma float_control(precise, off)
#endif




//
//-----------------------------------------------------------------------------------------------------------
IAIObject * CAISystem::GetNearestObjectOfTypeInRange(const Vec3 &pos, unsigned int nTypeID, float fRadiusMin, float fRadiusMax, IAIObject* pSkip, int nOption)
{
	IAIObject *pRet = 0;
	float mindist = 100000000;
	const float fRadiusMinSQR = fRadiusMin*fRadiusMin;
	const float fRadiusMaxSQR = fRadiusMax*fRadiusMax;

	AIObjects::iterator ai = m_Objects.find(nTypeID), aiEnd = m_Objects.end();
	for ( ;ai != aiEnd && ai->first == nTypeID; ++ai )
	{
		CAIObject* object = ai->second;
		if( pSkip == object || !(nOption & AIFAF_INCLUDE_DISABLED) && !object->IsEnabled() )
			continue;

		/*float fActivationRadius = object->GetRadius();
		fActivationRadius *= fActivationRadius;*/

		float f = (object->GetPos()-pos).GetLengthSquared();
		//use anchor radius ---------------------
		float	fCurRadiusMaxSQR( object->GetRadius()<0.01f ? fRadiusMaxSQR : (fRadiusMax+object->GetRadius())*(fRadiusMax+object->GetRadius()));
		if ( f < mindist && f > fRadiusMinSQR && f < fCurRadiusMaxSQR )
		{
			/*if ((fActivationRadius>0) && (f>fActivationRadius))
			continue;*/

			pRet = object;
			mindist = f;
		}
	}

	return pRet;
}



//
//-----------------------------------------------------------------------------------------------------------
IAIObject * CAISystem::GetNearestObjectOfTypeInRange(IAIObject * pRequester, unsigned int nTypeID, float fRadiusMin, float fRadiusMax, int nOption)
{
TICK_COUNTER_FUNCTION
	CPuppet* pPuppet = CastToCPuppetSafe(pRequester);

	Vec3 reqPos = pRequester->GetPos();
	if( nOption & AIFAF_USE_REFPOINT_POS )
	{
		if( !pPuppet || !pPuppet->GetRefPoint() )
			return 0;
		reqPos = pPuppet->GetRefPoint()->GetPos();
	}

	IAIObject* pRet = 0;
	float mindist = 100000000;
	const float fRadiusMinSQR = fRadiusMin*fRadiusMin;
	const float fRadiusMaxSQR = fRadiusMax*fRadiusMax;

	AIObjects::iterator ai = m_Objects.find(nTypeID), aiEnd = m_Objects.end();
	for ( ;ai != aiEnd && ai->first == nTypeID; ++ai )
	{
		CAIObject* pObj = ai->second;
		if ( pObj == pRequester || !(nOption & AIFAF_INCLUDE_DISABLED) && !pObj->IsEnabled() )
			continue;
		if (nOption & AIFAF_SAME_GROUP_ID) 
		{
			CAIActor* pActor = pObj->CastToCAIActor();
			if (pActor && pActor->GetGroupId() != pPuppet->GetGroupId())
				continue;
		}
//		float fActivationRadius = object->GetRadius();
//		fActivationRadius*=fActivationRadius;

		const Vec3& objPos = pObj->GetPos();
		float f = (objPos - reqPos).GetLengthSquared();
		if ( f < mindist && f > fRadiusMinSQR && f < fRadiusMaxSQR )
		{
			 if ((nOption & AIFAF_INCLUDE_DEVALUED) || !pPuppet || !pPuppet->IsDevalued(pObj))
			 {
//				if ((fActivationRadius>0) && (f>fActivationRadius))
//					continue;

				 if (nOption & AIFAF_HAS_FORMATION && !pObj->m_pFormation )
				 {
					 continue;
				 }

				if (nOption & AIFAF_VISIBLE_FROM_REQUESTER)
				{
					ray_hit rh;
					int colliders(0);
TICK_COUNTER_FUNCTION_BLOCK_START
					colliders = m_pWorld->RayWorldIntersection(objPos, reqPos - objPos, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &rh, 1);
TICK_COUNTER_FUNCTION_BLOCK_END
					if (colliders)
						continue;
				}
/*
				if (nOption & AIFAF_VISIBLE_TARGET)
				{
					if (pPuppet && pPuppet->m_pAttentionTarget)
					{
						CAIObject *pRealTarget = pPuppet->GetMemoryOwner(pPuppet->m_pAttentionTarget);
						if (!pRealTarget)
							pRealTarget = pPuppet->m_pAttentionTarget;
						ray_hit rh;
						int colliders(0);
TICK_COUNTER_FUNCTION_BLOCK_START
						colliders = m_pWorld->RayWorldIntersection(objPos, pRealTarget->GetPos() - objPos, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &rh, 1);
TICK_COUNTER_FUNCTION_BLOCK_END
						if (colliders)
							continue;
					}
				}
*/
				if( nOption & (AIFAF_LEFT_FROM_REFPOINT | AIFAF_RIGHT_FROM_REFPOINT) )
				{
					if( pPuppet && pPuppet->GetRefPoint() )
					{
						const Vec3	one = pPuppet->GetRefPoint()->GetPos() - reqPos;
						const Vec3	two = objPos - reqPos;

						float zcross = one.x * two.y - one.y * two.x;

						if( nOption & AIFAF_LEFT_FROM_REFPOINT )
						{
							if( zcross < 0 )
								continue;
						}
						else
						{
							if( zcross > 0 )
								continue;
						}
					}
				}

				if (nOption & AIFAF_INFRONT_OF_REQUESTER)
				{
					const Vec3	toTargetDir((objPos - reqPos).GetNormalizedSafe());
					float diffCosine(toTargetDir*pPuppet->m_State.vMoveDir);
//					float diffCosine(toTargetDir*pRequester->GetMoveDir());

					if(diffCosine<.7f)
						continue;
				}

				pRet = pObj;
				mindist = f;
			}
		}
	}

	if (pRet && !(nOption & AIFAF_DONT_DEVALUE))
		Devalue( pRequester, pRet, true );

	return pRet;
}

//
//-----------------------------------------------------------------------------------------------------------
IAIObject *CAISystem::GetNearestAssociatedObjectOfTypeInRange(IAIObject *pRequester , unsigned int nTypeID, float fRadiusMin, float fRadiusMax, int nOption)
{
TICK_COUNTER_FUNCTION
	CPuppet* pPuppet = CastToCPuppetSafe(pRequester);

	Vec3 reqPos = pRequester->GetPos();
	if( nOption & AIFAF_USE_REFPOINT_POS )
	{
		if( !pPuppet || !pPuppet->GetRefPoint() )
			return 0;
		reqPos = pPuppet->GetRefPoint()->GetPos();
	}

	IAIObject* pRet = 0;
	float mindist = 100000000;
	const float fRadiusMinSQR = fRadiusMin*fRadiusMin;
	const float fRadiusMaxSQR = fRadiusMax*fRadiusMax;

	AIObjects::iterator ai = m_Objects.find(nTypeID), aiEnd = m_Objects.end();
	for ( ;ai != aiEnd && ai->first == nTypeID; ++ai )
	{
		IAIObject* pObj = ai->second->GetAssociation();
		if (!pObj)
			continue;

		if ( pObj == pRequester || !(nOption & AIFAF_INCLUDE_DISABLED) && !pObj->IsEnabled() )
			continue;
		if (nOption & AIFAF_SAME_GROUP_ID) 
		{
			CAIActor* pActor = pObj->CastToCAIActor();
			if (pActor && pActor->GetGroupId() != pPuppet->GetGroupId())
				continue;
		}

		Vec3 objPos;
		IEntity *pEntity=pObj->GetEntity();

		if (pEntity)
			objPos=pEntity->GetPos();
		else
			objPos=pObj->GetPos();

		float f = (objPos - reqPos).GetLengthSquared();
		if ( f < mindist && f > fRadiusMinSQR && f < fRadiusMaxSQR )
		{
			 if ((nOption & AIFAF_INCLUDE_DEVALUED) || !pPuppet || !pPuppet->IsDevalued(pObj))
			 {
				if (nOption & AIFAF_VISIBLE_FROM_REQUESTER)
				{
					ray_hit rh;
					int colliders(0);
TICK_COUNTER_FUNCTION_BLOCK_START
					colliders = m_pWorld->RayWorldIntersection(objPos, reqPos - objPos, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &rh, 1);
TICK_COUNTER_FUNCTION_BLOCK_END
					if (colliders)
						continue;
				}

				if (nOption & AIFAF_INFRONT_OF_REQUESTER)
				{
					const Vec3	toTargetDir((objPos - reqPos).GetNormalizedSafe());
					float diffCosine(toTargetDir*pPuppet->m_State.vMoveDir);

					if(diffCosine<.7f)
						continue;
				}

				pRet = pObj;
				mindist = f;
			}
		}
	}

	if (pRet && !(nOption & AIFAF_DONT_DEVALUE))
		Devalue( pRequester, pRet, true );

	return pRet;
}

//====================================================================
// finds the closest object in front of me, using PhysPos
//====================================================================
//-----------------------------------------------------------------------------------------------------------
CAIObject * CAISystem::FindNearestObjectInFront(CAIObject *pRequester, unsigned int nTypeID, float fRadius, float cosTrh)
{
	CPuppet* pPuppet = CastToCPuppetSafe(pRequester);

	Vec3 reqPos = pRequester->GetPhysicsPos();

	CAIObject* pRet = 0;
	float mindist = 100000000;
	const float fRadiusSQR = fRadius*fRadius;

	AIObjects::iterator ai = m_Objects.find(nTypeID), aiEnd = m_Objects.end();
	for ( ;ai != aiEnd && ai->first == nTypeID; ++ai )
	{
		CAIObject* pObj = ai->second;
		if ( pObj == pRequester )
			continue;

		const Vec3 objPos = pObj->GetPhysicsPos();
		float f = (objPos - reqPos).GetLengthSquared();
		if ( f < mindist && f < fRadiusSQR  )
		{
			const Vec3	toTargetDir((objPos - reqPos).GetNormalizedSafe());
			float diffCosine(toTargetDir*pPuppet->m_State.vMoveDir);
			if(diffCosine<cosTrh)
				continue;
			pRet = pObj;
			mindist = f;
		}
	}
	return pRet;
}



//====================================================================
// GetIntermediatePosition
// Calculates ptOut which is (ptNum/(numPts-1)) of the way between start
// and end via mid. If distTotal < 0 then it gets calculated, otherwise
// it and distToMid are assumed to be correct
//====================================================================
static void GetIntermediatePosition(Vec3& ptOut,
                                    const Vec3& start, const Vec3& mid, const Vec3& end, 
                                    float& distTotal, float& distToMid, 
                                    int ptNum, int numPts)
{
  if (distTotal < 0.0f || distToMid < 0.0f)
  {
    distToMid = (mid - start).GetLength();
    distTotal = distToMid + (end - mid).GetLength();
  }
  if (distToMid <= 0.0f)
  {
    AIWarning("Got bad distance in GetIntermediatePosition - distToMid = %6.3f (start = %5.2f, %5.2f, %5.2f, mid = %5.2f, %5.2f, %5.2f, end = %5.2f, %5.2f, %5.2f)",
      distToMid,
      start.x, start.y, start.z,
      mid.x, mid.y, mid.z,
      end.x, end.y, end.z);
    ptOut = start;
    return;
  }

  if (numPts <= 1)
  {
    ptOut = start;
  }
  else
  {
    float distFromStart = (distTotal * ptNum) / (numPts-1);
    if (distFromStart < distToMid)
      ptOut = start + (mid - start) * distFromStart/distToMid;
    else
      ptOut = mid + (end - mid) * (distFromStart - distToMid) / (distTotal - distToMid);
  }
}

//====================================================================
// CalculateLinkExposure
//====================================================================
void CAISystem::CalculateLinkExposure(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode* pNode, unsigned link)
{
	I3DEngine *p3DEngine = m_pSystem->GetI3DEngine();
	static const float interval = 2.0f;
	static const float boxSize = 2.0f;
	static const float boxHeight = 2.0f;
	unsigned nextNodeIndex = linkManager.GetNextNode(link);
	GraphNode* pNextNode = nodeManager.GetNode(nextNodeIndex);
	float dist = (pNextNode->GetPos() - pNode->GetPos()).GetLength();
	int numPts = 1 + (int) (dist / interval);
	AABB box(Vec3(-boxSize, -boxSize, 0.0f), Vec3(boxSize, boxSize, boxHeight));
	float coverNum = 0.0f;
	for (int iPt = 0 ; iPt < numPts ; ++iPt)
	{
		Vec3 pt;
		float distTotal = -1.0f;
		float distToMid = -1.0f;
		GetIntermediatePosition(pt, pNode->GetPos(), linkManager.GetEdgeCenter(link), pNextNode->GetPos(),
			distTotal, distToMid, iPt, numPts);

    IPhysicalEntity**	ppList = NULL;
    int	numEntities = GetPhysicalWorld()->GetEntitiesInBox( pt + box.min, pt + box.max, ppList, ent_static );
    float thisCoverNum = 0.0f;
    for (int i = 0 ; i < numEntities; ++i)
    {
      IPhysicalEntity *pCurrent = ppList[i];
      pe_status_pos status;
      pCurrent->GetStatus(&status);
      float objTop = status.pos.z + status.BBox[1].z;
      if (objTop > pt.z + 2.0f)
        thisCoverNum += 0.5f;
      else if (objTop > pt.z + 1.0f)
        thisCoverNum += 0.25f;

			if (thisCoverNum >= 1.0f)
			{
				thisCoverNum = 1.0f;
				break;
			}
		}
		coverNum += thisCoverNum;
	}
	linkManager.SetExposure(link, 1.0f - (coverNum / (1.0f + numPts)));
}

//====================================================================
// CalculateLinkDeepWaterFraction
//====================================================================
void CAISystem::CalculateLinkWater(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode* pNode, unsigned link)
{
	linkManager.SetMaxWaterDepth(link, 0.0f);
	linkManager.SetMinWaterDepth(link, 1000.0f);

	Vec3 nodePos = pNode->GetPos();
	unsigned nextNodeIndex = linkManager.GetNextNode(link);
	GraphNode* pNextNode = nodeManager.GetNode(nextNodeIndex);
	Vec3 otherPos = pNextNode->GetPos();

	if (!IsPointInWaterAreas(nodePos) && !IsPointInWaterAreas(otherPos))
		return;

	I3DEngine *p3DEngine = m_pSystem->GetI3DEngine();
	static const float interval = 2.0f;
	float dist = (otherPos - nodePos).GetLength();
	int numPts = 1 + (int) (dist / interval);
	for (int iPt = 0 ; iPt < numPts ; ++iPt)
	{
		Vec3 pt;
		float distTotal = -1.0f;
		float distToMid = -1.0f;
		GetIntermediatePosition(pt, nodePos, linkManager.GetEdgeCenter(link), otherPos,
			distTotal, distToMid, iPt, numPts);
		float waterLevel = p3DEngine->GetWaterLevel(&pt);
		if (waterLevel != WATER_LEVEL_UNKNOWN)
		{
			float terrainZ = p3DEngine->GetTerrainElevation(pt.x, pt.y);
			float depth = waterLevel - terrainZ;
			if (depth > linkManager.GetMaxWaterDepth(link))
				linkManager.SetMaxWaterDepth(link, depth);
			if (depth < linkManager.GetMinWaterDepth(link))
				linkManager.SetMinWaterDepth(link, depth);
		}
	}
	if (linkManager.GetMinWaterDepth(link) < 0.0f)
		linkManager.SetMinWaterDepth(link, 0.0f);
	if (linkManager.GetMinWaterDepth(link) > linkManager.GetMaxWaterDepth(link))
		linkManager.SetMinWaterDepth(link, linkManager.GetMaxWaterDepth(link));
}

//====================================================================
// CalculateDistanceToCollidable
//====================================================================
float CAISystem::CalculateDistanceToCollidable(const Vec3 &pos, float maxDist) const
{
	std::vector< std::pair<float, unsigned> > nodes;
	CAllNodesContainer &allNodes = m_pGraph->GetAllNodes();
	allNodes.GetAllNodesWithinRange(nodes, pos, maxDist, IAISystem::NAV_TRIANGULAR);
	if (nodes.empty())
		return maxDist;
	// sort by increasing distance
	std::sort(nodes.begin(), nodes.end());

	unsigned nNodes = nodes.size();
	for (unsigned iNode = 0 ; iNode < nNodes ; ++iNode)
	{
		const GraphNode *pNode = m_pGraph->GetNodeManager().GetNode(nodes[iNode].second);
		unsigned nVerts = pNode->GetTriangularNavData()->vertices.size();
		for (unsigned iVert = 0 ; iVert < nVerts ; ++iVert)
		{
			unsigned index = pNode->GetTriangularNavData()->vertices[iVert];
			const ObstacleData &ob = m_VertexList.GetVertex(index);
			if (!ob.bCollidable)
				continue;
			float dist = Distance::Point_Point2D(ob.vPos, pos);
			dist -= ob.fApproxRadius > 0 ? ob.fApproxRadius : 0.0f;
			if (dist > maxDist)
				continue;
			return dist;
		}
	}
	// nothing found
	return maxDist;
}


//====================================================================
// CalculateLinkRadius
//====================================================================
void CAISystem::CalculateLinkRadius(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode* pNode, unsigned link)
{
	if ( (linkManager.GetStartIndex(link) >= pNode->GetTriangularNavData()->vertices.size()) || (linkManager.GetStartIndex(link)<0) || 
		(linkManager.GetEndIndex(link)>= pNode->GetTriangularNavData()->vertices.size()) || (linkManager.GetEndIndex(link)<0) ) 
	{
		AIWarning("found bad triangle index!!");
		return;
	}

	I3DEngine *p3DEngine = m_pSystem->GetI3DEngine();
	unsigned nextNodeIndex = linkManager.GetNextNode(link);
	GraphNode* pNextNode = nodeManager.GetNode(nextNodeIndex);
	if (IsPathForbidden(pNode->GetPos(), pNextNode->GetPos()))
	{
		linkManager.SetRadius(link, -1.0f);
	}
	else
	{
		ObstacleData &obStart = m_VertexList.ModifyVertex(pNode->GetTriangularNavData()->vertices[linkManager.GetStartIndex(link)]);
		ObstacleData &obEnd   = m_VertexList.ModifyVertex(pNode->GetTriangularNavData()->vertices[linkManager.GetEndIndex(link)]);
		Vec3 vStart = obStart.vPos;
		Vec3 vEnd = obEnd.vPos;
		vStart.z = p3DEngine->GetTerrainElevation(vStart.x, vStart.y);
		vEnd.z = p3DEngine->GetTerrainElevation(vEnd.x, vEnd.y);

    Vec3 bboxsize(1.f, 1.f, 1.f);
    IPhysicalEntity *pEndPhys=0;
    IPhysicalEntity *pStartPhys=0;

    IPhysicalEntity **pEntityList;
    int nCount = m_pWorld->GetEntitiesInBox(vStart-bboxsize, vStart+bboxsize,
      pEntityList, ent_static);
    int i=0;
    while (i<nCount)
    {
      pe_status_pos ppos;
			pEntityList[i]->GetStatus(&ppos);
			ppos.pos.z = vStart.z;
			if (IsEquivalent(ppos.pos, vStart,0.001f))
			{
				pStartPhys = pEntityList[i];
				break;
			}
			++i;
    }

    nCount = m_pWorld->GetEntitiesInBox(vEnd-bboxsize,vEnd+bboxsize,pEntityList,ent_static);
    i=0;
    while (i<nCount)
    {
      pe_status_pos ppos;
      pEntityList[i]->GetStatus(&ppos);
      ppos.pos.z = vEnd.z;
      if (IsEquivalent(ppos.pos, vEnd,0.001f))
      {
        pEndPhys = pEntityList[i];
        break;
      }
      ++i;
    }

    Vec3 vStartCut = vStart;
    Vec3 vEndCut = vEnd;
    Vec3 sideDir = vEnd - vStart;
    float sideHorLen = sideDir.GetLength2D();
    float sideLen = sideDir.NormalizeSafe();

    static float testLen = 10.0f;
    static float testRadius = 1.0f;
    ray_hit se_hit;

		if (pEndPhys)
		{
			float overrideRadius = -1.0f;
			IRenderNode * pRN = (IRenderNode*)pEndPhys->GetForeignData(PHYS_FOREIGN_ID_STATIC);
			if (pRN)
			{
				IStatObj *pStatObj = pRN->GetEntityStatObj(0);
				if (pStatObj)
				{
          pe_status_pos status;
          status.ipart = 0;
          pEndPhys->GetStatus( &status );
					float r = pStatObj->GetAIVegetationRadius();
					if (r > 0.0f)
						overrideRadius = r * status.scale;
					else
						overrideRadius = -1.0f;
				}
			}
			if (overrideRadius >= 0.0f)
			{
				vEndCut -= sideDir * overrideRadius;
			}
			else
			{
				Vec3 vModStart = vStart-testLen*sideDir;
				if (obEnd.bCollidable && m_pWorld->CollideEntityWithBeam(pEndPhys,vModStart,(vEnd-vModStart),testRadius,&se_hit))
				{
					float distToHit = testRadius + se_hit.dist;
					if (distToHit < testLen)
					{
						// NOTE Jul 27, 2007: <pvl> collision occured even *before* the beam
						// arrived to vStart.  That probably means that the whole edge
						// (vStart,vEnd) is inside pEndPhys so mark it unpassable now.
						linkManager.SetRadius(link, -1.0f);
						linkManager.SetEdgeCenter(link, 0.5f * (vStart + vEnd));
						return;
					}
					vEndCut = vModStart + distToHit * (vEnd-vModStart).GetNormalizedSafe();
				}
			}
		}

		if (pStartPhys)
		{	
			float overrideRadius = -1.0f;
			IRenderNode * pRN = (IRenderNode*)pStartPhys->GetForeignData(PHYS_FOREIGN_ID_STATIC);
			if (pRN)
			{
				IStatObj *pStatObj = pRN->GetEntityStatObj(0);
				if (pStatObj)
				{
          pe_status_pos status;
          status.ipart = 0;
          pStartPhys->GetStatus( &status );
					float r = pStatObj->GetAIVegetationRadius();
					if (r > 0.0f)
						overrideRadius = r * status.scale;
					else
						overrideRadius = -1.0f;
				}
			}
			if (overrideRadius >= 0.0f)
			{
				vStartCut += sideDir * overrideRadius;
			}
			else
			{
				Vec3 vModEnd = vEnd+testLen*sideDir;
				if (obStart.bCollidable && m_pWorld->CollideEntityWithBeam(pStartPhys,vModEnd,(vStart-vModEnd),testRadius,&se_hit))
				{
					float distToHit = testRadius + se_hit.dist;
					if (distToHit < testLen)
					{
						// NOTE Jul 27, 2007: <pvl> collision occured even *before* the beam
						// arrived to vEnd.  That probably means that the whole edge
						// (vStart,vEnd) is inside pStartPhys so mark it unpassable now.
						linkManager.SetRadius(link, -1.0f);
						linkManager.SetEdgeCenter(link, 0.5f * (vStart + vEnd));
						return;
					}
					vStartCut = vModEnd + distToHit * (vStart-vModEnd).GetNormalizedSafe();
				}
			}
		}

    float rStart = Distance::Point_Point2D(vStartCut, vStart);
    float rEnd = Distance::Point_Point2D(vEndCut, vEnd);
//    Limit(rStart, 0.0f, sideHorLen);
//    Limit(rEnd, 0.0f, sideHorLen);
    if (obStart.fApproxRadius < rStart)
      obStart.fApproxRadius = rStart;
    if (obEnd.fApproxRadius < rEnd)
      obEnd.fApproxRadius = rEnd;

		Vec3 NormStart=vStartCut, NormEnd=vEndCut;
		NormStart.z = NormEnd.z = 0;
		vStartCut.z = vEndCut.z = 0.0f;
		Vec3 vStartZero = vStart; vStartZero.z = 0.0f;
		float dStart2 = (NormStart - vStartZero).GetLengthSquared();
		float dEnd2 = (NormEnd - vStartZero).GetLengthSquared();
		linkManager.SetRadius(link, 0.5f * (NormEnd - NormStart).GetLength());
		if (dStart2 < dEnd2)
		{
			// sometimes the start/end cut positions seem to be wrong - i.e.
			// the modpoint isn't actually on the line joining the points.
			//							link.vEdgeCenter = 0.5f * (vStartCut+vEndCut);
			Vec3 delta = vEnd - vStart;
			delta.z = 0.0f;
			delta.NormalizeSafe();
			linkManager.SetEdgeCenter(link, vStart + 0.5f * (sqrtf(dStart2) + sqrtf(dEnd2)) * delta);
		}
		else
		{
			linkManager.SetEdgeCenter(link, 0.5f * (vStart+vEnd));
			linkManager.SetRadius(link, -linkManager.GetRadius(link));
		}
	}
	Vec3 vEdgeCenter = linkManager.GetEdgeCenter(link);
	vEdgeCenter.z = p3DEngine->GetTerrainElevation(linkManager.GetEdgeCenter(link).x, linkManager.GetEdgeCenter(link).y);
	linkManager.SetEdgeCenter(link, vEdgeCenter);
}

//====================================================================
// CalculateNoncollidableLinks
//====================================================================
void CAISystem::CalculateNoncollidableLinks()
{
	I3DEngine *p3DEngine = m_pSystem->GetI3DEngine();
	CAllNodesContainer& allNodes = m_pGraph->GetAllNodes();
	CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_TRIANGULAR);
	while (unsigned currentNodeIndex = it.Increment())
	{
		GraphNode* pCurrent = m_pGraph->GetNodeManager().GetNode(currentNodeIndex);

		for (unsigned link = pCurrent->firstLinkIndex ; link ; link = m_pGraph->GetLinkManager().GetNextLink(link))
		{
			unsigned startIndex = pCurrent->GetTriangularNavData()->vertices[m_pGraph->GetLinkManager().GetStartIndex(link)];
			unsigned endIndex = pCurrent->GetTriangularNavData()->vertices[m_pGraph->GetLinkManager().GetEndIndex(link)];
			const ObstacleData &obStart = m_VertexList.GetVertex(startIndex);
			const ObstacleData &obEnd = m_VertexList.GetVertex(endIndex);
			static float maxDist = 10.0f;
			if (obStart.bCollidable && obEnd.bCollidable)
			{
				continue;
			}
			else if (!obStart.bCollidable && !obEnd.bCollidable)
			{
				m_pGraph->GetLinkManager().SetEdgeCenter(link, 0.5f * (obStart.vPos + obEnd.vPos)); // TODO: Don't set shared member twice
				float linkRadius = 0.5f * Distance::Point_Point2D(obStart.vPos, obEnd.vPos);
				float distanceToCollidable = CalculateDistanceToCollidable(m_pGraph->GetLinkManager().GetEdgeCenter(link), maxDist);
				float radius = min(maxDist, distanceToCollidable);
				if (radius < linkRadius)
					radius = linkRadius;
				m_pGraph->GetLinkManager().SetRadius(link, radius);
			}
			else if (!obStart.bCollidable)
			{
				float distanceToCollidable = CalculateDistanceToCollidable(m_pGraph->GetLinkManager().GetEdgeCenter(link), maxDist);
				Vec3 dir = (obEnd.vPos - obStart.vPos).GetNormalizedSafe();
				float linkRadius = m_pGraph->GetLinkManager().GetRadius(link);
				if (linkRadius <= 0.0f)
					continue;
				Vec3 origEnd = m_pGraph->GetLinkManager().GetEdgeCenter(link) + dir * linkRadius;
				float newDist = Distance::Point_Point2D(origEnd, obStart.vPos);
				if (newDist < distanceToCollidable)
					newDist = distanceToCollidable;
				Vec3 newStart = origEnd - dir * newDist;
				m_pGraph->GetLinkManager().GetEdgeCenter(link) = 0.5f * (origEnd + newStart); // TODO: Don't set shared member twice
				m_pGraph->GetLinkManager().GetEdgeCenter(link).z = p3DEngine->GetTerrainElevation(m_pGraph->GetLinkManager().GetEdgeCenter(link).x, m_pGraph->GetLinkManager().GetEdgeCenter(link).y);
				m_pGraph->GetLinkManager().SetRadius(link, newDist * 0.5f);
			}
			else if (!obEnd.bCollidable)
			{
				float distanceToCollidable = CalculateDistanceToCollidable(m_pGraph->GetLinkManager().GetEdgeCenter(link), maxDist);
				Vec3 dir = (obEnd.vPos - obStart.vPos).GetNormalizedSafe();
				float linkRadius = m_pGraph->GetLinkManager().GetRadius(link);
				if (linkRadius <= 0.0f)
					continue;
				Vec3 origStart = m_pGraph->GetLinkManager().GetEdgeCenter(link) - dir * linkRadius;
				float newDist = Distance::Point_Point2D(origStart, obEnd.vPos);
				if (newDist < distanceToCollidable)
					newDist = distanceToCollidable;
				Vec3 newEnd = origStart + dir * newDist;
				m_pGraph->GetLinkManager().GetEdgeCenter(link) = 0.5f * (origStart + newEnd);
				m_pGraph->GetLinkManager().GetEdgeCenter(link).z = p3DEngine->GetTerrainElevation(m_pGraph->GetLinkManager().GetEdgeCenter(link).x, m_pGraph->GetLinkManager().GetEdgeCenter(link).y);
				m_pGraph->GetLinkManager().SetRadius(link, newDist * 0.5f);
			}
		}
	}
}

//===================================================================
// DisableThinNodesNearForbidden
//===================================================================
void CAISystem::DisableThinNodesNearForbidden()
{
	CAllNodesContainer& allNodes = m_pGraph->GetAllNodes();
	CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_TRIANGULAR);
	while (unsigned currentNodeIndex = it.Increment())
	{
		GraphNode* pCurrent = m_pGraph->GetNodeManager().GetNode(currentNodeIndex);

		bool badNode = false;

		const unsigned nVerts = pCurrent->GetTriangularNavData()->vertices.size();
		if (nVerts != 3)
			continue;

		for (unsigned link = pCurrent->firstLinkIndex ; link ; link = m_pGraph->GetLinkManager().GetNextLink(link))
		{
			unsigned nextNodeIndex = m_pGraph->GetLinkManager().GetNextNode(link);
			const GraphNode *pNext = m_pGraph->GetNodeManager().GetNode(nextNodeIndex);
			if (pNext->navType != IAISystem::NAV_TRIANGULAR)
				continue;
			if (m_pGraph->GetLinkManager().GetRadius(link) > 0.0f)
				continue;

			unsigned iVertStart = m_pGraph->GetLinkManager().GetStartIndex(link);
			unsigned iVertEnd = m_pGraph->GetLinkManager().GetEndIndex(link);
			// to get the other note that the some of the vertex numbers = 0 + 1 + 2 = 3
			unsigned iVertOther = 3 - (iVertStart + iVertEnd);

			const ObstacleData& odStart = m_VertexList.GetVertex(pCurrent->GetTriangularNavData()->vertices[iVertStart]);
			const ObstacleData& odEnd = m_VertexList.GetVertex(pCurrent->GetTriangularNavData()->vertices[iVertEnd]);
			const ObstacleData& od = m_VertexList.GetVertex(pCurrent->GetTriangularNavData()->vertices[iVertOther]);
			if (od.fApproxRadius<= 0.0f)
				continue;

			Lineseg seg(odStart.vPos, odEnd.vPos);
			float junk;
			float distToEdgeSq = Distance::Point_Lineseg2DSq(od.vPos, seg, junk);

			if (distToEdgeSq < square(od.fApproxRadius))
			{
				badNode = true;
				break;
			}
		} // loop over links

		if (badNode)
		{
			for (unsigned link = pCurrent->firstLinkIndex ; link ; link = m_pGraph->GetLinkManager().GetNextLink(link))
			{
				unsigned nextNodeIndex = m_pGraph->GetLinkManager().GetNextNode(link);
				GraphNode *pNext = m_pGraph->GetNodeManager().GetNode(nextNodeIndex);
				if (pNext->navType != IAISystem::NAV_TRIANGULAR)
					continue;
				unsigned incomingLink = pNext->GetLinkTo(m_pGraph->GetNodeManager(), m_pGraph->GetLinkManager(), pCurrent);
				if (m_pGraph->GetLinkManager().GetRadius(incomingLink) < 0.0f)
					continue;
				m_pGraph->GetLinkManager().SetRadius(incomingLink, -3.0f);
			}
		} // bad node
	} // loop over all nodes
}

//===================================================================
// MarkForbiddenTriangles
//===================================================================
void CAISystem::MarkForbiddenTriangles()
{
	CAllNodesContainer& allNodes = m_pGraph->GetAllNodes();
	CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_TRIANGULAR);
	while (unsigned currentNodeIndex = it.Increment())
	{
		GraphNode* pCurrent = m_pGraph->GetNodeManager().GetNode(currentNodeIndex);

		bool badNode = false;

		Vec3 pos = pCurrent->GetPos();
		bool forbidden = IsPointInForbiddenRegion(pos, 0, true);
		pCurrent->GetTriangularNavData()->isForbidden = forbidden;
		forbidden = IsPointInForbiddenRegion(pos, 0, false);
		pCurrent->GetTriangularNavData()->isForbiddenDesigner = forbidden;
	}
}

//====================================================================
// CalculateLinkProperties
//====================================================================
void CAISystem::CalculateLinkProperties()
{
  I3DEngine *p3DEngine = m_pSystem->GetI3DEngine();

	/// keep track of all tri-tri links that we've calculated so we can avoid doing 
	/// reciprocal calculations
	std::set<unsigned> processedTriTriLinks;

	CAllNodesContainer& allNodes = m_pGraph->GetAllNodes();
	CAllNodesContainer::Iterator it(allNodes, IAISystem::NAVMASK_ALL);
	while (unsigned currentNodeIndex = it.Increment())
	{
		GraphNode* pCurrent = m_pGraph->GetNodeManager().GetNode(currentNodeIndex);

		if (pCurrent->navType == IAISystem::NAV_TRIANGULAR)
		{
			// clamp the vertices to the ground
			for (unsigned iVert = 0 ; iVert < pCurrent->GetTriangularNavData()->vertices.size() ; ++iVert)
			{
				ObstacleData& od = m_VertexList.ModifyVertex(pCurrent->GetTriangularNavData()->vertices[iVert]);
				od.vPos.z = p3DEngine->GetTerrainElevation(od.vPos.x, od.vPos.y);
			}

			// find max passing radius between this node and all neighboors
			for(unsigned link=pCurrent->firstLinkIndex;link;link=m_pGraph->GetLinkManager().GetNextLink(link))
			{
				GraphNode* pNextNode = m_pGraph->GetNodeManager().GetNode(m_pGraph->GetLinkManager().GetNextNode(link));

				if (m_pGraph->GetLinkManager().GetNextNode(link) == m_pGraph->m_safeFirstIndex)
				{
					m_pGraph->GetLinkManager().SetRadius(link, -1.0f);
					continue;
				}

				if (pNextNode->navType & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE))
				{
					m_pGraph->GetLinkManager().SetRadius(link, 100.0f);
					m_pGraph->GetLinkManager().SetMaxWaterDepth(link, 0.0f);
					m_pGraph->GetLinkManager().SetMinWaterDepth(link, 0.0f);
					m_pGraph->GetLinkManager().GetEdgeCenter(link) = 0.5f * (pCurrent->GetPos() + pNextNode->GetPos());
					CalculateLinkExposure(m_pGraph->GetNodeManager(), m_pGraph->GetLinkManager(), pCurrent, link); 
					continue;
				}

				if (pNextNode->navType == IAISystem::NAV_TRIANGULAR)
				{
					if (processedTriTriLinks.insert(link).second)
					{
						CalculateLinkRadius(m_pGraph->GetNodeManager(), m_pGraph->GetLinkManager(), pCurrent, link);				
						CalculateLinkExposure(m_pGraph->GetNodeManager(), m_pGraph->GetLinkManager(), pCurrent, link);
						CalculateLinkWater(m_pGraph->GetNodeManager(), m_pGraph->GetLinkManager(), pCurrent, link);

						// if this was a non-forbidden link, use the result for the reciprocal link, unless
						// that is forbidden
						if (m_pGraph->GetLinkManager().GetRadius(link) > 0.0f)
						{
							for (unsigned incomingLink = pNextNode->firstLinkIndex; incomingLink; incomingLink = m_pGraph->GetLinkManager().GetNextLink(incomingLink))
							{
								GraphNode* pIncomingLinkTarget = m_pGraph->GetNodeManager().GetNode(m_pGraph->GetLinkManager().GetNextNode(incomingLink));
								if (pIncomingLinkTarget == pCurrent)
								{
									if (processedTriTriLinks.insert(incomingLink).second)
									{
										if (IsPathForbidden(pNextNode->GetPos(), pCurrent->GetPos()))
										{
											m_pGraph->GetLinkManager().SetRadius(incomingLink, -1.0f);
											m_pGraph->GetLinkManager().GetEdgeCenter(incomingLink).z = p3DEngine->GetTerrainElevation(m_pGraph->GetLinkManager().GetEdgeCenter(incomingLink).x, m_pGraph->GetLinkManager().GetEdgeCenter(incomingLink).y);
										}
										else
										{
											m_pGraph->GetLinkManager().SetRadius(incomingLink, m_pGraph->GetLinkManager().GetRadius(link));
											m_pGraph->GetLinkManager().GetEdgeCenter(incomingLink) = m_pGraph->GetLinkManager().GetEdgeCenter(link); // TODO: don't set shared member twice
										}
										assert((incomingLink >> 1) == (link >> 1));
										if ((incomingLink >> 1) != (link >> 1))
											m_pGraph->GetLinkManager().SetExposure(incomingLink, m_pGraph->GetLinkManager().GetExposure(link));
										m_pGraph->GetLinkManager().SetMaxWaterDepth(incomingLink, m_pGraph->GetLinkManager().GetMaxWaterDepth(link));
										m_pGraph->GetLinkManager().SetMinWaterDepth(incomingLink, m_pGraph->GetLinkManager().GetMinWaterDepth(link));
									}
									break;
								}
							}
						}
					}
				}
			}
		}
		else
		{
			// all building links have maximum pass radiuses
			for(unsigned link=pCurrent->firstLinkIndex;link;link=m_pGraph->GetLinkManager().GetNextLink(link))
			{ 
				GraphNode* pNextNode = m_pGraph->GetNodeManager().GetNode(m_pGraph->GetLinkManager().GetNextNode(link));
				if (pNextNode->navType == IAISystem::NAV_WAYPOINT_HUMAN)
					m_pGraph->GetLinkManager().SetRadius(link, 100);
			}
		}
	}
	CalculateNoncollidableLinks();
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::CreateNewTriangle(const ObstacleData & _od1, const ObstacleData & _od2, const ObstacleData & _od3, bool tag, ListNodeIds &lstNewNodes)
{
// Since we add vertices don't use reference arguments since the reference may become invalid!

	ObstacleData od1 = _od1;
	ObstacleData od2 = _od2;
	ObstacleData od3 = _od3;

	int	vIdx1 = m_VertexList.AddVertex(od1);
	int	vIdx2 = m_VertexList.AddVertex(od2);
	int	vIdx3 = m_VertexList.AddVertex(od3);

	if( vIdx1==vIdx2 || vIdx1==vIdx3 || vIdx2==vIdx3 )
	{	
		// it's degenerate
		return;
	}

	unsigned nodeIndex = m_pGraph->CreateNewNode(IAISystem::NAV_TRIANGULAR, Vec3(ZERO));
	GraphNode* pNewNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);

	pNewNode->GetTriangularNavData()->vertices.push_back(vIdx1);
	pNewNode->GetTriangularNavData()->vertices.push_back(vIdx2);
	pNewNode->GetTriangularNavData()->vertices.push_back(vIdx3);

	m_pGraph->FillGraphNodeData(nodeIndex);
	lstNewNodes.push_back(nodeIndex);
	if(tag)
		m_pGraph->TagNode(nodeIndex);

	return;
}

//====================================================================
// IsPointInSpecialArea
//====================================================================
bool CAISystem::IsPointInSpecialArea(const Vec3 &pos, const SpecialArea &sa)
{
  if ( sa.fHeight>0.00001f && (pos.z < sa.fMinZ || pos.z > sa.fMaxZ) )
		return false;
	if (Overlap::Point_Polygon2D(pos, sa.GetPolygon(), &sa.GetAABB()))
	  return true;
  else
    return false;
}

//===================================================================
// IsPointInTriangulationAreas
//===================================================================
bool CAISystem::IsPointInTriangulationAreas(const Vec3 &pos)
{
  AIAssert(GetISystem()->IsEditor());
  bool foundOne = false;
  for (SpecialAreaMap::iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
  {
    SpecialArea &sa = si->second;
    if (sa.type == SpecialArea::TYPE_TRIANGULATION)
    {
      if (Overlap::Point_Polygon2D(pos, sa.GetPolygon(), &sa.GetAABB()))
        return true;
      foundOne = true;
    }
  }
  return !foundOne;
}


//====================================================================
// IsPointInDynamicTriangularAreas
//====================================================================
bool CAISystem::IsPointInDynamicTriangularAreas(const Vec3 &pos) const
{
  return GetDynamicTriangulationValue(pos) > 0.0f;
}

//====================================================================
// CheckNavigationType
// When areas are nested there is an ordering - make this explicit by
// ordering the search over the navigation types
//====================================================================
IAISystem::ENavigationType CAISystem::CheckNavigationType(const Vec3 & pos, int & nBuildingID, IVisArea *&pAreaID, 
																												 IAISystem::tNavCapMask navCapMask)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  // make sure each area has a building id
	// Flight/water navigation modifiers are only used to limit the initial navigation 
	// area during preprocessing - give them a building id anyway
	for (SpecialAreaMap::iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
  {
		SpecialArea &sa = si->second;
		if (sa.nBuildingID<0)
			(si->second).nBuildingID = m_BuildingIDManager.GetId();
  }  

  if (navCapMask & IAISystem::NAV_WAYPOINT_HUMAN)
  {
		for (SpecialAreaMap::iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
    {
			SpecialArea &sa = si->second;
      if (sa.type == SpecialArea::TYPE_WAYPOINT_HUMAN)
      {
		    if (IsPointInSpecialArea(pos, sa))
		    {
					nBuildingID = sa.nBuildingID;
					I3DEngine *p3dEngine=m_pSystem->GetI3DEngine();
					pAreaID = p3dEngine->GetVisAreaFromPos(pos);
          return IAISystem::NAV_WAYPOINT_HUMAN;
        }
      }
    }
  }

  if (navCapMask & IAISystem::NAV_WAYPOINT_3DSURFACE)
  {
		for (SpecialAreaMap::iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
    {
			SpecialArea &sa = si->second;
      if (sa.type == SpecialArea::TYPE_WAYPOINT_3DSURFACE)
      {
		    if (IsPointInSpecialArea(pos, sa))
		    {
          // 3D surface is slightly different since it's likely to be overlapped with another
          // type - we only return it if we're in the area AND near a suitable node
          if (0 == m_pWaypoint3DSurfaceNavRegion->GetBestNodeForPosInInRegion(pos, sa))
            continue;

					nBuildingID = sa.nBuildingID;
					I3DEngine *p3dEngine=m_pSystem->GetI3DEngine();
					pAreaID = p3dEngine->GetVisAreaFromPos(pos);
          return IAISystem::NAV_WAYPOINT_3DSURFACE;
        }
      }
    }
  }

  if (navCapMask & IAISystem::NAV_VOLUME)
  {
		for (SpecialAreaMap::iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
    {
			SpecialArea &sa = si->second;
      if (sa.type == SpecialArea::TYPE_VOLUME)
      {
		    if (IsPointInSpecialArea(pos, sa))
		    {
					nBuildingID = sa.nBuildingID;
					I3DEngine *p3dEngine=m_pSystem->GetI3DEngine();
					pAreaID = p3dEngine->GetVisAreaFromPos(pos);
          return IAISystem::NAV_VOLUME;
        }
      }
    }
  }

	if( navCapMask & IAISystem::NAV_FLIGHT )
  {
		for (SpecialAreaMap::iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
    {
			SpecialArea &sa = si->second;
      if (sa.type == SpecialArea::TYPE_FLIGHT)
      {
		    if (IsPointInSpecialArea(pos, sa))
          return IAISystem::NAV_FLIGHT;
      }
    }
   // if (navCapMask != IAISystem::NAVMASK_ALL)
   //   AIWarning("No flight AI nav modifier around point (%5.2f, %5.2f, %5.2f)", pos.x, pos.y, pos.z);
  }

  if (navCapMask & IAISystem::NAV_FREE_2D)
  {
		for (SpecialAreaMap::iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
    {
			SpecialArea &sa = si->second;
      if (sa.type == SpecialArea::TYPE_FREE_2D)
      {
		    if (IsPointInSpecialArea(pos, sa))
		    {
					nBuildingID = sa.nBuildingID;
					I3DEngine *p3dEngine=m_pSystem->GetI3DEngine();
					pAreaID = p3dEngine->GetVisAreaFromPos(pos);
          return IAISystem::NAV_FREE_2D;
        }
      }
    }
  }

  if (navCapMask & IAISystem::NAV_TRIANGULAR)
  	return NAV_TRIANGULAR;

  if (navCapMask & IAISystem::NAV_FREE_2D)
  	return NAV_FREE_2D;

  return NAV_UNSET;
}

//===================================================================
// TriangleLineIntersection
//===================================================================
bool CAISystem::TriangleLineIntersection(GraphNode * pNode, const Vec3 & vStart, const Vec3 & vEnd)
{
  if (!pNode || pNode->navType != IAISystem::NAV_TRIANGULAR || pNode->GetTriangularNavData()->vertices.empty())
    return false;
  unsigned nVerts = pNode->GetTriangularNavData()->vertices.size();
  Vec3r P0 = vStart;
  Vec3r Q0 = vEnd;
  unsigned int index,next_index;
	for (index=0;index != nVerts; ++index)
	{
		Vec3r P1,Q1;
		next_index = (index+1) % nVerts;

		//get the triangle edge
		P1=m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[index]).vPos;
		Q1=m_VertexList.GetVertex(pNode->GetTriangularNavData()->vertices[next_index]).vPos;

		real s=-1,t=-1;
		if (Intersect::Lineseg_Lineseg2D(Linesegr(P0, Q0), Linesegr(P1, Q1), s, t))
		{
			// For some reason just using the intersection result as is generates errors
			// later on - it seems that it detects intersections maybe between two edges of 
			// the same triangle or something... Anyway, limiting the intersection test to
			// not include the segment ends makes this problem go away (epsilon can actually
			// be 0 here... but I don't like comparisons that close to 0)
 			static const real eps = 0.0000000001;
			if ((s>0.f-eps) && (s<1.f+eps) && (t>0.f+eps) && (t<1.f-eps))
				return true;
		}
	}
	return false;
}

//===================================================================
// SegmentInTriangle
//===================================================================
bool CAISystem::SegmentInTriangle(GraphNode * pNode, const Vec3 & vStart, const Vec3 & vEnd)
{
  if(m_pGraph->PointInTriangle( (vStart+vEnd)*.5f, pNode))
		return true;
  return TriangleLineIntersection(pNode, vStart, vEnd);
}


//====================================================================
// GetNavigationShapeName
//====================================================================
const char *CAISystem::GetNavigationShapeName(int nBuildingID) const
{
	for (SpecialAreaMap::const_iterator di = m_mapSpecialAreas.begin() ; di != m_mapSpecialAreas.end() ; ++di)
	{
    int bID = di->second.nBuildingID;
    const char *name = di->first.c_str();
		if (bID == nBuildingID)
			return name;
	}
	return "Cannot convert building id to name";
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::DoesNavigationShapeExists(const char * szName, EnumAreaType areaType, bool road)
{
	if (areaType == AREATYPE_PATH)
	{
		if (road)
			return m_pRoadNavRegion->DoesRoadExists(szName);
		else
			return m_mapDesignerPaths.find(szName) != m_mapDesignerPaths.end();
	}
	else if (areaType == AREATYPE_FORBIDDEN)
	{
		return m_designerForbiddenAreas.FindShape(szName) != 0;
	}
	else if (areaType == AREATYPE_FORBIDDENBOUNDARY)
	{
		return m_forbiddenBoundaries.FindShape(szName) != 0;
	}
	else if (areaType == AREATYPE_NAVIGATIONMODIFIER)
	{
		return m_mapSpecialAreas.find(szName) != m_mapSpecialAreas.end() ||
			m_mapExtraLinkCosts.find(szName) != m_mapExtraLinkCosts.end() ||
			m_mapDynamicTriangularNavigation.find(szName) != m_mapDynamicTriangularNavigation.end();
	}
	else if (areaType == AREATYPE_OCCLUSION_PLANE)
	{
		return m_mapOcclusionPlanes.find(szName) != m_mapOcclusionPlanes.end();
	}
	else if (areaType == AREATYPE_GENERIC)
	{
		return m_mapGenericShapes.find(szName) != m_mapGenericShapes.end();
	}

	return false;
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::CreateNavigationShape(const SNavigationShapeParams &params)
{
	// need at least one point in a path. Some paths need more than one (areas need 3)
	if (params.nPoints == 0)
		return true; // Do not report too few points as errors.

	std::vector<Vec3> vecPts(params.points, params.points + params.nPoints);

	if (params.areaType==AREATYPE_PATH && params.pathIsRoad == false )
	{
		//designer path need to preserve directions
	}
	else
	{
		if (params.closed)
			EnsureShapeIsWoundAnticlockwise<std::vector<Vec3>, float>(vecPts);
	}

	ListPositions listPts(vecPts.begin(), vecPts.end());

	if (params.areaType==AREATYPE_PATH)
	{
		if (listPts.size() < 2)
			return true; // Do not report too few points as errors.

		if (params.pathIsRoad)
		{
			return m_pRoadNavRegion->CreateRoad(params.szPathName, vecPts, 10.0f, 2.5f);
		}
		else
		{
			if (m_mapDesignerPaths.find(params.szPathName) != m_mapDesignerPaths.end())
			{
				AIError("CAISystem::CreateNavigationShape: Designer path '%s' already exists, please rename the path.", params.szPathName);
				return false;
			}

			if(params.closed)
			{
				if(Distance::Point_Point(listPts.front(), listPts.back()) > 0.1f)
					listPts.push_back(listPts.front());
			}
			m_mapDesignerPaths.insert(ShapeMap::iterator::value_type(params.szPathName, SShape(listPts, false, (IAISystem::ENavigationType)params.nNavType, params.nAuxType)));
		}
	}
	else if (params.areaType == AREATYPE_FORBIDDEN)
	{
		if (listPts.size() < 3)
			return true; // Do not report too few points as errors.

    ClearForbiddenQuadTrees();
		m_forbiddenAreas.Clear();	// The designer forbidden areas will be duplicated into the forbidden areas when regenerating navigation.

		CAIShape* pShape = m_designerForbiddenAreas.FindShape(params.szPathName);
		if (pShape)
		{
			AIError("CAISystem::CreateNavigationShape: Forbidden area '%s' already exists, please rename the shape.", params.szPathName);
			return false;
		}

		pShape = new CAIShape();
		pShape->SetName(params.szPathName);
		pShape->SetPoints(vecPts);
		m_designerForbiddenAreas.InsertShape(pShape);
	}
	else if (params.areaType == AREATYPE_FORBIDDENBOUNDARY)
	{
		if (listPts.size() < 3)
			return true; // Do not report too few points as errors.

    ClearForbiddenQuadTrees();

		CAIShape* pShape = m_forbiddenBoundaries.FindShape(params.szPathName);
		if (pShape)
		{
			AIError("CAISystem::CreateNavigationShape: Forbidden boundary '%s' already exists, please rename the shape.", params.szPathName);
			return false;
		}

		pShape = new CAIShape();
		pShape->SetName(params.szPathName);
		pShape->SetPoints(vecPts);
		m_forbiddenBoundaries.InsertShape(pShape);
	}
	else if (params.areaType == AREATYPE_NAVIGATIONMODIFIER)
	{
		if (listPts.size() < 3)
			return true; // Do not report too few points as errors.

		if (m_mapSpecialAreas.find(params.szPathName) != m_mapSpecialAreas.end())
		{
			AIError("CAISystem::CreateNavigationShape: Navigation modifier '%s' already exists, please rename the modifier.", params.szPathName);
			return false;
		}
		if (m_mapExtraLinkCosts.find(params.szPathName) != m_mapExtraLinkCosts.end())
		{
			AIError("CAISystem::CreateNavigationShape: Navigation modifier '%s' already exists, please rename the modifier.", params.szPathName);
			return false;
		}
		if (m_mapDynamicTriangularNavigation.find(params.szPathName) != m_mapDynamicTriangularNavigation.end())
		{
			AIError("CAISystem::CreateNavigationShape: Navigation modifier '%s' already exists, please rename the modifier.", params.szPathName);
			return false;
		}

		SpecialArea sa;
		sa.SetPolygon(listPts);
		sa.nBuildingID = m_BuildingIDManager.GetId();
		sa.fHeight = params.fHeight;
		sa.lightLevel = params.lightLevel;
		switch (params.nNavType)
		{
		case NMT_WAYPOINTHUMAN: sa.type = SpecialArea::TYPE_WAYPOINT_HUMAN; break;
		case NMT_VOLUME: sa.type = SpecialArea::TYPE_VOLUME; break;
		case NMT_FLIGHT: sa.type = SpecialArea::TYPE_FLIGHT; break;
		case NMT_WATER: sa.type = SpecialArea::TYPE_WATER; break;
		case NMT_WAYPOINT_3DSURFACE: sa.type = SpecialArea::TYPE_WAYPOINT_3DSURFACE; break;
		case NMT_EXTRA_NAV_COST: 
      {
        ExtraLinkCostShapeMap::iterator it = m_mapExtraLinkCosts.insert(ExtraLinkCostShapeMap::iterator::value_type(params.szPathName, SExtraLinkCostShape(listPts, params.extraLinkCostFactor))).first;
        if (params.fHeight < 0.00001f)
        {
     		  it->second.aabb.min.z = -10000.0f;
    		  it->second.aabb.max.z =  10000.0f;
        }
        else
        {
          it->second.aabb.max.z += params.fHeight;
        }
        return true;
      }
    case NMT_DYNAMIC_TRIANGULATION:
      m_mapDynamicTriangularNavigation.insert(
        DynamicTriangulationShapeMap::iterator::value_type(params.szPathName, SDynamicTriangulationShape(listPts, params.triangulationSize)));
      return true;
    case NMT_FREE_2D: sa.type = SpecialArea::TYPE_FREE_2D; break;
    case NMT_TRIANGULATION: sa.type = SpecialArea::TYPE_TRIANGULATION; break;

		default: AIWarning("Unhandled nNavType %d for area type", params.nNavType); return false;
		}
    sa.bCalculate3DNav = params.bCalculate3DNav;
    sa.f3DNavVolumeRadius = params.f3DNavVolumeRadius;
		sa.waypointConnections = params.waypointConnections;
    sa.bVehiclesInHumanNav = params.bVehiclesInHumanNav;
		sa.fNodeAutoConnectDistance = params.fNodeAutoConnectDistance;
		sa.fMinZ = 99999.0f;
		sa.fMaxZ = -99999.0f;
		for (ListPositions::const_iterator it = listPts.begin() ; it != listPts.end() ; ++it)
		{
			if (it->z < sa.fMinZ)
				sa.fMinZ = it->z;
			if (it->z > sa.fMaxZ)
				sa.fMaxZ = it->z;
		}
		sa.fMaxZ = sa.fMinZ + sa.fHeight;

		// insert new
		InsertSpecialArea (params.szPathName, sa);
	}
	else if (params.areaType == AREATYPE_OCCLUSION_PLANE)
	{
		if (listPts.size() < 3)
			return true; // Do not report too few points as errors.

		if (m_mapOcclusionPlanes.find(params.szPathName) != m_mapOcclusionPlanes.end())
		{
			AIError("CAISystem::CreateNavigationShape: Occlusion plane '%s' already exists, please rename the shape.", params.szPathName);
			return false;
		}

		m_mapOcclusionPlanes.insert(ShapeMap::iterator::value_type(params.szPathName,SShape(listPts)));
	}
	else if (params.areaType == AREATYPE_GENERIC)
	{
		if (listPts.size() < 3)
			return true; // Do not report too few points as errors.

		if (m_mapGenericShapes.find(params.szPathName) != m_mapGenericShapes.end())
		{
			AIError("CAISystem::CreateNavigationShape: Shape '%s' already exists, please rename the shape.", params.szPathName);
			return false;
		}

		m_mapGenericShapes.insert(ShapeMap::iterator::value_type(params.szPathName,
			SShape(listPts, false, IAISystem::NAV_UNSET, params.nAuxType, params.fHeight, params.lightLevel)));
	}

	return true;
}

// deletes designer created path
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DeleteNavigationShape(const char* szName)
{
	ShapeMap::iterator di;

	m_pRoadNavRegion->DeleteRoad(szName);

	di = m_mapDesignerPaths.find(szName);
	if (di!= m_mapDesignerPaths.end())
		m_mapDesignerPaths.erase(di);

	di = m_mapGenericShapes.find(szName);
	if (di!= m_mapGenericShapes.end())
	{
		m_mapGenericShapes.erase(di);
		// Make sure there is no dangling pointers left.
		AIObjects::iterator ai = m_Objects.find(AIOBJECT_PUPPET);
		for(; ai != m_Objects.end(); ++ai)
		{
			if(ai->first != AIOBJECT_PUPPET) break;
			CPuppet *pPuppet = ai->second->CastToCPuppet();
			if(pPuppet)
			{
				if(strcmp(pPuppet->GetRefShapeName(), szName) == 0)
					pPuppet->SetRefShapeName(0);
				if(strcmp(pPuppet->GetTerritoryShapeName(), szName) == 0)
					pPuppet->SetTerritoryShapeName(0);
			}
		}
	}

	if (CAIShape* pShape = m_designerForbiddenAreas.FindShape(szName))
	{
    ClearForbiddenQuadTrees();
		m_forbiddenAreas.Clear();	// The designer forbidden areas will be duplicated into the forbidden areas when regenerating navigation.
		m_designerForbiddenAreas.DeleteShape(pShape);
  }

	if (CAIShape* pShape = m_forbiddenBoundaries.FindShape(szName))
	{
		ClearForbiddenQuadTrees();
		m_forbiddenBoundaries.DeleteShape(pShape);
	}

	di = m_mapOcclusionPlanes.find(szName);
	if (di!= m_mapOcclusionPlanes.end())
		m_mapOcclusionPlanes.erase(di);

  DynamicTriangulationShapeMap::iterator ddi = m_mapDynamicTriangularNavigation.find(szName);
  if (ddi != m_mapDynamicTriangularNavigation.end())
    m_mapDynamicTriangularNavigation.erase(ddi);

	EraseSpecialArea(szName);

  ExtraLinkCostShapeMap::iterator it = m_mapExtraLinkCosts.find(szName);
  if (it != m_mapExtraLinkCosts.end())
    m_mapExtraLinkCosts.erase(it);

	// Light manager might have pointers to shapes and areas, update it.
	m_lightManager.Update(true);
}

float CAISystem::GetNearestPointOnPath( const char * sPathName, const Vec3& vPos, Vec3& vResult, bool& bLoopPath, float& totalLength )
{

	// calculate a point on a path which has the nearest distance from the given point.
	// Also returns segno it means ( 0.0 start point 100.0 end point always)
	// strict even if t == 0 or t == 1.0

	// return values
	// return value : segno;
	// vResult : the point on path
	// bLoopPath : true if the path is looped

	vResult		= ZERO;
	bLoopPath	= false;

	float result = -1.0;

	ListPositions junk1;
	AABB junk2;
	SShape pathShape(junk1, junk2);
	if (!GetAISystem()->GetDesignerPath(sPathName,pathShape))
		return result;
	if (pathShape.shape.empty())
		return result;

	float	dist	= FLT_MAX;
	bool	bFound	=false;
	float	segNo	=0.0f;
	float	howmanypath =0.0f;

	Vec3	vPointOnLine;

	ListPositions::const_iterator cur = pathShape.shape.begin();
	ListPositions::const_reverse_iterator last = pathShape.shape.rbegin();
	ListPositions::const_iterator next(cur);
	++next;
	
	float	lengthTmp = 0.0f;
	while( next != pathShape.shape.end())
	{
		Lineseg	seg(*cur, *next);

		lengthTmp += ( *cur - *next ).GetLength();

		float	t;
		float	d = Distance::Point_Lineseg(vPos, seg, t);
		if( d < dist)
		{
			Vec3 vSeg = seg.GetPoint(1.0f) - seg.GetPoint(0.0f);
			Vec3 vTmp = vPos - seg.GetPoint(t);

			vSeg.NormalizeSafe();
			vTmp.NormalizeSafe();

//			if ( vSeg.Dot(vTmp) < 0.0001f )
//			{
				dist	=d;
				bFound	=true;
				result	=segNo + t;
				vPointOnLine = seg.GetPoint(t);
//			}
		}
		cur = next;
		++next;
		segNo +=1.0f;
		howmanypath +=1.0f;
	}
	if ( howmanypath == 0.0f )
		return  result;

	if ( bFound == false )
	{
		segNo = 0.0f;
		cur = pathShape.shape.begin();
		while( cur != pathShape.shape.end() )
		{
			Vec3 vTmp = vPos - *cur;
			float d	= vTmp.GetLength();
			if( d < dist)
			{
				dist	=d;
				bFound	=true;
				result	=segNo;
				vPointOnLine = *cur;
			}
			++cur;
			segNo +=1.0f;
		}
	}

	vResult = vPointOnLine;

	cur		= pathShape.shape.begin();

	Vec3 vTmp	= *cur - *last;
	if ( vTmp.GetLength() < 0.0001f )
		bLoopPath = true;

	totalLength =lengthTmp;

	return result * 100.0f / howmanypath;

}

const std::vector<Vec3> *CAISystem::GetPath(const char *szPathName)
{
	ListPositions junk1;
	AABB junk2;
	SShape pathShape(junk1, junk2);
	static std::vector<Vec3> shape;

	if (!GetAISystem()->GetDesignerPath(szPathName,pathShape))
		return NULL;

	shape.resize(pathShape.shape.size());
	int i = 0;
	for (std::list<Vec3>::iterator it = pathShape.shape.begin(), e = pathShape.shape.end(); it != e; ++it)
	{
		shape[i++] = *it;
	}
	return &shape;
}

bool CAISystem::GetNearestPointOnPathReally( const char * szPathName, const Vec3& pos, float& dist, Vec3& nearestPt, float* distAlongPath /*= 0*/ )
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	ListPositions junk1;
	AABB junk2;
	SShape pathShape(junk1, junk2);

	if (!GetAISystem()->GetDesignerPath(szPathName,pathShape))
		return false;
	if (pathShape.shape.empty())
		return false;

	pathShape.NearestPointOnPath(pos, dist, nearestPt, distAlongPath);
	return true;
}

void CAISystem::GetPointOnPathBySegNo( const char * sPathName, Vec3& vResult, float segNo )
{

	vResult		= ZERO;
	ListPositions junk1;
	AABB junk2;
	SShape pathShape(junk1, junk2);

	if (!GetAISystem()->GetDesignerPath(sPathName,pathShape))
		return;
	if (pathShape.shape.empty())
		return;

	float totalLength = 0.0f;
	{
		ListPositions::const_iterator cur = pathShape.shape.begin();
		ListPositions::const_reverse_iterator last = pathShape.shape.rbegin();
		ListPositions::const_iterator next(cur);
		++next;
		
		float	lengthTmp = 0.0f;
		while( next != pathShape.shape.end())
		{
			Lineseg	seg(*cur, *next);
			totalLength += ( *cur - *next ).GetLength();
			cur = next;
			++next;
		}
	}

	float segLength = totalLength * segNo / 100.0; 
	float currentLength = 0.0f;
	float currentSegLength = 0.0f;
	{
		ListPositions::const_iterator cur = pathShape.shape.begin();
		ListPositions::const_reverse_iterator last = pathShape.shape.rbegin();
		ListPositions::const_iterator next(cur);
		++next;
		
		float	lengthTmp = 0.0f;
		while( next != pathShape.shape.end())
		{
			currentSegLength = ( *cur - *next ).GetLength();
			if ( currentLength + currentSegLength > segLength )
				break;
			currentLength += currentSegLength;
			cur = next;
			++next;
		}

		Vec3 vDef = *next - *cur;
		
		float delta = 0.0f;
		if ( currentSegLength > 0.0 )
			delta = ( segLength - currentLength ) / currentSegLength;
		else
			delta = ( segLength - currentLength );

		vResult = *cur + ( vDef * delta );

	}

}

const char*	CAISystem::GetNearestPathOfTypeInRange(IAIObject* requester, const Vec3& reqPos, float range, int type, float devalue, bool useStartNode)
{
	AIAssert(requester);
	float	rangeSq(sqr(range));

	ShapeMap::iterator	closestShapeIt(m_mapDesignerPaths.end());
	float closestShapeDist(rangeSq);
	CAIActor* pReqActor = requester->CastToCAIActor();

	for(ShapeMap::iterator it = m_mapDesignerPaths.begin(); it != m_mapDesignerPaths.end(); ++it)
	{
		const SShape&	path = it->second;

		// Skip paths which we cannot travel
		if((path.navType & pReqActor->GetMovementAbility().pathfindingProperties.navCapMask) == 0)
			continue;
		// Skip wrong type
		if(path.type != type)
			continue;
		// Skip locked paths
		if(path.devalueTime > 0.01f)
			continue;

		// Skip paths too far away
		Vec3	tmp;
		if(Distance::Point_AABBSq(reqPos, path.aabb, tmp) > rangeSq)
			continue;

		float	d;
		if(useStartNode)
		{
			// Disntance to start node.
			d = Distance::Point_PointSq(reqPos, path.shape.front());
		}
		else
		{
			// Distance to nearest point on path.
			ListPositions::const_iterator	nearest = path.NearestPointOnPath(reqPos, d, tmp);
		}

		if(d < closestShapeDist)
		{
			closestShapeIt = it;
			closestShapeDist = d;
		}
	}

	if(closestShapeIt != m_mapDesignerPaths.end())
	{
		closestShapeIt->second.devalueTime = devalue;
		return closestShapeIt->first.c_str();
	}

	return 0;
}

//====================================================================
// GetEnclosingGenericShapeOfType
//====================================================================
const char*	CAISystem::GetEnclosingGenericShapeOfType(const Vec3& reqPos, int type, bool checkHeight)
{
	ShapeMap::iterator end = m_mapGenericShapes.end();
	ShapeMap::iterator nearest = end;
	float	nearestDist = FLT_MAX;
	for(ShapeMap::iterator it = m_mapGenericShapes.begin(); it != end; ++it)
	{
		const SShape&	shape = it->second;
		if(!shape.enabled) continue;
		if(shape.type != type) continue;
		if(shape.IsPointInsideShape(reqPos, checkHeight))
		{
			float dist = 0;
			Vec3	pt;
			shape.NearestPointOnPath(reqPos, dist, pt);
			if(dist < nearestDist)
			{
				nearest = it;
				nearestDist = dist;
			}
		}
	}

	if(nearest != end)
		return nearest->first.c_str();

	return 0;
}

//====================================================================
// DistanceToGenericShape
//====================================================================
float CAISystem::DistanceToGenericShape(const Vec3& reqPos, const char* shapeName, bool checkHeight)
{
	if(!shapeName || shapeName[0] == 0)
		return 0.0f;

	ShapeMap::iterator	it = m_mapGenericShapes.find(shapeName);
	if(it == m_mapGenericShapes.end())
	{
		AIWarning("CAISystem::DistanceToGenericShape Unable to find generic shape called %s", shapeName);
		return 0.0f;
	}
	const SShape&	shape = it->second;
	float	dist;
	Vec3	nearestPt;
	shape.NearestPointOnPath(reqPos, dist, nearestPt);
	if(checkHeight)
		return dist;
	return Distance::Point_Point2D(reqPos, nearestPt);
}

//====================================================================
// IsPointInsideGenericShape
//====================================================================
bool CAISystem::IsPointInsideGenericShape(const Vec3& reqPos, const char* shapeName, bool checkHeight)
{
	if(!shapeName || shapeName[0] == 0)
		return false;

	ShapeMap::iterator	it = m_mapGenericShapes.find(shapeName);
	if(it == m_mapGenericShapes.end())
	{
		AIWarning("CAISystem::IsPointInsideGenericShape Unable to find generic shape called %s", shapeName);
		return false;
	}
	const SShape&	shape = it->second;
	return shape.IsPointInsideShape(reqPos, checkHeight);
}

//====================================================================
// ConstrainInsideGenericShape
//====================================================================
bool CAISystem::ConstrainInsideGenericShape(Vec3& pos, const char* shapeName, bool checkHeight)
{
	if(!shapeName || shapeName[0] == 0)
		return false;

	ShapeMap::iterator	it = m_mapGenericShapes.find(shapeName);
	if(it == m_mapGenericShapes.end())
	{
		AIWarning("CAISystem::ConstrainInsideGenericShape Unable to find generic shape called %s", shapeName);
		return false;
	}
	const SShape&	shape = it->second;
	return shape.ConstrainPointInsideShape(pos, checkHeight);
}

//====================================================================
// GetGenericShapeOfName
//====================================================================
SShape* CAISystem::GetGenericShapeOfName(const char* shapeName)
{
	if(!shapeName || shapeName[0] == 0)
		return 0;

	ShapeMap::iterator	it = m_mapGenericShapes.find(shapeName);
	if(it == m_mapGenericShapes.end())
		return 0;
	return &it->second;
}

//====================================================================
// CreateTemporaryGenericShape
//====================================================================
const char*	CAISystem::CreateTemporaryGenericShape(Vec3* points, int npts, float height, int type)
{
	if(npts < 2)
		return 0;

	// Make sure the shape is wound clockwise.
	std::vector<Vec3> vecPts(points, points + npts);
	EnsureShapeIsWoundAnticlockwise<std::vector<Vec3>, float>(vecPts);
	ListPositions listPts(vecPts.begin(), vecPts.end());

	// Create random name for the shape.
	char	name[16];
	ShapeMap::iterator di;
	do 
	{
		_snprintf(name, 16, "Temp%08x", ai_rand());
		di = m_mapGenericShapes.find(name);
	}
	while(di != m_mapGenericShapes.end());

	std::pair<ShapeMap::iterator, bool > pr;

	pr = m_mapGenericShapes.insert(ShapeMap::iterator::value_type(name, SShape(listPts, false, IAISystem::NAV_UNSET, type, height, AILL_NONE, true)));
	if(pr.second == true)
		return (pr.first)->first.c_str();
	return 0;
}

//====================================================================
// EnableGenericShape
//====================================================================
void CAISystem::EnableGenericShape(const char* shapeName, bool state)
{
	if(!shapeName || strlen(shapeName) < 1)
		return;
	ShapeMap::iterator	it = m_mapGenericShapes.find(shapeName);
	if(it == m_mapGenericShapes.end())
	{
		AIWarning("CAISystem::EnableGenericShape Unable to find generic shape called %s", shapeName);
		return;
	}

	SShape&	shape = it->second;

	// If the new state is the same, no need to inform the users.
	if(shape.enabled == state)
		return;

	// Change the state of the shape
	shape.enabled = state;

	// Notify the puppets that are using the shape that the shape state has changed.
	AIObjects::const_iterator ai = m_Objects.find(AIOBJECT_PUPPET);
	for(; ai != m_Objects.end(); ++ai)
	{
		if (ai->first != AIOBJECT_PUPPET)
			break;

		CAIObject* obj = ai->second;
		CPuppet* puppet = obj->CastToCPuppet();
		if(!puppet)
			continue;

		if(state)
		{
			// Shape enabled
			if(shape.IsPointInsideShape(puppet->GetPos(), true))
			{
				IAISignalExtraData* pData = CreateSignalExtraData();
				pData->SetObjectName(shapeName);
				pData->iValue = shape.type;
				puppet->SetSignal(1, "OnShapeEnabled", puppet->GetEntity(), pData, g_crcSignals.m_nOnShapeEnabled);
			}
		}
		else
		{
			// Shape disabled
			int	val = 0;
			if(puppet->GetRefShapeName() && strcmp(puppet->GetRefShapeName(), shapeName) == 0)
				val |= 1;
			if(puppet->GetTerritoryShapeName() && strcmp(puppet->GetTerritoryShapeName(), shapeName) == 0)
				val |= 2;
			if(val)
			{
				IAISignalExtraData* pData = CreateSignalExtraData();
				pData->iValue = val;
				puppet->SetSignal(1, "OnShapeDisabled", puppet->GetEntity(), pData, g_crcSignals.m_nOnShapeDisabled);
			}
		}
	}
}

//====================================================================
// AIActivtyInShape
//====================================================================
void CAISystem::AIActivtyInShape(EAITerritoryAction action, const char* shapeName, int species, EAINavigationFilter nav_filter)
{
	if(!shapeName || strlen(shapeName) < 1)
		return;
	ShapeMap::iterator	it = m_mapGenericShapes.find(shapeName);
	if(it == m_mapGenericShapes.end())
	{
		AIWarning("CAISystem::AIActivtyInShape Unable to find generic shape called %s", shapeName);
		return;
	}

	SShape&	shape = it->second;

	IGameRulesSystem *pGSys=NULL;
	IGameRules *pGRules=NULL;

	if (action == AITA_KILL)
	{
		pGSys=gEnv->pGame ? gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem() : NULL;
		pGRules=pGSys ? pGSys->GetCurrentGameRules() : NULL;

		if (!pGRules)
		{
			AIWarning("CAISystem::AIActivtyInShape Cannot kill with no game rules defined!!!");		
			return;
		}
	}

	// Notify the puppets that are using on AI Activity change
	AIObjects::const_iterator ai = m_Objects.find(AIOBJECT_PUPPET);
	for(; ai != m_Objects.end(); ++ai)
	{
		if (ai->first != AIOBJECT_PUPPET)
			break;

		CAIObject* obj = ai->second;
		CPuppet* puppet = obj->CastToCPuppet();
		if(!puppet)
			continue;

		if (species > 0 && puppet->m_Parameters.m_nSpecies != species)
			continue;

		if (nav_filter > AINF_ALL)
		{
			AgentMovementAbility mabl=puppet->GetMovementAbility();
			
			if ((nav_filter == AINF_AIR_ONLY) != mabl.b3DMove) // XOR
				continue;
		}

		// Shape enabled
		if(shape.IsPointInsideShape(puppet->GetPos(), false))
		{
			IEntity *pEnt=puppet->GetEntity();


			if (!pEnt)
				continue;

			// Check vehicle
			SAIBodyInfo bodyInfo;
			puppet->GetProxy()->QueryBodyInfo(bodyInfo);

			IEntity *pVehicle=bodyInfo.linkedVehicleEntity;


			if (action == AITA_KILL)
			{
				HitInfo hitInfo;
				hitInfo.damage=0;
				hitInfo.targetId=pEnt->GetId();
				hitInfo.type=pGRules->GetHitTypeId("event");
				pGRules->ServerHit(hitInfo);
			}
			else
			{
				pEnt->Hide(action == AITA_DISABLE);
				pEnt->Activate(action == AITA_ENABLE);

				if (pVehicle)
				{
					pVehicle->Hide(action == AITA_DISABLE);
					pVehicle->Activate(action == AITA_ENABLE);
				}

				if (action == AITA_DISABLE)
				puppet->Reset(AIOBJRESET_INIT);
		}
	}
}
}

//====================================================================
// SetUseAutoNavigation
//====================================================================
void CAISystem::SetUseAutoNavigation(const char *szPathName, EWaypointConnections waypointConnections)
{
	SpecialAreaMap::iterator di;
	di = m_mapSpecialAreas.find(szPathName);

	if (di== m_mapSpecialAreas.end())
	{
		AIWarning("CAISystem::SetUseAutoNavigation Area %s not found", szPathName);
		return;
	}

	SpecialArea& sa = di->second;

	sa.waypointConnections = waypointConnections;

  GetWaypointHumanNavRegion()->RemoveAutoLinksInBuilding(sa.nBuildingID);
	GetWaypointHumanNavRegion()->ReconnectWaypointNodesInBuilding(sa.nBuildingID);
}


//====================================================================
// GetSpecialArea
//====================================================================
const SpecialArea * CAISystem::GetSpecialArea(int buildingID) const
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);
	if (! m_pSystem->IsEditor())
	{
		size_t size = m_vectorSpecialAreas.size();

		assert(buildingID>=0 && buildingID<size);
		return m_vectorSpecialAreas[buildingID];
	}

	for (SpecialAreaMap::const_iterator di = m_mapSpecialAreas.begin() ; 
	di != m_mapSpecialAreas.end() ; ++di)
	{
		if (di->second.nBuildingID == buildingID)
			return &(di->second);
	}
	return 0;
}

void CAISystem::InsertSpecialArea(const string & name, SpecialArea & sa)
{
	std::pair <SpecialAreaMap::iterator, bool> result =
			m_mapSpecialAreas.insert(SpecialAreaMap::iterator::value_type(name,sa));

	if (! m_pSystem->IsEditor())
	{
		if (m_vectorSpecialAreas.size() <= sa.nBuildingID)
			m_vectorSpecialAreas.resize (2*m_vectorSpecialAreas.size());
		m_vectorSpecialAreas[sa.nBuildingID] = & result.first->second;
	}
}

void CAISystem::EraseSpecialArea(const string & name)
{
	SpecialAreaMap::iterator si;
	si = m_mapSpecialAreas.find(name);
	if (si== m_mapSpecialAreas.end())
		return;

	if (! m_pSystem->IsEditor())
	{
		m_vectorSpecialAreas[si->second.nBuildingID] = 0;
	}

	if (si->second.nBuildingID>=0)
		m_BuildingIDManager.FreeId(si->second.nBuildingID);
	m_mapSpecialAreas.erase(si);
}

void CAISystem::FlushSpecialAreas()
{
	m_mapSpecialAreas.clear();
	if (! m_pSystem->IsEditor())
	{
		m_vectorSpecialAreas.resize(0);
		// NOTE Mai 6, 2007: <pvl> 64 should be enough for most levels I guess. If
		// it's not for some level, InsertSpecialArea() will grow the vector.
		m_vectorSpecialAreas.resize(64,0);
	}
}

void CAISystem::FlushAllAreas()
{
	FlushSpecialAreas();

	m_designerForbiddenAreas.Clear();
	m_forbiddenAreas.Clear();
	m_forbiddenBoundaries.Clear();

	m_mapDesignerPaths.clear();
	m_mapOcclusionPlanes.clear();
	m_mapGenericShapes.clear();
	m_mapExtraLinkCosts.clear();
	m_BuildingIDManager.FreeAll();					// Mikko/Martin - id manager should be reset
}


//===================================================================
// GetSpecialAreaNearestPos
//===================================================================
const SpecialArea *CAISystem::GetSpecialAreaNearestPos(const Vec3 &pos, SpecialArea::EType areaType)
{
  const SpecialArea *pSA = GetSpecialArea(pos, areaType);
  if (pSA)
    return pSA;

  float bestDist = std::numeric_limits<float>::max();
  pSA = 0;
	for (SpecialAreaMap::const_iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
  {
		const SpecialArea &sa = si->second;
    if (sa.type == areaType)
    {
      if ( sa.fHeight>0.00001f && (pos.z < sa.fMinZ || pos.z > sa.fMaxZ) )
		    continue;
      Vec3 polyPt;
      float dist = Distance::Point_Polygon2DSq(pos, sa.GetPolygon(), polyPt);
		  if (dist < bestDist)
		  {
        bestDist = dist;
        pSA = &sa;
      }
    }
  }
  return pSA;
}

//====================================================================
// GetSpecialArea
//====================================================================
const SpecialArea *CAISystem::GetSpecialArea(const Vec3 &pos, SpecialArea::EType areaType)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

  // make sure each area has a building id
	// Flight/water navigation modifiers are only used to limit the initial navigation 
	// area during preprocessing - give them a building id anyway
	for (SpecialAreaMap::iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
  {
		SpecialArea &sa = si->second;
		if (sa.nBuildingID<0)
			(si->second).nBuildingID = m_BuildingIDManager.GetId();
  }  

	for (SpecialAreaMap::const_iterator si=m_mapSpecialAreas.begin();si!=m_mapSpecialAreas.end();++si)
  {
		const SpecialArea &sa = si->second;
    if (sa.type == areaType)
    {
		  if (IsPointInSpecialArea(pos, sa))
		  {
        return &sa;
      }
    }
  }
  return 0;
}


//====================================================================
// GetVolumeRegions
//====================================================================
void CAISystem::GetVolumeRegions(VolumeRegions& volumeRegions)
{
  volumeRegions.resize(0);
	for (SpecialAreaMap::const_iterator di = m_mapSpecialAreas.begin() ; di != m_mapSpecialAreas.end() ; ++di)
	{
    if (di->second.type == SpecialArea::TYPE_VOLUME)
      volumeRegions.push_back(std::make_pair(di->first, &di->second));
	}
}

//====================================================================
// GetOccupiedHideObjectPositions
//====================================================================
void CAISystem::GetOccupiedHideObjectPositions(const CPipeUser* pRequester, std::vector<Vec3>& hideObjectPositions)
{
	// Iterate over the list of active puppets and collect positions of the valid hidespots in use.
	hideObjectPositions.clear();
	
	for (unsigned j = 0, nj = m_enabledPuppetsSet.size(); j < nj; ++j)
	{
		CPipeUser* pOther = (CPipeUser*)m_enabledPuppetsSet[j];
		if (pRequester == pOther)
			continue;
		CAIHideObject& hide(pOther->m_CurrentHideObject);
		// Skip invalid hidespots.
		if (!hide.IsValid())
			continue;
		hideObjectPositions.push_back(hide.GetObjectPos());
	}
}

//====================================================================
// GetExtraLinkCost
//====================================================================
float CAISystem::GetExtraLinkCost(const Vec3 &pos1, const Vec3 &pos2, const AABB &linkAABB, const SExtraLinkCostShape &shape) const
{
  // Danny todo implement this properly. For now just return the full factor if there's any intersection - really
  // we should return just a fraction of the factor, depending on the extent of intersection.

  if (Overlap::Point_Polygon2D(pos1, shape.shape, &shape.aabb) || 
      Overlap::Point_Polygon2D(pos2, shape.shape, &shape.aabb) || 
      Overlap::Lineseg_Polygon2D(Lineseg(pos1, pos2), shape.shape, &shape.aabb))
  {
    return shape.costFactor;
  }
  else
  {
    return 0.0f;
  }
}

//====================================================================
// OverlapExtraCostAreas
//====================================================================
bool CAISystem::OverlapExtraCostAreas(const Lineseg & lineseg) const
{
  if (m_mapExtraLinkCosts.empty())
    return false;

  AABB linkAABB(AABB::RESET);
  linkAABB.Add(lineseg.start);
  linkAABB.Add(lineseg.end);

  for (ExtraLinkCostShapeMap::const_iterator it = m_mapExtraLinkCosts.begin() ; it != m_mapExtraLinkCosts.end() ; ++it)
  {
    const SExtraLinkCostShape &shape = it->second;
    if (Overlap::AABB_AABB(shape.aabb, linkAABB))
    {
      float cost = GetExtraLinkCost(lineseg.start, lineseg.end, linkAABB, shape);
      if (cost < 0.0f || cost > 0.0001f)
        return true;
    }
  }
  return false;
}


//====================================================================
// GetExtraLinkCost
//====================================================================
float CAISystem::GetExtraLinkCost(const Vec3 &pos1, const Vec3 &pos2) const
{
  if (m_mapExtraLinkCosts.empty())
    return 0.0f;

  AABB linkAABB(AABB::RESET);
  linkAABB.Add(pos1);
  linkAABB.Add(pos2);

  float factor = 0.0f;
  for (ExtraLinkCostShapeMap::const_iterator it = m_mapExtraLinkCosts.begin() ; it != m_mapExtraLinkCosts.end() ; ++it)
  {
    const SExtraLinkCostShape &shape = it->second;
    if (Overlap::AABB_AABB(shape.aabb, linkAABB))
    {
      float cost = GetExtraLinkCost(pos1, pos2, linkAABB, shape);
      if (cost < 0.0f)
        return -1.0f;
      factor += cost;
    }
  }
  return factor;
}


//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::IntersectsForbidden(const Vec3& start, const Vec3& end, Vec3& closestPoint,
																		const string* nameToSkip, Vec3* pNormal, EIFMode mode, bool bForceNormalOutwards) const
{
  FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

  if (m_forbiddenAreas.GetShapes().empty() && m_forbiddenBoundaries.GetShapes().empty())
		return false;
	
  switch(mode)
  {
  case IF_AREASBOUNDARIES: 
		if (m_forbiddenAreas.IntersectLineSeg(start, end, closestPoint, pNormal))
			return true;
		if (m_forbiddenBoundaries.IntersectLineSeg(start, end, closestPoint, pNormal, bForceNormalOutwards))
			return true;
		break;
  case IF_AREAS: // areas only
		if (m_forbiddenAreas.IntersectLineSeg(start, end, closestPoint, pNormal))
			return true;
    break;
  case IF_BOUNDARIES: // boundaries only
		if (m_forbiddenBoundaries.IntersectLineSeg(start, end, closestPoint, pNormal, bForceNormalOutwards))
			return true;
    break;
  default:
    AIError("Bad mode %d passed to CAISystem::IntersectsForbidden", mode);
		if (m_forbiddenAreas.IntersectLineSeg(start, end, closestPoint, pNormal, bForceNormalOutwards))
			return true;
		if (m_forbiddenBoundaries.IntersectLineSeg(start, end, closestPoint, pNormal, bForceNormalOutwards))
			return true;
    break;
  }
	return false;
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::IntersectsSpecialArea(const Vec3 & vStart, const Vec3 & vEnd, Vec3 & vClosestPoint)
{
	if (m_mapSpecialAreas.empty())
		return false;

  Lineseg lineseg(vStart, vEnd);
	SpecialAreaMap::iterator iend = m_mapSpecialAreas.end();
	for (SpecialAreaMap::iterator fi = m_mapSpecialAreas.begin() ; fi != iend ; ++fi)
	{
		Vec3 result;
    if (Overlap::Lineseg_AABB2D(lineseg, fi->second.GetAABB()))
    {
		  if (Intersect::Lineseg_Polygon2D(lineseg, fi->second.GetPolygon(), result))
		  {
			  vClosestPoint = result;
			  return true;
		  }
    }
		++fi;
	}
	return false;
}

//===================================================================
// DoesShapeSelfIntersect
//===================================================================
#ifdef AI_FP_FAST
#pragma float_control(precise, on)
#pragma fenv_access(on)
#endif
bool DoesShapeSelfIntersect(const ShapePointContainer &shape, Vec3 &badPt)
{
  ShapePointContainer::const_iterator liend = shape.end();

	// Check for degenerate edges.
  for (ShapePointContainer::const_iterator li=shape.begin() ; li!=liend ; ++li)
  {
    ShapePointContainer::const_iterator linext = li;
    ++linext;
    if (linext == liend)
      linext = shape.begin();

    if ( IsEquivalent2D((*li),(*linext),0.001f) )
    {
			badPt = *li;

			char msg[256];
			_snprintf(msg, 256, "Degenerate edge\n(%.f, %.f, %.f).", badPt.x, badPt.y, badPt.z);
			GetAISystem()->m_validationErrorMarkers.push_back(CAISystem::SValidationErrorMarker(msg, badPt, ColorB(255,0,0)));

      return true;
    }
  }

  for (ShapePointContainer::const_iterator li=shape.begin() ; li!=liend ; ++li)
  {
    ShapePointContainer::const_iterator linext = li;
    ++linext;
    if (linext == liend)
      linext = shape.begin();

    // check for self-intersection with all remaining segments
    int posIndex1 = 0;
    ShapePointContainer::const_iterator li1,linext1;
    for (li1 = shape.begin() ; li1 != liend ; ++li1, ++posIndex1)
    {
      linext1 = li1;
      ++linext1;
      if (linext1 == liend)
        linext1 = shape.begin();

      if (li1 == linext)
        continue;
      if (linext1 == li)
        continue;
      if (li1 == li)
        continue;
      real s, t;
      if (Intersect::Lineseg_Lineseg2D(Linesegr(*li, *linext), Linesegr(*li1, *linext1), s, t))
      {
        badPt = Lineseg(*li, *linext).GetPoint(s);

				char msg[256];
        _snprintf(msg, 256, "Self-intersection\n(%.f, %.f, %.f)", badPt.x, badPt.y, badPt.z);
        GetAISystem()->m_validationErrorMarkers.push_back(CAISystem::SValidationErrorMarker(msg, badPt, ColorB(255,0,0)));

        return true;
      }
    }
  }
  return false;
}
#ifdef AI_FP_FAST
#pragma fenv_access(off)
#pragma float_control(precise, off)
#endif


// NOTE Jun 7, 2007: <pvl> not const any more, needs to write to m_validationErrorMarkers
//===================================================================
// ForbiddenAreasOverlap
//===================================================================
bool CAISystem::ForbiddenAreasOverlap()
{
	bool res = false;
	for (unsigned i = 0, ni = m_forbiddenAreas.GetShapes().size(); i < ni; ++i)
	{
		if (ForbiddenAreaOverlap(m_forbiddenAreas.GetShapes()[i]))
			res = true;
	}
	for (unsigned i = 0, ni = m_forbiddenBoundaries.GetShapes().size(); i < ni; ++i)
	{
		if (ForbiddenAreaOverlap(m_forbiddenBoundaries.GetShapes()[i]))
			res = true;
	}
	return res;
}

//===================================================================
// ForbiddenAreasOverlap
//===================================================================
#ifdef AI_FP_FAST
#pragma float_control(precise, on)
#pragma fenv_access(on)
#endif
bool CAISystem::ForbiddenAreaOverlap(const CAIShape* pShape)
{
	const ShapePointContainer& pts = pShape->GetPoints();
	const string& name = pShape->GetName();

	if (pts.size() < 3)
	{
		AIError("Forbidden area/boundary %s has less than 3 points - you must delete it and rerun triangulation [Design bug]", name.c_str());
		return true;
	}

	for (unsigned i = 0, ni = pts.size(); i < ni; ++i)
	{
		const Vec3& v0 = pts[i];
		const Vec3& v1 = pts[(i+1) % ni];

		if (IsEquivalent2D(v0, v1, 0.001f))
		{
			AIError("Forbidden area/boundary %s contains one or more identical points (%5.2f, %5.2f, %5.2f) - Fix (e.g. adjust position of forbidden areas/objects) and run triangulation again [Design bug... possibly]",
        name.c_str(), v0.x, v0.y, v0.z);

			char msg[256];
			_snprintf(msg, 256, "Identical points\n(%.f, %.f, %.f).", v0.x, v0.y, v0.z);
			m_validationErrorMarkers.push_back(SValidationErrorMarker(msg, v0, ColorB(255,0,0)));

			return true;
		}
  }

	for (unsigned i = 0, ni = pts.size(); i < ni; ++i)
	{
		unsigned ine = (i+1) % ni;
		const Vec3& vi0 = pts[i];
		const Vec3& vi1 = pts[ine];

		// check for self-intersection with all remaining segments
		for (unsigned j = 0, nj = pts.size(); j < nj; ++j)
		{
			unsigned jne = (j+1) % nj;
			if (j == ine || jne == i || i == j)
				continue;

			const Vec3& vj0 = pts[j];
			const Vec3& vj1 = pts[jne];

			real s, t;
			if (Intersect::Lineseg_Lineseg2D(Linesegr(vi0, vi1), Linesegr(vj0, vj1), s, t))
			{
        Vec3 pos = Lineseg(vi0, vi1).GetPoint(s);
				AIError("Forbidden area/boundary %s self-intersects - (%5.2f, %5.2f, %5.2f). Please fix (e.g. adjust position of forbidden areas/objects) and re-triangulate [Design bug... possibly]",
          name.c_str(), pos.x, pos.y, pos.z);

				char msg[256];
				_snprintf(msg, 256, "Self-intersection\n(%.f, %.f, %.f).", pos.x, pos.y, pos.z);
				m_validationErrorMarkers.push_back(SValidationErrorMarker(msg, pos, ColorB(255,0,0)));

				return true;
			}
		}
		Vec3 pt;
		if (IntersectsForbidden(vi0, vi1, pt, &name))
		{
			AIError("!Forbidden area %s intersects with another forbidden area (%5.2f, %5.2f, %5.2f) . Please fix (e.g. adjust position of forbidden areas/objects) and re-triangulate [Design bug... possibly]",
        name.c_str(), pt.x, pt.y, pt.z);

			char msg[256];
			_snprintf(msg, 256, "Mutual-intersection\n(%.f, %.f, %.f).", pt.x, pt.y, pt.z);
			m_validationErrorMarkers.push_back(SValidationErrorMarker(msg, pt, ColorB(255,0,0)));

			return true;
		}
	}

	return false;
}
#ifdef AI_FP_FAST
#pragma fenv_access(off)
#pragma float_control(precise, off)
#endif

//===================================================================
// FindMarkNodeBy2Vertex
//===================================================================
GraphNode*	CAISystem::FindMarkNodeBy2Vertex(CGraphLinkManager& linkManager, int vIdx1, int vIdx2, GraphNode* exclude, ListNodes &lstNewNodes)
{
	ListNodes::iterator nItr=lstNewNodes.begin();

	for(;nItr!=lstNewNodes.end(); ++nItr)
	{
		GraphNode* curNode = (*nItr);
		if  ((curNode == exclude) || (curNode == m_pGraph->m_pSafeFirst))
			continue;
		for (unsigned link=curNode->firstLinkIndex;link;link = linkManager.GetNextLink(link))
			if( curNode->GetTriangularNavData()->vertices[linkManager.GetStartIndex(link)] == vIdx1 && curNode->GetTriangularNavData()->vertices[linkManager.GetEndIndex(link)] == vIdx2 ||
				curNode->GetTriangularNavData()->vertices[linkManager.GetStartIndex(link)] == vIdx2 && curNode->GetTriangularNavData()->vertices[linkManager.GetEndIndex(link)] == vIdx1 )
			{
				linkManager.SetRadius(link, -1.f);
				return curNode;
			}
			
	}
	return NULL;
}

//===================================================================
// CreatePossibleCutList
//===================================================================
#ifdef AI_FP_FAST
#pragma float_control(precise, on)
#pragma fenv_access(on)
#endif
void	CAISystem::CreatePossibleCutList( const Vec3 & vStart, const Vec3 & vEnd, ListNodeIds & lstNodes )
{
  if (doForbiddenDebug)
    AILogAlways("%d: CreatePossibleCutList start (%7.4f, %7.4f) end (%7.4f, %7.4f)",
    debugForbiddenCounter, vStart.x, vStart.y, vEnd.x, vEnd.y);

	float		offset=.5f;
	Vec3 pos = vStart+offset*(vEnd-vStart);
	unsigned currentIndex = GetTriangularNavRegion(m_pGraph)->GetEnclosing(pos);
	GraphNode *pCurNode = m_pGraph->GetNodeManager().GetNode(currentIndex);

	if (!pCurNode)
	{
		AIError("Unable to find graph node enclosing (%5.2f, %5.2f, %5.2f) [Code bug]", pos.x, pos.y, pos.z);
		return;
	}

	ListNodeIds queueList;
	lstNodes.clear();
	queueList.clear();
	m_pGraph->ClearTags();

	while( !SegmentInTriangle(pCurNode, vStart, vEnd) )
	{
		offset *=.5f;
		currentIndex = GetTriangularNavRegion(m_pGraph)->GetEnclosing(vStart+offset*(vEnd-vStart));
		pCurNode = m_pGraph->GetNode(currentIndex);
		if( offset< .0001f )
		{
      if (doForbiddenDebug)
        AILogAlways("%d: failed seg in triangle part", debugForbiddenCounter);
			// can't find  node on the edge
//			AIAssert( 0 );
			return;
		}
	}
  if (doForbiddenDebug)
    AILogAlways("%d: CreatePossibleCutList offset = %7.4f, node at (%7.4f, %7.4f)", 
    debugForbiddenCounter, offset, pCurNode->GetPos().x, pCurNode->GetPos().y);

	m_pGraph->TagNode( currentIndex );
	queueList.push_back( currentIndex );

	while( !queueList.empty() )
	{
		currentIndex = queueList.front();
		pCurNode = m_pGraph->GetNodeManager().GetNode(currentIndex);;
		queueList.pop_front();
		lstNodes.push_back( currentIndex );

		for( unsigned link=pCurNode->firstLinkIndex; link; link = m_pGraph->GetLinkManager().GetNextLink(link) )
		{
			unsigned candidateIndex = m_pGraph->GetLinkManager().GetNextNode(link);
			GraphNode *pCandidate = m_pGraph->GetNodeManager().GetNode(candidateIndex);
			if( pCandidate->tag )
				continue;
			m_pGraph->TagNode(candidateIndex);
			++debugForbiddenCounter;
			if( !SegmentInTriangle(pCandidate, vStart, vEnd) )
			{
				if (doForbiddenDebug)
					AILogAlways("%d:          reject node at (%7.4f, %7.4f)", debugForbiddenCounter, pCandidate->GetPos().x, pCandidate->GetPos().y);
				continue;
			}
			else
			{
				if (doForbiddenDebug)
					AILogAlways("%d:       -> accept node at (%7.4f, %7.4f)", debugForbiddenCounter, pCandidate->GetPos().x, pCandidate->GetPos().y);
				queueList.push_back( candidateIndex );
			}
		}
	}
	m_pGraph->ClearTags();
	for(ListNodeIds::iterator nI=lstNodes.begin();nI!=lstNodes.end();++nI)
		m_pGraph->TagNode(*nI);
	return;
}
#ifdef AI_FP_FAST
#pragma fenv_access(off)
#pragma float_control(precise, off)
#endif


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::NotifyEnableState(CPuppet* pPuppet, bool state)
{
	if (state)
	{
		m_disabledPuppetsSet.erase(pPuppet);
		m_enabledPuppetsSet.insert(pPuppet);
	}
	else
	{
		pPuppet->ClearProbableTargets();
		m_disabledPuppetsSet.insert(pPuppet);
		m_enabledPuppetsSet.erase(pPuppet);
	}
}

//
//-----------------------------------------------------------------------------------------------------------
const ObstacleData CAISystem::GetObstacle(int nIndex)
{
	return m_VertexList.GetVertex(nIndex);
}

// it removes all references to this object from all objects of the specified type
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::RemoveObjectFromAllOfType(int nType, CAIObject* pRemovedObject)
{
	AIObjects::iterator ai = m_Objects.lower_bound(nType);
	AIObjects::iterator end = m_Objects.end();
	for (; ai != end && ai->first == nType; ++ai)
		(ai->second)->OnObjectRemoved(pRemovedObject);
}

//TODO: find better solution - not to send a signal from there but notify AIProxy - make AIHAndler send the signal, same way OnPlayerSeen works
//
// sand signal to all objects of type nType, which have the pDeadObject as attention targer
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::NotifyTargetDead(CAIObject* pDeadObject)
{
	if (!pDeadObject)
		return;

	AIObjects::iterator ai = m_Objects.find(AIOBJECT_PUPPET);
	AIObjects::iterator end = m_Objects.end();
	// tell all objects of nType that this object is considered invalid
	for ( ; ai != end && ai->first == AIOBJECT_PUPPET; ++ai)
	{
		CPipeUser *pPipeUser = ai->second->CastToCPipeUser();
		if (pPipeUser)
		{
			if (pPipeUser->GetAttentionTarget() == pDeadObject)
				pPipeUser->SetSignal(0, "OnTargetDead", NULL, 0, g_crcSignals.m_nOnTargetDead);
		}
	}

	// Check if the death of the actor was part of any player stunt.
	CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
	CAIActor* pDeadActor = pDeadObject->CastToCAIActor();
	if (pPlayer && pDeadActor)
	{
		if (pPlayer->IsPlayerStuntAffectingTheDeathOf(pDeadActor))
		{
			// Skip the nearest thrown entity, since it is potentially blocking the view to the corpse.
			EntityId nearestThrownEntId = pPlayer->GetNearestThrownEntity(pDeadActor->GetPos());
			IEntity* pNearestThrownEnt = nearestThrownEntId ? gEnv->pEntitySystem->GetEntity(nearestThrownEntId) : 0;
			IPhysicalEntity* pNearestThrownEntPhys = pNearestThrownEnt ? pNearestThrownEnt->GetPhysics() : 0;

			short gid = (short)pDeadObject->GetGroupId();
			AIObjects::iterator ai = m_mapGroups.find(gid);
			AIObjects::iterator end = m_mapGroups.end();
			for ( ; ai != end && ai->first == gid; ++ai)
			{
				CPuppet* pPuppet = ai->second->CastToCPuppet();
				if (!pPuppet) continue;
				if (pPuppet->GetEntityID() == pDeadActor->GetEntityID()) continue;
				float dist = FLT_MAX;
				if (!CheckVisibilityToBody(pPuppet, pDeadActor, dist, pNearestThrownEntPhys))
					continue;
				pPuppet->SetSignal(1, "OnGroupMemberMutilated", pDeadObject->GetEntity(), 0);
			}
		}
	}

}


//-----------------------------------------------------------------------------------------------------------
bool CAISystem::IsPointOnForbiddenEdge(const Vec3 & pos, float tol, Vec3* pNormal, 
                                       const CAIShape** ppShape, bool checkAutoGenRegions)
{
  FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	if (checkAutoGenRegions)
	{
		if (m_forbiddenAreas.IsPointOnEdge(pos, tol, ppShape))
			return true;
	}
	else
	{
		if (m_designerForbiddenAreas.IsPointOnEdge(pos, tol, ppShape))
			return true;
	}

	if (m_forbiddenBoundaries.IsPointOnEdge(pos, tol, ppShape))
		return true;

	return false;
}


//====================================================================
// IsFlightSpaceVoid
// check if the space is void inside of the box which is made by these 8 point
//====================================================================
Vec3 CAISystem::IsFlightSpaceVoid(const Vec3 &vPos, const Vec3 &vFwd, const Vec3 &vWng, const Vec3 &vUp )
{
	CFlightNavRegion* pNav = GetFlightNavRegion();
	if ( pNav )
		return pNav ->IsSpaceVoid(vPos,vFwd,vWng,vUp);

	return Vec3(ZERO);
}

//====================================================================
// IsFlightSpaceVoidByRadius
//====================================================================
Vec3 CAISystem::IsFlightSpaceVoidByRadius(const Vec3 &vPosInput, const Vec3 &vFwdInput, float radius )
{

	Vec3 vUp(0.0,0.0,1.0f);
	Vec3 vWng;
	Vec3 vFwd = vFwdInput;
	Vec3 vPos = vPosInput;

	float fwdLen = vFwd.GetLength();
	vFwd.NormalizeSafe();

	if ( fabs(vUp.Dot(vFwd)) > cos( DEG2RAD(0.5f) ) )
	{
		vUp = Vec3( 0.0f, 0.0f, vFwd.z * fwdLen );
		vWng = Vec3( 1.0f, 0.0f, 0.0f );
		vFwd = Vec3( 0.0f, 1.0f, 0.0f );

		vPos -= ( vWng * radius );
		vPos -= ( vFwd * radius  );
		vWng *= radius * 2.0f;
		vFwd *= radius * 2.0f;
	}
	else
	{
		vFwd.NormalizeSafe();
		vWng =vFwd.Cross( vUp );
		vWng.NormalizeSafe();
		vUp = vFwd.Cross( vWng );
		vUp.NormalizeSafe();
		
		vPos -= ( vWng * radius );
		vPos -= ( vUp * radius  );

		vWng *= radius * 2.0f;
		vUp  *= radius * 2.0f;
		vFwd *= fwdLen;
	}
	CFlightNavRegion* pNav = GetFlightNavRegion();
	if ( pNav )
		return pNav->IsSpaceVoid(vPos,vFwd,vWng,vUp);

	return Vec3(ZERO);
}

//====================================================================
// IsPointInForbiddenRegion
// Note that although forbidden regions aren't allowed to cross they can
// be nested, so we assume that in/out alternates with nesting...
//====================================================================
bool CAISystem::IsPointInForbiddenRegion(const Vec3& pos, bool checkAutoGenRegions) const
{
	return IsPointInForbiddenRegion(pos, 0, checkAutoGenRegions);
}

//===================================================================
// IsPointInForbiddenRegion
//===================================================================
bool CAISystem::IsPointInForbiddenRegion(const Vec3& pos, const CAIShape** ppShape, bool checkAutoGenRegions) const
{
  FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	if (checkAutoGenRegions)
		return m_forbiddenAreas.IsPointInside(pos, ppShape);
	return m_designerForbiddenAreas.IsPointInside(pos, ppShape);
}

//====================================================================
// IsPointInForbiddenBoundary
//====================================================================
bool CAISystem::IsPointInForbiddenBoundary(const Vec3 & pos, const CAIShape** ppShape) const
{
  FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	const CAIShape* pHitShape = 0;
	unsigned hits = m_forbiddenBoundaries.IsPointInsideCount(pos, &pHitShape);

	if ((hits % 2) != 0)
	{
		if (ppShape)
			*ppShape = pHitShape;
		return true;
	}
	return false;
}


//====================================================================
// IsPointForbidden
//====================================================================
bool CAISystem::IsPointForbidden(const Vec3& pos, float tol, Vec3* pNormal, const CAIShape** ppShape)
{
	if (IsPointInForbiddenRegion(pos, ppShape, true))
		return true;
	return IsPointOnForbiddenEdge(pos, tol, pNormal, ppShape);
}


//====================================================================
// IsPathForbidden
//====================================================================
bool CAISystem::IsPathForbidden(const Vec3 & start, const Vec3 & end)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	// do areas and boundaries separately 
	
	// for boundaries get a list of each of the boundaries the start and end
	// are in. Path is forbidden if the lists are different.
	std::set<const void *> startBoundaries;
	std::set<const void *> endBoundaries;

	for (unsigned i = 0, ni = m_forbiddenBoundaries.GetShapes().size(); i < ni; ++i)
	{
		const CAIShape* pShape = m_forbiddenBoundaries.GetShapes()[i];
		if (pShape->IsPointInside(start))
			startBoundaries.insert(pShape);
	}

	for (unsigned i = 0, ni = m_forbiddenBoundaries.GetShapes().size(); i < ni; ++i)
	{
		const CAIShape* pShape = m_forbiddenBoundaries.GetShapes()[i];
		if (pShape->IsPointInside(end))
			endBoundaries.insert(pShape);
	}

	if (startBoundaries != endBoundaries)
		return true;

	// boundaries are not a problem. Areas are trikier because we can 
	// always go from a forbidden area into a not-forbidden area, but not 
	// the other way. Simplify things by assuming they are only nested a bit
	// - i.e. there are no more than two forbidden areas outside a point. 
	std::set<const void *> startAreas;
	std::set<const void *> endAreas;

	for (unsigned i = 0, ni = m_forbiddenAreas.GetShapes().size(); i < ni; ++i)
	{
		const CAIShape* pShape = m_forbiddenAreas.GetShapes()[i];
		if (pShape->IsPointInside(start))
			startAreas.insert(pShape);
	}

	for (unsigned i = 0, ni = m_forbiddenAreas.GetShapes().size(); i < ni; ++i)
	{
		const CAIShape* pShape = m_forbiddenAreas.GetShapes()[i];
		if (pShape->IsPointInside(end))
			endAreas.insert(pShape);
	}
	
	// start and end completely clear.
	if (startAreas.empty() && endAreas.empty())
		return false;

	// start and end in the same 
	if (startAreas == endAreas)
		return false;

	// going from nested-but-valid to not-nested-but-valid is forbidden
	if (startAreas.size() > endAreas.size()+1)
		return true;

	// check for transition from valid to forbidden
	bool startInForbidden = (startAreas.size() % 2) != 0;
	bool endInForbidden = (endAreas.size() % 2) != 0;
	if (!startInForbidden && endInForbidden)
		return true;

	// This isn't foolproof but it is OK in most sensible situations!
	return false;
}

//====================================================================
// GetPointOutsideForbidden
//====================================================================
Vec3 CAISystem::GetPointOutsideForbidden(Vec3& pos, float distance, const Vec3* startPos)
{
	Vec3 normal;
	Vec3 retPos(pos);
	const CAIShape* pRegion;
	if (startPos && IntersectsForbidden(pos, *startPos, retPos, NULL, &normal, IF_BOUNDARIES, true))
	{
		Vec3 delta = *startPos - retPos;
		float len = delta.NormalizeSafe();
		retPos += delta * min(distance, len);
	}
	else if (startPos && IntersectsForbidden(*startPos, pos, retPos, NULL, &normal, IF_AREAS))
	{
		Vec3 delta = *startPos - retPos;
		float len = delta.NormalizeSafe();
		retPos += delta * min(distance, len);
	}
	else if (IsPointInForbiddenRegion(pos, &pRegion, true))// ||IsPointInForbiddenBoundary(pos,pRegion))
	{ 
		// get the closest point in the area's edge
		Distance::Point_Polygon2DSq(pos, pRegion->GetPoints(), retPos, &normal);
		retPos -= normal*distance; // normal is going towards the pos -> inwards the polygon
	}

	return retPos;
}

//====================================================================
// IsPathWorth
//====================================================================
bool CAISystem::IsPathWorth(const CAIActor* pObject, const Vec3& startPos, Vec3 endPos,float maxLengthCoeff, float percent, Vec3* pReturnPos)
{
  AIAssert(pObject);
	AgentPathfindingProperties unitNavProperties = pObject->GetMovementAbility().pathfindingProperties;
	
	unitNavProperties.exposureFactor = 0;
  // formation stuff that uses this function isn't interested in small height differences - especially if the 
  // max distance is small (since then height differences can dominate).
  unitNavProperties.zScale = 0.0f;

	int building;
	IVisArea *pArea;
	
	ENavigationType navType =  CheckNavigationType(startPos,building,pArea,unitNavProperties.navCapMask );
  bool b2D = 0 != (navType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD));

	CStandardHeuristic heuristic;

	Vec3 vDistance = startPos - endPos;
	if(b2D)
		vDistance.z = 0; // 2D only
	float maxCost = vDistance.GetLength()*maxLengthCoeff;

	float radius = pObject->GetParameters().m_fPassRadius;
	//static float radius = 0.3f;

	IAISystem::tNavCapMask navmask = IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_VOLUME | IAISystem::NAV_WAYPOINT_3DSURFACE | IAISystem::NAV_FLIGHT ;
	static const AgentPathfindingProperties navProperties(
		navmask, 
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 
		5.0f, 10000.0f, -10000.0f, 0.0f, 20.0f, 0.0);
	//heuristic.SetRequesterProperties(CPathfinderRequesterProperties(radius, navProperties), 0);

	heuristic.SetRequesterProperties(CPathfinderRequesterProperties(radius, unitNavProperties), pObject);
	// check reachability of flank position		

	Vec3 pos = GetGraph()->GetBestPosition(heuristic, maxCost, startPos, endPos, pObject->m_lastNavNodeIndex, unitNavProperties.navCapMask );//IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN);
	Vec3 vDisplacement = pos - endPos;
	if(b2D)
		vDisplacement.z=0; // 2D only
	float fDistance = vDisplacement.GetLength();
	
	if (fDistance < maxCost*percent)
	{
		if(pReturnPos)
			*pReturnPos = pos;
		return true;
	}
	return false;
}

//===================================================================
// GetAutoBalanceInterface
//===================================================================
IAutoBalance * CAISystem::GetAutoBalanceInterface(void)
{
	return m_pAutoBalance;
}


//===================================================================
// ExitNodeImpossible
//===================================================================
bool CAISystem::ExitNodeImpossible(CGraphLinkManager& linkManager, const GraphNode * pNode, float fRadius) const
{
	for (unsigned gl = pNode->firstLinkIndex; gl; gl = linkManager.GetNextLink(gl))
	{
		if (linkManager.GetRadius(gl) >= fRadius)
			return false;
	}
	return true;
}

//===================================================================
// EnterNodeImpossible
//===================================================================
bool CAISystem::EnterNodeImpossible(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode * pNode, float fRadius) const
{
	for (unsigned link = pNode->firstLinkIndex; link; link = linkManager.GetNextLink(link))
	{
		unsigned nextNodeIndex = linkManager.GetNextNode(link);
		GraphNode* nextNode = nodeManager.GetNode(nextNodeIndex);
		unsigned incomingLink = nextNode->GetLinkTo(nodeManager, linkManager, pNode);
		if (incomingLink && linkManager.GetRadius(incomingLink) >= fRadius)
			return false;
	}
	return true;
}

//===================================================================
// SetSpeciesThreatMultiplier
//===================================================================
void CAISystem::SetSpeciesThreatMultiplier(int nSpeciesID, float fMultiplier)
{
	if (fMultiplier>1.f)
		fMultiplier = 1.f;

	if (fMultiplier< 0.f) // Modified from <= 0 -> = 0.01
		fMultiplier = 0.0f;

	// will use this multiplier any time a puppet perceives a target of these species
	MapMultipliers::iterator mi;
	
	mi = m_mapSpeciesThreatMultipliers.find(nSpeciesID);
	if (mi== m_mapSpeciesThreatMultipliers.end())
		m_mapSpeciesThreatMultipliers.insert(MapMultipliers::iterator::value_type(nSpeciesID,fMultiplier));
	else
		mi->second = fMultiplier;

}

//===================================================================
// IsAIInDevMode
//===================================================================
bool CAISystem::IsAIInDevMode()
{
  return ::IsAIInDevMode();
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DumpStateOf(IAIObject * pObject)
{
	AILogAlways("AIName: %s",pObject->GetName());
	CPuppet *pPuppet = CastToCPuppetSafe(pObject);
	if (pPuppet)
	{
		CGoalPipe *pPipe = pPuppet->GetCurrentGoalPipe();
		if (pPipe)
		{
			AILogAlways("Current pipes: %s",pPipe->m_sName.c_str());
			while (pPipe->IsInSubpipe())
			{
				pPipe = pPipe->GetSubpipe();
				AILogAlways("   subpipe: %s",pPipe->m_sName.c_str());
			}
		}
	}
}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::SupressSoundEvent(const Vec3 &pos, float &fAffectedRadius)
{
	AIObjects::iterator ai=m_Objects.find(AIOBJECT_SNDSUPRESSOR),aiend =m_Objects.end();
	for (;ai!=aiend;++ai)
	{
		if (ai->first != AIOBJECT_SNDSUPRESSOR)
			break;
		if (!(ai->second)->IsEnabled())
			continue;
		float fRadius = (ai->second)->GetRadius();
		float totalSuppressionRadius = fRadius*.3f;		//
		float dist = ((ai->second)->GetPos() - pos).GetLength();

		if( dist < totalSuppressionRadius )			// sound event is within supressor radius 
					fAffectedRadius= 0.0f;									// this sound can;t be heard - it's withing 
																					// complite silence radous of supressor

		else if( dist < fRadius )																			// sound event is within supressor radius 
					fAffectedRadius *= (dist-totalSuppressionRadius)/(fRadius - totalSuppressionRadius);		// supress sound -- reduse it's radius

	}
}	

//
//-----------------------------------------------------------------------------------------------------------
int CAISystem::RayObstructionSphereIntersection(const Vec3& start, const Vec3& end)
{
	for (unsigned i = 0, ni = m_ObstructSpheres.size(); i < ni; ++i)
	{
		if (m_ObstructSpheres[i].IsObstructing(start, end))
			return 1;
	}
	return 0;
}

//
//-----------------------------------------------------------------------------------------------------------
int CAISystem::RayOcclusionPlaneIntersection(const Vec3 &start,const Vec3 &end)
{
	if (m_mapOcclusionPlanes.empty())
		return 0;

	int iRet = 0;

	ShapeMap::iterator di = m_mapOcclusionPlanes.begin(),diend = m_mapOcclusionPlanes.end();
	for (;di!=diend;++di)
	{
		if (!di->second.shape.empty())
		{
			float fShapeHeight = ((di->second.shape).front()).z;
			if ( (start.z<fShapeHeight) && (end.z<fShapeHeight))
				continue;
			if ( (start.z>fShapeHeight) && (end.z>fShapeHeight))
				continue;

			// find out where ray hits horizontal plane fShapeHeigh (with a nasty hack)
			Vec3 vIntersection;
			float t = (start.z-fShapeHeight)/(start.z-end.z);
			vIntersection = start+t*(end-start);


			// is it inside the polygon?
			if (Overlap::Point_Polygon2D(vIntersection, di->second.shape, &di->second.aabb))
				return 1;
		}
	}

	return 0;
}

//
//-----------------------------------------------------------------------------------------------------------
int CAISystem::GetNumberOfObjects(unsigned short type)
{
	return m_Objects.count(type);

}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::ThroughVehicle(const Vec3 & start, const Vec3 & end)
{
		
	AIObjects::iterator ai,aiend = m_Objects.end();
	for (ai=m_Objects.find(AIOBJECT_VEHICLE);ai!=aiend;++ai)
	{
		if (ai->first !=AIOBJECT_VEHICLE) 
			break;
		CAIObject *pVehicle = ai->second;
		Vec3 veh_dir = pVehicle->GetPos() - start;
		Vec3 dir = end-start;
		float veh_dist = veh_dir.len2();
		float dist = dir.len2();
		dir.Normalize();
		veh_dir.Normalize();
		if ((dir.Dot(veh_dir) > 0.2) && (veh_dist < dist))
			return true;
	}
	return false;
}

IAIObjectIter* CAISystem::GetFirstAIObject(EGetFirstFilter filter, short n)
{
	if(filter == IAISystem::OBJFILTER_GROUP)
	{
    return SAIObjectMapIterOfType::Allocate(n, m_mapGroups.find(n), m_mapGroups.end());
	}
	else if(filter == IAISystem::OBJFILTER_SPECIES)
	{
    return SAIObjectMapIterOfType::Allocate(n, m_mapSpecies.find(n), m_mapSpecies.end());
	}
	else //	if(filter == IAISystem::OBJFILTER_TYPE)
	{
		if(n == 0)
      return SAIObjectMapIter::Allocate(m_Objects.begin(), m_Objects.end());
		else
      return SAIObjectMapIterOfType::Allocate(n, m_Objects.find(n), m_Objects.end());
	}
}

IAIObjectIter* CAISystem::GetFirstAIObjectInRange(EGetFirstFilter filter, short n, const Vec3& pos, float rad, bool check2D)
{
	if(filter == IAISystem::OBJFILTER_GROUP)
	{
    return SAIObjectMapIterOfTypeInRange::Allocate(n, m_mapGroups.find(n), m_mapGroups.end(), pos, rad, check2D);
	}
	else if(filter == IAISystem::OBJFILTER_SPECIES)
	{
    return SAIObjectMapIterOfTypeInRange::Allocate(n, m_mapSpecies.find(n), m_mapSpecies.end(), pos, rad, check2D);
	}
	else //	if(filter == IAISystem::OBJFILTER_TYPE)
	{
		if(n == 0)
      return SAIObjectMapIterInRange::Allocate(m_Objects.begin(), m_Objects.end(), pos, rad, check2D);
		else
      return SAIObjectMapIterOfTypeInRange::Allocate(n, m_Objects.find(n), m_Objects.end(), pos, rad, check2D);
	}
}

IAIObjectIter* CAISystem::GetFirstAIObjectInShape(EGetFirstFilter filter, short n, const char* shapeName, bool checkHeight)
{
	ShapeMap::iterator	it = m_mapGenericShapes.find(shapeName);

	// Return dummy iterator.
	if(it == m_mapGenericShapes.end())
    return SAIObjectMapIter::Allocate(m_Objects.end(), m_Objects.end());

	SShape&	shape = it->second;

	if(filter == IAISystem::OBJFILTER_GROUP)
	{
    return SAIObjectMapIterOfTypeInShape::Allocate(n, m_mapGroups.find(n), m_mapGroups.end(), shape, checkHeight);
	}
	else if(filter == IAISystem::OBJFILTER_SPECIES)
	{
    return SAIObjectMapIterOfTypeInShape::Allocate(n, m_mapSpecies.find(n), m_mapSpecies.end(), shape, checkHeight);
	}
	else //	if(filter == IAISystem::OBJFILTER_TYPE)
	{
		if(n == 0)
      return SAIObjectMapIterInShape::Allocate(m_Objects.begin(), m_Objects.end(), shape, checkHeight);
		else
      return SAIObjectMapIterOfTypeInShape::Allocate(n, m_Objects.find(n), m_Objects.end(), shape, checkHeight);
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::Event( int event, const char *name)
{
	switch(event)
	{
		case AISYSEVENT_DISABLEMODIFYER:
		{
			SpecialAreaMap::iterator di;
			di = m_mapSpecialAreas.find(name);
			if(di!=m_mapSpecialAreas.end())
			{
				SpecialArea& sa = di->second;
				if(sa.bAltered)	// already disabled
					break;
				InvalidatePathsThroughArea(sa.GetPolygon());
				RescheduleCurrentPathfindRequest();
				sa.bAltered = true;
				GetGraph()->BreakNodes( sa.nBuildingID );
			}
			break;
		}
		default:
			break;
	}
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::CreateFormationDescriptor(const char *name)
{
	FormationDescriptorMap::iterator itD = m_mapFormationDescriptors.find(name);
	if(itD!=m_mapFormationDescriptors.end())
		m_mapFormationDescriptors.erase(itD);
	CFormationDescriptor fdesc;
	fdesc.m_sName = name;
	m_mapFormationDescriptors.insert(FormationDescriptorMap::iterator::value_type(fdesc.m_sName, fdesc));
}

//-----------------------------------------------------------------------------------------------------------
bool CAISystem::IsFormationDescriptorExistent(const char *name)
{
	return m_mapFormationDescriptors.find(name)!=m_mapFormationDescriptors.end();
}
//-----------------------------------------------------------------------------------------------------------

void CAISystem::AddFormationPoint(const char *name, const FormationNode& nodeDescriptor)
{
	FormationDescriptorMap::iterator fdit = m_mapFormationDescriptors.find(name);
	if(fdit!=m_mapFormationDescriptors.end())
	{
		fdit->second.AddNode(nodeDescriptor);
	}
}

//-----------------------------------------------------------------------------------------------------------
ITrackPatternDescriptor* CAISystem::CreateTrackPatternDescriptor( const char* patternName )
{
	PatternDescriptorMap::iterator patIt = m_mapTrackPatternDescriptors.find( patternName );
/*	if( patIt != m_mapTrackPatternDescriptors.end() )
	{
		AIWarning( "Overwriting track pattern '%s'", patIt->second.m_name.c_str() );
		m_mapTrackPatternDescriptors.erase( patIt );
	}*/

	CTrackPatternDescriptor	pdesc( patternName );
	std::pair<PatternDescriptorMap::iterator, bool>	ret = m_mapTrackPatternDescriptors.insert( PatternDescriptorMap::value_type( patternName, pdesc ) );
	return (ITrackPatternDescriptor*)(&ret.first->second);
}

ITrackPatternDescriptor* CAISystem::FindTrackPatternDescriptor( const char* patternName )
{
	PatternDescriptorMap::iterator	patIt = m_mapTrackPatternDescriptors.find( patternName );
	if( patIt != m_mapTrackPatternDescriptors.end() )
		return (ITrackPatternDescriptor*)&(patIt->second);
	return 0;
}


//-----------------------------------------------------------------------------------------------------------

IAIObject* CAISystem::GetLeaderAIObject(int iGroupID)
{
	CLeader* pLeader = GetLeader(iGroupID);
	return pLeader ? pLeader->GetAssociation() : NULL;
}

IAIObject* CAISystem::GetLeaderAIObject(IAIObject* pObject)
{
	CAIActor* pActor = pObject->CastToCAIActor();
	CLeader* pLeader = GetLeader(pActor);
	return pLeader ? pLeader->GetAssociation() : NULL;
}

IAIObject* CAISystem::GetFormationPoint(IAIObject* pObject)
{
	CAIActor* pActor = pObject->CastToCAIActor();
	CLeader* pLeader = GetLeader(pActor);
	if(pLeader) 
	{
		return (pLeader->GetFormationPoint(static_cast<CAIObject*>(pObject)));
	}
	return NULL;
}


int CAISystem::GetFormationPointClass(const char* descriptorName, int position) 
{
	FormationDescriptorMap::iterator di;
	di = m_mapFormationDescriptors.find(descriptorName);
	if (di!=m_mapFormationDescriptors.end())
	{
		return di->second.GetNodeClass(position);
	}
	return -1;
}


//====================================================================
// DisableNavigationInBrokenRegion
//====================================================================
void CAISystem::DisableNavigationInBrokenRegion(std::list<Vec3> & outline)
{
	if (!m_pGraph)
	{
		AIWarning("Being asked to disable navigation in broken region, yet no graph");
		return;
	}
  AABB outlineAABB(AABB::RESET);
	for (std::list<Vec3>::const_iterator it = outline.begin() ; it != outline.end() ; ++it)
		outlineAABB.Add(*it);
	outlineAABB.min.z = -std::numeric_limits<float>::max();
	outlineAABB.max.z = std::numeric_limits<float>::max();
	m_pendingBrokenRegions.m_regions.push_back(CGraph::CRegion(outline, outlineAABB));

  AILogEvent("Disabling navigation in broken region (%5.2f, %5.2f, %5.2f) to (%5.2f, %5.2f, %5.2f)",
    outlineAABB.min.x, outlineAABB.min.y, outlineAABB.min.z, 
    outlineAABB.max.x, outlineAABB.max.y, outlineAABB.max.z);
}


static char outputBuffer[MAX_WARNING_LENGTH + 32]; // extra for the id/prefix

//====================================================================
// Warning
//====================================================================
void CAISystem::Warning(const char * id, const char * format, ...)
{
	size_t idLen = strlen(id);
	strncpy(outputBuffer, id, idLen);
	va_list args;
	va_start(args, format);
	vsprintf_s(outputBuffer + idLen, sizeof(outputBuffer) - idLen, format, args);
	va_end(args);
	AIWarning(outputBuffer);
}

//====================================================================
// Error
//====================================================================
void CAISystem::Error(const char * id, const char * format, ...)
{
	size_t idLen = strlen(id);
	strncpy(outputBuffer, id, idLen);
	va_list args;
	va_start(args, format);
	vsprintf_s(outputBuffer + idLen, sizeof(outputBuffer) - idLen, format, args);
	va_end(args);
	AIError(outputBuffer);
}

//====================================================================
// LogProgress
//====================================================================
void CAISystem::LogProgress(const char * id, const char * format, ...)
{
	size_t idLen = strlen(id);
	strncpy(outputBuffer, id, idLen);
	va_list args;
	va_start(args, format);
	vsprintf_s(outputBuffer + idLen, sizeof(outputBuffer) - idLen, format, args);
	va_end(args);
	AILogProgress(outputBuffer);
}

//====================================================================
// LogEvent
//====================================================================
void CAISystem::LogEvent(const char * id, const char * format, ...)
{
	size_t idLen = strlen(id);
	strncpy(outputBuffer, id, idLen);
	va_list args;
	va_start(args, format);
	vsprintf_s(outputBuffer + idLen, sizeof(outputBuffer) - idLen, format, args);
	va_end(args);
	AILogEvent(outputBuffer);
}

//====================================================================
// LogComment
//====================================================================
void CAISystem::LogComment(const char * id, const char * format, ...)
{
	size_t idLen = strlen(id);
	strncpy(outputBuffer, id, idLen);
	va_list args;
	va_start(args, format);
	vsprintf_s(outputBuffer + idLen, sizeof(outputBuffer) - idLen, format, args);
	va_end(args);
	AILogComment(outputBuffer);
}

std::vector<IAIObject*> nonEntityObjects;


//====================================================================
// PopulateObjectTracker
//====================================================================
void CAISystem::PopulateObjectTracker(CObjectTracker& objectTracker, CAIObject* pObject)
{
	unsigned short type = pObject->GetType();
	unsigned entityID = pObject->GetEntityID();
	char	typeBuffer[8];
	const char* typeName = 0;

	if(type > AIOBJECT_NONE)
	{
		sprintf(typeBuffer, "%d", type);
		typeName = typeBuffer;
	}
	else switch (type)
	{
		case AIOBJECT_PUPPET: typeName = "AIOBJECT_PUPPET"; break;
case AIOBJECT_LEADER: typeName = "AIOBJECT_LEADER"; break;
case AIOBJECT_ATTRIBUTE: typeName = "AIOBJECT_ATTRIBUTE"; break;
case AIOBJECT_VEHICLE: 
	if (pObject->GetSubType() == CAIObject::STP_CAR)
		typeName = "AIOBJECT_CAR";
	else if (pObject->GetSubType() == CAIObject::STP_BOAT)
		typeName = "AIOBJECT_BOAT";
	else if (pObject->GetSubType() == CAIObject::STP_HELI)
		typeName = "AIOBJECT_HELICOPTER";
	else
	{
		AIWarning("Unknown vehicle subtype %d for %s", pObject->GetSubType(), pObject->GetName());
		typeName = "AIOBJECT_VEHICLE";
	}
	break;
case AIOBJECT_BOAT: typeName = "AIOBJECT_BOAT"; break;
case AIOBJECT_CAR: typeName = "AIOBJECT_CAR"; break;
case AIOBJECT_HELICOPTER: typeName = "AIOBJECT_HELICOPTER"; break;
case AIOBJECT_PLAYER: typeName = "AIOBJECT_PLAYER"; break;
case AIOBJECT_DUMMY:
	if( pObject->GetSubType() == CAIObject::STP_TRACKDUMMY )
		typeName = "AIOBJECT_DUMMY_STP_TRACKDUMMY";
	else
		typeName = "AIOBJECT_DUMMY";
	break;
case AIOBJECT_WAYPOINT: typeName = "AIOBJECT_WAYPOINT"; break;
case AIOBJECT_MOUNTEDWEAPON: typeName = "AIOBJECT_MOUNTEDWEAPON"; break;
case AIOBJECT_HIDEPOINT: typeName = "AIOBJECT_HIDEPOINT"; break;
default: 
	typeName = "AIOBJECT_UNDEFINED"; 
	AIWarning("CAISystem::PopulateObjectTracker - undefined object type (using AIOBJECT) : %s", pObject->GetName());
	break;
	}

	bool bNeedsToBeCreated(entityID==0 && type!=AIOBJECT_LEADER);

	objectTracker.AddObject(pObject, typeName, bNeedsToBeCreated , entityID);

	if (bNeedsToBeCreated)
		nonEntityObjects.push_back(pObject);
}
//====================================================================
// PopulateObjectTracker
//====================================================================
void CAISystem::PopulateObjectTracker(CObjectTracker& objectTracker)
{
	objectTracker.AddObject(m_pGraph, false);
	objectTracker.AddObject(m_pHideGraph, false);

	objectTracker.AddObject(m_pAStarSolver, false);

	objectTracker.AddObject(&sStandardHeuristic, false);

	for (AIObjects::iterator it(m_Objects.begin()) ; it != m_Objects.end() ; ++it)
	{
		CAIObject* pObject = it->second;
		PopulateObjectTracker(objectTracker, pObject);
	}
	for (SetAIObjects::iterator it(m_setDummies.begin()) ; it != m_setDummies.end() ; ++it)
	{
		CAIObject* pObject = *it;
		PopulateObjectTracker(objectTracker, pObject);
	}

	objectTracker.AddObject(&m_navPath, false);

	// take care of formations
	for (FormationMap::iterator it(m_mapActiveFormations.begin()); it!=m_mapActiveFormations.end(); ++it)
		objectTracker.AddObject(it->second, false);
	// take care of unitActions
	for (AIObjects::iterator it(m_Objects.find(AIOBJECT_LEADER)); it!=m_Objects.end(); ++it)
	{
		if (it->first != AIOBJECT_LEADER)
			break;
		CLeader* pLeader = it->second->CastToCLeader();
		if (pLeader)
			pLeader->PopulateObjectTracker(objectTracker);
	}
}

//====================================================================
// CAIObjectCreator 
//====================================================================
struct CAIObjectCreator : public CObjectTracker::CObjectCreator
{
	CAIObjectCreator() {}
	virtual void* CreateObject(CObjectTracker::TID ID, CObjectTracker::TID externalID, const char* objectType) const;
};

//====================================================================
// CreateObject
// Objects just get created here - serialisation happens later
//====================================================================
void* CAIObjectCreator::CreateObject(CObjectTracker::TID ID, CObjectTracker::TID externalID, const char* objectType) const
{
	AILogEvent("Creating object of type %s for ID %d", objectType, ID);
	if (externalID != 0)
	{
		AIWarning("Being asked to dread an object of type %s even though it has an external ID %d",
			objectType, externalID);
		return 0;
	}

	if (strcmp(objectType, "AIOBJECT_PLAYER") == 0)
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_PLAYER, 0));
	else if (strcmp(objectType, "AIOBJECT_PUPPET") == 0)
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_PUPPET, 0));
	else if (strcmp(objectType, "AIOBJECT_2D_FLY") == 0)
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_2D_FLY, 0));
	else if (strcmp(objectType, "AIOBJECT_LEADER") == 0)
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_LEADER, 0));
	else if (strcmp(objectType, "AIOBJECT_ATTRIBUTE") == 0)
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_ATTRIBUTE, 0));
	else if (strcmp(objectType, "AIOBJECT_VEHICLE") == 0)
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_VEHICLE, 0));
	else if (strcmp(objectType, "AIOBJECT_BOAT") == 0)
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_BOAT, 0));
	else if (strcmp(objectType, "AIOBJECT_CAR") == 0)
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_CAR, 0));
	else if (strcmp(objectType, "AIOBJECT_HELICOPTER") == 0)
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_HELICOPTER, 0));
	else if (strcmp(objectType, "AIOBJECT_PLAYER") == 0)
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_PLAYER, 0));
	else if (strcmp(objectType, "AIOBJECT_MOUNTEDWEAPON") == 0)
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_MOUNTEDWEAPON, 0));
	else if (strcmp(objectType, "AIOBJECT_DUMMY") == 0)
	{
		nonEntityObjects.push_back(GetAISystem()->CreateDummyObject(""));
		nonEntityObjects.back()->SetType(AIOBJECT_DUMMY);
	}
	else if (strcmp(objectType, "AIOBJECT_DUMMY_STP_TRACKDUMMY") == 0)
	{
		nonEntityObjects.push_back(GetAISystem()->CreateDummyObject("", CAIObject::STP_TRACKDUMMY));
		nonEntityObjects.back()->SetType(AIOBJECT_DUMMY);
	}
	else if (strcmp(objectType, "AIOBJECT_WAYPOINT") == 0)
	{
		nonEntityObjects.push_back(GetAISystem()->CreateDummyObject(""));
		nonEntityObjects.back()->SetType(AIOBJECT_WAYPOINT);
	}
	else if (strcmp(objectType, "AIOBJECT_HIDEPOINT") == 0)
	{
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(AIOBJECT_HIDEPOINT, 0));
	}
	else if(objectType[0]>='0' && objectType[0]<='9')
	{
		int anchorType = 0;
		sscanf(objectType,"%d",&anchorType);
		nonEntityObjects.push_back(GetAISystem()->CreateAIObject(anchorType, 0));
	}
	else
	{
		AIWarning("Unhandled request to create an object of type %s", objectType);
		return 0;
	}
	if (!nonEntityObjects.empty() && nonEntityObjects.back() == 0)
	{
		AIWarning("Unable to create object of type %s", objectType);
		nonEntityObjects.pop_back();
	}
	return nonEntityObjects.empty() ? 0 : nonEntityObjects.back();
}


//====================================================================
// InstantiateFromObjectTracker
//====================================================================
void CAISystem::InstantiateFromObjectTracker(CObjectTracker& objectTracker)
{
	AIAssert(m_pGraph);
	objectTracker.CreateObjects(CAIObjectCreator());
}

//====================================================================
// DebugOutputObjects
//====================================================================
void CAISystem::DebugOutputObjects(const char *txt) const
{
	AILogComment("================= DebugOutputObjects: %s =================", txt);

  AILogComment("m_Objects");
	bool bNoEntityWarning = (AIGetLogConsoleVerbosity()>=3 || AIGetLogFileVerbosity()>=3);

	for (AIObjects::const_iterator it = m_Objects.begin() ; it != m_Objects.end() ; ++it)
	{
		short ID = it->first;
		const CAIObject *pObject = it->second;
		const char *name = pObject->GetName();
		unsigned entID = pObject->GetEntityID();
		unsigned short type = pObject->GetType();

		AILogComment("ID %d, entID = %d, type = %d, ptr = %p, name = %s", ID, entID, type, pObject, name);

		if(bNoEntityWarning && entID>0 && !gEnv->pEntitySystem->GetEntity(pObject->GetEntityID()) 
			&& pObject->GetAIType() !=AIOBJECT_LEADER)
			AIWarning("No entity for AIObject %s",pObject->GetName());
  }

	AILogComment("m_setDummies");
	for (SetAIObjects::const_iterator it = m_setDummies.begin() ; it != m_setDummies.end() ; ++it)
	{
		const CAIObject *pObject = *it;
		const char *name = pObject->GetName();
		unsigned entID = pObject->GetEntityID();
		unsigned short type = pObject->GetType();

		AILogComment("entID = %d, type = %d, ptr = %p, name = %s", entID, type, pObject, name);
	}

	AILogComment("================= DebugOutputObjects End =================");
}

//====================================================================
// Serialize
//====================================================================
void CAISystem::Serialize( TSerialize ser )
{
  DebugOutputObjects("Start of serialize");

/*
for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
	it->second->Dump();
for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
	it->second->Validate();
*/
	// DEBUG

  if ( m_pActionManager && ser.IsReading() )
	  m_pActionManager->Reset();

  nonEntityObjects.clear();
	// simplify serialising pathfinder
	RescheduleCurrentPathfindRequest();

	CObjectTracker objectTracker;
	if (ser.IsWriting())
	{
		PopulateObjectTracker(objectTracker);
		m_pGraph->PopulateObjectTracker(objectTracker);
		m_pHideGraph->PopulateObjectTracker(objectTracker);
    DebugOutputObjects("Writing - after populating obj tracker");
	}
	else
	{
    if (!m_mapBeacons.empty())
    {
      const BeaconMap::iterator iend = m_mapBeacons.end();
      for (BeaconMap::iterator bi=m_mapBeacons.begin() ; bi!=iend; ++ bi)
      {
        RemoveDummyObject((bi->second).pBeacon);
      }
      m_mapBeacons.clear();
    }
	}

	// get all objects sorted and pointers-to-existing objects set up
	if (ser.IsReading())
	{
		objectTracker.Serialize(ser);
		InstantiateFromObjectTracker(objectTracker);
    DebugOutputObjects("After instantiating");
	}

	// start serialising
  unsigned numObjects = 0;

	ser.BeginGroup( "AISystemStatus" );
	{
		// our stuff - do we need to split this up into two stages - 1st being to walk through all objects and
		// set up pointers in the object tracker - the other to actually serialise? However, that might not
		// be possible if, for example, we have an array of pointers - then you need to serialise properly
		// to get the array size...
		ser.BeginGroup("AI_Pointers");
		{
			objectTracker.SerializeObjectPointer(ser, "m_pGraph", m_pGraph, true);
			objectTracker.SerializeObjectPointer(ser, "m_pHideGraph", m_pHideGraph, true);
			objectTracker.SerializeObjectPointer(ser, "m_pAStarSolver", m_pAStarSolver, true);
			objectTracker.SerializeObjectReference(ser, "sStandardHeuristic", sStandardHeuristic);
			objectTracker.SerializeObjectReference(ser, "m_navPath", m_navPath);

			numObjects = m_setDummies.size() + m_Objects.size();
			ser.Value("numObjects", numObjects);
			// for debug purposes
			int count(m_setDummies.size());
			ser.Value("m_setDummies_size", count);
			count = m_Objects.size();
			ser.Value("m_Objects_size", count);

//			assert(count == m_Objects.size());	//this can happen on quickloading because already existing objects are recreated ...

			//remove new objects on reading [Jan]
			/*if(ser.IsWriting())	
			{
			for(AIObjects::iterator it = m_Objects.begin(); it != m_Objects.end(); ++it)
			{
			unsigned int id = (unsigned int)(it->second->GetEntityID());
			ser.Value("AIObjectID", id);
			}
			}
			else if(ser.IsReading())
			{
			std::vector<unsigned int> ids;
			ids.resize(count);
			for(int i = 0; i < count; ++i)
			ser.Value("AIObjectID", ids[i]);

			for(AIObjects::iterator it = m_Objects.begin(); it != m_Objects.end(); ++it)
			{
			bool found = false;
			int index = 0;
			while(!found && index < count)
			{
			if(ids[index] == it->second->GetEntityID())
			found = true;
			++index;
			}
			if(!found)
			{
			m_Objects.erase(it);
			--it;
			}
			}
			}*/

      // at this point there may be more objects+dummies than when saved because some entity objects (e.g. CPipeUser
      // always contain an internal non-entity object - when they sync up the numbers should match.

			// All the non-entity objects in order
			objectTracker.SerializeObjectPointerContainer(ser, "nonEntityObjects", nonEntityObjects, true, false);

			// now the entity objects - since we can't determine the order they use the entity ID
//			char AIObjectSaveName[256];
			unsigned int numEntity = 0;
			//for (AIObjects::iterator it = m_Objects.begin() ; it != m_Objects.end() ; ++it)
			AIObjects::iterator it = m_Objects.begin();
			
			AIObjects::iterator itEnd = m_Objects.end();

			if (!ser.IsReading())
			{
				ser.BeginGroup("AI_Object_Pointers");
				for (int i = 0 ;  i <  m_Objects.size(); ++i)
				{
					CAIObject* pObject = it->second;
					if (pObject->GetEntityID() )//&& pObject->GetAIType() != AIOBJECT_LEADER)
					{
/*
						++numEntity;
						//sprintf(AIObjectSaveName, "AIObject-%d-%d", pObject->GetEntityID(),pObject->GetAIType());
						ser.BeginGroup("ptr");
						objectTracker.SerializeObjectPointer(ser, "v", pObject, true);
						ser.EndGroup();
*/
					}
					else if(pObject->GetAIType() != AIOBJECT_LEADER)
					{
						AIWarning("Entity AIObject %s has no entity ID",pObject->GetName());
					}
					++it;
				}
				ser.Value("count",numEntity);
				ser.EndGroup();
			}
			else
			{
				ser.BeginGroup("AI_Object_Pointers");
				ser.Value("count",numEntity);
				numEntity = min(numEntity,(unsigned int)m_Objects.size());
				for (; it!=itEnd; ++it)
//				for (int i = 0 ;  i < numEntity && it!=itEnd; )
				{

					CAIObject* pObject = it->second;
					if (pObject->GetEntityID() )//&& pObject->GetAIType() != AIOBJECT_LEADER)
					{
// no need to cross-reference the pointers list - entity IDs are already in the map of records in the objTracker
// if do it old way - problem with removed entities
//						ser.BeginGroup("ptr");
//						objectTracker.SerializeObjectPointer(ser, "v", pObject, true);
//						ser.EndGroup();
//						++i;
						objectTracker.RestoreEntityObject(pObject, pObject->GetEntityID() );
					}
				}
				ser.EndGroup();
			}
			
			/*
			//serialize AILeaders after other AIObjects since they contain reference to other AI Objects
			ser.BeginGroup("AILeaders");
			{
				AIObjects::iterator it = m_Objects.find(AIOBJECT_LEADER), itEnd = m_Objects.end();
				for (;  it != itEnd; ++it)
				{
					if(it->first != AIOBJECT_LEADER)
						break;
					CAIObject* pObject = it->second;
					++numEntity;
					sprintf(AIObjectSaveName, "AIObject-%d-%d", pObject->GetEntityID(),pObject->GetAIType());
					objectTracker.SerializeObjectPointer(ser, AIObjectSaveName, pObject, true);
				}
			}
			ser.EndGroup();
			*/
			// TODO work out why this gets hit and then revert to AIAssert
//			AIAssert(numEntity + nonEntityObjects.size() == numObjects);

		}
		ser.EndGroup();

		//====================================================================
		// Object serialisation - pointers should all be set up by now
		//====================================================================
    Serialize(ser, objectTracker);

    // see comment above - on reading now everything should be synced and no left-over objects
    AIAssert(numObjects == m_setDummies.size() + m_Objects.size()); //this is also popping up in Profile :-

		if (ser.IsReading())
		{
			objectTracker.ValidateOwnership();
			DebugOutputObjects("After serialising");

			std::vector<CAIObject*> objectsToRemove;
			unsigned numUntouched = 0;
			for (SetAIObjects::iterator it = m_setDummies.begin(); it != m_setDummies.end(); ++it)
			{
				CAIObject* pAI = *it;
				if (!objectTracker.WasTouchedDuringSerialization(pAI))
				{

					AIWarning("%s was not touched", pAI->GetName());
					objectsToRemove.push_back(pAI);
					numUntouched++;
				}
			}
			if (!objectsToRemove.empty())
			{
				AIWarning("NumUntouched %d, removing!", objectsToRemove.size());
				for (unsigned i = 0, ni = objectsToRemove.size(); i < ni; ++i)
					RemoveDummyObject(objectsToRemove[i]);
			}

//			m_lastAmbientFireUpdateTime.SetSeconds(-10.0f);
//			m_lastExpensiveAccessoryUpdateTime.SetSeconds(-10.0f);
//			m_lastVisBroadPhaseTime.SetSeconds(-10.0f);
		}
		ser.Value("m_lastAmbientFireUpdateTime", m_lastAmbientFireUpdateTime);
		ser.Value("m_lastExpensiveAccessoryUpdateTime", m_lastExpensiveAccessoryUpdateTime);
		ser.Value("m_lastVisBroadPhaseTime", m_lastVisBroadPhaseTime);
		ser.Value("m_lastGroupUpdateTime", m_lastGroupUpdateTime);
		ser.Value("m_pathFindIdGen", m_pathFindIdGen);
	}
	ser.EndGroup();

	// write the object tracker database at the end in case objects got added during serialisation
	if (ser.IsWriting())
		objectTracker.Serialize(ser);

	if ( m_pActionManager )
		m_pActionManager->Serialize( ser );

	//serialize AI lua globals
	IScriptSystem* pSS = gEnv->pScriptSystem;
	if(pSS)
	{
		SmartScriptTable pScriptAI;
		if(pSS->GetGlobalValue("AI",pScriptAI)) 
			if(pScriptAI->HaveValue("OnSave") && pScriptAI->HaveValue("OnLoad"))
			{
				ser.BeginGroup("ScriptBindAI");
				SmartScriptTable persistTable(pSS);
				if (ser.IsWriting())
					Script::CallMethod(pScriptAI, "OnSave", persistTable);
				ser.Value( "ScriptData", persistTable.GetPtr() );
				if (ser.IsReading())
					Script::CallMethod(pScriptAI, "OnLoad", persistTable);
				ser.EndGroup();
			}
	}
//	for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
//		it->second->Validate();
}

//===================================================================
// Serialize
//===================================================================
void PathFindRequest::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
	ser.BeginGroup("PathFindRequest");
		GetAISystem()->GetGraph()->SerializeNodePointer(ser, "pStart", startIndex);
		GetAISystem()->GetGraph()->SerializeNodePointer(ser, "pEnd", endIndex);
		ser.Value("startPos", startPos);
		ser.Value("startDir", startDir);
		ser.Value("endPos", endPos);
		ser.Value("endDir", endDir);
		ser.Value("bSuccess", bSuccess);
		objectTracker.SerializeObjectPointer(ser, "pRequester", pRequester, false);
		ser.Value("bSuccess", bSuccess);
		ser.Value("nForceTargetBuildingId", nForceTargetBuildingId);
		ser.Value("allowDangerousDestination", allowDangerousDestination);
		ser.Value("endTol", endTol);
		ser.Value("endDistance", endDistance);
		ser.Value("isDirectional", isDirectional);
		ser.Value("bPathEndIsAsRequested", bPathEndIsAsRequested);
		ser.Value("id", id);
		ser.Value("navCapMask", navCapMask);
		ser.Value("passRadius", passRadius);
		ser.EnumValue("type", type, TYPE_ACTOR, TYPE_RAW);
	ser.EndGroup();
}


//====================================================================
// Serialize
//====================================================================
void CAISystem::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	if(!ser.IsWriting())
	{
		m_enabledPuppetsSet.clear();
		m_disabledPuppetsSet.clear();
	}

  ser.BeginGroup("CAISystem");
  {
    ser.BeginGroup("PathRequestQueue");
    {
      unsigned pathQueueSize = m_lstPathQueue.size();
      ser.Value("pathQueueSize", pathQueueSize);

      if (ser.IsReading())
      {
        AIAssert(m_lstPathQueue.empty());
        for (unsigned i = 0 ; i < pathQueueSize ; ++i)
        {
          PathFindRequest* request = new PathFindRequest(PathFindRequest::TYPE_ACTOR);
          request->Serialize(ser, objectTracker);
        }
      }
      else
      {
        for (PathQueue::const_iterator it = m_lstPathQueue.begin() ; it != m_lstPathQueue.end() ; ++it)
        {
          PathFindRequest* request = *it;
          AIAssert(request);
          request->Serialize(ser, objectTracker);
        }
      }
    }
    ser.EndGroup();

    if(ser.IsWriting())
    {
      if(ser.BeginOptionalGroup("m_pCurrentRequest", m_pCurrentRequest!=NULL))
      {
        m_pCurrentRequest->Serialize(ser, objectTracker);
        ser.EndGroup();
      }
    }
    else
    {
      SAFE_DELETE(m_pCurrentRequest);
      if(ser.BeginOptionalGroup("m_pCurrentRequest", true))
      {
        m_pCurrentRequest = new PathFindRequest(PathFindRequest::TYPE_ACTOR);
        m_pCurrentRequest->Serialize(ser, objectTracker);
        ser.EndGroup();
      }
    }

    ser.EnumValue("m_nPathfinderResult", m_nPathfinderResult, PATHFINDER_STILLFINDING, PATHFINDER_MAXVALUE);

    ser.BeginGroup("Beacons");
    {
      int count = m_mapBeacons.size();
      ser.Value("count", count);
      char name[32];
      if (ser.IsWriting())
      {
        int i = 0;
        for (BeaconMap::iterator it = m_mapBeacons.begin() ; it != m_mapBeacons.end() ; ++it, ++i)
        {
          sprintf(name, "beacon-%d", i);
          ser.BeginGroup(name);
          {
            unsigned short id = it->first;
            BeaconStruct &bs = it->second;

            ser.Value("id", id);
            objectTracker.SerializeObjectPointer(ser, "pBeacon", bs.pBeacon, false);
            objectTracker.SerializeObjectPointer(ser, "pOwner", bs.pOwner, false);
          }
          ser.EndGroup();
        }
      }
      else
      {
        m_mapBeacons.clear();
        for (int i = 0 ; i < count ; ++i)
        {
          sprintf(name, "beacon-%d", i);
          ser.BeginGroup(name);
          {
            unsigned short id;
            BeaconStruct bs;
            ser.Value("id", id);
            objectTracker.SerializeObjectPointer(ser, "pBeacon", bs.pBeacon, false);
            objectTracker.SerializeObjectPointer(ser, "pOwner", bs.pOwner, false);
            m_mapBeacons[id] = bs;
          }
          ser.EndGroup();
        }
      }
    }
    ser.EndGroup();

		m_lightManager.Serialize(ser, objectTracker);

    // Active formations
    ser.BeginGroup("ActiveFormations");
    {
      int count = m_mapActiveFormations.size();
      ser.Value("numObjects", count);

      if(ser.IsReading())
      {
        FormationMap::iterator iFormation=m_mapActiveFormations.begin(), iEnd = m_mapActiveFormations.end();
        for( ;iFormation != iEnd; ++iFormation)
          delete iFormation->second;
        m_mapActiveFormations.clear();
      }

      char formationName[32];
      while(--count>=0)
      {
        CFormation* pFormationObject(0);
        CAIObject* pOwner(NULL);

        if(ser.IsWriting())
        {
          FormationMap::iterator iFormation=m_mapActiveFormations.begin();
          std::advance(iFormation, count);
          pOwner = iFormation->first;
          pFormationObject = iFormation->second;
        }
        else
          pFormationObject = new CFormation();
        sprintf(formationName, "Formation_%d", count);
        ser.BeginGroup(formationName);
        {
          {
            objectTracker.SerializeObjectPointer(ser, "formationOwner", pOwner, false);
            pFormationObject->Serialize(ser, objectTracker);
          }
          objectTracker.SerializeObjectPointer(ser, "formation", pFormationObject, true);
        }
        ser.EndGroup();
        if(ser.IsReading())
          m_mapActiveFormations.insert(FormationMap::iterator::value_type(pOwner,pFormationObject));
      }
    }
    ser.EndGroup();


    m_PipeManager.Serialize(ser);

    // Serialize temporary shapes.
    ser.BeginGroup("TempGenericShapes");
    if(ser.IsWriting())
    {
      int	nTempShapes = 0;
      for(ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); ++it)
        if(it->second.temporary)
          nTempShapes++;
      ser.Value("nTempShapes", nTempShapes);
      for(ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); ++it)
      {
        SShape&	shape = it->second;
        if(shape.temporary)
        {
          ser.BeginGroup("Shape");
          string	name(it->first);
          ser.Value("name", name);
          ser.Value("aabbMin", shape.aabb.min);
          ser.Value("aabbMax", shape.aabb.max);
          ser.Value("type", shape.type);
          ser.Value("shape", shape.shape);
          ser.EndGroup();
        }
      }
    }
    else
    {
      // Remove temporary shapes.
      for(ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end();)
      {
        // Enable
        it->second.enabled = true;
        // Remove temp
        if(it->second.temporary)
          m_mapGenericShapes.erase(it++);
        else
          ++it;
      }
      // Create new temp shapes.
      int	nTempShapes = 0;
      ser.Value("nTempShapes", nTempShapes);
      for(int i = 0; i < nTempShapes; ++i)
      {
        string	name;
        SShape	shape;
        ser.BeginGroup("Shape");
        ser.Value("name", name);
        ser.Value("aabbMin", shape.aabb.min);
        ser.Value("aabbMax", shape.aabb.max);
        ser.Value("type", shape.type);
        ser.Value("shape", shape.shape);
        ser.EndGroup();
        shape.temporary = true;
        m_mapGenericShapes.insert(ShapeMap::iterator::value_type(name, shape));
      }
    }
    ser.EndGroup();

    ser.BeginGroup("DisabledGenericShapes");
    if(ser.IsWriting())
    {
      int	nDisabledShapes = 0;
      for(ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); ++it)
        if(!it->second.enabled)
          nDisabledShapes++;
      ser.Value("nDisabledShapes", nDisabledShapes);

      for(ShapeMap::iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); ++it)
      {
        if(it->second.enabled) continue;
				ser.BeginGroup("Shape");
        ser.Value("name", it->first);
				ser.EndGroup();
      }
    }
    else
    {
      int	nDisabledShapes = 0;
      ser.Value("nDisabledShapes", nDisabledShapes);
      string	name;
      for(int i = 0; i < nDisabledShapes; ++i)
      {
				ser.BeginGroup("Shape");
        ser.Value("name", name);
				ser.EndGroup();
        ShapeMap::iterator di = m_mapGenericShapes.find(name);
        if(di != m_mapGenericShapes.end())
          di->second.enabled = false;
        else
          AIWarning("CAISystem::Serialize Unable to find generic shape called %s", name.c_str());
      }
    }
    ser.EndGroup();

    ser.BeginGroup("AI_Objects");
    {
      // First all non-entity objects - these will have been loaded/saved in the same order
      char AIObjectSaveName[256];
      for (unsigned i = 0 ; i < nonEntityObjects.size() ; ++i)
      {
        sprintf(AIObjectSaveName, "Non-entity_AIObject-%d", i);
        ser.BeginGroup(AIObjectSaveName);
        {
          IAIObject* pObject = nonEntityObjects[i];
          AIAssert(pObject->GetEntityID() == 0);
          pObject->Serialize(ser, objectTracker);
        }
        ser.EndGroup();
      }

      // Now all entity objects - these will find themselves based on their entity ID
			int i=0;//DEBUG
      for (AIObjects::iterator it = m_Objects.begin() ; it != m_Objects.end() ; ++it)
      {
        CAIObject* pObject = it->second;
        if (pObject->GetEntityID() != 0 && it->first!=AIOBJECT_LEADER)
        {
          sprintf(AIObjectSaveName, "Entity_AIObject-%d", pObject->GetEntityID());
					AILogEvent("Serializing AIObject(%d) %s",i++,pObject->GetName());//DEBUG
          if( ser.BeginOptionalGroup(AIObjectSaveName, true))
          {
            pObject->Serialize(ser, objectTracker);
            ser.EndGroup();
					}
        }
      }
      //serialize AILeaders after other Entity AIObjects since they contain reference to other AI Objects
      ser.BeginGroup("AILeaders");
      {
				int numLeaders(0);
				if(ser.IsReading())
				{
					// remove all the leaders first;
					ClearAllLeaders( );
					ser.Value("numLeaders", numLeaders);
					for(int i=0; i<numLeaders; i++)
					{
						sprintf(AIObjectSaveName, "AILeaderN-%d", i);
						ser.BeginGroup(AIObjectSaveName);
						{
							int groupId;
							ser.Value("groupId", groupId);
							CLeader* pLeader = CreateLeader(groupId);
							if(!pLeader)
							{
								AIWarning("CAISystem::Serialization : can't create AILeader %s for group %d", AIObjectSaveName, groupId);
								ser.EndGroup();
								continue;
							}
							pLeader->Serialize(ser, objectTracker);
						}
						ser.EndGroup();
					}
				}
				else
				{
					for (AIObjects::iterator it = m_Objects.find(AIOBJECT_LEADER), itEnd = m_Objects.end();  
						it != itEnd && it->first == AIOBJECT_LEADER; ++it)
						++numLeaders;
					ser.Value("numLeaders", numLeaders);
					int counter(0);
					for (AIObjects::iterator it = m_Objects.find(AIOBJECT_LEADER), itEnd = m_Objects.end();  
						it != itEnd && it->first == AIOBJECT_LEADER; ++it)
					{
						sprintf(AIObjectSaveName, "AILeaderN-%d", counter++);
						ser.BeginGroup(AIObjectSaveName);
						{
							CLeader* pLeader = it->second->CastToCLeader();
							int groupId(pLeader->GetGroupId());
							ser.Value("groupId", groupId);
							pLeader->Serialize(ser, objectTracker);
						}
						ser.EndGroup();
					}
				}
/*
        AIObjects::iterator it = m_Objects.find(AIOBJECT_LEADER), itEnd = m_Objects.end();
        for (;  it != itEnd; ++it)
        {
          if(it->first != AIOBJECT_LEADER)
            break;
					CLeader* pLeader = it->second->CastToCLeader();
          sprintf(AIObjectSaveName, "AILeader-%d", pLeader->GetGroupId());
          ser.BeginGroup(AIObjectSaveName);
          {
            pLeader->Serialize(ser, objectTracker);
          }
          ser.EndGroup();
        }
*/
      }
      ser.EndGroup();
    }
    ser.EndGroup();

		if (m_pGraph)
			m_pGraph->Serialize(ser, objectTracker, "AINavGraph");
		if (m_pHideGraph)
			m_pHideGraph->Serialize(ser, objectTracker, "AIHideGraph");

    m_pTriangularNavRegion->Serialize(ser, objectTracker);
    m_pWaypointHumanNavRegion->Serialize(ser, objectTracker);
    m_pFlightNavRegion->Serialize(ser, objectTracker);
    m_pVolumeNavRegion->Serialize(ser, objectTracker);

    m_pAStarSolver->Serialize(ser, objectTracker);

    m_navPath.Serialize(ser, objectTracker);

    ser.Value("m_deadBodies", m_deadBodies);
    ser.Value("m_explosionSpot", m_explosionSpots);
    ser.Value("m_priorityObjectTypes", m_priorityObjectTypes);

    ser.Value("m_fFrameStartTime", m_fFrameStartTime);
    ser.Value("m_fFrameDeltaTime", m_fFrameDeltaTime);
    ser.Value("m_fLastPuppetUpdateTime", m_fLastPuppetUpdateTime);
    if (ser.IsReading())
    {
      // Danny: physics doesn't serialise its time (it doesn't really use it) so we can
      // set it here.
      GetISystem()->GetIPhysicalWorld()->SetPhysicsTime(m_fFrameStartTime.GetSeconds());
    }

    float fst = m_fFrameStartTime.GetSeconds();

    AIObjects::iterator itobjend = m_Objects.end();

    ser.BeginGroup("AI_Groups");
    {
      //regenerate the whole groups map when reading
      if(ser.IsReading())
      {
        for(AIGroupMap::iterator it = m_mapAIGroups.begin(); !m_mapAIGroups.empty(); )
        {
          delete it->second;
          m_mapAIGroups.erase(it++);
        }

        //m_mapAIGroups.clear();
        for (AIObjects::iterator itobj = m_Objects.begin() ; itobj != itobjend ; ++itobj)
        {
          CAIObject* pObject = itobj->second;
          if(pObject->CastToCPuppet() || pObject->CastToCAIPlayer() || pObject->CastToCLeader())
          {
						CAIActor* pActor = itobj->second->CastToCAIActor();
            int groupid = pActor->GetGroupId();
            if(m_mapAIGroups.find(groupid)==m_mapAIGroups.end())
            {
              CAIGroup* pGroup = new CAIGroup(groupid);
              m_mapAIGroups.insert(std::make_pair(groupid,pGroup));

            }
          }
        }
      }
      for(AIGroupMap::iterator it = m_mapAIGroups.begin(); it!=m_mapAIGroups.end(); ++it)
      {
        CAIGroup* pGroup = it->second;
        pGroup->Serialize(ser, objectTracker);
        if(ser.IsReading())
        {

          // assign group to leader and viceversa
          int groupid = it->first;
          CLeader* pLeader = NULL;
          bool found = false;

          AIObjects::const_iterator itl = m_Objects.find(AIOBJECT_LEADER);

          while(!found && itl!=itobjend && itl->first == AIOBJECT_LEADER)
          {
            pLeader = (CLeader*)itl->second;
            found = (pLeader->GetGroupId() == groupid);
            ++itl;
          }

          if(found && pLeader)
						pGroup->SetLeader(pLeader);
        }
      }
    }
    ser.EndGroup();


    m_pSmartObjects->Serialize( ser );

    char name[32];
    ser.BeginGroup("AI_Alertness");
    ser.Value("m_CurrentGlobalAlertness",m_CurrentGlobalAlertness);
    for(int i=0;i<4;i++)
    {
      sprintf(name,"AlertnessCounter%d",i);
      ser.Value(name,m_AlertnessCounters[i]);
    }
    ser.EndGroup();

    ser.Value("m_nRaysThisUpdateFrame",m_nRaysThisUpdateFrame);
    ser.Value("m_nTickCount",m_nTickCount);
    ser.Value("m_fLastPathfindTimeStart",m_fLastPathfindTimeStart);
    ser.Value("m_bUpdateSmartObjects",m_bUpdateSmartObjects);
    objectTracker.SerializeValueContainer(ser,"m_deadBodies",m_deadBodies);
    objectTracker.SerializeValueContainer(ser,"m_explosionSpots",m_explosionSpots);
    objectTracker.SerializeValueObjectContainer(ser,"m_mapAuxSignalsFired",m_mapAuxSignalsFired);
//		objectTracker.SerializeObjectPointerSet(ser,"m_enabledPuppetsSet",m_enabledPuppetsSet);
//		objectTracker.SerializeObjectPointerSet(ser,"m_disabledPuppetsSet",m_disabledPuppetsSet);

		m_isPointOnForbiddenEdgeQueue.Clear();
    m_isPointInForbiddenRegionQueue.Clear();
  }
  ser.EndGroup();
}

//====================================================================
// GetVehicleList
//====================================================================
void CAISystem::ClearAllLeaders( ) 
{
	for (AIObjects::iterator it = m_Objects.find(AIOBJECT_LEADER), itEnd = m_Objects.end();  
			 it != itEnd && it->first == AIOBJECT_LEADER; )
	{
		AIObjects::iterator itRemove = it;
		++it;
		RemoveObject(itRemove->second);
	}
}

//////////////////////////////////////////////////////////////////////////
// Vehicle finder 
//////////////////////////////////////////////////////////////////////////

//====================================================================
// GetVehicleList
//====================================================================
TAIObjectList* CAISystem::GetVehicleList(IAIObject* pRequestor) 
{
	return m_pVehicleFinder->GetSortedVehicleList(static_cast<CAIObject*>(pRequestor));
}
 
//====================================================================
// RequestVehicle
//====================================================================
bool CAISystem::RequestVehicle(IAIObject* pRequestor, Vec3 &vSourcePos, IAIObject *pAIObjectTarget, const Vec3& vTargetPos, int iGoalType)
{
	return m_pVehicleFinder->AddRequest(static_cast<CAIObject*>( pRequestor), vSourcePos, static_cast<CAIObject*>(pAIObjectTarget), vTargetPos, iGoalType);
}


//////////////////////////////////////////////////////////////////////////
// Smart Objects related functions
//////////////////////////////////////////////////////////////////////////

void CAISystem::ReloadSmartObjectRules()
{
	CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
	AIAssert( gEnv->pEntitySystem );
	if ( m_pSmartObjects )
	{
		// reset states of smart objects to initial state
		m_pSmartObjects->ClearConditions();
		// ...and reload them
		LoadSmartObjectsLibrary();
	}
	CTimeValue t2 = gEnv->pTimer->GetAsyncTime();
	AILogComment( "All smart object rules reloaded in %g mSec.", (t2-t1).GetMilliSeconds() );
}

//====================================================================
// LoadSmartObjectsLibrary
//====================================================================
bool CAISystem::LoadSmartObjectsLibrary()
{
	AIAssert( m_pSmartObjects );
	m_pSmartObjects->LoadSOClassTemplates();

	char szPath[512];
	sprintf(szPath,"%s%s",PathUtil::GetGameFolder().c_str(),SMART_OBJECTS_XML);	
	XmlNodeRef root = GetISystem()->LoadXmlFile( szPath );
	if ( !root )
		return false;

	m_pSmartObjects->m_mapHelpers.clear();

	SmartObjectHelper helper;
	SmartObjectCondition condition;
	int count = root->getChildCount();
	for ( int i = 0; i < count; ++i )
	{
		XmlNodeRef node = root->getChild(i);
		if ( node->isTag("SmartObject") )
		{
			condition.iTemplateId = 0;
			node->getAttr( "iTemplateId", condition.iTemplateId );

			condition.sName = node->getAttr( "sName" );
			condition.sDescription = node->getAttr( "sDescription" );
			condition.sFolder = node->getAttr( "sFolder" );
			condition.sEvent = node->getAttr( "sEvent" );

			node->getAttr( "iMaxAlertness", condition.iMaxAlertness );
			node->getAttr( "bEnabled", condition.bEnabled );

			condition.iRuleType = 0;
			node->getAttr( "iRuleType", condition.iRuleType );
			float fPassRadius = 0.0f; // default value if not found in XML
			node->getAttr( "fPassRadius", fPassRadius );
			if ( fPassRadius > 0 )
				condition.iRuleType = 1;

			condition.sEntranceHelper = node->getAttr( "sEntranceHelper" );
			condition.sExitHelper = node->getAttr( "sExitHelper" );

			condition.sUserClass = node->getAttr( "sUserClass" );
			condition.sUserState = node->getAttr( "sUserState" );
			condition.sUserHelper = node->getAttr( "sUserHelper" );

			condition.sObjectClass = node->getAttr( "sObjectClass" );
			condition.sObjectState = node->getAttr( "sObjectState" );
			condition.sObjectHelper = node->getAttr( "sObjectHelper" );

			node->getAttr( "fMinDelay", condition.fMinDelay );
			node->getAttr( "fMaxDelay", condition.fMaxDelay );
			node->getAttr( "fMemory", condition.fMemory );

			condition.fDistanceFrom = 0.0f; // default value if not found in XML
			node->getAttr( "fDistanceFrom", condition.fDistanceFrom );
			node->getAttr( "fDistanceLimit", condition.fDistanceTo );
			node->getAttr( "fOrientationLimit", condition.fOrientationLimit );
			condition.fOrientationToTargetLimit = 360.0f; // default value if not found in XML
			node->getAttr( "fOrientToTargetLimit", condition.fOrientationToTargetLimit );

			node->getAttr( "fProximityFactor", condition.fProximityFactor );
			node->getAttr( "fOrientationFactor", condition.fOrientationFactor );
			node->getAttr( "fVisibilityFactor", condition.fVisibilityFactor );
			node->getAttr( "fRandomnessFactor", condition.fRandomnessFactor );

			node->getAttr( "fLookAtOnPerc", condition.fLookAtOnPerc );
			condition.sUserPreActionState = node->getAttr( "sUserPreActionState" );
			condition.sObjectPreActionState = node->getAttr( "sObjectPreActionState" );

			condition.sAction = node->getAttr( "sAction" );
			int iActionType;
			if (node->getAttr( "eActionType", iActionType ))
			{
				condition.eActionType = (EActionType) iActionType;
			}
			else
			{
				condition.eActionType = eAT_None;

				bool bSignal = !condition.sAction.empty() && condition.sAction[0] == '%';
				bool bAnimationSignal = !condition.sAction.empty() && condition.sAction[0] == '$';
				bool bAnimationAction = !condition.sAction.empty() && condition.sAction[0] == '@';
				bool bAction = !condition.sAction.empty() && !bSignal && !bAnimationSignal && !bAnimationAction;

				bool bHighPriority = condition.iMaxAlertness >= 100;
				node->getAttr( "bHighPriority", bHighPriority );

				if ( bHighPriority && bAction )
					condition.eActionType = eAT_PriorityAction;
				else if ( bAction )
					condition.eActionType = eAT_Action;
				else if ( bSignal )
					condition.eActionType = eAT_AISignal;
				else if ( bAnimationSignal )
					condition.eActionType = eAT_AnimationSignal;
				else if ( bAnimationAction )
					condition.eActionType = eAT_AnimationAction;

				if ( bSignal || bAnimationSignal || bAnimationAction )
					condition.sAction.erase( 0, 1 );
			}
			condition.iMaxAlertness %= 100;
			if ( !node->getAttr( "bEarlyPathRegeneration", condition.bEarlyPathRegeneration ) )
				condition.bEarlyPathRegeneration = false;

			condition.sUserPostActionState = node->getAttr( "sUserPostActionState" );
			condition.sObjectPostActionState = node->getAttr( "sObjectPostActionState" );

			node->getAttr( "fApproachSpeed", condition.fApproachSpeed );
			node->getAttr( "iApproachStance", condition.iApproachStance );
			condition.sAnimationHelper = node->getAttr( "sAnimationHelper" );
			condition.sApproachHelper = node->getAttr( "sApproachHelper" );
			node->getAttr( "fStartRadiusTolerance", condition.fStartRadiusTolerance );
			node->getAttr( "fStartDirectionTolerance", condition.fDirectionTolerance );
			node->getAttr( "fTargetRadiusTolerance", condition.fTargetRadiusTolerance );

			if ( !node->getAttr("iOrder", condition.iOrder) )
				condition.iOrder = i;

			m_pSmartObjects->AddSmartObjectCondition( condition );
		}
		else if ( node->isTag("Helper") )
		{
			string className = node->getAttr( "className" );
			helper.name = node->getAttr( "name" );
			Vec3 pos;
			node->getAttr( "pos", pos );
			Quat rot;
			node->getAttr( "rot", rot );
			helper.qt = QuatT( rot, pos );
			helper.description = node->getAttr( "description" );

			MapSOHelpers::iterator it = m_pSmartObjects->m_mapHelpers.insert( std::make_pair(className, helper) );

			CSmartObjectClass* pClass = m_pSmartObjects->GetSmartObjectClass( className );
			pClass->AddHelper( &it->second );
		}
/* ignore the events - description is not needed in game
		else if ( node->isTag("Event") )
		{
			string name = node->getAttr( "name" );
			string description = node->getAttr( "description" );
			CEvent* pSmartObjectEvent = m_pSmartObjects->String2Event( name );
			pSmartObjectEvent->m_sDescription = description;
		}
*/
		else if ( node->isTag("Class") )
		{
			string templateName = node->getAttr( "template" );
			if ( !templateName.empty() )
			{
				templateName.MakeLower();
				string className = node->getAttr( "name" );
				CSmartObjectClass* pClass = m_pSmartObjects->GetSmartObjectClass( className );
				if ( pClass )
				{
					MapClassTemplateData::iterator itFind = m_pSmartObjects->m_mapClassTemplates.find( templateName );
					if ( itFind != m_pSmartObjects->m_mapClassTemplates.end() )
					{
						CClassTemplateData* pTemplateData = &itFind->second;
						pClass->m_pTemplateData = pTemplateData;
						CClassTemplateData::TTemplateHelpers::iterator itHelpers, itHelpersEnd = pTemplateData->helpers.end();
						int idx = 0;
						for ( itHelpers = pTemplateData->helpers.begin(); itHelpers != itHelpersEnd; ++itHelpers, ++idx )
						{
							helper.name = itHelpers->name;
							helper.qt = itHelpers->qt;
							helper.description.clear();
							helper.templateHelperIndex = idx;
							MapSOHelpers::iterator it = m_pSmartObjects->m_mapHelpers.insert( std::make_pair(className, helper) );
							pClass->AddHelper( &it->second );
						}
					}
				}
			}
		}
	}

	m_pSmartObjects->SoftReset();
	return true;
}

//====================================================================
// AddSmartObjectCondition
//====================================================================
void CAISystem::AddSmartObjectCondition( const SmartObjectCondition& condition )
{
	AIAssert( m_pSmartObjects );
	m_pSmartObjects->AddSmartObjectCondition( condition );
}

//====================================================================
// GetSmartObjectStateName
//====================================================================
const char* CAISystem::GetSmartObjectStateName( int id ) const
{
	AIAssert( m_pSmartObjects );
	return CSmartObject::CState::GetStateName( id );
}

//====================================================================
// GetSmartObjectHelper
//====================================================================
SmartObjectHelper* CAISystem::GetSmartObjectHelper( const char* className, const char* helperName ) const
{
	AIAssert( m_pSmartObjects );
	CSmartObjectClass* pClass = CSmartObjectClass::find( className );
	if ( pClass )
		return pClass->GetHelper( helperName );
	else
		return NULL;
}

//====================================================================
// RegisterSmartObjectState
//====================================================================
void CAISystem::RegisterSmartObjectState( const char* sState )
{
	if ( *sState )
	{
		// it's enough to create only one temp. instance of CState
		CSmartObject::CState state( sState );
	}
}

//====================================================================
// AddSmartObjectState
//====================================================================
void CAISystem::AddSmartObjectState( IEntity* pEntity, const char* sState )
{
	if ( m_pSmartObjects && sState && *sState )
	{
		CSmartObject::CState state( sState );
		m_pSmartObjects->AddSmartObjectState( pEntity, state );
	}
}

//====================================================================
// RemoveSmartObjectState
//====================================================================
void CAISystem::RemoveSmartObjectState( IEntity* pEntity, const char* sState )
{
	if ( m_pSmartObjects && sState && *sState )
	{
		CSmartObject::CState state( sState );
		m_pSmartObjects->RemoveSmartObjectState( pEntity, state );
	}
}

//====================================================================
// SetSmartObjectState
//====================================================================
void CAISystem::SetSmartObjectState( IEntity* pEntity, const char* sStateName )
{
	if(!IsEnabled())
		return;
	if ( m_pSmartObjects && sStateName && *sStateName )
	{
		CSmartObject::CState state( sStateName );
		m_pSmartObjects->SetSmartObjectState( pEntity, state );
	}
}

//====================================================================
// ModifySmartObjectStates
//====================================================================
void CAISystem::ModifySmartObjectStates( IEntity* pEntity, const char* listStates )
{
	if(!IsEnabled())
		return;
	if ( !m_pSmartObjects )
		return;
	AIAssert( pEntity );
	AIAssert( listStates );
	CSmartObject::DoubleVectorStates states;
	CSmartObjects::String2States(listStates, states);
	m_pSmartObjects->ModifySmartObjectStates( pEntity, states );
}

//====================================================================
// CheckSmartObjectStates
//====================================================================
bool CAISystem::CheckSmartObjectStates( IEntity* pEntity, const char* patternStates ) const
{
	if ( !m_pSmartObjects )
		return false;
	AIAssert( pEntity );
	AIAssert( patternStates );
	CSmartObject::CStatePattern pattern;
	m_pSmartObjects->String2StatePattern( patternStates, pattern );
	return m_pSmartObjects->CheckSmartObjectStates( pEntity, pattern );
}

//====================================================================
// GetSmartObjectLinkCostFactor
//====================================================================
float CAISystem::GetSmartObjectLinkCostFactor( const GraphNode* nodes[2], const CAIObject* pRequester ) const
{
	if (!pRequester)
		return -1;

	if ( !m_pSmartObjects )
		return -1;

	// the link must be only between two nodes created by the same smart object
	AIAssert( nodes[0]->GetSmartObjectNavData()->pSmartObject == nodes[1]->GetSmartObjectNavData()->pSmartObject );

	// also it must be between two helpers belonging to a same smart object class
	AIAssert( nodes[0]->GetSmartObjectNavData()->pClass == nodes[1]->GetSmartObjectNavData()->pClass );

	CSmartObject* pObject = nodes[0]->GetSmartObjectNavData()->pSmartObject;
	if ( pObject->GetValidationResult() == eSOV_InvalidStatic )
	{
		if (m_cvDebugPathFinding->GetIVal())
		{
			AILogAlways("CAISystem::GetSmartObjectLinkCostFactor %s NAVSO %s is statically invalid.",
				pRequester->GetName(), pObject->GetEntity() ? pObject->GetEntity()->GetName() : "<unknown>");
		}
		return -1;
	}

	if (m_bannedNavSmartObjects.find(pObject) != m_bannedNavSmartObjects.end())
		return -1;

/*	if ( !m_pSmartObjects->ValidateTemplate(pObject, false, pRequester->GetEntity()) )
	{
		if (m_cvDebugPathFinding->GetIVal())
		{
			AILogAlways("CAISystem::GetSmartObjectLinkCostFactor %s Template validation for NAVSO %s failed.",
				pRequester->GetName(), pObject->GetEntity() ? pObject->GetEntity()->GetName() : "<unknown>");
		}
		return -1;
	}*/

	CSmartObjectClass* pObjectClass = nodes[0]->GetSmartObjectNavData()->pClass;
	SmartObjectHelper* pFromHelper = nodes[0]->GetSmartObjectNavData()->pHelper;
	SmartObjectHelper* pToHelper = nodes[1]->GetSmartObjectNavData()->pHelper;

	const CPipeUser* pPipeUser = pRequester->CastToCPipeUser();
	AIAssert( pPipeUser );

	CSmartObjectClass::HelperLink* vHelperLinks[1];
	if ( FindSmartObjectLinks( pPipeUser, pObject, pObjectClass, pFromHelper, pToHelper, vHelperLinks ) )
		return vHelperLinks[0]->condition->fProximityFactor;
	else
		pPipeUser->InvalidateSOLink( pObject, pFromHelper, pToHelper );
	
	if (m_cvDebugPathFinding->GetIVal())
	{
		AILogAlways("CAISystem::GetSmartObjectLinkCostFactor %s Cannot find any links for NAVSO %s.",
			pRequester->GetName(), pObject->GetEntity() ? pObject->GetEntity()->GetName() : "<unknown>");
	}
	return -1;
}

//====================================================================
// GetNavigationalSmartObjectActionType
//====================================================================
int CAISystem::GetNavigationalSmartObjectActionType( CPipeUser* pPipeUser, const GraphNode* pFromNode, const GraphNode* pToNode )
{
	IEntity* pEntity = pPipeUser->GetEntity();
	CSmartObject* pUser = GetAISystem()->GetSmartObjects()->IsSmartObject( pEntity );
	if ( !pUser )
		return nSOmNone;

	CSmartObject* pObject = pFromNode->GetSmartObjectNavData()->pSmartObject;
	CSmartObjectClass* pObjectClass = pFromNode->GetSmartObjectNavData()->pClass;
	SmartObjectHelper* pFromHelper = pFromNode->GetSmartObjectNavData()->pHelper;
	SmartObjectHelper* pToHelper = pToNode->GetSmartObjectNavData()->pHelper;

	CSmartObjectClass::HelperLink* vHelperLinks[1];
	if ( !GetAISystem()->FindSmartObjectLinks( pPipeUser, pObject, pObjectClass, pFromHelper, pToHelper, vHelperLinks ) )
	{
		return nSOmNone;
	}

	switch( vHelperLinks[0]->condition->eActionType )
	{
	case eAT_AnimationSignal:
		// there is an ending animation to be played
		return nSOmSignalAnimation;
	case eAT_AnimationAction:
		// there is an ending animation to be played
		return nSOmActionAnimation;
	case eAT_None:
		// directly passable link
		return nSOmStraight;
	default:
		// Only the above methods are supported. The definition of the navSO is likely bad.
		return nSOmNone;
	}
	return nSOmNone;
}

//====================================================================
// PrepareNavigateSmartObject
//====================================================================
bool CAISystem::PrepareNavigateSmartObject( CPipeUser* pPipeUser, const GraphNode* pFromNode, const GraphNode* pToNode )
{
	// Sanity check that the nodes are indeed smart object nodes (the node data is accessed later).
	if (pFromNode->navType != IAISystem::NAV_SMARTOBJECT || pToNode->navType != IAISystem::NAV_SMARTOBJECT)
	{
		if (m_cvDebugPathFinding->GetIVal())
		{
			AILogAlways("CAISystem::PrepareNavigateSmartObject %s one of the input nodes is not SmartObject nav node.",
				pPipeUser->GetName());
		}
		return false;
	}

	IEntity* pEntity = pPipeUser->GetEntity();
	CSmartObject* pUser = GetAISystem()->GetSmartObjects()->IsSmartObject( pEntity );
	if(!pUser)
		return false;

	CSmartObject* pObject = pFromNode->GetSmartObjectNavData()->pSmartObject;
	CSmartObjectClass* pObjectClass = pFromNode->GetSmartObjectNavData()->pClass;
	SmartObjectHelper* pFromHelper = pFromNode->GetSmartObjectNavData()->pHelper;
	SmartObjectHelper* pToHelper = pToNode->GetSmartObjectNavData()->pHelper;

	// Check if the NAVSO is valid.
	if ( pObject->GetValidationResult() == eSOV_InvalidStatic )
	{
		if (m_cvDebugPathFinding->GetIVal())
		{
			AILogAlways("CAISystem::PrepareNavigateSmartObject %s NAVSO %s is statically invalid.",
				pPipeUser->GetName(), pObject->GetEntity() ? pObject->GetEntity()->GetName() : "<unknown>");
		}
		return false;
	}

	if ( !m_pSmartObjects->ValidateTemplate(pObject, false, pPipeUser->GetEntity(),
					pFromHelper->templateHelperIndex, pToHelper->templateHelperIndex) )
	{
		if (m_cvDebugPathFinding->GetIVal())
		{
			AILogAlways("CAISystem::PrepareNavigateSmartObject %s Template validation for NAVSO %s failed.",
				pPipeUser->GetName(), pObject->GetEntity() ? pObject->GetEntity()->GetName() : "<unknown>");
		}
		m_bannedNavSmartObjects[pObject] = m_cvBannedNavSoTime->GetFVal();
		return false;
	}

	CSmartObjectClass::HelperLink* vHelperLinks[8];
	uint numVariations = GetAISystem()->FindSmartObjectLinks( pPipeUser, pObject, pObjectClass, pFromHelper, pToHelper, vHelperLinks, 8 );

	// remove non-animation rules
	int i = numVariations;
	while ( i-- )
		if ( vHelperLinks[i]->condition->eActionType != eAT_AnimationSignal && vHelperLinks[i]->condition->eActionType != eAT_AnimationAction )
			std::swap( vHelperLinks[i], vHelperLinks[--numVariations] );

	if ( !numVariations )
	{
		return false;
	}
	// randomly choose one of the available rules
	std::swap( vHelperLinks[0], vHelperLinks[Random( numVariations )] );

	// there is a matching smart object rule - use it!
	pPipeUser->m_State.actorTargetReq.Reset();
	pPipeUser->m_State.actorTargetReq.lowerPrecision = true;
	if ( GetAISystem()->GetSmartObjects()->PrepareUseNavigationSmartObject( pPipeUser->m_State.actorTargetReq, pPipeUser->m_pendingNavSOStates, pUser, pObject, vHelperLinks[0]->condition, pFromHelper, pToHelper ) )
	{
		pPipeUser->m_State.actorTargetReq.approachLocation = pObject->GetHelperPos( pFromHelper );
		pPipeUser->m_State.actorTargetReq.approachDirection = pObject->GetOrientation( pFromHelper );

		// TODO: use the entity pos/orientation instead once we start using the template!
		if(m_cvUseObjectPosWithExactPos->GetIVal() == 1)
		{
			pPipeUser->m_State.actorTargetReq.animLocation = pObject->GetEntity()->GetPos();
			pPipeUser->m_State.actorTargetReq.animDirection = pPipeUser->m_State.actorTargetReq.approachDirection;
		}
		else
		{
			pPipeUser->m_State.actorTargetReq.animLocation = pPipeUser->m_State.actorTargetReq. approachLocation;
			pPipeUser->m_State.actorTargetReq.animDirection = pPipeUser->m_State.actorTargetReq.approachDirection;
		}

		if ( !vHelperLinks[0]->condition->sAnimationHelper.empty() )
		{
			// animation helper has been specified - override the animLocation and direction
			SmartObjectHelper* pAnimHelper = pObjectClass->GetHelper( vHelperLinks[0]->condition->sAnimationHelper.c_str() );
			if ( pAnimHelper )
			{
				pPipeUser->m_State.actorTargetReq.animLocation = pObject->GetHelperPos( pAnimHelper );
				pPipeUser->m_State.actorTargetReq.animDirection = pObject->GetOrientation( pAnimHelper );
			}
		}

		pPipeUser->m_navSOEarlyPathRegen = vHelperLinks[0]->condition->bEarlyPathRegeneration;

		switch( vHelperLinks[0]->condition->eActionType )
		{
		case eAT_AnimationSignal:
			// there is an ending animation to be played
			pPipeUser->m_eNavSOMethod = nSOmSignalAnimation;
			return true;
		case eAT_AnimationAction:
			// there is an ending animation to be played
			pPipeUser->m_eNavSOMethod = nSOmActionAnimation;
			return true;
		default:
			// Only the above methods are supported. The definition of the navSO is likely bad.
			AIAssert(0);
		}
	}

	return false;

}

//====================================================================
// FindSmartObjectLinks
//====================================================================
int CAISystem::FindSmartObjectLinks( const CPipeUser* pPipeUser, CSmartObject* pObject, CSmartObjectClass* pObjectClass,
	SmartObjectHelper* pFromHelper, SmartObjectHelper* pToHelper, CSmartObjectClass::HelperLink** pvHelperLinks, int iCount /*=1*/ ) const
{
	AIAssert( m_pSmartObjects );

	// hidden objects can not be used!
	if ( pObject->IsHidden() )
		return 0;

	if ( pPipeUser->IsSOLinkInvalidated(pObject, pFromHelper, pToHelper) )
	{
		if (m_cvDebugPathFinding->GetIVal())
		{
			AILogAlways("CAISystem::FindSmartObjectLinks %s NAVSO %s appears in pipeuser invalid list.",
				pPipeUser->GetName(), pObject->GetEntity() ? pObject->GetEntity()->GetName() : "<unknwon>");
		}
		return 0;
	}

	IEntity* pEntity = pPipeUser->GetEntity();
	CSmartObject* pUser = m_pSmartObjects->IsSmartObject( pEntity );
	if ( !pUser )
		return 0;

	float radius = pPipeUser->GetRadius();
	int numFound = 0;

	// check which user's classes can use the target object
	CSmartObjectClasses& vClasses = pUser->GetClasses();
	CSmartObjectClasses::iterator itClasses, itClassesEnd = vClasses.end();
	for ( itClasses = vClasses.begin(); iCount && itClasses != itClassesEnd; ++itClasses )
	{
		CSmartObjectClass* pClass = *itClasses;
		if ( pClass->IsSmartObjectUser() )
		{
			int count = pObjectClass->FindHelperLinks( pFromHelper, pToHelper, pClass, 1.0f, pUser->GetStates(), pObject->GetStates(),
				pvHelperLinks + numFound, iCount );
			numFound += count;
			iCount -= count;
		}
	}

	return numFound;
}

// notifies that entity has changed its position, which is important for smart objects
void CAISystem::NotifyAIObjectMoved( IEntity* pEntity, SEntityEvent event )
{
	if(!IsEnabled())
		return;
	AIAssert( m_pSmartObjects );
	((IEntitySystemSink*)m_pSmartObjects)->OnEvent( pEntity, event );
}


//====================================================================
// GetAIAction
//====================================================================
IAIAction* CAISystem::GetAIAction( const char* name )
{
	AIAssert( m_pActionManager );
	return m_pActionManager->GetAIAction( name );
}

//====================================================================
// GetAIAction
//====================================================================
IAIAction* CAISystem::GetAIAction( size_t index )
{
	AIAssert( m_pActionManager );
	return m_pActionManager->GetAIAction( index );
}

//====================================================================
// GetAIActionName
//====================================================================
const char* CAISystem::GetAIActionName( int index ) const
{
	AIAssert( m_pActionManager );
	CAIAction* pAction = m_pActionManager->GetAIAction( index );
	return pAction ? pAction->GetName() : NULL;
}

//====================================================================
// AbortAIAction
//====================================================================
void CAISystem::AbortAIAction( IEntity* pUser, int goalPipeId )
{
	AIAssert( m_pActionManager );
	m_pActionManager->AbortActionsOnEntity( pUser, goalPipeId );
}

//====================================================================
// ExecuteAIAction
//====================================================================
void CAISystem::ExecuteAIAction( const char* sActionName, IEntity* pUser, IEntity* pObject, int maxAlertness, int goalPipeId )
{
	AIAssert( m_pActionManager );
	string str( sActionName );
	str += ",\"\",\"\",\"\",\"\",";
	int i = str.find(',', 0);
	int j = str.find('\"', i+2);
	int k = str.find('\"', j+3);
	int l = str.find('\"', k+3);
	int m = str.find('\"', l+3);
	CAIAction* pAction = m_pActionManager->GetAIAction( str.substr(0, i) );
	if ( pAction )
	{
		m_pActionManager->ExecuteAction( pAction, pUser, pObject, maxAlertness, goalPipeId,
			str.substr(i+2,j-i-2), str.substr(j+3,k-j-3), str.substr(k+3,l-k-3), str.substr(l+3,m-l-3) );
	}
	else
  {
		AIError( "Requested execution of undefined AI Action '%s' [Design bug]", sActionName );
  }
}

//====================================================================
// ReloadActions
//====================================================================
void CAISystem::ReloadActions()
{
	CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
	AIAssert( gEnv->pEntitySystem );
	if ( m_pSmartObjects )
	{
		// reset states of smart objects to initial state
		//m_pSmartObjects->ClearConditions();

		m_pActionManager->LoadLibrary( AI_ACTIONS_PATH );

		// ...and reload them
		//LoadSmartObjectsLibrary();
	}
	CTimeValue t2 = gEnv->pTimer->GetAsyncTime();
	AILogComment( "All AI Actions reloaded in %g mSec.", (t2-t1).GetMilliSeconds() );
}

//////////////////////////////////////////////////////////////////////////
// smart object events related functions
//////////////////////////////////////////////////////////////////////////

//====================================================================
// SmartObjectEvent
//====================================================================
int CAISystem::SmartObjectEvent( const char* sEventName, IEntity*& pUser, IEntity*& pObject, const Vec3* pExtraPoint /*=NULL*/, bool bHighPriority /*=false*/ )
{
	if (!m_pSmartObjects) // can be zero in multiplayer
		return 0;
	return m_pSmartObjects->TriggerEvent( sEventName, pUser, pObject, NULL, pExtraPoint, bHighPriority );
}

//====================================================================
// GetSmartObjects
//====================================================================
CSmartObjects* CAISystem::GetSmartObjects()
{
	return m_pSmartObjects;
}

/*
float CAISystem::GetAreaMinSize(Vec3 point, bool b3D,int numIter)
{
	//Vec3 vSize(10000,10000,b3D? 10000:0);
	float fMinSize=200;
	Vec3 dir(0,1,0);
	Vec3 v2DAxis(0,0,1);
	unsigned int filter = ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid;
	float angle = g_PI/4/(float)(numIter-1);
	ray_hit hit;
	if(!b3D)
		point.z+=1;
    for(int i=0;i<numIter;i++)
	{
		float fdist;
		int rayresult = m_pWorld->RayWorldIntersection(point,point+dir*fMinSize/2,filter, 0,&hit,1);
		if(rayresult)		
		{
			float fSize = hit.dist;
			rayresult = m_pWorld->RayWorldIntersection(point,point-dir*fMinSize/2,filter, 0,&hit,1);
			if(rayresult)
			{
				fSize+=hit.dist;
				if(fSize<fMinSize)
					fMinSize = fSize;
			}
		}
		dir = dir.GetRotated(v2DAxis,angle);
	}
	if(b3D)
	{
		// TO DO
	}
	return fMinSize;
}*/

//====================================================================
// RegisterFirecommandHandler
//====================================================================
void CAISystem::RegisterFirecommandHandler(IFireCommandDesc* desc)
{
	for(std::list<IFireCommandDesc*>::iterator it = m_firecommandDescriptors.begin(); it != m_firecommandDescriptors.end(); ++it)
	{
		if((*it) == desc)
		{
			desc->Release();
			return;
		}
	}
	m_firecommandDescriptors.push_back(desc);
}

//====================================================================
// CreateFirecommandHandler
//====================================================================
IFireCommandHandler* CAISystem::CreateFirecommandHandler(const char* name, IAIActor *pShooter)
{
	for(std::list<IFireCommandDesc*>::iterator it = m_firecommandDescriptors.begin(); it != m_firecommandDescriptors.end(); ++it)
	{
		if(stricmp((*it)->GetName(), name) == 0)
			return (*it)->Create(pShooter);
	}
	AIWarning("CAISystem::CreateFirecommandHandler: Could not find firecommand handler '%s'", name);
	return 0;
}

//====================================================================
// CombatClasses - add new
//====================================================================
void CAISystem::AddCombatClass(int combatClass, float* pScalesVector, int size, const char* szCustomSignal)
{
	if (size == 0)
	{
		m_CombatClasses.clear();
		return;	
	}

	// Sanity check
	AIAssert(combatClass >= 0 && combatClass < 100);

	if (combatClass >= (int)m_CombatClasses.size())
		m_CombatClasses.resize(combatClass+1);

	SCombatClassDesc& desc = m_CombatClasses[combatClass];
	if (szCustomSignal && strlen(szCustomSignal) > 1)
		desc.customSignal = szCustomSignal;
	else
		desc.customSignal.clear();

	desc.mods.resize(size);
	for (int i = 0; i < size; ++i)
		desc.mods[i] = pScalesVector[i];
}

//====================================================================
// AIHandler calls this function to replace the OnPlayerSeen signal
//====================================================================
const char* CAISystem::GetCustomOnSeenSignal(int combatClass)
{
	if (combatClass < 0 || combatClass >= m_CombatClasses.size())
		return 0;
	if (m_CombatClasses[combatClass].customSignal.empty())
		return 0;
	return m_CombatClasses[combatClass].customSignal.c_str();
}

//====================================================================
// GetCombatClassScale - retrieves scale for given target/shooter
//====================================================================
float	CAISystem::GetCombatClassScale(int shooterClass, int targetClass)
{
	if (targetClass < 0 || shooterClass < 0 || shooterClass >= (int)m_CombatClasses.size())
		return 1.0f;
	SCombatClassDesc& desc = m_CombatClasses[shooterClass];
	if (targetClass >= desc.mods.size())
		return 1.0f;
	return desc.mods[targetClass];
}

//===================================================================
// GetDangerSpots
//===================================================================
unsigned int CAISystem::GetDangerSpots(const IAIObject* requester, float range, Vec3* positions, unsigned int* types,
																			 unsigned int n, unsigned int flags)
{
	int	maxBlockers = (int)n;
	float	rangeSq = sqr(range);

	const Vec3&	reqPos = requester->GetPos();
	
	const CAIActor* pActor = requester->CastToCAIActor();
	unsigned int species = pActor->GetParameters().m_nSpecies;
	unsigned int i = 0;

	if(flags & DANGER_EXPLOSIVE)
	{
		// Collect all grenades and merge close ones.
		std::list<Vec3>	grenades;
		AIObjects::const_iterator aio = m_Objects.find(AIOBJECT_GRENADE);
		for(; aio != m_Objects.end(); ++aio)
		{
			// TODO: add other explosives here too (maybe check if the requester knows about them also too)
			if (aio->first != AIOBJECT_GRENADE)
				break;

			// Skip explosives which are too far away.
			const Vec3&	pos = (aio->second)->GetPos();
			if(Distance::Point_PointSq(pos, reqPos) > rangeSq)
				continue;

			// Merge with current greanades if possible.
			bool	merged = false;
			for(std::list<Vec3>::iterator it = grenades.begin(); it != grenades.end(); ++it)
			{
				if(Distance::Point_Point((*it), pos) < 3.0f)
				{
					merged = true;
					break;
				}
			}

			// If cannot be merged, add new.
			if(!merged)
				grenades.push_back(pos);
		}

		for(std::list<Vec3>::iterator it = grenades.begin(); it != grenades.end(); ++it)
		{
			// Limit the number of points to output.
			if(i >= n) break;
			// Output point
			positions[i] = (*it);
			if(types) types[i] = DANGER_EXPLOSIVE;
			i++;
		}
	}

	if(flags & DANGER_EXPLOSION_SPOT)
	{
		// The explosions positions are merged already when they are added to the list.
		for (unsigned j = 0, nj = m_explosionSpots.size(); j < nj; ++j)
		{
			// Limit the number of points to output.
			if(i >= n) break;

			// Skip far away explosions.
			if(Distance::Point_PointSq(m_explosionSpots[j].pos, reqPos) > rangeSq)
				continue;
			// Output the point.
			positions[i] = m_explosionSpots[j].pos;
			if(types) types[i] = DANGER_EXPLOSION_SPOT;
			i++;
		}
	}

	if(flags & DANGER_DEADBODY)
	{
		// The dead body positions are merged already when they are added to the list.
		for (unsigned j = 0, nj = m_deadBodies.size(); j < nj; ++j)
		{
			// Limit the number of points to output.
			if(i >= n) break;

			// Skip dead bodies of different species.
			if (m_deadBodies[j].species != species) continue;

			// Skip far away bodies.
			const Vec3& p = m_deadBodies[j].pos;
			if(Distance::Point_PointSq(p, reqPos) > rangeSq)
				continue;
			// Output the point.
			positions[i] = p;
			if(types) types[i] = DANGER_DEADBODY;
			i++;
		}
	}

	// Returns number of points.
	return i;
}

//===================================================================
// RegisterDeadBody
//===================================================================
void CAISystem::RegisterDeadBody(unsigned int species, const Vec3& pos)
{
	// Check if the position is close to existing body
//	for(TDeadBodyList::iterator it = m_lstDeadBodies.begin(); it != m_lstDeadBodies.end(); ++it)
	for (unsigned i = 0, ni = m_deadBodies.size(); i < ni; ++i)
	{
		unsigned int s = m_deadBodies[i].species;
		Vec3& p = m_deadBodies[i].pos;
		if(s == species && Distance::Point_PointSq(p, pos) < sqr(2.0f))
		{
			// Merge the points
			p = (p + pos) * 0.5f;
			return;
		}
	}
	// Add the new body to the list.
	// Limit the number of bodies in the list.
	if (m_deadBodies.size() >= 20)
	{
		unsigned oldest = 0;
		float oldestTime = FLT_MAX;
		for (unsigned i = 0, ni = m_deadBodies.size(); i < ni; ++i)
		{
			if (m_deadBodies[i].t < oldestTime)
			{
				oldest = i;
				oldestTime = m_deadBodies[i].t;
			}
		}
		m_deadBodies[oldest].Set(species, pos);
	}
	else
	{
		m_deadBodies.push_back(SAIDeadBody(species, pos));
	}
}

//===================================================================
// RegisterExplosion
//===================================================================
void CAISystem::ExplosionEvent(const Vec3& pos, float radius, IAIObject* pShooter)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;

	AILogComment("CAISystem::ExplosionEvent radius=%f", radius);

	// For agents connected to a vehicle, use the vehicle AI instead.
	if (pShooter)
	{
		CAIActor* pShooterActor = pShooter->CastToCAIActor();
		if (pShooterActor && pShooterActor->GetProxy())
		{
			EntityId vehicleId = pShooterActor->GetProxy()->GetLinkedVehicleEntityId();
			if (vehicleId)
			{
				IEntity* pVehicleEnt = gEnv->pEntitySystem->GetEntity(vehicleId);
				if (pVehicleEnt && pVehicleEnt->GetAI())
					pShooter = pVehicleEnt->GetAI();
			}
		}
	}

	// AISound radius of explosion just a scaled up physical radius
	float bIsCrysis;
	gEnv->pAISystem->GetConstant(IAISystem::eAI_CONST_bIsCrysis1, bIsCrysis);
	float fExplosionWarningRadius;
	gEnv->pAISystem->GetConstant(IAISystem::eAI_CONST_fExplosionWarningRadius, fExplosionWarningRadius);

	SoundEvent(pos, radius * fExplosionWarningRadius, AISE_EXPLOSION, pShooter);

	float alarmRadiusSq = sqr(radius*15.0f);

	// React to explosions.
	for (unsigned j = 0, nj = m_enabledPuppetsSet.size(); j < nj; ++j)
	{
		CPuppet* pReceiverPuppet = m_enabledPuppetsSet[j];

		const float scale = pReceiverPuppet->GetParameters().m_PerceptionParams.collisionReactionScale;
		const float rangeSq = sqr(radius * 0.5f * scale);

		float distSq = Distance::Point_PointSq(pos, pReceiverPuppet->GetPos());

		if (!bIsCrysis)
		{
			//Alarm in a big radius
			if (distSq > alarmRadiusSq ) continue;
			pReceiverPuppet->SetAlarmed();
		}

		if (distSq > rangeSq) continue;

		IAISignalExtraData* pData = CreateSignalExtraData();
		pData->point = pos;
		pReceiverPuppet->SetSignal(0, "OnExposedToExplosion", 0, pData);
	
		if (bIsCrysis)
			pReceiverPuppet->SetAlarmed();
	}

	// update existing explosion if overlaps, or add new one
	const float	radiusSq = sqr(radius);
	int overlappedExplosion = -1;
	float	minDistSq = radiusSq;
	for (unsigned i = 0, ni = m_explosionSpots.size(); i < ni; ++i)
	{
		float	distSq = Distance::Point_PointSq(m_explosionSpots[i].pos, pos);
		if (distSq < radiusSq && distSq < minDistSq)
		{
			overlappedExplosion = (int)i;
			minDistSq = distSq;
		}
	}

	if (overlappedExplosion != -1)
		m_explosionSpots[overlappedExplosion].Reset(pos, m_ExplosionSpotLifeTime);
	else
		m_explosionSpots.push_back(SAIExplosionSpot(pos, radius, m_ExplosionSpotLifeTime));

	NotifyAIEventListeners(AIEVENT_EXPLOSION, pos, radius, 5.0f, pShooter ? pShooter->GetEntityID() : 0);
}

//===================================================================
// BulletHitEvent
//===================================================================
void CAISystem::BulletHitEvent(const Vec3& pos, float radius, float soundRadius, IAIObject* pShooter)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;

	// Filter out nearby hits.
	for (unsigned i = 0, ni = m_bulletEvents.size(); i < ni; ++i)
	{
		SAIBulletEvent& be = m_bulletEvents[i];
		float d = Distance::Point_PointSq(pos, be.pos);
		if (d < sqr(be.radius*0.5f))
		{
			// The new shot is really close to existing one.
			return;
		}
	}

	// For agents connected to a vehicle, use the vehicle AI instead.
	if (pShooter)
	{
		CAIActor* pShooterActor = pShooter->CastToCAIActor();
		if (pShooterActor && pShooterActor->GetProxy())
		{
			EntityId vehicleId = pShooterActor->GetProxy()->GetLinkedVehicleEntityId();
			if (vehicleId)
			{
				IEntity* pVehicleEnt = gEnv->pEntitySystem->GetEntity(vehicleId);
				if (pVehicleEnt && pVehicleEnt->GetAI())
					pShooter = pVehicleEnt->GetAI();
			}
		}
	}

	float bIsCrysis;
	gEnv->pAISystem->GetConstant(IAISystem::eAI_CONST_bIsCrysis1, bIsCrysis);

	if (bIsCrysis)
		SoundEvent(pos, soundRadius, AISE_COLLISION_LOUD, pShooter);
	else
		SoundEvent(pos, soundRadius, AISE_BULLETCOLLISION, pShooter);

	EntityId sender = pShooter ? pShooter->GetEntityID() : 0;
	NotifyAIEventListeners(AIEVENT_BULLET, pos, radius, 1.0f, sender);
	m_bulletEvents.push_back(SAIBulletEvent(sender, pos, radius, 0.5f));
}

//===================================================================
// BulletShotEvent
//===================================================================
void CAISystem::BulletShotEvent(const Vec3& pos, const Vec3& dir, float range, IAIObject* pShooter)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;

	const float halfRange = range * 0.5f;
	const Vec3 mid = pos + dir * halfRange;

	const float extraRad = 1.0f;

	Lineseg lof(pos, pos + dir*range);
	float t;

	// For agents connected to a vehicle, use the vehicle AI instead.
	if (pShooter)
	{
		CAIActor* pShooterActor = pShooter->CastToCAIActor();
		if (pShooterActor && pShooterActor->GetProxy())
		{
			EntityId vehicleId = pShooterActor->GetProxy()->GetLinkedVehicleEntityId();
			if (vehicleId)
			{
				IEntity* pVehicleEnt = gEnv->pEntitySystem->GetEntity(vehicleId);
				if (pVehicleEnt && pVehicleEnt->GetAI())
					pShooter = pVehicleEnt->GetAI();
			}
		}
	}

	CPuppet* pShooterPuppet = CastToCPuppetSafe(pShooter);

	for (unsigned i = 0, ni = m_enabledPuppetsSet.size(); i < ni; ++i)
	{
		CPuppet* pPuppet = m_enabledPuppetsSet[i];

		if (pPuppet == pShooterPuppet) continue;

		float d = Distance::Point_LinesegSq(pPuppet->GetPhysicsPos(), lof, t);
		if (d < sqr(5.0f))
		{
			const Vec3 hitPos = lof.GetPoint(t);
			const float hitRad = 1.0f;

			// Filter out nearby hits.
			bool filtered = false;
			for (unsigned j = 0, nj = m_bulletEvents.size(); j < nj; ++j)
			{
				SAIBulletEvent& be = m_bulletEvents[j];
				float distHitSqr = Distance::Point_PointSq(hitPos, be.pos);
				if (distHitSqr < sqr(be.radius))
				{
					// The new shot is really close to existing one.
					filtered = true;
					break;
				}
			}
			if (!filtered)
			{
				EntityId sender = pShooter ? pShooter->GetEntityID() : 0;
				NotifyAIEventListeners(AIEVENT_BULLET, hitPos, hitRad, 1.0f, sender);
				m_bulletEvents.push_back(SAIBulletEvent(sender, hitPos, hitRad, 0.5f));
			}
		}
	}
}

//===================================================================
// DynOmniLightEvent
//===================================================================
void CAISystem::DynOmniLightEvent(const Vec3& pos, float radius, EAILightEventType type, IAIObject* pShooter, float time)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;

	if(!IsEnabled())
		return;
	CAIActor* pActor = pShooter->CastToCAIActor();
	if (pActor)
		m_lightManager.DynOmniLightEvent(pos, radius, type, pActor, time);
}

//===================================================================
// DynSpotLightEvent
//===================================================================
void CAISystem::DynSpotLightEvent(const Vec3& pos, const Vec3& dir, float radius, float fov, EAILightEventType type, IAIObject* pShooter, float time)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;

	if(!IsEnabled())
		return;
	CAIActor* pActor = pShooter->CastToCAIActor();
	if (pActor)
		m_lightManager.DynSpotLightEvent(pos, dir, radius, fov, type, pActor, time);
}

//===================================================================
// GrenadeEvent
//===================================================================
void CAISystem::GrenadeEvent(const Vec3& pos, float radius, EAIGrenadeEventType type, IEntity* pGrenade, IEntity* pShooter)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	// Do not handle events while serializing.
	if (gEnv->pSystem->IsSerializingFile())
		return;

	if (!pGrenade)
		return;

	// Add the event, or update the current.
	EntityId grenadeId = pGrenade->GetId();
	EntityId shooterId = pShooter ? pShooter->GetId() : 0;
	
	SAIGrenadeEvent* pEvent = 0;
	for (unsigned i = 0, ni = m_grenadeEvents.size(); i < ni; ++i)
	{
		if (m_grenadeEvents[i].grenadeId == grenadeId)
		{
			pEvent = &m_grenadeEvents[i];
			break;
		}
	}
	if (!pEvent)
	{
		m_grenadeEvents.push_back(SAIGrenadeEvent(grenadeId, shooterId, pos, radius, 6.0f));
		pEvent = &m_grenadeEvents.back();
	}

	pEvent->pos = pos;
	pEvent->radius = max(pEvent->radius, radius);
	if (!pEvent->shooterId && shooterId)
		pEvent->shooterId = shooterId;

	if (!pShooter && pEvent->shooterId)
		pShooter = gEnv->pEntitySystem->GetEntity(pEvent->shooterId);

	CPuppet* pShooterPuppet = 0;
	if (pShooter)
		pShooterPuppet = CastToCPuppetSafe(pShooter->GetAI());

	if (type == AIGE_GRENADE_THROWN)
	{
		Vec3 throwPos = pGrenade->GetWorldPos();

		// Inform the AI that sees the throw
		for (unsigned i = 0, ni = m_enabledPuppetsSet.size(); i < ni; ++i)
		{
			CPuppet* pPuppet = m_enabledPuppetsSet[i];
			if (pPuppet == pShooterPuppet) continue;

			// If the puppet is not close to the predicted position, skip.
			if (Distance::Point_PointSq(pos, pPuppet->GetPos()) > sqr(20.0f))
				continue;

			// Inform enemies only.
			if (pShooterPuppet && !pShooterPuppet->IsHostile(pPuppet))
				continue;

			// Only sense grenades that are on front of the AI and visible when thrown.
			// Another signal is sent when the grenade hits the ground.
			Vec3 delta = throwPos - pPuppet->GetPos();	// grenade to AI
			float dist = delta.NormalizeSafe();
			const float thr = cosf(DEG2RAD(160.0f));
			if (delta.Dot(pPuppet->GetViewDir()) > thr)
			{
				ray_hit hit;
				static const int objTypes = ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid;
				static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
				int res = gEnv->pPhysicalWorld->RayWorldIntersection(pPuppet->GetPos(), delta*dist, objTypes, flags, &hit, 1);
				if (!res || hit.dist > dist*0.9f)
				{
					IAISignalExtraData* pEData = CreateSignalExtraData();	// no leak - this will be deleted inside SendAnonymousSignal
					pEData->point = pos;
					pEData->nID = shooterId;
					pEData->iValue = 1;
					SendSignal(SIGNALFILTER_SENDER, 1, "OnGrenadeDanger", pPuppet, pEData);
				}
			}
		}
	}
	else if (type == AIGE_GRENADE_COLLISION)
	{
		if (!pEvent->collided)
		{
			pEvent->collided = true;

			for (unsigned i = 0, ni = m_enabledPuppetsSet.size(); i < ni; ++i)
			{
				CPuppet* pPuppet = m_enabledPuppetsSet[i];
				// If the puppet is not close to the grenade, skip.
				if (Distance::Point_PointSq(pos, pPuppet->GetPos()) > sqr(12.0f))
					continue;
				IAISignalExtraData* pEData = CreateSignalExtraData();	// no leak - this will be deleted inside SendAnonymousSignal
				pEData->point = pos;
				pEData->nID = shooterId;
				pEData->iValue = 2;
				SendSignal(SIGNALFILTER_SENDER, 1, "OnGrenadeDanger", pPuppet, pEData);
			}
		}
	}
	else if (type == AIGE_FLASH_BANG)
	{
		for (unsigned i = 0, ni = m_enabledPuppetsSet.size(); i < ni; ++i)
		{
			CPuppet* pPuppet = m_enabledPuppetsSet[i];
			if (pPuppet == pShooterPuppet) continue;

			// If the puppet is not close to the flash, skip.
			if (Distance::Point_PointSq(pos, pPuppet->GetPos()) > sqr(radius))
				continue;

			// Only sense grenades that are on front of the AI and visible when thrown.
			// Another signal is sent when the grenade hits the ground.
			Vec3 delta = pos - pPuppet->GetPos();	// grenade to AI
			float dist = delta.NormalizeSafe();
			const float thr = cosf(DEG2RAD(160.0f));
			if (delta.Dot(pPuppet->GetViewDir()) > thr)
			{
				ray_hit hit;
				static const int objTypes = ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid;
				static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
				int res = gEnv->pPhysicalWorld->RayWorldIntersection(pPuppet->GetPos(), delta*dist, objTypes, flags, &hit, 1);
				if (!res)
				{
					IAISignalExtraData* pExtraData = CreateSignalExtraData();
					pExtraData->iValue = 0;
					SendSignal(SIGNALFILTER_SENDER, 1, "OnExposedToFlashBang", pPuppet, pExtraData);
				}
			}
		}
	}
	else if (type == AIGE_SMOKE)
	{
		for (unsigned i = 0, ni = m_enabledPuppetsSet.size(); i < ni; ++i)
		{
			CPuppet* pPuppet = m_enabledPuppetsSet[i];

			// If the puppet is not close to the smoke, skip.
			if (Distance::Point_PointSq(pos, pPuppet->GetPos()) > sqr(radius))
				continue;

			SendSignal(SIGNALFILTER_SENDER, 1, "OnExposedToSmoke", pPuppet);
		}
	}
}


//===================================================================
// RegisterAIEventListener
//===================================================================
void CAISystem::RegisterAIEventListener(IAIEventListener* pListener, const Vec3& pos, float rad, int flags)
{
	if (!pListener)
		return;

	// Check if the listener exists
	for (unsigned i = 0, ni = m_eventListeners.size(); i < ni; ++i)
	{
		SAIEventListener& listener = m_eventListeners[i];
		if (listener.pListener == pListener)
		{
			listener.pos = pos;
			listener.radius = rad;
			listener.flags = flags;
			return;
		}
	}

	m_eventListeners.resize(m_eventListeners.size() + 1);
	SAIEventListener& listener = m_eventListeners.back();
	listener.pListener = pListener;
	listener.pos = pos;
	listener.radius = rad;
	listener.flags = flags;
}

//===================================================================
// UnregisterAIEventListener
//===================================================================
void CAISystem::UnregisterAIEventListener(IAIEventListener* pListener)
{
	if (!pListener)
		return;

	// Check if the listener exists
	for (unsigned i = 0, ni = m_eventListeners.size(); i < ni; ++i)
	{
		SAIEventListener& listener = m_eventListeners[i];
		if (listener.pListener == pListener)
		{
			m_eventListeners[i] = m_eventListeners.back();
			m_eventListeners.pop_back();
			return;
		}
	}
}

//===================================================================
// NotifyAIEventListeners
//===================================================================
void CAISystem::NotifyAIEventListeners(EAIEventType type, const Vec3& pos, float radius, float threat, EntityId sender)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	int flags = (int)type;

	for (unsigned i = 0, ni = m_eventListeners.size(); i < ni; ++i)
	{
		SAIEventListener& listener = m_eventListeners[i];
		if ((listener.flags & flags) != 0 && Distance::Point_PointSq(pos, listener.pos) < sqr(listener.radius + radius))
			listener.pListener->OnAIEvent(type, pos, radius, threat, sender);
	}
}



//===================================================================
// IsPointAffectedByExplosion
//===================================================================
bool CAISystem::IsPointAffectedByExplosion(const Vec3& pos) const
{
	// update existing explosion if overlaps, or add new one
	for (unsigned i = 0, ni = m_explosionSpots.size(); i < ni; ++i)
	{
		float	distSq = Distance::Point_PointSq(m_explosionSpots[i].pos, pos);
		if (distSq < sqr(m_explosionSpots[i].radius))
			return true;
	}
	return false;
}


//===================================================================
// CheckPointsInsidePathObstacles
//===================================================================
unsigned int CAISystem::CheckPointsInsidePathObstacles(IAIObject* requester, const Vec3* points, bool* results, unsigned int n)
{
	CPuppet* puppet = CastToCPuppetSafe(requester);
	if(!puppet)
		return 0;

	const CPathObstacles& obstacles = puppet->GetPathAdjustmentObstacles();

	unsigned int ninside = 0;
	for(unsigned int i = 0; i < n; i++)
	{
		results[i] = obstacles.IsPointInsideObstacles(points[i]);
		if(results[i])
			ninside++;
	}
	return ninside;
}

//===================================================================
// RegisterDamageRegion
//===================================================================
void CAISystem::RegisterDamageRegion(const void *pID, const Sphere &sphere)
{
  if (sphere.radius > 0.0f)
    m_damageRegions[pID] = sphere;
  else
    m_damageRegions.erase(pID);
}


//===================================================================
//
//===================================================================
void CAISystem::TickCountStartStop( bool start )
{
static TickCounter tickCounter(TickCounter("_EXTERNAL_"));
	
	if(start)
		tickCounter.StartCount();
	else
		tickCounter.StopCount();
}

//===================================================================
// ModifyNavCostFactor
//===================================================================
void CAISystem::ModifyNavCostFactor(const char *navModifierName, float factor)
{
  size_t reqNameLen = strlen(navModifierName);

  for (ExtraLinkCostShapeMap::iterator it = m_mapExtraLinkCosts.begin() ; it != m_mapExtraLinkCosts.end() ; ++it)
  {
    const string &name = it->first;
    size_t nameLen = strlen(name.c_str());
    if (nameLen < reqNameLen)
      continue;

    const char *realName = name.c_str() + nameLen - reqNameLen;
    if (strncmp(realName, navModifierName, reqNameLen) == 0)
    {
      SExtraLinkCostShape &shape = it->second;
      shape.costFactor = factor;
      return;
    }
	}
  AIWarning("CAISystem::ModifyNavCostFactor Unable to find extra cost nav modifier called %s", navModifierName);
}

//===================================================================
// RemoveExtraPointsFromShape
//===================================================================
void RemoveExtraPointsFromShape(ShapePointContainer &pts)
{
  // Don't remove a point if the resulting segment would be longer than this
  static float maxSegDistSq = square(10.0f);
  // Don't remove a point(s) if the removed point(s) would be more than this
  // from the new segment. Differentiate between in and out
  static float criticalOutSideDist = 0.01f;
  static float criticalInSideDist = 0.01f;
  // Don't remove points if the angle between the prev/next segments has a 
  // dot-product smaller than this
  static float criticalDot = 0.9f;

  // number of points that will be cut each time (gets zerod after the cut)
  int numCutPoints = -1;

  // it is our current segment start
  for (ShapePointContainer::iterator it = pts.begin() ; it != pts.end() ; ++it)
  {
    const Vec3 &itPos = *it;
    // itSegEnd is our potential segment end. We keep increasing it until cutting an
    // intermediate point would diverge too much. If this happens then we move itSegEnd
    // back one place (i.e. back to the last safe segment end) and cut the intervening 
    // points
    ShapePointContainer::iterator itSegEnd;
    for (itSegEnd = it ; itSegEnd != pts.end() ; ++itSegEnd, ++numCutPoints)
    {
      if (itSegEnd == it)
        continue;

      const Vec3 &itSegEndPos = *itSegEnd;

      float potentialSegmentLenSq = Distance::Point_Point2DSq(itPos, itSegEndPos);
      if (potentialSegmentLenSq > maxSegDistSq)
        goto TryToCut;

      const Lineseg potentialSegment(itPos, itSegEndPos);

      // if the potential segment would result in self-intersection then skip it
      for (ShapePointContainer::iterator itRest = itSegEnd ; itRest != it ; ++itRest)
      {
        if (itRest == pts.end())
          itRest = pts.begin();
        if (itRest == it)
          break;
        if (itRest == itSegEnd)
          continue;

        ShapePointContainer::iterator itRestNext = itRest;
        ++itRestNext;
        if (itRestNext == pts.end())
          itRestNext = pts.begin();
        if (itRestNext == it || itRestNext == itSegEnd)
          continue;

        if (Overlap::Lineseg_Lineseg2D(potentialSegment, Lineseg(*itRest, *itRestNext)))
          goto TryToCut;
      }

      Vec3 potentialSegmentDir = itSegEndPos - itPos;
      potentialSegmentDir.z = 0.0f;
      potentialSegmentDir.NormalizeSafe();
      // assume shape is wound anti-clockwise
      Vec3 potentialSegmentOutsideDir(potentialSegmentDir.y, -potentialSegmentDir.x, 0.0f);

      // itCut is a potential point to cut
      for (ShapePointContainer::iterator itCut = it ; itCut != itSegEnd ; ++itCut)
      {
        if (itCut == it)
          continue;
        const Vec3 &itCutPos = *itCut;
        // check sideways distance
        Vec3 delta = itCutPos - potentialSegment.start;
        float outsideDist = potentialSegmentOutsideDir.Dot(delta - delta.Dot(potentialSegmentDir) * potentialSegmentDir);
        if (outsideDist > criticalOutSideDist)
          goto TryToCut;
        if (outsideDist < -criticalInSideDist)
          goto TryToCut;

        // check dot product
        ShapePointContainer::iterator itCutPrev = itCut; 
        --itCutPrev;
        ShapePointContainer::iterator itCutNext = itCut; 
        ++itCutNext;
        if (itCutNext == itSegEnd)
          continue;
        Vec3 deltaPrev = (itCutPos - *itCutPrev);
        deltaPrev.z = 0.0f;
        deltaPrev.NormalizeSafe();
        Vec3 deltaNext = (*itCutNext - itCutPos);
        deltaNext.z = 0.0f;
        deltaNext.NormalizeSafe();
        float dot = deltaPrev.Dot(deltaNext);
        if (dot < criticalDot)
          goto TryToCut;
      } // iterate over potential points to cut
    } // iterate over segment end points
TryToCut:
    // Either itSegEnd reached the end, or else it has gone just one step too far.
    // Either way we move it back one place and if that's successful then we remove 
    // points between it and the new location.
    if (pts.size() - numCutPoints < 3)
      return;
    if (itSegEnd != it && --itSegEnd != it)
    {
      ShapePointContainer::iterator itNext = it;
      ++itNext;
      if (itNext != itSegEnd && itNext != pts.end())
        pts.erase(itNext, itSegEnd);
    }
    numCutPoints = -1;
  }

}

//===================================================================
// CombineForbiddenAreas
//===================================================================
bool CAISystem::CombineForbiddenAreas(CAIShapeContainer& areasContainer)
{
	typedef std::list<CAIShape*> ShapeList;

  // when we find an area that doesn't intersect anything else we put it in here
	ShapeList areas;
	ShapeList processedAreas;

  // simplify all shapes
	for (unsigned i = 0, ni = areasContainer.GetShapes().size(); i < ni; ++i)
	{
		CAIShape* pShape = areasContainer.GetShapes()[i];
		areasContainer.DetachShape(pShape); // Detach the shape from the container
		RemoveExtraPointsFromShape(pShape->GetPoints());
		pShape->BuildAABB();
		areas.push_back(pShape);
  }
	areasContainer.Clear();

  // remove all the non-overlapping shapes
	for (ShapeList::iterator it = areas.begin(); it != areas.end(); )
	{
		bool overlap = false;
		// Find if the shape overlaps with other shapes.
		for (ShapeList::iterator it2 = areas.begin(); it2 != areas.end(); ++it2)
		{
			if (it == it2) continue; // Skipt self.
      const CAIShape* shapes[2] = { *it, *it2 };
      if (Overlap::Polygon_Polygon2D<ShapePointContainer>(shapes[0]->GetPoints(), shapes[1]->GetPoints(), 
        &shapes[0]->GetAABB(), &shapes[1]->GetAABB()))
			{
				overlap = true;
        break;
			}
    }

    if (!overlap)
    {
			// Move into processedAreas
			processedAreas.push_back(*it);
			it = areas.erase(it);
    }
    else
    {
      ++it;
    }
  }

  unsigned unionCounter = 0;
	I3DEngine *pEngine = m_pSystem->GetI3DEngine();

	while (!areas.empty())
	{
		CAIShape* pCurrentShape = areas.front();
		areas.pop_front();

		// Find the first shape that overlaps the current shape.
		CAIShape* pNextShape = 0;
		for (ShapeList::iterator it = areas.begin(); it != areas.end(); ++it)
		{
      if (Overlap::Polygon_Polygon2D<ShapePointContainer>(pCurrentShape->GetPoints(), (*it)->GetPoints(),
        &pCurrentShape->GetAABB(), &(*it)->GetAABB()))
			{
				pNextShape = *it;
				areas.erase(it);
				break;
			}
		}

		if (!pNextShape)
		{
			// Could not find overlapping shape!
			AIWarning("Forbidden area %s was reported to intersect with another shape, but the intersecting shape was not found! (%d areas left)", pCurrentShape->GetName().c_str(), areas.size());
			processedAreas.push_back(pCurrentShape);

			AABB aabb = pCurrentShape->GetAABB();
			Vec3 center = aabb.GetCenter();
			aabb.Move(-center);
			OBB	obb;
			obb.SetOBBfromAABB(Matrix33::CreateIdentity(), aabb);

			char msg[256];
			_snprintf(msg, 256, "Intersect mismatch\n(%.f, %.f, %.f).", center.x, center.y, center.z);
			m_validationErrorMarkers.push_back(SValidationErrorMarker(msg, center, obb, ColorB(255,0,196)));
		}
		else
		{
			const string* names[2] = { &pCurrentShape->GetName(), &pNextShape->GetName() };
			const CAIShape* shapes[2] = { pCurrentShape, pNextShape };

			AABB originalCombinedAABB(shapes[0]->GetAABB());
			originalCombinedAABB.Add(shapes[1]->GetAABB());

      // this is subtracted from the polygons before combining, then added afterwards to
      // reduce rounding errors during the geometric union
      Vec3 offset = std::accumulate(shapes[0]->GetPoints().begin(), shapes[0]->GetPoints().end(), Vec3(ZERO));
      offset +=std::accumulate(shapes[1]->GetPoints().begin(), shapes[1]->GetPoints().end(), Vec3(ZERO));
      offset /= (shapes[0]->GetPoints().size() + shapes[1]->GetPoints().size());
      offset.z = 0.0f;

      string newName;
      newName.Format("%s+%s", names[0]->c_str(), names[1]->c_str());
      if (newName.length() > 1024)
        newName.Format("CombinedArea%d", forbiddenNameCounter++);

      // combine the shapes
      Polygon2d polygons[2];
      for (unsigned iPoly = 0 ; iPoly < 2 ; ++iPoly)
      {
        const ShapePointContainer& pts = shapes[iPoly]->GetPoints();
//        Vec3 badPt;
//        if (DoesShapeSelfIntersect(shape, badPt))
//          AIWarning("Input polygon self intersects at (%5.2f, %5.2f) %s", badPt.x, badPt.y, names[iPoly]->c_str());

        unsigned nPts = pts.size();
//        for (ListPositions::const_iterator it = shape.begin() ; it != shape.end() ; ++it, ++i)
				for (unsigned k = 0; k < nPts; ++k)
        {
          Vector2d pt(pts[k].x - offset.x, pts[k].y - offset.y);
          polygons[iPoly].AddVertex(pt);
          polygons[iPoly].AddEdge(Polygon2d::Edge((k + nPts - 1) % nPts, k));
        }
        polygons[iPoly].CollapseVertices(0.001f);
        //polygons[iPoly].ComputeBspTree();

//        if (!polygons[iPoly].IsWoundAnticlockwise())
//          AIWarning("Input polygon not wound anti-clockwise %s", names[iPoly]->c_str());
      }

//      polygons[0].Print("PolyP.txt");
//      polygons[1].Print("PolyQ.txt");

      AILogProgress("Running polygon union %d", unionCounter++);

      Polygon2d polyUnion = polygons[0] | polygons[1];

//      if (!polyUnion.IsWoundAnticlockwise())
//        AIWarning("Union polygon not wound anti-clockwise %s", newName.c_str());

      polyUnion.CollapseVertices(0.001f);

//      polyUnion.Print("PolyUnion.txt");

      polyUnion.CalculateContours(true);
      unsigned nContours = polyUnion.GetContourQuantity();
      if (nContours != 1)
      {
        AILogComment("Got %d contours out of the union - using them all but expect problems. From %s", 
          nContours, newName.c_str());
      }
  
      AILogEvent("Finished Running polygon union");

			// Remove the merged shapes.
			delete shapes[0];
			shapes[0] = 0;
			delete shapes[1];
			shapes[1] = 0;

      // it's possible that the union will result in two or more just-touching contours
      // so need to add them all.
			AABB combinedTotalAABB(AABB::RESET);
      for (unsigned iContour = 0 ; iContour < nContours ; ++iContour)
      {
        const Vector2d *pPts;
        unsigned nPts = polyUnion.GetContour(iContour, &pPts); // guaranteed anti-clockwise wound
        if (nPts < 3)
          continue;
        
        ShapePointContainer newPts(nPts);
        for (unsigned iPt = 0; iPt < nPts; ++iPt)
        {
					// Sanity check
					if (pPts[iPt].x < -1000.0f || pPts[iPt].x > 10000.0f || pPts[iPt].y < -1000.0f || pPts[iPt].y > 10000.0f)
					{
						AIWarning("Contour %d/%d has bad point (%f, %f)", iContour+1, nContours, pPts[iPt].x, pPts[iPt].y);
					}

          Vec3 pt(pPts[iPt].x + offset.x, pPts[iPt].y + offset.y, 0.0f);
          pt.z = pEngine->GetTerrainElevation(pt.x, pt.y);
          newPts[iPt] = pt;
					combinedTotalAABB.Add(pt);
        }

        string thisName;
        if (nContours > 1)
          thisName.Format("%s-%d", newName.c_str(), iContour);
        else
          thisName = newName;

				// Make sure the name is unique
				for (ShapeList::iterator itn = areas.begin(); itn != areas.end(); ++itn)
				{
					if ((*itn)->GetName() == thisName)
					{
						thisName.Format("CFA-%d", forbiddenNameCounter++);
						break;
					}
				}
				for (ShapeList::iterator itn = processedAreas.begin(); itn != processedAreas.end(); ++itn)
				{
					if ((*itn)->GetName() == thisName)
					{
						thisName.Format("CFA-%d", forbiddenNameCounter++);
						break;
					}
				}

				RemoveExtraPointsFromShape(newPts);

        Vec3 badPt;
        if (DoesShapeSelfIntersect(newPts, badPt))
        {
          AIWarning("(%5.2f, %5.2f) Combined area self intersects: dropping it (triangulation will not be perfect). Check/modify objects nearby: %s", 
            badPt.x, badPt.y, thisName.c_str());
          continue;
        }

        if (newPts.size() < 3)
          continue;

				CAIShape* pNewShape = new CAIShape;
				pNewShape->SetName(thisName);
				pNewShape->SetPoints(newPts);

        // if this is free of overlap then don't process it any more
        bool isFreeOfOverlap = true;
        for (ShapeList::iterator ito = areas.begin(); ito != areas.end(); ++ito)
        {
					const CAIShape* pTestShape = *ito;
          if (Overlap::Polygon_Polygon2D<ShapePointContainer>(pNewShape->GetPoints(), pTestShape->GetPoints(),
						&pNewShape->GetAABB(), &pTestShape->GetAABB()))
          {
            isFreeOfOverlap = false;
            break;
          }
        }

        if (isFreeOfOverlap)
        {
          processedAreas.push_back(pNewShape);
        }
        else
        {
					areas.push_back(pNewShape);

/*          if (!areas.insert(ShapeMap::iterator::value_type(thisName, thisShape)).second)
          {
            AIError("CAISystem::CombineForbiddenAreas Duplicate auto-forbidden area name after processing %s", 
              thisName.c_str());
            return false;
          }*/
        }

				Vec3 origSize = originalCombinedAABB.GetSize();
				origSize.z = 0;
				Vec3 combinedSize = combinedTotalAABB.GetSize();
				combinedSize.z = 0;

				if (fabsf(origSize.GetLength() - combinedSize.GetLength()) > 0.1f)
				{
					// Size of the bounds are different
					AIWarning("The bounds of the combined forbidden area %s is different than expected from the combined results.", pNewShape->GetName().c_str());

					Vec3 center = originalCombinedAABB.GetCenter();
					originalCombinedAABB.Move(-center);
					OBB	obb;
					obb.SetOBBfromAABB(Matrix33::CreateIdentity(), originalCombinedAABB);

					char msg[256];
					_snprintf(msg, 256, "Combined bounds mismatch\n(%.f, %.f, %.f).", center.x, center.y, center.z);
					m_validationErrorMarkers.push_back(SValidationErrorMarker(msg, center, obb, ColorB(255,0,196)));
				}

			}

    }
  }

	for (ShapeList::iterator it = processedAreas.begin(); it != processedAreas.end(); ++it)
		areasContainer.InsertShape(*it);

	return true;
}

// for point sorting in CalculateForbiddenAreas
static const float vec3SortTol = 0.001f;
static inline bool operator<(const Vec3 &lhs, const Vec3 &rhs)
{
  if (lhs.x < rhs.x - vec3SortTol)
    return true;
  else if (lhs.x > rhs.x + vec3SortTol)
    return false;
  if (lhs.y < rhs.y - vec3SortTol)
    return true;
  else if (lhs.y > rhs.y + vec3SortTol)
    return false;
  return false; // equal
}
static inline bool Vec3Equivalent2D(const Vec3 &lhs, const Vec3 &rhs)
{
	return IsEquivalent2D(lhs, rhs, vec3SortTol);
}

//===================================================================
// CalculateForbiddenAreas
//===================================================================
bool CAISystem::CalculateForbiddenAreas()
{
  float criticalRadius = m_cvRadiusForAutoForbidden->GetFVal();

  m_forbiddenAreas.Copy(m_designerForbiddenAreas);
	forbiddenNameCounter = 0;

  if (ForbiddenAreasOverlap())
  {
    AIError("Designer forbidden areas overlap - aborting triangulation");
    return false;
  }

  if (criticalRadius >= 100.0f)
    return true;

  // get all the objects that need code-generated forbidden areas
  std::vector<Sphere> extraObjects;

	Vec3 min,max;
	float fTSize = (float) m_pSystem->GetI3DEngine()->GetTerrainSize();
	min.Set(0,0,-5000);
	max.Set(fTSize,fTSize,5000.0f);

	IPhysicalEntity** pObstacles;
	int count = m_pWorld->GetEntitiesInBox(min,max,pObstacles,ent_static|ent_ignore_noncolliding);
	
	I3DEngine *pEngine = m_pSystem->GetI3DEngine();

	CAIShapeContainer extraAreas;

  int iEO = 0;
	for (int i = 0 ; i < count ; ++i)
	{
		IPhysicalEntity *pCurrent = pObstacles[i];

    IRenderNode * pRN = (IRenderNode*)pCurrent->GetForeignData(PHYS_FOREIGN_ID_STATIC);
    if (pRN)
    {
      IStatObj *pStatObj = pRN->GetEntityStatObj(0);
      if (pStatObj)
      {
        float vegRad = pStatObj->GetAIVegetationRadius();
        pe_status_pos status;
        status.ipart = 0;
        pCurrent->GetStatus( &status );

        if (!IsPointInTriangulationAreas(status.pos))
          continue;

        if (vegRad > 0.0f)
          vegRad *= status.scale;

        if (vegRad >= criticalRadius)
        {
          // use the convex hull - first get the points
		      Vec3 obstPos = status.pos;
		      Matrix33 obstMat = Matrix33(status.q);
		      obstMat *= status.scale;

          static float criticalMinAlt = 0.5f;
          static float criticalMaxAlt = 1.8f;

  	      static std::vector<Vec3>	vertices;
		      vertices.resize(0);

					// add all parts
					pe_status_nparts statusNParts;
					int nParts = pCurrent->GetStatus(&statusNParts);

					pe_status_pos statusPos;
					pe_params_part paramsPart;
					for (statusPos.ipart = 0, paramsPart.ipart = 0 ; statusPos.ipart < nParts ; ++statusPos.ipart, ++paramsPart.ipart)
					{
						if(!pCurrent->GetParams(&paramsPart) || !pCurrent->GetStatus(&statusPos))
							continue;

						Vec3 partPos = statusPos.pos;
						Matrix33 partMat = Matrix33(statusPos.q);
						partMat *= statusPos.scale;

						IGeometry* geom = statusPos.pGeom;
						const primitives::primitive * prim = geom->GetData();
						int type = geom->GetType();
						switch (type)
						{
						case GEOM_BOX:
							{
								const primitives::box* bbox = static_cast<const primitives::box*>(prim);
								Matrix33 rot = bbox->Basis.T();

								Vec3	boxVerts[8];
								boxVerts[0] = bbox->center + rot * Vec3( -bbox->size.x, -bbox->size.y, -bbox->size.z );
								boxVerts[1] = bbox->center + rot * Vec3( bbox->size.x, -bbox->size.y, -bbox->size.z );
								boxVerts[2] = bbox->center + rot * Vec3( bbox->size.x, bbox->size.y, -bbox->size.z );
								boxVerts[3] = bbox->center + rot * Vec3( -bbox->size.x, bbox->size.y, -bbox->size.z );
								boxVerts[4] = bbox->center + rot * Vec3( -bbox->size.x, -bbox->size.y, bbox->size.z );
								boxVerts[5] = bbox->center + rot * Vec3( bbox->size.x, -bbox->size.y, bbox->size.z );
								boxVerts[6] = bbox->center + rot * Vec3( bbox->size.x, bbox->size.y, bbox->size.z );
								boxVerts[7] = bbox->center + rot * Vec3( -bbox->size.x, bbox->size.y, bbox->size.z );

								for( int j = 0; j < 8; j++ )
									boxVerts[j] = partPos + partMat * boxVerts[j];

								int edges[12][2] = 
								{
									{0, 1},
									{1, 2},
									{2, 3},
									{3, 0},
									{4, 5},
									{5, 6}, 
									{6, 7},
									{7, 4},
									{0, 4},
									{1, 5}, 
									{2, 6},
									{3, 7}
								};

								for (int iEdge = 0 ; iEdge < 12 ; ++iEdge)
								{
									const Vec3 &v0 = boxVerts[edges[iEdge][0]];
									const Vec3 &v1 = boxVerts[edges[iEdge][1]];

//									AddDebugLine(v0, v1, 255,255,255, 30.0f);

									// use the terrain z at the edge mid-point. This means that duplicate edges (from the adjacent triangle)
									// will be eliminated at the end
									Vec3 midPt = 0.5f * (v0 + v1);
									float terrainZ = pEngine->GetTerrainElevation(midPt.x, midPt.y);
									// if within range add v0 and up to 2 clip points. v1 might get added on the next edge
									if (v0.z > terrainZ + criticalMinAlt && v0.z < terrainZ + criticalMaxAlt)
										vertices.push_back(v0);
									const float clipAlts[2] = {terrainZ + criticalMinAlt, terrainZ + criticalMaxAlt};
									for (int iClip = 0 ; iClip < 2 ; ++iClip)
									{
										const float clipAlt = clipAlts[iClip];
										float d0 = v0.z - clipAlt;
										float d1 = v1.z - clipAlt;
										if (d0 * d1 < 0.0f)
										{
											float d = d0 - d1;
											Vec3 clipPos = (v1 * d0 - v0 * d1) / d;
											vertices.push_back(clipPos);
//											AddDebugSphere(clipPos, 0.02f, 255,255,255, 30.0f);
										}
									}

								}
	              
							} // box case
							break;
						case GEOM_TRIMESH:
							{
								const mesh_data * mesh = static_cast<const mesh_data *>(prim);

								int numVerts = mesh->nVertices;
								int numTris = mesh->nTris;

    						static std::vector<Vec3>	worldVertices;
								worldVertices.resize(numVerts);
								for( int j = 0; j < numVerts; j++ )
									worldVertices[j] = partPos + partMat * mesh->pVertices[j];

								for (int j = 0; j < numTris; ++j )
								{
									int indices[3] = {mesh->pIndices[j*3 + 0], mesh->pIndices[j*3 + 1], mesh->pIndices[j*3 + 2]};
									const Vec3 worldV[3] = {worldVertices[indices[0]], worldVertices[indices[1]], worldVertices[indices[2]]};

									for (int iEdge = 0 ; iEdge < 3 ; ++iEdge)
									{
										const Vec3 &v0 = worldV[iEdge];
										const Vec3 &v1 = worldV[(iEdge + 1) % 3];
										// use the terrain z at the edge mid-point. This means that duplicate edges (from the adjacent triangle)
										// will be eliminated at the end
										Vec3 midPt = 0.5f * (v0 + v1);
										float terrainZ = pEngine->GetTerrainElevation(midPt.x, midPt.y);
										// if within range add v0 and up to 2 clip points. v1 might get added on the next edge
										if (v0.z > terrainZ + criticalMinAlt && v0.z < terrainZ + criticalMaxAlt)
											vertices.push_back(v0);
										const float clipAlts[2] = {terrainZ + criticalMinAlt, terrainZ + criticalMaxAlt};
										for (int iClip = 0 ; iClip < 2 ; ++iClip)
										{
											const float clipAlt = clipAlts[iClip];
											float d0 = v0.z - clipAlt;
											float d1 = v1.z - clipAlt;
											if (d0 * d1 < 0.0f)
											{
												float d = d0 - d1;
												Vec3 clipPos = (v1 * d0 - v0 * d1) / d;
												vertices.push_back(clipPos);
											}
										}
									} // loop over edges
								} // loop over triangles
							} // trimesh
							break;
						case GEOM_SPHERE:
							{
								const primitives::sphere* sphere = static_cast<const primitives::sphere*>(prim);
								Vec3 center = partPos + sphere->center;
								float terrainZ = pEngine->GetTerrainElevation(center.x, center.y);
								float top = center.z + sphere->r;
								float bot = center.z - sphere->r;
								if (top > terrainZ && bot < terrainZ + criticalMaxAlt)
								{
									static int nPts = 8;
									vertices.reserve(nPts);
									for (int j = 0 ; j < nPts ; ++j)
									{
										float angle = j * gf_PI2 / nPts;
										Vec3 pos = center + sphere->r * Vec3(cos(angle), sin(angle), 0.0f);
										vertices.push_back(pos);
									}
								}
							}
							break;
						case GEOM_CAPSULE:
						case GEOM_CYLINDER:
							{
								// rely on capsule being the same struct as a cylinder
								Vec3 pts[2];
								const primitives::cylinder *capsule = static_cast<const primitives::cylinder*> (prim);
								pts[0] = partPos + partMat * (capsule->center - capsule->hh * capsule->axis);
								pts[1] = partPos + partMat * (capsule->center + capsule->hh * capsule->axis);
								float r = capsule->r;

								static int nPts = 8;
								vertices.reserve(nPts * 2);
								for (unsigned iC = 0 ; iC != 2 ; ++iC)
								{
									float terrainZ = pEngine->GetTerrainElevation(pts[iC].x, pts[iC].y);
									float top = pts[iC].z + capsule->r;
									float bot = pts[iC].z - capsule->r;
									if (top > terrainZ && bot < terrainZ + criticalMaxAlt)
									{
										for (int j = 0 ; j < nPts ; ++j)
										{
											float angle = j * gf_PI2 / nPts;
											Vec3 pos = pts[iC] + r * Vec3(cos(angle), sin(angle), 0.0f);
											vertices.push_back(pos);
										}
									}
								}
	//              if (!vertices.empty())
	//                AIWarning("cylinder at (%5.2f %5.2f %5.2f)", pts[0].x, pts[0].y, pts[0].z);
							}
							break;
						default:
							AIWarning("CAISystem::CalculateForbiddenAreas unhandled geom type %d", type);
							break;
						} // switch over geometry type
					} // all parts

          if (vertices.empty())
            continue;

          // remove duplicates
          std::sort(vertices.begin(), vertices.end());
          vertices.erase(std::unique(vertices.begin(), vertices.end(), Vec3Equivalent2D), vertices.end());

          if (vertices.size() < 3)
            continue;

          static std::vector<Vec3>	hullVertices;
		      hullVertices.resize(0);

          Vec3 offset = std::accumulate(vertices.begin(), vertices.end(), Vec3(ZERO)) / vertices.size();
          for (std::vector<Vec3>::iterator it = vertices.begin() ; it != vertices.end() ; ++it)
            *it -= offset;

					AABB pointsBounds(AABB::RESET);
					for (std::vector<Vec3>::iterator it = vertices.begin() ; it != vertices.end() ; ++it)
						pointsBounds.Add(*it);

          ConvexHull2D(hullVertices, vertices);
          if (hullVertices.size() < 3)
            continue;

					AABB hullBounds(&vertices[0], vertices.size());

					Vec3 pointsSize = pointsBounds.GetSize();
					pointsSize.z = 0;
					Vec3 hullSize = hullBounds.GetSize();
					hullSize.z = 0;
					if (fabsf(pointsSize.GetLength() - hullSize.GetLength()) > 0.01f)
					{
						// Size of the bounds are different
						AIWarning("The bounds of automatic forbidden area convex hull differs from the input point bounds.");

						Vec3 center = pointsBounds.GetCenter();
						pointsBounds.Move(-center);
						OBB	obb;
						obb.SetOBBfromAABB(Matrix33::CreateIdentity(), pointsBounds);

						char msg[256];
						_snprintf(msg, 256, "New area bounds mismatch\n(%.f, %.f, %.f).", center.x, center.y, center.z);
						m_validationErrorMarkers.push_back(SValidationErrorMarker(msg, center, obb, ColorB(255,0,196)));
					}

          // remove duplicates
          hullVertices.erase(std::unique(hullVertices.begin(), hullVertices.end(), Vec3Equivalent2D), hullVertices.end());
          while (hullVertices.size() > 1 && Vec3Equivalent2D(hullVertices.front(), hullVertices.back()))
            hullVertices.pop_back();

					// Check if the shape self-intersects
					Vec3 badPt;
					if (DoesShapeSelfIntersect(hullVertices, badPt))
					{
						AIWarning("(%5.2f, %5.2f) New automatic forbidden area self intersects: dropping it (triangulation will not be perfect).", badPt.x, badPt.y);
						continue;
					}

					// Check if the shape is degenerate
					float area = CalcPolygonArea<std::vector<Vec3>, float>(hullVertices);
					if (area < 0.01f)
					{
						if (area < 0.0f)
						{
							// Clockwise area
							// Size of the bounds are different
							AIWarning("The area of the automatic forbidden area is negative %f.", area);

							Vec3 center = pointsBounds.GetCenter();
							pointsBounds.Move(-center);
							OBB	obb;
							obb.SetOBBfromAABB(Matrix33::CreateIdentity(), pointsBounds);

							char msg[256];
							_snprintf(msg, 256, "Negative Area\n(%.f, %.f, %.f).", center.x, center.y, center.z);
							m_validationErrorMarkers.push_back(SValidationErrorMarker(msg, center, obb, ColorB(255,0,196)));
						}
						else
						{
							// Degenerate or badly wound shape.
							AILogProgress("Area of automatic forbidden areas less than 0.01 (%.5f), skipping.", area);
						}
						continue;
					}

          if (hullVertices.size() < 3)
            continue;

					for (std::vector<Vec3>::iterator it = hullVertices.begin() ; it != hullVertices.end() ; ++it)
						*it += offset;

          if (!IsShapeCompletelyInForbiddenRegion(hullVertices, false))
          {
            // should already be correctly wound
            string name;
            name.Format("CFA%04d", forbiddenNameCounter++);
            
						CAIShape* pShape = new CAIShape;
						pShape->SetName(name);
						pShape->SetPoints(hullVertices);
						extraAreas.InsertShape(pShape);
						AILogProgress("Created forbidden area %s with %d points", name.c_str(), hullVertices.size());
          }
        } // r >= criticalRadius - i.e. using geometry
        else if (false && vegRad > 0.0f)
        {
          // Use a circle
          pe_status_pos status;
		      pCurrent->GetStatus(&status);

					const unsigned nPts = 8;
  	      ShapePointContainer pts(nPts);
          for (unsigned iPt = 0 ; iPt < nPts ; ++iPt)
          {
            Vec3 pt = status.pos;
            float angle = iPt * gf_PI2 / nPts;
            pt += vegRad * Vec3(cosf(angle), sinf(angle), 0.0f);
            pts[iPt] = pt;
          }

          if (!IsShapeCompletelyInForbiddenRegion(pts, false))
          {
            string name;
            name.Format("CFA%04d", forbiddenNameCounter++);

						CAIShape* pShape = new CAIShape;
						pShape->SetName(name);
						pShape->SetPoints(pts);
						extraAreas.InsertShape(pShape);
						AILogProgress("Created forbidden area %s with %d points", name.c_str(), pts.size());
          }
        }
        else
        {
          // this object radius will be applied only to the links
        }
      } // stat obj
    } // render node
	} // loop over entities

	AILogProgress("Combining %d extra areas", extraAreas.GetShapes().size());
  if (!CombineForbiddenAreas(extraAreas))
  {
    m_forbiddenAreas.Copy(extraAreas);
    return false;
  }

	m_forbiddenAreas.Insert(extraAreas);

	AILogProgress("Combining %d forbidden areas", m_forbiddenAreas.GetShapes().size());
  if (!CombineForbiddenAreas(m_forbiddenAreas))
    return false;

  return true;
}

//===================================================================
// GetObstaclePositionsInRadius
//===================================================================
void CAISystem::GetObstaclePositionsInRadius(const Vec3& pos, float radius, float minHeight, int flags,
																						 std::vector<std::pair<Vec3, float> >& positions)
{
	static std::vector<int> checkedObstacleIndices;
	checkedObstacleIndices.clear();

	CVertexList& vertList = GetAISystem()->m_VertexList;
	CGraph* pHideGraph = (CGraph*)GetAISystem()->GetHideGraph();
	CAllNodesContainer& allNodes = pHideGraph->GetAllNodes();
	CGraphNodeManager& nodeManager = pHideGraph->GetNodeManager();

	typedef std::vector< std::pair<float, unsigned> > DistNodeVec;
	static DistNodeVec nodes;
	nodes.clear();

	bool covers = (flags & OBSTACLES_COVER) != 0;
	bool softCovers = (flags & OBSTACLES_SOFT_COVER) != 0;

	allNodes.GetAllNodesWithinRange(nodes, pos, radius, IAISystem::NAV_TRIANGULAR);

	for (unsigned i = 0, ni = nodes.size(); i < ni; ++i)
	{
		const GraphNode* pNode = nodeManager.GetNode(nodes[i].second);
		const STriangularNavData* pData = pNode->GetTriangularNavData();

		const ObstacleIndexVector::const_iterator vertEnd = pData->vertices.end();
		for (ObstacleIndexVector::const_iterator vertIt = pData->vertices.begin() ; vertIt != vertEnd ; ++vertIt)
		{
			int v = *vertIt;
			const ObstacleData& od =  vertList.GetVertex(v);
			if ((softCovers && !od.bCollidable) || (covers && od.bCollidable))
				if (od.GetApproxHeight() > minHeight && od.fApproxRadius > 0.0f)
					checkedObstacleIndices.push_back(v);
		}
	}

	// This method seems to be faster than using set to hold the unique vertices.
	std::sort(checkedObstacleIndices.begin(), checkedObstacleIndices.end());

	int prev = -1;
	for (unsigned i = 0, ni = checkedObstacleIndices.size(); i < ni; ++i)
	{
		int v = checkedObstacleIndices[i];
		if (v != prev)
		{
			const ObstacleData& od =  vertList.GetVertex(v);
			if ((softCovers && !od.bCollidable) || (covers && od.bCollidable))
				if (od.GetApproxHeight() > minHeight && od.fApproxRadius > 0.0f)
					positions.push_back(std::make_pair(od.vPos, od.fApproxRadius));
		}
		prev = v;
	}
}

//===================================================================
// GetHidespotsInRange
//===================================================================
MultimapRangeHideSpots &CAISystem::GetHideSpotsInRange(MultimapRangeHideSpots &hidespots, 
																											 MapConstNodesDistance &traversedNodes,
																											 const Vec3 & startPos, float maxDist, 
																											 IAISystem::tNavCapMask navCapMask, float passRadius, bool skipNavigationTest,
																											 IEntity* pSmartObjectUserEntity,
																											 unsigned lastNavNodeIndex, unsigned lastHideNodeIndex,
																											 const class CAIObject *pRequester)
{
	const GraphNode *pLastNavNode = m_pGraph->GetNodeManager().GetNode(lastNavNodeIndex);
	const GraphNode *pLastHideNode = m_pHideGraph->GetNodeManager().GetNode(lastHideNodeIndex);

	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );
	hidespots.clear();

	float maxDistSq = square(maxDist);

	if (!skipNavigationTest)
	{
		m_pGraph->GetNodesInRange(traversedNodes, startPos, maxDist, navCapMask, passRadius, lastNavNodeIndex, pRequester);
		if (traversedNodes.empty())
			return hidespots;
	}

	// waypoint
	if (navCapMask & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE))
	{
		if (skipNavigationTest)
		{
			AIWarning("Not implemented skipNavigationTest for waypoint");
		}
		else
		{
			const MapConstNodesDistance::const_iterator itEnd = traversedNodes.end();
			for (MapConstNodesDistance::const_iterator it = traversedNodes.begin() ; it != itEnd ; ++it)
			{
				const GraphNode *pNode = it->first;
				if (pNode->navType & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE))
				{
					float distance = it->second;
					const SWaypointNavData *pData = pNode->GetWaypointNavData();
					if (pData->type != WNT_HIDE)
						continue;
					MultimapRangeHideSpots::iterator resultIt = hidespots.insert(MultimapRangeHideSpots::value_type(distance, 
						SHideSpot(SHideSpot::HST_WAYPOINT, pNode->GetPos(), pData->dir)));
					resultIt->second.pNavNode = pNode;
				}
			}
		}
	}  

	// triangular
	if (navCapMask & IAISystem::NAV_TRIANGULAR)
	{

		MapConstNodesDistance traversedHideNodes;
		VectorSet<int> checkedObstacleIndices;
		m_pHideGraph->GetNodesInRange(traversedHideNodes, startPos, maxDist + 10, IAISystem::NAV_TRIANGULAR, 0.0f, lastHideNodeIndex, 0);
		const MapConstNodesDistance::const_iterator itEnd = traversedHideNodes.end();

		for (MapConstNodesDistance::const_iterator it = traversedHideNodes.begin() ; it != itEnd ; ++it)
		{
			const GraphNode *pHideNode = it->first;
			if (pHideNode->navType == IAISystem::NAV_TRIANGULAR)
			{
				float distance = it->second;
				const STriangularNavData *pData = pHideNode->GetTriangularNavData();

				const ObstacleIndexVector::const_iterator vertEnd = pData->vertices.end();
				for (ObstacleIndexVector::const_iterator vertIt = pData->vertices.begin() ; vertIt != vertEnd ; ++vertIt)
				{
					int vertIndex = *vertIt;
					if (!checkedObstacleIndices.insert(vertIndex).second)
						continue;

					const ObstacleData &od = m_VertexList.GetVertex(vertIndex);
					float distSq = Distance::Point_Point2DSq(od.vPos, startPos);
					if (distSq > maxDistSq)
						continue;

					const std::vector<const GraphNode *> &navNodes = od.GetNavNodes();
					const std::vector<const GraphNode *>::const_iterator nodeEnd = navNodes.end();
					for (std::vector<const GraphNode *>::const_iterator nodeIt = navNodes.begin() ; nodeIt != nodeEnd ; ++nodeIt)
					{
						const GraphNode *pNode = *nodeIt;
						if (!skipNavigationTest && traversedNodes.find(pNode) == traversedNodes.end())
							continue;

						MultimapRangeHideSpots::iterator resultIt = hidespots.insert(MultimapRangeHideSpots::value_type(distance,
							SHideSpot(SHideSpot::HST_TRIANGULAR, od.vPos, od.vDir)));
						resultIt->second.pNavNodes = &od.GetNavNodes();
						resultIt->second.pObstacle = &od;
						break;
					}
				}
			}
		}
	}

	// hide anchors
	{
		int anchorTypes[2] = {AIANCHOR_COMBAT_HIDESPOT, AIANCHOR_COMBAT_HIDESPOT_SECONDARY};
		for (int at = 0 ; at < 2 ; ++at)
		{
			const AIObjects::const_iterator oiEnd = m_Objects.end();
			for (AIObjects::const_iterator oi = m_Objects.find(anchorTypes[at]) ; oi != oiEnd ; ++oi)
			{
				CAIObject* object = oi->second;
				if (object->GetType() != anchorTypes[at])
					break;
				if (!object->IsEnabled())
					continue;
				float distanceSq = Distance::Point_PointSq(object->GetPos(), startPos);
				if (distanceSq > maxDistSq)
					continue;
				const GraphNode *pNode = object->GetAnchorNavNode();
				if (!pNode)
					continue;
				MapConstNodesDistance::const_iterator it;
				if (!skipNavigationTest)
					it = traversedNodes.find(pNode);
				if (skipNavigationTest || traversedNodes.end() != it)
				{
					float distance = skipNavigationTest ? Distance::Point_Point(object->GetPos(), startPos) : it->second;
					MultimapRangeHideSpots::iterator resultIt = hidespots.insert(MultimapRangeHideSpots::value_type(distance,
						SHideSpot(SHideSpot::HST_ANCHOR, object->GetPos(), object->GetMoveDir())));
					resultIt->second.pNavNode = pNode;
					resultIt->second.pAnchorObject = object;
				}
			}
		}
	}

	// Dynamic hidespots.
	{
		typedef std::vector<SDynamicObjectHideSpot> TDynamicHideSpots;
		static TDynamicHideSpots dynamicHideSpots;
		m_dynHideObjectManager.GetHidePositionsWithinRange(dynamicHideSpots, startPos, maxDist, navCapMask, passRadius, lastNavNodeIndex);

		const TDynamicHideSpots::const_iterator diEnd = dynamicHideSpots.end();
		for (TDynamicHideSpots::const_iterator di = dynamicHideSpots.begin() ; di != diEnd ; ++di)
		{
			const SDynamicObjectHideSpot& hs = *di;
			GraphNode *pNode = m_pGraph->GetNodeManager().GetNode(hs.nodeIndex);
			if (!pNode)
				continue;
			MapConstNodesDistance::const_iterator it;
			if (!skipNavigationTest)
				it = traversedNodes.find(pNode);
			if (skipNavigationTest || it != traversedNodes.end())
			{
				float distance = skipNavigationTest ? Distance::Point_Point(hs.pos, startPos) : it->second;
				MultimapRangeHideSpots::iterator resultIt = hidespots.insert(MultimapRangeHideSpots::value_type(distance,
					SHideSpot(SHideSpot::HST_DYNAMIC, hs.pos, hs.dir)));
				resultIt->second.pNavNode = pNode;
				resultIt->second.entityId = hs.entityId;
			}
		}
	}

	// smart objects
	if (pSmartObjectUserEntity)
	{
		QueryEventMap query;
		IEntity* pObjectEntity = NULL;
		GetAISystem()->GetSmartObjects()->TriggerEvent( "Hide", pSmartObjectUserEntity, pObjectEntity, &query );

		const QueryEventMap::const_iterator itEnd = query.end();
		for (QueryEventMap::const_iterator it = query.begin() ; it != itEnd ; ++it)
		{
			const CQueryEvent* pQueryEvent = &it->second;

			Vec3 pos;
			if (pQueryEvent->pRule->pObjectHelper)
				pos = pQueryEvent->pObject->GetHelperPos(pQueryEvent->pRule->pObjectHelper);
			else
				pos = pQueryEvent->pObject->GetPos();
			Vec3 dir = pQueryEvent->pObject->GetOrientation(pQueryEvent->pRule->pObjectHelper);

			unsigned nodeIndices[2];
			unsigned nNavNodes = pQueryEvent->pObject->GetNavNodes(nodeIndices, pQueryEvent->pRule->pObjectHelper);
			for (unsigned iNavNode = 0 ; iNavNode < nNavNodes ; ++iNavNode)
			{
				const GraphNode *pNavNode = m_pGraph->GetNodeManager().GetNode(nodeIndices[iNavNode]);
				AIAssert(pNavNode);
				MapConstNodesDistance::const_iterator dit(traversedNodes.end());
				if (!skipNavigationTest )
					dit = traversedNodes.find(pNavNode);
				if (skipNavigationTest || dit != traversedNodes.end())
				{
					float distance = skipNavigationTest ? Distance::Point_Point(pos, startPos) : dit->second;
					MultimapRangeHideSpots::iterator resultIt = hidespots.insert(MultimapRangeHideSpots::value_type(distance,
						SHideSpot(SHideSpot::HST_SMARTOBJECT, pos, dir)));
					resultIt->second.pNavNode = pNavNode;
					resultIt->second.SOQueryEvent = *pQueryEvent;
				}
			}
		}
	}

	// Volume hidespots
	if (navCapMask & IAISystem::NAV_VOLUME)
	{
		// well.. it could be done but it's not used?
		/*
		static std::vector<SVolumeHideSpot> hidePositions;
		hidePositions.resize(0);
		m_pVolumeNavRegion->GetHideSpotsWithinRadius(startPos, maxDist, SVolumeHideFunctor(hidePositions));
		*/
	}

	return hidespots;
}

//===================================================================
// AdjustDirectionalCoverPosition
//===================================================================
void CAISystem::AdjustDirectionalCoverPosition(Vec3& pos, const Vec3& dir, float agentRadius, float testHeight)
{
	Vec3	floorPos(pos);
	// Add fudge to the initial position in case the point is very near to ground.
	GetFloorPos(floorPos, pos + Vec3(0,0,testHeight/2), walkabilityFloorUpDist, walkabilityFloorDownDist, walkabilityDownRadius, AICE_ALL);
	floorPos.z += testHeight;
	Vec3 hitPos;
	const float	distToWall = COVER_CLEARANCE + agentRadius;
	float	hitDist = distToWall;
	if(IntersectSweptSphere(&hitPos, hitDist, Lineseg(floorPos, floorPos + dir * distToWall), 0.15f, AICE_ALL))
		pos = floorPos + dir * (hitDist - distToWall);
	else
		pos = floorPos;
}

//===================================================================
// AdjustOmniDirectionalCoverPosition
// if hideBehind false - move point to left/right side of cover, to locate spot by cover, but not hiden
//===================================================================
void CAISystem::AdjustOmniDirectionalCoverPosition(Vec3& pos, Vec3& dir, float hideRadius, float agentRadius, const Vec3& hideFrom, const bool hideBehind)
{
	dir = hideFrom - pos;
	if(!hideBehind)
	{
		// dir is left/right offset when not hiding behind cover
		float tmp(dir.x);
		dir.x = -dir.y;
		dir.y = tmp;
		if(ai_rand()%100 < 50)
			dir = -dir;
	}
	dir.z = 0;
	dir.NormalizeSafe();
	pos -= dir * (max(hideRadius, 0.0f) + agentRadius + COVER_CLEARANCE);
}

//===================================================================
// TempHideSpot
// Structure to hold hide points.
// Includes distance to sort the points and both the found hide pos and the object pos/dir.
//===================================================================
struct TempHideSpot
{
	TempHideSpot(const Vec3& pos, const Vec3& objPos, const Vec3& objDir, float rad, float dist, bool collidable, bool directional) :
		pos(pos), objPos(objPos), objDir(objDir), rad(rad), dist(dist), collidable(collidable), directional(directional) {}
	bool	operator<(const TempHideSpot& rhs) const { return dist < rhs.dist; }
	Vec3	pos, objPos, objDir;
	float	rad;
	float	dist;
	bool	collidable;
	bool	directional;
};

//===================================================================
// GetHideSpotsInRange
//===================================================================
unsigned int CAISystem::GetHideSpotsInRange(IAIObject* requester, const Vec3& reqPos,
																				 const Vec3& hideFrom, float minRange, float maxRange, bool collidableOnly, bool validatedOnly,
																				 unsigned int maxPts, Vec3* coverPos, Vec3* coverObjPos, Vec3* coverObjDir, float* coverRad, bool* coverCollidable)
{
	CPuppet* puppet = CastToCPuppetSafe(requester);
//	if (!puppet)
//		return 0;

	IAISystem::tNavCapMask navCapMask = IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_SMARTOBJECT;
	float	passRadius = 0.5f;
	IEntity* pRequesterEnt = 0;
	unsigned lastNavNodeIndex = 0;
	unsigned lastHideNodeIndex = 0;

	if (puppet)
	{
		navCapMask = puppet->GetMovementAbility().pathfindingProperties.navCapMask;
		passRadius = puppet->GetParameters().m_fPassRadius;
		pRequesterEnt = puppet->GetEntity();
		lastNavNodeIndex = puppet->m_lastNavNodeIndex;
		lastHideNodeIndex = puppet->m_lastHideNodeIndex;
	}

	MultimapRangeHideSpots hidespots;
	MapConstNodesDistance traversedNodes;

	GetHideSpotsInRange(hidespots, traversedNodes, reqPos, maxRange, 
		navCapMask, passRadius, false, pRequesterEnt,
		lastNavNodeIndex, lastHideNodeIndex, (CAIObject*)requester);

	// Only update the obstacles if validation is requested.
	const CPathObstacles* obstacles = 0;
	
	if (puppet)
		obstacles = &puppet->GetPathAdjustmentObstacles(validatedOnly);

	float	minRangeSq = sqr(minRange);
	float	maxRangeSq = sqr(maxRange);

	std::vector<TempHideSpot>	spots;
	spots.reserve(hidespots.size());

	const MultimapRangeHideSpots::iterator liEnd = hidespots.end();
	for (MultimapRangeHideSpots::iterator li = hidespots.begin(); li != liEnd; ++li)
	{
		const SHideSpot &hs = li->second;

		if (hs.type == SHideSpot::HST_SMARTOBJECT)
			continue;

		bool	collidable(true);
		float	rad = 0.0f;

		if (hs.pObstacle && !hs.pObstacle->bCollidable)
			collidable = false;
		if (hs.pAnchorObject && hs.pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT_SECONDARY)
		{
			collidable = false;
			rad = 0.2f;
		}

		if (collidableOnly && !collidable)
			continue;

		if (hs.pObstacle)
			rad = hs.pObstacle->fApproxRadius;

		float dist = li->first;
		float distSq = sqr(dist);

		Vec3 pos = hs.pos;
		Vec3 dir = hs.dir;
		const Vec3& objPos(hs.pos);

		if (hs.pObstacle || (hs.pAnchorObject && hs.pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT_SECONDARY))
			AdjustOmniDirectionalCoverPosition(pos, dir, max(rad, 0.0f), passRadius, hideFrom);

		float d = Distance::Point_PointSq(pos, reqPos);

		if (d >= minRangeSq && d <= maxRangeSq)
			spots.push_back(TempHideSpot(pos, objPos, dir, rad, d, collidable,
				hs.type == SHideSpot::HST_ANCHOR || hs.type == SHideSpot::HST_WAYPOINT || hs.type == SHideSpot::HST_DYNAMIC));
	}


	// Output best points.
	std::sort(spots.begin(), spots.end());

	unsigned int npts = 0;

	for (unsigned i = 0, n = spots.size(); i < n; ++i)
	{
		TempHideSpot&	spot = spots[i];
		// Check if the point is inside another cover object and skip if it is.
		bool embedded = false;
		for (unsigned j = 0, m = spots.size(); j < m; ++j)
		{
			TempHideSpot&	spotj = spots[j];
			if (i == j || !spotj.collidable || spotj.rad < 0.01f) continue;
			if (Distance::Point_Point2DSq(spot.pos, spotj.pos) < sqr(spotj.rad + passRadius))
			{
				embedded = true;
				break;
			}
		}
		if (embedded)
			continue;

		// Discard points which do not offer cover.
		if (validatedOnly)
		{
			if (spot.directional)
			{
				Vec3 dirHideToEnemy = hideFrom - spot.pos;
				dirHideToEnemy.NormalizeSafe();
				if (dirHideToEnemy.Dot(spot.objDir) < HIDESPOT_COVERAGE_ANGLE_COS)
					continue;
			}

			int nBuildingID;
			IVisArea* pArea;
			IAISystem::ENavigationType navType = CheckNavigationType(spot.pos, nBuildingID, pArea, navCapMask);
			if (navType == IAISystem::NAV_TRIANGULAR)
			{
				// Discard points inside forbidden areas
				if (IsPointForbidden(spot.pos, passRadius, 0, 0))
					continue;
				// Discard points inside dynamic obstacles
				if (obstacles && obstacles->IsPointInsideObstacles(spot.pos))
					continue;
			}
		}

		if (coverPos) coverPos[npts] = spot.pos;
		if (coverObjPos) coverObjPos[npts] = spot.objPos;
		if (coverObjDir) coverObjDir[npts] = spot.objDir;
		if (coverRad) coverRad[npts] = spot.rad;
		if (coverCollidable) coverCollidable[npts] = spot.collidable;
		++npts;

		if (npts >= maxPts) break;
	}

	return npts;
}


//===================================================================
// GetVisPerceptionDistScale
//===================================================================
void CAISystem::SetPerceptionDistLookUp( float* pLookUpTable, int tableSize )
{
	AILinearLUT& lut(m_VisDistLookUp);
	lut.Set(pLookUpTable, tableSize);
}


//===================================================================
// GetVisPerceptionDistScale
//===================================================================
float CAISystem::GetVisPerceptionDistScale( float distRatio )
{
  AILinearLUT& lut(m_VisDistLookUp);

  // Make sure the dist ratio is in required range [0..1]
  distRatio = clamp(distRatio, 0.0f, 1.0f);

  // If no table is setup, use simple quadratic fall off function.
  if(lut.GetSize() < 2)
    return sqr(1.0f - distRatio);
  return lut.GetValue(distRatio) / 100.0f;
}

//===================================================================
// ClearForbiddenQuadTrees
//===================================================================
void CAISystem::ClearForbiddenQuadTrees()
{
	m_forbiddenAreas.ClearQuadTree();
	m_forbiddenBoundaries.ClearQuadTree();
	m_designerForbiddenAreas.ClearQuadTree();
}

//===================================================================
// RebuildForbiddenQuadTrees
//===================================================================
void CAISystem::RebuildForbiddenQuadTrees()
{
  ClearForbiddenQuadTrees();

	m_forbiddenAreas.BuildBins();
	m_forbiddenBoundaries.BuildBins();
	m_designerForbiddenAreas.BuildBins();

	if (useForbiddenQuadTree)
	{
		m_forbiddenAreas.BuildQuadTree(8, 20.0f);
		m_forbiddenBoundaries.BuildQuadTree(8, 20.0f);
		m_designerForbiddenAreas.BuildQuadTree(8, 20.0f);
	}
}

//===================================================================
// DebugReportHitDamage
//===================================================================
void CAISystem::DebugReportHitDamage(IEntity* pVictim, IEntity* pShooter, float damage, const char* material)
{
	if (!pVictim)
		return;

	if (GetPlayer() && pVictim->GetId() == GetPlayer()->GetEntityID())
	{
		if (pShooter)
			m_DEBUG_playerShots.push_back(SDebugPlayerDamage(DBG_PLAYER_HIT, pShooter->GetName(), damage, material, GetFrameStartTime().GetSeconds()));
		else
			m_DEBUG_playerShots.push_back(SDebugPlayerDamage(DBG_PLAYER_HIT, "<Unknown>", damage, material, GetFrameStartTime().GetSeconds()));
	}

	char	msg[64];
	_snprintf(msg, 64, "%.1f (%s)", damage, material);
	IAIRecordable::RecorderEventData recorderEventData(msg);

	if (CPuppet* pPuppet = CastToCPuppetSafe(pVictim->GetAI()))
		pPuppet->RecordEvent(IAIRecordable::E_HIT_DAMAGE, &recorderEventData);
	else if (CAIPlayer* pPlayer = CastToCAIPlayerSafe(pVictim->GetAI()))
		pPlayer->RecordEvent(IAIRecordable::E_HIT_DAMAGE, &recorderEventData);
}

//===================================================================
// DebugReportDeath
//===================================================================
void CAISystem::DebugReportDeath(IAIObject* pVictim)
{
	if (!pVictim) return;

	if(CPuppet* pPuppet = pVictim->CastToCPuppet())
	{
		IAIRecordable::RecorderEventData recorderEventData("Death");
		pPuppet->RecordEvent(IAIRecordable::E_DEATH, &recorderEventData);
	}
	else if(CAIPlayer* pPlayer = pVictim->CastToCAIPlayer())
	{
		pPlayer->IncDeathCount();
		char	msg[32];
		_snprintf(msg, 32, "Death %d", pPlayer->GetDeathCount());
		IAIRecordable::RecorderEventData recorderEventData(msg);
		pPlayer->RecordEvent(IAIRecordable::E_DEATH, &recorderEventData);
	}
}

//===================================================================
// WouldHumanBeVisible
//===================================================================
bool CAISystem::WouldHumanBeVisible(const Vec3 &footPos, bool fullCheck) const
{
  int ignore = m_cvIgnoreVisibilityChecks->GetIVal();
  if (ignore)
    return false;

  static float radius = 0.5f;
  static float height = 1.8f;

  const CCamera& cam = GetISystem()->GetViewCamera();
  AABB aabb(AABB::RESET);
  aabb.Add(footPos + Vec3(0, 0, radius), radius);
  aabb.Add(footPos + Vec3(0, 0, height - radius), radius);

  if (!cam.IsAABBVisible_F(aabb))
    return false;

  // todo Danny do fullCheck
  return true;
}

//===================================================================
// Hostility restriction
//===================================================================
void CAISystem::SetupHostilityRange(IEntity* pAffected, int species, float min_range, float max_range, float degen, float increase, float cooldown)
{
	CAIActor* pAIActor = CastToCAIActorSafe(pAffected->GetAI());
	if( pAIActor )
	{
		pAIActor->SetupHostilityRange(species, min_range, max_range, degen, increase, cooldown);
	}
}

void CAISystem::AggressionImpact(IEntity* pAttacker, IEntity* pTarget)
{
	CAIActor* pAIActor = CastToCAIActorSafe(pAttacker->GetAI());
	CAIActor* pTargetActor = CastToCAIActorSafe(pTarget->GetAI());
	if( pAIActor && pTargetActor)
	{
		float dst=pAIActor->GetPos().GetSquaredDistance(pTargetActor->GetPos());
		pAIActor->IncreaseHostilityRangeSqr(pTargetActor->m_Parameters.m_nSpecies, dst);
		pTargetActor->IncreaseHostilityRangeSqr(pAIActor->m_Parameters.m_nSpecies, dst);
	}
}

void CAISystem::IgnoreSpecies(IEntity* pAffected, int species, bool ignore)
{
	CAIActor* pAIActor = CastToCAIActorSafe(pAffected->GetAI());
	if( pAIActor )
	{
		pAIActor->IgnoreSpecies(species, ignore);
	}
}

//===================================================================
// AddObstructSphere
//===================================================================
void CAISystem::AddObstructSphere( const Vec3& pos, float radius, float lifeTime, float fadeTime)
{
	m_ObstructSpheres.push_back(SObstrSphere(pos, radius, lifeTime, fadeTime));
}

//===================================================================
// SObstrSphere::Update
//===================================================================
bool CAISystem::SObstrSphere::Update(float	dt)
{
//	GetAISystem()->GetFrameDeltaTime();	
	curTime += dt;
	if(fadeTime<=0.001f)
	{
		curRadius = radius;
		return curTime<lifeTime;
	}

	if(curTime<fadeTime)
		curRadius = radius*(curTime/fadeTime);
	else if(curTime>lifeTime-fadeTime)
	{
		curRadius = radius*(1.f-(curTime-(lifeTime-fadeTime))/fadeTime);
		if(curRadius <=0.001f || curTime>=lifeTime)
			return false;
	}
	else
		curRadius = radius;
	return true;
}

//===================================================================
// SObstrSphere::IsObstructing
//===================================================================
bool CAISystem::SObstrSphere::IsObstructing(const Vec3&p1,const Vec3&p2)
{
float	radSqr(curRadius*curRadius);
	// at least one of the points is inside sphere
	if((p1-pos).len2()<radSqr || (p2-pos).len2()<radSqr)
		return true;

	Vec3 outp, outp2;
	return Intersect::Lineseg_Sphere(Lineseg(p1, p2), Sphere(pos, curRadius),outp, outp2)!=0;

}

//===================================================================
// ValidateSmartObjectArea
//===================================================================
bool CAISystem::ValidateSmartObjectArea(const SSOTemplateArea &templateArea, 
                                        const SSOUser &user, EAICollisionEntities collEntities, Vec3 &groundPos)
{
  groundPos = templateArea.pt;

  static float upDist = 1.0f;
  static float downDist = 1.0f;
  
  if (templateArea.projectOnGround && 
      !GetFloorPos(groundPos, templateArea.pt, upDist, downDist, walkabilityDownRadius, collEntities))
    return false;

  IPhysicalEntity* pSkipEntity = user.entity ? user.entity->GetPhysics() : NULL;

  Lineseg seg(groundPos + Vec3(0.0f, 0.0f, user.groundOffset), groundPos + Vec3(0.0f, 0.0f, user.groundOffset + user.height));
  if (OverlapCylinder(seg, user.radius + templateArea.radius, collEntities, pSkipEntity,geom_colltype_player))
    return false;

  return true;
}


// Static formation point
struct SAIStatFormPt
{
	enum ETested { NOT_TESTED, VALID, INVALID };
	SAIStatFormPt(const Vec3& p, float w, IAISystem::ENavigationType navType) : pos(p), w(w), d(0), vis(0), test(NOT_TESTED), navType(navType) {}
	Vec3	pos;
	float	d;
	float	w;
	unsigned char vis;
	ETested test;
	IAISystem::ENavigationType navType;
};

// Static formation histogram island
struct SAIStatFormIsland
{
	SAIStatFormIsland(int a, int b, float v) : a(a), b(b), v(v) {}
	inline bool operator<(const SAIStatFormIsland& rhs) const { return v < rhs.v; }
	inline bool operator>(const SAIStatFormIsland& rhs) const { return v > rhs.v; }
	int a, b;
	float v;
};

// Static formation unit
struct SAIStatFormUnit
{
	SAIStatFormUnit(const Vec3& pos, unsigned i) : pos(pos), i(i), targetPos(0,0,0), d(0) {}
	inline bool operator<(const SAIStatFormUnit& rhs) const { return d < rhs.d; }
	inline bool operator>(const SAIStatFormUnit& rhs) const { return d > rhs.d; }
	Vec3	pos;
	Vec3	targetPos;
	float	d;
	unsigned i;
};

//===================================================================
// GetStaticFormation
//===================================================================
bool CAISystem::CreateStaticFormation(SAIStaticFormation::EType type, SAIStaticFormation::ELocation location,
																	const std::vector<CPuppet*>& group, const Vec3& targetPos, const Vec3* pPivotPos,
																	float offsetTowardsTarget, float minDistanceToTarget, float searchRange, float spacing,
																	std::vector<std::pair<Vec3, float> >& otherGroups, CAIRadialOcclusionRaycast* pOcclusion,
																	SAIStaticFormation& outFormation)
{
	FUNCTION_PROFILER(m_pSystem,PROFILE_AI);

	if (group.empty())
	{
		outFormation.error = SAIStaticFormation::ERR_GROUP_EMPTY;
		return false;
	}
	
	Vec3	groupPos(0,0,0);
	std::vector<SAIStatFormUnit>	units;
	IAISystem::tNavCapMask navCapMask = NAVMASK_ALL;
	for (unsigned i = 0; i < group.size(); ++i)
	{
		units.push_back(SAIStatFormUnit(group[i]->GetPos(), i));
		const Vec3& pos = group[i]->GetPos();
		groupPos += pos;
		navCapMask &= group[i]->GetMovementAbility().pathfindingProperties.navCapMask;
	}
	groupPos /= (float)units.size();

	if (pPivotPos)
		groupPos = *pPivotPos;

/*	CPuppet* mostCenter = 0;
	float mostCenterDist = FLT_MAX;
	for (unsigned i = 0; i < group.size(); ++i)
	{
		float d = Distance::Point_PointSq(group[i]->GetPos(), groupPos);
		if (d < mostCenterDist)
		{
			mostCenterDist = d;
			mostCenter = group[i];
		}
	}*/

	Vec3	formationForw = targetPos - groupPos;
	formationForw.z = 0;
	formationForw.NormalizeSafe();
	Vec3	formationRight(formationForw.y, -formationForw.x, 0);


	CPuppet* mostFront = 0;
	float mostFrontDist = -FLT_MAX;
	for (unsigned i = 0; i < group.size(); ++i)
	{
//		float d = Distance::Point_PointSq(group[i]->GetPos(), groupPos);
		float d = formationForw.Dot(group[i]->GetPos() - groupPos);
		if (d > mostFrontDist)
		{
			mostFrontDist = d;
			mostFront = group[i];
		}
	}
	if (!mostFront)
	{
		outFormation.error = SAIStaticFormation::ERR_GROUP_EMPTY;
		return false;
	}

//	groupPos += mostFrontDist * formationForw;

	outFormation.debugForw = formationForw;
	outFormation.debugRight = formationRight;

	std::vector<SAIStatFormPt>	points;

	MultimapRangeHideSpots hidespots;
	MapConstNodesDistance traversedNodes;

	GetHideSpotsInRange(hidespots, traversedNodes, mostFront->GetPos(), searchRange, navCapMask, 0.0f, false);
	outFormation.debugSearchPos = mostFront->GetPos();

	if (hidespots.empty() && traversedNodes.empty())
	{
		outFormation.error = SAIStaticFormation::ERR_NO_HIDESPOTS;
		return false;
	}

	CAIRadialOcclusion softOcclusion;
	softOcclusion.Reset(targetPos, Distance::Point_Point(targetPos, mostFront->GetPos()) + searchRange*2);

	const float offsetFromCover = 2.0f;

	// Build list of obstacle cover segments.
	for (MultimapRangeHideSpots::iterator it = hidespots.begin(); it != hidespots.end(); ++it)
	{
		float distance = it->first;
		const SHideSpot &hs = it->second;


		IAISystem::ENavigationType navType = IAISystem::NAV_UNSET;
		if (hs.type == SHideSpot::HST_TRIANGULAR)
			navType = IAISystem::NAV_TRIANGULAR;
		else if (hs.type == SHideSpot::HST_WAYPOINT)
			navType = IAISystem::NAV_WAYPOINT_HUMAN;

		if (hs.pObstacle && hs.pObstacle->fApproxRadius > 0.01f)
		{
			Vec3	dir;
			Vec3	pos = hs.pos;

			GetAISystem()->AdjustOmniDirectionalCoverPosition(pos, dir, max(hs.pObstacle->fApproxRadius, 0.0f), 0.4f, targetPos);
			points.push_back(SAIStatFormPt(pos, hs.IsSecondary() ? 0.75f : 1.0f, navType));

			if (hs.pObstacle && !hs.pObstacle->bCollidable)
			{
				// On soft cover, try points at the side of the object, choose the visible one.
				//			odDir = enemyPos - hs.pObstacle->vPos;
				// Use sides for soft cover
				Vec3	norm(dir.y, -dir.x, 0.0f);
				norm.NormalizeSafe();

				float	rad = max(0.1f, hs.pObstacle->fApproxRadius);

				pos = hs.pos + norm * (rad + 1.0f);
				points.push_back(SAIStatFormPt(pos, 0.75f, navType));

				pos = hs.pos - norm * (rad + 1.0f);
				points.push_back(SAIStatFormPt(pos, 0.75f, navType));
			}

		}
		else
		{
			points.push_back(SAIStatFormPt(hs.pos, hs.IsSecondary() ? 0.75f : 1.0f, navType));
		}

		if (hs.pNavNodes)
		{
			for (std::vector<const GraphNode *>::const_iterator itn = hs.pNavNodes->begin(); itn != hs.pNavNodes->end(); ++itn)
			{
				const GraphNode* node = *itn;
				if (!node) continue;

				Vec3 dir = node->GetPos() - hs.pos;
				float	len = dir.GetLengthSquared();
				if (len > sqr(offsetFromCover))
				{
					Vec3	pos = hs.pos + dir * offsetFromCover/sqrtf(len);
					points.push_back(SAIStatFormPt(pos, 0.75f, navType));
				}
			}
		}
		if (hs.pNavNode)
		{
			Vec3 dir = hs.pNavNode->GetPos() - hs.pos;
			float	len = dir.GetLength();
			if (len > offsetFromCover)
			{
				Vec3	pos = hs.pos + dir * offsetFromCover/len;
				points.push_back(SAIStatFormPt(pos, 0.75f, navType));
			}
		}
	}

	VectorSet<int> touchedVertices;

	for (MapConstNodesDistance::iterator it = traversedNodes.begin(); it != traversedNodes.end(); ++it)
	{
		// Rasterize soft vegetation.
		const GraphNode* node = it->first;
		if (node->navType == IAISystem::NAV_TRIANGULAR)
		{
			const STriangularNavData* pTriData = node->GetTriangularNavData();
			for (unsigned i = 0; i < pTriData->vertices.size(); ++i)
				touchedVertices.insert(pTriData->vertices[i]);
		}

		const Vec3& p = node->GetPos();
		points.push_back(SAIStatFormPt(p, 0.25f, node->navType));
		for (unsigned link = node->firstLinkIndex; link; link = m_pGraph->GetLinkManager().GetNextLink(link))
		{
			Vec3 dir = m_pGraph->GetLinkManager().GetEdgeCenter(link) - p;
			float	len = dir.GetLengthSquared();
			if (len > sqr(offsetFromCover))
			{
				Vec3	pos = p + dir * offsetFromCover/sqrtf(len);
				points.push_back(SAIStatFormPt(pos, 0.25f, node->navType));
			}
		}
	}

	for (unsigned i = 0, ni = touchedVertices.size(); i < ni; ++i)
	{
		const ObstacleData& obstacle = GetAISystem()->m_VertexList.GetVertex(touchedVertices[i]);
		if (!obstacle.bCollidable && obstacle.fApproxRadius > 0.01f)
		{
			softOcclusion.RasterizeCircle(obstacle.vPos, max(0.1f, obstacle.fApproxRadius));
			outFormation.debugOcclusionObjectCount++;
		}
	}

	outFormation.debugOcclusion = softOcclusion;

	// Calc vis for points
	for (unsigned i = 0, ni = points.size(); i < ni; ++i)
	{
		Vec3 pos = points[i].pos;

		points[i].vis = 0;
		if (!softOcclusion.IsVisible(pos))
		{
			points[i].vis |= 1;
//			points[i].w *= 0.9f;
		}

		pos.z += 1.0f;
		if (pOcclusion && !pOcclusion->IsVisible(pos))
		{
			points[i].vis |= 2;
//			points[i].w *= 0.6f;
		}
	}


	// Copy the debug points.
	outFormation.debugPoints.resize(points.size());
	for (unsigned i = 0, n = points.size(); i < n; ++i)
		outFormation.debugPoints[i].Set(points[i].pos, points[i].w);

	// find max distance
	float minPtDist = FLT_MAX;
	float maxPtDist = -FLT_MAX;
	for (unsigned i = 0, n = points.size(); i < n; ++i)
	{
		Vec3	p = points[i].pos - groupPos;
		float	d = formationForw.Dot(p);
		minPtDist = min(minPtDist, d);
		maxPtDist = max(maxPtDist, d);
	}

	if (!pPivotPos)
		offsetTowardsTarget += mostFrontDist;

	float distToTarget = Distance::Point_Point(targetPos, groupPos);
	if (offsetTowardsTarget > distToTarget/2)
		offsetTowardsTarget = distToTarget/2;

	if (offsetTowardsTarget > max(0.0f, distToTarget - minDistanceToTarget))
		offsetTowardsTarget = max(0.0f, distToTarget - minDistanceToTarget);

	if (offsetTowardsTarget < minPtDist)
		offsetTowardsTarget = minPtDist;
	if (offsetTowardsTarget > maxPtDist)
		offsetTowardsTarget = maxPtDist;


	Vec3	advanceSpot = groupPos + formationForw * offsetTowardsTarget;


	for (unsigned i = 0, n = units.size(); i < n; ++i)
		units[i].d = formationRight.Dot(units[i].pos - groupPos);
	std::sort(units.begin(), units.end());


	for (unsigned i = 0, n = group.size(); i < n; ++i)
	{
		if (group[i]->GetTerritoryShape())
			group[i]->GetTerritoryShape()->ConstrainPointInsideShape(advanceSpot, true);
	}

	outFormation.debugAdvanceSpot = advanceSpot;

	const int histSamples = 30;
	float	histogram[histSamples];
	const float	histMinDist = -searchRange/2;
	const float	histMaxDist = searchRange/2;
	for (int i = 0; i < histSamples; ++i)
		histogram[i] = 0;
	const float histScale = 1.0f / (histMaxDist - histMinDist);

	const float distThreshold = searchRange; //offsetTowardsTarget * 1.5f;

	for (unsigned i = 0, n = points.size(); i < n; ++i)
	{
		Vec3	p = points[i].pos - advanceSpot;
		points[i].d = formationRight.Dot(p);
		float	dd = formationForw.Dot(p);

		if (fabsf(dd) < distThreshold)
		{
			float a = clamp(points[i].d - histMinDist, 0.0f, histMaxDist - histMinDist) * histScale;
			int	h = (int)(a * (histSamples-1));
			float v = (1.0f / (1+fabsf(dd))) * points[i].w;
			// gently bias the stuff in the center
			v *= 0.8f + sqr(sinf(a * gf_PI/2)) * 0.2f;
			histogram[h] += v;
		}
		else
		{
			points[i].test = SAIStatFormPt::INVALID;
		}
	}

	outFormation.debugHistogram.resize(histSamples);
	for (int i = 0; i < histSamples; ++i)
		outFormation.debugHistogram[i] = histogram[i];


	int maxSpotVal = -1;
	int maxSpotD = -1;

	float	histMinVal, histMaxVal;
	histMinVal = histMaxVal = histogram[0];

	for (int i = 1; i < histSamples-1; ++i)
	{
		histMinVal = min(histMinVal, histogram[i]);
		histMaxVal = max(histMaxVal, histogram[i]);
	}

	std::vector<SAIStatFormIsland>	islands;
	{
		float thr = histMinVal + (histMaxVal - histMinVal) / 3.0f;
		int start = -1;
		float accVal = 0;
		for (int i = 0; i < histSamples; ++i)
		{
			if (start == -1)
			{
				if (histogram[i] > thr)
				{
					accVal = histogram[i];
					start = i;
				}
			}
			else 
			{
				if (histogram[i] < thr)
				{
					islands.push_back(SAIStatFormIsland(start, i, -(i - start) * accVal));
					start = -1;
				}
				else
				{
					accVal += histogram[i];
				}
			}
		}
		if (start != -1)
		{
			islands.push_back(SAIStatFormIsland(start, histSamples, -(histSamples - start) * accVal));
		}
	}

//	std::sort(islands.begin(), islands.end());

	float	bestIsland = FLT_MAX;
	Vec3 bestIslandPos(0,0,0);

	const unsigned formationSize = units.size();
	float	formationWidth = spacing * (formationSize - 1);

	float groupD = formationRight.Dot(groupPos - advanceSpot);

	for (unsigned i = 0, ni = islands.size(); i < ni; ++i)
	{
		float a = (islands[i].a + 0.5f) / (float)histSamples;
		float	b = a + (islands[i].b - islands[i].a) / (float)histSamples;
		float d = (histMinDist + (a+b)/2 * (histMaxDist - histMinDist));

		if(d < histMinDist + formationWidth/2)
			d = histMinDist + formationWidth/2;
		if(d > histMaxDist - formationWidth/2)
			d = histMaxDist - formationWidth/2;

		outFormation.debugIslands.push_back(SAIStaticFormation::SDebugIsland(histMinDist + a * (histMaxDist - histMinDist),
			histMinDist + b * (histMaxDist - histMinDist), d, islands[i].v));

		float groupAddition = 0.0f;

		for (unsigned j = 0, nj = otherGroups.size(); j < nj; ++j)
		{
			std::pair<Vec3, float>& other = otherGroups[j];
			float distToLine = formationForw.Dot(other.first - advanceSpot);
			if (fabsf(distToLine) < other.second)
			{
				float distAlongLine = formationRight.Dot(other.first - advanceSpot);
				if (distAlongLine > (a - other.second) && distAlongLine < (b + other.second))
				{
					groupAddition += searchRange/3;
//					scale *= 2.0f;
				}
			}
		}

		if (location == SAIStaticFormation::AISF_NEAREST || location == SAIStaticFormation::AISF_BEST)
		{
			float absd = fabsf(d - groupD) + groupAddition; // * scale;
			if (absd < bestIsland)
			{
				bestIslandPos = advanceSpot + formationRight * d;
				bestIsland = absd;
			}
		}
		else if (location == SAIStaticFormation::AISF_NEAREST_LEFT)
		{
			float absd = fabsf(d - (groupD - searchRange/2)) + groupAddition; // * scale;
			if (absd < bestIsland)
			{
				bestIslandPos = advanceSpot + formationRight * d;
				bestIsland = absd;
			}
		}
		else if (location == SAIStaticFormation::AISF_NEAREST_RIGHT)
		{
			float absd = fabsf(d - (groupD + searchRange/2)) + groupAddition; // * scale;
			if (absd < bestIsland)
			{
				bestIslandPos = advanceSpot + formationRight * d;
				bestIsland = absd;
			}
		}
/*		else if (location == SAIStaticFormation::AISF_BEST)
		{
			float v = islands[i].v - groupAddition; // * (1/scale);
			if (v < bestIsland)
			{
				bestIslandPos = advanceSpot + formationRight * d;
				bestIsland = v;
			}
		}*/
	}

	if (!bestIslandPos.IsZero())
	{
		const Vec3& forw = formationForw;
		const Vec3& right = formationRight;

		outFormation.formationPoints.resize(units.size());
		outFormation.desiredFormationPoints.resize(units.size());
		outFormation.order.resize(units.size());

		float	avgUnitD = 0;
		float	avgTargetD = 0;

		for (unsigned i = 0; i < formationSize; ++i)
		{
			CPuppet* pPuppet = group[units[i].i];
			IAISystem::tNavCapMask puppetNavCapMask = pPuppet->GetMovementAbility().pathfindingProperties.navCapMask;
			const CPathObstacles &pathAdjustmentObstacles = pPuppet->GetPathAdjustmentObstacles();
			SShape* pTerrShape = pPuppet->GetTerritoryShape();
			float puppetRadius = pPuppet->GetParameters().m_fPassRadius;

			Vec3	desiredFormationPoint;
			if (type == SAIStaticFormation::AISF_ROW)
			{
				// row
				float a = (i - 0.5f * (formationSize-1)) * spacing;
				desiredFormationPoint = bestIslandPos + right*a;
				desiredFormationPoint.z += 0.5f;
			}
			else
			{
				// zig-zag line
				const float s = 1.0f / sqrtf(2.0f);
				float a = (i - 0.5f * (formationSize-1)) * spacing;
				float b = (i & 1) * 2.0f - 1.0f;
				desiredFormationPoint = bestIslandPos + forw*a*s + right*b*s;
				desiredFormationPoint.z += 0.5f;
			}

			float minDist = FLT_MAX;
			Vec3	pos;
			bool found = false;
			for (unsigned j = 0, m = points.size(); j < m; ++j)
			{
				if (points[j].test == SAIStatFormPt::INVALID) continue;

				// Check if the current points is too close to the target points of the already found units.
				bool skip = false;
				for (unsigned k = 0; k < i; ++k)
				{
					if (Distance::Point_PointSq(points[j].pos, units[k].targetPos) < sqr(spacing*0.9f))
					{
						skip = true;
						break;
					}
				}
				if (skip) continue;

				const Vec3& curPos = points[j].pos;

				Vec3	ab = curPos - desiredFormationPoint;
				float dx = right.Dot(ab) * 1.3f;
				float dy = forw.Dot(ab);
				float dz = ab.z;

				float vis = 0;
				if (points[j].vis & 1)
					vis += spacing*2.0f;
				if (points[j].vis & 2)
					vis += spacing*6.0f;

/*				if (points[j].vis < 0.0f)
				{
					points[j].vis = 0;
					if (!softOcclusion.IsVisible(points[j].pos))
						points[j].vis += spacing*1.5f;
					if (pOcclusion && !pOcclusion->IsVisible(points[j].pos+Vec3(0,0,1.0f)))
						points[j].vis += spacing*4.0f;
				}*/

				float	d =  sqrtf(dx*dx + dy*dy + dz*dz) * (1.5f - points[j].w*0.3) + vis;

				if (d < minDist)
				{
					if (points[j].test == SAIStatFormPt::NOT_TESTED)
					{
						if (pTerrShape && !pTerrShape->IsPointInsideShape(curPos, true))
						{
							points[j].test = SAIStatFormPt::INVALID;
							outFormation.debugPointsOutsideTerritory++;
							continue;
						}

						// Discard the point if it is inside another vegetation obstacle.
						bool	insideObstacle(false);
						for (unsigned k = 0, n = touchedVertices.size(); k < n; ++k)
						{
							const ObstacleData& obstacle = GetAISystem()->m_VertexList.GetVertex(touchedVertices[k]);
							if (obstacle.bCollidable && obstacle.fApproxRadius > 0.01f)
							{
								if (Distance::Point_Point2DSq(curPos, obstacle.vPos) < sqr(max(obstacle.fApproxRadius, 0.0f)))
								{
									insideObstacle = true;
									break;
								}
							}
						}
						if (insideObstacle)
						{
							points[j].test = SAIStatFormPt::INVALID;
							outFormation.debugPointsInsideObstacle++;
							continue;
						}

						int nBuildingID;
						IVisArea* pArea;
						IAISystem::ENavigationType navType = points[j].navType != IAISystem::NAV_UNSET ? points[j].navType :
							GetAISystem()->CheckNavigationType(curPos, nBuildingID, pArea, puppetNavCapMask);

						if (navType == IAISystem::NAV_TRIANGULAR)
						{
							if (IsPointForbidden(points[j].pos, puppetRadius, 0, 0))
							{
								points[j].test = SAIStatFormPt::INVALID;
								outFormation.debugPointsInsideForbiddenArea++;
								continue;
							}
							if (pathAdjustmentObstacles.IsPointInsideObstacles(points[j].pos))
							{
								points[j].test = SAIStatFormPt::INVALID;
								outFormation.debugPointsInsidePathObstacles++;
								continue;
							}
						}
					}

					minDist = d;
					pos = points[j].pos;
					found = true;
				}
			}

			if (!found)
			{
				outFormation.error = SAIStaticFormation::ERR_FORMATION_POINT_NOT_FOUND;
				return false;
			}

			avgUnitD += units[i].d;
			avgTargetD += formationRight.Dot(pos - groupPos);

			units[i].targetPos = pos;
			outFormation.formationPoints[units[i].i] = units[i].targetPos;
			outFormation.desiredFormationPoints[units[i].i] = desiredFormationPoint;
			outFormation.order[i] = units[i].i;
		}

		if (avgTargetD - avgUnitD > 0)
			std::reverse(outFormation.order.begin(), outFormation.order.end());

		return true;
	}

	outFormation.error = SAIStaticFormation::ERR_ISLANDS_NOT_FOUND;

	return false;
}


//===================================================================
// DebugDrawFakeTracer
//===================================================================
void CAISystem::DebugDrawFakeTracer(const Vec3& pos, const Vec3& dir)
{
	CAIObject* pPlayer = GetPlayer();
	if(!pPlayer) return;

	Vec3	dirNorm = dir.GetNormalizedSafe();
	const Vec3& playerPos = pPlayer->GetPos();
	Vec3	dirShooterToPlayer = playerPos - pos;

	float projDist = dirNorm.Dot(dirShooterToPlayer);

	float distShooterToPlayer = dirShooterToPlayer.NormalizeSafe();

	if(projDist < 0.0f)
		return;

	Vec3	nearestPt = pos + dirNorm * distShooterToPlayer;

	float tracerDist = Distance::Point_Point(nearestPt, playerPos);

	const float maxTracerDist = 20.0f;
	if(tracerDist > maxTracerDist)
		return;

	float a = 1 - tracerDist/maxTracerDist;

	ray_hit hit;
	float maxd = distShooterToPlayer * 2.0f;
	if(gEnv->pPhysicalWorld->RayWorldIntersection(pos, dirNorm * maxd, COVER_OBJECT_TYPES, HIT_COVER, &hit, 1))
	{
		maxd = hit.dist;
		// fake hit fx
		Vec3	p = hit.pt + hit.n * 0.1f;
		ColorF col(0.4f+ai_frand()*0.3f, 0.4f+ai_frand()*0.3f, 0.4f+ai_frand()*0.3f,0.9f);
		m_DEBUG_fakeHitEffect.push_back(SDebugFakeHitEffect(p, hit.n, (0.5f + ai_frand() * 0.5f) * 0.9f, 0.5f + ai_frand() * 0.3f, col));
	}

	const float maxTracerLen = 50.0f;
	float d0 = distShooterToPlayer - (0.75f + ai_frand()*0.25f) * maxTracerLen/2;
	float d1 = d0 + (0.5f + ai_frand()*0.5f) * maxTracerLen;

	Limit(d0, 0.0f, maxd);
	Limit(d1, 0.0f, maxd);

	m_DEBUG_fakeTracers.push_back(SDebugFakeTracer(pos + dirNorm*d0, pos + dirNorm*d1, a, 0.15f + ai_frand()*0.15f));


}

//===================================================================
// DEBUG_AddFakeDamageIndicator
//===================================================================
void CAISystem::DEBUG_AddFakeDamageIndicator(CPuppet* pShooter, float t)
{
	m_DEBUG_fakeDamageInd.push_back(SDebugFakeDamageInd(pShooter->GetPos(), t));

	IPhysicalEntity*	phys = 0;

	// If the AI is tied to a vehicle, use the vehicles physics to draw the silhouette.
	SAIBodyInfo bi;
	if(pShooter->GetProxy())
	{
		pShooter->GetProxy()->QueryBodyInfo(bi);
		if(bi.linkedVehicleEntity && bi.linkedVehicleEntity->GetAI() && bi.linkedVehicleEntity->GetAI()->GetProxy())
			phys = bi.linkedVehicleEntity->GetAI()->GetProxy()->GetPhysics(true);
	}

	if(!phys)
		phys = pShooter->GetProxy()->GetPhysics(true);

	if(!phys)
		return;

	SDebugFakeDamageInd& ind = m_DEBUG_fakeDamageInd.back();

	pe_status_nparts statusNParts;
	int nParts = phys->GetStatus(&statusNParts);

	pe_status_pos statusPos;
	phys->GetStatus(&statusPos);

	pe_params_part paramsPart;
	for (statusPos.ipart = 0, paramsPart.ipart = 0 ; statusPos.ipart < nParts ; ++statusPos.ipart, ++paramsPart.ipart)
	{
		phys->GetParams(&paramsPart);
		phys->GetStatus(&statusPos);

		primitives::box box;
		statusPos.pGeomProxy->GetBBox(&box);

		box.center *= statusPos.scale;
		box.size *= statusPos.scale;

		box.size.x += 0.05f;
		box.size.y += 0.05f;
		box.size.z += 0.05f;

		ind.verts.push_back(statusPos.pos + statusPos.q * (box.center + box.Basis.GetColumn0() * box.size.x));
		ind.verts.push_back(statusPos.pos + statusPos.q * (box.center + box.Basis.GetColumn0() * -box.size.x));
		ind.verts.push_back(statusPos.pos + statusPos.q * (box.center + box.Basis.GetColumn1() * box.size.y));
		ind.verts.push_back(statusPos.pos + statusPos.q * (box.center + box.Basis.GetColumn1() * -box.size.y));
		ind.verts.push_back(statusPos.pos + statusPos.q * (box.center + box.Basis.GetColumn2() * box.size.z));
		ind.verts.push_back(statusPos.pos + statusPos.q * (box.center + box.Basis.GetColumn2() * -box.size.z));
	}
}


//====================================================================
// IsSegmentValid
//====================================================================
bool CAISystem::IsSegmentValid(IAISystem::tNavCapMask navCap, float rad, const Vec3& posFrom, Vec3& posTo, IAISystem::ENavigationType& navTypeFrom)
{
	int nBuildingID = -1;
	IVisArea* pVisArea;

	navTypeFrom = CheckNavigationType(posFrom, nBuildingID, pVisArea, navCap);

	if (navTypeFrom == IAISystem::NAV_TRIANGULAR)
	{
		// Make sure not to enter forbidden area.
		if (IsPathForbidden(posFrom, posTo))
			return false;

		if (IsPointForbidden(posTo, rad))
			return false;
	}

	const SpecialArea* sa = 0;
	if (nBuildingID != -1)
		sa = GetSpecialArea(nBuildingID);
	if (sa)
	{
		const ListPositions	& buildingPolygon = sa->GetPolygon();
		SCheckWalkabilityState state;
		state.state = SCheckWalkabilityState::CWS_NEW_QUERY;
		state.numIterationsPerCheck = 100000;
		bool res = CheckWalkability(posFrom, posTo, rad + 0.15f - walkabilityRadius, true, buildingPolygon, AICE_ALL, 0, &state);
		if (res)
			posTo = state.toFloor;
		return res;
	}
	else
	{
		ListPositions buildingPolygon;
		SCheckWalkabilityState state;
		state.state = SCheckWalkabilityState::CWS_NEW_QUERY;
		state.numIterationsPerCheck = 100000;
		bool res = CheckWalkability(posFrom, posTo, rad + 0.15f - walkabilityRadius, true, buildingPolygon, AICE_ALL, 0, &state);
		if (res)
			posTo = state.toFloor;
		return res;
	}

}

//===================================================================
// ProcessBalancedDamage
//===================================================================
float CAISystem::ProcessBalancedDamage(IEntity* pShooterEntity, IEntity* pTargetEntity, float damage, const char* damageType)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	if (!pShooterEntity || !pTargetEntity || !pShooterEntity->GetAI() || !pTargetEntity->GetAI())
		return damage;

//	AILogEvent("CAISystem::ProcessBalancedDamage: %s -> %s Damage:%f type:%s\n", pShooterEntity->GetName(), pTargetEntity->GetName(), damage, damageType);

	CAIActor* pShooterActor = pShooterEntity->GetAI()->CastToCAIActor();
	if (pShooterActor->GetType() == AIOBJECT_PLAYER)
		return damage;

	if (pShooterActor && pShooterActor->GetProxy() && pShooterActor->GetProxy()->IsDriver() )
	{
		 EntityId vehicleId = pShooterActor->GetProxy()->GetLinkedVehicleEntityId();
		 if (vehicleId)
		 {
			IEntity* pVehicleEnt = gEnv->pEntitySystem->GetEntity(vehicleId);
			if (pVehicleEnt && pVehicleEnt->GetAI())
			{
				if ( pVehicleEnt->GetAI()->CastToCAIActor() )
				{
					pShooterEntity = pVehicleEnt;
					pShooterActor = pShooterEntity->GetAI()->CastToCAIActor();
				}
			}
		 }
	}


	if (!pShooterActor->IsHostile(pTargetEntity->GetAI()))
	{
		// Skip friendly bullets.
		// but only if this is not grabbed AI (human shield.)
		if ( GetPlayer()->GetProxy()->GetGrabbedEntity()!=pTargetEntity && strcmp(damageType, "bullet") == 0 )
			return 0.0f;
	}

	if (strcmp(damageType, "bullet") != 0)
		return damage;

	CPuppet* pShooterPuppet = pShooterEntity->GetAI()->CastToCPuppet();
	CAIActor* pTargetActor = pTargetEntity->GetAI()->CastToCAIActor();
	if (!pShooterPuppet || !pTargetActor)
		return 0.0f;

	if (pShooterPuppet->CanDamageTarget())
	{
		if (pTargetActor->GetType() == AIOBJECT_PLAYER)
		{
			if (pTargetActor->GetProxy())
			{
				Vec3 dirToTarget = pShooterPuppet->GetPos() - pTargetActor->GetPos();
				float distToTarget = dirToTarget.NormalizeSafe();

				float dirMod = (1.0f + dirToTarget.Dot(pTargetActor->GetViewDir()))/2;
				float distMod = 1.0f - sqr(1.0f - distToTarget/pShooterPuppet->GetParameters().m_fAttackRange);

				const float maxDirDamageMod = 0.6f;
				const float maxDistDamageMod = 0.7f;

				dirMod = maxDirDamageMod + (1 - maxDirDamageMod) * sqr(dirMod);
				distMod = maxDistDamageMod + (1 - maxDistDamageMod) * distMod;

				float maxHealth = pTargetActor->GetProxy()->GetActorMaxHealth();

//					Limit(damage, 0.0f,  maxHealth * 0.8f);

				damage *= dirMod * distMod;
				damage *= (0.6f + ai_frand() * 0.4f);

				DEBUG_AddFakeDamageIndicator(pShooterPuppet, (damage / maxHealth) * 5.0f);

				m_DEBUG_screenFlash += damage / maxHealth;
				if (m_DEBUG_screenFlash > 2.0f)
					m_DEBUG_screenFlash = 2.0f;
			}
		}

		return damage;
	}

	return 0.0f;
}


//===================================================================
// Interest System
//===================================================================
ICentralInterestManager* CAISystem::GetCentralInterestManager(void)
{
	return CCentralInterestManager::GetInstance();	
}


struct SAIPunchableObject
{
	SAIPunchableObject(const AABB& bounds, float dist, IPhysicalEntity* pPhys) :
		bounds(bounds), pPhys(pPhys), dist(dist) {}

	inline bool operator<(const SAIPunchableObject& rhs) const { return dist < rhs.dist; }

	AABB bounds;
	float dist;
	IPhysicalEntity* pPhys;
};

//===================================================================
// GetNearestPunchableObjectPosition
//===================================================================
bool CAISystem::GetNearestPunchableObjectPosition(IAIObject* pRef, const Vec3& searchPos, float searchRad, const Vec3& targetPos,
																									float minSize, float maxSize, float minMass, float maxMass,
																									Vec3& posOut, Vec3& dirOut, IEntity** objEntOut)
{
	CPuppet* pPuppet = CastToCPuppetSafe(pRef);
	if (!pPuppet)
		return false;

	const float agentRadius = 0.6f;
	const float distToObject = -0.1f;

	std::vector<SAIPunchableObject> objects;

	float searchRadSq = sqr(searchRad);

	AABB aabb(searchPos - Vec3(searchRad, searchRad, searchRad/2), searchPos + Vec3(searchRad, searchRad, searchRad/2));
	IPhysicalEntity** entities;
	unsigned nEntities = GetEntitiesFromAABB(entities, aabb, AICE_DYNAMIC);

	const CPathObstacles& pathAdjustmentObstacles = pPuppet->GetPathAdjustmentObstacles();
	IAISystem::tNavCapMask navCapMask = pPuppet->GetMovementAbility().pathfindingProperties.navCapMask;
	const float passRadius = pPuppet->GetParameters().m_fPassRadius;

	Vec3 dirToTarget = targetPos - searchPos;
	dirToTarget.Normalize();

	for (unsigned i = 0 ;i < nEntities ; ++i)
	{
		IPhysicalEntity* pPhysEntity = entities[i];

		// Skip moving objects
		pe_status_dynamics status;
		pPhysEntity->GetStatus(&status);
		if (status.v.GetLengthSquared() > sqr(0.1f))
			continue;
		if (status.w.GetLengthSquared() > sqr(0.1f))
			continue;

		pe_status_pos statusPos;
		pPhysEntity->GetStatus(&statusPos);


		AABB	bounds(statusPos.BBox[0] + statusPos.pos, statusPos.BBox[1] + statusPos.pos);
		Vec3 midBounds = bounds.GetCenter();

		float dist = Distance::Point_Point(statusPos.pos, searchPos);
		if (dist > searchRad)
			continue;

		float rad = bounds.GetRadius();
		if (rad < minSize || rad > maxSize)
			continue;
		float height = bounds.max.z - bounds.min.z;
		if (height < minSize || height > maxSize)
			continue;

		pe_status_dynamics	dyn;
		pPhysEntity->GetStatus(&dyn);
		float mass = dyn.mass;
		if (mass < minMass || mass > maxMass)
			continue;

		// Weight towards target.
		Vec3 dirToObject = statusPos.pos - searchPos;
		dirToObject.NormalizeFast();
		float dot = dirToTarget.Dot(dirToObject);
		float w = 0.25f + 0.75f * sqr((1-dot+1)/2);

		objects.push_back(SAIPunchableObject(bounds, dist * w, pPhysEntity));
	}

	std::sort(objects.begin(), objects.end());

	for (unsigned i = 0; i < objects.size(); ++i)
	{
		// check the altitude
		IPhysicalEntity* pPhysEntity = objects[i].pPhys;

		IEntity * pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pPhysEntity);
		if (!pEntity)
			continue;

		const AABB& bounds = objects[i].bounds;
		Vec3 center = bounds.GetCenter();

		Vec3 groundPos(center.x, center.y, center.z - 4.0f);
		Vec3 delta = groundPos - center;
		ray_hit hit;
		if (!gEnv->pPhysicalWorld->RayWorldIntersection(center, delta, AICE_ALL, 
			rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, &pPhysEntity, 1))
			continue;
		groundPos = hit.pt;

		Vec3 dirObjectToTarget = targetPos - center;
		dirObjectToTarget.z = 0;
		dirObjectToTarget.Normalize();

		float radius = bounds.GetRadius();

		Vec3	posBehindObject = center - dirObjectToTarget * (agentRadius + distToObject + radius);

		// Check if the point is reachable.
		delta = -dirObjectToTarget * (agentRadius + distToObject + radius);
		bool valid = true;
		if (gEnv->pPhysicalWorld->RayWorldIntersection(center, delta, AICE_ALL, 
			rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, &pPhysEntity, 1))
		{
			continue;
		}

		float height = bounds.max.z - bounds.min.z;

		// Raycast to find ground pos.
		if (!gEnv->pPhysicalWorld->RayWorldIntersection(posBehindObject, Vec3(0,0,-(height/2 + 1)), AICE_ALL, 
			rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, &pPhysEntity, 1))
			continue;
		posBehindObject = hit.pt;
		posBehindObject.z += 0.2f;

		// Check if it possible to stand at the object position.			
		if (OverlapCapsule(Lineseg(Vec3(posBehindObject.x, posBehindObject.y, posBehindObject.z+agentRadius+0.3f),
			Vec3(posBehindObject.x, posBehindObject.y, posBehindObject.z+2.0f - agentRadius)), agentRadius, AICE_ALL))
			continue;

		// Check stuff in front of the object.
		delta = dirObjectToTarget*3.0f;
		Vec3 target = center + delta;
		bool obstructed = false;
		if (gEnv->pPhysicalWorld->RayWorldIntersection(center, delta, AICE_ALL, 
			rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, &pPhysEntity, 1))
		{
			continue;
		}

		// Make sure the location can be reached.
		int nBuildingID;
		IVisArea* pArea;
		IAISystem::ENavigationType navType = CheckNavigationType(posBehindObject, nBuildingID, pArea, navCapMask);
		if (navType == IAISystem::NAV_TRIANGULAR)
		{
			if (GetAISystem()->IsPointForbidden(posBehindObject, passRadius, 0, 0))
				continue;
			if (pathAdjustmentObstacles.IsPointInsideObstacles(posBehindObject))
				continue;
		}

		// The object is valid.
		posOut = posBehindObject;
		dirOut = dirObjectToTarget;
		*objEntOut = pEntity;

		return true;
	}

	return false;
}

// NOTE Mai 21, 2007: <pvl> I put this function here because there's no AllNodesContainer.cpp
// and creating it for a single debugging function that might go away any time seems overkill.
// It's also too complex in terms of dependencies to go into AllNodesContainer.h .
// ATTN Mai 21, 2007: <pvl> if something seems strange in this function, it might well be
// a bug.  This is a piece of debugging code that was written too fast.
// Update: <mikko> Changed the validation to reflect the changes in hash space code.
bool CAllNodesContainer::ValidateHashSpace() const
{
	std::auto_ptr <Iterator> nodeIt (new Iterator(*this, IAISystem::NAV_WAYPOINT_HUMAN));
	int numWayptNodes = 0;

	AILogProgress(">>> Validating HashSpace");
	while (unsigned int nodeIndex = nodeIt->GetNode())
	{
		GraphNode * node = GetAISystem()->GetGraph()->GetNodeManager().GetNode(nodeIndex);

		++numWayptNodes;

		typedef std::vector< std::pair<float, unsigned> > TNodes;
		static TNodes nodes;
		nodes.resize(0);

		GetAllNodesWithinRange(nodes, node->GetPos(), 0.05f, IAISystem::NAV_WAYPOINT_HUMAN);

		if (nodes.size() > 1)
			AIWarning ("More than one node within 5cm from each other.");

		bool validated = false;

		TNodes::const_iterator it = nodes.begin();
		TNodes::const_iterator end = nodes.end();
		for ( ; it != end; ++it)
		{
			if (it->second == nodeIndex)
			{
				validated = true;
				break;
			}
		}
		if (!validated)
		{
			AIWarning ("HashSpace probably corrupt - a node in a wrong cell!");
			return false;
		}

		nodeIt->Increment();
	}
	AILogProgress (">>> HashSpace validation: %d waypts in the level", numWayptNodes);

	for (unsigned i = 0, ni = m_hashSpace.GetBucketCount(); i < ni; ++i)
	{
		for (unsigned j = 0, nj = m_hashSpace.GetObjectCountInBucket(i); j < nj; ++j)
		{
			const SGraphNodeRecord& nodeRec = m_hashSpace.GetObjectInBucket(j, i);

			if (!DoesNodeExist(nodeRec.nodeIndex))
			{
				Vec3 nodePos = nodeRec.GetPos(GetAISystem()->GetGraph()->GetNodeManager());
				AIWarning ("HashSpace probably corrupt - contains a node that's not in AllNodesContainer!");
				AIWarning ("  pos = (%5.2f, %5.2f, %5.2f), index = %d", nodePos.x, nodePos.y, nodePos.z, nodeRec.nodeIndex);
				return false;
			}

			int ii, jj, kk;
			m_hashSpace.GetIJKFromPosition(nodeRec.GetPos(GetAISystem()->GetGraph()->GetNodeManager()), ii, jj, kk);
			unsigned bucket = m_hashSpace.GetHashBucketIndex(ii,jj,kk);
			if (i != bucket)
				AIWarning ("HashSpace probably corrupt - a node incorrectly assigned to cell!");
		}
	}

	return true;
}

//====================================================================
// AllocGoalPipeId
//====================================================================
int CAISystem::AllocGoalPipeId() const
{
	static int g_iLastActionId = 0;
	g_iLastActionId++;
	if ( g_iLastActionId < 0 )
		g_iLastActionId = 1;
	return g_iLastActionId;
}

//====================================================================
// CreateSignalExtraData
//====================================================================
IAISignalExtraData* CAISystem::CreateSignalExtraData() const
{
	return new AISignalExtraData;
}

//====================================================================
// FreeSignalExtraData
//====================================================================
void CAISystem::FreeSignalExtraData( IAISignalExtraData* pData ) const
{
	if ( pData )
		delete (AISignalExtraData*) pData;
}

