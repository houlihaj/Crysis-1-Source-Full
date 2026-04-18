////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_node.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: terrain node
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain_sector.h"
#include "terrain.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "VoxMan.h"
#include "Vegetation.h"
#include <AABBSV.h>
#include "LightEntity.h"
#include "CullBuffer.h"

CProcVegetPoolMan * CTerrainNode::m_pProcObjPoolMan = NULL;
SProcObjChunkPool * CTerrainNode::m_pProcObjChunkPool = NULL;

int nSectorDrawId = 0;

CTerrainNode * CTerrainNode::GetTexuringSourceNode(int nTexMML, eTexureType eTexType)
{
	CTerrainNode * pTexSourceNode = this;
	while((!m_nEditorDiffuseTex || (eTexType == ett_LM)) && pTexSourceNode->m_pParent)
	{
    if(eTexType == ett_Diffuse)
    {
		  if(pTexSourceNode->m_nTreeLevel < nTexMML || pTexSourceNode->m_nNodeTextureOffset<0)
			  pTexSourceNode = pTexSourceNode->m_pParent;
		  else
			  break;
    }
    else if(eTexType == ett_LM)
    {
      if(pTexSourceNode->m_nTreeLevel < nTexMML || pTexSourceNode->m_nNodeTextureOffset<0 /*|| pTexSourceNode->GetSectorSizeInHeightmapUnits()<TERRAIN_LM_TEX_RES*/)
        pTexSourceNode = pTexSourceNode->m_pParent;
      else
        break;
    }
    else 
      assert(0);
	
		assert(pTexSourceNode);
	}

	return pTexSourceNode;
}

CTerrainNode * CTerrainNode::GetReadyTexSourceNode(int nTexMML, eTexureType eTexType)
{
	Vec3 vSunDir = Get3DEngine()->GetSunDir().normalized();

	CTerrainNode * pTexSourceNode = this;
	while((!m_nEditorDiffuseTex || eTexType == ett_LM) && pTexSourceNode)
	{
		if(eTexType == ett_Diffuse)
		{
			if(pTexSourceNode->m_nTreeLevel < nTexMML || !pTexSourceNode->m_nNodeTexSet.nTex0)
				pTexSourceNode = pTexSourceNode->m_pParent;
			else
				break;
		}
		else if(eTexType == ett_LM)
		{
			if(pTexSourceNode->m_nTreeLevel < nTexMML || !pTexSourceNode->m_nNodeTexSet.nTex1)
				pTexSourceNode = pTexSourceNode->m_pParent;
			else
				break;
		}
	}

	return pTexSourceNode;
}

void CTerrainNode::RequestTextures()
{
  FUNCTION_PROFILER_3DENGINE;

  if(m_nRenderStackLevel)
    return;

  // check if diffuse need to be updated
  if( CTerrainNode * pCorrectDiffSourceNode = GetTexuringSourceNode(m_cNodeNewTexMML, ett_Diffuse) )
  {
//    if(	pCorrectDiffSourceNode != pDiffSourceNode )
    {
      while(pCorrectDiffSourceNode)
      {
        GetTerrain()->ActivateNodeTexture(pCorrectDiffSourceNode);
        pCorrectDiffSourceNode = pCorrectDiffSourceNode->m_pParent;
      }
    }
  }

  // check if lightmap need to be updated
  if( CTerrainNode * pCorrectLMapSourceNode = GetTexuringSourceNode(m_cNodeNewTexMML, ett_LM) )
  {
//    if(	pCorrectLMapSourceNode != pLMSourceNode )
    {
      while(pCorrectLMapSourceNode)
      {
        GetTerrain()->ActivateNodeTexture(pCorrectLMapSourceNode);
        pCorrectLMapSourceNode = pCorrectLMapSourceNode->m_pParent;
      }
    }
  }
}

// setup texture id and texgen parameters
void CTerrainNode::SetupTexturing(bool bMakeUncompressedForEditing)
{
  FUNCTION_PROFILER_3DENGINE;

  CheckLeafData();

	// find parent node containing requested texture lod
 	CTerrainNode * pTextureSourceNode = GetReadyTexSourceNode(m_cNodeNewTexMML, ett_Diffuse);

  if(!pTextureSourceNode) // at least root texture has to be loaded
    pTextureSourceNode = m_pTerrain->m_pParentNode;

	// set output texture id's
	m_nTexSet.nTex0 = (pTextureSourceNode && pTextureSourceNode->m_nNodeTexSet.nTex0) ? 
		pTextureSourceNode->m_nNodeTexSet.nTex0 : GetTerrain()->m_nDefTerrainTexId;
	m_nTexSet.nTex1 = (pTextureSourceNode && pTextureSourceNode->m_nNodeTexSet.nTex1) ? 
		pTextureSourceNode->m_nNodeTexSet.nTex1 : GetTerrain()->m_nWhiteTexId;

	if(pTextureSourceNode && pTextureSourceNode->m_nNodeTexSet.nTex0)
		m_nTexSet = pTextureSourceNode->m_nNodeTexSet;

	CheckLeafData();

	if(pTextureSourceNode)
	{ // set texture generation parameters for terrain diffuse map
		float fSectorSizeScale = 1.0f;
		
		if(GetTerrain()->m_TerrainTextureLayer[0].nSectorSizePixels)
			fSectorSizeScale -= 1.0f/(float)(GetTerrain()->m_TerrainTextureLayer[0].nSectorSizePixels);	// we don't use half texel border so we have to compensate 

    int nRL = m_nRenderStackLevel;

		float dCSS = fSectorSizeScale/(CTerrain::GetSectorSize()<<pTextureSourceNode->m_nTreeLevel);
		GetLeafData()->m_arrTexGen[nRL][0] = -dCSS*pTextureSourceNode->m_nOriginY;
		GetLeafData()->m_arrTexGen[nRL][1] = -dCSS*pTextureSourceNode->m_nOriginX;
		GetLeafData()->m_arrTexGen[nRL][2] = dCSS;

		// shift texture by 0.5 pixel
		if(float fTexRes = GetTerrain()->m_TerrainTextureLayer[0].nSectorSizePixels)
		{
			GetLeafData()->m_arrTexGen[nRL][0] += 0.5f/fTexRes;
			GetLeafData()->m_arrTexGen[nRL][1] += 0.5f/fTexRes;
		}
	}
}

