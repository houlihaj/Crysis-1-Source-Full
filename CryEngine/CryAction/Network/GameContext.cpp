/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 3:9:2004   11:25 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "GameContext.h"
#include "GameServerChannel.h"
#include "GameClientChannel.h"
#include "IActorSystem.h"
#include "IGame.h"
#include "CryAction.h"
#include "GameRulesSystem.h"
#include "ScriptHelpers.h"
#include "GameObjects/GameObject.h"
#include "ScriptRMI.h"
#include "IPhysics.h"
#include "PhysicsSync.h"
#include "IEntityRenderState.h"
#include "GameClientNub.h"
#include "GameServerNub.h"
#include "ActionGame.h"
#include "ILevelSystem.h"
#include "IPlayerProfiles.h"
#include "VoiceController.h"
#include "BreakReplicator.h"
#include "CVarListProcessor.h"
#include "NetworkCVars.h"

// context establishment tasks
#include "NetHelpers.h"
#include "CET_ActionMap.h"
#include "CET_ClassRegistry.h"
#include "CET_CVars.h"
#include "CET_DemoMode.h"
#include "CET_EntitySystem.h"
#include "CET_GameRules.h"
#include "CET_LevelLoading.h"
#include "CET_NetConfig.h"
#include "CET_View.h"

#define VERBOSE_TRACE_CONTEXT_SPAWNS 0

CGameContext * CGameContext::s_pGameContext = NULL;

static const char * hexchars = "0123456789abcdef";

static ILINE string ToHexStr(const char * x, int len)
{
	string out;
	for (int i=0; i<len; i++)
	{
		uint8 c = x[i];
		out += hexchars[c >> 4];
		out += hexchars[c & 0xf];
	}
	return out;
}

static ILINE bool FromHexStr(string& x)
{
	string out;
	uint8 cur;
	for (int i=0; i<x.length(); i++)
	{
		int j;
		for (j=0; hexchars[j]; j++)
			if (hexchars[j] == x[i])
				break;
		if (!hexchars[j])
			return false;
		cur = (cur << 4) | j;
		if (i&1)
			out += cur;
	}
	x.swap(out);
	return true;
}

CGameContext::CGameContext( CCryAction * pFramework, CScriptRMI * pScriptRMI, CActionGame *pGame ) : 
	m_pFramework(pFramework), 
	m_pPhysicsSync(0),
	m_pNetContext(0),
	m_pEntitySystem(0),
	m_pGame(pGame),
	m_controlledObjects(false),
	m_isInLevelLoad(false),
	m_pScriptRMI(pScriptRMI),
	m_bStarted(false),
	m_bIsLoadingSaveGame(false),
	m_bHasSpawnPoint(true),
	m_bAllowSendClientConnect(false),
	m_loadFlags(eEF_LoadNewLevel),
	m_resourceLocks(0),
	m_removeObjectUnlocks(0),
	m_broadcastActionEventInGame(-1),
	m_levelChecksum(0)
{
	assert( s_pGameContext == NULL );
	s_pGameContext = this;
	gEnv->pConsole->AddConsoleVarSink(this);

	m_pScriptRMI->SetContext(this);
	m_pVoiceController = NULL;

	// default (permissive) flags until we are initialized correctly
	// ensures cvars can be changed just at the editor start
	m_flags = eGSF_LocalOnly | eGSF_Server | eGSF_Client;

	m_pPhysicalWorld = gEnv->pPhysicalWorld;
	m_pGame->AddGlobalPhysicsCallback(eEPE_OnCollisionLogged, OnCollision, 0);

	UnprotectCVars();
}

CGameContext::~CGameContext()
{
	gEnv->pConsole->RemoveConsoleVarSink( this );

	IScriptSystem * pSS = gEnv->pScriptSystem;
	for (DelegateCallbacks::iterator iter = m_delegateCallbacks.begin(); iter != m_delegateCallbacks.end(); ++iter)
	{
		pSS->ReleaseFunc(iter->second);
	}

	if (m_pEntitySystem)
		m_pEntitySystem->RemoveSink( this );

	m_pGame->RemoveGlobalPhysicsCallback(eEPE_OnCollisionLogged, OnCollision, 0);

	m_pScriptRMI->SetContext(NULL);
	delete m_pVoiceController;

	s_pGameContext = NULL;
}

void CGameContext::Init( INetContext * pNetContext )
{
	assert( !m_pEntitySystem );
	m_pEntitySystem = gEnv->pEntitySystem;
	assert( m_pEntitySystem );
	m_pEntitySystem->AddSink( this );
	m_pNetContext = pNetContext;

	m_pNetContext->DeclareAspect( "GameClientDynamic", eEA_GameClientDynamic, eAF_Delegatable );
	m_pNetContext->DeclareAspect( "GameServerDynamic", eEA_GameServerDynamic, 0 );
	m_pNetContext->DeclareAspect( "GameClientStatic", eEA_GameClientStatic, eAF_Delegatable );
	m_pNetContext->DeclareAspect( "GameServerStatic", eEA_GameServerStatic, eAF_ServerManagedProfile );
	m_pNetContext->DeclareAspect( "Physics", eEA_Physics, eAF_Delegatable | eAF_ServerManagedProfile | eAF_HashState | eAF_TimestampState );
	m_pNetContext->DeclareAspect( "Script", eEA_Script, 0 );
}

void CGameContext::SetContextInfo(unsigned flags, uint16 port, const char * connectionString)
{ 
	m_flags = flags; 
	m_port = port;
	m_connectionString = connectionString;

	if (!(flags & eGSF_LocalOnly))
		m_pBreakReplicator.reset( new CBreakReplicator(this) );

	if (!gEnv->pSystem->IsDedicated())
	{
		m_pVoiceController = new CVoiceController(this);
		if (!m_pVoiceController->Init())
		{
			delete m_pVoiceController;
			m_pVoiceController = NULL;
		}
		else
			m_pVoiceController->Enable(m_pFramework->IsVoiceRecordingEnabled());
	}
}

//
// IGameContext
//
 
void CGameContext::AddLoadLevelTasks( IContextEstablisher * pEst, bool isServer, int flags, bool ** ppLoadingStarted, int establishedToken )
{
	if (gEnv->bClient && (flags & eEF_LoadNewLevel) == 0)
	{
		AddActionEvent( pEst, eCVS_Begin, eAE_resetBegin );
		AddActionEvent( pEst, eCVS_Begin, SActionEvent(eAE_resetProgress, 0) );
	}

	AddLockResources( pEst, eCVS_Begin, eCVS_InGame, this );
	if (flags & eEF_LevelLoaded)
	{
		AddRandomSystemReset( pEst, eCVS_Begin );
	}
	if (flags & eEF_LoadNewLevel)
		AddPrepareLevelLoad( pEst, eCVS_Begin );
	bool resetSkipPlayers = false && ((flags & eEF_LoadNewLevel)==0) && isServer;
	bool resetSkipGamerules = ((flags & eEF_LoadNewLevel)==0);
	AddEntitySystemReset( pEst, eCVS_Begin, resetSkipPlayers, resetSkipGamerules );
	if (!(resetSkipPlayers || resetSkipGamerules))
		AddAnimationGraphReset( pEst, eCVS_Begin );
	if (flags & eEF_LoadNewLevel)
		AddClearPlayerIds( pEst, eCVS_Begin );
	AddSetValue( pEst, eCVS_EstablishContext, &m_isInLevelLoad, true, "BeginLevelLoad" );
	AddInitImmersiveness( pEst, eCVS_EstablishContext );
	if (flags & eEF_LoadNewLevel)
	{
		AddGameRulesCreation( pEst, eCVS_EstablishContext );
		// need to store the level checksum before loading, so it is reported correctly to GameSpy.
		if(gEnv->bMultiplayer && isServer)
			AddStoreLevelChecksum(pEst, eCVS_EstablishContext);

		AddLoadLevel( pEst, eCVS_EstablishContext, ppLoadingStarted );
	}
	else
	{
		AddFakeSpawn( pEst, eCVS_EstablishContext, eFS_GameRules | eFS_Opt_Rebind );
		AddLoadLevelEntities( pEst, eCVS_EstablishContext );
/*
		if (isServer)
		{
			AddSetValue( pEst, eCVS_EstablishContext, &m_isInLevelLoad, false, "PauseLevelLoad" );
			AddFakeSpawn( pEst, eCVS_EstablishContext, eFS_Players | eFS_Opt_Rebind );
			AddSetValue( pEst, eCVS_EstablishContext, &m_isInLevelLoad, true, "RestartLevelLoad" );
		}
*/
	}
	AddSetValue( pEst, eCVS_EstablishContext, &m_isInLevelLoad, false, "EndLevelLoad" );
	AddEstablishedContext( pEst, eCVS_EstablishContext, establishedToken );
	AddLockResources( pEst, eCVS_Begin, eCVS_InGame, this );
}

