/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 2:8:2004   15:20 : Created by Márcio Martins

*************************************************************************/

#include "StdAfx.h"
#include "Menus/OptionsManager.h"
#include "GameStartup.h"
#include "Game.h"
#include "Menus/FlashMenuObject.h"

#include <StringUtils.h>
#include <CryFixedString.h>
#include <CryLibrary.h>
#include <platform_impl.h>
#include <INetworkService.h>

#include "IHardwareMouse.h"
#include "ICryPak.h"

#if defined(PS3) && !defined(_LIB) 
#if defined PS3_PRX_CryAction
extern "C" IGameFramework *CreateGameFramework();
#endif
namespace
{
	static IGameFramework *PS3CreateGameFramework()
	{
#if defined PS3_PRX_CryAction
		return CreateGameFramework();
#else
		return gPS3Env->pInitFnTable->pInitAction();
#endif
	}
}
#define CreateGameFramework PS3CreateGameFramework
#elif defined(_LIB) || defined(LINUX) || defined(PS3)
extern "C" IGameFramework *CreateGameFramework();
#endif

#ifndef XENON
#define DLL_INITFUNC_CREATEGAME "CreateGameFramework"
#else
#define DLL_INITFUNC_CREATEGAME (LPCSTR)1
#endif

#ifdef WIN32
bool g_StickyKeysStatusSaved = false;
STICKYKEYS g_StartupStickyKeys = {sizeof(STICKYKEYS), 0};
TOGGLEKEYS g_StartupToggleKeys = {sizeof(TOGGLEKEYS), 0};
FILTERKEYS g_StartupFilterKeys = {sizeof(FILTERKEYS), 0};

const static bool g_debugWindowsMessages = false;

void RestoreStickyKeys()
{
	CGameStartup::AllowAccessibilityShortcutKeys(true);
}
#endif



#define EYEADAPTIONBASEDEFAULT		0.25f					// only needed for Crysis

bool g_inSizeMove = false;

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_Game : public ISystemEventListener
{
public:
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
	{
		switch (event)
		{
		case ESYSTEM_EVENT_RANDOM_SEED:
			g_random_generator.seed(wparam);
			break;
		case ESYSTEM_EVENT_CHANGE_FOCUS:
			{
				CGameStartup::AllowAccessibilityShortcutKeys(wparam==0);
				if (!wparam && !gEnv->pSystem->IsDevMode() && !gEnv->pSystem->IsEditor() && g_pGame && g_pGame->GetIGameFramework() && g_pGame->GetIGameFramework()->IsGameStarted() && !gEnv->bMultiplayer)
				{
					SAFE_MENU_FUNC(ShowInGameMenu(true));
				}
			}
			break;
		case ESYSTEM_EVENT_LEVEL_LOAD_START:
			{
				// hack for needed for Crysis - to reset cvar set in level.cfg
				ICVar *pCVar = gEnv->pConsole->GetCVar("r_EyeAdaptationBase");		assert(pCVar);

				float fOldVal = pCVar->GetFVal();

				if(fOldVal!=EYEADAPTIONBASEDEFAULT)
				{
					CryLog("r_EyeAdaptationBase was reset to default");
					pCVar->Set(EYEADAPTIONBASEDEFAULT);		// set to default value
				}
			}
			break;

		case ESYSTEM_EVENT_LEVEL_RELOAD:
			STLALLOCATOR_CLEANUP;
			break;
		}
	}
};
static CSystemEventListner_Game g_system_event_listener_game;

IGame* CGameStartup::m_pMod = NULL;
IGameRef CGameStartup::m_modRef;
IGameFramework* CGameStartup::m_pFramework = NULL;

HMODULE CGameStartup::m_modDll = 0;
HMODULE CGameStartup::m_frameworkDll = 0;
HMODULE CGameStartup::m_systemDll = 0;

string CGameStartup::m_rootDir;
string CGameStartup::m_binDir;
string CGameStartup::m_reqModName;

