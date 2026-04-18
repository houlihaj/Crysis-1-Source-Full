////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MusicLogic.h
//  Version:     v1.00
//  Created:     24/08/2006 by Tomas.
//  Compilers:   Visual Studio.NET
//  Description: implementation of the MusicLogic class.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "IAISystem.h"
#include "IAgent.h"
#include "IGame.h"
#include "IGameFramework.h"
#include "IMusicSystem.h"
#include "IActorSystem.h"
#include <IRenderer.h>

#include "MusicLogic.h"


//////////////////////////////////////////////////////////////////////////
// Initialization
//////////////////////////////////////////////////////////////////////////

CMusicLogic::CMusicLogic(void)
{
	m_pMusicSystem	= 0;
	m_pMusicState = 0;
	m_fMultiplier = 1.0f;
	m_bActive = false;
	m_bHitListener = false;
	m_tLastUpdate.SetMilliSeconds(0);

	m_AI_Intensity_ID = 0;
	m_Player_Intensity_ID = 0;
	m_Allow_Change_ID = 0;
	m_Game_Intensity_ID = 0;
	m_MusicTime_ID = 0;
	m_MoodTime_ID = 0;
	m_SilenceTime_ID = 0;

}

CMusicLogic::~CMusicLogic(void)
{
	if (m_bHitListener && gEnv->pGame->GetIGameFramework())
		if (gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem())
			if (gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules())
				gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules()->RemoveHitListener(this);

}

bool CMusicLogic::Init()
{
	m_pMusicState = gEnv->pGame->GetIGameFramework()->GetMusicGraphState();

	m_pMusicSystem	= gEnv->pMusicSystem;

	// start with disabled musiclogic
	Stop();

	m_AI_Intensity_ID			= m_pMusicState->GetInputId("AI_Intensity");
	m_Player_Intensity_ID = m_pMusicState->GetInputId("Player_Intensity");
	m_Allow_Change_ID			= m_pMusicState->GetInputId("Allow_Change");
	m_Game_Intensity_ID		= m_pMusicState->GetInputId("Game_Intensity");
	m_MusicTime_ID				= m_pMusicState->GetInputId("PlayingTime_Music");
	m_MoodTime_ID					= m_pMusicState->GetInputId("PlayingTime_Mood");
	m_SilenceTime_ID			= m_pMusicState->GetInputId("PlayingTime_Silence");

	return (m_pMusicState != NULL);
}

bool CMusicLogic::Start()
{

	if (m_pMusicState)
	{
		m_pMusicState->SetInput("Mode", "running");
		m_bActive = true;
	}
	
	//m_CurrTime = gEnv->pTimer->GetFrameStartTime();

	if(!m_pMusicSystem)
		return false;

	if (stricmp(m_pMusicSystem->GetMood(),"silence")==0)
	{
		m_pMusicState->PushForcedState("theme_any+silence");
		m_pMusicState->ForceTeleportToQueriedState();
	}

	if (stricmp(m_pMusicSystem->GetMood(),"ambient")==0)
	{
		m_pMusicState->PushForcedState("theme_any+ambient");
		m_pMusicState->ForceTeleportToQueriedState();
	}

	if (stricmp(m_pMusicSystem->GetMood(),"incidental")==0)
	{
		m_pMusicState->PushForcedState("theme_any+incidental");
		m_pMusicState->ForceTeleportToQueriedState();
	}

	if (stricmp(m_pMusicSystem->GetMood(),"middle")==0)
	{
		m_pMusicState->PushForcedState("theme_any+middle");
		m_pMusicState->ForceTeleportToQueriedState();
	}

	if (stricmp(m_pMusicSystem->GetMood(),"action")==0)
	{
		m_pMusicState->PushForcedState("theme_any+action");
		m_pMusicState->ForceTeleportToQueriedState();
	}

	return true;
}

bool CMusicLogic::Stop()
{
	if (m_pMusicState)
	{
		m_pMusicState->SetInput("Mode", "pauseAll");
		m_bActive = false;
	}
	
	return true;
}


