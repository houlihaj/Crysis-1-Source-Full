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
//#include "IGameFramework.h"
#include "IMusicSystem.h"
#include "../CryAction/IActorSystem.h"
#include <IRenderer.h>

#include "MusicLogic.h"
#include "ScriptBind_MusicLogic.h"
//#include "../SoundSystem.h"

//! This structure must 100% match EMusicLogicEvents enum.
static const char*	s_MusicEventNames[] = 
{
	"Set Multiplier",
	"Set AI Multiplier",
	"Set AI",
	"Change AI",
	"Set Player",
	"ChangeP layer",
	"Set Game",
	"Change Game",
	"Set AllowChange",
	"Change AllowChange",
	"Vehicle Enter",
	"Vehicle Leave",
	"Weapon Mount",
	"Weapon Unmount",
	"Sniper Mode Enter",
	"Sniper Mode Leave",
	"Cloak Mode Enter",
	"Cloak Mode Leave",
	"Enemy Spotted",
	"Enemy Killed",
	"Enemy Headshot",
	"Enemy Overrun",
	"Player Wounded",
	"Player Killed",
	"Player Spotted",
	"Player Attacked by Turret",
	"Player Swim Start",
	"Player Swim Stop",
	"Explosion",
	"Factory Captured",
	"Factory Lost",
	"Factory Recaptured",
	"Vehicle Created",

	// reserved.
	"",
};

//////////////////////////////////////////////////////////////////////////
// Initialization
//////////////////////////////////////////////////////////////////////////

CMusicLogic::CMusicLogic(IAnimationGraphState *pMusicState)
{
	m_pMusicState = pMusicState;
	m_pMusicSystem	= 0;

	m_bActive = false;
	m_fMultiplier = 1.0f;
	m_fAIMultiplier = 1.0f;
	m_bHitListener = false;
	m_tLastUpdate.SetMilliSeconds(0);

	m_AI_Intensity_ID = 0;
	m_Player_Intensity_ID = 0;
	m_Allow_Change_ID = 0;
	m_Game_Intensity_ID = 0;
	m_MusicTime_ID = 0;
	m_MoodTime_ID = 0;
	m_SilenceTime_ID = 0;
	m_nOldAlertness = 0;

	m_pScriptBindMusicLogic = new CScriptBind_MusicLogic(this);

}

CMusicLogic::~CMusicLogic(void)
{

	string sFullFileName = "/";
	sFullFileName += MUSICLOGIC_FILENAME;
	sFullFileName = PathUtil::GetGameFolder() + sFullFileName;

	XmlNodeRef root = GetISystem()->CreateXmlNode("Export");

	SerializeFile(root, false);

	root->saveToFile( sFullFileName.c_str() );


	if (m_bHitListener && gEnv->pGame->GetIGameFramework())
		if (gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem())
			if (gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules())
				gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules()->RemoveHitListener(this);

	SAFE_DELETE(m_pScriptBindMusicLogic);
}

bool CMusicLogic::Init()
{	

	Load(MUSICLOGIC_FILENAME);

	//m_pMusicState = gEnv->pGame->GetIGameFramework()->GetMusicGraphState();
	assert(m_pMusicState);

	if (!m_pMusicState)
		return false;

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

	return true;
}

