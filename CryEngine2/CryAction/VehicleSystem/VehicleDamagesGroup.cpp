/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements the base of the vehicle damages group

-------------------------------------------------------------------------
History:
- 23:02:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "VehicleDamagesGroup.h"
#include "Vehicle.h"
#include "VehicleDamageBehaviorDestroy.h"
#include "VehicleDamageBehaviorDetachPart.h"
#include "VehiclePartAnimatedJoint.h"

//------------------------------------------------------------------------
CVehicleDamagesGroup::~CVehicleDamagesGroup()
{
	for (TDamagesSubGroupVector::iterator ite = m_damageSubGroups.begin(); 
		ite != m_damageSubGroups.end(); ++ite)
	{
		SDamagesSubGroup& subGroup = *ite;
		TVehicleDamageBehaviorVector& damageBehaviors = subGroup.m_damageBehaviors;

		for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
		{
			IVehicleDamageBehavior* pBehavior = *behaviorIte;
			pBehavior->Release();
		}
	}

	m_delayedSubGroups.clear();
}

//------------------------------------------------------------------------
bool CVehicleDamagesGroup::Init(CVehicle* pVehicle, const SmartScriptTable& table)
{
	m_pVehicle = pVehicle;

	char* pName;
	if (!table->GetValue("name", pName))
		return false;

	m_name = pName;
  m_damageSubGroups.clear();

	return ParseDamagesGroup(table);
}

//------------------------------------------------------------------------
bool CVehicleDamagesGroup::ParseDamagesGroup(const SmartScriptTable& table)
{
	bool bDebrisActive = true;

	static ICVar *pVar = gEnv->pConsole->GetCVar("sys_spec_Physics");
	if( pVar && pVar->GetRealIVal() == 1 )
		bDebrisActive = false;

	char* pTemplateToUse = NULL;
	if (table->GetValue("useTemplate", pTemplateToUse) && pTemplateToUse[0])
	{
		IVehicleSystem* pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
		assert(pVehicleSystem);

		if (IVehicleDamagesTemplateRegistry* pDamageTemplReg = pVehicleSystem->GetDamagesTemplateRegistry())
			pDamageTemplReg->UseTemplate(pTemplateToUse, this);
	}

	SmartScriptTable DamagesSubGroupsTable;
	if (table->GetValue("DamagesSubGroups", DamagesSubGroupsTable))
	{
		m_damageSubGroups.reserve(DamagesSubGroupsTable->Count());

		IScriptTable::Iterator subGroupIte = DamagesSubGroupsTable->BeginIteration();

		while (DamagesSubGroupsTable->MoveNext(subGroupIte))
		{
			if (subGroupIte.value.type == ANY_TTABLE)
			{
				IScriptTable* pGroupTable = subGroupIte.value.table;
				assert(pGroupTable);

				m_damageSubGroups.resize(m_damageSubGroups.size() + 1);

				SDamagesSubGroup& subGroup = m_damageSubGroups.back();

				subGroup.id = m_damageSubGroups.size()-1;

				subGroup.m_isAlreadyInProcess = false;

				if (!pGroupTable->GetValue("delay", subGroup.m_delay))
					subGroup.m_delay = 0.0f;

				if (!pGroupTable->GetValue("randomness", subGroup.m_randomness))
					subGroup.m_randomness = 0.0f;

				SmartScriptTable damageBehaviorsTable;
				if (pGroupTable->GetValue("DamageBehaviors", damageBehaviorsTable))
				{
					subGroup.m_damageBehaviors.reserve(damageBehaviorsTable->Count());

					IScriptTable::Iterator behaviorsIte = damageBehaviorsTable->BeginIteration();

					while (damageBehaviorsTable->MoveNext(behaviorsIte))
					{
						if (behaviorsIte.value.type == ANY_TTABLE)
						{
							IScriptTable* pBehaviorTable = behaviorsIte.value.table;
							assert(pBehaviorTable);

							if (IVehicleDamageBehavior* pDamageBehavior = ParseDamageBehavior(pBehaviorTable))
							{
								subGroup.m_damageBehaviors.push_back(pDamageBehavior);

								if (CAST_VEHICLEOBJECT(CVehicleDamageBehaviorDestroy, pDamageBehavior))
								{
									TVehiclePartVector& parts = m_pVehicle->GetParts();
									for (TVehiclePartVector::iterator ite = parts.begin(); ite != parts.end(); ++ite)
									{
										IVehiclePart* pPart = ite->second;
										if (CVehiclePartAnimatedJoint* pAnimJoint = CAST_VEHICLEOBJECT(CVehiclePartAnimatedJoint, pPart))
										{
											if (pAnimJoint->IsPhysicalized() && !pAnimJoint->GetDetachBaseForce().IsZero())
											{
												CVehicleDamageBehaviorDetachPart* pDetachBehavior = new CVehicleDamageBehaviorDetachPart;
												pDetachBehavior->Init(m_pVehicle, pAnimJoint->GetName(), bDebrisActive);

												subGroup.m_damageBehaviors.push_back(pDetachBehavior);
											}
										}
									}
								}
							}
						}
					}

					damageBehaviorsTable->EndIteration(behaviorsIte);
				}
			}
		}

		DamagesSubGroupsTable->EndIteration(subGroupIte);
	}

	return true;
}

