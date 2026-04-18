/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a flow node to toggel vehicle lights 

-------------------------------------------------------------------------
History:
- 03:03:2007: Created by MichaelR

*************************************************************************/
#ifndef __FLOWVEHICLELights_H__
#define __FLOWVEHICLELights_H__

#include "FlowVehicleBase.h"

class CFlowVehicleLights
	: public CFlowVehicleBase
{
public:

	CFlowVehicleLights(SActivationInfo* pActivationInfo) { Init(pActivationInfo); }
	~CFlowVehicleLights() { Delete(); }

	// CFlowBaseNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActivationInfo);
	virtual void GetConfiguration(SFlowNodeConfig& nodeConfig);
	virtual void ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo);
	// ~CFlowBaseNode

	// IVehicleEventListener
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) {}
	// ~IVehicleEventListener

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

protected:

	enum EInputs
	{
    eIn_Lights = 0,
		eIn_Activate,
    eIn_Deactivate
	};

	enum EVehicleLights
  {
    eVL_All = 0,
    eVL_HeadLights,
    eVL_SearchLights
  };

};

#endif
