#include "StdAfx.h"
#include "ExactPositioning.h"
#include "PersistantDebug.h"

#include "AnimatedCharacter.h"

// Collision ray piercability to ignore leaves and other things
// NOTE: This is copy&pasted from Actor.h!
#define COLLISION_RAY_PIERCABILITY 10

// enable this to check nan's on position updates... useful for debugging some weird crashes
#define ENABLE_NAN_CHECK

#undef CHECKQNAN_FLT
#if defined(ENABLE_NAN_CHECK) && !defined(CHECKQNAN_FLT)
#define CHECKQNAN_FLT(x) \
	assert(*(unsigned*)&(x) != 0xffffffffu)
#else
#define CHECKQNAN_FLT(x) (void*)0
#endif

#ifndef CHECKQNAN_VEC
#define CHECKQNAN_VEC(v) \
	CHECKQNAN_FLT(v.x); CHECKQNAN_FLT(v.y); CHECKQNAN_FLT(v.z)
#endif

static const float EXTRA_DIRECTION_SLOP_IN_PREPARATION = DEG2RAD(30.0f);
static const float TARGET_POSITION_DRIFT_CAUSING_REPATHFIND = 0.85f; // meters
static const float CHECK_TARGET_POSITION_INTERVAL = 0.2f; // seconds
static const float RETRY_PATHFIND_INTERVAL_NORMAL = 0.1f; // seconds
static const float RETRY_PATHFIND_INTERVAL_CONSIDERING = 0.3f; // seconds
static const float TIME_DISABLED_BEFORE_IRRELEVANT = 3.0f; // seconds

ITimer * CExactPositioning::m_pTimer = 0;

CExactPositioning::SDynamicState::SDynamicState() :
	state(eTS_Disabled),
	startStateID(INVALID_STATE),
	targetStateID(INVALID_STATE),
	disabledTime(0),
	checkTime(0)
{
}

CExactPositioning::CExactPositioning( CAnimationGraphState * pState ) :
	m_pState(pState),
	m_triggerRecalculate(false),
	m_triggerQueryEnd(0),
	m_triggerQueryStart(0),
	m_pTriggerQueryStartRequest(0),
	m_pTriggerQueryEndRequest(0),
	m_triggerUser(eAGTU_AI)
{
	if (!m_pTimer)
		m_pTimer = gEnv->pTimer;
}

CExactPositioning::~CExactPositioning()
{
	if (m_triggerQueryStart)
	{
		m_pState->SendQueryComplete(m_triggerQueryStart, false);
		m_triggerQueryStart = 0;
	}
	if (m_triggerQueryEnd)
	{
		m_pState->SendQueryComplete(m_triggerQueryEnd, false);
		m_triggerQueryEnd = 0;
	}
}

void CExactPositioning::FrameUpdate()
{
	float frameTime = m_pTimer->GetFrameTime();

	if (m_ds.state == eTS_Disabled)
		m_ds.disabledTime += frameTime;
	m_ds.checkTime += frameTime;
	m_ds.pathfindRetryTimer -= frameTime;

	// keep checking for a valid path to emerge at regular intervals;
	// if we're still considering, we have more rigorous criteria (see CheckTriggers)
	// otherwise, just a simple polling at RETRY_PATHFIND_INTERVAL will suffice
	if (m_ds.state > eTS_Considering && m_ds.state <= eTS_Preparing && !m_ds.pathFindPerformed && m_ds.pathfindRetryTimer <= 0.0f)
	{
		MaybeRecalculateTriggering();
		m_ds.pathfindRetryTimer = RETRY_PATHFIND_INTERVAL_NORMAL;
	}

	if (m_ds.state >= eTS_Running && m_pState->GetEntity())
	{
		bool updatedTime = false;
		if (ICharacterInstance * pChar = m_pState->GetEntity()->GetCharacter(0))
		{
			ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();
			int nAnims = pSkel->GetNumAnimsInFIFO(0);
			int i=0;
			if ( nAnims > 0 )
			{
				CAnimation& anim = pSkel->GetAnimFromFIFO(0,0);
				if (anim.m_bActivated && anim.m_AnimParams.m_nUserToken == m_ds.token)
				{
					if (anim.m_nLoopCount >= 1)
						m_ds.target.activationTimeRemaining = -1.0f;
					else
					{
						assert(anim.m_fAnimTime >= 0.0f);
						assert(anim.m_fAnimTime <= 1.0f);
						float animTimeRemaining = (1.0f - anim.m_fAnimTime) * pChar->GetIAnimationSet()->GetDuration_sec(anim.m_LMG0.m_nAnimID[0]);
						assert(animTimeRemaining >= 0.0f);
						assert(m_ds.target.activationTimeRemaining >= 0.0f);
						m_ds.target.activationTimeRemaining = std::min(animTimeRemaining, m_ds.target.activationTimeRemaining);
						assert(m_ds.target.activationTimeRemaining >= 0.0f);

						if ( nAnims > 1 )
						{
							CAnimation& anim1 = pSkel->GetAnimFromFIFO(0,1);
							if ( anim1.m_bActivated )
								m_ds.target.activationTimeRemaining = -1.0f;
						}
					}
					updatedTime = true;
				}
			}
		}
		if (!updatedTime)
		{
			assert(m_ds.target.activationTimeRemaining >= 0.0f);
			m_ds.target.activationTimeRemaining -= m_pTimer->GetFrameTime();
			if (m_ds.target.activationTimeRemaining <= 0.0f)
				m_ds.target.activationTimeRemaining = -1.0f;
			//assert(m_ds.target.activationTimeRemaining >= 0.0f);
		}
	}

	const QuatT& curLocation = m_pState->GetAnimatedCharacter()->GetAnimLocation();
	//QuatT curLocation(m_pState->GetEntity()->GetWorldPos(), m_pState->GetEntity()->GetWorldRotation());

	if (m_pState->GetEntity())
		m_ds.trigger.Update( m_pTimer->GetFrameTime(), curLocation.t, curLocation.q, true );

	if (m_ds.state == eTS_Preparing && m_ds.trigger.IsTriggered())
		SetTriggerState(eTS_FinalPreparation);
}

bool CExactPositioning::SetTrigger( const SAnimationTargetRequest& req, EAnimationGraphTriggerUser user, TAnimationGraphQueryID * pQueryStart, TAnimationGraphQueryID * pQueryEnd )
{
	if (m_triggerUser > user)
		return false;
	for (int i=0; i<CAnimationGraph::MAX_INPUTS; i++)
	{
		m_triggerRequestInputs[i] = CStateIndex::INPUT_VALUE_DONT_CARE;
		m_triggerRequestInputsAsFloats[i] = 0.0f;
	}

	assert(req.position.GetLength() > 0.1f);
	assert(req.direction.GetLength() > 0.5f);
	assert(req.positionRadius >= 0.0f);
	assert(req.directionRadius >= DEG2RAD(0.0f));
	assert(req.prepareRadius > req.startRadius);
	assert(req.startRadius >= 0.0f);

	m_triggerRequest = req;
	m_triggerCommit = true;
	m_pTriggerQueryStartRequest = pQueryStart;
	m_pTriggerQueryEndRequest = pQueryEnd;
	m_triggerUser = user;
	return true;
}

