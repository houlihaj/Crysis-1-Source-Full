/*=============================================================================
  DynTexture.cpp : Common dynamic texture manager implementation.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"

//======================================================================
// Dynamic textures
SDynTexture SDynTexture::m_Root("Root");
int SDynTexture::m_nMemoryOccupied;

uint    SDynTexture::m_iNumTextureBytesCheckedOut;
uint    SDynTexture::m_iNumTextureBytesCheckedIn;

SDynTexture::TextureSet    SDynTexture::m_availableTexturePool2D_A8R8G8B8;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePool2D_A8R8G8B8;
SDynTexture::TextureSet    SDynTexture::m_availableTexturePool2D_R32F;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePool2D_R32F;
SDynTexture::TextureSet    SDynTexture::m_availableTexturePool2D_R16F;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePool2D_R16F;
SDynTexture::TextureSet    SDynTexture::m_availableTexturePool2D_G16R16F;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePool2D_G16R16F;
SDynTexture::TextureSet    SDynTexture::m_availableTexturePool2D_A16B16G16R16F;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePool2D_A16B16G16R16F;

SDynTexture::TextureSet    SDynTexture::m_availableTexturePool2D_Shadows[SBP_MAX];
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePool2D_Shadows[SBP_MAX];
SDynTexture::TextureSet    SDynTexture::m_availableTexturePoolCube_Shadows[SBP_MAX];
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePoolCube_Shadows[SBP_MAX];

SDynTexture::TextureSet    SDynTexture::m_availableTexturePool2DCustom_G16R16F;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePool2DCustom_G16R16F;


SDynTexture::TextureSet    SDynTexture::m_availableTexturePoolCube_A8R8G8B8;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePoolCube_A8R8G8B8;
SDynTexture::TextureSet    SDynTexture::m_availableTexturePoolCube_R32F;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePoolCube_R32F;
SDynTexture::TextureSet    SDynTexture::m_availableTexturePoolCube_R16F;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePoolCube_R16F;
SDynTexture::TextureSet    SDynTexture::m_availableTexturePoolCube_G16R16F;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePoolCube_G16R16F;
SDynTexture::TextureSet    SDynTexture::m_availableTexturePoolCube_A16B16G16R16F;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePoolCube_A16B16G16R16F;


SDynTexture::TextureSet    SDynTexture::m_availableTexturePoolCubeCustom_G16R16F;
SDynTexture::TextureSubset SDynTexture::m_checkedOutTexturePoolCubeCustom_G16R16F;

int SDynTexture2::m_nMemoryOccupied[eTP_Max];

//======================================================================

SDynTexture::SDynTexture(const char *szSource)
{
  m_nWidth = 0;
  m_nHeight = 0;
  m_nReqWidth = m_nWidth;
  m_nReqHeight = m_nHeight;
  m_pTexture = NULL;
  m_eTF = eTF_Unknown;
  m_eTT = eTT_2D;
  m_nTexFlags = FT_DONT_ANISO;
  strncpy(m_sSource, szSource, sizeof(m_sSource));
  m_sSource[sizeof(m_sSource)-1] = 0;
  m_bLocked = false;
	m_nUpdateMask = 0;
  m_nPriority = -1;
  if (gRenDev)
    m_nFrameReset = gRenDev->m_nFrameReset;

  m_Next = NULL;
  m_Prev = NULL;
  if (!m_Root.m_Next)
  {
    m_Root.m_Next = &m_Root;
    m_Root.m_Prev = &m_Root;
  }
  if (this != &m_Root)
    Link();
  AdjustRealSize();
}

SDynTexture::SDynTexture(int nWidth, int nHeight, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char *szSource, int8 nPriority)
{
  m_nWidth =  nWidth;
  m_nHeight = nHeight;
  m_nReqWidth = m_nWidth;
  m_nReqHeight = m_nHeight;
  m_eTF = eTF;
  m_eTT = eTT;
  m_nTexFlags = nTexFlags;
  strncpy(m_sSource, szSource, sizeof(m_sSource));
  m_sSource[sizeof(m_sSource)-1] = 0;
  m_bLocked = false;
	m_nUpdateMask = 0;
  m_nPriority = nPriority;
  if (gRenDev)
    m_nFrameReset = gRenDev->m_nFrameReset;

  m_pTexture = NULL;
  m_Next = NULL;
  m_Prev = NULL;
  if (!m_Root.m_Next)
  {
    m_Root.m_Next = &m_Root;
    m_Root.m_Prev = &m_Root;
  }
  if (this != &m_Root)
    Link();
  AdjustRealSize();
}

SDynTexture::~SDynTexture()
{
  if (m_pTexture)
    ReleaseDynamicRT(false);
  m_pTexture = NULL;
  Unlink();
}

int SDynTexture::GetTextureID() 
{ 
	return m_pTexture ? m_pTexture->GetTextureID() : 0; 
}

bool SDynTexture::FreeTextures(bool bOldOnly, int nNeedSpace)
{
  bool bFreed = false;
  if (bOldOnly)
  {
    SDynTexture *pTX = SDynTexture::m_Root.m_Prev;
    int nFrame = gRenDev->m_nFrameUpdateID-400;
    while (nNeedSpace+m_nMemoryOccupied > CRenderer::CV_r_dyntexmaxsize*1024*1024)
    {
      if (pTX == &SDynTexture::m_Root)
        break;
      SDynTexture *pNext = pTX->m_Prev;
      // We cannot unload locked texture or texture used in current frame
      // Better to increase pool size temporarily
      if (pTX->m_pTexture && !pTX->m_pTexture->IsLocked())
      {
        if (pTX->m_pTexture->m_nAccessFrameID<nFrame && pTX->m_pTexture->m_nUpdateFrameID<nFrame && !pTX->m_bLocked)
          pTX->ReleaseDynamicRT(true);
      }
      pTX = pNext;
    }
    if (nNeedSpace+m_nMemoryOccupied < CRenderer::CV_r_dyntexmaxsize*1024*1024)
      return true;
  }
  bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT==eTT_2D ? &m_availableTexturePool2D_A8R8G8B8 : &m_availableTexturePoolCube_A8R8G8B8, bOldOnly);
  if (!bFreed)
    bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT==eTT_2D ? &m_availableTexturePool2D_R32F : &m_availableTexturePoolCube_R32F, bOldOnly);
  if (!bFreed)
    bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT==eTT_2D ? &m_availableTexturePool2D_G16R16F : &m_availableTexturePoolCube_G16R16F, bOldOnly);
  if (!bFreed)
    bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT==eTT_2D ? &m_availableTexturePool2D_A16B16G16R16F : &m_availableTexturePoolCube_A16B16G16R16F, bOldOnly);
  //if (!bFreed && m_eTT==eTT_2D)
  //bFreed = FreeAvailableDynamicRT(nNeedSpace, &m_availableTexturePool2D_ATIDF24);

  //First pass - Free textures from the pools with the same texture types
  //shadows pools
  for(int nPool = SBP_DF16; nPool<SBP_MAX; nPool++)
  {
    if (!bFreed && m_eTT==eTT_2D)
      bFreed = FreeAvailableDynamicRT(nNeedSpace, &m_availableTexturePool2D_Shadows[nPool], bOldOnly);
  }
  for(int nPool = SBP_DF16; nPool<SBP_MAX; nPool++)
  {
    if (!bFreed && m_eTT!=eTT_2D)
      bFreed = FreeAvailableDynamicRT(nNeedSpace, &m_availableTexturePoolCube_Shadows[nPool], bOldOnly);
  }

  if (!bFreed)
    bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT==eTT_2D ? &m_availableTexturePoolCube_A8R8G8B8 : &m_availableTexturePool2D_A8R8G8B8, bOldOnly);
  if (!bFreed)
    bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT==eTT_2D ? &m_availableTexturePoolCube_R32F : &m_availableTexturePool2D_R32F, bOldOnly);
  if (!bFreed)
    bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT==eTT_2D ? &m_availableTexturePoolCube_G16R16F : &m_availableTexturePool2D_G16R16F, bOldOnly);
  if (!bFreed)
    bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT==eTT_2D ? &m_availableTexturePoolCube_A16B16G16R16F : &m_availableTexturePool2D_A16B16G16R16F, bOldOnly);

  //if (!bFreed && m_eTT!=eTT_2D)
  //	bFreed = FreeAvailableDynamicRT(nNeedSpace, &m_availableTexturePool2D_ATIDF24);

  //Second pass - Free textures from the pools with the different texture types
  //shadows pools
  for(int nPool = SBP_DF16; nPool<SBP_MAX; nPool++)
  {
    if (!bFreed && m_eTT!=eTT_2D)
      bFreed = FreeAvailableDynamicRT(nNeedSpace, &m_availableTexturePool2D_Shadows[nPool], bOldOnly);
  }
  for(int nPool = SBP_DF16; nPool<SBP_MAX; nPool++)
  {
    if (!bFreed && m_eTT==eTT_2D)
      bFreed = FreeAvailableDynamicRT(nNeedSpace, &m_availableTexturePoolCube_Shadows[nPool], bOldOnly);
  }
  return bFreed;
}               

bool SDynTexture::Update(int nNewWidth, int nNewHeight)
{
  Unlink();

  assert(m_iNumTextureBytesCheckedOut+m_iNumTextureBytesCheckedIn == m_nMemoryOccupied);

  if (nNewWidth != m_nReqWidth || nNewHeight != m_nReqHeight)
  {
    if (m_pTexture)
      ReleaseDynamicRT(false);
    m_pTexture = NULL;

    m_nReqWidth = nNewWidth;
    m_nReqHeight = nNewHeight;

    AdjustRealSize();
  }

  if (!m_pTexture)
  {
    int nNeedSpace = CTexture::TextureDataSize(m_nWidth, m_nHeight, 1, 1, m_eTF);
    if (m_eTT == eTT_Cube)
      nNeedSpace *= 6;
    SDynTexture *pTX = SDynTexture::m_Root.m_Prev;
    if (nNeedSpace+m_nMemoryOccupied > CRenderer::CV_r_dyntexmaxsize*1024*1024)
    {
      m_pTexture = GetDynamicRT();
      if (!m_pTexture)
      {
        bool bFreed = FreeTextures(true, nNeedSpace);
        if (!bFreed)
          bFreed = FreeTextures(false, nNeedSpace);

        if (!bFreed)
        {
			    pTX = SDynTexture::m_Root.m_Next;
          int nFrame = gRenDev->m_nFrameUpdateID-1;
          while (nNeedSpace+m_nMemoryOccupied > CRenderer::CV_r_dyntexmaxsize*1024*1024)
          {
            if (pTX == &SDynTexture::m_Root)
            {
              static int nThrash;
              if (nThrash != nFrame)
              {
                nThrash = nFrame;
                iLog->Log("Error: Dynamic textures thrashing (try to increase texture pool size - r_DynTexMaxSize)...");
              }
              break;
            }
            SDynTexture *pNext = pTX->m_Next;
            // We cannot unload locked texture or texture used in current frame
            // Better to increase pool size temporarily
            if (pTX->m_pTexture && !pTX->m_pTexture->IsLocked())
            {
              if (pTX->m_pTexture->m_nAccessFrameID<nFrame && pTX->m_pTexture->m_nUpdateFrameID<nFrame && !pTX->m_bLocked)
                pTX->ReleaseDynamicRT(true);
            }
            pTX = pNext;
          }
        }
      }
    }
  }
  if (!m_pTexture)
    m_pTexture = CreateDynamicRT();

  assert(m_iNumTextureBytesCheckedOut+m_iNumTextureBytesCheckedIn == m_nMemoryOccupied);

  if (m_pTexture)
  {
    Link();
    return true;
  }
  return false;
}

void SDynTexture::AdjustRealSize()
{
  m_nWidth = m_nReqWidth;
  m_nHeight = m_nReqHeight;
}

void SDynTexture::GetImageRect(uint32 & nX, uint32 & nY, uint32 & nWidth, uint32 & nHeight)
{
  nX = 0;
  nY = 0;
  nWidth = m_nWidth;
  nHeight = m_nHeight;
}

void SDynTexture::Apply(int nTUnit, int nTS)
{
  if (!m_pTexture)
    Update(m_nWidth, m_nHeight);
  if (m_pTexture)
    m_pTexture->Apply(nTUnit, nTS);
  gRenDev->m_cEF.m_RTRect.x = 0;
  gRenDev->m_cEF.m_RTRect.y = 0;
  gRenDev->m_cEF.m_RTRect.z = 1;
  gRenDev->m_cEF.m_RTRect.w = 1;
}

bool SDynTexture::FreeAvailableDynamicRT(int nNeedSpace, TextureSet *pSet, bool bOldOnly)
{
  assert(m_iNumTextureBytesCheckedOut+m_iNumTextureBytesCheckedIn == m_nMemoryOccupied);

  int nSpace = m_nMemoryOccupied;
  int nFrame = gRenDev->m_nFrameUpdateID-400;
  while (nNeedSpace+nSpace > CRenderer::CV_r_dyntexmaxsize*1024*1024)
  {
    TextureSetItor itor=pSet->begin();
    while (itor!=pSet->end())
    {
      TextureSubset *pSubset = itor->second;
      TextureSubsetItor itorss = pSubset->begin();
      while (itorss!=pSubset->end())
      {
        CTexture *pTex = itorss->second;
        if (!bOldOnly || (pTex->m_nAccessFrameID<nFrame && pTex->m_nUpdateFrameID<nFrame))
        {
          itorss->second = NULL;
          pSubset->erase(itorss);
          itorss = pSubset->begin();
          nSpace -= pTex->GetDataSize();
          m_iNumTextureBytesCheckedIn -= pTex->GetDataSize();
          SAFE_RELEASE(pTex);
          if (nNeedSpace+nSpace < CRenderer::CV_r_dyntexmaxsize*1024*1024)
            break;
        }
        else
          itorss++;
      }
      if (pSubset->size() == 0)
      {
        delete pSubset;
        pSet->erase(itor);
        itor = pSet->begin();
      }
      else
        itor++;
      if (nNeedSpace+nSpace < CRenderer::CV_r_dyntexmaxsize*1024*1024)
        break;
    }
		if (itor==pSet->end())
			break;
  }
  m_nMemoryOccupied = nSpace;

  assert(m_iNumTextureBytesCheckedOut+m_iNumTextureBytesCheckedIn == m_nMemoryOccupied);

  if (nNeedSpace+nSpace > CRenderer::CV_r_dyntexmaxsize*1024*1024)
    return false;
  return true;
}

void SDynTexture::ReleaseDynamicRT(bool bForce)
{
  if (!m_pTexture)
    return;
  m_nUpdateMask = 0;

  // first see if the texture is in the checked out pool.
  TextureSubset *pSubset;
  if (m_eTF == eTF_A8R8G8B8)
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_A8R8G8B8 : &m_checkedOutTexturePoolCube_A8R8G8B8;
  else
  if (m_eTF == eTF_R32F)
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_R32F : &m_checkedOutTexturePoolCube_R32F;
  else
	if (m_eTF == eTF_R16F)
		pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_R16F : &m_checkedOutTexturePoolCube_R16F;
	else
  if (m_eTF == eTF_G16R16F)
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_G16R16F : &m_checkedOutTexturePoolCube_G16R16F;
  else
  if (m_eTF == eTF_A16B16G16R16F)
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_A16B16G16R16F : &m_checkedOutTexturePoolCube_A16B16G16R16F;
  else
  if (ConvertTexFormatToShadowsPool(m_eTF)!= SBP_UNKNOWN)
  {
    if (m_eTT==eTT_2D)
    {
      pSubset = &m_checkedOutTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
    }
    else if (m_eTT==eTT_Cube)
    {
      pSubset = &m_checkedOutTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
    }
    else
    {
      pSubset = NULL;
      assert(0);
    }

  }
  else
  {
    pSubset = NULL;
    assert(0);
  }

  TextureSubsetItor coTexture = pSubset->find(m_pTexture->GetID());
  if (coTexture != pSubset->end())
  { // if it is there, remove it.
    pSubset->erase(coTexture);
    m_iNumTextureBytesCheckedOut -= m_pTexture->GetDataSize();
  }
  else
  {
    //assert(false);
    int nnn = 0;
  }
 
  // Don't cache too many unused textures.
  if (bForce)
  {
    m_nMemoryOccupied -= m_pTexture->GetDataSize();
    assert(m_iNumTextureBytesCheckedOut+m_iNumTextureBytesCheckedIn == m_nMemoryOccupied);
    int refCount = m_pTexture->Release();
		assert(refCount <= 0);
    m_pTexture = NULL;
    Unlink();
    return;
  }

  TextureSet *pSet;
  if (m_eTF == eTF_A8R8G8B8)
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_A8R8G8B8 : &m_availableTexturePoolCube_A8R8G8B8;
  else
  if (m_eTF == eTF_R32F)
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_R32F : &m_availableTexturePoolCube_R32F;
  else
	if (m_eTF == eTF_R16F)
		pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_R16F : &m_availableTexturePoolCube_R16F;
	else
  if (m_eTF == eTF_G16R16F)
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_G16R16F : &m_availableTexturePoolCube_G16R16F;
  else
  if (m_eTF == eTF_A16B16G16R16F)
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_A16B16G16R16F : &m_availableTexturePoolCube_A16B16G16R16F;
	else
  if (ConvertTexFormatToShadowsPool(m_eTF)!= SBP_UNKNOWN)
  {
    if (m_eTT==eTT_2D)
    {
		  pSet = &m_availableTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
    }
    else if (m_eTT==eTT_Cube)
    {
		  pSet = &m_availableTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
    }
    else
    {
      pSet = NULL;
      assert(0);
    }
	}
  else
  {
    pSet = NULL;
    assert(0);
  }
  TextureSetItor subset = pSet->find(m_nWidth);
  if (subset != pSet->end())
  { 
    subset->second->insert(TextureSubset::value_type(m_nHeight, m_pTexture));
    m_iNumTextureBytesCheckedIn += m_pTexture->GetDataSize();
  }
  else
  { 
    TextureSubset *pSubset = new TextureSubset;
    pSet->insert(TextureSet::value_type(m_nWidth, pSubset));
    pSubset->insert(TextureSubset::value_type(m_nHeight, m_pTexture));
    m_iNumTextureBytesCheckedIn += m_pTexture->GetDataSize();
  }
  m_pTexture = NULL;
  Unlink();
}

CTexture* SDynTexture::GetDynamicRT()
{
  TextureSet *pSet;
  TextureSubset *pSubset;

  assert(m_iNumTextureBytesCheckedOut+m_iNumTextureBytesCheckedIn == m_nMemoryOccupied);

  if (m_eTF == eTF_A8R8G8B8)
  {
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_A8R8G8B8 : &m_availableTexturePoolCube_A8R8G8B8;
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_A8R8G8B8 : &m_checkedOutTexturePoolCube_A8R8G8B8;
  }
  else
  if (m_eTF == eTF_R32F)
  {
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_R32F : &m_availableTexturePoolCube_R32F;
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_R32F : &m_checkedOutTexturePoolCube_R32F;
  }
  else
	if (m_eTF == eTF_R16F)
	{
		pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_R16F : &m_availableTexturePoolCube_R16F;
		pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_R16F : &m_checkedOutTexturePoolCube_R16F;
	}
	else
  if (m_eTF == eTF_G16R16F)
  {
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_G16R16F : &m_availableTexturePoolCube_G16R16F;
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_G16R16F : &m_checkedOutTexturePoolCube_G16R16F;
  }
  else
  if (m_eTF == eTF_A16B16G16R16F)
  {
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_A16B16G16R16F : &m_availableTexturePoolCube_A16B16G16R16F;
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_A16B16G16R16F : &m_checkedOutTexturePoolCube_A16B16G16R16F;
  }
	else
	if (ConvertTexFormatToShadowsPool(m_eTF)!= SBP_UNKNOWN)
	{
    if (m_eTT==eTT_2D)
    {
      pSet = &m_availableTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
      pSubset = &m_checkedOutTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
    }
    else if (m_eTT==eTT_Cube)
    {
      pSet = &m_availableTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
      pSubset = &m_checkedOutTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
    }
    else
    {
      pSet = NULL;
      pSubset = NULL;
      assert(0);
    }
	}
  else
  {
    pSet = NULL;
    pSubset = NULL;
    assert(0);
  }

  TextureSetItor subset = pSet->find(m_nWidth);
  if (subset != pSet->end())
  {
    TextureSubsetItor texture = subset->second->find(m_nHeight);
    if (texture != subset->second->end())
    { //  found one!
      // extract the texture
      CTexture *pTexture = texture->second;
      texture->second = NULL;
      // first remove it from this set.
      subset->second->erase(texture);
      // now add it to the checked out texture set.
      pSubset->insert(TextureSubset::value_type(pTexture->GetID(), pTexture));
      m_iNumTextureBytesCheckedOut += pTexture->GetDataSize();
      m_iNumTextureBytesCheckedIn -= pTexture->GetDataSize();

      assert(m_iNumTextureBytesCheckedOut+m_iNumTextureBytesCheckedIn == m_nMemoryOccupied);

      return pTexture;
    }
  }
  return NULL;
}

CTexture *SDynTexture::CreateDynamicRT()
{
  //assert(m_eTF == eTF_A8R8G8B8 && m_eTT == eTT_2D);

  assert(m_iNumTextureBytesCheckedOut+m_iNumTextureBytesCheckedIn == m_nMemoryOccupied);

  char name[256];
  CTexture *pTexture = GetDynamicRT();
  if (pTexture)
    return pTexture;

  TextureSet *pSet;
  TextureSubset *pSubset;
  if (m_eTF == eTF_A8R8G8B8)
  {
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_A8R8G8B8 : &m_availableTexturePoolCube_A8R8G8B8;
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_A8R8G8B8 : &m_checkedOutTexturePoolCube_A8R8G8B8;
  }
  else
  if (m_eTF == eTF_R32F)
  {
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_R32F : &m_availableTexturePoolCube_R32F;
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_R32F : &m_checkedOutTexturePoolCube_R32F;
  }
  else
	if (m_eTF == eTF_R16F)
	{
		pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_R16F : &m_availableTexturePoolCube_R16F;
		pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_R16F : &m_checkedOutTexturePoolCube_R16F;
	}
	else
  if (m_eTF == eTF_G16R16F)
  {
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_G16R16F : &m_availableTexturePoolCube_G16R16F;
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_G16R16F : &m_checkedOutTexturePoolCube_G16R16F;
  }
  else
  if (m_eTF == eTF_A16B16G16R16F)
  {
    pSet = m_eTT==eTT_2D ? &m_availableTexturePool2D_A16B16G16R16F : &m_availableTexturePoolCube_A16B16G16R16F;
    pSubset = m_eTT==eTT_2D ? &m_checkedOutTexturePool2D_A16B16G16R16F : &m_checkedOutTexturePoolCube_A16B16G16R16F;
  }
	else
	if (ConvertTexFormatToShadowsPool(m_eTF)!= SBP_UNKNOWN)
	{
    if (m_eTT==eTT_2D)
    {
      pSet = &m_availableTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
      pSubset = &m_checkedOutTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
    }
    else if (m_eTT==eTT_Cube)
    {
      pSet = &m_availableTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
      pSubset = &m_checkedOutTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
    }
    else
    {
      pSet = NULL;
      pSubset = NULL;
      assert(0);
    }
	}
  else
  {
    pSet = NULL;
    pSubset = NULL;
    assert(0);
  }

	if (m_eTT == eTT_2D)
		sprintf(name, "$Dyn_%s_2D_%s_%d",m_sSource?m_sSource:"", CTexture::NameForTextureFormat(m_eTF), gRenDev->m_TexGenID++);
	else
		sprintf(name, "$Dyn_%s_Cube_%s_%d",m_sSource?m_sSource:"", CTexture::NameForTextureFormat(m_eTF), gRenDev->m_TexGenID++);

	TextureSetItor subset = pSet->find(m_nWidth);
  if (subset != pSet->end())
  {
    CTexture *pNewTexture = CTexture::CreateRenderTarget(name, m_nWidth, m_nHeight, m_eTT, m_nTexFlags, m_eTF, -1, m_nPriority);
    pSubset->insert(TextureSubset::value_type(pNewTexture->GetID(), pNewTexture));
    m_nMemoryOccupied += pNewTexture->GetDataSize();
    m_iNumTextureBytesCheckedOut += pNewTexture->GetDataSize();

    assert(m_iNumTextureBytesCheckedOut+m_iNumTextureBytesCheckedIn == m_nMemoryOccupied);

    return pNewTexture;
  }
  else
  { 
    TextureSubset *pSSet = new TextureSubset;
    pSet->insert(TextureSet::value_type(m_nWidth, pSSet));
    CTexture *pNewTexture = CTexture::CreateRenderTarget(name, m_nWidth, m_nHeight, m_eTT, m_nTexFlags, m_eTF, -1, m_nPriority);
    pSubset->insert(TextureSubset::value_type(pNewTexture->GetID(), pNewTexture));
    m_nMemoryOccupied += pNewTexture->GetDataSize();
    m_iNumTextureBytesCheckedOut += pNewTexture->GetDataSize();

    assert(m_iNumTextureBytesCheckedOut+m_iNumTextureBytesCheckedIn == m_nMemoryOccupied);

    return pNewTexture;
  }
}

void SDynTexture::ResetUpdateMask()
{
  m_nUpdateMask = 0;
  if (gRenDev)
    m_nFrameReset = gRenDev->m_nFrameReset;
}

void SDynTexture::SetUpdateMask()
{
  int nFrame = gRenDev->m_nFrameSwapID % gRenDev->m_nGPUs;
  m_nUpdateMask |= 1<<nFrame;
}

bool SDynTexture::IsValid()
{
  if (!m_pTexture)
    return false;
  if (m_nFrameReset != gRenDev->m_nFrameReset)
  {
    m_nFrameReset = gRenDev->m_nFrameReset;
    m_nUpdateMask = 0;
    return false;
  }
  if (gRenDev->IsMultiGPUModeActive())
  {
    if ((gRenDev->GetFeatures() & RFT_HW_MASK) == RFT_HW_ATI) 
    {
      uint32 nX, nY, nW, nH;
      GetImageRect(nX, nY, nW, nH);
      if (nW<1024 && nH<1024)
        return true;
    }

    int nFrame = gRenDev->m_nFrameSwapID % gRenDev->m_nGPUs;
    if (!((1<<nFrame) & m_nUpdateMask))
      return false;
  }
  return true;
}

void SDynTexture::Init()
{

}

EShadowBuffers_Pool SDynTexture::ConvertTexFormatToShadowsPool( ETEX_Format e )
{
  switch (e)
  {
  case (eTF_DF16): 
    return SBP_DF16;
  case (eTF_DF24): 
    return SBP_DF24;
  case (eTF_D16): 
    return SBP_D16;
  case (eTF_D24S8): 
    return SBP_D24S8;
  case (eTF_G16R16): 
    return SBP_G16R16;
  case (eTF_D32F): 
    return SBP_D32F;

  default: break;
  }
  //assert( false );
  return SBP_UNKNOWN;
}


int SDynTexture2::GetTextureID() 
{ 
	return m_pTexture ? m_pTexture->GetTextureID() : 0; 
}

void SDynTexture2::GetImageRect(uint32 & nX, uint32 & nY, uint32 & nWidth, uint32 & nHeight)
{
  nX = 0;
  nY = 0;
  if (m_pTexture)
  {
    if (m_pTexture->GetWidth() != CRenderer::CV_r_texatlassize || m_pTexture->GetHeight() != CRenderer::CV_r_texatlassize)
      UpdateAtlasSize(CRenderer::CV_r_texatlassize, CRenderer::CV_r_texatlassize);                      
    assert (m_pTexture->GetWidth() == CRenderer::CV_r_texatlassize || m_pTexture->GetHeight() == CRenderer::CV_r_texatlassize);
  }
  nWidth  = CRenderer::CV_r_texatlassize;
  nHeight = CRenderer::CV_r_texatlassize;
}

//====================================================================================

SDynTexture_Shadow SDynTexture_Shadow::m_RootShadow("RootShadow");

SDynTexture_Shadow::SDynTexture_Shadow(const char *szSource) : SDynTexture(szSource)
{
  m_nUniqueID = 0;
  m_NextShadow = NULL;
  m_PrevShadow = NULL;
  if (!m_RootShadow.m_NextShadow)
  {
    m_RootShadow.m_NextShadow = &m_RootShadow;
    m_RootShadow.m_PrevShadow = &m_RootShadow;
  }
  if (this != &m_RootShadow)
    LinkShadow(&m_RootShadow);
}

SDynTexture_Shadow::SDynTexture_Shadow(int nWidth, int nHeight, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char *szSource) : SDynTexture(nWidth, nHeight, eTF, eTT, nTexFlags, szSource)
{
  m_nWidth = nWidth;
  m_nHeight = nHeight;

  if (gRenDev)
    m_nUniqueID = gRenDev->m_TexGenID++;
  else
    m_nUniqueID = 0;

  m_NextShadow = NULL;
  m_PrevShadow = NULL;
  if (!m_RootShadow.m_NextShadow)
  {
    m_RootShadow.m_NextShadow = &m_RootShadow;
    m_RootShadow.m_PrevShadow = &m_RootShadow;
  }
  if (this != &m_RootShadow)
    LinkShadow(&m_RootShadow);
}

void SDynTexture_Shadow::AdjustRealSize()
{
  if (m_eTT == eTT_2D)
  {
    if (m_nWidth < 256)
      m_nWidth = 256;
    else
    if (m_nWidth > 2048)
      m_nWidth = 2048;
    m_nHeight = m_nWidth;
  }
  if (m_eTT == eTT_Cube)
  {
    if (m_nWidth < 256)
      m_nWidth = 256;
    else
    if (m_nWidth > 512)
      m_nWidth = 512;
    m_nHeight = m_nWidth;
  }
}

SDynTexture_Shadow::~SDynTexture_Shadow()
{
  UnlinkShadow();
}

//====================================================================================


SDynTextureArray::SDynTextureArray(int nWidth, int nHeight, int nArraySize, ETEX_Format eTF, int nTexFlags, const char *szSource)
{
  m_eTF = eTF;
  m_eTT = eTT_2D;

  m_nWidth = nWidth;
  m_nHeight = nHeight;
  m_nArraySize = nArraySize;

  m_nReqWidth = m_nWidth;
  m_nReqHeight = m_nHeight;
  m_nTexFlags = nTexFlags;

  strncpy(m_sSource, szSource, sizeof(m_sSource));
  m_sSource[sizeof(m_sSource)-1] = 0;
  m_bLocked = false;
  m_nUpdateMask = 0;
  m_nPriority = -1;
  if (gRenDev)
    m_nFrameReset = gRenDev->m_nFrameReset;

  //FIX: is it necessary
  if (gRenDev)
    m_nUniqueID = gRenDev->m_TexGenID++;
  else
    m_nUniqueID = 0;

  m_pTexture = CTexture::CreateTextureArray(m_sSource, m_nWidth, m_nHeight, m_nArraySize, m_nTexFlags, m_eTF, -1, m_nPriority);

}

bool SDynTextureArray::Update(int nNewWidth, int nNewHeight)
{
  return 
    m_pTexture->Invalidate(nNewWidth, nNewHeight, m_eTF);
}

SDynTextureArray::~SDynTextureArray()
{
  SAFE_DELETE(m_pTexture);
}

//=================================================================

SDynTexture2::TextureSet2 SDynTexture2::m_TexturePool[eTP_Max];

SDynTexture2::SDynTexture2(const char *szSource, ETexPool eTexPool)
{
  m_nWidth = 0;
  m_nHeight = 0;

  m_pOwner = NULL;

#ifndef _DEBUG
  m_sSource = (char *)szSource;
#else
  strcpy(m_sSource, szSource);
#endif
  m_bLocked = false;

  m_eTexPool = eTexPool;

  m_nBlockID = ~0;
  m_pAllocator = NULL;
  m_Next = NULL;
  m_PrevLink = NULL;
  m_nUpdateMask = 0;
  if (gRenDev)
    m_nFrameReset = gRenDev->m_nFrameReset;
  SetUpdateMask();
}

void SDynTexture2::SetUpdateMask()
{
  if (gRenDev)
  {
    int nFrame = gRenDev->m_nFrameSwapID % gRenDev->m_nGPUs;
    m_nUpdateMask |= 1<<nFrame;
  }
}

void SDynTexture2::ResetUpdateMask()
{
  m_nUpdateMask = 0;
  if (gRenDev)
    m_nFrameReset = gRenDev->m_nFrameReset;
}

SDynTexture2::SDynTexture2(int nWidth, int nHeight, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char *szSource, ETexPool eTexPool)
{
  m_nWidth = nWidth;
  m_nHeight = nHeight;

  m_eTexPool = eTexPool;

  TextureSet2Itor tset = m_TexturePool[eTexPool].find(eTF);
  if (tset == m_TexturePool[eTexPool].end())
  {
    m_pOwner = new STextureSetFormat(eTF, FT_NOMIPS | FT_USAGE_ATLAS | FT_DONTSYNCMULTIGPU);
    m_TexturePool[eTexPool].insert(TextureSet2::value_type(eTF, m_pOwner));
  }
  else
    m_pOwner = tset->second;

  m_Next = NULL;
  m_PrevLink = NULL;
  m_bLocked = false;

#ifndef _DEBUG
  m_sSource = (char *)szSource;
#else
  strcpy(m_sSource, szSource);
#endif

  m_pTexture = NULL;
  m_nBlockID = ~0;
  m_pAllocator = NULL;
  m_nUpdateMask = 0;
  if (gRenDev)
    m_nFrameReset = gRenDev->m_nFrameReset;
  SetUpdateMask();
}

SDynTexture2::~SDynTexture2()
{
  Remove();
  m_bLocked = false;
}

void SDynTexture2::Init(ETexPool eTexPool)
{
  int nSize = CRenderer::CV_r_texatlassize;
  TArray<SDynTexture2 *> Texs;
  ETEX_Format eTF = eTF_A8R8G8B8;
  int nMaxSize = eTexPool==eTP_Clouds ? CRenderer::CV_r_dyntexatlascloudsmaxsize : CRenderer::CV_r_dyntexatlasspritesmaxsize;
  const char *szName = eTexPool==eTP_Clouds ? "PreallocClouds" : "PreallocSprites";
  nMaxSize *= 1024*1024;
  while (true)
  {
    int nNeedSpace = CTexture::TextureDataSize(nSize, nSize, 1, 1, eTF);
    if (nNeedSpace+m_nMemoryOccupied[eTexPool] > nMaxSize)
      break;
    SDynTexture2 *pTex = new SDynTexture2(nSize, nSize, eTF, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP | FT_NOMIPS, szName, eTexPool);
    pTex->Update(nSize, nSize);
    Texs.AddElem(pTex);
  }
  for (int i=0; i<Texs.Num(); i++)
  {
    SDynTexture2 *pTex = Texs[i];
    SAFE_DELETE(pTex);
  }
}

bool SDynTexture2::UpdateAtlasSize(int nNewWidth, int nNewHeight)
{
  if (!m_pOwner)
    return false;
  if (!m_pTexture)
    return false;
  if (!m_pAllocator)
    return false;
  if (m_pTexture->GetWidth() != nNewWidth || m_pTexture->GetHeight() != nNewHeight)
  {
    SDynTexture2 *pDT, *pNext;
    for (pDT=m_pOwner->m_pRoot; pDT; pDT=pNext)
    {
      pNext = pDT->m_Next;
      if (pDT == this)
        continue;
      assert(!pDT->m_bLocked);
      pDT->Remove();
      pDT->SetUpdateMask();
    }
    int nBlockW = (m_nWidth +TEX_POOL_BLOCKSIZE-1) / TEX_POOL_BLOCKSIZE;
    int nBlockH = (m_nHeight+TEX_POOL_BLOCKSIZE-1) / TEX_POOL_BLOCKSIZE;
    m_pAllocator->RemoveBlock(m_nBlockID);
    assert (m_pAllocator && m_pAllocator->GetNumUsedBlocks() == 0);

    int nW = (nNewWidth +TEX_POOL_BLOCKSIZE-1) / TEX_POOL_BLOCKSIZE;
    int nH = (nNewHeight +TEX_POOL_BLOCKSIZE-1) / TEX_POOL_BLOCKSIZE;
    m_pAllocator->UpdateSize(nW, nH);
    m_nBlockID = m_pAllocator->AddBlock(nBlockW, nBlockH);
    m_nMemoryOccupied[m_eTexPool] -= CTexture::TextureDataSize(m_pTexture->GetWidth(), m_pTexture->GetHeight(), 1, 1, m_pOwner->m_eTF);
    SAFE_RELEASE(m_pTexture);

    char name[256];
    sprintf(name, "$Dyn_2D_%s_%s_%d", CTexture::NameForTextureFormat(m_pOwner->m_eTF), m_eTexPool==eTP_Clouds ? "Clouds" : "Sprites", gRenDev->m_TexGenID++);
    m_pAllocator->m_pTexture = CTexture::CreateRenderTarget(name, nNewWidth, nNewHeight, m_pOwner->m_eTT, m_pOwner->m_nTexFlags, m_pOwner->m_eTF, -1, 50);
    m_pAllocator->m_pTexture->Fill(Col_Blue);
    m_nMemoryOccupied[m_eTexPool] += CTexture::TextureDataSize(nNewWidth, nNewHeight, 1, 1, m_pOwner->m_eTF);
    m_pTexture = m_pAllocator->m_pTexture;
  }
  return true;
}

bool SDynTexture2::Update(int nNewWidth, int nNewHeight)
{
  int i;
  if (!m_pOwner)
    return false;
  bool bRecreate = false;
  if (!m_pAllocator)
    bRecreate = true;
  int nStage = -1;
  m_nAccessFrame = gRenDev->GetFrameID(false);
  SDynTexture2 *pDT = NULL;

#if 0
  for (pDT=m_pOwner->m_pRoot; pDT; pDT=pDT->m_Next)
  {
    int nOldB = pDT->m_nBlockID;
    CPowerOf2BlockPacker *pOldPack = pDT->m_pAllocator;
    SDynTexture2 *pDT1 = NULL;
    for (pDT1=pDT->m_Next; pDT1; pDT1=pDT1->m_Next)
    {
      if (pDT1->m_pAllocator == pOldPack && pDT1->m_nBlockID == nOldB)
      {
        assert(0);
      }
    }
  }
#endif

  if (m_nWidth != nNewWidth || m_nHeight != nNewHeight)
  {
    bRecreate = true;
    m_nWidth = nNewWidth;
    m_nHeight = nNewHeight;
  }
  uint nFrame = gRenDev->m_nFrameUpdateID;
  if (bRecreate)
  {
    int nSize = CRenderer::CV_r_texatlassize;
    if (nSize <= 512)
      nSize = 512;
    else
    if (nSize <= 1024)
      nSize = 1024;
    else
    if (nSize >= 2048)
      nSize = 2048;

    int nBlockW = (nNewWidth +TEX_POOL_BLOCKSIZE-1) / TEX_POOL_BLOCKSIZE;
    int nBlockH = (nNewHeight+TEX_POOL_BLOCKSIZE-1) / TEX_POOL_BLOCKSIZE;
    Remove();
    SetUpdateMask();

    uint32 nID = ~0;
    CPowerOf2BlockPacker *pPack = NULL;
    for (i=0; i<m_pOwner->m_TexPools.size(); i++)
    {
      pPack = m_pOwner->m_TexPools[i];
      nID = pPack->AddBlock(LogBaseTwo(nBlockW), LogBaseTwo(nBlockH));
      if (nID != -1)
        break;
    }
    if (i == m_pOwner->m_TexPools.size())
    {
      nStage = 1;
      int nNeedSpace = CTexture::TextureDataSize(nSize, nSize, 1, 1, m_pOwner->m_eTF);
      int nMaxSize = m_eTexPool==eTP_Clouds ? CRenderer::CV_r_dyntexatlascloudsmaxsize : CRenderer::CV_r_dyntexatlasspritesmaxsize;
      if (nNeedSpace+m_nMemoryOccupied[m_eTexPool] > nMaxSize*1024*1024)
      {
        SDynTexture2 *pDTBest = NULL;
        SDynTexture2 *pDTBestLarge = NULL;
        int nError = 1000000;
        uint nFr = INT_MAX;
        uint nFrLarge = INT_MAX;
        int n = 0;
        for (pDT=m_pOwner->m_pRoot; pDT; pDT=pDT->m_Next)
        {
          if (pDT == this || pDT->m_bLocked)
            continue;
          n++;
          assert (pDT->m_pAllocator && pDT->m_pTexture && pDT->m_nBlockID != -1);
          if (pDT->m_nWidth == m_nWidth && pDT->m_nHeight == m_nHeight)
          {
            if (nFr > pDT->m_nAccessFrame)
            {
              nFr = pDT->m_nAccessFrame;
              pDTBest = pDT;
            }
          }
          else
          if (pDT->m_nWidth>=m_nWidth && pDT->m_nHeight>=m_nHeight)
          {
            int nEr = pDT->m_nWidth-m_nWidth + pDT->m_nHeight-m_nHeight;
            int fEr = nEr + (nFrame-pDT->m_nAccessFrame);

            if (fEr < nError)
            {
              nFrLarge = pDT->m_nAccessFrame;
              nError = fEr;
              pDTBestLarge = pDT;
            }
          }
        }
        pDT = NULL;
        if (pDTBest && nFr+1 < nFrame)
        {
          nStage = 2;
          pDT = pDTBest;
          nID = pDT->m_nBlockID;
          pPack = pDT->m_pAllocator;
          pDT->m_pAllocator = NULL;
          pDT->m_pTexture = NULL;
          pDT->m_nBlockID = ~0;
          pDT->m_nUpdateMask = 0;
          pDT->SetUpdateMask();
          pDT->Unlink();
        }
        else
        if (pDTBestLarge && nFrLarge+1 < nFrame)
        {
          nStage = 3;
          pDT = pDTBestLarge;
          CPowerOf2BlockPacker *pAllocator = pDT->m_pAllocator;
          pDT->Remove();
          pDT->SetUpdateMask();
          nID = pAllocator->AddBlock(LogBaseTwo(nBlockW), LogBaseTwo(nBlockH));
          assert (nID != -1);
          if (nID != -1)
            pPack = pAllocator;
          else
            pDT = NULL;
        }
        if (!pDT)
        {
          nStage = 4;
          // Try to find oldest texture pool
          float fTime = FLT_MAX;
          CPowerOf2BlockPacker *pPackBest = NULL;
          for (i=0; i<m_pOwner->m_TexPools.size(); i++)
          {
            CPowerOf2BlockPacker *pPack = m_pOwner->m_TexPools[i];
            if (fTime > pPack->m_fLastUsed)
            {
              fTime = pPack->m_fLastUsed;
              pPackBest = pPack;
            }
          }
          if (!pPackBest || fTime+0.5f>gRenDev->m_RP.m_RealTime)
          {
            nStage = 5;
            // Try to find most fragmented texture pool with less number of blocks
            uint32 nUsedBlocks = TEX_POOL_BLOCKSIZE * TEX_POOL_BLOCKSIZE + 1;
            pPackBest = NULL;
            for (i=0; i<m_pOwner->m_TexPools.size(); i++)
            {
              CPowerOf2BlockPacker *pPack = m_pOwner->m_TexPools[i];
              int nBlocks = pPack->GetNumUsedBlocks();
              if (nBlocks < nUsedBlocks)
              {
                nUsedBlocks = nBlocks;
                pPackBest = pPack;
              }
            }
          }
          if (pPackBest)
          {
            SDynTexture2 *pNext = NULL;
            for (pDT=m_pOwner->m_pRoot; pDT; pDT=pNext)
            {
              pNext = pDT->m_Next;
              if (pDT == this || pDT->m_bLocked)
                continue;
              if (pDT->m_pAllocator == pPackBest)
                pDT->Remove();
            }
            assert (pPackBest->GetNumUsedBlocks() == 0);
            pPack = pPackBest;
            nID = pPack->AddBlock(LogBaseTwo(nBlockW), LogBaseTwo(nBlockH));
            assert (nID != -1);
            pDT = this;
            pDT->m_nUpdateMask = 0;
          }
          else
          {
            nStage = 6;
            assert(0);
          }
        }
      }

      if (!pDT)
      {
        nStage |= 0x100;
        int n = (nSize +TEX_POOL_BLOCKSIZE-1) / TEX_POOL_BLOCKSIZE;
        pPack = new CPowerOf2BlockPacker(LogBaseTwo(n), LogBaseTwo(n));
        m_pOwner->m_TexPools.push_back(pPack);
        nID = pPack->AddBlock(LogBaseTwo(nBlockW), LogBaseTwo(nBlockH));
        char name[256];
        sprintf(name, "$Dyn_2D_%s_%s_%d", CTexture::NameForTextureFormat(m_pOwner->m_eTF), m_eTexPool==eTP_Clouds ? "Clouds" : "Sprites", gRenDev->m_TexGenID++);
        pPack->m_pTexture = CTexture::CreateRenderTarget(name, nSize, nSize, m_pOwner->m_eTT, m_pOwner->m_nTexFlags, m_pOwner->m_eTF, -1, 50);

        pPack->m_pTexture->Fill(Col_Blue);
        m_nMemoryOccupied[m_eTexPool] += nNeedSpace;
        if (nID == -1)
        {
          assert(0);
          nID = (uint32)-2;
        }
      }
    }
    m_nBlockID = nID;
    m_pAllocator = pPack;
    if (pPack)
    {
      m_pTexture = pPack->m_pTexture;
      uint32 nX1, nX2, nY1, nY2;
      pPack->GetBlockInfo(nID, nX1, nY1, nX2, nY2);
      m_nX = nX1 << TEX_POOL_BLOCKLOGSIZE;
      m_nY = nY1 << TEX_POOL_BLOCKLOGSIZE;
      m_nWidth  = (nX2 - nX1) << TEX_POOL_BLOCKLOGSIZE;
      m_nHeight = (nY2 - nY1) << TEX_POOL_BLOCKLOGSIZE;
      m_nUpdateMask = 0;
      SetUpdateMask();
    }
  }
  m_pAllocator->m_fLastUsed = gRenDev->m_RP.m_RealTime;
  Unlink();
  if (m_pTexture)
  {
    Link();
    return true;
  }
  return false;
}

bool SDynTexture2::Remove()
{
  if (!m_pAllocator)
    return false;
  m_pAllocator->RemoveBlock(m_nBlockID);
  m_nBlockID = ~0;
  m_pTexture = NULL;
  m_nUpdateMask = 0;
  m_pAllocator = NULL;
  Unlink();

  return true;
}

void SDynTexture2::Apply(int nTUnit, int nTS)
{
  if (!m_pAllocator)
    return;
  m_pAllocator->m_fLastUsed = gRenDev->m_RP.m_RealTime;

  if (!m_pTexture)
    Update(m_nWidth, m_nHeight);
  if (m_pTexture)
    m_pTexture->Apply(nTUnit, nTS);
	gRenDev->m_cEF.m_RTRect.x = (float)(m_nX+0.5f) / (float)m_pTexture->GetWidth();
	gRenDev->m_cEF.m_RTRect.y = (float)(m_nY+0.5f) / (float)m_pTexture->GetHeight();
	gRenDev->m_cEF.m_RTRect.z = (float)(m_nWidth-1) / (float)m_pTexture->GetWidth();
	gRenDev->m_cEF.m_RTRect.w = (float)(m_nHeight-1) / (float)m_pTexture->GetHeight();
}

bool SDynTexture2::IsValid()
{
  if (!m_pTexture)
    return false;
  m_nAccessFrame = gRenDev->GetFrameID(false);
  if (m_nFrameReset != gRenDev->m_nFrameReset)
  {
		m_nFrameReset = gRenDev->m_nFrameReset;
    m_nUpdateMask = 0;
		return false;
	}
	if (gRenDev->IsMultiGPUModeActive())
  {
    if ((gRenDev->GetFeatures() & RFT_HW_MASK) == RFT_HW_ATI) 
    {
      uint32 nX, nY, nW, nH;
      GetImageRect(nX, nY, nW, nH);
      if (nW<1024 && nH<1024)
        return true;
    }

    int nFrame = gRenDev->m_nFrameSwapID;
    nFrame = nFrame % gRenDev->m_nGPUs;
    if (!((1<<nFrame) & m_nUpdateMask))
      return false;
  }
  return true;
}

