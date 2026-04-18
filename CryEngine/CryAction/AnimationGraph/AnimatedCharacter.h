#ifndef __ANIMATEDCHARACTER_H__
#define __ANIMATEDCHARACTER_H__

#pragma once

#include "IAnimatedCharacter.h"
#include "IAnimationGraph.h"
#include "AnimationGraph.h"
#include "AnimationGraphStates.h"
#include <queue>
#include "BitFiddling.h"
#include "IDebugHistory.h"

//--------------------------------------------------------------------------------

#define ANIMCHAR_PROFILE_HEAVY

//--------------------------------------------------------------------------------

#define ANIMCHAR_PROFILE									FUNCTION_PROFILER(GetISystem(), PROFILE_GAME)

#ifdef ANIMCHAR_PROFILE_HEAVY
#	define ANIMCHAR_PROFILE_DETAILED				FUNCTION_PROFILER(GetISystem(), PROFILE_GAME)
#else
#	define ANIMCHAR_PROFILE_DETAILED				{}
#endif

#ifdef ANIMCHAR_PROFILE_HEAVY
#	define ANIMCHAR_PROFILE_SCOPE(label)		FRAME_PROFILER(label, GetISystem(), PROFILE_GAME)
#else
#	define ANIMCHAR_PROFILE_SCOPE(label)		{}
#endif

//--------------------------------------------------------------------------------

#ifdef _DEBUG
#define DEBUGHISTORY
#endif

//--------------------------------------------------------------------------------

struct IAnimationBlending;

//--------------------------------------------------------------------------------

#if 0
#define ANIMCHAR_SIZED_VAR(type,name,bits) type name
#else
#define ANIMCHAR_SIZED_VAR(type,name,bits) type name : bits
#endif

//--------------------------------------------------------------------------------
/*
struct IAnimatedCharacterPositionTracker;
typedef std::auto_ptr<IAnimatedCharacterPositionTracker> IAnimatedCharacterPositionTrackerAutoPtr;
struct IAnimatedCharacterPositionTracker
{
	virtual ~IAnimatedCharacterPositionTracker() {}
	virtual IAnimatedCharacterPositionTrackerAutoPtr Update( IEntity * pEnt, const SAnimatedCharacterParams& params, bool animationControlled ) = 0;
	virtual Vec3 GetRenderOffset( IEntity * pEnt, const SAnimatedCharacterParams& params, float desiredSpeed ) = 0;
	virtual void ResetPosition( IEntity * pEnt, const SAnimatedCharacterParams& params ) = 0;
	virtual void ResetRotation( IEntity * pEnt, const SAnimatedCharacterParams& params ) = 0;
	virtual Vec3 ApplyWorldTranslationAndGetAnimationMovement( IEntity * pEnt, const SAnimatedCharacterParams& params, const Vec3& animTrans, float frameTime ) = 0;
	virtual void ApplyFinalMovement( IEntity * pEnt, const SAnimatedCharacterParams& params, pe_action_move& move, const Quat& rot, const Quat& desiredRot, bool useImpulse, float frameTime ) = 0;
	virtual void SetAnimationTM( const Matrix34& mat ) = 0;
	virtual bool AdjustPhysicsSync( float& speed, Vec3& moveDir, IEntity * pEnt, const SAnimatedCharacterParams& params ) = 0;
	virtual void GetMemoryStatistics(ICrySizer * s) = 0;
};
*/

//--------------------------------------------------------------------------------

//struct IVec	{Vec3 ntoe;Vec3 toe;Vec3 nheel;Vec3 heel;};
struct IVec	{ Vec3 normal; Vec3 pos; };

//--------------------------------------------------------------------------------

enum EDebugHistoryID
{
	eDH_Undefined,

	eDH_FrameTime,

	eDH_TurnSpeed,
	eDH_TravelSpeed,
	eDH_TravelDist,
	eDH_TravelDistScale,
	eDH_TravelDirX,
	eDH_TravelDirY,

