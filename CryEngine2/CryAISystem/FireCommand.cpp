#include "StdAfx.h"
#include "FireCommand.h"
#include "Puppet.h"
#include "IRenderer.h"
#include "IRenderAuxGeom.h"	

//====================================================================
// CFireCommandInstantDeprecated
//====================================================================
CFireCommandInstantDeprecated::CFireCommandInstantDeprecated(IAIActor* pShooter) : 
m_fLastShootingTime(0),
m_fDrawFireLastTime(0),
m_nDrawFireCounter(0),
m_JustReset(false),
m_bIsTriggerDown(false),
m_bIsBursting(false),
m_fTimeOut(-1.f),
m_fLastBulletOutTime(0.f),
m_fFadingToNormalTime(0.f),
m_fLastValidTragetTime(0.f),
m_pShooter(((CAIActor*)pShooter)->CastToCPuppet())
{
}


//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandInstantDeprecated::DoNextShot(CAIObject* pTarget, bool canFire, EFireMode fireMode,
																		 const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	if(!canFire)
		return false;

	dbg_FrinedsInLine = false;
	dbg_IsAiming = true;

	CheckIfNeedsToFade(pTarget, canFire, fireMode, descriptor, outShootTargetPos);

	//now let's calculate where to fire
	SAIBodyInfo bodyInfo;
	m_pShooter->GetProxy()->QueryBodyInfo(bodyInfo);
	// Check if there is any friendly units infront
	if(m_pShooter->CheckFriendsInLineOfFire((pTarget->GetPos() - m_pShooter->GetFirePos()), true))
	{
		dbg_FrinedsInLine = true;
		return false;
	}
	bool fireNow;
	if(IsStartingNow())
		fireNow = SelectFireDirStartingFire(pTarget, outShootTargetPos);
	else
		fireNow = SelectFireDirNormalFire(pTarget, 1.f, outShootTargetPos);

	// Check if weapon is not pointing too differently from where we want to shoot
	// The outShootTargetPos is initialized with the aim target.
	if(fireNow && !IsWeaponPointingRight(outShootTargetPos))
		return false;

	return fireNow;
}


//
//--------------------------------------------------------------------------------------------------------
void CFireCommandInstantDeprecated::CheckIfNeedsToFade(CAIObject* pTarget, bool canFire, EFireMode fireMode,
																						 const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	float	curTime = gEnv->pTimer->GetCurrTime();

	// if not fading already
	if(!IsStartingNow() )
	{
		Vec3	lastTargetDir(m_LastFireTargetSeenPos - m_pShooter->GetPos());
		Vec3	currTargetDir(pTarget->GetPos() - m_pShooter->GetPos());

		lastTargetDir.NormalizeSafe();
		currTargetDir.NormalizeSafe();
		float	targetDiffDot = lastTargetDir*currTargetDir;

		// if not fading to normal fire currently and diff too big/time too long, start fading
		if((targetDiffDot<cosf(5.f) || curTime-m_fLastValidTragetTime>5.f) )
		{
			// calculation reaction time here
			float	dist((pTarget->GetPos()-m_pShooter->GetPos()).len());
			float	distScale(min(2.5f,(dist*2.f/m_pShooter->GetParameters().m_fAttackRange)));
			m_fFadingToNormalTime = 1.0f/*m_pShooter->GetParameters().m_fShootingReactionTime*/*distScale;
			m_fFadingToNormalTimeTotal = m_fFadingToNormalTime;
			m_fDrawFireDist = -1.f;
		}
	}
	int targetType(pTarget!=NULL ? ((CAIObject*)pTarget)->GetType() : AIOBJECT_NONE);
	if(targetType == AIOBJECT_PLAYER || targetType == AIOBJECT_PUPPET || targetType == AIOBJECT_CVEHICLE)
		m_fLastValidTragetTime = curTime;
}


//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandInstantDeprecated::SelectFireDirStartingFire(CAIObject* pTarget, Vec3& outShootTargetPos)
{
	// draw line or miss 
	if(DoDrawFireLine(pTarget, outShootTargetPos))
		return true;
	// no drawing line - decide if miss or hit now
	return SelectFireDirNormalFire(pTarget, 1.f-m_fFadingToNormalTime/m_fFadingToNormalTimeTotal, outShootTargetPos);

	//	outShootTargetPos = ChooseMissPoint(pTarget);
	//	return true;
}

//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandInstantDeprecated::SelectFireDirNormalFire(CAIObject* pTarget, float fAccuracyRamping, Vec3& outShootTargetPos)
{
	//accuracy/missing management: miss if not in front of player or if accuracy needed
	float	shooterCurrentAccuracy = m_pShooter->GetAccuracy(pTarget);
	bool bAccuracyMiss = Random(0.f,100.f) > (shooterCurrentAccuracy* 100.f);
	bool bMissNow = bAccuracyMiss; // || m_pShooter->GetAmbientFire();

	++dbg_ShotCounter;

	if(bMissNow)
	{
		outShootTargetPos = ChooseMissPoint(pTarget);
	}
	else
	{
		Vec3	vTargetPos;
		Vec3	vTargetDir;
		// In addition to common fire checks, make sure that we do not hit something too close between us and the target.
		// check if we don't hit anything (rocks/trees/cars)
		bool	lowDamage = fAccuracyRamping < 1.0f;
//		if(pTarget->GetAIType() == AIOBJECT_PLAYER)
//			lowDamage = lowDamage || !GetAISystem()->GetCurrentDifficultyProfile()->allowInstantKills;
		if(!lowDamage)
			lowDamage = Random(0.f,100.f) > (shooterCurrentAccuracy * 50.f);	// decide if want to hit legs/arms (low damage)
		if(!m_pShooter->CheckAndGetFireTarget_Deprecated(pTarget, lowDamage, vTargetPos, vTargetDir))
			return false;
		outShootTargetPos = vTargetPos;

		if(!outShootTargetPos.IsZero(1.f))
		{
			++dbg_ShotHitCounter;
		}
	}
	//
	if(outShootTargetPos.IsZero(1.f))
	{
		outShootTargetPos = pTarget->GetPos();
		return false;
	}

	return true;
}


//
//	we have candidate shooting direction, let's check if it is good
//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandInstantDeprecated::ValidateFireDirection(const Vec3& fireVector, bool mustHit)
{
	TICK_COUNTER_FUNCTION
		ray_hit hit;
	int rwiResult;
	IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
	TICK_COUNTER_FUNCTION_BLOCK_START
		rwiResult = pWorld->RayWorldIntersection(m_pShooter->GetFirePos(), fireVector, COVER_OBJECT_TYPES, HIT_COVER, &hit, 1);
	TICK_COUNTER_FUNCTION_BLOCK_END
		// if hitting something too close to shooter - bad fire direction
		float fireVectorLen;
	if (rwiResult!=0)
	{
		fireVectorLen = fireVector.GetLength();
		if(mustHit || hit.dist < fireVectorLen * (m_pShooter->GetFireMode() == FIREMODE_FORCED? 0.3f: 0.73f))
			return false;
	}
	return !m_pShooter->CheckFriendsInLineOfFire(fireVector, false);
}


//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandInstantDeprecated::DoThisShot(Vec3& outShootTargetPos)
{
	outShootTargetPos = m_LastFirePos;
	return m_bIsTriggerDown;
}


//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandInstantDeprecated::DoDrawFireLine(CAIObject* pTarget, Vec3& outShootTargetPos)
{
	if(!m_pShooter->IsAllowedToHitTarget())
		return false;
	if(m_fDrawFireDist<0.f)
	{
		m_fDrawFireDist = (pTarget->GetPos() - m_pShooter->GetPos()).len();
		if(m_fDrawFireDist < 5.f)
		{
			m_nDrawFireCounter = -1;
		}
		else
		{
			m_fDrawFireDist = min(20.f, m_fDrawFireDist*.5f);
			m_nDrawFireCounter = (int)(m_fDrawFireDist/3.f);
			m_BurstShotsCount = m_nDrawFireCounter;		// make it a single burst
		}
	}
	if(m_nDrawFireCounter<=0)
		return false;

	SAIBodyInfo	targetBody;
	if(pTarget->GetProxy())
		pTarget->GetProxy()->QueryBodyInfo(targetBody);
	else 
		return false;
	float	targetGroundLevel = pTarget->GetPhysicsPos().z;
	Vec3	drawFireNextPos(m_pShooter->GetPos() - pTarget->GetPos());

	drawFireNextPos.Normalize();
	drawFireNextPos = pTarget->GetPos() + drawFireNextPos*m_fDrawFireDist;
	//drawFireNextPos = pTarget->GetPos() + drawFireNextPos*8.f;
	drawFireNextPos.z = targetGroundLevel;
	m_fDrawFireDist -= m_fDrawFireDist/static_cast<float>(m_nDrawFireCounter);
	--m_nDrawFireCounter;

	if(!ValidateFireDirection(drawFireNextPos-m_pShooter->GetFirePos(), false))
	{
		m_nDrawFireCounter = -1;	// stop drawing line
		return false;
	}

	outShootTargetPos = drawFireNextPos;
	return true;
}

//
//--------------------------------------------------------------------------------------------------------
Vec3 CFireCommandInstantDeprecated::ChooseMissPoint(CAIObject* pTarget) const
{
	Vec3	vTargetPos(pTarget->GetPos());
	SAIBodyInfo	targetBody;
	if(pTarget->GetProxy())
		pTarget->GetProxy()->QueryBodyInfo(targetBody);
	float	targetGroundLevel = pTarget->GetPhysicsPos().z;
	return m_pShooter->ChooseMissPoint_Deprecated(vTargetPos);
}

