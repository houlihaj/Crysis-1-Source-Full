//////////////////////////////////////////////////////////////////////
//
//	Crytek Network source code
//	
//	File: CTPEndpoint.cpp
//  Description: 
//
//	History:
//	-July 25,2001:Created by Alberto Demichelis
//
//////////////////////////////////////////////////////////////////////


#ifndef __CTPENDPOINT_H__
#define __CTPENDPOINT_H__

#pragma once

#include <queue>
#include <set>

#include "Streams/ArithStream.h"
#include "PacketRateCalculator.h"
#include "MessageMapper.h"
#include "Serialize.h"
#include "FrameTypes.h"
#include "DebugKit/NetworkInspector.h"
#include "MessageQueue.h"
#include "ICTPEndpointListener.h"

#include "Compression/ArithAlphabet.h"
#include "Compression/ArithModel.h"

#include "Cryptography/rijndael.h"

class CNetInputSerialize;
class CNetChannel;
class CStatsCollector;

typedef CArithLargeAlphabetOrder0<> TMsgAlphabet;
typedef CArithAlphabetOrder1< CArithAlphabetOrder0 > TAckAlphabet;

class CCTPEndpoint
{
public:
	CCTPEndpoint( CMementoMemoryManagerPtr pMMM );
	~CCTPEndpoint();

	static const char * GetCurrentProcessingMessageDescription();

	void Reset();
	void Init( CNetChannel *pParent );
	ILINE bool AddSendable( INetSendable *p, int numAfterHandle, const SSendableHandle * afterHandle, SSendableHandle * handle ) { return CallAddSubstitute(p,numAfterHandle,afterHandle,handle,false); }
	ILINE bool SubstituteSendable( INetSendable *p, int numAfterHandle, const SSendableHandle * afterHandle, SSendableHandle * handle ) { return CallAddSubstitute(p,numAfterHandle,afterHandle,handle,true); }
	bool RemoveSendable( SSendableHandle handle );
	INetSendablePtr FindSendable( SSendableHandle handle );
	void Update(CTimeValue nTime, bool isDisconnecting, bool bAllowUserSend, bool bForceSend, bool bFlushBuffers);
	CTimeValue GetNextUpdateTime( CTimeValue );
	void ProcessPacket(CTimeValue nTime, TMemHdl hdl, bool allowQueueing, bool inSync);
	void GetMemoryStatistics(ICrySizer *pSizer, bool countingThis = false);
	void MarkNotUserSink( INetMessageSink * pSink );
	float GetPing( bool smoothed ) const { return m_PacketRateCalculator.GetPing( smoothed ); }
	bool IsSufferingHighLatency(CTimeValue nTime) const { return m_PacketRateCalculator.IsSufferingHighLatency(nTime); }
	CTimeValue GetRemoteTime() const { return m_PacketRateCalculator.GetRemoteTime(); }
	bool IsTimeReady() const { return m_PacketRateCalculator.IsTimeReady(); }
	CTimeValue GetNextUpdateTime();
	uint32 GetMostRecentAckedSeq() const { return m_nInputAck; }
	uint32 GetMostRecentSentSeq() const { return m_nOutputSeq; }
	void SetPerformanceMetrics( INetChannel::SPerformanceMetrics* pMetrics )
	{
		m_PacketRateCalculator.SetPerformanceMetrics( *pMetrics );
	}
	bool LookupMessage( const char * name, SNetMessageDef const ** ppDef, INetMessageSink ** ppSink );
	void FragmentedPacket(CTimeValue nTime) { m_PacketRateCalculator.FragmentedPacket(nTime); }
	void BackOff();

	bool GetBackoffTime( CTimeValue& tm, bool total );
	bool IsBackingOff()
	{
		CTimeValue blah;
		return GetBackoffTime(blah, false);
	}

	void SetEntityId( EntityId id ) { m_entityId = id; }

	unsigned int GetLostPackets() { return m_vInputState[m_nInputSeq].GetLostPackets(); }
	unsigned int GetUnreliableLostPackets() { return GetLostPackets(); }

	void PerformRegularCleanup();
	void EmptyMessages();
	bool InEmptyMode() const { return m_emptyMode; }

	void UnblockMessages()
	{
		NET_ASSERT(m_nStateBlockers);
		m_nStateBlockers--;
	}

	void ChangeSubscription( ICTPEndpointListener * pListener, uint32 eventMask );

	void SchedulerDebugDraw();
	void ChannelStatsDraw();

	void SetAfterSpawning( bool afterSpawning );

	float GetBandwidth( bool incoming )
	{
		return m_PacketRateCalculator.GetBandwidthUsage(g_time, incoming);
	}

