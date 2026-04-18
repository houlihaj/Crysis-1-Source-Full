////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   image_dxtc.cpp
//  Version:     v1.00
//  Created:     5/9/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Image_DXTC.h"
#include <CryFile.h>

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
	((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
	((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif //defined(MAKEFOURCC)
#include "dds.h"

CImage_DXTC::CImage_DXTC()
{
}

CImage_DXTC::~CImage_DXTC()
{
}

static ETEX_Format DecodePixelFormat( DDS_PIXELFORMAT* pddpf )
{
	if(pddpf->dwFlags&DDS_LUMINANCE && pddpf->dwRGBBitCount==8 && pddpf->dwRBitMask==0xff)
		return eTF_L8;

	ETEX_Format format = eTF_Unknown;

	switch( pddpf->dwFourCC )
	{
	case 0:
		// This dds texture isn't compressed so write out ARGB format
		format = eTF_A8R8G8B8;
		break;

	case MAKEFOURCC('D','X','T','1'):
		format = eTF_DXT1;
		break;

	case MAKEFOURCC('D','X','T','2'):
		//format = eTF_DXT2;
		break;

	case MAKEFOURCC('D','X','T','3'):
		format = eTF_DXT3;
		break;

	case MAKEFOURCC('D','X','T','4'):
		//format = eTF_DXT4;
		break;

	case MAKEFOURCC('D','X','T','5'):
		format = eTF_DXT5;
		break;

	case MAKEFOURCC('A','T','I','2'):
		format = eTF_3DC;
		break;
	}
	return format;
}

inline const char* NameForTextureFormat(ETEX_Format ETF)
{
	char *sETF;
	switch (ETF)
	{
	case eTF_R8G8B8:
		sETF = "R8G8B8";
		break;
	case eTF_A8R8G8B8:
		sETF = "A8R8G8B8";
		break;
	case eTF_X8R8G8B8:
		sETF = "X8R8G8B8";
		break;
	case eTF_A8:
		sETF = "A8";
		break;
	case eTF_A8L8:
		sETF = "A8L8";
		break;
	case eTF_L8:
		sETF = "L8";
		break;
	case eTF_A4R4G4B4:
		sETF = "A4R4G4B4";
		break;
	case eTF_R5G6B5:
		sETF = "R5G6B5";
		break;
	case eTF_R5G5B5:
		sETF = "R5G5B5";
		break;
	case eTF_DXT1:
		sETF = "DXT1";
		break;
	case eTF_DXT3:
		sETF = "DXT3";
		break;
	case eTF_DXT5:
		sETF = "DXT5";
		break;
	case eTF_3DC:
		sETF = "3DC";
		break;
	case eTF_V16U16:
		sETF = "V16U16";
		break;
	case eTF_X8L8V8U8:
		sETF = "X8L8V8U8";
		break;
	case eTF_V8U8:
		sETF = "V8U8";
		break;
	case eTF_L8V8U8:
		sETF = "L8V8U8";
		break;
	case eTF_A16B16G16R16F:
		sETF = "A16B16G16R16F";
		break;
	case eTF_A16B16G16R16:
		sETF = "A16B16G16R16";
		break;
	case eTF_A32B32G32R32F:
		sETF = "A32B32G32R32F";
		break;
	case eTF_G16R16F:
		sETF = "G16R16F";
		break;
	case eTF_R16F:
		sETF = "R16F";
		break;
	case eTF_R32F:
		sETF = "R32F";
		break;
	default:
		sETF = "Unknown";		// for better behaviour in non debug
		break;
	}
	return sETF;
}

//////////////////////////////////////////////////////////////////////////
bool CImage_DXTC::Load( const char *filename,CImage &outImage, bool *pQualityLoss )
{
	if(pQualityLoss)
		*pQualityLoss=false;

	CCryFile file;
	if (!file.Open( filename,"rb" ))
	{
		return( false );
	}

	DDS_HEADER ddsd;
	DWORD			dwMagic;

	BYTE	*pCompBytes;		// compressed image bytes
	BYTE	*pDecompBytes;


	// Read magic number
	file.ReadType( &dwMagic );

	if( dwMagic != MAKEFOURCC('D','D','S',' ') )
	{
		return( false);
	}

	// Read the surface description
	file.ReadTypeRaw( &ddsd );


	// Does texture have mipmaps?
	bool bMipTexture = ( ddsd.dwMipMapCount > 0 ) ? TRUE : FALSE;

	ETEX_Format format = DecodePixelFormat( &ddsd.ddspf );

	if (format == eTF_Unknown)
	{
		return false;
	}

	int nCompSize = file.GetLength() - sizeof(DDS_HEADER) - sizeof(DWORD);
	pCompBytes = new BYTE[nCompSize+32];
	file.ReadRaw( pCompBytes, nCompSize );

	/*
	// Read only first mip level for now:
	if (ddsd.dwFlags & DDSD_LINEARSIZE)
	{
		pCompBytes = new BYTE[ddsd.dwLinearSize];
		if (pCompBytes == NULL )
		{
			return( false );
		}
		file.Read( pCompBytes, ddsd.dwLinearSize );
	}
	else
	{
		DWORD dwBytesPerRow = ddsd.dwWidth * ddsd.ddspf.dwRGBBitCount / 8;
		pCompBytes = new BYTE[ddsd.lPitch*ddsd.dwHeight];
		if (pCompBytes == NULL )
		{
			return( false );
		}

		nCompSize = ddsd.lPitch * ddsd.dwHeight;
		nCompLineSz = dwBytesPerRow;

		BYTE* pDest = pCompBytes;
		for( DWORD yp = 0; yp < ddsd.dwHeight; yp++ )
		{
			file.Read( pDest, dwBytesPerRow );
			pDest += ddsd.lPitch;
		}
	}
	*/

	outImage.Allocate( ddsd.dwWidth,ddsd.dwHeight );
	pDecompBytes = (BYTE*)outImage.GetData();
	if (!pDecompBytes)
	{
		Warning( "Cannot allocate image %dx%d, Out of memory",ddsd.dwWidth,ddsd.dwHeight );
		delete []pCompBytes;
		return false;
	}

	if(pQualityLoss)
	{
		if(format==eTF_A4R4G4B4
		|| format==eTF_R5G6B5
		|| format==eTF_R5G5B5
		|| format==eTF_DXT1
		|| format==eTF_DXT3
		|| format==eTF_DXT5
		|| format==eTF_3DC)
			*pQualityLoss=true;
	}

	bool bOk = true;

	if (format == eTF_A8R8G8B8)
	{
		if (ddsd.ddspf.dwRGBBitCount == 24)
		{
			unsigned char *dest = pDecompBytes;
			unsigned char *src = pCompBytes;
			for (int y = 0; y < ddsd.dwHeight; y++)
			{
				for (int x = 0; x < ddsd.dwWidth; x++)
				{
					dest[0] = src[2];
					dest[1] = src[1];
					dest[2] = src[0];
					dest[3] = 255;
					dest+=4;
					src+=3;
				}
			}
		}
		else if (ddsd.ddspf.dwRGBBitCount == 32)
		{
			unsigned char *dest = pDecompBytes;
			unsigned char *src = pCompBytes;
			for (int y = 0; y < ddsd.dwHeight; y++)
			{
				for (int x = 0; x < ddsd.dwWidth; x++)
				{
					dest[0] = src[2];
					dest[1] = src[1];
					dest[2] = src[0];
					dest[3] = src[3];
					dest+=4;
					src+=4;
				}
			}
		}
	}
	else if (format == eTF_L8)
	{
		unsigned char *dest = pDecompBytes;
		unsigned char *src = pCompBytes;
		for (int y = 0; y < ddsd.dwHeight; y++)
		{
			for (int x = 0; x < ddsd.dwWidth; x++)
			{
				dest[0] = dest[1] = dest[2] = *src;
				dest[3] = 255;
				dest+=4;
				++src;
			}
		}
	}
	else /*if (format == eTF_DXT1 || format == eTF_DXT3 || format == eTF_DXT5)*/
	{
		bOk = GetIEditor()->GetRenderer()->DXTDecompress( pCompBytes,file.GetLength(),pDecompBytes,ddsd.dwWidth,ddsd.dwHeight,ddsd.dwMipMapCount,format,false,4 );
	}

	delete []pCompBytes;

	//////////////////////////////////////////////////////////////////////////
	CString strFormat = NameForTextureFormat(format);
	CString mips;
	mips.Format( " Mips:%d",ddsd.dwMipMapCount );
	strFormat += mips;

	outImage.SetFormatDescription( strFormat );

	// done reading file
	return bOk;
}