	eDH_StateSelection_State,
	eDH_StateSelection_StartTravelSpeed,
	eDH_StateSelection_EndTravelSpeed,
	eDH_StateSelection_TravelDistance,
	eDH_StateSelection_StartTravelAngle,
	eDH_StateSelection_EndTravelAngle,
	eDH_StateSelection_EndBodyAngle,

	eDH_MovementControlMethodH,
	eDH_MovementControlMethodV,

	eDH_DesiredLocalLocationTX, 
	eDH_DesiredLocalLocationTY, 
	eDH_DesiredLocalLocationRZ,

	eDH_DesiredLocalVelocityTX, 
	eDH_DesiredLocalVelocityTY, 
	eDH_DesiredLocalVelocityRZ,

	eDH_PredictionTime,
	eDH_Immediateness,

	eDH_EntMovementErrorTransX,
	eDH_EntMovementErrorTransY,
	eDH_EntMovementErrorRotZ,
	eDH_EntTeleportMovementTransX,
	eDH_EntTeleportMovementTransY,
	eDH_EntTeleportMovementRotZ,
	eDH_ProceduralAnimMovementTransX,
	eDH_ProceduralAnimMovementTransY,
	eDH_ProceduralAnimMovementRotZ,
	eDH_AnimTargetCorrectionTransX,
	eDH_AnimTargetCorrectionTransY,
	eDH_AnimTargetCorrectionRotZ,
	eDH_AnimAssetTransX,
	eDH_AnimAssetTransY,
	eDH_AnimAssetRotZ,

	eDH_AnimLocationPosX,
	eDH_AnimLocationPosY,
	eDH_AnimLocationOriZ,

	eDH_EntLocationPosX,
	eDH_EntLocationPosY,
	eDH_EntLocationOriZ,

	eDH_AnimErrorDistance,
	eDH_AnimErrorAngle,

	eDH_AnimEntityOffsetTransX,
	eDH_AnimEntityOffsetTransY,
	eDH_AnimEntityOffsetRotZ,

	eDH_ReqEntMovementTransX,
	eDH_ReqEntMovementTransY,
	eDH_ReqEntMovementRotZ,

	eDH_CarryCorrectionDistance,
	eDH_CarryCorrectionAngle,

	eDH_TEMP00,
	eDH_TEMP01,
	eDH_TEMP02,
	eDH_TEMP03,

	/*
	eDH_EntityMoveSpeed,
	eDH_EntityPhysSpeed,
	eDH_ACRequestSpeed,
	*/
};

//--------------------------------------------------------------------------------

enum EMCMComponent
{
	eMCMComponent_Horizontal = 0,
	eMCMComponent_Vertical,

	eMCMComponent_COUNT,
};

enum EMCMSlot
{
	eMCMSlot_AnimGraph = 0,
	eMCMSlot_AnimChar,
	eMCMSlot_Cur,
	eMCMSlot_Prev,
	eMCMSlot_Debug,

	eMCMSlot_COUNT,
};

//--------------------------------------------------------------------------------

enum EACInputIndex
{
	// !!!WARNING!!! When changing these, make sure to update g_szInputIDStr in AnimatedCharacterPPS.cpp!

	eACInputIndex_MoveSpeedLX = 0,
	eACInputIndex_MoveSpeedLY,
	eACInputIndex_MoveSpeedLH,
	eACInputIndex_MoveDirLH4,
	eACInputIndex_MoveSpeedLZ,
	eACInputIndex_MoveSpeedWH,
	eACInputIndex_MoveSpeedWV,
	eACInputIndex_MoveSpeed,
	eACInputIndex_TurnSpeedLZ,
	eACInputIndex_COUNT_SpeedSub,
	eACInputIndex_COUNT_SpeedTotal = eACInputIndex_COUNT_SpeedSub * 2,

	// !!!WARNING!!! When changing these, make sure to update g_szInputIDStr in AnimatedCharacterPPS.cpp!

	eACInputIndex_RequestedSpeedBase	= 0,
	eACInputIndex_ActualSpeedBase			= eACInputIndex_COUNT_SpeedSub,

	// !!!WARNING!!! When changing these, make sure to update g_szInputIDStr in AnimatedCharacterPPS.cpp!

