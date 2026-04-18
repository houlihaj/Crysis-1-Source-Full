/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	VehicleSystem CVars
-------------------------------------------------------------------------
History:
- 02:01:2007  12:47 : Created by MichaelR 

*************************************************************************/

#include "StdAfx.h"
#include "VehicleCVars.h"
#include "VehicleSystem.h"
#include "Vehicle.h"


CVehicleCVars* CVehicleCVars::s_pThis = 0;

void OnDebugViewVarChanged(ICVar* pDebugViewVar);
void OnDebugDrawVarChanged(ICVar* pVar);
void OnDriverControlledGunsChanged(ICVar* pVar);
void CmdExitPlayer(IConsoleCmdArgs* pArgs);


//------------------------------------------------------------------------
CVehicleCVars::CVehicleCVars()
{
	assert (s_pThis == 0);
	s_pThis = this;
  
	IConsole *pConsole = gEnv->pConsole;

  // debug draw
  pConsole->RegisterString("v_debugVehicle", "", 0, "Vehicle entity name to use for debugging output");    
  pConsole->Register("v_draw_components", &v_draw_components, 0, VF_DUMPTODISK, "Enables/disables display of components and their damage count", OnDebugDrawVarChanged);
  pConsole->Register("v_draw_helpers", &v_draw_helpers, 0, 0, "Enables/disables display of vehicle helpers", OnDebugDrawVarChanged);
  pConsole->Register("v_draw_seats", &v_draw_seats, 0, 0, "Enables/disables display of seat positions", OnDebugDrawVarChanged);
  pConsole->Register("v_draw_tm", &v_draw_tm, 0, 0, "Enables/disables drawing of local part matrices");  
  pConsole->Register("v_draw_passengers", &v_draw_passengers, 0, VF_CHEAT, "draw passenger TMs set by VehicleSeat");
  pConsole->Register("v_debugdraw", &v_debugdraw, 0, VF_DUMPTODISK, 
    "Displays vehicle status info on HUD\n"
    "Values:\n"
    "1:  common stuff\n"
    "2:  vehicle particles\n"
    "3:  parts\n"
    "4:  views\n"          
    "6:  parts + partIds\n"
    "7:  parts + transformations and bboxes\n"
    "8:  component damage\n"    
    "10: vehicle editor\n"

    );
	    
  // dev vars
  pConsole->Register("v_deformable", &v_deformable, 0, 0, "Enables/disables DeformMorph calls on vehicle parts");    
  pConsole->Register("v_lights", &v_lights, 2, 0, "Controls vehicle lights. 0: disable all lights, 1: disable all dynamic lights, 2: enable dynamic lights only for local player, 3: enable all lights");
  pConsole->Register("v_lights_enable_always", &v_lights_enable_always, 0, VF_CHEAT, "Vehicle lights are always on (debugging)");    
	pConsole->Register("v_lights_disable_time", &v_lights_disable_time, 10.0f, 0, "How long after passenger exits seat before lights are turned off. Zero for immediate, negative disables auto-turn-off");
  pConsole->Register("v_autoDisable", &v_autoDisable, 1, VF_CHEAT, "Enables/disables vehicle autodisabling");
  pConsole->Register("v_set_passenger_tm", &v_set_passenger_tm, 1, VF_CHEAT, "enable/disable passenger entity tm update");
  pConsole->Register("v_enable_lumberjacks", &v_enable_lumberjacks, 0, VF_CHEAT, "Enable/disable physicalization of lumberjack parts");  
  pConsole->Register("v_disable_hull", &v_disable_hull, 0, 0, "Disable hull proxies");  
  pConsole->Register("v_treadUpdateTime", &v_treadUpdateTime, 0, 0, "delta time for tread UV update, 0 means always update");    
  pConsole->Register("v_damage", &v_damage, 1, VF_CHEAT, "Enables/disables vehicle damage processing");  
  pConsole->Register("v_show_all", &v_show_all, 0, VF_CHEAT, "");  
  pConsole->Register("v_transitionAnimations", &v_transitionAnimations, 1, VF_CHEAT, "Enables enter/exit transition animations for vehicles");
  pConsole->Register("v_ragdollPassengers", &v_ragdollPassengers, 0, VF_CHEAT, "Forces vehicle passenger to detach and ragdoll when they die inside of a vehicle");
  pConsole->Register("v_goliathMode", &v_goliathMode, 0, VF_CHEAT, "Makes all vehicles invincible");  
  pConsole->Register("v_debugView", &v_debugView, 0, VF_CHEAT|VF_DUMPTODISK, "Activate a 360 degree rotating third person camera instead of the camera usually available on the vehicle class", OnDebugViewVarChanged);  
  pConsole->Register("v_debugCollisionDamage", &v_debugCollisionDamage, 0, VF_CHEAT, "Enable debug output for vehicle collisions");  
  pConsole->AddCommand("v_tpvDist", "VehicleSystem.SetTpvDistance(%1)", VF_CHEAT, "Set default distance for vehicle thirdperson cam (0 means distance from vehicle class is used)");
  pConsole->AddCommand("v_tpvHeight", "VehicleSystem.SetTpvHeight(%1)", VF_CHEAT, "Set default height offset for vehicle thirdperson cam (0 means height from vehicle class is used, if present)");

  // for tweaking  
  pConsole->Register("v_maxHeightBegin", &v_maxHeightBegin, 0.0f, VF_CHEAT, "Indicate the beginning of the max height zone (0 means that this feature is disabled)");
  pConsole->Register("v_maxHeightEnd", &v_maxHeightEnd, 0.0f, VF_CHEAT, "Indicate the end of the max height zone (0 means that this feature is disabled)");  
  pConsole->Register("v_slipSlopeFront", &v_slipSlopeFront, 0.f, VF_CHEAT, "coefficient for slip friction slope calculation (front wheels)");
  pConsole->Register("v_slipSlopeRear", &v_slipSlopeRear, 0.f, VF_CHEAT, "coefficient for slip friction slope calculation (rear wheels)");  
  pConsole->Register("v_slipFrictionModFront", &v_slipFrictionModFront, 0.f, VF_CHEAT, "if non-zero, used as slip friction modifier (front wheels)");
  pConsole->Register("v_slipFrictionModRear", &v_slipFrictionModRear, 0.f, VF_CHEAT, "if non-zero, used as slip friction modifier (rear wheels)");
  pConsole->Register("v_enterDirRadius", &v_enterDirRadius, 7.0f, VF_CHEAT, "Maximum direction radius tolerated to enter with transition animation (AI only)");
  
  pConsole->AddCommand("v_reload_system", "VehicleSystem.ReloadVehicleSystem()", 0, "Reloads VehicleSystem script");  
  pConsole->AddCommand("v_exit_player", CmdExitPlayer, VF_CHEAT, "Makes the local player exit his current vehicle.");
  
  pConsole->Register("v_vehicle_quality", &v_vehicle_quality, 4, 0, "Geometry/Physics quality (1-lowspec, 4-highspec)");
  pConsole->Register("v_driverControlledMountedGuns", &v_driverControlledMountedGuns, 1, VF_CHEAT, "Specifies if the driver can control the vehicles mounted gun when driving without gunner.", OnDriverControlledGunsChanged);
  pConsole->Register("v_independentMountedGuns", &v_independentMountedGuns, 1, 0, "Whether mounted gunners operate their turret independently from the parent vehicle");
}

