////////////////////////////////////////////////////////////////////////////
//
//  CRYTEK Source File.
//  Copyright (C), CRYTEK Studios, 2001-2007.
// -------------------------------------------------------------------------
//  Created:     31/10/2007 by Benjamin.
//  Compilers:   Visual Studio.NET
//
////////////////////////////////////////////////////////////////////////////
#pragma once


#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif


#include "resource.h"


class CLauncherWindow;


//////////////////////////////////////////////////////////////////////////
// Main Application; Starts up Launcher Window
class CLauncher : public CWinApp
{
public:
	CLauncherWindow*	GetWindow();

protected:
	virtual BOOL		InitInstance();

	DECLARE_MESSAGE_MAP()

private:
	CLauncherWindow*	m_pLauncherWindow;
};


extern CLauncher gTheApp;
