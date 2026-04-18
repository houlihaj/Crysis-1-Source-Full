/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  implementation of INetwork
 -------------------------------------------------------------------------
 History:
 - 26/07/2004   : Created by Craig Tiller
 !F RCON system - lluo
*************************************************************************/

#ifndef __NETWORK_H__
#define __NETWORK_H__

#pragma once

#include <vector>
#include <memory>
#include "TimeValue.h"
#include "Protocol/StatsCollector.h"
#include "Errors.h"
#include "Socket/NetAddress.h"
#include "INetwork.h"
#include "WorkQueue.h"
#include "NetTimer.h"
#include "Config.h"
#include "MementoMemoryManager.h"
#include "NetCVars.h"
#include "NetLog.h"
#include "CryThread.h"
#include "Protocol/MessageQueue.h"
#include "Socket/IStreamSocket.h"
#include "Socket/IDatagramSocket.h"
#include "Socket/ISocketIOManager.h"

#include "Cryptography/Whirlpool.h"
#include "Protocol/ExponentialKeyExchange.h"
#include "Compression/CompressionManager.h"

#include "Socket/WatchdogTimer.h"

#if defined(WIN32) || defined(WIN64)
#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>
# pragma comment(lib, "wbemuuid.lib")
#endif

struct ICVar;
class CNetNub;
class CNetContext;
struct INetworkMember;
struct IConsoleCmdArgs;
class CNetworkInspector;
class CServiceManager;
#if ENABLE_DISTRIBUTED_LOGGER
class CDistributedLogger;
#endif

struct SSystemBranchCounters
{
	SSystemBranchCounters()
	{
		memset(this, 0, sizeof(*this));
	}
	uint32 updateTickMain;
	uint32 updateTickSkip;
	uint32 iocpReapImmediate;
	uint32 iocpReapImmediateCollect;
	uint32 iocpReapSleep;
	uint32 iocpReapSleepCollect;
	uint32 iocpIdle;
	uint32 iocpForceSync;
	uint32 iocpBackoffCheck;
};

extern SSystemBranchCounters g_systemBranchCounters;

enum EMementoMemoryType
{
	eMMT_Memento = 0,
	eMMT_PacketData,
	eMMT_ObjectData,
	eMMT_Alphabet,
	eMMT_NUM_TYPES
};

template <class T>
class NetMutexLock
{
public:
	NetMutexLock( T& mtx, bool lock ) : m_mtx(mtx), m_lock(lock) { if (m_lock) m_mtx.Lock(); }
	~NetMutexLock() { if (m_lock) m_mtx.Unlock(); }

private:
	T& m_mtx;
	bool m_lock;
};

typedef CryLock<CRYLOCK_RECURSIVE> NetFastMutex;

enum EDebugMemoryMode
{
	eDMM_Context = 1,
	eDMM_Endpoint = 2,
	eDMM_Mementos = 4,
};

enum EDebugBandwidthMode
{
	eDBM_None = 0,
	eDBM_AspectAgeList = 1,
	eDBM_AspectAgeMap = 2,
	eDBM_PriorityMap = 3,
};

const char * GetTimestampString();

extern CTimeValue g_time;

class CNetwork : public INetwork
{
public:
	//! constructor
	CNetwork();
	//! destructor
	virtual ~CNetwork();

	bool Init( int ncpu );

	// interface INetwork -------------------------------------------------------------

	virtual bool HasNetworkConnectivity();
	virtual INetNub * CreateNub( const char * address, 
		IGameNub * pGameNub, 
		IGameSecurity * pSecurity,
		IGameQuery * pGameQuery );
	virtual ILanQueryListener * CreateLanQueryListener( IGameQueryListener * pGameQueryListener );
	virtual INetContext * CreateNetContext( IGameContext * pGameContext, uint32 flags );
	virtual const char *EnumerateError(NRESULT err);
	virtual void Release();
	virtual void GetMemoryStatistics(ICrySizer *pSizer);
	virtual void SyncWithGame(ENetworkGameSync);
	virtual const char * GetHostName();
	virtual uint32 GetHostLocalIp();
	virtual INetworkServicePtr GetService( const char * name );
	virtual void FastShutdown();
  virtual void SetCDKey(const char* key);
	virtual bool HaveCDKey() { return (m_CDKey.empty() == false); }
	virtual const char* GetCDKey() { return m_CDKey.c_str(); }
	virtual bool PbConsoleCommand(const char *, int length);   // EvenBalance - M. Quinn
	virtual void PbCaptureConsoleLog(const char* output, int length);   // EvenBalance - M. Quinn
	virtual void PbServerAutoComplete(const char *, int length);   // EvenBalance - M. Quinn
	virtual void PbClientAutoComplete(const char *, int length);   // EvenBalance - M. Quinn
	virtual void SetNetGameInfo( SNetGameInfo );
	virtual SNetGameInfo GetNetGameInfo();

