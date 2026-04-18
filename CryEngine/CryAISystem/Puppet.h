// Puppet.h: interface for the CPuppet class.
//
//////////////////////////////////////////////////////////////////////

#ifndef PUPPET_H
#define PUPPET_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// Forward declaration.
class CPuppet;

#include "CAISystem.h"
#include "PipeUser.h"
#include "AgentParams.h"
#include "IAgent.h"
#include "AIMovementCommon.h"
#include "GoalPipe.h"
#include "Graph.h"
#include "TrackPattern.h"
#include "AIPIDController.h"
#include "FireCommand.h"
#include "PathObstacles.h"

#include <list>
#include <map>
#include <vector>


enum EPuppetUpdatePriority
{
	AIPUP_VERY_HIGH,
	AIPUP_HIGH,
	AIPUP_MED,
	AIPUP_LOW,
};

struct SAIPotentialTarget
{
	CAIObject* pDummyRepresentation;

	EAITargetType type;				// Type of the event

	float priority;						// Priority of the event
	float upPriority;					// Extra priority set from outside the system.
	float upPriorityTime;			// Timeout for the extra priority.

	float soundTime;					// Current time of the sound event counting down from maxTime
	float soundMaxTime;				// Maximum duration of the sound event
	float soundThreatLevel;		// Threat level of the sound
	Vec3 soundPos;						// Position of the sound event

	enum EVisualType
	{
		VIS_NONE,
		VIS_VISIBLE,
		VIS_MEMORY,
		VIS_LAST // For serialization only.
	};

	float visualThreatLevel;	// Threat level of the sight.
	float visualTime;					// Current time of the sight event counting down from maxTime
	float visualMaxTime;			// Max duration of the sight event.
	int visualFrameId;
	Vec3 visualPos;						// Last seen position.
	bool indirectSight;				// Seeing something realted to the target (i.e. flashlight), but there is no direct LOS.
	EVisualType visualType;		// Type of the sight event.
	
	float exposure;						// Target exposure level, triggers the threat level up.
	EAITargetThreat threat;		// Threat level of the target.
	float threatTimeout;			// Remaining time of the threat level.
	float threatTime;					// Combined threat+threatTimeout in range [0..1]
	EAITargetThreat exposureThreat;	// Threat level of the target exposure.

	// Returns the total time until the perception will time out.
	inline float GetTimeout(const AgentPerceptionParameters& pp) const
	{
		float timeout = 0.0f;
		if (threat == AITHREAT_AGGRESSIVE)
		{
			timeout += pp.forgetfulnessMemory;
			timeout += pp.forgetfulnessSeek;
		}
		else if (threat == AITHREAT_THREATENING)
		{
			timeout += pp.forgetfulnessMemory;
		}
		timeout += threatTimeout;
		return timeout;
	}

	SAIPotentialTarget() :
		type(AITARGET_NONE),
		priority(0.0f),
		soundTime(0.0f),
		soundMaxTime(0.0f),
		soundThreatLevel(0.0f),
		soundPos(0,0,0),
		visualTime(0.0f),
		visualMaxTime(0.0f),
		visualThreatLevel(0.0f),
		visualPos(0,0,0),
		visualType(VIS_NONE),
		exposure(0.0),
		threat(AITHREAT_NONE),
		exposureThreat(AITHREAT_NONE),
		threatTimeout(0.0f),
		threatTime(0.0f),
		upPriority(0.0f),
		upPriorityTime(0.0f),
		indirectSight(false),
		pDummyRepresentation(0),
		visualFrameId(0)
	{
	}

	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
};

struct CSpeedControl	
{
	float	fPrevDistance;
	CTimeValue	fLastTime;
	Vec3	vLastPos;
	CAIPIDController PIDController;
	static const float	m_CMaxDist;

	CSpeedControl(): vLastPos(0.0f,0.0f,0.0f), fPrevDistance(0.0f){}
	void	Reset(const Vec3 vPos, CTimeValue fTime) 
	{
		fPrevDistance = 0;
		fLastTime=fTime; 
		vLastPos = vPos;
	}
};