//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandInstantDeprecated::IsWeaponPointingRight(const Vec3	&shootTargetPos) //const
{
	// Check if weapon is not pointing too differently from where we want to shoot
	Vec3	fireDir(shootTargetPos - m_pShooter->GetFirePos());
	float dist2(fireDir.GetLengthSquared());
	// if withing 2m from target - make very loose angle check
	float	trhAngle( fireDir.GetLengthSquared()<sqr(5.f) ? 73.f : 22.f);
	fireDir.normalize();
	SAIBodyInfo bodyInfo;
	m_pShooter->GetProxy()->QueryBodyInfo(bodyInfo);

	//dbg_AngleDiffValue = acos_tpl( fireDir.Dot(bodyInfo.vFireDir));
	dbg_AngleDiffValue = RAD2DEG( acos_tpl(( fireDir.Dot(bodyInfo.vFireDir))) );

	if(fireDir.Dot(bodyInfo.vFireDir) < cry_cosf(DEG2RAD(trhAngle)) )
	{
		dbg_AngleTooDifferent = true;
		return false;
	}
	dbg_AngleTooDifferent = false;
	return true;
}

//
//--------------------------------------------------------------------------------------------------------
void CFireCommandInstantDeprecated::Reset()
{
	AIAssert(m_pShooter);
	m_bFireTriggerDown = false;
	m_fFireTriggerDownTime = 0;
	m_fFireTriggerUpTime = 0;
	m_fBurstLength = .1f;
	m_nDrawFireCounter = -1;

	m_bIsTriggerDown = false;
	m_JustReset = true;

	dbg_ShotCounter = 0;
	dbg_ShotHitCounter = 0;
}

//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandInstantDeprecated::Update(IAIObject* pITarget, bool canFire, EFireMode fireMode, 
																 const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	if(!canFire)
		return false;

	if (!pITarget)
		return false;

	//-------------- kill fire mode - just select the most vulnerable part and shoot right there--
	if(fireMode == FIREMODE_KILL)
	{
		outShootTargetPos = pITarget->GetPos();
		CAIActor* pTargetActor = pITarget->CastToCAIActor();
		//		CAIObject* pTargetAI = static_cast<CAIObject*>(pITarget);
		if(pTargetActor && pTargetActor->GetDamageParts())
		{
			DamagePartVector&	parts = *(pTargetActor->GetDamageParts());
			float	maxValue(-1.f);
			int		maxIdx(-1);

			int	n = (int)parts.size();
			for(int i = 0; i < n; ++i)
				if(parts[i].damageMult > maxValue)
				{
					maxValue = parts[i].damageMult;
					maxIdx = i;
				}
				if(maxIdx>=0)
					outShootTargetPos = parts[maxIdx].pos;
		}
		return true;
	}
	//\\------------- kill mode ------------------------------------------------------------------

	AIAssert(m_pShooter);

	float dt = GetAISystem()->GetFrameDeltaTime();

	CAIObject* pTarget((CAIObject*)pITarget);
	m_LastFireTargetSeenPos = pTarget->GetPos();
	// handle bursting here
	if(!m_bIsBursting)
	{
		// scale down burst length if shooter moves
		IPhysicalEntity* pPhysics;
		IUnknownProxy* pProxy;
		if((pProxy = m_pShooter->GetProxy()) && (pPhysics = pProxy->GetPhysics()))
		{
			static pe_status_dynamics  dSt;
			pPhysics->GetStatus( &dSt );
			float fSpeed= dSt.v.GetLength();
			if(fSpeed > 1.0f)
				dt/=1.5f;//less decrease for the timeout, make timeout longer
		}

		m_fTimeOut -= dt;
		if(m_fTimeOut>0.f)
			return false;
		m_bIsBursting = true;
		m_BurstShotsCount = SelectBurstLength();

	}
	else 
	{ 
		if(m_BurstShotsCount<0)
		{
			m_bIsBursting = false;
			m_fTimeOut = SelectBurstTimeOut();
			return false;
		}
	}
	// If aiming is not ready, do not shoot.
	EAimState	aim = m_pShooter->GetAimState();
	if(aim != AI_AIM_READY && aim != AI_AIM_FORCED)
	{
		dbg_IsAiming = false;
		return false;
	}

	SAIWeaponInfo	weaponInfo;
	m_pShooter->GetProxy()->QueryWeaponInfo(weaponInfo);
	m_fLastBulletOutTime += dt;



	bool	needNextShootNow(canFire && (weaponInfo.shotIsDone || m_JustReset || m_fLastBulletOutTime>.73f));

	static float bulletTimeOut(5.f);
	if(m_nDrawFireCounter>0)
		needNextShootNow = canFire && (weaponInfo.shotIsDone || m_fLastBulletOutTime>bulletTimeOut);

	m_JustReset = false;

	if(needNextShootNow)
	{
		m_fLastBulletOutTime = 0;
		m_bIsTriggerDown = DoNextShot(pTarget, canFire, fireMode, descriptor, outShootTargetPos);

		if(m_bIsTriggerDown)
			--m_BurstShotsCount;
		//		return DoNextShot(pTarget, canFire, fireMode, fAccuracy,descriptor, outShootTargetPos);
	}
	else
		DoThisShot(outShootTargetPos);

	if(m_bIsTriggerDown)
	{
		m_fFadingToNormalTime -= dt;
		m_LastFirePos = outShootTargetPos;
	}
	return m_bIsTriggerDown;
}

void CFireCommandInstantDeprecated::DebugDraw(struct IRenderer *pRenderer)
{
}


//
//====================================================================
bool CFireCommandInstantDeprecated::UpdateTriggerPressing(bool canFire, EFireMode fireMode, float dt)
{
	if(m_bFireTriggerDown)
	{
		m_fFireTriggerDownTime += dt;
		m_fFireTriggerUpTime = 0.f;	
	}
	else
	{
		m_fFireTriggerUpTime += dt;
		m_fFireTriggerDownTime = 0.f;	
	}

	if(canFire)
	{	
		if(m_fFireTriggerDownTime > m_fBurstLength && (fireMode != FIREMODE_CONTINUOUS && fireMode != FIREMODE_FORCED)) 
		{
			m_bFireTriggerDown = false; 
		}
		else if(!m_bFireTriggerDown && m_fFireTriggerUpTime > .1f)
		{
			m_bFireTriggerDown = true;
			m_fBurstLength = SelectBurstLengthOld();
		}
	}
	else
	{
		m_bFireTriggerDown = false;
		return false;
	}
	return true;
}


//
//====================================================================
float CFireCommandInstantDeprecated::SelectBurstTimeOut()
{
	const float	minTime(.1f);
	const float	maxTime(.8f);

	float aggressionScale = 1.0f; //m_pShooter->GetParameters().m_fAggression;
//	if(!m_pShooter->IsAllowedToHitTarget())
//		aggressionScale *= GetAISystem()->GetCurrentDifficultyProfile()->ambientAggressionMod;
	aggressionScale = 1.0f - aggressionScale;

	float	timeOut(minTime + aggressionScale * 0.7f + Random(0.1f));
	//return minTime + aggressionScale * 0.7f + Random(0.1f);

	// scale it down with distance - the closer target - the smaller timeout - more shooting
	const float	distTrh(10.f);

	float targetDist((m_LastFireTargetSeenPos-m_pShooter->GetPos()).GetLength());
	if(targetDist > distTrh)
		return timeOut;
	timeOut = (timeOut/distTrh)*targetDist;
	return timeOut;
}

//
//====================================================================
int CFireCommandInstantDeprecated::SelectBurstLength()
{
	const int	maxLenght(8);
	int	rndVal(Random(100));
	int burstLength(1);

	float aggressionScale = 2.0f; //m_pShooter->GetParameters().m_fAggression * 2.0f;
//	if(!m_pShooter->IsAllowedToHitTarget())
//		aggressionScale *= GetAISystem()->GetCurrentDifficultyProfile()->ambientAggressionMod;

	if(rndVal<40)
		burstLength = (int)(3.f*aggressionScale);
	else if(rndVal<80)
		burstLength = (int)(5.f*aggressionScale);
	else 
		burstLength = (int)(7.f*aggressionScale);
	return clamp_tpl(burstLength, 2, maxLenght);
}

//
//====================================================================
float CFireCommandInstantDeprecated::SelectBurstLengthOld()
{
	const float	singleShotLenght(.1f);
	const float	maxLenght(2.f);
	int	rndVal(Random(100));
	float	fBurstLength(1.f);
	float aggressionScale(2.0f); //m_pShooter->GetParameters().m_fAggression*2.f);
	if(rndVal<40)
		fBurstLength = 3.f*aggressionScale;
	else if(rndVal<80)
		fBurstLength = 5.f*aggressionScale;
	else 
		fBurstLength = 7.f*aggressionScale;
	fBurstLength *= singleShotLenght;
	return clamp(fBurstLength, singleShotLenght, maxLenght);
}


//====================================================================
// CFireCommandInstant
//====================================================================
CFireCommandInstant::CFireCommandInstant(IAIActor* pShooter) : 
	m_pShooter(((CAIActor*)pShooter)->CastToCPuppet()),
	m_weaponBurstState(BURST_NONE),
	m_weaponBurstTime(0),
	m_weaponBurstTimeScale(1),
	m_weaponBurstBulletCount(0),
	m_curBurstBulletCount(0),
	m_curBurstPauseTime(0),
	m_weaponTriggerTime(0),
	m_weaponTriggerState(true),
	m_ShotsCount(0)
{
}