//------------------------------------------------------------------------
IVehicleDamageBehavior* CVehicleDamagesGroup::ParseDamageBehavior(const SmartScriptTable& table)
{
	char* pClassName;
	if (table->GetValue("class", pClassName))
	{
		IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();

		if (IVehicleDamageBehavior* pDamageBehavior = pVehicleSystem->CreateVehicleDamageBehavior(pClassName))
		{
			if (pDamageBehavior->Init((IVehicle*) m_pVehicle, table))
				return pDamageBehavior;
			else
			pDamageBehavior->Release();
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehicleDamagesGroup::Reset()
{
	for (TDamagesSubGroupVector::iterator ite = m_damageSubGroups.begin(); 
		ite != m_damageSubGroups.end(); ++ite)
	{
		SDamagesSubGroup& subGroup = *ite;
		TVehicleDamageBehaviorVector& damageBehaviors = subGroup.m_damageBehaviors;

		for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
		{
			IVehicleDamageBehavior* pBehavior = *behaviorIte;
			pBehavior->Reset();
		}

		subGroup.m_isAlreadyInProcess = false;
	}

	m_delayedSubGroups.clear();  
}

//------------------------------------------------------------------------
void CVehicleDamagesGroup::Serialize(TSerialize ser, unsigned int aspects)
{
	ser.BeginGroup("SubGroups");
	for (TDamagesSubGroupVector::iterator ite = m_damageSubGroups.begin(); 
		ite != m_damageSubGroups.end(); ++ite)
	{
		ser.BeginGroup("SubGroup");

		SDamagesSubGroup& subGroup = *ite;
		TVehicleDamageBehaviorVector& damageBehaviors = subGroup.m_damageBehaviors;

		for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
		{
			IVehicleDamageBehavior* pBehavior = *behaviorIte;
			ser.BeginGroup("Behavior");
			pBehavior->Serialize(ser, aspects);
			ser.EndGroup();
		}
		ser.EndGroup();
	}
	ser.EndGroup();

	int size = m_delayedSubGroups.size();
	ser.Value("DelayedSubGroupEntries", size);
	if(ser.IsWriting())
	{
		for (TDelayedDamagesSubGroupList::iterator ite = m_delayedSubGroups.begin(); ite != m_delayedSubGroups.end(); ++ite)
		{
			ser.BeginGroup("SubGroup");
			SDelayedDamagesSubGroupInfo& delayedInfo = *ite;
			ser.Value("delayedInfoId", delayedInfo.subGroupId);
			ser.Value("delayedInfoDelay", delayedInfo.delay);
			delayedInfo.behaviorParams.Serialize(ser, m_pVehicle);
			ser.EndGroup();
		}
	}
	else if(ser.IsReading())
	{
		m_delayedSubGroups.clear();  
		for(int i = 0; i < size; ++i)
		{	
			ser.BeginGroup("SubGroup");
			SDelayedDamagesSubGroupInfo delayInfo;
			ser.Value("delayedInfoId", delayInfo.subGroupId);
			ser.Value("delayedInfoDelay", delayInfo.delay);	
			delayInfo.behaviorParams.Serialize(ser, m_pVehicle);
			ser.EndGroup();
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamagesGroup::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
	if (m_pVehicle->IsDestroyed() || event == eVDBE_VehicleDestroyed)
		return;

  if (event == eVDBE_Repair)
  {
    m_delayedSubGroups.clear();        
  }
	
  for (TDamagesSubGroupVector::iterator subGroupIte = m_damageSubGroups.begin(); subGroupIte != m_damageSubGroups.end(); ++subGroupIte)
	{
    SDamagesSubGroup& subGroup = *subGroupIte;    
		TVehicleDamageBehaviorVector& damageBehaviors = subGroup.m_damageBehaviors;

		if (!subGroup.m_isAlreadyInProcess && subGroup.m_delay > 0.f && event != eVDBE_Repair)
		{
			m_delayedSubGroups.resize(m_delayedSubGroups.size() + 1);
			subGroup.m_isAlreadyInProcess = true;

			SDelayedDamagesSubGroupInfo& delayedSubGroupInfo = m_delayedSubGroups.back();

			delayedSubGroupInfo.delay = subGroup.m_delay;
			delayedSubGroupInfo.behaviorParams = behaviorParams;
			delayedSubGroupInfo.subGroupId = subGroup.id;
		}
		else
		{	
			SVehicleDamageBehaviorEventParams params(behaviorParams);
			params.randomness = subGroup.m_randomness;

			for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
			{
				IVehicleDamageBehavior* pBehavior = *behaviorIte;
				pBehavior->OnDamageEvent(event, params);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamagesGroup::Update(float frameTime)
{
  FUNCTION_PROFILER( gEnv->pSystem, PROFILE_GAME );
  
  for (TDelayedDamagesSubGroupList::iterator ite = m_delayedSubGroups.begin(); ite != m_delayedSubGroups.end(); ++ite)
	{
		SDelayedDamagesSubGroupInfo& delayedInfo = *ite;
		delayedInfo.delay -= frameTime;

		if (delayedInfo.delay <= 0.0f)
		{
			TDamagesSubGroupId id = delayedInfo.subGroupId;
			SDamagesSubGroup* pSubGroup = &m_damageSubGroups[id];
			delayedInfo.behaviorParams.randomness = pSubGroup->m_randomness;
			TVehicleDamageBehaviorVector& damageBehaviors =  pSubGroup->m_damageBehaviors;

			for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
			{
				IVehicleDamageBehavior* pBehavior = *behaviorIte;
				pBehavior->OnDamageEvent(eVDBE_ComponentDestroyed, delayedInfo.behaviorParams);
			}

      m_delayedSubGroups.erase(ite--);
		}        
	}

  if (!m_delayedSubGroups.empty())
    m_pVehicle->NeedsUpdate();
}

//------------------------------------------------------------------------
bool CVehicleDamagesGroup::IsPotentiallyFatal()
{
	for (TDamagesSubGroupVector::iterator subGroupIte = m_damageSubGroups.begin(); subGroupIte != m_damageSubGroups.end(); ++subGroupIte)
	{
		SDamagesSubGroup& subGroup = *subGroupIte;
		TVehicleDamageBehaviorVector& damageBehaviors = subGroup.m_damageBehaviors;

		for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
		{
			//IVehicleDamageBehavior* pBehavio
			if (CVehicleDamageBehaviorDestroy* pBehaviorDestroy = 
				CAST_VEHICLEOBJECT(CVehicleDamageBehaviorDestroy, (*behaviorIte)))
			{
				return true;
			}
		}
	}

	return false;
}
