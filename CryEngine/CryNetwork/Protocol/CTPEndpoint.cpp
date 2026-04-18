//////////////////////////////////////////////////////////////////////
//
//	Crytek Network source code
//	
//	File: CTPEndpoint.cpp
//  Description: non-sequential receive protocol
//
//	History:
//	-July 25,2001:Created by Alberto Demichelis
//  -July 20,2004:Refactored by Craig Tiller
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CTPEndpoint.h"
#include "IDataProbe.h"
#include "Streams/CommStream.h"
#include "Serialize.h"
#include "NetChannel.h"
#include "Network.h"
#include "ITimer.h"
#include "DebugKit/DebugKit.h"

#include "Context/ContextView.h"
#include "Config.h"
#include "ITextModeConsole.h"

static const SNetMessageDef * g_processingMessage = 0;

struct SProcessingMessage
{
public:
	SProcessingMessage( const SNetMessageDef * pDef )
	{
		m_prior = g_processingMessage;
		g_processingMessage = pDef;
	}

	~SProcessingMessage()
	{
		g_processingMessage = m_prior;
	}

private:
	const SNetMessageDef * m_prior;
};

// get the 64 bit representation of a time value
inline uint64 GetIntRepresentationOfTime( CTimeValue value )
{
	return (uint64) value.GetMilliSecondsAsInt64();
}
inline CTimeValue GetTimeRepresentationOfInt( uint64 value )
{
	CTimeValue out;
	out.SetMilliSeconds(value);
	return out;
}

static const char * ReliabilityMessage( INetMessage * pMsg )
{
	ENetReliabilityType nrt = pMsg->GetDef()->reliability;
	switch (nrt)
	{
	case eNRT_ReliableOrdered:
		return "reliable-ordered";
	case eNRT_ReliableUnordered:
		return "reliable-unordered";
	case eNRT_UnreliableOrdered:
		return "unreliable-ordered";
	default:
		return "unknown-reliability";
	}
}

static const uint32 TimestampBits = 64;
static const int LogMaxMessagesPerPacket = 16;
static const int MaxMessagesPerPacket = 1 << LogMaxMessagesPerPacket;

static const int SequenceNumberBits = 8;
static const uint32 SequenceNumberMask = (1u<<SequenceNumberBits)-1u;
static const uint32 SequenceNumberDiameter = 1u << SequenceNumberBits;
static const uint32 SequenceNumberRadius = 1u << (SequenceNumberBits-1);

static const CTimeValue IncomingTimeoutLength = 0.033f;

#if ENABLE_DEBUG_KIT
static void DumpBytes( const uint8 * p, size_t len )
{
	char l[256];

	char *o = l;
	for (int i=0; i<len; i++)
	{
		o += sprintf(o, "%.2x ", p[i]);
		if ((i & 31)==31)
		{
			NetLog(l);
			o = l;
		}
	}
	if (len & 31)
		NetLog(l);
}
#endif

//
// SBigEndpointState
//

CCTPEndpoint::SBigEndpointState::SBigEndpointState( size_t acks, CMessageMapper& msgMapper, Rijndael::Direction dir, const uint8* key, uint8* initVec ) :
	m_AckAlphabet(acks), 
	m_MsgAlphabet(msgMapper.GetNumberOfMsgIds()), 
	m_ArithModel()
{
	++g_objcnt.bigEndpointState;
#if ALLOW_ENCRYPTION
	m_crypt.init( Rijndael::CBC, dir, key, Rijndael::Key32Bytes, initVec );
#endif

	if (msgMapper.GetNumberOfMsgIds() == 1)
		return;

	m_MsgAlphabet.RecalculateProbabilities();
}

//void CCTPEndpoint::SBigEndpointState::GetMemoryStatistics(ICrySizer *pSizer)
//{
//	SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::SBigEndpointState");
//	m_ArithModel.GetMemoryStatistics(pSizer);
//	pSizer->AddObject( &m_AckAlphabet, m_AckAlphabet.GetMemorySize() );
//	pSizer->AddObject( &m_MsgAlphabet, m_MsgAlphabet.GetMemorySize() );
//}

//void CCTPEndpoint::CBigEndpointStateManager::GetMemoryStatistics(ICrySizer *pSizer)
//{
//	SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::CBigEndpointStateManager");
//	for (size_t i=0; i<m_vBuffer.size(); i++)
//		m_vBuffer[i]->GetMemoryStatistics(pSizer);
//}

//
// CCTPEndpoint::S*State
//

// Reset
ILINE void CCTPEndpoint::CState::Reset( CBigEndpointStateManager * pBigMgr )
{
	m_nAcks = 0;
	m_nAckedPackets = 0;
	m_nPackets = 0;
	m_nAcksThisPacket = 0;
	FreeState( pBigMgr );
}

ILINE void CCTPEndpoint::COutputState::Reset( CBigEndpointStateManager * pBigMgr )
{
	CState::Reset( pBigMgr );
	//m_queue.AckMessages( &*m_vpSentMessages.begin(), m_vpSentMessages.size(), false );
	NET_ASSERT(m_headSentMessage == InvalidSentElem);
	NET_ASSERT(!m_nStateBlockers);
	m_headSentMessage = InvalidSentElem;
	m_bAvailable = true;
	m_nStateBlockers = 0;
}

ILINE void CCTPEndpoint::CInputState::Reset( CBigEndpointStateManager * pBigMgr )
{
	CState::Reset( pBigMgr );
	m_nValidSeq = 0;
}

#if DEBUG_ENDPOINT_LOGIC
// Dump
void CCTPEndpoint::CState::Dump( FILE * f ) const
{
	fprintf( f, " %.4d %.4d %.4d\n", m_nPackets, m_nAcks, m_nAckedPackets );
	fprintf( f, "AckAlphabet:\n" );
	m_AckAlphabet.DumpCountsToFile(f);
	fprintf( f, "MsgAlphabet:\n" );
	m_MsgAlphabet.DumpCountsToFile(f);
	fprintf( f, "TextAlphabet:\n" );
	m_ArithModel.DumpCountsToFile(f);
}
#endif

// GetMemoryStatistics
//void CCTPEndpoint::CState::GetMemoryStatistics(ICrySizer *pSizer)
//{
//	SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::CState");
//	if (m_pBigState)
//		m_pBigState->GetMemoryStatistics( pSizer );
//}

//void CCTPEndpoint::COutputState::GetMemoryStatistics(ICrySizer *pSizer)
//{
//	SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint::COutputState");
//	CState::GetMemoryStatistics( pSizer );
//	pSizer->AddContainer( m_vpSentMessages );
//}

// Clone
ILINE void CCTPEndpoint::CState::Clone( CBigEndpointStateManager * pBigMgr, 
																			  const CState& s )
{
	m_nAcks = s.m_nAcks;
	m_nAckedPackets = s.m_nAckedPackets;
	m_nPackets = s.m_nPackets + 1;
	m_nAcksThisPacket = 0;

	NET_ASSERT( s.m_pBigState );
	NET_ASSERT( !m_pBigState );
	m_pBigState = pBigMgr->Clone( s.m_pBigState );
	m_pBigState->m_AckAlphabet.RecalculateProbabilities();
	m_pBigState->m_MsgAlphabet.RecalculateProbabilities();
	m_pBigState->m_ArithModel.RecalculateProbabilities();
}

void CCTPEndpoint::COutputState::Clone( CBigEndpointStateManager * pBigMgr, 
																			  const COutputState& s )
{
	CCTPEndpoint::CState::Clone( pBigMgr, s );
	NET_ASSERT(m_headSentMessage == InvalidSentElem);
	m_nStateBlockers = 0;
}

void CCTPEndpoint::CInputState::Clone( CBigEndpointStateManager * pBigMgr, 
																			 const CCTPEndpoint::CInputState& s, uint32 nSeq )
{
	CCTPEndpoint::CState::Clone( pBigMgr, s );

	m_nValidSeq = nSeq;
}

// acks & nacks
ILINE uint32 CCTPEndpoint::CState::AddAck( bool bAck )
{
	if (bAck) 
		m_nAcks++; 
	m_nAcksThisPacket++; 
	m_nAckedPackets++;
	return m_nAckedPackets;
}

void CCTPEndpoint::CState::WriteAck( CCommOutputStream& stm, bool bAck, CStatsCollector * pStats )
{
#if DEBUG_STREAM_INTEGRITY
	stm.WriteBits(35,8);
#endif
	m_pBigState->m_AckAlphabet.WriteSymbol( stm, bAck );
	AddAck( bAck );
}

void CCTPEndpoint::CState::WriteEndAcks( CCommOutputStream& stm, CStatsCollector * pStats )
{
#if DEBUG_STREAM_INTEGRITY
	stm.WriteBits(35,8);
#endif
	m_pBigState->m_AckAlphabet.WriteSymbol( stm, 2 );
}

