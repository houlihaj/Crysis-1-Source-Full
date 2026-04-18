#ifndef _CAISYSTEM_H_
#define _CAISYSTEM_H_

#if _MSC_VER > 1000
#pragma once
#endif

#include "IAISystem.h"
#include "ISystem.h"
#include "ITimer.h"
#include "AISignal.h"
#include "CTriangulator.h"
#include "Formation.h"
#include "TrackPatternDescriptor.h"
#include "Graph.h"
#include "BuildingIDManager.h"
#include "VertexList.h"
#include "PipeManager.h"
#include "AIDbgRecorder.h"
#include "AIObject.h"
#include "AICollision.h"
#include "FireCommand.h"
#include "SmartObjects.h"
#include "TickCounter.h"
#include "AIGroup.h"
#include "AIQuadTree.h"
#include "MiniQueue.h"
#include "AIRadialOcclusion.h"
#include "AILightManager.h"
#include "AIDynHideObjectManager.h"
#include "Shape.h"
#include "ShapeContainer.h"

#include <map>

#include <crc32.h>

#define VIEW_RAY_PIERCABILITY 10
#define HIT_COVER ((geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (VIEW_RAY_PIERCABILITY & rwi_pierceability_mask))
#define HIT_SOFT_COVER (geom_colltype14 << rwi_colltype_bit)

#define COVER_OBJECT_TYPES (ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid)

// Desired clearance between cover object and agent
#define COVER_CLEARANCE	0.8f

class CGraph;
struct GraphNode;
struct IRenderer;
struct ISystem;
struct IGame;
class CAIObject;
struct IAIObject;
struct ICVar;
class CPuppet;
class CAIVehicle;
class CPipeUser;
class CLeader;
class CAIAutoBalance;
struct IAgentProxy;
struct IEntity;
class CGoalPipe;
struct IPhysicalEntity;
class CVolumeNavRegion;
class CWaypointHumanNavRegion;
class CWaypoint3DSurfaceNavRegion;
class CFlightNavRegion;
class CRoadNavRegion;
class CFree2DNavRegion;
class CSmartObjectNavRegion;
class CVehicleFinder;
//class CSmartObjects;
class CAIActionManager;
class ICentralInterestManager;

struct AuxSignalDesc
{
	float fTimeout;
	string strMessage;
	void Serialize( TSerialize ser, CObjectTracker& objectTracker )
	{
		ser.Value("AuxSignalDescTimeOut",fTimeout);
		ser.Value("AuxSignalDescMessage",strMessage);
	}
};


struct BeaconStruct
{
	CAIObject *pBeacon;
	CAIObject *pOwner;

	BeaconStruct() 
	{
		pBeacon = 0;
		pOwner = 0;
	}
};

class CShapeMask
{
public:
  CShapeMask();
  ~CShapeMask();

  /// Builds this mask according to pShape down to the granularity
  void Build(const struct SShape *pShape, float granularity);

  /// categorisation of a point according to this mask
  enum EType {TYPE_IN, TYPE_EDGE, TYPE_OUT};
  EType GetType(const Vec3 pt) const;

  size_t MemStats() const;
private:
  /// used just during building
  EType GetType(const struct SShape *pShape, const Vec3 &min, const Vec3 &max) const;
  /// gets the min/max values of the 2D box indicated by i and j
  void GetBox(Vec3 &min, Vec3 &max, int i, int j);
  AABB m_aabb;
  unsigned m_nx, m_ny;
  float m_dx, m_dy; // number of pixels

  // container bit-pair is indexed by x + y * m_bytesPerRow
  unsigned m_bytesPerRow;
  typedef std::vector<unsigned char> TContainer;
  TContainer m_container;
  

};

struct SShape
{
  SShape() :
		navType(IAISystem::NAV_UNSET), type(0), devalueTime(0.0f), height(0.0f),
		temporary(false), aabb(AABB::RESET), enabled(true), shapeMask(0), lightLevel(AILL_NONE)
  {
  }

  explicit SShape(const ListPositions& shape, const AABB& aabb, IAISystem::ENavigationType navType = IAISystem::NAV_UNSET,
    int type = 0, float height = 0.0f, EAILightLevel lightLevel = AILL_NONE, bool temp = false) :
	  shape(shape), aabb(aabb), navType(IAISystem::NAV_UNSET), type(0), devalueTime(0), height(height),
		temporary(temp), enabled(true), shapeMask(0), lightLevel(lightLevel)
  {
  }

  explicit SShape(const ListPositions& shape, bool allowMask = false, IAISystem::ENavigationType navType = IAISystem::NAV_UNSET,
    int type = 0, float height = 0.0f, EAILightLevel lightLevel = AILL_NONE, bool temp = false);

  ~SShape()
  {
    ReleaseMask();
  }

  void RecalcAABB()
  {
    aabb.Reset();
    aabb.min.z =  10000.0f; // avoid including limit
    aabb.max.z = -10000.0f;
    for (ListPositions::const_iterator it = shape.begin() ; it != shape.end() ; ++it)
      aabb.Add(*it);
  }

  /// Builds a mask with a specified granularity
  void BuildMask(float granularity);

  void ReleaseMask()
  {
    delete shapeMask;
    shapeMask = 0;
  }

  ListPositions::const_iterator NearestPointOnPath(const Vec3& pos, float& dist, Vec3& nearestPt, float* distAlongPath = 0, Vec3* segmentDir = 0) const
  {
		if (shape.empty())
			return shape.end();
		ListPositions::const_iterator nearest(shape.begin());
		dist = FLT_MAX;
		ListPositions::const_iterator cur = shape.begin();
		ListPositions::const_iterator next(cur);
		++next;
		float pathLen = 0.0f;
		while(next != shape.end())
		{
			Lineseg	seg(*cur, *next);
			float	t;
			float	d = Distance::Point_Lineseg(pos, seg, t);
			if(d < dist)
			{
				dist = d;
				nearestPt = seg.GetPoint(t);
				if (distAlongPath)
					*distAlongPath = pathLen + Distance::Point_Point(seg.start, nearestPt);
				if (segmentDir)
					*segmentDir = seg.end - seg.start;
				nearest = next;
			}
			pathLen += Distance::Point_Point(seg.start, seg.end);
			cur = next;
			++next;
		}
		return nearest;
	}

	Vec3 GetPointAlongPath(float dist) const
	{
		if (shape.empty())
			return ZERO;

		if (dist < 0.0f)
			return shape.front();

		ListPositions::const_iterator cur = shape.begin();
		ListPositions::const_iterator next(cur);
		++next;

		float d = 0.0f;
		while (next != shape.end())
		{
			Vec3 delta = *next - *cur;
			float len = delta.GetLength();
			if (len > 0.0f && dist >= d && dist < d+len)
			{
				float t = (dist - d) / len;
				return *cur + delta * t;
			}
			d += len;
			cur = next;
			++next;
		}
		return *cur;
	}

  bool	IsPointInsideShape(const Vec3& pos, bool checkHeight) const
  {
    // Check height.
    if (checkHeight && height > 0.01f)
    {
      float	h = pos.z - aabb.min.z;
      if (h < 0.0f || h > height)
        return false;
    }
    // Is the request point inside the shape.
    if (Overlap::Point_Polygon2D(pos, shape, &aabb))
      return true;
    return false;
  }

  bool	ConstrainPointInsideShape(Vec3& pos, bool checkHeight) const
  {
    if(!IsPointInsideShape(pos, checkHeight))
    {
      // Target is outside the territory, find closest point on the edge of the territory.
      float	dist = 0;
      Vec3	nearest;
      NearestPointOnPath(pos, dist, nearest);
      // adjust height.
      nearest.z = pos.z;
      if(checkHeight && height > 0.00001f)
        nearest.z = clamp(nearest.z, aabb.min.z, aabb.min.z + height); 
      pos = nearest;
      return true;
    }
    return false;
  }

  size_t MemStats() const
  {
    size_t size = sizeof(*this) + shape.size() * sizeof(Vec3);
    if (shapeMask.Get())
      size += shapeMask->MemStats();
    return size;
  }

  ListPositions shape;                /// Points describing the shape.
  AABB aabb;                          /// Bounding box of the shape points.
  IAISystem::ENavigationType navType; /// Navigation type associated with AIPaths
  int type;                           /// AIobject/anchor type associated with an AIshape
  float  height;                      /// Height of the shape. The shape volume is described as [aabb.minz, aabb.minz+height]
  float  devalueTime;                 /// Timeout for occupied AIpaths
  bool  temporary;                    /// Flag indicating if the path is temporary and should be deleted upon reset.
  bool  enabled;                      /// Flag indicating if the shape is enabled. Disabled shapes are excluded from queries.
	EAILightLevel lightLevel;						/// The light level modifier of the shape.
  class ShapeMaskPtr
  {
  public:
    ShapeMaskPtr(CShapeMask *ptr = 0) : m_p(ptr) {}
    ShapeMaskPtr(const ShapeMaskPtr &other) {m_p = 0; if (other.m_p) {m_p = new CShapeMask; *m_p = *other.m_p;}}
    CShapeMask *operator->() {return m_p;}
    const CShapeMask *operator->() const {return m_p;}
    CShapeMask &operator*() {return *m_p;}
    const CShapeMask &operator*() const {return *m_p;}
    operator CShapeMask*() {return m_p;}
    CShapeMask* Get() {return m_p;}
    const CShapeMask* Get() const {return m_p;}
  private:
    CShapeMask *m_p;
  };
  ShapeMaskPtr shapeMask;             /// Optional mask for this shape that speeds up 2D in/out/edge tests
};

struct SExtraLinkCostShape
{
  SExtraLinkCostShape(const ListPositions& shape, const AABB& aabb, float costFactor) : shape(shape), aabb(aabb), costFactor(costFactor), origCostFactor(costFactor) {}
	SExtraLinkCostShape(const ListPositions& shape, float costFactor) : shape(shape), costFactor(costFactor), origCostFactor(costFactor)
	{
		aabb.Reset();
		for (ListPositions::const_iterator it = shape.begin() ; it != shape.end() ; ++it)
			aabb.Add(*it);
	}
	ListPositions shape;
	AABB aabb;
  // the cost factor can get modified at run-time - it will get reset at the same time
  // as graph links get reset etc
  float costFactor;
  float origCostFactor;
};

struct SDynamicTriangulationShape
{
	SDynamicTriangulationShape(const ListPositions& shape, const AABB& aabb, float triSize) : shape(shape), aabb(aabb), triSize(triSize) {}
	SDynamicTriangulationShape(const ListPositions& shape, float triSize) : shape(shape), triSize(triSize)
	{
		aabb.Reset();
		aabb.min.z = -10000.0f; // avoid including limit
		aabb.max.z =  10000.0f;
		for (ListPositions::const_iterator it = shape.begin() ; it != shape.end() ; ++it)
			aabb.Add(*it);
	}
	ListPositions shape;
	AABB aabb;
  float triSize;
};

//===================================================================
// SHideSpot
//===================================================================
struct SHideSpot
{
  enum EHideSpotType
  {
    HST_TRIANGULAR,
    HST_WAYPOINT,
    HST_ANCHOR,
    HST_SMARTOBJECT,
    HST_VOLUME,
    HST_DYNAMIC
  };

  SHideSpot(EHideSpotType type, const Vec3 &pos, const Vec3 &dir) : type(type), pos(pos), objPos(pos), dir(dir), 
    pNavNode(0), pNavNodes(0), pAnchorObject(0), pObstacle(0), entityId(0) {}

	/// Returns true if the hidepoint is secondary.
	inline bool IsSecondary() const
	{
		switch(type)
		{
		case HST_TRIANGULAR:
			return pObstacle && !pObstacle->bCollidable;
			break;
		case HST_WAYPOINT:
			return pNavNode && pNavNode->navType == IAISystem::NAV_WAYPOINT_HUMAN && pNavNode->GetWaypointNavData()->type == WNT_HIDESECONDARY;
			break;
		case HST_ANCHOR:
			return pAnchorObject && pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT_SECONDARY;
			break;
		default:
			return false;
		};
		return false;
	}

  const EHideSpotType type; ///< always set

  Vec3 pos; ///< always set
  Vec3 dir; ///< may be 0
	Vec3 objPos;

  // optional params - used by multiple types
  const GraphNode *pNavNode;                 ///< may be 0
  const std::vector<const GraphNode *> *pNavNodes; ///< may be 0
	EntityId		entityId;							///< The entity id of the source object for dynamic hidepoints.

  /// params only used by one specific type
  const ObstacleData *pObstacle;   ///< only set for triangular
  CQueryEvent SOQueryEvent;         ///< only set for smart objects
  const CAIObject *pAnchorObject;      ///< only set for anchors
};

typedef std::multimap<short , CAIObject *> AIObjects;
typedef std::multimap<INT_PTR,int> IntIntMultiMap;
typedef std::list<CAIObject *, stl::STLPoolAllocator<CAIObject*> > ListAIObjects;
typedef std::set<CAIObject *, std::less<CAIObject*>, stl::STLPoolAllocator<CAIObject*> > SetAIObjects;
typedef std::map<string, CFormationDescriptor> FormationDescriptorMap;
typedef std::map<string, CTrackPatternDescriptor> PatternDescriptorMap;
typedef std::map<CAIObject*, CFormation*> FormationMap;
typedef std::map<int, CAIGroup*> AIGroupMap;
typedef std::map<unsigned short, BeaconStruct> BeaconMap; 

typedef std::map<string, SShape> ShapeMap;

typedef std::map<string, SExtraLinkCostShape> ExtraLinkCostShapeMap;
typedef std::map<string, SDynamicTriangulationShape> DynamicTriangulationShapeMap;
typedef std::vector<CPuppet *> VectorPuppets;
typedef std::map<int, float> MapMultipliers;
typedef std::multimap< short, AuxSignalDesc> MapSignalStrings;
typedef std::multimap<float, SHideSpot> MultimapRangeHideSpots;
typedef std::list<unsigned short> ListOfUnsignedShort;

template<class T1, class T2>
struct SSerialisablePair : public std::pair<T1, T2>
{
  SSerialisablePair(const std::pair<T1, T2> &p) : std::pair<T1, T2>(p) {}
  SSerialisablePair() {}
  void Serialize(TSerialize ser) {ser.Value("first", this->first); ser.Value("second", this->second);}
};

//<<FIXME>> just used for profiling
typedef std::map<string,float> TimingMap;


struct SpecialArea
{
	enum EType { TYPE_WAYPOINT_HUMAN, TYPE_VOLUME, TYPE_FLIGHT, TYPE_WATER, TYPE_WAYPOINT_3DSURFACE, TYPE_FREE_2D, TYPE_TRIANGULATION};

  void SetPolygon(const ListPositions &polygon) {lstPolygon = polygon; CalcAABB();}
  const ListPositions &GetPolygon() const {return lstPolygon;}

  const AABB &GetAABB() const {return aabb;}

	float fMinZ,fMaxZ;
	float fHeight;
	float fNodeAutoConnectDistance;
  int m_iWaypointType;
  int bVehiclesInHumanNav;
	int	nBuildingID;
	SpecialArea::EType type;
  bool  bCalculate3DNav;
  float f3DNavVolumeRadius;
	EAILightLevel lightLevel;

  EWaypointConnections	waypointConnections;
	bool	bAltered;		// making links unpassible