bool CGameStartup::m_initWindow = false;

CGameStartup::CGameStartup()
{
	m_modRef = &m_pMod;
}

CGameStartup::~CGameStartup()
{
	if (m_pMod)
	{
		m_pMod->Shutdown();
		m_pMod = 0;
	}

	if (m_modDll)
	{
		CryFreeLibrary(m_modDll);
		m_modDll = 0;
	}

	ShutdownFramework();
}

IGameRef CGameStartup::Init(SSystemInitParams &startupParams)
{
	if (!InitFramework(startupParams))
	{
		return 0;
	}

	LOADING_TIME_PROFILE_SECTION(m_pFramework->GetISystem());

	ISystem* pSystem = m_pFramework->GetISystem();
	IConsole* pConsole = pSystem->GetIConsole();
	startupParams.pSystem = pSystem;

	pConsole->AddCommand("g_loadMod", RequestLoadMod);

	// load the appropriate game/mod
	const ICmdLineArg *pModArg = pSystem->GetICmdLine()->FindArg(eCLAT_Pre,"MOD");

	IGameRef pOut;
	if (pModArg && (*pModArg->GetValue() != 0) && (pSystem->IsMODValid(pModArg->GetValue())))
	{
		const char* pModName = pModArg->GetValue();
		assert(pModName);

		pOut = Reset(pModName);
	}
	else
	{
		pOut = Reset(GAME_NAME);
	}

	if (!m_pFramework->CompleteInit())
	{
		pOut->Shutdown();
		return 0;
	}

	// should be after init game (should be executed even if there is no game)
	pSystem->ExecuteCommandLine();
	pSystem->GetISystemEventDispatcher()->RegisterListener( &g_system_event_listener_game );

	return pOut;
}

IGameRef CGameStartup::Reset(const char *pModName)
{
	if (m_pMod)
	{
		m_pMod->Shutdown();

		if (m_modDll)
		{
			CryFreeLibrary(m_modDll);
			m_modDll = 0;
		}
	}

	m_modDll = 0;
	string modPath;

#ifndef SP_DEMO
	if (stricmp(pModName, GAME_NAME) != 0)
	{
		modPath.append("Mods\\");
		modPath.append(pModName);
		modPath.append("\\");

		string filename;
		filename.append("..\\");
		filename.append(modPath);

#ifdef WIN64
		filename.append("Bin64\\");
#else
		filename.append("Bin32\\");
#endif

		filename.append(pModName);
		filename.append(".dll");

#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
		m_modDll = CryLoadLibrary(filename.c_str());
#endif
	}
#endif

	if (!m_modDll)
	{
		ModuleInitISystem(m_pFramework->GetISystem());
		static char pGameBuffer[sizeof(CGame)];
		m_pMod = new ((void*)pGameBuffer) CGame();
	}
	else
	{
		IGame::TEntryFunction CreateGame = (IGame::TEntryFunction)CryGetProcAddress(m_modDll, "CreateGame");
		if (!CreateGame)
			return 0;

		m_pMod = CreateGame(m_pFramework);
	}

	if (m_pMod && m_pMod->Init(m_pFramework))
	{
		return m_modRef;
	}

	return 0;
}

void CGameStartup::Shutdown()
{
#ifdef WIN32
	AllowAccessibilityShortcutKeys(true);
#endif
	// we are not dynamically allocated (see GameDll.cpp)... therefore
	// we must not call delete here (it will cause big problems)...
	// call the destructor manually instead
	this->~CGameStartup();
}

