/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  implements a communications channel between two computers
 -------------------------------------------------------------------------
 History:
 - 26/07/2004   : Created by Craig Tiller
*************************************************************************/
#ifndef __NETCHANNEL_H__
#define __NETCHANNEL_H__

#pragma once

#include "NetHelpers.h"
#include "CTPEndpoint.h"
#include "NetNub.h"
#include "INubMember.h"
#include "Config.h"
#include "IDebugHistory.h"
#include <queue>

struct IDefenceContext;
class CNetContext;
class CDisconnectMessage;
class CContextView;

typedef std::pair<const SNetMessageDef *, const SNetMessageDef *> TUpdateMessageBrackets;

struct SSetRemoteChannelID
{
	SSetRemoteChannelID() : m_id(0) {}
	SSetRemoteChannelID(TNetChannelID id) : m_id(id) {}
	TNetChannelID m_id;
	void SerializeWith( TSerialize ser )
	{
		ser.Value("id", m_id);
	}
};

class CNetChannel : public CNetMessageSinkHelper<CNetChannel, INetChannel>, public INubMember, public ICTPEndpointListener
{
	CMementoMemoryManagerPtr m_pMMM;

public:
	CNetChannel( const TNetAddress& ipRemote, const CExponentialKeyExchange::KEY_TYPE& key, uint32 remoteSocketCaps );
	~CNetChannel();

	// INetChannel
	virtual void SetPerformanceMetrics( SPerformanceMetrics * pMetrics );
	virtual void SetServer( INetContext * pServerContext, bool cheatProtection );
	virtual void SetClient( INetContext * pServerContext, bool cheatProtection );
	virtual void Disconnect( EDisconnectionCause cause, const char * description );
	virtual void SendMsg( INetMessage * );
	virtual bool AddSendable( INetSendablePtr pSendable, int numAfterHandle, const SSendableHandle * afterHandle, SSendableHandle * handle );
	virtual bool SubstituteSendable( INetSendablePtr pSendable, int numAfterHandle, const SSendableHandle * afterHandle, SSendableHandle * handle );
	virtual bool RemoveSendable( SSendableHandle handle );
	virtual const SStatistics& GetStatistics();
	virtual void SetPassword( const char * password );
	virtual IGameChannel * GetGameChannel() 
	{ 
		SCOPED_GLOBAL_LOCK;
		return m_pGameChannel; 
	}
	virtual CTimeValue GetRemoteTime() const;
	virtual float GetPing( bool smoothed ) const;
	virtual bool IsSufferingHighLatency(CTimeValue nTime) const;
	virtual void DispatchRMI( IRMIMessageBodyPtr pBody );
	virtual void DeclareWitness( EntityId id );
	virtual bool IsLocal() const;
	virtual bool IsFakeChannel() const;
	virtual bool IsConnectionEstablished() const;
	virtual string GetRemoteAddressString() const;
	virtual bool GetRemoteNetAddress(uint &uip, ushort &port, bool firstLocal = true);
	virtual const char* GetName();
  virtual const char* GetNickname();
	virtual void SetNickname(const char* name);
	virtual TNetChannelID GetLocalChannelID() { return m_localChannelID; }
	virtual TNetChannelID GetRemoteChannelID() { return m_remoteChannelID; }

	virtual EChannelConnectionState GetChannelConnectionState() const;
	virtual EContextViewState GetContextViewState() const;
	virtual int GetContextViewStateDebugCode() const;
	virtual bool IsTimeReady() const { return m_ctpEndpoint.IsTimeReady(); }

	virtual bool IsInTransition();

	virtual CTimeValue TimeSinceVoiceTransmission();
	virtual CTimeValue TimeSinceVoiceReceipt( EntityId id );
	virtual void AllowVoiceTransmission( bool allow );

  virtual bool IsPreordered() const;
	virtual int GetProfileId() const;
	// ~INetChannel

  string  GetCDKeyHash() const;

