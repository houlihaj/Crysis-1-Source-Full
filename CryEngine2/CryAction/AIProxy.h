/********************************************************************
  CryGame Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   AIProxy.h
  Version:     v1.00
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 7:10:2004   18:54 : Created by Kirill Bulatsev

*********************************************************************/


#ifndef __AIProxy_H__
#define __AIProxy_H__

#pragma once
#include <IAgent.h>
#include <IWeapon.h>
#include "AIHandler.h"
#include "IGameObject.h"
#include "IAnimationGraph.h"

struct ISystem;
struct IAISignalExtraData;
struct IEntity;
//struct IFireController;
struct IAnimationGraphStateListener;
struct SActorTargetParams;
struct IActor;


struct IFireController
{
	virtual bool RequestFire(bool bFire) = 0;
};


class CAIProxy
	: public IPuppetProxy
	, public IWeaponEventListener
{
friend class CScriptBind_AI;
public:
	CAIProxy(IGameObject *pGameObject, bool useAIHandler=true);
	virtual ~CAIProxy(void);

	//------------------  IWeaponEventListener
	virtual void OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType,
		const Vec3 &pos, const Vec3 &dir, const Vec3 &vel);
	virtual void OnStartFire(IWeapon *pWeapon, EntityId shooterId) {};
	virtual void OnStopFire(IWeapon *pWeapon, EntityId shooterId) {};
	virtual void OnStartReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {};
	virtual void OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {};
	virtual void OnOutOfAmmo(IWeapon *pWeapon, IEntityClass* pAmmoType) {};
	virtual void OnReadyToFire(IWeapon *pWeapon) {};
	virtual void OnPickedUp(IWeapon *pWeapon, EntityId actorId, bool destroyed){};
	virtual void OnDropped(IWeapon *pWeapon, EntityId actorId);
	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId) {}
	virtual void OnStartTargetting(IWeapon *pWeapon) {}
	virtual void OnStopTargetting(IWeapon *pWeapon) {}
	virtual void OnSelected(IWeapon *pWeapon, bool selected);
	//------------------  ~IWeaponEventListener

	virtual bool	IsEnabled() const;
	virtual void	EnableUpdate(bool enable);

	//interface implementations IUnknownProxy
	void	EnableWeaponListener(bool needsSignal);
	bool QueryProxy(unsigned char type, void **pProxy);
	int Update(SOBJECTSTATE *pStates);
	bool CheckUpdateStatus();
	virtual int GetAlertnessState() const;
	virtual bool IsCurrentBehaviorExclusive() const;
	virtual bool SetCharacter( const char* character, const char* behaviour=NULL );
	virtual const char* GetCharacter();
	void QueryBodyInfo( SAIBodyInfo& bodyInfo ) ;
	bool QueryBodyInfo( EStance stance, float lean, bool defaultPose, SAIBodyInfo& bodyInfo );
	void QueryWeaponInfo( SAIWeaponInfo& weaponInfo );
	virtual EntityId GetLinkedDriverEntityId();
	virtual bool IsDriver();
	virtual EntityId GetLinkedVehicleEntityId();
	virtual IEntity* GetGrabbedEntity() const;
	void Release();
	IPhysicalEntity* GetPhysics(bool wantCharacterPhysics=false);
	void DebugDraw(struct IRenderer *pRenderer, int iParam = 0);
	int& DebugValue(int idx=0);	
	virtual bool SetAGInput(EAIAGInput input, const char* value);
	virtual bool ResetAGInput(EAIAGInput input);
	virtual bool IsSignalAnimationPlayed( const char* value );
	virtual bool IsActionAnimationStarted( const char* value );
	virtual bool IsAnimationBlockingMovement();

	virtual bool IsDead() const;
	virtual int GetActorHealth();
	virtual int GetActorMaxHealth();
	virtual int GetActorArmor();
	virtual int GetActorMaxArmor();
	virtual bool GetActorIsFallen() const;

	//------------------  ~IUnknownProxy

	//interface implementations IPuppetProxy
	virtual bool PredictProjectileHit(const Vec3& throwDir, float vel, Vec3& posOut, float& speedOut,
		Vec3* pTrajectory = 0, unsigned int* trajectorySizeInOut = 0);
	virtual int GetAndResetShotBulletCount() { int ret = m_shotBulletCount; m_shotBulletCount = 0; return ret; }
	//------------------  ~IPuppetProxy

	virtual int PlayReadabilitySound(const char* szReadability, bool stopPreviousSound);
	virtual const char* GetCurrentReadibilityName() const;
	virtual void TestReadabilityPack(bool start, const char* szReadability);
	virtual void GetReadabilityBlockingParams(const char* text, float& time, int& id);

	void Reset(EObjectResetType type);
	void Serialize( TSerialize ser );
	//inline void CancelSuspendedTasks() {m_AIHandler.CancelSuspendedTasks();};
	//inline bool SuspendedTask()	{return m_AIHandler.SuspendedTask();};

	// gets the corners of the tightest projected bounding rectangle in 2D world space coordinates
	// adds extra around
	virtual void GetWorldBoundingRect(Vec3& FL, Vec3& FR, Vec3& BL, Vec3& BR, float extra=0) const;
	inline void	SetMinFireTime(float fTime) {m_fMinFireTime = fTime;}

	virtual bool QueryCurrentAnimationSpeedRange(float& smin, float& smax);


	// TO DO: manage the decision of firing or not entirely in Puppet
