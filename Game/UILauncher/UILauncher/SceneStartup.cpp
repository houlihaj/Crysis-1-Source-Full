#include "StdAfx.h"
#include "SceneStartup.h"


#include <StringUtils.h>
#include <CryFixedString.h>
#include <CryLibrary.h>
#include <platform_impl.h>
#include <INetworkService.h>
#include <IHardwareMouse.h>
#include <ICryPak.h>

#include "../CrySystem/CryWaterMark.h"
#include "resource.h"
#include "Launcher.h"


#define DLL_INITFUNC_CREATEGAME "CreateGameFramework"


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


IGame* CSceneStartup::m_pMod					= NULL;
IGameRef CSceneStartup::m_modRef;
IGameFramework* CSceneStartup::m_pFramework		= NULL;

HMODULE CSceneStartup::m_modDll					= 0;
HMODULE CSceneStartup::m_frameworkDll			= 0;
HMODULE CSceneStartup::m_systemDll				= 0;

string CSceneStartup::m_rootDir;
string CSceneStartup::m_binDir;
string CSceneStartup::m_reqModName;

bool CSceneStartup::m_initWindow				= false;


CSceneStartup::CSceneStartup()
{
	m_modRef = &m_pMod;
}


CSceneStartup::~CSceneStartup()
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


void CSceneStartup::InitRootDir()
{
	char szExeFileName[_MAX_PATH];
	char szDrive[_MAX_DRIVE];
	char szDir[_MAX_DIR];
	char szEngineDirectory[_MAX_PATH];

	GetModuleFileName(GetModuleHandle(NULL), szExeFileName, _MAX_PATH);
	_splitpath(szExeFileName, szDrive, szDir, NULL, NULL);

	_makepath(szEngineDirectory, szDrive, szDir, NULL, NULL );
	strncat(szEngineDirectory, "../", _MAX_PATH);
	
	SetCurrentDirectory( szEngineDirectory );
}


IGameRef CSceneStartup::Init(SSystemInitParams &startupParams)
{  
	InitRootDir();

	if (!InitFramework(startupParams))
	{
		return 0;
	}

	LOADING_TIME_PROFILE_SECTION(m_pFramework->GetISystem());


	IGameRef pOut = Reset(GAME_DLL_URL);
	if (!m_pFramework->CompleteInit())
	{
		pOut->Shutdown();
		return 0;
	}

	return pOut;
}


IGameRef CSceneStartup::Reset(const char *pModName)
{
	if (m_pMod)
	{
		m_pMod->Shutdown();

		if (m_modDll)
		{
			CryFreeLibrary(m_modDll);
			m_modDll = 0;
		}

		m_pMod = NULL;
	}

	{
		m_modDll = CryLoadLibrary(pModName);
		
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


void CSceneStartup::Shutdown()
{
	this->~CSceneStartup();
}


IGameFramework* CSceneStartup::GetGameFramework()
{
	return m_pFramework;
}


int CSceneStartup::Update(bool haveFocus, unsigned int updateFlags)
{
	int returnCode = 0;
	
	// update the game
	if (m_pMod)
	{		
		returnCode = m_pMod->Update(haveFocus, updateFlags);
	}

	return returnCode;
}


int CSceneStartup::Run( const char * autoStartLevelName )
{
	return 0;
}


bool CSceneStartup::GetRestartLevel(char** levelName)
{
	if(GetISystem()->IsRelaunch())
		*levelName = (char*)(gEnv->pGame->GetIGameFramework()->GetLevelName());
	return GetISystem()->IsRelaunch();
}


bool CSceneStartup::GetRestartMod(char* pModName, int nameLenMax)
{
	if (m_reqModName.empty())
		return false;

	strncpy(pModName, m_reqModName.c_str(), nameLenMax);
	pModName[nameLenMax-1] = 0;
	return true;
}


const char* CSceneStartup::GetPatch() const
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


bool CSceneStartup::InitFramework(SSystemInitParams &startupParams)
{
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

	m_pFramework = CreateGameFramework();
	if (!m_pFramework)
	{
		CryError("Failed to create the GameFramework Interface!");
		// failed to create the framework

		return false;
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


void CSceneStartup::ShutdownFramework()
{
	if (m_pFramework)
	{
		m_pFramework->Shutdown();
		m_pFramework = 0;
	}
}