bool CCTPEndpoint::CState::ReadAck( CCommInputStream& stm, bool& bAck, uint32& nSeq )
{
#if DEBUG_STREAM_INTEGRITY
	uint8 stmBits = stm.ReadBits(8);
	NET_ASSERT( 35 == stmBits );
	if (35 != stmBits)
		return false;
#endif
	unsigned symbol = m_pBigState->m_AckAlphabet.ReadSymbol( stm );

	bool bRet = true;
	switch (symbol)
	{
	case 0:
		bAck = false;
		nSeq = AddAck( false );
		break;
	case 1:
		bAck = true;
		nSeq = AddAck( true );
		break;
	case 2:
		bRet = false;
		break;
	default:
		NET_ASSERT(false);
	}

	return bRet;
}

// message id's
ILINE bool CCTPEndpoint::CState::WriteMsgId( CCommOutputStream& stm, uint32 id, 
																						 CStatsCollector * pStats, const char * name )
{
	if (!m_pBigState)
		return false;

#if STATS_COLLECTOR && ENABLE_ACCURATE_BANDWIDTH_PROFILING
	float sz = stm.GetBitSize();
#endif

#if ENABLE_DEBUG_KIT
	DEBUGKIT_ANNOTATION(string("message: ") + name);
#endif
#if DEBUG_STREAM_INTEGRITY
	stm.WriteBits(73,8);
#endif
//	NetLog( "msg %s(%d): %f", name, id, m_pBigState->m_MsgAlphabet.EstimateSymbolSizeInBits(id) );

	m_pBigState->m_MsgAlphabet.WriteSymbol( stm, id );
#if DOUBLE_WRITE_MESSAGE_HEADERS
	m_pBigState->m_MsgAlphabet.WriteSymbol( stm, id );
#endif

#if DEBUG_STREAM_INTEGRITY
	stm.WriteBits(77,8);
#endif

#if STATS_COLLECTOR && ENABLE_ACCURATE_BANDWIDTH_PROFILING
	STATS.Message( name, stm.GetBitSize() - sz );
#endif

	return true;
}

ILINE uint32 CCTPEndpoint::CState::ReadMsgId( CCommInputStream& stm )
{
#if DEBUG_STREAM_INTEGRITY
	uint32 check = stm.ReadBits(8);
	NET_ASSERT( check == 73 );
#endif
	uint32 msg = m_pBigState->m_MsgAlphabet.ReadSymbol( stm );
#if DOUBLE_WRITE_MESSAGE_HEADERS
	uint32 msg2 = m_pBigState->m_MsgAlphabet.ReadSymbol( stm );
	NET_ASSERT(msg == msg2);
#endif
//	NetLog("gotmsg: %d", msg);
#if DEBUG_STREAM_INTEGRITY
	check = stm.ReadBits(8);
	NET_ASSERT( check == 77 );
#endif

	return msg;
}

//
// CCTPEndpoint
//

static CDefaultStreamAllocator s_allocator;

CCTPEndpoint::CCTPEndpoint( CMementoMemoryManagerPtr pMMM ) : 
	m_outputStreamImpl(m_assemblyBuffer+1, sizeof(m_assemblyBuffer)), // +1 to account for sequence bytes
	m_outputStream(m_outputStreamImpl),
	m_emptyMode(false),
	m_queue(CNetwork::Get()->GetMessageQueueConfig()),
	m_pMMM(pMMM),
	m_freeSentElem(CCTPEndpoint::InvalidSentElem)
{
#if LOG_TFRC
	char filename[sizeof("tfrc_endpoint_12345678.log")];
	_snprintf(filename, sizeof(filename), "tfrc_endpoint_%08x.log", (uint32)this);
	m_log_tfrc = fopen(filename, "wt");
#endif

#if DEBUG_ENDPOINT_LOGIC
	m_log_output = NULL;
	m_log_input = NULL;
#endif
	Init( NULL );
}

CCTPEndpoint::~CCTPEndpoint()
{
	SCOPED_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

#if LOG_TFRC
	if (m_log_tfrc) fclose(m_log_tfrc);
#endif

#if DEBUG_ENDPOINT_LOGIC
	if (m_log_output) fclose(m_log_output);
	if (m_log_input) fclose(m_log_input);
#endif
	for (size_t i=0; i<WHOLE_SEQ; i++)
	{
		m_vInputState[i].FreeState(&m_bigStateMgrInput);
		m_vOutputState[i].FreeState(&m_bigStateMgrOutput);
	}
	while (!m_incomingPackets.empty())
	{
		MMM().FreeHdl(m_incomingPackets.begin()->second.hdl);
		m_incomingPackets.erase(m_incomingPackets.begin());
	}

#if MESSAGE_BACKTRACE_LENGTH > 0
	NetLog("---------------------------------------------------------------------------------");
	NetLog("%d previously received messages:", m_previouslyReceivedMessages.Size());
	for (PreviouslyReceivedMessages::SIterator iter = m_previouslyReceivedMessages.Begin(); iter != m_previouslyReceivedMessages.End(); ++iter)
		NetLog("   %s", (*iter)->description);
	NetLog("---------------------------------------------------------------------------------");
	NetLog("%d previously sent messages:", m_previouslySentMessages.Size());
	for (PreviouslyReceivedMessages::SIterator iter = m_previouslySentMessages.Begin(); iter != m_previouslySentMessages.End(); ++iter)
		NetLog("   %s", (*iter)->description);
	NetLog("---------------------------------------------------------------------------------");
#endif

	m_bigStateMgrInput.FlushAll();
	m_bigStateMgrOutput.FlushAll();
}

void CCTPEndpoint::Init( CNetChannel *pParent )
{
	MMM_REGION(m_pMMM);
	static int N = 0;

	// these are the interfaces we use
	m_pParent = pParent;

	m_entityId = 0;

#if DEBUG_ENDPOINT_LOGIC
	if (m_log_output) fclose(m_log_output);
	if (m_log_input) fclose(m_log_input);
	m_log_output = NULL;
	m_log_input = NULL;
	if (pParent)
	{
		char buf[30];
		sprintf(buf, "output.%d.log", ++N);
		m_log_output = fopen(buf,"wt");
		sprintf(buf, "input.%d.log", N);
		m_log_input = fopen(buf,"wt");
		NET_ASSERT(m_log_input);
		NET_ASSERT(m_log_output);
	}
#endif

	// setup message
	struct SProtocolBuilder : public IProtocolBuilder
	{
		std::vector<SNetProtocolDef> sending;
		std::vector<SNetProtocolDef> receiving;
		std::vector<SSinkInfo> sinks;
		virtual void AddMessageSink( INetMessageSink * pSink, 
			const SNetProtocolDef& protocolSending, 
			const SNetProtocolDef& protocolReceiving )
		{
			sinks.push_back(pSink);
			sending.push_back(protocolSending);
			receiving.push_back(protocolReceiving);
		}
	};
	SProtocolBuilder protocolBuilder;
	if (pParent)
		pParent->DefineProtocol( &protocolBuilder );

	if (!protocolBuilder.sending.empty())
		m_OutputMapper.Reset( eIM_LastInternalMessage, 
			&protocolBuilder.sending[0], protocolBuilder.sending.size() );
	else
		m_OutputMapper.Reset( eIM_LastInternalMessage, 0,0 );

	if (!protocolBuilder.receiving.empty())
		m_InputMapper.Reset( eIM_LastInternalMessage, 
			&protocolBuilder.receiving[0], protocolBuilder.receiving.size() );
	else
		m_InputMapper.Reset( eIM_LastInternalMessage, 0,0 );

	m_MessageSinks.swap( protocolBuilder.sinks );

	// initialize our state array
	for (uint32 i = 0; i < WHOLE_SEQ; i++)
	{
		m_vInputState[i].Reset( &m_bigStateMgrInput );
		m_vOutputState[i].Reset( &m_bigStateMgrOutput );
	}
	m_vInputState[0].CreateState( &m_bigStateMgrInput, m_InputMapper, Rijndael::Decrypt, m_pParent ? m_pParent->GetPrivateKey().key : NULL, 0 );
	m_vOutputState[0].CreateState( &m_bigStateMgrOutput, m_OutputMapper, Rijndael::Encrypt, m_pParent ? m_pParent->GetPrivateKey().key : NULL, 0 );

	m_vOutputState[0].Unavailable();

	// initialize sequence numbers
	m_nOutputSeq = 0;
	m_nInputSeq = 1;
	m_nInputAck = 0;
	m_nReliableSeq = 0;
	m_bReliableWait = false;
	m_nLastBasisSeq = 0;
	m_nUrgentAcks = 0;
	m_receivedPacketSinceBackoff = true;

	// initialize debug counters
	m_nReceived = 0;
	m_nSent = 0;
	m_nDropped = 0;
	m_nReordered = 0;
	m_nQueued = 0;
	m_nRepeated = 0;

	m_nPQTimeout = 0;
	m_nPQReady = 0;

	// setup the ack structure
	m_nFrontAck = 1;
	m_dqAcks.clear();

	m_nStateBlockers = 0;
}

void CCTPEndpoint::MarkNotUserSink( INetMessageSink * pSink )
{
	for (size_t i=0; i<m_MessageSinks.size(); i++)
		if (m_MessageSinks[i].pSink == pSink)
			m_MessageSinks[i].bCryNetwork = true;
}

