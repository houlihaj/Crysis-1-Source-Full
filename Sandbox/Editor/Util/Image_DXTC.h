////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   image_dxtc.h
//  Version:     v1.00
//  Created:     5/9/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __image_dxtc_h__
#define __image_dxtc_h__
#pragma once

class CImage_DXTC  
{
public:
	// Arguments:
	//   pQualityLoss - 0 if info is not needed, pointer to the result otherwise - not need to preinitialize
	bool Load( const char *filename,CImage &outImage, bool *pQualityLoss=0 );		// true if success

	CImage_DXTC();
	~CImage_DXTC();
};

#endif // __image_dxtc_h__
