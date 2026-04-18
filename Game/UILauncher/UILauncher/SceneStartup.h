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


#ifdef WIN32
#undef min
#undef max
#endif


#include <IGameFramework.h>


#define GAME_FRAMEWORK_FILENAME			"CryAction.dll"
#define GAME_DLL_URL								"Mods/Base/Bin64/Base.dll"


extern HMODULE GetFrameworkDLL();


class CSceneStartup : public IGameStartup
{
public:
	CSceneStartup();
	virtual ~CSceneStartup();

	virtual IGameRef			Init(SSystemInitParams &startupParams);
	virtual IGameRef			Reset(const char *modName);
	virtual void				Shutdown();
	virtual int					Update(bool haveFocus, unsigned int updateFlags);
	virtual bool				GetRestartLevel(char** levelName);
	virtual const char*			GetPatch() const;
	virtual bool				GetRestartMod(char* pModName, int nameLenMax);
	virtual int					Run( const char * autoStartLevelName );
	IGameFramework* GetGameFramework();

private:
	static void					InitRootDir();
	static bool					InitFramework(SSystemInitParams &startupParams);
	static void					ShutdownFramework();

private:
	static IGame*				m_pMod;
	static IGameRef				m_modRef;
	static IGameFramework*		m_pFramework;
	static bool					m_initWindow;

	static HMODULE				m_modDll;
	static HMODULE				m_frameworkDll;

	static string				m_rootDir;
	static string				m_binDir;
	static string				m_reqModName;

	static HMODULE				m_systemDll;
};
