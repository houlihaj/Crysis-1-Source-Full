#include "StdAfx.h"
#include "CET_EntitySystem.h"
#include "GameContext.h"
#include "NetHelpers.h"
#include "IGameRulesSystem.h"
#include "ILevelSystem.h"
#include "CryAction.h"
#include "AnimationGraph/AnimationGraphManager.h"
#include "IDialogSystem.h"
#include "IMaterialEffects.h"

/*
 * Reset entity system
 */

class CCET_EntitySystemReset : public CCET_Base
{
public:
	CCET_EntitySystemReset( bool skipPlayers, bool skipGameRules ) : m_skipPlayers(skipPlayers), m_skipGameRules(skipGameRules) {}
	
	const char * GetName() { return "EntitySystemReset"; }

	EContextEstablishTaskResult OnStep( SContextEstablishState& state )
	{
		CScopedRemoveObjectUnlock unlockRemovals(CCryAction::GetCryAction()->GetGameContext());
		if (m_skipGameRules || m_skipPlayers)
		{
			IEntityIt* i = gEnv->pEntitySystem->GetEntityIterator();

			while(!i->IsEnd())
			{
				IEntity* pEnt = i->Next();

				// skip gamerules
				if (m_skipGameRules)
					if (pEnt->GetId() == CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRulesEntity()->GetId())
						continue;

				// skip players
				if (m_skipPlayers)
				{
					IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pEnt->GetId());
					if (pActor && pActor->IsPlayer())
						continue;
				}

				pEnt->ClearFlags(ENTITY_FLAG_UNREMOVABLE);

				// force remove all other entities
				gEnv->pEntitySystem->RemoveEntity(pEnt->GetId(), true);
			}

			if (!m_skipGameRules)
				gEnv->pEntitySystem->ReserveEntityId(1);
		}
		else
		{
			if(!gEnv->pSystem->IsSerializingFile())
				gEnv->pEntitySystem->Reset();
			gEnv->pEntitySystem->ReserveEntityId(1);
		}
		gEnv->pEntitySystem->ReserveEntityId( LOCAL_PLAYER_ENTITY_ID );
		return eCETR_Ok;
	}

private:
	bool m_skipPlayers;
	bool m_skipGameRules;
};

void AddEntitySystemReset( IContextEstablisher * pEst, EContextViewState state, bool skipPlayers, bool skipGamerules )
{
	pEst->AddTask( state, new CCET_EntitySystemReset(skipPlayers, skipGamerules) );
}

/*
 * Random system reset
 */

class CCET_RandomSystemReset : public CCET_Base
{
public:
	const char * GetName() { return "RandomSystemReset"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		// reset a bunch of subsystems
		gEnv->p3DEngine->ResetPostEffects();
		gEnv->p3DEngine->ResetParticlesAndDecals();
		gEnv->pGame->GetIGameFramework()->ResetBrokenGameObjects();
		gEnv->pPhysicalWorld->ResetDynamicEntities();
		gEnv->pFlowSystem->Reset();
		gEnv->pGame->GetIGameFramework()->GetIItemSystem()->Reset();
		gEnv->pGame->GetIGameFramework()->GetIDialogSystem()->Reset();
		gEnv->pGame->GetIGameFramework()->GetIMaterialEffects()->Reset();

		//m_pPersistantDebug->Reset();
		return eCETR_Ok;
	}
};

void AddRandomSystemReset( IContextEstablisher * pEst, EContextViewState state )
{
	pEst->AddTask( state, new CCET_RandomSystemReset() );
}

/*
 * animation graph system reset
 */

class CCET_AnimationGraphReset : public CCET_Base
{
public:
	const char * GetName() { return "AnimationGraphReset"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		// reset a bunch of subsystems
		CCryAction::GetCryAction()->GetAnimationGraphManager()->FreeGraphs();

		//m_pPersistantDebug->Reset();
		return eCETR_Ok;
	}
};

void AddAnimationGraphReset( IContextEstablisher * pEst, EContextViewState state )
{
	pEst->AddTask( state, new CCET_AnimationGraphReset() );
}

/*
 * Fake some spawns
 */

class CCET_FakeSpawns : public CCET_Base
{
public:
	CCET_FakeSpawns(unsigned what) : m_what(what) {}

	const char * GetName() { return "FakeSpawns"; }