//
//--------------------------------------------------------------------------------------------------------
void CFireCommandInstant::Reset()
{
	AIAssert(m_pShooter);
	m_weaponBurstState = BURST_NONE;
	m_weaponBurstTime = 0;
	m_weaponBurstTimeScale = 1;
	m_weaponBurstBulletCount = 0;
	m_curBurstBulletCount = 0;
	m_curBurstPauseTime = 0;
	m_weaponTriggerTime = 0;
	m_weaponTriggerState = true;
}

//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandInstant::Update(IAIObject* pITarget, bool canFire, EFireMode fireMode, 
																 const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	float dt = GetAISystem()->GetFrameDeltaTime();

	m_coverFireTime = descriptor.coverFireTime * GetAISystem()->m_cvRODCoverFireTimeMod->GetFVal();

	if (!canFire)
	{
		m_weaponBurstState = BURST_NONE;
		return false;
	}

	if (!pITarget)
	{
		m_weaponBurstState = BURST_NONE;
		return false;
	}

	EAimState	aim = m_pShooter->GetAimState();
	if (aim != AI_AIM_READY && aim != AI_AIM_FORCED)
	{
		m_weaponBurstState = BURST_NONE;
		return false;
	}

	if (m_pShooter->m_targetLostTime > m_coverFireTime &&
			fireMode != FIREMODE_CONTINUOUS && fireMode != FIREMODE_FORCED &&
			fireMode != FIREMODE_PANIC_SPREAD && fireMode != FIREMODE_KILL)
	{
		m_weaponBurstState = BURST_NONE;
		return false;
	}

	const int burstBulletCountMin = descriptor.burstBulletCountMin;
	const int burstBulletCountMax = descriptor.burstBulletCountMax;
	const float burstPauseTimeMin = descriptor.burstPauseTimeMin;
	const float burstPauseTimeMax = descriptor.burstPauseTimeMax;
	const float singleFireTriggerTime = descriptor.singleFireTriggerTime;

	bool singleFireMode = singleFireTriggerTime > 0.0f;

	if (singleFireMode)
	{
		m_weaponTriggerTime -= dt;
		if (m_weaponTriggerTime < 0.0f)
		{
			m_weaponTriggerState = !m_weaponTriggerState;
			m_weaponTriggerTime += singleFireTriggerTime * (0.75f + ai_frand()*0.5f);
		}
	}
	else
	{
		m_weaponTriggerTime = 0.0f;
		m_weaponTriggerState = true;
	}

	// Burst control
	if (fireMode == FIREMODE_BURST || fireMode == FIREMODE_BURST_DRAWFIRE ||
		fireMode == FIREMODE_BURST_WHILE_MOVING || fireMode == FIREMODE_BURST_SNIPE)
	{
		float distScale = 1.0f;
		switch (m_pShooter->m_targetZone)
		{
		case CPuppet::ZONE_KILL: distScale = 1.0f; break;
		case CPuppet::ZONE_COMBAT_NEAR: distScale = 0.9f; break;
		case CPuppet::ZONE_COMBAT_FAR: distScale = 0.9f; break;
		case CPuppet::ZONE_WARN: distScale = 0.4f; break;
		case CPuppet::ZONE_OUT: distScale = 0.0f; break;
		default: distScale = 0.0f; break;
		}

		const float	lostFadeMinTime = m_coverFireTime * 0.25f;
		const float	lostFadeMaxTime = m_coverFireTime;
		const int fadeSteps = 6;

		float a = 1.0f;
		float	fade = clamp((m_pShooter->m_targetLostTime - lostFadeMinTime) / (lostFadeMaxTime-lostFadeMinTime), 0.0f, 1.0f);
		a *= 1.0f - floorf(fade*fadeSteps)/(float)fadeSteps;	// more lost, less bullets
		a *= distScale;	// scaling based on the zone (distance)
		a *= m_pShooter->IsAllowedToHitTarget() ? 1.0f : 0.2f;
		m_curBurstBulletCount = (int)(burstBulletCountMin + (burstBulletCountMax - burstBulletCountMin) * a * m_weaponBurstTimeScale);
		m_curBurstPauseTime = burstPauseTimeMin + (1.0f - sqr(a)*0.75f) * (burstPauseTimeMax - burstPauseTimeMin) * m_weaponBurstTimeScale;

		if (m_weaponBurstState == BURST_NONE)
		{
			// Init
			m_weaponBurstTime = 0.0f;
			m_weaponBurstTimeScale = (1 + ai_rand() % 6) / 6.0f;
			m_weaponBurstState = BURST_FIRE;
			m_weaponBurstBulletCount = 0;

			// When starting burst in warn zone, force first bullets to miss
			// TODO: not nice to handle it this way!
			if (m_pShooter->m_targetZone == CPuppet::ZONE_WARN)
				m_pShooter->m_targetSeenTime = max(0.0f, m_pShooter->m_targetSeenTime - (1 + ai_rand() % 3)/10.0f);

			// Reset
			m_pShooter->GetProxy()->GetAndResetShotBulletCount();
		}
		else if (m_weaponBurstState == BURST_FIRE)
		{
			m_weaponBurstBulletCount += m_pShooter->GetProxy()->GetAndResetShotBulletCount();
			if (m_weaponBurstBulletCount > m_curBurstBulletCount)
			{
				if (m_curBurstPauseTime > 0.0f)
					m_weaponBurstState = BURST_PAUSE;
			}
		}
		else
		{
			// Wait
			m_weaponBurstTime += dt;
			if (m_weaponBurstTime > m_curBurstPauseTime)
				m_weaponBurstState = BURST_NONE;
		}
	}
	else
	{
		// Allow to fire always.
		m_weaponBurstState = BURST_FIRE;
	}

	if (m_weaponBurstState != BURST_FIRE)
	{
		m_weaponTriggerTime = 0;
		m_weaponTriggerState = true;
		return false;
	}

	if (!m_weaponTriggerState)
		return false;

	Vec3 adjustedTargetPos(outShootTargetPos);
	float	clampangle = DEG2RAD(30.0f);

	EntityId vehicleId = m_pShooter->GetProxy()->GetLinkedVehicleEntityId();
	if (vehicleId)
		clampangle = DEG2RAD(15.0f);

	if (m_pShooter->CheckAndAdjustFireTarget(outShootTargetPos, descriptor.spreadRadius, adjustedTargetPos, clampangle, -1.0f))
	//	if (m_pShooter->CheckAndAdjustFireTarget(outShootTargetPos, 0.25f, outShootTargetPos, DEG2RAD(30.0f), -1.0f))
	{
		outShootTargetPos = adjustedTargetPos;
		return true;
	}

	return false;
}

void CFireCommandInstant::DebugDraw(struct IRenderer *pRenderer)
{
	if (!m_pShooter) return;

	const float	lostFadeMinTime = m_coverFireTime * 0.25f;
	const float	lostFadeMaxTime = m_coverFireTime;
	const int fadeSteps = 6;

	float a = 1.0f;
	float	fade = clamp((m_pShooter->m_targetLostTime - lostFadeMinTime) / (lostFadeMaxTime-lostFadeMinTime), 0.0f, 1.0f);
	a *= 1.0f - floorf(fade*fadeSteps)/(float)fadeSteps;	// more lost, less bullets

	pRenderer->DrawLabel(m_pShooter->GetFirePos() - Vec3(0,0,1.5f), 1, "Weapon\nShot:%d/%d\nWait:%.2f/%.2f\nA=%f", m_weaponBurstBulletCount, m_curBurstBulletCount, m_weaponBurstTime, m_curBurstPauseTime, a);
}



//====================================================================
// CFireCommandProjectileSlow
//====================================================================
CFireCommandProjectileSlow::CFireCommandProjectileSlow(IAIActor* pShooter):
	m_pShooter((CAIActor*)pShooter)
{

}


void CFireCommandProjectileSlow::Reset()
{
}

bool CFireCommandProjectileSlow::Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
																				const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	AIAssert(m_pShooter);
	AIAssert(GetAISystem());

	if (!pTarget)
		return false;

	if (!canFire)
		return false;

	float frameTime = GetAISystem()->GetFrameDeltaTime();
	Vec3	vTargetPos(pTarget->GetPos());
	Vec3	vStartDir(vTargetPos - m_pShooter->GetFirePos());
	vStartDir.z = 0.f;
	vStartDir.normalize();
	vStartDir.z = .5f;
	vStartDir.normalize();

	outShootTargetPos = m_pShooter->GetFirePos() + vStartDir*10.f;

	SAIBodyInfo bodyInfo;
	m_pShooter->GetProxy()->QueryBodyInfo( bodyInfo);

	if(vStartDir.Dot(bodyInfo.vFireDir)<.9f)
		canFire = false;

	return canFire;
}


//====================================================================
// CFireCommandProjectileSlow
//====================================================================
CFireCommandProjectileFast::CFireCommandProjectileFast(IAIActor* pShooter):
	m_pShooter(((CAIActor*)pShooter)->CastToCPuppet()),
	m_aimPos(0,0,0),
	m_ShotCounter(0)
{
}


void CFireCommandProjectileFast::Reset()
{
	m_steadyTime = 0.0f;
}

