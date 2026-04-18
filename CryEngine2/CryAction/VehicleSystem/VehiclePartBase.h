/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a base class for vehicle parts

-------------------------------------------------------------------------
History:
- 23:08:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEPARTBASE_H__
#define __VEHICLEPARTBASE_H__

#include <list>
#include "IVehicleSystem.h"
#include "Network/NetActionSync.h"
#include "InterpolationHelpers.h"
#include "VehicleCVars.h"
#include "Vehicle.h"


class CVehiclePartSubPart;
class CVehiclePartSubPartEntity;
class CVehicle;
class CVehicleComponent;
class CVehicleSeatActionRotateTurret;

#define VEHICLE_MASS_COLLTYPE_HEAVY 1500.f
#define MAX_OPTIONAL_PARTS 5
#define GEOMETRY_DESTROYED_SUFFIX "_damaged"


class CVehiclePartBase
	: public IVehiclePart
{
  IMPLEMENT_VEHICLEOBJECT;
public:
  
	CVehiclePartBase();
	virtual ~CVehiclePartBase();

	// IVehiclePart
  virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo);  
	virtual void InitHelpers(IVehicle* pVehicle, const SmartScriptTable &table);
	virtual bool InitRotation(IVehicle* pVehicle, const SmartScriptTable &table);  
	virtual void PostInit();
	virtual void Reset();
	virtual void Release();

	virtual const string& GetName() { return m_name; }
	virtual IVehiclePart* GetParent(bool root=false);
	virtual IEntity* GetEntity();

  virtual void InvalidateTM(bool invalidate){}
  virtual void AddChildPart(IVehiclePart* pPart) { m_children.push_back((CVehiclePartBase*)pPart); }
  
	virtual void OnEvent(const SVehiclePartEvent& event);

	virtual bool ChangeState(EVehiclePartState state, int flags=0);
  virtual EVehiclePartState GetState() const { return m_state; }
	
  virtual void SetMaterial(IMaterial* pMaterial);
	virtual void Physicalize() {}
  virtual void SetMoveable() {}
  virtual bool GetRotationLimits(int axis, float& min, float& max);
  	
	virtual const Matrix34& GetLocalTM(bool relativeToParentPart);
	virtual const Matrix34& GetWorldTM();
	virtual void SetLocalTM(const Matrix34& localTM);
	virtual const AABB& GetLocalBounds();

  // set & get baseTM. for standard parts, this just forwards to LocalTM
  virtual const Matrix34& GetLocalBaseTM() { return GetLocalTM(true); }
  virtual void SetLocalBaseTM(const Matrix34& tm) { SetLocalTM(tm); }

  virtual void ResetLocalTM(bool recursive);

	virtual const Matrix34& GetLocalInitialTM() { return GetLocalTM(true); }
  
	virtual void OnHit(Vec3 localPos, float radius, float strength = 1.0f) {}

	virtual void Update(const float deltaTime);
	virtual void Serialize(TSerialize ser, unsigned aspects);
	virtual void PostSerialize() {}

	virtual void RegisterSerializer(IGameObjectExtension* gameObjectExt) {}
  virtual int GetType(){ return m_typeId; }    

  virtual IVehicleWheel* GetIWheel() { return NULL; }

	virtual const Vec3& GetDetachBaseForce() { return m_detachBaseForce; }
	virtual float GetMass() { return m_mass; }
  virtual int GetPhysId() { return m_physId; }
  virtual int GetSlot() { return m_slot; }  
	// ~IVehiclePart

  virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params){}
  
	virtual IStatObj* GetSubGeometry(CVehiclePartBase* pPart, EVehiclePartState state, Matrix34& position, bool removeFromParent);
	virtual IStatObj* GetStatObj(EVehiclePartState state = eVGS_Default) { return NULL; }
	virtual bool SetStatObj(IStatObj* pStatObj) {  return false; }
  virtual void GetGeometryName(EVehiclePartState state, string& name);
	
  virtual IMaterial* GetMaterial();
	
	bool IsPhysicalized() { return m_isPhysicalized; }
	void SetSeat(CVehicleSeat *pSeat) { m_pSeat = pSeat; };
	CVehicleSeat *GetSeat() { return m_pSeat; };
  
  const Matrix34& LocalToVehicleTM(const Matrix34& localTM);
  EVehiclePartState GetMaxState();
  float GetDetachProbability() { return m_detachProbability; }
  void InitComponent();

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		GetBaseMemoryStatistics(s);
	}

  static const char * m_geometryDestroyedSuffix;
