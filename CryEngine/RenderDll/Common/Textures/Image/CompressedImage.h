/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2008.
-------------------------------------------------------------------------
$Id: CompressedImage.h,v 1.0 2008/02/20 15:14:13 AntonKaplanyan Exp wwwrun $
$DateTime$
Description:  Routine for loading of compressed textures
-------------------------------------------------------------------------
History:
- 20:2:2008   10:31 : Created by Anton Kaplanyan
*************************************************************************/

#ifndef _COMPRESSED_DDS_IMAGE_H_
#define _COMPRESSED_DDS_IMAGE_H_

#	include <vector>
#	include <smartptr.h>
#	include "DDSImage.h"

namespace RTDXTCompression
{
	typedef unsigned short word;
	typedef unsigned int dword;
	#define C565_5_MASK 0xF8 // 0xFF minus last three bits
	#define C565_6_MASK 0xFC // 0xFF minus last two bits
	#define INSET_SHIFT 4 // inset the bounding box with ( range >> shift )

	void CompressImageDXT( const byte *inBuf, byte *outBuf, int width, int height, int &outputBytes, const bool isDXT5 );

	inline void ExtractBlock( const byte *inPtr, int width, byte *colorBlock )
	{
		for ( int j = 0; j < 4; j++ )
		{
			memcpy( &colorBlock[j*4*4], inPtr, 4*4 );
			inPtr += width * 4;
		}
	}

	inline word ColorTo565( const byte *color )
	{
#ifdef XENON
		return ( ( color[ 2 ] >> 3 ) << 11 ) | ( ( color[ 1 ] >> 2 ) << 5 ) | ( color[ 0 ] >> 3 );
#else
		return ( ( color[ 0 ] >> 3 ) << 11 ) | ( ( color[ 1 ] >> 2 ) << 5 ) | ( color[ 2 ] >> 3 );
#endif
	}

	inline void EmitByte( byte b, byte** pData )
	{
		(*pData)[0] = b;
		(*pData) += 1;
	}
	inline void EmitWord( word s, byte** pData )
	{
#ifdef XENON
		(*pData)[1] = byte(( s >> 0 ) & 255);
		(*pData)[0] = byte(( s >> 8 ) & 255);
#else
		(*pData)[0] = byte(( s >> 0 ) & 255);
		(*pData)[1] = byte(( s >> 8 ) & 255);
#endif
		(*pData) += 2;
	}
	inline void EmitDoubleWord( dword i, byte** pData )
	{
#ifdef XENON
		(*pData)[1] = byte(( i >> 0 ) & 255);
		(*pData)[0] = byte(( i >> 8 ) & 255);
		(*pData)[3] = byte(( i >> 16 ) & 255);
		(*pData)[2] = byte(( i >> 24 ) & 255);
#else
		(*pData)[0] = byte(( i >> 0 ) & 255);
		(*pData)[1] = byte(( i >> 8 ) & 255);
		(*pData)[2] = byte(( i >> 16 ) & 255);
		(*pData)[3] = byte(( i >> 24 ) & 255);
#endif
		(*pData) += 4;
	}



	void GetMinMaxColors( const byte *colorBlock, byte *minColor, byte *maxColor );
	void GetMinMaxColorsAndAlpha( const byte *colorBlock, byte *minColor, byte *maxColor );

	void EmitAlphaIndices( const byte *colorBlock, const byte minAlpha, const byte maxAlpha, byte** pData );
	void EmitColorIndices( const byte *colorBlock, const byte *minColor, const byte *maxColor, byte** pData );
};

/**
* An ImageFile subclass for read compress DDS textures
	and separated MIPs compressed with platform-specific lossy and lossless
*/
class CCompressedImageDDSFile : public _reference_target_t
{
  friend class CImageFile;
public:
#ifdef XENON
	// desc of PTC-compressed chunk of data
	struct PTCDesc
	{
		enum EPTCDescVersion
		{
			eptcdvVersion2		= MAKEFOURCC('P','T','C','3'),
			eptcdvDefault			= eptcdvVersion2,
		};
		enum EPTCDescCompressioParameters
		{
			numPTCMIPs				= 5,	
		};

		uint32			version;
		ETEX_Format	format;
		uint32			compressedSize;
		uint32			decompressedSize;
		uint32			numMips;
	};
	// desc of MCT-compressed chunk of data
	struct MCTDesc
	{
		enum EMCTDescVersion
		{
			emctdvVersion1		= MAKEFOURCC('M','C','T','2'),
			emctdvDefault			= emctdvVersion1,
		};
		uint32			version;
		ETEX_Format	format;
		uint32			numMips;
		uint32			compressedSize;
		uint32			decompressedSize;
	};
#endif	// XENON

	// desc of JPEG-compressed chunk of data
	struct JPEGDesc
	{
		enum EJPEGDescVersion
		{
			ejpegdvVersion0		= MAKEFOURCC('J','P','G','0'),
			ejpegdvDefault		= ejpegdvVersion0,
		};
		uint32			version;
		ETEX_Format	srcFormat;
		ETEX_Format	dataFormat;
		uint32			width;
		uint32			height;
		uint32			compressedSize;
		uint32			decompressedSize;
	};
private:
#ifdef XENON
	// Xenon specific compression

	// decompress DXT texture or separated MIPs with MCT codec
	size_t DecompressMCT(const byte* pInData, const uint32 nDecompressedSize);
	// decompress texture or separated MIPs with PTC codec
	size_t DecompressPTC(const byte* pInData, const uint32 nDecompressedSize);
	// load padded data
	void UploadFromTexture(struct IDirect3DTexture9* pTexture);
#endif	// XENON

	// platform-dependent JPEG decompression method
	bool DecompressPureJPEGChunk(const JPEGDesc* pDesc, const byte* pInData, byte* pOutData);

	// decompress texture or separated MIPs with JPEG codec
	bool DecompressJPEG(const byte* pInData, size_t* comprSize, const uint32 nDecompressedSize, size_t* decomprSize);

	CCompressedImageDDSFile( const byte* pCompressed, const uint32 nCompressedSize, const uint32 nDecompressedSize );
public:
	// in-place decompression, determines which codec to use
	static bool Decompress(const byte* pCompressed, const uint32 nCompressedSize, byte* pDecompressed, const uint32 nBufferSize);
	// decompression with own storage, determines which codec to use
	static CCompressedImageDDSFile* LoadFromMemory(const byte* pCompressed, const uint32 nCompressedSize, const uint32 nDecompressedSize);

	// returns size of decompressed image
	size_t GetSize() const;
	// pointer to beginning of decompressed image
	byte* GetData();

	// rt DXT compression
	void CompressToDXT(const std::vector<byte>& vecInData, std::vector<byte>& outData, const int width, const int height, const bool isDXT5);
public:
protected:
	// decompressed image
	std::vector<byte>	m_uncompressedContent;
};

typedef _smart_ptr<CCompressedImageDDSFile> CCompressedImageDDSFilePtr;

#endif	// _COMPRESSED_DDS_IMAGE_H_


