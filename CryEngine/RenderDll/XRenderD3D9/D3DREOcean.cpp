/*=============================================================================
  D3DREOcean.cpp : implementation of the Ocean Rendering.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <I3DEngine.h>

#if !defined(XENON) && !defined(PS3)

//=======================================================================

void CREOcean::GenerateIndices(int nLodCode)
{
  CD3D9Renderer *r = gcpRendD3D;
  SPrimitiveGroup pg;
  TArray<ushort> Indicies;

  if (m_OceanIndicies[nLodCode])
    return;
  SOceanIndicies *oi = new SOceanIndicies;
  m_OceanIndicies[nLodCode] = oi;
  int size;
  HRESULT h;
  ushort *dst;
  if (!(nLodCode & ~LOD_MASK))
  {
    int nL = nLodCode & LOD_MASK;
    pg.offsIndex = 0;
    pg.numIndices = m_pIndices[nL].Num();
    pg.numTris = pg.numIndices-2;
    pg.type = PT_STRIP;
    oi->m_Groups.AddElem(pg);
    size = pg.numIndices*sizeof(ushort); 
#if defined (DIRECT3D9) || defined(OPENGL)
    LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
    int flags = D3DUSAGE_WRITEONLY;
    D3DPOOL Pool = D3DPOOL_MANAGED;
    IDirect3DIndexBuffer9* ibuf;
    h = dv->CreateIndexBuffer(size, flags, D3DFMT_INDEX16, Pool, (IDirect3DIndexBuffer9**)&oi->m_pIndicies, NULL);
    ibuf = (IDirect3DIndexBuffer9*)oi->m_pIndicies;
    oi->m_nInds = pg.numIndices;
    h = ibuf->Lock(0, 0, (void **) &dst, 0);
    cryMemcpy(dst, &m_pIndices[nL][0], size);
    h = ibuf->Unlock();
#elif defined (DIRECT3D10)
    ID3D10Device *dv = gcpRendD3D->GetD3DDevice();
    ID3D10Buffer *ibuf = NULL;
    D3D10_BUFFER_DESC BufDesc;
    BufDesc.ByteWidth = size;
    BufDesc.Usage = D3D10_USAGE_DEFAULT;
    BufDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
    BufDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    BufDesc.MiscFlags = 0;
    h = dv->CreateBuffer(&BufDesc, NULL, &ibuf);
    if (SUCCEEDED(h))
    {
      oi->m_pIndicies = (ushort *)ibuf;
      h = ibuf->Map(D3D10_MAP_WRITE, 0, (void **)&dst);
      cryMemcpy(dst, &m_pIndices[nL][0], size);
      ibuf->Unmap();
    }
#endif
    return;
  }
  int nLod = nLodCode & LOD_MASK;
  int nl = 1<<nLod;
  int nGrid = (OCEANGRID+1);
  // set indices
  int iIndex = nGrid*nl+nl;
  int yStep = nGrid * nl;
  for(int a=nl; a<nGrid-1-nl; a+=nl)
  {
    for(int i=nl; i<nGrid-nl; i+=nl, iIndex+=nl)
    {
      Indicies.AddElem(iIndex);
      Indicies.AddElem(iIndex + yStep);
    }

    int iNextIndex = (a+nl) * nGrid + nl;

    // connect two strips by inserting two degenerated triangles 
    if(a < nGrid-1-nl*2)
    {
      Indicies.AddElem(iIndex + yStep - nl);
      Indicies.AddElem(iNextIndex);
    }
    iIndex = iNextIndex;
  }
  pg.numIndices = Indicies.Num();
  pg.numTris = pg.numIndices-2;
  pg.type = PT_STRIP;
  pg.offsIndex = 0;
  oi->m_Groups.AddElem(pg);

  // Left
  pg.offsIndex = Indicies.Num();
  iIndex = nGrid*(nGrid-1);
  if (!(nLodCode & (1<<LOD_LEFTSHIFT)))
  {
    Indicies.AddElem(iIndex);
    Indicies.AddElem(iIndex - nGrid * nl + nl);
    Indicies.AddElem(iIndex - nGrid * nl);
    iIndex = iIndex - nGrid * nl + nl;
    yStep = -(nGrid * nl) - nl;
    for(int i=nl; i<nGrid-nl; i+=nl, iIndex-=nGrid*nl)
    {
      Indicies.AddElem(iIndex);
      Indicies.AddElem(iIndex + yStep);
    }
  }
  else
  {
    for(int i=0; i<nGrid-nl; i+=nl*2)
    {
      Indicies.AddElem(iIndex);
      Indicies.AddElem(iIndex - (nGrid * nl) + nl);
      Indicies.AddElem(iIndex - nGrid * nl * 2);
      if (i < nGrid-nl-nl*2)
        Indicies.AddElem(iIndex - nGrid * nl * 2 + nl);
      iIndex -= nGrid * nl * 2;
    }
  }
  pg.numIndices = Indicies.Num()-pg.offsIndex;
  pg.numTris = pg.numIndices-2;
  oi->m_Groups.AddElem(pg);

  // Bottom
  pg.offsIndex = Indicies.Num();
  iIndex = 0;
  if (!(nLodCode & (1<<LOD_TOPSHIFT)))
  {
    Indicies.AddElem(iIndex);
    Indicies.AddElem(iIndex + nGrid*nl + nl);
    Indicies.AddElem(iIndex + nl);
    iIndex = iIndex + nGrid*nl + nl;
    yStep = -(nGrid * nl) + nl;
    for(int i=nl; i<nGrid-nl; i+=nl, iIndex+=nl)
    {
      Indicies.AddElem(iIndex);
      Indicies.AddElem(iIndex + yStep);
    }
  }
  else
  {
    for(int i=0; i<nGrid-nl; i+=nl*2)
    {
      Indicies.AddElem(iIndex);
      Indicies.AddElem(iIndex + (nGrid * nl) + nl);
      Indicies.AddElem(iIndex + nl * 2);
      if (i < nGrid-nl-nl*2)
        Indicies.AddElem(iIndex + nGrid * nl + nl * 2);
      iIndex += nl * 2;
    }
  }
  pg.numIndices = Indicies.Num()-pg.offsIndex;
  pg.numTris = pg.numIndices-2;
  oi->m_Groups.AddElem(pg);

  // Right
  pg.offsIndex = Indicies.Num();
  iIndex = nGrid-1;
  if (!(nLodCode & (1<<LOD_RIGHTSHIFT)))
  {
    Indicies.AddElem(iIndex);
    Indicies.AddElem(iIndex + nGrid * nl - nl);
    Indicies.AddElem(iIndex + nGrid * nl);
    iIndex = iIndex + nGrid * nl - nl;
    yStep = nGrid * nl + nl;
    for(int i=nl; i<nGrid-nl; i+=nl, iIndex+=nGrid*nl)
    {
      Indicies.AddElem(iIndex);
      Indicies.AddElem(iIndex + yStep);
    }
  }
  else
  {
    for(int i=0; i<nGrid-nl; i+=nl*2)
    {
      Indicies.AddElem(iIndex);
      Indicies.AddElem(iIndex + (nGrid * nl) - nl);
      Indicies.AddElem(iIndex + nGrid * nl * 2);
      if (i < nGrid-nl-nl*2)
        Indicies.AddElem(iIndex + nGrid * nl * 2 - nl);
      iIndex += nGrid * nl * 2;
    }
  }
  pg.numIndices = Indicies.Num()-pg.offsIndex;
  pg.numTris = pg.numIndices-2;
  oi->m_Groups.AddElem(pg);

  // Top
  pg.offsIndex = Indicies.Num();
  iIndex = nGrid*(nGrid-1)+nGrid-1;
  if (!(nLodCode & (1<<LOD_BOTTOMSHIFT)))
  {
    Indicies.AddElem(iIndex);
    Indicies.AddElem(iIndex - nGrid*nl - nl);
    Indicies.AddElem(iIndex - nl);
    iIndex = iIndex - nGrid*nl - nl;
    yStep = nGrid * nl - nl;
    for(int i=nl; i<nGrid-nl; i+=nl, iIndex-=nl)
    {
      Indicies.AddElem(iIndex);
      Indicies.AddElem(iIndex + yStep);
    }
  }
  else
  {
    for(int i=0; i<nGrid-nl; i+=nl*2)
    {
      Indicies.AddElem(iIndex);
      Indicies.AddElem(iIndex - (nGrid * nl) - nl);
      Indicies.AddElem(iIndex - nl * 2);
      if (i < nGrid-nl-nl*2)
        Indicies.AddElem(iIndex - nGrid * nl - nl * 2);
      iIndex -= nl * 2;
    }
  }
  pg.numIndices = Indicies.Num()-pg.offsIndex;
  pg.numTris = pg.numIndices-2;
  oi->m_Groups.AddElem(pg);

  size = Indicies.Num()*sizeof(ushort); 

#if defined (DIRECT3D9) || defined(OPENGL)
  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
  IDirect3DIndexBuffer9* ibuf;
  int flags = D3DUSAGE_WRITEONLY;
  D3DPOOL Pool = D3DPOOL_MANAGED;
  h = dv->CreateIndexBuffer(size, flags, D3DFMT_INDEX16, Pool, (IDirect3DIndexBuffer9**)&oi->m_pIndicies, NULL);
  ibuf = (IDirect3DIndexBuffer9*)oi->m_pIndicies;
  h = ibuf->Lock(0, 0, (void **) &dst, 0);
  memcpy(dst, &Indicies[0], size);
  h = ibuf->Unlock();
#elif defined (DIRECT3D10)
  ID3D10Device *dv = gcpRendD3D->GetD3DDevice();
  ID3D10Buffer *ibuf = NULL;
  D3D10_BUFFER_DESC BufDesc;
  BufDesc.ByteWidth = size;
  BufDesc.Usage = D3D10_USAGE_DEFAULT;
  BufDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
  BufDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
  BufDesc.MiscFlags = 0;
  h = dv->CreateBuffer(&BufDesc, NULL, &ibuf);
  if (SUCCEEDED(h))
  {
    oi->m_pIndicies = (ushort *)ibuf;
    h = ibuf->Map(D3D10_MAP_WRITE, 0, (void **)&dst);
    memcpy(dst, &Indicies[0], size);
    ibuf->Unmap();
  }
#endif
  oi->m_nInds = Indicies.Num();
}


void CREOcean::UpdateTexture()
{
  if (m_CustomTexBind[0]>0 && !CRenderer::CV_r_oceantexupdate)
    return;

  float time0 = iTimer->GetAsyncCurTime();

#if defined (DIRECT3D9) || defined(OPENGL)

  HRESULT h;
  CD3D9Renderer *r = gcpRendD3D;
  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
  byte data[OCEANGRID][OCEANGRID][4];
  for(unsigned int y=0; y<OCEANGRID; y++)
  {
    for( unsigned int x=0; x<OCEANGRID; x++)
    {
      data[y][x][0] = (byte)(m_Normals[y][x].x * 127.0f + 128.0f);
      data[y][x][1] = (byte)(m_Normals[y][x].y * 127.0f + 128.0f);
      data[y][x][2] = (byte)(m_Normals[y][x].z * 127.0f + 128.0f);
      data[y][x][3] = 0;
    }
  }
  if (m_CustomTexBind[0] <= 0)
  {
    char name[128];
    sprintf(name, "$AutoOcean_%d", r->m_TexGenID++);
    CTexture *tp = CTexture::Create2DTexture(name, OCEANGRID, OCEANGRID, 1, FT_NOMIPS | FT_DONT_STREAM, &data[0][0][0], eTF_X8R8G8B8, eTF_X8R8G8B8);
    m_CustomTexBind[0] = tp->GetID();
  }
  else
  {
    CTexture *tp = CTexture::GetByID(m_CustomTexBind[0]);
    IDirect3DTexture9 *pID3DTexture = (IDirect3DTexture9*)tp->GetDeviceTexture();
    D3DLOCKED_RECT d3dlr;
    h = pID3DTexture->LockRect(0, &d3dlr, NULL, 0);
    D3DSURFACE_DESC ddsdDescDest;
    pID3DTexture->GetLevelDesc(0, &ddsdDescDest);
    if (d3dlr.Pitch == tp->GetWidth()*4)
      memcpy(d3dlr.pBits, data, tp->GetWidth()*tp->GetHeight()*4);
    else
    {
      switch(ddsdDescDest.Format)
      {
        case D3DFMT_X8L8V8U8:
        case D3DFMT_L6V5U5:
          assert(0);
      	  break;
        case D3DFMT_V8U8:
          {
            byte *pDst = (byte *)d3dlr.pBits;
            byte *pSrc = &data[0][0][0];
            for (int i=0; i<tp->GetHeight(); i++)
            {
              for (int j=0; j<tp->GetWidth(); j++)
              {
                pDst[j*2+0] = pSrc[j*2+0];
                pDst[j*2+1] = pSrc[j*2+1];
              }
              pDst += d3dlr.Pitch;
              pSrc += tp->GetWidth()*2;
            }
          }
          break;
        case D3DFMT_X8R8G8B8:
          {
            byte *pDst = (byte *)d3dlr.pBits;
            byte *pSrc = &data[0][0][0];
            for (int i=0; i<tp->GetHeight(); i++)
            {
              for (int j=0; j<tp->GetWidth(); j++)
              {
                pDst[j*4+0] = pSrc[j*4+0];
                pDst[j*4+1] = pSrc[j*4+1];
                pDst[j*4+2] = pSrc[j*4+2];
                pDst[j*4+3] = pSrc[j*4+3];
              }
              pDst += d3dlr.Pitch;
              pSrc += tp->GetWidth()*4;
            }
          }
          break;
        default:
          assert(0);
      }
    }
    h = pID3DTexture->UnlockRect(0);
  }
#elif defined (DIRECT3D10)
  assert(0);
#endif
  m_RS.m_StatsTimeTexUpdate = iTimer->GetAsyncCurTime() - time0;
}

void CREOcean::DrawOceanSector(SOceanIndicies *oi)
{
  CD3D9Renderer *r = gcpRendD3D;

#if defined (DIRECT3D9) || defined(OPENGL)

  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
  HRESULT h;
  h = r->FX_SetIStream((IDirect3DIndexBuffer9 *)oi->m_pIndicies);
  for (uint i=0; i<oi->m_Groups.Num(); i++)
  {
    SPrimitiveGroup *g = &oi->m_Groups[i];
    switch (g->type)
    {
      case PT_STRIP:
        if (FAILED(h=dv->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, 0, 0, (OCEANGRID+1)*(OCEANGRID+1), g->offsIndex, g->numTris)))
        {
          r->Error("CREOcean::DrawOceanSector: DrawIndexedPrimitive error", h);
          return;
        }
        break;

      case PT_LIST:
        if (FAILED(h=dv->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, (OCEANGRID+1)*(OCEANGRID+1), g->offsIndex, g->numTris)))
        {
          r->Error("CREOcean::DrawOceanSector: DrawIndexedPrimitive error", h);
          return;
        }
        break;

      case PT_FAN:
        if (FAILED(h=dv->DrawIndexedPrimitive(D3DPT_TRIANGLEFAN, 0, 0, (OCEANGRID+1)*(OCEANGRID+1), g->offsIndex, g->numTris)))
        {
          r->Error("CREOcean::DrawOceanSector: DrawIndexedPrimitive error", h);
          return;
        }
        break;
    }
    r->m_RP.m_PS.m_nPolygons[r->m_RP.m_nPassGroupDIP] += g->numTris;
    r->m_RP.m_PS.m_nDIPs[r->m_RP.m_nPassGroupDIP]++;
  }
#elif defined (DIRECT3D10)
  assert(0);
#endif
}

static inline bool Compare(SOceanSector *p1, SOceanSector *p2)
{
  if(p1->m_Flags != p2->m_Flags)
    return p1->m_Flags < p2->m_Flags;
  return false;
}

static _inline float sCalcSplash(SSplash *spl, float fX, float fY)
{
  CD3D9Renderer *r = gcpRendD3D;

  float fDeltaTime = r->m_RP.m_RealTime - spl->m_fStartTime;
  float fScaleFactor = 1.0f / (r->m_RP.m_RealTime - spl->m_fLastTime + 1.0f);

  float vDelt[2];

  // Calculate 2D distance
  vDelt[0] = spl->m_Pos[0] - fX; vDelt[1] = spl->m_Pos[1] - fY;
  float fSqDist = vDelt[0]*vDelt[0] + vDelt[1]*vDelt[1];

  // Inverse square root
  unsigned int *n1 = (unsigned int *)&fSqDist;
  unsigned int nn = 0x5f3759df - (*n1 >> 1);
  float *n2 = (float *)&nn;
  float fDistSplash = 1.0f / ((1.5f - (fSqDist * 0.5f) * *n2 * *n2) * *n2);

  // Emulate sin waves
  float fDistFactor = fDeltaTime*10.0f - fDistSplash + 4.0f;
  fDistFactor = CLAMP(fDistFactor, 0.0f, 1.0f);
  float fRad = (fDistSplash - fDeltaTime*10) * 0.4f / 3.1416f * 1024.0f;
  float fSin = gRenDev->m_RP.m_tSinTable[QRound(fRad)&0x3ff] * fDistFactor;

  return fSin * fScaleFactor * spl->m_fForce;
}

float *CREOcean::mfFillAdditionalBuffer(SOceanSector *os, int nSplashes, SSplash *pSplashes[], int& nCurSize, int nLod, float fSize)
{
  float *pHM, *pTZ;

  int nFloats = (os->m_Flags & OSF_NEEDHEIGHTS) ? 1 : 0;
  if (nSplashes)
    nFloats++;

  pTZ = (float *)GetVBPtr((OCEANGRID+1)*(OCEANGRID+1));
  pHM = pTZ;
  int nStep = 1<<nLod;
  if (!nSplashes)
  {
    for (int ty=0; ty<OCEANGRID+1; ty+=nStep)
    {
      pTZ = &pHM[ty*(OCEANGRID+1)*2];
      for (int tx=0; tx<OCEANGRID+1; tx+=nStep)
      {
        float fX = m_Pos[ty][tx][0]*fSize+os->x;
        float fY = m_Pos[ty][tx][1]*fSize+os->y;
        float fZ = GetHMap(fX, fY);
        pTZ[0] = fZ;
        pTZ += nStep*2;
      }
    }
  }
  else
  if (!(os->m_Flags & OSF_NEEDHEIGHTS))
  {
    for (int ty=0; ty<OCEANGRID+1; ty+=nStep)
    {
      pTZ = &pHM[ty*(OCEANGRID+1)*2];
      for (int tx=0; tx<OCEANGRID+1; tx+=nStep)
      {
        float fX = m_Pos[ty][tx][0]*fSize+os->x;
        float fY = m_Pos[ty][tx][1]*fSize+os->y;
        float fSplash = 0;
        for (int n=0; n<nSplashes; n++)
        {
          SSplash *spl = pSplashes[n];
          fSplash += sCalcSplash(spl, fX, fY);
        }
        pTZ[0] = 0;
        pTZ[1] = fSplash;
        pTZ += nStep*2;
      }
    }
  }
  else
  {
    for (int ty=0; ty<OCEANGRID+1; ty+=nStep)
    {
      pTZ = &pHM[ty*(OCEANGRID+1)*2];
      for (int tx=0; tx<OCEANGRID+1; tx+=nStep)
      {
        float fX = m_Pos[ty][tx][0]*fSize+os->x;
        float fY = m_Pos[ty][tx][1]*fSize+os->y;
        float fZ = GetHMap(fX, fY);
        pTZ[0] = fZ;
        float fSplash = 0;
        for (int n=0; n<nSplashes; n++)
        {
          SSplash *spl = pSplashes[n];
          fSplash += sCalcSplash(spl, fX, fY);
        }
        pTZ[1] = fSplash; // / (float)nSplashes;
        pTZ += nStep*2;
      }
    }
  }
  return pHM;
}

void CREOcean::mfReset()
{
  for (int i=0; i<NUM_OCEANVBS; i++)
  {
    IDirect3DVertexBuffer9* vb = (IDirect3DVertexBuffer9*)m_pVertsPool[i];
    SAFE_RELEASE(vb);
    m_pVertsPool[i] = NULL;
  }
  if (m_pBuffer)
  {
    gRenDev->ReleaseBuffer(m_pBuffer, m_nNumVertsInPool);
    m_pBuffer = NULL;
  }
}

void CREOcean::InitVB()
{
  CD3D9Renderer *r = gcpRendD3D;

#if defined (DIRECT3D9) || defined(OPENGL)

  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
  HRESULT h;
  m_nNumVertsInPool = (OCEANGRID+1)*(OCEANGRID+1);
  int size = m_nNumVertsInPool * sizeof(struct_VERTEX_FORMAT_TEX2F);
  for (int i=0; i<NUM_OCEANVBS; i++)
  {
    h = dv->CreateVertexBuffer(size, D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, D3DFVF_TEX1, D3DPOOL_DEFAULT, (IDirect3DVertexBuffer9**)&m_pVertsPool[i], NULL);
  }
  m_nCurVB = 0;
  m_bLockedVB = false;

  {
    D3DVERTEXELEMENT9 elem[] =
    {
      {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},  // position
      D3DDECL_END()
    };
    h = dv->CreateVertexDeclaration(&elem[0], (IDirect3DVertexDeclaration9**)&m_VertDecl);
  }
  {
    D3DVERTEXELEMENT9 elem[] =
    {
      {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},  // position
      {1, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0}, // heightmpa/splashes
      D3DDECL_END()
    };
    h = dv->CreateVertexDeclaration(&elem[0], (IDirect3DVertexDeclaration9**)&m_VertDeclHeightSplash);
  }
#elif defined (DIRECT3D10)
#endif
}

struct_VERTEX_FORMAT_TEX2F *CREOcean::GetVBPtr(int nVerts)
{
  struct_VERTEX_FORMAT_TEX2F *pVertices = NULL;
  if (nVerts > m_nNumVertsInPool)
  {
    assert(0);
    return NULL;
  }
  if (m_bLockedVB)
    UnlockVBPtr();
  m_nCurVB++;
  if (m_nCurVB >= NUM_OCEANVBS)
    m_nCurVB = 0;
#if defined (DIRECT3D9) || defined(OPENGL)
  HRESULT h;
  IDirect3DVertexBuffer9 *pVB = (IDirect3DVertexBuffer9 *)m_pVertsPool[m_nCurVB];
  h = pVB->Lock(0, 0, (void **) &pVertices, D3DLOCK_DISCARD);
#elif defined (DIRECT3D10)
  assert(0);
#endif
  m_bLockedVB = true;
  return pVertices;
}

void CREOcean::UnlockVBPtr()
{
  if (m_bLockedVB)
  {
#if defined (DIRECT3D9) || defined(OPENGL)
    IDirect3DVertexBuffer9 *pVB = (IDirect3DVertexBuffer9 *)m_pVertsPool[m_nCurVB];
    HRESULT hr = pVB->Unlock();
#elif defined (DIRECT3D10)
    ID3D10Buffer *pVB = (ID3D10Buffer *)m_pVertsPool[m_nCurVB];
    pVB->Unmap();
#endif
    m_bLockedVB = false;
  }
}

bool CREOcean::mfDrawOceanSectors(CShader *ef, SShaderPass *sfm)
{ 
  float x, y;
  CD3D9Renderer *r = gcpRendD3D;
  HRESULT h;
  CVertexBuffer *vb = m_pBuffer;
  uint i;

  m_RS.m_StatsNumRendOceanSectors = 0;

  CShader *saveShader = r->m_RP.m_pShader;
  ushort *saveInds = r->m_RP.m_RendIndices;
  I3DEngine *eng = (I3DEngine *)iSystem->GetI3DEngine();
  float fMaxDist = eng->GetMaxViewDistance() / 1.0f;
  CCamera cam, *camCull;
  cam = r->GetCamera();
  if (r->m_RP.m_pCurObject->m_pCustomCamera)
    camCull = r->m_RP.m_pCurObject->m_pCustomCamera;
  else
    camCull = &cam;
  Vec3 mins;
  Vec3 maxs;
  Vec3 cameraPos = cam.GetPosition();
  float fCurX = (float)((int)cameraPos[0] & ~255) + 128.0f;
  float fCurY = (float)((int)cameraPos[1] & ~255) + 128.0f;

  float fSize = (float)CRenderer::CV_r_oceansectorsize;
  float fHeightScale = (float)CRenderer::CV_r_oceanheightscale;
  fMaxDist = (float)((int)(fMaxDist / fSize + 1.0f)) * fSize;
  if (fSize != m_fSectorSize)
  {
    m_fSectorSize = fSize;
    for (int i=0; i<OCEAN_HASH_SIZE; i++)
    {
      m_OceanSectorsHash[i].Free();
    }
  }
  float fWaterLevel = eng->GetWaterLevel();
  int nMaxSplashes = CLAMP(CRenderer::CV_r_oceanmaxsplashes, 0, 16);

	// hack to make sure the following code doesn't suffer from reallocations - this should be optimized!
  for (y=fCurY-fMaxDist; y<fCurY+fMaxDist; y+=fSize)
    for (x=fCurX-fMaxDist; x<fCurX+fMaxDist; x+=fSize)
      GetSectorByPos(x, y);

  //x = y = 0;
  //x = fCurX;
  //y = fCurY;
  m_VisOceanSectors.SetUse(0);
  int osNear = -1;
  int nMinLod = 65536;
  for (y=fCurY-fMaxDist; y<fCurY+fMaxDist; y+=fSize)
  {
    for (x=fCurX-fMaxDist; x<fCurX+fMaxDist; x+=fSize)
    {
      SOceanSector *os = GetSectorByPos(x, y);
      if (!(os->m_Flags & (OSF_FIRSTTIME | OSF_VISIBLE)))
        continue;
      mins.x = x+m_MinBound.x*fSize;
      mins.y = y+m_MinBound.y*fSize;
      mins.z = fWaterLevel+m_MinBound.z*fHeightScale;
      maxs.x = x+fSize+m_MaxBound.x*fSize;
      maxs.y = y+fSize+m_MaxBound.y*fSize;
      maxs.z = fWaterLevel+m_MaxBound.z*fHeightScale;
      int cull = camCull->IsAABBVisible_EH(AABB(mins,maxs));
      if (cull != CULL_EXCLUSION)
      {
        Vec3 vCenter = (mins + maxs) * 0.5f;
        os->nLod = GetLOD(cameraPos, vCenter);
        if (os->nLod < nMinLod)
        {
          nMinLod = os->nLod;
          osNear = m_VisOceanSectors.Num();
        }
        os->m_Frame = gRenDev->m_cEF.m_Frame;
        os->m_Flags &= ~OSF_LODUPDATED;
        m_VisOceanSectors.AddElem(os);
      }
    }
  }
  if (m_VisOceanSectors.Num())
  {
    LinkVisSectors(fSize);
    std::sort(&m_VisOceanSectors[0], &m_VisOceanSectors[0] + m_VisOceanSectors.Num(), Compare);
  }
  int nCurSize;
  bool bCurNeedBuffer = false;
#if defined (DIRECT3D9) || defined(OPENGL)
  h = gcpRendD3D->m_pd3dDevice->SetVertexDeclaration((LPDIRECT3DVERTEXDECLARATION9)m_VertDecl);
  assert (h == S_OK);
  r->m_pLastVDeclaration = (LPDIRECT3DVERTEXDECLARATION9)m_VertDecl;
#elif defined (DIRECT3D10)
  gcpRendD3D->m_pd3dDevice->IASetInputLayout((ID3D10InputLayout *)m_VertDecl);
  r->m_pLastVDeclaration = (ID3D10InputLayout *)m_VertDecl;
#endif
  float *pHM, *pTZ;
  int nIndVP = CRenderer::CV_r_waterreflections ? 0 : 1;
  uint nPasses = 0;
  ef->FXBegin(&nPasses, 0);
  if (!nPasses)
    return false;
  mfPreDraw(sfm);
  uint64 nRT = ~0;
  for (i=0; i<m_VisOceanSectors.Num(); i++)
  {
    SOceanSector *os = m_VisOceanSectors[i];
//    if ((int)os != 0x1ac32240 && (int)os != 0xe985fc8)
//      continue;
    int nLod = os->nLod;
		
		SOceanSector *osLeft = GetSectorByPos(os->x-fSize, os->y, false);
    SOceanSector *osRight = GetSectorByPos(os->x+fSize, os->y, false);
    SOceanSector *osTop = GetSectorByPos(os->x, os->y-fSize, false);
    SOceanSector *osBottom = GetSectorByPos(os->x, os->y+fSize, false);

    bool bL = osLeft && nLod < osLeft->nLod;
    bool bR = osRight && nLod < osRight->nLod;
    bool bT = osTop && nLod < osTop->nLod;
    bool bB = osBottom && nLod < osBottom->nLod;

    int nLodCode = nLod + (bL<<LOD_LEFTSHIFT) + (bR<<LOD_RIGHTSHIFT) + (bB<<LOD_BOTTOMSHIFT) + (bT<<LOD_TOPSHIFT);
    if (!m_OceanIndicies[nLodCode])
      GenerateIndices(nLodCode);
    int nSplashes = 0;
    SSplash *pSplashes[16];
    uint nS;
    for (nS=0; nS<r->m_RP.m_Splashes.Num(); nS++)
    {
      SSplash *spl = &r->m_RP.m_Splashes[nS];
      float fCurRadius = spl->m_fCurRadius;
      if (spl->m_Pos[0]-fCurRadius > os->x+fSize ||
        spl->m_Pos[1]-fCurRadius > os->y+fSize ||
        spl->m_Pos[0]+fCurRadius < os->x ||
        spl->m_Pos[1]+fCurRadius < os->y)
        continue;
      pSplashes[nSplashes++] = spl;
      if (nSplashes == nMaxSplashes)
        break;
    }
    if (os->m_Flags & OSF_FIRSTTIME)
    {
      pTZ = (float *)GetVBPtr((OCEANGRID+1)*(OCEANGRID+1));
      pHM = pTZ;

      float fMinLevel = 99999.0f;
      bool bBlend = false;
      bool bNeedHeights = false;
      for (uint ty=0; ty<OCEANGRID+1; ty++)
      {
        for (uint tx=0; tx<OCEANGRID+1; tx++)
        {
          float fX = m_Pos[ty][tx][0]*fSize+os->x;
          float fY = m_Pos[ty][tx][1]*fSize+os->y;
          float fZ = GetHMap(fX, fY);
          pTZ[0] = fZ;
          pTZ += 2;
          fMinLevel = min(fMinLevel, fZ);
          if (!bBlend && fWaterLevel-fZ <= 1.0f)
            bBlend = true;
          if (fWaterLevel-fZ < 16.0f)
            bNeedHeights = true;
        }
      }
      os->m_Flags &= ~OSF_FIRSTTIME;
      if (fMinLevel <= fWaterLevel)
        os->m_Flags |= OSF_VISIBLE;
      //bNeedHeights = bBlend = false;
      if (bNeedHeights)
        os->m_Flags |= OSF_NEEDHEIGHTS;
      if (bBlend)
      {
        os->m_Flags |= OSF_BLEND;
        os->RenderState = GS_DEPTHWRITE | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
      }
      else
        os->RenderState = GS_DEPTHWRITE;
    }
    else
    if ((os->m_Flags & OSF_NEEDHEIGHTS) || nSplashes)
      pHM = mfFillAdditionalBuffer(os, nSplashes, pSplashes, nCurSize, nLod, fSize);
    m_RS.m_StatsNumRendOceanSectors++;

    r->EF_SetState(os->RenderState);
    int nVP = 0;
    if (os->m_Flags & OSF_NEEDHEIGHTS)
      r->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
    if (nSplashes)
      r->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];

    // render
    Vec4 param;
    if (nRT != r->m_RP.m_FlagsShader_RT)
    {
      nRT = r->m_RP.m_FlagsShader_RT;
      ef->FXBeginPass(0);

      param[0] = fSize;
      param[1] = fSize;
      param[2] = fHeightScale;
      param[3] = 1;
			static CCryName PosScaleName("PosScale");
      ef->FXSetVSFloat(PosScaleName, &param, 1);

      param[2] = 0;
      param[3] = 0;
    }

    int nFloats = (os->m_Flags & OSF_NEEDHEIGHTS) ? 1 : 0;
    if (nSplashes)
      nFloats++;
    UnlockVBPtr();
    if (nFloats)
    {
      void *pVB = m_pVertsPool[m_nCurVB];
      h = r->FX_SetVStream(0, pVB, 0, sizeof(struct_VERTEX_FORMAT_TEX2F));
      if (!bCurNeedBuffer)
      {
#if defined (DIRECT3D9) || defined(OPENGL)
        h = gcpRendD3D->m_pd3dDevice->SetVertexDeclaration((LPDIRECT3DVERTEXDECLARATION9)m_VertDeclHeightSplash);
#elif defined (DIRECT3D10)
        gcpRendD3D->m_pd3dDevice->IASetInputLayout((ID3D10InputLayout *)m_VertDeclHeightSplash);
#endif
        bCurNeedBuffer = true;
      }
    }
    else
    if (bCurNeedBuffer)
    {
      h = r->FX_SetVStream(1, NULL, 0, 0);
#if defined (DIRECT3D9) || defined(OPENGL)
      h = gcpRendD3D->m_pd3dDevice->SetVertexDeclaration((LPDIRECT3DVERTEXDECLARATION9)m_VertDecl);
#elif defined (DIRECT3D10)
      gcpRendD3D->m_pd3dDevice->IASetInputLayout((ID3D10InputLayout *)m_VertDecl);
#endif
      bCurNeedBuffer = false;
    }
    param[0] = os->x;
    param[1] = os->y;
		static CCryName PosOffsetName("PosOffset");
    ef->FXSetVSFloat(PosOffsetName, &param, 1);

    // commit all render changes
    r->EF_Commit();

    /*if (nSplashes)
    {
      static Vec3 sPos;
      static float sTime;
      sPos = Vec3(306, 1942, 0);
      if ((GetAsyncKeyState(VK_NUMPAD1) & 0x8000))
      sTime = r->m_RP.m_RealTime;
      float fForce = 1.5f;
      cgBindIter *pBind = vpGL->mfGetParameterBind("Splash1");

      if (pBind)
      {
        param[0] = sPos[0];
        param[1] = sPos[1];
        param[2] = sPos[2];
        param[3] = 0;
        vpGL->mfParameter4f(pBind, param);
      }
      pBind = vpGL->mfGetParameterBind("Time");
      if (pBind)
      {
        float fDeltaTime = r->m_RP.m_RealTime - sTime;
        float fScaleFactor = 1.0f / (r->m_RP.m_RealTime - sTime + 1.0f);
        param[0] = fForce * fScaleFactor;
        param[1] = 0;
        param[2] = -fDeltaTime*10;
        param[3] = fDeltaTime*10.0f;
        vpGL->mfParameter4f(pBind, param);
      }
    }*/

    DrawOceanSector(m_OceanIndicies[nLodCode]);
  }
  ef->FXEnd();

  r->m_RP.m_pShader = saveShader;

  return true;
}