	SpecialArea() : nBuildingID(-1), type(SpecialArea::TYPE_WAYPOINT_HUMAN),
		bCalculate3DNav(true), f3DNavVolumeRadius(10.0f), waypointConnections(WPCON_DESIGNER_NONE),
		bVehiclesInHumanNav(false), bAltered(false), lightLevel(AILL_NONE)
	{
		fMinZ = 9999.f;
		fMaxZ = -9999.f;
		fHeight = 0;
    aabb.Reset();
	}

private:
  ListPositions	lstPolygon;
  AABB aabb;

  void CalcAABB()
	{
		aabb.Reset();
		for (ListPositions::const_iterator it = lstPolygon.begin() ; it != lstPolygon.end() ; ++it)
			aabb.Add(*it);
	}
};

typedef std::map<string, SpecialArea> SpecialAreaMap;
// used for faster GetSpecialArea() lookup in game mode
typedef std::vector<SpecialArea*> SpecialAreaVector;


struct PathFindRequest
{
	enum ERequestType
	{
		TYPE_ACTOR,
		TYPE_RAW,
	};
	ERequestType type;

	unsigned startIndex;
	unsigned endIndex;
	Vec3 startPos;
	Vec3 startDir;
	Vec3 endPos;
	/// endDir magnitude indicates the tendency to line up at the end of the path - 
	/// magnitude whould be between 0 and 1
	Vec3 endDir;
	bool bSuccess;
	CAIActor *pRequester;
  int nForceTargetBuildingId;
  bool allowDangerousDestination;
  float endTol;
	float endDistance;
  // as a result of RequestPathInDirection or RequestPathTo
  bool isDirectional;

	/// This gets set to false if the path end position doesn't match the requested end position
	/// (e.g. in the event of a partial path, or if the destination is in forbidden)
	bool bPathEndIsAsRequested;

	int id;
	IAISystem::tNavCapMask navCapMask;
	float passRadius;

  CStandardHeuristic::ExtraConstraints extraConstraints;

	const char* GetRequesterName() const
	{
		if (type == TYPE_ACTOR)
			return pRequester->GetName();
		static char label[16];
		_snprintf(label, 16, "#%d", id);
		return label;
	}

	PathFindRequest(ERequestType type) :
		type(type),
		startIndex(0),
		endIndex(0),
		pRequester(0),
		bPathEndIsAsRequested(false),
		allowDangerousDestination(false),
		endTol(std::numeric_limits<float>::max()),
		endDistance(0),
		nForceTargetBuildingId(-1),
		isDirectional(false),
		id(-1),
		navCapMask(IAISystem::NAV_UNSET),
		passRadius(0.0f)
	{
	}

  void Serialize(TSerialize ser, CObjectTracker& objectTracker);

};

typedef std::list<PathFindRequest *> PathQueue;
struct IVisArea;

struct CutEdgeIdx
{
	int idx1;
	int idx2;

	CutEdgeIdx( int i1, int i2 ) 
	{
		idx1 = i1;
		idx2 = i2;
	}

	// default ctor to allow std::vector::resize(0)
	CutEdgeIdx() {}
};

typedef std::vector<CutEdgeIdx> NewCutsVector;


//#define CALIBRATE_STOPPER

/// Used to determine when a calculation should stop. Normally this would be after a certain time (dt). However, 
/// if m_useCounter has been set then the times are internally converted to calls to ShouldCalculationStop using
/// callRate, which should have been estimated previously
class CCalculationStopper
{
public:
  /// name is used during calibration. dt is the amount of time (seconds).
  /// callRate is for when we are running in "counter" mode - the time
  /// gets converted into a number of calls to ShouldCalculationStop
	CCalculationStopper(const char * name, float dt, float callRate);
	bool ShouldCalculationStop() const;
  float GetMilliSecondsRemaining() const {if (m_useCounter) return 1000 * m_stopCounter / m_callRate; else return (m_endTime - gEnv->pTimer->GetAsyncTime()).GetMilliSeconds();}

  static bool m_useCounter;
private:
	CTimeValue m_endTime;
  mutable unsigned m_stopCounter; // if > 0 use this - stop when it's 0
	static bool m_neverStop; // for debugging
  float m_callRate;
#ifdef CALIBRATE_STOPPER
public:
  mutable unsigned m_calls;
  float m_dt;
  string m_name;
  /// the pair is calls and time (seconds)
  typedef std::map< string, std::pair<unsigned, float> > TMapCallRate;
  static TMapCallRate m_mapCallRate;
#endif
};


//===================================================================
// ShouldCalculationStop
//===================================================================
inline bool CCalculationStopper::ShouldCalculationStop() const 
{
  if (m_neverStop)
    return false;
  if (m_useCounter)
  {
    if (m_stopCounter > 0) --m_stopCounter;
    return m_stopCounter == 0;
  }
  else
  {
#ifdef CALIBRATE_STOPPER
    ++m_calls;
    if (gEnv->pTimer->GetAsyncTime() > m_endTime)
    {
      std::pair<unsigned, float> &record = m_mapCallRate[m_name];
      record.first += m_calls;
      record.second += m_dt;
      return true;
    }
    else
    {
      return false;
    }
#else
    return gEnv->pTimer->GetAsyncTime() > m_endTime;
#endif
  }
}

// Listener for path found events.
struct IAIPathFinderListerner
{
	virtual void OnPathResult(int id, const std::vector<unsigned>* pathNodes) = 0;
};

enum EGetObstaclesInRadiusFlags
{
	OBSTACLES_COVER = 0x01,
	OBSTACLES_SOFT_COVER = 0x02,
};


//====================================================================
//====================================================================
// CAISystem 
//====================================================================
class CTriangularNavRegion;
class TickCounterManager;

class CAISystem : public IAISystem
{
friend class VisCheckQueueManager;	// need to access debug-draw functions
public:
//-------------------------------------------------------------

	CAISystem(ISystem *pSystem);
	~CAISystem();
	void Release() {delete this;}

	// CRAIG HACKS
	CLeader * IsPartOfAnyLeadersUnits( CAIObject * pObject );

	//====================================================================
	// Interface functions (from IAISystem)
	//====================================================================
	virtual bool Init();
	virtual bool InitSmartObjects();
	virtual void Update(CTimeValue currentTime, float frameTime);
	virtual void Enable(bool enable=true) {m_IsEnabled=enable;}
  virtual bool GetUpdateAllAlways() const;
	// Resets all agent states to initial
	virtual void Reset(IAISystem::EResetReason reason);
  virtual void StoppingAIUpdates();
	virtual void ShutDown();
	/// this called before loading (level load/serialization)
	virtual void FlushSystem(void);		
	virtual void FlushSystemNavigation(void);
	// save/load
	virtual void Serialize( TSerialize ser );

	// profiling
	virtual void TickCountStartStop( bool start=true );

	virtual void Event( int event, const char *name);

	virtual void GetMemoryStatistics(ICrySizer *pSizer);
  /// This is the time at the beginning of the most recent AI system update. It should
  /// be used in preference to the system time so that AI syncs to physics better.
	virtual CTimeValue GetFrameStartTime() const {return m_fFrameStartTime;}
  /// This is the time between the most recent AI system update and the preceeding update.
  /// It will generally be different to the system frame time because of the synchronisation
  /// between AI and physics.
	virtual float GetFrameDeltaTime() const {return m_fFrameDeltaTime;}
  /// The time between AI full-updates 
	virtual float GetUpdateInterval() const;

	virtual void DebugDraw(IRenderer *pRenderer);

	virtual ENavigationType CheckNavigationType(const Vec3 & pos, int & nBuildingID, IVisArea *&pArea, IAISystem::tNavCapMask navCapMask);
	virtual void DisableNavigationInBrokenRegion(std::list<Vec3> & outline);
	virtual IGraph * GetHideGraph(void);
	virtual IGraph * GetNodeGraph(void);

	virtual IAIObject * GetAIObjectByName(unsigned short type, const char *pName) const;
	virtual IAIObject * CreateAIObject(unsigned short type, IAIObject *pAssociation);
	virtual void RemoveObject(IAIObject *pObject);
	virtual void RemoveSmartObject(IEntity *pEntity) { if ( m_pSmartObjects ) m_pSmartObjects->RemoveEntity( pEntity ); }

	virtual IAIObject *GetNearestObjectOfTypeInRange(const Vec3 &pos, unsigned int type, float fRadiusMin, float fRadiusMax, IAIObject* pSkip=NULL, int nOption = 0);
	virtual IAIObject *GetNearestObjectOfTypeInRange(IAIObject *pObject , unsigned int type, float fRadiusMin, float fRadiusMax, int nOption = 0);
	virtual IAIObject *GetNearestAssociatedObjectOfTypeInRange(IAIObject *pObject , unsigned int type, float fRadiusMin, float fRadiusMax, int nOption = 0);
	virtual IAIObject *GetRandomObjectInRange(IAIObject * pRef, unsigned short nType, float fRadiusMin, float fRadiusMax, bool bFaceAttTarget=false);
	virtual IAIObject *GetNearestToObjectInRange( IAIObject *pRef , unsigned short nType, float fRadiusMin, float fRadiusMax, float inCone=-1, bool bFaceAttTarget=false, bool bSeesAttTarget=false, bool bDevalue=true);
	virtual IAIObject *GetBehindObjectInRange(IAIObject * pRef, unsigned short nType, float fRadiusMin, float fRadiusMax);
	virtual CAIObject *FindNearestObjectInFront(CAIObject *pObject , unsigned int type, float fRadius, float cosTrh=.7f);

	virtual bool GetNearestPunchableObjectPosition(IAIObject* pRef, const Vec3& searchPos, float searchRad, const Vec3& targetPos, float minSize, float maxSize, float minMass, float maxMass, Vec3& posOut, Vec3& dirOut, IEntity** objEntOut);

	// Devalues an AI object for the refence object only or for whole group.
	virtual void			Devalue( IAIObject* pRef, IAIObject* pObject, bool group, float fDevalueTime=20.f );

	// Inherited from IAISystem
	virtual IAIObjectIter* GetFirstAIObject(EGetFirstFilter filter, short n);
	virtual IAIObjectIter* GetFirstAIObjectInRange(EGetFirstFilter filter, short n, const Vec3& pos, float rad, bool check2D);
	virtual IAIObjectIter* GetFirstAIObjectInShape(EGetFirstFilter filter, short n, const char* shapeName, bool checkHeight);

	// returns the leader of a group
	virtual ILeader* GetILeader(int nGroupID);
	virtual CLeader* GetLeader(int nGroupID);
	CLeader* GetLeader(const CAIActor* pSoldier);
	virtual void SetLeader(IAIObject* pObject);
	CLeader* CreateLeader(int nGroupID);
	// returns the AI group.
	virtual IAIGroup*	GetIAIGroup(int nGroupID);
	virtual CAIGroup*	GetAIGroup(int nGroupID);

	virtual IAIObject* GetLeaderAIObject(int iGroupId);
	virtual IAIObject* GetLeaderAIObject(IAIObject* pObject);
	//virtual void	RequestGroupAttack(int nGroupID, uint32 unitProperties, int actionType, float fDuration);
	virtual IAIObject* GetFormationPoint(IAIObject* pRequester);
	void FreeFormationPoint(CAIObject * pAIObject);
	virtual int	GetFormationPointClass(const char* descriptorName, int position);

	// Creates a new formation descriptor
	virtual void CreateFormationDescriptor(const char *name);
	// Adds a point to an existing formation descriptor
	virtual void AddFormationPoint(const char *name, const FormationNode& nodeDescriptor);
	
	bool IsFormationDescriptorExistent(const char *name);

	// Track patterns.
	virtual ITrackPatternDescriptor*	CreateTrackPatternDescriptor( const char* patternName );
	virtual ITrackPatternDescriptor*	FindTrackPatternDescriptor( const char* patternName );

	virtual	void RegisterFirecommandHandler(IFireCommandDesc* desc);
	virtual	IFireCommandHandler*	CreateFirecommandHandler(const char* name, IAIActor *pShooter);

	virtual IGoalPipe *CreateGoalPipe(const char *pName);
	virtual IGoalPipe *OpenGoalPipe(const char *pName);

	virtual const ObstacleData GetObstacle(int nIndex);

	// gets how many agents are in a specified group
	virtual int GetGroupCount(int nGroupID, int flags = IAISystem::GROUP_ALL, int type = 0);

	// adds new combat class, with target selection scales
	virtual void AddCombatClass(int combatClass, float* pScalesVector, int size, const char* szCustomSignal);
	
	// AIHandler calls this function to replace the OnPlayerSeen signal
	virtual const char* GetCustomOnSeenSignal(int combatClass);

	// adds fading in-out sphere to obstruct AI perception (smog grenades, etc) 
	virtual void AddObstructSphere( const Vec3& pos, float radius, float lifeTime, float fadeTime);

	// Hostility restriction
	virtual void SetupHostilityRange(IEntity* pAffected, int species, float min_range, float max_range, float degen, float increase, float cooldown);
	virtual void AggressionImpact(IEntity* pAttacker, IEntity* pTarget);
	virtual void IgnoreSpecies(IEntity* pAffected, int species, bool ignore);

	// Draws a fake tracer around the player.
	virtual void DebugDrawFakeTracer(const Vec3& pos, const Vec3& dir);

	// Process damage with balacing calculations.
	virtual float ProcessBalancedDamage(IEntity* pShooterEntity, IEntity* pTargetEntity, float damage, const char* damageType);

	// // loads the triangulation for this level and mission
	virtual void LoadNavigationData(const char * szLevel, const char * szMission);
	// Gets called after loading the mission
	virtual void OnMissionLoaded();

	/// Creates a shape object - called by editor
	virtual bool CreateNavigationShape(const SNavigationShapeParams &params);
	/// Deletes designer created path/shape - called by editor
	virtual void DeleteNavigationShape(const char * szName);
	/// Checks if navigation shape exists - called by editor
	virtual bool DoesNavigationShapeExists(const char * szName, EnumAreaType areaType, bool road = false);

	// calculate a point on a path which has the nearest distance from the given point.
	// Also returns segno
	// strict even if t == 0 or t == 1.0
	virtual	float GetNearestPointOnPath( const char * szPathName, const Vec3& vPos, Vec3& vResult, bool& bLoop, float& totalLength );
	virtual	const std::vector<Vec3> *GetPath(const char *szPathName);
	virtual	bool GetNearestPointOnPathReally( const char * szPathName, const Vec3& pos, float& dist, Vec3& nearestPt, float* distAlongPath = 0 );
	virtual void GetPointOnPathBySegNo( const char * sPathName, Vec3& vResult, float segNo );

	/// Returns nearest designer created path/shape.
	/// The devalue parameter specifies how long the path will be unusable by others after the query.
	/// If useStartNode is true the start point of the path is used to select nearest path instead of the nearest point on path.
	virtual const char*	GetNearestPathOfTypeInRange(IAIObject* requester, const Vec3& pos, float range, int type, float devalue, bool useStartNode);

	/// Returns the first shape of specified type that encloses the specified position.
	/// If checkHeight is true a check is done to see if the point is between shape.aabb.min.z && shape.aabb.min.z+height.
	virtual const char*	GetEnclosingGenericShapeOfType(const Vec3& pos, int type, bool checkHeight);
	/// Returns the true if the specified position is inside the specified shape.
	/// If checkHeight is true a check is done to see if the point is between shape.aabb.min.z && shape.aabb.min.z+height.
	virtual bool IsPointInsideGenericShape(const Vec3& pos, const char* shapeName, bool checkHeight);
	/// Constraints the specified point inside the specified generic shape. Returns true if the point is clamped.
	/// If checkHeight is specified, the height is clamped between shape.aabb.min.z && shape.aabb.min.z+height.
	virtual bool ConstrainInsideGenericShape(Vec3& pos, const char* shapeName, bool checkHeight);
	/// Returns distance to a specified generic shape.
	virtual float DistanceToGenericShape(const Vec3& pos, const char* shapeName, bool checkHeight);
	/// Creates a temporary generic shape, returns the name of the newly created shape or null if shape is not valid.
	virtual const char*	CreateTemporaryGenericShape(Vec3* points, int npts, float height, int type);
	/// Enables or disables a generic shape.
	virtual void EnableGenericShape(const char* shapeName, bool state);
	/// Returns shape of specified name, null if the shape does not exists.
	SShape*	GetGenericShapeOfName(const char* shapeName);
	const ShapeMap &GetGenericShapes() const {return m_mapGenericShapes;}

