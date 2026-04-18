// Puppet.cpp: implementation of the CPuppet class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Puppet.h"
#include "AILog.h"
#include "GoalOp.h"
#include "Graph.h"
#include "AIPlayer.h"
#include "Leader.h"
#include "CAISystem.h"
#include "AICollision.h"
#include "FlightNavRegion.h"
#include "VertexList.h"
#include "SquadMember.h"
#include "ObjectTracker.h"
#include "SmartObjects.h"
#include "TickCounter.h"
#include "PathFollower.h"
#include "AIVehicle.h"
#include "PNoise3.h"

#include "IConsole.h"
#include "IPhysics.h"
#include "ISystem.h"
#include "ILog.h"
#include "ITimer.h"
#include "I3DEngine.h"
#include "ISerialize.h"
#include "IRenderer.h"

#include "PersonalInterestManager.h"
#include <algorithm>
#include <list>

const float CPuppet::COMBATCLASS_IGNORE_TRESHOLD = 0.0001f;


static const float PERCEPTION_INTERESTING_THR = 0.3f;
static const float PERCEPTION_THREATENING_THR = 0.6f;
static const float PERCEPTION_AGGRESSIVE_THR = 0.9f;


inline float SmoothStep(float a, float b, float x)
{
	x = (x - a) / (b - a);
	if (x < 0.0f) x = 0;
	if (x > 1.0f) x = 1;
	return x*x * (3 - 2*x);
}

struct SSortedHideSpot
{
	SSortedHideSpot(float weight, SHideSpot* pHideSpot) : weight(weight), pHideSpot(pHideSpot) {}
	inline bool operator<(const SSortedHideSpot& rhs) const { return weight > rhs.weight; }	// highest weight first.
	float weight;
	SHideSpot* pHideSpot;
};


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPuppet::CPuppet(CAISystem *pAISystem)
: CPipeUser(pAISystem)
, m_fSuppressFiring( 0 )
, m_fAccuracyMotionAddition( 0.f )
, m_fAccuracySupressor( 0.f )
, m_fLastUpdateTime( 0.0f )
, m_fCurrentAwarenessLevel( PERCEPTION_SOMETHING_SEEN_VALUE/ 2.f )
, m_bWarningTargetDistance(false)
, m_SpeedControlFlipCounter(0)
, m_pTrackPattern(0)
, m_Alertness(0)
, m_pFireCmdHandler(0)
, m_pFireCmdGrenade(0)
, m_allowedStrafeDistanceStart(0)
, m_allowedStrafeDistanceEnd(0)
, m_allowStrafeLookWhileMoving(false)
, m_strafeStartDistance(0.0f)
, m_adaptiveUrgencyMin(0)
, m_adaptiveUrgencyMax(0)
, m_adaptiveUrgencyScaleDownPathLen(0)
, m_adaptiveUrgencyMaxPathLen(0)
, m_fTargetAwareness(0)
, m_friendOnWayCounter(0)
, m_fLastTimeAwareOfPlayer(0)
, m_fLastStuntTime(0)
, m_playerAwarenessType(PA_NONE)
, m_bCloseContact(false)
, m_timeSinceTriggerPressed(0)
, m_friendOnWayElapsedTime(0)
, m_territoryShape(0)
, m_allowedToHitTarget(false)
, m_allowedToUseExpensiveAccessory(false)
, m_firingReactionTimePassed(false)
, m_firingReactionTime(0.0f)
, m_targetApproach(0.0f)
, m_targetFlee(0.0f)
, m_targetApproaching(false)
, m_targetFleeing(false)
, m_lastTargetValid(false)
, m_lastTargetPos(0,0,0)
, m_lastTargetSpeed(0)
, m_bDryUpdate(false)
, m_FOVPrimaryCos(0)
, m_FOVSecondaryCos(0)
, m_LastMissPoint(0,0,0)
, m_chaseSpeed(0.0f)
, m_chaseSpeedRate(0.0f)
, m_lastChaseUrgencyDist(-1)
, m_lastChaseUrgencySpeed(-1)
, m_ControlledSpeed(0.0f)
, m_pAvoidedVehicle(0)
, m_vehicleAvoidingTime(0.0f)
, m_targetLastMissPoint(0,0,0)
, m_targetFocus(0.0f)
, m_targetZone(ZONE_OUT)
, m_targetPosOnSilhouettePlane(0,0,0)
, m_targetDistanceToSilhouette(FLT_MAX)
, m_targetBiasDirection(0,0,-1)
, m_targetEscapeLastMiss(0.0f)
, m_targetDamageHealthThr(0.0f)
, m_targetDamageHealthThrHistory(0)
, m_targetSeenTime(0)
, m_targetLostTime(0)
, m_targetLastMissPointSelectionTime(0.0f)
, m_burstEffectTime(0.0f)
, m_burstEffectState(0)
, m_targetDazzlingTime(0.0f)
, m_bCoverFireEnabled(false)
, m_lastSteerTime(0.0f)
, m_SeeTargetFrom(ST_HEAD)
, m_delayedStance(STANCE_NULL)
, m_delayedStanceMovementCounter(0)
, m_outOfAmmoTimeOut(0.0f)
, m_lastPuppetVisCheck(0)
, m_weaponSpinupTime(0)
, m_lastAimObstructionResult(true)
, m_updatePriority(AIPUP_LOW)
, m_vForcedNavigation(ZERO)
, m_adjustpath(0)
, m_LastShotsCount(0)
, m_steeringOccupancyBias(0)
, m_steeringAdjustTime(0)
, m_steeringEnabled(false)
, m_alarmedTime(0.0f)
, m_alarmedLevel(0.0f)
, m_closeRangeStrafing(false)
, m_lastBodyDir(ZERO)
, m_bodyTurningSpeed(0)
{
	_fastcast_CPuppet = true;

	static float kD(.00001f);
	static float kI(.0f);
	static float kP(1.2f);

	m_SpeedPID.m_kD = kD;
	m_SpeedPID.m_kI = kI;
	m_SpeedPID.m_kP = kP;


	m_CloseContactTime = 	GetAISystem()->GetFrameStartTime();

	ResetMiss();

	// todo: make it safe, not just casting
	m_pFireCmdGrenade = static_cast<CFireCommandGrenade*>(GetAISystem()->CreateFirecommandHandler("grenade", this));
	if(m_pFireCmdGrenade)
		m_pFireCmdGrenade->Reset();

	SetAvoidedVehicle(NULL);
}

CPuppet::~CPuppet()
{
#ifndef USER_dejan
  AILogComment("CPuppet::~CPuppet %s (%p)", GetName(), this);
#endif
	ClearPotentialTargets();

	if(m_pFireCmdGrenade) m_pFireCmdGrenade->Release();

	if(m_pFireCmdHandler) m_pFireCmdHandler->Release();
	delete m_pTrackPattern;

	m_territoryShape = 0;
	m_territoryShapeName.clear();

	if (m_pCurrentGoalPipe)
		ResetCurrentPipe( true );

	delete m_targetDamageHealthThrHistory;
}

//===================================================================
// ParseParameters
//===================================================================
void CPuppet::ParseParameters(const AIObjectParameters &params)
{
	IPuppetProxy *pProxy;
	if (params.pProxy->QueryProxy(AIPROXY_PUPPET, (void **) &pProxy))
		SetProxy(pProxy);

	CAIActor::ParseParameters( params );

	// do custom parse on the parameters
	m_Parameters  = params.m_sParamStruct;

	TransformFOV();

	if (m_Parameters.m_nSpecies>=0)
		GetAISystem()->AddToSpecies(this,m_Parameters.m_nSpecies);

	if (m_Parameters.m_fAccuracy <0)
		m_Parameters.m_fAccuracy = 0;
	if (m_Parameters.m_fAccuracy >1)
		m_Parameters.m_fAccuracy = 1;

	GetAISystem()->NotifyEnableState(this, m_bEnabled);
}

//===================================================================
// SetWeaponDescriptor
//===================================================================
void CPuppet::SetWeaponDescriptor(const AIWeaponDescriptor& descriptor)
{
	m_CurrentWeaponDescriptor = descriptor;
//	if(m_CurrentWeaponDescriptor.bSignalOnShoot)
	GetProxy()->EnableWeaponListener(m_CurrentWeaponDescriptor.bSignalOnShoot);

	// Make sure the fire command handler is up to date.

	// If the current fire command handler is already correct, do not recreate it.
	if(m_pFireCmdHandler && stricmp(descriptor.firecmdHandler.c_str(), m_pFireCmdHandler->GetName()) == 0)
	{
		m_pFireCmdHandler->Reset();
		return;
	}

	// Release the old handler and create new.
	SAFE_RELEASE(m_pFireCmdHandler);
	m_pFireCmdHandler = GetAISystem()->CreateFirecommandHandler(descriptor.firecmdHandler, this);
	if(m_pFireCmdHandler)
		m_pFireCmdHandler->Reset();
}

//===================================================================
// GetSightRange
//===================================================================
float CPuppet::GetSightRange(const CAIObject* pTarget) const
{
	if(!pTarget)
		return CAIActor::GetSightRange(pTarget);	//GetParameters().m_PerceptionParams.sightRange;

	float scale = 1.0f;
	if (IsAffectedByLightLevel())
	{
		const CAIActor* pTargetActor = pTarget->CastToCAIActor();

		EAILightLevel targetLightLevel;
		if (pTargetActor)
			targetLightLevel = pTargetActor->GetLightLevel();
		else
			targetLightLevel = GetAISystem()->GetLightManager()->GetLightLevelAt(pTarget->GetPos());

		switch (targetLightLevel)
		{
		//	case AILL_LIGHT: SOMSpeed
		case AILL_MEDIUM: scale = GetAISystem()->m_cvSightRangeMediumIllumMod->GetFVal(); break;
		case AILL_DARK:	scale = GetAISystem()->m_cvSightRangeDarkIllumMod->GetFVal(); break;
		}
	}

	float sightRange = CAIActor::GetSightRange(pTarget);

	// scale down sight range when target is underwater
	const Vec3& targetPos = pTarget->GetPos();
	float waterOcclusion = GetAISystem()->GetWaterOcclusionValue(targetPos);
	if(waterOcclusion > 0)
	{
		float distance = Distance::Point_Point(GetPos(), targetPos);
		float fDistanceFactor = 0.0f;
		if(sightRange != 0.0f)
			fDistanceFactor = GetAISystem()->GetVisPerceptionDistScale(distance/sightRange);
		waterOcclusion = 2*waterOcclusion + (1-fDistanceFactor)/2; // water occlusion also depends on distance to target
		if(waterOcclusion>1)
			waterOcclusion = 1;
	}

	scale *= (1-waterOcclusion);
	return sightRange * scale;
}

//===================================================================
// GetTargetAliveTime
//===================================================================
float CPuppet::GetTargetAliveTime(float* outMoveMod, float* outStanceMod, float* outDirMod, float* outHitMod, float* outZoneMod)
{
	float targetStayAliveTime = GetAISystem()->m_cvRODAliveTime->GetFVal();

	const CAIObject* pLiveTarget = GetLiveTarget(m_pAttentionTarget);
	if (!pLiveTarget)
		return targetStayAliveTime;

	if (pLiveTarget)
	{
		Vec3 dirToTarget = pLiveTarget->GetPos() - GetPos();
		float distToTarget = dirToTarget.NormalizeSafe();

		// Scale target life time based on target speed.
		const float moveInc = GetAISystem()->m_cvRODMoveInc->GetFVal();
//		if (moveMod > 0.0001f)
		{
			float inc = 0.0f;

			const Vec3& targetVel = pLiveTarget->GetVelocity();
			float speed = targetVel.GetLength();
			if (pLiveTarget->GetType() == AIOBJECT_PLAYER)
			{
				// Super speed run or super jump.
				if (speed > 12.0f || targetVel.z > 7.0f)
				{
					inc += moveInc*2.0f;
					// Dazzle the shooter for a moment.
					m_targetDazzlingTime = 2.0f;
				}
				else if (speed > 6.0f)
				{
					inc += moveInc;
				}
			}
			else
			{
				if (speed > 6.0f)
					inc *= moveInc;
			}

			targetStayAliveTime += inc;

			if (outMoveMod)
				*outMoveMod = inc;
		}

		// Scale target life time based on target stance.
		const float stanceInc = GetAISystem()->m_cvRODStanceInc->GetFVal();
//		if (stanceMod > 0.0001f)
		{
			float inc = 0.0f;

			SAIBodyInfo bi;
			AIAssert(pLiveTarget->GetProxy());
			pLiveTarget->GetProxy()->QueryBodyInfo(bi);
			if (bi.stance == STANCE_CROUCH && m_targetZone > ZONE_KILL)
				inc += stanceInc;
			else if (bi.stance == STANCE_PRONE && m_targetZone >= ZONE_COMBAT_FAR)
				inc += stanceInc * stanceInc;
			
			targetStayAliveTime += inc;
			if (outStanceMod)
				*outStanceMod = inc;
		}

		// Scale target life time based on target vs. shooter orientation.
		const float dirInc = GetAISystem()->m_cvRODDirInc->GetFVal();
//		if (dirMod > 0.0001f)
		{
			float inc = 0.0f;

			const float thr1 = cosf(DEG2RAD(30.0f));
			const float thr2 = cosf(DEG2RAD(95.0f));
			float dot = -dirToTarget.Dot(pLiveTarget->GetViewDir());
			if (dot < thr2)
				inc += dirInc*2.0f;
			else if (dot < thr1)
				inc += dirInc;

			targetStayAliveTime += inc;
			if (outDirMod)
				*outDirMod = inc;
		}
	}

	if (!m_allowedToHitTarget)
	{
		const float ambientFireInc = GetAISystem()->m_cvRODAmbientFireInc->GetFVal();
//		if (ambientFireMod > 0.001f)
		{
			// If the agent is set not to be allowed to hit the target, let the others shoot first.
			targetStayAliveTime += ambientFireInc;
			if(outHitMod)
				*outHitMod = ambientFireInc;
		}
	}
	else
	{
		const float killZoneInc = GetAISystem()->m_cvRODKillZoneInc->GetFVal();
//		if (killZoneMod > 0.001f)
		{
			float inc = 0.0f;

			// Kill much faster when the target is in kill-zone.
			SAIBodyInfo bi;
			GetProxy()->QueryBodyInfo(bi);
			if (!bi.linkedVehicleEntity)
			{
				if (m_targetZone == ZONE_KILL)
					inc += killZoneInc;
			}

			targetStayAliveTime += inc;
			if (outZoneMod)
				*outZoneMod = inc;
		}
	}

	return max(0.0f, targetStayAliveTime);
}

//===================================================================
// UpdateHealthTracking
//===================================================================
void CPuppet::UpdateHealthTracking()
{
  FUNCTION_PROFILER( GetISystem(),PROFILE_AI );

	// Update target health tracking.
	float targetHealth = 0.0f;
	float targetMaxHealth = 1.0f;

	// Convert the current health to normalized range (1.0 == normal max health).
	const CAIObject* pLiveTarget = GetLiveTarget(m_pAttentionTarget);
	if (pLiveTarget)
	{
		if (pLiveTarget->GetProxy())
		{
			float health = pLiveTarget->GetProxy()->GetActorHealth() + pLiveTarget->GetProxy()->GetActorArmor();
			float maxHealth = pLiveTarget->GetProxy()->GetActorMaxHealth() + pLiveTarget->GetProxy()->GetActorMaxArmor();

			targetMaxHealth = maxHealth / (float)pLiveTarget->GetProxy()->GetActorMaxHealth();
			targetHealth = health / (float)pLiveTarget->GetProxy()->GetActorMaxHealth();
		}
	}

	const float dt = GetAISystem()->GetFrameDeltaTime();

	// Calculate the rate of death.
	float targetStayAliveTime = GetTargetAliveTime();

	// This catches the case when the target turns on and off the armor.
	if (m_targetDamageHealthThr > targetMaxHealth)
		m_targetDamageHealthThr = targetMaxHealth;

	if (targetHealth >= m_targetDamageHealthThr || targetStayAliveTime == 0.f)
		m_targetDamageHealthThr = targetHealth;
	else
		m_targetDamageHealthThr = targetStayAliveTime>0.f ? 
											max(targetHealth, m_targetDamageHealthThr - (1.0f / targetStayAliveTime) * m_Parameters.m_fAccuracy * dt)
											:	targetHealth;

	if (GetAISystem()->m_cvDebugDrawDamageControl->GetIVal() > 0)
	{
		if (!m_targetDamageHealthThrHistory)
			m_targetDamageHealthThrHistory = new SAIValueHistory(100, 0.1f);
		m_targetDamageHealthThrHistory->Sample(m_targetDamageHealthThr, GetAISystem()->GetFrameDeltaTime());

		UpdateHealthHistory();
	}
}