void CCTPEndpoint::GetMemoryStatistics( ICrySizer * pSizer, bool countingThis )
{
	SIZER_COMPONENT_NAME(pSizer, "CCTPEndpoint");
	MMM_REGION(m_pMMM);

	if (countingThis)
		if (!pSizer->Add( *this ))
			return;

	for (size_t i=0; i<WHOLE_SEQ; i++)
	{
		m_vInputState[i].GetMemoryStatistics(pSizer);
		m_vOutputState[i].GetMemoryStatistics(pSizer);
	}

	m_bigStateMgrInput.GetMemoryStatistics(pSizer);
	m_bigStateMgrOutput.GetMemoryStatistics(pSizer);

	pSizer->AddContainer(m_incomingPackets);
	pSizer->AddObject(&m_timedOutPackets, m_timedOutPackets.size()*sizeof(uint32));

	m_queue.GetMemoryStatistics(pSizer);

	m_OutputMapper.GetMemoryStatistics(pSizer);
	m_InputMapper.GetMemoryStatistics(pSizer);
	pSizer->AddContainer( m_MessageSinks );
	pSizer->AddContainer( m_listeners );
	pSizer->AddContainer( m_dqAcks );

	m_outputStreamImpl.GetMemoryStatistics(pSizer);
	//m_outputStream.GetMemoryStatistics(pSizer);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CCTPEndpoint.TMemHdl");
		for (TIncomingPacketMap::const_iterator it = m_incomingPackets.begin(); it != m_incomingPackets.end(); ++it)
			MMM().AddHdlToSizer(it->second.hdl, pSizer);
	}

	m_PacketRateCalculator.GetMemoryStatistics(pSizer);
}

void CCTPEndpoint::Reset()
{
	Init( m_pParent );
}

void CCTPEndpoint::PerformRegularCleanup()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_NETWORK);
	MMM_REGION(m_pMMM);

	m_bigStateMgrInput.PerformRegularCleanup();
	m_bigStateMgrOutput.PerformRegularCleanup();
}

bool CCTPEndpoint::CallAddSubstitute( INetSendable * pMsg, int numAfterHandle, const SSendableHandle * afterHandle, SSendableHandle * handle, bool subs )
{
	if (m_nStateBlockers && pMsg->CheckParallelFlag(eMPF_StateChange))
		NET_ASSERT(!"Can't send state change with blocking messages still about");

	if (m_emptyMode)
	{
#if LOG_MESSAGE_DROPS
		if (CNetCVars::Get().LogDroppedMessages())
			NetLog("REJECTED MESSAGE: %s", pMsg->GetDescription());
#endif
		pMsg->UpdateState(0, eNSSU_Rejected);
		return false;
	}
	else
	{
		switch (m_queue.AddSendable( pMsg, numAfterHandle, afterHandle, handle, subs ))
		{
		case eMQASR_Failed:
			return false;
		case eMQASR_Ok_BecomeAlerted:
			BroadcastMessage(SCTPEndpointEvent(eCEE_BecomeAlerted));
			// fall through
		case eMQASR_Ok:
			return true;
		default:
			NET_ASSERT(!"unknown return code from CMessageQueue::AddSendable");
			return false;
		}
	}
}

bool CCTPEndpoint::RemoveSendable( SSendableHandle handle )
{
	return m_queue.RemoveSendable(handle);
}

INetSendablePtr CCTPEndpoint::FindSendable( SSendableHandle handle )
{
	return m_queue.FindSendable(handle);
}

CTimeValue CCTPEndpoint::GetNextUpdateTime( CTimeValue nTime )
{
	CTimeValue backoffTime;
	if (GetBackoffTime(backoffTime, false))
		return backoffTime;

	// search for when we can get a packet out
	int age = m_nOutputSeq - m_nInputAck;
	return m_PacketRateCalculator.GetNextPacketTime(age, IsIdle());
}

void CCTPEndpoint::UpdatePendingQueue( CTimeValue nTime, bool isDisconnecting )
{
	// check to see if we have anything that's timed out
	uint32 lastTimedOut = 0;
	for (TIncomingPacketMap::reverse_iterator it = m_incomingPackets.rbegin(); it != m_incomingPackets.rend(); ++it)
	{
		if (nTime - it->second.when > IncomingTimeoutLength)
		{
			lastTimedOut = it->first;
			break;
		}
	}
	// if something has timed out, process all packets up to and including it
	// also keep going as long as we're processing packets sequentially
	while (!m_incomingPackets.empty())
	{
		bool cont = false;
		if (m_incomingPackets.begin()->first <= lastTimedOut)
		{
			cont = true;
			m_nPQTimeout ++;
#if ENABLE_DEBUG_KIT
			if (CNetCVars::Get().EndpointPendingQueueLogging)
				NetLog("PQ: process %d due to %d timing out", m_incomingPackets.begin()->first, lastTimedOut);
#endif
		}
		else if (m_incomingPackets.begin()->first == m_nInputSeq+1)
		{
			cont = true;
			m_nPQReady ++;
#if ENABLE_DEBUG_KIT
			if (CNetCVars::Get().EndpointPendingQueueLogging)
				NetLog("PQ: process %d as it's in order", m_nInputSeq);
#endif
		}
		if (!cont)
			break;
		ProcessPacket( nTime, m_incomingPackets.begin()->second.hdl, false, false );
		m_incomingPackets.erase(m_incomingPackets.begin());
	}
}

void CCTPEndpoint::SendPacketsIfNecessary( CTimeValue nTime, bool isDisconnecting, bool bAllowUserSend, bool bForce, bool bFlush )
{
	if (IsBackingOff())
		return;
	
	// now see if we need to send some packets
	bool bResend = false;

	// if we're disconnecting, we're somewhat more stringent about what we do
	bFlush &= !isDisconnecting;
	bForce |= isDisconnecting;
	bAllowUserSend &= !isDisconnecting;

	bForce |= bFlush;
	do
	{
		// don't ever send a new packet if we would exhaust sequence numbers
		NET_ASSERT( m_nOutputSeq >= m_nInputAck );
		if ( m_nOutputSeq + 1 >= m_nInputAck + WHOLE_SEQ )
		{
			bResend = true;
		}
		int age = m_nOutputSeq - m_nInputAck;

		uint16 maxOutputSize = m_PacketRateCalculator.GetMaxPacketSize( nTime );

		bool bSend = false;
		SSendPacketParams params;

		params.bAllowUserSend = bAllowUserSend & !bFlush;

		params.nSize = m_PacketRateCalculator.GetIdealPacketSize( age, IsIdle(), maxOutputSize );
		bSend |= params.nSize != 0;

		params.bForce = m_dqAcks.size() > (WHOLE_SEQ/4);
		if (bForce)
		{
			params.bForce = true;
			params.nSize = maxOutputSize;
			bSend = true;
		}

		if (bSend)
		{
			if (bResend)
			{
				NetWarning( "Resend last packet to %s", RESOLVER.ToString(m_pParent->GetIP()).c_str() );
				CCommOutputStream& stm = m_outputStreamImpl.GetOutput();
				m_pParent->Send( m_assemblyBuffer, m_assemblySize );
				m_PacketRateCalculator.SentPacket( nTime, m_nOutputSeq, stm.GetOutputSize() );
				m_nRepeated ++;
				m_nSent ++;
			}
			else
			{
				BroadcastMessage(SCTPEndpointEvent(eCEE_SendingPacket));

				uint16 capOutputSize = maxOutputSize;
				if (capOutputSize > 500)
					capOutputSize = std::max(500, capOutputSize-32);
				if (params.nSize > capOutputSize)
					params.nSize = capOutputSize;

				bSend = SendPacket( nTime, params ) != 0;
			}
		}
	}
	while (!m_emptyMode && !bResend && bFlush && !m_queue.IsEmpty());
	// the above while loop keeps sending packets if we've been requested to flush until the message queues are
	// completely empty
}

void CCTPEndpoint::Update( CTimeValue nTime, bool isDisconnecting, bool bAllowUserSend, bool bForce, bool bFlush )
{
	ASSERT_GLOBAL_LOCK;
	FRAME_PROFILER( "CCTPEndpoint:Update", GetISystem(), PROFILE_NETWORK );

	if (m_emptyMode)
		return;

	m_PacketRateCalculator.UpdateLatencyLab(nTime);

	UpdatePendingQueue(nTime, isDisconnecting);

	if (m_emptyMode)
		return;

#if LOG_TFRC
	fprintf( m_log_tfrc, "[time:%f] TcpFriendlyBitRate=%f\n", nTime.GetSeconds(), m_PacketRateCalculator.GetTcpFriendlyBitRate() );
#endif
	SendPacketsIfNecessary(nTime, isDisconnecting, bAllowUserSend, bForce, bFlush);

#if DEBUG_ENDPOINT_LOGIC
	if (bSend)
	{
		DumpOutputState( nTime );
	}
#endif

	if (m_emptyMode)
		return;
}

