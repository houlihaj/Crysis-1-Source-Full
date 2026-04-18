/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2008.
-------------------------------------------------------------------------
$Id: CompressedImage.cpp,v 1.0 2008/02/20 15:14:13 AntonKaplanyan Exp wwwrun $
$DateTime$
Description:  Routine for loading of compressed textures
-------------------------------------------------------------------------
History:
- 20:2:2008   10:31 : Created by Anton Kaplanyan
*************************************************************************/

#include "StdAfx.h"
#	include "CImage.h"
#	include "CompressedImage.h"
#	include "ImageExtensionHelper.h"				// CImageExtensionHelper
#	include "StringUtils.h"									// stristr()
#	include "ILog.h"
#	include "IRenderer.h"
#	include "JpgImage.h"

//#define PROFILE_DECOMPRESSION

#if defined(_XBOX) || defined(XENON) || defined (PS3) || defined(LINUX) || defined(WIN64)
#	undef  USE_IJL
#else
#	define USE_IJL
#endif

#if !defined(WIN64) && !defined(LINUX) && !defined(PS3) && !defined(LINUX)
#ifndef USE_IJL
	typedef unsigned char u_char;
#	include "jpeg6/jpeglib.h"
#else //USE_IJL
	extern "C" {
#	include "ijl.h"
	}
#endif //USE_IJL
#endif

#ifdef XENON
#	include <XGraphics.h>
// helper namespace with platform-specific descriptions and formats
namespace
{
	// check if it's DDS DXT-compressed format
	static bool IsDXTCompressed(const D3DFORMAT fmt)
	{
		if (fmt == D3DFMT_LIN_DXT1 || fmt == D3DFMT_LIN_DXT3 || 
				fmt == D3DFMT_LIN_DXT5 || fmt == D3DFMT_LIN_DXN/*|| fmt == D3DFMT_LIN_CTX1*/)
			return true;
		return false;
	}

	// returns bytes/block
	static uint32 GetBytesPerBlock(const D3DFORMAT fmt)
	{
		switch (fmt)
		{
		case D3DFMT_LIN_A8R8G8B8:
		case D3DFMT_LIN_A8B8G8R8:
		case D3DFMT_LIN_X8R8G8B8:
		case D3DFMT_LIN_X8B8G8R8:
		case D3DFMT_LIN_G16R16F:
		case D3DFMT_LIN_R32F:
			return 4;
		case D3DFMT_LIN_A4R4G4B4:
			return 2;
		case D3DFMT_LIN_DXT3:
		case D3DFMT_LIN_DXT5:
		case D3DFMT_LIN_DXN:
			return 16;
		case D3DFMT_LIN_DXT1:
		//case D3DFMT_LIN_CTX1:
			return 8;
		case D3DFMT_LIN_A8:
		case D3DFMT_LIN_L8:
			return 1;
		default:
			assert(0);
		}
		return 0;
	}
	static const char* GetNameByFormat(const D3DFORMAT fmt)
	{
#define CASE(f)	case f: return #f;
		switch (fmt)
		{
		CASE(D3DFMT_LIN_A8R8G8B8)
		CASE(D3DFMT_LIN_X8R8G8B8)
		CASE(D3DFMT_LIN_G16R16F)
		CASE(D3DFMT_LIN_R32F)
		CASE(D3DFMT_LIN_A4R4G4B4)
		CASE(D3DFMT_LIN_DXT3)
		CASE(D3DFMT_LIN_DXT5)
		CASE(D3DFMT_LIN_DXN)
		CASE(D3DFMT_LIN_DXT1)
		//CASE(D3DFMT_LIN_CTX1)
		CASE(D3DFMT_LIN_A8)
		CASE(D3DFMT_LIN_L8)
		default:
			assert(0);
			return "UNKNOWN!";
		}
#undef CASE
	}
}
#endif //XENON

