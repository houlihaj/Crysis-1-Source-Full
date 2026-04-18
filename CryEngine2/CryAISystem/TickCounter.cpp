/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 File name:   TickCounter.cpp
 $Id$
 Description: 

 -------------------------------------------------------------------------
 History:
 - 17:may:2006   14:31 : Created by Kirill Bulatsev

*********************************************************************/

#include "StdAfx.h"
#include "TickCounter.h"
#include "IRenderer.h"



TickCounterManager*	TickCounter::m_pManager(NULL);

//
//----------------------------------------------------------------------------------------
TickCounter::TickCounter(const char* pUserName):
m_Ticks(0),
m_TicksPerTime(.0f),
m_TickMaxTime(.0f),
m_StartTicks(-1),
m_Calls(0),
m_CallsPerTime(0)
{
	m_pManager->AddCounter(pUserName, this);
}

//
//----------------------------------------------------------------------------------------
TickCounter::TickCounter():
m_Ticks(0),
m_TicksPerTime(.0f),
m_StartTicks(-1),
m_TicksMax(0)
{
}


//
//----------------------------------------------------------------------------------------
void TickCounter::StartCount()
{
		++m_Calls;
		m_StartTicks = CryGetTicks();
}


//
//----------------------------------------------------------------------------------------
void TickCounter::StopCount()
{
int64 deltaTicks(CryGetTicks()-m_StartTicks);
	if(deltaTicks>m_TicksMax)
		m_TicksMax = deltaTicks;
	m_Ticks += deltaTicks;
}


//
//----------------------------------------------------------------------------------------
TickCounterManager::TickCounterManager():
m_AccumulatedTime(0.f),
m_FramesCounter(0)
{
	TickCounter::m_pManager = this;
	AddCounter("_CORE_", &m_Core);
	m_Core.StartCount();
}


//
//----------------------------------------------------------------------------------------
void TickCounterManager::AddCounter(const char* pName, TickCounter *pCounter)
{
	m_Counters[pName] = pCounter;
}




//
//----------------------------------------------------------------------------------------
void TickCounterManager::Draw(IRenderer *pRenderer)
{
	if(m_Core.m_TicksPerTime==0)
		return;

	int col(70);
	int row(5);
	float color[4]={0.f,1.f,1.f,1.f};
	float ColumnSize = 11;
	float RowSize = 11;
	float baseY = 10;

	float	total(0.0f);
	int	callsTotal(0);
	for(t_CountersMap::iterator iCounter(m_Counters.begin()); iCounter!=m_Counters.end(); ++iCounter)
	{
		float curValue = (iCounter->second->m_TicksPerTime)/m_Core.m_TicksPerTime*100.f;
		if(curValue>99.f)
			continue;
		total+=curValue;
		callsTotal += iCounter->second->m_CallsPerTime;
		pRenderer->Draw2dLabel( ColumnSize*static_cast<float>(col),baseY+RowSize*static_cast<float>(row), 1.2f, color, false,
														"%.2fms %.2f%%  -%d- %s", iCounter->second->m_TickMaxTime, curValue, iCounter->second->m_CallsPerTime, iCounter->first.c_str());
//														"%.1f %s", TicksToMilliseconds(iCounter->second->m_TicksPerTime), iCounter->first.c_str());
		++row;
	}
	++row;
	pRenderer->Draw2dLabel( ColumnSize*static_cast<float>(col+4),baseY+RowSize*static_cast<float>(row), 1.2f, color, false,
		"%.2f%% -%d- >> grand total", total, callsTotal);
}
/*
//////////////////////////////////////////////////////////////////////////
float TickCounterManager::TicksToSeconds (float ticks)
{
	return (g_fSecondsPerTick * ticks);
}

//////////////////////////////////////////////////////////////////////////
float TickCounterManager::TicksToMilliseconds (float ticks)
{
	return (g_fMilliSecondsPerTick * ticks);
}
*/

//
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
//
void TickCounterManager::Update(float deltaTime)
{
	m_Core.StopCount();
	m_Core.StartCount();

const	float profileFrameTime(3.f);
	m_AccumulatedTime += deltaTime;
	++m_FramesCounter;
	if(m_AccumulatedTime<profileFrameTime)
		return;

	float	averageFrameTiks(m_Core.m_Ticks/m_FramesCounter);
	m_FramesCounter = 0;
	m_Core.StopCount();
	for(t_CountersMap::iterator iCounter(m_Counters.begin()); iCounter!=m_Counters.end(); ++iCounter)
	{
		TickCounter& curCounter(*iCounter->second);
		curCounter.m_TicksPerTime = static_cast<float>(curCounter.m_Ticks)/m_AccumulatedTime;
		curCounter.m_Ticks = 0;

		curCounter.m_CallsPerTime = (int)((curCounter.m_Calls)/m_AccumulatedTime);
		curCounter.m_Calls = 0;

		curCounter.m_TickMaxTime = GetISystem()->GetIProfileSystem()->TicksToSeconds(curCounter.m_TicksMax)*1000.f;
//		curCounter.m_TickMaxTime = 100.f*static_cast<float>(curCounter.m_TicksMax)/averageFrameTiks;
		curCounter.m_TicksMax = 0;
	}
	m_Core.StartCount();

	m_AccumulatedTime = 0.f;
}

//
//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------
