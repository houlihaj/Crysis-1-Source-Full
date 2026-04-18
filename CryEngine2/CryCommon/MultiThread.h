////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   MultiThread.h
//  Version:     v1.00
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MultiThread_h__
#define __MultiThread_h__
#pragma once

#define WRITE_LOCK_VAL (1<<16)

#if !defined(__SPU__)
	#if !defined(PS3)
		long   CryInterlockedIncrement( int volatile *lpAddend );
		long   CryInterlockedDecrement( int volatile *lpAddend );
		long   CryInterlockedExchangeAdd(long volatile * lpAddend, long Value);
		long	 CryInterlockedCompareExchange(long volatile * dst, long exchange, long comperand);
		void*	 CryInterlockedCompareExchangePointer(void* volatile * dst, void* exchange, void* comperand);
	#endif
	void*  CryCreateCriticalSection();
	void   CryDeleteCriticalSection( void *cs );
	void   CryEnterCriticalSection( void *cs );
	bool   CryTryCriticalSection( void *cs );
	void   CryLeaveCriticalSection( void *cs );

#if defined(PS3)
	#include <sys/ppu_thread.h>
	//the Global functions are required to perform leightweight locks between all PPU threads and all SPUs
	//for non PS3 this is obvious useless
	void*  CryCreateCriticalSectionGlobal();
	void   CryDeleteCriticalSectionGlobal( void *cs );
	void   CryEnterCriticalSectionGlobal( void *cs );
	bool   CryTryCriticalSectionGlobal( void *cs );
	void   CryLeaveCriticalSectionGlobal( void *cs );

	ILINE long CryInterlockedIncrement( int volatile *lpAddend )
	{
		register int r;

		__asm__(
			"0:      lwarx      %0, 0, %1     # load and reserve\n"
			"        addi       %0, %0, 1    \n"
			"        stwcx.     %0, 0, %1     # store if still reserved\n"
			"        bne-       0b            # loop if lost reservation\n"
			: "=r" (r)
			: "r" (lpAddend), "m" (*lpAddend), "0" (r)
			: "r0"
			);

		// Notes:
		// - %0 and %1 must be different registers. We're specifying %0 (r) as input
		//   _and_ output to enforce this.
		// - The 'addi' instruction will become 'li' if the second register operand
		//   is r0, so we're listing r0 is clobbered to make sure r0 is not allocated
		//   for %0.
		return r;
	}

	//////////////////////////////////////////////////////////////////////////
	ILINE long CryInterlockedDecrement( int volatile *lpAddend )
	{
		register int r;

		__asm__(
			"0:      lwarx      %0, 0, %1     # load and reserve\n"
			"        addi       %0, %0, -1    \n"
			"        stwcx.     %0, 0, %1     # store if still reserved\n"
			"        bne-       0b            # loop if lost reservation\n"
			: "=r" (r)
			: "r" (lpAddend), "m" (*lpAddend), "0" (r)
			: "r0"
			);

		return r;
	}
	//////////////////////////////////////////////////////////////////////////

	ILINE long	 CryInterlockedExchangeAdd(long volatile * lpAddend, long Value)
	{
		//implements atomically: 	long res = *dst; *dst += Value;	return res;
		uint32_t old, tmp;
		__asm__(
			".loop%=:\n"
			"	lwarx   %[old], 0, %[lpAddend]\n"
			"	add     %[tmp], %[Value], %[old]\n"
			"	stwcx.  %[tmp], 0, %[lpAddend]\n"
			"	bne-    .loop%=\n"
			: [old]"=&r"(old), [tmp]"=&r"(tmp)
			: [lpAddend]"b"(lpAddend), [Value]"r"(Value)
			: "cc", "memory");
		return old;
	}

	ILINE long	CryInterlockedCompareExchange(long volatile * dst, long exchange, long comperand)
	{
		//implements atomically: 	long res = *dst; if (comperand == res)*dst = exchange;	return res;
		uint32_t old;
		__asm__(
			".loop%=:\n"
			"	lwarx   %[old], 0, %[dst]\n"
			"	cmpw    %[old], %[comperand]\n"
			"	bne-    .Ldone%=\n"
			"	stwcx.  %[exchange], 0, %[dst]\n"
			"	bne-    .loop%=\n"
			".Ldone%=:\n"
			: [old]"=&r"(old)
			: [dst]"b"(dst), [comperand]"r"(comperand), [exchange]"r"(exchange)
			: "cc", "memory");

		return old;
	}

	ILINE void*	 CryInterlockedCompareExchangePointer(void* volatile * dst, void* exchange, void* comperand)
	{
		assert(sizeof(void*) == sizeof(long));
		return (void*)CryInterlockedCompareExchange((long volatile*)dst, (long)exchange, (long)comperand);
	}