//
//
//
//----------------------------------------------------------------------------------------------------------------
bool CFireCommandProjectileFast::Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
																				const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	AIAssert(m_pShooter);
	AIAssert(GetAISystem());

	SAIWeaponInfo	weaponInfo;
	m_pShooter->GetProxy()->QueryWeaponInfo(weaponInfo);

	if (!canFire || weaponInfo.shotIsDone || !pTarget)
	{
		m_steadyTime = 0.f;
		return false;
	}

	EAimState	aim = m_pShooter->GetAimState();
	if (aim != AI_AIM_READY && aim != AI_AIM_FORCED)
		return false;

	m_targetPos = pTarget->GetPos();
	m_targetVel = pTarget->GetVelocity();
	const float aimAheadTime(0.5f);
	m_aimPos = m_targetPos + m_targetVel * aimAheadTime;
	outShootTargetPos = m_aimPos;

	if(!NoFriendNearTarget(descriptor.fDamageRadius, pTarget->GetPos()))
		return false;
	// try different missing points	
	if(ChooseShootPoint(pTarget, descriptor.fDamageRadius*.4, outShootTargetPos, CFireCommandProjectileFast::MISS_INFRONT))
		return true;
	outShootTargetPos = m_aimPos;
	if(ChooseShootPoint(pTarget, descriptor.fDamageRadius*.8, outShootTargetPos, CFireCommandProjectileFast::MISS_SIDES))
		return true;
	outShootTargetPos = m_aimPos;
	if(ChooseShootPoint(pTarget, descriptor.fDamageRadius*.8, outShootTargetPos, CFireCommandProjectileFast::MISS_BACK))
		return true;

	// advance fire drawing
	//AdvanceFireDraw();
	return false;

/*
	float dt = GetAISystem()->GetFrameDeltaTime();
	float frameTime = GetAISystem()->GetFrameDeltaTime();
	Vec3	vTargetPos(pTarget->GetPos());
	Vec3	vStartDir(vTargetPos - m_pShooter->GetFirePos());
	vStartDir.z = 0.f;
	vStartDir.normalize();
	vStartDir.z = .5f;
	vStartDir.normalize();

	outShootTargetPos = m_pShooter->GetFirePos() + vStartDir*10.f;*/
/*
	m_targetPos = pTarget->GetPos();
	m_targetVel = pTarget->GetVelocity();
	const float aimAheadTime(0.5f);
	m_aimPos = m_targetPos + m_targetVel * aimAheadTime;

	outShootTargetPos = m_aimPos;

	// Check if we can hit the target.
	ray_hit hit;
	IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
	Vec3	dir(m_aimPos - m_pShooter->GetFirePos());
	IPhysicalEntity *pTargetPhys(NULL);
	if(pTarget->GetProxy() && pTarget->GetProxy()->GetPhysics())
		pTargetPhys = pTarget->GetProxy()->GetPhysics();
	int rwiResult = pWorld->RayWorldIntersection(m_pShooter->GetFirePos(), dir, ent_terrain|ent_static, HIT_COVER, &hit, 1, pTargetPhys);
	CPipeUser *pShooterPipeUser(m_pShooter->CastToCPipeUser());
	// if firced t 
	if(pShooterPipeUser && pShooterPipeUser->GetFireMode()==FIREMODE_FORCED)
	{
		if (rwiResult == 0 || hit.dist > 7.f)
			m_steadyTime += dt;
	}
	else
		if (rwiResult == 0 || hit.dist > dir.len() * 0.7f)
			m_steadyTime += dt;

	if(m_steadyTime > 1.0f && NoFriendNearTarget(descriptor.fDamageRadius, pTarget->GetPos()))
		return true;

	return false;
*/
}

//
//
//
//----------------------------------------------------------------------------------------------------------------
bool CFireCommandProjectileFast::ChooseShootPoint(IAIObject* pTarget, float missDist, Vec3& shootAtPos, CFireCommandProjectileFast::EMissMode missMode)
{
	if(m_ShotCounter==2 && Random(0,100)>50)
		missMode = MISS_HITFACE;

	// deal with eye-height
	// make sure we shoot not in head, but at feet
	if(missMode==MISS_INFRONT && pTarget->GetAIType()!=AIOBJECT_VEHICLE)
	{
		SAIBodyInfo bodyInfo;
		IAIActor *pTargetActor(CastToIAIActorSafe(pTarget));
		if (pTargetActor && pTargetActor->GetProxy())
		{
			pTargetActor->GetProxy()->QueryBodyInfo(bodyInfo);
			shootAtPos.z -= bodyInfo.stanceSize.GetSize().z;
		}
	}

	Vec3	targetForward2D(pTarget->GetBodyDir());
	targetForward2D.z = 0.f;
	// if target looks down - use direction to shooter as forward
	if(targetForward2D.IsZero(.07f))
	{
		targetForward2D = m_pShooter->GetPos() - pTarget->GetPos();
		targetForward2D.z = 0.f;
	}
	targetForward2D.Normalize();

	static float	offsetScale[]={.9f, .5f, .1f};
	Vec3	randomOffset(Random(-1.f,1.f),Random(-1.f,1.f),0.f);

	switch (missMode)
	{
		case CFireCommandProjectileFast::MISS_HITFACE:
			{
				shootAtPos = pTarget->GetPos();
				break;
			}
		case CFireCommandProjectileFast::MISS_INFRONT:
			{
			float	leftRightOffset(Random(-1.f,1.f));
			leftRightOffset*=.2f;
			shootAtPos += targetForward2D*missDist*offsetScale[m_ShotCounter] + 
											Vec3(-targetForward2D.y, targetForward2D.x, 0.f)*missDist*offsetScale[m_ShotCounter]*leftRightOffset;
			break;
			}
		case CFireCommandProjectileFast::MISS_SIDES:
			{
			float	leftRightOffset( Random(0,100)<50 ? Random(-1.f,-.5f) : Random(1.f,.5f));
			leftRightOffset*=.4f;
			shootAtPos += targetForward2D*missDist*offsetScale[m_ShotCounter]*.1f + 
				Vec3(-targetForward2D.y, targetForward2D.x, 0.f)*missDist*offsetScale[m_ShotCounter]*leftRightOffset;
			break;
			}
		case CFireCommandProjectileFast::MISS_BACK:
			{
			float	leftRightOffset( Random(0,100)<50 ? Random(-1.f,-.5f) : Random(1.f,.5f));
			leftRightOffset*=.1f;
			shootAtPos += -targetForward2D*missDist*offsetScale[m_ShotCounter] + 
				Vec3(-targetForward2D.y, targetForward2D.x, 0.f)*missDist*offsetScale[m_ShotCounter]*leftRightOffset;
			break;
			}
	}
	bool	goodSpot=ValidateFireDirection(shootAtPos-m_pShooter->GetFirePos(), false);
	if(goodSpot)
		m_aimPos = shootAtPos;
	return goodSpot;
}

//
//	we have candidate shooting direction, let's check if it is good
//
//----------------------------------------------------------------------------------------------------------------
bool CFireCommandProjectileFast::ValidateFireDirection(const Vec3& fireVector, bool mustHit)
{
// Check if we can hit the target.
	ray_hit hit;
	IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
	int rwiResult = pWorld->RayWorldIntersection(m_pShooter->GetFirePos(), fireVector, 
													ent_all , HIT_COVER, &hit, 1);

	return rwiResult == 0 || hit.dist > 7.f;
/*
	CPipeUser *pShooterPipeUser(m_pShooter->CastToCPipeUser());
// if firced t 
if(pShooterPipeUser && pShooterPipeUser->GetFireMode()==FIREMODE_FORCED)
{
	if (rwiResult == 0 || hit.dist > 7.f)
		m_steadyTime += dt;
}
else
if (rwiResult == 0 || hit.dist > dir.len() * 0.7f)
m_steadyTime += dt;
*/
}


//
//
//
//----------------------------------------------------------------------------------------------------------------
bool CFireCommandProjectileFast::NoFriendNearTarget(float explRadius, const Vec3& targetPos)
{
float	explRadius2(explRadius*explRadius);
int species = m_pShooter->GetParameters().m_nSpecies;
	//	AutoAIObjectIter it(GetAISystem()->GetFirstAIObject(IAISystem::OBJFILTER_SPECIES, species));
	AutoAIObjectIter it(GetAISystem()->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIOBJECT_PUPPET));
	bool	friendOnWay(false);
	for(; it->GetObject(); it->Next())
	{
		IAIObject*	pObject = it->GetObject();
		// skip desabled
		if(!pObject->IsEnabled())
			continue;
		// check only friends/neutrals
		if(m_pShooter->IsHostile(pObject))
			continue;
		float dist2((pObject->GetPos()-targetPos).GetLengthSquared());
		if(dist2<explRadius2)
			return false;
	}
	return true;
}


//
//
//
//----------------------------------------------------------------------------------------------------------------
void CFireCommandProjectileFast::DebugDraw(IRenderer *pRenderer)
{
	if(!m_pShooter)
		return;
	int	a(255 - (int)(clamp(m_steadyTime/2.0f, 0.0f, 1.0f)*255.0f));
	ColorB	col(255,a,a,128);
	ColorB	col2(255,255,255,255);
	col2.a = 128;
	pRenderer->GetIRenderAuxGeom()->DrawLine(m_pShooter->GetFirePos()+Vec3(0,0,0.25f), col, m_aimPos+Vec3(0,0,0.25f), col2);
}

//====================================================================
// CFireCommandProjectileFast EXP-1
//====================================================================
CFireCommandProjectileXP::CFireCommandProjectileXP(IAIActor* pShooter):
	CFireCommandProjectileFast(pShooter),
		m_weaponTriggerTime(0),
		m_weaponTriggerState(false)
{
}

void CFireCommandProjectileXP::Reset()
{
	CFireCommandProjectileFast::Reset();
	m_weaponTriggerTime = 0;
	m_weaponTriggerState = false;
}