void CGameContext::AddLoadingCompleteTasks( IContextEstablisher * pEst, int flags, bool * pLoadingStarted )
{
	if (HasContextFlag(eGSF_Server))
		AddPauseGame(pEst, eCVS_Begin, true, false);

	if (!gEnv->pSystem->IsDedicated())
		AddRegisterSoundListener( pEst, eCVS_InGame );

	if (flags & eEF_LoadNewLevel)
	{
		SEntityEvent startLevelEvent(ENTITY_EVENT_START_LEVEL);
		AddEntitySystemEvent( pEst, eCVS_InGame, startLevelEvent );
	}

	if (!gEnv->pSystem->IsEditor() && gEnv->pSystem->IsSerializingFile() != 2) /*  && (flags&eEF_LoadNewLevel)==0) */
		AddResetAreas( pEst, eCVS_InGame );

	if ((flags & eEF_LoadNewLevel) == 0)
		AddGameRulesReset( pEst, eCVS_InGame );

	if (!gEnv->pSystem->IsEditor() && (flags & eEF_LoadNewLevel))
		AddLoadingComplete( pEst, eCVS_InGame, pLoadingStarted );

	if (HasContextFlag(eGSF_Server))
		AddPauseGame(pEst, eCVS_InGame, false, false);
	AddSetValue( pEst, eCVS_InGame, &m_bStarted, true, "GameStarted" );

	if (HasContextFlag(eGSF_Client))
		if (CGameClientNub * pGCN = CCryAction::GetCryAction()->GetGameClientNub())
			if (CGameClientChannel * pGCC = pGCN->GetGameClientChannel())
				pGCC->AddUpdateLevelLoaded(pEst);

	AddLockResources( pEst, eCVS_Begin, eCVS_InGame, this );
}

bool CGameContext::InitGlobalEstablishmentTasks( IContextEstablisher * pEst, int establishedToken )
{
	AddLockResources( pEst, eCVS_Begin, eCVS_InGame, this );
	AddSetValue( pEst, eCVS_Begin, &m_bStarted, false, "GameNotStarted" );
	AddDisableActionMap(pEst, eCVS_Begin);
	if (gEnv->pSystem->IsEditor())
	{
		AddEstablishedContext(pEst, eCVS_EstablishContext, establishedToken);
		AddFakeSpawn(pEst, eCVS_EstablishContext, eFS_All);
	}
	else if (HasContextFlag(eGSF_Server))
	{
		bool * pLoadingStarted = 0;
		AddLoadLevelTasks(pEst, true, m_loadFlags, &pLoadingStarted, establishedToken);
		// here is the special handling for playing back a demo session in dedicated mode - both eGSF_Server and eGSF_Client are specified,
		// but we need to perform LoadingCompleteTasks while skipping all the channel establishment tasks
		if ( GetISystem()->IsDedicated() || !HasContextFlag(eGSF_Client))
			AddLoadingCompleteTasks(pEst, m_loadFlags, pLoadingStarted);
		if (HasContextFlag(eGSF_Server))
		{
			SEntityEvent startGameEvent(ENTITY_EVENT_START_GAME);
			AddEntitySystemEvent( pEst, eCVS_InGame, startGameEvent );
		}
	}
	AddLockResources( pEst, eCVS_Begin, eCVS_InGame, this );

	return true;
}

bool CGameContext::InitChannelEstablishmentTasks( IContextEstablisher * pEst, INetChannel * pChannel, int establishedSerial )
{
	// local channels on a dedicated server are dummy connection for dedicated server demo recording
	// skip all the channel establishment tasks (esp. player spawning and setup - OnClientConnect/EnteredGame)
	if ( GetISystem()->IsDedicated() && pChannel->IsLocal() )
		return true;

	// for DemoPlayback, we want the local player spawned as a spectator for the demo session, so
	// the normal channel establishment tasks are needed

	bool isLocal = pChannel->IsLocal();
	bool isServer = ((CGameChannel*)pChannel->GetGameChannel())->IsServer();
	bool isClient = !isServer;
	bool isLevelLoaded = false;
	CGameServerChannel *pServerChannel=0;
	if (isServer)
	{
		pServerChannel=(CGameServerChannel*)pChannel->GetGameChannel();
		isLevelLoaded = pServerChannel->CheckLevelLoaded();
	}
	else
	{
		if (CGameClientNub * pGCN = CCryAction::GetCryAction()->GetGameClientNub())
			if (CGameClientChannel * pGCC = pGCN->GetGameClientChannel())
				isLevelLoaded = pGCC->CheckLevelLoaded();
	}

	bool isReset = m_resetChannels.find(pChannel) != m_resetChannels.end();
	m_resetChannels.erase(pChannel);

	if (isReset && !isLevelLoaded)
		isReset = false;

	int flags = (isLevelLoaded? eEF_LevelLoaded : 0) | (isReset? 0 : eEF_LoadNewLevel);

	if (isServer && !isReset && (0 == (flags & eEF_LevelLoaded)))
		AddRegisterAllClasses( pEst, eCVS_Begin, &m_classRegistry );

	// please keep the following bare minimum channel establishment tasks first, since DemoRecorderChannel will make
	// an early out right after these
	if (isServer && !isLocal)
	{
		if (!isReset)
		{
			std::vector<SSendableHandle> * pWaitFor = 0;
			if (0 == (flags & eEF_LevelLoaded))
			{
				AddCVarSync( pEst, eCVS_Begin, (CGameServerChannel*)pChannel->GetGameChannel() );
				AddSendClassRegistration( pEst, eCVS_Begin, &m_classRegistry, &pWaitFor );
			}
			AddSendGameType( pEst, eCVS_Begin, &m_classRegistry, pWaitFor );
		}
		else
		{
			//AddSendResetMap( pEst, eCVS_Begin );
		}
	}

	// do NOT add normal channel establishment tasks here, otherwise it breaks DemoRecorder

	// should skip all other channel establishment tasks for DemoRecorderChannel (local channel ID is 0)
	if ( pChannel->GetLocalChannelID() == 0 )
		return true;

	// add normal channel establishment tasks here

	AddLockResources( pEst, eCVS_Begin, eCVS_InGame, this );

	if (isServer)
	{
		if (gEnv->pSystem->IsEditor())
			AddWaitValue( pEst, eCVS_PostSpawnEntities, &m_bAllowSendClientConnect, true, "WaitForAllowSendClientConnect" );
		AddOnClientConnect( pEst, eCVS_PostSpawnEntities, isReset );
		AddOnClientEnteredGame( pEst, eCVS_InGame, isReset );
		if (pServerChannel->IsOnHold())
			AddClearOnHold( pEst, eCVS_InGame );
		AddDelegateAuthorityToClientActor( pEst, eCVS_InGame );
	}

	bool * pLoadingStarted = 0;
	if (isClient && !HasContextFlag(eGSF_Server))
	{
		AddLoadLevelTasks(pEst, false, flags, &pLoadingStarted, establishedSerial);
	}

	if (isClient)
	{
		AddInitActionMap_ClientActor( pEst, eCVS_InGame );
		AddInitView_ClientActor( pEst, eCVS_InGame );

		AddDisableKeyboardMouse( pEst, eCVS_InGame );
	}

	if (isClient)
		AddLoadingCompleteTasks( pEst, flags, pLoadingStarted );

	if (isClient && !HasContextFlag(eGSF_Server))
	{
		SEntityEvent startGameEvent(ENTITY_EVENT_START_GAME);
		AddEntitySystemEvent( pEst, eCVS_InGame, startGameEvent );
	}

	AddLockResources( pEst, eCVS_Begin, eCVS_InGame, this );

	if (isClient && !GetISystem()->IsEditor() && !gEnv->bMultiplayer && !(m_flags & eGSF_DemoPlayback))
		AddInitialSaveGame( pEst, eCVS_InGame );

	if (isClient)
	{
		if (isReset)
		{
			for (int i=eCVS_EstablishContext; i<eCVS_PostSpawnEntities; i++)
				AddActionEvent( pEst, eCVS_Begin, SActionEvent(eAE_resetProgress, 100*(i-eCVS_EstablishContext)/(eCVS_PostSpawnEntities-eCVS_EstablishContext) ) );
			AddActionEvent( pEst, eCVS_InGame, eAE_resetEnd );
		}
		else
		{
			AddSetValue( pEst, eCVS_InGame, &m_broadcastActionEventInGame, 4, "BroadcastInGame" );
		}
	}

	// AlexL: Unpause the Game when Editor is started, but no Level isLevelLoaded
	//        this ensures that CTimer's ETIMER_GAME is updated and systems relying
	//        on it work correctly (CharEditor)
	if (gEnv->pSystem->IsEditor() && HasContextFlag(eGSF_Server))
		AddPauseGame(pEst, eCVS_Begin, false, false);

	if (pServerChannel)
		pServerChannel->AddUpdateLevelLoaded( pEst );

	return true;
}