// loads heterogeneous texture chunk and tries 
// to find compressed chunks' descriptions and decompress it
CCompressedImageDDSFile::CCompressedImageDDSFile( const byte* pInData, const uint32 nCompressedSize, const uint32 nDecompressedSize )
{
#	ifdef PROFILE_DECOMPRESSION
	DWORD startTime = GetTickCount();
	enum
	{
#ifndef XENON
		JPEGTime = 0,
#else
		PTCTime,
		MCTTime,
#endif
		numProfiles
	};
	DWORD times[numProfiles] = {0
#ifdef XENON
		, 0, 0
#endif
	};
#	endif

	const byte* pComressedData = pInData;
	const byte* pDataEnd = pInData + nCompressedSize;
	bool bPartiallyDecompressed = false;

	ETEX_Format textureFormat;

	// reserve enough memory
	m_uncompressedContent.reserve(nCompressedSize);

	int32 elapsedDecompressedSize = nDecompressedSize;
	// start seeking for compressed chunks
	while(pComressedData < pDataEnd)
	{
		const uint32 elapsedSize = pDataEnd - pComressedData;
		// try to cast header of data to different chunk descs
		const JPEGDesc* pJPEGDesc = (const JPEGDesc*)(pComressedData);
#ifdef XENON
		const PTCDesc* pPTCDesc = (const PTCDesc*)(pComressedData);
		const MCTDesc* pMCTDesc = (const MCTDesc*)(pComressedData);

		// if we have PTC 
		if(pPTCDesc->version == PTCDesc::eptcdvDefault && elapsedSize > sizeof(PTCDesc))
		{
#	ifdef PROFILE_DECOMPRESSION
			DWORD startPTCTime = GetTickCount();
#	endif
			// decompress this chunks as PTC compressed chunk and continue seeking
			const size_t decompSize = DecompressPTC(pComressedData, elapsedDecompressedSize);
			if(!decompSize)
			{
				assert(0);
				gEnv->pLog->LogError("Error: PTC decompression failed");
				m_uncompressedContent.clear();
				return;
			}
			else
				elapsedDecompressedSize -= decompSize;
			assert(elapsedDecompressedSize >= 0);
			if(!bPartiallyDecompressed)
				textureFormat = pPTCDesc->format;
			else
				assert(textureFormat == pPTCDesc->format);
			// shift seeking pointer
			assert(pComressedData + pPTCDesc->compressedSize + sizeof(PTCDesc) <= pDataEnd);
			pComressedData += pPTCDesc->compressedSize + sizeof(PTCDesc);
			bPartiallyDecompressed |= decompSize != 0;
#	ifdef PROFILE_DECOMPRESSION
			times[PTCTime] += GetTickCount() - startPTCTime;
#	endif
		}
		// if we have MCT 
		else if(pMCTDesc->version == MCTDesc::emctdvDefault && elapsedSize > sizeof(MCTDesc))
		{
#	ifdef PROFILE_DECOMPRESSION
			DWORD startMCTTime = GetTickCount();
#	endif
			// decompress this chunks as MCT compressed chunk and continue seeking
			const size_t decompSize = DecompressMCT(pComressedData, elapsedDecompressedSize);
			if(!decompSize)
			{
				assert(0);
				gEnv->pLog->LogError("Error: MCT decompression failed");
				m_uncompressedContent.clear();
				return;
			}
			else
				elapsedDecompressedSize -= decompSize;
			assert(elapsedDecompressedSize >= 0);
			if(!bPartiallyDecompressed)
				textureFormat = pMCTDesc->format;
			else
				assert(textureFormat == pMCTDesc->format);
			assert(pComressedData + pMCTDesc->compressedSize + sizeof(MCTDesc) <= pDataEnd);
			// shift seeking pointer
			pComressedData += pMCTDesc->compressedSize + sizeof(MCTDesc);
			bPartiallyDecompressed |= decompSize != 0;
#	ifdef PROFILE_DECOMPRESSION
			times[MCTTime] += GetTickCount() - startMCTTime;
#	endif
		}
		// if we have JPEG 
		else 
#else
			if(pJPEGDesc->version == JPEGDesc::ejpegdvDefault && elapsedSize > sizeof(JPEGDesc))
		{
#	ifdef PROFILE_DECOMPRESSION
			DWORD startJPEGTime = GetTickCount();
#	endif
			// decompress this chunks as MCT compressed chunk and continue seeking
			size_t compSize;
			size_t decompSize;
			if(!DecompressJPEG(pComressedData, &compSize, elapsedDecompressedSize, &decompSize))
			{
				gEnv->pLog->LogError("Error: JPEG decompression failed");
				m_uncompressedContent.clear();
				return;
			}
			elapsedDecompressedSize -= decompSize;
			assert(elapsedDecompressedSize >= 0);
			if(!bPartiallyDecompressed)
				textureFormat = pJPEGDesc->srcFormat;
			else
				assert(textureFormat == pJPEGDesc->srcFormat);
			assert(pComressedData + compSize <= pDataEnd);
			// shift seeking pointer
			pComressedData += compSize;
			bPartiallyDecompressed = true;
#	ifdef PROFILE_DECOMPRESSION
			times[JPEGTime] += GetTickCount() - startJPEGTime;
#	endif
		}
		else
#endif	// XENON
			if(bPartiallyDecompressed && elapsedSize <= CTexture::ComponentsForFormat(textureFormat))	// just in case of last MIP for PTC
		{
			// if we have the last 1x1 MIP only then just copy it
			// because PTC has some bug with 1x1 last MIP
			if(elapsedSize > 0)
			{
				size_t offset = m_uncompressedContent.size();
				m_uncompressedContent.resize(offset + elapsedSize);
				memcpy(&m_uncompressedContent[offset], pComressedData, elapsedSize);
				pComressedData = pDataEnd;
			}
		}
		else
		{
			// copy this part
			m_uncompressedContent.push_back(*pComressedData);
			// and continue seeking
			pComressedData++;
			elapsedDecompressedSize--;
			assert(elapsedDecompressedSize >= 0);
		}
	}
	assert(pComressedData == pDataEnd);
	// if failed, clear the result buffer
	if(!bPartiallyDecompressed)
	{
		m_uncompressedContent.clear();
		gEnv->pLog->LogError("Error: Compressed chunks not found in the image data");
		if(pDataEnd - pInData >= 4)
			gEnv->pLog->LogError("   Data begining: %c%c%c%c", pInData[0], pInData[1], pInData[2], pInData[3]);
	}
#	ifdef PROFILE_DECOMPRESSION
	else
	{
		DWORD totalTime = GetTickCount() - startTime;
		CryLogAlways("Texture decompressed: %dms, PTC: %dms, MCT: %dms, Seeking: %dms", totalTime, 
																				times[PTCTime], times[MCTTime], 
																				totalTime - times[PTCTime] - times[MCTTime]);
	}
#	endif
}

