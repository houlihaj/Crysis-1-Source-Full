#include "StdAfx.h"
#include "NetContextState.h"
#include "NetContext.h"
#include "Protocol/NetChannel.h"
#include "ContextView.h"
#include "ITextModeConsole.h"

#include "IEntitySystem.h"

#if ENABLE_DEBUG_KIT
#include "ClientContextView.h"
#endif

class CAutoFreeHandle
{
public:
	CAutoFreeHandle( TMemHdl& hdl ) : m_hdl(hdl) {}
	~CAutoFreeHandle()
	{
		MMM().FreeHdl(m_hdl);
		m_hdl = CMementoMemoryManager::InvalidHdl;
	}

	TMemHdl Grab()
	{
		TMemHdl out = m_hdl;
		m_hdl = CMementoMemoryManager::InvalidHdl;
		return out;
	}

	TMemHdl Peek()
	{
		return m_hdl;
	}

private:
	TMemHdl& m_hdl;
};

CNetContextState::CNetContextState( CNetContext* pContext, int token, CNetContextState * pPrev ) : m_pMMM(new CMementoMemoryManager)
{
	MMM_REGION(m_pMMM);

	m_pNetIDs.reset(new TNetIDMap);
	m_pLoggedBreakage.reset(new TNetIntBreakDescriptionList);

	++g_objcnt.netContextState;

	m_pContext = pContext;
	m_pGameContext = pContext->GetGameContext();
	m_token = token;
	m_multiplayer = pContext->IsMultiplayer();
	m_established = false;
	m_bInCleanup = false;
	m_bInGame = false;
	m_cleanupMember = 0;
	m_localPhysicsTime = m_pGameContext? m_pGameContext->GetPhysicsTime() : 0.0f;
	m_spawnedObjectId = 0;

	if (pPrev && pPrev->m_vObjects.size())
	{
		m_vObjects.resize(pPrev->m_vObjects.size());
		if (m_multiplayer)
			m_vObjectsEx.resize(pPrev->m_vObjectsEx.size());

		for (size_t i=0; i<m_vObjects.size(); i++)
		{
			m_vObjects[i].salt = pPrev->m_vObjects[i].salt + 2;
			m_freeObjects.push(i);
		}
	}

	extern void DownloadConfig();
	DownloadConfig();
}

CNetContextState::~CNetContextState()
{
	SCOPED_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

	--g_objcnt.netContextState;

	if (m_multiplayer)
	{
		for (size_t i=0; i<m_vObjectsEx.size(); i++)
		{
			for (int j=0; j<8; j++)
			{
				MMM().FreeHdl(m_vObjectsEx[i].vAspectData[j]);
				MMM().FreeHdl(m_vObjectsEx[i].vRemoteAspectData[j]);
			}
		}
	}

	m_vObjects.resize(0);
	m_vObjectsEx.resize(0);

	m_pNetIDs.reset();
	m_pLoggedBreakage.reset();
}

void CNetContextState::Die()
{
	MMM_REGION(m_pMMM);

	// take control of everything
	for (int i=0; i<m_vObjects.size(); i++)
	{
		if (m_vObjects[i].bOwned && m_vObjects[i].userID)
		{
			ASSERT_PRIMARY_THREAD;
			NC_DelegateAuthority( m_vObjects[i].userID, NULL );
		}
	}
	// forget changes
	m_vNetChangeLog.clear();
	m_vNetChangeLogUnique.clear();

	NET_ASSERT(m_pContext);
	m_pContext = 0;
}

bool CNetContextState::IsDead()
{
	return m_pContext == 0;
}

void CNetContextState::AttachRMILogger( IRMILogger * pLogger )
{
	ASSERT_GLOBAL_LOCK;
	stl::push_back_unique( m_rmiLoggers, pLogger );
}

void CNetContextState::DetachRMILogger( IRMILogger * pLogger )
{
	ASSERT_GLOBAL_LOCK;
	m_rmiLoggers.erase(
		std::remove( m_rmiLoggers.begin(), m_rmiLoggers.end(), pLogger ),
		m_rmiLoggers.end() );
}

void CNetContextState::ChangeSubscription( INetContextListenerPtr pListener, unsigned events )
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;
	HandleSubscriptionDelta( pListener, ChangeSubscription(m_subscriptions, pListener, events), events );
}

void CNetContextState::ChangeSubscription( INetContextListenerPtr pListener, INetChannel * pChannel, unsigned events )
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;
	ChangeSubscription( m_channelSubscriptions[pChannel], pListener, events );
}

unsigned CNetContextState::ChangeSubscription( TSubscriptions& subscriptions, INetContextListenerPtr pListener, unsigned events )
{
	ASSERT_GLOBAL_LOCK;
	for (TSubscriptions::iterator iter = subscriptions.begin(); iter != subscriptions.end(); ++iter)
	{
		if (iter->pListener == pListener)
		{
			unsigned out = iter->events;
			if (!events)
				subscriptions.erase( iter );
			else
				iter->events = events;
			return out;
		}
	}
	if (events)
		subscriptions.push_back( SSubscription(pListener, events) );
	return 0;
}

void CNetContextState::BroadcastChannelEvent( INetChannel * pFrom, SNetChannelEvent * pEvent )
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;
	std::map<INetChannel*, TSubscriptions>::const_iterator iter;
	if (pFrom)
	{
		iter = m_channelSubscriptions.find( pFrom );
		if (iter != m_channelSubscriptions.end())
			BroadcastChannelEventTo( iter->second, pFrom, pEvent );
	}
	iter = m_channelSubscriptions.find( NULL );
	if (iter != m_channelSubscriptions.end())
		BroadcastChannelEventTo( iter->second, pFrom, pEvent );
}

void CNetContextState::BroadcastChannelEventTo( const TSubscriptions& subscriptions, INetChannel * pFrom, SNetChannelEvent * pEvent )
{
	ASSERT_GLOBAL_LOCK;
	m_tempSubscriptions = subscriptions;
	for (TSubscriptions::const_iterator iter = m_tempSubscriptions.begin(); iter != m_tempSubscriptions.end(); ++iter)
		if (iter->events & pEvent->event)
			iter->pListener->OnChannelEvent( this, pFrom, pEvent );
}

ILINE void CNetContextState::SendEventTo( SNetObjectEvent * event, INetContextListener * pListener )
{
	MMM_REGION(0);

#if LOG_NETCONTEXT_MESSAGES
	NetLog("[netobjmsg] %s -> %s", GetEventName(event->event).c_str(), pListener->GetName().c_str());
#endif
	pListener->OnObjectEvent( this, event );
}

void CNetContextState::HandleSubscriptionDelta( INetContextListenerPtr pListener, unsigned oldEvents, unsigned newEvents )
{
	ASSERT_GLOBAL_LOCK;
	if (TurnedOnBit(eNOE_RemoveStaticEntity, oldEvents, newEvents))
		SendRemoveStaticEntitiesTo( pListener );
	if (TurnedOnBit(eNOE_BindObject, oldEvents, newEvents))
		SendBindEventsTo( pListener, TurnedOnBit(eNOE_SetAuthority, oldEvents, newEvents) );
	if (TurnedOnBit(eNOE_BindAspects, oldEvents, newEvents))
		SendBindAspectsTo( pListener );
	if (TurnedOnBit(eNOE_SetAspectProfile, oldEvents, newEvents))
		SendSetAspectProfileEventsTo( pListener );
	if (TurnedOnBit(eNOE_GotBreakage, oldEvents, newEvents))
		SendBreakageTo( pListener );
	if (m_bInGame && TurnedOnBit(eNOE_InGame, oldEvents, newEvents))
	{
		SNetObjectEvent event;
		event.event = eNOE_InGame;
		SendEventTo( &event, pListener );
	}

	if (IsContextEstablished() && TurnedOnBit(eNOE_EstablishedContext, oldEvents, newEvents))
	{
		SNetObjectEvent event;
		event.event = eNOE_EstablishedContext;
		SendEventTo( &event, pListener );
	}
}

void CNetContextState::SendRemoveStaticEntitiesTo( INetContextListenerPtr pListener )
{
	for (std::vector<EntityId>::iterator iter = m_removedStaticEntities.begin(); iter != m_removedStaticEntities.end(); ++iter)
	{
		SNetObjectEvent event;
		event.event = eNOE_RemoveStaticEntity;
		event.userID = *iter;
		SendEventTo( &event, pListener );
	}
}

