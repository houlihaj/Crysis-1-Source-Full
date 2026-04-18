/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  implements communications between two machines, with 
               context setup
 -------------------------------------------------------------------------
 History:
 - 26/07/2004   : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "NetChannel.h"
#include "IDataProbe.h"
#include "ITimer.h"
#include "AntiCheat/DefenceContext.h"
#include "Context/ClientContextView.h"
#include "Context/ServerContextView.h"
#include "StringUtils.h"
#include "FixedSizeArena.h"

// must be greater than zero; scott chose this one (it's his birthday)
TNetChannelID CNetChannel::m_nextLocalChannelID = 989;

int g_nChannels = 0;

static const float STATS_UPDATE_INTERVAL = 0.1f;
static const CTimeValue KEEPALIVE_TIMEOUT = 1.0f;
static const CTimeValue KEEPALIVE_REPLY_TIMEOUT = 0.7f;

CNetChannel::CNetChannel( const TNetAddress& ipRemote, const CExponentialKeyExchange::KEY_TYPE& key, uint32 remoteSocketCaps ) :
	m_pMMM(new CMementoMemoryManager),
	m_ctpEndpoint(m_pMMM),
	m_ip(ipRemote),
	m_privateKey(key),
	m_pGameChannel(NULL),
	m_pContextView(NULL),
	m_nKeepAliveTimer(0.0f),
	m_nKeepAliveTimer2(0.0f),
	m_nKeepAlivePingTimer(0.0f),
	m_nKeepAliveReplyTimer(0.0f),
	m_bDead(false),
	m_pNub(NULL),
	m_bConnectionEstablished(false),
	m_bForcePacketSend(true),
#if USE_DEFENCE
	m_pDefenceContext(NULL),
#endif
	m_localChannelID(m_nextLocalChannelID++),
	m_remoteChannelID(0),
	m_timer(0),
	m_statsTimer(0),
	m_pendingStuffBeforeDead(0),
	m_sentDisconnect(false),
	m_lastVoiceTransmission(0.0f),
  m_preordered(false),
	m_pingLock(false),
	m_profileId(0),
	m_lastPingSent(0.0f),
	m_remoteSocketCaps(remoteSocketCaps),
	m_gotFakePacket(false),
#if ENABLE_DEBUG_KIT
	m_pDebugHistory(0),
	m_pSinceSent(0),
	m_pElapsedRemote(0),
	m_pPing(0),
#endif
	m_nStrongCaptureRMIs(0),
	m_fastLookupId(-1)
{
	SCOPED_GLOBAL_LOCK;
	++g_nChannels;
	++g_objcnt.channel;
}

CNetChannel::~CNetChannel()
{
	SCOPED_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

	--g_nChannels;
	--g_objcnt.channel;

	if (m_pContextView)
		m_pContextView->OnChannelDestroyed();

	m_pContextView = 0;

#if USE_DEFENCE
	delete m_pDefenceContext;
#endif //NOT_USE_DEFENCE

	while (!m_queuedForSyncPackets.empty())
	{
		MMM().FreeHdl(m_queuedForSyncPackets.front());
		m_queuedForSyncPackets.pop();
	}

	TIMER.CancelTimer( m_timer );
	TIMER.CancelTimer( m_statsTimer );

#if ENABLE_DEBUG_KIT
	if (m_pDebugHistory)
	{
		ASSERT_PRIMARY_THREAD;
		m_pDebugHistory->Release();
	}
#endif

	m_ctpEndpoint.ChangeSubscription( this, 0 );

	if (m_fastLookupId >= 0)
		CNetwork::Get()->UnregisterFastLookup(m_fastLookupId);
}

void CNetChannel::SetClient( INetContext * pGameContext, bool cheats )
{
	SCOPED_GLOBAL_LOCK;
	if (m_pContextView 
#if USE_DEFENCE
		|| m_pDefenceContext
#endif
		)
		CryError("Cannot set two different types of channel on the same channel");
	if (!pGameContext && !cheats)
		CryError("Must set either context or cheat protection or both to make a client!");
	if (pGameContext)
		m_pContextView = new CClientContextView( this, (CNetContext*)pGameContext );

#if USE_DEFENCE
	if (cheats)
		m_pDefenceContext = new CClientDefence( this );
#endif //NOT_USE_DEFENCE
}

void CNetChannel::SetServer( INetContext * pServerContext, bool cheats )
{
	SCOPED_GLOBAL_LOCK;
	if (m_pContextView 
#if USE_DEFENCE
		|| m_pDefenceContext
#endif
		)
		CryError("Cannot set two different types of channel on the same channel");
	if (!pServerContext && !cheats)
		CryError("Must set either context or cheat protection or both to make a server!");
	if (pServerContext)
		m_pContextView = new CServerContextView( this, (CNetContext*)pServerContext );

#if USE_DEFENCE
	if (cheats)
		m_pDefenceContext = new CServerDefence( this );
#endif //NOT_USE_DEFENCE

	m_fastLookupId = CNetwork::Get()->RegisterForFastLookup(this);
}

