//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
//	
//	File:Renderer.cpp
//  Descriptin: Abstract renderer API
//
//	History:
//	-Feb 05,2001:Originally Created by Marco Corbetta
//	-: taken over by Andrey Honich
//.
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "Shadow_Renderer.h"
#include "IStatObj.h"
#include "I3DEngine.h"
#include "CREParticle.h"
#include "BitFiddling.h"															// IntegerLog2()	
#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
#include "CREParticleGPU.h"
#endif
#include "ImageExtensionHelper.h"											// CImageExtensionHelper
#include "Textures/Image/Squish/squish.h"							// CompressImage
#include "ResourceCompilerHelper.h"                   // CResourceCompilerHelper

#if defined(LINUX)
#include "ILog.h"
#endif

#include "3Dc/CompressorLib.h"

#ifndef NULL_RENDERER
# ifdef WIN32
#	 pragma message (">>> include lib: nvDXTlib")
#	  ifdef WIN64
#		  pragma comment ( lib, "nvDXTlibMTDLL.vc8.x64.lib" )
#	  else // WIN64
#		  pragma comment ( lib, "nvDXTlibMTDLL.vc8.lib" )
#	  endif // WIN64
# endif // WIN32
#endif // NULL_RENDERER
//
//////////////////////////////////////////////////////////////////////////
// Globals.
//////////////////////////////////////////////////////////////////////////
CRenderer *gRenDev = NULL;

#define EYEADAPTIONBASEDEFAULT		0.25f					// only needed for Crysis

#if defined(_LIB)
	extern int g_CpuFlags;
#else
	int g_CpuFlags;
#endif

#ifndef _XBOX
#include <CrtDebugStats.h>
#endif

//////////////////////////////////////////////////////////////////////////
// Pool allocators.
//////////////////////////////////////////////////////////////////////////
SDynTexture_PoolAlloc* g_pSDynTexture_PoolAlloc = 0;
//////////////////////////////////////////////////////////////////////////

// per-frame profilers: collect the infromation for each frame for 
// displaying statistics at the beginning of each frame
//#define PROFILER(ID,NAME) DEFINE_FRAME_PROFILER(ID,NAME)
//#include "FrameProfilers-list.h"
//#undef PROFILER

// Common render console variables
int CRenderer::CV_r_vsync;
int CRenderer::CV_r_stats;
int CRenderer::CV_r_log;
int CRenderer::CV_r_logTexStreaming;
int CRenderer::CV_r_logShaders;
int CRenderer::CV_r_logVBuffers;
int CRenderer::CV_r_flush;

int CRenderer::CV_r_fsaa;
int CRenderer::CV_r_fsaa_samples;
int CRenderer::CV_r_fsaa_quality;

int CRenderer::CV_r_atoc;

int CRenderer::CV_r_multigpu;
int CRenderer::CV_r_validateDraw;
int CRenderer::CV_r_profileDIPs;
int CRenderer::CV_r_occlusionqueries_mgpu;

int CRenderer::CV_r_texturesstreaming;
int CRenderer::CV_r_texturesstreamingnoupload;
int CRenderer::CV_r_texturesstreamingonlyvideo;
int CRenderer::CV_r_texturesstreamingsync;
int CRenderer::CV_r_texturesstreamingignore;
int CRenderer::CV_r_texturesstreampoolsize;
float CRenderer::CV_r_texturesstreamingmaxasync;
int CRenderer::CV_r_texturesmipbiasing;
int CRenderer::CV_r_texturesfilteringquality;

float CRenderer::CV_r_TextureLodDistanceRatio;
int CRenderer::CV_r_TextureLodMaxLod;

int CRenderer::CV_r_dyntexmaxsize;
int CRenderer::CV_r_dyntexatlascloudsmaxsize;
int CRenderer::CV_r_dyntexatlasspritesmaxsize;

int CRenderer::CV_r_texpostponeloading;
int CRenderer::CV_r_texatlassize;
int CRenderer::CV_r_texmaxanisotropy;
int CRenderer::CV_r_texmaxsize;
int CRenderer::CV_r_texminsize;
int CRenderer::CV_r_texbumpresolution;
int CRenderer::CV_r_texbumpheightmap;
int CRenderer::CV_r_texresolution;
int CRenderer::CV_r_texlmresolution;
int CRenderer::CV_r_texskyquality;
int CRenderer::CV_r_texskyresolution;
int CRenderer::CV_r_texnormalmaptype;
int CRenderer::CV_r_texhwmipsgeneration;
//int CRenderer::CV_r_texhwdxtcompression;
int CRenderer::CV_r_texgrid;
int CRenderer::CV_r_texlog;
int CRenderer::CV_r_texnoload;

int CRenderer::CV_r_debugrendermode;
int CRenderer::CV_r_debugrefraction;

#ifdef USE_HDR
int CRenderer::CV_r_hdrrendering;
int CRenderer::CV_r_hdrdebug;
#endif
int CRenderer::CV_r_hdrtype;
int CRenderer::CV_r_hdrallownonfp;
int CRenderer::CV_r_hdrhistogram;
float CRenderer::CV_r_hdrlevel;
float CRenderer::CV_r_hdrbrightoffset;
float CRenderer::CV_r_hdrbrightthreshold;

// eye adaption
float CRenderer::CV_r_eyeadaptationfactor;
float CRenderer::CV_r_eyeadaptationbase;
float CRenderer::CV_r_eyeadaptionmin;
float CRenderer::CV_r_eyeadaptionmax;
float CRenderer::CV_r_eyeadaptionscale;
float CRenderer::CV_r_eyeadaptionbase;
float CRenderer::CV_r_eyeadaptionspeed;
float CRenderer::CV_r_eyeadaptionclamp;		// todo:remove

int CRenderer::CV_r_geominstancing;
int CRenderer::CV_r_geominstancingthreshold;
int CRenderer::m_iGeomInstancingThreshold=0;		// 0 means not set yet

int CRenderer::CV_r_beams;
int CRenderer::CV_r_beamssoftclip;
int CRenderer::CV_r_beamshelpers;
int CRenderer::CV_r_beamsmaxslices;
float CRenderer::CV_r_beamsdistfactor;


int CRenderer::CV_r_LightVolumesDebug;

int CRenderer::CV_r_UseShadowsPool;

float CRenderer::CV_r_ShadowsSlopeScaleBias;
float CRenderer::CV_r_ShadowsBias;

int CRenderer::CV_r_ShadowGenMode;

int CRenderer::CV_r_shadowblur;
int CRenderer::CV_r_shadowtexformat;
int CRenderer::CV_r_ShadowsMaskResolution;
int CRenderer::CV_r_ShadowsMaskDownScale;
int CRenderer::CV_r_ShadowsDeferredMode;
int CRenderer::CV_r_ShadowsStencilPrePass;
int CRenderer::CV_r_ShadowsDepthBoundNV;
int CRenderer::CV_r_ShadowsForwardPass;
float CRenderer::CV_r_shadowbluriness;
float CRenderer::CV_r_shadow_jittering;
float CRenderer::CV_r_VarianceShadowMapBlurAmount;
int CRenderer::CV_r_ShadowsGridAligned;
int	CRenderer::CV_r_ShadowGenGS;
int	CRenderer::CV_r_ShadowPass;
int	CRenderer::CV_r_ShadowGen;
int CRenderer::CV_capture_misc_render_buffers;

int	CRenderer::CV_r_SSAO;
int	CRenderer::CV_r_SSAO_blur;
float CRenderer::CV_r_SSAO_blurriness;
float CRenderer::CV_r_SSAO_depth_range;
int	CRenderer::CV_r_SSAO_downscale_ztarget;
int	CRenderer::CV_r_SSAO_downscale_result_mask;
float CRenderer::CV_r_SSAO_amount;
float CRenderer::CV_r_SSAO_darkening;
int	CRenderer::CV_r_SSAO_quality;
float CRenderer::CV_r_SSAO_radius;
int	CRenderer::CV_r_TerrainAO;
int	CRenderer::CV_r_TerrainAO_FadeDist;
int	CRenderer::CV_r_FillLights;
int	CRenderer::CV_r_FillLightsMode;
int	CRenderer::CV_r_FillLightsDebug;

int CRenderer::CV_r_noloadtextures;
int CRenderer::CV_r_debuglights;
int CRenderer::CV_r_cullgeometryforlights;
int CRenderer::CV_r_lightssinglepass;
ICVar *CRenderer::CV_r_showlight;

int CRenderer::CV_r_impostersdraw;
float CRenderer::CV_r_imposterratio;
int CRenderer::CV_r_impostersupdateperframe;

int CRenderer::CV_r_nopreprocess;

int CRenderer::CV_r_shadersalwaysusecolors;
int CRenderer::CV_r_shadersuserfolder;
int CRenderer::CV_r_shadersdebug;
int CRenderer::CV_r_shadersignoreincludeschanging;
int CRenderer::CV_r_shaderspreactivate;
int CRenderer::CV_r_shadersintcompiler;
int CRenderer::CV_r_shadersasynccompiling;
int CRenderer::CV_r_shadersasyncmaxthreads;
int CRenderer::CV_r_shaderscacheoptimiselog;
int CRenderer::CV_r_shadersprecachealllights;
int CRenderer::CV_r_shadersstaticbranching;
int CRenderer::CV_r_shadersdynamicbranching;

int CRenderer::CV_r_optimisedlightsetup;

int CRenderer::CV_r_meshprecache;
int CRenderer::CV_r_meshshort;

int CRenderer::CV_r_usezpass;
int CRenderer::CV_r_usealphablend;
int CRenderer::CV_r_useedgeaa;
int CRenderer::CV_r_usehwskinning;
int CRenderer::CV_r_usemateriallayers;
int CRenderer::CV_r_usesoftparticles;
int CRenderer::CV_r_useparticles_refraction;
int CRenderer::CV_r_useparticles_glow;
int CRenderer::CV_r_usepom;

int CRenderer::CV_r_hair_sorting_quality;

int CRenderer::CV_r_motionblur;
int CRenderer::s_MotionBlurMode;				// potentially adjusted internal state of r_motionblur
float CRenderer::CV_r_motionblur_shutterspeed;
int CRenderer::CV_r_motionblurframetimescale;
int CRenderer::CV_r_motionblurdynamicquality;
float CRenderer::CV_r_motionblurdynamicquality_rotationacc_stiffness;  
float CRenderer::CV_r_motionblurdynamicquality_rotationthreshold;  
float CRenderer::CV_r_motionblurdynamicquality_translationthreshold;  
int CRenderer::CV_r_debug_motionblur;

int CRenderer::CV_r_debug_extra_scenetarget_fsaa;


int CRenderer::CV_r_customvisions;
int CRenderer::CV_r_rain;
float CRenderer::CV_r_rain_maxviewdist;

    
int CRenderer::CV_r_dof;

int CRenderer::CV_r_measureoverdraw;
float CRenderer::CV_r_measureoverdrawscale;
int CRenderer::CV_r_printmemoryleaks;
int CRenderer::CV_r_releaseallresourcesonexit;

int CRenderer::CV_r_debugscreenfx;  

int   CRenderer::CV_r_rc_autoinvoke;  

int CRenderer::CV_r_glow;
float CRenderer::CV_r_glow_fullscreen_threshold;  
float CRenderer::CV_r_glow_fullscreen_multiplier;  

int CRenderer::CV_r_refraction;
int CRenderer::CV_r_sunshafts; 
int CRenderer::CV_r_distant_rain;
int CRenderer::CV_r_nightvision;  

float CRenderer:: CV_r_glitterSize;  
float CRenderer:: CV_r_glitterVariation;  
float CRenderer:: CV_r_glitterSpecularPow;
int   CRenderer:: CV_r_glitterAmount;  

int CRenderer::CV_r_postprocess_effects;  
int CRenderer::CV_r_postprocess_effects_params_blending;
int CRenderer::CV_r_postprocess_effects_reset;
int CRenderer::CV_r_postprocess_effects_filters;
int CRenderer::CV_r_postprocess_effects_gamefx;
int CRenderer::CV_r_postprocess_profile_fillrate;

int CRenderer::CV_r_colorgrading;
int CRenderer::CV_r_colorgrading_selectivecolor;
int CRenderer::CV_r_colorgrading_levels;
int CRenderer::CV_r_colorgrading_filters;
int CRenderer::CV_r_colorgrading_dof;

int CRenderer::CV_r_cloudsupdatealways;
int CRenderer::CV_r_cloudsdebug;

int CRenderer::CV_r_showdyntextures;
ICVar *CRenderer::CV_r_showdyntexturefilter;
int CRenderer::CV_r_shownormals;
int CRenderer::CV_r_showlines;
float CRenderer::CV_r_normalslength;
int CRenderer::CV_r_showtangents;
int CRenderer::CV_r_showtimegraph;
int CRenderer::CV_r_showtextimegraph;
int CRenderer::CV_r_showlumhistogram;
int CRenderer::CV_r_graphstyle;
 
int CRenderer::CV_r_flares;

int CRenderer::CV_r_envlightcmdebug;
int CRenderer::CV_r_envcmwrite;
int CRenderer::CV_r_envcmresolution;  // 0-64,1-128,2-256
int CRenderer::CV_r_envtexresolution; // 0-64,1-128,2-256
//float CRenderer::CV_r_waterupdateFactor;
//float CRenderer::CV_r_waterupdateDistance;
//float CRenderer::CV_r_waterupdateRotation;
float CRenderer::CV_r_waterUpdateChange;
float CRenderer::CV_r_waterUpdateTimeMin;
float CRenderer::CV_r_waterUpdateTimeMax;
float CRenderer::CV_r_envlcmupdateinterval;
float CRenderer::CV_r_envcmupdateinterval;
float CRenderer::CV_r_envtexupdateinterval;
int CRenderer::CV_r_waterreflections;
int CRenderer::CV_r_waterreflections_mgpu;
int CRenderer::CV_r_waterreflections_quality;
int CRenderer::CV_r_waterreflections_use_min_offset;
float CRenderer::CV_r_waterreflections_min_visible_pixels_update;
float CRenderer::CV_r_waterreflections_minvis_updatefactormul;
float CRenderer::CV_r_waterreflections_minvis_updatedistancemul;
int CRenderer::CV_r_waterrefractions;
int CRenderer::CV_r_watercaustics;
int CRenderer::CV_r_water_godrays;
int CRenderer::CV_r_texture_anisotropic_level;
int CRenderer::CV_r_texnoaniso;

int CRenderer::CV_r_reflections;	
int CRenderer::CV_r_reflections_quality;	
float CRenderer::CV_r_waterreflections_offset;	

int CRenderer::CV_r_oceanrendtype;
int CRenderer::CV_r_oceanloddist;
int CRenderer::CV_r_oceantexupdate;
int CRenderer::CV_r_oceanmaxsplashes;
int CRenderer::CV_r_oceansectorsize;
int CRenderer::CV_r_oceanheightscale;
float CRenderer::CV_r_oceansplashscale;

int CRenderer::CV_r_reloadshaders;

int CRenderer::CV_r_cullbyclipplanes;
int CRenderer::CV_r_detailtextures;

int CRenderer::CV_r_detailnumlayers;
float CRenderer::CV_r_detailscale;
float CRenderer::CV_r_detaildistance;
int CRenderer::CV_r_texbindmode;
int CRenderer::CV_r_nodrawshaders;
int CRenderer::CV_r_nodrawnear;
int CRenderer::CV_r_drawnearfov;
int CRenderer::CV_r_profileshaders;
int CRenderer::CV_r_profileshaderssmooth;
ICVar *CRenderer::CV_r_excludeshader;
ICVar *CRenderer::CV_r_showonlyshader;
float CRenderer::CV_r_gamma;
float CRenderer::CV_r_contrast;
float CRenderer::CV_r_brightness;
int CRenderer::CV_r_nohwgamma;

int CRenderer::CV_r_scissor;

int CRenderer::CV_r_coronas;
float CRenderer::CV_r_coronafade;
float CRenderer::CV_r_coronasizescale;
float CRenderer::CV_r_coronacolorscale;

int CRenderer::CV_r_PolygonMode;
int CRenderer::CV_r_GetScreenShot;

int CRenderer::CV_r_character_nodeform;

int	CRenderer::CV_r_VegetationSpritesAlphaBlend;
int	CRenderer::CV_r_VegetationSpritesNoBend;
int CRenderer::CV_r_ZPassOnly;
int	CRenderer::CV_r_VegetationSpritesNoGen;
int	CRenderer::CV_r_VegetationSpritesTexRes;
int	CRenderer::CV_r_VegetationSpritesGenAlways;

int CRenderer::CV_r_ShowVideoMemoryStats;

int CRenderer::CV_r_ShowRenderTarget;
int CRenderer::CV_r_ShowRenderTarget_FullScreen;

int CRenderer::CV_r_ShowLightBounds;

int CRenderer::CV_r_MergeRenderChunksForDepth;
int CRenderer::CV_r_TextureCompressor;

int CRenderer::CV_r_RAM;
int CRenderer::CV_r_UseGSParticles;
int CRenderer::CV_r_Force3DcEmulation;

float CRenderer::CV_r_ZFightingDepthScale;
float CRenderer::CV_r_ZFightingExtrude;

//////////////////////////////////////////////////////////////////////

static void ShadersPrecache (IConsoleCmdArgs* Cmd)
{
  gRenDev->m_cEF.mfPrecacheShaders();
}

static void ShadersPrecacheList (IConsoleCmdArgs* Cmd)
{
  gRenDev->m_cEF.mfPrecacheShaders(NULL, true);
}

static void ShadersOptimise (IConsoleCmdArgs* Cmd)
{
  if (CRenderer::CV_r_shadersuserfolder)
  {
    string str = string("%USER%/") + string(gRenDev->m_cEF.m_ShadersCache);
    iLog->Log("Optimise user folder: '%s'", gRenDev->m_cEF.m_ShadersCache);
    gRenDev->m_cEF.mfOptimiseShaders(str.c_str());
  }
  iLog->Log("Optimise game folder: '%s'", gRenDev->m_cEF.m_ShadersCache);
  gRenDev->m_cEF.mfOptimiseShaders(gRenDev->m_cEF.m_ShadersCache);
}

static void ShadersMerge (IConsoleCmdArgs* Cmd)
{
  gRenDev->m_cEF.mfMergeShaders();
}

static void FixMaterials (IConsoleCmdArgs* Cmd)
{
  gRenDev->m_cEF.mfFixMaterials();
}

static void OnChange_CV_r_HDRRendering( ICVar* pCVar )
{
	ITimeOfDay* pTimeOfDay( gEnv->p3DEngine->GetTimeOfDay() );
	float time( pTimeOfDay->GetTime() );
	pTimeOfDay->SetTime( time, true );

	// FSAA requires HDR mode on
	// search for #LABEL_FSAA_HDR
	if(!pCVar->GetIVal())
	{
		// HDR was switched off
		ICVar *pFSAA = gEnv->pConsole->GetCVar("r_FSAA");

		if(pFSAA->GetIVal())
			pFSAA->Set(0);			// switch off FSAA
	}
}


static void OnTexturesMipBiasing( ICVar* pCVar )
{
	// adjusting this can cause severe texture cache miss performance problems
	assert(0);
	CryLog("Warning: r_TexturesMipBiasing was changed");
}


static void GetLogVBuffersStatic(ICVar* pCVar )
{
	gRenDev->GetLogVBuffers();
}

bool CRenderer::DoCompressedNormalmapEmulation() const
{
	bool b8KTexturesSupported = gRenDev->GetMaxTextureSize()>=8192;
	bool bNVidia = (gRenDev->GetFeatures()&RFT_HW_NVIDIA)!=0; 
	bool bDriverSupportsCompressedNormalmaps = m_iDeviceSupportsComprNormalmaps!=0;

	bool bEnableEmulation = !bDriverSupportsCompressedNormalmaps;

	if(GetRenderType()==eRT_DX9)				// only on DX9 we have hardware that can switch
	switch(CV_r_Force3DcEmulation)
	{
		case 0:	
			break;

		case 1:	
			bEnableEmulation = true;
			break;

		case 2:
			if(bNVidia && !b8KTexturesSupported)			// pre G80 hardware (NVidia, texture size) emulates 3Dc support in driver
				bEnableEmulation=true;
			break;

		default: assert(0);
	}

	return bEnableEmulation;
}


// required to apply changes in game
static void ChangeForce3DcEmulation( ICVar *pVar )
{
	assert(pVar);

	if(gEnv->pRenderer->GetRenderType()!=eRT_DX9)				// only on DX9 we have hardware that can switch
		return;

	CryLog("r_Force3DcEmulation trigger reloading of all textures");
	gEnv->pRenderer->EF_ReloadTextures();
}


void CRenderer::ChangeGeomInstancingThreshold( ICVar *pVar )
{
	// get user value
	m_iGeomInstancingThreshold = CV_r_geominstancingthreshold;

	// auto
	if(m_iGeomInstancingThreshold<=0)
	{
		int nGPU = gRenDev->GetFeatures() & RFT_HW_MASK;

		if(nGPU==RFT_HW_ATI)
			CRenderer::m_iGeomInstancingThreshold = 2;				// seems to help in performance on all cards
		 else if(nGPU==RFT_HW_NVIDIA || nGPU==RFT_HW_NV4X)
			CRenderer::m_iGeomInstancingThreshold = 8;				//
		 else
			CRenderer::m_iGeomInstancingThreshold = 7;				// might be a good start - can be tweaked further
	}

	CryLog(" used GeomInstancingThreshold is %d",m_iGeomInstancingThreshold);
}


