////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   SourceControlInterface.h
//  Version:     v1.00
//  Created:     1/9/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SourceControlInterface_h__
#define __SourceControlInterface_h__
#pragma once

#include "ISourceControl.h"

class CSourceControlInterface : private ISourceControl
{
public:
	CSourceControlInterface();
	~CSourceControlInterface();

	ISourceControl* GetSCMInterface();


	//////////////////////////////////////////////////////////////////////////
	// ISourceControl implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual uint32 GetFileAttributes( const char *filename );
	virtual bool Add( const char *filename, const char * desc, int nFlags){return false;}
	virtual bool CheckIn( const char *filename, const char * desc, int nFlags){return false;}
	virtual bool CheckOut( const char *filename, int nFlags){return false;}
	virtual bool UndoCheckOut( const char *filename, int nFlags){return false;}
	virtual bool Rename( const char *filename, const char *newfilename, const char * desc, int nFlags){return false;}
	virtual bool Delete( const char *filename, const char * desc, int nFlags){return false;}
	virtual bool GetLatestVersion( const char *filename, int nFlags){return false;}

	//////////////////////////////////////////////////////////////////////////
	
private:
	void Init();
	ISourceControl *m_pSCM;
};


#endif // __SourceControlInterface_h__