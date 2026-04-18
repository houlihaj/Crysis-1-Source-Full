////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef TERRAIN_H
#define TERRAIN_H

#include "TerrainModifications.h"						// CTerrainModifications

// lowest level of the outdoor world
#define TERRAIN_BOTTOM_LEVEL 0

// hightmap contain some additional info in last 4 bits
#define STYPE_BIT_MASK     (1|2|4|8)    // surface type id

#define STYPE_HOLE STYPE_BIT_MASK  // this surface type is reserved for hole in terrain
#define MAX_SURFACE_TYPES_COUNT 32  // max number of surfaces excluding hole type
#define TERRAIN_BASE_TEXTURES_NUM 2

// max view distance for objects shadow is size of object multiplied by this number
#define OBJ_MAX_SHADOW_VIEW_DISTANCE_RATIO 4

#define CHAR_TO_FLOAT 0.01f

#define TERRAIN_NODE_TREE_DEPTH 16

#define OCEAN_IS_VERY_FAR_AWAY 1000000.f

#define ARR_TEX_OFFSETS_SIZE_DET_MAT 16

const int nHMCacheSize = 32;

// Heightmap data
class CHeightMap : public Cry3DEngineBase
{
public:
  // Access to heightmap data
  float GetZSafe(int x, int y);
	float GetZ(int x, int y);
  void SetZ(const int x, const int y, float fHeight);
  float GetZfromUnits(uint nX_units, uint nY_units);
  void SetZfromUnits(uint nX_units, uint nY_units, float fHeight);
	float GetZMaxFromUnits(uint nX0_units, uint nY0_units, uint nX1_units, uint nY1_units);
	uchar GetSurfTypefromUnits(uint nX_units, uint nY_units);
	static float GetHeightFromUnits_Callback(int ix, int iy);
	static unsigned char GetSurfaceTypeFromUnits_Callback(int ix, int iy);
	bool IsPointUnderGround(uint nX_units, uint nY_units, float fTestZ);

  uchar GetSurfaceTypeID(int x, int y);
  float GetZApr(float x1, float y1, bool bIncludeVoxels = false);
	float GetZMax(float x0, float y0, float x1, float y1);
	bool GetHole(int x, int y);
  bool IntersectWithHeightMap(Vec3 vStartPoint, Vec3 vStopPoint, float fDist, int nMaxTestsToScip);
  bool Intersect(Vec3 vStartPoint, Vec3 vStopPoint, float fDist, int nMaxTestsToScip, Vec3 & vLastVisPoint);
  bool IsBoxOccluded( const AABB & objBox, float fDistance, bool bTerrainNode, OcclusionTestClient * pOcclTestVars );

	// Exact test.
	struct SRayTrace
	{
		float	fInterp;
		Vec3	vHit;
		Vec3	vNorm;
		IMaterial*	pMaterial;

		SRayTrace() {}
		SRayTrace(float fT, Vec3 const& vH, Vec3 const& vN, IMaterial* pM)
			: fInterp(fT), vHit(vH), vNorm(vN), pMaterial(pM)
		{}
	};
  bool RayTrace(Vec3 const& vStart, Vec3 const& vEnd, SRayTrace* prt = 0);

	CHeightMap()
	{
		m_nUnitsToSectorBitShift = m_nBitShift = 0;
		m_fHeightmapZRatio = 0;
		m_bHeightMapModified = false;
    ResetHeightMapCache();
	}

  void ResetHeightMapCache()
  {
    memset(m_arrCacheHeight,0,sizeof(m_arrCacheHeight));
    assert(sizeof(m_arrCacheHeight[0][0]) == 8);
    memset(m_arrCacheSurfType,0,sizeof(m_arrCacheSurfType));
    assert(sizeof(m_arrCacheSurfType[0][0]) == 4);
  }

	int				m_nBitShift;
	int				m_nUnitsToSectorBitShift;
	bool			m_bHeightMapModified;
	float			m_fHeightmapZRatio;

protected:

  struct SCachedHeight
  {
    SCachedHeight() { x=y=0; fHeight=0; } 
    ushort x,y;
    float fHeight;
  };

  struct SCachedSurfType
  {
    SCachedSurfType() { x = y = surfType = 0; } 
    uint x : 14;
    uint y : 14;
    uint surfType : 4;
  };

