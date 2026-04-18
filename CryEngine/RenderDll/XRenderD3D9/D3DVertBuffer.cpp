/*=============================================================================
  D3DTexturesStreaming.cpp : Direct3D9 vertex/index buffers management.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"

//===============================================================================

#ifdef _DEBUG
TArray<SDynVB> gDVB;
#endif

//#define VB_VIDEO_POOLS 1
//#define IB_VIDEO_POOLS 1

//===============================================================================

void CD3D9Renderer::DrawDynVB(int nOffs, int Pool, int nVerts)
{
	PROFILE_FRAME(Draw_IndexMesh_Dyn);

  if (!m_SceneRecurseCount)
  {
    iLog->Log("Error: CD3D9Renderer::DrawDynVB without BeginScene\n");
    return;
  }
  if (CV_d3d9_forcesoftware)
    return;
  if (!nVerts)
    return;
  if (m_bDeviceLost)
    return;

  HRESULT h = S_OK;

  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pDst = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(nVerts, nOffs);
  //assert(pDst);
  if (!pDst)
    return;

  if (m_TempDynVB)
  {
    memcpy(pDst, m_TempDynVB, nVerts*sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
    SAFE_DELETE_ARRAY(m_TempDynVB);
  }
  else
	{
    memcpy(pDst, m_DynVB, nVerts*sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
	}

  UnlockVB(POOL_P3F_COL4UB_TEX2F);
  FX_SetFPMode();

  // Bind our vertex as the first data stream of our device
  FX_SetVStream(0, m_pVB[POOL_P3F_COL4UB_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  if (FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
    return;

  // Render the two triangles from the data stream
#if defined (DIRECT3D9) || defined (OPENGL)
  h = m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, nOffs, nVerts/3);
#elif defined (DIRECT3D10)
  SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_pd3dDevice->Draw(nVerts, nOffs);
#endif
}


void CD3D9Renderer::DrawDynVB(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pBuf, ushort *pInds, int nVerts, int nInds, int nPrimType)
{
	PROFILE_FRAME(Draw_IndexMesh_Dyn);

  if (!pBuf)
    return;

  if (!m_SceneRecurseCount)
  {
    iLog->Log("Error: CD3D9Renderer::DrawDynVB without BeginScene\n");
    return;
  }
  if (CV_d3d9_forcesoftware)
    return;
  if (!nVerts || (pInds && !nInds))
    return;

#if defined (DIRECT3D9) || defined (OPENGL)
  if (FAILED(m_pd3dDevice->TestCooperativeLevel()))
    return;
#endif

  HRESULT h;

  int nOffs, nIOffs;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pDst = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(nVerts, nOffs);
  assert(pDst);
  if (!pDst)
    return;

  ushort *pDstInds = NULL;
  if (pInds)
    pDstInds = GetIBPtr(nInds, nIOffs);

  memcpy(pDst, pBuf, nVerts*sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  if (pDstInds)
	{
    memcpy(pDstInds, pInds, nInds*sizeof(short));
	}

  UnlockVB(0);
  if (pDstInds)
    UnlockIB();
  FX_SetFPMode();

  // Bind our vertex as the first data stream of our device
  h = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  // Render triangles from the data stream

	int primType;
	int vertexCount = (pDstInds ? nInds : nVerts);

#if defined (DIRECT3D9) || defined (OPENGL)
  int primCount;
	switch(nPrimType)
	{
	  case R_PRIMV_LINES:
		  primType = D3DPT_LINELIST;
		  primCount = vertexCount/2;
		  break;
		case R_PRIMV_LINESTRIP:
			primType = D3DPT_LINESTRIP;
			primCount = vertexCount-1;
			break;
	  case R_PRIMV_TRIANGLE_STRIP:
		  primType = D3DPT_TRIANGLESTRIP;
		  primCount = vertexCount-2;
		  break;
	  case R_PRIMV_TRIANGLE_FAN:
		  primType = D3DPT_TRIANGLEFAN;
		  primCount = vertexCount-2;
		  break;
	  default:
		  primType = D3DPT_TRIANGLELIST;
		  primCount = vertexCount/3;
		  break;
	}
#elif defined (DIRECT3D10)
  switch(nPrimType)
  {
    case R_PRIMV_LINES:
      primType = D3D10_PRIMITIVE_TOPOLOGY_LINELIST;
      break;
    case R_PRIMV_LINESTRIP:
      primType = D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP;
      break;
    case R_PRIMV_TRIANGLE_STRIP:
      primType = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
      break;
    case R_PRIMV_TRIANGLE_FAN:
      assert(0);
      break;
    default:
      primType = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      break;
  }
#endif

  if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
  {
	  if (pDstInds)
    {
      h = FX_SetIStream(m_pIB);
  #if defined (DIRECT3D9) || defined (OPENGL)
      h = m_pd3dDevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)primType, nOffs, 0, nVerts, nIOffs, primCount);
  #elif defined (DIRECT3D10)
      SetPrimitiveTopology((D3D10_PRIMITIVE_TOPOLOGY)primType);

      m_pd3dDevice->DrawIndexed(nInds, nIOffs, nOffs);
  #endif
    }
    else
    {
  #if defined (DIRECT3D9) || defined (OPENGL)
      h = m_pd3dDevice->DrawPrimitive((D3DPRIMITIVETYPE)primType, nOffs, primCount);
  #elif defined (DIRECT3D10)
      SetPrimitiveTopology((D3D10_PRIMITIVE_TOPOLOGY)primType);

      m_pd3dDevice->Draw(vertexCount, nOffs);
  #endif
    }
  }
}

//===============================================================================

void *CD3D9Renderer::GetDynVBPtr(int nVerts, int &nOffs, int Pool)
{
  void *vBuf = NULL;
  nOffs = 0;
  switch(Pool)
  {
    case 0:
    default:
      if (nVerts <= 2048)
      {
        SAFE_DELETE_ARRAY(m_TempDynVB);
        return m_DynVB;
      }
      else
      {
        m_TempDynVB = new struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F [nVerts];
        return m_TempDynVB;
      }
  	  break;

    case 1:
    case 2:
      vBuf = GetVBPtr(nVerts, nOffs, Pool);
  	  break;
  }
  return vBuf;
}

#ifdef _DEBUG
static char *szDescBuf[] = 
{
  "Base Mesh",
  "Mesh Tangents",
  "Mesh LM",
  "Mesh HW Skin",
  "Mesh SH",
  "Mesh Shape deform",
  "Mesh MorphTarget",
};
#endif

bool CD3D9Renderer::AllocateChunk(int bytes_count, TVertPool *Ptr, SVertexStream *pVB, const char *szSource)
{
  assert(bytes_count);


  alloc_info_struct *pAI = GetFreeChunk(bytes_count, Ptr->m_nBufSize, Ptr->m_alloc_info, szSource);
  if (pAI)
  {
    pVB->m_nBufOffset = pAI->ptr;
    pVB->m_pPool = (SVertPool *)Ptr;
    return true;
  }

  return false;
}

bool CD3D9Renderer::ReleaseVBChunk(TVertPool *Ptr, SVertexStream *pVB)
{
  int p = pVB->m_nBufOffset;
  bool bRes = ReleaseChunk(p, Ptr->m_alloc_info);
  if (!bRes)
    iLog->Log("Error: CD3D9Renderer::ReleaseVBChunk::ReleasePointer: pointer not found");

  return false;
}

void CD3D9Renderer::ReleaseVideoPools()
{
#if defined (VB_VIDEO_POOLS) || defined (IB_VIDEO_POOLS)
  CRenderMesh *pRM = CRenderMesh::m_Root.m_Prev;
  while (true)
  {
    if (pRM == &CRenderMesh::m_Root)
      break;

    CRenderMesh *Next = pRM->m_Prev;
    pRM->Unload();
    pRM = Next;
  }
#endif

  TVertPool* Ptr = sVertPools;

#ifdef VB_VIDEO_POOLS
  while(Ptr)
  {
    TVertPool* Next = Ptr->Next;
    IDirect3DVertexBuffer9 *buf = (IDirect3DVertexBuffer9 *)Ptr->m_pVB;
    SAFE_RELEASE (buf);
    Ptr->m_pVB = NULL;
    Ptr->Unlink();
    Ptr = Next;
  }
#endif

#ifdef IB_VIDEO_POOLS
  Ptr = sIndexPools;
  while(Ptr)
  {
    TVertPool* Next = Ptr->Next;
    IDirect3DIndexBuffer9 *buf = (IDirect3DVertexBuffer9 *)Ptr->m_pVB;
    SAFE_RELEASE (buf);
    Ptr->m_pVB = NULL;
    Ptr->Unlink();
    Ptr = Next;
  }
#endif
}

//================================================================================================

void CD3D9Renderer::AllocIBInPool(int size, SVertexStream *pIB)
{
  CD3D9Renderer *rd = gcpRendD3D;

  int IBsize = CV_d3d9_ibpoolsize;

  TVertPool* Ptr;
  for(Ptr=sIndexPools; Ptr; Ptr=Ptr->Next)
  {
    if (AllocateChunk(size, Ptr, pIB, NULL))
      break;
  }
  if (!Ptr)
  {
    Ptr = new TVertPool;
    Ptr->m_nBufSize = IBsize;
    Ptr->m_pVB = NULL;
    Ptr->Next = sIndexPools;
    Ptr->PrevLink = &sIndexPools;
    if (sIndexPools)
    {
      assert(sIndexPools->PrevLink == &sIndexPools);
      sIndexPools->PrevLink = &Ptr->Next;
    }
    sIndexPools = Ptr;
    AllocateChunk(size, Ptr, pIB, NULL);
  }
  if (!Ptr->m_pVB)
  {
#if defined (DIRECT3D9) || defined(OPENGL)
    int Flags = D3DUSAGE_WRITEONLY;
#ifdef IB_VIDEO_POOLS
    D3DPOOL Pool = D3DPOOL_DEFAULT;
#else
    D3DPOOL Pool = D3DPOOL_MANAGED;
#endif
    HRESULT hr = rd->m_pd3dDevice->CreateIndexBuffer(IBsize, Flags, D3DFMT_INDEX16, Pool, (IDirect3DIndexBuffer9 **)&Ptr->m_pVB, NULL);
    assert(hr == S_OK);
#elif defined (DIRECT3D10)
    D3D10_BUFFER_DESC BufDesc;
    ZeroStruct(BufDesc);
    BufDesc.ByteWidth = IBsize;
    BufDesc.Usage = D3D10_USAGE_DEFAULT;
    BufDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
    BufDesc.CPUAccessFlags = 0;
    BufDesc.MiscFlags = 0; //D3D10_RESOURCE_MISC_COPY_DESTINATION;
    HRESULT hReturn = m_pd3dDevice->CreateBuffer(&BufDesc, NULL, (ID3D10Buffer **)&Ptr->m_pVB);
    assert(SUCCEEDED(hReturn));
#endif
  }
}

void CD3D9Renderer::CreateIndexBuffer(SVertexStream *dest,const void *src,int indexcount, int oldIndexCount)
{
  ReleaseIndexBuffer(dest, oldIndexCount);
  
  int size = indexcount*sizeof(ushort);
  m_CurIndexBufferSize += size;
#if defined (DIRECT3D9) || defined(OPENGL)
  IDirect3DIndexBuffer9 *ibuf=NULL;
#elif defined (DIRECT3D10)
  ID3D10Buffer *ibuf = NULL;
#endif
  if (size)
  {
    if (CV_d3d9_ibpools && size < CV_d3d9_ibpoolsize && !dest->m_bDynamic)
    {
      AllocIBInPool(size, dest);
#ifdef _DEBUG
      int nOffs;
  #if defined (DIRECT3D9) || defined(OPENGL)
      IDirect3DIndexBuffer9 *ibuf = (IDirect3DIndexBuffer9 *)dest->GetStream(&nOffs);
  #elif defined (DIRECT3D10)
      ID3D10Buffer *ibuf = (ID3D10Buffer *)dest->GetStream(&nOffs);
  #endif
      sAddVB(ibuf, dest, NULL, "Index buf", nOffs);
#endif
    }
    else
    {
  #if defined (DIRECT3D9) || defined(OPENGL)
      int flags = D3DUSAGE_WRITEONLY;
      D3DPOOL Pool = D3DPOOL_MANAGED;
      if (dest->m_bDynamic)
      {
        flags |= D3DUSAGE_DYNAMIC;
        Pool = D3DPOOL_DEFAULT;
      }
      HRESULT hReturn = m_pd3dDevice->CreateIndexBuffer(size, flags, D3DFMT_INDEX16, Pool, &ibuf, NULL);
      assert(hReturn == S_OK);
  #elif defined (DIRECT3D10)
      D3D10_BUFFER_DESC BufDesc;
      ZeroStruct(BufDesc);
      BufDesc.ByteWidth = size;
      BufDesc.Usage = dest->m_bDynamic ? D3D10_USAGE_DYNAMIC : D3D10_USAGE_DEFAULT;
      BufDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
      BufDesc.CPUAccessFlags = dest->m_bDynamic ? D3D10_CPU_ACCESS_WRITE : 0;
      BufDesc.MiscFlags = 0; //D3D10_RESOURCE_MISC_COPY_DESTINATION;
      HRESULT hReturn = m_pd3dDevice->CreateBuffer(&BufDesc, NULL, &ibuf);
      assert(SUCCEEDED(hReturn));
  #endif

  #ifdef _DEBUG
      sAddVB(ibuf, dest, NULL, "Index buf", 0);
  #endif

      if (FAILED(hReturn))
      {
        iLog->Log("Failed to create index buffer\n");
        return;
      }
      dest->m_VertBuf.m_pPtr = ibuf;
    }
  }
  if (src && indexcount)
    UpdateIndexBuffer(dest, src, indexcount, indexcount, true);
}

void CD3D9Renderer::UpdateIndexBuffer(SVertexStream *dest,const void *src,int indexcount, int oldIndexCount, bool bUnLock, bool bDynamic)
{
  PROFILE_FRAME(Mesh_UpdateIBuffers);

  if (m_bDeviceLost)
    return;

  HRESULT hReturn = S_OK;
  int size = indexcount*sizeof(ushort);
  if (src && indexcount)
  {
    /*if (bDynamic)
    {
      int nOffset;
      void *dst = GetIBPtr(indexcount, nOffset);

      dest->m_nBufOffset = nOffset;
      dest->m_VData = dst;
      cryMemcpy(dst, src, sizeof(short)*indexcount);
      UnlockIB();
      dest->m_VertBuf.m_pPtr = m_pIB;
      dest->m_bLocked = false;
      return;
    }*/
    if (oldIndexCount < indexcount || !dest->GetStream(NULL))
    {
      if (dest->GetStream(NULL))
        ReleaseIndexBuffer(dest, oldIndexCount);
      CreateIndexBuffer(dest, NULL, indexcount, oldIndexCount);
    }
    assert(indexcount != 0);
    ushort *dst = NULL;
    int nOffs;
