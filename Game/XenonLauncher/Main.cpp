// Executable_Xenon.cpp : Defines the entry point for the application.
//

#include "StdAfx.h"
#include <IGameStartup.h>

#define _LAUNCHER
#include <platform_impl.h>

MEMORYSTATUS MemoryStatus;
int GetMemoryStatus()
{
	GlobalMemoryStatus(&MemoryStatus);
	return (MemoryStatus.dwTotalPhys - MemoryStatus.dwAvailPhys) >> 20;
}

#ifndef XENON
#	define DLL_INITFUNC_GAME "CreateGameStartup"
#else
#	define DLL_INITFUNC_GAME (LPCSTR)1
#endif

#ifdef _LIB
extern "C" IGameStartup* CreateGameStartup();
#endif

#define GAMEDLL_FILENAME "d:\\CryGame.dll"

int RunGame(const char *commandLine)
{
#ifndef _LIB
	// load the game dll
  HMODULE gameDll = LoadLibrary(GAMEDLL_FILENAME);
  if (!gameDll)
  {
    DWORD dwErr = GetLastError();
    int nnn = 0;
  }

	// get address of startup function
	IGameStartup::TEntryFunction CreateGameStartup = (IGameStartup::TEntryFunction)GetProcAddress(gameDll,DLL_INITFUNC_GAME );
	if (!CreateGameStartup)
	{
		// dll is not a compatible game dll
		FreeLibrary(gameDll);
		return 0;
	}
#endif

	SSystemInitParams startupParams;

	memset(&startupParams, 0, sizeof(SSystemInitParams));

	startupParams.hInstance = 0;
	startupParams.sLogFileName = "Game.log";
	strcpy(startupParams.szSystemCmdLine, commandLine);

	// create the startup interface
	IGameStartup* pGameStartup = CreateGameStartup();

	if (!pGameStartup)
	{
		//MessageBox(0, "Failed to create the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

		return 0;
	}

	// run the game
	if (pGameStartup->Init(startupParams))
	{
		int exitCode = pGameStartup->Run(NULL);
		pGameStartup->Shutdown();
		pGameStartup = 0;

		return exitCode;
	}

	// if initialization failed, we still need to call shutdown
	pGameStartup->Shutdown();
	pGameStartup = 0;

	//MessageBox(0, "Failed to initialize the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

	return 0;
}

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------

void __cdecl main()
{
	GetMemoryStatus();
	RunGame("");
}