byte* CCompressedImageDDSFile::GetData()
{
	assert(m_uncompressedContent.size());
	return &m_uncompressedContent[0];
}

size_t CCompressedImageDDSFile::GetSize() const
{
	return m_uncompressedContent.size();
}

#ifdef XENON
size_t CCompressedImageDDSFile::DecompressMCT( const byte* pInData, const uint32 nDecompressedSize )
{
	D3DBOX region;
	XGTEXTURE_DESC desc;
	UINT mipLevelCount;
	UINT minMipLevel;
	UINT maxMipLevel;

	HRESULT hr = E_FAIL;

	// check the MCT header for correctness
	const MCTDesc* pDesc = (MCTDesc*)pInData;
	if(pDesc->version != MCTDesc::emctdvDefault)
	{
		gEnv->pLog->LogError("Wrong MCT version!");
		return 0;
	}

	if(nDecompressedSize < pDesc->decompressedSize)
	{
		gEnv->pLog->LogError("Not enough memory for MCT decompression!");
		return 0;
	}

	const byte* pMCTData = pInData + sizeof(MCTDesc);

	// get internal MCT desc
	XGMCTGetDesc(pMCTData, pDesc->compressedSize, &desc, &mipLevelCount, &minMipLevel, &maxMipLevel, &region);
	assert(desc.ResourceType == D3DRTYPE_TEXTURE);
	assert(region.Top == 0 && region.Left == 0);
	assert(region.Back == 1 && region.Front == 0);
	assert(region.Right == desc.Width && region.Bottom == desc.Height);
	assert(pDesc->numMips > 0);

	// create texture for decompression
	IDirect3DTexture9* pTempTexture = new IDirect3DTexture9;
	UINT nTextureSize = XGSetTextureHeader(desc.Width, desc.Height, pDesc->numMips, D3DUSAGE_CPU_CACHED_MEMORY, 
																					(D3DFORMAT)CTexture::GetD3DLinFormat(CTexture::DeviceFormatFromTexFormat(pDesc->format)) , 
																					D3DPOOL_DEFAULT, 0, XGHEADER_CONTIGUOUS_MIP_OFFSET, desc.RowPitch, pTempTexture, NULL, NULL);
	void* pBaseBuffer = XPhysicalAlloc( nTextureSize, MAXULONG_PTR, 0, PAGE_READWRITE );
	assert(pBaseBuffer);
	XGOffsetResourceAddress( pTempTexture, pBaseBuffer ); 

	// decompress
	if(SUCCEEDED(hr = XGMCTDecompressTexture(NULL, pTempTexture, NULL, pMCTData, pDesc->compressedSize, 0)))
	{
		// upload padded data into raw format
		UploadFromTexture(pTempTexture);
	}
	else
	{
		assert(0);
		gEnv->pLog->LogError("MCT decompression failed!");
	}

	// dealloc texture data
	delete pTempTexture;
	XPhysicalFree(pBaseBuffer);
	
	return SUCCEEDED(hr) ? pDesc->decompressedSize : 0;
}