float GetPointToBoxDistance(Vec3 vPos, AABB bbox)
{
	if(vPos.x>=bbox.min.x && vPos.x<=bbox.max.x)
	if(vPos.y>=bbox.min.y && vPos.y<=bbox.max.y)
	if(vPos.z>=bbox.min.z && vPos.z<=bbox.max.z)
		return 0; // inside

	float dy;
	if(vPos.y<bbox.min.y)
		dy = bbox.min.y-vPos.y;
	else if(vPos.y>bbox.max.y)
		dy = vPos.y-bbox.max.y;
	else
		dy = 0;

	float dx;
	if(vPos.x<bbox.min.x)
		dx = bbox.min.x-vPos.x;
	else if(vPos.x>bbox.max.x)
		dx = vPos.x-bbox.max.x;
	else
		dx = 0;

	float dz;
	if(vPos.z<bbox.min.z)
		dz = bbox.min.z-vPos.z;
	else if(vPos.z>bbox.max.z)
		dz = vPos.z-bbox.max.z;
	else
		dz = 0;

	return cry_sqrtf(dx*dx+dy*dy+dz*dz);
}

// Hierarchically check nodes visibility 
// Add visible sectors to the list of visible terrain sectors

bool CTerrainNode::CheckVis(bool bAllInside, bool bAllowRenderIntoCBuffer)
{
  FUNCTION_PROFILER_3DENGINE;

  m_cNewGeomMML = MML_NOT_SET;

  if( !bAllInside && !GetCamera().IsAABBVisible_EHM(m_boxHeigtmap, &bAllInside) )
    return false;

  // get distances
  m_arrfDistance[m_nRenderStackLevel] = GetPointToBoxDistance(GetCamera().GetPosition(), m_boxHeigtmap);

  if(m_arrfDistance[m_nRenderStackLevel]>GetCamera().GetFarPlane())
    return false; // too far

  // occlusion test (affects only static objects)
  if(GetObjManager()->IsBoxOccluded(m_boxHeigtmap,m_arrfDistance[m_nRenderStackLevel], &m_OcclusionTestClient, false, eoot_TERRAIN_NODE))
    return false;
/*
  if(!m_nRenderStackLevel && !Get3DEngine()->GetCoverageBuffer()->IsObjectVisible(m_boxHeigtmap, eoot_TERRAIN_NODE, m_arrfDistance[m_nRenderStackLevel]))
  {
    return false;
  }
*/
	// find LOD of this sector 
	SetLOD();

  m_nSetLodFrameId = GetMainFrameID();

  int nSectorSize = CTerrain::GetSectorSize()<<m_nTreeLevel;

  bool bContinueRecursion = false;
  if(m_arrChilds[0] && 
    (m_arrfDistance[m_nRenderStackLevel]<nSectorSize || 
    m_cNewGeomMML<m_nTreeLevel || 
    m_cNodeNewTexMML<m_nTreeLevel || 
    m_bMergeNotAllowed))
    bContinueRecursion = true;

  if(m_arrfDistance[m_nRenderStackLevel] > GetCVars()->e_cbuffer_terrain_max_distance || m_bHasHoles || m_bNoOcclusion)
    bAllowRenderIntoCBuffer = false;

/* 
  { // for MichaelK
    bAllowRenderIntoCBuffer = false;
    Get3DEngine()->GetCoverageBuffer()->AddHeightMap(m_rangeInfo,
      m_boxHeigtmap.min.x, m_boxHeigtmap.min.y, m_boxHeigtmap.max.x, m_boxHeigtmap.max.y);
  }
*/

  if(bAllowRenderIntoCBuffer && 
    (!bContinueRecursion || //nSectorSize==128 || 
    nSectorSize < GetCVars()->e_cbuffer_terrain_lod_ratio*m_arrfDistance[m_nRenderStackLevel]))
  {
    bAllowRenderIntoCBuffer = false;

    if(!m_pCBRenderMesh)
    {
      int nTmp = m_cNewGeomMML;

      m_cNewGeomMML = m_nTreeLevel + GetCVars()->e_cbuffer_terrain_lod_shift;

      BuildIndices(GetTerrain()->m_StripsInfo, NULL, true);

      int nStep = (1<<m_cNewGeomMML)*CTerrain::GetHeightMapUnitSize();

      BuildVertices(nStep, true);   

      CStripsInfo * pSI = &(GetTerrain()->m_StripsInfo);

      assert(pSI->strip_info.Count() == 1);

      m_pCBRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
        GetTerrain()->m_lstTmpVertArray.GetElements(), GetTerrain()->m_lstTmpVertArray.Count(), VERTEX_FORMAT_P3F_N4B_COL4UB, NULL, 0,
        R_PRIMV_MULTI_STRIPS, "TerrainSector","TerrainSector", eBT_Static, pSI->strip_info.Count(), 
        m_nTexSet.nTex0, NULL, NULL, false, false, NULL);    

      m_pCBRenderMesh->SetBBox(m_boxHeigtmap.min,m_boxHeigtmap.max);

      m_pCBRenderMesh->UpdateSysIndices(pSI->idx_array.GetElements(), pSI->idx_array.Count());

      m_pCBRenderMesh->SetChunk(GetTerrain()->m_pTerrainEf,0,m_pCBRenderMesh->GetVertCount(),
        pSI->strip_info[0].begin, pSI->strip_info[0].end - pSI->strip_info[0].begin, 0);

      m_pCBRenderMesh->SetMaterial(GetTerrain()->m_pTerrainEf, m_nTexSet.nTex0);

      m_cNewGeomMML = nTmp;
    }

    Matrix34 mat;
    mat.SetIdentity();
    mat.SetTranslation(Vec3(0, 0, -GetCVars()->e_cbuffer_terrain_elevation_shift*(1<<m_nTreeLevel)));

    //if((nTestId&31) == (GetMainFrameID()&31))
    Get3DEngine()->GetCoverageBuffer()->AddRenderMesh(m_pCBRenderMesh, &mat, m_pCBRenderMesh->GetMaterial(), true, false, false);

    if(GetCVars()->e_cbuffer_terrain==2)
    {
      int nColor = nSectorSize/64;
      ColorF col((nColor&1)!=0,(nColor&2)!=0,(nColor&4)!=0);
      DrawBBox(m_boxHeigtmap, col);
    }

    nSectorDrawId++;
  }

	if(bContinueRecursion)
	{
//		for(int i=0; i<4; i++)
	//		m_arrChilds[i]->CheckVis(bAllInside);

    Vec3 vCenter = m_boxHeigtmap.GetCenter();

    Vec3 vCamPos = GetCamera().GetPosition();

    int nFirst = 
      ((vCamPos.x > vCenter.x) ? 2 : 0) |
      ((vCamPos.y > vCenter.y) ? 1 : 0);

    m_arrChilds[nFirst  ]->CheckVis(bAllInside, bAllowRenderIntoCBuffer);
    m_arrChilds[nFirst^1]->CheckVis(bAllInside, bAllowRenderIntoCBuffer);
    m_arrChilds[nFirst^2]->CheckVis(bAllInside, bAllowRenderIntoCBuffer);
    m_arrChilds[nFirst^3]->CheckVis(bAllInside, bAllowRenderIntoCBuffer);
	}
	else
	{
		GetTerrain()->AddVisSector(this);

		if(m_boxHeigtmap.min.z<GetTerrain()->GetWaterLevel() && !m_nRenderStackLevel)
			if(m_arrfDistance[m_nRenderStackLevel]<GetTerrain()->m_fDistanceToSectorWithWater)
				GetTerrain()->m_fDistanceToSectorWithWater = m_arrfDistance[m_nRenderStackLevel];

    if(m_arrChilds[0])
      for(int i=0; i<4; i++)
        m_arrChilds[i]->SetChildsLod(m_cNewGeomMML);

		static ICVar *pTexturesStreamingIgnore = gEnv->pConsole->GetCVar("r_TexturesStreamingIgnore");

		if (pTexturesStreamingIgnore->GetIVal() == 0)
		{
    RequestTextures();
	}
	}

	// update procedural vegetation
  if(GetCVars()->e_proc_vegetation)
	  if(!m_nRenderStackLevel && !m_nTreeLevel && m_arrfDistance[m_nRenderStackLevel] < GetCVars()->e_proc_vegetation_max_view_distance)
		  GetTerrain()->ActivateNodeProcObj(this);

	return true;
}