	eACInputIndex_AnimPhase = eACInputIndex_COUNT_SpeedTotal + 0,
	eACInputIndex_Action,
	eACInputIndex_PseudoSpeed,
	eACInputIndex_Stance,
	eACInputIndex_COUNT_Extra = 4, // NOTE: Bump this if you add more misc entries here!!!

	// !!!WARNING!!! When changing these, make sure to update g_szInputIDStr in AnimatedCharacterPPS.cpp!

	eACInputIndex_COUNT = eACInputIndex_COUNT_SpeedTotal + eACInputIndex_COUNT_Extra,

	// !!!WARNING!!! When changing these, make sure to update g_szInputIDStr in AnimatedCharacterPPS.cpp!
};

extern const char* g_szInputIDStr[eACInputIndex_COUNT];

//--------------------------------------------------------------------------------

struct SLocomotionStateSelectionParameters
{
	f32 m_fDuration;
	f32 m_fStartTravelSpeed;
	f32 m_fEndTravelSpeed;
	f32 m_fTravelDistance;
	f32 m_fStartTravelAngle; // Relative to StartBodyAngle, which is zero by definition.
	f32 m_fEndTravelAngle; // Relative to StartBodyAngle, which is zero by definition.
	f32 m_fEndBodyAngle;  // Relative to StartBodyAngle, which is zero by definition.
	f32 m_fUrgency;
	f32 m_fImmediateness;
};

//--------------------------------------------------------------------------------

extern const char* g_szAnimationGraphLayerProperty[eAnimationGraphLayer_COUNT];

//--------------------------------------------------------------------------------

enum RequestedEntityMovementType
{
	RequestedEntMovementType_Undefined = -1,
	RequestedEntMovementType_Absolute = 1,
	RequestedEntMovementType_Impulse = 2, // Relative
};

//--------------------------------------------------------------------------------

// TODO: Shuffle variable members around to better align and pack things tightly and reduce padding.
class CAnimatedCharacter : public CGameObjectExtensionHelper<CAnimatedCharacter, IAnimatedCharacter>, IAnimationGraphStateListener
{
public:
	CAnimatedCharacter();
	~CAnimatedCharacter();

	// IAnimatedCharacter
	virtual bool Init( IGameObject * pGameObject );
	virtual void InitClient(int channelId ) {};
	virtual void PostInit( IGameObject * pGameObject );
	virtual void PostInitClient(int channelId) {};
	virtual void Release();
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) { return true; }
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update( SEntityUpdateContext& ctx, int );
	virtual void HandleEvent( const SGameObjectEvent& );
	virtual void ProcessEvent( SEntityEvent& );
	virtual void SetChannelId(uint16 id) {}
	virtual void SetAuthority(bool auth) {}
	virtual void PostUpdate(float frameTime) { assert(false); }
	virtual void PostRemoteSpawn() {};
	virtual void GetMemoryStatistics(ICrySizer * s);

	// This is used for shifting the body/legs back in 1P, to avoid clipping the camera view.
	// It's potentially also used for trooper banking effects.
	virtual void SetExtraAnimationOffset( const Matrix34& offset );
	virtual void SetExtraAnimationOffset( const QuatT& offset );
	

	virtual IAnimationGraphPtr GetAnimationGraph(int layer) { return &*m_pGraph[layer]; }
	//virtual IAnimationGraphState * GetAnimationGraphState(int layer) { return m_pAnimationState[layer]; }
	virtual IAnimationGraphState* GetAnimationGraphState() { return &m_animationGraphStates; }

	virtual void PushForcedState( const char * state );
	virtual void ClearForcedStates();
	virtual void ChangeGraph( const char * graph, int layer );
	virtual void AddMovement( const SCharacterMoveRequest& request );
	virtual void SetEntityRotation(const Quat &rot){}
	virtual const SAnimatedCharacterParams& GetParams() { return m_params; }
	virtual void SetParams( const SAnimatedCharacterParams& params );
	virtual int GetCurrentStance();
	virtual void RequestStance( int stanceID, const char * name );
	virtual bool InStanceTransition();

	virtual void SetFacialAlertnessLevel(int alertness) { m_facialAlertness = alertness; }
	virtual int GetFacialAlertnessLevel() { return m_facialAlertness; }

	virtual void ResetState();

	virtual bool IsAnimationControlledView() const;
	virtual float FilterView(SViewParams &viewParams) const;

	//virtual void MakePushable(bool enable);
	virtual uint32 MakeFace(const char*pExpressionName, bool usePreviewChannel=true, float lifeTime=1.f);

	virtual void AllowLookIk( bool allow );
	virtual bool IsLookIkAllowed() const { return m_allowLookIk; }

	virtual void TriggerRecoil(float duration, float distance);
	virtual void SetWeaponRaisedPose(EWeaponRaisedPose pose);

	virtual void AllowFootIKNoCollision(bool allow) { m_bAllowFootIKNoCollision = allow; }

	virtual void SetIdle2MoveBehaviour(short allow) { m_Idle2MovePriority = allow; }
	virtual bool IsInIdle2MoveState() { return m_isInIdle2Move; }
	// ~IAnimatedCharacter

	// IAnimationGraphStateListener
	virtual void SetOutput( const char * output, const char * value );
	virtual void QueryComplete( TAnimationGraphQueryID queryID, bool succeeded );
	virtual void DestroyedState(IAnimationGraphState*);
	// ~IAnimationGraphStateListener
	void PathFollowingCallback(ICharacterInstance* pInstance);

