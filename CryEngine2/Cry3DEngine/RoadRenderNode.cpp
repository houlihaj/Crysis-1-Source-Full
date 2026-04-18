#include "StdAfx.h"

#include "3dEngine.h"
#include "RoadRenderNode.h"
#include "CullBuffer.h"
#include "terrain.h"
#include "ObjMan.h"
#include "MeshCompiler/MeshCompiler.h"

CRoadRenderNode::CRoadRenderNode(void)
{
	m_pRenderMesh = NULL;
	m_pMaterial = NULL;
	m_arrTexCoors[0] = 0;
	m_arrTexCoors[1] = 1;
  m_pPhysEnt = NULL;
  m_pPhysGeom = NULL;
	m_sortPrio = 0;

	GetInstCount(eERType_RoadObject_NEW)++;
}

CRoadRenderNode::~CRoadRenderNode(void)
{
  DePhysicalize();
	Get3DEngine()->UnRegisterEntity(this);
	GetRenderer()->DeleteRenderMesh(m_pRenderMesh);
  Get3DEngine()->FreeRenderNodeState(this);

  Get3DEngine()->m_lstRoadRenderNodesForUpdate.Delete(this);

	GetInstCount(eERType_RoadObject_NEW)--;
}

void CRoadRenderNode::SetVertices( const Vec3 * pVertsAll, int nVertsNumAll, 
                                  float fTexCoordBegin, float fTexCoordEnd, 
                                  float fTexCoordBeginGlobal, float fTexCoordEndGlobal )
{
  if(pVertsAll != m_arrVerts.GetElements())
  {
    m_arrVerts.Clear();
    m_arrVerts.AddList((Vec3*)pVertsAll, nVertsNumAll);
  }

  m_arrTexCoors[0] = fTexCoordBegin;
  m_arrTexCoors[1] = fTexCoordEnd;

  m_arrTexCoorsGlobal[0] = fTexCoordBeginGlobal;
  m_arrTexCoorsGlobal[1] = fTexCoordEndGlobal;

  ScheduleRebuild();
}