CRenderer::CRenderer()
{ 
	if (!gRenDev)
    gRenDev = this;
  m_cEF.m_Bin.m_pCEF = &m_cEF;

#ifdef USE_HDR
  ICVar* pCV_r_HDRRendering = iConsole->Register("r_HDRRendering", &CV_r_hdrrendering, 2, VF_DUMPTODISK,
		"Toggles HDR rendering.\n"
		"Usage: r_HDRRendering [0/1]\n"
		"Default is 1 (on). Set to 0 to disable HDR rendering.");
	pCV_r_HDRRendering->SetOnChangeCallback( OnChange_CV_r_HDRRendering );
  iConsole->Register("r_HDRDebug", &CV_r_hdrdebug, 0, 0,
		"Toggles HDR debugging info (to debug HDR/eye adaptaion)\n"
    "Usage: r_HDRDebug [0/1/2]\n"
		"0 off (default)\n"
		"1 to show some internal HDR textures on the screen\n"
		"2 to identify illegal colors (grey=normal, red=NotANumber, green=negative)");
#endif
  iConsole->Register("r_HDRType", &CV_r_hdrtype, 1, 0,
    "Selects HDR type.\n"
    "Usage: r_HDRType [0/1/2]\n"
    "Default is 1 (OpenEXR). Set to 0 to disable.");
  iConsole->Register("r_HDRAllowNonFP", &CV_r_hdrallownonfp, 0, 0,
    "Selects HDR FP blending.\n"
    "Usage: r_HDRAllowNonFP [0/1]\n"
    "Default is 0. Set to 1 to enable.");
  iConsole->Register("r_HDRHistogram", &CV_r_hdrhistogram, 0, 0,
    "Toggles HDR luminance measuring using histogram.\n"
    "Usage: r_HDRHistogram [0/1/2/3]\n"
    "Default is 1 (min/avg/max), 2 (min is set to 0), 3 (linear adjust between 0 and max), 0 to diable");
  iConsole->Register("r_HDRLevel", &CV_r_hdrlevel, 0.6f, VF_DUMPTODISK,
		"HDR rendering level (bloom multiplier, tweak together with threshold)\n"
    "Usage: r_HDRLevel [Value]\n"
    "Default is 0.6");

	// Eye Adaption
	iConsole->Register("r_EyeAdaptationFactor", &CV_r_eyeadaptationfactor, 0.5f, VF_DUMPTODISK,
		"HDR rendering eye adaptation factor (0 means no adaption to current scene luminance, 1 means full adaption)\n"
		"Usage: r_HDREyeAdaptionFactor [Value]\n"
		"Default is 0.5");
	iConsole->Register("r_EyeAdaptationBase", &CV_r_eyeadaptationbase, EYEADAPTIONBASEDEFAULT, 0,
		"HDR rendering eye adaptation base value (smaller values result in brighter adaption)\n"
		"Usage: r_EyeAdaptationBase [Value]\n");

	iConsole->Register("r_EyeAdaptionMin", &CV_r_eyeadaptionmin, 0.1f, VF_DUMPTODISK,
    "HDR eye adaption property (0..128)\n"
    "Usage: r_EyeAdaptionMin [Value]\n"
    "Default is 0.1");
	iConsole->Register("r_EyeAdaptionMax", &CV_r_eyeadaptionmax, 4.0f, VF_DUMPTODISK,
		"HDR eye adaption property (0..128)\n"
    "Usage: r_EyeAdaptionMax [Value]\n"
    "Default is 4");
	iConsole->Register("r_EyeAdaptionBase", &CV_r_eyeadaptionbase, 0.0f, VF_DUMPTODISK,
		"HDR eye adaption property (0..1)\n"
    "Usage: r_EyeAdaptionBase [Value]\n"
    "Default is 0.0");
		// todo:remove,     moved to environment tab   
	iConsole->Register("r_EyeAdaptionClamp", &CV_r_eyeadaptionclamp, 4.0f, VF_DUMPTODISK,
		"HDR eye adaption property (0=full clamp .. ) unfinished feature - value around 3 is good, big values e.g. 10 to disable the clamp\n"
		"Usage: r_EyeAdaptionClamp [Value]\n"
		"Default is 4");

	iConsole->Register("r_EyeAdaptionScale", &CV_r_eyeadaptionscale, 1.0f, VF_DUMPTODISK,
		"HDR eye adaption property (0..1, to scale the final result)\n"
    "Usage: r_EyeAdaptionScale [Value]\n"
    "Default is 1.0");
	iConsole->Register("r_EyeAdaptionSpeed", &CV_r_eyeadaptionspeed, 50.0f, VF_DUMPTODISK,
		"HDR eye adaption property (percent of adaption per second)\n"
    "Usage: r_EyeAdaptionMax [Value]\n"
    "Default is 50");

  iConsole->Register("r_HDRBrightThreshold", &CV_r_hdrbrightthreshold, 3.0f, VF_DUMPTODISK,
		"HDR rendering bright threshold.\n"
		"Usage: r_HDRBrightThreshold [Value]\n"
		"Default is 3.0f");
  iConsole->Register("r_HDRBrightOffset", &CV_r_hdrbrightoffset, 6.0f, VF_DUMPTODISK,
		"HDR rendering bright offset.\n"
		"Usage: r_HDRBrightOffset [Value]\n"
		"Default is 6.0f");
  iConsole->Register("r_Beams", &CV_r_beams, 3, 0,
    "Toggles light beams.\n"
    "Usage: r_Beams [0/1/2/3]\n"
    "Default is 3 (optimized beams with glow support). Set to 0 to disable beams or 2 to\n"
		"use fake beams. Set 1 for real beams, full resolution (slower). Set to 3 to use\n"
		"optimized and with glow support beams.");
#if defined(DIRECT3D10)
  iConsole->Register("r_BeamsSoftClip", &CV_r_beamssoftclip, 1, 0,
    "Toggles light beams clip type.\n"
    "Usage: r_BeamsSoftClip [0/1]\n"
    "Default is 1 (software clip beams). Set to 0 to enable hardware clipping.");
#else
  iConsole->Register("r_BeamsSoftClip", &CV_r_beamssoftclip, 1, 0,
    "Toggles light beams clip type.\n"
    "Usage: r_BeamsSoftClip [0/1]\n"
    "Default is 1 (software clip beams). Set to 0 to enable hardware clipping.");
#endif
  iConsole->Register("r_BeamsHelpers", &CV_r_beamshelpers, 0, 0,
    "Toggles light beams helpers drawing.\n"
    "Usage: r_BeamsHelpers [0/1]\n"
    "Default is 0 (disabled helpers). Set to 1 to enable drawing helpers.");
  iConsole->Register("r_BeamsMaxSlices", &CV_r_beamsmaxslices, 200, 0,
    "Number of volumetric slices allowed per light beam.\n"
    "Usage: r_BeamsMaxSlices [1-300]\n"
    "Default is 200 (high-spec).");
  iConsole->Register("r_BeamsDistFactor", &CV_r_beamsdistfactor, 0.01f, 0,
    "Distance between slices.\n"
    "Usage: r_BeamsDistFactor [fValue]\n"
    "Default is 0.01 (0.01 meters between slices).");
  ICVar *pGeomInstThreshold = iConsole->Register("r_GeomInstancingThreshold", &CV_r_geominstancingthreshold, 0, 0,
    "If the instance count gets bigger than the specified value the instancing feature is used.\n"
    "Usage: r_GeomInstancingThreshold [Num]\n"
    "Default is 0 (automatic depending on hardware, used value can be found in the log)",ChangeGeomInstancingThreshold);

#if defined(XENON)
  iConsole->Register("r_GeomInstancing", &CV_r_geominstancing, 0, 0,
		"Toggles HW geometry instancing.\n"
		"Usage: r_GeomInstancing [0/1]\n"
		"Default is 1 (on). Set to 0 to disable geom. instancing.");
#else
#if defined (DIRECT3D9) || defined (OPENGL)
  iConsole->Register("r_GeomInstancing", &CV_r_geominstancing, 1, 0,
    "Toggles HW geometry instancing.\n"
    "Usage: r_GeomInstancing [0/1]\n"
    "Default is 1 (on). Set to 0 to disable geom. instancing.");
#elif defined (DIRECT3D10)
  iConsole->Register("r_GeomInstancing", &CV_r_geominstancing, 1, 0,
    "Toggles HW geometry instancing.\n"
    "Usage: r_GeomInstancing [0/1]\n"
    "Default is 1 (on). Set to 0 to disable geom. instancing.");
#endif
#endif
  iConsole->Register("r_ImpostersDraw", &CV_r_impostersdraw, 1, 0,
    "Toggles imposters drawing.\n"
    "Usage: r_ImpostersDraw [0/1]\n"
    "Default is 1 (on). Set to 0 to disable imposters.");
	iConsole->Register("r_ImposterRatio", &CV_r_imposterratio, 1, 0,
		"Allows to scale the texture resolution of imposters (clouds)\n"
		"Usage: r_ImposterRatio [1..]\n"
		"Default is 1 (1:1 normal). Bigger values can help to save texture space (e.g. value 2 results in 1/3 texture memory usage)");
  iConsole->Register("r_ImpostersUpdatePerFrame", &CV_r_impostersupdateperframe, 6000, 0,
    "How many kilobytes to update per-frame.\n"
    "Usage: r_ImpostersUpdatePerFrame [1000-30000]\n"
    "Default is 6000 (6 megabytes).");
  iConsole->Register("r_UseZPass", &CV_r_usezpass, 1, 0,
    "Toggles Z pass optimizations.\n"
    "Usage: r_UseZPass [0/1]\n"
    "Default is 1 (on). Set to 0 to disable Z-pass.");
	iConsole->Register("r_UseAlphaBlend", &CV_r_usealphablend, 1, 0,
		"Toggles alpha blended objects.\n"
		"Usage: r_UseAlphaBlend [0/1]\n"
		"Default is 1 (on). Set to 0 to disable all alpha blended object.");
	iConsole->Register("r_UseEdgeAA", &CV_r_useedgeaa, 1, 0,
		"Toggles edge blurring/antialiasing\n"
		"Usage: r_UseEdgeAA [0/1/2]\n"
		"Default is 1 (edge blurring)\n"
		"1 = activate edge blurring mode\n"
		"2 = activate edge antialiasing mode (previous version)");
  iConsole->Register("r_UseHWSkinning", &CV_r_usehwskinning, 1, 0,
    "Toggles HW skinning.\n"
    "Usage: r_UseHWSkinning [0/1]\n"
    "Default is 1 (on). Set to 0 to disable HW-skinning.");
  iConsole->Register("r_UseMaterialLayers", &CV_r_usemateriallayers, 1,0,
    "Enables material layers rendering.\n"
    "Usage: r_UseMaterialLayers [0/1]\n"
    "Default is 1 (on). Set to 0 to disable material layers.\n");

  iConsole->Register("r_UseSoftParticles", &CV_r_usesoftparticles, 1,0,
    "Enables soft particles.\n"
    "Usage: r_UseSoftParticles [0/1]\n");

  iConsole->Register("r_UseParticlesRefraction", &CV_r_useparticles_refraction, 1,0,
    "Enables refractive particles.\n"
    "Usage: r_UseParticlesRefraction [0/1]\n");

  iConsole->Register("r_UseParticlesGlow", &CV_r_useparticles_glow, 1,0,
    "Enables glow particles.\n"
    "Usage: CV_r_useparticles_glow [0/1]\n");

	iConsole->Register("r_UsePOM", &CV_r_usepom, 1, 0,
		"Enables Parallax Occlusion Mapping.\n"
		"Usage: r_UsePOM [0/1]\n");

  iConsole->Register("r_HairSortingQuality", &CV_r_hair_sorting_quality, 1, 0,
    "Enables higher quality hair sorting.\n"
    "Usage: r_HairSortingQuality [0/1]\n");

  iConsole->Register("r_MotionBlur", &CV_r_motionblur, 1,0,
    "Enables per object and screen motion blur.\n"
    "Usage: r_MotionBlur [0/1/2/3/4/101/102/103/104]\n"
    "Default is 1 (screen motion blur on). 1 enables screen motion blur. 2 enables screen and object motion blur. 3 all motion blur and freezing. 4. only per object; modes above 100 also enable motion blur in multiplayer\n");  

  iConsole->Register("r_MotionBlurShutterSpeed", &CV_r_motionblur_shutterspeed, 0.015f,0,
    "Sets motion blur camera shutter speed.\n"
    "Usage: r_MotionBlurShutterSpeed [0...1]\n"
    "Default is 0.015f.\n");  

  iConsole->Register("r_DebugMotionBlur", &CV_r_debug_motionblur, 0,0,
    "Usage: r_MotionBlur [0/1]\n");

  iConsole->Register("r_MotionBlurShutterSpeed", &CV_r_motionblur_shutterspeed, 0.015f,0,
    "Sets motion blur camera shutter speed.\n"
    "Usage: r_MotionBlurShutterSpeed [0...1]\n"
    "Default is 0.015f.\n");  

  iConsole->Register("r_MotionBlurFrameTimeScale", &CV_r_motionblurframetimescale, 1,0,
    "Enables motion blur.frame time scalling - visually nicer on lower frame rates\n"
    "Usage: r_MotionBlurFrameTimeScale [0/1]\n");

  iConsole->Register("r_MotionBlurDynamicQuality", &CV_r_motionblurdynamicquality, 1,0,
    "Enables motion blur.dynamic quality setting depending on movement amount\n"
    "Usage: r_MotionBlurDynamicQuality [0/1]\n");

  iConsole->Register("r_MotionBlurDynQualityRotationAccStiffness", &CV_r_motionblurdynamicquality_rotationacc_stiffness, 1.0f,0,
    "Usage: r_MotionBlurDynQualityRotationAccStiffness value (default 1.0f)\n");

  iConsole->Register("r_MotionBlurDynQualityRotationThreshold", &CV_r_motionblurdynamicquality_rotationthreshold, 1.0f,0,
    "Enables motion blur.dynamic quality setting depending on movement amount\n"
    "Usage: r_MotionBlurDynQualityRotationThreshold value (default 1.0f)\n");

  iConsole->Register("r_MotionBlurDynQualityTranslationThreshold", &CV_r_motionblurdynamicquality_translationthreshold, 1.0f,0,
    "Enables motion blur.dynamic quality setting depending on movement amount\n"
    "Usage: r_MotionBlurDynQualityTranslationThreshold value (default 1.0f)\n");

  iConsole->Register("r_DebugExtraSceneTargetFSAA", &CV_r_debug_extra_scenetarget_fsaa, 1,0,
    "Disables usage of shared sceneTarget RT in case its multisampled\n"
    "Usage: r_DebugSceneTargetNoFSAA [0/1]\n");

  iConsole->Register("r_CustomVisions", &CV_r_customvisions, 1,0,
    "Enables custom visions, like heatvision, binocular view, etc.\n"
    "Usage: r_CustomVisions [0/1]\n"
    "Default is 0 (disabled). 1 enables\n");  

  iConsole->Register("r_Rain", &CV_r_rain, 1,0,
    "Enables rain rendering\n"
    "Usage: r_Rain [0/1/2]\n"
    "Default is 0 (disabled). 1 enables. 2 enables rain and rain fins\n");  

  iConsole->Register("r_RainMaxViewDist", &CV_r_rain_maxviewdist, 32.0f,0,
    "Sets rain max view distance\n"
    "Usage: r_RainMaxViewDist \n");

  iConsole->Register("r_DepthOfField", &CV_r_dof, 1,0,
    "Enables depth of field.\n"
    "Usage: r_DepthOfField [0/1/2]\n"
    "Default is 0 (disabled). 1 enables, 2 enables and overrides game settings\n");  

  iConsole->Register("r_LightVolumesDebug", &CV_r_LightVolumesDebug, 0, 0,
                   "0=Disable\n"
                   "1=Enable\n"
                   "Usage: r_LightVolumesDebug[0/1]\n");

  iConsole->Register("r_UseShadowsPool", &CV_r_UseShadowsPool, 0, 0,
    "0=Disable\n"
    "1=Enable\n"
    "Usage: r_UseShadowsPool[0/1]\n");
  
  iConsole->Register("r_ShadowsSlopeScaleBias", &CV_r_ShadowsSlopeScaleBias, 1.8f, VF_DUMPTODISK, //3.5
    "Select shadow map bluriness if r_ShadowBlur is activated.\n"
    "Usage: r_ShadowBluriness [0.1 - 16]\n");
  iConsole->Register("r_ShadowsBias", &CV_r_ShadowsBias, 0.00008f, VF_DUMPTODISK, //-0.00002
    "Select shadow map bluriness if r_ShadowsBias is activated.\n"
    "Usage: r_ShadowsBias [0.1 - 16]\n");

  iConsole->Register("r_ShadowGenMode", &CV_r_ShadowGenMode, 1, 0,
                   "0=Use Frustums Mask\n"
                   "1=Regenerate all sides\n"
                   "Usage: r_ShadowGenMode [0/1]\n");

  iConsole->Register("r_ShadowBlur", &CV_r_shadowblur, 3, VF_DUMPTODISK,
    "Selected shadow map screenspace blurring technique.\n"
    "Usage: r_ShadowBlur [0=no blurring(fastest)/1=blur/2=blur/3=blur without leaking(slower)]\n");
  iConsole->Register("r_ShadowTexFormat", &CV_r_shadowtexformat, 4, 0,
    "0=use R16G16 texture format for depth map, 1=try to use R16 format if supported as render target\n"
    "2=use R32F texture format for depth map\n"
    "3=use ATI's DF24 texture format for depth map\n"
    "4=use NVIDIA's D24S8 texture format for depth map\n"
    "5=use NVIDIA's D16 texture format for depth map\n"
    "Usage: r_ShadowTexFormat [0-5]\n");
  
  iConsole->Register("r_ShadowsDeferredMode", &CV_r_ShadowsDeferredMode, 1, 0,
    "0=Quad light bounds\n"
    "1=Use light volumes\n"
    "Usage: r_ShadowsDeferredMode [0/1]\n");
  iConsole->Register("r_ShadowsMaskResolution", &CV_r_ShadowsMaskResolution, 0, 0,
    "0=per pixel shadow mask\n"
    "1=horizontal half resolution shadow mask\n"
    "2=horizontal and vertical half resolution shadow mask\n"
    "Usage: r_ShadowsMaskResolution [0/1/2]\n");
  iConsole->Register("r_ShadowsMaskDownScale", &CV_r_ShadowsMaskDownScale, 0, 0,
    "Saves video memory by using lower resolution for shadow masks except first one\n"
    "0=per pixel shadow mask\n"
    "1=half resolution shadow mask\n"
    "Usage: r_ShadowsMaskDownScale [0/1]\n");
  iConsole->Register("r_ShadowsStencilPrePass", &CV_r_ShadowsStencilPrePass, 1, 0,
    "1=Use Stencil pre-pass for shadows\n"
    "Usage: r_ShadowsStencilPrePass [0/1]\n");
  iConsole->Register("r_ShadowsDepthBoundNV", &CV_r_ShadowsDepthBoundNV, 0, 0,
    "1=use NV Depth Bound extension\n"
    "Usage: CV_r_ShadowsDepthBoundNV [0/1]\n");
  iConsole->Register("r_ShadowsForwardPass", &CV_r_ShadowsForwardPass, 1, 0,
    "1=use Forward prepare depth maps pass\n"
    "Usage: CV_r_ShadowsForwardPass [0/1]\n");
  iConsole->Register("r_ShadowBluriness", &CV_r_shadowbluriness, 1.0f, VF_DUMPTODISK,
    "Select shadow map bluriness if r_ShadowBlur is activated.\n"
    "Usage: r_ShadowBluriness [0.1 - 16]\n");
	iConsole->Register("r_ShadowJittering", &CV_r_shadow_jittering, 3.4f, 0,
		"Activate shadow map jittering.\n"
		"Usage: r_ShadowJittering [0=off, 1=on]\n");
	iConsole->Register("r_VarianceShadowMapBlurAmount", &CV_r_VarianceShadowMapBlurAmount, 1.0f, VF_DUMPTODISK,
		"Activate shadow map blur.\n"
		"Usage: r_VarianceShadowMapBlurAmount [0=deactivate, >0 to specify blur amount (1=normal)]\n");
  iConsole->Register("r_DebugLights", &CV_r_debuglights, 0,VF_CHEAT,
		"Display dynamic lights for debugging.\n"
		"Usage: r_DebugLights [0/1/2/3]\n"
		"Default is 0 (off). Set to 1 to display centres of light sources,\n"
		"or set to 2 to display light centres and attenuation spheres, 3 to get light properties to the screen");
  iConsole->Register( "r_ShadowsGridAligned", &CV_r_ShadowsGridAligned, 1, VF_DUMPTODISK,
      "Selects algorithm to use for shadow mask generation:\n"
      "0 - Disable shadows snapping\n"
      "1 - Enable shadows snapping" );
  iConsole->Register( "r_ShadowGenGS", &CV_r_ShadowGenGS, 0, VF_DUMPTODISK,
    "Use geometry shader for shadow map generation (DX10 only, don't change at runtime)\n"
		"Usage: r_ShadowGenGS [0=off, 1=on]\n");
  iConsole->Register( "r_ShadowPass", &CV_r_ShadowPass, 1, 0,
    "Process shadow pass" );
  iConsole->Register( "r_ShadowGen", &CV_r_ShadowGen, 1, 0,
    "0=disable shadow map updates, 1=enable shadow map updates" );
	iConsole->Register( "capture_misc_render_buffers", &CV_capture_misc_render_buffers, 0, 0,
		"Captures HDR target, depth buffer, shadow mask buffer, AO buffer along with final frame buffer.\n"
		"0=Disable (default)\n"
		"1=Enable\n"
		"Usage: capture_misc_render_buffers [0/1]\n"
		"Note: Enable capturing via \"capture_frames 1\".");

	iConsole->Register( "r_SSAO", &CV_r_SSAO, 1, 0, "Enable ambient occlusion" );
	iConsole->Register( "r_SSAO_blur", &CV_r_SSAO_blur, 4, 0, "SSAO mask blur" );
  iConsole->Register( "r_SSAO_downscale_ztarget", &CV_r_SSAO_downscale_ztarget, 1, 0, "Use downscaled version of z-target" );
  iConsole->Register( "r_SSAO_downscale_result_mask", &CV_r_SSAO_downscale_result_mask, 0, 0, "Downscale final mask" );
	iConsole->Register( "r_SSAO_amount", &CV_r_SSAO_amount,	   1.00f, 0, "Controls how much SSAO affects ambient" );
  iConsole->Register( "r_SSAO_darkening", &CV_r_SSAO_darkening,	   0.075f, 0, "Controls how much SSAO darkens flat open surfaces" );
	iConsole->Register( "r_SSAO_radius",	&CV_r_SSAO_radius,	 2.00f, 0, "Controls size of area tested" );
	iConsole->Register( "r_SSAO_quality", &CV_r_SSAO_quality,	   2, 0, "SSAO shader quality" );
  iConsole->Register( "r_TerrainAO", &CV_r_TerrainAO, 7, 0, "7=Activate terrain AO deferred passes" );
  iConsole->Register( "r_TerrainAO_FadeDist", &CV_r_TerrainAO_FadeDist, 8, 0, "Controls sky light fading in tree canopy in Z direction" );
  iConsole->Register( "r_FillLights", &CV_r_FillLights, 1, 0, "Activate simple differed light sources usage" );
  iConsole->Register( "r_FillLightsMode", &CV_r_FillLightsMode, 14, 0, "Fill lights mode" );
  iConsole->Register( "r_FillLightsDebug", &CV_r_FillLightsDebug, 0, 0, "Visualize fill lights as spheres" );
  iConsole->Register("r_SSAO_blurriness", &CV_r_SSAO_blurriness, 1.0f, 0, "SSAO post-blur kernel size");
  iConsole->Register("r_SSAO_depth_range", &CV_r_SSAO_depth_range, 0.99999f, 0, "Use depth test to avoid SSAO computations on sky, 0 = disabled");

  iConsole->Register("r_LightsSinglePass", &CV_r_lightssinglepass, 0);
  iConsole->Register("r_CullGeometryForLights", &CV_r_cullgeometryforlights, 0,0,
		"Rendering optimization for lights.\n"
		"Usage: r_CullGeometryForLights [0/1/2]\n"
		"Default is 0 (off). Set to 1 to cull geometry behind\n"
		"light sources. Set to 2 to cull geometry behind static\n"
		"lights only.");
  CV_r_showlight = iConsole->RegisterString("r_ShowLight","0", NULL,
		"Display a light source by name.\n"
		"Usage: r_ShowLight lightname\n"
		"Default is 0. Set to 'lightname' to show only the light\n"
		"from the source named 'lightname'.");

  iConsole->Register("r_ShowDynTextures",&CV_r_showdyntextures,0, NULL,
	  "Display a dyn. textures, filtered by r_ShowDynTextureFilter\n"
	  "Usage: r_ShowDynTextures 0/1/2\n"
	  "Default is 0. Set to 1 to show all dynamic textures or 2 to display only the ones used in this frame\n");

  CV_r_showdyntexturefilter = iConsole->RegisterString("r_ShowDynTextureFilter","*", NULL,
    "Usage: r_ShowDynTextureFilter *end\n"
	"Usage: r_ShowDynTextureFilter *mid*\n"
	"Usage: r_ShowDynTextureFilter start*\n"
    "Default is *. Set to 'pattern' to show only specific textures (activate r_ShowDynTextures)\n");

  iConsole->Register("r_DebugScreenEffects", &CV_r_debugscreenfx, 0, VF_CHEAT,
    "Debugs screen effects textures.\n"
    "Usage: r_DebugScreenEffects #\n"
    "Where # represents:\n"
    "	0: disabled (default)\n"
    "	1: enabled\n");
   
	iConsole->Register("r_RC_AutoInvoke", &CV_r_rc_autoinvoke, (gEnv->pSystem->IsDevMode() ? 1:0) , 0,
		"Enable calling the resource compiler (rc.exe) to compile TIF file to DDS files if the date check\n"
		"showes that the destination is older or does not exist.\n"
		"Usage: r_RC_AutoInvoke 0 (default is 1)\n");

  iConsole->Register("r_Glow", &CV_r_glow, 1, 0,
    "Toggles the glow effect.\n"
    "Usage: r_Glow [0/1]\n"
    "Default is 0 (off). Set to 1 to enable glow effect.");

  iConsole->Register("r_GlowScreenThreshold", &CV_r_glow_fullscreen_threshold, 0.5, 0,
    "Sets fullscreen glow threshold.\n"
    "Usage: r_GlowScreenThreshold [value]\n"
    "Default is 0.5");

  iConsole->Register("r_GlowScreenMultiplier", &CV_r_glow_fullscreen_multiplier, 0.5, 0,
    "Sets fullscreen glow multiplier.\n"
    "Usage: r_GlowScreenMultiplier [value]\n"
    "Default is 0.5");

  iConsole->Register("r_NightVision", &CV_r_nightvision, 1, 0,
    "Toggles nightvision enabling.\n"
    "Usage: r_NightVision [0/1]\n"
    "Default is 1 (on). Set to 0 to completely disable nightvision.");

  iConsole->Register("r_refraction", &CV_r_refraction, 1, 0,
		"Enables refraction.\n"
		"Usage: r_refraction [0/1]\n"
		"Default is 1 (on). Set to 0 to disable.");  

  iConsole->Register("r_sunshafts", &CV_r_sunshafts, 1, 0,
    "Enables sun shafts.\n"
    "Usage: r_sunshafts [0/1]\n"
    "Default is 1 (on). Set to 0 to disable.");  

  iConsole->Register("r_distant_rain", &CV_r_distant_rain, 1, 0,
    "Enables distant rain rendering.\n"
    "Usage: r_distant_rain [0/1]\n"
    "Default is 1 (on). Set to 0 to disable.");  
  
  iConsole->Register("r_GlitterSize", &CV_r_glitterSize, 1.0f, 0,
    "Sets glitter sprite size.\n"
    "Usage: r_GlitterSize n (default is 1)\n"
    "Where n represents a number: eg: 0.5");  

  iConsole->Register("r_GlitterVariation", &CV_r_glitterVariation, 1.0f, 0,
    "Sets glitter variation.\n"
    "Usage: r_GlitterVariation n (default is 1)\n"
    "Where n represents a number: eg: 0.5");  

  iConsole->Register("r_GlitterSpecularPow", &CV_r_glitterSpecularPow, 2.0f, 0,
    "Sets glitter specular power.\n"
    "Usage: r_GlitterSpecularPow n (default is 2.0f)\n"
    "Where n represents a number: eg: 16.0");  

  iConsole->Register("r_GlitterAmount", &CV_r_glitterAmount, 1024, 0,
    "Sets amount of glitter sprites.\n"
    "Usage: r_GlitterAmount n (default is 1024)\n"
    "Where n represents a number: eg: 256");  

	iConsole->Register("r_PostProcessEffects", &CV_r_postprocess_effects, 1, VF_CHEAT,
		"Enables post processing special effects.\n"
		"Usage: r_PostProcessEffects [0/1/2]\n"
		"Default is 1 (enabled). 2 enables and displays active effects"); 

  iConsole->Register("r_PostProcessEffectsParamsBlending", &CV_r_postprocess_effects_params_blending, 1, 0,
    "Enables post processing effects parameters smooth blending\n"
    "Usage: r_PostProcessEffectsParamsBlending [0/1]\n"
    "Default is 1 (enabled)."); 

  iConsole->Register("r_PostProcessEffectsFilters", &CV_r_postprocess_effects_filters, 1, VF_CHEAT,
    "Enables post processing special effects filters.\n"
    "Usage: r_PostProcessEffectsFilters [0/1]\n"
    "Default is 1 (enabled). 0 disabled"); 

  iConsole->Register("r_PostProcessEffectsGameFx", &CV_r_postprocess_effects_gamefx, 1, VF_CHEAT,
    "Enables post processing special effects game fx.\n"
    "Usage: r_PostProcessEffectsGameFx [0/1]\n"
    "Default is 1 (enabled). 0 disabled"); 

  iConsole->Register("r_PostProcessEffectsReset", &CV_r_postprocess_effects_reset, 0, VF_CHEAT,
    "Enables post processing special effects reset.\n"
    "Usage: r_PostProcessEffectsReset [0/1]\n"
    "Default is 0 (disabled). 1 enabled"); 

  iConsole->Register("r_PostProcessProfileFillrate", &CV_r_postprocess_profile_fillrate, 0, VF_CHEAT,
    "Enables profile fillrate.\n"
    "Usage: r_PostProcessProfileFillrate [0/1]\n"
    "Default is 0 (disabled). 1 enabled"); 

  iConsole->Register("r_ColorGrading", &CV_r_colorgrading, 1, 0,
    "Enables color grading.\n"
    "Usage: r_ColorGrading [0/1]\n");

  iConsole->Register("r_ColorGradingSelectiveColor", &CV_r_colorgrading_selectivecolor, 1, 0,
    "Enables color grading.\n"
    "Usage: r_ColorGradingSelectiveColor [0/1]\n");

  iConsole->Register("r_ColorGradingLevels", &CV_r_colorgrading_levels, 1, 0,
    "Enables color grading.\n"
    "Usage: r_ColorGradingLevels [0/1]\n");

  iConsole->Register("r_ColorGradingFilters", &CV_r_colorgrading_filters, 1, 0,
    "Enables color grading.\n"
    "Usage: r_ColorGradingFilters [0/1]\n");

  iConsole->Register("r_ColorGradingDof", &CV_r_colorgrading_dof, 1, 0,
    "Enables color grading dof control.\n"
    "Usage: r_ColorGradingDof [0/1]\n");
  

  iConsole->Register("r_CloudsUpdateAlways", &CV_r_cloudsupdatealways, 0, 0,
		"Toggles updating of clouds each frame.\n"
		"Usage: r_CloudsUpdateAlways [0/1]\n"
		"Default is 0 (off.");
  iConsole->Register("r_CloudsDebug", &CV_r_cloudsdebug, 0, 0,
    "Toggles debugging mode for clouds."
    "Usage: r_CloudsDebug [0/1/2]\n"
    "Usage: r_CloudsDebug = 1: render just screen imposters\n"
    "Usage: r_CloudsDebug = 2: render just non-screen imposters\n"
    "Default is 0 (off)");

	iConsole->Register("r_DynTexMaxSize", &CV_r_dyntexmaxsize, 48); // 64 Mb
#if !defined(XENON)
  iConsole->Register("r_TexAtlasSize", &CV_r_texatlassize, 1024); // 1024x1024
  iConsole->Register("r_TexPostponeLoading", &CV_r_texpostponeloading, 1);
  iConsole->Register("r_DynTexAtlasCloudsMaxSize", &CV_r_dyntexatlascloudsmaxsize, 32); // 32 Mb
  iConsole->Register("r_DynTexAtlasSpritesMaxSize", &CV_r_dyntexatlasspritesmaxsize, 32); // 32 Mb
#else
  iConsole->Register("r_TexAtlasSize", &CV_r_texatlassize, 512); // 512x512
  iConsole->Register("r_TexPostponeLoading", &CV_r_texpostponeloading, 0);
  iConsole->Register("r_DynTexAtlasCloudsMaxSize", &CV_r_dyntexatlascloudsmaxsize, 16); // 16 Mb
  iConsole->Register("r_DynTexAtlasSpritesMaxSize", &CV_r_dyntexatlasspritesmaxsize, 8); // 8 Mb
#endif
  iConsole->Register("r_TexMaxAnisotropy", &CV_r_texmaxanisotropy, 8, VF_REQUIRE_LEVEL_RELOAD);
  iConsole->Register("r_TexMaxSize", &CV_r_texmaxsize, 0,VF_CHEAT);
  iConsole->Register("r_TexMinSize", &CV_r_texminsize, 64,VF_CHEAT);

	// changing default settings to reduce the insane amount of texture memory
  iConsole->Register("r_TexBumpResolution", &CV_r_texbumpresolution, 0, VF_DUMPTODISK,
		"Reduces texture resolution.\n"
		"Usage: r_TexBumpResolution [0/1/2 etc]\n"
		"When 0 (default) texture resolution is unaffected, 1 halves, 2 quarters etc.");

	iConsole->Register("r_TexBumpHeightmap", &CV_r_texbumpheightmap, 0, 0,
		"Allows to combine _DDN and _BUMP and _BUMP to _DDN on load conversion (processing adds to loading time and compression might be less)\n"
		"This is a legacy feature and should not be used\n"
		"Usage: r_TexBumpHeightmap [0/1]\n"
		"When 0 (default) the feature is deactivated, 1 enables it");

  iConsole->Register("r_TexResolution", &CV_r_texresolution, 0, VF_DUMPTODISK,
		"Reduces texture resolution.\n"
		"Usage: r_TexResolution [0/1/2 etc]\n"
		"When 0 (default) texture resolution is unaffected, 1 halves, 2 quarters etc.");
	//iConsole->Register("r_TexResolution", &CV_r_texresolution, 1, VF_DUMPTODISK );

  iConsole->Register("r_TexLMResolution", &CV_r_texlmresolution, 0, VF_DUMPTODISK,
		"Reduces texture resolution.\n"
		"Usage: r_TexLMResolution [0/1/2 etc]\n"
		"When 0 (default) texture resolution is unaffected, 1 halves, 2 quarters etc.");

  iConsole->Register( "r_TexSkyResolution", &CV_r_texskyresolution, 0, VF_DUMPTODISK );
	//iConsole->Register( "r_TexSkyResolution", &CV_r_texskyresolution, 1, VF_DUMPTODISK );

	iConsole->Register("r_TexSkyQuality", &CV_r_texskyquality, 0);
  iConsole->Register("r_TexNoAniso", &CV_r_texnoaniso, 0, VF_DUMPTODISK);
  iConsole->Register("r_Texture_Anisotropic_Level", &CV_r_texture_anisotropic_level, 1, VF_DUMPTODISK);
  iConsole->Register("r_TexNormalMapType", &CV_r_texnormalmaptype, 1, VF_REQUIRE_LEVEL_RELOAD);
  iConsole->Register("r_TexHWMipsGeneration", &CV_r_texhwmipsgeneration, 1);
//  iConsole->Register("r_TexHWDXTCompression", &CV_r_texhwdxtcompression, 1);
  iConsole->Register("r_TexGrid", &CV_r_texgrid, 0);
  iConsole->Register("r_TexLog", &CV_r_texlog, 0, 0,
    "Configures texture information logging.\n"
    "Usage:	r_TexLog #\n"
    "where # represents:\n"
    "	0: Texture logging off\n"
    "	1: Texture information logged to screen\n"
    "	2: All loaded textures logged to 'UsedTextures.txt'\n"
    "	3: Missing textures logged to 'MissingTextures.txt");
  iConsole->Register("r_TexNoLoad", &CV_r_texnoload, 0, 0,
    "Disables loading of textures.\n"
    "Usage:	r_TexNoLoad [0/1]\n"
    "When 1 texture loading is disabled.");

  iConsole->Register("r_TexturesStreamPoolSize", &CV_r_texturesstreampoolsize, 128, 0);
  iConsole->Register("r_TexturesStreamingSync", &CV_r_texturesstreamingsync, 0);
  iConsole->Register("r_TexturesStreamingMaxAsync", &CV_r_texturesstreamingmaxasync, 0.25f);
  iConsole->Register("r_TexturesStreamingNoUpload", &CV_r_texturesstreamingnoupload, 0);
  iConsole->Register("r_TexturesStreamingOnlyVideo", &CV_r_texturesstreamingonlyvideo, 0);
	iConsole->Register("r_TexturesStreamingIgnore", &CV_r_texturesstreamingignore, 0);
  iConsole->Register("r_TexturesStreaming", &CV_r_texturesstreaming, 0, VF_REQUIRE_APP_RESTART,
		"Enables direct streaming of textures from disk during game.\n"
		"Usage: r_TexturesStreaming [0/1]\n"
		"Default is 0 (off). All textures save in native format with mips in a\n"
		"cache file. Textures are then loaded into texture memory from the cache.");
  iConsole->Register("r_TexturesMipBiasing", &CV_r_texturesmipbiasing, 0,0,0,OnTexturesMipBiasing);
  iConsole->Register("r_TexturesFilteringQuality", &CV_r_texturesfilteringquality, 0, VF_REQUIRE_LEVEL_RELOAD,
    "Configures texture filtering adjusting.\n"
    "Usage: r_TexturesFilteringQuality [#]\n"
    "where # represents:\n"
    "	0: Highest quality\n"
    "	1: Medium quality\n"
    "	2: Low quality\n");

	iConsole->Register("r_TextureLodDistanceRatio", &CV_r_TextureLodDistanceRatio, -1, 0,
    "Controls dynamic LOD system for textures used in materials.\n"
    "Usage: r_TextureLodDistanceRatio [-1, 0 and bigger]\n"
    "Default is -1 (completely off). Value 0 will set full LOD to all textures used in frame.\n"
    "Values bigger than 0 will activate texture LOD selection depending on distance to the objects.");

	iConsole->Register("r_TextureLodMaxLod", &CV_r_TextureLodMaxLod, 1, 0,
		"Controls dynamic LOD system for textures used in materials.\n"
		"Usage: r_TextureLodMaxLod [1 or bigger]\n"
		"Default is 1 (playing between lod 0 and 1). Value 0 will set full LOD to all textures used in frame");

  iConsole->Register("r_MultiGPU", &CV_r_multigpu, 2, 0,
		"0=disabled, 1=extra overhead to allow SLI(NVidia) or Crossfire(ATI),\n"
		"2(default)=automatic detection (currently SLI only, means off for ATI)\n"
		"should be activated before rendering");
  iConsole->Register("r_ValidateDraw", &CV_r_validateDraw, 0, 0,
    "0=disabled, 1=validate each DIP (meshes consistency, shaders, declarations, etc)\n");
  iConsole->Register("r_ProfileDIPs", &CV_r_profileDIPs, 1, VF_CHEAT,
    "0=disabled, 1=profile each DIP performance (may cause very low frame rate)\n"
		"r_ProfileShaders needs to be activated to see the statistics");

  iConsole->Register("r_OcclusionQueriesMGPU", &CV_r_occlusionqueries_mgpu, 1, 0,
    "0=disabled, 1=enabled (if mgpu supported),\n");

  iConsole->Register("r_FSAA", &CV_r_fsaa, 0, VF_DUMPTODISK | VF_REQUIRE_APP_RESTART);
  iConsole->Register("r_FSAA_samples", &CV_r_fsaa_samples, 4, VF_DUMPTODISK | VF_REQUIRE_APP_RESTART);
  iConsole->Register("r_FSAA_quality", &CV_r_fsaa_quality, 0, VF_DUMPTODISK | VF_REQUIRE_APP_RESTART);

  iConsole->Register("r_ATOC", &CV_r_atoc, 0, VF_DUMPTODISK);

  iConsole->Register("r_ShowNormals", &CV_r_shownormals, 0, VF_CHEAT,
		"Toggles visibility of normal vectors.\n"
		"Usage: r_ShowNormals [0/1]"
		"Default is 0 (off).");
  iConsole->Register("r_ShowLines", &CV_r_showlines, 0, VF_CHEAT,
		"Toggles visibility of wireframe overlay.\n"
		"Usage: r_ShowLines [0/1]"
		"Default is 0 (off).");
  iConsole->Register("r_NormalsLength", &CV_r_normalslength, 0.1f, VF_CHEAT,
		"Sets the length of displayed vectors.\n"
		"r_NormalsLength 0.1\n"
		"Default is 0.1 (metres). Used with r_ShowTangents and r_ShowNormals.");
  iConsole->Register("r_ShowTangents", &CV_r_showtangents, 0, VF_CHEAT,
		"Toggles visibility of three tangent space vectors.\n"
		"Usage: r_ShowTangents [0/1]\n"
		"Default is 0 (off).");
  iConsole->Register("r_ShowTimeGraph", &CV_r_showtimegraph, 0, 0,
		"Configures graphic display of frame-times.\n"
		"Usage: r_ShowTimeGraph [0/1/2]\n"
		"	1: Graph displayed as points."
		"	2: Graph displayed as lines."
		"Default is 0 (off).");
  iConsole->Register("r_ShowTexTimeGraph", &CV_r_showtextimegraph, 0, 0,
    "Configures graphic display of frame-times.\n"
    "Usage: r_ShowTexTimeGraph [0/1/2]\n"
    "	1: Graph displayed as points."
    "	2: Graph displayed as lines."
    "Default is 0 (off).");
  iConsole->Register("r_ShowLumHistogram", &CV_r_showlumhistogram, 0, 0,
    "Configures graphic display of luminance histogram.\n"
    "Usage: r_ShowLumHistogram [0/1/2]\n"
    "	1: Graph displayed as points."
    "	2: Graph displayed as lines."
    "Default is 0 (off).");
  iConsole->Register("r_GraphStyle", &CV_r_graphstyle, 0);

  iConsole->Register("r_LogVBuffers", &CV_r_logVBuffers, 0, VF_CHEAT,
		"Logs vertex buffers in memory to 'LogVBuffers.txt'.\n"
		"Usage: r_LogVBuffers [0/1]\n"
		"Default is 0 (off).", GetLogVBuffersStatic);
  iConsole->Register("r_LogTexStreaming", &CV_r_logTexStreaming, 0,VF_CHEAT,
		"Logs streaming info to Direct3DLogStreaming.txt\n"
		"0: off\n"
		"1: normal\n"
		"2: extended");
  iConsole->Register("r_LogShaders", &CV_r_logShaders, 0,VF_CHEAT,
    "Logs shaders info to Direct3DLogShaders.txt\n"
    "0: off\n"
    "1: normal\n"
    "2: extended");
  iConsole->Register("r_Flush", &CV_r_flush, 1); // this causes the game to freeze (infinite loop) - do not use

  iConsole->Register("r_NoLoadTextures", &CV_r_noloadtextures, 0,VF_CHEAT);
  iConsole->Register("r_NoPreprocess", &CV_r_nopreprocess, 0);

  iConsole->Register("r_OptimisedLightSetup", &CV_r_optimisedlightsetup, 1);

  iConsole->Register("r_ShadersAlwaysUseColors", &CV_r_shadersalwaysusecolors, 1, 0);
  iConsole->Register("r_ShadersDebug", &CV_r_shadersdebug, 0, VF_DUMPTODISK,
		"Enable special logging when shaders become compiled\n"
		"Usage: r_ShadersDebug [0/1/2/3]\n"
		" 1 = assembly into directory Main/{Game}/shaders/cache/d3d9\n"
		" 2 = compiler input into directory Main/{Game}/testcg\n"
		" 3 = compiler input into directory Main/{Game}/testcg_1pass\n"
		"Default is 0 (off)");
  iConsole->Register("r_ShadersUserFolder", &CV_r_shadersuserfolder, 1, 0);
  iConsole->Register("r_ShadersIgnoreIncludesChanging", &CV_r_shadersignoreincludeschanging, 0, 0);
  iConsole->Register("r_ShadersPreactivate", &CV_r_shaderspreactivate, 0, VF_DUMPTODISK);
  iConsole->Register("r_ShadersIntCompiler", &CV_r_shadersintcompiler, 1, VF_DUMPTODISK);
  iConsole->Register("r_ShadersAsyncCompiling", &CV_r_shadersasynccompiling, 1, 0,
		"Enable asynchronous shader compiling\n"
		"Usage: r_ShadersAsyncCompiling [0/1]\n"
		" 0 = off, (stalling) shadering compiling\n"
		" 1 = on, shaders are compiled in parallel, missing shaders are rendered in yellow\n"
		" 2 = on, shaders are compiled in parallel, missing shaders are not rendered\n");

  iConsole->Register("r_ShadersAsyncMaxThreads", &CV_r_shadersasyncmaxthreads, 1, VF_DUMPTODISK);
  iConsole->Register("r_ShadersCacheOptimiseLog", &CV_r_shaderscacheoptimiselog, 0, 0);
  iConsole->Register("r_ShadersPrecacheAllLights", &CV_r_shadersprecachealllights, 1, 0);
  iConsole->Register("r_ShadersStaticBranching", &CV_r_shadersstaticbranching, 1, 0);
  iConsole->Register("r_ShadersDynamicBranching", &CV_r_shadersdynamicbranching, 0, 0);

  iConsole->Register("r_DebugRenderMode", &CV_r_debugrendermode, 0, VF_CHEAT);
  iConsole->Register("r_DebugRefraction", &CV_r_debugrefraction, 0, VF_CHEAT,
    "Debug refraction usage. Displays red instead of refraction\n"
    "Usage: r_DebugRefraction\n"
    "Default is 0 (off)");
  
  iConsole->Register("r_MeshPrecache", &CV_r_meshprecache, 1);
  iConsole->Register("r_MeshShort", &CV_r_meshshort, 0);

  CV_r_excludeshader = iConsole->RegisterString("r_ExcludeShader", "0", VF_CHEAT,
		"Exclude the named shader from the render list.\n"
		"Usage: r_ExcludeShader ShaderName\n"
		"Sometimes this is useful when debugging.");
  CV_r_showonlyshader = iConsole->RegisterString("r_ShowOnlyShader", "0", VF_CHEAT,
		"Render only the named shader, ignoring all others.\n"
		"Usage: r_ShowOnlyShader ShaderName");
  iConsole->Register("r_ProfileShaders", &CV_r_profileshaders, 0, VF_CHEAT,
		"Enables display of render profiling information.\n"
		"Usage: r_ProfileShaders [0/1]\n"
		"Default is 0 (off). Set to 1 to display profiling\n"
		"of rendered shaders.");
  iConsole->Register("r_ProfileShadersSmooth", &CV_r_profileshaderssmooth, 2, VF_CHEAT,
    "Enables display of render profiling information.\n"
    "Usage: r_ProfileShaders [0/1]\n"
    "Default is 0 (off). Set to 1 to display profiling\n"
    "of rendered shaders.");
  
  iConsole->Register("r_EnvLightCMDebug", &CV_r_envlightcmdebug, 0, VF_CHEAT,
    "Draw debug cube for env radiosity.\n"
    "Usage: r_EnvLightCMDebug [0/1]\n"
    "Default is 0 (off).\n");
  
  iConsole->Register("r_EnvCMWrite", &CV_r_envcmwrite, 0, 0,
		"Writes cube-map textures to disk.\n"
		"Usage: r_EnvCMWrite [0/1]\n"
		"Default is 0 (off). The textures are written to 'Cube_posx.jpg'\n"
		"'Cube_negx.jpg',...,'Cube_negz.jpg'. At least one of the real-time\n"
		"cube-map shaders should be present in the current scene.\n");
  
  iConsole->Register("r_EnvCMResolution", &CV_r_envcmresolution, 1, VF_DUMPTODISK,
		"Sets resolution for target environment cubemap, in pixels.\n"
		"Usage: r_EnvCMResolution #\n"
		"where # represents:\n"
		"	0: 64\n"
		"	1: 128\n"
		"	2: 256\n"
		"Default is 2 (256 by 256 pixels).");
  
  iConsole->Register("r_EnvTexResolution", &CV_r_envtexresolution, 3, VF_DUMPTODISK,
		"Sets resolution for 2d target environment texture, in pixels.\n"
		"Usage: r_EnvTexResolution #\n"
		"where # represents:\n"
		"	0: 64\n"
		"	1: 128\n"
		"	2: 256\n"
		"	3: 512\n"
		"Default is 3 (512 by 512 pixels).");
  
//  iConsole->Register("r_WaterUpdateDistance", &CV_r_waterupdateDistance, 2.0f, 0,"");
//	iConsole->Register("r_WaterUpdateRotation", &CV_r_waterupdateRotation, 0.05f, 0, "");
  
//  iConsole->Register("r_WaterUpdateFactor", &CV_r_waterupdateFactor, 0.01f, VF_DUMPTODISK,
//		"Distance factor for water reflected texture updating.\n"
//		"Usage: r_WaterUpdateFactor 0.01\n"
//		"Default is 0.01. 0 means update every frame");
  
	iConsole->Register("r_WaterUpdateChange", &CV_r_waterUpdateChange, 0.01f, VF_DUMPTODISK | VF_NOT_NET_SYNCED,
		"View-space change factor for water reflection updating.\n"
		"Usage: r_WaterUpdateChange 0.01\n"
		"Range is [0.0, 1.0], default is 0.01, 0 means update every frame.");
	iConsole->Register("r_WaterUpdateTimeMin", &CV_r_waterUpdateTimeMin, 0.01f, VF_DUMPTODISK | VF_NOT_NET_SYNCED,
		"Minimum update time of water reflection texture (that occurs at the level of water), in seconds.\n"
		"Usage: r_WaterUpdateTimeMin 0.01\n"
		"Range is [0.0, 1.0], Default is 0.01.");
	iConsole->Register("r_WaterUpdateTimeMax", &CV_r_waterUpdateTimeMax, 0.1f, VF_DUMPTODISK | VF_NOT_NET_SYNCED,
		"Maximum update time for water reflection texture (that occurs at 100m above the level of water), in seconds.\n"
		"Note that the update time can be higher when the camera is higher than 100m, and the update time is limited to max. 0.3 sec (that is ~3 FPS).\n"
		"Usage: r_WaterUpdateTimeMax 0.1\n"
		"Range is [0.0, 1.0], Default is 0.1.");
  
  iConsole->Register("r_EnvLCMupdateInterval", &CV_r_envlcmupdateinterval, 0.1f, VF_DUMPTODISK,
					"LEGACY - not used");
//		"Sets the interval between environmental cube map texture updates.\n"
//		"Usage: r_EnvCMupdateInterval 0.1\n"
//		"Default is 0.1.");
  iConsole->Register("r_EnvCMupdateInterval", &CV_r_envcmupdateinterval, 0.04f, VF_DUMPTODISK,
		"Sets the interval between environmental cube map texture updates.\n"
    "Usage: r_EnvCMupdateInterval #\n"
		"Default is 0.1.");
  iConsole->Register("r_EnvTexUpdateInterval", &CV_r_envtexupdateinterval, 0.001f, VF_DUMPTODISK,
		"Sets the interval between environmental 2d texture updates.\n"
		"Usage: r_EnvTexUpdateInterval 0.001\n"
		"Default is 0.001.");
  
  iConsole->Register("r_WaterReflections", &CV_r_waterreflections, 1, VF_DUMPTODISK,
		"Toggles water reflections.\n"
		"Usage: r_WaterReflections [0/1]\n"
		"Default is 1 (water reflects).");

  iConsole->Register("r_WaterReflectionsMGPU", &CV_r_waterreflections_mgpu, 1, VF_DUMPTODISK,
    "Toggles water reflections.multi-gpu support\n"
    "Usage: r_WaterReflectionsMGPU [0/1/2]\n"
    "Default is 0 (single render update), 1 (multiple render updates)");

  iConsole->Register("r_WaterReflectionsQuality", &CV_r_waterreflections_quality, 0, VF_DUMPTODISK,
		"Activates water reflections quality setting.\n"
		"Usage: r_WaterReflectionsQuality [0/1/2/3]\n"
		"Default is 0 (terrain only), 1 (terrain + particles), 2 (terrain + particles + brushes), 3 (everything)");

  iConsole->Register("r_WaterReflectionsUseMinOffset", &CV_r_waterreflections_use_min_offset, 1, VF_DUMPTODISK,
    "Activates water reflections use min distance offset.\n");

  iConsole->Register("r_WaterReflectionsMinVisiblePixelsUpdate", &CV_r_waterreflections_min_visible_pixels_update, 0.05f, VF_DUMPTODISK,
    "Activates water reflections if visible pixels above a certain threshold.\n");

  iConsole->Register("r_WaterReflectionsMinVisUpdateFactorMul", &CV_r_waterreflections_minvis_updatefactormul, 20.0f, VF_DUMPTODISK,
    "Activates update factor multiplier when water mostly occluded.\n");  
  iConsole->Register("r_WaterReflectionsMinVisUpdateDistanceMul", &CV_r_waterreflections_minvis_updatedistancemul, 10.0f, VF_DUMPTODISK,
    "Activates update distance multiplier when water mostly occluded.\n");  

  iConsole->Register("r_WaterRefractions", &CV_r_waterrefractions, 1, 0,
		"Toggles water refractions.\n"
		"Usage: r_WaterRefractions [0/1]\n"
		"Default is 1 (water refracts).");

  iConsole->Register("r_WaterCaustics", &CV_r_watercaustics,1, 0,
    "Toggles under water caustics.\n"
    "Usage: r_WaterCaustics [0/1]\n"
    "Default is 1 (enabled).");  

  iConsole->Register("r_WaterGodRays", &CV_r_water_godrays, 1, 0,
    "Enables under water god rays.\n"
    "Usage: r_WaterGodRays [0/1]\n"
    "Default is 1 (enabled).");  

  iConsole->Register("r_Reflections", &CV_r_reflections, 1, VF_DUMPTODISK,
    "Toggles reflections.\n"
    "Usage: r_Reflections [0/1]\n"
    "Default is 1 (reflects).");

  iConsole->Register("r_ReflectionsOffset", &CV_r_waterreflections_offset, 0.0f, 0);
    
  iConsole->Register("r_ReflectionsQuality", &CV_r_reflections_quality, 3, VF_DUMPTODISK,
    "Toggles reflections quality.\n"
    "Usage: r_ReflectionsQuality [0/1/2/3]\n"
    "Default is 0 (terrain only), 1 (terrain + particles), 2 (terrain + particles + brushes), 3 (everything)");
    
  iConsole->Register("r_DetailTextures", &CV_r_detailtextures, 1, VF_DUMPTODISK,
		"Toggles detail texture overlays.\n"
		"Usage: r_DetailTextures [0/1]\n"
		"Default is 1 (detail textures on).");

  iConsole->Register("r_ReloadShaders", &CV_r_reloadshaders, 0, VF_CHEAT,
		"Reloads shaders.\n"
		"Usage: r_ReloadShaders [0/1]\n"
		"Default is 0. Set to 1 to reload shaders.");

  iConsole->Register("r_DetailNumLayers", &CV_r_detailnumlayers, 2, VF_DUMPTODISK,
		"Sets the number of detail layers per surface.\n"
		"Usage: r_DetailNumLayers 2\n"
		"Default is 2.");
  iConsole->Register("r_DetailScale", &CV_r_detailscale, 8.0f, 0,
		"Sets the default scaling for detail overlays.\n"
		"Usage: r_DetailScale 8\n"
		"Default is 8. This scale applies only if the object's\n"
		"detail scale was not previously defined (in MAX).");
  iConsole->Register("r_DetailDistance", &CV_r_detaildistance, 6.0f, VF_DUMPTODISK,
		"Distance used for per-pixel detail layers blending.\n"
    "Usage: r_DetailDistance (1-20)\n"
		"Default is 6.");
  
  iConsole->Register("r_TexBindMode", &CV_r_texbindmode, 0, VF_CHEAT, "");
  iConsole->Register("r_NoDrawShaders", &CV_r_nodrawshaders, 0, VF_CHEAT,
		"Disable entire render pipeline.\n"
		"Usage: r_NoDrawShaders [0/1]\n"
		"Default is 0 (render pipeline enabled). Used for debugging and profiling.\n");
  iConsole->Register("r_NoDrawNear", &CV_r_nodrawnear, 0, 0,
		"Disable drawing of near objects.\n"
		"Usage: r_NoDrawNear [0/1]\n"
		"Default is 0 (near objects are drawn).");
	iConsole->Register("r_DrawNearFoV", &CV_r_drawnearfov, 60, 0,
		"Sets the FoV for drawing of near objects.\n"
		"Usage: r_DrawNearFoV [n]\n"
		"Default is 60.");

  iConsole->Register("r_Flares", &CV_r_flares, 1, VF_DUMPTODISK,
		"Toggles sunlight lens flare effect.\n"
		"Usage: r_Flares [0/1]\n"
		"Default is 1 (on).");

  iConsole->Register("r_Gamma", &CV_r_gamma, 1.0f, VF_DUMPTODISK,
		"Adjusts the graphics card gamma correction (fast, needs hardware support, affects also HUD and desktop)\n"
		"Usage: r_Gamma 1.0\n"
		"1 off (default), try values like 1.6 or 2.2");
  iConsole->Register("r_Brightness", &CV_r_brightness, 0.5f, VF_DUMPTODISK,
		"Sets the diplay brightness.\n"
		"Usage: r_Brightness 0.5\n"
		"Default is 0.5.");
  iConsole->Register("r_Contrast", &CV_r_contrast, 0.5f, VF_DUMPTODISK,
		"Sets the diplay contrast.\n"
		"Usage: r_Contrast 0.5\n"
		"Default is 0.5.");
  iConsole->Register("r_NoHWGamma", &CV_r_nohwgamma, 0, VF_DUMPTODISK,
		"Sets renderer to ignore hardware gamma correction.\n"
		"Usage: r_NoHWGamma [0/1]\n"
		"Default is 0 (allow hardware gamma correction).");

  iConsole->Register("r_Scissor", &CV_r_scissor, 1, 0, "Enables scissor test");
  iConsole->Register("r_Coronas", &CV_r_coronas, 1, VF_DUMPTODISK,
		"Toggles light coronas around light sources.\n"
		"Usage: r_Coronas [0/1]"
		"Default is 1 (on).");
  iConsole->Register("r_CoronaFade", &CV_r_coronafade, 0.5f, 0,
		"Time fading factor of the light coronas.\n"
		"Usage: r_CoronaFade 0.5"
		"Default is 0.5.");
  iConsole->Register("r_CoronaSizeScale", &CV_r_coronasizescale, 1.0f, 0);
  iConsole->Register("r_CoronaColorScale", &CV_r_coronacolorscale, 1.0f, 0,"");

  iConsole->Register("r_CullByClipPlanes", &CV_r_cullbyclipplanes, 1);

  iConsole->Register("r_OceanHeightScale", &CV_r_oceanheightscale, 4) ;
  iConsole->Register("r_OceanSectorSize", &CV_r_oceansectorsize, 128) ;
  iConsole->Register("r_OceanRendType", &CV_r_oceanrendtype, 0) ;
  iConsole->Register("r_OceanLodDist", &CV_r_oceanloddist, 100) ;
  iConsole->Register("r_OceanTexUpdate", &CV_r_oceantexupdate, 1) ;
  iConsole->Register("r_OceanMaxSplashes", &CV_r_oceanmaxsplashes, 8) ;
  iConsole->Register("r_OceanSplashScale", &CV_r_oceansplashscale, 1.0f) ;

  iConsole->Register("r_PolygonMode", &CV_r_PolygonMode, 1, VF_CHEAT);
	iConsole->Register("r_GetScreenShot", &CV_r_GetScreenShot, 0, 0,
     "To capture one screenshot (variable is set to 0 after capturing)\n"
     "0=do not take a screenshot (default), 1=save a screenshot (together with .HDR if enabled), 2=save a screenshot");

  iConsole->Register("r_Character_NoDeform", &CV_r_character_nodeform, 0);

  iConsole->Register("r_Log", &CV_r_log, 0, VF_CHEAT,
		"Logs rendering information to Direct3DLog.txt.\n"
		"Usage: r_Log [0/1/2/3/4]\n"
		"	1: Logs a list of all shaders without profile info.\n"
		"	2: Log contains a list of all shaders with profile info.\n"
		"	3: Logs all API function calls.\n"
		"	4: Highly detailed pipeline log, including all passes,\n"
		"			states, lights and pixel/vertex shaders.\n"
		"Default is 0 (off). Use this function carefully, because\n"
		"log files grow very quickly.");
  iConsole->Register("r_Stats", &CV_r_stats, 0, VF_CHEAT,
		"Toggles render statistics.\n"
    "0=disabled,\n"
    "1=global render stats,\n"
    "2=print shaders for selected object,\n"
    "11=print info about used RT's (switches),\n"
    "12=print info about used unique RT's,\n"
    "13=print info about cleared RT's\n"
		"Usage: r_Stats [0/1/2/3/11/12/13]");
  iConsole->Register("r_VSync", &CV_r_vsync, 0, VF_RESTRICTEDMODE|VF_DUMPTODISK,
		"Toggles vertical sync.\n"
		"Usage: r_VSync [0/1]");
  iConsole->Register("r_MeasureOverdraw", &CV_r_measureoverdraw, 0, VF_CHEAT,
		"0=off, 1=activate special rendering mode that visualize the rendering cost per pixel by colour\n"
		"Usage: r_MeasureOverdraw [0/1]");
  iConsole->Register("r_MeasureOverdrawScale", &CV_r_measureoverdrawscale, 1.5f, VF_CHEAT);
  iConsole->Register("r_PrintMemoryLeaks", &CV_r_printmemoryleaks, 0);
  iConsole->Register("r_ReleaseAllResourcesOnExit", &CV_r_releaseallresourcesonexit, 0);

  iConsole->Register("r_VegetationSpritesAlphaBlend",&CV_r_VegetationSpritesAlphaBlend,0);
  iConsole->Register("r_VegetationSpritesNoBend",&CV_r_VegetationSpritesNoBend,2);
  iConsole->Register("r_ZPassOnly",&CV_r_ZPassOnly,0,VF_CHEAT);
  iConsole->Register("r_VegetationSpritesNoGen",&CV_r_VegetationSpritesNoGen,0);
  iConsole->Register("r_VegetationSpritesGenAlways",&CV_r_VegetationSpritesGenAlways,0);
  iConsole->Register("r_VegetationSpritesTexRes",&CV_r_VegetationSpritesTexRes,64);

	iConsole->Register("r_ShowVideoMemoryStats", &CV_r_ShowVideoMemoryStats, 0);
	iConsole->Register("r_ShowRenderTarget", &CV_r_ShowRenderTarget, 0, VF_CHEAT,
		"Displays special render targets - for debug purpose\n"
		"Usage: r_Log [0=off/1/2/3/4/5/6/7/8/9]\n"
		"	1: m_Text_ZTarget\n"
		"	2: m_Text_SceneTarget\n"
		"	3: m_Text_ScreenShadowMap[0]\n"
		"	4: m_Text_ScreenShadowMap[1]\n"
		"	5: m_Text_ScreenShadowMap[2]\n"
		"	6: gTexture\n"
		"	7: gTexture2\n"
		"	8: m_Text_ScatterLayer\n"
		"	9: pEnvTex->m_pTex->m_pTexture\n"
		"10: m_Text_LightInfo[0]\n"
    "11: SSAO render target\n"
    "16: Downscaled depth target for SSAO\n"
		);
	iConsole->Register("r_ShowRenderTarget_FullScreen", &CV_r_ShowRenderTarget_FullScreen, 0,VF_CHEAT);
  iConsole->Register("r_ShowLightBounds", &CV_r_ShowLightBounds, 0, VF_CHEAT,
    "Display light bounds - for debug purpose\n"
    "Usage: r_ShowLightBounds [0=off/1=on]");
	iConsole->Register("r_MergeRenderChunksForDepth", &CV_r_MergeRenderChunksForDepth, 0);

	iConsole->Register("r_RAM", &CV_r_RAM, 1, 0, "Toggles Realtime Ambient Maps" );
	iConsole->Register("r_UseGSParticles", &CV_r_UseGSParticles, 1, 0, 
		"Toggles use of geometry shader particles (DX10 only, changing at runtime is supported)."
		"Usage: r_UseGSParticles [0/1=default]");
	iConsole->Register("r_Force3DcEmulation", &CV_r_Force3DcEmulation,2,0, 
		"Specifies renderer behavior for the 3Dc (compressed normal maps) emulation (DX9 only).\n"
		"(Emulation: DXT5, less quality but same memory requirements)\n"
		"0=use only if driver doesn't support 3Dc\n"
		"1=enforce on any hardware\n"
		"2=use only if hardware doesn't support 3Dc\n"
		"Usage: r_Force3DcEmulation [0/1/2]",ChangeForce3DcEmulation);
	
	iConsole->Register("r_ZFightingDepthScale", &CV_r_ZFightingDepthScale, 0.995f, VF_CHEAT, "Controls anti z-fighting measures in shaders (scaling homogeneous z)." );
	iConsole->Register("r_ZFightingExtrude", &CV_r_ZFightingExtrude, 0.001f, VF_CHEAT, "Controls anti z-fighting measures in shaders (extrusion along normal in world units)." );


  iConsole->AddCommand("r_PrecacheShaders", &ShadersPrecache);
  iConsole->AddCommand("r_PrecacheShaderList", &ShadersPrecacheList);
  iConsole->AddCommand("r_OptimiseShaders", &ShadersOptimise);
  iConsole->AddCommand("r_MergeShaders", &ShadersMerge,VF_CHEAT);

  iConsole->AddCommand("r_FixMaterials", &FixMaterials);

	iConsole->Register("r_TextureCompressor", &CV_r_TextureCompressor, 1, VF_DUMPTODISK,
		"Defines which texture compressor is used (fallback is DirectX)\n"
		"Usage: r_TextureCompressor [0/1]\n"
		"0 uses nvDXT, 1 uses Squish if possible");

	// ----------------------------------------------------------------

  m_cClearColor = ColorF(0,0,0,128.0f/255.0f);		// 128 is default GBuffer value
  m_LogFile = NULL;
  m_TexGenID = 1;
  m_VSync = CV_r_vsync;
  m_Features = 0;
#if !defined(XENON) && !defined(PS3)
	m_bNVLibInitialized=false;
	m_bDriverHasActiveMultiGPU=false;
#endif

  //init_math();
  
  m_nFrameID = (unsigned short)-2;

  m_bPauseTimer=0;
  m_fPrevTime=-1.0f;

//  m_RP.m_ShaderCurrTime = 0.0f;

  m_CurFontColor = Col_White;

  m_bUseHWSkinning = CV_r_usehwskinning != 0;

  m_vMeshScale = Vec4(1,1,1,1);
	m_bSwapBuffers = true;
  m_FS.m_bEnable = true;

#if defined(_DEBUG) && defined(WIN32)
  if (CV_r_printmemoryleaks)
  {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  }
#endif

  m_pREPostProcess=0;
  m_pDefaultFont = NULL;
  m_bUseZpass = CV_r_usezpass != 0;  
	m_nCloudShadowTexId = 0;
  m_vCloudShadowSpeed.Set(0,0,0);
	m_pSkyLightRenderParams = 0;
	m_fogVolumeContibutions.reserve( 1024 );

#ifndef EXCLUDE_SCALEFORM_SDK
	m_pSFDrawParams = 0;
#endif

  m_nGPUs = 1;

  //assert(!(FOB_MASK_AFFECTS_MERGING & 0xffff));
  //assert(sizeof(CRenderObject) == 256);
#ifndef WIN64
  STATIC_CHECK(sizeof(CRenderObject) == 256, CRenderObject);
#endif
  STATIC_CHECK(!(FOB_MASK_AFFECTS_MERGING & 0xffff), FOB_MASK_AFFECTS_MERGING);

	g_pSDynTexture_PoolAlloc = new SDynTexture_PoolAlloc;

  //m_RP.m_VertPosCache.m_nBufSize = 500000 * sizeof(Vec3);
  //m_RP.m_VertPosCache.m_pBuf = new byte [gRenDev->m_RP.m_VertPosCache.m_nBufSize];
/*
	m_RP.m_mLastWaterMatrix.SetIdentity();
*/
	m_RP.m_vLastWaterFrustumPoints[0] = 
		m_RP.m_vLastWaterFrustumPoints[1] = 
		m_RP.m_vLastWaterFrustumPoints[2] = 
		m_RP.m_vLastWaterFrustumPoints[3] = Vec3( 0.0f, 0.0f, 0.0f );

  m_pDefaultMaterial = NULL;
  m_pTerrainDefaultMaterial = NULL;

	m_CameraProjZeroMatrix.SetIdentity();
	m_InvCameraProjMatrix.SetIdentity();
	m_CameraZeroMatrix.SetIdentity();

	m_bMarkForDepthMapCapture = false;
}

