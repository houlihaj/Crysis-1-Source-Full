// Network.cpp: implementation of the CNetwork class.
// !F RCON system - lluo
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Network.h"
#include "Utils.h"

//#include "platform.h"
#if defined(LINUX)
#include "INetwork.h"
#endif

#include "IConsole.h"

#include "DebugKit/NetworkInspector.h"

#include "Protocol/NetNub.h"
#include "Protocol/NetChannel.h"
#include "Protocol/Serialize.h"

#include "Context/NetContext.h"

#include "Compression/CompressionManager.h"

#include "Services/ServiceManager.h"
#include "Services/CryLAN/LanQueryListener.h"

#include "VOIP/VoiceManager.h"
#include "VOIP/IVoiceEncoder.h"
#include "VOIP/IVoiceDecoder.h"

#include "DistributedLogger.h"
#include "DebugKit/DebugKit.h"

#include "RemoteControl/RemoteControl.h"
#include "Http/SimpleHttpServer.h"

#include "DebugKit/ServerProfiler.h"

#include "ThreadProfilerSupport.h"
#include "ITextModeConsole.h"
#include "Http/AutoConfigDownloader.h"

#if ENABLE_OBJECT_COUNTING
#include "PsApi.h"
#endif

static const int OCCASIONAL_TICKS = 50;

SSystemBranchCounters g_systemBranchCounters;
SObjectCounters g_objcnt;
static SSystemBranchCounters g_lastSystemBranchCounters;

static CAutoConfigDownloader * g_pFileDownloader;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CNetwork * CNetwork::m_pThis = 0;

int g_QuickLogY;
CTimeValue g_QuickLogTime;
CTimeValue g_time;
int g_lockCount = 0;

struct CNetwork::SNetError CNetwork::m_neNetErrors[]=
{
	{NET_OK, "No Error"},
	{NET_FAIL, "Generic Error"},
	// SOCKET
	{NET_EINTR, "WSAEINTR - interrupted function call"},
	{NET_EBADF, "WSAEBADF - Bad file number"},
	{NET_EACCES, "WSAEACCES - error in accessing socket"},
	{NET_EFAULT, "WSAEFAULT - bad address"},
	{NET_EINVAL, "WSAEINVAL - invalid argument"},
	{NET_EMFILE, "WSAEMFILE - too many open files"},
	{NET_EWOULDBLOCK, "WSAEWOULDBLOCK - resource temporarily unavailable"},
	{NET_EINPROGRESS, "WSAEINPROGRESS - operation now in progress"},
	{NET_EALREADY, "WSAEALREADY - operation already in progress"},
	{NET_ENOTSOCK, "WSAENOTSOCK - socket operation on non-socket"},
	{NET_EDESTADDRREQ, "WSAEDESTADDRREQ - destination address required"},
	{NET_EMSGSIZE, "WSAEMSGSIZE - message to long"},
	{NET_EPROTOTYPE, "WSAEPROTOTYPE - protocol wrong type for socket"},
	{NET_ENOPROTOOPT, "WSAENOPROTOOPT - bad protocol option"},
	{NET_EPROTONOSUPPORT, "WSAEPROTONOSUPPORT - protocol not supported"},
	{NET_ESOCKTNOSUPPORT, "WSAESOCKTNOSUPPORT - socket type not supported"},
	{NET_EOPNOTSUPP, "WSAEOPNOTSUPP - operation not supported"},
	{NET_EPFNOSUPPORT, "WSAEPFNOSUPPORT - protocol family not supported"},
	{NET_EAFNOSUPPORT, "WSAEAFNOSUPPORT - address family not supported by protocol"},
	{NET_EADDRINUSE, "WSAEADDRINUSE - address is in use"},
	{NET_EADDRNOTAVAIL, "WSAEADDRNOTAVAIL - address is not valid in context"},
	{NET_ENETDOWN, "WSAENETDOWN - network is down"},
	{NET_ENETUNREACH, "WSAENETUNREACH - network is unreachable"},
	{NET_ENETRESET, "WSAENETRESET - network dropped connection on reset"},
	{NET_ECONNABORTED, "WSACONNABORTED - software caused connection aborted"},
	{NET_ECONNRESET, "WSAECONNRESET - connection reset by peer"},
	{NET_ENOBUFS, "WSAENOBUFS - no buffer space available"},
	{NET_EISCONN, "WSAEISCONN - socket is already connected"},
	{NET_ENOTCONN, "WSAENOTCONN - socket is not connected"},
	{NET_ESHUTDOWN, "WSAESHUTDOWN - cannot send after socket shutdown"},
	{NET_ETOOMANYREFS, "WSAETOOMANYREFS - Too many references: cannot splice"},
	{NET_ETIMEDOUT, "WSAETIMEDOUT - connection timed out"},
	{NET_ECONNREFUSED, "WSAECONNREFUSED - connection refused"},
	{NET_ELOOP, "WSAELOOP - Too many levels of symbolic links"},
	{NET_ENAMETOOLONG, "WSAENAMETOOLONG - File name too long"},
	{NET_EHOSTDOWN, "WSAEHOSTDOWN - host is down"},
	{NET_EHOSTUNREACH, "WSAEHOSTUNREACH - no route to host"},
	{NET_ENOTEMPTY, "WSAENOTEMPTY - Cannot remove a directory that is not empty"},
	{NET_EUSERS, "WSAEUSERS - Ran out of quota"},
	{NET_EDQUOT, "WSAEDQUOT - Ran out of disk quota"},
	{NET_ESTALE, "WSAESTALE - File handle reference is no longer available"},
	{NET_EREMOTE, "WSAEREMOTE - Item is not available locally"},

	// extended winsock errors(not BSD compliant)
#ifdef _WIN32	
	{NET_EPROCLIM, "WSAEPROCLIM - too many processes"},
	{NET_SYSNOTREADY, "WSASYSNOTREADY - network subsystem is unavailable"},
	{NET_VERNOTSUPPORTED, "WSAVERNOTSUPPORTED - winsock.dll verison out of range"},
	{NET_NOTINITIALISED, "WSANOTINITIALISED - WSAStartup not yet performed"},
	{NET_NO_DATA, "WSANO_DATA - valid name, no data record of requested type"},
	{NET_EDISCON, "WSAEDISCON - graceful shutdown in progress"},
#endif	

	// extended winsock errors (corresponding to BSD h_errno)
	{NET_HOST_NOT_FOUND, "WSAHOST_NOT_FOUND - host not found"},
	{NET_TRY_AGAIN, "WSATRY_AGAIN - non-authoritative host not found"},
	{NET_NO_RECOVERY, "WSANO_RECOVERY - non-recoverable error"},
	{NET_NO_DATA, "WSANO_DATA - valid name, no data record of requested type"},
	{NET_NO_ADDRESS, "WSANO_ADDRESS - no address, look for MX record"},

