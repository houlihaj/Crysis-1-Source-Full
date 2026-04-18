/*=============================================================================
OcclusionIntegrator.cpp : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#include "stdafx.h"

#include "OcclusionIntegrator.h"
#include "IShader.h"
#include "AreaLight.h"

//===================================================================================
COcclusionIntegrator::COcclusionIntegrator() 
{
}

//===================================================================================
// Method				:	Illuminance
// Description	:	for a given position and axis give back the occlusion for lights
//===================================================================================
bool	COcclusionIntegrator::Illuminance( CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache )
{
	Vec3 vAxis(0.0f, 0.0f, 1.0f);
	return Illuminance(spectrum, vPosition, vAxis, (gf_PI/2.0f), LightIDs, pIntersectionCache );
}

//===================================================================================
// Method				:	Init
// Description	:	initialize the parameters.
//===================================================================================
void COcclusionIntegrator::Init()
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	pListLights	=	sceneCtx.GetLights();
	assert(pListLights!=NULL);
};

//===================================================================================
// Method				:	Illuminance
// Description	:	for a given position and axis give back the occlusion for lights
//===================================================================================
bool	COcclusionIntegrator::Illuminance( CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache )
{
	float fVisibility;
	int32	nLightNumber = LightIDs->size();

	assert( nLightNumber <= 16 );
	nLightNumber =  ( nLightNumber > 16 ) ? 16 : nLightNumber;

	bool bHaveRealLightInfo = false;

	for(int32 i=0; i<nLightNumber; ++i)
	{
		CDLight* pLight	=	pListLights->GetAt( (*LightIDs)[i].second );
		if( CAreaLight::GetVisibility( pLight, vPosition, vAxis, fVisibility, pIntersectionCache ) )
		{
			spectrum.m_vLightFlux[i/4][i%4] += fVisibility;
			bHaveRealLightInfo = true;
		}
	}

	return bHaveRealLightInfo;
}
