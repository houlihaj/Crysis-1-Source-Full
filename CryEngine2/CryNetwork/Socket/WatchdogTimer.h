//////////////////////////////////////////////////////////////////////
//
//	Crytek Network source code
//	
//	File: CTPEndpoint.cpp
//  Description: a thread that sends keep-alive notifications to registered sockets in the event that the global lock is held for a very long time
//
//	History:
//	-August 24,2007:Created by Craig Tiller
//
//////////////////////////////////////////////////////////////////////

#ifndef __WATCHDOGTIMER_H__
#define __WATCHDOGTIMER_H__

#pragma once

#include "NetAddress.h"

extern volatile uint32 g_watchdogTimerGlobalLockCount;
extern volatile uint32 g_watchdogTimerLockedCounter;

class CAutoUpdateWatchdogCounters
{
public:
	ILINE CAutoUpdateWatchdogCounters()
	{
		++g_watchdogTimerGlobalLockCount;
		++g_watchdogTimerLockedCounter;
	}
	ILINE ~CAutoUpdateWatchdogCounters()
	{
		--g_watchdogTimerLockedCounter;
	}
};

class CWatchdogTimer
{
public:
	CWatchdogTimer();
	~CWatchdogTimer();

	void RegisterTarget( SOCKET sock, const TNetAddress& addr );
	void UnregisterTarget( SOCKET sock, const TNetAddress& addr );

	bool HasStalled() const;
	void ClearStalls();

private:
	class CTimerThread;
	std::auto_ptr<CTimerThread> m_pThread;
};

#endif
