////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   MaterialHUDFXNodes.cpp
//  Version:     v1.00
//  Created:     29-11-2006 by AlexL - Benito GR
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MaterialEffects.h"
#include <FlowSystem/Nodes/FlowBaseNode.h>

//CHUDStartFXNode
//Special node that let us trigger a FlowGraph HUD effect related to 
//some kind of material, impact, explosion...
class CHUDStartFXNode : public CFlowBaseNode
{
public:
	CHUDStartFXNode(SActivationInfo* pActInfo)
	{
	}

	~CHUDStartFXNode()
	{
	}

	enum 
	{
		EIP_Trigger = 0,
	};

	enum
	{
		EOP_Started = 0,
		EOP_Distance,
		EOP_Param1,
		EOP_Param2,
		EOP_Param3,
		EOP_Param4
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void  ("Start",_HELP("Triggered automatically by MaterialEffects")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void   ("Started",_HELP("Triggered when Effect is started.")),
			OutputPortConfig<float> ("Distance",_HELP("Distance to player")),
			OutputPortConfig<float> ("Param1",_HELP("Custom Param 1")),
			OutputPortConfig<float> ("Param2",_HELP("Custom Param 2")),
			OutputPortConfig<float> ("Param3",_HELP("Custom Param 3")),
			OutputPortConfig<float> ("Param4",_HELP("Custom Param 4")),
			{0}
		};
		config.sDescription = _HELP("MaterialFX StartNode");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	//Just activate the output when the input is activated
	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Trigger))
				ActivateOutput(pActInfo, EOP_Started, true);
			break;
		}
	}

};

//CHUDEndFXNode
//Special node that let us know when the FlowGraph HUD Effect has finish
class CHUDEndFXNode : public CFlowBaseNode
{
public:
	CHUDEndFXNode(SActivationInfo* pActInfo)
	{
	}

	~CHUDEndFXNode()
	{
	}

	enum 
	{
		EIP_Trigger = 0,
	};

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void     ("Trigger", _HELP("Trigger to mark end of effect.")),
			{0}
		};
		config.sDescription = _HELP("MaterialFX EndNode");
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}

	//When activated, just notify the MaterialFGManager that the effect ended
	//See MaterialFGManager.cpp
	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Trigger))
			{
				CMaterialEffects* pMatFX = static_cast<CMaterialEffects*> (gEnv->pGame->GetIGameFramework()->GetIMaterialEffects());
				if (pMatFX)
				{
					//const string& name = GetPortString(pActInfo, EIP_Name);
					pMatFX->NotifyFGHudEffectEnd(pActInfo->pGraph);
				}
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE_SINGLETON( "MaterialFX:HUDStartFX", CHUDStartFXNode );
REGISTER_FLOW_NODE_SINGLETON( "MaterialFX:HUDEndFX", CHUDEndFXNode );
