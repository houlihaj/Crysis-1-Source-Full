////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_sector_render.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Create buffer, copy it into var memory, draw
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"
#include "terrain_sector.h"
#include "ObjMan.h"
#include "terrain_water.h"

PodArray<ushort> CTerrainNode::m_arrIndices[MAX_SURFACE_TYPES_COUNT][7];

void CTerrainNode::SetupTexGenParams(SSurfaceType * pSurfaceType, float * pOutParams, IMaterial::EProjAxis projAxis, bool bOutdoor, float fTexGenScale)
{
	assert(pSurfaceType);

	pSurfaceType->fMaxMatDistanceZ	= float(GetCVars()->e_detail_materials_view_dist_z  *Get3DEngine()->m_fTerrainDetailMaterialsViewDistRatio);
	pSurfaceType->fMaxMatDistanceXY = float(GetCVars()->e_detail_materials_view_dist_xy *Get3DEngine()->m_fTerrainDetailMaterialsViewDistRatio);

	// setup projection direction
	if(projAxis == IMaterial::ePA_XPos || projAxis == IMaterial::ePA_XNeg)
	{
		pOutParams[0] = 0;
		pOutParams[1] = pSurfaceType->fScale*fTexGenScale;
		pOutParams[2] = 0;
		pOutParams[3] = pSurfaceType->fMaxMatDistanceXY;

		pOutParams[4] = 0;
		pOutParams[5] = 0;
		pOutParams[6] = -pSurfaceType->fScale*fTexGenScale;
		pOutParams[7] = 0;
	}
	else if(projAxis == IMaterial::ePA_YPos || projAxis == IMaterial::ePA_YNeg)
	{
		pOutParams[0] = pSurfaceType->fScale*fTexGenScale;
		pOutParams[1] = 0;
		pOutParams[2] = 0;
		pOutParams[3] = pSurfaceType->fMaxMatDistanceXY;

		pOutParams[4] = 0;
		pOutParams[5] = 0;
		pOutParams[6] = -pSurfaceType->fScale*fTexGenScale;
		pOutParams[7] = 0;
	}
	else // Z
	{
		pOutParams[0] = pSurfaceType->fScale*fTexGenScale;
		pOutParams[1] = 0;
		pOutParams[2] = 0;
		pOutParams[3] = pSurfaceType->fMaxMatDistanceZ;

		pOutParams[4] = 0;
		pOutParams[5] = -pSurfaceType->fScale*fTexGenScale;
		pOutParams[6] = 0;
		pOutParams[7] = 0;
	}
}

void CTerrainNode::SetupTexGens()
{
	IRenderMesh * & pRenderMesh = GetLeafData()->m_pRenderMesh;

	if(pRenderMesh->GetChunks()->Count())
		for(int i=TERRAIN_BASE_TEXTURES_NUM; i<MAX_CUSTOM_TEX_BINDS_NUM; i++)
			pRenderMesh->GetChunks()->GetAt(0).pRE->m_CustomTexBind[i] = 0;

	pRenderMesh->SetMaterial(GetTerrain()->m_pTerrainEf, m_nTexSet.nTex0);

	if(pRenderMesh->GetChunks() && pRenderMesh->GetChunks()->Count() && pRenderMesh->GetChunks()->GetAt(0).pRE)
	{
		if(GetCVars()->e_terrain_texture_debug == 0)
			pRenderMesh->GetChunks()->GetAt(0).pRE->m_CustomTexBind[0] = m_nTexSet.nTex0;
		else if(GetCVars()->e_terrain_texture_debug == 1)
			pRenderMesh->GetChunks()->GetAt(0).pRE->m_CustomTexBind[0] = m_pTerrain->m_nDefTerrainTexId;
		else
			pRenderMesh->GetChunks()->GetAt(0).pRE->m_CustomTexBind[0] = 0;

		pRenderMesh->GetChunks()->GetAt(0).pRE->m_CustomTexBind[1] = m_nTexSet.nTex1;
	}
}

