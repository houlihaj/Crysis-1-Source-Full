
//////////////////////////////////////////////////////////////////////
//
//	Crytek Source code 
//	Copyright (c) Crytek 2001-2004
//
// ScriptTimerMgr.cpp: implementation of the CScriptTimerMgr class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ScriptTimerMgr.h"
#include <ITimer.h>
#include <IEntitySystem.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CScriptTimerMgr::CScriptTimerMgr(IScriptSystem *pScriptSystem )
{
	m_nLastTimerID = 1;
	m_pScriptSystem = pScriptSystem;
	m_bPause=false;
}

CScriptTimerMgr::~CScriptTimerMgr()
{
	Reset();
}

//////////////////////////////////////////////////////////////////////////
void CScriptTimerMgr::DeleteTimer( ScriptTimer *pTimer )
{
	assert( pTimer );

	if (pTimer->pScriptFunction)
		m_pScriptSystem->ReleaseFunc(pTimer->pScriptFunction);
	delete pTimer;
}

//////////////////////////////////////////////////////////////////////////
// Create a new timer and put it in the list of managed timers.
int	CScriptTimerMgr::AddTimer( ScriptTimer &timer )
{
	CTimeValue nCurrTimeMillis = gEnv->pTimer->GetFrameStartTime();
	timer.nStartTime = nCurrTimeMillis.GetMilliSecondsAsInt64();
	timer.nEndTime = timer.nStartTime + timer.nMillis;
	if (!timer.nTimerID)
	{
		m_nLastTimerID++;
		timer.nTimerID = m_nLastTimerID;
	}
	else
	{
		if (timer.nTimerID > m_nLastTimerID)
			m_nLastTimerID = timer.nTimerID+1;
	}
	m_mapTempTimers[timer.nTimerID] = new ScriptTimer(timer);
	return timer.nTimerID;
}

//////////////////////////////////////////////////////////////////////////
// Delete a timer from the list.
void CScriptTimerMgr::RemoveTimer(int nTimerID)
{
	ScriptTimerMapItor itor;
	// find timer
	itor=m_mapTimers.find(nTimerID);
	if(itor!=m_mapTimers.end())
	{
		// remove it
		ScriptTimer *pST=itor->second;
		DeleteTimer(pST);
		m_mapTimers.erase(itor);
	}
}

//////////////////////////////////////////////////////////////////////////
void	CScriptTimerMgr::Pause(bool bPause)
{ 
	m_bPause = bPause; 
}

//////////////////////////////////////////////////////////////////////////
// Remove all timers.
void CScriptTimerMgr::Reset()
{
	m_nLastTimerID = 1;
	ScriptTimerMapItor itor;
	itor=m_mapTimers.begin();
	while(itor!=m_mapTimers.end())
	{
		if(itor->second)
		{
			DeleteTimer(itor->second);
		}
		++itor;
	}
	m_mapTimers.clear();

	itor=m_mapTempTimers.begin();
	while(itor!=m_mapTempTimers.end())
	{
		if(itor->second)
		{
			DeleteTimer(itor->second);
		}
		++itor;
	}
	m_mapTempTimers.clear();


}

