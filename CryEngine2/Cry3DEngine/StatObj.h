////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobj.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef STAT_OBJ_H
#define STAT_OBJ_H

class CIndexedMesh;
class CRenderObject;
class CContentCGF;
struct CNodeCGF;
struct CMaterialCGF;
struct phys_geometry;
struct IIndexedMesh;

#include "../Cry3DEngine/Cry3DEngineBase.h"
#include "CryArray.h"

#include <IStatObj.h>
#include <IStreamEngine.h>
#include "RenderMeshUtils.h"

#define STATOBJ_EFT_PLANT             (EFT_USER_FIRST+1)
#define STATOBJ_EFT_PLANT_IN_SHADOW   (EFT_USER_FIRST+2)

#define CGF_STREAMING_MAX_TIME_PER_FRAME 0.01f
#define MAX_PHYS_GEOMS_TYPES 4
#define MAX_STATOBJ_LODS_NUM 6


struct SDeformableMeshData 
{
	IGeometry *pInternalGeom;
	int *pVtxMap;
	unsigned int *pUsedVtx;
	int *pVtxTri;
	int *pVtxTriBuf;
	float *prVtxValency;
	Vec3 *pPrevVtx;
	float kViscosity;
};

struct SSpine 
{
	~SSpine() { if (pVtx) delete[] pVtx; if (pVtxCur) delete[] pVtxCur; if (pSegDim) delete[] pSegDim; }

	bool bActive;
	Vec3 *pVtx;
	Vec3 *pVtxCur;
	Vec4 *pSegDim;
	int nVtx;
	float len;
	Vec3 navg;
	int idmat;
	int iAttachSpine;
	int iAttachSeg;
};

class CStatObjFoliage : public IFoliage, public Cry3DEngineBase
{
public:
	CStatObjFoliage() { 
		m_pStatObj=0; m_pRopes=0; m_pRopesActiveTime=0; m_nRopes=0; m_nRefCount=1; m_timeIdle=0; m_pVegInst=0;
		m_pSkinQuats=0; m_pPrevSkinQuats=0; m_nFrameID=0; m_iActivationSource=0; m_flags=0; m_bGeomRemoved=0;
		m_bWasVisible = 1; m_timeInvisible = 0;
	}
	~CStatObjFoliage();
	virtual void AddRef() { m_nRefCount++; }
	virtual void Release() { if (--m_nRefCount<=0) delete this; }
	//virtual void ProcessSkinning(const Vec3& t,const Matrix34& mtxModel, int nTemplate, int nLod=-1, bool bForceUpdate=false) {}
	virtual uint32 GetSkeletonPose(int nLod, const Matrix34& RenderMat34, QuatTS*& pBoneQuatsL, QuatTS*& pBoneQuatsS, QuatTS*& pMBBoneQuatsL, QuatTS*& pMBBoneQuatsS, Vec4 shapeDeformationData[], uint32 &DoWeNeedMorphtargets, uint8*& pRemapTable );

	virtual int Serialize(TSerialize ser);
	virtual void SetFlags(int flags);
	virtual int GetFlags() { return m_flags; }
	virtual IRenderNode* GetIRenderNode()	{	return m_pVegInst; }

	void OnHit(struct EventPhysCollision *pHit);
	void Update(float dt);
	void BreakBranch(int idx);

	CStatObjFoliage *m_next,*m_prev;
	int m_nRefCount;
	int m_flags;
	CStatObj *m_pStatObj;
	IPhysicalEntity **m_pRopes;
	float *m_pRopesActiveTime;
	int m_nRopes;
	float m_timeIdle,m_lifeTime;
	IFoliage **m_ppThis;
	QuatTS *m_pSkinQuats;
  QuatTS *m_pPrevSkinQuats;
  uint32 m_nFrameID;
	int m_iActivationSource;
	int m_bGeomRemoved;
	IRenderNode *m_pVegInst;
	int m_bWasVisible;
	float m_timeInvisible;
};

enum EFileStreamingStatus
{
	ecss_NotLoaded,
	ecss_InProgress,
	ecss_Ready
};

struct CStatObj : public IStatObj, public IStreamCallback, public Cry3DEngineBase
{
  CStatObj();
  ~CStatObj();