	/// Enables or disables AI inside a generic shape.
	virtual void AIActivtyInShape(EAITerritoryAction action, const char* shapeName, int species, EAINavigationFilter nav_filter);

	// Returns specified number of nearest hidespots. It considers the hidespots in graph and anchors.
	// Any of the pointers to return values can be null. Returns number of hidespots found.
	virtual unsigned int GetHideSpotsInRange(IAIObject* requester, const Vec3& reqPos,
		const Vec3& hideFrom, float minRange, float maxRange, bool collidableOnly, bool validatedOnly,
		unsigned int maxPts, Vec3* coverPos, Vec3* coverObjPos, Vec3* coverObjDir, float* coverRad, bool* coverCollidable);

	// Returns a point which is a valid distance away from a wall in front of the point.
	virtual void	AdjustDirectionalCoverPosition(Vec3& pos, const Vec3& dir, float agentRadius, float testHeight);
	// Returns a point which is a behind a cover object.
	virtual void	AdjustOmniDirectionalCoverPosition(Vec3& pos, Vec3& dir, float hideRadius, float agentRadius, const Vec3& hideFrom, const bool hideBehind=true);

	virtual void SetUseAutoNavigation(const char *szPathName, EWaypointConnections waypointConnections);

	/// The AI system will automatically reconnect nodes in this building ID 
	/// and recreate area/vertex information (only affects automatically
	/// generated links)
	virtual void ReconnectWaypointNodesInBuilding(int nBuildingID);

	virtual bool GetBuildingInfo(int nBuildingID, SBuildingInfo& info);
	virtual bool IsPointInBuilding(const Vec3& pos, int nBuildingID);

	/// Automatically reconnect all waypoint nodes in all buildings,
	/// and disconnects nodes not in buildings (only affects automatically
	/// generated links)
	virtual void ReconnectAllWaypointNodes();

	/// generate the triangulation for this level and mission
	virtual void GenerateTriangulation(const char * szLevel, const char * szMission);
	/// generate 3d nav volumes for this level and mission
	virtual void Generate3DVolumes(const char * szLevel, const char * szMission);
	/// generate flight navigation for this level and mission
	virtual void GenerateFlightNavigation(const char * szLevel, const char * szMission);

  /// inherited
  virtual void Generate3DDebugVoxels();

	virtual void ValidateNavigation();

  // inherited
  virtual void ModifyNavCostFactor(const char *navModifierName, float factor);

  // inherited
  virtual const std::vector<string> & GetVolumeRegionFiles(const char * szLevel, const char * szMission);

	virtual void ExportData(const char * navFileName, const char * hideFileName, const char * areasFileName, const char* roadsFileName, const char *waypoint3DSurfaceFileName);
  // inherited
  virtual void RegisterDamageRegion(const void *pID, const Sphere &sphere);

	virtual void SoundEvent(const Vec3 &pos, float fRadius, EAISoundEventType type, IAIObject* pObject);
	virtual void CollisionEvent(const Vec3& pos, const Vec3& vel, float mass, float radius, float impulse, IEntity* pCollider, IEntity* pTarget);
	virtual void IgnoreCollisionEventsFrom(EntityId ent, float time);
	virtual void BreakEvent(const Vec3& pos, float breakSize, float radius, float impulse, IEntity* pCollider);
	virtual	void ExplosionEvent(const Vec3& pos, float radius, IAIObject* pShooter);
	virtual	void BulletHitEvent(const Vec3& pos, float radius, float soundRadius, IAIObject* pShooter);
	virtual	void BulletShotEvent(const Vec3& pos, const Vec3& dir, float range, IAIObject* pShooter);
	virtual	void DynOmniLightEvent(const Vec3& pos, float radius, EAILightEventType type, IAIObject* pShooter, float time = 5.0f);
	virtual	void DynSpotLightEvent(const Vec3& pos, const Vec3& dir, float radius, float fov, EAILightEventType type, IAIObject* pShooter, float time = 5.0f);
	virtual void GrenadeEvent(const Vec3& pos, float radius, EAIGrenadeEventType type, IEntity* pGrenade, IEntity* pShooter);

	virtual void RegisterAIEventListener(IAIEventListener* pListener, const Vec3& pos, float rad, int flags);
	virtual void UnregisterAIEventListener(IAIEventListener* pListener);

	virtual IAutoBalance * GetAutoBalanceInterface(void);
	virtual void SetSpeciesThreatMultiplier(int nSpeciesID, float fMultiplier);
	virtual void DumpStateOf(IAIObject * pObject);

  virtual bool IsAIInDevMode();

	virtual void	SetPerceptionDistLookUp( float* pLookUpTable, int tableSize );
 // virtual float GetDetectionValue(IAIObject * pObject);
//	virtual float GetAmbientDetectionValue(IAIObject *pObject);

	// Returns the exposure and threat levels as perceived by the specified AIobject.
	// If the AIobject is null, the local player values are returns
	virtual void GetDetectionLevels(IAIObject *pObject, SAIDetectionLevels& level);

	virtual int GetAITickCount(void);
	virtual void SendAnonymousSignal(int nSignalID, const char * szText, const Vec3 & vPos, float fRadius, IAIObject *pObject, IAISignalExtraData* pData=NULL);

	virtual IAIObject * GetBeacon(unsigned short nGroupID);
	int GetBeaconGroupId(CAIObject* pBeacon);
	virtual void UpdateBeacon(unsigned short nGroupID, const Vec3 & vPos, IAIObject *pOwner);

	virtual void SetAssesmentMultiplier(unsigned short type, float fMultiplier);

	// current global AI alertness value (what's the most alerted puppet)
	virtual int GetAlertness() const;


	// functions to let external systems access the AI logging functions - don't
	// use directly from in AI. Pass in a character prefix - e.g. "<Lua> ".
	virtual void Warning(const char * id, const char * format, ...) PRINTF_PARAMS(3, 4);
	virtual void Error(const char * id, const char * format, ...) PRINTF_PARAMS(3, 4);
	virtual void LogProgress(const char * id, const char * format, ...) PRINTF_PARAMS(3, 4);
	virtual void LogEvent(const char * id, const char * format, ...) PRINTF_PARAMS(3, 4);
	virtual void LogComment(const char * id, const char * format, ...) PRINTF_PARAMS(3, 4);

	// smart objects
	virtual void ReloadSmartObjectRules();
	CSmartObjects* GetSmartObjects();
	bool LoadSmartObjectsLibrary();
	virtual void AddSmartObjectCondition( const SmartObjectCondition& condition );
	virtual const char* GetSmartObjectStateName( int id ) const;
	virtual SmartObjectHelper* GetSmartObjectHelper( const char* className, const char* helperName ) const;
	virtual void RegisterSmartObjectState( const char* sStateName );
	virtual void SetSmartObjectState( IEntity* pEntity, const char* sStateName );
	virtual void AddSmartObjectState( IEntity* pEntity, const char* sState );
	virtual void RemoveSmartObjectState( IEntity* pEntity, const char* sState );
	virtual void ModifySmartObjectStates( IEntity* pEntity, const char* listStates );
	virtual bool CheckSmartObjectStates( IEntity* pEntity, const char* patternStates ) const;

	virtual void GetSOClassTemplateIStatObj( IEntity* pEntity, IStatObj**& pStatObjects ) const
	{
		pStatObjects = NULL;
		if ( m_pSmartObjects )
		{
			m_pSmartObjects->RescanSOClasses( pEntity );
			static std::vector< IStatObj* > statObjects;
			m_pSmartObjects->GetTemplateIStatObj( pEntity, statObjects );
			if ( !statObjects.empty() )
				pStatObjects = &statObjects[0];
		}
	}
	virtual bool ValidateSOClassTemplate( IEntity* pEntity, bool bStaticOnly )
	{
		return m_pSmartObjects ? m_pSmartObjects->ValidateTemplate( pEntity, bStaticOnly ) : false;
	}
	virtual void DrawSOClassTemplate( IEntity* pEntity ) const
	{
		if ( m_pSmartObjects )
			m_pSmartObjects->DrawTemplate( pEntity );
	}

	CAILightManager* GetLightManager() { return &m_lightManager; }
	CAIDynHideObjectManager* GetDynHideObjectManager() { return &m_dynHideObjectManager; }

	// non-virtual, accessed from CAIObject.
	// notifies that AIObject has changed its position, which is important for smart objects
	void NotifyAIObjectMoved( IEntity* pEntity, SEntityEvent event );

	// used by COPTrace to use smart objects in navigation
	bool PrepareNavigateSmartObject( CPipeUser* pPipeUser, const GraphNode* pFromNode, const GraphNode* pToNode );
	// used by COPTrace to use smart objects in navigation
	int GetNavigationalSmartObjectActionType( CPipeUser* pPipeUser, const GraphNode* pFromNode, const GraphNode* pToNode );

	int FindSmartObjectLinks( const CPipeUser* pPipeUser, CSmartObject* pObject, CSmartObjectClass* pObjectClass,
		SmartObjectHelper* pFromHelper, SmartObjectHelper* pToHelper, CSmartObjectClass::HelperLink** pvHelperLinks, int iCount = 1 ) const;

	// AI Actions
	virtual void ReloadActions();
	virtual void ExecuteAIAction( const char* sActionName, IEntity* pUser, IEntity* pObject, int maxAlertness, int goalPipeId );
	virtual void AbortAIAction( IEntity* pUser, int goalPipeId );
	virtual IAIAction* GetAIAction( const char* name );
	virtual IAIAction* GetAIAction( size_t index );
	virtual const char* GetAIActionName( int index ) const;

	// smart object events
	virtual int SmartObjectEvent( const char* sEventName, IEntity*& pUser, IEntity*& pObject, const Vec3* pExtraPoint = NULL, bool bHighPriority = false );
	
	virtual int AllocGoalPipeId() const;
	virtual IAISignalExtraData* CreateSignalExtraData() const;
	virtual void FreeSignalExtraData( IAISignalExtraData* pData ) const;

	//====================================================================
	// 	Pathfinding
	//====================================================================
	CGraph * GetGraph(bool hide = false);
	/// Submits a pathfinding request. If allowDangerousDestination is false then no path will be found
	/// if the destination is "dangerous" - i.e. close to a forbidden boundary etc. It is possible to force
	/// the destination to be a waypoint inside a building by making forceTargetBuildingId >= 0
  /// endTol indicates how much the end of the path is allowed to deviate from that requested before the
  /// request is rejected (path failed)
	void RequestPathTo(const Vec3 &start, const Vec3 &end, const Vec3 &endDir, CPipeUser *pRequester, 
		bool allowDangerousDestination, int forceTargetBuildingId, float endTol, float endDistance);
  /// Submits a pathfinding request to travel as far as possible up to a max dist
  /// towards the specified position.
  void RequestPathInDirection(const Vec3 &start, const Vec3 &pos, float maxDist, CPipeUser *pRequester, float endDistance);

	int RequestRawPathTo(const Vec3 &start, const Vec3 &end, float passRadius, IAISystem::tNavCapMask navCapMask,
		unsigned& lastNavNode, bool allowDangerousDestination, float endTol, const CStandardHeuristic::ExtraConstraints& constraints, CPipeUser *pReference = 0);

	void RegisterPathFinderListener(IAIPathFinderListerner* pListener);
	void UnregisterPathFinderListener(IAIPathFinderListerner* pListener);
	void NotifyPathFinderListeners(int id, const std::vector<unsigned>* pathNodes);

	const CNavPath & GetNavPath() const {return m_navPath;};
	const PathFindRequest * GetPathFindCurrentRequest() const { return m_pCurrentRequest; };
	// Gets the special area with a particular building ID - may return 0 if it cannot be found
	const SpecialArea * GetSpecialArea(int buildingID) const;
	void InsertSpecialArea(const string & name, SpecialArea & sa);
	void EraseSpecialArea(const string & name);
	void FlushSpecialAreas();
	void FlushAllAreas();
  const SpecialAreaMap &GetSpecialAreas() const {return m_mapSpecialAreas;}
  const SpecialArea *GetSpecialArea(const Vec3 &pos, SpecialArea::EType areaType);
  const SpecialArea *GetSpecialAreaNearestPos(const Vec3 &pos, SpecialArea::EType areaType);

  /// Indicates if a human would be visible if placed at the specified position
  /// If fullCheck is false then only a camera frustum check will be done. If true
  /// then more precise (perhaps expensive) checking will be done.
  bool WouldHumanBeVisible(const Vec3 &footPos, bool fullCheck) const;

	/// Returns true if the given AIObject can navigate to vEndPos within a reasonable cost
	/// the function computes an estimation of the path and the path length, the cost is reasonable
	/// when the returned point (pReturnPos) is close to the end point, given the constraint that the cost
	/// is at maximum (maxLengthCoeff)*distance between start and end pos
	bool	IsPathWorth(const CAIActor* pObject, const Vec3 & vStartpos, Vec3 vEndPos, float maxLengthCoeff, float percent = 0.4f,Vec3* pReturnPos = 0);

  typedef std::vector< std::pair<string, const SpecialArea*> > VolumeRegions;
  /// Fills in the container of special areas that relate to 3D volumes
  void GetVolumeRegions(VolumeRegions& volumeRegions);
	// copies a designer path into provided list if a path of such name is found
	bool GetDesignerPath(const char * szName, SShape &path) const;
	const ShapeMap& GetDesignerPaths() const { return m_mapDesignerPaths; }
	void CancelAnyPathsFor(CPipeUser* pRequester, bool actorRemoved = false);
	void CancelPath(int id);
  bool IsFindingPathFor(const CAIObject * pRequester) const;
  /// This is just for debugging
  const char *GetNavigationShapeName(int nBuildingID) const;

	/// Adds object to a list of dead bodies to avoid.
	void RegisterDeadBody(unsigned int species, const Vec3& pos);
	/// Fills the array with possible dangers, returns number of dangers.
	virtual unsigned int GetDangerSpots(const IAIObject* requester, float range, Vec3* positions, unsigned int* types, unsigned int n, unsigned int flags);

	/// Checks for each point in the array if it is inside path obstacles of the specified requester. The corresponding result
	/// value will be set to true if the point is inside obstacle. Returns number of points inside the obstacles.
	virtual unsigned int CheckPointsInsidePathObstacles(IAIObject* requester, const Vec3* points, bool* results, unsigned int n);

  /// Returns the set of navigation blockers last used in A* (may be a little out of date)
  const NavigationBlockers& GetNavigationBlockers() const {return m_navigationBlockers;}

	/// used by heuristic to check is this link passable by current pathfind requester
	float GetSmartObjectLinkCostFactor( const GraphNode* nodes[2], const CAIObject *pRequester ) const;

  /// Indicates if a point is in a special area, checking the height too
  static bool IsPointInSpecialArea(const Vec3 &pos, const SpecialArea &sa);

