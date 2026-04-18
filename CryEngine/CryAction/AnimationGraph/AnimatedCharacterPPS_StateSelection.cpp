//--------------------------------------------------------------------------------

#include "StdAfx.h"
#include "AnimatedCharacter.h"
#include "CryAction.h"
#include "AnimationGraphManager.h"
#include "AnimationGraph.h"
#include "AnimationGraphCVars.h"
#include "HumanBlending.h"
#include "PersistantDebug.h"
#include "IFacialAnimation.h"
#include <IViewSystem.h>

//--------------------------------------------------------------------------------

#define BLENDCODE(code)		(*(uint32*)code)

//--------------------------------------------------------------------------------

CStateIndex::StateID CAnimatedCharacter::SelectLocomotionState(_smart_ptr<CAnimationGraph> pAnimGraph, CStateIndex::StateID curStateID, const CStateIndex::StateIDVec& stateIDs, CStateIndex::StateID defaultStateID)
{
	ANIMCHAR_PROFILE;

#if _DEBUG && defined(USER_david)
	SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

	if (m_simplifyMovement)
		return defaultStateID;

	if (pAnimGraph == NULL)
		return defaultStateID;

	if (stateIDs.size() < 2) // (will never be less than one, but just in case the calling code is broken.)
		return defaultStateID;

	RefreshAnimTarget();

	float bestError = 10000.0f;
	uint32 bestCapsCode = 0;
	CStateIndex::StateID bestStateID = defaultStateID;
	SLocomotionStateSelectionParameters bestParams;

	if (m_actualEntVelocity < 0.1f)
	{
		ANIMCHAR_PROFILE_SCOPE("SelectLocomotionState_IdleDeviationTimeout");

		bool stablePrediction = true;

		float distance = s_desiredParams.location[SDesiredParams::LAST_PARAM].t.GetLengthSquared();
		if (distance > (0.05f * 0.05f))
		{
			if (m_devatedPositionTime.GetValue() == 0)
				m_devatedPositionTime = m_curFrameStartTime;
		}
		else
		{
			m_devatedPositionTime.SetValue(0);
		}

		float angle = RAD2DEG(GetQuatAbsAngle(s_desiredParams.location[SDesiredParams::LAST_PARAM].q));
		if (angle > 5.0f)
		{
			if (m_devatedOrientationTime.GetValue() == 0)
				m_devatedOrientationTime = m_curFrameStartTime;
		}
		else
		{
			m_devatedOrientationTime.SetValue(0);
		}
	}
	else
	{
		m_devatedPositionTime.SetValue(0);
		m_devatedOrientationTime.SetValue(0);
	}

	const CAnimationGraphState* pAGState = (const CAnimationGraphState*) m_animationGraphStates.GetLayer(0);

	const SAnimationDesc* animDescCur = pAnimGraph->GetAnimationDesc(GetEntity(), defaultStateID, pAGState);
	const SAnimationSelectionProperties* animPropsCur = (animDescCur != NULL) ? animDescCur->properties : NULL;
	SLocomotionStateSelectionParameters params = CalculateStateSelectionParams();

	int stateIDCount = stateIDs.size();
	for (int i = 0; i < stateIDCount; ++i)
	{
		CStateIndex::StateID stateID = stateIDs[i];

/*
		if ((stateID != curStateID) && !pAnimGraph->IsDirectlyLinked(curStateID, stateID))
			continue;
*/

		const SAnimationDesc* animDesc = pAnimGraph->GetAnimationDesc(GetEntity(), stateID, pAGState);
		if (animDesc == NULL)
			continue;

		SAnimationSelectionProperties animPropsNull; animPropsNull.DebugCapsCode = BLENDCODE("IDLE");
		const SAnimationSelectionProperties* animProps = animDesc->properties;
		if (animProps != NULL)
		{
/*
			// TODO: Verify that this works and does not cause any undesired side effects.
			// Skip guarded/predicted states when character is not in decoupled movement.
			if ((GetMCMH() != eMCM_DecoupledCatchUp) && (animProps->m_bPredicted))
				continue;
/**/

			if ((CAnimationGraphCVars::Get().m_disableFancyTransitions != 0) && animProps->m_bPredicted)
				continue;

			if ((m_pAnimTarget != NULL) && (m_pAnimTarget->doingSomething || m_pAnimTarget->allowActivation) && animProps->m_bPredicted)
				continue;

			// Default properties (else branch) don't have locomotion flag set, but we don't want to early out on those anyway.
			if (!animProps->m_bLocomotion && (curStateID != stateID))
				continue;
		}
		else
		{
			// If we have a selectable null-state, we use default (zero) selection properties.
			// (For example, this will represent all the idle variations force followed from the idle null node.)
			animProps = &animPropsNull;
		}

		bool current = (defaultStateID == stateID);
		float error = CanAnimCatchUp(*animProps, params, current);

		// The three sections in the following scope is a poor mans context sensetive state selection.
		{
			// Prevent nonsense sequences.
		  if (animPropsCur != NULL)
		  {
			  if (((animPropsCur->DebugCapsCode == BLENDCODE("WALK")) && (animProps->DebugCapsCode == BLENDCODE("I2W_"))) ||
					  //((animPropsCur->DebugCapsCode == BLENDCODE("WALK")) && (animProps->DebugCapsCode == BLENDCODE("IROT"))) ||
					  //((animPropsCur->DebugCapsCode == BLENDCODE("WALK")) && (animProps->DebugCapsCode == BLENDCODE("ISTP"))) ||
					  ((animPropsCur->DebugCapsCode == BLENDCODE("I2W_")) && (animProps->DebugCapsCode == BLENDCODE("IROT"))) ||
					  ((animPropsCur->DebugCapsCode == BLENDCODE("I2W_")) && (animProps->DebugCapsCode == BLENDCODE("ISTP"))) ||
					  ((animPropsCur->DebugCapsCode == BLENDCODE("IDLE")) && (animProps->DebugCapsCode == BLENDCODE("W2I_"))) ||
					  ((animPropsCur->DebugCapsCode == BLENDCODE("ISTP")) && (animProps->DebugCapsCode == BLENDCODE("W2I_"))) ||
					  ((animPropsCur->DebugCapsCode == BLENDCODE("IROT")) && (animProps->DebugCapsCode == BLENDCODE("W2I_"))))
				  error += 100.0f;
			  if (((animPropsCur->DebugCapsCode == BLENDCODE("RUN_")) && (animProps->DebugCapsCode == BLENDCODE("I2M_"))) ||
					  //((animPropsCur->DebugCapsCode == BLENDCODE("RUN_")) && (animProps->DebugCapsCode == BLENDCODE("IROT"))) ||
					  //((animPropsCur->DebugCapsCode == BLENDCODE("RUN_")) && (animProps->DebugCapsCode == BLENDCODE("ISTP"))) ||
					  ((animPropsCur->DebugCapsCode == BLENDCODE("I2M_")) && (animProps->DebugCapsCode == BLENDCODE("IROT"))) ||
					  ((animPropsCur->DebugCapsCode == BLENDCODE("I2M_")) && (animProps->DebugCapsCode == BLENDCODE("ISTP"))) ||
					  ((animPropsCur->DebugCapsCode == BLENDCODE("IDLE")) && (animProps->DebugCapsCode == BLENDCODE("M2I_"))) ||
					  ((animPropsCur->DebugCapsCode == BLENDCODE("ISTP")) && (animProps->DebugCapsCode == BLENDCODE("M2I_"))) ||
					  ((animPropsCur->DebugCapsCode == BLENDCODE("IROT")) && (animProps->DebugCapsCode == BLENDCODE("M2I_"))))
				  error += 100.0f;
		  }

			// Prefer fancy transitions if they are accurate enough, 
			// since RUN is overlapping to be selectable without transitions.
		  if ((error < 1.0f) && (error >= bestError))
		  {
			  if (((animProps->DebugCapsCode == BLENDCODE("I2M_")) || 
					   (animProps->DebugCapsCode == BLENDCODE("M2I_"))) &&
					  ((bestCapsCode == BLENDCODE("RUN_")) || (bestCapsCode == BLENDCODE("WALK") && m_Idle2MovePriority > 0)))
			  {
				  error = -0.1f + bestError * 0.9f;
			  }
			  if (((animProps->DebugCapsCode == BLENDCODE("I2W_")) || 
					   (animProps->DebugCapsCode == BLENDCODE("W2I_"))) &&
					  (bestCapsCode == BLENDCODE("WALK")))
			  {
				  error = -0.1f + bestError * 0.9f;
			  }
		  }
  
			// Prefer fancy transitions if they are accurate enough, 
			// since RUN is overlapping to be selectable without transitions.
		  if (bestError < 1.0f)
		  {
			  if (((bestCapsCode == BLENDCODE("I2M_")) || 
				  (bestCapsCode == BLENDCODE("M2I_"))) &&
				  ((animProps->DebugCapsCode == BLENDCODE("RUN_")) || (animProps->DebugCapsCode == BLENDCODE("WALK")  && m_Idle2MovePriority > 0)))
			  {
				  error = 0.1f + bestError / 0.9f;
			  }
			  if (((bestCapsCode == BLENDCODE("I2W_")) || 
				  (bestCapsCode == BLENDCODE("W2I_"))) &&
				  (animProps->DebugCapsCode == BLENDCODE("WALK")))
			  {
				  error = 0.1f + bestError / 0.9f;
			  }
		  }
		}
  
//		if (m_isClient)
		{
			// m_Idle2MovePriority 
			//  0 : regular behavior 
			// >0 : always use
			// <0 : never use
			if (animProps->DebugCapsCode == BLENDCODE("I2M_"))
			{
				if (m_Idle2MovePriority > 0)
				{
					if ((bestCapsCode == BLENDCODE("WALK") || bestCapsCode == BLENDCODE("RUN_")) && 
						((animPropsCur == NULL) || animPropsCur->DebugCapsCode == BLENDCODE("IDLE")) )
					{
						if (params.m_fUrgency > 0.0f)
						{
							error = 0;
						}
					}
					else if ((animPropsCur != NULL) && (bestCapsCode == 0) && (animPropsCur->DebugCapsCode == BLENDCODE("I2M_")))
					{
						if (params.m_fUrgency > 0.0f)
						{
							error = 0;
						}
					}
				}
				else if (m_Idle2MovePriority < 0)
				{
					continue;
				}
			}
		}

		if (error <= bestError)
		{
			bestStateID = stateID;
			bestError = error;
			bestParams = params;
			bestCapsCode = animProps->DebugCapsCode;
		}
	}

#ifdef _DEBUG
	if ((bestError < 100.0f) && DebugFilter()) // check for debug histories initialization
	{
		DebugHistory_AddValue("eDH_StateSelection_StartTravelSpeed", bestParams.m_fStartTravelSpeed);
		DebugHistory_AddValue("eDH_StateSelection_EndTravelSpeed", bestParams.m_fEndTravelSpeed);
		DebugHistory_AddValue("eDH_StateSelection_TravelDistance", bestParams.m_fTravelDistance);
		DebugHistory_AddValue("eDH_StateSelection_StartTravelAngle", bestParams.m_fStartTravelAngle);
		DebugHistory_AddValue("eDH_StateSelection_EndTravelAngle", bestParams.m_fEndTravelAngle);
		DebugHistory_AddValue("eDH_StateSelection_EndBodyAngle", bestParams.m_fEndBodyAngle);
	}
#endif

	// For debugging...
	int x;
	if (bestCapsCode == BLENDCODE("RUN_"))
		x = 1;
	else if (bestCapsCode == BLENDCODE("I2M_"))
		x = 2;
	else if (bestCapsCode == BLENDCODE("M2I_"))
		x = 3;
	else
		x = 0;

	return bestStateID;
}

