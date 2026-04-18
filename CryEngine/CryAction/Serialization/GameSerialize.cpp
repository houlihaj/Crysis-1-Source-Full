#include "stdafx.h"
#include "GameSerialize.h"
#include "CryAction.h"

#include "IGameTokens.h"
#include "ILevelSystem.h"
#include "IActorSystem.h"
#include "IItemSystem.h"
#include "IGameRulesSystem.h"
#include "IVehicleSystem.h"
#include "ISound.h"
#include "IMusicSystem.h"
#include "IMovieSystem.h"
#include "DialogSystem/DialogSystem.h"
#include "MaterialEffects/MaterialEffects.h"
#include "Network/GameContext.h"
#include "Network/GameServerNub.h"

#include "CopyProtection.h"

#include <queue>
#include <set>
#include <time.h>
#include <ITimer.h>

#define ENABLE_QUICK_LOAD 1

//#define EXCESSIVE_ENTITY_DEBUG 1
//#undef  EXCESSIVE_ENTITY_DEBUG

static const char * SAVEGAME_ROOT_SECTION = "Game";
static const char * SAVEGAME_CONSOLEVARS_SECTION = "ConsoleVars";
static const char * SAVEGAME_GAMESTATE_SECTION = "GameState";
static const char * SAVEGAME_AISTATE_SECTION = "AIState";
static const char * SAVEGAME_ENTITYDATA_SECTION = "Entity";
static const char * SAVEGAME_SPECIAL_SECTION = "Class";
static const char * SAVEGAME_SCRIPT_SECTION = "Script";
static const char * SAVEGAME_GAMETOKEN_SECTION = "GameTokens";
static const char * SAVEGAME_TERRAINSTATE_SECTION = "TerrainState";
static const char * SAVEGAME_TIMER_SECTION = "Timer";
static const char * SAVEGAME_DIALOGSYSTEM_SECTION = "DialogSystem";
static const char * SAVEGAME_SOUNDSYSTEM_SECTION = "SoundSystem";
static const char * SAVEGAME_MUSICSYSTEM_SECTION = "MusicSystem";
static const char * SAVEGAME_MUSICLOGIC_SECTION = "MusicLogic";
static const char * SAVEGAME_LTLINVENTORY_SECTION = "LTLInventory";
static const char * SAVEGAME_VIEWSYSTEM_SECTION = "ViewSystem";
static const char * SAVEGAME_MATERIALEFFECTS_SECTION = "MatFX";
static const char * SAVEGAME_BUILD_TAG = "build";
static const char * SAVEGAME_TIME_TAG = "saveTime";
static const char * SAVEGAME_VERSION_TAG = "version";
static const char * SAVEGAME_LEVEL_TAG = "level";
static const char * SAVEGAME_GAMERULES_TAG = "gameRules";
static const char * SAVEGAME_ENTITYCOUNT_TAG = "entityCount";
static const char * SAVEGAME_ID_TAG = "id";
static const char * SAVEGAME_NAME_TAG = "name";
static const char * SAVEGAME_POS_TAG = "pos";
static const char * SAVEGAME_ROT_TAG = "rot";
static const char * SAVEGAME_SCALE_TAG = "scale";
static const char * SAVEGAME_CLASS_TAG = "class";
static const char * SAVEGAME_FLAGS_TAG = "flags";
static const char * SAVEGAME_UPDATEPOLICY_TAG = "updatePolicy";
static const char * SAVEGAME_ISHIDDEN_TAG = "isHidden";
static const char * SAVEGAME_PARENT_TAG = "parent";
static const char * SAVEGAME_CHECKPOINT_TAG = "checkPointName";

static const char * SAVEGAME_DEFINITIONFILE_VALUE = "definitionFile";

static const int SAVEGAME_VERSION_VALUE = 14;

#if 1 || defined(USER_alexl)
#define CHECKPOINT_OUTPUT true
#else
#define CHECKPOINT_OUTPUT false
#endif

// Some Helpers
// Checkpoint for easy section timing
struct Checkpoint
{
	// if msg is given (const char ptr!), auto-message on d'tor
	Checkpoint(bool bOutput, const char* msg=0) : m_bOutput(bOutput), m_szMsg(msg)
	{
		Start();
	}

	~Checkpoint()
	{
		if (m_szMsg)
		{
			Check(m_szMsg);
		}
	}
	void Start()
	{
		m_startTime = gEnv->pTimer->GetAsyncTime();
	}

	CTimeValue End()
	{
		return gEnv->pTimer->GetAsyncTime() - m_startTime;
	}
	CTimeValue Check(const char* msg)
	{
		CTimeValue elapsed = End();
		if (m_bOutput)
			CryLog("Checkpoint %12s : %.4f ms", msg, elapsed.GetMilliSeconds());
		Start();
		return elapsed;
	}
	CTimeValue  m_startTime;
	bool        m_bOutput;
	const char* m_szMsg;
};

// Safe pausing of the CTimer
// pause the timer
// if it had been already paused before it will not be unpaused
struct SPauseGameTimer
{
	SPauseGameTimer()
	{
		m_bGameTimerWasPaused = gEnv->pTimer->IsTimerPaused(ITimer::ETIMER_GAME);
		gEnv->pTimer->PauseTimer(ITimer::ETIMER_GAME, true);
	}

	~SPauseGameTimer()
	{
		if (m_bGameTimerWasPaused == false)
			gEnv->pTimer->PauseTimer(ITimer::ETIMER_GAME, false);
	}

	bool m_bGameTimerWasPaused;
};


class CConsoleVarSerialize : public ICVarDumpSink
{
public:
	CConsoleVarSerialize( ISaveGame * pSaveGame ) : m_pSaveGame(pSaveGame) {}

	virtual void OnElementFound( ICVar *pCVar )
	{
		m_pSaveGame->AddConsoleVariable( pCVar );
	}

private:
	ISaveGame * m_pSaveGame;
};

class CConsoleVarDeserialize : public ICVarDumpSink
{
public:
	CConsoleVarDeserialize( ILoadGame * pLoadGame ) : m_pLoadGame(pLoadGame) {}

	virtual void OnElementFound( ICVar *pCVar )
	{
		m_pLoadGame->GetConsoleVariable( pCVar );
	}

private:
	ILoadGame * m_pLoadGame;
};

void CGameSerialize::RegisterSaveGameFactory( const char * name, SaveGameFactory factory )
{
	m_saveGameFactories[name] = factory;
}

void CGameSerialize::RegisterLoadGameFactory( const char * name, LoadGameFactory factory )
{
	m_loadGameFactories[name] = factory;
}

