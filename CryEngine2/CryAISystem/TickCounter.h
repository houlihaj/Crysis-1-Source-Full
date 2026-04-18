/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   TickCounter.h
	$Id$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 17:may:2006   14:31 : Created by Kirill Bulatsev

*********************************************************************/


#ifndef __TICK_Counter__
#define __TICK_Counter__

#if _MSC_VER > 1000
#pragma once
#endif

//#include <IAISystem.h>
#include <string>
#include <map>


//
//-------------------------------------------------------------
class TickCounterManager;
struct IRenderer;


//
//-------------------------------------------------------------
struct TickCounter
{
	int		m_Calls;
	int		m_CallsPerTime;
	int64 m_Ticks;
	int64 m_TicksMax;
	float m_TicksPerTime;
	float m_TickMaxTime;
	int64 m_StartTicks;
	static TickCounterManager *m_pManager;

	TickCounter(const char* pUserName);
	TickCounter();
	void	StartCount();
	void	StopCount();
};

//
//-------------------------------------------------------------
struct TickCounterBlock
{
	TickCounter* m_pCounter;

	TickCounterBlock(TickCounter* pCounter):m_pCounter(pCounter) {m_pCounter->StartCount();}
	~TickCounterBlock(){m_pCounter->StopCount();}
};


//
//-------------------------------------------------------------
class TickCounterManager
{
public:
	TickCounterManager();
	void	AddCounter(const char* pName, TickCounter *pCounter);

	void	Update(float deltaTime);
	void	Draw(IRenderer *pRenderer);

//	static float TicksToSeconds( float ticks );
//	static float TicksToMilliseconds( float ticks );

protected:
typedef	std::map<string, TickCounter*> t_CountersMap;


	float	m_AccumulatedTime;
	float	m_FramesCounter;
	TickCounter	m_Core;
	t_CountersMap	m_Counters;

/*
	static int64 g_nTicksPerSecond;
	static double g_fSecondsPerTick;
	static double g_fMilliSecondsPerTick;
	// CPU speed, in Herz
	static unsigned g_nCPUHerz;
*/
};

//
//-------------------------------------------------------------

//ray trace profiling stuff------------------------------------
#define TICK_COUNTER_FUNCTION static TickCounter* pRTP_TickCounter=(new TickCounter( __FUNCTION__)); 
#define TICK_COUNTER_FUNCTION_BLOCK_START {TickCounterBlock blk(pRTP_TickCounter);
#define TICK_COUNTER_FUNCTION_BLOCK_END }
//ray trace profiling stuff------------------------------------


#endif __TICK_Counter__
