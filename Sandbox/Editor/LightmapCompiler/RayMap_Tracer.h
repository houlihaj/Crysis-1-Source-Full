/*=============================================================================
RayMap_Tracer.h : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef RAYMAP_TRACER_H
#define RAYMAP_TRACER_H

#if _MSC_VER > 1000
# pragma once
#endif

#include "GeomCore.h"
#include "RayMap.h"
#include "RLWaitProgress.h"
#include "SceneContext.h"

//===================================================================================
// Class				:	CPhotonTrace
// Description			:	This class implements ray
class CRayMap_Tracer
{
public:
	bool		Trace_Position( CRayMap* pRayMap, const Vec3 vPosition, CGeomCore* pGeomCore, const bool bIndirectOnly );

	bool		Trace_AllLigths(CRLWaitProgress* pWaitProgress, CRayMap* pRayMap, CGeomCore* pGeomCore );
};



#endif//RAYMAP_TRACER_H