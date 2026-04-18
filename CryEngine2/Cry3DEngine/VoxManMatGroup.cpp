//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:voxman.cpp
//  voxel tecnology researh
//
//	History:
//	-:Created by Vladimir Kajalin
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "VoxMan.h"
#include "IndexedMesh.h"
#include "MeshCompiler/MeshCompiler.h"

CVoxelMesh::CVoxelMesh(SSurfaceType ** ppSurfacesPalette)
{
	memset(this,0,sizeof(*this));

	for(int nLod=0; nLod<VOX_MAX_LODS_NUM; nLod++)
		for(int nSurf=0; nSurf<VOX_MAX_SURF_TYPES_NUM; nSurf++)
			m_arrSurfaceTypeInfo[nLod][nSurf].pSurfaceType = ppSurfacesPalette[nSurf];

  GetInstCount(eERType_VoxelMesh)++;
}

CVoxelMesh::~CVoxelMesh()
{
	ResetRenderMeshs();

  GetInstCount(eERType_VoxelMesh)--;
}

void CVoxelMesh::ResetRenderMeshs()
{
	for(int nLod=0; nLod<VOX_MAX_LODS_NUM; nLod++)
	{
		for(int l=VOX_MAX_SURF_TYPES_NUM-1; l>=0; l--)
			m_arrSurfaceTypeInfo[nLod][l].DeleteRenderMeshes(GetRenderer());

		GetRenderer()->DeleteRenderMesh(m_arrpRM_Ambient[nLod]);
		m_arrpRM_Ambient[nLod] = NULL;
	}
}

void CVoxelMesh::RenderAmbPass(int nLod, int nSort, int nAW, CRenderObject * pObj, int nTechId, const SRendParams & rParams)
{
	// ambient pass
	if(m_arrpRM_Ambient[nLod])
	{
		if(GetCVars()->e_debug_lights)
			m_arrpRM_Ambient[nLod]->RenderDebugLightPass(pObj->m_II.m_Matrix,rParams.nDLightMask,GetCVars()->e_debug_lights,GetCVars()->e_max_entity_lights);
		else
		m_arrpRM_Ambient[nLod]->AddRenderElements(pObj,nSort,nAW,NULL,nTechId);
}
}

void CVoxelMesh::RenderLightPasses(int nLod, int nDLMask, CVoxelObject * pVoxArea, bool bInShadow, float fDistance, const SRendParams & rParams)
{
	// detail light pass
	CRenderObject * pObj = NULL;

	for(int nSType=0; nSType<VOX_MAX_SURF_TYPES_NUM; nSType++)
		if(m_arrSurfaceTypeInfo[nLod][nSType].pSurfaceType)
			if(m_arrSurfaceTypeInfo[nLod][nSType].pSurfaceType->HasMaterial() && m_arrSurfaceTypeInfo[nLod][nSType].HasRM())
			{
        if(!pObj)
        {
          pObj = GetIdentityCRenderObject();
          if (!pObj)
            return;
          pObj->m_DynLMMask = nDLMask;
          pObj->m_ObjFlags |= (rParams.dwFObjFlags & FOB_SELECTED) | (FOB_DETAILPASS | FOB_NO_FOG);
          //				pObj->m_pShadowCasters = (pShadowCasters && pShadowCasters->Count()) ? pShadowCasters : NULL;
          pObj->m_II.m_AmbColor = rParams.AmbientColor;
          pObj->m_II.m_Matrix = *rParams.pMatrix;
          pObj->m_ObjFlags |= FOB_TRANS_MASK;
          if (bInShadow)
            pObj->m_ObjFlags |= FOB_INSHADOW;
          pObj->m_fDistance = rParams.fDistance;
          pObj->m_nRAEId = 0;

          if(rParams.pTerrainTexInfo && GetTerrain()->IsAmbientOcclusionEnabled())
          { // provide terrain texture info
            pObj->m_nTextureID  = rParams.pTerrainTexInfo->nTex0;
            pObj->m_nTextureID1 = rParams.pTerrainTexInfo->nTex1;
            pObj->m_fTempVars[0] = rParams.pTerrainTexInfo->fTexOffsetX;
            pObj->m_fTempVars[1] = rParams.pTerrainTexInfo->fTexOffsetY;
            pObj->m_fTempVars[2] = rParams.pTerrainTexInfo->fTexScale;
            pObj->m_fTempVars[3] = rParams.pTerrainTexInfo->fTerrainMinZ;
            pObj->m_fTempVars[4] = rParams.pTerrainTexInfo->fTerrainMaxZ;
            pObj->m_ObjFlags |= FOB_AMBIENT_OCCLUSION;
          }
        }

//        if(IMaterial * pMat = m_arrSurfaceTypeInfo[nLod][nSType].pSurfaceType->pLayerMat)
//          Get3DEngine()->InitMaterialDefautMappingAxis(pMat);

				IMaterial::EProjAxis projAxes[] = {IMaterial::ePA_XPos,IMaterial::ePA_YPos,IMaterial::ePA_ZPos,IMaterial::ePA_XNeg,IMaterial::ePA_YNeg,IMaterial::ePA_ZNeg};

			  for(int p=0; p<6; p++)
			  {
				  if(IMaterial * pSubMat = m_arrSurfaceTypeInfo[nLod][nSType].pSurfaceType->GetMaterialOfProjection(projAxes[p]))
					  if(fDistance<m_arrSurfaceTypeInfo[nLod][nSType].pSurfaceType->GetMaxMaterialDistanceOfProjection(projAxes[p]))
              if(IRenderMesh * pRM = m_arrSurfaceTypeInfo[nLod][nSType].arrpRM[p])
						  if(pRM->GetVertexContainer() == m_arrpRM_Ambient[nLod])
                if(pRM->GetSysIndicesCount())
										if(!GetCVars()->e_debug_lights)
							    pRM->AddRenderElements(pObj, EFSLIST_TERRAINLAYER, rParams.nAfterWater, pSubMat);
			  }
		}
}