typedef std::map<CAIObject*, SAIPotentialTarget> PotentialTargetMap;
typedef std::multimap<float, SHideSpot> MultimapRangeHideSpots;

class CGoalOp;
class CSquadMember;

typedef std::vector<QGoal> VectorOGoals;
typedef std::map<CAIObject*,float> DevaluedMap;
typedef std::map<CAIObject*,CAIObject*> ObjectObjectMap;

struct SShootingStatus
{
	bool			triggerPressed;							// True if the weapon trigger is being pressed.
	float			timeSinceTriggerPressed;		// Time in seconds since last time trigger is pressed.
	bool			friendOnWay;								// True if the fire if blocked by friendly unit.
	float			friendOnWayElapsedTime;			// Time in seconds the line of fire is blocked by friendly unit.
	EFireMode	fireMode;										// Fire mode status, see EFireMode.
};


class CPuppet : public CPipeUser, protected IPuppet
{
friend class CAISystem;
public:
	CPuppet(CAISystem*);
	virtual ~CPuppet();

	virtual const IPuppet* CastToIPuppet() const { return this; }
	virtual IPuppet* CastToIPuppet() { return this; }

	virtual bool IsPointInAudibleRange(const Vec3 &pos, float fRadius);
	virtual bool IsPointInFOVCone(const Vec3 &pos, float distanceScale = 1.0f);

	virtual void AdjustSpeed(CAIObject* pNavTarget, float distance=0);
	virtual void ResetSpeedControl();
  virtual bool GetValidPositionNearby(const Vec3 &proposedPosition, Vec3 &adjustedPosition) const;
  virtual bool GetTeleportPosition(Vec3 &teleportPos) const;
	virtual bool GetPosAlongPath(float dist, bool extrapolateBeyond, Vec3& retPos) const;
	virtual IFireCommandHandler* GetFirecommandHandler() const {return m_pFireCmdHandler;}

	virtual bool IsObjectInFOVCone(CAIObject* pTarget, float distanceScale = 1.0f);

	void SetAllowedToHitTarget(bool state) { m_allowedToHitTarget = state; }
	bool IsAllowedToHitTarget() const { return m_allowedToHitTarget; }

	void SetAllowedToUseExpensiveAccessory(bool state) { m_allowedToUseExpensiveAccessory = state; }
	bool IsAllowedToUseExpensiveAccessory() const { return m_allowedToUseExpensiveAccessory; }

	// Returns the AI object to shoot at.
	CAIObject* GetFireTargetObject() const;

	// Returns the reaction time of between allowed to fire and allowed to damage target.
	float GetFiringReactionTime(const Vec3& targetPos) const;

	// Returns true if the reaction time pas passed.
	bool HasFiringReactionTimePassed() const { return m_firingReactionTimePassed; }

	// Returns the current status shooting related parameters.
	void	GetShootingStatus(SShootingStatus& ss);

	// Sets the allowed start and end of the path strafing distance. 
	void SetAllowedStrafeDistances(float start, float end, bool whileMoving);

	// Sets the allowed start and end of the path strafing distance. 
	void SetAdaptiveMovementUrgency(float minUrgency, float maxUrgency, float scaleDownPathlen);

	// Sets the stance that will be used when the character starts to move.
	void SetDelayedStance(int stance);

	void AdjustMovementUrgency(float& urgency, float pathLength, float* maxPathLen = 0);

	/// Returns all the path obstacles that might affect paths. If allowRecalc = false then the cached obstacles
	/// will not be updated
	const CPathObstacles &GetPathAdjustmentObstacles(bool allowRecalc = true) {if (allowRecalc) CalculatePathObstacles(); return m_pathAdjustmentObstacles;}
	const CPathObstacles &GetLastPathAdjustmentObstacles() const { return m_pathAdjustmentObstacles; }

	// current accuracy (with soft cover and motion correction, distance/stance, ....)
	float	GetAccuracy(const CAIObject *pTarget) const;

