/*=============================================================================
  CImage.cpp : Common Image class implementation.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Khonich Andrey

=============================================================================*/

#include "StdAfx.h"

#include "CImage.h"
#include "DDSImage.h"

#ifndef XENON
#	include "JpgImage.h"
#	include "TgaImage.h"
#else
bool WriteTGA(byte *data, int width, int height, const char *filename, int src_bits_per_pixel, int dest_bits_per_pixel)
{
  return false;
}

bool WriteJPG( byte *dat, int wdt, int hgt, const char *name, int bpp )
{
  return false;
}

#endif

#include "ResourceCompilerHelper.h"				// CResourceCompilerHelper

//---------------------------------------------------------------------------

EImFileError CImageFile::m_eError = eIFE_OK;
char CImageFile::m_Error_detail[256];
char CImageFile::m_CurFileName[128];

CImageFile::CImageFile ()
{
  m_pByteImage[0] = NULL;
  m_pByteImage[1] = NULL;
  m_pByteImage[2] = NULL;
  m_pByteImage[3] = NULL;
  m_pByteImage[4] = NULL;
  m_pByteImage[5] = NULL;

  m_eError = eIFE_OK;
  m_Error_detail[0] = 0;
  m_eFormat = eTF_Unknown;
  m_eSrcFormat = eTF_Unknown;
  m_NumMips = 0;
  m_Flags = 0;
  m_ImgSize = 0;
  m_Depth = 1;
  m_AvgColor = Col_White;
  m_nStartSeek = 0;
}

CImageFile::~CImageFile ()
{
  for (int i=0; i<6; i++)
  {
    if (!CTexture::m_pLoadData || m_pByteImage[i]<CTexture::m_pLoadData || m_pByteImage[i]>=&CTexture::m_pLoadData[TEXPOOL_LOADSIZE])
    {
      SAFE_DELETE_ARRAY(m_pByteImage[i]);
    }
  }
}

void CImageFile::mfSet_dimensions (int w, int h)
{
  m_Width = w;
  m_Height = h;
}

void CImageFile::mfSet_error (EImFileError error, char* detail)
{
  CImageFile::m_eError = error;
  if (detail)
    strcpy (m_Error_detail, detail);
  m_Error_detail[0] = 0;
}

void CImageFile::mfWrite_error (char* extra)
{
  if (m_eError == eIFE_OK)
    return;
  char buf[1000];
  int idx = 0;
  if (extra)
    idx += sprintf (buf+idx, "'%s': ", extra);
  switch (m_eError)
  {
    case eIFE_OK:
      return;

    case eIFE_IOerror:
      idx += sprintf (buf+idx, "IO error");
      break;

    case eIFE_OutOfMemory:
      idx += sprintf (buf+idx, "Out of memory");
      break;

    case eIFE_BadFormat:
      idx += sprintf (buf+idx, "Bad format");
      break;

		case eIFE_ResourceCompiler:
			break;

		default:
			assert(0);
  }
  if (m_Error_detail[0])
    sprintf (buf+idx, " (%s)!\n", m_Error_detail);
  else
    sprintf (buf+idx, "!\n");
  iConsole->Exit ("%s", buf);
}



	




float gFOpenTime;
int nRejectFOpen;
int nAcceptFOpen;

CImageFile* CImageFile::mfLoad_file (const char* szFileName, const bool bReload, uint nFlags)
{
	float dTime0 = iTimer->GetAsyncCurTime();

	string sFileToLoad = CResourceCompilerHelper::GetOutputFilename(szFileName);		// change filename: e.g. instead of TIF, pass DDS

	// is it needed to invoke the resource compiler?
#if defined(WIN32) || defined(WIN64)
	if(CRenderer::CV_r_rc_autoinvoke!=0)
	{
		string ext = PathUtil::GetExt(szFileName);

		if(CResourceCompilerHelper::IsImageFormat(ext))
		if(!ext.empty())
		{
			if(gEnv->pRenderer->EF_Query(EFQ_Fullscreen) || !gEnv->pSystem->IsDevMode())
			{
				gEnv->pLog->LogWarning("r_rc_autoinvoke of '%s' suppressed (full screen or non DevMode)",szFileName);
			}
			else
			{
				static CResourceCompilerHelper rch;

				sFileToLoad = rch.ProcessIfNeeded(szFileName, bReload);

				if(rch.IsError())
				{
					m_eError = eIFE_ResourceCompiler;
					strcpy(m_Error_detail, "InvokeResourceCompiler() failed (missing rc.exe?)");
				}
			}
		}
	}
#endif

  FILE* pRawFile = iSystem->GetIPak()->FOpen(sFileToLoad.c_str(),"rb");

	unticks(dTime0);

	gFOpenTime += iTimer->GetAsyncCurTime();

  if (!pRawFile)
  {
		TextureWarning(sFileToLoad.c_str(),"Cannot load texture %s, file missing", szFileName);
    nRejectFOpen++;
    return NULL;
  }
  nAcceptFOpen++;

	strcpy(m_CurFileName,sFileToLoad.c_str());


	strlwr(m_CurFileName);

  CImageFile* pImageFile = mfLoad_file (pRawFile, nFlags);

	if (pImageFile)
		strcpy(pImageFile->m_FileName, m_CurFileName);
	else
		TextureWarning(sFileToLoad.c_str(),"Cannot load texture %s, Image file format is invalid", szFileName);
  iSystem->GetIPak()->FClose (pRawFile);
	return pImageFile;
}