void CRoadRenderNode::Compile()
{
  LOADING_TIME_PROFILE_SECTION(GetSystem());

  int nVertsNumAll = m_arrVerts.Count();

	assert(!(nVertsNumAll&1));

	if(nVertsNumAll<4)
		return;

	// free old object and mesh
	Get3DEngine()->UnRegisterEntity(this);
	if(m_pRenderMesh)
		GetRenderer()->DeleteRenderMesh(m_pRenderMesh);
	m_pRenderMesh = NULL;

  bool bCheckVoxels = false;

	// update object bbox
	m_WSBBox.Reset();
	for(int i=0; i<nVertsNumAll; i++)
	{
		Vec3 vTmp(m_arrVerts[i].x,m_arrVerts[i].y,GetTerrain()->GetZApr(m_arrVerts[i].x,m_arrVerts[i].y,true)+0.05f);
		m_WSBBox.Add(vTmp);
/*
    if(AffectedByVoxels)
    {
      float fTerrainOnly = GetTerrain()->GetZApr(m_arrVerts[i].x,m_arrVerts[i].y,false)+0.05f;
      if(fTerrainOnly!=vTmp.z)
        bCheckVoxels = true;
    }
    */
	}

  Vec3 vCenter = m_WSBBox.GetCenter();
//  PrintMessage("Compiling road sector (%d,%d,%d) ...", (int)vCenter.x, (int)vCenter.y, (int)vCenter.z);

	// prepare arrays for final mesh
	static PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> lstVerticesMerged; lstVerticesMerged.Clear();
	static PodArray<ushort> lstIndicesMerged; lstIndicesMerged.Clear();
	static PodArray<SPipTangents> lstTangMerged; lstTangMerged.Clear();

	float fChunksNum = (nVertsNumAll-2)/2;
	float fTexStep = (m_arrTexCoors[1] - m_arrTexCoors[0])/fChunksNum;

	// for every trapezoid
	for(int nVertId=0; nVertId<=nVertsNumAll-4; nVertId+=2)
	{
		const Vec3 * pVerts = &m_arrVerts[nVertId];

		if(	pVerts[0] == pVerts[1] ||
				pVerts[1] == pVerts[2] ||
				pVerts[2] == pVerts[3] ||
				pVerts[3] == pVerts[0])
			continue;

		// get texture coordinates range
		float arrTexCoors[2];
		arrTexCoors[0] = m_arrTexCoors[0]+fTexStep*(nVertId/2);
		arrTexCoors[1] = m_arrTexCoors[0]+fTexStep*(nVertId/2+1);

		// define clip planes
		Plane planes[4];
		planes[0].SetPlane(pVerts[0],pVerts[1],pVerts[1]+Vec3(0,0,1));
		planes[1].SetPlane(pVerts[2],pVerts[3],pVerts[3]+Vec3(0,0,-1));
		planes[2].SetPlane(pVerts[0],pVerts[2],pVerts[2]+Vec3(0,0,-1));
		planes[3].SetPlane(pVerts[1],pVerts[3],pVerts[3]+Vec3(0,0,1));

		// make trapezoid 2d bbox
		AABB WSBBox; 
		WSBBox.Reset();
		for(int i=0; i<4; i++)
		{
			Vec3 vTmp(pVerts[i].x,pVerts[i].y,0);
			WSBBox.Add(vTmp);
		}

		// make vert array
		int nUnitSize = GetTerrain()->GetHeightMapUnitSize();
		int x1 = int(WSBBox.min.x)/nUnitSize*nUnitSize;
		int x2 = int(WSBBox.max.x)/nUnitSize*nUnitSize+nUnitSize;
		int y1 = int(WSBBox.min.y)/nUnitSize*nUnitSize;
		int y2 = int(WSBBox.max.y)/nUnitSize*nUnitSize+nUnitSize;

		// make arrays of verts used in trapezoid area
		static PodArray<Vec3> lstVerts; lstVerts.Clear();
		for(int x = x1; x <= x2; x += nUnitSize)
		for(int y = y1; y <= y2; y += nUnitSize)
		{
			Vec3 vTmp(x,y,GetTerrain()->GetZApr(x,y,bCheckVoxels)+0.01f);
			lstVerts.Add(vTmp);
		}

		// make indices
		int dx = (x2-x1)/nUnitSize;
		int dy = (y2-y1)/nUnitSize;
		static PodArray<ushort> lstIndices; lstIndices.Clear();
		for(int x = 0; x < dx; x ++)
		{
			for(int y = 0; y < dy; y ++)
			{
				int nIdx0 = (x*(dy+1) + y);
				int nIdx1 = (x*(dy+1) + y+(dy+1));
				int nIdx2 = (x*(dy+1) + y+1);
				int nIdx3 = (x*(dy+1) + y+1+(dy+1));

				assert(nIdx3 < lstVerts.Count());

				lstIndices.Add(nIdx0);
				lstIndices.Add(nIdx1);
				lstIndices.Add(nIdx2);

				lstIndices.Add(nIdx1);
				lstIndices.Add(nIdx3);
				lstIndices.Add(nIdx2);
			}
		}

		// clip triangles
		int nOrigCount = lstIndices.Count();
		for(int i=0; i<nOrigCount; i+=3)
		{
			if(ClipTriangle(lstVerts, lstIndices, i, planes))
			{
				i-=3;
				nOrigCount-=3;
			}
		}

		if(lstIndices.Count()<3 || lstVerts.Count()<3)
			continue;

		// allocate tangent array
		static PodArray<SPipTangents> lstTang; lstTang.Clear();
		lstTang.PreAllocate(lstVerts.Count(),lstVerts.Count());

		int nStep = CTerrain::GetHeightMapUnitSize();

		// make real vertex data
		static PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> lstVertices; lstVertices.Clear();
		for(int i=0; i<lstVerts.Count(); i++)
		{
			struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F tmp;
			tmp.xyz = lstVerts[i];

			// do texgen
			float d0 = planes[0].DistFromPlane(tmp.xyz);
			float d1 = planes[1].DistFromPlane(tmp.xyz);
			float d2 = planes[2].DistFromPlane(tmp.xyz);
			float d3 = planes[3].DistFromPlane(tmp.xyz);

			float t = d0/(d0+d1);
			tmp.st[0] = (1-t)*fabs(arrTexCoors[0]) + t*fabs(arrTexCoors[1]);
			tmp.st[1] = d2/(d2+d3);

			int x = (int)tmp.xyz.x;
			int y = (int)tmp.xyz.y;

			Vec3 vNormal = GetTerrain()->GetTerrainSurfaceNormal(tmp.xyz, 0.25f, bCheckVoxels);
			//tmp.normal = vNormal;

			// calculate alpha value
			float fAlpha = 1.f;
			if(fabs(arrTexCoors[0]-m_arrTexCoorsGlobal[0])<0.01f)
				fAlpha = CLAMP(t,0,1.f);
			else if(fabs(arrTexCoors[1]-m_arrTexCoorsGlobal[1])<0.01f)
				fAlpha = CLAMP(1.f-t,0,1.f);

			tmp.color.bcolor[0] = 255;
			tmp.color.bcolor[1] = 255;
			tmp.color.bcolor[2] = 255;
			tmp.color.bcolor[3] = uint8(255.f*fAlpha);

			lstVertices.Add(tmp);

			Vec3 vBinorm = pVerts[1]-pVerts[0]; 
			vBinorm.Normalize();

			Vec3 vTang = pVerts[2]-pVerts[0]; 
			vTang.Normalize();

			vBinorm = -vNormal.Cross(vTang);
			vTang = vNormal.Cross(vBinorm);

			lstTang[i].Binormal = Vec4sf(tPackF2B(vBinorm.x),tPackF2B(vBinorm.y),tPackF2B(vBinorm.z), tPackF2B(-1));
			lstTang[i].Tangent  = Vec4sf(tPackF2B(vTang.x),tPackF2B(vTang.y),tPackF2B(vTang.z), tPackF2B(-1));
		}

		// shift indices
		for(int i=0; i<lstIndices.Count(); i++)
			lstIndices[i] += lstVerticesMerged.Count();

		lstIndicesMerged.AddList(lstIndices);
		lstVerticesMerged.AddList(lstVertices);
		lstTangMerged.AddList(lstTang);
	}

	mesh_compiler::CMeshCompiler meshCompiler;
  meshCompiler.WeldPos_VERTEX_FORMAT_P3F_COL4UB_TEX2F<ushort>( lstVerticesMerged, lstTangMerged, lstIndicesMerged, VEC_EPSILON, GetBBox() );

  DePhysicalize();

	// make render mesh
	if(lstIndicesMerged.Count())
	{
		m_pRenderMesh = GetRenderer()->CreateRenderMeshInitialized(
			lstVerticesMerged.GetElements(), lstVerticesMerged.Count(), VERTEX_FORMAT_P3F_COL4UB_TEX2F, 
			lstIndicesMerged.GetElements(), lstIndicesMerged.Count(), R_PRIMV_TRIANGLES,
			"RoadRenderNode",GetName(), eBT_Static, 1, 0, NULL, NULL, false, true, lstTangMerged.GetElements());

		m_pRenderMesh->SetChunk((m_pMaterial!=NULL) ? (IMaterial*)m_pMaterial : GetMatMan()->GetDefaultMaterial(),
			0,lstVerticesMerged.Count(), 0,lstIndicesMerged.Count());
		m_pRenderMesh->GetChunks()->Get(0)->m_vCenter = m_WSBBox.GetCenter();
		m_pRenderMesh->GetChunks()->Get(0)->m_fRadius = m_WSBBox.GetRadius();
		m_pRenderMesh->SetBBox(m_WSBBox.min, m_WSBBox.max);
	}

/*
	// physicalize every trapezoid
	for(int nVertId=0; nVertId<=nVertsNumAll-4; nVertId+=2)
	{
		const Vec3 * pVerts = &pVertsAll[nVertId];

		static PodArray<Vec3> lstVerts; lstVerts.Clear();
		static ushort g_idxBrd[] = { 0,1,2, 3,2,1, 4,6,5, 7,5,6, 0,2,6, 0,6,4, 3,1,5, 3,5,7 };
		lstVerts.PreAllocate(8,8);
		for(int i=0;i<8;i++)
		(lstVerts[i] = pVerts[i&3]).z += 0.3f*(1-(i>>1&2));
		if(m_pMaterial)
			Physicalize(g_idxBrd,sizeof(g_idxBrd)/sizeof(g_idxBrd[0]), lstVerts.GetElements(), m_pMaterial->GetSurfaceTypeId());
	}
*/
	// activate rendering
	Get3DEngine()->RegisterEntity(this);

//  PrintMessagePlus(" %d tris produced", m_pRenderMesh ? m_pRenderMesh->GetIndicesCount()/3 : 0);
}

