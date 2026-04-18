#pragma once

#ifndef IMAGEEXTENSIONHELPER_H
#define IMAGEEXTENSIONHELPER_H

//#include <ITexture.h>
#include <Endian.h>
#include <Cry_Math.h>
#include <Cry_Vector3.h>
#include <Cry_Color.h>

#ifndef MAKEFOURCC
	#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
		((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |       \
		((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))
#endif /* defined(MAKEFOURCC) */


// This header defines constants and structures that are useful when parsing 
// DDS files.  DDS files were originally designed to use several structures
// and constants that are native to DirectDraw and are defined in ddraw.h,
// such as DDSURFACEDESC2 and DDSCAPS2.  This file defines similar 
// (compatible) constants and structures so that one can use DDS files 
// without needing to include ddraw.h.

// Crytek specific image extensions
//
// usually added to the end of DDS files

#define DDS_FOURCC 			0x00000004  // DDPF_FOURCC
#define DDS_RGB    			0x00000040  // DDPF_RGB
#define DDS_LUMINANCE		0x00020000  // DDPF_LUMINANCE
#define DDS_RGBA   			0x00000041  // DDPF_RGB | DDPF_ALPHAPIXELS
#define DDS_A      			0x00000010   // DDPF_ALPHAPIXELS
#define DDS_A8     			0x0000001C   // DDPF_ALPHAPIXELS
#define DDS_A_ONLY 			0x00000020   // DDPF_ALPHA

#define DDS_FOURCC_R32F 0x00000072  // FOURCC R32F 
#define DDS_FOURCC_G16R16F 0x00000070  // FOURCC G16R16F 
#define DDS_FOURCC_A16B16G16R16F 0x00000071  // FOURCC A16B16G16R16F 

#define DDSD_CAPS		0x00000001l	// default
#define DDSD_PIXELFORMAT	0x00001000l
#define DDSD_WIDTH		0x00000004l
#define DDSD_HEIGHT		0x00000002l
#define DDSD_LINEARSIZE		0x00080000l

#define DDS_HEADER_FLAGS_TEXTURE    0x00001007  // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT 
#define DDS_HEADER_FLAGS_MIPMAP     0x00020000  // DDSD_MIPMAPCOUNT
#define DDS_HEADER_FLAGS_VOLUME     0x00800000  // DDSD_DEPTH
#define DDS_HEADER_FLAGS_PITCH      0x00000008  // DDSD_PITCH
#define DDS_HEADER_FLAGS_LINEARSIZE 0x00080000  // DDSD_LINEARSIZE

#define DDS_SURFACE_FLAGS_TEXTURE 0x00001000 // DDSCAPS_TEXTURE
#define DDS_SURFACE_FLAGS_MIPMAP  0x00400008 // DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
#define DDS_SURFACE_FLAGS_CUBEMAP 0x00000008 // DDSCAPS_COMPLEX

#define DDS_CUBEMAP_POSITIVEX 0x00000600 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX
#define DDS_CUBEMAP_NEGATIVEX 0x00000a00 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX
#define DDS_CUBEMAP_POSITIVEY 0x00001200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY
#define DDS_CUBEMAP_NEGATIVEY 0x00002200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY
#define DDS_CUBEMAP_POSITIVEZ 0x00004200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ
#define DDS_CUBEMAP_NEGATIVEZ 0x00008200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ

#define DDS_CUBEMAP_ALLFACES ( DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX |\
	DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |\
	DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ )

#define DDS_FLAGS_VOLUME 0x00200000 // DDSCAPS2_VOLUME

#define DDS_RESF1_NORMALMAP 0x01000000
#define DDS_RESF1_DSDT      0x02000000


namespace CImageExtensionHelper
{
	struct DDS_PIXELFORMAT
	{
		DWORD dwSize;
		DWORD dwFlags;
		DWORD dwFourCC;
		DWORD dwRGBBitCount;
		DWORD dwRBitMask;
		DWORD dwGBitMask;
		DWORD dwBBitMask;
		DWORD dwABitMask;
		AUTO_STRUCT_INFO
	};

	struct DDS_HEADER
	{
		DWORD dwSize;
		DWORD dwHeaderFlags;
		DWORD dwHeight;
		DWORD dwWidth;
		DWORD dwPitchOrLinearSize;
		DWORD dwDepth; // only if DDS_HEADER_FLAGS_VOLUME is set in dwHeaderFlags
		DWORD dwMipMapCount;
		DWORD dwAlphaBitDepth;
		DWORD dwReserved1[10];
		DDS_PIXELFORMAT ddspf;
		DWORD dwSurfaceFlags;
		DWORD dwCubemapFlags;
		DWORD dwReserved2[2];
		DWORD dwTextureStage;
		AUTO_STRUCT_INFO

		inline const bool IsValid() const { return sizeof(*this) == dwSize; }
		inline const uint32 GetMipCount() const { return max(1u, (uint32)dwMipMapCount); }
	};

	// standart description of file header
	struct DDS_FILE_DESC
	{
		DWORD dwMagic;
		DDS_HEADER header;
		AUTO_STRUCT_INFO

		inline const bool IsValid() const { return dwMagic == MAKEFOURCC('D','D','S',' ') && header.IsValid(); }
	};

	// chunk identifier
	const static uint32 FOURCC_CExt							= MAKEFOURCC('C','E','x','t');		// Crytek extension start
	const static uint32 FOURCC_AvgC							= MAKEFOURCC('A','v','g','C');		// average color
	const static uint32 FOURCC_CEnd							= MAKEFOURCC('C','E','n','d');		// Crytek extension end
	const static uint32 FOURCC_AttC							= MAKEFOURCC('A','t','t','C');		// Chunk Attached Channel

	// flags to propagate from the RC to the engine through GetImageFlags()
	// 32bit bitmask, numbers should not change as engine relies on those numbers and rc_presets_pc.ini as well
	const static uint32 EIF_Cubemap							= 0x01;
	const static uint32 EIF_Volumetexture				= 0x02;
	const static uint32 EIF_Decal								= 0x04;			// this is usually set through the preset
	const static uint32 EIF_Greyscale						= 0x08;			// hint for the engine (e.g. greyscale light beams can be applied to shadow mask), can be for DXT1 because compression artfacts don't count as color
	const static uint32 EIF_SupressEngineReduce	= 0x10;			// info for the engine: don't reduce texture resolution on this texture
	const static uint32 EIF_DontStream					= 0x20;			// TODO: eliminate this flag!
	const static uint32 EIF_FileSingle					= 0x40;			// info for the engine: no need to search for other files (e.g. DDNDIF) (only used for build)
	const static uint32 EIF_AllowStreaming			= 0x80;			// info for the engine: it's able to stream this resource
	const static uint32 EIF_BigEndianness				= 0x100;		// info for the engine: it's an big_endian-converted texture!
	const static uint32 EIF_Compressed					= 0x200;		// info for the engine: it's an MCT or PTC compressed texture for XBox
	const static uint32 EIF_AttachedAlpha				= 0x400;		// info for the engine: it's a texture with attached alpha channel
	const static uint32 EIF_SRGBRead						= 0x800;		// info for the engine: if gamma corrected rendering is on, this texture requires SRGBRead (it's not stored in linear)
	const static uint32 EIF_XBox360Native				= 0x1000;		// info for the engine: native XBox360 texture format
	const static uint32 EIF_PS3Native						= 0x2000;		// info for the engine: native PS3 texture format

	// Arguments:
	//   pDDSHeader - must not be 0
	// Returns:
	//   Chunk flags (combined from EIF_Cubemap,EIF_Volumetexture,EIF_Decal,...)
	inline uint32 GetImageFlags( DDS_HEADER *pDDSHeader )
	{
		assert(pDDSHeader);

		// non standardized way to expose some features in the header (same information is in attached chunk but then
		// streaming would need to find this spot in the file)
		// if this is causing problems we need to change it 
		if(pDDSHeader->dwSize>=sizeof(DDS_HEADER))
		if(pDDSHeader->dwTextureStage == 'CRYF')
			return pDDSHeader->dwReserved1[0];
		return 0;
	}

	// Arguments:
	//   Chunk flags (combined from EIF_Cubemap,EIF_Volumetexture,EIF_Decal,...)
	// Returns:
	//   true, if this texture is ready for this platform
	inline const bool IsImageNative( const uint32 nFlags )
	{
#ifdef XENON
		return (nFlags & EIF_XBox360Native) != 0;
#elif defined(PS3)
		return (nFlags & EIF_PS3Native) != 0;
#else
		return (nFlags & (EIF_XBox360Native|EIF_PS3Native)) == 0;
#endif
	}

	// Arguments:
	//   pMem - usually first byte behind DDS file data, can be 0 (e.g. in case there no more bytes than DDS file data)
	// Returns:
	//   0 if not existing
	inline uint8 const* _findChunkStart( uint8 const* pMem, const uint32 dwChunkName, uint32* dwOutSize = NULL )
	{
		if(pMem)
			if(*(uint32 *)pMem == SwapEndianValue(FOURCC_CExt))
			{
				pMem+=4;	// jump over chunk name
				while(*(uint32 *)pMem != SwapEndianValue(FOURCC_CEnd))
				{
					if(*(uint32 *)pMem == SwapEndianValue(dwChunkName))
					{
						pMem+=4;	// jump over chunk name 
						const uint32 size = SwapEndianValue(*(uint32 *)(pMem));
						if(dwOutSize)
							*dwOutSize = size;
						pMem+=4;	// jump over chunk size
						return size > 0 ? pMem : NULL;
					}

					pMem += 8 + SwapEndianValue(*(uint32 *)(&pMem[4]));		// jump over chunk
				}
			}

			return 0;	// chunk does not exist
	}

	// Arguments:
	//   pMem - usually first byte behind DDS file data, can be 0 (e.g. in case there no more bytes than DDS file data)
	static ColorF GetAverageColor( uint8 const* pMem )
	{
		pMem = _findChunkStart(pMem,FOURCC_AvgC);

		if(pMem)
		{
			ColorF ret = ColorF(SwapEndianValue(*(uint32 *)pMem));
			//flip red and blue
			const float cRed = ret.r;
			ret.r = ret.b;
			ret.b = cRed;
			return ret;
		}

		return Col_White;	// chunk does not exist
	}

	// Arguments:
	//   pMem - usually first byte behind DDS file data, can be 0 (e.g. in case there no more bytes than DDS file data)
	// Returns:
	//   pointer to the DDS header
	inline DDS_HEADER *GetAttachedImage( uint8 const*pMem, uint32* dwOutSize = NULL)
	{
		pMem=_findChunkStart(pMem,FOURCC_AttC, dwOutSize);
		if(pMem)
			return (DDS_HEADER *)(pMem + 4);

		if(dwOutSize)
			*dwOutSize=0;
		return 0;	// chunk does not exist
	}
};

namespace DDSFormats
{
	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_DXT1 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','1'), 0, 0, 0, 0, 0 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_DXT2 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','2'), 0, 0, 0, 0, 0 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_DXT3 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','3'), 0, 0, 0, 0, 0 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_DXT4 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','4'), 0, 0, 0, 0, 0 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_DXT5 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','5'), 0, 0, 0, 0, 0 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_3DC = { sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A','T','I','2'), 0, 0, 0, 0, 0 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_R32F =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, DDS_FOURCC_R32F, 0, 32, 0, 0, 0 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_G16R16F =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, DDS_FOURCC_G16R16F, 0, 32, 0, 0, 0 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A16B16G16R16F =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, DDS_FOURCC_A16B16G16R16F, 0, 64, 0, 0, 0 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A8R8G8B8 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A1R5G5B5 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00007c00, 0x000003e0, 0x0000001f, 0x00008000 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A4R4G4B4 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00000f00, 0x000000f0, 0x0000000f, 0x0000f000 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_R8G8B8 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGB, 0, 24, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_X8R8G8B8 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGB, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_R5G6B5 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x0000f800, 0x000007e0, 0x0000001f, 0x00000000 };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A8 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_A, 0, 8, 0x00000000, 0x00000000, 0x00000000, 0x000000ff };

	const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_L8 =
	{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_LUMINANCE, 0, 8, 0x000000ff, 0x000000ff, 0x000000ff, 0x00000000 };

	inline ETEX_Format GetFormatByDesc( const CImageExtensionHelper::DDS_PIXELFORMAT& ddspf )
	{
		if (ddspf.dwFourCC			== DDSPF_DXT1.dwFourCC)
			return eTF_DXT1;
		else if (ddspf.dwFourCC == DDSPF_DXT3.dwFourCC)
			return eTF_DXT3;
		else if (ddspf.dwFourCC == DDSPF_DXT5.dwFourCC)
			return eTF_DXT5;
		else if (ddspf.dwFourCC == DDSPF_3DC.dwFourCC)
			return eTF_3DC;
		else if( ddspf.dwFourCC == DDSPF_R32F.dwFourCC)
			return eTF_R32F;
		else if( ddspf.dwFourCC == DDSPF_G16R16F.dwFourCC)
			return eTF_G16R16F;  
		else if( ddspf.dwFourCC == DDSPF_A16B16G16R16F.dwFourCC)
			return eTF_A16B16G16R16F;
		else if (ddspf.dwFlags == DDS_RGBA && ddspf.dwRGBBitCount == 32 && ddspf.dwABitMask == 0xff000000)
			return eTF_A8R8G8B8;
		else if (ddspf.dwFlags == DDS_RGBA && ddspf.dwRGBBitCount == 16)
			return eTF_A4R4G4B4;
		else if (ddspf.dwFlags == DDS_RGB  && ddspf.dwRGBBitCount == 24)
			return eTF_R8G8B8;
		else if (ddspf.dwFlags == DDS_RGB  && ddspf.dwRGBBitCount == 32)
			return eTF_X8R8G8B8;
		else if (ddspf.dwFlags == DDS_LUMINANCE  && ddspf.dwRGBBitCount == 8)
			return eTF_L8;
		else if ((ddspf.dwFlags == DDS_A || ddspf.dwFlags == DDS_A_ONLY) && ddspf.dwRGBBitCount == 8)
			return eTF_A8;
		else if ((ddspf.dwFlags == DDS_A || ddspf.dwFlags == DDS_A_ONLY) && ddspf.dwRGBBitCount == 8)
			return eTF_A8;
		else if (ddspf.dwFourCC == DDS_A8 || ddspf.dwRGBBitCount == 8)
			return eTF_A8;

		assert(0);
		return eTF_Unknown;
	}

	inline const CImageExtensionHelper::DDS_PIXELFORMAT& GetDescByFormat( const ETEX_Format eTF )
	{
		switch (eTF)
		{
		case eTF_DXT1:
			return DDSPF_DXT1;
		case eTF_DXT3:
			return DDSPF_DXT3;
		case eTF_DXT5:
			return DDSPF_DXT5;
		case eTF_3DC:
			return DDSPF_3DC;
		case eTF_R32F:
			return DDSPF_R32F;
		case eTF_G16R16F:
			return DDSPF_G16R16F;
		case eTF_A16B16G16R16F:
			return DDSPF_A16B16G16R16F;
		case eTF_R8G8B8:
		case eTF_L8V8U8:
			return DDSPF_R8G8B8;
		case eTF_A8R8G8B8:
			return DDSPF_A8R8G8B8;
		case eTF_X8R8G8B8:
			return DDSPF_X8R8G8B8;
		case eTF_R5G6B5:
			return DDSPF_R5G6B5;
		case eTF_A8:
			return DDSPF_A8;
		default:
			assert(0);
			return DDSPF_A8R8G8B8;
		}
	}
};

#endif // IMAGEEXTENSIONHELPER