CTerrainNode::CTerrainNode(int x1, int y1, int nNodeSize, CTerrainNode * pParent, bool bBuildErrorsTable) :
	m_nNodeTexSet(0,0),
	m_nTexSet(0,0),
	m_nNodeTextureOffset(-1),
  m_nNodeHMDataOffset(-1),
  m_pCBRenderMesh(0)
{
	m_pGeomErrors = new float[GetTerrain()->m_nUnitsToSectorBitShift]; // TODO: fix duplicated reallocation
  memset(m_pGeomErrors, 0, sizeof(float)*GetTerrain()->m_nUnitsToSectorBitShift);

	m_pProcObjPoolPtr = NULL;

	ZeroStruct(m_arrChilds);

	m_pReadStream = NULL;
	m_eTexStreamingStatus = ecss_NotLoaded;

	// flags
	m_bNoOcclusion =
	m_bProcObjectsReady =
	m_bMergeNotAllowed =
	m_bHasHoles = // sector has holes in the ground
	m_nEditorDiffuseTex = 
	m_bHasLinkedVoxel = 0; // for editor

	m_nOriginX=m_nOriginY=0; // sector origin
	m_nLastTimeUsed=0; // basically last time rendered
	ZeroStruct(m_OcclusionTestClient);

	uchar m_cNewGeomMML=m_cCurrGeomMML=m_cNewGeomMML_Min=m_cNewGeomMML_Max=m_cNodeNewTexMML=m_cNodeNewTexMML_Min=0; 

	m_nLightMaskFrameId=0;
	
	m_pLeafData = 0;

	m_nTreeLevel=0;
	
	ZeroStruct(m_nNodeTexSet); ZeroStruct(m_nTexSet);

	m_nNodeRenderLastFrameId=m_nNodeTextureLastUsedSec=0;
	m_boxHeigtmap.Reset();
	m_pParent = NULL;
	m_nSetupTexGensFrameId=0;


	m_cNewGeomMML = 100;
	m_nLastTimeUsed = (int)GetCurTimeSec() + 20;

	m_cNodeNewTexMML = 100;
	m_pParent = NULL; 

	m_pParent = pParent;

	m_nOriginX = x1;
	m_nOriginY = y1;

	m_rangeInfo.fOffset = 0;
	m_rangeInfo.fRange = 0;
	m_rangeInfo.nSize = 0;
	m_rangeInfo.pHMData = NULL;

	for(int iStackLevel=0; iStackLevel<MAX_RECURSION_LEVELS; iStackLevel++)
	{
		m_arrfDistance[iStackLevel] = 0.f;
	}

	if(nNodeSize == CTerrain::GetSectorSize())
	{
		memset(m_arrChilds,0,sizeof(m_arrChilds));
		m_nTreeLevel = 0;
/*		InitSectorBoundsAndErrorLevels(bBuildErrorsTable);

		static int tic=0;
		if((tic&511)==0)
		{
			PrintMessagePlus(".");								// to visualize progress
			PrintMessage("");
		}
		tic++;
*/
	}
	else
	{
		int nSize = nNodeSize / 2;
		m_arrChilds[0] = new CTerrainNode(x1			, y1			, nSize, this, bBuildErrorsTable);
		m_arrChilds[1] = new CTerrainNode(x1+nSize, y1			, nSize, this, bBuildErrorsTable);
		m_arrChilds[2] = new CTerrainNode(x1			, y1+nSize, nSize, this, bBuildErrorsTable);
		m_arrChilds[3] = new CTerrainNode(x1+nSize, y1+nSize, nSize, this, bBuildErrorsTable);
		m_nTreeLevel = m_arrChilds[0]->m_nTreeLevel+1;

		for(int i=0; i<GetTerrain()->m_nUnitsToSectorBitShift; i++)
		{
			m_pGeomErrors[i] = max(max(
				m_arrChilds[0]->m_pGeomErrors[i],
				m_arrChilds[1]->m_pGeomErrors[i]),max(
				m_arrChilds[2]->m_pGeomErrors[i],
				m_arrChilds[3]->m_pGeomErrors[i]));
		}

		m_boxHeigtmap.min = SetMaxBB();
		m_boxHeigtmap.max = SetMinBB();

		for(int nChild=0; nChild<4; nChild++)
		{
			m_boxHeigtmap.min.CheckMin(m_arrChilds[nChild]->m_boxHeigtmap.min);
			m_boxHeigtmap.max.CheckMax(m_arrChilds[nChild]->m_boxHeigtmap.max);
		}

		m_arrfDistance[m_nRenderStackLevel] = 2.f*CTerrain::GetTerrainSize();
		if(m_arrfDistance[m_nRenderStackLevel]<0)
			m_arrfDistance[m_nRenderStackLevel]=0;

		m_nLastTimeUsed = (uint)-1;//GetCurTimeSec() + 100;
	}

	int nSectorSize = CTerrain::GetSectorSize()<<m_nTreeLevel;
	assert( x1>=0 && y1>=0 && x1<CTerrain::GetTerrainSize() && y1<CTerrain::GetTerrainSize() );
	GetTerrain()->m_arrSecInfoPyramid[m_nTreeLevel][x1/nSectorSize][y1/nSectorSize] = this;

//	m_boxStatics = m_boxHeigtmap;
}

