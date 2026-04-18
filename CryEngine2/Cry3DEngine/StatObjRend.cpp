////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjrend.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: prepare and add render element into renderer
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "../RenderDll/Common/Shadow_Renderer.h"
#include "LMCompStructures.h"

#include "IndexedMesh.h"
#include "VisAreas.h"
#include "GeomQuery.h"
#include "LMData.h"

void CStatObj::SetupBending(CRenderObject * pObj, Vec2 const& vBending)
{
  FUNCTION_PROFILER_3DENGINE;

	static float fBEND_AMPLITUDE = 0.002f;
	float fWaveAmp = fBEND_AMPLITUDE;

	const Vec3 &vObjPos = pObj->GetTranslation();
	pObj->m_vBending = vBending;
	if (GetCVars()->e_vegetation_wind)
	{
    float vrad = GetRadiusVert();
    float hrad = GetRadiusHors();
    float fBBoxRatio = vrad / hrad;
    fBBoxRatio = clamp_tpl<float>(fBBoxRatio, 0.5f, 1);

		pObj->m_fBendScale = fBBoxRatio * pObj->GetScaleX()  / (vrad * pObj->GetScaleZ());  
		fWaveAmp *= 1.f + 4.f * vBending.GetLength();
	}
	else
		// Flag for shader to use previous bending param scheme.
		pObj->m_fBendScale = 0.f;

	SWaveForm2 *pWF[2];
	pObj->AddWaves(pWF);
	SWaveForm2 *wf = pWF[0];
	wf->m_Level  = 0.000f; // between 0.001 and 0.1
	wf->m_Freq   = 1.0f/m_fRadiusVert/8.0f+0.2f; // between 0.001 and 0.1
	wf->m_Phase  = vObjPos.x/8.0f;
	wf->m_Amp    = fWaveAmp;

	wf = pWF[1];
	wf->m_Level  = 0.000f; // between 0.001 and 0.1
	wf->m_Freq   = 1.0f/m_fRadiusVert/7.0f+0.2f; // between 0.001 and 0.1
	wf->m_Phase  = vObjPos.y/8.0f;
	wf->m_Amp    = fWaveAmp;

  pObj->m_ObjFlags |= FOB_BENDED | FOB_VEGETATION;  
}