  /// returns true if pos is inside a TRIANGULATION nav modifier, or if there are no
  /// such modifiers.
  /// NOTE this must only be called in editor (since it shouldn't be needed in game) - will assert that
  /// this is the case!
  bool IsPointInTriangulationAreas(const Vec3 &pos);

	/// Attempts to generate an almost straight path between the current request
	/// start and end up to a maxCost. 
  /// If unsuccessful -1 will be returned. If successful 
	/// straightPath will be non-empty and the approximate
	/// cost will be returned. The straightPath will contain the start position, but no 
	/// points behind the requester's current position (as in BeautifyPath) which is
  /// specified by curPos. If returnPartialPath is set then success will be returned and 
  /// the path will go as far as it can towards the destination
  float AttemptStraightPath(TPathPoints &straightPath, const PathFindRequest &request, 
    const Vec3 &curPos, const CHeuristic* pHeuristic, float maxCost,
    const NavigationBlockers& navigationBlockers, bool returnPartialPath);

  /// Attempts to generate an almost straight (and simple) path in a certain direction, up to length
  /// maxDist. Returns the actual length of the path (-1 if there were no path points added)
  float AttemptPathInDir(TPathPoints &path, const Vec3 &curPos, const Vec3 &dir, const PathFindRequest &request, 
    const CHeuristic* pHeuristic, float maxCost, const NavigationBlockers& navigationBlockers);

	//====================================================================
	// 	Other
	//====================================================================
	CAIObject * CreateDummyObject(const string &name="", CAIObject::ESubTypes type = CAIObject::STP_NONE);
	void RemoveDummyObject(CAIObject *pObject);
	CAIObject * GetAIObjectByName(const char *pName) const;

	// gets the I-th agent in a specified group
	IAIObject* GetGroupMember(int nGroupID, int nIndex, int flags = GROUP_ALL, int type = 0);

	int GetAlertStatus(const IAIObject* pObject)  ;
	// removes specified object from group
	void RemoveFromGroup(int nGroupID, CAIObject * pObject);
	// adds an object to a group
	void AddToGroup(CAIActor * pObject,int nGroupId = -1);
	// adds an object to a species
	void AddToSpecies(CAIObject * pObject, unsigned short nSpecies);
	// Sends a signal using the desired filter to the desired agents
	void SendSignal(unsigned char cFilter, int nSignalId,const char *szText,  IAIObject *pSenderObject, IAISignalExtraData* pData=NULL);

	void NotifyEnableState(CPuppet* pPuppet, bool state);

	// creates a formation 
	CFormation *CreateFormation(CAIObject* pOwner, const char * szFormationName, Vec3 vTargetPos = ZERO);

	void ReleaseFormation(CAIObject* pOwner, bool bDelete);

	// changes the formation type for the given group id
	bool ChangeFormation(IAIObject* pOwner, const char * szFormationName,float fScale, bool force);

	// changes the formation's scale factor for the given group id
	bool ScaleFormation(IAIObject* pOwner, float fScale);
	
	// set the update flag of the formation
	bool SetFormationUpdate(IAIObject* pOwner, bool bUpdate);

	void ReleaseFormationPoint(CAIObject * pReserved);

	// changes the formation's scale factor for the given group id
	bool SameFormation(const CPuppet* pHuman, const CAIVehicle* pVehicle);

	void UpdateGroupStatus(int groupId);

	IPhysicalWorld * GetPhysicalWorld();
		
	CAIObject * GetPlayer() const;

	CGoalPipe *IsGoalPipe(const string &name);

	/// Triangular nav region may be associated with more than one graph
	CTriangularNavRegion* GetTriangularNavRegion(const class CGraph* pGraph) {return m_pGraph == pGraph ? m_pTriangularNavRegion : m_pTriangularHideRegion;}
	CWaypointHumanNavRegion* GetWaypointHumanNavRegion() {return m_pWaypointHumanNavRegion;}
	CWaypoint3DSurfaceNavRegion* GetWaypoint3DSurfaceNavRegion() {return m_pWaypoint3DSurfaceNavRegion;}
	CFlightNavRegion* GetFlightNavRegion() {return m_pFlightNavRegion;}
	CVolumeNavRegion* GetVolumeNavRegion() {return m_pVolumeNavRegion;}
	CRoadNavRegion* GetRoadNavRegion() {return m_pRoadNavRegion;}
	CFree2DNavRegion* GetFree2DNavRegion() {return m_pFree2DNavRegion;}
	CSmartObjectNavRegion* GetSmartObjectsNavRegion() {return m_pSmartObjectNavRegion;}

  /// Returns the base-class nav region - needs the graph only if type is waypoint (so you can
  /// pass in 0 if you know it's not... but be careful!)
  class CNavRegion *GetNavRegion(IAISystem::ENavigationType type, const class CGraph *pGraph);

	CTriangularNavRegion* GetTriangularHideRegion() {return m_pTriangularHideRegion;}

	/// Returns positions of currently occupied hide point objects excluding the requesters hide spot.
	void	GetOccupiedHideObjectPositions(const CPipeUser* pRequester, std::vector<Vec3>& hideObjectPositions);


  /// Finds all hidespots (and their path range) within path range of startPos, along with the navigation graph nodes traversed.
  /// Each hidespot contains info about where it came from. If you want smart-object hidespots you need to pass in an entity
  /// so it can be checked to see if it could use the smart object. pLastNavNode/pLastHideNode are just used as a hint.
  /// Smart Objects are only considered if pRequester != 0
  MultimapRangeHideSpots &GetHideSpotsInRange(MultimapRangeHideSpots &result, MapConstNodesDistance &traversedNodes, const Vec3 & startPos, float maxDist, 
    IAISystem::tNavCapMask navCapMask, float passRadius, bool skipNavigationTest,
    IEntity* pSmartObjectUserEntity = 0, unsigned lastNavNodeIndex = 0, unsigned lastHideNodeIndex = 0,
    const class CAIObject *pRequester = 0);

	/// Returns obstacles within specified radius.
	/// This function return obstacle within euclidean distance, not distance along the navigation graph.
	/// The obstacles are returned as <position, radius> pair. This function should be faster then using
	/// the graph search to query the vertices. Triangular obstacles only. See EGetObstaclesInRadiusFlags.
	void GetObstaclePositionsInRadius(const Vec3& pos, float radius, float minHeight, int flags, std::vector<std::pair<Vec3, float> >& positions);

  /// Returns true if all the links leading out of the node have radius < fRadius
	bool ExitNodeImpossible(CGraphLinkManager& linkManager, const GraphNode * pNode, float fRadius) const;

  /// Returns true if all the links leading into the node have radius < fRadius
  bool EnterNodeImpossible(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode * pNode, float fRadius) const;

  /// Returns the extra link cost associated with the link by intersecting against the appropriate
  /// shapes. 0.0 means no shapes were found.
  float GetExtraLinkCost(const Vec3 &pos1, const Vec3 &pos2) const;

  /// indicates if a linesegment would intersect the extra cost areas
  bool OverlapExtraCostAreas(const Lineseg & lineseg) const;

  /// if there's intersection vClosestPoint indicates the intersection point, and the edge normal
  /// is optionally returned. If bForceNormalOutwards is set then in the case of forbidden
  /// boundaries this normal is chosen to point (partly) towards vStart.
	/// nameToSkip can optionally point to a string indicating a forbidden area area to not check
  /// mode indicates if areas and/or boundaries should be checked
	virtual bool IntersectsForbidden(const Vec3 & vStart, const Vec3 & vEnd, Vec3 & vClosestPoint, const string * nameToSkip = 0,Vec3* pNormal = NULL, 
    EIFMode mode = IF_AREASBOUNDARIES, bool bForceNormalOutwards=false) const;
	bool IntersectsSpecialArea(const Vec3 & vStart, const Vec3 & vEnd, Vec3 & vClosestPoint);

	virtual bool IsSegmentValid(IAISystem::tNavCapMask navCap, float rad, const Vec3& posFrom, Vec3& posTo, IAISystem::ENavigationType& navTypeFrom);

	virtual Vec3 IsFlightSpaceVoid(const Vec3 &vPos, const Vec3 &vFwd, const Vec3 &vWng, const Vec3 &vUp ); // IAISystem implementation
	virtual Vec3 IsFlightSpaceVoidByRadius(const Vec3 &vPos, const Vec3 &vFwd, float radius );
	/// Returns true if a point is in a forbidden region. When two forbidden regions
	/// are nested then it is just the region between them that is forbidden. This
	/// only checks the forbidden areas, not the boundaries.
	virtual bool IsPointInForbiddenRegion(const Vec3 & pos, bool checkAutoGenRegions = true) const; // IAISystem implementation
  /// Optionally returns the forbidden region the point is in (if it is in a forbidden region)
	bool IsPointInForbiddenRegion(const Vec3 & pos, const CAIShape** ppShape, bool checkAutoGenRegions) const; // internal use
	/// Returns true if a point is within a forbidden boundary. This
	/// only checks the forbidden boundaries, not the areas.
	bool IsPointInForbiddenBoundary(const Vec3 & pos, const CAIShape** ppShape = 0) const;
	/// Get the best point outside any forbidden region given the input point, 
	/// and optionally a start position to stay close to
	Vec3 GetPointOutsideForbidden(Vec3& pos, float distance, const Vec3* startPos = 0);
  /// Returns true if the point lies within one of the regions marked out for dynamic triangular
  /// updates
  bool IsPointInDynamicTriangularAreas(const Vec3 &pos) const;
  /// Returns true if a shape is completely inside forbidden regions. Pass in the AABB for the shape
  template<typename VecContainer>
  bool IsShapeCompletelyInForbiddenRegion(VecContainer &shape, bool checkAutoGenRegions) const;
	/// Returns true if it is impossible (assuming path finding is OK) to get from start 
	/// to end without crossing a forbidden boundary (except for moving out of a 
	/// forbidden region). 
	bool IsPathForbidden(const Vec3 & start, const Vec3 & end);
  /// Returns true if the point is inside the nav modifiers marked out as containing water
  bool IsPointInWaterAreas(const Vec3 &pt) const;

	/// Returns true if a point is on/close to a forbidden boundary/area edge
	bool IsPointOnForbiddenEdge(const Vec3& pos, float tol = 0.0001f, Vec3* pNormal = 0, const CAIShape** ppPolygon = 0, bool checkAutoGenRegions = true);

	/// Returns true if a point is inside forbidden boundary/area edge or close to its edge
	bool IsPointForbidden(const Vec3 & pos, float tol, Vec3* pNormal = 0, const CAIShape** ppShape = 0);

  /// Returns the list of pass radii (sorted from big to small) that will use 3D navigation
  const std::vector<float> & Get3DPassRadii() const {return m_3DPassRadii;}

  typedef std::map<const void *, Sphere> TDamageRegions;
  /// Returns the regions game code has flagged as causing damage
  const TDamageRegions &GetDamageRegions() const {return m_damageRegions;}

	bool CheckPointsVisibility(const Vec3& from, const Vec3& to, float rayLength, IPhysicalEntity* pSkipEnt=0, IPhysicalEntity* pSkipEntAux=0);
	bool CheckObjectsVisibility(const IAIObject *pObj1, const IAIObject *pObj2, float rayLength);


	// it removes all references to this object from all objects of the specified type
	void RemoveObjectFromAllOfType(int nType, CAIObject* pRemovedObject);
	void NotifyTargetDead(CAIObject* pDeadObject);

	int GetNumberOfObjects(unsigned short type);
	bool ThroughVehicle(const Vec3 & start, const Vec3 & end);

	bool			RequestVehicle(IAIObject* pRequestor, Vec3 &vSourcePos, IAIObject *pAIObjectTarget, const Vec3& vTargetPos, int iGoalType);
	TAIObjectList*	GetVehicleList(IAIObject* pRequestor);

	/// Validation function - checks if a CAIObject exists and is registered with the AI system.
	/// Note that this does NOT call into pObject, so it can be used for debugging safely (makes
	/// it potentially slow)
	bool IsAIObjectRegistered(const CAIObject* pObject) const;

	// CActiveAction needs to access action manager
	CAIActionManager* GetActionManager() const { return m_pActionManager; }

	//combat classes scale
	float	GetCombatClassScale(int shooterClass, int targetClass);

	/// true it pos is withing any of registered explosions.
	bool IsPointAffectedByExplosion(const Vec3 & pos) const;

	float	GetVisPerceptionDistScale( float distRatio );	//distRatio (0-1) 1-at sight range

	static bool ValidateSONavData( const CNavPath& navPath );

	// returns value in range [0, 1]
	// calculate how much targetPos is obstructed by water
	// takes into account water depth and density
	float	GetWaterOcclusionValue(const Vec3& targetPos) const;

	// Calculates a static row formation for group of puppets.
	struct SAIStaticFormation
	{
		SAIStaticFormation() :
			debugOcclusionObjectCount(0), error(ERR_NONE),
			debugPointsOutsideTerritory(0),
			debugPointsInsideObstacle(0),
			debugPointsInsideForbiddenArea(0),
			debugPointsInsidePathObstacles(0)
		{
		}

		enum EType
		{
			AISF_ROW,
			AISF_LINE,
		};
		enum ELocation
		{
			AISF_NEAREST,
			AISF_NEAREST_LEFT,
			AISF_NEAREST_RIGHT,
			AISF_BEST,
		};
		enum EError
		{
			ERR_NONE,
			ERR_GROUP_EMPTY,
			ERR_NO_HIDESPOTS,
			ERR_FORMATION_POINT_NOT_FOUND,
			ERR_ISLANDS_NOT_FOUND,
		};

		std::vector<Vec3> formationPoints;
		std::vector<Vec3> desiredFormationPoints;
		std::vector<int> order;

		struct SDebugPoint
		{
			inline void Set(const Vec3& pos_, float w_) { pos = pos_; w = w_; }
			Vec3 pos;
			float w;
		};
		struct SDebugIsland
		{
			SDebugIsland(float smin, float smax, float mid, float score) : smin(smin), smax(smax), mid(mid), score(score) {}
			float smin, smax;
			float mid;
			float score;
		};
		std::vector<SDebugPoint> debugPoints;
		std::vector<SDebugIsland> debugIslands;
		Vec3 debugForw, debugRight;
		Vec3 debugAdvanceSpot;
		Vec3 debugSearchPos;
		std::vector<float> debugHistogram;
		CAIRadialOcclusion debugOcclusion;
		int debugOcclusionObjectCount;
		int debugPointsOutsideTerritory;
		int debugPointsInsideObstacle;
		int debugPointsInsideForbiddenArea;
		int debugPointsInsidePathObstacles;
		EError error;
	};

	bool CreateStaticFormation(SAIStaticFormation::EType type, SAIStaticFormation::ELocation location,
		const std::vector<CPuppet*>& group, const Vec3& targetPos, const Vec3* pPivotPos,
		float offsetTowardsTarget, float minDistanceToTarget, float searchRange, float spacing,
		std::vector<std::pair<Vec3, float> >& otherGroups, CAIRadialOcclusionRaycast* occlusion,
		SAIStaticFormation& outFormation);

	bool CheckVisibilityToBody(CPuppet* pObserver, CAIActor* pBody, float& closestDistSq, IPhysicalEntity* pSkipEnt = 0);

	//====================================================================
	// debug recorder
	//====================================================================
	virtual bool IsRecording( const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event ) const
	{
		return m_DbgRecorder.IsRecording( pTarget, event );
	}
	virtual void Record( const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event, const char* pString ) const
	{
		m_DbgRecorder.Record( pTarget, event, pString );
	}