CTerrainNode::~CTerrainNode()
{
	UnloadNodeTexture(false);

	for(int nChild=0; nChild<4; nChild++)
		delete m_arrChilds[nChild];

	ReleaseHeightMapGeometry();

	RemoveProcObjects(false);

	m_bHasHoles = 0;

	delete m_pLeafData;
	m_pLeafData = NULL;

	delete [] m_rangeInfo.pHMData;
	m_rangeInfo.pHMData = NULL;

	delete [] m_pGeomErrors;
	m_pGeomErrors = NULL;

  int nSectorSize = CTerrain::GetSectorSize()<<m_nTreeLevel;
  assert( m_nOriginX<CTerrain::GetTerrainSize() && m_nOriginY<CTerrain::GetTerrainSize() );
  GetTerrain()->m_arrSecInfoPyramid[m_nTreeLevel][m_nOriginX/nSectorSize][m_nOriginY/nSectorSize] = NULL;
}

void CTerrainNode::SetSectorTexture(unsigned int nEditorDiffuseTex)
{
	FUNCTION_PROFILER_3DENGINE;

	// unload old diffuse texture
	if(!m_nEditorDiffuseTex && m_nNodeTexSet.nTex0)
		m_pTerrain->m_texCache[0].ReleaseTexture(m_nNodeTexSet.nTex0);

	// assign new diffuse texture
	m_nTexSet.nTex0  = m_nNodeTexSet.nTex0 = nEditorDiffuseTex;
	m_nTexSet.nTex1		= m_nNodeTexSet.nTex1;

	// disable texture streaming
	m_nEditorDiffuseTex = nEditorDiffuseTex;
}

void CTerrainNode::CheckNodeGeomUnload()
{
	FUNCTION_PROFILER_3DENGINE;

	int nSectorSize = CTerrain::GetSectorSize()<<m_nTreeLevel;

	float fDistanse = GetPointToBoxDistance(GetCamera().GetPosition(), m_boxHeigtmap)*m_fZoomFactor;

  int nTime = fastftol_positive(GetCurTimeSec());

  // support timer reset
  m_nLastTimeUsed = min(m_nLastTimeUsed, nTime);

	if(m_nLastTimeUsed < (nTime - 16) && fDistanse>512)
	{ // try to release vert buffer if not in use int time
		ReleaseHeightMapGeometry();
	}
}

void CTerrainNode::RemoveProcObjects(bool bRecursive)
{
	FUNCTION_PROFILER_3DENGINE;

	// remove procedurally placed objects
	if(m_bProcObjectsReady)
	{
		if(m_pProcObjPoolPtr)
		{
			m_pProcObjPoolPtr->ReleaseAllObjects();
			m_pProcObjPoolMan->ReleaseObject(m_pProcObjPoolPtr);
			m_pProcObjPoolPtr = NULL;
		}

		m_bProcObjectsReady = false;

		if(GetCVars()->e_terrain_log==3)
			PrintMessage("ProcObjects removed %d", GetSecIndex());
	}

	if(bRecursive)
		for(int i=0; i<4; i++)
			if(m_arrChilds[i])
				m_arrChilds[i]->RemoveProcObjects(bRecursive);
}

void CTerrainNode::SetChildsLod(int nNewGeomLOD)
{
  m_cNewGeomMML = nNewGeomLOD;
  m_nSetLodFrameId = GetMainFrameID();

	if(m_arrChilds[0])
  {
		for(int i=0; i<4; i++)
    {
      m_arrChilds[i]->m_cNewGeomMML = nNewGeomLOD;
      m_arrChilds[i]->m_nSetLodFrameId = GetMainFrameID();
    }
  }
}

int CTerrainNode::GetAreaLOD()
{
  int nResult = MML_NOT_SET;

  CTerrainNode * pNode = this;

  while(pNode)
  {
    if(pNode->m_nSetLodFrameId == GetMainFrameID())
    {
      nResult = pNode->m_cNewGeomMML;
      break;
    }

    pNode = pNode->m_pParent;
  }

  if(pNode && m_nSetLodFrameId != GetMainFrameID())
  {
    m_cNewGeomMML = nResult;
    m_nSetLodFrameId = GetMainFrameID();
  }

  return nResult;
}

void CTerrainNode::RenderNodeHeightmap()
{
	FUNCTION_PROFILER_3DENGINE;

	if(m_arrfDistance[m_nRenderStackLevel]<8) // make sure near sectors are always potentially visible
		m_nLastTimeUsed = fastftol_positive(GetCurTimeSec()); 

	m_nLastTimeUsed = fastftol_positive(GetCurTimeSec());

	if(!GetVisAreaManager()->IsOutdoorAreasVisible())
		return;

  if(GetCVars()->e_terrain_draw_this_sector_only)
	{
		if(
			GetCamera().GetPosition().x > m_boxHeigtmap.max.x || 
			GetCamera().GetPosition().x < m_boxHeigtmap.min.x ||
			GetCamera().GetPosition().y > m_boxHeigtmap.max.y || 
			GetCamera().GetPosition().y < m_boxHeigtmap.min.y )
			return;
	}

	bool bVoxObjectFound = false;
	if(m_bHasLinkedVoxel && GetCVars()->e_voxel)
		bVoxObjectFound = Get3DEngine()->m_pObjectsTree->FindTerrainSectorVoxObject(m_boxHeigtmap) != NULL;

  SetupTexturing();

	// render heightmap if it is not replaced by voxel area
	if(!bVoxObjectFound)
		RenderSector(0);

	if(GetCVars()->e_terrain_bboxes)
	{
		GetRenderer()->GetIRenderAuxGeom()->DrawAABB(m_boxHeigtmap,false,ColorB(255*((m_nTreeLevel&1)>0),255*((m_nTreeLevel&2)>0),255,255),eBBD_Faceted);
		if(GetCVars()->e_terrain_bboxes == 3 && m_rangeInfo.nSize)
			GetRenderer()->DrawLabel(m_boxHeigtmap.GetCenter(), 2, "%dx%d", m_rangeInfo.nSize, m_rangeInfo.nSize);
	}

	if(GetCVars()->e_detail_materials_debug)
	{
		int nLayersNum = 0;
		for(int i=0; i<m_lstSurfaceTypeInfo.Count(); i++)
		{
			if(m_lstSurfaceTypeInfo[i].HasRM() && m_lstSurfaceTypeInfo[i].pSurfaceType->HasMaterial())
				if(m_lstSurfaceTypeInfo[i].GetIndexCount())
					nLayersNum++;
		}

		if(nLayersNum>=GetCVars()->e_detail_materials_debug)
		{
			GetRenderer()->GetIRenderAuxGeom()->DrawAABB(m_boxHeigtmap,false,ColorB(255*((nLayersNum&1)>0),255*((nLayersNum&2)>0),255,255),eBBD_Faceted);
			GetRenderer()->DrawLabel(m_boxHeigtmap.GetCenter(),2,"%d", nLayersNum);
		}
	}
}