	void AddPingSample( CTimeValue nTime, CTimeValue nPing, CTimeValue nRemoteTime )
	{
		m_PacketRateCalculator.AddPingSample(nTime, nPing, nRemoteTime);
	}

private:
	bool CallAddSubstitute( INetSendable *, int numAfterHandle, const SSendableHandle * afterHandle, SSendableHandle * handle, bool subs );

	//
	// types
	//

	// these are the messages that the endpoint can deal with directly
	// eIM_LastInternalMessage must appear last so that the dispatch code knows
	// when to pass a message to a higher level protocol handler
	enum EInternalMessages
	{
		// end of stream (needed so we know when to stop decoding)
		eIM_EndOfStream = 0,
		// must stay at the end
		eIM_LastInternalMessage
	};

	// this class represents data that we don't want to keep around
	// in all 128 sequence states (2*64) if at all possible
	struct SBigEndpointState
	{
		SBigEndpointState( size_t acks, CMessageMapper& msgMapper, Rijndael::Direction dir, const uint8* key, uint8* initVec );
		SBigEndpointState( const SBigEndpointState& cp ) : 
			m_AckAlphabet(cp.m_AckAlphabet),
			m_MsgAlphabet(cp.m_MsgAlphabet),
			m_ArithModel(cp.m_ArithModel)
#if ALLOW_ENCRYPTION
			,m_crypt(cp.m_crypt)
#endif
		{
			++g_objcnt.bigEndpointState;
		}
		~SBigEndpointState()
		{
			--g_objcnt.bigEndpointState;
		}
		SBigEndpointState& operator=( const SBigEndpointState& other )
		{
			m_AckAlphabet = other.m_AckAlphabet;
			m_MsgAlphabet = other.m_MsgAlphabet;
			m_ArithModel = other.m_ArithModel;
#if ALLOW_ENCRYPTION
			m_crypt = other.m_crypt;
#endif
			return *this;
		}
		void GetMemoryStatistics(ICrySizer *pSizer, bool countingThis = false)
		{
			SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::SBigEndpointState");
			if (countingThis)
				pSizer->Add(*this);
			m_ArithModel.GetMemoryStatistics(pSizer);
			m_AckAlphabet.GetMemoryStatistics(pSizer);
			m_MsgAlphabet.GetMemoryStatistics(pSizer);
		}

		void Encrypt( uint8 * pBuf, size_t len )
		{
#if ALLOW_ENCRYPTION
			uint8 * buf = (uint8*)alloca(len);
			NET_ASSERT(0 == (len&15));
			m_crypt.blockEncrypt( pBuf, len*8, buf );
			memcpy(pBuf, buf, len);
#endif
		}
		void Decrypt( uint8 * pBuf, size_t len )
		{
#if ALLOW_ENCRYPTION
			uint8 * buf = (uint8*)alloca(len);
			NET_ASSERT(0 == (len&15));
			m_crypt.blockDecrypt( pBuf, len*8, buf );
			memcpy(pBuf, buf, len);
#endif
		}

		size_t GetSize() { return m_AckAlphabet.GetSize() + /*m_MsgAlphabet.GetSize() +*/ m_ArithModel.GetSize(); }
		TAckAlphabet m_AckAlphabet;
		TMsgAlphabet m_MsgAlphabet;
		CArithModel m_ArithModel;
	private:
#if ALLOW_ENCRYPTION
		Rijndael m_crypt;
#endif
	};

	class CBigEndpointStateManager
	{
	public:
		CBigEndpointStateManager()
		{
			m_nAlloced = 0;
			m_minFree = 0;
			m_operationsSinceCleanup = 0;
		}

		~CBigEndpointStateManager()
		{
			FlushAll();
		}

		void FlushAll()
		{
			NET_ASSERT( m_nAlloced == 0 );

			while (m_vBuffer.size())
			{
				delete m_vBuffer.back();
				m_vBuffer.pop_back();
			}
		}

		SBigEndpointState * Create( size_t a, CMessageMapper& m, Rijndael::Direction dir, const uint8* key, uint8* initVec )
		{
			m_nAlloced ++;
			return new SBigEndpointState( a, m, dir, key, initVec );
		}

		SBigEndpointState * Clone( SBigEndpointState * pState )
		{
			FRAME_PROFILER("SBigEndpointsState:Clone", GetISystem(), PROFILE_NETWORK);
			m_nAlloced ++;

			m_operationsSinceCleanup ++;

			SBigEndpointState * pCloned;
			if (m_vBuffer.size())
			{
				pCloned = m_vBuffer.back();
				m_vBuffer.pop_back();
				*pCloned = *pState;
				m_minFree = std::min( m_minFree, uint32(m_vBuffer.size()) );
			}
			else
			{
				pCloned = new SBigEndpointState( *pState );
			}
			return pCloned;
		}

