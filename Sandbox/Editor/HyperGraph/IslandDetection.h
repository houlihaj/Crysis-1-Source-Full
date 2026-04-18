// routines for detecting islands of states

#ifndef __ISLANDDETECTION_H__
#define __ISLANDDETECTION_H__

#pragma once

#include <set>
#include "AnimationGraph.h"

typedef std::vector<CAGStatePtr> TAGIsland;
typedef std::list<TAGIsland> TAGIslands;

void GenerateIslands( TAGIslands& islands, CAnimationGraphPtr pGraph );
CString GenerateIslandReport( const TAGIslands& islands );
CString GenerateIslandReport( CAnimationGraphPtr pGraph );

#endif
