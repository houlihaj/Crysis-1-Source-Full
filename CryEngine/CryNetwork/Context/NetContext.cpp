/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  implements shared state between channels (managed by views)
 -------------------------------------------------------------------------
 History:
 - 26/07/2004   : Created by Craig Tiller
 - 08/09/2004   : Moved scheduling to ContextView; added support for
                  multiple object types (ie eNOT_RMI/eNOT_Synched)
*************************************************************************/
#include "StdAfx.h"
#include "Config.h"
#include "NetContext.h"
#include "DemoRecordListener.h"
#include "DemoPlaybackListener.h"
#include "RMILogger.h"
#include "VoiceContext.h"
//#include "ITimer.h"
#include "Utils.h"
#include "ITextModeConsole.h"

#include "DebugKit/DebugKit.h"
#include "Protocol/NetChannel.h"
#include "Streams/ByteStream.h"

static int SessionIDCounter = 10101;

/*
 * Helper functions
 */

string GetEventName( ENetChannelEvent evt )
{
	switch (evt)
	{
	case eNCE_InvalidEvent:
		return "InvalidEvent";
	case eNCE_ChannelDestroyed:
		return "ChannelDestroyed";
	case eNCE_RevokedAuthority:
		return "RevokedAuthority";
	case eNCE_SetAuthority:
		return "SetAuthority";
	case eNCE_UpdatedObject:
		return "UpdatedObject";
	case eNCE_BoundObject:
		return "BoundObject";
	case eNCE_UnboundObject:
		return "UnboundObject";
	case eNCE_SentUpdate:
		return "SentUpdate";
	case eNCE_AckedUpdate:
		return "AckedUpdate";
	case eNCE_NackedUpdate:
		return "NackedUpdate";
	case eNCE_InGame:
		return "InGame";
	case eNCE_DeclareWitness:
		return "DeclareWitness";
	}

	string temp;
	temp.Format("%.8x", evt);
	return temp;
}

string GetEventName( ENetObjectEvent evt )
{
	switch (evt)
	{
	case eNOE_InvalidEvent:
		return "InvalidEvent";
	case eNOE_BindObject:
		return "BindObject";
	case eNOE_UnbindObject:
		return "UnbindObject";
	case eNOE_Reset:
		return "Reset";
	case eNOE_ObjectAspectChange:
		return "ObjectAspectChange";
	case eNOE_UnboundObject:
		return "UnboundObject";
	case eNOE_ChangeContext:
		return "ChangeContext";
	case eNOE_EstablishedContext:
		return "EstablishedContext";
	case eNOE_ReconfiguredObject:
		return "ReconfiguredObject";
	case eNOE_BindAspects:
		return "BindAspects";
	case eNOE_UnbindAspects:
		return "UnbindAspects";
	case eNOE_ContextDestroyed:
		return "ContextDestroyed";
	case eNOE_SetAuthority:
		return "SetAuthority";
	case eNOE_InGame:
		return "InGame";
	case eNOE_SendVoicePackets:
		return "SendVoicePackets";
	case eNOE_RemoveRMIListener:
		return "RemoveRMIListener";
	case eNOE_SetAspectProfile:
		return "SetAspectProfile";
	case eNOE_SyncWithGame_Start:
		return "SyncWithGame_Start";
	case eNOE_SyncWithGame_End:
		return "SyncWithGame_End";
	case eNOE_GotBreakage:
		return "GotBreakage";
	}

	string temp;
	temp.Format("%.8x", evt);
	return temp;
}

/*
 * CNetContext
 */

CNetContext::CNetContext( IGameContext * pGameContext, uint32 flags ) : 
	INetworkMember(eNMUO_Context),
	m_delegatableAspects(0),
	m_regularlyUpdatedAspects(0),
	m_serverManagedProfileAspects(0),
	m_hashAspects(0),
	m_timestampedAspects(0),