void CNetContextState::SendBindEventsTo( INetContextListenerPtr pListener, bool alsoAuth )
{
	if (!m_pContext)
		return;

	ASSERT_GLOBAL_LOCK;
	SNetObjectEvent event;
	event.event = eNOE_BindObject;
	for (TContextObjects::iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
	{
		if (iter->bAllocated)
		{
			uint16 idx = iter - m_vObjects.begin();
			event.id = SNetObjectID(idx, iter->salt);
			SendEventTo( &event, pListener );

			if (alsoAuth && iter->pController == pListener)
			{
				event.event = eNOE_SetAuthority;
				event.authority = true;
				SendEventTo( &event, pListener );
				event.event = eNOE_BindObject;
			}
		}
	}
}

void CNetContextState::SendSetAspectProfileEventsTo( INetContextListenerPtr pListener )
{
	if (!m_pContext)
		return;

	ASSERT_GLOBAL_LOCK;
	SNetObjectEvent event;
	event.event = eNOE_SetAspectProfile;
	//for (TContextObjects::iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
	for (size_t it = 0; it < m_vObjects.size(); ++it)
	{
		//if (iter->bAllocated)
		if (m_vObjects[it].bAllocated)
		{
			//event.id = SNetObjectID(iter - m_vObjects.begin());
			event.id = SNetObjectID(it, m_vObjects[it].salt);
			CBitIter itAspects(m_pContext->ServerManagedProfileAspects());
			int i;
			while (itAspects.Next(i))
			{
				//if (iter->vAspectProfiles[i] != iter->vAspectDefaultProfiles[i])
				if (m_vObjects[it].vAspectProfiles[i] != m_vObjects[it].vAspectDefaultProfiles[i])
				{
					event.aspects = i;
					event.profile = m_vObjects[it].vAspectProfiles[i];
					SendEventTo( &event, pListener );
				}
			}
		}
	}
}

void CNetContextState::SendBreakageTo( INetContextListenerPtr pListener )
{
	ASSERT_GLOBAL_LOCK;
	SNetObjectEvent event;
	event.event = eNOE_GotBreakage;
	for (TNetIntBreakDescriptionList::iterator it = m_pLoggedBreakage->begin(); it != m_pLoggedBreakage->end(); ++it)
	{
		event.pBreakage = &*it;
		SendEventTo(&event, pListener);
	}
}

void CNetContextState::Broadcast( SNetObjectEvent * pEvent )
{
	ASSERT_GLOBAL_LOCK;
	for (TSubscriptions::iterator iter = m_subscriptions.begin(); iter != m_subscriptions.end(); ++iter)
		if (iter->events & pEvent->event)
			SendEventTo( pEvent, iter->pListener );
}

bool CNetContextState::SendEventToChannelListener( INetChannel * pIChannel, SNetObjectEvent * pEvent )
{
	ASSERT_GLOBAL_LOCK;
	if (pIChannel)
	{
		CNetChannel * pCChannel = (CNetChannel *)pIChannel;
		if (INetContextListenerPtr pListener = pCChannel->GetContextView())
			return SendEventToListener( pListener, pEvent );
	}
	return false;
}

bool CNetContextState::SendEventToListener( INetContextListenerPtr pListener, SNetObjectEvent * pEvent )
{
	ASSERT_GLOBAL_LOCK;
#if ENABLE_DEBUG_KIT
	// temporary debug hack
	if (pEvent->event == eNOE_SetAuthority && CVARS.LogLevel > 0)
		NetLog("Send SetAuthority(%d) event to %s", pEvent->authority, pListener->GetName().c_str());
	// ~temporary debug hack
#endif

	bool accepts = false;
	for (TSubscriptions::iterator iter = m_subscriptions.begin(); iter != m_subscriptions.end(); ++iter)
	{
		if (iter->pListener == pListener)
		{
			accepts = (iter->events & pEvent->event) != 0;
			if (accepts)
				SendEventTo( pEvent, iter->pListener );
			break;
		}
	}
#if ENABLE_DEBUG_KIT
	if (!accepts)
	{
		NetComment( "Could not send event %s to specific listener %s", GetEventName(pEvent->event).c_str(), pListener->GetName().c_str() );
	}
#endif
	return false;
}

void CNetContextState::EstablishedContext()
{
	NET_ASSERT( !IsContextEstablished() );

	m_established = true;

	SNetObjectEvent event;
	event.event = eNOE_EstablishedContext;
	Broadcast( &event );
}

void CNetContextState::FetchAndPropogateChangesFromGame( bool allowFetch )
{
	MMM_REGION(m_pMMM);

	if (!m_pContext || !m_multiplayer)
	{
		m_vGameChangedObjects.Flush();
		return;
	}

	CChangeList<SNetObjectAspectChange>::Objects& changed = m_vGameChangedObjects.GetObjects();
	if (!changed.empty())
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_NETWORK);

		CTimeValue newPhysicsTime = m_pGameContext->GetPhysicsTime();
		if (newPhysicsTime >= m_localPhysicsTime)
			m_localPhysicsTime = newPhysicsTime;

		NET_ASSERT(m_changeBuffer.empty());
		for (size_t i=0; i<changed.size(); i++)
		{
			if (!allowFetch || IsLocked(changed[i].first, eCOL_GameDataSync))
			{
				m_changeBuffer.push_back(changed[i]);
				changed[i].second.aspectsChanged = 0;
			}
			else
			{
				changed[i].second.aspectsChanged = changed[i].second.forceChange | UpdateAspectData( changed[i].first, changed[i].second.aspectsChanged );
			}
		}

		// add a terminator
		changed.push_back( std::make_pair(SNetObjectID(), SNetObjectAspectChange()) );

		// broadcast the event
		SNetObjectEvent event;
		event.event = eNOE_ObjectAspectChange;
		event.pChanges = &changed[0];
		Broadcast( &event );

		// clear the change list
		m_vGameChangedObjects.Flush();

		while (!m_changeBuffer.empty())
		{
			m_vGameChangedObjects.AddChange( m_changeBuffer.back().first, m_changeBuffer.back().second );
			m_changeBuffer.pop_back();
		}
	}
}

void CNetContextState::PropogateChangesToGame()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_NETWORK);

	MMM_REGION(m_pMMM);

	if (!m_pContext)
		return;

	if (!m_changedProfiles.empty())
	{
		for (TChangedProfiles::iterator it = m_changedProfiles.begin(); it != m_changedProfiles.end(); ++it)
		{
			ASSERT_PRIMARY_THREAD;
			m_pGameContext->SetAspectProfile( m_vObjects[it->obj.GetID().id].userID, it->aspect, it->profile );
			UnlockObject( it->obj.GetID(), eCOL_GameDataSync );
		}
		m_changedProfiles.resize(0);
	}

	if (m_vNetChangeLog.empty())
		return;

	NET_ASSERT(m_vNetChangeLogUnique.empty());
	std::sort(m_vNetChangeLog.begin(), m_vNetChangeLog.end(), SChannelChange::SCompareObjectsThenReceiveTime());
	std::vector<SChannelChange>::iterator iterUnique = m_vNetChangeLog.begin();
	m_vNetChangeLogUnique.push_back(*iterUnique++);
	for (; iterUnique != m_vNetChangeLog.end(); ++iterUnique)
	{
		if (iterUnique->obj == m_vNetChangeLogUnique.back().obj)
			if (iterUnique->aspect == m_vNetChangeLogUnique.back().aspect)
				continue;
		m_vNetChangeLogUnique.push_back(*iterUnique);
	}
	std::sort(m_vNetChangeLogUnique.begin(), m_vNetChangeLogUnique.end(), SChannelChange::SCompareChannelThenTimeThenObject());

	_smart_ptr<CNetChannel> pChannel = 0;
	_smart_ptr<CNetChannel> pUpdateChannel = 0;
	CTimeValue frameTime = 0.0f;
	for (std::vector<SChannelChange>::iterator iter = m_vNetChangeLogUnique.begin(); iter != m_vNetChangeLogUnique.end(); ++iter)
	{
		//const SContextObject * pObj = GetContextObject(iter->first);
		SContextObjectRef obj = GetContextObject(iter->obj);
		//if (!pObj || !pObj->userID)
		if (!obj.main || !obj.main->userID)
			continue;
		if (obj.xtra->vRemoteAspectData[iter->aspect] != CMementoMemoryManager::InvalidHdl)
		{
			CAutoFreeHandle freeHdl(const_cast<TMemHdl&>(obj.xtra->vRemoteAspectData[iter->aspect]));

			bool isTimeStamped = 0 != (m_pContext->TimestampedAspects() & (1 << iter->aspect));

			if (isTimeStamped)
			{
				if (iter->pChannel != pChannel || iter->channelTime != frameTime)
				{
					if (iter->pChannel->IsDead())
						break;
					if (pChannel)
						m_pGameContext->EndUpdateObjects();
					pChannel = iter->pChannel;
					frameTime = iter->channelTime;
					//NET_ASSERT(frameTime!=0.0f);
					m_pGameContext->BeginUpdateObjects( frameTime, pChannel );
#if LOG_BUFFER_UPDATES
					if (CNetCVars::Get().LogBufferUpdates)
						NetLog("[sync] channel frame time is %f", frameTime.GetMilliSeconds());
#endif
				}
			}
			else
			{
				if (pChannel)
					m_pGameContext->EndUpdateObjects();
				pChannel = 0;
				frameTime = 0.0f;
			}

#if LOG_BUFFER_UPDATES
			if (CNetCVars::Get().LogBufferUpdates)
				NetLog("[buf] push %s:%s", iter->obj.GetText(), m_pContext->GetAspectName(iter->aspect));
#endif
			const void * pData = MMM().PinHdl(obj.xtra->vRemoteAspectData[iter->aspect]);
			int nLength = MMM().GetHdlSize(obj.xtra->vRemoteAspectData[iter->aspect]);
			CByteInputStream in( (const uint8*) pData, nLength );
			bool failed = false;

#if ENABLE_DEBUG_KIT
			if (iter->aspect == 3)
				if (iter->pChannel)
					if (CContextView * pCtxView = iter->pChannel->GetContextView())
						if (pCtxView->IsClient())
							static_cast<CClientContextView*>(pCtxView)->BeginWorldUpdate( iter->obj );
#endif

			if (isTimeStamped)
			{
				CTimeValue when = in.GetTyped<CTimeValue>(); // skip time value for now
#if LOG_BUFFER_UPDATES
				if (CNetCVars::Get().LogBufferUpdates)
					NetLog("[sync] aspect time is %f", when.GetMilliSeconds());
#endif
				//NET_ASSERT(frameTime!=0.0f);
#if ENABLE_DEBUG_KIT
				if (when != frameTime)
				{
					NetQuickLog(true, 1, "Physics update commit %fms late", (when - frameTime).GetMilliSeconds());
				}
#endif
			}
			bool wasPartialUpdate = false;
			switch (CNetwork::Get()->GetCompressionManager().GameContextWrite( m_pGameContext, &in, obj.main->userID, 1<<iter->aspect, obj.main->vAspectProfiles[iter->aspect], GetChunkIDFromObject(obj, iter->aspect), wasPartialUpdate ))
			{
			case eSOR_Failed:
				{
					static const size_t len = 256;
					char buf[len];
					_snprintf( buf, len, "Failed to commit data from network to game! obj(eid:%.8x name:%s netid:%s aspect:%s)", obj.main->userID, obj.main->GetName(), iter->obj.GetText(), m_pContext->GetAspectName(iter->aspect) );
					iter->pChannel->Disconnect( eDC_ContextCorruption, buf );
					failed = true;
				}
				break;
			case eSOR_Ok:
				m_vGameChangedObjects.AddChange( iter->obj, SNetObjectAspectChange(0, 1<<iter->aspect) );
				if (wasPartialUpdate)
				{
					SNetObjectEvent evt;
					evt.event = eNOE_PartialUpdate;
					evt.aspects = iter->aspect;
					evt.id = iter->obj;
					SendEventToChannelListener( pChannel, &evt );
				}
				// TODO: craig - this is ugly as sin
				{
					TMemHdl& targetHdl = const_cast<TMemHdl&>(obj.xtra->vAspectData[iter->aspect]);
					TMemHdl newHdl = freeHdl.Peek();
					bool take = true;
					if (targetHdl != CMementoMemoryManager::InvalidHdl && MMM().GetHdlSize(targetHdl) == MMM().GetHdlSize(newHdl))
					{
						int ofs = 0;
						if (isTimeStamped)
							ofs += sizeof(CTimeValue);
						const char * pOld = (const char *)MMM().PinHdl(targetHdl);
						const char * pNew = (const char *)MMM().PinHdl(newHdl);
						int cmpSz = MMM().GetHdlSize(newHdl) - ofs;
						if (cmpSz > 0 && 0 == memcmp(pOld + ofs, pNew + ofs, cmpSz))
							take = false;
						else if (!cmpSz)
							take = false;
					}
					if (take)
					{
						MMM().FreeHdl(targetHdl);
						targetHdl = freeHdl.Grab();
						const_cast<uint32&>(obj.xtra->vAspectDataVersion[iter->aspect])++;
						if (isTimeStamped)
							*static_cast<CTimeValue*>(MMM().PinHdl(obj.xtra->vAspectData[iter->aspect])) = m_localPhysicsTime;
					}
				}
				// fall through
			case eSOR_Skip:
				break;
			}
			if (failed)
				break;

#if ENABLE_DEBUG_KIT
			if (iter->aspect == 3)
				if (iter->pChannel)
					if (CContextView * pCtxView = iter->pChannel->GetContextView())
						if (pCtxView->IsClient())
							static_cast<CClientContextView*>(pCtxView)->EndWorldUpdate( iter->obj );
#endif
		}
	}
	m_vNetChangeLog.resize(0);
	m_vNetChangeLogUnique.resize(0);
	if (pChannel)
		m_pGameContext->EndUpdateObjects();
}