struct SBasicEntityData
{
	enum ExtraFlags
	{
		FLAG_HIDDEN    = BIT(0),
		FLAG_ACTIVE    = BIT(1),
		FLAG_INVISIBLE = BIT(2),
	};
	SBasicEntityData()
	: pos(0,0,0),
		rot(1,0,0,0),
		scale(1,1,1),
		flags(0),
		updatePolicy(0),
		iPhysType(0),
		parentEntity(0),
		isHidden(false),
		isActive(false),
		isInvisible(false)
	{}
	void Serialize( TSerialize ser )
	{
		uint32 flags2 = 0;
		if (isHidden) flags2 |= FLAG_HIDDEN; else flags2 &= ~FLAG_HIDDEN;
		if (isActive) flags2 |= FLAG_ACTIVE; else flags2 &= ~FLAG_ACTIVE;
		if (isInvisible) flags2 |= FLAG_INVISIBLE; else flags2 &= ~FLAG_INVISIBLE;
		flags2 |= (updatePolicy&0xF) << (32-4);
		flags2 |= (iPhysType&0xF) << (32-8);

		bool bLoading = ser.IsReading();
		
		ser.Value("id", id);
		ser.Value("name", name);
		ser.Value("class", className);

		ser.Value("archetype",archetype);
		ser.Value("pos", pos );
		if (ser.IsReading() || !rot.IsIdentity())
			ser.Value("rot", rot );
		if (ser.IsReading() || !scale.IsEquivalent(Vec3(1,1,1)))
			ser.Value("scl", scale );
		ser.Value("flags", flags );
		ser.Value("flags2", flags2 );
		ser.Value("parent", parentEntity );

		//ser.Value("hidden", isHidden );
		//ser.Value("active", isActive );
		//ser.Value("invisible", isInvisible );

		iPhysType = (flags2 >> (32-8)) & 0xF;
		updatePolicy = (flags2 >> (32-4)) & 0xF;
		isHidden = (flags2 & FLAG_HIDDEN) != 0;
		isActive = (flags2 & FLAG_ACTIVE) != 0;
		isInvisible = (flags2 & FLAG_INVISIBLE) != 0;
		if (scale.x == 0 && scale.y == 0 && scale.z == 0)
			scale = Vec3(1,1,1);
	}

	EntityId id;
	string name;
	Vec3 pos;
	Quat rot;
	Vec3 scale;
	uint32 flags;
	uint32 updatePolicy;
	string className;
	string archetype;
	EntityId parentEntity;
	int iPhysType;
	bool isHidden;
	bool isActive;
	bool isInvisible;
};

//////////////////////////////////////////////////////////////////////////
void CGameSerialize::Clean()
{
	m_savedEntityIds.clear();
	gEnv->pEntitySystem->DeletePendingEntities();
}


