/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements the base of the vehicle damages

-------------------------------------------------------------------------
History:
- 23:02:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleDamages.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehicleComponent.h"
#include "IActorSystem.h"
#include "CryAction.h"

//------------------------------------------------------------------------
void CVehicleDamages::InitDamages(CVehicle* pVehicle, const SmartScriptTable& table)
{
	m_pVehicle = pVehicle;
  
	SmartScriptTable damagesTable;
	if (table->GetValue("Damages", damagesTable))
	{
		SmartScriptTable damagesGroupTable;
		if (damagesTable->GetValue("DamagesGroups", damagesGroupTable))
		{
			m_damagesGroups.reserve(damagesGroupTable->Count());

			IScriptTable::Iterator groupIte = damagesGroupTable->BeginIteration();

			while (damagesGroupTable->MoveNext(groupIte))
			{
				if (groupIte.value.type == ANY_TTABLE)
				{
					IScriptTable* pGroupTable = groupIte.value.table;
					assert(pGroupTable);

					CVehicleDamagesGroup* pDamageGroup = new CVehicleDamagesGroup;
					if (pDamageGroup->Init(pVehicle, pGroupTable))
						m_damagesGroups.push_back(pDamageGroup);
					else
						delete pDamageGroup;
				}
			}

			damagesGroupTable->EndIteration(groupIte);
		}

		damagesTable->GetValue("submergedRatioMax", m_damageParams.submergedRatioMax);
		damagesTable->GetValue("submergedDamageMult", m_damageParams.submergedDamageMult);

    damagesTable->GetValue("collDamageThreshold", m_damageParams.collisionDamageThreshold);    
    damagesTable->GetValue("groundCollisionMinMult", m_damageParams.groundCollisionMinMult);    
    damagesTable->GetValue("groundCollisionMaxMult", m_damageParams.groundCollisionMaxMult);    
    damagesTable->GetValue("groundCollisionMinSpeed", m_damageParams.groundCollisionMinSpeed);    
    damagesTable->GetValue("groundCollisionMaxSpeed", m_damageParams.groundCollisionMaxSpeed);  
		damagesTable->GetValue("vehicleCollisionDestructionSpeed", m_damageParams.vehicleCollisionDestructionSpeed);
		damagesTable->GetValue("playerCollisionMult", m_damageParams.playerCollisionMult);

		ParseDamageMultipliers(m_damageMultipliers, damagesTable);
	}
}

//------------------------------------------------------------------------
void CVehicleDamages::ParseDamageMultipliers(TDamageMultipliers& multipliers, const SmartScriptTable& table)
{
	SmartScriptTable damageMultipliersTable;
	if (table->GetValue("DamageMultipliers", damageMultipliersTable))
	{
		IScriptTable::Iterator multIte = damageMultipliersTable->BeginIteration();

		while (damageMultipliersTable->MoveNext(multIte))
		{
			if (multIte.value.GetVarType() == svtObject)
			{
				char* pDamageType;
				if (multIte.value.table->GetValue("damageType", pDamageType))
				{
					SDamageMultiplier mult;

					if (multIte.value.table->GetValue("multiplier", mult.mult) && mult.mult >= 0.0f)
          {
            multIte.value.table->GetValue("splash", mult.splash);
						multipliers.insert(TDamageMultipliers::value_type(pDamageType, mult));
          }
				}				
			}
		}

		damageMultipliersTable->EndIteration(multIte);
	}
}

//------------------------------------------------------------------------
void CVehicleDamages::ReleaseDamages()
{
	for (TVehicleDamagesGroupVector::iterator ite = m_damagesGroups.begin(); ite != m_damagesGroups.end(); ++ite)
	{
		CVehicleDamagesGroup* pDamageGroup = *ite;
		pDamageGroup->Release();
	}
}

//------------------------------------------------------------------------
void CVehicleDamages::ResetDamages()
{
	for (TVehicleDamagesGroupVector::iterator ite = m_damagesGroups.begin(); ite != m_damagesGroups.end(); ++ite)
	{
		CVehicleDamagesGroup* pDamageGroup = *ite;
		pDamageGroup->Reset();
	}
}

//------------------------------------------------------------------------
void CVehicleDamages::UpdateDamages(float frameTime)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

	for (TVehicleDamagesGroupVector::iterator ite = m_damagesGroups.begin(), end = m_damagesGroups.end(); ite != end; ++ite)
	{
		CVehicleDamagesGroup* pDamageGroup = *ite;
		pDamageGroup->Update(frameTime);
	}
}

//------------------------------------------------------------------------
bool CVehicleDamages::ProcessHit(float& damage, const char* hitClass, bool splash)
{
	TDamageMultipliers::const_iterator ite = m_damageMultipliers.find(CONST_TEMP_STRING(hitClass));
	if (ite != m_damageMultipliers.end())
	{
    const SDamageMultiplier& mult = ite->second;
    damage *= mult.mult * (splash ? mult.splash : 1.f);    
    
    if (VehicleCVars().v_debugdraw == eVDB_Damage)
			CryLog("Whole vehicle: mults for %s: %.2f, splash %.2f", hitClass, mult.mult, mult.splash);
    
    return true;
	}  

  return false;
}

//------------------------------------------------------------------------
CVehicleDamagesGroup* CVehicleDamages::GetDamagesGroup(const char* groupName)
{
	for (TVehicleDamagesGroupVector::iterator ite = m_damagesGroups.begin(); ite != m_damagesGroups.end(); ++ite)
	{
		CVehicleDamagesGroup* pDamageGroup = *ite;
		if (!strcmp(pDamageGroup->GetName().c_str(), groupName))
		{
			return pDamageGroup;
		}
	}

	return NULL;
}