void CGameContext::ResetMap( bool isServer )
{
	if (!isServer && HasContextFlag(eGSF_Server))
		return;

	m_loadFlags = EstablishmentFlags_ResetMap;
	m_resetChannels.clear();

	if (CGameClientNub * pGCN = CCryAction::GetCryAction()->GetGameClientNub())
		if (CGameClientChannel * pGCC = pGCN->GetGameClientChannel())
			m_resetChannels.insert(pGCC->GetNetChannel());

	if (CGameServerNub * pGSN = CCryAction::GetCryAction()->GetGameServerNub())
		if (TServerChannelMap * m = pGSN->GetServerChannelMap())
			for (TServerChannelMap::iterator iter = m->begin(); iter != m->end(); ++iter)
			{
				if(iter->second->GetNetChannel())//channel can be on hold thus not having net channel currently
				{
					CGameClientChannel::SendResetMapWith( SNoParams(), iter->second->GetNetChannel() );
					m_resetChannels.insert( iter->second->GetNetChannel() );
				}
			}

	if (IGameRules *pGameRules=CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules())
		pGameRules->OnResetMap();

	if (isServer)
		m_pNetContext->ChangeContext();
}

/*
void CGameContext::EndContext()
{
	// TODO: temporary
	IActionMap* pActionMap = 0;
	IPlayerProfileManager* pPPMgr = m_pFramework->GetIPlayerProfileManager();
	if (pPPMgr)
	{
		int userCount = pPPMgr->GetUserCount();
		if (userCount > 0)
		{
			IPlayerProfileManager::SUserInfo info;
			pPPMgr->GetUserInfo(0, info);
			IPlayerProfile* pProfile = pPPMgr->GetCurrentProfile(info.userId);
			if (pProfile)
			{
				pActionMap = pProfile->GetActionMap("default");
				if (pActionMap == 0)
					GameWarning("[PlayerProfiles] CGameContext::EndContext: User '%s' has no actionmap 'default'!", info.userId);
			}
			else
				GameWarning("[PlayerProfiles] CGameContext::EndContext: User '%s' has no active profile!", info.userId);
		}
		else
		{
			;
			// GameWarning("[PlayerProfiles] CGameContext::EndContext: No users logged in");
		}
	}

	if (pActionMap == 0)
	{
		// use action map without any profile stuff
		IActionMapManager * pActionMapMan = m_pFramework->GetIActionMapManager();
		if (pActionMapMan)
		{
			pActionMap = pActionMapMan->GetActionMap("default");
		}
	}

	if (pActionMap)
		pActionMap->SetActionListener(0);
	// ~TODO: temporary

	if (!HasContextFlag(eGSF_NoGameRules))
	{
		m_pEntitySystem->Reset();
		for (int i=0; i<64; i++)
			m_pEntitySystem->ReserveEntityId(LOCAL_PLAYER_ENTITY_ID+i);
		if (CGameClientNub * pClientNub = m_pFramework->GetGameClientNub())
			if (CGameClientChannel * pClientChannel = pClientNub->GetGameClientChannel())
				pClientChannel->ClearPlayer();
	}

	m_classRegistry.Reset();
	m_controlledObjects.Reset( m_pFramework->IsServer() );
}
*/

void CGameContext::LockResources()
{
	if (1 == ++m_resourceLocks)
	{
		gEnv->p3DEngine->LockCGFResources();
		GetISystem()->GetIAnimationSystem()->LockResources();
		if (gEnv->pSoundSystem)
			gEnv->pSoundSystem->LockResources();
	}
}

void CGameContext::UnlockResources()
{
	if (0 == --m_resourceLocks)
	{
		gEnv->p3DEngine->UnlockCGFResources();
		GetISystem()->GetIAnimationSystem()->UnlockResources();
		if (gEnv->pSoundSystem)
			gEnv->pSoundSystem->UnlockResources();
	}
}

bool CGameContext::HasSpawnPoint()
{
	IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();

	pIt->MoveFirst();
	while (!pIt->IsEnd())
	{
		if (IEntity * pEntity = pIt->Next())
			if (0 == strcmp(pEntity->GetClass()->GetName(), "SpawnPoint"))
				return true;
	}
	return false;
}

void CGameContext::CallOnSpawnComplete(IEntity *pEntity)
{
	IScriptTable *pScriptTable(pEntity->GetScriptTable());
	if (!pScriptTable)
		return;
	
	if (gEnv->bServer)
	{
		SmartScriptTable server;
		if (pScriptTable->GetValue("Server", server))
		{
			if ((server->GetValueType("OnSpawnComplete") == svtFunction) && pScriptTable->GetScriptSystem()->BeginCall(server, "OnSpawnComplete"))
			{
				pScriptTable->GetScriptSystem()->PushFuncParam(pScriptTable);
				pScriptTable->GetScriptSystem()->EndCall();
			}
		}
	}

	if (gEnv->bClient)
	{
		SmartScriptTable client;
		if (pScriptTable->GetValue("Client", client))
		{
			if ((client->GetValueType("OnSpawnComplete") == svtFunction) && pScriptTable->GetScriptSystem()->BeginCall(client, "OnSpawnComplete"))
			{
				pScriptTable->GetScriptSystem()->PushFuncParam(pScriptTable);
				pScriptTable->GetScriptSystem()->EndCall();
			}
		}
	}
}

uint8 CGameContext::GetDefaultProfileForAspect( EntityId id, uint8 aspectID )
{
	IEntity * pEntity = m_pEntitySystem->GetEntity( id );
	if (!pEntity)
	{
		if (!GetISystem()->IsEditor())
			GameWarning( "Trying to get default profile for aspect %d on unknown entity %d", aspectID, id );
		return ~uint8(0);
	}

	IEntityProxy * pProxy = pEntity->GetProxy( ENTITY_PROXY_USER );
	if (pProxy)
	{
		CGameObject * pGameObject = (CGameObject *)pProxy;
		return pGameObject->GetDefaultProfile( (EEntityAspects)aspectID );
	}
	return 0;
}