CRenderer::~CRenderer()
{
	delete g_pSDynTexture_PoolAlloc;
  gRenDev = NULL;
}

void CRenderer::Release()
{
  delete this;
}

//////////////////////////////////////////////////////////////////////
void CRenderer::AddListener(IRendererEventListener *pRendererEventListener)
{
	stl::push_back_unique(m_listRendererEventListeners,pRendererEventListener);
}

//////////////////////////////////////////////////////////////////////
void CRenderer::RemoveListener(IRendererEventListener *pRendererEventListener)
{
	stl::find_and_erase(m_listRendererEventListeners,pRendererEventListener);
}

//////////////////////////////////////////////////////////////////////
/*bool CRenderer::FindImage(CImage *image)
{
	for (ImageIt i=m_imageList.begin();i!=m_imageList.end();i++)
	{
		CImage *ci=(*i);

		if (ci==image) 
			return (true);
	} //i

	return (false);
} */

//////////////////////////////////////////////////////////////////////
/*CImage *CRenderer::FindImage(const char *filename)
{

	ImageIt istart=m_imageList.begin();
	ImageIt iend=m_imageList.end();

	for (ImageIt i=m_imageList.begin();i!=iend;i++)
	{
		CImage *ci=(*i);

		if (stricmp(ci->GetName(),filename)==0) 
			return (ci);
	} //i

	return (NULL);
} */