	virtual void	SetDrawRecorderRange(float start, float end, float cursor)
	{
		m_recorderDrawStart = start;
		m_recorderDrawEnd = end;
		m_recorderDrawCursor = cursor;
	}

//	void	DebugReportPlayerShotAt(IAIObject* pShooter, bool miss, const char* material);
//	void	DumpPlayerDamageReport();
	virtual void	DebugReportHitDamage(IEntity* pVictim, IEntity* pShooter, float damage, const char* material);
	virtual void	DebugReportDeath(IAIObject* pVictim);

	virtual void	AddDebugLine(const Vec3& start, const Vec3& end, uint8 r, uint8 g, uint8 b, float time);
	virtual void	AddDebugBox(const Vec3& pos, const OBB& obb, uint8 r, uint8 g, uint8 b, float time);
	virtual void	AddDebugSphere(const Vec3& pos, float radius, uint8 r, uint8 g, uint8 b, float time);

	// Get the interest system
	ICentralInterestManager* GetCentralInterestManager(void);

	virtual IAIRecorder * GetIAIRecorder(void) {return &m_Recorder;}

	struct SSOUser
	{
		IEntity* entity;
		float radius;
		float height;
		float groundOffset;
	};
	struct SSOTemplateArea
	{
		Vec3 pt;
		float radius;
		bool projectOnGround;
	};
	static bool ValidateSmartObjectArea(const SSOTemplateArea &templateArea, const SSOUser &user, EAICollisionEntities collEntities, Vec3 &groundPos);

	inline const VectorSet<CPuppet*>& GetEnabledPuppetSet() const { return m_enabledPuppetsSet; }
	inline const VectorSet<CPuppet*>& GetDisabledPuppetSet() const { return m_disabledPuppetsSet; }

	//====================================================================
	// Public data
	//====================================================================

	ISystem *m_pSystem;

	AIObjects m_Objects;

	float	m_fAutoBalanceCollectingTimeout;
	bool	m_bCollectingAllowed;

	AIObjects m_mapGroups;
//	AIObjects m_mapLeaders;
	AIObjects m_mapSpecies;
	MapMultipliers	m_mapMultipliers;
	MapMultipliers	m_mapSpeciesThreatMultipliers;

	TimingMap m_mapDEBUGTiming;
	TimingMap m_mapDEBUGTimingGOALS;

	CVertexList m_VertexList;

	CVehicleFinder* m_pVehicleFinder;

	int	m_AlertnessCounters[4];

  ICVar *m_cvUseCalculationStopperCounter;
	ICVar *m_cvAllowedTimeForPathfinding;
	ICVar *m_cvPathfinderUpdateTime;
	ICVar *m_cvDrawAgentFOV;
	ICVar *m_cvAgentStatsDist;
	ICVar *m_cvAiSystem;
	ICVar *m_cvDebugDraw;
  ICVar *m_cvDebugDrawHideSpotRange;
	ICVar *m_cvDebugDrawDynamicHideObjectsRange;
  ICVar *m_cvDebugDrawVolumeVoxels;
	ICVar *m_cvDebugDrawDeadBodies;
  ICVar *m_cvDebugPathFinding;
	ICVar *m_cvDebugDrawAStarOpenList;
	ICVar *m_cvDebugDrawBannedNavsos;
	ICVar *m_cvSoundPerception;
	ICVar *m_cvIgnorePlayer;
  ICVar *m_cvIgnoreVisibilityChecks;
	ICVar *m_cvDrawPath;				
  ICVar *m_cvDrawPathAdjustment;
	ICVar *m_cvDrawModifiers;
	ICVar *m_cvAllTime;				
	ICVar *m_cvProfileGoals;				
	ICVar *m_cvRunAccuracyMultiplier;				
	ICVar *m_cvTargetMovingAccuracyMultiplier;
	ICVar *m_cvLateralMovementAccuracyDecay;
	ICVar *m_cvDrawHide;
	ICVar *m_cvBeautifyPath;
	ICVar *m_cvAttemptStraightPath;
  ICVar *m_cvPredictivePathFollowing;
  ICVar *m_cvCrowdControlInPathfind;
	ICVar *m_cvDebugDrawCrowdControl;
	ICVar *m_cvOptimizeGraph;
	ICVar *m_cvUpdateProxy;
	ICVar *m_cvDrawType;
	ICVar *m_cvStatsTarget;
	ICVar *m_cvAIUpdateInterval;
	ICVar *m_cvDynamicWaypointUpdateTime;
	ICVar *m_cvDynamicTriangularUpdateTime;
	ICVar *m_cvDynamicVolumeUpdateTime;
  ICVar *m_cvSmartObjectUpdateTime;
  ICVar *m_cvAdjustPathsAroundDynamicObstacles;
	ICVar *m_cvAIVisRaysPerFrame;
	ICVar *m_cvDrawRefPoints; //FP_Vehicle
	ICVar *m_cvDrawFormations;
	ICVar *m_cvDrawPatterns;
	ICVar *m_cvDrawSmartObjects;
	ICVar *m_cvDrawReadibilities;
	ICVar *m_cvDrawGoals;
 	ICVar *m_cvDrawNode;
	ICVar *m_cvDrawNodeLinkType;
	ICVar *m_cvDrawNodeLinkCutoff;
	ICVar *m_cvDrawLocate;
	ICVar *m_cvDrawOffset;
	ICVar *m_cvDrawTargets;
//	ICVar *m_cvAILeaderDies;
	ICVar *m_cvDrawBadAnchors;
	ICVar *m_cvDrawStats;
	ICVar *m_cvSteepSlopeUpValue;
	ICVar *m_cvSteepSlopeAcrossValue;
  ICVar *m_cvDebugDrawVegetationCollisionDist;
	ICVar *m_cvExtraRadiusDuringBeautification;
	ICVar *m_cvExtraForbiddenRadiusDuringBeautification;
  ICVar *m_cvRadiusForAutoForbidden;
	ICVar *m_cvRecordLog;
	ICVar *m_cvRecordFilter;
	ICVar *m_cvDrawGroups;
	ICVar *m_cvDrawTrajectory;
	ICVar *m_cvDrawHideSpots;
	ICVar *m_cvDrawRadar;
	ICVar *m_cvDrawRadarDist;
	ICVar *m_cvDebugRecord;
	ICVar *m_cvDebugRecordBuffer;
	ICVar *m_cvTickCounter;
	ICVar *m_cvDrawGroupTactic;
  ICVar *m_cvUpdateAllAlways;
	ICVar *m_cvDrawShooting;
	ICVar *m_cvDrawExplosions;
	ICVar *m_cvDrawDistanceLUT;
  ICVar *m_cvPuppetDirSpeedControl;
	ICVar *m_cvDrawAreas;
	ICVar *m_cvDrawProbableTarget;
	ICVar *m_cvIncludeNonColEntitiesInNavigation;
	ICVar *m_cvDebugDrawDamageParts;
	ICVar *m_cvDebugDrawStanceSize;
	ICVar *m_cvDebugDrawObstrSpheres;
	ICVar *m_cvDebugDrawUpdate;
	ICVar *m_cvDebugDrawVisChecQueue;
	ICVar *m_cvDebugDrawLightLevel;
	ICVar *m_cvDebugDrawHashSpaceAround;

	ICVar *m_cvUseObjectPosWithExactPos;

	ICVar *m_cvProtoROD;
	ICVar *m_cvProtoRODFireRange;
	ICVar *m_cvProtoRODAliveTime;
	ICVar *m_cvProtoRODRegenTime;
	ICVar *m_cvProtoRODReactionTime;
	ICVar *m_cvProtoRODLogScale;
	ICVar *m_cvProtoRODAffectMove;
	ICVar *m_cvProtoRODHealthGraph;
	ICVar *m_cvProtoRODSilhuette;
	ICVar *m_cvProtoRODSpeedMod;
	ICVar *m_cvProtoRODGrenades;

	ICVar *m_cvSOMSpeedRelaxed;
	ICVar *m_cvSOMSpeedCombat;
	ICVar *m_cvSightRangeDarkIllumMod;
	ICVar *m_cvSightRangeMediumIllumMod;

	ICVar *m_cvMovementSpeedDarkIllumMod;
	ICVar *m_cvMovementSpeedMediumIllumMod;

	ICVar *m_cvRODAliveTime;
	ICVar *m_cvRODMoveInc;
	ICVar *m_cvRODStanceInc;
	ICVar *m_cvRODDirInc;
	ICVar *m_cvRODAmbientFireInc;
	ICVar *m_cvRODKillZoneInc;

	ICVar *m_cvRODKillRangeMod;
	ICVar *m_cvRODCombatRangeMod;

	ICVar *m_cvRODReactionTime;
	ICVar *m_cvRODReactionDarkIllumInc;
	ICVar *m_cvRODReactionMediumIllumInc;
	ICVar *m_cvRODReactionDistInc;
	ICVar *m_cvRODReactionDirInc;
	ICVar *m_cvRODReactionLeanInc;

	ICVar *m_cvRODLowHealthMercyTime;

	ICVar *m_cvRODCoverFireTimeMod;

	ICVar *m_cvAmbientFireUpdateInterval;
	ICVar *m_cvAmbientFireQuota;
	ICVar *m_cvDebugDrawAmbientFire;

	ICVar *m_cvDebugDrawExpensiveAccessoryQuota;

	ICVar *m_cvDebugTargetSilhouette;
	ICVar *m_cvDebugDrawDamageControl;
	ICVar *m_cvDrawFakeTracers;
	ICVar *m_cvDrawFakeHitEffects;
	ICVar *m_cvDrawFakeDamageInd;

	ICVar *m_cvForceStance;
  ICVar *m_cvForceAllowStrafing;
  ICVar *m_cvForceLookAimTarget;

	ICVar *m_cvInterestSystem;
	ICVar *m_cvDebugInterest;
	ICVar *m_cvEnableInterestScan;
	ICVar *m_cvInterestEntryBoost;
	ICVar *m_cvInterestSwitchBoost;
	ICVar *m_cvInterestSpeedMultiplier;
	ICVar *m_cvInterestListenMovement;
	ICVar *m_cvInterestScalingScan;
	ICVar *m_cvInterestScalingAmbient;
	ICVar *m_cvInterestScalingMovement;
	ICVar *m_cvInterestScalingEyeCatching;
	ICVar *m_cvInterestScalingView;

	ICVar *m_cvBannedNavSoTime;
	ICVar *m_cvWaterOcclusion;

	ICVar *m_cvDebugDrawAdaptiveUrgency;
	ICVar *m_cvDebugDrawReinforcements;
	ICVar *m_cvDebugDrawCollisionEvents;
	ICVar *m_cvDebugDrawBulletEvents;
	ICVar *m_cvDebugDrawSoundEvents;
	ICVar *m_cvDebugDrawGrenadeEvents;
	ICVar *m_cvDebugDrawPlayerActions;

	ICVar *m_cvCloakMinDist;
	ICVar *m_cvCloakMaxDist;
	ICVar *m_cvCloakIncrement;

	ICVar *m_cvBigBrushLimitSize;	// to be used for finding big objects not enclosed into forbidden areas

	ICVar *m_cvSimpleWayptPassability;
	ICVar *m_threadedVolumeNavPreprocess;
	ICVar *m_extraVehicleAvoidanceRadiusBig;
	ICVar *m_extraVehicleAvoidanceRadiusSmall;
	ICVar *m_obstacleSizeThreshold;
	ICVar *m_cvDrawGetEnclosingFailures;

	ICVar *m_cvUseAlternativeReadability;

private:
	void InitConsoleVars();
	void UpdateGlobalAlertness();
	static void CheckGoalpipes(IConsoleCmdArgs *);

	//====================================================================
	// 	Functions for graph creation/loading etc
	//====================================================================
	GraphNode* FindMarkNodeBy2Vertex(CGraphLinkManager& linkManager, int vIdx1, int vIdx2, GraphNode* exclude, ListNodes &lstNewNodes);
  /// make and tags a list of nodes that intersect the segment from start to end.
  void	CreatePossibleCutList( const Vec3 & vStart, const Vec3 & vEnd, ListNodeIds & lstNodes );
	bool ForbiddenAreasOverlap();
	bool ForbiddenAreaOverlap(const CAIShape* pShape);
	void RefineTriangle(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, unsigned nodeIndex, const Vec3r &start, const Vec3r &end, ListNodeIds &lstNewNodes, ListNodeIds &lstOldNodes, NewCutsVector &newCutsVector);
	bool TriangleLineIntersection(GraphNode * pNode, const Vec3 & vStart, const Vec3 & vEnd);
	bool SegmentInTriangle(GraphNode * pNode, const Vec3 & vStart, const Vec3 & vEnd);
	void CalculateLinkProperties();
  void MarkForbiddenTriangles();
  void DisableThinNodesNearForbidden();
	void CalculateLinkExposure(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode* pNode, unsigned link);
	void CalculateLinkWater(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode* pNode, unsigned link);
	void CalculateLinkRadius(CGraphNodeManager& nodeManager, CGraphLinkManager& linkManager, const GraphNode* pNode, unsigned link);
  void CalculateNoncollidableLinks();
  float CalculateDistanceToCollidable(const Vec3 &pos, float maxDist) const;
	void CreateNewTriangle(const ObstacleData & od1, const ObstacleData & od2, const ObstacleData & od3, bool tag, ListNodeIds &lstNewNodes);
	void AddForbiddenAreas();
	void AddForbiddenArea(CAIShape* pShape);
	void AddBreakableRegionsToTriangulator(float gridDelta, const AABB & worldAABB);
  void AddDynamicTrianglesToTriangulator(const AABB & worldAABB);
  // 0 means no extra triangulation, otherwise it is the smallest triangle size
  float GetDynamicTriangulationValue(const Vec3 &pos) const;
  /// Find points on the land/water boundary and add them to the triangulator
	void AddBeachPointsToTriangulator(const AABB & worldAABB);
  /// Combines designer and code forbidden areas - returns false if there's a problem
  bool CalculateForbiddenAreas();
  /// combines overlapping areas. Returns false if there's a problem
  bool CombineForbiddenAreas(CAIShapeContainer& areas);
	/// Check all the forbidden areas are sensible size etc
	bool ValidateAreas();
	bool ValidateBigObstacles();

	// parses ai information into file
	void ParseIntoFile(const char * szFileName, CGraph *pGraph, bool bForbidden = false);
	// writes special areas into file
	void WriteAreasIntoFile(const char * fileNameAreas);
	// reads special areas from file. clears the existing special areas
	void ReadAreasFromFile(const char * fileNameAreas);

	//====================================================================
	// 	Functions for pathfinding
	//====================================================================

	void UpdatePathFinder();
	void QueuePathFindRequest(const PathFindRequest &request);

	/// Sees if the pathfind request is so trivial we can handle it immediately - e.g.
	/// the start/end are the same/adjacent nodes.
	/// Returns true if the request was handled
	bool CheckForAndHandleShortPath(const PathFindRequest &request);

	// stops the pathfinder and reschedules the original request - used if the graph
	// changes
	void RescheduleCurrentPathfindRequest();
	void InvalidatePathsThroughArea(const ListPositions& areaShape);
	void InvalidatePathsThroughPendingBrokenRegions();
	bool PointInBrokenRegion(const Vec3 & point) const;

	/// Generates m_navPath based on the pathNodes passed in. m_navPath will not
	/// include points at the beginning of the path that the requester has already
	/// passed.
	void BeautifyPath(const VectorConstNodeIndices & pathNodes);

