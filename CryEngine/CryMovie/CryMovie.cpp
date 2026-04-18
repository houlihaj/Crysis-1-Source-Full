// CryMovie.cpp : Defines the entry point for the DLL application.
//

#include "StdAfx.h"
#include "CryMovie.h"
#include "Movie.h"
#include <CrtDebugStats.h>

// Included only once per DLL module.
#include <platform_impl.h>

/*
#ifdef WIN32
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH: 
			break;
  }
  return TRUE;
}
#endif //WIN32
*/

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_Movie : public ISystemEventListener
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

static CSystemEventListner_Movie g_system_event_listener_movie;

extern "C"
{

CRYMOVIE_API IMovieSystem *CreateMovieSystem(ISystem *pSystem)
{
	ModuleInitISystem(pSystem);
	pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_movie);
	return new CMovieSystem(pSystem);
};
}