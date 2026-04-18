////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   FlowSoundVolumeNodes.cpp
//  Version:     v1.00
//  Created:     Nov 22nd 2006 by AlexL
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <IEntityProxy.h>
#include <ISound.h>
#include "FlowBaseNode.h"

template<class Impl>
class CFlowNode_SoundVolume : public CFlowBaseNode, public Impl
{

public:
	CFlowNode_SoundVolume( SActivationInfo * pActInfo ) 
		: m_sourceValue(0.0f), m_targetValue(0.0f)
	{
	}

	~CFlowNode_SoundVolume()
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowNode_SoundVolume<Impl> (pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("m_startTime", m_startTime);
		ser.Value("m_endTime", m_endTime);
		ser.Value("m_sourceValue", m_sourceValue);
		ser.Value("m_targetValue", m_targetValue);
		ser.EndGroup();
	}

	enum INPUTS 
	{
		eIN_StartFade = 0,
		eIN_StopFade,
		eIN_FadeTime,
		eIN_SourceValue,
		eIN_TargetValue,
	};

	enum OUTPUTS 
	{
		eOUT_Done = 0,
		eOUT_Value
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = 
		{
			InputPortConfig_Void    ("StartFade", _HELP("Start fading from Source to Target value.")),
			InputPortConfig_Void    ("StopFade",  _HELP("Stop fading.")),
			InputPortConfig<float>  ("FadeTime", _HELP("Fade time. Can be 0 for instant setting.")),
			InputPortConfig<float>  ("SourceValue", -1.0f,  _HELP("Source Value. If -1, uses current.")),
			InputPortConfig<float>  ("TargetValue",  0.0f,  _HELP("Target Value. Default=0 -> FadeOut")),
			{0}
		};
		static const SOutputPortConfig outputs[] = 
		{
			OutputPortConfig_Void   ("Done", _HELP("Triggered when Fade is over.")),
			OutputPortConfig<float> ("Value", _HELP("Current value")),
			{0}
		};    
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = Impl::sDescription;
		config.SetCategory(EFLN_APPROVED);
	}


	void Interpol(SActivationInfo* pActInfo, const float fPosition)
	{
		float value = Lerp(m_sourceValue, m_targetValue, fPosition);
		Impl::SetValue(value);
		ActivateOutput(pActInfo, eOUT_Value, value);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, eIN_StopFade))
			{
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				ActivateOutput(pActInfo, eOUT_Done, true);
			}
			if (IsPortActive(pActInfo, eIN_StartFade))
			{
				m_startTime = gEnv->pTimer->GetFrameStartTime();
				m_endTime = m_startTime + GetPortFloat(pActInfo, eIN_FadeTime);
				const float currentValue = Impl::GetValue();
				m_sourceValue = GetPortFloat(pActInfo, eIN_SourceValue);
				if (m_sourceValue <= -1.0f)
					m_sourceValue = currentValue;
				m_targetValue = GetPortFloat(pActInfo, eIN_TargetValue);
				if (m_targetValue <= -1.0f)
					m_targetValue = currentValue;

				// are we doing a fade?
				if (m_endTime > m_startTime && m_sourceValue != m_targetValue)
				{
					Interpol(pActInfo, 0.0f);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				}
				// no real fade, set target value immediately
				else
				{
					Interpol(pActInfo, 1.0f);
				}
			}
			break;
		case eFE_Update:
			{
				const CTimeValue fTime = gEnv->pTimer->GetFrameStartTime();
				const float fDuration = (m_endTime - m_startTime).GetSeconds();
				float fPosition;
				if (fDuration <= 0.0f)
					fPosition = 1.0f;
				else
				{
					fPosition = (fTime - m_startTime).GetSeconds() / fDuration;
					fPosition = CLAMP(fPosition, 0.0f, 1.0f);
				}
				Interpol(pActInfo, fPosition);
				if (fTime >= m_endTime)
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					ActivateOutput(pActInfo, eOUT_Done, true);
				}
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

protected:
	CTimeValue m_startTime;
	CTimeValue m_endTime;
	float m_sourceValue;
	float m_targetValue;
};

struct SFXVolume
{
	static const char* sDescription;
	float GetValue()
	{
		// FIXME: for now use the cvar, until SoundSystem supports it
		ICVar* pCVar = gEnv->pConsole->GetCVar("s_GameSFXVolume");
		if (pCVar)
			return pCVar->GetFVal();
		return 0.0f;
	}
	void SetValue(const float value)
	{
		// FIXME: for now use the cvar, until SoundSystem supports it
		ICVar* pCVar = gEnv->pConsole->GetCVar("s_GameSFXVolume");
		if (pCVar)
			pCVar->Set(value);
	}
};
const char* SFXVolume::sDescription = _HELP("Fade Game SFX Volume");

struct MusicVolume
{
	static const char* sDescription;
	float GetValue()
	{
		// FIXME: for now use the cvar, until SoundSystem supports it
		ICVar* pCVar = gEnv->pConsole->GetCVar("s_GameMusicVolume");
		if (pCVar)
			return pCVar->GetFVal();
		return 0.0f;
	}
	void SetValue(const float value)
	{
		// FIXME: for now use the cvar, until SoundSystem supports it
		ICVar* pCVar = gEnv->pConsole->GetCVar("s_GameMusicVolume");
		if (pCVar)
			pCVar->Set(value);
	}
};
const char* MusicVolume::sDescription = _HELP("Fade Game Music Volume");

typedef CFlowNode_SoundVolume<SFXVolume>   SoundSFXVolumeNode;
typedef CFlowNode_SoundVolume<MusicVolume> SoundMusicVolumeNode;

REGISTER_FLOW_NODE("Sound:SFXVolume", SoundSFXVolumeNode)
REGISTER_FLOW_NODE("Sound:MusicVolume", SoundMusicVolumeNode)