int CGameStartup::Update(bool haveFocus, unsigned int updateFlags)
{
	int returnCode = 0;

	// update the game
	if (m_pMod)
	{
		returnCode = m_pMod->Update(haveFocus, updateFlags);
	}

	if (gEnv && gEnv->pSystem && gEnv->pConsole)
	{
#ifdef WIN32
		if(gEnv && gEnv->pRenderer && gEnv->pRenderer->GetHWND())
		{
			bool focus = (::GetFocus() == gEnv->pRenderer->GetHWND()) && (::GetForegroundWindow() == gEnv->pRenderer->GetHWND());
			static bool focused = focus;
			if (focus != focused)
			{
				if(gEnv->pSystem->GetISystemEventDispatcher())
				{
					gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS, focus, 0);
				}
				focused = focus;
			}
		}
#endif
	}

	// ghetto fullscreen detection, because renderer does not provide any kind of listener
	if (gEnv && gEnv->pSystem && gEnv->pConsole)
	{
		static ICVar *pVar=0;
		
		if(!pVar)
			pVar = gEnv->pConsole->GetCVar("r_Fullscreen");

		if(pVar)
		{
			static int fullscreen = pVar->GetIVal();
			if (fullscreen != pVar->GetIVal())
			{
				if(gEnv->pSystem->GetISystemEventDispatcher())
				{
					gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_TOGGLE_FULLSCREEN, pVar->GetIVal(), 0);
				}
				fullscreen = pVar->GetIVal();
			}
		}
	}

	return returnCode;
}

bool CGameStartup::GetRestartLevel(char** levelName)
{
	if(GetISystem()->IsRelaunch())
		*levelName = (char*)(gEnv->pGame->GetIGameFramework()->GetLevelName());
	return GetISystem()->IsRelaunch();
}

bool CGameStartup::GetRestartMod(char* pModName, int nameLenMax)
{
	if (m_reqModName.empty())
		return false;

	strncpy(pModName, m_reqModName.c_str(), nameLenMax);
	pModName[nameLenMax-1] = 0;
	return true;
}

void CGameStartup::RequestLoadMod(IConsoleCmdArgs* pCmdArgs)
{

	STARTUPINFO si;
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;

	char pBuffer[512];
	strcpy(pBuffer, " -modrestart");

	if (pCmdArgs->GetArgCount() == 2)
	{
		m_reqModName = pCmdArgs->GetArg(1);
		ISystem* pSystem = m_pFramework->GetISystem();

		if (m_reqModName)
		{
			  
			 strcat(pBuffer, " -mod ");
			 strcat(pBuffer, m_reqModName);
		}
	}

	if(gEnv->pSystem->IsDevMode())
	{
		strcat(pBuffer, " -devmode");
	}

	if(gEnv->pRenderer->GetRenderType() == eRT_DX10)
	{
		strcat(pBuffer, " -dx10");
	}
	else if(gEnv->pRenderer->GetRenderType() == eRT_DX9)
	{
		strcat(pBuffer, " -dx9");
	}

	CryFixedStringT<64> filename("");

#ifdef WIN64
	filename.append("Bin64/");
#else
	filename.append("Bin32/");
#endif

	filename.append("Crysis.exe");


	CreateProcess(filename.c_str(), pBuffer, NULL, NULL, FALSE, /*flags*/ 0, NULL, NULL, &si, &pi);

	gEnv->pSystem->Quit();

}

void CGameStartup::ForceCursorUpdate()
{
#ifdef WIN32
	if(gEnv && gEnv->pRenderer && gEnv->pRenderer->GetHWND())
	{
		SendMessage(HWND(gEnv->pRenderer->GetHWND()),WM_SETCURSOR,0,0);
	}
#endif
}

const char* CGameStartup::GetPatch() const
{
	INetworkService* pService = gEnv->pGame->GetIGameFramework()->GetISystem()->GetINetwork()->GetService("GameSpy");
	if(pService)
	{
		IPatchCheck* pPC = pService->GetPatchCheck();
		if(pPC && pPC->GetInstallOnExit())
		{
			return pPC->GetPatchFileName();
		}
	}
	return NULL;	
}

