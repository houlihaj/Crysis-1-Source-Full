////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   3dengine.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef C3DENGINE_H
#define C3DENGINE_H

#if _MSC_VER > 1000
# pragma once
#endif

#ifdef DrawText
#undef DrawText
#endif //DrawText

struct SLevelInfo
{
  SLevelInfo()
  {
    m_fSunSpecMult = 1.f;
  }

  float m_fSkyBoxAngle,
		m_fSkyBoxStretching;

  float m_fMaxViewDistHighSpec;
  float m_fMaxViewDistLowSpec;
  float m_fTerrainDetailMaterialsViewDistRatio;

	float m_volFogGlobalDensity;
	float m_volFogGlobalDensityMultiplierLDR;
	float m_volFogAtmosphereHeight;
	float m_volFogArtistTweakDensityOffset;

	float m_volFogGlobalDensityMod;
	float m_volFogAtmosphereHeightMod;

	float m_volFogGlobalDensityMultiplier;

	float m_fCloudShadingSunLightMultiplier;
	float m_fCloudShadingSkyLightMultiplier;

  Vec3 m_vFogColor; 
  Vec3 m_vDefFogColor; 
  Vec3 m_vSunDir;
	Vec3 m_vSunDirNormalized;
  float m_fSunDirUpdateTime;
  Vec3 m_vSunDirRealtime;
  Vec3 m_vWindSpeed;

  Vec3 m_vScatteringDir;

	float m_fEyeAdaptionClamp;

	Vec3 m_nightSkyHorizonCol;
	Vec3 m_nightSkyZenithCol;
	float m_nightSkyZenithColShift;
	float m_nightSkyStarIntensity;
	Vec3 m_nightMoonCol;
	float m_nightMoonSize;
	Vec3 m_nightMoonInnerCoronaCol;
	float m_nightMoonInnerCoronaScale;
	Vec3 m_nightMoonOuterCoronaCol;
	float m_nightMoonOuterCoronaScale;

	float m_sunRotationZ;						
	float m_sunRotationLongitude;		
	float m_moonRotationLatitude;					
	float m_moonRotationLongitude;		
	Vec3 m_moonDirection;
	int m_nWaterBottomTexId;
	int m_nCloudShadowTexId;
	int m_nNightMoonTexId;
	bool m_bShowTerrainSurface;
	bool m_bSunShadows;
	Vec3 m_oceanFogColor;
	Vec3 m_oceanFogColorShallow;
	float m_oceanFogDensity;
	float m_oceanFogColorMultiplier;
	float m_skyboxMultiplier;
	float m_dayNightIndicator;

  float m_oceanCausticsDistanceAtten; 
  float m_oceanCausticsMultiplier;  
  float m_oceanCausticsDarkeningMultiplier;

  string m_skyMatName;
  string m_skyLowSpecMatName;

  float m_oceanWindDirection;
  float m_oceanWindSpeed;
  float m_oceanWavesSpeed;
  float m_oceanWavesAmount;
  float m_oceanWavesSize;

	float m_dawnStart;
	float m_dawnEnd;
	float m_duskStart;
	float m_duskEnd;

  // special case for combat mode adjustments
  float m_fSaturation;
  Vec4 m_pPhotoFilterColor;
  float m_fPhotoFilterColorDensity;
  float m_fGrainAmount;
  
  float m_fSunSpecMult;
};

struct SLevelShaders
{
  IMaterial * m_pTerrainWaterMat;
	IShader   * m_pFarTreeSprites;
	IMaterial * m_pSkyMat;
	IMaterial * m_pSkyLowSpecMat;
	IMaterial * m_pSunMat;
};

struct SRenderElements
{
  CRESky              * m_pRESky;
	CREHDRSky           * m_pREHDRSky;
  CREPostProcess      * m_pREPostProcess;   
};

struct SEntInFoliage 
{
	int id;
	float timeIdle;
};

class CMemoryBlock : public IMemoryBlock
{
public:
	virtual void * GetData() { return m_pData; }
	virtual int GetSize() { return m_nSize; }
	virtual ~CMemoryBlock() { delete [] m_pData; }
	
	CMemoryBlock() { m_pData=0; m_nSize=0; }
	CMemoryBlock(void * pData, int nSize) 
	{ 
		m_pData=0; 
		m_nSize=0; 
		SetData(pData, nSize); 
	}
	void SetData(void * pData, int nSize) 
	{ 
		delete [] m_pData;
		m_pData = new uchar[nSize];
		memcpy(m_pData, pData, nSize);
		m_nSize = nSize;
	}
  void Free() 
  { 
    delete [] m_pData;
    m_pData = NULL;
    m_nSize = 0;
  }
	void Allocate(int nSize) 
	{ 
		delete [] m_pData;
		m_pData = new uchar[nSize];
		memset(m_pData, 0, nSize);
		m_nSize = nSize;
	}