	/// Populates a collection of navigation blockers based on the currently active 
	/// puppers (etc) in relation to the requester
	void GetNavigationBlockers(NavigationBlockers& navigationBlockers, CAIActor *pRequester, const PathFindRequest *pfr);

  /// Internal helper - check/handle a position. pAIObject can be zero (if so, assume
  /// it is the player)
  void GetNavigationBlocker(NavigationBlockers& navigationBlockers, 
    const Vec3 &requesterPos, const Vec3 &requesterVel,
    Vec3 pos,  const Vec3& blockerVel, CAIActor* pAIActor);

  /// Internal helper - returns the extra cost associated with traversing a single shape. Initial AABB is 
  /// assumed to have already been done and passed
  float GetExtraLinkCost(const Vec3 &pos1, const Vec3 &pos2, const AABB &linkAABB, const SExtraLinkCostShape &shape) const;

	/// Get an approximation of the minimum size of an area/volume around a point (in m), 
	/// The smaller is the size, the narrower is the area/volume
	/// the higher is numIter, the better refined is the calculation
//	float GetAreaMinSize(Vec3 point, bool b3D=false,int numIter=3);

	//====================================================================
	// Debug drawing helpers
	//====================================================================
	void DebugDrawPath(IRenderer *pRenderer) const;
  void DebugDrawPathAdjustments(IRenderer *pRenderer) const;
	void DebugDrawPathSingle(IRenderer *pRenderer, const CPuppet *pPuppet) const;
	void DebugDrawAgents(IRenderer *pRenderer) const;
	void DebugDrawAgent(IRenderer *pRenderer, CAIObject* pAgent) const;
	void DebugDrawStatsTarget(IRenderer *pRenderer, const char *pName);
	void DebugDrawFormations(IRenderer *pRenderer) const;
	void DebugDrawNavModifiers(IRenderer *pRenderer) const;
	void DebugDrawTaggedNodes(IRenderer *pRenderer) const;
	void DebugDrawNode(IRenderer *pRenderer) const;
	void DebugDrawNodeSingle(IRenderer *pRenderer, CAIObject* pTargetObject) const;
	void DebugDrawGraph(IRenderer *pRenderer, CGraph* pGraph, const std::vector<Vec3> * focusPositions = 0, float radius = 0.0f) const;
	void DebugDrawGraphErrors(IRenderer *pRenderer, CGraph* pGraph) const;
  void DebugDrawForbidden(IRenderer *pRenderer) const;
  void DebugDrawType(IRenderer *pRenderer) const;
	void DebugDrawTypeSingle(IRenderer *pRenderer, CAIObject* pAIObj) const;
	void DebugDrawPendingEvents(IRenderer *pRenderer, CPuppet* pPuppet, int xPos, int yPos) const;
	void DebugDrawTargetsList(IRenderer *pRenderer) const;
	void DebugDrawTargetUnit(IRenderer *pRenderer, CAIObject* pAIObj) const;
	void DebugDrawSquadmates(IRenderer *pRenderer) const;
	void DebugDrawStatsList(IRenderer *pRenderer) const;
	void DebugDrawLocate(IRenderer *pRenderer) const;
	void DebugDrawLocateUnit(IRenderer *pRenderer, CAIObject* pAIObj) const;
	void DebugDrawDummy(IRenderer *pRenderer, CAIObject* pAIObj, const char * typeName) const;
	void DebugDrawBadAnchors(IRenderer *pRenderer) const ;
	void DebugDrawSteepSlopes(IRenderer *pRenderer);
	void DebugDrawVegetationCollision(IRenderer *pRenderer);
	void DebugDrawGroups(IRenderer *pRenderer);
	void DebugDrawGroup(IRenderer *pRenderer, short grpID);
	void DebugDrawUnreachableNodeLocations(IRenderer *pRenderer) const;
	void DebugDrawHideSpots(IRenderer *pRenderer);
	void DebugDrawDynamicHideObjects(IRenderer *pRenderer);
	void DebugDrawHashSpace(IRenderer *pRenderer);
	void DebugDrawMyHideSpot(IRenderer *pRenderer, CAIObject* pAIObj) const;
	void DebugDrawSelectedHideSpots(IRenderer *pRenderer) const;
	void DebugDrawCrowdControl(IRenderer *pRenderer);
	void DebugDrawRadar(IRenderer *pRenderer);
	void DebugDrawDistanceLUT(IRenderer *pRenderer);
	void DrawRadarPath(IRenderer *pRenderer, CPuppet* puppet, const Matrix34& world, const Matrix34& screen);
	void DebugDrawRecorderRange(IRenderer *pRenderer) const;
	void DebugDrawShooting(IRenderer *pRenderer) const;
	void DebugDrawExplosions(IRenderer *pRenderer) const;
	void DebugDrawAreas(IRenderer *pRenderer) const;
	void DebugDrawAmbientFire(IRenderer *pRenderer) const;
	void DebugDrawExpensiveAccessoryQuota(IRenderer *pRenderer) const;
	void DebugDrawObstructSpheres(IRenderer *pRenderer) const;
	void DebugDrawDamageControlGraph(IRenderer *pRenderer) const;
	void DebugDrawAdaptiveUrgency(IRenderer *pRenderer) const;
	enum EDrawUpdateMode
	{
		DRAWUPDATE_NONE = 0,
		DRAWUPDATE_NORMAL,
		DRAWUPDATE_WARNINGS_ONLY,
	};
	void DebugDrawUpdate(IRenderer *pRenderer) const;
	bool DebugDrawUpdateUnit(IRenderer *pRenderer, CPuppet* pTargetPuppet, int row, EDrawUpdateMode mode) const;


	//====================================================================
	// Other internal helpers
	//====================================================================
	void SingleDryUpdate(CPuppet * pObject);
	bool SingleFullUpdate(CPuppet * pPuppet, int64 frameStartTime);
	bool SingleFullUpdate2(CPuppet * pPuppet, std::vector<CAIObject*>& priorityTargets);
	void TakeDetectionSnapshot();
	void VisCheckBroadPhase();

	void UpdatePlayerPerception();
	void UpdateAmbientFire();
	void UpdateExpensiveAccessoryQuota();
	void UpdateNavRegions();
	void UpdateDebugStuff();
	void UpdateAuxSignalsMap();
	void UpdateCollisionEvents();
	void UpdateBulletEvents();

	void CheckVisibilityBodies();
	int RayOcclusionPlaneIntersection(const Vec3& start, const Vec3& end);
	int RayObstructionSphereIntersection(const Vec3& start, const Vec3& end);

	/// tells if the sound is hearable by the puppet
	bool IsSoundHearable(CPuppet *pPuppet,const Vec3 &vSoundPos,float fSoundRadius);
	void SupressSoundEvent(const Vec3 &pos,float &fAffectedRadius);

	/// Walks through over object in the system that needs to be serialised, or is referenced
	/// by serialisation, and adds it to the object tracker (to generate/store an ID for it)
	void PopulateObjectTracker(class CObjectTracker& objectTracker);
	/// add a single AI object to tracker
	void PopulateObjectTracker(class CObjectTracker& objectTracker, CAIObject* pObject);

	/// Uses the object tracker to create empty objects that are ready to be filled in
	/// through subsequent load-serialisation
	void InstantiateFromObjectTracker(class CObjectTracker& objectTracker);

	/// Our own internal serialisation - just serialise our state (but not the things
	/// we own that are capable of serialising themselves)
	void Serialize( TSerialize ser, CObjectTracker& objectTracker );
	
	//
	void SerializeDynamicGoalPipes( TSerialize ser );

	//
	void ClearAllLeaders( );

  // just steps through objects - for debugging
  void DebugOutputObjects(const char *txt) const;

	//====================================================================
	// 	Private data
	//====================================================================
	CGraph *m_pGraph;
	CGraph *m_pHideGraph;
	/// Indicates if the navigation data is sufficiently valid after loading that
	/// we should continue
  enum ENavDataState {NDS_UNSET, NDS_OK, NDS_BAD};
	ENavDataState m_navDataState;

  class CTriangularNavRegion *m_pTriangularNavRegion;
  class CWaypointHumanNavRegion *m_pWaypointHumanNavRegion;
  class CWaypoint3DSurfaceNavRegion *m_pWaypoint3DSurfaceNavRegion;
  class CVolumeNavRegion *m_pVolumeNavRegion;	
  class CFlightNavRegion *m_pFlightNavRegion;	
  class CRoadNavRegion *m_pRoadNavRegion;	
  class CFree2DNavRegion *m_pFree2DNavRegion;
  class CSmartObjectNavRegion* m_pSmartObjectNavRegion;	
  class CTriangularNavRegion *m_pTriangularHideRegion;

  class CAStarSolver * m_pAStarSolver;
 
  NavigationBlockers m_navigationBlockers;

	CAIAutoBalance *m_pAutoBalance;

	typedef VectorMap<CSmartObject*, float> SmartObjectFloatMap;
	SmartObjectFloatMap m_bannedNavSmartObjects;

	unsigned int m_nNumPuppets;

	IVisArea *m_pAreaList[100];
	std::list<IVisArea*> lstSectors;
	int m_nRaysThisUpdateFrame;

	unsigned int m_nNumBuildings;
	CBuildingIDManager	m_BuildingIDManager;

	int		m_CurrentGlobalAlertness;

	int		m_visChecks, m_visChecksMax;
	int		m_visChecksRays, m_visChecksRaysMax;
	static const unsigned VIS_CHECK_HISTORY_SIZE = 200;
	int		m_visChecksHistory[VIS_CHECK_HISTORY_SIZE];
	int		m_visChecksRaysHistory[VIS_CHECK_HISTORY_SIZE];
	int		m_visChecksUpdatesHistory[VIS_CHECK_HISTORY_SIZE];
	int		m_visChecksHistoryHead;
	int		m_visChecksHistorySize;

	CTimeValue	m_lastVisBroadPhaseTime;
	CTimeValue	m_lastAmbientFireUpdateTime;
	CTimeValue	m_lastExpensiveAccessoryUpdateTime;
	CTimeValue	m_lastGroupUpdateTime;

	VectorSet<CPuppet*>	m_enabledPuppetsSet;		// Set of enabled puppets.
	VectorSet<CPuppet*>	m_disabledPuppetsSet;		// Set of disabled puppets.
	float m_enabledPuppetsUpdateError;
	int		m_enabledPuppetsUpdateHead;
	int		m_totalPuppetsUpdateCount;
	float m_disabledPuppetsUpdateError;
	int		m_disabledPuppetsHead;

	struct SAIDelayedExpAccessoryUpdate
	{
		SAIDelayedExpAccessoryUpdate(CPuppet* pPuppet, float time, bool state) :
			pPuppet(pPuppet), time(time), state(state) {}
		CPuppet* pPuppet;
		float time;
		bool state;
	};
	std::vector<SAIDelayedExpAccessoryUpdate>	m_delayedExpAccessoryUpdates;


	CAILightManager m_lightManager;
	CAIDynHideObjectManager m_dynHideObjectManager;

	//====================================================================
	// Used during graph generation - should subsequently be empty
	//====================================================================
	CTriangulator *m_pTriangulator;
	std::vector<Tri*>	m_vTriangles;
	VARRAY  m_vVertices;

	unsigned int m_nTickCount;
	bool m_bInitialized;

	IPhysicalWorld *m_pWorld;

	CTimeValue	m_fLastPathfindTimeStart;
	
	SetAIObjects m_setDummies;
	
//	VectorPuppets m_vWaitingToBeUpdated;	
//	VectorPuppets m_vAlreadyUpdated;	
	
	/// when we generate a path we store it here - the requester can get it from us when
	/// we send it AIEVENT_ONPATHDECISION. 
	CNavPath m_navPath;
	PathQueue	m_lstPathQueue;
	PathFindRequest *m_pCurrentRequest;
	EPathfinderResult m_nPathfinderResult;

	FormationMap	m_mapActiveFormations;
	FormationDescriptorMap m_mapFormationDescriptors;
	BeaconMap			m_mapBeacons;

	PatternDescriptorMap	m_mapTrackPatternDescriptors;

	// keeps all possible goal pipes that the agents can use
	CPipeManager	m_PipeManager;

	MapSignalStrings	m_mapAuxSignalsFired;

	ShapeMap m_mapDesignerPaths;
	ShapeMap m_mapOcclusionPlanes;

	CAIShapeContainer m_forbiddenAreas;
	CAIShapeContainer m_designerForbiddenAreas;
	CAIShapeContainer m_forbiddenBoundaries;

  DynamicTriangulationShapeMap m_mapDynamicTriangularNavigation;
  /// rebuilding the QuadTrees will not be super-quick so they simply get
  /// cleared when they are invalidated.
  void ClearForbiddenQuadTrees();
  /// QuadTrees will be built on (a) triangulation (b) loading a level
  void RebuildForbiddenQuadTrees();

  ExtraLinkCostShapeMap m_mapExtraLinkCosts;

	SpecialAreaMap	m_mapSpecialAreas;	// define where to disable automatic AI processing.	
	SpecialAreaVector m_vectorSpecialAreas;	// to speed up GetSpecialArea() in game mode

	ShapeMap m_mapGenericShapes;

	// list of outlines that we need to disable
	CGraph::CRegions m_pendingBrokenRegions;
	// list of broken regions
	CGraph::CRegions m_brokenRegions;

	std::vector<short>	m_priorityObjectTypes;

	CTimeValue m_fFrameStartTime;
	float m_fFrameDeltaTime;
	CTimeValue m_fLastPuppetUpdateTime;

	bool m_bUpdateSmartObjects;
	CSmartObjects* m_pSmartObjects;
	CAIActionManager* m_pActionManager;

	// combat classes
	// vector of target selection scale multipliers
	struct SCombatClassDesc
	{
		std::vector<float> mods;
		string customSignal;
	};
	std::vector<SCombatClassDesc>	m_CombatClasses;

  /// used during 3D nav generation - the pass radii of the entities that will use the navigation
  std::vector<float> m_3DPassRadii;
  
  /// used during triangulation to flag nodes that are obscured by their obstacles
  std::vector<Vec3> m_unreachableNodeLocations;

	// This map stores the AI group info.
	AIGroupMap	m_mapAIGroups;

	struct SDebugLine
	{
		SDebugLine( const Vec3& start_, const Vec3& end_, const ColorB& color_, float time_ ) :
			start(start_), end(end_), color(color_), time(time_) {}
		Vec3		start, end;
		ColorB	color;
		float		time;
	};
	std::vector<SDebugLine>	m_vecDebugLines;

	struct SDebugBox
	{
		SDebugBox( const Vec3& pos_, const OBB& obb_, const ColorB& color_, float time_ ) :
			pos(pos_), obb(obb_), color(color_), time(time_) {}
			Vec3		pos;
			OBB			obb;
			ColorB	color;
			float		time;
	};
	std::vector<SDebugBox>	m_vecDebugBoxes;

	struct SDebugSphere
	{
		SDebugSphere (const Vec3 &pos_, float radius_, const ColorB &color_, float time_) :
				pos(pos_), radius(radius_), color(color_), time(time_) { }
		Vec3		pos;
		float		radius;
		ColorB	color;
		float		time;
	};
	std::vector<SDebugSphere> m_vecDebugSpheres;

	void DrawDebugShape (const SDebugLine & );
	void DrawDebugShape (const SDebugBox & );
	void DrawDebugShape (const SDebugSphere & );
	template <typename ShapeContainer>
	void DrawDebugShapes (ShapeContainer & shapes, float dt);

	Vec3									m_lastStatsTargetTrajectoryPoint;
	std::list<SDebugLine>	m_lstStatsTargetTrajectory;