void CMusicLogic::Update()
{
	// Update only every 200ms
	CTimeValue tTimeDif = gEnv->pTimer->GetFrameStartTime() - m_tLastUpdate;

	if (tTimeDif.GetMilliSeconds() < UPDATE_MUSICLOGIC_IN_MS)
		return;

	m_tLastUpdate = gEnv->pTimer->GetFrameStartTime();

	// AAAAAaaaargh
	if (!m_bHitListener && gEnv->pGame->GetIGameFramework())
		if (gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem())
			if (gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules())
			{
				gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules()->AddHitListener(this);
				m_bHitListener = true;
			}

	CTimeValue tNow = gEnv->pTimer->GetFrameStartTime();
	tExplosionList::iterator itEnd = m_listExplosions.end();
	tExplosionList::iterator it=m_listExplosions.begin();
	while (it != itEnd)
	{
		CTimeValue tDiff = tNow - (*it);
		if (tDiff.GetSeconds() > 10 )
			m_listExplosions.pop_front();
		else
			break;

		it = m_listExplosions.begin();
	}


	if (m_pMusicState)
	{
		float fAIIntensity = 0.0f;

		// update AI_Intensity
		IActor *pClientPlayer = gEnv->pGame->GetIGameFramework()->GetClientActor();
		if (pClientPlayer)
		{
			Vec3 vPlayerPos = pClientPlayer->GetEntity()->GetWorldPos();
//			IAIObjectIter *pAIIter = gEnv->pAISystem->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIOBJECT_PUPPET);
			AutoAIObjectIter pAIIter(gEnv->pAISystem->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIOBJECT_PUPPET));

			IAIObject *pAIPlayer = pClientPlayer->GetEntity()->GetAI();

			for (; pAIIter->GetObject(); pAIIter->Next())
			{
				IAIObject *pAIObject = pAIIter->GetObject();
				if (pAIObject->IsEnabled() && pAIObject->IsHostile(pAIPlayer))
				{
					float fDistance = pAIObject->GetPos().GetDistance(vPlayerPos);
					if (fDistance < 100)
					{
						if (pAIObject->GetProxy())
						{
							int nAlertness = pAIObject->GetProxy()->GetAlertnessState() + 1; // we dont want the 0;
							fDistance = max(0.0f, sqr(100.0f-fDistance)/100.0f);
							fAIIntensity += fDistance*nAlertness;
						}
					}
				}
			}

			float fOldValue = m_pMusicState->GetInputAsFloat(m_AI_Intensity_ID);
			fOldValue = max(0.0f, fOldValue - fOldValue*0.005f);	// leak a bit of the old value // TODO replace with timer
			fAIIntensity = max(fAIIntensity*m_fMultiplier, fOldValue);							// take whatever is the higher value
			m_pMusicState->SetInput(m_AI_Intensity_ID, fAIIntensity);
		}

		// update Player_Intensity
		float fPlayer = m_pMusicState->GetInputAsFloat(m_Player_Intensity_ID);
		fPlayer = max(0.0f, fPlayer - fPlayer*0.005f);	// leak a bit of the old value // TODO replace with timer
		m_pMusicState->SetInput(m_Player_Intensity_ID, fPlayer);

		// update Allow_Change
		float fChange = m_pMusicState->GetInputAsFloat(m_Allow_Change_ID);
		fChange = max(0.0f, fChange - fChange*0.005f);	// leak a bit of the old value // TODO replace with timer
		m_pMusicState->SetInput(m_Allow_Change_ID, fChange);

		//tExplosionList::iterator iterEnd = m_listExplosions.end();
		//for (tExplosionList::iterator it=m_listExplosions.begin(); it!=iterEnd; ++it)
		//{
		//	fPlayer += 250;
		//}

		//m_pMusicState->SetInput(Player_ID, fPlayer);

		// update Game_Intensity ( AI_Intensity OR Player_Intensity)
		float fGame = m_pMusicState->GetInputAsFloat(m_Game_Intensity_ID);
		fGame = max(0.0f, fGame - fGame*0.005f);		// leak a bit of the old value // TODO replace with timer
		fGame = max(max(fAIIntensity,fPlayer), fGame);		// take whatever is the higher value
		m_pMusicState->SetInput(m_Game_Intensity_ID, fGame);

		// update Playing Times
		if (m_pMusicSystem)
		{
			CTimeValue tMoodLength = m_pMusicSystem->GetMoodTime();
			CTimeValue tMusicLength;
			CTimeValue tSilenceLength;
			CTimeValue tCurrTime = gEnv->pTimer->GetFrameStartTime();

			const char* sTheme = m_pMusicSystem->GetTheme();
			const char* sMood = m_pMusicSystem->GetMood();
			

			if (strcmp(sMood, "silence") == 0 || *sTheme == 0 || strcmp(sMood, "incidental") == 0)
			{
				if (m_SilenceTime.GetMilliSeconds() == 0.0f)
				{
					// just switched to Silence
					m_SilenceTime = tCurrTime;
					m_MusicTime.SetSeconds(0.0f);
				}
				tSilenceLength = tCurrTime - m_SilenceTime;
			}
			else
			{
				if (m_MusicTime.GetMilliSeconds() == 0.0f)
				{
					// just switched to Music
					m_MusicTime = tCurrTime;
					m_SilenceTime.SetSeconds(0.0f);
				}
				tMusicLength = tCurrTime - m_MusicTime;
			}

			m_pMusicState->SetInput(m_MusicTime_ID, tMusicLength.GetSeconds());
			m_pMusicState->SetInput(m_MoodTime_ID, tMoodLength.GetSeconds());
			m_pMusicState->SetInput(m_SilenceTime_ID, tSilenceLength.GetSeconds());
		}
	}
}

