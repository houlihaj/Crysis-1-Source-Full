/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script Binding for the Vehicle System

-------------------------------------------------------------------------
History:
- 05:10:2004   12:05 : Created by Mathieu Pinard

*************************************************************************/
#ifndef __SCRIPTBIND_VEHICLE_H__
#define __SCRIPTBIND_VEHICLE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IScriptSystem.h>
#include <ScriptHelpers.h>

struct IVehicleSystem;
struct IGameFramework;
class CVehicle;

// <title VehicleSystem>
// Syntax: VehicleSystem
class CScriptBind_Vehicle :
	public CScriptableBase
{
public:
	CScriptBind_Vehicle( ISystem *pSystem, IGameFramework *pGameFW );
	virtual ~CScriptBind_Vehicle();

	void Release() { delete this; };

	void AttachTo(IVehicle *pVehicle);
	CVehicle* GetVehicle(IFunctionHandler *pH);

	int Reset(IFunctionHandler *pH);

	int SetOwnerId(IFunctionHandler *pH, ScriptHandle ownerId);
	int GetOwnerId(IFunctionHandler *pH);

	int RefreshPhysicsGeometry(IFunctionHandler *pH, ScriptHandle partHandle, int slot, int physIndex);

	int GetVehiclePhysicsStatus(IFunctionHandler *pH, SmartScriptTable statusTable);	
	int SetPlayerToSit(IFunctionHandler *pH, ScriptHandle playerId, int flags);	
	
	int IsPartInsideRadius(IFunctionHandler *pH, int slot, Vec3 pos, float radius);
	int IsInsideRadius(IFunctionHandler *pH, Vec3 pos, float radius);
	int IsEmpty(IFunctionHandler *pH);

	int GetRepairableDamage(IFunctionHandler *pH);
	int IsRepairableDamage(IFunctionHandler* pH);

	int StartAbandonTimer(IFunctionHandler *pH);
	int KillAbandonTimer(IFunctionHandler *pH);
	int Destroy(IFunctionHandler *pH);

	int MultiplyWithEntityLocalTM(IFunctionHandler *pH, ScriptHandle entityHandle, Vec3 pos);
	int MultiplyWithWorldTM(IFunctionHandler *pH, Vec3 pos);
	int UpdateVehicleAnimation(IFunctionHandler *pH, ScriptHandle entity, int slot);
	int ResetSlotGeometry(IFunctionHandler* pH, int slot, const char* filename, const char* geometry);
  
	int AddSeat(IFunctionHandler* pH, SmartScriptTable paramsTable);

	int HasHelper(IFunctionHandler* pH, const char* name);
	int GetHelperPos(IFunctionHandler* pH, const char* name, bool isInVehicleSpace);
	int GetHelperDir(IFunctionHandler* pH, const char* name, bool isInVehicleSpace);
	int GetHelperWorldPos(IFunctionHandler* pH, const char* name);

	int SetExtensionActivation(IFunctionHandler* pH, const char *extension, bool bActivated);
	int SetExtensionParams(IFunctionHandler* pH, const char *extension, SmartScriptTable params);
	int GetExtensionParams(IFunctionHandler* pH, const char *extension, SmartScriptTable params);

	int SetMovement(IFunctionHandler *pH, char* movementClass, SmartScriptTable paramTable);
  int EnableMovement(IFunctionHandler *pH, bool enable);
  
  int DisableEngine(IFunctionHandler *pH, bool disable);

	int OnHit(IFunctionHandler* pH, ScriptHandle targetId, ScriptHandle shooterId, float damage, Vec3 position, float radius, char* pHitClass, bool explosion);
	
	int ProcessPassengerDamage(IFunctionHandler* pH, ScriptHandle passengerId, float actorHealth, float damage, const char* pDamageClass, bool explosion);
	int OnPassengerKilled(IFunctionHandler* pH, ScriptHandle passengerHandle);
  int IsDestroyed(IFunctionHandler* pH);
	int IsFlipped(IFunctionHandler* pH);
	int IsSubmerged(IFunctionHandler* pH);

	int IsUsable(IFunctionHandler* pH, ScriptHandle userHandle);
	int OnUsed(IFunctionHandler* pH, ScriptHandle userHandle, int index);

	int EnterVehicle(IFunctionHandler* pH, ScriptHandle actorHandle, int seatId, bool isAnimationEnabled);
	int ChangeSeat(IFunctionHandler* pH, ScriptHandle actorHandle, int seatId, bool isAnimationEnabled);
	int ExitVehicle(IFunctionHandler* pH, ScriptHandle actorHandle);

	int GetComponentDamageRatio(IFunctionHandler* pH, const char* pComponentName);
  int GetCollisionDamageThreshold(IFunctionHandler* pH);  
  int GetSelfCollisionMult(IFunctionHandler* pH, Vec3 velocity, Vec3 normal, int partId, ScriptHandle colliderId);
	int GetPlayerCollisionMult(IFunctionHandler* pH);
	int GetMovementDamageRatio(IFunctionHandler* pH);
	int SetMovementMode(IFunctionHandler* pH, int mode);
	int GetMovementType(IFunctionHandler* pH);

  int SetMovementSoundVolume(IFunctionHandler *pH, float volume);

	int SetAmmoCount(IFunctionHandler *pH, const char *name, int amount);
  
  int GetFrozenAmount(IFunctionHandler *pH);
	int SetFrozenAmount(IFunctionHandler *pH, float frost);

	int OpenAutomaticDoors(IFunctionHandler *pH);
	int CloseAutomaticDoors(IFunctionHandler *pH);
	int BlockAutomaticDoors(IFunctionHandler *pH, bool isBlocked);
	int ExtractGears(IFunctionHandler *pH);
	int RetractGears(IFunctionHandler *pH);

private:
	void RegisterGlobals();
	void RegisterMethods();
  	
	IGameFramework *m_pGameFW;  
	IVehicleSystem *m_pVehicleSystem;
};

#endif //__SCRIPTBIND_VEHICLE_H__
