/*=============================================================================
  RendElement.cpp : common RE functions.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"

//TArray<CRendElement *> CRendElement::m_AllREs;

CRendElement CRendElement::m_RootGlobal;

//===============================================================

TArray <ColorF> gCurLightStyles;

//============================================================================

void CRendElement::ShutDown()
{
  if (!CRenderer::CV_r_releaseallresourcesonexit)
    return;

  CRendElement *pRE = CRendElement::m_RootGlobal.m_NextGlobal;
  CRendElement *pRENext;
  for (pRE=CRendElement::m_RootGlobal.m_NextGlobal; pRE != &CRendElement::m_RootGlobal; pRE=pRENext)
  {
    pRENext = pRE->m_NextGlobal;
    if (CRenderer::CV_r_printmemoryleaks)
      iLog->Log("Warning: CRendElement::ShutDown: RenderElement %s was not deleted", pRE->mfTypeString());
    SAFE_RELEASE(pRE);
  }
}

CRendElement::~CRendElement()
{
  //assert(SRendItem::m_RecurseLevel <= 1);
  if ((m_Flags & FCEF_ALLOC_CUST_FLOAT_DATA) && m_CustomData)
  {
    delete [] ((float*)m_CustomData);
    m_CustomData=0;
  }

  UnlinkGlobal();
}

void CRendElement::mfPrepare()
{
}

CRenderChunk *CRendElement::mfGetMatInfo() {return NULL;}
PodArray<CRenderChunk> *CRendElement::mfGetMatInfoList() {return NULL;}
int CRendElement::mfGetMatId() {return -1;}
void CRendElement::mfReset() {}
bool CRendElement::mfCullByClipPlane(CRenderObject *pObj) { return false; }

const char *CRendElement::mfTypeString()
{
  switch(m_Type)
  {
    case eDATA_Sky:
      return "Sky";
    case eDATA_Beam:
      return "Beam";
    	break;
    case eDATA_ClientPoly:
      return "ClientPoly";
    	break;
    case eDATA_ClientPoly2D:
      return "ClientPoly2D";
    	break;
    case eDATA_Flare:
      return "Flare";
    	break;
    case eDATA_Terrain:
      return "Terrain";
    	break;
    case eDATA_SkyZone:
      return "SkyZone";
    	break;
    case eDATA_Mesh:
      return "Mesh";
    	break;
    case eDATA_TerrainSector:
      return "TerrainSector";
    	break;
    case eDATA_FarTreeSprites:
      return "FarTreeSprites";
    	break;
    case eDATA_ShadowMapGen:
      return "ShadowMapGen";
    	break;
    case eDATA_TerrainDetailTextureLayers:
      return "TerrainDetailTextureLayers";
    	break;
    case eDATA_TerrainParticles:
      return "TerrainParticles";
    	break;
    case eDATA_Ocean:
      return "Ocean";
    	break;
    case eDATA_PostProcess: 
      return "PostProcess";
      break;
		case eDATA_HDRSky: 
			return "HDRSky";
			break;
		case eDATA_FogVolume: 
			return "FogVolume";
			break;
  }
  return "Unknown";
}

CRendElement *CRendElement::mfCopyConstruct(void)
{
  CRendElement *re = new CRendElement;
  *re = *this;
  return re;
}
void CRendElement::mfCenter(Vec3& centr, CRenderObject *pObj)
{
  centr(0,0,0);
}
void CRendElement::mfGetPlane(Plane& pl)
{
  pl.n = Vec3(0,0,1);
  pl.d = 0;
}

void CRendElement::Release() { delete this;}
bool CRendElement::mfDraw(CShader *ef, SShaderPass *sfm) {return false;}
void *CRendElement::mfGetPointer(ESrcPointer ePT, int *Stride, EParamType Type, ESrcPointer Dst, int Flags) {return NULL;}
float CRendElement::mfDistanceToCameraSquared(Matrix34& matInst) {return 0.1f;}

//=============================================================================

void *SRendItem::mfGetPointerCommon(ESrcPointer ePT, int *Stride, EParamType Type, ESrcPointer Dst, int Flags)
{
  int j;
  switch (ePT)
  {      
    case eSrcPointer_Vert:
      *Stride = gRenDev->m_RP.m_Stride;
      return gRenDev->m_RP.m_Ptr.PtrB;

    case eSrcPointer_Color:
      *Stride = gRenDev->m_RP.m_Stride;
      return gRenDev->m_RP.m_Ptr.PtrB + gRenDev->m_RP.m_OffsD;

    case eSrcPointer_Tex:
    case eSrcPointer_TexLM:
      *Stride = gRenDev->m_RP.m_Stride;
      j = ePT - eSrcPointer_Tex;
      return gRenDev->m_RP.m_Ptr.PtrB + gRenDev->m_RP.m_OffsT + j*16;
  }
  return NULL;
}