public:

	enum EVehiclePartBaseEvent
	{
		eVPE_ParentPartUpdated = eVPE_OtherEvents,
	};

	typedef std::list <CVehiclePartBase*> TVehiclePartBaseList;

	virtual void Hide(bool hide);

protected:
    
	void ParsePhysicsParams(SEntityPhysicalizeParams& physicalizeParams, const SmartScriptTable &table);
	  
	float UpdateRotation(float frameTime);
  bool ClampToRotationLimits(Ang3& angles);
  
	bool InitRotationSounds(const SmartScriptTable &rotationParams);
  void UpdateRotationSound(float deltaTime, float updatedAngle);

  void CheckColltypeHeavy(int partid);
  void SetFlags(int partid, int flags, bool add);
  void SetFlagsCollider(int partid, int flagsCollider);
	void CloneMaterial();
	EntityId SpawnEntity();
  int GetCGASlot(int jointId);
    
  float GetDamageSpeedMul();
  virtual EVehiclePartState GetStateForDamageRatio(float ratio);
  
	void GetBaseMemoryStatistics(ICrySizer * s);
  
protected:
  int m_typeId;

	CVehicle* m_pVehicle;
	
	int m_slot;
	AABB m_bounds;
  	
	string m_name;	
  EVehiclePartState m_state;
  
  const char* m_pComponentName;    
  IVehiclePart* m_pParentPart;
  
  typedef std::vector<CVehiclePartBase*> TVehicleChildParts;
  TVehicleChildParts m_children;
	
	string m_helperPosName;
	Vec3 m_position;
  
	int m_hideCount;	// if 0, not hidden. Allows multiple hide/unhide requests simultaneously.
  int m_useOption;  	
		
	bool m_isPhysicalized;
	bool m_disableCollision;	
  int m_physId;
	float m_mass;
  float m_density;
	
	CVehicleSeat *m_pSeat;  
	
	float m_actionPitch, m_currentPitch, m_lastPitch;
	float m_actionYaw, m_currentYaw, m_lastYaw;
	float m_yawSpeed;
  float m_yawAccel;
	float m_yawMin, m_yawMax;
	float m_pitchSpeed;
  float m_pitchAccel;
	float m_pitchMin, m_pitchMax;
	
  float m_rotationStopped;
  float m_relSpeed;
  
  float m_rotWorldSpace;
  Quat m_prevWorldQuat;

  TVehicleSoundEventId m_turnSoundId;
  TVehicleSoundEventId m_damageSoundId;

	float m_damageRatio;		
  Matrix34 m_prevBaseTM;

	// used to fade in/out opacity

	enum EVehiclePartHide
	{
		eVPH_NoFade = 0,
		eVPH_FadeIn,
		eVPH_FadeOut,
	};

	EVehiclePartHide m_hideMode;
	float m_hideTimeMax;
	float m_hideTimeCount;

	Vec3 m_detachBaseForce;
  float m_detachProbability;

	int m_users;
    
	friend class CVehiclePartSubPart;
	friend class CVehiclePartSubPartEntity;
	friend class CVehiclePartAnimatedJoint;
	friend class CVehicleSeatActionRotateTurret;

	InterpolatedQuat m_orientation;

  bool m_rotationChanged;		  
  bool m_isRotationBlocked;
  bool m_wasMaterialCloned;

	IVehicleHelper* m_rotTestHelpers[2];
	string m_rotTestHelperNames[2];
	float m_rotTestRadius;
};

namespace
{
  ILINE bool IsDebugParts() 
  { 
    return VehicleCVars().v_debugdraw == eVDB_Parts; 
  } 
}

#endif
