/*=============================================================================
  D3DTexturesStreaming.cpp : Direct3D9 specific texture streaming technology.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"

//===============================================================================

//#define TEXUSAGE D3DUSAGE_DYNAMIC
//#define TEXPOOL  D3DPOOL_DEFAULT

#define TEXUSAGE 0

#if defined (DIRECT3D9) || defined(OPENGL)

int CTexture::StreamSetLod(int nToMip, bool bUnload)
{
  if (bUnload && m_bWasUnloaded)
    return 0;
  if (!IsStreamed())
    return 0;

  if (m_nMinMipVidUploaded == nToMip)
    return 0;
  int nPrevMinMip = m_nMinMipVidUploaded;

  int nSides = (m_eTT == eTT_Cube) ? 6 : 1;

  // Make sure we don't have to request any async loading at this stage
  assert(m_Requested.Num() == 0);
  STexCacheMipHeader *mh = m_pFileTexMips;

  // If async streaming is in process we can't unload this mips
  for (int i=0; i<nSides; i++)
  {
    for (int j=nToMip; j<m_nMips3DC; j++)
    {
      if (!mh[j].m_Mips[i].m_bUploaded)
        return 0;
    }
  }

  m_nMinMipVidUploaded = nToMip;

  int nFreeSize = m_pFileTexMips[nPrevMinMip].m_Size - m_pFileTexMips[nToMip].m_Size;

  // Release video-memory
  if (m_eTT != eTT_Cube)
  {
    IDirect3DTexture9 *pID3DTexture = (IDirect3DTexture9*)m_pDeviceTexture;
#if !defined(XENON) && !defined(PS3)
    pID3DTexture->SetLOD(nToMip);
#endif
  }
  else
  {
    nFreeSize *= 6;
    IDirect3DCubeTexture9 *pID3DCubeTexture = (IDirect3DCubeTexture9*)m_pDeviceTexture;
#if !defined(XENON) && !defined(PS3)
    pID3DCubeTexture->SetLOD(nToMip);
#endif
  }

  m_nDeviceSize -= nFreeSize;
  CTexture::m_StatsCurManagedStreamedTexMem -= nFreeSize;

  StreamValidateTexSize();

  return nFreeSize;
}

bool CTexture::StreamCopyMips(int nStartMip, int nEndMip, bool bToDevice)
{
  CD3D9Renderer *r = gcpRendD3D;
  LPDIRECT3DDEVICE9 dv = r->GetD3DDevice();
  IDirect3DTexture9 *pID3DTexture = NULL;
  IDirect3DCubeTexture9 *pID3DCubeTexture = NULL;
  HRESULT h;
  D3DLOCKED_RECT d3dlr;
  int i;
  STexCacheMipHeader *mh = m_pFileTexMips;

  CTexture::m_bStreamingShow = true;

  if (m_eTT != eTT_Cube)
  {
    pID3DTexture = (IDirect3DTexture9*)m_pDeviceTexture;
    int nMips = m_nMips3DC;
    int wdt = m_nWidth;
    int hgt = m_nHeight;
    int wd = wdt;
    int hg = hgt;
    if (!pID3DTexture)
    {
      if(FAILED(h = dv->CreateTexture(wd, hg, nMips, TEXUSAGE, (D3DFORMAT)DeviceFormatFromTexFormat(GetDstFormat()), TEXPOOL, &pID3DTexture, NULL)))
      {
        assert(false);
        return false;
      }
      m_pDeviceTexture = pID3DTexture;
    }
    if (CRenderer::CV_r_texturesstreamingnoupload && bToDevice)
      return true;

    int SizeToLoad = 0;
    if (m_pPoolItem)
    {
      m_nMinMipVidUploaded = min(nStartMip, nMips-1);
      for (i=nStartMip; i<=nEndMip; i++)
      {
        int nLod = i-nStartMip;
        SMipData *mp = &mh[i].m_Mips[0];

        if (bToDevice)
        {
          CTexture::m_StatsCurManagedStreamedTexMem += mh[i].m_Size;
          CTexture::m_UploadBytes += mh[i].m_Size;
        }
#if !defined(XENON) && !defined(PS3)
        LPDIRECT3DSURFACE9 pDestSurf, pSrcSurf;
        h = pID3DTexture->GetSurfaceLevel(nLod, &pDestSurf); 
        pSrcSurf = (LPDIRECT3DSURFACE9)m_pPoolItem->m_pOwner->m_SysSurfaces[nLod];
        h = pSrcSurf->LockRect(&d3dlr, NULL, 0);
#else
        h = pID3DTexture->LockRect(i, &d3dlr, NULL, 0);
#endif
        assert(h == S_OK);
#ifdef _DEBUG
        if (mh[i].m_USize >= 4)
        {
          int nD3DSize;
          if (IsDXTCompressed(GetDstFormat()))
            nD3DSize = CTexture::TextureDataSize(mh[i].m_USize, mh[i].m_VSize, 1, 1, m_eTFDst);
          else
            nD3DSize = d3dlr.Pitch * mh[i].m_VSize;
          assert(nD3DSize == mh[i].m_Size);
        }
#endif
        SizeToLoad += mh[i].m_Size;
        bool bLoaded = false;
        if (mh[i].m_USize < 4 && !IsDXTCompressed(m_eTFDst))
        {
          int nD3DSize = d3dlr.Pitch * mh[i].m_VSize;
          if (nD3DSize != mh[nLod].m_Size)
          {
            bLoaded = true;
            byte *pDst = (byte *)d3dlr.pBits;
            byte *pSrc = &mp->DataArray[0];
            int nPitchSrc = CTexture::TextureDataSize(mh[i].m_USize, 1, 1, 1, m_eTFDst);
            if (bToDevice)
            {
              for (int j=0; j<mh[i].m_VSize; j++)
              {
                memcpy(pDst, pSrc, nPitchSrc);
                pSrc += nPitchSrc;
                pDst += d3dlr.Pitch;
              }
            }
            else
            {
              for (int j=0; j<mh[i].m_VSize; j++)
              {
                memcpy(pSrc, pDst, nPitchSrc);
                pSrc += nPitchSrc;
                pDst += d3dlr.Pitch;
              }
            }
          }
        }
        if (!bLoaded)
        {
          // Copy data to system texture 
          if (bToDevice)
            cryMemcpy((byte *)d3dlr.pBits, &mp->DataArray[0], mh[i].m_Size);
          else
            cryMemcpy(&mp->DataArray[0], (byte *)d3dlr.pBits, mh[i].m_Size);
        }
#if !defined(XENON) && !defined(PS3)
        // Unlock the system texture
        pSrcSurf->UnlockRect();
        if (bToDevice)
          h = dv->UpdateSurface(pSrcSurf, 0, pDestSurf, 0);
        else
          D3DXLoadSurfaceFromSurface(pSrcSurf, NULL, NULL, pDestSurf, NULL, NULL, D3DX_FILTER_NONE, 0);
        SAFE_RELEASE(pDestSurf);
#else
        pID3DTexture->UnlockRect(i);
#endif
        mp->m_bUploaded = true;
      }
    }
    else
    {
      m_nMinMipSysUploaded = nStartMip;
#if !defined(XENON) && !defined(PS3)
      m_nMinMipVidUploaded = nStartMip;
      pID3DTexture->SetLOD(m_nMinMipVidUploaded);
#endif
      if (nEndMip > nMips-1)
      {
        int n = max(nStartMip, nMips);
        for (i=n; i<m_nMips3DC; i++)
        {
          SMipData *mp = &mh[i].m_Mips[0];
          mp->m_bUploaded = true;
        }
        nEndMip = nMips-1;
      }
      for (i=nStartMip; i<=nEndMip; i++)
      {
        int nLod = i;
        assert(nLod >= 0);
        SMipData *mp = &mh[nLod].m_Mips[0];
        if (mp->m_bUploaded)
        {
#ifdef _DEBUG
          int nI = i+1;
          while (nI <= nEndMip)
          {
            SMipData *mp = &mh[nI].m_Mips[0];
            assert (mp->m_bUploaded);
            nI++;
          }
#endif
          break;
        }
        m_nDeviceSize += mh[nLod].m_Size;
        if (bToDevice)
        {
          CTexture::m_StatsCurManagedStreamedTexMem += mh[nLod].m_Size;
          CTexture::m_UploadBytes += mh[nLod].m_Size;
        }
        mp->m_bUploaded = true;
        // To avoid crash (Direct3D bug)
        if (m_eTFDst==eTF_3DC && ((m_nWidth>>i)<4 || (m_nHeight>>i)<4))
          continue;
        /*LPDIRECT3DSURFACE9 pSurf;
        D3DSURFACE_DESC dtdsd;
        h = pID3DTexture->GetSurfaceLevel(nLod, &pSurf);
        pSurf->GetDesc(&dtdsd);
        //assert(mp->USize==dtdsd.Width && mp->VSize==dtdsd.Height);
        SAFE_RELEASE(pSurf);*/

        h = pID3DTexture->LockRect(nLod, &d3dlr, NULL, 0);
        assert(h == S_OK);