	// XNetwork specific
	{NET_NOIMPL, "XNetwork - Function not implemented"},
	{NET_SOCKET_NOT_CREATED, "XNetwork - socket not yet created"},
	{0, 0} // sentinel
};

static const float CLEANUP_TICK = 0.2f;

class CNetwork::CNetworkThread : public CrySimpleThread<>
{
public:
	CNetworkThread() {}

	void Run()
	{
		CryThreadSetName( -1,"Network" );

		while (Get()->UpdateTick(true))
			;
	}

	void Cancel()
	{
		Get()->m_pSocketIOManager->PushUserMessage(-1);
	}
};

class CNetwork::CBufferWakeups
{
public:
	CBufferWakeups( bool release, CNetwork * pNet ) : m_release(false), m_pNet(pNet) { m_pNet->m_bufferWakeups++; }
	~CBufferWakeups()
	{
		if (0 == --m_pNet->m_bufferWakeups)
			if (m_release)
				if (m_pNet->m_wokenUp)
				{
					m_pNet->WakeThread();
					m_pNet->m_wokenUp = false;
				}
	}

private:
	bool m_release;
	CNetwork * m_pNet;
};

CNetwork::CNetwork() : m_bQuitting(false), m_pResolver(0), m_bufferWakeups(0), m_wokenUp(false), m_nextCleanup(0.0f), m_cleanupMember(0), m_isMultithreaded(false), m_occasionalCounter(OCCASIONAL_TICKS), m_cpuCount(1), m_forceLock(true), m_isPbSvActive(false), m_isPbClActive(false)
{
	NET_ASSERT( !m_pThis );
	m_pThis = this;
	g_time = gEnv->pTimer->GetAsyncTime();

	for (int i=0; i<sizeof(m_inSync)/sizeof(*m_inSync); i++)
		m_inSync[i] = 0;

	SCOPED_GLOBAL_LOCK;

	if (gEnv->pSystem->IsDedicated())
		CServerProfiler::Init();

	//Timur, Turn off testing.
	/*
	if (!CWhirlpoolHash::Test())
	{
		CryError("Implementation error in Whirlpool hash");
	}
	*/

#if ENABLE_DEBUG_KIT
	m_pNetInspector = CNetworkInspector::CreateInstance();
#endif
	m_pServiceManager.reset( new CServiceManager );


//#if defined(WIN32) || defined(WIN64)
//	m_needSleeping = false;
//	m_sink.m_pNetwork = this;
//#endif
}

CNetwork::~CNetwork()
{
	NET_ASSERT( m_pThis == this );

	SyncWithGame( eNGS_Shutdown );
	NET_ASSERT( m_vMembers.empty() );

	if (m_isMultithreaded)
	{
		m_pThread->Cancel();
		m_pThread->Join();
		m_pThread.reset();
	}

#if ENABLE_DEBUG_KIT
	CDebugKit::Shutdown();
#endif

	m_pServiceManager.reset();

	// can safely delete singletons after this point
	// as all threads have been completed

#if ENABLE_DISTRIBUTED_LOGGER
	m_pLogger.reset();
#endif

#if ENABLE_DEBUG_KIT
	if (m_pNetInspector)
		CNetworkInspector::ReleaseInstance();
#endif

	SAFE_DELETE(m_pSocketIOManager);

#if defined(WIN32) || defined(XENON)
	WSACleanup();
#ifdef XENON
	XNetCleanup();
#endif // XENON
#else
	//CSocketManager::releaseImpl();
	//delete &ISocketManagerImplementation::getInstance();
#endif // WIN32 || XENON

	SAFE_DELETE(m_pResolver);

	m_pThis = 0;
}

bool CNetwork::AllSuicidal()
{
	for (VMembers::const_iterator iter = m_vMembers.begin(); iter != m_vMembers.end(); ++iter)
		if (!(*iter)->IsSuicidal())
			return false;
	return true;
}

void CNetwork::FastShutdown()
{
	//SyncWithGame( eNGS_Shutdown );
	m_pServiceManager.reset();
}

void CNetwork::SetCDKey(const char* key)
{
  m_CDKey = key;
}

void CNetwork::AddExecuteString( const string& toExec )
{
	if (IsPrimaryThread())
		gEnv->pConsole->ExecuteString(toExec.c_str());
	else
		m_consoleCommandsToExec.push_back(toExec);
}

// EvenBalance - M. Quinn
bool CNetwork::PbConsoleCommand(const char* command, int length)
{
#ifdef __WITH_PB__
	SCOPED_GLOBAL_LOCK;

	if ( !strnicmp ( command, "pb_sv", 5 )  ) {
		PbSvAddEvent ( PB_EV_CMD, -1, length, (char *)command ) ;
		return true ;
	}
	else if (!strnicmp ( command, "pb_", 3 ) ) {
		PbClAddEvent ( PB_EV_CMD, length, (char *)command ) ;
		return true ;
	}
	else
		return false ;
#else
		return true ;
#endif
}

// EvenBalance - M. Quinn
void CNetwork::PbCaptureConsoleLog(const char* output, int length)
{

#ifdef __WITH_PB__
	SCOPED_GLOBAL_LOCK_NO_LOG;

	PbCaptureConsoleOutput ( (char *)output, length ) ;
#endif

}

// EvenBalance - M. Quinn
void CNetwork::PbServerAutoComplete(const char* command, int length)
{
#ifdef __WITH_PB__
	SCOPED_GLOBAL_LOCK;

	PbServerCompleteCommand ( (char *)command, length ) ;
	return ;
#endif
}

// EvenBalance - M. Quinn
void CNetwork::PbClientAutoComplete(const char* command, int length)
{
#ifdef __WITH_PB__
	SCOPED_GLOBAL_LOCK;

	PbClientCompleteCommand ( (char *)command, length ) ;
	return ;
#endif
}

#if defined(WIN32) || defined(WIN64)
#include <IPHlpApi.h>
#pragma comment(lib, "IPHlpApi.lib")
#endif

bool CNetwork::CNetworkConnectivityDetection::HasNetworkConnectivity()
{
	float detectionInterval = std::max(1.0f, CVARS.NetworkConnectivityDetectionInterval);
	// if we've received a packet recently, then assume that we still have connectivity
	if ((g_time - m_lastPacketReceived).GetSeconds() < detectionInterval)
		m_hasNetworkConnectivity = true;
	// otherwise, if we've not checked for some time, check again (also triggers the first time this routine is called normally)
	else if ((g_time - m_lastCheck).GetSeconds() > detectionInterval)
	{
		DetectNetworkConnectivity();
		m_lastCheck = g_time;
	}
	return m_hasNetworkConnectivity;
}

