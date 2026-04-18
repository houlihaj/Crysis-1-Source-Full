////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjconstr.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: creation
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "IndexedMesh.h"
#include "../RenderDll/Common/Shadow_Renderer.h"
#include <IRenderer.h>
#include <CrySizer.h>
#include "ObjMan.h"
#include "RenderMeshMerger.h"

float CStatObj::m_fStreamingTimePerFrame=0;

#define MAX_VERTICES_MERGABLE 15000
#define MAX_TRIS_IN_LOD_0 512
#define TRIS_IN_LOD_WARNING_RAIO (1.5f)
// Minimal ratio of Lod(n-1)/Lod(n) polygons to consider LOD for sub-object merging.
#define MIN_TRIS_IN_MERGED_LOD_RAIO (1.5f)

//////////////////////////////////////////////////////////////////////////
void CStatObj::FreeIndexedMesh()
{
	delete m_pIndexedMesh;
	m_pIndexedMesh=0;
}

void CStatObj::CalcRadiuses()
{
	m_vBoxCenter = (m_vBoxMax+m_vBoxMin)*0.5f;
  m_fObjectRadius = m_vBoxMin.GetDistance(m_vBoxMax)*0.5f;
  float dxh = (float)max( fabs(GetBoxMax().x), fabs(GetBoxMin().x));
  float dyh = (float)max( fabs(GetBoxMax().y), fabs(GetBoxMin().y));
  m_fRadiusHors = (float)cry_sqrtf(dxh*dxh+dyh*dyh);
  m_fRadiusVert = (GetBoxMax().z - GetBoxMin().z)*0.5f;// never change this
}

CStatObj::CStatObj( ) 
{ 
  m_nUsers = 0; // reference counter
	m_nChildUsers = 0;
	m_nFlags = 0;
	m_nLastRendFrameId = 0;
  m_nMarkedForStreamingFrameId = 0;
	m_fOcclusionAmount = -1;
  m_pHeightmap = NULL;
  m_nHeightmapSize = 0;
  memset(m_arrpLowLODs,0,sizeof(m_arrpLowLODs));
  
	Init();

  GetCounter()->Add(this);
}

void CStatObj::Init() 
{
	m_pIndexedMesh = 0;
	m_nRenderTrisCount = m_nLoadedTrisCount = 0;
	m_nRenderMatIds = 0;
  m_fObjectRadius = 0;
	m_fRadiusHors = 0;
	m_fRadiusVert = 0;
	m_pParentObject = 0;
	m_pClonedSourceObject = 0;
	m_bHavePhysicsProxy = 0;
	m_bVehicleOnlyPhysics = 0;
	m_bBreakableByGame = 0;
	MARK_UNUSED m_idmatBreakable;

	m_nLoadedLodsNum = 1;
	m_nMinUsableLod0 = 0;
	m_nMaxUsableLod = 0;
	m_pLod0 = 0;
	m_vObjectNormal.Set(0,0,0);
	m_fObjectNormalMaxDot = 0;  
	
	m_aiVegetationRadius = -1.0f;
	m_phys_mass = -1.0f;
	m_phys_density = -1.0f;

	m_nMaterialLayersMask = 0;

	m_vBoxMin.Set(0,0,0); 
	m_vBoxMax.Set(0,0,0); 
	m_vBoxCenter.Set(0,0,0);
	memset(m_arrPhysGeomInfo, 0, sizeof(m_arrPhysGeomInfo));

//  m_pSMLSource = 0;

  m_pRenderMesh = 0;
	m_pRenderMeshOcclusion = 0;

	m_bDefaultObject=false;

	m_pMergedObject = 0;
  for(int i=0; i<MAX_STATOBJ_LODS_NUM; i++)
    if(m_arrpLowLODs[i])
      m_arrpLowLODs[i]->Init();

	m_eCCGFStreamingStatus = ecss_NotLoaded;
	m_pReadStream=0;
	m_nSubObjectMeshCount = 0;

	m_bUseStreaming  = false;
	m_bDefaultObject = false;
	m_bOpenEdgesTested = false;
	m_bSubObject = false;
	m_bSharesChildren = false;
	m_bHasDeformationMorphs = false;
	m_bTmpIndexedMesh = false;
	m_bMerged = false;
	m_bUnmergable = false;
	m_bLowSpecLod0Set = false;
	m_bForInternalUse = false;
	m_bHaveOcclusionProxy = false;
	m_bCheckGarbage = false;

	// Assign default material originally.
	m_pMaterial = GetMatMan()->GetDefaultMaterial();

	m_pLattice = 0;
	m_pDeformableMeshData = 0;
	m_pSpines = 0; m_nSpines = 0; m_pBoneMapping = 0;
	m_nPhysFoliages = 0;
	m_pSrcPhysMesh = 0;
	m_pLastBooleanOp = 0;
	m_pMapFaceToFace0 = 0;
}

CStatObj::~CStatObj() 
{ 
	if (m_bCheckGarbage)
		GetObjManager()->CheckForGarbage(this,false);

  GetCounter()->Delete(this);

	ShutDown();
}

void CStatObj::ShutDown() 
{
//	assert (IsHeapValid());
	m_pReadStream=0;

//	assert (IsHeapValid());

  delete m_pIndexedMesh; 
	m_pIndexedMesh = 0;

//	assert (IsHeapValid());

  for(int n=0; n<4; n++)
  if(m_arrPhysGeomInfo[n])
	{
		m_arrPhysGeomInfo[n]->pGeom->SetForeignData(0,0);
    GetPhysicalWorld()->GetGeomManager()->UnregisterGeometry(m_arrPhysGeomInfo[n]);
	}

//	assert (IsHeapValid());

//  delete m_pSMLSource;

	SetRenderMesh(0);

	if (m_pRenderMeshOcclusion)
	{
		m_pRenderMeshOcclusion = 0;
	}

//	assert (IsHeapValid());

/* // SDynTexture is not accessable for 3dengine

	for(int i=0; i<FAR_TEX_COUNT; i++)
		if(m_arrSpriteTexPtr[i])
			m_arrSpriteTexPtr[i]->ReleaseDynamicRT(true);

	for(int i=0; i<FAR_TEX_COUNT_60; i++)
		if(m_arrSpriteTexPtr_60[i])
			m_arrSpriteTexPtr_60[i]->ReleaseDynamicRT(true);
*/

	if (m_pLattice)
		m_pLattice->Release();
	m_pLattice = 0;

	for(int i=0; i<MAX_STATOBJ_LODS_NUM; i++)
		if(m_arrpLowLODs[i])
		{
			// Sub objects do not own the LODs, so they should not delete them.
			m_arrpLowLODs[i] = 0;
		}

	//////////////////////////////////////////////////////////////////////////
	// Handle sub-objects and parents.
	//////////////////////////////////////////////////////////////////////////
	for (size_t i=0; i<m_subObjects.size(); i++)
	{
		CStatObj *pChildObj = (CStatObj*)m_subObjects[i].pStatObj;
		if (pChildObj)
		{
			if (!m_bSharesChildren)
				pChildObj->m_pParentObject = NULL;
			pChildObj->Release();
		}
	}
	m_subObjects.clear();

	if (m_pParentObject && !m_pParentObject->m_subObjects.empty())
	{
		// Remove this StatObject from sub-objects of the parent.
		SSubObject *pSubObjects = &m_pParentObject->m_subObjects[0];
		for (int i = 0,num = m_pParentObject->m_subObjects.size(); i < num; i++)
		{
			if (pSubObjects[i].pStatObj == this)
			{
				m_pParentObject->m_subObjects.erase( m_pParentObject->m_subObjects.begin()+i );
				break;
			}
		}
	}

	SAFE_RELEASE(m_pClonedSourceObject);
	//////////////////////////////////////////////////////////////////////////

	if (m_pDeformableMeshData)
	{
		if (m_pDeformableMeshData->pInternalGeom) m_pDeformableMeshData->pInternalGeom->Release();
		if (m_pDeformableMeshData->pVtxMap) delete[] m_pDeformableMeshData->pVtxMap;
		if (m_pDeformableMeshData->pUsedVtx) delete[] m_pDeformableMeshData->pUsedVtx;
		if (m_pDeformableMeshData->pVtxTri) delete[] m_pDeformableMeshData->pVtxTri;
		if (m_pDeformableMeshData->pVtxTriBuf) delete[] m_pDeformableMeshData->pVtxTriBuf;
		if (m_pDeformableMeshData->pPrevVtx) delete[] m_pDeformableMeshData->pPrevVtx;
		if (m_pDeformableMeshData->prVtxValency) delete[] m_pDeformableMeshData->prVtxValency;
		delete m_pDeformableMeshData;
	}
	m_pDeformableMeshData = 0;

	if (m_pSpines) {
		for(int i=0;i<m_nSpines;i++)
			delete[] m_pSpines[i].pVtx, delete[] m_pSpines[i].pVtxCur;
		free(m_pSpines); m_pSpines = 0; m_nSpines = 0;
		if (m_pBoneMapping)
			delete[] m_pBoneMapping;
		m_pBoneMapping = 0;
	}

	if (m_pMapFaceToFace0)
		delete[] m_pMapFaceToFace0;
	m_pMapFaceToFace0 = 0;

  delete [] m_pHeightmap;

	if (m_pSrcPhysMesh)
	{
		m_pSrcPhysMesh->SetForeignData(0,0);
		m_pSrcPhysMesh->Release();
	}
}


