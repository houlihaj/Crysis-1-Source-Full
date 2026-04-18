/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script Binding for the Vehicle Seat

-------------------------------------------------------------------------
History:
- 28:04:2004   17:02 : Created by Mathieu Pinard

*************************************************************************/
#ifndef __SCRIPTBIND_VEHICLESEAT_H__
#define __SCRIPTBIND_VEHICLESEAT_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IScriptSystem.h>
#include <ScriptHelpers.h>

struct IVehicleSystem;
struct IGameFramework;
class CVehicleSeat;

// <title VehicleSeat>
// Syntax: VehicleSeat
class CScriptBind_VehicleSeat :
	public CScriptableBase
{
public:

	CScriptBind_VehicleSeat( ISystem *pSystem, IGameFramework *pGameFW );
	virtual ~CScriptBind_VehicleSeat();

  void AttachTo(IVehicle *pVehicle, TVehicleSeatId seatId);
	void Release() { delete this; };

	CVehicleSeat* GetVehicleSeat(IFunctionHandler *pH);

	int Reset(IFunctionHandler *pH);

  int IsFree(IFunctionHandler *pH);
	int SetPassenger(IFunctionHandler* pH, ScriptHandle passengerHandle, bool isThirdPerson);
	int RemovePassenger(IFunctionHandler* pH);
	int IsDriver(IFunctionHandler* pH);
	int IsGunner(IFunctionHandler* pH);
	int IsLocked(IFunctionHandler* pH);

	int GetWeaponId(IFunctionHandler* pH, int weaponIndex);
	int GetWeaponCount(IFunctionHandler* pH);

	int SetAIWeapon(IFunctionHandler* pH, ScriptHandle weaponHandle);

private:

	void RegisterGlobals();
	void RegisterMethods();
	
	IVehicleSystem *m_pVehicleSystem;
};

#endif //__SCRIPTBIND_VEHICLESEAT_H__