float CTerrainNode::GetSurfaceTypeAmount(Vec3 vPos, int nSurfType)
{
	float fUnitSize = GetTerrain()->GetHeightMapUnitSize();
	vPos *= 1.f/fUnitSize;

	int x1 = int(vPos.x);
	int y1 = int(vPos.y);
	int x2 = int(vPos.x+1);
	int y2 = int(vPos.y+1);
	
	float dx = vPos.x-x1;
	float dy = vPos.y-y1;

	float s00 = GetTerrain()->GetSurfaceTypeID((int)(x1*fUnitSize),(int)(y1*fUnitSize)) == nSurfType;
	float s01 = GetTerrain()->GetSurfaceTypeID((int)(x1*fUnitSize),(int)(y2*fUnitSize)) == nSurfType;
	float s10 = GetTerrain()->GetSurfaceTypeID((int)(x2*fUnitSize),(int)(y1*fUnitSize)) == nSurfType;
	float s11 = GetTerrain()->GetSurfaceTypeID((int)(x2*fUnitSize),(int)(y2*fUnitSize)) == nSurfType;

	if(s00 || s01 || s10 || s11)
	{
		float s0 = s00*(1.f-dy) + s01*dy;
		float s1 = s10*(1.f-dy) + s11*dy;
		float res = s0*(1.f-dx) + s1*dx;
		return res;
	}

	return 0;
}

bool CTerrainNode::CheckUpdateProcObjects()
{
	if(GetCVars()->e_terrain_draw_this_sector_only)
	{
		if(
			GetCamera().GetPosition().x > m_boxHeigtmap.max.x || 
			GetCamera().GetPosition().x < m_boxHeigtmap.min.x ||
			GetCamera().GetPosition().y > m_boxHeigtmap.max.y || 
			GetCamera().GetPosition().y < m_boxHeigtmap.min.y )
			return false;
	}

	if(m_bProcObjectsReady)
		return false;

	FUNCTION_PROFILER_3DENGINE;

#define g_rnd()	(((float)rand())/RAND_MAX)

	int nInstancesCounter = 0;

	srand(m_nOriginX+m_nOriginY);
	float nSectorSize = (float)(CTerrain::GetSectorSize()<<m_nTreeLevel);
	for( int nLayer=0; nLayer<m_lstSurfaceTypeInfo.Count(); nLayer++)
	{
		for( int g=0; g<m_lstSurfaceTypeInfo[nLayer].pSurfaceType->lstnVegetationGroups.Count(); g++)
		if(m_lstSurfaceTypeInfo[nLayer].pSurfaceType->lstnVegetationGroups[g]>=0)
		{
			int nGroupId = m_lstSurfaceTypeInfo[nLayer].pSurfaceType->lstnVegetationGroups[g];
			assert( nGroupId>=0 && nGroupId<GetObjManager()->m_lstStaticTypes.size() );
			if( nGroupId<0 || nGroupId>=GetObjManager()->m_lstStaticTypes.size() )
				continue;
			StatInstGroup * pGroup = &GetObjManager()->m_lstStaticTypes[nGroupId];
			if(!pGroup || !pGroup->GetStatObj() || pGroup->fSize<=0)
				continue;
			
			if (!CheckMinSpec(pGroup->minConfigSpec)) // Check min spec of this group.
				continue;

			assert(pGroup && pGroup->GetStatObj());

			if(pGroup->fDensity < 0.2f)
				pGroup->fDensity = 0.2f;

			float fMinX=(float)m_nOriginX;
			float fMinY=(float)m_nOriginY;
			float fMaxX=(float)(m_nOriginX+nSectorSize);
			float fMaxY=(float)(m_nOriginY+nSectorSize);

			for(float fX=fMinX; fX<fMaxX; fX+=pGroup->fDensity)
      {
				for(float fY=fMinY; fY<fMaxY; fY+=pGroup->fDensity)
				{
					Vec3 vPos(fX+(g_rnd()-0.5f)*pGroup->fDensity, fY+(g_rnd()-0.5f)*pGroup->fDensity, 0);
					vPos.x = CLAMP(vPos.x,fMinX,fMaxX);
					vPos.y = CLAMP(vPos.y,fMinY,fMaxY);

					// filtered surface type lockup
					float fSurfaceTypeAmount = GetSurfaceTypeAmount(vPos, m_lstSurfaceTypeInfo[nLayer].pSurfaceType->ucThisSurfaceTypeId);
					if(fSurfaceTypeAmount<=0.5)
						continue;

					float fScale = pGroup->fSize + (g_rnd()-0.5f)*pGroup->fSizeVar;
					if(fScale<=0)
						continue;

					vPos.z = GetTerrain()->GetZApr(vPos.x,vPos.y);
					if(vPos.z<pGroup->fElevationMin || vPos.z>pGroup->fElevationMax)
						continue;

					// check slope range
					if(pGroup->fSlopeMin!=0 || pGroup->fSlopeMax!=255)
					{
						int nStep = CTerrain::GetHeightMapUnitSize();
						int x = (int)fX;
						int y = (int)fY;

						// calculate surface normal
						float sx;
						if((x+nStep)<CTerrain::GetTerrainSize() && x>=nStep)
							sx = GetTerrain()->GetZ(x+nStep,y  ) - GetTerrain()->GetZ(x-nStep,y  );
						else
							sx = 0;

						float sy;
						if((y+nStep)<CTerrain::GetTerrainSize() && y>=nStep)
							sy = GetTerrain()->GetZ(x  ,y+nStep) - GetTerrain()->GetZ(x  ,y-nStep);
						else
							sy = 0;

						Vec3 vNormal = Vec3(-sx, -sy, nStep*2.0f);
						vNormal.NormalizeFast();

						float fSlope = (1-vNormal.z)*255;
						if(fSlope < pGroup->fSlopeMin || fSlope > pGroup->fSlopeMax)
							continue;
					}

					if( vPos.x<0 || vPos.x>=CTerrain::GetTerrainSize() || vPos.y<0 || vPos.y>=CTerrain::GetTerrainSize())
						continue;

					assert( pGroup);
					assert( pGroup->GetStatObj());

					if(pGroup->GetStatObj()->GetRadius()*fScale < GetCVars()->e_vegetation_min_size)
						continue; // skip creation of very small objects

					if(!m_pProcObjPoolPtr)
						m_pProcObjPoolPtr = m_pProcObjPoolMan->GetObject();

					CVegetation * pEnt = m_pProcObjPoolPtr->AllocateProcObject();				
					assert(pEnt);

					pEnt->SetScale(fScale);
					pEnt->m_vPos = vPos;

					pEnt->SetStatObjGroupId(nGroupId);
					
					pEnt->m_ucAngle = rand(); // keep fixed amount of rand() calls
					if(!pGroup->bRandomRotation)
						pEnt->m_ucAngle = 0;
					
					AABB aabb = pEnt->CalcBBox();

          pEnt->SetRndFlags(ERF_PROCEDURAL, true);

					pEnt->m_fWSMaxViewDist = pEnt->GetMaxViewDist(); // note: duplicated
				
					pEnt->UpdateRndFlags();

					pEnt->Physicalize();

          float fObjRadius = aabb.GetRadius();
          if(fObjRadius > MAX_VALID_OBJECT_VOLUME || !_finite(fObjRadius) || fObjRadius<=0)
          {
            Warning("CTerrainNode::CheckUpdateProcObjects: Object has invalid bbox: %s,%s, GetRadius() = %.2f",
              pEnt->GetName(), pEnt->GetEntityClassName(), fObjRadius);
          }

					Get3DEngine()->m_pObjectsTree->InsertObject(pEnt, aabb, fObjRadius, aabb.GetCenter());

					nInstancesCounter++;
					if(nInstancesCounter>=(MAX_PROC_OBJ_CHUNKS_NUM/MAX_PROC_OBJ_SECTORS_IN_CACHE)*MAX_PROC_OBJ_IN_CHUNK)
					{
						m_bProcObjectsReady = true;
						return true;
					}
				}
      }
		}
	}

	m_bProcObjectsReady = true;

//	GetISystem()->VTunePause();
	return true;
}