void CNetChannel::PerformRegularCleanup()
{
	m_ctpEndpoint.PerformRegularCleanup();
	if (m_pContextView)
		m_pContextView->PerformRegularCleanup();
}

string CNetChannel::GetRemoteAddressString()const
{
	return RESOLVER.ToString( m_ip );
}

bool CNetChannel::GetRemoteNetAddress(uint &uip, ushort &port, bool firstLocal)
{
  TNetAddress ip = m_ip;
  if(IsLocal())
  {
    TNetAddressVec addrv;
    GetLocalIPs(addrv);
    for(int i=0;i<addrv.size();i++)
      if(addrv[i].GetPtr<SIPv4Addr>())
      {
        ip = addrv[i];
				if(firstLocal)
					break;
      }
  }

  sockaddr_in addr;
  if(const SIPv4Addr* adr = ip.GetPtr<SIPv4Addr>())
  {
    ConvertAddr(TNetAddress(*adr),&addr);
    uip = S_ADDR_IP4(addr);
    port = addr.sin_port;
    return true;
  }
  else
    return false;
}

void CNetChannel::SetPerformanceMetrics( SPerformanceMetrics * pMetrics )
{
	SCOPED_GLOBAL_LOCK;
	m_ctpEndpoint.SetPerformanceMetrics( pMetrics );
}

const char * CNetChannel::GetName()
{
  TAddressString temp = RESOLVER.ToString( m_ip );

  static char buffer[256];
  if (temp.length() > 255)
	  temp = temp.substr(0,255);

  memcpy(buffer, temp.c_str(), temp.length()+1);

  return buffer;
}

const char* CNetChannel::GetNickname()
{
  return m_nickname.empty()?NULL:m_nickname.c_str();
}

EContextViewState CNetChannel::GetContextViewState() const
{
	return m_pContextView->GetLocalState();
}

void CNetChannel::Disconnect( EDisconnectionCause cause, const char * description )
{
	SCOPED_GLOBAL_LOCK;

	if (m_pNub)
		m_pNub->DisconnectChannel( cause, NULL, this, description );
}

void CNetChannel::SendMsg( INetMessage * pMsg )
{
	if (pMsg->CheckParallelFlag(eMPF_NoSendDelay))
	{
		SCOPED_GLOBAL_LOCK;
		NC_SendMsg(pMsg);
	}
	else
	{
		FROM_GAME(&CNetChannel::NC_SendMsg, this, pMsg);
	}
}

void CNetChannel::NC_SendMsg( INetMessage * pMsg )
{
	NetAddSendable( pMsg, 0, NULL, NULL );
}

bool CNetChannel::AddSendable( INetSendablePtr pSendable, int numAfterHandle, const SSendableHandle * afterHandle, SSendableHandle * handle )
{
	SCOPED_GLOBAL_LOCK;
	return NetAddSendable( pSendable, numAfterHandle, afterHandle, handle );
}

bool CNetChannel::SubstituteSendable( INetSendablePtr pSendable, int numAfterHandle, const SSendableHandle * afterHandle, SSendableHandle * handle )
{
	SCOPED_GLOBAL_LOCK;
	return NetSubstituteSendable( pSendable, numAfterHandle, afterHandle, handle );
}

bool CNetChannel::RemoveSendable( SSendableHandle handle )
{
	SCOPED_GLOBAL_LOCK;
	return NetRemoveSendable( handle );
}

void CNetChannel::DispatchRMI( IRMIMessageBodyPtr pBody )
{
	if (m_nStrongCaptureRMIs)
	{
		ASSERT_GLOBAL_LOCK;
		NC_DispatchRMI(pBody);
	}
	else if (pBody->pMessageDef && pBody->pMessageDef->CheckParallelFlag(eMPF_NoSendDelay))
	{
		SCOPED_GLOBAL_LOCK;
		if (!DoDispatchRMI(pBody))
		{
			FROM_GAME( &CNetChannel::NC_DispatchRMI, this, pBody );
		}
		else
			ForcePacketSend();
	}
	else
	{
		FROM_GAME( &CNetChannel::NC_DispatchRMI, this, pBody );
	}
}

void CNetChannel::NC_DispatchRMI( IRMIMessageBodyPtr pBody )
{
	if (!DoDispatchRMI(pBody) && CVARS.LogLevel>1)
		NetWarning( "RMI message discarded (%s)", pBody->pMessageDef? pBody->pMessageDef->description : "<<unknown>>" );
}

bool CNetChannel::DoDispatchRMI( IRMIMessageBodyPtr pBody )
{
	NET_ASSERT( pBody );

	if (!m_pContextView)
	{
		NetWarning( "RMI message with no context; ignored" );
		return false;
	}

	return m_pContextView->ScheduleAttachment( pBody );
}