void CTerrainNode::DrawArray(int nTechniqueID)
{
//	if(m_nSetupTexGensFrameId < GetMainFrameID())
	{
		SetupTexGens();
		m_nSetupTexGensFrameId = GetMainFrameID();
	}

	IRenderMesh * & pRenderMesh = GetLeafData()->m_pRenderMesh;

	// make renderobject
	CRenderObject * pTerrainRenderObject = GetIdentityCRenderObject();
  if (!pTerrainRenderObject)
    return;
//	if(GetCVars()->e_hw_occlusion_culling_objects)
	//	pTerrainRenderObject->m_ObjFlags |= FOB_NO_Z_PASS;
	pTerrainRenderObject->m_pID = this;
	pTerrainRenderObject->m_II.m_AmbColor = Get3DEngine()->GetSkyColor();
  pTerrainRenderObject->m_fDistance = m_arrfDistance[m_nRenderStackLevel];

  uint nSectorDLMask = 0;

  PodArray<CDLight*> * pAffLight = GetAffectingLights();

	// make light mask for terrain sector
	if(!nTechniqueID)
    nSectorDLMask = Get3DEngine()->BuildLightMask( m_boxHeigtmap, pAffLight, NULL, true );

  // pass only sun to base terrain pass
  if(pAffLight && pAffLight->Count() && (pAffLight->GetAt(0)->m_Flags & DLF_SUN))
    pTerrainRenderObject->m_DynLMMask = 1;

	if(!nTechniqueID && GetCVars()->e_shadows)
		pTerrainRenderObject->m_ObjFlags |= FOB_INSHADOW;

	if(GetTerrain()->m_hdrDiffTexInfo.dwFlags == TTFHF_AO_DATA_IS_VALID && GetCVars()->e_terrain_normal_map)
	{ // provide normal map texture for terrain shader
		pTerrainRenderObject->m_nTextureID = m_nTexSet.nTex0;
		pTerrainRenderObject->m_nTextureID1 = m_nTexSet.nTex1;
		pTerrainRenderObject->m_fTempVars[0] = m_nTexSet.fTexOffsetX;
		pTerrainRenderObject->m_fTempVars[1] = m_nTexSet.fTexOffsetY;
		pTerrainRenderObject->m_fTempVars[2] = m_nTexSet.fTexScale;
		pTerrainRenderObject->m_fTempVars[3] = m_nTexSet.fTerrainMinZ;
		pTerrainRenderObject->m_fTempVars[4] = m_nTexSet.fTerrainMaxZ;
		pTerrainRenderObject->m_ObjFlags |= FOB_AMBIENT_OCCLUSION;
	}

	// render terrain base
  //if(m_arrfDistance[0]<64)
  //  pTerrainRenderObject->m_ObjFlags |= FOB_ONLY_Z_PASS;

 	pRenderMesh->AddRenderElements(pTerrainRenderObject, EFSLIST_GENERAL, 1, NULL, nTechniqueID);

	if(GetCVars()->e_detail_materials /*&& !m_nRenderStackLevel*/ && !nTechniqueID)
	{
		CRenderObject * pDetailObj = NULL;

		for(int i=0; i<m_lstSurfaceTypeInfo.Count(); i++)
		{
			assert(m_lstSurfaceTypeInfo[i].pSurfaceType->HasMaterial() && m_lstSurfaceTypeInfo[i].HasRM());

			if(!pDetailObj)
			{ // make object on first attempt to draw detail layer
				pDetailObj = GetIdentityCRenderObject();
        if (!pDetailObj)
          return;
				pDetailObj->m_pID = this;
        pDetailObj->m_ObjFlags |= (FOB_DETAILPASS | ((!m_nRenderStackLevel)? FOB_NO_FOG : 0)); // enable fog on recursive rendering (per-vertex)
				pDetailObj->m_II.m_AmbColor = Get3DEngine()->GetSkyColor();
				pDetailObj->m_DynLMMask = nSectorDLMask;
        pDetailObj->m_fDistance = m_arrfDistance[m_nRenderStackLevel];
				if(GetCVars()->e_shadows && pDetailObj->m_DynLMMask)
        {
					  pDetailObj->m_ObjFlags |= FOB_INSHADOW;
        }

        if(GetTerrain()->m_hdrDiffTexInfo.dwFlags == TTFHF_AO_DATA_IS_VALID && GetCVars()->e_terrain_normal_map)
        { // provide normal map texture for terrain shader
          pDetailObj->m_nTextureID = m_nTexSet.nTex0;
          pDetailObj->m_nTextureID1 = m_nTexSet.nTex1;
          pDetailObj->m_fTempVars[0] = m_nTexSet.fTexOffsetX;
          pDetailObj->m_fTempVars[1] = m_nTexSet.fTexOffsetY;
          pDetailObj->m_fTempVars[2] = m_nTexSet.fTexScale;
          pDetailObj->m_fTempVars[3] = m_nTexSet.fTerrainMinZ;
          pDetailObj->m_fTempVars[4] = m_nTexSet.fTerrainMaxZ;
          pDetailObj->m_ObjFlags |= FOB_AMBIENT_OCCLUSION;
        }
			}

			// test for Mikko
//			bool bUseTerrainLayerList = (GetCVars()->e_debug_mask & 0x1)!=0;

			IMaterial::EProjAxis projAxes[] = {IMaterial::ePA_XPos,IMaterial::ePA_YPos,IMaterial::ePA_ZPos,IMaterial::ePA_XNeg,IMaterial::ePA_YNeg,IMaterial::ePA_ZNeg};

			for(int p=0; p<6; p++)
			{
        if(SSurfaceType * pSurf = m_lstSurfaceTypeInfo[i].pSurfaceType)
				if(pSurf->GetMaterialOfProjection(projAxes[p]))
        {
          pSurf->fMaxMatDistanceZ	  = float(GetCVars()->e_detail_materials_view_dist_z  *Get3DEngine()->m_fTerrainDetailMaterialsViewDistRatio);
          pSurf->fMaxMatDistanceXY  = float(GetCVars()->e_detail_materials_view_dist_xy *Get3DEngine()->m_fTerrainDetailMaterialsViewDistRatio);

					if(m_arrfDistance[m_nRenderStackLevel]<pSurf->GetMaxMaterialDistanceOfProjection(projAxes[p]))
            if(IRenderMesh * pMesh = m_lstSurfaceTypeInfo[i].arrpRM[p])
            {
              AABB aabb;
              pMesh->GetBBox(aabb.min, aabb.max);
              if(GetCamera().IsAABBVisible_F(aabb))
						    if(pMesh->GetVertexContainer() == pRenderMesh && pMesh->GetSysIndicesCount())
                {
                  if(GetCVars()->e_terrain_bboxes==2)
                    GetRenderer()->GetIRenderAuxGeom()->DrawAABB(aabb,false,ColorB(255*((m_nTreeLevel&1)>0),255*((m_nTreeLevel&2)>0),0,255),eBBD_Faceted);

							    pMesh->AddRenderElements(pDetailObj, EFSLIST_TERRAINLAYER, 1);
                }
            }
        }
			}
		}
	}
}

// update data in video buffer
void CTerrainNode::UpdateRenderMesh(CStripsInfo * pArrayInfo, bool bUpdateVertices)
{
	FUNCTION_PROFILER_3DENGINE;

	IRenderMesh * & pRenderMesh = GetLeafData()->m_pRenderMesh;

	// if changing lod or there is no buffer allocated - reallocate
	if(!pRenderMesh || m_cCurrGeomMML != m_cNewGeomMML || bUpdateVertices || GetCVars()->e_terrain_draw_this_sector_only)
	{ 
		if(pRenderMesh)
			GetRenderer()->DeleteRenderMesh(pRenderMesh);  

		assert(GetTerrain()->m_lstTmpVertArray.Count()<65536);

		static PodArray<SPipTangents> lstTangents; lstTangents.Clear();

		if(GetCVars()->e_default_material)
		{
			lstTangents.PreAllocate(GetTerrain()->m_lstTmpVertArray.Count(),GetTerrain()->m_lstTmpVertArray.Count());

			Vec3 vBinorm(1,0,0); 
			vBinorm.Normalize();

			Vec3 vTang(0,1,0); 
			vTang.Normalize();

			Vec3 vNormal(0,0,1);

			vBinorm = -vNormal.Cross(vTang);
			vTang = vNormal.Cross(vBinorm);

			lstTangents[0].Binormal = Vec4sf(tPackF2B(vBinorm.x),tPackF2B(vBinorm.y),tPackF2B(vBinorm.z), tPackF2B(-1));
			lstTangents[0].Tangent  = Vec4sf(tPackF2B(vTang.x),tPackF2B(vTang.y),tPackF2B(vTang.z), tPackF2B(-1));

			for(int i=1; i<lstTangents.Count(); i++)
				lstTangents[i] = lstTangents[0];
		}

		pRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
			GetTerrain()->m_lstTmpVertArray.GetElements(), GetTerrain()->m_lstTmpVertArray.Count(), VERTEX_FORMAT_P3F_N4B_COL4UB, NULL, 0,
			R_PRIMV_TRIANGLES, "TerrainSector","TerrainSector", eBT_Static, pArrayInfo->strip_info.Count(), 
			m_nTexSet.nTex0, NULL, NULL, false, true, lstTangents.Count() ? lstTangents.GetElements() : NULL);    
		pRenderMesh->SetBBox(m_boxHeigtmap.min,m_boxHeigtmap.max);

    if(GetCVars()->e_terrain_log==1)
      PrintMessage("RenderMesh created %d", GetSecIndex());
	}

	int i; 

	pRenderMesh->UpdateSysIndices(pArrayInfo->idx_array.GetElements(), pArrayInfo->idx_array.Count());

	while(pRenderMesh->GetChunks()->Count()>pArrayInfo->strip_info.Count())
		pRenderMesh->GetChunks()->Delete(pRenderMesh->GetChunks()->Count()-1);

	int j=0;
	for( i=0; i<pArrayInfo->strip_info.Count(); i++)
	{
		if(pArrayInfo->strip_info[i].end - pArrayInfo->strip_info[i].begin < 3)
		{
			j++;
			continue;
		}

		pRenderMesh->SetChunk(GetTerrain()->m_pTerrainEf,0,pRenderMesh->GetVertCount(),
			pArrayInfo->strip_info[i].begin,
			pArrayInfo->strip_info[i].end - pArrayInfo->strip_info[i].begin, i-j, true);

		if(pRenderMesh->GetChunks()->Get(i-j)->pRE)
			pRenderMesh->GetChunks()->Get(i-j)->pRE->m_CustomData = GetLeafData()->m_arrTexGen[0];
	}

	assert(pRenderMesh->GetChunks()->Count() <= 256); // holes may cause more than 100 chunks
}