	ILINE bool NetAddSendable( INetSendable * pMsg, int numAfterHandle, const SSendableHandle * afterHandle, SSendableHandle * handle )
	{
		ASSERT_GLOBAL_LOCK;

		if (m_bDead)
		{
			FailAddSendableDead(pMsg);
			return false;
		}

		return m_ctpEndpoint.AddSendable( pMsg, numAfterHandle, afterHandle, handle );
	}
	ILINE bool NetSubstituteSendable( INetSendable * pMsg, int numAfterHandle, const SSendableHandle * afterHandle, SSendableHandle * handle )
	{
		ASSERT_GLOBAL_LOCK;

		if (m_bDead)
		{
			FailAddSendableDead(pMsg);
			return false;
		}

		return m_ctpEndpoint.SubstituteSendable( pMsg, numAfterHandle, afterHandle, handle );
	}
	ILINE bool NetRemoveSendable( const SSendableHandle& handle )
	{
		ASSERT_GLOBAL_LOCK;
		return m_ctpEndpoint.RemoveSendable(handle);
	}

	INetSendablePtr NetFindSendable( SSendableHandle handle );
	void ChangeSubscription( ICTPEndpointListener * pListener, uint32 eventMask ) { m_ctpEndpoint.ChangeSubscription(pListener, eventMask); }

	// INetMessageSink
	virtual void DefineProtocol( IProtocolBuilder * pBuilder );
	// ~INetMessageSink

	// INubMember
	virtual CNetChannel * GetNetChannel();
	virtual bool IsDead();
	virtual void ProcessPacket( bool forceWaitForSync, const uint8 * pData, uint32 nLength );
	virtual void ProcessSimplePacket( EHeaders hdr );
	virtual void PerformRegularCleanup();
	virtual void SyncWithGame( ENetworkGameSync type );
	virtual void Die();
	virtual void RemovedFromNub();
  virtual bool IsSuicidal();
	virtual void NetDump(ENetDumpType type);
	// ~INubMember

	// ICTPEndpointListener
	virtual void OnEndpointEvent( const SCTPEndpointEvent& evt );
	// ~ICTPEndpointListener

	void ForcePacketSend();
	void FragmentedPacket();
	void Init( CNetNub * pNub, IGameChannel * pGameChannel );
	void Send( const uint8 * pData, size_t nLength )
	{
		ASSERT_GLOBAL_LOCK;
		if (m_pNub)
		{
			if (!m_pNub->SendTo( pData, nLength, m_ip ))
				Disconnect( eDC_ICMPError, "Send failed" );
		}
		else
			int i=0;
	}
	// send a packet out of band (not via the channel) directly to some other network address
	// like CNetNub::SendTo
	void SendWithNubTo( const uint8 * pData, size_t nLength, const TNetAddress& to )
	{
		if (m_pNub)
		{
			if (!m_pNub->SendTo( pData, nLength, to ))
				Disconnect( eDC_ICMPError, "Send failed");
		}
		return;
	}
	void GetMemoryStatistics(ICrySizer *pSizer, bool countingThis = false);
	void SetEntityId( EntityId id ) { m_ctpEndpoint.SetEntityId(id); }
	uint32 GetMostRecentAckedSeq() const { return m_ctpEndpoint.GetMostRecentAckedSeq(); }
	uint32 GetMostRecentSentSeq() const { return m_ctpEndpoint.GetMostRecentSentSeq(); }
	bool IsServer();
	void SetAfterSpawning( bool afterSpawning ) { m_ctpEndpoint.SetAfterSpawning(afterSpawning); }
	CTimeValue GetIdleTime( bool realtime ) { return g_time - (realtime? m_nKeepAliveTimer2 : m_nKeepAliveTimer); }
	CTimeValue GetInactivityTimeout( bool backingOff );

	CMementoMemoryManagerPtr GetChannelMMM() { return m_pMMM; }

	void UnblockMessages() { m_ctpEndpoint.UnblockMessages(); }
	bool OnMessageQueueEmpty();

	bool LookupMessage( const char * name, SNetMessageDef const **ppDef, INetMessageSink **ppSink )
	{
		return m_ctpEndpoint.LookupMessage(name, ppDef, ppSink);
	}
	bool IsConnected() const; // we are connected until we are disconnected

