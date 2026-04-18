//--------------------------------------------------------------------------------

#include "StdAfx.h"
#include "AnimatedCharacter.h"
#include "CryAction.h"
#include "AnimationGraphManager.h"
#include "AnimationGraphCVars.h"
#include "HumanBlending.h"
#include "PersistantDebug.h"
#include "IFacialAnimation.h"

#include "DebugHistory.h"

#include <IViewSystem.h>


//--------------------------------------------------------------------------------

const char* g_szInputIDStr[eACInputIndex_COUNT] =
{
	"RequestedMoveSpeedLX",
	"RequestedMoveSpeedLY",
	"RequestedMoveSpeedLH",
	"RequestedMoveDir4LH",
	"RequestedMoveSpeedLZ",
	"RequestedMoveSpeedWH",
	"RequestedMoveSpeedWV",
	"RequestedMoveSpeed",
	"RequestedTurnSpeedLZ",

	"ActualMoveSpeedLX",
	"ActualMoveSpeedLY",
	"ActualMoveSpeedLH",
	"ActualMoveDir4LH",
	"ActualMoveSpeedLZ",
	"ActualMoveSpeedWH",
	"ActualMoveSpeedWV",
	"ActualMoveSpeed",
	"ActualTurnSpeedLZ",

	"AnimPhase",
	"Action",
	"PseudoSpeed",
	"Stance",
};

//--------------------------------------------------------------------------------

