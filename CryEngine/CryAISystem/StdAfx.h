// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__81DAABA0_0054_42BF_8696_D99BA6832D03__INCLUDED_)
#define AFX_STDAFX_H__81DAABA0_0054_42BF_8696_D99BA6832D03__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <CryModuleDefs.h>
#define eCryModule eCryM_AISystem
#define RWI_NAME_TAG "RayWorldIntersection(AI)"
#define PWI_NAME_TAG "PrimitiveWorldIntersection(AI)"

#define CRYAISYSTEM_EXPORTS

#include <platform.h>

#include <stdio.h>

#include <limits>
#include <vector>
#include <map>
#include <numeric>
#include <algorithm>
#include <list>
#include <set>
#include <deque>
#include <memory>  // std::auto_ptr  // new implementation

// Reference additional interface headers your program requires here (not local headers)

#include "Cry_Math.h"
#include <CryArray.h>
#include "Cry_XOptimise.h" // required by AMD64 compiler
#include <Cry_Geo.h>
#include <ISystem.h>
#include "IScriptSystem.h"
#include <IConsole.h>
#include <ILog.h>
#include <ISerialize.h>
#include <IFlowSystem.h>
#include <IAIAction.h>
#include <IPhysics.h>
#include <I3DEngine.h>
#include <ITimer.h>
#include <IAgent.h>
#include <IEntity.h>
#include <IEntitySystem.h>
#include <CryFile.h>
//#include "ICryPak.h"
#include <IXml.h>
#include <ISerialize.h>
#include <IRenderer.h>
#include <IRenderAuxGeom.h>
#include <MTPseudoRandom.h>

extern int g_random_count;

//////////////////////////////////////////////////////////////////////////
inline unsigned int ai_rand()
{
	//g_random_count++;
	return cry_rand();
}
inline float  ai_frand()
{
	//g_random_count++;
	return cry_frand();
}

/// This frees the memory allocation for a vector (or similar), rather than just erasing the contents
template<typename T>
void ClearVectorMemory(T &container)
{
  T().swap(container);
}

// adding some headers here can improve build times... but at the cost of the compiler not registering
// changes to these headers if you compile files individually.
//#if !defined(USER_danny) && !defined(USER_mikko)
#include "AILog.h"
#include "CAISystem.h"
#include "GoalOp.h"
#include "Graph.h"
#include "GraphStructures.h"
//#endif

//////////////////////////////////////////////////////////////////////////

class CAISystem; 
inline CAISystem *GetAISystem() {extern CAISystem *g_pAISystem; return g_pAISystem;}

//====================================================================
// SetAABBCornerPoints
//====================================================================
inline void SetAABBCornerPoints(const AABB& b, Vec3* pts)
{
	pts[0].Set(b.min.x, b.min.y, b.min.z);
	pts[1].Set(b.max.x, b.min.y, b.min.z);
	pts[2].Set(b.max.x, b.max.y, b.min.z);
	pts[3].Set(b.min.x, b.max.y, b.min.z);

	pts[4].Set(b.min.x, b.min.y, b.max.z);
	pts[5].Set(b.max.x, b.min.y, b.max.z);
	pts[6].Set(b.max.x, b.max.y, b.max.z);
	pts[7].Set(b.min.x, b.max.y, b.max.z);
}


inline float LinStep(float a, float b, float x)
{
	x = (x - a) / (b - a);
	if (x < 0.0f) x = 0;
	if (x > 1.0f) x = 1;
	return x;
}

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__81DAABA0_0054_42BF_8696_D99BA6832D03__INCLUDED_)