	EContextEstablishTaskResult OnStep( SContextEstablishState& state )
	{
		bool allowPlayers = (m_what & eFS_Players) != 0;
		bool allowGameRules = (m_what & eFS_GameRules) != 0;
		bool allowOthers = (m_what & eFS_Others) != 0;
		bool rebindAllowedGameObjects = (m_what & eFS_Opt_Rebind) != 0;

		EntityId gameRulesId = 0;
		if (IEntity * pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRulesEntity())
			gameRulesId = pGameRules->GetId();

		// we are in the editor, and that means that there have been entities spawned already
		// that are not bound to the network context... so lets bind them!
		IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();
		while (IEntity * pEntity = pIt->Next())
		{
			bool isOther = true;

			bool isPlayer = false;
			IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pEntity->GetId());
			if (pActor && pActor->IsPlayer())
			{
				isPlayer = true;
				isOther = false;
			}
			if (isPlayer && !allowPlayers)
				continue;

			bool isGameRules = false;
			if (pEntity->GetId() == gameRulesId)
			{
				isGameRules = true;
				isOther = false;
			}
			if (isGameRules && !allowGameRules)
				continue;

			if (isOther && !allowOthers)
				continue;

			CGameObject * pGO = (CGameObject *) pEntity->GetProxy( ENTITY_PROXY_USER );
			if (pGO)
			{
				if (pGO->IsBoundToNetwork())
					pGO->BindToNetwork(eBTNM_Force); // force rebinding
			}

			SEntitySpawnParams fakeParams;
			fakeParams.id = pEntity->GetId();
			fakeParams.nFlags = pEntity->GetFlags();
			fakeParams.pClass = pEntity->GetClass();
			fakeParams.qRotation = pEntity->GetRotation();
			fakeParams.sName = pEntity->GetName();
			fakeParams.vPosition = pEntity->GetPos();
			fakeParams.vScale = pEntity->GetScale();
			CCryAction::GetCryAction()->GetGameContext()->OnSpawn( pEntity, fakeParams );
		}
		return eCETR_Ok;
	}

private:
	unsigned m_what;
};

void AddFakeSpawn( IContextEstablisher * pEst, EContextViewState state, unsigned what )
{
	pEst->AddTask(state, new CCET_FakeSpawns(what));
}

/*
 * load entities from the mission file
 */

class CCET_LoadLevelEntities : public CCET_Base
{
public:
	const char * GetName() { return "LoadLevelEntities"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		ILevel*			pLevel = CCryAction::GetCryAction()->GetILevelSystem()->GetCurrentLevel();
		if (!pLevel)
		{
			GameWarning("level is null");
			return eCETR_Failed;
		}
		const char * levelName = CCryAction::GetCryAction()->GetLevelName();
		if (!levelName)
		{
			GameWarning("levelName is null");
			return eCETR_Failed;
		}
		ILevelInfo* pLevelInfo = CCryAction::GetCryAction()->GetILevelSystem()->GetLevelInfo(levelName);
		if (!pLevelInfo)
		{
			GameWarning("levelInfo is null");
			return eCETR_Failed;
		}


		// delete any pending entities before reserving EntityIds in LoadEntities()
		gEnv->pEntitySystem->DeletePendingEntities();

		string missionXml = pLevelInfo->GetDefaultGameType()->xmlFile;
		string xmlFile = string(pLevelInfo->GetPath()) + "/" + missionXml;

		XmlNodeRef rootNode = GetISystem()->LoadXmlFile(xmlFile.c_str());

		if (rootNode)
		{
			const char *script = rootNode->getAttr("Script");

			if (script && script[0])
				gEnv->pScriptSystem->ExecuteFile(script, true, true);

			XmlNodeRef objectsNode = rootNode->findChild("Objects");

			if (objectsNode)
				gEnv->pEntitySystem->LoadEntities(objectsNode);
		}
		else
			return eCETR_Failed;
		return eCETR_Ok;
	}
};
	
void AddLoadLevelEntities( IContextEstablisher * pEst, EContextViewState state )
{
	pEst->AddTask( state, new CCET_LoadLevelEntities );
}

/*
 * send an event
 */

class CCET_EntitySystemEvent : public CCET_Base, private SEntityEvent
{
public:
	CCET_EntitySystemEvent( const SEntityEvent& evt ) : SEntityEvent(evt) {}

	const char * GetName() { return "EntitySystemEvent"; }

	EContextEstablishTaskResult OnStep( SContextEstablishState& state )
	{
		gEnv->pEntitySystem->SendEventToAll(*this);
		return eCETR_Ok;
	}
};

void AddEntitySystemEvent( IContextEstablisher * pEst, EContextViewState state, const SEntityEvent& evt )
{
	pEst->AddTask( state, new CCET_EntitySystemEvent(evt) );
}

/*
 * register sound listener
 */

class CCET_RegisterSoundListener : public CCET_Base
{
public:
	const char * GetName() { return "RegisterSoundListener"; }

	EContextEstablishTaskResult OnStep( SContextEstablishState& state )
	{
		IActor * pActor = CCryAction::GetCryAction()->GetClientActor();
		if (!pActor)
			return eCETR_Failed;
		ISoundSystem * pSoundSystem = gEnv->pSoundSystem;
		if (pSoundSystem)
			pSoundSystem->SetListenerEntity( LISTENERID_STANDARD, pActor->GetEntityId() );
		return eCETR_Ok;
	}
};

void AddRegisterSoundListener( IContextEstablisher * pEst, EContextViewState state )
{
	pEst->AddTask( state, new CCET_RegisterSoundListener );
}