		void Free( SBigEndpointState * pState )
		{
			NET_ASSERT( m_nAlloced != 0 );
			m_nAlloced --;
			if (!m_vBuffer.empty())
			{
				pState->m_AckAlphabet = m_vBuffer[0]->m_AckAlphabet;
				pState->m_MsgAlphabet = m_vBuffer[0]->m_MsgAlphabet;
				pState->m_ArithModel.SimplifyMemoryUsing(m_vBuffer[0]->m_ArithModel);
			}
			m_vBuffer.push_back(pState);
		}

		size_t SpareSize()
		{
			size_t n = 0;
			for (size_t i=0; i<m_vBuffer.size(); i++)
				n += m_vBuffer[i]->GetSize();
			return n;
		}

		void PerformRegularCleanup()
		{
			if (m_operationsSinceCleanup < 1024)
				return;

			NET_ASSERT(m_minFree <= m_vBuffer.size());
			for (uint32 i=0; i<m_minFree; i++)
			{
				delete m_vBuffer.back();
				m_vBuffer.pop_back();
			}
			m_minFree = m_vBuffer.size();
			m_operationsSinceCleanup = 0;
		}

		void GetMemoryStatistics(ICrySizer *pSizer, bool countingThis = false)
		{
			SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::CBigEndpointStateManager");
			if (countingThis)
				pSizer->Add(*this);
			for (size_t i=0; i<m_vBuffer.size(); i++)
				m_vBuffer[i]->GetMemoryStatistics(pSizer, true);
		}

	private:
		std::vector<SBigEndpointState*> m_vBuffer;
		uint32 m_nAlloced;
		uint32 m_minFree;
		int m_operationsSinceCleanup;
	};

	// this tracks the state we were in at each sent packet
	// (so we know what state the other end "thinks" that we're in)
	// also contains read/write helpers for different things that we compress
	// adaptively (we need to know what the other end thinks is the connection
	// state, so it makes sense to make these operations members of this class)
	class CState
	{
	public:
		// must call Reset() before using!!
		CState() : m_pBigState(NULL) {}

		// reset back to initial state (a new connection is starting)
		void Reset( CBigEndpointStateManager* pBigMgr );
		// start a new state based on an existing one
		void Clone( CBigEndpointStateManager * pBigMgr, const CState& );

		// write an ack or a nack
		void WriteAck( CCommOutputStream& stm, bool bAck, CStatsCollector * pStats );
		// finish writing ack block
		void WriteEndAcks( CCommOutputStream& stm, CStatsCollector * pStats );
		// read an ack or a nack (returns false if at the end of the ack block)
		bool ReadAck( CCommInputStream& stm, bool& bAck, uint32& nSeq );

		// read a message id
		uint32 ReadMsgId( CCommInputStream& stm );
		// write a message id
		bool WriteMsgId( CCommOutputStream& stm, uint32 id, CStatsCollector * pStats, const char * name );

		ILINE void Encrypt( uint8 * buf, size_t len )
		{
			m_pBigState->Encrypt(buf, len);
		}
		ILINE void Decrypt( uint8 * buf, size_t len )
		{
			m_pBigState->Decrypt(buf, len);
		}

		// number of acked packets seen in this state-chain
		uint32 GetNumberOfAckedPackets() const { return m_nAckedPackets; }
		// number of lost packets seen in this state-chain
		uint32 GetLostPackets() const { return m_nAckedPackets - m_nAcks; }

		CArithModel * GetArithModel() { return &m_pBigState->m_ArithModel; }

		void FreeState( CBigEndpointStateManager * pBigMgr )
		{
			if (m_pBigState) pBigMgr->Free( m_pBigState );
			m_pBigState = NULL;
		}
		void CreateState( CBigEndpointStateManager* pBigMgr, CMessageMapper& msgMapper, Rijndael::Direction dir, const uint8* key, uint8* initVec )
		{
			NET_ASSERT( !m_pBigState );
			m_pBigState = pBigMgr->Create( 3, msgMapper, dir, key, initVec );
		}
		void GetMemoryStatistics(ICrySizer *pSizer, bool countingThis = false)
		{
			SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::CState");
			if (countingThis)
				pSizer->Add(*this);
			if (m_pBigState)
				m_pBigState->GetMemoryStatistics( pSizer, true );
		}
		size_t GetSize() { return sizeof(*this) + (m_pBigState? m_pBigState->GetSize() : 0); }

#if DEBUG_ENDPOINT_LOGIC
		void Dump( FILE * f ) const;
#endif

