////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjmanshadows.cpp
//  Version:     v1.00
//  Created:     2/6/2002 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Shadow casters/receivers relations
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "3dEngine.h"
#include <AABBSV.h>
#include "Vegetation.h"
#include "LMCompStructures.h"
#include "Brush.h"
#include "LightEntity.h"
#include "ObjectsTree.h"

bool IsAABBInsideHull(SPlaneObject * pHullPlanes, int nPlanesNum, const AABB & aabbBox);

#define FORCED_SHADOWS_CAST_VIEW_DIST_RATIO 10000.0f

void CObjManager::MakeShadowCastersList(CVisArea*pArea, const AABB & aabbReceiver, int dwAllowedTypes, Vec3 vLightPos, CDLight * pLight, ShadowMapFrustum * pFr, PodArray<SPlaneObject> * pShadowHull )
{
	FUNCTION_PROFILER_3DENGINE;

	assert(pLight && vLightPos.len()>1); // world space pos required

	pFr->pCastersList->Clear();

	float old_e_shadows_cast_view_dist_ratio = GetCVars()->e_shadows_cast_view_dist_ratio;
	if (pFr->bForSubSurfScattering)
		GetCVars()->e_shadows_cast_view_dist_ratio = FORCED_SHADOWS_CAST_VIEW_DIST_RATIO;

  static ICVar*	pVarShadowGenGS = GetConsole()->GetCVar("r_ShadowGenGS");
  bool bUseFrustumTest = false;
  if (!pVarShadowGenGS || (pVarShadowGenGS && pVarShadowGenGS->GetIVal()!=1))
    bUseFrustumTest = true;

  CVisArea * pLightArea = pLight->m_pOwner ? (CVisArea*)pLight->m_pOwner->GetEntityVisArea() : NULL;

	if(pArea)
	{
    if(pArea->m_pObjectsTree)
      pArea->m_pObjectsTree->FillShadowCastersList(pLight, pFr, pShadowHull, bUseFrustumTest);

    if(pLightArea)
    {
      // check neighbor sectors and portals if light and object are not in same area
      if(!(pLight->m_Flags & DLF_THIS_AREA_ONLY))
      {
        for(int p=0; p<pArea->m_lstConnections.Count(); p++)
        {
          CVisArea * pN = pArea->m_lstConnections[p];
          if(pN->m_pObjectsTree)
            pN->m_pObjectsTree->FillShadowCastersList(pLight, pFr, pShadowHull, bUseFrustumTest);

          for(int p=0; p<pN->m_lstConnections.Count(); p++)
          {
            CVisArea * pNN = pN->m_lstConnections[p];
            if(pNN != pLightArea && pNN->m_pObjectsTree)
              pNN->m_pObjectsTree->FillShadowCastersList(pLight, pFr, pShadowHull, bUseFrustumTest );
          }
        }
      }
      else if(!pLightArea->IsPortal())
      { // visit also portals
        for(int p=0; p<pArea->m_lstConnections.Count(); p++)
        {
          CVisArea * pN = pArea->m_lstConnections[p];
          if(pN->m_pObjectsTree)
            pN->m_pObjectsTree->FillShadowCastersList(pLight, pFr, pShadowHull, bUseFrustumTest);
        }
      }
    }
	}
	else
	{	
    static PodArray<CTerrainNode*> lstCastingNodes; lstCastingNodes.Clear();

		if(pFr->bUseVarianceSM && pFr->bUseAdditiveBlending)
		{
      if(GetCVars()->e_terrain && Get3DEngine()->m_bShowTerrainSurface)
      {
			  // get terrain sectors

			  // find all caster sectors
			  GetTerrain()->IntersectWithShadowFrustum(&lstCastingNodes, pFr );

			  // make list of entities
			  for(int s=0; s<lstCastingNodes.Count(); s++)
			  {
				  IShadowCaster * pNode = (IShadowCaster*)lstCastingNodes[s];
				  pFr->pCastersList->Add(pNode);
			  }
      }
		}
		else if(Get3DEngine()->m_pObjectsTree)
		{
			if (pFr->bForSubSurfScattering)
				Get3DEngine()->m_pObjectsTree->FillShadowCastersList(pLight, pFr, NULL, false);
			else
			Get3DEngine()->m_pObjectsTree->FillShadowCastersList(pLight, pFr, pShadowHull, bUseFrustumTest);

			// check also visareas effected by sun
      {
        PodArray<CVisArea*> & lstAreas = GetVisAreaManager()->m_lstVisAreas;
			  for(int i=0; i<lstAreas.Count(); i++)
				  if(lstAreas[i]->IsAffectedByOutLights() && lstAreas[i]->m_pObjectsTree)
            if(!bUseFrustumTest || pFr->Intersect(*lstAreas[i]->GetAABBox()))
              if(pShadowHull && IsAABBInsideHull(pShadowHull->GetElements(), pShadowHull->Count(), *lstAreas[i]->GetAABBox()))
    					  lstAreas[i]->m_pObjectsTree->FillShadowCastersList(pLight, pFr, pShadowHull, bUseFrustumTest );
      }
      {
        PodArray<CVisArea*> & lstAreas = GetVisAreaManager()->m_lstPortals;
        for(int i=0; i<lstAreas.Count(); i++)
          if(lstAreas[i]->IsAffectedByOutLights() && lstAreas[i]->m_pObjectsTree)
            if(!bUseFrustumTest || pFr->Intersect(*lstAreas[i]->GetObjectsBBox()))
              if(pShadowHull && IsAABBInsideHull(pShadowHull->GetElements(), pShadowHull->Count(), *lstAreas[i]->GetObjectsBBox()))
                lstAreas[i]->m_pObjectsTree->FillShadowCastersList(pLight, pFr, pShadowHull, bUseFrustumTest );
      }
		
      bool bSun = (pLight->m_Flags&DLF_SUN) != 0;

      if(bSun && GetCVars()->e_terrain && (pFr->bForSubSurfScattering || GetCVars()->e_shadows_from_terrain_in_all_lods) && Get3DEngine()->m_bShowTerrainSurface)
      {
        // find all caster sectors
        GetTerrain()->IntersectWithShadowFrustum(&lstCastingNodes, pFr );

        // make list of entities
        for(int s=0; s<lstCastingNodes.Count(); s++)
        {
          CTerrainNode * pNode = (CTerrainNode*)lstCastingNodes[s];

          if (!pFr->bForSubSurfScattering)
          {
          if(pNode->m_nGSMFrameId == GetFrameID())
            continue;

            if(bSun && pFr->FrustumPlanes.IsAABBVisible_EH(pNode->GetBBoxVirtual()) == CULL_INCLUSION)
            pNode->m_nGSMFrameId = GetFrameID(); // mark as completely present in some LOD 
          }

          pFr->pCastersList->Add(pNode);
        }
      }
    }
	}
	GetCVars()->e_shadows_cast_view_dist_ratio = old_e_shadows_cast_view_dist_ratio;
}