void CTerrainNode::UpdateRangeInfoShift()
{
  if(!m_arrChilds[0] || !m_nTreeLevel)
    return;

  m_rangeInfo.nUnitBitShift = GetTerrain()->m_nUnitsToSectorBitShift;

  for(int i=0; i<4; i++)
  {
    m_arrChilds[i]->UpdateRangeInfoShift();
    m_rangeInfo.nUnitBitShift = min(m_arrChilds[i]->m_rangeInfo.nUnitBitShift, m_rangeInfo.nUnitBitShift);
  }
}

void CTerrainNode::ReleaseHeightMapGeometry(bool bRecursive)
{
	for(int i=0; i<m_lstSurfaceTypeInfo.Count(); i++)
		m_lstSurfaceTypeInfo[i].DeleteRenderMeshes(GetRenderer());

	if(GetLeafData() && GetLeafData()->m_pRenderMesh)
	{
		IRenderMesh * & pRenderMesh = GetLeafData()->m_pRenderMesh;

		GetRenderer()->DeleteRenderMesh(pRenderMesh);  
		pRenderMesh = NULL;

		if (GetCVars()->e_terrain_log==1)
			PrintMessage("RenderMesh unloaded %d", GetSecIndex());
	}

	delete m_pLeafData;
	m_pLeafData = NULL;

  GetRenderer()->DeleteRenderMesh(m_pCBRenderMesh);  
  m_pCBRenderMesh = NULL;

  if (bRecursive && m_arrChilds[0])
    for(int i=0; i<4; i++)
      m_arrChilds[i]->ReleaseHeightMapGeometry(bRecursive);
}

// fill vertex buffer
void CTerrainNode::BuildVertices(int nStep, bool bSafetyBorder)
{
	FUNCTION_PROFILER_3DENGINE;

	GetTerrain()->m_lstTmpVertArray.Clear();

	int nSectorSize = CTerrain::GetSectorSize()<<m_nTreeLevel;

  int nSafetyBorder = bSafetyBorder ? nStep : 0;

  struct_VERTEX_FORMAT_P3F_N4B_COL4UB vert;   

	for( int x=m_nOriginX-nSafetyBorder; x<=m_nOriginX+nSectorSize+nSafetyBorder; x+=nStep )
  {
		for( int y=m_nOriginY-nSafetyBorder; y<=m_nOriginY+nSectorSize+nSafetyBorder; y+=nStep )
		{
      int _x = CLAMP(x, m_nOriginX, m_nOriginX+nSectorSize);
      int _y = CLAMP(y, m_nOriginY, m_nOriginY+nSectorSize);
			vert.xyz.x = (float)_x;
			vert.xyz.y = (float)_y;
			vert.xyz.z = (GetTerrain()->GetZ(_x,_y));    
			
			// close gaps on border of terrain for shadow mapping
			if(	vert.xyz.x <= 0 || vert.xyz.x >= GetTerrain()->GetTerrainSize() ||
					vert.xyz.y <= 0 || vert.xyz.y >= GetTerrain()->GetTerrainSize())
			  vert.xyz.z = 0;

      // safety borders are used for cbuffer, note: holes not supported here
      if(	x != _x || y != _y )
        vert.xyz.z = max(vert.xyz.z-2.f*nStep, 0.f);

			int iLookupRadius = nStep;

			if(x==m_nOriginX || x==m_nOriginX+nSectorSize || y==m_nOriginY || y==m_nOriginY+nSectorSize)
				iLookupRadius=1;

			// calculate surface normal
			float sx;
			if((x+iLookupRadius)<CTerrain::GetTerrainSize() && x>=iLookupRadius)
				sx = GetTerrain()->GetZ(x+iLookupRadius,y  ) - GetTerrain()->GetZ(x-iLookupRadius,y  );
			else
				sx = 0;

			float sy;
			if((y+iLookupRadius)<CTerrain::GetTerrainSize() && y>=iLookupRadius)
				sy = GetTerrain()->GetZ(x  ,y+iLookupRadius) - GetTerrain()->GetZ(x  ,y-iLookupRadius);
			else
				sy = 0;

			// z component of normal will be used as point brightness ( for burned terrain )
			Vec3 vNorm = Vec3(-sx, -sy, iLookupRadius*2.0f);
			vNorm.Normalize();

			vert.normal.bcolor[0] = (byte)(vNorm[0] * 127.5f + 128.0f);
      vert.normal.bcolor[1] = (byte)(vNorm[1] * 127.5f + 128.0f);
      vert.normal.bcolor[2] = (byte)(vNorm[2] * 127.5f + 128.0f);
      vert.normal.bcolor[3] = 255;
      SwapEndian(vert.normal.dcolor);

			uchar ucSurfaceTypeID = GetTerrain()->GetSurfaceTypeID(x,y);
			if(ucSurfaceTypeID == STYPE_HOLE)
			{ // in case of hole - try to find some valid surface type around
				for(int i=-nStep; i<=nStep && (ucSurfaceTypeID == STYPE_HOLE); i++)
					for(int j=-nStep; j<=nStep && (ucSurfaceTypeID == STYPE_HOLE); j++)
						ucSurfaceTypeID = GetTerrain()->GetSurfaceTypeID(x+i,y+j);
			}

			vert.color.bcolor[0] = 255;
			vert.color.bcolor[1] = ucSurfaceTypeID;
			vert.color.bcolor[2] = 255;
			vert.color.bcolor[3] = 255;
			SwapEndian(vert.color.dcolor);

			GetTerrain()->m_lstTmpVertArray.Add(vert);
		}
  }
}

