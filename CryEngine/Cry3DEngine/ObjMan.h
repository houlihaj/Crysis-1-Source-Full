////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjman.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef CObjManager_H
#define CObjManager_H

enum EOcclusionObjectType
{
	eoot_OCCLUDER,
	eoot_OCEAN,
	eoot_OCCELL,
	eoot_OCCELL_OCCLUDER,
	eoot_OBJECT,
	eoot_OBJECT_TO_LIGHT,
	eoot_TERRAIN_NODE,
	eoot_PORTAL,
};

#include "StatObj.h"
#include "../RenderDll/Common/Shadow_Renderer.h"
#include "terrain_sector.h"
#include "StlUtils.h"
#include "CullBuffer.h"

#include <map>
#include <vector>

#define   ENTITY_MAX_DIST_FACTOR 100
#define MAX_VALID_OBJECT_VOLUME (10000000000.f)

struct CStatObj;
struct IIndoorBase;
struct IRenderNode;
struct ISystem;
struct IDecalRenderNode;

class CVegetation;

class C3DEngine;
struct IMaterial;

#define SMC_EXTEND_FRUSTUM 8
#define SMC_SHADOW_FRUSTUM_TEST 16

#define OCCL_TEST_HEIGHT_MAP	1
#define OCCL_TEST_CBUFFER			2
#define OCCL_TEST_INDOOR_OCCLUDERS_ONLY		4

const int FAR_TEX_COUNT = 24; // number of sprites per object
const int FAR_TEX_COUNT_60 = (FAR_TEX_COUNT/2); // number of 60 degree sprites per object
const int FAR_TEX_ANGLE = (360/FAR_TEX_COUNT);
const int FAR_TEX_ANGLE_60 = (360/FAR_TEX_COUNT_60);

//! contains stat obj instance group properies (vegetation object properties)
struct StatInstGroup : public IStatInstGroup
{
	StatInstGroup() 
  { 
    pStatObj = 0; 
    ZeroStruct( m_arrSpriteTexPtr );
    ZeroStruct( m_arrSpriteTexPtr_60 );
  }
	
	CStatObj * GetStatObj()
	{ 
		IStatObj * p = pStatObj;
		return (CStatObj*)p;
	}

	void Update(struct CVars * pCVars, int nGeomDetailScreenRes);

  // group sprite gen data
  struct SSpriteLightInfo
  {
    SSpriteLightInfo() { m_vSunDir = m_vSunColor = m_vSkyColor = Vec3(0,0,0); m_BrightnessMultiplier=0; }

    Vec3			m_vSunColor;
    Vec3			m_vSkyColor;
    uint8			m_BrightnessMultiplier;		// needed to allow usage of 8bit texture for HDR, computed by ComputeSpriteBrightnessMultiplier()

    // -----------------------------------------

    void SetLightingData( const Vec3 &vSunDir, const Vec3 &vSunColor, const Vec3 &vSkyColor )
    {
      m_vSunDir=vSunDir;
      m_vSunColor=vSunColor;
      m_vSkyColor=vSkyColor;
      ComputeSpriteBrightnessMultiplier();
    }

		// vSunDir should be normalized
    bool IsEqual( const Vec3 &vSunDir, const Vec3 &vSunColor, const Vec3 &vSkyColor, const float fDirThreshold, const float fColorThreshold ) const
    {
      return IsEquivalent(m_vSunDir,vSunDir,fDirThreshold)
        && IsEquivalent(m_vSunColor,vSunColor,fColorThreshold)
        && IsEquivalent(m_vSkyColor,vSkyColor,fColorThreshold);
    }

    void ComputeSpriteBrightnessMultiplier()
    {
      float fMax = max(m_vSunColor.x+m_vSkyColor.x,max(m_vSunColor.y+m_vSkyColor.y,m_vSunColor.z+m_vSkyColor.z));

      m_BrightnessMultiplier = (uint8)min((uint32)255,(uint32)(fMax*(255.0f/10.0f)+0.5f));		// 10.0f allows 10x brightness
    }

    float GetSpriteBrightnessMultiplier() const
    {
      return m_BrightnessMultiplier*(10.0f/255.0f);		// 10.0f allows 10x brightness
    }

	private: // --------------------------------------------------------

		Vec3			m_vSunDir;								// normalized sun direction
  };