size_t CCompressedImageDDSFile::DecompressPTC( const byte* pInData, const uint32 nDecompressedSize )
{
	UINT nWidth = -1;
	UINT nHeight = -1;
	D3DFORMAT PTCNativeFormat = D3DFMT_UNKNOWN;
	HRESULT hr = E_FAIL;

	const byte* pComressedData = pInData;

	const PTCDesc* pPTCDesc = (const PTCDesc*)(pInData);
	const byte* pPTCData = pInData + sizeof(PTCDesc);

	// check the PTC header for correctness
	if(pPTCDesc->version != PTCDesc::eptcdvDefault)
	{
		gEnv->pLog->LogError("Wrong PTC version!");
		return 0;
	}

	// check if we have unsupported format(PTC has problems with it)
	if(pPTCDesc->format == eTF_L8 || pPTCDesc->format == eTF_A8 || pPTCDesc->format == eTF_G16R16F)
	{
		gEnv->pLog->LogError("L8, A8 and R16G16F formats are not supported by PTC");
		return 0;
	}

	// get internal PTC desc
	if(FAILED(hr = XGGetPTCImageDesc(pPTCData, pPTCDesc->compressedSize, &nWidth, &nHeight, &PTCNativeFormat)))
	{
		gEnv->pLog->LogError("Wrong PTC data");
		return 0;
	}

	// if we need to compress to DXT, it's the special case
	ETEX_Format srcFromat = pPTCDesc->format;
	bool RTCompress = false;

	if(pPTCDesc->format == eTF_DXT5)
	{
		srcFromat = eTF_A8R8G8B8;
		RTCompress = true;
	}
	else if(pPTCDesc->format == eTF_DXT1)
	{
		srcFromat = eTF_X8R8G8B8;
		RTCompress = true;
	}

	if(GetBytesPerBlock(PTCNativeFormat) != CTexture::ComponentsForFormat(srcFromat))
	{
		assert(0);
		gEnv->pLog->LogWarning("Warning: Used non-native format for PTC decompression: src: %s, dst: %s", GetNameByFormat(PTCNativeFormat), CTexture::NameForTextureFormat(srcFromat));
	}

	uint32 uncomressedSize = 0;

	assert(pPTCDesc->numMips > 0 && pPTCDesc->numMips <= PTCDesc::numPTCMIPs);

	for(uint32 iMip = 0;iMip < pPTCDesc->numMips;++iMip)
	{
		// determine size in blocks and pitch
		uint32 nMipWidth = max(1u, nWidth >> iMip);
		uint32 nMipHeight = max(1u, nHeight >> iMip);
		if(CTexture::IsDXTCompressed(srcFromat))
		{
			nMipWidth  = (nMipWidth  + 3) / 4;
			nMipHeight = (nMipHeight + 3) / 4;
		}
		const uint32 nBlockRowPitch = nMipWidth * CTexture::ComponentsForFormat(srcFromat);
		const uint32 nMipDecompressedSize = nBlockRowPitch * nMipHeight;
		assert(nMipDecompressedSize > 0);

		const size_t offset = m_uncompressedContent.size();
		m_uncompressedContent.resize(offset + nMipDecompressedSize);

		// try to decompress the chunk
		if(FAILED(hr = XGPTCDecompressSurfaceEx(&m_uncompressedContent[offset], nBlockRowPitch,
																					nWidth, nHeight, (D3DFORMAT)CTexture::GetD3DLinFormat(CTexture::DeviceFormatFromTexFormat(srcFromat)), 
																					NULL, pPTCData, pPTCDesc->compressedSize, NULL, iMip)))
		{
			m_uncompressedContent.clear();
			gEnv->pLog->LogError("Failed to decompress PTC data");
			assert(0);
			return 0;
		}

		if(RTCompress)
		{
			std::vector<byte> rawData(nMipDecompressedSize);
			memcpy(&rawData[0], &m_uncompressedContent[offset], nMipDecompressedSize);
			m_uncompressedContent.resize(offset);	// shrink back

			if(srcFromat == eTF_A8R8G8B8 && pPTCDesc->format == eTF_DXT5)
			{
				CompressToDXT(rawData, m_uncompressedContent, nMipWidth, nMipHeight, true);
			}
			else if(srcFromat == eTF_X8R8G8B8 && pPTCDesc->format == eTF_DXT1)
			{
				CompressToDXT(rawData, m_uncompressedContent, nMipWidth, nMipHeight, false);
			}
			else
			{
				gEnv->pLog->LogError("Failed to recompress PTC data into DXT format: unknown formats combination");
				assert(0);
				return 0;
			}
		}

		const size_t decompressedFinalSize = m_uncompressedContent.size() - offset;

		// check if we already have the necessary budget
		if(uncomressedSize + decompressedFinalSize > nDecompressedSize)
		{
			m_uncompressedContent.resize(offset);	// shrink back
			break;
		}

		uncomressedSize += decompressedFinalSize;	// new size minus old size
	}

	return uncomressedSize;
}

// upload decompressed data from texture
void CCompressedImageDDSFile::UploadFromTexture( IDirect3DTexture9* pTexture)
{
	D3DLOCKED_RECT rect;
	D3DSURFACE_DESC desc;
	for(uint32 iMip=0;iMip<pTexture->GetLevelCount();++iMip)
	{
		pTexture->GetLevelDesc(iMip, &desc);

		// determine size in blocks
		uint32 nBlockWidth = desc.Width;
		uint32 nBlockHeight = desc.Height;

		if(IsDXTCompressed(desc.Format))
		{
			nBlockWidth  = (nBlockWidth  + 3) / 4;
			nBlockHeight = (nBlockHeight + 3) / 4;
		}

		// determine pitch and MIP size in bytes
		const uint32 nBlockRowPitch = nBlockWidth * GetBytesPerBlock(desc.Format);
		const size_t offset = m_uncompressedContent.size();

		// allocate memory
		m_uncompressedContent.resize(offset + nBlockRowPitch * nBlockHeight);
		byte* pData = &m_uncompressedContent[offset];

		// copy MIP data
		pTexture->LockRect(iMip, &rect, NULL, D3DLOCK_READONLY);
		byte* pRow = (byte*)rect.pBits;
		for(uint32 i = 0;i < nBlockHeight;++i)
		{
			memcpy(pData, pRow, nBlockRowPitch);
			pData += nBlockRowPitch;
			pRow += rect.Pitch;
			assert(pData - &m_uncompressedContent[0] <= m_uncompressedContent.size());
		}
		pTexture->UnlockRect(iMip);
	}
}

#endif

// helpers for JPEG formats
namespace
{
	static bool HasAttachedChannel(const ETEX_Format fmt)
	{
		switch (fmt)
		{
		case eTF_DXT5:
		case eTF_A8R8G8B8:
			return true;
		default:;
		}
		return false;
	}
	static bool NeedDXTCompress(const ETEX_Format src)
	{
		if(src == eTF_DXT5 )
			return true;
		if(src == eTF_DXT1 )
			return true;
		return false;
	}
}