//--------------------------------------------------------------------------------

bool CAnimatedCharacter::ValidateAnimGraphPathNode(_smart_ptr<CAnimationGraph> pAnimGraph, CStateIndex::StateID stateID)
{
	ANIMCHAR_PROFILE;
/*
	if (m_pAnimTarget != NULL)
		return false;
*/
	if (pAnimGraph == NULL)
		return true;

	if (m_simplifyMovement)
		return true;

	const CAnimationGraphState* pAGState = (const CAnimationGraphState*) m_animationGraphStates.GetLayer(0);

	const SAnimationDesc* animDesc = pAnimGraph->GetAnimationDesc(GetEntity(), stateID, pAGState);
	if (animDesc == NULL)
		return true;

	const SAnimationSelectionProperties* animProps = animDesc->properties;
	if (animProps == NULL)
		return true;

	if ((CAnimationGraphCVars::Get().m_disableFancyTransitions != 0) && animProps->m_bPredicted)
		return false;

	SLocomotionStateSelectionParameters params = CalculateStateSelectionParams();

	if (params.m_fUrgency < animProps->m_fUrgencyMin)
		return false;

	if (m_pAnimTarget != NULL)
	{
		if (m_pAnimTarget->activated)
			return false;

		if (m_pAnimTarget->preparing)
		{
			Vec3 distanceVector = m_pAnimTarget->position - m_animLocation.t;
			float distance = distanceVector.GetLength2D();
			if (distance < (animProps->m_fTravelDistanceMin * 2.0f))
				return false;
		}
	}

	if ((params.m_fTravelDistance < 0.5f) || animProps->m_bPredicted)
	{
		if (params.m_fTravelDistance < (animProps->m_fTravelDistanceMin * 0.9f))
			return false;

		if ((params.m_fImmediateness > 0.9f) && animProps->m_bPredicted)
			return false;

		#define RANGEERROR0(x,a,b)			max(max(0.0f, -(x - a)), max(0.0f, x - b))

		float endTravelToBodyAngle = params.m_fEndTravelAngle - params.m_fEndBodyAngle;
		endTravelToBodyAngle = Snap_s180(endTravelToBodyAngle);
		float errorEndTravelToBodyAngle = RANGEERROR0(endTravelToBodyAngle, animProps->m_fEndTravelToBodyAngleMin, animProps->m_fEndTravelToBodyAngleMax);

		static float thresholdEndTravelToBodyAngle = 20.0f;
		if (errorEndTravelToBodyAngle > thresholdEndTravelToBodyAngle)
			return false;
	}

	return true;
}

