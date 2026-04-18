#ifndef __NETCVARS_H__
#define __NETCVARS_H__

#pragma once

#include "IConsole.h"
#include "Config.h"
#include "TimeValue.h"

class CNetCVars
{
public:
	int CPU;
	int LogLevel;
	int CheatProtectionLevel;
	int EnableVoiceChat;
	int ChannelStats;
	float BandwidthAggressiveness;
	float NetworkConnectivityDetectionInterval; // disabled if less than 1.0f
	float InactivityTimeout;
	int BackoffTimeout;
	int MaxMemoryUsage;
	int EnableVoiceGroups;
	int RTTConverge;
	int EnableTFRC;

#if NET_ASSERT_LOGGING
	int AssertLogging;
#endif

#if LOG_MESSAGE_DROPS
	int LogDroppedMessagesVar;
	bool LogDroppedMessages()
	{
		return LogDroppedMessagesVar || LogLevel;
	}
#endif

#if ENABLE_DEBUG_KIT
	int NetInspector;
	int MemInfo;
	int EntityDebug;
	int UseCompression;
	int ShowObjLocks;
	int DebugObjUpdates;
	int LogComments;
	int DisableUpdateDisable;
	int EndpointPendingQueueLogging;
	int DisconnectOnUncollectedBreakage;
	int DebugConnectionState;
	int RandomPacketCorruption; // 0 - disabled; 1 - UDP level (final stage); 2 - CTPEndpoint (before signing and encryption)
	//float WMICheckInterval;
	int PerfCounters;
	int GSDebugOutput;
#endif

	int VoiceLeadPackets;
	int VoiceTrailPackets;
	float VoiceProximity;
	int VoiceAverageBitRate;

#if !ALWAYS_CHECK_CD_KEYS
  int CheckCDKeys;
#endif

	ICVar* StatsLogin;
	ICVar* StatsPassword;

  int RankedServer;

  int LanScanPortFirst;
  int LanScanPortNum;

	CTimeValue StallEndTime;

#if INTERNET_SIMULATOR
	float PacketLossRate;
	float PacketExtraLag;

	static void OnPacketLossRateChange(ICVar* NewLossRate)
	{
		if ( NewLossRate->GetFVal() < 0.0f )
			NewLossRate->Set(0.0f);
		else if ( NewLossRate->GetFVal() > 1.0f )
			NewLossRate->Set(1.0f);
	}

	static void OnPacketExtraLagChange(ICVar* NewExtraLag)
	{
		if ( NewExtraLag->GetFVal() < 0.0f )
			NewExtraLag->Set(0.0f);
		else if ( NewExtraLag->GetFVal() > 100.0f )
			NewExtraLag->Set(100.0f);
	}
#endif

	float HighLatencyThreshold; // disabled if less than 0.0f
	float HighLatencyTimeLimit;

#if ENABLE_DEBUG_KIT
	int VisWindow;
	int VisMode;
	int ShowPing;
#endif

	ICVar * pVoiceCodec;
  ICVar * pSchedulerDebug;

#if LOG_INCOMING_MESSAGES || LOG_OUTGOING_MESSAGES
	int LogNetMessages;
#endif
#if LOG_BUFFER_UPDATES
	int LogBufferUpdates;
#endif
#if STATS_COLLECTOR_INTERACTIVE
	int ShowDataBits;
#endif

	static ILINE CNetCVars& Get()
	{
		NET_ASSERT(s_pThis);
		return *s_pThis;
	}

private:
	friend class CNetwork; // Our only creator

	CNetCVars(); // singleton stuff
	~CNetCVars();
	CNetCVars(const CNetCVars&);
	CNetCVars& operator= (const CNetCVars&);

	static CNetCVars* s_pThis;

	static void ReloadScheduler( IConsoleCmdArgs* );
	static void DumpObjectState( IConsoleCmdArgs* );
	static void DumpBlockingRMIs( IConsoleCmdArgs* );
	static void Stall( IConsoleCmdArgs* );
  static void SetCDKey( IConsoleCmdArgs* );
};

#endif