bool CCompressedImageDDSFile::DecompressJPEG( const byte* pInData, size_t* comprSize, const uint32 nDecompressedSize, size_t* decomprSize )
{
	assert(decomprSize);
	assert(comprSize);
	*decomprSize = 0;
	*comprSize = 0;

	// check the JPEG header for correctness
	const JPEGDesc* pDesc = (JPEGDesc*)pInData;
	if(pDesc->version != JPEGDesc::ejpegdvDefault)
	{
		gEnv->pLog->LogError("Wrong JPEG version!");
		return false;
	}

	const byte* pJPEGData = pInData + sizeof(JPEGDesc);

	size_t decompressedSize = 0;
	size_t compressedSize = 0;

	const size_t offset = m_uncompressedContent.size();

	const bool alpha = HasAttachedChannel(pDesc->srcFormat);

	if(!alpha)	// just decompress color part
	{
		m_uncompressedContent.resize(offset + pDesc->decompressedSize);
		if(!DecompressPureJPEGChunk(pDesc, pJPEGData, &m_uncompressedContent[offset]))
		{
			return false;
		}
		decompressedSize = pDesc->decompressedSize;
		compressedSize = pDesc->compressedSize + sizeof(JPEGDesc);

		// expand to 4-bytes aligned format if necessary
		if(pDesc->dataFormat == eTF_R8G8B8)
		{
			if(pDesc->srcFormat == eTF_X8R8G8B8 || pDesc->srcFormat == eTF_DXT1)
			{
				assert(pDesc->width * pDesc->height * 3 == decompressedSize);
				std::vector<byte> tmpSrc(decompressedSize);
				memcpy(&tmpSrc[0], &m_uncompressedContent[offset], decompressedSize);

				decompressedSize = pDesc->width * pDesc->height * 4;
				m_uncompressedContent.resize(offset + decompressedSize);

				// interleave X channel
				byte* pStartData = &m_uncompressedContent[offset];
				for(uint32 iTexel = 0;iTexel < pDesc->width * pDesc->height;++iTexel)
				{
					pStartData[iTexel*4+1] = tmpSrc[iTexel*3+2];
					pStartData[iTexel*4+2] = tmpSrc[iTexel*3+1];
					pStartData[iTexel*4+3] = tmpSrc[iTexel*3+0];
					pStartData[iTexel*4+0] = 0;
				}
			}
			else
				assert(0);
		}
	}
	else	// load color and alpha indep, then interleave it
	{
		std::vector<byte> tempColors(pDesc->decompressedSize);

		// decompress colors
		if(!DecompressPureJPEGChunk(pDesc, pJPEGData, &tempColors[0]))
		{
			return false;
		}
		decompressedSize = pDesc->decompressedSize;
		compressedSize = pDesc->compressedSize + sizeof(JPEGDesc);
		const JPEGDesc* pAlphaDesc = (JPEGDesc*)(pJPEGData + pDesc->compressedSize);
		if(pAlphaDesc->version != JPEGDesc::ejpegdvDefault)
		{
			gEnv->pLog->LogError("Wrong JPEG alpha attachment version!");
			return false;
		}

		// get the guaranteed next alpha chunk
		const byte* pJPEGAlphaData = pJPEGData + pDesc->compressedSize + sizeof(JPEGDesc);
		assert(pAlphaDesc->dataFormat == eTF_A8);
		assert(pAlphaDesc->srcFormat == pDesc->srcFormat);
		assert(pAlphaDesc->decompressedSize * 3 == pDesc->decompressedSize);
		assert(pAlphaDesc->width == pDesc->width);
		assert(pAlphaDesc->height == pDesc->height);

		// decompress alpha
		std::vector<byte> tempAlpha(pAlphaDesc->decompressedSize);
		if(!DecompressPureJPEGChunk(pAlphaDesc, pJPEGAlphaData, &tempAlpha[0]))
		{
			gEnv->pLog->LogError("Unable to decompress JPEG alpha attachment!");
			return false;
		}

		compressedSize += pAlphaDesc->compressedSize + sizeof(JPEGDesc);
		decompressedSize += pAlphaDesc->decompressedSize;

		m_uncompressedContent.resize(offset + decompressedSize);

		// interleave alpha channel
		byte* pStartData = &m_uncompressedContent[offset];
		for(uint32 iTexel = 0;iTexel < pDesc->width * pDesc->height;++iTexel)
		{
			pStartData[iTexel*4+0] = tempColors[iTexel*3+2];
			pStartData[iTexel*4+1] = tempColors[iTexel*3+1];
			pStartData[iTexel*4+2] = tempColors[iTexel*3+0];
			pStartData[iTexel*4+3] = tempAlpha[iTexel];
		}
	}

	// compress to DXT if necessary
	if(NeedDXTCompress(pDesc->srcFormat))
	{
		std::vector<byte> tmpSrc(decompressedSize);
		memcpy(&tmpSrc[0], &m_uncompressedContent[offset], decompressedSize);
		m_uncompressedContent.resize(offset);	// shrink back
		CompressToDXT(tmpSrc, m_uncompressedContent, pDesc->width, pDesc->height, pDesc->srcFormat == eTF_DXT5);
		assert(m_uncompressedContent.size() > offset);
		decompressedSize = m_uncompressedContent.size() - offset;
	}

	// check for memory budget
	if(nDecompressedSize < decompressedSize)
	{
		gEnv->pLog->LogError("Not enough memory for JPEG decompression!");
		return false;
	}

	*comprSize = compressedSize;
	*decomprSize = decompressedSize;
	return compressedSize > 0 && decompressedSize > 0;
}

