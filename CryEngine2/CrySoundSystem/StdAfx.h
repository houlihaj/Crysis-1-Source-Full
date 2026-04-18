// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__9793C644_C91F_4BA6_A176_44537782901A__INCLUDED_)
#define AFX_STDAFX_H__9793C644_C91F_4BA6_A176_44537782901A__INCLUDED_

#include <CryModuleDefs.h>
#define eCryModule eCryM_SoundSystem
#define RWI_NAME_TAG "RayWorldIntersection(Sound)"
#define PWI_NAME_TAG "PrimitiveWorldIntersection(Sound)"

#define CRYSOUNDSYSTEM_EXPORTS

#include "platform.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// temporary memory leak detection
//////////////////////////////////////////////////////////////////////////
//#include "VisualLeakDetector/vld.h"
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Choose one of these SoundLibraries

// FMODEX 400 Win32/Xenon
//#define SOUNDSYSTEM_USE_FMODEX400
//#undef SOUNDSYSTEM_USE_FMODEX400
//
// XAudio Xenon only
//#define SOUNDSYSTEM_USE_XENON_XAUDIO
//#undef SOUNDSYSTEM_USE_XENON_XAUDIO

// Disable Prototype sound library choice

#if defined(_WIN32) && !defined(_WIN64) && !defined(XENON) && !defined(PS3)

	#		pragma message (">>> Win32:")
#define SOUNDSYSTEM_USE_FMODEX400
#undef SOUNDSYSTEM_USE_XENON_XAUDIO

#if defined(CRYSOUNDSYSTEM_FMOD_RELEASE)
#		pragma message (">>> include lib: fmodex/lib/fmodex_vc.lib")
#		pragma comment(lib,"fmodex/lib/fmodex_vc.lib")
#		pragma message (">>> include lib: fmodex/lib/fmod_event.lib")
#		pragma comment(lib,"fmodex/lib/fmod_event.lib")
#		pragma message (">>> include lib: fmodex/lib/fmod_event_net.lib")
#		pragma comment(lib,"fmodex/lib/fmod_event_net.lib")
#elif defined(CRYSOUNDSYSTEM_FMOD_DEBUG)
	#		pragma message (">>> include lib: fmodex/lib/fmodex_vcD.lib")
	#		pragma comment(lib,"fmodex/lib/fmodex_vcD.lib")
	#		pragma message (">>> include lib: fmodex/lib/fmod_eventD.lib")
	#		pragma comment(lib,"fmodex/lib/fmod_eventD.lib")
	#		pragma message (">>> include lib: fmodex/lib/fmod_event_netD.lib")
	#		pragma comment(lib,"fmodex/lib/fmod_event_netD.lib")
#endif

#endif



#if defined(_WIN64) && !defined(XENON) && !defined(PS3)

	#		pragma message (">>> Win64:")
	#define SOUNDSYSTEM_USE_FMODEX400
	#undef SOUNDSYSTEM_USE_XENON_XAUDIO

#if defined(CRYSOUNDSYSTEM_FMOD_RELEASE)
#		pragma message (">>> include lib: fmodex/lib/x64/fmodex64_vc.lib")
#		pragma comment(lib,"fmodex/lib/x64/fmodex64_vc.lib")
#		pragma message (">>> include lib: fmodex/lib/x64/fmod_event64.lib")
#		pragma comment(lib,"fmodex/lib/x64/fmod_event64.lib")
#		pragma message (">>> include lib: fmodex/lib/x64/fmod_event_net64.lib")
#		pragma comment(lib,"fmodex/lib/x64/fmod_event_net64.lib")
#elif defined(CRYSOUNDSYSTEM_FMOD_DEBUG)
#		pragma message (">>> include lib: fmodex/lib/x64/fmodex64_vc.lib")
#		pragma comment(lib,"fmodex/lib/x64/fmodex64_vc.lib")
#		pragma message (">>> include lib: fmodex/lib/x64/fmod_event64.lib")
#		pragma comment(lib,"fmodex/lib/x64/fmod_event64.lib")
#		pragma message (">>> include lib: fmodex/lib/x64/fmod_event_net64.lib")
#		pragma comment(lib,"fmodex/lib/x64/fmod_event_net64.lib")
#endif

#endif