static uint32 GetLowResSpacialCoord(float x)
{
	return (uint32)(x/0.75f);
}

static uint32 GetLowResAngle(float x)
{
	return (uint32)(180.0f*x/gf_PI/10);
}

uint32 CGameContext::HashAspect( EntityId entityId, uint8 nAspect )
{
	IEntity * pEntity = m_pEntitySystem->GetEntity( entityId );
	if (!pEntity)
	{
		if (!GetISystem()->IsEditor())
			GameWarning( "Trying to hash non-existant entity %d", entityId );
		return 0;
	}

	switch (nAspect)
	{
	case eEA_Physics:
		{
			pe_status_pos p;
			IPhysicalEntity * pPhys = pEntity->GetPhysics();
			if (!pPhys)
				break;
			pPhys->GetStatus(&p);
			static const int MULTIPLIER = 16;
			static const uint32 MASK = 0xf;
			uint32 hash = 0;
			hash = MULTIPLIER*hash + (GetLowResSpacialCoord(p.pos.x) & MASK);
			hash = MULTIPLIER*hash + (GetLowResSpacialCoord(p.pos.y) & MASK);
			hash = MULTIPLIER*hash + (GetLowResSpacialCoord(p.pos.z) & MASK);
			Ang3 angles(p.q);
			hash = MULTIPLIER*hash + (GetLowResAngle(angles.x) & MASK);
			hash = MULTIPLIER*hash + (GetLowResAngle(angles.y) & MASK);
			hash = MULTIPLIER*hash + (GetLowResAngle(angles.z) & MASK);
			return hash;
		}
		break;
	}

	return 0;
}

ESynchObjectResult CGameContext::SynchObject( EntityId entityId, uint8 nAspect, uint8 profile, TSerialize serialize, bool verboseLogging )
{
	IEntity * pEntity = m_pEntitySystem->GetEntity( entityId );
	if (!pEntity)
	{
		if (!GetISystem()->IsEditor())
			GameWarning( "Trying to synchronize non-existant entity %d", entityId );
		return eSOR_Failed;
	}

	switch (nAspect)
	{
	case eEA_GameClientStatic:
	case eEA_GameServerStatic:
	case eEA_GameClientDynamic:
	case eEA_GameServerDynamic:
		{
			IEntityProxy * pProxy = pEntity->GetProxy( ENTITY_PROXY_USER );
			if (!pProxy)
			{
				if (verboseLogging)
					GameWarning( "CGameContext::SynchObject: No user proxy with eEA_GameObject" );
				return eSOR_Failed;
			}
			CGameObject * pGameObject = (CGameObject *)pProxy;
			if (!pGameObject->NetSerialize( serialize, (EEntityAspects)nAspect, profile, 0 ))
			{
				if (verboseLogging)
					GameWarning("CGameContext::SynchObject: game fails to serialize aspect %d on profile %d", BitIndex(nAspect), int(profile));
				return eSOR_Failed;
			}
		}
		break;
	case eEA_Physics:
		{
			int pflags = 0;
			if (m_pPhysicsSync && serialize.IsReading())
			{
				if (m_pPhysicsSync->IgnoreSnapshot())
					return eSOR_Skip;
				else if (m_pPhysicsSync->NeedToCatchup())
					pflags |= ssf_compensate_time_diff;
			}
			IEntityProxy * pProxy = pEntity->GetProxy( ENTITY_PROXY_USER );
			if (pProxy)
			{
				CGameObject * pGameObject = (CGameObject *)pProxy;
				if (!pGameObject->NetSerialize( serialize, eEA_Physics, profile, pflags ))
				{
					if (verboseLogging)
						GameWarning("CGameContext::SynchObject: game fails to serialize physics aspect on profile %d", int(profile));
					return eSOR_Failed;
				}
			}
			else if (pProxy = pEntity->GetProxy( ENTITY_PROXY_PHYSICS ))
			{
				((IEntityPhysicalProxy*)pProxy)->Serialize( serialize );
			}
			if (m_pPhysicsSync && serialize.IsReading() && serialize.ShouldCommitValues())
				m_pPhysicsSync->UpdatedEntity( entityId );
		}
		break;
	case eEA_Script:
		{
			IEntityScriptProxy * pScriptProxy = static_cast<IEntityScriptProxy *>(pEntity->GetProxy( ENTITY_PROXY_SCRIPT ));
			if (pScriptProxy)
				pScriptProxy->Serialize(serialize);
			if (!m_pScriptRMI->SerializeScript( serialize, pEntity ))
			{
				if (verboseLogging)
					GameWarning("CGameContext::SynchObject: failed to serialize script aspect");
				return eSOR_Failed;
			}
		}
		break;
	default:
		;
//		GameWarning("Unknown aspect %d", nAspect);
//		return false;
	}
	return eSOR_Ok;
}

bool CGameContext::SetAspectProfile( EntityId id, uint8 aspectBit, uint8 profile )
{
	IEntity * pEntity = m_pEntitySystem->GetEntity( id );
	if (!pEntity)
	{
		GameWarning("Trying to set the profile of a non-existant entity %d", id);
		return true;
	}

	assert(0 == (aspectBit & (aspectBit-1)));

	IEntityProxy * pProxy = pEntity->GetProxy( ENTITY_PROXY_USER );
	if (pProxy)
	{
		CGameObject * pGameObject = (CGameObject *)pProxy;
		if (pGameObject->SetAspectProfile( (EEntityAspects)aspectBit, profile, true ))
			return true;
	}
	return false;
}

class CSpawnMsg : public INetSendableHook, private SBasicSpawnParams
{
public:
	CSpawnMsg( const SBasicSpawnParams& params, ISerializableInfoPtr pObj ) : 
			INetSendableHook(), 
				SBasicSpawnParams(params), 
				m_pObj(pObj) {}

	EMessageSendResult Send( INetSender * pSender )
	{
		pSender->BeginMessage( CGameClientChannel::DefaultSpawn );
		SBasicSpawnParams::SerializeWith(pSender->ser);
		if (m_pObj)
			m_pObj->SerializeWith(pSender->ser);
		return eMSR_SentOk;
	}

	void UpdateState( uint32 nFromSeq, ENetSendableStateUpdate )
	{
	}

	size_t GetSize()
	{
		return sizeof(*this);
	}

private:
	ISerializableInfoPtr m_pObj;
};

INetSendableHookPtr CGameContext::CreateObjectSpawner( EntityId entityId, INetChannel * pChannel )
{
	IEntity * pEntity = m_pEntitySystem->GetEntity( entityId );
	if (!pEntity)
		return NULL;

	uint16 channelId = ~uint16(0);
	if (pChannel)
	{
		IGameChannel * pIGameChannel = pChannel->GetGameChannel();
		CGameServerChannel * pGameServerChannel = (CGameServerChannel*) pIGameChannel;
		channelId = pGameServerChannel->GetChannelId();
	}

	IEntityProxy * pProxy = pEntity->GetProxy(ENTITY_PROXY_USER);

	CGameObject * pGameObject = reinterpret_cast<CGameObject*>( pProxy );
	if (pGameObject)
		pGameObject->InitClient(channelId);
	m_pScriptRMI->OnInitClient( channelId, pEntity );

	SBasicSpawnParams params;
	params.name = pEntity->GetName();
	ClassIdFromName( params.classId, pEntity->GetClass()->GetName() );
	params.pos = pEntity->GetPos();
	params.scale = pEntity->GetScale();
	params.rotation = pEntity->GetRotation();
	params.nChannelId = pGameObject? pGameObject->GetChannelId() : 0;

	params.bClientActor = pChannel?
		((CGameChannel*)pChannel->GetGameChannel())->GetPlayerId() == pEntity->GetId() : false;

	if (params.bClientActor)
		pChannel->DeclareWitness(entityId);

	return new CSpawnMsg(params, pGameObject->GetSpawnInfo());
}