bool CGameSerialize::SaveGame( CCryAction * pCryAction, const char * method, const char * name, ESaveGameReason reason, const char* checkPointName)
{
	if (reason == eSGR_FlowGraph)
	{
		if (pCryAction->IsInTimeDemo())

			return true; // Ignore checkpoint saving when running time demo
	}


	Checkpoint total(CHECKPOINT_OUTPUT, "TotalTime");
	Checkpoint checkpoint(CHECKPOINT_OUTPUT);

	Clean();
	checkpoint.Check("Clean");

	// initialize serialization method
	struct Local
	{
		static ISaveGame * ReturnNull()
		{
			return NULL;
		}
	};
	class CSaveGameHolder
	{
	public:
		CSaveGameHolder( ISaveGame * pSaveGame ) : m_pSaveGame(pSaveGame) {}
		~CSaveGameHolder()
		{
			if (m_pSaveGame)
				m_pSaveGame->Complete(false);
		}

		bool PointerOk()
		{
			return m_pSaveGame != 0;
		}

		bool Complete()
		{
			if (!m_pSaveGame)
				return false;
			bool ok = m_pSaveGame->Complete(true);
			m_pSaveGame = 0;
			return ok;
		}

		ISaveGame * operator->() const
		{
			return m_pSaveGame;
		}

		ISaveGame * Get() const
		{
			return m_pSaveGame;
		}

	private:
		ISaveGame * m_pSaveGame;
	};
	CSaveGameHolder pSaveGame( stl::find_in_map(m_saveGameFactories, method, Local::ReturnNull)() );
	if (!pSaveGame.PointerOk())
	{
		GameWarning("Invalid serialization method: %s", method);
		return false;
	}
	// TODO: figure correct path?
	if (!pSaveGame->Init(name))
	{
		GameWarning("Unable to save to %s", name);
		return false;
	}

	checkpoint.Check("Init");

	// verify that there's a game active
	const char * levelName = pCryAction->GetLevelName();
	if (!levelName)
	{
		GameWarning("No game active - cannot save");
		return false;
	}

	gEnv->pSystem->SetThreadState(ESubsys_Physics, false);

	// save serialization version
	pSaveGame->AddMetadata( SAVEGAME_VERSION_TAG, SAVEGAME_VERSION_VALUE );
	// save current level and game rules
	pSaveGame->AddMetadata( SAVEGAME_LEVEL_TAG, levelName );
	pSaveGame->AddMetadata( SAVEGAME_GAMERULES_TAG, pCryAction->GetIGameRulesSystem()->GetCurrentGameRulesEntity()->GetClass()->GetName() );
	// save some useful information for debugging - should not be relied upon in loading
	const SFileVersion& fileVersion = GetISystem()->GetFileVersion();
	char tmpbuf[128];
	fileVersion.ToString(tmpbuf);
	pSaveGame->AddMetadata( SAVEGAME_BUILD_TAG, tmpbuf );
	int bitSize = sizeof(char*) * 8;
	pSaveGame->AddMetadata( "Bit", bitSize);

	//add checkpoint name if available
	if(checkPointName)
		pSaveGame->AddMetadata( SAVEGAME_CHECKPOINT_TAG, checkPointName );
	else
		pSaveGame->AddMetadata( SAVEGAME_CHECKPOINT_TAG, "" );

	string timeString;
	GameUtils::timeToString(time(NULL), timeString);
	pSaveGame->AddMetadata( SAVEGAME_TIME_TAG, timeString.c_str());

	// save console variables
	{
		CConsoleVarSerialize consoleVarSerialize( pSaveGame.Get() );
		gEnv->pConsole->DumpCVars( &consoleVarSerialize, VF_SAVEGAME );
	}
	pSaveGame->SetSaveGameReason(reason);

	// call CryAction's Framework listeners
	pCryAction->NotifyGameFrameworkListeners(pSaveGame.Get());

	checkpoint.Start();

	// timer data
	gEnv->pTimer->Serialize(pSaveGame->AddSection(SAVEGAME_TIMER_SECTION));
	checkpoint.Check("Timer");

	// terrain modifications (e.g. heightmap changes)
	gEnv->p3DEngine->SerializeState(pSaveGame->AddSection(SAVEGAME_TERRAINSTATE_SECTION));
	checkpoint.Check("3DEngine");
	// game tokens
	pCryAction->GetIGameTokenSystem()->Serialize( pSaveGame->AddSection(SAVEGAME_GAMETOKEN_SECTION) );
	checkpoint.Check("GameToken");
	// view system
	pCryAction->GetIViewSystem()->Serialize(pSaveGame->AddSection(SAVEGAME_VIEWSYSTEM_SECTION));
	checkpoint.Check("ViewSystem");
	// ai system data
	gEnv->pAISystem->Serialize(pSaveGame->AddSection(SAVEGAME_AISTATE_SECTION));
	checkpoint.Check("AISystem");
	
	// SoundSystem data
	if (gEnv->pSoundSystem)
		gEnv->pSoundSystem->Serialize(pSaveGame->AddSection(SAVEGAME_SOUNDSYSTEM_SECTION));
	checkpoint.Check("SoundSystem");
	
	// MusicSystem data
  if (gEnv->pMusicSystem)
	  gEnv->pMusicSystem->Serialize(pSaveGame->AddSection(SAVEGAME_MUSICSYSTEM_SECTION));
	checkpoint.Check("MusicSystem");

	// MusicLogic data
	if (pCryAction->GetMusicLogic())
		pCryAction->GetMusicLogic()->Serialize(pSaveGame->AddSection(SAVEGAME_MUSICLOGIC_SECTION));
	checkpoint.Check("MusicLogic");

	//itemsystem - LTL inventory only
	if(pCryAction->GetIItemSystem())
		pCryAction->GetIItemSystem()->Serialize( pSaveGame->AddSection(SAVEGAME_LTLINVENTORY_SECTION) );
	checkpoint.Check("Inventory");


	CMaterialEffects* pMatFX = static_cast<CMaterialEffects*> (pCryAction->GetIMaterialEffects());
	if (pMatFX)
		pMatFX->Serialize(pSaveGame->AddSection(SAVEGAME_MATERIALEFFECTS_SECTION));
	checkpoint.Check("MaterialFX");

	CDialogSystem* pDS = pCryAction->GetDialogSystem();
	if (pDS)
		pDS->Serialize(pSaveGame->AddSection(SAVEGAME_DIALOGSYSTEM_SECTION));
	checkpoint.Check("DialogSystem");

	// now the primary game stuff
	{
		TSerialize gameState = pSaveGame->AddSection(SAVEGAME_GAMESTATE_SECTION);

		IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();

		SmartScriptTable persistedScriptData(gEnv->pScriptSystem);
		std::set<IEntity*> savedEntities;
		std::queue<IEntity*> pendingEntities;

		while (!pIt->IsEnd())
		{
			IEntity * pEntity = pIt->Next();

			if (!pEntity)
			{
				GameWarning( "Null entity encountered during save" );
				continue;
			}
			//if (pEntity->IsGarbage()) //will cause zombie enemies now, because AI is serializing them, game not
			//{
				// GameWarning("Skipping Entity %d '%s' '%s' Garbage", pEntity->GetId(), pEntity->GetName(), pEntity->GetClass()->GetName());
				//continue;
			//}
			uint32 flags = pEntity->GetFlags();
			if (flags & ENTITY_FLAG_NO_SAVE)
			{
				// GameWarning("Skipping Entity %d '%s' '%s' NoSave", pEntity->GetId(), pEntity->GetName(), pEntity->GetClass()->GetName());
				continue;	
			}

			pendingEntities.push(pEntity);
		}

		uint32 entityCount = pendingEntities.size();
		gameState.Value(SAVEGAME_ENTITYCOUNT_TAG, entityCount);

		size_t skipCount = 0;

		std::vector<SBasicEntityData> basicEntityData;
		
		m_savedEntityIds.resize(0);

		while (!pendingEntities.empty() && skipCount < pendingEntities.size())
		{
			IEntity * pEntity = pendingEntities.front();

			// check parenting
			pendingEntities.pop();
			IEntity * pParentEntity = pEntity->GetParent();
			if (pParentEntity)
			{
				if (savedEntities.find(pParentEntity) == savedEntities.end())
				{
					// parent not saved yet, keep this one for later
					skipCount ++;
					pendingEntities.push(pEntity);
					continue;
				}
			}
			skipCount = 0;

			// check no-save flag
			uint32 flags = pEntity->GetFlags();
			if (flags & ENTITY_FLAG_NO_SAVE)
			{
				// assert (false); // case should be handled above already!
				// GameWarning("Skipping Entity %d '%s' '%s' No save", pEntity->GetId(), pEntity->GetName(), pEntity->GetClass()->GetName());
				;
			}
			else
			{
				// basic entity data
				SBasicEntityData bed;
				bed.id = pEntity->GetId();
				bed.name = pEntity->GetName();
				if (pEntity->GetArchetype())
					bed.archetype = pEntity->GetArchetype()->GetName();
				IPhysicalEntity *pPhysEnt;
				pe_status_awake sa;
				pe_status_pos sp;
				// for active rigid bodies, query the most recent position from the physics, since due to multithreading
				// CEntity position might be lagging behind
				if (!pEntity->GetParent() && (pPhysEnt=pEntity->GetPhysics()) && pPhysEnt->GetType()==PE_RIGID && 
						pPhysEnt->GetStatus(&sa) && pPhysEnt->GetStatus(&sp)) 
				{
					bed.pos = sp.pos;
					bed.rot = sp.q;
				}	else
				{
					bed.pos = pEntity->GetPos();
					bed.rot = pEntity->GetRotation();
				}
				bed.scale = pEntity->GetScale();
				bed.flags = flags;
				bed.updatePolicy = (uint32)pEntity->GetUpdatePolicy();
				bed.isHidden = pEntity->IsHidden();
				bed.isActive = pEntity->IsActive();
				bed.isInvisible = pEntity->IsInvisible();
				bed.className = pEntity->GetClass()->GetName();
				bed.parentEntity = pParentEntity? pParentEntity->GetId() : 0;
				bed.iPhysType = pEntity->GetPhysics() ? pEntity->GetPhysics()->GetType() : PE_NONE;
				basicEntityData.push_back(bed);
				m_savedEntityIds.push_back(bed.id);

				// GameWarning("Saving Entity %d '%s' '%s'", pEntity->GetId(), pEntity->GetName(), pEntity->GetClass()->GetName());
				savedEntities.insert( pEntity );
			}
		}
		if (!pendingEntities.empty())
		{
			GameWarning("Not all entity parents could be resolved (%d orphans): save failed", pendingEntities.size());
			while (pendingEntities.empty() == false)
			{
				IEntity * pEntity = pendingEntities.front();
				pendingEntities.pop();
				GameWarning("Entity: Id=%d Name='%s'", pEntity->GetId(), pEntity->GetName()/*, pEntity->GetClass()->GetName()*/);
			}
			gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
			return false;
		}

		checkpoint.Check("EntityPrep");

		int nNumSavedEntities = basicEntityData.size();
	 
		// Store entity ids sorted in a file.
		std::sort( m_savedEntityIds.begin(),m_savedEntityIds.end() );

		gameState.Value("EntityIDList", m_savedEntityIds);
		gameState.BeginGroup( "BasicEntityData" );
		gameState.Value( "EntityCount",nNumSavedEntities );
		//////////////////////////////////////////////////////////////////////////
		// Serialize Basic Entity Data.
		//////////////////////////////////////////////////////////////////////////
		for (int i = 0; i < nNumSavedEntities; i++)
		{
			gameState.BeginGroup("Entity");
			basicEntityData[i].Serialize( gameState );
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(basicEntityData[i].id);
			if (pEntity)
			{
				// Serialize Entity properties.
				pEntity->Serialize( gameState, ENTITY_SERIALIZE_PROPERTIES );
			}
			gameState.EndGroup();
		}
		gameState.EndGroup(); // ser.BeginGroup( "BasicEntityData" );

		checkpoint.Check("EntitySystem2");

		// Must be serialized after basic entity data.
		pCryAction->Serialize(gameState); //breakable objects ..

		checkpoint.Check("Breakables");

		gameState.BeginGroup("ExtraEntityData");
		for (std::vector<SBasicEntityData>::const_iterator iter = basicEntityData.begin(); iter != basicEntityData.end(); ++iter)
		{
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(iter->id);
			assert(pEntity);

			// c++ entity data
			gameState.BeginGroup("Entity");
			EntityId temp = iter->id;
			gameState.Value("id", temp);

			if (iter->flags&ENTITY_FLAG_MODIFIED_BY_PHYSICS)
			{
				gameState.BeginGroup("EntityGeometry");
				pEntity->Serialize( gameState, ENTITY_SERIALIZE_GEOMETRIES );
				gameState.EndGroup();
			}

			pEntity->Serialize( gameState, ENTITY_SERIALIZE_PROXIES );

			gameState.EndGroup();//Entity
		}

		gameState.EndGroup();//ExtraEntityData

		checkpoint.Check("ExtraEntity");

		//EntitySystem
		gEnv->pEntitySystem->Serialize( gameState );

		checkpoint.Check("EntitySystem");

		// 3DEngine
		gEnv->p3DEngine->PostSerialize(false);

		checkpoint.Check("3DEnginePost");

	}
	gEnv->pSystem->SetThreadState(ESubsys_Physics, true);

	checkpoint.Check("Clean2");

	Clean();


	// final serialization
	const bool bResult = pSaveGame.Complete();
	checkpoint.Check("Writing");
	return bResult;
}

