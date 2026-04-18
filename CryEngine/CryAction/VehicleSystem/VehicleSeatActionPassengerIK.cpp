/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a seat action to handle IK on the passenger body

-------------------------------------------------------------------------
History:
- 10:01:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IActorSystem.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleSeatActionPassengerIK.h"

#include "IRenderAuxGeom.h"

//------------------------------------------------------------------------
bool CVehicleSeatActionPassengerIK::Init(IVehicle* pVehicle, TVehicleSeatId seatId, const SmartScriptTable &table)
{
	m_pVehicle = pVehicle;
	m_passengerId = 0;

	SmartScriptTable ikTable;
	if (!table->GetValue("PassengerIK", ikTable))
		return false;

	SmartScriptTable limbsTable;
	if (!ikTable->GetValue("Limbs", limbsTable))
		return false;

	if (!ikTable->GetValue("waitShortlyBeforeStarting", m_waitShortlyBeforeStarting))
		m_waitShortlyBeforeStarting = false;

	IScriptTable::Iterator limbIte = limbsTable->BeginIteration();
	m_ikLimbs.reserve(limbsTable->Count());

	while (limbsTable->MoveNext(limbIte))
	{
		if (limbIte.value.GetVarType() == svtObject)
		{
			SmartScriptTable limbTable;

			if (limbIte.value.CopyTo(limbTable))
			{
				SIKLimb limb;
				char* name;

				if (limbTable->GetValue("limb", name))
				{
					limb.limbName = name;

					if (limbTable->GetValue("helper", name))
					{
						limb.pHelper = m_pVehicle->GetHelper(name);


						m_ikLimbs.push_back(limb);
					}
				}
			}
		}

	}

	limbsTable->EndIteration(limbIte);

	if (m_ikLimbs.size() == 0)
		return false;

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeatActionPassengerIK::Reset()
{

}

//------------------------------------------------------------------------
void CVehicleSeatActionPassengerIK::StartUsing(EntityId passengerId)
{
	m_passengerId = passengerId;

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_Visible);
}

//------------------------------------------------------------------------
void CVehicleSeatActionPassengerIK::StopUsing()
{
	m_passengerId = 0;

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
}

//------------------------------------------------------------------------
void CVehicleSeatActionPassengerIK::Update(float frameTime)
{
	if (!m_passengerId)
		return;

	IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(m_passengerId);
	if (!pActor)
	{
		assert(!"Invalid entity id for the actor id, Actor System didn't know it");
		return;
	}

	if (ICharacterInstance* pCharInstance = pActor->GetEntity()->GetCharacter(0))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		assert(pSkeletonAnim);

		if (pSkeletonAnim->GetNumAnimsInFIFO(0) >= 1)
		{
			CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(0, 0);
			if (!m_waitShortlyBeforeStarting || (anim.m_nLoopCount > 0 || anim.m_fAnimTime > 0.9f))
			{
				for (TIKLimbVector::iterator ite = m_ikLimbs.begin(); ite != m_ikLimbs.end(); ++ite)
				{
					const SIKLimb& limb = *ite;

					Vec3 helperPos = limb.pHelper->GetWorldTM().GetTranslation();
					pActor->SetIKPos(limb.limbName.c_str(), helperPos, 1);
				}
			}
		}
	}
}

void CVehicleSeatActionPassengerIK::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_ikLimbs);
	for (size_t i=0; i<m_ikLimbs.size(); i++)
	{
		s->Add(m_ikLimbs[i].limbName);
	}
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionPassengerIK);
