/*=============================================================================
  DirectIlluminance.cpp : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#include "stdafx.h"

#include "DirectIlluminance.h"
#include "IShader.h"
#include "AreaLight.h"

CDirectIlluminance::CDirectIlluminance() 
{
}

//===================================================================================
// Method				:	Illuminance
// Description	:	for a given position and axis give back the direct illumination of lights
//===================================================================================
bool	CDirectIlluminance::Illuminance( CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache )
{
	Vec3 vAxis(0.0f, 0.0f, 1.0f);
	return Illuminance(spectrum, vPosition, vAxis, (gf_PI/2.0f), LightIDs, pIntersectionCache );
}

bool	CDirectIlluminance::Illuminance( CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache )
{

	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	//Obtain configuraion every call from SceneCtx
	//m_numSamples = sceneCtx.m_numDirectSamples;
	//m_fLightSrcRadius = sceneCtx.m_fLightSrcRadius;

	//TODO: implement illumination hook for castom light shaders
	//HIDER_ILLUMINATIONHOOK
	//context->illuminateBegin(Pf,Nf,thetaf);
	//context->illuminateEnd();

	const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();
	assert(pListLights!=NULL);
	int32	nLightNumber = LightIDs->size();

	for(int32 i=0; i<nLightNumber; ++i)
	{
		CDLight* pLight	=	pListLights->GetAt( (*LightIDs)[i].second );
		assert(pLight!=NULL);
		CAreaLight::Shade( pLight, vPosition, vAxis, spectrum, pIntersectionCache );
	}

	return true;
}

void CDirectIlluminance::HCLambertShader(Vec3& spectrum, const Vec3& vLightDir, const Vec3& vNormal, ColorF& Cl)
{


  ColorF shadeRes(0.0f, 0.0f, 0.0f);

	//shadeRes+= (reflHC * Cl) * (vLightDir.Dot(vNormal));
	//need refactorimg ColorF !
	shadeRes += Cl;
	shadeRes *= vLightDir.Dot(vNormal);

	spectrum.x = shadeRes.r;
	spectrum.y = shadeRes.g;
	spectrum.z = shadeRes.b;
}