	virtual bool IsPbClEnabled();
	virtual bool IsPbSvEnabled();

	virtual void StartupPunkBuster(bool server);
	virtual void CleanupPunkBuster();

	virtual bool IsPbInstalled();

	virtual IRemoteControlSystem* GetRemoteControlSystemSingleton();

	virtual ISimpleHttpServer* GetSimpleHttpServerSingleton();

	// nub helpers
	int GetLogLevel();
	ILINE CNetAddressResolver * GetResolver() { return m_pResolver; }
	ILINE CWorkQueue& GetToGameQueue() { return m_toGame; }
	ILINE CWorkQueue& GetToGameLazyQueue() { return m_toGameLazyBuilding; }
	ILINE CWorkQueue& GetFromGameQueue() { return m_fromGame; }
	ILINE CWorkQueue& GetInternalQueue() { return m_intQueue; }
	ILINE CNetTimer& GetTimer() { return m_timer; }
	ILINE const CNetCVars& GetCVars() { return m_cvars; }
	ILINE CMessageQueue::CConfig * GetMessageQueueConfig() const { return m_pMessageQueueConfig; }
	ILINE CCompressionManager& GetCompressionManager() { return m_compressionManager; }
	ILINE ISocketIOManager& GetSocketIOManager() { return *m_pSocketIOManager; }
	int GetMessageQueueConfigVersion() const { return m_schedulerVersion; }

	CTimeValue GetGameTime() { return m_gameTime; }

	void ReportGotPacket() { m_detection.ReportGotPacket(); }

	ILINE CStatsCollector * GetStats()  
	{ 
		static CStatsCollector m_stats("netstats.log");
		return &m_stats; 
	}

#if ENABLE_DEBUG_KIT
	ILINE CNetworkInspector* GetNetInspector()
	{
		return m_pNetInspector;
	}
#endif

	void WakeThread();

	static ILINE CNetwork * Get() { return m_pThis; }
	ILINE NetFastMutex& GetMutex() { return m_mutex; }
	ILINE NetFastMutex& GetCommMutex() { return m_commMutex; }
	ILINE NetFastMutex& GetLogMutex() { return m_logMutex; }

	ILINE bool IsMultithreaded() const { return m_isMultithreaded; }
	void EnableMultithreading( bool enable );

	void ReloadScheduler();

	CNetChannel * FindFirstClientChannel();

  CServiceManager* GetServiceManager(){return m_pServiceManager.get();}

	// fast lookup of channels (mainly for punkbuster)
	int RegisterForFastLookup(CNetChannel * pChannel);
	void UnregisterFastLookup(int id);
	CNetChannel * GetChannelByFastLookupId(int id);

	void AddExecuteString( const string& toExec );

	void BroadcastNetDump( ENetDumpType );

	// small hack to make adding thigns to the from game queue in general efficient
#define ADDTOFROMGAMEQUEUE_BODY(params) if (IsPrimaryThread()) m_fromGame.Add params; else { CryAutoLock<NetFastMutex> lk(m_fromGame_otherThreadLock); m_fromGame_otherThreadQueue.Add params; }
	template <class A> void AddToFromGameQueue( const A& a )
	{ ADDTOFROMGAMEQUEUE_BODY((a)); }
	template <class A, class B> void AddToFromGameQueue( const A& a, const B& b )
	{ ADDTOFROMGAMEQUEUE_BODY((a, b)); }
	template <class A, class B, class C> void AddToFromGameQueue( const A& a, const B& b, const C& c )
	{ ADDTOFROMGAMEQUEUE_BODY((a, b, c)); }
	template <class A, class B, class C, class D> void AddToFromGameQueue( const A& a, const B& b, const C& c, const D& d )
	{ ADDTOFROMGAMEQUEUE_BODY((a, b, c, d)); }
	template <class A, class B, class C, class D, class E> void AddToFromGameQueue( const A& a, const B& b, const C& c, const D& d, const E& e )
	{ ADDTOFROMGAMEQUEUE_BODY((a, b, c, d, e)); }
	template <class A, class B, class C, class D, class E, class F> void AddToFromGameQueue( const A& a, const B& b, const C& c, const D& d, const E& e, const F& f )
	{ ADDTOFROMGAMEQUEUE_BODY((a, b, c, d, e, f)); }
	template <class A, class B, class C, class D, class E, class F, class G> void AddToFromGameQueue( const A& a, const B& b, const C& c, const D& d, const E& e, const F& f, const G& g)
	{ ADDTOFROMGAMEQUEUE_BODY((a, b, c, d, e, f, g)); }

private:
	static CNetwork * m_pThis;

