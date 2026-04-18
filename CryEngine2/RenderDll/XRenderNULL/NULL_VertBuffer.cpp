//////////////////////////////////////////////////////////////////////
//
//  Crytek CryENGINE Source code
//  
//  File: PS2_VertBuffer.cpp
//  Description: Implementation of the vertex buffer management
//
//  History:
//  -Jan 31,2001:Created by Vladimir Kajalin
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "NULL_Renderer.h"

void *CNULLRenderer::GetDynVBPtr(int nVerts, int &nOffs, int Pool)
{
  return m_DynVB;
}

void CNULLRenderer::DrawDynVB(int nOffs, int Pool, int nVerts)
{
}

void CNULLRenderer::DrawDynVB(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pBuf, ushort *pInds, int nVerts, int nInds, int nPrimType)
{
}

// allocates vertex buffer
void CNULLRenderer::CreateBuffer(int size, int vertexformat, CVertexBuffer *buf, int Type, const char *szSource, bool bDynamic)
{
//  assert(Type >= 0 && Type <= 3);

  void *data;

  // System buffer 
  data = new unsigned char [size];
  buf->m_VS[Type].m_VData = data;
}

CVertexBuffer *CNULLRenderer::CreateBuffer(int vertexcount,int vertexformat, const char *szSource, bool bDynamic)
{
  CVertexBuffer * vtemp = new CVertexBuffer;

  vtemp->m_VS[VSF_GENERAL].m_bDynamic = bDynamic;

  int size = m_VertexSize[vertexformat]*vertexcount;
  // System buffer 
  vtemp->m_VS[VSF_GENERAL].m_VData = new unsigned char [size];

  vtemp->m_nVertexFormat = vertexformat;

  return (vtemp);
}


///////////////////////////////////////////
void CNULLRenderer::DrawBuffer(CVertexBuffer *src,SVertexStream *indicies,int numindices, int offsindex, int prmode, int vert_start, int vert_stop, CRenderChunk *pChunk)
{
}


///////////////////////////////////////////
// Updates the vertex buffer dest with the data from src
// NOTE: src may be NULL, in which case the data will not be copied
void CNULLRenderer::UpdateBuffer(CVertexBuffer *dest,const void *src,int vertexcount, bool bUnlock, int offs, int Type)
{
//  assert (Type >= 0 && Type <= 2);

  // NOTE: some subsystems need to initialize the system buffer without actually intializing its values;
	// for that purpose, src may sometimes be NULL
  if(src && vertexcount)
  {
    byte *dst = (byte *)dest->m_VS[Type].m_VData;
    if (dst)
    {
      if (Type == VSF_GENERAL)
        memcpy(&dst[m_VertexSize[dest->m_nVertexFormat]*offs],src,m_VertexSize[dest->m_nVertexFormat]*vertexcount);
      else
      if (Type == VSF_TANGENTS)
        memcpy(&dst[sizeof(SPipTangents)*offs],src,sizeof(SPipTangents)*vertexcount);
    }
  }
}

///////////////////////////////////////////
void CNULLRenderer::ReleaseBuffer(CVertexBuffer *bufptr, int nVerts)
{
  if (bufptr)
  {
    delete [] (byte *)bufptr->m_VS[VSF_GENERAL].m_VData;
    bufptr->m_VS[VSF_GENERAL].m_VData = NULL;
    delete [] (byte *)bufptr->m_VS[VSF_TANGENTS].m_VData;
    bufptr->m_VS[VSF_TANGENTS].m_VData = NULL;

    delete bufptr;
  } 
}

///////////////////////////////////////////
void CNULLRenderer::DrawTriStrip(CVertexBuffer *src, int vert_num)
{ 
}


void CNULLRenderer::CreateIndexBuffer(SVertexStream *dest,const void *src,int indexcount, int oldIndexCount)
{
  delete [] (byte *)dest->m_VData;
  dest->m_VData = NULL;
  if (indexcount)
  {
    dest->m_VData = new ushort[indexcount];
  }
  if (src && indexcount)
  {
    cryMemcpy(dest->m_VData, src, indexcount*sizeof(ushort));
    m_RP.m_PS.m_MeshUpdateBytes += indexcount*sizeof(ushort);
  }
}
void CNULLRenderer::UpdateIndexBuffer(SVertexStream *dest,const void *src,int indexcount, int oldIndexCount, bool bUnLock, bool bDynamic)
{
  PROFILE_FRAME(Mesh_UpdateIBuffers);
  if (src && indexcount)
  {
    if (oldIndexCount < indexcount)
    {
      delete [] (byte *)dest->m_VData;
      dest->m_VData = new ushort[indexcount];
    }
    cryMemcpy(dest->m_VData, src, indexcount*sizeof(ushort));
    m_RP.m_PS.m_MeshUpdateBytes += indexcount*sizeof(ushort);
  }
}
void CNULLRenderer::ReleaseIndexBuffer(SVertexStream *dest, int nIndices)
{
  delete [] (byte *)dest->m_VData;
  dest->m_VData = NULL;
  dest->Reset();
}

void CRenderMesh::DrawImmediately()
{
}