#ifdef _DEBUG
        if (mh[nLod].m_USize >= 4)
        {
          int nD3DSize;
          if (IsDXTCompressed(m_eTFDst))
            nD3DSize = CTexture::TextureDataSize(mh[nLod].m_USize, mh[nLod].m_VSize, 1, 1, m_eTFDst);
          else
            nD3DSize = d3dlr.Pitch * mh[nLod].m_VSize;
          assert(nD3DSize == m_pFileTexMips[nLod].m_Size);
        }
#endif
        bool bLoaded = false;
        SizeToLoad += m_pFileTexMips[nLod].m_Size;
        if (mh[nLod].m_USize < 4 && !IsDXTCompressed(m_eTFDst))
        {
          int nD3DSize = d3dlr.Pitch * mh[nLod].m_VSize;
          if (nD3DSize != m_pFileTexMips[nLod].m_Size)
          {
            bLoaded = true;
            byte *pDst = (byte *)d3dlr.pBits;
            byte *pSrc = &mp->DataArray[0];
            int nPitchSrc = CTexture::TextureDataSize(mh[nLod].m_USize, 1, 1, 1, m_eTFDst);
            if (bToDevice)
            {
              for (int j=0; j<mh[nLod].m_VSize; j++)
              {
                memcpy(pDst, pSrc, nPitchSrc);
                pSrc += nPitchSrc;
                pDst += d3dlr.Pitch;
              }
            }
            else
            {
              for (int j=0; j<mh[nLod].m_VSize; j++)
              {
                memcpy(pSrc, pDst, nPitchSrc);
                pSrc += nPitchSrc;
                pDst += d3dlr.Pitch;
              }
            }
          }
        }
        if (!bLoaded)
        {
          // Copy data to video texture 
          if (bToDevice)
            cryMemcpy((byte *)d3dlr.pBits, &mp->DataArray[0], m_pFileTexMips[nLod].m_Size);
          else
            cryMemcpy(&mp->DataArray[0], (byte *)d3dlr.pBits, m_pFileTexMips[nLod].m_Size);
        }
        // Unlock the video texture
        h = pID3DTexture->UnlockRect(nLod);
        assert(h == S_OK);
      }
    }
    if (gRenDev->m_LogFileStr)
      gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Uploading mips '%s'. (Side: %d, %d-%d[%d]), Size: %d, Time: %.3f\n", m_SrcName.c_str(), -1, nStartMip, i-1, m_nMips3DC, SizeToLoad, iTimer->GetAsyncCurTime());
  }
  else
  {
    pID3DCubeTexture = (IDirect3DCubeTexture9*)m_pDeviceTexture;
    if (!pID3DCubeTexture)
    {
      if( FAILED( h = dv->CreateCubeTexture(m_nWidth, m_nMips, TEXUSAGE, (D3DFORMAT)DeviceFormatFromTexFormat(GetDstFormat()), TEXPOOL, &pID3DCubeTexture, NULL)))
      {
        assert(false);
        return true;
      }
      m_pDeviceTexture = pID3DCubeTexture;
    }
    if (!m_pPoolItem)
    {
#if !defined(XENON) && !defined(PS3)
      m_nMinMipVidUploaded = nStartMip;
      pID3DCubeTexture->SetLOD(nStartMip);
#endif
    }

    m_nMinMipSysUploaded = nStartMip;

    for (int n=0; n<6; n++)
    {
      int SizeToLoad = 0;
      if (m_pPoolItem)
      {
        for (i=nStartMip; i<=nEndMip; i++)
        {
          int nLod = i-nStartMip;
          SMipData *mp = &mh[i].m_Mips[n];

          if (bToDevice)
          {
            CTexture::m_StatsCurManagedStreamedTexMem += mh[i].m_Size;
            CTexture::m_UploadBytes += mh[i].m_Size;
          }
#if !defined(XENON) && !defined(PS3)
          LPDIRECT3DSURFACE9 pDestSurf, pSrcSurf;
          h = pID3DCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)n, nLod, &pDestSurf);
          pSrcSurf = (LPDIRECT3DSURFACE9)m_pPoolItem->m_pOwner->m_SysSurfaces[nLod];
          h = pSrcSurf->LockRect(&d3dlr, NULL, 0);
#else
          assert(0);
#endif
#ifdef _DEBUG
          int nD3DSize;
          if (IsDXTCompressed(GetDstFormat()))
            nD3DSize = (d3dlr.Pitch/4) * ((mh[i].m_VSize+3)&~3);
          else
            nD3DSize = d3dlr.Pitch * mh[i].m_VSize;
          assert(nD3DSize == m_pFileTexMips[i].m_Size);
#endif
          SizeToLoad += mh[i].m_Size;
          // Copy data to system texture 
          if (bToDevice)
            cryMemcpy((byte *)d3dlr.pBits, &mp->DataArray[0], mh[i].m_Size);
          else
            cryMemcpy(&mp->DataArray[0], (byte *)d3dlr.pBits, mh[i].m_Size);
          // Unlock the system texture
#if !defined(XENON) && !defined(PS3)
          pSrcSurf->UnlockRect();
          h = dv->UpdateSurface(pSrcSurf, 0, pDestSurf, 0);
          SAFE_RELEASE(pDestSurf);
#else
          assert(0);
#endif
          mp->m_bUploaded = true;
        }
      }
      else
      {
        for (i=nStartMip; i<=nEndMip; i++)
        {
          SMipData *mp = &mh[i].m_Mips[n];
          if (mp->m_bUploaded)
          {
#ifdef _DEBUG
            int nI = i+1;
            while (nI <= nEndMip)
            {
              SMipData *mp = &mh[i].m_Mips[n];
              assert (mp->m_bUploaded);
              nI++;
            }
#endif
            break;
          }
          if (!mp->DataArray.GetMemoryUsage())
            continue;
          if (bToDevice)
          {
            m_nDeviceSize += mh[i].m_Size;
            CTexture::m_StatsCurManagedStreamedTexMem += mh[i].m_Size;
            CTexture::m_UploadBytes += mh[i].m_Size;
          }
          h = pID3DCubeTexture->LockRect((D3DCUBEMAP_FACES)n, i, &d3dlr, NULL, 0);
#ifdef _DEBUG
          if (mh[i].m_USize >= 4)
          {
            int nD3DSize;
            if (IsDXTCompressed(GetDstFormat()))
              nD3DSize = (d3dlr.Pitch/4) * ((mh[i].m_VSize+3)&~3);
            else
              nD3DSize = d3dlr.Pitch * mh[i].m_VSize;
            assert(nD3DSize == mh[i].m_Size);
          }
#endif

          bool bLoaded = false;
          SizeToLoad += mh[i].m_Size;
          if (mh[i].m_USize < 4 && !IsDXTCompressed(m_eTFDst))
          {
            int nD3DSize = d3dlr.Pitch * mh[i].m_VSize;
            if (nD3DSize != m_pFileTexMips[i].m_Size)
            {
              bLoaded = true;
              byte *pDst = (byte *)d3dlr.pBits;
              byte *pSrc = &mp->DataArray[0];
              int nPitchSrc = CTexture::TextureDataSize(mh[i].m_USize, 1, 1, 1, m_eTFDst);
              if (bToDevice)
              {
                for (int j=0; j<mh[i].m_VSize; j++)
                {
                  memcpy(pDst, pSrc, nPitchSrc);
                  pSrc += nPitchSrc;
                  pDst += d3dlr.Pitch;
                }
              }
              else
              {
                for (int j=0; j<mh[i].m_VSize; j++)
                {
                  memcpy(pSrc, pDst, nPitchSrc);
                  pSrc += nPitchSrc;
                  pDst += d3dlr.Pitch;
                }
              }
            }
          }

          if (!bLoaded)
          {
            // Copy data to video texture 
            if (bToDevice)
              cryMemcpy((byte *)d3dlr.pBits, &mp->DataArray[0], mh[i].m_Size);
            else
              cryMemcpy(&mp->DataArray[0], (byte *)d3dlr.pBits, mh[i].m_Size);
          }

          // Unlock the video texture
          pID3DCubeTexture->UnlockRect((D3DCUBEMAP_FACES)n, i);
          mp->m_bUploaded = true;
        }
      }
      if (gRenDev->m_LogFileStr)
        gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Uploading mips '%s'. (Side: %d, %d-%d[%d]), Size: %d, Time: %.3f\n", m_SrcName.c_str(), n, nStartMip, i-1, m_nMips3DC, SizeToLoad, iTimer->GetAsyncCurTime());
    }
  }
  return true;
}