void CNetwork::CNetworkConnectivityDetection::DetectNetworkConnectivity()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_NETWORK);

	std::vector<string> warnings;
	bool hadNetworkConnectivity = m_hasNetworkConnectivity;

	m_hasNetworkConnectivity = true;

#if defined(WIN32) || defined(WIN64)
	DWORD size = 0;
	GetInterfaceInfo(NULL, &size);

	std::vector<BYTE> buffer;
	buffer.resize(size);

	PIP_INTERFACE_INFO info = (PIP_INTERFACE_INFO)&buffer[0];
	if (NO_ERROR != GetInterfaceInfo(info, &size))
	{
		warnings.push_back(string().Format("[connectivity] Failed retrieving network interface table"));
		m_hasNetworkConnectivity = false;
	}
	else if (info->NumAdapters <= 0)
	{
		warnings.push_back(string().Format("[connectivity] No network adapters available"));
		m_hasNetworkConnectivity = false;
	}
	else
	{
		DWORD nconnected = info->NumAdapters;
		for (LONG i = 0; i < info->NumAdapters; ++i)
		{
			IP_ADAPTER_INDEX_MAP& adapter = info->Adapter[i];
			MIB_IFROW ifr; ZeroMemory(&ifr, sizeof(ifr));
			ifr.dwIndex = adapter.Index;
			if (NO_ERROR != GetIfEntry(&ifr))
			{
				warnings.push_back(string().Format("[connectivity] Failed retrieving network interface at index %lu", ifr.dwIndex));
				//m_hasNetworkConnectivity = false;
				--nconnected;
			}
			else if (ifr.dwOperStatus != MIB_IF_OPER_STATUS_OPERATIONAL)
			{
				warnings.push_back(string().Format("[connectivity] %s is not operational", ifr.bDescr));
				//m_hasNetworkConnectivity = false;
				--nconnected;
			}
		}
		m_hasNetworkConnectivity = (nconnected != 0);
	}
#else
	// TODO:
#endif

	if (hadNetworkConnectivity && !m_hasNetworkConnectivity)
		for (size_t i = 0; i < warnings.size(); ++i)
			NetWarning(warnings[i].c_str());
}

bool CNetwork::HasNetworkConnectivity()
{
	SCOPED_GLOBAL_LOCK;
	return m_detection.HasNetworkConnectivity();
}

bool CNetwork::Init( int ncpu )
{
	m_cpuCount = ncpu;

	m_gameTime = gEnv->pTimer->GetFrameStartTime();
	m_pMessageQueueConfig = CMessageQueue::LoadConfig(PathUtil::GetGameFolder() + "/Scripts/Network/Scheduler.xml");
	m_schedulerVersion = 1;

	RunTests();

	if (!m_pResolver)
		m_pResolver = new CNetAddressResolver();

#if defined(WIN32) || defined(XENON)

#ifdef XENON

	// Bypass the Xbox Secure Network Library (see "winsockx.h" or the XDK for details)
	XNetStartupParams xnsp;
	memset(&xnsp, 0, sizeof(xnsp));
	xnsp.cfgSizeOfStruct = sizeof(XNetStartupParams);
	xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
	if( XNetStartup( &xnsp ) != 0 )
	{
		NetWarning( "XNetStartup() failed." );
		return false;
	}
#endif // XENON

	uint16 wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		return false;
	}

	if (LOBYTE(wsaData.wVersion) != 2 ||
		HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
#ifdef XENON
		XNetCleanup();
#endif // XENON
		return false;
	}
#endif // WIN32 || XENON

	if (!(m_pSocketIOManager = CreateSocketIOManager(m_cpuCount)))
		return false;
	CryLogAlways("[net] using %s socket io management", m_pSocketIOManager->GetName());

	int n=0;
	while (m_neNetErrors[n].sErrorDescription!='\0')
	{
		m_mapErrors[m_neNetErrors[n].nrErrorCode]=m_neNetErrors[n].sErrorDescription;
		n++;
	}

	LogNetworkInfo();

	InitPrimaryThread();
	InitFrameTypes();

#if ENABLE_DISTRIBUTED_LOGGER
	m_pLogger.reset( new CDistributedLogger );
#endif

	g_pFileDownloader = new CAutoConfigDownloader;

	return true;
}

void DownloadConfig()
{
	g_pFileDownloader->TriggerRefresh();
}

void CNetwork::EnableMultithreading( bool enable )
{
	m_mutex.Lock();
	if (enable != m_isMultithreaded)
	{
		if (enable)
		{
			m_pThread.reset( new CNetworkThread() );
			m_pThread->Start();
		}
		else
		{
			m_pThread->Cancel();
			m_mutex.Unlock();
			m_pThread->Join();
			m_mutex.Lock();
			m_pThread.reset();
		}
		m_isMultithreaded = enable;
	}
	m_mutex.Unlock();
}

void CNetwork::WakeThread()
{
	if (m_bufferWakeups)
		m_wokenUp = true;
	else
		m_pSocketIOManager->PushUserMessage(-2);
}

void CNetwork::BroadcastNetDump( ENetDumpType type )
{
	for (size_t i=0; i<m_vMembers.size(); i++)
	{
		INetworkMemberPtr pMember = m_vMembers[i];
		pMember->NetDump(type);
	}
}

//////////////////////////////////////////////////////////////////////////
INetNub * CNetwork::CreateNub( const char * address, IGameNub * pGameNub, 
															 IGameSecurity * pSecurity, 
															 IGameQuery * pGameQuery )
{
	SCOPED_GLOBAL_LOCK;
	TNetAddressVec addrs;
	if (address)
	{
		CNameRequestPtr pReq = GetResolver()->RequestNameLookup(address);
		pReq->Wait();
		if (pReq->GetResult(addrs) != eNRR_Succeeded)
		{
			NetLogAlways("Name resolution for '%s' failed", address);
			return NULL;
		}
//		GetResolver()->FromString( address, addrs );
	}
	else
	{
		SIPv4Addr addr; // defaults to 0,0
		addrs.push_back( TNetAddress( addr ) );
	}
	if (addrs.empty())
	{
		SAFE_RELEASE(pGameNub);
		return NULL;
	}

	//NetLog( "Resolved name as %s", GetResolver()->ToNumericString(addrs[0]).c_str() );
	CNetNub * pNub = new CNetNub( addrs[0], pGameNub, 
																pSecurity, pGameQuery );
	if (!pNub->Init(this))
	{
		NetWarning("Failed creating network nub at %s", address);
		delete pNub;
		return NULL;
	}

	AddMember(pNub);
	CNetwork::Get()->WakeThread();
	return pNub;
}

ILanQueryListener * CNetwork::CreateLanQueryListener( IGameQueryListener * pGameQueryListener )
{
	SCOPED_GLOBAL_LOCK;
	CLanQueryListener * pLanQueryListener = new CLanQueryListener(pGameQueryListener);
	if (!pLanQueryListener->Init())
	{
		delete pLanQueryListener;
		return NULL;
	}

	AddMember( pLanQueryListener );
	CNetwork::Get()->WakeThread();
	return pLanQueryListener;
}

