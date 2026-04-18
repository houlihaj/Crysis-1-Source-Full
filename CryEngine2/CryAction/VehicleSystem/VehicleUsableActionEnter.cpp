/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a usable action to enter a vehicle seat

-------------------------------------------------------------------------
History:
- 19:01:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "GameObjects/GameObject.h"
#include "IActorSystem.h"
#include "IGameObject.h"
#include "IItem.h"
#include "IItemSystem.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleSeat.h"
#include "VehicleSeatGroup.h"
#include "VehicleUsableActionEnter.h"

//------------------------------------------------------------------------
bool CVehicleUsableActionEnter::Init(IVehicle* pVehicle, const SmartScriptTable &table)
{
	SmartScriptTable enterTable;
	if (!table->GetValue("Enter", enterTable))
		return false;

	m_pVehicle = static_cast<CVehicle*>(pVehicle);

	SmartScriptTable seatsTables;
	if (enterTable->GetValue("Seats", enterTable))
	{
		IScriptTable::Iterator seatsIte = enterTable->BeginIteration();

		while (enterTable->MoveNext(seatsIte))
		{
			char* pSeatName;
			if (seatsIte.value.CopyTo(pSeatName))
			{
				if (TVehicleSeatId seatId = m_pVehicle->GetSeatId(pSeatName))
					m_seatIds.push_back(seatId);
			}
		}

		enterTable->EndIteration(seatsIte);
	}
	
	return (m_seatIds.size() > 0);
}

//------------------------------------------------------------------------
int CVehicleUsableActionEnter::OnEvent(int eventType, SVehicleEventParams& eventParams)
{
	if (eventType == eVAE_IsUsable)
	{
		EntityId& userId = eventParams.entityId;

		for (TVehicleSeatIdVector::iterator ite = m_seatIds.begin(); ite != m_seatIds.end(); ++ite)
		{
			if (IVehicleSeat* pSeat = m_pVehicle->GetSeatById(*ite))
			{
				if (IsSeatAvailable(pSeat->GetSeatId(), userId))
				{
					eventParams.iParam = pSeat->GetSeatId();
					return 1;
				}
			}
		}

		return 0;
	}
	else if (eventType == eVAE_OnUsed)
	{
		EntityId& userId = eventParams.entityId;
		if(VehicleCVars().v_driverControlledMountedGuns == 1)
		{
			if(eventParams.iParam>0)
			{
				TVehicleSeatId driverId=m_pVehicle->GetDriverSeatId();
				if(IsSeatAvailable(driverId, userId)) 
					eventParams.iParam=driverId;
			}
		}

		IVehicleSeat* pSeat = m_pVehicle->GetSeatById(eventParams.iParam);

		if (pSeat && IsSeatAvailable(pSeat->GetSeatId(), userId))
			return pSeat->Enter(userId);

		return -1;
	}

	return 0;
}

//------------------------------------------------------------------------
bool CVehicleUsableActionEnter::IsSeatAvailable(TVehicleSeatId seatId, EntityId userId)
{
	IEntity* pUserEntity = gEnv->pEntitySystem->GetEntity(userId);
	if (!pUserEntity)
		return false;

	CVehicleSeat* pSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(seatId);
	if (!pSeat)
		return false;

	if (!pSeat->IsFree())
		return false;

	if (CVehicleSeatGroup* pGroup = pSeat->GetSeatGroup())
	{
		IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
		if (IAIObject* pAIObject = pUserEntity->GetAI())
		{
			for (int i = 0; i < pGroup->GetSeatCount(); i++)
			{
				if (CVehicleSeat* pGroupSeat = pGroup->GetSeatByIndex(i))
					if (EntityId passengerId = pGroupSeat->GetPassenger())
						if (IActor* pActor = pActorSystem->GetActor(passengerId))
							if (IAIObject* pActorAIObject = pActor->GetEntity()->GetAI())
								if (pActor->GetHealth() > 0 && pUserEntity->GetAI() && 
									pUserEntity->GetAI()->IsHostile(pActorAIObject,false))
									return false;
			}
		}
	}

	return true;
}

void CVehicleUsableActionEnter::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_seatIds);
}

DEFINE_VEHICLEOBJECT(CVehicleUsableActionEnter);
