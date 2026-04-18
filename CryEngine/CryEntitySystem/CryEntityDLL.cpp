// CryEntityDLL.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

#include <IEntitySystem.h>
#include "EntitySystem.h"
// Included only once per DLL module.
#include <platform_impl.h>


struct CSystemEventListner_Entity : public ISystemEventListener
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
static CSystemEventListner_Entity g_system_event_listener_entity;

CEntitySystem *g_pIEntitySystem = NULL;

#if !defined(_XBOX) && !defined(LINUX) && !defined(PS3)
CRYENTITYDLL_API struct IEntitySystem * CreateEntitySystem(ISystem *pISystem)
#else
struct IEntitySystem * CreateEntitySystem(ISystem *pISystem)
#endif
{
	ModuleInitISystem(pISystem);
	CEntitySystem *pEntitySystem= new CEntitySystem(pISystem);
	g_pIEntitySystem = pEntitySystem;
	if(!pEntitySystem->Init(pISystem))
	{
		pEntitySystem->Release();
		return NULL;
	}
	pISystem->GetISystemEventDispatcher()->RegisterListener( &g_system_event_listener_entity );
	return pEntitySystem;
}

#include <CrtDebugStats.h>
