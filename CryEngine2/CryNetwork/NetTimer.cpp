#include "StdAfx.h"
#include "NetTimer.h"
#include "Network.h"

CNetTimer::CNetTimer() : m_curSlot(0)
{
	// null callback
	m_callbacks.push_back(SCallbackInfo());

	for (int i=0; i<TIMER_SLOTS; i++)
		m_timerCallbacks[i] = -1;

#if TIMER_DEBUG
	m_epoch = 0.0f;
#endif
}

NetTimerId CNetTimer::AddTimer( CTimeValue when, NetTimerCallback callback, void * pUserData )
{
	ASSERT_GLOBAL_LOCK;

	if (when < m_lastExec)
	{
#if TIMER_DEBUG
		NetWarning("NetTimer: Timer added backwards in time!");
#endif
		when = m_lastExec;
	}

	int slotsInFuture = (int)((when - m_lastExec).GetSeconds() * TIMER_HERTZ);
	slotsInFuture += !slotsInFuture;

	int id = -1;
	if (m_freeCallbacks.empty())
	{
		id = m_callbacks.size();
		m_callbacks.push_back(SCallbackInfo());
	}
	else
	{
		id = m_freeCallbacks.back();
		m_freeCallbacks.pop_back();
	}

	SCallbackInfo& info = m_callbacks[id];
	info.callback = callback;
	info.pUserData = pUserData;
	info.cancelled = false;
#if TIMER_DEBUG
	info.schedTime = when;
	info.slotsInFuture = slotsInFuture;
	info.curSlotWhenScheduled = m_curSlot;
#endif

	if (slotsInFuture < TIMER_SLOTS)
	{
		int slot = (m_curSlot + slotsInFuture) % TIMER_SLOTS;
		info.next = m_timerCallbacks[slot];
		m_timerCallbacks[slot] = id;
	}
	else
	{
		info.next = -1;
		m_slowCallbacks.insert( std::make_pair(when, id) );
	}

#if TIMER_DEBUG
	if (m_epoch != 0.0f)
		NetLog("[timer] sched %d @ %f", id, (when - m_epoch).GetMilliSeconds());
#endif

	if (when < m_wakeup)
		CNetwork::Get()->WakeThread();
	return id;
}

void * CNetTimer::CancelTimer( NetTimerId& id )
{
	ASSERT_GLOBAL_LOCK;
	if (id <= 0 || id >= m_callbacks.size())
	{
		id = 0;
		return NULL;
	}
	void * pUser = NULL;
	if (!m_callbacks[id].cancelled)
	{
		pUser = m_callbacks[id].pUserData;
		m_callbacks[id].cancelled = true;
#if TIMER_DEBUG
		NetLog("[timer] cancel %d", id);
#endif
	}
	id = 0;
	return pUser;
}

CTimeValue CNetTimer::Update()
{
	ASSERT_GLOBAL_LOCK;
	g_time = gEnv->pTimer->GetAsyncTime();

#if TIMER_DEBUG
	if (m_epoch == 0.0f)
		m_epoch = g_time;
#endif

	while (!m_execCallbacks.empty())
	{
		int cb = m_execCallbacks.back();
		m_execCallbacks.pop_back();
		m_freeCallbacks.push_back(cb);
		NET_ASSERT(m_callbacks[cb].cancelled == true);
		m_callbacks[cb] = SCallbackInfo();
	}

	float elapsed = (g_time - m_lastExec).GetSeconds();
	int advanceSlots;
	if (elapsed > 60)
		advanceSlots = TIMER_SLOTS-1;
	else
		advanceSlots = (int)(elapsed * TIMER_HERTZ);
#if TIMER_DEBUG
	int oldAdvanceSlots = advanceSlots;
#endif
	if (advanceSlots >= TIMER_SLOTS)
		advanceSlots = TIMER_SLOTS-1;
	else if (advanceSlots < 0)
		advanceSlots = 0;
#if TIMER_DEBUG
	NetLog("[timer] advance %d (was %d) (%f elapsed)", advanceSlots, oldAdvanceSlots, (g_time - m_lastExec).GetSeconds());
#endif
	int endSlot = (m_curSlot + advanceSlots) % TIMER_SLOTS;
	for (; m_curSlot != endSlot; m_curSlot = (m_curSlot+1) % TIMER_SLOTS)
	{
		int id = m_timerCallbacks[m_curSlot];
		while (id != -1)
		{
			m_execCallbacks.push_back(id);
			id = m_callbacks[id].next;
		}
		m_timerCallbacks[m_curSlot] = -1;
	}
	while (!m_slowCallbacks.empty() && m_slowCallbacks.begin()->first < g_time)
	{
		int id = m_slowCallbacks.begin()->second;
		m_execCallbacks.push_back(id);
		m_slowCallbacks.erase(m_slowCallbacks.begin());
	}

	for (std::vector<int>::iterator iter = m_execCallbacks.begin(); iter != m_execCallbacks.end(); ++iter)
	{
		NetTimerId id = *iter;
		if (m_callbacks[id].cancelled)
			continue;
		m_callbacks[id].cancelled = true;
		void * pUserData = m_callbacks[id].pUserData;
		NetTimerCallback callback = m_callbacks[id].callback;

		g_time = gEnv->pTimer->GetAsyncTime();
#if TIMER_DEBUG
		float delay = (g_time - m_callbacks[id].schedTime).GetMilliSeconds();
		NET_ASSERT(delay < 1000.0f);
		NetLog("[timer] exec %d @ %f (%fms late)", id, (g_time - m_epoch).GetMilliSeconds(), delay);
#endif
		callback( id, pUserData, g_time );
	}

	if (advanceSlots)
		m_lastExec = g_time;
	// search forward for the first callback
	int first;
	bool foundFirst = false;
	for (first=0; first<TIMER_SLOTS; first++)
		if (m_timerCallbacks[(m_curSlot+first)%TIMER_SLOTS]>0)
		{
			foundFirst = true;
			break;
		}

	g_time = gEnv->pTimer->GetAsyncTime();
	if (foundFirst)
	{
		m_wakeup = g_time + float(first)/float(TIMER_HERTZ);
		if (!m_slowCallbacks.empty() && m_slowCallbacks.begin()->first < m_wakeup)
			m_wakeup = m_slowCallbacks.begin()->first;
	}
	else if (!m_slowCallbacks.empty())
		m_wakeup = m_slowCallbacks.begin()->first;
	else
		m_wakeup = g_time + 30.0f;

	return m_wakeup;
}
