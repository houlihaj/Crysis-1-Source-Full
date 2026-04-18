/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 11:8:2004   11:40 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "GameClientChannel.h"
#include "GameServerChannel.h"
#include "GameContext.h"
#include "CryAction.h"
#include "GameRulesSystem.h"
#include "GameObjects/GameObject.h"
#include "GameClientNub.h"
#include "ILevelSystem.h"
#include "IActorSystem.h"
#include "ActionGame.h"
#include "INetworkService.h"


#define LOCAL_ACTOR_VARIABLE "g_localActor"
#define LOCAL_ACTORID_VARIABLE "g_localActorId"

CGameClientChannel::CGameClientChannel(INetChannel *pNetChannel, CGameContext *pContext, CGameClientNub * pNub)
: m_pNub(pNub),
	m_hasLoadedLevel(false)
{
	pNetChannel->SetClient(pContext->GetNetContext(), true);
	Init( pNetChannel, pContext );

	INetChannel::SPerformanceMetrics pm;
	if (!gEnv->bMultiplayer)
		pm.pPacketRateDesired = gEnv->pConsole->GetCVar("g_localPacketRate");
	else
		pm.pPacketRateDesired = gEnv->pConsole->GetCVar("cl_packetRate");
	pm.pBitRateDesired = gEnv->pConsole->GetCVar("cl_bandwidth");
	pNetChannel->SetPerformanceMetrics(&pm);

	assert(pNetChannel);
	gEnv->pScriptSystem->SetGlobalToNull(LOCAL_ACTORID_VARIABLE);
	gEnv->pScriptSystem->SetGlobalToNull(LOCAL_ACTOR_VARIABLE);

  CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_channelCreated,0));
}

CGameClientChannel::~CGameClientChannel()
{
  CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_channelDestroyed,0));
	m_pNub->ClientChannelClosed();

	gEnv->pScriptSystem->SetGlobalToNull(LOCAL_ACTORID_VARIABLE);
	gEnv->pScriptSystem->SetGlobalToNull(LOCAL_ACTOR_VARIABLE);

	for (std::map<string,string>::iterator it = m_originalCVarValues.begin(); it != m_originalCVarValues.end(); ++it)
	{
		if (ICVar * pVar = gEnv->pConsole->GetCVar( it->first.c_str() ))
		{
			CryLog("Restore cvar '%s' to '%s'", it->first.c_str(), it->second.c_str());
			int flags = pVar->GetFlags();
			pVar->SetFlags( flags | VF_NOT_NET_SYNCED );
			pVar->ForceSet( it->second.c_str() );
			pVar->SetFlags( flags & ~VF_NOT_NET_SYNCED );
		}
	}
}

void CGameClientChannel::Release()
{
	delete this;
	CryLogAlways("CGameClientChannel::Release");
}

void CGameClientChannel::AddUpdateLevelLoaded( IContextEstablisher * pEst )
{
	if (!m_hasLoadedLevel)
		AddSetValue( pEst, eCVS_InGame, &m_hasLoadedLevel, true, "AllowChaining" );
}

bool CGameClientChannel::CheckLevelLoaded() const
{
	return m_hasLoadedLevel;
}

void CGameClientChannel::OnDisconnect(EDisconnectionCause cause, const char *description)
{
  //CryLogAlways("CGameClientChannel::OnDisconnect(%d, %s)", cause, description?description:"");
  CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_disconnected,int(cause),description));
  
  IGameRules *pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
	if (pGameRules)
		pGameRules->OnDisconnect(cause, description);

	if (IInput* pInput = gEnv->pInput)
	{
		pInput->EnableDevice(eDI_Keyboard, true);
		pInput->EnableDevice(eDI_Mouse, true);
	}
}

void CGameClientChannel::DefineProtocol(IProtocolBuilder *pBuilder)
{
	pBuilder->AddMessageSink(this, CGameServerChannel::GetProtocolDef(), CGameClientChannel::GetProtocolDef());
	CCryAction::GetCryAction()->GetIGameObjectSystem()->DefineProtocol( false, pBuilder );
	CCryAction::GetCryAction()->GetGameContext()->DefineContextProtocols(pBuilder, false);
	gEnv->pGame->ConfigureGameChannel(false, pBuilder);
}