  static PodArray<CStatObj*> * GetCounter()
  {
    static PodArray<CStatObj*> arrAll;
    return &arrAll;
  }

public:
 	//////////////////////////////////////////////////////////////////////////
	// Variables.
	//////////////////////////////////////////////////////////////////////////
	int m_nUsers; // reference counter
	int m_nChildUsers; // reference counter of all child users.

	IRenderMesh *m_pRenderMesh;
	// Render mesh for occlusion only.
	_smart_ptr<IRenderMesh> m_pRenderMeshOcclusion;
	CIndexedMesh *m_pIndexedMesh;

	string m_szFolderName;
	string m_szFileName;
	string m_szGeomName;
	string m_szProperties;
	
	int m_nLoadedTrisCount;
	int m_nRenderTrisCount;
	int m_nRenderMatIds;

	// Default material.
	_smart_ptr<IMaterial> m_pMaterial;
	uint32 m_nMaterialLayersMask;

  float m_fObjectRadius;
	float m_fRadiusHors;
	float m_fRadiusVert;

	Vec3 m_vBoxMin, m_vBoxMax, m_vBoxCenter;
	Vec3 m_vObjectNormal;
	float m_fObjectNormalMaxDot;
	
	phys_geometry *m_arrPhysGeomInfo[MAX_PHYS_GEOMS_TYPES];
	ITetrLattice *m_pLattice;
	IGeometry *m_pSrcPhysMesh;
	IStatObj *m_pLastBooleanOp;
	float m_lastBooleanOpScale;

//	ShadowMapFrustum * m_pSMLSource;

	_smart_ptr<CStatObj> m_arrpLowLODs[MAX_STATOBJ_LODS_NUM];
	CStatObj *m_pLod0; // Level 0 stat object. (Pointer to the original object of the LOD)
	unsigned int m_nMinUsableLod0 : 8;  // What is the minimal LOD that can be used as LOD0.
	unsigned int m_nMaxUsableLod  : 8;  // What is the maximal LOD that can be used.
	unsigned int m_nLoadedLodsNum : 8;  // How many lods loaded.

	// loading state
	int  m_nLastRendFrameId;
	int  m_nMarkedForStreamingFrameId;

	//////////////////////////////////////////////////////////////////////////
	// Externally set flags from enum EStaticObjectFlags.
	//////////////////////////////////////////////////////////////////////////
	int m_nFlags;

	//////////////////////////////////////////////////////////////////////////
	// Internal Flags.
	//////////////////////////////////////////////////////////////////////////
	unsigned int m_bCheckGarbage : 1;
	unsigned int m_bUseStreaming : 1;
	unsigned int m_bDefaultObject : 1;
	unsigned int m_bOpenEdgesTested : 1;
	unsigned int m_bSubObject : 1;       // This is sub object.
	unsigned int m_bHavePhysicsProxy : 1;  // Physics proxy geometry used to physicalize this object.
	unsigned int m_bVehicleOnlyPhysics : 1;  // Object can be used for collisions with vehicles only
	unsigned int m_bBreakableByGame : 1; // material is marked as breakable by game
	unsigned int m_bLod : 1;  // Physics proxy geometry used to physicalize this object.
	unsigned int m_bSharesChildren : 1; // means its subobjects belong to another parent statobj
	unsigned int m_bHasDeformationMorphs : 1;
	unsigned int m_bTmpIndexedMesh : 1; // indexed mesh is temporary and can be deleted after MakeRenderMesh
	unsigned int m_bUnmergable : 1; // Set if sub objects cannot be merged together to the single render merge.
	unsigned int m_bMerged : 1; // Set if sub objects merged together to the single render merge.
	unsigned int m_bForInternalUse : 1; // Set if sub objects merged together to the single render merge.
	unsigned int m_bLowSpecLod0Set : 1;
	unsigned int m_bHaveOcclusionProxy : 1; // If this stat object or its childs have occlusion proxy.
	int m_idmatBreakable;	// breakable id for the physics
	//////////////////////////////////////////////////////////////////////////
	
	IReadStreamPtr m_pReadStream;
	EFileStreamingStatus m_eCCGFStreamingStatus;

