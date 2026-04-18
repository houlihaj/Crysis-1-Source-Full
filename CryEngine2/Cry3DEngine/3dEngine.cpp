////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   3dengine.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Implementation of I3DEngine interface methods
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <ICryAnimation.h>

#include "3dEngine.h"
#include "terrain.h"
#include "VisAreas.h"
#include "ObjMan.h"
#include "terrain_water.h"

#include "partman.h"
#include "DecalManager.h"
#include "Vegetation.h"
#include "IndexedMesh.h"
#include "WaterVolumes.h"

#include "LMCompStructures.h"
#include "LMSerializationManager.h"
#include "MatMan.h"
#include "VertexScatterSerializationManager.h"

#include "Brush.h"
#include "CullBuffer.h"
#include "CGF/CGFLoader.h"
#include "CGF/ReadOnlyChunkFile.h"

#include "CloudRenderNode.h"
#include "CloudsManager.h"
#include "SkyLightManager.h"
#include "FogVolumeRenderNode.h"
#include "RoadRenderNode.h"
#include "DecalRenderNode.h"
#include "TimeOfDay.h"
#include "VoxMan.h"
#include "LightEntity.h"
#include "FogVolumeRenderNode.h"
#include "ObjectsTree.h"
#include "WaterVolumeRenderNode.h"
#include "WaterWaveRenderNode.h"
#include "DistanceCloudRenderNode.h"
#include "VolumeObjectRenderNode.h"
#include "AutoCubeMapRenderNode.h"
#include "WaterWaveRenderNode.h"
#include "RopeRenderNode.h"
#include "RenderMeshMerger.h"
#include "PhysCallbacks.h"

//#ifdef WIN32
//#include <windows.h>
//#endif

ISystem * Cry3DEngineBase::m_pSystem=0;
IRenderer * Cry3DEngineBase::m_pRenderer=0;
ITimer * Cry3DEngineBase::m_pTimer=0;
ILog * Cry3DEngineBase::m_pLog=0;
IPhysicalWorld * Cry3DEngineBase::m_pPhysicalWorld=0;
CTerrain * Cry3DEngineBase::m_pTerrain=0;
CObjManager * Cry3DEngineBase::m_pObjManager=0;
IConsole * Cry3DEngineBase::m_pConsole=0;
C3DEngine * Cry3DEngineBase::m_p3DEngine=0;
CVars * Cry3DEngineBase::m_pCVars=0;
ICryPak * Cry3DEngineBase::m_pCryPak=0;
CPartManager   * Cry3DEngineBase::m_pPartManager=0;
CDecalManager  * Cry3DEngineBase::m_pDecalManager=0;
CSkyLightManager* Cry3DEngineBase::m_pSkyLightManager=0;
CCloudsManager * Cry3DEngineBase::m_pCloudsManager=0;
CVisAreaManager* Cry3DEngineBase::m_pVisAreaManager=0;
CWaterWaveManager  * Cry3DEngineBase::m_pWaterWaveManager=0;
CRenderMeshMerger  * Cry3DEngineBase::m_pRenderMeshMerger = 0;
CMatMan        * Cry3DEngineBase::m_pMatMan=0;
int Cry3DEngineBase::m_nRenderStackLevel=-1;
float Cry3DEngineBase::m_fZoomFactor=1.f;
int Cry3DEngineBase::m_dwRecursionDrawFlags[MAX_RECURSION_LEVELS];
int Cry3DEngineBase::m_nRenderFrameID = 0;
int Cry3DEngineBase::m_nRenderMainFrameID = 0;
bool Cry3DEngineBase::m_bProfilerEnabled = false;
int Cry3DEngineBase::m_CpuFlags=0;
float Cry3DEngineBase::m_fPreloadStartTime=0;
bool Cry3DEngineBase::m_bEditorMode = false;
ESystemConfigSpec Cry3DEngineBase::m_LightConfigSpec = CONFIG_VERYHIGH_SPEC;
IMaterialManager* Cry3DEngineBase::m_pMaterialManager = NULL;
CCamera Cry3DEngineBase::m_Camera;
int Cry3DEngineBase::m_arrInstancesCounter[eERType_Last];


#define LAST_POTENTIALLY_VISIBLE_TIME 2

//////////////////////////////////////////////////////////////////////
C3DEngine::C3DEngine(ISystem	* pSystem)
{
//#if defined(_DEBUG) && defined(WIN32)
//	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
//#endif

	memset(m_dwRecursionDrawFlags,0,sizeof(m_dwRecursionDrawFlags));

  Cry3DEngineBase::m_pSystem=pSystem;
  Cry3DEngineBase::m_pRenderer=pSystem->GetIRenderer();
  Cry3DEngineBase::m_pTimer=pSystem->GetITimer();
  Cry3DEngineBase::m_pLog=pSystem->GetILog();
  Cry3DEngineBase::m_pPhysicalWorld=pSystem->GetIPhysicalWorld();
  Cry3DEngineBase::m_pConsole=pSystem->GetIConsole();
  Cry3DEngineBase::m_p3DEngine=this;
  Cry3DEngineBase::m_pCryPak=pSystem->GetIPak();
  Cry3DEngineBase::m_pCVars=0;
	Cry3DEngineBase::m_pRenderMeshMerger = new CRenderMeshMerger;
  Cry3DEngineBase::m_CpuFlags=pSystem->GetCPUFlags();
	memset(Cry3DEngineBase::m_arrInstancesCounter,0,sizeof(Cry3DEngineBase::m_arrInstancesCounter));

	m_pCVars            = new CVars();
	Cry3DEngineBase::m_pCVars = m_pCVars;

	m_bEditorMode = pSystem->IsEditor();

#ifdef _DEBUG
#ifndef _XBOX
//  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); // _crtBreakAlloc
#endif
#endif

  m_pMatMan = new CMatMan();
  m_pMatMan->InitDefaults();

	m_pTimeOfDay = new CTimeOfDay;
 
  memset(m_SunObject, 0, sizeof(m_SunObject));  
	m_szLevelFolder[0]=0;

	m_pSun=0;
	m_nFlags=0;
  m_pSkyMat = 0;
	m_pSkyLowSpecMat = 0;
  m_pTerrainWaterMat = 0;
	m_nWaterBottomTexId=0;
  m_vSunDir = Vec3(5.f, 5.f, DISTANCE_TO_THE_SUN);
  m_vSunDirRealtime = Vec3(5.f, 5.f, DISTANCE_TO_THE_SUN).GetNormalized();

  m_pTerrain=0;	

	m_nBlackTexID = GetRenderer()->EF_LoadTexture("Textures/Defaults/black.dds",0,eTT_2D)->GetTextureID();

  // create components
  m_pObjManager   = 0;//new CObjManager (m_pSystem);

  m_pDecalManager     = 0;//new CDecalManager   (m_pSystem, this);
	m_pCloudsManager    = new CCloudsManager;
	m_pPartManager			= 0;
  m_pVisAreaManager   = 0;
	m_pSkyLightManager  = new CSkyLightManager();
  m_pWaterWaveManager = 0;

  // create REs
  m_pRESky              = (CRESky*)             GetRenderer()->EF_CreateRE(eDATA_Sky); //m_pRESky->m_fAlpha = 1.f;
	m_pREHDRSky           = (CREHDRSky*)       GetRenderer()->EF_CreateRE(eDATA_HDRSky);
  m_pREPostProcess    = (CREPostProcess*)   GetRenderer()->EF_CreateRE(eDATA_PostProcess);
  
  m_pFarTreeSprites   = GetRenderer()->EF_LoadShader("FarTreeSprites", EF_SYSTEM);

  m_pPhysMaterialEnumerator=0;

  m_fMaxViewDistHighSpec = 8000;
  m_fMaxViewDistLowSpec  = 1000;
  m_fTerrainDetailMaterialsViewDistRatio = 1.f;

  m_fSkyBoxAngle=0;
	m_fSkyBoxStretching=0;
	m_fEyeAdaptionClamp=4.0f;

	m_pGlobalWind	= 0;
	m_vWindSpeed(1,0,0);

	m_bOcean = true;
	m_bCameraUnderWater = false;
  m_nOceanRenderFlags = 0;

	m_bSunShadows = m_bShowTerrainSurface = true;

	m_nRenderLightsNum=m_nRealLightsNum=0;

  m_bHRDEnabled=0;
  m_pLightQuality = GetConsole()->GetCVar("r_Quality_BumpMapping");
  m_pResetSfx=GetConsole()->GetCVar("r_ResetScreenFx");

  m_pHRDRendering=GetConsole()->GetCVar("r_HDRRendering"); 
	m_pSphericalSkinning=GetConsole()->GetCVar("ca_SphericalSkinning");

  m_pDeformableObjects = GetConsole()->GetCVar("e_deformable_objects");

	m_pConsolepFont = NULL;

	m_pCoverageBuffer = new CCullBuffer();

	m_pFirstFoliage = m_pLastFoliage = 0;
	m_pFirstFoliage = m_pLastFoliage = (CStatObjFoliage*)((INT_PTR)&m_pFirstFoliage-(INT_PTR)&m_pFirstFoliage->m_next+(INT_PTR)m_pFirstFoliage);

	m_fHDRDynamicMultiplier=2.0f;

	m_vSkyHightlightPos.Set(0,0,0);
	m_vSkyHightlightCol.Set(0,0,0);
	m_fSkyHighlightSize = 0;

	m_volFogGlobalDensity = 0.02f;
	m_volFogGlobalDensityMultiplierLDR = 1.0f;
	m_volFogAtmosphereHeight = 4000.0f;
	m_volFogArtistTweakDensityOffset = 0.0f;

	m_volFogGlobalDensityMod = 0;
	m_volFogAtmosphereHeightMod = 0;

	m_volFogGlobalDensityMultiplier = 1.0f;

	m_pObjectsTree = NULL;

	m_idMatLeaves = -1;

/* // structures should not grow too much - commented out as in 64bit they do
	assert(sizeof(CVegetation)-sizeof(IRenderNode)<=52);
	assert(sizeof(CBrush)-sizeof(IRenderNode)<=120);
	assert(sizeof(IRenderNode)<=96);
*/
	m_oceanFogColor = 0.2f * Vec3( 29.0f, 102.0f, 141.0f ) / 255.0f;
	m_oceanFogColorShallow = Vec3(0,0,0); //0.5f * Vec3( 206.0f, 249.0f, 253.0f ) / 255.0f;
	m_oceanFogDensity = 0; //0.2f;

  m_oceanCausticsDistanceAtten = 100.0f; 
  m_oceanCausticsMultiplier = 1.0f;  
  m_oceanCausticsDarkeningMultiplier = 1.0f;

  m_oceanWindDirection = 1;
  m_oceanWindSpeed = 4.0f;
  m_oceanWavesSpeed = 5.0f;
  m_oceanWavesAmount = 1.5f;
  m_oceanWavesSize = 0.75f;
	// just some defaults until the level gets loaded
	m_sunRotationZ = 0;
	m_sunRotationLongitude = 0;
	
  m_curr_e_vegetation_sprites_distance_ratio = GetCVars()->e_vegetation_sprites_distance_ratio;
  m_curr_e_vegetation_sprites_distance_custom_ratio_min = GetCVars()->e_vegetation_sprites_distance_custom_ratio_min;
	m_curr_e_view_dist_ratio = GetCVars()->e_view_dist_ratio;
	m_curr_e_view_dist_ratio_veg = GetCVars()->e_view_dist_ratio_vegetation;
	m_curr_e_view_dist_ratio_detail = GetCVars()->e_view_dist_ratio_detail;
	m_curr_e_default_material = (int)GetCVars()->e_default_material;
  m_nGeomDetailScreenRes = GetCVars()->e_force_detail_level_for_resolution ? 
    GetCVars()->e_force_detail_level_for_resolution : GetRenderer()->GetWidth();

	if (!m_LTPRootFree.pNext)
	{
		m_LTPRootFree.pNext = &m_LTPRootFree;
		m_LTPRootFree.pPrev = &m_LTPRootFree;
	}

	if (!m_LTPRootUsed.pNext)
	{
		m_LTPRootUsed.pNext = &m_LTPRootUsed;
		m_LTPRootUsed.pPrev = &m_LTPRootUsed;
	}
/*
	CTerrainNode::SetProcObjChunkPool(new SProcObjChunkPool);
	CTerrainNode::SetProcObjPoolMan(new CProcVegetPoolMan);
  */
  m_bResetRNTmpDataPool = false;

  m_fSunDirUpdateTime = 0;
	m_vSunDirNormalized.zero();

	m_nightSkyHorizonCol = Vec3(0,0,0);
	m_nightSkyZenithCol = Vec3(0,0,0);
	m_nightSkyZenithColShift = 0;
	m_nightSkyStarIntensity = 0;
	m_moonDirection = Vec3(0,0,0);
	m_nightMoonCol = Vec3(0,0,0);
	m_nightMoonSize = 0;
	m_nightMoonInnerCoronaCol = Vec3(0,0,0);
	m_nightMoonInnerCoronaScale = 1.0f;
	m_nightMoonOuterCoronaCol = Vec3(0,0,0);
	m_nightMoonOuterCoronaScale = 1.0f;
	m_sunRotationZ = 0;
	m_sunRotationLongitude = 0;
	m_moonRotationLatitude = 0;
	m_moonRotationLongitude = 0;
	m_oceanFogColorMultiplier = 0;	
	m_skyboxMultiplier = 1.0f;
	m_dayNightIndicator = 1.0f;

  m_vPrevMainFrameCamPos.Set(0,0,0);
  m_bContentPrecacheRequested = false;

	ClearDebugFPSInfo();

	m_BoundingBoxForScattering.Reset();
}

//////////////////////////////////////////////////////////////////////
C3DEngine::~C3DEngine()
{
	delete CTerrainNode::GetProcObjPoolMan();
	CTerrainNode::SetProcObjChunkPool(NULL);

	delete CTerrainNode::GetProcObjChunkPool();
	CTerrainNode::SetProcObjPoolMan(NULL);

	assert(IsHeapValid());

	ShutDown();

	delete m_pTimeOfDay;
  delete m_pDecalManager; 
  delete m_pVisAreaManager;
  delete m_pCVars;
	delete m_pMatMan; m_pMatMan=0;
	delete m_pCoverageBuffer; m_pCoverageBuffer=0;
	delete m_pSkyLightManager;
	delete m_pObjectsTree;
  delete m_pRenderMeshMerger;
  delete m_pCloudsManager;

  SAFE_RELEASE(m_pRESky);
	SAFE_RELEASE(m_pREHDRSky);
	SAFE_RELEASE(m_pREPostProcess);
}

//////////////////////////////////////////////////////////////////////

void C3DEngine::RemoveEntInFoliage(int i)
{
	IPhysicalEntity *pent;
	pe_params_foreign_data pfd;
	if ((pent=m_pPhysicalWorld->GetPhysicalEntityById(m_arrEntsInFoliage[i].id)) && pent->GetParams(&pfd) && 
			(pfd.iForeignFlags>>8&255)==i+1)
	{
		MARK_UNUSED pfd.pForeignData,pfd.iForeignData;
		pfd.iForeignFlags &= 255;
		pent->SetParams(&pfd,1);
	}
	int j = m_arrEntsInFoliage.size()-1;
	if (i<j)
	{
		m_arrEntsInFoliage[i] = m_arrEntsInFoliage[j];
		if ((pent=m_pPhysicalWorld->GetPhysicalEntityById(m_arrEntsInFoliage[i].id)) && pent->GetParams(&pfd) && 
			(pfd.iForeignFlags>>8&255)==j+1)
		{
			MARK_UNUSED pfd.pForeignData,pfd.iForeignData;
      pfd.iForeignFlags = pfd.iForeignFlags & 255 | (i+1)<<8;
			pent->SetParams(&pfd,1);
		}
	}
	m_arrEntsInFoliage.DeleteLast();
}

bool C3DEngine::Init()
{	  	
  ShutDown();

	if (!gEnv->pSystem->IsDedicated())
		m_pPartManager = new CPartManager();
	else
		m_pPartManager = 0;

	ICryFont *pCryFont = GetSystem()->GetICryFont();
	if (pCryFont)
	{
		m_pConsolepFont = pCryFont->GetFont("console");
	}
	if (GetPhysicalWorld())
	{
		CPhysCallbacks::Init();
	}
 
	return  (true);
}

