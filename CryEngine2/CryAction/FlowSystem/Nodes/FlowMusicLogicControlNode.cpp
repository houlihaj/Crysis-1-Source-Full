////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowMusicLogicControlNode.cpp
//  Version:     v1.00
//  Created:     Dec 20th 2006 by TomasN
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <IEntityProxy.h>
#include <ISound.h>
#include "MusicLogic/MusicLogic.h"
//#include "Game.h"
#include "FlowBaseNode.h"


class CFlowNode_MusicLogicControl : public CFlowBaseNode
{

public:
	CFlowNode_MusicLogicControl( SActivationInfo * pActInfo ) 
	{
		m_bEnable = true;
		m_bRegistered = false;
	}


	~CFlowNode_MusicLogicControl()
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowNode_MusicLogicControl(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_bEnable", m_bEnable);
	}

	enum INPUTS 
	{
		eIN_ENABLE = 0,
		eIN_DISABLE,
		// In
		eIN_START,
		eIN_STOP,
		eIN_SET_ALL,
		eIN_ADD_ALL,
		eIN_SET_AI,
		eIN_ADD_AI,
		eIN_SET_PLAYER,
		eIN_ADD_PLAYER,
		eIN_SET_GAME,
		eIN_ADD_GAME,
		eIN_SET_CHANGE,
		eIN_ADD_CHANGE,
		eIN_SET_MULTIPLIER,
		eIN_SET_AI_MULTIPLIER,
		// Out

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

			InputPortConfig_Void		("Start", _HELP("Starts MusicLogic"), _HELP("Start MusicLogic")),
			InputPortConfig_Void		("Stop", _HELP("Stops MusicLogic"), _HELP("Stop MusicLogic")),
			
			InputPortConfig_Void		("SetAll", _HELP("Sets all input values"), _HELP("Set all values")),
			InputPortConfig_Void		("AddAll", _HELP("Adds all input values"), _HELP("Add all values")),

			InputPortConfig<float>  ("SetAI", _HELP("Sets AI-Intensity to input value"), _HELP("Set AI-Intensity")),
			InputPortConfig<float>  ("AddAI", _HELP("Adds input value to AI-Intensity"), _HELP("Add AI-Intensity")),
			
			InputPortConfig<float>  ("SetPlayer", _HELP("Sets Player-Intensity to input value"), _HELP("Set Player-Intensity")),
			InputPortConfig<float>  ("AddPlayer", _HELP("Adds input value to Player-Intensity"), _HELP("Add Player-Intensity")),
			
			InputPortConfig<float>  ("SetGame", _HELP("Sets Game-Intensity to input value"), _HELP("Set Game-Intensity")),
			InputPortConfig<float>  ("AddGame", _HELP("Adds input value to Game-Intensity"), _HELP("Add Game-Intensity")),

			InputPortConfig<float>  ("SetChange", _HELP("Sets Allow-Change to input value"), _HELP("Set Allow-Change")),
			InputPortConfig<float>  ("AddChange", _HELP("Adds input value to Allow-Change"), _HELP("Add Allow-Change")),