PodArray<ShadowMapFrustum*> * CObjManager::GetShadowFrustumsList( PodArray<CDLight*> * pAffectingLights,
  const AABB & aabbReceiver, float fObjDistance, uint nDLightMask, bool bIncludeNearFrustums)
{
	FUNCTION_PROFILER_3DENGINE;

  assert(GetCVars()->e_shadows);

  if(!pAffectingLights || !pAffectingLights->Count())
    return NULL;

	const int MAX_MASKED_GSM_LODS_NUM = 4;

	int nTerrainFrustumInUse = 0;
	if(GetCVars()->e_shadows_from_terrain_in_all_lods && GetCVars()->e_terrain && Get3DEngine()->m_bShowTerrainSurface)
  	if(nDLightMask & 1)
	    if(CDLight * pLight = pAffectingLights->GetAt(0))
	      if((pLight->m_Flags & DLF_SUN) && (pLight->m_Id>=0))
		      nTerrainFrustumInUse = 1;

  C3DEngine * pEng = Get3DEngine();

	// calculate frustums list id
	uint64 nCastersListId = 0;
	if(bIncludeNearFrustums)
	for(int i=0; i<16 && i<pAffectingLights->Count(); i++)
	{
		CDLight * pLight = pAffectingLights->GetAt(i);
		
    assert(pLight->m_Id>=0||1);

		if((pLight->m_Id>=0) && (nDLightMask & (1<<pLight->m_Id)) && (pLight->m_Flags & DLF_CASTSHADOW_MAPS))
		{
			if(CLightEntity::ShadowMapInfo * pSMI = ((CLightEntity *)pLight->m_pOwner)->m_pShadowMapInfo)
			{
				for(int nLod=0; nLod<GetCVars()->e_gsm_lods_num && nLod<MAX_MASKED_GSM_LODS_NUM && pSMI->pGSM[nLod]; nLod++)
				{
					ShadowMapFrustum * pFr = pSMI->pGSM[nLod];
					if(pFr->pCastersList && pFr->pCastersList->Count())
					{	// take GSM Lod's containing receiver bbox inside shadow frustum
						if(pFr->Intersect(aabbReceiver))
						{
							int nBitId = pLight->m_Id*MAX_MASKED_GSM_LODS_NUM+nLod;
							assert(nBitId/MAX_MASKED_GSM_LODS_NUM<pEng->GetRealLightsNum());
							nCastersListId |= (uint64(1)<<nBitId);
						}

						// todo: if(nCull == CULL_INCLUSION) break;
					}
				}
			}
		}
	}

	if(!nTerrainFrustumInUse)
		if(!nCastersListId)
			return NULL;

	PodArray<ShadowMapFrustum*> * pList = stl::find_in_map( pEng->m_FrustumsCache[nTerrainFrustumInUse], nCastersListId, NULL );

	// fill the list if not ready
	if(!pList || !pList->Count())
	{ 
		if(!pList)
		{
			pList = new PodArray<ShadowMapFrustum*>;
			pEng->m_FrustumsCache[nTerrainFrustumInUse][nCastersListId] = pList;
		}

		for(int nBitId=0; nBitId<64 && (uint64(1)<<nBitId) <= nCastersListId; nBitId++)
		{
			if(nCastersListId & (uint64(1)<<nBitId))
			{
				int nLightId = nBitId/MAX_MASKED_GSM_LODS_NUM;
				int nLod = nBitId - nLightId*MAX_MASKED_GSM_LODS_NUM;

				assert(nLightId < pEng->GetRealLightsNum());

				CDLight * pLight = (CDLight*)GetRenderer()->EF_Query(EFQ_LightSource, nLightId);

        assert(pLight->m_Id>=0);

				CLightEntity::ShadowMapInfo * pSMI = ((CLightEntity *)pLight->m_pOwner)->m_pShadowMapInfo;

				pList->Add(pSMI->pGSM[nLod]);
			}
		}

    //Scattering LOD
    //TOFIX:: Currently GSM LOD is used together with terrain shadow LOD only
    if(GetCVars()->e_gsm_lods_num < MAX_GSM_LODS_NUM)
    {
      CDLight * pLight = pAffectingLights->GetAt(0);
      CLightEntity::ShadowMapInfo * pSMI0 = ((CLightEntity *)pLight->m_pOwner)->m_pShadowMapInfo;
      int nScatterLOD = GetCVars()->e_gsm_lods_num; //LOD is just after the last valid GSM LOD
      if(pSMI0 && pSMI0->pGSM[nScatterLOD])
        pList->Add(pSMI0->pGSM[nScatterLOD]);
    }

		if(nTerrainFrustumInUse && GetCVars()->e_gsm_lods_num <= MAX_GSM_LODS_NUM)
		{
			CDLight * pLight = pAffectingLights->GetAt(0);
			CLightEntity::ShadowMapInfo * pSMI = ((CLightEntity *)pLight->m_pOwner)->m_pShadowMapInfo;
			if(pSMI && pSMI->pGSM[GetCVars()->e_gsm_lods_num-1])
			pList->Add(pSMI->pGSM[GetCVars()->e_gsm_lods_num-1]);
		}

		assert(pList->Count());
	}

	pEng->m_FrustumsCacheUsers[nTerrainFrustumInUse][nCastersListId]++;

	return pList;
}
