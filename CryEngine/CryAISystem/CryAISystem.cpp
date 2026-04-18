	// CryAISystem.cpp : Defines the entry point for the DLL application.
//

#include "StdAfx.h"
#include "CryAISystem.h"
#include "CAISystem.h"
#include "AILog.h"
#include <platform_impl.h>
#include <ISystem.h>


CAISystem *g_pAISystem;
static bool isAIInDevMode = false;

AISIGNALS_CRC g_crcSignals;


bool IsAIInDevMode()
{
  return isAIInDevMode;
}

/*
//////////////////////////////////////////////////////////////////////////
// Pointer to Global ISystem.
static ISystem* gISystem = 0;
ISystem* GetISystem()
{
	return gISystem;
}
*/
//////////////////////////////////////////////////////////////////////////

#ifdef WIN32
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved )
{
	return TRUE;
}
#endif

int g_random_count = 0;


//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_AI : public ISystemEventListener
{
public:
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
	{
		switch (event)
		{
		case ESYSTEM_EVENT_RANDOM_SEED:
			g_random_generator.seed(wparam);
			g_random_count = 0;
			break;
		case ESYSTEM_EVENT_LEVEL_RELOAD:
			{
				STLALLOCATOR_CLEANUP;
				break;
			}
		}
	}
};
static CSystemEventListner_AI g_system_event_listener_ai;

CRYAIAPI IAISystem *CreateAISystem( ISystem *pSystem )
{
	ModuleInitISystem(pSystem);
	
	pSystem->GetISystemEventDispatcher()->RegisterListener( &g_system_event_listener_ai );

#if (defined(WIN32) || defined(WIN64))
	HKEY  key;
	DWORD type;
	// Open the appropriate registry key
	LONG result = RegOpenKeyEx( HKEY_CURRENT_USER,"Software\\Crytek\\AIConfig",0, KEY_READ, &key );
	if( ERROR_SUCCESS == result )
	{
		DWORD dwVal;
		DWORD size=sizeof(dwVal);

		result = RegQueryValueEx( key, "DevMode", NULL,&type, (LPBYTE)&dwVal, &size );

		if(ERROR_SUCCESS==result && type==REG_DWORD)
			isAIInDevMode = (dwVal!=0);
		else
			isAIInDevMode=false;
	}
#else
  isAIInDevMode=false;
#endif


	AIInitLog(pSystem);

	return (g_pAISystem = new CAISystem(pSystem));
}

//////////////////////////////////////////////////////////////////////////

#include <CrtDebugStats.h>

#include "TypeInfo_impl.h"
#include "AutoTypeStructs_info.h"
#include "Cry_Geo_info.h"
#include "Cry_Vector3_info.h"
#include "Cry_Quat_info.h"
#include "Cry_Matrix_info.h"

