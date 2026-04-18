/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 2:8:2004   11:04 : Created by Márcio Martins

*************************************************************************/
#ifndef __GAMESTARTUP_H__
#define __GAMESTARTUP_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IGameFramework.h>
#ifdef WIN32
#include <windows.h>
// get rid of (really) annoying MS defines
#undef min
#undef max
#endif

#define GAME_FRAMEWORK_FILENAME	"cryaction.dll"
#define GAME_WINDOW_CLASSNAME		"CryENGINE"

// implemented in GameDll.cpp
extern HMODULE GetFrameworkDLL();

class CGameStartup :
	public IGameStartup
{
public:
	CGameStartup();
	virtual ~CGameStartup();

	virtual IGameRef Init(SSystemInitParams &startupParams);
	virtual IGameRef Reset(const char *modName);
	virtual void Shutdown();
	virtual int Update(bool haveFocus, unsigned int updateFlags);
	virtual bool GetRestartLevel(char** levelName);
	virtual const char* GetPatch() const;
	virtual bool GetRestartMod(char* pModName, int nameLenMax);
	virtual int Run( const char * autoStartLevelName );

	static void RequestLoadMod(IConsoleCmdArgs* pCmdArgs);

	static void ForceCursorUpdate();

	static void AllowAccessibilityShortcutKeys(bool bAllowKeys);

private:

	static bool IsModAvailable(const string& modName);
	static string GetModPath(const string modName);

	static bool InitMod(const char* pName);
	static void ShutdownMod();

	static bool InitWindow(SSystemInitParams &startupParams);
	static void ShutdownWindow();

	static bool InitFramework(SSystemInitParams &startupParams);
	static void ShutdownFramework();

	void LoadLocalizationData();

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	static IGame						*m_pMod;
	static IGameRef				m_modRef;
	static IGameFramework	*m_pFramework;
	static bool						m_initWindow;

	static HMODULE					m_modDll;
	static HMODULE					m_frameworkDll;

	static string					m_rootDir;
	static string					m_binDir;
	static string					m_reqModName;

	static HMODULE				m_systemDll;
	static HWND						m_hWnd;
};


#endif //__GAMESTARTUP_H__