void CNetContextState::SendBindAspectsTo( INetContextListenerPtr pListener )
{
	if (!m_pContext)
		return;

	ASSERT_GLOBAL_LOCK;
	SNetObjectEvent event;
	event.event = eNOE_BindAspects;

	// TODO: check the logic here, it should only make sence for multiplayer game - Lin
	if (m_multiplayer)
	{
		for (size_t netID = 0; netID < m_vObjects.size(); netID++)
		{
			if (!m_vObjects[netID].bAllocated)
				continue;

			SContextObjectEx& objx = m_vObjectsEx[netID];
			event.id = SNetObjectID(netID, m_vObjects[netID].salt);
			event.aspects = objx.nAspectsEnabled;
			SendEventTo( &event, pListener );
		}
	}
}

void CNetContextState::SetParentObject( EntityId objId, EntityId parentId )
{
	FROM_GAME(&CNetContextState::GC_SetParentObject, this, objId, parentId);
}

SContextObjectRef CNetContextState::GetContextObject_Slow( SNetObjectID nID ) const
{
	SContextObjectRef obj;
	if (m_vObjects.size() <= nID.id || !m_vObjects[nID.id].bAllocated || m_vObjects[nID.id].salt != nID.salt)
		return obj;
	obj.main = &m_vObjects[nID.id];

	static SContextObjectEx objx;
	obj.xtra = m_multiplayer ? &m_vObjectsEx[nID.id] : &objx;

	m_cacheObjectID = nID;
	m_cacheObjectRef = obj;

	return obj;
}

SNetObjectID CNetContextState::GetNetID( EntityId userID, bool ensureNotUnbinding )
{
	SNetObjectID id = stl::find_in_map( *m_pNetIDs, userID, SNetObjectID() );
	if (ensureNotUnbinding && id)
		if (!m_vObjects[id.id].userID)
			id = SNetObjectID();
	if (id && m_vObjects[id.id].salt != id.salt)
		id = SNetObjectID();
	return id;
}

void CNetContextState::SetAspectProfile( EntityId id, uint8 aspectBit, uint8 profile )
{
	if (!m_pContext)
		return;

	SCOPED_GLOBAL_LOCK;
	SNetObjectID netID = GetNetID( id );
	bool lockedUpdate = false;

	if (profile > MaxProfilesPerAspect)
	{
		SContextObjectRef obj = GetContextObject(netID);
#if ENABLE_DEBUG_KIT
		if (CNetCVars::Get().LogLevel)
			NetWarning("WARNING: Illegal profile %d passed for aspect %s on entity %s (%d)", profile, m_pContext->GetAspectName(BitIndex(aspectBit)), obj.main ? obj.main->GetName() : "<<unknown>>", id);
#endif
		return;
	}

	if (!netID || !GetContextObject(netID).main)
	{
		// not sure if this is the correct thing to do, but should work
		// if the object is not bound, allow the SetAspectProfile to succeed anyway
		ASSERT_PRIMARY_THREAD;
		m_pGameContext->SetAspectProfile( id, aspectBit, profile );
	}
	else
	{
		SContextObject& obj = m_vObjects[netID.id];
		if (obj.bOwned)
		{
			ASSERT_PRIMARY_THREAD;
			m_pGameContext->SetAspectProfile( id, aspectBit, profile );
			LockObject( netID, eCOL_GameDataSync );
			lockedUpdate = true;
		}
	}

	FROM_GAME( &CNetContextState::NC_SetAspectProfile, this, id, aspectBit, profile, lockedUpdate );
}

void CNetContextState::NC_SetAspectProfile( EntityId userID, uint8 aspectBit, uint8 profile, bool lockedUpdate )
{
	ASSERT_GLOBAL_LOCK;
	DoSetAspectProfile( userID, aspectBit, profile, true, true, lockedUpdate );
}

uint8 CNetContextState::GetAspectProfile( EntityId userID, uint8 aspectBit )
{
	SCOPED_GLOBAL_LOCK;

	// force all from-game messages out of the queue
	GetISystem()->GetINetwork()->SyncWithGame(eNGS_FrameEnd);

	SNetObjectID netID = GetNetID( userID );

	if (!netID || !GetContextObject(netID).main )
	{
		return 255;
	}

	NET_ASSERT( GetContextObject(netID).main );
	const SContextObject& obj = m_vObjects[netID.id];

	if (CountBits(aspectBit) != 1)
	{
		CryError("Can only set one aspect profile at a time");
		return 255;
	}
	if (m_pContext && (m_pContext->ServerManagedProfileAspects() & aspectBit) == 0)
	{
#if ENABLE_DEBUG_KIT
		if (CNetCVars::Get().LogLevel > 2)
			NetWarning("aspect %s is not server managed", m_pContext? m_pContext->GetAspectName(BitIndex(aspectBit)) : "unknown");
#endif
		return 255;
	}

	aspectBit = BitIndex(aspectBit);
	return obj.vAspectProfiles[aspectBit];
}

void CNetContextState::LogChangedProfile( SNetObjectID id, uint8 aspect, uint8 profile )
{
	bool found = false;
	for (TChangedProfiles::iterator it = m_changedProfiles.begin(); it != m_changedProfiles.end(); ++it)
	{
		if (it->obj.GetID() == id && it->aspect == aspect)
		{
			found = true;
			if (it->profile == profile)
				return;
			else
			{
				m_changedProfiles.erase(it);
				break;
			}
		}
	}
	if (!found)
		LockObject( id, eCOL_GameDataSync );
	SChangedProfile p = {CNetObjectBindLock(this, id, "CHANGEPROFILE"), aspect, profile};
	m_changedProfiles.push_back(p);
}

bool CNetContextState::DoSetAspectProfile( EntityId userID, uint8 aspectBit, uint8 profile, bool checkOwnership, bool informedGame, bool unlockUpdate )
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;
	SNetObjectID netID = GetNetID( userID );

	if (!netID || !GetContextObject(netID).main )
		return false;

	SContextObjectRef obj = GetContextObject(netID);
	NET_ASSERT( obj.main );

	//NetLog("DoSetAspectProfile:%.8x:%.2x:%.2x:%d:%d:%d:obj_%.4x:chnk_%.4d:%s",userID,aspectBit,profile,checkOwnership,informedGame,unlockUpdate,netID,obj.vAspectProfileChunks[BitIndex(aspectBit)][profile],obj.GetLockString());

	if (CountBits(aspectBit) != 1)
	{
#if ENABLE_DEBUG_KIT
		CryError("Can only set one aspect profile at a time");
#endif
		return false;
	}
	if (m_pContext && (m_pContext->ServerManagedProfileAspects() & aspectBit) == 0)
	{
#if ENABLE_DEBUG_KIT
		NetWarning("aspect %s is not server managed", m_pContext? m_pContext->GetAspectName(BitIndex(aspectBit)) : "unknown");
#endif
		return false;
	}

	if (checkOwnership && !obj.main->bOwned)
	{
#if ENABLE_DEBUG_KIT
		NetWarning("Trying to change aspect profile on %s to %d but we are not the owner", m_pContext? m_pContext->GetAspectName(BitIndex(aspectBit)) : "unknown", profile);
#endif
		return false;
	}

	if (!informedGame)
		LogChangedProfile( netID, aspectBit, profile );
	if (unlockUpdate)
		UnlockObject( netID, eCOL_GameDataSync );

	aspectBit = BitIndex(aspectBit);

	SNetObjectEvent event;
	event.event = eNOE_SetAspectProfile;
	event.id = netID;
	event.aspects = aspectBit;
	event.profile = profile;
	Broadcast( &event );

	const_cast<SContextObject*>(obj.main)->vAspectProfiles[aspectBit] = profile;

	if (m_multiplayer)
	{
		if (obj.xtra->vAspectData[aspectBit] != CMementoMemoryManager::InvalidHdl)
		{
			MMM().FreeHdl(obj.xtra->vAspectData[aspectBit]);
			const_cast<SContextObjectEx*>(obj.xtra)->vAspectData[aspectBit] = CMementoMemoryManager::InvalidHdl;
		}
		MarkObjectChanged( netID, 1<<aspectBit );
	}

	return true;
}

void CNetContextState::MarkObjectChanged( SNetObjectID id, uint8 aspectsChanged )
{
	ASSERT_GLOBAL_LOCK;
	if (!m_vObjects[id.id].userID)
		return;
	SNetObjectAspectChange change(aspectsChanged, 0);
	m_vGameChangedObjects.AddChange(id, change);
}

void CNetContextState::HandleAspectChanges( EntityId userID, uint8 changedAspects, INetChannel * pChannel )
{
	ASSERT_GLOBAL_LOCK;

	SNetObjectID netID = GetNetID( userID );
	if (!netID)
	{
		//<<TODO>> CHECK THIS!
		//NetWarning( "HandleAspectChanges (ChangeAspects or ResetAspects) called on unbound object %.8x", userID );
		return;
	}
	NET_ASSERT( GetContextObject(netID).main );

	if (!pChannel)
	{
		MarkObjectChanged( netID, changedAspects );
	}
	else
	{
		std::pair<SNetObjectID, SNetObjectAspectChange> changes[] = {
			std::make_pair( netID, SNetObjectAspectChange(changedAspects, 0) ),
			std::make_pair( SNetObjectID(), SNetObjectAspectChange() )
		};

		SNetObjectEvent event;
		event.event = eNOE_ObjectAspectChange;
		event.pChanges = changes;

		SendEventToChannelListener( pChannel, &event );
	}
}