	// NOTE: NetCVars is the most dependent component in CryNetwork, it should be constructed before any other CryNetwork components and
	// destructed after all other CryNetwork components get destroyed (the order of member contructions is the order of declarations of
	// the class, and the order of member destruction is the reverse order of member constructions)
	CNetCVars m_cvars;

	void LogNetworkInfo();
	void DoSyncWithGame(ENetworkGameSync);
	void AddMember( INetworkMemberPtr pMember );
	// will all of our members will be dead soon?
	bool AllSuicidal();
	void UpdateLoop( ENetworkGameSync sync );
#if ENABLE_DISTRIBUTED_LOGGER
	std::auto_ptr<CDistributedLogger> m_pLogger;
#endif

	typedef std::map<NRESULT,string> TErrorMap;
	typedef std::vector<INetworkMemberPtr> VMembers;

	struct SNetError
	{
		NRESULT nrErrorCode; 
		const char *sErrorDescription;
	};

	TErrorMap               m_mapErrors;
	static SNetError        m_neNetErrors[];
	VMembers                m_vMembers;

	bool                    m_bQuitting;

	CNetAddressResolver * m_pResolver;
#if ENABLE_DEBUG_KIT
	CNetworkInspector *m_pNetInspector;
#endif
	CWorkQueue       m_toGame;
	CWorkQueue       m_fromGame;
	NetFastMutex     m_fromGame_otherThreadLock;
	CWorkQueue       m_fromGame_otherThreadQueue;
	CWorkQueue       m_intQueue;
	CWorkQueue       m_toGameLazyBuilding;
	CWorkQueue       m_toGameLazyProcessing;
	CNetTimer        m_timer;
	CCompressionManager m_compressionManager;
	CMessageQueue::CConfig * m_pMessageQueueConfig;
	int m_schedulerVersion;
	int m_inSync[eNGS_NUM_ITEMS+2]; // one extra for the continuous update loop, and the other for flushing the lazy queue

	std::auto_ptr<CServiceManager> m_pServiceManager;

	std::vector<CNetChannel*> m_vFastChannelLookup;

	class CNetworkThread;
	std::auto_ptr<CNetworkThread> m_pThread;
	NetFastMutex m_mutex;
	NetFastMutex m_commMutex;
	NetFastMutex m_logMutex;

  CTimeValue m_gameTime;
  
  string m_CDKey;

	class CBufferWakeups;
	int m_bufferWakeups;
	bool m_wokenUp;

	bool m_isMultithreaded;
	int m_occasionalCounter;
	int m_cleanupMember;
	CTimeValue m_nextCleanup;
	std::vector<string> m_consoleCommandsToExec;
	ISocketIOManager * m_pSocketIOManager;
	int m_cpuCount;

	CTimeValue m_lastUpdateLock;
	bool m_forceLock;

	SNetGameInfo m_gameInfo;

	void RunTests();
	bool UpdateTick( bool mt );

	bool m_isPbSvActive;
	bool m_isPbClActive;

	class CNetworkConnectivityDetection
	{
	public:
		CNetworkConnectivityDetection() : m_hasNetworkConnectivity(true), m_lastCheck(0.0f), m_lastPacketReceived(0.0f) {}

		bool HasNetworkConnectivity();
		void ReportGotPacket() { m_lastPacketReceived = MAX( g_time, m_lastPacketReceived ); }

		void AddRef() {}
		void Release() {}
		bool IsDead() { return false; }

	private:
		bool m_hasNetworkConnectivity;
		CTimeValue m_lastCheck;
		CTimeValue m_lastPacketReceived;

		void DetectNetworkConnectivity();
	};

	CNetworkConnectivityDetection m_detection; // NOTE: this relies on NetTimer and WorkQueue objects to function correctly, so it must be declared/defined after them
};