private:
	//void OnPhysicsPreStep( float frameTime );
	void ResetVars();
	void AnimationControlled(bool activate);

	bool IsAnimGraphUpdateNeeded();

private:

	// TODO: Find out how these are managed, and serialize accordingly.
	_smart_ptr<CAnimationGraph> m_pGraph[eAnimationGraphLayer_COUNT];
	CAnimationGraphState* m_pAnimationState[eAnimationGraphLayer_COUNT];

	// Serialized
	CAnimationGraphStates m_animationGraphStates;

	// Not serialized
	SAnimatedCharacterParams m_params; // TODO: Garbage collect unused members of this struct.

	// Not serialized
	QuatT m_extraAnimationOffset;

	// Not serialized
	int m_facialAlertness; // TODO: Make this an int8 instead. We never use more than a few unique values.

	// Serialized
	// TODO: Turn the stance enums into uint8 instead of int32.
	int m_currentStance;

	// Not serialized
	int m_requestedStance;
	const char* m_debugStanceName;
	TAnimationGraphQueryID m_stanceQuery;

	ANIMCHAR_SIZED_VAR(bool, m_allowLookIk, 1);

	// Not serialized
	int m_curFrameID; // Updated every frame in UpdateTime().
	int m_lastResetFrameId; // Initialized to the current frame id in ResetVars().
	int m_updateGrabbedInputFrameID; // This is updated in PostAnimationUpdate(), when checking grabbed AG input.
	int m_updateSkeletonSettingsFrameID; // This is updated in PostAnimationUpdate(), when setting some insignificant/low-frequency skeleton parameters.
	int	m_moveRequestFrameID; // NOTE: GetFrameID() returns an int and this is initialized to -1 in ResetVars().
	int m_lastSerializeReadFrameID; // 

	// TODO: Pack as bits with other bools.
	bool m_bSimpleMovementSetOnce;

	EWeaponRaisedPose m_curWeaponRaisedPose;

	// Not serialized
	SCharacterMoveRequest m_moveRequest;

public:

	virtual EColliderMode GetPhysicalColliderMode() { return m_colliderMode; }
	virtual void ForceRefreshPhysicalColliderMode();
	virtual void RequestPhysicalColliderMode(EColliderMode mode, EColliderModeLayer layer, const char* tag = NULL);
	virtual void SetMovementControlMethods(EMovementControlMethod horizontal, EMovementControlMethod vertical);
	virtual void SetLocationClampingOverride(float distance, float angle);

	// Used by fall and play, when transitioning from ragdoll to get up animation.
	virtual void ForceTeleportAnimationToEntity();

	ILINE const QuatT& GetAnimLocation() const { return m_animLocation; }
	ILINE const QuatT& GetPrevEntityLocation() const { return m_prevEntLocation; }

	void CalculateParamsForCurrentMotions();
	CStateIndex::StateID SelectLocomotionState(_smart_ptr<CAnimationGraph> pAnimGraph, CStateIndex::StateID curStateID, const CStateIndex::StateIDVec& stateIDs, CStateIndex::StateID defaultStateID);
	bool ValidateAnimGraphPathNode(_smart_ptr<CAnimationGraph> pAnimGraph, CStateIndex::StateID stateID);

