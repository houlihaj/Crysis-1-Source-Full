/*=============================================================================
	CommonRender.cpp: Crytek Common render helper functions and structures declarations.
	Copyright (c) 2001 Crytek Studios. All Rights Reserved.

	Revision history:
		* Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "Shadow_Renderer.h"
#include "MsStlSort.h"

TArray<SRendItem> SRendItem::m_RendItems[MAX_LIST_ORDER][EFSLIST_NUM];  

int SRendItem::m_RecurseLevel;
int SRendItem::m_StartRI[MAX_REND_RECURSION_LEVELS][MAX_LIST_ORDER][EFSLIST_NUM];
int SRendItem::m_EndRI[MAX_REND_RECURSION_LEVELS][MAX_LIST_ORDER][EFSLIST_NUM];
SRendLightGroup SRendItem::m_RenderLightGroups[MAX_REND_RECURSION_LEVELS][EFSLIST_NUM][MAX_LIST_ORDER][MAX_SORT_GROUPS][MAX_REND_LIGHT_GROUPS+1];
uint SRendItem::m_ShadowsValidMask[MAX_REND_RECURSION_LEVELS][MAX_REND_LIGHT_GROUPS];
SGroupProperty SRendItem::m_GroupProperty[MAX_REND_RECURSION_LEVELS][MAX_LIST_ORDER][EFSLIST_NUM];

inline bool CompareItemPreprocess( const SRendItem &a, const SRendItem &b )
{
  if (a.nBatchFlags != b.nBatchFlags)
    return a.nBatchFlags < b.nBatchFlags;

  if (a.SortVal != b.SortVal)
    return a.SortVal < b.SortVal;

  return false;
}

inline bool CompareRendItem( const SRendItem &a, const SRendItem &b )
{
  if (a.DynLMask != b.DynLMask)
    return a.DynLMask < b.DynLMask;

  if (a.SortVal != b.SortVal)
    return a.SortVal < b.SortVal;

  if (a.ObjSort != b.ObjSort)
    return a.ObjSort < b.ObjSort;

  if (a.Item != b.Item)
    return a.Item < b.Item;

  return false;
}

inline bool CompareItem_Decal( const SRendItem &a, const SRendItem &b )
{
	uint objSortA_Low(a.ObjSort & 0xFFFF);
	uint objSortA_High(a.ObjSort & ~0xFFFF);
	uint objSortB_Low(b.ObjSort & 0xFFFF);
	uint objSortB_High(b.ObjSort & ~0xFFFF);

	if (objSortA_Low != objSortB_Low)
		return objSortA_Low < objSortB_Low;

	if (a.DynLMask != b.DynLMask)
		return a.DynLMask < b.DynLMask;

	if (a.SortVal != b.SortVal)
		return a.SortVal < b.SortVal;

	if (objSortA_High != objSortB_High)
		return objSortA_High < objSortB_High;

	if (a.Item != b.Item)
		return a.Item < b.Item;

	return false;
}

inline bool CompareItem_Terrain( const SRendItem &a, const SRendItem &b )
{
  if (a.DynLMask != b.DynLMask)
    return a.DynLMask < b.DynLMask;

  CRendElement *pREa = a.Item;
  CRendElement *pREb = b.Item;

  if (pREa->m_CustomTexBind[0] != pREb->m_CustomTexBind[0])
    return pREa->m_CustomTexBind[0] < pREb->m_CustomTexBind[0];

  if (pREa->m_CustomTexBind[1] != pREb->m_CustomTexBind[1])
    return pREa->m_CustomTexBind[1] < pREb->m_CustomTexBind[1];

  if (a.ObjSort != b.ObjSort)
    return a.ObjSort < b.ObjSort;

  return false;
}

inline bool CompareItem_NoPtrCompare(const SRendItem &a, const SRendItem &b)
{
  if (a.DynLMask != b.DynLMask)
		return a.DynLMask < b.DynLMask;

	if (a.ObjSort != b.ObjSort)
		return a.ObjSort < b.ObjSort;

	float pSurfTypeA = ((float*)a.Item->m_CustomData)[8];
	float pSurfTypeB = ((float*)b.Item->m_CustomData)[8];
	if (pSurfTypeA != pSurfTypeB)
		return (pSurfTypeA < pSurfTypeB);

  pSurfTypeA = ((float*)a.Item->m_CustomData)[9];
  pSurfTypeB = ((float*)b.Item->m_CustomData)[9];
  if (pSurfTypeA != pSurfTypeB)
    return (pSurfTypeA < pSurfTypeB);

  pSurfTypeA = ((float*)a.Item->m_CustomData)[11];
  pSurfTypeB = ((float*)b.Item->m_CustomData)[11];
  if (pSurfTypeA != pSurfTypeB)
    return (pSurfTypeA < pSurfTypeB);

	return false;
}

inline bool CompareDist(const SRendItem &a, const SRendItem &b)
{
  return (a.fDist > b.fDist);
}

void SRendItem::mfSortPreprocess(SRendItem *First, int Num, int nList, int nAW)
{
	std::sort(First, First+Num, CompareItemPreprocess);
}

void SRendItem::mfSortByDist(SRendItem *First, int Num, int nList, int nAW, bool bDecals)
{
  CRenderer *r = gRenDev;
  int i, j;
  if (!bDecals)
  {
    for (i=0; i<Num; i++)
    {
      SRendItem *pRI = &First[i];
      CRenderObject *pObj = pRI->pObj;
		  if (!pObj)
		  {
			  CryLogAlways("SRendItem::mfSortByDist: pObject is NULL");
			  continue;
		  }
      float fAddDist = pObj->m_fSort;
      pRI->fDist = pObj->m_fDistance + fAddDist;
    }
	  std::sort(First, First+Num, CompareDist);
  }
  else
  {
    std::sort(First, First + Num, CompareItem_Decal);
  }

  int nR = SRendItem::m_RecurseLevel;

  int nNumLightsCastShadow = r->m_RP.m_DLights[nR].Num();
  for (uint n=0; n<r->m_RP.m_DLights[nR].Num(); n++)
  {
    CDLight *pLight = r->m_RP.m_DLights[nR][n];
    if (!(pLight->m_Flags & DLF_CASTSHADOW_MAPS))
    {
      nNumLightsCastShadow = n;
      break;
    }
  }

  // Create light groups
  bool bPrevProcessed = false;
  uint nSort = 0; 
  uint32 nBatchMask = 0;
  SRendLightGroup *pGR = &m_RenderLightGroups[nR][nList][nAW][nSort][0];
  for (j=0; j<=MAX_REND_LIGHT_GROUPS; j++)
  {
    pGR[j].Reset();
  }
  for (i=0; i<Num; i++)
  {
    SRendItem *ri = &First[i];
    nBatchMask |= ri->nBatchFlags;

    bool bProcessed = false;
    if (ri->DynLMask)
    {
      CRenderObject *pObj = ri->pObj;
#ifdef _DEBUG
      CTexture *pTex = CTexture::GetByID(pObj->m_nTextureID);
      CShader *pSh = mfGetShader(ri->SortVal);
      int nnn = 0;
#endif
			if (pObj->m_ObjFlags & FOB_INSHADOW)
      {
        int nFirstLightInGroupID = 0;
        int nPass = 0;
        if (!bPrevProcessed && nSort < MAX_SORT_GROUPS-1)
        {
          nSort++;
          pGR = &m_RenderLightGroups[nR][nList][nAW][nSort][0];
          for (j=0; j<=MAX_REND_LIGHT_GROUPS; j++)
          {
            pGR[j].Reset();
          }
          bPrevProcessed = true;
        }
        for (j=0; j<MAX_REND_LIGHT_GROUPS; j++, nFirstLightInGroupID+=4)
        {
          uint nCurMaskGroup = 0xf<<nFirstLightInGroupID;
          if (ri->DynLMask & nCurMaskGroup)
          {
            pGR[j].m_GroupLightMask |= (ri->DynLMask & nCurMaskGroup);
            pGR[j].RendItemsLights.push_back(nPass ? i|0x80000000 : i);
            nPass++;
					  if (pObj->m_pShadowCasters)
            {
						  bool bAdded[4];
						  *(uint *)&bAdded[0] = 0;
              PodArray<ShadowMapFrustum*> *lsources = pObj->m_pShadowCasters;
              for (int n=0; n<lsources->Count(); n++)
              {
                ShadowMapFrustum* pFr = lsources->GetAt(n);
                if (pFr->nDLightId >= nFirstLightInGroupID && pFr->nDLightId <= nFirstLightInGroupID+3)
                {
                  int nId = pFr->nDLightId-nFirstLightInGroupID;
                  if (!bAdded[nId])
                  {
                    bAdded[nId] = true;
                    pGR[j].RendItemsShadows[nId].push_back(i);
                  }
                }
              }
            }
          }
          if (ri->DynLMask < (uint)(1<<(nFirstLightInGroupID+4)))
            break;
        }
        ri->ObjSort = nPass << 8;
        bProcessed = true;
      }
    }
    if (!bProcessed)
    {
      ri->ObjSort = 1 << 8;
      pGR[MAX_REND_LIGHT_GROUPS].RendItemsLights.push_back(i);
      pGR[MAX_REND_LIGHT_GROUPS].m_GroupLightMask |= ri->DynLMask;
      bPrevProcessed = false;
    }
    if (i && bProcessed != bPrevProcessed && nSort < MAX_SORT_GROUPS-1)
    {
      nSort++;
      pGR = &m_RenderLightGroups[nR][nList][nAW][nSort][0];
      for (j=0; j<=MAX_REND_LIGHT_GROUPS; j++)
      {
        pGR[j].Reset();
      }
      bPrevProcessed = bProcessed;
    }
  }
  m_GroupProperty[nR][nAW][nList].m_nSortGroups = nSort+1;
  m_GroupProperty[nR][nAW][nList].m_nBatchMask = nBatchMask;
}

void SRendItem::mfSortByLight(SRendItem *First, int Num, int nList, bool bSort, const bool bDirectLightsOnly, const bool bIgnoreRePtr, bool bSortDecals, int nAW)
{
  int i;
  uint j;

  if (bSort)
	{
    if (bIgnoreRePtr)
			std::sort(First, First + Num, CompareItem_NoPtrCompare);
		else
		{
			if (bSortDecals)
				std::sort(First, First + Num, CompareItem_Decal);
			else
				std::sort(First, First + Num, CompareRendItem);
		}
	}

  CRenderer *r = gRenDev;
  int nR = SRendItem::m_RecurseLevel;

  bool bSunUsed = false;

  int nNumLightsCastShadow = r->m_RP.m_DLights[nR].Num();
  for (uint n=0; n<r->m_RP.m_DLights[nR].Num(); n++)
  {
    CDLight *pLight = r->m_RP.m_DLights[nR][n];

    if (pLight->m_Flags & DLF_DIRECTIONAL)
      bSunUsed = true;

    if (!(pLight->m_Flags & DLF_CASTSHADOW_MAPS))
    {
      nNumLightsCastShadow = n;
      break;
    }
  }

  SRendLightGroup *pGR = &m_RenderLightGroups[nR][nList][nAW][0][0];
  for (j=0; j<MAX_REND_LIGHT_GROUPS+1; j++)
  {
    pGR[j].Reset();
  }
  uint32 nBatchFlags = 0;
  for (i=0; i<Num; i++)
  {
    SRendItem *ri = &First[i];
    nBatchFlags |= ri->nBatchFlags;
		uint LightMask = ri->DynLMask;
    bool bProcessed = false;
    if (LightMask)
    {
      CRenderObject *pObj = ri->pObj;
#ifdef _DEBUG
      CShader *pSh = mfGetShader(ri->SortVal);
      for (j=0; j<r->m_RP.m_DLights[nR].Num(); j++)
      {
        if (pObj->m_DynLMMask & (1<<j))
        {
          CDLight *pDL = r->m_RP.m_DLights[nR][j];
          int nnn = 0;
        }
      }
#endif
      if (pObj->m_ObjFlags & FOB_INSHADOW)
      {
        int nFirstLightInGroupID = 0;
        bool bSecPass = /*bSecondPass*/ false;
        for (j=0; j<MAX_REND_LIGHT_GROUPS; j++, nFirstLightInGroupID+=4)
        {
          uint nCurMaskGroup = 0xf<<nFirstLightInGroupID;
          uint nCurLMask = LightMask & nCurMaskGroup;
          if (nCurLMask)
          {
            pGR[j].m_GroupLightMask |= (LightMask & nCurMaskGroup);
            pGR[j].RendItemsLights.push_back(bSecPass ? i|0x80000000 : i);
            bSecPass = true;

            //Test for sun light group
            bool bSunOnlyGroup = (nCurLMask <= 1 && bSunUsed);

            // Optimisation: don't build shadow pass lists in deferred shadow mode
            //if ( !CRenderer::CV_r_ShadowPassFS || !bSunOnlyGroup  )
            {
              if (pObj->m_pShadowCasters) 
              {
							  bool bAdded[4];
							  *(uint *)&bAdded[0] = 0;
                PodArray<ShadowMapFrustum*> *lsources = pObj->m_pShadowCasters;
                for (int n=0; n<lsources->Count(); n++)
                {
                  ShadowMapFrustum* pFr = lsources->GetAt(n);
                  int nId = pFr->nDLightId-nFirstLightInGroupID;
                  if (nId>=0 && nId<4 && !bAdded[nId])
                  {
                    bAdded[nId] = true;
                    pGR[j].RendItemsShadows[nId].push_back(i);
                  }
                }
              }
            }
          }
          if (LightMask < (uint)(1<<(nFirstLightInGroupID+4)))
            break;
        }
        bProcessed = true;
      }
    }
    if (!bProcessed)
    {
      pGR[MAX_REND_LIGHT_GROUPS].RendItemsLights.push_back(/*bSecondPass ? i|0x80000000 :*/ i);
      pGR[MAX_REND_LIGHT_GROUPS].m_GroupLightMask |= LightMask;
    }
  }
  m_GroupProperty[nR][nAW][nList].m_nSortGroups = 1;
  m_GroupProperty[nR][nAW][nList].m_nBatchMask = nBatchFlags;
}

//=================================================================