  /// inherited
  virtual void UpdateEntitiesToSkipInPathfinding();

	/// inherited
	virtual DamagePartVector*	GetDamageParts();

	// Returns the memory usage of the object.
	size_t MemStats();

	// Inherited from IPuppet
	virtual IAIObject* GetTargetTrackPoint();
	virtual void UpTargetPriority(const IAIObject *pTarget, float fPriorityIncrement);
	virtual void UpdateBeacon();
	virtual IAIObject* MakeMeLeader();
	virtual IAIObject* GetEventOwner(IAIObject* pOwned) const;
	virtual bool CheckFriendsInLineOfFire(const Vec3& fireDir, bool cheapTest);
	virtual Vec3 GetFloorPosition(const Vec3& pos);
	virtual void SetTerritoryShapeName(const char* name);
	virtual const char* GetTerritoryShapeName();
	SShape* GetTerritoryShape();

	// trace from weapon pos to target body/eye, 
	// returns true if good to shoot
	// in:	pTarget
	//			lowDamage	- low accuracy, should hit arms/legs if possible
	// out: vTargetPos, vTargetDir, distance
	bool CheckAndGetFireTarget_Deprecated(IAIObject* pTarget, bool lowDamage, Vec3 &vTargetPos, Vec3 &vTargetDir) const;
	virtual Vec3 ChooseMissPoint_Deprecated(const Vec3 &targetPos) const;

	// Inherited from IPipeUser
	virtual void Devalue(IAIObject *pObject, bool bDevaluePuppets, float fAmount=20.f);
	virtual bool IsDevalued(IAIObject *pObject);
	virtual void ClearDevalued();

	// Inherited from IAIObject
	virtual void SetParameters(AgentParameters & sParams);
	virtual void Event(unsigned short eType, SAIEVENT *pEvent);
	virtual void ParseParameters(const AIObjectParameters &params);
	virtual bool CreateFormation(const char * szName, Vec3 vTargetPos);
	virtual void Serialize(TSerialize ser, class CObjectTracker& objectTracker);
	virtual void SetPFBlockerRadius(int blockerType, float radius);
	virtual void SetWeaponDescriptor(const AIWeaponDescriptor& descriptor);

	// Inherited from CAIObject
	virtual void Update(EObjectUpdate type);
	virtual IPhysicalEntity* GetPhysics(bool wantCharacterPhysics=false) const;
	virtual void OnObjectRemoved(CAIObject *pObject);
	virtual void Reset(EObjectResetType type);
	virtual void AddNavigationBlockers(NavigationBlockers& navigationBlockers, const struct PathFindRequest *pfr) const;
	virtual ETriState CanTargetPointBeReached(CTargetPointRequest &request);
	virtual bool UseTargetPointRequest(const CTargetPointRequest &request);

	// <title GetDistanceAlongPath>
	// Description: returns the distance of a point along the puppet's path
	// Arguments:
	// Vec3& pos: point
	// bool bInit: if true, the current path is stored in a internal container; if false, the distance is computed
	//		along the stored path, no matter if the current path is different
	// Returns:
	// distance of point along Puppets path; distance value would be negative if the point is ahead along the path
	virtual float	GetDistanceAlongPath(const Vec3& pos, bool bInit);
	// Inherited from CPipeUser
	virtual void ClearPotentialTargets();
	virtual void UpdateLookTarget(CAIObject *pTarget);
	virtual void UpdateLookTarget3D(CAIObject *pTarget);
  virtual bool NavigateAroundObjects(const Vec3 & targetPos, bool fullUpdate, bool bSteerAroundTarget=true);
	virtual void GetMovementSpeedRange(float fUrgency, bool slowForStrafe, float& normalSpeed, float& minSpeed, float& maxSpeed) const;

	virtual Vec3 FindHidePoint(float fSearchDistance, const Vec3 &hideFrom, int nMethod, IAISystem::ENavigationType navType, bool bSameOk = false, float minDist = 0, SDebugHideSpotQuery* pDebug = 0);