void CNetChannel::DeclareWitness( EntityId id )
{
	FROM_GAME(&CNetChannel::NC_DeclareWitness, this, id);
}

void CNetChannel::NC_DeclareWitness( EntityId id )
{
	if (!m_pContextView)
	{
		NetWarning( "DeclareWitness %.8x without a context-view... ignored (hint: SetServer or SetClient", id );
		return;
	}

	m_pContextView->DeclareWitness( id );
}

const INetChannel::SStatistics& CNetChannel::GetStatistics()
{
	SCOPED_GLOBAL_LOCK;
	return m_statistics;
}

void CNetChannel::SetPassword( const char * password )
{
	SCOPED_GLOBAL_LOCK;
	if (m_pContextView)
		m_pContextView->SetPassword( password );
}

void CNetChannel::FragmentedPacket()
{
	CTimeValue now = g_time;
	return m_ctpEndpoint.FragmentedPacket( now );
}

CTimeValue CNetChannel::GetRemoteTime() const
{
	SCOPED_GLOBAL_LOCK;
	return m_ctpEndpoint.GetRemoteTime();
}

float CNetChannel::GetPing( bool smoothed ) const
{
	SCOPED_GLOBAL_LOCK;
	return m_ctpEndpoint.GetPing( smoothed );
}

bool CNetChannel::IsSufferingHighLatency(CTimeValue nTime) const
{
	SCOPED_GLOBAL_LOCK;
	return m_ctpEndpoint.IsSufferingHighLatency(nTime);
}

void CNetChannel::DefineProtocol( IProtocolBuilder * pBuilder )
{
	// we send and receive the same protocol here!
	pBuilder->AddMessageSink( this, GetProtocolDef(), GetProtocolDef() );

#if USE_DEFENCE
	if (m_pDefenceContext)
		m_pDefenceContext->DefineProtocol( pBuilder );
#endif //NOT_USE_DEFENCE

	if (m_pGameChannel)
		m_pGameChannel->DefineProtocol( pBuilder );
	if (m_pContextView)
		m_pContextView->DefineProtocol( pBuilder );
}

static CFixedSizeArena<sizeof(void*)*8 + sizeof(CTimeValue)*2, 64> m_pingpongarena;

class CNetChannel::CPingMsg : public INetSendable
{
public:
	CPingMsg() : INetSendable(eMPF_DontAwake, eNRT_UnreliableUnordered)
	{
		m_pChannel = 0;
		SetGroup('ping');
	}

	void Reset( CNetChannel * pChannel ) 
	{
		m_pChannel = pChannel;
	}

	virtual size_t GetSize() { return sizeof(*this); }
	virtual EMessageSendResult Send( INetSender * pSender ) 
	{ 
		m_pChannel->m_pingLock = false;
		m_pChannel->m_lastPingSent = g_time;
		m_pChannel->m_pings.push(g_time);
		pSender->BeginMessage( CNetChannel::Ping );
		pSender->ser.Value("when", g_time, 'ping');
		return eMSR_SentOk; 
	}
	virtual void UpdateState( uint32 nFromSeq, ENetSendableStateUpdate update ) {}
	virtual const char * GetDescription() { return "Ping"; }
	virtual void GetPositionInfo( SMessagePositionInfo& pos ) {}

private:
	CNetChannel * m_pChannel;

	virtual void DeleteThis()
	{
		m_pingpongarena.Dispose(this);
	}
};

class CNetChannel::CPongMsg : public INetSendable
{
public:
	CPongMsg() : INetSendable(0, eNRT_UnreliableUnordered) 
	{
		SetGroup('pong');
	}

	void Reset( CNetChannel * pChannel, CTimeValue when, CTimeValue recvd )
	{
		m_pChannel = pChannel;
		m_when = when;
		m_recvd = recvd;
	}

	virtual size_t GetSize() { return sizeof(*this); }
	virtual EMessageSendResult Send( INetSender * pSender )
	{ 
		pSender->BeginMessage( CNetChannel::Pong );
		pSender->ser.Value("when", m_when, 'pong');
		CTimeValue elapsed = g_time - m_recvd;
		pSender->ser.Value("elapsed", elapsed, 'pelp');
		CTimeValue gamenow = CNetwork::Get()->GetGameTime();
		pSender->ser.Value("remote", gamenow, 'trem');
		return eMSR_SentOk; 
	}
	virtual void UpdateState( uint32 nFromSeq, ENetSendableStateUpdate update ) {}
	virtual const char * GetDescription() { return "Pong"; }
	virtual void GetPositionInfo( SMessagePositionInfo& pos ) {}

private:
	CNetChannel * m_pChannel;
	CTimeValue m_when;
	CTimeValue m_recvd;

	virtual void DeleteThis()
	{
		m_pingpongarena.Dispose(this);
	}
};