#elif defined (DIRECT3D10)

int CTexture::StreamSetLod(int nToMip, bool bUnload)
{
  if (bUnload && m_bWasUnloaded)
    return 0;
  if (!IsStreamed())
    return 0;

  if (m_nMinMipVidUploaded == nToMip)
    return 0;
  int nPrevMinMip = m_nMinMipVidUploaded;

  int nSides = (m_eTT == eTT_Cube) ? 6 : 1;

  if (bUnload)
  {
    // Make sure we don't have to request any async loading at this stage
    assert(m_Requested.Num() == 0);
  }

  STexCacheMipHeader *mh = m_pFileTexMips;

  if (bUnload)
  {
    // If async streaming is in process we can't unload this mips
    for (int i=0; i<nSides; i++)
    {
      for (int j=nToMip; j<m_nMips3DC; j++)
      {
        if (!mh[j].m_Mips[i].m_bUploaded)
          return 0;
      }
    }
  }

  m_nMinMipVidUploaded = nToMip;

  int nFreeSize = bUnload ? m_pFileTexMips[nPrevMinMip].m_Size - m_pFileTexMips[nToMip].m_Size : 0;

  CD3D9Renderer *r = gcpRendD3D;
  D3DDevice *dv = r->GetD3DDevice();
  HRESULT hr = S_OK;
  D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
  ZeroStruct(SRVDesc);
  SRVDesc.Format = (DXGI_FORMAT)DeviceFormatFromTexFormat(GetDstFormat());
  ID3D10ShaderResourceView *pSRV = (ID3D10ShaderResourceView *)m_pDeviceShaderResource;
  SAFE_RELEASE(pSRV);

  // Release video-memory
  if (m_eTT != eTT_Cube)
  {
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = m_nMips - nToMip;
    SRVDesc.Texture2D.MostDetailedMip = nToMip;
    hr = dv->CreateShaderResourceView((D3DTexture *)m_pDeviceTexture, &SRVDesc, &pSRV);
  }
  else
  {
    nFreeSize *= 6;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
    SRVDesc.TextureCube.MipLevels = m_nMips - nToMip;
    SRVDesc.TextureCube.MostDetailedMip = nToMip;
    hr = dv->CreateShaderResourceView((D3DTexture *)m_pDeviceTexture, &SRVDesc, &pSRV);
  }
  m_pDeviceShaderResource = pSRV;
  assert(hr == S_OK);

  m_nDeviceSize -= nFreeSize;
  CTexture::m_StatsCurManagedStreamedTexMem -= nFreeSize;

  StreamValidateTexSize();

  return nFreeSize;
}

