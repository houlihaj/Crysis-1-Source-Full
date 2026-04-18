/*=============================================================================
  DDSImage.cpp : DDS image file format implementation.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Khonich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "CImage.h"
#include "DDSImage.h"
#include "ImageExtensionHelper.h"				// CImageExtensionHelper
#include "TypeInfo_impl.h"
#include "ImageExtensionHelper_info.h"	
#include "StringUtils.h"								// stristr()

/* needed for DirectX's DDSURFACEDESC2 structure definition */
#define FOURCC_DXT1  (MAKEFOURCC('D','X','T','1'))
#define FOURCC_DXT2  (MAKEFOURCC('D','X','T','2'))
#define FOURCC_DXT3  (MAKEFOURCC('D','X','T','3'))
#define FOURCC_DXT4  (MAKEFOURCC('D','X','T','4'))
#define FOURCC_DXT5  (MAKEFOURCC('D','X','T','5'))

#define FOURCC_3DC   (MAKEFOURCC('A','T','I','2'))
#define FOURCC_R32F   ( 0x00000072 )
#define FOURCC_G16R16F   ( 0x00000070 )


#include "ILog.h"

//===========================================================================

static int sDDSSize(int sx, int sy, int nDepth, ETEX_Format eF )
{
  return CTexture::TextureDataSize(sx, sy, nDepth, 1, eF);
}

int CImageDDSFile::mfSizeWithMips(int filesize, int sx, int sy, int nDepth, int nMips, ETEX_Format eTF)
{
  int nSize = CTexture::TextureDataSize(sx, sy, nDepth, nMips, eTF);
  //assert((int)(filesize-sizeof(DDS_HEADER)-4) >= nSize);
  return nSize;
}

static FILE *sFILELog;


ETEX_Format CImageDDSFile::GetTextureFormat( CImageExtensionHelper::DDS_HEADER *ddsh )
{
	assert(ddsh);

	if (ddsh->ddspf.dwFourCC == FOURCC_DXT1)
		return eTF_DXT1;
	else if (ddsh->ddspf.dwFourCC == FOURCC_DXT3)
		return eTF_DXT3;
	else if (ddsh->ddspf.dwFourCC == FOURCC_DXT5)
		return eTF_DXT5;
	else if (ddsh->ddspf.dwFourCC == FOURCC_3DC)
		return eTF_3DC;
	else if( ddsh->ddspf.dwFourCC == FOURCC_R32F)
		return eTF_R32F;
	else if( ddsh->ddspf.dwFourCC == FOURCC_G16R16F)
		return eTF_G16R16F;  
	else if (ddsh->ddspf.dwFlags == DDS_RGBA && ddsh->ddspf.dwRGBBitCount == 32 && ddsh->ddspf.dwABitMask == 0xff000000)
		return eTF_A8R8G8B8;
	else if (ddsh->ddspf.dwFlags == DDS_RGBA && ddsh->ddspf.dwRGBBitCount == 16)
		return eTF_A4R4G4B4;
	else if (ddsh->ddspf.dwFlags == DDS_RGB  && ddsh->ddspf.dwRGBBitCount == 24)
		return eTF_R8G8B8;
	else if (ddsh->ddspf.dwFlags == DDS_RGB  && ddsh->ddspf.dwRGBBitCount == 32)
		return eTF_X8R8G8B8;
	else if (ddsh->ddspf.dwFlags == DDS_LUMINANCE  && ddsh->ddspf.dwRGBBitCount == 8)
		return eTF_L8;
	else if ((ddsh->ddspf.dwFlags == DDS_A || ddsh->ddspf.dwFlags == DDS_A_ONLY) && ddsh->ddspf.dwRGBBitCount == 8)
		return eTF_A8;
	else
	{
		mfSet_error (eIFE_BadFormat, "Unknown DDS image format");
		assert(0);
		return eTF_Unknown;
	}
}


uint32 CImageDDSFile::GetMipCount( CImageExtensionHelper::DDS_HEADER *ddsh )
{
	uint32 dwRet = ddsh->dwMipMapCount;

	if(dwRet==0)
		dwRet=1;

	return dwRet;
}

