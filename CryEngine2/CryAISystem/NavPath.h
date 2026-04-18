/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   NavPath.h
	$Id$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 13:1:2005   15:53 : Created by Kirill Bulatsev

*********************************************************************/
#ifndef __NavPath_H__
#define __NavPath_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include "IAgent.h"
#include "SerializeFwd.h"
#include "AILog.h"
#include "PathObstacles.h"

#include "Cry_Color.h"

#include <list>

struct IRenderer;
class CGraph;
class CPipeUser;

// passing through navigational SO methods:
enum ENavSOMethod
{
	nSOmNone,							// not passing or not passable
	nSOmAction,						// execute an AI action
	nSOmPriorityAction,		// execute a higher priority AI action
	nSOmStraight,					// just pass straight
	nSOmSignalAnimation,	// play signal animation
	nSOmActionAnimation,	// play action animation
	nSOmLast
};

//====================================================================
// PathPointDescriptor
//====================================================================
struct PathPointDescriptor
{
	struct SmartObjectNavData : public _i_reference_target_t
	{
		unsigned fromIndex;
		unsigned toIndex;
    void Serialize(TSerialize ser);
	};
	typedef _smart_ptr< SmartObjectNavData > SmartObjectNavDataPtr;

	PathPointDescriptor(IAISystem::ENavigationType navType = IAISystem::NAV_UNSET, const Vec3 pos = ZERO) 
		: vPos(pos), pSONavData(0), navType(navType), navSOMethod(nSOmNone) {}

	void Serialize(TSerialize ser);
	Vec3 vPos;
	IAISystem::ENavigationType navType;
	SmartObjectNavDataPtr pSONavData;
	ENavSOMethod	navSOMethod;
	bool IsEquivalent(const PathPointDescriptor& other) const {
		return (navType == other.navType) && vPos.IsEquivalent(other.vPos, 0.01f);}
};

typedef std::list<PathPointDescriptor> TPathPoints;

//====================================================================
// SNavPathParams
//====================================================================
struct SNavPathParams
{
  SNavPathParams(const Vec3& start = Vec3(0, 0, 0), const Vec3& end = Vec3(0, 0, 0), 
    const Vec3& startDir = Vec3(0, 0, 0), const Vec3& endDir = Vec3(0, 0, 0), 
    int nForceBuildingID = -1, bool allowDangerousDestination = false, float endDistance = 0.0f,
		bool continueMovingAtEnd = false, bool isDirectional = false) 
    : start(start), end(end), startDir(startDir), endDir(endDir),
    nForceBuildingID(nForceBuildingID), allowDangerousDestination(allowDangerousDestination), 
    precalculatedPath(false), inhibitPathRegeneration(false), continueMovingAtEnd(continueMovingAtEnd),
		endDistance(endDistance), isDirectional(isDirectional) {}

  Vec3 start;
  Vec3 end;
  Vec3 startDir;
  Vec3 endDir;
  int nForceBuildingID;
  bool allowDangerousDestination;
  /// if path is precalculated it should not be regenerated, and also some things like steering
  /// will be disabled
  bool precalculatedPath;
  /// sometimes it is necessary to disable a normal path from getting regenerated
  bool inhibitPathRegeneration;
  bool continueMovingAtEnd;
	bool isDirectional;
	float endDistance; // The requested cut distance of the path, positive value means distance from path end, negative value means distance from path start.

  void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
};

//====================================================================
// CNavPath
//====================================================================
class CNavPath
{
public:
	CNavPath();
	~CNavPath();

  /// returns the path version - this is incremented every time the
  /// path is modified (wrapping around is unlikely to be a problem!)
  int GetVersion() const {return m_version.v;}

	/// Set the path version, should not normally be used.
	void SetVersion(int v) {m_version.v = v;}

  /// The original parameters used to generate the path will be stored
  void SetParams(const SNavPathParams& params) {m_params = params;}
  const SNavPathParams& GetParams() const {return m_params;}
  SNavPathParams& GetParams() {return m_params;}

	void Draw(IRenderer *pRenderer, const Vec3& drawOffset=ZERO) const;
	void Dump(const char *name) const;
  // gets distance from the current path point to the end of the path - needs to 
  // be told if this path should be traversed as if it's in 2D
	float	GetPathLength(bool twoD) const;

  /// Add a point to the front of the path
	void PushFront(const PathPointDescriptor& newPathPoint, bool force = false);

  /// Add a point to the back of the path
	void PushBack(const PathPointDescriptor& newPathPoint, bool force = false);

  /// Clear all the path. dbgString can be used to indicate the calling context
  /// If this path is owned by an object that might need to free smart-object
  /// resources when the path is cleared, then clearing the path should be done
  /// via CPipeUser::ClearPath()
	void	Clear(const char *dbgString);

  /// removes the current previous point (shifting all points) and sets nextPathPoint to the
  /// new next path point (if it's available). If there are no points remaining, then returns false, 
  /// and the path will be left with just the path end
	bool Advance(PathPointDescriptor& nextPathPoint);

