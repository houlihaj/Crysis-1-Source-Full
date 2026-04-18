//////////////////////////////////////////////////////////////////////
//
//	Wheeled Vehicle Entity header
//	
//	File: wheeledvehicleentity.h
//	Description : CWheeledVehicleEntity class declaration
//
//	History:
//	-:Created by Anton Knyazev
//
//////////////////////////////////////////////////////////////////////

#ifndef wheeledvehicleentity_h
#define wheeledvehicleentity_h
#pragma once

#include "trianglecache.h"

#define NUM_CACHED_WHEELS 8

struct suspension_point {
	Vec3 pos;
	quaternionf q;
	float scale;
	Vec3 BBox[2];

	int bDriving; // if the corresponding wheel a driving wheel
	float Tscale;
	int iAxle;
	Vec3 pt; // the uppermost suspension point in car frame
	float fullen; // unconstrained length
	float kStiffness; // stiffness coefficient
	float kDamping,kDamping0; // damping coefficient
	float len0; // initial length in model
	float Mpt; // hull "mass" at suspension upper point along suspension direction
	quaternionf q0;	// used to calculate geometry transformation from wheel transformation
	Vec3 pos0,ptc0; // ...
	float Iinv;
	float minFriction,maxFriction;
  float kLatFriction;
	int flags0,flagsCollider0;
	int bCanBrake;
  int bBlocked;
	int iBuddy;
	float r,rinv; // wheel radius, 1.0/radius
	float width;
	int bRayCast;

	float curlen; // current length
	float steer; // steering angle
	float rot; // current wheel rotation angle
	float w; // current rotation speed
	float wa; // current angular acceleration
	float T; // wheel's net torque
	float prevTdt;
	float prevw;
	EventPhysCollision *pCollEvent;

	Vec3 ncontact,ptcontact; // filled in RegisterPendingCollisions
	int bSlip,bSlipPull;
	int bContact;
	int surface_idx[2];
	Vec3 vrel;
	Vec3 rworld;
	float vworld;
	float PN;
	RigidBody *pbody;
	CPhysicalEntity *pent;
	int ipart;
	float unproj;
	entity_contact contact;
};

struct SWheeledVehicleEntityNetSerialize
{
	suspension_point * pSusp;
	float pedal;
	float steer;
	float clutch;
	float wengine;
	bool handBrake;
	int32 curGear;

	void Serialize( TSerialize ser, int nSusp );
};

class CWheeledVehicleEntity : public CRigidEntity {
 public:
	CWheeledVehicleEntity(CPhysicalWorld *pworld);
	virtual pe_type GetType() { return PE_WHEELEDVEHICLE; }

	virtual int SetParams(pe_params*,int bThreadSafe=1);
	virtual int GetParams(pe_params*);
	virtual int Action(pe_action*,int bThreadSafe=1);
	virtual int GetStatus(pe_status*);

	enum snapver { SNAPSHOT_VERSION = 1 };
	virtual int GetSnapshotVersion() { return SNAPSHOT_VERSION; }
	virtual int GetStateSnapshot(class CStream &stm, float time_back=0, int flags=0);
	virtual int GetStateSnapshot(TSerialize ser, float time_back=0, int flags=0);
	virtual int SetStateFromSnapshot(class CStream &stm, int flags=0);
	virtual int SetStateFromSnapshot(TSerialize ser, int flags=0);

	virtual int AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id=-1,int bThreadSafe=1);
	virtual void RemoveGeometry(int id,int bThreadSafe=1);

	virtual float GetMaxTimeStep(float time_interval);
	virtual float GetDamping(float time_interval);
	virtual void CheckAdditionalGeometry(float time_interval);
	virtual int RegisterContacts(float time_interval,int nMaxPlaneContacts);
	virtual int RemoveCollider(CPhysicalEntity *pCollider, bool bRemoveAlways=true);
	virtual int HasContactsWith(CPhysicalEntity *pent);
	virtual void AddAdditionalImpulses(float time_interval);
	virtual int Update(float time_interval, float damping);
	virtual void ComputeBBox(Vec3 *BBox, int flags);
	//virtual RigidBody *GetRigidBody(int ipart=-1) { return &m_bodyStatic; }
	virtual void OnContactResolved(entity_contact *pcontact, int iop, int iGroupId);
	virtual void DrawHelperInformation(IPhysRenderer *pRenderer, int flags);

	virtual void GetMemoryStatistics(ICrySizer *pSizer);

	void RecalcSuspStiffness();
	float ComputeDrivingTorque(float time_interval);

	suspension_point m_susp[NMAXWHEELS];
	CTriangleCache	m_wheelCache[NUM_CACHED_WHEELS];
	float m_enginePower,m_maxSteer;
	float m_engineMaxw,m_engineMinw,m_engineIdlew,m_engineShiftUpw,m_engineShiftDownw,m_gearDirSwitchw,m_engineStartw;
	float m_axleFriction,m_brakeTorque,m_clutchSpeed,m_minBrakingFriction,m_maxBrakingFriction,m_kDynFriction,m_slipThreshold;
	float m_kStabilizer;
	float m_enginePedal,m_steer,m_clutch,m_wengine;
	float m_gears[12];
	int m_nGears,m_iCurGear;
	int m_maxGear,m_minGear;
	float m_kSteerToTrack;
	int m_bHandBrake;
	int m_nHullParts;
	int m_iIntegrationType;
	float m_EminRigid,m_EminVehicle;
	float m_maxAllowedStepVehicle,m_maxAllowedStepRigid;
	float m_dampingVehicle;
	Vec3 m_Ffriction,m_Tfriction;
	float m_timeNoContacts;
	int m_nContacts,m_bHasContacts;
	volatile int m_lockVehicle;
  float m_pullTilt;
  float m_drivingTorque;	  
	int m_noShootImpulse;
};

#endif