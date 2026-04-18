// GoalOp.h: interface for the CGoalOp class.
//
//////////////////////////////////////////////////////////////////////


#if !defined(AFX_GOALOP_H__1CD1CEF7_0DC6_4C6C_BCF1_EDE36179F7E7__INCLUDED_)
#define AFX_GOALOP_H__1CD1CEF7_0DC6_4C6C_BCF1_EDE36179F7E7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "AIPIDController.h"
#include "PathMarker.h"
#include "TimeValue.h"

#include <list>

class CPuppet;
class CAIObject;
struct IAIObject;
class CAISystem;
struct GraphNode;
//<<FIXME>> remove later
class CAISystem;

//ADDED FOR PIPE USER
class CPipeUser;
//-------------------------

// Return value of the goalop execute.
// Based on the goalop, the result should be either 'succeed/failed' or
// if the goalop always succeeds and the result should not be used
// later return 'done'. As long as the goalop should be executed,
// 'in progress' should be returned. The goalops should not return the
// 'none' value.
enum EGoalOpResult
{
	AIGOR_NONE,						// Default value.
	AIGOR_IN_PROGRESS,		// Keep on updating.
	AIGOR_SUCCEED,				// The goalop succeed.
	AIGOR_FAILED,					// The goalop failed.
	AIGOR_DONE,						// The goalop is finished (neutral, do not change result state).
	LAST_AIGOR
};

class CGoalOp  
{
public:
	CGoalOp();
	virtual ~CGoalOp();

  /// What significance does the return value have?
	virtual EGoalOpResult Execute(CPipeUser *pOperand) { return AIGOR_DONE; }
  /// Stripped-down execute - should only be implemented if this goal op needs
  /// to be really responsive (gets called every frame - i.e. on the dry update)
	virtual void ExecuteDry(CPipeUser *pOperand) {}
	virtual void DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const {}
	virtual void Reset(CPipeUser *pOperand) {}
	virtual void OnObjectRemoved(CAIObject *pObject) {}
	virtual void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
};

////////////////////////////////////////////////////////////
//
//				ACQUIRE TARGET - acquires desired target, even if it is outside of view
//
////////////////////////////////////////////////////////

class COPAcqTarget: public CGoalOp
{
	
public:
	//COPAcqTarget(CAIObject *pTarget);
	COPAcqTarget(const string& name);
	~COPAcqTarget();

	string m_TargetName;

	EGoalOpResult Execute(CPipeUser *pOperand);
};


////////////////////////////////////////////////////////////
//
//				APPROACH - makes agent approach to "distance" from current att target
//
////////////////////////////////////////////////////////
class COPPathFind;
class COPTrace;
class COPApproach: public CGoalOp
{
public:
	COPApproach(float fEndDistance, float fEndAccuracy=1.f, float fDuration = 0.0f,
		bool bUseLastOpResult = false, bool bLookAtLastOp = false,
		bool bForceReturnPartialPath = false, bool bStopOnAnimationStart = false, const char* szNoPathSignalText = 0);
	~COPApproach();


	EGoalOpResult Execute(CPipeUser *pOperand);
	void DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const;
	void ExecuteDry(CPipeUser *pOperand);
  void OnObjectRemoved(CAIObject *pObject);
	void Reset(CPipeUser *pOperand);
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
private:
  float m_fLastDistance;
  float m_fInitialDistance;
  bool	m_bPercent;
  bool	m_bUseLastOpResult;
  bool	m_bLookAtLastOp;
  bool	m_stopOnAnimationStart;
  float m_fEndDistance; /// Stop short of the path end
  float m_fEndAccuracy; /// allow some error in the stopping position
  float m_fDuration; /// Stop after this time (0 means disabled)
  bool	m_bForceReturnPartialPath;
  string	m_noPathSignalText;
  int		m_looseAttentionId;

	float GetEndDistance(CPipeUser *pOperand) const;

  // set to true when the path is first found
  bool m_bPathFound;
  COPTrace		*m_pTraceDirective;
  COPPathFind		*m_pPathfindDirective;
};


////////////////////////////////////////////////////////////
//
//				FOLLOW - makes agent follow to "distance" from current att target/last op result
//
////////////////////////////////////////////////////////

class COPFollow: public CGoalOp
{

private:
	CAIObject *m_pFollowTarget;
	CAIObject *m_pLeader;

	float m_fInitialDistance;
	bool	m_bUseLastOpResult;
	Vec3	m_lastMoveDir;

	CTimeValue m_lastUpdateTime;
	float m_fLastTargetSpeed;
	float m_fVariance;

	CPathMarker*	m_pTargetPathMarker;

	CAIPIDController m_PIDController;

public:
	COPFollow(float distance, bool useLastOpResult = false );
	~COPFollow();

	float m_fDistance;

	EGoalOpResult Execute(CPipeUser *pOperand);
	void Reset(CPipeUser *pOperand);
	void OnObjectRemoved(CAIObject *pObject);
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
};

////////////////////////////////////////////////////////////
//
// FOLLOWPATH - makes agent follow the predefined path stored in the operand
//
////////////////////////////////////////////////////////

class COPFollowPath: public CGoalOp
{
public:
  /// pathFindToStart: whether the operand should path-find to the start of the path, or just go there in a straight line
  /// reverse: whether the operand should navigate the path from the first point to the last, or in reverse
  /// startNearest: whether the operand should start from the first point on the path, or from the nearest point on the path to his current location
  /// loops: how many times to traverse the path (goes round in a loop)
	COPFollowPath(bool pathFindToStart, bool reverse, bool startNearest, int loops, float fEndAccuracy, bool bUsePointList, bool bAvoidDynamicObjects);
	~COPFollowPath();

