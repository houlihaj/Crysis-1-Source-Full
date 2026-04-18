#include "StdAfx.h"
#include "GameObjectSystem.h"

#include "WorldQuery.h"
#include "Interactor.h"
#include "CameraSource.h"

#include "IGameFramework.h"

// entityClassString can be different from extensionClassName to allow mapping entity classes to differently
// named C++ Classes
#define REGISTER_GAME_OBJECT_EXTENSION(framework, entityClassString, extensionClassName, script)\
	{\
	IEntityClassRegistry::SEntityClassDesc clsDesc;\
	clsDesc.sName = entityClassString;\
	clsDesc.sScriptFile = script;\
  struct C##extensionClassName##Creator : public IGameObjectExtensionCreatorBase\
		{\
		C##extensionClassName *Create()\
			{\
			return new C##extensionClassName();\
			}\
			void GetGameObjectExtensionRMIData( void ** ppRMI, size_t * nCount )\
			{\
			C##extensionClassName::GetGameObjectExtensionRMIData( ppRMI, nCount );\
			}\
		};\
		static C##extensionClassName##Creator _creator;\
		framework->GetIGameObjectSystem()->RegisterExtension(entityClassString, &_creator, &clsDesc);\
	} \


void CGameObjectSystem::RegisterFactories( IGameFramework * pFW )
{
	REGISTER_FACTORY(pFW, "WorldQuery", CWorldQuery, false);
	REGISTER_FACTORY(pFW, "Interactor", CInteractor, false);

	// REGISTER_GAME_OBJECT_EXTENSION(pFW, "CameraSource", CameraSource, "Scripts/Entities/Others/CameraSource.lua");
}