bool CREOcean::mfDrawOceanScreenLod()
{ 
  return true;
}

bool CREOcean::mfDraw(CShader *ef, SShaderPass *sfm)
{ 
  float time0 = iTimer->GetAsyncCurTime();

  if (!m_pVertsPool[0])
    InitVB();
  if (!m_pBuffer)
    m_pBuffer = gRenDev->CreateBuffer((OCEANGRID+1)*(OCEANGRID+1), VERTEX_FORMAT_P3F, "Ocean", true);

  if (CRenderer::CV_r_oceanrendtype == 0)
    mfDrawOceanSectors(ef, sfm);
  else
    mfDrawOceanScreenLod();
  m_RS.m_StatsTimeRendOcean = iTimer->GetAsyncCurTime() - time0;

//  gRenDev->PostLoad();

  return true;
}


bool CREOcean::mfPreDraw(SShaderPass *sl)
{
  CVertexBuffer *vb = m_pBuffer;

  for (int i=0; i<VSF_NUM; i++)
  {
    if (vb->m_VS[i].m_bLocked)
      gcpRendD3D->UnlockBuffer(vb, i, m_nNumVertsInPool);
  }

  int nOffs;
  IDirect3DVertexBuffer9 *vptr = (IDirect3DVertexBuffer9 *)vb->GetStream(VSF_GENERAL, &nOffs);
  HRESULT h = gcpRendD3D->FX_SetVStream(0, vptr, nOffs, m_VertexSize[vb->m_nVertexFormat]);

  return true;
}