bool CGameContext::SendPostSpawnObject( EntityId id, INetChannel * pINetChannel )
{
	IEntity * pEntity = m_pEntitySystem->GetEntity( id );
	if (!pEntity)
		return false;

	uint16 channelId = ~uint16(0);
	if (pINetChannel)
	{
		IGameChannel * pIGameChannel = pINetChannel->GetGameChannel();
		CGameServerChannel * pGameServerChannel = (CGameServerChannel*) pIGameChannel;
		channelId = pGameServerChannel->GetChannelId();
	}

	IEntityProxy * pProxy = pEntity->GetProxy(ENTITY_PROXY_USER);
	CGameObject * pGameObject = reinterpret_cast<CGameObject*>( pProxy );
	if (pGameObject)
		pGameObject->PostInitClient(channelId);
	m_pScriptRMI->OnPostInitClient( channelId, pEntity );

	return true;
}

void CGameContext::ControlObject( EntityId id, bool bHaveControl )
{
	m_controlledObjects.Set( id, bHaveControl );

	if (IEntity * pEntity = m_pEntitySystem->GetEntity( id ))
		if (CGameObject * pGameObject = (CGameObject*) pEntity->GetProxy( ENTITY_PROXY_USER ))
			pGameObject->SetAuthority( bHaveControl );

	SDelegateCallbackIndex idx = {id,bHaveControl};
	DelegateCallbacks::iterator iter = m_delegateCallbacks.lower_bound( idx );
	IScriptSystem * pSS = gEnv->pScriptSystem;
	while (iter != m_delegateCallbacks.end() && iter->first == idx)
	{
		DelegateCallbacks::iterator next = iter;
		++next;

		Script::Call( pSS, iter->second );
		pSS->ReleaseFunc( iter->second );
		m_delegateCallbacks.erase(iter);

		iter = next;
	}
}

void CGameContext::PassDemoPlaybackMappedOriginalServerPlayer(EntityId id)
{
	CCryAction::GetCryAction()->GetIActorSystem()->SetDemoPlaybackMappedOriginalServerPlayer(id);
}

void CGameContext::AddControlObjectCallback( EntityId id, bool willHaveControl, HSCRIPTFUNCTION func )
{
	SDelegateCallbackIndex idx = {id, willHaveControl};
	m_delegateCallbacks.insert( std::make_pair(idx, func) );
}

void CGameContext::UnboundObject( EntityId entityId )
{
	m_pEntitySystem->RemoveEntity( entityId, false );
}

INetAtSyncItem * CGameContext::HandleRMI( bool bClient, EntityId objID, uint8 funcID, TSerialize ser )
{
	return m_pScriptRMI->HandleRMI( bClient, objID, funcID, ser );
}

//
// IEntitySystemSink
//

bool CGameContext::OnBeforeSpawn(SEntitySpawnParams &params )
{
	if (m_isInLevelLoad && !gEnv->pSystem->IsEditor())
	{
		static const char prefix[] = "GameType_";
		static const size_t prefixLen = sizeof(prefix)-1;

		// if a layer prefixed with GameType_ exists,
		// then discard it if it's not the current game type
		if (!strnicmp(params.sLayerName, prefix, prefixLen))
		{
			if (IEntity *pGameRules=CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRulesEntity())
			{
				const char *currentType=pGameRules->GetClass()->GetName();
				const char *gameType=params.sLayerName+prefixLen;
				if (stricmp(gameType, currentType))
					return false;
			}
		}
	}

	return true;
}

void CGameContext::OnSpawn( IEntity *pEntity, SEntitySpawnParams &params )
{
	if (HasContextFlag(eGSF_ImmersiveMultiplayer) && m_pBreakReplicator.get())
		m_pBreakReplicator->OnSpawn( pEntity, params );

	if (params.nFlags & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY))
		return;
	if (pEntity->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY))
		return;

//	if (aspects & eEA_Physics)
//	{
//		aspects &= ~eEA_Volatile;
//	}

#if VERBOSE_TRACE_CONTEXT_SPAWNS
	CryLogAlways( "CGameContext: Spawn Entity" );
	CryLogAlways( "  name       : %s", params.sName );
	CryLogAlways( "  id         : 0x%.4x", params.id );
	CryLogAlways( "  flags      : 0x%.8x", params.nFlags );
	CryLogAlways( "  pos        : %.1f %.1f %.1f", params.vPosition.x, params.vPosition.y, params.vPosition.z );
	CryLogAlways( "  cls name   : %s", params.pClass->GetName() );
	CryLogAlways( "  cls flags  : 0x%.8x", params.pClass->GetFlags() );
	CryLogAlways( "  channel    : %d", channelId );
	CryLogAlways( "  net aspects: 0x%.8x", aspects );
#endif

	if (pEntity->GetProxy( ENTITY_PROXY_SCRIPT ))
	{
		m_pScriptRMI->SetupEntity( params.id, pEntity, true, true );
	}

	CallOnSpawnComplete(pEntity);

	bool calledBindToNetwork = false;
	if (m_isInLevelLoad && gEnv->bMultiplayer)
	{
		if (!pEntity->GetProxy(ENTITY_PROXY_USER))
		{
			IGameObject * pGO = CCryAction::GetCryAction()->GetIGameObjectSystem()->CreateGameObjectForEntity(pEntity->GetId());
			if (pGO)
			{
				//CryLog("Forcibly binding %s to network", params.sName);
				calledBindToNetwork = true;
				pGO->BindToNetwork();
			}
		}
	}

	if (!calledBindToNetwork)
	{
		if (CGameObject * pGO = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER))
			pGO->BindToNetwork(eBTNM_NowInitialized);
	}
}

void CGameContext::CompleteUnbind( EntityId id )
{
	CScopedRemoveObjectUnlock removeObjectUnlock(this);
	gEnv->pEntitySystem->RemoveEntity(id);
}

bool CGameContext::OnRemove( IEntity *pEntity )
{
	if (0 == m_removeObjectUnlocks)
	{
		if (!HasContextFlag(eGSF_Server) && m_pNetContext->IsBound(pEntity->GetId()))
		{
			GameWarning("Attempt to remove entity %s %.8x - disallowing", pEntity->GetName(), pEntity->GetId());
			return false;
		}
		else if(gEnv->bMultiplayer)
		{
			m_pNetContext->SafelyUnbind(pEntity->GetId());
			return false;
		}
	}

	if (m_pBreakReplicator.get())
		m_pBreakReplicator->OnRemove( pEntity );

	if (pEntity->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY | ENTITY_FLAG_UNREMOVABLE))
	{
		m_pNetContext->UnbindObject( pEntity->GetId() );
		return true;
	}

	bool ok;
	if (ok = m_pNetContext->UnbindObject( pEntity->GetId() ))
	{
		m_pScriptRMI->RemoveEntity(pEntity->GetId());

		if (CGameClientNub * pClientNub = m_pFramework->GetGameClientNub())
			if (CGameClientChannel * pClientChannel = pClientNub->GetGameClientChannel())
				if (pClientChannel->GetPlayerId() == pEntity->GetId())
					pClientChannel->ClearPlayer();
	}
	return ok;
}

