#ifndef __FireCommand_H__
#define __FireCommand_H__

#include "IAgent.h"
#include "CAISystem.h"
//#include "Puppet.h"

class CAIActor;
class CPuppet;

//====================================================================
// CFireCommandInstantDeprecated
// For G04 compatibility.
//====================================================================
class CFireCommandInstantDeprecated : public IFireCommandHandler
{
friend class CAISystem;	// need this for DebugDraw
public:
	CFireCommandInstantDeprecated(IAIActor* pShooter);
	virtual const char*	GetName() { return "instant_deprecated"; }


	virtual void	Reset();
	virtual bool	Update(IAIObject* pITarget, bool canFire, EFireMode fireMode,
												const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool	ValidateFireDirection(const Vec3& fireVector, bool mustHit);
	virtual void	Release() { delete this; }
	virtual void DebugDraw(struct IRenderer *pRenderer);
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return true; }
	virtual void OnReload() {}
	virtual void OnShoot() {}
	virtual int	 GetShotCount() const {return 0;}

protected:
//private:

	bool	DoNextShot(CAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual void	CheckIfNeedsToFade(CAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool	IsStartingNow() const {return m_fFadingToNormalTime>0.f;}
	bool	SelectFireDirNormalFire(CAIObject* pTarget, float fAccuracyRamping, Vec3& outShootTargetPos);
	bool	SelectFireDirStartingFire(CAIObject* pTarget, Vec3& outShootTargetPos);
	bool	DoThisShot(Vec3& outShootTargetPos);
	bool	DoDrawFireLine(CAIObject* pTarget, Vec3& outShootTargetPos);
	bool	IsWeaponPointingRight(const Vec3	&shootTargetPos); //const;
	Vec3	ChooseMissPoint(CAIObject* pTarget) const;

	bool	UpdateTriggerPressing(bool canFire, EFireMode fireMode, float dt);
	virtual int		SelectBurstLength();
	float	SelectBurstTimeOut();
	float	SelectBurstLengthOld();

	CPuppet*	m_pShooter;
//	IAIObject* m_pShooter;
	bool			m_bFireTriggerDown;
	float			m_fFireTriggerDownTime;
	float			m_fFireTriggerUpTime;
	float			m_fBurstLength;
	// draw-fire-line controllers
//	float			m_fDrawFireTime;			// 
	float			m_fDrawFireLastTime;			// time when the last "draw-line-fire" happened 
	float			m_fDrawFireStepTime;			// 
	int				m_nDrawFireCounter;		// "draw-line-fire" stage

	float			m_fLastShootingTime;

	//--- per-shot stuff ------------------------------------------------------------------
	bool		m_JustReset;
	Vec3		m_LastFireTargetSeenPos;	// position where seen the target last time
	Vec3		m_LastFirePos;						// were was shooting last time (with missing, body parts, etc)

	bool		m_bIsTriggerDown;
	bool		m_bIsBursting;
	int			m_BurstShotsCount;
	float		m_fTimeOut;
	float		m_fLastBulletOutTime;

	float		m_fFadingToNormalTime;	//just fire using accuracy - otherwise DrawFireLine/miss  
	float		m_fFadingToNormalTimeTotal;	//just fire using accuracy - otherwise DrawFireLine/miss  
	float		m_fLastValidTragetTime;	//

	float			m_fDrawFireDist;			// 

	//debug draw stuff
	bool	dbg_FrinedsInLine;
	bool	dbg_IsAiming;
	bool	dbg_AngleTooDifferent;
	float	dbg_AngleDiffValue;
	int		dbg_ShotCounter;
	int		dbg_ShotHitCounter;

};


//====================================================================
// CFireCommandInstant
// Fire command handler for assault rifles and alike.
// When starting to fire the fire is drawn along a line towards the target.
//====================================================================
class CFireCommandInstant : public IFireCommandHandler
{
	friend class CAISystem;	// need this for DebugDraw
public:
	CFireCommandInstant(IAIActor* pShooter);
	virtual const char*	GetName() { return "instant"; }


	virtual void	Reset();
	virtual bool	Update(IAIObject* pITarget, bool canFire, EFireMode fireMode,
		const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual void	Release() { delete this; }
	virtual bool	ValidateFireDirection(const Vec3& fireVector, bool mustHit) { return true; }
	virtual void DebugDraw(struct IRenderer *pRenderer);
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return true; }
	virtual void OnReload() {}
	virtual void OnShoot() { ++m_ShotsCount; }
	virtual int	 GetShotCount() const {return m_ShotsCount;}

protected:
	CPuppet*	m_pShooter;

	// For new damage control
	enum EBurstState
	{
		BURST_NONE,
		BURST_FIRE,
		BURST_PAUSE,
	};
	EBurstState m_weaponBurstState;
	float		m_weaponBurstTime;
	float		m_weaponBurstTimeScale;
	int			m_weaponBurstBulletCount;
	int			m_curBurstBulletCount;
	float		m_curBurstPauseTime;
	float		m_weaponTriggerTime;
	bool		m_weaponTriggerState;
	float		m_coverFireTime;
	int			m_ShotsCount;
};


//====================================================================
// CFireCommandInstantSinle
// Fire command handler for pistols/shotguns/etc.
// When starting to fire the fire is drawn along a line towards the target.
//====================================================================
class CFireCommandInstantSingle : public CFireCommandInstant
{
public:
	CFireCommandInstantSingle(IAIActor* pShooter):
	  CFireCommandInstant(pShooter) {}

	virtual int		SelectBurstLength() {return 0;}
	virtual void	CheckIfNeedsToFade(CAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
										{return;}
	virtual bool	IsStartingNow() const {return false;}
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return true; }

};


//====================================================================
// CFireCommandProjectileSlow
// Fire command handler for slow projectiles such as hand grenades.
//====================================================================
class CFireCommandProjectileSlow: public IFireCommandHandler
{
public:
	CFireCommandProjectileSlow(IAIActor* pShooter);
	virtual const char*	GetName() { return "projectile_slow"; }
	virtual void	Reset();
	virtual bool	Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
												const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool	ValidateFireDirection(const Vec3& fireVector, bool mustHit) {return true;}
	virtual void Release() { delete this; }
	virtual void DebugDraw(struct IRenderer *pRenderer) {}
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return true; }
	virtual void OnReload() {}
	virtual void OnShoot() {}
	virtual int	 GetShotCount() const {return 0;}

private:
	CAIActor* m_pShooter;
};


//====================================================================
// CFireCommandProjectileFast
// Fire command handler for fast projectiles such as RPG.
//====================================================================
class CFireCommandProjectileFast : public IFireCommandHandler
{
public:
	CFireCommandProjectileFast(IAIActor* pShooter);
	virtual const char*	GetName() { return "projectile_fast"; }
	virtual void	Reset();
	virtual bool	Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
		const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool	ValidateFireDirection(const Vec3& fireVector, bool mustHit);
	virtual void Release() { delete this; }
	virtual void DebugDraw(struct IRenderer *pRenderer);
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return true; }
	virtual void OnReload() 
	{
		m_ShotCounter=0;
	}
	virtual void OnShoot() 
	{
		AdvanceFireDraw();
	}
	virtual int	 GetShotCount() const {return m_ShotCounter;}

private:
	enum EMissMode
	{
		MISS_INFRONT,
		MISS_SIDES,
		MISS_BACK,
		MISS_HITFACE,
	};