#if defined (DIRECT3D9) || defined(OPENGL)
    IDirect3DIndexBuffer9 *ibuf = (IDirect3DIndexBuffer9 *)dest->GetStream(&nOffs);
    {
      PROFILE_FRAME(Mesh_UpdateIBuffersLock);
#if !defined(XENON) && !defined(PS3)
      hReturn = ibuf->Lock(nOffs, size, (void **)&dst, dest->m_bDynamic ? D3DLOCK_DISCARD : 0);
#else
      if (ibuf == m_RP.m_pIndexStream)
      {
        m_RP.m_pIndexStream = NULL;
        m_pd3dDevice->SetIndices(NULL);
      }
      hReturn = ibuf->Lock(0, 0, (void **)&dst, dest->m_bDynamic ? D3DLOCK_DISCARD : 0);
#endif
      assert(hReturn == S_OK);
    }
#ifdef _DEBUG
    D3DINDEXBUFFER_DESC Desc;
    ibuf->GetDesc(&Desc);
    assert(Desc.Size >= size+nOffs);
#endif
    {
      PROFILE_FRAME(Mesh_UpdateIBuffersCopy);
      memcpy(dst, src, size);
    }
#elif defined (DIRECT3D10)
    // Get temporary index buffer
    if (indexcount+m_pIBTemp[m_nCurStagedIB]->GetOffset() > m_pIBTemp[m_nCurStagedIB]->GetCount())
    {
      m_nCurStagedIB++;
		  if (m_nCurStagedIB > CV_d3d10_NumStagingBuffers-1)
			  m_nCurStagedIB = 0;
    }
    int nBuf = m_nCurStagedIB;
    DynamicIB<ushort> *pTempBuf = m_pIBTemp[nBuf];
    bool bTemp = false;
    if (indexcount > m_pIBTemp[nBuf]->GetCount())
    {
      bTemp = true;
      pTempBuf = new DynamicIB <ushort>(m_pd3dDevice, indexcount);
    }

    ID3D10Buffer *ibuf = (ID3D10Buffer *)dest->GetStream(&nOffs);
    uint nOffsTemp = 0;
    if (ibuf)
    {
      dst = pTempBuf->Lock(indexcount, nOffsTemp);
    }
    {
      PROFILE_FRAME(Mesh_UpdateIBuffersCopy);
      cryMemcpy(dst, src, size);
    }