//	m_bInGame(false),
	m_pGameContext(pGameContext), 
//	m_nFreeObject(SNetObjectID::InvalidId),
//	m_nNextFreeAspect(0),
//	m_spawnedObjectId(0),
//	m_cleanupMember(0),
	m_bDead(false),
	m_nAllowDead(0),
//	m_bInCleanup(false),
	m_declaredAspects(0),
	m_ctxSerial(100),
//	m_localPhysicsTime(0.0f),
	m_backgroundPassthrough(0)
//	m_resetting(0)
#if ENABLE_SESSION_IDS
	,
	m_sessionID(GetISystem()->GetINetwork()->GetHostName(), SessionIDCounter++, time(NULL))
#endif
{
	++g_objcnt.netContext;

	m_demoPlayback = false;

	m_multiplayer = (flags & INetwork::eNCCF_Multiplayer) != 0;

	// use default compression (less memory usage) in single player game
	CNetwork::Get()->GetCompressionManager().Reset(m_multiplayer);

//	m_pUpdateDisableManager = new CUpdateDisableManager(this);

	m_pState = new CNetContextState(this, m_ctxSerial++, NULL);
//	ChangeSubscription( new CFowardingNetContextListener<CNetContext>(this), NULL, eNCE_InGame | eNCE_ChannelDestroyed );

	if (m_multiplayer)
	{
		// voice stuff disabled in single player game
		m_pVoiceContext = new CVoiceContext(this);
	}
}

bool CNetContext::IsDead()
{
	if (m_nAllowDead == 0)
		return m_bDead;
	else
		return false;
}

bool CNetContext::IsSuicidal()
{
	return m_bDead;
}

void CNetContext::Die()
{
#define SAFE_DIE(x) if (x) { x->Die(); x = 0; }
	SAFE_DIE(m_pAspectBandwidthDebugger);
	SAFE_DIE(m_pVoiceContext);
	SAFE_DIE(m_pDemo);

	SNetObjectEvent event;
	event.event = eNOE_ContextDestroyed;
	m_pState->Broadcast( &event );

	TIMER.CancelTimer(m_backgroundPassthrough);
	m_backgroundPassthrough = 0;

	m_pState->Die();

	m_bDead = true;
}

#if ENABLE_SESSION_IDS
void CNetContext::SetSessionID( const CSessionID& id )
{
	CDebugKit::Get().SetSessionID(id);
}
#endif

CNetContext::~CNetContext()
{
	SCOPED_GLOBAL_LOCK;
	TIMER.CancelTimer(m_backgroundPassthrough);
	CNetwork::Get()->GetCompressionManager().Reset(false);
	--g_objcnt.netContext;
}

void CNetContext::DeleteContext()
{
	SCOPED_GLOBAL_LOCK;
	Die();
}

string CNetContext::GetName()
{
	return "NetContext";
}

#if ENABLE_SESSION_IDS
// yes, this is nothing but a hack; it's a good thing that it won't be compiled in at ship
void CNetContext::EnableSessionDebugging()
{
	CNetSerialize::m_bEnableLogging = true;
}
#endif

void CNetContext::ActivateDemoRecorder( const char * filename )
{
	SCOPED_GLOBAL_LOCK;
#if INCLUDE_DEMO_RECORDING
	NET_ASSERT(m_pDemo == NULL);
	m_pDemo = new CDemoRecordListener( this, filename );
#else
	NetWarning( "Demo recording not enabled" );
#endif
}

void CNetContext::ActivateDemoPlayback( const char * filename, INetChannel * pClient, INetChannel * pServer )
{
	SCOPED_GLOBAL_LOCK;
#if INCLUDE_DEMO_RECORDING
	NET_ASSERT(m_pDemo == NULL);
	m_pDemo =
		new CDemoPlaybackListener( 
			this, 
			filename, 
			(CNetChannel *)pClient, 
			(CNetChannel *)pServer );
	m_demoPlayback = true;
#else
	NetWarning( "Demo recording not enabled" );
#endif
}