  /// Like NavigateAroundObjects but navigates around an individual AI obstacle (implemented by the puppet/vehicle).
  /// If steer is true then navigation should be done by steering. If false then navigation should be
  /// done by slowing down/stopping.
  virtual bool NavigateAroundAIObject(const Vec3 &targetPos, const CAIObject *obstacle, const Vec3 &predMyPos, const Vec3 &predObjectPos, bool steer, bool in3D);

  // AIOT_PLAYER is to be treated specially, as it's the player, but otherwise is the same as
  // AIOT_AGENTSMALL
  enum EAIObjectType {AIOT_UNKNOWN, AIOT_PLAYER, AIOT_AGENTSMALL, AIOT_AGENTMED, AIOT_AGENTBIG, AIOT_MAXTYPES};
  // indicates the sort of object the agent is with regard to navigation
  static EAIObjectType GetObjectType(const CAIObject *ai, unsigned short type);
  enum ENavInteraction {NI_IGNORE, NI_STEER, NI_SLOW};
  // indicates the way that two objects should negotiate each other
  static ENavInteraction GetNavInteraction(const CAIObject *navigator, const CAIObject *obstacle);

	// Returns the attention target type as the Puppet understands it internally, see ePerceivedAttentionTargetType.
//	int	GetPerceivedAttentionTargetType();

	// Does some tests to find the best posture (stance and lean) for the current situation, returns true if a good posture found.
	bool	SelectAimPosture(const Vec3& targetPos, EStance& outStance, float& outLean, bool allowLean = true, bool allowProne = false);

	// Does some tests to find the best posture (stance and lean) for the current situation, returns true if a good posture found.
	bool	SelectHidePosture(const Vec3& targetPos, EStance& outStance, float& outLean, bool allowLean = true, bool allowProne = false);

	// Returns the current alertness level of the puppet.
	inline int GetAlertness() const { return m_Alertness; }

	// check if enemy is close enough and send OnCloseContact if so
	void	CheckCloseContact(CPuppet* pEnemyPuppet, float dist);

	float	GetSightRange(const CAIObject* pTarget) const;
	virtual bool	IsActive() const { return m_bEnabled; }
	inline EPuppetUpdatePriority	GetUpdatePriority() const { return m_updatePriority; }
	inline void SetUpdatePriority(EPuppetUpdatePriority pri) {m_updatePriority = pri; }

	inline const AIWeaponDescriptor& GetWeaponDescriptor() const { return m_CurrentWeaponDescriptor; }
	// Returns true if it is possible to aim at the target without colliding with a wall etc in front of the puppet.
	bool CanAimWithoutObstruction(const Vec3 &vTargetPos);


	PotentialTargetMap m_perceptionEvents;
	CTrackPattern* m_pTrackPattern;
	IFireCommandHandler* m_pFireCmdHandler;
	CFireCommandGrenade* m_pFireCmdGrenade;

	float					m_targetApproach;
	float					m_targetFlee;
	bool					m_targetApproaching;
	bool					m_targetFleeing;
	bool					m_lastTargetValid;
	Vec3					m_lastTargetPos;
	float					m_lastTargetSpeed;

	int64					m_lastPuppetVisCheck;

protected:
	typedef enum 
	{
		PA_NONE=0,
		PA_LOOKING,
		PA_STICKING
	} TPlayerActionType;


	static const float COMBATCLASS_IGNORE_TRESHOLD;

	inline bool IsAffectedByLightLevel() const { return m_Parameters.m_PerceptionParams.isAffectedByLight; }

	void RemoveFromGoalPipe(CAIObject* pObject);
	void CheckAwarenessPlayer(); // check if the player is sticking or looking at me

	SAIPotentialTarget* AddEvent(CAIObject* pObject, SAIPotentialTarget& ed);
//	float CalculatePriority(CAIObject * pTarget);

	// Returns priority multiplier based on species, object and target type.
	float GetPriorityMultiplier(CAIObject* pTarget);

	void	UpdatePuppetInternalState();
	