bool CCompressedImageDDSFile::DecompressPureJPEGChunk( const JPEGDesc* pDesc, const byte* pInData, byte* pOutData )
{
#ifndef XENON
//#	ifdef PS3
//	// use libjpegdec from PS3 SDK with parallel decompression on SPUs
//
//	return true;
//#	else !PS3
	// else use simple jpeg6 library
#ifdef USE_IJL
	JPEG_CORE_PROPERTIES image;
	ZeroStruct( image );

	if( ijlInit( &image ) != IJL_OK )
	{
		gEnv->pLog->LogError ("Cannot initialize Intel JPEG library");
		return false;
	}
	image.JPGBytes = (byte*)pInData;
	image.JPGSizeBytes = pDesc->compressedSize;
	image.DIBBytes = (byte*)pOutData;
	if( ijlRead( &image, IJL_JBUFF_READPARAMS ) != IJL_OK )
	{
		gEnv->pLog->LogError ("Cannot read JPEG file header");
		ijlFree( &image );
		return false;
	}

	switch(image.JPGChannels)
	{
	case 1:
		image.JPGColor    = IJL_G;
		image.DIBChannels = 1;
		image.DIBColor    = IJL_G;
		break;
	case 3:
		image.JPGColor    = IJL_YCBCR;
		image.DIBChannels = 3;
		image.DIBColor    = IJL_RGB;
		break;
	default:
		assert(0);
		gEnv->pLog->LogError("Wrong JPEG format");
		return false;
	}

	image.DIBWidth    = image.JPGWidth;
	image.DIBHeight   = image.JPGHeight;
	if( ijlRead( &image, IJL_JBUFF_READWHOLEIMAGE ) != IJL_OK )
	{
		gEnv->pLog->LogError("Cannot read image data");
		ijlFree( &image );
		return false;
	}

	if( ijlFree( &image ) != IJL_OK )
		gEnv->pLog->LogWarning("Cannot free Intel(R) JPEG library");

	return true;
#else
	gEnv->pLog->LogError("JPEG decompression is not implemented on this platform!");
	return false;
#endif	// USE_IJL
//#	endif // PS3
#else	// XENON
	gEnv->pLog->LogError("JPEG decompression is not implemented on this platform!");
	return false;
#endif	// !XENON
}

CCompressedImageDDSFile* CCompressedImageDDSFile::LoadFromMemory( const byte* pCompressed, const uint32 nCompressedSize, const uint32 nDecompressedSize )
{
	// try to decompress image
	CCompressedImageDDSFile* pData = new CCompressedImageDDSFile(pCompressed, nCompressedSize, nDecompressedSize);
	if(!pData->GetSize())
		SAFE_DELETE(pData);

	return pData;
}

bool CCompressedImageDDSFile::Decompress( const byte* pCompressed, const uint32 nCompressedSize, byte* pDecompressed, const uint32 nBufferSize )
{
	assert(nCompressedSize <= nBufferSize);
	bool res = false;
	CCompressedImageDDSFile* pData = NULL;
	// in-place decompression
	if (pData = LoadFromMemory(pCompressed, nCompressedSize, nBufferSize))
	{
		if(pData->GetSize() <= nBufferSize)
		{
			// load decompressed data
			assert(pData->GetSize() == nBufferSize);
			memcpy(pDecompressed, pData->GetData(), pData->GetSize());
			res = true;
		}
		else
		{
			assert(0);
			gEnv->pLog->LogError("Insufficient memory to decompress texture: %dB allocated, %dB needed", nBufferSize, pData->GetSize());
		}
	}
	else
		gEnv->pLog->LogError("Failed to decompress texture");
	SAFE_DELETE(pData);
	return res;
}

// rt DXT compression
void CCompressedImageDDSFile::CompressToDXT( const std::vector<byte>& vecInData, std::vector<byte>& outData, const int width, const int height, const bool isDXT5 )
{
	const size_t offset = outData.size();

	const size_t mipSize = ((width + 3) / 4) * ((height + 3) / 4) * (isDXT5 ? 16 : 8);
	outData.resize(offset + mipSize);
	int size;
	RTDXTCompression::CompressImageDXT(&vecInData[0], &outData[offset], width, height, size, isDXT5);
	if(mipSize != size)
	{
		assert(0);
		gEnv->pLog->LogError("Failed to compress texture to DXT: %dB allocated, %dB compressed", mipSize, size);
	}
}

namespace RTDXTCompression
{
	void CompressImageDXT( const byte *inBuf, byte *outBuf, int width, int height, int &outputBytes, const bool isDXT5 )
	{
		byte* pBegining = outBuf;
		byte block[64];

		if(isDXT5)
		{
			byte minColor[4];
			byte maxColor[4];

			for ( int j = 0; j < height; j += 4, inBuf += width * 4*4 )
			{
				for ( int i = 0; i < width; i += 4 )
				{
					ExtractBlock( inBuf + i * 4, width, block );
					GetMinMaxColorsAndAlpha( block, minColor, maxColor );
#ifdef XENON
					EmitByte( minColor[0], &outBuf );
					EmitByte( maxColor[0], &outBuf );
					EmitAlphaIndices( block, minColor[0], maxColor[0], &outBuf );
					EmitWord( ColorTo565( maxColor+1 ), &outBuf );
					EmitWord( ColorTo565( minColor+1 ), &outBuf );
					EmitColorIndices( block+1, minColor+1, maxColor+1, &outBuf );
#else
					EmitByte( maxColor[3], &outBuf );
					EmitByte( minColor[3], &outBuf );
					EmitAlphaIndices( block, minColor[3], maxColor[3], &outBuf );
					EmitWord( ColorTo565( maxColor ), &outBuf );
					EmitWord( ColorTo565( minColor ), &outBuf );
					EmitColorIndices( block, minColor, maxColor, &outBuf );
#endif
				}
			}
		}
		else
		{
			byte minColor[3];
			byte maxColor[3];

			for ( int j = 0; j < height; j += 4, inBuf += width * 4*4 )
			{
				for ( int i = 0; i < width; i += 4 )
				{
					ExtractBlock( inBuf + i * 4, width, block );
#ifdef XENON
					GetMinMaxColors( block+1, minColor, maxColor );
					EmitWord( ColorTo565( maxColor ), &outBuf );
					EmitWord( ColorTo565( minColor ), &outBuf );
					EmitColorIndices( block+1, minColor, maxColor, &outBuf );
#else
					GetMinMaxColors( block, minColor, maxColor );
					EmitWord( ColorTo565( maxColor ), &outBuf );
					EmitWord( ColorTo565( minColor ), &outBuf );
					EmitColorIndices( block, minColor, maxColor, &outBuf );
#endif
				}
			}
		}
		outputBytes = outBuf - pBegining;
	}