bool CCTPEndpoint::AckPacket( CTimeValue nTime, uint32 nSeq, bool bOk )
{
//	NET_ASSERT( m_nOutputSeq >= nSeq );
	MMM_REGION(m_pMMM);

	if (m_nOutputSeq < nSeq)
	{
		char msg[512];
		sprintf(msg, "Received an ack for a packet newer than what we've sent (received %d, at %d); disconnecting", nSeq, m_nOutputSeq);
		m_pParent->Disconnect(eDC_ProtocolError, msg);
		return false;
	}

	// already acked?
	if (nSeq <= m_nInputAck)
		return true;

#if DETECT_DUPLICATE_ACKS
	if (m_ackedPackets.find(nSeq) != m_ackedPackets.end())
	{
		NET_ASSERT(false);
		NetLog("Duplicate ack detected (seq=%d)", nSeq);
		m_pParent->Disconnect(eDC_ProtocolError, "duplicate ack detected");
		return false;
	}
	m_ackedPackets.insert(nSeq);
#endif

	NET_ASSERT( nSeq == m_nInputAck+1 );

	if (m_vOutputState[m_nInputAck%WHOLE_SEQ].IsAvailable())
	{
		NetWarning("Illegal ack detected (seq=%d)", nSeq);
		m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
		return false;
	}
	m_vOutputState[m_nInputAck%WHOLE_SEQ].Available();
	m_vOutputState[m_nInputAck%WHOLE_SEQ].FreeState(&m_bigStateMgrOutput);

	m_nInputAck = nSeq;

	if (m_bReliableWait && m_nReliableSeq == nSeq)
		m_bReliableWait = false;

	COutputState& state = m_vOutputState[nSeq % WHOLE_SEQ];

	m_PacketRateCalculator.AckedPacket( nTime, nSeq, bOk );
	AckMessages( state.m_headSentMessage, nSeq, bOk, false );
	m_nStateBlockers -= state.m_nStateBlockers;
	state.m_nStateBlockers = 0;

	if (!m_nStateBlockers && !m_queue.IsBlockingStateChange())
	{
		SCTPEndpointEvent evt(eCEE_NoBlockingMessages);
		BroadcastMessage(evt);
	}

	return true;
}

void CCTPEndpoint::EmptyMessages()
{
	MMM_REGION(m_pMMM);

	m_emptyMode = true;

	for (size_t i=0; i<WHOLE_SEQ; i++)
	{
		AckMessages( m_vOutputState[i].m_headSentMessage, 0, false, true );
		m_nStateBlockers -= m_vOutputState[i].m_nStateBlockers;
		m_vOutputState[i].m_nStateBlockers = 0;
	}

	m_queue.Empty();

	for (size_t i=0; i<WHOLE_SEQ; i++)
	{
		m_vOutputState[i].Reset( &m_bigStateMgrOutput );
	}
}

#if DEBUG_ENDPOINT_LOGIC
void CCTPEndpoint::DumpOutputState( unsigned nTime )
{
	fprintf( m_log_output, "-----------------------------------\n" );
	fprintf( m_log_output, "Time: %d\n", nTime );
	fprintf( m_log_output, "OutputSeq: %d\n", m_nOutputSeq );
	fprintf( m_log_output, "InputAck: %d\n", m_nInputAck );
	fprintf( m_log_output, "ReliableSeq: %d %s\n", m_nReliableSeq, m_bReliableWait? "waiting" : "not-waiting" );
	fprintf( m_log_output, "Acks: from %d (with %d queued)\n     ", m_nFrontAck, m_dqAcks.size() );
	for (size_t i=0; i<m_dqAcks.size(); ++i)
		fprintf( m_log_output, m_dqAcks[i]?"#":"." );
	fprintf( m_log_output, "\n" );
	fprintf( m_log_output, "Current State:" );
	m_vOutputState[m_nOutputSeq%WHOLE_SEQ].Dump( m_log_output );
	fprintf( m_log_output, "Basis State:" );
	m_vOutputState[m_nInputAck%WHOLE_SEQ].Dump( m_log_output );

	fflush( m_log_output );
}

void CCTPEndpoint::DumpInputState( unsigned nTime )
{
	fprintf( m_log_input, "-----------------------------------\n" );
	fprintf( m_log_input, "Time: %d\n", nTime );
	fprintf( m_log_input, "InputSeq: %d\n", m_nInputSeq-1 );
	fprintf( m_log_input, "Acks: from %d (with %d queued)\n     ", m_nFrontAck, m_dqAcks.size() );
	for (size_t i=0; i<m_dqAcks.size(); ++i)
		fprintf( m_log_input, m_dqAcks[i]?"#":"." );
	fprintf( m_log_input, "\n" );
	fprintf( m_log_input, "Current State:" );
	m_vInputState[(m_nInputSeq-1)%WHOLE_SEQ].Dump( m_log_input );

	fflush( m_log_input );
}
#endif