	/// Indicates if this path finishes at approx the location that was requested, or
	/// if it uses some other end position (e.g. from a partial path)
	bool GetPathEndIsAsRequested() const {return m_pathEndIsAsRequested;}
	void SetPathEndIsAsRequested(bool val) {m_pathEndIsAsRequested = val;}

  /// Returns true if the path contains so few points it's not useable (returns false if 2 or more)
  bool	Empty() const;
  /// end of the path - returns 0 if empty
  const PathPointDescriptor	*GetLastPathPoint() const {return !m_pathPoints.empty() ? &m_pathPoints.back() : 0;}
  /// previous path point - returns 0 if not available
  const PathPointDescriptor *GetPrevPathPoint() const {return !m_pathPoints.empty() ? &m_pathPoints.front() : 0;}
  /// next path point - returns 0 if not available
  const PathPointDescriptor *GetNextPathPoint() const {return m_pathPoints.size() > 1 ? &(*(++m_pathPoints.begin())) : 0;}
  /// next but 1 path point - returns 0 if not available
  const PathPointDescriptor *GetNextNextPathPoint() const {return m_pathPoints.size() > 2 ? &(*(++(++m_pathPoints.begin()))) : 0;}

  /// Gets the position of next path point. Returns defaultPos if the path is empty
  const Vec3 &GetNextPathPos(const Vec3 &defaultPos = Vec3Constants<float>::fVec3_Zero) const {const PathPointDescriptor *ppd = GetNextPathPoint(); return ppd ? ppd->vPos : defaultPos;}
  /// Gets the end of the path. Returns defaultPos if the path is empty
  const Vec3 &GetLastPathPos(const Vec3 &defaultPos = Vec3Constants<float>::fVec3_Zero) const {const PathPointDescriptor *ppd = GetLastPathPoint(); return ppd ? ppd->vPos : defaultPos;}

	/// Returns the point that is dist ahead on the path, extrapolating if necessary. Returns false if there
  /// is no path points at all
	bool GetPosAlongPath(Vec3 &posOut, float dist, bool twoD, bool extrapolateBeyondEnd) const;

  /// Returns the distance from pos to the closest point on the path, looking a maximum of dist ahead.
  /// Also the closest point, and the distance it is along the path are returned.
  /// Returns -1 if there's no path.
  /// Note that this starts from the start of the path, NOT the current position
  float GetDistToPath(Vec3 &pathPosOut, float &distAlongPathOut, const Vec3 &pos, float dist, bool twoD) const;

  /// Returns the distance along the path from the current look-ahead position to the 
  /// first smart object path segment. If there's no path, or no smart objects on the 
  /// path, then std::numeric_limits<float>::max() will be returned
  float GetDistToSmartObject(bool twoD) const;

	/// Returns a pointer to a smartobject nav data if the last path point exists and
	/// its traversal method is one of the animation methods.
	PathPointDescriptor::SmartObjectNavDataPtr	GetLastPathPointAnimNavSOData() const;

  /// Sets the value of the previous path point, if it exists
	void SetPreviousPoint(const PathPointDescriptor& previousPoint);

	const TPathPoints & GetPath() const {return m_pathPoints;}
	void	GetPath(PATHPOINTVECTOR& pathVector) const;

  /// Returns AABB containing all the path between the current position and dist ahead
  AABB GetAABB(float dist) const;

  /// Calculates the path position and direction and (approx) radius of curvature distAhead beyond the current iterator
  /// position. If the this position would be beyond the path end then it extrapolates.
  /// If scaleOutputWithDist is set then it scales the lowestPathDotOut according to how far along the
  /// path it is compared to distAhead
  /// Returns false if there's not enough path to do the calculation - true if all is
  /// well.
  bool GetPathPropertiesAhead(float distAhead, bool twoD, Vec3 &posOut, Vec3 &dirOut, 
    float *invROut, float &lowestPathDotOut, bool scaleOutputWithDist) const;

