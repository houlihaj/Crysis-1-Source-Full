/*=============================================================================
  DirectIlluminance.cpp : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#include "stdafx.h"

#include "SunDirectIlluminance.h"
#include "IShader.h"

CSunDirectIlluminance::CSunDirectIlluminance() 
{
	//if we do not use SceneCtx then default parameters
}

//===================================================================================
// Method				:	Illuminance
// Description	:	for a given position and axis give back the direct illumination of sun
//===================================================================================
bool	CSunDirectIlluminance::Illuminance( CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache )
{
	Vec3 vAxis(0.0f, 0.0f, 1.0f);
	return Illuminance(spectrum, vPosition, vAxis, (gf_PI/2.0f), LightIDs, pIntersectionCache );
}

//===================================================================================
// Method				:	Illuminance
// Description	:	for a given position and axis give back the direct illumination of sun
//===================================================================================
bool	CSunDirectIlluminance::Illuminance( CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache )
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	Vec3 vP;
	CRay	ray;
	CGeomCore*	pGeomCore =	sceneCtx.GetGeomCore();

	vP = sceneCtx.m_vSunPos;

	ray.vFrom = vPosition;
//	ray.vTo = vPosition + (-sceneCtx.m_vSunDir); //FIX: hack
	ray.vDir = -sceneCtx.m_vSunDir;

	if ( ray.vDir.Dot(vAxis)<0) // cull back faces
		return true;

	ray.fTmin				=	CSceneContext::GetInstance().m_fShadowBias;

	ray.fT			=	 CGeomCore::m_fInf;


	if (!pGeomCore->FirstIntersect(&ray, pIntersectionCache))
	{
		Vec3 vLightDir = -ray.vDir;
		Vec3 vCl = sceneCtx.m_vSunColor;

		spectrum.Add(vCl, vLightDir);
	}

	return true;
}