void CVoxelObject::SerializeMeshes(CVoxelObject * pVoxArea, CIndexedMesh * pIndexedMesh)
{
  ResetRenderMeshs();
  
  m_pVoxelVolume->SerializeRenderMeshs(pVoxArea, pIndexedMesh);
}

void CVoxelVolume::SerializeRenderMeshs(CVoxelObject * pVoxArea, CIndexedMesh * pIndexedMesh)
{
	FUNCTION_PROFILER_3DENGINE;

	IMaterial * pTerrainEf = NULL;
	GetTerrain()->GetMaterials(pTerrainEf);

  pVoxArea->ReleaseMemBlocks();

	int nLodsNum = (pVoxArea->m_nFlags&IVOXELOBJECT_FLAG_GENERATE_LODS) ? min(1+GetCVars()->e_voxel_lods_num,VOX_MAX_LODS_NUM) : 1;
	for(int nLod=0; pIndexedMesh->m_numFaces && nLod<nLodsNum; nLod++)
	{ // make RenderMeshs
		if(pVoxArea->m_nFlags&IVOXELOBJECT_FLAG_GENERATE_LODS)
			SimplifyIndexedMesh(pIndexedMesh, nLod);

    pIndexedMesh->InitStreamSize();
    pIndexedMesh->SetSubSetCount(1);
    pIndexedMesh->CalcBBox();

    if(!m_bEditorMode)
      Warning( "CVoxelMesh::SerializeRenderMeshs: Stripifying geometry at loading time");

    CMesh mesh;
    mesh.Copy( *pIndexedMesh->GetMesh() );
    mesh_compiler::CMeshCompiler meshCompiler;
    meshCompiler.Compile( mesh, 0 );

    if(!mesh.GetIndexCount())
      break;

    if(nLod==0 && GetCVars()->e_voxel_make_physics)
      pVoxArea->Physicalize(&mesh, NULL);

    mesh.SetFacesCount(0);

    pVoxArea->SerializeMesh( &mesh );
	}
}