void CExactPositioning::ClearTrigger( EAnimationGraphTriggerUser user )
{ 
	if (m_triggerUser > user)
		return;
	if (m_ds.state < eTS_Running) 
		SetTriggerState(eTS_Disabled); 
}

void CExactPositioning::SetTriggerState( ETriggerState state )
{
	ANIM_PROFILE_FUNCTION;

	assert(state != m_ds.state || state == eTS_Disabled);

	if (state != m_ds.state)
	{
		if (state == eTS_Running && m_triggerQueryStart)
		{
			m_pState->SendQueryComplete( m_triggerQueryStart, true );
			m_triggerQueryStart = 0;
		}

		if ( ((m_ds.state == eTS_Running && state != eTS_Completing) || (m_ds.state == eTS_Completing)) && m_triggerQueryEnd )
		{
			m_pState->SendQueryComplete(m_triggerQueryEnd, true);
			m_triggerQueryEnd = 0;
		}

		if (state == eTS_Running || state == eTS_Completing)
		{
			if (m_pState->IsUsingTriggeredTransition() == false)
				m_pState->ClearOverrides();

			CPersistantDebug * pPD = CCryAction::GetCryAction()->GetPersistantDebug();
			bool debug = CAnimationGraphCVars::Get().m_debugExactPos != 0;
			if (debug)
				pPD->Begin( string(m_pState->GetEntity()->GetName()) + "_recalculatetriggerpositions", true );			
			Quat actorRot = m_pState->GetEntity()->GetWorldRotation();

			Vec3 targetPosition = m_trigger.position;
			Vec3 targetDirection = ( m_trigger.direction).GetNormalizedSafe(ZERO);

			Vec3 bumpUp(0,0,m_pState->GetEntity()->GetWorldPos().z+4);

			SAnimationMovement movement;
			float seconds = 0.0f;
			ColorF dbgClr1, dbgClr2;
			if (state == eTS_Running)
			{
				seconds = m_triggerMovement.duration.GetSeconds();
				movement = m_triggerMovement;
				dbgClr1 = ColorF(1,0,1,1);
				dbgClr2 = ColorF(0,0,1,1);
			}
			else if (state == eTS_Completing)
			{
				// TODO: optimize (don't look up animation...)
				// probably need to provide a different interface function, or make this optimized, and move the slow version elsewhere
				const SAnimationDesc* pDesc = m_pState->GetGraph()->GetAnimationDesc( m_pState->GetEntity(), m_ds.targetStateID, m_pState );
				if (pDesc != NULL)
					movement = pDesc->movement;

				assert(movement.translation.IsValid());
				assert(movement.rotation.IsValid());

				targetPosition += Quat::CreateRotationV0V1( FORWARD_DIRECTION, targetDirection ) * movement.translation;
				targetDirection = movement.rotation * targetDirection;
				seconds = movement.duration.GetSeconds();
				dbgClr1 = ColorF(1,1,0,1);
				dbgClr2 = ColorF(1,0,0,1);
			}
			else assert(false);

			Vec3 realStartPosition = m_pState->GetEntity()->GetWorldPos();
 			Quat realStartOrientation = actorRot;

			if (debug)
			{
				pPD->Begin( string(m_pState->GetEntity()->GetName()) + "_finalpathline", true );
				Vec3 targetPosition2D(targetPosition.x, targetPosition.y, realStartPosition.z);
				Vec3 realStartDirection = realStartOrientation.GetColumn1();
				pPD->AddLine(realStartPosition, targetPosition2D, ColorF(1,1,0,1), 5.0f);
				pPD->AddDirection(realStartPosition, 0.5f, realStartDirection, ColorF(1,0,0,1), 5.0f);
				pPD->AddDirection(targetPosition2D, 0.5f, targetDirection, ColorF(1,1,1,1), 5.0f);

				pPD->AddDirection(realStartPosition + bumpUp, 1, realStartOrientation.GetColumn1(), dbgClr1, 5);
				pPD->AddSphere(realStartPosition + bumpUp, 0.1f, dbgClr1, 5);
			}

			Quat startOrientation = realStartOrientation;
			// start orientation may not be within bounds; we'll fake things to get it there
			// figure out our expected end orientation, and hence direction
			Quat realEndOrientation = startOrientation * movement.rotation;
			Quat endOrientation = realEndOrientation;
			Vec3 realEndDirection = realEndOrientation.GetColumn1();
			Vec3 endDirection = realEndDirection;
			// how much are we out from our limitations?
			float dot = realEndDirection.Dot(targetDirection);
			float angleToDesiredDirection = cry_acosf( CLAMP(dot,-1.f,1.f) );
			if (angleToDesiredDirection > m_trigger.directionRadius)
			{
				float fractionOfTurnNeeded = m_trigger.directionRadius / angleToDesiredDirection;
				Quat rotationFromDesiredToCurrent = Quat::CreateRotationV0V1( targetDirection, realEndDirection );
				Quat amountToRotate = Quat::CreateSlerp( Quat::CreateIdentity(), rotationFromDesiredToCurrent, fractionOfTurnNeeded );
				endDirection = amountToRotate * targetDirection;
				endOrientation = Quat::CreateRotationV0V1( FORWARD_DIRECTION, endDirection );
				startOrientation = endOrientation * movement.rotation.GetInverted();
			}
			// find out approx. where the animation ends
			Vec3 realEndPoint = realStartPosition + realStartOrientation * movement.translation;
			Vec3 endPoint = realStartPosition + startOrientation * movement.translation;
			if (debug)
			{
				pPD->AddDirection(realEndPoint + bumpUp, 1, realEndDirection, dbgClr1, 5);
				pPD->AddSphere(realEndPoint + bumpUp, 0.1f, dbgClr1, 5);
				pPD->AddSphere(endPoint + bumpUp, 0.1f, dbgClr2, 5);
			}
			// if the animation is outside of the target sphere, project it to the boundary
			Vec3 dirTargetToEndPoint = endPoint - targetPosition;
			float distanceTargetToEndPoint = dirTargetToEndPoint.GetLength();
			if (distanceTargetToEndPoint > m_trigger.positionRadius)
			{
				dirTargetToEndPoint /= distanceTargetToEndPoint;
				dirTargetToEndPoint *= m_trigger.positionRadius * 0.8f;
				endPoint = targetPosition + dirTargetToEndPoint;
			}
			// calculate error velocities (between where we expect we'll end up, and where we'll really end up)
			if (seconds > 0.0f)
			{
				m_ds.target.errorRotationalVelocity = Quat::CreateSlerp( Quat::CreateIdentity(), realEndOrientation.GetInverted() * endOrientation, 1.0f/seconds );
				m_ds.target.errorVelocity = (endPoint - realEndPoint)/ seconds;
				if (debug)
					pPD->AddDirection(endPoint + bumpUp, 1, m_ds.target.errorVelocity, dbgClr2, 5);
			}
			else
			{
				m_ds.target.errorRotationalVelocity = Quat::CreateIdentity();
				m_ds.target.errorVelocity = ZERO;
				endPoint = targetPosition;
				endOrientation = m_ds.target.orientation;
			}

			// Project end point on ground.
			if (m_triggerRequest.projectEnd == true)
			{
				ray_hit hit;
				int rayFlags = rwi_stop_at_pierceable|(geom_colltype_player<<rwi_colltype_bit);
				IPhysicalEntity* skipEnts[1];
				skipEnts[0] = m_pState->GetEntity()->GetPhysics();
				if (gEnv->pPhysicalWorld->RayWorldIntersection(endPoint + Vec3(0,0,1.0f), Vec3(0,0,-2.0f), 
					ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid, rayFlags, &hit, 1, skipEnts, 1))
				{
					endPoint = hit.pt;
				}
			}

			// set the position/orientation
			m_ds.target.position = endPoint;
			//m_ds.target.position = realStartPosition + Filter2D(m_ds.target.position - realStartPosition, actorRot);

			m_ds.target.orientation = endOrientation;
			m_ds.target.orientationRadius = m_trigger.directionRadius;
			m_ds.target.positionRadius = m_trigger.positionRadius;
			//m_ds.target.maxRadius = movement.movement.GetLength();
			m_ds.target.maxRadius = 0.0f;

			assert(m_ds.target.position.IsValid());
			assert(m_ds.target.orientation.IsValid());
			assert(NumberValid(m_ds.target.positionRadius));
			assert(NumberValid(m_ds.target.orientationRadius));


			// tell AI the ending position
			if (state == eTS_Completing)
			{
				m_pState->NotifyFinishPoint(m_ds.target.position, m_ds.target.orientation);
			}
		}

		m_ds.state = state;
		m_ds.token = 0;
		switch (m_ds.state)
		{
		case eTS_Disabled:
			GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Disable exact positioning");
			m_ds = SDynamicState();
			break;
		case eTS_Considering:
			GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Consider exact positioning");
			m_ds.gotInitialPosition = false;
			m_ds.pathfindRetryTimer = 0.0f;
			m_ds.target.activated = false;
			m_ds.target.preparing = false;
			m_ds.pathFindPerformed = false;
			break;
		case eTS_Waiting:
			GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Waiting for exact positioning");
			m_ds.target.activated = false;
			m_ds.target.preparing = false;
			m_ds.pathFindPerformed = false;
			break;
		case eTS_Preparing:
		case eTS_FinalPreparation:
			GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "%s for exact positioning", m_ds.state == eTS_Preparing? "Preparing" : "Final preparation");
			assert(m_ds.pathFindPerformed);
			m_ds.trigger.ResetRadius( Vec3(m_trigger.startRadius, m_trigger.startRadius, 20.0f), m_trigger.directionRadius );
			m_ds.target.positionRadius = m_trigger.startRadius;
			m_ds.target.maxRadius = m_trigger.prepareRadius;
			m_ds.target.activated = false;
			m_ds.target.preparing = true;
			m_ds.target.doingSomething = true;
			if (CAnimationGraphCVars::Get().m_debugExactPos != 0)
			{
				CCryAction::GetCryAction()->GetPersistantDebug()->Begin("ExactPositioning PreparationRadius", false);
				CCryAction::GetCryAction()->GetPersistantDebug()->AddPlanarDisc(m_ds.target.position, 0.0f, m_trigger.startRadius, ColorF(1,1,1,0.5), 10.0f);
			}
			break;
		case eTS_Running:
		case eTS_Completing:
			GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "%s exact positioning", m_ds.state == eTS_Running? "Running" : "Completing");
			m_ds.trigger = CAnimationTrigger();
			assert(m_ds.pathFindPerformed);
			m_ds.target.activated = true;
			m_ds.target.preparing = false;
			m_ds.target.doingSomething = true;
			break;
		default:
			assert(!"need to add trigger state to CExactPositioning::SetTriggerState");
		}
		if (!m_ds.pathFindPerformed)
		{
			m_ds.targetStateID = INVALID_STATE;
			m_ds.startStateID = INVALID_STATE;
		}
	}
}