//===================================================================
// Update
//===================================================================
void CPuppet::Update(EObjectUpdate type)
{
	FUNCTION_PROFILER(GetAISystem()->m_pSystem, PROFILE_AI);

	// 
	if (!IsEnabled())
	{
		AIWarning("CPuppet::Update trying to update disabled  %s", GetName());
		AIAssert(0);
		return;
	}
	// There should never be puppets without proxies.
	if (!GetProxy())
	{
		AIWarning("CPuppet::Update Puppet %s does not have proxy!", GetName());
		AIAssert(0);
		return;
	}
	// There should never be puppets without physics.
	if (!GetPhysics())
	{
		AIWarning("CPuppet::Update Puppet %s does not have physics!", GetName());
		AIAssert(0);
		return;
	}
	// dead puppets should never be updated
	if (m_bEnabled && GetProxy()->GetActorHealth() < 1)
	{
		AIWarning("CPuppet::Update Trying to update dead character >> %s ", GetName());
//		AIAssert(0);
		return;
	}

	const float dt = GetAISystem()->GetFrameDeltaTime();

	// Update body angle and body turn speed
	float turnAngle = Ang3::CreateRadZ(m_lastBodyDir, GetBodyDir());
	if (dt > 0.0f)
		m_bodyTurningSpeed = turnAngle / dt;
	else
		m_bodyTurningSpeed = 0;
	m_lastBodyDir = GetBodyDir();


	m_bDryUpdate = type == AIUPDATE_DRY;

	if(!m_bDryUpdate)
	{
		m_CurrentHideObject.Update(this);

		// Update some special objects if they have been recently used.
		CAIObject* pSpecial = 0;

		Vec3 probTarget(0,0,0);

		pSpecial = m_pSpecialObjects[AISPECIAL_PROBTARGET_IN_TERRITORY];
		if (pSpecial && pSpecial->m_bTouched)
		{
			pSpecial->m_bTouched = false;
			if (probTarget.IsZero())
				probTarget = GetProbableTargetPosition();
			Vec3 pos = probTarget;
			// Clamp the point to the territory shape.
			if (GetTerritoryShape())
				GetTerritoryShape()->ConstrainPointInsideShape(pos, true);

			pSpecial->SetPos(pos);
		}

		pSpecial = m_pSpecialObjects[AISPECIAL_PROBTARGET_IN_REFSHAPE];
		if (pSpecial && pSpecial->m_bTouched)
		{
			pSpecial->m_bTouched = false;
			if (probTarget.IsZero())
				probTarget = GetProbableTargetPosition();
			Vec3 pos = probTarget;
			// Clamp the point to ref shape
			if (GetRefShape())
				GetRefShape()->ConstrainPointInsideShape(pos, true);

			pSpecial->SetPos(pos);
		}

		pSpecial = m_pSpecialObjects[AISPECIAL_PROBTARGET_IN_TERRITORY_AND_REFSHAPE];
		if (pSpecial && pSpecial->m_bTouched)
		{
			pSpecial->m_bTouched = false;
			if (probTarget.IsZero())
				probTarget = GetProbableTargetPosition();
			Vec3 pos = probTarget;
			// Clamp the point to ref shape
			if (GetRefShape())
				GetRefShape()->ConstrainPointInsideShape(pos, true);
			// Clamp the point to the territory shape.
			if (GetTerritoryShape())
				GetTerritoryShape()->ConstrainPointInsideShape(pos, true);

			pSpecial->SetPos(pos);
		}

		pSpecial = m_pSpecialObjects[AISPECIAL_ATTTARGET_IN_REFSHAPE];
		if (pSpecial && pSpecial->m_bTouched)
		{
			pSpecial->m_bTouched = false;
			if (GetAttentionTarget())
			{
				Vec3	pos = GetAttentionTarget()->GetPos();
				// Clamp the point to ref shape
				if (GetRefShape())
					GetRefShape()->ConstrainPointInsideShape(pos, true);
				// Update pos
				pSpecial->SetPos(pos);
			}
			else
			{
				pSpecial->SetPos(GetPos());
			}
		}

		pSpecial = m_pSpecialObjects[AISPECIAL_ATTTARGET_IN_TERRITORY];
		if (pSpecial && pSpecial->m_bTouched)
		{
			pSpecial->m_bTouched = false;
			if (GetAttentionTarget())
			{
				Vec3	pos = GetAttentionTarget()->GetPos();
				// Clamp the point to ref shape
				if (GetTerritoryShape())
					GetTerritoryShape()->ConstrainPointInsideShape(pos, true);
				// Update pos
				pSpecial->SetPos(pos);
			}
			else
			{
				pSpecial->SetPos(GetPos());
			}
		}

		pSpecial = m_pSpecialObjects[AISPECIAL_ATTTARGET_IN_TERRITORY_AND_REFSHAPE];
		if (pSpecial && pSpecial->m_bTouched)
		{
			pSpecial->m_bTouched = false;
			if (GetAttentionTarget())
			{
				Vec3	pos = GetAttentionTarget()->GetPos();
				// Clamp the point to ref shape
				if (GetRefShape())
					GetRefShape()->ConstrainPointInsideShape(pos, true);
				// Clamp the point to the territory shape.
				if (GetTerritoryShape())
					GetTerritoryShape()->ConstrainPointInsideShape(pos, true);

				pSpecial->SetPos(pos);
			}
			else
			{
				pSpecial->SetPos(GetPos());
			}
		}

	}

	// make sure to update direction when entity is not moved
	SAIBodyInfo bodyInfo;
	GetProxy()->QueryBodyInfo( bodyInfo);
	SetPos( bodyInfo.vEyePos );
  SetBodyDir( bodyInfo.vBodyDir );
  SetMoveDir( bodyInfo.vMoveDir );
	SetViewDir( bodyInfo.vEyeDir );

	UpdateHealthTracking();

	// Handle navigational smart object start and end states before executing goalpipes.
	if ( m_eNavSOMethod == nSOmSignalAnimation || m_eNavSOMethod == nSOmActionAnimation )
	{
		if(m_State.curActorTargetPhase == eATP_Started || m_State.curActorTargetPhase == eATP_StartedAndFinished)
		{
			// Copy the set state to the currently used state.
			m_currentNavSOStates = m_pendingNavSOStates;
			m_pendingNavSOStates.Clear();

			// keep track of last used smart object
			m_idLastUsedSmartObject = m_currentNavSOStates.objectEntId;

			// keep track of last used smart object
			m_idLastUsedSmartObject = m_currentNavSOStates.objectEntId;
		}

		if(m_State.curActorTargetPhase == eATP_StartedAndFinished || m_State.curActorTargetPhase == eATP_Finished)
		{
			// modify smart object states
			IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity( m_currentNavSOStates.objectEntId );
			GetAISystem()->ModifySmartObjectStates( GetEntity(), m_currentNavSOStates.sAnimationDoneUserStates );
			if ( pObjectEntity )
				GetAISystem()->ModifySmartObjectStates( pObjectEntity, m_currentNavSOStates.sAnimationDoneObjectStates );

			m_currentNavSOStates.Clear();
			m_forcePathGenerationFrom.zero();
			m_eNavSOMethod = nSOmNone;
			m_navSOEarlyPathRegen = false;
		}
	}

  static bool doDryUpdateCall = true;

	if (!m_bDryUpdate)
	{	
		CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
		if (m_fLastUpdateTime.GetSeconds()>0)
			m_fTimePassed = min(0.5f, (fCurrentTime - m_fLastUpdateTime).GetSeconds());
		else
			m_fTimePassed = 0;
		m_fLastUpdateTime = fCurrentTime;

		m_State.Reset();

		// Update target
		if( m_pTrackPattern )
			m_pTrackPattern->Update();
		
		// affect puppet parameters here----------------------------------------
		if (m_bCanReceiveSignals)
			UpdatePuppetInternalState();				

		// change state here----------------------------------------
		if (m_fSuppressFiring <= 0.0f)
			GetStateFromActiveGoals(m_State);

		// Store current position to debug stream.
		{
			RecorderEventData recorderEventData(GetPos());
			RecordEvent(IAIRecordable::E_AGENTPOS, &recorderEventData);
		}

		// Store current attention target position to debug stream.
		if(m_pAttentionTarget)
		{
			RecorderEventData recorderEventData(m_pAttentionTarget->GetPos());
			RecordEvent(IAIRecordable::E_ATTENTIONTARGETPOS, &recorderEventData);
		}

		m_friendOnWayCounter -= GetAISystem()->GetUpdateInterval();

		// Update last know target position, and last target position constrained into the territory.
		if(m_pAttentionTarget && m_pAttentionTarget->IsAgent())
		{
			m_lastLiveTargetPos = m_pAttentionTarget->GetPos();
			m_timeSinceLastLiveTarget = 0.0f;
		}
		else
		{
			if (m_timeSinceLastLiveTarget >= 0.0f)
				m_timeSinceLastLiveTarget += m_fTimePassed;
		}

		m_lightLevel = GetAISystem()->GetLightManager()->GetLightLevelAt(GetPos(), this, &m_usingCombatLight);
	}
  else if (doDryUpdateCall)
  {
    // if approaching then always update
    for (size_t i = 0; i < m_vActiveGoals.size(); i++)
    {
      QGoal& Goal = m_vActiveGoals[i];
      Goal.pGoalOp->ExecuteDry(this);
    }
  }

	if (m_delayedStance != STANCE_NULL)
	{
		bool wantsToMove = !m_State.vMoveDir.IsZero() && m_State.fDesiredSpeed > 0.0f;
		if (wantsToMove)
		{
			m_delayedStanceMovementCounter++;
			if (m_delayedStanceMovementCounter > 3)
			{
				m_State.bodystate = m_delayedStance;
				m_delayedStance = STANCE_NULL;
				m_delayedStanceMovementCounter = 0;
			}
		}
		else
		{
			m_delayedStanceMovementCounter = 0;
		}
	}

	// Reset the debug vectors related to the exact positioning on new request.
/*	if(m_State.actorTargetPhase == eATP_Request)
	{
		m_actorTargetFinishPos.zero();
		m_DEBUGCanTargetPointBeReached.clear();
		m_DEBUGUseTargetPointRequest.zero();
	}*/

	// Handle navigational smart object failure case. The start and finish states are handled
	// before executing the goalpipes, but since trace goalop can set the error too, the error is handled
	// here right after the goalops are executed. If this was done later, the state syncing code below would
	// clear the flag.
	if ( m_eNavSOMethod == nSOmSignalAnimation || m_eNavSOMethod == nSOmActionAnimation )
	{
		if ( m_State.curActorTargetPhase == eATP_Error )
		{
			// Invalidate the current link
			PathPointDescriptor::SmartObjectNavDataPtr pSmartObjectNavData = m_Path.GetLastPathPointAnimNavSOData();

			if ( pSmartObjectNavData )
			{
				const SSmartObjectNavData* pNavData = GetAISystem()->GetGraph()->GetNodeManager().GetNode(pSmartObjectNavData->fromIndex)->GetSmartObjectNavData();
				InvalidateSOLink( pNavData->pSmartObject, pNavData->pHelper, GetAISystem()->GetGraph()->GetNodeManager().GetNode(pSmartObjectNavData->toIndex)->GetSmartObjectNavData()->pHelper );
			}

			// modify smart object states
			if ( !m_currentNavSOStates.IsEmpty() )
			{
				GetAISystem()->ModifySmartObjectStates( GetEntity(), m_currentNavSOStates.sAnimationFailUserStates );
				IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity( m_currentNavSOStates.objectEntId );
				if ( pObjectEntity )
					GetAISystem()->ModifySmartObjectStates( pObjectEntity, m_currentNavSOStates.sAnimationFailObjectStates );
				m_currentNavSOStates.Clear();
			}

			if ( !m_pendingNavSOStates.IsEmpty() )
			{
				GetAISystem()->ModifySmartObjectStates( GetEntity(), m_pendingNavSOStates.sAnimationFailUserStates );
				IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity( m_pendingNavSOStates.objectEntId );
				if ( pObjectEntity )
					GetAISystem()->ModifySmartObjectStates( pObjectEntity, m_pendingNavSOStates.sAnimationFailObjectStates );
				m_pendingNavSOStates.Clear();
			}

			m_eNavSOMethod = nSOmNone;
			m_navSOEarlyPathRegen = false;
			m_forcePathGenerationFrom.zero();
		}
	}

	//
	// Sync the actor target phase with the AIProxy
	// The syncing should only happen here. The Trace goalop is allowed to
	// make a request and set error state if necessary.
	//

	bool	animationStarted = false;

	if(m_State.curActorTargetPhase == eATP_Waiting || m_State.curActorTargetPhase == eATP_Starting)
	{
	}
	else if(m_State.curActorTargetPhase == eATP_Started)
	{
//		m_State.actorTargetPhase = eATP_Playing;
		animationStarted = true;
	}
	else if(m_State.curActorTargetPhase == eATP_StartedAndFinished)
	{
		animationStarted = true;
//		m_State.actorTargetPhase = eATP_None;
		m_State.actorTargetReq.Reset();
	}
	else if(m_State.curActorTargetPhase == eATP_Finished)
	{
		m_State.curActorTargetPhase = eATP_None;
		m_State.actorTargetReq.Reset();
	}
	else if(m_State.curActorTargetPhase == eATP_Error)
	{
		// m_State.actorTargetReq shouldn't get reset here but only when path
		// is cleared after COPTrace has processed the error, otherwise
		// COPTrace may finish with AIGOR_SUCCEED instead of AIGOR_FAILED.
	}

	// Inform the listeners of current goal pipe that an animation has been started.
	if(animationStarted && m_eNavSOMethod == nSOmNone)
	{
		CGoalPipe* pCurrent = m_pCurrentGoalPipe;
		if ( pCurrent )
		{
			while ( pCurrent->GetSubpipe() )
				pCurrent = pCurrent->GetSubpipe();
			NotifyListeners( pCurrent, ePN_AnimStarted );
		}
	}


	//--------------------------------------------------------
	// Update the look at and strafe logic.
	if ( GetSubType() == CAIObject::STP_2D_FLY )
		UpdateLookTarget3D(m_pAttentionTarget); //this is only for the scout now.
	else
		UpdateLookTarget(m_pAttentionTarget);

	m_vDEBUG_VECTOR_movement = m_State.vMoveDir;

	if (m_pAttentionTarget)
	{
		m_State.nTargetType = m_pAttentionTarget->GetType();
		m_State.bTargetEnabled = m_pAttentionTarget->IsEnabled();
	}
	else
	{
		m_State.nTargetType = -1;
		m_State.bTargetEnabled = false;
		m_State.eTargetThreat = AITHREAT_NONE;
		m_State.eTargetType = AITARGET_NONE;
	}

	//--------------------------------------------------------
	// manipulate the fire flag
	if (m_pAttentionTarget && AllowedToFire() && m_State.nTargetType == AIOBJECT_PLAYER && GetAISystem()->m_bCollectingAllowed)
		m_fEngageTime += dt;

	m_targetDazzlingTime = max(0.0f, m_targetDazzlingTime - dt);
	if (GetAttentionTargetType() == AITARGET_VISUAL && GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
	{
		m_targetLostTime = 0.0f;
		m_targetSeenTime = min(m_targetSeenTime + dt, 10.0f);
	}
	else
	{
		m_targetLostTime += dt;
		m_targetSeenTime = max(0.0f, m_targetSeenTime - dt);
	}


	FireCommand();

	if (GetAISystem()->m_cvUpdateProxy->GetIVal())
	{
		FRAME_PROFILER("AIProxyUpdate",GetAISystem()->m_pSystem,PROFILE_AI);
		
		// Force stance etc
		int lastStance = m_State.bodystate;
		if(GetAISystem()->m_cvForceStance->GetIVal() > -1)
			m_State.bodystate = GetAISystem()->m_cvForceStance->GetIVal();
    if (GetAISystem()->m_cvForceAllowStrafing->GetIVal() > -1)
      m_State.allowStrafing = GetAISystem()->m_cvForceAllowStrafing->GetIVal() != 0;
    const char * forceLookAimTarget = GetAISystem()->m_cvForceLookAimTarget->GetString();
    if (strcmp(forceLookAimTarget, "none") != 0)
    {
      Vec3 targetPos = GetPos();
      if (strcmp(forceLookAimTarget, "x") == 0)
        targetPos += Vec3(10, 0, 0);
      else if (strcmp(forceLookAimTarget, "y") == 0)
        targetPos += Vec3(0, 10, 0);
      else if (strcmp(forceLookAimTarget, "xz") == 0)
        targetPos += Vec3(10, 0, 10);
      else if (strcmp(forceLookAimTarget, "yz") == 0)
        targetPos += Vec3(0, 10, 10);
      else
      {
        IEntity *pEntity = GetISystem()->GetIEntitySystem()->FindEntityByName(forceLookAimTarget);
        if (pEntity)
          targetPos = pEntity->GetPos();
        else
          targetPos.zero();
      }
      m_State.vLookTargetPos = targetPos;
      m_State.vAimTargetPos = targetPos;
			m_State.aimTargetIsValid = true;
    }

    GetProxy()->Update(&m_State);

		// Restore foced state.
		//m_State.bodystate = lastStance;

		m_State.bReevaluate = false;	// always give a chance for the reevaluation to reach the proxy
											// before setting it to false
		UpdateAlertness();
	}
	if (m_Parameters.m_fAwarenessOfPlayer >0)
		CheckAwarenessPlayer();

	// update close contact info
	if(m_bCloseContact && (GetAISystem()->GetFrameStartTime() - m_CloseContactTime).GetSeconds()>1.5f)
		m_bCloseContact = false;

	// Time out unreachable hidepoints.
	TimeOutVec3List::iterator end(m_recentUnreachableHideObjects.end());
	for(TimeOutVec3List::iterator it = m_recentUnreachableHideObjects.begin(); it != end;)
	{
		it->first -= GetAISystem()->GetFrameDeltaTime();
		if(it->first < 0.0f)
			it = m_recentUnreachableHideObjects.erase(it);
		else
			++it;
	}

	UpdateCloakScale();

	if(type == AIUPDATE_FULL)
	{
		// Health
		RecorderEventData recorderEventData((float)GetProxy()->GetActorHealth());
		RecordEvent(IAIRecordable::E_HEALTH, &recorderEventData);
	}
}

//===================================================================
// UpdateTargetMovementState
//===================================================================
void CPuppet::UpdateTargetMovementState()
{
	const float dt = m_fTimePassed;

	if(!m_pAttentionTarget || dt < 0.00001f)
	{
		m_lastTargetValid = false;
		m_targetApproaching = false;
		m_targetFleeing = false;
		m_targetApproach = 0;
		m_targetFlee = 0;
		return;
	}

	bool	targetValid = false;
	if(m_pAttentionTarget->GetType() == AIOBJECT_PLAYER || m_pAttentionTarget->GetType() == AIOBJECT_CPUPPET)
		targetValid = true;

	Vec3	targetPos = m_pAttentionTarget->GetPos();
	Vec3	targetDir = m_pAttentionTarget->GetMoveDir();
	float	targetSpeed = m_pAttentionTarget->GetVelocity().GetLength();
	const Vec3&	puppetPos = GetPos();

	if(!m_lastTargetValid)
	{
		m_lastTargetValid = true;
		m_lastTargetSpeed = targetSpeed;
		m_lastTargetPos = targetPos;
	}

	const float	approachMin = 10.0f;
	const float	approachMax = 20.0f;
	const float	fleeMin = 10.0f;
	const float fleeMax = 30.0f;

	if(!targetValid && m_lastTargetValid)
	{
		targetPos = m_lastTargetPos;
		targetSpeed = m_lastTargetSpeed;
	}


//	if(targetValid)
	{
		float	curDist = Distance::Point_Point(targetPos, puppetPos);
		float	lastDist = Distance::Point_Point(m_lastTargetPos, puppetPos);

		Vec3	dirTargetToPuppet = puppetPos - targetPos;
		dirTargetToPuppet.NormalizeSafe();

		float	dot = (1.0f + dirTargetToPuppet.Dot(targetDir)) * 0.5f;

		bool movingTowards = curDist < lastDist && targetSpeed > 0; //fabsf(curDist - lastDist) > 0.01f;
		bool movingAway = curDist > lastDist && targetSpeed > 0; //fabsf(curDist - lastDist) > 0.01f;

		if(curDist < approachMax && movingTowards)
			m_targetApproach += targetSpeed * sqr(dot) * 0.25f;
		else
			m_targetApproach -= dt * 2.0f;

		if(curDist > fleeMin && movingAway)
			m_targetFlee += targetSpeed * sqr(1.0f - dot) * 0.1f;
		else
			m_targetFlee -= dt * 2.0f;

		m_lastTargetPos = targetPos;
	}
/*	else
	{
		// Cannot see the target -> guess based on distance!
		m_targetApproach -= dt * 2.0f;
		m_targetFlee -= dt * 2.0f;
	}*/

	m_targetApproach = clamp(m_targetApproach, 0.0f, 10.0f);
	m_targetFlee = clamp(m_targetFlee, 0.0f, 10.0f);

	bool	approaching = m_targetApproach > 9.9f;
	bool	fleeing = m_targetFlee > 9.9f;

	if(approaching != m_targetApproaching)
	{
		m_targetApproaching = approaching;
		if(m_targetApproaching)
		{
			m_targetApproach = 0;
			SetSignal(1, "OnTargetApproaching", m_pAttentionTarget->GetEntity(), 0, g_crcSignals.m_nOnTargetApproaching);		
		}
	}

	if(fleeing != m_targetFleeing)
	{
		m_targetFleeing = fleeing;
		if(m_targetFleeing)
		{
			m_targetFlee = 0;
			SetSignal(1, "OnTargetFleeing", m_pAttentionTarget->GetEntity(), 0, g_crcSignals.m_nOnTargetFleeing);		
		}
	}
}

//===================================================================
// UpdateAlertness
//===================================================================
void CPuppet::UpdateAlertness()
{
	int nextAlertness = GetProxy()->GetAlertnessState();
	if (m_Alertness != nextAlertness && m_Parameters.m_nSpecies && m_Parameters.m_bSpeciesHostility)
	{
		if(nextAlertness>0)
		{
			// reset perceptionScale to 1.0f when alerted
			m_Parameters.m_PerceptionParams.perceptionScale.visual = 1.0f;
			m_Parameters.m_PerceptionParams.perceptionScale.audio = 1.0f;
		}
		if (m_Alertness && GetAISystem()->m_AlertnessCounters[m_Alertness])
			--GetAISystem()->m_AlertnessCounters[m_Alertness];
		++GetAISystem()->m_AlertnessCounters[nextAlertness];
	}
	m_Alertness = nextAlertness;
}


//===================================================================
// GetEventOwner
//===================================================================
IAIObject* CPuppet::GetEventOwner(IAIObject* pOwned) const
{
	if (!pOwned) return 0;
	// TO DO: for memory/sound target etc, probably it's better to set their association equal to the owner
	// instead of searching for it here
	for (PotentialTargetMap::const_iterator ei = m_perceptionEvents.begin(), eiend = m_perceptionEvents.end(); ei != eiend; ++ei)
	{
		const SAIPotentialTarget& ed = ei->second;
		if (ed.pDummyRepresentation == pOwned || ei->first == pOwned)
			return ei->first;
	}
	return 0;
}

//===================================================================
// UpdatePuppetInternalState
//===================================================================
void CPuppet::UpdatePuppetInternalState()
{
	FUNCTION_PROFILER(GetAISystem()->m_pSystem, PROFILE_AI);

	if (m_fLastStuntTime > 0.0f)
		m_fLastStuntTime = max(0.0f, m_fLastStuntTime - GetAISystem()->GetFrameDeltaTime() );

	// Decay the current attention target preference.
	const float targetPersistence = m_Parameters.m_PerceptionParams.targetPersistence;
	if (m_AttTargetPersistenceTimeout > 0.0f && m_pAttentionTarget)
	{
		const float dt = m_pAttentionTarget->GetSubType() == STP_MEMORY ? m_fTimePassed * 3.0f : m_fTimePassed;
		m_AttTargetPersistenceTimeout = max(0.0f, m_AttTargetPersistenceTimeout - dt);
	}
	const float curTargetPersistence = m_AttTargetPersistenceTimeout / (targetPersistence > 0.0f ? targetPersistence : 1.0f);

	// Update alarmed state
	m_alarmedTime -= m_fTimePassed;
	if (m_alarmedTime < 0.0f)
		m_alarmedTime = 0.0f;

	// Sight range threshold.
	// The sight attenuation range envelope changes based on the alarmed state.
	// The threshold is changed smoothly.
	const float alarmLevelChangeTime = 3.0f;
	const float alarmLevelGoal = IsAlarmed() ? 1.0f : 0.0f;
	m_alarmedLevel += (alarmLevelGoal - m_alarmedLevel) * (m_fTimePassed / alarmLevelChangeTime);
	Limit(m_alarmedLevel, 0.0f, 1.0f);

	CAIObject* bestTarget = 0;
	SAIPotentialTarget* bestTargetEvent = 0;
	float bestTargetPriority = 0.0f;
	bool currentTargetErased = false;

	for (PotentialTargetMap::iterator ei = m_perceptionEvents.begin(); ei != m_perceptionEvents.end(); )
	{
		SAIPotentialTarget& ed = ei->second;
		CAIObject* pEventOwner = ei->first;
		CAIActor* pEventOwnerActor = pEventOwner->CastToCAIActor();

		// Update event sound
		float soundExposure = 0.0f;
		if (ed.soundTime > 0.0f)
		{
			ed.soundTime -= m_fTimePassed;
			if (ed.soundTime < 0.0f)
			{
				ed.soundTime = 0.0f;
				ed.soundMaxTime = 0.0f;
				ed.soundThreatLevel = 0.0f;
				ed.soundPos.Set(0,0,0);
			}
			else
			{
				soundExposure = LinStep(0.0f, ed.soundMaxTime * 0.8f, ed.soundTime) * ed.soundThreatLevel;
			}
		}

		// Update vis event
		float visualExposure = 0.0f;

		if (ed.visualTime > 0.0f)
		{
			bool visible = ed.visualMaxTime > 0.0f && ed.visualTime >= (ed.visualMaxTime - 0.25f);
			if (visible)
			{
				ed.visualType = SAIPotentialTarget::VIS_VISIBLE;
				ed.visualPos = pEventOwner->GetPos();
			}
			else
			{
				ed.visualType = SAIPotentialTarget::VIS_MEMORY;

				// Wipe any sound info, so that we don't confuse memory and sound
				// Maybe do this other way around also
				ed.soundTime = 0.0f;
				ed.soundMaxTime = 0.0f;
				ed.soundThreatLevel = 0.0f;
				ed.soundPos.zero();
			}

			ed.visualTime -= m_fTimePassed;
			if (ed.visualTime < 0.0f)
			{
				ed.visualTime = 0.0f;
				ed.visualMaxTime = 0.0f;
				ed.visualThreatLevel = 0.0f;
				ed.visualPos.Set(0,0,0);
			}
			if (ed.visualMaxTime > 0.0f)
				visualExposure = (visible ? 1.0f : 0.0f) * ed.visualThreatLevel;
		}

		// Reaction speed.
		float exposureTarget = max(visualExposure, soundExposure);
		
		float delta = exposureTarget - ed.exposure;
		if (delta > 0.0f)
		{
			float changeTime = IsAlarmed() ? GetAISystem()->m_cvSOMSpeedCombat->GetFVal() : GetAISystem()->m_cvSOMSpeedRelaxed->GetFVal();
			changeTime *= 1.0f - exposureTarget*0.5f;
			float rate = min(m_fTimePassed / changeTime, 1.0f);
			if (delta > rate) delta = rate;
			ed.exposure += delta;
		}
		else
		{
			// TODO: Parametrize
			float changeTime = GetParameters().m_PerceptionParams.forgetfulnessTarget / (IsAlarmed() ? 4 : 8); //IsAlarmed() ? GetAISystem()->m_cvSOMSpeedCombat->GetFVal() : GetAISystem()->m_cvSOMSpeedRelaxed->GetFVal();
			float rate = min(m_fTimePassed / changeTime, 1.0f);
			if (delta < -rate) delta = -rate;

			ed.exposure += delta;
		}

		EAITargetThreat newThreat = AITHREAT_NONE;
		float newTimeout = 0.0f;
		if (ed.exposure > PERCEPTION_AGGRESSIVE_THR)
		{
			newThreat = AITHREAT_AGGRESSIVE;
			newTimeout = GetParameters().m_PerceptionParams.forgetfulnessTarget;
			SetAlarmed();
		}
		else if (ed.exposure > PERCEPTION_THREATENING_THR)
		{
			newThreat = AITHREAT_THREATENING;
			newTimeout = GetParameters().m_PerceptionParams.forgetfulnessSeek;
		}
		else if (ed.exposure > PERCEPTION_INTERESTING_THR)
		{
			newThreat = AITHREAT_INTERESTING;
			newTimeout = GetParameters().m_PerceptionParams.forgetfulnessMemory;
		}

		if (newThreat >= ed.threat)
		{
			ed.threat = newThreat;
			ed.threatTimeout = newTimeout;
		}
		ed.exposureThreat = newThreat;

		// Update the dummy location and direction.
		if (ed.pDummyRepresentation)
		{
			if (ed.exposure > PERCEPTION_INTERESTING_THR)
			{
				if (visualExposure >= soundExposure && !ed.visualPos.IsZero())
				{
					Vec3 dir = ed.visualPos - ed.pDummyRepresentation->GetPos();
					dir.Normalize();
					ed.pDummyRepresentation->SetPos(ed.visualPos, dir);
				}
				else if (!ed.soundPos.IsZero())
				{
					Vec3 dir = ed.soundPos - ed.pDummyRepresentation->GetPos();
					dir.Normalize();
					ed.pDummyRepresentation->SetPos(ed.soundPos);
				}
			}
		}

		// Remove the potential target if the target is invisible or dead.
		if (pEventOwnerActor && (pEventOwnerActor->GetParameters().m_bInvisible || !pEventOwnerActor->IsActive()))
		{
			if (pEventOwner == m_pAttentionTarget || ed.pDummyRepresentation == m_pAttentionTarget)
				currentTargetErased = true;
			// Remove iterator first, or else the event will be removed in OnObjectRemoved().
			CAIObject	*pDummyRepresentation = ed.pDummyRepresentation;
			m_perceptionEvents.erase(ei++);
			if (pDummyRepresentation)
				GetAISystem()->RemoveDummyObject(pDummyRepresentation);
			continue;
		}

		ed.threatTimeout -= m_fTimePassed;
		if (ed.threatTimeout < 0.0f)
		{
			if (ed.threat == AITHREAT_AGGRESSIVE)
			{
				// Enemy timed out.
				ed.threat = AITHREAT_THREATENING;
				ed.threatTimeout = GetParameters().m_PerceptionParams.forgetfulnessSeek;
			}
			else if (ed.threat == AITHREAT_THREATENING)
			{
				// Threatening timed out.
				ed.threat = AITHREAT_INTERESTING;
				ed.threatTimeout = GetParameters().m_PerceptionParams.forgetfulnessMemory;
				if (ed.visualType == SAIPotentialTarget::VIS_MEMORY)
					ed.visualType = SAIPotentialTarget::VIS_NONE;
			}
			else
			{
				// Interesting timed out.
				// Other timeouts have passed too - remove the event.
				if (ed.soundTime < 0.0001f && ed.visualTime < 0.0001f)
				{
					if (pEventOwner == m_pAttentionTarget || ed.pDummyRepresentation == m_pAttentionTarget)
						currentTargetErased = true;
					// Remove iterator first, or else the event will be removed in OnObjectRemoved().
					CAIObject	*pDummyRepresentation = ed.pDummyRepresentation;
					m_perceptionEvents.erase(ei++);
					if (pDummyRepresentation)
						GetAISystem()->RemoveDummyObject(pDummyRepresentation);
					continue;
				}
			}
		}

		// Figure out event type.
		ed.type = AITARGET_NONE;
		if (ed.threat >= AITHREAT_INTERESTING)
		{
			if (ed.visualType == SAIPotentialTarget::VIS_VISIBLE)
			{
				// Do not allow indirect sightings to get aggressive because of sounds.
				if (ed.indirectSight && ed.threat >= AITHREAT_AGGRESSIVE && ed.soundThreatLevel > PERCEPTION_INTERESTING_THR)
					ed.type = AITARGET_SOUND;
				else
					ed.type = AITARGET_VISUAL;
			}
			else if (ed.visualType == SAIPotentialTarget::VIS_MEMORY)
			{
				ed.type = AITARGET_MEMORY;
				ed.pDummyRepresentation->SetSubType(IAIObject::STP_MEMORY);
			}
			else
			{
				ed.type = AITARGET_SOUND;
				ed.pDummyRepresentation->SetSubType(IAIObject::STP_SOUND);
			}

			// Update priority
			switch (ed.threat)
			{
			case AITHREAT_AGGRESSIVE: ed.priority = 1.5f; break;
			case AITHREAT_THREATENING: ed.priority = 1.0f; break;
			case AITHREAT_INTERESTING: ed.priority = 0.5f; break;
			default: ed.priority = 0.0f; break;
			}

			// 1) Use the distance as the base priority.
			Vec3 targetPos;
			if (ed.type == AITARGET_VISUAL && ed.threat == AITHREAT_AGGRESSIVE)
				targetPos = pEventOwner->GetPos();	
			else
				targetPos = ed.pDummyRepresentation->GetPos();
			float distanceMultiplier = 0.0f;
			float fSightRange = GetSightRange(pEventOwner);
			if(fSightRange != 0.0f)
			{
				distanceMultiplier = 1.0f - min(Distance::Point_Point(targetPos, GetPos()) / fSightRange, 1.0f);
			}
			ed.priority += distanceMultiplier;

			// 2) Scale the priority down for agents which are not making any sound.
			// This also includes the weapon sounds, so targets firing their weapons
			// will get higher priority.
			ed.priority *= 0.8f + soundExposure * 0.2f;

			// 3) Scale by the combined species and object type multiplier.
			ed.priority *= GetPriorityMultiplier(pEventOwner);

			// 4) Bias the current target.
			if (pEventOwner == m_pAttentionTarget)
				ed.priority += curTargetPersistence;
			ed.upPriorityTime -= m_fTimePassed;
			if (ed.upPriorityTime < 0.0f)
			{
				ed.upPriority = 0.0f;
				ed.upPriorityTime = 0.0f;
			}
			ed.priority += ed.upPriority;

			// Bias the selection of the most threatening target based on the combat class scale.
			const int targetCombatClass = pEventOwnerActor ? pEventOwnerActor->GetParameters().m_CombatClass : -1;
			float	scaledPriority = ed.priority * GetAISystem()->GetCombatClassScale(m_Parameters.m_CombatClass, targetCombatClass);
			if (scaledPriority > 0.01f && scaledPriority > bestTargetPriority)
			{
				bestTargetPriority = scaledPriority;
				if (ed.type == AITARGET_VISUAL && ed.threat == AITHREAT_AGGRESSIVE)
					bestTarget = pEventOwner;
				else
					bestTarget = ed.pDummyRepresentation;
				bestTargetEvent = &ed;
			}
		}
		else
		{
			ed.priority = 0.0f;
			ed.upPriorityTime = 0.0f;
			ed.upPriority = 0.0f;
		}

		// Affect stealth-o-meter for players
		ed.threatTime = 0.0f;
		switch (ed.threat)
		{
		case AITHREAT_AGGRESSIVE:
			{
				float t = ed.threatTimeout / GetParameters().m_PerceptionParams.forgetfulnessTarget;
				ed.threatTime = PERCEPTION_THREATENING_THR + (PERCEPTION_AGGRESSIVE_THR - PERCEPTION_THREATENING_THR) * t;
			}
			break;
		case AITHREAT_THREATENING:
			{
				float t = ed.threatTimeout / GetParameters().m_PerceptionParams.forgetfulnessSeek;
				ed.threatTime = PERCEPTION_INTERESTING_THR + (PERCEPTION_THREATENING_THR - PERCEPTION_INTERESTING_THR) * t;
			}
			break;
		case AITHREAT_INTERESTING:
			{
				float t = ed.threatTimeout / GetParameters().m_PerceptionParams.forgetfulnessMemory;
				ed.threatTime = PERCEPTION_INTERESTING_THR * t;
			}
			break;
		}
		AffectSOM(pEventOwner, ed.exposure, ed.threatTime);

		++ei;
	}


	if (bestTarget && bestTargetEvent)
	{
		m_State.eTargetStuntReaction = AITSR_NONE;
		m_State.eTargetType = bestTargetEvent->type;
		m_State.eTargetThreat = bestTargetEvent->threat;

		if (bestTarget != m_pAttentionTarget) 
		{
			// New attention target
			SetAttentionTarget(bestTarget);
			m_AttTargetPersistenceTimeout = targetPersistence;

			// When seeing a visible target, check for stunt reaction.
			if (bestTargetEvent->type == AITARGET_VISUAL && bestTargetEvent->threat == AITHREAT_AGGRESSIVE)
			{
				if (CAIPlayer* pPlayer = bestTarget->CastToCAIPlayer())
				{
					if (IsHostile(pPlayer) && m_fLastStuntTime <= 0.0f )
					{
						bool isStunt = pPlayer->IsDoingStuntActionRelatedTo(GetPos(), m_Parameters.m_PerceptionParams.sightRange/5.0f);
						if (m_targetLostTime > m_Parameters.m_PerceptionParams.stuntReactionTimeOut && isStunt)
						{
							m_State.eTargetStuntReaction = AITSR_SEE_STUNT_ACTION;
							m_fLastStuntTime = m_Parameters.m_PerceptionParams.StuntTimeout;
						}
						else if (pPlayer->IsCloakEffective())
						{
							m_State.eTargetStuntReaction = AITSR_SEE_CLOAKED;
							m_fLastStuntTime = m_Parameters.m_PerceptionParams.StuntTimeout;
						}
					}
				}
			}
			m_State.bReevaluate = true;
		}
		else if (m_pAttentionTarget)
		{
			// Handle state change of the current attention target.

			// The state lowering.
			if (m_AttTargetThreat >= AITHREAT_AGGRESSIVE && bestTargetEvent->threat < AITHREAT_AGGRESSIVE)
			{
				// Aggressive -> threatening
				if (bestTargetEvent->type == AITARGET_VISUAL || bestTargetEvent->type == AITARGET_MEMORY)
					SetSignal(0, "OnNoTargetVisible", GetEntity(), 0, g_crcSignals.m_nOnNoTargetVisible);
			}
			else if (m_AttTargetThreat >= AITHREAT_THREATENING && bestTargetEvent->threat < AITHREAT_THREATENING)
			{
				// Threatening -> interesting
				SetSignal(0, "OnNoTargetAwareness", GetEntity(), 0, g_crcSignals.m_nOnNoTargetAwareness);
			}

			// The state rising.
			// Use the exposure threat to trigger the signals. This will result multiple
			// same signals (like on threatening sound) to be sent if the exposure is
			// crossing the threshold. This should allow more responsive behaviors, but
			// at the same time prevent too many signals.
			// The signal is resent only if the exposure is crossing the current or higher threshold.
			if (bestTargetEvent->exposureThreat >= bestTargetEvent->threat || m_AttTargetType != bestTargetEvent->type)
			{
				if (m_AttTargetExposureThreat < AITHREAT_AGGRESSIVE && bestTargetEvent->exposureThreat >= AITHREAT_AGGRESSIVE)
				{
					// Threatening -> aggressive
					m_State.bReevaluate = true;
					if (bestTargetEvent->type == AITARGET_VISUAL)
					{
						if (CAIPlayer* pPlayer = bestTarget->CastToCAIPlayer())
						{
							if (IsHostile(pPlayer) && m_fLastStuntTime <= 0.0f)
							{
								bool isStunt = pPlayer->IsDoingStuntActionRelatedTo(GetPos(), m_Parameters.m_PerceptionParams.sightRange/5.0f);
								if (m_targetLostTime > m_Parameters.m_PerceptionParams.stuntReactionTimeOut && isStunt)
								{
									m_State.eTargetStuntReaction = AITSR_SEE_STUNT_ACTION;
									m_fLastStuntTime = m_Parameters.m_PerceptionParams.StuntTimeout;
								}
								else if (pPlayer->IsCloakEffective())
								{
									m_State.eTargetStuntReaction = AITSR_SEE_CLOAKED;
									m_fLastStuntTime = m_Parameters.m_PerceptionParams.StuntTimeout;
								}
							}
						}
					}
				}
				else if (m_AttTargetExposureThreat < AITHREAT_THREATENING && bestTargetEvent->exposureThreat >= AITHREAT_THREATENING)
				{
					// Interesting -> threatening
					m_State.bReevaluate = true;
				}
				else if (m_AttTargetExposureThreat < AITHREAT_INTERESTING && bestTargetEvent->exposureThreat >= AITHREAT_INTERESTING)
				{
					// None -> interesting
					m_State.bReevaluate = true;
				}
			}
		}
		// Keep track of the current state, used to track the state transitions.
		m_AttTargetType = bestTargetEvent->type;
		m_AttTargetThreat = bestTargetEvent->threat;
		m_AttTargetExposureThreat = bestTargetEvent->exposureThreat;
	}
	else
	{
		// No attention target, reset if the current target is erased.
		// The check for current target erased allows to forcefully set the
		// attention target.
		if (m_pAttentionTarget)
		{
			if (currentTargetErased)
				SetAttentionTarget(0);
			m_State.bReevaluate = true;
		}

		m_AttTargetType = AITARGET_NONE;
		m_AttTargetThreat = AITHREAT_NONE;
		m_AttTargetExposureThreat = AITHREAT_NONE;

		m_State.eTargetType = AITARGET_NONE;
		m_State.eTargetThreat = AITHREAT_NONE;
	}

	// update devaluated points
	DevaluedMap::iterator di, next;
	for (di = m_mapDevaluedPoints.begin(); di != m_mapDevaluedPoints.end(); di = next)
	{
		next = di; ++next;
		di->second -= m_fTimePassed;
		if (di->second < 0)
			m_mapDevaluedPoints.erase(di);
	}

	UpdateTargetMovementState();

	if (m_pAttentionTarget)
		m_State.fDistanceFromTarget = Distance::Point_Point(m_pAttentionTarget->GetPos(), GetPos());
}

//===================================================================
// AffectSOM
//===================================================================
void CPuppet::AffectSOM(CAIObject* pTarget, float exposure, float threat)
{
	if (IsHostile(pTarget))
	{
		// affect stealth-o-meter for players
		if (CAIPlayer* pPlayer = pTarget->CastToCAIPlayer())
		{
			if (GetType() == AIOBJECT_VEHICLE)
				pPlayer->RegisterDetectionLevels(SAIDetectionLevels(0,0, exposure, threat));
			else
				pPlayer->RegisterDetectionLevels(SAIDetectionLevels(exposure, threat, 0,0));
		}
		else if (CAIVehicle* pVehicle = pTarget->CastToCAIVehicle()) // if this is player's vehicle
		{
			CAIPlayer* pPlayer( (CAIPlayer*)GetAISystem()->GetPlayer() );
			if (	 !pPlayer 	// no player? should not happen, just in case
				||  pPlayer->GetParameters().m_nSpecies != pVehicle->GetParameters().m_nSpecies  // enemy vehicle, but not player's
				|| !pVehicle->IsPlayerInside())	// player is not driving (to make sure all the friendly vehicles don't affect SOM)
			{
				// Enemy vehicle
				if (GetType() == AIOBJECT_VEHICLE)
					pVehicle->RegisterDetectionLevels(SAIDetectionLevels(0,0, exposure, threat));
				else
					pVehicle->RegisterDetectionLevels(SAIDetectionLevels(exposure, threat, 0,0));
			}
			else
			{
				// Players vehicle.
				if (GetType() == AIOBJECT_VEHICLE)
					pPlayer->RegisterDetectionLevels(SAIDetectionLevels(0,0, exposure, threat));
				else
					pPlayer->RegisterDetectionLevels(SAIDetectionLevels(exposure, threat, 0,0));
			}
		}
		else if (CPuppet* pPuppet = pTarget->CastToCPuppet())
		{
			if (GetType() == AIOBJECT_VEHICLE)
				pPuppet->RegisterDetectionLevels(SAIDetectionLevels(0,0, exposure, threat));
			else
				pPuppet->RegisterDetectionLevels(SAIDetectionLevels(exposure, threat, 0,0));
		}
	}
	else if (pTarget->GetSubType() == IAIObject::STP_SOUND)
	{
		if (CAIPlayer* pPlayer = (CAIPlayer*)GetAISystem()->GetPlayer() )
		{
			if (GetType() == AIOBJECT_VEHICLE)
				pPlayer->RegisterDetectionLevels(SAIDetectionLevels(0,0, exposure, threat));
			else
				pPlayer->RegisterDetectionLevels(SAIDetectionLevels(exposure, threat, 0,0));
		}
	}

}

//===================================================================
// Event
//===================================================================
void CPuppet::Event(unsigned short eType, SAIEVENT *pEvent)
{
//	GetAISystem()->Record(this, IAIRecordable::E_EVENT, GetEventName(eType));

	switch (eType)
	{
		case AIEVENT_DROPBEACON:
			UpdateBeacon();
			break;
		case AIEVENT_ONBODYSENSOR:
			if (pEvent->fThreat > m_fSuppressFiring)
				m_fSuppressFiring = pEvent->fThreat;
			else if (!pEvent->fThreat)
				m_fSuppressFiring = 0.0f;
			break;
		case AIEVENT_CLEAR:
			{
				ClearActiveGoals();
				m_bLooseAttention = false;
				GetAISystem()->FreeFormationPoint(this);
				SetAttentionTarget(0);
				m_bBlocked = false;
				m_bCanReceiveSignals = true;
			}
			break;
		case AIEVENT_CLEARACTIVEGOALS:
			ClearActiveGoals();
			m_bBlocked = false;
			break;
		case AIEVENT_REJECT:
			RestoreAttentionTarget( );
			break;
		case AIEVENT_DISABLE:
			m_bEnabled = false;
//			m_bUncheckedBody = false;
			GetAISystem()->UpdateGroupStatus(GetGroupId());
			GetAISystem()->NotifyEnableState(this, m_bEnabled);
			break;
		case AIEVENT_ENABLE:
			if (GetProxy()->GetActorHealth() < 1)
			{
// can happen when rendering dead bodies? AI should not be enabled
//				AIAssert(!"Trying to enable dead character!");
				return;
			}
			m_bEnabled = true;
			GetAISystem()->UpdateGroupStatus(GetGroupId());
			GetAISystem()->NotifyEnableState(this, m_bEnabled);
			break;
		case AIEVENT_SLEEP:
			m_fireMode = FIREMODE_OFF;
			m_bUncheckedBody = true;
			if ( GetProxy()->GetLinkedVehicleEntityId() == 0 )
			{
				m_bEnabled = false;
				GetAISystem()->NotifyEnableState(this, m_bEnabled);
			}
			break;
		case AIEVENT_WAKEUP:
			ClearActiveGoals();
			m_bLooseAttention = false;
			SetAttentionTarget(0);
			m_bEnabled = true;
			GetAISystem()->NotifyEnableState(this, m_bEnabled);
			m_bUncheckedBody = false;
			GetAISystem()->UpdateGroupStatus(GetGroupId());
			break;
		case AIEVENT_ONVISUALSTIMULUS:
				HandleVisualStimulus(pEvent);
			break;
		case AIEVENT_ONPATHDECISION:
				HandlePathDecision(pEvent);
			break;
		case AIEVENT_ONSOUNDEVENT:
				HandleSoundEvent(pEvent);
			break;
		case AIEVENT_AGENTDIED:
			// make sure everybody knows I have died
			GetAISystem()->NotifyTargetDead(this);

			m_bUncheckedBody = true;
			m_bEnabled = false;
			GetAISystem()->NotifyEnableState(this, m_bEnabled);
			GetAISystem()->RemoveFromGroup(GetGroupId(), this);
			
			GetAISystem()->ReleaseFormationPoint(this);
			GetAISystem()->CancelAnyPathsFor(this);
			ReleaseFormation();

			ResetCurrentPipe( true );
			m_State.vSignals.clear();

			// remove from alertness counters
			{
				// get alertness before doing GetProxy()->Reset()!!!
				int alertness = GetProxy()->GetAlertnessState();
				if ( m_Parameters.m_nSpecies &&
					m_Parameters.m_bSpeciesHostility &&
					GetAISystem()->m_AlertnessCounters[alertness] )
					--GetAISystem()->m_AlertnessCounters[alertness];
			}

			if(GetProxy())
				GetProxy()->Reset(AIOBJRESET_SHUTDOWN);

			// Create indication of dead body.
			GetAISystem()->RegisterDeadBody(m_Parameters.m_nSpecies, GetPos());

			if (pEvent->nDeltaHealth && m_Parameters.m_bPerceivePlayer)
				GetAISystem()->GetAutoBalanceInterface()->RegisterEnemyLifetime(m_fEngageTime);
			break;

		case AIEVENT_FORCEDNAVIGATION:
			m_vForcedNavigation = pEvent->vForcedNavigation;
			break;
		case AIEVENT_ADJUSTPATH:
			m_adjustpath =  pEvent->nType;
			break;
		default:
			// triggering non-existing event - script error?
			// no error - various e AIEVENT_PLAYER_STUNT_ go here - processed in AIPlayer
			//AIAssert(0);
			break;
	}
		
}


//===================================================================
// HandleVisualStimulus
//===================================================================
void CPuppet::HandleVisualStimulus(SAIEVENT* pAIEvent)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	CAIObject* pEventOwner = (CAIObject*)pAIEvent->pSeen;

	if (pEventOwner)
	{
		float fObjectMultiplier = 1.f;		
		MapMultipliers::iterator mi = GetAISystem()->m_mapMultipliers.find(pEventOwner->GetType());
		if (mi != GetAISystem()->m_mapMultipliers.end())
			fObjectMultiplier = mi->second;
		if (fObjectMultiplier > 1.0f)
			m_bCanReceiveSignals = true;
	}
	if (!m_bCanReceiveSignals)
		return;

	// Update existing event if possible.
	CAIObject* pEventAssociation = pEventOwner;
	if (pEventOwner->GetType() == AIOBJECT_ATTRIBUTE)
		pEventAssociation = (CAIObject*)pEventOwner->GetAssociation();
	if (!pEventAssociation)
		return;

	const bool isAttribute = pEventOwner->GetType() == AIOBJECT_ATTRIBUTE;

	float distToTarget = Distance::Point_Point(GetPos(), pEventOwner->GetPos());
	float sightRange = GetSightRange(pEventOwner);
	const float envMin = GetParameters().m_PerceptionParams.sightEnvScaleNormal;
	const float envMax = GetParameters().m_PerceptionParams.sightEnvScaleAlarmed;
	float sightRangeThr = sightRange * (envMin + GetPerceptionAlarmLevel() * (envMax - envMin));

	float sightRangeScale = 1.0f;
	float sightScale = 1.0f;

	// Reduce sight range for attributes.
	if (isAttribute)
		sightRangeScale *= 0.5f;

	CAIActor* pEventOwnerActor = pEventOwner->CastToCAIActor();
	if (pEventOwnerActor)
	{
		bool targetUsesCombatLight = pEventOwnerActor->IsUsingCombatLight();
		if (targetUsesCombatLight || isAttribute)
			SetAlarmed();

		// First calculate factors that will affect the sight range.

		// Target under water
		float waterOcclucion = GetAISystem()->GetWaterOcclusionValue(pEventOwnerActor->GetPos());
		sightRangeScale *= 0.1f + (1 - waterOcclucion) * 0.9f;

		if (!targetUsesCombatLight)
		{
			// Target stance
			float stanceSize = 1.0f;
			SAIBodyInfo bi;
			if (pEventOwnerActor->GetProxy())
				pEventOwnerActor->GetProxy()->QueryBodyInfo(bi);
			float targetHeight = bi.stanceSize.GetSize().z;
			if (pEventOwnerActor->GetType() == AIOBJECT_VEHICLE)
				targetHeight = 0.0f;
			if (targetHeight > 0.0f)
				stanceSize = targetHeight / m_Parameters.m_PerceptionParams.stanceScale;
			sightRangeScale *= 0.25f + stanceSize * 0.75f;
		}

/*			// calculate movement factor
		// harder to see if far and not moving
		float	vel2(0.0f);
		if(pAITargetObject->GetPhysics())
		{
			pe_status_dynamics	dyn;
			pAITargetObject->GetPhysics()->GetStatus(&dyn);
			vel2 = dyn.v.len2();
		}
		fNewIncrease*=(m_Parameters.m_PerceptionParams.velBase + m_Parameters.m_PerceptionParams.velScale*vel2);*/

		// Target scale factor
		if (m_Parameters.m_PerceptionParams.bThermalVision)
			sightRangeScale *= pEventOwnerActor->m_Parameters.m_PerceptionParams.heatScale;
		else
			sightRangeScale *= pEventOwnerActor->m_Parameters.m_PerceptionParams.camoScale;

		// Flowgraph Target scale factor
		sightRangeScale *= m_Parameters.m_PerceptionParams.perceptionScale.visual;

		// Secondly calculate factors that will affect the actual sighting value.

		// Scale the sighting based on the agent FOV.
		if (m_FOVPrimaryCos > 0.0f)
		{
			Vec3 dirToTarget = pEventOwnerActor->GetPos() - GetPos();
			Vec3 viewDir = GetViewDir();
			if (!IsUsing3DNavigation())
			{
				dirToTarget.z = 0;
				viewDir.z = 0;
			}
			dirToTarget.Normalize();
			viewDir.Normalize();

			float dot = viewDir.Dot(dirToTarget);
			float fovFade = 1.0f;
			if (dot < m_FOVPrimaryCos)
			{
				// Spread the FOV slightly when alarmed if the prim and sec fovs are different
				if (fabsf(m_FOVSecondaryCos - m_FOVPrimaryCos) > 0.001f)
				{
					const float thr = m_FOVPrimaryCos + (m_FOVSecondaryCos - m_FOVPrimaryCos) * 0.5f * GetPerceptionAlarmLevel();
					fovFade = LinStep(m_FOVSecondaryCos, thr, dot);
				}
			}

			// When the target is really close, reduce the effect of the FOV.
			if (distToTarget < m_fRadius * 2)
				fovFade = max(fovFade, LinStep(m_fRadius * 2, m_fRadius, distToTarget));

			sightScale *= fovFade;
		}

		// Target cloak
		if (pEventOwnerActor->IsCloakEffective())
		{
			// Cloak on and not carrying anything
			// Fade out the perception increment based on the cloak distance parameters.
			float cloakMinDist = pEventOwnerActor->GetCloakMinDist();
			float cloakMaxDist = pEventOwnerActor->GetCloakMaxDist();
			sightScale *= 1.f - LinStep(cloakMinDist, cloakMaxDist, distToTarget)*pEventOwnerActor->m_Parameters.m_fCloakScale;
		}
	}

	// Calculate final threat level.
	float threatLevel = 0.0f;

	sightRange *= sightRangeScale;
	sightRangeThr *= sightRangeScale;

	if (sightRange > 0.0001f)
	{
		if (fabsf(sightRange - sightRangeThr) < 1e-6)
			threatLevel = 1.0f;
		else
			threatLevel = LinStep(sightRange, sightRangeThr, distToTarget);
		threatLevel *= sightScale;

		// Do not allow attributes to go aggressive.
		if (pEventOwner->GetType() == AIOBJECT_ATTRIBUTE)
		{
			const float thr = PERCEPTION_AGGRESSIVE_THR * 0.95f;
			threatLevel = min(threatLevel, thr);
		}

		if (threatLevel > 0.0001f)
		{
			// The threat level is considerable enough to be registered.

			// Reuse existing event or create a new one of the current target has not been seen yet.
			SAIPotentialTarget* pEvent = 0;
			PotentialTargetMap::iterator ei = m_perceptionEvents.find(pEventAssociation);
			if (ei == m_perceptionEvents.end())
			{
				// The event for this target does not exists yet, add new.
				FRAME_PROFILER("AddToVisibleList > new",gEnv->pSystem,PROFILE_AI );
				// This object has no associated event with it - create a new potential target
				SAIPotentialTarget ed;
				static int nameCounter = 0;
				char szName[256];
				_snprintf(szName,256,"Perception of %s V%d", pEventAssociation != NULL ? pEventAssociation->GetName() : "NOBODY", nameCounter++);
				ed.pDummyRepresentation = GetAISystem()->CreateDummyObject(szName);
				ed.pDummyRepresentation->SetSubType(STP_MEMORY);
				ed.pDummyRepresentation->SetAssociation(pEventAssociation);

				pEvent = AddEvent(pEventAssociation, ed);
			}
			else
			{
				pEvent = &(ei->second);
			}

			if (!pEvent)
				return;

			// Combine the max threat level per target per update.
			if (pEvent->visualFrameId != GetAISystem()->GetAITickCount() || threatLevel > pEvent->visualThreatLevel)
			{
				if (pEventOwner->GetType() == AIOBJECT_ATTRIBUTE)
					pEvent->visualPos = pEventAssociation->GetPos();
				else
					pEvent->visualPos = pEventOwner->GetPos();
				pEvent->visualThreatLevel = threatLevel;
				pEvent->visualMaxTime = 3.0f;
				pEvent->visualTime = pEvent->visualMaxTime;
				pEvent->visualFrameId = GetAISystem()->GetAITickCount();
				pEvent->indirectSight = pEventOwner->GetType() == AIOBJECT_ATTRIBUTE;
			}
		}
		else
		{
			// If the target exists already, zero out the threat value.
			SAIPotentialTarget* pEvent = 0;
			PotentialTargetMap::iterator ei = m_perceptionEvents.find(pEventAssociation);
			if (ei != m_perceptionEvents.end())
			{
				pEvent = &(ei->second);
				pEvent->visualThreatLevel = 0.0f;
			}
		}
	}
}

