////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MusicLogic.h
//  Version:     v1.00
//  Created:     24/08/2006 by Tomas.
//  Compilers:   Visual Studio.NET
//  Description: decleration of the MusicLogic class.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MUSICLOGIC_H__
#define __MUSICLOGIC_H__

#include "IMusicSystem.h"
#include <ITimer.h>
#include "../CryAction/IGameRulesSystem.h"
#include "../CryAction/IAnimationGraph.h"

#include <list>

#pragma once

struct IAnimationGraphState;
class  CScriptBind_MusicLogic;

struct MusicLogicEvent
{
	CryFixedStringT<256> sEventName;
	float fSetAIIntensity;
	float fChangeAIIntensity;
	float fSetPlayerIntensity;
	float fChangePlayerIntensity;
	float fSetGameIntensity;
	float fChangeGameIntensity;
	float fSetAllowChangeIntensity;
	float fChangeAllowChangeIntensity;
};

typedef std::vector<MusicLogicEvent>	tLogicConfiguration;
typedef std::vector<int>							tEventIndex;

struct EventTime
{
	EMusicLogicEvents Event;
	CTimeValue				Time;
	float							Value;
};

typedef std::vector<EventTime> tEventHistory;

class CMusicLogic : public IMusicLogic, public IHitListener
{
public:

	CMusicLogic(IAnimationGraphState *pMusicState);
	~CMusicLogic(void);

	//////////////////////////////////////////////////////////////////////////
	// Initialization
	//////////////////////////////////////////////////////////////////////////

	virtual bool Init();

	virtual bool Start();
	virtual bool Stop();

	virtual void Update();

	// incoming events
	virtual void SetEvent(EMusicLogicEvents MusicEvent, const float fValue=0.0f);
	//void SetMusicEvent(EMusicEvents MusicEvent, const char *sText);

	virtual void GetMemoryStatistics( ICrySizer * );
	virtual void Serialize(TSerialize ser);

	// writes output to screen in debug
	virtual void DrawInformation(IRenderer* pRenderer, float xpos, float ypos, int nSoundInfo);

	//IHitListener
	virtual void OnHit(const HitInfo&);
	virtual void OnExplosion(const ExplosionInfo&);
	virtual void OnServerExplosion(const ExplosionInfo&);

private:

	bool			Load(const char* sFilename);
	bool			SerializeFile(XmlNodeRef &node, bool bLoading);

	IAnimationGraphState *m_pMusicState;

	tLogicConfiguration m_Configuration;
	tEventIndex					m_EventIndex;

	IMusicSystem		*m_pMusicSystem;

	float						m_fMultiplier;
	float						m_fAIMultiplier;

	CTimeValue			m_MusicTime;
	CTimeValue			m_SilenceTime;
	CTimeValue			m_tLastUpdate;

	int							m_nOldAlertness;

	IAnimationGraph::InputID m_AI_Intensity_ID;
	IAnimationGraph::InputID m_Player_Intensity_ID;
	IAnimationGraph::InputID m_Allow_Change_ID;
	IAnimationGraph::InputID m_Game_Intensity_ID;
	IAnimationGraph::InputID m_MusicTime_ID;
	IAnimationGraph::InputID m_MoodTime_ID;
	IAnimationGraph::InputID m_SilenceTime_ID;

	typedef std::list<CTimeValue> tExplosionList;
	tExplosionList m_listExplosions;

	bool						m_bActive;
	bool						m_bHitListener;

	tEventHistory		m_EventHistory;

	CScriptBind_MusicLogic* m_pScriptBindMusicLogic;

};
#endif