void CExactPositioning::CompleteTriggering( bool success )
{
	bool debug = CAnimationGraphCVars::Get().m_debugExactPos != 0;

	GetManager()->Log(ColorF(1,0,1,1),m_pState->GetEntity(),"Exact positioning completed %s", success? "successfully" : "unsuccessfully");
	if (m_triggerQueryStart)
	{
		m_pState->SendQueryComplete(m_triggerQueryStart, success);
		m_triggerQueryStart = 0;
	}
	if (m_triggerQueryEnd)
	{
		m_pState->SendQueryComplete(m_triggerQueryEnd, success);
		m_triggerQueryEnd = 0;
	}

	if (success && (CAnimationGraphCVars::Get().m_SafeExactPositioning != 0))
	{
		// don't want to reposition dead guys
		IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(m_pState->GetEntity()->GetId());
		if (!pActor->IsPlayer() && pActor->GetHealth() <= 0)
		{
			return;
		}

		CPersistantDebug * pPD = CCryAction::GetCryAction()->GetPersistantDebug();

		if (debug)
			pPD->Begin(string(m_pState->GetEntity()->GetName()) + "_safeteleport", true);

//	bool correctVertically = (m_pState->GetAnimatedCharacter()->GetMCMV() == eMCM_Animation);

		// 2D project teleported target position to current altitude.
		Vec3 targetPosition2D = m_ds.target.position;

/*
		if (!correctVertically)
			targetPosition2D.z = m_pState->GetEntity()->GetWorldPos().z;
*/

		Vec3 linearDistanceVector = m_ds.target.position - m_pState->GetEntity()->GetWorldPos();

/*
		if (!correctVertically)
			linearDistanceVector.z = 0.0f;
*/

		CAnimatedCharacter* pAnimChar = m_pState->GetAnimatedCharacter();
		EMovementControlMethod mcmh = pAnimChar->GetMCMH();
		EMovementControlMethod mcmv = pAnimChar->GetMCMV();
		pAnimChar->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
		{
			float linearDistance = linearDistanceVector.len();
			if (linearDistance > m_ds.target.positionRadius)
			{
				Matrix34 xform = m_pState->GetEntity()->GetWorldTM();
				xform.SetTranslation(targetPosition2D);
				m_pState->GetEntity()->SetWorldTM(xform);

				if (debug)
					pPD->AddSphere(targetPosition2D, 0.2f, ColorF(1,0,1,1), 5.0f);
			}

			Vec3 dirWanted = m_ds.target.orientation.GetColumn1();
			Vec3 dirCurrent = m_pState->GetEntity()->GetWorldRotation().GetColumn1();

			f32 dot	=	clamp_tpl(dirWanted.Dot(dirCurrent),-1.0f,+1.0f);

			float angularDistance = cry_acosf(dot);
			if (angularDistance > m_ds.target.orientationRadius)
			{
				Matrix34 xform = m_pState->GetEntity()->GetWorldTM();
				xform.SetRotation33(Matrix33(m_ds.target.orientation));
				m_pState->GetEntity()->SetWorldTM(xform);

				Vec3 targetDirection = m_ds.target.orientation.GetColumn1();
				if (debug)
					pPD->AddDirection(targetPosition2D, 0.5f, targetDirection, ColorF(1,0.5f,1,1), 5.0f);
			}
		}
		pAnimChar->SetMovementControlMethods(mcmh, mcmv);
	}

	SetTriggerState( eTS_Disabled );
}