// helper function to position/rotate/etc.. all entities that currently
// exist in the entity system based on current game state
bool CGameSerialize::RepositionEntities( const std::vector<SBasicEntityData>& basicEntityData, bool insistOnAllEntitiesBeingThere )
{
	bool bErrors = false;
	IEntitySystem * pEntitySystem = gEnv->pEntitySystem;

	for (std::vector<SBasicEntityData>::const_iterator iter = basicEntityData.begin(); iter != basicEntityData.end(); ++iter)
	{
		IEntity * pEntity = pEntitySystem->GetEntity(iter->id);
		if (pEntity)
		{
			if (iter->parentEntity)
			{
				IEntity * pParentEntity = pEntitySystem->GetEntity(iter->parentEntity);
				if (!pParentEntity && insistOnAllEntitiesBeingThere)
				{
					GameWarning("[LoadGame] Missing Entity with ID=%d, probably was removed during Serialization.", iter->parentEntity);
					bErrors = true;
					continue;
				}
				else if (pParentEntity)
				{
					IEntity * pCurrentParentEntity = pEntity->GetParent();
					if (pCurrentParentEntity != pParentEntity)
					{
						if (pCurrentParentEntity)
							pEntity->DetachThis();
						pParentEntity->AttachChild(pEntity);
					}
				}
			}
			else
			{
				if (pEntity->GetParent())
					pEntity->DetachThis();
			}
			pEntity->SetPosRotScale( iter->pos, iter->rot, iter->scale );
		}
		else if (insistOnAllEntitiesBeingThere)
		{
			GameWarning("[LoadGame] Missing Entity ID=%d", iter->id);
			bErrors = true;
		}
	}

	return !bErrors;
}