#if defined(XENON)

	#define _XENON
	#		pragma message (">>> Xbox360:")
	#include "FmodEx/inc/fmodxbox360.h"
	#define SOUNDSYSTEM_USE_FMODEX400
	#undef SOUNDSYSTEM_USE_XENON_XAUDIO

	#pragma comment(lib,"xaudio.lib")
	#pragma message (">>> include lib: xaudio.lib")
	#pragma comment(lib,"x3daudio.lib")
  #pragma message (">>> include lib: x3daudio.lib")
	#pragma comment(lib,"xnet.lib")
	#pragma message (">>> include lib: xnet.lib")
	#pragma comment(lib,"xmp.lib")
	#pragma message (">>> include lib: xmp.lib")


	//#pragma comment(lib,"libcmtd.lib")
	//#pragma message (">>> include lib: libcmtd.lib")

	#if defined(CRYSOUNDSYSTEM_FMOD_RELEASE)
	#		pragma message (">>> include lib: fmodex/lib/xbox360/fmodxbox360.lib")
	#		pragma comment(lib,"fmodex/lib/xbox360/fmodxbox360.lib")
	#		pragma message (">>> include lib: fmodex/lib/xbox360/fmod_event.lib")
	#		pragma comment(lib,"fmodex/lib/xbox360/fmod_event.lib")
	#		pragma message (">>> include lib: fmodex/lib/xbox360/fmod_event_net.lib")
	#		pragma comment(lib,"fmodex/lib/xbox360/fmod_event_net.lib")
	#elif defined(CRYSOUNDSYSTEM_FMOD_DEBUG)

	//#pragma comment(lib,"xbdm.lib")
	//#pragma message (">>> include lib: xbdm.lib")

	#		pragma message (">>> include lib: fmodex/lib/xbox360/fmodxbox360D.lib")
	#		pragma comment(lib,"fmodex/lib/xbox360/fmodxbox360D.lib")
	#		pragma message (">>> include lib: fmodex/lib/xbox360/fmod_eventD.lib")
	#		pragma comment(lib,"fmodex/lib/xbox360/fmod_eventD.lib")
	#		pragma message (">>> include lib: fmodex/lib/xbox360/fmod_event_netD.lib")
	#		pragma comment(lib,"fmodex/lib/xbox360/fmod_event_netD.lib")
#endif

#endif

#if defined(PS3)

	#define SOUNDSYSTEM_USE_FMODEX400
	#define USE_RSX_MEM
	#define USE_SPURS
	#if defined(USE_RSX_MEM)
		extern unsigned int PS3_MEM_FLAG;
	#else
		#define PS3_MEM_FLAG 0
	#endif
	#include "FmodEx/inc/fmodps3.h"
	#undef SOUNDSYSTEM_USE_XENON_XAUDIO
#else
	#define PS3_MEM_FLAG 0
#endif

#include "ProjectDefines.h"


#include <map>
#include <vector>
#include <set>
#include <algorithm>

#include <ILog.h>
#include <IConsole.h>
#include <ISound.h>
#include <ISystem.h>

#include <Cry_Math.h>
#include <ICryPak.h>
//#include <vector.h>


#ifdef PS2
inline void __CRYTEKDLL_TRACE(const char *sFormat, ... )
#else
_inline void __cdecl __CRYTEKDLL_TRACE(const char *sFormat, ... ) PRINTF_PARAMS(1, 2);
_inline void __cdecl __CRYTEKDLL_TRACE(const char *sFormat, ... )
#endif
{
	va_list vl;
	static char sTraceString[1024];
	
	va_start(vl, sFormat);
	vsprintf_s(sTraceString, sFormat, vl);
	va_end(vl);

	strcat(sTraceString, "\n");

	CryLog( "%s",sTraceString );
	//::OutputDebugString(sTraceString);	
}

#define TRACE __CRYTEKDLL_TRACE

#if defined(_DEBUG) && defined(WIN32)
//class CHeapGuardian
//{
//public: CHeapGuardian() {assert (_CrtCheckMemory());} ~CHeapGuardian() {assert (_CrtCheckMemory());}
//};
//#define GUARD_HEAP //CHeapGuardian __heap_guardian

#define GUARD_HEAP	//if (_HEAPOK != _heapchk()) __asm int 3

#else
#define GUARD_HEAP
#endif

// TODO: reference additional headers your program requires here

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__9793C644_C91F_4BA6_A176_44537782901A__INCLUDED_)