void CNetContextState::ChangedAspects( EntityId userID, uint8 aspectBits, INetChannel * pChannel )
{
	if (!m_multiplayer)
		return;

	if (pChannel)
	{
		SCOPED_GLOBAL_LOCK;
		HandleAspectChanges( userID, aspectBits, pChannel );
	}
	else
	{
		FROM_GAME( &CNetContextState::NC_ChangedAspects, this, userID, aspectBits );
	}
}

void CNetContextState::NC_ChangedAspects( EntityId userID, uint8 aspectBits )
{
	ASSERT_GLOBAL_LOCK;
	HandleAspectChanges( userID, aspectBits, 0 );
}

void CNetContextState::ChangedTransform( EntityId userID, const Vec3& pos, const Quat& rot, float drawDist )
{
	FROM_GAME( &CNetContextState::NC_ChangedTransform, this, userID, pos, rot, drawDist );
}

void CNetContextState::NC_ChangedTransform( EntityId userID, Vec3 pos, Quat rot, float drawDist )
{
	ASSERT_GLOBAL_LOCK;
	if (!m_multiplayer)
		return; // TODO: make sure this is only relevant to multiplayer - Lin
	SNetObjectID id = GetNetID( userID );
	if (id)
	{
		SContextObject& obj = m_vObjects[id.id];
		SContextObjectEx& objx = m_vObjectsEx[id.id];
		if (obj.bAllocated)
		{
			objx.hasPosition = true;
			objx.position = pos;
			objx.hasDirection = true;
			objx.direction = rot * FORWARD_DIRECTION;
			objx.hasDrawDistance = drawDist > 0;
			objx.drawDistance = drawDist;
		}
	}
}

void CNetContextState::ChangedFov( EntityId id, float fov )
{
	FROM_GAME( &CNetContextState::NC_ChangedFov, this, id, fov );
}

void CNetContextState::NC_ChangedFov( EntityId userID, float fov )
{
	if (!m_multiplayer)
		return;
	SNetObjectID id = GetNetID( userID );
	if (id)
	{
		SContextObject& obj = m_vObjects[id.id];
		SContextObjectEx& objx = m_vObjectsEx[id.id];
		if (obj.bAllocated)
		{
			objx.hasFov = true;
			objx.fov = fov;
		}
	}
}

bool CNetContextState::SetSchedulingParams( EntityId objId, uint32 normal, uint32 owned )
{
	SCOPED_GLOBAL_LOCK;
	if (!m_multiplayer)
		return true;
	SNetObjectID netId = GetNetID(objId);
	if (!netId)
		return false;
	SContextObject& obj = m_vObjects[netId.id];
	SContextObjectEx& objx = m_vObjectsEx[netId.id];
	if (!obj.bAllocated)
		return false;
	objx.scheduler_owned = owned;
	objx.scheduler_normal = normal;
	return true;
}

bool CNetContextState::HandleRMI( SNetObjectID objID, bool bClient, TSerialize ser, CNetChannel * pChannel )
{
	ASSERT_GLOBAL_LOCK;
	uint8 funcId;
	ser.Value( "funcID", funcId );
	//const SContextObject * pObj = GetContextObject(objID);
	SContextObjectRef obj = GetContextObject(objID);
	//if (!pObj)
	if (!obj.main)
		return false;

	//	ASSERT_PRIMARY_THREAD;  - this function is threadsafe
	INetAtSyncItem * pItem = m_pGameContext->HandleRMI( bClient, obj.main->userID, funcId, ser );
	if (!pItem)
	{
		NetWarning("Failed handling %s RMI on entity %d @ index %d", bClient? "client" : "server", obj.main->userID, funcId);
		return false;
	}

	if (m_pContext)
	{
		TO_GAME(pItem, pChannel);
	}
	else
	{
		pItem->DeleteThis();
	}
	return true;
}

void CNetContextState::LogRMI( const char * function, ISerializable * pParams )
{
	SCOPED_GLOBAL_LOCK;
	// check fast path
	if (m_rmiLoggers.empty())
		return;
/* I am not using RMI logger to record CPPRMI's in demo recorder - Lin
#if INCLUDE_DEMO_RECORDING
	CRMILoggerImpl loggerImpl( function, m_rmiLoggers );
	CSimpleSerialize<CRMILoggerImpl> logger(loggerImpl);
	pParams->SerializeWith( TSerialize(&logger) );
#else
	NetWarning( "Demo recording not included" );
#endif
*/
}

void CNetContextState::LogCppRMI( EntityId id, IRMICppLogger * pLogger )
{
	SCOPED_GLOBAL_LOCK;
	// check fast path
	if (m_rmiLoggers.empty())
		return;

#if INCLUDE_DEMO_RECORDING
	for (std::vector<IRMILogger*>::iterator iter = m_rmiLoggers.begin(); iter != m_rmiLoggers.end(); ++iter)
	{
		(*iter)->LogCppRMI( id, pLogger );
	}
#else
	NetWarning( "Demo recording not included" );
#endif
}

void CNetContextState::BindObject( EntityId userID, uint8 aspectBits, bool bStatic )
{
	SCOPED_GLOBAL_LOCK;
	AllocateObject( userID, SNetObjectID(), aspectBits, true, bStatic? eST_Static : eST_Normal, NULL );
}

void CNetContextState::RebindObject( SNetObjectID netId, EntityId userId )
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;

	if (!m_pContext)
		return;

	if (!GetContextObject(netId).main)
	{
		TO_GAME(&CNetContextState::GC_UnboundObject, this, userId);
	}
	else
	{
		SContextObject& obj = m_vObjects[netId.id];
		NET_ASSERT(!obj.userID);
		obj.refUserID = obj.userID = userId;

		for (int i = 0; i < NumAspects; ++i)
			obj.vAspectProfiles[i] = obj.vAspectDefaultProfiles[i] = m_pGameContext->GetDefaultProfileForAspect(userId, 1<<i);

		// TODO: make sure only relevant to mp - Lin
		if (m_multiplayer)
		{
			SContextObjectEx& objx = m_vObjectsEx[netId.id];
			NET_ASSERT(objx.nAspectsEnabled == 0);
			for (int i=0; i<NumAspects; i++)
			{
				ASSERT_PRIMARY_THREAD;
				objx.vAspectData[i] = CMementoMemoryManager::InvalidHdl;
				objx.vRemoteAspectData[i] = CMementoMemoryManager::InvalidHdl;

				// probe object for which serialization chunks it will use for various profiles
				for (size_t c=0; c<MaxProfilesPerAspect; c++)
					objx.vAspectProfileChunks[i][c] = CNetwork::Get()->GetCompressionManager().GameContextGetChunkID( m_pGameContext, userId, 1<<i, c );
			}

			TurnAspectsOn(netId, 8);
		}

		SNetObjectEvent event;
		event.event = eNOE_BindObject;
		event.id = netId;
		Broadcast( &event );
	}
}

void CNetContextState::UnbindStaticObject( EntityId id )
{
	SNetObjectID netID = GetNetID(id);
	if (netID)
	{
		UnbindObject( netID, eUOF_CallGame );
	}
	else
	{
		TO_GAME( &CNetContextState::GC_UnboundObject, this, id );
	}
}

bool CNetContextState::UnbindObject( EntityId userID )
{
	SCOPED_GLOBAL_LOCK;

	SNetObjectID netID = GetNetID( userID );

	if (!netID)
	{
		//		NetWarning( "Tried to unbind not bound user id %d", userID );
		return true;
	}

	if (GetContextObject(netID).main->spawnType == eST_Static)
	{
		stl::push_back_unique( m_removedStaticEntities, userID );
		SNetObjectEvent event;
		event.event = eNOE_RemoveStaticEntity;
		event.userID = userID;
		Broadcast(&event);
	}

	return UnbindObject( netID, 0 );
}

bool CNetContextState::UnbindObject( SNetObjectID netID, uint32 flags )
{
	ASSERT_GLOBAL_LOCK;
	if (!GetContextObject(netID).main)
	{
#if ENABLE_DEBUG_KIT
		NetWarning( "No such object %s", netID.GetText() );
#endif
		return true;
	}
	SContextObject& obj = m_vObjects[netID.id];

	//NetLog("UNBIND OBJECT netID=%s entityID=%d '%s'", netID.GetText(), obj.userID, obj.GetName());
	if (!obj.userID)
	{
#if ENABLE_DEBUG_KIT
		NetWarning( "Already unbinding %s", netID.GetText() );
#endif
		return true;
	}

	if (obj.spawnType == eST_Collected && 0 == (flags & eUOF_AllowCollected))
		return false;

	// make sure the object is not in the change lists
	m_vGameChangedObjects.Remove( netID );

	// make sure we don't track this network id as being valid anymore
	NET_ASSERT(GetContextObject(netID).main);
	NET_ASSERT(obj.userID);

	// Assumption: User-id 0 is null
	EntityId userId = obj.userID;
	obj.userID = 0;

	SNetObjectEvent event;
	event.event = eNOE_UnbindObject;
	event.id = netID;
	event.userID = userId;
	Broadcast( &event );

	if (flags & eUOF_CallGame)
	{
		TO_GAME( &CNetContextState::GC_UnboundObject, this, userId );
	}

	UnboundObject( netID, "BIND" );
	return true;
}

bool CNetContextState::UnboundObject( SNetObjectID netID, const char * reason )
{
	MMM_REGION(m_pMMM);

	SCOPED_GLOBAL_LOCK;
	// TODO: temporary hack, shouldnt happen
	if (!GetContextObject(netID).main)
	{
#if ENABLE_DEBUG_KIT
		NetWarning("Object %s already unbound... not unbinding again", netID.GetText());
#endif
		return true;
	}

	// debug
	//NetLog("REF - %d %s [%d]", netID, reason, m_vObjects[netID].vLocks[eCOL_Reference]-1);
#if CHECKING_BIND_REFCOUNTS
	m_vObjects[netID.id].debug_whosLocking[reason] --;
#endif
	// ~debug

	SContextObject& obj = m_vObjects[netID.id];

	if (obj.spawnType == eST_Collected && obj.vLocks[eCOL_Reference] == 1)
		return false;

	if (UnlockObject(netID, eCOL_Reference))
	{
		ClearContextObjectCache();
		obj.bAllocated = false;
		m_freeObjects.push(netID.id);

		m_pNetIDs->erase( obj.refUserID );

		if (m_multiplayer)
		{
			SContextObjectEx& objx = m_vObjectsEx[netID.id];
			SNetObjectEvent event;
			event.event = eNOE_UnbindAspects;
			event.id = netID;
			event.aspects = objx.nAspectsEnabled;
			Broadcast( &event );
			for (int i = 0; i < NumAspects; i++)
			{
				MMM().FreeHdl(objx.vAspectData[i]);
				MMM().FreeHdl(objx.vRemoteAspectData[i]);
				objx.vAspectData[i] = CMementoMemoryManager::InvalidHdl;
				objx.vRemoteAspectData[i] = CMementoMemoryManager::InvalidHdl;
			}
		}

		SNetObjectEvent event;
		event.event = eNOE_UnboundObject;
		event.id = netID;
		Broadcast( &event );

		obj.pController = NULL;

		return true;
	}

	return false;
}