#ifndef _LIB
uint8 BoxSides[0x40*8] = {
	  0,0,0,0, 0,0,0,0, //00
		0,4,6,2, 0,0,0,4, //01
		7,5,1,3, 0,0,0,4, //02
		0,0,0,0, 0,0,0,0, //03
		0,1,5,4, 0,0,0,4, //04
		0,1,5,4, 6,2,0,6, //05
		7,5,4,0, 1,3,0,6, //06
		0,0,0,0, 0,0,0,0, //07
		7,3,2,6, 0,0,0,4, //08
		0,4,6,7, 3,2,0,6, //09
		7,5,1,3, 2,6,0,6, //0a
		0,0,0,0, 0,0,0,0, //0b
		0,0,0,0, 0,0,0,0, //0c
		0,0,0,0, 0,0,0,0, //0d
		0,0,0,0, 0,0,0,0, //0e
		0,0,0,0, 0,0,0,0, //0f
		0,2,3,1, 0,0,0,4, //10
		0,4,6,2, 3,1,0,6, //11
		7,5,1,0, 2,3,0,6, //12
		0,0,0,0, 0,0,0,0, //13
		0,2,3,1, 5,4,0,6, //14
		1,5,4,6, 2,3,0,6, //15
		7,5,4,0, 2,3,0,6, //16
		0,0,0,0, 0,0,0,0, //17
		0,2,6,7, 3,1,0,6, //18
		0,4,6,7, 3,1,0,6, //19
		7,5,1,0, 2,6,0,6, //1a
		0,0,0,0, 0,0,0,0, //1b
		0,0,0,0, 0,0,0,0, //1c
		0,0,0,0, 0,0,0,0, //1d
		0,0,0,0, 0,0,0,0, //1e
		0,0,0,0, 0,0,0,0, //1f
		7,6,4,5, 0,0,0,4, //20
		0,4,5,7, 6,2,0,6, //21
		7,6,4,5, 1,3,0,6, //22
		0,0,0,0, 0,0,0,0, //23
		7,6,4,0, 1,5,0,6, //24
		0,1,5,7, 6,2,0,6, //25
		7,6,4,0, 1,3,0,6, //26
		0,0,0,0, 0,0,0,0, //27
		7,3,2,6, 4,5,0,6, //28
		0,4,5,7, 3,2,0,6, //29
		6,4,5,1, 3,2,0,6, //2a
		0,0,0,0, 0,0,0,0, //2b
		0,0,0,0, 0,0,0,0, //2c
		0,0,0,0, 0,0,0,0, //2d
		0,0,0,0, 0,0,0,0, //2e
		0,0,0,0, 0,0,0,0, //2f
		0,0,0,0, 0,0,0,0, //30
		0,0,0,0, 0,0,0,0, //31
		0,0,0,0, 0,0,0,0, //32
		0,0,0,0, 0,0,0,0, //33
		0,0,0,0, 0,0,0,0, //34
		0,0,0,0, 0,0,0,0, //35
		0,0,0,0, 0,0,0,0, //36
		0,0,0,0, 0,0,0,0, //37
		0,0,0,0, 0,0,0,0, //38
		0,0,0,0, 0,0,0,0, //39
		0,0,0,0, 0,0,0,0, //3a
		0,0,0,0, 0,0,0,0, //3b
		0,0,0,0, 0,0,0,0, //3c
		0,0,0,0, 0,0,0,0, //3d
		0,0,0,0, 0,0,0,0, //3e
		0,0,0,0, 0,0,0,0, //3f
};
#endif

#endif
