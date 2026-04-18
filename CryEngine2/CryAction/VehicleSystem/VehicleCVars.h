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

#ifndef __VEHICLECVARS_H__
#define __VEHICLECVARS_H__

#pragma once


class CVehicleCVars 
{
public:
     
  // debug draw   
  int v_debugdraw;    
  int v_draw_components;
  int v_draw_helpers;
  int v_draw_seats;
  int v_draw_tm;  
  int v_draw_passengers;  
  
  // dev vars
  int v_deformable;    
  int v_transitionAnimations;
  int v_autoDisable;
  int v_lights;
  int v_lights_enable_always;
	int v_lights_disable_time;
  int v_set_passenger_tm;
  int v_enable_lumberjacks;
  int v_disable_hull;    
  int v_damage;
  int v_ragdollPassengers;
  int v_goliathMode;
  int v_debugView;
  int v_debugCollisionDamage;
  int v_show_all;
  float v_treadUpdateTime;
  float v_tpvDist;
  float v_tpvHeight;
    
  // tweaking  
  float v_slipSlopeFront;
  float v_slipSlopeRear;
  float v_slipFrictionModFront;
  float v_slipFrictionModRear;
  float v_maxHeightBegin;
  float v_maxHeightEnd;     
  float v_enterDirRadius;

  int v_vehicle_quality;  
  int v_driverControlledMountedGuns;
  int v_independentMountedGuns;
    
	static __inline CVehicleCVars& Get()
	{
		assert (s_pThis != 0);
		return *s_pThis;
	}

private:
	friend class CVehicleSystem; // Our only creator

  CVehicleCVars(); // singleton stuff  
	~CVehicleCVars();
	CVehicleCVars(const CVehicleCVars&);
	CVehicleCVars& operator= (const CVehicleCVars&);

	static CVehicleCVars* s_pThis;  
};

ILINE const CVehicleCVars& VehicleCVars() { return CVehicleCVars::Get(); }


#endif // __VEHICLECVARS_H__
