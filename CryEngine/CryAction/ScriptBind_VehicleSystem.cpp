/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script Binding for the Vehicle System

-------------------------------------------------------------------------
History:
- 26:04:2005   : Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "ICryAnimation.h"
#include "ActorSystem.h"
#include "IGameFramework.h"
#include "VehicleSystem.h"
#include "ScriptBind_VehicleSystem.h"
#include "VehicleSystem/VehicleViewThirdPerson.h"

//------------------------------------------------------------------------
CScriptBind_VehicleSystem::CScriptBind_VehicleSystem( ISystem *pSystem, CVehicleSystem* vehicleSystem )
{	
	m_pVehicleSystem = vehicleSystem;

	Init( pSystem->GetIScriptSystem(), pSystem );
	SetGlobalName("Vehicle");

	RegisterGlobals();
	RegisterMethods();

  pSystem->GetIScriptSystem()->ExecuteFile("Scripts/Entities/Vehicles/VehicleSystem.lua");
}

//------------------------------------------------------------------------
CScriptBind_VehicleSystem::~CScriptBind_VehicleSystem()
{
}

//------------------------------------------------------------------------
void CScriptBind_VehicleSystem::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptBind_VehicleSystem::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_VehicleSystem::

	SCRIPT_REG_TEMPLFUNC(LogSlots, "vehicleName");  
  SCRIPT_REG_FUNC(GetVehicleImplementations);
  SCRIPT_REG_TEMPLFUNC(GetOptionalScript, "vehicleName");
  SCRIPT_REG_TEMPLFUNC(SetTpvDistance, "distance");
  SCRIPT_REG_TEMPLFUNC(SetTpvHeight, "height");
  SCRIPT_REG_TEMPLFUNC(ReloadSystem, "");
}


//------------------------------------------------------------------------
void CScriptBind_VehicleSystem::LogSlotsForEntity(IEntity* pVehicleEntity)
{
	for (int s = 0; s < pVehicleEntity->GetSlotCount(); s++)
	{
		if (pVehicleEntity->IsSlotValid(s))
		{
			if (pVehicleEntity->GetStatObj(s))
			{
				IStatObj* pStatObj = pVehicleEntity->GetStatObj(s);
				Vec3 worldPos = pVehicleEntity->GetWorldPos();
				GameWarning("- slot %d: statobj   - filename=<%s>, geometry=<%s>, worldpos={%f,%f,%f}", s, pStatObj->GetFilePath(), pStatObj->GetGeoName(), worldPos.x, worldPos.y, worldPos.z);
			}
			else if (pVehicleEntity->GetCharacter(s))
			{
				ICharacterInstance* pCharInst = pVehicleEntity->GetCharacter(s);
				GameWarning("- slot %d: character - filename=<%s>", s, pCharInst->GetFilePath());
			}
			else
			{
				GameWarning("- slot %d: unknown content", s);
			}
		}
	}
}

//------------------------------------------------------------------------
void CScriptBind_VehicleSystem::LogChildsSlots(IEntity* pEntity)
{
	assert(pEntity);

	for (int i = 0; i < pEntity->GetChildCount(); i++)
	{
		IEntity* pChild = pEntity->GetChild(i);
		if (pChild)
		{
			GameWarning("* child %d: name=<%s>", i, pChild->GetName());
			LogSlotsForEntity(pChild);

			if (pChild->GetChildCount() > 0)
			{
				LogChildsSlots(pChild);
			}			
		}
	}
}

//------------------------------------------------------------------------
int CScriptBind_VehicleSystem::LogSlots(IFunctionHandler *pH, char* vehicleName)
{
	IEntity* pVehicleEntity = gEnv->pEntitySystem->FindEntityByName(vehicleName);
	if (!pVehicleEntity)
	{
		GameWarning("Error: Vehicle <%s> not found", vehicleName);
		return pH->EndFunction();
	}

	IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(pVehicleEntity->GetId());
	if (!pVehicle)
	{
		GameWarning("Error: Entity <%s> is not a vehicle", vehicleName);
		return pH->EndFunction();
	}

	GameWarning("Overview of the slot usage for vehicle <%s>", vehicleName);

	LogSlotsForEntity(pVehicleEntity);
	GameWarning(" ");
	LogChildsSlots(pVehicleEntity);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_VehicleSystem::GetVehicleImplementations(IFunctionHandler* pH)
{ 
  SmartScriptTable tableImpls(m_pSS->CreateTable());
  SVehicleImpls impls;
  
  m_pVehicleSystem->GetVehicleImplementations(impls);

  //CryLog("Scriptbind got %i vehicles", impls.Count());
  for (int i=0; i<impls.Count(); ++i)
  { 
    tableImpls->SetAt(i+1, impls.GetAt(i).c_str());
  }
  return pH->EndFunction(tableImpls);
}


//------------------------------------------------------------------------
int CScriptBind_VehicleSystem::GetOptionalScript(IFunctionHandler *pH, char* vehicleName)
{  
  char scriptName[1024] = {0};
  
  if (m_pVehicleSystem->GetOptionalScript(vehicleName, scriptName, sizeof(scriptName)))
    return pH->EndFunction(scriptName);

  return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_VehicleSystem::SetTpvDistance(IFunctionHandler *pH, float distance)
{
  CVehicleViewThirdPerson::SetDefaultDistance(distance);
  
  return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_VehicleSystem::SetTpvHeight(IFunctionHandler *pH, float height)
{
  CVehicleViewThirdPerson::SetDefaultHeight(height);

  return pH->EndFunction();
}



//------------------------------------------------------------------------
int CScriptBind_VehicleSystem::ReloadSystem(IFunctionHandler *pH)
{
  m_pVehicleSystem->ReloadSystem();

  return pH->EndFunction();
}