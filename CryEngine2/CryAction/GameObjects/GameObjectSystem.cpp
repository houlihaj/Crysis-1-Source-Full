#include "StdAfx.h"
#include "GameObjects/GameObject.h"
#include "GameObjectSystem.h"
#include "IGameObject.h"

static bool StringToKey( const char * s, uint32& key )
{
	const size_t len = strlen(s);
	if (len > 4)
		return false;

	key = 0;
	for (size_t i=0; i<len; i++)
	{
		key <<= 8;
		key |= uint8(s[i]);
	}

	return true;
}

bool CGameObjectSystem::Init()
{
	IEntityClassRegistry::SEntityClassDesc desc;
	desc.sName = "PlayerProximityTrigger";
	desc.flags = ECLF_INVISIBLE;
	IEntityClassRegistry * pClsReg = gEnv->pEntitySystem->GetClassRegistry();
	m_pClassPlayerProximityTrigger = pClsReg->RegisterStdClass( desc );
	if (!m_pClassPlayerProximityTrigger)
		return false;

	memset(&m_defaultProfiles, 0, sizeof(m_defaultProfiles));

	if (XmlNodeRef schedParams = gEnv->pSystem->LoadXmlFile(PathUtil::GetGameFolder() + "/Scripts/Network/EntityScheduler.xml"))
	{
		uint32 defaultPolicy = 0;

		if (XmlNodeRef defpol = schedParams->findChild("Default"))
		{
			if (!StringToKey(defpol->getAttr("policy"), defaultPolicy))
			{
				GameWarning("Unable to read Default from EntityScheduler.xml");
			}
		}

		m_defaultProfiles.normal = m_defaultProfiles.owned = defaultPolicy;

		for (int i=0; i<schedParams->getChildCount(); i++)
		{
			XmlNodeRef node = schedParams->getChild(i);
			if (0 != strcmp(node->getTag(), "Class"))
				continue;

			string name = node->getAttr("name");

			SEntitySchedulingProfiles p;
			p.normal = defaultPolicy;
			if (node->haveAttr("policy"))
				StringToKey(node->getAttr("policy"), p.normal);
			p.owned = p.normal;
			if (node->haveAttr("own"))
				StringToKey(node->getAttr("own"), p.owned);

			m_schedulingParams[name] = p;
		}
	}

	m_pSpawnSerializer=0;
	
	return true;
}

IEntity * CGameObjectSystem::CreatePlayerProximityTrigger()
{
	IEntitySystem * pES = gEnv->pEntitySystem;
	SEntitySpawnParams params;
	params.nFlags = ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_NO_SAVE;
	params.pClass = m_pClassPlayerProximityTrigger;
  params.sName = "PlayerProximityTrigger";
	IEntity * pEntity = pES->SpawnEntity( params );
	if (!pEntity)
		return NULL;
	if (!pEntity->CreateProxy(ENTITY_PROXY_TRIGGER))
	{
		pES->RemoveEntity( pEntity->GetId() );
		return NULL;
	}
	return pEntity;
}

void CGameObjectSystem::RegisterExtension( const char * name, IGameObjectExtensionCreatorBase * pCreator, IEntityClassRegistry::SEntityClassDesc * pClsDesc )
{
	string sName = name;

	if (m_nameToID.find(sName) != m_nameToID.end())
		CryError("Duplicate game object extension %s found", name);

	SExtensionInfo info;
	info.name = sName;
	info.pFactory = pCreator;
	ExtensionID id = m_extensionInfo.size();
	m_extensionInfo.push_back(info);
	m_nameToID[sName] = id;

	assert( GetName(GetID(sName)) == sName );

	// bind network interface
	void * pRMI;
	size_t nRMI;
	pCreator->GetGameObjectExtensionRMIData( &pRMI, &nRMI );
	m_dispatch.RegisterInterface( (SGameObjectExtensionRMI*)pRMI, nRMI );

	if (pClsDesc)
	{
		pClsDesc->pUserProxyCreateFunc = CreateGameObjectWithPreactivatedExtension;
//		pClsDesc->pUserProxyData = new SSpawnUserData(sName);
		if (!gEnv->pEntitySystem->GetClassRegistry()->RegisterStdClass(*pClsDesc))
		{
			CRY_ASSERT_MESSAGE(0, "Unable to register entity class");
			return;
		}
	}
}

void CGameObjectSystem::DefineProtocol( bool server, IProtocolBuilder * pBuilder )
{
	INetMessageSink * pSink = server? m_dispatch.GetServerSink() : m_dispatch.GetClientSink();
	pSink->DefineProtocol( pBuilder );
}

IGameObjectSystem::ExtensionID CGameObjectSystem::GetID( const char * name )
{
	std::map<string, ExtensionID>::const_iterator iter = m_nameToID.find(CONST_TEMP_STRING(name));
	if (iter != m_nameToID.end())
		return iter->second;
	else
		return InvalidExtensionID;
}

const char * CGameObjectSystem::GetName( ExtensionID id )
{
	if (id > m_extensionInfo.size())
		return NULL;
	else
		return m_extensionInfo[id].name.c_str();
}

