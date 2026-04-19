////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   stdafx.h
//  Version:     v1.00
//  Created:     30/9/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Precompiled Header.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __stdafx_h__
#define __stdafx_h__

#define _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS  // silence <hash_map> in deprecated warning  // new implementation

#if _MSC_VER > 1000
#pragma once
#endif

#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0400
#endif

#include <CryModuleDefs.h>
#define eCryModule eCryM_System

#define CRYSYSTEM_EXPORTS

#include <platform.h>
//////////////////////////////////////////////////////////////////////////
// CRT
//////////////////////////////////////////////////////////////////////////
#include <string.h>
#if !defined(PS3)
	#include <memory.h>
	#include <malloc.h>
#endif
#include <fcntl.h>

#if !defined(PS3)
	#if defined( LINUX )
		#	include <sys/io.h>
	#else
		#	include <io.h>
	#endif
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#undef GetCharWidth
#undef GetUserName
#endif

/////////////////////////////////////////////////////////////////////////////
// STL //////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#include <vector>
#include <list>
#include <map>
#if defined(LINUX)
	#include <ext/hash_map>
#else
	#include <hash_map>
#endif
#include <set>
#include <stack>
#include <deque>
#include <algorithm>
#include <memory>  // std::auto_ptr  // new implementation

#include "platform.h"
// If not XBOX/GameCube/...
#ifdef WIN32
//#include <process.h>
#include <malloc.h>  // _set_sbh_threshold  // new implementation
#endif

/////////////////////////////////////////////////////////////////////////////
// CRY Stuff ////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#include "Cry_Math.h"
#include <smartptr.h>
#include <Range.h>
#include <CrySizer.h>
#include <StlUtils.h>


static inline int
RoundToClosestMB( size_t memSize )
{
	// add half a MB and shift down to get closest MB
	return( (int) ( ( memSize + ( 1 << 19 ) ) >> 20 ) );
}

/////////////////////////////////////////////////////////////////////////////
//forward declarations for common Interfaces.
/////////////////////////////////////////////////////////////////////////////
class ITexture;
struct IRenderer;
struct ISystem;
struct IScriptSystem;
struct ITimer;
struct IFFont;
struct IInput;
struct IKeyboard;
struct ICVar;
struct IConsole;
struct IGame;
struct IEntitySystem;
struct IProcess;
struct ICryPak;
struct ICryFont;
struct I3DEngine;
struct IMovieSystem;
struct ISoundSystem;
struct IPhysicalWorld;

#endif // __stdafx_h__