	/// Serialise the current request
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);

	Vec3	GetEndDir() const {return m_endDir;}
	void	SetEndDir(const Vec3& endDir) {m_endDir=endDir;}

	/// Obtains a new target direction and approximate distance to the path end (for speed control). Also updates
	/// the current position along the path.
	///
	/// isResolvingSticking indicates if the algorithm is trying to resolve agent sticking.
  /// distToPathOut indicates the approximate perpendicular distance the agent is from the path.
  /// pathDirOut, pathAheadPosOut and pathAheadDirOut are the path directions/position at the current
  /// and look-ahead positions
	/// currentPos/Vel are the tracer's current position/velocity.
	/// lookAhead is the look-ahead distance for smoothing the path.
	/// pathRadius is the radius of the core part of the path - the narrower the
	/// path the more constrained the lookahead will be at corners
	/// dt is the time since last update (mainly used to detect the tracer 
	/// getting stuck).
	/// resolveSticking indicates if getting stuck should be handled - handling is
	/// done by limiting the lookahead which can confuse vehicles that use
	/// manouevering.
	/// twoD indicates if this should work in 2D
	/// Return value indicates if we're still tracing the path - false means we reached the end
	bool UpdateAndSteerAlongPath(Vec3& dirOut, float& distToEndOut, float &distToPathOut, bool& isResolvingSticking,
    Vec3 &pathDirOut, Vec3 &pathAheadDirOut, Vec3 &pathAheadPosOut,
		Vec3 currentPos, const Vec3& currentVel, 
		float lookAhead, float pathRadius, float dt, bool resolveSticking, bool twoD);

	/// Iterates over the path and fills the navigation methods for the path segments.
	/// Cuts the path at the path point where the next navSO animation should be played.
	/// The trace goalop will follow the path and play the animation at the target location
	/// and then regenerate the path to the target.
	void PrepareNavigationalSmartObjects(CPipeUser* pUser);

	/// trim the path to specified length.
	void TrimPath(float length, bool twoD);

	/// after the path is being cut on smart object this function
	/// can tell the length of the discarded path section
	float GetDiscardedPathLength() const { return m_fDiscardedPathLength; }

  /// Adjusts the path (or what's left of it) to steer it around the obstacles passed in.
  /// These obstacles should already have been expanded if necessary to allow for pass
  /// radius. Returns false if there was no way to adjust the path (in which case the path
  /// may be partially adjusted)
  bool AdjustPathAroundObstacles(const CPathObstacles &obstacles, IAISystem::tNavCapMask navCapMask);

  /// This should be called for each object that goes into AdjustPathAroundObstacles so that the
  /// caller knows not to locally steer around them later
  void AddObjectAdjustedFor(const class CAIObject *pObject) {m_objectsAdjustedFor.push_back(pObject);}

  /// Get the list of objects that have already adjusted the path
  const std::vector<const class CAIObject*> &GetObjectsAdjustedFor() const {return m_objectsAdjustedFor;}

  /// Clear the list of objects that have already adjusted the path
  void ClearObjectsAdjustedFor() {m_objectsAdjustedFor.resize(0);}

	/// Advances path state until it represents agentPos. If allowPathToFinish = false then
  /// it won't allow the path to finish
	/// Returns the remaining path length
	float UpdatePathPosition(Vec3 agentPos, float pathLookahead, bool twoD, bool allowPathToFinish);

	/// Calculates a target position which is lookAhead along the path, 
	Vec3 CalculateTargetPos(Vec3 agentPos, float lookAhead, float minLookAheadAlongPath, float pathRadius, bool twoD) const;

  /// Attempt to move the path end/target point to the new value, given the abilities
  /// of the puppet. Returns true if the path is modified and the new position
  /// is possible
  bool TryToUsePathTargetPoint(const Vec3 &pt, class CPuppet *pPuppet, bool twoD);

  /// Returns true if the path can be modified to use request.targetPoint, and byproducts 
  /// of the test are cached in request.
  ETriState CanTargetPointBeReached(CTargetPointRequest &request, const class CPuppet *pPuppet, bool twoD) const;
  /// Returns true if the request is still valid/can be used, false otherwise.
  bool UseTargetPointRequest(const CTargetPointRequest &request, class CPuppet *pPuppet, bool twoD);

private:
  /// Makes the path avoid a single obstacle - return false if this is impossible.
  /// It only works on m_pathPoints.
  bool AdjustPathAroundObstacle(const CPathObstacle &obstacle, IAISystem::tNavCapMask navCapMask);
  bool AdjustPathAroundObstacleShape2D(const SPathObstacleShape2D &obstacle, IAISystem::tNavCapMask navCapMask);
  bool CheckPath(const TPathPoints &pathList, float radius) const;
  void MovePathEndsOutOfObstacles(const CPathObstacles &obstacles);

  float GetPathDeviationDistance(Vec3 &deviationOut, float criticalDeviation, bool twoD);

  TPathPoints	m_pathPoints;
	Vec3	m_endDir;

	struct SDebugLine
	{
		Vec3	P0, P1; ColorF	col;
	};
	void	DebugLine( const Vec3& P0, const Vec3& P1, const ColorF& col ) const;
	mutable std::list<SDebugLine>			m_debugLines;

	struct SDebugSphere
	{
		Vec3	pos; float r; ColorF	col;
	};
	void	DebugSphere( const Vec3& P, float r, const ColorF& col ) const;
	mutable std::list<SDebugSphere>			m_debugSpheres;

	/// the current position on the path between previous and current point
	float m_currentFrac;

	/// How long we've been stuck for - used to reduce the lookAhead
	float m_stuckTime;

	/// Indicates if this path finishes at approx the location that was requested, or
	/// if it uses some other end position (e.g. from a partial path)
	bool m_pathEndIsAsRequested;

  /// parameters used to generate the request - in case the requester wants to regenerate
  /// it
  SNavPathParams m_params;

  std::vector<const class CAIObject *> m_objectsAdjustedFor;

  /// the length of the path segment discarded after a smart object
  float m_fDiscardedPathLength;

  /// Lets other things track when we've changed
  /// also we change version when copied!
  struct SVersion
  {
    int v;
    void operator=(const SVersion &other) {v = other.v + 1;}
    SVersion() : v(-1) {}
    void Serialize(TSerialize ser);
  };
  SVersion m_version;
};


#endif __NavPath_H__