struct SDDS
{
  DWORD dwMagic;
  CImageExtensionHelper::DDS_HEADER ddsh;
};
CImageDDSFile::CImageDDSFile (byte* ptr, FILE *fp, int filesize, uint nFlags) : CImageFile ()
{
  DWORD dwMagic;
  CImageExtensionHelper::DDS_HEADER *ddsh;

  SDDS dds;

  if (!ptr)
  {
    assert(fp);
    if (!fp)
    {
      mfSet_error (eIFE_BadFormat, "Invalid input data");
      return;
    }

    iSystem->GetIPak()->FReadRaw(&dds, sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD), 1, fp);
    ptr = (byte *)&dds;
  }

  byte *pStart = ptr;
	SwapEndian(*(DWORD *)ptr);
  dwMagic = *(DWORD *)ptr;
  ptr += sizeof(DWORD);
  if (dwMagic != MAKEFOURCC('D','D','S',' '))
  {
    mfSet_error (eIFE_BadFormat, "Not a DDS file");
    return;
  }
  ddsh = (CImageExtensionHelper::DDS_HEADER *)ptr;
	SwapEndian(*ddsh);
  ptr += sizeof(CImageExtensionHelper::DDS_HEADER);
  if (ddsh->dwSize != sizeof(CImageExtensionHelper::DDS_HEADER))
  {
    mfSet_error (eIFE_BadFormat, "Unknown DDS file header");
    return;
  }
  m_Width = ddsh->dwWidth;
  m_Height = ddsh->dwHeight;
	int numMips = GetMipCount(ddsh);

	m_eFormat = GetTextureFormat(ddsh);

  m_eSrcFormat = m_eFormat;
  mfSet_numMips(numMips);
  if (!(nFlags & FIM_ALPHA))
  {
    if ((ddsh->dwReserved1[0] & DDS_RESF1_NORMALMAP) ||
			(strlen(m_CurFileName)>4 && (CryStringUtils::stristr(m_CurFileName, "_ddn"))))
      mfSet_Flags(FIM_NORMALMAP);
    else
    if ((ddsh->dwReserved1[0] & DDS_RESF1_DSDT) ||
         (strlen(m_CurFileName)>4 && CryStringUtils::stristr(m_CurFileName, "_ddt")))
    {
      mfSet_Flags(FIM_DSDT);
      if (m_eFormat == eTF_R8G8B8)
        m_eFormat = eTF_L8V8U8;
    }
  }
  int nDepth = ddsh->dwDepth;
  if (nDepth <= 0)
    nDepth = 1;
  m_Depth = nDepth;
  int nSides = 1;
  int nOffsSrc = 0;
  int nOffsDst = 0;
  if ((ddsh->dwSurfaceFlags & DDS_SURFACE_FLAGS_CUBEMAP) && (ddsh->dwCubemapFlags & DDS_CUBEMAP_ALLFACES))
  {
    nSides = 6;
  }
	// Crytek extension: we attached chunk based data to DDS files
	uint8 *pAttachedData = 0;
  int nSizeDDS = mfSizeWithMips(filesize, m_Width, m_Height, nDepth, numMips, m_eFormat) * nSides;
	//PS3HACK : not implemented yet
#if !defined(PS3)
	{
		if ((int)filesize-sizeof(CImageExtensionHelper::DDS_HEADER)-4 > nSizeDDS)
			pAttachedData = ptr+nSizeDDS;
	}
