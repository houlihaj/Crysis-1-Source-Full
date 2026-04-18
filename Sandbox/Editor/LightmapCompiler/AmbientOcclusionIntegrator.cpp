/*=============================================================================
AmbientOcclusionIntegrator.cpp : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#include "stdafx.h"

#include "AmbientOcclusionIntegrator.h"
#include "IShader.h"

//===================================================================================
// Method				:	Illuminance
// Description	:	for a given position and axis give back the ambient occlusion
//===================================================================================
bool	CAmbientOcclusionIntegrator::Illuminance( CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache)
{
	Vec3 vAxis(0.0f, 0.0f, 1.0f);
	return Illuminance(spectrum, vPosition, vAxis, (gf_PI/2.0f), LightIDs, pIntersectionCache );
}

//===================================================================================
// Method				:	Init
// Description	:	initialize the parameters.
//===================================================================================
void CAmbientOcclusionIntegrator::Init()
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	//Obtain configuraion every call from SceneCtx
	numSamples = sceneCtx.m_numAmbientOcclusionSamples;

	fMaxDistance = sceneCtx.m_fMaxAmbientOcclusionSearchRadius;
	fMinDistance = sceneCtx.m_fMinAmbientOcclusionSearchRadius;

	pGeomCore =	sceneCtx.GetGeomCore();
	assert(pGeomCore!=NULL);

//*
	iM1 = (int32)(f32)sqrt_tpl((f32)numSamples);
	iM1 = iM1 > 0 ? iM1 : 1;
	iM2 = numSamples / iM1;
	iM2 = iM2 > 0 ? iM2 : 1;
/*/
	iM1 = 4;
	iM2 = 8;
//*/
	fOnePerNumSamples = 1.f / (f32) (iM1*iM2);
	fDivider = fOnePerNumSamples / (fMaxDistance-fMinDistance);

	m_pIntersectionCache.Init(10);
}

//===================================================================================
// Method				:	Illuminance
// Description	:	for a given position and axis give back the ambient occlusion
//===================================================================================
bool	CAmbientOcclusionIntegrator::Illuminance( CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache )
{
	if( 0 == numSamples ) 
		return false;

	const Vec3*	pPositions	=	&vPosition;
	uint	PositionCounter		=	0;
	const uint POSITIONCOUNT=	static_cast<uint>(angle);
	CRay	ray;

	Matrix33 m33 = Matrix33::CreateOrthogonalBase(vAxis);
	Vec3 vTangent(m33.GetColumn1() );
	Vec3 vBinromal( m33.GetColumn2() );
	ray.vOrigNormal = vAxis;

	float fMaxDistanceBig = fMaxDistance*15;
	float fAverageDistance = 0.f;
	float fFullAO = 0.f;
//	Vec3 vUnoccludedDir(0,0,0);

	f32 fPhi,fTheta, fX, fY;
	float fCoss = 0;

	for( int32 i = 0; i < iM1; ++i )
		for( int32 j = 0; j < iM2; ++j )
	{
		ray.vFrom = pPositions[PositionCounter++%POSITIONCOUNT];
		fX = (f32(i)+0.499f) / (f32)(iM1);
		fY = (f32(j)+0.499f) / (f32)(iM2);
//		fY = (f32(i) + 0.35f + 0.3f*(f32)rand() /(f32)(RAND_MAX+1) ) / (f32)(iM1);
//		fX = (f32(j) + 0.35f + 0.3f*(f32)rand() /(f32)(RAND_MAX+1) ) / (f32)(iM2);

//		fTheta = asinf(sqrt_tpl( fY ));
//		fPhi = gf_PI2*fX;
		CreateUniformDistribution( fX, fY, fTheta, fPhi );
		float fCosS = cosf(fTheta);//1;//fOnePerNumSamples * fThetaCos;

		f32 fThetaSin = sinf(fTheta);
		ray.vDir	= Vec3( vTangent * fThetaSin * cosf( fPhi ) );
		ray.vDir	+= vBinromal * fThetaSin * sinf( fPhi );
		ray.vDir	+= vAxis * fCosS;
		assert(vAxis*vAxis>=0.99f && vAxis*vAxis<=1.01f);
		assert(ray.vDir*ray.vDir>=0.99f && ray.vDir*ray.vDir<=1.01f);

		ray.fT		=	fMaxDistance;
/*
		// Position debugging
		float fB = (vPosition.x+vPosition.y+vPosition.z);
		fFullAO += fB - floor(fB);
/*/
//		if( !pGeomCore->FirstIntersect(&ray,pIntersectionCache,m_pBoundingCell) )
//		if( pGeomCore->FirstIntersect(&ray,pIntersectionCache,NULL) )
		if(pGeomCore->FirstIntersect_Improved(&ray,pIntersectionCache))
		{
//			fFullAO += ray.fT > fMaxDistance ? ( fCosS*(1.f-0.5f*ray.fT/250.f)) : 0.f;
				Vec3 Nrm	=	ray.HitNormal();
				Nrm.Normalize();
				fFullAO += ( max(ray.vDir*Nrm*-1.f,0.f)* fCosS*ray.fT/fMaxDistance);
		}
		else
			fFullAO += fCosS;

		fCoss += fCosS;
	}

	//Here is the secret spice :) -> we need the "direction" encoded in the RAE even when the surround is the same,
	Vec3 vAbsAxis( vAxis.abs() );
	float fSpice = vAbsAxis.Dot( Vec3(0.33f,0.24f,0.36f) ) / vAbsAxis.Dot( Vec3(1,1,1) );

	spectrum.m_vLightFlux[0].y += fFullAO / fCoss;
	spectrum.m_vLightFlux[0].x	=	0.f;//spectrum.m_vLightFlux[0].y	+	1.f/32.f;	//  add 1/3 bit
	spectrum.m_vLightFlux[0].z	=	0.f;//spectrum.m_vLightFlux[0].y	-	1.f/32.f; //  sub 1/3 bit
	spectrum.m_vLightFlux[0].w	=	1.f;//
//	spectrum.m_vLightFlux[0][0] =(f32)rand() /(f32)RAND_MAX;
//	spectrum.m_vLightFlux[0][1] += 0fAverageDistance/fMaxDistanceBig / fCoss;

	return true;
}
