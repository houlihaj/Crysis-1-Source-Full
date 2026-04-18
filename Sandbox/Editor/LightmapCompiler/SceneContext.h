/*=============================================================================
  SceneContext.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/

#ifndef __SCENECONTEXT_H__
#define __SCENECONTEXT_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "RLSingleton.h"

#include "IShader.h"
#include "PhotonMap.h"
#include "GeomCore.h"

enum ELightmapMode
{
	LMMODE_PHOTONMAP_ONLY,
	LMMODE_DIRECT_PHOTONMAP,
	LMMODE_DIRECT_REGATHERING,
};

enum ELightmapQuality
{
	LMQUALITY_LOW,
	LMQUALITY_MID,
	LMQUALITY_HIGH,
};

//fix implement flags
const	uint32	FLAGS_ILLUMINATE_FRONT_ONLY	=	1 << 17;


//	Should be very close to the surface attribute definition(non-uniform designer-based approach) 
//	but for uniform global illumination computation now then we define it per scene
class CSceneContext : public RLSingleton<CSceneContext>
{
public: 
		CSceneContext();

		//Get singleton
		static CSceneContext& GetInstance();

		//CSceneContext(const CSceneContext*);
		virtual		~CSceneContext()	
		{};

		//Add or remove a lightsource from the environment
		void				AddLight(CDLight*)
		{};
		void				RemoveLight(CDLight*)
		{};

    void	SetLights(const PodArray<CDLight*>* pLstLights);
		void	SetGeomCore(CGeomCore* pGeomCore);

		const PodArray<CDLight*>*	GetLights();
		CGeomCore*	GetGeomCore() const;

		void				SetGlobalMap(CPhotonMap*);
		CPhotonMap*	GetGlobalMap();
		void				SetCausticMap(CPhotonMap*);
		CPhotonMap*	GetCausticMap();

		//Restore default parameters for scene context
		void RestoreDefault();
		void SetPredefinedState( int nStateID );

public:

		//rasterizaion mode paramiters
		int	m_eLightmapMode;			//lightmap gathering mode
		int m_eLightmapQuality;		//lightmap quality seting

		bool m_bUseSuperSampling;		//number of samples for supersampling 
		int m_numJitterSamples;		//number of samples for supersampling 

  	const PodArray<CDLight*>*	m_pLstLightSources;								// The list of active light sources

		CGeomCore*		m_pGeomCore;

		//Used photon maps
		string				m_globalMapName;						// Name of the global photon map
		string				m_causticMapName;						// Name of the caustic photon map
		CPhotonMap*		m_pGlobalMap;								// Global photon map
		CPhotonMap*		m_pCausticMap;							// Ccaustic photon map

		//Photon trace parameters
		int32				m_numEmitPhotons;							// The number of photons to emit from the light source
		int32				m_nMaxDiffuseDepth;						// The maximum number of diffuse bounces before going to the photon map
		int32				m_nMaxSpecularDepth;					// The maximum number of specular bounces
		int32				m_nShootStep;									// The step size for shooting rays for the attached object

		//Photon maps parameters
		int32				m_nPhotonEstimator;						// Estimation number of photon
		f32					m_fMaxPhotonSearchRadius;			// Max search radius
		f32					m_fMinPhotonSearchRadius;			// Min search radius

		//string		irradianceHandle;					// The irradiance cache
		//string		irradianceHandleMode;			// The irradiance cache mode
		//float			irradianceMaxError;					// The error threshold for the irradiance cache
		//float			irradianceMaxPixelDistance;	// The maximum pixel distance between the samples for irradiance caching

		//Ambient occlusion parameters
		int32				m_numAmbientOcclusionSamples;		// Then number of Ambient occlusion rays shooted
		float				m_fMinAmbientOcclusionSearchRadius;		// The minimum distance for AO.
		float				m_fMaxAmbientOcclusionSearchRadius;		// The minimum distance for AO.

		//Global illum params
		f32					m_fShadowBias;
		f32					m_fGridJitterBias;
		int32				m_numIndirectSamples;
		f32					m_fMaxBrightness;

		//Do the final regathering
		bool				m_bFinalRegatharing;

		//Area light source params
		int32	m_numDirectSamples;		// The number of samples for light source estimation
		//f32		m_fLightSrcRadius;	// The radius of the light for the spherical light source

		//Directional sun light
		bool	m_bUseSunLight;
		int32	m_numSunDirectSamples;	// The number of samples for direct sun light estimation
		Vec3	m_vSunPos;		// Constant sun position
		Vec3	m_vSunDir;		// Constant sun direction
		Vec3	m_vSunColor;		// Constant sun direction
		int32	m_numSunPhotons;	// The number of samples for direct sun light estimation

		//Rasterization parameters
		int32 m_nLightmapPageWidth;
		int32 m_nLightmapPageHeight;
		f32		m_fLumenPerUnitU;
		f32		m_fLumenPerUnitV;

		//fix: make separate statistic handler
		//statistics
		int32 m_numIndirectIlluminanceRays;
		int32 m_numIndirectIlluminanceSamples;
		int32 m_numIndirectDiffusePhotonmapLookups;

		//rasterization properties
		bool	m_bMakeRAE;				//Enable RAE reflection map generation
		bool	m_bMakeOcclMap;		//Enable occlusion map generation
		bool	m_bMakeLightMap;	//Enable light map generation
		int32 m_numComponents;  //number of components for rasterization
		int32 m_numRAEComponents; //number of components of RAE for rasterization

		int8	m_nSlidingChannelNumber;			//The number of channels which checked when try to optimize a surface - more == better packing & more rendering time
		int32	m_nMaxTrianglePerPass;				//The maximum triangle / rendering pass - try to mimimalize - but if you have a light with a big radius - that can be a problem too


		bool	m_bDistributedMap;					//Enable Distributed comiling map creation
		uint32 m_nDistributedBlockSize;		//Size of one pixel
};

#endif