void CVoxelMesh::MakeRenderMeshsFromMemBlocks(CVoxelObject * pVoxArea)
{
  FUNCTION_PROFILER_3DENGINE;

  ResetRenderMeshs();

  IMaterial * pTerrainEf = NULL;
  GetTerrain()->GetMaterials(pTerrainEf);

  Get3DEngine()->CheckMemoryHeap();

  int nLodsNum = (pVoxArea->m_nFlags&IVOXELOBJECT_FLAG_GENERATE_LODS) ? min(1+GetCVars()->e_voxel_lods_num,VOX_MAX_LODS_NUM) : 1;
  for(int nLod=0; nLod<nLodsNum && nLod<pVoxArea->m_arrMeshesForSerialization.Count(); nLod++)
  { // make RenderMeshs
    CMemoryBlock * pSerialized = pVoxArea->m_arrMeshesForSerialization[nLod];
    CMemoryBlock * pTmp = pSerialized;

    // check if was compressed
    if(strncmp((char *)pSerialized->GetData(),"CryTek",6)!=0)
    {
      pTmp = CMemoryBlock::DecompressFromMemBlock(pSerialized, GetSystem());
      if(!pTmp)
      {
        Get3DEngine()->CheckMemoryHeap();
        return Error("CVoxelMesh::MakeRenderMeshsFromMemBlocks: CMemoryBlock::DecompressFromMemBlock failed");
      }
      assert(strncmp((char *)pTmp->GetData(),"CryTek",6)==0);
    }

    std::vector<char> physData;
    CMesh * pLoaded = NULL;
    CContentCGF * pContent = pVoxArea->LoadFromMemBlock( pTmp, pLoaded, nLod==0 ? &physData : NULL);

    if(pTmp != pSerialized)
      delete pTmp;

    if(nLod==0 && GetCVars()->e_voxel_make_physics)
      pVoxArea->Physicalize(pLoaded, &physData);

    // make ambient pass geometry
    m_arrpRM_Ambient[nLod] = CreateRenderMeshFromIndexedMesh(pLoaded, pVoxArea);

    DeleteCContentCGF(pContent);

    if(m_arrpRM_Ambient[nLod])
      PrintMessagePlus(" %d tris produced,", m_arrpRM_Ambient[nLod]->GetSysIndicesCount()/3);

    m_arrpRM_Ambient[nLod]->SetMaterial(pTerrainEf);

    // build all indices
    CTerrainNode::GenerateIndicesForAllSurfaces(m_arrpRM_Ambient[nLod]);

    static PodArray<SSurfaceType *> lstReadyTypes; lstReadyTypes.Clear(); // protection from duplications in palette of types

    for(int i=0; i<VOX_MAX_SURF_TYPES_NUM; i++)
    {
      if(SSurfaceType * pSType = m_arrSurfaceTypeInfo[nLod][i].pSurfaceType)
      {
        if(lstReadyTypes.Find(pSType)<0)
        {
					IMaterial::EProjAxis projAxes[] = {IMaterial::ePA_XPos,IMaterial::ePA_YPos,IMaterial::ePA_ZPos,IMaterial::ePA_XNeg,IMaterial::ePA_YNeg,IMaterial::ePA_ZNeg};

          for(int p=0; p<6; p++)
          {
            if(pSType->GetMaterialOfProjection(projAxes[p]) || pSType->IsMaterial3D()) // todo: duplicate rendering?
            {
              int nProjId = pSType->IsMaterial3D() ? p : 6;
              PodArray<unsigned short> & lstIndices = CTerrainNode::m_arrIndices[pSType->ucThisSurfaceTypeId][nProjId];
              CTerrainNode::UpdateSurfaceRenderMeshes(m_arrpRM_Ambient[nLod], pSType, m_arrSurfaceTypeInfo[nLod][i].arrpRM[p], p, lstIndices, "VoxelMaterialLayer");
            }
          }
          lstReadyTypes.Add(pSType);
        }
      }
    }
  }

  Get3DEngine()->CheckMemoryHeap();

  if(m_arrpRM_Ambient[0])
    UpdateVertexHash(m_arrpRM_Ambient[0]);

  memset(m_arrCurrAoRadiusAndScale,0,sizeof(m_arrCurrAoRadiusAndScale));
}

