////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: check vis
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"
#include "terrain_sector.h"
#include "StatObj.h"
#include "ObjMan.h"
#include "CullBuffer.h"
#include "terrain_water.h"
#include "VisAreas.h"
#include "Vegetation.h"

SSurfaceType CTerrain::m_SSurfaceType[MAX_SURFACE_TYPES_COUNT];

extern int nSectorDrawId;

void CTerrain::AddVisSector(CTerrainNode * newsec)
{
	assert(newsec->m_cNewGeomMML<m_nUnitsToSectorBitShift);
  m_lstVisSectors.Add((CTerrainNode * )newsec);
}

void CTerrain::CheckVis( )
{
  FUNCTION_PROFILER_3DENGINE;

  // reopen texture file if needed, texture pack may be randomly closed by editor so automatic reopening used
  if(!m_nDiffTexIndexTableSize)
    OpenTerrainTextureFile(m_hdrDiffTexHdr, m_hdrDiffTexInfo, "terrain/cover.ctc", m_ucpDiffTexTmpBuffer, m_nDiffTexIndexTableSize);

  if(m_nRenderStackLevel==0)
    m_fDistanceToSectorWithWater = OCEAN_IS_VERY_FAR_AWAY;

  m_lstVisSectors.Clear();

  nSectorDrawId = 0;

  m_pParentNode->CheckVis(false, (GetCVars()->e_cbuffer_terrain!=0) && (GetCVars()->e_cbuffer!=0));

  if(m_nRenderStackLevel==0)
  {
    m_bOceanIsVisibe = (m_fDistanceToSectorWithWater != OCEAN_IS_VERY_FAR_AWAY) || !m_lstVisSectors.Count();

    if(m_fDistanceToSectorWithWater<0)
      m_fDistanceToSectorWithWater=0;

    if(!m_lstVisSectors.Count())
      m_fDistanceToSectorWithWater=0;

    m_fDistanceToSectorWithWater = max(m_fDistanceToSectorWithWater, (GetCamera().GetPosition().z-m_fOceanWaterLevel));
  }
}

int __cdecl CmpTerrainNodesImportance(const void* v1, const void* v2)
{
	CTerrainNode *p1 = *(CTerrainNode**)v1;
	CTerrainNode *p2 = *(CTerrainNode**)v2;

	// process textures in progress first
	bool bInProgress1 = p1->m_eTexStreamingStatus == ecss_InProgress;
	bool bInProgress2 = p2->m_eTexStreamingStatus == ecss_InProgress;
	if(bInProgress1 > bInProgress2)
		return -1;
	else if(bInProgress1 < bInProgress2)
		return 1;

	// process recently requested textures first
	if(p1->m_nNodeTextureLastUsedSec > p2->m_nNodeTextureLastUsedSec)
		return -1;
	else if(p1->m_nNodeTextureLastUsedSec < p2->m_nNodeTextureLastUsedSec)
		return 1;

	// move parents first
	float f1 = p1->m_nTreeLevel;
	float f2 = p2->m_nTreeLevel;
	if(f1 > f2)
		return -1;
	else if(f1 < f2)
		return 1;

	// move closest first
	f1 = (p1->m_arrfDistance[0]);
	f2 = (p2->m_arrfDistance[0]);
	if(f1 > f2)
		return 1;
	else if(f1 < f2)
		return -1;

	return 0;
}

int __cdecl CmpTerrainNodesDistance(const void* v1, const void* v2)
{
	CTerrainNode *p1 = *(CTerrainNode**)v1;
	CTerrainNode *p2 = *(CTerrainNode**)v2;

	float f1 = (p1->m_arrfDistance[0]);
	float f2 = (p2->m_arrfDistance[0]);
	if(f1 > f2)
		return 1;
	else if(f1 < f2)
		return -1;

	return 0;
}

void CTerrain::ActivateNodeTexture( CTerrainNode * pNode )
{
	if(pNode->m_nNodeTextureOffset < 0 || m_nRenderStackLevel)
		return;

	pNode->m_nNodeTextureLastUsedSec = (int)GetCurTimeSec();

	if(m_lstActiveTextureNodes.Find(pNode)<0)	
	{
		if(	!pNode->m_nNodeTexSet.nTex1 || !pNode->m_nNodeTexSet.nTex0 )
		{
			m_lstActiveTextureNodes.Add(pNode);
		}
	}
}

void CTerrain::ActivateNodeProcObj( CTerrainNode * pNode )
{
	if(m_lstActiveProcObjNodes.Find(pNode)<0)	
	{
		m_lstActiveProcObjNodes.Add(pNode);
	}
}