bool CFireCommandProjectileXP::Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
																				const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	bool res=CFireCommandProjectileFast::Update(pTarget, canFire, fireMode, descriptor, outShootTargetPos);

	// Add refire restrictions
	const float singleFireTriggerTime = descriptor.singleFireTriggerTime;
	bool singleFireMode = singleFireTriggerTime > 0.0f;

	if (singleFireMode)
	{
		m_weaponTriggerTime -= GetAISystem()->GetFrameDeltaTime();
		if (m_weaponTriggerTime < 0.0f)
		{
			m_weaponTriggerState = true;
			m_weaponTriggerTime += singleFireTriggerTime * (0.75f + ai_frand()*0.5f);
		}
	}
	else
	{
		m_weaponTriggerTime = 0.0f;
		m_weaponTriggerState = true;
	}

	if (res && m_weaponTriggerState)
	{
		m_weaponTriggerState=false;
		return true;
	}

	return false;
}

//====================================================================
// CFireCommandStrafing
//====================================================================
CFireCommandStrafing::CFireCommandStrafing(IAIActor* pShooter):
	m_pShooter(((CAIActor*)pShooter)->CastToCPuppet())
{
	CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
	m_fLastStrafingTime = fCurrentTime;
	m_fLastUpdateTime = fCurrentTime;
	m_StrafingCounter =0;
}


//
//
//
//----------------------------------------------------------------------------------------------------------------
void CFireCommandStrafing::Reset()
{
	AIAssert(m_pShooter);
	CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
	m_fLastStrafingTime = fCurrentTime;
	m_fLastUpdateTime = fCurrentTime;
	m_StrafingCounter =0;
}

//
//
//
//----------------------------------------------------------------------------------------------------------------
bool CFireCommandStrafing::ManageFireDirStrafing(IAIObject* pTarget, Vec3& vTargetPos, bool& fire, const AIWeaponDescriptor& descriptor)
{
	fire = false;
	
	const AgentParameters&	params = m_pShooter->GetParameters();

	CPuppet*	pShooterPuppet = m_pShooter->CastToCPuppet();
	if(!pShooterPuppet)
		return false;	

	if (!m_pShooter->GetProxy())
		return false;

	SAIBodyInfo bodyInfo;
	m_pShooter->GetProxy()->QueryBodyInfo(bodyInfo);

	bool	strafeLoop(false);
	int	maxStrafing	=(int)params.m_fStrafingPitch;
	if ( maxStrafing == 0 )
		maxStrafing = 1;

	if ( maxStrafing < 0 )
	{
		maxStrafing = -maxStrafing;
		strafeLoop = true;
		if ( maxStrafing * 2 <= m_StrafingCounter )
			m_StrafingCounter = 0;
	}

	Vec3 vActualFireDir	= bodyInfo.vFireDir;
	if (m_StrafingCounter==0)
	{
		CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
		m_fLastStrafingTime = fCurrentTime;
		m_fLastUpdateTime = fCurrentTime;
		m_StrafingCounter ++;
	}
	if ( strafeLoop || maxStrafing * 2 > m_StrafingCounter )
	{
		
		CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
		float deltaTime = (fCurrentTime - m_fLastUpdateTime).GetSeconds();
		m_fLastUpdateTime = fCurrentTime;

		// { t : (X-C)N=0,X=P+tV,V=N } <-> t=(C-P)N 

		Vec3	N,C,P,X,V,V2;
		float	t,d,s;

		s = max( 30.0f, min( descriptor.fSpeed, 100.0f )) ;
		N = Vec3(0.0,0.0,1.0f);
		C = vTargetPos-Vec3(0.0f,0.0f,0.3f);
		P = m_pShooter->GetFirePos();
		V = m_pShooter->GetVelocity();
		V2 = pTarget->GetVelocity();
		P += V2 * 2.0;
		d = (C-P).GetLength();
		t = (C-P).Dot(N);

		if (fabs(t)>0.01f)	//means if we have enough distance to the target.
		{
			X = N;
			X.SetLength(t);
			X +=P;

			Vec3	vStrafingDirUnit = X-C;
			float	fStrafingLen = vStrafingDirUnit.GetLength();
			Limit( fStrafingLen, 1.0f, 30.0f );

			float r = (float)m_StrafingCounter /(float)maxStrafing;

			float fStrafingOffset =((float)(maxStrafing-m_StrafingCounter))*fStrafingLen/(float)maxStrafing;
			if ( fStrafingOffset < 0 )
				fStrafingOffset =0.0f;

			vTargetPos = vStrafingDirUnit;
			vTargetPos.SetLength(fStrafingOffset);
			vTargetPos +=C;

			Vec3 vShootDirection = vTargetPos-P;
			vShootDirection.NormalizeSafe();

			float dotUp = 1.0f;
			float tempSolution = 30.0f;
			if	( pShooterPuppet->GetSubType() == CAIObject::STP_2D_FLY )
				tempSolution = 60.0f; 

			if ( bodyInfo.linkedVehicleEntity && bodyInfo.linkedVehicleEntity->GetAI() )
			{
				Matrix33 wm( bodyInfo.linkedVehicleEntity->GetWorldTM() );
				Vec3 vUp = wm.GetColumn(2);
				float dotUp = vUp.Dot( Vec3(0.0f,0.0f,1.0f) );
			}
/*
			{
				GetAISystem()->AddDebugLine(m_pShooter->GetFirePos(), vTargetPos, 0, 0, 255, 0.1f);
			}
*/
			if (pShooterPuppet->GetSubType() == CAIObject::STP_2D_FLY)
			{
			}
			else
			{
				pShooterPuppet->m_State.vAimTargetPos = vTargetPos;
			}

			bool rejected = false;
			if ( dotUp > cry_cosf(DEG2RAD(tempSolution)) )
			{
				float dot = vShootDirection.Dot(vActualFireDir);

				if ( dot > cry_cosf(DEG2RAD(tempSolution)) )
				{
					if (pShooterPuppet->GetSubType() == CAIObject::STP_2D_FLY)
					{
						if (pShooterPuppet->CheckAndAdjustFireTarget(vTargetPos, descriptor.spreadRadius, vTargetPos, -1, -1))
							fire = true;
						else
							rejected = true;
					}
					else
					{
						if (pShooterPuppet->CheckAndAdjustFireTarget(vTargetPos, descriptor.spreadRadius, vTargetPos, -1, -1))
						{
							fire = true;
						}
						else
							rejected = true;
					}
				}
			}
/*
			if ( fire== true )
			{
				Vec3 v1 = m_pShooter->GetFirePos();
				GetAISystem()->AddDebugLine(v1, vTargetPos, 255, 255, 255, 0.1f);
				Vec3 v2 =vActualFireDir * 100.0f;
				GetAISystem()->AddDebugLine(v1, v1 + v2, 255, 0, 0, 0.1f);
			}
*/
			if ( rejected == true )
			{
				if ( (fCurrentTime - m_fLastStrafingTime).GetSeconds() > 0.8f )
				{
					m_fLastStrafingTime = fCurrentTime;
					m_StrafingCounter ++;
				}
			}
			if ( (fCurrentTime - m_fLastStrafingTime).GetSeconds() > 0.08f )
			{
				m_fLastStrafingTime = fCurrentTime;
				m_StrafingCounter ++;
			}
		}
	}

	return false;
}

//
//
//
//----------------------------------------------------------------------------------------------------------------
bool CFireCommandStrafing::Update(IAIObject* pTargetSrc, bool canFire, EFireMode fireMode,
																	const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	AIAssert(m_pShooter);
	AIAssert(GetAISystem());

	if(!canFire)
	{
		Reset();
		return false;
	}

	if (!pTargetSrc)
	{
		Reset();
		return false;
	}

	CPuppet* pShooterPuppet = m_pShooter->CastToCPuppet();
	if (!pShooterPuppet)
	{
		Reset();
		return false;
	}

	CAIObject* pTarget = (CAIObject*)pTargetSrc;

	if(pTarget->GetAssociation())
		pTarget = (CAIObject*)pTarget->GetAssociation();

	Vec3	vTargetPos = pTarget->GetPos();

	if ( pTarget && pTarget->GetProxy() )
	{
		int targetType = pTarget->GetType();
		if ( targetType == AIOBJECT_VEHICLE )
		{
			SAIBodyInfo	bodyInfo;
			pTarget->GetProxy()->QueryBodyInfo(bodyInfo);

			if ( bodyInfo.linkedDriverEntity && bodyInfo.linkedDriverEntity->GetAI() )
			{
				pTarget = (CAIObject*)bodyInfo.linkedDriverEntity->GetAI();
				vTargetPos = pTarget->GetPos();
				vTargetPos.z -= 2.0f;
			}

		}
	}

	bool	fire = false;

	ManageFireDirStrafing(pTarget, vTargetPos, fire, descriptor);
	outShootTargetPos = vTargetPos;

	return fire;
}

//====================================================================
// CFireCommandFastLightMOAR
//====================================================================
CFireCommandFastLightMOAR::CFireCommandFastLightMOAR(IAIActor* pShooter):
	m_pShooter((CAIActor*)pShooter)
{
}

void CFireCommandFastLightMOAR::Reset()
{
	m_startTime = GetAISystem()->GetFrameStartTime();
	m_startFiring = false;
}