void CCTPEndpoint::ProcessPacket( CTimeValue nTime, TMemHdl hdl, bool bQueueing, bool inSync )
{
	MMM_REGION(m_pMMM);

#if _DEBUG && defined(USER_craig)
	//SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

	ASSERT_GLOBAL_LOCK;
	FRAME_PROFILER( "CCTPEndpoint:ProcessPacket", GetISystem(), PROFILE_NETWORK );

	if (bQueueing)
		m_receivedPacketSinceBackoff = true;

	if (MMM().GetHdlSize(hdl) < 8 || ((MMM().GetHdlSize(hdl)-2-4*DEBUG_SEQUENCE_NUMBERS) & 15))
	{
		uint32 sizeForMessage = MMM().GetHdlSize(hdl);
		//m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
		MMM().FreeHdl(hdl);
		NetWarning("Illegal packet size [size was %d, from %s]", sizeForMessage, m_pParent->GetName());
		return;
	}

	//
	// read sequence data
	//

	uint8* normBytes = (uint8*)MMM().PinHdl(hdl);
	size_t pktLen = MMM().GetHdlSize(hdl)-2-4*DEBUG_SEQUENCE_NUMBERS;

//	DumpBytes( normBytes, MMM_PACKETDATA.GetHdlSize(hdl) );

	uint32 nCurrentSeq, nBasisSeq;

	// read out the current sequence number, and the basis sequence number "tags"
	// the basis is our agreed upon state with the other end of the connection
	uint32 nBasisSeqTag = Frame_HeaderToID[normBytes[0]];
	if (nBasisSeqTag >= eH_TransportSeq0 && nBasisSeqTag <= eH_TransportSeq_Last)
		nBasisSeqTag -= eH_TransportSeq0;
	else if (nBasisSeqTag >= eH_SyncTransportSeq0 && nBasisSeqTag <= eH_SyncTransportSeq_Last)
	{
		NET_ASSERT(inSync);
		nBasisSeqTag -= eH_SyncTransportSeq0;
	}
	else
	{
		NET_ASSERT(false);
		return;
	}
	uint32 nCurrentSeqTag = UnseqBytes[normBytes[1]];

#if DEBUG_ENDPOINT_LOGIC
	if (m_log_input)
	{
		fprintf( m_log_input, "| got tags: %d, %d with seq %d\n", nCurrentSeqTag, nBasisSeqTag, m_nInputSeq );
		fflush( m_log_output );
	}
#endif

	nCurrentSeq = (m_nInputSeq & ~SequenceNumberMask) | nCurrentSeqTag;
	if (m_nInputSeq >= SequenceNumberRadius && nCurrentSeq < m_nInputSeq - SequenceNumberRadius)
		nCurrentSeq += SequenceNumberDiameter;
	else if (nCurrentSeq > m_nInputSeq + SequenceNumberRadius)
		nCurrentSeq -= SequenceNumberDiameter;
	nBasisSeq = nCurrentSeq - nBasisSeqTag - 1;
	//NetLog( "got tags: %.2x, %.2x with seq %.8x; => %.8x, %.8x\n", nCurrentSeqTag, nBasisSeqTag, m_nInputSeq, nCurrentSeq, nBasisSeq );

	//	NetLog("process packet (%d)", stm.GetBonus()%WHOLE_SEQ);

#if ENABLE_DEBUG_KIT
	SDebugPacket debugPacket( m_pParent->GetLoggingChannelID(), false, nCurrentSeq );
#endif

#if DEBUG_SEQUENCE_NUMBERS
	//DEBUG:
	uint32 nProperSeq = *(uint32*)(normBytes + pktLen + 2);
	NetLog( "got tags: %d, %d with seq %d; => %d, %d [should be %d]\n", nCurrentSeqTag, nBasisSeqTag, m_nInputSeq, nCurrentSeq, nBasisSeq, nProperSeq );
	NET_ASSERT( nCurrentSeq == nProperSeq );
#endif

	// check our location
	if (m_nInputSeq > nCurrentSeq)
	{
		m_nReordered ++;
		// already processed this packet... we can early out here
		MMM().FreeHdl(hdl);
		return;
	}

	if (nCurrentSeq > m_nInputSeq + WHOLE_SEQ)
	{
		NetWarning( "Sequence %d is way in the future (I'm only up to %d) - ignoring it", nCurrentSeq, m_nInputSeq );
		MMM().FreeHdl(hdl);
		return;
	}

	// maybe we can schedule this packet for later? - to reorder it and get
	// better packet loss rates
	if (!inSync && bQueueing && nCurrentSeq > m_nInputSeq)
	{
		if (m_incomingPackets.find(nCurrentSeq) != m_incomingPackets.end())
		{
			// already have this packet
			MMM().FreeHdl(hdl);
		}
		else
		{
			TQueuedIncomingPacket pkt;
			pkt.hdl = hdl;
			pkt.when = nTime;
			m_incomingPackets[nCurrentSeq] = pkt;
#if ENABLE_DEBUG_KIT
			if (CNetCVars::Get().EndpointPendingQueueLogging)
				NetLog( "Queued packet %d for later (as at %d)", nCurrentSeq, m_nInputSeq );
#endif
			m_nQueued ++;
		}
		// we'll process this later - in Update()
		return;
	}

	m_nReceived ++;

#if LOG_INCOMING_MESSAGES
	if (CNetCVars::Get().LogNetMessages & 2)
		NetLog( "INCOMING: frame #%d from %s", nCurrentSeq, m_pParent->GetName() );
#endif

	// get state structs
	CInputState& basis = m_vInputState[ nBasisSeq % WHOLE_SEQ ];
	CInputState& state = m_vInputState[ nCurrentSeq % WHOLE_SEQ ];

	if (basis.LastValid() != nBasisSeq)
	{
		NetWarning( "Ignoring bad basis state %d (still at %d) - possible malicious stream",
			nBasisSeq, basis.LastValid() );
		MMM().FreeHdl(hdl);
		return;
	}

	NET_ASSERT( nBasisSeq >= m_nLastBasisSeq );
	if (nBasisSeq < m_nLastBasisSeq)
	{
#if VERBOSE_MALFORMED_PACKET_REPORTS
		NetWarning("Ignoring old basis state %d (currently at %d)", nBasisSeq, m_nLastBasisSeq);
#endif
		MMM().FreeHdl(hdl);
		m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
		return;
	}

	for (uint32 i = m_nLastBasisSeq; i < nBasisSeq; i++)
	{
		m_vInputState[i % WHOLE_SEQ].FreeState(&m_bigStateMgrInput);
	}
	m_nLastBasisSeq = nBasisSeq;

#if DEBUG_ENDPOINT_LOGIC
	DumpInputState( nTime );
#endif

	// final setup of the state for decoding
	state.Clone( &m_bigStateMgrInput, basis, nCurrentSeq );
	// decrypt
	state.Decrypt( normBytes+2, pktLen );

	uint8 hash = 0;
	for (int i=0; i<pktLen; i++)
		hash = 5*hash + normBytes[i];
	if ((hash ^ normBytes[pktLen]) != normBytes[pktLen+1])
	{
#if VERBOSE_MALFORMED_PACKET_REPORTS
		NetWarning("Packet early hash mismatch (%.2x ^ %.2x -> %.2x != %.2x)", hash, normBytes[pktLen], hash ^ normBytes[pktLen], normBytes[pktLen+1]);
#endif
		m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
		MMM().FreeHdl(hdl);
		return;
	}

	// schedule acks and nacks
	while (m_nInputSeq < nCurrentSeq)
	{
		if (CVARS.LogLevel > 1)
			NetWarning("!!!!! On receiving packet %d, input seq was %d, causing a nack to be sent (basis state %d)", nCurrentSeq, m_nInputSeq, nBasisSeq);
		NET_ASSERT( m_nInputSeq == m_nFrontAck + m_dqAcks.size() );
		m_dqAcks.push_back( SAckData(false,false,nTime) );
		m_nInputSeq ++;
		m_nDropped ++;
		// dont allow connection to idle here
		m_dqAcks.back().bHadUrgentData = true;
		m_nUrgentAcks ++;
	}
	NET_ASSERT( m_nInputSeq == m_nFrontAck + m_dqAcks.size() );
	m_dqAcks.push_back( SAckData(true,false,nTime) );
	m_nInputSeq ++;

	m_PacketRateCalculator.GotPacket( nTime, MMM().GetHdlSize(hdl) );

	CNetInputSerializeImpl stmImpl( normBytes+1, pktLen-1, m_pParent );
	CSimpleSerialize<CNetInputSerializeImpl> stm(stmImpl);

	((CArithModel*)state.GetArithModel())->SetNetContextState( m_pParent->GetContextView()->ContextState() );
	stmImpl.SetArithModel(state.GetArithModel());

	//
	// read acks & nacks
	//

	bool bAck;
	uint32 nAckSeq;
	while (state.ReadAck( stmImpl.GetInput(), bAck, nAckSeq ))
	{
		if (!AckPacket( nTime, nAckSeq, bAck ))
		{
			MMM().FreeHdl(hdl);
			return;
		}
	}

	//
	// validate signing
	//
	uint8 signingKey;
	signingKey = stmImpl.GetInput().ReadBits(8);
	if (signingKey != normBytes[pktLen])
	{
#if VERBOSE_MALFORMED_PACKET_REPORTS
		NetWarning("Packet middle hash mismatch (%.2x != %.2x)", signingKey, normBytes[pktLen]);
#endif
		m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
		return;
	}

	//
	// read message stream
	//

	bool bCompletedParse = false;

	// it is possible for an infinite loop to occur here; a malicious packet could
	// be encoded in such a way that eIM_EndOfStream can never be found.
	// we prevent this by enforcing a maximum number of iterations through the loop;
	int nMessages = 0;
	while (!bCompletedParse && nMessages < MaxMessagesPerPacket)
	{
		if (m_emptyMode || m_pParent->IsDead())
			break;

		FRAME_PROFILER("CCTPEndpoint:ProcessPacket_OneMessage", GetISystem(), PROFILE_NETWORK);
		// read a message id and process it - we have some internal messages
		// handled by a switch, and anything higher-level is dispatched through
		// our message mapper
		size_t msg = state.ReadMsgId( stmImpl.GetInput() );
		nMessages ++;

		// we must ensure that the memento streams are NULL at this point
		// (maybe we need only do this in debug?)
		stmImpl.SetMementoStreams(NULL, NULL, 0, false);

		switch (msg)
		{
		case eIM_EndOfStream:
			// Arithmetic compression => we need an end-of-stream marker to know
			// when the stream is really complete
			bCompletedParse = true;
			m_dqAcks.back().bHadUrgentData = (stmImpl.GetInput().ReadBits(1) != 0);
			m_nUrgentAcks += m_dqAcks.back().bHadUrgentData;
			// final check
#if CRC8_ENCODING_GLOBAL
			{
				uint8 crcStream = stmImpl.GetInput().GetCRC();
				uint8 crcWritten = stmImpl.GetInput().ReadBits(8);
				if (crcStream != crcWritten)
				{
					NetWarning("Packet crc mismatch %.2x != %.2x", crcStream, crcWritten);
					m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
				}
			}
#endif
			signingKey = stmImpl.GetInput().ReadBits(8);
			if (signingKey != (normBytes[pktLen]^0xff))
			{
#if VERBOSE_MALFORMED_PACKET_REPORTS
				NetWarning("Packet end hash mismatch (%.2x != %.2x ^ ff -> %.2x)", signingKey, normBytes[pktLen], normBytes[pktLen]^0xff);
#endif
				m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
				return;
			}
			break;
		default:
			{
				ENSURE_REALTIME;

				m_dqAcks.back().bHadData = true;

				size_t nSink;
				const SNetMessageDef * pDef = m_InputMapper.GetDispatchInfo( msg, &nSink );
				SProcessingMessage processingMessage(pDef);

#if MESSAGE_BACKTRACE_LENGTH > 0
				if (m_previouslyReceivedMessages.Full())
					m_previouslyReceivedMessages.Pop();
				m_previouslyReceivedMessages.Push(pDef);
#endif

#if LOG_INCOMING_MESSAGES
				if (CNetCVars::Get().LogNetMessages & 1)
					NetLog("INCOMING: %s [id=%u;state=%d]", pDef->description, (unsigned)msg, m_pParent->GetContextViewState());
#endif
				if (pDef->CheckParallelFlag(eMPF_DecodeInSync) && !inSync)
				{
					char buffer[256];
					sprintf(buffer, "received %s without having the packet marked for synchronous processing", pDef->description);
					m_pParent->Disconnect(eDC_ProtocolError, buffer);
					MMM().FreeHdl(hdl);
					return;
				}
				INetMessageSink * pSink = m_MessageSinks[nSink].pSink;

				EntityId entityId = m_entityId; // make sure internal structures don't get broken, entityId is passed by reference!
				TNetMessageCallbackResult r = pDef->handler( pDef->nUser, pSink, TSerialize( &stm ), nCurrentSeq, nBasisSeq, &entityId, m_pParent );

#if CRC8_ENCODING_MESSAGE
				{
					uint8 crcStream = stmImpl.GetInput().GetCRC();
					uint8 crcWritten = stmImpl.GetInput().ReadBits(8);
					if (crcStream != crcWritten)
					{
						NetWarning("Message crc mismatch %.2x != %.2x decoding %s", crcStream, crcWritten, GetCurrentProcessingMessageDescription());
						m_pParent->Disconnect(eDC_ProtocolError, "Malformed packet");
					}
				}
#endif

				if (!r.first)
				{
					if (r.second)
						r.second->DeleteThis();
					NetLog( "Protocol error handling message %s", pDef->description );
					m_pParent->Disconnect( eDC_ProtocolError, pDef->description );
					bCompletedParse = true;
				}
				else if (r.second)
				{
					if (inSync)
					{
						r.second->Sync();
						r.second->DeleteThis();
					}
					else
					{
						if (!entityId && !m_entityId && pDef->CheckParallelFlag(eMPF_DiscardIfNoEntity))
						{
							NetLog("%s message with no entity id; discarding", pDef->description);
							r.second->DeleteThis();
						}
						else if (pDef->CheckParallelFlag(eMPF_BlocksStateChange))
						{
							m_nStateBlockers ++;
							TO_GAME( r.second, m_pParent, m_pParent, &CNetChannel::UnblockMessages );
						}
						else
						{
							TO_GAME( r.second, m_pParent );
						}
					}
				}
			}
		}
	}
	if (nMessages >= MaxMessagesPerPacket)
	{
		NetWarning( "Too many messages in packet (will disconnect now)" );
		m_pParent->Disconnect( eDC_ProtocolError, "Too many messages in packet" );
	}

#if ENABLE_DEBUG_KIT
	debugPacket.Sent();
#endif

	MMM().FreeHdl(hdl);
}