#if defined(NDEBUG) || defined(PS3)
# define ASSERT_MUTEX_LOCKED(mtx)
# define ASSERT_MUTEX_UNLOCKED(mtx)
#else
# define ASSERT_MUTEX_LOCKED(mtx) if (CNetwork::Get()->IsMultithreaded()) NET_ASSERT((mtx).IsLocked())
# define ASSERT_MUTEX_UNLOCKED(mtx) if (CNetwork::Get()->IsMultithreaded()) NET_ASSERT(!(mtx).IsLocked())
#endif

#define ASSERT_GLOBAL_LOCK ASSERT_MUTEX_LOCKED(CNetwork::Get()->GetMutex())
#define ASSERT_COMM_LOCK ASSERT_MUTEX_LOCKED(CNetwork::Get()->GetCommMutex())

extern int g_lockCount;
#define SCOPED_GLOBAL_LOCK_NO_LOG \
	ASSERT_MUTEX_UNLOCKED(CNetwork::Get()->GetCommMutex()); \
	NetMutexLock<NetFastMutex> globalLock(CNetwork::Get()->GetMutex(), CNetwork::Get()->IsMultithreaded()); \
	g_lockCount++; \
	CAutoUpdateWatchdogCounters updateWatchdogTimers
#if LOG_LOCKING
# define SCOPED_GLOBAL_LOCK SCOPED_GLOBAL_LOCK_NO_LOG; NetLog("lock %s", __FUNCTION__) ; ENSURE_REALTIME
#else
# define SCOPED_GLOBAL_LOCK SCOPED_GLOBAL_LOCK_NO_LOG ; ENSURE_REALTIME
#endif

// should be used in the NetLog functions only
#define SCOPED_GLOBAL_LOG_LOCK CryAutoLock<NetFastMutex> globalLogLock(CNetwork::Get()->GetLogMutex())
#define SCOPED_COMM_LOCK NetMutexLock<NetFastMutex> commMutexLock(CNetwork::Get()->GetCommMutex(), CNetwork::Get()->IsMultithreaded())

// never place these macros in a block without braces...
// BAD: if(x) TO_GAME(blah);
// GOOD: if(x) {TO_GAME(blah);}

// pass a message callback from network system to game, to execute during SyncToGame
// must hold the global lock
// global lock will be held in the callback
#define TO_GAME ASSERT_GLOBAL_LOCK; CNetwork::Get()->GetToGameQueue().Add
// pass a message callback from network system to game, to execute in parallel with the network thread
// global lock will *NOT* be held in the callback
#define TO_GAME_LAZY ASSERT_GLOBAL_LOCK; CNetwork::Get()->GetToGameLazyQueue().Add
// pass a message from the game to the network; 
// not thread-safe!
// the game must guarantee that its calls to the network system are synchronized
// the network code must guarantee that it never calls FROM_GAME in one of its threads
// global lock will be held in the callback
#define FROM_GAME CNetwork::Get()->AddToFromGameQueue
// pass a message from the network engine to the network engine to be executed later
// must hold the global lock 
// global lock will be held in the callback
#define NET_TO_NET ASSERT_GLOBAL_LOCK; CNetwork::Get()->WakeThread(); CNetwork::Get()->GetInternalQueue().Add

#define RESOLVER (*CNetwork::Get()->GetResolver())
#define TIMER CNetwork::Get()->GetTimer()
#define CVARS CNetwork::Get()->GetCVars()
#define NET_INSPECTOR (*CNetwork::Get()->GetNetInspector())
#define STATS (*CNetwork::Get()->GetStats())

class CMementoMemoryRegion
{
public:
	ILINE CMementoMemoryRegion( CMementoMemoryManager * pMMM )
	{
		ASSERT_GLOBAL_LOCK;
		m_pPrev = m_pMMM;
		m_pMMM = pMMM;
	}

	ILINE ~CMementoMemoryRegion()
	{
		ASSERT_GLOBAL_LOCK;
		m_pMMM = m_pPrev;
	}

	static ILINE CMementoMemoryManager& Get()
	{
		assert(m_pMMM);
		return *m_pMMM;
	}

private:
	CMementoMemoryRegion( const CMementoMemoryRegion& );
	CMementoMemoryRegion& operator=( const CMementoMemoryRegion& );

	CMementoMemoryManager * m_pPrev;
	static CMementoMemoryManager * m_pMMM;
};

#define MMM_REGION(pMMM) CMementoMemoryRegion _mmmrgn(pMMM)

ILINE CMementoMemoryManager& MMM()
{
	return CMementoMemoryRegion::Get();
}

#endif
