////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowInputNode.cpp
//  Version:     v1.00
//  Created:     13/10/2006 by AlexL.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowBaseNode.h"
#include <CryAction.h>
#include <IGameObjectSystem.h>
#include <StringUtils.h>
#include <IInput.h>

class CFlowNode_InputKey : public CFlowBaseNode, public IInputEventListener
{
	enum INPUTS {
		EIP_ENABLE = 0,
		EIP_DISABLE,
		EIP_KEY,
		EIP_NONDEVMODE,
	};

	enum OUTPUTS
	{
		EOP_PRESSED,
		EOP_RELEASED,
	};

public:
	CFlowNode_InputKey( SActivationInfo * pActInfo ) : m_bActive(false)
	{
	}

	~CFlowNode_InputKey()
	{
		Register(false, 0);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		// declare input ports
		static const SInputPortConfig inputs[] = 
		{
			InputPortConfig_Void( "Enable", _HELP("Trigger to enable. Default: Already enabled." )),
			InputPortConfig_Void( "Disable", _HELP("Trigger to disable" )),
			InputPortConfig<string>( "Key", _HELP("Key name. Leave empty for any." )),
			InputPortConfig<bool> ("NonDevMode", false, _HELP("If set to true, can be used in Non-Devmode as well [Debugging backdoor]")),
			{0}
		};
		static const SOutputPortConfig outputs[] = 
		{
			OutputPortConfig<string>( "Pressed", _HELP("Key pressed." )),
			OutputPortConfig<string>( "Released", _HELP("Key released." )),
			{0}
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to catch key inputs. Use only for debugging. It is enabled by default.");
		config.SetCategory(EFLN_DEBUG);
	}

	IFlowNodePtr Clone(SActivationInfo *pActInfo)
	{
		return new CFlowNode_InputKey(pActInfo);
	}

	void Register(bool bRegister, SActivationInfo *pActInfo)
	{
		IInput* pInput = gEnv->pInput;
		if (pInput)
		{
			if (bRegister)
			{
				assert (pActInfo != 0);
				const bool bAllowedInNonDevMode = GetPortBool(pActInfo, EIP_NONDEVMODE );
				if (gEnv->pSystem->IsDevMode() || bAllowedInNonDevMode)
					pInput->AddEventListener(this);
			}
			else
				pInput->RemoveEventListener(this);
		}
		m_bActive = bRegister;
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_bActive", m_bActive);
		if (ser.IsReading())
		{
			Register(m_bActive, pActInfo);
		}
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{	
			case eFE_Initialize:
				m_actInfo = *pActInfo;
				Register(true, pActInfo);
				break;
			case eFE_Activate:
				if (IsPortActive(pActInfo, EIP_DISABLE))
					Register(false, pActInfo);
				if (IsPortActive(pActInfo, EIP_ENABLE))
					Register(true, pActInfo);
				break;
		}
	}

	virtual bool OnInputEvent(const SInputEvent& event)
	{
		const string& keyName = GetPortString(&m_actInfo, EIP_KEY);
		if (keyName.empty() == false)
		{
			if (stricmp(keyName.c_str(), event.keyName.c_str()) != 0)
				return false;
		}
		string realKey((const char*)(event.keyName));
		if (event.state == eIS_Pressed)
			ActivateOutput(&m_actInfo, EOP_PRESSED, realKey);
		else if (event.state == eIS_Released)
			ActivateOutput(&m_actInfo, EOP_RELEASED, realKey);

		// return false, so other listeners get notification as well
		return false;
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	SActivationInfo m_actInfo;
	bool m_bActive;
};


class CFlowNode_InputActionFilter : public CFlowBaseNode
{
	enum INPUTS {
		EIP_ENABLE = 0,
		EIP_DISABLE,
		EIP_FILTER
	};

	enum OUTPUTS
	{
		EOP_ENABLED,
		EOP_DISABLED,
	};

public:
	CFlowNode_InputActionFilter( SActivationInfo * pActInfo ) : m_bEnabled(false)
	{
	}

	~CFlowNode_InputActionFilter()
	{
		EnableFilter(false);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		// declare input ports
		static const SInputPortConfig inputs[] = 
		{
			InputPortConfig_Void( "Enable", _HELP("Trigger to enable ActionFilter" )),
			InputPortConfig_Void( "Disable", _HELP("Trigger to disable ActionFilter" )),
			InputPortConfig<string>( "Filter", _HELP("Action Filter"), 0, _UICONFIG("enum_global:action_filter") ),
			{0}
		};
		static const SOutputPortConfig outputs[] = 
		{
			OutputPortConfig_Void("Enabled", _HELP("Triggered when enabled." )),
			OutputPortConfig_Void("Disabled", _HELP("Triggered when disabled." )),
			{0}
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to enable/disable ActionFilters");
		config.SetCategory(EFLN_ADVANCED);
	}

	IFlowNodePtr Clone(SActivationInfo *pActInfo)
	{
		return new CFlowNode_InputActionFilter(pActInfo);
	}

	void Serialize(SActivationInfo *, TSerialize ser)
	{
		// disable on reading
		if (ser.IsReading())
			EnableFilter(false);
		ser.Value("m_filterName", m_filterName);
		// on saving we ask the ActionMapManager if the filter is enabled
		// so, all basically all nodes referring to the same filter will write the correct (current) value
		// on quickload, all of them or none of them will enable/disable the filter again
		// maybe use a more central location for this
		if (ser.IsWriting())
		{
			bool bWroteIt = false;
			IActionMapManager* pAMMgr = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();
			if (pAMMgr && m_filterName.empty() == false)
			{
				IActionFilter* pFilter = pAMMgr->GetActionFilter(m_filterName.c_str());
				if (pFilter)
				{
					bool bIsEnabled = pAMMgr->IsFilterEnabled(m_filterName.c_str());
					ser.Value("m_bEnabled", bIsEnabled);
					bWroteIt = true;
				}
			}
			if (!bWroteIt)
				ser.Value("m_bEnabled", m_bEnabled);
		}
		else
		{
			ser.Value("m_bEnabled", m_bEnabled);
		}
		// re-enable
		if (ser.IsReading())
			EnableFilter(m_bEnabled);
	}

	void EnableFilter(bool bEnable)
	{
		if (m_filterName.empty())
			return;
		m_bEnabled = bEnable;
		IActionMapManager* pAMMgr = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();
		if (pAMMgr)
		{
			pAMMgr->EnableFilter(m_filterName, bEnable);
		}
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{	
		case eFE_Initialize:
			EnableFilter(false);
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_DISABLE))
			{
				m_filterName = GetPortString(pActInfo, EIP_FILTER);
				EnableFilter(false);
			}
			if (IsPortActive(pActInfo, EIP_ENABLE))
			{
				m_filterName = GetPortString(pActInfo, EIP_FILTER);
				EnableFilter(true);
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

protected:
	bool m_bEnabled;
	string m_filterName; // we need to store this name, as it the d'tor we need to disable the filter, but don't have access to port names
};

REGISTER_FLOW_NODE("Input:Key",	CFlowNode_InputKey);
REGISTER_FLOW_NODE("Input:ActionFilter", CFlowNode_InputActionFilter);