bool C3DEngine::IsCameraAnd3DEngineInvalid(const CCamera cam, const char * szCaller)
{
  const Vec3& vCamPos = cam.GetPosition();
//	const Ang3 vAngles = Ang3::GetAnglesXYZ(Matrix33(cam.GetMatrix()));
  const float fFov      = cam.GetFov();

	if (!m_pObjManager || !m_pDecalManager)
	{
		//PrintMessage("Warning: %s: Engine not initialized or level not loaded");
		return (true); 
	}

	if( GetMaxViewDistance()<=0 || !_finite(vCamPos.x) || !_finite(vCamPos.y) || !_finite(vCamPos.z) ||
      vCamPos.z < -100000.f || vCamPos.z > 100000.f || fFov < 0.0001f || fFov > gf_PI)
  {
    Warning("%s: Camera undefined : Pos=(%.1f, %.1f, %.1f), Fov=%.1f, MaxViewDist=%.1f", 
      szCaller, 
      vCamPos.x, vCamPos.y, vCamPos.z, 
      fFov, GetMaxViewDistance());
    return true;
  }

  return false;
}

void C3DEngine::OnFrameStart()
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_pPartManager)
		m_pPartManager->OnFrameStart();
}

float g_oceanLevel,g_oceanStep;
float GetOceanLevelCallback(int ix,int iy)
{
	return gEnv->p3DEngine->GetOceanWaterLevel(Vec3(ix*g_oceanStep,iy*g_oceanStep,g_oceanLevel));	
}
unsigned char GetOceanSurfTypeCallback(int ix,int iy)
{
	return 0;	
}

void C3DEngine::Update()
{
	m_bProfilerEnabled = GetISystem()->GetIProfileSystem()->IsProfiling();

  FUNCTION_PROFILER_3DENGINE;

	m_LightConfigSpec = (ESystemConfigSpec)GetCurrentLightSpec();

///	if(m_pVisAreaManager)
	//	m_pVisAreaManager->Preceche(m_pObjManager);

	if (GetObjManager())
		GetObjManager()->ClearStatObjGarbage();

	if(m_pTerrain)
  {
    m_pTerrain->Voxel_Recompile_Modified_Incrementaly_Objects();
    m_pTerrain->Recompile_Modified_Incrementaly_RoadRenderNodes();
  }

	if(m_pDecalManager)
		m_pDecalManager->Update(GetTimer()->GetFrameTime());

	if(GetCVars()->e_precache_level == 3)
		PrecacheLevel(true,0,0);

	if(GetCVars()->e_voxel_build)
		GetTerrain()->BuildVoxelSpace();

	DebugDraw_Draw();

	////////////////////////////////////////////////////////////////////////////////////////
	// Process cvars change
	////////////////////////////////////////////////////////////////////////////////////////

	if ((GetCVars()->e_view_dist_ratio != m_curr_e_view_dist_ratio) ||
			(GetCVars()->e_view_dist_ratio_detail != m_curr_e_view_dist_ratio_detail)||
			(GetCVars()->e_view_dist_ratio_vegetation != m_curr_e_view_dist_ratio_veg))
	{
		float fMapSizeMeters = GetTerrainSize();
		GetObjManager()->ReregisterEntitiesInArea(
			Vec3(-fMapSizeMeters,-fMapSizeMeters,-fMapSizeMeters), 
			Vec3(fMapSizeMeters*2.f,fMapSizeMeters*2.f,fMapSizeMeters*2.f));

		m_curr_e_view_dist_ratio = (int)GetCVars()->e_view_dist_ratio;
		m_curr_e_view_dist_ratio_veg = (int)GetCVars()->e_view_dist_ratio_vegetation;
		m_curr_e_view_dist_ratio_detail = (int)GetCVars()->e_view_dist_ratio_detail;
	}

	if(GetCVars()->e_default_material != m_curr_e_default_material)
	{
		GetTerrain()->ResetTerrainVertBuffers();
		m_curr_e_default_material = (int)GetCVars()->e_default_material;
	}

  int nGeomDetailScreenRes = GetCVars()->e_force_detail_level_for_resolution ? 
    GetCVars()->e_force_detail_level_for_resolution : GetRenderer()->GetWidth();

  if( GetCVars()->e_vegetation_sprites_distance_ratio != m_curr_e_vegetation_sprites_distance_ratio ||
      GetCVars()->e_vegetation_sprites_distance_custom_ratio_min != m_curr_e_vegetation_sprites_distance_custom_ratio_min ||
      m_nGeomDetailScreenRes != nGeomDetailScreenRes )
  {
    m_curr_e_vegetation_sprites_distance_ratio = GetCVars()->e_vegetation_sprites_distance_ratio;
    m_curr_e_vegetation_sprites_distance_custom_ratio_min = GetCVars()->e_vegetation_sprites_distance_custom_ratio_min;
    m_nGeomDetailScreenRes = nGeomDetailScreenRes;
    UpdateStatInstGroups();
  }

	float dt = GetTimer()->GetFrameTime();
	CStatObjFoliage *pFoliage,*pFoliageNext;
	for(pFoliage=m_pFirstFoliage; &pFoliage->m_next!=&m_pFirstFoliage; pFoliage=pFoliageNext) {
		pFoliageNext = pFoliage->m_next;
		pFoliage->Update(dt);
	}
	for(int i=m_arrEntsInFoliage.size()-1;i>=0;i--) if ((m_arrEntsInFoliage[i].timeIdle+=dt)>0.3f)
		RemoveEntInFoliage(i);

	pe_params_area pa;
	IPhysicalEntity *pArea;
	if ((pArea = gEnv->pPhysicalWorld->AddGlobalArea())->GetParams(&pa))
		if (GetCVars()->e_phys_ocean_cell>0 && (!pa.pGeom || g_oceanStep!=GetCVars()->e_phys_ocean_cell))
		{
			pa = pe_params_area();
			primitives::heightfield hf;
			hf.origin.zero().z = g_oceanLevel = GetWaterLevel();
			g_oceanStep = GetCVars()->e_phys_ocean_cell;
			hf.size.set((int)(8192/g_oceanStep),(int)(8192/g_oceanStep));
			hf.step.set(g_oceanStep,g_oceanStep);
			hf.fpGetHeightCallback = GetOceanLevelCallback;
			hf.fpGetSurfTypeCallback = GetOceanSurfTypeCallback;
			hf.Basis.SetIdentity();
			hf.bOriented = 0;
			hf.typemask = 255;
			hf.typehole = 255;
			hf.heightscale = 1.0f;
			pa.pGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::heightfield::type, &hf);
			pArea->SetParams(&pa);

			Vec4 par0,par1;
			GetOceanAnimationParams(par0,par1);
			pe_params_buoyancy pb;
			pb.waterFlow = Vec3(par1.z,par1.y,0)*(par0.z*0.25f); // tone down the speed
			//pArea->SetParams(&pb);
		} 
		else if (GetCVars()->e_phys_ocean_cell<=0 && pa.pGeom)
		{
			pa = pe_params_area();
			pa.pGeom = 0;
			pArea->SetParams(&pa);
			pe_params_buoyancy pb;
			pb.waterFlow.zero();
			pArea->SetParams(&pb);
		}
}

//////////////////////////////////////////////////////////////////////
void C3DEngine::UpdateScene()
{	
	if(GetCVars()->e_sun)
		UpdateSunLightSource();

	FindPotentialLightSources();

	// Set traceable fog volume areas
	CFogVolumeRenderNode::SetTraceableArea( AABB( GetCamera().GetPosition() - Vec3( 1024.0f, 1024.0f, 1024.0f ), GetCamera().GetPosition() + Vec3( 1024.0f, 1024.0f, 1024.0f ) ) );

	// streaming timing update
	CStatObj::m_fStreamingTimePerFrame-=CGF_STREAMING_MAX_TIME_PER_FRAME;
	if(CStatObj::m_fStreamingTimePerFrame<0)
		CStatObj::m_fStreamingTimePerFrame=0;
	if(CStatObj::m_fStreamingTimePerFrame>0.25f)
		CStatObj::m_fStreamingTimePerFrame=0.25f;

	if(m_pObjManager)
		m_pObjManager->PreloadNearObjects();
	/*
	if(GetCVars()->e_stream_preload_textures && Cry3DEngineBase::m_nRenderStackLevel==0)
	{
		m_fPreloadStartTime = GetCurTimeSec();
		bool bPreloadOutdoor = m_pVisAreaManager->PreloadResources();

		//    for(int p=0; p<16; p++)
		if(bPreloadOutdoor && m_pTerrain->PreloadResources() && m_pSkyBoxMat)
		{
			FRAME_PROFILER( "Renderer::EF_PrecacheResource", GetSystem(), PROFILE_RENDERER );
			GetRenderer()->EF_PrecacheResource(m_pSkyBoxMat->GetShaderItem().m_pShader, 0, 1.f, 0);
		}
	}
	*/
	/*
		// precache tarrain data if camera was teleported more than 32 meters
		if(m_nRenderStackLevel==0)
		{
			if(GetDistance(m_pTerrain->m_vPrevCameraPos, GetCamera().GetPosition()) > 32)
				m_pTerrain->PreCacheArea(GetCamera().GetPosition(), GetCamera().GetZMax()*1.5f);
				m_pTerrain->m_vPrevCameraPos = GetCamera().GetPosition();
		}
	*/
}

//////////////////////////////////////////////////////////////////////
void C3DEngine::ShutDown()
{
	PrintMessage("Removing physicalized foliages ...");
	CStatObjFoliage *pFoliage,*pFoliageNext;
	for(pFoliage=m_pFirstFoliage; &pFoliage->m_next!=&m_pFirstFoliage; pFoliage=pFoliageNext) {
		pFoliageNext = pFoliage->m_next;
		delete pFoliage;
	}
	m_arrEntsInFoliage.Reset();

	if(GetRenderer() != GetSystem()->GetIRenderer())
		GetSystem()->Error("Renderer was deallocated before I3DEngine::ShutDown() call");

	ClearRenderResources();

  // reinit console variables
//  delete m_pCVars;
//  m_pCVars = new CVars(m_pSystem);

  PrintMessage("Removing lights ...");
  for(int i=0; i<m_lstDynLights.Count(); i++)
  {
    CDLight * pLight = &m_lstDynLights[i];
    FreeLightSourceComponents(pLight);
  }
  DeleteAllStaticLightSources();

  PrintMessage("Deleting visareas ...");
  delete m_pVisAreaManager;
  m_pVisAreaManager = 0;

  PrintMessage("Deleting terrain ...");
  delete m_pTerrain;
  m_pTerrain=0;

	PrintMessage("Deleting outdoor ...");
	delete m_pObjectsTree;
	m_pObjectsTree = 0;
	if (m_pGlobalWind != 0)
	{
		GetPhysicalWorld()->DestroyPhysicalEntity( m_pGlobalWind );
		m_pGlobalWind = 0;
	}

	UnlockCGFResources();

	PrintMessage("PartManager shutdown ...");
  delete m_pPartManager;
	m_pPartManager=0;

  PrintMessage("ObjManager shutdown ...");
  delete m_pObjManager;
  m_pObjManager=0;

	if (GetPhysicalWorld())
	{
		CPhysCallbacks::Done();
	}

	assert(IsHeapValid());

	SAFE_RELEASE(m_pSkyMat);
	SAFE_RELEASE(m_pSkyLowSpecMat);

	FreeRNTmpDataPool();
}

#ifdef WIN64
#pragma warning( push )									//AMD Port
#pragma warning( disable : 4311 )
#endif

//////////////////////////////////////////////////////////////////////
void C3DEngine::SetupCamera(const CCamera & newCam) 
{
  if(IsCameraAnd3DEngineInvalid(newCam, "C3DEngine::SetCamera"))
    return;

	SetCamera(newCam);
//PS3HACK -> remove due to not loaded animations the hardcoded camera might be underwater but we dont want any underwater rendering yet
#if defined(PS3)
	m_bCameraUnderWater = false;//IsUnderWater(newCam.GetPosition());
#else
	m_bCameraUnderWater = IsUnderWater(newCam.GetPosition());
#endif

	GetRenderer()->SetCamera(newCam);
}

#ifdef WIN64
#pragma warning( pop )									//AMD Port
#endif

IStatObj* C3DEngine::LoadStatObj( const char *szFileName,const char *szGeomName,IStatObj::SSubObject **ppSubObject )
{
  if(!szFileName || !szFileName[0])
  {
		assert(0);			// don't call this function without proper data
    Warning( "I3DEngine::LoadStatObj: filename is not specified" );
    return 0;
  }

  if(!m_pObjManager)
    m_pObjManager = new CObjManager();

  return m_pObjManager->LoadStatObj(szFileName, szGeomName, ppSubObject,false);
}

Vec3 C3DEngine::GetEntityRegisterPoint( IRenderNode* pEnt )
{
  AABB aabb = pEnt->GetBBox();

  Vec3 vPoint;
   
  if(pEnt->m_dwRndFlags & ERF_REGISTER_BY_POSITION)
  {
    vPoint = pEnt->GetPos();  
    vPoint.z += 0.25f;

		// check for valid position
    if(aabb.GetDistance(vPoint)>128.f)
      Warning("I3DEngine::RegisterEntity: entity reports invalid position: Name: %s (%s), Class: %s, Pos=(%.1f,%.1f,%.1f), aabb centre=(%.1f,%.1f,%.1f)", 
      pEnt->GetName(), pEnt->GetDebugString('s'), pEnt->GetEntityClassName(), pEnt->GetPos().x, pEnt->GetPos().y, pEnt->GetPos().z, aabb.GetCenter().x, aabb.GetCenter().y, aabb.GetCenter().z);

    // clamp by bbox
    vPoint.CheckMin(aabb.max);
    vPoint.CheckMax(aabb.min+Vec3(0,0,.5f));
  }
  else
    vPoint = aabb.GetCenter();  

  return vPoint;
}

void C3DEngine::RegisterEntity( IRenderNode* pEnt )
{
	FUNCTION_PROFILER_3DENGINE;

	if(m_nRenderStackLevel>=0)
	{
		Warning("I3DEngine::RegisterEntity: RegisterEntity is called for %s during 3DEngine::Render() call - ignored", pEnt->GetName());
    assert(0);
		return;
	}

#ifdef _DEBUG // crash test basically
  const char * szClass = pEnt->GetEntityClassName();
  const char * szName = pEnt->GetName();
  if(!szName[0] && !szClass[0])
    Warning("I3DEngine::RegisterEntity: Entity undefined"); // do not register undefined objects
//  if(strstr(szName,"Dude"))
  //  int y=0;
#endif

  AABB aabb(pEnt->GetBBox());
  float fObjRadius = aabb.GetRadius();
  
  if(pEnt->m_pRNTmpData)
    pEnt->m_pRNTmpData->userData.m_OcclState.vLastVisPoint.Set(0,0,0);

  if(GetCVars()->e_obj_fast_register && pEnt->m_pOcNode && ((COctreeNode*)pEnt->m_pOcNode)->IsRightNode(aabb, fObjRadius, pEnt->m_fWSMaxViewDist))
  { // same octree node
    Vec3 vEntCenter = GetEntityRegisterPoint( pEnt );

    if(pEnt->GetEntityVisArea() && pEnt->GetEntityVisArea()->IsPointInsideVisArea(vEntCenter))
      return; // same visarea

    IVisArea * pVisAreaFromPos = (!m_pVisAreaManager || pEnt->GetRndFlags()&ERF_OUTDOORONLY) ? NULL : GetVisAreaManager()->GetVisAreaFromPos(vEntCenter);
    if(pVisAreaFromPos == pEnt->GetEntityVisArea())
      return; // same visarea or same outdoor
  }

  if(pEnt->m_pOcNode)
	  UnRegisterEntity( pEnt );

  pEnt->m_fWSMaxViewDist = pEnt->GetMaxViewDist();

  EERType eERType = pEnt->GetRenderNodeType();

  if(eERType != eERType_Light)
  {
    if(fObjRadius > MAX_VALID_OBJECT_VOLUME || !_finite(fObjRadius) || fObjRadius<=0)
    {
      Warning("I3DEngine::RegisterEntity: Object has invalid bbox: name: %s, class name: %s, GetRadius() = %.2f",
        pEnt->GetName(), pEnt->GetEntityClassName(), fObjRadius);
      return; // skip invalid objects - usually only objects with invalid very big scale will reach this point
    }

    if(pEnt->m_dwRndFlags & ERF_RENDER_ALWAYS)
      if(m_lstAlwaysVisible.Find(pEnt)<0)
        m_lstAlwaysVisible.Add(pEnt);
  }

	if(eERType == eERType_Vegetation)
	{
		CVegetation * pInst = (CVegetation*)pEnt;
		pInst->UpdateRndFlags();
	}

	//////////////////////////////////////////////////////////////////////////
	// Check for occlusion proxy.
	{
		bool bHasProxy = false;
		if(CStatObj * pStatObj = (CStatObj *)pEnt->GetEntityStatObj(0))
			if(pStatObj->m_bHaveOcclusionProxy)
				bHasProxy = true;
		if (bHasProxy)
		{
			pEnt->m_dwRndFlags |=	ERF_GOOD_OCCLUDER;
			pEnt->m_nInternalFlags |=	IRenderNode::HAS_OCCLUSION_PROXY;
		}
	}
	//////////////////////////////////////////////////////////////////////////

	if(pEnt->GetRndFlags()&ERF_OUTDOORONLY || !(m_pVisAreaManager && m_pVisAreaManager->SetEntityArea(pEnt, aabb, fObjRadius)))
	{
		if(!m_pObjectsTree)
			m_pObjectsTree = new COctreeNode(AABB(Vec3(0,0,0),Vec3(GetTerrainSize(),GetTerrainSize(),GetTerrainSize())), NULL);

    m_pObjectsTree->InsertObject(pEnt, aabb, fObjRadius, aabb.GetCenter());
	}
}

