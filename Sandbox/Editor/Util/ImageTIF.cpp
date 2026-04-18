////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   imagetif.cpp
//  Version:     v1.00
//  Created:     15/3/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:		10/01/2007 Added saving (Sergey Shaykin)
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ImageTIF.h"
#include "CryFile.h"


#define _TIFF_DATA_TYPEDEFS_					// because we defined uint32,... already
#include "TiffLib/include/tiffio.h"		// TIFF library


// Function prototypes
static tsize_t libtiffDummyReadProc (thandle_t fd, tdata_t buf, tsize_t size);
static tsize_t libtiffDummyWriteProc (thandle_t fd, tdata_t buf, tsize_t size);
static toff_t libtiffDummySizeProc(thandle_t fd);
static toff_t libtiffDummySeekProc (thandle_t fd, toff_t off, int i);
//static int libtiffDummyCloseProc (thandle_t fd);

// We need globals because of the callbacks (they don't allow us to pass state)
char *globalImageBuffer=0;
unsigned long globalImageBufferOffset=0;
unsigned long globalImageBufferSize=0;



/////////////////// Callbacks to libtiff

static int libtiffDummyMapFileProc(thandle_t, tdata_t*, toff_t*)
{
	return 0;
}

static void libtiffDummyUnmapFileProc(thandle_t, tdata_t, toff_t)
{
}

static toff_t libtiffDummySizeProc(thandle_t fd)
{
	return globalImageBufferSize;
}

static tsize_t
libtiffDummyReadProc (thandle_t fd, tdata_t buf, tsize_t size)
{
	tsize_t nBytesLeft=globalImageBufferSize-globalImageBufferOffset;

	if(size>nBytesLeft)
		size=nBytesLeft;

	memcpy(buf,&globalImageBuffer[globalImageBufferOffset],size);

	globalImageBufferOffset+=size;

	// Return the amount of data read
	return size;
}

static tsize_t
libtiffDummyWriteProc (thandle_t fd, tdata_t buf, tsize_t size)
{
	return (size);
}

static toff_t
libtiffDummySeekProc (thandle_t fd, toff_t off, int i)
{
	switch(i)
	{
	case SEEK_SET:
		globalImageBufferOffset=off;
		break;

	case SEEK_CUR:
		globalImageBufferOffset+=off;
		break;

	case SEEK_END:
		globalImageBufferOffset=globalImageBufferSize-off;
		break;

	default:
		globalImageBufferOffset=off;
		break;
	}

	// This appears to return the location that it went to
	return globalImageBufferOffset;
}

static int
libtiffDummyCloseProc (thandle_t fd)
{
	// Return a zero meaning all is well
	return 0;
}



bool CImageTIF::Load( const CString &fileName, CImage &outImage )
{
	std::vector<uchar> data;
	CCryFile file;
	if (!file.Open( fileName,"rb" ))
	{
		CLogFile::FormatLine( "File not found %s",(const char*)fileName );
		return false;
	}
	globalImageBufferSize = file.GetLength();

	data.resize(globalImageBufferSize);
	globalImageBuffer = (char *)&data[0];
	globalImageBufferOffset=0;

	file.ReadRaw( globalImageBuffer,globalImageBufferSize );


	// Open the dummy document (which actually only exists in memory)
	TIFF* tif = TIFFClientOpen (fileName, "rm", (thandle_t) - 1, libtiffDummyReadProc,
		libtiffDummyWriteProc, libtiffDummySeekProc,
		libtiffDummyCloseProc, libtiffDummySizeProc, libtiffDummyMapFileProc, libtiffDummyUnmapFileProc);

//	TIFF* tif = TIFFOpen(fileName,"r");

	bool bRet=false;

	if(tif) 
	{
		uint32 dwWidth, dwHeight;
		size_t npixels;
		uint32* raster;

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &dwWidth);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &dwHeight);
		npixels = dwWidth * dwHeight;
		raster = (uint32*) _TIFFmalloc((tsize_t)(npixels * sizeof (uint32)));

		if(raster) 
		{
			if(TIFFReadRGBAImage(tif, dwWidth, dwHeight, raster, 0)) 
			{
				if(outImage.Allocate(dwWidth,dwHeight))
				{
					char *dest = (char *)outImage.GetData();
					uint32 dwPitch = dwWidth*4;

					for(uint32 dwY=0;dwY<dwHeight;++dwY)
					{
						char *src2 = (char *)&raster[(dwHeight-1-dwY)*dwWidth];
						char *dest2 = &dest[dwPitch*dwY];

						memcpy(dest2,src2,dwWidth*4);
					}
					bRet=true;

					/*
					{
						CString sSpecialInstructions = GetSpecialInstructionsFromTIFF(tif);

						char *p = sSpecialInstructions.GetBuffer();

						while(*p)
						{
							CString sKey,sValue;

							JumpOverWhitespace(p);

							if(*p != '/')								// /
							{
								m_pCC->pLog->LogError("Special instructions in TIFF '%s' are broken (/ expected)",lpszPathName);
								break;
							}
							p++;												// jump over /

							while(IsValidNameChar(*p))	// key
								sKey+=*p++;

							JumpOverWhitespace(p);

							if(*p != '=')								// =
							{
								m_pCC->pLog->LogError("Special instructions in TIFF '%s' are broken (= expected)",lpszPathName);
								break;
							}
							p++;												// jump over =

							JumpOverWhitespace(p);

							while(IsValidNameChar(*p))	// value
								sValue+=*p++;

							JumpOverWhitespace(p);

							sKey.Trim();sValue.Trim();

							m_pCC->pFileSpecificConfig->UpdateOrCreateEntry("",sKey,sValue);
						}
					}
					*/
				}
			}

			_TIFFfree(raster);
		}
		TIFFClose(tif);
	}

	if(!bRet)
		outImage.Detach();

	return bRet;
}

//////////////////////////////////////////////////////////////////////////
bool CImageTIF::Save( const CString &fileName, const CImage * inImage )
{
	bool bRet = false;
	TIFF * tif = TIFFOpen(fileName, "wb");
	if(tif)
	{
		TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, inImage->GetWidth());
		TIFFSetField(tif, TIFFTAG_IMAGELENGTH, inImage->GetHeight());
		TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
		TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
		TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, inImage->GetHeight());
		TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
		TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

		tsize_t size = TIFFWriteRawStrip(tif, 0, inImage->GetData(), 4 * inImage->GetWidth() * inImage->GetHeight());
		if(size>0)
			bRet = true;
		TIFFClose(tif);
	}
	return bRet;
}