void CTerrainNode::AddIndexAliased(int _x, int _y, int _step, int nSectorSize, PodArray<CTerrainNode*> * plstNeighbourSectors, CStripsInfo * pArrayInfo)
{
	int nAliasingX=1, nAliasingY=1;
	int nShiftX=0, nShiftY=0;

	if(_x && _x<nSectorSize && plstNeighbourSectors)
	{
		if(_y==0)
		{
			if(CTerrainNode * pNode = GetTerrain()->GetSecInfo(m_nOriginX+_x, m_nOriginY+_y-_step))
			{
        int nAreaMML = pNode->GetAreaLOD();
				if(nAreaMML != MML_NOT_SET)
				{
					nAliasingX = CTerrain::GetHeightMapUnitSize()*(1<<nAreaMML);
					nShiftX = nAliasingX/4;
				}
				if(plstNeighbourSectors->Find(pNode)<0) // todo: cache this list, it is always the same
					plstNeighbourSectors->Add(pNode);
			}
		}
		else if(_y==nSectorSize)
		{
			if(CTerrainNode * pNode = GetTerrain()->GetSecInfo(m_nOriginX+_x, m_nOriginY+_y+_step))
			{
        int nAreaMML = pNode->GetAreaLOD();
				if(nAreaMML != MML_NOT_SET)
				{
					nAliasingX = CTerrain::GetHeightMapUnitSize()*(1<<nAreaMML);
					nShiftX = nAliasingX/2;
				}
				if(plstNeighbourSectors->Find(pNode)<0)
					plstNeighbourSectors->Add(pNode);
			}
		}
	}

	if(_y && _y<nSectorSize && plstNeighbourSectors)
	{
		if(_x==0)
		{
			if(CTerrainNode * pNode = GetTerrain()->GetSecInfo(m_nOriginX+_x-_step, m_nOriginY+_y))
			{
        int nAreaMML = pNode->GetAreaLOD();
				if(nAreaMML != MML_NOT_SET)
				{
					nAliasingY = CTerrain::GetHeightMapUnitSize()*(1<<nAreaMML);
					nShiftY = nAliasingY/4;
				}
				if(plstNeighbourSectors->Find(pNode)<0)
					plstNeighbourSectors->Add(pNode);
			}
		}
		else if(_x==nSectorSize)
		{
			if(CTerrainNode * pNode = GetTerrain()->GetSecInfo(m_nOriginX+_x+_step, m_nOriginY+_y))
			{
        int nAreaMML = pNode->GetAreaLOD();
				if(nAreaMML != MML_NOT_SET)
				{
					nAliasingY = CTerrain::GetHeightMapUnitSize()*(1<<nAreaMML);
					nShiftY = nAliasingY/2;
				}
				if(plstNeighbourSectors->Find(pNode)<0)
					plstNeighbourSectors->Add(pNode);
			}
		}
	}

	int XA = nAliasingX ? (_x+nShiftX)/nAliasingX*nAliasingX : _x;
	int YA = nAliasingY ? (_y+nShiftY)/nAliasingY*nAliasingY : _y;

	assert(XA>=0 && XA<=nSectorSize);
	assert(YA>=0 && YA<=nSectorSize);

	pArrayInfo->AddIndex(XA,YA,_step,nSectorSize);
}

void CTerrainNode::BuildIndices(CStripsInfo & si, PodArray<CTerrainNode*> * pNeighbourSectors, bool bSafetyBorder)
{
	FUNCTION_PROFILER_3DENGINE;

	int nStep = (1<<m_cNewGeomMML)*CTerrain::GetHeightMapUnitSize();
	int nSectorSize = CTerrain::GetSectorSize()<<m_nTreeLevel;
	STerrainNodeLeafData * pLeafData = GetLeafData();

  int nSafetyBorder = bSafetyBorder ? nStep : 0;

  int nSectorSizeSB = nSectorSize+nSafetyBorder*2;

	si.Clear();

	// degenerative strips, compatible with any size neighbour sectors
	si.BeginStrip();
	for( int x=0; x<nSectorSizeSB; x+=nStep )
	{   
		int y;
		for( y=0; y<=nSectorSizeSB; y+=nStep )
		{
			bool bHole = m_bHasHoles && !m_cNewGeomMML && GetTerrain()->GetHole(m_nOriginX+x,m_nOriginY+y);
			if(!bHole && !si.bInStrip)
				si.BeginStrip();
			if(si.bInStrip)
			{
				AddIndexAliased(x,y,nStep,nSectorSizeSB,pNeighbourSectors,&si);
				AddIndexAliased(x+nStep,y,nStep,nSectorSizeSB,pNeighbourSectors,&si);
			}
			if(bHole && si.bInStrip)
				si.EndStrip();
		}

		bool bHole = m_bHasHoles && !m_cNewGeomMML && GetTerrain()->GetHole(m_nOriginX+x,m_nOriginY+nSectorSize);
		if(!bHole && !si.bInStrip)
			si.BeginStrip();
		if(si.bInStrip)
		{
			AddIndexAliased(x+nStep,nSectorSizeSB,nStep,nSectorSizeSB,pNeighbourSectors,&si);
			AddIndexAliased(x+nStep,nSectorSizeSB,nStep,nSectorSizeSB,pNeighbourSectors,&si);
		}
		if(bHole && si.bInStrip)
			si.EndStrip();

		x+=nStep;

		if(x<nSectorSizeSB)
		{
			for( y=nSectorSizeSB; y>=0; y-=nStep )
			{
				bool bHole = m_bHasHoles && !m_cNewGeomMML && GetTerrain()->GetHole(m_nOriginX+x,m_nOriginY+y-nStep);
				if(!bHole && !si.bInStrip)
					si.BeginStrip();
				if(si.bInStrip)
				{
					AddIndexAliased(x+nStep,y,nStep,nSectorSizeSB,pNeighbourSectors,&si);
					AddIndexAliased(x,y,nStep,nSectorSizeSB,pNeighbourSectors,&si);
				}
				if(bHole && si.bInStrip)
					si.EndStrip();
			}

			bool bHole = m_bHasHoles && !m_cNewGeomMML && GetTerrain()->GetHole(m_nOriginX+x,m_nOriginY);
			if(!bHole && !si.bInStrip)
				si.BeginStrip();
			if(si.bInStrip)
			{
				AddIndexAliased(x+nStep,0,nStep,nSectorSizeSB,pNeighbourSectors,&si);
				AddIndexAliased(x+nStep,0,nStep,nSectorSizeSB,pNeighbourSectors,&si);
			}
			if(bHole && si.bInStrip)
				si.EndStrip();
		}
	}

	if(si.bInStrip)
		si.EndStrip();

  // fix triangle back/front culling
  for( int i=0; i<si.strip_info.Count(); i++)
  {
    for(int t = si.strip_info[i].begin; (t+4) < si.strip_info[i].end; t += 6)
    {
      int tmp = si.idx_array[t+3];
      si.idx_array[t+3] = si.idx_array[t+4];
      si.idx_array[t+4] = tmp;
    }
  }
}