CVegetation * CProcObjSector::AllocateProcObject()
{
	FUNCTION_PROFILER_3DENGINE;

	// find pool id
	int nLastPoolId = m_nProcVegetNum/MAX_PROC_OBJ_IN_CHUNK;
	if(nLastPoolId>=m_ProcVegetChunks.Count())
	{
		//PrintMessage("CTerrainNode::AllocateProcObject: Sector(%d,%d) %d objects",m_nOriginX,m_nOriginY,m_nProcVegetNum);
		m_ProcVegetChunks.PreAllocate(nLastPoolId+1,nLastPoolId+1);
		SProcObjChunk*pChink = m_ProcVegetChunks[nLastPoolId] = CTerrainNode::m_pProcObjChunkPool->GetObject();

		// init objects
		for(int o=0; o<MAX_PROC_OBJ_IN_CHUNK; o++)
			pChink->m_pInstances[o].Init();
	}

	// find empty slot id and return pointer to it
	int nNextSlotInPool = m_nProcVegetNum - nLastPoolId*MAX_PROC_OBJ_IN_CHUNK;
	CVegetation * pObj = &(m_ProcVegetChunks[nLastPoolId]->m_pInstances)[nNextSlotInPool];
	m_nProcVegetNum++;
	return pObj;
}

void CProcObjSector::ReleaseAllObjects()
{
	for(int i=0; i<m_ProcVegetChunks.Count(); i++)
	{
		SProcObjChunk*pChink = m_ProcVegetChunks[i];
		for(int o=0; o<MAX_PROC_OBJ_IN_CHUNK; o++)
			pChink->m_pInstances[o].ShutDown();
		CTerrainNode::m_pProcObjChunkPool->ReleaseObject(m_ProcVegetChunks[i]);
	}
	m_ProcVegetChunks.Clear();
	m_nProcVegetNum = 0;
}

void CProcObjSector::GetMemoryUsage(ICrySizer*pSizer)
{
	pSizer->AddObject(this, sizeof(*this));

	pSizer->AddContainer(m_ProcVegetChunks);

	for(int i=0; i<m_ProcVegetChunks.Count(); i++)
		pSizer->AddObject(m_ProcVegetChunks[i], sizeof(CVegetation)*MAX_PROC_OBJ_IN_CHUNK);
}

CProcObjSector::~CProcObjSector()
{
	FUNCTION_PROFILER_3DENGINE;

	ReleaseAllObjects();
}

void SProcObjChunk::GetMemoryUsage(class ICrySizer * pSizer)
{
  pSizer->AddObject(this, sizeof(*this));

  if(m_pInstances)
    pSizer->AddObject(m_pInstances, sizeof(CVegetation)*MAX_PROC_OBJ_IN_CHUNK);
}

void CTerrainNode::IntersectTerrainAABB(const AABB & aabbBox, PodArray<CTerrainNode*> & lstResult)
{
	if(m_boxHeigtmap.IsIntersectBox(aabbBox))
	{
		lstResult.Add(this);
		if(m_arrChilds[0])
			for(int i=0; i<4; i++)
				m_arrChilds[i]->IntersectTerrainAABB(aabbBox, lstResult);
	}
}

/*
int CTerrainNode__Cmp_SSurfaceType_UnitsNum(const void* v1, const void* v2)
{
	SSurfaceType * p1 = *(SSurfaceType **)v1;
	SSurfaceType * p2 = *(SSurfaceType **)v2;

	assert(p1 && p2);

	if(p1->nUnitsNum > p2->nUnitsNum)
		return -1;
	else if(p1->nUnitsNum < p2->nUnitsNum)
		return 1;

	return 0;
}
*/
/*
int CTerrainNode__Cmp_TerrainSurfaceLayerAmount_UnitsNum(const void* v1, const void* v2)
{
	TerrainSurfaceLayerAmount * p1 = (TerrainSurfaceLayerAmount *)v1;
	TerrainSurfaceLayerAmount * p2 = (TerrainSurfaceLayerAmount *)v2;

	assert(p1 && p2);

	if(p1->nUnitsNum > p2->nUnitsNum)
		return -1;
	else if(p1->nUnitsNum < p2->nUnitsNum)
		return 1;

	return 0;
}
*/
void CTerrainNode::UpdateDetailLayersInfo(bool bRecursive)
{
	FUNCTION_PROFILER_3DENGINE;

	if(m_arrChilds[0])
	{ // accumulate surface info from childs
		if(bRecursive)
		{
			for(int nChild=0; nChild<4; nChild++)
				m_arrChilds[nChild]->UpdateDetailLayersInfo(bRecursive);
		}

		static PodArray<SSurfaceType*> lstChildsLayers; lstChildsLayers.Clear();
		for(int nChild=0; nChild<4; nChild++)
		{
			for(int i=0; i<m_arrChilds[nChild]->m_lstSurfaceTypeInfo.Count(); i++)
			{
				SSurfaceType * pType = m_arrChilds[nChild]->m_lstSurfaceTypeInfo[i].pSurfaceType;
				assert(pType);				
				if(lstChildsLayers.Find(pType) < 0 && pType->HasMaterial())
					lstChildsLayers.Add(pType);
			}
		}

		for(int i=0; i<m_lstSurfaceTypeInfo.Count(); i++)
			m_lstSurfaceTypeInfo[i].DeleteRenderMeshes(GetRenderer());
		m_lstSurfaceTypeInfo.Clear();

		m_lstSurfaceTypeInfo.PreAllocate(lstChildsLayers.Count());

		assert(lstChildsLayers.Count()<=MAX_TERRAIN_SURFACE_TYPES);

		for(int i=0; i<lstChildsLayers.Count(); i++)
		{
			SSurfaceTypeInfo si;
			si.pSurfaceType = lstChildsLayers[i];
			m_lstSurfaceTypeInfo.Add(si);
		}
	}
	else
	{ // find all used surface types
		int arrSurfaceTypesInSector[MAX_TERRAIN_SURFACE_TYPES];
		for(int i=0; i<MAX_TERRAIN_SURFACE_TYPES; i++)
			arrSurfaceTypesInSector[i] = 0;

    m_bHasHoles = false;

		for(int X=m_nOriginX; X<=m_nOriginX+CTerrain::GetSectorSize(); X+=CTerrain::GetHeightMapUnitSize())
		for(int Y=m_nOriginY; Y<=m_nOriginY+CTerrain::GetSectorSize(); Y+=CTerrain::GetHeightMapUnitSize())
		{
			uchar ucSurfaceTypeID = GetTerrain()->GetSurfaceTypeID(X,Y);
			if(STYPE_BIT_MASK == ucSurfaceTypeID)
				m_bHasHoles = true;
			arrSurfaceTypesInSector[ucSurfaceTypeID] ++;
		}

		for(int i=0; i<m_lstSurfaceTypeInfo.Count(); i++)
			m_lstSurfaceTypeInfo[i].DeleteRenderMeshes(GetRenderer());
		m_lstSurfaceTypeInfo.Clear();

		int nSurfCount=0;
		for(int i=0; i<MAX_TERRAIN_SURFACE_TYPES; i++)
			if(arrSurfaceTypesInSector[i])
		{
			SSurfaceTypeInfo si;
			si.pSurfaceType = &GetTerrain()->m_SSurfaceType[i];
			if(si.pSurfaceType->HasMaterial())
				nSurfCount++;
		}

		m_lstSurfaceTypeInfo.PreAllocate(nSurfCount);

		for(int i=0; i<MAX_TERRAIN_SURFACE_TYPES; i++)
			if(arrSurfaceTypesInSector[i])
		{
			SSurfaceTypeInfo si;
			si.pSurfaceType = &GetTerrain()->m_SSurfaceType[i];
			if(si.pSurfaceType->HasMaterial())
				m_lstSurfaceTypeInfo.Add(si);
		}
	}
}


