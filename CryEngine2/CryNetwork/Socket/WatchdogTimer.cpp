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

#include "StdAfx.h"
#include "WatchdogTimer.h"
#include "CryThread.h"
#include "Network.h"

volatile uint32 g_watchdogTimerGlobalLockCount = 0;
volatile uint32 g_watchdogTimerLockedCounter = 0;

namespace
{
	enum EAction
	{
		eA_RegisterTarget,
		eA_UnregisterTarget,
	};

	struct SAction
	{
		EAction action;
		SOCKET sock;
		TNetAddress addr;
	};
}

class CWatchdogTimer::CTimerThread : public CrySimpleThread<>
{
public:
	CTimerThread() : m_done(false), m_stallDetected(false) {}

	void Run()
	{
		CryThreadSetName( -1,"NetworkWatchdog" );

		NetFastMutex& commMtx = CNetwork::Get()->GetCommMutex();
		uint32 lastSeenGlobalLockCount = 0;

		bool possiblyStalled = false;

		while (true)
		{
			{
				CryAutoLock<TMutex> lk(m_mutex);
				m_cond.TimedWait(m_mutex, 500);
				if (m_done)
					break;
			}

			CryAutoLock<NetFastMutex> lk(commMtx);

			if (!m_actions.empty())
			{
				for (std::vector<SAction>::const_iterator it = m_actions.begin(); it != m_actions.end(); ++it)
				{
					STarget tgt;
					switch (it->action)
					{
					case eA_RegisterTarget:
						tgt.sock = it->sock;
						tgt.addr = it->addr;
						stl::push_back_unique(m_target, tgt);
						break;
					case eA_UnregisterTarget:
						tgt.sock = it->sock;
						tgt.addr = it->addr;
						stl::find_and_erase(m_target, tgt);
						break;
					}
				}
				m_actions.resize(0);
			}

			if (g_watchdogTimerLockedCounter && lastSeenGlobalLockCount == g_watchdogTimerGlobalLockCount)
			{
				if (possiblyStalled)
				{
					m_stallDetected = true;
					SendKeepAlives();
				}
				else
					possiblyStalled = true;
			}
			else
			{
				possiblyStalled = false;
				lastSeenGlobalLockCount = g_watchdogTimerGlobalLockCount;
			}
		}
	}
	void Cancel()
	{
		CryAutoLock<TMutex> lk(m_mutex);
		m_done = true;
		m_cond.Notify();
	}

	void QueueAction( const SAction& action )
	{
		SCOPED_COMM_LOCK;
		m_actions.push_back(action);
	}

	ILINE bool HasStalled() const
	{
		return m_stallDetected;
	}

	ILINE void ClearStalls()
	{
		m_stallDetected = false;
	}

private:
	typedef CryCondLock<CRYLOCK_FAST> TMutex;
	typedef CryCond<TMutex> TCond;

	TMutex m_mutex;
	TCond m_cond;
	volatile bool m_done;
	volatile bool m_stallDetected;

	std::vector<SAction> m_actions;

	struct STarget
	{
		SOCKET sock;
		TNetAddress addr;

		bool operator ==(const STarget& rhs) const
		{
			return sock == rhs.sock && addr == rhs.addr;
		}
	};

	std::vector<STarget> m_target;

	void SendKeepAlives()
	{
		for (std::vector<STarget>::iterator it = m_target.begin(); it != m_target.end(); ++it)
		{
			char addrBuf[_SS_MAXSIZE];
			int addrSize = _SS_MAXSIZE;

			if (ConvertAddr(it->addr, (sockaddr*)addrBuf, &addrSize))
				sendto(it->sock, (const char *)(Frame_IDToHeader + eH_BackOff), 1, 0, (sockaddr*)addrBuf, addrSize);
		}
	}
};

CWatchdogTimer::CWatchdogTimer()
{
	m_pThread.reset( new CTimerThread );
	m_pThread->Start();
}

CWatchdogTimer::~CWatchdogTimer()
{
	m_pThread->Cancel();
	m_pThread->Join();
	m_pThread.reset();
}

void CWatchdogTimer::RegisterTarget( SOCKET sock, const TNetAddress& addr )
{
	SAction action;
	action.action = eA_RegisterTarget;
	action.sock = sock;
	action.addr = addr;
	m_pThread->QueueAction(action);
}

void CWatchdogTimer::UnregisterTarget( SOCKET sock, const TNetAddress& addr )
{
	SAction action;
	action.action = eA_UnregisterTarget;
	action.sock = sock;
	action.addr = addr;
	m_pThread->QueueAction(action);
}

bool CWatchdogTimer::HasStalled() const
{
	return m_pThread->HasStalled();
}

void CWatchdogTimer::ClearStalls()
{
	m_pThread->ClearStalls();
}
