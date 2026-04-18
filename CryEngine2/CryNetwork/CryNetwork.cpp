//////////////////////////////////////////////////////////////////////
//
//	Crytek Network source code
//	
//	File: crynetwork.cpp
//  Description: dll entry point
//
//	History:
//	-July 25,2001:Created by Alberto Demichelis
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Network.h"
// Included only once per DLL module.
#include <platform_impl.h>


//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_Network : public ISystemEventListener
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

static CSystemEventListner_Network g_system_event_listener_network;

/*
//////////////////////////////////////////////////////////////////////////
#if !defined(XBOX) && !defined(LINUX)
BOOL APIENTRY DllMain( HANDLE hModule, 
                       uint32  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{


    return TRUE;
}

#endif
*/

#ifndef _XBOX
CRYNETWORK_API INetwork *CreateNetwork(ISystem *pSystem, int ncpu)
#else
INetwork *CreateNetwork(ISystem *pSystem, int ncpu)
#endif
{
	ModuleInitISystem(pSystem);

	CNetwork *pNetwork=new CNetwork;

	if(!pNetwork->Init(ncpu))
	{
		delete pNetwork;
		return NULL;
	}
	pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_network);
	return pNetwork;
}

#include <CrtDebugStats.h>