	bool	NoFriendNearTarget(float explRadius, const Vec3& targetPos);
	bool	ChooseShootPoint(IAIObject* pTarget, float missDist, Vec3& shootAtPos, EMissMode missMode);
	void	AdvanceFireDraw()
	{
		m_ShotCounter=min(2, m_ShotCounter+1); // there are 3 rockets in the LAW
	}

	CPuppet*		m_pShooter;
	Vec3				m_aimPos;
	Vec3				m_targetPos;
	Vec3				m_targetVel;
	float				m_steadyTime;
	int					m_ShotCounter;
};


//====================================================================
// CFireCommandProjectileXP
// Extended ai_handler for LAW with firing rate limitation
//====================================================================
class CFireCommandProjectileXP : public CFireCommandProjectileFast
{
public:
	CFireCommandProjectileXP(IAIActor* pShooter);
	virtual const char*	GetName() { return "projectile_fast_xp"; }
	virtual void	Reset();
	virtual bool	Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
		const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	
private:


	float		m_weaponTriggerTime;
	bool		m_weaponTriggerState;
};


//====================================================================
// CFireCommandStrafing
// Special fire command handler for alien Scout.
//====================================================================
class CFireCommandStrafing : public IFireCommandHandler
{
public:
	CFireCommandStrafing(IAIActor* pShooter);
	virtual const char*	GetName() { return "strafing"; }
	virtual void	Reset();
	virtual bool	Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
												const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool	ValidateFireDirection(const Vec3& fireVector, bool mustHit) {return true;}
	virtual void	Release() { delete this; }
	bool	ManageFireDirStrafing(IAIObject* pTarget, Vec3 &targetPos, bool& fire, const AIWeaponDescriptor& descriptor);
	virtual void DebugDraw(struct IRenderer *pRenderer) {}
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const
	{
		if (fireMode == FIREMODE_BURST_DRAWFIRE)
			return false;
		else if (fireMode == FIREMODE_PANIC_SPREAD)
			return false;
		return true;
	}
	virtual void OnReload() {}
	virtual void OnShoot() {}
	virtual int	 GetShotCount() const {return 0;}

private:
	CPuppet*	m_pShooter;
	CTimeValue				m_fLastStrafingTime;
	CTimeValue				m_fLastUpdateTime;
	int						m_StrafingCounter;
};