	EGoalOpResult Execute(CPipeUser *pOperand);
	void ExecuteDry(CPipeUser *pOperand);
	void Reset(CPipeUser *pOperand);
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
private:
	bool HandleBlockInTheWay(CPipeUser *pOperand);

	COPTrace		*m_pTraceDirective;
  COPPathFind *m_pPathFindDirective;
  CAIObject * m_pPathStartPoint;
  bool	m_pathFindToStart;
	bool	m_reversePath;
	bool	m_startNearest;
	bool	m_bUsePointList;
	int		m_loops;
	int		m_loopCounter;
	float	m_notMovingTime;
	CTimeValue	m_lastTime;
	bool	m_returningToPath;
	float m_fEndAccuracy;
	bool m_bAvoidDynamicObjects;

};

////////////////////////////////////////////////////////////
//
//				BACKOFF - makes agent back off to "distance" from current att target
//
////////////////////////////////////////////////////////
class COPBackoff: public CGoalOp
{
public:
  /// If distance < 0 then the backoff is done from the agent's current position,
  /// else it is done from the target position.
  /// If duration > 0 then backoff stops after that duration
	COPBackoff(float distance, float duration = 0.f, int filter=0, float minDistance=0.f);
	~COPBackoff();

	void Reset(CPipeUser *pOperand);
	EGoalOpResult Execute(CPipeUser *pOperand);
	void ExecuteDry(CPipeUser *pOperand);
  void OnObjectRemoved(CAIObject *pObject);
	void DebugDraw(CPipeUser* pOperand, IRenderer* pRenderer) const;
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
private:
	void ResetNavigation(CPipeUser* pOperand);
	void ResetMoveDirections();

private:
	bool m_bUseTargetPosition;	// distance from target or from own current position 
	int	m_iDirection;
	int m_iCurrentDirectionIndex;
	std::vector<int> m_MoveDirections;

	float m_fDistance;
	float m_fDuration;
	CTimeValue m_fInitTime;
	CTimeValue m_fLastUpdateTime;
	bool	m_bUseLastOp;
	bool	m_bLookForward;
	Vec3	m_moveStart;
	Vec3	m_moveEnd;
	int		m_currentDirMask;
	float m_fMinDistance;
	Vec3	m_vBestFailedBackoffPos;
	float m_fMaxDistanceFound;
	bool	m_bTryingLessThanMinDistance;
	int		m_looseAttentionId;
	bool	m_bRandomOrder;
	bool	m_bCheckSlopeDistance;


	COPTrace		*m_pTraceDirective;
	COPPathFind		*m_pPathfindDirective;
  CAIObject *m_pBackoffPoint;
};

////////////////////////////////////////////////////////////
//
//				TIMEOUT - counts down a timer...
//
////////////////////////////////////////////////////////
class COPTimeout: public CGoalOp
{

	CTimeValue	m_startTime;
	float				m_fIntervalMin, m_fIntervalMax;
	float				m_fCurrentInterval;

public:
	COPTimeout(float intervalMin, CAISystem *pAISystem, float intervalMax = 0);
	~COPTimeout();

	EGoalOpResult Execute(CPipeUser *pOperand);
	void Reset(CPipeUser *pOperand);
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker)
	{
		// Luciano - TO DO: serialize it properly
		m_startTime.SetSeconds(0.0f);
	}
};

////////////////////////////////////////////////////////////
//
//				STRAFE makes agent strafe left or right (1,-1)... 0 stops strafing
//
////////////////////////////////////////////////////////
class COPStrafe: public CGoalOp
{
	
public:
	COPStrafe(float distanceStart, float distanceEnd, bool strafeWhileMoving);
	~COPStrafe();

	float m_fDistanceStart;
	float m_fDistanceEnd;
	bool	m_bStrafeWhileMoving;

	EGoalOpResult Execute(CPipeUser *pOperand);
};

////////////////////////////////////////////////////////////
//
//				FIRECMD - 1 allowes agent to fire, 0 forbids agent to fire
//
////////////////////////////////////////////////////////
class COPFireCmd: public CGoalOp
{
public:
	COPFireCmd(int firemode, bool useLastOpResult, float intervalMin, float intervalMax);
	~COPFireCmd();

	EFireMode	m_command;
	bool	m_bUseLastOpResult;

	void Reset(CPipeUser *pOperand);
	EGoalOpResult Execute(CPipeUser *pOperand);

protected:

	float m_fIntervalMin;
	float m_fIntervalMax;
	float m_fCurrentInterval;
	CTimeValue m_startTime;
};

////////////////////////////////////////////////////////////
//
//				BODYCMD - controls agents stance (0- stand, 1-crouch, 2-prone)
//
////////////////////////////////////////////////////////
class COPBodyCmd: public CGoalOp
{
public:
	COPBodyCmd(int bodystate, bool delayed=false);
	~COPBodyCmd(){}

	int m_nBodyState;
	bool m_bDelayed;	// this stance has to be set at the end of next/current trace

	EGoalOpResult Execute(CPipeUser *pOperand);
};

////////////////////////////////////////////////////////////
//
//				RUNCMD - makes the agent run if 1, walk if 0
//
////////////////////////////////////////////////////////
class COPRunCmd: public CGoalOp
{
	float ConvertUrgency(float speed);

	float m_fMaxUrgency;
	float m_fMinUrgency;
	float m_fScaleDownPathLength;

public:
	COPRunCmd(float maxUrgency, float minUrgency, float scaleDownPathLength);
	~COPRunCmd() {}

	EGoalOpResult Execute(CPipeUser *pOperand);
};