//
//
//
//----------------------------------------------------------------------------------------------------------------
bool CFireCommandFastLightMOAR::Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
		const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{

	AIAssert(m_pShooter);
	AIAssert(GetAISystem());

	if(!pTarget)
		return false;

	const float	chargeTime = descriptor.fChargeTime;
	float	drawTime = descriptor.drawTime;

	float	dt = GetAISystem()->GetFrameDeltaTime();
	float elapsedTime = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds();
	Vec3	targetPos(pTarget->GetPos());
	Vec3	dirToTarget(targetPos - m_pShooter->GetFirePos());

	float distToTarget = dirToTarget.GetLength();
	float sightRange2 = m_pShooter->GetParameters().m_PerceptionParams.sightRange/2;
	if(sightRange2 > 0 && distToTarget > sightRange2)
		drawTime*= 1+(distToTarget - sightRange2)/sightRange2;
	// The moment it is
	if(canFire && !m_startFiring)
	{
		m_laggedTarget = m_pShooter->GetFirePos() + m_pShooter->GetViewDir() * distToTarget;
		m_startFiring = true;
		m_startDist = distToTarget;
		m_rand.set(Random(-1, 1), Random(-1, 1));
	}

	if(m_startFiring)
	{
		float	f = min(dt * 2.0f, 1.0f);
		m_laggedTarget += (targetPos - m_laggedTarget) * f;

		float	t = 0;

		if(elapsedTime < chargeTime)
		{
			// Wait until the weapon has charged
			t = 0;
		}
		else if(elapsedTime < chargeTime + drawTime)
		{
			t = ((elapsedTime - chargeTime) / drawTime);
		}
		else
		{
			t = 1.0f;
		}

		/*
		// Spiral draw fire.
		Matrix33	basis;
		basis.SetRotationVDir(dirToTarget.GetNormalizedSafe());
		// Wobbly offset
		float	oa = sinf(cosf(t * gf_PI * 33.321f) * gf_PI + t * gf_PI * 45.0f) * cosf(t * gf_PI * 13.1313f) + cosf(t * gf_PI * 6.1313f) * cosf(t * gf_PI * 4.1313f);
		float	or = cosf(sinf(t * gf_PI * 22.521f) * gf_PI + t * gf_PI * 64.1f) * sinf(t * gf_PI * 9.1413f) + cosf(t * gf_PI * 7.3313f) * sinf(t * gf_PI * 3.6313f);

		float	ang = t * gf_PI * 4.0f + oa * DEG2RAD(4.0f);
		float	rad = (sqrtf(1.0f - t) * 6.0f) * (0.9f + or * 0.2f);

		Vec3	offset(sinf(ang) * rad, 0, -cosf(ang) * rad);

		outShootTargetPos = m_laggedTarget + basis * offset;
		*/

		// Line draw fire.
		Vec3	dir = m_laggedTarget - m_pShooter->GetFirePos();
//		dir.z = 0;
		float	dist = dir.GetLength();
		if(dist > 0.0f)
			dir /= dist;

		Matrix33	basis;
		basis.SetRotationVDir(dir);

		const float	drawLen = 20.0f;
		float	minDist = dist - drawLen;
		if(minDist < m_startDist * 0.5f) minDist = m_startDist * 0.5f;

		float	startHeight = ((minDist - dist) / drawLen) * drawLen / 5.0f; //(minDist - dist) / (dist - minDist) * (drawLen / 3.0f);

		//float	a = (sinf(t * gf_PI * 3.321f + m_rand.x * 0.23424f) + sinf(t * gf_PI * 17.321f + m_rand.y * 0.433221) * 0.2f) / 1.2f;
		float	a = ( t < 1 ? sinf((descriptor.sweepFrequency *t + m_rand.x) * gf_PI ) * descriptor.sweepWidth * (1-t*t) : 0);
		Vec3	h;
		h.x = a;//cosf(gf_PI/2 + a/2 * gf_PI/2) * startHeight;
		h.y = 0;
		h.z = (t-1)*(2+distToTarget/max(sightRange2,2.f));//m_startDist/(2.f*max(distToTarget,0.01f)) ;//sinf(gf_PI/2 + a/2 * gf_PI/2) * startHeight;
		h = h * basis;

		Vec3	start = m_laggedTarget + dir * (minDist - dist) + h; //basis.GetRow(2) * startHeight;
		Vec3	end = m_laggedTarget;

		GetAISystem()->AddDebugLine(m_laggedTarget + dir * (minDist - dist), m_laggedTarget, 0, 0, 0, 0.1f);

		GetAISystem()->AddDebugLine(start, end, 255, 255, 255, 0.1f);

		Vec3	shoot = start + (end - start) * t*t;
		GetAISystem()->AddDebugLine(shoot, shoot + basis.GetRow(2), 255, 0, 0, 0.1f);

		Vec3	wob;
		wob.x = sinf(t * gf_PI * 4.321f + m_rand.x) + cosf(t * gf_PI * 15.321f + m_rand.y * 0.433221) * 0.2f;
		wob.y = 0;
		wob.z = 0;//cosf(t * gf_PI * 3.231415f + m_rand.y) + sinf(t * gf_PI * 16.321f + m_rand.x * 0.333221) * 0.2f;
		wob = wob * basis;

		GetAISystem()->AddDebugLine(shoot, shoot + wob, 255, 0, 0, 0.1f);

		outShootTargetPos = shoot + wob * 0.5f * sqrtf(1 - t);

//		float	wobble = sinf(cosf(t * gf_PI * 33.321f) * gf_PI + t * gf_PI * 45.0f) * cosf(t * gf_PI * 13.1313f) + cosf(t * gf_PI * 6.1313f) * cosf(t * gf_PI * 4.1313f);

/*		Matrix33	basis;
		basis.SetRotationVDir(dirToTarget.GetNormalizedSafe());

		Vec3	wob;
		wob.x = sinf(cosf(t * gf_PI * 33.321f) * gf_PI + t * gf_PI * 45.0f) * cosf(t * gf_PI * 13.1313f) + cosf(t * gf_PI * 6.1313f) * cosf(t * gf_PI * 4.1313f);
		wob.y = 0;
		wob.z = cosf(sinf(t * gf_PI * 22.521f) * gf_PI + t * gf_PI * 64.1f) * sinf(t * gf_PI * 9.1413f) + cosf(t * gf_PI * 7.3313f) * sinf(t * gf_PI * 3.6313f);

		Vec3	pos = m_pShooter->GetFirePos();
		pos.z = m_laggedTarget.z;

		outShootTargetPos = m_laggedTarget + (advance * dir); // + basis * (wob * 0.2f);*/
	}

	return canFire;
}

//====================================================================
// CFireCommandHunterMOAR
//====================================================================
CFireCommandHunterMOAR::CFireCommandHunterMOAR( IAIActor* pShooter )
	: m_pShooter( (CAIActor*) pShooter )
{
}

bool CFireCommandHunterMOAR::Update( IAIObject* pTarget, bool canFire, EFireMode fireMode, 
	const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos )
{	
	if ( !pTarget )
		return false;

	float elapsedTime = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds() - descriptor.fChargeTime;
	float dt = GetAISystem()->GetFrameDeltaTime();

	const Vec3& targetPos = pTarget->GetPos();

	if ( !m_startFiring )
	{
		m_lastPos = m_targetPos = targetPos;
		m_startFiring = true;
	}

	if ( m_startFiring )
	{
		if ( elapsedTime < 0 )
		{
			// Wait until the weapon has charged
			return canFire;
		}

		const float oscillationFrequency = 1.0f; // GetPortFloat( pActInfo, 1 );
		const float oscillationAmplitude = 1.0f; // GetPortFloat( pActInfo, 2 );
		const float duration = 6.0f; // GetPortFloat( pActInfo, 3 );
		const float alignSpeed = 8.0f; // GetPortFloat( pActInfo, 4 );

		if ( dt > 0 )
		{
			// make m_lastPos follow m_targetPos with limited speed
			Vec3 aim = targetPos + (targetPos - m_targetPos) * 2.0f;
			m_targetPos = targetPos;
			Vec3 dir = aim - m_lastPos;
			float maxDelta = alignSpeed * dt;
			if ( maxDelta > 0 )
			{
				if ( dir.GetLengthSquared() > maxDelta*maxDelta )
					dir.SetLength( maxDelta );
			}
			m_lastPos += dir;
		}

		// a - vertical offset
		float a = min( 8.0f, max((2.5f - elapsedTime)*3.0f, -0.5f) );
		a += cos( elapsedTime * (100.0f * gf_PI/180.0f) ) * 1.5f; // add some high frequency low amplitude vertical oscillation

		// b - horizontal offset
		float b = sin( pow(elapsedTime, 1.4f) * oscillationFrequency * (2.0f*gf_PI) );
		b += sin( elapsedTime * 355.0f * (gf_PI/180.0f) ) * 0.1f;
		b *= max( 2.5f - elapsedTime, 0.0f ) * oscillationAmplitude;

		Vec3 fireDir = m_lastPos - m_pShooter->GetFirePos();
		Vec3 fireDir2d( fireDir.x, fireDir.y, 0 );
		Vec3 sideDir = fireDir.Cross( fireDir2d );
		Vec3 vertDir = sideDir.Cross( fireDir ).normalized();
		sideDir.NormalizeSafe();

		outShootTargetPos = m_lastPos - vertDir * a + sideDir * b;
	}

	return canFire;
}

//====================================================================
// CFireCommandHunterSweepMOAR
//====================================================================
CFireCommandHunterSweepMOAR::CFireCommandHunterSweepMOAR( IAIActor* pShooter )
: m_pShooter( (CAIActor*) pShooter )
{
}