  static SCachedHeight m_arrCacheHeight[nHMCacheSize][nHMCacheSize];
  static SCachedSurfType m_arrCacheSurfType[nHMCacheSize][nHMCacheSize];
};

struct SSurfaceType
{
  SSurfaceType() 
  { memset(this, 0,sizeof(SSurfaceType)); ucThisSurfaceTypeId=255; }

	bool IsMaterial3D() { return pLayerMat && pLayerMat->GetSubMtlCount()>=3; }

	bool HasMaterial() { return pLayerMat != NULL; }

	IMaterial * GetMaterialOfProjection(const IMaterial::EProjAxis projAxis)
	{
		if (pLayerMat)
		{
			if(pLayerMat->GetSubMtlCount() >= 3)
			{
				for(int i=0; i<pLayerMat->GetSubMtlCount(); i++)
					if(pLayerMat->GetSubMtl(i)->m_defautMappingAxis == projAxis)
						return pLayerMat->GetSubMtl(i);
			}
			else if(projAxis == defProjAxis)
				return pLayerMat;

			//assert(!"SSurfaceType::GetMaterialOfProjection: Material not found");
		}

		return NULL;
	}

	float GetMaxMaterialDistanceOfProjection(const IMaterial::EProjAxis projAxis)
	{
		if(projAxis == IMaterial::ePA_XPos || projAxis == IMaterial::ePA_XNeg || projAxis == IMaterial::ePA_YPos || projAxis == IMaterial::ePA_YNeg)
			return fMaxMatDistanceXY;

		return fMaxMatDistanceZ;
	}

	char szName[64];
	_smart_ptr<IMaterial> pLayerMat;
	_smart_ptr<IRenderMesh> m_pProcObjectsTile;
  float fScale;
	PodArray<int> lstnVegetationGroups;
	float fMaxMatDistanceXY;
	float fMaxMatDistanceZ;
	float arrRECustomData[7][ARR_TEX_OFFSETS_SIZE_DET_MAT];
	IMaterial::EProjAxis defProjAxis;
	uchar ucThisSurfaceTypeId;
};

struct CTerrainNode;


#define TERRAIN_TEX_CACHE_POOL_SIZE 128

struct CTextureCache : public Cry3DEngineBase
{
	PodArray<uint> m_FreeTextures;
	PodArray<uint> m_UsedTextures;
	PodArray<uint> m_Quarantine;
	ETEX_Format m_eTexFormat;
	int m_nDim;

	CTextureCache();
	~CTextureCache();
	uint GetTexture(byte * pData);
	void ReleaseTexture(uint nTexId);
	bool Update();
	void InitPool(byte * pData, int nDim, ETEX_Format eTexFormat);
	void ResetTexturePool();
	int GetPoolSize();
};

struct CStripInfo {int begin,end;};

struct CStripsInfo 
{ 
  PodArray<CStripInfo> strip_info; PodArray<unsigned short> idx_array; bool bInStrip;
  inline void Clear() { strip_info.Clear(); idx_array.Clear(); bInStrip = false; } 
  inline void Reset() { strip_info.Reset(); idx_array.Reset(); bInStrip = false; } 
  inline void AddIndex(int _x, int _y, int _step,  int nSectorSize)
  {
    unsigned short id = _x/_step*(nSectorSize/_step+1) + _y/_step;

    int nCount = idx_array.Count();

    if(nCount-(strip_info.Last().begin) < 3)
    {
      idx_array.Add(id);
    }
    else
    {
      unsigned short idm2 = idx_array[nCount-2];
      unsigned short idm1 = idx_array[nCount-1];
      idx_array.Add(idm2);
      idx_array.Add(idm1);
      idx_array.Add(id);
    }
  }

  static ushort GetIndex(int _x, int _y, int _step,  int nSectorSize)
  {
    unsigned short id = _x/_step*(nSectorSize/_step+1) + _y/_step;
    return id;
  }

  inline void BeginStrip()
  {
    assert(!bInStrip);
    CStripInfo si;
    si.begin = idx_array.Count();
    si.end   = 0;
    strip_info.Add(si);
    bInStrip = true;
  }