private:

	// Not serialized
	// TODO: Pack these as bits instead.
	bool m_isPlayer;
	bool m_isClient;

	// Not serialized
	CTimeValue m_curFrameStartTime;
	CTimeValue m_devatedPositionTime;
	CTimeValue m_devatedOrientationTime;

	// Not serialized
	double m_curFrameTime;
	double m_prevFrameTime;

	// Not serialized (during anim target it's special)
	QuatT m_animLocation;
	QuatT m_prevAnimLocation;
	QuatT m_desiredAnimMovement;
	float m_animTargetTime;
	QuatT m_animTarget;

	// Not serialized
	QuatT m_entLocation; // Current entity location.
	QuatT m_prevEntLocation; // Previous entity location.
	QuatT m_expectedEntMovement; // We request this movement to physics, and store it to measure how accurately it actually got moved.
	QuatT m_actualEntMovement; // This is the actual measured difference in location between two frames.
	QuatT m_entTeleportMovement; // Offset between entity locations between and after set pos/rot (teleportation).
	QuatT m_parentEntLocation; // 

	// PROCEDURAL LEANING STUFF (wip)
	// Not serialized
	bool EnableProceduralLeaning();
	float GetProceduralLeaningScale();
	QuatT CalculateProceduralLeaning();
	#define NumAxxSamples 32
	Vec2 m_reqLocalEntAxx[NumAxxSamples];
	Vec2 m_reqEntVelo[NumAxxSamples];
	CTimeValue m_reqEntTime[NumAxxSamples];
	int m_reqLocalEntAxxNextIndex;
	Vec3 m_smoothedActualEntVelo;
	float m_smoothedAmountAxx;
	Vec3 m_avgLocalEntAxx;

	// Serialized
	// TODO: Rename these to Smooth blabla, since they lowpass over time.
	// TODO: Maybe these can be packed as two uint8 instead?
	float m_expectedEntMovementLengthPrev; // This is just storing the length of the previous m_expectedEntMovement value, used for prediction retraction calculations.
	float m_actualEntMovementLengthPrev; // This is just storing the length of the previous m_actualEntMovement value, used for prediction retraction calculations.

	// Not serialized
	// TODO: Pack these two as one uint8, 4 bits each.
	int8 m_requestedEntMoveDirLH4;
	int8 m_actualEntMoveDirLH4;

	// Serialized
	// TODO: Make sure EMovementControlMethod is not bigger than 8 bits.
	EMovementControlMethod m_movementControlMethod[eMCMComponent_COUNT][eMCMSlot_COUNT];
	float m_elapsedTimeMCM[eMCMComponent_COUNT];

	// Serialized
	// TODO: Make sure EColliderMode is not bigger than 8 bits.
	const char* m_colliderModeLayersTag[eColliderModeLayer_COUNT];
	EColliderMode m_colliderModeLayers[eColliderModeLayer_COUNT];
	EColliderMode m_colliderMode;

	// Not serialized
	IAnimationGraph::InputID m_inputID[eACInputIndex_COUNT];

	// Not serialized
	// TODO: Try to pack these into bits instead. ANIMCHAR_SIZED_VAR(bool, name, 1);
	bool m_simplifiedAGSpeedInputsRequested;
	bool m_simplifiedAGSpeedInputsActual;

	// Not serialized
	QuatT m_requestedEntityMovement;
	RequestedEntityMovementType m_requestedEntityMovementType;
	int m_requestedIJump; // TODO: Turn this into an int8 instead.
	bool m_bDisablePhysicalGravity; // TODO: Pack this into bits instead.
	
	// Not serialized
	bool m_simplifyMovement; // TODO: Pack this into bits instead.
	bool m_forceDisableSlidingContactEvents; // TODO: Pack this into bits instead.
	bool m_sleepAnimGraph; // TODO: Pack this into bits instead.
	float m_noMovementTimer; // TODO: This does not have to be very accurate. Try packing it as an uint8 or uint16.

	// Serialized
	float m_actualEntVelocity; // TODO: Look into how this is actually used. (at least it's converted from QuatT to float, rest was redundant)

	// Not serialized
	const SAnimationTarget* m_pAnimTarget;
	ICharacterInstance* m_pCharacter;
	ISkeletonAnim* m_pSkeletonAnim;
	ISkeletonPose* m_pSkeletonPose;

	// Not serialized
	// TODO: Pack these as bits instead.
	bool m_bGrabbedInViewSpace;
	bool m_noMovementOverrideExternal;
	bool m_bAnimationGraphStatePaused;

	void UpdateTime();