#endif
    dest->m_VData = dst;

    if (bUnLock)
    {
#if defined (DIRECT3D9) || defined(OPENGL)
      hReturn = ibuf->Unlock();
#elif defined (DIRECT3D10)
      {
        pTempBuf->Unlock();
        D3D10_BOX box;
        ZeroStruct(box);
        box.left = nOffsTemp*sizeof(ushort);
        box.right = size+box.left;
        box.bottom = 1;
        box.back = 1;
        m_pd3dDevice->CopySubresourceRegion(ibuf, 0, nOffs, 0, 0, pTempBuf->GetInterface(), 0, &box);
        if (bTemp)
        {
          SAFE_DELETE(pTempBuf);
        }
      }

#endif
      dest->m_bLocked = false;
    }
    else
      dest->m_bLocked = true;
  }
  else
  if (dest->m_VertBuf.m_pPtr)
  {
    if (bUnLock && dest->m_bLocked)
    {
#if defined (DIRECT3D9) || defined(OPENGL)
      IDirect3DIndexBuffer9 *ibuf = (IDirect3DIndexBuffer9 *)dest->m_VertBuf.m_pPtr;
      hReturn = ibuf->Unlock();
#endif
      dest->m_bLocked = false;
    }
    else
    if (!bUnLock && !dest->m_bLocked)
    {
      PROFILE_FRAME(Mesh_UpdateIBuffersLock);
      ushort *dst = NULL;
#if defined (DIRECT3D9) || defined(OPENGL)
      int nOffs;
      IDirect3DIndexBuffer9 *ibuf = (IDirect3DIndexBuffer9 *)dest->GetStream(&nOffs);
      hReturn = ibuf->Lock(nOffs, size, (void **)&dst, dest->m_bDynamic ? D3DLOCK_DISCARD : 0);
#endif
      dest->m_bLocked = true;
      dest->m_VData = dst;
    }
  }
}
void CD3D9Renderer::ReleaseIndexBuffer(SVertexStream *dest, int nIndices)
{
  if (dest)
  {
    m_CurIndexBufferSize -= nIndices * sizeof(short);

    if (dest->m_pPool)
    {
      TVertPool *pPool = (TVertPool *)dest->m_pPool;
#ifdef _DEBUG
      int nOffs = 0;
  #if defined (DIRECT3D9) || defined(OPENGL)
      IDirect3DIndexBuffer9 *ibuf = (IDirect3DIndexBuffer9 *)dest->GetStream(&nOffs);
  #elif defined (DIRECT3D10)
      ID3D10Buffer *ibuf = (ID3D10Buffer *)dest->GetStream(&nOffs);
  #endif
      sRemoveVB(ibuf, dest, nOffs);
#endif
      if (ReleaseVBChunk(pPool, dest))
      {
#if defined (DIRECT3D9) || defined(OPENGL)
        IDirect3DIndexBuffer9 *itemp = (IDirect3DIndexBuffer9 *)pPool->m_pVB;
#elif defined (DIRECT3D10)
        ID3D10Buffer *itemp = (ID3D10Buffer *)pPool->m_pVB;
#endif
        if (itemp == m_RP.m_pIndexStream)
          m_RP.m_pIndexStream = NULL;
        SAFE_RELEASE(itemp);
        pPool->Unlink();
        delete pPool;
      }
    }
    else
    {
#if defined (DIRECT3D9) || defined(OPENGL)
      IDirect3DIndexBuffer9 *itemp = (IDirect3DIndexBuffer9 *)dest->m_VertBuf.m_pPtr;
      if (itemp && itemp == m_RP.m_pIndexStream)
      {
        m_pd3dDevice->SetIndices(NULL);
        m_RP.m_pIndexStream = NULL;
      }
#elif defined (DIRECT3D10)
      ID3D10Buffer *itemp = (ID3D10Buffer *)dest->m_VertBuf.m_pPtr;
#endif

#ifdef _DEBUG
      if (itemp)
        sRemoveVB(itemp, dest, 0);
#endif
      if (!dest->m_bDynamic)
      {
        SAFE_RELEASE(itemp);
      }
      dest->m_VertBuf.m_pPtr = NULL;
    }
  }
  dest->Reset();
  dest->m_nBufOffset = 0;
  dest->m_pPool = NULL;
}

