/*=============================================================================
  DirectIlluminance.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#include "StdAfx.h"

#include "IndirectIlluminance.h"
#include "IShader.h"
#include "Cry_Math.h"

CIndirectIlluminance::CIndirectIlluminance() 
{
}


//===================================================================================
// Method				:	Illuminance
// Description	:	for a given position and axis give back the indirect illumination for lights
//===================================================================================
bool CIndirectIlluminance::Illuminance(CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache )
{
	return true;
}

//===================================================================================
// Method				:	Illuminance
// Description	:	for a given position and axis give back the indirect illumination for lights
//===================================================================================
bool CIndirectIlluminance::Illuminance(CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache )
{

	//fix: handle this params in future GI compiling
	Vec3 vEnvDir;
	f32 fCoverage;


	Sample(spectrum, vEnvDir, fCoverage, vPosition, vAxis);

	//fix in shader
	//spectrum *= fCoverage;

	return true;
}

void	CIndirectIlluminance::Sample(CIllumHandler& vColO,Vec3& vEnvDirO, f32& fCoverageO,
																	 const Vec3& vPosI,const Vec3& vNormI)
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	//CacheSample		*cSample;
	int32			i,j;
	int32			numSamples	=	sceneCtx.m_numIndirectSamples;
	f32				fCoverage;
	Vec3			vIrradiance;
	Vec3			vEnvDir;
	f32				rMean;
	f32				rMeanMin;
	CRay			ray;
	int32			nt,np;
	Vec3			vX,vY;

	//accumulate all the hemisphere samples for father approximation by irrad maps
	//CIlluminanceSample*	pSamples;

	//CCacheNode	*cNode;
	//int32			nDepth;

	CGeomCore*	pGeomCore =	sceneCtx.GetGeomCore();
	assert(pGeomCore!=NULL);

	//Calc number of samples
	nt					=	(int32) (sqrt(numSamples / g_PI) + 0.5);
	np					=	(int32) (g_PI*nt + 0.5);
	numSamples	=	nt*np;
	//pSamples	=	new CIlluminanceSample[numSamples];

	// Create an orthanormal coordinate system
	vX = vNormI.Cross(vPosI);
	vX.Normalize();
	vY = vNormI.Cross(vX);

	// Sample the hemisphere
	fCoverage	=	0;
	vIrradiance.Set(0,0,0);
	vEnvDir.Set(0,0,0);

	rMean		=	0;
	rMeanMin	=	CGeomCore::m_fInf;

	//save stats
	sceneCtx.m_numIndirectIlluminanceRays	+=	numSamples;
	sceneCtx.m_numIndirectIlluminanceSamples++;

	int32 nSampleCnt = 0; 
	for (i=0;i<nt;i++)
	{ 
		for (j=0;j<np;j++,nSampleCnt++)
		{
			//CIlluminanceSample* pCurSample = &pSamples[nSampleCnt];

			f32		fTmp				=	(f32) sqrt((i+FRand()) / (float) nt);
			const f32	phi			=	(f32) (2.0*g_PI*(j+FRand()) / (float) np);
			const f32	cosPhi	=	(f32) (cos(phi)*fTmp);
			const f32	sinPhi	=	(f32) (sin(phi)*fTmp);
			const f32	dx			=	(2.0*FRand()-1.0)*sceneCtx.m_fGridJitterBias;
			const f32	dy			=	(2.0*FRand()-1.0)*sceneCtx.m_fGridJitterBias;

			fTmp	=	(float) sqrt(1 - fTmp*fTmp);

			ray.vDir.x		=	vX.x*sinPhi + vY.x*cosPhi + vNormI.x*fTmp;
			ray.vDir.y		=	vX.y*sinPhi + vY.y*cosPhi + vNormI.y*fTmp;
			ray.vDir.z		=	vX.z*sinPhi + vY.z*cosPhi + vNormI.z*fTmp;

			ray.vFrom.x		=	vPosI.x + vX.x*dx + vY.x*dy;
			ray.vFrom.y		=	vPosI.y + vX.y*dx + vY.y*dy;
			ray.vFrom.z		=	vPosI.z + vX.z*dx + vY.z*dy;

			ray.fTmin	=	sceneCtx.m_fShadowBias;
			//ray.CalcInvDir();
			ray.fT		=	CGeomCore::m_fInf;//sceneCtx.maxDistance;

			bool bIsIntersect = pGeomCore->Intersect(&ray);

			if (bIsIntersect) 
			{
				Vec3 vP,vN,vC;
				CPhotonMap	*pGlobalMap;

				//init material
				IRenderShaderResources* pMaterial	=	ray.pMaterial;

				if ((pGlobalMap = sceneCtx.m_pGlobalMap) != NULL)
				{
					f32	fTmp;

					vN = ray.vN;
					//vP = (ray.vDir * ray.fT) + ray.vFrom;
					vP = ray.vP;

					pGlobalMap->Lookup(&vC,vP,vN,sceneCtx.m_nPhotonEstimator);

					// HACK: Avoid too bright spots
					fTmp	=	max(max(vC.x,vC.y),vC.z);
					if (fTmp > sceneCtx.m_fMaxBrightness)	
						vC *= ((sceneCtx.m_fMaxBrightness)/fTmp);
					
					//Get material color
					ColorF mtlDiffuse = pMaterial->GetDiffuseColor();

					//apply reflectance
					vC.x *= mtlDiffuse.r;
					vC.y *= mtlDiffuse.g;
					vC.z *= mtlDiffuse.b;

//fix:remove it
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*					vP-=ray.vFrom;
					f32 fIntensity = 30.0f;
					f32 fVisibility	=	fIntensity / vP.Dot(vP); //fix: attenuation function

					Vec3 vCl =	vC * fVisibility;
					//will be replaced by flux handlers
					Vec3 lambColor;
					HCLambertShader(lambColor, ray.vDir, vNormI, vCl);
					//accumulate radiance

					vIrradiance += lambColor;
					pCurSample->vIrradiance = lambColor;*/