void CNetContextState::PulseObject( EntityId objId, uint32 pulseType )
{
	FROM_GAME( &CNetContextState::GC_PulseObject, this, objId, pulseType );
}

void CNetContextState::GC_PulseObject( EntityId objId, uint32 pulseType )
{
	ASSERT_GLOBAL_LOCK;
	if (!m_multiplayer)
		return;
	SNetObjectID id = GetNetID(objId);
	if (id)
	{
		SContextObject& obj = m_vObjects[id.id];
		SContextObjectEx& objx = m_vObjectsEx[id.id];
		if (obj.bAllocated)
		{
			objx.pPulseState->Pulse(pulseType);
		}
	}
}

bool CNetContextState::AllocateObject( EntityId userID, SNetObjectID netID, uint8 aspectBits, 
																 bool bOwned, ESpawnType spawnType, INetContextListenerPtr pController )
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;

	ClearContextObjectCache();

	if (!m_pContext)
		return false;

	uint8 profiles[NumAspects];
	for (int i=0; i<NumAspects; i++)
		profiles[i] = 0;
	if (userID)
	{
		for (int i=0; i<NumAspects; i++)
		{
			ASSERT_PRIMARY_THREAD;
			profiles[i] = m_pGameContext->GetDefaultProfileForAspect(userID, 1<<i);
			if (profiles[i] >= MaxProfilesPerAspect)
			{
#if ENABLE_DEBUG_KIT
				NetLog("Trying to fetch aspect-profiles for object %d, but it doesn't seem to have any.", userID);
				NetLog("Refusing to bind this object");
#endif
				return false;
			}
		}
	}

	//	NetLog( "AllocateObject: userID:%.8x netID:%.4x aspectBits:%.2x [%s %s] controller:%p",
	//		userID, netID, aspectBits, bOwned? "owned" : "not owned", bStatic? "static" : "dynamic", pController );

	if ( GetNetID(userID) )
	{
		//NetWarning( "Spawning an entity without removing it from the world first (entityid=%d; netid=%s); "
		//"This will 'probably' work... but no guarantees", userID, netID.GetText() );
		return false;
	}

	// allocate a reflecting network id for this user id
	// (userID's are likely sparse, and we can do better in the network if network id's are
	// dense, so we try to keep a reflecting structure here)
	if (!netID)
	{
		if (m_freeObjects.empty())
		{
			if (m_vObjects.size() == SNetObjectID::InvalidId)
				CryError( "WAY too many synchronized objects" );
			netID.id = m_vObjects.size();
			m_vObjects.push_back( SContextObject() );
			if (m_multiplayer)
				m_vObjectsEx.push_back( SContextObjectEx() ); // IMPORTANT: make sure m_vObjectsEx matches m_vObjects in multiplayer game
		}
		else
		{
			netID.id = m_freeObjects.top();
			m_freeObjects.pop();
		}
		NET_ASSERT(!m_vObjects[netID.id].bAllocated);
		do 
		{
			netID.salt = ++m_vObjects[netID.id].salt;
		} 
		while (!netID.salt);
	}
	else if (netID.id >= m_vObjects.size())
	{
		NET_ASSERT(netID.salt);
		m_vObjects.resize( netID.id+1, SContextObject() );
		if (m_multiplayer)
			m_vObjectsEx.resize( netID.id+1, SContextObjectEx() ); // IMPORTANT: 
		m_vObjects[netID.id].salt = netID.salt; // ??? maybe the other way
	}
	else if (m_vObjects[netID.id].bAllocated)
	{
		NET_ASSERT(netID.salt);
		if (spawnType == eST_Collected && m_vObjects[netID.id].spawnType == eST_Collected)
			return true;
#if ENABLE_DEBUG_KIT
		char buf1[64], buf2[64];
		NetWarning("Trying to allocate already allocated network id %s (was %s)", netID.GetText(buf1), SNetObjectID(netID.id, m_vObjects[netID.id].salt).GetText(buf2));
#endif
		NET_ASSERT(false);
		return false;
	}
	else
	{
		m_vObjects[netID.id].salt = netID.salt;
	}

	SContextObject& obj = m_vObjects[netID.id];

	NET_ASSERT(obj.salt == netID.salt);

	// TODO: check we're setting everything ok
	obj.pController = pController;
	obj.bAllocated = true;
	obj.bOwned = bOwned;
	obj.bControlled = bOwned;
	obj.spawnType = spawnType;
	obj.refUserID = obj.userID = userID;
	obj.parent = SNetObjectID();
	for (int i = 0; i < NumAspects; ++i)
		obj.vAspectProfiles[i] = obj.vAspectDefaultProfiles[i] = profiles[i];

	for (int i=0; i<eCOL_NUM_LOCKS; i++)
	{
		obj.vLocks[i] = 0;
	}

	if (m_multiplayer)
	{
		SContextObjectEx& objx = m_vObjectsEx[netID.id];
		objx.nAspectsEnabled = 0;
		objx.hasPosition = false;
		objx.hasDirection = false;
		objx.hasDrawDistance = false;
		objx.hasFov = false;
		objx.scheduler_normal = 0;
		objx.scheduler_owned = 0;
		//static CPriorityPulseState* pDefaultPulseState = NULL;
		//if (pDefaultPulseState == NULL)
		//	pDefaultPulseState = new CPriorityPulseState(); // since pPulseState is ref counted, and it requires dynamic allocation - Lin
		//objx.pPulseState = m_multiplayer ? new CPriorityPulseState() : pDefaultPulseState;
		class CPriorityPulseStateWrapper : public CPriorityPulseState
		{
		public:
			CPriorityPulseStateWrapper()
			{
				++g_objcnt.priorityPulseState;
			}
			~CPriorityPulseStateWrapper()
			{
				--g_objcnt.priorityPulseState;
			}
		};
		objx.pPulseState = new CPriorityPulseStateWrapper();
#if ENABLE_ASPECT_HASHING
		for (int i=0; i<NumAspects; i++)
			objx.hash[i] = 0;
#endif

		if (obj.userID)
		{
			for (int i=0; i<NumAspects; i++)
			{
				ASSERT_PRIMARY_THREAD;
				objx.vAspectData[i] = CMementoMemoryManager::InvalidHdl;
				objx.vRemoteAspectData[i] = CMementoMemoryManager::InvalidHdl;
				objx.vAspectDataVersion[i] = 0;

				// probe object for which serialization chunks it will use for various profiles
				for (size_t c=0; c<MaxProfilesPerAspect; c++)
					objx.vAspectProfileChunks[i][c] = CNetwork::Get()->GetCompressionManager().GameContextGetChunkID( m_pGameContext, userID, 1<<i, c );
			}
		}
		else
		{
			for (int i=0; i<NumAspects; i++)
			{
				// fill in some garbage data that the rest of the system knows about here... we'll get
				// real data on Rebind()
				objx.vAspectData[i] = CMementoMemoryManager::InvalidHdl;
				objx.vRemoteAspectData[i] = CMementoMemoryManager::InvalidHdl;
				objx.vAspectDataVersion[i] = 0;

				// probe object for which serialization chunks it will use for various profiles
				for (size_t c=0; c<MaxProfilesPerAspect; c++)
					objx.vAspectProfileChunks[i][c] = InvalidChunkID;
			}
		}

		MarkObjectChanged( netID, objx.nAspectsEnabled );

		TurnAspectsOn( netID, aspectBits * (spawnType != eST_Collected || bOwned) );

		// Leaving this here so the mistake is not repeated:
		//  the ContextView will set all changed bits on when it binds the object into the view
		//  (as new views need to sync everything anyway) - hence marking things as changed here
		//  is a bad idea (costs performance for zero benefit)
		//	ChangedAspects( userID, aspectBits );
	}

#if CHECKING_BIND_REFCOUNTS
	obj.debug_whosLocking.clear();
#endif

	if (userID)
		(*m_pNetIDs)[userID] = netID;

	UpdateAuthority( netID, false, false );

	SNetObjectEvent event;
	event.event = eNOE_BindObject;
	event.id = netID;
	Broadcast( &event );

	BoundObject( netID, "BIND" );

	return true;
}

void CNetContextState::SpawnedObject( EntityId userID )
{
	SCOPED_GLOBAL_LOCK;
	NET_ASSERT( m_spawnedObjectId == 0 );
	m_spawnedObjectId = userID;
}

void CNetContextState::DelegateAuthority( EntityId id, INetChannel * pControlling )
{
	FROM_GAME( &CNetContextState::NC_DelegateAuthority, this, id, pControlling );
}

void CNetContextState::NC_DelegateAuthority( EntityId userID, INetChannel * pChannel )
{
	ASSERT_GLOBAL_LOCK;
	SNetObjectID netID = GetNetID( userID );

	if (!netID || !GetContextObject(netID).main || IsDead())
	{
		return;
	}

	NET_ASSERT( GetContextObject(netID).main );
	SContextObject& obj = m_vObjects[netID.id];

#if ENABLE_DEBUG_KIT
	struct DebugHelper
	{
		DebugHelper(SContextObject& obj) : m_obj(obj) { Dump(">"); }
		SContextObject& m_obj;
		~DebugHelper() { Dump("<"); }
		void Dump(const char * prefix)
		{
			if (CVARS.LogLevel>1)
				NetLog("%s DelegateAuthority on '%s' cur:%s", 
				prefix, 
				m_obj.GetName(), 
				m_obj.pController? m_obj.pController->GetName().c_str() : "none" );
		}
	};
	DebugHelper debugHelper(obj);
#endif

	INetContextListenerPtr pController = NULL;
	if (pChannel)
	{
		pController = ((CNetChannel*)pChannel)->GetContextView();
		NET_ASSERT( pController != NULL );
	}

	if (obj.pController != pController)
	{
		if (obj.pController)
		{
			SNetObjectEvent event;
			event.event = eNOE_SetAuthority;
			event.authority = false;
			event.id = netID;
			SendEventToListener( obj.pController, &event );
		}
		if (pController)
		{
			SNetObjectEvent event;
			event.event = eNOE_SetAuthority;
			event.authority = true;
			event.id = netID;
			SendEventToListener( pController, &event );
		}
	}
	obj.pController = pController;
}

