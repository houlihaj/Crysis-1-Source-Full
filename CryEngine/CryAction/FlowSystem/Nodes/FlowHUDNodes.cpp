////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowHUDNodes.cpp
//  Version:     v1.00
//  Created:     July 4th 2005 by Julien.
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
#include <IActorSystem.h>
#include <PersistantDebug.h>

// display an instruction message in the HUD
class CFlowNode_DisplayInstructionMessage : public CFlowBaseNode
{
public:
	CFlowNode_DisplayInstructionMessage( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		// declare input ports
		static const SInputPortConfig in_ports[] = 
		{
			// a "void" port that can be pulsed to display the message
			InputPortConfig_Void( "display", _HELP("Connect event here to display the message" )),
			// this string port is the message that should be displayed
			InputPortConfig<string>( "text_message", _HELP("Display this message on the hud" )),
			// this floating point input port is how long the message should be displayed
			InputPortConfig<float>( "displayTime", 10.0f, _HELP("How long to display message" )),
			{0}
		};
		// we set pointers in "config" here to specify which input and output ports the node contains
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_OBSOLETE);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		// if the event is activate, and the 0th port ("display") has been activated,
		// display the message
		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
			if (CFlowSystemCVars::Get().m_noDebugText == 0)
			{
				static const ColorF col (175.0f/255.0f, 218.0f/255.0f, 154.0f/255.0f, 1.0f);      
				CCryAction::GetCryAction()->GetIPersistantDebug()->Add2DText(GetPortString(pActInfo,1).c_str(), 2.f, col, GetPortFloat(pActInfo, 2));
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

// display an info message in the HUD
class CFlowNode_DisplayInfoMessage : public CFlowBaseNode
{
public:
	CFlowNode_DisplayInfoMessage( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		// declare input ports
		static const SInputPortConfig in_ports[] = 
		{
			// a "void" port that can be pulsed to display the message
			InputPortConfig_Void( "display", _HELP("Connect event here to display the message" )),
			// this string port is the message that should be displayed
			InputPortConfig<string>( "message", _HELP("Display this message on the hud" )),
			// this floating point input port is how long the message should be displayed
			InputPortConfig<float>( "displayTime", 10.0f, _HELP("How long to display message" )),
			{0}
		};
		// we set pointers in "config" here to specify which input and output ports the node contains
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_OBSOLETE);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		// if the event is activate, and the 0th port ("display") has been activated,
		// display the message
		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
			if (CFlowSystemCVars::Get().m_noDebugText == 0)
			{
				static const ColorF col (175.0f/255.0f, 218.0f/255.0f, 154.0f/255.0f, 1.0f);
				CCryAction::GetCryAction()->GetIPersistantDebug()->Add2DText(GetPortString(pActInfo,1).c_str(), 2.f, col, GetPortFloat(pActInfo, 2));
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

// display a debug message in the HUD
class CFlowNode_DisplayDebugMessage : public CFlowBaseNode
{
public:
	CFlowNode_DisplayDebugMessage( SActivationInfo * pActInfo )
	{
	}

	void Serialize(SActivationInfo *pActInfo, TSerialize ser)
	{
		// CryLogAlways("CFlowNode_DisplayDebugMessage: Message='%s'", GetPortString(pActInfo, 0));
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		// declare input ports
		static const SInputPortConfig in_ports[] = 
		{
			// this string port is the message that should be displayed
			InputPortConfig<string>( "message", _HELP("Display this message on the hud" )),
				// this floating point input port is the x position where the message should be displayed
				InputPortConfig<float>( "posX",	50.0f, _HELP("Input x text position" )),
				// this floating point input port is the y position where the message should be displayed
				InputPortConfig<float>( "posY",	50.0f, _HELP("Input y text position" )),
				// this floating point input port is the font size of the message that should be displayed
				InputPortConfig<float>( "fontSize",	2.0f, _HELP("Input font size" )),
			{0}
		};
		// we set pointers in "config" here to specify which input and output ports the node contains
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_OBSOLETE);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (CFlowSystemCVars::Get().m_noDebugText != 0)
			return;

		if (event == eFE_Initialize)
		{
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
		}
		else if (event == eFE_Update)
		{
			IRenderer *pRenderer = gEnv->pRenderer;
			float drawColor[4] = {1,1,1,1};
			pRenderer->Draw2dLabel(		GetPortFloat(pActInfo,1), 
				GetPortFloat(pActInfo,2), 
				GetPortFloat(pActInfo,3), 
				drawColor, 
				false, 
				GetPortString(pActInfo,0).c_str() );
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};



// display a debug message in the HUD
class CFlowNode_DisplayTimedDebugMessage : public CFlowBaseNode
{
	CTimeValue m_endTime;

public:
	CFlowNode_DisplayTimedDebugMessage( SActivationInfo * pActInfo )
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo *pActInfo)
	{
		return new CFlowNode_DisplayTimedDebugMessage(pActInfo);
	}

	void Serialize(SActivationInfo *, TSerialize ser)
	{
		ser.Value("m_endTime", m_endTime);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		// declare input ports
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void   ( "Trigger", _HELP("Trigger this port to display message" )),
			InputPortConfig<string>( "Message", _HELP("Message to display" )),
			InputPortConfig<float> ( "DisplayTime", 2.0f,  _HELP("How long to display in seconds (<=0.0 means forever)" )),
			InputPortConfig<float> ( "PosX",	50.0f, _HELP("X Position of text" )),
			InputPortConfig<float> ( "PosY",	50.0f, _HELP("Y Position of text" )),
			InputPortConfig<float> ( "FontSize",	2.0f, _HELP("Input font size" )),
			{0}
		};
		// we set pointers in "config" here to specify which input and output ports the node contains
		config.sDescription = _HELP("Display a debug [Message] for [DisplayTime] seconds");
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_DEBUG);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{

		if (event == eFE_Initialize)
		{
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
		}
		else if (event == eFE_Activate)
		{
			if (CFlowSystemCVars::Get().m_noDebugText == 0)
			{
				if (IsPortActive(pActInfo, 0))
				{
					m_endTime = gEnv->pTimer->GetFrameStartTime() + GetPortFloat(pActInfo, 2);
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
				}
			}
		}
		else if (event == eFE_Update)
		{
			if (CFlowSystemCVars::Get().m_noDebugText == 0)
			{	
				IRenderer *pRenderer = gEnv->pRenderer;
				float drawColor[4] = {1,1,1,1};
				pRenderer->Draw2dLabel(GetPortFloat(pActInfo,3), 
					GetPortFloat(pActInfo,4), 
					GetPortFloat(pActInfo,5), 
					drawColor, 
					false, 
					GetPortString(pActInfo,1).c_str() );
			}
			// check if we should show forever...
			if (GetPortFloat(pActInfo, 2) > 0.0f)
			{
				// no, so check if time-out
				CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();
				if (curTime >= m_endTime)
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
			}
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE_SINGLETON("HUD:DisplayInstructionMessage",	CFlowNode_DisplayInstructionMessage);
REGISTER_FLOW_NODE_SINGLETON("HUD:DisplayInfoMessage",				CFlowNode_DisplayInfoMessage);
REGISTER_FLOW_NODE_SINGLETON("HUD:DisplayDebugMessage",				CFlowNode_DisplayDebugMessage);
REGISTER_FLOW_NODE("HUD:DisplayTimedDebugMessage",	CFlowNode_DisplayTimedDebugMessage);