//--------------------------------------------------------------------------------

float CAnimatedCharacter::CanAnimCatchUp(const SAnimationSelectionProperties& props, const SLocomotionStateSelectionParameters& params, bool current)
{
	#define RANGEERROR0(x,a,b)			max(max(0.0f, -(x - a)), max(0.0f, x - b))
	#define RANGEERROR(component)		RANGEERROR0(params.component, props.component##Min, props.component##Max)

	float weightUrgency = 1000.0f;
	float weightDuration = 0.0f;
	float weightStartTravelSpeed = 50.0f;
	float weightEndTravelSpeed = 50.0f;
	float weightTravelDistance = 50.0f;
	float weightStartTravelAngle = 0.05f; // 1/20
	float weightEndTravelAngle = 0.05f; // 1/20
	float weightEndBodyAngle = 0.05f; // 1/20
	float weightTravelAngleChange = 0.05f; // 1/20
	float weightEndTravelToBodyAngle = 0.05f; // 1/20


	float errorUrgency = RANGEERROR(m_fUrgency) * weightUrgency;
	float errorDuration = RANGEERROR(m_fDuration) * weightDuration;
	float errorStartTravelSpeed = RANGEERROR(m_fStartTravelSpeed) * weightStartTravelSpeed;
	float errorEndTravelSpeed = RANGEERROR(m_fEndTravelSpeed) * weightEndTravelSpeed;
	float errorTravelDistance = RANGEERROR(m_fTravelDistance) * weightTravelDistance;
	float errorStartTravelAngle = RANGEERROR(m_fStartTravelAngle) * weightStartTravelAngle;
	float errorEndTravelAngle = RANGEERROR(m_fEndTravelAngle) * weightEndTravelAngle;
	float errorEndBodyAngle = RANGEERROR(m_fEndBodyAngle) * weightEndBodyAngle;

	// AI for some reason need to be more aligned with the desired direction, but should also not just make more but smaller turns.
	// Therefor the need for some timeout concept that over times makes the thresholds smaller.
	float devatedPositionTimer = (m_devatedPositionTime.GetValue() != 0) ? (m_curFrameStartTime - m_devatedPositionTime).GetSeconds() : 0.0f;
	float devatedOrientationTimer = (m_devatedOrientationTime.GetValue() != 0) ? (m_curFrameStartTime - m_devatedOrientationTime).GetSeconds() : 0.0f;
	float devatedPositionTimerFraction = CLAMP((devatedPositionTimer - 1.0f) / 1.0f, 0.0f, 1.0f);
	float devatedOrientationTimerFraction = CLAMP((devatedOrientationTimer - 1.0f) / 1.0f, 0.0f, 1.0f);
	float positionThresholdScale = (1.0f - devatedPositionTimerFraction);
	float orientationThresholdScale = (1.0f - devatedOrientationTimerFraction);

	//DebugHistory_AddValue("eDH_TEMP00", devatedPositionTimer);
	//DebugHistory_AddValue("eDH_TEMP01", devatedOrientationTimer);

	// IdleTurns should not be interrupted when they are currently playing, or the angle will not be consumed.
	// The reason for this is that the angle threshold is quite big (70-90 degrees), while the distance threshold is quite small (0.1 meters).
	float motionEndBodyAngleThreshold = props.m_fEndBodyAngleThreshold * orientationThresholdScale;
	if (!current || (m_devatedOrientationTime.GetValue() == 0))
	{
		if ((props.m_fEndBodyAngleMin < -motionEndBodyAngleThreshold) && 
			(props.m_fEndBodyAngleMax > motionEndBodyAngleThreshold) && 
			(abs(params.m_fEndBodyAngle) < motionEndBodyAngleThreshold))
		{
			errorEndBodyAngle += 100.0f * motionEndBodyAngleThreshold * weightEndBodyAngle;
		}
	}

	// IdleSteps can be interrupted if the distance is small enough, just blend them out.
	// TODO: Find out why distance threshold can't be applied only for non current states.
	// The angle threshold is quite big (70-90 degrees), while the distance threshold is quite small (0.1 meters).
	float motionTravelDistanceThreshold = props.m_fTravelDistanceThreshold * positionThresholdScale;
	{
		if ((props.m_fTravelDistanceMax > motionTravelDistanceThreshold) && 
			(abs(params.m_fTravelDistance) < motionTravelDistanceThreshold))
		{
			errorTravelDistance += 100.0f * motionTravelDistanceThreshold * weightTravelDistance;
		}
	}

	float travelAngleChange = params.m_fEndTravelAngle - params.m_fStartTravelAngle;
	if (travelAngleChange > 180.0f)
		travelAngleChange -= 360.0f;
	if (travelAngleChange < -180.0f)
		travelAngleChange += 360.0f;
	float errorTravelAngleChange = RANGEERROR0(travelAngleChange, props.m_fTravelAngleChangeMin, props.m_fTravelAngleChangeMax) * weightEndTravelToBodyAngle;

	float endTravelToBodyAngle = params.m_fEndTravelAngle - params.m_fEndBodyAngle;
	if (endTravelToBodyAngle > 180.0f)
		endTravelToBodyAngle -= 360.0f;
	if (endTravelToBodyAngle < -180.0f)
		endTravelToBodyAngle += 360.0f;
	float errorEndTravelToBodyAngle = RANGEERROR0(endTravelToBodyAngle, props.m_fEndTravelToBodyAngleMin, props.m_fEndTravelToBodyAngleMax) * weightEndTravelToBodyAngle;

	float sum = (errorUrgency + 
							 errorDuration + 
							 errorStartTravelSpeed + 
							 errorEndTravelSpeed + 
							 errorTravelDistance + 
							 errorStartTravelAngle + 
							 errorEndTravelAngle + 
							 errorEndBodyAngle + 
							 errorTravelAngleChange + 
							 errorEndTravelToBodyAngle);
	return sum;
}

