// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__844E5BAB_B810_40FC_8939_167146C07AED__INCLUDED_)
#define AFX_STDAFX_H__844E5BAB_B810_40FC_8939_167146C07AED__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <CryModuleDefs.h>
#define eCryModule eCryM_ScriptSystem
#define RWI_NAME_TAG "RayWorldIntersection(Script)"
#define PWI_NAME_TAG "PrimitiveWorldIntersection(Script)"

#define CRYSCRIPTSYSTEM_EXPORTS

#include <platform.h>

#include <vector>
#include <map>

#ifdef PS2
#include "iostream.h"
//wrapper for VC specific function
inline void itoa(int n, char *str, int basen)
{
 	sprintf(str,"%d", n);
}
inline char *_strlwr(const char *str)
{
    return PS2strlwr(str);
}
#endif

//#define DEBUG_LUA_STATE

#include <ISystem.h>
#include <StlUtils.h>
#include <CrySizer.h>
#include <PoolAllocator.h>
#include "CryTypeInfo.h"

//////////////////////////////////////////////////////////////////////////
//! Reports a Game Warning to validator with WARNING severity.
void ScriptWarning(const char *, ...) PRINTF_PARAMS(1, 2);
inline void ScriptWarning( const char *format,... )
{
	if (!format)
		return;

	char buffer[MAX_WARNING_LENGTH];
	va_list args;
	va_start(args, format);
	vsprintf_s(buffer, sizeof(buffer), format, args);
	va_end(args);
	CryWarning( VALIDATOR_MODULE_SCRIPTSYSTEM,VALIDATOR_WARNING,buffer );
}


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__844E5BAB_B810_40FC_8939_167146C07AED__INCLUDED_)