bool CMusicLogic::Start()
{
	if (m_pMusicState)
	{
		m_pMusicState->SetInput("Mode", "running");
		m_bActive = true;
	}

	if (m_bActive)
		return true;	
	
	//m_CurrTime = gEnv->pTimer->GetFrameStartTime();

	if(!m_pMusicSystem || !m_pMusicState)
		return false;

	bool bSilence			= stricmp(m_pMusicSystem->GetMood(),"silence") == 0;
	bool bIncidental	= stricmp(m_pMusicSystem->GetMood(),"incidental") == 0;
	bool bAmbient			= stricmp(m_pMusicSystem->GetMood(),"ambient") == 0;
	bool bMiddle			= stricmp(m_pMusicSystem->GetMood(),"middle") == 0;
	bool bAction			= stricmp(m_pMusicSystem->GetMood(),"action") == 0;

	if (bSilence || bIncidental || bAmbient || bMiddle || bAction)
	{
		// push current mood
		if (bSilence)
			m_pMusicState->PushForcedState("theme_any+silence");

		if (bIncidental)
			m_pMusicState->PushForcedState("theme_any+incidental");

		if (bAmbient)
			m_pMusicState->PushForcedState("theme_any+ambient");

		if (bMiddle)
			m_pMusicState->PushForcedState("theme_any+middle");

		if (bAction)
			m_pMusicState->PushForcedState("theme_any+action");
	}
	else
	{
		// push the default
		m_pMusicState->PushForcedState("theme_any+incidental");
	}

	m_pMusicState->ForceTeleportToQueriedState();

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
	float fMS = tTimeDif.GetMilliSeconds();

	if (abs(fMS) < UPDATE_MUSICLOGIC_IN_MS)
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

	float fTimeDiffInSec = tTimeDif.GetSeconds();

	if (m_pMusicState)
	{
		float fAIIntensity = 0.0f;

		// update AI_Intensity
		IActor *pClientPlayer = gEnv->pGame->GetIGameFramework()->GetClientActor();
		if (pClientPlayer)
		{
			Vec3 vPlayerPos = pClientPlayer->GetEntity()->GetWorldPos();

			if (gEnv->pAISystem)
			{
				// global alertness
				int nNewAlertness = gEnv->pAISystem->GetAlertness();

				if (nNewAlertness > m_nOldAlertness)
					SetEvent(eMUSICLOGICEVENT_PLAYER_SPOTTED);

				m_nOldAlertness = nNewAlertness;
				
				//individual alertness
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
			}

			float fOldValue = m_pMusicState->GetInputAsFloat(m_AI_Intensity_ID);
			fOldValue = max(0.0f, fOldValue - fOldValue*0.07f*fTimeDiffInSec - 0.1f);	// leak a bit of the old value
			fAIIntensity = max(fAIIntensity*m_fMultiplier*m_fAIMultiplier, fOldValue);							// take whatever is the higher value
			m_pMusicState->SetInput(m_AI_Intensity_ID, fAIIntensity);
		}

		// update Player_Intensity
		float fPlayer = m_pMusicState->GetInputAsFloat(m_Player_Intensity_ID);
		fPlayer = max(0.0f, fPlayer - fPlayer*0.07f*fTimeDiffInSec - 0.1f);	// leak a bit of the old value
		m_pMusicState->SetInput(m_Player_Intensity_ID, fPlayer);

		// update Allow_Change
		float fChange = m_pMusicState->GetInputAsFloat(m_Allow_Change_ID);
		fChange = max(0.0f, fChange - fChange*0.07f*fTimeDiffInSec - 0.1f);	// leak a bit of the old value
		m_pMusicState->SetInput(m_Allow_Change_ID, fChange);

		// update Game_Intensity ( AI_Intensity OR Player_Intensity)
		float fGame = m_pMusicState->GetInputAsFloat(m_Game_Intensity_ID);
		fGame = max(0.0f, fGame - fGame*0.07f*fTimeDiffInSec - 0.1f);		// leak a bit of the old value
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

	if (!m_EventHistory.empty())
	{
		tEventHistory::reverse_iterator rItEnd = m_EventHistory.rend();
		for (tEventHistory::reverse_iterator rIt = m_EventHistory.rbegin(); rIt!=rItEnd; ++rIt)
		{
			CTimeValue tDiff = gEnv->pTimer->GetFrameStartTime() - (*rIt).Time;
			float fSec = tDiff.GetSeconds();

			if (abs(fSec) > 5.0f)
			{
				// remove it: copy last element and pop back
				if (rIt != m_EventHistory.rbegin())
				{
					(*rIt) = *(m_EventHistory.rbegin());
				}
				m_EventHistory.pop_back();
			}

		}
	}
}

// incoming events
void CMusicLogic::SetEvent(EMusicLogicEvents MusicEvent, const float fValue)
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

	EventTime NewEventTime;
	NewEventTime.Event = MusicEvent;
	NewEventTime.Time = gEnv->pTimer->GetFrameStartTime();
	NewEventTime.Value = FLT_MAX;

	switch(MusicEvent)
	{
	case eMUSICLOGICEVENT_SET_MULTIPLIER:
		m_fMultiplier = fValue;
		NewEventTime.Value = fValue;
		break;
	case eMUSICLOGICEVENT_SET_AI_MULTIPLIER:
		m_fAIMultiplier = fValue;
		NewEventTime.Value = fValue;
		break;
	case eMUSICLOGICEVENT_SET_AI:
		bSetAIIntensity			= true;
		bChangeAIIntensity	= true;
		fAIIntensity				= fValue;
		NewEventTime.Value	= fValue;
		break;
	case eMUSICLOGICEVENT_CHANGE_AI:
		bChangeAIIntensity	= true;
		fAIIntensity				= fValue;
		NewEventTime.Value	= fValue;
		break;
	case eMUSICLOGICEVENT_SET_PLAYER:
		bSetPlayerIntensity			= true;
		bChangePlayerIntensity	= true;
		fPlayerIntensity				= fValue;
		NewEventTime.Value			= fValue;
		break;
	case eMUSICLOGICEVENT_CHANGE_PLAYER:
		bChangePlayerIntensity	= true;
		fPlayerIntensity				= fValue;
		NewEventTime.Value			= fValue;
		break;
	case eMUSICLOGICEVENT_SET_GAME:
		bSetGameIntensity			= true;
		bChangeGameIntensity	= true;
		fGameIntensity				= fValue;
		NewEventTime.Value		= fValue;
		break;
	case eMUSICLOGICEVENT_CHANGE_GAME:
		bChangeGameIntensity	= true;
		fGameIntensity				= fValue;
		NewEventTime.Value		= fValue;
		break;
	case eMUSICLOGICEVENT_SET_ALLOWCHANGE:
		bSetAllowChange			= true;
		bChangeAllowChange	= true;
		fAllowChange				= fValue;
		NewEventTime.Value	= fValue;
		break;
	case eMUSICLOGICEVENT_CHANGE_ALLOWCHANGE:
		bChangeAllowChange	= true;
		fAllowChange				= fValue;
		NewEventTime.Value	= fValue;
		break;
	default:
		{
			int nNum = m_EventIndex.size();

			assert (MusicEvent < m_EventIndex.size());

			if (MusicEvent < m_EventIndex.size())
			{
				assert (m_EventIndex[MusicEvent] < m_Configuration.size());
				MusicLogicEvent *pEvent = &(m_Configuration[m_EventIndex[MusicEvent]]);


				bSetAIIntensity = pEvent->fSetAIIntensity != 0.0f;
				bSetPlayerIntensity = pEvent->fSetPlayerIntensity != 0.0f;
				bSetGameIntensity = pEvent->fSetGameIntensity != 0.0f;
				bSetAllowChange = pEvent->fSetAllowChangeIntensity != 0.0f;

				bChangeAIIntensity = (pEvent->fChangeAIIntensity != 0.0f || bSetAIIntensity);
				bChangePlayerIntensity = (pEvent->fChangePlayerIntensity != 0.0f || bSetPlayerIntensity);
				bChangeGameIntensity = (pEvent->fChangeGameIntensity != 0.0f || bSetGameIntensity);
				bChangeAllowChange = (pEvent->fChangeAllowChangeIntensity != 0.0f || bSetAllowChange);

				fAIIntensity = bSetAIIntensity?pEvent->fSetAIIntensity:pEvent->fChangeAIIntensity;
				fPlayerIntensity = bSetPlayerIntensity?pEvent->fSetPlayerIntensity:pEvent->fChangePlayerIntensity;
				fGameIntensity = bSetGameIntensity?pEvent->fSetGameIntensity:pEvent->fChangeGameIntensity;
				fAllowChange = bSetAllowChange?pEvent->fSetAllowChangeIntensity:pEvent->fChangeAllowChangeIntensity;
			}
			break;
		}

	}

	m_EventHistory.push_back(NewEventTime);

	if (bChangeAIIntensity)
	{
		float fAI = bSetAIIntensity?(fAIIntensity*m_fMultiplier*m_fAIMultiplier):(m_pMusicState->GetInputAsFloat(m_AI_Intensity_ID) + fAIIntensity*m_fMultiplier*m_fAIMultiplier);
		m_pMusicState->SetInput(m_AI_Intensity_ID, fAI);
	}

	if (bChangePlayerIntensity)
	{
		float fPlayer = bSetPlayerIntensity?(fPlayerIntensity*m_fMultiplier):(m_pMusicState->GetInputAsFloat(m_Player_Intensity_ID) + fPlayerIntensity*m_fMultiplier);
		m_pMusicState->SetInput(m_Player_Intensity_ID, fPlayer);
	}

	if (bChangeGameIntensity)
	{
		float fGame = bSetGameIntensity?(fGameIntensity*m_fMultiplier):(m_pMusicState->GetInputAsFloat(m_Game_Intensity_ID) + fGameIntensity*m_fMultiplier);
		m_pMusicState->SetInput(m_Game_Intensity_ID, fGame);
	}

	if (bChangeAllowChange)
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
			if (fDistance < 10000)
			{
				SetEvent(eMUSICLOGICEVENT_EXPLOSION);
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

	if (m_pScriptBindMusicLogic)
		m_pScriptBindMusicLogic->GetMemoryStatistics(s);

	
	s->AddContainer(m_listExplosions);
}

// writes output to screen in debug
void CMusicLogic::DrawInformation(IRenderer* pRenderer, float xpos, float ypos, int nSoundInfo)
{
	if (nSoundInfo && pRenderer)
	{
		// renders default stuff in the top left corner
		float fColor[4]				={1.0f, 1.0f, 1.0f, 0.7f};
		float fColorRed[4]		={1.0f, 0.0f, 0.0f, 0.7f};
		float fColorYellow[4]	={1.0f, 1.0f, 0.0f, 0.7f};
		float fColorGreen[4]	={0.0f, 1.0f, 0.0f, 0.7f};
		float fColorOrange[4]	={1.0f, 0.5f, 0.0f, 0.7f};
		float fColorBlue[4]		={0.4f, 0.4f, 7.0f, 0.7f};

		pRenderer->Draw2dLabel(xpos, ypos, 2, m_bActive?fColorGreen:fColorRed, false, "MusicLogic is %s", m_bActive?"Active":"Inactive");

		
		tEventHistory::reverse_iterator rItEnd = m_EventHistory.rend();
		for (tEventHistory::reverse_iterator rIt = m_EventHistory.rbegin(); rIt!=rItEnd; ++rIt)
		{
			CTimeValue tDiff = gEnv->pTimer->GetFrameStartTime() - (*rIt).Time;
			float fSec = 5.0f - tDiff.GetSeconds();
			fColorOrange[3] = clamp(fSec, 0.0f, 1.0f);
			
			if ((*rIt).Value != FLT_MAX)
				pRenderer->Draw2dLabel(xpos+400, ypos, 1.35f, fColorOrange, false, "MusicEvent: %s %.1f", s_MusicEventNames[(*rIt).Event], (*rIt).Value);
			else
				pRenderer->Draw2dLabel(xpos+400, ypos, 1.35f, fColorOrange, false, "MusicEvent: %s", s_MusicEventNames[(*rIt).Event]);

			ypos += 12;
		}
	}
}

bool CMusicLogic::Load(const char* sFilename)
{
	bool bResult = false;

	string sFullFileName = "/";
	sFullFileName += sFilename;
	sFullFileName = PathUtil::GetGameFolder() + sFullFileName;

	XmlNodeRef root = gEnv->pSystem->LoadXmlFile( sFullFileName.c_str());
	SerializeFile(root, true);

	return true;
}

bool CMusicLogic::SerializeFile(XmlNodeRef &node, bool bLoading)
{
	bool bResult = true;

	if (bLoading)
	{
		m_Configuration.clear();
		m_EventIndex.clear();

		//// couldn't load or find file
		if (!node)
			return false; 

		//// quick test for correct tag
		if (strcmp("MusicLogic", node->getTag()) != 0)
			return false;

		//// go through Configuration and parse them
		XmlNodeRef ConfigurationNode = node->getChild(0);

		if (ConfigurationNode)
		{
			// go through Events and parse them
			int nGroupChildCount = ConfigurationNode->getChildCount();

			m_Configuration.reserve(nGroupChildCount);
			m_EventIndex.resize(eMUSICLOGICEVENT_MAX);
			
			for (int i=0 ; i < nGroupChildCount; ++i)
			{
				m_EventIndex[i] = 0;
			}

			for (int i=0 ; i < nGroupChildCount; ++i)
			{
				XmlNodeRef EventNode = ConfigurationNode->getChild(i);

				// read the name of the Event
				const char *sEventName = EventNode->getAttr("Name");

				MusicLogicEvent NewEvent;
				NewEvent.sEventName = sEventName;

				if (!EventNode->getAttr("SetAI", NewEvent.fSetAIIntensity))
					NewEvent.fSetAIIntensity = 0.0f;
				if (!EventNode->getAttr("ChangeAI", NewEvent.fChangeAIIntensity))
					NewEvent.fChangeAIIntensity = 0.0f;

				if (!EventNode->getAttr("SetPlayer", NewEvent.fSetPlayerIntensity))
					NewEvent.fSetPlayerIntensity = 0.0f;
				if (!EventNode->getAttr("ChangePlayer", NewEvent.fChangePlayerIntensity))
					NewEvent.fChangePlayerIntensity = 0.0f;

				if (!EventNode->getAttr("SetGame", NewEvent.fSetGameIntensity))
					NewEvent.fSetGameIntensity = 0.0f;
				if (!EventNode->getAttr("ChangeGame", NewEvent.fChangeGameIntensity))
					NewEvent.fChangeGameIntensity = 0.0f;

				if (!EventNode->getAttr("SetAllowChange", NewEvent.fSetAllowChangeIntensity))
					NewEvent.fSetAllowChangeIntensity = 0.0f;
				if (!EventNode->getAttr("ChangeAllowChange", NewEvent.fChangeAllowChangeIntensity))
					NewEvent.fChangeAllowChangeIntensity = 0.0f;

				m_Configuration.push_back(NewEvent);

		//		//	eMUSICLOGICEVENT_SET_MULTIPLIER = 0,
		//		//	eMUSICLOGICEVENT_SET_AI,
		//		//	eMUSICLOGICEVENT_CHANGE_AI,
		//		//	eMUSICLOGICEVENT_SET_PLAYER,
		//		//	eMUSICLOGICEVENT_CHANGE_PLAYER,
		//		//	eMUSICLOGICEVENT_SET_GAME,
		//		//	eMUSICLOGICEVENT_CHANGE_GAME,
		//		//	eMUSICLOGICEVENT_SET_ALLOWCHANGE,
		//		//	eMUSICLOGICEVENT_CHANGE_ALLOWCHANGE,
		//		//	eMUSICLOGICEVENT_ENTER_VEHICLE,
		//		//	eMUSICLOGICEVENT_LEAVE_VEHICLE,
		//		//	eMUSICLOGICEVENT_MOUNT_WEAPON,
		//		//	eMUSICLOGICEVENT_UNMOUNT_WEAPON,
		//		//	eMUSICLOGICEVENT_ENEMY_SPOTTED,
		//		//	eMUSICLOGICEVENT_ENEMY_KILLED,
		//		//	eMUSICLOGICEVENT_ENEMY_HEADSHOT,
		//		//	eMUSICLOGICEVENT_ENEMY_OVERRUN,
		//		//	eMUSICLOGICEVENT_PLAYER_WOUNDED,
		//		//	eMUSICLOGICEVENT_EXPLOSION,
		//		//	eMUSICLOGICEVENT_MAX

				if (NewEvent.sEventName == "SetMultiplier")
					m_EventIndex[eMUSICLOGICEVENT_SET_MULTIPLIER] = i;
				if (NewEvent.sEventName == "SetAIMultiplier")
					m_EventIndex[eMUSICLOGICEVENT_SET_AI_MULTIPLIER] = i;
				if (NewEvent.sEventName == "SetAI")
					m_EventIndex[eMUSICLOGICEVENT_SET_AI] = i;
				if (NewEvent.sEventName == "ChangeAI")
					m_EventIndex[eMUSICLOGICEVENT_CHANGE_AI] = i;
				if (NewEvent.sEventName == "SetPlayer")
					m_EventIndex[eMUSICLOGICEVENT_SET_PLAYER] = i;
				if (NewEvent.sEventName == "ChangePlayer")
					m_EventIndex[eMUSICLOGICEVENT_CHANGE_PLAYER] = i;
				if (NewEvent.sEventName == "SetGame")
					m_EventIndex[eMUSICLOGICEVENT_SET_GAME] = i;
				if (NewEvent.sEventName == "ChangeGame")
					m_EventIndex[eMUSICLOGICEVENT_CHANGE_GAME] = i;
				if (NewEvent.sEventName == "SetAllowChange")
					m_EventIndex[eMUSICLOGICEVENT_SET_ALLOWCHANGE] = i;
				if (NewEvent.sEventName == "ChangeAllowChange")
					m_EventIndex[eMUSICLOGICEVENT_CHANGE_ALLOWCHANGE] = i;

				if (NewEvent.sEventName == "VehicleEnter")
					m_EventIndex[eMUSICLOGICEVENT_VEHICLE_ENTER] = i;
				if (NewEvent.sEventName == "VehicleLeave")
					m_EventIndex[eMUSICLOGICEVENT_VEHICLE_LEAVE] = i;
				if (NewEvent.sEventName == "WeaponMount")
					m_EventIndex[eMUSICLOGICEVENT_WEAPON_MOUNT] = i;
				if (NewEvent.sEventName == "WeaponUnmount")
					m_EventIndex[eMUSICLOGICEVENT_WEAPON_UNMOUNT] = i;			
				
				if (NewEvent.sEventName == "SniperModeEnter")
					m_EventIndex[eMUSICLOGICEVENT_SNIPERMODE_ENTER] = i;			
				if (NewEvent.sEventName == "SniperModeLeave")
					m_EventIndex[eMUSICLOGICEVENT_SNIPERMODE_LEAVE] = i;		
				if (NewEvent.sEventName == "CloakModeEnter")
					m_EventIndex[eMUSICLOGICEVENT_CLOAKMODE_ENTER] = i;		
				if (NewEvent.sEventName == "CloakModeLeave")
					m_EventIndex[eMUSICLOGICEVENT_CLOAKMODE_LEAVE] = i;		

				if (NewEvent.sEventName == "EnemySpotted")
					m_EventIndex[eMUSICLOGICEVENT_ENEMY_SPOTTED] = i;
				if (NewEvent.sEventName == "EnemyKilled")
					m_EventIndex[eMUSICLOGICEVENT_ENEMY_KILLED] = i;
				if (NewEvent.sEventName == "EnemyHeadshot")
					m_EventIndex[eMUSICLOGICEVENT_ENEMY_HEADSHOT] = i;
				if (NewEvent.sEventName == "EnemyOverrun")
					m_EventIndex[eMUSICLOGICEVENT_ENEMY_OVERRUN] = i;

				if (NewEvent.sEventName == "PlayerWounded")
					m_EventIndex[eMUSICLOGICEVENT_PLAYER_WOUNDED] = i;
				if (NewEvent.sEventName == "PlayerKilled")
					m_EventIndex[eMUSICLOGICEVENT_PLAYER_KILLED] = i;
				if (NewEvent.sEventName == "PlayerSpotted")
					m_EventIndex[eMUSICLOGICEVENT_PLAYER_SPOTTED] = i;
				if (NewEvent.sEventName == "PlayerAttackedByTurret")
					m_EventIndex[eMUSICLOGICEVENT_PLAYER_TURRET_ATTACK] = i;
				if (NewEvent.sEventName == "PlayerSwimEnter")
					m_EventIndex[eMUSICLOGICEVENT_PLAYER_SWIM_ENTER] = i;
				if (NewEvent.sEventName == "PlayerSwimLeave")
					m_EventIndex[eMUSICLOGICEVENT_PLAYER_SWIM_LEAVE] = i;

				if (NewEvent.sEventName == "Explosion")
					m_EventIndex[eMUSICLOGICEVENT_EXPLOSION] = i;

				if (NewEvent.sEventName == "FactoryCaptured")
					m_EventIndex[eMUSICLOGICEVENT_FACTORY_CAPTURED] = i;
				if (NewEvent.sEventName == "FactoryLost")
					m_EventIndex[eMUSICLOGICEVENT_FACTORY_LOST] = i;
				if (NewEvent.sEventName == "FactoryRecaptured")
					m_EventIndex[eMUSICLOGICEVENT_FACTORY_RECAPTURED] = i;
				if (NewEvent.sEventName == "VehicleCreated")
					m_EventIndex[eMUSICLOGICEVENT_VEHICLE_CREATED] = i;


			} // done with all MusicELogicvents
		}

	}
	else // saving
	{
		node->setTag("MusicLogic");
		XmlNodeRef ConfigurationNode = node->newChild("Configuration");

		tLogicConfiguration::iterator ItEnd = m_Configuration.end();
		for (tLogicConfiguration::iterator It = m_Configuration.begin(); It != ItEnd; ++It)
		{
			XmlNodeRef EventNode = ConfigurationNode->newChild("MusicLogicEvent");

			MusicLogicEvent* pEvent = &(*It);

			EventNode->setAttr("Name", pEvent->sEventName.c_str());
			
			if (pEvent->fSetAIIntensity != 0.0f)
				EventNode->setAttr("SetAI", pEvent->fSetAIIntensity);
			if (pEvent->fChangeAIIntensity != 0.0f)
				EventNode->setAttr("ChangeAI", pEvent->fChangeAIIntensity);

			if (pEvent->fSetPlayerIntensity != 0.0f)
				EventNode->setAttr("SetPlayer", pEvent->fSetPlayerIntensity);
			if (pEvent->fChangePlayerIntensity != 0.0f)
				EventNode->setAttr("ChangePlayer", pEvent->fChangePlayerIntensity);

			if (pEvent->fSetGameIntensity != 0.0f)
				EventNode->setAttr("SetGame", pEvent->fSetGameIntensity);
			if (pEvent->fChangeGameIntensity != 0.0f)
				EventNode->setAttr("ChangeGame", pEvent->fChangeGameIntensity);

			if (pEvent->fSetAllowChangeIntensity != 0.0f)
				EventNode->setAttr("SetAllowChange", pEvent->fSetAllowChangeIntensity);
			if (pEvent->fChangeAllowChangeIntensity != 0.0f)
				EventNode->setAttr("ChangeAllowChange", pEvent->fChangeAllowChangeIntensity);

		}

	}

	return bResult;

}

void CMusicLogic::Serialize(TSerialize ser)
{

	float fAI = 0.0f;
	float fPlayer = 0.0f;
	float fGame = 0.0f;
	float fChange = 0.0f;

	ser.Value("m_bActive", m_bActive);
	ser.Value("m_MusicTime", m_MusicTime);
	ser.Value("m_SilenceTime", m_SilenceTime);
	ser.Value("m_fMultiplier", m_fMultiplier);
	ser.Value("m_fAIMultiplier", m_fAIMultiplier);
	ser.Value("m_nOldAlertness", m_nOldAlertness);

	if(m_pMusicState)
	{
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
}