bool C3DEngine::UnRegisterEntity( IRenderNode* pEnt )
{
	FUNCTION_PROFILER_3DENGINE;

//	assert(m_nRenderStackLevel<0);
//	if(m_nRenderStackLevel>=0)
	//	Warning("C3DEngine::UnRegisterEntity: UnRegisterEntity is called for %s during 3DEngine::Render() call", pEnt->GetName());

#ifdef _DEBUG // crash test basically
	const char * szClass = pEnt->GetEntityClassName();
	const char * szName = pEnt->GetName();
	if(!szName[0] && !szClass[0])
		Warning("C3DEngine::RegisterEntity: Entity undefined");
#endif

  bool bFound = false;
	pEnt->PhysicalizeFoliage(false);

	if(pEnt->m_pOcNode)
		bFound = ((COctreeNode*)pEnt->m_pOcNode)->DeleteObject(pEnt);

  if(pEnt->m_dwRndFlags & ERF_RENDER_ALWAYS)
    m_lstAlwaysVisible.Delete(pEnt);

  return bFound;
}

bool C3DEngine::CreateDecalOnStatObj( const CryEngineDecalInfo& decal, CDecal* pCallerManagedDecal )
{
  FUNCTION_PROFILER( gEnv->pSystem, PROFILE_3DENGINE );
	
  if( !GetCVars()->e_decals && !pCallerManagedDecal )
		return false;

	CryEngineDecalInfo decal_adjusted = decal;
	decal_adjusted.bAdjustPos = true;
	return m_pDecalManager->Spawn( decal_adjusted, GetMaxViewDistance(), GetTerrain(), pCallerManagedDecal );
}

void C3DEngine::SelectEntity(IRenderNode *pEntity)
{
  static IRenderNode *pSelectedNode; 
  static float fLastTime;
  if (pEntity && GetCVars()->e_decals == 2)
  {
    float fCurTime = GetISystem()->GetITimer()->GetAsyncCurTime();
    if (fCurTime-fLastTime < 1.0f)
      return;
    fLastTime = fCurTime;
    if (pSelectedNode)
      pSelectedNode->SetRndFlags(ERF_SELECTED, false);
    pEntity->SetRndFlags(ERF_SELECTED, true);
    pSelectedNode = pEntity;
  }
}

void C3DEngine::CreateDecal(const struct CryEngineDecalInfo & decal)
{
  if(!GetCVars()->e_decals_allow_game_decals)
    return;

	if(GetCVars()->e_decals == 2)
  {
    IRenderNode * pRN = decal.ownerInfo.pRenderNode;
    PrintMessage("Debug: C3DEngine::CreateDecal: Pos=(%.1f,%.1f,%.1f) Size=%.2f DecalMaterial=%s HitObjectName=%s(%s)", 
      decal.vPos.x, decal.vPos.y, decal.vPos.z, decal.fSize, decal.szMaterialName,
      pRN ? pRN->GetName() : "NULL", pRN ? pRN->GetEntityClassName() : "NULL");
  }

	if(decal.ownerInfo.pRenderNode && !decal.ownerInfo.pRenderNode->m_pOcNode)
		return;

  // decal on CHR, ownerInfo.nPartID is bone id
  if(decal.ownerInfo.pRenderNode)
    if(ICharacterInstance * pChar = decal.ownerInfo.pRenderNode->GetEntityCharacter(0))
  {
    if(pChar->GetOjectType() == CHR)
    {
      CryEngineDecalInfo DecalLCS = decal;
      pChar->CreateDecal(DecalLCS);
    }
  }

  static uint nGroupId = 0; nGroupId++;

  if(decal.ownerInfo.pRenderNode && decal.fSize>.5f)
  {
    PodArray<SRNInfo> lstEntities;
    Vec3 vRadius(decal.fSize,decal.fSize,decal.fSize);
    const AABB cExplosionBox(decal.vPos - vRadius, decal.vPos + vRadius);

    if(CVisArea * pArea = (CVisArea *)decal.ownerInfo.pRenderNode->GetEntityVisArea())
    {
      if(pArea->m_pObjectsTree)
        pArea->m_pObjectsTree->MoveObjectsIntoList(&lstEntities, &cExplosionBox, false, true, true, true);
    }
    else
      Get3DEngine()->m_pObjectsTree->MoveObjectsIntoList(&lstEntities, &cExplosionBox, false, true, true, true);

    for(int i=0; i<lstEntities.Count(); i++)
    {
      // decals on statobj's of render node
      CryEngineDecalInfo decalOnRenderNode = decal;
      decalOnRenderNode.ownerInfo.pRenderNode = lstEntities[i].pNode;
      decalOnRenderNode.nGroupId = nGroupId;

      if(decalOnRenderNode.ownerInfo.pRenderNode->GetRenderNodeType() != decal.ownerInfo.pRenderNode->GetRenderNodeType())
        continue;

      if(decalOnRenderNode.ownerInfo.pRenderNode->GetRndFlags()&ERF_HIDDEN)
        continue;

      m_pDecalManager->SpawnHierarhical( decalOnRenderNode, GetMaxViewDistance(), GetTerrain(), NULL );
    }
  }
  else
  {
    CryEngineDecalInfo decalStatic = decal;
    decalStatic.nGroupId = nGroupId;
    m_pDecalManager->SpawnHierarhical( decalStatic, GetMaxViewDistance(), GetTerrain(), NULL );
  }
}

/*
float C3DEngine::GetDayTime(float fPrevDayTime) 
{ 
  if(fPrevDayTime)
  {
    float fDayTimeDiff = (GetCVars()->e_day_time - fPrevDayTime);
    while(fDayTimeDiff>12)
      fDayTimeDiff-=12;
    while(fDayTimeDiff<-12)
      fDayTimeDiff+=12;  
    fDayTimeDiff = (float)fabs(fDayTimeDiff);
    return fDayTimeDiff;
  }

  return GetCVars()->e_day_time; 
} */

