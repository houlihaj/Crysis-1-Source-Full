////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   voxman.h
//  Version:     v1.00
//  Created:     28/5/2004 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef VOX_MAN_H
#define VOX_MAN_H


#define TERRAIN_VOX_LAYERS_NUM 8
#define DEF_VOX_VOLUME_SIZE 32 // units
#define DEF_VOX_UNIT_SIZE 2.f // meters
#define VOX_MAX_LODS_NUM 3
#define VOX_MAX_SURF_TYPES_NUM 16
#define VOX_ISO_LEVEL (256*256/2)
#define VOX_MAT_MASK STYPE_BIT_MASK
#define VOX_MAX_USHORT (256*256-1)
const int VOX_HASH_SIZE = 16;
const float VOX_HASH_SCALE = 0.125f;

#if !defined(ARR_TEX_OFFSETS_SIZE_DET_MAT)
#define ARR_TEX_OFFSETS_SIZE_DET_MAT 16
#endif

#pragma pack(push,1)

struct SVoxelChunkVer3
{
	int nChunkVersion;
	Vec3i vOrigin, vSize;
	uint nFlags;
	ushort m_arrVolume[DEF_VOX_VOLUME_SIZE][DEF_VOX_VOLUME_SIZE][DEF_VOX_VOLUME_SIZE];
	char m_arrSurfaceNames[VOX_MAX_SURF_TYPES_NUM][64];
	ColorB m_arrColors[DEF_VOX_VOLUME_SIZE][DEF_VOX_VOLUME_SIZE][DEF_VOX_VOLUME_SIZE];

	AUTO_STRUCT_INFO
};

struct SVoxelChunkVer4
{
  int nChunkVersion;
  Vec3i vOrigin, vSize;
  uint nFlags;
  char m_arrSurfaceNames[VOX_MAX_SURF_TYPES_NUM][64];

  AUTO_STRUCT_INFO
};

#pragma pack(pop)

// structures used during compiling
typedef struct {
	Vec3 p[3];
	int	arrMatId[3];
	ColorB arrColor[3];
	Vec3 vNormal;
	bool bRemove;
} TRIANGLE;

typedef struct {
	Vec3 p[8];
	float val[8];
	int arrMatId[8];
	ColorB arrColor[8];
} GRIDCELL;

#include "ObjMan.h"

// this class contain one ambient pass and several light pass RenderMesh's
class CVoxelMesh : public Cry3DEngineBase
{
	SSurfaceTypeInfo m_arrSurfaceTypeInfo[VOX_MAX_LODS_NUM][VOX_MAX_SURF_TYPES_NUM];
	IRenderMesh * m_arrpRM_Ambient[VOX_MAX_LODS_NUM];
	PodArray<ushort> m_arrVertHash[VOX_HASH_SIZE][VOX_HASH_SIZE];
  int m_arrCurrAoRadiusAndScale[VOX_MAX_LODS_NUM];

public:
	CVoxelMesh(SSurfaceType ** ppSurfacesPalette);
	~CVoxelMesh();

	// render
	void RenderAmbPass(int nLod, int nSort, int nAW, CRenderObject * pObj, int nTechId, const SRendParams & rParams);
	void RenderLightPasses(int nLod, int nDLMask, class CVoxelObject * pVoxArea, bool bInShadow, float fDistance, const SRendParams & rParams);
	void SetupAmbPassMapping(int nElemCount, float * pHMTexOffsets, const struct SSectorTextureSet & texSet, bool bSetupSinglePassTexIds);

	// create
	void ResetRenderMeshs();
	IRenderMesh * CreateRenderMeshFromIndexedMesh(CMesh * pMaterialMesh, CVoxelObject * pVoxArea);
  void MakeRenderMeshsFromMemBlocks(CVoxelObject * pVoxArea);

	// access
	IRenderMesh * GetRenderMesh(int nLod) { return m_arrpRM_Ambient[nLod]; }