////////////////////////////////////////////////////////////
//
//				LOOKAROUND - looks in a random direction
//
////////////////////////////////////////////////////////
class COPLookAround : public CGoalOp
{
	//CAIObject *m_pDummyAttTarget;
	float m_fLastDot;
	float m_fLookAroundRange;
	float	m_fIntervalMin, m_fIntervalMax;
	float	m_fTimeOut;
	float	m_fScanIntervalRange;
	float	m_fScanTimeOut;
	CTimeValue m_startTime;
	CTimeValue m_scanStartTime;
	bool	m_breakOnLiveTarget;
	bool	m_useLastOp;
	float	m_lookAngle;
	float	m_lookZOffset;
	float	m_lastLookAngle;
	float	m_lastLookZOffset;
	int		m_looseAttentionId;
	bool	m_bInitialized;
	Vec3	m_initialDir;
	void	UpdateLookAtTarget(CPipeUser *pOperand);
	Vec3	GetLookAtDir(CPipeUser *pOperand, float angle, float dz) const;

public:
	void Reset(CPipeUser *pOperand);
	COPLookAround(float lookAtRange, float scanIntervalRange, float intervalMin, float intervalMax, bool breakOnLiveTarget, bool useLastOp);
	~COPLookAround();

	virtual EGoalOpResult Execute(CPipeUser *pOperand);
	virtual void ExecuteDry(CPipeUser *pOperand);
	virtual void DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const;
	virtual void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
};


////////////////////////////////////////////////////////////
//
//				PATHFIND - generates a path to desired aiobject
//
////////////////////////////////////////////////////////
class COPPathFind : public CGoalOp
{
	IAIObject *m_pTarget;
	string m_sObjectName;
	bool m_bKeepMoving;
  float m_fDirectionOnlyDistance;
	float m_fEndTol;
	float m_fEndDistance;
	Vec3 m_TargetPos;
  Vec3 m_TargetOffset;
	// if >= 0 forces the path destination to be within a particular building
	int  m_nForceTargetBuildingID;
public:

	bool m_bWaitingForResult;

  /// fDirectionOnlyDistance > 0 means that we should attempt to pathfind that distance in the direction
  /// between the supposed path start/end points
	COPPathFind(const char *szName, IAIObject *pTarget = 0, float fEndTol = 0.0f, float fEndDistance = 0.0f, bool bKeepMoving=false, float fDirectionOnlyDistance = 0.0f);
	~COPPathFind();

	void Reset(CPipeUser *pOperand);
	EGoalOpResult Execute(CPipeUser *pOperand);
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
	void OnObjectRemoved(CAIObject* pObject);
	void SetForceTargetBuildingId(int id) {m_nForceTargetBuildingID = id;}
  void SetTargetOffset(const Vec3 &offset) {m_TargetOffset = offset;}
};


////////////////////////////////////////////////////////////
//
//				LOCATE - locates an aiobject in the map
//
////////////////////////////////////////////////////////
class COPLocate : public CGoalOp
{
	string m_sName;
	unsigned int m_nObjectType;
	float m_fRange;
public:
	COPLocate(const char *szName, unsigned int ot = 0, float range = 0) : m_fRange(range) { if (szName) m_sName = szName; m_nObjectType = ot;}
	~COPLocate() {}

	EGoalOpResult Execute(CPipeUser *pOperand);
};

////////////////////////////////////////////////////////////
//
//				TRACE - makes agent follow generated path... does nothing if no path generated
//
////////////////////////////////////////////////////////
class COPTrace : public CGoalOp
{
private:
  bool  m_bDisabledPendingFullUpdate; // set when we we should follow the path no more
  float	m_ManeuverDist;
  CTimeValue m_ManeuverTime;
  /// For things like helicopters that land then this gets set to some offset used at the end of the
  /// path. Then at the path end the descent is gradually controlled - path only finishes when the
  /// agent touches ground
  float m_landHeight;
  /// Specifies how high we should be above the path at the current point
  float m_workingLandingHeightOffset;
  // The ground position and facing direction
  Vec3 m_landingPos;
  Vec3 m_landingDir;

  bool m_bExactFollow;
	bool m_bForceReturnPartialPath;
  Vec3	m_lastPosition;
  CTimeValue m_lastTime;
  float	m_TimeStep;
  CPipeUser *m_pOperand;
	int		m_looseAttentionId;
  /// keep track of how long we've been tracing a path for - however, this is more of a time
  /// since the last "event" - it can get reset when we regenerate the path etc
  float m_fTotalTracingTime;
  bool m_inhibitPathRegen;
  bool	m_waitingForPathResult;
  bool	m_earlyPathRegen;
	bool	m_bSteerAroundPathTarget; 

  /// How far we've moved since the very start
  float m_fTravelDist;
  /// what time we started getting executed
  CTimeValue m_startTime;

  enum ETraceActorTgtRequest
  {
    eTATR_None,			// no request
    eTATR_NavSO,			// navigational smart object
    eTATR_EndOfPath,	// end of path exact positioning
  };

  /// flag indicating the the trace requested exact positioning.
  ETraceActorTgtRequest	m_actorTargetRequester;
  ETraceActorTgtRequest	m_pendingActorTargetRequester;
  CAIObject *m_pNavTarget;
  bool m_stopOnAnimationStart;

public:
  enum EManeuver{eMV_None, eMV_Back, eMV_Fwd};
  enum EManeuverDir {eMVD_Clockwise, eMVD_AntiClockwise};
  EManeuver	m_Maneuver;
  EManeuverDir m_ManeuverDir;