void CGameStartup::AllowAccessibilityShortcutKeys(bool bAllowKeys)
{
#if defined(WIN32)
	if(!g_StickyKeysStatusSaved)
	{
		SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &g_StartupStickyKeys, 0);
		SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_StartupToggleKeys, 0);
		SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &g_StartupFilterKeys, 0);
		g_StickyKeysStatusSaved = true;
		atexit(RestoreStickyKeys);
	}

	if(bAllowKeys)
	{
		// Restore StickyKeys/etc to original state and enable Windows key      
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &g_StartupStickyKeys, 0);
		SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_StartupToggleKeys, 0);
		SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &g_StartupFilterKeys, 0);
	}
	else
	{
		STICKYKEYS skOff = g_StartupStickyKeys;
		skOff.dwFlags &= ~SKF_HOTKEYACTIVE;
		skOff.dwFlags &= ~SKF_CONFIRMHOTKEY; 
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &skOff, 0);

		TOGGLEKEYS tkOff = g_StartupToggleKeys;
		tkOff.dwFlags &= ~TKF_HOTKEYACTIVE;
		tkOff.dwFlags &= ~TKF_CONFIRMHOTKEY;
		SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tkOff, 0);

		FILTERKEYS fkOff = g_StartupFilterKeys;
		fkOff.dwFlags &= ~FKF_HOTKEYACTIVE;
		fkOff.dwFlags &= ~FKF_CONFIRMHOTKEY;
		SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fkOff, 0);
	}
#endif
}


int CGameStartup::Run( const char * autoStartLevelName )
{
	gEnv->pConsole->ExecuteString( "exec autoexec.cfg" );
	if (autoStartLevelName)
	{
		//load savegame
		if(CryStringUtils::stristr(autoStartLevelName, ".CRYSISJMSF") != 0 )
		{
			CryFixedStringT<256> fileName (autoStartLevelName);
			// NOTE! two step trimming is intended!
			fileName.Trim(" ");  // first:  remove enclosing spaces (outside ")
			fileName.Trim("\""); // second: remove potential enclosing "
			gEnv->pGame->GetIGameFramework()->LoadGame(fileName.c_str());
		}
		else	//start specified level
		{
			CryFixedStringT<256> mapCmd ("map ");
			mapCmd+=autoStartLevelName;
			gEnv->pConsole->ExecuteString(mapCmd.c_str());
		}
	}

#ifdef WIN32
	if (!(gEnv && gEnv->pSystem) || (!gEnv->pSystem->IsEditor() && !gEnv->pSystem->IsDedicated()))
	{
		::ShowCursor(TRUE);
		if (gEnv && gEnv->pHardwareMouse)
			gEnv->pHardwareMouse->DecrementCounter();
	}

	AllowAccessibilityShortcutKeys(false);

	for(;;)
	{
		MSG msg;

		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			if (msg.message != WM_QUIT)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				break;
			}
		}
		else
		{
			if (!Update(true, 0))
			{
				// need to clean the message loop (WM_QUIT might cause problems in the case of a restart)
				// another message loop might have WM_QUIT already so we cannot rely only on this 
				while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				break;
			}
		}
	}
#else
	for(;;)
	{
		if (!Update(true, 0))
		{
			break;
		}
	}
#endif //WIN32

	return 0;
}