  IDynTexture *m_arrSpriteTexPtr[FAR_TEX_COUNT];
  SSpriteLightInfo m_arrSSpriteLightInfo[FAR_TEX_COUNT];
  IDynTexture *m_arrSpriteTexPtr_60[FAR_TEX_COUNT_60];
  SSpriteLightInfo m_arrSSpriteLightInfo_60[FAR_TEX_COUNT_60];
	float m_fSpriteSwitchDistUnScaled;
};

struct SExportedBrushMaterial
{
  int size;
  char material[64];
};

struct SRenderMeshInfoInput
{
	SRenderMeshInfoInput() { memset(this,0,sizeof(*this)); nChunkId=-1; }
	IRenderMesh * pMesh;
	IMaterial * pMat;
	Matrix34	mat;
	int nChunkId;
	IRenderMesh * pLMTexCoordsRM;
	IRenderNode * pSrcRndNode;

	// light map data
	struct SSharedLMInfo
	{
		int iRAETex;
    int nRndFlags;
	} LMInfo;
};

struct SRenderMeshInfoOutput
{
	SRenderMeshInfoOutput() { memset(this,0,sizeof(*this)); }
	IRenderMesh * pMesh;
	Vec2 * pLMTexCoords;
};

class CObjManager : public Cry3DEngineBase
{
public:
  CObjManager();
  ~CObjManager();

  void UnloadObjects(bool bDeleteAll);
  void UnloadVegetationModels();

	void DrawFarObjects(float fMaxViewDist);
  void GenerateFarObjects(float fMaxViewDist);
  void RenderFarObjects();

  template <class T>
	static int GetItemId(std::vector<T*> * pArray, T * pItem, bool bAssertIfNotFound = true)
	{
    for (uint32 i =0, end = pArray->size() ; i < end; ++i)
      if ((*pArray)[i] == pItem)
        return i;

    if(bAssertIfNotFound)
      assert(!"Item not found");

    return -1;
	}

  template <class T>
  static T * GetItemPtr(std::vector<T*> * pArray, int nId)
  {
    if(nId<0)
      return NULL;

    assert(nId < pArray->size());

    return (*pArray)[nId];
  }

  CStatObj *LoadStatObj( const char *szFileName,const char *szGeomName=NULL,IStatObj::SSubObject **ppSubObject=NULL,bool bLoadLater=false );
	void GetLoadedStatObjArray( IStatObj** pObjectsArray,int &nCount );
	
	// Deletes object.
	// Only should be called by Release function of CStatObj.
	bool InternalDeleteObject( CStatObj *pObject );

	std::vector<StatInstGroup> m_lstStaticTypes;

  PodArray<ShadowMapFrustum*> * GetShadowFrustumsList(PodArray<CDLight*> * pAffectingLights, const AABB & aabbReceiver, 
		float fObjDistance, uint nDLightMask, bool bIncludeNearFrustums);

  PodArray<SVegetationSpriteInfo> m_lstFarObjects[MAX_RECURSION_LEVELS];

	void MakeShadowCastersList(CVisArea*pReceiverArea, const AABB & aabbReceiver, 
		int dwAllowedTypes, Vec3 vLightPos, CDLight * pLight, ShadowMapFrustum * pFr, PodArray<struct SPlaneObject> * pShadowHull);

	// decal pre-caching
	typedef std::vector< IDecalRenderNode* > DecalsToPrecreate;
	DecalsToPrecreate m_decalsToPrecreate;

protected:
  void InitFarState();

	typedef std::map<string,CStatObj*,stl::less_stricmp<string> > ObjectsMap;
	ObjectsMap m_nameToObjectMap;

	typedef std::set<CStatObj*> LoadedObjects;
	LoadedObjects m_lstLoadedObjects;

protected:
  CREFarTreeSprites * m_REFarTreeSprites;

#ifdef WIN64
#pragma warning( push )									//AMD Port
#pragma warning( disable : 4267 )
#endif

public:
	int GetLoadedObjectCount() { return m_lstLoadedObjects.size(); }

#ifdef WIN64
#pragma warning( pop )									//AMD Port
#endif


