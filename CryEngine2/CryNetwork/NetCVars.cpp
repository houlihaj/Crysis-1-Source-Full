#include "StdAfx.h"
#include "NetCVars.h"
#include "Network.h"

CNetCVars * CNetCVars::s_pThis;

CNetCVars::CNetCVars()
{
	memset(this, 0, sizeof(*this));

	NET_ASSERT(!s_pThis);
	s_pThis = this;

	IConsole * c = gEnv->pConsole;

#if defined(_DEBUG) || defined(DEBUG)
	c->AddCommand("net_ExpKeyXch_test", CExponentialKeyExchange::Test);
#endif

//	c->Register( "sys_network_CPU", &CPU, 1, 0, "Run network multithreaded" );
	pSchedulerDebug = c->RegisterString( "net_scheduler_debug", "0", 0, "Show scheduler debugger for some channel" );
	c->Register( "net_log", &LogLevel, 0, 0, "Logging level of network system" );

#if ENABLE_DEBUG_KIT
	c->Register( "net_inspector", &NetInspector, 0, 0, "Logging level of network system" );
	c->Register( "net_meminfo", &MemInfo, 0, 0 );
	c->Register( "net_entitydebug", &EntityDebug, 0, VF_CHEAT );
	c->Register( "net_logcomments", &LogComments, 0, 0 );
	c->Register( "net_disable_update_disable", &DisableUpdateDisable, 1, VF_DUMPTODISK);
	c->Register( "net_showobjlocks", &ShowObjLocks, 0, 0);
	c->Register( "net_pq_log", &EndpointPendingQueueLogging, 0, 0, "Log updates to the pending queue for each channel");
	c->Register( "net_disconnect_on_uncollected_breakage", &DisconnectOnUncollectedBreakage, 0, 0, "Disconnect on uncollected breakage" );
	c->Register( "net_debug_connection_state", &DebugConnectionState, 0, 0, "Show current connecting status as a head-up display" );
	c->Register( "net_log_dropped_messages", &LogDroppedMessagesVar, 0 );
	c->Register( "net_perfcounters", &PerfCounters, 0, 0, "Display some dedicated server performance counters" );
	c->Register( "net_gamespy_debug",&GSDebugOutput,0,VF_NOHELP,"Sets debug output level for GameSpy SDKs");
	c->Register( "net_random_packet_corruption", &RandomPacketCorruption, 0, VF_CHEAT );
#endif

	c->Register( "net_channelstats", &ChannelStats, 0, 0, "Display bandwidth statistics per-channel" );
	c->Register( "net_bw_aggressiveness", &BandwidthAggressiveness, 0.5f, 0, "Balances TCP friendlyness versus prioritization of game traffic");
	c->Register( "net_inactivitytimeout", &InactivityTimeout, 30, 0, "Set's how many seconds without receiving a packet a connection will stay alive for (can only be set on server)" );
	c->Register( "net_backofftimeout", &BackoffTimeout, 6*60, 0, "Maximum time to allow a remote machine to stall for before disconnecting" );
	c->Register( "sv_maxmemoryusage", &MaxMemoryUsage, 0, 0, "Maximum memory a dedicated server is allowed to use" );

#if NET_ASSERT_LOGGING
	c->Register( "net_assertlogging", &AssertLogging, 0, VF_DUMPTODISK, "Log network assertations" );
#endif

#if LOG_INCOMING_MESSAGES || LOG_OUTGOING_MESSAGES
	c->Register( "net_logmessages", &LogNetMessages, 0, VF_NOHELP );
#endif
#if LOG_BUFFER_UPDATES
	c->Register( "net_logbuffers", &LogBufferUpdates, 0, VF_NOHELP );
#endif
#if STATS_COLLECTOR_INTERACTIVE
	c->Register( "net_showdatabits", &ShowDataBits, 0, 0, "show bits used for different data" );
#endif
#if ENABLE_DEBUG_KIT
	static const int DEFAULT_CHEAT_PROTECTION = 3;
#else
	static const int DEFAULT_CHEAT_PROTECTION = 1;
#endif
	c->Register( "sv_cheatprotection", &CheatProtectionLevel, DEFAULT_CHEAT_PROTECTION, 0, "" );
	pVoiceCodec=c->RegisterString( "sv_voicecodec", "speex", VF_REQUIRE_LEVEL_RELOAD );
	c->Register("sv_voice_enable_groups",&EnableVoiceGroups,1,0);

	c->Register("net_enable_voice_chat",&EnableVoiceChat,1,0);
	c->Register("net_rtt_convergence_factor", &RTTConverge, 995);
#if !ALWAYS_CHECK_CD_KEYS
  c->Register( "sv_check_cd_keys",&CheckCDKeys, 0, VF_NOHELP, "Sets CDKey validation mode:\n 0 - no check\n 1 - check&disconnect unverified\n 2 - check only\n");
#endif
  c->Register( "sv_ranked", &RankedServer, 0, VF_NOHELP, "Enable statistics report, for official servers only.");
	// network visualizationsv_
#if ENABLE_DEBUG_KIT
	c->Register( "net_vis_mode", &VisMode, 0 );
	c->Register( "net_vis_window", &VisWindow, 0/*.3f*/ );
	c->Register( "net_show_ping", &ShowPing, 0 );
#endif

#if INTERNET_SIMULATOR
	c->Register( "net_packetlossrate", &PacketLossRate, 0.0f, VF_CHEAT|VF_NOT_NET_SYNCED, "", OnPacketLossRateChange );
	c->Register( "net_packetextralag", &PacketExtraLag, 0.0f, VF_CHEAT|VF_NOT_NET_SYNCED, "", OnPacketExtraLagChange );
#endif

	c->Register( "net_highlatencythreshold", &HighLatencyThreshold, 0.5f ); // must be net synched
	c->Register( "net_highlatencytimelimit", &HighLatencyTimeLimit, 1.5f ); // must be net synched

	c->Register( "net_enable_tfrc", &EnableTFRC, 1 );

	//c->Register( "net_wmi_check_interval", &WMICheckInterval, gEnv->pSystem->IsDedicated() ? 0.0f : 2.0f, 0 );
	c->Register("net_connectivity_detection_interval", &NetworkConnectivityDetectionInterval, 1.0f);

	// TODO: remove/cleanup
	c->RegisterFloat( "net_defaultChannelBitRateDesired", 200000.0f, VF_READONLY );
	c->RegisterFloat( "net_defaultChannelBitRateToleranceLow", 0.5f, VF_READONLY );
	c->RegisterFloat( "net_defaultChannelBitRateToleranceHigh", 0.001f, VF_READONLY );
	c->RegisterFloat( "net_defaultChannelPacketRateDesired", 50.0f, VF_READONLY );
	c->RegisterFloat( "net_defaultChannelPacketRateToleranceLow", 0.1f, VF_READONLY );
	c->RegisterFloat( "net_defaultChannelPacketRateToleranceHigh", 2.0f, VF_READONLY );
	c->RegisterFloat( "net_defaultChannelIdlePacketRateDesired", 0.05f, VF_READONLY );

  c->Register( "net_voice_lead_packets", &VoiceLeadPackets, 5 );
  c->Register( "net_voice_trail_packets", &VoiceTrailPackets, 5 );
	c->Register( "net_voice_proximity", &VoiceProximity, 0.0f, 0);
	c->Register( "net_voice_averagebitrate", &VoiceAverageBitRate, 15000, 0, "The average bit rate for VOIP transmission\n(default 15000, higher=better quality)."); 

  c->Register( "net_lan_scanport_first", &LanScanPortFirst,  SERVER_DEFAULT_PORT, VF_DUMPTODISK, "Starting port for LAN games scanning");
  c->Register( "net_lan_scanport_num", &LanScanPortNum, 5, VF_DUMPTODISK, "Num ports for LAN games scanning" );

#if ENABLE_DEBUG_KIT
	c->AddCommand("net_reload_scheduler", ReloadScheduler, 0);
#endif

  c->AddCommand("net_set_cdkey", SetCDKey,VF_DUMPTODISK);
	StatsLogin = c->RegisterString( "net_stats_login", "", VF_DUMPTODISK, "Login for reporting stats on dedicated server");
	StatsPassword = c->RegisterString( "net_stats_pass", "", VF_DUMPTODISK, "Password for reporting stats on dedicated server");

	c->AddCommand("net_dump_object_state", DumpObjectState, 0);

#if ENABLE_DEBUG_KIT
	c->AddCommand("net_stall", Stall, VF_CHEAT|VF_NOHELP);
#endif
}