//===================================================================
// IsObjectInFOVCone
//===================================================================
bool CPuppet::IsObjectInFOVCone(CAIObject* pTarget, float distanceScale)
{
FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );

	const Vec3& pos = pTarget->GetPos();

	Vec3 direction = pos - GetPos();

	// lets see if it is outside of its vision range
	if (direction.GetLengthSquared() > sqr(GetSightRange(pTarget) * distanceScale))
		return false; 

	// Check for the omnidirectional special case.
	if( m_FOVSecondaryCos <= -1.0f )
		return true;

	//TODO remove this - needed now cos vehicle system puts passenger/driver at the same point
	if (direction.GetLengthSquared() < .1f )
		return false; 

	Vec3 myorievector = GetViewDir();

//	if(GetType() == AIOBJECT_VEHICLE)	// vehicle models are flipped on z axis
//		myorievector = -myorievector;

	direction.NormalizeSafe();

	float fdot = direction.Dot(myorievector);

	if (fdot < m_FOVSecondaryCos)
		return false;	// its outside of his FOV
	return true;
}

bool CPuppet::IsPointInFOVCone(const Vec3 &pos, float distanceScale)
{
FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );
	Vec3 direction = pos - GetPos();

	// lets see if it is outside of its vision range
	if (direction.GetLengthSquared() > sqr(GetSightRange(0) * distanceScale))
		return false; 

	// Check for the omnidirectional special case.
	if( m_FOVSecondaryCos <= -1.0f )
		return true;

	//TODO remove this - needed now cos vehicle system puts passenger/driver at the same point
	if (direction.GetLengthSquared() < .1f )
		return false; 

	Vec3 myorievector = GetViewDir();

//	if(GetType() == AIOBJECT_VEHICLE)	// vehicle models are flipped on z axis
//		myorievector = -myorievector;

	direction.NormalizeSafe();

	float fdot = direction.Dot(myorievector);

	if (fdot < m_FOVSecondaryCos)
		return false;	// its outside of his FOV
	return true;
}

//===================================================================
// UpdateLookTarget
//===================================================================
void CPuppet::UpdateLookTarget(CAIObject *pTarget)
{
	FUNCTION_PROFILER(GetAISystem()->m_pSystem, PROFILE_AI);
		
	if (m_pPersonalInterestManager)
	{
		CAIObject * pInterest = m_pPersonalInterestManager->GetInterestDummyPoint();
		if (pInterest) 
			pTarget = pInterest; // If no interest point, will be NULL
	}

	// Update look direction and strafing
	bool lookAtTarget = false;
	Vec3 lookTarget(0,0,0);

	// If not moving allow to look at target.
	if (m_State.fDesiredSpeed < 0.01f || m_Path.Empty())
		lookAtTarget = true;

	// Check if strafing should be allowed.
	UpdateStrafing();
	if (m_State.allowStrafing)
		lookAtTarget = true;

	if (m_bLooseAttention)
	{
		if (m_pLooseAttentionTarget)
			pTarget = m_pLooseAttentionTarget;
	}
	if (m_pFireTarget && m_fireMode != FIREMODE_OFF)
		pTarget = GetFireTargetObject();

	if(m_fireMode == FIREMODE_MELEE || m_fireMode == FIREMODE_MELEE_FORCED)
	{
		if (pTarget)
		{
			lookTarget = pTarget->GetPos();
			lookTarget.z = GetPos().z;
			lookAtTarget = true;
		}
	}

	bool	use3DNav = IsUsing3DNavigation();
	bool isMoving = m_State.fDesiredSpeed > 0.0f && m_State.curActorTargetPhase == eATP_None && !m_State.vMoveDir.IsZero();

	Vec3 dirToTarget(0,0,0);
	float distToTarget = FLT_MAX;
	if (pTarget)
	{
		dirToTarget = pTarget->GetPos() - GetPos();
		distToTarget = dirToTarget.GetLength();
		if(distToTarget > 0.0001f)
			dirToTarget /= distToTarget;

		// Allow to look at the target when it is almost at the movement direction or very close.
		if (isMoving)
		{
			Vec3	move(m_State.vMoveDir);
			if (!use3DNav)
				move.z = 0.0f;
			move.NormalizeSafe(Vec3(0,0,0));
			if (distToTarget < 2.5f || move.Dot(dirToTarget) > cosf(DEG2RAD(60)))
				lookAtTarget = true;
		}
	}

	if (lookAtTarget && pTarget)
	{
		Vec3	vTargetPos = pTarget->GetPos();

		const float	maxDeviation = distToTarget * sinf(DEG2RAD(15));

		if(distToTarget > GetParameters().m_fPassRadius)
		{
			lookTarget = vTargetPos;
			Limit(lookTarget.z, GetPos().z - maxDeviation, GetPos().z + maxDeviation);
		}

		// Clamp the lookat height when the target is close.
		int TargetType = pTarget->GetType();
		if (distToTarget < 1.0f ||
			(TargetType == AIOBJECT_DUMMY || TargetType == AIOBJECT_HIDEPOINT || TargetType == AIOBJECT_WAYPOINT ||
			TargetType > AIOBJECT_PLAYER) && distToTarget < 5.0f) // anchors & dummy objects
		{
			if(!use3DNav)
			{
				lookTarget = vTargetPos;
				Limit(lookTarget.z, GetPos().z - maxDeviation, GetPos().z + maxDeviation);
			}
		}
	}
	else if (isMoving)
	{
		// Check if strafing should be allowed.

		// Look forward or to the movement direction
		Vec3  lookAheadPoint;
		float	lookAheadDist = 2.5f;

		if(m_pPathFollower)
		{
      float junk;
			lookAheadPoint = m_pPathFollower->GetPathPointAhead(lookAheadDist, junk);
		}
		else
		{
			if(!m_Path.GetPosAlongPath(lookAheadPoint, lookAheadDist, !m_movementAbility.b3DMove, true))
				lookAheadPoint = GetPhysicsPos();
		}

		// Since the path height is not guaranteed to follow terrain, do not even try to look up or down.
		lookTarget = lookAheadPoint;

		// Make sure the lookahead position is far enough so that the catchup logic in the path following
		// together with look-ik does not get flipped.
		Vec3  delta = lookTarget - GetPhysicsPos();
		delta.z = 0.0f;
		float dist = delta.GetLengthSquared();
		if (dist < sqr(1.0f))
		{
			float u = 1.0f - sqrtf(dist);
			Vec3 safeDir = GetBodyDir();
			safeDir.z = 0;
			delta = delta + (safeDir - delta) * u;
		}
		delta.Normalize();

		lookTarget = GetPhysicsPos() + delta * 40.0f;
		lookTarget.z = GetPos().z;
	}
	else
	{
		// Disable look target.
		lookTarget.zero();
	}

	if (!m_posLookAtSmartObject.IsZero())
	{
		// The SO lookat should override the lookat target in case not requesting to fire and not using lookat goalop.
		if (!m_bLooseAttention && m_fireMode == FIREMODE_OFF)
		{
			lookTarget = m_posLookAtSmartObject;
		}
	}

	if (!lookTarget.IsZero())
	{
		if (m_allowStrafeLookWhileMoving && m_fireMode != FIREMODE_OFF && GetFireTargetObject())
		{
			float distSqr = Distance::Point_Point2DSq(GetFireTargetObject()->GetPos(), GetPos());
			if (!m_closeRangeStrafing)
			{
				// Outside the range
				const float thr = GetParameters().m_PerceptionParams.sightRange * 0.12f;
				if (distSqr < sqr(thr))
					m_closeRangeStrafing = true;
			}
			if (m_closeRangeStrafing)
			{
				// Inside the range
				m_State.allowStrafing = true;
				const float thr = GetParameters().m_PerceptionParams.sightRange * 0.12f + 2.0f;
				if (distSqr > sqr(thr))
					m_closeRangeStrafing = false;
			}
		}

		float distSqr = Distance::Point_Point2DSq(lookTarget, GetPos());
		if (distSqr < sqr(2.0f))
		{
			Vec3 fakePos = GetPos() + GetBodyDir() * 2.0f;
			if (distSqr < sqr(0.7f))
				lookTarget = fakePos;
			else
			{
				float speed = m_State.vMoveDir.GetLength();
				speed = CLAMP(speed,0,10.f);
				float d = sqrtf(distSqr);
				float u = 1.0f - (d - 0.7f) / (2.0f - 0.7f);
				lookTarget += speed/10 * u * (fakePos - lookTarget);
			}
		}
	}

	// for the invehicle gunners
	if ( GetProxy() )
	{
		SAIBodyInfo bodyInfo;
		GetProxy()->QueryBodyInfo(bodyInfo);

		if ( bodyInfo.linkedVehicleEntity )
		{
			if ( GetProxy()->GetActorIsFallen() )
			{
				lookTarget.zero();
			}
			else
			{
				CAIObject* pUnit = (CAIObject*)bodyInfo.linkedVehicleEntity->GetAI();
				if (pUnit)
				{
					if (pUnit->CastToCAIVehicle())
					{
						lookTarget.zero();
						if ( m_bLooseAttention && m_pLooseAttentionTarget )
							pTarget = m_pLooseAttentionTarget;
						if ( pTarget )
						{
							lookTarget = pTarget->GetPos();
							m_State.allowStrafing = false;
						}
					}
				}
			}
		}
	}

	float lookTurnSpeed = GetAlertness() > 0 ? m_Parameters.m_lookCombatTurnSpeed : m_Parameters.m_lookIdleTurnSpeed;
	if (lookTurnSpeed <= 0.0f)
		m_State.vLookTargetPos = lookTarget;
	else
		m_State.vLookTargetPos = InterpolateLookOrAimTargetPos(m_State.vLookTargetPos, lookTarget, lookTurnSpeed);
}

//===================================================================
// InterpolateLookOrAimTargetPos
//===================================================================
Vec3 CPuppet::InterpolateLookOrAimTargetPos(const Vec3& current, const Vec3& target, float maxRatePerSec)
{
	if (!target.IsZero())
	{
		// Interpolate.
		Vec3	curDir;
		if (current.IsZero())
		{
			curDir = GetBodyDir(); // GetViewDir();
		}
		else
		{
			curDir = current - GetPos();
			curDir.Normalize();
		}

		Vec3	reqDir = target - m_vLastPosition; // GetPos();
		float dist = reqDir.NormalizeSafe();

		// Slerp
		float cosAngle = curDir.Dot(reqDir);

		const float eps = 1e-6f;
		const float maxRate = maxRatePerSec * GetAISystem()->GetFrameDeltaTime();
		const float thr = cosf(maxRate);

		if (cosAngle > thr || maxRate < eps)
		{
			return target;
		}
		else
		{
			float angle = acos_tpl(cosAngle);

			// Allow higher rate when over 90degrees.
			float scale = 1.0f + clamp((angle - gf_PI/4)/(gf_PI/4), 0.0f, 1.0f)*2;
			if (angle < eps || angle < maxRate*scale)
				return target;

			float t = (maxRate*scale) / angle;

			Quat	curView;
			curView.SetRotationVDir(curDir);
			Quat	reqView;
			reqView.SetRotationVDir(reqDir);

			Quat	view;
			view.SetSlerp(curView, reqView, t);

			return GetPos() + (view * FORWARD_DIRECTION) * dist;
		}
	}

	// Clear look target.
	return target;
}

//===================================================================
// UpdateLookTarget3D
//===================================================================
void CPuppet::UpdateLookTarget3D(CAIObject *pTarget)
{
	// this is for the scout mainly

	m_State.vLookTargetPos.zero();
	m_State.aimTargetIsValid = false;	//	m_State.vAimTargetPos.zero();

	if ( m_bLooseAttention && m_pLooseAttentionTarget )
	{
		pTarget = m_pLooseAttentionTarget;
		m_State.vLookTargetPos = pTarget->GetPos();
	}
	m_State.vForcedNavigation =m_vForcedNavigation;

}
//===================================================================
// AdjustPath
//===================================================================
void CPuppet::AdjustPath()
{
	if (false == AdjustPathAroundObstacles())
	{
		AILogEvent("CPuppet::AdjustPath Unable to adjust path around obstacles for %s", GetName());
		ClearPath("CPuppet::AdjustPath m_Path");
		m_nPathDecision = PATHFINDER_NOPATH;
		return;
	}

	if ( m_adjustpath > 0 )
	{
		IAISystem::tNavCapMask navCapMask = m_movementAbility.pathfindingProperties.navCapMask;
		int nBuildingID;
		IVisArea *pArea;
		IAISystem::ENavigationType currentNavType	= GetAISystem()->CheckNavigationType(GetPos(), nBuildingID, pArea, navCapMask);
		ConvertPathToSpline( currentNavType );
	}
}

//===================================================================
// AdjustMovementUrgency
//===================================================================
void CPuppet::AdjustMovementUrgency(float& urgency, float pathLength, float* maxPathLen)
{
	if (!maxPathLen)
		maxPathLen = &m_adaptiveUrgencyMaxPathLen;

	if(m_adaptiveUrgencyScaleDownPathLen > 0.0001f)
	{
		if(pathLength > *maxPathLen)
		{
			*maxPathLen = pathLength;

			int scaleDown = 0;
			float scale = m_adaptiveUrgencyScaleDownPathLen;
			while(scale > m_adaptiveUrgencyMaxPathLen && scaleDown < 4)
			{
				scale /= 2.0f;
				scaleDown++;
			}

			int minIdx = MovementUrgencyToIndex(m_adaptiveUrgencyMin);
			int maxIdx = MovementUrgencyToIndex(m_adaptiveUrgencyMax);
			int idx = maxIdx - scaleDown;
			if(idx < minIdx) idx = minIdx;

			// Special case for really short paths.
			if (*maxPathLen < 1.2f && idx > 2)
				idx = 2; // walk

			urgency = IndexToMovementUrgency(idx);
		}
	}
}

//===================================================================
// HandlePathDecision
//===================================================================
void CPuppet::HandlePathDecision(SAIEVENT *pEvent)
{
	static bool dumpOrigPath = false;
	if (dumpOrigPath)
	{
		GetAISystem()->GetNavPath().Dump("AI system path");
		dumpOrigPath = false;
	}


  if (pEvent->bPathFound)
	{
    if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
    {
      Vec3 end(-1, -1, -1);
      Vec3 start(-1, -1, -1);
      Vec3 next(-1, -1, -1);
      if (!GetAISystem()->GetNavPath().Empty())
      {
        end = GetAISystem()->GetNavPath().GetLastPathPos();
        start = GetAISystem()->GetNavPath().GetPrevPathPoint()->vPos;
        next = GetAISystem()->GetNavPath().GetNextPathPos();
      }
      const SNavPathParams &params = GetAISystem()->GetNavPath().GetParams();
      AILogAlways("CPuppet::HandlePathDecision %s found path to (%5.2f, %5.2f, %5.2f) from (%5.2f, %5.2f, %5.2f) Stored path end is (%5.2f, %5.2f, %5.2f) Next is (%5.2f, %5.2f, %5.2f)",
        GetName(), 
        end.x, end.y, end.z, 
        start.x, start.y, start.z, 
        params.end.x, params.end.y, params.end.z,
        next.x, next.y, next.z);
    }

    m_nPathDecision = PATHFINDER_PATHFOUND;

		// Check if there is a pending NavSO which has not started playing yet and cancel it.
		// Path regeneration while the animation is playing is ok, but should be inhibited while preparing for actor target.
		// This may happen if some goal operation or such requests path regeneration
		// while the actor target has been requested. This should not happen.
		if(m_eNavSOMethod != nSOmNone && m_State.curActorTargetPhase == eATP_Waiting)
		{
			if(GetAISystem()->m_cvDebugPathFinding->GetIVal())
			{
				if(!m_State.actorTargetReq.animation.empty())
					AILogAlways("CPuppet::HandlePathDecision %s got new path during actor target request (phase:%d, anim:%s). Please investigate, this case should not happen.",
						GetName(), (int)m_State.curActorTargetPhase, m_State.actorTargetReq.animation.c_str());
				else
					AILogAlways("CPuppet::HandlePathDecision %s got new path during actor target request (phase:%d, vehicle:%s, seat:%d). Please investigate, this case should not happen.",
						GetName(), (int)m_State.curActorTargetPhase, m_State.actorTargetReq.vehicleName.c_str(), m_State.actorTargetReq.vehicleSeat);
			}
			SetNavSOFailureStates();
		}

		// Check if there is a pending end-of-path actor target request which has not started playing yet and cancel it.
		// Path regeneration should be inhibited while preparing for actor target.
		if(GetActiveActorTargetRequest() && m_State.curActorTargetPhase == eATP_Waiting)
		{
			if(GetAISystem()->m_cvDebugPathFinding->GetIVal())
			{
				if(!m_State.actorTargetReq.animation.empty())
					AILogAlways("CPuppet::HandlePathDecision %s got new path during actor target request (phase:%d, anim:%s). Please investigate, this case should not happen.",
					GetName(), (int)m_State.curActorTargetPhase, m_State.actorTargetReq.animation.c_str());
				else
					AILogAlways("CPuppet::HandlePathDecision %s got new path during actor target request (phase:%d, vehicle:%s, seat:%d). Please investigate, this case should not happen.",
					GetName(), (int)m_State.curActorTargetPhase, m_State.actorTargetReq.vehicleName.c_str(), m_State.actorTargetReq.vehicleSeat);
			}
			// Stop exact positioning.
			m_State.actorTargetReq.Reset();
		}

		// lets get it from the ai system
		// [Mikko] The AIsystem working path version does not always match the 
		// pipeuser path version. When new path is found, for it to be used.
		// In some weird cases the new path version ca even be the same as the
		// current one. Make sure the new path version is different than the current.
		int oldVersion = m_Path.GetVersion();
		m_Path = GetAISystem()->GetNavPath();
		m_Path.SetVersion(oldVersion+1);

		// Store the path destination point for later checks.
		if(!m_Path.Empty())
			m_PathDestinationPos = m_Path.GetLastPathPos();

		// Cut the path end
		float trimLength = m_Path.GetParams().endDistance;
		if (fabsf(trimLength) > 0.0001f)
			m_Path.TrimPath(trimLength, IsUsing3DNavigation());

		// Trim the path so that it will only reach until the first navSO animation.
		m_Path.PrepareNavigationalSmartObjects(this);

		// Update adaptive urgency control before the path gets processed further
		AdjustMovementUrgency(m_State.fMovementUrgency, m_Path.GetPathLength(IsUsing3DNavigation()));

		AdjustPath();
		
    m_OrigPath = m_Path;
    // AdjustPath may have failed 
    if (m_nPathDecision == PATHFINDER_PATHFOUND && m_OrigPath.GetPath().size() < 2)
    {
      AIWarning("CPuppet::HandlePathDecision (%5.2f, %5.2f, %5.2f) original path size = %d for %s",
				GetPos().x, GetPos().y, GetPos().z, m_OrigPath.GetPath().size(), GetName());
    }
	
	}
	else
	{
		if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
		{
			AILogAlways("CPuppet::HandlePathDecision %s No path", GetName());
		}
		ClearPath("CPuppet::HandlePathDecision m_Path");
		//m_OrigPath.Clear("CPuppet::HandlePathDecision m_OrigPath");
		m_nPathDecision = PATHFINDER_NOPATH;
	} 
}

//===================================================================
// Devalue
//===================================================================
void CPuppet::Devalue(IAIObject *pObject, bool bDevaluePuppets, float	fAmount)
{
	DevaluedMap::iterator di;

	unsigned short type = ((CAIObject*)pObject)->GetType();

	if (pObject == m_pAttentionTarget)
		SetAttentionTarget(0);

	if ((type == AIOBJECT_PUPPET) && !bDevaluePuppets)
		return;

	if (type == AIOBJECT_PLAYER)
		return;

	// remove it from map of pending events, so that it does not get reacquire
	PotentialTargetMap::iterator ei = m_perceptionEvents.find((CAIObject*)pObject);
	if (ei != m_perceptionEvents.end())
	{
		CAIObject*	dummy = ei->second.pDummyRepresentation;
		m_perceptionEvents.erase(ei);
		if (dummy)
			GetAISystem()->RemoveDummyObject(dummy);
	}

	if ((di = m_mapDevaluedPoints.find((CAIObject*)pObject)) == m_mapDevaluedPoints.end())
		m_mapDevaluedPoints.insert(DevaluedMap::iterator::value_type((CAIObject*)pObject,fAmount));
}

//===================================================================
// IsDevalued
//===================================================================
bool CPuppet::IsDevalued(IAIObject *pObject)
{
	return m_mapDevaluedPoints.find((CAIObject*)pObject) != m_mapDevaluedPoints.end();
}

//===================================================================
// ClearDevalued
//===================================================================
void CPuppet::ClearDevalued()
{
	m_mapDevaluedPoints.clear();
}

//===================================================================
// OnObjectRemoved
//===================================================================
void CPuppet::OnObjectRemoved(CAIObject *pObject)
{
	CPipeUser::OnObjectRemoved(pObject);

	PotentialTargetMap::iterator ei = m_perceptionEvents.find(pObject);
	if (ei != m_perceptionEvents.end())
	{
		CAIObject*	dummy = ei->second.pDummyRepresentation;
		m_perceptionEvents.erase(ei);
		if (dummy && dummy != pObject)
			GetAISystem()->RemoveDummyObject(dummy);
	}

  if (!m_steeringObjects.empty())
    m_steeringObjects.erase(std::remove(m_steeringObjects.begin(), m_steeringObjects.end(), pObject), m_steeringObjects.end());

	DevaluedMap::iterator di;

	if ( (di=m_mapDevaluedPoints.find(pObject)) != m_mapDevaluedPoints.end() )
		m_mapDevaluedPoints.erase(di);

	if (pObject == m_pAttentionTarget)
		SetAttentionTarget(0);

	if (pObject == m_pPrevAttentionTarget)
		m_pPrevAttentionTarget = 0;

	RemoveFromGoalPipe(pObject);

	if (pObject == m_pLastOpResult)
		m_pLastOpResult = 0;

	if (pObject == m_pReservedNavPoint)
		m_pReservedNavPoint = 0;

	if (pObject == m_pLooseAttentionTarget)
	{
		m_pLooseAttentionTarget = 0;
		m_bLooseAttention = false;
	}

	if (!m_State.vSignals.empty())
	{
		DynArray<AISIGNAL>::iterator si=m_State.vSignals.begin();
		for (;si!=m_State.vSignals.end();)
		{
			if ((*si).pSender && (*si).pSender == pObject->GetEntity())
				si=m_State.vSignals.erase(si);
			else
				++si;
		}
	}

	if( m_pTrackPattern )
		m_pTrackPattern->OnObjectRemoved( pObject );
}

//===================================================================
// UpTargetPriority
//===================================================================
void CPuppet::UpTargetPriority(const IAIObject* pTarget, float fPriorityIncrement)
{
	PotentialTargetMap::iterator eiTarget = m_perceptionEvents.find((CAIObject*)pTarget);
	// see if target is available
	if (eiTarget == m_perceptionEvents.end())
		return;

	SAIPotentialTarget& ed = eiTarget->second;

	// only existing targets can be upped
	// otherwise can get AttTarget set without OnPlayerSeen sent
	if (ed.type == AITARGET_MEMORY || ed.type == AITARGET_NONE)
		return;

	float bestPriority = 0.0f;
	CAIObject* bestTarget = 0;

	// we want to skip the top one
	for (PotentialTargetMap::iterator ei = m_perceptionEvents.begin(), end = m_perceptionEvents.end(); ei != end; ++ei)
	{
		SAIPotentialTarget& edLooping = ei->second;

		// target selection based on priority
		if (edLooping.priority > bestPriority)
		{
			bestPriority = edLooping.priority;
			bestTarget = ei->first;
		}
	}
	if (bestTarget == pTarget)
		return;

	ed.upPriority += fPriorityIncrement;
	if (ed.upPriority > 1.0f)
		ed.upPriority = 1.0f;
	ed.upPriorityTime = 2.0f;	// TODO: parameterize.
}

//===================================================================
// HandleSoundEvent
//===================================================================
void CPuppet::HandleSoundEvent(SAIEVENT* pEvent)
{
	if (!m_bCanReceiveSignals)
		return;

	Vec3 eventPos = pEvent->vPosition;
	CAIObject* pEventOwner = (CAIObject*)pEvent->pSeen;
	float eventRadius = pEvent->fThreat;
	float distanceToEvent = Distance::Point_Point(eventPos, GetPos());

	float soundThreatLevel = 0.0f;
	float soundTime = 0.0f;

	float distNorm = distanceToEvent / eventRadius;

	switch(pEvent->nType)
	{
	case AISE_GENERIC:
		soundThreatLevel = PERCEPTION_INTERESTING_THR * LinStep(1.0f, 0.25f, distNorm);
		soundTime = 2.0f;
		break;
	case AISE_COLLISION:
		soundThreatLevel = PERCEPTION_INTERESTING_THR * LinStep(1.0f, 0.5f, distNorm);
		soundTime = 4.0f;
		break;
	case AISE_COLLISION_LOUD:
		soundThreatLevel = PERCEPTION_THREATENING_THR * LinStep(1.0f, 0.5f, distNorm);
		soundTime = 4.0f;
		break;
	case AISE_MOVEMENT:
		soundThreatLevel = PERCEPTION_INTERESTING_THR * LinStep(1.0f, 0.5f, distNorm);
		soundTime = 2.0f;
		break;
	case AISE_MOVEMENT_LOUD:
		soundThreatLevel = PERCEPTION_THREATENING_THR * LinStep(1.0f, 0.5f, distNorm);
		soundTime = 4.0f;
		break;
	case AISE_BULLETCOLLISION:
		soundThreatLevel = PERCEPTION_AGGRESSIVE_THR * LinStep(1.0f, 0.75f, distNorm);
		soundTime = 4.0f;
		if (soundThreatLevel > PERCEPTION_INTERESTING_THR)
		{	
			SetAlarmed();
			return;
		}
		break;
	case AISE_WEAPON:
		soundThreatLevel = PERCEPTION_AGGRESSIVE_THR * LinStep(1.0f, 0.25f, distNorm);
		soundTime = 4.0f;
		if (soundThreatLevel > PERCEPTION_INTERESTING_THR)
			SetAlarmed();
		break;
	case AISE_EXPLOSION:
		soundThreatLevel = PERCEPTION_AGGRESSIVE_THR * LinStep(1.0f, 0.75f, distNorm);
		soundTime = 6.0f;
		if (soundThreatLevel > PERCEPTION_INTERESTING_THR)
			SetAlarmed();
		break;
	default:
		soundThreatLevel = PERCEPTION_INTERESTING_THR * LinStep(1.0f, 0.25f, distNorm);
		soundTime = 2.0f;
		break;
	};

	// Make sure that at max level the value reaches over the threshold.
	soundThreatLevel *= 1.1f;

	// Attenuate by perception parameter scaling.
	const float paramScale = m_Parameters.m_PerceptionParams.audioScale * m_Parameters.m_PerceptionParams.perceptionScale.audio;
	soundThreatLevel *= paramScale;

	// Skip really quiet sounds.
	if (soundThreatLevel < 0.00001f)
		return;

	if (pEventOwner)
	{
		if (CPuppet* pPuppet = pEventOwner->CastToCPuppet())
		{
			if (!IsHostile(pEventOwner))
			{
				if (pEvent->nType == AISE_WEAPON)
				{
					// A living agent from the same species is shooting,
					// make the sound at the position where the agent is shooting at
					// and decoy it as an enemy.
					pEventOwner = (CAIObject*)pPuppet->GetAttentionTarget();
					if (!pEventOwner || !pEventOwner->IsAgent())
						return;
					eventPos = pEventOwner->GetPos();
					float distanceToTarget = Distance::Point_Point(eventPos, GetPos());
					distNorm = min(distanceToEvent / eventRadius, 1.0f);
					soundThreatLevel = PERCEPTION_AGGRESSIVE_THR * LinStep(1.0f, 0.25f, distNorm);
					soundTime = 4.0f;
				}
				else if (pEvent->nType != AISE_EXPLOSION)
				{
					return;
				}
			}
		}
		else
		{
			// Handle enemy sound.
			if (!IsHostile(pEventOwner))
				return;
		}
	}

	PotentialTargetMap::iterator ei = m_perceptionEvents.find(pEventOwner);
	if (ei != m_perceptionEvents.end())
	{
		SAIPotentialTarget &ed = ei->second;
		if (soundThreatLevel >= ed.soundThreatLevel)
		{
			ed.soundTime = soundTime;
			ed.soundMaxTime = soundTime;
			ed.soundThreatLevel = soundThreatLevel;
			ed.soundPos = eventPos;
		}
	}
	else
	{
		SAIPotentialTarget ed;

		ed.soundTime = soundTime;
		ed.soundMaxTime = soundTime;
		ed.soundThreatLevel = soundThreatLevel;
		ed.soundPos = eventPos;

		static int nameCounter = 0;
		char szName[256];
		_snprintf(szName,256,"Perception of %s S%d", pEventOwner != NULL ? pEventOwner->GetName() : "NOBODY", nameCounter++);
		ed.pDummyRepresentation = GetAISystem()->CreateDummyObject(szName);

		if (pEventOwner)
		{
			ed.pDummyRepresentation->SetAssociation(pEventOwner);
			AddEvent(pEventOwner, ed);
		}
		else
		{
			AddEvent(ed.pDummyRepresentation, ed);
		}
	}

}

//===================================================================
// IsPointInAudibleRange
//===================================================================
bool CPuppet::IsPointInAudibleRange(const Vec3 &pos, float fRadius)
{
	return (pos-GetPos()).GetLengthSquared() <= fRadius*fRadius;
}

