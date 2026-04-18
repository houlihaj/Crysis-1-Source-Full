////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_sector.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef SECINFO_H
#define SECINFO_H

#define MML_NOT_SET ((uchar)-1)

#define ARR_TEX_OFFSETS_SIZE 4
#define MAX_TERRAIN_SURFACE_TYPES 16
#define MAX_SURFACE_TYPES_COUNT 32

#include "BasicArea.h"
#include "Array2d.h"

enum eTexureType
{
	ett_Diffuse,
	ett_LM
};

struct SSurfaceTypeInfo
{
	SSurfaceTypeInfo() { memset(this, 0, sizeof(SSurfaceTypeInfo)); }
	struct SSurfaceType * pSurfaceType;
	IRenderMesh * arrpRM[6];

	bool HasRM() { return arrpRM[0] || arrpRM[1] || arrpRM[2] || arrpRM[3] || arrpRM[4] || arrpRM[5]; }
	int GetIndexCount();
	void DeleteRenderMeshes(IRenderer * pRend);
};

struct SRangeInfo 
{
  SRangeInfo()
  {
    nModified = false;
  }

	void UpdateBitShift(int nUnitToSectorBS)
	{
		const int nSecSize = 1<<nUnitToSectorBS;
		int n = nSize-1;
		nUnitBitShift = 0;
		while(n>0 && nSecSize>n) 
		{ 
			nUnitBitShift++; 
			n*=2; 
		}
		
		if(nSize>1)
			fInvStep = 1.f/(nSecSize/(nSize-1));	
		else
			fInvStep = 1.f;
	}

	inline ushort GetDataLocal( int nX, int nY ) const
	{
		assert(nX>=0 && nX<(int)nSize);
		assert(nY>=0 && nY<(int)nSize);
		assert(pHMData);
		return pHMData[nX*nSize + nY];
	}

  inline void SetDataLocal( int nX, int nY, ushort usValue )
  {
    assert(nX>=0 && nX<(int)nSize);
    assert(nY>=0 && nY<(int)nSize);
    assert(pHMData);
    pHMData[nX*nSize + nY] = usValue;
  }

	inline ushort GetDataUnits( int nX_units, int nY_units ) const
	{
		int nMask = nSize-2;
		int nX = nX_units >> nUnitBitShift;
		int nY = nY_units >> nUnitBitShift;
		return GetDataLocal(nX&nMask, nY&nMask);
	}

	float fOffset;
	float fRange;
	ushort * pHMData;
	ushort nSize;
	uchar nUnitBitShift;
  uchar nModified;
	float fInvStep;
};

template <class T, int nPoolSize> class TPool
{
public:

	TPool()
	{
		m_pPool = new T[nPoolSize];
		m_lstFree.PreAllocate(nPoolSize,0);
		m_lstUsed.PreAllocate(nPoolSize,0);
		for(int i=0; i<nPoolSize; i++)
			m_lstFree.Add(&m_pPool[i]);
	}

	~TPool()
	{
		delete [] m_pPool;
	}

	void ReleaseObject(T * pInst)
	{
		if(m_lstUsed.Delete(pInst))
			m_lstFree.Add(pInst);
	}

	int GetUsedInstancesCount(int & nAll)
	{
		nAll = nPoolSize;
		return m_lstUsed.Count();
	}

	T * GetObject() 
	{
		T * pInst = NULL;
		if(m_lstFree.Count())
		{
			pInst = m_lstFree.Last();
			m_lstFree.DeleteLast();
			m_lstUsed.Add(pInst);
		}
		else
			assert("TPool::GetObject: Out of free elements error");

		return pInst;
	}

	void GetMemoryUsage(class ICrySizer * pSizer)
	{
    pSizer->AddObject(this, sizeof(*this)+m_lstFree.GetDataSize()+m_lstUsed.GetDataSize());

    if(m_pPool)
      for(int i=0; i<nPoolSize; i++)
	  	  m_pPool[i].GetMemoryUsage(pSizer);
	}

	PodArray<T*> m_lstFree;
	PodArray<T*> m_lstUsed;
	T * m_pPool;
};

#define MAX_PROC_OBJ_IN_CHUNK 1024
#define MAX_PROC_OBJ_CHUNKS_NUM 128
#define MAX_PROC_OBJ_SECTORS_IN_CACHE 16

struct SProcObjChunk
{
	CVegetation * m_pInstances;
	SProcObjChunk();
	~SProcObjChunk();
  void GetMemoryUsage(class ICrySizer * pSizer);
};

typedef TPool<SProcObjChunk,MAX_PROC_OBJ_CHUNKS_NUM> SProcObjChunkPool;

