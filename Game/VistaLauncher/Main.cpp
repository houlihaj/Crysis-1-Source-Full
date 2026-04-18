/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
	- 01:2006		: Vista Launcher version created by Jan Müller

*************************************************************************/
#include "StdAfx.h"
#include <CryLibrary.h>
#include <IGameStartup.h>
#include <IConsole.h>
#include <platform_impl.h>
#include "Setup.h"

#define GAMEDLL_FILENAME "CryGame.dll"

int RunGame(const char *commandLine)
{
	HANDLE mutex = CreateMutex(NULL, TRUE, "CrytekApplication");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		if(MessageBox(GetDesktopWindow(), "There is already a Crytek application running\nDo you want to start another one?", "Too many apps", MB_YESNO)!=IDYES)
			return 1;
	}

	//restart parameters
	static const size_t MAX_RESTART_LEVEL_NAME = 256;
	char restartLevelName[MAX_RESTART_LEVEL_NAME];
	strcpy(restartLevelName, "");
	bool restart = false;
	static const char logFileName[] = "Game.log";
	static const char logFileNameServer[] = "Server.log";
	bool dedicated = false;

	// Check if we're dedicated server (hack).
	if (strstr(commandLine, "-dedicated"))
		dedicated = true;

	// load the game dll
	HMODULE gameDll = CryLoadLibrary(GAMEDLL_FILENAME);

	if (!gameDll)
	{
		MessageBox(0, "Failed to load the Game DLL!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);
		// failed to load the dll

		return 0;
	}

	// get address of startup function
	IGameStartup::TEntryFunction CreateGameStartup = (IGameStartup::TEntryFunction)CryGetProcAddress(gameDll, "CreateGameStartup");

	if (!CreateGameStartup)
	{
		// dll is not a compatible game dll
		CryFreeLibrary(gameDll);

		MessageBox(0, "Specified Game DLL is not valid!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

		CloseHandle(mutex);
		return 0;
	}

	SSystemInitParams startupParams;

	memset(&startupParams, 0, sizeof(SSystemInitParams));

	startupParams.hInstance = GetModuleHandle(0);
	startupParams.sLogFileName = dedicated ? logFileNameServer : logFileName;
	strcpy(startupParams.szSystemCmdLine, commandLine);

	// create the startup interface
	IGameStartup *pGameStartup = CreateGameStartup();

	if (!pGameStartup)
	{
		// failed to create the startup interface
		CryFreeLibrary(gameDll);

		MessageBox(0, "Failed to create the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

		CloseHandle(mutex);
		return 0;
	}

	// run the game
	if (pGameStartup->Init(startupParams))
	{
		char * pRestartLevelName = NULL;
		if (strlen(restartLevelName))
			pRestartLevelName = restartLevelName;

		std::string commandLine(commandLine);
		int pos = commandLine.find("restart");
		if(pos != std::string::npos)
		{
			commandLine.erase(pos, 7);
			pGameStartup->Run(commandLine.c_str());	//restartLevel to be loaded
		}
		else
		{
			pos = commandLine.find("load");
			if(pos != string::npos)
			{
				commandLine.erase(pos, 4);
				pGameStartup->Run(commandLine.c_str());	//restartLevel to be loaded
			}
			else
				pGameStartup->Run(NULL);
		}

		restart = pGameStartup->GetRestartLevel(&pRestartLevelName);
		if (pRestartLevelName)
		{
			if (strlen(pRestartLevelName) < MAX_RESTART_LEVEL_NAME)
				strcpy(restartLevelName, pRestartLevelName);
		}

		if (restart)
		{
			STARTUPINFO si;
			ZeroMemory( &si, sizeof(si) );
			si.cb = sizeof(si);
			PROCESS_INFORMATION pi;
			std::string cmdLine("restarting:");
			cmdLine.append(" restart ");
			cmdLine.append(restartLevelName);
			CreateProcess("bin32/CrysisV.exe", (char*)(cmdLine.c_str()), NULL, NULL, FALSE, /*flags*/ 0, NULL, NULL, &si, &pi);
		}

		pGameStartup->Shutdown();
		pGameStartup = 0;

		CryFreeLibrary(gameDll);
	}
	else
	{
		MessageBox(0, "Failed to initialize the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

		// if initialization failed, we still need to call shutdown
		pGameStartup->Shutdown();
		pGameStartup = 0;

		CryFreeLibrary(gameDll);

		CloseHandle(mutex);
		return 0;
	}

	CloseHandle(mutex);
	return 0;
}
///////////////////////////////////////////////
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{

	//setup routine *************************************************************
	std::string commando(lpCmdLine);
	int spacePos = 0;
	int setup = 0;
	if(commando.size()>0)
	{
		if(!strcmp(commando.c_str(), "install") || !strcmp(commando.c_str(), "1"))
			setup = 1;
		else if(!strcmp(commando.c_str(), "uninstall") || !strcmp(commando.c_str(), "2"))
			setup = 2;
	}
	if(setup)
	{
		wchar_t buffer[MAX_PATH+1];
		GetCurrentDirectoryW(MAX_PATH, buffer);
		std::wstring workingDir(buffer);
		if(workingDir.rfind(L"\\") != workingDir.size()-1)
			workingDir.append(L"\\");

		std::wstring fullPath(workingDir);
		fullPath.append(L"CrysisV.exe");	//name of executable!

		if(setup == 1)
		{
			printf("Installing Crysis ..\n");
		  return int(InstallGame(LPWSTR(fullPath.c_str()), LPWSTR(fullPath.c_str()), LPWSTR(workingDir.c_str()), GIS_CURRENT_USER));
		}
		else if(setup == 2)
		{
			printf("Attempting to uninstall Crysis ...\n");
			HRESULT hr = E_FAIL;
			bool    bCleanupCOM = false;
			IGameExplorer* pFwGameExplorer = NULL;

			hr = CoInitialize( 0 );
			bCleanupCOM = SUCCEEDED(hr); 

			//check whether the access is allowed
			hr = CoCreateInstance( __uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, 
				__uuidof(IGameExplorer), (void**) &pFwGameExplorer );
			if( FAILED(hr) || pFwGameExplorer == NULL )
			{
				hr = E_FAIL;
				if( pFwGameExplorer ) pFwGameExplorer->Release();
				if( bCleanupCOM ) CoUninitialize();
			}
			else
			{
				BSTR path = SysAllocString( LPWSTR(fullPath.c_str()) );
				BOOL haveAccess = false;
				pFwGameExplorer->VerifyAccess(path, &haveAccess);
				if(!haveAccess)
				{
					MessageBox(0, "Either the working directory couldn't be accessed or your parental control settings are not sufficient to play this title!", "Access forbidden!", MB_OK);
					return -1;
				}
				SysFreeString( path );
			}

			printf("Removing game ..\n");
			if(RemoveGame(GIS_CURRENT_USER))
				printf("Successfully ..\n");
		}
		return 2;
	}
	//****************************************************************************

	// we need to pass the full command line, including the filename
	// lpCmdLine does not contain the filename.
	else
	{
#if 0
		std::string commandLine ("Command Line: '");
		commandLine+=commando;
		commandLine+="'";
		MessageBox(0,  commandLine.c_str(), "Jan1", MB_OK);
#endif

		//check for "load game"
		int pos = commando.find("load");
		if(pos != string::npos)
		{
			return RunGame(lpCmdLine);
		}
		//check for a restart
		else
		{
			pos = commando.find("restart");
			if(pos != std::string::npos)
			{
				Sleep(1000); //wait for old instance to be deleted
				return RunGame(lpCmdLine);	//pass the restart level if restarting
			}
		}

		return RunGame(GetCommandLineA());
	}
}
