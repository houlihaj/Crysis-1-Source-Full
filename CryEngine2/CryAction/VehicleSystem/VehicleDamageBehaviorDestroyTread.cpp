/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2008.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a damage behavior which destroys a tread part

-------------------------------------------------------------------------
History:
- 07.05.2008: Created by Steven Humphreys

*************************************************************************/
#include "StdAfx.h"

#include "ICryAnimation.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimatedJoint.h"

#include "VehicleDamageBehaviorDestroyTread.h"

//------------------------------------------------------------------------
CVehicleDamageBehaviorDestroyTread::CVehicleDamageBehaviorDestroyTread()
{
}

//------------------------------------------------------------------------
CVehicleDamageBehaviorDestroyTread::~CVehicleDamageBehaviorDestroyTread()
{
}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorDestroyTread::Init(IVehicle* pVehicle, const SmartScriptTable &table)
{
	m_pVehicle = pVehicle;

	SmartScriptTable destroyTreadParams;
	if (table->GetValue("DestroyTread", destroyTreadParams))
	{
		char* pPartName;
		if (destroyTreadParams->GetValue("part", pPartName))
		{
			m_partName = pPartName;

			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorDestroyTread::Init(IVehicle* pVehicle, const string& partName)
{
	m_pVehicle = pVehicle;
	m_partName = partName;

	return true;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDestroyTread::Reset()
{

}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDestroyTread::Serialize(TSerialize ser, unsigned aspects)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorDestroyTread::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
	if ((event == eVDBE_Hit || event == eVDBE_Repair) && behaviorParams.pVehicleComponent)
	{
		if(!m_partName.empty())
		{
			if(IVehiclePart* pPart = m_pVehicle->GetPart(m_partName.c_str()))
			{
				IVehiclePart::SVehiclePartEvent partEvent;

				if(event == eVDBE_Hit)
				{
					partEvent.type = IVehiclePart::eVPE_Damaged;
					partEvent.fparam = 1.0f;
				}
				else
				{
					partEvent.type = IVehiclePart::eVPE_Repair;
					partEvent.fparam = 0.0f;
				}
				pPart->OnEvent(partEvent);
			}
		}
	}
}

void CVehicleDamageBehaviorDestroyTread::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->Add(m_partName);
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorDestroyTread);