  /// Aim to stay/stop this far from the target.
//	float	m_fEndDistance;
  /// we aim to stop within m_fAccuracy of the target minus m_fStickDistance (normally do better than this)
  float m_fEndAccuracy;
  /// finish tracing after this duration
//  float m_fDuration;

	bool	m_passingStraightNavSO;

  void Reset(CPipeUser *pOperand);
  /// Don't know what bExactFollow means!
  /// fEndDistance is the distance to stop before the end (handles smart objects etc)
  /// fEndAccuracy is the allowed stopping error at the end
  /// fDuration is used to make us stop gracefully after a certain time
  /// bForceReturnPartialPath is used when we regen the path internally
  /// bStopOnAnimationStart would make trace finish once the exact positioning animation at the end of path is started
  COPTrace( bool bExactFollow, float fEndAccuracy = 1.0f,
		bool bForceReturnPartialPath = false, bool bStopOnAnimationStart = false);
  ~COPTrace();

	inline void SetSteerAroundPathTarget(bool steer) {m_bSteerAroundPathTarget = steer;}

  void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
  EGoalOpResult Execute(CPipeUser *pOperand);
  bool ExecuteTrace(CPipeUser *pOperand, bool fullUpdate);
	bool IsPathRegenerationInhibited() const { return m_inhibitPathRegen || m_passingStraightNavSO; }
	void DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const;
protected:
  void CreateDummyFromNode(const Vec3 &pos, CAISystem *pSystem, const char* ownerName);
  bool Execute2D(CPipeUser *pOperand, bool fullUpdate);
  bool Execute3D(CPipeUser *pOperand, bool fullUpdate);
  bool ExecutePathFollower(CPipeUser *pOperand, bool fullUpdate);
  void ExecuteManeuver(CPipeUser *pOperand, const Vec3& pathDir);
  bool ExecutePreamble(CPipeUser *pOperand);
  bool ExecutePostamble(CPipeUser *pOperand, bool &reachedEnd, bool fullUpdate, bool twoD, float distToEnd);
  /// returns true when landing is completed
  bool ExecuteLanding(CPipeUser *pOperand, const Vec3 &pathEnd);
};

////////////////////////////////////////////////////////////
//
//				IGNOREALL - 1, puppet does not reevaluate threats, 0 evaluates again
//
////////////////////////////////////////////////////////
class COPIgnoreAll : public CGoalOp
{
	bool	m_bParam;
public:
	COPIgnoreAll(bool param) { m_bParam = param;}
	~COPIgnoreAll() {}

	EGoalOpResult Execute(CPipeUser *pOperand);
};


////////////////////////////////////////////////////////////
//
//				SIGNAL - send a signal to himself or other agents
//
////////////////////////////////////////////////////////
class COPSignal : public CGoalOp
{
	int m_nSignalID;
	string m_sSignal;
	unsigned char m_cFilter;
	CAIObject *m_pTarget;
	bool m_bSent;
	int	m_iDataValue;
public:
	COPSignal(int param, const char *szSignal, unsigned char cFilter, int data) : m_bSent(false), m_nSignalID(param), m_pTarget(0), m_cFilter(cFilter), m_sSignal(szSignal), m_iDataValue(data) {};
	~COPSignal() {}

	EGoalOpResult Execute(CPipeUser *pOperand);
};


////////////////////////////////////////////////////////////
//
//				DEVALUE - devalues current attention target 
//
////////////////////////////////////////////////////////
class COPDeValue : public CGoalOp
{
	bool bDevaluePuppets;
	bool m_bClearDevalued;
public:
	COPDeValue(int nPuppetsAlso = 0,bool bClearDevalued = false) { bDevaluePuppets = (nPuppetsAlso!=0);m_bClearDevalued =bClearDevalued; }
	~COPDeValue() {}

	EGoalOpResult Execute(CPipeUser *pOperand);
};

/*
////////////////////////////////////////////////////////////
//
//				FORGET - makes agent forget the current attention target, if it can be found in memory
//
////////////////////////////////////////////////////////
class COPForget : public CGoalOp
{

public:
	COPForget() {}
	~COPForget() {}

	bool Execute(CPipeUser *pOperand);
};
*/

////////////////////////////////////////////////////////////
//
//			HIDE - makes agent find closest hiding place and then hide there
//
////////////////////////////////////////////////////////
class COPHide : public CGoalOp
{
	CAIObject *m_pHideTarget;
	Vec3 m_vHidePos;
	Vec3 m_vLastPos;
	float m_fSearchDistance;
	float m_fMinDistance;
	int		m_nEvaluationMethod;
	COPPathFind *m_pPathFindDirective;
	COPTrace *m_pTraceDirective;
	bool m_bLookAtHide;
	bool	m_bLookAtLastOp;
	//char m_nTicks;
	bool m_bAttTarget;
	bool m_bEndEffect;
	int m_iActionId;
	int		m_looseAttentionId;

public:
	COPHide(float distance, float minDistance, int method, bool bExact, bool bLookatLastOp)
	{
		m_fSearchDistance=distance; m_nEvaluationMethod = method; m_pHideTarget=0;
		m_fMinDistance = minDistance;
		m_pPathFindDirective = 0;
		m_pTraceDirective = 0;
		m_bLookAtHide = bExact;
		m_bLookAtLastOp = bLookatLastOp;
		m_iActionId = 0;
		//m_nTicks = 0;
		m_looseAttentionId = 0;
		m_vHidePos.Set(0,0,0);
		m_vLastPos.Set(0,0,0);
		m_bAttTarget = false;
		m_bEndEffect = false;
	}