	void DisconnectGame( EDisconnectionCause dc, string msg );
	bool GetWitnessPosition( Vec3& pos );
	bool GetWitnessDirection( Vec3& pos );
	bool GetWitnessFov( float& fov );
	EMessageSendResult WriteHeader( INetSender * pSender );
	EMessageSendResult WriteFooter( INetSender * pSender );

	bool IsIdle();
	void OnChangedIdle();

	TUpdateMessageBrackets GetUpdateMessageBrackets();

#if LOAD_NETWORK_CODE
	struct SLoadStructure
	{
		unsigned char buf[LOAD_NETWORK_PKTSIZE];
		void SerializeWith( TSerialize ser )
		{
			for (int i=0; i<LOAD_NETWORK_PKTSIZE; i++)
			{
				ser.Value( "b", buf[i] );
			}
		}
	};
	std::set<SSendableHandle> m_loadHandles;
	class CLoadMsg;
	void AddLoadMsg();
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(LoadNetworkMessage, SLoadStructure);
#endif

	NET_DECLARE_IMMEDIATE_MESSAGE(Ping);
	NET_DECLARE_IMMEDIATE_MESSAGE(Pong);

#if USE_DEFENCE
	void AddProtectedFile( const CDefenceData::SProtectedFile& file );
	void ClearProtectedFiles();
	CDefenceData * GetDefenceData() { return m_pNub? m_pNub->GetDefenceData() : 0; }
	void DisconnectThroughPB(const char *reason, const char *log);
#endif //NOT_USE_DEFENCE

	CNetwork * GetNetwork() { return (CNetwork*)gEnv->pNetwork; }

	void PunkDetected( EPunkType punkType );
	const TNetAddress& GetIP() { return m_ip; }
	void DoProcessPacket( TMemHdl hdl, bool inSync );

	void GetLocalIPs( TNetAddressVec& vIPs )
	{
		if (m_pNub)
			m_pNub->GetLocalIPs( vIPs );
	}

	CContextView * GetContextView()
	{
		if ( m_pContextView == NULL )
			return 0;
		else
			return m_pContextView;
	}

	//string  GetRemoteAddressString()const;
  //bool GetRemoteNetAddress(uint &ip, ushort &port);

	void BeginStrongCaptureRMIs()
	{
		m_nStrongCaptureRMIs ++;
	}
	void EndStrongCaptureRMIs()
	{
		m_nStrongCaptureRMIs --;
	}

	void TransmittedVoice()
	{
		m_lastVoiceTransmission = g_time;
	}

  void SetPreordered(bool p);
	void SetProfileId(int id);
  void SetCDKeyHash(const char* hash);

	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(SetRemoteChannelID, SSetRemoteChannelID);

#if ENABLE_DEBUG_KIT
	TNetChannelID GetLoggingChannelID();
	void GC_AddPingReadout( float elapsedRemote, float sinceSent, float ping );
#endif

	void GC_SendableSink( INetSendablePtr ) {}

	// queued functions from game
	void NC_DispatchRMI( IRMIMessageBodyPtr pBody );
	bool DoDispatchRMI( IRMIMessageBodyPtr pBody );
	void NC_SendMsg( INetMessage * pMsg );
	void NC_DeclareWitness( EntityId id );

	const CExponentialKeyExchange::KEY_TYPE& GetPrivateKey() { return m_privateKey; }

private:
	friend class CDisconnectMessage;

	CExponentialKeyExchange::KEY_TYPE m_privateKey;

	static void TimerCallback( NetTimerId, void*, CTimeValue );
	void Update( bool finalUpdate, CTimeValue time );
	void UpdateTimer( CTimeValue time );
	void UpdateStats( CTimeValue time );

	bool SupportsBackoff()
	{
		return (m_remoteSocketCaps & eSIOMC_SupportsBackoff)!=0 && (CNetwork::Get()->GetSocketIOManager().caps & eSIOMC_SupportsBackoff)!=0;
	}

	std::queue<TMemHdl>        m_queuedForSyncPackets;

