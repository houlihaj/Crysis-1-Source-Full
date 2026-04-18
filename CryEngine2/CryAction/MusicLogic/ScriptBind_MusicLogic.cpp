/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 28:08:2006 : Created by TomasN

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_MusicLogic.h"
#include "MusicLogic.h"
#include "../CryAction/IAnimationGraph.h"
//#include "Game.h"
//#include "Menus/FlashMenuObject.h"


//------------------------------------------------------------------------
CScriptBind_MusicLogic::CScriptBind_MusicLogic(CMusicLogic *pMusicLogic)
{
	m_pSystem			= gEnv->pSystem;
	m_pSS					= gEnv->pScriptSystem;
	m_pMusicLogic = pMusicLogic;

	Init(m_pSS, m_pSystem);
	SetGlobalName("MusicLogic");

	RegisterMethods();
	RegisterGlobals();
}

//------------------------------------------------------------------------
CScriptBind_MusicLogic::~CScriptBind_MusicLogic()
{
}

//------------------------------------------------------------------------
void CScriptBind_MusicLogic::RegisterGlobals()
{
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_ENTER_VEHICLE",eMUSICLOGICEVENT_VEHICLE_ENTER);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_LEAVE_VEHICLE",eMUSICLOGICEVENT_VEHICLE_LEAVE);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_MOUNT_WEAPON",eMUSICLOGICEVENT_WEAPON_MOUNT);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_UNMOUNT_WEAPON",eMUSICLOGICEVENT_WEAPON_UNMOUNT);
	
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_SNIPERMODE_ENTER",eMUSICLOGICEVENT_SNIPERMODE_ENTER);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_SNIPERMODE_LEAVE",eMUSICLOGICEVENT_SNIPERMODE_LEAVE);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_CLOAKMODE_ENTER",eMUSICLOGICEVENT_CLOAKMODE_ENTER);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_CLOAKMODE_LEAVE",eMUSICLOGICEVENT_CLOAKMODE_LEAVE);

	m_pSS->SetGlobalValue("MUSICLOGICEVENT_ENEMY_SPOTTED",eMUSICLOGICEVENT_ENEMY_SPOTTED);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_ENEMY_KILLED",eMUSICLOGICEVENT_ENEMY_KILLED);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_ENEMY_HEADSHOT",eMUSICLOGICEVENT_ENEMY_HEADSHOT);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_ENEMY_OVERRUN",eMUSICLOGICEVENT_ENEMY_OVERRUN);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_PLAYER_WOUNDED",eMUSICLOGICEVENT_PLAYER_WOUNDED);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_PLAYER_KILLED",eMUSICLOGICEVENT_PLAYER_KILLED);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_PLAYER_SPOTTED",eMUSICLOGICEVENT_PLAYER_SPOTTED);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_PLAYER_TURRET_ATTACK",eMUSICLOGICEVENT_PLAYER_TURRET_ATTACK);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_PLAYER_SWIM_ENTER",eMUSICLOGICEVENT_PLAYER_SWIM_ENTER);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_PLAYER_SWIM_LEAVE",eMUSICLOGICEVENT_PLAYER_SWIM_LEAVE);

	m_pSS->SetGlobalValue("MUSICLOGICEVENT_EXPLOSION",eMUSICLOGICEVENT_EXPLOSION);

	m_pSS->SetGlobalValue("MUSICLOGICEVENT_FACTORY_CAPTURED",eMUSICLOGICEVENT_FACTORY_CAPTURED);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_FACTORY_LOST",eMUSICLOGICEVENT_FACTORY_LOST);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_FACTORY_RECAPTURED",eMUSICLOGICEVENT_FACTORY_RECAPTURED);
	m_pSS->SetGlobalValue("MUSICLOGICEVENT_VEHICLE_CREATED",eMUSICLOGICEVENT_VEHICLE_CREATED);

}