//====================================================================
// CFireCommandFastLightMOAR
// Special fire command handler for alien light MOAR.
// Draws fire in spiral fashion.
//====================================================================
class CFireCommandFastLightMOAR : public IFireCommandHandler
{
public:
	CFireCommandFastLightMOAR(IAIActor* pShooter);
	virtual const char*	GetName() { return "fast_light_moar"; }
	virtual void	Reset();
	virtual bool	Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
												const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool	ValidateFireDirection(const Vec3& fireVector, bool mustHit) {return true;}
	virtual void	Release() { delete this; }
	virtual void DebugDraw(struct IRenderer *pRenderer) {}
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return true; }
	virtual void OnReload() {}
	virtual void OnShoot() {}
	virtual int	 GetShotCount() const {return 0;}

private:
	CAIActor*			m_pShooter;
	CTimeValue		m_startTime;
	Vec3					m_laggedTarget;
	bool					m_startFiring;
	float					m_startDist;
	Vec2					m_rand;
};


//====================================================================
// CFireCommandHunterMOAR
// Special fire command handler for Hunter's MOAR.
//====================================================================
class CFireCommandHunterMOAR : public IFireCommandHandler
{
public:
	CFireCommandHunterMOAR( IAIActor* pShooter );
	virtual const char*	GetName() { return "hunter_moar"; }
	virtual void Reset() { m_startTime = gEnv->pAISystem->GetFrameStartTime(); m_startFiring = false; }
	virtual bool Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool ValidateFireDirection(const Vec3& fireVector, bool mustHit) { return true; }
	virtual void Release() { delete this; }
	virtual void DebugDraw(struct IRenderer *pRenderer) {}
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return false; }
	virtual void OnReload() {}
	virtual void OnShoot() {}
	virtual int	 GetShotCount() const {return 0;}

private:
	CAIActor* m_pShooter;
	CTimeValue m_startTime;
	Vec3 m_targetPos;
	Vec3 m_lastPos;
	bool m_startFiring;
};


//====================================================================
// CFireCommandHunterSweepMOAR
// Special fire command handler for Hunter's MOAR.
//====================================================================
class CFireCommandHunterSweepMOAR : public IFireCommandHandler
{
public:
	CFireCommandHunterSweepMOAR( IAIActor* pShooter );
	virtual const char*	GetName() { return "hunter_sweep_moar"; }
	virtual void Reset() { m_startTime = gEnv->pAISystem->GetFrameStartTime(); m_startFiring = false; }
	virtual bool Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool ValidateFireDirection(const Vec3& fireVector, bool mustHit) { return true; }
	virtual void Release() { delete this; }
	virtual void DebugDraw(struct IRenderer *pRenderer) {}
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return false; }
	virtual void OnReload() {}
	virtual void OnShoot() {}
	virtual int	 GetShotCount() const {return 0;}

