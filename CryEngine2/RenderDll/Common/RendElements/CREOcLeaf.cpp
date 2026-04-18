/*=============================================================================
  CREMesh.cpp : implementation of OcLeaf Render Element.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"
#include <ICryAnimation.h>

bool CREMesh::mfIsHWSkinned()
{
  if (!m_pChunk)
    return false;
	uint32 numBonesPerSubset = m_pChunk->m_arrChunkBoneIDs.size();
  if (numBonesPerSubset)
    return true;
  return false;
}

void CREMesh::mfReset()
{
  if (!m_pRenderMesh)
    return;
  m_pRenderMesh->InvalidateVideoBuffer(-1);
}

void CREMesh::mfCenter(Vec3& Pos, CRenderObject*pObj)
{
  if (m_Flags & FCEF_CALCCENTER)
  {
    Pos = m_Center;
    if (pObj)
      Pos += pObj->GetTranslation();
    return;
  }
  m_Flags |= FCEF_CALCCENTER;
  int i;
  CRenderer *rd = gRenDev;
  CRenderMesh *pRM = m_pRenderMesh;

  Pos = Vec3(0,0,0);
  byte *pD = pRM->m_pSysVertBuffer ? (byte *)pRM->m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData : 0;
  if (!pD)
  {
    Vec3 Mins = m_pRenderMesh->m_vBoxMin;
    Vec3 Maxs = m_pRenderMesh->m_vBoxMax;
    Pos = (Mins + Maxs) * 0.5f;
    m_Center = Pos;
    if (pObj)
      Pos += pObj->GetTranslation();
    return;
  }
  int Stride = m_VertexSize[pRM->m_nVertexFormat];
  pD += m_pChunk->nFirstVertId * Stride;
  for (i=0; i<m_pChunk->nNumVerts; i++, pD+=Stride)
  {
    Vec3 *p = (Vec3 *)pD;
    Pos += *p;
  }
  float f = 1.0f / (float)m_pChunk->nNumVerts;
  Pos *= f;
  m_Center = Pos;
  if (pObj)
    Pos += pObj->GetTranslation();
}

void CREMesh::mfGetBBox(Vec3& vMins, Vec3& vMaxs)
{
  vMins = m_pRenderMesh->GetVertexContainerInternal()->m_vBoxMin;
  vMaxs = m_pRenderMesh->GetVertexContainerInternal()->m_vBoxMax;
}

float CREMesh::mfDistanceToCameraSquared(Matrix34& matInst)
{
  CRenderer *rd = gRenDev;

  Vec3 Mins = m_pRenderMesh->m_vBoxMin;
  Vec3 Maxs = m_pRenderMesh->m_vBoxMax;
  //Vec3 Center = (Mins + Maxs) * 0.5f;
  //Center += thisObject.GetTranslation();
  //Vec3 delta = rd->GetRCamera().Orig - Center;
  AABB aabb(Mins, Maxs);
  float fDistSq = Distance::Point_AABBSq(rd->GetRCamera().Orig-matInst.GetTranslation(), aabb);
  return fDistSq;

/*	float dist = delta.GetLength();
	dist += 60.f;//TODO: remove me, i am a hack for cxp to let the particle effect be in front of the sphere surrounding it
	return sqr(dist);
*/
  //return (delta).GetLengthSquared();
}