  inline void EndStrip()
  {
    assert(bInStrip);
    assert(strip_info.Count()); 
    strip_info.Last().end = idx_array.Count();
    bInStrip = false;
  }

  void GetMemoryUsage(ICrySizer* pSizer)
  {
    pSizer->AddContainer (strip_info);
    pSizer->AddContainer (idx_array);
  }
};

// The Terrain Class
class CTerrain : public ITerrain, public CHeightMap
{
	friend struct CTerrainNode;

public:

  CTerrain( const STerrainInfo & TerrainInfo );
  ~CTerrain();

	void InitHeightfieldPhysics();
	ILINE static const int GetTerrainSize()					{ return m_nTerrainSize; }
	ILINE static const int GetSectorSize()					{ return m_nSectorSize; }
	ILINE static const int GetHeightMapUnitSize()		{ return m_nUnitSize; }
	ILINE static const int GetSectorsTableSize()		{ return m_nSectorsTableSize; }
	ILINE static const float GetInvUnitSize()				{ return m_fInvUnitSize; }

	ILINE const int GetTerrainUnits()								{ return m_nTerrainSize>>m_nBitShift; }
	ILINE void ClampUnits(uint& xu, uint& yu)
	{
		xu = min(max((int)xu, 0), GetTerrainUnits()-1);
		yu = min(max((int)yu, 0), GetTerrainUnits()-1);
	}
	ILINE CTerrainNode* GetSecInfoUnits(int xu, int yu)
	{
		return m_arrSecInfoPyramid[0][xu>>m_nUnitsToSectorBitShift][yu>>m_nUnitsToSectorBitShift];
	}
	ILINE CTerrainNode* GetSecInfo(int x, int y)
	{
		if (x < 0 || y < 0 || x >= m_nTerrainSize || y >= m_nTerrainSize)
			return 0;
		return GetSecInfoUnits(x >> m_nBitShift, y >> m_nBitShift);
	}
	ILINE CTerrainNode * GetSecInfo(const Vec3 & pos)
	{
		return GetSecInfo((int)pos.x, (int)pos.y);
	}
	float GetWaterLevel() { return m_fOceanWaterLevel;/* ? m_fOceanWaterLevel : WATER_LEVEL_UNKNOWN;*/ }
	void SetWaterLevel(float fOceanWaterLevel) { m_fOceanWaterLevel = fOceanWaterLevel; }

	//////////////////////////////////////////////////////////////////////////
	// ITerrain Implementation.
	//////////////////////////////////////////////////////////////////////////

  template <class T>
  bool Load_T(T * & f, int & nDataSize, STerrainChunkHeader * pTerrainChunkHeader, std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable);

  virtual bool GetCompiledData(byte * pData, int nDataSize, std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable);
	virtual int  GetCompiledDataSize();
	virtual bool Compile();

	virtual IRenderNode* AddVegetationInstance( int nStaticGroupID,const Vec3 &vPos,const float fScale,uchar ucBright,uchar angle );
	virtual void SetOceanWaterLevel( float fOceanWaterLevel );

	//////////////////////////////////////////////////////////////////////////

	void RemoveAllStaticObjects();
	void ApplyForceToEnvironment(Vec3 vPos, float fRadius, float fAmountOfForce);
	void CheckUnload();
	void CloseTerrainTextureFile();
	void DrawVisibleSectors();
  void RenderAOSectors();
	void MakeCrater(Vec3 vExploPos, float fExploRadius);
	bool RemoveObjectsInArea(Vec3 vExploPos, float fExploRadius);
	float GetDistanceToSectorWithWater() { return m_fDistanceToSectorWithWater; }
	void GetMemoryUsage(class ICrySizer*pSizer);
	void GetObjects(PodArray<struct SRNInfo> * pLstObjects);
	void GetObjectsAround(Vec3 vPos, float fRadius, PodArray<struct SRNInfo> * pEntList, bool bSkip_ERF_NO_DECALNODE_DECALS = false, bool bSkipDynamicObjects = false);
	class COcean * GetOcean() { return m_pOcean; }
	Vec3 GetTerrainSurfaceNormal(Vec3 vPos, float fRange, bool bIncludeVoxels = false);
	int GetActiveTextureNodesCount() { return m_lstActiveTextureNodes.Count(); }
	int GetActiveProcObjNodesCount() { return m_lstActiveProcObjNodes.Count(); }
	int GetNotReadyTextureNodesCount();
	void GetTextureCachesStatus(int & nCount0, int & nCount1) 
	{ nCount0 = m_texCache[0].GetPoolSize(); nCount1 = m_texCache[1].GetPoolSize(); }