int CTerrain::GetNotReadyTextureNodesCount() 
{ 
	int nRes=0;
	for(int i=0; i<m_lstActiveTextureNodes.Count(); i++)
		if(m_lstActiveTextureNodes[i]->m_eTexStreamingStatus != ecss_Ready)
			nRes++;
	return nRes; 
}

void CTerrain::CheckNodesGeomUnload()
{
  FUNCTION_PROFILER_3DENGINE;

  if(!Get3DEngine()->m_bShowTerrainSurface)
    return;

  for(int n=0; n<32; n++)
  {
    static uint 
      nOldSectorsX=-1, nOldSectorsY=-1, nTreeLevel=-1;

    if(nTreeLevel > m_pParentNode->m_nTreeLevel)
      nTreeLevel = m_pParentNode->m_nTreeLevel;

    uint nTableSize = CTerrain::GetSectorsTableSize()>>nTreeLevel;
    assert(nTableSize);

    // x/y cycle
    nOldSectorsY++;
    if(nOldSectorsY >= nTableSize)
    {
      nOldSectorsY = 0;
      nOldSectorsX++;
    }

    if(nOldSectorsX >= nTableSize)
    {
      nOldSectorsX = 0;
      nTreeLevel++;
    }

    if(nTreeLevel > m_pParentNode->m_nTreeLevel)
      nTreeLevel = 0;

    if(CTerrainNode * pNode = m_arrSecInfoPyramid[nTreeLevel][nOldSectorsX][nOldSectorsY])
      pNode->CheckNodeGeomUnload();
  }
}

void CTerrain::UpdateNodesIncrementaly()
{
	FUNCTION_PROFILER_3DENGINE;

	// process textures
	if(m_nDiffTexIndexTableSize && m_lstActiveTextureNodes.Count() && m_texCache[0].Update())
	{
		m_texCache[1].Update();

		for(int i=0; i<m_lstActiveTextureNodes.Count(); i++)
			m_lstActiveTextureNodes[i]->UpdateDistance();

		// sort by importance
		qsort(m_lstActiveTextureNodes.GetElements(), m_lstActiveTextureNodes.Count(), 
			sizeof(m_lstActiveTextureNodes[0]), CmpTerrainNodesImportance);

		// release unimportant textures and make sure at least one texture is free for possible loading
		while(m_lstActiveTextureNodes.Count()>TERRAIN_TEX_CACHE_POOL_SIZE-1)
		{
			m_lstActiveTextureNodes.Last()->UnloadNodeTexture(false);
			m_lstActiveTextureNodes.DeleteLast();
		}

		// load one sector if needed
		assert(m_texCache[0].m_FreeTextures.Count());
		for(int i=0; i<m_lstActiveTextureNodes.Count(); i++)
			if(!m_lstActiveTextureNodes[i]->CheckUpdateDiffuseMap())
        if(!Get3DEngine()->IsTerrainSyncLoad())
				  break;

		if(GetCVars()->e_terrain_texture_streaming_debug>=2)
		{
			for(int i=0; i<m_lstActiveTextureNodes.Count(); i++)
			{
				CTerrainNode * pNode = m_lstActiveTextureNodes[i];
				if(pNode->m_nTreeLevel <= GetCVars()->e_terrain_texture_streaming_debug-2)
					switch(pNode->m_eTexStreamingStatus)
				{
					case ecss_NotLoaded:
						DrawBBox(pNode->GetBBoxVirtual(), Col_Red);
						break;
					case ecss_InProgress:
						DrawBBox(pNode->GetBBoxVirtual(), Col_Green);
						break;
					case ecss_Ready:
						DrawBBox(pNode->GetBBoxVirtual(), Col_Blue);
						break;
				}
			}
		}
	}

	// process procedural objects
	if(m_lstActiveProcObjNodes.Count())
	{
    if(!CTerrainNode::GetProcObjChunkPool())
    {
      CTerrainNode::SetProcObjChunkPool(new SProcObjChunkPool);
      CTerrainNode::SetProcObjPoolMan(new CProcVegetPoolMan);
    }

		// make sure distances are correct
		for(int i=0; i<m_lstActiveProcObjNodes.Count(); i++)
			m_lstActiveProcObjNodes[i]->UpdateDistance();

		// sort by distance
		qsort(m_lstActiveProcObjNodes.GetElements(), m_lstActiveProcObjNodes.Count(), 
			sizeof(m_lstActiveProcObjNodes[0]), CmpTerrainNodesDistance);

		// release unimportant sectors
		static int nMaxSectors = MAX_PROC_OBJ_SECTORS_IN_CACHE;
		while(m_lstActiveProcObjNodes.Count() > (GetCVars()->e_proc_vegetation ? nMaxSectors : 0))
		{
			m_lstActiveProcObjNodes.Last()->RemoveProcObjects(false);
			m_lstActiveProcObjNodes.DeleteLast();
		}

    while(1)
    {
		  // release even more if we are running out of memory
		  while(m_lstActiveProcObjNodes.Count())
		  {
			  int nAll=0;
			  int nUsed = CTerrainNode::GetProcObjChunkPool()->GetUsedInstancesCount(nAll);
			  if(nAll - nUsed > (MAX_PROC_OBJ_CHUNKS_NUM/MAX_PROC_OBJ_SECTORS_IN_CACHE)) // make sure at least X chunks are free and ready to be used in this frame
				  break;

			  m_lstActiveProcObjNodes.Last()->RemoveProcObjects(false);
			  m_lstActiveProcObjNodes.DeleteLast();

			  nMaxSectors = min(nMaxSectors, m_lstActiveProcObjNodes.Count());
		  }

		  // build most important not ready sector
      bool bAllDone = true;
		  for(int i=0; i<m_lstActiveProcObjNodes.Count(); i++)
      {
			  if(m_lstActiveProcObjNodes[i]->CheckUpdateProcObjects())
        {
          bAllDone = false;
          break;
        }
      }

      if(!Get3DEngine()->IsTerrainSyncLoad() || bAllDone)
        break;
    }

//    if(m_lstActiveProcObjNodes.Count())
  //    m_lstActiveProcObjNodes[0]->RemoveProcObjects(false);

		if(GetCVars()->e_proc_vegetation==2)
			for(int i=0; i<m_lstActiveProcObjNodes.Count(); i++)
				DrawBBox(m_lstActiveProcObjNodes[i]->GetBBoxVirtual(),
				m_lstActiveProcObjNodes[i]->IsProcObjectsReady() ? Col_Green : Col_Red);
	}
}