class CProcObjSector : public Cry3DEngineBase
{
public:
	CProcObjSector() { m_nProcVegetNum = 0; m_ProcVegetChunks.PreAllocate(32); }
	~CProcObjSector();
	CVegetation * AllocateProcObject();
	void ReleaseAllObjects();
	int GetUsedInstancesCount(int & nAll) { nAll = m_ProcVegetChunks.Count(); return m_nProcVegetNum; }
	void GetMemoryUsage(ICrySizer*pSizer);

protected:
	PodArray<SProcObjChunk*> m_ProcVegetChunks;
	int m_nProcVegetNum;
};

typedef TPool<CProcObjSector, MAX_PROC_OBJ_SECTORS_IN_CACHE> CProcVegetPoolMan;

struct CTerrainNode : public Cry3DEngineBase, public IShadowCaster, public IStreamCallback
{
public:

	friend class CTerrain;

	virtual bool Render(const SRendParams &RendParams);
  virtual const AABB GetBBoxVirtual() { return m_boxHeigtmap; }
  virtual struct ICharacterInstance* GetEntityCharacter( unsigned int nSlot, Matrix34 * pMatrix = NULL, bool bReturnOnlyVisible = false ) {return NULL;};
	
	// streaming
	virtual void StreamOnComplete (IReadStream* pStream, unsigned nError);	
	void StartSectorTexturesStreaming(bool bFinishNow);
	IReadStreamPtr m_pReadStream;
	EFileStreamingStatus m_eTexStreamingStatus;

	CTerrainNode(int x1, int y1, int nNodeSize, CTerrainNode * pParent, bool bBuildErrorsTable);
	~CTerrainNode();
	bool CheckVis(bool bAllIN, bool bAllowRenderIntoCBuffer);
	void SetupTexturing(bool bMakeUncompressedForEditing = false);
  void RequestTextures();
	void SetSectorTexture(unsigned int textureId);
	void CheckNodeGeomUnload();
	IRenderMesh * MakeSubAreaRenderMesh(const Vec3 & vPos, float fRadius, IRenderMesh * pPrevRenderMesh, IMaterial * pMaterial, bool bRecalIRenderMeshconst, const char * szLSourceName);
	void SetChildsLod(int nNewGeomLOD);
  int GetAreaLOD();
	void RenderNodeHeightmap();
	bool CheckUpdateProcObjects();
	void IntersectTerrainAABB(const AABB & aabbBox, PodArray<CTerrainNode*> & lstResult);
	void UpdateDetailLayersInfo(bool bRecursive);
	void RemoveProcObjects(bool bRecursive = false);
	void IntersectWithShadowFrustum(PodArray<CTerrainNode*> * plstResult, ShadowMapFrustum * pFrustum, const float fHalfGSMBoxSize);
	void IntersectWithBox(const AABB & aabbBox, PodArray<CTerrainNode*> * plstResult);
  void GetTerrainAOTextureNodesInBox(const AABB & aabbBox, PodArray<CTerrainNode*> * plstResult);
	CTerrainNode * FindMinNodeContainingBox(const AABB & aabbBox);
	void RenderSector(int nTechniqueID);
	CTerrainNode * GetTexuringSourceNode(int nTexMML, eTexureType eTexType);
	CTerrainNode * GetReadyTexSourceNode(int nTexMML, eTexureType eTexType);
	int GetData(byte * & pData, int & nDataSize);

  template<class T> 
  int Load_T(T * & f, int & nDataSize);
  int Load(uchar * & f, int & nDataSize);
  int Load(FILE * & f, int & nDataSize);
  int ReloadModifiedHMData(FILE * f);

	void UnloadNodeTexture(bool bRecursive);
	float GetSurfaceTypeAmount(Vec3 vPos, int nSurfType);
	void GetMemoryUsage(ICrySizer*pSizer);
//	void GetProcVegetMemoryUsage(ICrySizer*pSizer);

	void SetLOD();
	uchar GetTextureLOD(float fDistance);

//	void SetHeightmapAABBAndHoleFlag();

	void ReleaseHeightMapGeometry(bool bRecursive = false);
	int GetSecIndex();

	void DrawArray(int nTechniqueID);
	void UpdateRenderMesh(struct CStripsInfo * pArrayInfo, bool bUpdateVertices);
	void BuildVertices(int step, bool bSafetyBorder);  
	int  GetMML(int dist, int mmMin, int mmMax);

	PodArray<CDLight*> * GetAffectingLights();
	void AddLightSource(CDLight * pSource);
  void CheckInitAffectingLights();

	void GenerateIndicesForQuad(IRenderMesh * pRM, Vec3 vBoxMin, Vec3 vBoxMax, PodArray<ushort> & dstIndices);

	uint GetLastTimeUsed() { return m_nLastTimeUsed; }

	void AddIndexAliased(int _x, int _y, int _step,  int nSectorSize, PodArray<CTerrainNode*> * plstNeighbourSectors, CStripsInfo * pArrayInfo);
	static void GenerateIndicesForAllSurfaces(IRenderMesh * pRM);
	void BuildIndices(CStripsInfo & si, PodArray<CTerrainNode*> * pNeighbourSectors, bool bSafetyBorder);