/*
void CNetContext::OnObjectEvent( SNetObjectEvent * pEvent )
{
}

void CNetContext::OnChannelEvent( INetChannel * pFrom, SNetChannelEvent * pEvent )
{
	ASSERT_GLOBAL_LOCK;
	switch (pEvent->event)
	{
	case eNCE_InGame:
		if (!m_bInGame)
		{
			m_bInGame = true;
			SNetObjectEvent evt;
			evt.event = eNOE_InGame;
			Broadcast( &evt );
		}
		break;
	case eNCE_ChannelDestroyed:
		for (TContextObjects::iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
		{
			CContextView * pView = ((CNetChannel*)pFrom)->GetContextView();
			if (iter->pController == pView)
				iter->pController = 0;
		}
		break;
	}
}
*/
void CNetContext::SyncWithGame( ENetworkGameSync type )
{
	ASSERT_GLOBAL_LOCK;
	FUNCTION_PROFILER( GetISystem(), PROFILE_NETWORK );
	ENSURE_REALTIME;

/*
	bool debugMode = CVARS.EntityDebug != 0;
	if (debugMode && !m_pNetEntityDebug)
	{
		m_pNetEntityDebug = new CNetEntityDebug(this);
	}
	else if (!debugMode && m_pNetEntityDebug)
	{
		SAFE_DIE(m_pNetEntityDebug);
	}
*/

	SNetObjectEvent syncEvent;

	switch (type)
	{
	case eNGS_FrameStart:
		ASSERT_PRIMARY_THREAD;

		m_pGameContext->OnStartNetworkFrame();
		m_pState->PropogateChangesToGame();

		syncEvent.event = eNOE_SyncWithGame_Start;
		break;

	case eNGS_FrameEnd:
		ASSERT_PRIMARY_THREAD;
		m_pGameContext->OnEndNetworkFrame();
		m_pState->FetchAndPropogateChangesFromGame( true );

		syncEvent.event = eNOE_SyncWithGame_End;
		break;
	}

	if (syncEvent.event != eNOE_InvalidEvent)
		m_pState->Broadcast(&syncEvent);
}

void CNetContext::BackgroundPassthrough(NetTimerId, void* p, CTimeValue)
{
	CNetContext * pThis = static_cast<CNetContext*>(p);
	pThis->m_pState->FetchAndPropogateChangesFromGame( false );
	pThis->m_backgroundPassthrough = TIMER.AddTimer( g_time+0.01f, BackgroundPassthrough, pThis );
}

/*
void CNetContext::ClearAllState()
{
	ASSERT_GLOBAL_LOCK;
	m_nFreeObject = SNetObjectID::InvalidId;

	for (size_t i=0; i<m_vObjects.size(); i++)
	{
		uint16 oldSalt = m_vObjects[i].salt;
		m_vObjects[i] = SContextObject();
		m_vObjects[i].salt = oldSalt;
		if (i < m_vObjectsEx.size())
			m_vObjectsEx[i] = SContextObjectEx();
	}
	for (size_t i=0; i<m_vObjects.size(); i++)
	{
		m_vObjects[i].next = m_nFreeObject;
		m_nFreeObject = i;
	}

	m_mNetIDs.clear();
	m_bInGame = false;
	m_vGameChangedObjects.Flush();
	m_vNetChangeLog.resize(0);
	m_removedStaticEntities.resize(0);
	m_loggedBreakage.resize(0);
	m_changedProfiles.resize(0);
	CResettingLock rlk;
	
	SNetObjectEvent event;
	event.event = eNOE_Reset;
	Broadcast( &event );

	//TO_GAME( &IGameContext::EndContext, m_pGameContext );
	m_nAllowDead++;
	TO_GAME( &CNetContext::GC_EndContext, this, rlk );
}
*/