//////////////////////////////////////////////////////////////////////////
void CGameSerialize::ReserveEntityIds()
{
	//////////////////////////////////////////////////////////////////////////
	// Reserve id for local player.
	//////////////////////////////////////////////////////////////////////////
	gEnv->pEntitySystem->ReserveEntityId( LOCAL_PLAYER_ENTITY_ID );

	//////////////////////////////////////////////////////////////////////////
	for (std::vector<EntityId>::const_iterator iter = m_savedEntityIds.begin(); iter != m_savedEntityIds.end(); ++iter)
	{
		const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*iter);
		if (!pEntity)
		{
			// for existing entities this will set the salt to 0!
			gEnv->pEntitySystem->ReserveEntityId(*iter);
#ifdef EXCESSIVE_ENTITY_DEBUG
			EntityId id = *iter;
			CryLogAlways("> ReserveEntityId: ID=%d HexID=%X", id,id );
#endif
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameSerialize::FlushActivatableGameObjectExtensions()
{
	IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();

	pIt->MoveFirst();
	while (!pIt->IsEnd())
	{
		const IEntity * pEntity = pIt->Next();
		CGameObject * pGameObject = (CGameObject *) pEntity->GetProxy(ENTITY_PROXY_USER);
		if (pGameObject)
			pGameObject->FlushActivatableExtensions();
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameSerialize::DeleteDynamicEntities()
{
	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;

#ifdef EXCESSIVE_ENTITY_DEBUG
	// dump entities which can potentially be re-used (if class matches)
	CryLogAlways("*QL -----------------------------------------");
	CryLogAlways("*QL DeleteDynamicEntities");
#endif


	IEntityItPtr pIt = pEntitySystem->GetEntityIterator();
	//////////////////////////////////////////////////////////////////////////
	// Delete all entities except the unremovable ones and the local player.
	pIt->MoveFirst();
	while (!pIt->IsEnd())
	{
		IEntity * pEntity = pIt->Next();
		uint32 nEntityFlags = pEntity->GetFlags();

		// Local player must not be deleted.
		if (nEntityFlags & ENTITY_FLAG_LOCAL_PLAYER)
			continue;

		if (nEntityFlags & ENTITY_FLAG_UNREMOVABLE)
		{

#ifdef EXCESSIVE_ENTITY_DEBUG
			CryLogAlways(">Unremovable Entity ID=%d Name='%s'", pEntity->GetId(), pEntity->GetEntityTextDescription() );
#endif

			if (pEntity->IsGarbage() && (stl::binary_find(m_savedEntityIds.begin(),m_savedEntityIds.end(),pEntity->GetId() ) != m_savedEntityIds.end()))
			{
				// Restore disabled, un-removable entity.
				SEntityEvent event; 
				event.event = ENTITY_EVENT_INIT;
				pEntity->SendEvent(event);
			}
		}
		else
		{
#ifdef EXCESSIVE_ENTITY_DEBUG
			CryLogAlways(">Removing Entity ID=%d Name='%s'", pEntity->GetId(), pEntity->GetEntityTextDescription() );
#endif

			pEntitySystem->RemoveEntity( pEntity->GetId() );
		}
	}
	// Force deletion of removed entities.
	pEntitySystem->DeletePendingEntities();
	//////////////////////////////////////////////////////////////////////////

#ifdef EXCESSIVE_ENTITY_DEBUG
	// dump entities which can potentially be re-used (if class matches)
	CryLogAlways("*QL DeleteDynamicEntities Done");
	CryLogAlways("*QL -----------------------------------------");
#endif
}

//////////////////////////////////////////////////////////////////////////
void CGameSerialize::DumpEntities()
{
	IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();
	pIt->MoveFirst();
	while (!pIt->IsEnd())
	{
		IEntity * pEntity = pIt->Next();
		if (pEntity)
			CryLogAlways("ID=%d Name='%s'", pEntity->GetId(),pEntity->GetEntityTextDescription() );
		else
			CryLogAlways("Invalid NULL entity encountered.");
	}
}

struct STempAutoResourcesLock
{
	STempAutoResourcesLock()
	{
		gEnv->p3DEngine->LockCGFResources();
		gEnv->pCharacterManager->LockResources();
	}

	~STempAutoResourcesLock()
	{
		gEnv->pCharacterManager->UnlockResources();
		gEnv->p3DEngine->UnlockCGFResources();
	}

};


// TODO: split into 37 functions and maybe some separate classes...
ELoadGameResult CGameSerialize::LoadGame( CCryAction * pCryAction, const char * method, const char * path, SGameStartParams& params, bool requireQuickLoad, bool loadingSaveGame)
{
	Checkpoint total(CHECKPOINT_OUTPUT, "TotalTime");
	Checkpoint checkpoint(CHECKPOINT_OUTPUT);

	Clean();
	checkpoint.Check("Clean");

	bool bLoadingErrors = false; // True if any loading errors.

	// initialize serialization method
	struct Local
	{
		static ILoadGame * ReturnNull()
		{
			return NULL;
		}
	};
	class CLoadGameHolder
	{
	public:
		CLoadGameHolder( ILoadGame * pLoadGame ) : m_pLoadGame(pLoadGame) {}
		~CLoadGameHolder()
		{
			if (m_pLoadGame)
				m_pLoadGame->Complete();
		}

		bool PointerOk()
		{
			return m_pLoadGame != 0;
		}

		ILoadGame * operator->() const
		{
			return m_pLoadGame;
		}

		ILoadGame * Get() const
		{
			return m_pLoadGame;
		}

	private:
		ILoadGame * m_pLoadGame;
	};

	//////////////////////////////////////////////////////////////////////////
	// Lock geometries to not be released during game loading.
	STempAutoResourcesLock autoResourcesLock;
	//////////////////////////////////////////////////////////////////////////

	ELoadGameResult failure = eLGR_Failed;

	gEnv->p3DEngine->ResetPostEffects(true);

	CLoadGameHolder pLoadGame( stl::find_in_map(m_loadGameFactories, method, Local::ReturnNull)() );
	if (!pLoadGame.PointerOk())
	{
		GameWarning("Invalid serialization method: %s", method);
		return failure;
	}
	// TODO: figure correct path?
	if (!pLoadGame->Init(path))
	{
		GameWarning("Unable to Load to %s", path);
		return failure;
	}

	checkpoint.Check("Loading&Uncompress");

	gEnv->pSystem->SetThreadState(ESubsys_Physics, false);


	// editor always requires quickload
	requireQuickLoad |= GetISystem()->IsEditor();

#if !ENABLE_QUICK_LOAD
	if (requireQuickLoad)
	{
		GameWarning("Quick-load not currently enabled");
		return failure;
	}
#endif

	// sanity checks - version, node names, etc
	/*
	if (0 != strcmp(rootNode->getTag(), SAVEGAME_ROOT_SECTION))
	{
	GameWarning( "Not a save game file: %s", path );
	return false;
	}
	*/
	int version = -1;
	if (!pLoadGame->GetMetadata(SAVEGAME_VERSION_TAG, version) || version != SAVEGAME_VERSION_VALUE)
	{
		GameWarning( "Invalid save game version in %s; expecting %d got %d", path, SAVEGAME_VERSION_VALUE, version );
		gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
		return failure;
	}

#if ENABLE_QUICK_LOAD
	const char* levelName = pLoadGame->GetMetadata( SAVEGAME_LEVEL_TAG );
	const char * currentLevel = pCryAction->GetLevelName();
	if (currentLevel)
	{
		if (stricmp(currentLevel, levelName) != 0 && requireQuickLoad)
		{
			GameWarning("Unable to quick load: different level names (%s != %s)",
				currentLevel, levelName);
			gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
			return failure;
		}
	}
#endif

	//tell existing entities that serialization starts
	SEntityEvent event(ENTITY_EVENT_PRE_SERIALIZE);
	gEnv->pEntitySystem->SendEventToAll(event);

	checkpoint.Check("PreSerialize Event");

	std::auto_ptr<TSerialize> pSer;

	// load timer data
	std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_TIMER_SECTION));
	pSer = temp;

	if (pSer.get())
		gEnv->pTimer->Serialize(*pSer);
	else
	{
		GameWarning("Unable to find timer");
		gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
		return failure;
	}

	// load console variables
	{
		CConsoleVarDeserialize consoleVarSerialize( pLoadGame.Get() );
		gEnv->pConsole->DumpCVars( &consoleVarSerialize, VF_SAVEGAME );
	}

	// call CryAction's Framework listeners
	pCryAction->NotifyGameFrameworkListeners(pLoadGame.Get());

	checkpoint.Check("FrameWork Listeners");

	// ai system data
	// let's reset first
	gEnv->pAISystem->FlushSystem();

	checkpoint.Check("AIFlush");

	// reset movie system, don't play any sequences
	if (gEnv->pMovieSystem)
		gEnv->pMovieSystem->Reset(false,false);

	checkpoint.Check("MovieSystem");

	// reset areas (who entered who caches get flushed)
	gEnv->pEntitySystem->ResetAreas();

	// basic game state loading... need to do it early to reserve entity id's
	{
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_GAMESTATE_SECTION));
		pSer = temp;
	}
	if (!pSer.get()) {
		gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
		return failure;
	}
	IEntitySystem * pEntitySystem = gEnv->pEntitySystem;
	IEntityItPtr pIt = pEntitySystem->GetEntityIterator();

	pSer->Value("EntityIDList", m_savedEntityIds);
	
	// Entity ids vector must always be sorted, binary search is used on it.
	std::sort( m_savedEntityIds.begin(),m_savedEntityIds.end() );

	checkpoint.Check("EntityIDSort");

	//pSer->Value("BasicEntityData", basicEntityData);

	bool bHaveReserved = false;

	// load the level
	do
	{
		SGameContextParams ctx;

		if (!pLoadGame->HaveMetadata(SAVEGAME_LEVEL_TAG) || !pLoadGame->HaveMetadata(SAVEGAME_GAMERULES_TAG)) {
			gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
			return failure;
		}

		ctx.levelName = pLoadGame->GetMetadata( SAVEGAME_LEVEL_TAG );
		ctx.gameRules = pLoadGame->GetMetadata( SAVEGAME_GAMERULES_TAG );

#if ENABLE_QUICK_LOAD
		const char * currentLevel = pCryAction->GetLevelName();
		if (currentLevel)
		{
			if (0 == stricmp(currentLevel, ctx.levelName))
			{
				// can quick-load
				if(!gEnv->p3DEngine->RestoreTerrainFromDisk())
          return failure;

				if(gEnv->p3DEngine)
				{
					gEnv->p3DEngine->ResetPostEffects();
				}

				pEntitySystem->DeletePendingEntities();
				failure = eLGR_FailedAndDestroyedState;
				break;
			}
			else if (requireQuickLoad)
			{
				GameWarning("Unable to quick load: different level names (%s != %s)",
					currentLevel, ctx.levelName);
				gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
				return failure;
			}
			else if (GetISystem()->IsEditor())
			{
				GameWarning("Can only quick-load in editor");
				gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
				return failure;
			}
		}
		else if (requireQuickLoad)
		{
			GameWarning("Unable to quick load: no level loaded");
			gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
			return failure;
		}
#endif

		//NON-QUICKLOAD PATH
		params.pContextParams = &ctx;
		params.flags |= eGSF_BlockingClientConnect | eGSF_LocalOnly;

		gEnv->pEntitySystem->Reset();

		// delete any left-over entities
		pEntitySystem->DeletePendingEntities();
		FlushActivatableGameObjectExtensions();
		pCryAction->FlushBreakableObjects();

		// Clean all entities that should not be in system before loading entities from savegame.
		DeleteDynamicEntities(); //must happen before reserving entities

		failure = eLGR_FailedAndDestroyedState;

		if (!pCryAction->StartGameContext( &params ))
		{
			GameWarning("Could not start game context.");
			gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
			return failure;
		}
		pCryAction->GetGameContext()->SetLoadingSaveGame(loadingSaveGame);

		bHaveReserved = true;
		ReserveEntityIds();

		// Reset the flowsystem here (sending eFE_Initialize) to all FGs
		// also makes sure, that nodes present in the level.pak but not in the
		// savegame loaded afterwards get initialized correctly
		gEnv->pFlowSystem->Reset();
	}
	while (false);

	checkpoint.Check("DestroyedState");

	failure = eLGR_FailedAndDestroyedState;

	// timer serialization (after potential context switch, e.g. when loading a savegame for which Level has not been loaded yet)
	{
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_TIMER_SECTION));
		pSer = temp;
	}
	if (!pSer.get())
		GameWarning("Unable to open timer %s", SAVEGAME_TIMER_SECTION);
	else
		gEnv->pTimer->Serialize(*pSer);


