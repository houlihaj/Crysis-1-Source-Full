// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__0F62BDE9_4784_4F7A_A265_B7D1A71883D0__INCLUDED_)
#define AFX_STDAFX_H__0F62BDE9_4784_4F7A_A265_B7D1A71883D0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//#define NOT_USE_CRY_MEMORY_MANAGER

#include <CryModuleDefs.h>
#define eCryModule eCryM_Network

#define CRYNETWORK_EXPORTS
#define CRYNETWORK_RELEASEBUILD 1

#include <platform.h>


#define NOT_USE_UBICOM_SDK

#include <stdio.h>
#include <stdarg.h>

#ifdef WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
//#define _WIN32_WINNT 0x400  // as found
#define _WIN32_WINNT 0x0501
//#define _WIN32_WINNT 0x0600
#endif
#endif

#ifdef LINUX
#include <sys/socket.h>
#include <netdb.h>
extern int h_errno;
#endif


#include <map>
#include <algorithm>

#include <Cry_Math.h>
#include <CrySizer.h>
#include "StlUtils.h"

#include "IRenderer.h"

#if defined(WIN32)
	#include <process.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define S_ADDR_IP4(ADDR) ((ADDR).sin_addr.S_un.S_addr)
#elif defined(XENON)
	#include <xtl.h>
#elif defined(PS3)
	#if defined(__cplusplus)
		extern "C" 
		{
			extern int *_sys_net_errno_loc();
		}
	#endif
	#include <sys/select.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netex/errno.h>
	#include <netex/net.h>
#elif defined(LINUX)
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netdb.h>
	#include <unistd.h>
	#include <fcntl.h>
#endif

#include <memory>
#include <vector>

#if !defined(S_ADDR_IP4)
	#define S_ADDR_IP4(ADDR) ((ADDR).sin_addr.s_addr)
#endif

//#if defined(_DEBUG) && defined(WIN32)
//#include <crtdbg.h>
//#endif

#define NET_ASSERT_LOGGING 1

#if CRYNETWORK_RELEASEBUILD
# undef NET_ASSERT_LOGGING
# define NET_ASSERT_LOGGING 0
#endif
void NET_ASSERT_FAIL(const char * check, const char * file, int line);
#if NET_ASSERT_LOGGING
# define NET_ASSERT(x) if (true) { bool net_ok = true; if (!(x)) net_ok = false; CRY_ASSERT_MESSAGE(net_ok, #x); if (!net_ok) NET_ASSERT_FAIL(#x, __FILE__, __LINE__); } else
#else
# define NET_ASSERT assert
#endif

#ifndef NET____TRACE
#define NET____TRACE

// src and trg can be the same pointer (in place encryption)
// len must be in bytes and must be multiple of 8 byts (64bits).
// key is 128bit:  int key[4] = {n1,n2,n3,n4};
// void encipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k )
#define TEA_ENCODE( src,trg,len,key ) {\
	register unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	register unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) {\
	register unsigned int y=v[0],z=v[1],n=32,sum=0; \
	while(n-->0) { sum += delta; y += (z << 4)+a ^ z+sum ^ (z >> 5)+b; z += (y << 4)+c ^ y+sum ^ (y >> 5)+d; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

// src and trg can be the same pointer (in place decryption)
// len must be in bytes and must be multiple of 8 byts (64bits).
// key is 128bit: int key[4] = {n1,n2,n3,n4};
// void decipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k)
#define TEA_DECODE( src,trg,len,key ) {\
	register unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	register unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) { \
	register unsigned int y=v[0],z=v[1],sum=0xC6EF3720,n=32; \
	while(n-->0) { z -= (y << 4)+c ^ y+sum ^ (y >> 5)+d; y -= (z << 4)+a ^ z+sum ^ (z >> 5)+b; sum -= delta; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

// encode size ignore last 3 bits of size in bytes. (encode by 8bytes min)
#define TEA_GETSIZE( len ) ((len) & (~7))

inline void __cdecl __NET_TRACE(const char *sFormat, ... ) PRINTF_PARAMS(1, 2);

#ifndef PS2
inline void __cdecl __NET_TRACE(const char *sFormat, ... )
{
	/*
	va_list vl;
	static char sTraceString[500];

	va_start(vl, sFormat);
	vsprintf_s(sTraceString, sFormat, vl);
	va_end(vl);
	NET_ASSERT(strlen(sTraceString) < 500) 
	NetLog( sTraceString );

	va_list vl;
	static char sTraceString[500];

	va_start(vl, sFormat);
	vsprintf_s(sTraceString, sFormat, vl);
	va_end(vl);
	NET_ASSERT(strlen(sTraceString) < 500) 
	::OutputDebugString(sTraceString);*/
	
}
#else
inline void __NET_TRACE(const char *sFormat, ... )
{
	va_list vl;
	static char sTraceString[500];

	va_start(vl, sFormat);
	vsprintf_s(sTraceString, sFormat, vl);
	va_end(vl);
	NET_ASSERT(strlen(sTraceString) < 500)
	cout << sTraceString;
	
}

#endif	//PS2

#if 1

#define NET_TRACE __NET_TRACE

#else

#define NET_TRACE 1?(void)0 : __NET_TRACE;

#endif //NET____TRACE

#endif //_DEBUG

struct IStreamAllocator
{
	virtual void *Alloc(size_t size, void * callerOverride = 0)=0;
	virtual void *Realloc(void *old,size_t size)=0;
	virtual void Free(void *old)=0;
};


class CDefaultStreamAllocator : public IStreamAllocator
{
	void *Alloc(size_t size, void *) { return malloc(size); }
	void *Realloc(void *old,size_t size) { return realloc(old,size); }
	void Free(void *old) { free(old); }
};

#include "IValidator.h"			// MAX_WARNING_LENGTH
#include "ISystem.h"				// NetLog
#include "NetLog.h"

#if _MSC_VER > 1000
#pragma intrinsic(memcpy)
#endif

#if !defined(PS3)
#if __WITH_PB__
	#include "PunkBuster/pbcommon.h"
#endif __WITH_PB__
#endif

#include "objcnt.h"

#if defined(_MSC_VER)
extern "C" void * _ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)
#define UP_STACK_PTR _ReturnAddress()
#else
#define UP_STACK_PTR 0
#endif

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__0F62BDE9_4784_4F7A_A265_B7D1A71883D0__INCLUDED_)