void CTerrainNode::IntersectWithShadowFrustum(PodArray<CTerrainNode*> * plstResult, ShadowMapFrustum * pFrustum, const float fHalfGSMBoxSize )
{
	if( pFrustum && pFrustum->Intersect(m_boxHeigtmap))
	{
		if(m_arrChilds[0] && (m_boxHeigtmap.max.x-m_boxHeigtmap.min.x) > fHalfGSMBoxSize)
		{
			for(int i=0; m_arrChilds[0] && i<4; i++)
				m_arrChilds[i]->IntersectWithShadowFrustum(plstResult, pFrustum,fHalfGSMBoxSize);
		}
		else
			plstResult->Add(this);
	}
}

void CTerrainNode::GetTerrainAOTextureNodesInBox(const AABB & aabbBox, PodArray<CTerrainNode*> * plstResult)
{
  if(aabbBox.IsIntersectBox(m_boxHeigtmap) && GetCamera().IsAABBVisible_EM(aabbBox))
  {
    UpdateDistance();

    if( m_arrChilds[0] && m_boxHeigtmap.GetSize().x>256.f 
//      && m_arrfDistance[m_nRenderStackLevel]<m_boxHeigtmap.GetSize().x
      )
    {
      for(int i=0; m_arrChilds[0] && i<4; i++)
        m_arrChilds[i]->GetTerrainAOTextureNodesInBox(aabbBox, plstResult);
    }
    else
      plstResult->Add(this);
  }
}

void CTerrainNode::IntersectWithBox(const AABB & aabbBox, PodArray<CTerrainNode*> * plstResult)
{
	if(aabbBox.IsIntersectBox(m_boxHeigtmap))
	{
		for(int i=0; m_arrChilds[0] && i<4; i++)
			m_arrChilds[i]->IntersectWithBox(aabbBox, plstResult);

		plstResult->Add(this);
	}
}

CTerrainNode * CTerrainNode::FindMinNodeContainingBox(const AABB & aabbBox)
{
	if( aabbBox.min.x < m_boxHeigtmap.min.x-0.01f || aabbBox.min.y < m_boxHeigtmap.min.y-0.01f ||
			aabbBox.max.x > m_boxHeigtmap.max.x+0.01f || aabbBox.max.y > m_boxHeigtmap.max.y+0.01f )
			return NULL;

	if(m_arrChilds[0])
		for(int i=0; i<4; i++)
			if(CTerrainNode * pRes = m_arrChilds[i]->FindMinNodeContainingBox(aabbBox))
				return pRes;

	return this;
}

void CTerrainNode::UnloadNodeTexture(bool bRecursive)
{
	if(m_pReadStream)
		m_pReadStream->Wait();

	if(m_nNodeTexSet.nTex0 && !m_nEditorDiffuseTex)
		m_pTerrain->m_texCache[0].ReleaseTexture(m_nNodeTexSet.nTex0);

	if(m_nNodeTexSet.nTex1)
		m_pTerrain->m_texCache[1].ReleaseTexture(m_nNodeTexSet.nTex1);

	if(m_nNodeTexSet.nTex0 && GetCVars()->e_terrain_texture_streaming_debug==2)
		PrintMessage("Texture released %d, Level=%d", GetSecIndex(), m_nTreeLevel);

	m_nNodeTexSet = SSectorTextureSet(0,0);

	for(int i=0; m_arrChilds[i] && bRecursive && i<4; i++)
		m_arrChilds[i]->UnloadNodeTexture(bRecursive);

	m_eTexStreamingStatus = ecss_NotLoaded;
}

void CTerrainNode::UpdateDistance()
{
	m_arrfDistance[m_nRenderStackLevel] = GetPointToBoxDistance(GetCamera().GetPosition(), m_boxHeigtmap);
}