ETriState CExactPositioning::CheckTargetMovement( const SAnimationMovement& movement, float radius, CTargetPointRequest& targetPointRequest )
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (!m_pState->GetPointVerifier())
		return eTS_true;

	CPersistantDebug * pPD = CCryAction::GetCryAction()->GetPersistantDebug();

	bool debug = CAnimationGraphCVars::Get().m_debugExactPos != 0;
	if (debug)
		pPD->Begin( string(m_pState->GetEntity()->GetName()) + "_checkmovement", true );

	SAnimationTarget tgt = CalculateTarget(movement);
	targetPointRequest = CTargetPointRequest(tgt.position);
	if (debug)
		pPD->AddSphere( targetPointRequest.GetPosition(), 0.1f, ColorF(1,0,1,1), 3.0f );
	GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Try target point %.2f %.2f %.2f", targetPointRequest.GetPosition().x, targetPointRequest.GetPosition().y, targetPointRequest.GetPosition().z);
  ETriState triState = m_pState->GetPointVerifier()->CanTargetPointBeReached(targetPointRequest);
	if (triState != eTS_true)
	{
		if (m_pState->GetEntity())
		{
			Vec3 dist = targetPointRequest.GetPosition() - m_trigger.position;
			dist.z = 0.0f;
			if (dist.IsZero())
				return triState;

			targetPointRequest = CTargetPointRequest(targetPointRequest.GetPosition() + dist.GetNormalizedSafe() * 0.5f);
		}
		if (debug)
			pPD->AddSphere(targetPointRequest.GetPosition(), 0.1f, ColorF(1,0,1,1), 3.0f );
		GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Try target point %.2f %.2f %.2f", targetPointRequest.GetPosition().x, targetPointRequest.GetPosition().y, targetPointRequest.GetPosition().z);
		triState = m_pState->GetPointVerifier()->CanTargetPointBeReached(targetPointRequest);
	}
	if (triState == eTS_true)
	{
		tgt.position = targetPointRequest.GetPosition();
//		m_ds.target = tgt;
		GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Update animation trigger to %.2f %.2f %.2f", targetPointRequest.GetPosition().x, targetPointRequest.GetPosition().y, targetPointRequest.GetPosition().z);
		m_ds.trigger = CAnimationTrigger( tgt.position, Vec3(tgt.positionRadius, tgt.positionRadius, 20.0f), tgt.orientation, m_trigger.directionRadius, 0.1f, movement.translation.GetLength() );
	}
	return triState;
}

SAnimationTarget CExactPositioning::CalculateTarget( const SAnimationMovement& movement ) const
{
	ANIM_PROFILE_FUNCTION;

	SAnimationTarget tgt = m_ds.target;

	Quat actorRot = m_pState->GetEntity()->GetWorldRotation();
	Vec3 worldPos = m_pState->GetEntity()->GetWorldPos();

	if (m_ds.state == eTS_Considering)
	{
		tgt.position = m_trigger.position;
		tgt.orientation = movement.rotation.GetInverted() * Quat::CreateRotationV0V1(FORWARD_DIRECTION, m_trigger.direction);
		tgt.positionRadius = m_trigger.prepareRadius;
	}
	else if (m_ds.state >= eTS_Waiting && m_ds.state < eTS_FinalPreparation)
	{
		bool debug = CAnimationGraphCVars::Get().m_debugExactPos != 0;

		tgt.orientation = movement.rotation.GetInverted() * Quat::CreateRotationV0V1(FORWARD_DIRECTION, m_trigger.direction);
		tgt.position = m_trigger.position - tgt.orientation * movement.translation;

		if (m_ds.state < eTS_Preparing)
			tgt.positionRadius = m_trigger.prepareRadius;
		else
			tgt.positionRadius = m_trigger.startRadius;

		if (debug)
		{
			CPersistantDebug * pPD = CCryAction::GetCryAction()->GetPersistantDebug();
			pPD->Begin( string(m_pState->GetEntity()->GetName()) + "_recalculatetriggerpositions", true );
			pPD->AddDirection( m_trigger.position, 1, m_trigger.direction, ColorF(1,0.8f,0.8f,1), 1 );
			pPD->AddDirection( m_trigger.position, 1.5f, tgt.orientation.GetColumn1(), ColorF(0,1,1,1), 1 );
			pPD->AddSphere( m_trigger.position, 0.1f, ColorF(0,1,0,1), 1 );
			pPD->AddSphere( tgt.position, 0.1f, ColorF(0,1,0,1), 1 );
		}
	}

	return tgt;
}

