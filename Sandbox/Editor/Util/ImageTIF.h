////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   imagetif.h
//  Version:     v1.00
//  Created:     11/05/2004 by Martin.
//  Compilers:   Visual C++ .NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __imagetif_h__
#define __imagetif_h__

#if _MSC_VER > 1000
#pragma once
#endif

class CImageTIF
{
public:
	bool Load( const CString &fileName, CImage &outImage );
	bool Save( const CString &fileName, const CImage * inImage );
};


#endif // __imagetif_h__