// message implementation
NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameClientChannel, RegisterEntityClass, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return GetGameContext()->RegisterClassName( param.name, param.id );
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameClientChannel, SetGameType, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	string rulesClass;
	string levelName = param.levelName;
	if (!GetGameContext()->ClassNameFromId(rulesClass, param.rulesClass))
		return false;
	bool ok = true;
	if (!GetGameContext()->SetImmersive( param.immersive ))
		return false;

	// check level exists locally, and that it's checksum matches the server version; disconnect if not.
	if(gEnv->bMultiplayer && !gEnv->bServer)
	{
		// Save server information here - it might be possible to reconnect after we've 
		// downloaded a map, for instance. First time only though.
		bool firstConnect = false;
		if(!CCryAction::GetCryAction()->GetStoredServerInfo())
		{
			firstConnect = true;
			SServerConnectionInfo server;
			server.levelChecksum = param.levelChecksum;
			server.levelDownloadURL = param.levelDownloadURL;
			server.levelName = param.levelName;
			server.serverAddr = gEnv->pConsole->GetCVar("cl_serveraddr")->GetString();
			server.serverPort = gEnv->pConsole->GetCVar("cl_serverport")->GetString();
			CCryAction::GetCryAction()->StoreServerInfo(server);
		}

		uint64 nCode=CCryAction::GetCryAction()->GetILevelSystem()->CalcLevelChecksum(levelName);
		if(nCode != param.levelChecksum)
		{
			// disconnect here with correct reason. 
			if(firstConnect)
			{
				//If a URL was supplied, the frontend will attempt to 
				//	start the auto map download process, otherwise just show an error.
				if(nCode == 0)
					pNetChannel->Disconnect(eDC_MapNotFound, param.levelName);
				else
					pNetChannel->Disconnect(eDC_MapVersion, param.levelName);
			}
			else
			{
				SServerConnectionInfo* pInfo = CCryAction::GetCryAction()->GetStoredServerInfo();
				if(pInfo && pInfo->levelName != param.levelName)
				{
					// this was a reconnect attempt. Either the new map isn't found, or the version is wrong.
					// overwrite the stored information so we now download the correct map.
					pInfo->levelChecksum = param.levelChecksum;
					pInfo->levelDownloadURL = param.levelDownloadURL;
					pInfo->levelName = param.levelName;
					pNetChannel->Disconnect(eDC_ServerMapChanged_NewOneNotFound, param.levelName);
				}
				else
				{
					// reconnection to the same level, but still can't play (probably the download was wrong).
					//	Can't really do much more than disconnect with the original errors.
					//	TODO: do we want a specific error here?
			if(nCode == 0)
						pNetChannel->Disconnect(eDC_MapNotFound, param.levelName);
			else
						pNetChannel->Disconnect(eDC_MapVersion, param.levelName);
				}
			}

			return false;
		}
	}

	if (!bFromDemoSystem)
	{
		CryLogAlways( "Game rules class: %s", rulesClass.c_str() );
		SGameContextParams params;
		params.levelName = levelName.c_str();
		params.gameRules = rulesClass.c_str();
		ok = GetGameContext()->ChangeContext( false, &params );
	}
	else
	{
		GetGameContext()->SetLevelName(levelName.c_str());
		GetGameContext()->SetGameRules(rulesClass.c_str());
		CCryAction::GetCryAction()->GetIGameRulesSystem()->CreateGameRules(rulesClass.c_str()); // hack - since we don't do context establishment tasks when playing back demo
		ok = CCryAction::GetCryAction()->GetILevelSystem()->LoadLevel( levelName.c_str() ) != 0;
	}

	return ok;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CGameClientChannel, ResetMap, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	GetGameContext()->ResetMap(false);
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CGameClientChannel, DefaultSpawn, eNRT_UnreliableOrdered, eMPF_BlocksStateChange | eMPF_DecodeInSync)
{
	bool bFromDemoSystem = nCurSeq == DEMO_PLAYBACK_SEQ_NUMBER && nOldSeq == DEMO_PLAYBACK_SEQ_NUMBER;

	SBasicSpawnParams param;
	param.SerializeWith(ser);

	string actorClass;
	if (!GetGameContext()->ClassNameFromId(actorClass, param.classId))
		return false;

	IEntitySystem * pEntitySystem = gEnv->pEntitySystem;
	SEntitySpawnParams esp;
	esp.id = 0;

	esp.nFlags = 0;
	esp.pClass = pEntitySystem->GetClassRegistry()->FindClass( actorClass );
	if (!esp.pClass)
		return false;
	esp.pUserData = NULL;
	esp.qRotation = param.rotation;
	esp.sName = param.name.c_str();
	esp.vPosition	= param.pos;
	esp.vScale = param.scale;
	CCryAction::GetCryAction()->GetIGameObjectSystem()->SetSpawnSerializer(&ser);
	if (IEntity * pEntity = pEntitySystem->SpawnEntity( esp ))
	{
		CGameObject * pGameObject = (CGameObject*)CCryAction::GetCryAction()->GetIGameObjectSystem()->CreateGameObjectForEntity(pEntity->GetId());
		assert(pGameObject);
		if (!bFromDemoSystem)
			pGameObject->SetChannelId( param.nChannelId );
		else
			pGameObject->SetChannelId( param.nChannelId != 0 ? -1 : 0 );
		if (param.bClientActor)
		{
			IActor * pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor( pEntity->GetId() );
			if (!pActor)
			{
				pEntitySystem->RemoveEntity( pEntity->GetId() );
				CCryAction::GetCryAction()->GetIGameObjectSystem()->SetSpawnSerializer(NULL);
				pNetChannel->Disconnect(eDC_ContextCorruption, "Client actor spawned entity was not an actor");
				return false;
			}
			SetPlayerId( pGameObject->GetEntityId() );
		}
		GetGameContext()->GetNetContext()->SpawnedObject( pEntity->GetId() );
		CCryAction::GetCryAction()->GetIGameObjectSystem()->SetSpawnSerializer(NULL);
		pGameObject->PostRemoteSpawn();
		return true;
	}
	pNetChannel->Disconnect(eDC_ContextCorruption, "Failed to spawn entity");
	CCryAction::GetCryAction()->GetIGameObjectSystem()->SetSpawnSerializer(NULL);
	return false;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE( CGameClientChannel, SetPlayerId_LocalOnly, eNRT_ReliableUnordered, eMPF_BlocksStateChange )
{
	if (!GetNetChannel()->IsLocal())
		return false;
	SetPlayerId( param.id );
	return true;
}

NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE( CGameClientChannel, SetConsoleVariable, eNRT_ReliableUnordered, eMPF_BlocksStateChange )
{
	if (GetNetChannel()->IsLocal() && !bFromDemoSystem)
		return false;

	CryLog("Server sets console variable '%s' to '%s'", param.key.c_str(), param.value.c_str());

	IConsole * pConsole = gEnv->pConsole;
	ICVar * pVar = pConsole->GetCVar( param.key.c_str() );
	if (!pVar)
	{
		CryLog("   cvar not found, ignoring");
		return true;
	}
	int flags = pVar->GetFlags();
	if (flags & VF_NOT_NET_SYNCED)
	{
		CryLog("   cvar not synchronized, disconnecting");
		return false;
	}

	std::map<string, string>::iterator orit = m_originalCVarValues.lower_bound(param.key);
	if (orit == m_originalCVarValues.end() || orit->first != param.key)
		m_originalCVarValues.insert( std::make_pair(param.key, pVar->GetString()) );

	pVar->SetFlags( flags | VF_NOT_NET_SYNCED );
	pVar->ForceSet( param.value.c_str() );
	pVar->SetFlags( flags & ~VF_NOT_NET_SYNCED );
	return true;
}

void CGameClientChannel::SetPlayerId( EntityId id )
{
	if (gEnv->pGame)
		gEnv->pGame->PlayerIdSet(id);

	CGameChannel::SetPlayerId( id );

	CCryAction::GetCryAction()->GetGameContext()->PlayerIdSet(id);
	
	if (id)
	{
		ScriptHandle hdl;
		hdl.n = GetPlayerId();
		IScriptSystem * pSS = gEnv->pScriptSystem;
		pSS->SetGlobalValue( LOCAL_ACTORID_VARIABLE, hdl );
		IEntity * pEntity = gEnv->pEntitySystem->GetEntity(id);
		if (pEntity)
		{
			pSS->SetGlobalValue( LOCAL_ACTOR_VARIABLE, pEntity->GetScriptTable() );
		}
		else
		{
			pSS->SetGlobalToNull( LOCAL_ACTOR_VARIABLE );
		}

		CallOnSetPlayerId();
	}
	else
	{
		gEnv->pScriptSystem->SetGlobalToNull( LOCAL_ACTORID_VARIABLE );
		gEnv->pScriptSystem->SetGlobalToNull( LOCAL_ACTOR_VARIABLE );
	}
}

void CGameClientChannel::CallOnSetPlayerId()
{
	IEntity	*pPlayer = gEnv->pEntitySystem->GetEntity(GetPlayerId());
	if (!pPlayer)
		return;

	IScriptTable *pScriptTable = pPlayer->GetScriptTable();
	if (!pScriptTable)
		return;
	
	SmartScriptTable client;
	if (pScriptTable->GetValue("Client", client))
	{
		if (pScriptTable->GetScriptSystem()->BeginCall(client, "OnSetPlayerId"))
		{
			pScriptTable->GetScriptSystem()->PushFuncParam(pScriptTable);
			pScriptTable->GetScriptSystem()->EndCall();
		}
	}

	IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(GetPlayerId());
	assert(pActor);

	pActor->InitLocalPlayer();
}