//////////////////////////////////////////////////////////////////////
/*void CRenderer::AddImage(CImage *image)
{
	m_imageList.push_back(image);
} */

//////////////////////////////////////////////////////////////////////
//void CRenderer::ShowFps(const char *command/* =NULL */)
/*{
	if (!command) 
		return;
	if (stricmp(command,"true")==0) 
		m_showfps=true;
	else
	if (stricmp(command,"false")==0) 
		m_showfps=false;	
	else
	  iConsole->Help("ShowFps");
} */

//////////////////////////////////////////////////////////////////////
void CRenderer::TextToScreenColor(int x, int y, float r, float g, float b, float a, const char * format, ...)
{
//  if(!cVars->e_text_info)
  //  return;

  char buffer[512];
  va_list args;
  va_start(args, format);
	if (vsnprintf(buffer,sizeof(buffer),format, args) == -1)
		buffer[sizeof(buffer)-1] = 0;
  va_end(args);

  WriteXY((int)(0.01f*800*x), (int)(0.01f*600*y), 1, 1, r,g,b,a, buffer);
}

//////////////////////////////////////////////////////////////////////
void CRenderer::TextToScreen(float x, float y, const char * format, ...)
{
//  if(!cVars->e_text_info)
  //  return;

  char buffer[512];
  va_list args;
  va_start(args, format);
	if (vsnprintf(buffer,sizeof(buffer),format, args) == -1)
		buffer[sizeof(buffer)-1] = 0;
  va_end(args);

  WriteXY((int)(0.01f*800*x), (int)(0.01f*600*y), 1, 1, 1,1,1,1, buffer);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::Draw2dText( float posX,float posY,const char *szText,SDrawTextInfo &ti )
{
  // Check for the presence of a D3D device
  if (!iSystem)
    return;

 	if (!m_pDefaultFont)
	{
		ICryFont *pCryFont = iSystem->GetICryFont();
		if (!pCryFont)
			return;
		m_pDefaultFont = pCryFont->GetFont("default");
	}
	if (!m_pDefaultFont)
		return;
	IFFont *pFont = m_pDefaultFont;

  float r = CLAMP( ti.color[0], 0.0f, 1.0f);
  float g = CLAMP( ti.color[1], 0.0f, 1.0f);
  float b = CLAMP( ti.color[2], 0.0f, 1.0f);
  float a = CLAMP( ti.color[3], 0.0f, 1.0f);

  pFont->SetColor(ColorF(r,g,b,a));
  pFont->SetCharWidthScale(1);

  if (ti.flags & eDrawText_FixedSize)
  {
    pFont->UseRealPixels(true);
    pFont->SetSize( vector2f(12*ti.xscale,12*ti.yscale) );
    pFont->SetSameSize(false);
		posX = ScaleCoordX(posX);
		posY = ScaleCoordY(posY);
  }
  else
  {
    //		pFont->SetSameSize(true);
    //	pFont->SetSize( vector2f(16*ti.xscale,12*ti.yscale) );

    pFont->UseRealPixels(false);
    pFont->SetSameSize(true);
    pFont->SetCharWidthScale(1.5f / 3.0f);
    pFont->SetSize(vector2f(12*ti.xscale,12*ti.yscale));
  }

  pFont->SetColor(ColorF(r,g,b,a));
  //pFont->SetEffect("buttonfocus");
  //pFont->SetCharWidthScale(2.0f/3.0f);

	if (ti.flags & eDrawText_Center)
		posX -= pFont->GetTextSize(szText).x * 0.5f;
	else if (ti.flags & eDrawText_Right)
		posX -= pFont->GetTextSize(szText).x;

  pFont->DrawString(posX,posY,szText);
}

void CRenderer::PrintToScreen(float x, float y, float size, const char *buf)
{
  SDrawTextInfo ti;
  ti.xscale = size*0.5f/8;
  ti.yscale = size*1.f/8;
  ti.color[0] = 1; ti.color[1] = 1; ti.color[2] = 1; ti.color[3] = 1;
  Draw2dText( x,y,buf,ti );
}

void CRenderer::WriteXY( int x, int y, float xscale, float yscale, float r, float g, float b, float a, const char *format, ...)
{
  //////////////////////////////////////////////////////////////////////
  // Write a string to the screen
  //////////////////////////////////////////////////////////////////////

  va_list args;
  char buffer[512];

  // Check for the presence of a D3D device
  // Format the string
  va_start(args, format);
	if (vsnprintf(buffer,sizeof(buffer),format, args) == -1)
		buffer[sizeof(buffer)-1] = 0;
  va_end(args);

  SDrawTextInfo ti;
  ti.xscale = xscale;
  ti.yscale = yscale;
  ti.color[0] = r;
  ti.color[1] = g;
  ti.color[2] = b;
  ti.color[3] = a;
  Draw2dText( (float)x,(float)y,buffer,ti );
}



//////////////////////////////////////////////////////////////////////
void CRenderer::DrawLabel(Vec3 pos, float font_size, const char * label_text, ...)
{
  if (label_text && m_listMessages.Count()<4096)
  {
    text_info_struct m;

    va_list args;
    va_start(args, label_text);
		if (vsnprintf(m.mess,sizeof(m.mess),label_text, args) == -1)
			m.mess[sizeof(m.mess)-1] = 0;
    va_end(args);

    m.pos = pos;
    m.font_size = font_size;

    m.fColor[0] = m.fColor[1] = m.fColor[2] = m.fColor[3] = 1;
    m.bFixedSize = true;
    m.bCenter = false;
		m.b2D = false;

		m.nTextureId=-1;

    m_listMessages.Add(m);
  }
}

//////////////////////////////////////////////////////////////////////
void CRenderer::DrawLabelEx(Vec3 pos, float font_size, float * pfColor, bool bFixedSize, bool bCenter, const char * label_text, ...)
{
  if(m_listMessages.Count()<1000)
  {
    text_info_struct m;

    va_list args;
    va_start(args, label_text);
		if (vsnprintf(m.mess,sizeof(m.mess),label_text, args) == -1)
			m.mess[sizeof(m.mess)-1] = 0;
    va_end(args);

    m.pos = pos;
    m.font_size = font_size;

    if(pfColor)
      memcpy(m.fColor, pfColor, sizeof(m.fColor));
    else
      m.fColor[0] = m.fColor[1] = m.fColor[2] = m.fColor[3] = 1;

    m.bFixedSize = bFixedSize;
    m.bCenter = bCenter;
		m.b2D = false;

		m.nTextureId=-1;

    m_listMessages.Add(m);
  }
}

//////////////////////////////////////////////////////////////////////
void CRenderer::Draw2dLabel( float x,float y, float font_size, float * pfColor,bool bCenter, const char * label_text, ...)
{
  if (m_listMessages.Count()<1000 && !gEnv->pSystem->IsDedicated())
  {
    text_info_struct m;

    va_list args;
    va_start(args, label_text);
		if (vsnprintf(m.mess,sizeof(m.mess),label_text, args) == -1)
			m.mess[sizeof(m.mess)-1] = 0;
    va_end(args);

    m.pos.x = x;
		m.pos.y = y;
		m.pos.z = 0.5f;
    m.font_size = font_size;

    if(pfColor)
      memcpy(m.fColor, pfColor, sizeof(m.fColor));
    else
      m.fColor[0] = m.fColor[1] = m.fColor[2] = m.fColor[3] = 1;

    m.bFixedSize = true;
    m.bCenter = bCenter;
		m.b2D = true;

		m.nTextureId=-1;

    m_listMessages.Add(m);
  }
}

//////////////////////////////////////////////////////////////////////
void CRenderer::DrawLabelImage(const Vec3 &vPos,float fImageSize,int nTextureId)
{
  if (m_listMessages.Count()<1000)
  {
    text_info_struct m;    

		memset(m.mess,0,32);
    m.pos = vPos;
    m.font_size = fImageSize;

    m.fColor[0] = m.fColor[1] = m.fColor[2] = m.fColor[3] = 1;
    m.bFixedSize = false;
    m.bCenter = false;
		m.b2D = false;

		m.nTextureId=nTextureId;

    m_listMessages.Add(m);
  }
}

//////////////////////////////////////////////////////////////////////
void CRenderer::FlushTextMessages()
{
	if (m_listMessages.Count() == 0)
		return;

	if (gEnv->pSystem->IsDedicated())
	{
		m_listMessages.Clear();
		return;
	}

	//if (GetIRenderAuxGeom())
	//	GetIRenderAuxGeom()->Flush();

  EnableFog(false);
	int vx,vy,vw,vh;
	GetViewport( &vx,&vy,&vw,&vh );

	for(int i=0; i<m_listMessages.Count(); i++)
  {
    text_info_struct * pTextInfo = &m_listMessages[i];

    float sx,sy,sz;

		if (!pTextInfo->b2D)
		{
			float fDist = 1; //GetDistance(pTextInfo->pos,GetCamera().GetPosition());
      
			float K = GetCamera().GetFarPlane()/fDist;
      if(fDist>GetCamera().GetFarPlane()*0.5)
        pTextInfo->pos = GetCamera().GetPosition() + K*(pTextInfo->pos - GetCamera().GetPosition());

			ProjectToScreen( pTextInfo->pos.x, pTextInfo->pos.y, pTextInfo->pos.z, 
				&sx, &sy, &sz );
		}
		else
		{
			sx = (pTextInfo->pos.x) / vw * 100;
			sy = (pTextInfo->pos.y) / vh * 100;
			sz = pTextInfo->pos.z;
		}

    if(sx>0 && sx<100)
    if(sy>0 && sy<100)
    if(sz>0 && sz<1)
    {
      // calculate size
      float sizeX;
			float sizeY;
      if (pTextInfo->bFixedSize)
			{
				sizeX = pTextInfo->font_size;
				sizeY = pTextInfo->font_size;
        //sizeX = pTextInfo->font_size * 800.0f/vw;
				//sizeY = pTextInfo->font_size * 500.0f/vh;
			}
      else
			{
        sizeX = sizeY = (1.0f-(float)(sz))*32.f*pTextInfo->font_size;
				sizeX *= 0.5f;
			}

			if (pTextInfo->mess[0])
			{
	      // print
				SDrawTextInfo ti;
				ti.color[0] = pTextInfo->fColor[0];
				ti.color[1] = pTextInfo->fColor[1];
				ti.color[2] = pTextInfo->fColor[2];
				ti.color[3] = pTextInfo->fColor[3];
				if (pTextInfo->bFixedSize)
					ti.flags |= eDrawText_FixedSize;
				if (pTextInfo->bCenter)
					ti.flags |= eDrawText_Center;
				ti.xscale = sizeX;
				ti.yscale = sizeY;
				Draw2dText( 0.01f*800*sx,0.01f*600*sy,pTextInfo->mess,ti );
				//[Timur] 
				/*
	      WriteXY(iConsole->GetFont(),
	        (int)(0.01f*vw*sx),(int)(0.01f*vh*sy), sizeX, 1.f*sizeY,
	        pTextInfo->fColor[0],
	        pTextInfo->fColor[1],
	        pTextInfo->fColor[2],
	        pTextInfo->fColor[3],
	        pTextInfo->mess);
				*/
			}			

			if (pTextInfo->nTextureId>=0)
			{
        SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);
        SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
				SetTexture(pTextInfo->nTextureId);				

			  Matrix44	mat;
			  GetModelViewMatrix(mat.GetData());
				Vec3		vRight,vUp,vFront;	

				//CELL_CHANGED_BY_IVO
				//vRight(mat.cell(0), mat.cell(4), mat.cell(8));
				//vUp   (mat.cell(1), mat.cell(5), mat.cell(9)); 				
				vRight( mat(0,0), mat(1,0), mat(2,0) );
				vUp   ( mat(0,1), mat(1,1), mat(2,1) ); 				

				//DrawQuad(vRight,vUp,pTextInfo->pos,true);		
				DrawQuad(vRight,vUp,pTextInfo->pos,2);		
        SetState(GS_DEPTHWRITE);
			}
    }
  }

  m_listMessages.Clear();
}

#if !defined(PS3) && !defined(LINUX)
#pragma pack (push)
#pragma pack (1)
typedef struct
{
  unsigned char  id_length, colormap_type, image_type;
  unsigned short colormap_index, colormap_length;
  unsigned char  colormap_size;
  unsigned short x_origin, y_origin, width, height;
  unsigned char  pixel_size, attributes;
}	TargaHeader_t;
#pragma pack (pop)
#else
typedef struct
{
	unsigned char  id_length, colormap_type, image_type;
	unsigned short colormap_index _PACK, colormap_length _PACK;
	unsigned char  colormap_size;
	unsigned short x_origin _PACK, y_origin _PACK, width _PACK, height _PACK;
	unsigned char  pixel_size, attributes;
}	TargaHeader_t;
#endif

