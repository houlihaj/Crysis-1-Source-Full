/*=============================================================================
  PhotonTrace.cpp : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan

=============================================================================*/
#include "stdafx.h"

#include "SceneContext.h"
#include "PhotonMap.h"
#include "PhotonTrace.h"

//===================================================================================
// Method				:	CPhotonTrace
// Description			:	Constructor
CPhotonTrace::CPhotonTrace(uint32 numEmitPhotons, uint32 nTessNum)
{

	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	//FIX:: include ranges check
	m_pGeomCore = sceneCtx.GetGeomCore();
	assert(m_pGeomCore!=NULL);

	m_pGlobalMap	=	sceneCtx.m_pGlobalMap;
	assert(m_pGlobalMap!=NULL);

	m_pCausticMap	=	sceneCtx.m_pCausticMap;
	assert(m_pCausticMap!=NULL);

	m_nMaxDiffuseDepth = sceneCtx.m_nMaxDiffuseDepth;

	//it's varing per light source 
	m_numEmitPhotons = numEmitPhotons;
	m_nTessNum = nTessNum;
}

//===================================================================================
// Method				:	CPhotonTrace
// Description			:	Destructor
//===================================================================================
CPhotonTrace::~CPhotonTrace() 
{
	/*//CPhotonMap	*cMap;

	// Balance the maps that have been modified
	while((cMap = balanceList.pop()) != NULL) 
	{
		cMap->modifying	=	FALSE;
		cMap->write(world);
	}*/
}

//===================================================================================
// Class				:	CPhotonTrace
// Method				:	Shade
// Description		:	Trace all the photons to the scene
//===================================================================================
void	CPhotonTrace::Shade(CRLWaitProgress* pWaitProgress)
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	uint32 nEmit;
	
	Vec3 vDir;

	//for WaitProgress representation
	uint32 nTotalPhotons, nTotalEmitedPhotons;

	// Compute the world bounding sphere
	m_vWorldCenter = m_pGeomCore->GetBBox()->GetCenter();
	m_fWorldRadius	=	((m_pGeomCore->GetBBox()->max) - m_vWorldCenter).GetLength();

	m_stage	=	PHOTON_TRACE;

	//process sun light
	pWaitProgress->Caption("Photons Tracing from Sun...");

	if(sceneCtx.m_bUseSunLight)
	{

		nEmit	=	sceneCtx.m_numSunPhotons;

		nTotalPhotons = nEmit;
		nTotalEmitedPhotons = 0;

		if (nEmit > 0) 
		{
			m_fPhotonPower	=	1.0 / (float) nEmit;

			f32* pfTheta = NULL;
			Vec3* vDir = NULL;

			Vec3& vSunColor = sceneCtx.m_vSunColor;
			Vec3& vSunDir = sceneCtx.m_vSunDir;

			while(nEmit > 0)
			{
				m_numVertices	=	min(m_nTessNum,nEmit); //for future area light support
				SolarBegin(vSunDir, NULL, vSunColor);
				nEmit		-=	m_numVertices;

				//wait progress
				nTotalEmitedPhotons += m_numVertices; 
				pWaitProgress->SetProgress((f32)nTotalEmitedPhotons/(f32)nTotalPhotons);

				//SolarEnd();
			}

		}
	}

	const PodArray<CDLight*>* pListLights = sceneCtx.m_pLstLightSources;
	uint32	numAllLights	=	pListLights->Count();
	uint32	numLights	=	0;

	//search the lightmap enabled lights
	for(int32 i=0; i<numAllLights; ++i)
	{
		CDLight* cLight	=	pListLights->GetAt(i);
		assert(cLight!=NULL);

		//don't want to be the part of the lightmap
		if( !(cLight->m_Flags & DLF_LM ) || cLight->m_Flags & DLF_SUN )
			continue;

		++numLights;
	}


	//for WaitProgress representation
	nTotalPhotons = numLights * m_numEmitPhotons;
	nTotalEmitedPhotons = 0;

	//process light sources
	pWaitProgress->Caption("Photons Tracing from LightSources...");
	for(int32 i=0; i<numAllLights; ++i)
	{
		CDLight* cLight	=	pListLights->GetAt(i);
		assert(cLight!=NULL);

		//don't want to be the part of the lightmap, or sun (sun has an own path)
		if( !(cLight->m_Flags & DLF_LM ) || cLight->m_Flags & DLF_SUN )
			continue;


		nEmit	=	m_numEmitPhotons;

		if (nEmit > 0) 
		{
			m_fPhotonPower	=	1.0 / (float) nEmit;

			f32* pfTheta = NULL;
			Vec3* vDir = NULL;

			if (cLight->m_Flags & DLF_PROJECT)
			{
				//FIX:: hack - fix after refactoring
				GetIEditor()->GetRenderer()->EF_UpdateDLight(cLight);
				pfTheta =  &(cLight->m_fBaseLightFrustumAngle);
				static Vec3 Z(0,0,1);
				vDir = &Z; //&(cLight->m_Orientation.m_vForward);
			}

			//hack
			GetIEditor()->GetRenderer()->EF_UpdateDLight(cLight);

			//hack
			ColorF tmpColor;
			tmpColor = cLight->m_Color;
			
			tmpColor *= cLight->m_fRadius; //handle intesivity !

			while(nEmit > 0) 
			{
				m_numVertices	=	min(m_nTessNum,nEmit); //for future area light support

				//vDir = cLight->m_m
				// Execute the light source shader
				IlluminateBegin(cLight->m_BaseOrigin, tmpColor, NULL, NULL);
//				ShadowBegin(cLight->m_BaseOrigin, NULL, NULL );

				//fix:: add here execution of Light Source shader
				nEmit		-=	m_numVertices;

				//wait progress
				nTotalEmitedPhotons += m_numVertices; 
				pWaitProgress->SetProgress((f32)nTotalEmitedPhotons/(f32)nTotalPhotons);

				IlluminateEnd();
//				ShadowEnd();
			}
		}
	}
}