//===================================================================
// GetMovementSpeedRange
//===================================================================
void CPuppet::GetMovementSpeedRange(float fUrgency, bool slowForStrafe, float& normalSpeed, float& minSpeed, float& maxSpeed) const
{
	AgentMovementSpeeds::EAgentMovementUrgency urgency;
	AgentMovementSpeeds::EAgentMovementStance stance;

	bool vehicle = GetType() == AIOBJECT_VEHICLE;

	if (fUrgency < 0.5f * (AISPEED_SLOW + AISPEED_WALK))
		urgency = AgentMovementSpeeds::AMU_SLOW;
	else if (fUrgency < 0.5f * (AISPEED_WALK + AISPEED_RUN))
		urgency = AgentMovementSpeeds::AMU_WALK;
	else if (fUrgency < 0.5f * (AISPEED_RUN + AISPEED_SPRINT))
		urgency = AgentMovementSpeeds::AMU_RUN;
	else
		urgency = AgentMovementSpeeds::AMU_SPRINT;
/*	{
		if (slowForStrafe)
			urgency = AgentMovementSpeeds::AMU_RUN;
		else
			urgency = AgentMovementSpeeds::AMU_SPRINT;
	}*/

	if (IsAffectedByLightLevel() && m_movementAbility.lightAffectsSpeed)
	{
		// Disable sprinting in dark light conditions.
		if (urgency == AgentMovementSpeeds::AMU_SPRINT)
		{
			if (GetLightLevel() == AILL_DARK)
				urgency = AgentMovementSpeeds::AMU_RUN;
		}
	}

	SAIBodyInfo bodyInfo;
	GetProxy()->QueryBodyInfo(bodyInfo);

	switch (bodyInfo.stance)
	{
	case STANCE_STEALTH: stance = AgentMovementSpeeds::AMS_STEALTH; break;
	case STANCE_CROUCH: stance = AgentMovementSpeeds::AMS_CROUCH; break;
	case STANCE_PRONE: stance = AgentMovementSpeeds::AMS_PRONE; break;
	case STANCE_SWIM: stance = AgentMovementSpeeds::AMS_SWIM; break;
	case STANCE_RELAXED: stance = AgentMovementSpeeds::AMS_RELAXED; break;
	default: stance = AgentMovementSpeeds::AMS_COMBAT; break;
	}

	const float artificialMinSpeedMult = 1.0f;

	if(fUrgency==0.f)
		normalSpeed = minSpeed = maxSpeed= 0.f;
	else
	{
		AgentMovementSpeeds::SSpeedRange	fwdRange = m_movementAbility.movementSpeeds.GetRange(stance, urgency);
		fwdRange.min *= artificialMinSpeedMult;
		normalSpeed = fwdRange.def;
		minSpeed = fwdRange.min;
		maxSpeed = fwdRange.max;
	}

	if (m_movementAbility.directionalScaleRefSpeedMin > 0.0f)
	{
		float desiredSpeed = normalSpeed;
		float desiredTurnSpeed = m_bodyTurningSpeed;
		float travelAngle = Ang3::CreateRadZ(GetBodyDir(), GetMoveDir()); //m_State.vMoveDir);

		const float refSpeedMin = m_movementAbility.directionalScaleRefSpeedMin;
		const float refSpeedMax = m_movementAbility.directionalScaleRefSpeedMax;

		// When a locomotion is slow (0.5-2.0f), then we can do this motion in all direction more or less at the same speed 
		float t = sqr(clamp((desiredSpeed - refSpeedMin) / (refSpeedMax - refSpeedMin), 0.0f, 1.0f));
		float scaleLimit = clamp(0.8f*(1-t) + 0.1f*t, 0.3f, 1.0f); //never scale more then 0.4 down 

		//adjust desired speed for turns
		float speedScale = 1.0f - fabsf(desiredTurnSpeed*0.40f)/gf_PI;
		speedScale = clamp(speedScale, scaleLimit, 1.0f);

		//adjust desired speed when strafing and running backward
		float strafeSlowDown = (gf_PI - fabsf(travelAngle*0.60f))/gf_PI;
		strafeSlowDown = clamp(strafeSlowDown, scaleLimit, 1.0f);

		//adjust desired speed when running uphill & downhill
		float slopeSlowDown = (gf_PI - fabsf(DEG2RAD(bodyInfo.slopeAngle)/12.0f))/gf_PI;
		slopeSlowDown = clamp(slopeSlowDown, scaleLimit, 1.0f);

		float scale = min(speedScale, min(strafeSlowDown, slopeSlowDown));

		normalSpeed *= scale;
		Limit(normalSpeed, minSpeed, maxSpeed);
//		minSpeed *= scale;
//		maxSpeed *= scale;
	}


/*
//	float fwd = m_movementAbility.movementSpeeds.GetSpeed(urgency, stance, AgentMovementSpeeds::AMD_FWD);
//	float speed = fwd;

	// todo Danny: really should only allow this speed control whilst strafing, since otherwise 
	// idle->move transitions will not get selected properly. However, if the speed control is not
	// done always then we end up seeing very fast back/sideways movement.
	if (slowForStrafe)
	{
		AgentMovementSpeeds::SSpeedRange	sideRange = m_movementAbility.movementSpeeds.GetRange(stance, urgency, AgentMovementSpeeds::AMD_SIDE);
		AgentMovementSpeeds::SSpeedRange	bwdRange = m_movementAbility.movementSpeeds.GetRange(stance, urgency, AgentMovementSpeeds::AMD_BWD);

		sideRange.min *= artificialMinSpeedMult;
		bwdRange.min *= artificialMinSpeedMult;

//		float side = m_movementAbility.movementSpeeds.GetSpeed(urgency, stance, AgentMovementSpeeds::AMD_SIDE);
//		float bwd = m_movementAbility.movementSpeeds.GetSpeed(urgency, stance, AgentMovementSpeeds::AMD_BWD);

		Vec3 velDir = GetVelocity();
		Vec3 dir = GetBodyDir();
		velDir.z = dir.z = 0.0f;
		velDir.NormalizeSafe(); dir.NormalizeSafe();
		float dot = clamp(velDir.Dot(dir), -1.0f, 1.0f);
		float angle = acos_tpl(dot);
		float angleScale = 1.0f - 2.0f * (angle / gf_PI);
		float strafeScale = sgn(angleScale) * sqr(angleScale);	// angleScale

		if (angleScale > 0.0f)
		{
			const float t = strafeScale; //SmoothStep(0.0f, 1.0f, strafeScale);
			normalSpeed = t * fwdRange.def + (1 - t) * sideRange.def;
			minSpeed = t * fwdRange.min + (1 - t) * sideRange.min;
			maxSpeed = t * fwdRange.max + (1 - t) * sideRange.max;
		}
		else
		{
			const float t = 1 + strafeScale; //SmoothStep(-1.0f, 0.0f, strafeScale);
			normalSpeed = t * sideRange.def + (1 - t) * bwdRange.def;
			minSpeed = t * sideRange.min + (1 - t) * bwdRange.min;
			maxSpeed = t * sideRange.max + (1 - t) * bwdRange.max;
		}
	}

	Limit(normalSpeed, minSpeed, maxSpeed);

	// Slow down based on lighting conditions.
	if (IsAffectedByLightLevel() && m_movementAbility.lightAffectsSpeed)
	{
		if (GetLightLevel() == AILL_DARK)
			normalSpeed = minSpeed + (normalSpeed - minSpeed) * GetAISystem()->m_cvMovementSpeedMediumIllumMod->GetFVal();
		else if (GetLightLevel() == AILL_MEDIUM)
			normalSpeed = minSpeed + (normalSpeed - minSpeed) * GetAISystem()->m_cvMovementSpeedDarkIllumMod->GetFVal();
	}
*/
	//	return speed;
}


//===================================================================
// GetObjectType
//===================================================================
CPuppet::EAIObjectType CPuppet::GetObjectType(const CAIObject *ai, unsigned short type)
{
  if (type == AIOBJECT_PLAYER)
    return AIOT_PLAYER;
  else if (type == AIOBJECT_PUPPET)
    return AIOT_AGENTSMALL;
  else if (type == AIOBJECT_VEHICLE)
  {
    // differentiate between medium and big vehicles (e.g. tank can drive over jeep)
    return AIOT_AGENTMED;
  }
  else
    return AIOT_UNKNOWN;
}

//===================================================================
// GetNavInteraction
//===================================================================
CPuppet::ENavInteraction CPuppet::GetNavInteraction(const CAIObject *navigator, const CAIObject *obstacle)
{
  SAIBodyInfo info;
  obstacle->GetProxy()->QueryBodyInfo(info);
  if (info.linkedVehicleEntity)
    return NI_IGNORE;

  unsigned short navigatorType = navigator->GetType();
  unsigned short obstacleType = obstacle->GetType();

  bool enemy = navigator->IsHostile(obstacle);

  EAIObjectType navigatorOT = GetObjectType(navigator, navigatorType);
  EAIObjectType obstacleOT = GetObjectType(obstacle, obstacleType);

  switch (navigatorOT)
  {
  case AIOT_UNKNOWN: 
  case AIOT_PLAYER: 
    return NI_IGNORE;
  case AIOT_AGENTSMALL: 
    // don't navigate around their enemies, unless the enemy is bigger
/*    if (enemy)
      return obstacleOT > navigatorOT ? NI_STEER : NI_IGNORE;
    else*/
      return NI_STEER;
  case AIOT_AGENTMED: 
  case AIOT_AGENTBIG: 
    // don't navigate around their enemies, unless the enemy is same size or bigger
    if (enemy)
      return obstacleOT >= navigatorOT ? NI_STEER : NI_IGNORE;
    else
      return NI_STEER;
  default:
    AIError("GetNavInteraction: Unhandled switch case %d", navigatorOT);
    return NI_IGNORE;
  }
}

//===================================================================
// NavigateAroundObjectsBasicCheck
//===================================================================
bool CPuppet::NavigateAroundObjectsBasicCheck(const Vec3 & targetPos, const Vec3 &myPos, bool in3D, const CAIObject *object, float extraDist)
{
	//to make sure we skip disable entities (hidden in the game, etc)
	IEntity *pEntity(object->GetEntity());
	if(pEntity && !pEntity->IsActive())
		return false;

  if (object->GetType() != AIOBJECT_VEHICLE && !object->IsEnabled()) // vehicles are not enabled when idle, so don't skip them
    return false;

  if (object == this)
    return false;

  ENavInteraction navInteraction = GetNavInteraction(this, object);
  if (navInteraction == NI_IGNORE)
    return false;

/*  static bool checkAdjusted = true;
  if (!m_Path.GetObjectsAdjustedFor().empty() && checkAdjusted)
  {
    if (std::find(m_Path.GetObjectsAdjustedFor().begin(), m_Path.GetObjectsAdjustedFor().end(), object) != m_Path.GetObjectsAdjustedFor().end())
      return false;
  }*/

  Vec3 objectPos = object->GetPos();
  Vec3 delta = objectPos - myPos;
  const CAIActor* pActor = object->CastToCAIActor();
  float avoidanceR = m_movementAbility.avoidanceRadius; 
  avoidanceR += pActor->m_movementAbility.avoidanceRadius;
  avoidanceR += extraDist;
  float avoidanceRSq = square(avoidanceR);

  // skip if we're not close
  float distSq = delta.GetLengthSquared();
  if (distSq > avoidanceRSq)
    return false;

  return true;
}

//===================================================================
// NavigateAroundObjectsInternal
//===================================================================
bool CPuppet::NavigateAroundObjectsInternal(const Vec3 & targetPos, const Vec3 &myPos, bool in3D, const CAIObject *object)
{
  // skip if we're not trying to move towards
  Vec3 objectPos = object->GetPos();
  Vec3 delta = objectPos - myPos;
  float dot = m_State.vMoveDir.Dot(delta);
  if (dot < 0.001f)
    return false;
  const CAIActor* pActor = object->CastToCAIActor();
  float avoidanceR = m_movementAbility.avoidanceRadius; 
  avoidanceR += pActor->m_movementAbility.avoidanceRadius;
  float avoidanceRSq = square(avoidanceR);

  // skip if we're not close
  float distSq = delta.GetLengthSquared();
  if (distSq > avoidanceRSq)
    return false;

  return NavigateAroundAIObject(targetPos, object, myPos, objectPos, true, in3D);
}



//===================================================================
// NavigateAroundObjects
//===================================================================
bool CPuppet::NavigateAroundObjects(const Vec3 &targetPos, bool fullUpdate, bool bSteerAroundTarget)
{
  FUNCTION_PROFILER( GetISystem(),PROFILE_AI );
  bool in3D = IsUsing3DNavigation();
  const Vec3 myPos = GetPhysicsPos();

  bool steering = false;
	bool lastSteeringEnabled = m_steeringEnabled;
	bool steeringEnabled = m_State.vMoveDir.GetLength() > 0.01f && m_State.fDesiredSpeed > 0.01f;

  CTimeValue curTime = GetAISystem()->GetFrameStartTime();
  float deltaT = (curTime - m_lastSteerTime).GetSeconds();
  static float timeForUpdate = 0.5f;

  if (steeringEnabled && (deltaT > timeForUpdate || !lastSteeringEnabled))
  {
		FRAME_PROFILER("NavigateAroundObjects Gather Objects", gEnv->pSystem, PROFILE_AI)
    m_lastSteerTime = curTime;
    m_steeringObjects.clear();

		// Active vehicles and puppets
		const VectorSet<CPuppet*>& enabledPuppetsSet = GetAISystem()->GetEnabledPuppetSet();
		for (VectorSet<CPuppet*>::const_iterator it = enabledPuppetsSet.begin(), itend = enabledPuppetsSet.end(); it != itend; ++it)
		{
			const CAIObject* object = (CAIObject*)*it;
			if(!bSteerAroundTarget && object == m_pPathFindTarget)
				continue;
			static float extraDist = 10.0f;
			if (NavigateAroundObjectsBasicCheck(targetPos, myPos, in3D, object, extraDist))
				m_steeringObjects.push_back(object);
		}

		// Players
		const AIObjects::iterator aiEnd = GetAISystem()->m_Objects.end();
		for (AIObjects::iterator ai = GetAISystem()->m_Objects.find(AIOBJECT_PLAYER); 
			ai != aiEnd && ai->first == AIOBJECT_PLAYER ; ++ai)
		{
			const CAIObject* object = (CAIObject*)ai->second;
			if(!bSteerAroundTarget && object == m_pPathFindTarget)
				continue;
			static float extraDist = 10.0f;
			if (NavigateAroundObjectsBasicCheck(targetPos, myPos, in3D, object, extraDist))
				m_steeringObjects.push_back(object);
		}
	}

	if (steeringEnabled)
	{
		if (GetType() == AIOBJECT_PUPPET && !in3D)
		{
			FRAME_PROFILER("NavigateAroundObjects Update Steering", gEnv->pSystem, PROFILE_AI)
			float nearestDist = FLT_MAX;

			bool check = fullUpdate;
			if (m_updatePriority == AIPUP_VERY_HIGH || m_updatePriority == AIPUP_HIGH)
				check = true;

			if (check || !lastSteeringEnabled)
			{
				AABB selfBounds;
				GetEntity()->GetWorldBounds(selfBounds);
				selfBounds.min.z -= 0.25f;
				selfBounds.max.z += 0.25f;

				// Build occupancy map
				const float radScale = 1.0f - clamp(m_steeringAdjustTime - 1.0f, 0.0f, 1.0f);
				const float selfRad = m_movementAbility.pathRadius * radScale;
				m_steeringOccupancy.Reset(GetPhysicsPos(), GetBodyDir(), m_movementAbility.avoidanceRadius * 2.0f);

				for (unsigned i = 0, ni = m_steeringObjects.size(); i < ni; ++i)
				{
					const CAIActor* pActor = m_steeringObjects[i]->CastToCAIActor();
					if (!pActor) // || !pActor->IsActive())
						continue;

					// Check that the height overlaps.
					IEntity* pActorEnt = pActor->GetEntity();
					AABB bounds;
					pActorEnt->GetWorldBounds(bounds);
					if (selfBounds.min.z >= bounds.max.z || selfBounds.max.z <= bounds.min.z)
						continue;

					if (pActor->GetType() == AIOBJECT_VEHICLE)
					{
						AABB localBounds;
						pActorEnt->GetLocalBounds(localBounds);
						const Matrix34& tm = pActorEnt->GetWorldTM();

						SAIRect3 r;
						GetFloorRectangleFromOrientedBox(tm, localBounds, r);
						// Extend the box based on velocity.
						Vec3 vel = pActor->GetVelocity();
						float speedu = r.axisu.Dot(vel) * 0.25f;
						float speedv = r.axisv.Dot(vel) * 0.25f;
						if (speedu > 0)
							r.max.x += speedu;
						else
							r.min.x += speedu;
						if (speedv > 0)
							r.max.y += speedv;
						else
							r.min.y += speedv;

						// Extend the box based on the 
						r.min.x -= selfRad;
						r.min.y -= selfRad;
						r.max.x += selfRad;
						r.max.y += selfRad;

						const unsigned maxpts = 4;
						Vec3 rect[maxpts];
						rect[0] = r.center + r.axisu * r.min.x + r.axisv * r.min.y;
						rect[1] = r.center + r.axisu * r.max.x + r.axisv * r.min.y;
						rect[2] = r.center + r.axisu * r.max.x + r.axisv * r.max.y;
						rect[3] = r.center + r.axisu * r.min.x + r.axisv * r.max.y;

/*						unsigned last = maxpts-1;
						for (unsigned i = 0; i < maxpts; ++i)
						{
							GetAISystem()->AddDebugLine(rect[last], rect[i], 255,0,0, 0);
							last = i;
						}*/

						float x = r.axisu.Dot(m_steeringOccupancy.GetCenter() - r.center);
						float y = r.axisv.Dot(m_steeringOccupancy.GetCenter() - r.center);

						const float eps = 0.1f;
						if (x >= r.min.x - eps && x <= r.max.x + eps && y >= r.min.y - eps && y <= r.max.y + eps)
						{
//							GetAISystem()->AddDebugLine(m_steeringOccupancy.GetCenter()+Vec3(0,0,-10), m_steeringOccupancy.GetCenter()+Vec3(0,0,10), 0,0,0, 0);
							m_steeringOccupancy.AddObstructionDirection(r.center - m_steeringOccupancy.GetCenter());
						}
						else
						{
							unsigned last = maxpts-1;
							for (unsigned j = 0; j < maxpts; ++j)
							{
								m_steeringOccupancy.AddObstructionLine(rect[last], rect[j]);
								last = j;
							}
						}
					}
					else
					{
						const Vec3& pos = pActor->GetPhysicsPos();
						const float rad = pActor->GetMovementAbility().pathRadius;

						const Vec3& bodyDir = pActor->GetBodyDir();
						Vec3 right(bodyDir.y, -bodyDir.x, 0);
						right.Normalize();

						m_steeringOccupancy.AddObstructionCircle(pos, selfRad + rad);
						Vec3 vel = pActor->GetVelocity();
						if (vel.GetLengthSquared() > sqr(0.1f))
							m_steeringOccupancy.AddObstructionCircle(pos + vel*0.25f + right*rad*0.3f, selfRad + rad);
					}
				}
			}


			// Steer around
			Vec3 oldMoveDir = m_State.vMoveDir;
			m_State.vMoveDir = m_steeringOccupancy.GetNearestUnoccupiedDirection(m_State.vMoveDir, m_steeringOccupancyBias);

			if (GetAISystem()->m_cvDebugDrawCrowdControl->GetIVal() > 0)
			{
				Vec3 pos = GetPhysicsPos() + Vec3(0,0,0.75f);
				GetAISystem()->AddDebugLine(pos, pos + oldMoveDir * 2.0f, 196,0,0, 0);
				GetAISystem()->AddDebugLine(pos, pos + m_State.vMoveDir * 2.0f, 255,196,0, 0);
			}

			if (fabsf(m_steeringOccupancyBias) > 0.1f)
			{
				steering = true;

				Vec3 aheadPos;
				if (m_pPathFollower)
				{
					float junk;
					aheadPos = m_pPathFollower->GetPathPointAhead(m_movementAbility.pathRadius, junk);
					if(Distance::Point_Point2DSq(aheadPos, GetPhysicsPos()) < square(m_movementAbility.avoidanceRadius))
					{
						m_pPathFollower->Advance(m_movementAbility.pathRadius*0.3f);
					}
				}
				else
				{
					aheadPos = m_Path.CalculateTargetPos(myPos, 0.f, 0.f, m_movementAbility.pathRadius, true);
					if(!m_Path.Empty() && Distance::Point_Point2DSq(aheadPos, GetPhysicsPos()) < square(m_movementAbility.avoidanceRadius))
					{
						Vec3 pathNextPoint;
						if (m_Path.GetPosAlongPath(pathNextPoint, m_movementAbility.pathRadius*0.3f, true, false))
							m_Path.UpdatePathPosition(pathNextPoint, 100.0f, true, false);
					}
				}

				float slowdown = 1.0f - (oldMoveDir.Dot(m_State.vMoveDir) + 1.0f) * 0.5f;

				float normalSpeed, minSpeed, maxSpeed;
				GetMovementSpeedRange(m_State.fMovementUrgency, m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

				m_State.fDesiredSpeed += (minSpeed - m_State.fDesiredSpeed) * slowdown;

				if (slowdown > 0.1f)
					m_steeringAdjustTime += m_fTimePassed;
			}
			else
			{
				m_steeringAdjustTime -= m_fTimePassed;
			}

			Limit(m_steeringAdjustTime, 0.0f, 3.0f);
		}
		else
		{
			FRAME_PROFILER("NavigateAroundObjects Update Steering Old", gEnv->pSystem, PROFILE_AI)
			// Old type steering for the rest of the objects.
			unsigned nObj = m_steeringObjects.size();
			for (unsigned i = 0 ; i < nObj ; ++i)
			{
				const CAIObject *object = m_steeringObjects[i];
				if (NavigateAroundObjectsInternal(targetPos, myPos, in3D, object))
					steering = true;
			}
		}
	}

	m_steeringEnabled = steeringEnabled;

	return steering;
}

//===================================================================
// NavigateAroundAIObject
//===================================================================
bool CPuppet::NavigateAroundAIObject(const Vec3 &targetPos, const CAIObject *obstacle, const Vec3 &myPos, 
                                     const Vec3 &objectPos, bool steer, bool in3D)
{
  FUNCTION_PROFILER( GetISystem(),PROFILE_AI );
  if (steer)
  {
    if (in3D)
      return SteerAround3D(targetPos, obstacle, myPos, objectPos);
    else
      if (obstacle->GetType() == AIOBJECT_VEHICLE)
        return SteerAroundVehicle(targetPos, obstacle, myPos, objectPos);
      else
        return SteerAroundPuppet(targetPos, obstacle, myPos, objectPos);
  }
  else
  {
    AIError("NavigateAroundAIObject - only handles steering so far");
    return false;
  }
}

//===================================================================
// SteerAroundFlight
//===================================================================
bool CPuppet::SteerAroundFlight(const Vec3 &targetPos, const CAIObject *object, const Vec3 &myPos, const Vec3 &objectPos)
{
	const CAIActor* pActor = object->CastToCAIActor();
	const CPuppet* pPuppet = object->CastToCPuppet();
  if (!pActor)
    return false;

  if (pPuppet && pPuppet->m_IsSteering)
    return false;

	float avoidanceR = m_movementAbility.avoidanceRadius;
	avoidanceR += pActor->m_movementAbility.avoidanceRadius;
	float avoidanceRSq = square(avoidanceR);

  Vec3 myVel = GetVelocity();
  Vec3 otherVel = object->GetVelocity();

  if (myVel.GetLength() < 1.0f)
    return false;

  // if we are not steering already then only start if we would actually hit.
  if (!m_IsSteering)
  {
    Vec3 dir = (myVel - otherVel).GetNormalizedSafe();

    Vec3 i0, i1;
    static float checkDistScale = 3.0f;
    if (!Intersect::Lineseg_Sphere(Lineseg(myPos, myPos + dir * avoidanceR * checkDistScale), Sphere(objectPos, avoidanceR), i0, i1))
      return false;
  }

  float height, heightAbove;
  GetAISystem()->GetFlightNavRegion()->GetHeightAndSpaceAt(myPos, height, heightAbove);
  float alt = myPos.z - height;

  static float avoidanceAlt = 20.0f;
  float criticalAltForDescent = avoidanceAlt;

  bool goUp = false;
  if (alt < criticalAltForDescent)
    goUp = true;
  else
    goUp = myPos.z > objectPos.z;

  m_flightSteeringZOffset = goUp ? avoidanceAlt : -avoidanceAlt;

  return true;
}


//===================================================================
// SteerAroundPuppet
//===================================================================
bool CPuppet::SteerAroundPuppet(const Vec3 &targetPos, const CAIObject *object, const Vec3 &myPos, const Vec3 &objectPos)
{
	const CPuppet* pPuppet = object->CastToCPuppet();
	if (!pPuppet)
		return false;

  float avoidanceR = m_movementAbility.avoidanceRadius;
  avoidanceR += pPuppet->m_movementAbility.avoidanceRadius;
  float avoidanceRSq = square(avoidanceR);

  float maxAllowedSpeedMod = 10.0f;
  Vec3 steerOffset(ZERO);

  bool outside = true;
  if (m_lastNavNodeIndex && (GetAISystem()->GetGraph()->GetNodeManager().GetNode(m_lastNavNodeIndex)->navType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD)) == 0)
    outside = false;

  Vec3 delta = objectPos - myPos;
  // skip if we're not close
  float distSq = delta.GetLengthSquared();
  if (distSq > avoidanceRSq)
    return false;

  // don't overtake unless we're in more of a hurry
  static float overtakeScale = 2.0f;
  static float overtakeOffset = 0.1f;
  static float criticalOvertakeMoveDot = 0.4f;

  float holdbackDist = 0.6f;
  if (object->GetType() == AIOBJECT_VEHICLE)
    holdbackDist = 15.0f;

  if (m_State.vMoveDir.Dot(pPuppet->GetState()->vMoveDir) > criticalOvertakeMoveDot)
  {
		float myNormalSpeed, myMinSpeed, myMaxSpeed;
		GetMovementSpeedRange(m_State.fMovementUrgency, m_State.allowStrafing, myNormalSpeed, myMinSpeed, myMaxSpeed);
//    float myNormalSpeed = GetNormalMovementSpeed(m_State.fMovementUrgency, true);
//    float otherNormalSpeed = pPuppet->GetNormalMovementSpeed(pPuppet->m_State.fMovementUrgency, true);
		float otherNormalSpeed, otherMinSpeed, otherMaxSpeed;
		pPuppet->GetMovementSpeedRange(pPuppet->m_State.fMovementUrgency, pPuppet->m_State.allowStrafing, otherNormalSpeed, otherMinSpeed, otherMaxSpeed);

		if (!outside || myNormalSpeed < overtakeScale * otherNormalSpeed + overtakeOffset)
    {
			float maxDesiredSpeed = max(myMinSpeed, object->GetVelocity().Dot(m_State.vMoveDir));

      float dist = sqrt(distSq);
      dist -= holdbackDist;

      float extraFrac = dist / holdbackDist;
      Limit(extraFrac, -0.1f, 0.1f);
      maxDesiredSpeed *= 1.0f + extraFrac;

      if (m_State.fDesiredSpeed > maxDesiredSpeed)
        m_State.fDesiredSpeed = maxDesiredSpeed;

      return true;
    }
    avoidanceR *= 0.75f;
    avoidanceRSq = square(avoidanceR);
  }

  // steer around
  Vec3 aheadPos;
  if (m_pPathFollower)
  {
    float junk;
    aheadPos = m_pPathFollower->GetPathPointAhead(m_movementAbility.pathRadius, junk);
    if(outside && Distance::Point_Point2DSq(aheadPos, objectPos) < square(avoidanceR))
    {
      static float advanceFrac = 0.2f;
      m_pPathFollower->Advance(advanceFrac * avoidanceR);
    }
  }
  else
  {
    aheadPos = m_Path.CalculateTargetPos(myPos, 0.f, 0.f, m_movementAbility.pathRadius, true);
    if(outside && !m_Path.Empty() && Distance::Point_Point2DSq(aheadPos, objectPos) < square(avoidanceR))
    {
      static float advanceFrac = 0.2f;
      Vec3 pathNextPoint;
      if (m_Path.GetPosAlongPath(pathNextPoint, advanceFrac * avoidanceR, true, false))
        m_Path.UpdatePathPosition(pathNextPoint, 100.0f, true, false);
    }
  }

  Vec3	toTargetDir(objectPos - myPos);
  toTargetDir.z = 0.f;
  float toTargetDist = toTargetDir.NormalizeSafe();
  Vec3 forward2D(m_State.vMoveDir);
  forward2D.z = 0.f;
  forward2D.NormalizeSafe();
  Vec3	steerSideDir(forward2D.y, -forward2D.x, 0.f);
  // +ve when approaching, -ve when receeding
  float	toTargetDot = forward2D.Dot(toTargetDir);
  Limit(toTargetDot, 0.5f, 1.0f);

  // which way to turn
  float steerSign = steerSideDir.Dot(toTargetDir);
  steerSign = steerSign > 0.0f ? 1.0f : -1.0f;

  float toTargetDistScale = 1.0f - (toTargetDist / avoidanceR);
  Limit(toTargetDistScale, 0.0f, 1.0f);
  toTargetDistScale = sqrt(toTargetDistScale);

  Vec3 thisSteerOffset = -toTargetDistScale * steerSign * steerSideDir * toTargetDot;
  if (!outside)
    thisSteerOffset *= 0.1f;

  steerOffset += thisSteerOffset;

	float normalSpeed, minSpeed, maxSpeed;
	GetMovementSpeedRange(m_State.fMovementUrgency, m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);
//  float normalSpeed = GetNormalMovementSpeed(m_State.fMovementUrgency, true);
  if (m_State.fDesiredSpeed > maxAllowedSpeedMod * normalSpeed)
    m_State.fDesiredSpeed = maxAllowedSpeedMod * normalSpeed;

  if (!steerOffset.IsZero())
  {
    m_State.vMoveDir += steerOffset;
    m_State.vMoveDir.NormalizeSafe();
  }
	return true;
}

//===================================================================
// SteerAround3D
// Danny TODO make this 3D!!!
//===================================================================
bool CPuppet::SteerAround3D(const Vec3 &targetPos, const CAIObject *object, const Vec3 &myPos, const Vec3 &objectPos)
{
  // flight is special
  if (!m_Path.Empty() && m_Path.GetNextPathPoint()->navType == IAISystem::NAV_FLIGHT)
    return SteerAroundFlight(targetPos, object, myPos, objectPos);

  const CAIActor* pActor = object->CastToCAIActor();
  float avoidanceR = m_movementAbility.avoidanceRadius;
  avoidanceR += pActor->m_movementAbility.avoidanceRadius;
  float avoidanceRSq = square(avoidanceR);

  Vec3 delta = objectPos - myPos;
  // skip if we're not close
  float distSq = delta.GetLengthSquared();
  if (distSq > square(avoidanceR))
    return false;

  Vec3 aheadPos = m_Path.CalculateTargetPos(myPos, 0.f, 0.f, m_movementAbility.pathRadius, true);
  if(Distance::Point_Point2DSq(aheadPos, objectPos) < square(avoidanceR))
	{
    static float advanceFrac = 0.2f;
		Vec3 pathNextPoint;
    if (m_Path.GetPosAlongPath(pathNextPoint, advanceFrac * avoidanceR, true, false))
  		m_Path.UpdatePathPosition(pathNextPoint, 100.0f, true, false);
	}
	Vec3	toTargetDir(objectPos - myPos);
	toTargetDir.z=0.f;
	float toTargetDist = toTargetDir.NormalizeSafe();
	Vec3	forward2D(m_State.vMoveDir);
	forward2D.z = 0.f;
	forward2D.NormalizeSafe();
	Vec3	steerDir(forward2D.y, -forward2D.x, 0.f);

  // +ve when approaching, -ve when receeding
	float	toTargetDot = forward2D.Dot(toTargetDir);
  // which way to turn
  float steerSign = steerDir.Dot(toTargetDir);
  steerSign = steerSign > 0.0f ? 1.0f : -1.0f;

  float toTargetDistScale = 1.0f - (toTargetDist / avoidanceR);
  Limit(toTargetDistScale, 0.0f, 1.0f);
  toTargetDistScale = sqrt(toTargetDistScale);

  m_State.vMoveDir = m_State.vMoveDir - toTargetDistScale * steerSign * steerDir * toTargetDot;
	m_State.vMoveDir.NormalizeSafe();

  return true;
}

//===================================================================
// SteerAroundVehicles
//===================================================================
bool CPuppet::SteerAroundVehicle(const Vec3 &targetPos, const CAIObject *object, const Vec3 &myPos, const Vec3 &objectPos)
{
	// if vehicle is in the same formation (convoy) - don't steer around it, otherwise can't stay in formation
	if (const CAIVehicle *pVehicle = object->CastToCAIVehicle())
	{
		if(GetAISystem()->SameFormation(this, pVehicle))
			return false;
	}
  // currently puppet algorithm seems to work ok.
  return SteerAroundPuppet(targetPos, object, myPos, objectPos);
}

//===================================================================
// CreateFormation
//===================================================================
bool CPuppet::CreateFormation(const char * szName, Vec3 vTargetPos)
{
	// special handling for beacons :) It will create a formation point where the current target of the
	// puppet is.
	if (!stricmp(szName,"beacon"))
	{
		UpdateBeacon();
		return (m_pFormation!=NULL);
	}
	return CAIObject::CreateFormation(szName, vTargetPos);
}

//===================================================================
// Reset
//===================================================================
void CPuppet::Reset(EObjectResetType type)
{
#ifndef USER_dejan
  AILogComment("CPuppet::Reset %s (%p)", GetName(), this);
#endif
	CPipeUser::Reset(type);

	m_steeringOccupancy.Reset(Vec3(0,0,0), Vec3(0,1,0), 1.0f);
	m_steeringOccupancyBias = 0;
	m_steeringAdjustTime = 0;

//	m_mapDevaluedUpTargets.clear();
	m_mapDevaluedPoints.clear();

  m_steeringObjects.clear();

  ClearPotentialTargets();
	m_fLastUpdateTime = 0.0f;

	//Reset target movement tracking
	m_targetApproaching = false;
	m_targetFleeing = false;
	m_targetApproach = 0;
	m_targetFlee = 0;
	m_lastTargetValid = false;
	m_allowedStrafeDistanceStart = 0.0f;
	m_allowedStrafeDistanceEnd = 0.0f;
	m_allowStrafeLookWhileMoving = false;
	m_strafeStartDistance = 0;
	m_closeRangeStrafing = false;

	m_friendOnWayCounter = 0.0f;
	m_lastAimObstructionResult = true;
	m_updatePriority = AIPUP_LOW;

	m_adaptiveUrgencyMin = 0.0f;
	m_adaptiveUrgencyMax = 0.0f;
	m_adaptiveUrgencyScaleDownPathLen = 0.0f;
	m_adaptiveUrgencyMaxPathLen = 0.0f;

	m_delayedStance = 0;
	m_delayedStanceMovementCounter = 0;

	m_timeSinceTriggerPressed = 0.0f;
	m_friendOnWayElapsedTime = 0.0f;

	delete m_pTrackPattern;
	m_pTrackPattern = 0;

	m_territoryShape = 0;
	m_territoryShapeName.clear();

	if(m_pFireCmdHandler)
		m_pFireCmdHandler->Reset();

	m_PFBlockers.clear();

	m_fTargetAwareness = 0.0f;

	m_fireTargetCache.Reset();

	m_CurrentHideObject.Set(0, ZERO, ZERO);
	m_InitialPath.clear();

	SetAvoidedVehicle(NULL);
	//make sure i'm added to groupsMap; no problem if there already. 
	GetAISystem()->AddToGroup(this, GetGroupId());

	m_allowedToHitTarget = false;
	m_allowedToUseExpensiveAccessory = false;
	m_firingReactionTimePassed = false;
	m_firingReactionTime = 0.0f;
	m_outOfAmmoTimeOut = 0.0f;

	m_currentNavSOStates.Clear();
	m_pendingNavSOStates.Clear();

	m_targetSilhouette.Reset();
	m_targetLastMissPoint.Set(0,0,0);
	m_targetFocus = 0.0f;
	m_targetZone = ZONE_OUT;
	m_targetPosOnSilhouettePlane.Set(0,0,0);
	m_targetDistanceToSilhouette = FLT_MAX;
	m_targetBiasDirection.Set(0,0,-1);
	m_targetEscapeLastMiss = 0.0f;
	m_targetSeenTime = 0;
	m_targetLostTime = 0;
	m_targetLastMissPointSelectionTime.SetSeconds(0.0f);
	m_weaponSpinupTime = 0;

	m_alarmedTime = 0.0f;

	m_targetDamageHealthThr = -1.0f;
	m_burstEffectTime = 0.0f;
	m_burstEffectState = 0;
	m_targetDazzlingTime = 0.0f;

	if(m_targetDamageHealthThrHistory) m_targetDamageHealthThrHistory->Reset();
	m_targetHitPartIndex.clear();

	m_vForcedNavigation = ZERO;
	m_adjustpath  = 0;
	m_SeeTargetFrom = ST_HEAD;

	m_lastPuppetVisCheck = 0;
	m_LastShotsCount = 0;

	m_lastBodyDir = ZERO;
	m_bodyTurningSpeed = 0;

	GetAISystem()->NotifyEnableState(this, m_bEnabled);
}

