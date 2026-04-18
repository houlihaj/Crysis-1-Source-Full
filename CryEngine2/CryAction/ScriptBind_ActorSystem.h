/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Exposes basic Actor System to the Script System.

-------------------------------------------------------------------------
History:
- 21:9:2004   3:00 : Created by Mathieu Pinard

*************************************************************************/
#ifndef __SCRIPTBIND_ACTORSYSTEM_H__
#define __SCRIPTBIND_ACTORSYSTEM_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IScriptSystem.h>
#include <ScriptHelpers.h>

struct IActorSystem;
struct IGameFramework;

// <title ActorSystem>
// Syntax: ActorSystem
class CScriptBind_ActorSystem :
	public CScriptableBase
{
public:
	CScriptBind_ActorSystem( ISystem *pSystem, IGameFramework *pGameFW );
	virtual ~CScriptBind_ActorSystem();

	void Release() { delete this; };

	int CreateActor( IFunctionHandler *pH, int channelId, SmartScriptTable actorParams );
	int GetActors(IFunctionHandler *pH);

private:
	void RegisterGlobals();
	void RegisterMethods();

  ISystem        *m_pSystem;
	IScriptSystem  *m_pSS;
	IGameFramework *m_pGameFW;
};

#endif //__SCRIPTBIND_ACTORSYSTEM_H__