bool CTexture::StreamCopyMips(int nStartMip, int nEndMip, bool bToDevice)
{
  CD3D9Renderer *r = gcpRendD3D;
  D3DDevice *dv = r->GetD3DDevice();
  D3DTexture *pID3DTexture = NULL;
  HRESULT h = S_OK;
  int i;
  STexCacheMipHeader *mh = m_pFileTexMips;

  CTexture::m_bStreamingShow = true;

  if (m_eTT != eTT_Cube)
  {
    pID3DTexture = (D3DTexture*)m_pDeviceTexture;
    int nMips = m_nMips3DC;
    int wdt = m_nWidth;
    int hgt = m_nHeight;
    if (!pID3DTexture)
    {
      D3D10_TEXTURE2D_DESC Desc;
      ZeroStruct(Desc);
      Desc.Width = wdt;
      Desc.Height = hgt;
      Desc.MipLevels = nMips;
      Desc.Format = (DXGI_FORMAT)DeviceFormatFromTexFormat(GetDstFormat());
      Desc.ArraySize = 1;
      Desc.BindFlags |= D3D10_BIND_SHADER_RESOURCE;
      Desc.CPUAccessFlags = 0;
      Desc.Usage = D3D10_USAGE_DEFAULT;
      Desc.SampleDesc.Count = 1;
      Desc.SampleDesc.Quality = 0;
      Desc.MiscFlags = 0;
      h = dv->CreateTexture2D(&Desc, NULL, (ID3D10Texture2D **)&pID3DTexture);

      m_pDeviceTexture = pID3DTexture;
    }
    assert(h == S_OK);

    if (CRenderer::CV_r_texturesstreamingnoupload && bToDevice)
      return true;

    int SizeToLoad = 0;
    if (m_pPoolItem)
    {
      assert(0);
    }
    else
    {
      m_nMinMipSysUploaded = nStartMip;
      StreamSetLod(nStartMip, false);
      if (nEndMip > nMips-1)
      {
        int n = max(nStartMip, nMips);
        for (i=n; i<m_nMips3DC; i++)
        {
          SMipData *mp = &mh[i].m_Mips[0];
          mp->m_bUploaded = true;
        }
        nEndMip = nMips-1;
      }
      for (i=nStartMip; i<=nEndMip; i++)
      {
        int nLod = i;
        assert(nLod >= 0);
        SMipData *mp = &mh[nLod].m_Mips[0];
        if (mp->m_bUploaded)
        {
#ifdef _DEBUG
          int nI = i+1;
          while (nI <= nEndMip)
          {
            SMipData *mp = &mh[nI].m_Mips[0];
            assert (mp->m_bUploaded);
            nI++;
          }
#endif
          break;
        }
        m_nDeviceSize += mh[nLod].m_Size;
        if (bToDevice)
        {
          CTexture::m_StatsCurManagedStreamedTexMem += mh[nLod].m_Size;
          CTexture::m_UploadBytes += mh[nLod].m_Size;
        }
        mp->m_bUploaded = true;

        SizeToLoad += m_pFileTexMips[nLod].m_Size;
        byte *pSrc = &mp->DataArray[0];
        int nPitchSrc = CTexture::TextureDataSize(mh[nLod].m_USize, 1, 1, 1, m_eTFDst);
        if (bToDevice)
        {
          int nRowPitch = CTexture::TextureDataSize(mh[nLod].m_USize, 1, 1, 1, m_eTFDst);
          D3D10_BOX rc = {0, 0, 0, mh[nLod].m_USize, mh[nLod].m_VSize, 1};
          if (m_eTFSrc == eTF_A8R8G8B8 || m_eTFSrc == eTF_X8R8G8B8 || m_eTFSrc == eTF_R8G8B8)
          {
            assert(mh[nLod].m_Size == mp->DataArray.size());
            byte *pNew = new byte[mp->DataArray.size()];
            int nSize = mh[nLod].m_USize * mh[nLod].m_VSize;
            for (int i=0; i<nSize; i++)
            {
              pNew[i*4+0] = pSrc[i*4+2];
              pNew[i*4+1] = pSrc[i*4+1];
              pNew[i*4+2] = pSrc[i*4+0];
              pNew[i*4+3] = pSrc[i*4+3];
            }
            pSrc = pNew;
          }
          dv->UpdateSubresource(pID3DTexture, nLod, &rc, pSrc, nRowPitch, 0);
          if (pSrc != &mp->DataArray[0])
            delete [] pSrc;
        }
        else
        {
          assert(0);
        }
        // Unlock the video texture
        assert(h == S_OK);
      }
    }
    if (gRenDev->m_LogFileStr)
      gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Uploading mips '%s'. (Side: %d, %d-%d[%d]), Size: %d, Time: %.3f\n", m_SrcName.c_str(), -1, nStartMip, i-1, m_nMips3DC, SizeToLoad, iTimer->GetAsyncCurTime());
  }
  else
  {
    pID3DTexture = (D3DTexture*)m_pDeviceTexture;
    if (!pID3DTexture)
    {
      D3D10_TEXTURE2D_DESC Desc;
      ZeroStruct(Desc);
      Desc.Width = m_nWidth;
      Desc.Height = m_nHeight;
      Desc.MipLevels = m_nMips;
      Desc.Format = (DXGI_FORMAT)DeviceFormatFromTexFormat(GetDstFormat());
      Desc.ArraySize = 6;
      Desc.BindFlags |= D3D10_BIND_SHADER_RESOURCE;
      Desc.CPUAccessFlags = 0;
      Desc.Usage = D3D10_USAGE_DEFAULT;
      Desc.SampleDesc.Count = 1;
      Desc.SampleDesc.Quality = 0;
      Desc.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;
      h = dv->CreateTexture2D(&Desc, NULL, (ID3D10Texture2D **)&pID3DTexture);

      m_pDeviceTexture = pID3DTexture;
    }
    assert(h == S_OK);

    if (!m_pPoolItem)
    {
      StreamSetLod(nStartMip, false);
    }

    m_nMinMipSysUploaded = nStartMip;

    for (int n=0; n<6; n++)
    {
      int SizeToLoad = 0;
      if (m_pPoolItem)
      {
        assert(0);
      }
      else
      {
        for (i=nStartMip; i<=nEndMip; i++)
        {
          SMipData *mp = &mh[i].m_Mips[n];
          if (mp->m_bUploaded)
          {
#ifdef _DEBUG
            int nI = i+1;
            while (nI <= nEndMip)
            {
              SMipData *mp = &mh[i].m_Mips[n];
              assert (mp->m_bUploaded);
              nI++;
            }
#endif
            break;
          }
          if (!mp->DataArray.GetMemoryUsage())
            continue;
          if (bToDevice)
          {
            m_nDeviceSize += mh[i].m_Size;
            CTexture::m_StatsCurManagedStreamedTexMem += mh[i].m_Size;
            CTexture::m_UploadBytes += mh[i].m_Size;
          }

          SizeToLoad += mh[i].m_Size;
          // Copy data to video texture 
          if (bToDevice)
          {
            int nRowPitch = CTexture::TextureDataSize(mh[i].m_USize, 1, 1, 1, m_eTFDst);
            D3D10_BOX rc = {0, 0, 0, mh[i].m_USize, mh[i].m_VSize, 1};
            dv->UpdateSubresource(pID3DTexture, n*m_nMips+i, &rc, &mp->DataArray[0], nRowPitch, 0);
          }
          else
          {
            assert(0);
          }
          mp->m_bUploaded = true;
        }
      }
      if (gRenDev->m_LogFileStr)
        gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Uploading mips '%s'. (Side: %d, %d-%d[%d]), Size: %d, Time: %.3f\n", m_SrcName.c_str(), n, nStartMip, i-1, m_nMips3DC, SizeToLoad, iTimer->GetAsyncCurTime());
    }
  }
  return true;
}
#endif

