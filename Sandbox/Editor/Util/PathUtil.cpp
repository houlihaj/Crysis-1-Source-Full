////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   PathUtil.cpp
//  Version:     v1.00
//  Created:     4/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PathUtil.h"

namespace Path
{
	CString GamePathToFullPath( const CString &path )
	{
		char szAdjustedFile[ICryPak::g_nMaxPath];
		const char *szTemp = gEnv->pCryPak->AdjustFileName( path,szAdjustedFile,0 );

		return szAdjustedFile;
	}
}


