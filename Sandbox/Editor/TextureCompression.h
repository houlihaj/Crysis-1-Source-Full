// TextureCompression.h: interface for the CTextureCompression class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TEXTURECOMPRESSION_H__B2702EC6_F5D8_4BB3_B2EE_A2F66C128380__INCLUDED_)
#define AFX_TEXTURECOMPRESSION_H__B2702EC6_F5D8_4BB3_B2EE_A2F66C128380__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Util/CryMemFile.h"				// CCryMemFile

class CTextureCompression  
{
public:
	// constructor
	// Arguments:
	//   bHighQuality - true:slower but better quality, false=use hardware instead
	CTextureCompression( const bool bHighQuality );
	// destructor
	virtual ~CTextureCompression();

	// Arguments:
	//   vLumWeight - 0,0,0 if default should be used
	void WriteDDSToFile( CFile &toFile,unsigned char *dat, int w,int h,int Size,
		ETEX_Format eSrcF, ETEX_Format eDstF, int NumMips, const bool bHeader, const Vec3 vLumWeight, const bool bDither );

private: // ------------------------------------------------------------------

	static void SaveCompressedMipmapLevel( const void *data, size_t size, void * userData );

	bool										m_bHighQuality;		// true:slower but better quality, false=use hardware instead
	static CFile *					m_pFile;
};

#endif // !defined(AFX_TEXTURECOMPRESSION_H__B2702EC6_F5D8_4BB3_B2EE_A2F66C128380__INCLUDED_)