#else
	#define CryCreateCriticalSectionGlobal CryCreateCriticalSection
	#define CryDeleteCriticalSectionGlobal CryDeleteCriticalSection
	#define CryEnterCriticalSectionGlobal CryEnterCriticalSection
	#define CryTryCriticalSectionGlobal CryTryCriticalSection 
	#define CryLeaveCriticalSectionGlobal CryLeaveCriticalSection
#endif

ILINE void CrySpinLock(volatile int *pLock,int checkVal,int setVal)
{ 
#ifdef _CPU_X86
# ifdef __GNUC__
	register int val;
	__asm__(
		"0:  mov %[checkVal], %%eax\n"
		"    lock cmpxchg %[setVal], (%[pLock])\n"
		"    jnz 0b"
		: "=m" (*pLock)
		: [pLock] "r" (pLock), "m" (*pLock),
		  [checkVal] "m" (checkVal),
		  [setVal] "r" (setVal)
		: "eax"
		);
# else
	__asm
	{
		mov edx, setVal
		mov ecx, pLock
Spin:
		// Trick from Intel Optimizations guide
#ifdef _CPU_SSE
		pause
#endif 
		mov eax, checkVal
		lock cmpxchg [ecx], edx
		jnz Spin
	}
# endif
#else
# if defined(PS3)
	register int val;
	__asm__(
		"0:  lwarx   %[val], 0, %[pLock]\n"
		"    cmpw    %[checkVal], %[val]\n"
		"    bne-    0b\n"
		"    stwcx.  %[setVal], 0, %[pLock]\n"
		"    bne-    0b\n"
		: [val] "=&r" (val), "=m" (*pLock)
		: [pLock] "r" (pLock), "m" (*pLock),
		  [checkVal] "r" (checkVal),
		  [setVal] "r" (setVal)
		);
# else
	// NOTE: The code below will fail on 64bit architectures!
	while(_InterlockedCompareExchange((volatile long*)pLock,setVal,checkVal)!=checkVal);
# endif
#endif
}

//////////////////////////////////////////////////////////////////////////
ILINE void CryInterlockedAdd(volatile int *pVal, int iAdd)
{
#ifdef _CPU_X86
# ifdef __GNUC__
	__asm__(
		"lock add %[iAdd], (%[pVal])\n"
		: "=m" (*pVal)
		: [pVal] "r" (pVal), "m" (*pVal), [iAdd] "r" (iAdd)
		);
# else
	__asm
	{
		mov edx, pVal
		mov eax, iAdd
		lock add [edx], eax
	}
# endif
#else
# if defined(PS3)
	register int val;
	__asm__(
		"0:  lwarx   %[val], 0, %[pVal]\n"
		"    add     %[val], %[val], %[iAdd]\n"
		"    stwcx.  %[val], 0, %[pVal]\n"
		"    bne-    0b\n"
		: [val] "=&r" (val), "=m" (*pVal)
		: [pVal] "r" (pVal), "m" (*pVal),
			[iAdd] "r" (iAdd)
		);
# else
	// NOTE: The code below will fail on 64bit architectures!
	_InterlockedExchangeAdd((volatile long*)pVal,iAdd);
# endif
#endif
}

//////////////////////////////////////////////////////////////////////////
struct ReadLock
{
	ILINE ReadLock(volatile int &rw)
	{
		CryInterlockedAdd(prw=&rw,1);
		volatile char *pw=(volatile char*)&rw+2; for(;*pw;);
	}
	~ReadLock()
	{
		CryInterlockedAdd(prw,-1);
	}
private:
	volatile int *prw;
};

struct ReadLockCond
{
	ILINE ReadLockCond(volatile int &rw,int bActive)
	{
		if (bActive)
		{
			CryInterlockedAdd(&rw,1);
			bActivated = 1;
			volatile char *pw=(volatile char*)&rw+2; for(;*pw;);
		}
		else
		{
			bActivated = 0;
		}
		prw = &rw; 
	}
	void SetActive(int bActive=1) { bActivated = bActive; }
	void Release() { CryInterlockedAdd(prw,-bActivated); }
	~ReadLockCond()
	{
		CryInterlockedAdd(prw,-bActivated);
	}

private:
	volatile int *prw;
	int bActivated;
};