// this class is for collecting regular messages
class CCTPEndpoint::CMessageSender : /*public INetMessageSender,*/ public IMessageOutput, public INetSender
{
public:
	ILINE CMessageSender( CCTPEndpoint * pEndpoint, COutputState& State, 
		size_t nSize, int& nMessages, CStatsCollector * pStats, 
		INetChannel * pChannel, CTimeValue now ) :
		INetSender(TSerialize(&pEndpoint->m_outputStream), pEndpoint->m_nOutputSeq, pEndpoint->m_nInputAck, pEndpoint->m_pParent->IsServer()),
		m_State(State),
		m_nSize(nSize),
		m_nMessages(nMessages),
		m_pStats(pStats),
		m_pEndpoint(pEndpoint),
		m_pChannel(pChannel),
		m_inWrite(false),
		m_now(now),
		m_bHadUrgentData(false),
		m_bIsCorrupt(false),
		m_brackets(((CNetChannel*)pChannel)->GetUpdateMessageBrackets())
	{
	}

	bool HadUrgentData() const
	{
		return m_bHadUrgentData;
	}

	void BeginMessage( const SNetMessageDef * pDef )
	{
		if (!m_updatingId && m_lastUpdatingId)
		{
			BeginMessage_Primitive(m_brackets.second);
			m_lastUpdatingId = SNetObjectID();
		}
		BeginMessage_Primitive(pDef);
	}

	void BeginUpdateMessage( SNetObjectID id )
	{
		NET_ASSERT(id);
		if (!m_updatingId && m_lastUpdatingId == id)
		{
			m_updatingId = id;
		}
		else
		{
			if (m_lastUpdatingId)
				BeginMessage_Primitive( m_brackets.second );
			m_updatingId = m_lastUpdatingId = id;
			BeginMessage_Primitive(m_brackets.first);
			ser.Value("netID", id, 'eid');
		}
	}

	void EndUpdateMessage()
	{
		NET_ASSERT(m_updatingId);
		NET_ASSERT(m_updatingId == m_lastUpdatingId);
		m_updatingId = SNetObjectID();
	}

	void BeginMessage_Primitive( const SNetMessageDef * pDef )
	{
		uint32 id = m_pEndpoint->m_OutputMapper.GetMsgId( pDef );
#if LOG_OUTGOING_MESSAGES
		if (CNetCVars::Get().LogNetMessages & 4)
			NetLog("OUTGOING: %s [id=%u;state=%d]", pDef->description, (unsigned)id, m_pEndpoint->m_pParent->GetContextViewState());
#endif
#if CRC8_ENCODING_MESSAGE
		if (m_nMessages)
			m_pEndpoint->m_outputStreamImpl.GetOutput().WriteBits( m_pEndpoint->m_outputStreamImpl.GetOutput().GetCRC(), 8 );
#endif
		if (!m_State.WriteMsgId( m_pEndpoint->m_outputStreamImpl.GetOutput(), id, m_pStats, pDef->description ))
		{
			NetWarning("Sending %s fails - no state", pDef->description);
			return;
		}
#if MESSAGE_BACKTRACE_LENGTH > 0
		if (m_pEndpoint->m_previouslySentMessages.Full())
			m_pEndpoint->m_previouslySentMessages.Pop();
		m_pEndpoint->m_previouslySentMessages.Push(pDef);
#endif
		m_nMessages ++;
		m_nMessagesInBlock ++;
		m_pEndpoint->m_outputStreamImpl.SetMementoStreams(NULL, NULL, 0, false);

		if (pDef->CheckParallelFlag(eMPF_DecodeInSync))
			m_bWritingPacketNeedsInSyncProcessing = true;
		m_nStateBlockers += pDef->CheckParallelFlag(eMPF_BlocksStateChange);
	}

	EMessageSendResult HeaderBody(void * p)
	{
		return m_pEndpoint->m_pParent->WriteHeader(this);
	}

	EMessageSendResult FooterBody(void * p)
	{
		return m_pEndpoint->m_pParent->WriteFooter(this);
	}

	EMessageSendResult MessageBody(void * p)
	{
		return static_cast<SSentMessage*>(p)->pSendable->Send( this );
	}

	const char * GetName()
	{
		return m_pEndpoint->m_pParent->GetName();
	}

	EWriteMessageResult TranslateResult( EMessageSendResult res )
	{
		EWriteMessageResult out = eWMR_Fail_Finish;

		while (true)
		{
			switch (res)
			{
			case eMSR_SentOk:
				out = eWMR_Ok_Continue;
				break;
			case eMSR_NotReady:
				if (m_nMessagesInBlock)
				{
					NetWarning("Message declared not-ready, but data was sent; upgrading to a connection failure");
					res = eMSR_FailedConnection;
					continue;
				}
				out = eWMR_Delay;
				break;
			case eMSR_FailedMessage:
				if (m_nMessagesInBlock)
				{
					NetWarning("Message declared failed, but data was sent; upgrading to a packet failure");
					res = eMSR_FailedPacket;
					continue;
				}
				out = eWMR_Fail_Continue;
				break;
			case eMSR_FailedPacket:
				out = eWMR_Fail_Finish;
				m_bIsCorrupt = true;
				break;
			case eMSR_FailedConnection:
				m_pEndpoint->m_pParent->Disconnect(eDC_ProtocolError, "Message send failed");
				m_bIsCorrupt = true;
				out = eWMR_Fail_Finish;
				break;
			}

			break;
		}
		if (out == eWMR_Ok_Continue || out == eWMR_Fail_Continue)
		{
			bool cont = m_pEndpoint->m_outputStreamImpl.GetOutput().GetApproximateSize() < m_nSize;
			cont &= m_nMessages < MaxMessagesPerPacket;

			if (!cont)
			{
				if (out == eWMR_Ok_Continue)
					out = eWMR_Ok_Finish;
				else
					out = eWMR_Fail_Finish;
			}
		}

		return out;
	}

	ILINE EWriteMessageResult DoWrite( EMessageSendResult (CMessageSender::*sendBody)(void*), void * pUser )
	{
		NET_ASSERT(!m_inWrite);
		m_inWrite = true;
		m_bWritingPacketNeedsInSyncProcessing = false;
		m_nStateBlockers = 0;
		m_nMessagesInBlock = 0;
		EMessageSendResult res = (this->*sendBody)(pUser);

		m_inWrite = false;

		return TranslateResult(res);
	}
	
	EWriteMessageResult WriteHeader()
	{
		return DoWrite( &CMessageSender::HeaderBody, NULL );
	}

	EWriteMessageResult WriteFooter()
	{
		NET_ASSERT(!m_updatingId);
		if (m_lastUpdatingId)
		{
			BeginMessage_Primitive(m_brackets.second);
			m_lastUpdatingId = SNetObjectID();
		}

		return DoWrite( &CMessageSender::FooterBody, NULL );
	}

	EWriteMessageResult WriteMessage( SSentMessage& msg, SSendableHandle hdl )
	{
		if (!msg.pSendable->CheckParallelFlag(eMPF_DontAwake))
			m_bHadUrgentData = true;

		EWriteMessageResult out = DoWrite( &CMessageSender::MessageBody, &msg );

		if (out == eWMR_Ok_Continue || out == eWMR_Ok_Finish)
		{
			m_State.SentMessage( *m_pEndpoint, hdl, m_nStateBlockers );
			m_pEndpoint->m_nStateBlockers += m_nStateBlockers;
			m_pEndpoint->m_bWritingPacketNeedsInSyncProcessing |= m_bWritingPacketNeedsInSyncProcessing;
		}

		/*
		if (CVARS.NetInspector)
		{
			NET_INSPECTOR.AddMessage(msg.pSendable->GetDescription(), RESOLVER.ToString(m_pEndpoint->m_pParent->GetIP()).c_str(),
				(GetStream()->GetBitSize() - sizeBefore)/8.0f);
		}
		*/

		return out;
	}

	CCommOutputStream * GetStream()
	{
		return &m_pEndpoint->m_outputStreamImpl.GetOutput();
	}

