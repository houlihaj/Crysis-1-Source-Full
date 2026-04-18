////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowSoundMoodNode.cpp
//  Version:     v1.00
//  Created:     Oct 24th 2005 by TomasN
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <IEntityProxy.h>
#include <ISound.h>
#include "ISoundMoodManager.h"
#include "FlowBaseNode.h"



class CFlowNode_SoundMood : public CFlowBaseNode
{

public:
	CFlowNode_SoundMood( SActivationInfo * pActInfo ) 
	{
		m_fLatestTargetValue = 0.0f;
		m_bEnable = true;
		m_bFadingIn = false;
		m_bFadingOut = false;
	}


	~CFlowNode_SoundMood()
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowNode_SoundMood(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_bEnable", m_bEnable);
		ser.Value("m_sMoodName", m_sMoodName);
		ser.Value("m_bFadingIn", m_bFadingIn);
		ser.Value("m_bFadingOut", m_bFadingOut);
		ser.Value("m_fLatestTargetValue", m_fLatestTargetValue);

		if (ser.IsReading())
		{
			if (m_bEnable)
			{
				ISoundMoodManager *pMoodManager = NULL;

				if (gEnv->pSoundSystem)
					pMoodManager = gEnv->pSoundSystem->GetIMoodManager();

				if (pMoodManager)
				{
					pMoodManager->RegisterSoundMood(m_sMoodName);
					pMoodManager->UpdateSoundMood(m_sMoodName, m_fLatestTargetValue, 0);
				}

			}
		}
	}

	enum INPUTS 
	{
		//eIN_FADE = 0,
		eIN_ENABLE = 0,
		eIN_DISABLE,
		eIN_NAME,
		// In
		eIN_FADEINTIME,
		eIN_FADEINVALUE,
		eIN_TRIGGERFADEIN,
		// Out
		eIN_FADEOUTTIME,
		eIN_TRIGGERFADEOUT,

	};

	enum OUTPUTS 
	{
		eOUT_DONE
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = 
		{
			InputPortConfig_Void		("Enable", _HELP("Enables functionality")),
			InputPortConfig_Void		("Disable", _HELP("Disables functionality")),
			InputPortConfig<string> ("mood_SoundMood", _HELP("Name of SoundMood to apply")),
			InputPortConfig<float>  ("FadeInTime", _HELP("Time to fade-in in Seconds")),
			InputPortConfig<float>  ("FadeInValue", _HELP("FadeValue in Seconds")),
			InputPortConfig_Void		("TriggerFadeIn", _HELP("Start to fade-in")),
			InputPortConfig<float>  ("FadeOutTime", _HELP("Time to fade-out in Seconds")),
			InputPortConfig_Void		("TriggerFadeOut", _HELP("Start to fade-out")),
			{0}
		};
		static const SOutputPortConfig outputs[] = 
		{
			// OutputPortConfig_AnyType ("Done", _HELP("Triggered if Sound has stopped playing")),
			{0}
		};    
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Apply a sound mood");
		config.SetCategory(EFLN_ADVANCED);
	}

	

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_bFadingIn = false;
				m_bFadingOut = false;
				break;
			}
		case eFE_Activate:
			{
				bool bUpdate = false;
				float fFadeInValue = 0;
				float fFadeInTime = 0;
				float fFadeOutTime = 0;

				ISoundMoodManager *pMoodManager = NULL;
				
				if (gEnv->pSoundSystem)
					pMoodManager = gEnv->pSoundSystem->GetIMoodManager();

				// Disable
				if (IsPortActive(pActInfo, eIN_DISABLE))
				{
					if (pMoodManager && !m_sMoodName.empty())
						pMoodManager->UnregisterSoundMood(m_sMoodName.c_str());

					m_fLatestTargetValue = 0.0f;
					m_bEnable = false;
					m_bFadingIn = false;
					m_bFadingOut = false;
				}

				// Enable
				if (IsPortActive(pActInfo, eIN_ENABLE))
					m_bEnable = true;

				// Name
				if (m_bEnable && IsPortActive(pActInfo, eIN_NAME) || m_sMoodName.empty())
				{
					if (m_sMoodName != GetPortString(pActInfo, eIN_NAME))
					{
						if (pMoodManager && !m_sMoodName.empty())
							pMoodManager->UnregisterSoundMood(m_sMoodName.c_str());
					}

					m_fLatestTargetValue = 0.0f;
					m_sMoodName = GetPortString(pActInfo, eIN_NAME);

				}

				// Trigger Fade In
				if (m_bEnable && IsPortActive(pActInfo, eIN_TRIGGERFADEIN))
				{
					fFadeInValue = GetPortFloat(pActInfo, eIN_FADEINVALUE);
					fFadeInTime = GetPortFloat(pActInfo, eIN_FADEINTIME)*1000.0f;

					if (pMoodManager && !m_sMoodName.empty())
					{

						pMoodManager->RegisterSoundMood(m_sMoodName.c_str());
						pMoodManager->UpdateSoundMood(m_sMoodName.c_str(), fFadeInValue, (uint32)fFadeInTime);
						m_fLatestTargetValue = fFadeInValue;
						m_bFadingIn = true;
						m_bFadingOut = false;
					}

				}

				// Trigger Fade Out
				if (m_bEnable && IsPortActive(pActInfo, eIN_TRIGGERFADEOUT))
				{
					fFadeOutTime = GetPortFloat(pActInfo, eIN_FADEOUTTIME)*1000.0f;;

					if (pMoodManager && !m_sMoodName.empty())
					{
						pMoodManager->UpdateSoundMood(m_sMoodName.c_str(), 0, (uint32)fFadeOutTime);
						m_bFadingIn = false;
						m_bFadingOut = true;
					}

					m_fLatestTargetValue = 0.0f;
				}
			}
		break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->Add(m_sMoodName);
	}

protected:
	bool m_bEnable;
	bool m_bFadingIn;
	bool m_bFadingOut;
	float m_fLatestTargetValue;
	string m_sMoodName;
};


REGISTER_FLOW_NODE("Sound:SoundMood", CFlowNode_SoundMood);