// Copy device texture to system mips
bool CTexture::StreamRestoreMipsData()
{
  bool bResult = true;
  int nEndMip = m_nMips3DC-m_CacheFileHeader.m_nMipsPersistent-1;

  if (m_nMinMipSysUploaded >= nEndMip)
  {
    float time0 = iTimer->GetAsyncCurTime();
    bResult = StreamCopyMips(m_nMinMipSysUploaded, nEndMip, false);

    gcpRendD3D->m_RP.m_PS.m_fTexRestoreTime += iTimer->GetAsyncCurTime() - time0;
  }

  return bResult;
}


// Just remove item from the texture object and keep Item in Pool list for future use
// This function doesn't release API texture
void CTexture::StreamRemoveFromPool()
{
  if (!m_pPoolItem)
    return;
  STexPoolItem *pIT = m_pPoolItem;
  m_pPoolItem = NULL;
  pIT->m_pTex = NULL;
  pIT->m_fLastTimeUsed = gRenDev->m_RP.m_RealTime;
  m_nDeviceSize = (uint)-1;
  m_pDeviceTexture = NULL;
}

bool CTexture::StreamAddToPool(int nStartMip, int nMips)
{
  assert (nStartMip<m_nMips);
  STexCacheMipHeader *mh = m_pFileTexMips;
  STexPool *pPool = NULL;

  if (m_pPoolItem)
  {
    if (m_pPoolItem->m_pOwner->m_nMips == nMips && m_pPoolItem->m_pOwner->m_Width == mh[nStartMip].m_USize && m_pPoolItem->m_pOwner->m_Height == mh[nStartMip].m_VSize)
    {
      assert(mh[nStartMip].m_Mips[0].m_bUploaded);
      return false;
    }
    STexPoolItem *pIT = m_pPoolItem;
    StreamRemoveFromPool();
    pIT->LinkFree(&CTexture::m_FreeTexPoolItems);
    int nSides = 1;
    if (m_eTT == eTT_Cube)
      nSides = 6;
    for (int j=0; j<nSides; j++)
    {
      for (int i=0; i<m_nMips; i++)
      {
        mh[i].m_Mips[j].m_bUploaded = false;
      }
    }
    /*IDirect3DBaseTexture9 *pTex = (IDirect3DBaseTexture9*)pIT->m_pAPITexture;
    SAFE_RELEASE(pTex);
    pIT->Unlink();
    gRenDev->m_TexMan->m_StatsCurTexMem -= pIT->m_pOwner->m_Size;
    delete pIT;*/
  }
  pPool = CTexture::StreamCreatePool(mh[nStartMip].m_USize, mh[nStartMip].m_VSize, nMips, GetDstFormat(), m_eTT);
  if (!pPool)
    return false;

  // Try to find empty item in the pool
  STexPoolItem *pIT = pPool->m_ItemsList.m_Next;
  while (pIT != &pPool->m_ItemsList)
  {
    if (!pIT->m_pTex)
      break;
    pIT = pIT->m_Next;
  }
  if (pIT != &pPool->m_ItemsList)
  {
    pIT->UnlinkFree();
    CTexture::m_StatsCurManagedStreamedTexMem -= pPool->m_Size;
  }
  else
  {
    pIT = new STexPoolItem;
    pIT->m_pOwner = pPool;
    pIT->Link(&pPool->m_ItemsList);

    // Create API texture for the item in DEFAULT pool
#if defined (DIRECT3D9) || defined(OPENGL)
    HRESULT h = S_OK;
    IDirect3DCubeTexture9 *pID3DCubeTexture = NULL;
    IDirect3DTexture9 *pID3DTexture = NULL;
    if (m_eTT != eTT_Cube)
    {
      if(FAILED(h=gcpRendD3D->m_pd3dDevice->CreateTexture(mh[nStartMip].m_USize, mh[nStartMip].m_VSize, nMips, TEXUSAGE, (D3DFORMAT)DeviceFormatFromTexFormat(GetDstFormat()), D3DPOOL_DEFAULT, &pID3DTexture, NULL)))
      {
        assert(0);
        return false;
      }
      pIT->m_pAPITexture = pID3DTexture;
    }
    else
    {
      if(FAILED(h=gcpRendD3D->m_pd3dDevice->CreateCubeTexture(mh[nStartMip].m_USize, nMips, TEXUSAGE, (D3DFORMAT)DeviceFormatFromTexFormat(GetDstFormat()), D3DPOOL_DEFAULT, &pID3DCubeTexture, NULL)))
      {
        assert(0);
        return false;
      }
      pIT->m_pAPITexture = pID3DCubeTexture;
    }
#elif defined (DIRECT3D10)
    assert(0);
#endif
  }
  m_nDeviceSize = pPool->m_Size;
  m_pPoolItem = pIT;
  pIT->m_pTex = this;
  m_pDeviceTexture = pIT->m_pAPITexture;

  return true;
}