//===================================================================
// SDynamicHidePositionNotInNavType
//===================================================================
struct SDynamicHidePositionNotInNavType
{
  SDynamicHidePositionNotInNavType(IAISystem::tNavCapMask mask) : mask(mask) {}
  bool operator()(const SHideSpot &hs)
  {
    if (hs.type != SHideSpot::HST_DYNAMIC)
      return false;
    int nBuildingID; IVisArea *pVis;
    IAISystem::ENavigationType navType = GetAISystem()->CheckNavigationType(hs.pos, nBuildingID, pVis, mask);
    return (navType & mask) == 0;
  }
  IAISystem::tNavCapMask mask;
};

//===================================================================
// IsSmartObjectHideSpot
//===================================================================
inline bool IsSmartObjectHideSpot(const std::pair<float, SHideSpot> &hsPair)
{
	return hsPair.second.type == SHideSpot::HST_SMARTOBJECT;
}

//===================================================================
// HasPointInRange
//===================================================================
static bool	HasPointInRange(const std::vector<Vec3>& list, const Vec3& pos, float range)
{
	float	r(sqr(range));
	for (unsigned i = 0, ni = list.size(); i < ni; ++i)
		if(Distance::Point_PointSq(list[i], pos) < r)
			return true;
	return false;
}

//===================================================================
// GetHidePoint
//===================================================================
Vec3 CPuppet::FindHidePoint(float searchDistance, const Vec3 &hideFrom, int nMethod,
														IAISystem::ENavigationType navType, bool bSameOk, float minDist,
														SDebugHideSpotQuery* pDebug)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	if (pDebug)
	{
		pDebug->found = false;
	}

	// reset hiding behind smart objects
	m_CurrentHideObject.ClearSmartObject();

	if (nMethod & HM_USE_LASTOP_RADIUS)
	{
		if (m_pLastOpResult && m_pLastOpResult->GetRadius() > 0.0f)
			searchDistance = m_pLastOpResult->GetRadius();
		nMethod &= ~HM_USE_LASTOP_RADIUS;
	}

	// first get all hide spots within the search distance
	static MultimapRangeHideSpots hidespots;
	static MapConstNodesDistance traversedNodes;
	hidespots.clear();
	traversedNodes.clear();

	bool skipNavigationTest = 0 != (m_movementAbility.pathfindingProperties.navCapMask & IAISystem::NAV_VOLUME);
	if ((nMethod & HM_AROUND_LASTOP) == 0)
	{
		GetAISystem()->GetHideSpotsInRange(hidespots, traversedNodes, GetPos(), searchDistance, 
			m_movementAbility.pathfindingProperties.navCapMask, GetParameters().m_fPassRadius, skipNavigationTest,
			GetEntity(), m_lastNavNodeIndex, m_lastHideNodeIndex, this);
	}
	else
	{
		if (m_pLastOpResult)
		{
			GetAISystem()->GetHideSpotsInRange(hidespots, traversedNodes, m_pLastOpResult->GetPos(), searchDistance, 
				m_movementAbility.pathfindingProperties.navCapMask, GetParameters().m_fPassRadius, skipNavigationTest,
				GetEntity(), m_lastNavNodeIndex, m_lastHideNodeIndex, this);
		}
		else
		{
			// no hiding points - NO LAST OP RESULT
			// generate signal that there is no Hiding place 
			AISignalExtraData* pData = new AISignalExtraData;
			pData->fValue = searchDistance;
			SetSignal(1, "OnNoHidingPlace", GetEntity(), pData, g_crcSignals.m_nOnNoHidingPlace);
			m_CurrentHideObject.Invalidate();
			return GetPos();
		}
		if(nMethod == HM_RANDOM_AROUND_LASTOPRESULT) 
			nMethod = HM_RANDOM;
	}


	float minDistanceSqr = sqr(minDist);
	float searchDistanceSqr = sqr(searchDistance);

	bool	bIncludeSoftCovers = (nMethod & HM_INCLUDE_SOFTCOVERS) != 0;
	bool	bIgnoreEnemies = (nMethod & HM_IGNORE_ENEMIES) != 0;
	bool	bBackwards	= (nMethod & HM_BACK) != 0;
	bool	bLastOp	= (nMethod & HM_AROUND_LASTOP) != 0;
	bool	bHideBehind	= (nMethod & HM_ON_SIDE) == 0;
	nMethod &= ~(HM_INCLUDE_SOFTCOVERS | HM_IGNORE_ENEMIES | HM_BACK | HM_AROUND_LASTOP | HM_USE_LASTOP_RADIUS | HM_ON_SIDE);

	// Collect the hide points into a list and sort them by goodness.
	// Then validate the list from the best to worst.
	static std::vector<SSortedHideSpot> sortedHideSpots;
	sortedHideSpots.clear();

	IAISystem::tNavCapMask navCapMask = GetMovementAbility().pathfindingProperties.navCapMask;
	const float passRadius = GetParameters().m_fPassRadius;

	Vec3 searchPos(bLastOp && m_pLastOpResult ? m_pLastOpResult->GetPos() : GetPos());
	const Vec3& refPointPos = m_pRefPoint->GetPos();
	Vec3 dirToRefPoint = refPointPos - searchPos;
	float distToRefPoint = dirToRefPoint.NormalizeSafe();
	Vec3 normToRefPoint(dirToRefPoint.y, -dirToRefPoint.x, 0);
	normToRefPoint.Normalize();

	Vec3	dirToTarget(hideFrom - searchPos);
	dirToTarget.z = 0;
	dirToTarget.NormalizeSafe();

	const VectorSet<CPuppet*>& enabledPuppetsSet = GetAISystem()->GetEnabledPuppetSet();

	// Colled active enemies to avoid.
	std::vector<CAIActor*> enemies;
	if (!bIgnoreEnemies)
	{
		for (VectorSet<CPuppet*>::const_iterator it = enabledPuppetsSet.begin(), itend = enabledPuppetsSet.end(); it != itend; ++it)
		{
			CPuppet* pPuppet = *it;
			if (IsHostile(pPuppet))
				enemies.push_back(pPuppet);
		}
	}

	const MultimapRangeHideSpots::iterator liEnd = hidespots.end();
	for (MultimapRangeHideSpots::iterator li = hidespots.begin(); li != liEnd; ++li)
	{
		SHideSpot &HS = li->second;
		if (bIncludeSoftCovers && HS.type == SHideSpot::HST_SMARTOBJECT)
			continue;

		if (!bIncludeSoftCovers)
		{
			if(HS.IsSecondary())
				continue;
		}

		float dot = 1.0f;
		if (!HS.dir.IsZero() )
		{
			// check that the enemy is vaguely in the dir of the hide anchor
			Vec3 dirHideToEnemy = hideFrom - HS.pos;
			dirHideToEnemy.NormalizeSafe();
			dot = dirHideToEnemy.Dot( HS.dir );
			if (dot < HIDESPOT_COVERAGE_ANGLE_COS)
				continue;
		}

		float dist = li->first;
		float distSqr = sqr(dist);

		if (HS.pObstacle)
		{
			GetAISystem()->AdjustOmniDirectionalCoverPosition(HS.pos, HS.dir, max(HS.pObstacle->fApproxRadius, 0.0f), 
				passRadius, hideFrom, bHideBehind);
		}
		// These are designer set secondary hidespots. Handle them as omni-directional points.
		if (HS.pAnchorObject && HS.pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT_SECONDARY)
		{
			GetAISystem()->AdjustOmniDirectionalCoverPosition(HS.pos, HS.dir, 0.1f, 0.0f, hideFrom);
		}
		if (HS.pNavNode && HS.pNavNode->navType == IAISystem::NAV_WAYPOINT_HUMAN &&
			HS.pNavNode->GetWaypointNavData()->type == WNT_HIDESECONDARY)
		{
			GetAISystem()->AdjustOmniDirectionalCoverPosition(HS.pos, HS.dir, 0.1f, 0.0f, hideFrom);
		}

		if (minDist > 0)
		{
			// exclude obstacles too close 
			if (nMethod == HM_NEAREST_BACKWARDS || nMethod == HM_NEAREST_TOWARDS_REFPOINT) //to self
			{
				if (distSqr < minDistanceSqr)
					continue;
			}
			else					// to target
			{
				if (Distance::Point_PointSq(HS.pos, hideFrom) < minDistanceSqr)
					continue;
			}
		}

		if (bBackwards)
		{
			if ((hideFrom - searchPos).GetNormalizedSafe().Dot((HS.pos - searchPos).GetNormalizedSafe()) > 0.2f) 
				continue;
		}

		bool indoor = HS.pNavNode && (HS.pNavNode->navType == IAISystem::NAV_WAYPOINT_HUMAN || HS.pNavNode->navType == IAISystem::NAV_WAYPOINT_3DSURFACE);

		// check if there are enemies around
		float avoidEnemiesCoeff = 1.0f;
		if (!bIgnoreEnemies)
		{
			for (unsigned i = 0, ni = enemies.size(); i < ni; ++i)
			{
				const CAIActor* pFoeActor = enemies[i];
				float safeDist = indoor ? pFoeActor->GetParameters().m_fDistanceToHideFrom/2.0f : pFoeActor->GetParameters().m_fDistanceToHideFrom;
				float distFoeSqr = Distance::Point_PointSq(HS.pos, pFoeActor->GetPos());
				if (distFoeSqr < sqr(safeDist))
					avoidEnemiesCoeff *= sqrtf(distFoeSqr) / safeDist;
			}
		}

		// Compromising.
		if (!indoor && nMethod != HM_RANDOM)
		{
			// allow him to use only the hidespots closer to him than to the enemy
			Vec3 dirHideToEnemy = hideFrom - HS.pos;
			Vec3 dirHide = HS.pos - searchPos;
			if (dirHide.GetLengthSquared() > dirHideToEnemy.GetLengthSquared())
				continue;
		}

		// Calculate the weight (goodness) of the cover point.
		float weight = 0;

		switch (nMethod)
		{
		case HM_RANDOM:
			{
				weight = ai_frand() + avoidEnemiesCoeff;
			}
			break;
		case HM_NEAREST:
		case HM_NEAREST_TO_ME:
			{
				float directDist = Distance::Point_Point(HS.pos, nMethod==HM_NEAREST? searchPos: GetPos());
				weight = (searchDistance - directDist) * avoidEnemiesCoeff * ((3+dot)/4);
			}
			break;

		case HM_NEAREST_TOWARDS_REFPOINT:
			{
				const float rad = min(searchDistance/2, distToRefPoint);
				Vec3 diff = HS.pos - (searchPos + dirToRefPoint * rad);
				float u = normToRefPoint.Dot(diff) * 2.0f;
				float v = dirToRefPoint.Dot(diff);
				float dd = sqr(u) + sqr(v);
				if (dd > sqr(rad))
					continue;
				float val = sqr(distToRefPoint) - dd;
				val *= avoidEnemiesCoeff;
				weight = val;
			}
			break;

		case HM_NEAREST_TOWARDS_TARGET_PREFER_SIDES:
		case HM_NEAREST_TOWARDS_TARGET_LEFT_PREFER_SIDES:
		case HM_NEAREST_TOWARDS_TARGET_RIGHT_PREFER_SIDES:
			{
				Vec3	ax(-dirToTarget.y, dirToTarget.x, 0);
				Vec3	ay(dirToTarget);
				Vec3	diff(HS.pos - searchPos);
				float	x = ax.Dot(diff) * 0.5f;	// prefer sides
				float	y = ay.Dot(diff);
				// Skip points which are behind the agent.
				if(y < -1.0f)
					continue;
				// Skip points at right side.
				if(nMethod == HM_NEAREST_TOWARDS_TARGET_LEFT_PREFER_SIDES && x > 1.0f)
					continue;
				// Skip points at left side.
				if(nMethod == HM_NEAREST_TOWARDS_TARGET_RIGHT_PREFER_SIDES && x < -1.0f)
					continue;
				weight = (searchDistance - sqrtf(x*x + y*y)) * avoidEnemiesCoeff;
			}
			break;

		case HM_NEAREST_PREFER_SIDES:
			{
				Vec3	ax(-dirToTarget.y, dirToTarget.x, 0);
				Vec3	ay(dirToTarget);
				Vec3	diff(HS.pos - searchPos);
				float	x = ax.Dot(diff) * 0.5f;	// prefer sides
				float	y = ay.Dot(diff);
				weight = (searchDistance - sqrtf(x*x + y*y)) * avoidEnemiesCoeff * ((3+dot)/4);
			}
			break;

		case HM_NEAREST_TO_TARGET:
		case HM_NEAREST_TOWARDS_TARGET:
			{
				Vec3 one = hideFrom-searchPos;
				Vec3	two = HS.pos-searchPos;
				float distanceObstacle_hideFrom = (HS.pos-hideFrom).GetLength();
				weight = (20000 - distanceObstacle_hideFrom) * avoidEnemiesCoeff;
				if (one.Dot(two) <= 0 || (nMethod==HM_NEAREST_TO_TARGET && distanceObstacle_hideFrom > one.GetLength()))
					continue;
			}
			break;
		case HM_FARTHEST_FROM_TARGET:
			{
				Vec3 one = hideFrom-searchPos;
				Vec3	two = HS.pos-searchPos;
				if (one.Dot(two)>0)
					continue;
				weight = ((HS.pos-hideFrom).len2()) * avoidEnemiesCoeff * ((3+dot)/4);
			}
			break;
		case HM_NEAREST_BACKWARDS:
			{
				Vec3 one = hideFrom-searchPos;
				Vec3	two = HS.pos-searchPos;
				weight = (2000.f - dist) * avoidEnemiesCoeff;
				if( !m_movementAbility.b3DMove )
					one.z = two.z = .0f;
				one.normalize();
				two.normalize();
				if (one.Dot(two)>-0.7f)
					continue;
			}
			break;
		case HM_LEFTMOST_FROM_TARGET:
			{
				Vec3 one = hideFrom-searchPos;
				Vec3	two = HS.pos-searchPos;
				float zcross = one.x*two.y - one.y*two.x;
				zcross = (2000 + zcross) * avoidEnemiesCoeff * ((3+dot)/4);
				weight = zcross;
			}
			break;
		case HM_FRONTLEFTMOST_FROM_TARGET:
			{
				Vec3 one = hideFrom - searchPos;
				Vec3	two = HS.pos - searchPos;
				float zcross = one.x*two.y - one.y*two.x;
				one.Normalize();
				two.Normalize();
				float f = one.Dot(two);
				if (f<0.2)
					continue;
				weight = (2000 + zcross) * avoidEnemiesCoeff * ((3+dot)/4);
			}
			break;
		case HM_RIGHTMOST_FROM_TARGET:
			{
				Vec3 one = hideFrom-searchPos;
				Vec3	two = HS.pos-searchPos;
				float zcross = one.x*two.y - one.y*two.x;
				weight = (2000 - zcross) * avoidEnemiesCoeff * ((3+dot)/4);
			}
			break;
		case HM_FRONTRIGHTMOST_FROM_TARGET:
			{
				Vec3 one = hideFrom - searchPos;
				Vec3	two = HS.pos - searchPos;
				float zcross = one.x*two.y - one.y*two.x;
				one.Normalize();
				two.Normalize();
				float f = one.Dot(two);
				if (f<0.2)
					continue;
				weight = (2000 - zcross) * avoidEnemiesCoeff * ((3+dot)/4);
			}
			break;
			// add more here as needed
		}

		sortedHideSpots.push_back(SSortedHideSpot(weight, &HS));
	}

	// Sort the hidespots and start validating them starting from the best match.
	std::sort(sortedHideSpots.begin(), sortedHideSpots.end());

	if (pDebug)
	{
		for (unsigned i = 0, ni = sortedHideSpots.size(); i < ni; ++i)
		{
			SSortedHideSpot& spot = sortedHideSpots[i];
			SHideSpot &HS = *spot.pHideSpot;
			pDebug->spots.push_back(SDebugHideSpotQuery::SHideSpot(HS.pos, HS.dir, spot.weight));
		}
	}

	bool bCheckForSameObject(!bSameOk &&
		nMethod != HM_NEAREST &&
		nMethod != HM_NEAREST_PREFER_SIDES &&
		nMethod != HM_RANDOM);
	if (nMethod == HM_NEAREST_TOWARDS_REFPOINT)
		bCheckForSameObject = true;

	Vec3 playerPos(ZERO);
	bool bPlayerHostile = false;
	CAIObject* pPlayer = GetAISystem()->GetPlayer();
	if(pPlayer)
	{
		playerPos = (pPlayer->GetEntity()? pPlayer->GetEntity()->GetPos(): pPlayer->GetPos());
		bPlayerHostile = IsHostile(pPlayer);
	}

	// Setup territory check.
	// Territory describes an area where the hidespots should be limited to.
	SShape*	territoryShape = GetTerritoryShape();

	const CPathObstacles &pathAdjustmentObstacles = GetPathAdjustmentObstacles();

	// Check if dead bodies should be avoided.
	const unsigned maxDeadBodySpots = 3;
	Vec3	deadBodySpots[maxDeadBodySpots];
	unsigned	nDeadBodySpots = 0;
	float			deadBodyBlockerDistSq = 0.0f;
	// Check if the dead body navigation blocker is set to disable navigation around dead bodies.
	TMapBlockers::const_iterator itr(m_PFBlockers.find(PFB_DEAD_BODIES));
	if(itr != m_PFBlockers.end() && (*itr).second < -0.01f)
	{
		nDeadBodySpots = GetAISystem()->GetDangerSpots(static_cast<const IAIObject*>(this), 40.0f, deadBodySpots, 0, maxDeadBodySpots, IAISystem::DANGER_DEADBODY);
		deadBodyBlockerDistSq = sqr(-(*itr).second);
	}

	// Collect list of occupied hide object positions.
	std::vector<Vec3>	occupiedSpots;
	GetAISystem()->GetOccupiedHideObjectPositions(this, occupiedSpots);

	AIAssert(GetProxy());
	SAIBodyInfo bi;
	GetProxy()->QueryBodyInfo(STANCE_STAND, 0.0f, true, bi);

	for (unsigned i = 0, ni = sortedHideSpots.size(); i < ni; ++i)
	{
		SHideSpot &HS = *sortedHideSpots[i].pHideSpot;

		// check distance of player to the obstacle
		if (pPlayer)
		{
			IAISystem::ENavigationType obstacleNavType;
			bool b2D = !HS.pNavNode || (obstacleNavType= HS.pNavNode->navType) != IAISystem::NAV_WAYPOINT_3DSURFACE && 
				obstacleNavType != IAISystem::NAV_VOLUME && obstacleNavType != IAISystem::NAV_FLIGHT; 
			float distPlayerSqr =	b2D?	Distance::Point_Point2DSq(HS.pos,playerPos): Distance::Point_PointSq(HS.pos,playerPos);
			float thr(0);
			if (HS.pNavNode && HS.pNavNode->navType == IAISystem::NAV_WAYPOINT_HUMAN || !HS.pObstacle )
				thr = 1.2f;
			else 
				thr =  HS.pObstacle->fApproxRadius+1.5f;
			if (distPlayerSqr < sqr(thr))//check if player is close to the obstacle
			{
				//for player's friends, check if player is behind the obstacle, 
				// and discard it unless the puppet is searching right around the player
				if(bPlayerHostile) 
					continue;
				else if (!(bLastOp && m_pLastOpResult == pPlayer) && (hideFrom - HS.pos).Dot(playerPos - HS.pos) <0)
					continue;
			}
		}

		// If if the same hide object should not be used.
		if (bCheckForSameObject && IsEquivalent(m_CurrentHideObject.GetObjectPos(), HS.objPos))
			continue;

		// Skip points outside territory.
		if (territoryShape && !territoryShape->IsPointInsideShape(HS.pos, true))
			continue;

		// Skip used hide objects.
		if (HasPointInRange(occupiedSpots, HS.objPos, 0.5f))
			continue;

		// Skip recent unreachable hidespots.
		if (WasHideObjectRecentlyUnreachable(HS.objPos))
			continue;

		// Skip hidespots next to dead bodies, if that is to be avoided.
		if (nDeadBodySpots)
		{
			bool	closeToDeadBody(false);
			for (unsigned j = 0; j < nDeadBodySpots; ++j)
			{
				if (Distance::Point_PointSq(deadBodySpots[j], HS.pos) < deadBodyBlockerDistSq)
				{
					closeToDeadBody = true;
					break;
				}
			}
			if (closeToDeadBody)
				continue;
		}

		// skip points close to explosion craters
		if (GetAISystem()->IsPointAffectedByExplosion(HS.pos))
			continue;

		if (HS.pObstacle)
		{
			// Discard the point if it is inside another vegetation obstacle.
			bool	insideObstacle(false);
			for (MultimapRangeHideSpots::iterator li = hidespots.begin(); li != hidespots.end(); ++li)
			{
				const ObstacleData* pOtherObstacle = li->second.pObstacle;
				if (pOtherObstacle == HS.pObstacle)
					continue;
				if (!pOtherObstacle || !pOtherObstacle->bCollidable)
					continue;
				if(Distance::Point_Point2DSq(HS.pos, pOtherObstacle->vPos) < sqr(max(pOtherObstacle->fApproxRadius, 0.0f)))
				{
					insideObstacle = true;
					break;
				}
			}
			if(insideObstacle)
				continue;
		}

		if (HS.pNavNode && HS.pNavNode->navType == IAISystem::NAV_TRIANGULAR)
		{
			if (GetAISystem()->IsPointForbidden(HS.pos, passRadius, 0, 0))
				continue;
			if (pathAdjustmentObstacles.IsPointInsideObstacles(HS.pos))
				continue;
		}

		// The spot is valid, use it!
		// Move the spot away from the wall.
		if (HS.type == SHideSpot::HST_ANCHOR || HS.type == SHideSpot::HST_WAYPOINT)
		{
			if ((HS.pAnchorObject && HS.pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT) ||
					(HS.pNavNode && HS.pNavNode->navType == IAISystem::NAV_WAYPOINT_HUMAN &&
					HS.pNavNode->GetWaypointNavData()->type == WNT_HIDE))
			{
				GetAISystem()->AdjustDirectionalCoverPosition(HS.pos, HS.dir, passRadius, 0.75f);
			}
		}

		// Do final physics validation for dynamic hide points.
		if (HS.type == SHideSpot::HST_DYNAMIC)
		{
			if (!GetAISystem()->GetDynHideObjectManager()->ValidateHideSpotLocation(HS.pos, bi, HS.entityId))
				continue;
		}

		m_CurrentHideObject.Set(&HS, HS.pos, HS.dir);

		//	GetAISystem()->AddDebugSphere(retPoint+Vec3(0,0,1), 0.5f,  255,0,0, 8.0f);

		if (HS.type == SHideSpot::HST_SMARTOBJECT )
			m_CurrentHideObject.SetSmartObject( HS.SOQueryEvent );

		if (pDebug)
		{
			pDebug->hidePos = HS.pos;
			pDebug->hideDir = HS.dir;
			pDebug->found = true;
		}

		return HS.pos;	
	}

	// no hiding points - not with your specified filter or range
	// generate signal that there is no Hiding place 
	AISignalExtraData* pData = new AISignalExtraData;
	pData->fValue = searchDistance;
	SetSignal(1,"OnNoHidingPlace",GetEntity(),pData, g_crcSignals.m_nOnNoHidingPlace);

	// Invalidate the current hidepoint if not close to it.
	//if(bCheckForSameObject || !m_CurrentHideObject.IsNearCover(this))
	m_CurrentHideObject.Invalidate();

	return GetPos();
}


//====================================================================
// ResetMiss
//====================================================================
void CPuppet::ResetMiss( )
{
	m_LastMissPoint.zero();
}


//====================================================================
// GetAccuracy
//	calculate current accuracy - account for distance, target stance, ...
//	at close range always hit, at attackRange always miss
//	if accuracy in properties is 1 - ALWAYS hit
//	if accuracy in properties is 0 - ALWAYS miss
//====================================================================
float CPuppet::GetAccuracy(const CAIObject *pTarget) const 
{
	//---------parameters------------------------------------------
	const static float	absoluteAccurateTrh(5.f);	// if closer than this - always hit
	const static float	accurateTrhSlope(.3f);		// slop from aways-hit to nominal accuracy
	const static float	nominalAccuracyStartAt(.3f);		// at what attack range nominal accuracy is on
	const static float	nominalAccuracyStopAt(.73f);		// at what attack range nominal accuracy is starting to fade to 0
	//-------------------------------------------------------------

	float	unscaleAccuracy = GetParameters().m_fAccuracy;
	float	curAccuracy = unscaleAccuracy;
	if(curAccuracy <= 0.00001f)
		return 0.f;

	if(!IsAllowedToHitTarget())
		curAccuracy *= 0.0f;

	if(!pTarget)
		return curAccuracy;

	const CAIActor* pTargetActor = pTarget->CastToCAIActor();

	curAccuracy = max(0.f, curAccuracy - m_fAccuracySupressor);	// account for soft cover
	//	curAccuracy -= m_fAccuracyMotionAddition;	// account for movement
	float	distance((pTarget->GetPos()-GetPos()).len());

	if(distance<absoluteAccurateTrh)	// too close - always hit
		return 1.f;
	if(distance>=GetParameters().m_fAttackRange)	// too far - always miss
		return 0.f;
	// at what DISTANCE nominal accuracy is on
	float	nominalAccuracyStartDistance(max(absoluteAccurateTrh+1.f, GetParameters().m_fAttackRange*nominalAccuracyStartAt));
	// at what DISTANCE nominal accuracy is starting to fade to 0
	float	nominalAccuracyStopDistance(min(GetParameters().m_fAttackRange-.1f, GetParameters().m_fAttackRange*nominalAccuracyStopAt));

	// scale down accuracy if target prones 
	if(pTargetActor && pTargetActor->GetProxy())
	{
		SAIBodyInfo	targetBodyInfo;
		pTarget->GetProxy()->QueryBodyInfo(targetBodyInfo);
		if(targetBodyInfo.stance == STANCE_STAND)
			curAccuracy *= 1.0f;
		else if(targetBodyInfo.stance == STANCE_CROUCH)
			curAccuracy *= 0.8f;
		else if(targetBodyInfo.stance == STANCE_PRONE)
			curAccuracy *= 0.3f;
	}

	// scale down accuracy if shooter moves
	if(GetPhysics())
	{
		static pe_status_dynamics  dSt;
		GetPhysics()->GetStatus( &dSt );
		float fSpeed= dSt.v.GetLength();
		if(fSpeed > 1.0f)
		{
			if(fSpeed > 5.0f)
				fSpeed = 5.0f;
			if(IsCoverFireEnabled())
				fSpeed /=2.f;
			curAccuracy *= 3.f/(3.f +fSpeed);
		}
	}
	
	if(distance<nominalAccuracyStartDistance)	// 1->nominal interpolation
	{
		float slop((1.f-curAccuracy)/(nominalAccuracyStartDistance-absoluteAccurateTrh));
		float scaledAccuracy(curAccuracy + slop*(nominalAccuracyStartDistance - (distance-absoluteAccurateTrh)));
		return scaledAccuracy;
	}
	if(distance>nominalAccuracyStopDistance)	// nominal->0 interpolation
	{
		float slop((curAccuracy)/(GetParameters().m_fAttackRange-nominalAccuracyStopDistance));
		float scaledAccuracy(slop*(GetParameters().m_fAttackRange-distance));
		return scaledAccuracy;
	}
	return curAccuracy;
}

//====================================================================
// GetFireTargetObject
//====================================================================
CAIObject* CPuppet::GetFireTargetObject() const
{
	if (m_pFireTarget)
		return m_pFireTarget;
	return m_pAttentionTarget;
}

//====================================================================
// CanDamageTarget
//====================================================================
bool CPuppet::CanDamageTarget()
{
	// Allow to hit always when requested kill fire mode.
	if (m_fireMode == FIREMODE_KILL)
		return true;
	if (!m_firingReactionTimePassed)
		return false;
	if (m_targetDazzlingTime > 0.0f)
		return false;
	// Never hit when in panic spread fire mode.
	if (m_fireMode == FIREMODE_PANIC_SPREAD)
		return false;

	if (m_Parameters.m_fAccuracy < 0.001f)
		return false;

	const CAIObject* pLiveTarget = GetLiveTarget(m_pAttentionTarget);
	if (!pLiveTarget || !pLiveTarget->GetProxy())
		return false;

	// If the target is at low health, allow short time of mercy for it.
	if (const CAIActor* pLiveTargetActor = pLiveTarget->CastToCAIActor())
		if (pLiveTargetActor->IsLowHealthPauseActive())
			return false;

	int maxHealth = pLiveTarget->GetProxy()->GetActorMaxHealth();
	int health = pLiveTarget->GetProxy()->GetActorHealth() + pLiveTarget->GetProxy()->GetActorArmor();

	int thr = (int)(m_targetDamageHealthThr * maxHealth);
	return thr > 0 && health >= thr;
}

//====================================================================
// ResetTargetTracking
//====================================================================
void CPuppet::ResetTargetTracking()
{
	m_targetLastMissPoint.Set(0,0,0);
	m_targetFocus = 0.0f;
	m_targetEscapeLastMiss = 0.0f;

	if(m_targetSilhouette.valid)
		m_targetSilhouette.Reset();
}

//====================================================================
// GetFiringReactionTime
//====================================================================
float CPuppet::GetFiringReactionTime(const Vec3& targetPos) const
{
//	float reactionTime = m_Parameters.m_fShootingReactionTime;
//	if(GetAISystem()->m_cvRODReactionTime->GetFVal() > 0.001f)
//		reactionTime = GetAISystem()->m_cvRODReactionTime->GetFVal();
	float reactionTime = GetAISystem()->m_cvRODReactionTime->GetFVal();

	if (IsAffectedByLightLevel())
	{
		EAILightLevel targetLightLevel = GetAISystem()->GetLightManager()->GetLightLevelAt(targetPos);
		switch (targetLightLevel)
		{
		case AILL_MEDIUM: reactionTime += GetAISystem()->m_cvRODReactionMediumIllumInc->GetFVal(); break;
		case AILL_DARK: reactionTime += GetAISystem()->m_cvRODReactionDarkIllumInc->GetFVal(); break;
		}
	}

	const float distInc = min(1.0f, GetAISystem()->m_cvRODReactionDistInc->GetFVal());
	// Increase reaction time if the target is further away.
	if (m_targetZone == ZONE_COMBAT_NEAR)
		reactionTime += distInc;
	else if (m_targetZone >= ZONE_COMBAT_FAR)
		reactionTime += distInc*2.0f;

	// Increase the reaction time of the target is leaning.
	const CAIActor* pLiveTarget = CastToCAIActorSafe(GetLiveTarget(m_pAttentionTarget));
	if (pLiveTarget)
	{
		SAIBodyInfo bi;
		if (pLiveTarget->GetProxy())
		{
			pLiveTarget->GetProxy()->QueryBodyInfo(bi);
			if (fabsf(bi.lean) > 0.01f)
			{
				reactionTime += GetAISystem()->m_cvRODReactionLeanInc->GetFVal();
			}
		}

		Vec3 dirTargetToShooter = GetPos() - pLiveTarget->GetPos();
		dirTargetToShooter.Normalize();
		float dot = pLiveTarget->GetViewDir().Dot(dirTargetToShooter);

		const float thr1 = cosf(DEG2RAD(30.0f));
		const float thr2 = cosf(DEG2RAD(95.0f));
		if (dot < thr2)
			reactionTime += GetAISystem()->m_cvRODReactionDirInc->GetFVal();
		else if (dot < thr1)
			reactionTime += GetAISystem()->m_cvRODReactionDirInc->GetFVal()*2.0f;
	}

	return reactionTime;
}