bool CFireCommandHunterSweepMOAR::Update( IAIObject* pTarget, bool canFire, EFireMode fireMode, 
	const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos )
{	
	if ( !pTarget )
		return false;

	float elapsedTime = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds() - descriptor.fChargeTime;
	float dt = GetAISystem()->GetFrameDeltaTime();

	const Vec3& targetPos = pTarget->GetPos();

	if ( !m_startFiring )
	{
		m_lastPos = m_targetPos = targetPos;
		m_startFiring = true;
	}

	if ( m_startFiring )
	{
		if ( elapsedTime < 0 )
		{
			// Wait until the weapon has charged
			return canFire;
		}

		const float baseDist = 20.0f; // GetPortFloat( pActInfo, 1 );
		const float leftRightSpeed = 6.0f; // GetPortFloat( pActInfo, 2);
		const float width = 10.0f; // GetPortFloat( pActInfo, 3);
		const float nearFarSpeed = 7.0f; // GetPortFloat( pActInfo, 4);
		const float dist = 7.0f; // GetPortFloat( pActInfo, 5);
		const float duration = 10.0f; // GetPortFloat( pActInfo, 6);

		Matrix34 worldTransform = m_pShooter->GetEntity()->GetWorldTM();
		Vec3 fireDir = worldTransform.GetColumn1();
		Vec3 sideDir = worldTransform.GetColumn0();
		Vec3 upDir = worldTransform.GetColumn2();
		Vec3 pos = worldTransform.GetColumn3();

		// runs from 0 to 2*PI and indicates where in the 2*PI long cycle we are now
		float t = 2.0f*gf_PI * elapsedTime/duration;

		// NOTE Mrz 31, 2007: <pvl> essetially a Lissajous curve, with some
		// irregularity added by a higher frequency harmonic and offset vertically
		// because we don't want the hunter to shoot at the sky.
		outShootTargetPos = m_pShooter->GetPos() +
			baseDist * fireDir +
			dist  * sin (nearFarSpeed * t) * upDir  +
			0.2f * dist * sin (2.3f*nearFarSpeed * t + 1.0f/*phase shift*/) * upDir +
			-1.8f * dist * upDir +
			width * sin (leftRightSpeed * t) * sideDir +
			0.2 * width * sin (2.3f*leftRightSpeed * t + 1.0f/*phase shift*/) * sideDir;
	}

	return canFire;
}

//====================================================================
// CFireCommandHunterSingularityCannon
//====================================================================
CFireCommandHunterSingularityCannon::CFireCommandHunterSingularityCannon( IAIActor* pShooter )
: m_pShooter( (CAIActor*) pShooter )
{
}

bool CFireCommandHunterSingularityCannon::Update( IAIObject* pTarget, bool canFire, EFireMode fireMode, 
	const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos )
{	
	if ( !pTarget )
		return false;

	float elapsedTime = (GetAISystem()->GetFrameStartTime() - m_startTime).GetSeconds() - descriptor.fChargeTime;
	float dt = GetAISystem()->GetFrameDeltaTime();

	const Vec3& targetPos = pTarget->GetPos();
	outShootTargetPos = targetPos;

	if ( !m_startFiring )
	{
		m_targetPos = targetPos;
		m_startFiring = true;
	}

/*
	if ( m_startFiring )
	{
		if ( elapsedTime < 0 )
		{
			// Wait until the weapon has charged
			return canFire;
		}
	}
*/

	return canFire;
}

//---------------------------------------------------------------------------------------------------------------
//	
//			GRENADE fire command
//
//====================================================================
CFireCommandGrenade::CFireCommandGrenade(IAIActor* pShooter):
	m_pShooter(((CAIActor*)pShooter)->CastToCPuppet()),
	m_iter(0),
	m_targetPos(0,0,0),
	m_preferredHeight(0),
	m_throwDir(0,0,0),
	m_throwSpeedScale(0),
	m_bestScore(-1)
{

}

	//
	//---------------------------------------------------------------------------------------------------------------
CFireCommandGrenade::~CFireCommandGrenade()
{
	ClearDebugEvals();
}

//
//---------------------------------------------------------------------------------------------------------------
void CFireCommandGrenade::Reset()
{
	ClearDebugEvals();

	m_iter = 0;
	m_targetPos.Set(0,0,0);
	m_preferredHeight = 0.0f;
	m_throwDir.Set(0,0,0);
	m_throwSpeedScale = 0;
	m_bestScore = -1;
}

//
//---------------------------------------------------------------------------------------------------------------
void CFireCommandGrenade::ClearDebugEvals()
{
	for (unsigned i = 0, ni = m_DEBUG_evals.size(); i < ni; ++i)
		delete m_DEBUG_evals[i];
	m_DEBUG_evals.clear();
}

//
//---------------------------------------------------------------------------------------------------------------
bool	CFireCommandGrenade::Update(IAIObject* pTarget, bool canFire, EFireMode fireMode, 
										 const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
//	ClearDebugEvals();

	if (m_iter == 0)
	{
		// First iteration, setup tests

		// Calculate target offset.
		Vec3	targetDir2D(pTarget->GetPos() - m_pShooter->GetPos());
		targetDir2D.z = 0;
		float distToTarget = targetDir2D.NormalizeSafe();
		const float minRad = -distToTarget*0.1f;
		const float maxRad = distToTarget*0.1f;
		const float distScale = m_pShooter->GetParameters().m_fGrenadeThrowDistScale;
		float r0 = minRad + ai_frand() * (maxRad - minRad);
		float r1 = ai_frand() * maxRad + (1 - distScale)*distToTarget;
		Vec3 right(-targetDir2D.y, targetDir2D.x, 0);
		m_targetPos = pTarget->GetPos() + right * r0 - targetDir2D * r1;

		int nBuildingID;
		IVisArea *pArea;
		IAISystem::ENavigationType navType = GetAISystem()->CheckNavigationType(m_pShooter->GetPhysicsPos(), nBuildingID, pArea,
			m_pShooter->GetMovementAbility().pathfindingProperties.navCapMask);

		m_preferredHeight = navType == IAISystem::NAV_WAYPOINT_HUMAN ? 3.0f : 5.0f;
		m_bestScore = -1.0f;
	}

	// Flat target dir
	Vec3 targetDir2D = m_targetPos - m_pShooter->GetPos();
	targetDir2D.z = 0;
	const float distToTarget = targetDir2D.NormalizeSafe();

	const unsigned maxIterPerUpdate = 1;
	const unsigned maxIter = 4;

	for (unsigned i = 0; i < maxIterPerUpdate && m_iter < maxIter; ++i)
	{
		float u = (float)m_iter / (float)(maxIter-1) - 0.25f;
		Vec3 delta = m_targetPos - m_pShooter->GetFirePos();
		float x = targetDir2D.Dot(delta) - u * distToTarget * 0.25f;
		float y = delta.z;

		float h = max(1.0f, y + m_preferredHeight);

		const float g = -9.8f;

		// Try good solution
		float vy = sqrtf(-h*g);
		float det = sqr(vy) + 2*g*y;
		if (det < 0.0f)
			continue;
		float tx = (-vy - sqrtf(det)) / g;
		float vx = x / tx;

		Vec3 dir = targetDir2D * vx + Vec3(0,0,vy);
		float vel = dir.NormalizeSafe();

		float speed = 1.0f;
		float score = EvaluateThisThrow(m_targetPos, pTarget->GetViewDir(), dir, vel, speed, fireMode);
		if (score >= 0 && (score < m_bestScore || m_bestScore < 0))
		{
			m_throwDir = dir;
			m_throwSpeedScale = vel / speed;
			m_bestScore = score;
		}
		m_iter++;
	}

	if (m_iter >= maxIter)
	{
		if (m_bestScore <= 0)
			outShootTargetPos.Set(0,0,0);
		else
			outShootTargetPos = m_pShooter->GetFirePos() + m_throwDir * 10.0f;
		return true;
	}

	// Not ready yet.
	return false;
}

//
//---------------------------------------------------------------------------------------------------------------
float	CFireCommandGrenade::EvaluateThisThrow(const Vec3& targetPos, const Vec3& targetViewDir, const Vec3& dir, float vel, float& speedOut, EFireMode fireMode)
{
	IPuppetProxy	*pPuppetProxy;
	if (!m_pShooter->GetProxy()->QueryProxy(AIPROXY_PUPPET, (void **) &pPuppetProxy))
		return -1;

	SDebugThrowEval* pEval = new SDebugThrowEval;
	unsigned int sampleCount = 50;
	pEval->trajectory.resize(sampleCount);
	pEval->score = 0;
	pEval->fake = false;

	Vec3	grenadeHitPos(0,0,0);
	bool res = pPuppetProxy->PredictProjectileHit(dir, vel, grenadeHitPos, speedOut, &pEval->trajectory[0], &sampleCount);

	pEval->trajectory.resize(sampleCount);
	pEval->pos = grenadeHitPos;
	m_DEBUG_evals.push_back(pEval);

	if (!res)
		return -1;

	float	score = 0;

	const float minDistToFriendSq = sqr(6.0f);
	const float maxDistToTarget = 7.0f;

	float travelDist = Distance::Point_Point(m_pShooter->GetPos(), grenadeHitPos);
	float targetDist = Distance::Point_Point(m_pShooter->GetPos(), targetPos);
	const float distScale = m_pShooter->GetParameters().m_fGrenadeThrowDistScale;

	if (travelDist < targetDist*0.75f*distScale)
		return -1;

	float d;

	// Prefer positions which close to the target.
	Vec3 delta = grenadeHitPos - targetPos;
	d = delta.GetLength();
	score += d;
	if (d > maxDistToTarget)
		return -1;
	// no friends proximity checks for smoke grenades
	if(fireMode!=FIREMODE_SECONDARY_SMOKE)
	{
		// make sure not to throw grenade near NOGRENADE_SPOT
		AutoAIObjectIter itAnchors(GetAISystem()->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIANCHOR_NOGRENADE_SPOT));
		for (; itAnchors->GetObject(); itAnchors->Next())
		{
			IAIObject* pAvoidAnchor = itAnchors->GetObject();
			if (!pAvoidAnchor->IsEnabled())
				continue;
			float avoidDistSq(pAvoidAnchor->GetRadius());
			avoidDistSq *= avoidDistSq;
			d = Distance::Point_PointSq(grenadeHitPos, pAvoidAnchor->GetPos());
			if (d < avoidDistSq)
				return -1;
		}

		int species = m_pShooter->GetParameters().m_nSpecies;
		AutoAIObjectIter it(GetAISystem()->GetFirstAIObject(IAISystem::OBJFILTER_SPECIES, species));

		for (; it->GetObject(); it->Next())
		{
			IAIObject* pFriend = it->GetObject();
			if (!pFriend->IsEnabled())
				continue;

			d = Distance::Point_PointSq(grenadeHitPos, pFriend->GetPos());
			if (d < minDistToFriendSq)
			{
				if (score > 0) score = 0;
				score -= 1.0f;
			}
		}
	}
	// Prefer positions in front of the target
	if (targetViewDir.Dot(delta) < 0.0f)
		score += maxDistToTarget;

	pEval->score = score;

	return score;