bool	CRenderer::SaveTga(unsigned char *sourcedata,int sourceformat,int w,int h,const char *filename,bool flip)
{
  //assert(0);
//  return CImage::SaveTga(sourcedata,sourceformat,w,h,filename,flip);
	
	if (flip)
	{
		int size=w*(sourceformat/8);
		unsigned char *tempw=new unsigned char [size];
		unsigned char *src1=sourcedata;		
		unsigned char *src2=sourcedata+(w*(sourceformat/8))*(h-1);
		for (int k=0;k<h/2;k++)
		{
			memcpy(tempw,src1,size);
			memcpy(src1,src2,size);
			memcpy(src2,tempw,size);
			src1+=size;
			src2-=size;
		}
		delete [] tempw;
	}
	

	unsigned char *oldsourcedata=sourcedata;

	if (sourceformat==FORMAT_8_BIT)
	{

		unsigned char *desttemp=new unsigned char [w*h*3];
		memset(desttemp,0,w*h*3);

		unsigned char *destptr=desttemp;
		unsigned char *srcptr=sourcedata;

		unsigned char col;

		for (int k=0;k<w*h;k++)
		{			
			col=*srcptr++;
			*destptr++=col;		
			*destptr++=col;	
			*destptr++=col;		
		}
		
		sourcedata=desttemp;

		sourceformat=FORMAT_24_BIT;
	}

	TargaHeader_t header;

	memset(&header, 0, sizeof(header));
	header.image_type = 2;
	header.width = w;
	header.height = h;
	header.pixel_size = sourceformat;
  	
	unsigned char *data = new unsigned char[w*h*(sourceformat>>3)];
	unsigned char *dest = data;
	unsigned char *source = sourcedata;

	//memcpy(dest,source,w*h*(sourceformat>>3));
	
	for (int ax = 0; ax < h; ax++)
	{
		for (int by = 0; by < w; by++)
		{
			unsigned char r, g, b, a;
			r = *source; source++;
			g = *source; source++;
			b = *source; source++;
			if (sourceformat==FORMAT_32_BIT) 
			{
				a = *source; source++;
			}				
			*dest = b; dest++;
			*dest = g; dest++;
			*dest = r; dest++;
			if (sourceformat==FORMAT_32_BIT) 
			{
				*dest = a; dest++;
			}
		}
	}
	

	FILE *f = fxopen(filename,"wb");
	if (!f)
	{		
		//("Cannot save %s\n",filename);
		delete [] data;
		return (false);
	}

	if (!fwrite(&header, sizeof(header),1,f))
	{		
		//CLog::LogToFile("Cannot save %s\n",filename);
		delete [] data;
		fclose(f);
		return (false);
	}

	if (!fwrite(data, w*h*(sourceformat>>3),1,f))
	{		
		//CLog::LogToFile("Cannot save %s\n",filename);
		delete [] data;
		fclose(f);
		return (false);
	}

	fclose(f);	

	delete [] data;
	if (sourcedata!=oldsourcedata)
		delete [] sourcedata;

	return (true);	  
}

//================================================================



////////////////////////////////////////////////////////////////////////////////////////////////////

//#include "../Common/Character/CryModel.h"

void CRenderer::FreeResources(int nFlags)
{
  iLog->Log("*** Clearing render resources ***");

  if (nFlags == FRR_ALL)
    CRenderMesh::ShutDown();

  uint i;

  if (nFlags & FRR_SHADERS)
    gRenDev->m_cEF.ShutDown();
  if (nFlags & FRR_SYSTEM)
  {
    for (i=0; i<m_RP.m_Objects.Num(); i++)
    {
      CRenderObject *obj = m_RP.m_Objects[i];
      if (!obj)
        continue;
      delete obj;
      m_RP.m_Objects[i] = NULL;
    }
    m_RP.m_Objects.Free();
    SAFE_DELETE_ARRAY(m_RP.m_ObjectsPool);
    for (i=0; i<m_RP.m_TempObjects.Num(); i++)
    {
      CRenderObject *obj = m_RP.m_TempObjects[i];
      if (!obj)
        continue;
      if (i >= m_RP.m_nNumObjectsInPool)
        delete obj;
      m_RP.m_TempObjects[i] = NULL;
    }
    m_RP.m_TempObjects.Free();
    EF_PipelineShutdown();
  }
  if (nFlags == FRR_ALL)
    CRendElement::ShutDown();

  if (nFlags & FRR_TEXTURES)
    CTexture::ShutDown();

  if ((nFlags & FRR_RESTORE) && !(nFlags & FRR_SYSTEM))
    m_cEF.mfInit();
}

EScreenAspectRatio CRenderer::GetScreenAspect(int nWidth, int nHeight)
{
  EScreenAspectRatio eSA = eAspect_Unknown;

  float fNeed16_9 = 16.0f / 9.0f;
  float fNeed16_10 = 16.0f / 10.0f;
  float fNeed4_3 = 4.0f / 3.0f;

  float fCur = (float)nWidth / (float)nHeight;
  if (fabs(fCur-fNeed16_9) < 0.1f)
    eSA = eAspect_16_9;

  if (fabs(fCur-fNeed4_3) < 0.1f)
    eSA = eAspect_4_3;

  if (fabs(fCur-fNeed16_10) < 0.1f)
    eSA = eAspect_16_10;

  return eSA;
}

bool CRenderer::WriteTGA(byte *dat, int wdt, int hgt, const char *name, int src_bits_per_pixel, int dest_bits_per_pixel)
{
	return ::WriteTGA((byte*)dat, wdt, hgt, name, src_bits_per_pixel,dest_bits_per_pixel);
}

const char *sourceFile;
unsigned int sourceLine;


bool CRenderer::WriteDDS(byte *dat, int wdt, int hgt, int Size, const char *nam, ETEX_Format eFDst, int NumMips)
{
	bool bRet=true;

  byte *data = NULL;
  if (Size == 3)
  {
    data = new byte[wdt*hgt*4];
    for (int i=0;  i<wdt*hgt; i++)
    {
      data[i*4+0] = dat[i*3+0];
      data[i*4+1] = dat[i*3+1];
      data[i*4+2] = dat[i*3+2];
      data[i*4+3] = 255;
    }
    dat = data;
  }
  char name[256];
  fpStripExtension(nam, name);
  strcat(name, ".dds");

  bool bMips = false;
  if (NumMips != 1)
    bMips = true;
  int nDxtSize;
  byte *dst = CTexture::Convert(dat, wdt, hgt, NumMips, eTF_A8R8G8B8, eFDst, NumMips, nDxtSize, true);
  if (dst)
  {
    ::WriteDDS(dst, wdt, hgt, 1, name, eFDst, NumMips, eTT_2D);
    delete [] dst;
  }
  if (data)
    delete [] data;

	return bRet;
}

string *CRenderer::EF_GetShaderNames(int& nNumShaders)
{
  nNumShaders = m_cEF.m_ShaderNames.size();
  return &m_cEF.m_ShaderNames[0];
}

IShader *CRenderer::EF_LoadShader (const char *name, int flags, uint nMaskGen)
{
#ifdef NULL_RENDERER
  return m_cEF.m_DefaultShader;
#else
  return m_cEF.mfForName(name, flags, NULL, nMaskGen);
#endif
}

SShaderItem CRenderer::EF_LoadShaderItem (const char *szName, bool bShare, int flags, SInputShaderResources *Res, uint nMaskGen)
{
  LOADING_TIME_PROFILE_SECTION(iSystem);
  
#ifdef NULL_RENDERER
	return m_cEF.m_DefaultShaderItem;
#else
  return gRenDev->m_cEF.mfShaderItemForName(szName, bShare, flags, Res, nMaskGen);
#endif
}

//////////////////////////////////////////////////////////////////////////
bool CRenderer::EF_ReloadFile (const char *szFileName)
{
	// Replace .tif extensions with .dds extensions.
	char realName[MAX_PATH + 1];
	int nameLength = __min(strlen(szFileName), size_t(MAX_PATH));
	memcpy(realName, szFileName, nameLength);
	realName[nameLength] = 0;
	const char* tifExtension = "tif";
	const char* ddsExtension = "dds";
	const int extensionLength = 3;

#if defined(_WIN32)
	if (nameLength >= extensionLength && (memcmp(realName + nameLength - extensionLength, tifExtension, extensionLength) == 0 ||
		memcmp(realName + nameLength - extensionLength, ddsExtension, extensionLength) == 0))
	{
		// Usually reloading a dds will automatically trigger the resource compiler if necessary to
		// compile the .tif into a .dds. However, this does not happen if texture streaming is
		// enabled, so we explicitly run the resource compiler here.
		memcpy(realName + nameLength - extensionLength, tifExtension, extensionLength);

		char unixNameBuffer[256];	
		fpConvertDOSToUnixName(unixNameBuffer, realName);

		char gameFolderPath[256];
		strcpy(gameFolderPath, PathUtil::GetGameFolder());
		int gameFolderPathLength = strlen(gameFolderPath);
		if (gameFolderPathLength > 0 && gameFolderPath[gameFolderPathLength - 1] == '\\')
		{
			gameFolderPath[gameFolderPathLength - 1] = '/';
		}
		else if (gameFolderPathLength > 0 || gameFolderPath[gameFolderPathLength - 1] != '/')
		{
			gameFolderPath[gameFolderPathLength++] = '/';
			gameFolderPath[gameFolderPathLength] = 0;
		}

		char* gameRelativePath = unixNameBuffer;
		if (strlen(gameRelativePath) >= gameFolderPathLength && memcmp(gameRelativePath, gameFolderPath, gameFolderPathLength) == 0)
			gameRelativePath += gameFolderPathLength;

#ifndef XENON
		CResourceCompilerHelper().ProcessIfNeeded(gameRelativePath, true);
#endif
	}
#endif //defined(_WIN32)

	if (nameLength >= extensionLength && memcmp(realName + nameLength - extensionLength, tifExtension, extensionLength) == 0)
		memcpy(realName + nameLength - extensionLength, ddsExtension, extensionLength);

  char nmf[512];
  char drn[512];
  char drv[16];
  char dirn[512];
  char fln[128]; 
  char extn[16];
  _splitpath(realName, drv, dirn, fln, extn);
  strcpy(drn, drv);
  strcat(drn, dirn);
  strcpy(nmf, fln);
  strcat(nmf, extn);
  if (!stricmp(extn, ".cfx") || (!CV_r_shadersignoreincludeschanging && !stricmp(extn, ".cfi")))
  {
    gRenDev->m_cEF.m_Bin.InvalidateCache();
    return gRenDev->m_cEF.mfReloadFile(drn, nmf, FRO_SHADERS);
  }
  if (!stricmp(extn, ".tif") || !stricmp(extn, ".tga") || !stricmp(extn, ".pcx") || !stricmp(extn, ".dds") || !stricmp(extn, ".jpg") || !stricmp(extn, ".gif") || !stricmp(extn, ".bmp"))
    return CTexture::ReloadFile(realName, FRO_TEXTURES);
  return false;
}

void CRenderer::EF_ReloadShaderFiles (int nCategory)
{
  //gRenDev->m_cEF.mfLoadFromFiles(nCategory);
}

void CRenderer::EF_ReloadTextures ()
{
  CTexture::ReloadTextures();
}

bool CRenderer::EF_ScanEnvironmentCM (const char *name, int size, Vec3& Pos)
{
  return CTexture::ScanEnvironmentCM(name, size, Pos);
}

int CRenderer::EF_LoadLightmap (const char *name)
{
  CTexture *tp = (CTexture *)EF_LoadTexture(name, FT_STATE_CLAMP | FT_NOMIPS, eTT_2D);
  if (tp->IsTextureLoaded())
    return tp->GetID();
  else
    return -1;
}

ITexture *CRenderer::EF_GetTextureByID(int Id)
{
  if (Id > 0)
  {
    CTexture *tp = CTexture::GetByID(Id);
    if (tp)
      return tp;
  }
  return NULL;
}

ITexture *CRenderer::EF_LoadTexture(const char* nameTex, uint flags, byte eTT, float fAmount1, float fAmount2)
{
  LOADING_TIME_PROFILE_SECTION(iSystem);

#if defined (NULL_RENDERER)
  if (CTexture::m_Text_NoTexture)
    return CTexture::m_Text_NoTexture;
  else
#endif
  {
		const char *ext = fpGetExtension(nameTex);
		if(ext && stricmp(ext,".tif")==0)		// for TIF files, register by the dds file name (to not load it twice)
		{
			char name[256];
			fpStripExtension(nameTex, name);
			strcat(name, ".dds");

	    return CTexture::ForName(name, flags, eTF_Unknown, fAmount1, fAmount2);
		}
		else
      return CTexture::ForName(nameTex, flags, eTF_Unknown, fAmount1, fAmount2);
  }
}

void CRenderer::EF_StartEf ()
{
  int i, j;
  if (!SRendItem::m_RecurseLevel)
  {
    for (j=0; j<2; j++)
    {
      for (i=0; i<EFSLIST_NUM; i++)
      {
        SRendItem::m_RendItems[j][i].SetUse(0);
        SRendItem::m_StartRI[SRendItem::m_RecurseLevel][j][i] = 0;
      }
    }
    m_RP.m_NumVisObjects = 1;
    m_RP.m_TempObjects.SetUse(1);
    CRenderObject::m_Waves.SetUse(1);
    CRenderObject::m_ObjData.resize(0);

    EF_RemovePolysFromScene();
		m_fogVolumeContibutions.resize(0);
  }

  for (j=0; j<2; j++)
  {
    for (i=0; i<EFSLIST_NUM; i++)
    {
      SRendItem::m_StartRI[SRendItem::m_RecurseLevel][j][i] = SRendItem::m_RendItems[j][i].Num();
    }
  }
  m_RP.m_RejectedObjects.SetUse(0);
  EF_PushObjectsList();

  SRendItem::m_RecurseLevel++;
  EF_ClearLightsList();
}

// Hide shader template (exclude from list)
bool CRenderer::EF_HideTemplate(const char *name)
{
  uint i;

  CCryName nm = CCryName(name);

  for (i=0; i<m_HidedShaderTemplates.Num(); i++)
  {
    if (m_HidedShaderTemplates[i] == nm)
      return false;
  }
  m_HidedShaderTemplates.AddElem(nm);
  return true;
}

// UnHide shader template (include in list)
bool CRenderer::EF_UnhideTemplate(const char *name)
{
  uint i;

  CCryName nm = CCryName(name);

  for (i=0; i<m_HidedShaderTemplates.Num(); i++)
  {
    if (m_HidedShaderTemplates[i] == nm)
    {
      m_HidedShaderTemplates.Remove(i, 1);
      return true;
    }
  }
  return false;
}

// UnHide all shader templates (include in list)
bool CRenderer::EF_UnhideAllTemplates()
{
  m_HidedShaderTemplates.Free();
  return true;
}

uint CRenderer::EF_BatchFlags(SShaderItem& SH, CRenderObject *pObj)
{
  uint nFlags = FB_GENERAL;
  SShaderTechnique *pTech = SH.GetTechnique();
  SRenderShaderResources *pR = (SRenderShaderResources *)SH.m_pShaderResources;
  CShader *pS = (CShader *)SH.m_pShader;
  if (SRendItem::m_RecurseLevel == 1 && pTech)
  {
    if (!(pTech->m_Flags & FHF_POSITION_INVARIANT) && ((pTech->m_Flags & FHF_TRANSPARENT) || pObj->m_fAlpha<1.0f || (pR && pR->Opacity() < 1.0f)))
      nFlags |= FB_TRANSPARENT;

    if ((pTech->m_nTechnique[TTYPE_Z] > 0 && (!(pObj->m_ObjFlags & FOB_NO_Z_PASS) || pS->m_Flags2 & EF2_FORCE_ZPASS) ))
      nFlags |= FB_Z;

    if (pTech->m_nTechnique[TTYPE_DETAIL] > 0 && pR->m_Textures[EFTT_DETAIL_OVERLAY] && !(pS->m_Flags2 & EF2_DETAILBUMPMAPPING))
      nFlags |= FB_DETAIL;

    if ( CV_r_usemateriallayers && pObj->m_nMaterialLayers && !(pObj->m_ObjFlags& FOB_RENDER_INTO_SHADOWMAP) )
    {
      uint32 nResourcesNoDrawFlags = pR->GetMtlLayerNoDrawFlags();

      if( (pObj->m_nMaterialLayers & MTL_LAYER_BLEND_DYNAMICFROZEN) && !(nResourcesNoDrawFlags&MTL_LAYER_FROZEN) )
        nFlags |= FB_MULTILAYERS;

      if( (pObj->m_nMaterialLayers&MTL_LAYER_BLEND_CLOAK) && !(nResourcesNoDrawFlags&MTL_LAYER_CLOAK) )
      {
        nFlags &= ~FB_TRANSPARENT; 
        nFlags |= FB_REFRACTIVE | FB_MULTILAYERS; 
      }
      else
      if ( !(nFlags & FB_MULTILAYERS) && (pObj->m_nMaterialLayers&MTL_LAYER_BLEND_WET) && pTech->m_nTechnique[TTYPE_RAINPASS] > 0 && CRenderer::CV_r_rain)
      {
        // don't apply to decals
        if( !(pObj->m_ObjFlags & FOB_DECAL) && !(pS->m_Flags & EF_DECAL) )
        {
          // Skip if beyond rain max view dist
          if( 1.0f - min(1.0f, pObj->m_fDistance / CRenderer::CV_r_rain_maxviewdist) > 0.01f )
            nFlags |= FB_RAIN;
        }
      }
    }

    if (pTech->m_nTechnique[TTYPE_CAUSTICS] > 0 && CRenderer::CV_r_watercaustics)
      nFlags |= FB_CAUSTICS;

    if (pTech->m_nTechnique[TTYPE_GLOWPASS] > 0 && pR->Glow())
      nFlags |= FB_GLOW;

    if (pTech->m_nTechnique[TTYPE_CUSTOMRENDERPASS] > 0 && CRenderer::CV_r_customvisions && !(pObj->m_ObjFlags & FOB_VEGETATION) && pObj->m_nVisionParams) //SH.m_pShaderResources->GetGlow())
      nFlags |= FB_CUSTOM_RENDER;

    if (pR && (pR->m_ResFlags & MTL_FLAG_SCATTER))
      nFlags |= FB_SCATTER;

    if (SH.m_nPreprocessFlags && !(m_RP.m_PersFlags & RBPF_SHADOWGEN))
      nFlags |= FB_PREPROCESS;

    if (pTech->m_nTechnique[TTYPE_MOTIONBLURPASS] > 0 && pObj->m_nMotionBlurAmount )
      nFlags |= FB_MOTIONBLUR;

    if (CV_r_refraction && pS && (pS->m_Flags & EF_REFRACTIVE))
      nFlags |= FB_REFRACTIVE;
  }
  else
  if( SRendItem::m_RecurseLevel > 1 && pTech && m_RP.m_PersFlags2&RBPF_MIRRORCAMERA)
  {
    if (!(pTech->m_Flags & FHF_POSITION_INVARIANT) && ((pTech->m_Flags & FHF_TRANSPARENT) || pObj->m_fAlpha<1.0f || (pR && pR->Opacity() < 1.0f)))
      nFlags |= FB_TRANSPARENT;
    if (pTech->m_nTechnique[TTYPE_CAUSTICS] > 0)
      nFlags |= FB_CAUSTICS;    
  }

  return nFlags;
}

#ifdef _DEBUG
static float sMatIdent[12] = 
{
  1,0,0,0,
  0,1,0,0,
  0,0,1,0
};
#endif

void CRenderer::EF_AddEf_NotVirtual (CRendElement *re, SShaderItem& SH, CRenderObject *obj, int nList, int nAW)
{
  assert(nList>0 && nList<EFSLIST_NUM);  
  
  // Ignore default object (overflow case)
  if (obj->m_Id<=0)
    return;
  if (!re || !SH.m_pShader)
    return;
  
  CShader *pSH = (CShader *)SH.m_pShader;
  if (pSH->m_Flags2 & EF2_NODRAW)
    return;

  if ((obj->m_ObjFlags & FOB_DECAL) || (pSH->m_Flags & EF_DECAL))
  {         
    if (m_RP.m_PersFlags & RBPF_SHADOWGEN)
      return;
  }
	
  if (pSH->m_Flags & EF_RELOADED)
  {
    pSH->m_Flags &= ~EF_RELOADED;
    SH.PostLoad();
  }

	if (m_RP.m_PersFlags & RBPF_SHADOWGEN)
	{
		if (SH.m_nTechnique<= 0)
			return;
	}

#ifdef _DEBUG
  if (memcmp(sMatIdent, obj->m_II.m_Matrix.GetData(), 3*4*4))
  {
    if (!(obj->m_ObjFlags & FOB_TRANS_MASK))
    {
      assert(0);
    }
  }
#endif

#ifdef XENON
  // On X360 we have to precache geometry before rendering since mesh updating
  // is not allowed with predicated tiling
  re->mfPrecache(SH);
#endif

  uint nBatchFlags = EF_BatchFlags(SH, obj);
  assert(!(nBatchFlags & FB_SCATTER) || nList == EFSLIST_GENERAL);

  if ((FOB_ONLY_Z_PASS & obj->m_ObjFlags) || CV_r_ZPassOnly)
  {
    if (!(nBatchFlags & (FB_TRANSPARENT | FB_SCATTER)))
      nBatchFlags = FB_Z;
  }

  if (nBatchFlags & FB_PREPROCESS)
    SRendItem::mfAdd(re, obj, SH, EFSLIST_PREPROCESS, 0, nBatchFlags);

  if (nBatchFlags & FB_REFRACTIVE)
  {
    nBatchFlags &= ~FB_Z;
    nList = EFSLIST_REFRACTPASS;
  }
  else
  if (nBatchFlags & FB_TRANSPARENT)
  {
    if (nList == EFSLIST_GENERAL)
      nList = EFSLIST_TRANSP;
  }
  //else
  //if (nBatchFlags & FB_SCATTER)
  //  nBatchFlags &= ~(FB_GENERAL | FB_Z);

  // make sure decals go into proper render list 
  // also, set additional shadow flag (i.e. reuse the shadow mask generated for underlying geometry)
  // TODO: specify correct render list and additional flags directly in the engine once non-material decal rendering support is removed! 
  if (((obj->m_ObjFlags & FOB_DECAL) || (pSH->m_Flags & EF_DECAL)))
  {         
    static ICVar *pVar = iConsole->GetCVar("e_decals");
    if( pVar && pVar->GetIVal() == 0)
      return;

    if (m_RP.m_PersFlags & RBPF_SHADOWGEN)
      return;

    if (pSH->m_Flags2 & EF2_FORCE_ZPASS)
      nBatchFlags |= FB_Z; 

    nList = EFSLIST_DECAL;
    obj->m_ObjFlags |= FOB_INSHADOW;
  }
  if (nList != EFSLIST_GENERAL) 
    nBatchFlags &= ~FB_Z;

  if (pSH->m_Flags2 & (EF2_FORCE_DRAWLAST|EF2_FORCE_ZPASS|EF2_FORCE_TRANSPASS|EF2_FORCE_GENERALPASS|EF2_FORCE_DRAWAFTERWATER|EF2_FORCE_WATERPASS|EF2_AFTERHDRPOSTPROCESS) )
  {
    // Force rendering in last place
    if (pSH->m_Flags2 & EF2_FORCE_DRAWLAST)
      obj->m_fSort -= 100000.0f;

    if( pSH->m_Flags2 & EF2_FORCE_ZPASS && !( (nBatchFlags & FB_REFRACTIVE) && (nBatchFlags &FB_MULTILAYERS) ) )  
      nBatchFlags |= FB_Z;

    if( pSH->m_Flags2 & EF2_FORCE_TRANSPASS ) 
      nList = EFSLIST_TRANSP;
    else
    if( pSH->m_Flags2 & EF2_FORCE_GENERALPASS )
      nList = EFSLIST_GENERAL;
    else
    if( pSH->m_Flags2 & EF2_FORCE_WATERPASS )
      nList = EFSLIST_WATER;
    if( pSH->m_Flags2 & EF2_AFTERHDRPOSTPROCESS ) 
      nList = EFSLIST_AFTER_HDRPOSTPROCESS;

    if( pSH->m_Flags2& EF2_FORCE_DRAWAFTERWATER )
      nAW = 1;
  }

  SRendItem::mfAdd(re, obj, SH, nList, nAW, nBatchFlags);
}

void CRenderer::EF_AddEf (CRendElement *re, SShaderItem& pSH, CRenderObject *obj, int nList, int nAW)
{
  EF_AddEf_NotVirtual (re, pSH, obj, nList, nAW);
}

CRendElement *CRenderer::EF_CreateRE( EDataType edt )
{
  CRendElement *re = NULL;

  switch(edt)
  {
    case eDATA_Mesh:
      re = new CREMesh;
      break;

    case eDATA_Imposter:
      re = new CREImposter;
      break;

		case eDATA_PanoramaCluster:
			re = new CREPanoramaCluster;
			break;

    case eDATA_HDRProcess:
      re = new CREHDRProcess;
      break;

    case eDATA_OcclusionQuery:
      re = new CREOcclusionQuery;
      break;

    case eDATA_Ocean:
#if !defined(XENON) && !defined(PS3)
      re = new CREOcean;
#else
      assert(0);
#endif
      break;

    case eDATA_Flare:
      re = new CREFlare;
      break;

    case eDATA_Cloud:
      re = new CRECloud;
      break;

    case eDATA_Sky:
      re = new CRESky;
      break;

		case eDATA_HDRSky:
			re = new CREHDRSky;
			break;

    case eDATA_Beam:
      re = new CREBeam;
      break;

    case eDATA_TerrainSector:
      re = new CRECommon;
      break;

    case eDATA_FarTreeSprites:
      re = new CREFarTreeSprites;
      break;

    case eDATA_Dummy:
      re = new CREDummy;
      break;

    case eDATA_PostProcess:
      if(!m_pREPostProcess)
      {
        re = new CREPostProcess;
        m_pREPostProcess=static_cast<CREPostProcess*>(re);
      }
      else
      {
        re=m_pREPostProcess;
      }
      break;

    case eDATA_ShadowMapGen:
      re = new CREShadowMapGen;
      break;
	
    case eDATA_TerrainDetailTextureLayers:
      re = new CRETerrainDetailTextureLayers;
      break;

    case eDATA_TerrainParticles:
      re = new CRETerrainParticles;
      break;
      
		case eDATA_FogVolume:
			re = new CREFogVolume;
			break;

		case eDATA_WaterVolume:
			re = new CREWaterVolume;
			break;

    case eDATA_WaterWave:
      re = new CREWaterWave;
      break;

    case eDATA_WaterOcean:
      re = new CREWaterOcean;
      break;

		case eDATA_VolumeObject:
			re = new CREVolumeObject;
			break;
  }
  return re;
}