void CRoadRenderNode::SetSortPriority(uint8 sortPrio)
{
	m_sortPrio = sortPrio;
}

bool CRoadRenderNode::Render(const SRendParams &_RendParams)
{
	if(!GetCVars()->e_roads)
		return false;

	SRendParams RendParams(_RendParams);

	CRenderObject * pObj = GetIdentityCRenderObject();
  if (!pObj)
    return false;

	pObj->m_DynLMMask = RendParams.nDLightMask;
	pObj->m_ObjFlags |= RendParams.dwFObjFlags;
	pObj->m_II.m_AmbColor = RendParams.AmbientColor;

  RendParams.nRenderList = EFSLIST_DECAL;

	RendParams.pShadowMapCasters = NULL;

	pObj->m_nRAEId =  m_sortPrio;

//	if (RendParams.pShadowMapCasters)
	pObj->m_ObjFlags |= (FOB_INSHADOW | FOB_NO_FOG);

	if(_RendParams.pTerrainTexInfo && (_RendParams.dwFObjFlags & (FOB_BLEND_WITH_TERRAIN_COLOR | FOB_AMBIENT_OCCLUSION)))
	{
		pObj->m_nTextureID = _RendParams.pTerrainTexInfo->nTex0;
		pObj->m_nTextureID1 = _RendParams.pTerrainTexInfo->nTex1;
		pObj->m_fTempVars[0] = _RendParams.pTerrainTexInfo->fTexOffsetX;
		pObj->m_fTempVars[1] = _RendParams.pTerrainTexInfo->fTexOffsetY;
		pObj->m_fTempVars[2] = _RendParams.pTerrainTexInfo->fTexScale;
		pObj->m_fTempVars[3] = _RendParams.pTerrainTexInfo->fTerrainMinZ;
		pObj->m_fTempVars[4] = _RendParams.pTerrainTexInfo->fTerrainMaxZ;
	}

	if(m_pRenderMesh)
		m_pRenderMesh->Render(RendParams, pObj, 
			(m_pMaterial!=NULL) ? (IMaterial*)m_pMaterial : GetMatMan()->GetDefaultMaterial());

	return (m_pRenderMesh != NULL);
}

