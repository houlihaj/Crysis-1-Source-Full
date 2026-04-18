/*=============================================================================
  TextureStreaming.cpp : Common Texture Streaming manager implementation.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/


#include "StdAfx.h"
#include "../CommonRender.h"
#include "Image/CImage.h"
#include "Image/DDSImage.h"
#include "StringUtils.h"								// stristr()

int CTexture::m_nTexSizeHistory;
int CTexture::m_TexSizeHistory[8];
int CTexture::m_nPhaseProcessingTextures;
int CTexture::m_nProcessedTextureID1;
CTexture *CTexture::m_pProcessedTexture1;
int CTexture::m_nProcessedTextureID2;
CTexture *CTexture::m_pProcessedTexture2;
TArray<CTexture *> CTexture::m_Requested;

bool CTexture::m_bStreamOnlyVideo;
bool CTexture::m_bStreamDontKeepSystem;

int CTexture::m_LoadBytes;
int CTexture::m_UploadBytes;
float CTexture::m_fAdaptiveStreamDistRatio;
float CTexture::m_fResolutionRatio;
int CTexture::m_StatsCurManagedStreamedTexMem;
int CTexture::m_StatsCurManagedNonStreamedTexMem;
int CTexture::m_StatsCurDynamicTexMem;

CTexture CTexture::m_Root;

// DXT compressed block for black image (DXT1, DXT3, DXT5)
static byte sDXTData[3][16] = 
{
  {0,0,0,0,0,0,0,0},
  {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, 0,0,0,0,0,0,0,0},
  {0xff,0xff,0,0,0,0,0,0, 0,0,0,0,0,0,0,0}
};

void CTexture::StreamReleaseMipsData(int nStartMip, int nEndMip)
{
  STexCacheMipHeader *mh = m_pFileTexMips;
  int nSides = (m_eTT == eTT_Cube) ? 6 : 1;
  int nEnd = m_nMips - m_CacheFileHeader.m_nMipsPersistent - 1;
  if (nEndMip > nEnd)
    nEndMip = nEnd;
  for (int i=0; i<nSides; i++)
  {
    for (int j=nStartMip; j<=nEndMip; j++)
    {
      mh[j].m_Mips[i].DataArray.Free();
    }
  }
}

int CTexture::StreamUnload()
{
  if (m_bWasUnloaded)
    return 0;
  if (!IsStreamed())
    return 0;

  if (CTexture::m_nStreamed & 1)
  {
    int nDevSize = m_nDeviceSize;
    if (gRenDev->m_LogFileStr)
      gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Unload '%s', Time: %.3f\n", m_SrcName.c_str(), iTimer->GetAsyncCurTime());
    ReleaseDeviceTexture(true);
    m_bWasUnloaded = true;
    m_bStreamingInProgress = false;
    return nDevSize;
  }
  return 0;
}

void CTexture::StreamRestore()
{
  if (CTexture::m_nStreamed & 1)
  {
    StreamLoadFromCache(FPR_IMMEDIATELLY, gRenDev->m_RP.m_fMinDistance);
    return;
  }
}

void CTexture::StreamGetCacheName(char *name)
{
  char *sETT;
  switch (m_eTT)
  {
    case eTT_2D:
      sETT = "2D";
      break;
    case eTT_3D:
      sETT = "3D";
      break;
    case eTT_Cube:
      sETT = "Cube";
      break;
  }
  char nam[512];
  if (!strncmp("Spr_$", m_SrcName.c_str(), 5))
    strcpy(nam, m_SrcName.c_str());
  else
    fpStripExtension(m_SrcName.c_str(), nam);
  sprintf(name, "%s[%s]", nam, sETT); 
}

bool CTexture::StreamValidateMipsData(int nStartMip, int nEndMip)
{
  int i, j;
  bool bOk = true;
  STexCacheMipHeader *mh = m_pFileTexMips;
  if (m_eTT == eTT_Cube)
  {
    for (j=0; j<6; j++)
    {
      for (i=nStartMip; i<=nEndMip; i++)
      {
        SMipData *mp = &mh[i].m_Mips[j];
        if (!m_bStreamOnlyVideo || m_bStreamDontKeepSystem)
        {
          if (mp->m_bUploaded)
            continue;
        }
        if (!mp->DataArray.GetMemoryUsage())
        {
          bOk = false;
          break;
        }
      }
    }
  }
  else
  {
    for (i=nStartMip; i<=nEndMip; i++)
    {
      SMipData *mp = &mh[i].m_Mips[0];
      if (!m_bStreamOnlyVideo || m_bStreamDontKeepSystem)
      {
        if (mp->m_bUploaded)
          continue;
      }
      if (!mp->DataArray.GetMemoryUsage())
      {
        bOk = false;
        break;
      }
    }
  }
  return bOk;
}

void CTextureStreamCallback::StreamOnComplete (IReadStream* pStream, unsigned nError)
{
  PROFILE_FRAME(Texture_StreamAsyncUpload);

  int i, j;
  STexCacheInfo *pTexCacheFileInfo = (STexCacheInfo *)pStream->GetUserData();
  CCryName Name = CTexture::mfGetClassName();
  SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
  assert(pRL);
  if (pRL->m_RList.size() <= pTexCacheFileInfo->m_TexId || !gRenDev)
	{
	  SAFE_DELETE(pTexCacheFileInfo);
    return;
	}
  CTexture *tp = CTexture::GetByID(pTexCacheFileInfo->m_TexId);
  if (!tp || !tp->m_pFileTexMips || !tp->IsStreamed())
  {
    //Warning( VALIDATOR_FLAG_TEXTURE,tp->GetName(),"CTextureStreamCallback::StreamOnComplete texture is missing %s",tp->GetName() );
		SAFE_DELETE(pTexCacheFileInfo);
    return;
  }
  int n = 0;
  byte *Src = (byte *)pStream->GetBuffer();
  STexCacheFileHeader *fh = &tp->m_CacheFileHeader;
  STexCacheMipHeader *mh = tp->m_pFileTexMips;
  int nMips = tp->GetNumDevMips();
  int nSide = pTexCacheFileInfo->m_nCubeSide;
  int nS = nSide < 0 ? 0 : nSide;
  if (gRenDev->m_LogFileStr)
    gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Async Finish Load '%s' (Side: %d, %d-%d[%d]), Size: %d, Time: %.3f\n", tp->GetSourceName(), nS, pTexCacheFileInfo->m_nStartLoadMip, pTexCacheFileInfo->m_nEndLoadMip, tp->GetNumMips(), pTexCacheFileInfo->m_nSizeToLoad, iTimer->GetAsyncCurTime());
  for (i=pTexCacheFileInfo->m_nStartLoadMip; i<=pTexCacheFileInfo->m_nEndLoadMip; i++)
  {
    SMipData *mp = &mh[i].m_Mips[nS];
    //assert(!mp->m_bUploaded);
    if (!mp->m_bLoading)
    {
      // Was unloaded
      if (!mp->m_bUploaded)
      {
        assert(!tp->GetDeviceTexture());
        assert(tp->IsUnloaded());
      }
		  SAFE_DELETE(pTexCacheFileInfo);
      return;
    }

    mp->m_bLoading = false;
    if (!mp->m_bUploaded || (CTexture::m_bStreamOnlyVideo && !mp->DataArray.GetMemoryUsage())) // Already uploaded synchronously
    {
      float time0 = iTimer->GetAsyncCurTime();

      if (!mp->DataArray.GetMemoryUsage())
        mp->DataArray.Reserve(mh[i].m_Size);
      if (tp->GetSrcFormat() == eTF_R8G8B8 && tp->IsStreamFromDDS())
      {
        byte *pTemp = new byte[mh[i].m_USize*mh[i].m_VSize*3];
        cryMemcpy(pTemp, &Src[n], mh[i].m_USize*mh[i].m_VSize*3);
        for (int n=0; n<mh[i].m_USize*mh[i].m_VSize; n++)
        {
          mp->DataArray[n*4+0] = pTemp[n*3+0];
          mp->DataArray[n*4+1] = pTemp[n*3+1];
          mp->DataArray[n*4+2] = pTemp[n*3+2];
          mp->DataArray[n*4+3] = 255;
        }
        delete [] pTemp;
      }
      else
        cryMemcpy(&mp->DataArray[0], &Src[n], mh[i].m_Size);

      gRenDev->m_RP.m_PS.m_fTexUploadTime += iTimer->GetAsyncCurTime() - time0;
    }
    if (tp->GetSrcFormat() == eTF_R8G8B8 && tp->IsStreamFromDDS())
      n += mh[i].m_USize*mh[i].m_VSize*3;
    else
      n += mh[i].m_Size;
  }
  tp->StreamValidateTexSize();
  bool bNeedToWait = false;
  if (nSide >= 0)
  {
    if (tp->GetFlags() & (FT_REPLICATE_TO_ALL_SIDES | FT_FORCE_CUBEMAP))
    {
      for (j=1; j<6; j++)
      {
        for (int i=pTexCacheFileInfo->m_nStartLoadMip; i<=pTexCacheFileInfo->m_nEndLoadMip; i++)
        {
          SMipData *mp = &mh[i].m_Mips[j];
          if (!mp->DataArray.GetMemoryUsage())
            mp->DataArray.Reserve(mh[i].m_Size);
          if (tp->GetFlags() & FT_REPLICATE_TO_ALL_SIDES)
            cryMemcpy(&mp->DataArray[0], &mh[i].m_Mips[0].DataArray[0], mp->DataArray.GetMemoryUsage());
          else
          if (tp->GetFlags() & FT_FORCE_CUBEMAP)
          {
            if (tp->IsDXTCompressed(tp->GetSrcFormat()))
            {
              int nBlocks = ((mh[i].m_USize+3)/4)*((mh[i].m_VSize+3)/4);
              int blockSize = tp->GetSrcFormat() == eTF_DXT1 ? 8 : 16;
              int nOffsData = tp->GetSrcFormat() - eTF_DXT1;
              int nCur = 0;
              for (int n=0; n<nBlocks; n++)
              {
                cryMemcpy(&mp->DataArray[nCur], sDXTData[nOffsData], blockSize);
                nCur += blockSize;
              }
            }
            else
              memset(&mp->DataArray[0], 0, mp->DataArray.GetMemoryUsage());
          }
        }
      }
    }

    for (j=0; j<6; j++)
    {
      for (i=pTexCacheFileInfo->m_nStartLoadMip; i<=pTexCacheFileInfo->m_nEndLoadMip; i++)
      {
        SMipData *mp = &mh[i].m_Mips[j];
        if (!mp->DataArray.GetMemoryUsage())
        {
          bNeedToWait = true;
          break;
        }
      }
      if (i <= pTexCacheFileInfo->m_nEndLoadMip)
        break;
    }
  }
  if (!bNeedToWait)
  {
    bool bOk = tp->StreamValidateMipsData(pTexCacheFileInfo->m_nStartLoadMip, nMips-1);
    if (!bOk)
    {
      tp->StreamReleaseMipsData(0, nMips);
      tp->SetWasUnload(true);
    }
    else
    {
      bool bRes = tp->StreamUploadMips(pTexCacheFileInfo->m_nStartLoadMip, nMips-1);
      if (!pTexCacheFileInfo->m_nStartLoadMip && bRes)
        tp->StreamReleaseMipsData(0, nMips);
      tp->SetWasUnload(false);

      if (!mh[0].m_Mips[0].m_bUploaded)
        tp->SetPartyallyLoaded(true);
      else
        tp->SetPartyallyLoaded(false);
    }
    tp->Relink(&CTexture::m_Root);
    //CTexture::StreamCheckTexLimits(NULL);
  }
  SAFE_DELETE(pTexCacheFileInfo);
  tp->SetStreamingInProgress(false);

  CTexture::StreamValidateTexSize();
}

void CTexture::StreamLoadSynchronously(int nStartMip, int nEndMip)
{
  int i, j;
  int nMips, nSides;
  FILE *fp = NULL;
  int nOffsSide = 0;
  STexCacheFileHeader *fh = &m_CacheFileHeader;
  STexCacheMipHeader *mh = m_pFileTexMips;
  nSides = fh->m_nSides;
  nMips = m_nMips3DC;
  int nSize;

  if (nStartMip >= 0)
  {
    PROFILE_FRAME(Texture_StreamSync);

    assert(nStartMip <= nEndMip);
    if (nSides > 1)
      nSize = m_nSize/6;
    else
      nSize = m_nSize;
    //assert (Size > nSeekFromStart);
    assert (nSize == m_pFileTexMips[0].m_SizeWithMips);

    for (j=0; j<nSides; j++)
    {
      int SizeToLoad = 0;
      for (i=nStartMip; i<=nEndMip; i++)
      {
        if (mh[i].m_Mips[j].DataArray.GetMemoryUsage() || mh[i].m_Mips[j].m_bUploaded)
        {
          i++;
          break;
        }
        SMipData *mp = &mh[i].m_Mips[j];
        if (!mp->DataArray.GetMemoryUsage())
          mp->DataArray.Reserve(mh[i].m_Size);
        assert(!mp->m_bUploaded);
        CTexture::m_LoadBytes += mh[i].m_Size;
        SizeToLoad += mh[i].m_Size;
        if (!fp)
        {
          fp = iSystem->GetIPak()->FOpen(m_SrcName.c_str(), "rb");
          if (!fp)
            break;
        }
        iSystem->GetIPak()->FSeek(fp, m_pFileTexMips[i].m_Seek+nOffsSide, SEEK_SET);
        if (GetSrcFormat() == eTF_R8G8B8 && m_bStreamFromDDS)
        {
          byte *pTemp = new byte[mh[i].m_USize*mh[i].m_VSize*3];
          iSystem->GetIPak()->FRead(pTemp, mh[i].m_USize*mh[i].m_VSize*3, fp);
          for (int n=0; n<mh[i].m_USize*mh[i].m_VSize; n++)
          {
            mp->DataArray[n*4+0] = pTemp[n*3+0];
            mp->DataArray[n*4+1] = pTemp[n*3+1];
            mp->DataArray[n*4+2] = pTemp[n*3+2];
            mp->DataArray[n*4+3] = 255;
          }
          delete [] pTemp;
        }
        else
        if (!j || !(m_nFlags & (FT_REPLICATE_TO_ALL_SIDES | FT_FORCE_CUBEMAP)))
          iSystem->GetIPak()->FRead(&mp->DataArray[0], mh[i].m_Size, fp);
        else
        if (j)
        {
          if (m_nFlags & FT_REPLICATE_TO_ALL_SIDES)
            memcpy(&mp->DataArray[0], &mh[i].m_Mips[0].DataArray[0], mp->DataArray.GetMemoryUsage());
          else
          if (m_nFlags & FT_FORCE_CUBEMAP)
          {
            if (IsDXTCompressed(GetSrcFormat()))
            {
              int nBlocks = ((mh[i].m_USize+3)/4)*((mh[i].m_VSize+3)/4);
              int blockSize = GetSrcFormat() == eTF_DXT1 ? 8 : 16;
              int nOffsData = GetSrcFormat() - eTF_DXT1;
              int nCur = 0;
              for (int n=0; n<nBlocks; n++)
              {
                memcpy(&mp->DataArray[nCur], sDXTData[nOffsData], blockSize);
                nCur += blockSize;
              }
            }
            else
              memset(&mp->DataArray[0], 0, mp->DataArray.GetMemoryUsage());
          }
        }
      }
      if (gRenDev->m_LogFileStr && SizeToLoad > 0)
        gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Sync Load '%s' (Side: %d, %d-%d[%d]), Size: %d, Time: %.3f\n", m_SrcName.c_str(), j, nStartMip, i-1, nMips, SizeToLoad, iTimer->GetAsyncCurTime());
      nOffsSide += nSize;
    }
    if (fp)
      iSystem->GetIPak()->FClose(fp);
    bool bRes;
    { 
      PROFILE_FRAME(Texture_LoadFromCache_UploadSync);
      bRes = StreamUploadMips(nStartMip, nMips-1);
    }
    if (!nStartMip && bRes)
      StreamReleaseMipsData(0, nMips);
    m_bWasUnloaded = false;
    if (!mh[0].m_Mips[0].m_bUploaded)
      m_bPartyallyLoaded = true;
    else
      m_bPartyallyLoaded = false;
    //CTexture::CheckTexLimits(this);
  }
}

void CTexture::StreamUpdateMip(float fDist)
{
  fDist *= 0.5f;
  if (fDist < 0)
  {
    if (m_nFrameCache != gRenDev->m_nFrameUpdateID)
      fDist = 0;
    else
      fDist = m_fMinDistance;
  }
  else
    fDist *= (m_fAdaptiveStreamDistRatio + m_fResolutionRatio);
  if (m_nFrameCache != gRenDev->m_nFrameUpdateID)
  {
    m_nFrameCache = gRenDev->m_nFrameUpdateID;
    m_fMinDistance = fDist;
  }
  else
  if (fDist < m_fMinDistance)
    m_fMinDistance = fDist;
  else
    return;

  int nMip = fastround_positive(log(max(fDist, 1.0f)));

  int nResMip = 0;
  if ((m_nWidth > 64 || m_nHeight > 64) && !CRenderer::CV_r_texturesstreamingsync)
  {
    nResMip = min(m_nMips3DC-1, nResMip);
    if (0 != nResMip)
      ReleaseDeviceTexture(true);
  }
  nMip = min(m_nMips3DC-1, nMip+nResMip);
  m_nMinMipCur = nMip;
}

bool CTexture::StreamLoadFromCache(int Flags, float fDist)
{
  int i, j;

  PROFILE_FRAME(Texture_Stream);

  StreamUpdateMip(fDist);

  if (!m_pFileTexMips)
  {
    assert(0);
  }

  STexCacheMipHeader *mh = m_pFileTexMips;

  if (mh[m_nMinMipCur].m_Mips[0].m_bUploaded)
    return true;
  CTexture::StreamValidateTexSize();

  int nEndMip = m_nMips3DC-1;
  int n = nEndMip;
  int nSyncStartMip = -1;
  int nSyncEndMip = -1;
  int nASyncStartMip = -1;
  int nASyncEndMip = -1;
  if (m_nMips == 1 || CRenderer::CV_r_texturesstreamingsync)
  {
    m_nMinMipCur = 0;
    nSyncStartMip = m_nMinMipCur;
    nSyncEndMip = nEndMip;
  }
  else
  {
    // Always stream lowest nMipsPersistent mips synchronously
    int nStartLowestM = max(m_nMinMipCur, nEndMip-m_CacheFileHeader.m_nMipsPersistent+1);
    for (i=nStartLowestM; i<=nEndMip; i++)
    {
      if (!mh[i].m_Mips[0].m_bUploaded)
      {
        nSyncStartMip = i;
        for (j=i+1; j<=nEndMip; j++)
        {
          if (mh[j].m_Mips[0].m_bUploaded)
            break;
        }
        nSyncEndMip = j-1;
        break;
      }
    }
    // Let's see which part of the texture not loaded yet and stream it asynhronously
    if (nStartLowestM > m_nMinMipCur)
    {
      for (i=m_nMinMipCur; i<=nStartLowestM-1; i++)
      {
        if (!mh[i].m_Mips[0].m_bLoading && !mh[i].m_Mips[0].m_bUploaded)
        {
          nASyncStartMip = i;
          break;
        }
      }
      if (nASyncStartMip >= 0 && nASyncStartMip <= nStartLowestM-1)
      {
        for (i=nASyncStartMip; i<=nStartLowestM-1; i++)
        {
          if (mh[i].m_Mips[0].m_bUploaded || mh[i].m_Mips[0].m_bLoading)
            break;
        }
        nASyncEndMip = i-1;
      }
    }
  }
  if (nASyncStartMip < 0 && nSyncStartMip < 0)
    return true;

  Relink(&CTexture::m_Root);

  // Synchronous loading
  StreamLoadSynchronously(nSyncStartMip, nSyncEndMip);

  // Asynchronous request
  if (nASyncStartMip >= 0 && CRenderer::CV_r_texturesstreamingignore == 0)
  {
    assert(nASyncEndMip >= nASyncStartMip);
    assert(!mh[nASyncStartMip].m_Mips[0].m_bUploaded && !mh[nASyncStartMip].m_Mips[0].m_bLoading);
    if (mh[nASyncStartMip].m_Mips[0].m_bUploaded)
      return true;

    if (!m_bStreamRequested || m_nAsyncStartMip>nASyncStartMip)
    {
      m_nAsyncStartMip = nASyncStartMip;
      if (!m_bStreamRequested)
      {
        m_bStreamRequested = true;
        m_Requested.AddElem(this);
      }
    }
  }
  //int nH = IsHeapValid();
  //assert (nH == 1);

  return true;
}

// For now we support only .dds files streaming
bool CTexture::StreamPrepare(bool bReload, ETEX_Format eTFDst)
{
  if (!(m_nStreamed & 1))
    return false;

  if (m_nFlags & FT_DONT_STREAM)
    return false;

  FILE *fp = NULL;
  uint32 dwSize = 0;
  char szName[1024];

  fp = iSystem->GetIPak()->FOpen(m_SrcName.c_str(), "rb");
  if (!fp)
  {
    const char *szExt = fpGetExtension(m_SrcName.c_str());
    if (szExt && !stricmp(szExt, ".tif"))
    {
      strcpy(szName, m_SrcName.c_str());
      fpStripExtension(szName, szName);
      fpAddExtension(szName, ".dds");
      fp = iSystem->GetIPak()->FOpen(szName, "rb");
      if (fp)
        m_SrcName = szName;
    }
  }
  if ((m_nFlags & FT_TEX_NORMAL_MAP) && !CryStringUtils::stristr(m_SrcName.c_str(), "_ddn"))
  {
    if (fp)
    {
      iSystem->GetIPak()->FClose(fp);
      fp = NULL;
    }
    iLog->Log("Warning: streaming of heightmap textures (%s) are not supported (texture name should contain _ddn suffix)", m_SrcName.c_str());
  }

  StreamGetCacheName(szName);
  fpConvertDOSToUnixName(szName, szName);

  if (!fp)
    return false;

  int nSides, nMips;
  CImageFile *pIM = NULL;
  {
    STexCacheFileHeader fh;
    pIM = CImageFile::mfLoad_file(fp, ((m_nFlags & FT_ALPHA) ? FIM_ALPHA : 0) | FIM_LAST4MIPS);
    iSystem->GetIPak()->FClose(fp);
    // Can't stream volume textures yet
    if (pIM && pIM->mfGet_error() == eIFE_OK && pIM->mfGetFormat() != eTF_Unknown && pIM->mfGet_depth() <= 1 && pIM->mfGet_numMips() > 1)
    {
      if (m_nFlags & FT_ALPHA)
      {
        if (!pIM->mfGet_image(0))
        {
          assert(!m_pDeviceTexture);
          m_pDeviceTexture = m_Text_White->m_pDeviceTexture;
#if defined (DIRECT3D10)
          m_pDeviceShaderResource = m_Text_White->m_pDeviceShaderResource;
#endif
          m_nDefState = m_Text_White->m_nDefState;
          m_nFlags = m_Text_White->GetFlags();
          m_eTFDst = m_Text_White->GetDstFormat();
          m_nMips = m_Text_White->GetNumMips();
          m_nWidth = m_Text_White->GetWidth();
          m_nHeight = m_Text_White->GetHeight();
          m_nDepth = 1;
          m_nDefState = m_Text_White->m_nDefState;
          m_bNoTexture = true;
          SAFE_DELETE(pIM);
          return false;
        }
      }
#if defined (DIRECT3D10)
      if (pIM->mfIs_image(1) || pIM->mfGet_depth()>1 || (m_nFlags & FT_FORCE_CUBEMAP))
      {
        SAFE_DELETE(pIM);
        return false;
      }
#endif

      m_bStreamFromDDS = true;
      int i, j;
      fh.m_SizeOf = sizeof(STexCacheFileHeader);
      nMips = pIM->mfGet_numMips();
      fh.m_nMipsPersistent = min(4, nMips);
      fh.m_Version = TEXCACHE_VERSION;
      m_eTFSrc = pIM->mfGetSrcFormat();
      m_eTFDst = pIM->mfGetFormat();
      m_AvgColor = pIM->mfGet_AvgColor();
      strcpy(fh.m_sETF, CTexture::NameForTextureFormat(m_eTFSrc));
      if (pIM->mfIs_image(1) || (m_nFlags & FT_FORCE_CUBEMAP))
        m_eTT = eTT_Cube;
      else
      if (pIM->mfGet_depth() > 1)
        m_eTT = eTT_3D;
      else
        m_eTT = eTT_2D;
      fh.m_nSides = m_eTT == eTT_Cube ? 6 : 1;
      fh.m_DstFormat = m_eTFDst;
      m_CacheFileHeader = fh;
      nSides = m_CacheFileHeader.m_nSides;
      m_pFileTexMips = new STexCacheMipHeader[nMips];
      int wdt = pIM->mfGet_width();
      int hgt = pIM->mfGet_height();
      int nSeek = pIM->mfGet_StartSeek();
      for (i=0; i<nMips; i++)
      {
        assert(wdt || hgt);
        if (!wdt) wdt = 1;
        if (!hgt) hgt = 1;
        m_pFileTexMips[i].m_USize = wdt;
        m_pFileTexMips[i].m_VSize = hgt;
        if (CTexture::IsDXTCompressed(m_eTFSrc))
        {
          m_pFileTexMips[i].m_USize = (m_pFileTexMips[i].m_USize + 3) & ~3;
          m_pFileTexMips[i].m_VSize = (m_pFileTexMips[i].m_VSize + 3) & ~3;
        }
        m_pFileTexMips[i].m_Size = CTexture::TextureDataSize(wdt, hgt, 1, 1, m_eTFDst);
        m_pFileTexMips[i].m_SizeOf = sizeof(STexCacheMipHeader);
        m_pFileTexMips[i].m_Seek = nSeek;
        nSeek += CTexture::TextureDataSize(wdt, hgt, 1, 1, m_eTFSrc);
        wdt >>= 1;
        hgt >>= 1;
      }
      for (i=0; i<nMips; i++)
      {
        m_pFileTexMips[i].m_SizeWithMips = 0;
        for (j=i; j<nMips; j++)
        {
          m_pFileTexMips[i].m_SizeWithMips += m_pFileTexMips[j].m_Size;
        }
      }
      m_nWidth = m_pFileTexMips[0].m_USize;
      m_nHeight = m_pFileTexMips[0].m_VSize;
      m_nSrcWidth = m_nWidth;
      m_nSrcHeight = m_nHeight;
      m_nDepth = pIM->mfGet_depth();
      m_nMips = nMips;
      m_nFlags |= FT_FROMIMAGE;
      m_bWasUnloaded = true;
      m_bUseDecalBorderCol = (pIM->mfGet_Flags() & FIM_DECAL) != 0;
    }
    else
    {
      SAFE_DELETE(pIM);
      return false;
    }
  }

  assert (m_eTFDst != eTF_Unknown);

  SetTexStates();
  PostCreate();

  // Always load lowest nMipsPersistent mips synchronously
  byte *buf = NULL;
  if (m_nMips > 1)
  {
    int i, j;

    STexCacheFileHeader *fh = &m_CacheFileHeader;
    STexCacheMipHeader *mh = m_pFileTexMips;
    nSides = fh->m_nSides;
    nMips = m_nMips;
    int nSyncStartMip = -1;
    int nSyncEndMip = -1;
    int nEndMip = m_nMips-1;
    int nStartLowestM = max(0, nEndMip-fh->m_nMipsPersistent+1);
    for (i=nStartLowestM; i<=nEndMip; i++)
    {
      if (!mh[i].m_Mips[0].m_bUploaded)
      {
        nSyncStartMip = i;
        for (j=i+1; j<=nEndMip; j++)
        {
          if (mh[j].m_Mips[0].m_bUploaded)
            break;
        }
        nSyncEndMip = j-1;
        break;
      }
    }

    int nOffs = 0;
    if (nSyncStartMip >= 0)
    {
      assert(nSyncStartMip <= nSyncEndMip);

      for (j=0; j<nSides; j++)
      {
        int SizeToLoad = 0;
        nOffs = 0;
        buf = pIM->mfGet_image(j);
        for (i=nSyncStartMip; i<=nSyncEndMip; i++)
        {
          if (mh[i].m_Mips[j].DataArray.GetMemoryUsage() || mh[i].m_Mips[j].m_bUploaded)
          {
            i++;
            break;
          }
          SMipData *mp;
          mp = &mh[i].m_Mips[j];
          if (!mp->DataArray.GetMemoryUsage())
            mp->DataArray.Reserve(mh[i].m_Size);
          assert(!mp->m_bUploaded);
          CTexture::m_LoadBytes += mh[i].m_Size;
          SizeToLoad += mh[i].m_Size;
          if (buf && (!j || !(m_nFlags & (FT_REPLICATE_TO_ALL_SIDES | FT_FORCE_CUBEMAP))))
          {
            cryMemcpy(&mp->DataArray[0], &buf[nOffs], mh[i].m_Size);
            nOffs += mh[i].m_Size;
          }
          else
          if (j)
          {
            if (m_nFlags & FT_REPLICATE_TO_ALL_SIDES)
              memcpy(&mp->DataArray[0], &mh[i].m_Mips[0].DataArray[0], mp->DataArray.GetMemoryUsage());
            else
            if (m_nFlags & FT_FORCE_CUBEMAP)
            {
              if (IsDXTCompressed(GetSrcFormat()))
              {
                int nBlocks = ((mh[i].m_USize+3)/4)*((mh[i].m_VSize+3)/4);
                int blockSize = GetSrcFormat() == eTF_DXT1 ? 8 : 16;
                int nOffsData = GetSrcFormat() - eTF_DXT1;
                int nCur = 0;
                for (int n=0; n<nBlocks; n++)
                {
                  memcpy(&mp->DataArray[nCur], sDXTData[nOffsData], blockSize);
                  nCur += blockSize;
                }
              }
              else
                memset(&mp->DataArray[0], 0, mp->DataArray.GetMemoryUsage());
            }
          }
        }
      }
    }
  }
  SAFE_DELETE(pIM);

  m_bStreamPrepared = true;

  return true;
}

//=========================================================================

bool CTexture::StreamValidateTexSize()
{
#if defined(_DEBUG) && !defined(PS3)
  int nSizeManagedStreamed = 0;
  int nSizeManagedNonStreamed = 0;
  int nSizeDynamic = 0;
  CCryName Name = CTexture::mfGetClassName();
  SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
  ResourcesMapItor itor;
  if (!pRL)
    return true;
  for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
  {
    CTexture *tp = (CTexture *)itor->second;
    if (!tp->IsDynamic())
    {
      if (tp->IsStreamed())
        nSizeManagedStreamed += tp->GetDeviceDataSize();
      else
        nSizeManagedNonStreamed += tp->GetDeviceDataSize();
    }
    else
      nSizeDynamic += tp->GetDeviceDataSize();
  }
  STexPoolItem *pIT = CTexture::m_FreeTexPoolItems.m_PrevFree;
  while (pIT != &CTexture::m_FreeTexPoolItems)
  {
    nSizeManagedStreamed += pIT->m_pOwner->m_Size;
    pIT = pIT->m_PrevFree;
  }

  assert(nSizeManagedStreamed == m_StatsCurManagedStreamedTexMem);
  assert(nSizeManagedNonStreamed == m_StatsCurManagedNonStreamedTexMem);
  assert(nSizeDynamic == m_StatsCurDynamicTexMem);
#endif
  return true;
}

inline bool CompareItemDist(CTexture *a, CTexture *b)
{
  return (*((int *)&a->m_fMinDistance) < *((int *)&b->m_fMinDistance));
}

void CTexture::AsyncRequestsProcessing()
{
  int i, j, n;
  CTexture *pTex;
  int nSize;
  n = 0;

  PROFILE_FRAME(Texture_StreamASyncRequests);

  IStreamEngine *pSE = iSystem->GetStreamEngine();
  int nCurSizeToLoad = 0;
  if (m_Requested.Num())
  {
    m_bStreamingShow = true;
    std::sort(&m_Requested[0], &m_Requested[0]+m_Requested.Num(), CompareItemDist);
    for (n=0; n<m_Requested.Num(); n++)
    {
      pTex = m_Requested[n];
      if (!pTex)
        continue;
      STexCacheMipHeader *mh = pTex->m_pFileTexMips;
      if (!mh)
        continue;
      assert (pTex->m_bStreamRequested);
      assert (pTex->m_nAsyncStartMip >= 0);
      int nASyncStartMip, nASyncEndMip;
      nASyncStartMip = pTex->m_nAsyncStartMip;
      if (nASyncStartMip < 0)
        continue;
      int nSides = pTex->m_CacheFileHeader.m_nSides;
      int nMips = pTex->m_nMips3DC;
      for (j=nASyncStartMip; j<nMips; j++)
      {
        if (mh[j].m_Mips[0].m_bUploaded || mh[j].m_Mips[0].m_bLoading)
          break;
      }
      nASyncEndMip = j-1;
      // we can upload mips synchronously meanwhile (during sprite-gen for example).
      // assert(nASyncStartMip <= nASyncEndMip);
      int nSeekFromStart = mh[nASyncStartMip].m_Seek;
      if (nSides > 1)
        nSize = pTex->m_nSize/6;
      else
        nSize = pTex->m_nSize;
      pTex->m_bStreamRequested = false;
      for (j=0; j<nSides; j++)
      {
        for (i=nASyncStartMip; i<=nASyncEndMip; i++)
        {
          if (mh[i].m_Mips[j].m_bLoading || mh[i].m_Mips[j].m_bUploaded)
            break;
          if (pTex->GetSrcFormat() == eTF_R8G8B8 && pTex->m_bStreamFromDDS)
            nCurSizeToLoad += mh[i].m_USize*mh[i].m_VSize*3;
          else
            nCurSizeToLoad += mh[i].m_Size;
        }
      }
      if (CRenderer::CV_r_texturesstreamingmaxasync)
      {
        if ((float)nCurSizeToLoad > CRenderer::CV_r_texturesstreamingmaxasync*1024.0f*1024.0f && n)
          break;
      }
      for (j=0; j<nSides; j++)
      {
        int nSizeToLoad = 0;
        for (i=nASyncStartMip; i<=nASyncEndMip; i++)
        {
          if (mh[i].m_Mips[j].m_bLoading || mh[i].m_Mips[j].m_bUploaded)
            break;
          mh[i].m_Mips[j].m_bLoading = true;
          if (pTex->GetSrcFormat() == eTF_R8G8B8 && pTex->m_bStreamFromDDS)
            nSizeToLoad += mh[i].m_USize*mh[i].m_VSize*3;
          else
            nSizeToLoad += mh[i].m_Size;
        }
        if (j && (pTex->m_nFlags & (FT_FORCE_CUBEMAP | FT_REPLICATE_TO_ALL_SIDES)))
          continue;

        if (nSizeToLoad)
        {
          i--;
          if (gRenDev->m_LogFileStr)
            gRenDev->LogStrv(SRendItem::m_RecurseLevel, "Async Start Load '%s' (Side: %d, %d-%d[%d]), Size: %d, Time: %.3f\n", pTex->m_SrcName.c_str(), j, nASyncStartMip, i, nMips, nSizeToLoad, iTimer->GetAsyncCurTime());
          CTexture::m_LoadBytes += nSizeToLoad;
          STexCacheInfo *pTexCacheFileInfo = new STexCacheInfo;
          pTexCacheFileInfo->m_TexId = pTex->GetTextureID();
          pTexCacheFileInfo->m_pTempBufferToStream = new byte[nSizeToLoad];
          byte *buf = (byte *)pTexCacheFileInfo->m_pTempBufferToStream;
          pTexCacheFileInfo->m_nStartLoadMip = nASyncStartMip;
          pTexCacheFileInfo->m_nEndLoadMip = i;
          pTexCacheFileInfo->m_nSizeToLoad = nSizeToLoad;
          pTexCacheFileInfo->m_nCubeSide = nSides > 1 ? j : -1;
          pTexCacheFileInfo->m_fStartTime = iTimer->GetCurrTime();
          pTex->m_bStreamingInProgress = true;
          StreamReadParams StrParams;
          StrParams.nOffset = nSeekFromStart;
          StrParams.dwUserData = (DWORD_PTR)pTexCacheFileInfo;
          StrParams.nLoadTime = 1;
          StrParams.nMaxLoadTime = 4;
          StrParams.nPriority = 0;
          StrParams.pBuffer = pTexCacheFileInfo->m_pTempBufferToStream;
          StrParams.nSize = nSizeToLoad;
          IReadStreamPtr pStream = pSE->StartRead("Renderer", pTex->m_SrcName.c_str(), &pTexCacheFileInfo->m_Callback, &StrParams);
        }
        nSeekFromStart += nSize;
      }
    }
  }
  for (; n<m_Requested.Num(); n++)
  {
    m_Requested[n]->m_bStreamRequested = false;
  }
  m_Requested.SetNum(0);

  StreamCheckTexLimits(NULL);
}

bool CTexture::StreamUploadMips(int nStartMip, int nEndMip)
{
  float time0 = iTimer->GetAsyncCurTime();

  if (nEndMip < 0)
    return true;
  if (nStartMip < 0)
    nStartMip = 0;

  assert(nStartMip < m_nMinMipSysUploaded);
  if (m_bStreamOnlyVideo)
  {
    // Restore system mips from 
    if (m_bStreamDontKeepSystem)
      StreamRestoreMipsData();

    if (!StreamAddToPool(nStartMip, m_nMips-nStartMip))
      return true;
    if (!m_pPoolItem)
      return true;
  }

  StreamCopyMips(nStartMip, nEndMip, true);

  if (m_bStreamDontKeepSystem)
    StreamReleaseMipsData(nStartMip, nEndMip);

  StreamValidateTexSize();

  gRenDev->m_RP.m_PS.m_fTexUploadTime += iTimer->GetAsyncCurTime() - time0;

  return true;
}

void CTexture::InitStreaming()
{
  iLog->Log("Init textures management (%d Mb of texture memory)...", gRenDev->m_MaxTextureMemory/1024/1024);
  m_nStreamed = 1;

  static ICVar *pPak = gEnv->pConsole->GetCVar("sys_LowSpecPak");						assert(pPak);
  bool bLowSpecPak = !pPak ? false : pPak->GetIVal()!=0;

  // unless user explicitly wants so
  if (CRenderer::CV_r_texturesstreaming == 0)
    m_nStreamed=0;

  SSystemGlobalEnvironment *pEnv = iSystem->GetGlobalEnvironment();

  // auto mode
  if (CRenderer::CV_r_texturesstreaming == 2)
  {
    // or we have enough memory resources to handle
    if (sizeof(void*)==8 && gRenDev->m_MaxTextureMemory >= 500*1024*1024)
      m_nStreamed=0;

#ifdef WIN32
    // if we have low res textures we should consider
    if (bLowSpecPak && gRenDev->m_MaxTextureMemory >= 200*1024*1024 && pEnv->pi.numCoresAvailableToProcess == 1)
    {
      // no vista
      if (pEnv->pi.winVer!=SPlatformInfo::WinVista)
        m_nStreamed = 0;

      // vista 32bit with patch installed
      if (pEnv->pi.winVer==SPlatformInfo::WinVista && !pEnv->pi.win64Bit && !pEnv->pi.vistaKB940105Required)
        m_nStreamed = 0;

      // 64-bit Vista + 64 bit Crysis
      if (pEnv->pi.winVer==SPlatformInfo::WinVista && pEnv->pi.win64Bit && sizeof(void*) == 8) // potentially add sizeof(void*) == 8 if you want to say 
        m_nStreamed = 0;
    }                                
    if (m_nStreamed)
      iLog->Log("  Textures streaming automatically enabled (Configuration: %d cores, OS: '%s', %d bits OS)...", pEnv->pi.numCoresAvailableToProcess, pEnv->pi.winVer==SPlatformInfo::WinVista ? "Vista" : pEnv->pi.winVer==SPlatformInfo::WinXP ? "XP" : pEnv->pi.winVer==SPlatformInfo::Win2000 ? "Win2000" : pEnv->pi.winVer==SPlatformInfo::WinSrv2003 ? "Windows Server 2003" : "Unknown", pEnv->pi.win64Bit ? 64 : 32);
    else
      iLog->Log("  Textures streaming automatically disabled (Configuration: %d cores, OS: '%s', %d bits OS)...", pEnv->pi.numCoresAvailableToProcess, pEnv->pi.winVer==SPlatformInfo::WinVista ? "Vista" : pEnv->pi.winVer==SPlatformInfo::WinXP ? "XP" : pEnv->pi.winVer==SPlatformInfo::Win2000 ? "Win2000" : pEnv->pi.winVer==SPlatformInfo::WinSrv2003 ? "Windows Server 2003" : "Unknown", pEnv->pi.win64Bit ? 64 : 32);
#endif
  }
  else
  {
#ifdef WIN32
    if (m_nStreamed)
      iLog->Log("  Textures streaming forced on (Configuration: %d cores, OS: '%s', %d bits OS)...", pEnv->pi.numCoresAvailableToProcess, pEnv->pi.winVer==SPlatformInfo::WinVista ? "Vista" : pEnv->pi.winVer==SPlatformInfo::WinXP ? "XP" : pEnv->pi.winVer==SPlatformInfo::Win2000 ? "Win2000" : pEnv->pi.winVer==SPlatformInfo::WinSrv2003 ? "Windows Server 2003" : "Unknown", pEnv->pi.win64Bit ? 64 : 32);
    else
      iLog->Log("  Textures streaming forced off (Configuration: %d cores, OS: '%s', %d bits OS)...", pEnv->pi.numCoresAvailableToProcess, pEnv->pi.winVer==SPlatformInfo::WinVista ? "Vista" : pEnv->pi.winVer==SPlatformInfo::WinXP ? "XP" : pEnv->pi.winVer==SPlatformInfo::Win2000 ? "Win2000" : pEnv->pi.winVer==SPlatformInfo::WinSrv2003 ? "Windows Server 2003" : "Unknown", pEnv->pi.win64Bit ? 64 : 32);
#endif
  }
  m_nStreamingMode = CRenderer::CV_r_texturesstreaming;
#ifndef XENON
  m_bStreamOnlyVideo = CRenderer::CV_r_texturesstreamingonlyvideo != 0;
  if (m_bStreamOnlyVideo)
    m_bStreamDontKeepSystem = false;
  else
    m_bStreamDontKeepSystem = true;
#else
    m_bStreamOnlyVideo = true;
  m_bStreamDontKeepSystem = true;
#endif
  if (!m_FreeTexPoolItems.m_NextFree)
  {
    m_FreeTexPoolItems.m_NextFree = &m_FreeTexPoolItems;
    m_FreeTexPoolItems.m_PrevFree = &m_FreeTexPoolItems;
  }
  if (m_nStreamed)
  {
#ifdef WIN32
    // no vista
    if (pEnv->pi.winVer==SPlatformInfo::WinVista && !pEnv->pi.win64Bit)
    {
			if (gRenDev->m_MaxTextureMemory <= 220*1024*1024)
			{
      if (CRenderer::CV_r_texturesstreampoolsize > 100)
        _SetVar("r_TexturesStreamPoolSize", 100);
				if (CRenderer::CV_r_texturesstreamingmaxasync > 0.18f)
					CRenderer::CV_r_texturesstreamingmaxasync = 0.18f;
			}
			else
			{
				if (CRenderer::CV_r_texturesstreampoolsize > 148)
					_SetVar("r_TexturesStreamPoolSize", 148);
			}
    }
    else
#endif
    if (gRenDev->m_MaxTextureMemory <= 220*1024*1024)
    {
      if (CRenderer::CV_r_texturesstreampoolsize > 100)
        _SetVar("r_TexturesStreamPoolSize", 100);
      if (CRenderer::CV_r_texturesstreamingmaxasync > 0.18f)
        CRenderer::CV_r_texturesstreamingmaxasync = 0.18f;
    }
    else
    if (gRenDev->m_MaxTextureMemory <= 500*1024*1024)
    {
      if (CRenderer::CV_r_texturesstreampoolsize > 148)
        _SetVar("r_TexturesStreamPoolSize", 148);
    }
    else
    if (gRenDev->m_MaxTextureMemory <= 700*1024*1024)
    {
      if (CRenderer::CV_r_texturesstreampoolsize > 256)
        _SetVar("r_TexturesStreamPoolSize", 256);
    }
    else
    {
      if (CRenderer::CV_r_texturesstreampoolsize < 256)
        _SetVar("r_TexturesStreamPoolSize", 256);
    }
    if (CRenderer::CV_r_texturesstreampoolsize < 100)
      _SetVar("r_TexturesStreamPoolSize", 100);
    iLog->Log("  Enabling of textures streaming...");
    iLog->Log("  Using %d Mb of textures pool for streaming...", CRenderer::CV_r_texturesstreampoolsize);
  }
  else
    iLog->Log("  Disabling of textures streaming...");

  if (gRenDev->m_MaxTextureMemory <= 256*1024*1024)
  {
    if (CRenderer::CV_r_dyntexatlascloudsmaxsize > 24)
      _SetVar("r_DynTexAtlasCloudsMaxSize", 24);
    if (CRenderer::CV_r_dyntexatlasspritesmaxsize > 32)
      _SetVar("r_DynTexAtlasSpritesMaxSize", 32);
    if (CRenderer::CV_r_texatlassize > 1024)
      _SetVar("r_TexAtlasSize", 1024);
    if (CRenderer::CV_r_dyntexmaxsize > 64)
      _SetVar("r_DynTexMaxSize", 64);
  }
  iLog->Log("  Video textures: Atlas clouds max size: %d Mb", CRenderer::CV_r_dyntexatlascloudsmaxsize);
  iLog->Log("  Video textures: Atlas sprites max size: %d Mb", CRenderer::CV_r_dyntexatlasspritesmaxsize);
  iLog->Log("  Video textures: Dynamic managed max size: %d Mb", CRenderer::CV_r_dyntexmaxsize);

  CCryName Name = CTexture::mfGetClassName();
  SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
  ResourcesMapItor itor;

  if (pRL)
  {
    int nID = 0;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++, nID++)
    {
      CTexture *tp = (CTexture *)itor->second;
      if (!tp)
        continue;
      if (!(tp->GetFlags() & (FT_FROMIMAGE | FT_DONT_STREAM)))
        continue;
      tp->ToggleStreaming(m_nStreamed);
    }
  }
}