void CVoxelMesh::SetupAmbPassMapping(int nElemCount, float * pHMTexOffsets, const SSectorTextureSet & texSet, bool bSetupSinglePassTexIds)
{
/*	float * pTexOffsets = m_arrLayerTexGenInfo_Ambient;

	// setup base texture texgen
	if(pHMTexOffsets)
		memcpy(pTexOffsets, pHMTexOffsets, sizeof(float)*8);
*/
	// setup detail layers
	/*for(int t=0; t<VOX_MAX_SINGLETPASS_MATS_NUM && t<m_lstOpenMaterials.Count(); t++)
	{
		float * pOutParams = pTexOffsets + 8 + t*8;

		if(&pOutParams[7] >= pTexOffsets+nElemCount)
			break;

		uchar ucProjAxis = m_lstOpenMaterials[t].pOpenMat->m_ucDefautMappingAxis;
		float fScale = m_lstOpenMaterials[t].pOpenMat->m_fDefautMappingScale;
		float fMaxMatDistanceXY = m_lstOpenMaterials[t].pSurface->fMaxMatDistanceXY;
		float fMaxMatDistanceZ = m_lstOpenMaterials[t].pSurface->fMaxMatDistanceZ;

		// setup projection direction
		if(ucProjAxis == 'X')
		{
			pOutParams[0] = 0;
			pOutParams[1] = fScale;
			pOutParams[2] = 0;
			pOutParams[3] = fMaxMatDistanceXY;

			pOutParams[4] = 0;
			pOutParams[5] = 0;
			pOutParams[6] = fScale;
			pOutParams[7] = 0;
		}
		else if(ucProjAxis =='Y')
		{
			pOutParams[0] = fScale;
			pOutParams[1] = 0;
			pOutParams[2] = 0;
			pOutParams[3] = fMaxMatDistanceXY;

			pOutParams[4] = 0;
			pOutParams[5] = 0;
			pOutParams[6] = fScale;
			pOutParams[7] = 0;
		}
		else // Z
		{
			pOutParams[0] = fScale;
			pOutParams[1] = 0;
			pOutParams[2] = 0;
			pOutParams[3] = fMaxMatDistanceZ;

			pOutParams[4] = 0;
			pOutParams[5] = fScale;
			pOutParams[6] = 0;
			pOutParams[7] = 0;
		}
	}*/

	for(int nLod=0; nLod<VOX_MAX_LODS_NUM; nLod++)
	if(m_arrpRM_Ambient[nLod] && m_arrpRM_Ambient[nLod]->GetVertCount())
	{
		IRenderMesh * pRM = m_arrpRM_Ambient[nLod];

		if(pHMTexOffsets)
			pRM->SetRECustomData(pHMTexOffsets);

		PodArray<CRenderChunk> * pMats = pRM->GetChunks();
		if(!pMats->Count())
			break;

		for(int i=TERRAIN_BASE_TEXTURES_NUM; i<MAX_CUSTOM_TEX_BINDS_NUM; i++)
			pMats->GetAt(0).pRE->m_CustomTexBind[i] = 0;

		pMats->GetAt(0).pRE->m_CustomTexBind[0] = texSet.nTex0;
		pMats->GetAt(0).pRE->m_CustomTexBind[1] = texSet.nTex1;
/*
		if(texSet.pLightMap && texSet.pLightMap->GetTextureID())
			pMats->GetAt(0).pRE->m_CustomTexBind[1] = texSet.pLightMap->GetTextureID();
		else
			pMats->GetAt(0).pRE->m_CustomTexBind[1] = m_pTerrain->m_nBlackTexId;
*/
/*  No simple detail texturing for voxel
		if(GetCVars()->e_detail_texture && bSetupSinglePassTexIds)
		for(int m = 0; m<VOX_MAX_SINGLETPASS_MATS_NUM && m<MAX_CUSTOM_TEX_BINDS_NUM && m<m_lstOpenMaterials.Count(); m++)
		{
			if(IMaterial * pMat = m_lstOpenMaterials[m].pOpenMat)
			if(SRenderShaderResources * pShRes = m_lstOpenMaterials[m].pOpenMat->GetShaderItem().m_pShaderResources)
			if(SEfResTexture * pEfRes = pShRes->m_Textures[EFTT_DIFFUSE])
			if(CREMesh * pReMesh = pRM->GetChunks()->GetAt(0).pRE)
			{
				ITexture * pTex = ((ITexture *)(pEfRes->m_Sampler.m_pITex));
				if(pTex)
					pReMesh->m_CustomTexBind[m+TERRAIN_BASE_TEXTURES_NUM] = pTex->GetTextureID();
			}
		}*/
	}
}

IRenderMesh * CVoxelMesh::CreateRenderMeshFromIndexedMesh(CMesh * pMatMesh, CVoxelObject * pVoxArea)
{
  IRenderMesh * pRM = GetRenderer()->CreateRenderMesh(eBT_Static,"Voxel","Voxel");
  pRM->SetMesh( *pMatMesh, 0, (GetCVars()->e_default_material ? 0 : FSM_NO_TANGENTS) | FSM_VOXELS | FSM_CREATE_DEVICE_MESH);
	pRM->SetBBox( pMatMesh->m_bbox.min, pMatMesh->m_bbox.max);
	return pRM;
}

