/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 3:9:2004   11:24 : Created by Márcio Martins

*************************************************************************/
#ifndef __GameContext_H__
#define __GameContext_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameFramework.h"
#include "IEntitySystem.h"
#include "IScriptSystem.h"
#include "SensedInclusionSet.h"
#include "ClassRegistryReplicator.h"
#include "INetworkService.h"

// FIXME: Cell SDK GCC bug workaround.
#ifndef __GAMEOBJECT_H__
#include "GameObjects/GameObject.h"
#endif

class CScriptRMI;
class CPhysicsSync;
class CGameServerChannel;
class CActionGame;
class CVoiceController;

struct SBreakEvent;
class CBreakReplicator;

struct SParsedConnectionInfo
{
	bool allowConnect;
	EDisconnectionCause cause;
	string gameConnectionString;
	string errmsg;
	string playerName;
};

class CCryAction;
class CGameContext :
	public IGameContext,
	public IGameQuery,
	public IEntitySystemSink,
	public IConsoleVarSink
{
public:
	CGameContext( CCryAction * pFramework, CScriptRMI * , CActionGame *pGame);
	virtual ~CGameContext();

	void Init( INetContext * pNetContext );

	// IGameContext
	virtual bool InitGlobalEstablishmentTasks( IContextEstablisher * pEst, int establishedToken );
	virtual bool InitChannelEstablishmentTasks( IContextEstablisher * pEst, INetChannel * pChannel, int establishedToken );
	virtual ESynchObjectResult SynchObject( EntityId id, uint8 nAspect, uint8 currentProfile, TSerialize pSerialize, bool verboseLogging );
	virtual INetSendableHookPtr CreateObjectSpawner( EntityId id, INetChannel * pChannel );
	virtual bool SendPostSpawnObject( EntityId id, INetChannel * pChannel );
	virtual uint8 GetDefaultProfileForAspect( EntityId id, uint8 aspectID );
	virtual bool SetAspectProfile( EntityId id, uint8 nAspect, uint8 profile );
	virtual void BoundObject( EntityId id, uint8 nAspects );
	virtual void UnboundObject( EntityId id );
	virtual INetAtSyncItem * HandleRMI( bool bClient, EntityId objID, uint8 funcID, TSerialize ser );
	virtual void ControlObject( EntityId id, bool bHaveControl );
	virtual void PassDemoPlaybackMappedOriginalServerPlayer(EntityId id);
	virtual void OnStartNetworkFrame();
	virtual void OnEndNetworkFrame();
	virtual void ReconfigureGame( INetChannel * pNetChannel );
	virtual uint32 HashAspect( EntityId id, uint8 nAspect );
	virtual CTimeValue GetPhysicsTime();
	virtual void BeginUpdateObjects( CTimeValue physTime, INetChannel * pChannel );
	virtual void EndUpdateObjects();
	virtual void PlaybackBreakage( int breakId, INetBreakagePlaybackPtr pBreakage );
	virtual void CompleteUnbind( EntityId id );
	// ~IGameContext

	// IGameQuery
	XmlNodeRef GetGameState();
	// ~IGameQuery

	// IEntitySystemSink
	virtual bool OnBeforeSpawn( SEntitySpawnParams &params );
	virtual void OnSpawn( IEntity *pEntity,SEntitySpawnParams &params );
	virtual bool OnRemove( IEntity *pEntity );
	virtual void OnEvent( IEntity *pEntity, SEntityEvent &event );
	// ~IEntitySystemSink

	// IConsoleVarSink
	virtual bool OnBeforeVarChange( ICVar *pVar,const char *sNewValue );
	virtual void OnAfterVarChange( ICVar *pVar );
	// ~IConsoleVarSink

	INetContext * GetNetContext() { return m_pNetContext; }
	CCryAction * GetFramework() { return m_pFramework; }
	bool IsGameStarted() const { return m_bStarted; }
	bool IsInLevelLoad() const { return m_isInLevelLoad; }
	bool IsLoadingSaveGame() const { return m_bIsLoadingSaveGame; };

	SParsedConnectionInfo ParseConnectionInfo( const string& s );

	void SetLoadingSaveGame(bool isLoading) { m_bIsLoadingSaveGame = true; }

	void OnBrokeSomething( const SBreakEvent& be, bool isPlane );
	void PhysSimulateExplosion(pe_explosion *pexpl, IPhysicalEntity **pSkipEnts,int nSkipEnts, int iTypes);

	void ResetMap( bool isServer );
	bool SetImmersive( bool immersive );

	string GetLevelName() { return m_levelName; }
	string GetRequestedGameRules() { return m_gameRules; }
	uint64 GetLevelChecksum() { return m_levelChecksum; }

	void SetLevelName(const char* levelName) { m_levelName = levelName; }
	void SetGameRules(const char* gameRules) { m_gameRules = gameRules; }
	void SetLevelChecksum(uint64 checksum) { m_levelChecksum = checksum; }

	void SetPhysicsSync( CPhysicsSync * pSync ) { m_pPhysicsSync = pSync; }

	// public so that GameClientChannel can call it
	// -- registers a class in our class name <-> id database
	bool RegisterClassName( const string& name, uint16 id );

	bool Update();

	bool ClassNameFromId( string& name, uint16 id ) const;
	bool ClassIdFromName( uint16& id, const string& name ) const;

	void ChangedScript( EntityId id )
	{
		m_pNetContext->ChangedAspects( id, eEA_Script, NULL );
	}
	void EnablePhysics( EntityId id, bool enable );

	bool ChangeContext( bool isServer, const SGameContextParams * pParams );

	void SetContextInfo( unsigned flags, uint16 port, const char * connectionString ) ;
	bool HasContextFlag( EGameStartFlags flag ) const { return (m_flags & flag) != 0; }
	uint16 GetServerPort() const { return m_port; }
	string GetConnectionString(bool fake = false) const;

	void DefineContextProtocols( IProtocolBuilder * pBuilder, bool server );

	bool ControlsEntity( EntityId id ) const
	{
		return m_controlledObjects.Get( id );
	}

	void EnableAspects( EntityId id, uint8 aspects, bool bEnable );
	void AddControlObjectCallback( EntityId id, bool willHaveControl, HSCRIPTFUNCTION func );

	static void RegisterExtensions( IGameFramework * pFW );

	void AllowCallOnClientConnect();

	void PlayerIdSet(EntityId id);

	void GetMemoryStatistics(ICrySizer * );

	void LockResources();
	void UnlockResources();

private:
	static IEntity* GetEntity( int i, void * p )
	{
		if (i == PHYS_FOREIGN_ID_ENTITY)
			return (IEntity*)p;
		return NULL;
	}

	void BeginChangeContext( const string& levelName );
	void CallOnSpawnComplete( IEntity *pEntity );
	bool HasSpawnPoint();

	void AddLoadLevelTasks( IContextEstablisher * pEst, bool isServer, int flags, bool ** ppLoadingStarted, int establishedToken );
	void AddLoadingCompleteTasks( IContextEstablisher * pEst, int flags, bool * pLoadingStarted );
	void UnprotectCVars();
	std::set<INetChannel*> m_resetChannels;

	enum EEstablishmentFlags
	{
		eEF_LevelLoaded		= 1<<0,
		eEF_LoadNewLevel	= 1<<1,
		eEF_DemoPlayback	= 1<<2
	};

	static const int EstablishmentFlags_InitialLoad = eEF_LoadNewLevel;
	static const int EstablishmentFlags_LoadNextLevel = eEF_LoadNewLevel | eEF_LevelLoaded;
	static const int EstablishmentFlags_ResetMap = eEF_LevelLoaded;

	int m_loadFlags;

	static void OnCollision( const EventPhys * pEvent, void * );

	// singleton
	static CGameContext * s_pGameContext;

	IEntitySystem * m_pEntitySystem;
	IPhysicalWorld * m_pPhysicalWorld;
	IActorSystem * m_pActorSystem;
	INetContext * m_pNetContext;
	CCryAction * m_pFramework;
	CActionGame * m_pGame;
	CScriptRMI * m_pScriptRMI;
	CPhysicsSync * m_pPhysicsSync;
	CVoiceController * m_pVoiceController;

	CClassRegistryReplicator m_classRegistry;

	SensedInclusionSet<EntityId> m_controlledObjects;

	// context parameters
	string m_levelName;
	string m_gameRules;
	uint64 m_levelChecksum;

	unsigned m_flags;
	uint16 m_port;
	string m_connectionString;
	bool m_isInLevelLoad;
	bool m_bStarted;
	bool m_bIsLoadingSaveGame;
	bool m_bHasSpawnPoint;
	bool m_bAllowSendClientConnect;
	int m_resourceLocks;
	int m_removeObjectUnlocks;
	int m_broadcastActionEventInGame;
	friend class CScopedRemoveObjectUnlock;

	struct SDelegateCallbackIndex
	{
		EntityId id;
		bool bHaveControl;
		bool operator<( const SDelegateCallbackIndex& rhs ) const
		{
			return bHaveControl < rhs.bHaveControl || (id < rhs.id);
		}
		bool operator ==(const SDelegateCallbackIndex& rhs) const
		{
			return (bHaveControl == rhs.bHaveControl) && (id == rhs.id);
		}
	};
	typedef std::multimap<SDelegateCallbackIndex, HSCRIPTFUNCTION> DelegateCallbacks;
	DelegateCallbacks m_delegateCallbacks;

	std::auto_ptr<IContextEstablisher> m_pContextEstablisher;
	std::auto_ptr<CBreakReplicator> m_pBreakReplicator;
};

class CScopedRemoveObjectUnlock
{
public:
	CScopedRemoveObjectUnlock( CGameContext* pCtx ) : m_pCtx(pCtx) { pCtx->m_removeObjectUnlocks++; }
	~CScopedRemoveObjectUnlock() { m_pCtx->m_removeObjectUnlocks--; }

private:
	CScopedRemoveObjectUnlock(const CScopedRemoveObjectUnlock&);
	CScopedRemoveObjectUnlock& operator=(const CScopedRemoveObjectUnlock&);

	CGameContext * m_pCtx;
};

#endif //__GameContext_H__