//=======================================================================================

void CD3D9Renderer::AllocVBInPool(int size, int nVFormat, SVertexStream *pVB)
{
  CD3D9Renderer *rd = gcpRendD3D;

  assert(nVFormat>=0 && nVFormat<VERTEX_FORMAT_NUMS);

  int VBsize = CV_d3d9_vbpoolsize;

  TVertPool* Ptr;
  for(Ptr=sVertPools; Ptr; Ptr=Ptr->Next)
  {
    if (AllocateChunk(size, Ptr, pVB, NULL))
      break;
  }
  if (!Ptr)
  {
    Ptr = new TVertPool;
    Ptr->m_nBufSize = VBsize;
    Ptr->m_pVB = NULL;
    Ptr->Next = sVertPools;
    Ptr->PrevLink = &sVertPools;
    if (sVertPools)
    {
      assert(sVertPools->PrevLink == &sVertPools);
      sVertPools->PrevLink = &Ptr->Next;
    }
    sVertPools = Ptr;
    AllocateChunk(size, Ptr, pVB, NULL);
  }
  if (!Ptr->m_pVB)
  {
#if defined (DIRECT3D9) || defined(OPENGL)
    int Flags = D3DUSAGE_WRITEONLY;
#ifdef VB_VIDEO_POOLS
    D3DPOOL Pool = D3DPOOL_DEFAULT;
#else
    D3DPOOL Pool = D3DPOOL_MANAGED;
#endif
    int fvf = 0;
    HRESULT hr = rd->m_pd3dDevice->CreateVertexBuffer(VBsize, Flags, fvf, Pool, (IDirect3DVertexBuffer9 **)&Ptr->m_pVB, NULL);
    assert(hr == S_OK);
#elif defined (DIRECT3D10)
    D3D10_BUFFER_DESC BufDesc;
    ZeroStruct(BufDesc);
    BufDesc.ByteWidth = VBsize;
    BufDesc.Usage = D3D10_USAGE_DEFAULT;
    BufDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    BufDesc.CPUAccessFlags = 0;
    BufDesc.MiscFlags = 0; //D3D10_RESOURCE_MISC_COPY_DESTINATION;
    HRESULT hReturn = m_pd3dDevice->CreateBuffer(&BufDesc, NULL, (ID3D10Buffer **)&Ptr->m_pVB);
    assert(SUCCEEDED(hReturn));
#endif
  }
}