//////////////////////////////////////////////////////////////////////
void CStatObj::Render(const SRendParams & rParams)
{
	if (m_nFlags & STATIC_OBJECT_HIDDEN)
		return;

	FUNCTION_PROFILER_3DENGINE;

	if(m_pIndexedMesh && m_pIndexedMesh->m_bInvalidated)
	{
		MakeRenderMesh();
		m_pIndexedMesh->m_bInvalidated = false;
	}

	m_nLastRendFrameId = GetFrameID();

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// (Timur) Temporary, remove later.
	//////////////////////////////////////////////////////////////////////////
	if (GetCVars()->e_stat_obj_merge == 0 && m_bMerged)
	{
		UnMergeSubObjectsRenderMeshes();
	}
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	if (!(m_nFlags & STATIC_OBJECT_COMPOUND))
	{ // draw mesh, don't even try to render childs
		int nNewLod = 0;
    bool bRenderNodeValid(rParams.pRenderNode && ((int)(void*)(rParams.pRenderNode)>0));
    if(bRenderNodeValid)
		{
			float fScale = rParams.pMatrix->GetColumn0().GetLength();
			nNewLod = CObjManager::GetObjectLOD(rParams.fDistance, rParams.pRenderNode->GetLodRatioNormalized(), GetRadius()*fScale);
			int nMinLod = GetMinUsableLod();
			nNewLod += nMinLod;
			int nMaxUsableLod = std::min((int)m_nMaxUsableLod,GetCVars()->e_lod_max);
			if (rParams.pFoliage)
				nNewLod = 0;
			nNewLod = CLAMP(nNewLod, 0, nMaxUsableLod );
		}

    bool bMatLayersInUse = rParams.nMaterialLayersBlend || rParams.nMaterialLayers;

		if( !bMatLayersInUse && (rParams.nDLightMask<=15) && GetCVars()->e_dissolve && bRenderNodeValid && !(rParams.pRenderNode->m_nInternalFlags & IRenderNode::PER_OBJECT_SHADOW_PASS_NEEDED) && !m_nRenderStackLevel && !(rParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP) && rParams.nLodTransSlotId<LOD_TRANSITION_MAX_OBJ_STATES_NUM && rParams.ppRNTmpData )
		{
			bool bSetLodImmediately = false;

      Get3DEngine()->CheckCreateRNTmpData(rParams.ppRNTmpData, rParams.pRenderNode);
      CRNTmpData * pRNTmpData = *rParams.ppRNTmpData;

			bSetLodImmediately = (pRNTmpData->nCreatedFrameId == GetMainFrameID() || 
        fabs(1.f-pRNTmpData->userData.fLastFrameDistance/max(rParams.fDistance,0.001f)) > GetCVars()->e_dissolve_transition_threshold);

			SLodTransitionState * pState = &pRNTmpData->userData.states[rParams.nLodTransSlotId];

			if(pState->pStatObj && pState->pStatObj != this && pState->ucLods[0]==255)
				assert(0);

			pState->pStatObj = this;

			if(bSetLodImmediately)
				pState->ucLods[0] = pState->ucLods[1] = nNewLod;

			if(pState->ucLods[0] == pState->ucLods[1] && nNewLod != pState->ucLods[0])
			{ // initiate lod switch
				pState->ucLods[1] = nNewLod;
				pState->fStartTime = GetCurTimeSec();
			}
			else if(pState->ucLods[0] != pState->ucLods[1] && pState->fStartTime <= GetCurTimeSec() - GetCVars()->e_dissolve_transition_time)
			{ // finish transition
				pState->ucLods[0] = pState->ucLods[1];
			}

			float fState = (pState->ucLods[1] == pState->ucLods[0]) ? 0.f : SATURATE((GetCurTimeSec() - pState->fStartTime)/GetCVars()->e_dissolve_transition_time);
			float arrLodAmmount[2] = { 1.f-fState, fState };

			for(int i = 0; i<2; i++)
			{ 
				if(float fLodAmmount = SATURATE(arrLodAmmount[i]*2.f))
				{
					int nLod = pState->ucLods[i];
					if(nLod && m_arrpLowLODs[nLod] != 0 && GetCVars()->e_lods)
						m_arrpLowLODs[nLod]->RenderRenderMesh(rParams, nLod, fLodAmmount, rParams.pInstInfo);
					else
						RenderRenderMesh(rParams, 0, fLodAmmount, rParams.pInstInfo);
				}
			}
		}
		else 
		{
			if (nNewLod && m_arrpLowLODs[nNewLod] != 0 && GetCVars()->e_lods)
			{
				if (rParams.m_pScatterBuffer)
				{
					// Tbyte HACK: const_cast should be eliminated somehow
					SRendParams &rp = const_cast<SRendParams&>(rParams);
					rp.m_pScatterBuffer = &rParams.m_pScatterBuffer[nNewLod];
				}
				m_arrpLowLODs[nNewLod]->RenderRenderMesh(rParams, nNewLod, 1.f, rParams.pInstInfo);
			}
			else
				RenderRenderMesh(rParams, nNewLod, 1.f, rParams.pInstInfo);
		}
	}
	else
	{ // render subMeshes
		//////////////////////////////////////////////////////////////////////////
		// (Timur) Temporary, remove later.
		//////////////////////////////////////////////////////////////////////////
		if (GetCVars()->e_stat_obj_merge != 0 && !m_bMerged && !m_bUnmergable)
		{
			// Try to merge to merge sub meshes of this object.
			MergeSubObjectsRenderMeshes();
		}
		if (m_pMergedObject)
		{
			m_pMergedObject->Render( rParams );
		}
		else
		{
			//////////////////////////////////////////////////////////////////////////
			// Render SubMeshes if present.
			//////////////////////////////////////////////////////////////////////////
			if (m_nSubObjectMeshCount > 0)
			{
				Matrix34 *pOldTM = rParams.pMatrix;
				Matrix34 *pOldPrevTM = rParams.pPrevMatrix;
				SRendParams &rp = const_cast<SRendParams&>(rParams);
				Matrix34 renderTM, renderPrevTM;

				Matrix34 tm, tmPrev;
				assert(rParams.pMatrix);
				renderTM = *rParams.pMatrix;

				if( rParams.pPrevMatrix )
				{
					renderPrevTM  = *rParams.pPrevMatrix;
				}

				//			assert(rParams.m_LMDataCount==m_subObjects.size() || rParams.m_pLMData==0);
				SLMData* pLMSubObjData	=	rParams.m_pLMData;
				IRenderMesh** pVertexScatteringSubObj = rParams.m_pScatterBuffer;
				for (uint i = 0; i < m_subObjects.size(); i++)
				{
					if (m_subObjects[i].nType != STATIC_SUB_OBJECT_MESH) // all the meshes are at the beginning of the array.
						break;

					if (!m_subObjects[i].pStatObj || m_subObjects[i].bHidden)
						continue;
					if(pLMSubObjData)
						rp.m_pLMData	=	&pLMSubObjData[i];
					if(pVertexScatteringSubObj)
						rp.m_pScatterBuffer	=	&pVertexScatteringSubObj[i];
					rp.pWeights = m_subObjects[i].pWeights;
					rp.nLodTransSlotId = i;
					if (m_subObjects[i].bIdentityMatrix)
					{
						rp.pMatrix = &renderTM;
						if( rParams.pPrevMatrix )
							rp.pPrevMatrix = &renderPrevTM ;          				

						m_subObjects[i].pStatObj->Render(rParams);
					}
					else
					{
						tm = renderTM * m_subObjects[i].tm;
						rp.pMatrix = &tm;

						if( rParams.pPrevMatrix )
						{
							tmPrev = renderPrevTM * m_subObjects[i].tm;
							rp.pPrevMatrix = &tmPrev;
						}

						m_subObjects[i].pStatObj->Render(rp);
					}
				}
				rp.pMatrix = pOldTM;
				rp.pPrevMatrix = pOldPrevTM;
				rp.nLodTransSlotId = 0;
			}

			if (GetCVars()->e_debug_draw)
			{
				IMaterial *pMaterial = 0;
				RenderDebugInfo(const_cast<SRendParams&>(rParams),0, pMaterial, 0 );
			}
		}

		//////////////////////////////////////////////////////////////////////////
	}
}