public:
	virtual EMovementControlMethod GetMCMH() const;
	virtual EMovementControlMethod GetMCMV() const;
private:
	void UpdateMCMs();
	void UpdateMCMComponent(EMCMComponent component);
	QuatT MergeMCM(const QuatT& ent, const QuatT& anim, const QuatT& decoupled, bool flat) const;
	void GetMaxAnimErrorMCM(int mcm, float& maxDistance, float& maxAngle) const;

	ILINE bool RecentQuickLoad() { return ((m_lastSerializeReadFrameID + 3) > m_curFrameID); }

	bool EvaluateSimpleMovementConditions();
	void UpdateSimpleMovementConditions();
	bool UpdateAnimGraphSleepTracking(float frameTime);

	void PrePhysicsUpdate();

	void PreAnimationUpdate();
	void SetAnimGraphSpeedInputs(const QuatT& movement, const QuatT& origin, float frameTime, int baseInputIndex = 0);

public:
	void PostAnimationUpdate();
	void PostProcessingUpdate();

	virtual void SetNoMovementOverride(bool external);
	bool NoMovementOverride() const;

	// Returns the angle (in degrees) of the ground below the character.
	// Zero is flat ground (along facing direction), positive when character facing uphill, negative when character facing downhill.
	float GetGroundSlopeAngle() const { return m_fGroundSlopeSmooth; }

private:
	// Not serialized
	f32 m_fStandUpTimer;
	f32 m_fJumpSmooth;
	f32 m_fJumpSmoothRate;

	f32 m_fGroundSlopeSmooth;
	f32 m_fGroundSlopeRate;

	f32 m_fRootHeightSmooth;
	f32 m_fRootHeightRate;

	Vec2	m_LLastHeel2D;
	IVec	m_LLastHeelIVec;
	IVec	m_LLastHeelIVecSmooth;
	IVec	m_LLastHeelIVecSmoothRate;
	bool	m_bAllowFootIKNoCollision;

	short m_Idle2MovePriority;
	bool	m_isInIdle2Move;

	Vec2	m_RLastHeel2D;
	IVec	m_RLastHeelIVec;
	IVec	m_RLastHeelIVecSmooth;
	IVec	m_RLastHeelIVecSmoothRate;

public:
	IVec CheckFootIntersection(f32 RootHeight, const Vec3& Final_Heel, IPhysicalWorld* pIPhysicsWorld);