void CD3D9Renderer::CreateBuffer(int size, int vertexformat, CVertexBuffer *buf, int nType, const char *szSource, bool bDynamic)
{
  PROFILE_FRAME(Mesh_CreateVBuffers);

#if defined (DIRECT3D9) || defined(OPENGL)
  IDirect3DVertexBuffer9 *vptr = NULL;
#elif defined (DIRECT3D10)
  ID3D10Buffer *vptr = NULL;
#endif

  if (CV_d3d9_vbpools && size < CV_d3d9_vbpoolsize && !bDynamic)
  {
    m_CurVertBufferSize += size;
    AllocVBInPool(size, vertexformat, &buf->m_VS[nType]);

#ifdef _DEBUG
    int nOffs = 0;
  #if defined (DIRECT3D9) || defined(OPENGL)
    IDirect3DVertexBuffer9 *vbuf = (IDirect3DVertexBuffer9 *)buf->GetStream(nType, &nOffs);
  #elif defined (DIRECT3D10)
    ID3D10Buffer *vbuf = (ID3D10Buffer *)buf->GetStream(nType, &nOffs);
  #endif
    char *szDesc = szDescBuf[nType];
    if (szSource && szSource[0])
    {
      char s[256];
      sprintf(s, "%s (%s)", szSource, szDesc);
      szDesc = s;
    }
    sAddVB(vbuf, &buf->m_VS[nType], buf, szDesc, nOffs);
#endif
    return;
  }

  void *dst = NULL;
  buf->m_VS[nType].m_bDynamic = bDynamic;
  if (bDynamic)
  {
    //assert(0);
    return;
  }

#if defined (DIRECT3D9) || defined(OPENGL)
  int fvf = 0;
  int Flags = D3DUSAGE_WRITEONLY;
  D3DPOOL Pool = D3DPOOL_MANAGED;
  HRESULT hReturn = m_pd3dDevice->CreateVertexBuffer(size, Flags, fvf, Pool, &vptr, NULL);
  assert(hReturn == S_OK);
#elif defined (DIRECT3D10)
  D3D10_BUFFER_DESC BufDesc;
  ZeroStruct(BufDesc);
  BufDesc.ByteWidth = size;
  BufDesc.Usage = D3D10_USAGE_DEFAULT;
  BufDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
  BufDesc.CPUAccessFlags = 0;
  BufDesc.MiscFlags = 0; //D3D10_RESOURCE_MISC_COPY_DESTINATION;
  HRESULT hReturn = m_pd3dDevice->CreateBuffer(&BufDesc, NULL, &vptr);
  assert(SUCCEEDED(hReturn));
#endif

  if (FAILED(hReturn))
    return;

#ifdef _DEBUG
  char *szDesc = szDescBuf[nType];
  if (szSource && szSource[0])
  {
    char s[256];
    sprintf(s, "%s (%s)", szSource, szDesc);
    szDesc = s;
  }
  sAddVB(vptr, &buf->m_VS[nType], buf, szDesc, 0);
#endif

  buf->m_VS[nType].m_VertBuf.m_pPtr = vptr;
#if defined (DIRECT3D9) || defined(OPENGL)
  hReturn = vptr->Lock(0, 0, (void **) &dst, 0);
  assert(hReturn == S_OK);
  buf->m_VS[nType].m_VData = dst;
  hReturn = vptr->Unlock();
#endif
  m_CurVertBufferSize += size;
}

CVertexBuffer *CD3D9Renderer::CreateBuffer(int vertexcount,int vertexformat, const char *szSource, bool bDynamic)
{
  PROFILE_FRAME(Mesh_CreateVBuffers);

  CVertexBuffer *newbuf = new CVertexBuffer;
  newbuf->m_VS[VSF_GENERAL].m_bDynamic = bDynamic;
  newbuf->m_VS[VSF_GENERAL].m_bLocked = false;
  newbuf->m_nVertexFormat = vertexformat;

  if (bDynamic)
  {
    return newbuf;
  }

  int size = m_VertexSize[vertexformat]*vertexcount;
  m_CurVertBufferSize += size;
  if (size+m_CurVertBufferSize+m_CurIndexBufferSize > m_MaxVertBufferSize)
  {
    CRenderMesh *pRM = CRenderMesh::m_Root.m_Prev;
    while (size+m_CurVertBufferSize+m_CurIndexBufferSize > m_MaxVertBufferSize)
    {
      if (pRM == &CRenderMesh::m_Root)
        iConsole->Exit("Error: Pipeline buffer overflow. Current geometry cannot fit in video memory (%s)", gRenDev->GetStatusText(eRS_VidBuffer));

      CRenderMesh *Next = pRM->m_Prev;
      pRM->Unload(true);
      pRM = Next;
    }
  }

#if defined (DIRECT3D9) || defined(OPENGL)
  IDirect3DVertexBuffer9 *vptr = NULL;
#elif defined (DIRECT3D10)
  ID3D10Buffer *vptr = NULL;
#endif
  if (CV_d3d9_vbpools && size < CV_d3d9_vbpoolsize && !bDynamic)
  {
    AllocVBInPool(size, vertexformat, &newbuf->m_VS[VSF_GENERAL]);
#ifdef _DEBUG
    char *szDesc = szDescBuf[VSF_GENERAL];
    if (szSource && szSource[0])
    {
      char s[256];
      sprintf(s, "%s (%s)", szSource, szDesc);
      szDesc = s;
    }
    int nOffs = 0;
  #if defined (DIRECT3D9) || defined(OPENGL)
    vptr = (IDirect3DVertexBuffer9 *)newbuf->GetStream(VSF_GENERAL, &nOffs);
  #elif defined (DIRECT3D10)
    vptr = (ID3D10Buffer *)newbuf->GetStream(VSF_GENERAL, NULL);
  #endif
    sAddVB(vptr, &newbuf->m_VS[VSF_GENERAL], newbuf, szDesc, nOffs);
#endif
    return newbuf;
  }

#if defined (DIRECT3D9) || defined(OPENGL)
  int fvf = 0;
  int Flags = D3DUSAGE_WRITEONLY;
  D3DPOOL Pool = D3DPOOL_MANAGED;
  HRESULT hReturn = m_pd3dDevice->CreateVertexBuffer(size, Flags, fvf, Pool, &vptr, NULL);
#elif defined (DIRECT3D10)
  D3D10_BUFFER_DESC BufDesc;
  BufDesc.ByteWidth = size;
  BufDesc.Usage = D3D10_USAGE_DEFAULT;
  BufDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
  BufDesc.CPUAccessFlags = 0;
  BufDesc.MiscFlags = 0; //D3D10_RESOURCE_MISC_COPY_DESTINATION;
  HRESULT hReturn = m_pd3dDevice->CreateBuffer(&BufDesc, NULL, &vptr);
#endif
  if (FAILED(hReturn))
    return (NULL);
  newbuf->m_VS[VSF_GENERAL].m_VertBuf.m_pPtr = vptr;

#ifdef _DEBUG
  char *szDesc = szDescBuf[VSF_GENERAL];
  if (szSource && szSource[0])
  {
    char s[256];
    sprintf(s, "%s (%s)", szSource, szDesc);
    szDesc = s;
  }
  sAddVB(vptr, &newbuf->m_VS[VSF_GENERAL], newbuf, szDesc, 0);
#endif

  return(newbuf);
}

