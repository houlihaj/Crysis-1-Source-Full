// CryScriptSystem.cpp : Defines the entry point for the DLL application.
//

#include "StdAfx.h"
#include "ScriptSystem.h"

// Included only once per DLL module.
#include <platform_impl.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
HANDLE gDLLHandle = NULL;
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved )
{
	gDLLHandle = hModule;
	return TRUE;
}
#endif // WIN32

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_Script : public ISystemEventListener
{
public:
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
	{
		switch (event)
		{
		case ESYSTEM_EVENT_RANDOM_SEED:
			g_random_generator.seed(wparam);
			break;
		case ESYSTEM_EVENT_LEVEL_RELOAD:
			{
				STLALLOCATOR_CLEANUP;
				break;
			}

		}
	}
};
static CSystemEventListner_Script g_system_event_listener_script;

//////////////////////////////////////////////////////////////////////
IScriptSystem *CreateScriptSystem( ISystem *pSystem,bool bStdLibs )
{
	ModuleInitISystem(pSystem);
	CScriptSystem *pScriptSystem = new CScriptSystem;

	pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_script);

	if (!pScriptSystem->Init( pSystem,bStdLibs, 1024))
	{
		pScriptSystem->Release();
		pScriptSystem = 0;
	}
	return pScriptSystem;
}

#include <CrtDebugStats.h>