void CRenderObject::Init()
{
  m_ObjFlags = 0;
  m_nRAEId=0;
  m_nObjDataId = -1;
  m_pRE = NULL;
  m_CustomData = NULL;
  m_DynLMMask = 0;
  m_RState = 0;
  m_fDistance = 0.0f;
  //m_cGBufferValue=0x80;	// default

  m_nMotionBlurAmount = 0;
  m_pCurrMaterial = 0;  
  m_nMaterialLayers = 0;

  m_nMDV = 0;
  m_fSort = 0;
  m_nWaveID = 0;
	m_nTextureID = -1;
	m_nTextureID1 = -1;
  m_pShadowCasters = NULL;

  m_II.m_AmbColor = ColorF(1.0f, 1.0f, 1.0f, 1.0f);
  m_AlphaRef = 0;
  m_fAlpha = 1.0f;
  m_FogVolumeContribIdx = (uint16)-1;
  m_pCharInstance = NULL;
  m_pLightImage = NULL;
  m_pID = NULL;
  m_pWeights = NULL;

  m_nScissorX1=m_nScissorX2=m_nScissorY1=m_nScissorY2=0;

  m_fRenderQuality = 1.f;

	m_pVisArea = NULL;
}


CRenderObject::~CRenderObject()
{
  /*if (m_ShaderParams && m_bShaderParamCreatedInRenderer)
  {
    m_bShaderParamCreatedInRenderer = false;
    delete m_ShaderParams;
  }*/
}

/*void CRenderObject::SetShaderFloat(const char *Name, float Val)
{
  uint i;

  if (!m_ShaderParams)
    m_ShaderParams = new TArray<SShaderParam>;
  for (i=0; i<m_ShaderParams->Num(); i++)
  {
    if (!stricmp(Name, m_ShaderParams->Get(i).m_Name))
      break;
  }
  if (i == m_ShaderParams->Num())
  {
    SShaderParam pr;
    pr.m_NameID = CCryName::create(Name);
    strncpy(pr.m_Name, Name, 32);
    m_ShaderParams->AddElem(pr);
  }
  SShaderParam *pr = &m_ShaderParams->Get(i);
  pr->m_Type = eType_FLOAT;
  pr->m_Value.m_Float = Val;
  m_bShaderParamCreatedInRenderer = true;
}*/

TArray<SWaveForm2> CRenderObject::m_Waves;
DynArray<SRenderObjData> CRenderObject::m_ObjData;

void CRenderObject::RemovePermanent()
{
  for (uint i=0; i<gRenDev->m_RP.m_Objects.Num(); i++)
  {
    if (gRenDev->m_RP.m_Objects[i] == this)
    {
      m_ObjFlags |= FOB_REMOVED;
      if (m_pRE && m_pRE->mfGetType() == eDATA_OcclusionQuery)
      {
        m_pRE->Release();
        m_pRE = NULL;
      }
      break;
    }
  }
}

void CRenderObject::AddWaves(SWaveForm2 **pWF)
{
  int n1, n2;
  SWaveForm2 *wf;
  if (!m_nWaveID)
  {
    n1 = m_Waves.Num();
    m_Waves.AddIndex(1);
    wf = &m_Waves[n1];
    m_nWaveID = n1;
    wf->m_Amp = 0;
    wf->m_Freq = 0;
    wf->m_Level = 0;
    wf->m_Phase = 0;
    wf->m_eWFType = eWF_Sin;

    n2 = m_Waves.Num();
    m_Waves.AddIndex(1);
    wf = &m_Waves[n2];
    wf->m_Amp = 0;
    wf->m_Freq = 0;
    wf->m_Level = 0;
    wf->m_Phase = 0;
    wf->m_eWFType = eWF_Sin;
  }
  if (pWF)
  {
    pWF[0] = &m_Waves[n1];
    pWF[1] = &m_Waves[n2];
  }
}

SRenderObjData *CRenderObject::GetObjData(bool bCreate)
{
  if (m_nObjDataId<0)
  {
    if (!bCreate)
      return NULL;
    m_nObjDataId = m_ObjData.size();
    m_ObjData.resize(m_nObjDataId+1);
    m_ObjData[m_nObjDataId].m_Constants.clear();
  }
  return &m_ObjData[m_nObjDataId];
}

CREMesh *CRenderer::EF_GetTempMeshRE()
{
  CREMesh **pREs = m_RP.m_TempREs.AddIndex(1);
  CREMesh *pRE = *pREs;
  if (!pRE)
  {
    pRE = new CREMesh;
    *pREs = pRE;
  }
  return pRE;
}

CRenderObject *CRenderer::EF_GetObject (bool bTemp, int num)
{
  CRenderObject *obj;
  static int sFrameWarn;
  uint i;

  if (num >= 0)
  {
    obj = m_RP.m_Objects[num];
    if (m_RP.m_NumVisObjects >= MAX_REND_OBJECTS)
      obj = NULL;
  }
  else
  {
    TArray <CRenderObject *> *Objs;
    if (bTemp)
      Objs = &m_RP.m_TempObjects;
    else
      Objs = &m_RP.m_Objects;
  	uint n = Objs->Num();
		if (m_RP.m_NumVisObjects >= MAX_REND_OBJECTS)
		{
      if (sFrameWarn != m_nFrameID)
      {
        sFrameWarn = m_nFrameID;
        iLog->Log("Error: CRenderer::EF_GetObject: Too many objects (> %d)\n", MAX_REND_OBJECTS);
      }
			obj = NULL;
		}
		else
		{
      if (bTemp && m_RP.m_RejectedObjects.Num())
      {
        int i;
        for (i=m_RP.m_RejectedObjects.Num()-1; i>=0; i--)
        {
          obj = m_RP.m_RejectedObjects[i];
          if (obj->m_VisId < m_RP.m_NumVisObjects)
            break;
        }
        if (i >= 0)
        {
          m_RP.m_RejectedObjects.SetUse(i);
          obj->Init();
          return obj;
        }
      }
      if (!bTemp)
      {
        for (i=1; i<Objs->Num(); i++)
        {
          if (!Objs->Get(i) || (Objs->Get(i)->m_ObjFlags & FOB_REMOVED))
            break;
        }
        if (i != Objs->Num())
          n = i;
        else
        {
          if (Objs->Num() != MAX_REND_OBJECTS-1)
    		    Objs->AddIndex(1);
					else
						assert(false);
        }
      }
      else
  		  Objs->AddIndex(1);
			obj = (*Objs)[n];
      if (!obj)
      {
        obj = new CRenderObject;
        (*Objs)[n] = obj;
      }
      obj->m_Id = n;
      obj->Init();
    }
  }
	if (obj)
	{
		m_RP.m_VisObjects[m_RP.m_NumVisObjects] = obj;
		obj->m_VisId = m_RP.m_NumVisObjects;
		m_RP.m_NumVisObjects++;
	}

  return obj;
}

void CRenderer::EF_AddSplash(Vec3 Pos, eSplashType eST, float fForce, int Id)
{
  uint i;

  fForce *= CRenderer::CV_r_oceansplashscale;
  SSplash *spl = NULL;
  //Id = 0;
  if (Id >= 0)
  {
    for (i=0; i<m_RP.m_Splashes.Num(); i++)
    {
      spl = &m_RP.m_Splashes[i];
      if (spl->m_Id == Id)
        break;
    }
    if (i == m_RP.m_Splashes.Num())
      spl = NULL;
  }
  if (!spl)
  {
    SSplash sp;
    sp.m_Id = Id;
    m_RP.m_Splashes.AddElem(sp);
    spl = &m_RP.m_Splashes[m_RP.m_Splashes.Num()-1];
    spl->m_fStartTime = iTimer->GetCurrTime();
  }
  spl->m_Pos = Pos;
  spl->m_fForce = fForce;
  spl->m_eType = eST;
  spl->m_fLastTime = iTimer->GetCurrTime();
}

void CRenderer::EF_UpdateSplashes(float fCurTime)
{
  uint i;

  for (i=0; i<m_RP.m_Splashes.Num(); i++)
  {
    SSplash *spl = &m_RP.m_Splashes[i];
    float fScaleFactor = 1.0f / (m_RP.m_RealTime - spl->m_fLastTime + 1.0f);
    if (fScaleFactor*spl->m_fForce < 0.1f)
    {
      m_RP.m_Splashes.Remove(i);
      i--;
    }
    spl->m_fCurRadius = (fCurTime-spl->m_fLastTime)*10.0f*4.0f;
  }
}

float CRenderer::EF_GetWaterZElevation(float fX, float fY)
{
#if !defined(XENON) && !defined(PS3)
  if (CREOcean::m_pStaticOcean)
    return CREOcean::m_pStaticOcean->GetWaterZElevation(fX, fY);
#endif
  I3DEngine *eng = (I3DEngine *)iSystem->GetI3DEngine();
  if (!eng)
    return 0;
  return eng->GetWaterLevel();
}

void CRenderer::EF_RemovePolysFromScene()
{  
  CREClientPoly2D::m_PolysStorage.SetUse(0);
  for (int i=0; i<4; i++)
  {
    CREClientPoly::m_PolysStorage[i].SetUse(0);
  }
  m_RP.m_SysVertexPool.SetUse(0);
  m_RP.m_SysIndexPool.SetUse(0);
  m_RP.m_Sprites.SetUse(0);
  m_RP.m_Polygons.SetUse(0);

	EF_RemoveParticlesFromScene();
}

CRenderObject *CRenderer::MergePolygonRO( CRenderObject* pRO )
{
	int i;

	if (pRO)
	{
		SRefSprite *rs = NULL;
		for (i=0; i<m_RP.m_Polygons.Num(); i++)
		{
			rs = &m_RP.m_Polygons[i];
			if (pRO == rs->m_pObj)
				break;
		}
		if (i == m_RP.m_Polygons.Num())
		{
			for (i=0; i<m_RP.m_Polygons.Num(); i++)
			{
				rs = &m_RP.m_Polygons[i];
				if (rs->m_pObj->m_DynLMMask == pRO->m_DynLMMask && rs->m_pObj->m_RState == pRO->m_RState && rs->m_pObj->m_fSort == pRO->m_fSort &&
					rs->m_pObj->m_II.m_AmbColor == pRO->m_II.m_AmbColor && rs->m_pObj->m_fAlpha == pRO->m_fAlpha &&
					(rs->m_pObj->m_nRAEId & 0xFF) == (pRO->m_nRAEId & 0xFF))
				{
					m_RP.m_RejectedObjects.AddElem(pRO);
					pRO = rs->m_pObj;
					break;
				}
			}
			if (i == m_RP.m_Polygons.Num())
			{
				SRefSprite s;
				s.m_pObj = pRO;
				//obj->m_Matrix.SetIdentity();
				m_RP.m_Polygons.AddElem(s);
				rs = &m_RP.m_Polygons[m_RP.m_Polygons.Num()-1];
			}
		}
	} 

	return pRO;
}

CRenderObject *CRenderer::EF_AddPolygonToScene(const SShaderItem& si, int numPts, const struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *verts, const SPipTangents *tangs, CRenderObject *obj, uint16 *inds, int ninds, int nAW, bool bMerge)
{
	if (bMerge)
		obj = MergePolygonRO( obj );
	
  int num = CREClientPoly::m_PolysStorage[SRendItem::m_RecurseLevel].Num();
  CREClientPoly::m_PolysStorage[SRendItem::m_RecurseLevel].GrowReset(1);

  CREClientPoly *pl = CREClientPoly::m_PolysStorage[SRendItem::m_RecurseLevel][num];
  if (!pl)
  {
    pl = new CREClientPoly;
    CREClientPoly::m_PolysStorage[SRendItem::m_RecurseLevel][num] = pl;
  }

  pl->m_Shader = si;
  pl->m_sNumVerts = numPts;
  pl->m_pObject = obj;
  pl->m_nAW = nAW;

  int nSize = m_VertexSize[VERTEX_FORMAT_P3F_COL4UB_TEX2F] * numPts;
  int nOffs = m_RP.m_SysVertexPool.Num();
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vt = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)m_RP.m_SysVertexPool.GrowReset(nSize);
  pl->m_nOffsVert = nOffs;
  for (int i=0; i<numPts; i++, vt++)
  {
    vt->xyz = verts[i].xyz;
    vt->st[0] = verts[i].st[0];
    vt->st[1] = verts[i].st[1];
    vt->color.dcolor = verts[i].color.dcolor;
  }
  if (tangs)
  {
    nSize = sizeof(SPipTangents) * numPts;
    nOffs = m_RP.m_SysVertexPool.Num();
    SPipTangents *vt = (SPipTangents *)m_RP.m_SysVertexPool.GrowReset(nSize);
    pl->m_nOffsTang = nOffs;
    for (int i=0; i<numPts; i++, vt++)
    {
      *vt = tangs[i];
    }
  }
  else
    pl->m_nOffsTang = -1;


  pl->m_nOffsInd = m_RP.m_SysIndexPool.Num();

  if (inds && ninds)
  {
    ushort* dstind = m_RP.m_SysIndexPool.GrowReset(ninds);
    memcpy(dstind, inds, ninds*sizeof(ushort));
    pl->m_sNumIndices = ninds;
  }
  else
  {
    ushort* dstind = m_RP.m_SysIndexPool.GrowReset((numPts-2) * 3);
    for (int i=0; i<numPts-2; i++, dstind+=3)
    {
      dstind[0] = 0;
      dstind[1] = i+1;
      dstind[2] = i+2;
    }
    pl->m_sNumIndices = (numPts-2) * 3;
  }

  return obj;
}

CRenderObject* CRenderer::EF_AddPolygonToScene(const SShaderItem& si, CRenderObject* obj, int numPts, int ninds, struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F*& verts, SPipTangents*& tangs, uint16*& inds, int nAW, bool bMerge)
{
	if (bMerge)
		obj = MergePolygonRO( obj );

	int num = CREClientPoly::m_PolysStorage[SRendItem::m_RecurseLevel].Num();
	CREClientPoly::m_PolysStorage[SRendItem::m_RecurseLevel].GrowReset(1);

	CREClientPoly *pl = CREClientPoly::m_PolysStorage[SRendItem::m_RecurseLevel][num];
	if (!pl)
	{
		pl = new CREClientPoly;
		CREClientPoly::m_PolysStorage[SRendItem::m_RecurseLevel][num] = pl;
	}

	pl->m_Shader = si;
	pl->m_pObject = obj;
  pl->m_nAW = nAW;

	//////////////////////////////////////////////////////////////////////////
	// allocate buffer space for caller to fill

	pl->m_sNumVerts = numPts;
	pl->m_nOffsVert = m_RP.m_SysVertexPool.Num();
	pl->m_nOffsTang = m_RP.m_SysVertexPool.Num() + sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F) * numPts;
	m_RP.m_SysVertexPool.GrowReset( (sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F) + sizeof(SPipTangents)) * numPts );
	verts = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F*) &m_RP.m_SysVertexPool[ pl->m_nOffsVert ];
	tangs = (SPipTangents*) &m_RP.m_SysVertexPool[ pl->m_nOffsTang ];

	pl->m_sNumIndices = ninds;
	pl->m_nOffsInd = m_RP.m_SysIndexPool.Num();
	inds = (uint16*) m_RP.m_SysIndexPool.GrowReset( ninds );

	//////////////////////////////////////////////////////////////////////////

	return obj;
}

void CRenderer::EF_AddPolyToScene2D(const SShaderItem& si, int nTempl, int numPts, const struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *verts)
{
  int num = CREClientPoly2D::m_PolysStorage.Num();
  CREClientPoly2D::m_PolysStorage.GrowReset(1);

  CShader *sh = (CShader *)si.m_pShader;

  CREClientPoly2D *pl = CREClientPoly2D::m_PolysStorage[num];
  if (!pl)
  {
    pl = new CREClientPoly2D;
    CREClientPoly2D::m_PolysStorage[num] = pl;
  }

  pl->m_Shader = si;
  pl->m_sNumVerts = numPts;

  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vt = pl->m_Verts;
  int i;
  for (i=0; i<numPts; i++, vt++)
  {
    *vt = verts[i];
  }

  byte *vrtind = pl->m_Indices;

  for (i=0; i<numPts-2; i++, vrtind+=3)
  {
    vrtind[0] = 0;
    vrtind[1] = i+1;
    vrtind[2] = i+2;
  }
  pl->m_sNumIndices = (numPts-2) * 3;
}

void CRenderer::EF_AddPolyToScene2D(int Ef, int numPts, const struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *verts)
{
  int num = CREClientPoly2D::m_PolysStorage.Num();
  CREClientPoly2D::m_PolysStorage.GrowReset(1);

  CREClientPoly2D *pl = CREClientPoly2D::m_PolysStorage[num];
  if (!pl)
  {
    pl = new CREClientPoly2D;
    CREClientPoly2D::m_PolysStorage[num] = pl;
  }

  CShader *pSh = (CShader *)CShaderMan::m_pContainer->m_RList[Ef];

  pl->m_Shader = SShaderItem(pSh);
  pl->m_sNumVerts = numPts;

  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vt = pl->m_Verts;
  int i;
  for (i=0; i<numPts; i++, vt++)
  {
    *vt = verts[i];
  }

  byte *vrtind = pl->m_Indices;

  for (i=0; i<numPts-2; i++, vrtind+=3)
  {
    vrtind[0] = 0;
    vrtind[1] = i+1;
    vrtind[2] = i+2;
  }
  pl->m_sNumIndices = (numPts-2) * 3;
}

//================================================================================================================

CRenderObject* CRenderer::MergeParticleRO( CRenderObject* pRO )
{
	uint i;

	if (pRO)
	{
		SRefSprite *rs = NULL;
		for (i=0; i<m_RP.m_Sprites.Num(); i++)
		{
			rs = &m_RP.m_Sprites[i];
			if (pRO == rs->m_pObj)
				break;
		}
		if (i == m_RP.m_Sprites.Num())
		{
			for (i=0; i<m_RP.m_Sprites.Num(); i++)
			{
				rs = &m_RP.m_Sprites[i];
				if (rs->m_pObj->m_nTextureID == pRO->m_nTextureID && rs->m_pObj->m_nTextureID1 == pRO->m_nTextureID1 && rs->m_pObj->m_DynLMMask == pRO->m_DynLMMask && rs->m_pObj->m_RState == pRO->m_RState && rs->m_pObj->m_fSort == pRO->m_fSort &&
					( !((rs->m_pObj->m_ObjFlags ^ pRO->m_ObjFlags) & FOB_PARTICLE_MASK) ) &&
					    rs->m_pObj->m_FogVolumeContribIdx == pRO->m_FogVolumeContribIdx && rs->m_pObj->m_II.m_AmbColor == pRO->m_II.m_AmbColor)
				{
					m_RP.m_RejectedObjects.AddElem(pRO);
					pRO = rs->m_pObj;
					break;
				}
			}
			if (i == m_RP.m_Sprites.Num())
			{
				SRefSprite s;
				s.m_pObj = pRO;
				//obj->m_Matrix.SetIdentity();
				m_RP.m_Sprites.AddElem(s);
				rs = &m_RP.m_Sprites[m_RP.m_Sprites.Num()-1];
			}
		}
	}

	return pRO;
}

//////////////////////////////////////////////////////////////////////////
#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
void CRenderer::EF_AddGPUParticlesToScene( int32 nGPUParticleIdx, AABB const& bb, const SShaderItem& shaderItem, CRenderObject* pRO, bool nAW, bool canUseGS )
{
	//pRO = MergeParticleRO( pRO );

	CREParticleGPU* pParticle = CREParticleGPU::Create( nGPUParticleIdx, bb );	
	int afterWater = 0;

	if( nAW == true )
	{
		afterWater = 1;
	}
	else if( nAW == false )
	{
		afterWater = 0;	
	}

	if( pRO != NULL )
	{
		//if( shaderItem.m_nPreprocessFlags != 0 )
		//{
		//		SRendItem::mfAdd( pParticle, pRO, shaderItem, EFSLIST_PREPROCESS, afterWater );
		//	}

		SRendItem::mfAdd (pParticle, pRO, shaderItem, EFSLIST_TRANSP, afterWater, FB_GENERAL);

		//	SShaderTechnique *pTech = shaderItem.GetTechnique();
		//	if( CV_r_glow && pTech && pTech->m_nTechnique[TTYPE_GLOWPASS] > 0 && shaderItem.m_pShaderResources && shaderItem.m_pShaderResources->m_GlowAmount )
		//	{
		//		SRendItem::mfAdd(pParticle, pRO, shaderItem, EFSLIST_GLOWPASS, afterWater ); 
		//	}    
	}

}
#endif

CRenderObject* CRenderer::EF_AddParticlesToScene(const SShaderItem& si, CRenderObject* pRO, IParticleVertexCreator* pPVC, SParticleRenderInfo const& RenInfo, int nAW, bool& canUseGS)
{
	pRO = MergeParticleRO(pRO);

#if defined(DIRECT3D10)
	SShaderItem shaderItem(si);
	SShaderTechnique* pTech(shaderItem.GetTechnique());

	if (!CV_r_UseGSParticles)
		canUseGS = false;

	bool techniqueSupportsGS(pTech && (pTech->m_Flags & FHF_USE_GEOMETRY_SHADER) != 0);
	if (!techniqueSupportsGS)
		canUseGS = false;
	else
	{
		if (!canUseGS)
			shaderItem.m_nTechnique = shaderItem.m_nTechnique >= 0 ? shaderItem.m_nTechnique + 1 : 1;
	}

	assert(shaderItem.GetTechnique() && canUseGS == ((shaderItem.GetTechnique()->m_Flags & FHF_USE_GEOMETRY_SHADER) != 0));

	CREParticle* pParticle(CREParticle::Create(pPVC, RenInfo, canUseGS));
	EF_AddParticle(pParticle, shaderItem, pRO, nAW);
#else
	CREParticle* pParticle(CREParticle::Create(pPVC, RenInfo));
	EF_AddParticle(pParticle, si, pRO, nAW);
#endif

	canUseGS = pParticle->UseGeomShader();
	return pRO;
}

void CRenderer::EF_AddParticle(CREParticle* pParticle, const SShaderItem& shaderItem, CRenderObject* pRO, int nAW)
{
	if (pRO)
	{
    uint32 nBatchFlags = FB_GENERAL;
    int nList = EFSLIST_TRANSP;
    if ( shaderItem.m_pShader && (((CShader *)shaderItem.m_pShader)->m_Flags & EF_REFRACTIVE) )
    {
      if( CV_r_refraction && CV_r_useparticles_refraction )
      {
        nBatchFlags |= FB_REFRACTIVE;
        nList = EFSLIST_REFRACTPASS;
      }
      else 
        return;  // skip adding refractive particle
    }

    SShaderTechnique* pTech(shaderItem.GetTechnique());
    if (CV_r_glow && CV_r_useparticles_glow && pTech && pTech->m_nTechnique[TTYPE_GLOWPASS] > 0 && shaderItem.m_pShaderResources && ((SRenderShaderResources *)shaderItem.m_pShaderResources)->Glow())
      nBatchFlags |= FB_GLOW;
    SRendItem::mfAdd(pParticle, pRO, shaderItem, nList, nAW, nBatchFlags);    
	}
}

void CRenderer::EF_AddClientPolys()
{
  uint i;
  CREClientPoly *pl;

  for (i=0; i<CREClientPoly::m_PolysStorage[SRendItem::m_RecurseLevel].Num(); i++)
  {
    pl = CREClientPoly::m_PolysStorage[SRendItem::m_RecurseLevel][i];

    if (pl->m_Shader.m_nPreprocessFlags)
      SRendItem::mfAdd(pl, pl->m_pObject, pl->m_Shader, EFSLIST_PREPROCESS, 0, FB_GENERAL);
    if (pl->m_Shader.m_pShader->GetFlags() & EF_DECAL) // || pl->m_pObject && (pl->m_pObject->m_ObjFlags & FOB_DECAL))
    {
      SRendItem::mfAdd(pl, pl->m_pObject, pl->m_Shader, EFSLIST_DECAL, pl->m_nAW, FB_GENERAL);
    }
    else
      SRendItem::mfAdd(pl, pl->m_pObject, pl->m_Shader, EFSLIST_GENERAL, pl->m_nAW, FB_GENERAL);
  }
}


//================================================================================================================
// 2d interface for shaders

void CRenderer::EF_AddClientPolys2D()
{
  uint i;
  CREClientPoly2D *pl;

  for (i=0; i<CREClientPoly2D::m_PolysStorage.Num(); i++)
  {
    pl = CREClientPoly2D::m_PolysStorage[i];
    
    if (pl->m_Shader.m_nPreprocessFlags)
      SRendItem::mfAdd(pl, NULL, pl->m_Shader, EFSLIST_PREPROCESS, 0, FB_GENERAL);
    SRendItem::mfAdd(pl, NULL, pl->m_Shader, 0, 0, FB_GENERAL);
  }
}

// Dynamic lights
bool CRenderer::EF_IsFakeDLight(CDLight *Source)
{
  if (!Source)
  {
    iLog->Log("Warning: EF_IsFakeDLight: NULL light source\n");
    return true;
  }

  bool bIgnore = false;
  if (Source->m_Flags & DLF_FILL_LIGHT)
    bIgnore = true;
  else
  if (Source->m_Flags & DLF_FAKE)
    bIgnore = true;
  return bIgnore;
}