	void	HandleSoundEvent(SAIEVENT *pEvent);
	void	HandlePathDecision(SAIEVENT *pEvent);
	void	HandleVisualStimulus(SAIEVENT *pEvent);

	virtual void AdjustPath();
	void	TransformFOV();
//	float GetEventPriority(const CAIObject *pObj,const EventData &ed) const;
	void	UpdateAlertness();

	// Steers the puppet around the object and makes it avoid the immediate obstacles. Returns true if steering is 
  // being done
	bool SteerAroundVehicle(const Vec3 &targetPos, const CAIObject *object, const Vec3 &predMyPos, const Vec3 &predObjectPos);
  bool SteerAroundPuppet(const Vec3 &targetPos, const CAIObject *object, const Vec3 &predMyPos, const Vec3 &predObjectPos);
	bool SteerAround3D(const Vec3 &targetPos, const CAIObject *object, const Vec3 &predMyPos, const Vec3 &predObjectPos);
  bool SteerAroundFlight(const Vec3 &targetPos, const CAIObject *object, const Vec3 &predMyPos, const Vec3 &predObjectPos);
  /// Helper for NavigateAroundObjects
  bool NavigateAroundObjectsInternal(const Vec3 & targetPos, const Vec3 &myPos, bool in3D, const CAIObject *object);
  /// Helper for NavigateAroundObjects
  bool NavigateAroundObjectsBasicCheck(const Vec3 & targetPos, const Vec3 &myPos, bool in3D, const CAIObject *object, float extraDist);

  /// Adjust the path to plan "local" steering. Returns true if the resulting path can be used, false otherwise
  bool AdjustPathAroundObstacles();

	// decides whether to fire or not
	void FireCommand();
	// resets last missing point tracking (should accurate once in a while - (when burst is over?))
	void	ResetMiss();
	// process secondary fire
	void FireSecondary(CAIObject*	pTarget);
	// process melee fire
	void FireMelee(CAIObject*	pTarget);

	bool CheckTargetInRange(Vec3& vTargetPos);

  /// Selects the best hide points from a list that should just contain accessible hidespots
	Vec3 GetHidePoint(MultimapRangeHideSpots &hidespots, float fSearchDistance, const Vec3 &hideFrom, int nMethod, bool bSameOk, float fMinDistance);

	// Evaluates whether the chosen navigation point will expose us too much to the target
	bool Compromising(const Vec3& pos, const Vec3& dir, const Vec3& hideFrom, const Vec3& objectPos, const Vec3& searchPos, bool bIndoor, bool bCheckVisibility) const;

	void RegisterTargetAwareness(float amount);

	void UpdateTargetMovementState();

	bool IsFriendInLineOfFire(CAIObject* pFriend, const Vec3& firePos, const Vec3& fireDirection, bool cheapTest);

	inline bool CloseContactEnabled() {return !m_bCloseContact;}
	void SetCloseContact(bool bContact);

	void AdjustWithPrediction(CAIObject* pTarget, Vec3& posOut);

	void AffectSOM(CAIObject*	pTarget, float exposure, float threat);

	// Returns true if it pActor is obstructing aim.
	bool ActorObstructingAim(const CAIActor* pActor, const Vec3& firePos, const Vec3& dir, const Ray& fireRay, float checkDist);

	CTimeValue		m_fLastUpdateTime;

	DevaluedMap		m_mapDevaluedPoints;

	bool					m_bDryUpdate;

	float					m_fCurrentAwarenessLevel;

	int						m_Alertness;
	float					m_FOVPrimaryCos;
	float					m_FOVSecondaryCos;

	float					m_fAccuracyMotionAddition;
	float					m_fAccuracySupressor;
	float					m_fSuppressFiring;	// in seconds
	float					m_fLastTimeAwareOfPlayer;
	float					m_fLastStuntTime;
	TPlayerActionType		m_playerAwarenessType;

	bool					m_allowedToHitTarget;
	bool					m_allowedToUseExpensiveAccessory;
	bool					m_firingReactionTimePassed;
	float					m_firingReactionTime;
	float					m_outOfAmmoTimeOut;