///////////////////////////////////////////
void CD3D9Renderer::UpdateBuffer(CVertexBuffer *dest, const void *src, int vertexcount, bool bUnLock, int offs, int nType)
{
  PROFILE_FRAME(Mesh_UpdateVBuffers);

  if (m_bDeviceLost)
    return;

  VOID *pVertices = NULL;

  HRESULT hr = 0;
#if defined (DIRECT3D9) || defined(OPENGL)
  IDirect3DVertexBuffer9 *tvert;
#elif defined (DIRECT3D10)
  ID3D10Buffer *tvert;
#endif
  int nOffs = 0;
  int size;
  if (nType == VSF_GENERAL)
    size = m_VertexSize[dest->m_nVertexFormat];
  else
    size = m_StreamSize[nType];

  if (!src)
  {
    tvert = (D3DVertexBuffer *)dest->GetStream(nType, &nOffs);
    if (tvert)
    {
      if (bUnLock)
      {
        // video buffer update
        if (dest->m_VS[nType].m_bLocked)
        {
          dest->m_VS[nType].m_bLocked = false;
  #if defined (DIRECT3D9) || defined(OPENGL)
          hr = tvert->Unlock();
  #elif defined (DIRECT3D10)
          UnlockBuffer(dest, nType, vertexcount);
  #endif
        }
      }
      else
      {
        // video buffer update
        if (!dest->m_VS[nType].m_bLocked)
        {
          PROFILE_FRAME(Mesh_UpdateVBuffersLock);
          dest->m_VS[nType].m_bLocked = true;
  #if defined (DIRECT3D9) || defined(OPENGL)
          hr = tvert->Lock(dest->m_VS[nType].m_nBufOffset, size*vertexcount, (void **) &pVertices, dest->m_VS[nType].m_bDynamic ? D3DLOCK_DISCARD : 0);
          assert(hr == S_OK);
  #elif defined (DIRECT3D10)
          if (m_pVBTemp[m_nCurStagedVB]->GetBytesOffset()+size*vertexcount > m_pVBTemp[m_nCurStagedVB]->GetBytesCount())
          {
            m_nCurStagedVB++;
            if (m_nCurStagedVB >= CV_d3d10_NumStagingBuffers)
              m_nCurStagedVB = 0;
          }
          uint nOffs;
          pVertices = m_pVBTemp[m_nCurStagedVB]->Lock(size*vertexcount, nOffs);
          m_StagedStream[nType] = m_nCurStagedVB;
          m_StagedStreamOffset[nType] = nOffs;
  #endif
          dest->m_VS[nType].m_VData = (byte *)pVertices;
        }
      }
    }
    return;
  }

  // If dyn. buffer get new pointer for it
  if (dest->m_VS[nType].m_bDynamic)
  {
    uint nOffset;
    void *dst = FX_LockVB(size*vertexcount, nOffset);

    dest->m_VS[nType].m_VertBuf.m_pPtr = m_RP.m_VBs[m_RP.m_CurVB].VBPtr_0->m_pVB;
    dest->m_VS[nType].m_VData = dst;
    dest->m_VS[nType].m_nBufOffset = nOffset;
    byte *pDataSrc = (byte *)src;
    memcpy(dst, &pDataSrc[offs*size], size*vertexcount);
    FX_UnlockVB();
    dest->m_VS[nType].m_bLocked = false;
    m_RP.m_PS.m_MeshUpdateBytes += size*vertexcount;
    return;
  }

  if (dest->m_VS[nType].m_pPool)
    tvert = (D3DVertexBuffer *)dest->m_VS[nType].m_pPool->m_pVB;
  else
    tvert = (D3DVertexBuffer *)dest->m_VS[nType].m_VertBuf.m_pPtr;

  if (!tvert)  // system buffer update
  {
    PROFILE_FRAME(Mesh_UpdateVBuffersCopy);
    if (dest->m_VS[nType].m_VData)
      cryMemcpy(dest->m_VS[nType].m_VData, src, size*vertexcount);
    return;
  }

#if defined (DIRECT3D10)
  if (m_pVBTemp[m_nCurStagedVB]->GetBytesOffset()+size*vertexcount > m_pVBTemp[m_nCurStagedVB]->GetBytesCount())
  {
    m_nCurStagedVB++;
    if (m_nCurStagedVB >= CV_d3d10_NumStagingBuffers)
      m_nCurStagedVB = 0;
  }
  int nBuf = m_nCurStagedVB;
  m_StagedStream[nType] = nBuf;
  DynamicVB<byte> *pBufTemp = m_pVBTemp[nBuf];
  bool bTemp = false;
  if (size*vertexcount > CV_d3d9_vbpoolsize)
  {
    bTemp = true;
    pBufTemp = new DynamicVB<byte>(m_pd3dDevice, 0, size*vertexcount);
  }
  uint nOffsTemp = 0;
#endif

  // video buffer update
  if (!dest->m_VS[nType].m_bLocked)
  {
    PROFILE_FRAME(Mesh_UpdateVBuffersLock);
    dest->m_VS[nType].m_bLocked = true;
#if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
    hr = tvert->Lock(dest->m_VS[nType].m_nBufOffset, size*vertexcount, (void **) &pVertices, dest->m_VS[nType].m_bDynamic ? D3DLOCK_DISCARD : 0);
#else
    hr = tvert->Lock(dest->m_VS[nType].m_nBufOffset, 0, (void **) &pVertices, dest->m_VS[nType].m_bDynamic ? D3DLOCK_DISCARD : 0);
#endif
#elif defined (DIRECT3D10)
    pVertices = pBufTemp->Lock(size*vertexcount, nOffsTemp);
    m_StagedStreamOffset[nType] = nOffsTemp;
#endif
    assert(SUCCEEDED(hr));
    dest->m_VS[nType].m_VData = pVertices;
  }
  else
  {
    int nnn = 0;
  }

  if (SUCCEEDED(hr) && src)
  {
    PROFILE_FRAME(Mesh_UpdateVBuffersCopy);
#ifdef _DEBUG
 #if defined (DIRECT3D9) || defined(OPENGL)
    D3DVERTEXBUFFER_DESC Desc;
    tvert->GetDesc(&Desc);
    assert(Desc.Size >= dest->m_VS[nType].m_nBufOffset+size*vertexcount);
 #endif
#endif
    memcpy(dest->m_VS[nType].m_VData, src, size*vertexcount);
#if defined (DIRECT3D9) || defined(OPENGL)
    tvert->Unlock();
#elif defined (DIRECT3D10)
    pBufTemp->Unlock();
    D3D10_BOX box;
    ZeroStruct(box);
    box.left = nOffsTemp;
    box.right = size*vertexcount+nOffsTemp;
    box.bottom = 1;
    box.back = 1;
    m_pd3dDevice->CopySubresourceRegion(tvert, 0, dest->m_VS[nType].m_nBufOffset, 0, 0, pBufTemp->m_pVB, 0, &box);
    if (bTemp)
    {
      SAFE_DELETE(pBufTemp);
    }
#endif
    dest->m_VS[nType].m_bLocked = false;
    m_RP.m_PS.m_MeshUpdateBytes += size*vertexcount;
  }
  else
  {
    if (dest->m_VS[nType].m_bLocked && bUnLock)
    {
  #if defined (DIRECT3D9) || defined(OPENGL)
      tvert->Unlock();
  #elif defined (DIRECT3D10)
      pBufTemp->Unlock();
  #endif
      dest->m_VS[nType].m_bLocked = false;
    }
  }
}