/*
void CStatObj::BuildOcTree()
{
  if(!m_pIndexedMesh->m_nFaceCount || m_pIndexedMesh->m_nFaceCount<=2)
    return;

  CBox parent_box( m_pIndexedMesh->m_vBoxMin, m_pIndexedMesh->m_vBoxMax );
  parent_box.max += 0.01f;
  parent_box.min +=-0.01f;

  SFace ** allFaces = new SFace *[m_pIndexedMesh->m_nFaceCount];

  for(int f=0; f<m_pIndexedMesh->m_nFaceCount; f++)
  {
    m_pIndexedMesh->m_pFaces[f].m_vCenter = 
     (Vec3(&m_pIndexedMesh->m_pVerts[m_pIndexedMesh->m_pFaces[f].v[0]].x) + 
      Vec3(&m_pIndexedMesh->m_pVerts[m_pIndexedMesh->m_pFaces[f].v[1]].x) + 
      Vec3(&m_pIndexedMesh->m_pVerts[m_pIndexedMesh->m_pFaces[f].v[2]].x))/3.f;

    allFaces[f] = &m_pIndexedMesh->m_pFaces[f];
  }

  const int max_tris_in_leaf = 2000;
  const float leaf_min_size  = stricmp(m_szGeomName,"sector_0") ? 16.f : 32.f;

  text_to_log("  Generating octree ... ");
  m_pOcTree = new octree_node( &parent_box, allFaces, IndexedMesh->m_nFaceCount, IndexedMesh, leaf_min_size, max_tris_in_leaf, 2);
  text_to_log_plus("%d leafs created", octree_node::static_current_leaf_id);
  m_pOcTree->update_bbox(IndexedMesh);

  delete [] allFaces;
}*/

//float M akeBuffersTime = 0;

