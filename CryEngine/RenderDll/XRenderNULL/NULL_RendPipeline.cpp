/*=============================================================================
  PS2_RendPipeline.cpp : PS2 specific rendering using shaders pipeline.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

    Revision history:
  		* Created by Honich Andrey
    
=============================================================================*/

#include "StdAfx.h"
#include "NULL_Renderer.h"

//============================================================================================
// Init Shaders rendering

void CNULLRenderer::EF_Init()
{
  bool nv = 0;

  m_RP.m_MaxVerts = 600;
  m_RP.m_MaxTris = 300;

//==================================================

  SAFE_DELETE_ARRAY(m_RP.m_VisObjects);

  CRenderObject::m_Waves.Create(32);
  m_RP.m_VisObjects = new CRenderObject *[MAX_REND_OBJECTS];

  if (!m_RP.m_TempObjects.Num())
    m_RP.m_TempObjects.Reserve(MAX_REND_OBJECTS);
  if (!m_RP.m_Objects.Num())
  {
    m_RP.m_Objects.Reserve(MAX_REND_OBJECTS);
    m_RP.m_Objects.SetUse(1);
    SAFE_DELETE_ARRAY(m_RP.m_ObjectsPool);
    m_RP.m_nNumObjectsInPool = 384;
    m_RP.m_ObjectsPool = new CRenderObject[m_RP.m_nNumObjectsInPool];
    for (int i=0; i<m_RP.m_nNumObjectsInPool; i++)
    {
      m_RP.m_TempObjects[i] = &m_RP.m_ObjectsPool[i];
      m_RP.m_TempObjects[i]->Init();
      m_RP.m_TempObjects[i]->m_II.m_AmbColor = Col_White;
      m_RP.m_TempObjects[i]->m_ObjFlags = 0;
      m_RP.m_TempObjects[i]->m_II.m_Matrix.SetIdentity();
      m_RP.m_TempObjects[i]->m_RState = 0;
    }
    m_RP.m_VisObjects[0] = &m_RP.m_ObjectsPool[0];
  }
  EF_InitVFMergeMap();
}

// Init the vertex format merge map.
void CNULLRenderer::EF_InitVFMergeMap()
{
  for (int i=0; i<VERTEX_FORMAT_NUMS; i++)
  {
    for (int j=0; j<VERTEX_FORMAT_NUMS; j++)
    {
      SVertBufComps Cps[2];
      GetVertBufComps(&Cps[0], i);
      GetVertBufComps(&Cps[1], j);

      bool bNeedTC = Cps[1].m_bHasTC | Cps[0].m_bHasTC;
      bool bNeedCol = Cps[1].m_bHasColors | Cps[0].m_bHasColors;
      if (i == VERTEX_FORMAT_P3F_N4B_COL4UB || j == VERTEX_FORMAT_P3F_N4B_COL4UB)
        m_RP.m_VFormatsMerge[i][j] = VERTEX_FORMAT_P3F_N4B_COL4UB;
      else
        if (i == VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F || j == VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F)
          m_RP.m_VFormatsMerge[i][j] = VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F;
        else
          m_RP.m_VFormatsMerge[i][j] = VertFormatForComponents(bNeedCol, bNeedTC, false);
    }
  }
}

void CNULLRenderer::EF_SetClipPlane (bool bEnable, float *pPlane, bool bRefract)
{
}

void CNULLRenderer::EF_PipelineShutdown()
{
  int i, j;

  CRenderObject::m_Waves.Free();
  SAFE_DELETE_ARRAY(m_RP.m_VisObjects);

  for (i=0; i<CREClientPoly2D::m_PolysStorage.Num(); i++)
  {
    SAFE_RELEASE(CREClientPoly2D::m_PolysStorage[i]);
  }
  CREClientPoly2D::m_PolysStorage.Free();

  for (j=0; j<4; j++)
  {
    for (i=0; i<CREClientPoly::m_PolysStorage[j].Num(); i++)
    {
      SAFE_RELEASE(CREClientPoly::m_PolysStorage[j][i]);
    }
    CREClientPoly::m_PolysStorage[j].Free();
  }
}

void CNULLRenderer::EF_Release(int nFlags)
{
}

//==========================================================================

void CNULLRenderer::EF_SetState(int st, int AlphaRef)
{
  m_RP.m_CurState = st;
  m_RP.m_CurAlphaRef = AlphaRef;
}
void CRenderer::EF_SetStencilState(int st, uint nStencRef, uint nStencMask, uint nStencWriteMask)
{
}

//=================================================================================

DEFINE_ALIGNED_DATA( Matrix44, sIdentityMatrix( 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 ), 16 ); 

// Initialize of the new shader pipeline (only 2d)
void CRenderer::EF_Start(CShader *ef, int nTech, SRenderShaderResources *Res, CRendElement *re)
{
  m_RP.m_Frame++;
}

void CRenderer::EF_CheckOverflow(int nVerts, int nInds, CRendElement *re, int* nNewVerts, int* nNewInds)
{
}

//========================================================================================

void CNULLRenderer::EF_EndEf3D(int nFlags)
{
  m_RP.m_RealTime = iTimer->GetCurrTime();
  EF_RemovePolysFromScene();
  SRendItem::m_RecurseLevel--;
}

//double timeFtoI, timeFtoL, timeQRound;
//int sSome;
void CNULLRenderer::EF_EndEf2D(bool bSort)
{
}