void CExactPositioning::Update()
{

	if (m_ds.state == eTS_Preparing && m_ds.trigger.IsTriggered())
		SetTriggerState(eTS_FinalPreparation);

	if (m_ds.state < eTS_FinalPreparation && m_trigger.position.GetLengthSquared() < 0.001f)
		SetTriggerState(eTS_Disabled);

	if (m_triggerCommit)
		CommitTriggering();

	if (m_ds.state >= eTS_Waiting && m_triggerRecalculate)
		RecalculateTriggering();

	if ((m_ds.state == eTS_Running) && (m_ds.target.activationTimeRemaining < 0.0f))
	{
		m_ds.target.activationTimeRemaining = 0.0f;
	}

	StateID currentStateID = m_pState->GetCurrentState();
	if ((m_ds.state == eTS_Running) && (m_ds.targetStateID == currentStateID))
	{
		GetManager()->Log(ColorF(1,0,1,1),m_pState->GetEntity(),"Commit exact positioning input values");
		for (int i=0; i<CAnimationGraph::MAX_INPUTS; i++)
		{
			if (m_triggerInputs[i] != CStateIndex::INPUT_VALUE_DONT_CARE)
				m_pState->SetInputValue(i, m_triggerInputs[i], m_triggerInputsAsFloats[i]);
		}
		m_ds.target.activationTimeRemaining = 0.f;
		if (!m_pState->GetGraph()->IsStateLooped(m_pState->GetCurrentState()))
		{
			const SAnimationDesc* animDesc = m_pState->GetGraph()->GetAnimationDesc( m_pState->GetEntity(), m_pState->GetCurrentState(), m_pState );
			if (animDesc != NULL)
				animDesc->movement.duration.GetSeconds();

			// sum remaining time over all animations in queue as well, not just the current one.
			if (ICharacterInstance * pChar = m_pState->GetEntity()->GetCharacter(0))
			{
				ISkeletonAnim * pSkel = pChar->GetISkeletonAnim();
				int nAnims = pSkel->GetNumAnimsInFIFO(0);
				for (int i=0; i<nAnims; i++)
				{
					CAnimation& anim = pSkel->GetAnimFromFIFO(0, i);
					float animTimeRemaining = (1.0f - anim.m_fAnimTime) * pChar->GetIAnimationSet()->GetDuration_sec(anim.m_LMG0.m_nAnimID[0]);
					//				if (i > 0 && anim.m_AnimParams.m_fTransTime > 0.0f)
					//					animTimeRemaining -= anim.m_AnimParams.m_fTransTime;
					if (animTimeRemaining > 0.0f)
					{
						if (anim.m_bActivated)
							m_ds.target.activationTimeRemaining = animTimeRemaining;
						else
							m_ds.target.activationTimeRemaining += animTimeRemaining;
					}
				}
			}
		}
		if (m_triggerMovement.duration.GetSeconds() <= 0.1f)
			m_ds.target.activationTimeRemaining = std::max( m_ds.target.activationTimeRemaining, 0.1f );
		SetTriggerState( eTS_Completing );
		m_ds.token = m_pState->GetCurrentToken();
	}

	if (m_ds.state == eTS_Completing && (m_ds.target.activationTimeRemaining == -1.0f))
	{
		// NOTE: activationTimeRemaining will be reset to zero eventually within this scope, 
		// when the dynamic state is set to a default constructed object, so we don't have to do it explicitly.

/*
		// only allow complete 'completing' if we have played all 'running' animations.
		bool tokenAllowsCompleting = true;
		if (ICharacterInstance * pChar = m_pState->GetEntity()->GetCharacter(0))
		{
			ISkeleton * pSkel = pChar->GetISkeleton();
			int nAnims = pSkel->GetNumAnimsInFIFO(0);
			for (int i=0; i<nAnims; i++)
			{
				CAnimation& anim = pSkel->GetAnimFromFIFO(0, i);
				if (m_ds.token <= anim.m_AnimParams.m_nUserToken)
					tokenAllowsCompleting = false;
			}
		}
		if (tokenAllowsCompleting)
*/
			CompleteTriggering(true);
	}

	// check for a big problem somewhere and somewhat recover
	if (m_ds.state == eTS_Running && !(m_ds.targetStateID == m_pState->GetCurrentState() || m_ds.targetStateID == m_pState->GetQueriedState()) && m_ds.pathFindPerformed && m_ds.transition.Empty())
	{
		GameWarning("%s: Exact positioning running, we have an unsatisfied target, have performed pathfinding, and have no transition; failing", m_pState->GetEntity()? m_pState->GetEntity()->GetName() : "<<unknown entity>>");
		CompleteTriggering(false);
	}

	if (!m_pState->GetEntity())
		return;

	m_ds.trigger.SetMinMaxSpeed( m_pState->GetQueriedStateMinMaxSpeed() );

	if (m_ds.state <= eTS_Preparing)
	{
		bool rePathFind = false;
		if (m_ds.checkTime > CHECK_TARGET_POSITION_INTERVAL)
		{
			SAnimationTarget tgt = CalculateTarget(m_triggerMovement);
			if (tgt.position.GetDistance(m_ds.target.position) > TARGET_POSITION_DRIFT_CAUSING_REPATHFIND || !m_ds.gotInitialPosition)
				rePathFind = true;
			if (!m_ds.gotInitialPosition)
			{
				m_ds.target = tgt;
				m_ds.gotInitialPosition = true;
			}
		}
		if (rePathFind)
		{
			MaybeRecalculateTriggering();
		}
	}

	CheckTriggers();
}

void CExactPositioning::CheckTriggers()
{
	Quat actorRot = m_pState->GetEntity()->GetWorldRotation();
	Vec3 worldPos = m_pState->GetEntity()->GetWorldPos();
	Vec3 worldOrientation = actorRot.GetColumn1();

	Vec3 targetPos = m_ds.target.position;
	float startRadiusSq = worldPos.GetSquaredDistance(targetPos);

	if (m_ds.state == eTS_Considering)
	{
		static const float END_CONSIDERING_DISTANCE = 15.0f;
		if (startRadiusSq <= square(END_CONSIDERING_DISTANCE) && m_ds.pathfindRetryTimer <= 0.0f)
		{
			GetManager()->AddLogMarker();
			GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Within %f meters of target", END_CONSIDERING_DISTANCE);
			SDynamicState backup = m_ds;
			SetTriggerState(eTS_Waiting);
			RecalculateTriggering();
			if (!m_ds.pathFindPerformed || m_ds.state < eTS_Waiting)
			{
				m_ds = backup;
				m_ds.pathfindRetryTimer = RETRY_PATHFIND_INTERVAL_CONSIDERING;
				GetManager()->DoneLogMarker(false);
				return;
			}

			targetPos = m_ds.target.position;
			startRadiusSq = worldPos.GetSquaredDistance(targetPos);
			GetManager()->DoneLogMarker(true);
		}
	}

	ETriggerState oldState = m_ds.state;
	if (m_ds.target.allowActivation || m_ds.target.activated || m_ds.target.preparing)
	{
		GetManager()->AddLogMarker();
		if (m_ds.state == eTS_Preparing || (m_ds.state == eTS_Waiting && m_ds.pathFindPerformed))
		{
			if (m_ds.trigger.IsReached())
				SetTriggerState( eTS_FinalPreparation );
		}
		if (m_ds.state == eTS_FinalPreparation && m_ds.trigger.IsTriggered())
			SetTriggerState( eTS_Running );
		if (m_ds.state == eTS_Waiting && m_ds.pathFindPerformed)
		{
			if (startRadiusSq <= m_trigger.prepareRadius*m_trigger.prepareRadius)
				SetTriggerState( eTS_Preparing );
		}
		GetManager()->DoneLogMarker( oldState != m_ds.state );
	}
}

void CExactPositioning::SetInput( InputID id, const char * pValue )
{
	if (id >= CAnimationGraph::MAX_INPUTS || id >= m_pState->GetGraph()->m_inputValues.size())
		return;
	m_triggerRequestInputs[id] = m_pState->GetGraph()->m_inputValues[id]->EncodeInput( pValue );
	m_triggerRequestInputsAsFloats[id] = 0.0f;
	m_triggerCommit = true;
}

void CExactPositioning::SetInput( InputID id, float value )
{
	CHECKQNAN_FLT(value);

	if (id >= CAnimationGraph::MAX_INPUTS || id >= m_pState->GetGraph()->m_inputValues.size())
		return;
	m_triggerRequestInputs[id] = m_pState->GetGraph()->m_inputValues[id]->EncodeInput( value );
	m_triggerRequestInputsAsFloats[id] = value;
	m_triggerCommit = true;
}

void CExactPositioning::SetInput( InputID id, int value )
{
	if (id >= CAnimationGraph::MAX_INPUTS || id >= m_pState->GetGraph()->m_inputValues.size())
		return;
	m_triggerRequestInputs[id] = m_pState->GetGraph()->m_inputValues[id]->EncodeInput( value );
	m_triggerRequestInputsAsFloats[id] = value;
	m_triggerCommit = true;
}

