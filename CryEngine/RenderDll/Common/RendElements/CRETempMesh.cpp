#include "StdAfx.h"
#include "RendElement.h"

void CRETempMesh::mfPrepare()
{
  gRenDev->EF_CheckOverflow(0, 0, this);

  gRenDev->m_RP.m_pRE = this;
  gRenDev->m_RP.m_RendNumIndices = 6;
  gRenDev->m_RP.m_RendNumVerts = 4;
  gRenDev->m_RP.m_FirstVertex = 0;
  gRenDev->m_RP.m_FirstIndex = 0;
}

void *CRETempMesh::mfGetPointer(ESrcPointer ePT, int *Stride, EParamType Type, ESrcPointer Dst, int Flags)
{
  *Stride = sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F);
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pVertices = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)m_pVBuffer->m_VS[VSF_GENERAL].m_VData;
  SBufInfoTable *pOffs = &gBufInfoTable[m_pVBuffer->m_nVertexFormat];

  switch(ePT) 
  {
    case eSrcPointer_Vert:
      return &pVertices->xyz.x;
    case eSrcPointer_Tex:
      return &pVertices->st[0];
    case eSrcPointer_Color:
      return &pVertices->color.dcolor;
  }
  return NULL;
}