	static float m_fStreamingTimePerFrame;
	//////////////////////////////////////////////////////////////////////////

	SDeformableMeshData *m_pDeformableMeshData;
	uint16 *m_pMapFaceToFace0;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Bendable Foliage.
	//////////////////////////////////////////////////////////////////////////
	SSpine *m_pSpines;
	int m_nSpines;
	struct SMeshBoneMapping *m_pBoneMapping;
	std::vector<uint16> m_chunkBoneIds;
	//IRenderMesh *m_pRenderMeshSkinned;
	int m_nPhysFoliages;
	//////////////////////////////////////////////////////////////////////////

private:
	//////////////////////////////////////////////////////////////////////////
	// Sub objects.
	//////////////////////////////////////////////////////////////////////////
	DynArray<SSubObject> m_subObjects;
	CStatObj *m_pParentObject;  // Parent object (Must not be smart pointer). 
	CStatObj *m_pClonedSourceObject;  // If this is cloned object, pointer to original source object (Must not be smart pointer).
	_smart_ptr<CStatObj> m_pMergedObject;
	int m_nSubObjectMeshCount;

	//////////////////////////////////////////////////////////////////////////
	// Special AI/Physics parameters.
	//////////////////////////////////////////////////////////////////////////
  float m_aiVegetationRadius;
	float m_phys_mass;
	float m_phys_density;

	//////////////////////////////////////////////////////////////////////////
	// METHODS.
	//////////////////////////////////////////////////////////////////////////
public:
	//////////////////////////////////////////////////////////////////////////
	// Fast non virtual access functions.
	ILINE SSubObject& SubObject( int nIndex ) { return m_subObjects[nIndex]; };
	ILINE int SubObjectCount() const { return m_subObjects.size(); };
	//////////////////////////////////////////////////////////////////////////

	IIndexedMesh *GetIndexedMesh(bool bCreatefNone=false);
	void ReleaseIndexedMesh(bool bRenderMeshUpdated=false);
	ILINE const Vec3 GetCenter() { return m_vBoxCenter; }
	ILINE float GetRadius() { return m_fObjectRadius; }

	virtual void SetFlags( int nFlags ) { m_nFlags = nFlags; };
	virtual int  GetFlags() { return m_nFlags; };

  // Loader
  bool LoadCGF( const char *filename,bool bLod=false,bool bForceBreakable=false );

	//////////////////////////////////////////////////////////////////////////
	void SetMaterial( IMaterial *pMaterial );
	IMaterial* GetMaterial() { return m_pMaterial; };
	//////////////////////////////////////////////////////////////////////////

  virtual void Render(const SRendParams & rParams);
	void RenderRenderMesh(const SRendParams & rParams, int nLod, float fLodAmmount, struct SInstancingInfo * pInstInfo = NULL);
	phys_geometry* GetPhysGeom( int nGeomType ) { return m_arrPhysGeomInfo[nGeomType]; }
	void SetPhysGeom( phys_geometry *pPhysGeom, int nGeomType=0 ) 
	{ 
		if(m_arrPhysGeomInfo[nGeomType])
			GetPhysicalWorld()->GetGeomManager()->UnregisterGeometry(m_arrPhysGeomInfo[nGeomType]);
		m_arrPhysGeomInfo[nGeomType] = pPhysGeom; 
	}
	ITetrLattice *GetTetrLattice() { return m_pLattice; }

  float GetAIVegetationRadius() const {return m_aiVegetationRadius;}
  void SetAIVegetationRadius(float radius) {m_aiVegetationRadius = radius;}

  //! Refresh object ( reload shaders or/and object geometry )
	virtual void Refresh(int nFlags);

  IRenderMesh * GetRenderMesh() { return m_pRenderMesh; };
  void SetRenderMesh( IRenderMesh *pRM );

  const char *GetFolderName() { return (m_szFolderName); }
  const char *GetFilePath()    { return (m_szFileName); }
	void SetFilePath(const char * szFileName) { m_szFileName = szFileName; }
  const char *GetGeoName()    { return (m_szGeomName); }
  bool IsSameObject(const char * szFileName, const char * szGeomName);