void CNetChannel::TimerCallback( NetTimerId id, void* pUser, CTimeValue time )
{
	CNetChannel * pThis = static_cast<CNetChannel *>(pUser);
	if (pThis->m_timer == id)
	{
		pThis->Update(false, time);
		pThis->m_timer = 0; // expired this one...
		pThis->UpdateTimer(time);
	}
	else if (pThis->m_statsTimer == id)
	{
		pThis->UpdateStats(time);
		if (!pThis->m_bDead)
			pThis->m_statsTimer = TIMER.AddTimer( time + STATS_UPDATE_INTERVAL, TimerCallback, pThis );
	}
	else
	{
		//NET_ASSERT(false);
		//NetWarning("TimerCallBack(m_timer=%d, timer_id=%d)", pThis->m_timer, id);
	}

	// opportunistically try and send a ping (piggybacking on whatever update this is)
	if (!pThis->m_pingLock && (g_time - pThis->m_lastPingSent).GetSeconds() > 0.25f)
	{
		if (CPingMsg * pMsg = m_pingpongarena.Construct<CPingMsg>())
		{
			pMsg->Reset(pThis);
			pThis->NetAddSendable( pMsg, 0, 0, 0 );
			pThis->m_pingLock = true;
		}
	}
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CNetChannel, Ping, eNRT_UnreliableUnordered, 0)
{
	CTimeValue when;
	ser.Value("when", when, 'ping');
	if (CPongMsg * pMsg = m_pingpongarena.Construct<CPongMsg>())
	{
		pMsg->Reset(this, when, g_time);
		NetSubstituteSendable( pMsg, 0, 0, &m_pongHdl );
	}
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CNetChannel, Pong, eNRT_UnreliableUnordered, 0)
{
	CTimeValue when, elapsed, remote;
	ser.Value("when", when, 'pong');
	ser.Value("elapsed", elapsed, 'pelp');
	ser.Value("remote", remote, 'trem');
	bool found = false;
	while (!found && !m_pings.empty())
	{
		float millis = (m_pings.top() - when).GetMilliSeconds();
		if (millis > 1.0f)
			break;
		else if (millis > -1.0f)
		{
			when = m_pings.top();
			found = true;
		}
		m_pings.pop();
	}
	if (found)
	{
		CTimeValue ping = g_time - when - elapsed;
		if (ping > 0.0f)
			m_ctpEndpoint.AddPingSample(g_time, ping, remote);
	}
	return true;
}

CTimeValue CNetChannel::GetInactivityTimeout( bool backingOff )
{
	CTimeValue inactivityTimeout = 30.0f;
	if (CNetCVars::Get().InactivityTimeout > 0.001f)
		inactivityTimeout = CNetCVars::Get().InactivityTimeout;
	inactivityTimeout += 30.0f * !SupportsBackoff();
	inactivityTimeout += 60.0f * CNetwork::Get()->GetSocketIOManager().HasPendingData();
	inactivityTimeout += 60.0f * backingOff;
	return inactivityTimeout;
}

void CNetChannel::Update( bool finalUpdate, CTimeValue nTime )
{
	MMM_REGION(m_pMMM);

	CTimeValue backoffTime;
	bool backingOff = m_ctpEndpoint.GetBackoffTime(backoffTime, true);
	if (CNetCVars::Get().BackoffTimeout && backingOff && backoffTime.GetSeconds() > CNetCVars::Get().BackoffTimeout)
	{
		float idleTime = backoffTime.GetSeconds();
		char buf[256];
		sprintf(buf, "Zombie kicked; remote machine was backing off for %.1f seconds", idleTime);
		Disconnect(eDC_Timeout, buf);
	}

	if (m_nKeepAliveTimer != CTimeValue(0.0f) && !IsLocal())
	{
		CTimeValue idleTime = nTime - m_nKeepAliveTimer;
		if (idleTime > GetInactivityTimeout(backingOff))
		{
			char buf[256];
			sprintf(buf, "Timeout occurred; no packet for %.1f seconds", idleTime.GetSeconds());
			Disconnect(eDC_Timeout, buf);
		}
	}

	if (m_nKeepAliveTimer != CTimeValue(0.0f) && nTime > m_nKeepAlivePingTimer + KEEPALIVE_TIMEOUT && !IsLocal())
	{
		SendSimplePacket(eH_KeepAlive);
		m_nKeepAlivePingTimer = nTime;
	}

	if (!m_bDead)
	{
		bool bAllowUserMessages = true;

		if (m_pContextView)
			bAllowUserMessages &= m_pContextView->IsInState(eCVS_InGame);
		bAllowUserMessages &= !finalUpdate;

		m_ctpEndpoint.Update( nTime, m_bDead, bAllowUserMessages, m_bForcePacketSend, finalUpdate );
		m_bForcePacketSend = false;
	}
}

