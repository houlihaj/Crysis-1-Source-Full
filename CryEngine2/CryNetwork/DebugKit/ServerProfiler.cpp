#include "StdAfx.h"
#include "ServerProfiler.h"

#if defined(WIN32) && !defined(WIN64)

#include "PsApi.h"

extern uint64 g_bytesIn, g_bytesOut;
extern int g_nChannels;

bool CServerProfiler::m_saveAndCrash = false;

static float ftdiff( const FILETIME& b, const FILETIME& a )
{
	uint64 aa = *reinterpret_cast<const uint64*>(&a);
	uint64 bb = *reinterpret_cast<const uint64*>(&b);
	return (bb - aa)*1e-7f;
}

CServerProfiler * CServerProfiler::m_pThis;

static const float TICK = 5.0f;

void CServerProfiler::Init()
{
	NET_ASSERT(m_pThis == 0);
	m_pThis = new CServerProfiler;
	if (!m_pThis->m_fout)
		Cleanup();
	else
		atexit(Cleanup);
}

void CServerProfiler::Cleanup()
{
	NET_ASSERT(m_pThis);
	SAFE_DELETE(m_pThis);
}

CServerProfiler::CServerProfiler()
{
	SCOPED_GLOBAL_LOCK;

	m_timer = 0;

	m_timer = TIMER.AddTimer( g_time + TICK, TimerCallback, this );
	string filename = gEnv->pSystem->GetRootFolder();
	filename += "server_profile.txt";
	if (m_fout = fxopen( filename.c_str(), "wt" ))
		fprintf(m_fout, "cpu\tmemory\tcommitted\tbwin\tbwout\tnchan\n");

	GetSystemTimeAsFileTime( &m_lastTime );
	FILETIME notNeeded;
	GetProcessTimes( GetCurrentProcess(), &notNeeded, &notNeeded, &m_lastKernel, &m_lastUser );
	m_lastIn = g_bytesIn;
	m_lastOut = g_bytesOut;
}

CServerProfiler::~CServerProfiler()
{
	SCOPED_GLOBAL_LOCK;
	TIMER.CancelTimer(m_timer);
	fclose(m_fout);
}

void CServerProfiler::Tick()
{
	FILETIME kernel, user, cur;
	FILETIME notNeeded;
	GetSystemTimeAsFileTime( &cur );
	GetProcessTimes( GetCurrentProcess(), &notNeeded, &notNeeded, &kernel, &user );

	float sKernel = ftdiff(kernel, m_lastKernel);
	float sUser = ftdiff(user, m_lastUser);
	float sCur = ftdiff(cur, m_lastTime);

	float cpu = 100*(sKernel+sUser)/sCur;

	PROCESS_MEMORY_COUNTERS memcnt;
	memcnt.cb = sizeof(memcnt);
	GetProcessMemoryInfo( GetCurrentProcess(), &memcnt, sizeof(memcnt) );

	float megs = memcnt.WorkingSetSize / 1024.0f / 1024.0f;
	float commitMegs = memcnt.PagefileUsage / 1024.0f / 1024.0f;

	float in = (g_bytesIn - m_lastIn) / sCur / 1024.0f;
	float out = (g_bytesOut - m_lastOut) / sCur / 1024.0f;

	fprintf(m_fout, "%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%d\n", cpu, megs, commitMegs, in, out, g_nChannels);
	fflush(m_fout);

	if (CNetCVars::Get().MaxMemoryUsage && commitMegs > CNetCVars::Get().MaxMemoryUsage)
	{
		m_saveAndCrash = true;
	}

	m_lastTime = cur;
	m_lastKernel = kernel;
	m_lastUser = user;
	m_lastIn = g_bytesIn;
	m_lastOut = g_bytesOut;
}

void CServerProfiler::TimerCallback( NetTimerId, void *, CTimeValue )
{
	m_pThis->Tick();
	TIMER.AddTimer( g_time + TICK, TimerCallback, m_pThis );
}

#endif
