/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  manages state that is shared between a group of channels
               (primarily a set of ISynchedObjects to be replicated)
 -------------------------------------------------------------------------
 History:
 - 02/09/2004   12:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __NETCONTEXT_H__
#define __NETCONTEXT_H__

#pragma once

#include "INetwork.h"
#include <queue>
#include <memory>
#include "INetContextListener.h"
#include "Socket/NetAddress.h"
#include "INetworkMember.h"
#include "Streams/ArithStream.h"
#include "SessionID.h"
#include "ChangeList.h"
#include "ContextEstablisher.h"
#include "MementoMemoryManager.h"
#include "Protocol/NetChannel.h"
#include "Compression/CompressionManager.h"
#include "NetContextState.h"

class CContextView;
class CServerContextView;
class CClientContextView;
class CNetEntityDebug;
class CVoiceContext;
class CMessageBuffer;
class CNetChannel;
class CNetContext;

class CNetContext : 
	public INetworkMember,
	public INetContext, 
	public CDefaultStreamAllocator
{
public:
	using INetworkMember::Release;

	CNetContext( IGameContext * pGameContext, uint32 flags );
	~CNetContext();

	// INetContext
	virtual void DeleteContext();

	virtual void ActivateDemoRecorder( const char * filename );
	virtual void ActivateDemoPlayback( const char * filename, INetChannel * pClient, INetChannel * pServer );
	virtual bool IsDemoPlayback() const { return m_demoPlayback; }
	virtual void LogRMI( const char * function, ISerializable * pParams );
	virtual void LogCppRMI( EntityId id, IRMICppLogger * pLogger );
	virtual void DeclareAspect( const char * name, uint8 aspectBit, uint8 aspectFlags );
	virtual void BindObject( EntityId id, uint8 aspectBits, bool bStatic );
	virtual bool UnbindObject( EntityId id );
	virtual void EnableAspects( EntityId id, uint8 aspectBits, bool enabled );
	virtual void DelegateAuthority( EntityId id, INetChannel * pControlling );
	virtual bool ChangeContext();
	virtual void ChangedAspects( EntityId id, uint8 aspectBits, INetChannel * pChannel );
	virtual void ChangedTransform( EntityId id, const Vec3& pos, const Quat& rot, float drawDist );
	virtual void ChangedFov( EntityId id, float fov );
	virtual void EstablishedContext( int establishToken );
	virtual void SpawnedObject( EntityId userID );
	virtual bool IsBound( EntityId userID );
	virtual void RemoveRMIListener( IRMIListener * pListener );
	virtual bool RemoteContextHasAuthority( INetChannel * pChannel, EntityId id );
	virtual void SetAspectProfile( EntityId id, uint8 aspectBit, uint8 profile );
	virtual uint8 GetAspectProfile( EntityId id, uint8 aspectBit );
	virtual void SetParentObject( EntityId objId, EntityId parentId );
	virtual void LogBreak( const SNetBreakDescription& breakage );
	virtual bool SetSchedulingParams( EntityId objId, uint32 normal, uint32 owned );
	virtual void PulseObject( EntityId objId, uint32 pulseType );
	virtual void EnableBackgroundPassthrough( bool enable );
	virtual int RegisterPredictedSpawn(INetChannel * pChannel, EntityId id);
	virtual void RegisterValidatedPredictedSpawn( INetChannel * pChannel, int predictionHandle, EntityId id );
	virtual void SafelyUnbind( EntityId id );
	virtual void RequestRemoteUpdate( EntityId id, uint8 aspects );
	// ~INetContext

	// pseudo INetContextListener
	void Die();
	void OnObjectEvent( SNetObjectEvent * pEvent );
	void OnChannelEvent( INetChannel * pFrom, SNetChannelEvent * pEvent );
	string GetName();
	SObjectMemUsage GetObjectMemUsage( SNetObjectID id ) { return SObjectMemUsage(); }
	INetChannel * GetAssociatedChannel() { return 0; }
	// ~INetContextListener

	// INetworkMember
	//virtual void Update( CTimeValue blocking );
	virtual void GetMemoryStatistics( ICrySizer * pSizer );
	virtual bool IsDead();
	virtual bool IsSuicidal();
	virtual void NetDump( ENetDumpType type );
	virtual void SyncWithGame( ENetworkGameSync type );
	virtual void PerformRegularCleanup();
	// ~INetworkMember

	CNetContextState * GetCurrentState() { return m_pState; }
	INetContextListener* GetDemoMode() const { return &(*m_pDemo); }

	//uint16 GetSaltForId( uint16 id );
	uint8 DelegatableAspects() const { return m_delegatableAspects; }
	uint8 ServerManagedProfileAspects() const { return m_serverManagedProfileAspects; }
	uint8 HashedAspects() const { return m_hashAspects; }
	uint8 DeclaredAspects() const { return m_declaredAspects; }
	uint8 TimestampedAspects() const { return m_timestampedAspects; }

	CVoiceContext *GetVoiceContextImpl() { return m_pVoiceContext; }

	void SetLevelName(const char * levelName);


#if ENABLE_SESSION_IDS
	void SetSessionID( const CSessionID& id );
	const CSessionID& GetSessionID() const
	{
		return m_sessionID;
	}

	void EnableSessionDebugging();
#endif

	// register a context view
	void RegisterLocalIPs( const TNetAddressVec&, CContextView* );
	void DeregisterLocalIPs( const TNetAddressVec& );

	// returns NULL if no such local context associated with an IP address/port
	// in this NetContext
	CContextView * GetLocalContext( const TNetAddress& localAddr ) const;

	const char * GetAspectName( int aspectID )
	{
		NET_ASSERT( aspectID < 8 );
		return m_aspectNames[aspectID].c_str();
	}

	IGameContext* GetGameContext()
	{
		return m_pGameContext;
	}

	// demo mode -> CryAction communication
	//void PassDemoModeSpectator(EntityId id)
	//{
	//	ASSERT_PRIMARY_THREAD;
	//	m_pGameContext->PassDemoModeSpectator(id);
	//}

	// queued callbacks to game
#if 0
	void GC_UnboundObject( EntityId );
	void GC_BeginContext(CTimeValue, int, CResettingLock);
	void GC_BeginContext_Wait(CTimeValue, int, CResettingLock);
	void GC_BeginContext2( CTimeValue waitStarted, int, CResettingLock );
	void GC_BeginContext2_Wait( CTimeValue waitStarted, int, CResettingLock );
	void GC_BeginContext3( int, CResettingLock );
	void GC_ControlObject( SNetObjectID, bool, CNetObjectBindLock );
	void GC_BoundObject( std::pair<EntityId, uint8> );
	void GC_SendPostSpawnEntities( CContextViewPtr pView );
	void GC_SetAspectProfile( uint8, uint8, SNetObjectID, CNetObjectBindLock );
	void GC_EndContext(CResettingLock);
	void GC_SetParentObject( EntityId objId, EntityId parentId );
	void GC_PulseObject( EntityId objId, uint32 pulseType );
	void GC_CompleteUnbind( EntityId id );

	void GC_Lazy_TickEstablishers();

	// queued callbacks from game
	void NC_DelegateAuthority( EntityId, INetChannel * );
	void NC_SetAspectProfile( EntityId userID, uint8 aspectBit, uint8 profile, bool lockedUpdate );
	void NC_ChangedAspects( EntityId userID, uint8 aspectBit );
	void NC_ChangedTransform( EntityId, Vec3, Quat, float );
	void NC_ChangedFov( EntityId, float );
	void NC_BroadcastSimpleEvent( ENetObjectEvent event );
#endif

	// reset our state
	void ClearAllState();

	IVoiceContext *GetVoiceContext();

	bool IsMultiplayer() { return m_multiplayer; }

private:
	bool m_demoPlayback;

	bool m_multiplayer;

	bool CanDestroyContext();

	string m_aspectNames[8];
	uint8 m_delegatableAspects;
	uint8 m_regularlyUpdatedAspects;
	uint8 m_hashAspects;
	uint8 m_declaredAspects;
	uint8 m_serverManagedProfileAspects;
	uint8 m_timestampedAspects;

	int m_ctxSerial;

	// we have a game context associated with us
	IGameContext * m_pGameContext;

	// ip address to context view map; for determining when two views are on the
	// same machine
	std::map<TNetAddress, CContextView*> m_mIPToContext;

	bool m_bDead;
	// are we allowed to be dead?
	int m_nAllowDead;
	// are we in background passthrough mode
	NetTimerId m_backgroundPassthrough;
	static void BackgroundPassthrough( NetTimerId, void*, CTimeValue );

	_smart_ptr<CNetContextState> m_pState;

	INetContextListenerPtr m_pDemo;
	_smart_ptr<CVoiceContext> m_pVoiceContext;
	INetContextListenerPtr m_pAspectBandwidthDebugger;

	// this context's session id
#if ENABLE_SESSION_IDS
	CSessionID m_sessionID;
#endif
};

#endif