  //set object's min/max bbox 
  void  SetBBoxMin(const Vec3 &vBBoxMin) { m_vBoxMin=vBBoxMin; }  
  void  SetBBoxMax(const Vec3 &vBBoxMax) { m_vBoxMax=vBBoxMax; }
  Vec3 GetBoxMin() { return m_vBoxMin; }
  Vec3 GetBoxMax() { return m_vBoxMax; }
  AABB GetAABB() {  return AABB(m_vBoxMin,m_vBoxMax);  }
	bool GetNormalsCone(Vec3 & vNormal, float & fMaxDot);
//  void MakeVegetationShadowMap(const Vec3 vSunPos);
//	void FreeVegetationShadowMap();

	float ComputeExtent(GeomQuery& geo, EGeomForm eForm);
	void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm);

  void SetCurDynMask(int nCurDynMask);

  virtual Vec3 GetHelperPos(const char * szHelperName);
  virtual const Matrix34& GetHelperTM(const char * szHelperName);

  float & GetRadiusVert() { return m_fRadiusVert; }
  float & GetRadiusHors() { return m_fRadiusHors; }

  virtual void AddRef();
  virtual void Release();

  virtual bool IsDefaultObject() { return (m_bDefaultObject); }

  float GetDistFromPoint(const Vec3 & vPoint);
  int GetLoadedTrisCount() { return m_nLoadedTrisCount; }
	int GetRenderTrisCount() { return m_nRenderTrisCount; }

	// Load LODs
	void SetLodObject( int nLod,CStatObj *pLod );
	void LoadLowLODs(bool bLoadLater);
	// Free render resources for unused upper LODs.
	void CleanUnusedLods();

  virtual void FreeIndexedMesh();
//  virtual const CDLight * GetLightSources(int nId);
  bool RenderDebugInfo( SRendParams &rParams,CRenderObject *pObj,IMaterial* &pMaterial, int nLod );
  void DrawMatrix(const Matrix34 & pMat);

  //! Release method.
  void GetMemoryUsage(class ICrySizer* pSizer);
  
  void ShutDown();
  void Init();

  void CheckLoaded();
  virtual IStatObj* GetLodObject( int nLodLevel,bool bReturnNearest=false );

	// interface IStreamCallback -----------------------------------------------------
  
	virtual void StreamOnComplete (IReadStream* pStream, unsigned nError);

	// -------------------------------------------------------------------------------

  void StreamCCGF(bool bFinishNow);

  void MakeCompiledFileName(char * szCompiledFileName, int nMaxLen);
  void ProcessStreamOnCompleteError();

  bool IsPhysicsExist();
  void PreloadResources(float fDist, float fTime, int dwFlags);
  void SetupBending(CRenderObject * pObj, Vec2 const& vBending);
  bool IsSphereOverlap(const Sphere& sSphere);
	void Invalidate( bool bPhysics=false );

	void AnalizeFoliage();
	void CopyFoliageData(IStatObj *pObjDst, bool bMove=false, IFoliage *pSrcFoliage=0, int *pVtxMap=0, primitives::box *pMoveBoxes=0,int nMovedBoxes=-1);
	int PhysicalizeFoliage(IPhysicalEntity *pTrunk, const Matrix34 &mtxWorld, IFoliage *&pRes, float lifeTime=0.0f, int iSource=0);
	int SerializeFoliage(TSerialize ser, ISkinnable *pFoliage);

	IStatObj *UpdateVertices(strided_pointer<Vec3> pVtx, strided_pointer<Vec3> pNormals, int iVtx0,int nVtx, int *pVtxMap=0);

	//////////////////////////////////////////////////////////////////////////
	// Sub objects.
	//////////////////////////////////////////////////////////////////////////
	virtual int GetSubObjectCount() const { return m_subObjects.size(); }
	virtual void SetSubObjectCount( int nCount );
	virtual SSubObject* FindSubObject( const char *sNodeName );
	virtual SSubObject* GetSubObject( int nIndex );
	virtual bool RemoveSubObject( int nIndex );
	virtual IStatObj* GetParentObject() const { return m_pParentObject; }
	virtual IStatObj* GetCloneSourceObject() const { return m_pClonedSourceObject; }
	virtual bool IsSubObject() const { return m_bSubObject; };
	virtual bool CopySubObject( int nToIndex,IStatObj *pFromObj,int nFromIndex );
	virtual void PhysicalizeSubobjects(IPhysicalEntity *pent, const Matrix34 *pMtx, float mass,float density=0.0f, int id0=0);
	virtual SSubObject& AddSubObject( IStatObj *pStatObj );
	//////////////////////////////////////////////////////////////////////////	

	virtual bool SaveToCGF( const char *sFilename,IChunkFile** pOutChunkFile=NULL );

	virtual IStatObj* Clone(bool bCloneChildren=true);
	virtual IStatObj* Clone(bool bCloneGeometry,bool bCloneChildren,bool bMeshesOnly);

	virtual void MarkAsDeformable(IGeometry *pInternalGeom = 0);

	virtual int SetDeformationMorphTarget(IStatObj *pDeformed);
	virtual int SubobjHasDeformMorph(int iSubObj);
	virtual IStatObj *DeformMorph(const Vec3 &pt,float r,float strength, IRenderMesh *pWeights=0);

	virtual IStatObj *HideFoliage();

	virtual int Serialize(TSerialize ser);

	// Get object properties as loaded from CGF.
	virtual const char* GetProperties() { return m_szProperties.c_str(); };

	virtual bool GetPhysicalProperties( float &mass,float &density );

	virtual IStatObj *GetLastBooleanOp(float &scale) { scale=m_lastBooleanOpScale; return m_pLastBooleanOp; }

	// Intersect ray with static object.
	// Ray must be in object local space.
	virtual bool RayIntersection( SRayHitInfo &hitInfo,IMaterial *pCustomMtl=0 );
	//////////////////////////////////////////////////////////////////////////

	float GetOcclusionAmount();

	IParticleEffect* GetSurfaceBreakageEffect( const char *sType );

  void CheckUpdateObjectHeightmap();
  float GetObjectHeight(float x, float y);
  int GetMinUsableLod();

	int CountChildReferences();