//--------------------------------------------------------------------------------

					vIrradiance += vC;
					//pCurSample->vIrradiance = vC;
					vColO.Add(vC*(1.0f/numSamples), -ray.vDir); //accomulate irradiance

          sceneCtx.m_numIndirectDiffusePhotonmapLookups++;
				} else 
				{
					//pCurSample->vIrradiance.Set(1.0f,1.0f,1.0f);
					vIrradiance += Vec3(1.0f, 1.0f, 1.0f);
				}

				// Yes
				fCoverage++;

				//pCurSample->fCoverage =	1;
				//pCurSample->vEnvDir.Set(0,0,0);
			} else 
			{
				// No
				vEnvDir += ray.vDir;

				//pCurSample->fCoverage	=	0;
				//pCurSample->vEnvDir,ray.vDir;

				////pCurSample->vIrradiance *= sceneCtx.backgroundColor;
				////pCurSample->vIrradiance *= sceneCtx.backgroundColor;
			}

			rMean						+=	1 / ray.fT;
			rMeanMin					=	min(rMeanMin,ray.fT);

			//pCurSample->fDepth			=	ray.fT;
			//pCurSample->fInvDepth		=	1 / ray.fT;
			//pCurSample->vDir = ray.vDir;

			//assert(pCurSample->fInvDepth > 0);
		}
	}

	// Normalize
	fCoverage	=	fCoverage / (f32) numSamples;
	vIrradiance *= (1 / (f32) numSamples);
	vEnvDir *= (1 / (f32) numSamples);

	if (vEnvDir.Dot(vEnvDir) > 0)	
		vEnvDir.Normalize();

	//fix: output it directly to the lum handlers
	//FIX - look for samples
	//delete [] pSamples;

	// Simple integrated output
	
	//vColO =	vIrradiance;
	fCoverageO	=	fCoverage;
	vEnvDirO =	vEnvDir;
}

void CIndirectIlluminance::HCLambertShader(Vec3& spectrum, const Vec3& vLightDir, const Vec3& vNormal, const Vec3& vCl)
{
	Vec3 shadeRes(0.0f, 0.0f, 0.0f);

	//shadeRes+= (reflHC * Cl) * (vLightDir.Dot(vNormal));
	//need refactorimg ColorF !
	shadeRes += vCl;
	shadeRes *= vLightDir.Dot(vNormal);

	spectrum = shadeRes;
}