  ~COPHide();
	EGoalOpResult Execute(CPipeUser *pOperand);
	void ExecuteDry(CPipeUser *pOperand);
  void OnObjectRemoved(CAIObject *pObject);
	void Reset(CPipeUser *pOperand);
	bool IsBadHiding(CPipeUser *pOperand);
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
};

////////////////////////////////////////////////////////////
//
//			USECOVER - hide/unhide at current hide spot
//
////////////////////////////////////////////////////////
class COPUseCover : public CGoalOp
{

public:
	COPUseCover(bool unHideNow, bool useLastOpAsBackup, float intervalMin, float intervalMax, bool speedUpTimeOutWhenNoTarget);
	~COPUseCover() {}

	EGoalOpResult Execute(CPipeUser *pOperand);
	void ExecuteDry(CPipeUser *pOperand);

	void OnObjectRemoved(CAIObject *pObject);
	void Reset(CPipeUser *pOperand);
	void DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const;
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
protected:

	EGoalOpResult UseCoverSO(CPipeUser *pOperand);
	EGoalOpResult UseCoverSOHide(CPipeUser *pOperand);
	EGoalOpResult UseCoverSOUnHide(CPipeUser *pOperand);
	EGoalOpResult UseCoverEnv(CPipeUser *pOperand);
	void ChooseCoverUsage(CPipeUser *pOperand);
	void RandomizeTimeOut(CPipeUser *pOperand);
	void UpdateInvalidSeg(CPipeUser *pOperand);

	enum EUncoverState
	{
		UCS_NONE,
		UCS_HOLD,
		UCS_MOVE,
	};

	EUncoverState	m_ucs;
	EUncoverState	m_ucsLast;
//	float					m_ucsTime;
	int						m_ucsSide;
	float					m_ucsWaitTime;
	bool					m_ucsMoveIn;
	CTimeValue		m_ucsStartTime;

	bool				m_speedUpTimeOutWhenNoTarget;
	bool				m_targetReached;
	bool				m_coverCompromised;
	bool				m_coverCompromisedSignalled;
	Vec3				m_hidePos;
	Vec3				m_peekPosRight;
	Vec3				m_peekPosLeft;
	Vec3				m_pose;
	Vec3				m_moveTarget;
	int					m_curCoverUsage;
	float				m_weaponOffset;
	float				m_intervalMin;
	float				m_intervalMax;
	float				m_fTimeOut;
	CTimeValue	m_startTime;
	bool				m_bUnHide;
	bool				m_useLastOpAsBackup;

//	float				m_leftInvalidTime;
	CTimeValue	m_leftInvalidStartTime;
	float				m_leftInvalidDist;
	Vec3				m_leftInvalidPos;
//	float				m_rightInvalidTime;
	CTimeValue	m_rightInvalidStartTime;
	float				m_rightInvalidDist;
	Vec3				m_rightInvalidPos;

	bool				m_leftEdgeCheck;
	bool				m_rightEdgeCheck;

	float				m_maxPathLen;

	EStance			m_moveStance;

	bool				m_DEBUG_checkValid;
	Vec3				m_DEGUG_checkPos[6];
	Vec3				m_DEGUG_checkDir[6];
	bool				m_DEBUG_checkRes[6];
};


////////////////////////////////////////////////////////////
//
//			STICK - the agent keeps at a constant distance to his target
//
// regenerate path if target moved for more than m_fTrhDistance
////////////////////////////////////////////////////////
class COPStick : public CGoalOp
{
public:
//	COPStick(float stickDistance, float accuracy, float duration, bool bUseLastOp, bool bLookatLastOp, 
//    bool bContinuous, bool bTryShortcutNavigation, bool bForceReturnPartialPath=false, bool bStopOnAnimationStart=false, bool bConstantSpeed=false);
		COPStick(float stickDistance, float accuracy, float duration, int flags, int flagsAux);
	~COPStick();

	EGoalOpResult Execute(CPipeUser *pOperand);
	void ExecuteDry(CPipeUser *pOperand);
	void Reset(CPipeUser *pOperand);
	void OnObjectRemoved(CAIObject *pObject);
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);