namespace
{
	inline const float GetSunSkyRel(const Vec3& crSkyColor, const Vec3& crSunColor)
	{
		const float cSunMax = max(0.1f, max(crSunColor.x, max(crSunColor.y, crSunColor.z)));
		const float cSkyMax = max(0.1f, max(crSkyColor.x, max(crSkyColor.y, crSkyColor.z)));
		return cSunMax / (cSkyMax + cSunMax);
	}
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::SetSkyColor(Vec3 vColor)
{
	if (m_pObjManager)
	{
		m_pObjManager->m_vSkyColor = vColor;
		m_pObjManager->m_fSkyBrightness = 
			m_pObjManager->m_vSkyColor.GetLength()/(m_pObjManager->m_vSkyColor.GetLength()+m_pObjManager->m_vSunColor.GetLength());
		
		m_pObjManager->m_fSunSkyRel = GetSunSkyRel(m_pObjManager->m_vSkyColor, m_pObjManager->m_vSunColor);
	}
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::SetSunColor(Vec3 vColor)
{
	if (m_pObjManager)
	{
		m_pObjManager->m_vSunColor = vColor;
		m_pObjManager->m_fSkyBrightness = 
			m_pObjManager->m_vSkyColor.GetLength()/(m_pObjManager->m_vSkyColor.GetLength()+m_pObjManager->m_vSunColor.GetLength());

		m_pObjManager->m_fSunSkyRel = GetSunSkyRel(m_pObjManager->m_vSkyColor, m_pObjManager->m_vSunColor);
	}
}

void C3DEngine::SetSkyBrightness(float fMul)
{
	if (m_pObjManager)
		m_pObjManager->m_fSkyBrightMul = fMul;
}

void C3DEngine::SetSSAOAmount(float fMul)
{
  if (m_pObjManager)
    m_pObjManager->m_fSSAOAmount = fMul;
}

float C3DEngine::GetTerrainElevation(float x, float y, bool bIncludeOutdoorVoxles)
{
  return m_pTerrain ? m_pTerrain->GetZApr(x, y, bIncludeOutdoorVoxles) : 0;
}

float C3DEngine::GetTerrainZ(int x, int y)
{
  if(x<0 || y<0 || x>=CTerrain::GetTerrainSize() || y>=CTerrain::GetTerrainSize())
		return TERRAIN_BOTTOM_LEVEL;
  return m_pTerrain ? m_pTerrain->GetZ(x, y) : 0;
}

int C3DEngine::GetHeightMapUnitSize()
{
  return CTerrain::GetHeightMapUnitSize();
}

int C3DEngine::GetTerrainSize()
{
  return CTerrain::GetTerrainSize();
}

void C3DEngine::RemoveAllStaticObjects()
{
  if(m_pTerrain)
	  m_pTerrain->RemoveAllStaticObjects();
}

void C3DEngine::SetTerrainSurfaceType(int x, int y, int nType)
{
	assert(0);
//  if(m_pTerrain)
  //  m_pTerrain->SetSurfaceType(x,y,nType);
}

IMaterial* C3DEngine::GetTerrainSurfaceMaterial(int x,int y)
{
	if (m_pTerrain != NULL)
	{
		int surfaceTypeID = m_pTerrain->GetSurfaceTypeID(x,y);

		if (surfaceTypeID != STYPE_HOLE)
		{
			SSurfaceType *pLayers = m_pTerrain->GetSurfaceTypes();

			if (pLayers[surfaceTypeID].pLayerMat != NULL)
			{
				return pLayers[surfaceTypeID].pLayerMat;
			}
		}
	}

	return NULL;
}

void C3DEngine::SetTerrainSectorTexture( const int nTexSectorX, const int nTexSectorY, unsigned int textureId )
{
	if(m_pTerrain)
		m_pTerrain->SetTerrainSectorTexture(nTexSectorX, nTexSectorY, textureId);
}

void C3DEngine::OnExplosion(Vec3 vPos, float fRadius, bool bDeformTerrain)
{
  if(GetCVars()->e_decals == 2)
    PrintMessage("Debug: C3DEngine::OnExplosion: Pos=(%.1f,%.1f,%.1f) fRadius=%.2f", vPos.x, vPos.y, vPos.z, fRadius);

  if(vPos.x<0 || vPos.x>=CTerrain::GetTerrainSize() || vPos.y<0 || vPos.y>=CTerrain::GetTerrainSize() || fRadius<=0)
    return; // out of terrain

	// do not create decals near the terrain holes
  for(int x=int(vPos.x-fRadius); x<=int(vPos.x+fRadius)+1; x+=CTerrain::GetHeightMapUnitSize())
  for(int y=int(vPos.y-fRadius); y<=int(vPos.y+fRadius)+1; y+=CTerrain::GetHeightMapUnitSize())
  if(m_pTerrain->GetHole(x,y))
    return;

	// try to remove objects around
	bool bGroundDeformationAllowed = m_pTerrain->RemoveObjectsInArea(vPos,fRadius) && bDeformTerrain && GetCVars()->e_terrain_deformations;

	// reduce ground decals size depending on distance to the ground
	float fExploHeight = vPos.z - GetTerrain()->GetZApr(vPos.x,vPos.y);

	if(bGroundDeformationAllowed && (fExploHeight > -0.1f) && fExploHeight<fRadius && fRadius>0.125f)
  {
    m_pTerrain->MakeCrater(vPos,fRadius);

    // update roads
    if(m_pObjectsTree)
    {
      PodArray<IRenderNode*> lstRoads;
			AABB bbox(vPos-Vec3(fRadius,fRadius,fRadius), vPos+Vec3(fRadius,fRadius,fRadius));
      m_pObjectsTree->GetObjectsByType(lstRoads, eERType_RoadObject_NEW, &bbox);
      for(int i=0; i<lstRoads.Count(); i++)
      {
        CRoadRenderNode * pRoad = (CRoadRenderNode *)lstRoads[i];
        pRoad->OnTerrainChanged();
      }
    }
  }

	// delete decals what can not be correctly updated
	Vec3 vRadius(fRadius,fRadius,fRadius);
  AABB areaBox(vPos-vRadius, vPos+vRadius);
	Get3DEngine()->DeleteDecalsInRange( &areaBox, NULL );
}

float C3DEngine::GetMaxViewDistance( )
{
  // spec lerp factor
  float lerpSpec = SATURATE(GetCVars()->e_max_view_dst_spec_lerp);
	
  // camera height lerp factor
  float lerpHeight = SATURATE(max( 0.f, GetSystem()->GetViewCamera().GetPosition().z - GetWaterLevel()) / max( 1.f, GetCVars()->e_max_view_dst_full_dist_cam_height ));
  
  // lerp between specs
  float fMaxViewDist = m_fMaxViewDistLowSpec * (1.f - lerpSpec) + m_fMaxViewDistHighSpec * lerpSpec;

  // lerp between prev result and high spec
  fMaxViewDist = fMaxViewDist * (1.f - lerpHeight) + m_fMaxViewDistHighSpec * lerpHeight;

  // for debugging
  if(GetCVars()->e_max_view_dst>=0)
    fMaxViewDist = GetCVars()->e_max_view_dst;

  return fMaxViewDist;
}

void C3DEngine::SetFogColor(const Vec3& vFogColor)
{
  m_vFogColor    = vFogColor;
	GetRenderer()->SetClearColor(m_vFogColor);
}

Vec3 C3DEngine::GetFogColor( )
{
  return m_vFogColor;
}

void C3DEngine::GetVolumetricFogSettings( float& globalDensity, float& atmosphereHeight, float& artistTweakDensityOffset, float& globalDensityMultiplierLDR )
{
	globalDensity = m_volFogGlobalDensity;
	globalDensityMultiplierLDR = m_volFogGlobalDensityMultiplierLDR;
	atmosphereHeight = m_volFogAtmosphereHeight;
	artistTweakDensityOffset = m_volFogArtistTweakDensityOffset;
}

void C3DEngine::SetVolumetricFogSettings( float globalDensity, float atmosphereHeight, float artistTweakDensityOffset )
{
	m_volFogGlobalDensity = ( globalDensity < 0.0f ) ? 0.0f : globalDensity;
	m_volFogAtmosphereHeight = ( atmosphereHeight < 1.0f ) ? 1.0f : atmosphereHeight;
	m_volFogArtistTweakDensityOffset = ( artistTweakDensityOffset < 0.0f ) ? 0.0f : artistTweakDensityOffset;
}

void C3DEngine::GetVolumetricFogModifiers(float& globalDensityModifier, float& atmosphereHeightModifier)
{
	globalDensityModifier = m_volFogGlobalDensityMod;
	atmosphereHeightModifier = m_volFogAtmosphereHeightMod;
}	

void C3DEngine::SetVolumetricFogModifiers(float globalDensityModifier, float atmosphereHeightModifier, bool reset)
{
	if (reset)
	{
		m_volFogGlobalDensityMod = globalDensityModifier;
		m_volFogAtmosphereHeightMod = atmosphereHeightModifier;
	}
	else
	{
		m_volFogGlobalDensityMod += globalDensityModifier;
		m_volFogAtmosphereHeightMod += atmosphereHeightModifier;
	}
}

void C3DEngine::GetVolumetricFogMultipliers(float& globalDensityMultiplier)
{
	globalDensityMultiplier = m_volFogGlobalDensityMultiplier;
}

void C3DEngine::SetVolumetricFogMultipliers(float globalDensityMultiplier)
{
	m_volFogGlobalDensityMultiplier = ( globalDensityMultiplier < 0.0f ) ? 0.0f : globalDensityMultiplier;
}

void C3DEngine::GetSkyLightParameters( Vec3& sunIntensity, float& Km, float& Kr, float& g, Vec3& rgbWaveLengths )
{
	CSkyLightManager::SSkyDomeCondition skyCond;
	m_pSkyLightManager->GetCurSkyDomeCondition( skyCond );

	g = skyCond.m_g;
	Km = skyCond.m_Km;
	Kr = skyCond.m_Kr;
	sunIntensity = skyCond.m_sunIntensity;
	rgbWaveLengths = skyCond.m_rgbWaveLengths;
}

void C3DEngine::SetSkyLightParameters( const Vec3& sunIntensity, float Km, float Kr, float g, const Vec3& rgbWaveLengths, bool forceImmediateUpdate )
{
	CSkyLightManager::SSkyDomeCondition skyCond;

	skyCond.m_g = g;
	skyCond.m_Km = Km;
	skyCond.m_Kr = Kr;
	skyCond.m_sunIntensity = sunIntensity;
	skyCond.m_rgbWaveLengths = rgbWaveLengths;
	skyCond.m_sunDirection = m_vSunDir;
	
	m_pSkyLightManager->SetSkyDomeCondition( skyCond );
	if( false != forceImmediateUpdate )
	{
		m_pSkyLightManager->FullUpdate();
	}
}

float C3DEngine::GetHDRDynamicMultiplier() const
{
	return m_fHDRDynamicMultiplier;
}

void C3DEngine::SetHDRDynamicMultiplier( const float value )
{
	m_fHDRDynamicMultiplier = clamp_tpl(value,0.01f,100.0f);
}

void C3DEngine::TraceFogVolumes( const Vec3& worldPos, ColorF& fogVolumeContrib )
{
	CFogVolumeRenderNode::TraceFogVolumes( worldPos, fogVolumeContrib );
}

void C3DEngine::GetCloudShadingMultiplier( float& sunLightMultiplier, float& skyLightMultiplier )
{
	sunLightMultiplier = m_fCloudShadingSunLightMultiplier;
	skyLightMultiplier = m_fCloudShadingSkyLightMultiplier;
}

void C3DEngine::SetCloudShadingMultiplier( float sunLightMultiplier, float skyLightMultiplier )
{
	m_fCloudShadingSunLightMultiplier = ( sunLightMultiplier >= 0 ) ? sunLightMultiplier : 0;
	m_fCloudShadingSkyLightMultiplier = ( skyLightMultiplier >= 0 ) ? skyLightMultiplier : 0;
}

void C3DEngine::SetPhysMaterialEnumerator(IPhysMaterialEnumerator * pPhysMaterialEnumerator)
{
  m_pPhysMaterialEnumerator = pPhysMaterialEnumerator;
}

IPhysMaterialEnumerator * C3DEngine::GetPhysMaterialEnumerator()
{
  return m_pPhysMaterialEnumerator;
}

void C3DEngine::ApplyForceToEnvironment(Vec3 vPos, float fRadius, float fAmountOfForce)
{
  if(m_pTerrain)
    m_pTerrain->ApplyForceToEnvironment(vPos, fRadius, fAmountOfForce);
}

float C3DEngine::GetDistanceToSectorWithWater()
{
	CTerrainNode * pSectorInfo = NULL;
	if(m_pTerrain)
		pSectorInfo = m_pTerrain->GetSecInfo(GetCamera().GetPosition());

  return (pSectorInfo && (m_pTerrain->GetDistanceToSectorWithWater() > 0.1f))
    ? m_pTerrain->GetDistanceToSectorWithWater() : max(GetCamera().GetPosition().z - GetWaterLevel(),0.1f);
}


Vec3 C3DEngine::GetSkyColor()
{
  return m_pObjManager ? m_pObjManager->m_vSkyColor : Vec3(0,0,0);
}

Vec3 C3DEngine::GetSunColor()
{
  return m_pObjManager ? m_pObjManager->m_vSunColor : Vec3(0,0,0);
}

float C3DEngine::GetSkyBrightness()
{
	return m_pObjManager ? m_pObjManager->m_fSkyBrightMul : 1.0f;
}

float C3DEngine::GetSSAOAmount()
{
  return m_pObjManager ? m_pObjManager->m_fSSAOAmount: 1.0f;
}

float C3DEngine::GetSunRel()
{
	return m_pObjManager->m_fSunSkyRel;
}

float C3DEngine::GetSunShadowIntensity() const
{
  if(!m_pTerrain)
    return 1;

	return m_pTerrain->GetSunShadowIntensity();
}

void C3DEngine::SetSunDir( const Vec3& newSunDir ) 
{ 
  Vec3 vSunDirNormalized = newSunDir.normalized();
  m_vSunDirRealtime = vSunDirNormalized;
  if( vSunDirNormalized.Dot(m_vSunDirNormalized) < GetCVars()->e_sun_angle_snap_dot || 
    GetCurTimeSec() - m_fSunDirUpdateTime > GetCVars()->e_sun_angle_snap_sec )
  {
    m_vSunDirNormalized = vSunDirNormalized;
    m_vSunDir = m_vSunDirNormalized * DISTANCE_TO_THE_SUN; 

    m_fSunDirUpdateTime = GetCurTimeSec();
  }
}

Vec3 C3DEngine::GetSunDir() 
{ 
  return m_vSunDir; 
}

Vec3 C3DEngine::GetSunDirNormalized()
{ 
	return m_vSunDirNormalized; 
}

Vec3 C3DEngine::GetRealtimeSunDirNormalized()
{ 
  return m_vSunDirRealtime; 
}

Vec3 C3DEngine::GetAmbientColorFromPosition(const Vec3 & vPos, float fRadius) 
{
  Vec3 ret(0,0,0); 

  if(m_pObjManager)
	{
		if(CVisArea * pVisArea = (CVisArea *)m_pVisAreaManager->GetVisAreaFromPos(vPos))
      ret = pVisArea->m_vAmbColor*((CTimeOfDay*)Get3DEngine()->GetTimeOfDay())->GetHDRDynamicMultiplier();
		else
			ret = m_pObjManager->m_vSkyColor;

		assert(ret.x>=0 && ret.y>=0 && ret.z>=0);
	}

	return ret;
}

void C3DEngine::FreeRenderNodeState(IRenderNode * pEnt)
{
//  PrintMessage("vlad: C3DEngine::FreeRenderNodeState: %d, %s", (int)(IRenderNode*)pEnt, pEnt->GetName());

	/*
	LightMapInfo * pLightMapInfo = (LightMapInfo *)pEnt->GetLightMapInfo();
	if(pLightMapInfo && pLightMapInfo->pLMTCBuffer)
		GetRenderer()->ReleaseBuffer(pLightMapInfo->pLMTCBuffer);
	*/
	// TODO: Delete data here, or at a more appropiate place 
	// (currently done in CBrush::~CBrush(), hope that is 'a more appropiate place')

  m_lstAlwaysVisible.Delete(pEnt);

  if(m_pDecalManager && (pEnt->m_nInternalFlags & IRenderNode::DECAL_OWNER))
    m_pDecalManager->OnEntityDeleted(pEnt);

  if(m_pPartManager  && (pEnt->m_nInternalFlags & IRenderNode::PARTICLES_OWNER))
    m_pPartManager->OnEntityDeleted(pEnt);

  if(pEnt->GetRenderNodeType() == eERType_Light)
    GetRenderer()->OnEntityDeleted(pEnt);

  // Procedural vegetation in pure game seems the only case where we can rely on ERF_CASTSHADOWMAPS flag
  // and it is the only time critical one because of high amount of such objects
  // So only those objects are checked for ERF_CASTSHADOWMAPS flag before searching in casters list
  // TODO: use IRenderNode::WAS_CASTER flag when available
  if((pEnt->GetRndFlags() & ERF_CASTSHADOWMAPS) || !(pEnt->GetRndFlags() & ERF_PROCEDURAL))
  { // make sure pointer to object will not be used somewhere in the renderer
    PodArray<CDLight> * pLigts = GetDynamicLightSources();
    for(int i=0; i<pLigts->Count(); i++)
    {
      CLightEntity * pLigt = (CLightEntity *)(pLigts->Get(i)->m_pOwner);
      pLigt->OnCasterDeleted(pEnt);
    }

    if(m_pSun)
      m_pSun->OnCasterDeleted(pEnt);
  }

  UnRegisterEntity(pEnt);

  if(pEnt->m_pRNTmpData)
  {
    Get3DEngine()->FreeRNTmpData(&pEnt->m_pRNTmpData);
    assert(!pEnt->m_pRNTmpData);
  }

//#endif
	/*
  if(pEnt->m_pShadowMapInfo)
	{
		pEnt->m_pShadowMapInfo->Release(pEnt->GetRenderNodeType(), GetRenderer());
		pEnt->m_pShadowMapInfo = NULL;
	}
*/
  // check that was really unregistered
/*  if(pEnt->m_pSector)
    for(int t=0; t<2; t++)
	{
	  for(int e=0; e<pEnt->m_pSector->m_lstEntities[t].Count(); e++)
		{
			if(pEnt->m_pSector->m_lstEntities[t][e]->GetEntityRS() == pEntRendState)
			{
				assert(0); // UnRegisterInAllSectors do this already
				pEnt->m_pSector->m_lstEntities[t].Delete(e);
			}
		}

		pEnt->m_pSector->m_lstEntities[t].Delete(pEnt);
	}										 

  pEnt->m_pSector = 0;
  */

	// delete occlusion client
/*	for(int i=0; i<2; i++)
	if(m_pOcclState && m_pOcclState->arrREOcclusionQuery[i])
	{
		m_pOcclState->arrREOcclusionQuery[i]->Release();
		m_pOcclState->arrREOcclusionQuery[i] = 0;
	}*/

//  delete m_pOcclState;
  //m_pOcclState = 0;

  //todo: is it needed?
//	pEnt->GetEntityVisArea() = 0;
}

IParticleEmitter* C3DEngine::CreateParticleEmitter( bool bIndependent, Matrix34 const& mLoc, const ParticleParams& Params )
{
  if (m_pPartManager)
    return m_pPartManager->CreateEmitter( bIndependent, mLoc, NULL, &Params );
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::DeleteParticleEmitter(IParticleEmitter* pPartEmitter)
{
	if (m_pPartManager && pPartEmitter)
		m_pPartManager->DeleteEmitter(pPartEmitter);
}

const char * C3DEngine::GetLevelFilePath(const char * szFileName)
{
  strcpy(m_sGetLevelFilePathTmpBuff, m_szLevelFolder);
  strcat(m_sGetLevelFilePathTmpBuff, szFileName);
	return m_sGetLevelFilePathTmpBuff;
}
/*
ushort * C3DEngine::GetUnderWaterSmoothHMap(int & nDimensions)
{
  return m_pTerrain ? m_pTerrain->GetUnderWaterSmoothHMap(nDimensions) : 0;
}

void C3DEngine::MakeUnderWaterSmoothHMap(int nWaterUnitSize)
{
  if(m_pTerrain)
    m_pTerrain->MakeUnderWaterSmoothHMap(nWaterUnitSize);
}
*/
void C3DEngine::SetTerrainBurnedOut(int x, int y, bool bBurnedOut)
{
	assert(!"not supported");
/*  if(m_pTerrain)
  {
    m_pTerrain->SetBurnedOut(x, y, bBurnedOut);

    // update vert buffers
    CTerrainNode * pInfo = m_pTerrain->GetSecInfo(x,y);
    if(pInfo)
      pInfo->ReleaseHeightMapGeometry();
  }*/
}

bool C3DEngine::IsTerrainBurnedOut(int x, int y)
{
	assert(!"not supported");
  return 0;//m_pTerrain ? m_pTerrain->IsBurnedOut(x, y) : 0;
}

int C3DEngine::GetTerrainSectorSize()
{
  return m_pTerrain ? m_pTerrain->GetSectorSize() : 0;
}

void C3DEngine::AddWaterSplash (Vec3 vPos, eSplashType eST, float fForce, int Id=-1)
{
  GetRenderer()->EF_AddSplash(vPos, eST, fForce, Id);
}

void C3DEngine::ActivatePortal(const Vec3 &vPos, bool bActivate, const char * szEntityName)
{
	if (m_pVisAreaManager)
		m_pVisAreaManager->ActivatePortal(vPos, bActivate, szEntityName);
}

bool C3DEngine::SetStatInstGroup(int nGroupId, const IStatInstGroup & siGroup)
{
	m_pObjManager->m_lstStaticTypes.resize(1024);//PreAllocate(1024,1024);

	if(nGroupId<0 || nGroupId>=m_pObjManager->m_lstStaticTypes.size())
		return false;

	StatInstGroup & rGroup = m_pObjManager->m_lstStaticTypes[nGroupId];

	rGroup.pStatObj			= siGroup.pStatObj;

	if(rGroup.pStatObj)
		strncpy(rGroup.szFileName, siGroup.pStatObj->GetFilePath(), sizeof(rGroup.szFileName));
  else
    rGroup.szFileName[0] = 0;

  rGroup.bHideability	= siGroup.bHideability;
  rGroup.bHideabilitySecondary	= siGroup.bHideabilitySecondary;
  rGroup.bPickable	  = siGroup.bPickable;
	rGroup.fBending			= siGroup.fBending;
	rGroup.bCastShadow   = siGroup.bCastShadow;
	rGroup.bRecvShadow   = siGroup.bRecvShadow;
	rGroup.bPrecShadow   = siGroup.bPrecShadow;

	rGroup.bUseAlphaBlending						= siGroup.bUseAlphaBlending;
	rGroup.fSpriteDistRatio						= siGroup.fSpriteDistRatio;
	rGroup.fShadowDistRatio						= siGroup.fShadowDistRatio;
	rGroup.fMaxViewDistRatio						= siGroup.fMaxViewDistRatio;

	rGroup.fBrightness									= siGroup.fBrightness;
  
	rGroup.pMaterial										= siGroup.pMaterial;	
//	if(siGroup.pMaterial) // make sure mat id is set
	//	if(int nMaterialId = GetObjManager()->GetObjectMaterialId(rGroup.pMaterial))
		//	if(nMaterialId<0)
			//	rGroup.pMaterial = NULL;

	rGroup.bUseSprites						      = siGroup.bUseSprites;

	rGroup.fDensity										= siGroup.fDensity;
	rGroup.fElevationMax								= siGroup.fElevationMax;
	rGroup.fElevationMin								= siGroup.fElevationMin;
	rGroup.fSize												= siGroup.fSize;
	rGroup.fSizeVar										= siGroup.fSizeVar;
	rGroup.fSlopeMax										= siGroup.fSlopeMax;
	rGroup.fSlopeMin										= siGroup.fSlopeMin;

	rGroup.bRandomRotation             = siGroup.bRandomRotation;
  rGroup.nMaterialLayers = siGroup.nMaterialLayers;

	rGroup.bUseTerrainColor             = siGroup.bUseTerrainColor;
	rGroup.bAlignToTerrain             = siGroup.bAlignToTerrain;
  rGroup.bAffectedByVoxels             = siGroup.bAffectedByVoxels;
	rGroup.minConfigSpec = siGroup.minConfigSpec;

	rGroup.Update(GetCVars(), Get3DEngine()->m_nGeomDetailScreenRes);

	if (GetTerrain())
		GetTerrain()->MarkAllSectorsAsUncompiled();

  MarkRNTmpDataPoolForReset();

	return true;
}

bool C3DEngine::GetStatInstGroup(int nGroupId, IStatInstGroup & siGroup)
{
	if(nGroupId<0 || nGroupId>=m_pObjManager->m_lstStaticTypes.size())
		return false;

	StatInstGroup & rGroup = m_pObjManager->m_lstStaticTypes[nGroupId];

	siGroup.pStatObj			= rGroup.pStatObj;
	if(siGroup.pStatObj)
		strncpy(siGroup.szFileName, rGroup.pStatObj->GetFilePath(), sizeof(siGroup.szFileName));

	siGroup.bHideability	= rGroup.bHideability;
	siGroup.bHideabilitySecondary	= rGroup.bHideabilitySecondary;
  siGroup.bPickable	    = rGroup.bPickable;
	siGroup.fBending			= rGroup.fBending;
	siGroup.bCastShadow   = rGroup.bCastShadow;
	siGroup.bRecvShadow   = rGroup.bRecvShadow;
	siGroup.bPrecShadow   = rGroup.bPrecShadow;

	siGroup.bUseAlphaBlending						= rGroup.bUseAlphaBlending;
	siGroup.fSpriteDistRatio						= rGroup.fSpriteDistRatio;
	siGroup.fShadowDistRatio						= rGroup.fShadowDistRatio;
	siGroup.fMaxViewDistRatio						= rGroup.fMaxViewDistRatio;

	siGroup.fBrightness									= rGroup.fBrightness;
	siGroup.pMaterial										= rGroup.pMaterial;
  siGroup.bUseSprites                 = rGroup.bUseSprites;

	siGroup.fDensity										= rGroup.fDensity;
	siGroup.fElevationMax								= rGroup.fElevationMax;
	siGroup.fElevationMin								= rGroup.fElevationMin;
	siGroup.fSize												= rGroup.fSize;
	siGroup.fSizeVar										= rGroup.fSizeVar;
	siGroup.fSlopeMax										= rGroup.fSlopeMax;
	siGroup.fSlopeMin										= rGroup.fSlopeMin;

	return true;
}

void C3DEngine::UpdateStatInstGroups()
{
  if(!m_pObjManager)
    return;

  for(int nGroupId=0; nGroupId<m_pObjManager->m_lstStaticTypes.size(); nGroupId++)
  {
    StatInstGroup & rGroup = m_pObjManager->m_lstStaticTypes[nGroupId];
    rGroup.Update(GetCVars(), Get3DEngine()->m_nGeomDetailScreenRes);
  }
}

void C3DEngine::GetMemoryUsage(class ICrySizer * pSizer)
{
	if (!pSizer->Add(*this))
		return; // we've already added this object

	{
	  SIZER_COMPONENT_NAME(pSizer, "Particles");
		if(m_pPartManager)
			m_pPartManager->GetMemoryUsage(pSizer);
	}

	pSizer->AddContainer(m_lstDynLights);
  pSizer->AddContainer(m_lstKilledVegetations);
	
	pSizer->AddObject(m_pCVars, sizeof(CVars));

	if(m_pDecalManager)
	{
	  SIZER_COMPONENT_NAME(pSizer, "DecalManager");
		m_pDecalManager->GetMemoryUsage(pSizer);
		pSizer->AddObject(m_pDecalManager, sizeof(*m_pDecalManager));
	}

	if(m_pObjManager)
	{
	  SIZER_COMPONENT_NAME(pSizer, "ObjManager");
		m_pObjManager->GetMemoryUsage(pSizer);
	}
	
	if (m_pTerrain)
	{
	  SIZER_COMPONENT_NAME(pSizer, "Terrain");
		m_pTerrain->GetMemoryUsage(pSizer);
	}

	if(m_pObjectsTree)
	{
		SIZER_COMPONENT_NAME(pSizer, "OutdoorObjectsTree");
		pSizer->AddObject(m_pObjectsTree, m_pObjectsTree->GetMemoryUsage(pSizer));
	}

	if (m_pVisAreaManager)
	{
	  SIZER_COMPONENT_NAME(pSizer, "VisAreas");
		m_pVisAreaManager->GetMemoryUsage(pSizer);
	}

	if(m_pCoverageBuffer)
	{
		SIZER_COMPONENT_NAME(pSizer, "ObjManager");
		pSizer->AddObject(m_pCoverageBuffer, sizeof(*m_pCoverageBuffer));
	}

	if(m_pCoverageBuffer)
		m_pCoverageBuffer->GetMemoryUsage(pSizer);

  {
    SIZER_COMPONENT_NAME(pSizer, "RNTmpDataPool");

    CRNTmpData * pNext = NULL;

    for(CRNTmpData * pElem = m_LTPRootFree.pNext; pElem!=&m_LTPRootFree; pElem = pNext)
    {
      pSizer->AddObject(pElem, sizeof(*pElem));
      pNext = pElem->pNext;
    }

    for(CRNTmpData * pElem = m_LTPRootUsed.pNext; pElem!=&m_LTPRootUsed; pElem = pNext)
    {
      pSizer->AddObject(pElem, sizeof(*pElem));
      pNext = pElem->pNext;
    }
    

//    pSizer->AddObject(&m_RNTmpDataPools, m_RNTmpDataPools.GetMemUsage());
  }

  if(CProcVegetPoolMan * pMan = CTerrainNode::GetProcObjPoolMan())
  {
    SIZER_COMPONENT_NAME(pSizer, "ProcVegetPool");

		if (pMan)
			pMan->GetMemoryUsage(pSizer);

    SProcObjChunkPool * pPool = CTerrainNode::GetProcObjChunkPool();
		if (pPool)
			pPool->GetMemoryUsage(pSizer);
  }
}

bool C3DEngine::IsUnderWater( const Vec3& vPos ) const
{
	bool bUnderWater = false;
	for (IPhysicalEntity* pArea = 0; pArea = GetPhysicalWorld()->GetNextArea(pArea); )
	{
		if (bUnderWater)
			// Must finish iteration to unlock areas.
			continue;

		pe_params_buoyancy buoy;
		if (pArea->GetParams(&buoy) && buoy.iMedium == 0)
		{
			if (!is_unused(buoy.waterPlane.n))
			{
				if (buoy.waterPlane.n * vPos < buoy.waterPlane.n * buoy.waterPlane.origin)
				{
					pe_status_contains_point st;
					st.pt = vPos;
					if (pArea->GetStatus(&st))
						bUnderWater = true;
				}
			}
		}
	}
	return bUnderWater;
}

void C3DEngine::SetOceanRenderFlags( uint8 nFlags )
{
  m_nOceanRenderFlags = nFlags;
}

uint8 C3DEngine::GetOceanRenderFlags() const
{
  return m_nOceanRenderFlags;
}

uint32 C3DEngine::GetOceanVisiblePixelsCount() const
{
  return COcean::GetVisiblePixelsCount();
}

float C3DEngine::GetBottomLevel(const Vec3 & referencePos, float maxRelevantDepth, int objflags)
{
	FUNCTION_PROFILER_3DENGINE;

	const float terrainWorldZ = GetTerrainElevation(referencePos.x, referencePos.y);

	const float padding = 0.2f;
	float rayLength;

	// NOTE: Terrain is above referencePos, so referencePos is probably inside a voxel or something.
	if (terrainWorldZ <= referencePos.z)
		rayLength = min(maxRelevantDepth, (referencePos.z - terrainWorldZ));
	else
		rayLength = maxRelevantDepth;

	rayLength += padding * 2.0f;

	ray_hit hit;
	int rayFlags = geom_colltype_player<<rwi_colltype_bit | rwi_stop_at_pierceable;
	if (m_pPhysicalWorld->RayWorldIntersection(referencePos + Vec3(0,0,padding), Vec3(0,0,-rayLength), 
		ent_terrain|ent_static|ent_sleeping_rigid|ent_rigid, rayFlags, &hit, 1))
	{
		return hit.pt.z;
	}

	// Terrain was above or too far below referencePos, and no solid object was close enough.
	return BOTTOM_LEVEL_UNKNOWN;
}

float C3DEngine::GetBottomLevel(const Vec3 & referencePos, float maxRelevantDepth /* = 10.0f */)
{
	return GetBottomLevel (referencePos, maxRelevantDepth, ent_terrain|ent_static|ent_sleeping_rigid|ent_rigid);
}

float C3DEngine::GetBottomLevel(const Vec3 & referencePos, int objflags)
{
	return GetBottomLevel (referencePos, 10.0f, objflags);
}

float C3DEngine::GetWaterLevel(const Vec3 * pvPos, Vec3 * pvFlowDir)
{
	FUNCTION_PROFILER_3DENGINE;

	if (pvPos)
	{
		bool bInVisarea = m_pVisAreaManager && m_pVisAreaManager->GetVisAreaFromPos(*pvPos)!=0;

		Vec3 gravity;
		pe_params_buoyancy pb[4];
		int i,nBuoys=m_pPhysicalWorld->CheckAreas(*pvPos,gravity,pb,4);
		
		float max_level = (!bInVisarea)? GetOceanWaterLevel( *pvPos ) : WATER_LEVEL_UNKNOWN;

		for(i=0;i<nBuoys;i++)
		{ 
			if (pb[i].iMedium==0 && (!bInVisarea || fabs_tpl(pb[i].waterPlane.origin.x)+fabs_tpl(pb[i].waterPlane.origin.y)>0))
			{
				float fVolumeLevel = pvPos->z-pb[i].waterPlane.n.z*(pb[i].waterPlane.n*(*pvPos-pb[i].waterPlane.origin));

				// it's not ocean
				if( pb[i].waterPlane.origin.y + pb[i].waterPlane.origin.x != 0.0f)
					max_level = max(max_level, fVolumeLevel);
			}
		}
		if (max_level > WATER_LEVEL_UNKNOWN)
			return max_level;
	}
  else if(m_pTerrain)
		return m_pTerrain->GetWaterLevel();

	return WATER_LEVEL_UNKNOWN;
}

float C3DEngine::GetOceanWaterLevel( const Vec3 &pCurrPos ) const
{
  FUNCTION_PROFILER_3DENGINE;
  static int nFrameID = -1;
  int nCurrFrameID = GetFrameID();

  static float fWaterLevel = 0;
  if( nFrameID != nCurrFrameID && m_pTerrain)
  {
    fWaterLevel = m_pTerrain->GetWaterLevel();
    nFrameID = nCurrFrameID;
  }

  float fHeight = fWaterLevel + COcean::GetWave( pCurrPos );

  return fHeight;
}

Vec4 C3DEngine::GetCausticsParams() const
{
  return Vec4(1, m_oceanCausticsDistanceAtten, m_oceanCausticsMultiplier, m_oceanCausticsDarkeningMultiplier);
}

void C3DEngine::GetOceanAnimationParams(Vec4 &pParams0, Vec4 &pParams1 ) const
{
  static int nFrameID = -1;
  int nCurrFrameID = GetFrameID();

  static Vec4 s_pParams0 = Vec4(0,0,0,0);
  static Vec4 s_pParams1 = Vec4(0,0,0,0);

  if( nFrameID != nCurrFrameID && m_pTerrain)
  {
		cry_sincosf(m_oceanWindDirection, &s_pParams1.y, &s_pParams1.z);

    s_pParams0 = Vec4(m_oceanWindDirection,  m_oceanWindSpeed, m_oceanWavesSpeed, m_oceanWavesAmount);
    s_pParams1.x = m_oceanWavesSize;
    s_pParams1.w = m_pTerrain->GetWaterLevel();

    nFrameID = nCurrFrameID;
  }
  
  pParams0 = s_pParams0;
  pParams1 = s_pParams1;
};

IVisArea * C3DEngine::CreateVisArea()
{
	return m_pObjManager ? m_pVisAreaManager->CreateVisArea() : 0;
}

void C3DEngine::DeleteVisArea(IVisArea * pVisArea)
{
  if(!m_pVisAreaManager->IsValidVisAreaPointer((CVisArea*)pVisArea))
  {
    Warning( "I3DEngine::DeleteVisArea: Invalid VisArea pointer");
    return;
  }
	if(m_pObjManager)
	{
		CVisArea * pArea = (CVisArea*)pVisArea;

		if(pArea->m_pObjectsTree)
			pArea->m_pObjectsTree->FreeAreaBrushes(true);

		PodArray<SRNInfo> lstEntitiesInArea;
		if(pArea->m_pObjectsTree)
			pArea->m_pObjectsTree->MoveObjectsIntoList(&lstEntitiesInArea,NULL);

		// unregister from indoor
		for(int i=0; i<lstEntitiesInArea.Count(); i++)
			Get3DEngine()->UnRegisterEntity(lstEntitiesInArea[i].pNode);

		if(pArea->m_pObjectsTree)
			assert(pArea->m_pObjectsTree->GetObjectsCount(eMain)==0);

		m_pVisAreaManager->DeleteVisArea((CVisArea*)pVisArea);

		for(int i=0; i<lstEntitiesInArea.Count(); i++)
			Get3DEngine()->RegisterEntity(lstEntitiesInArea[i].pNode);
	}
}

void C3DEngine::UpdateVisArea(IVisArea * pVisArea, const Vec3 * pPoints, int nCount, const char * szName, 
  const SVisAreaInfo & info, bool bReregisterObjects)
{
	if(!m_pObjManager)
    return;

  CVisArea * pArea = (CVisArea*)pVisArea;

//	PrintMessage("C3DEngine::UpdateVisArea: %s", szName);

  Vec3 vTotalBoxMin = pArea->m_boxArea.min;
  Vec3 vTotalBoxMax = pArea->m_boxArea.max;

  m_pVisAreaManager->UpdateVisArea((CVisArea*)pVisArea, pPoints, nCount, szName, info);

  if(((CVisArea*)pVisArea)->m_pObjectsTree && ((CVisArea*)pVisArea)->m_pObjectsTree->GetObjectsCount(eMain))
  {
    // merge old and new bboxes
    vTotalBoxMin.CheckMin(pArea->m_boxArea.min);
    vTotalBoxMax.CheckMax(pArea->m_boxArea.max);
  }
  else
  {
    vTotalBoxMin = pArea->m_boxArea.min;
    vTotalBoxMax = pArea->m_boxArea.max;
  }

	if(bReregisterObjects)
		m_pObjManager->ReregisterEntitiesInArea(vTotalBoxMin - Vec3(8,8,8), vTotalBoxMax + Vec3(8,8,8));
}

void C3DEngine::ResetParticlesAndDecals()
{
	if(m_pPartManager)
		m_pPartManager->Reset(true);

	if(m_pDecalManager)
		m_pDecalManager->Reset();

	CStatObjFoliage *pFoliage,*pFoliageNext;
	for(pFoliage=m_pFirstFoliage; &pFoliage->m_next!=&m_pFirstFoliage; pFoliage=pFoliageNext) {
		pFoliageNext = pFoliage->m_next;
		delete pFoliage;
	}

  ReRegisterKilledVegetationInstances();
}

IRenderNode * C3DEngine::CreateRenderNode( EERType type )
{
	switch (type)
	{
		case eERType_Brush:
			{
				CBrush * pBrush = new CBrush();
				return pBrush;
			}
		case eERType_Vegetation:
			{
				CVegetation * pVeget = new CVegetation();
				return pVeget;
			}
		case eERType_Cloud:
			{
				CCloudRenderNode* pCloud = new CCloudRenderNode();
				return pCloud;
			}
		case eERType_VoxelObject:
			{
				CVoxelObject* pVoxArea = new CVoxelObject(Vec3(0,0,0), DEF_VOX_UNIT_SIZE, 
					DEF_VOX_VOLUME_SIZE, DEF_VOX_VOLUME_SIZE, DEF_VOX_VOLUME_SIZE);
				return pVoxArea;
			}
		case eERType_FogVolume:
			{
				CFogVolumeRenderNode* pFogVolume = new CFogVolumeRenderNode();
				return pFogVolume;
			}
    case eERType_RoadObject_NEW:
      {
        CRoadRenderNode* pRoad = new CRoadRenderNode();
        return pRoad;
      }
		case eERType_Decal:
			{
				CDecalRenderNode* pDecal = new CDecalRenderNode();
				return pDecal;
			}
		case eERType_WaterVolume:
			{
				CWaterVolumeRenderNode* pWaterVolume = new CWaterVolumeRenderNode();
				return pWaterVolume;
			}
    case eERType_WaterWave:
      {
        CWaterWaveRenderNode *pRenderNode = new CWaterWaveRenderNode();
        return pRenderNode;
      }
		case eERType_DistanceCloud:
			{
				CDistanceCloudRenderNode *pRenderNode = new CDistanceCloudRenderNode();
				return pRenderNode;
			}
		case eERType_AutoCubeMap:
			{
				CAutoCubeMapRenderNode *pRenderNode = new CAutoCubeMapRenderNode();
				return pRenderNode;
			}
		case eERType_Rope:
			{
				IRopeRenderNode *pRenderNode = new CRopeRenderNode();
				return pRenderNode;
			}
		case eERType_VolumeObject:
			{
				IVolumeObjectRenderNode* pRenderNode = new CVolumeObjectRenderNode();
				return pRenderNode;
			}
	}
	assert( !"C3DEngine::CreateRenderNode: Specified node type is not supported." );
	return 0;
}

void C3DEngine::DeleteRenderNode(IRenderNode * pRenderNode)
{
	UnRegisterEntity(pRenderNode);
//	m_pObjManager->m_lstBrushContainer.Delete((CBrush*)pRenderNode);
	//m_pObjManager->m_lstVegetContainer.Delete((CVegetation*)pRenderNode);
	delete pRenderNode;
}

void C3DEngine::SetWind( const Vec3 & vWind )
{
	m_vWindSpeed = vWind;
	if (!m_vWindSpeed.IsZero())
	{
		// Maintain a large physics area for global wind.
		if (!m_pGlobalWind)
		{
			primitives::sphere geomSphere;
			geomSphere.center = Vec3(0);
			geomSphere.r = 1e7f;
			IGeometry *pGeom = m_pPhysicalWorld->GetGeomManager()->CreatePrimitive( primitives::sphere::type, &geomSphere );
			m_pGlobalWind = GetPhysicalWorld()->AddArea( pGeom, Vec3(ZERO), Quat(IDENTITY), 1.0f );

			pe_params_foreign_data fd;
			fd.iForeignFlags = PFF_OUTDOOR_AREA;
			m_pGlobalWind->SetParams(&fd);
		}

		// Set medium half-plane above water level.
		pe_params_buoyancy pb;
		pb.iMedium = 1;
		pb.waterDensity = 0;
		pb.waterResistance = 0;
		pb.waterPlane.n.Set(0,0,-1);
		pb.waterPlane.origin.Set(0,0,GetWaterLevel());
		pb.waterFlow = m_vWindSpeed;
		m_pGlobalWind->SetParams(&pb);
	}
	else if (m_pGlobalWind)
	{
		GetPhysicalWorld()->DestroyPhysicalEntity( m_pGlobalWind );
		m_pGlobalWind = 0;
	}
}

Vec3 C3DEngine::GetWind( const AABB& box, bool bIndoors ) const
{
  FUNCTION_PROFILER_3DENGINE;

	if (!m_pCVars->e_wind)
		return Vec3(ZERO);

	// Start with global wind.
	Vec3 vWind;
	if (bIndoors)
		vWind.zero();
	else
		vWind = m_vWindSpeed;
	if (m_pCVars->e_wind_areas)
	{
		// Iterate all areas, looking for wind areas. Skip first (global) area, and global wind.
		pe_params_buoyancy pb;

		IPhysicalEntity* pArea = GetPhysicalWorld()->GetNextArea(0);
		if (pArea)
		{
			assert(!(pArea->GetParams(&pb) && pb.iMedium == 1));
			while (pArea = GetPhysicalWorld()->GetNextArea(pArea))
			{
				if (pArea != m_pGlobalWind && pArea->GetParams(&pb) && pb.iMedium == 1)
				{
					if (bIndoors)
					{
						// Skip outdoor areas.
						pe_params_foreign_data fd;
						if (pArea->GetParams(&fd) && (fd.iForeignFlags & PFF_OUTDOOR_AREA))
							continue;
					}
					pe_status_area area;
					area.ctr = box.GetCenter();				
					area.size = box.GetSize() * 0.5f;          
					if (pArea->GetStatus(&area))
						vWind += area.pb.waterFlow;
				}
			}
		}
	}
	return vWind;
}

IVisArea * C3DEngine::GetVisAreaFromPos(const Vec3 &vPos )
{
	if(m_pObjManager && m_pVisAreaManager)
		return m_pVisAreaManager->GetVisAreaFromPos(vPos);

	return 0;
}

bool C3DEngine::IntersectsVisAreas(const AABB& box, void** pNodeCache)
{
	if(m_pObjManager && m_pVisAreaManager)
		return m_pVisAreaManager->IntersectsVisAreas(box, pNodeCache);
	return false;
}

bool C3DEngine::ClipToVisAreas(IVisArea* pInside, Sphere& sphere, Vec3 const& vNormal, void* pNodeCache)
{
	if (pInside)
		return pInside->ClipToVisArea(true, sphere, vNormal);
	else if (m_pVisAreaManager)
		return m_pVisAreaManager->ClipOutsideVisAreas(sphere, vNormal, pNodeCache);
	return false;
}

bool C3DEngine::IsVisAreasConnected(IVisArea * pArea1, IVisArea * pArea2, int nMaxRecursion, bool bSkipDisabledPortals)
{
	if (pArea1==pArea2)
		return (true);	// includes the case when both pArea1 & pArea2 are NULL (totally outside)
										// not considered by the other checks
	if (!pArea1 || !pArea2)
		return (false); // avoid a crash - better to put this check only
										// here in one place than in all the places where this function is called

	nMaxRecursion *= 2; // include portals since portals are the areas

	if(m_pObjManager && m_pVisAreaManager)
		return ((CVisArea*)pArea1)->FindVisArea((CVisArea*)pArea2, nMaxRecursion, bSkipDisabledPortals);

	return false;
}

bool C3DEngine::IsOutdoorVisible()
{
	if(m_pObjManager && m_pVisAreaManager)
		return m_pVisAreaManager->IsOutdoorAreasVisible();

	return false;
}

void C3DEngine::EnableOceanRendering(bool bOcean)
{
	m_bOcean = bOcean;
}

//////////////////////////////////////////////////////////////////////////
IMaterialManager* C3DEngine::GetMaterialManager()
{
	return m_pMatMan;
}
//////////////////////////////////////////////////////////////////////////


ILMSerializationManager * C3DEngine::CreateLMSerializationManager()
{
	return new CLMSerializationManager();
}

IVertexScatterSerializationManager * C3DEngine::CreateVertexScatterSerializationManager() const
{
	return new CVertexScatterSerializationManager();
}

bool C3DEngine::IsTerrainHightMapModifiedByGame()
{
  return m_pTerrain ? m_pTerrain->m_bHeightMapModified : 0;
}

void C3DEngine::CheckPhysicalized(const Vec3 & vBoxMin, const Vec3 & vBoxMax)
{
/*  if(!GetCVars()->e_stream_areas)
    return;

  CVisArea * pVisArea = (CVisArea *)GetVisAreaFromPos((vBoxMin+vBoxMax)*0.5f);
  if(pVisArea)
  { // indoor
    pVisArea->CheckPhysicalized();
    IVisArea * arrConnections[16];
    int nConections = pVisArea->GetRealConnections(arrConnections,16);
    for(int i=0; i<nConections; i++)
      ((CVisArea*)arrConnections[i])->CheckPhysicalized();
  }
  else
  { // outdoor
    CTerrainNode * arrSecInfo[] = 
    {
      m_pTerrain->GetSecInfo(int(vBoxMin.x), int(vBoxMin.y)),
      m_pTerrain->GetSecInfo(int(vBoxMax.x), int(vBoxMin.y)),
      m_pTerrain->GetSecInfo(int(vBoxMin.x), int(vBoxMax.y)),
      m_pTerrain->GetSecInfo(int(vBoxMax.x), int(vBoxMax.y))
    };

    for(int i=0; i<4; i++)
      if(arrSecInfo[i])
        arrSecInfo[i]->CheckPhysicalized();
  }*/
}

void C3DEngine::CheckMemoryHeap()
{
	assert (IsHeapValid());
}

bool C3DEngine::SetMaterialFloat( char * szMatName, int nSubMatId, int nTexSlot, char * szParamName, float fValue )
{
	IMaterial * pMat = GetMaterialManager()->FindMaterial( szMatName );
	if(!pMat)
	{ Warning("I3DEngine::SetMaterialFloat: Material not found: %s", szMatName); return false; }

	if(nSubMatId>0)
	{
		if(nSubMatId<=pMat->GetSubMtlCount())
			pMat = pMat->GetSubMtl(nSubMatId-1);
		else
		{ 
			Warning("I3DEngine::SetMaterialFloat: SubMaterial not found: %s, SubMatId: %d", szMatName, nSubMatId); 
			return false; 
		}
	}

	SEfResTexture * pTex = pMat->GetShaderItem().m_pShaderResources->GetTexture(nTexSlot);
	if(!pTex)
	{ Warning("I3DEngine::SetMaterialFloat: Texture slot not found: %s, TexSlot: %d", szMatName, nTexSlot); return false; }

	char szM_Name[128] = "";

	if(strncmp(szParamName,"m_", 2))
		strncpy(szM_Name,"m_",sizeof(szM_Name));

	strncat(szM_Name,szParamName,sizeof(szM_Name));

	return pTex->m_TexModificator.SetMember(szM_Name,fValue);
}

void C3DEngine::CloseTerrainTextureFile()
{
	if(m_pTerrain)
		m_pTerrain->CloseTerrainTextureFile();
}

//////////////////////////////////////////////////////////////////////////
IParticleEffect* C3DEngine::CreateParticleEffect()
{
	if (m_pPartManager)
		return m_pPartManager->CreateEffect();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::DeleteParticleEffect( IParticleEffect* pEffect )
{
	if (m_pPartManager)
		m_pPartManager->RemoveEffect( pEffect );
}

//////////////////////////////////////////////////////////////////////////
IParticleEffect* C3DEngine::FindParticleEffect( const char* sEffectName, const char* sCallerType, const char* sCallerName, bool bLoad )
{
	if (m_pPartManager)
		return m_pPartManager->FindEffect( sEffectName, sCallerType, sCallerName, bLoad );
	return 0;
}

//////////////////////////////////////////////////////////////////////////
int C3DEngine::GetLoadedObjectCount()
{
	int nObjectsLoaded = m_pObjManager ? m_pObjManager->GetLoadedObjectCount() : 0;
	
	if(GetSystem()->GetIAnimationSystem())
	{
		ICharacterManager::Statistics AnimStats;
		GetSystem()->GetIAnimationSystem()->GetStatistics(AnimStats);
		nObjectsLoaded += AnimStats.numAnimObjectModels + AnimStats.numCharModels;
	}

  nObjectsLoaded += GetInstCount(eERType_VoxelMesh);

	return nObjectsLoaded;
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::GetLoadedStatObjArray( IStatObj** pObjectsArray,int &nCount )
{
	if (m_pObjManager)
		m_pObjManager->GetLoadedStatObjArray( pObjectsArray,nCount );
	else
		nCount = 0;
}


//////////////////////////////////////////////////////////////////////////
void C3DEngine::DeleteEntityDecals(IRenderNode * pEntity)
{
	if(m_pDecalManager && pEntity && (pEntity->m_nInternalFlags & IRenderNode::DECAL_OWNER))
		m_pDecalManager->OnEntityDeleted(pEntity);
}

void C3DEngine::CompleteObjectsGeometry()
{
  while(m_pTerrain && m_pTerrain->Voxel_Recompile_Modified_Incrementaly_Objects());
  while(m_pTerrain && m_pTerrain->Recompile_Modified_Incrementaly_RoadRenderNodes());
}

void C3DEngine::LoadLightmaps()
{
	if(!m_bEditorMode && GetCVars()->e_ram_maps)
		m_pVisAreaManager->LoadLightmap();
}

void C3DEngine::DeleteDecalsInRange( AABB * pAreaBox, IRenderNode * pEntity )
{
	if(m_pDecalManager)
		m_pDecalManager->DeleteDecalsInRange( pAreaBox, pEntity );
}

void C3DEngine::LockCGFResources()
{
	if(m_pObjManager)
		m_pObjManager->m_bLockCGFResources = true;
}

void C3DEngine::UnlockCGFResources()
{
	if(m_pObjManager)
	{
		m_pObjManager->m_bLockCGFResources = false;
		m_pObjManager->FreeNotUsedCGFs();
	}
}

void CLightEntity::ShadowMapInfo::Release(struct IRenderer * pRenderer)
{
/*	if(pShadowMapCasters)
	{
//		pShadowMapCasters->Clear();

//		delete pShadowMapCasters;
		pShadowMapCasters = 0;
	}
*/
/*	if(	pShadowMapFrustumContainer )//&& eEntType != eERType_Vegetation ) // vegetations share same frustum allocated in CStatObj
	{
		delete pShadowMapFrustumContainer->m_LightFrustums.Get(0)->pEntityList;
		pShadowMapFrustumContainer->m_LightFrustums.Get(0)->pEntityList=0;

		delete pShadowMapFrustumContainer;
		pShadowMapFrustumContainer=0;
	}

	if(	pShadowMapFrustumContainerPassiveCasters )//&& eEntType != eERType_Vegetation ) // vegetations share same frustum allocated in CStatObj
	{
		delete pShadowMapFrustumContainerPassiveCasters->m_LightFrustums.Get(0)->pEntityList;
		pShadowMapFrustumContainerPassiveCasters->m_LightFrustums.Get(0)->pCastersList=0;

		delete pShadowMapFrustumContainerPassiveCasters;
		pShadowMapFrustumContainerPassiveCasters=0;
	}*/
/*
	if(pShadowMapRenderMeshsList)
	{
		for(int i=0; i<pShadowMapRenderMeshsList->Count(); i++)
		{
			if(pShadowMapRenderMeshsList->GetAt(i))
			{
				pRenderer->DeleteRenderMesh(pShadowMapRenderMeshsList->GetAt(i));
				pShadowMapRenderMeshsList->GetAt(i)=0;
			}
		}
		delete pShadowMapRenderMeshsList;
		pShadowMapRenderMeshsList=0;
	}*/

	delete this;
}

Vec3 C3DEngine::GetTerrainSurfaceNormal(Vec3 vPos) 
{ 
	return m_pTerrain ? m_pTerrain->GetTerrainSurfaceNormal(vPos, 0.5f*GetHeightMapUnitSize()) : Vec3(0.f,0.f,1.f); 
}

IMemoryBlock * C3DEngine::Voxel_GetObjects(Vec3 vPos, float fRadius, int nSurfaceTypeId, EVoxelEditOperation eOperation, EVoxelBrushShape eShape, EVoxelEditTarget eTarget)
{
	PodArray<CVoxelObject*> lstVoxAreas;

	Vec3 vBaseColor(0,0,0);

	if(m_pTerrain)
		m_pTerrain->DoVoxelShape(vPos, fRadius, nSurfaceTypeId, vBaseColor, eOperation, eShape, eTarget, &lstVoxAreas);

	if(m_pVisAreaManager)
		m_pVisAreaManager->DoVoxelShape(vPos, fRadius, nSurfaceTypeId, vBaseColor, eOperation, eShape, eTarget, &lstVoxAreas);

	CMemoryBlock * pMemBlock = new CMemoryBlock();
	pMemBlock->SetData(lstVoxAreas.GetElements(),lstVoxAreas.GetDataSize());

	return pMemBlock;
}

void C3DEngine::Voxel_Paint(Vec3 vPos, float fRadius, int nSurfaceTypeId, Vec3 vBaseColor, EVoxelEditOperation eOperation, EVoxelBrushShape eShape, EVoxelEditTarget eTarget)
{
  if(eOperation==eveoCreate || eOperation==eveoSubstract || eOperation==eveoMaterial)
    if(nSurfaceTypeId<0)
    {
      Warning("C3DEngine::Voxel_Paint: Surface type is not specified");
      return;
    }

	if(m_pTerrain)
		m_pTerrain->DoVoxelShape(vPos, fRadius, nSurfaceTypeId, vBaseColor, eOperation, eShape, eTarget, NULL);

	if(m_pVisAreaManager)
		m_pVisAreaManager->DoVoxelShape(vPos, fRadius, nSurfaceTypeId, vBaseColor, eOperation, eShape, eTarget, NULL);
}

//////////////////////////////////////////////////////////////////////////
IIndexedMesh* C3DEngine::CreateIndexedMesh()
{
	return new CIndexedMesh();
}

void C3DEngine::Voxel_SetFlags(bool bPhysics, bool bSimplify, bool bShadows, bool bMaterials)
{
	if(m_pTerrain)
		m_pTerrain->Voxel_SetFlags(bPhysics, bSimplify, bShadows, bMaterials);
}


void C3DEngine::SerializeState( TSerialize ser )
{
	if (ser.IsReading())
	{
		CStatObjFoliage *pFoliage,*pFoliageNext;
		for(pFoliage=m_pFirstFoliage; &pFoliage->m_next!=&m_pFirstFoliage; pFoliage=pFoliageNext) {
			pFoliageNext = pFoliage->m_next;
			delete pFoliage;
		}
	}

	ser.Value("m_moonRotationLatitude", m_moonRotationLatitude);
	ser.Value("m_moonRotationLongitude", m_moonRotationLongitude);
	if (ser.IsReading())
		UpdateMoonDirection();

	if(m_pTerrain)
		m_pTerrain->SerializeTerrainState(ser);

	if(m_pDecalManager)
		m_pDecalManager->Serialize(ser);

	m_pPartManager->Serialize(ser);
	m_pTimeOfDay->Serialize(ser);
}

void C3DEngine::PostSerialize( bool bReading )
{
	m_pPartManager->PostSerialize(bReading);
}

void C3DEngine::InitMaterialDefautMappingAxis(IMaterial * pMat)
{
	pMat->m_defautMappingAxis = IMaterial::ePA_None;
	pMat->m_fDefautMappingScale = 1.f;
	for(int c=0; c<6 && c<pMat->GetSubMtlCount(); c++)
	{
		IMaterial * pSubMat = (IMaterial*)pMat->GetSubMtl(c);
		pSubMat->m_fDefautMappingScale = pMat->m_fDefautMappingScale;

		const char *subMatName = pSubMat->GetName();

		if (stricmp(subMatName,"X") == 0 || stricmp(subMatName,"XP") == 0)
		{
			pSubMat->m_defautMappingAxis = IMaterial::ePA_XPos;
		}
		else if (stricmp(subMatName,"Y") == 0 || stricmp(subMatName,"YP") == 0)
		{
			pSubMat->m_defautMappingAxis = IMaterial::ePA_YPos;
		}
		else if (stricmp(subMatName,"Z") == 0 || stricmp(subMatName,"ZP") == 0)
		{
			pSubMat->m_defautMappingAxis = IMaterial::ePA_ZPos;
		}
		else if (stricmp(subMatName,"XN") == 0)
		{
			pSubMat->m_defautMappingAxis = IMaterial::ePA_XNeg;
		}
		else if (stricmp(subMatName,"YN") == 0)
		{
			pSubMat->m_defautMappingAxis = IMaterial::ePA_YNeg;
		}
		else if (stricmp(subMatName,"ZN") == 0)
		{
			pSubMat->m_defautMappingAxis = IMaterial::ePA_ZNeg;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CContentCGF* C3DEngine::LoadChunkFileContent( const char *filename,bool bNoWarningMode )
{
	class Listener : public ILoaderCGFListener
	{
	public:
		virtual void Warning( const char *format ) {Cry3DEngineBase::Warning("%s", format);}
		virtual void Error( const char *format ) {Cry3DEngineBase::Error("%s", format);}
	};
	Listener listener;
  CLoaderCGF cgf;
#if !defined(XENON) && !defined(PS3)
	CReadOnlyChunkFile chunkFile(bNoWarningMode);
#else
  CChunkFile chunkFile;
#endif
	return cgf.LoadCGF( filename,chunkFile,&listener );
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::ReleaseChunkFileContent( CContentCGF *pCGF )
{
	delete pCGF;
}

//////////////////////////////////////////////////////////////////////////
IChunkFile * C3DEngine::CreateChunkFile( bool bReadOnly )
{
#if !defined(XENON) && !defined(PS3)
	if (bReadOnly)
		return new CReadOnlyChunkFile;
	else
#endif
		return new CChunkFile;
}

int C3DEngine::GetTerrainTextureNodeSizeMeters()
{
	if(m_pTerrain)
		return m_pTerrain->GetTerrainTextureNodeSizeMeters();
	return 0;
}

int C3DEngine::GetTerrainTextureNodeSizePixels(int nLayer)
{
	if(m_pTerrain)
		return m_pTerrain->GetTerrainTextureNodeSizePixels(nLayer);
	return 0;
}

void C3DEngine::SetHeightMapMaxHeight(float fMaxHeight)
{
	if(m_pTerrain)
		m_pTerrain->SetHeightMapMaxHeight(fMaxHeight);
}

ITerrain * C3DEngine::CreateTerrain(const STerrainInfo & TerrainInfo)
{
	delete m_pTerrain;
	m_pTerrain = new CTerrain(TerrainInfo);
	return (ITerrain *)m_pTerrain;
}

void C3DEngine::DeleteTerrain()
{
	delete m_pTerrain;
	m_pTerrain = NULL;
}
/*
void C3DEngine::RenderImposterContent(class CREImposter * pImposter, const CCamera & cam)
{
	if(m_pTerrain && pImposter)
	{
		// set camera and disable frustum culling
		CCamera oldCam = Get3DEngine()->GetCamera();
		Get3DEngine()->SetCamera(cam);
		GetCVars()->e_camera_frustum_test = false;

		// start rendering
		GetRenderer()->EF_StartEf();  

		// prepare lights
		UpdateLightSources();
		PrepareLightSourcesForRendering();    

		// render required terrain sector
		m_pTerrain->RenderImposterContent(pImposter, cam);

		// setup fog
		SetupDistanceFog();

		// finish rendering
		GetRenderer()->EF_EndEf3D(SHDF_SORT | SHDF_ZPASS);

		// restore
		Get3DEngine()->SetCamera(oldCam);
		GetCVars()->e_camera_frustum_test = true;
	}
}*/

void C3DEngine::GetVoxelRenderNodes(struct IRenderNode**pRenderNodes, int & nCount)
{
}

void C3DEngine::PrecacheLevel(bool bPrecacheAllVisAreas, Vec3 * pPrecachePoints, int nPrecachePointsNum, int *pPrecachedPointsCount)
{
	if(GetVisAreaManager())
		GetVisAreaManager()->PrecacheLevel(bPrecacheAllVisAreas, pPrecachePoints, nPrecachePointsNum, pPrecachedPointsCount);
}

//////////////////////////////////////////////////////////////////////////
ITimeOfDay* C3DEngine::GetTimeOfDay()
{
	return m_pTimeOfDay;
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::SetGlobalParameter( E3DEngineParameter param,const Vec3 &v )
{
	float fValue = v.x;
	switch (param)
	{
	case E3DPARAM_SKY_COLOR:
		SetSkyColor( v );
		break;
	case E3DPARAM_SUN_COLOR:
		SetSunColor( v );
		break;

	case E3DPARAM_SKY_HIGHLIGHT_POS:
		m_vSkyHightlightPos = v;
		break;
	case E3DPARAM_SKY_HIGHLIGHT_COLOR:
		m_vSkyHightlightCol = v;
		break;
	case E3DPARAM_SKY_HIGHLIGHT_SIZE:
		m_fSkyHighlightSize = fValue > 0.0f ? fValue : 0.0f;
		break;

	case E3DPARAM_NIGHSKY_HORIZON_COLOR:
		m_nightSkyHorizonCol = v;
		break;
	case E3DPARAM_NIGHSKY_ZENITH_COLOR:
		m_nightSkyZenithCol = v;
		break;
	case E3DPARAM_NIGHSKY_ZENITH_SHIFT:
		m_nightSkyZenithColShift = v.x;
		break;
	case E3DPARAM_NIGHSKY_STAR_INTENSITY:
		m_nightSkyStarIntensity = v.x;
		break;
	case E3DPARAM_NIGHSKY_MOON_COLOR:
		m_nightMoonCol = v;
		break;
	case E3DPARAM_NIGHSKY_MOON_SIZE:
		m_nightMoonSize = v.x;
		break;
	case E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR:
		m_nightMoonInnerCoronaCol = v;
		break;
	case E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE:
		m_nightMoonInnerCoronaScale = v.x;
		break;
	case E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR:
		m_nightMoonOuterCoronaCol = v;
		break;
	case E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE:
		m_nightMoonOuterCoronaScale = v.x;
		break;
	case E3DPARAM_OCEANFOG_COLOR_MULTIPLIER:
		m_oceanFogColorMultiplier = v.x;
		break;
	case E3DPARAM_SKY_MOONROTATION:
		m_moonRotationLatitude = v.x;
		m_moonRotationLongitude = v.y;
		UpdateMoonDirection();
		break;
	case E3DPARAM_SKYBOX_MULTIPLIER:
		m_skyboxMultiplier = v.x;
		break;
	case E3DPARAM_DAY_NIGHT_INDICATOR:
		m_dayNightIndicator = v.x;
		break;
	case E3DPARAM_EYEADAPTIONCLAMP:
		m_fEyeAdaptionClamp = v.x;
		break;

  case E3DPARAM_COLORGRADING_COLOR_SATURATION:
    m_fSaturation = fValue;
    SetPostEffectParam("ColorGrading_Saturation", m_fSaturation);
    break;
  case E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR:
    m_pPhotoFilterColor = Vec4(v.x, v.y, v.z, 1);
    SetPostEffectParamVec4("clr_ColorGrading_PhotoFilterColor", m_pPhotoFilterColor);
    break;
  case E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY:
    m_fPhotoFilterColorDensity = fValue;
    SetPostEffectParam("ColorGrading_PhotoFilterColorDensity", m_fPhotoFilterColorDensity);
    break;
  case E3DPARAM_COLORGRADING_FILTERS_GRAIN:
    m_fGrainAmount = fValue;
    SetPostEffectParam("ColorGrading_GrainAmount", m_fGrainAmount);
    break;

	case E3DPARAM_NIGHSKY_MOON_DIRECTION: // moon direction is fixed per level or updated via FG node (E3DPARAM_SKY_MOONROTATION)
	case E3DPARAM_SKY_SUNROTATION: // sun rotation parameters are fixed per level, position updated by ToD (query via GetSunPosition)

	default:
		assert(0);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void C3DEngine::GetGlobalParameter( E3DEngineParameter param,Vec3 &v )
{
	switch (param)
	{
	case E3DPARAM_SKY_COLOR:
		v = GetSkyColor();
		break;
	case E3DPARAM_SUN_COLOR:
		v = GetSunColor();
		break;

	case E3DPARAM_SKY_HIGHLIGHT_POS:
		v = m_vSkyHightlightPos;
		break;
	case E3DPARAM_SKY_HIGHLIGHT_COLOR:
		v = m_vSkyHightlightCol;
		break;
	case E3DPARAM_SKY_HIGHLIGHT_SIZE:
		v.x = m_fSkyHighlightSize;
		break;

	case E3DPARAM_NIGHSKY_HORIZON_COLOR:
		v = m_nightSkyHorizonCol;
		break;
	case E3DPARAM_NIGHSKY_ZENITH_COLOR:
		v = m_nightSkyZenithCol;
		break;
	case E3DPARAM_NIGHSKY_ZENITH_SHIFT:
		v = Vec3( m_nightSkyZenithColShift, 0, 0 );
		break;
	case E3DPARAM_NIGHSKY_STAR_INTENSITY:
		v = Vec3( m_nightSkyStarIntensity, 0, 0 );
		break;
	case E3DPARAM_NIGHSKY_MOON_DIRECTION:
		v = m_moonDirection;
		break;
	case E3DPARAM_NIGHSKY_MOON_COLOR:
		v = m_nightMoonCol;
		break;
	case E3DPARAM_NIGHSKY_MOON_SIZE:
		v = Vec3( m_nightMoonSize, 0, 0 );
		break;
	case E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR:
		v = m_nightMoonInnerCoronaCol;
		break;
	case E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE:
		v = Vec3( m_nightMoonInnerCoronaScale, 0, 0 );
		break;
	case E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR:
		v = m_nightMoonOuterCoronaCol;
		break;
	case E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE:
		v = Vec3( m_nightMoonOuterCoronaScale, 0, 0 );
		break;
	case E3DPARAM_SKY_SUNROTATION:
		v = Vec3(m_sunRotationZ, m_sunRotationLongitude, 0);
		break;
	case E3DPARAM_SKY_MOONROTATION:
		v = Vec3(m_moonRotationLatitude, m_moonRotationLongitude, 0);
		break;
	case E3DPARAM_OCEANFOG_COLOR_MULTIPLIER:
		v = Vec3(m_oceanFogColorMultiplier, 0, 0);
		break;
	case E3DPARAM_SKYBOX_MULTIPLIER:
		v = Vec3(m_skyboxMultiplier, 0, 0);
		break;
	case E3DPARAM_DAY_NIGHT_INDICATOR:
		v = Vec3(m_dayNightIndicator, 0, 0);
		break;
	case E3DPARAM_EYEADAPTIONCLAMP:
		v = Vec3(m_fEyeAdaptionClamp, 0, 0);
		break;
  case E3DPARAM_COLORGRADING_COLOR_SATURATION:
    v = Vec3(m_fSaturation, 0, 0);
      break;
  case E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_COLOR:
    v = Vec3(m_pPhotoFilterColor.x, m_pPhotoFilterColor.y, m_pPhotoFilterColor.z);
    break;
  case E3DPARAM_COLORGRADING_FILTERS_PHOTOFILTER_DENSITY:
    v = Vec3(m_fPhotoFilterColorDensity, 0, 0);
      break;
  case E3DPARAM_COLORGRADING_FILTERS_GRAIN:
    v = Vec3(m_fGrainAmount, 0, 0);
      break;
	default:
		assert(0);
	}
}

//////////////////////////////////////////////////////////////////////////
bool C3DEngine::CheckIntersectClouds(const Vec3 & p1, const Vec3 & p2)
{
	return m_pCloudsManager->CheckIntersectClouds(p1, p2);
}

void C3DEngine::OnRenderMeshDeleted(IRenderMesh * pRenderMesh)
{
	if(m_pDecalManager)
		m_pDecalManager->OnRenderMeshDeleted(pRenderMesh);
}

bool C3DEngine::RayObjectsIntersection( Vec3 vStart, Vec3 vEnd, Vec3 & vHitPoint, EERType eERType )
{
	float fClosestHitDistance = 1000000;

	if(m_pObjectsTree)
		m_pObjectsTree->RayObjectsIntersection2D( vStart, vEnd, vHitPoint, fClosestHitDistance, eERType );

	return (fClosestHitDistance < 1000000);
}

void C3DEngine::CheckCreateRNTmpData(CRNTmpData ** ppInfo, IRenderNode * pRNode)
{
  if(!(*ppInfo))
  {
    FUNCTION_PROFILER_3DENGINE;

	  // make sure element is allocated
	  if(m_LTPRootFree.pNext == &m_LTPRootFree)
	  {
		  CRNTmpData * pNew = new CRNTmpData;//m_RNTmpDataPools.GetNewElement();
		  pNew->Link(&m_LTPRootFree);
	  }

	  // move element from m_LTPRootFree to m_LTPRootUsed
	  CRNTmpData * pElem = m_LTPRootFree.pNext;
	  pElem->Unlink();
	  pElem->Link(&m_LTPRootUsed);

	  *ppInfo = pElem;
	  (*ppInfo)->pOwnerRef = ppInfo;

/*
    memset(pElem->szDebugName,0,sizeof(pElem->szDebugName));
    if(pRNode)
      strncpy(pElem->szDebugName, pRNode->GetName(), sizeof(pElem->szDebugName)-1);
    else
      strncpy(pElem->szDebugName, "NULL", sizeof(pElem->szDebugName)-1);

    if(strstr(pElem->szDebugName, "generic"))
      int y=0;
*/

    memset(&pElem->userData, 0, sizeof(pElem->userData));

    pRNode->OnRenderNodeBecomeVisible();

    (*ppInfo)->nCreatedFrameId = GetMainFrameID();
  }

  (*ppInfo)->nLastUsedFrameId = GetMainFrameID();
}

void C3DEngine::FreeRNTmpData(CRNTmpData ** ppInfo)
{
	assert((*ppInfo)->pNext != &m_LTPRootFree);
	assert((*ppInfo)->pPrev != &m_LTPRootFree);
	(*ppInfo)->Unlink();
	(*ppInfo)->Link(&m_LTPRootFree);	
	*((*ppInfo)->pOwnerRef) = NULL;
}

void C3DEngine::UpdateRNTmpDataPool(bool bFreeAll)
{
	FUNCTION_PROFILER_3DENGINE;

	// move old elements into m_LTPRootFree
	CRNTmpData * pNext = NULL;
	for(CRNTmpData * pElem = m_LTPRootUsed.pNext; pElem!=&m_LTPRootUsed; pElem = pNext)
	{
		pNext = pElem->pNext;
		if(bFreeAll || (pElem->nLastUsedFrameId < (GetMainFrameID()-4)))
    {
			FreeRNTmpData(&pElem);
    }
	}
  
/*
  for(int p=0; p<m_RNTmpDataPools.m_Pools.Count(); p++)
  {
    CRNTmpData * pArray = m_RNTmpDataPools.m_Pools[p];

    for(int e=0; e<m_RNTmpDataPools.GetMaxElemsInChunk(); e++)
    {
      CRNTmpData * pElem = &pArray[e];

      if(pElem->pOwnerRef && *(pElem->pOwnerRef))
      {
        if(bFreeAll || (pElem->nLastUsedFrameId < (GetMainFrameID()-4)))
        {
          FreeRNTmpData(&pElem);
        }
      }
    }
  }*/
}

void C3DEngine::FreeRNTmpDataPool()
{
	// move all into m_LTPRootFree
	UpdateRNTmpDataPool(true);

	// delete all elements of m_LTPRootFree
	CRNTmpData * pNext = NULL;
	for(CRNTmpData * pElem = m_LTPRootFree.pNext; pElem!=&m_LTPRootFree; pElem = pNext)
	{
		pNext = pElem->pNext;
		pElem->Unlink();
		delete pElem;
	}
}

bool C3DEngine::IsAmbientOcclusionEnabled() 
{ 
  return GetTerrain() ? GetTerrain()->IsAmbientOcclusionEnabled() : false; 
}


uint32 C3DEngine::GetObjectsByType( EERType objType, IRenderNode **pObjects )
{
	if(!m_pObjectsTree)
		return 0;

	PodArray<IRenderNode*> lstObjects;

	if (m_pObjectsTree)
		m_pObjectsTree->GetObjectsByType(lstObjects,objType,NULL);

	if (GetVisAreaManager())
		GetVisAreaManager()->GetObjectsByType(lstObjects,objType);

	if(pObjects && !lstObjects.IsEmpty())
		memcpy(pObjects,&lstObjects[0],lstObjects.GetDataSize());

	return lstObjects.Count();
}

#define LOADING_TIME_CONTAINER_MAX_TEXT_SIZE 1024

struct CLoadingTimeContainer
{
  CLoadingTimeContainer(const char * pCallStack, const char * pPureFuncName) 
  { 
    m_dSelfMemUsage = m_dTotalMemUsage = m_dSelfTime = m_dTotalTime = 0; m_nCounter = 0; m_pFuncName = pPureFuncName;
    strncpy(m_szCallStack, pCallStack, sizeof(m_szCallStack)); 
    m_szCallStack[sizeof(m_szCallStack)-1]=0;
  }
  double m_dSelfTime, m_dTotalTime;
  double m_dSelfMemUsage, m_dTotalMemUsage;
  uint m_nCounter;
  char m_szCallStack[LOADING_TIME_CONTAINER_MAX_TEXT_SIZE];
  const char * m_pFuncName;
};

typedef std::map<string,CLoadingTimeContainer*> LoadingTimeContainersMap;
LoadingTimeContainersMap mapLoadingTimeContainers;

#define MAX_LOADING_TIME_PROFILER_STACK_DEPTH 16
CLoadingTimeContainer * arrLoadingTimeProfilerStack[MAX_LOADING_TIME_PROFILER_STACK_DEPTH] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int nLoadingTimeProfilerStackPos = 0;

int Cmp_CLoadingTimeContainer_Time(const void* v1, const void* v2)
{
  CLoadingTimeContainer *pChunk1 = (CLoadingTimeContainer*)v1;
  CLoadingTimeContainer *pChunk2 = (CLoadingTimeContainer*)v2;

  if(pChunk1->m_dSelfTime>pChunk2->m_dSelfTime)
    return -1;
  else if(pChunk1->m_dSelfTime<pChunk2->m_dSelfTime)
    return 1;

  return 0;
}

int Cmp_CLoadingTimeContainer_MemUsage(const void* v1, const void* v2)
{
  CLoadingTimeContainer *pChunk1 = (CLoadingTimeContainer*)v1;
  CLoadingTimeContainer *pChunk2 = (CLoadingTimeContainer*)v2;

  if(pChunk1->m_dSelfMemUsage>pChunk2->m_dSelfMemUsage)
    return -1;
  else if(pChunk1->m_dSelfMemUsage<pChunk2->m_dSelfMemUsage)
    return 1;

  return 0;
}

void C3DEngine::OutputLoadingTimeStats()
{
  assert(nLoadingTimeProfilerStackPos==0);

  if(GetCVars()->e_profile_level_loading>1)
  { // loading time stats
    PrintMessage("---------------------------------------------------------------------");
    PrintMessage("----------- Level loading time (sec) by call stack ------------------");
    PrintMessage("  Self |  Total |  Calls | CallStack");
    PrintMessage("---------------------------------------------------------------------");

    LoadingTimeContainersMap::iterator it = mapLoadingTimeContainers.begin();
    for(; it != mapLoadingTimeContainers.end(); ++it)
    {
      const char * szName = it->first;
      const CLoadingTimeContainer * pTimeContainer = it->second;

      // call stack level
      char szNameNoStack[LOADING_TIME_CONTAINER_MAX_TEXT_SIZE]="";
      for(char * pTmp = (char *)szName; pTmp && pTmp<(szName+strlen(szName));)
      {
        pTmp = strstr(pTmp+2,">>");
        if(pTmp)
          strcat(szNameNoStack,". ");
      }

      strcat(szNameNoStack,pTimeContainer->m_pFuncName);

      PrintMessage("%6.1f | %6.1f | %6d | %s", 
        pTimeContainer->m_dSelfTime, pTimeContainer->m_dTotalTime, (int)pTimeContainer->m_nCounter, szNameNoStack);
    }

    PrintMessage("---------------------------------------------------------------------");
  }

  if(GetCVars()->e_profile_level_loading>1)
  { // loading mem stats
    PrintMessage("------- Level loading memory allocations (MB) by call stack ---------");
    PrintMessage("  Self |  Total |  Calls | CallStack");
    PrintMessage("---------------------------------------------------------------------");

    LoadingTimeContainersMap::iterator it = mapLoadingTimeContainers.begin();
    for(; it != mapLoadingTimeContainers.end(); ++it)
    {
      const char * szName = it->first;
      const CLoadingTimeContainer * pTimeContainer = it->second;

      // call stack level
      char szNameNoStack[LOADING_TIME_CONTAINER_MAX_TEXT_SIZE]="";
      for(char * pTmp = (char *)szName; pTmp && pTmp<(szName+strlen(szName));)
      {
        pTmp = strstr(pTmp+2,">>");
        if(pTmp)
          strcat(szNameNoStack,". ");
      }

      strcat(szNameNoStack,pTimeContainer->m_pFuncName);

      PrintMessage("%6.1f | %6.1f | %6d | %s", 
        pTimeContainer->m_dSelfMemUsage, pTimeContainer->m_dTotalMemUsage, (int)pTimeContainer->m_nCounter, szNameNoStack);
    }

    PrintMessage("---------------------------------------------------------------------");
  }

  PodArray<CLoadingTimeContainer> arrNoStack;

  { // merge items by func name
    LoadingTimeContainersMap::iterator it = mapLoadingTimeContainers.begin();
    for(; it != mapLoadingTimeContainers.end(); ++it)
    {
      const char * szName = it->first;
      const CLoadingTimeContainer * pTimeContainer = it->second;

      int nFoundId;
      for(nFoundId=0; nFoundId<arrNoStack.Count(); nFoundId++)
      {
        if(!strcmp(arrNoStack[nFoundId].m_pFuncName, pTimeContainer->m_pFuncName))
        {
          arrNoStack[nFoundId].m_dSelfTime += pTimeContainer->m_dSelfTime;
          arrNoStack[nFoundId].m_dTotalTime += pTimeContainer->m_dTotalTime;
          arrNoStack[nFoundId].m_dSelfMemUsage += pTimeContainer->m_dSelfMemUsage;
          arrNoStack[nFoundId].m_dTotalMemUsage += pTimeContainer->m_dTotalMemUsage;
          arrNoStack[nFoundId].m_nCounter += pTimeContainer->m_nCounter;
          break;
        }
      }

      if(nFoundId >= arrNoStack.Count())
      {
        arrNoStack.Add(*pTimeContainer);
        arrNoStack.Last().m_szCallStack[0]=0;
      }
    }
  }

  if(GetCVars()->e_profile_level_loading>0)
  { // loading mem stats per func
    PrintMessage("------ Level loading memory allocations (MB) by function ------------");
    PrintMessage("  Self |  Total |  Calls | Function");
    PrintMessage("---------------------------------------------------------------------");

    qsort(arrNoStack.GetElements(), arrNoStack.Count(), sizeof(arrNoStack[0]), Cmp_CLoadingTimeContainer_MemUsage);

    for(int i=0; i < arrNoStack.Count(); i++)
    {
      const CLoadingTimeContainer * pTimeContainer = &arrNoStack[i];
      PrintMessage("%6.1f | %6.1f | %6d | %s", 
        pTimeContainer->m_dSelfMemUsage, pTimeContainer->m_dTotalMemUsage, (int)pTimeContainer->m_nCounter, pTimeContainer->m_pFuncName);
    }

    PrintMessage("---------------------------------------------------------------------");
  }

  if(GetCVars()->e_profile_level_loading>0)
  { // loading time stats per func
    PrintMessage("----------- Level loading time (sec) by function --------------------");
    PrintMessage("  Self |  Total |  Calls | Function");
    PrintMessage("---------------------------------------------------------------------");

    qsort(arrNoStack.GetElements(), arrNoStack.Count(), sizeof(arrNoStack[0]), Cmp_CLoadingTimeContainer_Time);

    for(int i=0; i < arrNoStack.Count(); i++)
    {
      const CLoadingTimeContainer * pTimeContainer = &arrNoStack[i];
      PrintMessage("%6.1f | %6.1f | %6d | %s", 
        pTimeContainer->m_dSelfTime, pTimeContainer->m_dTotalTime, (int)pTimeContainer->m_nCounter, pTimeContainer->m_pFuncName);
    }

    if(GetCVars()->e_profile_level_loading==1)
      PrintMessage("----- ( Use e_profile_level_loading 2 for more detailed stats ) -----");
    else
      PrintMessage("---------------------------------------------------------------------");
  }

  mapLoadingTimeContainers.clear();
}

CLoadingTimeContainer * C3DEngine::StartLoadingSectionProfiling(CLoadingTimeProfiler * pProfiler, const char * szFuncName)
{
  if(!GetCVars()->e_profile_level_loading)
    return NULL;

  pProfiler->m_fConstructorTime = pProfiler->m_pSystem->GetITimer()->GetAsyncCurTime();
  pProfiler->m_fConstructorMemUsage = 0.000001f*(double)pProfiler->m_pSystem->GetUsedMemory();

  static char szCallGraph[LOADING_TIME_CONTAINER_MAX_TEXT_SIZE]; szCallGraph[0]=0;

  if(nLoadingTimeProfilerStackPos>0)
    strcat(szCallGraph, arrLoadingTimeProfilerStack[nLoadingTimeProfilerStackPos-1]->m_szCallStack);

  strcat(szCallGraph, ">>");
  strcat(szCallGraph, szFuncName);

  CLoadingTimeContainer * pTimeContainer = 0;
  if(!(pTimeContainer = stl::find_in_map( mapLoadingTimeContainers, szCallGraph, NULL )))
	{
		pTimeContainer = new CLoadingTimeContainer(szCallGraph,szFuncName);
    mapLoadingTimeContainers[szCallGraph] = pTimeContainer;
	}

  arrLoadingTimeProfilerStack[nLoadingTimeProfilerStackPos] = pTimeContainer; 

  nLoadingTimeProfilerStackPos++;

  assert(nLoadingTimeProfilerStackPos<MAX_LOADING_TIME_PROFILER_STACK_DEPTH);

  return pTimeContainer;
}

void C3DEngine::EndLoadingSectionProfiling(CLoadingTimeProfiler * pProfiler)
{
  if(!GetCVars()->e_profile_level_loading)
    return;

  double fSelfTime = pProfiler->m_pSystem->GetITimer()->GetAsyncCurTime() - pProfiler->m_fConstructorTime;
  double fMemUsage = 0.000001f*(double)pProfiler->m_pSystem->GetUsedMemory();
  double fSelfMemUsage = fMemUsage - pProfiler->m_fConstructorMemUsage;

  pProfiler->m_pTimeContainer->m_dSelfTime += fSelfTime;
  pProfiler->m_pTimeContainer->m_dTotalTime += fSelfTime;
  pProfiler->m_pTimeContainer->m_dSelfMemUsage += fSelfMemUsage;
  pProfiler->m_pTimeContainer->m_dTotalMemUsage += fSelfMemUsage;
  pProfiler->m_pTimeContainer->m_nCounter ++;

  nLoadingTimeProfilerStackPos--;

  if(nLoadingTimeProfilerStackPos>0)
  {
    arrLoadingTimeProfilerStack[nLoadingTimeProfilerStackPos-1]->m_dSelfTime -= fSelfTime;
    arrLoadingTimeProfilerStack[nLoadingTimeProfilerStackPos-1]->m_dSelfMemUsage -= fSelfMemUsage;
  }

  arrLoadingTimeProfilerStack[nLoadingTimeProfilerStackPos] = NULL;
}

void C3DEngine::DetermineBoundingBoxForScattering(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber)
{
	IRenderer* pIRenderer = GetRenderer();
	m_BoundingBoxForScattering.Reset();
	Matrix34 worldmatrix;

	for (int i=0; i<nIRenderNodesNumber; ++i)
	{
		IStatObj* pIStatObj = pIRenderNodes[i]->GetEntityStatObj(0, 0, &worldmatrix);

		// TODO: Set LOD count
		IRenderMesh* pRenderMesh = pIRenderNodes[i]->GetRenderMesh(0);

		if (pRenderMesh)
		{
			int nVertexCount = pRenderMesh->GetVertCount();

			int nStride;
			byte* pVertexPositions = pRenderMesh->GetStridedPosPtr(nStride);
			for (int j=0; j<nVertexCount; ++j)
				m_BoundingBoxForScattering.Add(worldmatrix**reinterpret_cast<Vec3*>(pVertexPositions + j*nStride));
		}
		else
		{
			assert(pIStatObj);
			int subobjectcount = pIStatObj->GetSubObjectCount();
			assert(subobjectcount);

			for (int i=0; i<subobjectcount; ++i)
			{
				IStatObj::SSubObject* pSubObject = pIStatObj->GetSubObject(i);
				if(pSubObject->nType!=STATIC_SUB_OBJECT_MESH || !pSubObject->pStatObj)
					continue;
				pRenderMesh	=	pSubObject->pStatObj->GetRenderMesh();
				if(!pRenderMesh)
					continue;

				Matrix34 worldmatrix_subobject = worldmatrix * pSubObject->tm;
				int nVertexCount = pRenderMesh->GetVertCount();
				int nStride;
				byte* pVertexPositions = pRenderMesh->GetStridedPosPtr(nStride);
				for (int j=0; j<nVertexCount; ++j)
					m_BoundingBoxForScattering.Add(worldmatrix_subobject**reinterpret_cast<Vec3*>(pVertexPositions + j*nStride));
			}
		}
	}

	if (!m_BoundingBoxForScattering.IsReset())
	{
		Vec3 center = m_BoundingBoxForScattering.GetCenter();
		float edge_size = m_BoundingBoxForScattering.max.x - m_BoundingBoxForScattering.min.x;
		edge_size = max(edge_size, m_BoundingBoxForScattering.max.y - m_BoundingBoxForScattering.min.y);
		edge_size = max(edge_size, m_BoundingBoxForScattering.max.z - m_BoundingBoxForScattering.min.z);
		edge_size *= 0.5f;
		// Tbyte HACK
		//edge_size *= 2.0f;
		m_BoundingBoxForScattering.max = center + Vec3(edge_size, edge_size, edge_size);
		m_BoundingBoxForScattering.min = center - Vec3(edge_size, edge_size, edge_size);
	}
}