private:
	CAIActor* m_pShooter;
	CTimeValue m_startTime;
	Vec3 m_targetPos;
	Vec3 m_lastPos;
	bool m_startFiring;
};


//====================================================================
// CFireCommandHunterSingularityCannon
// Special fire command handler for Hunter's Singularity Cannon.
//====================================================================
class CFireCommandHunterSingularityCannon : public IFireCommandHandler
{
public:
	CFireCommandHunterSingularityCannon( IAIActor* pShooter );
	virtual const char*	GetName() { return "hunter_singularity_cannon"; }
	virtual void Reset() { m_startTime = gEnv->pAISystem->GetFrameStartTime(); m_startFiring = false; }
	virtual bool Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool ValidateFireDirection(const Vec3& fireVector, bool mustHit) { return true; }
	virtual void Release() { delete this; }
	virtual void DebugDraw(struct IRenderer *pRenderer) {}
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return false; }
	virtual void OnReload() {}
	virtual void OnShoot() {}
	virtual int	 GetShotCount() const {return 0;}

private:
	CAIActor* m_pShooter;
	CTimeValue m_startTime;
	Vec3 m_targetPos;
	bool m_startFiring;
};


//====================================================================
// CFireCommandGrenade
// calculates the grenade trajectory - finds the best one; makes sure not to hit friends
// 
//====================================================================
class CFireCommandGrenade : public IFireCommandHandler
{
public:
	CFireCommandGrenade(IAIActor* pShooter);
	~CFireCommandGrenade();
	virtual const char*	GetName() { return "grenade"; }
	virtual void	Reset();
	virtual bool	Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
		const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool	ValidateFireDirection(const Vec3& fireVector, bool mustHit) {return true;}
	virtual void	Release() { delete this; }
	virtual void DebugDraw(struct IRenderer *pRenderer);
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return true; }
	virtual void OnReload() {}
	virtual void OnShoot() {}
	virtual int	 GetShotCount() const {return 0;}

	inline float GetThrowSpeedScale() const { return m_throwSpeedScale; }

private:

	float	EvaluateThisThrow(const Vec3& targetPos, const Vec3& targetViewDir, const Vec3& dir, float vel, float& speedOut, EFireMode fireMode);

	CPuppet*	m_pShooter;

	int m_iter;
	Vec3 m_targetPos;
	float m_preferredHeight;
	Vec3	m_throwDir;
	float	m_throwSpeedScale;
	float m_bestScore;

	void ClearDebugEvals();

	struct SDebugThrowEval
	{
		std::vector<Vec3> trajectory;
		Vec3 pos;
		float score;
		bool fake;
	};

	std::vector<SDebugThrowEval*> m_DEBUG_evals;
};


//====================================================================
// CFireCommandHurricane
// Special fire command handler for Hurricane - shoot long bursts
//====================================================================
class CFireCommandHurricane: public IFireCommandHandler //: public CFireCommandInstant
{
public:
	CFireCommandHurricane(IAIActor* pShooter);
	virtual const char*	GetName() { return "hurricane"; }
	virtual void	Reset();
	virtual bool	Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
		const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos);
	virtual bool	ValidateFireDirection(const Vec3& fireVector, bool mustHit);
	virtual void	Release() { delete this; }
	virtual void DebugDraw(struct IRenderer *pRenderer) {}
	virtual bool UseDefaultEffectFor(EFireMode fireMode) const { return true; }
	virtual void OnReload() {}
	virtual void OnShoot() {}
	virtual int	 GetShotCount() const {return 0;}

private:
	bool	SelectFireDirNormalFire(CAIObject* pTarget, float fAccuracyRamping, Vec3& outShootTargetPos);
	bool	IsWeaponPointingRight(const Vec3	&shootTargetPos);
	Vec3  ChooseMissPoint(CAIObject* pTarget) const;

	CPuppet*	m_pShooter;
	//debug draw stuff
	bool	dbg_FrinedsInLine;
	bool	dbg_IsAiming;
	bool	dbg_AngleTooDifferent;
	float	dbg_AngleDiffValue;
	int		dbg_ShotCounter;
	int		dbg_ShotHitCounter;
};


#endif
