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


#include "ViewPort.h"
#include "SceneStartup.h"
#include "SceneController.h"
#include "Launcher.h"


//////////////////////////////////////////////////////////////////////////
// Main Window; Creates CryEngine instance through CSceneStartup, handles classes for view and control
// Loads a DLL whitch keeps IGame interface (currently Base.dll mod hardcoded)
// Runs .cry Levels choosable by a dialog (currently MyHelloWorld.cry runs hardcoded at startup).
class CLauncherWindow : public CDialog, public ISystemUserCallback
{
public:
	CLauncherWindow(CWnd* pParent = NULL);
	enum { IDD = IDD_LAUNCHER_DLG };

	// *** from CDialog ***
protected:
	//////////////////////////////////////////////////////////////////////////
	// Setup CryEngine, view, controller and scene
	virtual BOOL		OnInitDialog();

	//////////////////////////////////////////////////////////////////////////
	// Checks when to toggle GameMode (active/passive), forwards keys to console, calls HandleCommand
	virtual BOOL		PreTranslateMessage(MSG* pMsg);

	afx_msg void		OnPaint();
	afx_msg void		OnSize(UINT nType, int cx, int cy);
	afx_msg void		OnDestroy();
	DECLARE_MESSAGE_MAP()

	// *** from ISystemUserCallback ***
private:
	virtual bool		OnError( const char *szErrorString );
	virtual void		OnSaveDocument() {}
	virtual void		OnProcessSwitch() {}
	virtual void		OnInitProgress( const char *sProgressMsg );
	virtual void		GetMemoryUsage( ICrySizer* pSizer ) {}

private:
	//////////////////////////////////////////////////////////////////////////
	// Setup SSystemInitParams and SceneStartup
	bool				InitSystem(HWND hWnd);
	void				LoadLevel(const std::string &name);

	//////////////////////////////////////////////////////////////////////////
	// Handles menu entries and dialog commands
	bool				HandleCommand(WPARAM param);

private:
	HICON				m_hIcon;
	CSceneStartup*		m_pSceneStartup;
	CViewPort*			m_pViewPort;
	CSceneController	m_sceneController;
};
