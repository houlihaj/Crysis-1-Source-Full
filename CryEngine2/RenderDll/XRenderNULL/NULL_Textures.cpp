/*=============================================================================
  PS2_Textures.cpp : PS2 specific texture manager implementation.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"
#include "NULL_Renderer.h"

//=================================================================================

///////////////////////////////////////////////////////////////////////////////////


void CNULLRenderer::MakeSprite( IDynTexture * &rTexturePtr, float _fSpriteDistance, int nTexSize, float angle, float angle2, IStatObj * pStatObj, const float fBrightnessMultiplier, SRendParams& rParms )
{
  rTexturePtr = NULL;

}

void CNULLRenderer::DrawObjSprites(PodArray<struct SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor)
{
}

void CNULLRenderer::GenerateObjSprites(PodArray<struct SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor)
{
}

int CNULLRenderer::GenerateAlphaGlowTexture(float k)
{
  return 0;
}

bool CNULLRenderer::EF_SetLightHole(Vec3 vPos, Vec3 vNormal, int idTex, float fScale, bool bAdditive)
{
  return false;
}


bool CTexture::ScanEnvironmentCM(const char *name, int size, Vec3& Pos)
{
  return true;
}

bool CTexture::ScanEnvironmentCube(SEnvTexture *pEnv, uint nRendFlags, int Size, bool bLightCube)
{
  return true;
}

void CTexture::Apply(int nTUnit, int nTS, int nTSlot, int nSSlot, int nResView)
{
}

byte *CTexture::Convert(byte *pSrc, int nWidth, int nHeight, int nMips, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nOutMips, int& nOutSize, bool bLinear)
{
  return NULL;
}

void CTexture::ReleaseDeviceTexture(bool bKeepLastMips)
{
}

byte *CTexture::LockData(int& nPitch, int nSide, int nLevel)
{
  return NULL;
}

void CTexture::UnlockData(int nSide, int nLevel)
{
}

bool CTexture::Fill(const ColorF& color)
{
  return true;
}

void CTexture::SetTexStates()
{
  m_nDefState = m_nGlobalDefState;
  if (m_nFlags & FT_STATE_CLAMP)
    SetClampingMode(TADDR_CLAMP, TADDR_CLAMP, TADDR_CLAMP);
}

void CTexture::GenerateFuncTextures(void)
{
}

bool CTexture::CreateDeviceTexture(byte *pData[6])
{
  return true;
}

ETEX_Format CTexture::ClosestFormatSupported(ETEX_Format eTFDst)
{
  return eTFDst;
}

bool CTexture::SetFilterMode(int nFilter)
{
  return m_sDefState.SetFilterMode(nFilter);
}

bool CTexture::CreateRenderTarget(ETEX_Format eTF)
{
  return true;
}

bool CTexture::SetClampingMode(int nAddressU, int nAddressV, int nAddressW)
{
  return m_sDefState.SetClampMode(nAddressU, nAddressV, nAddressW);
}

//======================================================================================

bool CFlashTextureSource::Update(float distToCamera)
{
  return true;
}

bool CVideoTextureSource::Update(float distToCamera)
{
	return true;
}

void SEnvTexture::Release()
{
}

bool SDynTexture::RestoreRT(int nRT, bool bPop)
{
  return true;
}

bool SDynTexture::SetRT(int nRT, bool bPush, SD3DSurface *pDepthSurf)
{
  return true;
}

bool SDynTexture2::SetRT(int nRT, bool bPush, SD3DSurface *pDepthSurf)
{
  return true;
}

bool SDynTexture2::RestoreRT(int nRT, bool bPop)
{
  return true;
}

bool SDynTexture2::SetRectStates()
{
  return true;
}

//===============================================================================

void STexState::PostCreate()
{
}

#if defined(_RENDERER)
STexState::~STexState()
{
}
#endif

#if defined(_RENDERER)
STexState::STexState(const STexState& src)
{
  memcpy(this, &src, sizeof(src));
}
#endif

bool STexState::SetClampMode(int nAddressU, int nAddressV, int nAddressW)
{
  m_nAddressU = 0;
  m_nAddressV = 0;
  m_nAddressW = 0;
  if (m_nAddressU < 0)
    m_nAddressU = CTexture::m_TexStates[CTexture::m_nGlobalDefState].m_nAddressU;
  if (m_nAddressV < 0)
    m_nAddressV = CTexture::m_TexStates[CTexture::m_nGlobalDefState].m_nAddressV;
  if (m_nAddressW < 0)
    m_nAddressW = CTexture::m_TexStates[CTexture::m_nGlobalDefState].m_nAddressW;
  return true;
}

bool STexState::SetFilterMode(int nFilter)
{
  m_nMinFilter = CTexture::m_TexStates[CTexture::m_nGlobalDefState].m_nMinFilter;
  m_nMagFilter = CTexture::m_TexStates[CTexture::m_nGlobalDefState].m_nMagFilter;
  m_nMipFilter = CTexture::m_TexStates[CTexture::m_nGlobalDefState].m_nMipFilter;
  return true;
}

void STexState::SetBorderColor(DWORD dwColor)
{
  m_dwBorderColor = dwColor; 
}


SD3DSurface::~SD3DSurface()
{
}

bool CTexture::SaveDDS(const char *szName, bool bMips)
{
  return false;
}

bool CTexture::SaveJPG(const char *szName, bool bMips)
{
  return false;
}

bool CTexture::SaveTGA(const char *szName, bool bMips)
{
  return false;
}

ETEX_Format CTexture::TexFormatFromDeviceFormat(int nFormat)
{
  return eTF_Unknown;
}