bool CAnimatedCharacter::EvaluateSimpleMovementConditions()
{
	IEntity* pEntity = GetEntity();
	assert(pEntity);

	bool debug = false;
	if(gEnv->pConsole->GetCVar("es_logDrawnActors")->GetIVal() != 0)
		debug = true;

	if(debug)
		CryLogAlways("EvaluateSimpleMovementConditions() for %s: current is %d", pEntity->GetName(), m_simplifyMovement);

	if ((m_pAnimTarget != NULL) && m_pAnimTarget->activated)
	{
		if(debug)
			CryLogAlways("==== Anim target activated, returning false");
		return false;
	}

	if (GetMCMH() == eMCM_Animation)
	{
		if(debug)
			CryLogAlways("==== GetMCMH() == eMCM_Animation: returning false");
		return false;
	}

	if (pEntity->IsHidden() && !(pEntity->GetFlags() & ENTITY_FLAG_UPDATE_HIDDEN))
	{
		if(debug)
			CryLogAlways("==== Entity Hidden: %d, Hidden flag: %d, returning true", pEntity->IsHidden(), (pEntity->GetFlags() & ENTITY_FLAG_UPDATE_HIDDEN));
		return true;
	}

	if (gEnv->pSystem->IsDedicated())
	{
		if(debug)
			CryLogAlways("==== dedicated server, returning true");
		return true;
	}

	if (CAnimationGraphCVars::Get().m_forceSimpleMovement != 0)
	{
		if(debug)
			CryLogAlways("==== forcing simple movement, returning true");
		return true;
	}

	if(CAnimationGraphCVars::Get().m_forceNoSimpleMovement != 0)
	{
		if(debug)
			CryLogAlways("==== forcing not simple movement, returning false");
		return false;
	}

	if ((m_pCharacter == NULL) || !m_pCharacter->IsCharacterVisible())
	{
		if(debug)
			CryLogAlways("==== Character %s, returning true", m_pCharacter == NULL ? "NULL" : "Not visible");
		return true;
	}

	if(debug)
		CryLogAlways("==== end of function, returning false");
	return false;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::UpdateSimpleMovementConditions()
{
	bool simplifyMovement = EvaluateSimpleMovementConditions();
	bool forceDisableSlidingContactEvents = (CAnimationGraphCVars::Get().m_disableSlidingContactEvents != 0);

	// TODO: For some reason the current frame id is always one more than the last reset frame id, should be the same, afaik.
	if ((m_simplifyMovement != simplifyMovement) || 
		(m_forceDisableSlidingContactEvents != forceDisableSlidingContactEvents) ||
		((m_lastResetFrameId+5) >= m_curFrameID) ||
		!m_bSimpleMovementSetOnce) // Loading a singleplayer map in game takes too long between character reset and update.
	{
		m_bSimpleMovementSetOnce = true;

		m_forceDisableSlidingContactEvents = forceDisableSlidingContactEvents;
		m_simplifyMovement = simplifyMovement;

		if (HasSplitUpdate())
			GetGameObject()->SetUpdateSlotEnableCondition(this, 0, m_simplifyMovement ? eUEC_Never : eUEC_Always);
		else
			GetGameObject()->SetUpdateSlotEnableCondition(this, 0, eUEC_Always);

		IEntity* pEntity = GetEntity();
		//string name = pEntity->GetName();
		//CryLogAlways("AC[%s]: simplified movement %s!", name, (m_simplifyMovement ? "enabled" : "disabled"));
		IPhysicalEntity *pPhysEnt = pEntity->GetPhysics();
		if ((pPhysEnt != NULL) && (pPhysEnt->GetType() == PE_LIVING))
		{
			pe_params_flags pf;

			if (m_simplifyMovement || m_forceDisableSlidingContactEvents)
			{
				pf.flagsAND = ~lef_report_sliding_contacts;
				//CryLogAlways("AC[%s]: events disabled!", name);
			}
			else
			{
				pf.flagsOR = lef_report_sliding_contacts;
				//CryLogAlways("AC[%s]: events enabled!", name);
			}

			pPhysEnt->SetParams(&pf);
		}		
	}
}

//--------------------------------------------------------------------------------

bool CAnimatedCharacter::UpdateAnimGraphSleepTracking(float frameTime)
{
	if (m_simplifyMovement && m_actualEntMovement.IsIdentity())
	{
		assert(m_prevAnimLocation.q.IsValid());
		QuatT actualAnimMovement = GetWorldOffset(m_prevAnimLocation, m_animLocation);

		if (actualAnimMovement.IsIdentity())
		{
			static float SleepOnNoMovementTimeThreshold = 1.0f; // If not moving for one second, go to sleep.
			if (m_noMovementTimer >= SleepOnNoMovementTimeThreshold)
			{
				m_noMovementTimer = SleepOnNoMovementTimeThreshold;
				m_sleepAnimGraph = true;
				return true;
			}
			else
			{
				m_noMovementTimer += frameTime;
			}
		}
	}
	else
	{
		m_noMovementTimer = 0.0f;
	}

	if (m_sleepAnimGraph)
	{
		//m_animationGraphStates.SetCatchupFlag(); // TODO: This does not work properly, removed until it's fixed.
		m_sleepAnimGraph = false;
	}

	return false;
}

//--------------------------------------------------------------------------------

int AnimatedCharacter_PostAnimationUpdate_Global(ICharacterInstance* pCharacterInstance, void* pAnimatedCharacter)
{
	((CAnimatedCharacter*)pAnimatedCharacter)->PostAnimationUpdate();
	return 1; 
}
int AnimatedCharacter_PostProcessingUpdate_Global(ICharacterInstance* pCharacterInstance, void* pAnimatedCharacter)
{
	((CAnimatedCharacter*)pAnimatedCharacter)->PostProcessingUpdate();
	return 1; 
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//- PreAnimationUpdate -----------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

void CAnimatedCharacter::PreAnimationUpdate()
{
	ANIMCHAR_PROFILE;

#if _DEBUG && defined(USER_david)
	SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

#ifdef DEBUGHISTORY
	SetupDebugHistories();
#endif

#ifdef _DEBUG
	RunTests();
#endif

	//IEntity* pEntity = GetEntity();
	//assert(pEntity);

	UpdateTime();
	if (m_curFrameTime <= 0.0f)
		return;

	AcquireRequestedBehaviourMovement();

	UpdateAnimGraphSleepTracking(m_curFrameTime);

	if (!m_simplifyMovement)
	{
		SetAnimGraphSpeedInputs(m_requestedEntityMovement, m_entLocation, m_curFrameTime, eACInputIndex_RequestedSpeedBase);
		SetAnimGraphSpeedInputs(m_actualEntMovement, m_prevEntLocation, m_prevFrameTime, eACInputIndex_ActualSpeedBase);
	}

	// Special case for grabbed characters...
	if (((m_updateGrabbedInputFrameID + 5) < m_curFrameID) || RecentQuickLoad() || m_bGrabbedInViewSpace)
	{
		ANIMCHAR_PROFILE_SCOPE("PreAnimationUpdate_UpdateGrabbedAGInput");

		m_updateGrabbedInputFrameID = m_curFrameID;

		char action[256];
		m_animationGraphStates.GetInput(m_inputID[eACInputIndex_Action], action);
		bool grabbed = (strcmp(action, "grabStruggleFP") == 0) || (strcmp(action, "grabStruggleFP2") == 0);
		m_bGrabbedInViewSpace = grabbed;
	}

	if (((m_updateSkeletonSettingsFrameID + 500) < m_curFrameID) || RecentQuickLoad())
	{
		ANIMCHAR_PROFILE_SCOPE("PreAnimationUpdate_UpdateSkeletonSettings");

		m_updateSkeletonSettingsFrameID = m_curFrameID;

		if (m_pSkeletonAnim != NULL)
		{
			m_pSkeletonPose->SetFootAnchoring(1); // Tell motion playback to calculate foot anchoring adjustments.
			m_pSkeletonAnim->SetAnimationDrivenMotion(1); // Tell motion playback to calculate root/locator trajectory.

			// Set aim smoothing to zero, to prevent 3rd person shadow lagging behind (local player only).
			if (m_isPlayer && m_isClient)
				m_pSkeletonPose->SetAimIKTargetSmoothTime(0.0f);

			// PostAnimationUpdate() is called from main Update when pre/post is merged.
			//if (SplitPrePostPhysicsUpdate())
			//	m_pSkeleton->SetPreProcessCallback(AnimatedCharacter_PostAnimationUpdate_Global, this);
			//else
			m_pSkeletonAnim->SetPreProcessCallback(NULL, this);
		}
	}


	//use ground alignment only when character is close to the camera
	if (m_pSkeletonPose)
	{
		int (*callback)(ICharacterInstance*, void*) = AnimatedCharacter_PostProcessingUpdate_Global;

		if (m_simplifyMovement)
		{
			callback = NULL;
		}
		else
		{
			//check if player is close enough
			CCamera& camera = gEnv->pSystem->GetViewCamera();
			f32 fDistance	=	(camera.GetPosition() - m_animLocation.t).GetLength();

			if (fDistance > 35.0f)
			{
				callback = NULL;
			}
			else if (GetEntity()->GetParent() != NULL)
			{
				callback = NULL;
			}
			else if ((GetCurrentStance() == STANCE_SWIM) || 
				(GetCurrentStance() == STANCE_ZEROG) || 
				(GetCurrentStance() == STANCE_PRONE))
			{
				callback = NULL;
			}
			else if (NoMovementOverride())
			{
				callback = NULL;
			}
			else if ((m_colliderMode == eColliderMode_Disabled) || (m_colliderMode == eColliderMode_PushesPlayersOnly))
			{
				if(!m_bAllowFootIKNoCollision)
					callback = NULL;
			}
			else if (GetMCMH() == eMCM_Animation)
			{
				callback = NULL;
			}
		}

		m_pSkeletonPose->SetPostProcessCallback1(callback, this);
	}


	// Find last queued and active animation's normalized time (phasing), set it as AnimPhase input.
	if (!m_simplifyMovement)
	{
		ANIMCHAR_PROFILE_SCOPE("PreAnimationUpdate_UpdateAnimPhase");

		float animPhase = 0.0f;
		if (m_pSkeletonAnim != NULL)
		{
			int animCount = m_pSkeletonAnim->GetNumAnimsInFIFO(0);
			if (animCount > 0)
			{
				const CAnimation& anim = m_pSkeletonAnim->GetAnimFromFIFO(0, animCount-1);
				animPhase = anim.m_fAnimTime;
			}
		}

		uint8 curAnimPhaseHash = (uint8)((animPhase * 16.0f) + 0.5f); // TODO: Shift int part instead of mul float?
		if (curAnimPhaseHash != m_prevAnimPhaseHash)
		{
			ANIMCHAR_PROFILE_SCOPE("PreAnimationUpdate_SetAnimPhase_Actual");
			m_prevAnimPhaseHash = curAnimPhaseHash;
			m_animationGraphStates.SetInput(m_inputID[eACInputIndex_AnimPhase], animPhase);
		}
		else
		{
			ANIMCHAR_PROFILE_SCOPE("PreAnimationUpdate_SetAnimPhase_Skipped");
		}
	}

	// This is not used at all during simplified movement, 
	// so we don't even need to initialize it to something proper.
	if (!m_simplifyMovement)
	{
		if (m_curFrameTime <= 0.0f)
		{
			ANIMCHAR_PROFILE_SCOPE("PreAnimationUpdate_PrecachePrediction_Simplified");
			s_desiredParams.reset();
		}
		else
		{
			ANIMCHAR_PROFILE_SCOPE("PreAnimationUpdate_PrecachePrediction_Complex");

			// create desired location and velocity lookup table
#if 0
			float timeStep = 1.0f / float(SDesiredParams::LAST_PARAM);
			for (int i = 0; i <= SDesiredParams::LAST_PARAM; ++i)
			{
				s_desiredParams.time[i] = float(i) * timeStep;
				bool debug = (i == SDesiredParams::LAST_PARAM);
				CalculateDesiredLocationAndVelocity(s_desiredParams.location[i], s_desiredParams.velocity[i], s_desiredParams.time[i], s_desiredParams.immediateness[i], m_curFrameTime, debug);
			}
#else
			s_desiredParams.time[0] = 0.00f;
			CalculateDesiredLocationAndVelocity(s_desiredParams.location[0], s_desiredParams.velocity[0], s_desiredParams.time[0], s_desiredParams.immediateness[0], m_curFrameTime, false);

			s_desiredParams.time[1] = 0.25f;
			CalculateDesiredLocationAndVelocity(s_desiredParams.location[1], s_desiredParams.velocity[1], s_desiredParams.time[1], s_desiredParams.immediateness[1], m_curFrameTime, false);

			s_desiredParams.time[2] = 0.50f;
			CalculateDesiredLocationAndVelocity(s_desiredParams.location[2], s_desiredParams.velocity[2], s_desiredParams.time[2], s_desiredParams.immediateness[2], m_curFrameTime, false);

			s_desiredParams.time[3] = 0.75f;
			CalculateDesiredLocationAndVelocity(s_desiredParams.location[3], s_desiredParams.velocity[3], s_desiredParams.time[3], s_desiredParams.immediateness[3], m_curFrameTime, false);

			s_desiredParams.time[4] = 1.00f;
			CalculateDesiredLocationAndVelocity(s_desiredParams.location[4], s_desiredParams.velocity[4], s_desiredParams.time[4], s_desiredParams.immediateness[4], m_curFrameTime, true);
#endif
		}
	}

	Vec3 curEntMovement;
	if (RecentCollision())
	{
		curEntMovement = m_requestedEntityMovement.t;
		curEntMovement = RemovePenetratingComponent(curEntMovement, m_collisionNormal[0], 0.2f, 0.4f);
		curEntMovement = RemovePenetratingComponent(curEntMovement, m_collisionNormal[1], 0.2f, 0.4f);
		curEntMovement = RemovePenetratingComponent(curEntMovement, m_collisionNormal[2], 0.2f, 0.4f);
		curEntMovement = RemovePenetratingComponent(curEntMovement, m_collisionNormal[3], 0.2f, 0.4f);
	}
	else
		curEntMovement = m_requestedEntityMovement.t;

	// actualEntMovement measures from the previous frame, so we need to use prevFrameTime.
	float prevFrameTimeInv = (m_prevFrameTime > 0.0f) ? (1.0f / m_prevFrameTime) : 0.0f;
	float actualEntVelocity = m_actualEntMovement.t.GetLength2D() * prevFrameTimeInv;
	m_actualEntVelocity = actualEntVelocity;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::UpdateTime()
{
	float frameTime = gEnv->pTimer->GetFrameTime();
	frameTime = CLAMP(frameTime, 0.01f, 0.1f);

	assert(NumberValid(frameTime));

	m_curFrameID = gEnv->pRenderer->GetFrameID();

	// This is cached to be used by SelectLocomotionState, to not touch global timer more than once per frame.
	m_curFrameStartTime = gEnv->pTimer->GetFrameStartTime();

	m_prevFrameTime = m_curFrameTime;
	m_curFrameTime = frameTime;

	m_elapsedTimeMCM[eMCMComponent_Horizontal] += m_prevFrameTime;
	m_elapsedTimeMCM[eMCMComponent_Vertical] += m_prevFrameTime;

#ifdef _DEBUG
	DebugHistory_AddValue("eDH_FrameTime", m_curFrameTime);

	if (DebugTextEnabled())
	{
		const ColorF cWhite = ColorF(1,1,1,1);
		gEnv->pRenderer->Draw2dLabel(10, 50, 2.0f, (float*)&cWhite, false, "FrameTime Cur[%f] Prev[%f]", m_curFrameTime, m_prevFrameTime);
	}
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::AcquireRequestedBehaviourMovement()
{
	ANIMCHAR_PROFILE_DETAILED;

	m_requestedEntityMovementType = RequestedEntMovementType_Undefined;
	m_requestedIJump = 0;

	if (m_moveRequestFrameID < m_curFrameID)
	{
		m_requestedEntityMovement.SetIdentity();
		return;
	}

	assert(m_moveRequest.velocity.IsValid());
	assert(m_moveRequest.rotation.IsValid());

	m_requestedEntityMovement.q = m_moveRequest.rotation;
	m_requestedEntityMovement.t.zero();

	m_bDisablePhysicalGravity = (m_moveRequest.type == eCMT_Swim);

	switch (m_moveRequest.type)
	{
	case eCMT_None:
		assert(false);
		break;

	case eCMT_Normal:
		assert(m_requestedEntityMovementType != RequestedEntMovementType_Impulse);
		m_requestedEntityMovementType = RequestedEntMovementType_Absolute;
		m_requestedEntityMovement.t = m_moveRequest.velocity;
		break;

	case eCMT_Fly:
	case eCMT_Swim:
	case eCMT_ZeroG:
		assert(m_requestedEntityMovementType != RequestedEntMovementType_Impulse);
		m_requestedEntityMovementType = RequestedEntMovementType_Absolute;
		m_requestedEntityMovement.t = m_moveRequest.velocity;
		m_requestedIJump = 3;
		break;

	case eCMT_JumpAccumulate:
		assert(m_requestedEntityMovementType != RequestedEntMovementType_Impulse);
		m_requestedEntityMovementType = RequestedEntMovementType_Absolute;
		m_requestedEntityMovement.t = m_moveRequest.velocity;
		m_requestedIJump = 1;
		break;

	case eCMT_JumpInstant:
		assert(m_requestedEntityMovementType != RequestedEntMovementType_Impulse);
		m_requestedEntityMovementType = RequestedEntMovementType_Absolute;
		m_requestedEntityMovement.t = m_moveRequest.velocity ;
		m_requestedIJump = 2;
		break;

	case eCMT_Impulse:
		assert(m_requestedEntityMovementType != RequestedEntMovementType_Absolute);
		m_requestedEntityMovementType = RequestedEntMovementType_Impulse;
		assert(m_curFrameTime > 0.0f);
		if (m_curFrameTime > 0.0f)
		{
			// TODO: Impulses are per frame at the moment, while normal movement is per second. 
			// NOTE: Not anymore =). Impulses are now velocity per second through out the player/alien code.
			m_requestedEntityMovement.t = m_moveRequest.velocity/* / m_curFrameTime*/; 
		}
		break;
	}

	assert(m_requestedEntityMovement.IsValid());

	//float ReqEntRotZ = RAD2DEG(Ang3(m_requestedEntityMovement.q).z);

	// rotation is per frame (and can't be per second, since Quat's can only represent a max angle of 360 degrees),
	// Convert velocity from per second into per frame.
	m_requestedEntityMovement.t *= m_curFrameTime;

	if (NoMovementOverride())
	{
		m_requestedEntityMovement.SetIdentity();
		m_requestedEntityMovementType = RequestedEntMovementType_Absolute;
		m_requestedIJump = 0;
	}

#ifdef _DEBUG
	DebugGraphQT(m_requestedEntityMovement, "eDH_ReqEntMovementTransX", "eDH_ReqEntMovementTransY", "eDH_ReqEntMovementRotZ");

	if (DebugTextEnabled())
	{
		Ang3 requestedEntityRot(m_requestedEntityMovement.q);
		const ColorF cWhite = ColorF(1,1,1,1);
		gEnv->pRenderer->Draw2dLabel(350, 50, 2.0f, (float*)&cWhite, false, "Req Movement[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f]",
			m_requestedEntityMovement.t.x / m_curFrameTime, m_requestedEntityMovement.t.y / m_curFrameTime, m_requestedEntityMovement.t.z / m_curFrameTime, 
			RAD2DEG(requestedEntityRot.x), RAD2DEG(requestedEntityRot.y), RAD2DEG(requestedEntityRot.z));
	}
#endif

	assert(m_requestedEntityMovement.IsValid());
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::SetAnimGraphSpeedInputs(const QuatT& movement, const QuatT& origin, float frameTime, int baseInputIndex /* = 0 */)
{
	ANIMCHAR_PROFILE;

	bool* simplified = NULL;
	int8* curLocalMoveDirH4 = NULL;
	if (baseInputIndex == eACInputIndex_RequestedSpeedBase)
	{
		curLocalMoveDirH4 = &m_requestedEntMoveDirLH4;
		simplified = &m_simplifiedAGSpeedInputsRequested;
	}
	else if (baseInputIndex == eACInputIndex_ActualSpeedBase)
	{
		curLocalMoveDirH4 = &m_actualEntMoveDirLH4;
		simplified = &m_simplifiedAGSpeedInputsActual;
	}

	Vec3 worldMoveSpeed = (frameTime > 0.0f) ? (movement.t / frameTime) : ZERO;
	if ((worldMoveSpeed.GetLength2D() < 0.5f) && !RecentQuickLoad())
	{
		ANIMCHAR_PROFILE_SCOPE("SetAnimGraphspeedInputs_Simplified");

		if (!*simplified)
		{
			*simplified = true;

			*curLocalMoveDirH4 = 0;
			m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveDirLH4], "Idle");

			m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedLX], 0.0f);
			m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedLY], 0.0f);
			m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedLH], 0.0f);
			m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedLZ], 0.0f);
			m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedWH], 0.0f);
			m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedWV], 0.0f);
			m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeed], 0.0f);
		}
	}
	else
	{
		ANIMCHAR_PROFILE_SCOPE("SetAnimGraphspeedInputs_Complex");

		*simplified = false;

		float worldMoveSpeedH = worldMoveSpeed.GetLength2D();
		Vec3 localMoveSpeed = origin.q.GetInverted() * worldMoveSpeed;
		float localMoveSpeedH = localMoveSpeed.GetLength2D();
		float localMoveAngleH = RAD2DEG(atan2f(-localMoveSpeed.x, localMoveSpeed.y));

		const int moveDirCount = 5;
		char* moveDirName[moveDirCount] = { "Idle", "Fwd", "Bwd", "Lft", "Rgt" };
		float moveDirAngle[moveDirCount] = { -1000, 0, 180, 90, -90 };
		float moveDirWidth = 160.0f;

		float minAngle = moveDirAngle[*curLocalMoveDirH4] - moveDirWidth / 2.0f;
		float maxAngle = moveDirAngle[*curLocalMoveDirH4] + moveDirWidth / 2.0f;
		if (!((localMoveAngleH >= minAngle) && (localMoveAngleH <= maxAngle)) &&
			!((localMoveAngleH+360 >= minAngle) && (localMoveAngleH+360 <= maxAngle)) &&
			!((localMoveAngleH-360 >= minAngle) && (localMoveAngleH-360 <= maxAngle)))
		{
			for (int8 i = 0; i < moveDirCount; ++i)
			{
				minAngle = moveDirAngle[i] - moveDirWidth / 2.0f;
				maxAngle = moveDirAngle[i] + moveDirWidth / 2.0f;
				if (((localMoveAngleH >= minAngle) && (localMoveAngleH <= maxAngle)) || 
					((localMoveAngleH+360 >= minAngle) && (localMoveAngleH+360 <= maxAngle)) ||
					((localMoveAngleH-360 >= minAngle) && (localMoveAngleH-360 <= maxAngle)))
				{
					*curLocalMoveDirH4 = i;
				}
			}
		}

		assert(*curLocalMoveDirH4 > 0);

		m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveDirLH4], moveDirName[*curLocalMoveDirH4]);

		m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedLX], localMoveSpeed.x);
		m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedLY], localMoveSpeed.y);
		m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedLH], localMoveSpeedH);
		m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedLZ], localMoveSpeed.z);
		m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedWH], worldMoveSpeedH);
		m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeedWV], worldMoveSpeed.z);
		m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_MoveSpeed], worldMoveSpeed.GetLength());
	}

	if (movement.q.IsIdentity())
	{
		// TODO: Add simplified culling here as well, to not touch the AG states at all.
		m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_TurnSpeedLZ], 0.0f);
	}
	else
	{
		// NOTE: Don't know why this was turned into 'local' space. 
		// The rotation is relative by definition and should be the same regardless of referencespace.
		float localTurnSpeedZ = (frameTime > 0.0f) ? RAD2DEG(movement.q.GetRotZ() / frameTime) : 0.0f;
		m_animationGraphStates.SetInput(m_inputID[baseInputIndex + eACInputIndex_TurnSpeedLZ], localTurnSpeedZ);
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//- PostAnimationUpdate ----------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

void CAnimatedCharacter::PostAnimationUpdate()
{
	//ANIMCHAR_PROFILE_SCOPE("SetAnimGraphspeedInputs_Simplified");
	ANIMCHAR_PROFILE;

#if _DEBUG && defined(USER_david)
	SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

	if (m_curFrameTime <= 0.0f)
		return;

}



//--------------------------------------------------------------------------------
//------------                  PostProcessingUpdate          --------------------
//--------------------------------------------------------------------------------
//----    here we have the final skeleton pose with Look-IK and Aim-IK        ----
//--           the last missing feature is the Foot-IK, and this                --
//------          requires consideration of the game-environment            ------
//--------------------------------------------------------------------------------

void CAnimatedCharacter::PostProcessingUpdate()
{
	ANIMCHAR_PROFILE;

	extern f32 g_fPrintLine;
	const ColorF cWhite = ColorF(1,0,0,1);

#if _DEBUG && defined(USER_david)
	SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

	if (m_pSkeletonAnim == NULL)
		return;
	if (m_pSkeletonPose == NULL)
		return;

	// TODO: cache these indices instead of fetching every frame!
	int32 PelvisIdx	=	m_pSkeletonPose->GetJointIDByName("Bip01 Pelvis");
	if (PelvisIdx<0) 
		return; //probably not a human
	int32 LHeelIdx	=	m_pSkeletonPose->GetJointIDByName("Bip01 L Heel");
	if (LHeelIdx<0) 
		return;
	int32 RHeelIdx	=	m_pSkeletonPose->GetJointIDByName("Bip01 R Heel");
	if (RHeelIdx<0) 
		return;

//----------------------------------------------------------------------------------------

	IRenderAuxGeom*	pAuxGeom	= gEnv->pRenderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );

	//check the feet-positions in the animation
	//Vec3 Final_LHeel	=	m_entLocation*m_pSkeletonPose->GetAbsJointByID(LHeelIdx).t;
	//Vec3 Final_RHeel	=	m_entLocation*m_pSkeletonPose->GetAbsJointByID(RHeelIdx).t;
	//pAuxGeom->DrawSphere( Final_LHeel,0.1f, RGBA8(0x00,0xff,0x00,0x00) );
	//pAuxGeom->DrawSphere( Final_RHeel,0.1f, RGBA8(0x00,0xff,0x00,0x00) );


	SmoothCD(m_fJumpSmooth, m_fJumpSmoothRate, m_curFrameTime, f32(m_moveRequest.jumping), 0.30f);
	if (m_moveRequest.jumping)
		m_fJumpSmooth = 1.0f;
	//gEnv->pRenderer->Draw2dLabel(10, g_fPrintLine, 1.3f, (float*)&cWhite, false, "m_fJumpSmooth: %f", m_fJumpSmooth );
	//g_fPrintLine += 0x10;



	{
		//Find the ground-slope in forward facing direction of the character. It's needed for motion parameterization
		Vec3 vRootNormal = (m_LLastHeelIVecSmooth.normal+m_RLastHeelIVecSmooth.normal).GetNormalizedSafe(Vec3(0,0,1));
		Vec3 vGroundNormalRoot	=	vRootNormal*m_entLocation.q;
		Vec3 gnormal	= Vec3(0,vGroundNormalRoot.y,vGroundNormalRoot.z);
		f32 cosine		=	Vec3(0,0,1)|gnormal;
		Vec3 sine			=	Vec3(0,0,1)%gnormal;
		f32 fGroundSlopeRad = atan2( sgn(sine.x)*sine.GetLength(),cosine );
		f32 fGroundSlopeDeg = RAD2DEG(fGroundSlopeRad);
		f32 fGroundAngle	= acos_tpl(CLAMP(vGroundNormalRoot.z,-1,1));
		if ( fGroundAngle>DEG2RAD(35.0f) )
			fGroundAngle=DEG2RAD(35.0f);
		SmoothCD(m_fGroundSlopeSmooth, m_fGroundSlopeRate, m_curFrameTime, fGroundSlopeDeg, 0.20f);
		m_pSkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSlope, m_fGroundSlopeSmooth, m_curFrameTime, false);
		//	gEnv->pRenderer->Draw2dLabel(10, g_fPrintLine, 1.3f, (float*)&cWhite, false, "m_fGroundSlopeSmooth: %f", m_fGroundSlopeSmooth );
		//	g_fPrintLine += 0x10;
	}

	//never move the legs higher then the Pelvis
	Vec3 Final_LHeel	=	m_entLocation*m_pSkeletonPose->GetAbsJointByID(LHeelIdx).t;
	Vec3 Final_RHeel	=	m_entLocation*m_pSkeletonPose->GetAbsJointByID(RHeelIdx).t;
	f32 fOriginalPelvis		=	m_pSkeletonPose->GetAbsJointByID(PelvisIdx).t.z;
	f32 fHight=fOriginalPelvis;
	fOriginalPelvis *=	0.75f;
	fOriginalPelvis -=	0.2f;
	fHight=fOriginalPelvis-fHight;
	Vec3 UpperLimit =	m_pSkeletonPose->GetAbsJointByID(PelvisIdx).t; 
	UpperLimit.z += fHight;
	UpperLimit		=	m_entLocation*UpperLimit;



	f32 ldif = m_LLastHeelIVec.pos.z;
	f32 rdif = m_RLastHeelIVec.pos.z;
	if (m_moveRequest.jumping)
	{
		ldif -= Final_LHeel.z;
		rdif -= Final_RHeel.z;
	}
	else
	{
		ldif -= m_entLocation.t.z;
		rdif -= m_entLocation.t.z;
	}

	
	f32 minHeightStance = (GetCurrentStance() == STANCE_CROUCH) ? (-0.2f) : (-0.4f);
	f32 minHeight = LERP(minHeightStance, 0.0f, m_fJumpSmooth);
	f32 maxHeight = LERP(0.0f, 2.0f, m_fJumpSmooth);
	f32 fRootHeight = clamp_tpl(min(ldif,rdif), minHeight, maxHeight);
	//gEnv->pRenderer->Draw2dLabel(10, g_fPrintLine, 1.3f, (float*)&cWhite, false, "ldif: %f   rdif: %f",ldif,rdif );
	//g_fPrintLine += 0x10;

	{
		//this tricky timer-stuff is needs to ensure better blending from rag-doll into animation
		f32 fStandUpTime = 15.0f;
		bool fallen = false;
		if ( IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(GetEntity()->GetId()) )
			fallen = pActor->IsFallen();
		if (fallen)	m_fStandUpTimer=3.0f; //character is rag-doll. We don't need ground alignment in this case
		fRootHeight*=(m_fStandUpTimer/fStandUpTime);
		m_fStandUpTimer=min(fStandUpTime,f32(m_fStandUpTimer+m_curFrameTime));
	}

	SmoothCD(m_fRootHeightSmooth, m_fRootHeightRate, m_curFrameTime, fRootHeight, 0.20f);
	m_fRootHeightSmooth = LERP(m_fRootHeightSmooth, fRootHeight, m_fJumpSmooth);

	//gEnv->pRenderer->Draw2dLabel(10, g_fPrintLine, 1.3f, (float*)&cWhite, false, "m_fRootHeightSmooth: %f", m_fRootHeightSmooth );
	//g_fPrintLine += 0x10;

	m_pSkeletonPose->MoveSkeletonVertical(m_fRootHeightSmooth);
	//gEnv->pRenderer->Draw2dLabel(10, g_fPrintLine, 1.5f, (float*)&cWhite, false, "m_fRootHeightSmooth: %f", m_fRootHeightSmooth );
	//g_fPrintLine += 0x10;


	//------------------------------------------------------------------------------------------

	float prevFrameTimeInv = (m_prevFrameTime > 0.0f) ? (1.0f / m_prevFrameTime) : 0.0f;
	float actualEntVelocityZ = m_actualEntMovement.t.z * prevFrameTimeInv;

	IPhysicalWorld* pIPhysicsWorld = GetISystem()->GetIPhysicalWorld();
	{
		f32 fLDistance =	(Vec2(Final_LHeel.x,Final_LHeel.y)-m_LLastHeel2D).GetLength();
		if ((fLDistance>0.05f) || (actualEntVelocityZ > 0.05f) || (m_actualEntVelocity > 0.05f))
		{
			m_LLastHeelIVec=CheckFootIntersection(m_fRootHeightSmooth,Final_LHeel,pIPhysicsWorld);
			m_LLastHeel2D.x  =	m_LLastHeelIVec.pos.x;
			m_LLastHeel2D.y  =	m_LLastHeelIVec.pos.y;
			//gEnv->pRenderer->Draw2dLabel(10, g_fPrintLine, 1.3f, (float*)&cWhite, false, "Raycast Left Heel: %f %f %f",m_LLastHeelIVec.pos.x,m_LLastHeelIVec.pos.y,m_LLastHeelIVec.pos.z );
			//g_fPrintLine += 0x10;
		}
		if (m_LLastHeelIVec.pos.z>UpperLimit.z) m_LLastHeelIVec.pos.z=UpperLimit.z;
		//left leg
		SmoothCD( m_LLastHeelIVecSmooth.normal, m_LLastHeelIVecSmoothRate.normal, m_curFrameTime, m_LLastHeelIVec.normal, 0.10f);
		SmoothCD( m_LLastHeelIVecSmooth.pos, m_LLastHeelIVecSmoothRate.pos, m_curFrameTime, m_LLastHeelIVec.pos, 0.10f);
		m_LLastHeelIVecSmooth.pos = LERP(m_LLastHeelIVecSmooth.pos, m_LLastHeelIVec.pos, m_fJumpSmooth);
		Plane LGroundPlane=Plane::CreatePlane(m_LLastHeelIVecSmooth.normal*m_entLocation.q,m_entLocation.GetInverted()*m_LLastHeelIVecSmooth.pos);
	//	gEnv->pRenderer->Draw2dLabel(10, g_fPrintLine, 1.5f, (float*)&cWhite, false, "LGroundPlane: %f %f %f  d:%f", LGroundPlane.n.x,LGroundPlane.n.y,LGroundPlane.n.z,LGroundPlane.d );
	//	g_fPrintLine += 0x10;
		m_pSkeletonPose->SetFootGroundAlignmentCCD( CA_LEG_LEFT, LGroundPlane);
	}


	{
		f32 fRDistance =	(Vec2(Final_RHeel.x,Final_RHeel.y)-m_RLastHeel2D).GetLength();
		if ((fRDistance>0.05f)|| (actualEntVelocityZ > 0.05f) || (m_actualEntVelocity > 0.05f))
		{
			m_RLastHeelIVec	=	CheckFootIntersection(m_fRootHeightSmooth,Final_RHeel,pIPhysicsWorld);
			m_RLastHeel2D.x	=	m_RLastHeelIVec.pos.x;
			m_RLastHeel2D.y	=	m_RLastHeelIVec.pos.y;
			//gEnv->pRenderer->Draw2dLabel(10, g_fPrintLine, 1.3f, (float*)&cWhite, false, "Raycast Right Heel: %f %f %f",m_LLastHeelIVec.pos.x,m_LLastHeelIVec.pos.y,m_LLastHeelIVec.pos.z );
			//g_fPrintLine += 0x10;
		}
		if (m_RLastHeelIVec.pos.z>UpperLimit.z) m_RLastHeelIVec.pos.z=UpperLimit.z;
		//right leg
		SmoothCD( m_RLastHeelIVecSmooth.normal, m_RLastHeelIVecSmoothRate.normal, m_curFrameTime, m_RLastHeelIVec.normal, 0.10f);
		SmoothCD( m_RLastHeelIVecSmooth.pos, m_RLastHeelIVecSmoothRate.pos, m_curFrameTime, m_RLastHeelIVec.pos, 0.10f);
		m_RLastHeelIVecSmooth.pos = LERP(m_RLastHeelIVecSmooth.pos, m_RLastHeelIVec.pos, m_fJumpSmooth);
		Plane RGroundPlane=Plane::CreatePlane(m_RLastHeelIVecSmooth.normal*m_entLocation.q,m_entLocation.GetInverted()*m_RLastHeelIVecSmooth.pos);
		//gEnv->pRenderer->Draw2dLabel(10, g_fPrintLine, 1.5f, (float*)&cWhite, false, "RGroundPlane: %f %f %f  d:%f", RGroundPlane.n.x,RGroundPlane.n.y,RGroundPlane.n.z,RGroundPlane.d );
		//g_fPrintLine += 0x10;
		m_pSkeletonPose->SetFootGroundAlignmentCCD( CA_LEG_RIGHT, RGroundPlane);
	}
}
			
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
IVec CAnimatedCharacter::CheckFootIntersection(f32 fRootHeight, const Vec3& vFinal_Heel, IPhysicalWorld* pIPhysicsWorld)
{
	Vec3 out;
	uint32 ival=0;
	f32 fHeightHeel	=-10000.0f;
	Vec3 normalHeel	=	Vec3(0,0,1);

	Vec3 vRayHeel=vFinal_Heel; vRayHeel.z+=1.0f;

	ray_hit HitHeel;
	int	numHeelHits = pIPhysicsWorld->RayWorldIntersection( vRayHeel, Vec3(0,0,-3), ent_rigid|ent_sleeping_rigid|ent_static|ent_terrain, rwi_stop_at_pierceable, &HitHeel, 1, GetEntity()->GetPhysics() );
	if (numHeelHits)
	{
		f32 fLocalHeel=HitHeel.pt.z-fRootHeight;
		if (fLocalHeel>-0.05f)
		{ 
			HitHeel.n.z	=	fabsf(HitHeel.n.z); //normals can be flipped because of bad assets 	
			fHeightHeel	=	HitHeel.pt.z;
			normalHeel	=	HitHeel.n;
		};

		//IRenderAuxGeom*	pAuxGeom	= gEnv->pRenderer->GetIRenderAuxGeom();
		//pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
		//pAuxGeom->DrawLine( vRayHeel,RGBA8(0x1f,0xff,0x1f,0x00),vRayHeel+Vec3(0,0,-3),RGBA8(0x1f,0xff,0x1f,0x00) );
		//pAuxGeom->DrawSphere( HitHeel.pt,0.1f, RGBA8(0xff,0x00,0x00,0x00) );
	}


	IVec retval;
	retval.normal	=	normalHeel.GetNormalized();
	Vec3 proj = Vec3(retval.normal.x,retval.normal.y,0);
	if (proj.GetLength()>0.1f)
	{
		f32 fMaxSlope=DEG2RAD(30.0f);
		proj.Normalize();
		f32 fSlopeAngle = acos_tpl(retval.normal.z);
		Vec3 cross = (Vec3(0,0,1)%retval.normal).GetNormalized();
		if (fSlopeAngle>fMaxSlope)
			retval.normal = Quat::CreateRotationAA(fMaxSlope,cross)*Vec3(0,0,1);
	}	
	retval.pos		=	Vec3(vFinal_Heel.x,vFinal_Heel.y,fHeightHeel);
	return retval;
}