//====================================================================
// UpdateTargetTracking
//====================================================================
Vec3 CPuppet::UpdateTargetTracking(CAIObject* pTarget, const Vec3& targetPos)
{
	const float	dt = GetAISystem()->GetFrameDeltaTime();
	const float reactionTime = GetFiringReactionTime(targetPos);

	if (!IsAllowedToHitTarget() || !AllowedToFire())
	{
		m_firingReactionTime = max(0.0f, m_firingReactionTime - dt);
	}
	else
	{
		if (GetAttentionTargetType() == AITARGET_VISUAL && GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
			m_firingReactionTime = min(m_firingReactionTime + dt, reactionTime+0.001f);
		else
			m_firingReactionTime = max(0.0f, m_firingReactionTime - dt);
	}

	m_firingReactionTimePassed = m_firingReactionTime >= reactionTime;

	const CAIObject* pLiveTarget = GetLiveTarget(pTarget);
	if (!pLiveTarget)
	{
		ResetTargetTracking();
		m_targetBiasDirection += (Vec3(0,0,-1) - m_targetBiasDirection) * dt;
		return targetPos;
	}

	const float killRange = GetAISystem()->m_cvRODKillRangeMod->GetFVal();
	const float combatRange = GetAISystem()->m_cvRODCombatRangeMod->GetFVal();

	float	distToTargetSqr = Distance::Point_PointSq(GetPos(), pTarget->GetPos());
	if (distToTargetSqr < sqr(m_Parameters.m_fAttackRange * killRange))
		m_targetZone = ZONE_KILL;
	else if (distToTargetSqr < sqr(m_Parameters.m_fAttackRange * (combatRange+killRange)/2))
		m_targetZone = ZONE_COMBAT_NEAR;
	else if (distToTargetSqr < sqr(m_Parameters.m_fAttackRange * combatRange))
		m_targetZone = ZONE_COMBAT_FAR;
	else if (distToTargetSqr < sqr(m_Parameters.m_fAttackRange))
		m_targetZone = ZONE_WARN;
	else
		m_targetZone = ZONE_OUT;

	float	focusTargetValue = 0.0f;
	focusTargetValue += pLiveTarget->GetVelocity().GetLength() / 3.0f;
	focusTargetValue += m_targetEscapeLastMiss;
	if (m_targetZone >= ZONE_WARN)
		focusTargetValue += 1.0f;
	Limit(focusTargetValue, 0.0f, 1.0f);
	if (focusTargetValue > m_targetFocus)
		m_targetFocus += (focusTargetValue - m_targetFocus) * dt;
	else
		m_targetFocus += (focusTargetValue - m_targetFocus) * 0.4f * dt;

//	if (m_timeSinceTriggerPressed < 0.2f)
//		m_targetEscapeLastMiss = clamp(m_targetEscapeLastMiss + 0.05f, 0.0f, 1.0f);
//	else
//		m_targetEscapeLastMiss = clamp(m_targetEscapeLastMiss - 0.1f, 0.0f, 1.0f);


	// Calculate a silhouette which is later used to miss the player intentionally.
	// The silhouette is the current target AABB extruded into the movement direction.
	// The silhouette exists on a plane which separates the shooter and the target.
	if (!m_bDryUpdate || !m_targetSilhouette.valid)
	{
		m_targetSilhouette.valid = true;
		
		AIAssert(pLiveTarget->GetProxy());

		// Calculate the current and predicted AABB of the target.
		const float MISS_PREDICTION_TIME = 0.8f;

		AABB aabbCur, aabbNext;

		IPhysicalEntity* pPhys = pLiveTarget->GetProxy()->GetPhysics(true);
		if (!pPhys)
			pPhys = pLiveTarget->GetProxy()->GetPhysics();

		if (!pPhys)
		{
			AIWarning("CPuppet::UpdateTargetTracking() Puppet %s does not have physics!", pLiveTarget->GetName());
			AIAssert(0);
			ResetTargetTracking();
			m_targetBiasDirection += (Vec3(0,0,-1) - m_targetBiasDirection) * dt;
			return targetPos;
		}

		pe_status_pos statusPos;
		pPhys->GetStatus(&statusPos);
		aabbCur.Reset();
		aabbCur.Add(statusPos.BBox[0] + statusPos.pos);
		aabbCur.Add(statusPos.BBox[1] + statusPos.pos);
		aabbNext = aabbCur;
		aabbCur.min.z -= 0.05f;	// This adjustment moves any ground effect slightly infront of the target.
		aabbNext.min.z -= 0.05f;
		aabbNext.Move(pLiveTarget->GetVelocity() * MISS_PREDICTION_TIME);

		// Create a list of points which is used to calculate the silhuette.
		Vec3	points[16];
		SetAABBCornerPoints(aabbCur, &points[0]);
		SetAABBCornerPoints(aabbNext, &points[8]);

		m_targetSilhouette.center = aabbCur.GetCenter();

		// Project points on a plane between the shooter and the target.
		Vec3	dir = m_targetSilhouette.center - GetPos();
		dir.NormalizeSafe();
		m_targetSilhouette.baseMtx.SetRotationVDir(dir, 0.0f);

		const Vec3& u = m_targetSilhouette.baseMtx.GetColumn0();
		const Vec3& v = m_targetSilhouette.baseMtx.GetColumn2();

		static std::vector<Vec3>	projectedPoints(16);
		for (unsigned i = 0; i < 16; ++i)
		{
			Vec3	pt = points[i] - m_targetSilhouette.center;
			projectedPoints[i].Set(u.Dot(pt), v.Dot(pt), 0.0f);
		}

		// The silhouette is the convex hull of all the points in the two AABBs.
		m_targetSilhouette.points.clear();
		ConvexHull2D(m_targetSilhouette.points, projectedPoints);
	}

	// Calculate a direction that is used to calculate the miss points on the silhouette.
	Vec3	desiredTargetBias(0,0,0);

	// 1) Bend the direction towards the target movement direction.
	if (pLiveTarget)
		desiredTargetBias += pLiveTarget->GetVelocity().GetNormalizedSafe(Vec3(0,0,0));

	// 2) Bend the direction towards the point that is visible to the shooter.
	// Note: If the target is visible targetPos==m_targetSilhouette.center.
	desiredTargetBias += ((targetPos - m_targetSilhouette.center) / 2.0f) * (1 - m_targetEscapeLastMiss);

	// 3) Bend the direction towards ground if not trying to adjust the away from obstructed area.
	desiredTargetBias.z -= 0.1f + 0.5f * (1 - m_targetEscapeLastMiss);

	// 4) If the current aim is obstructed, try to climb the silhouette to the other side.
	if (m_targetEscapeLastMiss > 0.0f && !m_targetLastMissPoint.IsZero())
	{
		Vec3 deltaProj = m_targetSilhouette.ProjectVectorOnSilhouette(m_targetBiasDirection);
		deltaProj.NormalizeSafe();
		if (!deltaProj.IsZero())
		{
			float lastMissAngle = atan2f(deltaProj.y, deltaProj.x);

			const Vec3& u = m_targetSilhouette.baseMtx.GetColumn0();
			const Vec3& v = m_targetSilhouette.baseMtx.GetColumn2();

			// Choose the climb direction based on the current side
			float a = u.Dot(m_targetBiasDirection) < 0.0f ? -gf_PI/2 : gf_PI/2;

			float	x = cosf(lastMissAngle+a);
			float	y = sinf(lastMissAngle+a);

			desiredTargetBias += (u*x + v*y) * m_targetEscapeLastMiss;
		}
	}

	if (desiredTargetBias.NormalizeSafe(ZERO) > 0.0f)
	{
		m_targetBiasDirection += (desiredTargetBias - m_targetBiasDirection) * 4.0f * dt;
		m_targetBiasDirection.NormalizeSafe(ZERO);
	}

	m_targetPosOnSilhouettePlane = m_targetSilhouette.IntersectSilhouettePlane(GetFirePos(), targetPos);
	Vec3	targetPosOnSilhouettePlaneProj = m_targetSilhouette.ProjectVectorOnSilhouette(m_targetPosOnSilhouettePlane - m_targetSilhouette.center);

	// Calculate the distance between the target pos and the silhouette.
	if (Overlap::Point_Polygon2D(targetPosOnSilhouettePlaneProj, m_targetSilhouette.points))
	{
		// Inside the polygon, zero dist.
		m_targetDistanceToSilhouette = 0.0f;
	}
	else
	{
		// Distance to the nearest edge.
		Vec3	pt;
		m_targetDistanceToSilhouette = Distance::Point_Polygon2D(targetPosOnSilhouettePlaneProj, m_targetSilhouette.points, pt);
	}

//	if (m_targetDistanceToSilhouette < 0.5f)
//		return targetPos + m_targetBiasDirection * 0.5f;

	return targetPos;
}


//====================================================================
// FireCommand
//====================================================================
void CPuppet::FireCommand()
{
	FUNCTION_PROFILER(GetAISystem()->m_pSystem, PROFILE_AI);
	
	if (m_fSuppressFiring > 0.0f)
		m_fSuppressFiring = max(0.0f, m_fSuppressFiring - GetAISystem()->GetFrameDeltaTime());

	m_timeSinceTriggerPressed += m_fTimePassed;

	if(m_pAttentionTarget && m_pAttentionTarget->IsAgent())
		m_lastLiveTargetPos = m_pAttentionTarget->GetPos();

	bool	fireSupressed(m_fSuppressFiring > 0.0f);

	bool	lastAim = m_State.aimTargetIsValid;

	// Reset aiming and shooting, they will be set in to code below if needed.
	m_State.aimTargetIsValid = false;
	m_State.fire = false;
	m_State.fireSecondary = false;
	m_State.fireMelee = false;

	// Choose target to shoot at.
	CAIObject*	pTarget = m_pAttentionTarget;
	if (m_pFireTarget)
		pTarget = m_pFireTarget;

	if (m_fireMode == FIREMODE_SECONDARY ||
			m_fireMode == FIREMODE_SECONDARY_SMOKE )
	{
		if (m_fireModeUpdated)
		{
			m_pFireCmdGrenade->Reset();
			m_fireModeUpdated = false;
		}
		if (m_fireMode == FIREMODE_SECONDARY_SMOKE && 
			m_pLastOpResult)
			pTarget = m_pLastOpResult;
		if (pTarget)
		{
			m_State.vAimTargetPos = pTarget->GetPos();
			m_State.aimTargetIsValid = true;
			m_aimState = AI_AIM_READY;
			FireSecondary(pTarget);
		}
		return;
	}
	if (m_fireMode == FIREMODE_MELEE || m_fireMode == FIREMODE_MELEE_FORCED)
	{
		if (pTarget)
		{
			// Do not aim, use look only.
			m_State.vAimTargetPos = pTarget->GetPos();
			m_State.aimTargetIsValid = false;
			m_aimState = AI_AIM_NONE; // AI_AIM_READY;
			FireMelee(pTarget);
		}
		return;
	}

	if (!m_pFireCmdHandler)
		return;

	if (m_fireMode == FIREMODE_OFF)
	{
		m_friendOnWayElapsedTime = 0.0f;
	}

	bool	targetValid = false;
	Vec3	aimTarget(0,0,0);

	// Check if the target is valid based on the current fire mode.
	if (m_fireMode == FIREMODE_AIM || m_fireMode == FIREMODE_AIM_SWEEP ||
			m_fireMode == FIREMODE_FORCED || m_fireMode == FIREMODE_PANIC_SPREAD)
	{
		// Aiming and forced fire has the loosest checking.
		// As long as the target exists, it can be used.
		if (pTarget)
			targetValid = true;
	}
	else if (m_fireMode != FIREMODE_OFF)
	{
		// The rest of the first modes require that the target exists and is an agent or a vehicle with player inside.
		// The IsActive() handles both cases.
		if (pTarget)
		{
			if (pTarget->GetSubType() == IAIObject::STP_MEMORY)
			{
				targetValid = true;
			}
			else
			{
				CAIActor* pTargetActor = pTarget->CastToCAIActor();
				if (pTargetActor && pTargetActor->IsActive())
					targetValid = true;
			}
		}
	}

	// The loose attention target is more important than the normal target.
	if (m_bLooseAttention && pTarget != m_pLooseAttentionTarget && GetSubType() != CAIObject::STP_2D_FLY )
		targetValid = false;

	bool	canFire = targetValid && AllowedToFire();
	float distToTargetSqr = FLT_MAX;
	if (canFire)
	{
		if (fireSupressed)
			canFire = false;
		if (pTarget)
		{
			// Allow forced fire even when outside the specified range.
			if(m_fireMode != FIREMODE_FORCED)
			{
				// Check if within fire range
				distToTargetSqr = Distance::Point_PointSq(pTarget->GetPos(), GetFirePos());
				if (distToTargetSqr > sqr(m_Parameters.m_fAttackRange))
				{
					// Out of range.
					canFire = false;

					bool bIsCrysis;
					gEnv->pAISystem->GetConstant(IAISystem::eAI_CONST_bIsCrysis1, bIsCrysis);
					if (bIsCrysis)
						targetValid = false;
				}
			}
		}
	}

	bool useLiveTargetForMemory = m_targetLostTime < m_CurrentWeaponDescriptor.coverFireTime;

	// As a default handling, aim at the target.
	// The fire command handler will alter the target position later,
	// but a sane default value is provided here for at least aiming.
	if (m_fireMode != FIREMODE_OFF && targetValid && !fireSupressed)
	{
		m_weaponSpinupTime += GetAISystem()->GetFrameDeltaTime();

		aimTarget = pTarget->GetPos();

		if(useLiveTargetForMemory && pTarget && pTarget->GetSubType() == IAIObject::STP_MEMORY)
			aimTarget = m_lastLiveTargetPos;

		// When using laser, aim slightly lower so that it does not look bad when hte laser asset
		// is pointing directly at the player.
		const int enabledAccessories = GetParameters().m_weaponAccessories;
		if (enabledAccessories & AIWEPA_LASER)
		{
			// Make sure the laser is visibile when aiming directly at the player
			aimTarget.z -= 0.15f;
		}

		if	( GetSubType() != CAIObject::STP_2D_FLY )
		{
			float distSqr = Distance::Point_Point2DSq(aimTarget, GetFirePos());
			if (distSqr < sqr(2.0f))
			{
				Vec3 safePos = GetFirePos() + GetBodyDir() * 2.0f;
				if (distSqr < sqr(0.7f))
					aimTarget = safePos;
				else
				{
					float speed = m_State.vMoveDir.GetLength();
					speed = CLAMP(speed,0,10.f);

					float d = sqrtf(distSqr);
					float u = 1.0f - (d - 0.7f) / (2.0f - 0.7f);
					aimTarget += speed/10 * u * (safePos - aimTarget);
				}
			}
		}
//		GetAISystem()->AddDebugLine(GetPos(), aimTarget, 255,255,255, 0.1f);

		if (!m_pFireCmdHandler || m_pFireCmdHandler->UseDefaultEffectFor(m_fireMode))
		{
			switch(m_fireMode)
			{
			case FIREMODE_PANIC_SPREAD:
				HandleWeaponEffectPanicSpread(pTarget, aimTarget, canFire);
				break;
			case FIREMODE_BURST_DRAWFIRE:
				HandleWeaponEffectBurstDrawFire(pTarget, aimTarget, canFire);
				break;
			case FIREMODE_BURST_SNIPE:
				HandleWeaponEffectBurstSnipe(pTarget, aimTarget, canFire);
				break;
			case FIREMODE_BURST_WHILE_MOVING:
				HandleWeaponEffectBurstWhileMoving(pTarget, aimTarget, canFire);
				break;
			case FIREMODE_AIM_SWEEP:
				HandleWeaponEffectAimSweep(pTarget, aimTarget, canFire);
				break;
			};
		}

		aimTarget = UpdateTargetTracking(pTarget, aimTarget);

	}
	else
	{
		ResetTargetTracking();
		m_targetBiasDirection *= 0.5f;
		m_burstEffectTime = 0.0f;
		m_burstEffectState = 0;
		m_weaponSpinupTime = 0;
	}

	if (m_fireMode == FIREMODE_OFF)
		aimTarget.Set(0,0,0);

	// When moving and not allowed to strafe, limit the aiming to targets towards the movement target.
	if (GetSubType() != CAIObject::STP_2D_FLY)
	{
		bool isMoving = m_State.fDesiredSpeed > 0.0f && m_State.curActorTargetPhase == eATP_None && !m_State.vMoveDir.IsZero();
		if (isMoving && !m_State.allowStrafing)
		{
			Vec3 dirToTarget = aimTarget - GetFirePos();
			float distToTarget = dirToTarget.NormalizeSafe();
			const float thr = cosf(DEG2RAD(30));
			if (m_State.vMoveDir.Dot(dirToTarget) < thr)
			{
				aimTarget.Set(0,0,0);
				targetValid = false;
				canFire = false;
			}
		}
	}

	// Aim target interpolation.
	float turnSpeed = -1;
	if (canFire)
		turnSpeed = m_Parameters.m_fireTurnSpeed;
	else
		turnSpeed = m_Parameters.m_aimTurnSpeed;

	if (turnSpeed <= 0.0f)
		m_State.vAimTargetPos = aimTarget;
	else
		m_State.vAimTargetPos = InterpolateLookOrAimTargetPos(m_State.vAimTargetPos, aimTarget, turnSpeed);

	// Check if the aim is obstructed and cancel aiming if it is.
	m_State.aimObstructed = false;
	if (m_fireMode != FIREMODE_FORCED)
	{
		if (!m_State.vAimTargetPos.IsZero())
		{
			// Aiming, check towards aim target.
			if (!CanAimWithoutObstruction(m_State.vAimTargetPos))
				m_State.aimObstructed = true;
		}
		else
		{
			// Not aiming, check along forward direction.
			if (!CanAimWithoutObstruction(GetFirePos() + GetBodyDir() * 10.0f))
				m_State.aimObstructed = true;
		}
	}

	// Disable firing when obstructed.
	if (m_State.aimObstructed)
	{
		m_State.aimTargetIsValid = false;
		targetValid = false;
		canFire = false;
	}


/*	if (m_State.aimLook)
	{
		GetAISystem()->AddDebugLine(GetFirePos(), aimTarget, 255,196,0, 0.5f);
		GetAISystem()->AddDebugLine(GetFirePos(), m_State.vAimTargetPos, 255,0,0, 0.5f);
		GetAISystem()->AddDebugLine(m_State.vAimTargetPos, m_State.vAimTargetPos+Vec3(0,0,0.5f), 255,0,0, 0.5f);
	}*/


	// Make the shoot target the same as the aim target by default.
	m_State.vShootTargetPos = m_State.vAimTargetPos;

	if (m_fireMode != FIREMODE_OFF)
	{
		SAIWeaponInfo	weaponInfo;
		GetProxy()->QueryWeaponInfo(weaponInfo);

		if (weaponInfo.outOfAmmo || weaponInfo.isReloading)
			canFire = false;

		if (!m_outOfAmmoSent)
		{
			if (weaponInfo.outOfAmmo)
			{
				SetSignal(1,"OnOutOfAmmo",GetEntity(), 0, g_crcSignals.m_nOnOutOfAmmo);
				m_outOfAmmoSent = true;
				m_outOfAmmoTimeOut = 0.0f;
				m_burstEffectTime = 0.0f;
				m_burstEffectState = 0;
				if (m_pFireCmdHandler)
					m_pFireCmdHandler->OnReload();
			}
		}
		else
		{
			if (!weaponInfo.outOfAmmo)
				m_outOfAmmoSent = false;
			else if (!weaponInfo.isReloading)
			{
				m_outOfAmmoTimeOut += m_fTimePassed;
				if (m_outOfAmmoTimeOut > 3.0f)
					m_outOfAmmoSent = false;
			}
		}
	}

	if (m_pFireCmdHandler && m_fireMode != FIREMODE_OFF)
	{
		// If just starting to fire, reset the fire handler.
		if (m_fireModeUpdated)
		{
			m_pFireCmdHandler->Reset();
			m_fireModeUpdated = false;
			// Reset the friend on way filter too.
			m_friendOnWayCounter = 0.0f;
		}

		// [mikko] - This hack sets the memory target position temporarily
		// to the last known live target position for better covering fire behavior.
		Vec3	hackTemp;
		if (useLiveTargetForMemory && pTarget && pTarget->GetSubType() == IAIObject::STP_MEMORY)
		{
			hackTemp = pTarget->GetPos();
			if(!m_lastLiveTargetPos.IsZero())
				pTarget->SetPos(m_lastLiveTargetPos);
		}

		m_State.fire = m_pFireCmdHandler->Update(pTarget, canFire, m_fireMode, m_CurrentWeaponDescriptor, m_State.vShootTargetPos);

		// Hack continued.
		if (useLiveTargetForMemory && pTarget && pTarget->GetSubType() == IAIObject::STP_MEMORY)
			pTarget->SetPos(hackTemp);
	}

	if(m_State.fire)
		m_timeSinceTriggerPressed = 0.0f;

	if (m_State.vAimTargetPos.IsZero())
	{
		m_State.aimTargetIsValid = false;
		if (m_State.aimObstructed)
		{
			// Something has been detected to be obstructing the aiming.
			m_aimState = AI_AIM_OBSTRUCTED;
		}
		else
		{
			// No aiming requested.
			m_aimState = AI_AIM_NONE;
		}
	}
	else
	{
		m_State.aimTargetIsValid = true;
		SAIBodyInfo bodyInfo;
		GetProxy()->QueryBodyInfo(bodyInfo);

		if (bodyInfo.isAiming)
		{
			// The animation has finished the aim transition.
			m_aimState = AI_AIM_READY;
		}
		else
		{
			if (!lastAim)
			{
				// The game has not yet responded to the aiming request, wait.
				m_aimState = AI_AIM_WAITING;
			}
			else
			{
				// The game has disabled aiming while it was previously possible, treat as if the aiming is obscured.
				m_aimState = AI_AIM_OBSTRUCTED;
			}
		}
	}

	// Warn about conflicting fire state.
	if (m_State.fire && !canFire)
	{
		AIWarning("CPuppet::FireCommand(): state.fire = true && canFire == false");
	}

	// Update accessories.
	m_State.weaponAccessories = 0;

	const int enabledAccessories = GetParameters().m_weaponAccessories;

	if (enabledAccessories & AIWEPA_LASER)
		m_State.weaponAccessories |= AIWEPA_LASER;

	if (IsAllowedToUseExpensiveAccessory())
	{
		if (enabledAccessories & AIWEPA_COMBAT_LIGHT)
		{
			if (GetAlertness() > 0)
				m_State.weaponAccessories |= AIWEPA_COMBAT_LIGHT;
		}
		if (enabledAccessories & AIWEPA_PATROL_LIGHT)
		{
			m_State.weaponAccessories |= AIWEPA_PATROL_LIGHT;
		}
	}
}

//===================================================================
// HandleWeaponEffectBurstWhileMoving
//===================================================================
void CPuppet::HandleWeaponEffectBurstWhileMoving(CAIObject* pTarget, Vec3& aimTarget, bool& canFire)
{
	if (canFire)
	{
		// Allow to shoot while moving.
		if (pTarget && (pTarget->IsAgent() || pTarget->GetSubType() == STP_MEMORY))
		{
			m_burstEffectTime += m_fTimePassed;

			bool sprinting = m_State.fMovementUrgency >= 0.5f * (AISPEED_RUN + AISPEED_SPRINT);
			const float burstLenght = sprinting ? 1.5f : 2.5f; //IsCoverFireEnabled() ? 4.0f : 3.0f;

			if(m_burstEffectTime > 3.5f)
				m_burstEffectTime = 0.0f;

			const float facingThr = cosf(DEG2RAD(sprinting ? 10.0f : 30.0f));
			Vec3 targetDir = aimTarget - GetFirePos();
			targetDir.Normalize();

			Vec3 aimDir = GetBodyDir(); //m_State.vAimTargetPos - GetFirePos();
//			aimDir.Normalize();

/*			if (GetBodyDir().Dot(aimDir) > facingThr &&
				((m_burstEffectTime > 0.1f && m_burstEffectTime < burstLenght) ||*/
			if ((aimDir.Dot(targetDir) > facingThr && m_burstEffectTime > 0.1f && m_burstEffectTime < burstLenght) ||
				(m_State.fDistanceToPathEnd > 0.0f && m_State.fDistanceToPathEnd < 2.0f))
			{
				canFire = true;
			}
			else
			{
				canFire = false;
				aimTarget.Set(0,0,0);
			}
		}
		else
		{
			canFire = false;
			aimTarget.Set(0,0,0);
			m_burstEffectTime = 0.0f;
			m_burstEffectState = 0;
		}
	}
}

//===================================================================
// HandleWeaponEffectBurstDrawFire
//===================================================================
void CPuppet::HandleWeaponEffectBurstDrawFire(CAIObject* pTarget, Vec3& aimTarget, bool& canFire)
{
	float drawFireTime = m_CurrentWeaponDescriptor.drawTime;
	if (m_CurrentWeaponDescriptor.fChargeTime > 0)
		drawFireTime += m_CurrentWeaponDescriptor.fChargeTime;

	const float minDist = 5.0f;

	if (Distance::Point_PointSq(aimTarget, GetFirePos()) > sqr(minDist))
	{
		if (m_burstEffectTime < drawFireTime)
		{
			Vec3 shooterGroundPos = GetPhysicsPos();
			Vec3 targetGroundPos;
			Vec3 targetPos = aimTarget;
			if (pTarget->GetProxy())
			{
				targetGroundPos = pTarget->GetPhysicsPos();
				targetGroundPos.z -= 0.25f;
			}
			else
			{
				// Assume about human height target.
				targetGroundPos = targetPos;
				targetGroundPos.z -= 1.5f;
			}

			float floorHeight = min(targetGroundPos.z, shooterGroundPos.z);

			//						GetAISystem()->AddDebugLine(Vec3(shooterGroundPos.x, shooterGroundPos.y, floorHeight),
			//							Vec3(targetGroundPos.x, targetGroundPos.y, floorHeight), 128,128,128, 0.5f);
			//						Vec3 mid = (shooterGroundPos+targetGroundPos)/2;
			//						GetAISystem()->AddDebugLine(Vec3(mid.x, mid.y, floorHeight-1),
			//							Vec3(mid.x, mid.y, floorHeight+1), 255,128,128, 0.5f);

			Vec3 dirTargetToShooter = shooterGroundPos - targetGroundPos;
			dirTargetToShooter.z = 0.0f;
			float dist = dirTargetToShooter.NormalizeSafe();

			const Vec3& firePos = GetFirePos();

			float endHeight = targetGroundPos.z;
			float startHeight = floorHeight - (firePos.z - floorHeight);

			float t;
			if (m_CurrentWeaponDescriptor.fChargeTime > 0)
				t = clamp((m_burstEffectTime-m_CurrentWeaponDescriptor.fChargeTime)/(drawFireTime-m_CurrentWeaponDescriptor.fChargeTime), 0.0f, 1.0f);
			else
				t = clamp(m_burstEffectTime/drawFireTime, 0.0f, 1.0f);

			CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();
			float noiseScale = clamp((m_burstEffectTime-0.5f), 0.0f, 1.0f);
			float noise = noiseScale * pNoise->Noise1D(m_spreadFireTime + m_burstEffectTime*m_CurrentWeaponDescriptor.sweepFrequency);
			Vec3 right(dirTargetToShooter.y, -dirTargetToShooter.x, 0);

			aimTarget = targetGroundPos + right * (noise * m_CurrentWeaponDescriptor.sweepWidth);
			aimTarget.z = startHeight + (endHeight - startHeight) * (1- sqr(1-t));

			// Clamp to bottom plane.
			if (aimTarget.z < floorHeight && fabsf(aimTarget.z - firePos.z) > 0.01f)
			{
				float u = (floorHeight - firePos.z) / (aimTarget.z - firePos.z);
				aimTarget = firePos + (aimTarget - firePos) * u;
			}

			//						GetAISystem()->AddDebugLine(firePos, targetGroundPos, 255,255,255, 0.5f);
			//						GetAISystem()->AddDebugLine(firePos, m_State.vAimTargetPos, 255,255,255, 0.5f);
			//						GetAISystem()->AddDebugLine(firePos, aimTarget, 255,255,255, 0.5f);
			//						GetAISystem()->AddDebugLine(aimTarget, aimTarget+Vec3(0,0,0.5f), 255,255,255, 0.5f);
		}
		else if (m_targetLostTime > m_CurrentWeaponDescriptor.drawTime)
		{
			float amount = clamp((m_targetLostTime - m_CurrentWeaponDescriptor.drawTime) / m_CurrentWeaponDescriptor.drawTime, 0.0f, 1.0f);

			Vec3 forw = GetBodyDir();
			Vec3 right(forw.y, -forw.x, 0);
			Vec3 up(0,0,1);
			right.NormalizeSafe();
			float distToTarget = Distance::Point_Point(aimTarget, GetPos());

			float t = m_spreadFireTime + m_burstEffectTime*m_CurrentWeaponDescriptor.sweepFrequency;
			float mag = amount * m_CurrentWeaponDescriptor.sweepWidth/2;

			CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();

			float ox = sinf(t*1.5f) * mag + pNoise->Noise1D(t) * mag;
			float oy = pNoise->Noise1D(t + 33.0f)/2 * mag;

			aimTarget += ox*right + oy*up;
		}

		if (m_burstEffectTime < 0.2f)
		{
			if (!m_State.vAimTargetPos.IsZero())
			{
				const Vec3& pos = GetPos();
				// When starting to fire, make sure the aim target as fully contracted.
				//						if (Distance::Point_PointSq(aimTarget, m_State.vAimTargetPos) > sqr(0.5f))

				if (m_State.vAimTargetPos.z > (aimTarget.z + 0.25f))
				{
					// Current aim target still to high.
					canFire = false;
				}
				else
				{
					// Check for distance.
					const float distToCurSq = Distance::Point_Point2DSq(pos, m_State.vAimTargetPos);
					const float thr = sqr(Distance::Point_Point2D(pos, aimTarget) + 0.5f);
					if (distToCurSq > thr)
						canFire = false;
				}
			}
		}
		if (canFire) // && m_aimState != AI_AIM_OBSTRUCTED)
			m_burstEffectTime += m_fTimePassed;
	}
}

//===================================================================
// HandleWeaponEffectBurstSnipe
//===================================================================
void CPuppet::HandleWeaponEffectBurstSnipe(CAIObject* pTarget, Vec3& aimTarget, bool& canFire)
{
	float drawFireTime = m_CurrentWeaponDescriptor.drawTime;
	if (m_CurrentWeaponDescriptor.fChargeTime > 0)
		drawFireTime += m_CurrentWeaponDescriptor.fChargeTime;

	const float minDist = 5.0f;

	// Make it look like the sniper is aiming for headshots.
	float headHeight = aimTarget.z;
	const CAIActor* pLiveTarget = CastToCAIActorSafe(GetLiveTarget(pTarget));
	if (pLiveTarget && pLiveTarget->GetProxy())
	{
		IPhysicalEntity* pPhys = pLiveTarget->GetProxy()->GetPhysics(true);
		if (!pPhys)
			pPhys = pLiveTarget->GetPhysics();
		if (pPhys)
		{
			pe_status_pos statusPos;
			pPhys->GetStatus(&statusPos);
			float minz = statusPos.BBox[0].z + statusPos.pos.z;
			float maxz = statusPos.BBox[1].z + statusPos.pos.z + 0.25f;

			// Rough sanity check.
			if (headHeight >= minz && headHeight <= maxz)
				headHeight = maxz;
		}
	}

//	GetAISystem()->AddDebugLine(Vec3(aimTarget.x-1, aimTarget.y, headHeight),
//		Vec3(aimTarget.x+1, aimTarget.y, headHeight), 255,255,255, 0.2f);

	const Vec3& firePos = GetFirePos();
	Vec3 dirTargetToShooter = aimTarget - firePos;
	dirTargetToShooter.z = 0.0f;
	float dist = dirTargetToShooter.NormalizeSafe();
	Vec3 right(dirTargetToShooter.y, -dirTargetToShooter.x, 0);
	Vec3 up(0,0,1);
	float noiseScale = 1.0f;

	if (m_burstEffectState == 0)
	{
		if (canFire && m_aimState != AI_AIM_OBSTRUCTED)
			m_burstEffectTime += m_fTimePassed;

		// Aim towards the head position.
		if (m_burstEffectTime < drawFireTime)
		{
			Vec3 shooterGroundPos = GetPhysicsPos();
			Vec3 targetGroundPos;
			Vec3 targetPos = aimTarget;
			targetPos.z = headHeight;

			//					float floorHeight = min(targetGroundPos.z, shooterGroundPos.z);

			//					GetAISystem()->AddDebugLine(Vec3(shooterGroundPos.x, shooterGroundPos.y, floorHeight),
			//					Vec3(targetGroundPos.x, targetGroundPos.y, floorHeight), 128,128,128, 0.5f);
			//					Vec3 mid = (shooterGroundPos+targetGroundPos)/2;
			//					GetAISystem()->AddDebugLine(Vec3(mid.x, mid.y, floorHeight-1),
			//					Vec3(mid.x, mid.y, floorHeight+1), 255,128,128, 0.5f);

			

			float endHeight = aimTarget.z; //targetGroundPos.z;
			float startHeight = aimTarget.z-0.5f; //floorHeight - (firePos.z - floorHeight);

			float t;
			if (m_CurrentWeaponDescriptor.fChargeTime > 0)
				t = clamp((m_burstEffectTime-m_CurrentWeaponDescriptor.fChargeTime)/(drawFireTime-m_CurrentWeaponDescriptor.fChargeTime), 0.0f, 1.0f);
			else
				t = clamp(m_burstEffectTime/drawFireTime, 0.0f, 1.0f);

			noiseScale = t;

			//					aimTarget = targetGroundPos + right * (noise * m_CurrentWeaponDescriptor.sweepWidth);
//			aimTarget = aimTarget + ox*right + oy*up;
			aimTarget.z = startHeight + (endHeight - startHeight) * t;

			// Clamp to bottom plane.
			//					if (aimTarget.z < floorHeight && fabsf(aimTarget.z - firePos.z) > 0.01f)
			//					{
			//						float u = (floorHeight - firePos.z) / (aimTarget.z - firePos.z);
			//						aimTarget = firePos + (aimTarget - firePos) * u;
			//					}

			//						GetAISystem()->AddDebugLine(firePos, targetGroundPos, 255,255,255, 0.5f);
			//						GetAISystem()->AddDebugLine(firePos, m_State.vAimTargetPos, 255,255,255, 0.5f);
			//						GetAISystem()->AddDebugLine(firePos, aimTarget, 255,255,255, 0.5f);
			//						GetAISystem()->AddDebugLine(aimTarget, aimTarget+Vec3(0,0,0.5f), 255,255,255, 0.5f);
		}
		else
		{
			m_burstEffectState = 1;
			m_burstEffectTime = -1;
		}
	}
	else if (m_burstEffectState == 1)
	{
		if (m_targetLostTime > m_CurrentWeaponDescriptor.drawTime)
		{
			if (m_burstEffectTime < 0)
				m_burstEffectTime = 0;

			if (canFire && m_aimState != AI_AIM_OBSTRUCTED)
				m_burstEffectTime += m_fTimePassed;

			// Target getting lost, aim above the head.
			float amount = clamp((m_targetLostTime - m_CurrentWeaponDescriptor.drawTime) / m_CurrentWeaponDescriptor.drawTime, 0.0f, 1.0f);

			Vec3 forw = GetBodyDir();
			Vec3 right(forw.y, -forw.x, 0);
			Vec3 up(0,0,1);
			right.NormalizeSafe();
			float distToTarget = Distance::Point_Point(aimTarget, GetFirePos());

			float t = m_spreadFireTime + m_burstEffectTime*m_CurrentWeaponDescriptor.sweepFrequency;
			float mag = amount * m_CurrentWeaponDescriptor.sweepWidth/2 * clamp((distToTarget - 1.0f)/5.0f, 0.0f, 1.0f);

			CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();

			float tsweep = m_burstEffectTime*m_CurrentWeaponDescriptor.sweepFrequency;

			float ox = sinf(tsweep) * mag; // + pNoise->Noise1D(t) * mag;
			float oy = 0; //pNoise->Noise1D(t + 33.0f)/4 * mag;

			aimTarget.z = aimTarget.z + (headHeight - aimTarget.z) * clamp(m_burstEffectTime, 0.0f, 1.0f);
			aimTarget += ox*right + oy*up;
		}
	}

	float noiseTime = m_spreadFireTime;
	m_spreadFireTime += m_fTimePassed;
		
	noiseTime *= m_CurrentWeaponDescriptor.sweepFrequency * 2;
	CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();
	float nx = pNoise->Noise1D(noiseTime) * noiseScale * 0.1f;
	float ny = pNoise->Noise1D(noiseTime + 33.0f) * noiseScale * 0.1f;
	aimTarget += right*nx + up*ny;

//	GetAISystem()->AddDebugLine(aimTarget, firePos, 255,0,0, 10.0f);

}

//===================================================================
// HandleWeaponEffectAimSweep
//===================================================================
void CPuppet::HandleWeaponEffectAimSweep(CAIObject* pTarget, Vec3& aimTarget, bool& canFire)
{
	float drawFireTime = m_CurrentWeaponDescriptor.drawTime;
	if (m_CurrentWeaponDescriptor.fChargeTime > 0)
		drawFireTime += m_CurrentWeaponDescriptor.fChargeTime;

	const float minDist = 5.0f;

	// Make it look like the sniper is aiming for headshots.
	float headHeight = aimTarget.z;
	const CAIActor* pLiveTarget = CastToCAIActorSafe(GetLiveTarget(pTarget));
	if (pLiveTarget && pLiveTarget->GetProxy())
	{
		IPhysicalEntity* pPhys = pLiveTarget->GetProxy()->GetPhysics(true);
		if (!pPhys)
			pPhys = pLiveTarget->GetPhysics();
		if (pPhys)
		{
			pe_status_pos statusPos;
			pPhys->GetStatus(&statusPos);
			float minz = statusPos.BBox[0].z + statusPos.pos.z;
			float maxz = statusPos.BBox[1].z + statusPos.pos.z + 0.25f;

			// Rough sanity check.
			if (headHeight >= minz && headHeight <= maxz)
				headHeight = maxz;
		}
	}

	if (m_burstEffectTime < 0)
		m_burstEffectTime = 0;

	if (m_aimState != AI_AIM_OBSTRUCTED)
		m_burstEffectTime += m_fTimePassed;

	// Target getting lost, aim above the head.

	Vec3 forw = GetBodyDir();
	Vec3 right(forw.y, -forw.x, 0);
	Vec3 up(0,0,1);
	right.NormalizeSafe();

	float t = m_spreadFireTime + m_burstEffectTime*m_CurrentWeaponDescriptor.sweepFrequency;
	float mag = m_CurrentWeaponDescriptor.sweepWidth/2;

	CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();

	float distToTarget = Distance::Point_Point(aimTarget, GetFirePos());
	float dscale = clamp((distToTarget - 1.0f)/5.0f, 0.0f, 1.0f);

	float tsweep = m_burstEffectTime*m_CurrentWeaponDescriptor.sweepFrequency;

	float ox = sinf(tsweep) * mag * dscale; // + pNoise->Noise1D(t) * mag;
	float oy = 0; //pNoise->Noise1D(t + 33.0f)/4 * mag;

	aimTarget.z = aimTarget.z + (headHeight - aimTarget.z) * clamp(m_burstEffectTime/2, 0.0f, 1.0f);
	aimTarget += ox*right + oy*up;


	float noiseTime = m_spreadFireTime;
	m_spreadFireTime += m_fTimePassed;

	float noiseScale = clamp(m_burstEffectTime, 0.0f, 1.0f) * dscale;

	noiseTime *= m_CurrentWeaponDescriptor.sweepFrequency * 2;
	float nx = pNoise->Noise1D(noiseTime) * noiseScale * 0.1f;
	float ny = pNoise->Noise1D(noiseTime + 33.0f) * noiseScale * 0.1f;
	aimTarget += right*nx + up*ny;
}

//===================================================================
// HandleWeaponEffectPanicSpread
//===================================================================
void CPuppet::HandleWeaponEffectPanicSpread(CAIObject* pTarget, Vec3& aimTarget, bool& canFire)
{
	// Don't start moving until the aim is ready.
	if (m_aimState == AI_AIM_READY)
	{
		m_burstEffectTime += GetAISystem()->GetFrameDeltaTime();
		m_spreadFireTime += GetAISystem()->GetFrameDeltaTime();
	}

	// Calculate aside-to-side wiggly motion when requesting the spread fire.
	float t = m_spreadFireTime;

	Vec3 forw = GetBodyDir();
	Vec3 right(forw.y, -forw.x, 0);
	Vec3 up(0,0,1);
	right.NormalizeSafe();
	float distToTarget = Distance::Point_Point(aimTarget, GetPos());

	float speed = 1.7f;
	float spread = DEG2RAD(40.0f);

	t *= speed;
	float mag = distToTarget*tanf(spread/2);
	mag *= 0.25f + min(m_burstEffectTime / 0.5f, 1.0f)*0.75f;	// Scale the effect down when starting.

	CPNoise3* pNoise = gEnv->pSystem->GetNoiseGen();

	float ox = sinf(t*1.7f) * mag + pNoise->Noise1D(t) * mag;
	float oy = pNoise->Noise1D(t*0.98432f + 33.0f) * mag;

	aimTarget += ox*right + oy*up;
}

//===================================================================
// FireSecondary
//===================================================================
void CPuppet::FireSecondary(CAIObject* pTarget) 
{
	if (!pTarget)
	{
		SetFireMode(FIREMODE_OFF);
		return;
	}

	if (!m_pFireCmdGrenade)
	{
		SetFireMode(FIREMODE_OFF);
		return;
	}

	if(m_fireMode == FIREMODE_SECONDARY_SMOKE)
		m_State.secondaryType = RST_SMOKE_GRENADE;
	else
		m_State.secondaryType = RST_ANY;

	AIWeaponDescriptor dummy;
	Vec3 targetPos = pTarget->GetPos();
	m_State.fireSecondary = m_pFireCmdGrenade->Update(pTarget, true, m_fireMode, dummy, targetPos);
	m_State.vShootTargetPos = targetPos;
	m_State.fProjectileSpeedScale = m_pFireCmdGrenade->GetThrowSpeedScale();

	if (m_State.fireSecondary)
		SetFireMode(FIREMODE_OFF);

	if (m_State.vShootTargetPos.IsZero())
		m_State.fireSecondary = false;
}


//===================================================================
// FireMelee
//===================================================================
void CPuppet::FireMelee(CAIObject* pTarget) 
{
	SAIWeaponInfo weaponInfo;
	GetProxy()->QueryWeaponInfo(weaponInfo);

	// Wait until at close range and until facing approx the right direction.
	if (m_fireMode != FIREMODE_MELEE_FORCED && Distance::Point_PointSq(pTarget->GetPos(), GetPos()) > sqr(1.5f)) //m_Parameters.m_fMeleeDistance))
		return;

	Vec3	dirToTarget = pTarget->GetPos() - GetPos();
	dirToTarget.Normalize();

	float	dot = dirToTarget.Dot(GetBodyDir());
	const float MELEE_LOOK_TRESHOLD = cosf(DEG2RAD(20.0f));
	if (m_fireMode != FIREMODE_MELEE_FORCED && dot < MELEE_LOOK_TRESHOLD)
		return;

	// Execute the melee.
	m_State.fireMelee = true;
	SetFireMode(FIREMODE_OFF);

	SetSignal(1,"OnMeleeExecuted",GetEntity(), 0, g_crcSignals.m_nOnMeleeExecuted);
}


//===================================================================
// Compromising
// Evaluates whether the chosen navigation point will expose us too much to the target
//===================================================================
bool CPuppet::Compromising(const Vec3& pos, const Vec3& dir, const Vec3& hideFrom, const Vec3& objectPos, const Vec3& searchPos, bool bIndoor, bool bCheckVisibility) const
{
	if( !bIndoor )
	{
		// allow him to use only the hidespots closer to him than to the enemy
		Vec3 dirHideToEnemy = hideFrom - pos;
		Vec3 dirHide = pos - searchPos;
		if ( dirHide.GetLengthSquared() > dirHideToEnemy.GetLengthSquared() )
			return true;
	}
	// finally: check is the enemy visible from there
	if ( bCheckVisibility && GetAISystem()->CheckPointsVisibility(pos, hideFrom, 5.0f) )
		return true;

	return false;
}

//===================================================================
// TransformFOV
//===================================================================
void CPuppet::TransformFOV()
{
	float	FOVPrimary = m_Parameters.m_PerceptionParams.FOVPrimary;
	float	FOVSecondary = m_Parameters.m_PerceptionParams.FOVSecondary;

	if (FOVPrimary < 0.0f || FOVPrimary > 360.0f )	// see all around
	{
		m_FOVPrimaryCos = -1;
		m_FOVSecondaryCos = -1;
	}
	else 
	{
		if( FOVSecondary > 0.0f && FOVPrimary > FOVSecondary )
			FOVSecondary = FOVPrimary;

		m_FOVPrimaryCos = cosf( DEG2RAD( FOVPrimary / 2.0f ) );

		if (FOVSecondary < 0.0f || FOVSecondary > 360.0f )	// see all around
			m_FOVSecondaryCos = -1.0f;
		else 
			m_FOVSecondaryCos = cosf( DEG2RAD( FOVSecondary / 2.0f ) );
	}
}

//===================================================================
// SetParameters
//===================================================================
void CPuppet::SetParameters(AgentParameters & sParams)
{
	if (sParams.m_nSpecies != m_Parameters.m_nSpecies)
		GetAISystem()->AddToSpecies(this,sParams.m_nSpecies);

	SetGroupId(sParams.m_nGroup);

	CAIGroup*	pGroup = GetAISystem()->GetAIGroup(GetGroupId());
	AIAssert(pGroup);
	pGroup->SetUnitRank((IAIObject*)this,sParams.m_nRank);

	// Update tracker.
	if( sParams.m_trackPatternName.empty() )
	{
		// The string was set to empty, clear the track pattern.
		delete m_pTrackPattern;
		m_pTrackPattern = 0;
	}
	else
	{
		// Update track patterns. This is the main way to enable the trackpattern in the first place.
		if( !m_pTrackPattern )
			m_pTrackPattern = new CTrackPattern( this );

		CTrackPatternDescriptor*	pdesc = (CTrackPatternDescriptor*)GetAISystem()->FindTrackPatternDescriptor( sParams.m_trackPatternName );
		if( pdesc )
			m_pTrackPattern->SetPattern( *pdesc );
		else
			AIWarning("%s Cannot find track pattern '%s'", GetName(), sParams.m_trackPatternName.c_str());
	}

	// Set continue flag.
	if( m_pTrackPattern && sParams.m_trackPatternContinue )
	{
		m_pTrackPattern->ContinueAdvance( sParams.m_trackPatternContinue == 1 );
		sParams.m_trackPatternContinue = 0;
	}

	m_Parameters = sParams;

	TransformFOV();
}

//===================================================================
// CheckAwarenessPlayer
//===================================================================
void CPuppet::CheckAwarenessPlayer()
{
	CAIObject *pPlayer = GetAISystem()->GetPlayer();

	if (!pPlayer)
		return;

	Vec3 lookDir = pPlayer->GetViewDir();
	Vec3 relPos = (GetPos() - pPlayer->GetPos());
	float dist = relPos.GetLength();
	if(dist>0)
		relPos /= dist;
	float fdot;
	float threshold;
	bool bCheckPlayerLooking = false;
	if(dist<=1.2f)
	{
		fdot = GetMoveDir().Dot(-relPos);
		threshold = 0;
	}
	else
	{
		bCheckPlayerLooking = true;
		fdot = lookDir.Dot(relPos);
		threshold = cos(atan(0.5/dist)); // simulates a circle of 0.5m radius centered on AIObject, 
	}																	//checks if the player is looking inside it
	if (fdot > threshold  && IsObjectInFOVCone(pPlayer))
	{
		if(m_fLastTimeAwareOfPlayer ==0 && bCheckPlayerLooking)
		{
			ray_hit hit[2];
			IEntity * pEntity = NULL;
			IPhysicalEntity* pPlayerPhE = (pPlayer->GetProxy() ? pPlayer->GetPhysics() : NULL);

			if (GetAISystem()->GetPhysicalWorld()->RayWorldIntersection(pPlayer->GetPos(), lookDir * (dist+0.5f), COVER_OBJECT_TYPES+ent_living, HIT_COVER|HIT_SOFT_COVER , hit, 1,pPlayerPhE))
			{
				IPhysicalEntity* pCollider =  hit[0].pCollider;
				pEntity = (IEntity*) pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
			}
			if (!(pEntity && pEntity==GetEntity()))
			{
				//m_fLastTimePlayerLooking = 0;
				return;
			}
		}
		float fCurrentTime =  GetAISystem()->GetFrameStartTime().GetSeconds();
		if(m_fLastTimeAwareOfPlayer ==0)
			m_fLastTimeAwareOfPlayer = fCurrentTime;
		else if(fCurrentTime - m_fLastTimeAwareOfPlayer >= GetParameters().m_fAwarenessOfPlayer)
		{
			IAISignalExtraData* 	pData = GetAISystem()->CreateSignalExtraData();
			if(pData)
				pData->fValue = dist;
			m_playerAwarenessType = dist<=GetParameters().m_fMeleeDistance ? PA_STICKING: PA_LOOKING;
			IEntity* pUserEntity = GetEntity();
			IEntity* pObjectEntity = pPlayer->GetEntity();
			GetAISystem()->SmartObjectEvent(bCheckPlayerLooking ? "OnPlayerLooking" : "OnPlayerSticking",pUserEntity,pObjectEntity);
			SetSignal(1,  bCheckPlayerLooking ? "OnPlayerLooking" : "OnPlayerSticking",pPlayer->GetEntity(),pData, bCheckPlayerLooking ? g_crcSignals.m_nOnPlayerLooking : g_crcSignals.m_nOnPlayerSticking);
			m_fLastTimeAwareOfPlayer = fCurrentTime;
		}
	}
	else
	{
		IEntity* pUserEntity = GetEntity();
		IEntity* pObjectEntity = pPlayer->GetEntity();
		if(m_playerAwarenessType==PA_LOOKING)
		{
			SetSignal(1, "OnPlayerLookingAway" ,0 ,0, g_crcSignals.m_nOnPlayerLookingAway);
			GetAISystem()->SmartObjectEvent( "OnPlayerLookingAway", pUserEntity, pObjectEntity );
		}
		else if(m_playerAwarenessType==PA_STICKING)
		{
			GetAISystem()->SmartObjectEvent( "OnPlayerGoingAway", pUserEntity, pObjectEntity );
			SetSignal(1, "OnPlayerGoingAway", 0,0, g_crcSignals.m_nOnPlayerGoingAway);
		}
		m_fLastTimeAwareOfPlayer = 0;
		m_playerAwarenessType = PA_NONE;
	}
}

//===================================================================
// RemoveFromGoalPipe
//===================================================================
void CPuppet::RemoveFromGoalPipe(CAIObject* pObject)
{
	if (m_pCurrentGoalPipe)
	{
		if (m_pCurrentGoalPipe->IsInSubpipe())
		{
			CGoalPipe *pPipe = m_pCurrentGoalPipe;
			do
			{
				if (pPipe->m_pArgument == pObject)
					pPipe->m_pArgument = 0;
			}
			while (pPipe = pPipe->GetSubpipe());
		}
		else
		{
			if (m_pCurrentGoalPipe->m_pArgument == pObject)
				m_pCurrentGoalPipe->m_pArgument = 0;
		}
	}
}

//===================================================================
// Serialize
//===================================================================
void CPuppet::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
//	if(ser.IsReading())
//		Reset();  // Luc: already reset by AI System

	ser.BeginGroup( "AIPuppet" );
	{

		ser.BeginGroup( "PendingEvents" );
		{
			ser.Value("m_fCurrentAwarenessLevel",m_fCurrentAwarenessLevel);
			int eventCount = m_perceptionEvents.size();
			ser.Value("eventCount", eventCount);

			// The events should be empty while reading.
			if (ser.IsReading())
				AIAssert(m_perceptionEvents.empty());
			
			char eventName[32];
			while(--eventCount>=0)
			{
				CAIObject* pEventObject(0);
				SAIPotentialTarget eventData;
				if(ser.IsWriting())
				{
					PotentialTargetMap::iterator iEvent = m_perceptionEvents.begin();
					std::advance(iEvent, eventCount);
					pEventObject = iEvent->first;
					eventData = iEvent->second;
				}
				sprintf(eventName, "Event_%d", eventCount);
				ser.BeginGroup(eventName);
				{
					objectTracker.SerializeObjectPointer(ser, "pObject", pEventObject, false);
					eventData.Serialize(ser, objectTracker);
				}
				ser.EndGroup();
				if(ser.IsReading())
					AddEvent(pEventObject, eventData);
			}
		}
		ser.EndGroup();


		//
		if (m_pLastOpResult && ser.IsWriting())
		{
			if (false == GetAISystem()->IsAIObjectRegistered(m_pLastOpResult))
			{
				AIWarning("Unable to find m_pLastOpResult during serialisation - setting to 0");
				m_pLastOpResult = 0;
			}
		}
		CPipeUser::Serialize( ser, objectTracker );

		// The track pattern may not exists on all puppets.
		bool trackPatternExists = m_pTrackPattern != 0;
		ser.Value( "trackPatternExists", trackPatternExists );

		if( trackPatternExists )
		{
			// Make sure we create the track pattern when reading.
			if( ser.IsReading() && !m_pTrackPattern )
				m_pTrackPattern = new CTrackPattern( this );
			m_pTrackPattern->Serialize( ser, objectTracker );
		}

		ser.Value("m_targetApproach",m_targetApproach);
		ser.Value("m_targetFlee",m_targetFlee);
		ser.Value("m_targetApproaching",m_targetApproaching);
		ser.Value("m_targetFleeing",m_targetFleeing);
		ser.Value("m_lastTargetValid",m_lastTargetValid);
		ser.Value("m_lastTargetPos",m_lastTargetPos);
		ser.Value("m_lastTargetSpeed",m_lastTargetSpeed);
		ser.Value("m_fLastUpdateTime",m_fLastUpdateTime);
		ser.Value("m_weaponSpinupTime", m_weaponSpinupTime);

		ser.Value("m_Alertness", m_Alertness);
		ser.Value("m_allowedToHitTarget", m_allowedToHitTarget);
		ser.Value("m_bCoverFireEnabled",m_bCoverFireEnabled);
		ser.Value("m_firingReactionTimePassed", m_firingReactionTimePassed);
		ser.Value("m_firingReactionTime", m_firingReactionTime);
		ser.Value("m_outOfAmmoTimeOut", m_outOfAmmoTimeOut);
		ser.Value("m_allowedToUseExpensiveAccessory", m_allowedToUseExpensiveAccessory);

		ser.Value("m_adaptiveUrgencyMin",m_adaptiveUrgencyMin);
		ser.Value("m_adaptiveUrgencyMax",m_adaptiveUrgencyMax);
		ser.Value("m_adaptiveUrgencyScaleDownPathLen",m_adaptiveUrgencyScaleDownPathLen);
		ser.Value("m_adaptiveUrgencyMaxPathLen",m_adaptiveUrgencyMaxPathLen);
		ser.Value("m_chaseSpeed",m_chaseSpeed);
		ser.Value("m_lastChaseUrgencyDist", m_lastChaseUrgencyDist);
		ser.Value("m_lastChaseUrgencySpeed", m_lastChaseUrgencySpeed);
		ser.Value("m_chaseSpeedRate",m_chaseSpeedRate);
		ser.Value("m_delayedStance", m_delayedStance);
		ser.Value("m_delayedStanceMovementCounter", m_delayedStanceMovementCounter);

		ser.Value("m_allowedStrafeDistanceStart", m_allowedStrafeDistanceStart);
		ser.Value("m_allowedStrafeDistanceEnd", m_allowedStrafeDistanceEnd);
		ser.Value("m_allowStrafeLookWhileMoving", m_allowStrafeLookWhileMoving);
		ser.Value("m_strafeStartDistance", m_strafeStartDistance);

		// m_mapMemory (currently never updated in game)

		objectTracker.SerializeObjectPointerValueContainer(ser,"MapDevaluedPoints",m_mapDevaluedPoints,false);
//		objectTracker.SerializeObjectPointerValueContainer(ser,"MapDevaluedUpTargets",m_mapDevaluedUpTargets,false);
		
		//objectTracker.SerializeObjectContainer(ser,"MapVisibleAgents",m_mapVisibleAgents);

		ser.Value( "m_playerAwarenessType", (int&)m_playerAwarenessType );
		ser.Value( "m_fLastTimeAwareOfPlayer", m_fLastTimeAwareOfPlayer );
		ser.Value( "m_fLastStuntTime", m_fLastStuntTime );
		ser.Value( "m_bCloseContact",m_bCloseContact);
		ser.Value( "m_vForcedNavigation",m_vForcedNavigation);
		ser.Value( "m_adjustpath",m_adjustpath);

		ser.Value("m_alarmedTime", m_alarmedTime);

		ser.BeginGroup("InitialPath");
		{
			int pointCount = m_InitialPath.size();
			ser.Value("pointCount", pointCount);
			Vec3 point;
			char name[16];
			if(ser.IsReading())
			{
				m_InitialPath.clear();

				for(int i=0;i<pointCount;i++)
				{
					sprintf(name, "Point_%d", i);
					ser.Value(name,point);
					m_InitialPath.push_back(point);
				}
			}
			else
			{
				TPointList::iterator itend = m_InitialPath.end();
				int i=0;
				for(TPointList::iterator it = m_InitialPath.begin();it!=itend;++it, i++)
				{
					sprintf(name, "Point_%d", i);
					point = *it;
					ser.Value(name,point);
				}
			}
		}
		ser.EndGroup();
		
		ser.Value("m_vehicleAvoidingTime",m_vehicleAvoidingTime);
		objectTracker.SerializeObjectPointer(ser,"m_pAvoidedVehicle",m_pAvoidedVehicle,false);

		// Territory
		ser.Value("m_territoryShapeName", m_territoryShapeName);
		if(ser.IsReading())
			m_territoryShape = GetAISystem()->GetGenericShapeOfName(m_territoryShapeName.c_str());

		// Target tracking
		ser.BeginGroup("TargetTracking");
		{
			// The target tracking is update frequently, so just reset them here.
			if(ser.IsReading())
			{
				m_targetSilhouette.Reset();
				m_targetLastMissPoint.Set(0,0,0);
				m_targetFocus = 0.0f;
				m_targetZone = ZONE_OUT;
				m_targetDistanceToSilhouette = FLT_MAX;
				m_targetBiasDirection.Set(0,0,-1);
				m_targetEscapeLastMiss = 0.0f;
				m_targetLastMissPointSelectionTime.SetSeconds(0.0f);
				m_burstEffectTime = 0.0f;
				m_burstEffectState = 0;
				m_targetHitPartIndex.clear();
			}

			// Serialize the more important values.
			ser.Value("m_targetDamageHealthThr", m_targetDamageHealthThr);
			ser.Value("m_targetSeenTime", m_targetSeenTime);
			ser.Value("m_targetLostTime", m_targetLostTime);
			ser.Value("m_targetDazzlingTime", m_targetDazzlingTime);
		}
		ser.EndGroup();
	}
	ser.EndGroup();

	if (ser.IsReading())
	{
		GetAISystem()->NotifyEnableState(this, m_bEnabled);
		m_steeringOccupancy.Reset(Vec3(0,0,0), Vec3(0,1,0), 1.0f);
		m_steeringOccupancyBias = 0;
		m_steeringAdjustTime = 0;
	}
}