void CGameObjectSystem::BroadcastEvent( const SGameObjectEvent& evt )
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	//CryLog("BroadcastEvent called");

	IEntityItPtr pEntIt = gEnv->pEntitySystem->GetEntityIterator();
	while (IEntity * pEntity = pEntIt->Next())
	{
		if (CGameObject * pGameObject = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER))
		{
			pGameObject->SendEvent( evt );
		}
	}
}

void CGameObjectSystem::RegisterEvent( uint32 id, const char* name )
{
	if (GetEventID(name) != InvalidEventID || GetEventName(id) != 0)
	{
		CryError("Duplicate game object event (%d - %s) found", id, name);
	}

	m_eventNameToID[name] = id;
	m_eventIDToName[id] = name;
}

uint32 CGameObjectSystem::GetEventID( const char* name )
{
	if (!name)
		return InvalidEventID;

	std::map<string, uint32>::const_iterator iter = m_eventNameToID.find(CONST_TEMP_STRING(name));
	if (iter != m_eventNameToID.end())
		return iter->second;
	else
		return InvalidEventID;
}

const char* CGameObjectSystem::GetEventName( uint32 id )
{
	std::map<uint32, string>::const_iterator iter = m_eventIDToName.find(id);
	if (iter != m_eventIDToName.end())
		return iter->second.c_str();
	else
		return 0;
}


IGameObject *CGameObjectSystem::CreateGameObjectForEntity(EntityId entityId)
{
	if (IGameObject *pGameObject=CCryAction::GetCryAction()->GetGameObject(entityId))
		return pGameObject;

	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	if (pEntity)
	{
		CGameObject *pGameObject = new CGameObject();
		pEntity->SetProxy(ENTITY_PROXY_USER, pGameObject);

		SEntitySpawnParams spawnParams;
		pGameObject->Init(pEntity, spawnParams);

		return pGameObject;
	}

	return 0;
}

IGameObjectExtension * CGameObjectSystem::Instantiate( ExtensionID id, IGameObject * pObject )
{
	if (id > m_extensionInfo.size())
		return NULL;
	IGameObjectExtension * pExt = m_extensionInfo[id].pFactory->Create();
	if (!pExt)
		return NULL;
	if (m_pSpawnSerializer)
		pExt->SerializeSpawnInfo( *m_pSpawnSerializer );
	if (!pExt->Init( pObject ))
	{
		pExt->Release();
		return NULL;
	}
	return pExt;
}

/* static */
IEntityProxy * CGameObjectSystem::CreateGameObjectWithPreactivatedExtension(IEntity *pEntity, SEntitySpawnParams &params, void *pUserData)
{
	CGameObject * pGameObject = new CGameObject();
	if (!pGameObject->ActivateExtension( params.pClass->GetName() ))
	{
		pGameObject->Release();
		pGameObject = 0;
		return 0;
	}

	if (params.pUserData)
	{
		SEntitySpawnParamsForGameObjectWithPreactivatedExtension * pParams = 
			static_cast<SEntitySpawnParamsForGameObjectWithPreactivatedExtension*>(params.pUserData);
		if (!pParams->hookFunction( pEntity, pGameObject, pParams->pUserData ))
		{
			pGameObject->Release();
			pGameObject = 0;
		}
	}

	return pGameObject;
}

void CGameObjectSystem::PostUpdate( float frameTime )
{
	static std::vector<IGameObject*> objects;
	objects = m_postUpdateObjects;
	for (std::vector<IGameObject*>::const_iterator iter = objects.begin(); iter != objects.end(); ++iter)
	{
		(*iter)->PostUpdate(frameTime);
	}
}

void CGameObjectSystem::SetPostUpdate( IGameObject * pGameObject, bool enable )
{
	if (enable)
		stl::push_back_unique( m_postUpdateObjects, pGameObject );
	else
		stl::find_and_erase( m_postUpdateObjects, pGameObject );
}

const SEntitySchedulingProfiles * CGameObjectSystem::GetEntitySchedulerProfiles( IEntity * pEnt )
{
	if (!gEnv->bMultiplayer)
		return &m_defaultProfiles;

	if (pEnt->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY))
		return &m_defaultProfiles;

	std::map<string, SEntitySchedulingProfiles>::iterator iter = m_schedulingParams.find(CONST_TEMP_STRING(pEnt->GetClass()->GetName()));

	if (iter == m_schedulingParams.end())
	{
		if (gEnv->bMultiplayer)
			GameWarning("No network scheduling parameters set for entities of class '%s'", pEnt->GetClass()->GetName());
		return &m_defaultProfiles;
	}
	return &iter->second;
}

void CGameObjectSystem::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "GameObjectSystem");

	s->Add(*this);
	s->AddContainer(m_nameToID);
	s->AddContainer(m_extensionInfo);
	m_dispatch.GetMemoryStatistics(s);
	s->AddContainer(m_postUpdateObjects);
	s->AddContainer(m_schedulingParams);

	for (std::vector<SExtensionInfo>::iterator it = m_extensionInfo.begin(); it != m_extensionInfo.end(); ++it)
		s->Add(it->name);

	IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();
	while (IEntity * pEnt = pIt->Next())
	{
		CGameObject * pGO = (CGameObject *)pEnt->GetProxy(ENTITY_PROXY_USER);
		if (!pGO)
			continue;

		pGO->GetMemoryStatistics(s);
	}
}
