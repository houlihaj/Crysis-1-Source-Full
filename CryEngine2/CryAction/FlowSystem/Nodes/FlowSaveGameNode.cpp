////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowSaveGameNode.cpp
//  Version:     v1.00
//  Created:     28-08-2006 by AlexL
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <FlowSystem/Nodes/FlowBaseNode.h>

#include "CryAction.h"
#include "IGame.h"

class CFlowSaveGameNode : public CFlowBaseNode
{
public:
	CFlowSaveGameNode(SActivationInfo* pActInfo)
	{
	}

	~CFlowSaveGameNode()
	{
	}

	/*
	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return this; // new CFlowSaveGameNode(pActInfo);
	}
	*/

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	enum 
	{
		EIP_Save = 0,
		EIP_Load,
		EIP_Name,
		EIP_Desc,
		EIP_InGameOnly,
		EIP_EnableSave,
		EIP_DisableSave,
	};

	enum 
	{
		EOP_Saved = 0,
		EOP_Loaded,
	};

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void ("Save",_HELP("Trigger to save game")),
			InputPortConfig_Void( "Load",_HELP("Trigger to load game")),
			InputPortConfig<string> ("Name", string("quicksave"), _HELP("Name of SaveGame to save/load. Use $LAST to load last savegame")),
			InputPortConfig<string> ("Desc", string(""), _HELP("Description [Currently ignored]"), _HELP("Description")),
			InputPortConfig<bool> ("InGameOnly", true, _HELP("Load/Save in pure game mode only. Default=true.")),
			InputPortConfig_Void ("EnableSave",_HELP("Trigger to globally allow quick-saving")),
			InputPortConfig_Void ("DisableSave",_HELP("Trigger to globally disallow quick-saving")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void ("SaveDone", _HELP("Triggered when saved")),
			OutputPortConfig_Void ("LoadDone", _HELP("Triggered when loaded")),
			{0}
		};
		config.sDescription = _HELP("SaveGame for Autosave");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_DisableSave))
				CCryAction::GetCryAction()->AllowSave(false);
			if (IsPortActive(pActInfo, EIP_EnableSave))
				CCryAction::GetCryAction()->AllowSave(true);

			// when GameOnly and we're in Editor, return
			if (GetPortBool(pActInfo, EIP_InGameOnly) && gEnv->pSystem->IsEditor())
				return;

			if (IsPortActive(pActInfo, EIP_Save))
			{
				string name = GetPortString(pActInfo, EIP_Name);
				PathUtil::RemoveExtension(name);
				//name+=".CRYSISJMSF";

				if(IGame *pGame = gEnv->pGame)
				{
					ICVar *pAutosave = gEnv->pConsole->GetCVar("g_enableAutoSave");
					if(pAutosave && !pAutosave->GetIVal())
						CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Autosave was disabled, savegame ignored."); 
					else
						CCryAction::GetCryAction()->SaveGame(pGame->CreateSaveGameName(), true, false, eSGR_FlowGraph, false, name.c_str());
				}
				else
					CCryAction::GetCryAction()->SaveGame(name.c_str(), true, false, eSGR_FlowGraph);
				ActivateOutput(pActInfo, EOP_Saved, true);
			}
			if (IsPortActive(pActInfo, EIP_Load))
			{
				string name = GetPortString(pActInfo, EIP_Name);
				if (name == "$LAST")
				{
					CCryAction::GetCryAction()->ExecuteCommandNextFrame("loadLastSave");
				}
				else
				{
					PathUtil::RemoveExtension(name);
					name+=".CRYSISJMSF";
					CCryAction::GetCryAction()->LoadGame(name.c_str(), true);
				}
				ActivateOutput(pActInfo, EOP_Loaded, true);
			}
			break;
		}
	}
};

REGISTER_FLOW_NODE_SINGLETON( "System:SaveGame", CFlowSaveGameNode );