	CAIDbgRecorder	m_DbgRecorder;
	CAIRecorder	m_Recorder;

	std::list<IFireCommandDesc*>					m_firecommandDescriptors;

	struct SAIDeadBody
	{
		inline SAIDeadBody() {}
		inline SAIDeadBody(unsigned int s, const Vec3& p) { Set(s, p); }
		inline void Set(unsigned int s, const Vec3& p)
		{
			species = s;
			pos = p;
			t = 20.0f;
		}
		inline void Serialize(TSerialize ser)
		{
			ser.BeginGroup("AIDeadBody");
			ser.Value("pos", pos);
			ser.Value("species", species);
			ser.Value("t", t);
			ser.EndGroup();
		}
		unsigned int species;
		Vec3 pos;
		float t;
	};
//  typedef std::vector<SSerialisablePair<unsigned int, Vec3> >	TDeadBodyList;
	std::vector<SAIDeadBody> m_deadBodies;

	const float	m_ExplosionSpotLifeTime;
	struct SAIExplosionSpot
	{
		inline SAIExplosionSpot() {}
		inline SAIExplosionSpot(const Vec3& pos, float radius, float t) : pos(pos), radius(radius), t(t) {}
		inline void Reset(const Vec3& newPos, float newTime) { pos = newPos; t = newTime; }
		Vec3 pos;
		float radius;
		float t;
		inline void Serialize(TSerialize ser)
		{
			ser.BeginGroup("AIExplosionSpot");
			ser.Value("pos", pos);
			ser.Value("radius", radius);
			ser.Value("t", t);
			ser.EndGroup();
		}
	};
	std::vector<SAIExplosionSpot>	m_explosionSpots;

  TDamageRegions m_damageRegions;

	float	m_recorderDrawStart;
	float	m_recorderDrawEnd;
	float	m_recorderDrawCursor;

	TickCounterManager*	m_pProfileTickCounter;

	class AILinearLUT
	{
		int	m_size;
		float* m_pData;

	public:
		AILinearLUT() : m_size(0), m_pData(0) {}
		~AILinearLUT()
		{
			if (m_pData)
				delete [] m_pData;
		}

		// Returns the size of the table.
		int	GetSize() const { return m_size; }
		// Returns the value is specified sample.
		float	GetSampleValue(int i) const { return m_pData[i]; }

		/// Set the lookup table from a array of floats.
		void Set(float* values, int n)
		{
			delete [] m_pData;
			m_size = n;
			m_pData = new float[n];
			for(int i = 0; i < n; ++i) m_pData[i] = values[i];
		}

		// Returns linearly interpolated value, t in range [0..1]
		float	GetValue(float t) const
		{
			const int	last = m_size - 1;

			// Convert 't' to a sample.
			t *= (float)last;
			const int	n = (int)floorf(t);

			// Check for out of range cases.
			if(n < 0) return m_pData[0];
			if(n >= last) return m_pData[last];

			// Linear interpolation between the two adjacent samples.
			const float	a = t - (float)n;
			return m_pData[n] + (m_pData[n+1] - m_pData[n]) * a;
		}
	};

	AILinearLUT	m_VisDistLookUp;

  // structures to cache forbidden area query results
  struct SIPOFEQuery
  {
    SIPOFEQuery(const Vec3 &pos, const float tol, bool checkAutoRegions)
      : pos(pos), tol(tol), checkAutoRegions(checkAutoRegions), normal(ZERO), pPolygon(0) {}
    SIPOFEQuery() {}
    bool operator==(const SIPOFEQuery &other) {return pos == other.pos && tol == other.tol && checkAutoRegions == other.checkAutoRegions;}

    Vec3 pos;
    float tol;
    bool checkAutoRegions;

    bool result;
    Vec3 normal;
    const ListPositions* pPolygon;
  };

  struct SIPIFRQuery
  {
    SIPIFRQuery(const Vec3 &pos, bool checkAutoRegions)
      : pos(pos), checkAutoRegions(checkAutoRegions), pPolygon(0) {}
    SIPIFRQuery() {}
    bool operator==(const SIPIFRQuery &other) {return pos == other.pos && checkAutoRegions == other.checkAutoRegions;}

    Vec3 pos;
    bool checkAutoRegions;

    bool result;
    const ListPositions* pPolygon;
  };

  enum EQueueSizes {QS_IPOFE = 8, QS_IPIFR = 8};
  typedef MiniQueue<SIPOFEQuery, QS_IPOFE> tIPOFEQueue;
  typedef MiniQueue<SIPIFRQuery, QS_IPIFR> tIPIFRQueue;
  mutable tIPOFEQueue m_isPointOnForbiddenEdgeQueue;
  mutable tIPIFRQueue m_isPointInForbiddenRegionQueue;

	struct SValidationErrorMarker
	{
		SValidationErrorMarker(const string& msg, const Vec3& pos, const OBB& obb, ColorB col) : msg(msg), pos(pos), obb(obb), col(col) {}
		SValidationErrorMarker(const string& msg, const Vec3& pos, ColorB col) : msg(msg), pos(pos), obb(obb), col(col)
		{
			obb.SetOBBfromAABB(Matrix33(IDENTITY), AABB(Vec3 (-0.1f, -0.1f, -0.1f), Vec3 (0.1f, 0.1f, 0.1f)));
		}
		Vec3 pos;
		OBB obb;
		string msg;
		ColorB col;
	};

	// NOTE Jun 9, 2007: <pvl> needs to access m_validationErrorMarkers
	friend bool DoesShapeSelfIntersect(const ShapePointContainer & , Vec3 & );

	std::vector<SValidationErrorMarker>	m_validationErrorMarkers;

  //-----------------------------------------------------------------------------------------------------
  //	obstruct spheres
	struct SObstrSphere
	{
		inline SObstrSphere(const Vec3& _pos, float _radius, float _lifeTime, float _fadeTime) : 
			pos(_pos), radius(_radius), lifeTime(_lifeTime),
			fadeTime(_fadeTime), curRadius(0.f), curTime(0.f)
		{
			if (lifeTime < 2.0f*fadeTime)
				fadeTime = lifeTime*0.5f;
		}

		bool	Update(float dt);
		bool	IsObstructing(const Vec3&p1,const Vec3&p2);

		Vec3	pos;
		float	radius;
		float	curRadius;
		float	lifeTime;
		float	fadeTime;
		float	curTime;
	};
	std::vector<SObstrSphere> m_ObstructSpheres;

	enum EDebugPlayerDamageType
	{
		DBG_PLAYER_SHOT_AT,
		DBG_PLAYER_HIT,
	};

	struct SDebugPlayerDamage
	{
		SDebugPlayerDamage(EDebugPlayerDamageType type, const char* shooterName, float damage, const char* materialName, float t) :
			type(type), shooterName(shooterName), damage(damage), materialName(materialName), t(t) {}
		EDebugPlayerDamageType	type;
		string	shooterName;
		float		damage;
		string	materialName;
		float	t;
	};
	typedef std::list<SDebugPlayerDamage>	DebugPlayerDamageList;
	DebugPlayerDamageList	m_DEBUG_playerShots;


	struct SDebugFakeTracer
	{
		SDebugFakeTracer(const Vec3& p0, const Vec3& p1, float a, float t) : p0(p0), p1(p1), a(a), t(t), tmax(t) {}
		Vec3	p0, p1;
		float a;
		float t, tmax;
	};
	std::vector<SDebugFakeTracer>	m_DEBUG_fakeTracers;

	void DEBUG_AddFakeDamageIndicator(CPuppet* pShooter, float t);

	struct SDebugFakeDamageInd
	{
		SDebugFakeDamageInd() {}
		SDebugFakeDamageInd(const Vec3& pos, float t) : p(pos), t(t), tmax(t) {}
		std::vector<Vec3> verts;
		Vec3	p;
		float	t, tmax;
	};
	std::vector<SDebugFakeDamageInd>	m_DEBUG_fakeDamageInd;

	struct SDebugFakeHitEffect
	{
		SDebugFakeHitEffect() {}
		SDebugFakeHitEffect(const Vec3& p, const Vec3& n, float r, float t, ColorB c) : p(p), n(n), r(r), t(t), tmax(t), c(c) {}
		Vec3	p, n;
		float	r, t, tmax;
		ColorB c;
	};
	std::vector<SDebugFakeHitEffect>	m_DEBUG_fakeHitEffect;

	float m_DEBUG_screenFlash;


	enum SAICollisionObjClassification
	{
		AICO_SMALL,
		AICO_MEDIUM,
		AICO_LARGE,
	};

	struct SAICollisionEvent
	{
		SAICollisionEvent(EntityId id, const Vec3& pos, float radius, float affectRadius, float soundRadius, bool thrownByPlayer,
			SAICollisionObjClassification type, float tmax) :
			id(id), pos(pos), radius(radius), affectRadius(affectRadius), soundRadius(soundRadius),
			thrownByPlayer(thrownByPlayer), type(type), t(0), tmax(tmax), signalled(false) {}
		Vec3 pos;
		float radius;
		float affectRadius;
		float soundRadius;
		bool thrownByPlayer;
		bool smallObject;
		float t, tmax;
		EntityId id;
		SAICollisionObjClassification type;
		bool signalled;
		VectorSet<EntityId> processed;
	};
	std::vector<SAICollisionEvent>	m_collisionEvents;

	typedef VectorMap< EntityId, float > MapIgnoreCollisionsFrom;
	MapIgnoreCollisionsFrom m_ignoreCollisionEventsFrom;

	struct SAISoundEvent
	{
		SAISoundEvent(EntityId senderId, const Vec3& pos, float radius, EAISoundEventType type, float tmax) :
			senderId(senderId), pos(pos), radius(radius), type(type), t(0), tmax(tmax) {}
		Vec3 pos;
		float radius;
		float t, tmax;
		EntityId senderId;
		EAISoundEventType type;
	};
	std::vector<SAISoundEvent>	m_soundEvents;

	struct SAIGrenadeEvent
	{
		SAIGrenadeEvent(EntityId grenadeId, EntityId shooterId, const Vec3& pos, float radius, float tmax) :
			grenadeId(grenadeId), shooterId(shooterId), pos(pos), radius(radius), t(0), tmax(tmax), collided(false) {}
		Vec3 pos;
		float radius;
		float t, tmax;
		bool collided;
		EntityId grenadeId;
		EntityId shooterId;
	};
	std::vector<SAIGrenadeEvent>	m_grenadeEvents;

	struct SAIBulletEvent
	{
		SAIBulletEvent(EntityId shooterId, const Vec3& pos, float radius, float tmax) :
			shooterId(shooterId), pos(pos), radius(radius), t(0), tmax(tmax), signalled(false) {}
		Vec3 pos;
		float radius;
		float t, tmax;
		EntityId shooterId;
		bool signalled;
	};
	std::vector<SAIBulletEvent>	m_bulletEvents;

	struct SAIEventListener
	{
		IAIEventListener* pListener;
		Vec3 pos;
		float radius;
		int flags;
	};
	std::vector<SAIEventListener>	m_eventListeners;

	void NotifyAIEventListeners(EAIEventType type, const Vec3& pos, float radius, float threat, EntityId sender);

	VectorSet<IAIPathFinderListerner*> m_pathFindListeners;
	int m_pathFindIdGen;

	const bool IsEnabled() const {return m_IsEnabled;}
	bool m_IsEnabled;

};

#include "ObstacleAllocator.h"

//===================================================================
// IsShapeCompletelyInForbiddenRegion
//===================================================================
template<typename VecContainer>
bool CAISystem::IsShapeCompletelyInForbiddenRegion(VecContainer &shape, bool checkAutoGenRegions) const
{
  const typename VecContainer::const_iterator itEnd = shape.end();
  for (typename VecContainer::const_iterator it = shape.begin() ; it != itEnd ; ++it)
  {
    if (!IsPointInForbiddenRegion(*it, checkAutoGenRegions))
      return false;
  }
  return true;
}

// CRC for AISIGNALS helper
struct AISIGNALS_CRC 
{
	uint32 m_nOnRequestNewObstacle;
	uint32 m_nOnRequestUpdate;
	uint32 m_nOnRequestShootSpot;
	uint32 m_nOnRequestHideSpot;
	uint32 m_nOnStartTimer;
	uint32 m_nOnLastKnownPositionApproached;
	uint32 m_nOnEndApproach;
	uint32 m_nOnAbortAction;
	uint32 m_nOnActionCompleted;
	uint32 m_nOnNoPathFound;
	uint32 m_nOnApproachEnd;
	uint32 m_nOnEndFollow;
	uint32 m_nOnBulletRain;
	uint32 m_nOnBulletHit;
	uint32 m_nOnDriverEntered;
	uint32 m_nAIORD_ATTACK;
	uint32 m_nAIORD_SEARCH;
	uint32 m_nAIORD_HIDE;
	uint32 m_nAIORD_HOLD;
	uint32 m_nAIORD_GOTO;
	uint32 m_nAIORD_USE;
	uint32 m_nAIORD_FOLLOW;
	uint32 m_nAIORD_IDLE;
	uint32 m_nAIORD_REPORTDONE;
	uint32 m_nAIORD_REPORTFAIL;
	uint32 m_nAIORD_LEAVE_VEHICLE;
	uint32 m_nUSE_ORDER_ENABLED;
	uint32 m_nUSE_ORDER_DISABLED;
	uint32 m_nORD_ALIEN_SEARCH;
	uint32 m_nOnChangeStance;
	uint32 m_nOnEnableFire;
	uint32 m_nOnDisableFire;
	uint32 m_nOnScaleFormation;
	uint32 m_nOnUnitDied;
	uint32 m_nOnUnitDamaged;
	uint32 m_nOnUnitBusy;
	uint32 m_nOnUnitSuspended;
	uint32 m_nOnUnitResumed;
	uint32 m_nOnSetUnitProperties;
	uint32 m_nOnJoinTeam;
	uint32 m_nRPT_LeaderDead;
	uint32 m_nOnVehicleRequest;
	uint32 m_nOnVehicleListProvided;
	uint32 m_nOnEnterEnemyArea;
	uint32 m_nOnLeaveEnemyArea;
	uint32 m_nOnEnterFriendlyArea;
	uint32 m_nOnLeaveFriendlyArea;
	uint32 m_nOnEnableAlertStatus;
	uint32 m_nOnDisableAlertStatus;
	uint32 m_nOnSpotSeeingTarget;
	uint32 m_nOnSpotLosingTarget;
	uint32 m_nOnPause;
	uint32 m_nOnTaskSuspend;
	uint32 m_nOnTaskResume;
	uint32 m_nOnFormationPointReached;
	uint32 m_nOnKeepEnabled;
	uint32 m_nOnActionDone;
	uint32 m_nOnGroupAdvance;
	uint32 m_nOnGroupSeek;
	uint32 m_nOnLeaderReadabilitySeek;
	uint32 m_nOnGroupCover;
	uint32 m_nAddDangerPoint;
	uint32 m_nOnLeaderReadabilityAlarm;
	uint32 m_nOnAdvanceTargetCompromised;
	uint32 m_nOnGroupCohesionTest;
	uint32 m_nOnGroupTestReadabilityCohesion;
	uint32 m_nOnGroupTestReadabilityAdvance;
	uint32 m_nOnGroupAdvanceTest;
	uint32 m_nOnLeaderReadabilityAdvanceLeft;
	uint32 m_nOnLeaderReadabilityAdvanceRight;
	uint32 m_nOnLeaderReadabilityAdvanceForward;
	uint32 m_nOnGroupTurnAmbient;
	uint32 m_nOnGroupTurnAttack;
	uint32 m_nOnShapeEnabled;
	uint32 m_nOnShapeDisabled;
	uint32 m_nOnCloseContact;
	uint32 m_nOnGroupMemberDiedNearest;
	uint32 m_nOnTargetDead;
	uint32 m_nOnEndPathOffset;
	uint32 m_nOnPathFound;
	uint32 m_nOnBackOffFailed;
	uint32 m_nOnBadHideSpot;
	uint32 m_nOnHideSpotReached;
	uint32 m_nOnRightLean;
	uint32 m_nOnLeftLean;
	uint32 m_nOnLowHideSpot;
	uint32 m_nOnChargeStart;
	uint32 m_nOnChargeHit;
	uint32 m_nOnChargeMiss;
	uint32 m_nOnChargeBailOut;
	uint32 m_nOnCoverCompromised;
	uint32 m_nORDER_IDLE;
	uint32 m_nOnNoGroupTarget;
	uint32 m_nOnLeaderTooFar;
	uint32 m_nOnEnemySteady;
	uint32 m_nOnShootSpotFound;
	uint32 m_nOnShootSpotNotFound;
	uint32 m_nOnLeaderStop;
	uint32 m_nOnUnitMoving;
	uint32 m_nOnUnitStop;
	uint32 m_nORDER_EXIT_VEHICLE;
	uint32 m_nOnNoFormationPoint;
	uint32 m_nOnTargetApproaching;
	uint32 m_nOnTargetFleeing;
	uint32 m_nOnNoTargetVisible;
	uint32 m_nOnNoTargetAwareness;
	uint32 m_nOnNoHidingPlace;
	uint32 m_nOnOutOfAmmo;
	uint32 m_nOnMeleeExecuted;
	uint32 m_nOnPlayerLooking;
	uint32 m_nOnPlayerSticking;
	uint32 m_nOnPlayerLookingAway;
	uint32 m_nOnPlayerGoingAway;
	uint32 m_nOnTargetTooClose;
	uint32 m_nOnTargetTooFar;
	uint32 m_nOnFriendInWay;
	uint32 m_nOnUseSmartObject;
	uint32 m_nOnTrackPatternEnclosed;
	uint32 m_nOnTrackPatternExposed;
	uint32 m_nOnReachedTrackPatternNode;
	uint32 m_nOnAvoidDanger;
	uint32 m_nOnRequestUpdateTowards;
	uint32 m_nOnRequestUpdateAlternative;
	uint32 m_nOnClearSpotList;
	uint32 m_nOnSetPredictionTime;
	uint32 m_nOnInterestSystemEvent;
	uint32 m_nOnCheckDeadTarget;
	uint32 m_nOnCheckDeadBody;
	uint32 m_nOnSpecialAction;