int CVoxelMesh::GetMemoryUsage()
{
  int nSize = sizeof(*this);

  // clear hash
  for(int i=0; i<VOX_HASH_SIZE; i++)
    for(int j=0; j<VOX_HASH_SIZE; j++)
      nSize += m_arrVertHash[i][j].capacity()*sizeof(ushort);

	return nSize;
}

void CVoxelMesh::UpdateVertexHash(IRenderMesh * pRM)
{
	// get offsets
	int nPosStride=0, nColorStride=0, nNormStride=0;
	byte  *pPos      = pRM->GetStridedPosPtr(nPosStride);
	uchar *pColor    = pRM->GetStridedColorPtr(nColorStride);

	// clear hash
	for(int i=0; i<VOX_HASH_SIZE; i++)
		for(int j=0; j<VOX_HASH_SIZE; j++)
			m_arrVertHash[i][j].Clear();

	const int nShift = VOX_HASH_SIZE/4;
	const int nMask = VOX_HASH_SIZE-1;

  static ushort arrVertHashElemCount[VOX_HASH_SIZE][VOX_HASH_SIZE];
  memset(arrVertHashElemCount,0,sizeof(arrVertHashElemCount));

  // count hash elements
  for(int i=0, nVertCount = pRM->GetVertCount(); i<nVertCount; i++)
  {
    Vec3 & vPos = *((Vec3*)&(pPos[i*nPosStride]));
    int x = uchar(vPos.x*VOX_HASH_SCALE+nShift)&nMask;
    int y = uchar(vPos.y*VOX_HASH_SCALE+nShift)&nMask;
    arrVertHashElemCount[x][y]++;
  }

  // preallocate hash arrays
  for(int x=0; x<VOX_HASH_SIZE; x++)
    for(int y=0; y<VOX_HASH_SIZE; y++)
      m_arrVertHash[x][y].PreAllocate(arrVertHashElemCount[x][y]);

	// register verts in hash
	for(int i=0, nVertCount = pRM->GetVertCount(); i<nVertCount; i++)
	{
    Vec3 & vPos = *((Vec3*)&(pPos[i*nPosStride]));
    int x = uchar(vPos.x*VOX_HASH_SCALE+nShift)&nMask;
    int y = uchar(vPos.y*VOX_HASH_SCALE+nShift)&nMask;
		PodArray<ushort> & rHash = m_arrVertHash[x][y];
    assert(rHash.capacity()>rHash.size());
    rHash.Add(i);
	}
}

int CVoxelMesh::UpdateAmbientOcclusion(CVoxelObject ** pNeighbours, CVoxelObject * pThisArea, int nLod)
{
	FUNCTION_PROFILER_3DENGINE;

	IRenderMesh * pRM = m_arrpRM_Ambient[nLod];
	if(!pRM)
		return 0;

	// get offsets
	int nPosStride=0, nColorStride=0, nNormStride=0;
	byte *pPos      = pRM->GetStridedPosPtr(nPosStride);
	uchar*pColor    = pRM->GetStridedColorPtr(nColorStride, 0, true);
	byte *pNormB    = pPos + sizeof(Vec3);
  nNormStride = nPosStride;

	float fVox2WorldRatio = pThisArea->m_Matrix.GetColumn(0).GetLength() * DEF_VOX_UNIT_SIZE; // *pThisArea->m_fUnitSize;
	const float fTestRadiusWS = GetCVars()->e_voxel_ao_radius*fVox2WorldRatio;
	Vec3 vTestRadiusWS(fTestRadiusWS,fTestRadiusWS,fTestRadiusWS);

	bool bSnapToTerrainZ = pThisArea->IsSnappedToTerrainSectors() || (pThisArea->m_nFlags & IVOXELOBJECT_FLAG_SNAP_TO_TERRAIN);

	// calculate occlusion
	for(int i=0, nVertCount = pRM->GetVertCount(); i<nVertCount; i++)
	{
		Vec3 & vPos    = *((Vec3*)&pPos[nPosStride*i]);

		const byte * bNorm = &pNormB[nNormStride*i];
		const Vec3 vNorm = Vec3((bNorm[0]-128.0f)/127.5f, (bNorm[1]-128.0f)/127.5f, (bNorm[2]-128.0f)/127.5f);

		SMeshColor & uColor = *((SMeshColor*)&pColor[nColorStride*i]);

    Vec3 vPosWS = pThisArea->m_Matrix.TransformPoint(vPos);

		if(bSnapToTerrainZ)
		{
			float fTerrainZDiff = GetTerrain()->GetZApr(vPosWS.x, vPosWS.y) - vPosWS.z;
			if(fabs(fTerrainZDiff)<0.5f)
			{
				uColor.a = 255;
				continue;
			}
		}

		float fAmbSumm = 1.f;
		int nSamplesNum = 1;
		
		AABB boxVertexArea;
		boxVertexArea.min = vPosWS - vTestRadiusWS;
		boxVertexArea.max = vPosWS + vTestRadiusWS;

		if(pNeighbours && !pThisArea->GetBBox().ContainsBox(boxVertexArea))
		{
			for(int x=0; x<3; x++)
			{
				for(int y=0; y<3; y++)
				{
					for(int z=0; z<3; z++)
					{
						if(CVoxelObject * pNeibArea = pNeighbours[x*9+y*3+z])
						{
							const AABB & boxNeibAreaBox = pNeibArea->GetBBox();
							if(Overlap::AABB_AABB(boxVertexArea, boxNeibAreaBox))
								nSamplesNum += pNeibArea->GetAmbientOcclusionForPoint(vPosWS, vNorm, fAmbSumm);
						}
					}
				}
			}
		}
		else
			nSamplesNum += GetAmbientOcclusionForPoint(vPos, vNorm, fAmbSumm, pThisArea);

		assert(nSamplesNum);
		if(nSamplesNum)
			fAmbSumm /= nSamplesNum;
		fAmbSumm = 1.f - (1.f - fAmbSumm) * GetCVars()->e_voxel_ao_scale;
		
		uColor.a = uchar(255*min(1.f,max(0.f,fAmbSumm)));
	}

	pRM->InvalidateVideoBuffer(1);

	return 0;
}