STexPool *CTexture::StreamCreatePool(int nWidth, int nHeight, int nMips, ETEX_Format eTF, ETEX_Type eTT)
{
  int i;
  STexPool *pPool = NULL;

  for (i=0; i<m_TexPools.Num(); i++)
  {
    pPool = m_TexPools[i];
    if (pPool->m_nMips == nMips && pPool->m_Width == nWidth && pPool->m_Height == nHeight && pPool->m_eFormat == eTF && pPool->m_eTT == eTT)
      break;
  }
  // Create new pool
  if (i == m_TexPools.Num())
  {
    pPool = new STexPool;
    pPool->m_eTT = eTT;
    pPool->m_eFormat = eTF;
    pPool->m_Width = nWidth;
    pPool->m_Height = nHeight;
    pPool->m_nMips = nMips;
    pPool->m_Size = CTexture::TextureDataSize(nWidth, nHeight, 1, nMips, eTF);
    if (eTT == eTT_Cube)
      pPool->m_Size *= 6;

#if !defined(XENON) && !defined(PS3)
#if defined (DIRECT3D9) || defined(OPENGL)
    HRESULT h = S_OK;
    IDirect3DTexture9 *pID3DTexture = NULL;
    if(FAILED(h=gcpRendD3D->m_pd3dDevice->CreateTexture(nWidth, nHeight, nMips, TEXUSAGE, (D3DFORMAT)CTexture::DeviceFormatFromTexFormat(eTF), D3DPOOL_SYSTEMMEM, &pID3DTexture, NULL)))
      return NULL;
    pPool->m_pSysTexture = pID3DTexture;
    for (i=0; i<nMips; i++)
    {
      LPDIRECT3DSURFACE9 pDestSurf;
      h = pID3DTexture->GetSurfaceLevel(i, &pDestSurf);
      pPool->m_SysSurfaces.AddElem(pDestSurf);
    }
#elif defined (DIRECT3D10)
    assert(0);
#endif
    pPool->m_SysSurfaces.Shrink();
#endif

    pPool->m_ItemsList.m_Next = &pPool->m_ItemsList;
    pPool->m_ItemsList.m_Prev = &pPool->m_ItemsList;
    CTexture::m_TexPools.AddElem(pPool);
  }

  return pPool;
}

void CTexture::CreatePools()
{
#if defined (DIRECT3D9) || defined(OPENGL)
  int nMaxTexSize = gcpRendD3D->m_pd3dDevice->GetAvailableTextureMem();
#elif defined (DIRECT3D10)
  int nMaxTexSize = 128*1024*1024;
#endif
  int i = 10;
  int j, n;

  while (i)
  {
    int wdt = (1<<i);
    int hgt = (1<<i);
    int nTexSize = 0;
    nTexSize += CTexture::TextureDataSize(wdt, hgt, 1, 1, eTF_DXT1);
    nTexSize += CTexture::TextureDataSize(wdt, hgt, 1, 1, eTF_DXT3);
    nTexSize += CTexture::TextureDataSize(wdt, hgt, 1, 1, eTF_A8R8G8B8);
    int nH = i-1;
    if (nH < 0)
      nH = 0;
    hgt = (1<<nH);
    nTexSize += CTexture::TextureDataSize(wdt, hgt, 1, 1, eTF_DXT1);
    nTexSize += CTexture::TextureDataSize(wdt, hgt, 1, 1, eTF_DXT3);
    nTexSize += CTexture::TextureDataSize(wdt, hgt, 1, 1, eTF_A8R8G8B8);
    n = nMaxTexSize / nTexSize / i / 2;
    for (j=0; j<n; j++)
    {
      StreamCreatePool(1<<i, 1<<i, i, eTF_DXT1, eTT_2D);
      StreamCreatePool(1<<i, 1<<i, i, eTF_DXT3, eTT_2D);
      StreamCreatePool(1<<i, 1<<i, i, eTF_A8R8G8B8, eTT_2D);

      StreamCreatePool(1<<i, 1<<nH, i, eTF_DXT1, eTT_2D);
      StreamCreatePool(1<<i, 1<<nH, i, eTF_DXT3, eTT_2D);
      StreamCreatePool(1<<i, 1<<nH, i, eTF_A8R8G8B8, eTT_2D);
    }

    i--;
  }
}