int CTerrain::RenderTerrainWater()
{
  FUNCTION_PROFILER_3DENGINE;

  if(!m_pOcean || !GetCVars()->e_water_ocean)
    return 0;

  m_pOcean->Render(m_nRenderStackLevel);

  m_pOcean->SetLastFov(GetCamera().GetFov());

  return 0;
}

void CTerrain::ApplyForceToEnvironment(Vec3 vPos, float fRadius, float fAmountOfForce)
{
  if(fRadius<=0)
    return;

  if(!_finite(vPos.x) || !_finite(vPos.y) || !_finite(vPos.z))
  {
    PrintMessage("Error: 3DEngine::ApplyForceToEnvironment: Undefined position passed to the function");
    return;
  }

  if (fRadius > 50.f)
    fRadius = 50.f;

  if(fAmountOfForce>1.f)
    fAmountOfForce = 1.f;

  if( (vPos.GetDistance(GetCamera().GetPosition()) > 50.f+fRadius*2.f ) || // too far
    vPos.z < (GetZApr(vPos.x, vPos.y)-1.f)) // under ground
    return;

	if(!Get3DEngine()->m_pObjectsTree)
		return;

	PodArray<SRNInfo> lstObjects;
	Get3DEngine()->m_pObjectsTree->MoveObjectsIntoList(&lstObjects, NULL);

	// TODO: support in octree

  // affect objects around
  for(int i=0; i<lstObjects.Count(); i++)
  if(lstObjects[i].pNode->GetRenderNodeType() == eERType_Vegetation)
  {
    CVegetation * pVegetation =  (CVegetation*)lstObjects[i].pNode;
		Vec3 vDist = pVegetation->GetPos() - vPos;
    float fDist = vDist.GetLength();
    if(fDist<fRadius && fDist>0.f)
    {
      float fMag = (1.f - fDist/fRadius) * fAmountOfForce;
      pVegetation->AddBending(vDist * (fMag/fDist));
    }
  }
}

Vec3 CTerrain::GetTerrainSurfaceNormal(Vec3 vPos, float fRange, bool bIncludeVoxels)
{ 
	fRange += 0.05f;
  Vec3 v1 = Vec3(vPos.x-fRange, vPos.y-fRange,  GetZApr(vPos.x-fRange , vPos.y-fRange , bIncludeVoxels));
  Vec3 v2 = Vec3(vPos.x+fRange, vPos.y,				  GetZApr(vPos.x+fRange , vPos.y        , bIncludeVoxels));
  Vec3 v3 = Vec3(vPos.x,			vPos.y+fRange,    GetZApr(vPos.x        , vPos.y+fRange , bIncludeVoxels));
  return (v2-v1).Cross(v3-v1).GetNormalized(); 
}