bool CRoadRenderNode::ClipTriangle(PodArray<Vec3> & lstVerts, PodArray<ushort> & lstInds, int nStartIdxId, Plane * pPlanes)
{
	static PodArray<Vec3> lstPolygon; lstPolygon.Clear();
	lstPolygon.Add(lstVerts[lstInds[nStartIdxId+0]]);
	lstPolygon.Add(lstVerts[lstInds[nStartIdxId+1]]);
	lstPolygon.Add(lstVerts[lstInds[nStartIdxId+2]]);

	for(int i=0; i<4 && lstPolygon.Count()>=3; i++)
		CCoverageBuffer::ClipPolygon(&lstPolygon, pPlanes[i]);

	if(lstPolygon.Count() < 3)
	{
		lstInds.Delete(nStartIdxId, 3);
		return true; // entire triangle is clipped away
	}

	if(lstPolygon.Count() == 3)
		if(lstPolygon[0].IsEquivalent(lstVerts[lstInds[nStartIdxId+0]]))
			if(lstPolygon[1].IsEquivalent(lstVerts[lstInds[nStartIdxId+1]]))
				if(lstPolygon[2].IsEquivalent(lstVerts[lstInds[nStartIdxId+2]]))
					return false; // entire triangle is in

	// replace old triangle with several new triangles
	int nStartId = lstVerts.Count();
	lstVerts.AddList(lstPolygon);

	// put first new triangle into position of original one
	lstInds[nStartIdxId+0] = nStartId+0;
	lstInds[nStartIdxId+1] = nStartId+1;
	lstInds[nStartIdxId+2] = nStartId+2;

	// put others in the end
	for(int i=1; i<lstPolygon.Count()-2 ; i++)
	{
		lstInds.Add(nStartId+0);
		lstInds.Add(nStartId+i+1);
		lstInds.Add(nStartId+i+2);
	}

	return false;
}

float CRoadRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio_detail*GetViewDistRatioNormilized());

	return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio*GetViewDistRatioNormilized());
}

void CRoadRenderNode::SetMaterial(IMaterial * pMat) 
{ 
	m_pMaterial = pMat; 
}

void CRoadRenderNode::DePhysicalize()
{
  if(m_pPhysEnt)
  {
    GetPhysicalWorld()->GetGeomManager()->UnregisterGeometry(m_pPhysGeom);
    m_pPhysEnt->RemoveGeometry(0);
    GetPhysicalWorld()->DestroyPhysicalEntity(m_pPhysEnt);
    m_pPhysEnt = NULL;
  }
}

void CRoadRenderNode::Physicalize(ushort *pIndices,int nIndices, Vec3 *pVerts, int nPhysMatId)
{
  FUNCTION_PROFILER_3DENGINE;

  int flags = mesh_multicontact1 | mesh_SingleBB | mesh_no_vtx_merge | mesh_always_static | mesh_approx_box;

  int arrSurfaceTypesId[16];
  memset(arrSurfaceTypesId,0,sizeof(arrSurfaceTypesId));

  IGeomManager *pGeoman = GetPhysicalWorld()->GetGeomManager();
  IGeometry * pGeom = pGeoman->CreateMesh(pVerts, pIndices, NULL, NULL, nIndices/3, flags, 1e10f);
  m_pPhysGeom = pGeoman->RegisterGeometry(pGeom,nPhysMatId);
  pGeom->Release();

  assert(!m_pPhysEnt);
  m_pPhysEnt = GetPhysicalWorld()->CreatePhysicalEntity(PE_STATIC,NULL,NULL,PHYS_FOREIGN_ID_TERRAIN);

  pe_action_remove_all_parts remove_all;
  m_pPhysEnt->Action(&remove_all);

  pe_geomparams params;	  
	params.flags = geom_mat_substitutor;
  m_pPhysEnt->AddGeometry(m_pPhysGeom, &params);

  pe_params_flags par_flags;
  par_flags.flagsOR = pef_never_affect_triggers;
  m_pPhysEnt->SetParams(&par_flags);

  pe_params_pos par_pos;
  Matrix34 mat; mat.SetIdentity();
  mat.SetTranslation(Vec3(0,0,0.01f));
  par_pos.pMtx3x4 = &mat;
  m_pPhysEnt->SetParams(&par_pos);

  pe_params_foreign_data par_foreign_data;
  par_foreign_data.pForeignData = (IRenderNode*)this;
  par_foreign_data.iForeignData = PHYS_FOREIGN_ID_STATIC;
  m_pPhysEnt->SetParams(&par_foreign_data);
}

void CRoadRenderNode::GetMemoryUsage(ICrySizer * pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "Road");
	pSizer->AddObject(this, sizeof(*this));
}

void CRoadRenderNode::ScheduleRebuild()
{
  if(Get3DEngine()->m_lstRoadRenderNodesForUpdate.Find(this)<0)
    Get3DEngine()->m_lstRoadRenderNodesForUpdate.Add(this);
}

void CRoadRenderNode::OnTerrainChanged()
{
  if(!m_pRenderMesh)
    return;

  int nPosStride = 0;
  byte * pPos = m_pRenderMesh->GetStridedPosPtr(nPosStride,0,true);

  for(int i=0, nVertsNum = m_pRenderMesh->GetVertCount(); i<nVertsNum; i++)
  {
    Vec3 & vPos = *(Vec3*)&pPos[i*nPosStride];
    vPos.z = GetTerrain()->GetZApr(vPos.x,vPos.y,false)+0.01f;
  }

  m_pRenderMesh->InvalidateVideoBuffer();

  m_pRenderMesh->UnlockStream(VSF_GENERAL);
}

void CRoadRenderNode::OnRenderNodeBecomeVisible()
{
  assert(m_pRNTmpData);
  m_pRNTmpData->userData.objMat.SetIdentity();
  m_pRNTmpData->userData.fRadius = GetBBox().GetRadius();
}