INetContext * CNetwork::CreateNetContext( IGameContext * pGameContext, uint32 flags )
{
	SCOPED_GLOBAL_LOCK;
	CNetContext * pContext = new CNetContext(pGameContext, flags);
	AddMember( pContext );
	CNetwork::Get()->WakeThread();
	return pContext;
}

const char *CNetwork::EnumerateError(NRESULT err)
{
	TErrorMap::iterator i = m_mapErrors.find(err);
	if (i != m_mapErrors.end())
	{
		return i->second;
	}
	return "Unknown";
}

void CNetwork::Release()
{
//	SCOPED_GLOBAL_LOCK;
	
//	if (m_nNetworkInitialized)
//		;

	delete this;
}

void CNetwork::GetMemoryStatistics(ICrySizer *pSizer)
{
	SCOPED_GLOBAL_LOCK;
	SIZER_COMPONENT_NAME(pSizer, "CNetwork");

	if (!pSizer->Add(*this))
		return;

	pSizer->AddObject(m_neNetErrors,sizeof(m_neNetErrors));
	pSizer->AddContainer(m_vMembers);

	for (VMembers::iterator i = m_vMembers.begin(); i != m_vMembers.end(); ++i)
		(*i)->GetMemoryStatistics( pSizer );

	m_toGame.GetMemoryStatistics(pSizer);
	m_fromGame.GetMemoryStatistics(pSizer);
	m_intQueue.GetMemoryStatistics(pSizer);
	m_toGameLazyBuilding.GetMemoryStatistics(pSizer);
	m_toGameLazyProcessing.GetMemoryStatistics(pSizer);
	m_timer.GetMemoryStatistics(pSizer);

	{
		CryAutoLock<NetFastMutex> lk(m_fromGame_otherThreadLock);
		m_fromGame_otherThreadQueue.GetMemoryStatistics(pSizer);
	}

//	for (int i=0; i<eMMT_NUM_TYPES; i++)
//		m_mmm[i].GetMemoryStatistics(pSizer);

	m_compressionManager.GetMemoryStatistics(pSizer);

	m_pServiceManager->GetMemoryStatistics(pSizer);

	//m_pMessageQueueConfig->GetMemoryStatistics(pSizer);


//#if ( defined(WIN32) || defined(WIN64) ) && ( defined(_DEBUG) || defined(DEBUG) )
//	_CrtMemDumpAllObjectsSince(NULL);
//	_CrtMemState ms;
//	_CrtMemCheckpoint(&ms);
//	_CrtMemDumpStatistics(&ms);
//#endif
}

class CInSync
{
public:
	CInSync( int& inSync ) : m_inSync(inSync) 
	{ 
		ASSERT_PRIMARY_THREAD;
		++m_inSync; 
	}
	~CInSync() 
	{ 
		ASSERT_PRIMARY_THREAD;
		--m_inSync; 
	}
private:
	int& m_inSync;
};

//////////////////////////////////////////////////////////////////////////
void CNetwork::SyncWithGame(ENetworkGameSync type)
{
//#if defined(WIN32) || defined(WIN64)
//	// HACK: to make the WMI thread responsive on single core
//	if (type == eNGS_FrameEnd && m_cpuCount < 2 && m_needSleeping)
//		Sleep(1);
//#endif

	static CThreadProfilerEvent evt_start("net_startframe");
	CThreadProfilerEvent::Instance show_evt_start(evt_start, type == eNGS_FrameStart);

	FUNCTION_PROFILER( GetISystem(), PROFILE_NETWORK );

#if _DEBUG && defined(USER_craig)
	//SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

	int totSync = 0;
	for (int i=0; i<sizeof(m_inSync)/sizeof(*m_inSync); i++)
		totSync += m_inSync[i];

	switch (type)
	{
	case eNGS_Shutdown:
		if (totSync)
			CryError("CNetwork::SyncWithGame(eNGS_Shutdown) called recursively");
		break;
	case eNGS_FrameStart:
	case eNGS_FrameEnd:
		if (totSync && totSync != (m_inSync[eNGS_Shutdown] + m_inSync[eNGS_NUM_ITEMS+1]))
			return;
		break;
	}

	{
		SCOPED_GLOBAL_LOCK;
		CBufferWakeups bufferWakeups(type == eNGS_FrameEnd, this);
		CInSync inSync(m_inSync[type]);
		DoSyncWithGame(type);
	}

	if (!m_inSync[eNGS_NUM_ITEMS+1])
	{
		CInSync inSyncFlush(m_inSync[eNGS_NUM_ITEMS+1]);
		m_toGameLazyProcessing.Flush(false);
	}
}

#if ENABLE_OBJECT_COUNTING
struct SObjCntDisplay
{
	const char * name;
	int n;
	bool operator<( const SObjCntDisplay& r ) const
	{
		return n > r.n;
	}
};
static void DumpObjCnt()
{
	SObjCntDisplay temp;
	std::vector<SObjCntDisplay> v;
#define COUNTER(cnt) temp.n = g_objcnt.cnt.QuickPeek(); if (temp.n) { temp.name = #cnt; v.push_back(temp); }
#include "objcnt_defs.h"
#undef COUNTER

	std::sort(v.begin(), v.end());

	if (ITextModeConsole * pTMC = gEnv->pSystem->GetITextModeConsole())
	{
		char buf[256];
		int y = 20;

		PROCESS_MEMORY_COUNTERS memcnt;
		GetProcessMemoryInfo( GetCurrentProcess(), &memcnt, sizeof(memcnt) );
		float kils = memcnt.WorkingSetSize / 1024.0f;
		sprintf(buf, "MEM: %.2f kilobyte", kils);
		pTMC->PutText(0, y++, buf);

		for (std::vector<SObjCntDisplay>::iterator it = v.begin(); it != v.end(); ++it)
		{
			sprintf(buf, "%d", it->n);
			pTMC->PutText(0, y, buf);
			pTMC->PutText(10, y++, it->name);
		}
	}
};
#endif