void CTexture::StreamUnloadOldTextures(CTexture *pExclude)
{
  if (!CRenderer::CV_r_texturesstreampoolsize)
    return;
  //ValidateTexSize();

  // First try to release freed texture pool items
  STexPoolItem *pIT = CTexture::m_FreeTexPoolItems.m_PrevFree;
  while (pIT != &CTexture::m_FreeTexPoolItems)
  {
    assert (!pIT->m_pTex);
    STexPoolItem *pITNext = pIT->m_PrevFree;
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
    IDirect3DBaseTexture9 *pTex = (IDirect3DBaseTexture9*)pIT->m_pAPITexture;
    SAFE_RELEASE(pTex);
#endif
    pIT->Unlink();
    pIT->UnlinkFree();
    m_StatsCurManagedStreamedTexMem -= pIT->m_pOwner->m_Size;
    delete pIT;

    pIT = pITNext;
    if (m_StatsCurManagedStreamedTexMem < CRenderer::CV_r_texturesstreampoolsize*1024*1024)
      break;
  }

  // Second: release old texture objects
  CTexture *pTP = CTexture::m_Root.m_Prev;
  if (m_StatsCurManagedStreamedTexMem >= CRenderer::CV_r_texturesstreampoolsize*1024*1024)
  {
    while (m_StatsCurManagedStreamedTexMem >= CRenderer::CV_r_texturesstreampoolsize*1024*1024)
    {
      if (pTP == &CTexture::m_Root)
      {
        ICVar *var = iConsole->GetCVar("r_TexturesStreamPoolSize");
        var->Set(m_StatsCurManagedStreamedTexMem/(1024*1024)+30);
        iLog->Log("WARNING: Texture pool was changed to %d Mb", CRenderer::CV_r_texturesstreampoolsize);
        return;
      }

      CTexture *Next = pTP->m_Prev;
      if (pTP != pExclude)
        pTP->StreamUnload();
      pTP = Next;
    }
  }
}


void CTexture::StreamPrecacheAsynchronously(float fDist, int Flags)
{
  if (gRenDev->m_bDeviceLost)
    return;

  PROFILE_FRAME(Texture_Precache);
  if (CTexture::m_nStreamed & 1)
  {
    if (m_bWasUnloaded || m_bPartyallyLoaded)
    {
      if (!IsStreamed())
        return;
      //if (!(Flags & FPR_NEEDLIGHT) && m_eTT == eTT_Bumpmap)
      //  return;
      if (CRenderer::CV_r_logTexStreaming >= 2)
        gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Precache '%s' (Stream)\n", m_SrcName.c_str());
      StreamLoadFromCache(Flags, fDist);
    }
    else
    {
      if (CRenderer::CV_r_logTexStreaming >= 2)
        gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Precache '%s' (Skip)\n", m_SrcName.c_str());
    }
  }
  if (CTexture::m_nStreamed & 2)
  {
    if (CRenderer::CV_r_logTexStreaming >= 2)
      gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Precache '%s' (Draw)\n", m_SrcName.c_str());
    gcpRendD3D->EF_SetState(GS_COLMASK_NONE | GS_NODEPTHTEST);
    Apply(0);

    int nOffs;
    struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *vTri = (struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *)gcpRendD3D->GetVBPtr(POOL_TRP3F_COL4UB_TEX2F, nOffs);
    DWORD col = (DWORD)-1;

    // Define the triangle
    vTri[0].pos.x = 0;
    vTri[0].pos.y = 0;
    vTri[0].pos.z = 1;
    vTri[0].pos.w = 1.0f;
    vTri[0].color.dcolor = col;
    vTri[0].st[0] = 0;
    vTri[0].st[1] = 0;

    vTri[1].pos.x = 2;
    vTri[1].pos.y = 0;
    vTri[1].pos.z = 1;
    vTri[1].pos.w = 1.0f;
    vTri[1].color.dcolor = col;
    vTri[1].st[0] = 1;
    vTri[1].st[1] = 0;

    vTri[2].pos.x = 2;
    vTri[2].pos.y = 2;
    vTri[2].pos.z = 1;
    vTri[2].pos.w = 1.0f;
    vTri[2].color.dcolor = col;
    vTri[2].st[0] = 1;
    vTri[2].st[1] = 1;

    gcpRendD3D->UnlockVB(POOL_TRP3F_COL4UB_TEX2F);

    gcpRendD3D->FX_SetVStream(0, gcpRendD3D->m_pVB[POOL_TRP3F_COL4UB_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F));
    if (!FAILED(gcpRendD3D->EF_SetVertexDeclaration(0, VERTEX_FORMAT_TRP3F_COL4UB_TEX2F)))
    {
  #if defined (DIRECT3D9) || defined(OPENGL)
      gcpRendD3D->m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, nOffs, 1);
  #elif defined (DIRECT3D10)
      gcpRendD3D->SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      gcpRendD3D->m_pd3dDevice->Draw(3, nOffs);
  #endif
    }
    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);
  }
}