void CTerrain::GetMemoryUsage( class ICrySizer*pSizer)
{
  {
    SIZER_COMPONENT_NAME(pSizer, "SecInfoTable");
		for(int nTreeLevel = 0; nTreeLevel<TERRAIN_NODE_TREE_DEPTH && m_arrSecInfoPyramid[nTreeLevel].m_nSize; nTreeLevel++)
			m_arrSecInfoPyramid[nTreeLevel].GetMemoryUsage(pSizer);
  }

  if(CTerrainNode::GetProcObjPoolMan())
	{
		SIZER_COMPONENT_NAME(pSizer, "ProcVeget");
		CTerrainNode::GetProcObjPoolMan()->GetMemoryUsage(pSizer);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "TerrainNode");
		for(int nTreeLevel = 0; nTreeLevel<TERRAIN_NODE_TREE_DEPTH && m_arrSecInfoPyramid[nTreeLevel].m_nSize; nTreeLevel++)
			for(int x=0; x<CTerrain::GetSectorsTableSize()>>nTreeLevel; x++)
				for(int y=0; y<CTerrain::GetSectorsTableSize()>>nTreeLevel; y++)
					if (m_arrSecInfoPyramid[nTreeLevel][x][y])
						m_arrSecInfoPyramid[nTreeLevel][x][y]->GetMemoryUsage(pSizer);
	}

  int nSize = 0;
  nSize += sizeofVector(m_lstTmpVertArray);
  nSize += sizeofVector(m_lstVisSectors);

  if(m_pOcean)
    nSize += sizeof(*m_pOcean) + m_pOcean->GetMemoryUsage();

  nSize += m_TerrainTextureLayer[0].nSectorSizeBytes;

  pSizer->AddObject(this, nSize + sizeof(*this));
}

bool CTerrain::PreloadResources()
{
  FUNCTION_PROFILER_3DENGINE;

  return 0;
}

void CTerrain::GetObjects(PodArray<SRNInfo> * pLstObjects)
{
  // take out all objects in terrain 
/*	for(int nTreeLevel = 0; nTreeLevel<TERRAIN_NODE_TREE_DEPTH && m_arrSecInfoPyramid[nTreeLevel].m_nSize; nTreeLevel++)
	for(int x=0; x<CTerrain::GetSectorsTableSize()>>nTreeLevel; x++)
	for(int y=0; y<CTerrain::GetSectorsTableSize()>>nTreeLevel; y++)
  {
    CBasicArea * pSecInfo = m_arrSecInfoPyramid[nTreeLevel][x][y];  
    for(int nStatic=0; nStatic<2; nStatic++)
    {
      if(pLstObjects)
        pLstObjects->AddList(pSecInfo->m_lstEntities[nStatic]);
      pSecInfo->m_lstEntities[nStatic].Reset();        
    }
  }*/
}

int CTerrain::GetTerrainNodesAmount() 
{
//	((N pow l)-1)/(N-1)
#if defined(__GNUC__)
	uint64 amount = (uint64)0xaaaaaaaaaaaaaaaaULL;
#else
	uint64 amount = (uint64)0xaaaaaaaaaaaaaaaa;
#endif
	amount >>= (65-(m_pParentNode->m_nTreeLevel+1)*2);
	return (int)amount;
}

void CTerrain::GetVisibleSectorsInAABB(PodArray<struct CTerrainNode*> & lstBoxSectors, const AABB & boxBox)
{
	lstBoxSectors.Clear();
	for(int i=0; i<m_lstVisSectors.Count(); i++)
	{
		CTerrainNode * pNode = m_lstVisSectors[i];
		if(pNode->m_boxHeigtmap.IsIntersectBox(boxBox))
			lstBoxSectors.Add(pNode);
	}
}

void CTerrain::RegisterLightMaskInSectors(CDLight * pLight)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(pLight->m_Id >= 0 || 1);
	//assert(!pLight->m_Shader.m_pShader || !(pLight->m_Shader.m_pShader->GetLFlags() & LMF_DISABLE));

	// get intersected outdoor sectors
	static PodArray<CTerrainNode*> lstSecotors; lstSecotors.Clear();
	AABB aabbBox(pLight->m_Origin - Vec3(pLight->m_fRadius,pLight->m_fRadius,pLight->m_fRadius), 
		pLight->m_Origin + Vec3(pLight->m_fRadius,pLight->m_fRadius,pLight->m_fRadius));
	m_pParentNode->IntersectTerrainAABB(aabbBox, lstSecotors);

	// set lmask in all affected sectors
	for( int s=0; s<lstSecotors.Count(); s++)
		lstSecotors[s]->AddLightSource(pLight);
}

