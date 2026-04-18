#ifndef _AI_VEHICLE_
#define _AI_VEHICLE_

#if _MSC_VER > 1000
#pragma once
#endif

#include "Puppet.h"

#include "CAISystem.h"
#include "PipeUser.h"

class CPuppet;

class CAIVehicle :
	public CPuppet, IAIVehicle
{
public:
	CAIVehicle(CAISystem*);
	~CAIVehicle(void);

	virtual const IAIVehicle* CastToIAIVehicle() const { return this; }
	virtual IAIVehicle* CastToIAIVehicle() { return this; }

	void Update(EObjectUpdate type);
	void UpdateDisabled(EObjectUpdate type);
	void Navigate(CAIObject *pTarget);
	void Event(unsigned short eType, SAIEVENT *pEvent);

	void Reset(EObjectResetType type);
	void ParseParameters(const AIObjectParameters &params);
	void OnObjectRemoved(CAIObject *pObject);

	void	SetParameters(AgentParameters & sParams);

	void AlertPuppets(void);
	virtual void Serialize( TSerialize ser, class CObjectTracker& objectTracker );

	bool HandleVerticalMovement( const Vec3& targetPos );

	bool IsDriverInside() const;
	bool IsPlayerInside();

	virtual bool IsActive() const { return m_bEnabled || IsDriverInside(); }

protected:
	void	FireCommand(void);
	bool	GetEnemyTarget(int objectType, Vec3& hitPosition,float fDamageRadius2, CAIObject** pTarget);

private:

	// local functions for firecommand()
	Vec3 PredictMovingTarget(const CAIObject* pTarget, const Vec3 &vTargetPos, const Vec3 &vFirePos, const float duration, const float distpred );
	bool CheckExplosion( const Vec3 &vTargetPos, const Vec3 &vFirePos, const Vec3 &vActuallFireDir, const float fDamageRadius );
	Vec3 GetError( const Vec3 &vTargetPos ,const Vec3 &vFirePos, const float fAccuracy );
	float RecalculateAccuracy( void );

	bool		m_bPoweredUp;

	CTimeValue	m_fNextFiringTime;			//To put an interval for the next firing. 
	CTimeValue	m_fFiringPuaseTime;			//Put a pause time after the trurret rotation has completed.
	CTimeValue	m_fFiringStartTime;			//the time that the firecommand started.

	Vec3		m_vDeltaTarget;				//To add a vector to the target position for errors and moving prediction

	int			m_ShootPhase;
	mutable int			m_driverInsideCheck;
	int			m_playerInsideCheck;
	bool		m_bDriverInside;
};

inline const CAIVehicle* CastToCAIVehicleSafe(const IAIObject* pAI) { return pAI ? pAI->CastToCAIVehicle() : 0; }
inline CAIVehicle* CastToCAIVehicleSafe(IAIObject* pAI) { return pAI ? pAI->CastToCAIVehicle() : 0; }

#endif