	bool					m_bWarningTargetDistance;
//	bool					m_bInFrontOfPlayer;	// this one should be set when puppet is in player's FOV, 
																		// to determine active/passive combat states (near misses/ambient combat)
	bool					m_bCloseContact;		// used to prevent the signal OnCloseContact being sent repeatedly to same object
	CTimeValue		m_CloseContactTime; // timeout for closecontact

	Vec3					m_LastMissPoint;

	float					m_fTargetAwareness;					// how much I perceive the target.

	float					m_friendOnWayCounter;

	CSpeedControl m_SpeedControl;
	SPID					m_SpeedPID;
	float					m_ControlledSpeed;
	int						m_SpeedControlFlipCounter;

  // for smoothing the output of AdjustSpeed
  float m_chaseSpeed;
  float m_chaseSpeedRate;
	int	m_lastChaseUrgencyDist;
	int	m_lastChaseUrgencySpeed;

	CAIObject*		m_pAvoidedVehicle;
	CTimeValue		m_vehicleAvoidingTime;

	Vec3		m_vForcedNavigation;
	int			m_adjustpath;

	float m_bodyTurningSpeed;
	Vec3 m_lastBodyDir;

//	IFireCommandHandler*	m_pFireCmdHandler;

	// map blocker_type-radius
	typedef std::map< int, float > TMapBlockers;
	TMapBlockers	m_PFBlockers;

	//my current weapon
	AIWeaponDescriptor	m_CurrentWeaponDescriptor;

	float	m_allowedStrafeDistanceStart;	// Allowed strafe distance at the start of the path
	float	m_allowedStrafeDistanceEnd;		// Allowed strafe distance at the end of the path
	bool	m_allowStrafeLookWhileMoving;	// Allow strafing when moving along path.
	bool	m_closeRangeStrafing;

	float m_strafeStartDistance;

	float m_adaptiveUrgencyMin;
	float m_adaptiveUrgencyMax;
	float m_adaptiveUrgencyScaleDownPathLen;
	float m_adaptiveUrgencyMaxPathLen;

	int m_delayedStance;
	int m_delayedStanceMovementCounter;

	enum ESeeTargetFrom
	{
		ST_HEAD,
		ST_WEAPON,
		ST_OFSETTED_LEFT,
		ST_OFSETTED_RIGHT,
	};
	ESeeTargetFrom	m_SeeTargetFrom;
	float m_lastTimeSeeFromHead;

	float	m_timeSinceTriggerPressed;
	bool	m_bCoverFireEnabled;
	float	m_friendOnWayElapsedTime;

	DamagePartVector	m_damageParts;

	string	m_territoryShapeName;
	SShape*	m_territoryShape;

  /// Private helper - calculates the path obstacles - in practice a previous result may
  /// be used if it's not out of date
  void CalculatePathObstacles();
  CPathObstacles m_pathAdjustmentObstacles;

	typedef std::list<Vec3> TPointList;
	TPointList m_InitialPath;

	void UpdateHealthTracking();

	
public:

	// Returns the attention target if it is an agent, or the attention target association, or null if there is no association.
	inline const CAIObject* GetLiveTarget(const CAIObject* pTarget) const
	{
		if (!pTarget)
			return 0;
		if (const CAIActor* pActor = pTarget->CastToCAIActor())
			if (!pActor->IsActive())
				return 0;
		if (pTarget->IsAgent())
			return pTarget;
		const CAIObject* pAssociation = (CAIObject*)pTarget->GetAssociation();
		if (pAssociation && pAssociation->IsEnabled() && pAssociation->IsAgent())
			return pAssociation;
		return 0;
	}

	inline CAIObject* GetLiveTarget(CAIObject* pTarget) const
	{
		if (!pTarget)
			return 0;
		if (CAIActor* pActor = pTarget->CastToCAIActor())
			if (!pActor->IsActive())
				return 0;
		if (pTarget->IsAgent())
			return pTarget;
		CAIObject* pAssociation = (CAIObject*)pTarget->GetAssociation();
		if (pAssociation && pAssociation->IsEnabled() && pAssociation->IsAgent())
			return pAssociation;
		return 0;
	}