bool CGameStartup::InitFramework(SSystemInitParams &startupParams)
{
#if !defined(_LIB) && !defined(LINUX) && !defined(PS3)
	m_frameworkDll = GetFrameworkDLL();

	if (!m_frameworkDll)
	{
		// failed to open the framework dll
		CryError("Failed to open the GameFramework DLL!");

		return false;
	}

	IGameFramework::TEntryFunction CreateGameFramework = (IGameFramework::TEntryFunction)CryGetProcAddress(m_frameworkDll, DLL_INITFUNC_CREATEGAME );

	if (!CreateGameFramework)
	{
		// the dll is not a framework dll
		CryError("Specified GameFramework DLL is not valid!");

		return false;
	}
#endif //_LIB

	m_pFramework = CreateGameFramework();

	if (!m_pFramework)
	{
		CryError("Failed to create the GameFramework Interface!");
		// failed to create the framework

		return false;
	}

	if (!startupParams.hWnd)
	{
		m_initWindow = true;

		if (!InitWindow(startupParams))
		{
			// failed to register window class
			CryError("Failed to register CryENGINE window class!");

			return false;
		}
	}

	// initialize the engine
	if (!m_pFramework->Init(startupParams))
	{
		CryError("Failed to initialize CryENGINE!");

		return false;
	}
	ModuleInitISystem(m_pFramework->GetISystem());

	return true;
}

void CGameStartup::ShutdownFramework()
{
	if (m_pFramework)
	{
		m_pFramework->Shutdown();
		m_pFramework = 0;
	}

	ShutdownWindow();
}

