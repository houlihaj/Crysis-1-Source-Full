/*=============================================================================
OcclusionIntegrator.h : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef __OCCLUSIONINTEGRATOR_H__
#define __OCCLUSIONINTEGRATOR_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IlluminanceIntegrator.h"
#include "SceneContext.h"

#include "CryArray.h"

class	COcclusionIntegrator : public CIlluminanceIntegrator
{
public:
	COcclusionIntegrator ();
	virtual	~COcclusionIntegrator()
	{
	};

	void Init();

	//Illuminance statements
	//fix: replace return value by multu-samplies color
	bool Illuminance(CIllumHandler&, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache );
	bool Illuminance(CIllumHandler&, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache );

protected:
	const PodArray<CDLight*>*	pListLights;
};

#endif