#endif
  if (pAttachedData)
  {
    if (fp)
    {
      iSystem->GetIPak()->FSeek(fp, nSizeDDS+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD), SEEK_SET);
      pAttachedData = new byte[filesize - (nSizeDDS+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD))];
      iSystem->GetIPak()->FReadRaw(pAttachedData, filesize-(nSizeDDS+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD)), 1, fp);
    }
		m_AvgColor = CImageExtensionHelper::GetAverageColor(pAttachedData);
		uint32 imageFlags = CImageExtensionHelper::GetImageFlags((CImageExtensionHelper::DDS_HEADER *)ddsh);

		if (imageFlags & CImageExtensionHelper::EIF_Decal)
			m_Flags |= FIM_DECAL;
		if (imageFlags & CImageExtensionHelper::EIF_Greyscale)
			m_Flags |= FIM_GREYSCALE;
		if (imageFlags & CImageExtensionHelper::EIF_FileSingle)
			m_Flags |= FIM_FILESINGLE;
		if (imageFlags & CImageExtensionHelper::EIF_DontStream)
    {
			m_Flags |= FIM_DONTSTREAM;
      if (fp)
      {
        mfSet_error (eIFE_BadFormat, "Streaming mismatch");
        SAFE_DELETE_ARRAY(pAttachedData);
        return;
      }
    }
  }
  m_nStartSeek = sizeof(CImageExtensionHelper::DDS_HEADER) + sizeof(DWORD);
 
	if (nFlags & FIM_ALPHA)
  {
    bool bIgnore = true;
    if (pAttachedData)
    {
      CImageExtensionHelper::DDS_HEADER *pDDSHeader = (CImageExtensionHelper::DDS_HEADER *)CImageExtensionHelper::GetAttachedImage(pAttachedData);

			if (pDDSHeader)
      {
        ptr = (uint8 *)(pDDSHeader)+sizeof(CImageExtensionHelper::DDS_HEADER);
        m_nStartSeek = (byte *)pDDSHeader-pAttachedData+sizeof(CImageExtensionHelper::DDS_HEADER) + nSizeDDS+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD);

				m_eFormat = eTF_A8;
        m_eSrcFormat = eTF_A8;
//				assert(m_eFormat==GetTextureFormat(pDDSHeader));					// attached alpha channel should have alpha texture format
				m_Width=pDDSHeader->dwWidth;
				m_Height=pDDSHeader->dwHeight;
				numMips = GetMipCount(pDDSHeader);
			  mfSet_numMips(numMips);
        bIgnore = false;
      }
    }
    if (bIgnore)
    {
      if (fp)
      {
        SAFE_DELETE_ARRAY(pAttachedData);
      }
      return;
    }
  }
  if (fp)
    ptr = NULL;

  nOffsSrc = 0;
  int nWdt = m_Width;
  int nHgt = m_Height;
  if (nFlags & FIM_LAST4MIPS)
  {
    assert(fp);
    int nMips = min(4, numMips);
    ETEX_Format eTFDst = m_eFormat;
#ifdef DIRECT3D10
    if (eTFDst == eTF_L8)
      eTFDst = eTF_X8R8G8B8;
#endif
    if (numMips > nMips)
    {
      nOffsSrc = mfSizeWithMips(filesize, m_Width, m_Height, 1, numMips-nMips, m_eFormat);
      nWdt = m_Width >> (numMips-nMips);
      nHgt = m_Height >> (numMips-nMips);
      if (!nWdt) nWdt = 1;
      if (!nHgt) nHgt = 1;
    }
  }
  for (int nS=0; nS<nSides; nS++)
  {
    nOffsDst = 0;
    SAFE_DELETE_ARRAY(m_pByteImage[nS]);
    ETEX_Format eTFDst = m_eFormat;
#ifdef DIRECT3D10
    if (eTFDst == eTF_L8)
      eTFDst = eTF_X8R8G8B8;
#endif
    int nMipsDst = numMips;
    if (nFlags & FIM_LAST4MIPS)
      nMipsDst = min(4, numMips);
    int sizeSrc = mfSizeWithMips(filesize, nWdt, nHgt, 1, nMipsDst, m_eFormat);
    int sizeSrcIncr = mfSizeWithMips(filesize, m_Width, m_Height, 1, numMips, m_eFormat);
    int sizeImg = mfSizeWithMips(filesize, nWdt, nHgt, nDepth, nMipsDst, eTFDst);
    int sizeDst = mfSizeWithMips(filesize, nWdt, nHgt, 1, nMipsDst, m_eFormat);
    if (m_eFormat == eTF_L8V8U8 || m_eFormat == eTF_R8G8B8)
    {
      ETEX_Format eTF = m_eFormat;
      m_eFormat = eTF_A8R8G8B8;
      sizeDst = mfSizeWithMips(filesize, m_Width, m_Height, 1, numMips, m_eFormat);
      sizeImg = mfSizeWithMips(filesize, m_Width, m_Height, nDepth, numMips, m_eFormat);
      m_eFormat = eTF;
    }
    mfSet_ImageSize(sizeImg);
    mfGet_image(nS);

    for (int dpt=0; dpt<nDepth; dpt++)
    {
      if (CTexture::IsDXTCompressed(m_eFormat))
      {
        if (fp)
        {
          iSystem->GetIPak()->FSeek(fp, nOffsSrc+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD), SEEK_SET);
          iSystem->GetIPak()->FReadRaw(&m_pByteImage[nS][nOffsDst], sizeSrc, 1, fp);
        }
        else
          cryMemcpy(&m_pByteImage[nS][nOffsDst], &ptr[nOffsSrc], sizeSrc);
#if !defined(PS3)
				// No DXT endian swap for PS3!
				SwapEndian((int16*)&m_pByteImage[nS][nOffsDst], sizeSrc/2);
#endif
        nOffsSrc += sizeSrcIncr;
        nOffsDst += sizeDst;
      }
      else
      if (m_eSrcFormat == eTF_L8)
      {
#ifdef DIRECT3D10
        int nComps = sizeSrc;
        int nO = nOffsSrc;
        if (fp)
        {
          ptr = new byte[sizeSrc];
          iSystem->GetIPak()->FSeek(fp, nOffsSrc+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD), SEEK_SET);
          iSystem->GetIPak()->FReadRaw(ptr, sizeSrc, 1, fp);
          nO = 0;
        }
        for (int i=0; i<nComps; i++)
        {
          byte bSrc = ptr[i+nO];
          m_pByteImage[nS][i*4+nOffsDst+0] = bSrc;
          m_pByteImage[nS][i*4+nOffsDst+1] = bSrc;
          m_pByteImage[nS][i*4+nOffsDst+2] = bSrc;
          m_pByteImage[nS][i*4+nOffsDst+3] = 255;
        }
        if (fp)
        {
          SAFE_DELETE_ARRAY(ptr);
        }
#else
        if (fp)
        {
          iSystem->GetIPak()->FSeek(fp, nOffsSrc+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD), SEEK_SET);
          iSystem->GetIPak()->FReadRaw(&m_pByteImage[nS][nOffsDst], sizeSrc, 1, fp);
        }
        else
          cryMemcpy(&m_pByteImage[nS][nOffsDst], &ptr[nOffsSrc], sizeSrc);
