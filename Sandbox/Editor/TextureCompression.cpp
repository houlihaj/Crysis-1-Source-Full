// TextureCompression.cpp: implementation of the CTextureCompression class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TextureCompression.h"

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
                ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#include "Util\dds.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CFile* CTextureCompression::m_pFile = NULL;

CTextureCompression::CTextureCompression( const bool bHighQuality ) : m_bHighQuality(bHighQuality)
{
}

CTextureCompression::~CTextureCompression()
{
}


//////////////////////////////////////////////////////////////////////////
void CTextureCompression::SaveCompressedMipmapLevel( const void *data, size_t size, void *userData )
{
	assert(m_pFile);

	// without header
	m_pFile->Write(data,size);
}



//////////////////////////////////////////////////////////////////////////
void CTextureCompression::WriteDDSToFile( CFile &file,unsigned char *dat, int w,int h,int Size,
	ETEX_Format eSrcF, ETEX_Format eDstF, int NumMips, const bool bHeader, const Vec3 vLumWeight, const bool bDither )
{
	if(bHeader)
	{
		DWORD dwMagic;
		DDS_HEADER ddsh;
		ZeroStruct(ddsh);

		dwMagic = MAKEFOURCC('D','D','S',' ');
		file.Write( &dwMagic,sizeof(DWORD) );

		ddsh.dwSize = sizeof(DDS_HEADER);
		ddsh.dwWidth = w;
		ddsh.dwHeight = h;
		ddsh.dwMipMapCount = NumMips;
		if (!NumMips)
			ddsh.dwMipMapCount = 1;
		ddsh.dwHeaderFlags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
		ddsh.dwSurfaceFlags = DDS_SURFACE_FLAGS_TEXTURE | DDS_SURFACE_FLAGS_MIPMAP;

		switch (eDstF)  
		{
		case eTF_DXT1:
			ddsh.ddspf = DDSPF_DXT1;
			break;
		case eTF_DXT3:
			ddsh.ddspf = DDSPF_DXT3;
			break;
		case eTF_DXT5:
			ddsh.ddspf = DDSPF_DXT5;
			break;
		case eTF_R5G6B5:
			ddsh.ddspf = DDSPF_R5G6B5;
			break;
		case eTF_R8G8B8:
		case eTF_L8V8U8:
			ddsh.ddspf = DDSPF_R8G8B8;
			break;
		case eTF_A8R8G8B8:
			ddsh.ddspf = DDSPF_A8R8G8B8;
			break;
		case eTF_L8:
			ddsh.ddspf = DDSPF_L8;
			break;
		default:
			assert(0);
			return;
		}
		file.Write( &ddsh, sizeof(ddsh) );
	}

	byte *data = NULL;

	if (eDstF == eTF_R8G8B8 || eDstF == eTF_L8V8U8)
	{
		data = new byte[Size];
		int n = Size / 3;
		for (int i=0; i<n; i++)
		{
			data[i*3+0] = dat[i*3+2];
			data[i*3+1] = dat[i*3+1];
			data[i*3+2] = dat[i*3+0];
		}
		file.Write( data,Size );
	}
	else
	if (eDstF == eTF_A8R8G8B8)
	{
		data = new byte[Size];
		int n = Size / 4;
		for (int i=0; i<n; i++)
		{
			data[i*4+0] = dat[i*4+2];
			data[i*4+1] = dat[i*4+1];
			data[i*4+2] = dat[i*4+0];
			data[i*4+3] = dat[i*4+3];
		}
		file.Write( data,Size );
	}
	else
	if (eDstF == eTF_R5G6B5)		// input X8R8G8B8, output R5G6B5
	{
		data = new byte[Size/2];
		int n = Size / 4;
		for (int i=0; i<n; i++)
		{
			ColorB rgba(dat[i*4+0],dat[i*4+1],dat[i*4+2],dat[i*4+3]);

			*(uint16 *)(&data[i*2]) = rgba.pack_rgb565();
		}
		file.Write( data,Size/2 );
	}  
	else
	if (eDstF == eTF_A4R4G4B4)		// input X8R8G8B8, output A4R4G4B4
	{
		data = new byte[Size/2];
		int n = Size / 4;
		
		if(bDither)
			for (int i=0; i<n; i++)
			{
				int a = clamp_tpl((int)dat[i*4+0]+(rand()%16)-8,0,255);
				int b = clamp_tpl((int)dat[i*4+1]+(rand()%16)-8,0,255);
				int c = clamp_tpl((int)dat[i*4+2]+(rand()%16)-8,0,255);
				int d = clamp_tpl((int)dat[i*4+3]+(rand()%16)-8,0,255);

				*(uint16 *)(&data[i*2]) = ColorB(a,b,c,d).pack_argb4444();
			}
		else
			for (int i=0; i<n; i++)
			{
				ColorB rgba(dat[i*4+0],dat[i*4+1],dat[i*4+2],dat[i*4+3]);

				*(uint16 *)(&data[i*2]) = rgba.pack_argb4444();
			}
		file.Write( data,Size/2 );
	}
	else if(eSrcF==eTF_A8R8G8B8 && eDstF==eTF_L8)
	{
		const int n = Size / 4;
		data = new byte[n];

		for (int i=0; i<n; i++)
			data[i] = dat[i*4+1];			// copy green
		file.Write( data,n);
	}
	else if(eSrcF==eTF_A8R8G8B8 && (eDstF==eTF_DXT1 || eDstF==eTF_DXT5))
	{
		bool bUseHW = !m_bHighQuality;

		m_pFile = &file;
		GetIEditor()->GetRenderer()->DXTCompress(dat,w,h,eTF_DXT5,bUseHW,false,4,vLumWeight,SaveCompressedMipmapLevel );
		m_pFile=0;
	}
	else
	{
		assert(eSrcF==eDstF);

		file.Write( dat,Size );
	}

	SAFE_DELETE_ARRAY(data);
}
