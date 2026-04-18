/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	CVars for the AnimationGraph Subsystem

-------------------------------------------------------------------------
History:
- 02:03:2006  12:00 : Created by AlexL

*************************************************************************/

#ifndef __ANIMATIONGRAPH_CVARS_H__
#define __ANIMATIONGRAPH_CVARS_H__

#pragma once

struct ICVar;

struct CAnimationGraphCVars
{
	int m_agHumanBlending;  // debug Ivo's human-blending
	int m_logEffects;     // AGAttachmentEffect logging
	int m_logSounds;      // AGSound logging
	int m_logSelections;  // Log animation graph selection results
	int m_logTransitions; // Log animation graph transition calls to the console
	int m_breakMode;			// Are we single stepping?
	int m_debugExactPos;  // Enable exact positioning debug
	int m_showPhysSync;   // show physics synchronization disc
	int m_correctMovementInExactPositioning;
	int m_showExactPositioningCorrectionPath;
	int m_log; // show the anim graph debug log
	int m_fpAnimPop;
	int m_debugMusic;
	int m_debugLayer;

	// DEPRECATED, will soon be removed
	int m_LockToEntity; // Lock animation to entity (zero offsetting)
	int m_adjustToCatchUp; // Adjust requested move direction of animation to catch up with the entity.
	int m_ForceAdjust; // Enable forced small step adjustments.
	int m_forceInsideErrorDisc; // Forces the animation to stay within the maximum error distance.
	int m_MeasureActualSpeeds; // Measure the actual travel speeds of the entity and the animation origins.
	int m_averageTravelSpeed; // Average travel speed over a few frames.

	// DEPRECATED, will soon be removed
	float m_TargetCorrectionTimeScale; // > 1.0f will make the target correction more biased towards the beginning of the animation.
	float m_TriggerCorrectionTimeScale; // > 1.0f will make the target correction more biased towards the beginning of the animation.
	float m_PredictionSmoothingPos;
	float m_PredictionSmoothingOri;
	float m_PredictionProbabilityPos;
	float m_PredictionProbabilityOri;

	int m_SafeExactPositioning; // Will teleport the entity to the requested position/orientation when EP think it's done.

	int m_entityAnimClamp;
  int m_animErrorClamp;
	float m_animErrorMaxDistance;
	float m_animErrorMaxAngle;

	int m_disableFancyTransitions;

	float m_clampDurationEntity;
	float m_clampDurationAnimation;

	int m_debugErrors; // displays debug infos on the entities

	int m_MCMH;
	int m_MCMV;
	int m_MCMFilter;
	int m_TemplateMCMs;
	int m_forceColliderModePlayer;
	int m_forceColliderModeAI;
	int m_enableExtraSolidCollider;
	int m_debugLocations;
	ICVar* m_pDebugFilter;
	int m_debugText;
	int m_debugSelection;
	int m_debugSelectionParams;
	int m_debugMotionParams;
	int m_debugPrediction;
	int m_debugLocationsGraphs;
	int m_debugAnimError;
	int m_debugAnimTarget;
	int m_debugFutureAnimPath;
	int m_debugColliderMode;
	ICVar* m_debugColliderModeName;
	int m_debugMovementControlMethods;
	int m_debugTempValues;
	int m_debugTweakTrajectoryFit;
	int m_debugFrameTime;
	int m_debugCarryClamping;
	int m_debugEntityParams;
	int m_forceSimpleMovement;
	int m_forceNoSimpleMovement;
	int m_disableSlidingContactEvents;
	int m_debugAnimEffects;
	
	ICVar* ca_GameControlledStrafingPtr;

	int m_carryCorrection_filter;
	float m_carryCorrection_distanceFilterSize;
	float m_carryCorrection_angleFilterSize;
	int m_carryCorrection_clamp;
	float m_carryCorrection_maxDistance;
	float m_carryCorrection_maxAngle;

	float m_physErrorInnerRadiusFactor;
	float m_physErrorOuterRadiusFactor;
	float m_physErrorMinOuterRadius;
	float m_physErrorMaxOuterRadius;

	ICVar * m_pLogEntity; // filter the log for only this entity
	ICVar * m_pBreakOnQuery; // If we get this state as a query result, enter break mode
	ICVar * m_pQueue;     // Next state to force
	ICVar * m_pSignal;    // Next signal to send
	ICVar * m_pAction;    // Force this action if set
	ICVar * m_pItem;      // Force this item if set
	ICVar * m_pStance;      // Force this stance if set
	ICVar * m_pCaDebug;   // Unused, kept for reference
	ICVar * m_pShowMovementRequests; // Show movement requests?

	static __inline CAnimationGraphCVars& Get()
	{
		assert (s_pThis != 0);
		return *s_pThis;
	}

private:
	friend class CAnimationGraphManager; // Our only creator

	CAnimationGraphCVars(); // singleton stuff
	~CAnimationGraphCVars();
	CAnimationGraphCVars(const CAnimationGraphCVars&);
	CAnimationGraphCVars& operator= (const CAnimationGraphCVars&);

	static CAnimationGraphCVars* s_pThis;
};

#endif // __ANIMATIONGRAPH_CVARS_H__