  void RenderObject( IRenderNode * o,
        PodArray<CDLight*> * pAffectingLights, 
				const Vec3 & vAmbColor,
				const AABB & objBox, 
				float fEntDistance, 
				const CCamera & rCam, 
        struct SInstancingInfo * pInstInfo,
        bool bSunOnly);
	void RenderOccluderIntoZBuffer(IRenderNode * pEnt, float fEntDistance, CCullBuffer & rCB, bool bCompletellyInFrustum);
	bool CullBackObject(IRenderNode * pEnt, const Vec3 & vCamPos, const Vec3 & vCenter);
	void RenderObjectDebugInfo(IRenderNode * pEnt, float fEntDistance, const SRendParams & DrawParams);

  float GetXYRadius(int nType);
  bool GetStaticObjectBBox(int nType, Vec3 & vBoxMin, Vec3 & vBoxMax);
  
  Vec3					m_vSkyColor;				// 
	float					m_fSkyBrightness;		
  Vec3					m_vSunColor;				// 
	float					m_fSunSkyRel;				//relation factor of sun sky, 1->sun has full part of brightness, 0->sky has full part
	float					m_fILMul;
	float					m_fSkyBrightMul;
  float					m_fSSAOAmount;

  IStatObj * GetStaticObjectByTypeID(int nTypeID);
  float GetBendingRandomFactor();

  bool IsBoxOccluded(const AABB & objBox, float fDistance, OcclusionTestClient * pOcclTestVars, bool bIndoorOccludersOnly, EOcclusionObjectType eOcclusionObjectType );

  void AddParticleToRenderer( const int nTexBindId, 
															IMaterial * pMat, 
															bool isCustomMat,
															const int nDynLMask,
															Vec3 right,
															Vec3 up,
															const UCol& ucResCol,
															const EParticleBlendType eBlendType,
															const ColorF& clrAmbient,
															uint16 fogColorContribIdx,
															Vec3 vPos,
															const RectF& TexRect,
															const struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F* pTailVerts = NULL,
															const int nTailVertsNum = 0,
															const byte* pTailIndices = NULL,
															const int nTailIndicesNum = 0,
															const float fSortId = 0,
															const int dwCCObjFlags = 0,
															PodArray<ShadowMapFrustum*>* pShadowMapCasters = NULL,
															float fRadius = -1.0f );

	void AddParticleToRenderer( const int nTexBindId, 
														  IMaterial* pMat,
														  bool isCustomMat,
														  const int nDynLMask,
														  const UCol & ucResCol,
														  const EParticleBlendType eBlendType,
														  const ColorF& clrAmbient,
															uint16 fogColorContribIdx,
														  const Vec3& vPos,
														  Vec2 vXAxis,
														  Vec2 vYAxis,
															const RectF& TexRect,
														  const float fSortId = 0,
														  const int dwCCObjFlags = 0,
														  PodArray< ShadowMapFrustum* >* pShadowMapCasters = 0 );

	void AddDecalToRenderer( IMaterial* pMat,
													 const int nDynLMask,
													 const uint8 sortPrio,
													 Vec3 right,
													 Vec3 up,
													 const UCol& ucResCol,
													 const EParticleBlendType eBlendType,
													 const Vec3& vAmbientColor,
													 Vec3 vPos,
													 const int nAfterWater,
													 CVegetation* pVegetation = 0 );

  // tmp containers (replacement for local static vars)

//  void DrawObjSpritesSorted(PodArray<CVegetation*> *pList, float fMaxViewDist, int useBending);
//	void ProcessActiveShadowReceiving(IRenderNode * pEnt, float fEntDistance, CDLight * pLight, bool bFocusOnHead);

//	void SetupEntityShadowMapping( IRenderNode * pEnt, SRendParams * pDrawParams, float fEntDistance, CDLight * pLight );
	float m_fMaxViewDistanceScale;
	float m_fGSMMaxDistance;
	IShader * m_pShaderOcclusionQuery;
	
//	bool LoadStaticObjectsFromXML(XmlNodeRef xmlVegetation);
	_smart_ptr<CStatObj> m_pDefaultCGF;
	IRenderMesh * m_pRMBox;

	//////////////////////////////////////////////////////////////////////////
	std::vector<_smart_ptr<IStatObj> > m_lockedObjects;
	bool m_bLockCGFResources;

	//////////////////////////////////////////////////////////////////////////
	std::vector<CStatObj*> m_checkForGarbage;

	//////////////////////////////////////////////////////////////////////////