void CGameContext::OnEvent( IEntity *pEntity, SEntityEvent &event )
{
	struct SGetEntId
	{
		ILINE SGetEntId(IEntity * pEntity) : m_pEntity(pEntity), m_id(0) {}

		ILINE operator EntityId()
		{
			if (!m_id && m_pEntity)
				m_id = m_pEntity->GetId();
			return m_id;
		}

		IEntity * m_pEntity;
		EntityId m_id;
	};
	SGetEntId entId(pEntity);

	struct SEntIsMP
	{
		ILINE SEntIsMP(IEntity * pEntity) : m_pEntity(pEntity), m_got(false) {}

		ILINE operator bool()
		{
			if (!m_got)
			{
				m_is = !(m_pEntity->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY));
				m_got = true;
			}
			return m_is;
		}

		IEntity * m_pEntity;
		bool m_got;
		bool m_is;
	};
	SEntIsMP entIsMP(pEntity);

	switch (event.event)
	{
	case ENTITY_EVENT_XFORM:
		if (gEnv->bMultiplayer && entIsMP)
		{
			bool doAspectUpdate = true;
			if (event.nParam[0] & ENTITY_XFORM_FROM_PARENT)
				doAspectUpdate = false;
			float drawDistance = -1;
			if (IEntityRenderProxy * pRP = (IEntityRenderProxy *)pEntity->GetProxy(ENTITY_PROXY_RENDER))
				if (IRenderNode * pRN = pRP->GetRenderNode())
					drawDistance = pRN->GetMaxViewDist();
			// position has changed, best let other people know about it
			// disabled volatile... see OnSpawn for reasoning
			if (doAspectUpdate)
				m_pNetContext->ChangedAspects( entId, /*eEA_Volatile |*/ eEA_Physics, NULL );
			m_pNetContext->ChangedTransform( entId, pEntity->GetWorldPos(), pEntity->GetWorldRotation(), drawDistance );
		}
		break;
	case ENTITY_EVENT_ENTER_SCRIPT_STATE:
		if (entIsMP)
		{
			m_pNetContext->ChangedAspects( entId, eEA_Script, NULL );
		}
		break;
	}
}

//
// physics synchronization
//

void CGameContext::OnCollision( const EventPhys * pEvent, void * )
{
	const EventPhysCollision *pCEvent = static_cast<const EventPhysCollision *>(pEvent);
	//IGameObject *pSrc = pCollision->iForeignData[0]==PHYS_FOREIGN_ID_ENTITY ? s_this->GetEntityGameObject((IEntity*)pCollision->pForeignData[0]):0;
	//IGameObject *pTrg = pCollision->iForeignData[1]==PHYS_FOREIGN_ID_ENTITY ? s_this->GetEntityGameObject((IEntity*)pCollision->pForeignData[1]):0;
	IEntity* pEntitySrc = GetEntity( pCEvent->iForeignData[0], pCEvent->pForeignData[0] );
	IEntity* pEntityTrg = GetEntity( pCEvent->iForeignData[1], pCEvent->pForeignData[1] );

	if (!pEntitySrc || !pEntityTrg)
		return;

	s_pGameContext->m_pNetContext->PulseObject(pEntitySrc->GetId(), 'bump');
	s_pGameContext->m_pNetContext->PulseObject(pEntityTrg->GetId(), 'bump');
}

//
// internal functions
//

bool CGameContext::RegisterClassName( const string& name, uint16 id )
{
	return m_classRegistry.RegisterClassName(name, id);
}

bool CGameContext::ClassIdFromName( uint16& id, const string& name ) const
{
	return m_classRegistry.ClassIdFromName(id, name);
}

bool CGameContext::ClassNameFromId( string& name, uint16 id ) const
{
	return m_classRegistry.ClassNameFromId(name, id);
}

void CGameContext::EnablePhysics( EntityId id, bool enable )
{
	EEntityAspects aspect;
	aspect = eEA_Physics;
// disabled... see OnSpawn
//	m_pNetContext->EnableAspects( id, eEA_Volatile, !enable );
	EnableAspects( id, aspect, enable );
}

void CGameContext::BoundObject( EntityId id, uint8 nAspects )
{
	// called by net system only on the client when a new object has been bound
	IEntity * pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (!pEntity)
	{
		GameWarning("[net] notification of binding non existant entity %.8x received", id);
		return;
	}
	CGameObject * pGameObject = (CGameObject*) pEntity->GetProxy(ENTITY_PROXY_USER);
	if (!pGameObject)
		return; // not a game object, things are ok
	pGameObject->BecomeBound();
}

bool CGameContext::ChangeContext( bool isServer, const SGameContextParams * params )
{
	if (CCryAction::GetCryAction()->GetILevelSystem()->IsLevelLoaded())
		m_loadFlags = EstablishmentFlags_LoadNextLevel;
	else
		m_loadFlags = EstablishmentFlags_InitialLoad;

	m_resetChannels.clear();

	if (HasContextFlag(eGSF_DemoRecorder) && HasContextFlag(eGSF_DemoPlayback))
	{
		GameWarning( "Cannot both playback a demo file and record a demo at the same time." );
		return false;
	}

	if (!HasContextFlag(eGSF_NoLevelLoading))
	{
		if (!params->levelName)
		{
			GameWarning( "No level specified: not changing context" );
			return false;
		}

		ILevelInfo * pLevelInfo = m_pFramework->GetILevelSystem()->GetLevelInfo( params->levelName );
		if (!pLevelInfo)
		{
			GameWarning( "Level %s not found", params->levelName );

			m_pFramework->GetILevelSystem()->OnLevelNotFound( params->levelName );

			return false;
		}

#ifndef _DEBUG
		string gameMode = params->gameRules;
		if((pLevelInfo->GetGameTypeCount()>0) && (gameMode != "SinglePlayer") && !pLevelInfo->SupportsGameType(gameMode) && !gEnv->pSystem->IsEditor() && !gEnv->pSystem->IsDevMode())
		{
			GameWarning( "Level %s does not support %s gamerules.", params->levelName, gameMode.c_str());

			return false;
		}
#endif
	}

	if (!HasContextFlag(eGSF_NoGameRules))
	{
		if (!params->gameRules)
		{
			GameWarning( "No rules specified: not changing context" );
			return false;
		}

		if (!m_pFramework->GetIGameRulesSystem()->HaveGameRules( params->gameRules ))
		{
			GameWarning( "Game rules %s not found; not changing context", params->gameRules );
			return false;
		}
	}

	if (params->levelName)
		m_levelName = params->levelName;
	else
		m_levelName.clear();
	if (params->gameRules)
		m_gameRules = params->gameRules;
	else
		m_gameRules.clear();

	//if (HasContextFlag(eGSF_Server) && HasContextFlag(eGSF_Client))
	while ( HasContextFlag(eGSF_Server) )
	{
		if ( HasContextFlag(eGSF_Client) && HasContextFlag(eGSF_DemoPlayback) )
		{
			if (params->demoPlaybackFilename == NULL)
				break;
			INetChannel * pClientChannel = m_pFramework->GetGameClientNub()->GetGameClientChannel()->GetNetChannel();
			INetChannel * pServerChannel = m_pFramework->GetGameServerNub()->GetChannel(1)->GetNetChannel();
			m_pNetContext->ActivateDemoPlayback( params->demoPlaybackFilename, pClientChannel, pServerChannel );
			break;
		}
		if ( HasContextFlag(eGSF_DemoRecorder) )
		{
			if (params->demoRecorderFilename == NULL)
				break;
			m_pNetContext->ActivateDemoRecorder( params->demoRecorderFilename );
			break;
		}

		break;
	}

	m_pNetContext->EnableBackgroundPassthrough( HasContextFlag(eGSF_Server) && gEnv->bMultiplayer );

	// start this thing off
	if (isServer)
	{
		if (!m_pNetContext->ChangeContext())
			return false;
	}

	return true;
}