  static CMemoryBlock * CompressToMemBlock(void * pData, int nSize, ISystem * pSystem)
  { 
    CMemoryBlock * pMemBlock = NULL;
    uchar * pTmp = new uchar[nSize+4];
    size_t nComprSize = nSize;
    *(uint*)pTmp = nSize;
    if( pSystem->CompressDataBlock(pData, nSize, pTmp+4, nComprSize ))
    {
      pMemBlock = new CMemoryBlock(pTmp, nComprSize+4);
    }

    delete [] pTmp;
    return pMemBlock;
  }

  static CMemoryBlock * DecompressFromMemBlock(CMemoryBlock * pMemBlock, ISystem * pSystem)
  { 
    size_t nUncompSize = *(uint*)pMemBlock->GetData();
    CMemoryBlock * pResult = new CMemoryBlock;
    pResult->Allocate(nUncompSize);
    if(!pSystem->DecompressDataBlock((byte*)pMemBlock->GetData()+4, pMemBlock->GetSize()-4, pResult->GetData(), nUncompSize))
    {
      delete pResult;
      pResult = NULL;
    }

    return pResult;
  }

	uchar * m_pData;
	int m_nSize;
};

struct SNodeInfo;
class CStitchedImage;
struct DLightAmount{ CDLight * pLight; float fAmount; };

#define LOD_TRANSITION_MAX_OBJ_STATES_NUM 16
#define LIGHTS_CULL_INFO_CACHE_SIZE 16

struct CRNTmpData
{
  struct SRNUserData
  {
    int m_narrDrawFrames[MAX_RECURSION_LEVELS];	
    SLodTransitionState states[LOD_TRANSITION_MAX_OBJ_STATES_NUM];
    SLightInfo lightsCache[LIGHTS_CULL_INFO_CACHE_SIZE];
    Matrix34 objMat;
    int objMatFlags;
    float fRadius;
    Vec2 m_vCurrentBending;
    Vec2 m_vFinalBending;
    OcclusionTestClient m_OcclState;
    union
    {
      float		m_HMARange;
      uint32	m_HMAIndex;
    };
    uint m_nGSMFrameId;
    uint m_nOccluderFrameId;
    float fLastFrameDistance;
  } userData;

  CRNTmpData() { memset(this,0,sizeof(*this)); assert((uint)this == (uint)&(this->userData)); }
  CRNTmpData *pNext, *pPrev;
  CRNTmpData ** pOwnerRef;
  int nLastUsedFrameId;
  int nCreatedFrameId;

	void Unlink()
	{
		if (!pNext || !pPrev)
			return;
		pNext->pPrev = pPrev;
		pPrev->pNext = pNext;
		pNext = pPrev = NULL;
	}

	void Link( CRNTmpData* Before )
	{
		if (pNext || pPrev)
			return;
		pNext = Before->pNext;
		Before->pNext->pPrev = this;
		Before->pNext = this;
		pPrev = Before;
	}

	int Count()
	{
		int nCounter = 0;
		for(CRNTmpData * pElem = pNext; pElem!=this; pElem = pElem->pNext)
			nCounter++;
		return nCounter;
	}
};

template <class T, int nMaxElemsInChunk> struct CPoolAllocator
{
  CPoolAllocator() { m_nCounter=0; } 

  ~CPoolAllocator()
  {
    Reset();
  }

  void Reset()
  {
    for(int i=0; i<m_Pools.Count(); i++)
    {
      delete [] m_Pools[i];
      m_Pools[i] = NULL;
    }
    m_nCounter = 0;
  }

  T * GetNewElement() 
  { 
    int nPoolId = m_nCounter/nMaxElemsInChunk;
    int nElemId = m_nCounter - nPoolId*nMaxElemsInChunk;
    m_Pools.PreAllocate(nPoolId+1,nPoolId+1);
    if(!m_Pools[nPoolId])
      m_Pools[nPoolId] = new T[nMaxElemsInChunk];
    m_nCounter++;
    return &m_Pools[nPoolId][nElemId];
  }

  int GetMemUsage() { return m_Pools.Count() * nMaxElemsInChunk * sizeof(T); }

  int GetMaxElemsInChunk() { return nMaxElemsInChunk; }

  int m_nCounter;
  PodArray<T*> m_Pools;
};