void CExactPositioning::CommitTriggering()
{
	if (m_ds.state < eTS_Running)
	{
		bool changed = false;
		GetManager()->AddLogMarker();
		if (m_triggerRequest != m_trigger)
		{
			GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Trigger point changed");
			m_trigger = m_triggerRequest;
			changed = true;
		}
		if (0 != memcmp(m_triggerRequestInputs, m_triggerInputs, sizeof(m_triggerInputs)))
		{
			GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Inputs changed");
			memcpy(m_triggerInputs, m_triggerRequestInputs, sizeof(m_triggerInputs));
			changed = true;
		}
		memcpy(m_triggerInputsAsFloats, m_triggerRequestInputsAsFloats, sizeof(m_triggerInputsAsFloats));
		if (changed || m_ds.state == eTS_Disabled)
		{
			GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Start exact positioning");
			if ( m_ds.state == eTS_Considering )
				SetTriggerState( eTS_Disabled );
			SetTriggerState( eTS_Considering );
			m_triggerRecalculate = true;
		}
		if (m_pTriggerQueryStartRequest)
		{
			if (m_triggerQueryStart && m_triggerQueryStart != *m_pTriggerQueryStartRequest)
			{
				GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Fail old start query");
				m_pState->SendQueryComplete(m_triggerQueryStart, false);
				m_triggerQueryStart = *m_pTriggerQueryStartRequest = m_pState->AllocQuery();
			}
			else if (m_triggerQueryStart && m_triggerQueryStart == *m_pTriggerQueryStartRequest)
				;
			else
				m_triggerQueryStart = *m_pTriggerQueryStartRequest = m_pState->AllocQuery();
			m_pTriggerQueryStartRequest = 0;
			changed = true;
		}
		if (m_pTriggerQueryEndRequest)
		{
			if (m_triggerQueryEnd && m_triggerQueryEnd != *m_pTriggerQueryEndRequest)
			{
				GetManager()->Log(ColorF(1,0,1,1), m_pState->GetEntity(), "Fail old finish query");
				m_pState->SendQueryComplete(m_triggerQueryEnd, false);
				m_triggerQueryEnd = *m_pTriggerQueryEndRequest = m_pState->AllocQuery();
			}
			else if (m_triggerQueryEnd && m_triggerQueryEnd == *m_pTriggerQueryEndRequest)
				;
			else
				m_triggerQueryEnd = *m_pTriggerQueryEndRequest = m_pState->AllocQuery();
			m_pTriggerQueryEndRequest = 0;
			changed = true;
		}
		m_triggerCommit = false;
		GetManager()->DoneLogMarker(changed);
	}
}

void CExactPositioning::MaybeRecalculateTriggering()
{
	GetManager()->AddLogMarker();
	SDynamicState backup = m_ds;
	m_ds.pathFindPerformed = false;
	m_ds.startStateID = INVALID_STATE;
	m_ds.targetStateID = INVALID_STATE;
	m_triggerRecalculate = true;
	
	bool aborted = RecalculateTriggering();

	bool commit = true;
	if (m_ds.state != backup.state && !aborted)
	{
		commit = false;
		m_ds = backup;
	}
	GetManager()->DoneLogMarker(commit);
}

bool CExactPositioning::RecalculateTriggering()
{
	ANIM_PROFILE_FUNCTION;

	if ((!m_triggerRecalculate && m_ds.pathFindPerformed) || m_ds.state < eTS_Waiting)
		return false;

	m_triggerRecalculate = false;
	m_ds.checkTime = 0;

	// calculate desired state
	CStateIndex::InputValue inputs[CAnimationGraph::MAX_INPUTS];
	for (int i=0; i<CAnimationGraph::MAX_INPUTS; i++)
	{
		inputs[i] = m_pState->GetInputValues()[i];
		if (m_triggerInputs[i] != CStateIndex::INPUT_VALUE_DONT_CARE)
			inputs[i] = m_triggerInputs[i]; 
	}

	CStateIndex::StateIDVec validStates;
	m_pState->BasicQuery( inputs, validStates );

	StateID triggerState = INVALID_STATE;
	if (!validStates.empty())
	{
		triggerState = validStates[ m_pState->GetRandomNumber() % validStates.size() ];

		// allow hystereses - if the current state matches exactly as well as the newly queried state, keep the current state
		if (m_pState->GetCurrentState() != INVALID_STATE)
		{
			for (CStateIndex::StateIDVec::iterator iter = validStates.begin(); iter != validStates.end(); ++iter)
			{
				if (*iter == m_pState->GetCurrentState())
				{
					triggerState = *iter;
					break;
				}
			}
		}
	}

	//marcok: likely to get sometimes stuck in here if AG is fubar
	if (triggerState == INVALID_STATE)
	{
		if (m_ds.state <= eTS_Preparing)
			SetTriggerState(eTS_Considering);
//			CompleteTriggering(false);
		return false;
	}

	// have our start/finish states changed?
	if (m_ds.state <= eTS_Preparing)
	{
		if (m_pState->GetCurrentState() != m_ds.startStateID || triggerState != m_ds.targetStateID)
		{
			m_ds.startStateID = m_pState->GetCurrentState();
			m_ds.targetStateID = triggerState;

			//marcok: likely to get sometimes stuck in here if AG is fubar
			if (m_ds.startStateID == INVALID_STATE || m_ds.targetStateID == INVALID_STATE)
			{
				m_trigger = SAnimationTargetRequest();
//				CompleteTriggering(false);
				if (m_ds.state <= eTS_Preparing)
					SetTriggerState(eTS_Considering);
			}
			else
			{
				if (m_ds.startStateID != m_ds.targetStateID)
				{
					if (CAnimationGraphCVars::Get().m_log)
						GetManager()->Log(ColorB(1,0,0,1), m_pState->GetEntity(), "Exact positioning: pathfind %s -> %s", m_pState->GetGraph()->StateIDToName(m_ds.startStateID).c_str(), m_pState->GetGraph()->StateIDToName(m_ds.targetStateID).c_str());
					m_ds.transition.Clear();

					CAnimationGraph::SPathFindParams params;
					params.destinationStateID = m_ds.targetStateID;
					params.pTransitions = &m_ds.transition;
					params.pMovement = &m_triggerMovement;
					params.pCurInputValues = inputs;
					params.radius = 5.0;
					params.time = 5.0;
					params.pEntity = m_pState->GetEntity();
					params.pGraphState = m_pState;
					params.pStats = NULL;
					params.randomNumber = m_pState->GetRandomNumber();
					CTargetPointRequest targetPointRequest;
					params.pTargetPointRequest = &targetPointRequest;

					ETriState success = m_pState->BasicPathfind( m_ds.startStateID, params);

					if (success == eTS_true)
					{
						SAnimationTarget tgt = CalculateTarget( m_triggerMovement );
						targetPointRequest = CTargetPointRequest( tgt.position );
						if ( m_pState->GetPointVerifier()->CanTargetPointBeReached( targetPointRequest ) == eTS_false )
						{
							if ( m_pState->GetEntity() )
							{
								Vec3 dist = targetPointRequest.GetPosition() - m_trigger.position;
								if ( !dist.IsZero() )
								{
									Vec3 offset = dist.GetNormalizedSafe() * 0.5f;
									m_triggerMovement.translation += offset;
									tgt = CalculateTarget( m_triggerMovement );
									targetPointRequest = CTargetPointRequest( tgt.position );
								}
							}
						}
						m_ds.target = tgt;
						m_ds.trigger = CAnimationTrigger( m_ds.target.position, Vec3(m_ds.target.positionRadius, m_ds.target.positionRadius, 20.0f), m_ds.target.orientation, m_trigger.directionRadius, 0.1f, m_triggerMovement.translation.GetLength() );
						if (m_triggerRequest.navSO)
						{
							m_ds.trigger.SetDistanceErrorFactor(0.5f);
							m_ds.target.isNavigationalSO = true;
						}

						m_pState->GetPointVerifier()->UseTargetPointRequest( targetPointRequest );
					}
					else
					{
						if (params.pStats->aiWalkabilityQueriesPerformed>0)
						{
							// we failed because walkability failed
							m_pState->GetPointVerifier()->NotifyAllPointsNotReachable();
						}
						if (success == eTS_false)
						{
							CompleteTriggering(false);
							return true;
						}
						else
						{
							if (m_ds.state <= eTS_Preparing)
								SetTriggerState(eTS_Considering);
						}
					}
				}
				else
				{
					m_triggerMovement.translation = Vec3(ZERO);
					m_triggerMovement.rotation = Quat::CreateIdentity();
					m_triggerMovement.duration = 0.0f;
					m_ds.trigger = CAnimationTrigger( m_trigger.position, Vec3(m_trigger.positionRadius, m_trigger.positionRadius, 20.0f), Quat::CreateRotationV0V1(FORWARD_DIRECTION, m_trigger.direction), m_trigger.directionRadius, 0.1f, 0 );
				}
				m_ds.target.activationTimeRemaining = max( m_triggerMovement.duration.GetSeconds(), 0.1f );
				m_ds.pathFindPerformed = true;
			}
		}
		else
		{
			m_ds.pathFindPerformed = true;
		}
	}

	return false;
}