//////////////////////////////////////////////////////////////////////////
void CNetwork::DoSyncWithGame( ENetworkGameSync type )
{
	bool continuous = false;

	switch (type)
	{
	case eNGS_FrameStart:
		{
			FRAME_PROFILER("Network:FrameStart", gEnv->pSystem, PROFILE_NETWORK);
			if (gEnv->pSystem->IsDedicated())
				g_pFileDownloader->Update();
			if (!gEnv->pSystem->IsEditor())
				m_gameTime = gEnv->pTimer->GetFrameStartTime();
			else
				m_gameTime = gEnv->pTimer->GetAsyncCurTime();
			//NetQuickLog(false, 0, "Local: %f", m_gameTime.GetSeconds());
			FlushNetLog(false);
			m_toGame.Flush(true);
			if (!m_inSync[eNGS_NUM_ITEMS+1])
				m_toGameLazyBuilding.Swap(m_toGameLazyProcessing);

#if ENABLE_DEBUG_KIT
			if (CNetCVars::Get().PerfCounters)
			{
				if (ITextModeConsole * pTMC = gEnv->pSystem->GetITextModeConsole())
				{
					int y = 0;
					char buf[256];
#define DUMP_COUNTER(name) sprintf(buf, #name ": %d (%d)", g_systemBranchCounters.name, g_systemBranchCounters.name - g_lastSystemBranchCounters.name); pTMC->PutText(0, y++, buf)
					DUMP_COUNTER(updateTickMain);
					DUMP_COUNTER(updateTickSkip);
					DUMP_COUNTER(iocpReapSleep);
					DUMP_COUNTER(iocpReapSleepCollect);
					DUMP_COUNTER(iocpReapImmediate);
					DUMP_COUNTER(iocpReapImmediateCollect);
					DUMP_COUNTER(iocpIdle);
					DUMP_COUNTER(iocpBackoffCheck);
					DUMP_COUNTER(iocpForceSync);
#undef DUMP_COUNTER
					g_lastSystemBranchCounters = g_systemBranchCounters;
				}
			}
#endif

#if ENABLE_OBJECT_COUNTING
			DumpObjCnt();
#endif
		}
		break;
	case eNGS_FrameEnd:
		{
			FRAME_PROFILER("Network:FrameEnd", gEnv->pSystem, PROFILE_NETWORK);

			//NetQuickLog( gEnv->pSystem->IsDedicated(), 0, "locks/frame: %d", g_lockCount );
			g_lockCount = 0;

			FlushNetLog(true);
			m_fromGame.Flush(true);
			{
				CryAutoLock<NetFastMutex> lk(m_fromGame_otherThreadLock);
				m_fromGame_otherThreadQueue.Flush(true);
			}
//			for (int i=0; i<eMMT_NUM_TYPES; i++)
//				m_mmm[i].DebugDraw(i);

#if ENABLE_DEBUG_KIT
			CDebugKit::Get().Update();
#endif
#if STATS_COLLECTOR_INTERACTIVE
			GetStats()->InteractiveUpdate();
#endif

			if (CServerProfiler::ShouldSaveAndCrash())
			{
				gEnv->pConsole->ExecuteString("SaveLevelStats");
				_exit(0);
			}
		}
		break;
	case eNGS_Shutdown:
		FlushNetLog(true);
		continuous = true;
		break;
	}

	if (!continuous)
	{
		UpdateLoop(type);
	}
	else while (true)
	{
		CInSync inSync(m_inSync[eNGS_NUM_ITEMS]);

		bool emptied = false;
		bool suicidal = false;
		{
			m_toGame.Flush(true);
			UpdateLoop( type );
			emptied = m_vMembers.empty();
			if (!emptied)
				suicidal = AllSuicidal();
			m_fromGame.Flush(true);
			m_toGame.Flush(true);
			{
				CryAutoLock<NetFastMutex> lk(m_fromGame_otherThreadLock);
				m_fromGame_otherThreadQueue.Flush(true);
			}
		}
		if (continuous)
			if (!emptied)
				if (suicidal)
				{
					{
						CInSync inSyncFlush(m_inSync[eNGS_NUM_ITEMS+1]);
						if (m_isMultithreaded)
							m_mutex.Unlock();
						m_toGameLazyProcessing.Flush(false);
						if (m_isMultithreaded)
							m_mutex.Lock();
						m_toGameLazyBuilding.Swap(m_toGameLazyProcessing);
					}
					DoSyncWithGame(eNGS_FrameStart);
					DoSyncWithGame(eNGS_FrameEnd);
					continue;
				}
		break;
	}
#if ENABLE_DISTRIBUTED_LOGGER
	if (m_pLogger.get())
		m_pLogger->Update( g_time );
#endif
#if ENABLE_DEBUG_KIT
	if (m_cvars.NetInspector)
		if (m_pNetInspector)
			m_pNetInspector->Update();
#endif
}

void CNetwork::UpdateLoop( ENetworkGameSync type )
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_NETWORK);
	if (type == eNGS_FrameEnd)
	{
		for (size_t i=0; i<m_vMembers.size(); i++)
		{
			INetworkMemberPtr pMember = m_vMembers[i];
			if (pMember->IsDead())
			{
				m_vMembers.erase( m_vMembers.begin() + i );
				i --;
			}
			else
			{
				ENSURE_REALTIME;
				pMember->SyncWithGame(type);
			}
		}
	}
	else
	{
		for (size_t i=0; i<m_vMembers.size(); i++)
		{
			ENSURE_REALTIME;
			m_vMembers[i]->SyncWithGame(type);
		}
	}

#ifdef __WITH_PB__
	if (type == eNGS_FrameEnd && !gEnv->bEditor)
	{
		if (m_isPbSvActive)
		{
			//NET_ASSERT(isPbSvEnabled() != 0);
			PbServerProcessEvents();
		}

		if (m_isPbClActive)
		{
			//NET_ASSERT(isPbClEnabled() != 0);
			PbClientProcessEvents();
		}

		ASSERT_PRIMARY_THREAD;
		for (size_t i=0; i<m_consoleCommandsToExec.size(); i++)
			gEnv->pConsole->ExecuteString(m_consoleCommandsToExec[i].c_str());
		m_consoleCommandsToExec.resize(0);
	}
#endif

	if (!m_isMultithreaded && type == eNGS_FrameEnd)
	{
		UpdateTick(false);
	}
}

bool CNetwork::UpdateTick( bool mt )
{
	float waitTime = 0.0f;

	ITimer * pTimer = gEnv->pTimer;
	bool mustLock = m_forceLock;
	m_forceLock = false;
	CTimeValue now = pTimer->GetAsyncTime();
	mustLock |= ((now - m_lastUpdateLock).GetMilliSeconds() > 15);

	bool isLocked = false;
	if (mustLock)
	{
		m_mutex.Lock();
		isLocked = true;
		m_lastUpdateLock = pTimer->GetAsyncTime();
	}
	else
	{
		isLocked = m_mutex.TryLock();
		m_lastUpdateLock = now;
	}
		
	if (isLocked)
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_NETWORK);

		g_systemBranchCounters.updateTickMain ++;

		waitTime = (TIMER.Update() - g_time).GetSeconds();
		m_pThis->m_intQueue.Flush(true);

		if (!m_vMembers.empty() && g_time >= m_nextCleanup)
		{
			INetworkMemberPtr pMember = m_vMembers[m_cleanupMember % m_vMembers.size()];
			if (!pMember->IsDead())
				pMember->PerformRegularCleanup();
			m_cleanupMember ++;
		}

		waitTime *= 1000.0f;
		if (waitTime > 600.0f*1000)
			waitTime = 600.0f*1000;
		else if (waitTime < 10) // min 10ms sleep
			waitTime = 10;
		waitTime += 3.0f * rand() / RAND_MAX; // random extra sleep to save any coherency effects with the main thread

		if (!mt)
			waitTime = 0;
		
		m_mutex.Unlock();
	}
	else
	{
		waitTime = 1;
		g_systemBranchCounters.updateTickSkip ++;
	}

	bool ret = true;
	bool didWorkInPoll = false;
	int pollres = m_pSocketIOManager->Poll(waitTime, didWorkInPoll);
	if (pollres <= -666)
		;
	else switch (pollres)
	{
	case -1:
		ret = false;
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case -2:
		break;
	default:
		NetWarning("Unhandled poll result %d", pollres);
		break;
	}
	m_forceLock = !didWorkInPoll;

	return ret;
}