	static void UpdateSurfaceRenderMeshes(IRenderMesh * pSrcRM, struct SSurfaceType * pSurface, IRenderMesh * & pMatRM, int nProjectionId, PodArray<unsigned short> & lstIndices, const char * szComment);
	void SetupTexGens();
	static void SetupTexGenParams(SSurfaceType * pLayer, float * pOutParams, IMaterial::EProjAxis projAxis, bool bOutdoor, float fTexGenScale = 1.f);

	int CreateSectorTexturesFromBuffer();
		
	bool CheckUpdateDiffuseMap();
	bool AssignTextureFileOffset(int16 * &pIndices, int16 & nElementsNum);
	static CProcVegetPoolMan * GetProcObjPoolMan() { return m_pProcObjPoolMan; }
	static SProcObjChunkPool * GetProcObjChunkPool() { return m_pProcObjChunkPool; }
	static void SetProcObjPoolMan(CProcVegetPoolMan * pProcObjPoolMan) { m_pProcObjPoolMan = pProcObjPoolMan; }
	static void SetProcObjChunkPool(SProcObjChunkPool * pProcObjChunkPool) { m_pProcObjChunkPool = pProcObjChunkPool; }
	void UpdateDistance();
	bool IsProcObjectsReady() { return m_bProcObjectsReady; }
  void UpdateRangeInfoShift();
  
	// member variables
	CTerrainNode * m_arrChilds[4];

	// flags
	bool m_bProcObjectsReady : 1;
	bool m_bMergeNotAllowed : 1;
	bool m_bHasHoles : 1; // sector has holes in the ground
	bool m_bHasLinkedVoxel: 1; // for editor
	bool m_bNoOcclusion : 1; // sector has visareas under terrain surface

	uint m_nEditorDiffuseTex; // for editor

  ushort m_nOriginX, m_nOriginY; // sector origin
  int m_nLastTimeUsed; // basically last time rendered
  int m_nSetLodFrameId;
	OcclusionTestClient m_OcclusionTestClient;

	uchar // LOD's
		m_cNewGeomMML, m_cCurrGeomMML, 
		m_cNewGeomMML_Min, m_cNewGeomMML_Max,
		m_cNodeNewTexMML, m_cNodeNewTexMML_Min; 

protected:

  float * m_pGeomErrors; // errors for each lod level

//	void LoadLayer1LightmapData(STerrainTextureLayerFileHeader *pLayers, uchar *pLightMapData, Array2d<uchar>& rLightMap);

public:

  PodArray<CDLight*> m_lstAffectingLights; uint m_nLightMaskFrameId;

	PodArray<SSurfaceTypeInfo> m_lstSurfaceTypeInfo;

	SRangeInfo m_rangeInfo;

  IRenderMesh * m_pCBRenderMesh;

	struct STerrainNodeLeafData
	{
		STerrainNodeLeafData() { memset(this,0,sizeof(*this)); }
		~STerrainNodeLeafData();
		float m_arrTexGen[MAX_RECURSION_LEVELS][ARR_TEX_OFFSETS_SIZE];
		PodArray<CTerrainNode*> m_lstNeighbourSectors;
		PodArray<int> m_lstNeighbourLods;
		IRenderMesh * m_pRenderMesh;
	} * m_pLeafData;

	CProcObjSector * m_pProcObjPoolPtr;

	void CheckLeafData()
	{ 
		if(!m_pLeafData)
			m_pLeafData = new STerrainNodeLeafData; 
	}

	inline STerrainNodeLeafData * GetLeafData()
	{ 
		return m_pLeafData;
	}

  int GetSectorSizeInHeightmapUnits();

  int m_nTreeLevel;
  SSectorTextureSet m_nNodeTexSet, m_nTexSet; // texture id's

	struct STerrainLightInfo 
	{ 
		uchar r,g,b,a; 
	};

  int m_nNodeTextureLastUsedSec;
	int m_nNodeRenderLastFrameId;
	AABB m_boxHeigtmap;
	struct CTerrainNode * m_pParent;
	int m_nSetupTexGensFrameId;
  int m_nGSMFrameId;

	float m_arrfDistance[MAX_RECURSION_LEVELS];
	int m_nNodeTextureOffset;
  int m_nNodeHMDataOffset;
  int FTell(uchar * & f);
  int FTell(FILE * & f);

	static PodArray<ushort> m_arrIndices[MAX_SURFACE_TYPES_COUNT][7];
	static CProcVegetPoolMan * m_pProcObjPoolMan;
	static SProcObjChunkPool * m_pProcObjChunkPool;
};

#pragma pack(push,1)

struct STerrainNodeChunk
{
	int		nChunkVersion;
	AABB	boxHeightmap;
	bool	bHasHoles;
	float fOffset;
	float fRange;
	int		nSize;
	int		nSurfaceTypesNum;

	AUTO_STRUCT_INFO
};

#pragma pack(pop)

#endif SECINFO_H