	int UpdateAmbientOcclusion(CVoxelObject ** pNeighbours, CVoxelObject * pThisArea, int nLod);
	void UpdateVertexHash(IRenderMesh * pRM);
	int GetAmbientOcclusionForPoint(Vec3 vPos, Vec3 vNorm, float & fResult, CVoxelObject * pThisArea);
  void CheckUpdateLighting(int nLod, CVoxelObject * pSrcArea);
  int GetMemoryUsage();
  void DeleteCContentCGF(CContentCGF * pObj);
};

template <class T> struct Array3d
{
	Array3d() 
	{
		m_pData = NULL;
		m_nSizeX=m_nSizeY=m_nSizeZ=0;
	} 

	~Array3d() 
	{
		delete [] m_pData;
	} 

	T * m_pData;
	int m_nSizeX,m_nSizeY,m_nSizeZ;

	T & GetAt(int x, int y, int z) 
	{ 
		int nPos = x*(m_nSizeY*m_nSizeZ) + y*m_nSizeZ + z;
		assert(nPos>=0 && nPos<GetDataSize());
		return m_pData[nPos]; 
	}

	T & GetAtClamped(int x, int y, int z) 
	{ 
		int nPos = 
			CLAMP(x, 0, m_nSizeX-1)*(m_nSizeY*m_nSizeZ) + 
			CLAMP(y, 0, m_nSizeY-1)*(m_nSizeZ) + 
			CLAMP(z, 0, m_nSizeZ-1); 

		assert(nPos>=0 && nPos<GetDataSize());
		return m_pData[nPos]; 
	}

	void Allocate(int nSizeX, int nSizeY, int nSizeZ) 
	{ 
		if(m_nSizeX != nSizeX || m_nSizeY != nSizeY || m_nSizeZ != nSizeZ )
		{
			delete [] m_pData;
			m_nSizeX = nSizeX;
			m_nSizeY = nSizeY;
			m_nSizeZ = nSizeZ; 
			m_pData = new T [m_nSizeX*m_nSizeY*m_nSizeZ];
		}

		memset(m_pData, 0, m_nSizeX*m_nSizeY*m_nSizeZ*sizeof(T));
	}

	void CopyFrom(Array3d & other) 
	{ 
    Allocate(other.m_nSizeX, other.m_nSizeY, other.m_nSizeZ);
		memcpy(m_pData, other.m_pData, GetDataSize());
	}

	void UpdateFrom(T * otherData, int nDataSize) 
	{ 
		assert(m_pData && nDataSize == GetDataSize());
		memcpy(m_pData, otherData, GetDataSize());
	}

	int GetDataSize() { return m_nSizeX*m_nSizeY*m_nSizeZ*sizeof(T); }
	T * GetElements() { return m_pData; }
};

class CVoxelVolume : public Cry3DEngineBase
{
public:

  CVoxelVolume(float fUnitSize, int nSizeXinUnits, int nSizeYinUnits, int nSizeZinUnits);

  // array of voxels of the sector
  Array3d<ushort> m_arrVolume;
  Array3d<ushort> m_arrVolumeBackup;
  Array3d<ColorB> m_arrColors;
  float m_fUnitSize;
  CVoxelObject * m_pSrcArea;
  bool m_bUpdateRequested;
  uint m_nUpdateRequestedFrameId;