void CNetChannel::UpdateTimer( CTimeValue time )
{
	TIMER.CancelTimer(m_timer);
	if (!m_bDead)
	{
		CTimeValue when = m_ctpEndpoint.GetNextUpdateTime(time);
		m_timer = TIMER.AddTimer( when, TimerCallback, this );
	}
}

void CNetChannel::Init( CNetNub * pNub, IGameChannel * pGameChannel )
{
	NET_ASSERT( !m_pGameChannel );
	m_pNub = pNub;
	m_pGameChannel = pGameChannel;

	if (m_pContextView)
		m_pContextView->CompleteInitialization();

	m_ctpEndpoint.Init( this );
	m_ctpEndpoint.MarkNotUserSink( this );
	m_ctpEndpoint.MarkNotUserSink( m_pContextView );
	
#if USE_DEFENCE
	m_ctpEndpoint.MarkNotUserSink( m_pDefenceContext );
#endif //NOT_USE_DEFENCE


	UpdateTimer( g_time ); 
	m_statsTimer = TIMER.AddTimer( g_time, TimerCallback, this );
	CNetChannel::SendSetRemoteChannelIDWith( SSetRemoteChannelID(m_localChannelID), this );

	if (!IsLocal())
	{
		if (SupportsBackoff())
			m_pNub->RegisterBackoffAddress( m_ip );
		else
			NetComment("Backoff system not supported to %s; timeouts are extended", RESOLVER.ToString(m_ip).c_str());
	}
}

void CNetChannel::RemovedFromNub()
{ 
	ASSERT_GLOBAL_LOCK;
	if (!IsLocal() && SupportsBackoff())
		m_pNub->UnregisterBackoffAddress( m_ip );
	m_pNub = 0; 
}

void CNetChannel::SyncWithGame( ENetworkGameSync type )
{
	if (type != eNGS_FrameStart)
		return;

	MMM_REGION( m_pMMM );

	while (!m_queuedForSyncPackets.empty())
	{
		TMemHdl hdl = m_queuedForSyncPackets.front();

		DoProcessPacket( hdl, true );
		m_queuedForSyncPackets.pop();
	}

	static ICVar * pSchedDebug = CNetCVars::Get().pSchedulerDebug;
	if (pSchedDebug)
	{
		const char * v = pSchedDebug->GetString();
		if (v[0] == '0' && v[1] == '\0')
			;
		else if (CryStringUtils::stristr(GetName(), v))
			m_ctpEndpoint.SchedulerDebugDraw();
	}

	if (CNetCVars::Get().ChannelStats)
	{
		m_ctpEndpoint.ChannelStatsDraw();
	}

	//if (IsTimeReady())
	//	NetQuickLog("%s: %f", GetName(), GetRemoteTime().GetSeconds());
}

void CNetChannel::UpdateStats(CTimeValue time)
{
	m_statistics.bandwidthDown = m_ctpEndpoint.GetBandwidth(true);
	m_statistics.bandwidthUp = m_ctpEndpoint.GetBandwidth(false);
}

#if LOAD_NETWORK_CODE
class CNetChannel::CLoadMsg : public CSimpleNetMessage<CNetChannel::SLoadStructure>
{
public:
	CLoadMsg( CNetChannel * pChannel ) : CSimpleNetMessage<CNetChannel::SLoadStructure>(SLoadStructure(), CNetChannel::LoadNetworkMessage) 
	{
		m_pChannel = pChannel;
	}

	void SetHandle( SSendableHandle handle )
	{
		m_id = handle;
	}

	EMessageSendResult Send( INetSender * pSender )
	{
		m_k++;
		return CSimpleNetMessage<CNetChannel::SLoadStructure>::Send(pSender);
	}

	void UpdateState( uint32 nFromSeq, ENetSendableStateUpdate state )
	{
		if (state != eNSSU_Requeue)
		{
			m_pChannel->m_loadHandles.erase(m_id);
			if (state != eNSSU_Rejected)
				m_pChannel->AddLoadMsg();
		}
	}

private:
	_smart_ptr<CNetChannel> m_pChannel;
	SSendableHandle m_id;
	int m_k;
};

void CNetChannel::AddLoadMsg()
{
	_smart_ptr<CLoadMsg> pMsg = new CLoadMsg(this);
	std::vector<SSendableHandle> deps;
	for (std::set<SSendableHandle>::iterator it = m_loadHandles.begin(); it != m_loadHandles.end(); ++it)
	{
		if (rand() < RAND_MAX/64)
			deps.push_back(*it);
	}
//	if (deps.empty() && !m_loadHandles.empty())
//		deps.push_back(*m_loadHandles.begin());
	SSendableHandle hdl;
	NetAddSendable( &*pMsg, deps.size(), deps.empty()? 0 : &deps[0], &hdl );
	pMsg->SetHandle(hdl);
	m_loadHandles.insert(hdl);
}
#endif