#endif
        nOffsSrc += sizeSrc;
        nOffsDst += sizeDst;
      }
      else
      if (m_eFormat == eTF_A8 || m_eFormat == eTF_R32F)
      {
        if (fp)
        {
          iSystem->GetIPak()->FSeek(fp, nOffsSrc+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD), SEEK_SET);
          iSystem->GetIPak()->FReadRaw(&m_pByteImage[nS][nOffsDst], sizeSrc, 1, fp);
        }
        else
          cryMemcpy(&m_pByteImage[nS][nOffsDst], &ptr[nOffsSrc], sizeSrc);
        nOffsSrc += sizeSrc;
        nOffsDst += sizeDst;
      }
      else
      if( m_eFormat == eTF_G16R16F )
      {
        if (fp)
        {
          iSystem->GetIPak()->FSeek(fp, nOffsSrc+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD), SEEK_SET);
          iSystem->GetIPak()->FReadRaw(&m_pByteImage[nS][nOffsDst], sizeSrc, 1, fp);
        }
        else
          cryMemcpy(&m_pByteImage[nS][nOffsDst], &ptr[nOffsSrc], sizeSrc);
        //SwapEndian((int32*)&m_pByteImage[nS][nOffsDst], sizeSrc >> 1);

        nOffsSrc += sizeSrc;
        nOffsDst += sizeDst;
      }
      else
      if (m_eFormat == eTF_A8R8G8B8)
      {
        if (fp)
        {
          iSystem->GetIPak()->FSeek(fp, nOffsSrc+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD), SEEK_SET);
          iSystem->GetIPak()->FReadRaw(&m_pByteImage[nS][nOffsDst], sizeSrc, 1, fp);
        }
        else
          cryMemcpy(&m_pByteImage[nS][nOffsDst], &ptr[nOffsSrc], sizeSrc);
        SwapEndian((int32*)&m_pByteImage[nS][nOffsDst], sizeSrc/4);
        int nComps = sizeSrc/4;
        for (int i=0; i<nComps; i++)
        {
#if defined(LINUX)
					m_pByteImage[nS][i*4+nOffsDst+3] = ptr[i*4+nOffsSrc+0];
					m_pByteImage[nS][i*4+nOffsDst+2] = ptr[i*4+nOffsSrc+1];
					m_pByteImage[nS][i*4+nOffsDst+1] = ptr[i*4+nOffsSrc+2];
					m_pByteImage[nS][i*4+nOffsDst+0] = ptr[i*4+nOffsSrc+3];
#endif
#ifdef DIRECT3D10
          if (!fp)
            std::swap(m_pByteImage[nS][i*4+nOffsDst+2], m_pByteImage[nS][i*4+nOffsDst+0]);
#endif
        }
        nOffsSrc += sizeSrc;
        nOffsDst += sizeDst;
      }
      else
      if (m_eFormat == eTF_A4R4G4B4)
      {
        cryMemcpy(&m_pByteImage[nS][nOffsDst], &ptr[nOffsSrc], sizeSrc);
        SwapEndian((int32*)&m_pByteImage[nS][nOffsDst], sizeSrc/4);
        nOffsSrc += sizeSrc;
        nOffsDst += sizeDst;
      }
      else
      if (m_eFormat == eTF_X8R8G8B8)
      {
        if (fp)
        {
          iSystem->GetIPak()->FSeek(fp, nOffsSrc+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD), SEEK_SET);
          iSystem->GetIPak()->FReadRaw(&m_pByteImage[nS][nOffsDst], sizeSrc, 1, fp);
        }
        else
          cryMemcpy(&m_pByteImage[nS][nOffsDst], &ptr[nOffsSrc], sizeSrc);
        SwapEndian((int32*)&m_pByteImage[nS][nOffsDst], sizeSrc/4);
        int nComps = sizeSrc/4;