//------------------------------------------------------------------------
CVehicleCVars::~CVehicleCVars()
{
	assert (s_pThis != 0);
	s_pThis = 0;

	IConsole *pConsole = gEnv->pConsole;

	pConsole->RemoveCommand("v_tpvDist");
	pConsole->RemoveCommand("v_tpvHeight");
	pConsole->RemoveCommand("v_reload_system");  
	pConsole->RemoveCommand("v_exit_player");

	pConsole->UnregisterVariable("v_debugVehicle", true);  	
	pConsole->UnregisterVariable("v_draw_components", true);
	pConsole->UnregisterVariable("v_draw_helpers", true);
  pConsole->UnregisterVariable("v_draw_seats", true);
	pConsole->UnregisterVariable("v_draw_tm", true);  
	pConsole->UnregisterVariable("v_draw_passengers", true);
	pConsole->UnregisterVariable("v_debugdraw", true);

	pConsole->UnregisterVariable("v_deformable", true);    
	pConsole->UnregisterVariable("v_lights", true);
	pConsole->UnregisterVariable("v_lights_enable_always", true);    
	pConsole->UnregisterVariable("v_lights_disable_time", true);
	pConsole->UnregisterVariable("v_autoDisable", true);
	pConsole->UnregisterVariable("v_set_passenger_tm", true);
	pConsole->UnregisterVariable("v_enable_lumberjacks", true);  
	pConsole->UnregisterVariable("v_disable_hull", true);  
	pConsole->UnregisterVariable("v_treadUpdateTime", true);    
	pConsole->UnregisterVariable("v_damage", true);  
	pConsole->UnregisterVariable("v_transitionAnimations", true);
	pConsole->UnregisterVariable("v_ragdollPassengers", true);
	pConsole->UnregisterVariable("v_goliathMode", true);  
	pConsole->UnregisterVariable("v_debugView", true);  

	pConsole->UnregisterVariable("v_maxHeightBegin", true);
	pConsole->UnregisterVariable("v_maxHeightEnd", true);  
	pConsole->UnregisterVariable("v_slipSlopeFront", true);
	pConsole->UnregisterVariable("v_slipSlopeRear", true);  
	pConsole->UnregisterVariable("v_slipFrictionModFront", true);
	pConsole->UnregisterVariable("v_slipFrictionModRear", true);
	pConsole->UnregisterVariable("v_enterDirRadius", true);

  pConsole->UnregisterVariable("v_vehicle_quality", true);
  pConsole->UnregisterVariable("v_driverControlledMountedGuns", true);
}