void CNetChannel::ProcessPacket( bool forceWaitForSync, const uint8 * pData, uint32 nLength )
{
	MMM_REGION(m_pMMM);

	if (m_bDead)
		return;

#if LOAD_NETWORK_CODE
	if (!m_bConnectionEstablished)
	{
		for (int i=0; i<LOAD_NETWORK_COUNT; i++)
		{
			AddLoadMsg();
		}
	}
#endif

	if (!m_bConnectionEstablished)
		m_ctpEndpoint.ChangeSubscription(this, eCEE_BecomeAlerted | eCEE_BackoffTooLong);
	m_bConnectionEstablished = true;

#if FORCE_DECODING_TO_GAME_THREAD
	forceWaitForSync = true;
#endif

	TMemHdl hdl = MMM().AllocHdl(nLength);
	memcpy(MMM().PinHdl(hdl), pData, nLength);

	if (!forceWaitForSync && m_queuedForSyncPackets.empty())
		DoProcessPacket(hdl, false);
	else
		m_queuedForSyncPackets.push(hdl);
}

void CNetChannel::GotPacket( EGotPacketType type )
{
	ASSERT_GLOBAL_LOCK;
	switch (type)
	{
	case eGPT_Fake:
		if (m_gotFakePacket)
		{
	case eGPT_Normal:
			m_nKeepAliveTimer2 = g_time;
	case eGPT_KeepAlive:
			m_nKeepAlivePingTimer = m_nKeepAliveTimer = g_time;
			m_gotFakePacket = (type == eGPT_Fake);
		}
		break;
	}
}

void CNetChannel::DoProcessPacket( TMemHdl hdl, bool inSync )
{
	MMM_REGION(m_pMMM);

	if (m_bDead)
	{
		MMM().FreeHdl(hdl);
		return;
	}

	GotPacket(eGPT_Normal);
	m_ctpEndpoint.ProcessPacket( g_time, hdl, true, inSync );

	UpdateTimer( g_time );
}