//////////////////////////////////////////////////////////////////////
class C3DEngine : public I3DEngine, 
  public SLevelInfo, 
  public SLevelShaders, 
  public SRenderElements,
  public Cry3DEngineBase
{
  // IProcess Implementation
  void	SetFlags(int flags) { m_nFlags=flags; }
	int		GetFlags(void) { return m_nFlags; }
	int		m_nFlags;

public:

  // I3DEngine interface implementation
	virtual bool Init();
  virtual void OnFrameStart();
  virtual void Update();
	virtual void RenderWorld(const int nRenderFlags, const CCamera &cam, const char *szDebugName, const int dwDrawFlags, const int nFilterFlags);
	virtual void ShutDown();
  virtual void Release() { delete this; };
	virtual void SetLevelPath( const char * szFolderName );
	virtual bool LoadLevel(const char * szFolderName, const char * szMissionName);
	virtual void UnloadLevel( bool bForceClearParticlesAssets=false );
	virtual void PostLoadLevel();
	virtual bool InitLevelForEditor(const char * szFolderName, const char * szMissionName);
  virtual void SetupCamera(const CCamera &cam);
  virtual void DisplayInfo(float & fTextPosX, float & fTextPosY, float & fTextStepY, const bool bEnhanced);
  virtual void SetupDistanceFog();
	virtual IStatObj* LoadStatObj( const char *szFileName,const char *szGeomName=NULL,/*[Out]*/IStatObj::SSubObject **ppSubObject=NULL );
  virtual void RegisterEntity( IRenderNode * pEnt );
  virtual void SelectEntity( IRenderNode * pEnt );
  virtual bool UnRegisterEntity( IRenderNode * pEnt );
	virtual bool IsUnderWater( const Vec3& vPos) const;
  virtual void SetOceanRenderFlags( uint8 nFlags );
  virtual uint8 GetOceanRenderFlags() const;
  virtual uint32 GetOceanVisiblePixelsCount() const;
	virtual float GetBottomLevel(const Vec3 & referencePos, float maxRelevantDepth, int objtypes);
	virtual float GetBottomLevel(const Vec3 & referencePos, float maxRelevantDepth /* = 10.0f*/);
	virtual float GetBottomLevel(const Vec3 & referencePos, int objflags);
  virtual float GetWaterLevel(const Vec3 * pvPos = NULL, Vec3 * pvFlowDir = NULL);
  virtual float GetOceanWaterLevel( const Vec3 &pCurrPos ) const;
  virtual Vec4 GetCausticsParams() const;  
  virtual void GetOceanAnimationParams(Vec4 &pParams0, Vec4 &pParams1 ) const;  
	virtual void CreateDecal( const CryEngineDecalInfo& Decal );
  virtual void DrawFarTrees();
  virtual void GenerateFarTrees();
  virtual float GetTerrainElevation(float x, float y, bool bIncludeOutdoorVoxles = false);
  virtual float GetTerrainZ(int x, int y);
  virtual int GetHeightMapUnitSize();
  virtual int GetTerrainSize();
	virtual void SetSunDir( const Vec3& newSunOffset );
  virtual Vec3 GetSunDir();
  virtual Vec3 GetSunDirNormalized();
  virtual Vec3 GetRealtimeSunDirNormalized();
	virtual void SetSkyColor(Vec3 vColor);
	virtual void SetSunColor(Vec3 vColor);
  virtual void SetSunSpecMultiplier(float fMult) { m_fSunSpecMult = fMult; }
	virtual void SetSkyBrightness(float fMul); 
  virtual void SetSSAOAmount(float fMul);
	virtual float GetSunRel();
  virtual void OnExplosion(Vec3 vPos, float fRadius, bool bDeformTerrain = true);
  //! For editor  
  virtual void RemoveAllStaticObjects();
  virtual void SetTerrainSurfaceType(int x, int y, int nType);
	virtual IMaterial* GetTerrainSurfaceMaterial(int x,int y);
	virtual void SetTerrainSectorTexture( const int nTexSectorX, const int nTexSectorY, unsigned int textureId );
  virtual void SetPhysMaterialEnumerator(IPhysMaterialEnumerator * pPhysMaterialEnumerator);
  virtual IPhysMaterialEnumerator * GetPhysMaterialEnumerator();
  virtual void LoadMissionDataFromXMLNode(const char * szMissionName);

  void AddDynamicLightSource(const class CDLight & LSource, ILightSource *pEnt, int nEntityLightId=-1, float fFadeout = 0.f);
  virtual void ApplyForceToEnvironment(Vec3 vPos, float fRadius, float fAmountOfForce);
//  virtual void SetMaxViewDistance(float fMaxViewDistance);
  virtual float GetMaxViewDistance( );
  virtual void SetFogColor(const Vec3& vFogColor);
  virtual Vec3 GetFogColor( );
	virtual float GetDistanceToSectorWithWater();

	virtual void GetVolumetricFogSettings( float& globalDensity, float& atmosphereHeight, float& artistTweakDensityOffset, float& globalDensityMultiplierLDR );
	virtual void SetVolumetricFogSettings( float globalDensity, float atmosphereHeight, float artistTweakDensityOffset );
	virtual void GetVolumetricFogModifiers(float& globalDensityModifier, float& atmosphereHeightModifier);
	virtual void SetVolumetricFogModifiers(float globalDensityModifier, float atmosphereHeightModifier, bool reset = false);	
	virtual void GetVolumetricFogMultipliers(float& globalDensityMultiplier);
	virtual void SetVolumetricFogMultipliers(float globalDensityMultiplier);

	virtual void GetSkyLightParameters( Vec3& sunIntensity, float& Km, float& Kr, float& g, Vec3& rgbWaveLengths );
	virtual void SetSkyLightParameters( const Vec3& sunIntensity, float Km, float Kr, float g, const Vec3& rgbWaveLengths, bool forceImmediateUpdate = false );
		
	virtual void GetCloudShadingMultiplier( float& sunLightMultiplier, float& skyLightMultiplier );
	virtual void SetCloudShadingMultiplier( float sunLightMultiplier, float skyLightMultiplier );

	virtual IAutoCubeMapRenderNode* GetClosestAutoCubeMap(const Vec3& p);

	virtual float GetHDRDynamicMultiplier() const;
	virtual void SetHDRDynamicMultiplier( const float value );
  inline float GetHDRDynamicMultiplierInline() const { return m_fHDRDynamicMultiplier; }

	virtual void TraceFogVolumes( const Vec3& worldPos, ColorF& fogVolumeContrib );


  virtual Vec3 GetSkyColor();
  virtual Vec3 GetSunColor();
  virtual float GetSunSpecMultiplier() { return m_fSunSpecMult; }
	virtual float GetSkyBrightness();
  virtual float GetSSAOAmount();
	virtual float GetSunShadowIntensity() const;

  virtual Vec3 GetAmbientColorFromPosition(const Vec3 & vPos, float fRadius=1.f);
  virtual void ClearRenderResources();
  virtual void FreeRenderNodeState(IRenderNode * pEnt);
  virtual IParticleEmitter* CreateParticleEmitter( bool bIndependent, Matrix34 const& mLoc, const ParticleParams& Params );
  virtual void DeleteParticleEmitter(IParticleEmitter* pPartEmitter);
  virtual const char * GetLevelFilePath(const char * szFileName);
  virtual void SetTerrainBurnedOut(int x, int y, bool bBurnedOut);
  virtual bool IsTerrainBurnedOut(int x, int y);
  virtual int GetTerrainSectorSize();
  virtual void AddWaterSplash (Vec3 vPos, eSplashType eST, float fForce, int Id);
  virtual void LoadTerrainSurfacesFromXML(XmlNodeRef pDoc, bool bUpdateTerrain);
  virtual bool SetStatInstGroup(int nGroupId, const IStatInstGroup & siGroup);
  virtual bool GetStatInstGroup(int nGroupId,       IStatInstGroup & siGroup);
  virtual void ActivatePortal(const Vec3 &vPos, bool bActivate, const char * szEntityName);
  virtual void GetMemoryUsage(class ICrySizer * pSizer);
  virtual IVisArea * CreateVisArea();
  virtual void DeleteVisArea(IVisArea * pVisArea);
  virtual void UpdateVisArea(IVisArea * pArea, const Vec3 * pPoints, int nCount, const char * szName, 
    const SVisAreaInfo & info, bool bReregisterObjects);
  virtual void ResetParticlesAndDecals( );
  virtual IRenderNode* CreateRenderNode( EERType type );
  virtual void DeleteRenderNode(IRenderNode * pRenderNode);
  virtual void SetWind( const Vec3 & vWind );
  virtual Vec3 GetWind( const AABB& box, bool bIndoors ) const;
  virtual IVisArea* GetVisAreaFromPos(const Vec3 &vPos);	
	virtual	bool IntersectsVisAreas(const AABB& box, void** pNodeCache = 0);
	virtual	bool ClipToVisAreas(IVisArea* pInside, Sphere& sphere, Vec3 const& vNormal, void* pNodeCache = 0);
  virtual bool IsVisAreasConnected(IVisArea * pArea1, IVisArea * pArea2, int nMaxReqursion, bool bSkipDisabledPortals);
  virtual ILMSerializationManager * CreateLMSerializationManager();
	virtual IVertexScatterSerializationManager * CreateVertexScatterSerializationManager() const;
  void EnableOceanRendering(bool bOcean); // todo: remove
	//////////////////////////////////////////////////////////////////////////
  // Materials access.
	virtual IMaterialManager* GetMaterialManager();
	//////////////////////////////////////////////////////////////////////////
	// ParticleEffect
	virtual IParticleEffect* CreateParticleEffect();
	virtual void DeleteParticleEffect( IParticleEffect* pEffect );
	virtual IParticleEffect* FindParticleEffect( const char *sEffectName, const char* sCallerType = "", const char* sCallerName = "", bool bLoad = true );
	//////////////////////////////////////////////////////////////////////////

	virtual struct ILightSource * CreateLightSource();
	virtual void DeleteLightSource(ILightSource * pLightSource);
  virtual const PodArray<CDLight*> * GetStaticLightSources();
  virtual bool IsTerrainHightMapModifiedByGame();
  virtual bool RestoreTerrainFromDisk();  
  virtual void CheckMemoryHeap();
	virtual bool SetMaterialFloat( char * szMatName, int nSubMatId, int nTexSlot, char * szParamName, float fValue );
	virtual void CloseTerrainTextureFile();
	virtual int GetLoadedObjectCount();
	virtual void GetLoadedStatObjArray( IStatObj** pObjectsArray,int &nCount );
	virtual void DeleteEntityDecals(IRenderNode * pEntity);
	virtual void DeleteDecalsInRange( AABB * pAreaBox, IRenderNode * pEntity );
	virtual void LoadLightmaps();
	virtual void CompleteObjectsGeometry();
	virtual void LockCGFResources();
	virtual void UnlockCGFResources();
	//! paint voxel shape
	IMemoryBlock * Voxel_GetObjects(Vec3 vPos, float fRadius, int nSurfaceTypeId, EVoxelEditOperation eOperation, EVoxelBrushShape eShape, EVoxelEditTarget eTarget);
	virtual void Voxel_Paint(Vec3 vPos, float fRadius, int nSurfaceTypeId, Vec3 vBaseColor, EVoxelEditOperation eOperation, EVoxelBrushShape eShape, EVoxelEditTarget eTarget);
	virtual void Voxel_SetFlags(bool bPhysics, bool bSimplify, bool bShadows, bool bMaterials);

	virtual void SerializeState( TSerialize ser );
	virtual void PostSerialize( bool bReading );

	virtual void SetHeightMapMaxHeight(float fMaxHeight);

	//////////////////////////////////////////////////////////////////////////
	// CGF Loader.
	//////////////////////////////////////////////////////////////////////////
	virtual CContentCGF* LoadChunkFileContent( const char *filename,bool bNoWarningMode=false );
	virtual void ReleaseChunkFileContent( CContentCGF *pCGF );
	//////////////////////////////////////////////////////////////////////////
	virtual IChunkFile *CreateChunkFile( bool bReadOnly=false );

  //////////////////////////////////////////////////////////////////////////
  // Post processing effects interfaces    

  virtual void SetPostEffectParam(const char *pParam, float fValue) const;
  virtual void SetPostEffectParamVec4(const char *pParam, const Vec4 &pValue) const;
  virtual void SetPostEffectParamString(const char *pParam, const char *pszArg) const;

  virtual void GetPostEffectParam(const char *pParam, float &fValue) const;  
  virtual void GetPostEffectParamVec4(const char *pParam, Vec4 &pValue) const;  
  virtual void GetPostEffectParamString(const char *pParam, const char *pszArg) const;

  virtual void ResetPostEffects(bool delayed = false) const;

	virtual uint32 GetObjectsByType( EERType objType, IRenderNode **pObjects );

  //////////////////////////////////////////////////////////////////////////
  
	virtual int GetTerrainTextureNodeSizeMeters();
	virtual int GetTerrainTextureNodeSizePixels(int nLayer);

	bool SaveCGF(std::vector<IStatObj *>& pObjs);

  int GetCurrentLightSpec()
  {
    if (m_pLightQuality)
      return m_pLightQuality->GetIVal() + CONFIG_LOW_SPEC; // Light quality is 0,1,2
    return CONFIG_VERYHIGH_SPEC; // very high spec.
  }

	bool IsCameraUnderWater() const { return m_bCameraUnderWater; }

	void ScreenShotHighRes(CStitchedImage* pStitchedImage,const int nRenderFlags, const CCamera &cam, const int dwDrawFlags, const int nFilterFlags,uint32 SliceCount,f32 fTransitionSize);

	// cylindrical mapping made by multiple slices rendered and distorted
	// Returns:
	//   true=mode is active, stop normal rendering, false=mode is not active
	bool ScreenShotPanorama(CStitchedImage*	pStitchedImage,const int nRenderFlags, const CCamera &_cam, const int dwDrawFlags, const int nFilterFlags,uint32 SliceCount,f32 fTransitionSize );
	// Render simple top-down screenshot for map overviews
	bool ScreenShotMap(CStitchedImage* pStitchedImage,const int nRenderFlags, const CCamera &_cam, const int dwDrawFlags, const int nFilterFlags,uint32 SliceCount,f32 fTransitionSize );

	void ScreenshotDispatcher(const int nRenderFlags, const CCamera &_cam, const int dwDrawFlags, const int nFilterFlags);

	virtual void FillDebugFPSInfo(SDebugFPSInfo& info);

	void ClearDebugFPSInfo()
	{
		m_fAverageFPS = 0.0f;
		m_fMinFPS = 0.0f;
		m_fMaxFPS = 0.0f;
		m_nFramesCount = 0;
		memset(afFPSCounts, 0, sizeof(afFPSCounts));
		arrFrameTimeforSaveLevelStats.clear();
	}

	virtual bool RefineRayHit(ray_hit *phit, const Vec3 &dir);

public:
  C3DEngine( ISystem * pSystem );
  ~C3DEngine();

  virtual void RenderScene(const int nRenderFlags, unsigned int dwDrawFlags, const int nFilterFlags);
	virtual void DebugDraw_PushFrustrum( const char *szName, const CCamera &rCam, const ColorB col, const float fQuadDist=-1.0f );
	virtual void DebugDraw_PushFrustrum( const char *szName, const CRenderCamera &rCam, const ColorB col, const float fQuadDist=-1.0f );
  uint BuildLightMask( const AABB & objBox );
  uint BuildLightMask( const AABB & objBox, PodArray<CDLight*> * pAffectingLights, CVisArea * pObjArea, bool bObjOutdoorOnly );
  bool BuildLightMask_CheckPortals( CVisArea * pObjArea, CVisArea * pLightArea, CDLight * pLight, const AABB & objBox );
	void DebugDraw_Draw();
  uint GetFullLightMask();
  bool IsOutdoorVisible();
  void RenderSkyBox(IMaterial*pMat);

	bool CreateDecalOnStatObj( const CryEngineDecalInfo& DecalInfo, class CDecal* pCallerManagedDecal );
	//void CreateDecalOnCharacterComponents(ICharacterInstance * pChar, const struct CryEngineDecalInfo & decal);

  // access to components
  ILINE CVars * GetCVars() { return m_pCVars; }
  ILINE CVisAreaManager * GetVisAreaManager() { return m_pVisAreaManager; }
	ILINE CMatMan * GetMatMan() { return m_pMatMan; }
	ILINE const PodArray<ILightSource*> * GetLightEntities() { return &m_lstStaticLights; }

	ICVar *m_pSphericalSkinning;
  ICVar *m_pDeformableObjects;
	AABB m_BoundingBoxForScattering;

private:

	static const int nFPSMaxTrack = 120;
	float afFPSCounts[nFPSMaxTrack+1];

	std::vector<float> arrFrameTimeforSaveLevelStats;

  CRenderObject *	m_SunObject[MAX_RECURSION_LEVELS];	// misc
  float						m_fSunFlareScale;										// Save sun flare scale here, since somehow lights info is always getting overridden
  float						m_fHDRDynamicMultiplier;						// only used for HDR, to amplify the dynamic range for emissive objects, 1.0 means no amplification

  int m_nBlackTexID;
  char m_sGetLevelFilePathTmpBuff[MAX_PATH_LENGTH];
  char m_szLevelFolder[_MAX_PATH];

  bool m_bOcean; // todo: remove

	Vec3 m_vSkyHightlightPos;
	Vec3 m_vSkyHightlightCol;
	float m_fSkyHighlightSize;
	IPhysicalEntity* m_pGlobalWind;
	bool m_bCameraUnderWater;
  uint8 m_nOceanRenderFlags;
  Vec3  m_vPrevMainFrameCamPos;
  bool m_bContentPrecacheRequested;
  
  // interfaces
  IPhysMaterialEnumerator * m_pPhysMaterialEnumerator;

  // data containers
  PodArray<CDLight> m_lstDynLights;
	PodArray<CDLight> m_lstDynLightsNoLight;
	int m_nRealLightsNum;
  int m_nRenderLightsNum;
  PodArray<ILightSource*> m_lstStaticLights;
	PodArray<PodArray<struct ILightSource*>*> m_lstAffectingLightsCombinations;

#define MAX_LIGHTS_NUM 32
  PodArray<CCamera> m_arrLightProjFrustums;

	class CTimeOfDay*    m_pTimeOfDay;

  ICVar*					m_pLightQuality;
    
  bool            m_bHRDEnabled;  
  ICVar           *m_pResetSfx;

  ICVar           *m_pHRDRendering;

	IFFont          *m_pConsolepFont;

	// FPS for savelevelstats 

	float m_fAverageFPS;
	float m_fMinFPS;
	float m_fMaxFPS;
	long  m_nFramesCount;
// not sorted
  
  //! Saving of cgf file
  bool WriteMaterials(TArray<CHUNK_HEADER>& Chunks, TArray<IShader *>& Shaders, FILE *out, int &MatChunk);
  bool WriteNodes(TArray<CHUNK_HEADER>& Chunks, TArray<NODE_CHUNK_DESC>& Nodes, FILE *out, TArray<SNodeInfo>& NI, int& MatChunk, int& ExpFrame, std::vector<IStatObj *>& pObjs);
  bool WriteMesh(TArray<CHUNK_HEADER>& Chunks, TArray<NODE_CHUNK_DESC>& Nodes, TArray<IShader *>& Shaders, FILE *out, TArray<SNodeInfo>& NI, int& MatChunk, int& ExpFrame);
  bool WriteNodeMesh(int nNode, MESH_CHUNK_DESC *chunk, FILE *out, TArray<IShader *>& Shaders, TArray<SNodeInfo>& NI, struct CStatObj *pObj);
  bool WriteLights(TArray<CHUNK_HEADER>& Chunks, TArray<NODE_CHUNK_DESC>& Nodes, FILE *out, std::vector<IStatObj *>& pObjs);

  void LoadEnvironmentSettingsFromXML(XmlNodeRef pInputNode);
	void LoadTimeOfDaySettingsFromXML( XmlNodeRef node );
  char * GetXMLAttribText(XmlNodeRef pInputNode, const char * szLevel1,const char * szLevel2,const char * szDefaultValue);
	char * GetXMLAttribText( XmlNodeRef pInputNode, const char* szLevel1, const char* szLevel2, const char* szLevel3, const char* szDefaultValue );

	// without calling high level functions like panorama screenshot 
	void RenderInternal(const int nRenderFlags, const CCamera &cam, const char *szDebugName, const int dwDrawFlags, const int nFilterFlags);

  void RegisterLightSourceInSectors(CDLight * pDynLight);
  Vec3 GetTerrainSurfaceNormal(Vec3 vPos);

  bool IsCameraAnd3DEngineInvalid(const CCamera cam, const char * szCaller);

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	void SetupLightScissors(CDLight *pLight);

	void ResetCasterCombinationsCache();

	void FindPotentialLightSources();
	void DeleteAllStaticLightSources();
	void LoadParticleEffects( XmlNodeRef &levelDataRoot );

	void UpdateSunLightSource();

	IMaterial* GetSkyMaterial();

	class CCullBuffer* m_pCoverageBuffer;
	struct CLightEntity * m_pSun;

	void UpdateMoonDirection();

public:

  bool IsTerrainSyncLoad() { return m_bContentPrecacheRequested && GetCVars()->e_level_auto_precache_terrain_and_proc_veget; }
  bool IsShadersSyncLoad() { return m_bContentPrecacheRequested && GetCVars()->e_level_auto_precache_textures_and_shaders; }

	typedef std::map<uint64,PodArray<ShadowMapFrustum*>*> ShadowFrustumListsCache;
	ShadowFrustumListsCache m_FrustumsCache[2];
	typedef std::map<int,int> ShadowFrustumListsCacheUsers;
	ShadowFrustumListsCacheUsers m_FrustumsCacheUsers[2];

	class CStatObjFoliage *m_pFirstFoliage,*m_pLastFoliage;
	PodArray<SEntInFoliage> m_arrEntsInFoliage;
	void RemoveEntInFoliage(int i);

  PodArray<class CVoxelObject*> m_lstVoxelObjectsForUpdate;
  PodArray<class CRoadRenderNode*> m_lstRoadRenderNodesForUpdate;

	struct ILightSource * GetSunEntity();
	PodArray<struct ILightSource*> * GetAffectingLights(const AABB & bbox, bool bAllowSun);
	void UregisterLightFromAccessabilityCache(ILightSource * pLight);

	CCullBuffer * GetCoverageBuffer() { return m_pCoverageBuffer; }

	void UpdateScene();
  void SortForShadowMask();
	void UpdateLightSources();
	void PrepareLightSourcesForRendering_0();
  void PrepareLightSourcesForRendering_1();
	void InitShadowFrustums();

  void FreeLightSourceComponents(CDLight *pLight);
	void RemoveEntityLightSources(IRenderNode * pEntity);

  void CheckPhysicalized(const Vec3 & vBoxMin, const Vec3 & vBoxMax);

  PodArray<CDLight> * GetDynamicLightSources() { return &m_lstDynLights; }  
  int GetRealLightsNum() { return m_nRealLightsNum; }
  int & GetRenderLightsNum() { return m_nRenderLightsNum; }
	void SetupClearColor();
  void CheckAddLight(CDLight*pLight);

	void DrawText(float x, float y, const char * format, ...) PRINTF_PARAMS(4, 5);
	void DrawTextRA(float x, float y, const char * format, ...) PRINTF_PARAMS(4, 5);

	float GetLightAmount(CDLight * pLight, const AABB & objBox);
    
	IStatObj * CreateStatObj();

	IStatObj * UpdateDeformableStatObj(IGeometry *pPhysGeom, bop_meshupdate *pLastUpdate=0, IFoliage *pSrcFoliage=0);
	IStatObj * SmearStatObj(IStatObj *pObj, const Vec3 &pthit,const Vec3 &dir, float r,float depth);

	// Creates a new indexed mesh.
	IIndexedMesh* CreateIndexedMesh();

	void InitMaterialDefautMappingAxis(IMaterial * pMat);    

	virtual ITerrain * GetITerrain() { return (ITerrain*)m_pTerrain; }
	virtual IVisAreaManager * GetIVisAreaManager() { return (IVisAreaManager*)m_pVisAreaManager; }
	
	ITerrain * CreateTerrain(const STerrainInfo & TerrainInfo);
	void DeleteTerrain();
	bool LoadTerrain(XmlNodeRef pDoc, std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable);
	bool LoadVisAreas(std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable);
	bool LoadUsedShadersList();
	bool PrecreateDecals();
	void LoadVertexScatter(const char* szFolderName);

	void GetVoxelRenderNodes(struct IRenderNode**pRenderNodes, int & nCount);
 
  virtual float GetLightAmountInRange(const Vec3 &pPos, float fRange, bool bAccurate = 0);
    
	virtual void PrecacheLevel(bool bPrecacheAllVisAreas, Vec3 * pPrecachePoints, int nPrecachePointsNum, int *pPrecachedPointsCount = NULL);
  virtual void ProposeContentPrecache() { m_bContentPrecacheRequested = true; }
      
	virtual ITimeOfDay* GetTimeOfDay();

	bool RenderPotentialOccluders(CCullBuffer & rCB, const CCamera & viewCam, bool bResetAffectedLights);
	bool RenderVisAreaPotentialOccluders(CVisArea * pThisArea, CCullBuffer & rCB, const CCamera & viewCam, bool bResetAffectedLights);

	virtual void SetGlobalParameter( E3DEngineParameter param,const Vec3 &v );
	virtual void GetGlobalParameter( E3DEngineParameter param,Vec3 &v );

	virtual int SaveStatObj(IStatObj *pStatObj, TSerialize ser);
	virtual IStatObj *LoadStatObj(TSerialize ser);

	virtual bool CheckIntersectClouds(const Vec3 & p1, const Vec3 & p2);
	virtual void OnRenderMeshDeleted(IRenderMesh * pRenderMesh);
	virtual bool RayObjectsIntersection( Vec3 vStart, Vec3 vEnd, Vec3 & vHitPoint, EERType eERType );

  virtual bool IsAmbientOcclusionEnabled();

  void MarkRNTmpDataPoolForReset() { m_bResetRNTmpDataPool = true; }

	class COctreeNode * m_pObjectsTree;

	int m_idMatLeaves; // for shooting foliages
  bool m_bResetRNTmpDataPool;

  float m_curr_e_vegetation_sprites_distance_ratio;
  float m_curr_e_vegetation_sprites_distance_custom_ratio_min;
	float m_curr_e_view_dist_ratio;
	float m_curr_e_view_dist_ratio_detail;
	float m_curr_e_view_dist_ratio_veg;
	int   m_curr_e_default_material;
  int   m_nGeomDetailScreenRes;

  PodArray<IRenderNode*> m_lstAlwaysVisible;
  PodArray<IRenderNode *> m_lstKilledVegetations;

	CRNTmpData m_LTPRootFree, m_LTPRootUsed;
	void CheckCreateRNTmpData(CRNTmpData ** ppInfo, IRenderNode * pRNode);
	void FreeRNTmpData(CRNTmpData ** ppInfo);
	void UpdateRNTmpDataPool(bool bFreeAll);
	void FreeRNTmpDataPool();
  
//  CPoolAllocator<CRNTmpData, 512> m_RNTmpDataPools;

  virtual struct CLoadingTimeContainer * StartLoadingSectionProfiling(struct CLoadingTimeProfiler * pProfiler, const char * szFuncName);
  virtual void EndLoadingSectionProfiling(struct CLoadingTimeProfiler * pProfiler);
  void OutputLoadingTimeStats();
  void UpdateStatInstGroups();
  void ProcessOcean(const int dwDrawFlags);
  void ReRegisterKilledVegetationInstances();
  Vec3 GetEntityRegisterPoint( IRenderNode* pEnt );

	virtual void DetermineBoundingBoxForScattering(struct IRenderNode* const *pIRenderNodes, int32 nIRenderNodesNumber);
};
  
#endif // C3DENGINE_H
