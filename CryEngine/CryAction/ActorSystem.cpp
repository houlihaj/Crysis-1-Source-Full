/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 23:8:2004   15:52 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ActorSystem.h"
#include "Network/GameServerChannel.h"
#include "Network/GameServerNub.h"
#include "CryPath.h"
#include "CryAction.h"

//------------------------------------------------------------------------
void CActorSystem::DemoSpectatorSystem::SwitchSpectator(TActorMap *pActors, EntityId id)
{
	if(pActors->size() <= 1) 
		return;	

	IActor *nextActor, *lastActor;

	lastActor = (*pActors)[m_currentActor];

	if(id == 0)
	{
		TActorMap::const_iterator it = pActors->begin();
		while(it->first != m_currentActor)
			++it;
		++it;
		if(it == pActors->end())
			it = pActors->begin();
		nextActor = it->second;
	}
	else
	{
		nextActor = (*pActors)[id];
		if(!nextActor)
			return;
	}

	if(nextActor == lastActor)
		return;

	m_currentActor = nextActor->GetEntityId();
	if(IView* view = gEnv->pGame->GetIGameFramework()->GetIViewSystem()->GetActiveView())
		view->LinkTo(nextActor->GetEntity());

	nextActor->SwitchDemoModeSpectator(true);
	lastActor->SwitchDemoModeSpectator(false);

	ISoundSystem * pSoundSystem = gEnv->pSoundSystem;
	if (pSoundSystem)
		pSoundSystem->SetListenerEntity( LISTENERID_STANDARD, m_currentActor );
}

//------------------------------------------------------------------------
CActorSystem::CActorSystem(ISystem *pSystem, IEntitySystem *pEntitySystem)
: m_pSystem(pSystem),
	m_pEntitySystem(pEntitySystem)
{
	m_demoPlaybackMappedOriginalServerPlayer = 0;
}

//------------------------------------------------------------------------
CActorSystem::~CActorSystem()
{
	std::for_each(m_iteratorPool.begin(), m_iteratorPool.end(), stl::container_object_deleter());
	// delete the created userdata in each class
	for (TActorClassMap::iterator it = m_classes.begin(); it != m_classes.end(); ++it)
	{
		IEntityClass *pClass = m_pEntitySystem->GetClassRegistry()->FindClass(it->first.c_str());
		if( pClass )
			delete (char*)pClass->GetUserProxyData();
	}
}

void CActorSystem::Reset()
{
	if(GetISystem()->IsSerializingFile() == 1)
	{
		TActorMap::iterator it = m_actors.begin();
		IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
		for (; it != m_actors.end(); ++it)
		{
			EntityId id = it->first;
			IEntity *pEntity = pEntitySystem->GetEntity(id);
			if(!pEntity)
			{
				m_actors.erase(it);
				--it;
			}
		}
	}
}

//------------------------------------------------------------------------
IActor *CActorSystem::GetActor(EntityId entityId)
{
	TActorMap::iterator it = m_actors.find(entityId);

	if (it != m_actors.end())
	{
		return it->second;
	}

	return 0;
}