	//the ctor initializations are now moved into Init since the central crc generator access must be at a known time
	AISIGNALS_CRC() : m_crcGen(NULL){}

	//initializes all crc dependent members, called in CAISystem ctor
	void Init(const Crc32Gen* const crcGen)
	{
		assert(crcGen);
		m_crcGen = crcGen;
		m_nOnRequestNewObstacle = crcGen->GetCRC32("OnRequestNewObstacle");
		m_nOnRequestUpdate = crcGen->GetCRC32("OnRequestUpdate"); 
		m_nOnRequestUpdateAlternative = crcGen->GetCRC32("OnRequestUpdateAlternative"); 
		m_nOnRequestUpdateTowards = crcGen->GetCRC32("OnRequestUpdateTowards"); 
		m_nOnClearSpotList = crcGen->GetCRC32("OnClearSpotList"); 
		m_nOnRequestShootSpot = crcGen->GetCRC32("OnRequestShootSpot");
		m_nOnRequestHideSpot = crcGen->GetCRC32("OnRequestHideSpot");
		m_nOnStartTimer = crcGen->GetCRC32("OnStartTimer");
		m_nOnLastKnownPositionApproached = crcGen->GetCRC32("OnLastKnownPositionApproached");
		m_nOnEndApproach = crcGen->GetCRC32("OnEndApproach");
		m_nOnAbortAction = crcGen->GetCRC32("OnAbortAction");
		m_nOnActionCompleted = crcGen->GetCRC32("OnActionCompleted");
		m_nOnNoPathFound = crcGen->GetCRC32("OnNoPathFound");
		m_nOnApproachEnd = crcGen->GetCRC32("OnApproachEnd");
		m_nOnEndFollow = crcGen->GetCRC32("OnEndFollow");
		m_nOnBulletRain = crcGen->GetCRC32("OnBulletRain");
		m_nOnBulletHit = crcGen->GetCRC32("OnBulletHit");
		m_nOnDriverEntered = crcGen->GetCRC32("OnDriverEntered");
		m_nOnCheckDeadTarget = crcGen->GetCRC32("OnCheckDeadTarget");
		m_nOnCheckDeadBody = crcGen->GetCRC32("OnCheckDeadBody");

		m_nAIORD_ATTACK = crcGen->GetCRC32(AIORD_ATTACK);
		m_nAIORD_SEARCH = crcGen->GetCRC32(AIORD_SEARCH);
		m_nAIORD_HIDE = crcGen->GetCRC32(AIORD_HIDE);
		m_nAIORD_HOLD = crcGen->GetCRC32(AIORD_HOLD);
		m_nAIORD_GOTO = crcGen->GetCRC32(AIORD_GOTO);
		m_nAIORD_USE = crcGen->GetCRC32(AIORD_USE);
		m_nAIORD_FOLLOW = crcGen->GetCRC32(AIORD_FOLLOW);
		m_nAIORD_IDLE = crcGen->GetCRC32(AIORD_IDLE);
		m_nAIORD_REPORTDONE = crcGen->GetCRC32(AIORD_REPORTDONE);
		m_nAIORD_REPORTFAIL = crcGen->GetCRC32(AIORD_REPORTFAIL);
		m_nAIORD_LEAVE_VEHICLE = crcGen->GetCRC32(AIORD_LEAVE_VEHICLE);
		m_nUSE_ORDER_ENABLED = crcGen->GetCRC32(USE_ORDER_ENABLED);
		m_nUSE_ORDER_DISABLED = crcGen->GetCRC32(USE_ORDER_DISABLED);
		m_nORD_ALIEN_SEARCH = crcGen->GetCRC32("ORD_ALIEN_SEARCH");
		m_nOnChangeStance =  crcGen->GetCRC32("OnChangeStance");
		m_nOnEnableFire =  crcGen->GetCRC32("OnEnableFire");
		m_nOnDisableFire =  crcGen->GetCRC32("OnDisableFire");
		m_nOnScaleFormation = crcGen->GetCRC32("OnScaleFormation");
		m_nOnUnitDied = crcGen->GetCRC32("OnUnitDied");
		m_nOnUnitDamaged = crcGen->GetCRC32("OnUnitDamaged");
		m_nOnUnitBusy = crcGen->GetCRC32("OnUnitBusy");
		m_nOnUnitSuspended = crcGen->GetCRC32("OnUnitSuspended");
		m_nOnUnitResumed = crcGen->GetCRC32("OnUnitResumed");
		m_nOnSetUnitProperties = crcGen->GetCRC32("OnSetUnitProperties");
		m_nOnJoinTeam = crcGen->GetCRC32("OnJoinTeam");
		m_nRPT_LeaderDead = crcGen->GetCRC32("RPT_LeaderDead"); 
		m_nOnVehicleRequest = crcGen->GetCRC32("OnVehicleRequest");
		m_nOnVehicleListProvided = crcGen->GetCRC32("OnVehicleListProvided");
		m_nOnEnterEnemyArea = crcGen->GetCRC32("OnEnterEnemyArea");
		m_nOnLeaveEnemyArea = crcGen->GetCRC32("OnLeaveEnemyArea");
		m_nOnEnterFriendlyArea = crcGen->GetCRC32("OnEnterFriendlyArea");
		m_nOnLeaveFriendlyArea = crcGen->GetCRC32("OnLeaveFriendlyArea");
		m_nOnEnableAlertStatus = crcGen->GetCRC32("OnEnableAlertStatus");
		m_nOnDisableAlertStatus = crcGen->GetCRC32("OnDisableAlertStatus");
		m_nOnSpotSeeingTarget = crcGen->GetCRC32("OnSpotSeeingTarget");
		m_nOnSpotLosingTarget = crcGen->GetCRC32("OnSpotLosingTarget");
		m_nOnPause = crcGen->GetCRC32("OnPause");
		m_nOnTaskSuspend = crcGen->GetCRC32("OnTaskSuspend");
		m_nOnTaskResume = crcGen->GetCRC32("OnTaskResume");
		m_nOnFormationPointReached = crcGen->GetCRC32("OnFormationPointReached");
		m_nOnKeepEnabled = crcGen->GetCRC32("OnKeepEnabled");
		m_nOnActionDone = crcGen->GetCRC32("OnActionDone");
		m_nOnGroupAdvance = crcGen->GetCRC32("OnGroupAdvance");
		m_nOnGroupSeek = crcGen->GetCRC32("OnGroupSeek"); 
		m_nOnLeaderReadabilitySeek = crcGen->GetCRC32("OnLeaderReadabilitySeek");
		m_nAddDangerPoint = crcGen->GetCRC32("AddDangerPoint");
		m_nOnGroupCover = crcGen->GetCRC32("OnGroupCover");
		m_nOnLeaderReadabilityAlarm = crcGen->GetCRC32("OnLeaderReadabilityAlarm");
		m_nOnAdvanceTargetCompromised = crcGen->GetCRC32("OnAdvanceTargetCompromised");
		m_nOnGroupCohesionTest = crcGen->GetCRC32("OnGroupCohesionTest");
		m_nOnGroupTestReadabilityCohesion = crcGen->GetCRC32("OnGroupTestReadabilityCohesion");
		m_nOnGroupTestReadabilityAdvance = crcGen->GetCRC32("OnGroupTestReadabilityAdvance");
		m_nOnGroupAdvanceTest = crcGen->GetCRC32("OnGroupAdvanceTest");
		m_nOnLeaderReadabilityAdvanceLeft = crcGen->GetCRC32("OnLeaderReadabilityAdvanceLeft");
		m_nOnLeaderReadabilityAdvanceRight = crcGen->GetCRC32("OnLeaderReadabilityAdvanceRight");
		m_nOnLeaderReadabilityAdvanceForward = crcGen->GetCRC32("OnLeaderReadabilityAdvanceForward");
		m_nOnGroupTurnAmbient = crcGen->GetCRC32("OnGroupTurnAmbient");
		m_nOnGroupTurnAttack = crcGen->GetCRC32("OnGroupTurnAttack");
		m_nOnShapeEnabled = crcGen->GetCRC32("OnShapeEnabled");
		m_nOnShapeDisabled = crcGen->GetCRC32("OnShapeDisabled");
		m_nOnCloseContact = crcGen->GetCRC32("OnCloseContact");
		m_nOnGroupMemberDiedNearest = crcGen->GetCRC32("OnGroupMemberDiedNearest"); 
		m_nOnTargetDead = crcGen->GetCRC32("OnTargetDead"); 
		m_nOnEndPathOffset = crcGen->GetCRC32("OnEndPathOffset");
		m_nOnPathFound = crcGen->GetCRC32("OnPathFound");
		m_nOnBackOffFailed = crcGen->GetCRC32("OnBackOffFailed");
		m_nOnBadHideSpot = crcGen->GetCRC32("OnBadHideSpot");
		m_nOnHideSpotReached = crcGen->GetCRC32("OnHideSpotReached");
		m_nOnRightLean = crcGen->GetCRC32("OnRightLean");
		m_nOnLeftLean = crcGen->GetCRC32("OnLeftLean");
		m_nOnLowHideSpot = crcGen->GetCRC32("OnLowHideSpot"); 
		m_nOnChargeStart = crcGen->GetCRC32("OnChargeStart");
		m_nOnChargeHit = crcGen->GetCRC32("OnChargeHit");
		m_nOnChargeMiss = crcGen->GetCRC32("OnChargeMiss");
		m_nOnChargeBailOut = crcGen->GetCRC32("OnChargeBailOut");
		m_nOnCoverCompromised = crcGen->GetCRC32("OnCoverCompromised");
		m_nORDER_IDLE= crcGen->GetCRC32("ORDER_IDLE");
		m_nOnNoGroupTarget = crcGen->GetCRC32("OnNoGroupTarget");
		m_nOnLeaderTooFar = crcGen->GetCRC32("OnLeaderTooFar");
		m_nOnEnemySteady = crcGen->GetCRC32("OnEnemySteady");
		m_nOnShootSpotFound= crcGen->GetCRC32("OnShootSpotFound");
		m_nOnShootSpotNotFound= crcGen->GetCRC32("OnShootSpotNotFound"); 
		m_nOnLeaderStop = crcGen->GetCRC32("OnLeaderStop");
		m_nOnUnitMoving = crcGen->GetCRC32("OnUnitMoving");
		m_nOnUnitStop = crcGen->GetCRC32("OnUnitStop");
		m_nORDER_EXIT_VEHICLE = crcGen->GetCRC32("ORDER_EXIT_VEHICLE");
		m_nOnNoFormationPoint = crcGen->GetCRC32("OnNoFormationPoint");
		m_nOnTargetApproaching = crcGen->GetCRC32("OnTargetApproaching");
		m_nOnTargetFleeing = crcGen->GetCRC32("OnTargetFleeing");
		m_nOnNoTargetVisible= crcGen->GetCRC32("OnNoTargetVisible");
		m_nOnNoTargetAwareness = crcGen->GetCRC32("OnNoTargetAwareness");
		m_nOnNoHidingPlace = crcGen->GetCRC32("OnNoHidingPlace");
		m_nOnOutOfAmmo = crcGen->GetCRC32("OnOutOfAmmo");
		m_nOnMeleeExecuted = crcGen->GetCRC32("OnMeleeExecuted");
		m_nOnPlayerLooking = crcGen->GetCRC32("OnPlayerLooking");
		m_nOnPlayerSticking = crcGen->GetCRC32("OnPlayerSticking");
		m_nOnPlayerLookingAway = crcGen->GetCRC32("OnPlayerLookingAway");
		m_nOnPlayerGoingAway = crcGen->GetCRC32("OnPlayerGoingAway");
		m_nOnTargetTooClose = crcGen->GetCRC32("OnTargetTooClose");
		m_nOnTargetTooFar = crcGen->GetCRC32("OnTargetTooFar"); 
		m_nOnFriendInWay = crcGen->GetCRC32("OnFriendInWay");
		m_nOnUseSmartObject = crcGen->GetCRC32("OnUseSmartObject");
		m_nOnTrackPatternEnclosed = crcGen->GetCRC32("OnTrackPatternEnclosed");
		m_nOnTrackPatternExposed = crcGen->GetCRC32("OnTrackPatternExposed"); 
		m_nOnReachedTrackPatternNode = crcGen->GetCRC32("OnReachedTrackPatternNode");
		m_nOnAvoidDanger = crcGen->GetCRC32("OnAvoidDanger");
		m_nOnSetPredictionTime = crcGen->GetCRC32("m_nOnSetPredictionTime");
		m_nOnInterestSystemEvent = crcGen->GetCRC32("OnInterestSystemEvent");		
		m_nOnSpecialAction =  crcGen->GetCRC32("OnSpecialAction");		
	};

	const Crc32Gen *m_crcGen;

};// g_CRCs;

extern AISIGNALS_CRC g_crcSignals;


#endif // _CAISYSTEM_H_