bool CREMesh::mfCullByClipPlane(CRenderObject *pObj)
{
  CRenderer *rd = gRenDev;

  if (rd->m_RP.m_ClipPlaneEnabled && CRenderer::CV_r_cullbyclipplanes)
  {
    Vec3 Mins = m_pRenderMesh->m_vBoxMin;
    Vec3 Maxs = m_pRenderMesh->m_vBoxMax;
    if (!(pObj->m_ObjFlags & FOB_TRANS_ROTATE))
    {
      // Non rotated bounding-box. Fast solution
      if (!(pObj->m_ObjFlags & FOB_TRANS_SCALE))
      {
        Mins += pObj->GetTranslation();
        Maxs += pObj->GetTranslation();
      }
      else
      {
        Matrix34& m = pObj->GetMatrix();
        Mins = m.TransformPoint(Mins);
        Maxs = m.TransformPoint(Maxs);
      }
    }
    else
    { 
      // General slow solution (separatelly transform all 8 points of
      // non transformed cube and calculate axis aligned bounding box)
      Vec3 v[8];
      int i;
      Matrix34& m = pObj->GetMatrix();
      for (i=0; i<8; i++)
      {
        if (i & 1)
          v[i].x = Mins.x;
        else
          v[i].x = Maxs.x;

        if (i & 2)
          v[i].y = Mins.y;
        else
          v[i].y = Maxs.y;

        if (i & 4)
          v[i].z = Mins.z;
        else
          v[i].z = Maxs.z;
        v[i] = m.TransformPoint(v[i]);
      }
      Mins=SetMaxBB();
      Maxs=SetMinBB();
      for (i=0; i<8; i++)
      {
        AddToBounds(v[i], Mins, Maxs);
      }
    }
    //if (CullBoxByPlane(&Mins.x, &Maxs.x, &rd->m_RP.m_CurClipPlaneCull) == 2)
    return false;
      return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////

void CREMesh::mfPrepare()
{
  CRenderer *rd = gRenDev;

  {
    //PROFILE_FRAME_TOTAL(Mesh_REPrepare_Ocleaf);

    if (rd->m_RP.m_ClipPlaneEnabled && CRenderer::CV_r_cullbyclipplanes)
    {
      if (mfCullByClipPlane(rd->m_RP.m_pCurObject))
      {
        rd->EF_CheckOverflow(0, 0, this);
        // Completly culled by plane (nothing to do with it)
        rd->m_RP.m_pRE = NULL;
        rd->m_RP.m_RendNumIndices = 0;
        rd->m_RP.m_RendNumVerts = 0;
        return;
      }
    }

    CRenderMesh *pRM = m_pRenderMesh;
    CRenderChunk *pChunk = m_pChunk;

    rd->EF_CheckOverflow(0, 0, this);

    rd->m_RP.m_pRE = this;

    if (pRM->m_nPrimetiveType == R_PRIMV_TRIANGLE_STRIP)
    {
      rd->m_RP.m_FirstVertex = 0;
      rd->m_RP.m_FirstIndex = 0;
      rd->m_RP.m_RendNumIndices = pChunk->nNumIndices;
      rd->m_RP.m_RendNumVerts = pChunk->nNumVerts;
    }
    else
    {
      if (pRM->m_bShort)
      {
        if (!(rd->m_RP.m_ObjFlags & (FOB_BENDED | FOB_CHARACTER)) && !rd->m_RP.m_pCurObject->m_pCharInstance)
          rd->m_RP.m_FlagsPerFlush |= RBSI_SHORTPOS;
        rd->m_vMeshScale.x = pRM->m_vScale.x;
        rd->m_vMeshScale.y = pRM->m_vScale.y;
        rd->m_vMeshScale.z = pRM->m_vScale.z;
      }

      rd->m_RP.m_FirstVertex = pChunk->nFirstVertId;
      rd->m_RP.m_FirstIndex = pChunk->nFirstIndexId;
      rd->m_RP.m_RendNumIndices = pChunk->nNumIndices;
      rd->m_RP.m_RendNumVerts = pChunk->nNumVerts;
    }      
  }
}

PodArray<CRenderChunk> *CREMesh::mfGetMatInfoList()
{
  return m_pRenderMesh->m_pChunks;
}

int CREMesh::mfGetMatId()
{
  return m_pChunk->m_nMatID;
}

CRenderChunk *CREMesh::mfGetMatInfo()
{
  return m_pChunk;
}

float CREMesh::mfMinDistanceToCamera(Matrix34& matInst)
{
  Vec3 pos = gRenDev->GetCamera().GetPosition();
  CRenderChunk *pChunk = m_pChunk;
  float fDist = 0;
  if (!pChunk->m_fRadius)
  {
    Vec3 vMins, vMaxs, vCenterRE;
    mfGetBBox(vMins, vMaxs);
    vCenterRE = (vMins + vMaxs) * 0.5f;
    vCenterRE += matInst.GetTranslation();
    float fRadRE = (vMaxs - vMins).GetLength() * 0.5f;
    float fScale = matInst(0,0)*matInst(0,0) + matInst(0,1)*matInst(0,1) + matInst(0,2)*matInst(0,2);
    fScale = isqrt_fast_tpl(fScale);
    fDist = (pos - vCenterRE).GetLength();
    fDist = fDist - fRadRE / fScale;
  }
  else
  {
    Vec3 vMid = pChunk->m_vCenter;

    vMid += matInst.GetTranslation();
    float fScale = matInst(0,0)*matInst(0,0) + matInst(0,1)*matInst(0,1) + matInst(0,2)*matInst(0,2);
    fScale = isqrt_fast_tpl(fScale);
    fDist = (pos - vMid).GetLength();
    fDist -= pChunk->m_fRadius / fScale * 2.0f;
  }
  if (fDist < 0.25f)
    fDist = 0.25f;

  return fDist;
}

void CREMesh::mfPrecache(const SShaderItem& SH)
{
  CShader *pSH = (CShader *)SH.m_pShader;
  if (!pSH)
    return;
  if (!m_pRenderMesh)
    return;
  if (m_pRenderMesh->m_pVertexBuffer)
    return;
  if (!(m_pRenderMesh->m_UpdateVBufferMask & (1 | 2 | FMINV_INDICES)))
    return;

  mfCheckUpdate(pSH->m_VertexFormatId, 2);
}

bool CREMesh::mfCheckUpdate(int nVertFormat, int Flags)
{
  //PROFILE_FRAME(Mesh_CheckUpdate);

  m_pRenderMesh->Unlink();
  m_pRenderMesh->Link(&CRenderMesh::m_Root);
  CRenderMesh *pRM = m_pRenderMesh->GetVertexContainerInternal();

  if (gRenDev->m_RP.m_pShader && (gRenDev->m_RP.m_pShader->m_Flags2 & EF2_DEFAULTVERTEXFORMAT))
    nVertFormat = pRM->m_nVertexFormat;
  bool bWasReleased = m_pRenderMesh->CheckUpdate(nVertFormat, Flags, m_pChunk->nNumVerts, m_pChunk->nNumIndices, m_pChunk->nFirstVertId, m_pChunk->nFirstIndexId);

  if (!pRM->m_pVertexBuffer)
    return false;

#if !defined(PS3) || defined(PS3_ACTIVATE_CONSTANT_ARRAYS)//we cannot map it yet, will be removed soon
  CRenderMesh *pWeights = (CRenderMesh *)gRenDev->m_RP.m_pCurObject->GetWeights();
  if (pWeights && m_pRenderMesh->m_pMorphBuddy)
  {
    pWeights->CheckUpdate(pWeights->GetVertexFormat(), 0, m_pChunk->nNumVerts, m_pChunk->nNumIndices, m_pChunk->nFirstVertId, m_pChunk->nFirstIndexId);
    m_pRenderMesh->m_pMorphBuddy->CheckUpdate(nVertFormat, Flags, m_pChunk->nNumVerts, m_pChunk->nNumIndices, m_pChunk->nFirstVertId, m_pChunk->nFirstIndexId);
  }
#endif //PS3_ACTIVATE_CONSTANT_ARRAYS

  gRenDev->m_RP.m_CurVFormat = pRM->m_nVertexFormat;

  if((Flags & VSM_LMTC))
  {
    CRenderObject *pObj = gRenDev->m_RP.m_pCurObject;
    int nO = 0;
    while (pObj)
    {
      SRenderObjData *pD = pObj->GetObjData(false);
      if (pD && pD->m_pLMTCBufferO)
      {
        pRM = (CRenderMesh*)pD->m_pLMTCBufferO;
        pRM->Unlink();
        pRM->Link(&CRenderMesh::m_Root);
        if (!pRM->m_pVertexBuffer)
          pRM->UpdateVidVertices(NULL, 0, VSF_GENERAL, true);
        if (!pRM->m_pVertexBuffer)
          return false;
      }
      nO++;
      if (nO >= gRenDev->m_RP.m_ObjectInstances.Num())
        break;
      pObj = gRenDev->m_RP.m_ObjectInstances[nO];
    }
  }

	if((Flags & VSM_SCATTER))
	{
		CRenderObject *pObj = gRenDev->m_RP.m_pCurObject;
		int nO = 0;
		while (pObj)
		{
			SRenderObjData *pD = pObj->GetObjData(false);
			if (pD && pD->m_pScatterBuffer)
			{
				pRM = (CRenderMesh*)pD->m_pScatterBuffer;
				pRM->Unlink();
				pRM->Link(&CRenderMesh::m_Root);
				if (!pRM->m_pVertexBuffer)
					pRM->UpdateVidVertices(NULL, 0, VSF_GENERAL, true);
				if (!pRM->m_pVertexBuffer)
					return false;
			}
			nO++;
			if (nO >= gRenDev->m_RP.m_ObjectInstances.Num())
				break;
			pObj = gRenDev->m_RP.m_ObjectInstances[nO];
		}
	}

  return true;
}

static _inline byte *sGetBuf(CRenderMesh *pRM, int *Stride, int Stream, int Flags)
{
  byte *pD;
  if (Flags & FGP_WAIT)
    gRenDev->UpdateBuffer(pRM->m_pVertexBuffer, NULL, 0, false, 0, Stream);
  if (!(Flags & FGP_SRC))
  {
    pD = (byte*)pRM->m_pVertexBuffer->m_VS[Stream].m_VData;
  }
  else
  {
    if (!pRM->m_pSysVertBuffer)
      pD = NULL;
    else
      pD = (byte *)pRM->m_pSysVertBuffer->m_VS[Stream].m_VData;
    if (!pD && pRM->m_pVertexBuffer && !pRM->m_pVertexBuffer->m_VS[Stream].m_bDynamic)
      pD = (byte*)pRM->m_pVertexBuffer->m_VS[Stream].m_VData;
  }
  if (!pD)
    return pD;
  if (Stream == VSF_GENERAL)
    *Stride = m_VertexSize[pRM->m_nVertexFormat];
  else
  if (Stream == VSF_TANGENTS)
    *Stride = sizeof(SPipTangents);
  else
    assert(0);
  int Offs = gRenDev->m_RP.m_FirstVertex * (*Stride);
  if (Flags & FGP_REAL)
    pD = &pD[Offs];

  return pD;
}

void *CREMesh::mfGetPointer(ESrcPointer ePT, int *Stride, EParamType Type, ESrcPointer Dst, int Flags)
{
  SBufInfoTable *pOffs;
  CRenderMesh *pRM = m_pRenderMesh->GetVertexContainerInternal();
  byte *pD;

  switch(ePT) 
  {
    case eSrcPointer_Vert:
      pD = sGetBuf(pRM, Stride, VSF_GENERAL, Flags);
      return pD;
    case eSrcPointer_Tex:
      pD = sGetBuf(pRM, Stride, VSF_GENERAL, Flags);
      pOffs = &gBufInfoTable[pRM->m_nVertexFormat];
      if (pOffs->OffsTC)
        return &pD[pOffs->OffsTC];
      else
      {
        if (!(Flags & FGP_SRC))
        {
          Warning("Error: Missed texcoord pointer for shader '%s'", gRenDev->m_RP.m_pShader->GetName());
          return NULL;
        }
        *Stride = sizeof(Vec2);
        if (Flags & FGP_REAL)
          return &pRM->m_TempTexCoords[gRenDev->m_RP.m_FirstVertex];
        else
          return pRM->m_TempTexCoords;
      }
      break;
    case eSrcPointer_TexLM:     
      {
        SRenderObjData *pDT;
        if (gRenDev->m_RP.m_pCurObject && (pDT=gRenDev->m_RP.m_pCurObject->GetObjData(false)))
        {
          if (pDT->m_pLMTCBufferO)
          { // separate stream for lightmaps
            CVertexBuffer * pVideoBuffer = ((CRenderMesh*)pDT->m_pLMTCBufferO)->m_pVertexBuffer;
            assert(pVideoBuffer->m_nVertexFormat==VERTEX_FORMAT_TEX2F);    // M.M.
            *Stride = m_VertexSize[pVideoBuffer->m_nVertexFormat];
            return(pVideoBuffer->m_VS[VSF_GENERAL].m_VData);
          }
          pD = sGetBuf(pRM, Stride, VSF_GENERAL, Flags);
          pOffs = &gBufInfoTable[pRM->m_nVertexFormat];
          return pD;
        }
        else
          return NULL;
      }
    case eSrcPointer_Normal:
      pD = sGetBuf(pRM, Stride, VSF_GENERAL, Flags);
      if (pRM->m_nVertexFormat == VERTEX_FORMAT_P3F_N4B_COL4UB)
      {
        return &pD[sizeof(Vec3)];
      }
      else
      {
        Warning("Error: Missed normal pointer for shader '%s'", gRenDev->m_RP.m_pShader->GetName());
        return NULL;
      }
    case eSrcPointer_Binormal:
      pD = sGetBuf(pRM, Stride, VSF_TANGENTS, Flags);
      return &pD[sizeof(int16f)*4];
    case eSrcPointer_Tangent:
      pD = sGetBuf(pRM, Stride, VSF_TANGENTS, Flags);
      return &pD[0];
    case eSrcPointer_Color:
      pD = sGetBuf(pRM, Stride, VSF_GENERAL, Flags);
      pOffs = &gBufInfoTable[pRM->m_nVertexFormat];
      return &pD[pOffs->OffsColor];
    default:
      assert(false);
      break;
  }
  return NULL;
}

void CREMesh::mfGetPlane(Plane& pl)
{
  CRenderMesh *pRM = m_pRenderMesh->GetVertexContainerInternal();

  // fixme: plane orientation based on biggest bbox axis
  Vec3 pMin, pMax;
  mfGetBBox(pMin, pMax);
  Vec3 p0 = pMin;
  Vec3 p1 = Vec3(pMax.x, pMin.y, pMin.z);
  Vec3 p2 = Vec3(pMin.x, pMax.y, pMin.z);
  pl.SetPlane(p2, p0, p1);
}
