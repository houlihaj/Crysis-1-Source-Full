//////////////////////////////////////////////////////////////////////
//
//  CryFont Source Code
//
//  File: ICryFont.cpp
//  Description: Create the font interface.
//
//  History:
//  - August 17, 2001: Created by Alberto Demichelis
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
// Included only once per DLL module.
#include <platform_impl.h>

#include "CryFont.h"
#if defined(USE_NULLFONT)
#include "NullFont.h"
#endif

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_Font : public ISystemEventListener
{
public:
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
	{
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_RELOAD:
			{
				STLALLOCATOR_CLEANUP;
				break;
			}
		}
	}
};
static CSystemEventListner_Font g_system_event_listener_font;

///////////////////////////////////////////////
extern "C" ICryFont* CreateCryFontInterface(ISystem *pSystem)
{
	ModuleInitISystem(pSystem);


	if (pSystem->IsDedicated())
	{
#if defined(USE_NULLFONT)
		return new CCryNullFont();
#else
		// The NULL font implementation must be present for all platforms
		// supporting running as a pure dedicated server.
		pSystem->GetILog()->LogError(
				"Missing NULL font implementation for dedicated server");
		return NULL;
#endif
	}
	else
	{
		pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_font);
		return new CCryFont(pSystem);
	}
}

/*
///////////////////////////////////////////////
#ifndef _XBOX
#ifndef PS2
BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}
#endif
#endif
*/