void CTerrain::IntersectWithShadowFrustum(PodArray<CTerrainNode*> * plstResult, ShadowMapFrustum * pFrustum)
{
	if(m_pParentNode)
	{
		float fHalfGSMBoxSize = 0.5f / (pFrustum->fFrustrumSize * GetCVars()->e_gsm_range);

		m_pParentNode->IntersectWithShadowFrustum(plstResult, pFrustum, fHalfGSMBoxSize);
	}
}

void CTerrain::IntersectWithBox(const AABB & aabbBox, PodArray<CTerrainNode*> * plstResult)
{
	if(m_pParentNode)
		m_pParentNode->IntersectWithBox(aabbBox, plstResult);
}

void CTerrain::MarkAllSectorsAsUncompiled()
{
	if(m_pParentNode)
		m_pParentNode->RemoveProcObjects(true);

	if(Get3DEngine()->m_pObjectsTree)
		Get3DEngine()->m_pObjectsTree->MarkAsUncompiled();
}

void CTerrain::SetHeightMapMaxHeight(float fMaxHeight) 
{
	m_fHeightmapZRatio = 1.f/(256.f*256.f)*fMaxHeight; 
	
	InitHeightfieldPhysics();
}
/*
void CTerrain::RenaderImposterContent(class CREImposter * pImposter, const CCamera & cam)
{
	pImposter->GetTerrainNode()->RenderImposterContent(pImposter, cam);
}
*/

void SetTerrain( CTerrain &rTerrain );


void CTerrain::SerializeTerrainState( TSerialize ser )
{
	ser.BeginGroup("TerrainState");

	m_StoredModifications.SerializeTerrainState(ser);

	ser.EndGroup();
}

CTerrainNode * CTerrain::FindMinNodeContainingBox(const AABB & someBox)
{
	FUNCTION_PROFILER_3DENGINE;

	return m_pParentNode ? m_pParentNode->FindMinNodeContainingBox(someBox) : NULL;
/*
	static PodArray<CTerrainNode*> lstTerrainSectors; lstTerrainSectors.Clear();
	static PodArray<CTerrainNode*> lstTerrainSectorsResult; lstTerrainSectorsResult.Clear();
	IntersectWithBox(someBox,&lstTerrainSectors);

	for(int i=0; i<lstTerrainSectors.Count(); i++)
	{
		CTerrainNode * pNode = lstTerrainSectors[i];

		if(pNode->m_boxHeigtmap.max.x > someBox.max.x)
		if(pNode->m_boxHeigtmap.max.y > someBox.max.y)
		if(pNode->m_boxHeigtmap.min.x < someBox.min.x)
		if(pNode->m_boxHeigtmap.min.y < someBox.min.y)
		if(pNode->m_nTreeLevel >= GetTerrain()->m_nDiffTexTreeLevelOffset)
			lstTerrainSectorsResult.Add(pNode);
	}

	if(lstTerrainSectorsResult.Count())
		return lstTerrainSectorsResult[0];

	return NULL;*/
}

int CTerrain::GetTerrainLightmapTexId( Vec4 & vTexGenInfo )
{
	AABB nearWorldBox;
	float fBoxSize = 512;
	nearWorldBox.min = GetCamera().GetPosition() - Vec3(fBoxSize,fBoxSize,fBoxSize);
	nearWorldBox.max = GetCamera().GetPosition() + Vec3(fBoxSize,fBoxSize,fBoxSize);
	
	CTerrainNode * pNode = m_pParentNode->FindMinNodeContainingBox(nearWorldBox);
	
	if(pNode)
		pNode = pNode->GetTexuringSourceNode(0, ett_LM);
	else
		pNode = m_pParentNode;

	vTexGenInfo.x = pNode->m_nOriginX;
	vTexGenInfo.y = pNode->m_nOriginY;
	vTexGenInfo.z = (CTerrain::GetSectorSize()<<pNode->m_nTreeLevel);
	vTexGenInfo.w = 512.0f;
	return (pNode && pNode->m_nNodeTexSet.nTex1>0) ? pNode->m_nNodeTexSet.nTex1 : 0;
}

bool CTerrain::IsAmbientOcclusionEnabled() 
{ 
	return (m_hdrDiffTexInfo.dwFlags == TTFHF_AO_DATA_IS_VALID) && GetCVars()->e_ambient_occlusion; 
}
