//--------------------------------------------------------------------------------

#include "StdAfx.h"
#include "AnimatedCharacter.h"
#include "AnimationGraphCVars.h"
#include "PersistantDebug.h"

//--------------------------------------------------------------------------------

void ProcessDebugPredictedEntLocation(QuatT& predictedEntLocation, const QuatT& animLocation)
{
#ifdef _DEBUG
	if (CAnimationGraphCVars::Get().m_debugTweakTrajectoryFit == 0)
		return;

	IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName("prediction");
	if (pEntity != NULL)
	{
		predictedEntLocation.t = pEntity->GetWorldPos();
		predictedEntLocation.q = Quat(Ang3(0, 0, Ang3(pEntity->GetWorldRotation()).z));
	}
	else
	{
		static float x = 5.0f;
		static float y = 1.5f;
		static float a = 0.0f;
		QuatT localPredictedLocationError(Vec3(x, y, 0), Quat(Ang3(0, 0, DEG2RAD(a))));
		predictedEntLocation = animLocation * localPredictedLocationError;
	}
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::DebugRenderDesiredLocationAndVelocity(const QuatT& desiredLocalLocation, const QuatT& desiredLocalVelocity) const
{
#ifdef _DEBUG
	if (CAnimationGraphCVars::Get().m_debugPrediction == 0)
		return;

	CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
	if (pPD == NULL)
		return;

	QuatT desiredWorldLocation = m_animLocation * desiredLocalLocation;
	QuatT desiredWorldVelocity(m_animLocation.q * desiredLocalVelocity.t, desiredLocalVelocity.q);

	static Vec3 pbump(0.005f,0,0.12f);
	static float predDuration = 0.5f;
	static float predTrailDuration = 5.0f;
	static ColorF predColor0(0,1,1,1);
	static ColorF predColor1(0.2f,1,1,1);
	static ColorF connectColor(0.5f,0.5f,1,0.2f);

	pPD->Begin(UNIQUE("AnimatedCharacter.DesiredLocationAndVelocity"), true);

	pPD->AddSphere(desiredWorldLocation.t+pbump, 0.05f, predColor0, predDuration);
	pPD->AddLine(desiredWorldLocation.t+pbump, desiredWorldLocation.t+pbump+Vec3(0,0,5), predColor0, predDuration);
	pPD->AddLine(desiredWorldLocation.t+pbump, desiredWorldLocation.t+pbump+(desiredWorldLocation.q*Vec3(0,1,0)), predColor1, predDuration);
	pPD->AddLine(desiredWorldLocation.t+pbump+(desiredWorldLocation.q*Vec3(-0.5,0,0)), desiredWorldLocation.t+pbump+(desiredWorldLocation.q*Vec3(+0.5,0,0)), predColor0, predDuration);

	pPD->AddLine(desiredWorldLocation.t+pbump, desiredWorldLocation.t+desiredWorldVelocity.t+pbump, ColorF(0.5f,0.7f,1,1), 0.5f); // linear velocity
	pPD->AddLine(m_animLocation.t+pbump, desiredWorldLocation.t+pbump, ColorF(0,0.5f,1,0.5f), 0.5f);
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::DebugGraphMotionParams(const ISkeletonAnim* pSkeleton)
{
#ifdef DEBUGHISTORY

	float paramTurnSpeed = pSkeleton->GetDesiredMotionParam(eMotionParamID_TurnSpeed);
	float paramTravelSlope = pSkeleton->GetDesiredMotionParam(eMotionParamID_TravelSlope);
	float paramTravelSpeed = pSkeleton->GetDesiredMotionParam(eMotionParamID_TravelSpeed);
	float paramTravelAngle = pSkeleton->GetDesiredMotionParam(eMotionParamID_TravelAngle);
	Vec2 paramTravelDir(-cry_sinf(paramTravelAngle), cry_cosf(paramTravelAngle));
	float paramTravelDist = pSkeleton->GetDesiredMotionParam(eMotionParamID_TravelDist);
	float paramTravelDistScale = pSkeleton->GetDesiredMotionParam(eMotionParamID_TravelDistScale);
	float paramCurvingFraction = pSkeleton->GetDesiredMotionParam(eMotionParamID_Curving);

/*
	const CAnimation& anim = const_cast<ISkeleton*>(pSkeleton)->GetAnimFromFIFO(0, 0);
	paramTurnSpeed = anim.m_LMG0.m_params[eMotionParamID_TurnSpeed].value;
	paramCurvingFraction = anim.m_LMG0.m_params[eMotionParamID_Curving].value;
/**/

	if (DebugFilter())
	{
		DebugHistory_AddValue("eDH_TurnSpeed", RAD2DEG(paramTurnSpeed));
		DebugHistory_AddValue("eDH_TravelSlope", paramTravelSlope);
		DebugHistory_AddValue("eDH_TravelSpeed", paramTravelSpeed);
		DebugHistory_AddValue("eDH_TravelDist", paramTravelDist);
		DebugHistory_AddValue("eDH_TravelDirX", paramTravelDir.x);
		DebugHistory_AddValue("eDH_TravelDirY", paramTravelDir.y);
		DebugHistory_AddValue("eDH_TravelDistScale", paramTravelDistScale);
		DebugHistory_AddValue("eDH_CurvingFraction", paramCurvingFraction);
	}

	CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();

	if ((pPD != NULL) && (CAnimationGraphCVars::Get().m_debugMotionParams != 0))
	{
		static Vec3 bump(0,0,0.13f);

		pPD->Begin(UNIQUE("AnimatedCharacter.MotionParams.TravelVector"), true);

		Vec3 travelVector(paramTravelDir.x * paramTravelDistScale, paramTravelDir.y * paramTravelDistScale, 0);
		pPD->AddLine(m_animLocation.t+bump, m_animLocation.t+bump+m_animLocation.q*travelVector, ColorF(1,1,1,1), 0.5f);
	}

#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::CalculateDesiredLocationAndVelocity(QuatT& desiredLocalLocation, QuatT& desiredLocalVelocity, float& desiredCatchupTime, float& immediateness, float frameTime, bool debug /* = false */)
{
	ANIMCHAR_PROFILE;

	// NOTE: This whole function was optimized to ignore xy rotations and only consider z rotations!

	// Use zero/identity prediction while anim target is active (prevents some jittering from PMC in end of smart objects).
	if ((m_pAnimTarget != NULL) && m_pAnimTarget->activated)
	{
		desiredLocalLocation.SetIdentity();
		desiredLocalVelocity.SetIdentity();
		// desiredCatchupTime is also input and good already.
		immediateness = 1.0f;
		return;
	}

	float invFrameTime = 1.0f / frameTime;
	Vec3 requestedEntLinVelo = m_requestedEntityMovement.t * invFrameTime;
	float requestedEntAngVelo = m_requestedEntityMovement.q.GetRotZ() * invFrameTime;

	QuatT animLocationInv = m_animLocation.GetInverted();

	// PREDICTION TIME
	const SPredictedCharacterStates* pPrediction = GetPredictedCharacterStates();
#ifdef _DEBUG
	if (pPrediction != NULL)
		assert(pPrediction->IsValid());
#endif

	float predictionTime = desiredCatchupTime;

	bool hasPrediction = (pPrediction != NULL) && (pPrediction->nStates > 0);

	// Don't use AI prediction during smart object approach. Synthesize prediction from immediate steering instead.
	bool approachingAnimTarget = (m_pAnimTarget != NULL) && m_pAnimTarget->preparing;
	if (approachingAnimTarget)
		hasPrediction = false;

	// PREDICTION TIME CLAMPING
	// We don't want the prediction time (catchup delay) to be too small, since the parameters will then ramp up to their max to catch up in no time.
	static float minPredictionTime = 0.05f; // This should be as small as possible though, but the current value is tweaked to look good.
	predictionTime = max(minPredictionTime, predictionTime);

	// PREDICTED WORLD LOCATION
	QuatT predictedWorldLocation;
	SPredictedCharacterState predictedState;
	if (hasPrediction)
	{
		predictedState = pPrediction->GetInterpolatedState(predictionTime);
		predictedWorldLocation.t = predictedState.position;
		predictedWorldLocation.t.z = m_animLocation.t.z;
		predictedWorldLocation.q = predictedState.orientation;
	}
	else
	{
		Vec3 predictedWorldLinMovement = requestedEntLinVelo * predictionTime;
		Quat predictedWorldAngMovement = m_requestedEntityMovement.q; // Don't predict rotation more than one frame, it changes too quickly for that.

		// When moving we want the rotational prediction to use full prediction time, 
		// to prevent the animation orientation from lagging behind.
		// Though, when standing still this is not desired, 
		// since the idleturn is selected prematurely due to the preduction being too far.
		if (m_actualEntVelocity > 0.1f)
			predictedWorldAngMovement.SetRotationZ(requestedEntAngVelo * predictionTime);

		QuatT predictedWorldMovement(predictedWorldLinMovement, predictedWorldAngMovement);
		predictedWorldLocation = ApplyWorldOffset(m_entLocation, predictedWorldMovement);
	}

#ifdef _DEBUG
	ProcessDebugPredictedEntLocation(predictedWorldLocation, m_animLocation);
#endif

	bool applyPredictionRetraction = true;

/*
	if (RecentQuickLoad())
		applyPredictionRetraction = false;
*/

	if (!RecentCollision())
		applyPredictionRetraction = false;

#ifdef _DEBUG
/*
	if (debug && DebugFilter())
	{
		DebugHistory_AddValue("eDH_TEMP00", m_smoothedActualAnimVelocity);
		DebugHistory_AddValue("eDH_TEMP01", RecentCollision() ? 1.0f : 0.0f);
		DebugHistory_AddValue("eDH_TEMP02", (float)(m_curFrameID - m_collisionFrameID));
		DebugHistory_AddValue("eDH_TEMP03", applyPredictionRetraction ? 0.0f : 1.0f);
	}
/**/
#endif

	if (applyPredictionRetraction)
	{
		Vec3 predictionWorldLocationOffset = predictedWorldLocation.t - m_entLocation.t;
		predictionWorldLocationOffset = RemovePenetratingComponent(predictionWorldLocationOffset, m_collisionNormal[0], 0.0f, 0.2f);
		predictionWorldLocationOffset = RemovePenetratingComponent(predictionWorldLocationOffset, m_collisionNormal[1], 0.0f, 0.2f);
		predictionWorldLocationOffset = RemovePenetratingComponent(predictionWorldLocationOffset, m_collisionNormal[2], 0.0f, 0.2f);
		predictionWorldLocationOffset = RemovePenetratingComponent(predictionWorldLocationOffset, m_collisionNormal[3], 0.0f, 0.2f);
		predictedWorldLocation.t = m_entLocation.t + predictionWorldLocationOffset;
	}

	// PREDICTED LOCAL LOCATION
	QuatT predictedLocalLocation = animLocationInv * predictedWorldLocation;

	// PREDICTED WORLD VELOCITY
	Vec3 predictedWorldLinVelo;
	float predictedWorldAngVelo;
	if (hasPrediction)
	{
		predictedWorldLinVelo = predictedState.velocity;
		predictedWorldAngVelo = 0.0f;
	}
	else
	{
		predictedWorldLinVelo = requestedEntLinVelo;
		predictedWorldAngVelo = requestedEntAngVelo;
	}

	// PREDICTED LOCAL VELOCITY
	Vec3 predictedLocalLinVelo = animLocationInv.q * predictedWorldLinVelo;
	float predictedLocalAngVelo = predictedWorldAngVelo;

	if (applyPredictionRetraction)
		//predictedLocalLinVelo = RemovePenetratingComponent(predictedLocalLinVelo, m_collisionNormal);
		predictedLocalLinVelo = ZERO;

	assert(predictedLocalLinVelo.IsValid());

	// IMMEDIATENESS
	static float predictionErrorDistanceMin = 0.1f;
	static float predictionErrorDistanceMax = 0.3f;
	static float predictionErrorDistanceSpan = (predictionErrorDistanceMax - predictionErrorDistanceMin);
	static float predictionErrorDistanceSpanInv = 1.0f / predictionErrorDistanceSpan;
	float predictionErrorDistance = predictedLocalLocation.t.GetLength();
	float predictionErrorDistanceFraction = (predictionErrorDistance - predictionErrorDistanceMin) * predictionErrorDistanceSpanInv;
	predictionErrorDistanceFraction = CLAMP(predictionErrorDistanceFraction, 0.0f, 1.0f);
	float predictionErrorDistanceImmediateness = 1.0f - predictionErrorDistanceFraction;

	static float predictionErrorAngleMin = 0.0f;
	static float predictionErrorAngleMax = 20.0f;
	static float predictionErrorAngleSpan = (predictionErrorAngleMax - predictionErrorAngleMin);
	static float predictionErrorAngleSpanInv = 1.0f / predictionErrorAngleSpan;

	//Vec3 predictionLocalDir = predictedLocalLocation.q.GetColumn1();
	//float predictionErrorAngle = abs(cry_atan2f(-predictionLocalDir.x, predictionLocalDir.y));
	float predictionErrorAngle = abs(predictedLocalLocation.q.GetRotZ()); // TODO: Verify that GetRotZ() behaves the same as the above!!!

	float predictionErrorAngleFraction = (predictionErrorAngle - predictionErrorAngleMin) * predictionErrorAngleSpanInv;
	predictionErrorAngleFraction = CLAMP(predictionErrorAngleFraction, 0.0f, 1.0f);
	float predictionErrorAngleImmediateness = 1.0f - predictionErrorAngleFraction;

	// If entity is almost not moving, we want to ramp down the predicted error distance, to prevent oscillating around a stationary entity.
	static float immediateVelocityMin = 0.0f;
	static float immediateVelocityMax = 0.5f;
	static float immediateVelocitySpan = (immediateVelocityMax - immediateVelocityMin);
	static float immediateVelocityspanInv = 1.0f / immediateVelocitySpan;
	float immediateTravelVelocity = requestedEntLinVelo.GetLength();
	float immediateTravelVelocityFraction = (immediateTravelVelocity - immediateVelocityMin) * immediateVelocityspanInv;
	immediateTravelVelocityFraction = CLAMP(immediateTravelVelocityFraction, 0.0f, 1.0f);

	immediateness = min(predictionErrorDistanceImmediateness, predictionErrorAngleImmediateness) * immediateTravelVelocityFraction;
	if (applyPredictionRetraction)
		immediateness = 0.0f;

	// EXTRAPOLATED PREDICTED LOCAL LOCATION
	QuatT predictedLocalLocationImmediate;
	predictedLocalLocationImmediate.t = predictedLocalLinVelo * predictionTime;
	predictedLocalLocationImmediate.q.SetRotationZ(predictedLocalAngVelo * predictionTime);

	// FINAL RESULT
	desiredLocalLocation.SetNLerp(predictedLocalLocation, predictedLocalLocationImmediate, immediateness);
	if (m_pCharacter && m_pCharacter->GetForceInPlaceAnimsOnMovingTrain())//big hack for character on the moving train not moving in place
		desiredLocalLocation.SetTranslation(Vec3(0.f, 0.f, 0.f));

	// When local player is in first person this flag will be set from Player.cpp.
	// Here it's used to slow down the animation locomotion speed to make the footstep sounds less frequent.
	if (m_params.flags & eACF_AlwaysPhysics)
	{
		desiredLocalLocation.t *= 0.7f;
		desiredLocalVelocity.t *= 0.7f;
	}

	desiredLocalVelocity.t = predictedLocalLinVelo;
	// TODO: WARNING: This can't represent rotations bigger than 360 deg/sec!!!
	desiredLocalVelocity.q.SetRotationZ(predictedLocalAngVelo);

	desiredCatchupTime = predictionTime;
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::CalculateParamsForCurrentMotions()
{
	ANIMCHAR_PROFILE;

#if _DEBUG && defined(USER_david)
	SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

	IEntity* pEntity = GetEntity();
	assert(pEntity);

	ICharacterInstance* pCharacterInstance = pEntity->GetCharacter(0);
	if (pCharacterInstance == NULL)
		return;

	ISkeletonAnim* pSkeletonAnim = pCharacterInstance->GetISkeletonAnim();
	if (pSkeletonAnim == NULL)
		return;

	// Get voted lookahead time from currently playing animations.
	// This could potentially also be a max(), but voted average seems to work fine.
	float lookaheadTime = 0.0f;

	float lookaheadTimeWeightSum = 0.0f;
	uint32 animCount = pSkeletonAnim->GetNumAnimsInFIFO(0);
	for (uint32 animIndex = 0; animIndex < animCount; ++animIndex)
	{
		const CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(0, animIndex);
		lookaheadTime += anim.m_LMG0.m_fDesiredLocalLocationLookaheadTime;
		lookaheadTimeWeightSum += anim.m_fTransitionWeight;
	}
	if (lookaheadTimeWeightSum > 0.0f)
		lookaheadTime /= lookaheadTimeWeightSum;

	QuatT desiredLocalLocation;
	QuatT desiredLocalVelocity;
	float immediateness;
	s_desiredParams.LookupDesiredLocationAndVelocity(lookaheadTime, desiredLocalLocation, desiredLocalVelocity, immediateness);

	// Without this parameterization is running one frame ahead, for some reason. Find out why and fix properly.
	lookaheadTime += HasAtomicUpdate() ? m_curFrameTime : 0.0f;

	pSkeletonAnim->SetDesiredLocalLocation(desiredLocalLocation, lookaheadTime, m_curFrameTime, m_isPlayer ? 1.0f : 1.0f);

	if (EnableProceduralLeaning())
	{
		pSkeletonAnim->SetDesiredMotionParam(eMotionParamID_Curving, 0.0f, m_curFrameTime);
	}
	else
	{
		if (CAnimationGraphCVars::Get().ca_GameControlledStrafingPtr->GetIVal() != 0)
			pSkeletonAnim->SetDesiredMotionParam(eMotionParamID_Curving, m_moveRequest.allowStrafe ? 0.0f : 1.0f, m_curFrameTime);
	}

#ifdef _DEBUG
	DebugGraphQT(desiredLocalLocation, "eDH_DesiredLocalLocationTX", "eDH_DesiredLocalLocationTY", "eDH_DesiredLocalLocationRZ");
	DebugGraphQT(desiredLocalVelocity, "eDH_DesiredLocalVelocityTX", "eDH_DesiredLocalVelocityTY", "eDH_DesiredLocalVelocityRZ");

	if (DebugFilter())
	{
		DebugHistory_AddValue("eDH_PredictionTime", lookaheadTime);
		DebugHistory_AddValue("eDH_Immediateness", immediateness);
	}

	DebugRenderDesiredLocationAndVelocity(desiredLocalLocation, desiredLocalVelocity);
	DebugGraphMotionParams(pSkeletonAnim);
	DebugRenderFutureAnimPath(pSkeletonAnim, lookaheadTime);
#endif
}

//--------------------------------------------------------------------------------

void CAnimatedCharacter::SDesiredParams::LookupDesiredLocationAndVelocity(float& time, QuatT& desiredLocalLocation, QuatT& desiredLocalVelocity, float& immediateness)
{
	static const float timeStep = 1.0f / (float)LAST_PARAM;
	static const float timeStepInv = 1.0f / timeStep;
	float fIndex = time * timeStepInv;
	int index = (int)(time * timeStepInv);

	if (index >= LAST_PARAM)
	{
		//we clamp to the end
		desiredLocalLocation = this->location[LAST_PARAM];
		desiredLocalVelocity = this->velocity[LAST_PARAM];
		immediateness = this->immediateness[LAST_PARAM];
		time = this->time[LAST_PARAM];
		return;
	}

	if ((index < 0) || ((index == 0) && (fIndex == 0.0f)))
	{
		// we clamp to the beginning
		desiredLocalLocation = this->location[0];
		desiredLocalVelocity = this->velocity[0];
		immediateness = this->immediateness[0];
		time = this->time[0];
		return;
	}

	float fraction = fIndex - (float)index;
	desiredLocalLocation.SetNLerp(this->location[index], this->location[index+1], fraction);
	desiredLocalVelocity.SetNLerp(this->velocity[index], this->velocity[index+1], fraction);
	immediateness = LERP(this->immediateness[index], this->immediateness[index+1], fraction);
	time = LERP(this->time[index], this->time[index+1], fraction);
}

//--------------------------------------------------------------------------------