//--------------------------------------------------------------------------------
//- End of Ivo's foot IK code ----------------------------------------------------
//--------------------------------------------------------------------------------




//--------------------------------------------------------------------------------

void CAnimatedCharacter::UpdateCurAnimLocation()
{
	ANIMCHAR_PROFILE_DETAILED;

	IEntity* pEntity = GetEntity();
	assert(pEntity);
	IEntity* pParent = pEntity->GetParent();

	assert(m_animLocation.IsValid());

	m_desiredAnimMovement.SetIdentity();

	// If simplified, anim location will be set to entity anyway, so we don't need any of this.
	if (m_simplifyMovement)
		return;

	// ASSET MOVEMENT
	QuatT assetAnimMovement(IDENTITY);
	if (m_pSkeletonAnim != NULL)
	{
/*		
		Vec3 assetAnimTransLocal = m_pSkeleton->GetRelTranslation();
		float assetAnimRotZLocal = m_pSkeleton->GetRelRotationZ();
		assetAnimMovement.t = m_animLocation.q * assetAnimTransLocal;
		assetAnimMovement.q = Quat::CreateRotationZ(assetAnimRotZLocal);
		assert(assetAnimMovement.IsValid());
*/
		assetAnimMovement = m_pSkeletonAnim->GetRelMovement();
		assetAnimMovement.t = m_animLocation.q * assetAnimMovement.t;
		m_desiredAnimMovement = ApplyWorldOffset(m_desiredAnimMovement, assetAnimMovement);
	  
	//	IRenderAuxGeom*	pAuxGeom	= gEnv->pRenderer->GetIRenderAuxGeom();
	//	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
	//	pAuxGeom->DrawLine( m_desiredAnimMovement.t,RGBA8(0x00,0xff,0x00,0x00),m_desiredAnimMovement.t+Vec3(0,0,+20),RGBA8(0xff,0xff,0xff,0x00) );

	}
	else
	{
		assetAnimMovement.SetIdentity();
	}

#ifdef _DEBUG
	DebugGraphQT(assetAnimMovement, "eDH_AnimAssetTransX", "eDH_AnimAssetTransY", "eDH_AnimAssetRotZ");
#endif

	// This assert was removed, because Troppers use physicalized animation controlled movement to dodge.
	// Have not investigated what problems this might cause, them penetrating into obstacles, entity trying to catch up with animation, etc.
	// Luciano said it seems to work fine. We'll see...
	//assert((GetMCMH() != eMCM_Animation) || (m_colliderMode != eColliderMode_Pushable));

	//((GetMCMH() == eMCM_DecoupledCatchUp) && (m_requestedEntityMovement.t.GetLengthSquared() > 0.1f))))
	//m_desiredAnimMovement = ApplyWorldOffset(m_desiredAnimMovement, m_entTeleportMovement);
	/*
	// ENTITY MOVEMENT ERROR
	if ((m_colliderMode == eColliderMode_Pushable) && (GetMCMH() == eMCM_Animation))
	{
	// Character is animation driven and has collider enabled.
	// The entity was told to move but could not due to some external forces (collision, not explicit teleportation).
	// Adjust the animation location to the actual new entity position, since that's the only choice we have to join animation and entity locations again.

	QuatT entMovementError = GetWorldOffset(m_expectedEntMovement, m_actualEntMovement);
	assert(entMovementError.IsValid());
	m_desiredAnimMovement = ApplyWorldOffset(m_desiredAnimMovement, entMovementError);

	#ifdef _DEBUG
	DebugGraphQT(entMovementError, "eDH_EntMovementErrorTransX", "eDH_EntMovementErrorTransY", "eDH_EntMovementErrorRotZ");
	#endif
	}
	else
	{
	#ifdef _DEBUG
	DebugGraphQT(IDENTITY, "eDH_EntMovementErrorTransX", "eDH_EntMovementErrorTransY", "eDH_EntMovementErrorRotZ");
	#endif
	}
	*/

	// ANIM TARGET CORRECTION
	QuatT animTargetCorrectionMovement = CalculateAnimTargetMovement();
#ifdef _DEBUG
	DebugGraphQT(animTargetCorrectionMovement, "eDH_AnimTargetCorrectionTransX", "eDH_AnimTargetCorrectionTransY", "eDH_AnimTargetCorrectionRotZ");
#endif
	assert(animTargetCorrectionMovement.IsValid());
	m_desiredAnimMovement = ApplyWorldOffset(m_desiredAnimMovement, animTargetCorrectionMovement);

	// FOOT ANCHORING
	if ((m_pSkeletonAnim != NULL) && (pParent == NULL))
	{
		// Only apply foot anchoring corrections when not parented.
		m_desiredAnimMovement.t += m_pSkeletonAnim->GetRelFootSlide();
	}

#ifdef _DEBUG
	if (CAnimationGraphCVars::Get().m_debugTweakTrajectoryFit != 0)
	{
		m_desiredAnimMovement.SetIdentity();
	}
#endif

	if (NoMovementOverride())
	{
		m_desiredAnimMovement.SetIdentity();
	}

	if (RecentCollision())
	{
		m_desiredAnimMovement.t = RemovePenetratingComponent(m_desiredAnimMovement.t, m_collisionNormal[0], 0.0f, 0.1f);
		m_desiredAnimMovement.t = RemovePenetratingComponent(m_desiredAnimMovement.t, m_collisionNormal[1], 0.0f, 0.1f);
		m_desiredAnimMovement.t = RemovePenetratingComponent(m_desiredAnimMovement.t, m_collisionNormal[2], 0.0f, 0.1f);
		m_desiredAnimMovement.t = RemovePenetratingComponent(m_desiredAnimMovement.t, m_collisionNormal[3], 0.0f, 0.1f);
	}

	//DebugGraphQT(m_desiredAnimMovement, eDH_TEMP00, eDH_TEMP01, eDH_TEMP03); DebugHistory_AddValue(eDH_TEMP02, m_desiredAnimMovement.t.z);
	assert(m_desiredAnimMovement.IsValid());

	// While parented this will be overwritten by entity location in PreRenderUpdate().
	// If not, it would not have any inherited movement from the parent.
	m_animLocation = ApplyWorldOffset(m_animLocation, m_desiredAnimMovement);

	assert(m_animLocation.IsValid());
}