int CVoxelMesh::GetAmbientOcclusionForPoint(Vec3 vPos, Vec3 vNorm, float & fResult, CVoxelObject * pThisArea)
{
	FUNCTION_PROFILER_3DENGINE;

	IRenderMesh * pRM = m_arrpRM_Ambient[0];
	if(!pRM)
		return 0;

	// get offsets
	int nPosStride=0, nColorStride=0, nNormStride=0;
	byte *pPos      = pRM->GetStridedPosPtr(nPosStride);
	byte *pNormB    = pPos + sizeof(Vec3);
  nNormStride = nPosStride;

	const float fTestRadiusOS = GetCVars()->e_voxel_ao_radius*DEF_VOX_UNIT_SIZE;//pThisArea->m_fUnitSize;

	const int nShift = VOX_HASH_SIZE/4;
	const int nMask = VOX_HASH_SIZE-1;

	// calculate occlusion for point
	// ambient occlusion
	float fAmbLight = 1.f;
	int nSamplesNum = 0;

	int hx = (int)(vPos.x*VOX_HASH_SCALE+nShift);
	int hy = (int)(vPos.y*VOX_HASH_SCALE+nShift);

	int nHashArea = (int)max(1.f,fTestRadiusOS*VOX_HASH_SCALE);
	for(int x=hx-nHashArea; x<=hx+nHashArea; x++)
	{
		for(int y=hy-nHashArea; y<=hy+nHashArea; y++)
		{
			PodArray<ushort> * pList = &m_arrVertHash[uchar(x)&nMask][uchar(y)&nMask];
			for(int n=0; n<pList->Count(); n++)
			{
				int idx = pList->GetAt(n);
				Vec3 & vPos0    = *((Vec3*)&pPos[nPosStride*idx]);

				const byte * bNorm0 = &pNormB[nNormStride*idx];
				const Vec3 vNorm0 = Vec3((bNorm0[0]-128.0f)/127.5f, (bNorm0[1]-128.0f)/127.5f, (bNorm0[2]-128.0f)/127.5f);

				float fDistV = vPos0.GetDistance(vPos);
				if(fDistV<fTestRadiusOS && fDistV>0.1f)
				{
					float fDistN = (vPos0+vNorm0*fDistV*0.5f).GetDistance(vPos+vNorm*fDistV*0.5f);
					fDistV = max(fDistV,0.001f);
					float t = fDistV/fTestRadiusOS;
					float fRatioFull = min(1.f, fDistN/fDistV);
					float fRatio = (1.f-t)*fRatioFull + t;
					fResult += fRatio;
					nSamplesNum++;
				}
			}
		}
	}

	return nSamplesNum;
}