// incoming events
void CMusicLogic::SetMusicEvent(EMusicEvents MusicEvent, const float fValue)
{
	float fAIIntensity			= 0.0f;
	float fPlayerIntensity	= 0.0f;
	float fGameIntensity		= 0.0f;
	float fAllowChange			= 0.0f;
	
	bool bChangeAIIntensity			= false;
	bool bChangePlayerIntensity	= false;
	bool bChangeGameIntensity		= false;
	bool bChangeAllowChange			= false;

	bool bSetAIIntensity			= false;
	bool bSetPlayerIntensity	= false;
	bool bSetGameIntensity		= false;
	bool bSetAllowChange			= false;

	switch(MusicEvent)
	{
	case MUSICEVENT_SET_MULTIPLIER:
		m_fMultiplier = fValue;
		break;
	case MUSICEVENT_SET_AI:
		bSetAIIntensity			= true;
		bChangeAIIntensity	= true;
		fAIIntensity				= fValue;
		break;
	case MUSICEVENT_ADD_AI:
		bChangeAIIntensity	= true;
		fAIIntensity				= fValue;
		break;
	case MUSICEVENT_SET_PLAYER:
		bSetPlayerIntensity			= true;
		bChangePlayerIntensity	= true;
		fPlayerIntensity				= fValue;
		break;
	case MUSICEVENT_ADD_PLAYER:
		bChangePlayerIntensity	= true;
		fPlayerIntensity				= fValue;
		break;
	case MUSICEVENT_SET_GAME:
		bSetGameIntensity			= true;
		bChangeGameIntensity	= true;
		fGameIntensity				= fValue;
		break;
	case MUSICEVENT_ADD_GAME:
		bChangeGameIntensity	= true;
		fGameIntensity				= fValue;
		break;
	case MUSICEVENT_SET_CHANGE:
		bSetAllowChange			= true;
		bChangeAllowChange	= true;
		fAllowChange				= fValue;
		break;
	case MUSICEVENT_ADD_CHANGE:
		bChangeAllowChange	= true;
		fAllowChange				= fValue;
		break;
	case MUSICEVENT_ENTER_VEHICLE:
		bChangeAllowChange	= true;
		bChangePlayerIntensity	= true;
		fAllowChange			+= 100.0f;
		fPlayerIntensity	+= 200.0f;
		break;
	case MUSICEVENT_LEAVE_VEHICLE:
		bChangeAllowChange	= true;
		bChangePlayerIntensity	= true;
		fAllowChange			+= 100.0f;
		fPlayerIntensity	-= 150.0f;
		break;
	case MUSICEVENT_MOUNT_WEAPON:
		bChangeAllowChange	= true;
		bChangePlayerIntensity	= true;
		fAllowChange			+= 200.0f;
		fPlayerIntensity	+= 200.0f;
	    break;
	case MUSICEVENT_UNMOUNT_WEAPON:
		bChangeAllowChange	= true;
		bChangePlayerIntensity	= true;
		fAllowChange			+= 200.0f;
		fPlayerIntensity	-= 150.0f;
	    break;
	case MUSICEVENT_ENEMY_SPOTTED:
		bChangeAllowChange	= true;
		bChangePlayerIntensity	= true;
		fAllowChange			+= 100.0f;
		fPlayerIntensity	+= 50.0f;
		break;
	case MUSICEVENT_ENEMY_KILLED:
		bChangeAllowChange	= true;
		bChangePlayerIntensity	= true;
		fAllowChange			+= 50.0f;
		fPlayerIntensity	+= 50.0f;
		break;
	case MUSICEVENT_ENEMY_HEADSHOT:
		bChangeAllowChange	= true;
		bChangePlayerIntensity	= true;
		fAllowChange			+= 100.0f;
		fPlayerIntensity	+= 100.0f;
	    break;
	case MUSICEVENT_ENEMY_OVERRUN:
		bChangeAllowChange	= true;
		bChangePlayerIntensity	= true;
		fAllowChange			+= 50.0f;
		fPlayerIntensity	+= 100.0f;
	    break;
	case MUSICEVENT_PLAYER_WOUNDED:
		break;
	case MUSICEVENT_EXPLOSION:
		bChangeAllowChange	= true;
		bChangePlayerIntensity	= true;
		fAllowChange			+= 300.0f;
		fPlayerIntensity	+= 50.0f;
		break;
	default:
		break;
	}

	if (bChangeAIIntensity && m_pMusicState)
	{
		float fAI = bSetAIIntensity?(fAIIntensity*m_fMultiplier):(m_pMusicState->GetInputAsFloat(m_AI_Intensity_ID) + fAIIntensity*m_fMultiplier);
		m_pMusicState->SetInput(m_AI_Intensity_ID, fAI);
	}

	if (bChangePlayerIntensity && m_pMusicState)
	{
		float fPlayer = bSetPlayerIntensity?(fPlayerIntensity*m_fMultiplier):(m_pMusicState->GetInputAsFloat(m_Player_Intensity_ID) + fPlayerIntensity*m_fMultiplier);
		m_pMusicState->SetInput(m_Player_Intensity_ID, fPlayer);
	}

	if (bChangeGameIntensity && m_pMusicState)
	{
		float fGame = bSetGameIntensity?(fGameIntensity*m_fMultiplier):(m_pMusicState->GetInputAsFloat(m_Game_Intensity_ID) + fGameIntensity*m_fMultiplier);
		m_pMusicState->SetInput(m_Game_Intensity_ID, fGame);
	}

	if (bChangeAllowChange && m_pMusicState)
	{
		float fChange = bSetAllowChange?(fAllowChange*m_fMultiplier):(m_pMusicState->GetInputAsFloat(m_Allow_Change_ID) + fAllowChange*m_fMultiplier);
		m_pMusicState->SetInput(m_Allow_Change_ID, fChange);
	}


}