//===================================================================
// AddEvent
//===================================================================
SAIPotentialTarget* CPuppet::AddEvent(CAIObject* pObject, SAIPotentialTarget& ed)
{
	if (GetAISystem()->m_cvIgnorePlayer->GetIVal())
	{
    if (pObject->GetType() == AIOBJECT_PLAYER)
    {
      if (ed.pDummyRepresentation)
        GetAISystem()->RemoveDummyObject(ed.pDummyRepresentation);
      ed.pDummyRepresentation = 0;
			return 0;
    }
	}

	std::pair<PotentialTargetMap::iterator, bool> res = m_perceptionEvents.insert(PotentialTargetMap::iterator::value_type(pObject, ed));
	if (!res.second)
  {
    AIWarning("CPuppet::AddEvent %s Unable to insert pending event", m_sName.c_str());
    if (ed.pDummyRepresentation)
      GetAISystem()->RemoveDummyObject(ed.pDummyRepresentation);
    ed.pDummyRepresentation = 0;
		return 0;
  }

	if (ed.pDummyRepresentation)
	{
		CAIGroup* pGroup = GetAISystem()->GetAIGroup(GetGroupId());
		if (pGroup)
			pGroup->OnUnitAttentionTargetChanged();
	}

	return &(res.first->second);
}

//===================================================================
// GetPriorityMultiplier
//===================================================================
float CPuppet::GetPriorityMultiplier(CAIObject * pTarget)
{
	float priorityMult = 0.0f;

	// find object type multiplier, if one exists
	float objectMultiplier = 1.0f;
	MapMultipliers::iterator mi = GetAISystem()->m_mapMultipliers.find(pTarget->GetType());
	if (mi != GetAISystem()->m_mapMultipliers.end())
		objectMultiplier = mi->second;

	int targetSpecies(-1);
	bool targetSpeciesHostility(false);
	CAIActor* pTargetActor = pTarget->CastToCAIActor();
	if (pTargetActor)
	{
		AgentParameters apTarget = pTargetActor->GetParameters();
		targetSpecies = apTarget.m_nSpecies;
		targetSpeciesHostility = apTarget.m_bSpeciesHostility;
	}
	// find species multiplier, if one exists
	float speciesMultiplier = 1.0f;
	mi = GetAISystem()->m_mapSpeciesThreatMultipliers.find(targetSpecies); 
	if (mi != GetAISystem()->m_mapSpeciesThreatMultipliers.end())
		speciesMultiplier = mi->second;

	// species multiplication
	if (m_Parameters.m_nSpecies != targetSpecies &&
		(m_Parameters.m_bSpeciesHostility || targetSpecies < 0) &&
		(targetSpeciesHostility || m_Parameters.m_nSpecies < 0))
		return speciesMultiplier * objectMultiplier;
	else
		return objectMultiplier;
}

//===================================================================
// MakeMeLeader
//===================================================================
IAIObject* CPuppet::MakeMeLeader()
{
	if (GetGroupId() == -1 || !GetGroupId())
	{
		AIWarning("CPuppet::MakeMeLeader: Invalid GroupID ... %d", GetGroupId());
		return NULL;
	}

	CLeader* pLeader = (CLeader*) GetAISystem()->GetLeader(GetGroupId());
	if (pLeader)
	{
		CPuppet* pPuppet = (CPuppet*) pLeader->GetAssociation();
		if (pPuppet != this)
			pLeader->SetAssociation(this);
	}
	else
		pLeader = (CLeader*) GetAISystem()->CreateAIObject(AIOBJECT_LEADER, this);
	return pLeader;
}


//===================================================================
// AdjustSpeed
//===================================================================
void CPuppet::AdjustSpeed(CAIObject* pNavTarget, float distance)
{
	if(!pNavTarget)
		return;

	if(GetType() != AIOBJECT_PUPPET || IsUsing3DNavigation())
  {
	  // Danny/Kirill TODO: make some smart AdjastSpeed for vehicles chasing, currently just force maximum speed for vehicles always
//	  m_State.fDesiredSpeed = 1.0f;
	  return;
  }
  // puppet speed control
	CTimeValue fCurrentTime =  GetAISystem()->GetFrameStartTime();
	float timeStep = (fCurrentTime - m_SpeedControl.fLastTime).GetSeconds();

	// evaluate the potential target speed
	float targetSpeed = 0.0f;
	Vec3 targetVel(ZERO);
  Vec3 targetPos = pNavTarget->GetPos();
	IPhysicalEntity * pPhysProxy(NULL);
	if(pNavTarget->GetProxy() && pNavTarget->GetPhysics())
  {
		pPhysProxy = pNavTarget->GetPhysics();
  }
	else if(pNavTarget->GetSubType() == CAIObject::STP_FORMATION)
	{
		CAIObject *pOwner=static_cast<CAIObject*>(pNavTarget->GetAssociation());
		if(pOwner && pOwner->GetProxy() && pOwner->GetPhysics())
			pPhysProxy = pOwner->GetPhysics();
	}

	if (pPhysProxy)
	{
		pe_status_dynamics status;
		pPhysProxy->GetStatus(&status);
		targetVel = status.v;
	}
	else if (fCurrentTime>m_SpeedControl.fLastTime)
	{
		targetVel = (m_SpeedControl.vLastPos - GetPos())/(fCurrentTime - m_SpeedControl.fLastTime).GetSeconds();
	}

	targetSpeed = targetVel.GetLength();

  // get/estimate the path distance to the target point
  float distToEnd = m_State.fDistanceToPathEnd;

  float distToTarget = Distance::Point_Point2D(GetPos(), targetPos);
  distToEnd = max(distToEnd, distToTarget);

  distToEnd -= distance;
  if (distToEnd < 0.0f)
    distToEnd = 0.0f;

	float walkSpeed, runSpeed, sprintSpeed, junk0, junk1;
	GetMovementSpeedRange(AISPEED_WALK, false, junk0, junk1, walkSpeed);
	GetMovementSpeedRange(AISPEED_RUN, false, junk0, junk1, runSpeed);
	GetMovementSpeedRange(AISPEED_SPRINT, false, junk0, junk1, sprintSpeed);

	// ramp up/down the urgency
	static float distForWalk = 4.0f;
	static float distForRun = 6.0f;
	static float distForSprint = 10.0f;

	if (m_lastChaseUrgencyDist > 0)
	{
		if (m_lastChaseUrgencyDist == 4)
		{
			// Sprint
			if (distToEnd < distForSprint - 1.0f)
				m_lastChaseUrgencyDist = 3;
		}
		else if (m_lastChaseUrgencyDist == 3)
		{
			// Run
			if (distToEnd > distForSprint)
				m_lastChaseUrgencyDist = 4;
			if (distToEnd < distForWalk - 1.0f)
				m_lastChaseUrgencyDist = 2;
		}
		else
		{
			// Walk
			if (distToEnd > distForRun)
				m_lastChaseUrgencyDist = 3;
		}
	}
	else
	{
		m_lastChaseUrgencyDist = 0;	// zero
		if (distToEnd > distForRun)
			m_lastChaseUrgencyDist = 4;	// sprint
		else if (distToEnd > distForWalk)
			m_lastChaseUrgencyDist = 3;	// run
		else
			m_lastChaseUrgencyDist = 2;	// walk
	}

	float urgencyDist = IndexToMovementUrgency(m_lastChaseUrgencyDist);


	if (m_lastChaseUrgencySpeed > 0)
	{
		if (m_lastChaseUrgencyDist == 4)
		{
			// Sprint
			if (targetSpeed < runSpeed)
				m_lastChaseUrgencySpeed = 3;
		}
		else if (m_lastChaseUrgencyDist == 3)
		{
			// Run
			if (targetSpeed > runSpeed * 1.2f)
				m_lastChaseUrgencySpeed = 4;
			if (targetSpeed < walkSpeed)
				m_lastChaseUrgencySpeed = 2;
		}
		else
		{
			// Walk
			if (targetSpeed > walkSpeed * 1.2f)
				m_lastChaseUrgencySpeed = 3;
			if (targetSpeed < 0.001f)
				m_lastChaseUrgencySpeed = 0;
		}
	}
	else
	{
		if (targetSpeed > runSpeed * 1.2f)
			m_lastChaseUrgencySpeed = 4;	// sprint
		else if (targetSpeed > walkSpeed * 1.2f)
			m_lastChaseUrgencySpeed = 3;	// run
		else if (targetSpeed > 0.0f)
			m_lastChaseUrgencySpeed = 2;	// walk
		else
			m_lastChaseUrgencySpeed = 0;	// zero
	}

	float urgencySpeed = IndexToMovementUrgency(m_lastChaseUrgencySpeed);


/*	float urgencyDist = AISPEED_ZERO;
	if (distToEnd > distForRun)
		urgencyDist = AISPEED_SPRINT;
	else if (distToEnd > distForWalk)
		urgencyDist = AISPEED_RUN;
	else
		urgencyDist = AISPEED_WALK;

	float urgencySpeed = AISPEED_ZERO;

	if (targetSpeed > runSpeed)
		urgencySpeed = AISPEED_SPRINT;
	else if (targetSpeed > walkSpeed)
		urgencySpeed = AISPEED_RUN;
	else if (targetSpeed > 0.0f)
		urgencySpeed = AISPEED_WALK;
	else
		urgencySpeed = AISPEED_ZERO;*/

	float urgency = max(urgencySpeed, urgencyDist);

	m_State.fMovementUrgency = urgency;
	m_State.predictedCharacterStates.nStates = 0;

	float normalSpeed, minSpeed, maxSpeed;
	GetMovementSpeedRange(urgency, m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

	if (targetSpeed < minSpeed)
		targetSpeed = minSpeed;

  // calculate the speed required to match the target speed, and also the
  // speed desired to trace the path without worrying about the target movement.
  // Then take the maximum of each. 
  // Also we have to make sure that we don't overshoot the path, otherwise we'll 
  // keep stopping.
  // m_State.fDesiredSpeed will/should already include/not include slowing at end
  // depending on if the target is moving
  // If dist to path end > lagDistance * 2 then use path speed control
  // if between lagDistance*2 and lagDistance blend between path speed control and absolute speed
  // if less than than then blend to 0
  static float maxExtraLag = 2.0f;
  static float speedForMaxExtraLag = 1.5f;

  float lagDistance = 0.1f + maxExtraLag * targetSpeed / speedForMaxExtraLag;
  Limit(lagDistance, 0.0f, maxExtraLag);

  float frac = distToEnd / lagDistance;
  Limit(frac, 0.0f, 2.2f);
  float chaseSpeed;
  if (frac < 1.0f)
    chaseSpeed = frac * targetSpeed + (1.0f - frac) * minSpeed;
  else if (frac < 2.0f)
    chaseSpeed = (2.0f - frac) * targetSpeed + (frac - 1.0f) * maxSpeed;
  else
    chaseSpeed = maxSpeed * (frac - 1.0f);

  static float chaseSpeedSmoothTime = 1.0f;
  SmoothCD(m_chaseSpeed, m_chaseSpeedRate, timeStep, chaseSpeed, chaseSpeedSmoothTime);

  if (m_State.fDesiredSpeed > m_chaseSpeed)
    m_State.fDesiredSpeed = m_chaseSpeed;

}

const float CSpeedControl::m_CMaxDist = 3.0f;

//===================================================================
// SAIPotentialTarget::Serialize
//===================================================================
void SAIPotentialTarget::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.EnumValue("type", type, AITARGET_NONE, AITARGET_LAST);
	ser.Value("priority", priority);
	ser.Value("upPriority", upPriority);
	ser.Value("upPriorityTime", upPriorityTime);
	ser.Value("soundTime", soundTime);
	ser.Value("soundMaxTime", soundMaxTime);
	ser.Value("soundThreatLevel", soundThreatLevel);
	ser.Value("soundPos", soundPos);
	ser.Value("visualFrameId", visualFrameId);
	ser.Value("visualTime", visualTime);
	ser.Value("visualMaxTime", visualMaxTime);
	ser.Value("visualPos", visualPos);
	ser.EnumValue("visualType", visualType, VIS_NONE, VIS_LAST);
	ser.EnumValue("threat", threat, AITHREAT_NONE, AITHREAT_LAST);
	ser.Value("threatTime", threatTime);
	ser.Value("exposure", exposure);
	ser.Value("threatTimeout", threatTimeout);
	ser.Value("indirectSight", indirectSight);

	objectTracker.SerializeObjectPointer(ser, "pDummyRepresentation", pDummyRepresentation,false);
}

//===================================================================
// UpdateBeacon
//===================================================================
void CPuppet::UpdateBeacon()
{
	if (m_pAttentionTarget)
		GetAISystem()->UpdateBeacon(GetGroupId(), m_pAttentionTarget->GetPos(), m_pAttentionTarget);
	else if (m_pLastOpResult)
		GetAISystem()->UpdateBeacon(GetGroupId(), m_pLastOpResult->GetPos(), m_pLastOpResult);
}

//===================================================================
// CheckTargetInRange
//===================================================================
bool CPuppet::CheckTargetInRange(Vec3& vTargetPos)
{

	Vec3	vTarget = vTargetPos - GetPos();
	float	targetDist2 = vTarget.GetLengthSquared();

	// don't shoot if the target is not in range
	float fMinDistance = m_CurrentWeaponDescriptor.fRangeMin;// m_FireProperties.GetMinDistance();
	float fMaxDistance = m_CurrentWeaponDescriptor.fRangeMax;// m_FireProperties.GetMaxDistance();
	if(targetDist2 < fMinDistance*fMinDistance && fMinDistance>0)
	{
		if(!m_bWarningTargetDistance)
		{
			SetSignal(0, "OnTargetTooClose", GetEntity(), 0, g_crcSignals.m_nOnTargetTooClose);
			m_bWarningTargetDistance = true;
		}
		return false;
	}
	else if(targetDist2 > fMaxDistance*fMaxDistance && fMaxDistance>0)
	{
		if(!m_bWarningTargetDistance)
		{
			SetSignal(0, "OnTargetTooFar", GetEntity(), 0, g_crcSignals.m_nOnTargetTooFar);
			m_bWarningTargetDistance = true;
		}
		return false;
	}
	else
		m_bWarningTargetDistance = false;

	return true;
}


//===================================================================
// GetTargetTrackPoint
//===================================================================
IAIObject* CPuppet::GetTargetTrackPoint()
{
	if( !m_pTrackPattern )
		return 0;
  
	// Make sure the target is up to date.
	m_pTrackPattern->WakeUp();

	return m_pTrackPattern->GetTarget();
}

//===================================================================
// ResetSpeedControl
//===================================================================
void CPuppet::ResetSpeedControl()
{
	m_SpeedControl.Reset(GetPos(),GetAISystem()->GetFrameStartTime());
  m_chaseSpeed = 0.0f;
  m_chaseSpeedRate = 0.0f;
	m_lastChaseUrgencyDist = -1;
	m_lastChaseUrgencySpeed = -1;
}