private:
  virtual void DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const;
  /// Keep a list of "safe" points of the stick target. This will be likely to have a valid
  /// path between the point and the stick target - so when teleporting is necessary one
  /// of these points (probably the one furthest back and invisible) can be chosen
  void UpdateStickTargetSafePoints(CPipeUser *pOperand);
  /// Attempts to teleport, and if it's necessary/practical does it, and returns true.
  bool TryToTeleport(CPipeUser * pOperand);
  /// sets teleporting info to the default don't-need-to-teleport state
  void ClearTeleportData();
  void RegeneratePath(CPipeUser *pOperand, const Vec3 &destination);

	float GetEndDistance(CPipeUser *pOperand) const;

  /// Stores when/where the stick target was
  struct SSafePoint
  {
    SSafePoint(const Vec3 &pos = ZERO, unsigned nodeIndex = 0, bool safe = false, const CTimeValue &time = CTimeValue()) : 
			pos(pos), nodeIndex(nodeIndex), safe(safe), time(time) {}
		void Serialize(TSerialize ser);
    Vec3 pos;
    unsigned nodeIndex;
    bool safe; // used to store _unsafe_ locations too
    CTimeValue time;
    // if stored in a container, then we need to be told the object tracker
    static CObjectTracker *pObjectTracker;
  };
  /// Point closest to the stick target is at the front
  typedef MiniQueue<SSafePoint, 32> TStickTargetSafePoints;
  TStickTargetSafePoints m_stickTargetSafePoints;
  /// distance between safe points
  float m_safePointInterval;
  /// teleport current/destination locations
  Vec3 m_teleportCurrent, m_teleportEnd;
  /// time at which the old teleport position was checked
  CTimeValue m_lastTeleportTime;
  /// time at which the operand was last visible
  CTimeValue m_lastVisibleTime;

  // teleport params
  /// Only teleport if the resulting apparent speed would not exceed this.
  /// If 0 it disables teleporting
  float m_maxTeleportSpeed;
  // Only teleport if the operand's path (if it exists) is longer than this
  float m_pathLengthForTeleport;
  // Don't teleport closer than this to player
  float m_playerDistForTeleport;


  Vec3	m_vLastUsedTargetPos;
  float	m_fTrhDistance;
  CAIObject *m_pStickTarget; 
  CAIObject *m_pSightTarget;

  /// Aim to stay/stop this far from the target
  float	m_fStickDistance;
  /// we aim to stop within m_fAccuracy of the target minus m_fStickDistance
  float m_fEndAccuracy;
  /// Stop after this time (0 means disabled)
  float m_fDuration;
  bool	m_bContinuous;		// stick OR just approach moving target
  bool	m_bTryShortcutNavigation;	//
  bool	m_bUseLastOpResult;
  bool	m_bLookAtLastOp;	// where to look at
  bool	m_bInitialized;
  bool	m_bForceReturnPartialPath;
  bool	m_stopOnAnimationStart;
  float m_targetPredictionTime;
	bool	m_bConstantSpeed;

  // used for estimating the target position movement
  CTimeValue m_lastTargetPosTime;
  Vec3 m_lastTargetPos;
  Vec3 m_smoothedTargetVel;

	int		m_looseAttentionId;

  // set to true when the path is first found
  bool m_bPathFound;
	bool m_bSteerAroundPathTarget;
  COPTrace		*m_pTraceDirective;
  COPPathFind		*m_pPathfindDirective;
};


////////////////////////////////////////////////////////////
//
//			FORM - this agent creates desired formation
//
////////////////////////////////////////////////////////
class COPForm : public CGoalOp
{
	string m_sName;
	
public:
	COPForm(const char *name) { m_sName = name;}
	~COPForm() {}

	EGoalOpResult Execute(CPipeUser *pOperand);
};


////////////////////////////////////////////////////////////
//
//			CLEAR - clears the actions for the operand puppet
//
////////////////////////////////////////////////////////
class COPClear : public CGoalOp
{
	string m_sName;
	
public:
	COPClear() {}
	~COPClear() {}

	EGoalOpResult Execute(CPipeUser *pOperand);
};


////////////////////////////////////////////////////////////
//
//			LOOKAT - look toward a specified direction
//
////////////////////////////////////////////////////////
class COPLookAt : public CGoalOp
{
	float m_fStartAngle;
	float m_fEndAngle;
	//CAIObject *m_pDummyAttTarget;
	float m_fLastDot;
	bool m_bUseLastOp;
	bool m_bContinuous;
	//int		m_looseAttentionId;
	bool m_bInitialized;
	bool m_bUseBodyDir;
public:
	COPLookAt(float startangle, float endangle, int mode = 0, bool bUseLastOp = false); 
	~COPLookAt();

	EGoalOpResult Execute(CPipeUser *pOperand);
	void Reset(CPipeUser *pOperand);
	void ResetLooseTarget(CPipeUser *pOperand, bool bForceReset = false);
	void OnObjectRemoved(CAIObject *pObject);
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
};


////////////////////////////////////////////////////////////
//
//				CONTINUOUS - continuous movement, keep on going in last movement direction. Not to stop while tracing path. 
//
////////////////////////////////////////////////////////
class COPContinuous: public CGoalOp
{
	bool	m_bKeepMoving;
public:
	COPContinuous(bool bKeepMoving = true);
	~COPContinuous();

	EGoalOpResult Execute(CPipeUser *pOperand);
	void Reset(CPipeUser *pOperand);
};



////////////////////////////////////////////////////////////
//
//				MOVE - move in specified direction without pathfinding
//
////////////////////////////////////////////////////////
class COPMove: public CGoalOp
{
	CAIObject*	m_moveTarget;
	CAIObject*	m_sightTarget;

	Vec3	m_debug[5];

	float	m_distance;
	float	m_speed;
	bool	m_continuous;
	bool	m_useLastOpResult;
	bool	m_lookAtLastOp;
	bool	m_initialized;

	float		m_stuckTime;
	int			m_allStuckCounter;
	int		m_looseAttentionId;


public:
	COPMove(float distance, float speed, bool bUseLastOp, bool bLookatLastOp, bool bContinuous);
	~COPMove();

	void SteerToTarget(CPipeUser *pOperand);

	EGoalOpResult Execute(CPipeUser *pOperand);
	void ExecuteDry(CPipeUser *pOperand);
	void Reset(CPipeUser *pOperand);
	void OnObjectRemoved(CAIObject *pObject);
	void DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const;
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
};


////////////////////////////////////////////////////////////
//
//				CHARGE - chase target and charge when condition is met.
//
////////////////////////////////////////////////////////
class COPCharge: public CGoalOp
{
	CAIObject*	m_moveTarget;
	CAIObject*	m_sightTarget;
	int		m_looseAttentionId;

	Vec3	m_debugAntenna[9];
	Vec3	m_debugRange[2];
	Vec3	m_debugHit[2];
	bool	m_debugHitRes;
	float	m_debugHitRad;

	int		m_state;
	enum EChargeState
	{
		STATE_APPROACH,
		STATE_ANTICIPATE,
		STATE_CHARGE,
		STATE_FOLLOW_TROUGH,
	};