//	inline void ClearFire() {m_StartWeaponSpinUpTime=0;}
	void UpdateMind(SOBJECTSTATE *state);

	void UpdateMeAlways(bool doUpdateMeAlways);
	// those are needed for debugging
	bool IsUpdateAlways() const {return m_UpdateAlways;}
	bool IfShouldUpdate() {return m_pGameObject->ShouldUpdate();}

	inline CAIHandler*	GetAIHandler() { return m_pAIHandler; }

	// checks if an AI can jump to a point
	// dest = destination point
	// theta = input angle (in degrees)
	// maxheight = max allowed jump height (in meters)
	// flags (AI_JUMP_ON_GROUND -> the point will be projected on ground - AI_JUMP_CHECK_COLLISION -> collision will be checked on the jump parabula)
	// retVelocity = if jump can be performed, this is the returned initial velocity
	// tDest = if jump can be performed, this is the returned time to reach destination
	// pSkipEnt = optional, skip this entity in collision check
	// startPos = optional, compute the jump start from this position instead of AI's position
	// Returns: true if the jump can be performed
	virtual bool CanJumpToPoint(Vec3& dest, float theta, float maxheight, int flags, Vec3& retVelocity, float& tDest, IPhysicalEntity* pSkipEnt, Vec3* pStartPos);

protected:

	inline IActor*	GetActor() const
	{
		if (!m_pIActor)
			m_pIActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(m_pGameObject->GetEntityId());  
		return m_pIActor;
	}

	void SendSignal(int signalID, const char * szText, IEntity *pSender, const IAISignalExtraData* pData);
	void UpdateAuxSignal(SOBJECTSTATE *state);
	IWeapon *GetCurrentWeapon() const;
	IWeapon *GetSecWeapon( ERequestedSecondaryType prefType=RST_ANY ) const;
	IFireController* GetFireController();

	void	UpdateShooting(SOBJECTSTATE *pStates, bool isAlive);

	IGameObject	*m_pGameObject;
	CAIHandler	*m_pAIHandler;
	bool m_firing;
	bool m_firingSecondary;

	string	m_readibilityName;
	float		m_readibilityDelay;
	bool		m_readibilityPlayed;
	int			m_readibilityPriority;
	int			m_readibilityId;
	int			m_readibilityIgnored;

	// weapon/shooting related
	float		m_fMinFireTime; //forces a minimum time for fire trigger to stay on
	bool		m_WeaponShotIsDone;	//if weapon did shot during last update
	bool		m_NeedsShootSignal;

	int			m_shotBulletCount;
	CTimeValue m_lastShotTime;

	int			m_PrevAlertness;
	bool		m_IsDisabled;
	bool		m_UpdateAlways;
	bool		m_IsWeaponForceAlerted;

	mutable IActor	*m_pIActor;

	int			dbg_ShotsCount;
	int			dbg_HitsCount;
};


#endif __AIProxy_H__