	void ResetTargetTracking();
	Vec3 UpdateTargetTracking(CAIObject* pTarget, const Vec3& targetPos);
	bool CheckAndAdjustFireTarget(const Vec3& targetPos, float extraRad, Vec3& posOut, float clampAngle, float maxMoveDelta);
	bool CanDamageTarget();
	bool GetHitPointOnTarget(CAIObject* pTarget, const Vec3& targetPos, Vec3& posOut, float clampAngle);
	bool IsFireTargetValid(const Vec3& pos, const CAIActor* pTargetObject);

	float GetTargetAliveTime(float* outMoveMod = 0, float* outStanceMod = 0, float* outDirMod = 0, float* outHitMod = 0, float* outZoneMod = 0);

	Vec3	InterpolateLookOrAimTargetPos(const Vec3& current, const Vec3& target, float maxRate);

	void HandleWeaponEffectBurstWhileMoving(CAIObject* pTarget, Vec3& aimTarget, bool& canFire);
	void HandleWeaponEffectBurstDrawFire(CAIObject* pTarget, Vec3& aimTarget, bool& canFire);
	void HandleWeaponEffectBurstSnipe(CAIObject* pTarget, Vec3& aimTarget, bool& canFire);
	void HandleWeaponEffectPanicSpread(CAIObject* pTarget, Vec3& aimTarget, bool& canFire);
	void HandleWeaponEffectAimSweep(CAIObject* pTarget, Vec3& aimTarget, bool& canFire);

	struct STargetSilhouette
	{
		STargetSilhouette()
		{
			Reset();
			baseMtx.SetIdentity();
		}

		bool valid;
		std::vector<Vec3>	points;
		Matrix33 baseMtx;
		Vec3 center;

		inline Vec3 ProjectVectorOnSilhouette(const Vec3& vec)
		{
			return Vec3(baseMtx.GetColumn0().Dot(vec), baseMtx.GetColumn2().Dot(vec), 0.0f);
		}

		inline Vec3 IntersectSilhouettePlane(const Vec3& from, const Vec3& to)
		{
			if (!valid)
				return to;

			// Intersect (infinite) segment with the silhouette plane.
			Vec3 pn = baseMtx.GetColumn1();
			float pd = -pn.Dot(center);
			Vec3 res(to);
			Vec3 dir = to - from;

			float d = pn.Dot(dir);
			if (fabsf(d) < 1e-6)
				return to;

			float n = pn.Dot(from) + pd;
			return from + dir * (-n/d);
		}

		void Reset()
		{
			valid = false;
			points.clear();
			center.Set(0,0,0);
		}
	};

	enum ETargetZone
	{
		ZONE_KILL,
		ZONE_COMBAT_NEAR,
		ZONE_COMBAT_FAR,
		ZONE_WARN,
		ZONE_OUT,
		LAST_ZONE,
	};

	STargetSilhouette	m_targetSilhouette;
	Vec3 m_targetLastMissPoint;
	Vec3 m_targetPosOnSilhouettePlane;
	Vec3 m_targetBiasDirection;
	float m_targetFocus;
	ETargetZone m_targetZone;
	float m_targetDistanceToSilhouette;
	float m_targetEscapeLastMiss;
	float m_targetSeenTime;
	float m_targetLostTime;
	CTimeValue m_targetLastMissPointSelectionTime;
	float m_targetDazzlingTime;
	float m_burstEffectTime;
	int		m_burstEffectState;

	int		m_LastShotsCount;
	float m_weaponSpinupTime;
	float m_targetDamageHealthThr;
	std::vector<int>	m_targetHitPartIndex;
	bool	m_lastAimObstructionResult;
	EPuppetUpdatePriority	m_updatePriority;

