#include "StdAfx.h"
#include "GameObjects/GameObject.h"
#include "ScriptBind_Action.h"
#include "Serialization/XMLScriptLoader.h"
#include "CryAction.h"
#include "Network/GameServerChannel.h"
#include "Network/GameServerNub.h"
#include "Network/GameContext.h"
#include <IEntitySystem.h>

#include <IGameTokens.h>
#include <IEffectSystem.h>
#include <IGameplayRecorder.h>
#include "PersistantDebug.h"

//------------------------------------------------------------------------
CScriptBind_Action::CScriptBind_Action(CCryAction *pCryAction)
: m_pCryAction(pCryAction)
{
	Init( gEnv->pScriptSystem, GetISystem() );
	SetGlobalName("CryAction");

	RegisterGlobals();
	RegisterMethods();
}

//------------------------------------------------------------------------
CScriptBind_Action::~CScriptBind_Action()
{
}

//------------------------------------------------------------------------
void CScriptBind_Action::RegisterGlobals()
{
	SCRIPT_REG_GLOBAL(eGE_DiscreetSample);
	SCRIPT_REG_GLOBAL(eGE_GameReset);
	SCRIPT_REG_GLOBAL(eGE_GameStarted);
	SCRIPT_REG_GLOBAL(eGE_GameEnd);
	SCRIPT_REG_GLOBAL(eGE_Connected);
	SCRIPT_REG_GLOBAL(eGE_Disconnected);
	SCRIPT_REG_GLOBAL(eGE_Renamed);
	SCRIPT_REG_GLOBAL(eGE_ChangedTeam);
	SCRIPT_REG_GLOBAL(eGE_Death);
	SCRIPT_REG_GLOBAL(eGE_Scored);
	SCRIPT_REG_GLOBAL(eGE_Currency);
	SCRIPT_REG_GLOBAL(eGE_Rank);
	SCRIPT_REG_GLOBAL(eGE_Spectator);
	SCRIPT_REG_GLOBAL(eGE_ScoreReset);
}

//------------------------------------------------------------------------
void CScriptBind_Action::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Action::

	SCRIPT_REG_TEMPLFUNC(LoadXML, "definitionFile, dataFile");
	SCRIPT_REG_TEMPLFUNC(SaveXML, "definitionFile, dataFile, dataTable");
	SCRIPT_REG_TEMPLFUNC(IsServer, "");
	SCRIPT_REG_TEMPLFUNC(IsClient, "");
	SCRIPT_REG_TEMPLFUNC(IsGameStarted, "");
	SCRIPT_REG_TEMPLFUNC(IsGameObjectProbablyVisible, "entityId");
	SCRIPT_REG_TEMPLFUNC(GetPlayerList, "");
	SCRIPT_REG_TEMPLFUNC(ActivateEffect, "name");
	SCRIPT_REG_TEMPLFUNC(GetWaterInfo, "pos");
	SCRIPT_REG_TEMPLFUNC(GetServer, "number");
	SCRIPT_REG_TEMPLFUNC(ConnectToServer, "server");
	SCRIPT_REG_TEMPLFUNC(RefreshPings, "");
	SCRIPT_REG_TEMPLFUNC(GetServerTime, "");
	SCRIPT_REG_TEMPLFUNC(PauseGame, "pause");
	SCRIPT_REG_TEMPLFUNC(IsImmersivenessEnabled, "");
	SCRIPT_REG_TEMPLFUNC(IsChannelSpecial, "entityId/channelId");


	SCRIPT_REG_TEMPLFUNC(ForceGameObjectUpdate, "entityId, force");
	SCRIPT_REG_TEMPLFUNC(CreateGameObjectForEntity, "entityId");
	SCRIPT_REG_TEMPLFUNC(BindGameObjectToNetwork, "entityId");
	SCRIPT_REG_TEMPLFUNC(ActivateExtensionForGameObject, "entityId, extension, activate");
	SCRIPT_REG_TEMPLFUNC(SetNetworkParent, "entityId, parentId");
	SCRIPT_REG_TEMPLFUNC(IsChannelOnHold, "channelId");
	SCRIPT_REG_TEMPLFUNC(BanPlayer, "playerId, message");

  SCRIPT_REG_TEMPLFUNC(PersistantSphere, "pos, radius, color, name, timeout");
  SCRIPT_REG_TEMPLFUNC(PersistantLine, "start, end, color, name, timeout");
  SCRIPT_REG_TEMPLFUNC(PersistantArrow, "pos, radius, color, dir, name, timeout");
  SCRIPT_REG_TEMPLFUNC(Persistant2DText, "text, size, color, name, timeout");

	SCRIPT_REG_TEMPLFUNC(SendGameplayEvent, "entityId, event, [desc], [value]");

	SCRIPT_REG_TEMPLFUNC(CacheItemSound, "itemName");
	SCRIPT_REG_TEMPLFUNC(CacheItemGeometry, "itemName");

	SCRIPT_REG_TEMPLFUNC(DontSyncPhysics, "entityId");
}

