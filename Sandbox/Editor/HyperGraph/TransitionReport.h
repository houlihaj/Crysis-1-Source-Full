#ifndef __TRANSITIONREPORT_H__
#define __TRANSITIONREPORT_H__

#pragma once

#include "AnimationGraph.h"

void FindLongTransitions( CString& out, CAnimationGraphPtr pGraph, size_t minCost );

#endif
