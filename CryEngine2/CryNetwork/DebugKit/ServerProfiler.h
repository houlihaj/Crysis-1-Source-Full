#ifndef __SERVERPROFILER_H__
#define __SERVERPROFILER_H__

#pragma once

#if defined(WIN32) && !defined(WIN64)

#include "Network.h"

class CServerProfiler
{
public:
	static void Init();

	static bool ShouldSaveAndCrash() { return m_saveAndCrash; }

private:
	CServerProfiler();
	~CServerProfiler();

	static CServerProfiler * m_pThis;
	FILE * m_fout;
	NetTimerId m_timer;

	FILETIME m_lastKernel, m_lastUser, m_lastTime;
	uint64 m_lastIn, m_lastOut;

	static bool m_saveAndCrash;

	static void TimerCallback( NetTimerId, void *, CTimeValue );
	void Tick();
	static void Cleanup();
};

#else // !WIN32

class CServerProfiler
{
public:
	static void Init() {}
	static bool ShouldSaveAndCrash() { return false; }
};

#endif // WIN32

#endif