void CRenderer::EF_ADDDlight(CDLight *Source)
{
  if (!Source)
  {
    iLog->Log("Warning: EF_ADDDlight: NULL light source\n");
    return;
  }

  bool bIgnore = EF_IsFakeDLight(Source);
  //Source->m_Flags &= ~DLF_POINT;
  //Source->m_Flags |= DLF_DIRECTIONAL;

  if (bIgnore)
    Source->m_Id = -1;
  else
  {
    assert((Source->m_Flags & DLF_LIGHTTYPE_MASK) != 0);
    Source->m_Id = m_RP.m_DLights[SRendItem::m_RecurseLevel].Num();
    if (Source->m_Id >= 32)
    {
      //iLog->Log("Warning: EF_ADDDlight: Too many light sources (Ignored)\n");
      Source->m_Id = -1;
      return;
    }
    m_RP.m_DLights[SRendItem::m_RecurseLevel].AddElem(Source);
  }
  EF_PrecacheResource(Source, (m_cam.GetPosition()-Source->m_Origin).GetLength(), 0.1f, 0);

  // Only once
  /*CRenderObject *pObj = EF_GetObject();
  SShaderItem SI = EF_LoadShaderItem("Clouds.CryCloudEditor", true);

  // each frame
  //============================
  TArray<CRendElement> *pRE = SI.m_pShader->GetREs(SI.m_nTechnique);
  if (pRE)
  {
    pObj->m_Matrix = Matrix34::CreateTranslationMat(Vec3(1,1,1));
    pObj->m_ObjFlags |= FOB_TRANS_MASK;
    float fRadius = 50.0f;
    pObj->m_TempVars[0] = fRadius;
    EF_AddEf(0, pRE->Get(0), SI, pObj, EFSLIST_TRANSP);
  }

  // destructor
  SAFE_RELEASE(SI.m_pShader);
  SAFE_RELEASE(SI.m_pShaderResources);*/


  if (!(m_RP.m_PersFlags2 & RBPF2_IMPOSTERGEN))
  {
    // Add light coronas, lens flares, beams and so on (depends on shader)
    if (Source->m_Shader.m_pShader && Source->m_Shader.m_pShader->GetREs(Source->m_Shader.m_nTechnique) && Source->m_Shader.m_pShader->GetREs(Source->m_Shader.m_nTechnique)->Num())
    {
      I3DEngine *eng = (I3DEngine *)iSystem->GetI3DEngine();
      int rl = SRendItem::m_RecurseLevel;
      float fWaterLevel = eng->GetWaterLevel();
      float fCamZ = m_cam.GetPosition().z;
      for (uint nr=0; nr<Source->m_Shader.m_pShader->GetREs(Source->m_Shader.m_nTechnique)->Num(); nr++)
      {
        //get the permanent object
        if (!Source->m_pObject[rl][nr])
        {         
          Source->m_pObject[rl][nr] = EF_GetObject(false, -1);
          Source->m_pObject[rl][nr]->m_fAlpha = 0;
          Source->m_pObject[rl][nr]->m_II.m_AmbColor = Vec3(0,0,0);
          Source->m_pObject[rl][nr]->m_fTempVars[0] = 0;
          Source->m_pObject[rl][nr]->m_fTempVars[1] = 0;
          Source->m_pObject[rl][nr]->m_fTempVars[3] = Source->m_fRadius;
         
        }
        else
          EF_GetObject(false, Source->m_pObject[rl][nr]->m_Id);	

        CRendElement *pRE = Source->m_Shader.m_pShader->GetREs(Source->m_Shader.m_nTechnique)->Get(nr);
        float fCustomSort = 0;
        if (pRE->mfGetType() == eDATA_Flare)
        {          
          Source->m_pObject[rl][nr]->m_II.m_AmbColor.r = Source->m_Color.r;
          Source->m_pObject[rl][nr]->m_II.m_AmbColor.g = Source->m_Color.g;
          Source->m_pObject[rl][nr]->m_II.m_AmbColor.b = Source->m_Color.b;
          fCustomSort = 4000.0f;
        }
        else
        {
          Source->m_pObject[rl][nr]->m_II.m_AmbColor = Source->m_Color;
          Source->m_pObject[rl][nr]->m_fTempVars[3] = Source->m_fRadius;
        }

        //if (Source->m_Flags & DLF_SUN)
        //  Source->m_pObject[rl][nr]->m_ObjFlags |= FOB_DRSUN;
        //mathCalcMatrix(Source->m_pObject[rl][nr]->m_Matrix, Source->m_Origin, Source->m_ProjAngles, Vec3(1,1,1), g_CpuFlags);
        //Matrix44 mTemp;
        //mathMatrixInverse(mTemp.GetData(), Source->m_ProjMatrix.GetData(), g_CpuFlags);
        //mathMatrixTranspose(mTemp.GetData(), mTemp.GetData(), g_CpuFlags);

        Source->m_pObject[rl][nr]->m_II.m_Matrix = Source->m_ObjMatrix;
        Source->m_pObject[rl][nr]->m_pLight = Source;
        Source->m_pObject[rl][nr]->m_DynLMMask = 1<<Source->m_Id;
        Source->m_pObject[rl][nr]->m_ObjFlags |= FOB_TRANS_MASK;

        UINT nList = EFSLIST_TRANSP;
        int nAW = 0;
        if((fCamZ-fWaterLevel)*(Source->m_Origin.z-fWaterLevel)>0)
          nAW = 1;
        else
          nAW = 0;
        EF_AddEf(pRE, Source->m_Shader, Source->m_pObject[rl][nr], nList, nAW);
      }
    }
  }
}

void CRenderer::EF_ADDFillLight(const SFillLight & rLight)
{
  if(CV_r_FillLights && m_RP.m_FillLights[SRendItem::m_RecurseLevel].Num()<512)
    m_RP.m_FillLights[SRendItem::m_RecurseLevel].AddElem(rLight);
}

void CRenderer::EF_ClearLightsList()
{
  m_RP.m_DLights[SRendItem::m_RecurseLevel].SetUse(0);
}

inline Matrix44	ToLightMatrix(const Ang3 &angle)	{
	Matrix33 ViewMatZ=Matrix33::CreateRotationZ(-angle.x);
	Matrix33 ViewMatX=Matrix33::CreateRotationX(-angle.y);
	Matrix33 ViewMatY=Matrix33::CreateRotationY(+angle.z);
	return GetTransposed44(Matrix44(ViewMatX*ViewMatY*ViewMatZ));
}

bool CRenderer::EF_UpdateDLight(CDLight *dl)
{
  bool bRes = false;
  if (!dl)
    return bRes;

  float fTime = iTimer->GetCurrTime();			// todo: light does not need to use float time - better: renderframeid

  bRes = true;

  SShaderTechnique *pTech = dl->m_Shader.GetTechnique();
  uint nStyle = dl->m_nLightStyle;
  if (pTech && pTech->m_EvalLights && dl->m_fLastTime < fTime)
  {
    SLightEval *le = pTech->m_EvalLights;
    if (!nStyle)
      nStyle = le->m_LightStyle;

    // Update light styles
    if (nStyle>0 && nStyle<CLightStyle::m_LStyles.Num() && CLightStyle::m_LStyles[nStyle])
    {
      CLightStyle *ls = CLightStyle::m_LStyles[nStyle];
      ls->mfUpdate(fTime);
      switch (le->m_EStyleType)
      {
        case eLS_Intensity:
        default:
          dl->m_Color = dl->m_BaseColor * ls->m_fIntensity;
					dl->m_RAEAmbientColor = dl->m_RAEBaseAmbientColor * ls->m_fIntensity;
          break;
        case eLS_RGB:
          dl->m_Color = ls->m_Color;
          break;
      }
      dl->m_Color.a = 1.0f;
    }
  }
  else
  if (nStyle>0 && nStyle<CLightStyle::m_LStyles.Num() && CLightStyle::m_LStyles[nStyle])
  {
    CLightStyle *ls = CLightStyle::m_LStyles[nStyle];
    ls->mfUpdate(fTime);
    dl->m_Color = dl->m_BaseColor * ls->m_fIntensity;
		dl->m_RAEAmbientColor = dl->m_RAEBaseAmbientColor * ls->m_fIntensity;
  }
	else
	{
		dl->m_Color = dl->m_BaseColor;
		dl->m_RAEAmbientColor = dl->m_RAEBaseAmbientColor;
	}

	if(EF_Query(EFQ_HDRModeEnabled)!=0)
	{
		I3DEngine *p3DEngine = (I3DEngine *)iSystem->GetI3DEngine();		assert(p3DEngine);

		float fHDR = powf( p3DEngine->GetHDRDynamicMultiplier(), dl->m_fHDRDynamic );
  	dl->m_Color *= fHDR;
		dl->m_RAEAmbientColor *= fHDR;
	}  

	dl->m_fLastTime = fTime;

	if ((dl->m_Flags & DLF_PROJECT))
  {
    //we need to rotate the cubemap to account for the spotlights orientation
    //convert the orienations ortho normal basis (ONB) into XYZ space, and then
    //into the base direction space (using ONB prevents having to calculate angles)
    /*dl->m_Orientation.m_vForward = Vec3(1,0,0);
    dl->m_Orientation.m_vUp = Vec3(0,1,0);
    dl->m_Orientation.m_vRight = Vec3(0,0,1);
    dl->m_Orientation.rotate(Vec3(1,0,0), dl->m_ProjAngles.x);
    dl->m_Orientation.rotate(Vec3(0,1,0), dl->m_ProjAngles.y);
    dl->m_Orientation.rotate(Vec3(0,0,1), dl->m_ProjAngles.z);

    Matrix44 m = dl->m_Orientation.matrixBasisToXYZ();

    //scale the cubemap to adjust the default 45 degree 1/2 angle fustrum to 
    //the desired angle (0 to 90 degrees)
    float scaleFactor = cry_tanf((90.0f-dl->m_fLightFrustumAngle)*PI/180.0f);

		m	=	Matrix33::CreateScale(Vec3(1,scaleFactor,scaleFactor)) * m;
    mathMatrixTranspose(dl->m_TextureMatrix.GetData(), m.GetData(), g_CpuFlags);

    //translate the vertex relative to the light position
    Matrix44 transMat;
    transMat.SetIdentity();
    transMat(3,0) = -dl->m_Origin[0]; transMat(3,1) = -dl->m_Origin[1]; transMat(3,2) = -dl->m_Origin[2];

    mathMatrixMultiply(dl->m_TextureMatrix.GetData(), dl->m_TextureMatrix.GetData(), transMat.GetData(), g_CpuFlags);*/
	}
  return false;
}

void CRenderer::EF_ApplyShaderQuality(EShaderType eST)
{
  SShaderProfile *pSP = &m_cEF.m_ShaderProfiles[eST];
  m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_QUALITY] | g_HWSR_MaskBit[HWSR_QUALITY1]);
  int nQuality = (int)pSP->GetShaderQuality();
  m_RP.m_nShaderQuality = nQuality;
  switch (nQuality)
  {
    case eSQ_Medium:
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY];
      break;
    case eSQ_High:
    case eSQ_VeryHigh:
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY1];
      break;
  }
}

EShaderQuality CRenderer::EF_GetShaderQuality(EShaderType eST)
{
  SShaderProfile *pSP = &m_cEF.m_ShaderProfiles[eST];
  int nQuality = (int)pSP->GetShaderQuality();

  switch (nQuality)
  {
    case eSQ_Low:
      return eSQ_Low;
    case eSQ_Medium:
      return eSQ_Medium;
    case eSQ_High:
    case eSQ_VeryHigh:
      return eSQ_High;
  }

  return eSQ_Low;
}

ERenderQuality CRenderer::EF_GetRenderQuality() const
{
  return (ERenderQuality) m_RP.m_eQuality;
}

#ifdef WIN64
#pragma warning( push )							//AMD Port
#pragma warning( disable : 4312 )				// 'type cast' : conversion from 'int' to 'void *' of greater size
#endif
 
void *CRenderer::EF_Query(int Query, INT_PTR Param)
{
  static int nSize;
  switch (Query)
  {
    case EFQ_DeleteMemoryArrayPtr:
      {
        char *pPtr = (char *)Param;
        delete [] pPtr;
      }
      break;
    case EFQ_DeleteMemoryPtr:
      {
        char *pPtr = (char *)Param;
        delete pPtr;
      }
      break;
    case EFQ_GetShaderCombinations:
      {
        const char *szPtr = CHWShader::GetCurrentShaderCombinations();
        return (void *)szPtr;
      }
      break;
    case EFQ_SetShaderCombinations:
      {
        if (CV_r_shaderspreactivate)
        {
          iLog->Log("--- Preactivate shaders...");
          iLog->UpdateLoadingScreen(0);
          bool bRes = CHWShader::SetCurrentShaderCombinations((const char *)Param);
				  m_cEF.m_bActivated = true;
        }
        return NULL;
      }
      break;
    case EFQ_CloseShaderCombinations:
      {
        m_cEF.mfCloseShadersCache();
        return NULL;
      }
      break;
    case EFQ_LightSource:
      {
        if (m_RP.m_DLights[SRendItem::m_RecurseLevel].Num() > (uint)Param)
          return m_RP.m_DLights[SRendItem::m_RecurseLevel][Param];
        return NULL;
      }
      break;

    case EFQ_ShaderGraphBlocks:
      return (void *)&m_cEF.m_GR.m_Blocks;

    case EFQ_Pointer2FrameID:
      return (void *)&m_nFrameID;
    case EFQ_D3DDevice:
      return gGet_D3DDevice();
    case EFQ_glReadPixels:
      return gGet_glReadPixels();
    case EFQ_DeviceLost:
      nSize = (int)CheckDeviceLost();
      return (void *)&nSize;
    case EFQ_RecurseLevel:
      return (void *)(UINT_PTR)SRendItem::m_RecurseLevel;
    case EFQ_Alloc_APITextures:
      {
        nSize = 0;
        CCryName Name = CTexture::mfGetClassName();
        SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
        if (pRL)
        {
          ResourcesMapItor itor;
          for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
          {
            CTexture *tp = (CTexture *)itor->second;
            if (!tp || tp->IsNoTexture())
              continue;
#if !defined(XENON) && !defined(PS3)
            if (!(tp->GetFlags() & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET)))
              nSize += tp->GetDeviceDataSize();
#else
            nSize += tp->GetDeviceDataSize();
#endif
          }
        }
        return (void *)&nSize;
      }
      break;

    case EFQ_Alloc_APIMesh:
      {
        nSize = 0;
        CRenderMesh *pRM = CRenderMesh::m_RootGlobal.m_NextGlobal;
        // Vertices
        for (pRM=CRenderMesh::m_RootGlobal.m_NextGlobal; pRM!=&CRenderMesh::m_RootGlobal; pRM=pRM->m_NextGlobal)
        {
          nSize += pRM->Size(1);
					nSize += pRM->Size(2);
        }
        return (void *)&nSize;
      }
      break;
		
		case EFQ_Alloc_Mesh_SysMem:
			{
				nSize = 0;
				CRenderMesh *pRM = CRenderMesh::m_RootGlobal.m_NextGlobal;
				for (pRM=CRenderMesh::m_RootGlobal.m_NextGlobal; pRM!=&CRenderMesh::m_RootGlobal; pRM=pRM->m_NextGlobal)
				{
					nSize += pRM->Size(0);
				}
				return (void *)&nSize;
			}
			break;
		
		case EFQ_Mesh_Count:
			{
				nSize = 0;
				CRenderMesh *pRM = CRenderMesh::m_RootGlobal.m_NextGlobal;
				// Vertices
				for (pRM=CRenderMesh::m_RootGlobal.m_NextGlobal; pRM!=&CRenderMesh::m_RootGlobal; pRM=pRM->m_NextGlobal)
				{
					nSize++;
				}
				return (void *)&nSize;
			}
			break;

		case EFQ_GetAllMeshes:
			{
				IRenderMesh **pMeshes = (IRenderMesh**)Param;
				nSize = 0;
				CRenderMesh *pRM = CRenderMesh::m_Root.m_NextGlobal;
				// Vertices
				for (pRM=CRenderMesh::m_RootGlobal.m_NextGlobal; pRM!=&CRenderMesh::m_RootGlobal; pRM=pRM->m_NextGlobal)
				{
					if (pMeshes)
					{
						pMeshes[nSize] = pRM;
						CryLog("Still alive %s", pRM->GetSourceName());
					}
					nSize++;
				}
				return (void *)&nSize;
			}
			break;

		case EFQ_GetAllTextures:
			{
				nSize = 0;
				ITexture **pTextures = (ITexture**)Param;
				CCryName Name = CTexture::mfGetClassName();
				SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
				if (pRL)
				{
					ResourcesMapItor itor;
					for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
					{
						CTexture *tp = (CTexture *)itor->second;
						if (!tp || tp->IsNoTexture())
							continue;
						if (pTextures)
						{
							pTextures[nSize] = tp;
						}
						nSize++;
					}
				}
				return (void *)&nSize;
			}
			break;

		case EFQ_HDRModeEnabled:
			return IsHDRModeEnabled() ? (void *)1 : 0;

		case EFQ_MultiGPUEnabled:
			return IsMultiGPUModeActive() ? (void *)1 : 0;

		case EFQ_DrawNearFov:
			{
				static float prevValue;
				prevValue= CV_r_drawnearfov;
				if (Param)
					CV_r_drawnearfov = (int)(*(float*) Param);
				return (void*) &prevValue;
			}

		case EFQ_TextureStreamingEnabled:
			return CTexture::m_nStreamingMode ? (void *)1 : 0;

		case EFQ_FSAAEnabled:
			return m_RP.IsFSAAEnabled() ? (void *)1 : 0;

		case EFQ_Fullscreen:
			return m_FullScreen ? (void *)1 : 0;

		default:
      assert(0);
  }
  return NULL;
}

#ifdef WIN64
#pragma warning( pop )							//AMD Port
#endif

//================================================================================================================

IRenderMesh * CRenderer::CreateRenderMesh(bool bDynamic, const char *szType,const char *szSourceName)
{
  // make material table with clean elements
  CRenderMesh *pRenderMesh = new CRenderMesh(szType,szSourceName);
	pRenderMesh->AddRef();
  pRenderMesh->m_bDynamic = bDynamic;
  pRenderMesh->m_Indices.m_bDynamic = bDynamic;
  /*if (szSourceName && strstr(szSourceName, "eapons"))
  {
    int nnn = 0;
  }*/

  return pRenderMesh;
}

void CRenderer::DeleteRenderMesh(IRenderMesh * pLBuffer)
{
  if(pLBuffer)
	{
		pLBuffer->Release();
	}
}

// Creates the RenderMesh with the materials, secondary buffer (system buffer)
// indices and perhaps some other stuff initialized.
// NOTE: if the pVertBuffer is NULL, the system buffer doesn't get initialized with any values
// (trash may be in it)
IRenderMesh * CRenderer::CreateRenderMeshInitialized(
  void * pVertBuffer, int nVertCount, int nVertFormat, 
  ushort * pIndices, int nIndices,
  int nPrimetiveType, const char *szType, const char *szSourceName, EBufferType eBufType,
  int nMatInfoCount, int nClientTextureBindID,
  bool (*PrepareBufferCallback)(IRenderMesh *, bool),
  void *CustomData,bool bOnlyVideoBuffer, bool bPrecache, 
	SPipTangents * pTangents)
{
  CRenderMesh * pRenderMesh = new CRenderMesh(szType,szSourceName);
	pRenderMesh->AddRef();
 
  // make mats info list
  pRenderMesh->m_pChunks = new PodArray<CRenderChunk>;
  pRenderMesh->m_pChunks->PreAllocate(nMatInfoCount);
  pRenderMesh->m_bMaterialsWasCreatedInRenderer=true;
  pRenderMesh->m_bDynamic = (eBufType == eBT_Dynamic);
  pRenderMesh->m_Indices.m_bDynamic = pRenderMesh->m_bDynamic;
  pRenderMesh->m_nVertexFormat = nVertFormat;
  pRenderMesh->m_nNumVidIndices = 0;

  pRenderMesh->m_nVertCount  = nVertCount;

  // copy vert buffer
  if (pVertBuffer && !PrepareBufferCallback && !bOnlyVideoBuffer)
  {
	  pRenderMesh->m_pSysVertBuffer = new CVertexBuffer(CreateVertexBuffer(nVertFormat, nVertCount),nVertFormat);
		pRenderMesh->UpdateSysVertices(pVertBuffer, nVertCount, nVertFormat, VSF_GENERAL);
    //gRenDev->UpdateBuffer(pRenderMesh->m_pSysVertBuffer,pVertBuffer,nVertCount,true);
    pRenderMesh->m_pSysVertBuffer->m_VS[0].m_bDynamic = pRenderMesh->m_bDynamic;

		if(pTangents)
		{
			SPipTangents *pTBuff = new SPipTangents[nVertCount];
			memcpy(pTBuff, pTangents, sizeof(SPipTangents)*nVertCount);
			pRenderMesh->m_pSysVertBuffer->m_VS[VSF_TANGENTS].m_VData = pTBuff;
      pRenderMesh->m_pSysVertBuffer->m_VS[VSF_TANGENTS].m_bDynamic = pRenderMesh->m_bDynamic;
		}
  }
  else
  if (pVertBuffer && bOnlyVideoBuffer && nVertCount)
  {
    pRenderMesh->CreateVidVertices();
    pRenderMesh->UpdateVidVertices(NULL, 0, VSF_GENERAL, false);
  }

	pRenderMesh->m_pCustomData = CustomData;

	pRenderMesh->PrepareBufferCallback = PrepareBufferCallback;

  if (pIndices)
    pRenderMesh->UpdateSysIndices(pIndices, nIndices);
  pRenderMesh->m_nPrimetiveType = nPrimetiveType;

  pRenderMesh->m_nClientTextureBindID = nClientTextureBindID;

  // Precache for static buffers
  if (CV_r_meshprecache && pRenderMesh->m_nVertCount && bPrecache && eBufType!=eBT_Dynamic && !m_bDeviceLost)
  {
    pRenderMesh->CheckUpdate(nVertFormat, 0, pRenderMesh->m_nVertCount, pRenderMesh->m_SysIndices.Num(), 0, 0);
#ifndef NULL_RENDERER
    assert (pRenderMesh->m_pVertexBuffer);
#endif
  }
  
  return pRenderMesh;
}
/*
void CRenderer::AddMatInfoIntoRenderMesh( CRenderMesh * pRenderMesh, 
                                          const char * szEfName, 
                                          int nFirstVertId, int nVertCount, 
                                          int nFirstIndexId, int nIndexCount)
{
  if(!nIndexCount || !nVertCount)
    return;

  // make single chunk
  CRenderChunk matinfo;
  matinfo.pShader = EF_LoadShader((char*)szEfName, -1, eEF_World, 0);
  matinfo.pRE = (CREMesh*)EF_CreateRE(eDATA_OcLeaf);
  matinfo.nFirstIndexId = nFirstIndexId;
  matinfo.nNumIndices  = nIndexCount;
  matinfo.nFirstVertId = nFirstVertId;
  matinfo.nNumVerts = nVertCount;
  
  // register this chunk
  pRenderMesh->m_pChunks->Add(matinfo);
  pRenderMesh->m_pChunks->Last().pRE->m_pChunk = &(pRenderMesh->m_pChunks->Last());
  pRenderMesh->m_pChunks->Last().pRE->m_pBuffer = pRenderMesh;
}*/
/*
CRenderMesh * CRenderer::CreateRenderMeshInitialized(
  const char * szEfName,
  PipVertex * pVertBuffer, int nVertCount,
  PodArray<ushort> * pIndices,
  int nPrimetiveType)
{
  CRenderMesh * pRenderMesh = new CRenderMesh();

  // make single chunk
  CRenderChunk matinfo;
  matinfo.pShader = EF_LoadShader((char*)szEfName, -1, eEF_World, 0);
  matinfo.pRE = (CREMesh*)EF_CreateRE(eDATA_OcLeaf);
  matinfo.nFirstIndexId = 0;
  matinfo.nNumIndices  = pIndices->Count();
  matinfo.nFirstVertId = 0;
  matinfo.nNumVerts = nVertCount;
  
  // register this chunk
  pRenderMesh->m_pChunks = new PodArray<CRenderChunk>;
  pRenderMesh->m_pChunks->Add(matinfo);

  // copy vert buffer
	pRenderMesh->m_SysVertCount   = nVertCount;
	pRenderMesh->m_pSysVertBuffer = new CVertexBuffer;
  pRenderMesh->m_pSysVertBuffer->m_vertexformat = VERTEX_FORMAT_P3F_N3F_COL4UB_COL4UB_TEX2F_TEX2F_FOG1F_TANGENT_TANNORM;
  pRenderMesh->m_pSysVertBuffer->m_data = new PipVertex[nVertCount];
	memcpy(pRenderMesh->m_pSysVertBuffer->m_data, pVertBuffer, sizeof(PipVertex)*nVertCount);

  pRenderMesh->GetIndices()        = *pIndices;
  pRenderMesh->m_nPrimetiveType = nPrimetiveType;
  
  return pRenderMesh;
}*/


//=======================================================================

void CRenderer::SetWhiteTexture()
{
  CTexture::m_Text_White->Apply();
}
void CRenderer::SetTexture(int tnum)
{
  CTexture::ApplyForID(tnum);
}

// used for sprite generation
void CRenderer::SetTextureAlphaChannelFromRGB(byte * pMemBuffer, int nTexSize)
{
#ifndef XENON
  // set alpha channel
  for(int y=0; y<nTexSize; y++)
    for(int x=0; x<nTexSize; x++)
    {
      int t = (x+nTexSize*y)*4;
      if( abs(pMemBuffer[t+0]-pMemBuffer[0+0])<2 && 
          abs(pMemBuffer[t+1]-pMemBuffer[0+1])<2 && 
          abs(pMemBuffer[t+2]-pMemBuffer[0+2])<2 )
        pMemBuffer[t+3] = 0;
      else
        pMemBuffer[t+3] = 255;

      // set border alpha to 0
      if( x==0 || y == 0 || x == nTexSize-1 || y == nTexSize-1 )
        pMemBuffer[t+3] = 0;
    }
#else
  // set alpha channel
  for(int y=0; y<nTexSize; y++)
  {
    for(int x=0; x<nTexSize; x++)
    {
      int t = (x+nTexSize*y)*4;
      if( abs(pMemBuffer[t+3]-pMemBuffer[0+3])<2 && 
          abs(pMemBuffer[t+2]-pMemBuffer[0+2])<2 && 
          abs(pMemBuffer[t+1]-pMemBuffer[0+1])<2 )
        pMemBuffer[t+0] = 0;
      else
        pMemBuffer[t+0] = 255;

      // set border alpha to 0
      if( x==0 || y == 0 || x == nTexSize-1 || y == nTexSize-1 )
        pMemBuffer[t+0] = 0;
    }
  }
#endif
}