CNetCVars::~CNetCVars()
{
	NET_ASSERT(s_pThis);
	s_pThis = 0;
}

void CNetCVars::DumpObjectState(IConsoleCmdArgs* pArgs)
{
	SCOPED_GLOBAL_LOCK;
	CNetwork::Get()->BroadcastNetDump( eNDT_ObjectState );
}

#if ENABLE_DEBUG_KIT
void CNetCVars::ReloadScheduler(IConsoleCmdArgs* pArgs)
{
	SCOPED_GLOBAL_LOCK;
	CNetwork::Get()->ReloadScheduler();
}
#endif

void CNetCVars::SetCDKey( IConsoleCmdArgs* pArgs)
{
  SCOPED_GLOBAL_LOCK;

  if(pArgs->GetArgCount()>1)
  {
    CNetwork::Get()->SetCDKey(pArgs->GetArg(1));
  }
  else
  {
    CNetwork::Get()->SetCDKey("");
  }
}

#if ENABLE_DEBUG_KIT
void CNetCVars::Stall( IConsoleCmdArgs* pArgs )
{
	float stallTime = 1.0f;
	if (pArgs && pArgs->GetArgCount()>=2)
	{
		if (!sscanf(pArgs->GetArg(1), "%f", &stallTime))
			stallTime = 1.0f;
	}
	if (stallTime > 0.0f)
	{
		NetLogAlways("Stalling all sending for %f seconds", stallTime);
		Get().StallEndTime = g_time + stallTime;
	}
	else
	{
		NetLogAlways("Sleeping for %f seconds", -stallTime);
		SCOPED_GLOBAL_LOCK;
		Sleep((DWORD)(-1000.0f*stallTime));
	}
}
#endif