namespace
{
	struct CompareMembers
	{
		bool operator()( INetworkMemberPtr p1, INetworkMemberPtr p2 ) const
		{
			return p1->UpdateOrder < p2->UpdateOrder;
		}
	};
}

void CNetwork::AddMember( INetworkMemberPtr pMember )
{
	m_vMembers.push_back(pMember);
	std::stable_sort(m_vMembers.begin(), m_vMembers.end(), CompareMembers());
}

const char * CNetwork::GetHostName()
{
	static char buf[256];
#ifdef XENON
	strcpy(buf, "Xenon");
#else
#if defined(PS3)
	strcpy(buf, "PlayStation3");
#else
	gethostname(buf, 256);
#endif
#endif
	return buf;
}

uint32 CNetwork::GetHostLocalIp()
{
#if defined(WIN32) || defined(LINUX)

	char	buf[256];

	if(!gethostname(buf, sizeof(buf))) 
	{
		CNameRequestPtr nr = RESOLVER.RequestNameLookup(buf);
		nr->Wait();
		uint32 ip = 0;
		TNetAddressVec ips;
		if(nr->GetResult(ips) == eNRR_Succeeded)
		{
			for(int i=0;i<ips.size();++i)
			{
				if(SIPv4Addr* addr = ips[i].GetPtr<SIPv4Addr>())
				{
					if(RESOLVER.IsPrivateAddr(ips[i]))
					{
						ip = addr->addr;
						break;
					}
					else if (!ip)
					{
						ip = addr->addr;
					}						
				}
			}
		}
		return ip;
	}
#endif //#if defined(WIN32) || defined(LINUX)
	return 0;
}

void CNetwork::LogNetworkInfo()
{
#if defined(WIN32) || defined(LINUX)
	char	buf[256];

	if(!gethostname(buf, sizeof(buf))) 
	{
		NetLogAlways("network hostname: %s", buf);
		CNameRequestPtr nr = RESOLVER.RequestNameLookup(buf);
		nr->Wait();
		uint32 ip = 0;
		TNetAddressVec ips;
		if(nr->GetResult(ips) == eNRR_Succeeded)
		{
			for(int i=0;i<ips.size();++i)
			{
				if(SIPv4Addr* addr = ips[i].GetPtr<SIPv4Addr>())
				{
					NetLogAlways("  ip:%d.%d.%d.%d",		//  port:%d  family:%x",	
						((addr->addr>>24)&0xFF),
						((addr->addr>>16)&0xFF),
						((addr->addr>>8)&0xFF),
						(addr->addr&0xFF));
				}
			}
		}
	}
#endif //#if defined(WIN32) || defined(LINUX)
}

