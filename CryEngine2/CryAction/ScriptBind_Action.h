#ifndef __SCRIPTBIND_ACTION__
#define __SCRIPTBIND_ACTION__

#pragma once

#include <IScriptSystem.h>
#include <ScriptHelpers.h>

// FIXME: Cell SDK GCC bug workaround.
#ifndef __IGAMEOBJECTSYSTEM_H__
#include "IGameObjectSystem.h"
#endif

class CCryAction;
class CScriptBind_Action :
	public CScriptableBase
{
public:
	CScriptBind_Action(CCryAction *pCryAction);
	virtual ~CScriptBind_Action();

	void Release() { delete this; };

	int LoadXML( IFunctionHandler *pH, const char * definitionFile, const char * dataFile );
	int SaveXML( IFunctionHandler *pH, const char * definitionFile, const char * dataFile, SmartScriptTable dataTable );
	int IsServer( IFunctionHandler *pH );
	int IsClient( IFunctionHandler *pH );
	int IsGameStarted( IFunctionHandler *pH );
	int GetPlayerList( IFunctionHandler *pH );
	int IsGameObjectProbablyVisible( IFunctionHandler *pH, ScriptHandle gameObject );
	int ActivateEffect(IFunctionHandler *pH, const char * name );
	int GetWaterInfo(IFunctionHandler *pH, Vec3 pos );
	int GetServer( IFunctionHandler *pFH, int number );
	int RefreshPings( IFunctionHandler *pFH );
	int ConnectToServer( IFunctionHandler *pFH, char* server );
	int GetServerTime( IFunctionHandler *pFH );
	int PauseGame( IFunctionHandler *pFH, bool pause );
	int IsImmersivenessEnabled( IFunctionHandler * pH );
	int IsChannelSpecial( IFunctionHandler * pH );

	int ForceGameObjectUpdate( IFunctionHandler *pH, ScriptHandle entityId, bool force );
	int CreateGameObjectForEntity( IFunctionHandler *pH, ScriptHandle entityId );
	int BindGameObjectToNetwork(  IFunctionHandler *pH, ScriptHandle entityId );
	int ActivateExtensionForGameObject( IFunctionHandler *pH, ScriptHandle entityId, const char *extension, bool activate );
	int SetNetworkParent( IFunctionHandler *pH, ScriptHandle entityId, ScriptHandle parentId );
	int IsChannelOnHold( IFunctionHandler *pH, int channelId );
	int BanPlayer( IFunctionHandler *pH, ScriptHandle entityId, const char* message );

  int PersistantSphere(IFunctionHandler* pH, Vec3 pos, float radius, Vec3 color, const char* name, float timeout);
  int PersistantLine(IFunctionHandler* pH, Vec3 start, Vec3 end, Vec3 color, const char* name, float timeout);
	int PersistantArrow(IFunctionHandler* pH, Vec3 pos, float radius, Vec3 dir, Vec3 color, const char* name, float timeout);
  int Persistant2DText(IFunctionHandler* pH, const char* text, float size, Vec3 color, const char* name, float timeout);

	int SendGameplayEvent(IFunctionHandler* pH, ScriptHandle entityId, int event);

	int CacheItemSound(IFunctionHandler* pH, const char *itemName);
	int CacheItemGeometry(IFunctionHandler* pH, const char *itemName);

	int DontSyncPhysics(IFunctionHandler* pH, ScriptHandle entityId);

private:
	void RegisterGlobals();
	void RegisterMethods();
	
	CCryAction *m_pCryAction;
};

#endif
