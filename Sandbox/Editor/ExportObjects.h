////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ExportObjects.h
//  Version:     v1.00
//  Created:     06/11/2005 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __exportobjects_h__
#define __exportobjects_h__
#pragma once


/*! Class responsible for exporting particles to game format.
*/
/** Export brushes from specified Indoor to .bld file.
*/
class CExportObjects
{
public:
	bool Export( const char *pszFileName );
};

#endif // __exportobjects_h__