private:
	bool HasSplitUpdate() const;
	bool HasAtomicUpdate() const;

	void ClampAnimLocation();
	QuatT CalculateAnimRenderLocation();

	void GetCurrentEntityLocation();
	void UpdateCurAnimLocation();

	void AcquireRequestedBehaviourMovement();

	QuatT CalculateWantedEntityMovement();
	QuatT CalculateAnimTargetMovement();

	void ClampLocations(QuatT& wantedEntLocation, QuatT& wantedAnimLocation);
	void ClampLocationsMCM(EMovementControlMethod mcm, QuatT& wantedEntLocation, QuatT& wantedAnimLocation, float elapsedMCMTime, EMCMComponent mcmcmp, bool debug);
	void ApplyAnimGraphClampingOverrides(float& distanceMax, float& angleMax);
	void ApplyMotionDependentClampingOverrides(float& distanceMax, float& angleMax);
	void ApplyPlayerProximityClampingOverrides(float& distanceMax, float& angleMax);

	void RefreshAnimTarget();

	ILINE bool RecentCollision() { return ((m_collisionFrameID + 5) > m_curFrameID); }
	Vec3 RemovePenetratingComponent(const Vec3& v, const Vec3& n, float approachDistanceMin, float approachDistanceMax);

	ILINE bool InCutscene()
	{
		if ((m_pSkeletonAnim != NULL) && (m_pSkeletonAnim->GetTrackViewStatus() != 0))
			return true;

		return false;
	}

	bool m_forcedRefreshColliderMode;

	// Not serialized
	// Move these to pack better with other small variables.
	uint8 m_prevAnimPhaseHash;
	float m_prevAnimEntOffsetHash; // TODO: This could maybe be turned into an int8 and packed with some other 8 bitters.
	float m_prevMoveVeloHash;
	uint8 m_prevMoveJump;

	// Not serialized
	float m_prevOffsetDistance;
	float m_prevOffsetAngle;

	// Serialized
	float m_overrideClampDistance;
	float m_overrideClampAngle;

	int m_collisionFrameID;
	int m_collisionNormalCount;
	Vec3 m_collisionNormal[4];

	IPhysicalEntity* m_pFeetColliderPE;

	const SPredictedCharacterStates* GetPredictedCharacterStates() const;
	void CalculateDesiredLocationAndVelocity(QuatT& desiredLocalLocation, QuatT& desiredLocalVelocity, float& desiredCatchupTime, float& immediateness, float frameTime, bool debug = false);

	void CalculateAndRequestPhysicalEntityMovement();
	void RequestPhysicalEntityMovement(const QuatT& wantedEntLocation, const QuatT& wantedEntMovement);
	void UpdatePhysicalColliderMode();
	void UpdatePhysicsInertia();

	void CreateExtraSolidCollider();
	void DestroyExtraSolidCollider();

	float CanAnimCatchUp(const SAnimationSelectionProperties& props, const SLocomotionStateSelectionParameters& params, bool current);

	SLocomotionStateSelectionParameters CalculateStateSelectionParams();

private:

	// Consistency Tests
	void RunTests();

	// Debug rendering (in world) and text
	bool DebugFilter() const;
	bool DebugTextEnabled() const;
	void DebugRenderFutureAnimPath(ISkeletonAnim* pSkeletonAnim, float duration);
	void DebugRenderCurLocations() const;
	void DebugAnimEntDeviation(const QuatT& offset, float distance, float angle, float distanceMax, float angleMax);
	void DebugDisplayNewLocationsAndMovements(const QuatT& entLocation, const QuatT& entMovement, 
																					 const QuatT& animLocation, const QuatT& animMovement, 
																					 float frameTime) const;
	void DebugRenderDesiredLocationAndVelocity(const QuatT& desiredLocalLocation, const QuatT& desiredLocalVelocity) const;

	void LayoutHelper(const char* id, const char* name, bool visible, float minout, float maxout, float minin, float maxin, float x, float y, float w=1.0f, float h=1.0f);

	// TODO: #ifdef _DEBUG this struct, since it's member functions take up code memory?
	// AnimError Clamping Debugging/Analysis
	struct SAnimErrorClampStats
	{
		float m_elapsedTime;
		float m_distanceMin;
		float m_distanceMax;
		float m_distanceAvg;
		float m_distanceClampDuration;
		float m_angleMin;
		float m_angleMax;
		float m_angleAvg;
		float m_angleClampDuration;

		static const uint8 ms_histogramSize = 10;
		float m_distanceHistogramTime[ms_histogramSize];
		float m_distanceHistogramMin[ms_histogramSize];

		SAnimErrorClampStats()
		{
			m_elapsedTime = 0.0f;
			m_distanceMin = 1000.0f;
			m_distanceMax = -1000.0f;
			m_distanceAvg = 0.0f;
			m_distanceClampDuration = 0.0f;
			m_angleMin = 1000.0f;
			m_angleMax = -1000.0f;
			m_angleAvg = 0.0f;
			m_angleClampDuration = 0.0f;

			static float animErrorClampHistogramMax = 2.0f;
			for (int i = 0; i < ms_histogramSize; i++)
			{
				m_distanceHistogramMin[i] = animErrorClampHistogramMax * (float)i / (float)(ms_histogramSize - 1);
				m_distanceHistogramTime[i] = 0.0f;
			}
		}

		void Update(float frameTime, float distance, float angle, float distanceMax, float angleMax);
	};

	// Not serialized
	// TODO: Look at how this is managed.
	SAnimErrorClampStats* m_pAnimErrorClampStats;

	// Not serialized
	// TODO: Look at how this is managed.
	// DebugHistory Graphs
	void SetupDebugHistories();
	IDebugHistoryManager*	m_debugHistoryManager;

	void DebugGraphMotionParams(const ISkeletonAnim* pSkeletonAnim);
	void DebugBlendWeights(const ISkeletonAnim* pSkeletonAnim);

	void DebugHistory_AddValue(const char* id, float x);
	void DebugGraphQT(const QuatT& m, const char* tx, const char* ty, const char* rz, const char* tz = 0);