	private:
		// helper to update ack count trackers
		uint32 AddAck( bool bAck );

		// total packets acknowledged
		uint32 m_nAcks;
		// total packets acknowledged or not acknowledged
		uint32 m_nAckedPackets;
		// total packets through this state train
		uint32 m_nPackets;
		// number of acks in this state so far
		uint32 m_nAcksThisPacket;

		SBigEndpointState * m_pBigState;
	};
	// specialization of CState for outgoing messages
	class COutputState : public CState
	{
	public:
		COutputState() : m_nStateBlockers(0), m_headSentMessage(InvalidSentElem) {}

		void Reset( CBigEndpointStateManager* pBigMgr );
		void Clone( CBigEndpointStateManager * pBigMgr, const COutputState& );
		void SentMessage( CCTPEndpoint& ep, SSendableHandle msghdl, int nStateBlockers ) 
		{ 
			ep.SentElem( m_headSentMessage, msghdl );
			m_nStateBlockers += nStateBlockers; 
		}
		// these are debug helpers - to prevent over-writing a still in use state
		void Available() { NET_ASSERT(m_bAvailable == false); m_bAvailable = true; }
		void Unavailable() { NET_ASSERT(m_bAvailable == true); m_bAvailable = false; }
		bool IsAvailable() { return m_bAvailable; }
		void GetMemoryStatistics(ICrySizer *pSizer, bool countingThis = false)
		{
			SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::COutputState");
			if (countingThis)
				pSizer->Add(*this);
			CState::GetMemoryStatistics( pSizer );
		}

		// the messages which were sent for this state (for implementing the various
		// reliability mechanisms)
		uint32 m_headSentMessage;
		int m_nStateBlockers;

	private:
		bool m_bAvailable;
	};
	class CInputState : public CState
	{
	public:
		void Clone( CBigEndpointStateManager * pBigMgr, const CInputState&, uint32 nSeq );
		uint32 LastValid() const { return m_nValidSeq; }
		void Reset( CBigEndpointStateManager* pBigMgr );

	private:
		uint32 m_nValidSeq;
	};
	class CMessageSender;

	struct SAckData
	{
		SAckData( bool received, bool hadData, CTimeValue whatsTheTimeAgain ) : bReceived(received), bHadData(hadData), bHadUrgentData(0), when(whatsTheTimeAgain) {}
		bool bReceived : 1;
		bool bHadData   : 1;
		bool bHadUrgentData : 1;
		CTimeValue when;
	};

	typedef std::deque<SAckData> TAckDeque;

	// this struct holds a packet that has been received but not processed yet
	// (in the case of out of order received packets, we try to delay their
	//  processing momentarily)
	struct TQueuedIncomingPacket
	{
		TMemHdl hdl;
		CTimeValue when;
	};
	typedef std::map<uint32, TQueuedIncomingPacket> TIncomingPacketMap;

	//
	// constants
	//

	// the length of a whole window of packets
	static const uint32 WHOLE_SEQ = NUM_SEQ_SLOTS;
	// the length of half a window of packets
	static const uint32 HALF_SEQ = WHOLE_SEQ / 2;

	//
	// variables
	//

	// we use this to actually send data
	CNetChannel *	            m_pParent;

	// the sequence number of the last packet sent
	// (incremented in SendPacket)
	uint32					  		    m_nOutputSeq;
	// the sequence number of the last packet we received
	// from our paired endpoint
	uint32                    m_nInputSeq;
	// the sequence number of the last ack we received
	uint32                    m_nInputAck;
	// the last basis sequence we got
	uint32                    m_nLastBasisSeq;

	// this helps support RMI - by allowing upper layers to specify
	// an entity id to pass around
	EntityId                  m_entityId;

	// the sequence number of the last packet sent with reliable data
	uint32                    m_nReliableSeq;
	// should we wait for an ack/nack before sending reliable data again
	// (we are ping-pong (1-bit sliding window) for reliable ordered messages,
	//  we need to wait for an ack before sending the next packet with
	//  reliable ordered messages in it)
	bool                      m_bReliableWait;

	// message mapper for outgoing packets
	CMessageMapper            m_OutputMapper;
	// message mapper for incoming packets
	CMessageMapper            m_InputMapper;
	// all of our message sinks
	struct SSinkInfo
	{
		SSinkInfo( INetMessageSink * p ) : bCryNetwork(false), pSink(p), lastUsed(0) {}
		// crynetwork supplied message sinks get some special treatment - they
		// are assumed to know about when it's appropriate to post a message
		// and when not
		bool bCryNetwork;
		// the actual sink
		INetMessageSink * pSink;
		// when was this sink last used?
		uint32 lastUsed;
	};