	bool PreloadResources();
	void CheckVis( );
	int RenderTerrainWater();
	void UpdateNodesIncrementaly();
  void CheckNodesGeomUnload();
	void GetStreamingStatus(int & nLoadedSectors, int & nTotalSectors);
	void InitTerrainWater(IMaterial * pTerrainWaterMat, int nWaterBottomTexId);
	void ResetTerrainVertBuffers();
	void SetTerrainSectorTexture( const int nTexSectorX, const int nTexSectorY, unsigned int textureId );
	static void SetDetailLayerProperties(int nId, float fScaleX, float fScaleY, IMaterial::EProjAxis projAxis, const char * szSurfName, const PodArray<int> & lstnVegetationGroups, IMaterial * pMat);
	bool IsOceanVisible() { return m_bOceanIsVisibe; }
	void SetTerrainElevation(int x1, int y1, int nSizeX, int nSizeY, float * pTerrainBlock, unsigned char * pSurfaceData, uint32 * pResolMap, int nResolMapSizeX, int nResolMapSizeY);
	void GetVisibleSectorsInAABB(PodArray<struct CTerrainNode*> & lstBoxSectors, const AABB & boxBox);
	void RegisterLightMaskInSectors(CDLight * pLight);
	static void LoadSurfaceTypesFromXML(XmlNodeRef pDoc);
	void UpdateSurfaceTypes();
	bool RenderArea(Vec3 vPos, float fRadius, 
		IRenderMesh ** arrLightRenderMeshs, CRenderObject * pObj, IMaterial * pMaterial, 
		const char * szComment, float * pCustomData, Plane * planes);
	void IntersectWithShadowFrustum(PodArray<CTerrainNode*> * plstResult, ShadowMapFrustum * pFrustum);
	void IntersectWithBox(const AABB & aabbBox, PodArray<CTerrainNode*> * plstResult);
	void MarkAllSectorsAsUncompiled();
	void BuildErrorsTableForArea(float * pLodErrors, int nMaxLods, int X1, int Y1, int X2, int Y2, float * pTerrainBlock,
		uchar * pSurfaceData, int nSurfOffsetX, int nSurfOffsetY, int nSurfSizeX, int nSurfSizeY);

protected:

  void AddVisSector(CTerrainNode * newsec);
 
  CTerrainNode * m_pParentNode;

	void BuildSectorsTree(bool bBuildErrorsTable);
	int GetTerrainNodesAmount();
	bool OpenTerrainTextureFile(SCommonFileHeader &hdrDiffTexHdr, STerrainTextureFileHeader & hdrDiffTexInfo, const char * szFileName, uchar *  & ucpDiffTexTmpBuffer, int & nDiffTexIndexTableSize);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////////////////////////

public:

	void DoVoxelShape(Vec3 vWSPos, float fRadius, int nSurfaceTypeId, Vec3 vBaseColor, EVoxelEditOperation eOperation, EVoxelBrushShape eShape, EVoxelEditTarget eTarget, PodArray<class CVoxelObject*> * pAffectedVoxAreas);
	bool Voxel_Recompile_Modified_Incrementaly_Objects();
	bool Voxel_FindNeighboursForObject(class CVoxelObject * pThisObject, class CVoxelObject ** arrNeighbours);
	void Voxel_SetFlags(bool bPhysics, bool bSimplify, bool bShadows, bool bMaterials);
	void BuildVoxelSpace();

  bool Recompile_Modified_Incrementaly_RoadRenderNodes();

	virtual void SerializeTerrainState( TSerialize ser );

	void SetHeightMapMaxHeight(float fMaxHeight);

	SSurfaceType * GetSurfaceTypePtr(int x, int y)
	{
		int nType = GetSurfaceTypeID(x,y);
		assert(nType>=0 && nType<MAX_SURFACE_TYPES_COUNT);
		return (nType!=STYPE_HOLE) ? &m_SSurfaceType[nType] : NULL;
	}