void CStatObj::RenderRenderMesh(const SRendParams & rParams, int nLod, float fLodAmmount, SInstancingInfo * pInstInfo)
{
  FUNCTION_PROFILER_3DENGINE;

  m_nLastRendFrameId = GetFrameID();

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Specify transformation
	////////////////////////////////////////////////////////////////////////////////////////////////////

	CRenderObject * pObj;
	IRenderer * pRend = GetRenderer();
	pObj = pRend->EF_GetObject(true, -1);
  if (!pObj)
    return;
	pObj->m_pID = this;
	pObj->m_pCharInstance = rParams.pFoliage;
	if (pObj->m_pCharInstance)
		pObj->m_ObjFlags |= FOB_CHARACTER;

  uint8 nAlphaRef = rParams.nAlphaRef;
  if((nAlphaRef = (uint8)max((float)nAlphaRef, 255.f - 255.f*fLodAmmount))>0)
    pObj->m_ObjFlags |= FOB_DISSOLVE;

	pObj->m_AlphaRef = nAlphaRef;

	pObj->m_fSort = rParams.fCustomSortOffset;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// pass Heightmap adaption parameters
	////////////////////////////////////////////////////////////////////////////////////////////////////

	//pass Heightmap adaption coefficents
	pObj->m_HMAData	=	rParams.m_HMAData;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// pass pointer to VisArea for AmbientCube queries
	////////////////////////////////////////////////////////////////////////////////////////////////////

	pObj->m_pVisArea		=	rParams.m_pVisArea;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Set flags
	////////////////////////////////////////////////////////////////////////////////////////////////////

	pObj->m_ObjFlags |= rParams.dwFObjFlags;
	SRenderObjData *pObjData = NULL;

  if(pInstInfo)
  {
    pObjData = pObj->GetObjData(true);
    pObjData->m_pInstancingInfo = &pInstInfo->arrMats;
  }

  assert(rParams.pMatrix);
  {
    pObj->m_II.m_Matrix = *rParams.pMatrix;  
    pObj->m_prevMatrix = *rParams.pMatrix;
    if( rParams.nMotionBlurAmount && rParams.pPrevMatrix && !pObj->m_II.m_Matrix.IsEquivalent(*rParams.pPrevMatrix, 0.001f) )
    {
      pObj->m_nMotionBlurAmount = 128;
      pObj->m_prevMatrix = *rParams.pPrevMatrix;
    }
  }

	pObj->m_II.m_AmbColor = rParams.AmbientColor;

	if (GetCVars()->e_shadows)
		pObj->m_pShadowCasters = rParams.pShadowMapCasters;
	else
		pObj->m_pShadowCasters=0;

	pObj->m_ObjFlags |= FOB_INSHADOW;

	assert(!pObj->m_CustomData);
	if (rParams.pCCObjCustomData)
		pObj->m_CustomData = rParams.pCCObjCustomData;

	pObj->m_fAlpha = rParams.fAlpha;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Process bending
	////////////////////////////////////////////////////////////////////////////////////////////////////

	if(GetCVars()->e_vegetation_bending && pObj->m_ObjFlags & FOB_VEGETATION )
	{
    float fBendAmount = fabs_tpl(rParams.vBending.x + rParams.vBending.y) + fabs_tpl(pObj->m_fBendScale);
    
    // Disable bending/detail bending with barely any movement..
		if( fBendAmount >= 0.00005f)
			SetupBending(pObj, rParams.vBending);
	}
	else
	{
		pObj->m_vBending = rParams.vBending;
	}

  if( rParams.nVisionParams && !(rParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP) && !(pObj->m_ObjFlags & FOB_VEGETATION))
    pObj->m_nVisionParams = rParams.nVisionParams;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Set render quality
	////////////////////////////////////////////////////////////////////////////////////////////////////

	pObj->m_fRenderQuality = rParams.fRenderQuality;
	pObj->m_fDistance =	rParams.fDistance;

	pObj->m_DynLMMask = rParams.nDLightMask;


#ifdef _DEBUG
	int nCount = 0;
	for(int i=0; i<32; i++)
	{
		if(pObj->m_DynLMMask & (1<<i))
			nCount++;
	}
	if(nCount>16)
	{
		Warning( "Warning: CStatObj::Render: no more than 16 lsources can be requested" );
	}
#endif

	if (rParams.m_pScatterBuffer)
	{
		if(!pObjData)
			pObjData = pObj->GetObjData(true);
		pObjData->m_pScatterBuffer = rParams.m_pScatterBuffer[0];
		pObjData->m_VertexScatterTransformZ = rParams.mVertexScatterTransformZ;
	}

	if(rParams.m_pLMData && rParams.m_pLMData->m_pLMData && !(rParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP) && !nLod)
	{
    if(!pObjData)
      pObjData = pObj->GetObjData(true);

		// set object lmaps and texture coordinates
		pObjData->m_pLMTCBufferO = rParams.m_pLMData->m_pLMTCBuffer;
		pObj->m_nRAEId = rParams.m_pLMData->m_pLMData->GetRAETex();
	}
	else
	{
		//clear, when exchange the state of pLightMapInfo to NULL, the pObj parameters must be update...
		pObj->m_nRAEId = 0;
	}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Add render elements
////////////////////////////////////////////////////////////////////////////////////////////////////

	IMaterial * pMaterial = m_pMaterial;
	if (GetCVars()->e_materials && !m_bDefaultObject && rParams.pMaterial)
		pMaterial = rParams.pMaterial;

  // prepare multi-layer stuff to render object
  if( !rParams.nMaterialLayersBlend && rParams.nMaterialLayers ) 
  {
    uint8 nFrozenLayer = ( rParams.nMaterialLayers&(1<<0) )? 255: 0;
    uint8 nWetLayer = ( rParams.nMaterialLayers&(1<<1) )? 255: 0;
    pObj->m_nMaterialLayers = (uint32) (nFrozenLayer<< 24) | (nWetLayer<<16);
  }
  else
    pObj->m_nMaterialLayers = rParams.nMaterialLayersBlend;

	if(rParams.pTerrainTexInfo && (rParams.dwFObjFlags & (FOB_BLEND_WITH_TERRAIN_COLOR | FOB_AMBIENT_OCCLUSION)))
	{
		pObj->m_nTextureID = rParams.pTerrainTexInfo->nTex0;
		pObj->m_nTextureID1 = rParams.pTerrainTexInfo->nTex1;
		pObj->m_fTempVars[0] = rParams.pTerrainTexInfo->fTexOffsetX;
		pObj->m_fTempVars[1] = rParams.pTerrainTexInfo->fTexOffsetY;
		pObj->m_fTempVars[2] = rParams.pTerrainTexInfo->fTexScale;
		pObj->m_fTempVars[3] = rParams.pTerrainTexInfo->fTerrainMinZ - GetCamera().GetPosition().z;
		pObj->m_fTempVars[4] = rParams.pTerrainTexInfo->fTerrainMaxZ - GetCamera().GetPosition().z;
	}

	bool bNotCurArea = false;

/*	if(GetCVars()->e_overlay_geometry >= 2)
	{
		bNotCurArea = (!rParams.pCaller || !GetVisAreaManager() ||
			((CVisArea *)rParams.pCaller->GetEntityVisArea() && !((CVisArea *)rParams.pCaller->GetEntityVisArea())->
			FindVisArea((CVisArea *)GetVisAreaManager()->m_pCurArea,1, true))); 

		SetShaderFloat("offset", 0);
	}
	else if(GetCVars()->e_overlay_geometry < 2 )
		m_ShaderParams.SetNum(0);*/


	if (GetCVars()->e_debug_draw)
	{
		if (RenderDebugInfo(const_cast<SRendParams&>(rParams),pObj,pMaterial,nLod))
			return;
	}
 
  if( rParams.pFoliage )
  {
    pObj->m_ObjFlags |= FOB_VEGETATION;
  }

  if (IRenderMesh *pMesh = m_pRenderMesh)
  {
    if(GetCVars()->e_debug_lights && rParams.pRenderNode)
      pMesh->RenderDebugLightPass(pObj->m_II.m_Matrix,	pObj->m_DynLMMask, GetCVars()->e_debug_lights, GetCVars()->e_max_entity_lights);
    else
    {
      // Some bug here: pFoliage is existing but SkinnedChunks not.
      bool bSkinned = (rParams.pFoliage != NULL && pMesh->GetChunksSkinned() != NULL);
      pMesh->Render(rParams, pObj, pMaterial, bSkinned);
    }
  }
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::RenderDebugInfo( SRendParams & rParams, CRenderObject *pObj, IMaterial* &pMaterial, int nRndLod )
{
	if(rParams.nTechniqueID || (rParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP) || m_nRenderStackLevel)
		return false;

  IRenderer * pRend = GetRenderer();
	IRenderAuxGeom *pAuxGeom = GetRenderer()->GetIRenderAuxGeom();

	Matrix34 tm;
	assert(rParams.pMatrix);
	tm = *rParams.pMatrix;

	AABB bbox(m_vBoxMin,m_vBoxMax);
	
	bool bOnlyBoxes = GetCVars()->e_debug_draw == -1;

	if (GetCVars()->e_debug_draw == 10)
	{
		if (m_pRenderMesh)
			m_pRenderMesh->DebugDraw( *rParams.pMatrix,0 );
		return true;
	}
	if ((GetCVars()->e_debug_draw == 11 || GetCVars()->e_debug_draw == 12))
	{
		string shortName = PathUtil::GetFile(m_szFileName);
		float color[4] = {1,1,1,1};
		IRenderMesh *pRenderMeshOcclusion = m_pRenderMeshOcclusion;
		if (m_pLod0)
			pRenderMeshOcclusion = m_pLod0->m_pRenderMeshOcclusion;
		if (pRenderMeshOcclusion)
		{
			pRenderMeshOcclusion->DebugDraw( *rParams.pMatrix,1 );
			int nTris = pRenderMeshOcclusion->GetSysIndicesCount()/3;
			pRend->DrawLabelEx( tm.GetTranslation(),1.3f,color,true,true,"%s\n%d",shortName.c_str(),nTris);
		}
		if (GetCVars()->e_debug_draw == 12)
			return true;
		else
			return false;
	}

	if (GetCVars()->e_debug_draw == 1 || bOnlyBoxes)
	{
		if (!m_bMerged)
			pAuxGeom->DrawAABB( bbox,tm,false,ColorB(0,255,255,128),eBBD_Faceted );
		else
			pAuxGeom->DrawAABB( bbox,tm,false,ColorB(255,200,0,128),eBBD_Faceted );
	}

	/*
	// scaled bbox (frustum culling check)
	if(GetCVars()->e_debug_draw!=8)
  if (GetCVars()->e_debug_draw > 2 && !bOnlyBoxes)
  {
		AABB bbox(m_vBoxMin,m_vBoxMax);
		bbox.min *= 1.5f;
		bbox.max *= 1.5f;
		bbox.SetTransformedAABB(tm,bbox);
		pAuxGeom->DrawAABB( bbox,false,ColorB(255,0,255,128),eBBD_Faceted );
  }
	*/

	int e_debug_draw = GetCVars()->e_debug_draw;
	bool bNoText = e_debug_draw < 0;
	if (e_debug_draw < 0 && e_debug_draw != 1)
		e_debug_draw = -e_debug_draw;

	if (m_nRenderTrisCount > 0 && !bOnlyBoxes)
	{ // cgf's name and tris num

		int nMaxUsableLod = (m_pLod0) ? m_pLod0->m_nMaxUsableLod : m_nMaxUsableLod;
		int nRealNumLods = (m_pLod0) ? m_pLod0->m_nLoadedLodsNum : m_nLoadedLodsNum;
		int nNumLods = nRealNumLods;
		if (nNumLods > nMaxUsableLod+1)
			nNumLods = nMaxUsableLod+1;
		int nLod = nRndLod+1;
		if (nLod > nNumLods)
			nLod = nNumLods;

		Vec3 pos = tm.GetTranslation();
		float color[4] = {1,1,1,1};
		int nMats = (m_pRenderMesh && m_pRenderMesh->GetChunks()) ? m_pRenderMesh->GetChunks()->Count() : 0;
		int nRenderMats=0;

		if(nMats)
		{
			for(int i=0;i<nMats;++i)
			{	
				CRenderChunk &rc = m_pRenderMesh->GetChunks()->GetAt(i);
				if(rc.pRE && rc.nNumIndices && rc.nNumVerts && ((rc.m_nMatFlags&MTL_FLAG_NODRAW)==0))
					++nRenderMats;
			}
		}

		switch (e_debug_draw)
		{
		case 1:
			{
				const char *shortName = "";
				if (!m_szGeomName.empty())
					shortName = m_szGeomName.c_str();
				else
					shortName = PathUtil::GetFile(m_szFileName);
				if (nNumLods > 1)
					pRend->DrawLabelEx( pos,1.3f,color,true,true,"%s\n%d (LOD %d/%d)",shortName,m_nRenderTrisCount,nLod,nNumLods );
				else
					pRend->DrawLabelEx( pos,1.3f,color,true,true,"%s\n%d",shortName,m_nRenderTrisCount );
			}
			break;

		case 2:
			{
				//////////////////////////////////////////////////////////////////////////
				// Show colored poly count.
				//////////////////////////////////////////////////////////////////////////
				int fMult = 1;
				int nTris = m_nRenderTrisCount;
				ColorB clr = ColorB(255,255,255,255);
				if (nTris >= 20000*fMult)
					clr = ColorB(255,0,0,255);
				else if (nTris >= 10000*fMult)
					clr = ColorB(255,255,0,255);
				else if (nTris >= 5000*fMult)
					clr = ColorB(0,255,0,255);
				else if (nTris >= 2500*fMult)
					clr = ColorB(0,255,255,255);
				else if (nTris > 1250*fMult)
					clr = ColorB(0,0,255,255);
				else
					clr = ColorB(nTris/10,nTris/10,nTris/10,255);

				if (pMaterial)
					pMaterial = GetMatMan()->GetDefaultHelperMaterial();
				if (pObj)
				{
					pObj->m_II.m_AmbColor = ColorF(clr.r/155.0f,clr.g/155.0f,clr.b/155.0f,1);
					pObj->m_nMaterialLayers = 0;
				}
				
				if (!bNoText)
					pRend->DrawLabelEx( pos,1.3f,color,true,true,"%d",m_nRenderTrisCount );

				return false;
				//////////////////////////////////////////////////////////////////////////
			}
			break;

		case 3:
			{
				//////////////////////////////////////////////////////////////////////////
				// Show Lods
				//////////////////////////////////////////////////////////////////////////
				ColorB clr;
				if (nNumLods < 2)
				{
					if (m_nRenderTrisCount <= GetCVars()->e_lod_min_tris  || nRealNumLods > 1)
					{
						clr = ColorB(50,50,50,255);
					}
					else
					{
						clr = ColorB(255,0,0,255);
						clr.g = 127 + (uint8)(sin(GetTimer()->GetCurrTime()*10.0f)*120);
					}
				}
				else
				{
					if (nLod == 1)
						clr = ColorB(255,0,0,255);
					else if (nLod == 2)
						clr = ColorB(0,255,0,255);
					else if (nLod == 3)
						clr = ColorB(0,0,255,255);
					else if (nLod == 4)
						clr = ColorB(0,255,255,255);
					else
						clr = ColorB(255,255,255,255);
				}

				if (pMaterial)
					pMaterial = GetMatMan()->GetDefaultHelperMaterial();
				if (pObj)
				{
					pObj->m_II.m_AmbColor = ColorF(clr.r/155.0f,clr.g/155.0f,clr.b/155.0f,1);
					pObj->m_nMaterialLayers = 0;
				}

        bool bRenderNodeValid(rParams.pRenderNode && ((int)(void*)(rParams.pRenderNode)>0));
        int nMaxUsableLod = std::min((int)m_nMaxUsableLod,GetCVars()->e_lod_max);
				if (nNumLods > 1 && !bNoText)
          pRend->DrawLabelEx( pos,1.3f,color,true,true,"%d/%d (%d/%.1f/%d)",
            nLod, nNumLods, 
            bRenderNodeValid ? rParams.pRenderNode->GetLodRatio() : -1, rParams.fDistance, nMaxUsableLod);

				return false;
				//////////////////////////////////////////////////////////////////////////
			}
			break;

		case 4:
			{
				// Show texture usage.
        if(m_pRenderMesh)
				{
					int nTexMemUsage = m_pRenderMesh->GetTextureMemoryUsage( pMaterial );
					pRend->DrawLabelEx( pos,1.3f,color,true,true,"%d",nTexMemUsage/1024 );		// in KByte
				}
			}
			break;

		case 5:
			{
				//////////////////////////////////////////////////////////////////////////
				// Show Num Render materials.
				//////////////////////////////////////////////////////////////////////////
				ColorB clr;
				if (nRenderMats == 1)
					clr = ColorB(0,0,255,255);
				else if (nRenderMats == 2)
					clr = ColorB(0,255,255,255);
				else if (nRenderMats == 3)
					clr = ColorB(0,255,0,255);
				else if (nRenderMats == 4)
					clr = ColorB(255,0,255,255);
				else if (nRenderMats == 5)
					clr = ColorB(255,255,0,255);
				else if (nRenderMats >= 6)
					clr = ColorB(255,0,0,255);
				else if (nRenderMats >= 11)
					clr = ColorB(255,255,255,255);

				if (pMaterial)
					pMaterial = GetMatMan()->GetDefaultHelperMaterial();
				if (pObj)
				{
					pObj->m_II.m_AmbColor = ColorF(clr.r/155.0f,clr.g/155.0f,clr.b/155.0f,1);
					pObj->m_nMaterialLayers = 0;
				}

				if (!bNoText)
					pRend->DrawLabelEx( pos,1.3f,color,true,true,"%d",nRenderMats );
			}
			break;
		
		case 6:
			{
				if (pMaterial)
					pMaterial = GetMatMan()->GetDefaultHelperMaterial();
				if (pObj)
				{
					pObj->m_nMaterialLayers = 0;
				}
				
				pRend->DrawLabelEx( pos,1.3f,color,true,true,"%d,%d,%d,%d",
					(int)(rParams.AmbientColor.r*255.0f),(int)(rParams.AmbientColor.g*255.0f),(int)(rParams.AmbientColor.b*255.0f),(int)(rParams.AmbientColor.a*255.0f)
					);
			}
			break;
		
		case 7:
      if(m_pRenderMesh)
			{
				int nTexMemUsage = m_pRenderMesh->GetTextureMemoryUsage( pMaterial );
				pRend->DrawLabelEx( pos,1.3f,color,true,true,"%d,%d,%d",m_nRenderTrisCount,nRenderMats,nTexMemUsage/1024 );
			}
			break;
		
		case 13:
			{
				float fOcclusion = GetOcclusionAmount();
				pRend->DrawLabelEx( pos,1.3f,color,true,true,"%.2f", fOcclusion);
			}
			break;
		}
	}

	if (GetCVars()->e_debug_draw == 15 && !bOnlyBoxes)
	{
		// helpers
		for (unsigned int i = 0; i < m_subObjects.size(); i++)
		{
			SSubObject *pSubObject = &(m_subObjects[i]);
			if (pSubObject->nType == STATIC_SUB_OBJECT_MESH && pSubObject->pStatObj)
				continue;

			// make object matrix
			Matrix34 tMat = tm * pSubObject->tm;
			Vec3 pos = tMat.GetTranslation();

			// draw axes 
			//DrawMatrix(tMat);
			float s = 0.02f;
      ColorB col(0,255,255,255);
			pAuxGeom->DrawAABB( AABB( Vec3(-s,-s,-s),Vec3(s,s,s) ),tMat,false,col,eBBD_Faceted );      
      pAuxGeom->DrawLine(pos + s*tMat.GetColumn1(), col, pos + 3.f*s*tMat.GetColumn1(), col);
                  
			// text
			float color[4] = {0,1,1,1};
			pRend->DrawLabelEx( pos, 1.3f, color,true,true, "%s", pSubObject->name.c_str( ));
		}
	}
	return false;
}

void CStatObj::DrawMatrix(const Matrix34 & tMat)
{
  IRenderer * pRend = GetRenderer();

	Vec3 vTrans = tMat.GetTranslation();

	float color[] = {1,1,1,1};

	//CHANGED_BY_IVO
	pRend->SetMaterialColor(1,0,0,0);
	//pRend->Draw3dBBox( vTrans, vTrans+0.05f*Vec3(tMat.m_values[0]), true );
	//pRend->DrawLabelEx(vTrans+0.05f*Vec3(tMat.m_values[0]), 0.75f,color,false,true,"x");  
	pRend->Draw3dBBox( vTrans, vTrans+0.05f*tMat.GetColumn(0), true );
	        pRend->DrawLabelEx(vTrans+0.05f*tMat.GetColumn(0), 0.75f,color,false,true,"x");  

	pRend->SetMaterialColor(0,1,0,0);
	//pRend->Draw3dBBox( vTrans, vTrans+0.05f*Vec3(tMat.m_values[1]), true );
	//pRend->DrawLabelEx(vTrans+0.05f*Vec3(tMat.m_values[1]), 0.75f,color,false,true,"y");  
	pRend->Draw3dBBox( vTrans, vTrans+0.05f*tMat.GetColumn(1), true );
	        pRend->DrawLabelEx(vTrans+0.05f*tMat.GetColumn(1), 0.75f,color,false,true,"y");  

	pRend->SetMaterialColor(0,0,1,0);
	//pRend->Draw3dBBox( vTrans, vTrans+0.05f*Vec3(tMat.m_values[2]), true );
	//pRend->DrawLabelEx(vTrans+0.05f*Vec3(tMat.m_values[2]), 0.75f,color,false,true,"z");  
	pRend->Draw3dBBox( vTrans, vTrans+0.05f*tMat.GetColumn(2), true );
	        pRend->DrawLabelEx(vTrans+0.05f*tMat.GetColumn(2), 0.75f,color,false,true,"z");  
}

//
// StatObj functions.
//

float CStatObj::ComputeExtent(GeomQuery& geo, EGeomForm eForm)
{
	int nSubCount = GetSubObjectCount();
	geo.AllocParts(nSubCount+1);

	// Create parts for main and sub-objects.
	float fExtent = 0.f;
	if (m_pRenderMesh)
		fExtent += geo.GetPart(0).GetExtent(m_pRenderMesh, eForm);

	// Evaluate sub-objects.
	for (int i = 0; i < nSubCount; i++)
	{
		IStatObj::SSubObject* pSub = GetSubObject(i);
		if (pSub->nType==STATIC_SUB_OBJECT_MESH && pSub->pStatObj)
			fExtent += geo.GetPart(1+i).GetExtent(pSub->pStatObj, eForm);
	}
	return fExtent;
}

void CStatObj::GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm)
{
	geo.GetExtent(this, eForm);
	int i = geo.GetRandomPart();
	if (i == 0)
	{
		if (m_pRenderMesh)
		{
			m_pRenderMesh->GetRandomPos(ran, geo.GetPart(0), eForm);
			return;
		}
	}
	else
	{
		IStatObj::SSubObject* pSub = GetSubObject(i-1);
		if (pSub->nType==STATIC_SUB_OBJECT_MESH && pSub->pStatObj)
		{
			pSub->pStatObj->GetRandomPos(ran, geo.GetPart(i), eForm);
			return;
		}
	}

	ran.vPos.zero();
	ran.vNorm.zero();
}

int CStatObj::GetMinUsableLod() 
{ 
  return clamp_tpl(GetCVars()->e_lod_min,0,(int)m_nMinUsableLod0);
}
