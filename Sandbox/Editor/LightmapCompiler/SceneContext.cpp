/*=============================================================================
  PhotonMap.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#include "stdafx.h"

#include "SceneContext.h"

template<> CSceneContext* RLSingleton<CSceneContext>::ms_pSingleton = NULL;

CSceneContext::CSceneContext()
{
	RestoreDefault();
}

void CSceneContext::RestoreDefault()
{
	//rasterizaion parameters
	m_eLightmapMode = LMMODE_DIRECT_PHOTONMAP;
	m_eLightmapQuality = LMQUALITY_MID;
	m_bUseSuperSampling = false;
	m_numJitterSamples = 0;	//number of samples for supersampling 

	m_pGeomCore = NULL;
	m_pGlobalMap = NULL;
	m_pCausticMap = NULL;

	//Photon trace parameters
	m_nMaxDiffuseDepth = 3;
	m_nMaxSpecularDepth = 3;
	m_numEmitPhotons = 5000;

	//Photon map's lookup estimator parameters
	m_nPhotonEstimator  = 400;
	m_fMaxPhotonSearchRadius = 0.00001f;		// Max search radius
	m_fMinPhotonSearchRadius = 0;						// Min search radius

	//ray-tracing parameters
	m_fShadowBias = 0.1f; //fix: should be less then OCT_EPSILON

	//indirect light parameters
	m_fGridJitterBias = 2.7f; //same as for shadow bias by far
	m_numIndirectSamples = 400;
	m_fMaxBrightness =3.0f;

	m_bFinalRegatharing = false;

	//direct light parameters
	m_numDirectSamples = 1;	// The number of samples for light source estimation

	//sun direct light parameters
	m_bUseSunLight = false;
	m_numSunDirectSamples = 2;	// The number of samples for direct sun light estimation
	m_numSunPhotons = 5000;

	//rasterization parameters
	m_nLightmapPageWidth = 1024;
	m_nLightmapPageHeight= 1024;
	m_fLumenPerUnitU = 8.0f;
	m_fLumenPerUnitV = 8.0f;

	//init statistics' parameters
	m_numIndirectIlluminanceRays = 0;
	m_numIndirectIlluminanceSamples = 0;
	m_numIndirectDiffusePhotonmapLookups = 0;

	m_bDistributedMap = false;
	m_bMakeRAE = false;
	m_bMakeOcclMap = false;
	m_bMakeLightMap = false;

	m_numAmbientOcclusionSamples = 0;
	m_fMinAmbientOcclusionSearchRadius = 0.01f;
	m_fMaxAmbientOcclusionSearchRadius = 1.5f;

	m_numComponents = 16;
	m_numRAEComponents = 1;

	m_nSlidingChannelNumber = 4;
	m_nMaxTrianglePerPass = 2500000;
}


void CSceneContext::SetPredefinedState( int nStateID )
{
	if( 	m_eLightmapQuality == nStateID )
		return;

	m_eLightmapQuality = nStateID;

	switch( nStateID )
	{
		case 0:
			m_nMaxDiffuseDepth = 1;
			m_nMaxSpecularDepth = 1;
			m_numEmitPhotons = 0;
			m_nPhotonEstimator  = 1;
			m_fMaxPhotonSearchRadius = 0.00001f;
			m_fMinPhotonSearchRadius = 0;
			m_numIndirectSamples = 1;
			m_numDirectSamples = 1;
			m_numSunDirectSamples = 3;
			m_numSunPhotons = 0;
			break;
		case 1:
			m_nMaxDiffuseDepth = 3;
			m_nMaxSpecularDepth = 3;
			m_numEmitPhotons = 5000;
			m_nPhotonEstimator  = 400;
			m_fMaxPhotonSearchRadius = 0.00001f;
			m_fMinPhotonSearchRadius = 0;
			m_numIndirectSamples = 400;
			m_numDirectSamples = 1;
			m_numSunDirectSamples = 2;
			m_numSunPhotons = 5000;
			break;
		case 2:
			m_nMaxDiffuseDepth = 5;
			m_nMaxSpecularDepth = 3;
			m_numEmitPhotons = 50000;
			m_nPhotonEstimator  = 800;
			m_fMaxPhotonSearchRadius = 0.00001f;
			m_fMinPhotonSearchRadius = 0;
			m_numIndirectSamples = 1024;
			m_numDirectSamples = 1;
			m_numSunDirectSamples = 3;
			m_numSunPhotons = 50000;
			break;
	}
}


CSceneContext& CSceneContext::GetInstance()
{
	assert( ms_pSingleton );
	return ( *ms_pSingleton );
}

void	CSceneContext::SetLights(const PodArray<CDLight*>* pLstLights)
{
	m_pLstLightSources = pLstLights;
};

void	CSceneContext::SetGeomCore(CGeomCore* pGeomCore)
{
	assert(pGeomCore!=NULL || pGeomCore == NULL );
	m_pGeomCore = pGeomCore;
};

const PodArray<CDLight*>*	CSceneContext::GetLights()
{
	assert(m_pLstLightSources!=NULL);
	return m_pLstLightSources;
};

CGeomCore*	CSceneContext::GetGeomCore() const
{
	assert(m_pGeomCore!=NULL);
	return m_pGeomCore;
};

void CSceneContext::SetGlobalMap(CPhotonMap* pGlobalMap)
{
	assert(pGlobalMap!=NULL);
	m_pGlobalMap = pGlobalMap;
	return;
};

CPhotonMap*	CSceneContext::GetGlobalMap()
{
	assert(m_pGlobalMap!=NULL);
	return m_pGlobalMap;
};

void CSceneContext::SetCausticMap(CPhotonMap* pCausticMap)
{
	assert(pCausticMap!=NULL);
	m_pCausticMap = pCausticMap;
	return;
};

CPhotonMap*	CSceneContext::GetCausticMap()
{
	assert(m_pCausticMap!=NULL);
	return m_pCausticMap;
};