int CScriptBind_Action::LoadXML( IFunctionHandler *pH, const char * definitionFile, const char * dataFile )
{
	return pH->EndFunction( XmlScriptLoad( definitionFile, dataFile ) );
}

int CScriptBind_Action::SaveXML( IFunctionHandler *pH, const char * definitionFile, const char * dataFile, SmartScriptTable dataTable )
{
	return pH->EndFunction( XmlScriptSave( definitionFile, dataFile, dataTable ) );
}

int CScriptBind_Action::IsGameStarted( IFunctionHandler * pH )
{
	return pH->EndFunction(m_pCryAction->IsGameStarted());
}

int CScriptBind_Action::IsImmersivenessEnabled( IFunctionHandler * pH )
{
	int out = 0;
	if (!gEnv->bMultiplayer)
		out = 1;
	else if (CGameContext * pGC = CCryAction::GetCryAction()->GetGameContext())
		if (pGC->HasContextFlag(eGSF_ImmersiveMultiplayer))
			out = 1;
	return pH->EndFunction(out);
}

int CScriptBind_Action::IsChannelSpecial( IFunctionHandler * pH )
{
	INetChannel *pChannel=0;

	if (pH->GetParamCount()>0)
	{
		if (pH->GetParamType(1)==svtNumber)
		{
			int channelId=0;
			if (pH->GetParam(1, channelId))
			{
				if (CGameServerChannel *pGameChannel=CCryAction::GetCryAction()->GetGameServerNub()->GetChannel(channelId))
					pChannel=pGameChannel->GetNetChannel();
			}
		}
		else if (pH->GetParamType(1)==svtPointer)
		{
			ScriptHandle entityId;
			if (pH->GetParam(1, entityId))
			{
				if (IActor *pActor=CCryAction::GetCryAction()->GetIActorSystem()->GetActor((EntityId)entityId.n))
				{
					int channelId=pActor->GetChannelId();
					if (CGameServerChannel *pGameChannel=CCryAction::GetCryAction()->GetGameServerNub()->GetChannel(channelId))
						pChannel=pGameChannel->GetNetChannel();
				}
			}
		}

		if (pChannel && pChannel->IsPreordered())
			return pH->EndFunction(true);
	}

	return pH->EndFunction();
}


int CScriptBind_Action::IsClient(IFunctionHandler *pH)
{
	return pH->EndFunction(gEnv->bClient);
}

int CScriptBind_Action::IsServer(IFunctionHandler *pH)
{
	return pH->EndFunction(gEnv->bServer);
}

int CScriptBind_Action::GetPlayerList( IFunctionHandler *pH )
{
	CGameServerNub * pNub = m_pCryAction->GetGameServerNub();
	if (!pNub)
	{
		GameWarning("No game server nub");
		return pH->EndFunction();
	}
	TServerChannelMap *playerMap = m_pCryAction->GetGameServerNub()->GetServerChannelMap();
	if (!playerMap)
		return pH->EndFunction();

	IEntitySystem *pES = gEnv->pEntitySystem;

	int	k=1;
	SmartScriptTable playerList(m_pSS);
	
	for (TServerChannelMap::iterator it = playerMap->begin(); it != playerMap->end(); it++)
	{
		EntityId playerId = it->second->GetPlayerId();
		
		if (!playerId)
			continue;

		IEntity *pPlayer = pES->GetEntity(playerId);
		if (!pPlayer)
			continue;
		if (pPlayer->GetScriptTable())
			playerList->SetAt(k++, pPlayer->GetScriptTable());
	}

	return pH->EndFunction(*playerList);
}

