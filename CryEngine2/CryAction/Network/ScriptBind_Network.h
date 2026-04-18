/*************************************************************************
	Crytek Source File.
	Copyright (C), Crytek Studios, 2001-2004.
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: Binding of network functions into script

	-------------------------------------------------------------------------
	History:
	- 24:11:2004   11:30 : Created by Craig Tiller

*************************************************************************/
#ifndef __SCRIPTBIND_NETWORK_H__
#define __SCRIPTBIND_NETWORK_H__

#pragma once

#include <IScriptSystem.h>
#include <ScriptHelpers.h>

class CGameContext;
class CCryAction;

// <title Network>
// Syntax: Network
class CScriptBind_Network :
	public CScriptableBase
{
public:
	CScriptBind_Network( ISystem *pSystem, CCryAction *pFW );
	virtual ~CScriptBind_Network();

	void Release() { delete this; };

	int Expose( IFunctionHandler * pFH );


	// Delegate authority for an object to some client
	int DelegateAuthority( IFunctionHandler * pFH, ScriptHandle ent, int channel );

private:
	void RegisterGlobals();
	void RegisterMethods();

	ISystem        *m_pSystem;
	IScriptSystem  *m_pSS;
	CCryAction     *m_pFW;
};

#endif
