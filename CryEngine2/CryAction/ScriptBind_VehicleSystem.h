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
#ifndef __SCRIPTBIND_VEHICLESYSTEM_H__
#define __SCRIPTBIND_VEHICLESYSTEM_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IScriptSystem.h>
#include <ScriptHelpers.h>

struct IVehicleSystem;
struct IGameFramework;
class CVehicleSystem;

// <title VehicleSystem>
// Syntax: VehicleSystem
class CScriptBind_VehicleSystem :
	public CScriptableBase
{
public:
	CScriptBind_VehicleSystem( ISystem *pSystem, CVehicleSystem* vehicleSystem );
	virtual ~CScriptBind_VehicleSystem();

	void Release() { delete this; };

	int LogSlots(IFunctionHandler *pH, char* vehicleName);  
  int GetVehicleImplementations(IFunctionHandler* pH);
  int GetOptionalScript(IFunctionHandler* pH, char* vehicleName);
  int SetTpvDistance(IFunctionHandler *pH, float distance);
  int SetTpvHeight(IFunctionHandler *pH, float height);  
  int ReloadSystem(IFunctionHandler *pH);

private:
	void LogChildsSlots(IEntity* pEntity);
	void LogSlotsForEntity(IEntity* pVehicleEntity);

	void RegisterGlobals();
	void RegisterMethods();
	
	CVehicleSystem *m_pVehicleSystem;
};

#endif //__SCRIPTBIND_VEHICLESYSTEM_H__