	std::vector<SSinkInfo>    m_MessageSinks;

	// these two variables track which acks and nacks we need to send
	// (bool true == ack, false == nack, dqAcks first ack sequence number
	//  is nFrontAck)
	TAckDeque                 m_dqAcks;
	uint32                    m_nFrontAck;

	// these two structures track queued packets that are waiting to be
	// processed
	TIncomingPacketMap        m_incomingPackets;
	// only a member to avoid reallocating each frame - see Update()
	std::priority_queue<uint32> m_timedOutPackets; 

	// the state that the other end of the connection thinks that we're in
	COutputState              m_vOutputState[WHOLE_SEQ];
	// the state we think the other end of the connection is in
	CInputState               m_vInputState[WHOLE_SEQ];

	CPacketRateCalculator     m_PacketRateCalculator;

	CBigEndpointStateManager  m_bigStateMgrInput;
	CBigEndpointStateManager  m_bigStateMgrOutput;
	CNetOutputSerializeImpl   m_outputStreamImpl;
	bool m_bWritingPacketNeedsInSyncProcessing;
	CSimpleSerialize<CNetOutputSerializeImpl> m_outputStream;

	bool m_emptyMode;
	//int m_nBlockingMessages;

#if LOG_TFRC
	FILE* m_log_tfrc;
#endif

#if DEBUG_ENDPOINT_LOGIC
	// these functions are useful for debugging the low-level details
	// of the protocol implemented by this class
	FILE * m_log_output;
	FILE * m_log_input;
	void DumpOutputState(unsigned);
	void DumpInputState(unsigned);
#endif

#if DETECT_DUPLICATE_ACKS
	std::set<uint32> m_ackedPackets;
#endif

	//
	// functions
	//

	// this struct encodes the parameters that SendPacketIfAppropriate has
	// determined for SendPacket
	struct SSendPacketParams
	{
		SSendPacketParams() : nSize(0), bForce(false), bAllowUserSend(true) {}
		size_t nSize;
		bool bForce;
		bool bAllowUserSend;
	};
	// send a packet with some configuration parameters
	uint32 SendPacket( CTimeValue nTime, const SSendPacketParams& );

	// helper to perform actions necessary when receiving an ack
	bool AckPacket( CTimeValue nTime, uint32 nSeq, bool bOk );
	void BroadcastMessage( const SCTPEndpointEvent& evt );
	void UpdatePendingQueue( CTimeValue nTime, bool isDisconnecting );
	void SendPacketsIfNecessary( CTimeValue nTime, bool isDisconnecting, bool bAllowUserSend, bool bForce, bool bFlush );

	bool IsIdle();

#if MESSAGE_BACKTRACE_LENGTH > 0
	typedef MiniQueue<const SNetMessageDef *, MESSAGE_BACKTRACE_LENGTH> PreviouslyReceivedMessages;
	PreviouslyReceivedMessages m_previouslyReceivedMessages;
	PreviouslyReceivedMessages m_previouslySentMessages;
#endif

	// simple counters to aid debugging
	int m_nReceived;
	int m_nSent;
	int m_nDropped;
	int m_nReordered;
	int m_nQueued;
	int m_nRepeated;
	
	int m_nPQTimeout;
	int m_nPQReady;

	CTimeValue m_backoffTimer;
	bool m_receivedPacketSinceBackoff;

	class CMessageOutput;

	CMessageQueue m_queue;
	int m_nStateBlockers;
	int m_nUrgentAcks;

	struct SListener
	{
		ICTPEndpointListener * pListener;
		uint32 eventMask;
	};
	std::vector<SListener> m_listeners;

	uint8 m_assemblyBuffer[8192];
	uint32 m_assemblySize;

	static const uint32 InvalidSentElem = ~uint32(0);
	struct SSentElem
	{
		SSendableHandle hdl;
		uint32 next;
		SSentElem() : next(InvalidSentElem) {}
	};
	std::vector<SSentElem> m_sentElems;
	uint32 m_freeSentElem;
	void SentElem( uint32& head, const SSendableHandle& hdl );
	void ClearSentElems( uint32& head, std::vector<SSendableHandle>* pOut );
	void AckMessages( uint32& head, uint32 nSeq, bool ack, bool clear );

	CMementoMemoryManagerPtr m_pMMM;
};

#endif