  void SetVolume(ushort * pData, ColorB * pColors);
  void GetVolume(ushort * pData, ColorB * pColors);
  void RenderDebug(CVoxelObject ** arrNeighbours);
  bool DoVoxelShape(EVoxelEditOperation eOperation, Vec3 vPos, float fRadius, SSurfaceType * pLayer, Vec3 vBaseColor, EVoxelBrushShape eShape, CVoxelObject ** pNeighbours);
  void SubmitVoxelSpace();
  Vec3i GetVolumeSize() { return Vec3i(m_arrVolume.m_nSizeX,m_arrVolume.m_nSizeY,m_arrVolume.m_nSizeZ); }
  ushort GetVoxelValue(int x, int y, int z, CVoxelObject ** pNeighbours, int * pMatId, ColorB * pColor);
  CIndexedMesh * Compile(CVoxelObject ** pNeighbours);
  CIndexedMesh * MakeIndexedMesh(PodArray<TRIANGLE> & lstTris, CVoxelObject ** pNeighbours);
  void SimplifyIndexedMesh(CIndexedMesh * pIndexedMesh, int nLod);
  void GenerateTrianglesFromVoxels(CVoxelObject ** pNeighbours, int nLod, PodArray<TRIANGLE> & lstTris);
  void FillCellInfo(GRIDCELL & cell, int x, int y, int z, int v, CVoxelObject ** pNeighbours);
  int Polygonise(GRIDCELL grid,float isolevel,TRIANGLE *triangles);
  Vec3 VertexInterp(float isolevel, Vec3 p1, Vec3 p2, float valp1, float valp2, int nMat1, int nMat2, 
    int*pnMatId, const ColorB & col1, const ColorB & col2, ColorB * colOut);
  float GetVoxelValueInterpolated(float x, float y, float z, CVoxelObject ** pNeighbours, int * pMatId, ColorB * pColor, int nScale);
  int GenerateTrianglesForCell(Vec3i & vCell, CVoxelObject ** pNeighbours, int nLod, PodArray<TRIANGLE> & lstTris, int nStep);
  ushort GetLocalSurfaceId(SSurfaceType * pLayer);
  void InitVoxelsFromHeightMap(bool bOnlyMaterials);
  ushort GetHeightMapValue(const Vec3 vPos, ushort ucDefaultValue, bool bCheckForHole = false);
  ushort GetLessUsedSurfaceId();
  bool IsEmpty(CVoxelObject ** pNeighbours);
  void SerializeRenderMeshs(CVoxelObject * pVoxArea, CIndexedMesh * pIndexedMesh);
  AABB GetAABB();
  bool SetVolumeData( struct SVoxelChunkVer3 * pChunk, unsigned char ucChildId );
  bool ResetTransformation();
  IMaterial *GetMaterial(Vec3 * pHitPos);
  void InterpolateVoxelData();
  int GetMemoryUsage();
  void CopyHM();
  void ValidateSurfaceTypes();
  void NormalizeVolume(CVoxelObject ** pNeighbours);
};

// class containing voxel data and geometry of terrain sector
class CVoxelObject : public IVoxelObject, public Cry3DEngineBase
{
public:

	// physical representation
	IPhysicalEntity * m_pPhysEnt;
	phys_geometry * m_pPhysGeom;
	Vec3 m_vPos;
  Matrix34 m_Matrix;
  int m_nEditorObjectId;
  float m_fCurrDistance;
  PodArray<class CMemoryBlock*> m_arrMeshesForSerialization;
  struct SSurfaceType * m_arrSurfacesPalette[VOX_MAX_SURF_TYPES_NUM];
  int m_arrUsedSTypes[VOX_MAX_SURF_TYPES_NUM];
  CVoxelMesh * m_pVoxelMesh;
  int m_nFlags;
  AABB m_WSBBox;
  CVoxelVolume * m_pVoxelVolume;
  char * m_pObjectName;

  bool InitMaterials();
  void Compile(CVoxelObject ** pNeighbours);
  SSurfaceType * GetGlobalSurfaceType(ushort ucSurfaceTypeId);
  bool DoVoxelShape(EVoxelEditOperation eOperation, Vec3 vPos, float fRadius, 
    SSurfaceType * pLayer, Vec3 vBaseColor, EVoxelBrushShape eShape, CVoxelObject ** pNeighbours);
  void Physicalize(CMesh * pMesh, std::vector<char> * physData);
  using IRenderNode::Physicalize;
  void DePhysicalize();

