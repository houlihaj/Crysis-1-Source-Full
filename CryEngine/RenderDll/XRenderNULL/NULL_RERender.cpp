/*=============================================================================
  PS2_RERender.cpp : implementation of the Rendering RenderElements pipeline.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"
#include "NULL_Renderer.h"
#include "I3DEngine.h"

//#include "../cry3dengine/StatObj.h"

#undef THIS_FILE
static char THIS_FILE[] = __FILE__;

//=======================================================================

bool CRESky::mfDraw(CShader *ef, SShaderPass *sfm)
{
  return true;
}

bool CREHDRSky::mfDraw( CShader *ef, SShaderPass *sfm )
{
  return true;
}

bool CREFogVolume::mfDraw( CShader* ef, SShaderPass* sfm )
{
  return true;
}

bool CREWaterVolume::mfDraw( CShader* ef, SShaderPass* sfm )
{
  return true;
}

bool CREWaterWave::mfDraw( CShader* ef, SShaderPass* sfm )
{
  return true;
}

void CREWaterOcean::FrameUpdate()
{

}

void CREWaterOcean::Create(  uint32 nVerticesCount, struct_VERTEX_FORMAT_P3F *pVertices, uint32 nIndicesCount, uint16 *pIndices )
{
  
}

void CREWaterOcean::Release()
{

}

bool CREWaterOcean::mfDraw( CShader* ef, SShaderPass* sfm )
{
  return true;
}

CREOcclusionQuery::~CREOcclusionQuery()
{
  mfReset();
}

uint32 CREOcclusionQuery::GetVisSamplesCount() const
{
  return 800*600;
}

void CREOcclusionQuery::mfReset()
{
  memset(m_nVisSamples, 800*600, sizeof(m_nVisSamples)); // was 0 before
  memset(m_nCheckFrame, 0, sizeof(m_nCheckFrame));
  memset(m_nDrawFrame, 0, sizeof(m_nDrawFrame));

  m_nCurrQuery = 0;
}

uint CREOcclusionQuery::m_nQueriesPerFrameCounter = 0;
uint CREOcclusionQuery::m_nReadResultNowCounter = 0;
uint CREOcclusionQuery::m_nReadResultTryCounter = 0;

bool CREOcclusionQuery::mfDraw(CShader *ef, SShaderPass *sfm)
{
  return true;
}
bool CREOcclusionQuery::mfDrawQuad( struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad, int nOffs )
{
  return true;
}
bool CREOcclusionQuery::mfReadResult_Now(void)
{
  return true;
}
bool CREOcclusionQuery::mfReadResult_Try(void)
{
  return true;
}

void CRETempMesh::mfReset()
{
  gRenDev->ReleaseBuffer(m_pVBuffer, m_nVertices);
  gRenDev->ReleaseIndexBuffer(&m_Inds, m_nIndices);
  m_pVBuffer = NULL;
}

bool CRETempMesh::mfPreDraw(SShaderPass *sl)
{
  return true;
}

bool CRETempMesh::mfDraw(CShader *ef, SShaderPass *sl)
{
  return true;
}

bool CREMesh::mfPreDraw(SShaderPass *sl)
{
  return true;
}

bool CREMesh::mfDraw(CShader *ef, SShaderPass *sl)
{
  return true;
}

bool CREFlare::mfCheckVis(CRenderObject *obj)
{
  return false;
}

bool CREFlare::mfDraw(CShader *ef, SShaderPass *sfm)
{
  return true;
}

bool CREHDRProcess::mfDraw(CShader *ef, SShaderPass *sfm)
{
  return true;
}

bool CREBeam::mfDraw(CShader *ef, SShaderPass *sl)
{
  return true;
}

bool CREImposter::mfDraw(CShader *ef, SShaderPass *pPass)
{
  return true;
}

bool CREPanoramaCluster::mfDraw(CShader *ef, SShaderPass *pPass)
{
  return true;
}

bool CRECloud::mfDraw(CShader *ef, SShaderPass *pPass)
{
  return true;
}

bool CRECloud::UpdateImposter(CRenderObject *pObj)
{
  return true;
}

bool CREImposter::UpdateImposter()
{
  return true;
}

bool CREParticle::mfPreDraw(SShaderPass *sl)
{
	return true;
}

bool CREParticle::mfDraw(CShader* ef, SShaderPass* sfm)
{
	return true;
}

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
bool CREParticleGPU::mfDraw(CShader *ef, SShaderPass *sfm)
{
	return true;
}
#endif

bool CREVolumeObject::mfDraw(CShader* ef, SShaderPass* sfm)
{
	return true;
}
