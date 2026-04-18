////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   GameTokenLibrary.h
//  Version:     v1.00
//  Created:     10/11/2003 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GameTokenLibrary_h__
#define __GameTokenLibrary_h__
#pragma once

#include "BaseLibrary.h"

//////////////////////////////////////////////////////////////////////////
class CRYEDIT_API CGameTokenLibrary : public CBaseLibrary
{
public:
	CGameTokenLibrary( CBaseLibraryManager *pManager ) : CBaseLibrary(pManager) {};
	virtual bool Save();
	virtual bool Load( const CString &filename );
	virtual void Serialize( XmlNodeRef &node,bool bLoading );
private:
};

#endif // __GameTokenLibrary_h__