void CStatObj::MakeRenderMesh()
{
	if(GetSystem()->IsDedicated())
		return;

  FUNCTION_PROFILER_3DENGINE;

	SetRenderMesh(0);
	
	if (m_pIndexedMesh && m_pIndexedMesh->GetSubSetCount() == 0)
		return;

	CMesh *pMesh = m_pIndexedMesh->GetMesh();

	m_nRenderTrisCount = 0;
	//////////////////////////////////////////////////////////////////////////
	// Initialize Mesh subset material flags.
	//////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < pMesh->GetSubSetCount(); i++)
	{
		SMeshSubset &subset = pMesh->m_subsets[i];
		if (!(subset.nMatFlags&MTL_FLAG_NODRAW))
		{
			m_nRenderTrisCount += subset.nNumIndices/3;
		}
	}
	//////////////////////////////////////////////////////////////////////////
	if (!m_nRenderTrisCount)
		return;

	m_pRenderMesh = GetRenderer()->CreateRenderMesh(eBT_Static,"StatObj_Dynamic",GetFilePath());
	m_pRenderMesh->SetMaterial( m_pMaterial );
	SMeshBoneMapping *pBoneMap = pMesh->m_pBoneMapping;
	pMesh->m_pBoneMapping = 0;
	m_pRenderMesh->SetMesh( *pMesh );
	pMesh->m_pBoneMapping = pBoneMap;
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::GetNormalsCone(Vec3 & vNormal, float & fMaxDot)
{
	vNormal = m_vObjectNormal;
	fMaxDot = m_fObjectNormalMaxDot;
	return m_fObjectNormalMaxDot>0;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::SetMaterial( IMaterial *pMaterial )
{
	m_pMaterial = pMaterial;
	if (m_pRenderMesh)
		m_pRenderMesh->SetMaterial( m_pMaterial );
}

///////////////////////////////////////////////////////////////////////////////////////    
Vec3 CStatObj::GetHelperPos(const char * szHelperName)
{
	SSubObject* pSubObj = FindSubObject(szHelperName);
	if (!pSubObj)
		return Vec3(0,0,0);

	return Vec3(pSubObj->tm.m03, pSubObj->tm.m13, pSubObj->tm.m23);
}

///////////////////////////////////////////////////////////////////////////////////////   
const Matrix34& CStatObj::GetHelperTM(const char * szHelperName)
{
  SSubObject* pSubObj = FindSubObject(szHelperName);  
  
  if (!pSubObj)
  {    
    static Matrix34 identity(IDENTITY);
    return identity;
  }

  return pSubObj->tm;
}



/*
bool CStatObj::GetHelper(int id, char * szHelperName, int nMaxHelperNameSize, Vec3 * pPos, Vec3 * pRot)
{
  if(id<0 || id>=IndexedMesh->m_Helpers.Count())
    return false;

  strncpy(szHelperName, IndexedMesh->m_Helpers[id].name, nMaxHelperNameSize);
  *pPos = IndexedMesh->m_Helpers[id].pos;
  *pRot = IndexedMesh->m_Helpers[id].rot;
  return true;
} */

/*
const char * CStatObj::GetPhysMaterialName(int nMatID)
{
  if(m_pRenderMesh && m_pRenderMesh->GetChunks() && nMatID < m_pRenderMesh->GetChunks()->Count())
    return m_pRenderMesh->GetChunks()->Get(nMatID)->szPhysMat;

  return 0;
}

bool CStatObj::SetPhysMaterialName(int nMatID, const char * szPhysMatName)
{
  if(m_pRenderMesh && m_pRenderMesh->GetChunks() && nMatID < m_pRenderMesh->GetChunks()->Count())
  {
    strncpy(m_pRenderMesh->GetChunks()->Get(nMatID)->szPhysMat, szPhysMatName, sizeof(m_pRenderMesh->GetChunks()->Get(nMatID)->szPhysMat));
    return true;
  }

  return false;
}
*/

void CStatObj::AddRef()
{
	m_nUsers++;
}

void CStatObj::Release()
{
	m_nUsers--;
	if (m_nUsers <= 1 && m_pParentObject && !m_pParentObject->m_bCheckGarbage && m_pParentObject->m_nUsers <= 0)
	{
		m_pParentObject->m_bCheckGarbage = true;
		GetObjManager()->CheckForGarbage( m_pParentObject,true );
	}
	if(m_nUsers<=0)
	{
		GetObjManager()->InternalDeleteObject(this);
	}
}

float CStatObj::GetDistFromPoint(const Vec3 & vPoint)
{
  float fMinDist = 4096;
  for(int v=0; v<m_pIndexedMesh->m_numVertices; v++)
  {
    float fDist = m_pIndexedMesh->m_pPositions[v].GetDistance(vPoint);
    if(fDist < fMinDist)
      fMinDist = fDist;
  }

  return fMinDist;
}

bool CStatObj::IsSameObject(const char * szFileName, const char * szGeomName)
{
	// cmp object names
	if (szGeomName)
	{
		// [Anton] - always use new cgf for objects used for cloth simulation
		if (*szGeomName && stricmp(szGeomName,"cloth") == 0)
			return false;

		if(stricmp(szGeomName,m_szGeomName)!=0)
			return false;
	}

  // Normilize file name
	char szFileNameNorm[MAX_PATH_LENGTH]="";
	char *pszDest = szFileNameNorm;
	const char *pszSource = szFileName;
	while (*pszSource)
	{
		if (*pszSource=='\\')
			*pszDest++='/';
		else 
			*pszDest++=*pszSource;
		pszSource++;
	}
	*pszDest=0;

	// cmp file names
	if(stricmp(szFileNameNorm,m_szFileName)!=0)
		return false;

	return true;
}

void CStatObj::GetMemoryUsage(ICrySizer* pSizer)
{
  int nSize = sizeof(CStatObj);

  nSize += m_subObjects.size() * sizeof(SSubObject);

	pSizer->AddObject(this,nSize);

	for (uint i = 0; i < m_subObjects.size(); i++)
	{
		if (m_subObjects[i].pStatObj)
			m_subObjects[i].pStatObj->GetMemoryUsage(pSizer);
	}

	for(int i=1; i<MAX_STATOBJ_LODS_NUM; i++)
  {
		if(m_arrpLowLODs[i])
			m_arrpLowLODs[i]->GetMemoryUsage(pSizer);
  }

	if(m_pIndexedMesh)
	{
		SIZER_COMPONENT_NAME(pSizer, "IndexedMesh");
		pSizer->AddObject(m_pIndexedMesh,m_pIndexedMesh->GetMemoryUsage());
	}

	if (m_pMergedObject)
		m_pMergedObject->GetMemoryUsage(pSizer);
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::SetLodObject( int nLod,CStatObj *pLod )
{
	assert( nLod > 0 && nLod < MAX_STATOBJ_LODS_NUM );
	if (nLod <= 0 || nLod >= MAX_STATOBJ_LODS_NUM)
		return;

	if (strstr(m_szProperties,"lowspeclod0") != 0)
		m_bLowSpecLod0Set = true;

	if (pLod)
	{
		// Check if low lod decrease ammount of used materials.
		int nPrevLodMatIds = m_nRenderMatIds;
		int nPrevLodTris = m_nRenderTrisCount;
		if (nLod > 1 && m_arrpLowLODs[nLod-1])
		{
			nPrevLodMatIds = m_arrpLowLODs[nLod-1]->m_nRenderMatIds;
			nPrevLodTris = m_arrpLowLODs[nLod-1]->m_nRenderTrisCount;
		}

		int min_tris = GetCVars()->e_lod_min_tris;
		if (((pLod->m_nRenderTrisCount >= min_tris || nPrevLodTris >= (3*min_tris)/2)
				|| pLod->m_nRenderMatIds < nPrevLodMatIds) && nLod > m_nMaxUsableLod)
		{
			m_nMaxUsableLod = nLod;
		}

		pLod->m_pLod0 = this;
		pLod->m_pMaterial = m_pMaterial; // Lod must use same material as parent.
		if ((m_nRenderTrisCount && pLod != 0) && (pLod->m_nRenderTrisCount > m_nRenderTrisCount / TRIS_IN_LOD_WARNING_RAIO) &&
			(m_nRenderMatIds == pLod->m_nRenderMatIds))
		{
			FileWarning(0, GetFilePath(),	"LOD%d of geometry %s contains too many polygons comparing to the LOD0 [%d against %d]",nLod,m_szGeomName.c_str(),
				pLod->m_nRenderTrisCount, m_nRenderTrisCount);
		}
		if (pLod->m_nRenderTrisCount > MAX_TRIS_IN_LOD_0)
		{
			if (strstr(pLod->GetProperties(),"lowspeclod0") != 0 && !m_bLowSpecLod0Set)
			{
				m_bLowSpecLod0Set = true;
				m_nMinUsableLod0 = nLod;
			}
			if (!m_bLowSpecLod0Set)
				m_nMinUsableLod0 = nLod;
		}
		if (nLod+1 > m_nLoadedLodsNum)
			m_nLoadedLodsNum = nLod+1;

		/*  Should depend from prev LOD
		if(m_arrpLowLODs[nLodLevel] != 0 && m_arrpLowLODs[nLodLevel]->m_nRenderTrisCount < MAX_TRIS_IN_LOD && m_arrpLowLODs[nLodLevel]->m_nRenderTrisCount != 0)
		{
		FileWarning(0, sLodFileName,	"Low LOD geometry %s contains too few polygons [%d], there is no sense to use it. Minimal recommended value is %d",
		sLodFileName,m_arrpLowLODs[nLodLevel]->m_nRenderTrisCount, MAX_TRIS_IN_LOD);
		}
		*/

		// When assigning lod to child object.
		if (m_pParentObject)
		{
			if (nLod+1 > m_pParentObject->m_nLoadedLodsNum)
				m_pParentObject->m_nLoadedLodsNum = nLod+1;
		}
	}
	m_arrpLowLODs[nLod] = pLod;
}

//////////////////////////////////////////////////////////////////////////
IStatObj * CStatObj::GetLodObject(int nLodLevel,bool bReturnNearest)
{
	if(nLodLevel<1)
  {
    if (!bReturnNearest || m_pRenderMesh != 0)
      return this;
		
		nLodLevel = 1;
  }

	CStatObj *pLod = 0;
	if(nLodLevel < MAX_STATOBJ_LODS_NUM)
	{
		pLod = m_arrpLowLODs[nLodLevel];
		
		// Look up
		if (bReturnNearest && !pLod)
		{
			// Find highest existing lod looking up.
			int lod = nLodLevel;
			while (lod > 0 && m_arrpLowLODs[lod] == 0) lod--;
			if (lod > 0)
				pLod = m_arrpLowLODs[lod];
			else if (m_pRenderMesh)
			{
				pLod = this;
			}
		}
		// Look down
		if (bReturnNearest && !pLod)
		{
			// Find highest existing lod looking down.
			for (int lod = nLodLevel+1; lod < MAX_STATOBJ_LODS_NUM; lod++)
			{
				if (m_arrpLowLODs[lod])
				{
					pLod = m_arrpLowLODs[lod];
					break;
				}
			}
		}
	}

	return pLod;
}

bool CStatObj::IsPhysicsExist()
{
	return m_arrPhysGeomInfo[0] || m_arrPhysGeomInfo[1] || m_arrPhysGeomInfo[2] || m_arrPhysGeomInfo[3];
}

bool CStatObj::IsSphereOverlap(const Sphere& sSphere)
{
	if(m_pRenderMesh && Overlap::Sphere_AABB(sSphere,AABB(m_vBoxMin,m_vBoxMax)))
	{ // if inside bbox
		int nInds = 0, nPosStride=0;
		ushort *pInds = m_pRenderMesh->GetIndices(&nInds);
		const byte * pPos = m_pRenderMesh->GetStridedPosPtr(nPosStride);
		
		if(pInds && pPos)
		for(int i=0; (i+2)<nInds; i+=3)
		{	// test all triangles of water surface strip
			Vec3 v0 = *(Vec3*)&pPos[nPosStride*pInds[i+0]];
			Vec3 v1 = *(Vec3*)&pPos[nPosStride*pInds[i+1]];
			Vec3 v2 = *(Vec3*)&pPos[nPosStride*pInds[i+2]];
			Vec3 vBoxMin = v0; vBoxMin.CheckMin(v1); vBoxMin.CheckMin(v2);
			Vec3 vBoxMax = v0; vBoxMax.CheckMax(v1); vBoxMax.CheckMax(v2);

			if(	Overlap::Sphere_AABB(sSphere,AABB(vBoxMin,vBoxMax)))
				return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::Invalidate( bool bPhysics )
{
	if (m_pIndexedMesh)
	{
		m_pIndexedMesh->Invalidate();

		m_vBoxMin = m_pIndexedMesh->m_bbox.min;
		m_vBoxMax = m_pIndexedMesh->m_bbox.max;

		MakeRenderMesh();
		m_pIndexedMesh->m_bInvalidated = false;
		m_nLoadedTrisCount = m_pIndexedMesh->GetFacesCount();
		if (!m_nLoadedTrisCount)
		{
			m_nLoadedTrisCount = m_pIndexedMesh->GetIndexCount()/3;
		}
		CalcRadiuses();

		if (bPhysics && !m_bHavePhysicsProxy)
			Physicalize( *m_pIndexedMesh->GetMesh() );

		ReleaseIndexedMesh(true);
	}

	// if this is compound multi-sub object.
	if (m_bMerged)
		UnMergeSubObjectsRenderMeshes();
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CStatObj::Clone(bool bCloneChildren)
{
	if (m_bDefaultObject)
		return this;

	CStatObj *pNewObj = new CStatObj;
	pNewObj->m_bForInternalUse = m_bForInternalUse;

	pNewObj->m_pClonedSourceObject = m_pClonedSourceObject ? m_pClonedSourceObject:this;
	pNewObj->m_pClonedSourceObject->AddRef(); // Cloned object will keep a reference to this.

	pNewObj->m_nLoadedTrisCount = m_nLoadedTrisCount;
	pNewObj->m_nRenderTrisCount = m_nRenderTrisCount;

	if (m_pIndexedMesh && !m_bMerged)
	{
		pNewObj->m_pIndexedMesh = new CIndexedMesh;
		pNewObj->m_pIndexedMesh->Copy( *m_pIndexedMesh->GetMesh() );
	}

	if (m_pRenderMesh && !m_bMerged) 
	{
		IRenderMesh *pRenderMesh = GetRenderer()->CreateRenderMesh(eBT_Static,"StatObj_Cloned",pNewObj->GetFilePath());
		m_pRenderMesh->CopyTo(pRenderMesh,true);
		pNewObj->SetRenderMesh(pRenderMesh);
	}

	//////////////////////////////////////////////////////////////////////////
	pNewObj->m_pRenderMeshOcclusion = m_pRenderMeshOcclusion;
	//////////////////////////////////////////////////////////////////////////

	//pNewObj->->m_szFolderName = m_szFolderName;
	//pNewObj->->m_szFileName = m_szFileName;
	//pNewObj->->m_szGeomName = m_szGeomName;

	// Default material.
	pNewObj->m_pMaterial = m_pMaterial;
	pNewObj->m_nMaterialLayersMask = m_nMaterialLayersMask;

	//*pNewObj->m_arrSpriteTexID = *m_arrSpriteTexID;

	int i;
	for (i = 0; i < MAX_PHYS_GEOMS_TYPES; i++)
	{
		pNewObj->m_arrPhysGeomInfo[i] = m_arrPhysGeomInfo[i];
		if (pNewObj->m_arrPhysGeomInfo[i])
			GetPhysicalWorld()->GetGeomManager()->AddRefGeometry( pNewObj->m_arrPhysGeomInfo[i] );
	}
	pNewObj->m_vBoxMin = m_vBoxMin;
	pNewObj->m_vBoxMax = m_vBoxMax;
	pNewObj->m_vBoxCenter = m_vBoxCenter;

	pNewObj->m_fObjectRadius = m_fObjectRadius;
	pNewObj->m_fRadiusHors = m_fRadiusHors;
	pNewObj->m_fRadiusVert = m_fRadiusVert;

	// Clone Lods?
	/*
	for (i = 0; i < MAX_STATOBJ_LODS_NUM; i++)
	{
		if (m_arrpLowLODs[i])
			pNewObj->m_arrpLowLODs[i] = m_arrpLowLODs[i]->Clone();
	}
	pNewObj->m_nLoadedLodsNum = m_nLoadedLodsNum;
	*/

	pNewObj->m_nFlags = m_nFlags | STATIC_OBJECT_CLONE;

	//////////////////////////////////////////////////////////////////////////
	// Internal Flags.
	//////////////////////////////////////////////////////////////////////////
	pNewObj->m_bUseStreaming = m_bUseStreaming;
	pNewObj->m_bDefaultObject = m_bDefaultObject;
	pNewObj->m_bOpenEdgesTested = m_bOpenEdgesTested;
	pNewObj->m_bSubObject = false; // [anton] since parent is not copied anyway
	pNewObj->m_bHavePhysicsProxy = m_bHavePhysicsProxy;
	pNewObj->m_bVehicleOnlyPhysics  = m_bVehicleOnlyPhysics;
	pNewObj->m_idmatBreakable  = m_idmatBreakable;
	pNewObj->m_bBreakableByGame = m_bBreakableByGame;
	pNewObj->m_bHasDeformationMorphs = m_bHasDeformationMorphs;
	pNewObj->m_bTmpIndexedMesh = m_bTmpIndexedMesh;
	pNewObj->m_bHaveOcclusionProxy = m_bHaveOcclusionProxy;
	//////////////////////////////////////////////////////////////////////////

	int nNumSubObj = (int)m_subObjects.size();
	pNewObj->m_subObjects.resize( nNumSubObj );
	for (i = 0; i < nNumSubObj; i++)
	{
		pNewObj->m_subObjects[i] = m_subObjects[i];
		if (m_subObjects[i].pStatObj)
		{
			if (bCloneChildren)
			{
				pNewObj->m_subObjects[i].pStatObj = m_subObjects[i].pStatObj->Clone();
				pNewObj->m_subObjects[i].pStatObj->AddRef();
				((CStatObj*)(pNewObj->m_subObjects[i].pStatObj))->m_pParentObject = this;
			}
			else
				m_subObjects[i].pStatObj->AddRef();
		}
	}
	pNewObj->m_nSubObjectMeshCount = m_nSubObjectMeshCount;
	if (!bCloneChildren)
		pNewObj->m_bSharesChildren = true;

	if (m_pDeformableMeshData && pNewObj->GetIndexedMesh(true))
	{
		int nVtx;
		CMesh *pMesh = pNewObj->GetIndexedMesh()->GetMesh();
		pNewObj->m_pDeformableMeshData = new SDeformableMeshData;
		nVtx = pMesh->m_numVertices;
		pMesh->ReallocStream(CMesh::POSITIONS, (nVtx-1&~31)+32);
		pMesh->ReallocStream(CMesh::NORMALS, (nVtx-1&~31)+32);
		pMesh->ReallocStream(CMesh::TEXCOORDS, (nVtx-1&~31)+32);
		pMesh->ReallocStream(CMesh::TANGENTS, (nVtx-1&~31)+32);
		pNewObj->m_pDeformableMeshData->pPrevVtx = new Vec3[pMesh->m_numVertices];
		memcpy(pNewObj->m_pDeformableMeshData->pVtxMap=new int[pMesh->m_numVertices], 
			m_pDeformableMeshData->pVtxMap, pMesh->m_numVertices*sizeof(int));
		memcpy(pNewObj->m_pDeformableMeshData->pUsedVtx = new unsigned int[pMesh->m_numVertices>>5], 
			m_pDeformableMeshData->pUsedVtx, (pMesh->m_numVertices>>5)*sizeof(int));
		memcpy(pNewObj->m_pDeformableMeshData->pVtxTri = new int[pMesh->m_numVertices+1], 
			m_pDeformableMeshData->pVtxTri, (pMesh->m_numVertices+1)*sizeof(int));
		memcpy(pNewObj->m_pDeformableMeshData->pVtxTriBuf = new int[pMesh->m_nIndexCount],
			m_pDeformableMeshData->pVtxTriBuf, pMesh->m_nIndexCount*sizeof(int));
		memcpy(pNewObj->m_pDeformableMeshData->prVtxValency = new float[pMesh->m_numVertices],
			m_pDeformableMeshData->prVtxValency, pMesh->m_numVertices*sizeof(float));
		pNewObj->m_pDeformableMeshData->kViscosity = m_pDeformableMeshData->kViscosity;
		if (pNewObj->m_pDeformableMeshData->pInternalGeom = m_pDeformableMeshData->pInternalGeom)
			pNewObj->m_pDeformableMeshData->pInternalGeom->AddRef();
		pMesh->m_numVertices = pMesh->m_nCoorCount = 
		pMesh->m_streamSize[CMesh::POSITIONS] = pMesh->m_streamSize[CMesh::NORMALS]=
		pMesh->m_streamSize[CMesh::TEXCOORDS] = pMesh->m_streamSize[CMesh::TANGENTS] = nVtx;
	}

	return pNewObj;
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CStatObj::Clone( bool bCloneGeometry,bool bCloneChildren,bool bMeshesOnly )
{
	if (m_bDefaultObject)
		return this;

	CStatObj *pNewObj = new CStatObj;
	pNewObj->m_bForInternalUse = m_bForInternalUse;

	pNewObj->m_pClonedSourceObject = m_pClonedSourceObject ? m_pClonedSourceObject:this;
	pNewObj->m_pClonedSourceObject->AddRef(); // Cloned object will keep a reference to this.

	pNewObj->m_nLoadedTrisCount = m_nLoadedTrisCount;
	pNewObj->m_nRenderTrisCount = m_nRenderTrisCount;

	if (bCloneGeometry)
	{
		if (m_pIndexedMesh && !m_bMerged)
		{
			pNewObj->m_pIndexedMesh = new CIndexedMesh;
			pNewObj->m_pIndexedMesh->Copy( *m_pIndexedMesh->GetMesh() );
		}
		if (m_pRenderMesh && !m_bMerged) 
		{
			IRenderMesh *pRenderMesh = GetRenderer()->CreateRenderMesh(eBT_Static,"StatObj_Cloned",pNewObj->GetFilePath());
			m_pRenderMesh->CopyTo(pRenderMesh,true);
			pNewObj->SetRenderMesh(pRenderMesh);
		}
	}
	else
	{
		if (m_pRenderMesh && !m_bMerged)
		{
			pNewObj->SetRenderMesh(m_pRenderMesh);
			m_pRenderMesh->AddRef();
		}
	}

	pNewObj->m_pRenderMeshOcclusion = m_pRenderMeshOcclusion;

	static string strName = "<Cloned>";
	pNewObj->m_szFolderName = strName;
	pNewObj->m_szFileName = m_szFileName;
	pNewObj->m_szGeomName = m_szGeomName;

	// Default material.
	pNewObj->m_pMaterial = m_pMaterial;

	//*pNewObj->m_arrSpriteTexID = *m_arrSpriteTexID;

	pNewObj->m_fObjectRadius = m_fObjectRadius;

	int i;
	for (i = 0; i < MAX_PHYS_GEOMS_TYPES; i++)
	{
		pNewObj->m_arrPhysGeomInfo[i] = m_arrPhysGeomInfo[i];
		if (pNewObj->m_arrPhysGeomInfo[i])
			GetPhysicalWorld()->GetGeomManager()->AddRefGeometry( pNewObj->m_arrPhysGeomInfo[i] );
	}
	pNewObj->m_vBoxMin = m_vBoxMin;
	pNewObj->m_vBoxMax = m_vBoxMax;
	pNewObj->m_vBoxCenter = m_vBoxCenter;

	pNewObj->m_fRadiusHors = m_fRadiusHors;
	pNewObj->m_fRadiusVert = m_fRadiusVert;

	// Clone Lods?
	/*
	for (i = 0; i < MAX_STATOBJ_LODS_NUM; i++)
	{
	if (m_arrpLowLODs[i])
	pNewObj->m_arrpLowLODs[i] = m_arrpLowLODs[i]->Clone();
	}
	pNewObj->m_nLoadedLodsNum = m_nLoadedLodsNum;
	*/

	pNewObj->m_nFlags = m_nFlags | STATIC_OBJECT_CLONE;

	//////////////////////////////////////////////////////////////////////////
	// Internal Flags.
	//////////////////////////////////////////////////////////////////////////
	pNewObj->m_bUseStreaming = m_bUseStreaming;
	pNewObj->m_bDefaultObject = m_bDefaultObject;
	pNewObj->m_bOpenEdgesTested = m_bOpenEdgesTested;
	pNewObj->m_bSubObject = false; // [anton] since parent is not copied anyway
	pNewObj->m_bHavePhysicsProxy = m_bHavePhysicsProxy;
	pNewObj->m_bVehicleOnlyPhysics = m_bVehicleOnlyPhysics;
	pNewObj->m_idmatBreakable = m_idmatBreakable;
	pNewObj->m_bBreakableByGame = m_bBreakableByGame;
	pNewObj->m_bHasDeformationMorphs = m_bHasDeformationMorphs;
	pNewObj->m_bTmpIndexedMesh = m_bTmpIndexedMesh;
	pNewObj->m_bHaveOcclusionProxy = m_bHaveOcclusionProxy;
	//////////////////////////////////////////////////////////////////////////

	int numSubObj = (int)m_subObjects.size();
	if (bMeshesOnly) 
	{
		numSubObj = 0;
		for(i=0;i<m_subObjects.size();i++)
		{
			if (m_subObjects[i].nType==STATIC_SUB_OBJECT_MESH)
				numSubObj++;
			else
				break;
		}
	}
	pNewObj->m_subObjects.resize(numSubObj);
	for (i=0; i < numSubObj; i++)
	{
		pNewObj->m_subObjects[i] = m_subObjects[i];
		if (m_subObjects[i].pStatObj)
		{
			if (bCloneChildren)
			{
				pNewObj->m_subObjects[i].pStatObj = m_subObjects[i].pStatObj->Clone();
				pNewObj->m_subObjects[i].pStatObj->AddRef();
				((CStatObj*)(pNewObj->m_subObjects[i].pStatObj))->m_pParentObject = this;
			}
			else
				m_subObjects[i].pStatObj->AddRef();
		}
	}
	pNewObj->m_nSubObjectMeshCount = m_nSubObjectMeshCount;
	if (!bCloneChildren)
		pNewObj->m_bSharesChildren = true;

	if (bCloneGeometry && m_pDeformableMeshData && pNewObj->GetIndexedMesh(true))
	{
		int nVtx;
		CMesh *pMesh = pNewObj->GetIndexedMesh()->GetMesh();
		pNewObj->m_pDeformableMeshData = new SDeformableMeshData;
		nVtx = pMesh->m_numVertices;
		pMesh->ReallocStream(CMesh::POSITIONS, (nVtx-1&~31)+32);
		pMesh->ReallocStream(CMesh::NORMALS, (nVtx-1&~31)+32);
		pMesh->ReallocStream(CMesh::TEXCOORDS, (nVtx-1&~31)+32);
		pMesh->ReallocStream(CMesh::TANGENTS, (nVtx-1&~31)+32);
		pNewObj->m_pDeformableMeshData->pPrevVtx = new Vec3[pMesh->m_numVertices];
		memcpy(pNewObj->m_pDeformableMeshData->pVtxMap=new int[pMesh->m_numVertices], 
			m_pDeformableMeshData->pVtxMap, pMesh->m_numVertices*sizeof(int));
		memcpy(pNewObj->m_pDeformableMeshData->pUsedVtx = new unsigned int[pMesh->m_numVertices>>5], 
			m_pDeformableMeshData->pUsedVtx, (pMesh->m_numVertices>>5)*sizeof(int));
		memcpy(pNewObj->m_pDeformableMeshData->pVtxTri = new int[pMesh->m_numVertices+1], 
			m_pDeformableMeshData->pVtxTri, (pMesh->m_numVertices+1)*sizeof(int));
		memcpy(pNewObj->m_pDeformableMeshData->pVtxTriBuf = new int[pMesh->m_nIndexCount],
			m_pDeformableMeshData->pVtxTriBuf, pMesh->m_nIndexCount*sizeof(int));
		memcpy(pNewObj->m_pDeformableMeshData->prVtxValency = new float[pMesh->m_numVertices],
			m_pDeformableMeshData->prVtxValency, pMesh->m_numVertices*sizeof(float));
		pNewObj->m_pDeformableMeshData->kViscosity = m_pDeformableMeshData->kViscosity;
		if (pNewObj->m_pDeformableMeshData->pInternalGeom = m_pDeformableMeshData->pInternalGeom)
			pNewObj->m_pDeformableMeshData->pInternalGeom->AddRef();
		pMesh->m_numVertices = pMesh->m_nCoorCount = 
			pMesh->m_streamSize[CMesh::POSITIONS] = pMesh->m_streamSize[CMesh::NORMALS]=
			pMesh->m_streamSize[CMesh::TEXCOORDS] = pMesh->m_streamSize[CMesh::TANGENTS] = nVtx;
	}

	return pNewObj;
}
//////////////////////////////////////////////////////////////////////////

IIndexedMesh *CStatObj::GetIndexedMesh(bool bCreateIfNone)
{ 
	if (m_pIndexedMesh)
		return m_pIndexedMesh;
	else if (m_pRenderMesh && bCreateIfNone)
	{
		m_pRenderMesh->GetIndexedMesh(m_pIndexedMesh = new CIndexedMesh);
		CMesh *pMesh = m_pIndexedMesh->GetMesh();
		if (!pMesh || pMesh->m_subsets.size()<=0)
		{
			m_pIndexedMesh->Release(); m_pIndexedMesh=0; 
			return 0;
		}
		m_bTmpIndexedMesh = true;

		int i,j,i0=pMesh->m_subsets[0].nFirstVertId+pMesh->m_subsets[0].nNumVerts;
		for(i=j=1;i<pMesh->m_subsets.size();i++) if (pMesh->m_subsets[i].nFirstVertId-i0<pMesh->m_subsets[j].nFirstVertId-i0)
			j = i;
		if (j<pMesh->m_subsets.size() && pMesh->m_subsets[0].nPhysicalizeType==PHYS_GEOM_TYPE_DEFAULT && 
				pMesh->m_subsets[j].nPhysicalizeType!=PHYS_GEOM_TYPE_DEFAULT && pMesh->m_subsets[j].nFirstVertId>i0)
		{
			pMesh->m_subsets[j].nNumVerts += pMesh->m_subsets[j].nFirstVertId-i0;
			pMesh->m_subsets[j].nFirstVertId = i0;
		}
		return m_pIndexedMesh;
	}
	return 0;
}

void CStatObj::ReleaseIndexedMesh(bool bRenderMeshUpdated)
{ 
	if (m_bTmpIndexedMesh && m_pIndexedMesh) 
	{ 
		int i,istart,iend;
		CMesh *pMesh = m_pIndexedMesh->GetMesh();
		if (m_pRenderMesh && !bRenderMeshUpdated)
		{
			PodArray<CRenderChunk> *pChunks = m_pRenderMesh->GetChunks();
			for(int i=0;i<pMesh->m_subsets.size();i++)
				(*pChunks)[i].m_nMatFlags |= pMesh->m_subsets[i].nMatFlags & 1<<30;
		}
		if (bRenderMeshUpdated && m_pBoneMapping)
		{
			for(i=iend=0;i<pMesh->m_subsets.size();i++)
				if ((pMesh->m_subsets[i].nMatFlags & (MTL_FLAG_NOPHYSICALIZE|MTL_FLAG_NODRAW)) == MTL_FLAG_NOPHYSICALIZE)	
				{
					for(istart=iend++; iend<m_chunkBoneIds.size() && m_chunkBoneIds[iend]; iend++);	
					if (!pMesh->m_subsets[i].nNumIndices) 
					{
						m_chunkBoneIds.erase(m_chunkBoneIds.begin()+istart,m_chunkBoneIds.begin()+iend);
						iend = istart;
					}
				}
		}
		m_pIndexedMesh->Release(); m_pIndexedMesh=0; 
		m_bTmpIndexedMesh = false; 
	}
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::MergeSubObjectsRenderMeshes()
{
  FUNCTION_PROFILER_3DENGINE;

	if (m_bUnmergable || m_nSubObjectMeshCount <= 0)
		return false;

	m_bMerged = false;
	m_pMergedObject = 0;

	for (int i = 0; i < m_nLoadedLodsNum; i++)
	{
		if (!MergeSubObjectsRenderMeshes(i))
			break;
	}
	if (!m_bMerged)
		m_bUnmergable = true;

	return m_bMerged;
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::MergeSubObjectsRenderMeshes( int nLod )
{
	if(GetSystem()->IsDedicated())
		return false;
	
	int nSubObjCount = (int)m_subObjects.size();

	int s;

	for(int s=0; s < nSubObjCount; s++)
	{
		CStatObj::SSubObject *pSubObj = &m_subObjects[s];
		if (pSubObj->pStatObj && pSubObj->nType == STATIC_SUB_OBJECT_MESH)
		{
			CStatObj *pStatObj = (CStatObj*)pSubObj->pStatObj;
			if (pStatObj->m_pMaterial != m_pMaterial) // All materials must be same.
			{
				return false;
			}
		}
	}

	PodArray<SRenderMeshInfoOutput> resultMeshes;
	PodArray<SRenderMeshInfoInput> lstRMI;

	SRenderMeshInfoInput rmi;

	rmi.pMat = m_pMaterial;
	rmi.mat.SetIdentity();
	rmi.pMesh = 0;
	rmi.pSrcRndNode = 0;
	int nTotalVertexCount = 0;
	int nTotalTriCount = 0;
	
	for(s=0; s < nSubObjCount; s++)
	{
		CStatObj::SSubObject *pSubObj = &m_subObjects[s];
		if (pSubObj->pStatObj && pSubObj->nType == STATIC_SUB_OBJECT_MESH && !pSubObj->bHidden)
		{
			CStatObj *pStatObj = (CStatObj*)pSubObj->pStatObj->GetLodObject(nLod,true); // Get lod, if not exist get lowest existing.
			if (pStatObj && pStatObj->m_pRenderMesh)
			{
				rmi.pMesh = pStatObj->m_pRenderMesh;
				rmi.mat = pSubObj->tm;
				lstRMI.Add(rmi);
				nTotalVertexCount += pStatObj->m_pRenderMesh->GetVertCount();
				nTotalTriCount += pStatObj->m_nRenderTrisCount;
			}
		}
	}
	if (nTotalVertexCount > MAX_VERTICES_MERGABLE || lstRMI.size() <= 1)
	{
		return false;
	}
	// Check if merged LOD is almost the same poly count as previous lod
	if (nLod > 0 && m_pMergedObject)
	{
		CStatObj *pPrevMergedLod = (CStatObj*)m_pMergedObject->GetLodObject(nLod-1);
		if (pPrevMergedLod)
		{
			// Get LOD0 triangle count.
			int nLod0TriCount = pPrevMergedLod->m_nRenderTrisCount;
			if (nTotalTriCount*MIN_TRIS_IN_MERGED_LOD_RAIO > nLod0TriCount)
			{
				return false;
			}
		}
	}

	if (!lstRMI.empty())
	{
		resultMeshes.clear();

		CRenderMeshMerger::SMergeInfo info;
		info.sMeshName = GetFilePath();
		info.sMeshType = "StatObj_Merged";
		info.bMergeToOneRenderMesh = true;
		info.pUseMaterial = m_pMaterial;
		GetSharedRenderMeshMerger()->MergeRenderMeshes( &lstRMI[0],lstRMI.size(),resultMeshes, info);
	}

	if (resultMeshes.empty())
	{
		// Nothing to merge.
		return false;
	}

	// Make merged stat object.
	_smart_ptr<CStatObj> pMergedStatObj = new CStatObj;
	pMergedStatObj->m_bForInternalUse = true;
	pMergedStatObj->m_szFileName = m_szFileName;
	pMergedStatObj->m_szFolderName = m_szFolderName;

	IRenderMesh *pMergedMesh = resultMeshes[0].pMesh;
	pMergedMesh->SetMaterial( m_pMaterial );
	pMergedStatObj->SetRenderMesh(pMergedMesh);
	pMergedStatObj->m_bMerged = true;
	pMergedStatObj->SetMaterial( m_pMaterial );
	pMergedStatObj->m_vBoxMin = m_vBoxMin;
	pMergedStatObj->m_vBoxMax = m_vBoxMax;
	pMergedStatObj->CalcRadiuses();

	if (nLod == 0)
	{
		m_pMergedObject = pMergedStatObj;
	}
	else if (m_pMergedObject)
	{
		m_pMergedObject->SetLodObject(nLod,pMergedStatObj);
	}

	m_bMerged = true;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::UnMergeSubObjectsRenderMeshes()
{
	if (m_bMerged)
	{
		if (m_pLod0)
		{
			// If called on LOD.
			m_pLod0->UnMergeSubObjectsRenderMeshes();
		}
		else
		{
			m_bMerged = false;
			m_pMergedObject = 0;
			SetRenderMesh(0);
			for (int i = 1; i < m_nLoadedLodsNum; i++)
			{
				CStatObj *pStatObj = (CStatObj*)GetLodObject(i);
				if (pStatObj)
				{
					pStatObj->m_bMerged = false;
					pStatObj->SetRenderMesh(0);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CStatObj::SSubObject* CStatObj::FindSubObject( const char *sNodeName )
{
	uint32 numSubObjects = m_subObjects.size();
	for (uint32 i=0; i<numSubObjects; i++)
	{
		if (stricmp(m_subObjects[i].name.c_str(),sNodeName) == 0)
		{
			return &m_subObjects[i];
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IStatObj::SSubObject& CStatObj::AddSubObject( IStatObj *pSubObj )
{
	assert( pSubObj );
	SetSubObjectCount( m_subObjects.size()+1 );
	InitializeSubObject( m_subObjects[m_subObjects.size()-1] );
	m_subObjects[m_subObjects.size()-1].pStatObj = pSubObj;
	pSubObj->AddRef();
	return m_subObjects[m_subObjects.size()-1];
}

//////////////////////////////////////////////////////////////////////////
CStatObj::SSubObject* CStatObj::GetSubObject( int nIndex )
{
	if ( nIndex >= 0 && nIndex < (int)m_subObjects.size() )
		return &m_subObjects[nIndex];
	else
		return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::RemoveSubObject( int nIndex )
{
	if ( nIndex >= 0 && nIndex < (int)m_subObjects.size() )
	{
		SSubObject *pSubObj = &m_subObjects[nIndex];
		CStatObj *pChildObj = (CStatObj*)pSubObj->pStatObj;
		if (pChildObj)
		{
			if (!m_bSharesChildren)
				pChildObj->m_pParentObject = NULL;
			pChildObj->Release();
		}
		m_subObjects.erase( m_subObjects.begin()+nIndex );
		Invalidate(true);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::SetSubObjectCount( int nCount )
{
	// remove sub objects.
	while (m_subObjects.size() > nCount)
	{
		RemoveSubObject(m_subObjects.size()-1);
	}
	
	int i = m_subObjects.size();
	m_subObjects.resize( nCount );
	for(; i<nCount; i++) 
	{
		InitializeSubObject( m_subObjects[i] );
	}
	if (nCount > 0)
	{
		m_nFlags |= STATIC_OBJECT_COMPOUND;
	}
	Invalidate(true);
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::CopySubObject( int nToIndex,IStatObj *pFromObj,int nFromIndex )
{
	SSubObject *pSrcSubObj = pFromObj->GetSubObject(nFromIndex);
	if (!pSrcSubObj)
		return false;

	if (nToIndex >= (int)m_subObjects.size())
	{
		SetSubObjectCount(nToIndex+1);
		if (pFromObj==this)
			pSrcSubObj = pFromObj->GetSubObject(nFromIndex);
	}
	
	m_subObjects[nToIndex] = *pSrcSubObj;
	if (pSrcSubObj->pStatObj)
		pSrcSubObj->pStatObj->AddRef();

	Invalidate(true);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::GetPhysicalProperties( float &mass,float &density )
{
	mass = m_phys_mass;
	density = m_phys_density;
	if (mass < 0 && density < 0)
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::RayIntersection( SRayHitInfo &hitInfo,IMaterial *pCustomMtl )
{
	Vec3 vOut;
	// First check if ray intersect objects bounding box.
	if (!Intersect::Ray_AABB(hitInfo.inRay,AABB(m_vBoxMin,m_vBoxMax),vOut))
		return false;

	if (m_pRenderMesh)
	{
		bool bRes = CRenderMeshUtils::RayIntersection(m_pRenderMesh,hitInfo,pCustomMtl);
    if (bRes)
    {
      hitInfo.pStatObj = this;
      hitInfo.pRenderMesh = m_pRenderMesh;
    }
    return bRes;
	}
	else
	{
		SRayHitInfo hitOut;
		bool bAnyHit = false;
		float fMinDistance = FLT_MAX;
		for (int i = 0,num = m_subObjects.size(); i < num; i++)
		{
			if (m_subObjects[i].pStatObj && m_subObjects[i].nType == STATIC_SUB_OBJECT_MESH && !m_subObjects[i].bHidden)
			{
				Matrix34 invertedTM = m_subObjects[i].tm.GetInverted();

				SRayHitInfo hit = hitInfo;
				
				// Transform ray into sub-object local space.
				hit.inReferencePoint = invertedTM.TransformPoint(hit.inReferencePoint);
				hit.inRay.origin = invertedTM.TransformPoint(hit.inRay.origin);
				hit.inRay.direction = invertedTM.TransformVector(hit.inRay.direction);

				if (((CStatObj*)m_subObjects[i].pStatObj)->RayIntersection( hit,pCustomMtl ))
				{
					if (hit.fDistance < fMinDistance)
					{
            hitInfo.pStatObj = m_subObjects[i].pStatObj;
						bAnyHit = true;
						hitOut = hit;
					}
				}
			}
		}
		if (bAnyHit)
		{
			// Restore input ray/reference point.
			hitOut.inReferencePoint = hitInfo.inReferencePoint;
			hitOut.inRay = hitInfo.inRay;

			hitInfo = hitOut;
			return true;
		}
	}
	return false;
}

float CStatObj::GetOcclusionAmount()
{
	if(m_fOcclusionAmount<0)
	{
		PrintMessage("Computing occlusion value for: %s ... ", GetFilePath());

		float fHits=0;
		float fTests=0;

		Matrix34 objMatrix; objMatrix.SetIdentity();

		Vec3 vStep = Vec3(0.5f,0.5f,0.5f);

		Vec3 vClosestHitPoint;
		float fClosestHitDistance=1000;

		// y-rays
		for(float x=m_vBoxMin.x; x<=m_vBoxMax.x; x+=vStep.x)
		{
			for(float z=m_vBoxMin.z; z<=m_vBoxMax.z; z+=vStep.z)
			{
				Vec3 vStart	(x,m_vBoxMin.y,z);
				Vec3 vEnd		(x,m_vBoxMax.y,z);
				if(CObjManager::RayStatObjIntersection(this, objMatrix, GetMaterial(), vStart, vEnd, vClosestHitPoint, fClosestHitDistance, true))
					fHits++;				
				fTests++;
			}
		}

		// x-rays
		for(float y=m_vBoxMin.y; y<=m_vBoxMax.y; y+=vStep.y)
		{
			for(float z=m_vBoxMin.z; z<=m_vBoxMax.z; z+=vStep.z)
			{
				Vec3 vStart	(m_vBoxMin.x,y,z);
				Vec3 vEnd		(m_vBoxMax.x,y,z);
				if(CObjManager::RayStatObjIntersection(this, objMatrix, GetMaterial(), vStart, vEnd, vClosestHitPoint, fClosestHitDistance, true))
					fHits++;
				fTests++;
			}
		}

		// z-rays
		for(float y=m_vBoxMin.y; y<=m_vBoxMax.y; y+=vStep.y)
		{
			for(float x=m_vBoxMin.x; x<=m_vBoxMax.x; x+=vStep.x)
			{
				Vec3 vStart	(x,y,m_vBoxMax.z);
				Vec3 vEnd		(x,y,m_vBoxMin.z);
				if(CObjManager::RayStatObjIntersection(this, objMatrix, GetMaterial(), vStart, vEnd, vClosestHitPoint, fClosestHitDistance, true))
					fHits++;
				fTests++;
			}
		}

		m_fOcclusionAmount = fTests ? (fHits/fTests) : 0;

		PrintMessagePlus("[%.2f]", m_fOcclusionAmount);
	}

	return m_fOcclusionAmount;
}

void CStatObj::SetRenderMesh( IRenderMesh *pRM ) 
{ 
	if (pRM == m_pRenderMesh)
		return;

	if (m_pRenderMesh)
	{
		GetRenderer()->DeleteRenderMesh(m_pRenderMesh);
	}
	m_pRenderMesh = 0;

  m_pRenderMesh = pRM; 

  m_nRenderTrisCount = 0;
	m_nRenderMatIds = 0;
  if(m_pRenderMesh)
  {
    PodArray<CRenderChunk> *	pChunks = m_pRenderMesh->GetChunks();
    for(int nChunkId=0; nChunkId<pChunks->Count(); nChunkId++)
    {
      CRenderChunk *pChunk = pChunks->Get(nChunkId);
      if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
        continue;

			if (pChunk->nNumIndices > 0)
			{
				m_nRenderMatIds++;
				m_nRenderTrisCount += pChunk->nNumIndices/3;
			}
    }
		m_nLoadedTrisCount = m_nRenderTrisCount;
  }
}

void CStatObj::CheckUpdateObjectHeightmap()
{
  if(m_pHeightmap)
    return;

  PrintMessage("Computing object heightmap for: %s ... ", GetFilePath());

  m_nHeightmapSize = (int)(CLAMP(max(m_vBoxMax.x-m_vBoxMin.x,m_vBoxMax.y-m_vBoxMin.y)*16, 8, 256));
  m_pHeightmap = new float[m_nHeightmapSize*m_nHeightmapSize];
  memset(m_pHeightmap,0,sizeof(float)*m_nHeightmapSize*m_nHeightmapSize);

  float dx = max(0.001f, (m_vBoxMax.x-m_vBoxMin.x)/m_nHeightmapSize);
  float dy = max(0.001f, (m_vBoxMax.y-m_vBoxMin.y)/m_nHeightmapSize);

  Matrix34 objMatrix; objMatrix.SetIdentity();

  for(float x=m_vBoxMin.x+dx; x<m_vBoxMax.x-dx; x+=dx)
  {
    for(float y=m_vBoxMin.y+dy; y<m_vBoxMax.y-dy; y+=dy)
    {
      Vec3 vStart (x, y, m_vBoxMax.z);
      Vec3 vEnd   (x, y, m_vBoxMin.z);

      Vec3 vClosestHitPoint(0,0,0);
      float fClosestHitDistance = 1000000;

      if(CObjManager::RayStatObjIntersection(this, objMatrix, GetMaterial(),
        vStart, vEnd, vClosestHitPoint, fClosestHitDistance, false))
      {
        int nX = CLAMP(int((x-m_vBoxMin.x)/dx), 0, m_nHeightmapSize-1);
        int nY = CLAMP(int((y-m_vBoxMin.y)/dy), 0, m_nHeightmapSize-1);
        m_pHeightmap[nX*m_nHeightmapSize + nY] = vClosestHitPoint.z;
      }
    }
  }

  PrintMessagePlus("[%dx%d] done", m_nHeightmapSize, m_nHeightmapSize);
}


float CStatObj::GetObjectHeight(float x, float y)
{
  CheckUpdateObjectHeightmap();

  float dx = max(0.001f, (m_vBoxMax.x-m_vBoxMin.x)/m_nHeightmapSize);
  float dy = max(0.001f, (m_vBoxMax.y-m_vBoxMin.y)/m_nHeightmapSize);

  int nX = CLAMP(int((x-m_vBoxMin.x)/dx), 0, m_nHeightmapSize-1);
  int nY = CLAMP(int((y-m_vBoxMin.y)/dy), 0, m_nHeightmapSize-1);

  return m_pHeightmap[nX*m_nHeightmapSize + nY];
}

//////////////////////////////////////////////////////////////////////////
int CStatObj::CountChildReferences()
{
	// Check if it must be released.
	int nChildRefs = 0;
	for (int k = 0,numChilds = m_subObjects.size(); k < numChilds; k++)
	{
		IStatObj::SSubObject &so = m_subObjects[k];
		if (so.pStatObj)
			nChildRefs += ((CStatObj*)so.pStatObj)->m_nUsers-1; // 1 reference from parent should be ignored.
	}
	return nChildRefs;
}