const char * GetTimestampString()
{
#ifndef WIN32
	return "";
#else
	SYSTEMTIME st;
	GetSystemTime(&st);
	static char buffer[512];
	sprintf(buffer, "%.2d:%.2d:%.2d.%.3d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	return buffer;
#endif
}

INetworkServicePtr CNetwork::GetService( const char * name )
{
	return m_pServiceManager->GetService(name);
}

IRemoteControlSystem* CNetwork::GetRemoteControlSystemSingleton()
{
	return &CRemoteControlSystem::GetSingleton();
}

ISimpleHttpServer* CNetwork::GetSimpleHttpServerSingleton()
{
	return &CSimpleHttpServer::GetSingleton();
}

void CNetwork::ReloadScheduler()
{
	ASSERT_GLOBAL_LOCK;
	
	CMessageQueue::CConfig * old = m_pMessageQueueConfig;
	m_pMessageQueueConfig = CMessageQueue::LoadConfig(PathUtil::GetGameFolder() + "/Scripts/Network/Scheduler.xml");
	if (m_pMessageQueueConfig)
	{
		CMessageQueue::DestroyConfig(old);
		m_schedulerVersion ++;
	}
	else
		m_pMessageQueueConfig = old;
}

void CNetwork::SetNetGameInfo( SNetGameInfo info )
{
	SCOPED_GLOBAL_LOCK;
	m_gameInfo = info;
}

SNetGameInfo CNetwork::GetNetGameInfo()
{
	ASSERT_GLOBAL_LOCK;
	return m_gameInfo;
}

int CNetwork::RegisterForFastLookup(CNetChannel * pChannel)
{
	size_t i;
	for (i=0; i<m_vFastChannelLookup.size(); i++)
	{
		if (!m_vFastChannelLookup[i])
		{
			m_vFastChannelLookup[i] = pChannel;
			return i;
		}
	}
	m_vFastChannelLookup.push_back(pChannel);
	return i;
}

void CNetwork::UnregisterFastLookup(int id)
{
	NET_ASSERT(id>=0);
	NET_ASSERT(id<m_vFastChannelLookup.size());
	NET_ASSERT(m_vFastChannelLookup[id]);
	m_vFastChannelLookup[id] = 0;
}

CNetChannel * CNetwork::GetChannelByFastLookupId(int id)
{
	NET_ASSERT(id>=0);
	if (id<m_vFastChannelLookup.size())
		return m_vFastChannelLookup[id];
	else
		return 0;
}

void CEnsureRealtime::Failed()
{
	DEBUG_BREAK;
	NET_ASSERT(!"REALTIME");
}

























// unit tests


struct ImprovedTest
{
	uint32 nValue; 
	uint32 nLeft; 
	uint32 nRight; 
	uint32 nHeight; 
	uint32 nRange;
	uint8  nInRangePercentage;
	uint8  nMaxBits;
};


struct ITestPolyQueue 
{
	virtual void Poo() = 0;
};
CPolymorphicQueue<ITestPolyQueue> testq;
template <int N>
struct CTestPolyQueue : public ITestPolyQueue
{
	CTestPolyQueue(int i) : m_val(i) {}
	int m_val;
	int padding[N*100];
	void Poo() 
	{
		//		CryLogAlways("%d", m_val);
		if (rand()>RAND_MAX/2)
			testq.Add(CTestPolyQueue(m_val + 10000000));
	}
};
void TestPolyQueue( ITestPolyQueue * p )
{
	p->Poo();
}

bool TestStringToKey( const char * x, uint32 n )
{
	uint32 m;
	if (!StringToKey(x, m))
		return false;
	if (m != n)
		return false;
	if (KeyToString(n) != x)
		return false;
	return true;
}

extern void TestTableDirVec3();

class CStressTestMMM
{
public:
	void Run(int iters)
	{
		for (int i=0; i<iters; i++)
		{
			bool alloc = rand() > RAND_MAX/3;
			bool resize = false;
			if (m_alloced.size() > 15000)
				alloc = false;
			if (m_alloced.empty())
				alloc = true;
			if (alloc && !m_alloced.empty() && rand() < RAND_MAX/2)
				resize = true;
			VerifyAll();
			size_t sz = rand() % 2070;
			int which = m_alloced.empty()? 0 : rand() % m_alloced.size();
			if (alloc)
			{
				if (resize)
				{
					std::map<TMemHdl, size_t>::iterator it = m_alloced.begin();
					for (int j=0; j<which; j++)
						++it;
					m_mmm.ResizeHdl(it->first, sz);
					it->second = sz;
					Init(it->first);
					//					VerifyAll();
				}
				else
				{
					TMemHdl hdl = m_mmm.AllocHdl(sz);
					m_alloced.insert( std::make_pair(hdl, sz) );
					Init(hdl);
					//					VerifyAll();
				}
			}
			else
			{
				std::map<TMemHdl, size_t>::iterator it = m_alloced.begin();
				for (int j=0; j<which; j++)
					++it;
				m_mmm.FreeHdl(it->first);
				m_alloced.erase(it);
				//				VerifyAll();
			}
		}
	}

private:
	CMementoMemoryManager m_mmm;
	std::map<TMemHdl, size_t> m_alloced;

	void Init( TMemHdl hdl )
	{
		std::map<TMemHdl, size_t>::iterator it = m_alloced.find(hdl);
		NET_ASSERT(it != m_alloced.end());
		NET_ASSERT(m_mmm.GetHdlSize(hdl) == it->second);
		uint8 * p = (uint8 *)m_mmm.PinHdl(hdl);
		for (int i=0; i<it->second; i++)
		{
			uint8 b = i;
			p[i] = b;
		}
		Verify(hdl);
	}

	void VerifyAll()
	{
		for (std::map<TMemHdl, size_t>::iterator it = m_alloced.begin(); it != m_alloced.end(); ++it)
			Verify(it->first);
	}

	void Verify( TMemHdl hdl )
	{
		std::map<TMemHdl, size_t>::iterator it = m_alloced.find(hdl);
		NET_ASSERT(it != m_alloced.end());
		NET_ASSERT(m_mmm.GetHdlSize(hdl) == it->second);
		uint8 * p = (uint8 *)m_mmm.PinHdl(hdl);
		for (int i=0; i<it->second; i++)
		{
			uint8 b = i;
			NET_ASSERT( p[i] == b );
		}
	}
};

struct SAlphabetTestOp
{
	uint32 what;
	uint32 value;
	uint32 buffer;
};

void CNetwork::RunTests()
{
#if 0
/*
	for (int SZ=15; SZ<10000; SZ*=1.5f)
	{
		CDefaultStreamAllocator alloc;
		CCommOutputStream os(&alloc, 1024);
		CArithLargeAlphabetOrder0<21,15> alphabet(SZ);

		for (int j=0; j<100000; j++)
			alphabet.WriteSymbol(os, cry_rand32()%SZ);
		for (int j=0; j<100000; j++)
			alphabet.WriteSymbol(os, SZ/2);
	}
*/
	std::vector<SAlphabetTestOp> ops;
	static const int INISZ = 20;
	CDefaultStreamAllocator alloc;
	CCommOutputStream os(&alloc, 1024);
	typedef CArithLargeAlphabetOrder0<21,15> TA;
	typedef std::vector<TA> VTA;
	VTA alphas;
	TA a(INISZ);
	for (int i=0; i<100; i++)
	 alphas.push_back(a);
	VTA alphas0 = alphas;
	for (int i=0; i<1000; i++)
	{
		SAlphabetTestOp op;
		switch (cry_rand32()%10)
		{
		case 0:
			op.what = 0;
			op.buffer = cry_rand32() % alphas.size();
			op.value = 5 + cry_rand32() % 10000;
			alphas[op.buffer].Resize(op.value);
			break;
		case 1:
			op.what = 1;
			op.buffer = cry_rand32() % alphas.size();
			do 
			{
				op.value = cry_rand32() % alphas.size();
			} while(op.value == op.buffer);
			alphas[op.buffer] = alphas[op.value];
			break;
		default:
			op.what = 2;
			op.buffer = cry_rand32() % alphas.size();
			op.value = cry_rand32() % alphas[op.buffer].GetNumSymbols();
			alphas[op.buffer].WriteSymbol(os, op.value);
			break;
		}
		ops.push_back(op);
	}
	os.Flush();
	alphas = alphas0;
	CCommInputStream is(os.GetPtr(), os.GetOutputSize());
	for (std::vector<SAlphabetTestOp>::iterator it = ops.begin(); it != ops.end(); ++it)
	{
		switch (it->what)
		{
		case 0:
			alphas[it->buffer].Resize(it->value);
			break;
		case 1:
			alphas[it->buffer] = alphas[it->value];
			break;
		case 2:
			{
				uint32 r = alphas[it->buffer].ReadSymbol(is);
				NET_ASSERT(r == it->value);
			}
			break;
		}
	}
#endif

#if 0
	CStressTestMMM().Run(1000000);
#endif

#if 0
	CSegmentedCompressionSpace scs;
	scs.Load("scripts/network/wrld.xml");
	CDefaultStreamAllocator dsa;
	CCommOutputStream os(&dsa, 1024);
	int32 test[3] = {-11154, -197, -122};
	scs.Encode(os, test, 3);
#endif

#if 0
	for (int loop=0; loop<5000; loop++)
	{
		int sizes[] = {
			1, 2, 4, 3, 5, 8, 12, 16, 83, 11, 97
		};
		static const int nsizes = sizeof(sizes) / sizeof(*sizes);
		for (int i=0; i<nsizes; i++)
			std::swap(sizes[rand()%nsizes], sizes[rand()%nsizes]);
		CDefaultStreamAllocator alloc;
		CByteOutputStream os(&alloc,1);
		for (int i=0; i<nsizes*20; i++)
		{
			switch (sizes[i%nsizes])
			{
			case 1:
				os.PutByte('x');
				break;
			case 2:
				os.PutTyped<uint16>() = 1824;
				break;
			case 4:
				os.PutTyped<uint32>() = 19283442;
				break;
			case 12:
				os.PutTyped<Vec3>() = Vec3(0,1,2);
				break;
			case 16:
				os.PutTyped<Quat>() = IDENTITY;
				break;
			default:
				{
					uint8 * temp = new uint8[sizes[i%nsizes]];
					for (int j=0; j<sizes[i%nsizes]; j++)
						temp[j] = j;
					os.Put( temp, sizes[i%nsizes] );
					delete[] temp;
				}
			}
		}
		CByteInputStream is( os.GetBuffer(), os.GetSize() );
		for (int i=0; i<nsizes*20; i++)
		{
			switch (sizes[i%nsizes])
			{
			case 1:
				NET_ASSERT( is.GetByte() == 'x' );
				break;
			case 2:
				NET_ASSERT( is.GetTyped<uint16>() == 1824 );
				break;
			case 4:
				NET_ASSERT( is.GetTyped<uint32>() == 19283442 );
				break;
			case 12:
				NET_ASSERT( is.GetTyped<Vec3>() == Vec3(0,1,2) );
				break;
			case 16:
				NET_ASSERT( is.GetTyped<Quat>().IsIdentity() );
				break;
			default:
				{
					uint8 * temp = new uint8[sizes[i%nsizes]];
					for (int j=0; j<sizes[i%nsizes]; j++)
						temp[j] = j;
					NET_ASSERT( 0 == memcmp(temp, is.Get( sizes[i%nsizes] ), sizes[i%nsizes]) );
					delete[] temp;
				}
			}
		}
	}
#endif

#if 0
	for (int i=0; i<1000000; i++)
	{
		switch (rand()%10)
		{
		case 0:
			testq.Add(CTestPolyQueue<10>(i));
			break;
		case 1:
			testq.Add(CTestPolyQueue<1>(i));
			break;
		case 2:
			testq.Add(CTestPolyQueue<2>(i));
			break;
		case 3:
			testq.Add(CTestPolyQueue<3>(i));
			break;
		case 4:
			testq.Add(CTestPolyQueue<4>(i));
			break;
		case 5:
			testq.Add(CTestPolyQueue<5>(i));
			break;
		case 6:
			testq.Add(CTestPolyQueue<6>(i));
			break;
		case 7:
			testq.Add(CTestPolyQueue<7>(i));
			break;
		case 8:
			testq.Add(CTestPolyQueue<8>(i));
			break;
		case 9:
			testq.Add(CTestPolyQueue<9>(i));
			break;
		}
		if (rand()%100000 == 0)
			testq.Flush( TestPolyQueue );
	}
#endif

	/*
	CDefaultStreamAllocator dsa;
	for (size_t i=0; i<10000000u; i++)
	{
	CCommOutputStream os(&dsa, 1024);
	std::vector<ImprovedTest> check;
	for (size_t j=0; j<500; j++)
	{
	ImprovedTest t;
	t.nHeight = rand() % 2048 + 1;
	t.nMaxBits = rand() % 22 + 2;
	t.nRange = (1 << t.nMaxBits)-1;
	t.nInRangePercentage = rand()%98 + 1;
	t.nLeft = rand() % t.nRange;
	t.nRight = rand() % t.nRange;
	if (t.nLeft > t.nRight)
	std::swap(t.nLeft, t.nRight);
	t.nValue = rand() % t.nRange;
	check.push_back(t);
	SquarePulseProbabilityWriteImproved( os, t.nValue, t.nLeft, t.nRight, t.nHeight, t.nRange, t.nInRangePercentage, t.nMaxBits );
	}
	os.Flush();
	CCommInputStream is(os.GetPtr(), os.GetOutputSize());
	for (size_t j=0; j<check.size(); j++)
	{
	ImprovedTest& t = check[j];
	uint32 x = SquarePulseProbabilityReadImproved( is, t.nLeft, t.nRight, t.nHeight, t.nRange, t.nInRangePercentage, t.nMaxBits );
	while (x != t.nValue)
	{
	CryError("Bad Format");
	uint8 buf[512];
	CCommOutputStream os2(buf, 512);
	SquarePulseProbabilityWriteImproved( os2, t.nValue, t.nLeft, t.nRight, t.nHeight, t.nRange, t.nInRangePercentage, t.nMaxBits );
	os2.Flush();
	CCommInputStream is2(buf, os2.GetOutputSize());
	uint32 x2 = SquarePulseProbabilityReadImproved( is2, t.nLeft, t.nRight, t.nHeight, t.nRange, t.nInRangePercentage, t.nMaxBits );
	x = x2;
	}
	}
	}
	*/

	/*
	IVoiceEncoder * pV = CVoiceManager::CreateEncoder( GetVoiceCodec() );
	IVoiceDecoder * pW = CVoiceManager::CreateDecoder( GetVoiceCodec() );
	if (pV && pW)
	{
	int frameSize = pV->GetFrameSize();

	std::vector<short> buffer(frameSize);
	for (int i=0; i<frameSize; i++)
	buffer[i] = 8000 * sin(0.5 * i);
	uint8 temp[1024];

	pW->DecodeFrame( pV->EncodeFrame( frameSize, &buffer[0], 1024, temp ), temp, frameSize, &buffer[0] );

	pV->Release();
	}
	*/

}

CNetChannel * CNetwork::FindFirstClientChannel()
{
	for (VMembers::const_iterator iter = m_vMembers.begin(); iter != m_vMembers.end(); ++iter)
		if (CNetChannel * pChan = (*iter)->FindFirstClientChannel())
			return pChan;
	return 0;
}

// NOTE: the following functions cannot call isPbClSvEnabled() function directly, since they tend to
// load PB dll's if not loaded, and that's not something we want (what we are doing is load PB dll
// only during PB enabled multiplayer sessions)

bool CNetwork::IsPbClEnabled()
{
	return m_isPbClActive;
}

bool CNetwork::IsPbSvEnabled()
{
	return m_isPbSvActive;
}

void CNetwork::StartupPunkBuster(bool server)
{
#ifdef __WITH_PB__
	PBsdk_SetPointers();

	if (server)
	{
		PbServerInitialize();
		EnablePbSv();
		m_isPbSvActive = true;
	}
	else
	{
		PbClientInitialize(NULL);
		EnablePbCl();
		m_isPbClActive = true;
	}
#endif
}

void CNetwork::CleanupPunkBuster()
{
#ifdef __WITH_PB__
	PbShutdown();
	m_isPbSvActive = false;
	m_isPbClActive = false;
#endif
}

bool CNetwork::IsPbInstalled()
{
#ifdef __WITH_PB__
	return isPbInstalled() != 0;
#else
	return false;
#endif
}