//===================================================================================
// Class				:	CPhotonTrace
// Method				:	SolarBegin
// Description		:	Solar begin hook
// Fix: use pTehra to specify three-dimensional cone for distan light source
//===================================================================================
void	CPhotonTrace::SolarBegin(const Vec3& vL,const float *pTheta, Vec3& vSunColor) 
{
	Vec3	vShadeL;	//light direction for shading
	Vec3	vP;				//position
	Vec3	vCl;			//light color
  f32	fDX,fDY;		//offsets

	Vec3	cX;
	f64	fR;
	f64	fTheta;
	Vec3	vX,vY(1,2,3);

	m_fPowerScale	=	gf_PI* m_fWorldRadius*m_fWorldRadius;

	vX = vL.Cross(vY); //calc any vector which is perpendicular to the Light Direction
	vY = vX.Cross(vL);
	vX.Normalize();
	vY.Normalize();
	vX *= m_fWorldRadius;
	vY *= m_fWorldRadius;

	// Generate the solar samples
	for (uint32 i = 0; i< m_numVertices; i++) 
	{
		fDX	=	rnd();
		fDY	=	rnd();
		fR		=	sqrt(fDX);
		fTheta	=	fDY * 2.0 * gf_PI;

		cX = vX * ((f32) (fR*cos(fTheta)));
		vP = vY * ((f32) (fR*sin(fTheta)));
		vP += cX;
		vP += m_vWorldCenter;
		vShadeL = vL.GetNormalized();

		//////////////////////////////////////////////////////////////////////////
		//should be in SolarEnd() call
		//////////////////////////////////////////////////////////////////////////
		
		vCl = vSunColor;
		vP -= vShadeL * m_fWorldRadius;
		vCl *= (m_fPowerScale*m_fPhotonPower);

		TracePhoton(vP,vShadeL,vCl);
		//////////////////////////////////////////////////////////////////////////
	}
   
}

//===================================================================================
// Class				:	CPhotonTrace
// Method				:	SolarEnd
// Description		:	Solar end hook
//===================================================================================
void	CPhotonTrace::SolarEnd()
{
	if (m_stage == PHOTON_ESTIMATE)
	{
		// Estimation
		//const Vec3&	vCl = vSunColor;

		// Solar lights are easy, just do the averaging
		for (uint32 i = 0; i< m_numVertices; i++) 
		{
			//const float fPower	=	(vCl.x + vCl.y + vCl.z) / (f32) 3;

			//FIX:remove
			//fMinPower	=	min(fMinPower,fPower);
			//fMaxPower	=	max(fMaxPower,fPower);
			//fAvgPower	+=	fPower;
			//fNumPower++;
		}
	} else 
	{
		for (uint32 i = 0; i< m_numVertices; i++) 
		{
			//vCl = vSunColor;
			//vP -= vShadeL * m_fWorldRadius;
			//vCl *= (m_fPowerScale*m_fPhotonPower);
			//TracePhoton(Ps,vShadeL,vCl);
		}
	}
}