void CNetContextState::UpdateAuthority( SNetObjectID id, bool bAuth, bool bLocal )
{
	ASSERT_GLOBAL_LOCK;
	NET_ASSERT( GetContextObject(id).main );
	SContextObject& obj = m_vObjects[id.id];
	obj.bControlled = (obj.bOwned ^ bAuth) || (bAuth && bLocal);
#if DEBUG_LOGGING
	string className;
	if (IEntity *pEntity = gEnv->pEntitySystem->GetEntity(obj.userID))
		className = pEntity->GetClass()->GetName();

	NetLog( "%.8x[%s] '%s' controlled = %s (owned:%s auth:%s local:%s) class = %s", 
		obj.userID, 
		id.GetText(), 
		obj.GetName(),
		obj.bControlled? "true" : "false",
		obj.bOwned? "true" : "false",
		bAuth? "true" : "false",
		bLocal? "true" : "false",
		className.c_str()
		);
#endif
	TO_GAME( &CNetContextState::GC_ControlObject, this, id, obj.bControlled, LockObject(id, "CONT") );
}

void CNetContextState::EnableAspects( EntityId userID, uint8 aspectBits, bool enabled )
{
	SCOPED_GLOBAL_LOCK;
	SNetObjectID netID = GetNetID(userID);
	if (!netID)
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning( "EnableAspects called for invalid user id 0x%.8x", userID );
#endif
		return;
	}
	if (!m_multiplayer)
		return;
	SContextObject& obj = m_vObjects[netID.id];
	SContextObjectEx& objx = m_vObjectsEx[netID.id];
	if (enabled)
		aspectBits |= objx.nAspectsEnabled;
	else
		aspectBits = objx.nAspectsEnabled & ~aspectBits;
	ReconfigureObject( netID, aspectBits );

	SNetObjectEvent event;
	event.event = eNOE_ReconfiguredObject;
	event.id = netID;
	event.aspects = aspectBits;
	Broadcast( &event );
}

bool CNetContextState::IsBound( EntityId userID )
{
	SCOPED_GLOBAL_LOCK;
	return GetNetID(userID);
}

void CNetContextState::ReconfigureObject( SNetObjectID netID, uint8 aspects )
{
	ASSERT_GLOBAL_LOCK;
	if (!m_multiplayer)
		return;
	SContextObject& obj = m_vObjects[netID.id];
	SContextObjectEx& objx = m_vObjectsEx[netID.id];
	// what has changed
	uint8 aspectsReconfigured = aspects ^ objx.nAspectsEnabled;
	// what has been turned on
	uint8 aspectsOn = aspectsReconfigured & aspects;
	// what has been turned off
	uint8 aspectsOff = aspectsReconfigured & objx.nAspectsEnabled;

	NET_ASSERT( (aspectsOn & aspectsOff) == 0 );

	//	NetLog("ReconfigureObject %s aspectsOn:%d aspectsOff:%d", netID.GetText(), aspectsOn, aspectsOff);

	TurnAspectsOn( netID, aspectsOn );
	TurnAspectsOff( netID, aspectsOff );
}

void CNetContextState::TurnAspectsOn( SNetObjectID netID, uint8 aspects )
{
	ASSERT_GLOBAL_LOCK;
	if (!m_multiplayer)
		return;
	SContextObject& obj = m_vObjects[netID.id];
	SContextObjectEx& objx = m_vObjectsEx[netID.id];

	NET_ASSERT( (aspects & objx.nAspectsEnabled) == 0 );
	// safety
	aspects &= ~objx.nAspectsEnabled;

	// setup an initial aspect structure
	int i;
	CBitIter itAspects(aspects);
	while (itAspects.Next(i))
	{
		if (0 == (m_pContext->DeclaredAspects() & (1<<i)))
		{
#if ENABLE_DEBUG_KIT
			NetWarning("Attempt to enable an undeclared aspect (%d)", i);
#endif
			aspects &= ~(1<<i);
			continue;
		}
		else if (0 == (m_pContext->ServerManagedProfileAspects() & (1<<i)))
		{
			bool allEmpty = true;
			for (int a=0; allEmpty && a<MaxProfilesPerAspect; a++)
				allEmpty &= CNetwork::Get()->GetCompressionManager().IsChunkEmpty( objx.vAspectProfileChunks[i][a] );
			if (allEmpty)
			{
				aspects &= ~(1<<i);
				continue;
			}
		}
	}

	SNetObjectEvent event;
	event.event = eNOE_BindAspects;
	event.id = netID;
	event.aspects = aspects;
	Broadcast( &event );

	objx.nAspectsEnabled |= aspects;

	MarkObjectChanged( netID, aspects );
}

void CNetContextState::TurnAspectsOff( SNetObjectID netID, uint8 aspects )
{
	ASSERT_GLOBAL_LOCK;
	if (!m_multiplayer)
		return;
	SContextObject& obj = m_vObjects[netID.id];
	SContextObjectEx& objx = m_vObjectsEx[netID.id];

	NET_ASSERT( (aspects & objx.nAspectsEnabled) == aspects );

	int i;
	CBitIter itAspects(aspects);
	while (itAspects.Next(i))
	{
		if (0 == (m_pContext->DeclaredAspects() & (1<<i)))
		{
#if ENABLE_DEBUG_KIT
			NetWarning("Attempt to disable an undeclared aspect (%d)", i);
#endif
			aspects &= ~(1<<i);
			continue;
		}
	}

	SNetObjectEvent event;
	event.event = eNOE_UnbindAspects;
	event.id = netID;
	event.aspects = aspects;
	Broadcast( &event );

	objx.nAspectsEnabled &= ~aspects;
}

bool CNetContextState::RemoteContextHasAuthority( INetChannel * pChannel, EntityId id )
{
	SCOPED_GLOBAL_LOCK;
	SNetObjectID netID = GetNetID(id);
	if (!netID)
		return false;
	//const SContextObject * pContextObject = GetContextObject(netID);
	SContextObjectRef obj = GetContextObject(netID);
	if (!obj.main)
		return false;
	if (!obj.main->pController)
		return false;
	return ((CContextView*)&*obj.main->pController)->Parent() == pChannel;
}

void CNetContextState::RemoveRMIListener( IRMIListener * pListener )
{ 
	SCOPED_GLOBAL_LOCK;
	SNetObjectEvent evt; 
	evt.event = eNOE_RemoveRMIListener; 
	evt.pRMIListener = pListener; 
	Broadcast( &evt ); 
}

void CNetContextState::PerformRegularCleanup()
{
	ASSERT_GLOBAL_LOCK;
	if (m_subscriptions.empty())
		return;

	if (m_bInCleanup)
		return;
	m_bInCleanup = true;
	INetContextListenerPtr pListener = m_subscriptions[m_cleanupMember % m_subscriptions.size()].pListener;
	pListener->PerformRegularCleanup();
	m_bInCleanup = false;
}

void CNetContextState::GetAllObjects( std::vector<SNetObjectID>& objs )
{
	objs.resize(0);
	for (TContextObjects::iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
	{
		if (iter->bAllocated && iter->userID)
			objs.push_back(SNetObjectID(iter - m_vObjects.begin(), iter->salt));
	}
}

INetContextListenerPtr CNetContextState::FindListener( const char * name )
{
	ASSERT_GLOBAL_LOCK;
	if (name[0] == 0)
		return 0;
	for (TSubscriptions::iterator iter = m_subscriptions.begin(); iter != m_subscriptions.end(); ++iter)
		if (iter->pListener->GetName().find(name) != string::npos)
			return iter->pListener;
	return NULL;
}

bool CNetContextState::AllInOrPastState( EContextViewState state )
{
	for (TSubscriptions::iterator iter = m_subscriptions.begin(); iter != m_subscriptions.end(); ++iter)
	{
		INetChannel * pChannel = iter->pListener->GetAssociatedChannel();
		if (pChannel)
		{
			if (pChannel->GetName() == string("DemoRecorder"))
				continue; // HACK!!!
			CContextView * pView = ((CNetChannel*)pChannel)->GetContextView();
			if (!pView->IsBeforeState(eCVS_InGame))
				return false;
			if (!pView->IsPastOrInState(state))
				return false;
		}
	}
	return true;
}

uint8 CNetContextState::UpdateAspectData( SNetObjectID id, uint8 fetchAspects )
{
	MMM_REGION(m_pMMM);

	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_NETWORK);
	ASSERT_GLOBAL_LOCK;
	NET_ASSERT( GetContextObject(id).main );
	if (!m_multiplayer || !m_pContext)
		return 0;
	SContextObject& obj = m_vObjects[id.id];
	SContextObjectEx& objx = m_vObjectsEx[id.id];
	SContextObjectRef ref(&obj, &objx);

	uint8 allowedAspects = 0;
	if (obj.bOwned)
		allowedAspects = objx.nAspectsEnabled;
	else if (obj.bControlled)
		allowedAspects = m_pContext->DelegatableAspects() & objx.nAspectsEnabled;

	uint8 takenAspects = 0;

#if ENABLE_ASPECT_HASHING
	uint8 hashAspects = fetchAspects & objx.nAspectsEnabled;
#endif
	fetchAspects &= allowedAspects;

	for (int i=0; i<NumAspects; i++)
	{
		bool enabled = (objx.nAspectsEnabled & (1<<i)) != 0;
		TMemHdl oldHdl = CMementoMemoryManager::InvalidHdl;
		if (enabled && objx.vAspectData[i] == CMementoMemoryManager::InvalidHdl)
		{
			fetchAspects |= (1<<i);
		}
		else if (enabled && ((1<<i)&fetchAspects) && objx.vAspectData[i] != CMementoMemoryManager::InvalidHdl && ((1<<i)&allowedAspects))
		{
			oldHdl = objx.vAspectData[i];
			objx.vAspectData[i] = CMementoMemoryManager::InvalidHdl;
		}
		else if (!enabled && objx.vAspectData[i] != CMementoMemoryManager::InvalidHdl)
		{
			oldHdl = objx.vAspectData[i];
			objx.vAspectData[i] = CMementoMemoryManager::InvalidHdl;
			fetchAspects &= ~(1<<i);
		}
		if (fetchAspects & (1<<i))
		{
			CMementoStreamAllocator streamAllocatorForNewState(&MMM());
			size_t sizeHint = 8;
			if (oldHdl != CMementoMemoryManager::InvalidHdl)
				sizeHint = MMM().GetHdlSize(oldHdl);
			CByteOutputStream stm( &streamAllocatorForNewState, sizeHint );
			NET_ASSERT(objx.vAspectData[i] == CMementoMemoryManager::InvalidHdl);
#if LOG_BUFFER_UPDATES
			if (CNetCVars::Get().LogBufferUpdates)
				NetLog("[buf] pull %s:%s", id.GetText(), m_pContext->GetAspectName(i));
#endif
			if (m_pContext->TimestampedAspects() & (1<<i))
			{
				stm.PutTyped<CTimeValue>() = m_localPhysicsTime;
			}
			if (!CNetwork::Get()->GetCompressionManager().GameContextRead( m_pGameContext, &stm, obj.userID, 1<<i, (obj.vAspectProfiles[i] >= MaxProfilesPerAspect? 0 : obj.vAspectProfiles[i]), GetChunkIDFromObject(ref, i) ))
			{
				MMM().FreeHdl(streamAllocatorForNewState.GetHdl());
				objx.vAspectData[i] = oldHdl;
				oldHdl = CMementoMemoryManager::InvalidHdl;
			}
			else
			{
				bool take = true;
				if (oldHdl != CMementoMemoryManager::InvalidHdl)
				{
					if (stm.GetSize() == MMM().GetHdlSize(oldHdl))
					{
						int ofs = 0;
						if (m_pContext->TimestampedAspects() & (1<<i))
							ofs += sizeof(CTimeValue);
						const char * pOld = (const char *)MMM().PinHdl(oldHdl);
						const char * pNew = (const char *)MMM().PinHdl(streamAllocatorForNewState.GetHdl());
						int cmpSz = stm.GetSize() - ofs;
						if (cmpSz > 0 && 0 == memcmp(pOld + ofs, pNew + ofs, cmpSz))
							take = false;
						else if (!cmpSz)
							take = false;
					}
				}
				if (take)
				{
					objx.vAspectData[i] = streamAllocatorForNewState.GetHdl();
					MMM().ResizeHdl( objx.vAspectData[i], stm.GetSize() );
					objx.vAspectDataVersion[i] ++;
					takenAspects |= (1<<i);
				}
				else
				{
					MMM().FreeHdl(streamAllocatorForNewState.GetHdl());
					objx.vAspectData[i] = oldHdl;
					oldHdl = CMementoMemoryManager::InvalidHdl;
				}
			}
		}
		if (oldHdl != CMementoMemoryManager::InvalidHdl)
		{
			MMM().FreeHdl(oldHdl);
		}
#if ENABLE_ASPECT_HASHING
		if (hashAspects & (1<<i))
		{
			ASSERT_PRIMARY_THREAD;
			objx.hash[i] = m_pGameContext->HashAspect( obj.userID, 1<<i );
		}
#endif
	}

	//NET_ASSERT((takenAspects & ~allowedAspects) == 0);
	//NET_ASSERT((takenAspects & ~fetchAspects) == 0);

	return takenAspects;
}