void CNetContext::EnableBackgroundPassthrough( bool enable )
{
	SCOPED_GLOBAL_LOCK;
	if (enable && !m_backgroundPassthrough)
		m_backgroundPassthrough = TIMER.AddTimer( g_time, BackgroundPassthrough, this );
	else
	{
		TIMER.CancelTimer( m_backgroundPassthrough );
		m_backgroundPassthrough = 0;
	}
}

void CNetContext::DeclareAspect( const char * name, uint8 aspectBit, uint8 aspectFlags )
{
	SCOPED_GLOBAL_LOCK;

	if (CountBits(aspectBit) != 1)
		CryError("Can only declare one aspect at a time");
	aspectBit = BitIndex(aspectBit);
	if (m_declaredAspects & (1<<aspectBit))
		CryError("Net aspect %d declared twice", aspectBit);

	m_aspectNames[aspectBit] = name;

	if (aspectFlags & eAF_Delegatable)
		m_delegatableAspects |= 1<<aspectBit;
	if (aspectFlags & eAF_ServerManagedProfile)
		m_serverManagedProfileAspects |= 1<<aspectBit;
	if (aspectFlags & eAF_HashState)
		m_hashAspects |= 1<<aspectBit;
	if (aspectFlags & eAF_TimestampState)
		m_timestampedAspects |= 1<<aspectBit;

	m_declaredAspects |= 1<<aspectBit;
}

bool CNetContext::ChangeContext()
{
	SCOPED_GLOBAL_LOCK;
	_smart_ptr<CNetContextState> pNewState = new CNetContextState(this, m_ctxSerial++, m_pState);
	m_pState->Die();
	SNetObjectEvent evt;
	evt.event = eNOE_ChangeContext;
	evt.pNewState = pNewState;
	m_pState->Broadcast(&evt);
	m_pState = pNewState;
	TO_GAME_LAZY(&CNetContextState::GC_BeginContext, &*m_pState, g_time);
	return true;
}

void CNetContext::EstablishedContext( int establishToken )
{
	SCOPED_GLOBAL_LOCK;
	if (m_pState->GetToken() != establishToken)
		return;
	m_pState->EstablishedContext();
}

/*
void CNetContext::ConfigureContext( CContextView * pView, bool isServer )
{
	m_pGameContext->ConfigureContext( pView->Parent(), pView->IsLocal(), isServer );
}
*/

void CNetContext::RegisterLocalIPs( const TNetAddressVec& ips, CContextView* pView )
{
	ASSERT_GLOBAL_LOCK;
	for (TNetAddressVec::const_iterator i = ips.begin(); i != ips.end(); ++i)
		m_mIPToContext[*i] = pView;
}

void CNetContext::DeregisterLocalIPs( const TNetAddressVec& ips )
{
	ASSERT_GLOBAL_LOCK;
	for (TNetAddressVec::const_iterator i = ips.begin(); i != ips.end(); ++i)
		m_mIPToContext.erase( *i );
}

CContextView * CNetContext::GetLocalContext( const TNetAddress& localAddr ) const
{
	ASSERT_GLOBAL_LOCK;
	CContextView * out = NULL;
	std::map<TNetAddress, CContextView*>::const_iterator i = m_mIPToContext.find( localAddr );
	if ( i != m_mIPToContext.end() )
		out = i->second;
	return out;
}

void CNetContext::NetDump( ENetDumpType type )
{
	m_pState->NetDump(type);
}

void CNetContext::PerformRegularCleanup()
{
	m_pState->PerformRegularCleanup();
}

void CNetContext::LogRMI( const char * function, ISerializable * pParams )
{
	m_pState->LogRMI( function, pParams );
}

void CNetContext::LogCppRMI( EntityId id, IRMICppLogger * pLogger )
{
	m_pState->LogCppRMI( id, pLogger );
}

