#include "StdAfx.h"
#include "FlowBaseNode.h"
#include "CryAction.h"
#include "IGameRulesSystem.h"

#include "CopyProtection.h"

#define NEXT_LEVEL_LIMIT	66

class CFlowNode_EndLevel : public CFlowBaseNode
{
public:
	CFlowNode_EndLevel( SActivationInfo * pActInfo )
	{
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void   ( "Trigger",   _HELP("Finish the current mission (go to next level)") ),
			InputPortConfig<string>( "NextLevel", _HELP("Which level is the next level?") ),
			{0}
		};
		config.sDescription = _HELP("Advance to a new level");
		config.pInputPorts  = in_ports;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event == eFE_Activate && IsPortActive(pActInfo,0))
		{
#ifdef SECUROM_32		
			DWORD sp_result = 0;

			IActor *pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
			if (pActor)
			{
				int prg=0;
				int dmy;
				pActor->GetProgress(prg, dmy);
				if (prg < NEXT_LEVEL_LIMIT)
				{
					CCryAction::GetCryAction()->ScheduleEndLevel(GetPortString(pActInfo, 1));
					return;
				}
			}

			SECUROM_MARKER_SECURITY_ON(303)
			
			Sony_VM.code_ptr = SMS_INT;
			Sony_VM.std.Param4 = SPOT_CHECK_1;
			Sony_VM.std.Param5 = (DWORD)&sp_result;

			SendMessage(SecuROM.sms_handle, SecuROM.SMS_spotcheck, NULL, (LPARAM) & Sony_VM);
			SECUROM_MARKER_SECURITY_OFF(303)

		  if (sp_result==TRIGGER_OK)
				CCryAction::GetCryAction()->ScheduleEndLevel(GetPortString(pActInfo, 1));
#else
			CCryAction::GetCryAction()->ScheduleEndLevel(GetPortString(pActInfo, 1));
#endif
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE_SINGLETON("Mission:EndLevelNew", CFlowNode_EndLevel);
