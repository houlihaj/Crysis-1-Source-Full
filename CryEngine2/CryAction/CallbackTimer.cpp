/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	Implementation of a timer based callback system

-------------------------------------------------------------------------
History:
- 14:6:2005 :  Created by Thomas

*************************************************************************/

#include "StdAfx.h"
#include "CallbackTimer.h"

//--- Constructor --------------------------------------------------------
CCallbackTimer::CCallbackTimer()
{
	m_nReference = FIRST_HANDLE;
	m_pCallBacks = new CallBackEntry;
	m_pCallBackQueue = new CallBackQueue;
}

//--- Destructor ---------------------------------------------------------
CCallbackTimer::~CCallbackTimer()
{
	delete m_pCallBackQueue;
	delete m_pCallBacks;
}

//--- Add a timer --------------------------------------------------------
int CCallbackTimer::AddTimer(CTimeValue nInterval, bool bRepeat, void (*pFunction)(void*, int), void *pUserData)
{
	static ITimer * pTimer = gEnv->pTimer;

	assert(pFunction != NULL);

	m_pCallBacks->insert(CallBackPair(m_nReference, CCBack(nInterval, bRepeat, pFunction, pUserData)));
	m_pCallBackQueue->push(CallBackTimeout(pTimer->GetFrameStartTime() + nInterval, m_nReference));

	if(m_nReference > REFERENCE_WARNING)
	{
		GameWarning("Number of timers add/remove is getting high");
	}

	return m_nReference++;
}

//--- Remove a timer -----------------------------------------------------
void * CCallbackTimer::RemoveTimer(int nIndex)
{
	void * pRet = NULL;

	CallBackEntry::iterator it;
	it = m_pCallBacks->find(nIndex);
	if(it == m_pCallBacks->end())
	{
		GameWarning("Invalid callback entry (%d) - CallbackTimer::RemoveTimer\n", nIndex);
	}
	else 
	{
		pRet = it->second.m_pUserData;
		m_pCallBacks->erase(it);
	}
	return pRet;
}

//--- Update the timer system --------------------------------------------
void CCallbackTimer::UpdateTimer(void)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if(m_pCallBacks == NULL) return;

	CTimeValue time = gEnv->pTimer->GetFrameStartTime();

	// create time zero to compare against
	CTimeValue zeroTime;
	zeroTime.SetSeconds((int64)0);

	while (!m_pCallBackQueue->empty() && m_pCallBackQueue->top().timeout < time)
	{
		CallBackTimeout ent = m_pCallBackQueue->top();
		m_pCallBackQueue->pop();
		CallBackEntry::iterator it = m_pCallBacks->find(ent.nRef);
		if (it != m_pCallBacks->end())
		{
			it->second.m_pFunction( it->second.m_pUserData, ent.nRef );
			if (it->second.m_bRepeat)
			{
				m_pCallBackQueue->push( CallBackTimeout( ent.timeout + it->second.m_Interval, ent.nRef ) );
			}
			else
			{
				m_pCallBacks->erase(it);
			}
		}
	}
}

void CCallbackTimer::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	if (m_pCallBackQueue)
	{
		s->Add(*m_pCallBackQueue);
		//s->AddContainer(*m_pCallBackQueue);
	}
}