CImageFile* CImageFile::mfLoad_file (FILE* fp, uint nFlags)
{
	CImageFile* file = 0;

  if (nFlags & FIM_LAST4MIPS)
  {
    const char *ext = fpGetExtension(m_CurFileName);
    if (!strcmp(ext, ".dds"))
    {
      iSystem->GetIPak()->FSeek(fp, 0, SEEK_END);
      int nSize = iSystem->GetIPak()->FTell(fp);
      iSystem->GetIPak()->FSeek(fp, 0, SEEK_SET);
      file = (CImageFile *)new CImageDDSFile (NULL, fp, nSize, nFlags);
    }
  }
  else
  {
	  size_t nFileSize = 0;

    iSystem->GetIPak()->FSeek (fp, 0, SEEK_END);
    nFileSize = iSystem->GetIPak()->FTell (fp);
    iSystem->GetIPak()->FSeek (fp, 0, SEEK_SET);
    byte *buf = NULL;
    if (CTexture::m_pLoadData && nFileSize+CTexture::m_nLoadOffset <= TEXPOOL_LOADSIZE)
    {
      buf = &CTexture::m_pLoadData[CTexture::m_nLoadOffset];
      CTexture::m_nLoadOffset += nFileSize;
    }
    else
      buf = new byte [nFileSize];
    if (!buf)
      return 0;		// out of memory

    iSystem->GetIPak()->FReadRaw (buf, 1, nFileSize, fp);
    file = mfLoad_file (buf, nFileSize, nFlags);
    if (buf<CTexture::m_pLoadData || buf>=&CTexture::m_pLoadData[TEXPOOL_LOADSIZE])
      delete [] buf;

	  //byte *buf = (byte*)iSystem->GetIPak()->FGetCachedFileData (fp, nFileSize);
	  //if (buf)
		//  file = mfLoad_file (buf, nFileSize, nFlags);
  }
  return file;
}

CImageFile* CImageFile::mfLoad_file (byte* buf, int size, uint nFlags)
{
  CImageFile* file = NULL;
  CImageFile::m_eError = eIFE_OK;

  // Catch NULL pointers (for example, when ZIP file is corrupt)
  assert (buf);

  const char *ext = fpGetExtension(m_CurFileName);

	if(ext)
	{
		// Try DDS first
		if (!strcmp(ext, ".dds"))
			CHK (file = (CImageFile *)new CImageDDSFile (buf, NULL, size, nFlags));
#ifndef XENON
		else
		if (!strcmp(ext, ".jpg"))
			CHK (file = (CImageFile *)new CImageJpgFile (buf, size));
		else
		if (!strcmp(ext, ".tga"))
			CHK (file = (CImageFile *)new CImageTgaFile (buf, size));
#endif
		else
		{
			//assert(0);
		}
	}

  if (file && (CImageFile::mfGet_error () != eIFE_OK))
  {
    CHK (delete file);
    file = NULL;
  } /* endif */

  return file;
}

//---------------------------------------------------------------------------

void BlurImage8(byte * pImage, int nSize, int nPassesNum)
{
#define DATA_TMP(_x,_y) (pTemp [(_x)+nSize*(_y)])
#define DATA_IMG(_x,_y) (pImage[(_x)+nSize*(_y)])

  byte * pTemp = new byte [nSize*nSize];

  for(int nPass=0; nPass<nPassesNum; nPass++)
  {
    cryMemcpy(pTemp,pImage,nSize*nSize);

    for(int x=1; x<nSize-1; x++)
    for(int y=1; y<nSize-1; y++)
    {
      float fVal = 0;
      fVal += DATA_TMP(x,y);
      fVal += DATA_TMP(x+1,y+1);
      fVal += DATA_TMP(x-1,y+1);
      fVal += DATA_TMP(x+1,y-1);
      fVal += DATA_TMP(x-1,y-1);
      DATA_IMG(x,y) = uchar(fVal*0.2f);
    }
  }

  delete [] pTemp;

#undef DATA_IMG
#undef DATA_TMP
}