//--------------------------------------------------------------------------------

SLocomotionStateSelectionParameters CAnimatedCharacter::CalculateStateSelectionParams()
{
	SLocomotionStateSelectionParameters params;

	float desiredCatchupTime = 1.0f;
	QuatT desiredLocalLocation;
	QuatT desiredLocalVelocity;

	params.m_fUrgency = m_animationGraphStates.GetInputAsFloat(m_inputID[eACInputIndex_PseudoSpeed]);

	s_desiredParams.LookupDesiredLocationAndVelocity(desiredCatchupTime, desiredLocalLocation, desiredLocalVelocity, params.m_fImmediateness);
	params.m_fTravelDistance = desiredLocalLocation.t.GetLength2D();

	params.m_fDuration = desiredCatchupTime;
	params.m_fEndTravelSpeed = desiredLocalVelocity.t.GetLength();

	params.m_fStartTravelSpeed = m_actualEntVelocity;
	if (desiredLocalLocation.t.GetLengthSquared2D() > 0.01f)
		params.m_fStartTravelAngle = RAD2DEG(cry_atan2f(-desiredLocalLocation.t.x, desiredLocalLocation.t.y));
	else
		params.m_fStartTravelAngle = 0.0f;
	if (desiredLocalVelocity.t.GetLengthSquared2D() > 0.01f)
		params.m_fEndTravelAngle = RAD2DEG(cry_atan2f(-desiredLocalVelocity.t.x, desiredLocalVelocity.t.y));
	else
		params.m_fEndTravelAngle = params.m_fStartTravelAngle;

	//Vec3 desiredLocalDir = desiredLocalLocation.q.GetColumn1();
	//params.m_fEndBodyAngle = RAD2DEG(cry_atan2f(-desiredLocalDir.x, desiredLocalDir.y));
	params.m_fEndBodyAngle = RAD2DEG(desiredLocalLocation.q.GetRotZ());

	// This prevents walking very short distances (while making sure it's not just a very slow but continuous walk).
	// Also, don't care if walk or run is requested, just force it to zero urgency if only a nudge is requested.
	// TODO: What was !current doing here?
	if (/*!current &&*/ (params.m_fUrgency > 0.0f))
	{
		if ((params.m_fTravelDistance < 0.5f) && 
		    (params.m_fStartTravelSpeed < 0.2f) && 
		    (params.m_fEndTravelSpeed < 0.2f))
		{
			params.m_fUrgency = 0.0f;
		}
	}

	return params;
}

//--------------------------------------------------------------------------------