void CD3D9Renderer::UnlockBuffer(CVertexBuffer *buf, int nType, int nVerts)
{
  if (!buf->m_VS[nType].m_bLocked)
    return;
  if (m_bDeviceLost)
    return;

  int size;
  if (nType == VSF_GENERAL)
    size = m_VertexSize[buf->m_nVertexFormat];
  else
    size = m_StreamSize[nType];
  
#if defined (DIRECT3D9) || defined(OPENGL)
  IDirect3DVertexBuffer9 *tvert = (IDirect3DVertexBuffer9 *)buf->GetStream(nType, NULL);
  HRESULT hr = tvert->Unlock();
#elif defined (DIRECT3D10)
	int nBuf = m_StagedStream[nType];
  uint nOffs = m_StagedStreamOffset[nType];
  ID3D10Buffer *tvert = (ID3D10Buffer*)buf->GetStream(nType, NULL);
  m_pVBTemp[nBuf]->Unlock();
  D3D10_BOX box;
  ZeroStruct(box);
  box.left = nOffs;
  box.right = size*nVerts+nOffs;
  box.bottom = 1;
  box.back = 1;
  m_pd3dDevice->CopySubresourceRegion(tvert, 0, buf->m_VS[nType].m_nBufOffset, 0, 0, m_pVBTemp[nBuf]->m_pVB, 0, &box);
#endif
  buf->m_VS[nType].m_bLocked = false;
}


///////////////////////////////////////////

void sReleaseVBStream(CVertexBuffer *bufptr, int nStream, int nVerts)
{
  SVertexStream *pStream = &bufptr->m_VS[nStream];
  int nOffs;
  CD3D9Renderer *rd = gcpRendD3D;
  if (bufptr->m_VS[nStream].m_bLocked)
    rd->UnlockBuffer(bufptr, nStream, nVerts);
  if (pStream->m_pPool)
  {
    TVertPool *pPool = (TVertPool *)pStream->m_pPool;
#if defined (DIRECT3D9) || defined(OPENGL)
    IDirect3DVertexBuffer9 *vptr = (IDirect3DVertexBuffer9 *)bufptr->GetStream(nStream, &nOffs);
#elif defined (DIRECT3D10)
    ID3D10Buffer *vptr = (ID3D10Buffer *)bufptr->GetStream(nStream, &nOffs);
#endif
#ifdef _DEBUG
    sRemoveVB(vptr, &bufptr->m_VS[nStream], nOffs);
#endif
    if (vptr == rd->m_RP.m_VertexStreams[0].pStream && nOffs == rd->m_RP.m_VertexStreams[0].nOffset)
      rd->m_RP.m_VertexStreams[0].pStream = NULL;
    if (rd->ReleaseVBChunk(pPool, pStream))
    {
#if defined (DIRECT3D9) || defined(OPENGL)
      IDirect3DVertexBuffer9 *vtemp = (IDirect3DVertexBuffer9 *)pPool->m_pVB;
      if (vtemp == rd->m_RP.m_VertexStreams[nStream].pStream)
      {
        rd->m_RP.m_VertexStreams[nStream].pStream = NULL;
        rd->m_pd3dDevice->SetStreamSource(nStream, NULL, 0, 0);
      }
#elif defined (DIRECT3D10)
      ID3D10Buffer *vtemp = (ID3D10Buffer *)pPool->m_pVB;
#endif
      SAFE_RELEASE(vtemp);
      pPool->m_pVB = NULL;
      pPool->Unlink();
      delete pPool;
    }
  }
  else
  {
#if defined (DIRECT3D9) || defined(OPENGL)
    IDirect3DVertexBuffer9 *vtemp = (IDirect3DVertexBuffer9 *)pStream->m_VertBuf.m_pPtr;
    if (vtemp == rd->m_RP.m_VertexStreams[nStream].pStream)
    {
      rd->m_RP.m_VertexStreams[nStream].pStream = NULL;
      rd->m_pd3dDevice->SetStreamSource(nStream, NULL, 0, 0);
    }
#elif defined (DIRECT3D10)
    ID3D10Buffer *vtemp = (ID3D10Buffer *)pStream->m_VertBuf.m_pPtr;
#endif

#ifdef _DEBUG
    if (vtemp)
      sRemoveVB(vtemp, pStream, 0);
#endif
    if (!pStream->m_bDynamic)
    {
      SAFE_RELEASE(vtemp);
    }
    pStream->m_VertBuf.m_pPtr = NULL;
  }
}