//////////////////////////////////////////////////////////////////////////
struct WriteLock
{
	ILINE WriteLock(volatile int &rw) { CrySpinLock(&rw,0,WRITE_LOCK_VAL); prw=&rw; }
	~WriteLock() { CryInterlockedAdd(prw,-WRITE_LOCK_VAL); }
private:
	volatile int *prw;
};

//////////////////////////////////////////////////////////////////////////
struct WriteAfterReadLock
{
	ILINE WriteAfterReadLock(volatile int &rw) { CrySpinLock(&rw,1,WRITE_LOCK_VAL+1); prw=&rw; }
	~WriteAfterReadLock() { CryInterlockedAdd(prw,-WRITE_LOCK_VAL); }
private:
	volatile int *prw;
};

//////////////////////////////////////////////////////////////////////////
struct WriteLockCond
{
	ILINE WriteLockCond(volatile int &rw,int bActive=1)
	{
		if (bActive)
			CrySpinLock(&rw,0,iActive=WRITE_LOCK_VAL);
		else 
			iActive = 0;
		prw = &rw; 
	}
	~WriteLockCond() { CryInterlockedAdd(prw,-iActive); }
	void SetActive(int bActive=1) { iActive = -bActive & WRITE_LOCK_VAL; }
	void Release() { CryInterlockedAdd(prw,-iActive); }
private:
	volatile int *prw;
	int iActive;
};

//////////////////////////////////////////////////////////////////////////
class CCryThread
{
public:
	CCryThread( void (*func)(void *), void * p );
	~CCryThread();
	static void SetName( const char * name );

private:
	CCryThread( const CCryThread& );
	CCryThread& operator=( const CCryThread& );
#if defined(PS3)
	sys_ppu_thread_t m_handle;
#elif defined(LINUX)
	pthread_t m_handle;
#else
	THREAD_HANDLE m_handle;
#endif
};

//////////////////////////////////////////////////////////////////////////
class CCryMutex
{
public:
	CCryMutex() : m_pCritSection(CryCreateCriticalSection())
	{}

	~CCryMutex()
	{
		CryDeleteCriticalSection(m_pCritSection);
	}

	class CLock
	{
	public:
		CLock( CCryMutex& mtx ) : m_pCritSection(mtx.m_pCritSection)
		{
			CryEnterCriticalSection(m_pCritSection);
		}
		~CLock()
		{
			CryLeaveCriticalSection(m_pCritSection);
		}

	private:
		CLock( const CLock& );
		CLock& operator=( const CLock& );

		void *m_pCritSection;
	};

private:
	CCryMutex( const CCryMutex& );
	CCryMutex& operator=( const CCryMutex& );

	void *m_pCritSection;
};
#endif //__SPU__


//for PS3 we need additional global locking primitives to lock between all PPU threads and all SPUs
#if defined(PS3)
#if !defined __CRYCG__
class CCryMutexGlobal
{
public:
#if !defined(__SPU__)
	CCryMutexGlobal() : m_pCritSection(CryCreateCriticalSectionGlobal())
	{}

	~CCryMutexGlobal()
	{
		CryDeleteCriticalSectionGlobal(m_pCritSection);
	}
#endif//__SPU__
	class CLock
	{
	public:
		CLock( CCryMutexGlobal& mtx ) : m_pCritSection(mtx.m_pCritSection)
		{
			CryEnterCriticalSectionGlobal(m_pCritSection);
		}
		~CLock()
		{
			CryLeaveCriticalSectionGlobal(m_pCritSection);
		}

	private:
		CLock( const CLock& );
		CLock& operator=( const CLock& );

		void *m_pCritSection;
	};

private:
#if defined(__SPU__)
	CCryMutexGlobal();
	~CCryMutexGlobal();
#endif//__SPU__
	CCryMutexGlobal( const CCryMutexGlobal& );
	CCryMutexGlobal& operator=( const CCryMutexGlobal& );

	void *m_pCritSection;
};
#endif // !__CRYCG__
#else
	//no impl. needed for non PS3	
	#define CCryMutexGlobal CCryMutex
#endif//PS3

#if defined __CRYCG__
class CCryMutex
{
public:
	CCryMutex();
	class CLock { public: CLock(CCryMutex&); };
};
class CCryMutexGlobal
{
public:
	CCryMutexGlobal();
	class CLock { public: CLock(CCryMutexGlobal&); };
};
class CCryThread
{
public:
	CCryThread(void (*)(void *), void *);
	static void SetName(const char *);
};
#endif // __CRYCG__

#endif // __MultiThread_h__