	float	m_distanceFront;
	float	m_distanceBack;
	bool	m_continuous;
	bool	m_useLastOpResult;
	bool	m_lookAtLastOp;
	bool	m_initialized;
	CTimeValue m_anticipateTime;
	CTimeValue m_approachStartTime;
	bool	m_bailOut;

	float		m_stuckTime;

	Vec3	m_chargePos;
	Vec3	m_chargeStart;
	Vec3	m_chargeEnd;
	Vec3	m_lastOpPos;

	CPipeUser*	m_pOperand;

	void	ValidateRange();
	void	SetChargeParams();
	void	UpdateChargePos();

public:
	COPCharge(float distanceFront, float distanceBack, bool bUseLastOp, bool bLookatLastOp);
	~COPCharge();

	void SteerToTarget();

	EGoalOpResult Execute(CPipeUser* pOperand);
	void ExecuteDry(CPipeUser* pOperand);
	void Reset(CPipeUser* pOperand);
	void OnObjectRemoved(CAIObject* pObject);
	void DebugDraw(CPipeUser* pOperand, IRenderer* pRenderer) const;
};

////////////////////////////////////////////////////////
//
//	waitsignal - waits for a signal and counts down a timer...
//
////////////////////////////////////////////////////////
class COPWaitSignal: public CGoalOp
{
	string m_sSignal;
	enum _edMode
	{
		edNone,
		edString,
		edInt,
		edId
	} m_edMode;
	string m_sObjectName;
	int m_iValue;
	EntityId m_nID;

	CTimeValue m_startTime;
	float m_fInterval;
	bool m_bSignalReceived;

public:
	COPWaitSignal( const char* sSignal, float fInterval = 0 );
	COPWaitSignal( const char* sSignal, const char* sObjectName, float fInterval = 0 );
	COPWaitSignal( const char* sSignal, int iValue, float fInterval = 0 );
	COPWaitSignal( const char* sSignal, EntityId nID, float fInterval = 0 );

	EGoalOpResult Execute( CPipeUser* pOperand );
	void Reset( CPipeUser* pOperand) ;
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);

	bool NotifySignalReceived( CAIObject* pOperand, const char* szText, IAISignalExtraData* pData );
};

////////////////////////////////////////////////////////
//
//	animation -  sets AG input.
//
////////////////////////////////////////////////////////
class COPAnimation : public CGoalOp
{
protected:
	EAnimationMode m_eMode;
	string m_sValue;

	bool m_bAGInputSet;

public:
	COPAnimation( EAnimationMode mode, const char* value );
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);

	EGoalOpResult Execute( CPipeUser* pOperand );
	void Reset( CPipeUser* pOperand );
};

////////////////////////////////////////////////////////
//
//	exact positioning animation to be played at the end of the path
//
////////////////////////////////////////////////////////
class COPAnimTarget: public CGoalOp
{
	bool	m_bSignal;
	string	m_sAnimName;
	float	m_fStartRadius;
	float	m_fDirectionTolerance;
	float	m_fTargetRadius;
	Vec3	m_vApproachPos;

public:
	COPAnimTarget( bool signal, const char* animName, float startRadius, float dirTolerance, float targetRadius, const Vec3& approachPos );

	EGoalOpResult Execute( CPipeUser* pOperand );
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
};

////////////////////////////////////////////////////////
//
//	wait for group of goals to be executed
//
////////////////////////////////////////////////////////
class COPWait: public CGoalOp
{
	EOPWaitType	m_WaitType;
	int					m_BlockCount;	//	number of non-blocking goals in the block

public:
	COPWait( int waitType, int blockCount );

	EGoalOpResult Execute( CPipeUser* pOperand );
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
};

////////////////////////////////////////////////////////
//
//	Adjust aim while staying still.
//
////////////////////////////////////////////////////////
class COPAdjustAim: public CGoalOp
{
	CTimeValue	m_startTime;
	float				m_evalTime;
	bool				m_hide;
	bool				m_useLastOpAsBackup;
	bool				m_allowProne;

	float	RandomizeEvalTime();

public:
	COPAdjustAim(bool hide, bool useLastOpAsBackup, bool allowProne);

	virtual EGoalOpResult Execute(CPipeUser* pOperand);
	virtual void Reset( CPipeUser* pOperand );
	virtual void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
	virtual void DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const;
};


////////////////////////////////////////////////////////////
//
// SEEKCOVER - Seek cover dynamically
//
////////////////////////////////////////////////////////

class COPSeekCover: public CGoalOp
{
public:
	COPSeekCover(bool uncover, float radius, int iterations, bool useLastOpAsBackup, bool towardsLastOpResult);
	~COPSeekCover();

	EGoalOpResult Execute(CPipeUser *pOperand);
	void ExecuteDry(CPipeUser *pOperand);
	void Reset(CPipeUser *pOperand);
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
	void DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const;

private:

	struct SAIStrafeCoverPos
	{
		SAIStrafeCoverPos(const Vec3& pos, bool vis, bool valid, IAISystem::ENavigationType navType) :
			pos(pos), vis(vis), valid(valid), navType(navType) {}
		Vec3 pos;
		bool vis;
		bool valid;
		IAISystem::ENavigationType navType;
		std::vector<SAIStrafeCoverPos> child;
	};

	bool IsTargetVisibleFrom(const Vec3& from, const Vec3& targetPos);

	typedef std::pair<Vec3, float>	Vec3FloatPair;

	bool IsSegmentValid(IAISystem::tNavCapMask navCap, float rad, const Vec3& posFrom, Vec3& posTo, IAISystem::ENavigationType& navTypeFrom);
	void CalcStrafeCoverTree(SAIStrafeCoverPos& pos, int i, int j, const Vec3& center, const Vec3& forw, const Vec3& right,
		float radius, const Vec3& target, IAISystem::tNavCapMask navCap, float passRadius, bool insideSoftCover,
		const std::vector<Vec3FloatPair>& avoidPos, const std::vector<Vec3FloatPair>& softCoverPos);