// entry point
void CTerrainNode::RenderSector(int nTechniqueID)
{
	FUNCTION_PROFILER_3DENGINE;

	m_nNodeRenderLastFrameId = GetMainFrameID();

	assert(m_nRenderStackLevel || m_cNewGeomMML<GetTerrain()->m_nUnitsToSectorBitShift);

	STerrainNodeLeafData * pLeafData = GetLeafData();

	// detect if any neighbors switched lod since previous frame
	bool bNeighbourChanged = false;
	if(!nTechniqueID)
	for(int i=0; i<pLeafData->m_lstNeighbourSectors.Count(); i++)
	{
    int nAreaMML = pLeafData->m_lstNeighbourSectors[i]->GetAreaLOD();
		if(nAreaMML != pLeafData->m_lstNeighbourLods[i])
		{
			bNeighbourChanged = true;
			break;
		}
	}

	IRenderMesh * & pRenderMesh = GetLeafData()->m_pRenderMesh;

	bool bDetailLayersReady = nTechniqueID ||
		!m_lstSurfaceTypeInfo.Count() ||
		m_lstSurfaceTypeInfo[0].HasRM() || 
		!GetCVars()->e_detail_materials ||
		!m_lstSurfaceTypeInfo[0].pSurfaceType ||
		!m_lstSurfaceTypeInfo[0].pSurfaceType->HasMaterial();

	// just draw if everything is up to date already
	if(pRenderMesh && GetCVars()->e_terrain_draw_this_sector_only<2 && bDetailLayersReady)
	{
		if( m_nRenderStackLevel || ( m_cCurrGeomMML == m_cNewGeomMML && !bNeighbourChanged ))
		{ 
			DrawArray(nTechniqueID); 
			return; 
		}
	}

	if(m_nRenderStackLevel)
		if(pRenderMesh)
			return;

	pLeafData->m_lstNeighbourSectors.Clear();
	pLeafData->m_lstNeighbourLods.Clear();

	// build indices and fill neighbor info
	BuildIndices(GetTerrain()->m_StripsInfo, &pLeafData->m_lstNeighbourSectors, false);

	// remember neighbor LOD's
	for(int i=0; i<pLeafData->m_lstNeighbourSectors.Count(); i++)
		pLeafData->m_lstNeighbourLods.Add(pLeafData->m_lstNeighbourSectors[i]->GetAreaLOD());

  int nStep = (1<<m_cNewGeomMML)*CTerrain::GetHeightMapUnitSize();
  int nSectorSize = CTerrain::GetSectorSize()<<m_nTreeLevel;
  assert(nStep && nStep<=nSectorSize);
  if(nStep > nSectorSize)
    nStep = nSectorSize;

	// update vertices if needed
  int nVertsNumRequired = (nSectorSize/nStep)*(nSectorSize/nStep)+(nSectorSize/nStep)+(nSectorSize/nStep)+1;
	if(!pRenderMesh || m_cCurrGeomMML != m_cNewGeomMML || nVertsNumRequired != pRenderMesh->GetVertCount() 
    || GetCVars()->e_terrain_draw_this_sector_only)
	{
		BuildVertices(nStep, false);   
    UpdateRenderMesh(&GetTerrain()->m_StripsInfo, true);
	}
  else
    UpdateRenderMesh(&GetTerrain()->m_StripsInfo, false);

	m_cCurrGeomMML = m_cNewGeomMML;

	// update detail layers indices
	if(GetCVars()->e_detail_materials ) //&& (!m_nRenderStackLevel || GetCVars()->e_detail_materials))
	{
    int nSrcCount=0;
    ushort * pSrcInds = pRenderMesh->GetIndices(&nSrcCount);

/*
    CStripsInfo * pArrayInfo = &m_StripsInfo;

    if(pRenderMesh->GetVertCount() != nVertsNumRequired)
      Error("pRenderMesh->GetVertCount() != nVertsNumRequired");

    if(pArrayInfo->idx_array.Count() != nSrcCount)
      Error("pArrayInfo->idx_array.Count() != nSrcCount");

    for(int k=0; k<pArrayInfo->idx_array.Count(); k++)
      if(pArrayInfo->idx_array[k] != pSrcInds[k])
      {
        Error("pArrayInfo->idx_array[k] != pSrcInds[k]");
        break;
      }
*/
		// build all indices
		GenerateIndicesForAllSurfaces(pRenderMesh);

    static PodArray<SSurfaceType *> lstReadyTypes; lstReadyTypes.Clear(); // protection from duplications in palette of types

		IMaterial::EProjAxis projAxes[] = {IMaterial::ePA_XPos,IMaterial::ePA_YPos,IMaterial::ePA_ZPos,IMaterial::ePA_XNeg,IMaterial::ePA_YNeg,IMaterial::ePA_ZNeg};
		for(int i=0; i<m_lstSurfaceTypeInfo.Count(); i++)
		{
      if(SSurfaceType * pSurfaceType = m_lstSurfaceTypeInfo[i].pSurfaceType)
      {
        if(lstReadyTypes.Find(pSurfaceType)<0)
        {
          bool b3D = pSurfaceType->IsMaterial3D();
          for(int p=0; p<6; p++)
          {
            if(b3D || pSurfaceType->GetMaterialOfProjection(projAxes[p]))
            {
              int nProjId = pSurfaceType->IsMaterial3D() ? p : 6;
              PodArray<unsigned short> & lstIndices = m_arrIndices[pSurfaceType->ucThisSurfaceTypeId][nProjId];
              UpdateSurfaceRenderMeshes(pRenderMesh, pSurfaceType, m_lstSurfaceTypeInfo[i].arrpRM[p], p, lstIndices, "TerrainMaterialLayer");
            }
          }
          lstReadyTypes.Add(pSurfaceType);
        }
      }
		}
	}

	DrawArray(nTechniqueID);
}

