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
#ifndef __CallbackTimer_H__
#define __CallbackTimer_H__

#pragma once

#include <map>
#include <queue>
#include "TimeValue.h"

//--- Definitions --------------------------------------------------------
#define FIRST_HANDLE			100									// first handle returned
#define REFERENCE_WARNING		1000000								// warn when too may add/remove operations are performed so that we do not wrap the index IDs

struct ITimer;

//--- Timer callback class -----------------------------------------------
class CCallbackTimer
{
public:
	void UpdateTimer(void);										// update the timers
	int AddTimer(CTimeValue nInterval, bool bRepeat, void (*pFunction)(void*, int), void *pUserData);		// add a new timer and return its handle; interval is in ms
	void* RemoveTimer(int nIndex);								// remove an existing timer, returns user data
	void GetMemoryStatistics(ICrySizer * s);

	CCallbackTimer();
	virtual ~CCallbackTimer();

private:
	struct CCBack
	{
		CTimeValue m_Interval;															// interval
		void (*m_pFunction)(void*, int);														// function to call
		void * m_pUserData;
		bool m_bRepeat;

		CCBack(CTimeValue nInterval, bool bRepeat, void(*pFunction)(void*, int), void * pUserData)
		{
			assert(pFunction != NULL);

			m_Interval = nInterval;
			m_pFunction = pFunction;
			m_pUserData = pUserData;
			m_bRepeat = bRepeat;
		}
	};

	typedef std::map<int, CCBack> CallBackEntry;				// define the list structure
	typedef std::pair<int, CCBack> CallBackPair;				// define the pair that goes in the map

	int m_nReference;											// reference value incremented with each callback added to guarantee a unique ID
	CallBackEntry *m_pCallBacks;								// call back list

	struct CallBackTimeout
	{
		CallBackTimeout( CTimeValue timeout, int nRef )
		{
			this->timeout = timeout;
			this->nRef = nRef;
		}
		CTimeValue timeout;
		int nRef;
		bool operator<( const CallBackTimeout& rhs ) const
		{
			return this->timeout > rhs.timeout;
		}
	};
	typedef std::priority_queue<CallBackTimeout> CallBackQueue;
	CallBackQueue * m_pCallBackQueue;
};

#endif //__CallbackTimer_H__