#ifndef XENON
        for (int i=0; i<nComps; i++)
        {
#if !defined(LINUX)
          m_pByteImage[nS][i*4+nOffsDst+3] = 255;
#else
					m_pByteImage[nS][i*4+nOffsDst+3] = ptr[i*4+nOffsSrc+0];
					m_pByteImage[nS][i*4+nOffsDst+2] = ptr[i*4+nOffsSrc+1];
					m_pByteImage[nS][i*4+nOffsDst+1] = ptr[i*4+nOffsSrc+2];
					m_pByteImage[nS][i*4+nOffsDst+0] = 255;
#endif
#ifdef DIRECT3D10
          if (!fp)
            std::swap(m_pByteImage[nS][i*4+nOffsDst+2], m_pByteImage[nS][i*4+nOffsDst+0]);
#endif
        }
#endif
        nOffsSrc += sizeSrc;
        nOffsDst += sizeDst;
      }
      else
      if (m_eFormat == eTF_R8G8B8)
      {
        int nComps = sizeSrc/3;
        int nO = nOffsSrc;
        if (fp)
        {
          ptr = new byte[sizeSrc];
          iSystem->GetIPak()->FSeek(fp, nOffsSrc+sizeof(CImageExtensionHelper::DDS_HEADER)+sizeof(DWORD), SEEK_SET);
          iSystem->GetIPak()->FReadRaw(ptr, sizeSrc, 1, fp);
          nO = 0;
        }
        for (int i=0; i<nComps; i++)
        {
#if defined(XENON) || defined(PS3)
          m_pByteImage[nS][i*4+nOffsDst+3] = ptr[i*3+nO+0];
          m_pByteImage[nS][i*4+nOffsDst+2] = ptr[i*3+nO+1];
          m_pByteImage[nS][i*4+nOffsDst+1] = ptr[i*3+nO+2];
          m_pByteImage[nS][i*4+nOffsDst+0] = 255;
#elif defined (DIRECT3D10)
          if (!fp)
          {
            m_pByteImage[nS][i*4+nOffsDst+0] = ptr[i*3+nO+2];
            m_pByteImage[nS][i*4+nOffsDst+1] = ptr[i*3+nO+1];
            m_pByteImage[nS][i*4+nOffsDst+2] = ptr[i*3+nO+0];
            m_pByteImage[nS][i*4+nOffsDst+3] = 255;
          }
          else
          {
            m_pByteImage[nS][i*4+nOffsDst+0] = ptr[i*3+nO+0];
            m_pByteImage[nS][i*4+nOffsDst+1] = ptr[i*3+nO+1];
            m_pByteImage[nS][i*4+nOffsDst+2] = ptr[i*3+nO+2];
            m_pByteImage[nS][i*4+nOffsDst+3] = 255;
          }
#else
          m_pByteImage[nS][i*4+nOffsDst+0] = ptr[i*3+nO+0];
          m_pByteImage[nS][i*4+nOffsDst+1] = ptr[i*3+nO+1];
          m_pByteImage[nS][i*4+nOffsDst+2] = ptr[i*3+nO+2];
          m_pByteImage[nS][i*4+nOffsDst+3] = 255;
#endif
          /*if (m_pByteImage[nS][i*4+nOffsDst+0] == 0 && m_pByteImage[nS][i*4+nOffsDst+1] == 0 && m_pByteImage[nS][i*4+nOffsDst+2] == 255)
          {
            m_pByteImage[nS][i*4+nOffsDst+0] = 0;
            m_pByteImage[nS][i*4+nOffsDst+1] = 0;
            m_pByteImage[nS][i*4+nOffsDst+2] = 0;
          }*/
        }
        if (fp)
        {
          SAFE_DELETE_ARRAY(ptr);
        }
        nOffsSrc += sizeSrc;
        nOffsDst += sizeDst;
      }
      else
      if (m_eFormat == eTF_L8V8U8)
      {
        int nComps = sizeSrc/3;
        for (int i=0; i<nComps; i++)
        {
          m_pByteImage[nS][i*4+nOffsDst+0] = ptr[i*3+nOffsSrc+2];
          m_pByteImage[nS][i*4+nOffsDst+1] = ptr[i*3+nOffsSrc+1];
          m_pByteImage[nS][i*4+nOffsDst+2] = ptr[i*3+nOffsSrc+0];
          m_pByteImage[nS][i*4+nOffsDst+3] = 255;
        }
        nOffsSrc += sizeSrc;
        nOffsDst += sizeDst;
      }
    }
  }
  if (m_eFormat == eTF_3DC && !(nFlags & FIM_ALPHA))
  {
		// even if not requested when loading 3dc we store the info if ALPHA would be available so we know for later
    if (pAttachedData)
    {
      CImageExtensionHelper::DDS_HEADER *pDDSHeader = (CImageExtensionHelper::DDS_HEADER *)CImageExtensionHelper::GetAttachedImage(pAttachedData);
      if (pDDSHeader)
			{
//				assert(eTF_A8==GetTextureFormat(pDDSHeader));					// attached alpha channel should have alpha texture format       
				mfSet_Flags(FIM_ALPHA);
			}
    }
  }