//=============================================================================
// Precaching

bool CRenderer::EF_PrecacheResource(IRenderMesh *_pPB, float fDist, float fTimeToReady, int Flags)
{
  int i;
  if (!CTexture::m_nStreamed)
    return true;

	CRenderMesh * pPB = (CRenderMesh*)_pPB;

  for (i=0; i<pPB->m_pChunks->Count(); i++)
  {
    CRenderChunk *pChunk = &pPB->m_pChunks->GetAt(i);
		assert(!"do pre-cache with real materials");

		assert(0);

		//@TODO: Timur
		assert( pPB->GetMaterial() && "RenderMesh must have material" );
    SRenderShaderResources *pSR = (SRenderShaderResources *)pPB->GetMaterial()->GetShaderItem(pChunk->m_nMatID).m_pShaderResources;
    if (!pSR)
      continue;
    if (pSR->m_nFrameLoad != m_nFrameID)
    {
      pSR->m_nFrameLoad = m_nFrameID;
      pSR->m_fMinDistanceLoad = 999999.0f;
    }
    else
      if (fDist >= pSR->m_fMinDistanceLoad || CTexture::m_nStreamed == 2)
      continue;
    pSR->m_fMinDistanceLoad = fDist;
    for (int j=0; j<=pSR->m_nLastTexture; j++)
    {
      if (!pSR->m_Textures[j])
        continue;
      CTexture *tp = pSR->m_Textures[j]->m_Sampler.m_pTex;
      if (!tp)
        continue;
      tp->PrecacheAsynchronously(fDist, Flags);
    }
  }
  return true;
}

bool CRenderer::EF_PrecacheResource(CDLight *pLS, float fDist, float fTimeToReady, int Flags)
{
  if (!CTexture::m_nStreamed)
    return true;

  if (pLS->m_pLightImage)
    pLS->m_pLightImage->PrecacheAsynchronously(fDist, Flags);
  return true;
}

bool CRenderer::EF_PrecacheResource(ITexture *pTP, float fDist, float fTimeToReady, int Flags)
{
  if (!CTexture::m_nStreamed)
    return true;

  if (pTP)
    pTP->PrecacheAsynchronously(fDist, Flags);
  return true;
}

bool CRenderer::EF_PrecacheResource(IShader *pSH, float fDist, float fTimeToReady, int Flags)
{
  if (!CTexture::m_nStreamed)
    return true;

  return true;
}

#if !defined(XENON) && !defined(PS3)

#include "Textures/Image/NVDXT/dxtlib.h"
#define DDSD_CAPS		0x00000001l	// default
#define DDSD_PIXELFORMAT	0x00001000l
#define DDSD_WIDTH		0x00000004l
#define DDSD_HEIGHT		0x00000002l
#define DDSD_LINEARSIZE		0x00080000l

#define FOURCC_DXT1  (MAKEFOURCC('D','X','T','1'))
#define FOURCC_DXT2  (MAKEFOURCC('D','X','T','2'))
#define FOURCC_DXT3  (MAKEFOURCC('D','X','T','3'))
#define FOURCC_DXT4  (MAKEFOURCC('D','X','T','4'))
#define FOURCC_DXT5  (MAKEFOURCC('D','X','T','5'))

extern  byte *sDDSData;

NV_ERROR_CODE ReadDTXnFile(void *buffer, size_t count, void * userData)
{
	cryMemcpy(buffer, sDDSData, count);
	sDDSData += count;

	return NV_OK;
}
#endif

bool CRenderer::DXTDecompress(byte *srcData,const size_t srcFileSize,byte *dstData,int nWidth,int nHeight,int nMips,ETEX_Format eSrcTF, bool bUseHW, int nDstBytesPerPix)
{
#if !defined(NULL_RENDERER) && !defined(PS3) && !defined(LINUX) && !defined(XENON)
  if (nDstBytesPerPix != 3 && nDstBytesPerPix != 4)
    return false;

	if (eSrcTF == eTF_3DC)
	{
		if (!DeCompressTextureATI)
			return false;
		DWORD outSize = 0;
		if (nWidth < 4 || nHeight < 4)
			return false;
		void *outData = NULL;
		COMPRESSOR_ERROR err = DeCompressTextureATI(nWidth, nHeight, FORMAT_COMP_ATI2N, FORMAT_ARGB_8888, srcData, &outData, &outSize);
		if (err != COMPRESSOR_ERROR_NONE)
			return false;

		byte *src = (byte *)outData;
		for (int n=0; n < nWidth*nHeight; n++)
		{
			dstData[n*4+0] = src[n*4+2];
			dstData[n*4+1] = src[n*4+1];
			dstData[n*4+2] = src[n*4+0];
			dstData[n*4+3] = src[n*4+3];
		}

		// alpha channel might be attached (currently only in 3DC)
		{
			size_t nSizeDDS = CTexture::TextureDataSize(nWidth,nHeight,1,nMips,eTF_3DC);

			uint8 *pMem = 0;

			if (srcFileSize-sizeof(CImageExtensionHelper::DDS_HEADER)-4 > nSizeDDS)
				pMem = srcData+nSizeDDS;

			CImageExtensionHelper::DDS_HEADER *pDDSHeader = (CImageExtensionHelper::DDS_HEADER *)CImageExtensionHelper::GetAttachedImage(pMem);

			if(pDDSHeader)
			{
				uint8 *pL8Data = (uint8 *)(pDDSHeader)+sizeof(CImageExtensionHelper::DDS_HEADER);		// assuming it's A8 format (ensured with assets when loading)

				// assuming attached image can have lower res and difference is power of two
				uint32 reducex = IntegerLog2((uint32)(nWidth/pDDSHeader->dwWidth));
				uint32 reducey = IntegerLog2((uint32)(nHeight/pDDSHeader->dwHeight));

				uint8 *pDst = &dstData[3];

				for(int y=0;y<nHeight;++y)
				for(int x=0;x<nWidth;++x)
				{
					*pDst = pL8Data[(x>>reducex)+(y>>reducey)*pDDSHeader->dwWidth];
					pDst += 4;
				}
			}
		}

		DeleteDataATI(outData);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      
		return true;
	}

  // NOTE: AMD64 port: implement
  if (!bUseHW)
  {
    CImageExtensionHelper::DDS_HEADER *ddsh;
    int blockSize = (eSrcTF == eTF_DXT1) ? 8 : 16;
    int DXTSize = ((nWidth+3)/4)*((nHeight+3)/4)*blockSize;
    byte *dd = new byte [DXTSize + sizeof(CImageExtensionHelper::DDS_HEADER) + sizeof(DWORD)];

    DWORD dwMagic = MAKEFOURCC('D','D','S',' ');
    *(DWORD *)dd = dwMagic;
    ddsh = (CImageExtensionHelper::DDS_HEADER *)&dd[sizeof(DWORD)];
    memset(ddsh, 0, sizeof(CImageExtensionHelper::DDS_HEADER));
    cryMemcpy(&dd[sizeof(DWORD)+sizeof(CImageExtensionHelper::DDS_HEADER)], srcData, DXTSize);

    ddsh->dwSize = sizeof(CImageExtensionHelper::DDS_HEADER);
    ddsh->dwWidth = nWidth;
    ddsh->dwHeight = nHeight;
    ddsh->dwHeaderFlags = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_LINEARSIZE;
    ddsh->dwPitchOrLinearSize = nWidth*nHeight*4/blockSize;
    if (eSrcTF == eTF_DXT1)
      ddsh->ddspf.dwFourCC = FOURCC_DXT1;
    else
    if (eSrcTF == eTF_DXT3)
      ddsh->ddspf.dwFourCC = FOURCC_DXT3;
    else
    if (eSrcTF == eTF_DXT5)
      ddsh->ddspf.dwFourCC = FOURCC_DXT5;
    ddsh->ddspf.dwSize = sizeof(ddsh->ddspf);
    ddsh->ddspf.dwFlags = DDS_FOURCC;
    ddsh->dwSurfaceFlags = DDS_SURFACE_FLAGS_TEXTURE;

    sDDSData = dd;
//    int planes;
//    int lTotalWidth;
//    int rowBytes;
//    int width;
//    int height;
//    int src_format;

		//byte *_data = nvDXTdecompress(width, height, planes, lTotalWidth, rowBytes, src_format);

		nvImageContainer imageData;
		int readMIPMapCount=1;

		imageData.rgbaMIPImage.resize(1);
		imageData.rgbaMIPImage[0].realloc(nWidth,nHeight);		// to alloc memory in our DLL

		if(nvDDS::nvDXTdecompress(imageData,PF_RGBA,readMIPMapCount,ReadDTXnFile,0)!=NV_OK)
		{
			delete []dd;
			return false;
		}

		RGBAImage &mip = imageData.rgbaMIPImage[0];

		byte *_data = (byte *)mip.pixels();

		if (imageData.nPlanes != nDstBytesPerPix)
    {
      int n = imageData.width * imageData.height;
      if (imageData.nPlanes == 4)
      {
        assert (nDstBytesPerPix == 3);
        byte *data1 = _data;
        byte *dd1 = dstData;

        for (int i=0; i<n; i++)
        {
          dd1[0] = data1[2];
          dd1[1] = data1[1];
          dd1[2] = data1[0];
          dd1   += 3;
          data1 += 4;
        }
      }
      else
      if (imageData.nPlanes == 3)
      {
        assert (nDstBytesPerPix == 4);
        byte *data1 = _data;
        byte *dd1 = dstData;

        for (int i=0; i<n; i++)
        {
          dd1[0] = data1[2];
          dd1[1] = data1[1];
          dd1[2] = data1[0];
          dd1[3] = 255;
          dd1   += 4;
          data1 += 3;
        }
      }
    }
    else
      cryMemcpy(dstData, _data, imageData.width*imageData.height*imageData.nPlanes);

		delete []dd;
//		CRTDeleteArray(_data);
    return true;
  }
  else
#endif
  {
    return false;
  }
}

#if !defined(XENON) && !defined(PS3)

// wrapper used to remove DDS header from the NVDXT callback
static void *g_pNVDXT_UserData=0;
static MIPDXTcallback g_pNVDXT_Callback=0;
static size_t g_NVDXT_HeaderToRead = 4+sizeof(CImageExtensionHelper::DDS_HEADER);		// magic word+struct

static NV_ERROR_CODE NVIDIA_DXTWriteCallback(const void *buffer,size_t count, const MIPMapData * mipMapData, void * userData)
{
	assert(g_pNVDXT_UserData);
	assert(g_pNVDXT_Callback);

	uint8 *newbuffer=(uint8 *)buffer;
	size_t newcount=count;

	// ignore the first few bytes (dds header)
	if(newcount>g_NVDXT_HeaderToRead)
	{
		newbuffer+=g_NVDXT_HeaderToRead;
		newcount-=g_NVDXT_HeaderToRead;
		g_NVDXT_HeaderToRead=0;
	}
	else
	{
		g_NVDXT_HeaderToRead-=count;
		newbuffer+=count;
		newcount=0;
	}

	if(newcount)
		g_pNVDXT_Callback(newbuffer,newcount,userData);

	return NV_OK;
}

#endif

bool CRenderer::DXTCompress( byte *raw_data,int nWidth,int nHeight,ETEX_Format eTF, bool bUseHW, bool bGenMips,
	int nSrcBytesPerPix, const Vec3 vLumWeight, MIPDXTcallback callback)
{
#ifdef WIN32
	if(IsBadReadPtr(raw_data, nWidth*nHeight*nSrcBytesPerPix))
	{
		assert(0);
		iLog->Log("Warning: CRenderer::DXTCompress: invalid data passed to the function");
		return false;
	}
#endif

#if !defined (NULL_RENDERER) && !defined (XENON)
  if(!bUseHW)
	if(CV_r_TextureCompressor==1)							// try squish
	if(bGenMips==false)				// todo: support mips
	{
		squish::SSquishOptions squishoptions;			assert(squishoptions.m_flags==0);
		if(vLumWeight.x>0)
		{
			squishoptions.m_Weights[0]=vLumWeight.x;
			squishoptions.m_Weights[1]=vLumWeight.y;
			squishoptions.m_Weights[2]=vLumWeight.z;
		}

		switch(eTF)
		{
			case eTF_DXT1:	squishoptions.m_flags = squish::kDxt1; break;
//			case eTF_DXT3:	squishoptions.m_flags = squish::kDxt3; break;		// not yet supported - would require proper alpha treatment
			case eTF_DXT5:	squishoptions.m_flags = squish::kDxt5; break;		// not very well tested yet
			default: assert(0);		// maybe squish would support the format
		}

		if(squishoptions.m_flags)
		{
			int size = squish::GetStorageRequirements(nWidth,nHeight,squishoptions.m_flags);

			uint8 *pOutMem = new uint8[size];

			if(pOutMem)
			{      
				//				uint32 nMips = CTexture::CalcNumMips(nWidth, nHeight);
				//				for(uint32 dwMip=0;dwMip<nMips;++dwMip)
				{
					//					if (!nWidth)
					//						nWidth = 1;
					//					if (!nHeight)
					//						nHeight = 1;

					//					int size = squish::GetStorageRequirements(nWidth,nHeight,squishformat);

					squish::CompressImageBGRX(raw_data,nWidth,nHeight,pOutMem,squishoptions);

//					MIPMapData MMData;
//					MMData.height	=	nHeight;
//					MMData.width	=	nWidth;
//					MMData.mipLevel	=	0;
//					(*callback)(pOutMem,size,&MMData,NULL);
					(*callback)(pOutMem,size,NULL);

						//					nWidth >>= 1;
						//					nHeight >>= 1;
				}

				delete [] pOutMem;
				return true;
			}

			return false;		// not enough memory
		}
	}

  if(!bUseHW)
	if(CV_r_TextureCompressor==2)							// try ATI compressor
	if(bGenMips==false)				// todo: support mips
	{
		DWORD size;
		void *pOutMem;

		CompressTextureATI(nWidth, nHeight, FORMAT_ARGB_8888, FORMAT_COMP_ATI2N_DXT5, raw_data, &pOutMem, &size);

		(*callback)(pOutMem,size,NULL);

		delete [] pOutMem;
		return true;
	}
#endif // NULL_RENDERER

#if !defined(WIN64) && !defined(NULL_RENDERER) && !defined(PS3) && !defined(LINUX) && !defined(XENON)
	// NOTE: AMD64 port: implement
  if (!bUseHW)
  {
    nvCompressionOptions opt;

		if(vLumWeight.x>0)
		{
			opt.weight[0]=vLumWeight.x;
			opt.weight[1]=vLumWeight.y;
			opt.weight[2]=vLumWeight.z;
			opt.weightType=kUserDefinedWeighting;
		}

    switch(eTF)
    {
			case eTF_A4R4G4B4:
				opt.textureFormat = k4444;
				break;
			case eTF_A8R8G8B8:
        opt.textureFormat = k8888;
				break;
      case eTF_DXT1:
        opt.textureFormat = kDXT1;
    	  break;
      case eTF_DXT3:
        opt.textureFormat = kDXT3;
        break;
      case eTF_DXT5:
        opt.textureFormat = kDXT5;
        break;
			case eTF_A8:
				opt.textureFormat = kA8;
				break;
			case eTF_L8:
				opt.textureFormat = kL8;
				break;
      default:
        assert(0);
        return false;
    }
    opt.mipFilterType = kMipFilterQuadratic;
    if (bGenMips)
      opt.mipMapGeneration = kGenerateMipMaps;
    else
      opt.mipMapGeneration = kNoMipMaps;
#if defined(XENON) || defined(PS3)

		// not implemented

#else
		g_pNVDXT_UserData=0;							// not utilized yet
		g_pNVDXT_Callback=callback;
		g_NVDXT_HeaderToRead = 4+sizeof(CImageExtensionHelper::DDS_HEADER);		// magic word+struct

		nvDDS::nvDXTcompress(raw_data,nWidth,nHeight,nWidth*nSrcBytesPerPix,nvBGRA,&opt,NVIDIA_DXTWriteCallback);

		g_pNVDXT_UserData=0;
		g_pNVDXT_Callback=0;
		g_NVDXT_HeaderToRead=0;

#endif
  }
  else
#endif
#if !defined(XENON) && !defined(PS3)
  {
    int i;
    byte *nDst = raw_data;
    if (nSrcBytesPerPix >= 3)
    {
      nDst = new byte[nWidth*nHeight*4];
			bool bUseAlpha = nSrcBytesPerPix > 3;
      for (i=0; i<nWidth*nHeight; i++)
      {
        nDst[i*4+0] = raw_data[i*nSrcBytesPerPix+0];
        nDst[i*4+1] = raw_data[i*nSrcBytesPerPix+1];
        nDst[i*4+2] = raw_data[i*nSrcBytesPerPix+2];
				nDst[i*4+3] = bUseAlpha ? raw_data[i*nSrcBytesPerPix+3] : 255;
      }
    }
    int nMips = 1;
    if (bGenMips)
      nMips = CTexture::CalcNumMips(nWidth, nHeight);
    int DXTSize = 0;
    byte *data = CTexture::Convert(nDst, nWidth, nHeight, nMips, eTF_A8R8G8B8, eTF, nMips, DXTSize, true);
    if (callback)
    {
      int blockSize = (eTF == eTF_DXT1) ? 8 : 16;
      int nOffs = 0;
      int wdt = nWidth;
      int hgt = nHeight;
      for (i=0; i<nMips; i++)
      {
        if (!wdt)
          wdt = 1;
        if (!hgt)
          hgt = 1;
        int nSize = ((wdt+3)/4)*((hgt+3)/4)*blockSize;
        assert(nSize+nOffs <= DXTSize);
//				MIPMapData MMData;
//				MMData.mipLevel	=	i;
//				MMData.width	=	wdt;
//				MMData.height	=	hgt;
//        (*callback)(&data[nOffs], nSize, &MMData, NULL);
				(*callback)(&data[nOffs], nSize, NULL);
        nOffs += nSize;
        wdt >>= 1;
        hgt >>= 1;
      }
    }
    delete [] data;
    if (nDst != raw_data)
      delete [] nDst;
  }
#endif
  return true;
}


bool CRenderer::WriteJPG(byte *dat, int wdt, int hgt, char *name, int src_bits_per_pixel )
{
#ifndef WIN64
	return ::WriteJPG(dat, wdt, hgt, name, src_bits_per_pixel);
#else
	return false;
#endif
}

//////////////////////////////////////////////////////////////////////////
// IShaderPublicParams implementation class.
//////////////////////////////////////////////////////////////////////////

struct CShaderPublicParams : public IShaderPublicParams
{
  CShaderPublicParams()
  {
    m_nRefCount = 0;
  }

  virtual void AddRef() { m_nRefCount++; };
  virtual void Release() { if (--m_nRefCount <=0) delete this; };

  virtual void SetParamCount( int nParam ) { m_shaderParams.resize(nParam); }
  virtual int  GetParamCount() const { return m_shaderParams.size(); };

  virtual SShaderParam& GetParam( int nIndex )
  {
    assert( nIndex >= 0 && nIndex < (int)m_shaderParams.size()  );
    return m_shaderParams[nIndex];
  }

  virtual const SShaderParam& GetParam( int nIndex ) const
  {
    assert( nIndex >= 0 && nIndex < (int)m_shaderParams.size()  );
    return m_shaderParams[nIndex];
  }

  virtual void SetParam( int nIndex,const SShaderParam &param )
  {
    assert( nIndex >= 0 && nIndex < (int)m_shaderParams.size()  );
    m_shaderParams[nIndex] = param;
  }

  virtual void AddParam( const SShaderParam &param )
  {
    // shouldn't add existing parameter ?
    m_shaderParams.push_back(param);
  }

  virtual void SetParam(const char *pszName, UParamVal &pParam, EParamType nType = eType_FLOAT)
  {  
    uint i;

    for (i=0; i<m_shaderParams.size(); i++)
    {
      if (!stricmp(pszName, m_shaderParams[i].m_Name))
      {
        break;
      }
    }

    if (i == m_shaderParams.size())
    {
      SShaderParam pr;
      strncpy(pr.m_Name, pszName, 32);    
      pr.m_Type = nType;
      m_shaderParams.push_back(pr);
    }

    SShaderParam::SetParam(pszName, &m_shaderParams, pParam);  
  }  

  virtual void SetShaderParams( const DynArray<SShaderParam> &pParams)
  {
    m_shaderParams = pParams;
  }

  /*virtual void AssignToRenderParams( struct SRendParams &rParams )
  {
    if (!m_shaderParams.empty())
      rParams.pShaderParams = &m_shaderParams;
  }*/

  virtual DynArray<SShaderParam> *GetShaderParams()
  {
    if (m_shaderParams.empty())
    {
      return 0;
    }

    return &m_shaderParams;
  }

  virtual const DynArray<SShaderParam> *GetShaderParams() const
  {
    if (m_shaderParams.empty())
    {
      return 0;
    }

    return &m_shaderParams;
  }

private:
  int m_nRefCount;
  DynArray<SShaderParam> m_shaderParams;
};

//////////////////////////////////////////////////////////////////////////
IShaderPublicParams* CRenderer::CreateShaderPublicParams()
{
	return new CShaderPublicParams;
}

#ifndef EXCLUDE_SCALEFORM_SDK

//////////////////////////////////////////////////////////////////////////
void CRenderer::SF_ConfigMask(ESFMaskOp maskOp, unsigned int stencilRef)
{
	int stencilFunc(FSS_STENCFUNC_ALWAYS);
	int stencilPass(FSS_STENCOP_KEEP);

	switch (maskOp)
	{
	case BeginSubmitMask_Clear:
		{
			stencilFunc = FSS_STENCFUNC_ALWAYS;
			stencilPass = FSS_STENCOP_REPLACE;
			break;
		}
	case BeginSubmitMask_Inc:
		{
			stencilFunc = FSS_STENCFUNC_EQUAL;
			stencilPass = FSS_STENCOP_INCR;
			break;
		}
	case BeginSubmitMask_Dec:
		{
			stencilFunc = FSS_STENCFUNC_EQUAL;
			stencilPass = FSS_STENCOP_DECR;
			break;
		}
	case EndSubmitMask:
		{
			stencilFunc = FSS_STENCFUNC_EQUAL;
			stencilPass = FSS_STENCOP_KEEP;
			break;
		}
	case DisableMask:
		{
			stencilFunc = FSS_STENCFUNC_ALWAYS;
			stencilPass = FSS_STENCOP_KEEP;
			break;
		}
	}

#ifdef DIRECT3D10
	EF_SetStencilState(STENC_FUNC(stencilFunc) | STENC_CCW_FUNC(stencilFunc) |
		STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_CCW_FAIL(FSS_STENCOP_KEEP) |
		STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_CCW_ZFAIL(FSS_STENCOP_KEEP) | 
		STENCOP_PASS(stencilPass) | STENCOP_CCW_PASS(stencilPass), stencilRef, 0xFFFFFFFF, 0xFFFFFFFF);
#else
	EF_SetStencilState(STENC_FUNC(stencilFunc) | STENCOP_FAIL(FSS_STENCOP_KEEP) | 
		STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(stencilPass), stencilRef, 0xFFFFFFFF, 0xFFFFFFFF);
#endif
}

//////////////////////////////////////////////////////////////////////////
int CRenderer::SF_CreateTexture(int width, int height, int numMips, unsigned char* pData, ETEX_Format eTF)
{
	char name[128];
	sprintf(name, "$SF_%d", m_TexGenID++);

	int flags(FT_DONT_STREAM | FT_DONT_RESIZE | FT_DONT_ANISO);
	flags |= !numMips ? FT_FORCE_MIPS : 0;

	CTexture* pTexture(CTexture::Create2DTexture(name, width, height, numMips, flags, pData, eTF, eTF));
	return (pTexture != 0) ? pTexture->GetID() : -1;
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::SF_GetMeshMaxSize(int& numVertices, int& numIndices) const
{
	numVertices = m_RP.m_MaxVerts;
	numIndices = m_RP.m_MaxTris * 3;
}

#endif // #ifndef EXCLUDE_SCALEFORM_SDK

//////////////////////////////////////////////////////////////////////////
uint16 CRenderer::PushFogVolumeContribution( const ColorF& fogVolumeContrib )
{
	const size_t maxElems( ( 1 << ( sizeof( uint16 ) * 8 ) ) - 1 );
	size_t numElems( m_fogVolumeContibutions.size() );
	assert( numElems < maxElems);
	if( numElems >= maxElems )
		return (uint16) -1;

	m_fogVolumeContibutions.push_back( fogVolumeContrib );
	return (uint16) ( m_fogVolumeContibutions.size() - 1 );
}

//////////////////////////////////////////////////////////////////////////
const ColorF& CRenderer::GetFogVolumeContribution( uint16 idx ) const
{
	assert( idx < m_fogVolumeContibutions.size() );
	if( idx >= m_fogVolumeContibutions.size() )
	{
		static ColorF s_defContrib( 0, 0, 0, 1 );
		return s_defContrib;
	}
	return m_fogVolumeContibutions[idx];
}


//////////////////////////////////////////////////////////////////////////
bool CRenderer::IsMultiGPUModeActive() const
{ 
	if(CV_r_multigpu==1)		// on
		return true;
	
	if(CV_r_multigpu==2)		// auto
		return m_bDriverHasActiveMultiGPU;

	return false;						// off
}

const char * CRenderer::GetTextureFormatName(ETEX_Format eTF)
{
	return CTexture::NameForTextureFormat(eTF);
}

int CRenderer::GetTextureFormatDataSize(int nWidth, int nHeight, int nDepth, int nMips, ETEX_Format eTF)
{
	return CTexture::TextureDataSize(nWidth, nHeight, nDepth, nMips, eTF);
}

//////////////////////////////////////////////////////////////////////////
ERenderType CRenderer::GetRenderType() const
{
#if defined(DIRECT3D10)
	return eRT_DX10;
#elif defined(DIRECT3D9)
#	if defined(XENON)
		return eRT_Xbox360;
#	else
		return eRT_DX9;
#	endif
#elif defined(PS3)
	return eRT_PS3;
#elif defined(NULL_RENDERER)
	return eRT_Null;
#else
	return eRT_Undefined;
#endif
}