public:
	// This is a per frame cache of the desired/predicted path, up to one second in the future.
	struct SDesiredParams
	{
		static const int LAST_PARAM = 4;

		QuatT	location[LAST_PARAM+1];
		QuatT	velocity[LAST_PARAM+1];
		float immediateness[LAST_PARAM+1];
		float time[LAST_PARAM+1];

		void LookupDesiredLocationAndVelocity(float& time, QuatT& desiredLocalLocation, QuatT& desiredLocalVelocity, float& immediateness);

		SDesiredParams()
		{
			reset();
		}

		void reset()
		{
			float timeStep = 1.0f / float(SDesiredParams::LAST_PARAM);
			for (uint32 i=0; i<(LAST_PARAM+1); i++)
			{
				location[i].SetIdentity();
				velocity[i].SetIdentity();
				immediateness[i] = 1.0f;
				time[i] = float(i) * timeStep;
			}
		}

	};

	// Temporary per frame struct
	// NOTE: All characters share this, so it's only valid during the PrePhysicsUpdate of each character.
	static SDesiredParams	s_desiredParams;
};

//--------------------------------------------------------------------------------

#define UNIQUE(s)  (s + string().Format("%08X", this)).c_str()

//--------------------------------------------------------------------------------

extern float ApplyAntiOscilationFilter(float value, float filtersize);
extern void SmoothCDQuat(Quat& current, Quat& delta, const Quat& target, float frameTime, float smoothTime);
extern void ScaleQuatAngles(Quat& q, const Ang3& a);
extern float GetQuatAbsAngle(const Quat& q);
extern f32 GetYaw( const Vec3& v0, const Vec3& v1);
extern f32 GetYaw( const Vec2& v0, const Vec2& v1);
extern QuatT ApplyWorldOffset(const QuatT& origin, const QuatT& offset);
extern QuatT GetWorldOffset(const QuatT& origin, const QuatT& destination);
extern QuatT GetClampedOffset(const QuatT& offset, float maxDistance, float maxAngle, float& distance, float& angle);
extern QuatT ExtractHComponent(const QuatT& m);
extern QuatT ExtractVComponent(const QuatT& m);
extern QuatT CombineHVComponents2D(const QuatT& cmpH, const QuatT& cmpV);
extern QuatT CombineHVComponents3D(const QuatT& cmpH, const QuatT& cmpV);
extern void DebugRenderAngleMeasure(CPersistantDebug* pPD, const Vec3& origin, const Quat& orientation, const Quat& offset, float fraction);
extern void DebugRenderDistanceMeasure(CPersistantDebug* pPD, const Vec3& origin, const Vec3& offset, float fraction);
extern QuatT GetDebugEntityLocation(const char* name, const QuatT& _default);

//--------------------------------------------------------------------------------

#undef ANIMCHAR_SIZED_VAR

#endif