//------------------------------------------------------------------------
IActor *CActorSystem::GetActorByChannelId(uint16 channelId)
{
	for (TActorMap::iterator it = m_actors.begin(); it != m_actors.end(); it++)
	{
		if (it->second->GetGameObject()->GetChannelId() == channelId)
		{
			return it->second;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
IActor *CActorSystem::CreateActor(uint16 channelId, const char *name, const char *actorClass, const Vec3 &pos, const Quat &rot, const Vec3 &scale, EntityId id)
{
	// get the entity class
	IEntityClass *pEntityClass = m_pEntitySystem->GetClassRegistry()->FindClass(actorClass);

	if (!pEntityClass)
	{
		assert(pEntityClass);

		return 0;
	}

	// if a channel is specified and already has a player,
	// use that entity id

	if (channelId)
	{
		if (CGameServerNub * pGameServerNub = CCryAction::GetCryAction()->GetGameServerNub())
			if (CGameServerChannel * pGameServerChannel = pGameServerNub->GetChannel(channelId))
				if (pGameServerChannel->GetNetChannel()->IsLocal())
				{
					id = LOCAL_PLAYER_ENTITY_ID;
					if(IsDemoPlayback()) //if playing a demo - spectator id is changed
						m_demoSpectatorSystem.m_currentActor = m_demoSpectatorSystem.m_originalActor = id;
				}
	}

	IGameObjectSystem::SEntitySpawnParamsForGameObjectWithPreactivatedExtension userData;
	userData.hookFunction = HookCreateActor;
	userData.pUserData = &channelId;

	SEntitySpawnParams params;
	params.id = id;
	params.pUserData = (void *)&userData;
	params.sName = name;
	params.vPosition = pos;
	params.qRotation = rot;
	params.vScale = scale;
	params.nFlags = ENTITY_FLAG_TRIGGER_AREAS;
	if (channelId)
		params.nFlags |= ENTITY_FLAG_NEVER_NETWORK_STATIC;
	params.pClass = pEntityClass;

	IEntity *pEntity = m_pEntitySystem->SpawnEntity(params, true);
	assert(pEntity);

	if (!pEntity)
	{
		return 0;
	}

	return GetActor(pEntity->GetId());
}

//------------------------------------------------------------------------
void CActorSystem::SetDemoPlaybackMappedOriginalServerPlayer(EntityId id)
{
	m_demoPlaybackMappedOriginalServerPlayer = id;
}

EntityId CActorSystem::GetDemoPlaybackMappedOriginalServerPlayer() const
{
	return m_demoPlaybackMappedOriginalServerPlayer;
}

//------------------------------------------------------------------------
void CActorSystem::SwitchDemoSpectator(EntityId id)
{
	m_demoSpectatorSystem.SwitchSpectator(&m_actors, id);
}

//------------------------------------------------------------------------
IActor* CActorSystem::GetCurrentDemoSpectator()
{
	return m_actors[m_demoSpectatorSystem.m_currentActor];
}

//------------------------------------------------------------------------
IActor* CActorSystem::GetOriginalDemoSpectator()
{
	return m_actors[m_demoSpectatorSystem.m_originalActor];
}

//------------------------------------------------------------------------
void CActorSystem::RegisterActorClass(const char *name, IGameFramework::IActorCreator * pCreator, bool isAI)
{
	IEntityClassRegistry::SEntityClassDesc actorClass;

	char scriptName[1024] = {0};
	if(!isAI)
		sprintf(scriptName, "Scripts/Entities/Actor/%s.lua", name);
	else
		sprintf(scriptName, "Scripts/Entities/AI/%s.lua", name);

	// Allow the name to contain relative path, but use only the name part as class name.
	string	className(PathUtil::GetFile(name));
	actorClass.sName = className.c_str();
	actorClass.sScriptFile = scriptName;
	if(!isAI)
		actorClass.flags |= ECLF_INVISIBLE;

	CCryAction::GetCryAction()->GetIGameObjectSystem()->RegisterExtension(className.c_str(), pCreator, &actorClass);

	m_classes.insert(TActorClassMap::value_type(name, pCreator));
}

//------------------------------------------------------------------------
void CActorSystem::AddActor(EntityId entityId, IActor *pProxy)
{
	m_actors.insert(TActorMap::value_type(entityId, pProxy));
}

//------------------------------------------------------------------------
void CActorSystem::RemoveActor(EntityId entityId)
{
	TActorMap::iterator it = m_actors.find(entityId);

	if (it != m_actors.end())
	{
		m_actors.erase(it);
	}
}

//------------------------------------------------------------------------
bool CActorSystem::HookCreateActor( IEntity *pEntity, IGameObject *pGameObject, void *pUserData )
{
	pGameObject->SetChannelId( *static_cast<uint16*>(pUserData) );
	return true;
}

//------------------------------------------------------------------------
#if 0
IEntityProxy *CActorSystem::CreateActor(IEntity *pEntity, SEntitySpawnParams &params, void *pUserData)
{
	/*
	SActorUserData *pActorUserData = reinterpret_cast<SActorUserData *>(pUserData);
	string className = pActorUserData->className;

	CActorSystem *pActorSystem = reinterpret_cast<CActorSystem *>(pActorUserData->pActorSystem);
	assert(pActorSystem);

	const SSpawnUserData * pSpawnUserData = static_cast<const SSpawnUserData *>(params.pUserData);

	TActorClassMap::iterator it = pActorSystem->m_classes.find(className);

	if (it == pActorSystem->m_classes.end())
	{
		GameWarning( "Unknown Actor class '%s'", className );

		return 0;
	}

	IActor *pActor = (*it->second)();
	CActor *pActor = new CActor(pActorSystem, className.c_str(), pEntity, pActor);
	assert(pActor);
	*/

	pActor->GetGameObject()->SetChannelId( pSpawnUserData ? pSpawnUserData->channelId : 0);
	
	//pEntity->Activate(true);

	//pActorSystem->AddActor(pEntity->GetId(), pActor);

	return pActor;
}
#endif

IActorIteratorPtr CActorSystem::CreateActorIterator()
{
	if (m_iteratorPool.empty())
	{
		return new CActorIterator(this);
	}
	else
	{
		CActorIterator* pIter = m_iteratorPool.back();
		m_iteratorPool.pop_back();
		new (pIter) CActorIterator(this);
		return pIter;
	}
}

void CActorSystem::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "ActorSystem");
	s->Add(*this);
	s->AddContainer(m_actors);
	s->AddContainer(m_classes);
	s->AddContainer(m_iteratorPool);
	for (size_t i=0; i<m_iteratorPool.size(); i++)
		s->Add(*m_iteratorPool[i]);
}