	void GetMemoryUsage(class ICrySizer * pSizer);

//  PodArray<class CBrush*> m_lstBrushContainer;
//  PodArray<class CVegetation*> m_lstVegetContainer;
	void LoadBrushes();
//  void MergeBrushes();
	void ReregisterEntitiesInArea(Vec3 vBoxMin, Vec3 vBoxMax);
//	void ProcessEntityParticles(IRenderNode * pEnt, float fEntDistance);
	void CheckUnload();
	void PreloadNearObjects();
	// time counters

	static bool IsAfterWater(const Vec3 & vPos, const Vec3 & vCamPos, float fUserWaterLevel = WATER_LEVEL_UNKNOWN);

  void SetupWindBending( IRenderNode *pEnt, SRendParams &pDrawParams, const AABB & objBox );

  void GetObjectsStreamingStatus(int & nReady, int & nTotalInStreaming, int & nTotal);
//	bool ProcessShadowMapCasting(IRenderNode * pEnt, CDLight * pLight);
	void CheckObjectLeaks(bool bDeleteAll = false);
//	bool IsSphereAffectedByShadow(IRenderNode * pCaster, IRenderNode * pReceiver, CDLight * pLight);
//	void MakeShadowCastersListInArea(CBasicArea * pArea, const AABB & boxReceiver, 
//		int dwAllowedTypes, Vec3 vLightPos, CDLight * pLight, ShadowMapFrustum * pFr, PodArray<struct SPlaneObject> * pShadowHull );
	void PrefechObjects();
//	void DrawEntityShadowFrustums(IRenderNode * pEnt);
	
	void FreeNotUsedCGFs();
//	void RenderObjectVegetationNonCastersNoFogVolume( IRenderNode * pEnt,uint nDLightMask,
	//	const CCamera & EntViewCamera, 
		//bool bAllInside, float fMaxViewDist, IRenderNodeInfo * pEntInfo);
//	void InitEntityShadowMapInfoStructure(IRenderNode * pEnt);
//	float CalculateEntityShadowVolumeExtent(IRenderNode * pEntity, CDLight * pLight);
//	void MakeShadowBBox(Vec3 & vBoxMin, Vec3 & vBoxMax, const Vec3 & vLightPos, float fLightRadius, float fShadowVolumeExtent);
	void MakeUnitCube();
//	bool IsBoxOccluded_HWOcclQuery( const AABB & objBox, float fDistance, OcclusionTestClient * pOcclTestVars );
	bool IsBoxOccluded_HeightMap( const AABB & objBox, float fDistance, EOcclusionObjectType eOcclusionObjectType );
//	void UnregisterCGFFromTypeTables(CStatObj * pStatObj);	
	bool RenderStatObjIntoZBuffer(IStatObj * pStatObj, Matrix34 & objMatrix, 
		unsigned int nRenderFlags, bool bCompletellyInFrustum,
		CCullBuffer & rCB, IMaterial * pMat);

	//////////////////////////////////////////////////////////////////////////
	// Garbage collection for parent stat objects.
	//////////////////////////////////////////////////////////////////////////
	void CheckForGarbage( CStatObj *pStatObj,bool bAdd );
	void ClearStatObjGarbage();
	
	
	float m_fOcclTimeRatio;

	static int GetObjectLOD(float fDistance, float fLodRatioNorm, float fRadius);
	static bool RayStatObjIntersection(IStatObj * pStatObj, const Matrix34 & objMat, IMaterial * pMat,
		Vec3 vStart, Vec3 vEnd, Vec3 & vClosestHitPoint, float & fClosestHitDistance, bool bFastTest);
	static bool RayRenderMeshIntersection(IRenderMesh * pRenderMesh, const Vec3 & vInPos, const Vec3 & vInDir, Vec3 & vOutPos, Vec3 & vOutNormal,bool bFastTest,IMaterial * pMat);
  static bool SphereRenderMeshIntersection(IRenderMesh * pRenderMesh, const Vec3 & vInPos, const float fRadius);
  static void CullLightsPerTriangle(IRenderMesh * pRenderMesh, uint & nDLightMask, CRNTmpData * pRNTmpData);
  static void FillTerrainTexInfo(IRenderNode * pEnt, EERType eERType, float fEntDistance, struct SSectorTextureSet * & pTerrainTexInfo, int & dwFObjFlags, const AABB & objBox);
};

#endif // CObjManager_H