			InputPortConfig<float>  ("SetMultiplier", 1.0f, _HELP("Sets multiplier for global tweaking"), _HELP("Set Multiplier")),
			InputPortConfig<float>  ("SetAIMultiplier", 1.0f, _HELP("Sets multiplier for AI tweaking"), _HELP("Set AI Multiplier")),
			{0}
		};
		static const SOutputPortConfig outputs[] = 
		{
			// OutputPortConfig_AnyType ("Done", _HELP("Triggered if Sound has stopped playing")),
			{0}
		};    
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Control MusicLogic");
		config.SetCategory(EFLN_APPROVED);
	}

	

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{

		IMusicLogic *pMusicLogic = NULL;
		if (gEnv->pGame)
			if (gEnv->pGame->GetIGameFramework())
				if (gEnv->pGame->GetIGameFramework()->GetMusicLogic())
					pMusicLogic = gEnv->pGame->GetIGameFramework()->GetMusicLogic();

		if (!pMusicLogic)
			return;

		switch (event)
		{
		case eFE_Initialize:
			{
				m_bRegistered = false;
				break;
			}
		case eFE_Activate:
			{
				bool bUpdate = false;

				// Disable
				if (IsPortActive(pActInfo, eIN_DISABLE))
					m_bEnable = false;

				// Enable
				if (IsPortActive(pActInfo, eIN_ENABLE))
					m_bEnable = true;

				// Start Logic
				if (m_bEnable && IsPortActive(pActInfo, eIN_START))
					pMusicLogic->Start();
				
				// Stop Logic
				if (m_bEnable && IsPortActive(pActInfo, eIN_STOP))
					pMusicLogic->Stop();

				// Set All
				if (m_bEnable && IsPortActive(pActInfo, eIN_SET_ALL))
				{
					// Set AI
					float fAI = GetPortFloat(pActInfo, eIN_SET_AI);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_SET_AI, fAI);
					// Set Player
					float fPlayer = GetPortFloat(pActInfo, eIN_SET_PLAYER);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_SET_PLAYER, fPlayer);
					// Set Game
					float fGame = GetPortFloat(pActInfo, eIN_SET_GAME);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_SET_GAME, fGame);
					// Set Change
					float fChange = GetPortFloat(pActInfo, eIN_SET_CHANGE);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_SET_ALLOWCHANGE, fChange);
				}

				// Add All
				if (m_bEnable && IsPortActive(pActInfo, eIN_ADD_ALL))
				{
					// Set AI
					float fAI = GetPortFloat(pActInfo, eIN_SET_AI);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_CHANGE_AI, fAI);
					// Set Player
					float fPlayer = GetPortFloat(pActInfo, eIN_SET_PLAYER);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_CHANGE_PLAYER, fPlayer);
					// Set Game
					float fGame = GetPortFloat(pActInfo, eIN_SET_GAME);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_CHANGE_GAME, fGame);
					// Set Change
					float fChange = GetPortFloat(pActInfo, eIN_SET_CHANGE);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_CHANGE_ALLOWCHANGE, fChange);
				}

				// Set AI
				if (m_bEnable && IsPortActive(pActInfo, eIN_SET_AI))
				{
					float fSet = GetPortFloat(pActInfo, eIN_SET_AI);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_SET_AI, fSet);
				}

				// Add AI
				if (m_bEnable && IsPortActive(pActInfo, eIN_ADD_AI))
				{
					float fAdd = GetPortFloat(pActInfo, eIN_ADD_AI);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_CHANGE_AI, fAdd);
				}

				// Set Player
				if (m_bEnable && IsPortActive(pActInfo, eIN_SET_PLAYER))
				{
					float fSet = GetPortFloat(pActInfo, eIN_SET_PLAYER);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_SET_PLAYER, fSet);
				}

				// Add Player
				if (m_bEnable && IsPortActive(pActInfo, eIN_ADD_PLAYER))
				{
					float fAdd = GetPortFloat(pActInfo, eIN_ADD_PLAYER);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_CHANGE_PLAYER, fAdd);
				}
				
				// Set Game
				if (m_bEnable && IsPortActive(pActInfo, eIN_SET_GAME))
				{
					float fSet = GetPortFloat(pActInfo, eIN_SET_GAME);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_SET_GAME, fSet);
				}

				// Add Game
				if (m_bEnable && IsPortActive(pActInfo, eIN_ADD_GAME))
				{
					float fAdd = GetPortFloat(pActInfo, eIN_ADD_GAME);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_CHANGE_GAME, fAdd);
				}
				
				// Set Change
				if (m_bEnable && IsPortActive(pActInfo, eIN_SET_CHANGE))
				{
					float fSet = GetPortFloat(pActInfo, eIN_SET_CHANGE);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_SET_ALLOWCHANGE, fSet);
				}

				// Add Change
				if (m_bEnable && IsPortActive(pActInfo, eIN_ADD_CHANGE))
				{
					float fAdd = GetPortFloat(pActInfo, eIN_ADD_CHANGE);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_CHANGE_ALLOWCHANGE, fAdd);
				}

				// SetMultiplier
				if (m_bEnable && IsPortActive(pActInfo, eIN_SET_MULTIPLIER))
				{
					float fMultiplier = GetPortFloat(pActInfo, eIN_SET_MULTIPLIER);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_SET_MULTIPLIER, fMultiplier);
				}
				
				// SetAIMultiplier
				if (m_bEnable && IsPortActive(pActInfo, eIN_SET_AI_MULTIPLIER))
				{
					float fAIMultiplier = GetPortFloat(pActInfo, eIN_SET_AI_MULTIPLIER);
					pMusicLogic->SetEvent(eMUSICLOGICEVENT_SET_AI_MULTIPLIER, fAIMultiplier);
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
	bool m_bEnable;
	bool m_bRegistered;
};


REGISTER_FLOW_NODE("Sound:MusicLogicControl", CFlowNode_MusicLogicControl);