//===================================================================
// AddNavigationBlockers
//===================================================================
void CPuppet::AddNavigationBlockers(NavigationBlockers& navigationBlockers, const struct PathFindRequest *pfr) const
{
	static float cost = 5.0f; //1000.0f;
	static bool radialDecay = true;
	static bool directional = true;

	TMapBlockers::const_iterator itr(m_PFBlockers.find(PFB_ATT_TARGET));
	float	curRadius(itr!=m_PFBlockers.end() ? (*itr).second : 0.f);
	float	sign(1.0f);
	if(curRadius < 0.0f) { sign = -1.0f; curRadius = -curRadius; }
	// see if attention target needs to be avoided
	if ( curRadius > 0.f &&
		m_pAttentionTarget && IsHostile(m_pAttentionTarget))
	{
		float r( curRadius );
		if (pfr)
		{
			static float extra = 1.5f;
			float d1 = extra * Distance::Point_Point(m_pAttentionTarget->GetPos(), pfr->startPos);
			float d2 = extra * Distance::Point_Point(m_pAttentionTarget->GetPos(), pfr->endPos);
			r = min(min(d1, d2), curRadius);
		}
		SNavigationBlocker	enemyBlocker(m_pAttentionTarget->GetPos(), r * sign, 0.f, cost, radialDecay, directional);
		navigationBlockers.push_back(enemyBlocker);
	}

	// avoid player
	itr = m_PFBlockers.find(PFB_PLAYER);
	curRadius = itr != m_PFBlockers.end() ? (*itr).second : 0.f;
	sign = 1.0f;
	if(curRadius < 0.0f) { sign = -1.0f; curRadius = -curRadius; }
	if (curRadius > 0.0f)
	{
		CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetAISystem()->GetPlayer());
		if (pPlayer)
		{
			SNavigationBlocker	blocker(pPlayer->GetPos() + pPlayer->GetBodyDir()*curRadius/2, curRadius * sign, 0.f, cost, radialDecay, directional);
			navigationBlockers.push_back(blocker);
		}
	}

	// avoid player
	itr = m_PFBlockers.find(PFB_BETWEEN_NAV_TARGET);
	curRadius = itr != m_PFBlockers.end() ? (*itr).second : 0.f;
	sign = 1.0f;
	if(curRadius < 0.0f) { sign = -1.0f; curRadius = -curRadius; }
	if (curRadius > 0.0f)
	{
		float biasTowardsTarget = 0.7f;
		Vec3 mid = pfr->endPos * biasTowardsTarget + GetPos() * (1 - biasTowardsTarget);
		curRadius = min(curRadius, Distance::Point_Point(pfr->endPos, GetPos()) * 0.8f);
		SNavigationBlocker	blocker(mid, curRadius * sign, 0.f, cost, radialDecay, directional);
		navigationBlockers.push_back(blocker);
	}


	// see if ref point needs to be avoided
	itr = m_PFBlockers.find(PFB_REF_POINT);
	curRadius = itr!=m_PFBlockers.end() ? (*itr).second : 0.f;
	sign = 1.0f;
	if(curRadius < 0.0f) { sign = -1.0f; curRadius = -curRadius; }
	if ( curRadius > 0.f )
	{
		float r( curRadius );
		if (pfr)
		{
			static float extra = 1.5f;
			float d1 = extra * Distance::Point_Point(m_pRefPoint->GetPos(), pfr->startPos);
			float d2 = extra * Distance::Point_Point(m_pRefPoint->GetPos(), pfr->endPos);
			r = min(min(d1, d2), curRadius);
		}
		SNavigationBlocker	enemyBlocker(m_pRefPoint->GetPos(), r, 0.f, cost * sign, radialDecay, directional);
		navigationBlockers.push_back(enemyBlocker);
	}

	itr = m_PFBlockers.find(PFB_BEACON);
	curRadius = itr!=m_PFBlockers.end() ? (*itr).second : 0.f;
	sign = 1.0f;
	if(curRadius < 0.0f) { sign = -1.0f; curRadius = -curRadius; }
	IAIObject* pBeacon;
	if ( curRadius > 0.f && (pBeacon=GetAISystem()->GetBeacon(GetGroupId())))
	{
		float r( curRadius );
		if (pfr)
		{
			static float extra = 1.5f;
			float d1 = extra * Distance::Point_Point(pBeacon->GetPos(), pfr->startPos);
			float d2 = extra * Distance::Point_Point(pBeacon->GetPos(), pfr->endPos);
			r = min(min(d1, d2), curRadius);

		}
		SNavigationBlocker	enemyBlocker(pBeacon->GetPos(), r, 0.f, cost * sign, radialDecay, directional);
		navigationBlockers.push_back(enemyBlocker);
	}

	// Avoid dead bodies
	itr = m_PFBlockers.find(PFB_DEAD_BODIES);
	float	deadRadius = itr != m_PFBlockers.end() ? (*itr).second : 0.f;
	itr = m_PFBlockers.find(PFB_EXPLOSIVES);
	float	explosiveRadius = itr != m_PFBlockers.end() ? (*itr).second : 0.f;

	if(fabsf(deadRadius) > 0.01f || fabsf(explosiveRadius) > 0.01f)
	{
		const unsigned int maxn = 3;
		Vec3	positions[maxn];
		unsigned int types[maxn];
		unsigned int n = GetAISystem()->GetDangerSpots(static_cast<const IAIObject*>(this), 40.0f, positions, types, maxn, IAISystem::DANGER_ALL);
		for(unsigned i = 0; i < n; i++)
		{
			float	r = explosiveRadius;
			if(types[i] == IAISystem::DANGER_DEADBODY)
				r = deadRadius;
			// Skip completely blocking blocking blockers which are too close.
			if (r < 0.0f && Distance::Point_PointSq(GetPos(), positions[i]) < sqr(fabsf(r) + 2.0f))
				continue;
			sign = 1.0f;
			if(r < 0.0f) { sign = -1.0f; r = -r; }
			SNavigationBlocker	enemyBlocker(positions[i], r, 0.f, cost * sign, radialDecay, directional);
			navigationBlockers.push_back(enemyBlocker);
		}
	}
}

//===================================================================
// SetPFBlockerRadius
//===================================================================
void CPuppet::SetPFBlockerRadius(int blockerType, float radius)
{
	m_PFBlockers[blockerType] = radius;
}

//===================================================================
// CanTargetPointBeReached
//===================================================================
ETriState CPuppet::CanTargetPointBeReached(CTargetPointRequest &request)
{
	m_DEBUGCanTargetPointBeReached.push_back(request.GetPosition());
	return m_Path.CanTargetPointBeReached(request, this, true);
}

//===================================================================
// UseTargetPointRequest
//===================================================================
bool CPuppet::UseTargetPointRequest(const CTargetPointRequest &request)
{
	m_DEBUGUseTargetPointRequest = request.GetPosition();
	return m_Path.UseTargetPointRequest(request, this, true);
}

//===================================================================
// CheckFriendsInLineOfFire
//===================================================================
bool CPuppet::CheckFriendsInLineOfFire(const Vec3& fireDirection, bool cheapTest)
{
	FUNCTION_PROFILER(GetAISystem()->m_pSystem, PROFILE_AI);

	if (m_updatePriority != AIPUP_VERY_HIGH)
		cheapTest = true;

	const Vec3& firePos = GetFirePos();
	bool friendOnWay = false;

	CAIPlayer *pPlayer = CastToCAIPlayerSafe(GetAISystem()->GetPlayer());
	if (pPlayer && !pPlayer->IsHostile(this))
	{
		if (IsFriendInLineOfFire(pPlayer, firePos, fireDirection, cheapTest))
			friendOnWay = true;
	}

	if (!friendOnWay)
	{
		const float checkRadiusSqr = sqr(fireDirection.GetLength() + 2.0f);

		const int species = GetParameters().m_nSpecies;

		const VectorSet<CPuppet*>& enabledPuppetsSet = GetAISystem()->GetEnabledPuppetSet();
		for (VectorSet<CPuppet*>::const_iterator it = enabledPuppetsSet.begin(), itend = enabledPuppetsSet.end(); it != itend; ++it)
		{
			CPuppet* pFriend = *it;
			// Skip for self
			if (!pFriend || pFriend == this)
				continue;
			if (IsHostile(pFriend)) continue;
			// Check against only puppets (ignore vehicles).
			if (pFriend->GetType() != AIOBJECT_PUPPET)
				continue;
			if (!pFriend->IsEnabled())
				continue;
			//FIXME this should never happen - puppet should always have proxy and physics - for Luciano to fix
			if (!pFriend->GetProxy() && !pFriend->GetPhysics())
				continue;
			if (Distance::Point_PointSq(pFriend->GetPos(), firePos) > checkRadiusSqr)
				continue;

			// Skip friends in vehicles.
			if (pFriend->GetProxy()->GetLinkedVehicleEntityId())
				continue;

			if (IsFriendInLineOfFire(pFriend, firePos, fireDirection, cheapTest))
			{
				friendOnWay = true;
				break;
			}
		}
	}


	if (friendOnWay)
		m_friendOnWayElapsedTime += GetAISystem()->GetUpdateInterval();
	else
		m_friendOnWayElapsedTime = 0.0f;

	return friendOnWay;
}

//===================================================================
// IsFriendInLineOfFire
//===================================================================
bool CPuppet::IsFriendInLineOfFire(CAIObject* pFriend, const Vec3& firePos, const Vec3& fireDirection, bool cheapTest) //, const Vec3& conePos, const Vec3& coneDir, float coneMinDistSqr, float coneMaxDistSqr)
{
	if (!pFriend->GetProxy())
		return false;
	IPhysicalEntity* pPhys = pFriend->GetProxy()->GetPhysics(true);
	if (!pPhys)
		pPhys = pFriend->GetPhysics();
	if (!pPhys)
		return false;

	const float detectionSide = 0.2f;
	Vec3 fudge(detectionSide,detectionSide,detectionSide);

	pe_status_pos statusPos;
	pPhys->GetStatus(&statusPos);
	AABB	bounds(statusPos.BBox[0] - fudge + statusPos.pos, statusPos.BBox[1] + fudge + statusPos.pos);

	bool hitBounds = Overlap::Lineseg_AABB(Lineseg(firePos, firePos + fireDirection), bounds);

	if (cheapTest)
		return hitBounds;

	if (!hitBounds)
		return false;

	ray_hit	hit;
	if (!GetAISystem()->GetPhysicalWorld()->CollideEntityWithBeam(pPhys, firePos, fireDirection, 0.2f, &hit))
		return false;

	// Send signal to the friend informing that a friendly agent is blocking the 
	if(m_friendOnWayCounter < 0.01f)
	{
		IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
		if(pData)
		{
			pData->point = pFriend->GetPhysicsPos();
			pe_status_dynamics  dSt;
			pFriend->GetPhysics()->GetStatus( &dSt );
			pData->fValue = dSt.v.GetLength();
		}
		SetSignal(1, "OnFriendInWay", pFriend->GetEntity(), pData, g_crcSignals.m_nOnFriendInWay);
		m_friendOnWayCounter = 2.0f;
	}

	return true;
}

//===================================================================
// GetFloorPosition
//===================================================================
Vec3 CPuppet::GetFloorPosition(const Vec3& pos)
{
	Vec3	floorPos(pos);
	if(GetFloorPos(floorPos, pos, walkabilityFloorUpDist, walkabilityFloorDownDist, walkabilityDownRadius, AICE_STATIC))
		return floorPos;
	return pos;
}

//===================================================================
// AdjustPathAroundObstacles
//===================================================================
bool CPuppet::AdjustPathAroundObstacles()
{
	FUNCTION_PROFILER(GetAISystem()->m_pSystem, PROFILE_AI);
  if (GetAISystem()->m_cvAdjustPathsAroundDynamicObstacles->GetIVal() == 0)
    return true;

  m_Path.ClearObjectsAdjustedFor();
  m_OrigPath.ClearObjectsAdjustedFor();

  CalculatePathObstacles();

  const CPathObstacles::TAIPathObjects &AIObstacles = m_pathAdjustmentObstacles.GetAIObstacles();
  for (CPathObstacles::TAIPathObjects::const_iterator it = AIObstacles.begin() ; it != AIObstacles.end() ; ++it)
    m_Path.AddObjectAdjustedFor(*it);

  return m_Path.AdjustPathAroundObstacles(m_pathAdjustmentObstacles, m_movementAbility.pathfindingProperties.navCapMask);
}

//===================================================================
// CalculatePathObstacles
//===================================================================
void CPuppet::CalculatePathObstacles()
{
  m_pathAdjustmentObstacles.CalculateObstaclesAroundPuppet(this);
}

//===================================================================
// SetCloseContact
//===================================================================
void CPuppet::SetCloseContact(bool bContact) 
{
	if(bContact && !m_bCloseContact)
		m_CloseContactTime = GetAISystem()->GetFrameStartTime();
	m_bCloseContact = bContact;
}

//===================================================================
// UpdateEntitiesToSkipInPathfinding
//===================================================================
void CPuppet::UpdateEntitiesToSkipInPathfinding()
{

}

//===================================================================
// GetShootingStatus
//===================================================================
void CPuppet::GetShootingStatus(SShootingStatus& ss)
{
	ss.fireMode = m_fireMode;
	ss.timeSinceTriggerPressed = m_timeSinceTriggerPressed;
	ss.triggerPressed = m_State.fire;
	ss.friendOnWay = m_friendOnWayElapsedTime > 0.001f;
	ss.friendOnWayElapsedTime = m_friendOnWayElapsedTime;
}

//===================================================================
// GetDistanceAlongPath
//===================================================================
float CPuppet::GetDistanceAlongPath(const Vec3& pos, bool bInit)
{
	Vec3 myPos(GetEntity() ? GetEntity()->GetPos() : GetPos());

	if(m_nPathDecision != PATHFINDER_PATHFOUND) 
		return 0;

	if(bInit)
	{
		m_InitialPath.clear();
		TPathPoints::const_iterator liend = m_OrigPath.GetPath().end();
		for(TPathPoints::const_iterator li =m_OrigPath.GetPath().begin();li!=liend;++li)
			m_InitialPath.push_back(li->vPos);

	}
	if(!m_InitialPath.empty())
	{
		float mindist = 10000000.f;
		float mindistObj = 10000000.f;
		float totdist = 0;
		TPointList::const_iterator listart =m_InitialPath.begin();
		TPointList::const_iterator li =listart;
		TPointList::const_iterator liend = m_InitialPath.end();
		TPointList::const_reverse_iterator lilast = m_InitialPath.rbegin();
		TPointList::const_iterator linext = li;
		TPointList::const_iterator liMyPoint = liend;
		TPointList::const_iterator liObjPoint = liend;
		Vec3 p1(ZERO);
		++linext;
		int count = 0;
		int myCount=0;
		int objCount = 0;
		float objCoeff=0;
		int maxCount = m_InitialPath.size()-1;
		for(;linext!=liend;++li,++linext)
		{
			float t,u;
			float distObj = Distance::Point_Lineseg(pos,Lineseg(*li,*linext),t);
			float mydist = Distance::Point_Lineseg(myPos,Lineseg(*li,*linext),u);
			if(distObj<mindistObj && (count==0 && t<0 || t>=0 && t<=1 || count==maxCount && t>1))
			{
				liObjPoint = li;
				mindistObj = distObj;
				objCount = count;
				objCoeff = t;
			}
			if(mydist<mindist && (count==0 && u<0 || u>=0 && u<=1 || count==maxCount && u>1))
			{
				liMyPoint = li;
				mindist = mydist;
				myCount = count;
			}
			count++;
		}
		
		
		// check if object is outside the path
		if(objCoeff<=0 && objCount==0)
			return Distance::Point_Point(pos,*listart) + Distance::Point_Point(myPos,*listart);
		else if(objCoeff>=1 && objCount>=count-1)
			return -(Distance::Point_Point(pos,*lilast) + Distance::Point_Point(myPos,*lilast));

		if(liMyPoint!=liend && liObjPoint != liend && liMyPoint!=liObjPoint)	
		{
			if(objCount>myCount)
			{
				// other object is ahead
				++liMyPoint;
				float dist = Distance::Point_Point(*liObjPoint,pos);
				if(liMyPoint!=liend)
					dist += Distance::Point_Point(*liMyPoint,myPos);

				li = liMyPoint;
				linext = li;
				++linext;

				for(;li!=liObjPoint && linext!=liend; ++li,++linext)
					dist += Distance::Point_Point(*li,*linext);

				return -dist;
			}
			else
			{
				// other object is back
				++liObjPoint;
				float dist = Distance::Point_Point(*liMyPoint,myPos);
				if(liObjPoint!=liend)
					dist += Distance::Point_Point(*liObjPoint,pos);

				li = liObjPoint;
				linext = li;
				++linext;

				for(;li!=liMyPoint && linext!=liend; ++li,++linext)
					dist += Distance::Point_Point(*li,*linext);

				return dist;
			}
		}
		// check just positions, object is on the same path segment
		Vec3 myOrientation(ZERO);
		if(GetPhysics())
		{
			pe_status_dynamics  dSt;
			GetPhysics()->GetStatus( &dSt );
			myOrientation = dSt.v;
		}
		if(myOrientation.IsEquivalent(ZERO))
			myOrientation = GetViewDir();

		Vec3 dir = pos-myPos;
		float dist= dir.GetLength();

		return(myOrientation.Dot(dir) <0  ? dist:-dist);

	}
	// no path
	return 0;
}

//===================================================================
// ClearPotentialTargets
//===================================================================
void CPuppet::ClearPotentialTargets() 
{
	// The reason for this slightly weird deletion is that the RemoveDummyObject() will
	// call the OnObjectRemoved of this puppet which in turn tries to erase the event.
	while (!m_perceptionEvents.empty())
	{
		SAIPotentialTarget& ed = m_perceptionEvents.begin()->second;
		CAIObject	*pDummyRepresentation = ed.pDummyRepresentation;
		m_perceptionEvents.erase(m_perceptionEvents.begin());
		if (pDummyRepresentation)
			GetAISystem()->RemoveDummyObject(pDummyRepresentation);
	}
}

//===================================================================
// SetTerritoryShapeName
//===================================================================
void CPuppet::SetTerritoryShapeName(const char* shapeName)
{
	// Set and resolve the shape name.
	m_territoryShapeName = shapeName;
	m_territoryShape = GetAISystem()->GetGenericShapeOfName(m_territoryShapeName.c_str());
	if (!m_territoryShape)
		m_territoryShapeName.clear();
	// Territory shapes should be really simple
	if (m_territoryShape && m_territoryShape->shape.size() > 8)
		AIWarning("Territory shape %s for %s has %d points. Territories should not have more than 8 points", m_territoryShapeName.c_str(), GetName(), m_territoryShape->shape.size());
}

//===================================================================
// GetTerritoryShapeName
//===================================================================
const char* CPuppet::GetTerritoryShapeName()
{
	return m_territoryShapeName.c_str();
}

//===================================================================
// GetTerritoryShape
//===================================================================
SShape* CPuppet::GetTerritoryShape()
{
	return m_territoryShape;
}

//===================================================================
// GetDamageParts
//===================================================================
DamagePartVector* CPuppet::GetDamageParts()
{
	UpdateDamageParts(m_damageParts);
	return &m_damageParts;
}

//===================================================================
// GetValidPositionNearby
//===================================================================
bool CPuppet::GetValidPositionNearby(const Vec3 &proposedPosition, Vec3 &adjustedPosition) const
{
  adjustedPosition = proposedPosition;
  if (!GetFloorPos(adjustedPosition, proposedPosition, 1.0f, 2.0f, walkabilityDownRadius, AICE_ALL))
    return false;

  static float maxFloorDeviation = 1.0f;
  if (fabsf(adjustedPosition.z - proposedPosition.z) > maxFloorDeviation)
    return false;

  if (!CheckBodyPos(adjustedPosition, AICE_ALL))
    return false;

  unsigned nodeIndex = GetAISystem()->GetGraph()->GetEnclosing(
    adjustedPosition, m_movementAbility.pathfindingProperties.navCapMask, m_Parameters.m_fPassRadius,
		m_lastNavNodeIndex);

  return nodeIndex != 0;
}

//===================================================================
// GetTeleportPosition
//===================================================================
bool CPuppet::GetTeleportPosition(Vec3 &teleportPos) const
{
  Vec3 curPos = GetPos();
  teleportPos.zero();

  IAISystem::tNavCapMask navCapMask = m_movementAbility.pathfindingProperties.navCapMask;
  navCapMask &= IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN;

  int nBuildingID;
  IVisArea *pArea;
  IAISystem::ENavigationType currentNavType = GetAISystem()->CheckNavigationType(curPos, nBuildingID, pArea, navCapMask);

  GetAISystem()->GetNavRegion(currentNavType, GetAISystem()->GetGraph())->GetTeleportPosition(curPos, teleportPos, GetName());

  if (!teleportPos.IsZero())
    return true;
  else
    return false;
}

//===================================================================
// SelectAimPosture
//===================================================================
bool CPuppet::SelectAimPosture(const Vec3& targetPos, EStance& outStance, float& outLean, bool allowLean, bool allowProne)
{
	struct SPosture
	{
		SPosture(float ln = 0.0f, EStance st = STANCE_STAND, int p = -1) :
			lean(ln), stance(st), targetVis(false), targetAim(false), parent(p),
			valid(false), eye(0,0,0), weapon(0,0,0) {}

		void	Set(float ln, EStance st, int p = -1)
		{
			lean = ln;
			stance = st;
			valid = false;
			parent = p;
		}

		float		lean;					// Leaning of the posture.
		EStance	stance;				// Stance of the posture.
		Vec3		eye, weapon;	// Updated eye and weapon position.
		Vec3		weaponDir;		// Updated weapon direction.
		int			parent;				// Parent state for leans, to check if the lean is possible.
		bool		targetVis;		// Target visible from this posture.
		bool		targetAim;		// Target aimable from this posture.
		bool		valid;				// Posture valid.
	};

	const unsigned	nPostures = 8;
	SPosture	postures[nPostures];

	// The postures are evaluated and selected based on the order
	// they appear in the list. For that reason the leans are
	// randomized so that they do not always select the same side.
	unsigned n = 0;
//	if (allowProne)

	int crouch = -1;
	int stand = -1;

	if (allowProne)
	{
		postures[n].Set(0.0f, STANCE_PRONE); n++;
	}
	crouch = (int)n;
	postures[n].Set(0.0f, STANCE_CROUCH); n++;
	stand = (int)n;
	postures[n].Set(0.0f, STANCE_STAND); n++;

	if (allowLean)
	{
		if(ai_frand() > 0.5f)
		{
			postures[n].Set( 1.0f, STANCE_CROUCH, crouch); n++;
			postures[n].Set(-1.0f, STANCE_CROUCH, crouch); n++;
		}
		else
		{
			postures[n].Set(-1.0f, STANCE_CROUCH, crouch); n++;
			postures[n].Set( 1.0f, STANCE_CROUCH, crouch); n++;
		}

		if(ai_frand() > 0.5f)
		{
			postures[n].Set(-1.0f, STANCE_STAND, stand); n++;
			postures[n].Set( 1.0f, STANCE_STAND, stand); n++;
		}
		else
		{
			postures[n].Set( 1.0f, STANCE_STAND, stand); n++;
			postures[n].Set(-1.0f, STANCE_STAND, stand); n++;
		}
	}

	// Allow prone if nothing else works out
	if (!allowProne)
	{
		postures[n].Set(0.0f, STANCE_PRONE, !allowProne); n++;
	}

	ray_hit	hit;

	float	distToTarget = Distance::Point_Point(targetPos, GetPos());

	int	targetVis = -1;
	int	targetVisAim = -1;
	int emergancy = -1;

	bool	isInDoors = false;
	if(m_lastNavNodeIndex && (GetAISystem()->GetGraph()->GetNodeManager().GetNode(m_lastNavNodeIndex)->navType & IAISystem::NAV_WAYPOINT_HUMAN))
		isInDoors = true;

	// Update the postures.
	for(unsigned i = 0; i < n; ++i)
	{
		if(postures[i].stance == STANCE_PRONE)
		{
			if (isInDoors)
			{
				postures[i].valid = false;
				continue;
			}
			else
			{
				float checkDist = allowProne ? 50.0f : 8.0f;
				if (distToTarget < checkDist)
				{
					postures[i].valid = false;
					continue;
				}
			}
		}
		if(postures[i].stance == STANCE_CROUCH && distToTarget < 5.0f)
		{
			postures[i].valid = false;
			continue;
		}
		if(postures[i].stance == STANCE_STEALTH && distToTarget < 3.5f)
		{
			postures[i].valid = false;
			continue;
		}

		SAIBodyInfo bi;
		if(GetProxy()->QueryBodyInfo(postures[i].stance, postures[i].lean, false, bi))
		{
			postures[i].eye = bi.vEyePos;
			postures[i].weapon = bi.vFirePos;
			postures[i].weaponDir = bi.vFireDir;

			postures[i].targetVis = true;
			postures[i].targetAim = true;

			// Try to avoid humans.
			Vec3	dir = targetPos - postures[i].weapon;
			Ray	fireRay(postures[i].weapon, dir);

			AutoAIObjectIter it(GetAISystem()->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIOBJECT_PUPPET));
			for(; it->GetObject(); it->Next())
			{
				IAIObject*	pObject = it->GetObject();
				// skip disabled
				if(!pObject->IsEnabled()) continue;

				CPuppet*	pPuppet = pObject->CastToCPuppet();
				if(!pPuppet)
					continue;

				if(pPuppet == this) continue;

				if(Distance::Point_PointSq(GetPos(), pPuppet->GetPos()) > sqr(7.0f))
					continue;

				// Hit test against the collider (not anim skeleton).
				ray_hit	hit;
				if(GetAISystem()->GetPhysicalWorld()->CollideEntityWithBeam(pPuppet->GetPhysics(false),
					postures[i].weapon, dir, 0.05f, &hit))
				{
					postures[i].targetAim = false;
					break;
				}

				// Additional check for the head, in case of leaning.
				if(fabsf(pPuppet->GetState()->lean) > 0.01f)
				{
					// The reason using the weapon position here is that it is close to the body and 
					// better represents the leaned out upper body.
					Vec3	toeToHead = pPuppet->GetPos() - pPuppet->GetPhysicsPos();
					float	len = toeToHead.GetLength();
					if(len > 0.0001f)
						toeToHead *= (len - 0.3f) / len;
					Vec3	hit0, hit1;
					if(Intersect::Ray_Sphere(fireRay, Sphere(pPuppet->GetPhysicsPos() + toeToHead, 0.4f + 0.05f), hit0, hit1))
					{
						postures[i].targetAim = false;
						break;
					}
				}
			}

			// Check if the path from the non-lean state to the lean state is clear.
			if(postures[i].parent != -1)
			{
				if(gEnv->pPhysicalWorld->RayWorldIntersection(postures[postures[i].parent].eye,
					postures[postures[i].parent].eye - postures[i].eye,
					COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1))
				{
					postures[i].valid = false;
					continue;
				}
				// TODO: Do the simple sphere test as above too with friends.
			}

			// Check if the aiming and looking is obstructed from the changed stance.
			// checking just half the distance - not to shoot long rays
			dir = targetPos - postures[i].eye;
			if(gEnv->pPhysicalWorld->RayWorldIntersection(postures[i].eye, dir*.5f, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1))
				postures[i].targetVis = false;

			dir = targetPos - postures[i].weapon;
			// checking just half the distance - not to shoot long rays
			if(gEnv->pPhysicalWorld->RayWorldIntersection(postures[i].weapon, dir*.5f, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1))
					postures[i].targetAim = false;

			postures[i].valid = true;

			// Choose if the target is good to use.
			if(targetVisAim == -1 && postures[i].targetVis && postures[i].targetAim)
			{
				// Could stop updating here already. - so, let's break
				targetVisAim = i;
				break;
			}
			if(targetVis == -1 && postures[i].targetVis && fabsf(postures[i].lean) < 0.01f)
				targetVis = i;
		}
		else
		{
			postures[i].valid = false;
		}
	}

	if(GetFireMode() != FIREMODE_OFF)
	{
		if(targetVisAim != -1)
		{
			outStance = postures[targetVisAim].stance;
			outLean = postures[targetVisAim].lean;
			return true;
		}
	}
	else
	{
		if(targetVisAim != -1)
		{
			outStance = postures[targetVisAim].stance;
			outLean = postures[targetVisAim].lean;
			return true;
		}
		else if(targetVis != -1)
		{
			outStance = postures[targetVis].stance;
			outLean = postures[targetVis].lean;
			return true;
		}
	}

	return false;
}

//===================================================================
// SelectHidePosture
//===================================================================
bool CPuppet::SelectHidePosture(const Vec3& targetPos, EStance& outStance, float& outLean, bool allowLean, bool allowProne)
{
	struct SPosture
	{
		SPosture(float ln = 0.0f, EStance st = STANCE_STAND, int p = -1) :
			lean(ln), stance(st), targetVis(false), targetAim(false), parent(p),
			valid(false), eye(0,0,0), weapon(0,0,0) {}

		void	Set(float ln, EStance st, int p = -1) { lean = ln; stance = st; valid = false; parent = p; }

		float		lean;					// Leaning of the posture.
		EStance	stance;				// Stance of the posture.
		Vec3		eye, weapon;	// Updated eye and weapon position.
		Vec3		weaponDir;		// Updated weapon direction.
		int			parent;				// Parent state for leans, to check if the lean is possible.
		bool		targetVis;		// Target visible from this posture.
		bool		targetAim;		// Target aimable from this posture.
		bool		valid;				// Posture valid.
	};

	const unsigned	nPostures = 4;
	SPosture	postures[nPostures];

	// The postures are evaluated and selected based on the order
	// they appear in the list. For that reason the leans are
	// randomized so that they do not always select the same side.
	unsigned n = 0;
	if (allowProne)
	{
		postures[n].Set(0.0f, STANCE_PRONE); n++;
	}
	postures[n].Set(0.0f, STANCE_CROUCH); n++;
	postures[n].Set(0.0f, STANCE_STEALTH); n++;
	postures[n].Set(0.0f, STANCE_STAND); n++;

	float	distToTarget = Distance::Point_Point(targetPos, GetPos());

	int	targetVis = -1;
	int	targetVisAim = -1;

	ray_hit hit;

	// Update the postures.
	for(unsigned i = 0; i < n; ++i)
	{
		if(postures[i].stance == STANCE_PRONE && distToTarget < 25.0f)
		{
			postures[i].valid = false;
			continue;
		}
		if(postures[i].stance == STANCE_CROUCH && distToTarget < 5.0f)
		{
			postures[i].valid = false;
			continue;
		}
		if(postures[i].stance == STANCE_STEALTH && distToTarget < 3.5f)
		{
			postures[i].valid = false;
			continue;
		}

		SAIBodyInfo bi;
		if(GetProxy()->QueryBodyInfo(postures[i].stance, postures[i].lean, false, bi))
		{
			postures[i].eye = bi.vEyePos;
			postures[i].weapon = bi.vFirePos;
			postures[i].weaponDir = bi.vFireDir;

			postures[i].targetVis = true;
			postures[i].targetAim = true;

			// Check if the path from the non-lean state to the lean state is clear.
			if(postures[i].parent != -1)
			{
				if(gEnv->pPhysicalWorld->RayWorldIntersection(postures[postures[i].parent].eye,
					postures[postures[i].parent].eye - postures[i].eye,
					COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1))
				{
					postures[i].valid = false;
					continue;
				}
				// TODO: Do the simple sphere test as above too with friends.
			}

			Vec3	dir;

			// Check if the aiming and looking is obstructed from the changed stance.
			dir = targetPos - postures[i].eye;
			// checking just half the distance - not to shoot long rays
			if(gEnv->pPhysicalWorld->RayWorldIntersection(postures[i].eye, dir*.5f, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1))
				postures[i].targetVis = false;

			dir = targetPos - postures[i].weapon;
			// checking just half the distance - not to shoot long rays
			if(gEnv->pPhysicalWorld->RayWorldIntersection(postures[i].weapon, dir*.5f, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1))
				postures[i].targetAim = false;

			postures[i].valid = true;
			// Choose if the target is good to use.
			if(targetVisAim == -1 && !postures[i].targetVis && !postures[i].targetAim)
			{
				// Could stop updating here already. - let's break
				targetVisAim = i;
				break;
			}
			if(targetVis == -1 && !postures[i].targetVis && fabsf(postures[i].lean) < 0.01f)
				targetVis = i;
		}
		else
		{
			postures[i].valid = false;
		}
	}

	if(targetVisAim != -1)
	{
		outStance = postures[targetVisAim].stance;
		outLean = postures[targetVisAim].lean;
		return true;
	}
	else if(targetVis != -1)
	{
		outStance = postures[targetVis].stance;
		outLean = postures[targetVis].lean;
		return true;
	}

	return false;
}


//===================================================================
// SetAllowedStrafeDistances
//===================================================================
void CPuppet::SetAllowedStrafeDistances(float start, float end, bool whileMoving)
{
	m_allowedStrafeDistanceStart = start; 
	m_allowedStrafeDistanceEnd = end;
	m_allowStrafeLookWhileMoving = whileMoving;

	m_strafeStartDistance = 0.0f;

	UpdateStrafing();
}

//===================================================================
// SetAdaptiveMovementUrgency
//===================================================================
void CPuppet::UpdateStrafing()
{
	m_State.allowStrafing = false;
	if (m_State.fDistanceToPathEnd > 0)
	{
//		if (!m_State.vAimTargetPos().IsZero())
		{
			if (m_allowedStrafeDistanceStart > 0.001f)
			{
				// Calculate the max travelled distance.
				float	distanceMoved = m_OrigPath.GetPathLength(false) - m_State.fDistanceToPathEnd;
				m_strafeStartDistance = max(m_strafeStartDistance, distanceMoved);

				if (m_strafeStartDistance < m_allowedStrafeDistanceStart)
					m_State.allowStrafing = true;
			}

			if (m_allowedStrafeDistanceEnd > 0.001f)
			{
				if (m_State.fDistanceToPathEnd < m_allowedStrafeDistanceEnd)
					m_State.allowStrafing = true;
			}
		}
	}
}

//===================================================================
// SetAdaptiveMovementUrgency
//===================================================================
void CPuppet::SetAdaptiveMovementUrgency(float minUrgency, float maxUrgency, float scaleDownPathlen)
{
	m_adaptiveUrgencyMin = minUrgency;
	m_adaptiveUrgencyMax = maxUrgency;
	m_adaptiveUrgencyScaleDownPathLen = scaleDownPathlen;
	m_adaptiveUrgencyMaxPathLen = 0.0f;
}

//===================================================================
// SetDelayedStance
//===================================================================
void CPuppet::SetDelayedStance(int stance)
{
	m_delayedStance = stance;
	m_delayedStanceMovementCounter = 0;
}

//===================================================================
// GetPhysics
//===================================================================
IPhysicalEntity* CPuppet::GetPhysics(bool wantCharacterPhysics) const
{
	if(GetProxy() && GetProxy()->GetPhysics(wantCharacterPhysics))
		return GetProxy()->GetPhysics(wantCharacterPhysics);

//	AIWarning("CPuppet::GetPhysics Puppet %s does not have physics!", GetName());
//	AIAssert(0);

	return NULL;
}

//===================================================================
// GetPosAlongPath
//===================================================================
bool CPuppet::GetPosAlongPath(float dist, bool extrapolateBeyond, Vec3& retPos) const
{
	return m_Path.GetPosAlongPath(retPos, dist, !m_movementAbility.b3DMove, extrapolateBeyond);
}

//===================================================================
// CheckCloseContact
//===================================================================
void CPuppet::CheckCloseContact(CPuppet* pEnemyPuppet, float dist) 
{
FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );
	if (GetAttentionTarget() == pEnemyPuppet && dist < GetParameters().m_fMeleeDistance && !m_bCloseContact)
	{
		SetSignal(1, "OnCloseContact",pEnemyPuppet->GetEntity(), 0, g_crcSignals.m_nOnCloseContact);		
		SetCloseContact(true);
	}
}