void CTerrainNode::GenerateIndicesForQuad(IRenderMesh * pRM, Vec3 vBoxMin, Vec3 vBoxMax, PodArray<ushort> & dstIndices)
{
	dstIndices.Clear();

	int nSrcCount=0;
	ushort * pSrcInds = pRM->GetIndices(&nSrcCount);

	int nPosStride=0;
	byte * pPos = pRM->GetStridedPosPtr(nPosStride);

	int nSrcIndicesPerTriangle;
	if(pRM->GetPrimetiveType() == R_PRIMV_MULTI_STRIPS)
		nSrcIndicesPerTriangle = 1;
	else
		nSrcIndicesPerTriangle = 3;

	for(int i=0; i<(*pRM->GetChunks()).Count(); i++)
	{
		if (!(*pRM->GetChunks())[i].pRE)
			continue;

		CRenderChunk * pChunk = &(*pRM->GetChunks())[i];

		for (int j=pChunk->nFirstIndexId; (j+2)<pChunk->nNumIndices+pChunk->nFirstIndexId; j+=nSrcIndicesPerTriangle)
		{
			int nIndex0 = pSrcInds[j+0];
			int nIndex1 = pSrcInds[j+1];
			int nIndex2 = pSrcInds[j+2];

			assert(nIndex0>=0 && nIndex0<pRM->GetVertCount());
			assert(nIndex1>=0 && nIndex1<pRM->GetVertCount());
			assert(nIndex2>=0 && nIndex2<pRM->GetVertCount());

			Vec3 & vPos0 = *(Vec3*)&pPos[nIndex0*nPosStride];
			Vec3 & vPos1 = *(Vec3*)&pPos[nIndex1*nPosStride];
			Vec3 & vPos2 = *(Vec3*)&pPos[nIndex2*nPosStride];

			if(nIndex0 != nIndex1 && nIndex1 != nIndex2 && nIndex2 != nIndex0)
				if(Overlap::AABB_Triangle(AABB(vBoxMin,vBoxMax),vPos0,vPos1,vPos2))
			{
				if(j&1)
				{
					dstIndices.Add(nIndex0);
					dstIndices.Add(nIndex2);
					dstIndices.Add(nIndex1);
				}
				else
				{
					dstIndices.Add(nIndex0);
					dstIndices.Add(nIndex1);
					dstIndices.Add(nIndex2);
				}
			}
		}
	}
}

int GetVecProjId(const Vec3 & vNorm,SSurfaceType * pLayer)
{
	Vec3 vNormAbs = vNorm.abs();

	int nOpenId=0;

	if(vNormAbs.x>=vNormAbs.y && vNormAbs.x>=vNormAbs.z)
		nOpenId = 0;
	else if(vNormAbs.y>=vNormAbs.x && vNormAbs.y>=vNormAbs.z)
		nOpenId = 1;
	else 
		nOpenId = 2;

	if (nOpenId == 0 && vNorm.x < 0 && pLayer->GetMaterialOfProjection(IMaterial::ePA_XNeg))
		nOpenId = 3;
	if (nOpenId == 1 && vNorm.y < 0 && pLayer->GetMaterialOfProjection(IMaterial::ePA_YNeg))
		nOpenId = 4;
	if (nOpenId == 2 && vNorm.z < 0 && pLayer->GetMaterialOfProjection(IMaterial::ePA_ZNeg))
		nOpenId = 5;

	return nOpenId;
}