	void GetMinMaxColors( const byte *colorBlock, byte *minColor, byte *maxColor )
	{
		int i;
		byte inset[3];
		minColor[0] = minColor[1] = minColor[2] = 255;
		maxColor[0] = maxColor[1] = maxColor[2] = 0;
		for ( i = 0; i < 16; i++ )
		{
			minColor[0] = min(colorBlock[i*4+0], minColor[0]);
			minColor[1] = min(colorBlock[i*4+1], minColor[1]);
			minColor[2] = min(colorBlock[i*4+2], minColor[2]);

			maxColor[0] = max(colorBlock[i*4+0], maxColor[0]);
			maxColor[1] = max(colorBlock[i*4+1], maxColor[1]);
			maxColor[2] = max(colorBlock[i*4+2], maxColor[2]);
		}
		inset[0] = ( maxColor[0] - minColor[0] ) >> INSET_SHIFT;
		inset[1] = ( maxColor[1] - minColor[1] ) >> INSET_SHIFT;
		inset[2] = ( maxColor[2] - minColor[2] ) >> INSET_SHIFT;
		minColor[0] = ( minColor[0] + inset[0] <= 255 ) ? minColor[0] + inset[0] : 255;
		minColor[1] = ( minColor[1] + inset[1] <= 255 ) ? minColor[1] + inset[1] : 255;
		minColor[2] = ( minColor[2] + inset[2] <= 255 ) ? minColor[2] + inset[2] : 255;
		maxColor[0] = ( maxColor[0] >= inset[0] ) ? maxColor[0] - inset[0] : 0;
		maxColor[1] = ( maxColor[1] >= inset[1] ) ? maxColor[1] - inset[1] : 0;
		maxColor[2] = ( maxColor[2] >= inset[2] ) ? maxColor[2] - inset[2] : 0;
	}
	void EmitAlphaIndices( const byte *colorBlock, const byte minAlpha, const byte maxAlpha, byte** ppData )
	{
		assert( maxAlpha >= minAlpha );
		byte indices[16];
		byte mid = ( maxAlpha - minAlpha ) / ( 2 * 7 );
		byte ab1 = minAlpha + mid;
		byte ab2 = ( 6 * maxAlpha + 1 * minAlpha ) / 7 + mid;
		byte ab3 = ( 5 * maxAlpha + 2 * minAlpha ) / 7 + mid;
		byte ab4 = ( 4 * maxAlpha + 3 * minAlpha ) / 7 + mid;
		byte ab5 = ( 3 * maxAlpha + 4 * minAlpha ) / 7 + mid;
		byte ab6 = ( 2 * maxAlpha + 5 * minAlpha ) / 7 + mid;
		byte ab7 = ( 1 * maxAlpha + 6 * minAlpha ) / 7 + mid;
#ifndef XENON
		colorBlock += 3;
#endif
		for ( int i = 0; i < 16; i++ ) {
			byte a = colorBlock[i*4];
			int b1 = ( a <= ab1 );
			int b2 = ( a <= ab2 );
			int b3 = ( a <= ab3 );
			int b4 = ( a <= ab4 );
			int b5 = ( a <= ab5 );
			int b6 = ( a <= ab6 );
			int b7 = ( a <= ab7 );
			int index = ( b1 + b2 + b3 + b4 + b5 + b6 + b7 + 1 ) & 7;
			indices[i] = index ^ ( 2 > index );
		}
#ifdef XENON
		EmitByte( (indices[ 2] >> 2) | (indices[ 3] << 1) | (indices[ 4] << 4) | (indices[ 5] << 7), ppData );
		EmitByte( (indices[ 0] >> 0) | (indices[ 1] << 3) | (indices[ 2] << 6), ppData );
		EmitByte( (indices[ 8] >> 0) | (indices[ 9] << 3) | (indices[10] << 6), ppData );
		EmitByte( (indices[ 5] >> 1) | (indices[ 6] << 2) | (indices[ 7] << 5), ppData );
		EmitByte( (indices[13] >> 1) | (indices[14] << 2) | (indices[15] << 5), ppData );
		EmitByte( (indices[10] >> 2) | (indices[11] << 1) | (indices[12] << 4) | (indices[13] << 7), ppData );
#else
		EmitByte( (indices[ 0] >> 0) | (indices[ 1] << 3) | (indices[ 2] << 6), ppData );
		EmitByte( (indices[ 2] >> 2) | (indices[ 3] << 1) | (indices[ 4] << 4) | (indices[ 5] << 7), ppData );
		EmitByte( (indices[ 5] >> 1) | (indices[ 6] << 2) | (indices[ 7] << 5), ppData );
		EmitByte( (indices[ 8] >> 0) | (indices[ 9] << 3) | (indices[10] << 6), ppData );
		EmitByte( (indices[10] >> 2) | (indices[11] << 1) | (indices[12] << 4) | (indices[13] << 7), ppData );
		EmitByte( (indices[13] >> 1) | (indices[14] << 2) | (indices[15] << 5), ppData );
#endif
	}