bool CGameContext::Update()
{
	bool ret = false;

	float white[] = {1,1,1,1};
	if (!m_bHasSpawnPoint)
	{
		gEnv->pRenderer->Draw2dLabel( 10, 10, 4, white, false, "NO SPAWN POINT" );
	}
	static ICVar * pPacifist = 0;
	if (!pPacifist)
		pPacifist = gEnv->pConsole->GetCVar("sv_pacifist");
	if (gEnv->bClient && !gEnv->bServer && pPacifist->GetIVal() == 1)
	{
		gEnv->pRenderer->Draw2dLabel( 10, 10, 4, white, false, "PACIFIST MODE" );
	}

/*
	else if (m_bScheduledLevelLoad)
	{
		m_bScheduledLevelLoad = false;
		// if we're not the editor then we need to load a level
		bool oldLevelLoad = m_isInLevelLoad;
		m_isInLevelLoad = true;
		m_pFramework->GetIActionMapManager()->Enable(false); // diable action processing during loading
		if (!m_pFramework->GetILevelSystem()->LoadLevel(m_levelName.c_str()))
		{
			m_pFramework->EndGameContext();
			return false;
		}
		m_bHasSpawnPoint = HasSpawnPoint();
		m_pFramework->GetIActionMapManager()->Enable(true);
		m_isInLevelLoad = oldLevelLoad;
		m_pNetContext->EstablishedContext();

		if (!HasContextFlag(eGSF_Client))
		{
			StartGame();
			m_bStarted = true;
		}
	}
*/
	if(m_pVoiceController)
		m_pVoiceController->Enable(m_pFramework->IsVoiceRecordingEnabled());

	if (0 == (m_broadcastActionEventInGame -= (m_broadcastActionEventInGame != -1)))
		CCryAction::GetCryAction()->OnActionEvent(eAE_inGame);

	return ret;
}

bool CGameContext::OnBeforeVarChange( ICVar *pVar,const char *sNewValue )
{
	if (0 == (pVar->GetFlags() & VF_NOT_NET_SYNCED))
	{
		if (!HasContextFlag(eGSF_Server))
			CryLog("Server controlled CVar: %s", pVar->GetName());
		return HasContextFlag(eGSF_Server);
	}
	return true;
}

void CGameContext::OnAfterVarChange( ICVar *pVar )
{
}

static XmlNodeRef Child( XmlNodeRef& from, const char * name )
{
	if (XmlNodeRef found = from->findChild(name))
		return found;
	else
	{
		XmlNodeRef newNode = from->createNode(name);
		from->addChild(newNode);
		return newNode;
	}
}

XmlNodeRef CGameContext::GetGameState()
{
	CGameServerNub * pGameServerNub = m_pFramework->GetGameServerNub();

	XmlNodeRef root = GetISystem()->CreateXmlNode("root");

	// add server/network properties
	XmlNodeRef serverProperties = Child(root, "server");

	// add game properties
	XmlNodeRef gameProperties = Child(root, "game");
	gameProperties->setAttr( "levelName", m_levelName.c_str() );
	gameProperties->setAttr( "curPlayers", pGameServerNub->GetPlayerCount() );
	gameProperties->setAttr( "gameRules", m_gameRules.c_str() );
	char buffer[32];
	GetISystem()->GetProductVersion().ToShortString(buffer);
	gameProperties->setAttr( "version", string(buffer).c_str());
	gameProperties->setAttr( "maxPlayers", pGameServerNub->GetMaxPlayers() );

	return root;
}

void CGameContext::EnableAspects( EntityId id, uint8 aspects, bool bEnable )
{
	if (bEnable)
	{
		IEntity * pEntity = m_pEntitySystem->GetEntity(id);
		assert(pEntity);
		CGameObject * pGameObject = (CGameObject*) pEntity->GetProxy(ENTITY_PROXY_USER);
		if (pGameObject)
		{
			aspects &= pGameObject->GetEnabledAspects();
		}
	}
	m_pNetContext->EnableAspects( id, aspects, bEnable );
}

void CGameContext::OnStartNetworkFrame()
{
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->OnStartFrame();
}

void CGameContext::OnEndNetworkFrame()
{
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->OnEndFrame();
}

void CGameContext::OnBrokeSomething( const SBreakEvent& be, bool isPlane )
{
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->OnBrokeSomething( be, isPlane );
}

void CGameContext::ReconfigureGame( INetChannel * pNetChannel )
{
	CryLogAlways("Game reconfiguration: %s", pNetChannel->GetName());
}

void CGameContext::AllowCallOnClientConnect()
{
	m_bAllowSendClientConnect = true;
}

CTimeValue CGameContext::GetPhysicsTime()
{
	int tick = gEnv->pPhysicalWorld->GetiPhysicsTime();
	CTimeValue tm = tick * double(gEnv->pPhysicalWorld->GetPhysVars()->timeGranularity);
	return tm;
}

void CGameContext::BeginUpdateObjects( CTimeValue physTime, INetChannel * pChannel )
{
	if (pChannel)
		if (CGameChannel * pGameChannel = (CGameChannel*)pChannel->GetGameChannel())
			if (m_pPhysicsSync = pGameChannel->GetPhysicsSync())
				m_pPhysicsSync->OnPacketHeader(physTime);
}

void CGameContext::EndUpdateObjects()
{
	if (m_pPhysicsSync)
	{
		m_pPhysicsSync->OnPacketFooter();
		m_pPhysicsSync = 0;
	}
}

void CGameContext::PlayerIdSet(EntityId id)
{
	if(m_pVoiceController)
		m_pVoiceController->PlayerIdSet(id);
	if (IEntity * pEnt = gEnv->pEntitySystem->GetEntity(id))
	{
		pEnt->AddFlags(ENTITY_FLAG_LOCAL_PLAYER|ENTITY_FLAG_TRIGGER_AREAS);
		if (CGameObject * pGO = (CGameObject*) pEnt->GetProxy(ENTITY_PROXY_USER))
		{
			SGameObjectEvent goe(eGFE_BecomeLocalPlayer, eGOEF_ToAll);
			pGO->SendEvent(goe);
		}
	}
}

void CGameContext::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	if (m_pVoiceController)
		m_pVoiceController->GetMemoryStatistics(s);
	m_classRegistry.GetMemoryStatistics(s);
	m_controlledObjects.GetMemoryStatistics(s);
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->GetMemoryStatistics(s);
	s->Add(m_levelName);
	s->Add(m_gameRules);
	s->Add(m_connectionString);
	s->AddContainer(m_delegateCallbacks);
}