void CTerrainNode::GenerateIndicesForAllSurfaces(IRenderMesh * pRM)
{
	FUNCTION_PROFILER_3DENGINE;

	byte arrMat3DFlag[MAX_SURFACE_TYPES_COUNT];
	SSurfaceType *arrMat3DSurface[MAX_SURFACE_TYPES_COUNT];

	for(int s=0; s<MAX_SURFACE_TYPES_COUNT; s++)
	{
		for(int p=0; p<7; p++)
			m_arrIndices[s][p].Clear();

		arrMat3DFlag[s] = GetTerrain()->GetSurfaceTypes()[s].IsMaterial3D();
		arrMat3DSurface[s] = &GetTerrain()->GetSurfaceTypes()[s];
	}

  // Restore system buffer
	int nSrcCount=0;
	ushort * pSrcInds = pRM->GetIndices(&nSrcCount);

	int nColorStride=0;
	byte * pColor = pRM->GetStridedColorPtr(nColorStride);

	int nNormStride=0;
	byte * pNormB = pRM->GetStridedPosPtr(nNormStride) + sizeof(Vec3);

	assert(pColor);

	int nSrcIndicesPerTriangle;
	if(pRM->GetPrimetiveType() == R_PRIMV_MULTI_STRIPS)
		nSrcIndicesPerTriangle = 1;
	else
		nSrcIndicesPerTriangle = 3;

	for(int i=0; pColor && i<pRM->GetChunks()->Count(); i++)
	{
		CRenderChunk * pChunk = pRM->GetChunks()->Get(i);

		if(!pChunk->nNumIndices)
			continue;

		int nLastIndex = pChunk->nNumIndices+pChunk->nFirstIndexId-2;

		for (int j=pChunk->nFirstIndexId; j<nLastIndex; j+=nSrcIndicesPerTriangle)
		{
			int nIndex0 = pSrcInds[j+0];
			int nIndex1 = pSrcInds[j+1];
			int nIndex2 = pSrcInds[j+2];

			// ignore invalid triangles
			if(nIndex0 == nIndex1 || nIndex1 == nIndex2 || nIndex2 == nIndex0)
				continue;

			assert(nIndex0>=0 && nIndex0<pRM->GetVertCount());
			assert(nIndex1>=0 && nIndex1<pRM->GetVertCount());
			assert(nIndex2>=0 && nIndex2<pRM->GetVertCount());

			UCol & Color0 = *(UCol*)&pColor[nIndex0*nColorStride];
			UCol & Color1 = *(UCol*)&pColor[nIndex1*nColorStride];
			UCol & Color2 = *(UCol*)&pColor[nIndex2*nColorStride];

#if defined(NEED_ENDIAN_SWAP)
#define ENDIAN_CONVERSION(_val) (3-_val)
#else
#define ENDIAN_CONVERSION(_val) _val
#endif

			uint nVertSurfId0 = Color0.bcolor[ENDIAN_CONVERSION(1)]&127;
			uint nVertSurfId1 = Color1.bcolor[ENDIAN_CONVERSION(1)]&127;
			uint nVertSurfId2 = Color2.bcolor[ENDIAN_CONVERSION(1)]&127;

			assert(nVertSurfId0<MAX_SURFACE_TYPES_COUNT);
			assert(nVertSurfId1<MAX_SURFACE_TYPES_COUNT);
			assert(nVertSurfId2<MAX_SURFACE_TYPES_COUNT);

			static PodArray<int> lstSurfTypes; lstSurfTypes.Clear();
			lstSurfTypes.Add(nVertSurfId0);
			if(nVertSurfId1 != nVertSurfId0)
				lstSurfTypes.Add(nVertSurfId1);
			if(nVertSurfId2 != nVertSurfId0 && nVertSurfId2 != nVertSurfId1)
				lstSurfTypes.Add(nVertSurfId2);

			static PodArray<int> lstProjIds; lstProjIds.Clear();

			// if there is 3d materials - analyze normals
			if(arrMat3DFlag[nVertSurfId0])
			{
        const byte *bNorm0 = &pNormB[nIndex0*nNormStride];
#if defined(XENON) || defined(PS3)
        DWORD dwNorm = *(DWORD *)bNorm0;
        SwapEndian(dwNorm);
        bNorm0 = (byte *)&dwNorm;
#endif
				const Vec3 vNorm0 = Vec3(((float)bNorm0[0]-127.5f)/127.5f, ((float)bNorm0[1]-127.5f)/127.5f, ((float)bNorm0[2]-127.5f)/127.5f);
				int nProjId0 = GetVecProjId(vNorm0,arrMat3DSurface[nVertSurfId0]);
				if(lstProjIds.Find(nProjId0)<0)
					lstProjIds.Add(nProjId0);
			}

			if(arrMat3DFlag[nVertSurfId1])
			{
        const byte *bNorm1 = &pNormB[nIndex1*nNormStride];
#if defined(XENON) || defined(PS3)
        DWORD dwNorm = *(DWORD *)bNorm1;
        SwapEndian(dwNorm);
        bNorm1 = (byte *)&dwNorm;
#endif
        const Vec3 vNorm1 = Vec3(((float)bNorm1[0]-127.5f)/127.5f, ((float)bNorm1[1]-127.5f)/127.5f, ((float)bNorm1[2]-127.5f)/127.5f);
				int nProjId1 = GetVecProjId(vNorm1,arrMat3DSurface[nVertSurfId1]);
				if(lstProjIds.Find(nProjId1)<0)
					lstProjIds.Add(nProjId1);
			}

			if(arrMat3DFlag[nVertSurfId2])
			{
        const byte *bNorm2 = &pNormB[nIndex2*nNormStride];
#if defined(XENON) || defined(PS3)
        DWORD dwNorm = *(DWORD *)bNorm2;
        SwapEndian(dwNorm);
        bNorm2 = (byte *)&dwNorm;
#endif
        const Vec3 vNorm2 = Vec3(((float)bNorm2[0]-127.5f)/127.5f, ((float)bNorm2[1]-127.5f)/127.5f, ((float)bNorm2[2]-127.5f)/127.5f);
				int nProjId2 = GetVecProjId(vNorm2,arrMat3DSurface[nVertSurfId2]);
				if(lstProjIds.Find(nProjId2)<0)
					lstProjIds.Add(nProjId2);
			}

			// if not 3d materials found
			if(!arrMat3DFlag[nVertSurfId0] || !arrMat3DFlag[nVertSurfId1] || !arrMat3DFlag[nVertSurfId2])
				lstProjIds.Add(6);

			for(int s=0; s<lstSurfTypes.Count(); s++)
			{
				for(int p=0; p<lstProjIds.Count(); p++)
				{
					PodArray<ushort> & rList = m_arrIndices[lstSurfTypes[s]][lstProjIds[p]];

					if(j&1 && nSrcIndicesPerTriangle==1)
					{
						rList.Add(nIndex0);
						rList.Add(nIndex2);
						rList.Add(nIndex1);
					}
					else
					{
						rList.Add(nIndex0);
						rList.Add(nIndex1);
						rList.Add(nIndex2);
					}
				}
			}
		}
	}
}

/*void CTerrainNode::GenerateIndicesForSurface(IRenderMesh * pRM, uint nVertSurfId, PodArray<ushort> & dstIndices, int nProjectionId)
{
  FUNCTION_PROFILER_3DENGINE;

	dstIndices.Clear();

	int nSrcCount=0;
	ushort * pSrcInds = pRM->GetIndices(&nSrcCount);

	int nColorStride=0;
	byte * pColor = pRM->GetColorPtr(nColorStride);

	int nNormStride=0;
	byte * pNorm = pRM->GetNormalPtr(nNormStride);

	assert(pColor);

	int nSrcIndicesPerTriangle;
	if(pRM->GetPrimetiveType() == R_PRIMV_MULTI_STRIPS)
		nSrcIndicesPerTriangle = 1;
	else
		nSrcIndicesPerTriangle = 3;

	for(int i=0; pColor && i<pRM->GetChunks()->Count(); i++)
	{
		CRenderChunk * pChunk = pRM->GetChunks()->Get(i);
		
		if(!pChunk->nNumIndices)
			continue;

		int nLastIndex = pChunk->nNumIndices+pChunk->nFirstIndexId-2;

		for (int j=pChunk->nFirstIndexId; j<nLastIndex; j+=nSrcIndicesPerTriangle)
		{
			int nIndex0 = pSrcInds[j+0];
			int nIndex1 = pSrcInds[j+1];
			int nIndex2 = pSrcInds[j+2];

			assert(nIndex0>=0 && nIndex0<pRM->GetSysVertCount());
			assert(nIndex1>=0 && nIndex1<pRM->GetSysVertCount());
			assert(nIndex2>=0 && nIndex2<pRM->GetSysVertCount());

			UCol & Color0 = *(UCol*)&pColor[nIndex0*nColorStride];
			UCol & Color1 = *(UCol*)&pColor[nIndex1*nColorStride];
			UCol & Color2 = *(UCol*)&pColor[nIndex2*nColorStride];

#if defined(NEED_ENDIAN_SWAP)
#define ENDIAN_CONVERSION(_val) (3-_val)
#else
#define ENDIAN_CONVERSION(_val) _val
#endif

			const Vec3 & vNorm0 = *(Vec3*)&pNorm[nIndex0*nNormStride];
			const Vec3 & vNorm1 = *(Vec3*)&pNorm[nIndex1*nNormStride];
			const Vec3 & vNorm2 = *(Vec3*)&pNorm[nIndex2*nNormStride];

			uint nVertSurfId0 = Color0.bcolor[ENDIAN_CONVERSION(2)];
			uint nVertSurfId1 = Color1.bcolor[ENDIAN_CONVERSION(2)];
			uint nVertSurfId2 = Color2.bcolor[ENDIAN_CONVERSION(2)];

			if(nIndex0 != nIndex1 && nIndex1 != nIndex2 && nIndex2 != nIndex0) // if valid triangle
				if(	(nVertSurfId0 == nVertSurfId) || (nVertSurfId1 == nVertSurfId) || (nVertSurfId2 == nVertSurfId) ) // if at least one vertex has needed material
					if( (nProjectionId<0) || GetVecProjId(vNorm0) == nProjectionId || GetVecProjId(vNorm1) == nProjectionId || GetVecProjId(vNorm2) == nProjectionId )
			{
				if(j&1 && nSrcIndicesPerTriangle==1)
				{
					dstIndices.Add(nIndex0);
					dstIndices.Add(nIndex2);
					dstIndices.Add(nIndex1);
				}
				else
				{
					dstIndices.Add(nIndex0);
					dstIndices.Add(nIndex1);
					dstIndices.Add(nIndex2);
				}
			}
		}
	}
}*/