//===================================================================================
// Class				:	CPhotonTrace
// Method				:	IlluminateBegin
// Description		:	Illuminate begin hook
// FIX:: add separate shading for specular component of light
//===================================================================================
void	CPhotonTrace::IlluminateBegin(const Vec3& vP, const ColorF& vC,const Vec3* pvN, const float* pTheta)
{
	//Vec3	vShaderPs; // fix:: add varing parameter for tesselation
	Vec3	vShaderL;
	ColorF	vCl;
	
	if (pTheta == NULL) 
	{
		// Point light source
		m_fPowerScale		=	4*gf_PI;

		for ( uint32 i = 0; i< m_numVertices; i++) 
		{
			// Sample a direction in the unit sphere
			do 
			{

        vShaderL.SetRandomDirection();

			} while(vShaderL.Dot(vShaderL) > 1);

			// Compute the point being shaded
			vShaderL.Normalize();

			vCl = vC * m_fPowerScale*m_fPhotonPower;

			TracePhoton(vP,vShaderL,vCl);
		}
	} else 
	{
		// Area && directional light source
		m_fPowerScale		=	2*gf_PI;

		assert(pvN != NULL);

		for ( uint32 i = 0; i< m_numVertices; i++) 
		{
			// Sample a direction in the unit sphere
			SampleHemisphere(vShaderL,*pvN,*pTheta);
			// Compute the point being shaded	

			vShaderL.Normalize();

			vCl = vC * (m_fPowerScale*m_fPhotonPower);
			TracePhoton(vP,vShaderL,vCl);

		}
	}
}

//===================================================================================
// Class				:	CPhotonTrace
// Method				:	IlluminateEnd
// Description		:	Illuminate end hook
//===================================================================================
void		CPhotonTrace::IlluminateEnd()
{
	//fix:: do photon tracing ( use cache from IlluminateBegin)
}

void	CPhotonTrace::IlluminateTest(Vec3& vFrom, Vec3& vC)
{
		Vec3 vDir;

		for(uint32 nEmit	=	0; nEmit < m_numEmitPhotons; nEmit++)
		{
			vDir.SetRandomDirection();
			TracePhoton(vFrom, vDir, vC);
		}

		return;
}


//===================================================================================
// Class				:	CPhotonTrace
// Method				:	ShadowBegin
// Description		:	Shadow begin hook
//===================================================================================
void	CPhotonTrace::ShadowBegin(const Vec3& vP, const Vec3* pvN, const float* pTheta)
{
	Vec3	vShaderL;
	ColorF	vCl;

	if (pTheta == NULL) 
	{
		// Point light source
		m_fPowerScale		=	4*gf_PI;

		for ( uint32 i = 0; i< m_numVertices; i++) 
		{
			// Sample a direction in the unit sphere
			do 
			{
				vShaderL.SetRandomDirection();
			} while(vShaderL.Dot(vShaderL) > 1);

			// Compute the point being shaded
			vShaderL.Normalize();
			TraceShadowPhoton(vP,vShaderL );
		}
	} else 
	{
		// Area && directional light source
		m_fPowerScale		=	2*gf_PI;

		assert(pvN != NULL);

		for ( uint32 i = 0; i< m_numVertices; i++) 
		{
			// Sample a direction in the unit sphere
			SampleHemisphere(vShaderL,*pvN,*pTheta);
			// Compute the point being shaded	

			vShaderL.Normalize();
			TraceShadowPhoton(vP,vShaderL);
		}
	}
}

//===================================================================================
// Class				:	CPhotonTrace
// Method				:	ShadowEnd
// Description		:	Shadow end hook
//===================================================================================
void		CPhotonTrace::ShadowEnd()
{
}


