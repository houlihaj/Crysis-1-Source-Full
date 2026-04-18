////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   ScriptBind_Script.h
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ScriptBind_Script_h__
#define __ScriptBind_Script_h__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <IScriptSystem.h>

/*
	<title Script>
	Syntax: Script

	This class implements script-functions for exposing the scripting system functionalities

	REMARKS:
	After initialization of the script-object it will be globally accessable through scripts using the namespace "Script".
	
	Example:
		Script.LoadScript("scripts/common.lua")

	IMPLEMENTATIONS NOTES:
	These function will never be called from C-Code. They're script-exclusive.
*/
class CScriptBind_Script : public CScriptableBase
{
public:
	CScriptBind_Script(IScriptSystem *pScriptSystem, ISystem *pSystem);
	virtual ~CScriptBind_Script();
	virtual void GetMemoryStatistics(ICrySizer *pSizer) { pSizer->Add(this); };

	int LoadScript(IFunctionHandler *pH);
	int ReloadScripts(IFunctionHandler *pH);
	int ReloadScript(IFunctionHandler *pH);
	int ReloadEntityScript(IFunctionHandler *pH, const char *className);
	int UnloadScript(IFunctionHandler *pH);
	int DumpLoadedScripts(IFunctionHandler *pH);
	int Debug(IFunctionHandler *pH);
	int DebugFull(IFunctionHandler *pH);


	// <title SetTimer>
	// Syntax: Script.SetTimer( int nMilliseconds,luaFunction,[optional]Table userData,[optional]bool bUpdateDuringPause )
	// Description:
	//    Set a general script timer, when timer expires will call back a specified lua function.
	//    Lua function will accept 1 or 2 parameters,
	//    if useData is specified lua function must be:
	//      LuaCallback = function( usedata,nTimerId )
	//      end;
	//    if useData is not specified lua function must be:
	//      LuaCallback = function( nTimerId )
	//      end;
	// Arguments:
	//    nMilliseconds - Delay of trigger in milliseconds.
	//    userData - Any user defines table, if specified will be passed as a first argument of the callback function..
	//    bUpdateDuringPause - will be updated and trigger even if in pause mode.
	// Return:
	//    ID assigned to this timer or nil if not specified.
	int SetTimer(IFunctionHandler *pH,int nMilliseconds,HSCRIPTFUNCTION hFunc );


	// <title SetTimerForFunction>
	// Syntax: Script.SetTimerForFunction( int nMilliseconds,luaFunction name,[optional]Table userData,[optional]bool bUpdateDuringPause )
	// Description:
	//    Set a general script timer, when timer expires will call back a specified lua function.
	//    Lua function will accept 1 or 2 parameters,
	//    if useData is specified lua function must be:
	//      LuaCallback = function( usedata,nTimerId )
	//      end;
	//    if useData is not specified lua function must be:
	//      LuaCallback = function( nTimerId )
	//      end;
	// Arguments:
	//    nMilliseconds - Delay of trigger in milliseconds.
	//    userData - Any user defines table, if specified will be passed as a first argument of the callback function..
	//    bUpdateDuringPause - will be updated and trigger even if in pause mode.
	// Return:
	//    ID assigned to this timer or nil if not specified.
	int SetTimerForFunction(IFunctionHandler *pH,int nMilliseconds,const char *sFunctionName );

	// <title KillTimer>
	// Syntax: Script.KillTimer( int nTimerId )
	// Description:
	//    Stops a timer set by the Script.SetTimer function.
	// Arguments:
	//    nTimerId - ID of the timer returned by the Script.SetTimer function.
	int KillTimer(IFunctionHandler *pH,ScriptHandle nTimerId);

private: // -------------------------------------------------------------------------

	//! recursive
	//! /return amount of table elements (recursive)
	int Debug_Buckets_recursive( IScriptTable *pCurrent, string &sPath, std::set<const void *> &setVisited, const int dwMinBucket );

	//! not recursive
	void Debug_Elements( IScriptTable *pCurrent, string &sPath, std::set<const void *> &setVisited );

	//! recursive
	void Debug_Full_recursive( IScriptTable *pCurrent, string &sPath, std::set<const void *> &setVisited );
};

#endif // __ScriptBind_Script_h__