void CTerrainNode::UpdateSurfaceRenderMeshes(IRenderMesh * pSrcRM, SSurfaceType * pSurface, IRenderMesh * & pMatRM, int nProjectionId, PodArray<unsigned short> & lstIndices, const char * szComment)
{
	FUNCTION_PROFILER_3DENGINE;

//	static PodArray<unsigned short> lstIndices; lstIndices.Clear();
	//GenerateIndicesForSurface(pSrcRM, pSurface->ucThisSurfaceTypeId, lstIndices, pSurface->IsMaterial3D() ? nProjectionId : -1);

	if(!pMatRM)
	{
		pMatRM = GetRenderer()->CreateRenderMeshInitialized(
			NULL, 0, VERTEX_FORMAT_P3F_N4B_COL4UB, NULL, 0,
			R_PRIMV_TRIANGLES, szComment, szComment, eBT_Static, 1, 0, NULL, NULL, false, false);
	}

	IMaterial::EProjAxis projAxes[] = {IMaterial::ePA_XPos,IMaterial::ePA_YPos,IMaterial::ePA_ZPos,IMaterial::ePA_XNeg,IMaterial::ePA_YNeg,IMaterial::ePA_ZNeg};

	pMatRM->SetVertexContainer(pSrcRM);
	pMatRM->UpdateSysIndices(lstIndices.GetElements(), lstIndices.Count());
	pMatRM->SetChunk(pSurface->GetMaterialOfProjection(projAxes[nProjectionId]), 0, pSrcRM->GetVertCount(), 0, lstIndices.Count());
	pMatRM->SetUpdateFrame(pSrcRM->GetUpdateFrame());

	assert(nProjectionId>=0 && nProjectionId<6);
	float * pParams = pSurface->arrRECustomData[nProjectionId];
	SetupTexGenParams(pSurface,pParams,projAxes[nProjectionId],true);
	pMatRM->SetMaterial(pSurface->GetMaterialOfProjection(projAxes[nProjectionId]));

	// set surface type
	if(pSurface->IsMaterial3D())
	{
		pParams[ 8] = (nProjectionId == 0 || nProjectionId == 3);
		pParams[ 9] = (nProjectionId == 1 || nProjectionId == 4);
		pParams[10] = (nProjectionId == 2 || nProjectionId == 5);
	}
	else
	{
		pParams[ 8] = 1.f;
		pParams[ 9] = 1.f;
		pParams[10] = 1.f;
	}

	pParams[11] = pSurface->ucThisSurfaceTypeId;

	// set texgen offset
	Vec3 vCamPos = GetCamera().GetPosition();

	pParams[12] = pParams[13] = pParams[14] = pParams[15] = 0;

	// for diffuse
	if(IMaterial * pMat = pSurface->GetMaterialOfProjection(projAxes[nProjectionId]))
		if(pMat->GetShaderItem().m_pShaderResources)
	{
		if(	SEfResTexture * pTex = pMat->GetShaderItem().m_pShaderResources->GetTexture(EFTT_DIFFUSE))
		{
			float fScaleX = pTex->m_bUTile ? pTex->m_TexModificator.m_Tiling[0]*pSurface->fScale : 1.f;
			float fScaleY = pTex->m_bVTile ? pTex->m_TexModificator.m_Tiling[1]*pSurface->fScale : 1.f;

			pParams[12] = int(vCamPos.x*fScaleX)/fScaleX;
			pParams[13] = int(vCamPos.y*fScaleY)/fScaleY;
		}

		if(	SEfResTexture * pTex = pMat->GetShaderItem().m_pShaderResources->GetTexture(EFTT_BUMP))
		{
			float fScaleX = pTex->m_bUTile ? pTex->m_TexModificator.m_Tiling[0]*pSurface->fScale : 1.f;
			float fScaleY = pTex->m_bVTile ? pTex->m_TexModificator.m_Tiling[1]*pSurface->fScale : 1.f;

			pParams[14] = int(vCamPos.x*fScaleX)/fScaleX;
			pParams[15] = int(vCamPos.y*fScaleY)/fScaleY;
		}
	}

	assert(8+8<=ARR_TEX_OFFSETS_SIZE_DET_MAT);

	if(pMatRM->GetChunks()->Count() && pMatRM->GetChunks()->Get(0)->pRE)
		pMatRM->GetChunks()->Get(0)->pRE->m_CustomData = pParams;

	Vec3 vMin, vMax;
	pSrcRM->GetBBox(vMin, vMax);
	pMatRM->SetBBox(vMin, vMax);

  if( pMatRM->GetSysIndicesCount() < pSrcRM->GetSysIndicesCount() / 4 )
    pMatRM->UpdateBBoxFromMesh();
}

CTerrainNode::STerrainNodeLeafData::~STerrainNodeLeafData()
{
	GetRenderer()->DeleteRenderMesh(m_pRenderMesh);
}

/*void CTerrainNode::OnLightMapDeleted(IDynTexture * pLightMap)
{
	if(m_nTexSet.pLightMap == pLightMap)
		m_nTexSet.pLightMap = NULL;

	if(m_arrChilds[0])
		for(int i=0; i<4; i++)
			m_arrChilds[i]->OnLightMapDeleted(pLightMap);
}*/
