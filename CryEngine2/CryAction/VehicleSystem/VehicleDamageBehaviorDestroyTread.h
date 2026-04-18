/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2008.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a damage behavior which destroys a tread part

-------------------------------------------------------------------------
History:
- 07.05.2008: Created by Steven Humphreys

*************************************************************************/
#ifndef __VEHICLEDAMAGEBEHAVIORMOVEMENTDESTROYTREAD_H__
#define __VEHICLEDAMAGEBEHAVIORMOVEMENTDESTROYTREAD_H__

class CVehicle;
struct IParticleEffect;

class CVehicleDamageBehaviorDestroyTread
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorDestroyTread();
	virtual ~CVehicleDamageBehaviorDestroyTread();

	virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);
	virtual bool Init(IVehicle* pVehicle, const string& partName);
	virtual void Reset();
	virtual void Release() { delete this; }

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams);
	
	virtual void Serialize(TSerialize ser, unsigned aspects);
	virtual void Update(const float deltaTime) {}

  virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params){}

	virtual void GetMemoryStatistics(ICrySizer * s);

protected:

	IVehicle* m_pVehicle;
	string m_partName;
};

#endif