#ifdef DIRECT3D10
  if (m_eSrcFormat == eTF_L8)
    m_eFormat = eTF_X8R8G8B8;
#endif

  if (m_eSrcFormat == eTF_R8G8B8)
    m_eFormat = eTF_X8R8G8B8;
  else
  if (m_eSrcFormat == eTF_L8V8U8)
    m_eFormat = eTF_X8L8V8U8;

  if (fp)
  {
    SAFE_DELETE_ARRAY(pAttachedData);
  }
}

CImageDDSFile::~CImageDDSFile ()
{
}

byte *WriteDDS(byte*dat, int wdt, int hgt, int dpth, const char*name, ETEX_Format eTF, int nMips, ETEX_Type eTT, bool bToMemory, int *pSize)
{
  DWORD dwMagic;
  CImageExtensionHelper::DDS_HEADER ddsh;
  memset(&ddsh, 0, sizeof(ddsh));
  byte *pData = NULL;
  FILE *fp = NULL;
  int nOffs = 0;
  int nSize = CTexture::TextureDataSize(wdt, hgt, dpth, nMips, eTF);

  dwMagic = MAKEFOURCC('D','D','S',' ');

  if (!bToMemory)
  {
    fp = iSystem->GetIPak()->FOpen(name, "wb");
    if (!fp)
      return false;

    iSystem->GetIPak()->FWrite(&dwMagic, 1, sizeof(DWORD), fp);
  }
  else
  {
    pData = new byte[sizeof(DWORD)+sizeof(CImageExtensionHelper::DDS_HEADER)+nSize];
    *(DWORD *)pData = dwMagic;
    nOffs += sizeof(DWORD);
  }

  ddsh.dwSize = sizeof(CImageExtensionHelper::DDS_HEADER);
  ddsh.dwWidth = wdt;
  ddsh.dwHeight = hgt;
  ddsh.dwMipMapCount = nMips;
  if (nMips <= 0)
    ddsh.dwMipMapCount = 1;
  ddsh.dwHeaderFlags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
  ddsh.dwSurfaceFlags = DDS_SURFACE_FLAGS_TEXTURE | DDS_SURFACE_FLAGS_MIPMAP;
  int nSides = 1;
  if (eTT == eTT_Cube)
  {
    ddsh.dwSurfaceFlags |= DDS_SURFACE_FLAGS_CUBEMAP;
    ddsh.dwCubemapFlags |= DDS_CUBEMAP_ALLFACES;
    nSides = 6;
  }
  else
  if (eTT == eTT_3D)
  {
    ddsh.dwHeaderFlags |= DDS_HEADER_FLAGS_VOLUME;
  }
  if (eTT != eTT_3D)
    dpth = 1;
  ddsh.dwDepth = dpth;
  if (name)
  {
    size_t len = strlen(name);
	  if (len > 4)
	  {
		  if (!stricmp(&name[len-4], ".ddn"))
			  ddsh.dwReserved1[0] = DDS_RESF1_NORMALMAP;
		  else
		  if (!stricmp(&name[len-4], ".ddt"))
			  ddsh.dwReserved1[0] = DDS_RESF1_DSDT;
	  }
  }

  switch (eTF)
  {
    case eTF_DXT1:
      ddsh.ddspf = DDSFormats::DDSPF_DXT1;
      break;
    case eTF_DXT3:
      ddsh.ddspf = DDSFormats::DDSPF_DXT3;
      break;
    case eTF_DXT5:
      ddsh.ddspf = DDSFormats::DDSPF_DXT5;
      break;
    case eTF_3DC:
      ddsh.ddspf = DDSFormats::DDSPF_3DC;
      break;
    case eTF_R32F:
      ddsh.ddspf = DDSFormats::DDSPF_R32F;
      break;
    case eTF_G16R16F:
      ddsh.ddspf = DDSFormats::DDSPF_G16R16F;
      break;
    case eTF_R8G8B8:
    case eTF_L8V8U8:
      ddsh.ddspf = DDSFormats::DDSPF_R8G8B8;
      ddsh.ddspf.dwRGBBitCount = 24;
      break;
    case eTF_A8R8G8B8:
      ddsh.ddspf = DDSFormats::DDSPF_A8R8G8B8;
      ddsh.ddspf.dwRGBBitCount = 32;
      break;
    case eTF_X8R8G8B8:
      ddsh.ddspf = DDSFormats::DDSPF_R8G8B8;
      ddsh.ddspf.dwRGBBitCount = 32;
      break;
    case eTF_R5G6B5:
      ddsh.ddspf = DDSFormats::DDSPF_R5G6B5;
      ddsh.ddspf.dwRGBBitCount = 16;
      break;
    case eTF_A8:
      ddsh.ddspf = DDSFormats::DDSPF_A8;
      ddsh.ddspf.dwRGBBitCount = 8;
      break;
    default:
      assert(0);
      return false;
  }
  ddsh.dwPitchOrLinearSize = CTexture::TextureDataSize(wdt, 1, 1, 1, eTF);
  if (!bToMemory)
  {
    iSystem->GetIPak()->FWrite(&ddsh, 1, sizeof(ddsh), fp);

    int nOffs = 0;
    int nSide;
    for (nSide=0; nSide<nSides; nSide++)
    {
      iSystem->GetIPak()->FWrite(&dat[nOffs], 1, nSize, fp);
      nOffs += nSize;
    }

    iSystem->GetIPak()->FClose (fp);
  }
  else
  {
    memcpy(&pData[nOffs], &ddsh, sizeof(ddsh));
    nOffs += sizeof(ddsh);

    int nSide;
    int nSrcOffs = 0;
    for (nSide=0; nSide<nSides; nSide++)
    {
      memcpy(&pData[nOffs], &dat[nSrcOffs], nSize);
      nSrcOffs += nSize;
      nOffs += nSize;
    }

    if (pSize)
      *pSize = nOffs;
    return pData;
  }

  return NULL;
}