const SAnimationTarget * CExactPositioning::GetAnimationTarget()
{
	//	if (m_ds.state == eTS_Disabled)
	//		return NULL;
	return &m_ds.target;
}

bool CExactPositioning::IsUpdateReallyNecessary()
{
	return m_triggerCommit || (m_ds.state != eTS_Disabled);
}

void CExactPositioning::CheckEphemeralToken()
{
	if (m_ds.state == eTS_Running && m_ds.transition.Size() == 1 && !m_ds.token && m_ds.targetStateID == m_pState->GetCurrentState())
		m_ds.token = m_pState->GetCurrentToken();
}

bool CExactPositioning::AllowRollbacks()
{
	return m_ds.state <= eTS_Preparing || m_ds.state == eTS_Completing;
}

bool CExactPositioning::IsMovingAnimation( ICharacterInstance * pChar )
{
	// heuristics to detect if we can reasonably control a character in its current state
	ISkeletonAnim* pSkel = pChar->GetISkeletonAnim();

	int numAnims = pSkel->GetNumAnimsInFIFO(0);

	if (!numAnims) // no animation => can't control
		return false;

	if (pSkel->GetCurrentVelocity().GetLength() < 0.2f) // too slow?
		return false;

	for (int i=0; i<numAnims; i++)
	{
		CAnimation& anim = pSkel->GetAnimFromFIFO(0,i);
		if (anim.m_AnimParams.m_nUserToken != m_pState->GetCurrentToken())
			continue;
		if (anim.m_LMG0.m_nLMGID < 0 || anim.m_LMG1.m_nLMGID < 0) // this is no LMG set
			return false;
		if (anim.m_LMG0.m_numAnims < 4) // not enough animations to really control things well
			return false;
		return true;
	}

	return false;
}

bool CExactPositioning::OverrideQuery( QueryResult& qr, bool isSteady )
{
	if (isSteady && (m_ds.state == eTS_Preparing || m_ds.state == eTS_FinalPreparation) && m_pState->GetCurrentState() != INVALID_STATE)
	{
		// EARLY OUT!!
		bool ok = false;
		if (m_pState->GetEntity())
			if (ICharacterInstance * pChar = m_pState->GetEntity()->GetCharacter(0))
				if (IsMovingAnimation(pChar))
					ok = true;
		if (ok)
		{
			qr.foundResult = true;
			qr.countsAsQueriedState = false;
			qr.state = m_pState->GetCurrentState();
			return true;
		}
	}
	if (!qr.foundResult && m_ds.state == eTS_Running)
	{
		qr.foundResult = true;
		qr.queriedState = qr.state = m_ds.targetStateID;
		qr.countsAsQueriedState = true;
		qr.useTriggeredTransition = ( !m_ds.transition.Empty() && qr.state == m_ds.transition.Back() );
		return true;
	}
	return false;
}

