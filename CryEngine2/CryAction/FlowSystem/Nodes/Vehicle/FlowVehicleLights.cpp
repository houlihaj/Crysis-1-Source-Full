/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a flow node to toggel vehicle lights 

-------------------------------------------------------------------------
History:
- 03:03:2007: Created by MichaelR

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "IFlowSystem.h"
#include "FlowSystem/Nodes/FlowBaseNode.h"
#include "FlowVehicleLights.h"
#include "VehicleSystem/VehicleSeat.h"
#include "VehicleSystem/VehicleSeatActionLights.h"

//------------------------------------------------------------------------
IFlowNodePtr CFlowVehicleLights::Clone(SActivationInfo* pActivationInfo)
{
	IFlowNode* pNode = new CFlowVehicleLights(pActivationInfo);
	return pNode;
}

//------------------------------------------------------------------------
void CFlowVehicleLights::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	CFlowVehicleBase::GetConfiguration(nodeConfig);

	static const SInputPortConfig pInConfig[] = 
	{
    InputPortConfig<int>("Lights", _HELP("Lights to be activated"), _HELP("Lights"), _UICONFIG("enum_int:All=0,HeadLights=1,SearchLights=2") ),
		InputPortConfig<SFlowSystemVoid>("Activate", _HELP("Activate lights")),
    InputPortConfig<SFlowSystemVoid>("Deactivate", _HELP("Deactivate lights")),
		{0}
	};

	static const SOutputPortConfig pOutConfig[] = 
	{
		{0}
	};

	nodeConfig.sDescription = _HELP("Toggle vehicle lights");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_APPROVED);
}

//------------------------------------------------------------------------
void CFlowVehicleLights::ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
{
	CFlowVehicleBase::ProcessEvent(flowEvent, pActivationInfo);

	if (flowEvent == eFE_Activate)
	{
		if (IVehicle* pVehicle = GetVehicle())
		{
      bool activate = IsPortActive(pActivationInfo, eIn_Activate);
      bool deactivate = IsPortActive(pActivationInfo, eIn_Deactivate);
			
      if (activate || deactivate)
			{
		    int lights = GetPortInt(pActivationInfo, eIn_Lights);

        for (int i=InvalidVehicleSeatId+1,n=pVehicle->GetSeatCount(); i<=n; ++i)
        {
          CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(pVehicle->GetSeatById(i));

          if (lights == eVL_HeadLights && !pSeat->IsDriver())
            continue;
          else if (lights == eVL_SearchLights && !pSeat->IsGunner())
            continue;
          
          const TVehicleSeatActionVector& actions = pSeat->GetSeatActions();
          for (TVehicleSeatActionVector::const_iterator it=actions.begin(),end=actions.end(); it!=end; ++it)
          {
            if (CVehicleSeatActionLights* pLights = CAST_VEHICLEOBJECT(CVehicleSeatActionLights, it->pSeatAction))
            {
              pLights->OnAction(eVAI_ToggleLights, eAAM_OnPress, activate?1:0);
            }
          }   

          if (lights == eVL_HeadLights) // break after driver seat
            break;
        }
			}
		}
	}
}

REGISTER_FLOW_NODE( "Vehicle:VehicleLights", CFlowVehicleLights);