bool CGameStartup::InitWindow(SSystemInitParams &startupParams)
{
#ifdef WIN32
	WNDCLASS wc;

	memset(&wc, 0, sizeof(WNDCLASS));

	wc.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wc.lpfnWndProc   = (WNDPROC)CGameStartup::WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = GetModuleHandle(0);
#ifdef SP_DEMO
	wc.hIcon         = LoadIcon((HINSTANCE)startupParams.hInstance, MAKEINTRESOURCE(102));
	wc.hCursor       = LoadCursor((HINSTANCE)startupParams.hInstance, MAKEINTRESOURCE(101));
#else
	// FIXME: Very bad way of getting the Icon and Cursor from the Launcher project
	wc.hIcon         = LoadIcon((HINSTANCE)startupParams.hInstance, MAKEINTRESOURCE(101));
	wc.hCursor       = LoadCursor((HINSTANCE)startupParams.hInstance, MAKEINTRESOURCE(105));
#endif
	wc.hbrBackground =(HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = GAME_WINDOW_CLASSNAME;

	if (!RegisterClass(&wc))
	{
		return false;
	}

	if (startupParams.pSystem == NULL || (!startupParams.pSystem->IsEditor() && !startupParams.pSystem->IsDedicated()))
		::ShowCursor(FALSE);

#endif WIN32
	return true;
}

void CGameStartup::ShutdownWindow()
{
#ifdef WIN32
	if (m_initWindow)
	{
		UnregisterClass(GAME_WINDOW_CLASSNAME, GetModuleHandle(0));
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
#ifdef WIN32

//////////////////////////////////////////////////////////////////////////
void LogWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!gEnv || !gEnv->pLog)
		return;

	switch(msg)
	{
	case WM_MOUSEACTIVATE:
		gEnv->pLog->Log("MSG: WM_MOUSEACTIVATE (%s %s)", (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_ENTERSIZEMOVE:
		gEnv->pLog->Log("MSG: WM_ENTERSIZEMOVE (%s %s)", (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_EXITSIZEMOVE:
		gEnv->pLog->Log("MSG: WM_EXITSIZEMOVE (%s %s)", (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_ENTERMENULOOP:
		gEnv->pLog->Log("MSG: WM_ENTERMENULOOP (%s %s)", (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_EXITMENULOOP:
		gEnv->pLog->Log("MSG: WM_EXITMENULOOP (%s %s)", (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_MOVE:
		gEnv->pLog->Log("MSG: WM_MOVE %d %d (%s %s)", LOWORD(lParam), HIWORD(lParam), (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_SIZE:
		gEnv->pLog->Log("MSG: WM_SIZE %d %d (%s %s)", LOWORD(lParam), HIWORD(lParam), (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_ACTIVATEAPP:
		gEnv->pLog->Log("MSG: WM_ACTIVATEAPP %d (%s %s)", wParam, (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_ACTIVATE:
		gEnv->pLog->Log("MSG: WM_ACTIVATE %d (%s %s)", LOWORD(wParam), (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_SETFOCUS:
		gEnv->pLog->Log("MSG: WM_SETFOCUS (%s %s)", (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_KILLFOCUS:
		gEnv->pLog->Log("MSG: WM_KILLFOCUS (%s %s)", (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");	
		break;
	case WM_WINDOWPOSCHANGED:
		gEnv->pLog->Log("MSG: WM_WINDOWPOSCHANGED (%s %s)", (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_STYLECHANGED:
		gEnv->pLog->Log("MSG: WM_STYLECHANGED (%s %s)", (GetFocus()==hWnd)?"focused":"", (GetForegroundWindow()==hWnd)?"foreground":"");
		break;
	case WM_INPUTLANGCHANGE:
		gEnv->pLog->Log("MSG: WM_INPUTLANGCHANGE");
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK CGameStartup::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (g_debugWindowsMessages)
		LogWndProc(hWnd, msg, wParam, lParam);

	switch(msg)
	{
	case WM_CLOSE:
		if (gEnv && gEnv->pSystem)
			gEnv->pSystem->Quit();
		return 0;
	case WM_MOUSEACTIVATE:
		return MA_ACTIVATE;
	case WM_ENTERSIZEMOVE:
		g_inSizeMove = true;
		if (gEnv && gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->IncrementCounter();
		}
		return  0;
	case WM_EXITSIZEMOVE:
		g_inSizeMove = false;
		if (gEnv && gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->DecrementCounter();
			gEnv->pHardwareMouse->ConfineCursor(true);
		}
		return  0;
	case WM_ENTERMENULOOP:
		if (gEnv && gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->IncrementCounter();
		}
		return  0;
	case WM_EXITMENULOOP:
		if (gEnv && gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->DecrementCounter();
		}
		return  0;
	case WM_HOTKEY:
	case WM_SYSCHAR:	// prevent ALT + key combinations from creating 'ding' sounds
		return  0;
	case WM_CHAR:
		{
			if (gEnv && gEnv->pInput)
			{
				SInputEvent event;
				event.modifiers = gEnv->pInput->GetModifiers();
				event.deviceId = eDI_Keyboard;
				event.state = eIS_UI;
				event.value = 1.0f;
				event.pSymbol = 0;//m_rawKeyboard->GetSymbol((lParam>>16)&0xff);
				if (event.pSymbol)
					event.keyId = event.pSymbol->keyId;

				wchar_t tmp[2] = { 0 };
				MultiByteToWideChar(CP_ACP, 0, (char*)&wParam, 1, tmp, 2);
				event.timestamp = tmp[0];

				char szKeyName[4] = {0};
				if (wctomb(szKeyName, (WCHAR)wParam) != -1)
				{
					if (szKeyName[1]==0 && ((unsigned char)szKeyName[0])>=32)
					{
						event.keyName = szKeyName;
						gEnv->pInput->PostInputEvent(event);
					}
				}
			}
		}
		return 0;
	case WM_SYSKEYDOWN:	// prevent ALT-key entering menu loop
		if (wParam != VK_RETURN && wParam != VK_F4)
		{
			return 0;
		}
		else
		{
			if (wParam == VK_RETURN)	// toggle fullscreen
			{
				if (gEnv && gEnv->pRenderer && gEnv->pRenderer->GetRenderType() != eRT_DX10)
				{
					static ICVar *pVar = gEnv->pConsole->GetCVar("r_Fullscreen");
					if (pVar)
					{
						int fullscreen = pVar->GetIVal();
						pVar->Set((int)(fullscreen == 0));
						if (fullscreen != 0 && gEnv->pHardwareMouse)
							gEnv->pHardwareMouse->ConfineCursor(false);
					}
				}
			}
			// let the F4 pass through to default handler (it will send an WM_CLOSE)
		}
		break;
	case WM_SETCURSOR:
		if(g_pGame && g_pGame->GetOptions())
		{
#ifdef SP_DEMO
			int iResource = 101;
#else
			ECrysisProfileColor eCrysisProfileColor = g_pGame->GetOptions()->GetCrysisProfileColor();
			int iResource = 102;
			switch(eCrysisProfileColor)
			{
			case CrysisProfileColor_Amber:		iResource = 103;	break;
			case CrysisProfileColor_Blue:			iResource = 104;	break;
			case CrysisProfileColor_Green:		iResource = 105;	break;
			case CrysisProfileColor_Cyan:			iResource = 106;	break;
			case CrysisProfileColor_White:		iResource = 107;	break;
			default:													CRY_ASSERT(0);		break;
			}			
#endif
			HCURSOR hCursor = LoadCursor(GetModuleHandle(0),MAKEINTRESOURCE(iResource));
			::SetCursor(hCursor);
		}
		return 0;
	case WM_MOUSEMOVE:
		if(gEnv && gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->Event(LOWORD(lParam),HIWORD(lParam),HARDWAREMOUSEEVENT_MOVE);
		}
		return 0;
	case WM_LBUTTONDOWN:
		if(gEnv && gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->Event(LOWORD(lParam),HIWORD(lParam),HARDWAREMOUSEEVENT_LBUTTONDOWN);
		}
		return 0;
	case WM_LBUTTONUP:
		if(gEnv && gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->Event(LOWORD(lParam),HIWORD(lParam),HARDWAREMOUSEEVENT_LBUTTONUP);
		}
		return 0;
	case WM_LBUTTONDBLCLK:
		if(gEnv && gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->Event(LOWORD(lParam),HIWORD(lParam),HARDWAREMOUSEEVENT_LBUTTONDOUBLECLICK);
		}
		return 0;
	case WM_MOVE:
		if(gEnv && gEnv->pSystem && gEnv->pSystem->GetISystemEventDispatcher())
		{
			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_MOVE,LOWORD(lParam), HIWORD(lParam));
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	case WM_SIZE:
		if(gEnv && gEnv->pSystem && gEnv->pSystem->GetISystemEventDispatcher())
		{
			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_RESIZE,LOWORD(lParam), HIWORD(lParam));
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	case WM_ACTIVATEAPP:
		// always unconfine cursor if deactivating
		if (wParam == FALSE && gEnv && gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->ConfineCursor(false);
		}
		break;
	case WM_ACTIVATE:
		if(gEnv && gEnv->pSystem && gEnv->pSystem->GetISystemEventDispatcher())
		{
			//gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS, LOWORD(wParam) != WA_INACTIVE, 0);
		}
		break;
	case WM_SETFOCUS:
		if(gEnv && gEnv->pSystem && gEnv->pSystem->GetISystemEventDispatcher())
		{
			//gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS, 1, 0);
		}
		break;
	case WM_KILLFOCUS:
		if(gEnv && gEnv->pSystem && gEnv->pSystem->GetISystemEventDispatcher())
		{
			//gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS, 0, 0);
		}
		break;
	case WM_WINDOWPOSCHANGED:
		if(gEnv && gEnv->pHardwareMouse && !g_inSizeMove)
		{
			gEnv->pHardwareMouse->ConfineCursor(true);
		}
		if(gEnv && gEnv->pSystem && gEnv->pSystem->GetISystemEventDispatcher())
		{
			//gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS, 1, 0);
		}
		break;
	case WM_STYLECHANGED:
		if(gEnv && gEnv->pSystem && gEnv->pSystem->GetISystemEventDispatcher())
		{
			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS, 1, 0);
		}
		break;
	case WM_INPUTLANGCHANGE:
		if(gEnv && gEnv->pSystem && gEnv->pSystem->GetISystemEventDispatcher())
		{
			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LANGUAGE_CHANGE, wParam, lParam);
		}
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}
//////////////////////////////////////////////////////////////////////////
#endif //WIN32