void CNetChannel::PunkDetected( EPunkType punkType )
{
	if (!m_pNub)
		return;
	NET_ASSERT( m_pNub->GetSecurity() );
	m_pNub->GetSecurity()->OnPunkDetected( RESOLVER.ToString(m_ip), punkType );
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE( CNetChannel, SetRemoteChannelID, eNRT_ReliableUnordered, true )
{
	m_remoteChannelID = param.m_id;
	return m_remoteChannelID > 0;
}

#if LOAD_NETWORK_CODE
NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE( CNetChannel, LoadNetworkMessage, eNRT_ReliableUnordered, false )
{
	return true;
}
#endif

void CNetChannel::GC_OnDisconnect( EDisconnectionCause cause, string msg, CDontDieLock lock )
{
	m_pGameChannel->OnDisconnect( cause, msg.c_str() );
}

void CNetChannel::GC_DeleteGameChannel( CDontDieLock lock )
{
	if (m_pGameChannel)
		m_pGameChannel->Release();
	m_pGameChannel = 0;
}

void CNetChannel::Die()
{
	MMM_REGION(m_pMMM);

	if (m_bDead)
		return;

	DisconnectGame( eDC_Unknown, "Disconnected" );

	m_bDead = true;
	if (m_pContextView)
		m_pContextView->Die();

	TO_GAME( &CNetChannel::GC_DeleteGameChannel, this, CDontDieLock(this) );

	m_ctpEndpoint.EmptyMessages();
	m_bConnectionEstablished = false;

	TIMER.CancelTimer( m_timer );
	TIMER.CancelTimer( m_statsTimer );
	m_timer = 0;
	m_statsTimer = 0;
}

void CNetChannel::GetMemoryStatistics(ICrySizer *pSizer, bool countingThis)
{
	SIZER_COMPONENT_NAME(pSizer, "CNetChannel");
	MMM_REGION(m_pMMM);

	if (countingThis)
		if (!pSizer->Add(*this))
			return;
	for (size_t i = 0; i < m_queuedForSyncPackets.size(); ++i)
	{
		TMemHdl hdl = m_queuedForSyncPackets.front();
		MMM().AddHdlToSizer(hdl, pSizer);
		m_queuedForSyncPackets.pop();
		m_queuedForSyncPackets.push(hdl);
	}
	m_ctpEndpoint.GetMemoryStatistics(pSizer);
	m_pContextView->GetMemoryStatistics(pSizer);
}

#if USE_DEFENCE
void CNetChannel::AddProtectedFile( const CDefenceData::SProtectedFile& file )
{
	if (m_pDefenceContext)
		m_pDefenceContext->AddProtectedFile( file );
}

void CNetChannel::ClearProtectedFiles()
{
	if (m_pDefenceContext)
		m_pDefenceContext->ClearProtectedFiles();
}

void CNetChannel::DisconnectThroughPB(const char *reason, const char *log)
{
#ifdef __WITH_PB__
	if (isPbSvEnabled())
	{
		string event;
		event.Format("pb_sv_vio %d %s", m_fastLookupId+1, log);

		PbSvAddEvent(PB_EV_CMD, -1, event.length(), const_cast<char *>(event.c_str()));
	}
	else
#endif
		Disconnect(eDC_PunkDetected, reason);
}
#endif //NOT_USE_DEFENCE

bool CNetChannel::IsLocal() const
{
	SCOPED_GLOBAL_LOCK;
	if (m_ip.GetPtr<TLocalNetAddress>())
		return true;
	return m_pContextView? m_pContextView->IsLocal() : false;
}

bool CNetChannel::IsFakeChannel() const
{
	return IsLocal() && gEnv->pSystem->IsDedicated();
}

bool CNetChannel::IsConnectionEstablished() const
{
	SCOPED_GLOBAL_LOCK;
	return m_bConnectionEstablished;
}

bool CNetChannel::IsConnected() const
{
	return !m_bDead && !m_ctpEndpoint.InEmptyMode();
}

void CNetChannel::ForcePacketSend()
{
	SCOPED_GLOBAL_LOCK;
	m_bForcePacketSend = true;
}

CNetChannel * CNetChannel::GetNetChannel()
{
	return this;
}

bool CNetChannel::IsDead()
{
#if USE_DEFENCE
	if (m_pDefenceContext && !m_pDefenceContext->CanRemove())
		return false;
	else 
#endif
	{
		if (!m_pendingStuffBeforeDead)
			return m_bDead;
		else
			return false;
	}
}

bool CNetChannel::IsSuicidal()
{
	return m_bDead;
}

#if ENABLE_DEBUG_KIT
TNetChannelID CNetChannel::GetLoggingChannelID()
{
	TNetChannelID out = 0;
	if (m_pContextView)
		out = m_pContextView->GetLoggingChannelID(m_localChannelID, m_remoteChannelID);
	return out;
}
#endif

void CNetChannel::DisconnectGame( EDisconnectionCause cause, string msg )
{
	if (m_sentDisconnect)
		return;

	NetLogAlways("Disconnect %s; profid=%d; cause=%d; msg='%s'", GetRemoteAddressString().c_str(), GetProfileId(), cause, msg.c_str());

	TO_GAME(&CNetChannel::GC_OnDisconnect, this, cause, msg, CDontDieLock(this));
	m_sentDisconnect = true;
}

INetSendablePtr CNetChannel::NetFindSendable( SSendableHandle handle )
{
	return m_ctpEndpoint.FindSendable(handle);
}

bool CNetChannel::GetWitnessPosition( Vec3& pos )
{
	if (m_pContextView)
		return m_pContextView->GetWitnessPosition(pos);
	else
		return false;
}

bool CNetChannel::GetWitnessDirection( Vec3& pos )
{
	if (m_pContextView)
		return m_pContextView->GetWitnessDirection(pos);
	else
		return false;
}

bool CNetChannel::GetWitnessFov( float& fov )
{
	if (m_pContextView)
		return m_pContextView->GetWitnessFov(fov);
	else
		return false;
}

EMessageSendResult CNetChannel::WriteHeader( INetSender * pSender )
{
	if (m_pContextView)
		return m_pContextView->WriteHeader(pSender);
	else
		return eMSR_SentOk;
}

EMessageSendResult CNetChannel::WriteFooter( INetSender * pSender )
{
	if (m_pContextView)
		return m_pContextView->WriteFooter(pSender);
	else
		return eMSR_SentOk;
}

bool CNetChannel::IsServer()
{
	if (!m_pContextView)
		return false;
	else
		return m_pContextView->IsServer();
}

EChannelConnectionState CNetChannel::GetChannelConnectionState() const
{
	if (m_bDead || m_sentDisconnect)
		return eCCS_Disconnecting;
	if (!m_bConnectionEstablished)
		return eCCS_StartingConnection;
	if (m_pContextView && m_pContextView->IsPastOrInState(eCVS_InGame))
		return eCCS_InGame;
	if (m_pContextView)
		return eCCS_InContextInitiation;
	else
		return eCCS_InGame;
}

int CNetChannel::GetContextViewStateDebugCode() const
{
	if (m_pContextView)
		return m_pContextView->GetStateDebugCode();
	else
		return 0;
}

#if ENABLE_DEBUG_KIT
#include "IGameFramework.h"
void CNetChannel::GC_AddPingReadout(float elapsedRemote, float sinceSent, float ping)
{
	if (!CVARS.ShowPing)
		return;

	if (!m_pDebugHistory)
	{
		m_pDebugHistory = gEnv->pGame->GetIGameFramework()->CreateDebugHistoryManager();
	}

	if(!m_pElapsedRemote)
	{
		m_pElapsedRemote = m_pDebugHistory->CreateHistory("ElapsedRemote");
		m_pElapsedRemote->SetupLayoutAbs(50, 110, 200, 120, 5);
		m_pElapsedRemote->SetupScopeExtent(-360, +360, -.01f, +.01f);
	}

	if(!m_pSinceSent)
	{
		m_pSinceSent = m_pDebugHistory->CreateHistory("TimeSinceSent");
		m_pSinceSent->SetupLayoutAbs(50, 240, 200, 120, 5);
		m_pSinceSent->SetupScopeExtent(-360, +360, -.01f, +.01f);
	}

	if(!m_pPing)
	{
		m_pPing = m_pDebugHistory->CreateHistory("FrameTime");
		m_pPing->SetupLayoutAbs(50, 370, 200, 120, 5);
		m_pPing->SetupScopeExtent(-360, +360, -.01f, +.01f);
	}

	m_pElapsedRemote->AddValue(elapsedRemote);
	m_pSinceSent->AddValue(sinceSent);
	m_pPing->AddValue(ping);

	m_pElapsedRemote->SetVisibility(true);
	m_pSinceSent->SetVisibility(true);
	m_pPing->SetVisibility(true);
}
#endif

void CNetChannel::OnEndpointEvent( const SCTPEndpointEvent& evt )
{
	switch (evt.event)
	{
	case eCEE_BecomeAlerted:
		OnChangedIdle();
		break;
	case eCEE_BackoffTooLong:
		if (m_nKeepAliveTimer != CTimeValue(0.0f) && (g_time - m_nKeepAliveTimer2).GetSeconds() < 20.0f)
			GotPacket( eGPT_Fake );
		break;
	}
}

void CNetChannel::OnChangedIdle()
{
	UpdateTimer(g_time);
}

CTimeValue CNetChannel::TimeSinceVoiceTransmission()
{
	SCOPED_GLOBAL_LOCK;
	return g_time - m_lastVoiceTransmission;
}

CTimeValue CNetChannel::TimeSinceVoiceReceipt( EntityId id )
{
	SCOPED_GLOBAL_LOCK;
	if (!m_pContextView)
		return 1000000.0f;
	else
		return m_pContextView->TimeSinceVoiceReceipt(id);
}

void CNetChannel::AllowVoiceTransmission( bool allow )
{
	SCOPED_GLOBAL_LOCK;
	if (!m_pContextView)
		return;
	m_pContextView->AllowVoiceTransmission(allow);
}

bool CNetChannel::IsPreordered() const
{
#ifdef CRYSIS_BETA
	return true;
#else
  return m_preordered;
#endif
}

int CNetChannel::GetProfileId() const
{
	return m_profileId;
}

string CNetChannel::GetCDKeyHash() const
{
  return m_CDKeyHash;
}

void CNetChannel::SetProfileId(int id)
{
	m_profileId=id;;
}

void CNetChannel::SetNickname(const char* name)
{
  m_nickname = name;
}

void CNetChannel::SetPreordered(bool p)
{
  m_preordered = p;
}

void CNetChannel::SetCDKeyHash(const char* hash)
{
  m_CDKeyHash = hash;
}

bool CNetChannel::IsIdle()
{
	if (!m_ip.GetPtr<TLocalNetAddress>())
		return false;
	return IsTimeReady() && (m_pContextView? m_pContextView->IsIdle() : true);
}

void CNetChannel::NetDump(ENetDumpType type)
{
	if (m_pContextView)
		m_pContextView->NetDump(type);
}

bool CNetChannel::IsInTransition()
{
	SCOPED_GLOBAL_LOCK;
	if (!m_pContextView || !m_pContextView->Context() || !m_pContextView->ContextState())
		return false;
	return m_pContextView->Context()->GetCurrentState() != m_pContextView->ContextState();
}

void CNetChannel::SendSimplePacket( EHeaders header )
{
	Send( &Frame_IDToHeader[header], 1 );
}

void CNetChannel::ProcessSimplePacket( EHeaders hdr )
{
	ASSERT_GLOBAL_LOCK;
	GotPacket( eGPT_KeepAlive );
	switch (hdr)
	{
	case eH_BackOff:
		NetLog( "%s requests backoff due to congestion", GetName() );
		m_ctpEndpoint.BackOff();
		UpdateTimer(g_time);
		break;
	case eH_KeepAlive:
		if (g_time - KEEPALIVE_REPLY_TIMEOUT > m_nKeepAliveReplyTimer)
		{
			SendSimplePacket(eH_KeepAliveReply);
			m_nKeepAliveReplyTimer = g_time;
		}
		break;
	case eH_KeepAliveReply:
		// do nothing
		break;
	}
}

TUpdateMessageBrackets CNetChannel::GetUpdateMessageBrackets()
{
	return m_pContextView->GetUpdateMessageBrackets();	
}
