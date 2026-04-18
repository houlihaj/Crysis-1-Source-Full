/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a flow node to handle vehicle damages

-------------------------------------------------------------------------
History:
- 02:12:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "VehicleSystem/Vehicle.h"
#include "IFlowSystem.h"
#include "FlowSystem/Nodes/FlowBaseNode.h"
#include "FlowVehicleDamage.h"

//------------------------------------------------------------------------
IFlowNodePtr CFlowVehicleDamage::Clone(SActivationInfo* pActivationInfo)
{
	IFlowNode* pNode = new CFlowVehicleDamage(pActivationInfo);
	return pNode;
}

//------------------------------------------------------------------------
void CFlowVehicleDamage::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	CFlowVehicleBase::GetConfiguration(nodeConfig);

	static const SInputPortConfig pInConfig[] = 
	{
		InputPortConfig_AnyType("HitTrigger", _HELP("trigger which cause the vehicle to receive damages")),
		InputPortConfig<float>("HitValue", _HELP("hit value which the vehicle will receive when HitTrigger is set")),
		InputPortConfig<Vec3>("HitPosition", _HELP("local position at which the vehicle will receive the hit")),
		InputPortConfig<float>("HitRadius", _HELP("radius of the hit")),
    InputPortConfig<bool>("Indestructible", _HELP("Trigger true to set vehicle to indestructible, false to reset.")),
		InputPortConfig<string>("HitType", _HELP("an hit type which should be known by the GameRules")),
		{0}
	};

	static const SOutputPortConfig pOutConfig[] = 
	{
		OutputPortConfig<float>("Damaged", _HELP("hit value received by the vehicle which caused some damage")),
		OutputPortConfig<bool>("Destroyed", _HELP("vehicle gets destroyed")),
		OutputPortConfig<float>("Hit", _HELP("hit value received by the vehicle which may or may not cause damage")),
		{0}
	};

	nodeConfig.sDescription = _HELP("handle vehicle damage");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_APPROVED);
}

//------------------------------------------------------------------------
void CFlowVehicleDamage::ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
{
	CFlowVehicleBase::ProcessEvent(flowEvent, pActivationInfo);
	
	if (flowEvent == eFE_Initialize)
	{
		ActivateOutput(pActivationInfo, OUT_DAMAGED, 0.0f);
		ActivateOutput(pActivationInfo, OUT_DESTROYED, false);
		ActivateOutput(pActivationInfo, OUT_HIT, 0.0f);
	}
  
	if (flowEvent == eFE_Activate)
	{
    if (IsPortActive(pActivationInfo, IN_INDESTRUCTIBLE))
    {
      if (CVehicle* pVehicle = static_cast<CVehicle*>(GetVehicle()))
      {
        SVehicleEventParams params;
        params.bParam = GetPortBool(pActivationInfo, IN_INDESTRUCTIBLE);
        pVehicle->BroadcastVehicleEvent(eVE_Indestructible, params);
      }
    }  

		if (IsPortActive(pActivationInfo, IN_HITTRIGGER))
		{
			float hitValue = GetPortFloat(pActivationInfo, IN_HITVALUE);

			if (hitValue > 0.0f)
			{
				Vec3 hitPos;
				hitPos.zero();
				hitPos = GetPortVec3(pActivationInfo, IN_HITPOS);

				float hitRadius = 0.0f;
				hitRadius = GetPortFloat(pActivationInfo, IN_HITRADIUS);

				if (IVehicle* pVehicle = GetVehicle())
				{
					string hitType = GetPortString(pActivationInfo, IN_HITTYPE);
					static_cast<CVehicle*>(pVehicle)->OnHit(pVehicle->GetEntityId(), pVehicle->GetEntityId(), hitValue, pVehicle->GetEntity()->GetWorldTM()*hitPos, hitRadius, hitType, false);

				}
			}
		} 
	}
}

//------------------------------------------------------------------------
void CFlowVehicleDamage::Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
{
	CFlowVehicleBase::Serialize(pActivationInfo, ser);
}

//------------------------------------------------------------------------
void CFlowVehicleDamage::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	CFlowVehicleBase::OnVehicleEvent(event, params);

	if (event == eVE_Damaged)
	{
		SFlowAddress addr(m_nodeID, OUT_DAMAGED, true);
		m_pGraph->ActivatePort(addr, params.fParam);
	}
	else if (event == eVE_Destroyed)
	{
		SFlowAddress addr(m_nodeID, OUT_DESTROYED, true);
		m_pGraph->ActivatePort(addr, params.bParam);
	}
	else if (event == eVE_Hit)
	{
		SFlowAddress addr(m_nodeID, OUT_HIT, true);
		m_pGraph->ActivatePort(addr, params.fParam);
	}
}



REGISTER_FLOW_NODE( "Vehicle:VehicleDamage", CFlowVehicleDamage);