	INetChannel * GetChannel()
	{
		return m_pChannel;
	}

	size_t CurrentSizeEstimation()
	{
		return m_pEndpoint->m_outputStreamImpl.GetOutput().GetApproximateSize();
	}

	bool IsCorrupt()
	{
		return m_bIsCorrupt;
	}

private:
	CCTPEndpoint * m_pEndpoint;
	COutputState& m_State;
	size_t m_nSize;
	int& m_nMessages;
	CStatsCollector * m_pStats;
	INetChannel * m_pChannel;
	CTimeValue m_now;

	SNetObjectID m_updatingId;
	SNetObjectID m_lastUpdatingId;

	TUpdateMessageBrackets m_brackets;

	// per-message-chain attributes
	bool m_inWrite;
	bool m_bWritingPacketNeedsInSyncProcessing;
	int m_nStateBlockers;
	int m_nMessagesInBlock;

	bool m_bHadUrgentData;
	bool m_bIsCorrupt;
};

uint32 CCTPEndpoint::SendPacket( CTimeValue nTime, const SSendPacketParams& params )
{
	ASSERT_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

	const bool bAnyData = 
		params.bForce ||
		!m_queue.IsEmpty() ||
		!m_dqAcks.empty()
		;

	if (!bAnyData)
		return 0;

	m_bWritingPacketNeedsInSyncProcessing = false;

//	NET_ASSERT( params.nSize <= MaxTargetSize );
//	uint8 buffer[MaxOutputSize];
//	memset(buffer,0,MaxOutputSize);

	FRAME_PROFILER( "CCTPEndpoint:SendPacket", GetISystem(), PROFILE_NETWORK );

	//
	// update output state
	//

	m_nOutputSeq++;

	m_outputStreamImpl.ResetLogging();

	STATS.BeginPacket( m_pParent->GetIP(), nTime, m_nOutputSeq );

	// remove any old states from our state cache
	COutputState& basis = m_vOutputState[ m_nInputAck % WHOLE_SEQ ];
	COutputState& state = m_vOutputState[ m_nOutputSeq % WHOLE_SEQ ];
	state.Clone( &m_bigStateMgrOutput, basis );

	// and remove any old acks from our ack cache
	while (m_nFrontAck <= basis.GetNumberOfAckedPackets())
	{
		NET_ASSERT( !m_dqAcks.empty() );
		m_nFrontAck++;
		SAckData& ack = m_dqAcks.front();
		m_nUrgentAcks -= ack.bHadUrgentData;
		m_dqAcks.pop_front();
	}

	//
	// send sequence data
	//

	NET_ASSERT( m_nOutputSeq > m_nInputAck );
	NET_ASSERT( m_nOutputSeq - m_nInputAck <= WHOLE_SEQ );
	// create the stream, with the bonus (first) byte specifying it's a ctp frame,
	// and our basis offset, and the second byte indicating the current sequence number obfuscated a little
	uint8 nBasisSeqTag = m_nOutputSeq-m_nInputAck-1;
	m_assemblyBuffer[0] = Frame_IDToHeader[eH_TransportSeq0+nBasisSeqTag];
	m_outputStreamImpl.GetOutput().Reset( SeqBytes[m_nOutputSeq&SequenceNumberMask] );

	//NetLog("Send packet %.8x basis %.8x => tags %.2x, %.2x", m_nOutputSeq, m_nInputAck, nBasisSeqTag, m_nOutputSeq&SequenceNumberMask);

#if LOG_OUTGOING_MESSAGES
	if (CNetCVars::Get().LogNetMessages & 8)
		NetLog( "OUTGOING: frame #%d to %s", m_nOutputSeq, m_pParent->GetName() );
#endif

#if ENABLE_DEBUG_KIT
	SDebugPacket debugPacket( -(int)m_pParent->GetLoggingChannelID(), true, m_nOutputSeq );
#endif

	state.GetArithModel()->SetNetContextState( m_pParent->GetContextView()->ContextState() );
	m_outputStreamImpl.SetArithModel( state.GetArithModel() );

#if DEBUG_SEQUENCE_NUMBERS
	char debugBuffer[512];
	int pos = 0;
	for (size_t i = m_nInputAck; (i%WHOLE_SEQ) != ((m_nInputAck-1)%WHOLE_SEQ); i++)
	{
		if (i == m_nOutputSeq)
			pos += sprintf( debugBuffer+pos, "|" );
		pos += sprintf( debugBuffer+pos, "%d", m_vOutputState[i%WHOLE_SEQ].IsAvailable() );
	}
	NetLog( "SENDSEQ[%p]: %s", this, debugBuffer );
	NetLog( "send tags: %d, %d for %d, %d", nBasisSeqTag, m_nOutputSeq&0xff, m_nOutputSeq, m_nInputAck );
#endif

	NET_ASSERT( !m_vOutputState[m_nInputAck%WHOLE_SEQ].IsAvailable() );

	int nMessages = 0;

	//
	// send acknowledgement data
	//

	for (TAckDeque::const_iterator pb = m_dqAcks.begin(); pb != m_dqAcks.end(); ++pb)
		state.WriteAck( m_outputStreamImpl.GetOutput(), pb->bReceived, &STATS );
	state.WriteEndAcks( m_outputStreamImpl.GetOutput(), &STATS );

	//
	// signing key
	//

	uint8 signingKey = (cry_rand32() >> 6) & 0xff;
	m_outputStreamImpl.GetOutput().WriteBits(signingKey, 8);

	//
	// send message stream
	//

	SSchedulingParams schedParams;
	schedParams.now = nTime;
	schedParams.next = nTime + 0.1f;
	schedParams.nSeq = m_nOutputSeq;
	schedParams.targetBytes = params.nSize;
	schedParams.transportLatency = m_PacketRateCalculator.GetPing(true) * 0.5f;
	schedParams.haveWitnessPosition = m_pParent->GetWitnessPosition( schedParams.witnessPosition );
	schedParams.haveWitnessDirection = m_pParent->GetWitnessDirection( schedParams.witnessDirection );
	schedParams.haveWitnessFov = m_pParent->GetWitnessFov( schedParams.witnessFov );
	schedParams.pChannel = m_pParent;
	CMessageSender sender( this, state, params.nSize, nMessages, &STATS, m_pParent, nTime );
	bool actuallySend = m_queue.BuildPacket( &sender, schedParams );
	actuallySend &= !sender.IsCorrupt();

	uint32 nSent = 0;
	// make sure we write the terminating data (needed for the arithmetic compression)
#if CRC8_ENCODING_MESSAGE
	if (nMessages)
		m_outputStreamImpl.GetOutput().WriteBits( m_outputStreamImpl.GetOutput().GetCRC(), 8 );
#endif
	if (!state.WriteMsgId( m_outputStreamImpl.GetOutput(), eIM_EndOfStream, &STATS, "CCTPEndpoint:EndOfStream" ))
	{
		if (CNetCVars::Get().LogLevel > 0)
			NetWarning("Failed to terminate packet; aborting send");
		m_pParent->Disconnect(eDC_ProtocolError, "Failed to terminate packet");
		return 0;
	}
	m_outputStreamImpl.GetOutput().WriteBits(sender.HadUrgentData(), 1);
#if CRC8_ENCODING_GLOBAL
	m_outputStreamImpl.GetOutput().WriteBits(m_outputStreamImpl.GetOutput().GetCRC(), 8);
#endif
	m_outputStreamImpl.GetOutput().WriteBits(signingKey^0xff, 8);

	// final tidying 

	state.Unavailable();

	nSent = uint32(m_outputStreamImpl.GetOutput().Flush());

	if (m_bWritingPacketNeedsInSyncProcessing)
	{
		uint8& hdr = m_assemblyBuffer[0];
		hdr = Frame_IDToHeader[Frame_HeaderToID[hdr]+eH_SyncTransportSeq0-eH_TransportSeq0];
	}

#if ENABLE_DEBUG_KIT
	CAutoCorruptAndRestore acr(m_assemblyBuffer+1, nSent, CVARS.RandomPacketCorruption == 2);
#endif

	// sign
	int nBig = nSent + 1 + 2; // one byte for the header that the range encoder needs, 2 bytes for signing
	while ((nBig-2) & 15)
		nBig++;
	m_outputStreamImpl.GetOutput().PutZeros(nBig - nSent - 1);
	uint8 * normBytes = m_assemblyBuffer;
	uint8 * endBytes = normBytes + nBig - 2;
	endBytes[0] = signingKey;
	uint8 hash = 0;
	for (int i=0; i<nBig-2; i++)
		hash = 5*hash + normBytes[i];
	endBytes[1] = hash ^ signingKey;

	state.Encrypt(normBytes+2, nBig-2);
	STATS.EndPacket( nBig );

	NET_ASSERT(0==((nBig-2)&15));
#if DEBUG_SEQUENCE_NUMBERS
	*(uint32*)(normBytes + nBig) = m_nOutputSeq;
#endif
//	DumpBytes( normBytes, nBig + 4*DEBUG_SEQUENCE_NUMBERS );
	if (actuallySend)
		m_pParent->Send( normBytes, nBig + 4*DEBUG_SEQUENCE_NUMBERS );
	else
		NetWarning("Refusing to send corrupted packet");
	m_assemblySize = nBig + 4*DEBUG_SEQUENCE_NUMBERS;
	m_PacketRateCalculator.SentPacket( nTime, m_nOutputSeq, nBig );
#if ENABLE_DEBUG_KIT
	debugPacket.Sent();
#endif

	m_nSent ++;

	return nSent;
}