/*	float	dmgTrh2(10.f*10.f);
	float	dmgUnit2(2.f*2.f);
	float	safeTrh2(10.f*10.f);
	//---- see if close to target ---------------------------------
	Vec3	targetDiff(grenadeHitPos - targetPos);
	// if the hit is behind or in-front of target
	bool	isTooFar(targetDiff*(m_pShooter->GetPos()-targetPos)<0 ? true : false);
	float	tagetdist2(targetDiff.GetLengthSquared());
	if(tagetdist2>dmgTrh2)
		return isTooFar ? GRD_TOOFAR : GRD_TOOCLOSE;

	evaluationValue = min( static_cast<int>(GRD_DMGLIMIT), static_cast<int>(tagetdist2/dmgUnit2));

	// see if not hitting any friends
	int species = m_pShooter->GetParameters().m_nSpecies;
	AutoAIObjectIter it(GetAISystem()->GetFirstAIObject(IAISystem::OBJFILTER_SPECIES, species));

	for(; it->GetObject(); it->Next())
	{
		IAIObject*	pFriend = it->GetObject();

		if(!pFriend->IsEnabled())
			continue;

		// Check for self
		if(pFriend == m_pShooter)
			continue;

		//FIXME this should never happen - puppet should always have proxy and physics - for Luciano to fix
		if(!pFriend->GetProxy() && !pFriend->GetProxy()->GetPhysics())
			continue;
		float dist2((grenadeHitPos - pFriend->GetPos()).GetLengthSquared());
		if(dist2 < safeTrh2)
			evaluationValue += GRD_FRIENDHIT;
		if(evaluationValue > GRD_FRIENDHITLIMIT)
			break;
	}

	int score = max(1,evaluationValue) * (isTooFar ? GRD_FAR : GRD_CLOSE);

	pEval->score = score;

	return score;*/
}

void CFireCommandGrenade::DebugDraw(struct IRenderer *pRenderer)
{
	for (unsigned i = 0, ni = m_DEBUG_evals.size(); i < ni; ++i)
	{
		SDebugThrowEval* pEval = m_DEBUG_evals[i];
		pRenderer->GetIRenderAuxGeom()->DrawSphere(pEval->pos, 0.2f, ColorB(255,0,0));
		pRenderer->DrawLabel(pEval->pos, 1.2f, "%.1f", pEval->score);
		pRenderer->GetIRenderAuxGeom()->DrawPolyline(&pEval->trajectory[0], pEval->trajectory.size(), false, pEval->fake ? ColorB(255,0,0) : ColorB(255,255,255), 2.0f);
	}
}

//
//---------------------------------------------------------------------------------------------------------------

//====================================================================
// CFireCommandHurricane
//====================================================================
CFireCommandHurricane::CFireCommandHurricane(IAIActor* pShooter):
/*m_fLastShootingTime(0),
m_fDrawFireLastTime(0),
m_nDrawFireCounter(0),
m_JustReset(false),
m_bIsTriggerDown(false),
m_bIsBursting(false),
m_fTimeOut(-1.f),
m_fLastBulletOutTime(0.f),
m_fFadingToNormalTime(0.f),
m_fLastValidTragetTime(0.f),*/
m_pShooter(((CAIActor*)pShooter)->CastToCPuppet())
{
}

void CFireCommandHurricane::Reset()
{
}

//
//
//
//----------------------------------------------------------------------------------------------------------------
bool CFireCommandHurricane::Update(IAIObject* pITarget, bool canFire, EFireMode fireMode, 
																			 const AIWeaponDescriptor& descriptor, Vec3& outShootTargetPos)
{
	if(!pITarget)
		return false;

	if(!canFire)
		return false;

	CAIObject* pTarget((CAIObject*)pITarget);

	bool fireNow(SelectFireDirNormalFire(pTarget, 1.f, outShootTargetPos));

	// Check if weapon is not pointing too differently from where we want to shoot
	// The outShootTargetPos is initialized with the aim target.
	if(fireNow && !IsWeaponPointingRight(outShootTargetPos))
		return false;

	if(fireNow && !ValidateFireDirection(outShootTargetPos-m_pShooter->GetFirePos(), false))
		return false;

	return fireNow;
}

//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandHurricane::SelectFireDirNormalFire(CAIObject* pTarget, float fAccuracyRamping, Vec3& outShootTargetPos)
{
	//accuracy/missing management: miss if not in front of player or if accuracy needed
	float	shooterCurrentAccuracy = m_pShooter->GetAccuracy(pTarget);
	bool bAccuracyMiss = Random(0.f,100.f) > (shooterCurrentAccuracy* 100.f);
	bool bMissNow = bAccuracyMiss; // || m_pShooter->GetAmbientFire();

	++dbg_ShotCounter;

	if(bMissNow)
	{
		outShootTargetPos = ChooseMissPoint(pTarget);
	}
	else
	{
		Vec3	vTargetPos;
		Vec3	vTargetDir;
		// In addition to common fire checks, make sure that we do not hit something too close between us and the target.
		// check if we don't hit anything (rocks/trees/cars)
		bool	lowDamage = fAccuracyRamping < 1.0f;
//		if(pTarget->GetAIType() == AIOBJECT_PLAYER)
//			lowDamage = lowDamage || !GetAISystem()->GetCurrentDifficultyProfile()->allowInstantKills;
		if(!lowDamage)
			lowDamage = Random(0.f,100.f) > (shooterCurrentAccuracy * 50.f);	// decide if want to hit legs/arms (low damage)
		if(!m_pShooter->CheckAndGetFireTarget_Deprecated(pTarget, lowDamage, vTargetPos, vTargetDir))
			return false;
		outShootTargetPos = vTargetPos;

		if(!outShootTargetPos.IsZero(1.f))
		{
			++dbg_ShotHitCounter;
		}
	}
	//
	if(outShootTargetPos.IsZero(1.f))
	{
		outShootTargetPos = pTarget->GetPos();
		return false;
	}

	return true;
}

//
//	we have candidate shooting direction, let's check if it is good
//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandHurricane::ValidateFireDirection(const Vec3& fireVector, bool mustHit)
{
	TICK_COUNTER_FUNCTION
		ray_hit hit;
	int rwiResult;
	IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
	TICK_COUNTER_FUNCTION_BLOCK_START
		rwiResult = pWorld->RayWorldIntersection(m_pShooter->GetFirePos(), fireVector, COVER_OBJECT_TYPES, HIT_COVER, &hit, 1);
	TICK_COUNTER_FUNCTION_BLOCK_END
		// if hitting something too close to shooter - bad fire direction
		float fireVectorLen;
	if (rwiResult!=0)
	{
		fireVectorLen = fireVector.GetLength();
		if(mustHit || hit.dist < fireVectorLen * (m_pShooter->GetFireMode() == FIREMODE_FORCED? 0.3f: 0.73f))
			return false;
	}
	return !m_pShooter->CheckFriendsInLineOfFire(fireVector, false);
}

//
//--------------------------------------------------------------------------------------------------------
bool CFireCommandHurricane::IsWeaponPointingRight(const Vec3	&shootTargetPos) //const
{
	// Check if weapon is not pointing too differently from where we want to shoot
	Vec3	fireDir(shootTargetPos - m_pShooter->GetFirePos());
	float dist2(fireDir.GetLengthSquared());
	// if withing 2m from target - make very loose angle check
	float	trhAngle( fireDir.GetLengthSquared()<sqr(5.f) ? 73.f : 22.f);
	fireDir.normalize();
	SAIBodyInfo bodyInfo;
	m_pShooter->GetProxy()->QueryBodyInfo(bodyInfo);

	//dbg_AngleDiffValue = acos_tpl( fireDir.Dot(bodyInfo.vFireDir));
	dbg_AngleDiffValue = RAD2DEG( acos_tpl(( fireDir.Dot(bodyInfo.vFireDir))) );

	if(fireDir.Dot(bodyInfo.vFireDir) < cry_cosf(DEG2RAD(trhAngle)) )
	{
		dbg_AngleTooDifferent = true;
		return false;
	}
	dbg_AngleTooDifferent = false;
	return true;
}

//
//--------------------------------------------------------------------------------------------------------
Vec3 CFireCommandHurricane::ChooseMissPoint(CAIObject* pTarget) const
{
	Vec3	vTargetPos(pTarget->GetPos());
	SAIBodyInfo	targetBody;
	if(pTarget->GetProxy())
		pTarget->GetProxy()->QueryBodyInfo(targetBody);
	float	targetGroundLevel = pTarget->GetPhysicsPos().z;
	return m_pShooter->ChooseMissPoint_Deprecated(vTargetPos);
}

//
//---------------------------------------------------------------------------------------------------------------