	bool GetPathTo(SAIStrafeCoverPos& pos, SAIStrafeCoverPos* pTarget, CNavPath& path, const Vec3& dirToTarget, IAISystem::tNavCapMask navCap, float passRadius);
	void DrawStrafeCoverTree(IRenderer *pRenderer, SAIStrafeCoverPos& pos, const Vec3& target) const;

	bool OverlapSegmentAgainstAvoidPos(const Vec3& from, const Vec3& to, float rad, const std::vector<Vec3FloatPair>& avoidPos);
	bool OverlapCircleAgaintsAvoidPos(const Vec3& pos, float rad, const std::vector<Vec3FloatPair>& avoidPos);
	void GetNearestPuppets(CPuppet* pSelf, const Vec3& pos, float radius, std::vector<Vec3FloatPair>& positions);
	bool IsInDeepWater(const Vec3& pos);

	SAIStrafeCoverPos* GetFurthestFromTarget(SAIStrafeCoverPos& pos, const Vec3& center, const Vec3& forw, const Vec3& right, float& maxDist);
	SAIStrafeCoverPos* GetNearestToTarget(SAIStrafeCoverPos& pos, const Vec3& center, const Vec3& forw, const Vec3& right, float& minDist);
	SAIStrafeCoverPos* GetNearestHidden(SAIStrafeCoverPos& pos, const Vec3& from, float& minDist);

	void UsePath(CPipeUser* pOperand, SAIStrafeCoverPos* pos, IAISystem::ENavigationType navType);

	SAIStrafeCoverPos* m_pRoot;

	bool m_uncover;
	int m_state;
	float m_radius;
	int m_iterations;
	float m_endAccuracy;
	bool m_useLastOpAsBackup;
	bool m_towardsLastOpResult;
	CTimeValue m_lastTime;
	float m_notMovingTime;
	COPTrace* m_pTraceDirective;
	Vec3 m_center, m_forward, m_right;
	Vec3 m_target;
	std::vector<Vec3FloatPair> m_avoidPos;
	std::vector<Vec3FloatPair> m_softCoverPos;
};


////////////////////////////////////////////////////////////
//
// PROXIMITY - Send signal on proximity
//
////////////////////////////////////////////////////////

class COPProximity : public CGoalOp
{
public:
	COPProximity(float radius, const string& signalName, bool signalWhenDisabled, bool visibleTargetOnly);
	~COPProximity();

	virtual EGoalOpResult Execute(CPipeUser *pOperand);
	virtual void Reset(CPipeUser *pOperand);
	virtual void OnObjectRemoved(CAIObject *pObject);
	virtual void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
	virtual void DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const;

private:
	float m_radius;
	bool m_triggered;
	bool m_signalWhenDisabled;
	bool m_visibleTargetOnly;
	string m_signalName;
	CAIObject* m_pProxObject;
};

////////////////////////////////////////////////////////////
//
//				MOVETOWARDS - move specified distance towards last op result
//
////////////////////////////////////////////////////////
class COPMoveTowards: public CGoalOp
{
public:
	COPMoveTowards(float distance, float duration);
	~COPMoveTowards();

	void Reset(CPipeUser *pOperand);
	EGoalOpResult Execute(CPipeUser *pOperand);
	void ExecuteDry(CPipeUser *pOperand);
	void OnObjectRemoved(CAIObject *pObject);
	void DebugDraw(CPipeUser* pOperand, IRenderer* pRenderer) const;
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
private:
	void ResetNavigation(CPipeUser* pOperand);

private:

	float GetEndDistance(CPipeUser *pOperand) const;

	float m_distance;
	float m_duration;
	int		m_looseAttentionId;
	Vec3	m_moveStart;
	Vec3	m_moveEnd;
	float	m_moveDist;
	float	m_moveSearchRange;

	COPTrace* m_pTraceDirective;
	COPPathFind* m_pPathfindDirective;
};


////////////////////////////////////////////////////////////
//
// DODGE - Dodges a target
//
////////////////////////////////////////////////////////

class COPDodge: public CGoalOp
{
public:
	COPDodge(float distance, bool useLastOpAsBackup);
	~COPDodge();

	EGoalOpResult Execute(CPipeUser *pOperand);
	void ExecuteDry(CPipeUser *pOperand);
	void Reset(CPipeUser *pOperand);
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
	void DebugDraw(CPipeUser *pOperand, IRenderer *pRenderer) const;

private:

	typedef std::pair<Vec3, float>	Vec3FloatPair;

	bool OverlapSegmentAgainstAvoidPos(const Vec3& from, const Vec3& to, float rad, const std::vector<Vec3FloatPair>& avoidPos);
	void GetNearestPuppets(CPuppet* pSelf, const Vec3& pos, float radius, std::vector<Vec3FloatPair>& positions);

	float m_distance;
	bool m_useLastOpAsBackup;
	float m_endAccuracy;
	CTimeValue m_lastTime;
	float m_notMovingTime;
	COPTrace* m_pTraceDirective;
	std::vector<Vec3FloatPair> m_avoidPos;

	std::vector<Vec3> m_DEBUG_testSegments;
	Matrix33	m_basis;
	Vec3 m_targetPos;
	Vec3 m_targetView;
	Vec3 m_bestPos;
};




#endif // !defined(AFX_GOALOP_H__1CD1CEF7_0DC6_4C6C_BCF1_EDE36179F7E7__INCLUDED_)