// src and trg can be the same pointer (in place encryption)
// len must be in bytes and must be multiple of 8 bytes (64bits).
// key is 128bit:  int key[4] = {n1,n2,n3,n4};
// void encipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k )
#define TEA_ENCODE( src,trg,len,key ) {\
	register unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	register unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) {\
	register unsigned int y=v[0],z=v[1],n=32,sum=0; \
	while(n-->0) { sum += delta; y += (z << 4)+a ^ z+sum ^ (z >> 5)+b; z += (y << 4)+c ^ y+sum ^ (y >> 5)+d; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

// src and trg can be the same pointer (in place decryption)
// len must be in bytes and must be multiple of 8 bytes (64bits).
// key is 128bit: int key[4] = {n1,n2,n3,n4};
// void decipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k)
#define TEA_DECODE( src,trg,len,key ) {\
	register unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	register unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) { \
	register unsigned int y=v[0],z=v[1],sum=0xC6EF3720,n=32; \
	while(n-->0) { z -= (y << 4)+c ^ y+sum ^ (y >> 5)+d; y -= (z << 4)+a ^ z+sum ^ (z >> 5)+b; sum -= delta; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

// encode size ignore last 3 bits of size in bytes. (encode by 8bytes min)
#define TEA_GETSIZE( len ) ((len) & (~7))

static ILINE bool IsDX10()
{
	ERenderType renderType = gEnv->pRenderer->GetRenderType();

	// the password is lemmings
	string unlock = 
		string(char(87)) +
		string(char(125)) +
		string(char(64)) +
		string(char(-41));
	std::vector<unsigned> key;
	key.push_back(622112183);

	if (ICVar * pVar = gEnv->pConsole->GetCVar("g_fake"))
	{
		key.push_back(2168041573U);
		if (const char * s = pVar->GetString())
		{
			key.push_back(1562214850);
			if (strlen(s)*strlen(s) == 64)
			{
				key.push_back(3891779665U);
				unsigned buf[2];
				unsigned * data = (unsigned *)s;
				unsigned * cmp = (unsigned *)&unlock[0];
				unlock +=
					string(char(-128)) +
					string(char(-13)) +
					string(char(8)) +
					string(char(-32));
				TEA_ENCODE(data, buf, TEA_GETSIZE(8), &key[0]);
				key.pop_back();
				if (0 == cmp[0] - buf[0] + cmp[1] - buf[1])
					renderType = eRT_DX10;
				key.pop_back();
			}
			key.pop_back();
		}
		key.pop_back();
	}

	return renderType == eRT_DX10;
}

static ILINE bool HasController()
{
	if (gEnv->pInput)
		return gEnv->pInput->HasInputDeviceOfType( eIDT_Gamepad );
	else
		return false;
}

static ILINE bool HasKeyboardMouse()
{
	if (gEnv->pInput)
		return gEnv->pInput->HasInputDeviceOfType( eIDT_Keyboard ) && gEnv->pInput->HasInputDeviceOfType( eIDT_Mouse );
	else
		return true;
}

static unsigned conkey[4] = {'eins', 'zwei', 'drei', 'vier'};

string CGameContext::GetConnectionString(bool fake) const
{
	string playerName;
	if (IPlayerProfileManager *pProfileManager=CCryAction::GetCryAction()->GetIPlayerProfileManager())
	{
		if (const char *user=pProfileManager->GetCurrentUser())
		{
			if (IPlayerProfile *profile=pProfileManager->GetCurrentProfile(user))
			{
				if(stricmp(profile->GetName(),"default")!=0)
					playerName=profile->GetName();
			}
		}
	}

	SModInfo info;
	string modName;
	if(CCryAction::GetCryAction()->GetModInfo(&info))
	{
		modName = info.m_name;
		modName += info.m_version;
	}

	char buf[128];
	string constr;
	constr += '!';
	gEnv->pSystem->GetProductVersion().ToShortString(buf);
	constr += buf;
	constr += ':';
	if (fake || IsDX10())
		constr += 'X';
	if (fake || HasController())
		constr += 'C';
	if (fake || HasKeyboardMouse())
		constr += 'K';
	constr += '#';
	constr += ToHexStr(modName.data(), modName.size()); 	// send mod name even if none
	constr += ':';
	constr += ToHexStr(playerName.data(), playerName.size());
	constr += ':';
	constr += fake ? "fake" : m_connectionString;
	sprintf(buf, "%d", (int)time(NULL));
	constr = buf + constr;
	int len = constr.length();
	int r = cry_rand32()%20 + CLAMP(64-len, 0, 64);
	for (int i=0; i<r || (constr.length()&7); i++)
	{
		char c = 'a' + cry_rand32()%26;
		constr = c + constr;
	}
	len = constr.length();
	unsigned int * data = (unsigned int*)constr.data();

	string out = ToHexStr(&constr[0], len);
	string temp = out;
	assert(FromHexStr(temp));
	assert(temp == constr);
	return out;
}

SParsedConnectionInfo CGameContext::ParseConnectionInfo( const string& sconst )
{
	SParsedConnectionInfo info;
	info.allowConnect = false;
	info.cause = eDC_GameError;

	string badFormat = "Illegal connection packet";
	string temp = sconst;
	if (!FromHexStr(temp))
	{
		info.errmsg = badFormat;
		return info;
	}

	int slen = temp.length();
	if (temp.length() > 511)
	{
		info.errmsg = badFormat;
		return info;
	}

	char s[512];
	memset(s, 0, 512);
	memcpy(s, temp.data(), TEA_GETSIZE(slen));

	char buf[128];
	gEnv->pSystem->GetProductVersion().ToShortString(buf);

	int i = 0;
	string versionMismatch = "Version mismatch";
	while (i < slen && s[i] != '!')
		++i;
	if (!s[i])
		return info;
	++i;
	int sbuf = i;
	for (; buf[i-sbuf]; i++)
	{
		if (slen <= i)
		{
			info.errmsg = badFormat;
			return info;
		}
		if (s[i] != buf[i-sbuf])
		{
			info.errmsg = versionMismatch;
			info.cause = eDC_VersionMismatch;
			return info;
		}
	}
	if (slen <= i)
	{
		info.errmsg = badFormat;
		return info;
	}
	if (s[i++] != ':')
	{
		info.errmsg = badFormat;
		return info;
	}

	bool isDX10 = false;
	bool hasController = false;
	bool hasKeyboardMouse = false;
	bool gotName=false;
	for (; i < slen; i++)
	{
		switch (s[i])
		{
		case 'X':
			isDX10 = true;
			break;
		case 'C':
			hasController = true;
			break;
		case 'K':
			hasKeyboardMouse = true;
			break;
		case '#':
			{
				// # is followed by 'modname:'
				string modName;
				int k=i+1;
				while (k<slen && s[k]!=':') ++k;
				modName=string(&s[i+1], k-i-1);
				i=k-1;

				FromHexStr(modName);
				string serverModName;
				SModInfo modInfo;
				if(CCryAction::GetCryAction()->GetModInfo(&modInfo))
				{
					serverModName = modInfo.m_name;
					serverModName += modInfo.m_version;
				}

				if(modName != serverModName)
				{
					if(!serverModName.empty())
					{
						// send back the mod name and version required to connect. This can be displayed
						//	to the user
						info.errmsg = modInfo.m_name;
						info.errmsg += " ";
						info.errmsg += modInfo.m_version;
					}
					else
					{
						// tell the client we're not running a mod
						info.errmsg = "None";
					}
						
					info.cause = eDC_ModMismatch;
					return info;
				}
				break;
			}
		case ':':
			if (!gotName)
			{
				gotName=true;
				int k=i+1;
				while (k<slen && s[k]!=':') ++k;
				info.playerName=string(&s[i+1], k-i-1);
				i=k-1;

				if (!FromHexStr(info.playerName))
				{
					info.errmsg = badFormat;
					return info;
				}
				break;
			}
			else
			{
				if (HasContextFlag(eGSF_RequireController) && !hasController)
				{
					info.errmsg = "Client has no game controller";
					info.cause = eDC_NoController;
					return info;
				}
				if (HasContextFlag(eGSF_RequireKeyboardMouse) && !hasKeyboardMouse)
				{
					info.errmsg = "Client has no keyboard/mouse";
					info.cause = eDC_GameError;
					return info;
				}
				if (HasContextFlag(eGSF_ImmersiveMultiplayer) && !isDX10)
				{
					info.errmsg = "Client is not DX10 capable";
					info.cause = eDC_NotDX10Capable;
					return info;
				}
				info.allowConnect = true;
				info.gameConnectionString = s + i + 1;
				return info;
			}
		default:
			info.errmsg = badFormat;
			return info;
		}
	}
	info.errmsg = badFormat;
	return info;
}

void CGameContext::DefineContextProtocols( IProtocolBuilder * pBuilder, bool server )
{
	if (m_pBreakReplicator.get())
	{
		m_pBreakReplicator->hack_defineProtocolMode_server = server;
		m_pBreakReplicator->DefineProtocol( pBuilder );
	}
}

void CGameContext::PlaybackBreakage( int breakId, INetBreakagePlaybackPtr pBreakage )
{
	if (m_pBreakReplicator.get())
		m_pBreakReplicator->PlaybackBreakage( breakId, pBreakage );
}

class CCVarUnprotect : public ICVarListProcessorCallback
{
  void OnCVar(ICVar* pV)
  {
    pV->SetFlags( pV->GetFlags() | VF_NOT_NET_SYNCED );
  }
};

void CGameContext::UnprotectCVars()
{
	CCVarListProcessor p(PathUtil::GetGameFolder() + "/Scripts/Network/cvars.txt");
	CCVarUnprotect cb;
  p.Process(&cb);
}

bool CGameContext::SetImmersive( bool immersive )
{
	if (immersive && !IsDX10())
		return false;
	if (immersive)
		m_flags |= eGSF_ImmersiveMultiplayer;
	else
		m_flags &= ~eGSF_ImmersiveMultiplayer;
	return true;
}