void CNetContext::SetAspectProfile( EntityId id, uint8 aspectBit, uint8 profile )
{
	m_pState->SetAspectProfile( id, aspectBit, profile );
}

uint8 CNetContext::GetAspectProfile( EntityId id, uint8 aspectBit )
{
	return m_pState->GetAspectProfile( id, aspectBit );
}

void CNetContext::BindObject( EntityId id, uint8 aspectBits, bool bStatic )
{
	m_pState->BindObject( id, aspectBits, bStatic );
}

void CNetContext::SafelyUnbind( EntityId id )
{
	m_pState->SafelyUnbind(id);
}

bool CNetContext::IsBound(EntityId userID)
{
	return m_pState->IsBound(userID);
}

void CNetContext::SpawnedObject( EntityId userID )
{
	m_pState->SpawnedObject(userID);
}

void CNetContext::SetLevelName(const char * levelName)
{
	m_pState->SetLevelName(levelName);
}

bool CNetContext::UnbindObject(EntityId id)
{
	return m_pState->UnbindObject(id);
}

void CNetContext::EnableAspects(EntityId id, uint8 aspectBits, bool enabled)
{
	m_pState->EnableAspects(id, aspectBits, enabled);
}

void CNetContext::ChangedAspects(EntityId id, uint8 aspectBits, INetChannel * pChannel)
{
	m_pState->ChangedAspects(id, aspectBits, pChannel);
}

void CNetContext::ChangedTransform(EntityId id, const Vec3& pos, const Quat& rot, float drawDist)
{
	m_pState->ChangedTransform(id, pos, rot, drawDist);
}

void CNetContext::ChangedFov(EntityId id, float fov)
{
	m_pState->ChangedFov(id, fov);
}

void CNetContext::DelegateAuthority(EntityId id, INetChannel * pControlling)
{
	m_pState->DelegateAuthority(id, pControlling);
}

void CNetContext::RemoveRMIListener(IRMIListener * pListener)
{
	m_pState->RemoveRMIListener(pListener);
}

bool CNetContext::RemoteContextHasAuthority(INetChannel * pChannel, EntityId id)
{
	return m_pState->RemoteContextHasAuthority(pChannel, id);
}

void CNetContext::SetParentObject(EntityId objId, EntityId parentId)
{
	m_pState->SetParentObject(objId, parentId);
}

void CNetContext::LogBreak(const SNetBreakDescription& breakage)
{
	m_pState->LogBreak(breakage);
}

bool CNetContext::SetSchedulingParams(EntityId objId, uint32 normal, uint32 owned)
{
	return m_pState->SetSchedulingParams(objId, normal, owned);
}

void CNetContext::PulseObject(EntityId objId, uint32 pulseType)
{
	m_pState->PulseObject(objId, pulseType);
}

IVoiceContext * CNetContext::GetVoiceContext()
{
	return GetVoiceContextImpl();
}

int CNetContext::RegisterPredictedSpawn(INetChannel * pChannel, EntityId id)
{
	return m_pState->RegisterPredictedSpawn(pChannel, id);
}

void CNetContext::RegisterValidatedPredictedSpawn(INetChannel * pChannel, int predictionHandle, EntityId id)
{
	m_pState->RegisterValidatedPredictedSpawn(pChannel, predictionHandle, id);
}

void CNetContext::RequestRemoteUpdate( EntityId id, uint8 aspects )
{
	m_pState->RequestRemoteUpdate( id, aspects );
}

void CNetContext::GetMemoryStatistics( ICrySizer * pSizer )
{
	SIZER_COMPONENT_NAME(pSizer, "CNetContext");

	pSizer->Add(*this);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_mIPToContext");
		pSizer->AddContainer(m_mIPToContext);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_aspectNames");
		for (int i = 0; i < 8; ++i)
			pSizer->AddString(m_aspectNames[i]);
	}

	if (m_pState)
		m_pState->GetMemoryStatistics(pSizer);
}