//------------------------------------------------------------------------
void OnDebugViewVarChanged(ICVar* pDebugViewVar)
{ 
  if (IActor* pActor = CCryAction::GetCryAction()->GetClientActor())
  {
    if (IVehicle* pVehicle = pActor->GetLinkedVehicle())
    {
      SVehicleEventParams eventParams;
      eventParams.bParam = pDebugViewVar->GetIVal() == 1;

      pVehicle->BroadcastVehicleEvent(eVE_ToggleDebugView, eventParams);
    }
  }
}

//------------------------------------------------------------------------
void OnDebugDrawVarChanged(ICVar* pVar)
{ 
  if (pVar->GetIVal())
  {
    IVehicleSystem* pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
    
    IVehicleIteratorPtr pIter = pVehicleSystem->CreateVehicleIterator();    
    while (IVehicle* pVehicle = pIter->Next())
    {      
      pVehicle->NeedsUpdate();
    }     
  }  
}

//------------------------------------------------------------------------
void OnDriverControlledGunsChanged(ICVar* pVar)
{
  if (gEnv->bMultiplayer)
    return;

  if (IActor* pActor = CCryAction::GetCryAction()->GetClientActor())
  {
    if (IVehicle* pVehicle = pActor->GetLinkedVehicle())    
    {
      SVehicleEventParams params;
      params.bParam = (pVar->GetIVal() != 0);
      
      pVehicle->BroadcastVehicleEvent(eVE_ToggleDriverControlledGuns, params);
    }
     
  }
}

//------------------------------------------------------------------------
void CmdExitPlayer(IConsoleCmdArgs* pArgs)
{ 
  if (IActor* pActor = CCryAction::GetCryAction()->GetClientActor())
  {
    if (IVehicle* pVehicle = pActor->GetLinkedVehicle())    
      pVehicle->GetGameObject()->InvokeRMI(CVehicle::SvRequestLeave(), CVehicle::RequestLeaveParams(pActor->GetEntityId(), ZERO), eRMI_ToServer);    
  }
}