int CScriptBind_Action::IsGameObjectProbablyVisible( IFunctionHandler *pH, ScriptHandle gameObject )
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(gameObject.n);
	if (pEntity)
	{
		CGameObject *pGameObject = static_cast<CGameObject *>(pEntity->GetProxy(ENTITY_PROXY_USER));
		if (pGameObject && pGameObject->IsProbablyVisible())
			return pH->EndFunction(1);
	}
	return pH->EndFunction();
}


int CScriptBind_Action::ActivateEffect( IFunctionHandler *pH, const char * name)
{
	int i = CCryAction::GetCryAction()->GetIEffectSystem()->GetEffectId(name);
	CCryAction::GetCryAction()->GetIEffectSystem()->Activate(i);
	return pH->EndFunction();
}

int CScriptBind_Action::GetWaterInfo(IFunctionHandler *pH, Vec3 pos)
{
	const int mb=8;
	pe_params_buoyancy buoyancy[mb];
	Vec3 gravity;

	if (int n=gEnv->pPhysicalWorld->CheckAreas(pos, gravity, buoyancy, mb))
	{
		for (int i=0;i<n;i++)
		{
			if (buoyancy[i].iMedium==0)	// 0==water
			{
				return pH->EndFunction(buoyancy[i].waterPlane.origin.z,
					Script::SetCachedVector(buoyancy[i].waterPlane.n, pH, 2), Script::SetCachedVector(buoyancy[i].waterFlow, pH, 2));
			}
		}
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::GetServer( IFunctionHandler *pFH, int number )
{
	char* server = 0;
	int ping = 9999;
	char* data = 0;

	ILanQueryListener *pLanQueryListener = m_pCryAction->GetILanQueryListener();
	IGameQueryListener *pGameQueryListener = NULL;
	if(pLanQueryListener)
		pGameQueryListener = pLanQueryListener->GetGameQueryListener();
	if(pGameQueryListener)
		pGameQueryListener->GetServer(number, &server, &data, ping);

	return pFH->EndFunction(server, data, ping);
}

//------------------------------------------------------------------------
int CScriptBind_Action::RefreshPings(IFunctionHandler *pFH )
{
	ILanQueryListener *pLanQueryListener = m_pCryAction->GetILanQueryListener();
	IGameQueryListener *pGameQueryListener = NULL;
	if(pLanQueryListener)
		pGameQueryListener = pLanQueryListener->GetGameQueryListener();
	if(pGameQueryListener)
		pGameQueryListener->RefreshPings();
	return pFH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::ConnectToServer( IFunctionHandler *pFH, char* server )
{
	ILanQueryListener *pLanQueryListener = m_pCryAction->GetILanQueryListener();
	if(pLanQueryListener)
		pLanQueryListener->GetGameQueryListener()->ConnectToServer(server);
	return pFH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::GetServerTime( IFunctionHandler *pFH )
{
	return pFH->EndFunction(m_pCryAction->GetServerTime().GetSeconds());
}

//------------------------------------------------------------------------
int CScriptBind_Action::ForceGameObjectUpdate( IFunctionHandler *pH, ScriptHandle entityId, bool force )
{
	if (IGameObject *pGameObject=CCryAction::GetCryAction()->GetGameObject((EntityId)entityId.n))
		pGameObject->ForceUpdate(force);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::CreateGameObjectForEntity( IFunctionHandler *pH, ScriptHandle entityId )
{
	CCryAction::GetCryAction()->GetIGameObjectSystem()->CreateGameObjectForEntity((EntityId)entityId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::BindGameObjectToNetwork(  IFunctionHandler *pH, ScriptHandle entityId )
{
	if (IGameObject *pGameObject=CCryAction::GetCryAction()->GetGameObject((EntityId)entityId.n))
		pGameObject->BindToNetwork();
	
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::ActivateExtensionForGameObject( IFunctionHandler *pH, ScriptHandle entityId, const char *extension, bool activate )
{
	if (IGameObject *pGameObject=CCryAction::GetCryAction()->GetGameObject((EntityId)entityId.n))
	{
		if (activate)
			pGameObject->ActivateExtension(extension);
		else
			pGameObject->DeactivateExtension(extension);
	}

	return pH->EndFunction();

}

//------------------------------------------------------------------------
int CScriptBind_Action::SetNetworkParent( IFunctionHandler *pH, ScriptHandle entityId, ScriptHandle parentId )
{
	if (IGameObject *pGameObject=CCryAction::GetCryAction()->GetGameObject((EntityId)entityId.n))
		pGameObject->SetNetworkParent((EntityId)parentId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::IsChannelOnHold(IFunctionHandler *pH, int channelId )
{
	if (CGameServerChannel *pServerChannel=CCryAction::GetCryAction()->GetGameServerNub()->GetChannel(channelId))
		return pH->EndFunction(pServerChannel->IsOnHold());
	return pH->EndFunction();
}

int CScriptBind_Action::BanPlayer( IFunctionHandler *pH, ScriptHandle entityId, const char* message )
{
	if (IActor* pAct = CCryAction::GetCryAction()->GetIActorSystem()->GetActor((EntityId)entityId.n) )
	{
		CCryAction::GetCryAction()->GetGameServerNub()->BanPlayer(pAct->GetChannelId(),message);		
	}
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::PauseGame( IFunctionHandler *pH, bool pause )
{
	bool forced = false;

	if (pH->GetParamCount() > 1)
	{
		pH->GetParam(2, forced);
	}
	CCryAction::GetCryAction()->PauseGame(pause, forced);

	return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Action::PersistantSphere(IFunctionHandler* pH, Vec3 pos, float radius, Vec3 color, const char* name, float timeout)
{
  IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();

  pPD->Begin(name, false);
  pPD->AddSphere(pos, radius, ColorF(color, 1.f), timeout);

  return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Action::PersistantLine(IFunctionHandler* pH, Vec3 start, Vec3 end, Vec3 color, const char* name, float timeout)
{
  IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();
  
  pPD->Begin(name, false);
  pPD->AddLine(start, end, ColorF(color, 1.f), timeout);

  return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Action::PersistantArrow(IFunctionHandler* pH, Vec3 pos, float radius, Vec3 dir, Vec3 color, const char* name, float timeout)
{
  IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();

  pPD->Begin(name, false);
  pPD->AddDirection(pos, radius, dir, ColorF(color, 1.f), timeout);

  return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Action::Persistant2DText(IFunctionHandler* pH, const char* text, float size, Vec3 color, const char* name, float timeout)
{
  IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();

  pPD->Begin(name, false);
  pPD->Add2DText(text, size, ColorF(color, 1.f), timeout);

  return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::SendGameplayEvent(IFunctionHandler* pH, ScriptHandle entityId, int event)
{
	const char *desc=0;
	float value=0.0f;

	if (pH->GetParamCount()>2 && pH->GetParamType(3)==svtString)
		pH->GetParam(3, desc);
	if (pH->GetParamCount()>3 && pH->GetParamType(4)==svtNumber)
		pH->GetParam(4, value);

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n);
	CCryAction::GetCryAction()->GetIGameplayRecorder()->Event(pEntity, GameplayEvent(event, desc, value));

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::CacheItemSound(IFunctionHandler* pH, const char *itemName)
{
	CCryAction::GetCryAction()->GetIItemSystem()->CacheItemSound(itemName);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::CacheItemGeometry(IFunctionHandler* pH, const char *itemName)
{
	CCryAction::GetCryAction()->GetIItemSystem()->CacheItemGeometry(itemName);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Action::DontSyncPhysics(IFunctionHandler* pH, ScriptHandle entityId)
{
	if (CGameObject * pGO = static_cast<CGameObject*>(CCryAction::GetCryAction()->GetIGameObjectSystem()->CreateGameObjectForEntity(entityId.n)))
		pGO->DontSyncPhysics();
	else
		GameWarning("DontSyncPhysics: Unable to find entity %d", entityId.n);
	return pH->EndFunction();
}