  /// It's quite expensive to walk through all AI objects and find which ones to steer
  /// around so they get cached here and only set on full updates
  std::vector<const CAIObject *> m_steeringObjects;
  CTimeValue m_lastSteerTime;

	CAIRadialOccypancy	m_steeringOccupancy;
	float								m_steeringOccupancyBias;
	bool								m_steeringEnabled;
	float								m_steeringAdjustTime;

	SAIValueHistory* m_targetDamageHealthThrHistory;

	inline CAIObject* GetAvoidedVehicle() {return m_pAvoidedVehicle;}
	inline float GetVehicleAvoidingTime() { return (GetAISystem()->GetFrameStartTime() - m_vehicleAvoidingTime).GetSeconds(); }
	inline void EnableCoverFire(bool enable) {m_bCoverFireEnabled = enable;}
	inline bool IsCoverFireEnabled() const {return m_bCoverFireEnabled ;}

	inline void SetAvoidedVehicle(CAIObject* pVehicle) 
	{ 
		m_pAvoidedVehicle = pVehicle; 
		m_vehicleAvoidingTime = (pVehicle ? GetAISystem()->GetFrameStartTime() : 0.f);
	}

	// Updates the strafing logic.
	void UpdateStrafing();


	// The fire target cache caches raycast results used for
	// testing if a
	class SAIFireTargetCache
	{
	public:
		SAIFireTargetCache() : m_size(0), m_head(0), m_queries(0), m_hits(0) {}

		float QueryCachedResult(const Vec3& pos, const Vec3& dir, float reqDist)
		{
			m_queries++;
			const float distThr = sqr(0.15f);
			const float angleThr = cosf(DEG2RAD(6.0f));
			int j = m_head;
			for (int i = 0; i < m_size; ++i)
			{
				j--;
				if (j < 0) j = CACHE_SIZE-1;

				if ((m_cacheReqDist[j] - 0.5f) >= reqDist &&
						Distance::Point_PointSq(pos, m_cachePos[j]) < distThr &&
						m_cacheDir[j].Dot(dir) > angleThr)
				{
					m_hits++;
					return m_cacheDist[j];
				}
			}
			// Could not find.
			return -1.0f;
		}

		void Insert(const Vec3& pos, const Vec3& dir, float reqDist, float dist)
		{
			m_cachePos[m_head] = pos;
			m_cacheDir[m_head] = dir;
			m_cacheDist[m_head] = dist;
			m_cacheReqDist[m_head] = reqDist;
			if (m_size < CACHE_SIZE)
				m_size++;
			m_head++;
			if (m_head >= CACHE_SIZE)
				m_head = 0;
		}

		int GetQueries() const { return m_queries; }
		int GetHits() const { return m_hits; }

		void Reset()
		{
			m_size = 0;
			m_head = 0;
			m_hits = 0;
			m_queries = 0;
		}

	private:
		static const int CACHE_SIZE = 8;
		Vec3 m_cachePos[CACHE_SIZE];
		Vec3 m_cacheDir[CACHE_SIZE];
		float m_cacheReqDist[CACHE_SIZE];
		float m_cacheDist[CACHE_SIZE];
		int m_size, m_head;
		int m_hits;
		int m_queries;
	};

	SAIFireTargetCache	m_fireTargetCache;

	inline void SetAlarmed()
	{
		m_alarmedTime = GetParameters().m_PerceptionParams.forgetfulnessTarget +
			GetParameters().m_PerceptionParams.forgetfulnessMemory;
	}
	inline bool IsAlarmed() const { return m_alarmedTime > 0.01f; }
	inline float GetPerceptionAlarmLevel() const { return max(GetParameters().m_PerceptionParams.minAlarmLevel, m_alarmedLevel); }
	
	float m_alarmedTime;
	float m_alarmedLevel;
};

inline const CPuppet* CastToCPuppetSafe(const IAIObject* pAI) { return pAI ? pAI->CastToCPuppet() : 0; }
inline CPuppet* CastToCPuppetSafe(IAIObject* pAI) { return pAI ? pAI->CastToCPuppet() : 0; }

#endif
