/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a seat action to rotate a turret

-------------------------------------------------------------------------
History:
- 14:12:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLESEATACTIONROTATETURRET_H__
#define __VEHICLESEATACTIONROTATETURRET_H__

class CVehiclePartBase;

class CVehicleSeatActionRotateTurret
	: public IVehicleSeatAction
{
	IMPLEMENT_VEHICLEOBJECT
public:

	// IVehicleSeatAction
	virtual bool Init(IVehicle* pVehicle, TVehicleSeatId seatId, const SmartScriptTable &table);
	virtual void Reset();
	virtual void Release() { delete this; }

	virtual void StartUsing(EntityId passengerId);
	virtual void StopUsing();
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);
	
	virtual void Serialize(TSerialize ser, unsigned aspects);
  virtual void PostSerialize(){}
	virtual void Update(const float deltaTime);

	void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }
	// ~IVehicleSeatAction

	void SetAimGoal(Vec3 aimPos, int priority = 0);
  const Vec3& GetAimGoal();

  virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params){}

protected:

	IVehicle			*m_pVehicle;
	IEntity				*m_pUserEntity;
  TVehicleSeatId m_seatId;
	
	CVehiclePartBase* m_pPitchPart;
	CVehiclePartBase* m_pYawPart;

	Vec3 m_aimGoal;
	int m_aimGoalPriority;
	float m_actionYaw;
	float m_actionPitch;

	bool m_hasReceivedAction;

	friend class CVehiclePartBase;
};

#endif