	int m_nStrongCaptureRMIs;
	
	IGameChannel *             m_pGameChannel;
	_smart_ptr<CContextView>   m_pContextView;
#if USE_DEFENCE
	IDefenceContext *          m_pDefenceContext;
#endif
	CNetNub *                  m_pNub;
	// is the connection established (has the other end sent us a packet)
	bool                       m_bConnectionEstablished;
	// should we force a packet output this frame?
	bool                       m_bForcePacketSend;

	TNetAddress                m_ip;
	CCTPEndpoint               m_ctpEndpoint;

	bool                       m_bDead;
	CTimeValue                 m_nKeepAliveTimer;
	CTimeValue                 m_nKeepAliveTimer2;
	CTimeValue                 m_nKeepAlivePingTimer;
	CTimeValue								 m_nKeepAliveReplyTimer;
	SStatistics                m_statistics;
	string                     m_password;
	NetTimerId                 m_timer;
	NetTimerId								 m_statsTimer;
	uint32                     m_remoteSocketCaps;

	bool											 m_pingLock;
	CTimeValue                 m_lastPingSent;
	SSendableHandle            m_pongHdl;
	std::priority_queue< CTimeValue, std::vector<CTimeValue>, std::greater<CTimeValue> > m_pings;

	const TNetChannelID				 m_localChannelID;
	TNetChannelID              m_remoteChannelID;
	static TNetChannelID       m_nextLocalChannelID;

public:
	int m_fastLookupId;
private:
  CTimeValue  m_lastVoiceTransmission;
  bool        m_preordered;
	int		      m_profileId;
  string      m_CDKeyHash;
  string      m_nickname;

	int m_pendingStuffBeforeDead;
	bool m_sentDisconnect;
	bool m_gotFakePacket;

	class CDontDieLock;
	void GC_DeleteGameChannel( CDontDieLock );
	void GC_OnDisconnect( EDisconnectionCause cause, string msg, CDontDieLock );

	void SendSimplePacket( EHeaders header );
	enum EGotPacketType
	{
		eGPT_Fake,
		eGPT_KeepAlive,
		eGPT_Normal
	};
	void GotPacket( EGotPacketType );

	class CPingMsg;
	class CPongMsg;

#if ENABLE_DEBUG_KIT
	IDebugHistoryManager * m_pDebugHistory;
	IDebugHistory * m_pElapsedRemote;
	IDebugHistory * m_pSinceSent;
	IDebugHistory * m_pPing;
#endif

	void LockAlive()
	{
		++m_pendingStuffBeforeDead;
	}
	void UnlockAlive()
	{
		--m_pendingStuffBeforeDead;
	}

	class CDontDieLock
	{
	public:
		CDontDieLock() : m_pChannel(0) {}
		CDontDieLock( CNetChannel * pChannel ) : m_pChannel(pChannel)
		{
			m_pChannel->LockAlive();
		}
		CDontDieLock( const CDontDieLock& lk ) : m_pChannel(lk.m_pChannel)
		{
			if (m_pChannel)
				m_pChannel->LockAlive();
		}
		~CDontDieLock()
		{
			if (m_pChannel)
				m_pChannel->UnlockAlive();
		}
		void Swap( CDontDieLock& lk )
		{
			std::swap(m_pChannel, lk.m_pChannel);
		}
		CDontDieLock& operator=( CDontDieLock lk )
		{
			Swap(lk);
			return *this;
		}

	private:
		CNetChannel * m_pChannel;
	};

	void FailAddSendableDead( INetSendable * pMsg )
	{
		NetWarning( "Attempting to send message %s after disconnection", pMsg->GetDescription() );
		pMsg->UpdateState(0,eNSSU_Rejected);
	}
};

struct StrongCaptureRMIs
{
	StrongCaptureRMIs(CNetChannel * pNC) : m_pNC(pNC) { m_pNC->BeginStrongCaptureRMIs(); }
	~StrongCaptureRMIs() { m_pNC->EndStrongCaptureRMIs(); }
	_smart_ptr<CNetChannel> m_pNC;
};

#endif