bool CCTPEndpoint::LookupMessage( const char * name, SNetMessageDef const ** ppDef, INetMessageSink ** ppSink )
{
	bool result;
	size_t nProto;
	if (result = m_InputMapper.LookupMessage( name, ppDef, &nProto ))
		*ppSink = m_MessageSinks[nProto].pSink;
	return result;
}

void CCTPEndpoint::BroadcastMessage( const SCTPEndpointEvent& evt )
{
	for (std::vector<SListener>::iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		if (iter->eventMask & evt.event)
			iter->pListener->OnEndpointEvent( evt );
	}
}

void CCTPEndpoint::ChangeSubscription( ICTPEndpointListener * pListener, uint32 eventMask )
{
	for (std::vector<SListener>::iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		if (iter->pListener == pListener)
		{
			if (eventMask)
				iter->eventMask = eventMask;
			else
				m_listeners.erase(iter);
			return;
		}
	}
	SListener listener;
	listener.pListener = pListener;
	listener.eventMask = eventMask;
	m_listeners.push_back(listener);
}

void CCTPEndpoint::SchedulerDebugDraw()
{
	float white[] = {1,1,1,1};
	gEnv->pRenderer->Draw2dLabel( 300, 10, 1, white, false, "%d %d %d %f", m_nOutputSeq, m_nInputAck, m_nInputSeq, m_PacketRateCalculator.GetPing(false) );

	SSchedulingParams schedParams;
	schedParams.now = g_time;
	schedParams.next = schedParams.now + 0.1f;
	schedParams.nSeq = m_nOutputSeq;
	int age = m_nOutputSeq - m_nInputAck;
	schedParams.targetBytes = m_PacketRateCalculator.GetMaxPacketSize( schedParams.now );
	schedParams.transportLatency = m_PacketRateCalculator.GetPing(true) * 0.5f;
	schedParams.haveWitnessPosition = m_pParent->GetWitnessPosition( schedParams.witnessPosition );
	schedParams.haveWitnessDirection = m_pParent->GetWitnessDirection( schedParams.witnessDirection );
	schedParams.haveWitnessFov = m_pParent->GetWitnessFov( schedParams.witnessFov );
	schedParams.pChannel = m_pParent;
	m_queue.DebugDraw(schedParams);
}

void CCTPEndpoint::ChannelStatsDraw()
{
	ITextModeConsole * pTMC = gEnv->pSystem->GetITextModeConsole();
	static int y = 0;
	static int yframe = -1;
	float white[4] = {1,1,1,1};
	int frame = gEnv->pRenderer->GetFrameID();
	if (frame != yframe)
	{
		                  /*01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789*/
		const char * hdr = "   ping   smooth tcprate       bwin     bwout    pktrate cur     loss% maxsz pktsz idle katime/kftime dctime botime";
		if (pTMC) pTMC->PutText(0,0,hdr);
		gEnv->pRenderer->Draw2dLabel( 10, 50, 1, white, false, "%s", hdr );
		yframe = frame;
		y = 1;
	}
	char buf[512];
	uint16 maxsz = m_PacketRateCalculator.GetMaxPacketSize(g_time);
	if (pTMC) pTMC->PutText(0,y,m_pParent->GetName());
	gEnv->pRenderer->Draw2dLabel( 10, (y++)*10+50, 1, white, false, m_pParent->GetName() );
	CTimeValue backoffTime;
	typedef CryFixedStringT<64> TBOStr;
	TBOStr backoffTimeStr = "      ";
	bool backingOff;
	if (backingOff = GetBackoffTime(backoffTime, true))
		backoffTimeStr.Format("%4.2f/", backoffTime.GetMilliSeconds());
	else
		backoffTimeStr = "../";
	if (GetBackoffTime(backoffTime, false))
		backoffTimeStr += TBOStr().Format("%4.2f", backoffTime.GetMilliSeconds());
	else
		backoffTimeStr += "..";
	sprintf(buf, "   %6.1f %6.1f %13.2f %8d %8d %7.3f %7.3f %5.1f %5d %5d %s %6.2f/%6.2f %6.2f %s", 
		m_PacketRateCalculator.GetPing(false), 
		m_PacketRateCalculator.GetPing(true),
		m_PacketRateCalculator.GetTcpFriendlyBitRate(),
		m_PacketRateCalculator.GetBandwidthUsage(g_time, true),
		m_PacketRateCalculator.GetBandwidthUsage(g_time, false),
		m_PacketRateCalculator.GetPacketRate(IsIdle(), g_time),
		m_PacketRateCalculator.GetCurrentPacketRate(g_time),
		m_PacketRateCalculator.GetPacketLossPerPacketSent(g_time) * 100.0f,
		maxsz,
		m_PacketRateCalculator.GetIdealPacketSize(m_nOutputSeq - m_nInputAck, IsIdle(), maxsz),
		IsIdle()? "yes " : "no  ",
		m_pParent->GetIdleTime(true).GetSeconds(),
		m_pParent->GetIdleTime(false).GetSeconds(),
		m_pParent->GetInactivityTimeout(backingOff).GetSeconds(),
		backoffTimeStr.c_str()
		);
	if (pTMC) pTMC->PutText(0,y,buf);
	gEnv->pRenderer->Draw2dLabel( 10, (y++)*10+50, 1, white, false, buf );

	if (CNetCVars::Get().ChannelStats > 1)
	{
		sprintf(buf, "rcvd:%d sent:%d dropped:%d reord:%d queued:%d rptsend:%d  pq to:%d rdy:%d urgack:%d", 
			m_nReceived, m_nSent, m_nDropped, m_nReordered, m_nQueued, m_nRepeated, m_nPQTimeout, m_nPQReady, m_nUrgentAcks);
		if (pTMC) pTMC->PutText(0,y,buf);
		gEnv->pRenderer->Draw2dLabel( 10, (y++)*10+50, 1, white, false, buf );
	}
}

void CCTPEndpoint::SetAfterSpawning( bool afterSpawning )
{
	if (afterSpawning)
		m_queue.SetFlags( m_queue.GetFlags() | eMQF_IsAfterSpawning );
	else
		m_queue.SetFlags( m_queue.GetFlags() & ~eMQF_IsAfterSpawning );
}

void CCTPEndpoint::SentElem( uint32& head, const SSendableHandle& hdl )
{
	ASSERT_GLOBAL_LOCK;
	uint32 cur;
	if (m_freeSentElem == InvalidSentElem)
	{
		cur = m_sentElems.size();
		m_sentElems.push_back(SSentElem());
	}
	else
	{
		cur = m_freeSentElem;
		m_freeSentElem = m_sentElems[cur].next;
	}
	NET_ASSERT(!m_sentElems[cur].hdl);
	m_sentElems[cur].hdl = hdl;
	m_sentElems[cur].next = head;
	head = cur;
}

void CCTPEndpoint::ClearSentElems( uint32& head, std::vector<SSendableHandle>* pOut )
{
	uint32 cur = head;
	while (cur != InvalidSentElem)
	{
		SSentElem& elem = m_sentElems[cur];
		if (pOut)
			pOut->push_back(elem.hdl);
		elem.hdl = SSendableHandle();
		uint32 next = elem.next;
		elem.next = m_freeSentElem;
		m_freeSentElem = cur;
		cur = next;
	}
	head = InvalidSentElem;
}

void CCTPEndpoint::AckMessages( uint32& head, uint32 nSeq, bool ack, bool clear )
{
	static std::vector<SSendableHandle> temp;
	ClearSentElems( head, &temp );
	if (!temp.empty())
	{
		m_queue.AckMessages( &temp[0], temp.size(), nSeq, ack, clear );
		temp.resize(0);
	}
}

bool CCTPEndpoint::IsIdle()
{
	return m_nUrgentAcks==0 && m_queue.IsIdle() && m_pParent->IsIdle();
}

const char * CCTPEndpoint::GetCurrentProcessingMessageDescription()
{
	if (g_processingMessage)
		return g_processingMessage->description;
	else
		return "no-message";
}

void CCTPEndpoint::BackOff()
{
	if (m_receivedPacketSinceBackoff)
	{
		m_receivedPacketSinceBackoff = false;
		m_backoffTimer = g_time;
	}
}

bool CCTPEndpoint::GetBackoffTime( CTimeValue& tm, bool total )
{
	tm = g_time - m_backoffTimer;
	float timeSince = tm.GetSeconds();
	if (!m_receivedPacketSinceBackoff)
	{
		if (!total && timeSince > 5.0f)
		{
			BroadcastMessage(SCTPEndpointEvent(eCEE_BackoffTooLong));			
			return false;
		}
		if (!total)
			tm = 0.2f;
		return true;
	}
	if (timeSince > 0.1f)
		return false;
	if (!total)
		tm = 0.2f;
	return true;
}