#ifdef USER_alexl
	CryLogAlways("[GameSerialize]: Loaded Timer and paused at %f (frame=%d)", gEnv->pTimer->GetCurrTime(), gEnv->pRenderer->GetFrameID(false));
#endif

	// Now Pause the Game timer if not already done!
	// so no time passes until serialization is over
	SPauseGameTimer pauseGameTimer;

	// terrain modifications (e.g. heightmap changes)
	{
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_TERRAINSTATE_SECTION));
		pSer = temp;
	}
	if (!pSer.get())
		GameWarning("Unable to open section %s", SAVEGAME_TERRAINSTATE_SECTION);
	else
		gEnv->p3DEngine->SerializeState( *pSer );

	checkpoint.Check("3DEngine");

	// game tokens
	{
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_GAMETOKEN_SECTION));
		pSer = temp;
	}
	if (!pSer.get())
		GameWarning("No game token data in save game");
	else
	{
		IGameTokenSystem* pGTS = CCryAction::GetCryAction()->GetIGameTokenSystem();
		if (pGTS)
		{
			if (GetISystem()->IsEditor())
			{
				char* levelName;
				char* levelFolder;
				pCryAction->GetEditorLevel(&levelName, &levelFolder);
				string tokenPath = levelFolder;
				tokenPath += "/GameTokens/*.xml";
				pGTS->Reset();
				pGTS->LoadLibs( tokenPath );
				pGTS->Serialize( *pSer );
			}
			else
			{
				ILevelSystem *pLSys = CCryAction::GetCryAction()->GetILevelSystem();
				assert (pLSys != 0);
				if (pLSys) {
					ILevel *pLevel = pLSys->GetCurrentLevel();
					assert (pLevel != 0);
					if (pLevel)
					{
						string tokenPath = pLevel->GetLevelInfo()->GetPath();
						tokenPath+="/GameTokens/*.xml";
						pGTS->Reset();
						pGTS->LoadLibs( tokenPath );
						pGTS->Serialize( *pSer );
					}
				}
			}
		}
	}

	checkpoint.Check("GameTokens");

	CMaterialEffects* pMatFX = static_cast<CMaterialEffects*> (pCryAction->GetIMaterialEffects());
	if (pMatFX)
		pMatFX->Reset();

	{
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_MATERIALEFFECTS_SECTION));
		pSer = temp;
	}
	if (!pSer.get())
		GameWarning("Unable to open section %s", SAVEGAME_MATERIALEFFECTS_SECTION);
	else
	{
		if (pMatFX)
		{
			pMatFX->Serialize( *pSer );
		}
	}
	checkpoint.Check("MaterialFX");

	// ViewSystem Serialization
	IViewSystem* pViewSystem = pCryAction->GetIViewSystem();
	{
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_VIEWSYSTEM_SECTION));
		pSer = temp;
	}
	if (!pSer.get())
		GameWarning("Unable to open section %s", SAVEGAME_VIEWSYSTEM_SECTION);
	else
	{
		if (pViewSystem)
		{
			pViewSystem->Serialize( *pSer );
		}
	}

	checkpoint.Check("ViewSystem");

	// SoundSystem Serialization
	ISoundSystem* pSoundystem = gEnv->pSoundSystem;
	if (pSoundystem)
	{
		pSoundystem->Silence(false, false);
		pSoundystem->Update(eSoundUpdateMode_All); // call one update so it can remove some sounds
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_SOUNDSYSTEM_SECTION));
		pSer = temp;
		
		if (pSer.get())
			pSoundystem->Serialize(*pSer);
		else
			GameWarning("Unable to open section %s", SAVEGAME_SOUNDSYSTEM_SECTION);
	}
	checkpoint.Check("SoundSystem");

	if(pCryAction->GetIItemSystem())
	{
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_LTLINVENTORY_SECTION));
		pSer = temp;
		if(pSer.get())
			pCryAction->GetIItemSystem()->Serialize(*pSer);
		else
			GameWarning("Unable to open section %s", SAVEGAME_LTLINVENTORY_SECTION);
	}
	checkpoint.Check("ItemSystem");

	// Dialog System Reset & Serialization
	CDialogSystem* pDS = pCryAction->GetDialogSystem();
	if (pDS)
		pDS->Reset();

	// Dialog System First Pass [recreates DialogSessions]
	{
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_DIALOGSYSTEM_SECTION));
		pSer = temp;
	}
	if (!pSer.get())
		GameWarning("Unable to open section %s", SAVEGAME_DIALOGSYSTEM_SECTION);
	else
	{
		if (pDS)
		{
			pDS->Serialize( *pSer );
		}
	}
	checkpoint.Check("DialogSystem");

	// game state...

	std::vector<SBasicEntityData> basicEntityData;
	// entity creation/deletion/repositioning
	{
		checkpoint.Check("DeleteDynamicEntities");

#ifdef EXCESSIVE_ENTITY_DEBUG
		// dump entities which can potentially be re-used (if class matches)
		CryLogAlways("*QL -----------------------------------------");
		CryLogAlways("*QL Dumping Entities after deletion of unused");
		DumpEntities();
		CryLogAlways("*QL Done Dumping Entities after deletion of unused");
		CryLogAlways("*QL -----------------------------------------");
#endif

		// we reserve the Entities AFTER we deleted any left-overs
		// = entities which are in mem, but not in savegame.
		// this could cause collisions with SaltHandles because a left-over and a new one can point to the same
		// handle (with different salts). if we reserve before deletion, we would actually delete our reserved handle
		if (!bHaveReserved)
		{
			// delete any left-over entities
			pEntitySystem->DeletePendingEntities();
			checkpoint.Check("DeletePending");

			FlushActivatableGameObjectExtensions();
			checkpoint.Check("FlushExtensions");

			pCryAction->FlushBreakableObjects();
			checkpoint.Check("FlushBreakables");

			// Clean all entities that should not be in system before loading entties from savegame.
			DeleteDynamicEntities(); //must happen before reserving entities

			ReserveEntityIds();
			bHaveReserved = true;
		}

		// serialize breakable objects AFTER reservation
		// this will quite likely spawn new entities
		{
			std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_GAMESTATE_SECTION));
			pSer = temp;
		}
		if (!pSer.get()) {
			gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
			return failure;
		}

		
		pCryAction->Serialize(*pSer);	//breakable object
		checkpoint.Check("SerializeBreakables");

		/*
		// reposition any entities that are in both the save game and the level
		if (!RepositionEntities( basicEntityData, false ))
		{
		bLoadingErrors = true;
		//return failure;
		}
		*/

		//lock entity system
		gEnv->pEntitySystem->LockSpawning(true);
		//fix breakables forced ids
		gEnv->pEntitySystem->SetNextSpawnId(0);

		int nNumSavedEntities = 0;
		pSer->BeginGroup( "BasicEntityData" );
		pSer->Value( "EntityCount",nNumSavedEntities );

		assert( nNumSavedEntities == m_savedEntityIds.size() );

		// create any entities that are not in the level, but are in the save game
		// or that have the wrong class
		for (int i = 0; i < nNumSavedEntities; i++)
		{
			SSerializeScopedBeginGroup entity_group( *pSer,"Entity" );

			SBasicEntityData bed;
			bed.Serialize( *pSer );
			basicEntityData.push_back(bed);

			IEntity *pEntity = pEntitySystem->GetEntity(bed.id);
			bool forceRemoval = false;
			if (!pEntity)
			{
				if (EntityId realId = pEntitySystem->GetEntityIdInSlot(bed.id))
				{
					pEntity = pEntitySystem->GetEntity(realId);
					GameWarning( "[LoadGame] Entity ID=%d '%s' in the level has clobbered entity of class '%s' with id %d with non-matching salt",pEntity->GetId(), pEntity->GetEntityTextDescription(), bed.className.c_str(), bed.id );
					forceRemoval = true;
				}
			}
			else
			{
				forceRemoval = (0 != strcmp(bed.className,pEntity->GetClass()->GetName()));
			}
			if (pEntity && forceRemoval)
			{
				GameWarning( "[LoadGame] Entity ID=%d '%s', class mismatch, should be '%s'",pEntity->GetId(), pEntity->GetEntityTextDescription(), bed.className.c_str() );
				if (!(pEntity->GetFlags() & ENTITY_FLAG_UNREMOVABLE))
				{
					pEntitySystem->RemoveEntity( pEntity->GetId(), true );
					pEntity = 0;
				}
				else
				{
					// Set loaded flags on old entity.
					pEntity->SetFlags(bed.flags);
				}
			}
			if (!pEntity)
			{
				IEntityClass * pClass = pEntitySystem->GetClassRegistry()->FindClass(bed.className.c_str());
				if (!pClass)
				{
					GameWarning( "[LoadGame] Failed Spawning Entity ID=%d, Class '%s' not found", bed.id,bed.className.c_str() );
					bLoadingErrors = true;
					continue;
					//return failure;
				}

				SEntitySpawnParams params;
				bool ok = true;
				params.id = bed.id;
				params.vPosition = bed.pos;
				params.qRotation = bed.rot;
				params.vScale = bed.scale;
				params.sName = bed.name.c_str();
				params.nFlags = bed.flags;
				params.pClass = pClass;
				params.bIgnoreLock = true;
				if (!bed.archetype.empty())
				{
					params.pArchetype = gEnv->pEntitySystem->LoadEntityArchetype(bed.archetype);
				}
				pEntity = pEntitySystem->SpawnEntity( params,false );
				if (!pEntity)
				{
					GameWarning( "[LoadGame] Failed Spawning Entity ID=%d, %s (%s)", params.id,bed.name.c_str(),bed.className.c_str() );
					//return failure;
					bLoadingErrors = true;
					continue;
				}
				else if(pEntity->GetId() != bed.id)
				{
					GameWarning("[LoadGame] Spawned Entity ID=%d, %s (%s), with wrong ID=%d", bed.id,bed.name.c_str(), bed.className.c_str(), pEntity->GetId());
					//return failure;
					bLoadingErrors = true;
					continue;
				}
#ifdef _DEBUG
				else	//look for additionally spawned entities (that are not in the save file)
				{
					pIt->MoveFirst();
					while (!pIt->IsEnd())
					{
						IEntity * pNextEntity = pIt->Next();
						if ((stl::binary_find(m_savedEntityIds.begin(),m_savedEntityIds.end(),pNextEntity->GetId() ) == m_savedEntityIds.end()))
							GameWarning("[LoadGame] Entities were spawned that are not in the save file! : %s with ID=%d", pNextEntity->GetEntityTextDescription(), pNextEntity->GetId());
					}
				}
#endif

				// Serialize script properties, this must happen before entity Init
				pEntity->Serialize( *pSer,ENTITY_SERIALIZE_PROPERTIES );

				// Initialize entity after properties have been loaded.
				if (!pEntitySystem->InitEntity( pEntity,params ))
				{
					GameWarning( "[LoadGame] Failed Initializing Entity ID=%d, %s (%s)", params.id,bed.name.c_str(),bed.className.c_str() );
					bLoadingErrors = true;
					continue;
				}
			}
			if (!pEntitySystem->GetEntity(bed.id))
			{
				GameWarning("[LoadGame] Failed Loading Entity ID=%d, %s (%s)", bed.id,bed.name.c_str(), bed.className.c_str() );
				//return failure;
				bLoadingErrors = true;
				continue;
			}
		}
		pSer->EndGroup(); // pSer->BeginGroup( "BasicEntityData" );

		checkpoint.Check("AllEntities");

	}

	// reposition any entities (once again for sanities sake)
	if (!RepositionEntities( basicEntityData, true ))
	{
		//return failure;
		bLoadingErrors = true;
	}

	checkpoint.Check("Reposition");

	FlushActivatableGameObjectExtensions();
	checkpoint.Check("FlushExtensions2");

	
	// entity state restoration
	pSer->BeginGroup("ExtraEntityData");

	// set the basic entity data
	for (std::vector<SBasicEntityData>::const_iterator iter = basicEntityData.begin(); iter != basicEntityData.end(); ++iter)
	{
		IEntity * pEntity = gEnv->pEntitySystem->GetEntity(iter->id);
		if (!pEntity)
		{
			GameWarning( "[LoadGame] Missing Entity ID %d", iter->id);
			//return failure;
			bLoadingErrors = true;
			continue;
		}

		pEntity->SetUpdatePolicy( (EEntityUpdatePolicy) iter->updatePolicy );
		pEntity->Hide( iter->isHidden );
		pEntity->Invisible( iter->isInvisible );
		pEntity->Activate( iter->isActive );

		// extra sanity check for matching class
		if (iter->className != pEntity->GetClass()->GetName())
		{
			GameWarning( "[LoadGame] Entity class mismatch ID=%d %s, should have class '%s'",pEntity->GetId(), pEntity->GetEntityTextDescription(), iter->className.c_str() );
			bLoadingErrors = true;
			continue;
		}

		if (pEntity->CheckFlags(ENTITY_FLAG_SPAWNED|ENTITY_FLAG_MODIFIED_BY_PHYSICS) && iter->iPhysType!=PE_NONE)
		{
			SEntityPhysicalizeParams epp;
			epp.type = iter->iPhysType;
			pEntity->Physicalize(epp);
		}
		if ((iter->flags & (ENTITY_FLAG_MODIFIED_BY_PHYSICS|ENTITY_FLAG_SPAWNED)) == ENTITY_FLAG_MODIFIED_BY_PHYSICS)
			pEntity->AddFlags(iter->flags & ENTITY_FLAG_MODIFIED_BY_PHYSICS);

		// c++ state
		pSer->BeginGroup("Entity");
		EntityId id = (EntityId)-1;
		pSer->Value( "id", id );
		if (id != iter->id)
		{
			pSer->EndGroup();
			GameWarning("[LoadGame] Expected Entity ID=%d, %s (%s), Loaded ID=%d", id,iter->name.c_str(), iter->className.c_str(), pEntity->GetId());
			bLoadingErrors = true;
			continue;
		}

		int nEntityFlags = iter->flags;

		if ((nEntityFlags&ENTITY_FLAG_MODIFIED_BY_PHYSICS) != 0)
		{
			pSer->BeginGroup("EntityGeometry");
			pEntity->Serialize( *pSer, ENTITY_SERIALIZE_GEOMETRIES );
			pSer->EndGroup();
		}

#ifdef SECUROM_32
		if (gEnv->pProtectedFunctions[eProtectedFunc_Load])
			gEnv->pProtectedFunctions[eProtectedFunc_Load]( (void*)pEntity,(void*)&(*pSer) );
#else
		pEntity->Serialize( *pSer, ENTITY_SERIALIZE_PROXIES );
#endif

		pSer->EndGroup();
	}
	
	pSer->EndGroup();
	checkpoint.Check("ExtraEntityData");

	//unlock entity system
	gEnv->pEntitySystem->LockSpawning(false);

	//serialize timers etc. in entity system
	gEnv->pEntitySystem->Serialize(*pSer);
	checkpoint.Check("EntitySystem");

	{
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_AISTATE_SECTION));
		pSer = temp;
	}
	if (!pSer.get())
	{
		GameWarning("No AI section in save game");
		//return failure;
		bLoadingErrors = true;
	}
	else
	{
		gEnv->pAISystem->Serialize(*pSer);
	}
	checkpoint.Check("AI System");

	//clear old entities from the systems [could become obsolete]
	gEnv->pGame->GetIGameFramework()->GetIItemSystem()->Reset();
	gEnv->pGame->GetIGameFramework()->GetIActorSystem()->Reset();
	gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->Reset();
	checkpoint.Check("ResetSubSystems");

	// MusicSystem Serialization
	if (gEnv->pMusicSystem)
	{
		gEnv->pMusicSystem->EndTheme(EThemeFade_StopAtOnce, 0);
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_MUSICSYSTEM_SECTION));
		pSer = temp;

		if (pSer.get())
			gEnv->pMusicSystem->Serialize(*pSer);
		else
			GameWarning("Unable to open section %s", SAVEGAME_MUSICSYSTEM_SECTION);
	}
	checkpoint.Check("MusicSystem");

	// MusicLogic Serialization
	if (pCryAction->GetMusicLogic())
	{
		std::auto_ptr<TSerialize> temp(pLoadGame->GetSection(SAVEGAME_MUSICLOGIC_SECTION));
		pSer = temp;

		if (pSer.get())
			pCryAction->GetMusicLogic()->Serialize(*pSer);
		else
			GameWarning("Unable to open section %s", SAVEGAME_MUSICLOGIC_SECTION);
	}
	checkpoint.Check("MusicLogic");

	//when loading a save game from scratch this is hopefully singleplayer; let network know ...
	pCryAction->GetGameServerNub()->SetMaxPlayers(1);

	if(GetISystem()->IsSerializingFile() == 1)		//general quickload fix-ups
	{
		//clear keys
		gEnv->pInput->ClearKeyState();

		//reattach actor - links
		/*IActorSystem *pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
		IActor *pClientActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
		int actorCount = pActorSystem->GetActorCount();
		if (actorCount > 0)
		{
			IActorIteratorPtr pIter = pActorSystem->CreateActorIterator();
			while (IActor *pActor = pIter->Next())
			{
				if(pActor == pClientActor)
					continue;

				bool holster = false;

				IItem * pItem = pActor->GetCurrentItem();
				if(!pItem)
				{
					if(pItem = pActor->GetHolsteredItem())
						holster = true;
				}

				if(pItem)
				{
					if(!pItem->IsMountable())
						pItem->PickUp(pActor->GetEntityId(), false, true);
					pItem->Select(true);
					if(holster)
					{
						pActor->HolsterItem(false); //must unholster first
						pActor->HolsterItem(true);
					}
				}
				//else
				//	GameWarning("On Loading: Actor spawned without weapon!");
			}
		}*/
	}

	gEnv->p3DEngine->PostSerialize(true);
	checkpoint.Check("3DPostSerialize");

#ifdef EXCESSIVE_ENTITY_DEBUG
	CryLogAlways("Dumping Entities after serialization");
	DumpEntities();
#endif

	//inform all entities that serialization is over
	SEntityEvent event2(ENTITY_EVENT_POST_SERIALIZE);
	gEnv->pEntitySystem->SendEventToAll(event2);
	gEnv->pSystem->SetThreadState(ESubsys_Physics, true);

	//return failure;
	if (bLoadingErrors)
	{
		GameWarning( "!Errors during Game Loading" );
	}
	checkpoint.Check("EntityPostSerialize");

	if (gEnv->pScriptSystem)
		gEnv->pScriptSystem->ForceGarbageCollection();

	checkpoint.Check("Lua GC Cycle");

	Clean();
	checkpoint.Check("PostClean");

	return eLGR_Ok;
}