//--------------------------------------------------------------------------------

QuatT CAnimatedCharacter::CalculateAnimTargetMovement()
{
	// NOTE: The negative m_animTargetTime is encoding different steps/phases for debugging purpose only.

	RefreshAnimTarget();

	if (m_pAnimTarget == NULL)
	{
		m_animTargetTime = -4.0f;
		return IDENTITY;
	}

	if (!m_pAnimTarget->preparing && !m_pAnimTarget->activated && !m_pAnimTarget->allowActivation && 
		m_pAnimTarget->position.IsZero() && m_pAnimTarget->orientation.IsIdentity())
	{
		m_animTargetTime = -4.0f;
		return IDENTITY;
	}

	//assert(m_pAnimTarget->activationTimeRemaining >= 0.0f);
	if (m_pAnimTarget->activationTimeRemaining < 0.0f)
	{
		if (m_animTargetTime > 0.0f)
			m_animTargetTime = -666.0f;

		return IDENTITY;
	}

	QuatT target(m_pAnimTarget->position, m_pAnimTarget->orientation);
	float remainingTime = m_pAnimTarget->activationTimeRemaining;
	QuatT predictedLocation = target;
	QuatT animTargetMovement;

	if (m_animTargetTime < 0.0f)
	{
		if (!m_pAnimTarget->preparing && !m_pAnimTarget->activated)
		{
			m_animTargetTime = -3.0f;
			if (!target.t.IsZero() || !target.q.IsIdentity())
			{
				m_animTargetTime = -2.0f;
				m_animTarget = target;
				assert(m_animTarget.IsValid());
			}
			return IDENTITY;
		}
		else
		{
			if (m_pAnimTarget->preparing)
			{
				m_animTargetTime = -2.0f;
				m_animTarget = target;
				assert(m_animTarget.IsValid());
				return IDENTITY;
			}
			else
			{
				assert(m_pAnimTarget->activated);
				if (m_animTargetTime < -1.0f)
				{
					QuatT correction;
					//if (m_animTargetTime == -2.0f)
					//	correction = GetWorldOffset(m_animLocation, m_animTarget);
					//else
					correction.SetIdentity(); // This means we never got a preparing phase, went directly into activated. Though, we don't really know where the start should be, so we can't correct it there.

					m_animTargetTime = -1.0f;
					m_animTarget = target;

					assert(correction.IsValid());
					return correction;
				}
				else
				{
					assert(m_animTargetTime == -1.0f);
					m_animTargetTime = remainingTime;
				}
			}
		}
	}

	m_animTargetTime = max(m_animTargetTime, remainingTime);

	if (remainingTime <= 1.0f)
	{
		// look into the future ... where will we end up
		if (m_pSkeletonAnim != NULL)
		{
			m_pSkeletonAnim->SetFuturePathAnalyser(true);
			AnimTransRotParams amp = m_pSkeletonAnim->GetBlendedAnimTransRot(remainingTime, true);
			if (fabs(remainingTime - amp.m_DeltaTime) < 0.1f)
			{
				QuatT remainingMovement(m_animLocation.q * amp.m_TransRot.t, amp.m_TransRot.q); // Convert from local anim space into world space.
				predictedLocation = ApplyWorldOffset(m_animLocation, remainingMovement);
			}
		}
	}

	QuatT offsetToTarget = GetWorldOffset(predictedLocation, target);

	float fade2 = (m_animTargetTime > 0.0f) ? (1.0f - remainingTime / m_animTargetTime) : 0.0f;
	assert(fade2 <= 1.0f);
	float fade = (remainingTime > 0.0f) ? (m_curFrameTime / remainingTime * fade2 * fade2) : 0.0f;

	// When m_curFrameTime is bigger than remainingTime, which might happen during big spikes, fade might be bigger than 1.0f.
	//assert(fade <= 1.0f);
	fade = CLAMP(fade, 0.0f, 1.0f);

	if (remainingTime < 0.2f)
	{
		fade = fade * remainingTime/0.2f;
	}

	animTargetMovement = offsetToTarget.GetScaled(fade);

#ifdef _DEBUG

	if (CAnimationGraphCVars::Get().m_debugLocations != 0)
	{
		CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();

		if (pPD != NULL)
		{
			Vec3 bump(0,0,0.1f);

			pPD->Begin(UNIQUE("AnimatedCharacter.PrePhysicsStep2.AnimTarget"), true);
			pPD->AddSphere(predictedLocation.t+bump, 0.08f, ColorF(0,0.5f,0,1), 0.5f);
			pPD->AddDirection(predictedLocation.t+bump, 0.10f, predictedLocation.q.GetColumn1(), ColorF(0,0.5f,0,1), 0.5f);
			pPD->AddLine(m_animLocation.t+bump, predictedLocation.t+bump, ColorF(0,0.5f,0,1), 0.5f);
		}
	}

	ColorF colorWhite(1,1,1,1);

	if (DebugTextEnabled())
	{
		Ang3 targetOri(target.q);
		Ang3 targetRot(animTargetMovement.q);
		gEnv->pRenderer->Draw2dLabel(10, 150, 2.0f, (float*)&colorWhite, false, 
			"Target Location[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f] Movement[%.2f, %.2f, %.2f | %.2f, %.2f, %.2f] TimeLeft[%.2f]",
			target.t.x, target.t.y, target.t.z, 
			RAD2DEG(targetOri.x), RAD2DEG(targetOri.y), RAD2DEG(targetOri.z),
			animTargetMovement.t.x / m_curFrameTime, animTargetMovement.t.y / m_curFrameTime, animTargetMovement.t.z / m_curFrameTime, 
			RAD2DEG(targetRot.x), RAD2DEG(targetRot.y), RAD2DEG(targetRot.z),
			remainingTime);
	}

	if (CAnimationGraphCVars::Get().m_debugLocations != 0)
	{
		CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
		if (pPD != NULL)
		{
			Vec3 bump(0, 0, 0.05f);
			pPD->Begin(UNIQUE("AnimatedCharacter.PrePhysicsStep2.AnimTargetMovement"), true);
			pPD->AddLine(m_animLocation.t+bump, m_animLocation.t+animTargetMovement.t+bump, ColorF(1, 1, 1, 0.5f), 5.0f);
		}
	}
#endif

	assert(animTargetMovement.IsValid());
	return animTargetMovement;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::CalculateAndRequestPhysicalEntityMovement()
{
	ANIMCHAR_PROFILE_DETAILED;

	QuatT wantedEntMovement = CalculateWantedEntityMovement();
	QuatT wantedEntLocation = ApplyWorldOffset(m_entLocation, wantedEntMovement); // m_entLocation might be out of date here.
	wantedEntLocation.q.Normalize();

/*
	Vec3 pos = m_entLocation.t + Vec3(0,0,0.2f);
	Vec3 curDir = m_entLocation.q.GetColumn1();
	Vec3 wantDir = wantedEntLocation.q.GetColumn1();
	Vec3 lftDir = m_entLocation.q.GetColumn0();
	float rot = wantedEntMovement.q.GetRotZ();
	gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags( e_Def3DPublicRenderflags );
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,255,0,255), pos+curDir, ColorB(255,255,0,255), 2.0f);
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,0,255,255), pos+wantDir, ColorB(255,0,255,255), 2.0f);
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pos+curDir, ColorB(0,255,255,255), pos+curDir+lftDir*rot, ColorB(0,255,255,255), 2.0f);
*/

	RequestPhysicalEntityMovement(wantedEntLocation, wantedEntMovement);
}

//--------------------------------------------------------------------------------

QuatT CAnimatedCharacter::CalculateWantedEntityMovement()
{
	ANIMCHAR_PROFILE_DETAILED;

#ifdef _DEBUG
	if (CAnimationGraphCVars::Get().m_debugTweakTrajectoryFit != 0)
		return IDENTITY;
#endif

	if (NoMovementOverride())
		return IDENTITY;

	QuatT catchupMovement;
	IEntity* pParent = GetEntity()->GetParent();
	if (pParent != NULL)
		catchupMovement = m_desiredAnimMovement;
	else
	{
		catchupMovement = GetWorldOffset(m_entLocation, m_animLocation);

		// For atomic update and multithreaded, this prevents oscillation, for some reason.
		if ((HasAtomicUpdate() || GetMCMH() == eMCM_Animation) && gEnv->pPhysicalWorld->GetPhysVars()->bMultithreaded)
		{
			catchupMovement.t -= m_expectedEntMovement.t;
			catchupMovement.t += m_desiredAnimMovement.t;
		}
	}

	assert(catchupMovement.IsValid());

#ifdef _DEBUG
	if (DebugFilter())
	{
		DebugHistory_AddValue("eDH_TEMP00", m_desiredAnimMovement.t.z);
		DebugHistory_AddValue("eDH_TEMP01", m_expectedEntMovement.t.z);
		DebugHistory_AddValue("eDH_TEMP02", m_actualEntMovement.t.z);
		DebugHistory_AddValue("eDH_TEMP10", m_requestedEntityMovement.t.z);
		DebugHistory_AddValue("eDH_TEMP11", catchupMovement.t.z);
	}
#endif

	if ((catchupMovement.t == m_requestedEntityMovement.t) && (catchupMovement.q == m_requestedEntityMovement.q))
		return catchupMovement;

	QuatT wantedEntMovement = MergeMCM(m_requestedEntityMovement, catchupMovement, m_requestedEntityMovement, false);
	assert(wantedEntMovement.IsValid());
	return wantedEntMovement;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::RequestPhysicalEntityMovement(const QuatT& wantedEntLocationConst, const QuatT& wantedEntMovementConst)
{
	ANIMCHAR_PROFILE_DETAILED;

	IEntity* pEntity = GetEntity();
	assert(pEntity != NULL);

	IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();
	if (pPhysEnt == NULL)
		return;

	if (pPhysEnt->GetType() == PE_ARTICULATED)
		return;

	// local/world movement into force/velocity or whatever.
	// TODO: look at the tracker apply functions, see what they do different.

	QuatT wantedEntLocation = wantedEntLocationConst;
	QuatT wantedEntMovement = wantedEntMovementConst;

	// PROTOTYPE: Limit entity velocity to animation velocity projected along entity velocity direction.
	// Problems due to:
	// - Animations can play at a faster speed than how they look good. Using this method to limit prone (for example)
	//   results in too-fast animations being "normal"
	// - Making entity movement dependant on animation results in a very variable entity speed
	// - Whenever the clamping happens the prediction becomes wrong - the AC tends to end up ahead of the entity (when 
	//   the clamping is done after the animation is determined by the prediction etc.
	// - If the entity movement is clamped before the animation properties are set then starting becomes impossible - AC
	//   tells entity it can't move, so entity doesn't move, and that makes the AC not move etc
	// 

	//bool allowEntityClamp = (CAnimationGraphCVars::Get().m_TemplateMCMs == 0) ? (GetMCMH() == eMCM_Entity) : (GetMCMH() == eMCM_ClampedEntity);
	bool allowEntityClamp = (GetMCMH() == eMCM_ClampedEntity);

	/*
	// NOTE: During AnimTarget MCM is forced to Enitity (approaching/preparing) or Animation (active), so this is redundant.
	const SAnimationTarget* pAnimTarget = m_animationGraphStates.GetAnimationTarget();
	bool hasAnimTarget = (pAnimTarget != NULL) && (pAnimTarget->preparing || pAnimTarget->activated);
	*/

	/*
	// NOTE: MCM is forced to Entity for all players, so this is redundant.
	// NOTE: Actually, it's only entity in first person, not nessecarily in third person.
	IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pEntity->GetId());
	bool isPlayer = pActor->IsPlayer();
	*/

	/*
	// NOTE: Not supported anymore. Was only used at one place in grunt_x.lua, always set to one.
	if ((m_params.flags & eACF_AllowEntityClampingByAnimation) == 0)
	allowEntityClamp = false;
	*/

#ifdef _DEBUG
	if (allowEntityClamp && (CAnimationGraphCVars::Get().m_entityAnimClamp == 0))
		allowEntityClamp = false;
#endif

	if (allowEntityClamp)
	{
		ICharacterInstance* pCharacterInstance = pEntity->GetCharacter(0);
		ISkeletonAnim* pSkeletonAnim = (pCharacterInstance != NULL) ? pCharacterInstance->GetISkeletonAnim() : NULL;

		float requestedEntVelocity = iszero(m_curFrameTime) ? 0.0f : wantedEntMovement.t.GetLength() / m_curFrameTime;
		if ((requestedEntVelocity > 0.0f) && (pSkeletonAnim != NULL))
		{
			Vec3 assetAnimMovement = m_animLocation.q * pSkeletonAnim->GetRelTranslation();
			Vec3 requestedEntMovementDir = m_requestedEntityMovement.t.GetNormalizedSafe();
			float projectedActualAnimVelocity = (m_prevFrameTime > 0.0f) ? ((assetAnimMovement | requestedEntMovementDir) / m_prevFrameTime) : 0.0f;
			wantedEntMovement.t *= CLAMP(projectedActualAnimVelocity / requestedEntVelocity, 0.0f, 1.0f);
			wantedEntLocation = ApplyWorldOffset(m_entLocation, wantedEntMovement); // TODO: Make sure wantedEntLocation comes from entlocation+wantedmovement
		}
	}

	// TODO: Warning, this is framerate dependent, should be fixed.
	m_expectedEntMovementLengthPrev = LERP(m_expectedEntMovement.t.GetLength(), m_expectedEntMovementLengthPrev, 0.9f);

	m_expectedEntMovement = wantedEntMovement;

	assert(wantedEntLocation.IsValid());
	assert(wantedEntMovement.IsValid());

	/*
	float wantedEntRotZ = RAD2DEG(Ang3(wantedEntMovement.q).z);
	float wantedEntRotZAbs = abs(wantedEntRotZ);
	*/

	//DebugGraphQT(wantedEntMovement, eDH_ReqEntMovementTransX, eDH_ReqEntMovementTransY, eDH_ReqEntMovementRotZ);

	// This will update/activate the collider if needed, which makes it safe to move again.
	// (According to Craig we don't need to wake up other things that the requester might have put to sleep).
	if (!wantedEntMovement.t.IsZero())
		RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_ForceSleep, "ReqPhysEntMove");

	IEntity* pParent = pEntity->GetParent();
	if (pParent != NULL) 
	{
		// When mounted and we have entity driven movement the only code to update the entity matrix is the vehicle (ref: Michael Rauh)
		// TODO: If needed, find a solution that supports split horizontal & vertical components. Could just MergeMCM cur & wanted locations?
		//assert(GetMCMH() == GetMCMV());
		if (GetMCMH() != eMCM_Entity)
		{
			QuatT curEntLocationLocal(pEntity->GetLocalTM());
			curEntLocationLocal.q.Normalize();
			assert(curEntLocationLocal.IsValid());
			Quat ParentOrientation = pParent->GetWorldRotation();
			assert(ParentOrientation.IsValid());
			Quat ParentOrientationInv = ParentOrientation.GetInverted();
			assert(ParentOrientationInv.IsValid());
			QuatT wantedEntLocationLocal;
			wantedEntLocationLocal.t = curEntLocationLocal.t + ParentOrientationInv * wantedEntMovement.t;
			wantedEntLocationLocal.q = curEntLocationLocal.q * wantedEntMovement.q;
			assert(wantedEntLocationLocal.IsValid());
			Matrix34 wantedEntLocationLocalMat(wantedEntLocationLocal);
			assert(wantedEntLocationLocalMat.IsValid());
			pEntity->SetLocalTM(wantedEntLocationLocalMat);

			pe_action_move move;
			move.dir = ZERO;
			move.iJump = 0;
			pPhysEnt->Action(&move);
		}
	}
	else
	{
		{
			ANIMCHAR_PROFILE_SCOPE("UpdatePhysicalEntityMovement_RequestToPhysics");

			// Force normal movement type during animation controlled movement, 
			// since the entity needs to catch up regardless of what movement type game code requests.
			if ((m_requestedEntityMovementType == RequestedEntMovementType_Absolute) || 
					((GetMCMH() == eMCM_Animation) && (GetMCMV() == eMCM_Animation)))
			{
				float curFrameTimeInv = (m_curFrameTime != 0.0f) ? (1.0f / m_curFrameTime) : 0.0f;
				pe_action_move move;
				move.dir = wantedEntMovement.t * curFrameTimeInv;
				move.iJump = m_requestedIJump;

				assert(move.dir.IsValid());

				float curMoveVeloHash = 5.0f * move.dir.x + 7.0f * move.dir.y + 3.0f * move.dir.z;
				float actualMoveVeloHash = 5.0f * m_actualEntMovement.t.x * curFrameTimeInv + 7.0f * m_actualEntMovement.t.y * curFrameTimeInv + 3.0f * m_actualEntMovement.t.z * curFrameTimeInv;
				if ((curMoveVeloHash != m_prevMoveVeloHash) || (move.iJump != m_prevMoveJump) || (actualMoveVeloHash != curMoveVeloHash) || RecentQuickLoad())
				{
					ANIMCHAR_PROFILE_SCOPE("UpdatePhysicalEntityMovement_RequestToPhysics_Movement");

					if(!move.dir.IsZero() && !wantedEntMovement.t.IsZero())
					assert(m_colliderModeLayers[eColliderModeLayer_ForceSleep] == eColliderMode_Undefined);

					pPhysEnt->Action(&move);
					m_prevMoveVeloHash = curMoveVeloHash;
					m_prevMoveJump = move.iJump;

					pe_params_flags pf;
					if (m_bDisablePhysicalGravity || 
						(m_colliderMode == eColliderMode_Disabled) ||
						(m_colliderMode == eColliderMode_PushesPlayersOnly) || 
						(m_colliderMode == eColliderMode_Spectator) ||
						(GetMCMV() == eMCM_Animation))
						pf.flagsOR = pef_ignore_areas;
					else
						pf.flagsAND = ~pef_ignore_areas;
					pPhysEnt->SetParams(&pf);
				}
			}
			else if (m_requestedEntityMovementType == RequestedEntMovementType_Impulse)
			{
				ANIMCHAR_PROFILE_SCOPE("UpdatePhysicalEntityMovement_RequestToPhysics_Impulse");

				pe_action_impulse impulse;
				impulse.impulse = wantedEntMovement.t;
				impulse.iApplyTime = 0;

				pPhysEnt->Action(&impulse);
			}
			else
			{
				if (m_requestedEntityMovementType != RequestedEntMovementType_Undefined) // Can be underfined right after start, if there is no requested movement yet.
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "AnimatedCharacter::UpdatePhysicalEntityMovement() - Undefined movement type %d, expected absolute(1) or impulse(2).", m_requestedEntityMovementType);

				// When there is no valid requested movement, clear the requested velocity in living entity.
				pe_action_move move;
				move.dir = ZERO;
				move.iJump = 0;
				pPhysEnt->Action(&move);
			}
		}

		if (!NoMovementOverride())
		{
			ANIMCHAR_PROFILE_SCOPE("UpdatePhysicalEntityMovement_SetEntityRotation");

			assert(wantedEntLocation.q.IsValid());

			if (wantedEntLocation.q != m_entLocation.q)
				pEntity->SetRotation(wantedEntLocation.q, ENTITY_XFORM_USER|ENTITY_XFORM_NOT_REREGISTER);
		}
	}
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::UpdatePhysicalColliderMode()
{
	ANIMCHAR_PROFILE_DETAILED;

	bool debug = (CAnimationGraphCVars::Get().m_debugColliderMode != 0);

	// If filtered result is Undefined it will be forced to Pushable below.
	EColliderMode newColliderMode = eColliderMode_Undefined;

//#ifdef _DEBUG
	if (debug && DebugFilter())
	{
		if (m_isPlayer)
			m_colliderModeLayers[eColliderModeLayer_Debug] = (EColliderMode)CAnimationGraphCVars::Get().m_forceColliderModePlayer;
		else
			m_colliderModeLayers[eColliderModeLayer_Debug] = (EColliderMode)CAnimationGraphCVars::Get().m_forceColliderModeAI;
	}
//#endif

	bool disabled = false;
	for (int layer = 0; layer < eColliderModeLayer_COUNT; layer++)
	{
		if (m_colliderModeLayers[layer] != eColliderMode_Undefined)
		{
			newColliderMode = m_colliderModeLayers[layer];
			if (m_colliderModeLayers[layer] == eColliderMode_Disabled)
				disabled = true;
		}
	}

	if (disabled && (m_colliderModeLayers[eColliderModeLayer_Debug] == eColliderMode_Undefined))
		newColliderMode = eColliderMode_Disabled;

	if (newColliderMode == eColliderMode_Undefined) 
		newColliderMode = eColliderMode_Pushable;

//#ifdef _DEBUG
	if (debug && DebugFilter())
	{
		ColorF color(1,0.8f,0.6f,1);
		static float x = 50.0f;
		static float y = 70.0f;
		static float h = 20.0f;

		string name = GetEntity()->GetName();		
		gEnv->pRenderer->Draw2dLabel(x, y - h, 1.7f, (float*)&color, false, "ColliderMode (" + name + ")");

		string layerStr;
		int layer;
		const char* tag;
		for (layer = 0; layer < eColliderModeLayer_COUNT; layer++)
		{
			tag = (m_colliderModeLayersTag[layer] == NULL) ? "" : m_colliderModeLayersTag[layer];
			layerStr = string().Format("  %s(%d): %s(%d)   %s", g_szColliderModeLayerString[layer], layer,
				g_szColliderModeString[m_colliderModeLayers[layer]], m_colliderModeLayers[layer], tag);
			gEnv->pRenderer->Draw2dLabel(x, y + (float)layer * h, 1.7f, (float*)&color, false, layerStr);
		}
		layerStr = string().Format("  FINAL: %s(%d)", g_szColliderModeString[newColliderMode], newColliderMode);
		gEnv->pRenderer->Draw2dLabel(x, y + (float)layer * h, 1.7f, (float*)&color, false, layerStr);
	}
//#endif

	if ((newColliderMode == m_colliderMode) && !RecentQuickLoad() && !m_forcedRefreshColliderMode)
		return;

	if (!m_forcedRefreshColliderMode &&
		(newColliderMode != eColliderMode_Disabled) &&
		(newColliderMode != eColliderMode_PushesPlayersOnly) &&
		(newColliderMode != eColliderMode_Spectator) &&
		((m_curFrameID <= 0) || ((m_lastResetFrameId+1) > m_curFrameID)))
	{
			return;
	}

	m_forcedRefreshColliderMode = false;
	m_colliderMode = newColliderMode;

	assert(m_colliderMode != eColliderMode_Undefined);

	IEntity* pEntity = GetEntity();
	assert(pEntity);

	IPhysicalEntity *pPhysEnt = pEntity->GetPhysics();
	if (pPhysEnt == NULL || pPhysEnt->GetType()!=PE_LIVING)
		return;

	pe_player_dynamics pd;
	pe_params_part pp;
	pe_params_flags pf;
	pd.kInertia = m_params.inertia;
	pd.kInertiaAccel = m_params.inertiaAccel;

	if (m_colliderMode == eColliderMode_Disabled)
	{
		pd.bActive = 0;
		pd.collTypes = ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid|ent_living;
		pp.flagsAND = ~geom_colltype_player;

		pe_action_move move;
		move.dir = ZERO;
		move.iJump = 0;
		pPhysEnt->Action(&move);
		pf.flagsAND = ~(pef_ignore_areas|lef_loosen_stuck_checks);
	}
	else if (m_colliderMode == eColliderMode_GroundedOnly)
	{
		pd.bActive = 1;
		pd.collTypes = ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid|ent_living;
		pp.flagsAND = ~geom_colltype_player;
		pp.flagsColliderAND = ~geom_colltype_player;
		pf.flagsAND = ~(pef_ignore_areas|lef_loosen_stuck_checks);
	}
	else if (m_colliderMode == eColliderMode_Pushable)
	{
		pd.bActive = 1;
		pd.collTypes = ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid|ent_living;
		pp.flagsOR = geom_colltype_player;
		pp.flagsColliderOR = geom_colltype_player;
		pf.flagsOR = pef_pushable_by_players;
		pf.flagsAND = ~(pef_ignore_areas|lef_loosen_stuck_checks);
	}
	else if (m_colliderMode == eColliderMode_NonPushable)
	{
		pd.bActive = 1;
		pd.collTypes = ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid|ent_living;
		pp.flagsOR = geom_colltype_player;
		pp.flagsColliderOR = geom_colltype_player;
		pf.flagsOR = lef_loosen_stuck_checks;
		pf.flagsAND = ~pef_pushable_by_players & ~pef_ignore_areas;
	}
	else if (m_colliderMode == eColliderMode_PushesPlayersOnly)
	{
		pd.bActive = 1;
		pd.collTypes = ent_living;
		pd.gravity.zero();
		pd.kInertia = 0;
		pd.kInertiaAccel = 0;
		pp.flagsOR = geom_colltype_player;
		pp.flagsColliderOR = geom_colltype_player;
		pf.flagsOR = pef_ignore_areas|lef_loosen_stuck_checks;
		pf.flagsAND = ~pef_pushable_by_players;
	}
	else if (m_colliderMode == eColliderMode_Spectator)
	{
		pd.bActive = 1;
		pd.collTypes = ent_terrain;

		if(ICVar* pCollideWithBuildings = gEnv->pConsole->GetCVar("g_spectatorcollisions"))
		{
			if(pCollideWithBuildings->GetIVal())
			{
				pd.collTypes |= ent_static;
			}
		}

		pp.flagsAND = ~geom_colltype_player;
		pp.flagsColliderOR = geom_colltype_player;

		pe_action_move move;
		move.dir = ZERO;
		move.iJump = 0;
		pPhysEnt->Action(&move);
		pd.gravity.zero();
		pf.flagsOR = pef_ignore_areas;
		pf.flagsAND = ~lef_loosen_stuck_checks;
	}
	else
	{
		assert(!"ColliderMode not implemented!");
		return;
	}

	if ((m_colliderMode == eColliderMode_NonPushable) && (m_pFeetColliderPE == NULL))
		CreateExtraSolidCollider();

	if ((m_colliderMode != eColliderMode_NonPushable) && (m_pFeetColliderPE != NULL))
		DestroyExtraSolidCollider();

	pp.ipart = 0;
	pPhysEnt->SetParams(&pd);
	pPhysEnt->SetParams(&pp);
	pPhysEnt->SetParams(&pf);
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::CreateExtraSolidCollider()
{
	// TODO: The solid collider created below is not supported/ignored by AI walkability tests.
	// It needs to be disabled until AI probes are adjusted.
	if (CAnimationGraphCVars::Get().m_enableExtraSolidCollider == 0)
		return;

	if (m_pFeetColliderPE != NULL)
		DestroyExtraSolidCollider();

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();
	if (pPhysEnt == NULL)
		return;

	pe_params_pos pp;
	pp.pos = GetEntity()->GetWorldPos();
	pp.iSimClass = 2;
	m_pFeetColliderPE = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_ARTICULATED, &pp, GetEntity(), PHYS_FOREIGN_ID_ENTITY);

	primitives::capsule prim;
	prim.axis.Set(0,0,1);
	prim.center.zero();
	prim.r = 0.45f; prim.hh = 0.6f;
	IGeometry *pPrimGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::capsule::type, &prim);
	phys_geometry *pGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(pPrimGeom, 0);
	pGeom->nRefCount = 0;

	pe_geomparams gp;
	gp.pos.zero();
	gp.flags = geom_colltype_solid & ~geom_colltype_player;
	gp.flagsCollider = 0;
	gp.mass = 0.0f;
	gp.density = 0.0f;
	//gp.minContactDist = 0.0f;
	m_pFeetColliderPE->AddGeometry(pGeom, &gp, 102); // some arbitrary id (100 main collider, 101 veggie collider)

	pe_params_articulated_body pab;
	pab.pHost = pPhysEnt;
	pab.posHostPivot = Vec3(0, 0, 0.8f);
	pab.bGrounded = 1;
	m_pFeetColliderPE->SetParams(&pab);
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::DestroyExtraSolidCollider()
{
	if (m_pFeetColliderPE == NULL)
		return;

	gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pFeetColliderPE);
	m_pFeetColliderPE = NULL;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::UpdatePhysicsInertia()
{
	ANIMCHAR_PROFILE_DETAILED;

	if (m_elapsedTimeMCM[eMCMComponent_Horizontal] > 0.5f)
		return;

	IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysics();
	if (pPhysEnt)
	{
		int mcmh = GetMCMH();

		pe_player_dynamics dynOld;
		if (pPhysEnt->GetParams(&dynOld) != 0)
		{
			if ((mcmh == eMCM_DecoupledCatchUp) || (mcmh == eMCM_Entity))
			{
				if ((dynOld.kInertia != m_params.inertia) || 
						(dynOld.kInertiaAccel != m_params.inertiaAccel) || 
						(dynOld.timeImpulseRecover != m_params.timeImpulseRecover))
				{
					pe_player_dynamics dynNew;
					dynNew.kInertia = m_params.inertia;
					dynNew.kInertiaAccel = m_params.inertiaAccel;
					dynNew.timeImpulseRecover = m_params.timeImpulseRecover;
					pPhysEnt->SetParams(&dynNew);
				}
			}
			else if (mcmh == eMCM_Animation)
			{
				if ((dynOld.kInertia > 0.0f) || 
						(dynOld.kInertiaAccel > 0.0f) ||
						(dynOld.timeImpulseRecover > 0.0f))
				{
					pe_player_dynamics dynNew;
					dynNew.kInertia = 0.0f;
					dynNew.kInertiaAccel = 0.0f;
					dynNew.timeImpulseRecover = 0.0f;
					pPhysEnt->SetParams(&dynNew);
				}
			}
		}

		/*
		if (DebugTextEnabled())
		{
		ColorF colorWhite(1,1,1,1);
		gEnv->pRenderer->Draw2dLabel(500, 35, 1.0f, (float*)&colorWhite, false, "Inertia [%.2f, %.2f]", dynNew.kInertia, dynNew.kInertiaAccel);
		}
		*/
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//- PreRenderUpdate --------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

void CAnimatedCharacter::ClampAnimLocation()
{
	ANIMCHAR_PROFILE;

	// Since parent location is lagging one frame before this function, 
	// we can't us it for conversions between local and world space.
	// Thus vehicle enter/exiting animation movement is applied local entity location, not world animation location.
	// The reliable entity location is updating animation location here instead.
	IEntity* pParent = GetEntity()->GetParent();
	if (m_simplifyMovement || (pParent != NULL) || NoMovementOverride())
	{
		m_animLocation = m_entLocation;
	}
	else
	{
		QuatT entLocationClamped = m_entLocation; // We don't want to change the actual m_entLocation here.
		QuatT animLocationClamped = m_animLocation; // (We don't want to change the actual m_animLocation here.)
		ClampLocations(entLocationClamped, animLocationClamped);

		if (GetMCMV() == eMCM_SmoothedEntity)
		{
			static float elevationBlendDuration = 0.1f;
			float elevationBlend = CLAMP(m_curFrameTime / elevationBlendDuration, 0.0f, 1.0f);

			// Blend out vertical lag when not moving (e.g. standing still on a vertically moving platform).
			float velocityFractionH = CLAMP(m_actualEntVelocity / 0.1f, 0.0f, 1.0f);
			elevationBlend = LERP(1.0f, elevationBlend, velocityFractionH);

			// Blend out vertical lag when not moving (e.g. standing still on a vertically moving platform).
			float prevFrameTimeInv = (m_prevFrameTime > 0.0f) ? (1.0f / m_prevFrameTime) : 0.0f;
			float actualEntVelocityZ = m_actualEntMovement.t.z * prevFrameTimeInv;
			float velocityFractionZ = CLAMP(actualEntVelocityZ / 1.0f, 0.0f, 1.0f);
			elevationBlend = LERP(elevationBlend, 1.0f, velocityFractionZ);

			m_animLocation.q = animLocationClamped.q;
			m_animLocation.t.x = animLocationClamped.t.x;
			m_animLocation.t.y = animLocationClamped.t.y;
			m_animLocation.t.z = LERP(m_animLocation.t.z, animLocationClamped.t.z, elevationBlend);
		}
		else
		{
			m_animLocation = animLocationClamped;
		}

		assert(m_animLocation.IsValid());
	}

	// Update actual anim movement.
	m_prevAnimLocation = m_animLocation;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::GetCurrentEntityLocation()
{
	m_prevEntLocation = m_entLocation;

	IEntity* pEntity = GetEntity();
	assert(pEntity != NULL);

	m_entLocation.t = pEntity->GetWorldPos();
	m_entLocation.q = pEntity->GetWorldRotation();
	assert(m_entLocation.IsValid());

	// TODO: Warning, this is framerate dependent, should be fixed.
	m_actualEntMovementLengthPrev = LERP(m_actualEntMovement.t.GetLength(), m_actualEntMovementLengthPrev, 0.9f);

	m_actualEntMovement = GetWorldOffset(m_prevEntLocation, m_entLocation);

#ifdef _DEBUG
	//DebugHistory_AddValue("eDH_TEMP02", (m_prevFrameTime != 0.0f) ? m_actualEntMovement.t.GetLength2D() / m_prevFrameTime : 0.0f);
#endif

#ifdef _DEBUG
	DebugGraphQT(m_entTeleportMovement, "eDH_EntTeleportMovementTransX", "eDH_EntTeleportMovementTransY", "eDH_EntTeleportMovementRotZ");
#endif

	m_entTeleportMovement.SetIdentity();
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::GetMaxAnimErrorMCM(int mcm, float& maxDistance, float& maxAngle) const
{
#ifdef _DEBUG // Only touch the cvar memory in debug
	maxDistance = CAnimationGraphCVars::Get().m_animErrorMaxDistance;
	maxAngle = CAnimationGraphCVars::Get().m_animErrorMaxAngle;
#else
	// These are the current values of the cvars, but better check on them from time to time.
	maxDistance = 0.5f;
	maxAngle = 45.0f;
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::ClampLocations(QuatT& wantedEntLocation, QuatT& wantedAnimLocation)
{
	ANIMCHAR_PROFILE_DETAILED;

	// Avoid touching cvar memory
#ifdef _DEBUG
	float clampFadeInDurationEnt = CAnimationGraphCVars::Get().m_clampDurationEntity;
#else
	float clampFadeInDurationEnt = 0.7f;
#endif
	if ((GetMCMH() == eMCM_Entity) && (m_elapsedTimeMCM[eMCMComponent_Horizontal] > clampFadeInDurationEnt) && 
		(GetMCMV() == eMCM_Entity) && (m_elapsedTimeMCM[eMCMComponent_Vertical] > clampFadeInDurationEnt))
	{
		wantedAnimLocation = wantedEntLocation;
		return;
	}

	// Avoid touching cvar memory
#ifdef _DEBUG
	float clampFadeInDurationAnim = CAnimationGraphCVars::Get().m_clampDurationAnimation;
#else
	float clampFadeInDurationAnim = 0.3f;
#endif
	if ((GetMCMH() == eMCM_Animation) && (m_elapsedTimeMCM[eMCMComponent_Horizontal] > clampFadeInDurationAnim) && 
		(GetMCMV() == eMCM_Animation) && (m_elapsedTimeMCM[eMCMComponent_Vertical] > clampFadeInDurationAnim))
	{
		wantedEntLocation = wantedAnimLocation;
		return;
	}

	assert(wantedEntLocation.IsValid());
	assert(wantedAnimLocation.IsValid());

	// TODO: Are these needed for the clamping? Can the CombineComponents take care is the extraction instead?
	QuatT wantedEntLocationH = ExtractHComponent(wantedEntLocation);
	QuatT wantedEntLocationV = ExtractVComponent(wantedEntLocation);
	QuatT wantedAnimLocationH = ExtractHComponent(wantedAnimLocation);
	QuatT wantedAnimLocationV = ExtractVComponent(wantedAnimLocation);

	assert(wantedEntLocationH.IsValid());
	assert(wantedEntLocationV.IsValid());
	assert(wantedAnimLocationH.IsValid());
	assert(wantedAnimLocationV.IsValid());

	// Adjust wanted animation and entity location, depending on the current movement control method.
	ClampLocationsMCM(GetMCMH(), wantedEntLocationH, wantedAnimLocationH, m_elapsedTimeMCM[eMCMComponent_Horizontal], eMCMComponent_Horizontal, true);
	ClampLocationsMCM(GetMCMV(), wantedEntLocationV, wantedAnimLocationV, m_elapsedTimeMCM[eMCMComponent_Vertical], eMCMComponent_Vertical, false);

	assert(wantedEntLocationH.IsValid());
	assert(wantedEntLocationV.IsValid());
	assert(wantedAnimLocationH.IsValid());
	assert(wantedAnimLocationV.IsValid());

	wantedEntLocation = CombineHVComponents2D(wantedEntLocationH, wantedEntLocationV);
	wantedAnimLocation = CombineHVComponents2D(wantedAnimLocationH, wantedAnimLocationV);

	assert(wantedEntLocation.IsValid());
	assert(wantedAnimLocation.IsValid());
}

//--------------------------------------------------------------------------------

// Clamp/Adjust wanted animation and entity movement and state, depending on the current movement control method.
void CAnimatedCharacter::ClampLocationsMCM(EMovementControlMethod mcm, QuatT& wantedEntLocation, QuatT& wantedAnimLocation, float elapsedMCMTime, EMCMComponent mcmcmp, bool debug)
{
	float clampTime = 0.0f;
#ifndef _DEBUG
	float clampDurationEntity = 0.7f;
	float clampDurationAnimation = 0.3f;
#else
	// Only touch the cvar memory in debug.
	float clampDurationEntity = CAnimationGraphCVars::Get().m_clampDurationEntity;
	float clampDurationAnimation = CAnimationGraphCVars::Get().m_clampDurationAnimation;
#endif
	switch (mcm)
	{
	case eMCM_Entity: case eMCM_ClampedEntity: clampTime = clampDurationEntity; break;
	case eMCM_Animation: clampTime = clampDurationAnimation; break;
	}

	float timefraction = (clampTime > 0.0f) ? CLAMP(elapsedMCMTime / clampTime, 0, 1) : 1.0f;
	float scale = 1.0f - timefraction;

	float distance, angle;
	float distanceMax, angleMax;

	GetMaxAnimErrorMCM(mcm, distanceMax, angleMax);

	// TODO: Why was this max?! Should be min, right?
	// There is a desired max, but also the actual current, which should not get worse, so we use it as a max.

	if (mcm != eMCM_DecoupledCatchUp)
	{
		distanceMax = min(m_prevOffsetDistance, distanceMax);
		angleMax = min(m_prevOffsetAngle, angleMax);
	}
	else
	{
		distanceMax = max(m_prevOffsetDistance, distanceMax);
		angleMax = max(m_prevOffsetAngle, angleMax);
	}

	{
		ANIMCHAR_PROFILE_SCOPE("ClampLocationsMCM_ApplyClampingOverrides");
		ApplyAnimGraphClampingOverrides(distanceMax, angleMax);
	}

	if (mcm != eMCM_DecoupledCatchUp)
	{
		if (mcm != eMCM_ClampedEntity)
		{
			distanceMax *= scale;
		}
		angleMax *= scale;
	}

	// Just make sure some broken asset or whatever didn't set totally insane values.
	distanceMax = min(distanceMax, 2.0f);
	angleMax = min(angleMax, 360.0f);

	QuatT offset = GetWorldOffset(wantedEntLocation, wantedAnimLocation);

	// Retract offset in colliding direction (prevent characters from pushing into each other while decoupled).
	if (RecentCollision())
	{
		offset.t = RemovePenetratingComponent(offset.t, m_collisionNormal[0], 0.0f, 0.1f);
		offset.t = RemovePenetratingComponent(offset.t, m_collisionNormal[1], 0.0f, 0.1f);
		offset.t = RemovePenetratingComponent(offset.t, m_collisionNormal[2], 0.0f, 0.1f);
		offset.t = RemovePenetratingComponent(offset.t, m_collisionNormal[3], 0.0f, 0.1f);
	}

	offset = GetClampedOffset(offset, distanceMax, angleMax, distance, angle);

	// TODO: Don't remember why elapsedMCMTime == 0.0f is needed here. Might be removable.
	if ((mcmcmp == eMCMComponent_Horizontal) && ((elapsedMCMTime == 0.0f) || (mcm == eMCM_DecoupledCatchUp)))
	{
		m_prevOffsetDistance = min(distance, distanceMax);
		m_prevOffsetAngle = min(angle, angleMax);
	}

	QuatT clampedEntLocation = wantedEntLocation;
	QuatT clampedAnimLocation = wantedAnimLocation;
	if (mcm == eMCM_Animation)
	{
		offset.q.Invert();
		offset.t *= -1.0f;
		clampedEntLocation = ApplyWorldOffset(wantedAnimLocation, offset);
	}
	else
	{
		clampedAnimLocation = ApplyWorldOffset(wantedEntLocation, offset);
	}

	// TODO: Add another cvar to be able to disable vertical clamping as well.
#ifdef _DEBUG
	if (CAnimationGraphCVars::Get().m_animErrorClamp != 0)
#endif
	{
		wantedEntLocation = clampedEntLocation;
		wantedAnimLocation = clampedAnimLocation;
	}
#ifdef _DEBUG
	else
	{
		// If clamping is turned off, only keep the vertical clamping (since we want that always anyway).
		//wantedEntLocation = CombineHVComponents(ExtractHComponent(wantedEntLocation), ExtractVComponent(clampedEntLocation));
		//wantedAnimLocation = CombineHVComponents(ExtractHComponent(wantedAnimLocation), ExtractVComponent(clampedAnimLocation));
		wantedEntLocation = CombineHVComponents2D(wantedEntLocation, clampedEntLocation);
		wantedAnimLocation = CombineHVComponents2D(wantedAnimLocation, clampedAnimLocation);
	}
#endif

#ifdef _DEBUG
	if (debug)
	{
		DebugAnimEntDeviation(offset, distance, angle, distanceMax, angleMax);
	}
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::ApplyAnimGraphClampingOverrides(float& distanceMax, float& angleMax)
{
	if (m_overrideClampDistance != -1.0f)
		distanceMax = m_overrideClampDistance;

	if (m_overrideClampAngle != -1.0f)
		angleMax = m_overrideClampAngle;
}

//--------------------------------------------------------------------------------

bool CAnimatedCharacter::EnableProceduralLeaning()
{
	return ( m_moveRequest.proceduralLeaning > 0.0f );
}

//--------------------------------------------------------------------------------

float CAnimatedCharacter::GetProceduralLeaningScale()
{
	return m_moveRequest.proceduralLeaning;
}

//--------------------------------------------------------------------------------

QuatT CAnimatedCharacter::CalculateProceduralLeaning()
{
	ANIMCHAR_PROFILE_DETAILED;

	float frameTimeScale = (m_curFrameTime > 0.0f) ? (1.0f / (m_curFrameTime / 0.02f)) : 0.0f;
	float curving = 0.0f;
	Vec2 prevVelo(ZERO);
	Vec3 avgAxx(0);
	float weightSum = 0.0f;
	CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();
	for (int i = 0; i < NumAxxSamples; i++)
	{
		int j = (m_reqLocalEntAxxNextIndex + i) % NumAxxSamples;

		// AGE WEIGHT
		float age = (curTime - m_reqEntTime[j]).GetSeconds();
		float ageFraction = CLAMP(age / 0.3f, 0.0f, 1.0f);
		float weight = (0.5f - fabs(0.5f - ageFraction)) * 2.0f;
		weight = 1.0f - sqr(1.0f - weight);
		weight = sqr(weight);

		weightSum += weight;
		avgAxx.x += m_reqLocalEntAxx[j].x * weight * frameTimeScale;
		avgAxx.y += m_reqLocalEntAxx[j].y * weight * frameTimeScale;

		// CURVING
		float area = fabs(m_reqEntVelo[j].Cross(prevVelo));
		float len = prevVelo.GetLength() * m_reqEntVelo[j].GetLength();
		if (len > 0.0f)
			area /= len;
		curving += area * weight;
		prevVelo = m_reqEntVelo[j];
	}
	if (weightSum > 0.0f)
	{
		avgAxx /= weightSum;
		curving /= weightSum;
	}

/*
	Vec3 curAxx;
	curAxx.x = m_reqLocalEntAxx[(m_reqLocalEntAxxNextIndex-1)%NumAxxSamples].x * frameTimeScale;
	curAxx.y = m_reqLocalEntAxx[(m_reqLocalEntAxxNextIndex-1)%NumAxxSamples].y * frameTimeScale;
*/

	float curvingFraction = CLAMP(curving / 0.3f, 0.0f, 1.0f);
	curvingFraction = 1.0f - sqr(1.0f - curvingFraction);
	float pulldownFraction = sqr(1.0f - CLAMP(curvingFraction / 0.3f, 0.0f, 1.0f));

	Vec3 prevActualEntVeloW = (m_curFrameTime > 0.0f) ? (m_requestedEntityMovement.t / m_curFrameTime) : ZERO;
	//Vec3 prevActualEntVeloW = (m_curFrameTime > 0.0f) ? (m_actualEntMovement.t / m_curFrameTime) : ZERO;
	prevActualEntVeloW.z = 0.0f; // Only bother about horizontal accelerations.
	float smoothVeloBlend = CLAMP(m_curFrameTime / 0.1f, 0.0f, 1.0f);
	Vec3 prevSmoothedActualEntVelo = m_smoothedActualEntVelo;
	m_smoothedActualEntVelo = LERP(m_smoothedActualEntVelo, prevActualEntVeloW, smoothVeloBlend);
	Vec3 smoothedActualEntVeloL = m_entLocation.q.GetInverted() * m_smoothedActualEntVelo;

	Vec3 deltaVelo = (m_smoothedActualEntVelo - prevSmoothedActualEntVelo);
	deltaVelo = m_entLocation.q.GetInverted() * deltaVelo;
	m_reqLocalEntAxx[m_reqLocalEntAxxNextIndex].x = deltaVelo.x;
	m_reqLocalEntAxx[m_reqLocalEntAxxNextIndex].y = deltaVelo.y;
	m_reqEntVelo[m_reqLocalEntAxxNextIndex].x = m_smoothedActualEntVelo.x;
	m_reqEntVelo[m_reqLocalEntAxxNextIndex].y = m_smoothedActualEntVelo.y;
	m_reqEntTime[m_reqLocalEntAxxNextIndex] = gEnv->pTimer->GetFrameStartTime();
	m_reqLocalEntAxxNextIndex = (m_reqLocalEntAxxNextIndex + 1) % NumAxxSamples;

	// EQUALIZE VELOCITIES
	float velo = m_smoothedActualEntVelo.GetLength();
	float equalizeScale = 1.0f;
	if ((velo >= 0.0f) && (velo < 1.5f))
	{
		float fraction = (velo - 0.0f) / 1.5f;
		equalizeScale *= LERP(1.0f, 2.5f, fraction);
	}
	if ((velo >= 1.5f) && (velo < 3.0f))
	{
		float fraction = (velo - 1.5f) / 1.5f;
		equalizeScale *= LERP(2.5f, 1.0f, fraction);
	}
	else if ((velo >= 3.0f) && (velo < 6.0f))
	{
		float fraction = (velo - 3.0f) / 3.0f;
		equalizeScale *= LERP(1.0f, 0.7f, fraction);
	}
	else if ((velo >= 6.0f) && (velo < 10.0f))
	{
		float fraction = (velo - 6.0f) / 4.0f;
		equalizeScale *= LERP(0.7f, 0.3f, fraction);
	}
	else if ((velo >= 10.0f) && (velo < 50.0f))
	{

		float fraction = (min(velo,20.0f) - 10.0f) / 10.0f;
		equalizeScale *= LERP(0.3f, 0.1f, fraction);
	}

	float scale = 1.0f;
	scale *= (1.0f + 1.0f * curvingFraction);
	scale *= equalizeScale;
	scale *= 1.0f;
	Vec3 avgAxxScaled = avgAxx * scale;

	// CLAMP AMOUNT
	float alignmentOriginal = (smoothedActualEntVeloL | m_avgLocalEntAxx);
	float amount = avgAxxScaled.GetLength();
	float amountMaxNonCurving = (0.5f + 0.5f * CLAMP(alignmentOriginal * 2.0f, 0.0f, 1.0f));
	float amountMax = 0.35f;
	amountMax *= LERP(amountMaxNonCurving, 1.0f, curvingFraction);
	if (amount > 0.0f)
		avgAxxScaled = (avgAxxScaled / amount) * min(amount, amountMax);

	float axxContinuityMag = avgAxxScaled.GetLength() * m_avgLocalEntAxx.GetLength();
	float axxContinuityDot = (avgAxxScaled | m_avgLocalEntAxx);
	float axxContinuity = (axxContinuityMag > 0.0f) ? (axxContinuityDot / axxContinuityMag) : 0.0f;
	axxContinuity = CLAMP(axxContinuity, 0.0f, 1.0f);
	float axxBlend = (m_curFrameTime / 0.2f);
	axxBlend *= 0.5f + 0.5f * axxContinuity;
	m_avgLocalEntAxx = LERP(m_avgLocalEntAxx, avgAxxScaled, CLAMP(axxBlend, 0.0f, 1.0f));

	amount = m_avgLocalEntAxx.GetLength();

	Vec3 lean = Vec3(0,0,1) + m_avgLocalEntAxx;
	lean.NormalizeSafe(Vec3(0,0,1));
	float pulldown = max(LERP((-0.0f), (-0.25f), pulldownFraction), -amount * 0.6f);
	//pulldown = 0.0f;

	QuatT leaning;
	leaning.q.SetRotationV0V1(Vec3(0,0,1), lean);
	leaning.t.zero();
	leaning.t = -m_avgLocalEntAxx;
	leaning.t *= (0.8f + 0.4f * curvingFraction);
	leaning.t.z = pulldown;

	leaning.t *= 0.1f;
	if (!m_isPlayer)
		leaning.t *= 0.5f;

	float proceduralLeaningScale = GetProceduralLeaningScale();
	CLAMP( proceduralLeaningScale, 0.0f, 1.0f );
	leaning = leaning.GetScaled( proceduralLeaningScale );

#if _DEBUG && defined(USER_david)
	if (DebugFilter())
	{
		CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();

		if (pPD != NULL)
		{
/*
			DebugHistory_AddValue("eDH_TEMP00", m_actualEntMovement.t.x / m_curFrameTime);
			DebugHistory_AddValue("eDH_TEMP01", m_actualEntMovement.t.y / m_curFrameTime);
			DebugHistory_AddValue("eDH_TEMP02", m_smoothedActualEntVelo.x);
			DebugHistory_AddValue("eDH_TEMP03", m_smoothedActualEntVelo.y);
			DebugHistory_AddValue("eDH_TEMP04", curAxx.x);
			DebugHistory_AddValue("eDH_TEMP05", curAxx.y);
			DebugHistory_AddValue("eDH_TEMP06", avgAxx.x);
			DebugHistory_AddValue("eDH_TEMP07", avgAxx.y);

			DebugHistory_AddValue("eDH_TEMP10", alignmentOriginal);
			DebugHistory_AddValue("eDH_TEMP11", amountMaxNonCurving);
			DebugHistory_AddValue("eDH_TEMP12", equalizeScale);
			DebugHistory_AddValue("eDH_TEMP13", curvingFraction);
			DebugHistory_AddValue("eDH_TEMP14", scale);

			DebugHistory_AddValue("eDH_TEMP15", amount);
			DebugHistory_AddValue("eDH_TEMP16", m_avgLocalEntAxx.x);
			DebugHistory_AddValue("eDH_TEMP17", m_avgLocalEntAxx.y);
/**/
//*
			DebugHistory_AddValue("eDH_TEMP00", m_smoothedActualEntVelo.x);
			DebugHistory_AddValue("eDH_TEMP01", m_smoothedActualEntVelo.y);
			DebugHistory_AddValue("eDH_TEMP02", m_avgLocalEntAxx.x);
			DebugHistory_AddValue("eDH_TEMP03", m_avgLocalEntAxx.y);

			DebugHistory_AddValue("eDH_TEMP10", curving);
			DebugHistory_AddValue("eDH_TEMP11", curvingFraction);
			DebugHistory_AddValue("eDH_TEMP12", pulldownFraction);
			DebugHistory_AddValue("eDH_TEMP13", pulldown);

			DebugHistory_AddValue("eDH_TEMP06", alignmentOriginal);
/**/

/*
 			Vec3 bump(0,0,0.1f);
			pPD->Begin(UNIQUE("AnimatedCharacter.FakeLeaning"), true);
			pPD->AddLine(m_animLocation.t+bump, m_animLocation.t+bump + m_entLocation.q*lean, ColorF(1,1,1,1), 0.5f);
 			pPD->AddLine(m_animLocation.t+bump+Vec3(0,0,1), m_animLocation.t+bump+Vec3(0,0,1) + m_entLocation.q*m_avgLocalEntAxx, ColorF(0.5f,1,0.5f,1), 0.5f);
			pPD->Begin(UNIQUE("AnimatedCharacter.FakeLeaningAxx"), false);
			pPD->AddLine(m_animLocation.t+bump, m_animLocation.t+bump + m_entLocation.q*m_avgLocalEntAxx, ColorF(1.0f,1.0f,1.0f,1), 5.0f);
*/
		}
	}
#endif

	return leaning;
}

QuatT CAnimatedCharacter::CalculateAnimRenderLocation()
{
	assert(m_entLocation.IsValid());
	assert(m_animLocation.IsValid());

	if (NoMovementOverride())
		return m_entLocation;

	QuatT proceduralLeaning;
	if (EnableProceduralLeaning() && !NoMovementOverride() && 
			((m_pAnimTarget == NULL) || (!m_pAnimTarget->activated)) &&
			(GetMCMH() == eMCM_DecoupledCatchUp)) // This was added to primarily prevent deviation while parachuting.
		proceduralLeaning = CalculateProceduralLeaning();
	else
		proceduralLeaning.SetIdentity();

	QuatT animRenderLocation = m_animLocation * m_extraAnimationOffset * proceduralLeaning;
	return animRenderLocation;
}

//--------------------------------------------------------------------------------