protected:
	void MakeRenderMesh();
	bool MergeSubObjectsRenderMeshes();
	bool MergeSubObjectsRenderMeshes( int nLod );
	void UnMergeSubObjectsRenderMeshes();

	bool LoadCGF_Info( const char *filename );
	void ParseProperties();

	void CalcRadiuses();
//	void PrepareShadowMaps(const Vec3 & obj_pos, ShadowMapFrustum * pLSource, const Vec3 & vSunPos);

	void SavePhysicalizeData( CNodeCGF *pNode );
	void PhysicalizeCompiled( CNodeCGF *pNode );
	bool Physicalize( CMesh &mesh );
	bool PhysicalizeGeomType( int nGeomType,CMesh &mesh );
	bool RegisterPhysicGeom( int nGeomType,phys_geometry *pPhysGeom );

	void SetPhysGeom( int nGeomType,phys_geometry *pPhysGeom,CMesh &mesh );

	// Creates static object contents from mesh.
	// Return true if successfull.
	bool SetFromMesh( CMesh *pMesh );
	void CalcNormalCone(IRenderMesh * pRM);

  float * m_pHeightmap;
  int m_nHeightmapSize;
  float m_fOcclusionAmount;
};


//////////////////////////////////////////////////////////////////////////
// Wrapper arround CStatObj that allow rendering of static object with specified parameters.
//////////////////////////////////////////////////////////////////////////
class CStatObjWrapper : public CStatObj
{
	virtual void Render(const SRendParams & rParams);
private:
	// Reference Static Object this wraper was created for.
	CStatObj *m_pReference;
};

//////////////////////////////////////////////////////////////////////////
inline void InitializeSubObject( IStatObj::SSubObject &so )
{
	so.localTM.SetIdentity();
	so.name = "";
	so.properties = "";
	so.nType = STATIC_SUB_OBJECT_MESH;
	so.pWeights = 0;
	so.nParent = -1;
	so.tm.SetIdentity();
	so.bIdentityMatrix = true;
	so.bHidden = false;
	so.helperSize = Vec3(0,0,0);
	so.pStatObj = 0;
}

#endif // STAT_OBJ_H