	// construct
	CVoxelObject(Vec3 vOrigin, float fUnitSize, int nSizeXinUnits, int nSizeYinUnits, int nSizeZinUnits);
	~CVoxelObject();

  static PodArray<CVoxelObject*> * GetCounter()
  {
    static PodArray<CVoxelObject*> arrAll;
    return &arrAll;
  }

	IRenderMesh * GetRenderMesh(int nLod);
  void SetSurfacesInfo( SVoxelChunkVer3 * pChunk );
  void SetData( struct SVoxelChunkVer3 * pChunk, unsigned char ucChildId );
	void ResetRenderMeshs();
	void GetData(struct SVoxelChunkVer3 & chunk, Vec3 vOrigin);
  void GetData(struct SVoxelChunkVer4 & chunk, Vec3 vOrigin);
	virtual const char * GetEntityClassName(void) const { return "VoxAreaClass"; }
	virtual const char *GetName(void) const { return "VoxAreaName"; }
	virtual Vec3 GetPos(bool) const { return m_vPos; }
	virtual bool Render(const SRendParams &RendParams);
	virtual IPhysicalEntity *GetPhysics(void) const { return m_pPhysEnt; }
	virtual void SetPhysics(IPhysicalEntity * pPhys) { assert(!pPhys); }
	virtual void SetMaterial(IMaterial * pMat) { /*assert(!pMat);*/ }
	virtual IMaterial *GetMaterial(Vec3 * pHitPos = NULL);
	virtual float GetMaxViewDist();
	virtual EERType GetRenderNodeType() { return eERType_VoxelObject; }
	virtual int GetEditorObjectId() const;
	virtual void SetEditorObjectId(int nEditorObjectId) { m_nEditorObjectId = nEditorObjectId; }
	virtual struct IStatObj * GetEntityStatObj( unsigned int nPartId, unsigned int nSubPartId = 0, Matrix34* pMatrix = NULL, bool bReturnOnlyVisible = false);
	virtual void GetMemoryUsage(ICrySizer * pSizer);
	virtual void SetFlags(int nFlags);
	virtual void Regenerate();
	virtual void CopyHM();
	virtual void SetMatrix( const Matrix34& mat );
	const Matrix34& GetMatrix( ) { return m_Matrix; }
	virtual IMemoryBlock * GetCompiledData();
	virtual void SetCompiledData(void * pData, int nSize, unsigned char ucChildId = 0);
  virtual void SetObjectName( const char * pName );
	virtual bool ResetTransformation();
	virtual void ScheduleRebuild();
	virtual void InterpolateVoxelData();
	bool IsSnappedToTerrainSectors();
	int GetAmbientOcclusionForPoint(Vec3 vPos, Vec3 vNorm, float & fRes);
  void OnRenderNodeBecomeVisible();
  virtual const AABB GetBBox() const { return m_WSBBox; }
  virtual void SetBBox( const AABB& WSBBox ) { m_WSBBox = WSBBox; }
  bool SaveIndexedMeshToFile( CMesh * pMesh, const char *sFilename, IChunkFile** pOutChunkFile );
  void SerializeMesh( CMesh * pMesh );
  CContentCGF * LoadFromMemBlock( CMemoryBlock * pMemBlock, CMesh * & pMesh, std::vector<char> * physData );
  void ReleaseMemBlocks();
  int GetCompiledMeshDataSize();
  byte * GetCompiledMeshData(int & nSize);
  void SetCompiledMeshData(byte * pData, int nSize);
  void SavePhysicalizeData( CNodeCGF *pNode );
  void SerializeMeshes(CVoxelObject * pVoxArea, CIndexedMesh * pIndexedMesh);
  IPhysicalEntity * GetPhysics(int id) { return m_pPhysEnt; }
  bool IsEmpty();

	virtual void MoveContent(const Vec3 &move);
	virtual bool IsLocked();
};

#endif