void CExactPositioning::DebugDraw_Status( IRenderer * pRend, int& x, int& y, int YSPACE )
{
	float white[4] = {1,1,1,1};
	float red[4] = {1,0,0,1};
	float yellow[4] = {1,1,0,1};
	float green[4] = {0,1,0,1};

	static const size_t BUFSZ = 512;
	char buf[BUFSZ];

	if (m_ds.state != eTS_Disabled)
	{
		y += 2*YSPACE;

		const char * stateName = "<<erm, better fix me>>";
		float radius = 0.0f;
		Vec3 point = m_ds.target.position;

		switch (m_ds.state)
		{
		case eTS_Considering:
			stateName = "Considering";
			radius = 10.0f;
			break;
		case eTS_Waiting:
			stateName = "Waiting for";
			radius = m_trigger.prepareRadius;
			break;
		case eTS_FinalPreparation:
			stateName = "About to start";
			radius = m_trigger.startRadius;
			break;
		case eTS_Preparing:
			stateName = "Preparing for";
			radius = m_trigger.startRadius;
			break;
		case eTS_Running:
			stateName = "Running";
			radius = m_trigger.positionRadius;
			break;
		case eTS_Completing:
			stateName = "Completing";
			radius = m_trigger.positionRadius;
			break;
		}
		pRend->Draw2dLabel(x, y, 1, white, false, "%s transition to %s", stateName, m_ds.targetStateID!=INVALID_STATE? m_pState->GetGraph()->m_states[m_ds.targetStateID].id.c_str() : "<<invalid state>>");
		y += YSPACE;
		if (m_ds.state <= eTS_Preparing)
		{
			pRend->Draw2dLabel(x, y, 1, white, false, "Distance to target: %f", m_ds.target.position.GetDistance(m_pState->GetEntity()->GetWorldPos()));
			y += YSPACE;
			pRend->Draw2dLabel(x, y, 1, white, false, "Distance to end: %f", m_trigger.position.GetDistance(m_pState->GetEntity()->GetWorldPos()));
			y += YSPACE;

			Vec3 worldOrientation = m_pState->GetEntity()->GetWorldRotation().GetColumn1();
			float directionErrorRadians = cry_acosf( worldOrientation.Dot(m_ds.target.orientation.GetColumn1()) );
			bool inDirectionRange = directionErrorRadians < (m_trigger.directionRadius + EXTRA_DIRECTION_SLOP_IN_PREPARATION);
			float directionErrorDegrees = RAD2DEG(directionErrorRadians);

			float * clr = white;
			if (!inDirectionRange)
				clr = red;
			pRend->Draw2dLabel(x, y, 1, clr, false, "Direction error: %f\0260", directionErrorDegrees);
			y += YSPACE;
		}
		pRend->Draw2dLabel(x, y, 1, white, false, "Trigger:");
		y += YSPACE;
		pRend->Draw2dLabel(x+10, y, 1, white, false, "pos: %.2f %.2f %.2f + %.2f", m_trigger.position.x, m_trigger.position.y, m_trigger.position.z, m_trigger.positionRadius);
		y += YSPACE;
		pRend->Draw2dLabel(x+10, y, 1, white, false, "dir: %.2f %.2f %.2f + %.2f", m_trigger.direction.x, m_trigger.direction.y, m_trigger.direction.z, m_trigger.directionRadius);
		y += YSPACE;
		pRend->Draw2dLabel(x, y, 1, white, false, "Movement:");
		y += YSPACE;
		pRend->Draw2dLabel(x+10, y, 1, white, false, "dist: %.2f %.2f %.2f", m_triggerMovement.translation.x, m_triggerMovement.translation.y, m_triggerMovement.translation.z);
		y += YSPACE;
		pRend->Draw2dLabel(x+10, y, 1, white, false, "orient: %.2f %.2f %.2f %.2f", m_triggerMovement.rotation.v.x, m_triggerMovement.rotation.v.y, m_triggerMovement.rotation.v.z, m_triggerMovement.rotation.w);
		y += YSPACE;
		pRend->Draw2dLabel(x+10, y, 1, white, false, "time: %.2f", m_triggerMovement.duration.GetSeconds());
		y += YSPACE;
		pRend->Draw2dLabel(x, y, 1, m_ds.target.allowActivation? green : yellow, false, "Target: [%d%d]", m_ds.target.activated, m_ds.target.preparing);
		y += YSPACE;
		pRend->Draw2dLabel(x+10, y, 1, white, false, "pos: %.2f %.2f %.2f", m_ds.target.position.x, m_ds.target.position.y, m_ds.target.position.z);
		y += YSPACE;
		pRend->Draw2dLabel(x+10, y, 1, white, false, "orient: %.2f %.2f %.2f %.2f", m_ds.target.orientation.v.x, m_ds.target.orientation.v.y, m_ds.target.orientation.v.z, m_ds.target.orientation.w);
		y += YSPACE;
		pRend->Draw2dLabel(x+10, y, 1, white, false, "time: %.2f", m_ds.target.activationTimeRemaining);
		y += YSPACE;
		pRend->Draw2dLabel(x, y, 1, white, false, "Inputs:");
		y += YSPACE;

		for (int i=0; i<CAnimationGraph::MAX_INPUTS; i++)
		{
			if (m_triggerInputs[i] != CStateIndex::INPUT_VALUE_DONT_CARE)
			{
				m_pState->GetGraph()->m_inputValues[i]->DebugText( buf, m_triggerInputs[i], NULL );
				int correctness = 0;
				if (m_pState->GetCurrentState() != INVALID_STATE)
					correctness |= (int) m_pState->GetGraph()->m_stateIndex.StateMatchesInput( m_pState->GetCurrentState(), i, m_triggerInputs[i], eSMIF_ConsiderMatchesAny );
				if (m_pState->GetQueriedState() != INVALID_STATE)
					correctness |= 2 * m_pState->GetGraph()->m_stateIndex.StateMatchesInput( m_pState->GetQueriedState(), i, m_triggerInputs[i], eSMIF_ConsiderMatchesAny );

				float * clr;
				switch (correctness)
				{
				default:
					clr = red;
					break;
				case 1:
					clr = yellow;
					break;
				case 2:
					clr = green;
					break;
				case 3:
					clr = white;
					break;
				}

				pRend->Draw2dLabel( x+10, y, 1, clr, false, "  %.20s: [%d] %s", m_pState->GetGraph()->m_inputValues[i]->name.c_str(), m_triggerInputs[i], buf );
				y += YSPACE;
			}
		}

		IRenderAuxGeom * pAux = pRend->GetIRenderAuxGeom();
		pAux->SetRenderFlags( e_Mode3D | e_AlphaBlended | e_DrawInFrontOff | e_FillModeSolid | e_CullModeBack | e_DepthWriteOn | e_DepthTestOn );
		if (radius)
			pAux->DrawSphere( point, radius, ColorF(0,1,1,0.1f) );
		pAux->DrawSphere( m_trigger.position, 0.2f, ColorF(1,0,0,1) );

		if (m_ds.target.errorVelocity.GetLength() > 0.001f)
		{
			Vec3 ptLineMid = point + Vec3(0,0,2);
			Vec3 ptLineStart = ptLineMid - m_ds.target.errorVelocity * 5.0f;
			Vec3 ptLineEnd = ptLineMid + m_ds.target.errorVelocity * 5.0f;

			pAux->DrawLine( ptLineStart, ColorB(241,193,92,255), ptLineEnd, ColorB(241,193,92,255) );
			pAux->DrawCone( ptLineEnd, m_ds.target.errorVelocity.GetNormalized(), 0.4f, 0.4f, ColorB(241,193,92,255) );
		}
		if (true)
		{
			Vec3 dir = m_ds.target.orientation.GetColumn1();

			Vec3 ptLineMid = point + Vec3(0,0,2);
			Vec3 ptLineStart = ptLineMid - dir * 5.0f;
			Vec3 ptLineEnd = ptLineMid + dir * 5.0f;

			pAux->DrawLine( ptLineStart, ColorB(156,14,241,255), ptLineEnd, ColorB(156,14,241,255) );
			pAux->DrawCone( ptLineEnd, dir, 0.4f, 0.4f, ColorB(156,14,241,255) );
		}
	}
	else
	{
		pRend->Draw2dLabel( x, y, 1, white, false, "Exact positioning disabled for %f seconds", m_ds.disabledTime );
		y += YSPACE;
	}

	m_ds.trigger.DebugDraw();
}

void CExactPositioning::DebugDraw_Transition( IRenderer * pRend, int& x, int& y, int YSPACE )
{
	float white[4] = {1,1,1,1};
	float red[4] = {1,0,0,1};
	float yellow[4] = {1,1,0,1};
	float green[4] = {0,1,0,1};

	pRend->Draw2dLabel( x, y, 1, white, false, "Trigger transition (%d)", m_ds.transition.Size() );
	y += YSPACE;
	for (StateList::SIterator iter = m_ds.transition.Begin(); iter != m_ds.transition.End(); ++iter)
	{
		const char * id = "<invalid>";
		if (*iter != INVALID_STATE)
			id = m_pState->GetGraph()->m_states[*iter].id.c_str();
		pRend->Draw2dLabel( x, y, 1, white, false, id );
		y += YSPACE;
	}
}

bool CExactPositioning::CanDestroy() const
{
	return m_ds.state == eTS_Disabled && m_ds.disabledTime > TIME_DISABLED_BEFORE_IRRELEVANT;
}
