/*************************************************************************
	Crytek Source File.
	Copyright (C), Crytek Studios, 2001-2004.
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: Game DLL entry point.

	-------------------------------------------------------------------------
	History:
	- 2:8:2004   10:38 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "GameStartup.h"

#include <CryLibrary.h>
#include "..\CrySystem\CryWaterMark.h"


extern "C"
{
	/*
	GAME_API IGame *CreateGame(IGameFramework* pGameFramework)
	{
		ModuleInitISystem(pGameFramework->GetISystem());

		static char pGameBuffer[sizeof(IGame)];
		return new ((void*)pGameBuffer) CGame();

		return new CGame();
	}

	GAME_API IGameStartup *CreateGameStartup()
	{
		// at this point... we have no dynamic memory allocation, and we cannot
		// rely on atexit() doing the right thing; the only recourse is to
		// have a static buffer that we use for this object
		static char gameStartup_buffer[sizeof(CGameStartup)];
		return new ((void*)gameStartup_buffer) CGameStartup();
	}

	GAME_API IEditorGame *CreateEditorGame()
	{
		return new CEditorGame();
	}
	*/
}

/*
 * this section makes sure that the framework dll is loaded and cleaned up
 * at the appropriate time
 */

#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)

static HMODULE s_frameworkDLL;

static void CleanupFrameworkDLL()
{
	assert( s_frameworkDLL );
	FreeLibrary( s_frameworkDLL );
	s_frameworkDLL = 0;
}

HMODULE GetFrameworkDLL()
{
	if (!s_frameworkDLL)
	{
		s_frameworkDLL = CryLoadLibrary(GAME_FRAMEWORK_FILENAME);
		atexit( CleanupFrameworkDLL );
	}
	return s_frameworkDLL;
}

#endif //WIN32

///////////////////////////////////////////////
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// we need pass the full command line, including the filename
	// lpCmdLine does not contain the filename.

	HANDLE mutex = CreateMutex(NULL, TRUE, "CrytekApplication");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		if(MessageBox(GetDesktopWindow(), "There is already a Crytek application running\nDo you want to start another one?", "Too many apps", MB_YESNO)!=IDYES)
			return 1;
	}

	// create the startup interface
	IGameStartup *pGameStartup = new CGameStartup();

	if (!pGameStartup)
	{
		// failed to create the startup interface
		//CryFreeLibrary(gameDll);

		MessageBox(0, "Failed to create the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

		CloseHandle(mutex);
		return 0;
	}

	static const char logFileName[] = "Mod.log";

	SSystemInitParams startupParams;

	memset(&startupParams, 0, sizeof(SSystemInitParams));

	startupParams.sLogFileName = logFileName;

	startupParams.hInstance = GetModuleHandle(0);
	startupParams.sLogFileName = logFileName;
	strcpy(startupParams.szSystemCmdLine, GetCommandLineA());

	// run the game
	if (pGameStartup->Init(startupParams))
	{
		pGameStartup->Run(NULL);

		pGameStartup->Shutdown();
		pGameStartup = 0;
	}

	CloseHandle(mutex);
	return 0;
}