//===================================================================================
// Method				:	TracePhoton
// Description	:	tracing of photon
//===================================================================================
void	CPhotonTrace::TracePhoton(const Vec3& vP,const Vec3& vL,const ColorF& vC)
{
	CRay				ray;
	Vec3				vCl,vPl,vNl;
	//CAttributes*	attributes;
 	//const float*	pfSurfaceColor;
	int32					numDiffuseBounces, numSpecularBounces;
	bool					bLastBounceSpecular;

	// check L for unit vector (dot=1)
	assert( ((vL.Dot(vL)) - 1.0f)*((vL.Dot(vL)) - 1.0f) < 0.00001f ); //fix: replace 

	ray.vFrom = vP; //position
	ray.vDir = vL; //light direction
//	ray.vTo = vP + vL;	//position + light direction

	vCl.x = vC.r; //photon radiance
	vCl.y = vC.g;
	vCl.z = vC.b;

	//number of produced diffuse and specular bounces
	numDiffuseBounces		=	0;
	numSpecularBounces		=	0;
	bLastBounceSpecular		=	false;

	// The intersection bias
	//should be specified separate for translucent tracing
	ray.fTmin				=	0;
	ray.fT			=	 CGeomCore::m_fInf;

processBounce:;

	// Set initial ray's parameters
	//ray.CalcInvDir();
	ray.fT			=	 CGeomCore::m_fInf;
	//ray.object		=	NULL;

	// Trace the ray in the scene
	bool bFound =  m_pGeomCore->Intersect(&ray);

	if (bFound)
	{
		//init intersected primitive's material
		IRenderShaderResources* pMaterial	=	ray.pMaterial;


		//FIX !!!!!!!!!
		//if (attributes->flags & ATTRIBUTES_FLAGS_ILLUMINATE_FRONT_ONLY) { // fix: implement diff tracing models
		if ( ray.vN.Dot( ray.vDir ) > 0.0f )
			return;
		//}

		vPl = ray.vP;

		//second algorithm
		//vPl = ray.vDir * ray.fT;
		//vPl += ray.vFrom;

		//implement different shading models
		//switch(shadingModel) {
		//+++++++++ process materials
		// Matte surface

		// Get the normalized normal vector at the intersection
		vNl = ray.vN;
		if ((ray.vDir).Dot(vNl) > 0.0f) 		//FIX !!!!!!!!!!!
		{
			vNl *= -1.0f;
		}

		//////////////////////////////////////////////////////////////////////////
		// Save the photon
		if (m_pGlobalMap->m_bModifying == false) 
		{
			m_pGlobalMap->m_bModifying	=	true;
			m_pGlobalMap->Reset();
			//balanceList.push(globalMap);
		}

		bool bFinalRegatharing = CSceneContext::GetInstance().m_bFinalRegatharing;
    if (numDiffuseBounces>0 || bFinalRegatharing) //store indirect light only if Final Regatharing is Disabled
		{
			m_pGlobalMap->Store( vPl, vNl, ray.vDir, vCl );
		}

		//////////////////////////////////////////////////////////////////////////
		//	Save the caustic's photon
		if (bLastBounceSpecular)
		{
			if (m_pCausticMap->m_bModifying == false) 
			{
				m_pCausticMap->m_bModifying	=	TRUE;
				m_pCausticMap->Reset();
				//balanceList.push(causticMap);
			}
	
			m_pCausticMap->Store(vPl, vNl, ray.vDir, vCl); //store direct light also

			return; // we don't need another tracing because of mate surface 
		}

		// Check if we hit the maximum number of bounces
		if (numDiffuseBounces >= m_nMaxDiffuseDepth) 
			return;
		numDiffuseBounces++;

		// Sample the reflection direction
		//Fix:: Here could be the problem with proper random generator
		SampleCosineHemisphere(ray.vDir,vNl,(float) (gf_PI / 2.0f));//fix: use not only phong lobe
		ray.vDir.Normalize();

		assert(vNl.Dot(ray.vDir) > 0.0f); //fix
		
		if( pMaterial )
		{
			//Get material color
			ColorF mtlDiffuse = pMaterial->GetDiffuseColor();
			SEfResTexture* mtlDiffuseTex = pMaterial->GetTexture(EFTT_DIFFUSE);

			//apply reflectance for photon's bounce
			assert(mtlDiffuse.r<=1.0f && mtlDiffuse.g<=1.0f && mtlDiffuse.b<=1.0f);

			//mtlDiffuse.Clamp();
			vCl.x *= mtlDiffuse.r;
			vCl.y *= mtlDiffuse.g;
			vCl.z *= mtlDiffuse.b;

			if (mtlDiffuseTex!= NULL) //multiply by avg color of diffuse texture 
			{
				ColorF AvgColor= mtlDiffuseTex->m_Sampler.m_pITex->GetAvgColor();
				vCl.x *= AvgColor.r;
				vCl.y *= AvgColor.g;
				vCl.z *= AvgColor.b;
			}
		}

		// Process the current hit
		ray.vFrom = vPl; 
		ray.fTmin	=	0;
		ray.fT		=	 CGeomCore::m_fInf;
		bLastBounceSpecular		=	false;

		goto processBounce;

	}
}