	void EmitColorIndices( const byte *colorBlock, const byte *minColor, const byte *maxColor, byte** ppData )
	{
		word colors[4][4];
		dword result = 0;
		colors[0][0] = ( maxColor[0] & C565_5_MASK ) | ( maxColor[0] >> 5 );
		colors[0][1] = ( maxColor[1] & C565_6_MASK ) | ( maxColor[1] >> 6 );
		colors[0][2] = ( maxColor[2] & C565_5_MASK ) | ( maxColor[2] >> 5 );
		colors[1][0] = ( minColor[0] & C565_5_MASK ) | ( minColor[0] >> 5 );
		colors[1][1] = ( minColor[1] & C565_6_MASK ) | ( minColor[1] >> 6 );
		colors[1][2] = ( minColor[2] & C565_5_MASK ) | ( minColor[2] >> 5 );
		colors[2][0] = ( 2 * colors[0][0] + 1 * colors[1][0] ) / 3;
		colors[2][1] = ( 2 * colors[0][1] + 1 * colors[1][1] ) / 3;
		colors[2][2] = ( 2 * colors[0][2] + 1 * colors[1][2] ) / 3;
		colors[3][0] = ( 1 * colors[0][0] + 2 * colors[1][0] ) / 3;
		colors[3][1] = ( 1 * colors[0][1] + 2 * colors[1][1] ) / 3;
		colors[3][2] = ( 1 * colors[0][2] + 2 * colors[1][2] ) / 3;
		for ( int i = 15; i >= 0; i-- )
		{
			int c0 = colorBlock[i*4+0];
			int c1 = colorBlock[i*4+1];
			int c2 = colorBlock[i*4+2];
			int d0 = abs( colors[0][0] - c0 ) + abs( colors[0][1] - c1 ) + abs( colors[0][2] - c2 );
			int d1 = abs( colors[1][0] - c0 ) + abs( colors[1][1] - c1 ) + abs( colors[1][2] - c2 );
			int d2 = abs( colors[2][0] - c0 ) + abs( colors[2][1] - c1 ) + abs( colors[2][2] - c2 );
			int d3 = abs( colors[3][0] - c0 ) + abs( colors[3][1] - c1 ) + abs( colors[3][2] - c2 );
			int b0 = d0 > d3;
			int b1 = d1 > d2;
			int b2 = d0 > d2;
			int b3 = d1 > d3;
			int b4 = d2 > d3;
			int x0 = b1 & b2;
			int x1 = b0 & b3;
			int x2 = b0 & b4;
			result |= ( x2 | ( ( x0 | x1 ) << 1 ) ) << ( i << 1 );
		}
		EmitDoubleWord( result, ppData );
	}

	void GetMinMaxColorsAndAlpha( const byte *colorBlock, byte *minColor, byte *maxColor )
	{
		int i;
		byte inset[4];
		minColor[0] = minColor[1] = minColor[2] = minColor[3] = 255;
		maxColor[0] = maxColor[1] = maxColor[2] = maxColor[3] = 0;
		for ( i = 0; i < 16; i++ )
		{
			minColor[0] = min(colorBlock[i*4+0], minColor[0]);
			minColor[1] = min(colorBlock[i*4+1], minColor[1]);
			minColor[2] = min(colorBlock[i*4+2], minColor[2]);
			minColor[3] = min(colorBlock[i*4+3], minColor[3]);

			maxColor[0] = max(colorBlock[i*4+0], maxColor[0]);
			maxColor[1] = max(colorBlock[i*4+1], maxColor[1]);
			maxColor[2] = max(colorBlock[i*4+2], maxColor[2]);
			maxColor[3] = max(colorBlock[i*4+3], maxColor[3]);
		}
		inset[0] = ( maxColor[0] - minColor[0] ) >> INSET_SHIFT;
		inset[1] = ( maxColor[1] - minColor[1] ) >> INSET_SHIFT;
		inset[2] = ( maxColor[2] - minColor[2] ) >> INSET_SHIFT;
		inset[3] = ( maxColor[3] - minColor[3] ) >> INSET_SHIFT;
		minColor[0] = ( minColor[0] + inset[0] <= 255 ) ? minColor[0] + inset[0] : 255;
		minColor[1] = ( minColor[1] + inset[1] <= 255 ) ? minColor[1] + inset[1] : 255;
		minColor[2] = ( minColor[2] + inset[2] <= 255 ) ? minColor[2] + inset[2] : 255;
		minColor[3] = ( minColor[3] + inset[3] <= 255 ) ? minColor[3] + inset[3] : 255;
		maxColor[0] = ( maxColor[0] >= inset[0] ) ? maxColor[0] - inset[0] : 0;
		maxColor[1] = ( maxColor[1] >= inset[1] ) ? maxColor[1] - inset[1] : 0;
		maxColor[2] = ( maxColor[2] >= inset[2] ) ? maxColor[2] - inset[2] : 0;
		maxColor[3] = ( maxColor[3] >= inset[3] ) ? maxColor[3] - inset[3] : 0;
	}
};