	SSurfaceType * GetSurfaceTypes() { return &m_SSurfaceType[0]; }

  int GetTerrainTextureNodeSizeMeters() { return CTerrain::GetSectorSize(); }

	int GetTerrainTextureNodeSizePixels(int nLayer)
	{
		assert(nLayer>=0 && nLayer<2);
		return m_TerrainTextureLayer[nLayer].nSectorSizePixels;
	}

	void GetMaterials(IMaterial * & pTerrainEf)
	{
		pTerrainEf = m_pTerrainEf;
	}

	void SetSunLightMask(uint nSunLightMask) { m_nSunLightMask = nSunLightMask; }
	uint GetSunLightMask() { return m_nSunLightMask; }

	int m_nWhiteTexId;
	int m_nDefTerrainTexId;
	int m_nBlackTexId;

	Array2d<struct CTerrainNode*> m_arrSecInfoPyramid[TERRAIN_NODE_TREE_DEPTH];

  CStripsInfo									m_StripsInfo;										//

	float GetSunShadowIntensity() const { return m_hdrDiffTexInfo.m_fSunShadowIntensity; }
//	void ProcessPanorama();
//	void ProcessPanoramaCluster(CREPanoramaCluster * pImp, const Vec3 & vCamDir, float fFOV, bool bCheckUpdate);
	
	void ActivateNodeTexture( CTerrainNode * pNode );
	void ActivateNodeProcObj( CTerrainNode * pNode );
	CTerrainNode * FindMinNodeContainingBox(const AABB & someBox);
	int GetTerrainLightmapTexId( Vec4 & vTexGenInfo );

	IRenderMesh * MakeAreaRenderMesh(	const Vec3 & vPos, float fRadius, 
		IMaterial * pMat, const char * szLSourceName, Plane * planes);

	bool IsAmbientOcclusionEnabled();

  template <class T> static bool LoadDataFromFile(T *data, size_t elems, FILE * & f, int & nDataSize)
  {
    if(GetPak()->FRead(data, elems, f) != elems)
    {
      assert(0);
      return false;
    }
    nDataSize -= sizeof(T)*elems;
    return true;
  }

  template <class T> static bool LoadDataFromFile(T *data, size_t elems, uchar * & f, int & nDataSize)
  {
    StepDataCopy(data, f, &nDataSize, elems);
    return true;
  }

  int ReloadModifiedHMData(FILE * f);

protected: // ------------------------------------------------------------------------

	CTerrainModifications				m_StoredModifications;					// to serialize (load/save) terrain heighmap changes and limit the modification
	int													m_nLoadedSectors;								//
	bool												m_bOceanIsVisibe;								//

	float												m_fDistanceToSectorWithWater;		//
	
	struct {
		int m_nDiffTexIndexTableSize;
		SCommonFileHeader m_hdrDiffTexHdr;
		STerrainTextureFileHeader m_hdrDiffTexInfo;
		STerrainTextureLayerFileHeader m_TerrainTextureLayer[2];
		uchar * m_ucpDiffTexTmpBuffer;
	};

	PodArray<struct_VERTEX_FORMAT_P3F_N4B_COL4UB> m_lstTmpVertArray;

	IMaterial * m_pTerrainEf, *m_pImposterEf;

	float												m_fOceanWaterLevel;
	uint												m_nSunLightMask;

	PodArray<struct CTerrainNode*> m_lstVisSectors;

	static SSurfaceType					m_SSurfaceType[MAX_SURFACE_TYPES_COUNT];

	static int						m_nUnitSize;							// in meters
	static float					m_fInvUnitSize;						// in 1/meters
	static int						m_nTerrainSize; 					// in meters
	static int						m_nSectorSize;						// in meters
	static int						m_nSectorsTableSize; 			// sector width/height of the finest LOD level (sector count is the square of this value)

	class COcean *				m_pOcean;

	PodArray<CTerrainNode*> m_lstActiveTextureNodes;
	PodArray<CTerrainNode*> m_lstActiveProcObjNodes;

	CTextureCache m_texCache[2];

public:
  bool SetCompiledData(byte * pData, int nDataSize, std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable);
  bool Load(FILE * f, int nDataSize, struct STerrainChunkHeader * pTerrainChunkHeader, std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable);
};

#endif // TERRAIN_H

