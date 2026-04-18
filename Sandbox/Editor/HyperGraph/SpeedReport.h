#ifndef __SPEEDREPORT_H__
#define __SPEEDREPORT_H__

#pragma once

#include "ICryAnimation.h"
#include "AnimationGraph.h"

CString GenerateSpeedReport( IAnimationSet * pAnimSet );
CString MatchMovementSpeedsToAnimations( IAnimationSet * pAnimSet, CAnimationGraph * pGraph );
CString GenerateBadCALReport( IAnimationSet * pAnimSet, CAnimationGraph * pGraph );
CString GenerateNullNodesWithNoForceLeaveReport( CAnimationGraph * pGraph );
CString GenerateOrphanNodesReport( CAnimationGraph * pGraph );
CString GenerateDeadInputsReport( CAnimationGraph * pGraph );

#endif
