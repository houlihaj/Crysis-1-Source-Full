////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   platform.h
//  Version:     v1.00
//  Created:     11/12/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Platform dependend stuff.
//               Include this file instead of windows.h
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _PLATFORM_H_
#define _PLATFORM_H_
#pragma once

#define DEBUG_BREAK _asm { int 3 }
#define _CPU_X86

#include <windows.h>
#include <Win32specific.h>

#include "stdio.h"

#define CPUF_SSE   1
#define CPUF_SSE2  2
#define CPUF_3DNOW 4
#define CPUF_MMX   8

long   CryInterlockedIncrement( int volatile *lpAddend );
long   CryInterlockedDecrement( int volatile *lpAddend );
void*  CryCreateCriticalSection();
void   CryDeleteCriticalSection( void *cs );
void   CryEnterCriticalSection( void *cs );
void   CryLeaveCriticalSection( void *cs );

#define AUTO_STRUCT_INFO
#define AUTO_STRUCT_INFO_LOCAL
#define AUTO_TYPE_INFO(Type) Type ati(Type);
#define STRUCT_INFO_BEGIN(a)
#define STRUCT_INFO_END(a)
#define STRUCT_BITFIELD_INFO(a,b,c)
#define STRUCT_VAR_INFO(a,b)
#define TYPE_INFO(a)
#define assert(a)

#if defined(__GNUC__)
  #define PRINTF_PARAMS(...) __attribute__ ((format (printf, __VA_ARGS__)))
  #define SCANF_PARAMS(...) __attribute__ ((format (scanf, __VA_ARGS__)))
#else
  #define PRINTF_PARAMS(...)
	#define SCANF_PARAMS(...)
#endif

//////////////////////////////////////////////////////////////////////////
// Use our own memory manager.
// No Any STL includes must be before this line.
//////////////////////////////////////////////////////////////////////////
#ifndef NOT_USE_CRY_MEMORY_MANAGER
#define USE_NEWPOOL
#include <CryMemoryManager.h>
#else
#define _ACCESS_POOL
#endif // NOT_USE_CRY_MEMORY_MANAGER

#include <string>
typedef std::string string;
typedef std::wstring wstring;

// 32/64 Bit versions.
#define SIGN_MASK(x) ((intptr_t)(x) >> ((sizeof(size_t)*8)-1))
#define BIT(x) (1<<(x))

#pragma warning(disable: 4018)

// macro for structure alignement
#define DEFINE_ALIGNED_DATA( type, name, alignment ) _declspec(align(alignment)) type name;
#define DEFINE_ALIGNED_DATA_STATIC( type, name, alignment ) static _declspec(align(alignment)) type name;
#define DEFINE_ALIGNED_DATA_CONST( type, name, alignment ) const _declspec(align(alignment)) type name;

#define _CRY_SYSTEM_H_
#define __CRYTHREAD_WINDOWS_H__
#define __CRYTHREADIMPL_WINDOWS_H__
#define __TYPEINFO_H
#define _CRY_COMMON_WIN32_SPECIFIC_HDR_

#define SwapEndian(a)
struct ISystemEventListener {};
enum { ESYSTEM_EVENT_RANDOM_SEED, ESYSTEM_EVENT_LEVEL_RELOAD };
typedef int ESystemEvent;
#define ModuleInitISystem(a)
#define FSystemAlloc FModuleAlloc

#define CryLog printf
#define CryLogAlways printf

struct ISystem *GetISystem();
struct ISystem {
	virtual int GetCPUFlags() = 0;
	virtual struct ILog *GetILog() = 0;
	virtual ISystem *GetISystemEventDispatcher() { return this; }
	virtual void RegisterListener(void*) {}
};
#define CryError {}

#define MTRAND_H
inline float cry_frand() { return rand()*(1.0f/RAND_MAX); }
inline unsigned int cry_rand32() { return rand(); }
struct CMTRand_int32 {
	int Generate() { return rand(); }
	float GenerateFloat() { return rand()*(1.0f/RAND_MAX); }
	void seed(int) {}
};

///////////////////////////////////////////////
////////// Inlined Frame Profiler /////////////

__forceinline int64 CryQueryPerformanceCounter()
{
#if defined(_CPU_X86)
	int64 nTime;
	int64 *pnTime = &nTime;
	__asm {
		mov ebx, pnTime
		rdtsc
		mov [ebx], eax
		mov [ebx+4], edx
	}
#elif defined(WIN32)
	LARGE_INTEGER li;
	QueryPerformanceCounter( &li );
	return li.QuadPart;
#endif
}

__declspec(naked) inline __int64 getTicks() { 
	__asm rdtsc
	__asm ret
}

extern int g_iLastProfilerId;
struct CFrameProfiler {
	CFrameProfiler(char *name) {
		m_name = name;
		m_id = ++g_iLastProfilerId;
	}
	char *m_name;
	int m_id;
};