//////////////////////////////////////////////////////////////////////////
// Update all managed timers.
void CScriptTimerMgr::Update(int64 nCurrentTime)
{
	ScriptTimerMapItor itor;
	
	// Loop through all timers.
	itor = m_mapTimers.begin();

	while (itor != m_mapTimers.end())
	{
		ScriptTimer *pST = itor->second;

		if (m_bPause && !pST->bUpdateDuringPause)
		{
			++itor;
			continue;
		}

    // if it is time send a timer-event
		if(nCurrentTime >= pST->nEndTime)
		{
			ScriptHandle timerIdHandle;
			timerIdHandle.n = pST->nTimerID;
			// Call function specified by the script.

			if (pST->pScriptFunction)
			{
				if (!pST->pUserData)
					Script::Call( m_pScriptSystem,pST->pScriptFunction,timerIdHandle );
				else
					Script::Call( m_pScriptSystem,pST->pScriptFunction,pST->pUserData,timerIdHandle );
			}
			else if (*pST->sFuncName)
			{
				HSCRIPTFUNCTION hFunc = 0;
				if (gEnv->pScriptSystem->GetGlobalValue(pST->sFuncName,hFunc))
				{
					if (!pST->pUserData)
						Script::Call( m_pScriptSystem,hFunc,timerIdHandle );
					else
						Script::Call( m_pScriptSystem,hFunc,pST->pUserData,timerIdHandle );
					gEnv->pScriptSystem->ReleaseFunc(hFunc);
				}
			}
			

			// After sending the event we can remove the timer.
			ScriptTimerMapItor tempItor=itor;
			++itor;
			m_mapTimers.erase(tempItor);
			DeleteTimer(pST);
		}
		else
		{
			++itor;
		}
	}
	// lets move all new created timers in the map. this is done at this point to avoid recursion, while trying to create a timer on a timer-event.
	if(!m_mapTempTimers.empty())
	{
		ScriptTimerMapItor itor;
		itor=m_mapTempTimers.begin();
		while(itor!=m_mapTempTimers.end())
		{
			m_mapTimers.insert(ScriptTimerMapItor::value_type(itor->first,itor->second));
			++itor;
		}
		m_mapTempTimers.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptTimerMgr::GetMemoryStatistics(ICrySizer *pSizer)
{
	int nSize = 0;
	nSize += sizeof(*this);
	nSize += m_mapTimers.size()*(sizeof(ScriptTimer) + sizeof(ScriptTimerMap::value_type));
	nSize += m_mapTempTimers.size()*(sizeof(ScriptTimer) + sizeof(ScriptTimerMap::value_type));
	pSizer->Add( this,nSize );
}

//////////////////////////////////////////////////////////////////////////
void CScriptTimerMgr::Serialize( TSerialize &ser )
{
	if (ser.GetSerializationTarget() == eST_Network)
		return;
	
	ser.BeginGroup("ScriptTimers");

	if (ser.IsReading())
	{
		Reset();

		m_nLastTimerID++;

		int nTimers = 0;
		ser.Value( "count",nTimers );

		for (int i = 0; i < nTimers; i++)
		{
			ScriptTimer timer;

			ser.BeginGroup("timer");
			ser.Value("id",timer.nTimerID);
			ser.Value("millis",timer.nMillis);

			string func;
			ser.Value("func",func);
			strcpy(timer.sFuncName,func);

			uint32 nEntityId = 0;
			ser.Value( "entity",nEntityId );
			if (nEntityId && gEnv->pEntitySystem)
			{
				IEntity *pEntity = gEnv->pEntitySystem->GetEntity(nEntityId);
				if (pEntity)
				{
					timer.pUserData = pEntity->GetScriptTable();
				}
			}
			AddTimer(timer);
			ser.EndGroup(); //timer
		}
	}
	else
	{
		// Saving.
		int nTimers = 0;
		for (ScriptTimerMap::iterator it = m_mapTimers.begin(); it != m_mapTimers.end(); ++it)
		{
			ScriptTimer *pTimer = it->second;

			if (pTimer->pScriptFunction) // Do not save timers that have script handle callback.
				continue;

			ser.BeginGroup("timer");
			ser.Value("id",pTimer->nTimerID);
			ser.Value("millis",pTimer->nMillis);
			
			nTimers++;
			string sFuncName = pTimer->sFuncName;
			ser.Value("func",sFuncName);

			if (!pTimer->pUserData)
			{

			}
			else
			{
				ScriptHandle handle;
				if (pTimer->pUserData->GetValue("id",handle))
				{
					uint32 nEntityId = handle.n;
					ser.Value( "entity",nEntityId );
				}
			}
			ser.EndGroup(); //timer
		}
		ser.Value( "count",nTimers );
	}


	ser.EndGroup(); //ScriptTimers
}
