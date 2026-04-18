/*=============================================================================
	CREClientPoly.cpp : implementation of 3D Client polygons RE.
	Copyright (c) 2001-2005 Crytek Studios. All Rights Reserved.

	Revision history:
		* Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"


//===============================================================


TArray<CREClientPoly *> CREClientPoly::m_PolysStorage[4];

CRendElement *CREClientPoly::mfCopyConstruct(void)
{
  CREClientPoly *cp = new CREClientPoly;
  *cp = *this;
  return cp;
}

void CREClientPoly::mfPrepare(void)
{
  CRenderer *rd = gRenDev;
  CShader *ef = rd->m_RP.m_pShader;
  int i, n;

  rd->EF_StartMerging();
  CREClientPoly::mRS.NumRendPolys++;

  int savev = rd->m_RP.m_RendNumVerts;
  int savei = rd->m_RP.m_RendNumIndices;

  int nVerts, nInds;
  rd->EF_CheckOverflow(m_sNumVerts, m_sNumIndices, this, &nVerts, &nInds);
  
  ushort *pSrcInds = &rd->m_RP.m_SysIndexPool[m_nOffsInd];
  n = rd->m_RP.m_RendNumVerts;
  ushort *dinds = &rd->m_RP.m_RendIndices[gRenDev->m_RP.m_RendNumIndices];
  for (i=0; i<nInds; i++, dinds++, pSrcInds++)
  {
    *dinds = *pSrcInds+n;
  }
  rd->m_RP.m_RendNumIndices += i;

  UPipeVertex ptr = rd->m_RP.m_NextPtr;
  byte *OffsT, *OffsD;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pSrc = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)&rd->m_RP.m_SysVertexPool[m_nOffsVert];
  switch(rd->m_RP.m_CurVFormat)
  {
    case VERTEX_FORMAT_P3F_COL4UB_TEX2F:
      OffsT = rd->m_RP.m_OffsT + ptr.PtrB;
      OffsD = rd->m_RP.m_OffsD + ptr.PtrB;
      for (i=0; i<nVerts; i++, ptr.PtrB+=rd->m_RP.m_Stride, OffsT+=rd->m_RP.m_Stride, OffsD+=rd->m_RP.m_Stride)
      {
        *(float *)(ptr.PtrB+0) = pSrc[i].xyz[0];
        *(float *)(ptr.PtrB+4) = pSrc[i].xyz[1];
        *(float *)(ptr.PtrB+8) = pSrc[i].xyz[2];
        *(float *)(OffsT) = pSrc[i].st[0];
        *(float *)(OffsT+4) = pSrc[i].st[1];
        *(uint *)OffsD = pSrc[i].color.dcolor;
      }
    	break;
    case VERTEX_FORMAT_P3F_TEX2F:
      OffsT = rd->m_RP.m_OffsT + ptr.PtrB;
      for (i=0; i<nVerts; i++, ptr.PtrB+=rd->m_RP.m_Stride, OffsT+=rd->m_RP.m_Stride)
      {
        *(float *)(ptr.PtrB+0) = pSrc[i].xyz[0];
        *(float *)(ptr.PtrB+4) = pSrc[i].xyz[1];
        *(float *)(ptr.PtrB+8) = pSrc[i].xyz[2];
        *(float *)(OffsT) = pSrc[i].st[0];
        *(float *)(OffsT+4) = pSrc[i].st[1];
      }
      break;
    case VERTEX_FORMAT_P3F_COL4UB:
      OffsD = rd->m_RP.m_OffsD + ptr.PtrB;
      for (i=0; i<nVerts; i++, ptr.PtrB+=rd->m_RP.m_Stride, OffsD+=rd->m_RP.m_Stride)
      {
        *(float *)(ptr.PtrB+0) = pSrc[i].xyz[0];
        *(float *)(ptr.PtrB+4) = pSrc[i].xyz[1];
        *(float *)(ptr.PtrB+8) = pSrc[i].xyz[2];
        *(uint *)OffsD = pSrc[i].color.dcolor;
      }
      break;
    default:
      assert(false);
  }
  rd->m_RP.m_NextPtr = ptr;

  if (m_nOffsTang >= 0)
  {
    UPipeVertex ptrTang = rd->m_RP.m_NextPtrTang;
    SPipTangents *pSrc = (SPipTangents *)&rd->m_RP.m_SysVertexPool[m_nOffsTang];
    for (i=0; i<nVerts; i++, ptrTang.PtrB+=sizeof(SPipTangents))
    {
      *(SPipTangents *)(ptrTang.PtrB) = pSrc[i];
    }
    rd->m_RP.m_NextPtrTang = ptrTang;
  }

  rd->m_RP.m_RendNumVerts += nVerts;

  CREClientPoly::mRS.NumVerts += rd->m_RP.m_RendNumVerts - savev; 
  CREClientPoly::mRS.NumIndices += rd->m_RP.m_RendNumIndices - savei; 
}


//=======================================================================

SClientPolyStat CREClientPoly::mRS;

void CREClientPoly::mfPrintStat()
{
/*  char str[1024];

  *gpCurPrX = 4;
  sprintf(str, "Num Indices: %i\n", mRS.NumIndices);
  gRenDev->mfPrintString (str, PS_TRANSPARENT | PS_UP);

  *gpCurPrX = 4;
  sprintf(str, "Num Verts: %i\n", mRS.NumVerts);
  gRenDev->mfPrintString (str, PS_TRANSPARENT | PS_UP);

  *gpCurPrX = 4;
  sprintf(str, "Num Render Client Polys: %i\n", mRS.NumRendPolys);
  gRenDev->mfPrintString (str, PS_TRANSPARENT | PS_UP);

  *gpCurPrX = 4;
  sprintf(str, "Num Occluded Client Polys: %i\n", mRS.NumOccPolys);
  gRenDev->mfPrintString (str, PS_TRANSPARENT | PS_UP);

  *gpCurPrX = 4;
  gRenDev->mfPrintString ("\nClient Polygons status info:\n", PS_TRANSPARENT | PS_UP);*/
}
