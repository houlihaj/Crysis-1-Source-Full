#include "StdAfx.h"
#include "CET_ActionMap.h"
#include "IPlayerProfiles.h"
#include "NetHelpers.h"

/*
 * disable the action map
 */

class CCET_DisableActionMap : public CCET_Base
{
public:
	const char * GetName() { return "DisableActionMap"; }

	EContextEstablishTaskResult OnStep( SContextEstablishState& state )
	{
		IActionMapManager *pActionMapMan = CCryAction::GetCryAction()->GetIActionMapManager();
		pActionMapMan->Enable(false);
		pActionMapMan->EnableActionMap(0, true); // enable all actionmaps
		pActionMapMan->EnableFilter(0, false); // disable all actionfilters
		return eCETR_Ok;
	}
};

void AddDisableActionMap( IContextEstablisher * pEst, EContextViewState state )
{
	pEst->AddTask( state, new CCET_DisableActionMap );
}

/*
 * action map initialization
 */

class CCET_InitActionMap : public CCET_Base
{
public:
	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
    IActionMapManager *pActionMapMan = CCryAction::GetCryAction()->GetIActionMapManager();
    assert(pActionMapMan);
    
    IActionMap* pDefaultActionMap = 0;
		IActionMap* pDebugActionMap = 0;
		IActionMap* pPlayerActionMap = 0;
		if (true)
		{
			IPlayerProfileManager* pPPMgr = CCryAction::GetCryAction()->GetIPlayerProfileManager();
			if (pPPMgr)
			{
				int userCount = pPPMgr->GetUserCount();
				if (userCount > 0)
				{
					IPlayerProfileManager::SUserInfo info;
					pPPMgr->GetUserInfo(0, info);
					IPlayerProfile* pProfile = pPPMgr->GetCurrentProfile(info.userId);
					if (pProfile)
					{
						pDefaultActionMap = pProfile->GetActionMap("default");
						pDebugActionMap = pProfile->GetActionMap("debug");
						pPlayerActionMap = pProfile->GetActionMap("player");
						if (pDefaultActionMap == 0 && pPlayerActionMap == 0)
						{
							GameWarning("[PlayerProfiles] CGameContext::StartGame: User '%s' has no actionmap 'default'!", info.userId);
							//return eCETR_Failed;
						}
					}
					else
					{
						GameWarning("[PlayerProfiles] CGameContext::StartGame: User '%s' has no active profile!", info.userId);
						//return eCETR_Failed;
					}
				}
				else
				{
					GameWarning("[PlayerProfiles] CGameContext::StartGame: No users logged in");
					return eCETR_Ok;
				}
			}
		}

		if (pDefaultActionMap == 0 )
		{
			// use action map without any profile stuff
			pActionMapMan->EnableActionMap( "default", true );
			pDefaultActionMap = pActionMapMan->GetActionMap("default");
		}

		if (pDebugActionMap == 0 )
		{
			// use action map without any profile stuff
			pActionMapMan->EnableActionMap( "debug", true );
			pDebugActionMap = pActionMapMan->GetActionMap("debug");
		}

		if (pPlayerActionMap == 0)
		{
			pActionMapMan->EnableActionMap( "player", true );
			pPlayerActionMap = pActionMapMan->GetActionMap("player");
		}

		if (!pDefaultActionMap)
			return eCETR_Failed;

		EntityId actorId = GetListener();
		if (!actorId)
			return eCETR_Wait;

		if(pDefaultActionMap)
		{
			pDefaultActionMap->SetActionListener( actorId );
		}
		if(pDebugActionMap)
		{
			pDebugActionMap->SetActionListener( actorId );
		}
		if(pPlayerActionMap)
		{
			pPlayerActionMap->SetActionListener( actorId );
		}
		CCryAction::GetCryAction()->GetIActionMapManager()->Enable(true);

		return eCETR_Ok;
	}

private:
	virtual EntityId GetListener() = 0;
};

class CCET_InitActionMap_ClientActor : public CCET_InitActionMap
{
private:
	const char * GetName() { return "InitActionMap.ClientActor"; }

	EntityId GetListener()
	{
		if (IActor * pActor = CCryAction::GetCryAction()->GetClientActor())
			return pActor->GetEntityId();
		return 0;
	}
};

class CCET_InitActionMap_OriginalSpectator : public CCET_InitActionMap
{
private:
	const char * GetName() { return "InitActionMap.OriginalSpectator"; }

	EntityId GetListener()
	{
		if (IActor * pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetOriginalDemoSpectator())
			return pActor->GetEntityId();
		return 0;
	}
};


void AddInitActionMap_ClientActor( IContextEstablisher * pEst, EContextViewState state )
{
	pEst->AddTask( state, new CCET_InitActionMap_ClientActor() );
}

void AddInitActionMap_OriginalSpectator( IContextEstablisher * pEst, EContextViewState state )
{
	pEst->AddTask( state, new CCET_InitActionMap_OriginalSpectator() );
}

class CCET_DisableKeyboardMouse : public CCET_Base
{
public:
	const char * GetName() { return "DisableKeyboardMouse"; }

	EContextEstablishTaskResult OnStep( SContextEstablishState& state )
	{
		if ( ICVar* cvar = gEnv->pConsole->GetCVar("sv_requireinputdevice") )
		{
			if ( 0 == strcmpi(cvar->GetString(), "gamepad") )
			{
				IInput* pInput = gEnv->pInput;
				pInput->EnableDevice(eDI_Keyboard, false);
				pInput->EnableDevice(eDI_Mouse, false);
			}
		}

		return eCETR_Ok;
	}
};

void AddDisableKeyboardMouse( IContextEstablisher * pEst, EContextViewState state )
{
	pEst->AddTask( state, new CCET_DisableKeyboardMouse() );
}