void CNetContextState::LogBreak( const SNetBreakDescription& breakage )
{
	SCOPED_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

	m_pLoggedBreakage->push_back(SNetIntBreakDescription());
	SNetIntBreakDescription& des = m_pLoggedBreakage->back();

	des.pMessagePayload = breakage.pMessagePayload;
	for (int i=0; i<breakage.nEntities; i++)
	{
		AllocateObject( breakage.pEntities[i], SNetObjectID(), 8, true, eST_Collected, NULL );
#if ENABLE_DEBUG_KIT
		NetLog( "Allocated collected object %s for index %d; entity id = %d", GetNetID(breakage.pEntities[i]).GetText(), i, breakage.pEntities[i] );
#endif
		des.spawnedObjects.push_back(breakage.pEntities[i]);
	}

	SNetObjectEvent event;
	event.event = eNOE_GotBreakage;
	event.pBreakage = &des;
	Broadcast( &event );
}

void CNetContextState::NotifyGameOfAspectUpdate( SNetObjectID objID, uint8 nAspect, CNetChannel * pChannel, CTimeValue remoteTime )
{
	if (m_pContext)
		m_vNetChangeLog.push_back( SChannelChange(objID, nAspect, pChannel, remoteTime) );
}

void CNetContextState::ForceUnbindObject( SNetObjectID id )
{
/*
	SContextObjectRef obj = GetContextObject(id);
	if (obj.main)
	{
		TO_GAME(&CNetContextState::GC_UnboundObject, this, obj.main->userID);
	}
*/
	UnbindObject( id, eUOF_CallGame | eUOF_AllowCollected );
}

/*
 * GameContext calls from the work queue
 */

void CNetContextState::GC_UnboundObject( EntityId id )
{
	ASSERT_GLOBAL_LOCK;
	ASSERT_PRIMARY_THREAD;
	ENSURE_REALTIME;
	m_pGameContext->UnboundObject(id);
}

static const float SPAM_SECONDS = 3.0f;

void CNetContextState::GC_BeginContext(CTimeValue waitStarted)
{
	ASSERT_PRIMARY_THREAD;

	SCOPED_GLOBAL_LOCK;
	CContextEstablisherPtr pEstablisher(new CContextEstablisher);
	m_pGameContext->InitGlobalEstablishmentTasks( pEstablisher, m_token );
	RegisterEstablisher(NULL, pEstablisher);
	SetEstablisherState(NULL, eCVS_InGame);
}

void CNetContextState::GC_ControlObject( SNetObjectID id, bool controlled, CNetObjectBindLock lk )
{
	ASSERT_GLOBAL_LOCK;
	ASSERT_PRIMARY_THREAD;
	ENSURE_REALTIME;
	if (m_vObjects[id.id].userID)
		m_pGameContext->ControlObject( m_vObjects[id.id].userID, controlled );
}

void CNetContextState::GC_BoundObject( std::pair<EntityId, uint8> p )
{
	ASSERT_GLOBAL_LOCK;
	ASSERT_PRIMARY_THREAD;
	ENSURE_REALTIME;
	m_pGameContext->BoundObject( p.first, p.second );
}

void CNetContextState::GC_SendPostSpawnEntities( CContextViewPtr pView )
{
	ASSERT_GLOBAL_LOCK;
	ENSURE_REALTIME;
	if (pView->IsDead())
		return;
	for (TContextObjects::iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
	{
		if (iter->bAllocated && iter->userID)
		{
			ASSERT_PRIMARY_THREAD;
			if (!m_pGameContext->SendPostSpawnObject( iter->userID, pView->Parent() ))
			{
				SNetObjectID id(iter - m_vObjects.begin(), iter->salt);
#if ENABLE_DEBUG_KIT
				NetWarning("Entity %d (%s) was not found during post spawning of objects: removing it", iter->userID, id.GetText());
#endif
				UnbindObject(iter->userID);
				return;
			}
		}
	}
	//pView->FinishLocalState();
}

void CNetContextState::GC_SetAspectProfile( uint8 aspect, uint8 profile, SNetObjectID netID, CNetObjectBindLock lock )
{
	ASSERT_GLOBAL_LOCK;
	ENSURE_REALTIME;
	ASSERT_PRIMARY_THREAD;
	if (m_vObjects[netID.id].userID)
		m_pGameContext->SetAspectProfile( m_vObjects[netID.id].userID, aspect, profile );
	UnlockObject( netID, eCOL_GameDataSync );
}

void CNetContextState::GC_EndContext()
{
	ASSERT_GLOBAL_LOCK;
	ASSERT_PRIMARY_THREAD;
	ENSURE_REALTIME;
	//m_pGameContext->EndContext();
}

void CNetContextState::GC_SetParentObject( EntityId objId, EntityId parentId )
{
	ASSERT_GLOBAL_LOCK;
	ENSURE_REALTIME;
	SNetObjectID objNetId = GetNetID(objId);
	SNetObjectID parentNetId = GetNetID(parentId);
	if (!GetContextObject(objNetId).main || !GetContextObject(parentNetId).main)
		return;
	m_vObjects[objNetId.id].parent = parentNetId;
}

// context view configure context stuff

void CNetContextState::NC_BroadcastSimpleEvent( ENetObjectEvent event )
{
	ENSURE_REALTIME;
	SNetObjectEvent ev;
	ev.event = event;
	Broadcast(&ev);
}

void CNetContextState::RegisterEstablisher( INetContextListenerPtr pListener, CContextEstablisherPtr pEst )
{
	ASSERT_GLOBAL_LOCK;
	m_allEstablishers[pListener] = pEst;
	if (m_allEstablishers.size() == 1)
	{
		TO_GAME_LAZY(&CNetContextState::GC_Lazy_TickEstablishers, this);
	}
}

void CNetContextState::SetEstablisherState( INetContextListenerPtr pListener, EContextViewState state )
{
	ASSERT_GLOBAL_LOCK;
	EstablishersMap::iterator iter = m_allEstablishers.find(pListener);
	if (iter == m_allEstablishers.end())
#if ENABLE_DEBUG_KIT
		NetWarning("Couldn't find establisher trying to set state %d for %s", state, pListener? pListener->GetName().c_str() : "Context");
#else
		;
#endif
	else
		iter->second.state = state;
}

void CNetContextState::GC_Lazy_TickEstablishers()
{
	_smart_ptr<CNetContextState> pThis = this;

	// fetch
	{
		SCOPED_GLOBAL_LOCK;
		m_currentEstablishers = m_allEstablishers;
	}

	// execute
	for (EstablishersMap::iterator iter = m_currentEstablishers.begin(); iter != m_currentEstablishers.end(); ++iter)
	{
		INetContextListenerPtr pListener = iter->first;
		SContextEstablisher& est = iter->second;

		if (IsDead() || (pListener && pListener->IsDead()))
		{
			est.pEst->Fail( eDC_Unknown, "Couldn't establish context" );
			est.phase = eCEP_Dead;
		}
		else
		{
#if ENABLE_DEBUG_KIT
			est.pEst->DebugDraw();
#endif
			bool done = false;
			SContextEstablishState s;
			s.contextState = est.state;
			s.pSender = pListener? pListener->GetAssociatedChannel() : NULL;
			while (!done)
			{
				switch (est.phase)
				{
				case eCEP_Fresh:
					est.pEst->Start();
					est.phase = eCEP_Working;
					break;
				case eCEP_Working:
					if (!est.pEst->StepTo( s ) || est.pEst->IsDone())
						est.phase = eCEP_Dead;
					else
						done = true;
					break;
				case eCEP_Dead:
					done = true;
					break;
				}
			}
		}
	}

	// commit
	SCOPED_GLOBAL_LOCK;
	for (EstablishersMap::iterator iterCur = m_currentEstablishers.begin(); iterCur != m_currentEstablishers.end(); ++iterCur)
	{
		EstablishersMap::iterator iterEst = m_allEstablishers.find(iterCur->first);

		SContextEstablisher& estCur = iterCur->second;
		SContextEstablisher& estEst = iterEst->second;
		if (estEst.pEst != estCur.pEst)
		{
			estEst.pEst->Fail(eDC_Unknown, "Supersceded");
		}
		else if (estCur.phase != eCEP_Dead)
		{
			estEst.phase = estCur.phase;
		}
		else
		{
			m_allEstablishers.erase(iterEst);
		}
	}
	m_currentEstablishers.clear();
	if (!m_allEstablishers.empty())
	{
		TO_GAME_LAZY(&CNetContextState::GC_Lazy_TickEstablishers, this);
	}
}

/*
IVoiceContext *CNetContextState::GetVoiceContext()
{
	return m_pVoiceContext;
}

CVoiceContext *CNetContextState::GetVoiceContextImpl()
{
	return m_pVoiceContext;
}
*/

int CNetContextState::RegisterPredictedSpawn(INetChannel * pChannel, EntityId id)
{
	SCOPED_GLOBAL_LOCK;

	if (!pChannel)
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Spawn predicted on a null channel; this is not valid");
#endif
		return 0;
	}

	if (!id)
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Spawn predicted on an invalid entity id; this is not valid -- ignoring");
#endif
		return 0;
	}

	if (GetNetID(id))
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Entity id %d was predicted to be spawned but is already bound to the network system; this is invalid (already bound as %s)", id, GetNetID(id).GetText());
#endif
		return 0;
	}

	SNetObjectEvent event;
	event.event = eNOE_PredictSpawn;
	event.predictedEntity = id;
	SendEventToChannelListener( pChannel, &event );

	return id;
}