//===================================================================================
// Method				:	TraceShadowPhoton
// Description	:	tracing of shadow photon
//===================================================================================
void	CPhotonTrace::TraceShadowPhoton(const Vec3& vP,const Vec3& vL)
{
	CRay				ray;
	short				nFlag = 0x10;									//0 = direct photon, 1 = shadow photon, 2 = penumbra

	// check L for unit vector (dot=1)
	assert( ((vL.Dot(vL)) - 1.0f)*((vL.Dot(vL)) - 1.0f) < 0.00001f ); //fix: replace 

	// Set initial ray's parameters
	ray.vFrom = vP; //position
	ray.vDir = vL; //light direction
	ray.fT			=	 CGeomCore::m_fInf;
	// The intersection bias
	//should be specified separate for translucent tracing
	ray.fTmin				=	CSceneContext::GetInstance().m_fShadowBias;

	//////////////////////////////////////////////////////////////////////////
	// Save the photon
	if (m_pGlobalMap->m_bModifying == false) 
	{
		m_pGlobalMap->m_bModifying	=	true;
		m_pGlobalMap->Reset();
		//balanceList.push(globalMap);
	}

	// Trace the ray in the scene
	bool bFound =  m_pGeomCore->Intersect(&ray);
	while( bFound )
	{
		m_pGlobalMap->Store( ray.vP, ray.vN, ray.vDir, Vec3(0,0,0), nFlag );

		//from the first time we store shadow photons..
		nFlag = 0x20;

		// Process the current hit - we don't change the direction...
		ray.fT			=	CGeomCore::m_fInf;
		ray.fTmin		=	0;
		bFound			= m_pGeomCore->Intersect(&ray);
	}

}

//===================================================================================
// Method				:	SampleCosineHemisphere
// Description			:	Sample vectors cos-distributed in a hemisphere
// Fix	: Should be heavy Optimize by precached samples !
//===================================================================================
void	CPhotonTrace::SampleCosineHemisphere(Vec3& vR,const Vec3& vZ,const float theta) 
{
	Vec3			vP;
	f32			fPw;
	Vec3			vPo;
	f32				fCosa;
	f32				fSina;
	const f32		fCosmin	=	(float) cos(theta);

	while(true)
	{
		//optimize rand generator in this way
		//generator.Get(vP);
	
		vP.SetRandomDirection();

		// Sample a uniformly distributed point on a sphere
		if (vP.Dot(vP) < 1.0f)	
		{		
			fPw =  (float)rand() /(float)RAND_MAX;
			break;
		}
	}

	fCosa	=	 sqrt(fPw)*(1.0f - fCosmin) + fCosmin;
	fSina	=	 sqrt(1.0f - fCosa*fCosa);

	vPo = vP ^ vZ;
	// Po is unit length
	vPo.Normalize();

	// Construct the sample vector
	vR =(vZ * fCosa) + (vPo * fSina);
}

//===================================================================================
// Method				:	SampleHemisphere
// Description	:	Sample vectors distributed uniformly in a hemisphere
//===================================================================================
void	CPhotonTrace::SampleHemisphere(Vec3& vR,const Vec3& vZ,const float theta)
{
	Vec3			vP;
	f32			fPw;
	Vec3			vPo;
	f32				fCosa;
	f32				fSina;

	while(true)
	{
		//optimize rand generator in this way
		//generator.Get(vP);

		vP.SetRandomDirection();

		// Sample a uniformly distributed point on a sphere
		if (vP.Dot(vP) < 1.0f)	
		{		
			fPw =  (float)rand() /(float)RAND_MAX;
			break;
		}
	}

	fCosa	=	 1.0f - fPw * (1.0f - (f32) cos_tpl(theta));
	fSina	=	 sqrt_tpl(1.0f - fCosa*fCosa);

	vPo = vP ^ vZ;
	// Po is unit length
	vPo.Normalize();

	// Construct the sample vector
	vR =(vZ * fCosa) + (vPo * fSina);
}