void CTexture::StreamCheckTexLimits(CTexture *pExclude)
{
  StreamValidateTexSize();
  if (!m_nStreamed)
    return;

  // To avoid textures thrashing we will try to predict it using some statistics
  // and using 4-Phase textures releasing scheme
  m_TexSizeHistory[m_nTexSizeHistory & 7] = CTexture::m_StatsCurManagedStreamedTexMem;
  m_nTexSizeHistory++;
  int n = (m_nTexSizeHistory-8) & 7;
  int nMinTexSize = m_TexSizeHistory[n];
  for (int i=n+1; (i&7)!=n; i++)
  {
    i &= 7;
    nMinTexSize = min(m_TexSizeHistory[i], nMinTexSize);
  }
  bool bStreamReleaseMode = false;
  int nPoolSize = CRenderer::CV_r_texturesstreampoolsize*1024*1024;
  if (nMinTexSize < (int)(nPoolSize*0.8f))
  {
    if (nMinTexSize < (int)(nPoolSize*0.5f))
    {
      if (m_fAdaptiveStreamDistRatio > 0.4f)
        m_fAdaptiveStreamDistRatio *= 0.85f;
      if (m_fAdaptiveStreamDistRatio < 0.4f)
        m_fAdaptiveStreamDistRatio = 0.4f;
    }
    else
    {
      if (m_fAdaptiveStreamDistRatio > 1.0f)
        m_fAdaptiveStreamDistRatio *= 0.85f;
      if (m_fAdaptiveStreamDistRatio < 1.0f)
        m_fAdaptiveStreamDistRatio = 1.0f;
    }
  }
  else
  if (nMinTexSize > (int)(nPoolSize*0.9f))
  {
    if (m_fAdaptiveStreamDistRatio < 4.0f)
      m_fAdaptiveStreamDistRatio *= 1.25f;
  }
  else
  if (m_fAdaptiveStreamDistRatio > 2.5f)
    bStreamReleaseMode = true;

  if (!m_nPhaseProcessingTextures)
    m_nPhaseProcessingTextures = 1;

  // Phase1: delete old freed pool items
  if (m_nPhaseProcessingTextures == 1)
  {
    float fTime = gRenDev->m_RP.m_RealTime;
    int n = 20;
    STexPoolItem *pIT = CTexture::m_FreeTexPoolItems.m_PrevFree;
    while (pIT != &CTexture::m_FreeTexPoolItems)
    {
      assert (!pIT->m_pTex);
      STexPoolItem *pITNext = pIT->m_PrevFree;
      if (fTime-pIT->m_fLastTimeUsed > 2.0f)
      {
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
        IDirect3DBaseTexture9 *pTex = (IDirect3DBaseTexture9*)pIT->m_pAPITexture;
        SAFE_RELEASE(pTex);
        pIT->Unlink();
        pIT->UnlinkFree();
        m_StatsCurManagedStreamedTexMem -= pIT->m_pOwner->m_Size;
        nMinTexSize -= pIT->m_pOwner->m_Size;
        delete pIT;
        n--;
#endif
      }
      pIT = pITNext;
      if (!n || nMinTexSize <= (int)(nPoolSize*0.88f))
        break;
    }
    if (nMinTexSize <= (int)(nPoolSize*0.88f))
      m_nPhaseProcessingTextures = 0;
    else
    if (pIT == &CTexture::m_FreeTexPoolItems)
    {
      m_nProcessedTextureID1 = 0;
      m_pProcessedTexture1 = NULL;
      m_nPhaseProcessingTextures = 2;
    }
  }

  // Phase2: unload old textures (which was used at least 2 secs ago)
  if (m_nPhaseProcessingTextures == 2)
  {
    CTexture *tp;
    if (!m_nProcessedTextureID1 || !(tp=CTexture::GetByID(m_nProcessedTextureID1)) || !tp->m_Prev || tp != m_pProcessedTexture1)
      tp = CTexture::m_Root.m_Prev;
    int nFrame = gRenDev->m_nFrameUpdateID;
    int nWeight = 0;
    while (tp != &CTexture::m_Root)
    {
      CTexture *Next = tp->m_Prev;
      if (tp->m_nAccessFrameID && nFrame-tp->m_nAccessFrameID > 100 && tp->IsStreamed())
      {
        nWeight += 10;
        tp->StreamUnload();
      }
      tp = Next;
      nWeight++;
      if (nWeight > 40)
        break;
    }
    if (tp == &CTexture::m_Root)
      m_nPhaseProcessingTextures = 0;
    else
    {
      m_nProcessedTextureID1 = tp->GetTextureID();
      m_pProcessedTexture1 = tp;
    }
  }

  // Phase3: reallocate video memory based on current priorities
  {
    CTexture *tp;
    if (!m_nProcessedTextureID2 || !(tp=CTexture::GetByID(m_nProcessedTextureID2)) || !tp->m_Next || tp != m_pProcessedTexture2)
      tp = CTexture::m_Root.m_Next;
    int nFrame = gRenDev->m_nFrameUpdateID;
    int nWeight = 0;
    while (tp != &CTexture::m_Root)
    {
      CTexture *Next = tp->m_Next;
      if (tp->m_nAccessFrameID && nFrame-tp->m_nAccessFrameID < 80 && tp->IsStreamed())
      {
        if (tp->StreamSetLod(tp->m_nMinMipCur, true))
          nWeight += 10;
      }
      nWeight++;
      if (nWeight > 40)
        break;
      tp = Next;
    }
    if (tp != &CTexture::m_Root)
    {
      m_nProcessedTextureID2 = tp->GetTextureID();
      m_pProcessedTexture2 = tp;
    }
    else
      m_nProcessedTextureID2 = 0;
  }

  if (!(CTexture::m_nStreamed & 1))
    return;
  if (CRenderer::CV_r_texturesstreampoolsize < 10)
    CRenderer::CV_r_texturesstreampoolsize = 10;

  StreamValidateTexSize();

  // Usually we do it in case of textures thrashing
  if (CTexture::m_StatsCurManagedStreamedTexMem >= CRenderer::CV_r_texturesstreampoolsize*1024*1024 || bStreamReleaseMode)
    StreamUnloadOldTextures(pExclude);
}