void CNetContextState::RegisterValidatedPredictedSpawn(INetChannel * pChannel, int predictionHandle, EntityId id)
{
	SCOPED_GLOBAL_LOCK;

	if (!pChannel)
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Spawn predicted on a null channel; this is not valid");
#endif
		return;
	}

	if (!id)
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Spawn predicted on an invalid entity id; this is not valid -- ignoring");
#endif
		return;
	}

	if (GetNetID(id))
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Entity id %d was predicted to be spawned but is already bound to the network system; this is invalid (already bound as %s)", id, GetNetID(id).GetText());
#endif
		return;
	}

	SNetObjectEvent event;
	event.event = eNOE_ValidatePrediction;
	event.predictedEntity = id;
	event.predictedReference = predictionHandle;
	SendEventToChannelListener( pChannel, &event );
}

CNetObjectBindLock CNetContextState::LockObject(SNetObjectID id, const char * why)
{
	return CNetObjectBindLock(this, id, why);
}

void CNetContextState::Resaltify( SNetObjectID& id )
{
	if (!id) 
		return;
	if (m_vObjects.size() <= id.id)
	{
		id.salt = 1;
	}
	else
	{
		id.salt = m_vObjects[id.id].salt;
	}
}

IGameContext * CNetContextState::GetGameContext()
{
	return m_pGameContext;
}

void CNetContextState::RequestRemoteUpdate( EntityId id, uint8 aspects )
{
	FROM_GAME(&CNetContextState::NC_RequestRemoteUpdate, this, id, aspects);
}

void CNetContextState::NC_RequestRemoteUpdate( EntityId id, uint8 aspects )
{
	SNetObjectEvent event;
	event.event = eNOE_PartialUpdate;
	event.id = GetNetID(id);
	if (!event.id)
		return;

	CBitIter itAspects(aspects);
	int i;
	while (itAspects.Next(i))
	{
		event.aspects = i;
		Broadcast( &event );
	}

	MarkObjectChanged( event.id, aspects );
}

/*
 * Debug support routines go here
 */

void CNetContextState::GetMemoryStatistics( ICrySizer * pSizer )
{
	SIZER_COMPONENT_NAME(pSizer, "CNetContextState");

	MMM_REGION(m_pMMM);

	pSizer->Add(*this);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_subscriptions");
		pSizer->AddContainer(m_subscriptions);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_vObjects");
		pSizer->AddContainer(m_vObjects);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_vObjectsEx");
		pSizer->AddContainer(m_vObjectsEx);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_mNetIDs");
		pSizer->AddContainer(*m_pNetIDs);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_channelSubscriptions");
		pSizer->AddContainer(m_channelSubscriptions);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_tempSubscriptions");
		pSizer->AddContainer(m_tempSubscriptions);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_removedStaticEntities");
		pSizer->AddContainer(m_removedStaticEntities);
	}

/*
	if (m_pVoiceContext)
		m_pVoiceContext->GetMemoryStatistics(pSizer);
*/

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_allEstablishers");
		pSizer->AddContainer(m_allEstablishers);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_currentEstablishers");
		pSizer->AddContainer(m_currentEstablishers);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_changeBuffer");
		pSizer->AddContainer(m_changeBuffer);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_loggedBreakage");
		pSizer->AddContainer(*m_pLoggedBreakage);
	}

	m_vGameChangedObjects.GetMemoryStatistics(pSizer);
	pSizer->AddContainer(m_vNetChangeLog);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext.m_vObjects.TMemHdl");
		for (size_t i = 0; i < m_vObjectsEx.size(); ++i)
			for (size_t j = 0; j < NumAspects; ++j)
				MMM().AddHdlToSizer( m_vObjectsEx[i].vAspectData[j], pSizer );
	}
	
//	m_pContext->GetMemoryStatistics(pSizer);
}

void CNetContextState::NetDump( ENetDumpType type )
{
	ASSERT_GLOBAL_LOCK;

	switch (type)
	{
	case eNDT_ObjectState:
		NetLog("Bound objects:");
		for (TNetIDMap::const_iterator iterNetIDs = m_pNetIDs->begin(); iterNetIDs != m_pNetIDs->end(); ++iterNetIDs)
		{
			IEntity * pEntity = gEnv->pEntitySystem->GetEntity(iterNetIDs->first);
			const char * name = pEntity? pEntity->GetName() : "<<no name>>";

			SContextObjectRef obj = GetContextObject(iterNetIDs->second);
			if (obj.main && obj.xtra)
			{
				NetLog( "%d %s %s flags:%d%d%d%d aspects:%.2x",
					iterNetIDs->first, iterNetIDs->second.GetText(), name,
					obj.main->bAllocated, obj.main->bControlled, obj.main->spawnType, obj.main->bOwned,
					obj.xtra->nAspectsEnabled );
			}
			else
			{
				NetLog( "%d %s %s", iterNetIDs->first, iterNetIDs->second.GetText(), name );
			}
		}
		break;
	}
}

const char * SContextObject::GetName() const
{
	ASSERT_GLOBAL_LOCK;
	if (userID)
	{
		if (IEntity * pEntity = gEnv->pEntitySystem->GetEntity(userID))
			return pEntity->GetName();
	}
	return "<unknown entity>";
}

void CNetContextState::SafelyUnbind( EntityId id )
{
	FROM_GAME(&CNetContextState::GC_CompleteUnbind, this, id);
}

void CNetContextState::GC_CompleteUnbind( EntityId id )
{
	ASSERT_PRIMARY_THREAD;
	m_pGameContext->CompleteUnbind(id);
}

void CNetContextState::DrawDebugScreens()
{
#if ENABLE_DEBUG_KIT
	if (CNetCVars::Get().ShowObjLocks)
	{
		ITextModeConsole * pTMC = gEnv->pSystem->GetITextModeConsole();
		int textY = 0;
		string out;
		for (TContextObjects::const_iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
		{
			out.resize(0);
			if (iter->bAllocated)
			{
				out += string().Format("%s: eid=%.8x nid=%s", iter->GetName(), iter->userID, SNetObjectID(iter-m_vObjects.begin(), iter->salt).GetText());
				for (int i=0; i<eCOL_NUM_LOCKS; i++)
					out += string().Format(" %d", iter->vLocks[i]);
#if CHECKING_BIND_REFCOUNTS
				for (std::map<string, int>::const_iterator it = iter->debug_whosLocking.begin(); it != iter->debug_whosLocking.end(); ++it)
					if (it->second)
						out += string().Format(" %s:%d", it->first.c_str(), it->second);
#endif
				NetQuickLog( false, 0.0f, "%s", out.c_str() );
				if (pTMC)
					pTMC->PutText( 0, textY++, out.c_str() );
			}
		}
	}

	/*
 	 * MEMORY USAGE DEBUGGING
	 */
	if (CVARS.MemInfo & eDMM_Context)
	{
		typedef std::map< IEntityClass*, TNetObjectIDs > ClassMap;
		ClassMap cm;
		for (TContextObjects::const_iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
		{
			if (iter->bAllocated && iter->userID)
				cm[gEnv->pEntitySystem->GetEntity(iter->userID)->GetClass()].push_back(SNetObjectID(iter - m_vObjects.begin(), iter->salt));
		}
		static const int xstart = 250;
		int x = xstart;
		IRenderer * pRend = gEnv->pRenderer;
		float white[] = {1,1,1,1};
		for (ClassMap::const_iterator cmiter = cm.begin(); cmiter != cm.end(); ++cmiter)
		{
			pRend->Draw2dLabel( x, 100, 1, white, false, cmiter->first->GetName() );
			x += 50;
		}
		int y = 110;
		for (TSubscriptions::const_iterator iter = m_subscriptions.begin(); iter != m_subscriptions.end(); ++iter)
		{
			x = xstart;
			pRend->Draw2dLabel( x - 200, y, 1, white, false, iter->pListener->GetName() );
			for (ClassMap::const_iterator cmiter = cm.begin(); cmiter != cm.end(); ++cmiter)
			{
				SObjectMemUsage mu;
				for (TNetObjectIDs::const_iterator noiter = cmiter->second.begin(); noiter != cmiter->second.end(); ++noiter)
					mu += iter->pListener->GetObjectMemUsage( *noiter );
				pRend->Draw2dLabel( x, y, 1, white, false, "%d/%d[%d]", mu.required, mu.used, mu.instances );
				x += 50;
			}
			y += 10;
		}
	}
#endif
}