struct CFrameProfilerTimeSample {
	int iTotalTime;
	int iSelfTime;
	int iCount;
	CFrameProfiler *pProfiler;
	unsigned __int64 iCode;
	int iChild,iNext;

	void Reset() {
		iChild = iNext = -1;
		iTotalTime = iSelfTime = iCount = 0;
	}
};

struct CFrameProfilerSectionBase {
	__int64 m_iStartTime;
	unsigned __int64 m_iCurCode;
	int m_iCurSlot;
	int m_bActive;
};

struct ProfilerData {
	int iLevel,iLastSampleSlot,iLastTimeSample;
	CFrameProfilerSectionBase sec0;
	CFrameProfilerSectionBase *pCurSection[16];
	CFrameProfilerTimeSample TimeSamples[256];
};
extern ProfilerData g_pd;

struct CFrameProfilerSection : CFrameProfilerSectionBase {
	CFrameProfilerSection(CFrameProfiler *pProfiler, int bActive=1) {
		if (m_bActive=bActive) {
			m_iCurCode = g_pd.pCurSection[g_pd.iLevel]->m_iCurCode | (unsigned __int64)pProfiler->m_id<<(7-g_pd.iLevel)*8;
			if (g_pd.TimeSamples[g_pd.iLastSampleSlot].iCode == m_iCurCode)
				m_iCurSlot = g_pd.iLastSampleSlot;
			else {
				if ((m_iCurSlot = g_pd.TimeSamples[g_pd.pCurSection[g_pd.iLevel]->m_iCurSlot].iChild) < 0) {
					g_pd.TimeSamples[g_pd.pCurSection[g_pd.iLevel]->m_iCurSlot].iChild = m_iCurSlot = ++g_pd.iLastTimeSample;
					g_pd.TimeSamples[m_iCurSlot].Reset();
				} else {
					for(; g_pd.TimeSamples[m_iCurSlot].iNext>=0 && g_pd.TimeSamples[m_iCurSlot].iCode!=m_iCurCode; m_iCurSlot=g_pd.TimeSamples[m_iCurSlot].iNext);
					if (g_pd.TimeSamples[m_iCurSlot].iCode!=m_iCurCode) {
						m_iCurSlot = (g_pd.TimeSamples[m_iCurSlot].iNext = ++g_pd.iLastTimeSample);
						g_pd.TimeSamples[m_iCurSlot].Reset();
					}
				}
				g_pd.TimeSamples[g_pd.iLastSampleSlot = m_iCurSlot].iCode = m_iCurCode;
			}
			g_pd.TimeSamples[m_iCurSlot].pProfiler = pProfiler;
			m_iStartTime = getTicks();
			g_pd.TimeSamples[m_iCurSlot].iCount++;
			g_pd.pCurSection[++g_pd.iLevel] = this;
		}
	}

	~CFrameProfilerSection() 
	{
		if (m_bActive) {
			int iTime = (int)(getTicks()-m_iStartTime);
			g_pd.TimeSamples[m_iCurSlot].iSelfTime += iTime;
			g_pd.TimeSamples[m_iCurSlot].iTotalTime += iTime;
			g_pd.TimeSamples[g_pd.pCurSection[--g_pd.iLevel]->m_iCurSlot].iSelfTime -= iTime;
		}
	}
};

void ResetProfiler(ProfilerData *pd)
{
	pd->iLevel = 0;
	pd->sec0.m_iCurCode = 0;
	pd->sec0.m_iCurSlot = pd->iLastSampleSlot = pd->iLastTimeSample = 0;
	pd->pCurSection[0] = &pd->sec0;
	pd->TimeSamples[0].Reset();
}

extern __declspec(thread) int iCaller;

#define FUNCTION_PROFILER( pISystem,subsystem ) \
	static CFrameProfiler staticFrameProfiler( __FUNCTION__ ); \
	CFrameProfilerSection frameProfilerSection( &staticFrameProfiler,(::iCaller=iCaller)^1 );	
#define FRAME_PROFILER( szProfilerName,pISystem,subsystem ) \
	static CFrameProfiler staticFrameProfiler( szProfilerName ); \
	CFrameProfilerSection frameProfilerSection( &staticFrameProfiler );
#define PROFILE_PHYSICS	0

#ifndef PHYSICS_EXPORTS
extern "C" __declspec(dllimport) ProfilerData *GetProfileData();
#endif 

#define TestbedPlaceholder foo() {}; int g_iLastProfilerId=0; ProfilerData g_pd; \
	__declspec(thread) int iCaller = 0; \
	CMTRand_int32 g_random_generator; \
	extern "C" CRYPHYSICS_API ProfilerData *GetProfileData() { return &g_pd; } \
	void TestbedPlaceholder


///////////////////////////////////////////////////////////////////////////////
// common Typedef                                                                   //
///////////////////////////////////////////////////////////////////////////////
typedef double real;
typedef int index_t;
typedef int                 INT;
typedef unsigned int        UINT;
typedef unsigned int        *PUINT;

#endif // _PLATFORM_H_