//------------------------------------------------------------------------
void CScriptBind_MusicLogic::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_MusicLogic::

	//SCRIPT_REG_TEMPLFUNC(SetMusicState, "fIntensity,fBoredom");
	SCRIPT_REG_TEMPLFUNC(SetEvent, "eMusicLogicEvent, fValue");
	SCRIPT_REG_TEMPLFUNC(StartLogic, "");
	SCRIPT_REG_TEMPLFUNC(StopLogic, "");

	//SCRIPT_REG_TEMPLFUNC(PauseGame, "pause");

#undef SCRIPT_REG_CLASSNAME
}

////------------------------------------------------------------------------
//int CScriptBind_MusicLogic::SetMusicState(IFunctionHandler *pH, float fIntensity, float fBoredom)
//{
//	if (m_pMusicLogic)
//	{
//		CMusicLogic::SMusicStateInfo MusicInfo;
//		MusicInfo.fIntensity = fIntensity;
//		MusicInfo.fBoredom = fBoredom;
//		m_pMusicLogic->SetMusicStateInfo(&MusicInfo);
//	}
//
//	return pH->EndFunction();
//}

//------------------------------------------------------------------------
int CScriptBind_MusicLogic::SetEvent(IFunctionHandler *pH, int eMusicLogicEvent)
{

	if (m_pMusicLogic)
	{
		switch(eMusicLogicEvent)
		{
		case eMUSICLOGICEVENT_VEHICLE_ENTER:
			m_pMusicLogic->SetEvent(eMUSICLOGICEVENT_VEHICLE_ENTER);
			break;
		case eMUSICLOGICEVENT_VEHICLE_LEAVE:
			m_pMusicLogic->SetEvent(eMUSICLOGICEVENT_VEHICLE_LEAVE);
			break;
		case eMUSICLOGICEVENT_WEAPON_MOUNT:
			m_pMusicLogic->SetEvent(eMUSICLOGICEVENT_WEAPON_MOUNT);			
			break;
		case eMUSICLOGICEVENT_WEAPON_UNMOUNT:
			m_pMusicLogic->SetEvent(eMUSICLOGICEVENT_WEAPON_UNMOUNT);
			break;
		case eMUSICLOGICEVENT_ENEMY_SPOTTED:
			m_pMusicLogic->SetEvent(eMUSICLOGICEVENT_ENEMY_SPOTTED);
			break;
		case eMUSICLOGICEVENT_ENEMY_KILLED:
			m_pMusicLogic->SetEvent(eMUSICLOGICEVENT_ENEMY_KILLED);
			break;
		case eMUSICLOGICEVENT_ENEMY_HEADSHOT:
			m_pMusicLogic->SetEvent(eMUSICLOGICEVENT_ENEMY_HEADSHOT);
			break;
		case eMUSICLOGICEVENT_ENEMY_OVERRUN:
			m_pMusicLogic->SetEvent(eMUSICLOGICEVENT_ENEMY_OVERRUN);
			break;
		case eMUSICLOGICEVENT_PLAYER_WOUNDED:
			m_pMusicLogic->SetEvent(eMUSICLOGICEVENT_PLAYER_WOUNDED);
			break;
		case eMUSICLOGICEVENT_MAX:
			break;
		default:

			break;
		}

	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
//int CScriptBind_MusicLogic::PauseGame( IFunctionHandler *pH, bool pause )
//{
//	bool forced = false;
//
//	if (pH->GetParamCount() > 1)
//	{
//		pH->GetParam(2, forced);
//	}
//	m_pGameFW->PauseGame(pause, forced);
//
//	return pH->EndFunction();
//}


//------------------------------------------------------------------------
int CScriptBind_MusicLogic::StartLogic(IFunctionHandler *pH)
{
	if (m_pMusicLogic)
		m_pMusicLogic->Start();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_MusicLogic::StopLogic(IFunctionHandler *pH)
{
	if (m_pMusicLogic)
		m_pMusicLogic->Stop();

	return pH->EndFunction();
}