void CMusicLogic::OnHit(const HitInfo&)
{

}

void CMusicLogic::OnExplosion(const ExplosionInfo	&ei)
{
	if (m_pMusicState && m_bActive)
	{
		// update Player_Intensity

		IActor *pClientPlayer = gEnv->pGame->GetIGameFramework()->GetClientActor();
		if (pClientPlayer)
		{
			Vec3 vPlayerPos = pClientPlayer->GetEntity()->GetWorldPos();
			float fDistance = vPlayerPos.GetSquaredDistance(ei.pos);
			if (fDistance < 2500)
			{
				SetMusicEvent(MUSICEVENT_EXPLOSION, 0.0f);
				//CTimeValue myTime = gEnv->pTimer->GetFrameStartTime();
				//m_listExplosions.push_back(myTime);
				//IAnimationGraph::InputID ID = m_pMusicState->GetInputId("Player_Intensity");
				//float fOldValue = m_pMusicState->GetInputAsFloat(ID);
				//m_pMusicState->SetInput(ID, fOldValue + 50.0f);
			}
		}

	}
}

void CMusicLogic::OnServerExplosion(const ExplosionInfo& explosion)
{

}


void CMusicLogic::GetMemoryStatistics(ICrySizer *	s)
{
	SIZER_SUBCOMPONENT_NAME(s, "MusicLogic");
	s->Add(*this);
	if (m_pMusicState)
		m_pMusicState->GetMemoryStatistics(s);
	s->AddContainer(m_listExplosions);
}

void CMusicLogic::Serialize(TSerialize ser,unsigned aspects)
{

	float fAI = 0.0f;
	float fPlayer = 0.0f;
	float fGame = 0.0f;
	float fChange = 0.0f;

	ser.Value("m_bActive", m_bActive);
	ser.Value("m_MusicTime", m_MusicTime);
	ser.Value("m_SilenceTime", m_SilenceTime);
	ser.Value("m_fMultiplier", m_fMultiplier);

	if (ser.IsWriting())
	{
		fAI = m_pMusicState->GetInputAsFloat(m_AI_Intensity_ID);
		fPlayer = m_pMusicState->GetInputAsFloat(m_Player_Intensity_ID);
		fGame = m_pMusicState->GetInputAsFloat(m_Game_Intensity_ID);
		fChange = m_pMusicState->GetInputAsFloat(m_Allow_Change_ID);

		ser.Value("fAI", fAI);
		ser.Value("fPlayer",fPlayer);
		ser.Value("fGame", fGame);
		ser.Value("fChange", fChange);
	}

	if (ser.IsReading())
	{
		ser.Value("fAI", fAI);
		ser.Value("fPlayer",fPlayer);
		ser.Value("fGame", fGame);
		ser.Value("fChange", fChange);

		m_pMusicState->SetInput(m_AI_Intensity_ID, fAI);
		m_pMusicState->SetInput(m_Player_Intensity_ID, fPlayer);
		m_pMusicState->SetInput(m_Game_Intensity_ID, fGame);
		m_pMusicState->SetInput(m_Allow_Change_ID, fChange);

		if (m_bActive)
			Start();
		else
			Stop();

	}
}