void CTerrainNode::GetMemoryUsage(ICrySizer*pSizer)
{
//	pSizer->GetResourceCollector().OpenBBox(m_boxHeigtmap.min,m_boxHeigtmap.max);

/*	{
		SIZER_COMPONENT_NAME(pSizer, "LightMap");
		m_LightMap.GetMemoryUsage(pSizer);
	}
	{
		SIZER_COMPONENT_NAME(pSizer, "AccMap");
		m_AccMap.GetMemoryUsage(pSizer);
	}*/

/*	int nSize=0;
	for(int nStatic=0; nStatic<ENTITY_LISTS_NUM; nStatic++)
		for(int i=0; i<m_lstEntities[nStatic].Count(); i++)
		{
			IRenderNode *pRNode = m_lstEntities[nStatic][i].pNode;			assert(pRNode);
			nSize+=pRNode->GetMemoryUsage();

			{
				IStatObj *pStatObj = pRNode->GetEntityStatObj(0);

				if(pStatObj)
					pStatObj->GetMemoryUsage(pSizer);
			}

			for(uint32 nSlot=0;;++nSlot)
			{
				ICharacterInstance *pChar = pRNode->GetEntityCharacter(nSlot);
			
				if(!pChar)
					break;

				if(pSizer->GetResourceCollector().AddResourceIfSuccessfulOpenDependencies(pChar->GetFilePath(),0xffffffff))
				{
					IMaterial *pMat = pChar->GetMaterial();

					pChar->GetMemoryUsage(pSizer);

					if(pMat)
					{
						pSizer->GetResourceCollector().AddResource(string(pMat->GetName())+".mtl",0xffffffff);
//							pMat->GetMemoryUsage(pSizer);			this we don't add because only some parts of the material might be used
					}

					pSizer->GetResourceCollector().CloseDependencies();
				}
			}
		}*/

	pSizer->AddObject(this,sizeof(*this)/*+nSize*/);

	{
		SIZER_COMPONENT_NAME(pSizer, "SurfaceTypeInfo");
		pSizer->AddContainer(m_lstSurfaceTypeInfo);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "HMData");
		pSizer->AddObject(m_rangeInfo.pHMData, m_rangeInfo.nSize*m_rangeInfo.nSize*sizeof(m_rangeInfo.pHMData[0]));
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "GeomErrors");
		pSizer->AddObject(m_pGeomErrors, GetTerrain()->m_nUnitsToSectorBitShift*sizeof(m_pGeomErrors[0]) );
	}

//	pSizer->GetResourceCollector().CloseBBox();
}

bool CTerrainNode::Render(const SRendParams &RendParams)
{
	// render only prepared nodes for now
	if(!GetLeafData() || !GetLeafData()->m_pRenderMesh)
	{
		// get distances
		m_arrfDistance[m_nRenderStackLevel] = GetPointToBoxDistance(GetCamera().GetPosition(), m_boxHeigtmap);
		SetLOD();

		int nSectorSize = CTerrain::GetSectorSize()<<m_nTreeLevel;
		while((1<<m_cNewGeomMML)*CTerrain::GetHeightMapUnitSize() < nSectorSize/64)
			m_cNewGeomMML ++;
	}

	if(GetCVars()->e_terrain_draw_this_sector_only)
	{
		if(
			GetCamera().GetPosition().x > m_boxHeigtmap.max.x || 
			GetCamera().GetPosition().x < m_boxHeigtmap.min.x ||
			GetCamera().GetPosition().y > m_boxHeigtmap.max.y || 
			GetCamera().GetPosition().y < m_boxHeigtmap.min.y )
			return false;
	}

	CheckLeafData();

	m_nRenderStackLevel++;

	RenderSector(RendParams.nTechniqueID);

	m_nRenderStackLevel--;

	m_nLastTimeUsed = fastftol_positive(GetCurTimeSec());
	
	return true;
}

/*bool CTerrainNode::CheckUpdateLightMap()
{
	Vec3 vSunDir = Get3DEngine()->GetSunDir().normalized();

	bool bUpdateIsRequired = m _nNodeTextureLastUsedFrameId>(GetMainFrameID()-30) &&
		(GetSectorSizeInHeightmapUnits()>=TERRAIN_LM_TEX_RES && m_nNodeTextureOffset>=0) && !m_nNodeTexSet.nTex1;

	if(!bUpdateIsRequired)
		return true; // allow to go next node

	int nLM_Finished_Size = 0;
  int nCount = 0;
	float fStartTime = GetCurAsyncTimeSec();
	while(!nLM_Finished_Size && (GetCurAsyncTimeSec()-fStartTime) < 0.001f*GetCVars()->e_terrain_lm_gen_ms_per_frame)
  {
    nLM_Finished_Size = DoLightMapIncrementaly(m_nNodeTexSet.nLM_X, m_nNodeTexSet.nLM_Y, 512);
    nCount++;
  }

	if(nLM_Finished_Size)
	{
		if(m_nNodeTexSet.nTex1)
			GetRenderer()->RemoveTexture(m_nNodeTexSet.nTex1);

		m_nNodeTexSet.nTex1 = GetRenderer()->DownLoadToVideoMemory((uchar*)m_LightMap.GetData(), nLM_Finished_Size, nLM_Finished_Size, 
			eTF_A8R8G8B8, eTF_A8R8G8B8, 0, false, FILTER_LINEAR);

		// Mark Vegetations Brightness As Un-compiled
		m_boxHeigtmap.min.z = max(0.f,m_boxHeigtmap.min.z);

		return true; // allow to go next node
	}

	return false; // more time is needed for this node
}*/

bool CTerrainNode::CheckUpdateDiffuseMap()
{
	if(!m_nNodeTexSet.nTex0 && m_eTexStreamingStatus != ecss_Ready)
	{ // make texture loading once per several frames
		CheckLeafData();

		if(m_eTexStreamingStatus == ecss_NotLoaded)
			StartSectorTexturesStreaming(Get3DEngine()->IsTerrainSyncLoad());
		
		if(m_eTexStreamingStatus != ecss_Ready)
			return false;

		return false;
	}

	return true; // allow to go next node
}

bool CTerrainNode::AssignTextureFileOffset(int16 * &pIndices, int16 & nElementsLeft)
{
	if(this)
		m_nNodeTextureOffset = *pIndices;

	pIndices++;
	nElementsLeft--;

	for(int i=0; i<4; i++)
	{
		if(*pIndices>=0)
		{
			if(!m_arrChilds[i])
				return false;

			if(!m_arrChilds[i]->AssignTextureFileOffset(pIndices, nElementsLeft))
				return false;
		}
		else
		{
			pIndices++;
			nElementsLeft--;
		}
	}

	m_bMergeNotAllowed = false;

	return true;
}

int SSurfaceTypeInfo::GetIndexCount() 
{
	int nCount=0;
	for(int i=0; i<6; i++)
		if(arrpRM[i])
			nCount += arrpRM[i]->GetSysIndicesCount();

	return nCount;

}

void SSurfaceTypeInfo::DeleteRenderMeshes(IRenderer * pRend)
{
	for(int i=0; i<6; i++)
	{
		pRend->DeleteRenderMesh(arrpRM[i]);
		arrpRM[i] = NULL;
	}
}

int CTerrainNode::GetSectorSizeInHeightmapUnits()
{ 
  int nSectorSize = CTerrain::GetSectorSize()<<m_nTreeLevel;
  return nSectorSize/CTerrain::GetHeightMapUnitSize();
}

SProcObjChunk::SProcObjChunk()
{
	m_pInstances = new CVegetation[MAX_PROC_OBJ_IN_CHUNK];
}

SProcObjChunk::~SProcObjChunk()
{
	delete [] m_pInstances;
}