void CD3D9Renderer::ReleaseBuffer(CVertexBuffer *bufptr, int nVerts)
{
  int i;
  if (bufptr)
  {
    m_CurVertBufferSize -= m_VertexSize[bufptr->m_nVertexFormat]*nVerts;
    for (i=1; i<VSF_NUM; i++)
    {
      if ((bufptr->m_VS[i].m_VertBuf.m_pPtr || bufptr->m_VS[i].m_pPool) && !bufptr->m_VS[i].m_bDynamic)
        m_CurVertBufferSize -= m_StreamSize[i] * nVerts;
    }

    for (i=0; i<VSF_NUM; i++)
    {
      sReleaseVBStream(bufptr, i, nVerts);
    }

    delete bufptr;
  }
}

///////////////////////////////////////////
void CD3D9Renderer::DrawBuffer(CVertexBuffer *src,SVertexStream *indicies,int numindices,int offsindex,int prmode,int vert_start,int vert_stop, CRenderChunk *pChunk)
{
  if (m_bDeviceLost)
    return;

  if (CV_d3d9_forcesoftware)
    return;

  if (!m_SceneRecurseCount)
  {
    iLog->Log("ERROR: CD3D9Renderer::DrawBuffer before BeginScene");
    return;
  }

  int nIBOffsets = 0;
#if defined (DIRECT3D9) || defined(OPENGL)  
  IDirect3DIndexBuffer9 *pIB = (IDirect3DIndexBuffer9 *)indicies->GetStream( &nIBOffsets );
#else
  ID3D10Buffer *pIB = (ID3D10Buffer *)indicies->m_VertBuf.m_pPtr;
#endif

  if (!pIB || !src)
    return;

  PROFILE_FRAME(Draw_IndexMesh);

  int size = numindices * sizeof(short);

  if (src->m_VS[VSF_GENERAL].m_bLocked)
  {
#if defined (DIRECT3D9) || defined(OPENGL)
    IDirect3DVertexBuffer9 *tvert =  (IDirect3DVertexBuffer9 *)src->m_VS[VSF_GENERAL].m_VertBuf.m_pPtr;
    tvert->Unlock();
#elif defined (DIRECT3D10)
    ID3D10Buffer *tvert =  (ID3D10Buffer *)src->m_VS[VSF_GENERAL].m_VertBuf.m_pPtr;
#endif
    src->m_VS[VSF_GENERAL].m_bLocked = false;
  }
  HRESULT h = EF_SetVertexDeclaration(0, src->m_nVertexFormat);
  if (!FAILED(h))
  {
    int nOffs;
  #if defined (DIRECT3D9) || defined(OPENGL)
    IDirect3DIndexBuffer9 *ibuf = pIB;
    IDirect3DVertexBuffer9 *pBuf = (IDirect3DVertexBuffer9 *)src->GetStream(VSF_GENERAL, &nOffs);
    h = FX_SetVStream(0, pBuf, nOffs, m_VertexSize[src->m_nVertexFormat]);
    h = FX_SetIStream(ibuf);
  #elif defined (DIRECT3D10)
    ID3D10Buffer *ibuf = pIB;
    ID3D10Buffer *pBuf = (ID3D10Buffer *)src->GetStream(VSF_GENERAL, &nOffs);
    h = FX_SetVStream(0, pBuf, nOffs, m_VertexSize[src->m_nVertexFormat]);
    h = FX_SetIStream(ibuf);
  #endif
    EF_Commit();

    int NumVerts = 0;
    if (vert_stop)
      NumVerts = vert_stop;
    else
    {
      assert(0);
    }


  #if defined (DIRECT3D9) || defined(OPENGL)
    switch(prmode)
    {
	  case R_PRIMV_LINES:
		  h = m_pd3dDevice->DrawIndexedPrimitive(D3DPT_LINELIST,0,0,NumVerts,offsindex,numindices/2);
		  //m_nPolygons+=numindices/2;
		  break;

	  case R_PRIMV_TRIANGLES:
        h = m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,NumVerts,offsindex,numindices/3);
        m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL] += numindices/3;
        m_RP.m_PS.m_nDIPs[EFSLIST_GENERAL]++;
        break;

      case R_PRIMV_TRIANGLE_STRIP:
        h = m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,0,0,NumVerts,offsindex,numindices-2);
        m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL] += numindices-2;
        m_RP.m_PS.m_nDIPs[EFSLIST_GENERAL]++;
        break;
    }
  #elif defined (DIRECT3D10)
    switch(prmode)
    {
    case R_PRIMV_LINES:
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
      m_pd3dDevice->DrawIndexed(numindices, offsindex, 0);
      //m_nPolygons+=numindices/2;
      break;

    case R_PRIMV_TRIANGLES:
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      m_pd3dDevice->DrawIndexed(numindices, offsindex, 0);
      m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL]+=numindices/3;
      break;

    case R_PRIMV_TRIANGLE_STRIP:
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      m_pd3dDevice->DrawIndexed(numindices, offsindex, 0);
      m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL]+=numindices-2;
      break;
    }
  #endif
  }
}

