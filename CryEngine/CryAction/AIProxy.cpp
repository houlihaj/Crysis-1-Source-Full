/********************************************************************
	CryGame Source File.
	Copyright (C), Crytek Studios, 2001-2004.
	-------------------------------------------------------------------------
	File name:   AIProxy.cpp
	Version:     v1.00
	Description: 

	-------------------------------------------------------------------------
	History:
	- 7:10:2004   18:54 : Created by Kirill Bulatsev
	- 2:12:2005           Now talks directly to CryAction interfaces (craig tiller)

*********************************************************************/



#include "StdAfx.h"
#include "IAISystem.h"
#include "AIProxy.h"
#include "AIHandler.h"
#include "IActorSystem.h"
#include "ISerialize.h"
#include "IMovementController.h"
#include "IItemSystem.h"
#include "IVehicleSystem.h"
#include "ICryAnimation.h"
#include "CryAction.h"

#include "IRenderer.h"
#include "IRenderAuxGeom.h"	

//
//----------------------------------------------------------------------------------------------------------
CAIProxy::CAIProxy(IGameObject *pGameObject, bool useAIHandler):
	m_pGameObject(pGameObject),
	m_pAIHandler(0),
	m_firing(false),
	m_firingSecondary(false),
	m_readibilityDelay(0.0f),
	m_readibilityPlayed(false),
	m_readibilityPriority(0),
	m_readibilityId(0),
	m_readibilityIgnored(0),
	m_fMinFireTime(0),
	m_WeaponShotIsDone(false),
	m_NeedsShootSignal(false),
	m_PrevAlertness(0),
	m_IsDisabled(false),
	m_UpdateAlways(false),
	m_IsWeaponForceAlerted(false),
	m_pIActor(0),
	dbg_ShotsCount(0),
	dbg_HitsCount(0),
	m_shotBulletCount(0),
	m_lastShotTime(0.0f)
{
	if(useAIHandler)
	{
		m_pAIHandler = new CAIHandler(m_pGameObject);
		m_pAIHandler->Init(GetISystem());
	}
}

//
//----------------------------------------------------------------------------------------------------------
CAIProxy::~CAIProxy(void)
{
	Release();
	if(m_pAIHandler)
		delete m_pAIHandler;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::QueryProxy(unsigned char type, void **pProxy)
{
	if (type == AIPROXY_PUPPET)
	{
		*pProxy = (void *) this;
		return true;
	}
	else if (type == AIPROXY_VEHICLE)
	{
		*pProxy = (void *) this;
		return true;
	}
	else
		return false;
}

//
//----------------------------------------------------------------------------------------------------------
EntityId CAIProxy::GetLinkedDriverEntityId()
{
	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	IVehicle* pVehicle = pVehicleSystem->GetVehicle(m_pGameObject->GetEntityId());
	if (pVehicle)
		if (IVehicleSeat *pVehicleSeat = pVehicle->GetSeatById(1))	//1 means driver
			return pVehicleSeat->GetPassenger();
	return 0;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::IsDriver()
{
	if (IActor* pActor = GetActor())
	{
		if (pActor->GetLinkedVehicle())
		{
			IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
			IVehicle* pVehicle = pVehicleSystem->GetVehicle(pActor->GetLinkedVehicle()->GetEntityId());
			if (pVehicle)
			{
				if (IVehicleSeat *pVehicleSeat = pVehicle->GetSeatById(1))	//1 means driver
				{
					if (  m_pGameObject )
					{
						EntityId passengerId = pVehicleSeat->GetPassenger();
						EntityId selfId = m_pGameObject->GetEntityId();
						if ( passengerId == selfId )
							return true;
					}
				}
			}
		}
	}

	return false;
}

//----------------------------------------------------------------------------------------------------------
EntityId CAIProxy::GetLinkedVehicleEntityId()
{
	if (IActor* pActor = GetActor())
		if (pActor->GetLinkedVehicle())
			return pActor->GetLinkedVehicle()->GetEntityId();
	return 0;
}

//
//----------------------------------------------------------------------------------------------------------
void  CAIProxy::QueryBodyInfo( SAIBodyInfo& bodyInfo ) 
{
	IMovementController * pMC = m_pGameObject->GetMovementController();
	if (!pMC)
		return;
	SMovementState moveState;
	pMC->GetMovementState( moveState );
	bodyInfo.maxSpeed = moveState.maxSpeed;
	bodyInfo.minSpeed = moveState.minSpeed;
	bodyInfo.normalSpeed = moveState.normalSpeed;
	bodyInfo.stance = moveState.stance;
	bodyInfo.stanceSize = moveState.m_StanceSize;
	bodyInfo.colliderSize = moveState.m_ColliderSize;
	bodyInfo.vEyeDir = moveState.eyeDirection;
	bodyInfo.vEyeDirAnim = moveState.animationEyeDirection;
	bodyInfo.vEyePos = moveState.eyePosition;
  bodyInfo.vBodyDir = moveState.bodyDirection;
  bodyInfo.vMoveDir = moveState.movementDirection;
	bodyInfo.vUpDir = moveState.upDirection;
	bodyInfo.lean = moveState.lean;
	bodyInfo.slopeAngle = moveState.slopeAngle;

	bodyInfo.vFirePos = moveState.weaponPosition;
	bodyInfo.vFireDir = moveState.aimDirection;
	bodyInfo.isAiming = moveState.isAiming; 
	bodyInfo.isFiring = moveState.isFiring;

	IActor*	pActor = GetActor();
	IVehicle* pVehicle = pActor ? pActor->GetLinkedVehicle() : 0;
	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	IEntitySystem* pEntitySystem = GetISystem()->GetIEntitySystem();

	bodyInfo.linkedVehicleEntity = pVehicle ? pVehicle->GetEntity() : 0;

	bodyInfo.linkedDriverEntity = 0;
	{
		pVehicle = pVehicleSystem->GetVehicle(m_pGameObject->GetEntityId());
		if ( pVehicle )
		{
			IVehicleSeat *pVehicleSeat = pVehicle->GetSeatById(1);//1 means driver
			if ( pVehicleSeat )
			{
				EntityId driverId = pVehicleSeat->GetPassenger();
				if ( driverId )
				{
					if ( pEntitySystem ) 
					{
						IEntity* pEntity = pEntitySystem->GetEntity( driverId );
						if ( pEntity )
						{
							bodyInfo.linkedDriverEntity = pEntity;
						}
					}
				}
			}
		}
	}

}

bool CAIProxy::QueryBodyInfo( EStance stance, float lean, bool defaultPose, SAIBodyInfo& bodyInfo )
{
	IMovementController * pMC = m_pGameObject->GetMovementController();
	if (!pMC)
		return false;
	SStanceState stanceState;
	if(!pMC->GetStanceState( stance, lean, defaultPose, stanceState ))
		return false;
	bodyInfo.maxSpeed = 0.0f;
	bodyInfo.minSpeed = 0.0f;
	bodyInfo.normalSpeed = 0.0f;
	bodyInfo.stance = stance;
	bodyInfo.stanceSize = stanceState.m_StanceSize;
	bodyInfo.colliderSize = stanceState.m_ColliderSize;
	bodyInfo.vEyeDir = stanceState.eyeDirection;
	bodyInfo.vEyeDirAnim.Set(0,0,0);
	bodyInfo.vEyePos = stanceState.eyePosition;
  bodyInfo.vBodyDir = stanceState.bodyDirection;
  bodyInfo.vMoveDir.zero();
	bodyInfo.vUpDir = stanceState.upDirection;
	bodyInfo.vFirePos = stanceState.weaponPosition;
	bodyInfo.vFireDir = stanceState.aimDirection;
	bodyInfo.isAiming = false; 
	bodyInfo.isFiring = false;
	bodyInfo.lean = stanceState.lean;

	bodyInfo.linkedVehicleEntity = 0;
	bodyInfo.linkedDriverEntity = 0;

	return true;
}

//
//----------------------------------------------------------------------------------------------------------
static float QuadraticInterp( float x, float k, float A, float B, float C )
{
	float a = (A - B - A*k + C*k) / (k - k*k);
	float b = -(A - B - A*k*k + C*k*k) / (k - k*k);
	float c = A;

	if(x < 1.0f)
		return a*x*x + b*x + c;
	else
		return (2.0f*a + b)*x + c - a;
}

static float LinearInterp(float x, float k, float A, float B, float C)
{
	if(x < k)
	{
		x = x / k;
		return A + (B - A) * x;
	}
	else
	{
		x = (x - k) / (1.0f - k);
		return B + (C - B) * x;
	}
}

IEntity* CAIProxy::GetGrabbedEntity() const
{ 
	IActor*	pActor(GetActor());
	if(pActor)
		return gEnv->pEntitySystem->GetEntity(pActor->GetGrabbedEntityId());
	return 0;
}


IWeapon * CAIProxy::GetCurrentWeapon() const
{ 
	IItem* pItem = 0;
	IActor*	pActor = GetActor();
  if (!pActor)
    return 0;

  EntityId itemId = 0;
  if (IVehicle* pVehicle = pActor->GetLinkedVehicle())
  {
    // mounted weapon( gunner seat )
    itemId = pVehicle->GetCurrentWeaponId(m_pGameObject->GetEntityId());

		// vehicle weapon( driver seat )
		IVehicleSeat *pVehicleSeat = pVehicle->GetSeatById(1);//1 means driver
		if ( pVehicleSeat )
		{
			IEntity* pMyEntity = pActor->GetEntity();
			if ( pMyEntity )
			{
				EntityId driverId = pVehicleSeat->GetPassenger();
				EntityId myId = pMyEntity->GetId();
				if ( driverId == myId )
					return 0;
			}
		}

		pItem = CCryAction::GetCryAction()->GetIItemSystem()->GetItem( itemId );
	}
  else  
  {
		pItem = pActor->GetCurrentItem();
  }

	if (pItem)
    return pItem->GetIWeapon();
		
	return 0;
}

IWeapon * CAIProxy::GetSecWeapon(ERequestedSecondaryType prefType) const
{
	IActor*	pActor = GetActor();
	IInventory * pInventory = pActor->GetInventory();
	if (!pInventory)
		return NULL;

	IWeapon	*pGrenadeWeapon(NULL);
	IAIObject* pAI = m_pGameObject->GetEntity()->GetAI();

	EntityId itemEntity = 0;
	if (pAI && pAI->GetAIType()==AIOBJECT_PLAYER)
	{
		IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("OffHand");
		itemEntity = pInventory->GetItemByClass(pClass);
	}
	else if(prefType == RST_SMOKE_GRENADE)
	{
#ifndef SP_DEMO
		IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AISmokeGrenades");
		itemEntity = pInventory->GetItemByClass(pClass);
#endif
	}
	else
	{
		IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AIGrenades");
		itemEntity = pInventory->GetItemByClass(pClass);
#ifndef SP_DEMO
		if (!itemEntity)
		{
			pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AIFlashbangs");
			itemEntity = pInventory->GetItemByClass(pClass);
		}
		if (!itemEntity)
		{
			pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AISmokeGrenades");
			itemEntity = pInventory->GetItemByClass(pClass);
		}
#endif
	}

	if (itemEntity)
	{
		if (IItem *pItem = CCryAction::GetCryAction()->GetIItemSystem()->GetItem(itemEntity))
			return pItem->GetIWeapon();
	}

	return 0;
}

IFireController* CAIProxy::GetFireController()
{
	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();

	if (IVehicle* pVehicle = (IVehicle*) pVehicleSystem->GetVehicle(m_pGameObject->GetEntityId()))
		return pVehicle->GetFireController();

	return NULL;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::IsEnabled() const
{
	return !m_IsDisabled;
}


//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::EnableUpdate(bool enable)
{
	// DO NOT:
	//  1. call SetAIActivation on GameObject
	//  2. call Activate on Entity
	// (this will result in an infinite-ish loop)
	if(enable)
	{
		IAIObject* pAI = m_pGameObject->GetEntity()->GetAI();
		m_IsDisabled=false;
		pAI->Event(AIEVENT_ENABLE, NULL);
	}
	else
	{
		IAIObject* pAI = m_pGameObject->GetEntity()->GetAI();
		m_IsDisabled=true;
		pAI->Event(AIEVENT_DISABLE, NULL);
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::UpdateMeAlways(bool doUpdateMeAlways) 
{
	m_UpdateAlways=doUpdateMeAlways;
	CheckUpdateStatus();
}


//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::CheckUpdateStatus()
{
	bool update = false;
	// DO NOT call Activate on Entity
	// (this will result in an infinite-ish loop)
	if (gEnv->pAISystem->GetUpdateAllAlways() || m_UpdateAlways)
		update = m_pGameObject->SetAIActivation(eGOAIAM_Always);
	else
		update = m_pGameObject->SetAIActivation(eGOAIAM_VisibleOrInRange);
	return update;
}

//
//----------------------------------------------------------------------------------------------------------
int CAIProxy::Update(SOBJECTSTATE *pStates)
{
	if(!CheckUpdateStatus())
		return 0;
/*
	bool needsUpdate(gEnv->pAISystem->GetUpdateAllAlways() || m_UpdateAlways || m_pGameObject->IsProbablyVisible() || !m_pGameObject->IsProbablyDistant());
	if(!needsUpdate)	
	{
		IAIObject* pAI = m_pGameObject->GetEntity()->GetAI();
		m_IsDisabled=true;
		pAI->Event(AIEVENT_DISABLE, NULL);
		//m_pGameObject->GetEntity()->Activate(false);
		m_pGameObject->SetAIActivation(false);
		return 0;
	} 
*/
	if( !m_pAIHandler )
		return 0;

	UpdateMind(pStates);

	if(GetActorHealth()<=0 && GetPhysics()==NULL)
	{
		// killed actor is updated by AI ?
		return 0;
	}

	IActor*	pActor = GetActor();
	
	if(m_PrevAlertness==0 && GetAlertnessState()!=0)
	{

		// speed up transitions when getting alerted (idle-combat)
		{
			if (pActor)
				pActor->GetAnimationGraphState()->Hurry();
		}
	}
	m_PrevAlertness=GetAlertnessState();

	bool fire = pStates->fire;
	bool fireSec = pStates->fireSecondary;
	bool isAlive(true);


	IMovementController * pMC = m_pGameObject->GetMovementController();
	if (pMC)
	{
		SMovementState moveState;
		pMC->GetMovementState(moveState);
		isAlive = moveState.isAlive;

		if (!isAlive)
		{
			fire = false;
			fireSec = false;
		}

		Vec3 currentPos = m_pGameObject->GetEntity()->GetWorldPos();

		CMovementRequest mr;

		int alertness = GetAlertnessState();
		mr.SetAlertness(alertness);

		// [DAVID] Set Actor/AC alertness
		IActor* pActor = GetActor();
		if (pActor)
			pActor->SetFacialAlertnessLevel(alertness);

		if( !pStates->vLookTargetPos.IsZero() )
			mr.SetLookTarget( pStates->vLookTargetPos );
		else
			mr.ClearLookTarget();

		if(fabsf(pStates->lean) > 0.01f)
			mr.SetLean(pStates->lean);
		else
			mr.ClearLean();

		// it is possible that we have a move target but zero speed, in which case we want to turn
		// on the spot.
		bool wantsToMove = !pStates->vMoveDir.IsZero();
		if(m_pAIHandler && m_pAIHandler->IsAnimationBlockingMovement())
			wantsToMove = false;

		if (!wantsToMove)
		{
			mr.ClearMoveTarget();
			mr.SetDesiredSpeed(0.0f);
			mr.SetPseudoSpeed(pStates->curActorTargetPhase == eATP_Waiting ? fabs(pStates->fMovementUrgency) : 0.0f);
      mr.ClearPrediction();
		}
		else
		{
			if (pStates->predictedCharacterStates.nStates > 0)
			{
				int num = min((int) pStates->predictedCharacterStates.nStates, (int) SPredictedCharacterStates::maxStates);
				SPredictedCharacterStates predStates;
				predStates.nStates = num;
				pe_status_pos status;
				GetPhysics()->GetStatus(&status);
				for (int iPred = 0 ; iPred != num ; ++iPred)
				{
					SAIPredictedCharacterState &pred = pStates->predictedCharacterStates.states[iPred];
					predStates.states[iPred].deltatime = pred.predictionTime;
					predStates.states[iPred].position = pred.position;
					predStates.states[iPred].velocity = pred.velocity;
					//predStates.states[iPred].rotation.SetIdentity();
					if (pStates->allowStrafing || pred.velocity.IsZero(0.01f))
						predStates.states[iPred].orientation = status.q;
					else
						predStates.states[iPred].orientation.SetRotationV0V1(Vec3(0, 1, 0), pred.velocity.GetNormalized());
				}
				mr.SetPrediction(predStates);
			}
      else
      {
        mr.ClearPrediction();
      }
			mr.SetMoveTarget(currentPos + pStates->vMoveDir.GetNormalized());

			if (fabs(pStates->fDesiredSpeed * pStates->fMovementUrgency) > 0.00001f)
			{
        mr.SetDesiredSpeed( pStates->fMovementUrgency > 0.0f ? pStates->fDesiredSpeed : -pStates->fDesiredSpeed);
				mr.SetPseudoSpeed( fabs(pStates->fMovementUrgency) );
			}
			else if(pStates->fMovementUrgency==0.f)
			{//Special case: AIFollowPathSpeedsStance Speed set to 0, (run property set to -5)
			 //decrease the vehicle speed slowly, not to slide sideways from path during brake
				mr.SetDesiredSpeed(pStates->fDesiredSpeed);
				mr.SetPseudoSpeed(pStates->fDesiredSpeed);
			}
			else
			{
				mr.SetDesiredSpeed(0.0f);
				mr.SetPseudoSpeed(0.0f);
			}
		}

		if(m_pAIHandler)
			m_pAIHandler->HandleExactPositioning(pStates, mr);

		mr.SetDistanceToPathEnd( pStates->fDistanceToPathEnd );

		switch (pStates->bodystate)
		{
		case BODYPOS_CROUCH:
//			CryLog( "%s STANCE_CROUCH", m_pGameObject->GetEntity()->GetName() );
			mr.SetStance( STANCE_CROUCH );
			break;
		case BODYPOS_PRONE:
//			CryLog( "%s STANCE_PRONE", m_pGameObject->GetEntity()->GetName() );
			mr.SetStance( STANCE_PRONE );
			break;
		case BODYPOS_RELAX:
//			CryLog( "%s STANCE_STAND", m_pGameObject->GetEntity()->GetName() );
			mr.SetStance( STANCE_RELAXED );
			//mr.SetStance( STANCE_STAND );
			break;
		case BODYPOS_STEALTH:
//			CryLog( "%s STANCE_STEALTH", m_pGameObject->GetEntity()->GetName() );
			mr.SetStance( STANCE_STEALTH );
			break;
		default:
//			CryLog( "%s STANCE_STAND", m_pGameObject->GetEntity()->GetName() );
			mr.SetStance( STANCE_STAND );
			break;
		}

		if (pStates->jump)
			mr.SetJump();

		if (pStates->aimTargetIsValid && !pStates->vAimTargetPos.IsZero())
			mr.SetAimTarget(pStates->vAimTargetPos);
		else
			mr.ClearAimTarget();

		if (fire || fireSec || pStates->fireMelee)
			mr.SetFireTarget( pStates->vShootTargetPos );
		else 
			mr.ClearFireTarget();

		mr.SetAllowStrafing( pStates->allowStrafing );

		if (pStates->vForcedNavigation.IsZero())
			mr.ClearForcedNavigation();
		else
			mr.SetForcedNavigation(pStates->vForcedNavigation);

		// try, and retry once
		if (!pMC->RequestMovement( mr ))
			pMC->RequestMovement( mr );
	}

	UpdateShooting(pStates, isAlive);

	// NOTE: This is not supported anymore in AnimatedCharacter, so it should be removed here.
	if (IAnimatedCharacter * pAnimChar = m_pIActor? m_pIActor->GetAnimatedCharacter() : 0)
  {
    SAnimatedCharacterParams params = pAnimChar->GetParams();

		// NOTE: This is not supported anymore in AnimatedCharacter, so it should be removed here.
		if (pStates->allowEntityClampingByAnimation)
      params.flags |= eACF_AllowEntityClampingByAnimation;
    else
      params.flags &= ~eACF_AllowEntityClampingByAnimation;
    pAnimChar->SetParams( params );
  }

	return 0;
}


//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::UpdateShooting(SOBJECTSTATE *pStates, bool isAlive)
{
	m_WeaponShotIsDone = false;

	bool fire = pStates->fire && !pStates->aimObstructed;//|| (m_StartWeaponSpinUpTime>0);
	bool fireSec = pStates->fireSecondary;
	if(!isAlive)
	{
		fire = false;
		fireSec = false;
	}

	if(pStates->fireMelee)
	{
		if(isAlive)
		{
			if (IWeapon * pWeapon = GetCurrentWeapon())
				pWeapon->MeleeAttack();
		}
		fire = false;
		fireSec = false;
	}

	//---------------------------------------------------------------------------
	// TODO remove when aiming/fire direction is working
	// debugging aiming dir
	struct DBG_shoot{
		Vec3	src;
		Vec3	dst;
	};
	const	int	DGB_ShotCounter(3);
	static int	DBG_curIdx(-1);
	static int	DBG_curLimit(-1);
	static DBG_shoot DBG__shots[DGB_ShotCounter];
	bool	debugDrawAim(gEnv->pConsole->GetCVar("g_aimdebug") && gEnv->pConsole->GetCVar("g_aimdebug")->GetIVal()!=0);
	//const ColorF	queueFireCol( .4f, 1.0f, 0.4f, 1.0f );
	if(debugDrawAim)
	{
		const ColorF	fireCol( 1.0f, 0.1f, 0.0f, 1.0f );
		for(int dbgIdx(0);dbgIdx<DBG_curLimit; ++dbgIdx)
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( DBG__shots[dbgIdx].src, fireCol, DBG__shots[dbgIdx].dst, fireCol );
	}

	IWeapon * pWeapon = GetCurrentWeapon();
	IFireMode* pFireMode = pWeapon ? pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()) : 0;

	// Update accessories
	if (pWeapon && pFireMode)
	{
		bool enableLaser = (pStates->weaponAccessories & AIWEPA_LASER) != 0;
		bool enableLight = (pStates->weaponAccessories & (AIWEPA_COMBAT_LIGHT|AIWEPA_PATROL_LIGHT)) != 0;

		if (pFireMode->IsReloading())
			enableLaser = false;

		if (pWeapon->IsLamAttached())
		{
			if (pWeapon->IsLamLaserActivated() != enableLaser)
				pWeapon->ActivateLamLaser(enableLaser);
			if (pWeapon->IsLamLightActivated() != enableLight)
				pWeapon->ActivateLamLight(enableLight);
		}
		else if(pWeapon->IsFlashlightAttached())
		{
			if (pWeapon->IsLamLightActivated() != enableLight)
				pWeapon->ActivateLamLight(enableLight);
		}
	}

	if (pWeapon)
	{
		if(pFireMode)
			m_firing = pFireMode->IsFiring();
		if(pStates->aimObstructed)
			pWeapon->RaiseWeapon(true);
		else
			pWeapon->RaiseWeapon(false);
	}

	// firing control
	if (fire && !m_firing)
	{
		if (pWeapon && pWeapon->CanFire())
		{
			pWeapon->StartFire();
			m_firing = true;

			if(debugDrawAim)
			{
				IMovementController * pMC = m_pGameObject->GetMovementController();
				if (pMC)
				{
					SMovementState moveState;
					pMC->GetMovementState(moveState);
					if(++DBG_curLimit>DGB_ShotCounter)	DBG_curLimit = DGB_ShotCounter;
					if(++DBG_curIdx>=DGB_ShotCounter)	DBG_curIdx = 0;
					DBG__shots[DBG_curIdx].src=moveState.weaponPosition;
					DBG__shots[DBG_curIdx].dst = pStates->vShootTargetPos;
				}
			}
		}
		else
		{
			if (IFireController* pFireController = GetFireController())
			{
				m_firing = true;
				pFireController->RequestFire(m_firing);
			}
		}
	}
	else if (!fire && m_firing)
	{
		if (pWeapon)
		{
			pWeapon->StopFire();
			m_firing = false;
		}
		else
		{
			if (IFireController* pFireController = GetFireController())
			{
				m_firing = false;
				pFireController->RequestFire(m_firing);
			}
		}
	}

	// secondary weapon firing control
	if (fireSec && !m_firingSecondary)
	{
		if (IWeapon *pWeapon=GetSecWeapon( pStates->secondaryType ))
		{
			pWeapon->PerformThrow(pStates->fProjectileSpeedScale);

			m_firingSecondary = true;

#ifndef SP_DEMO
			if(IEntity* pEntity = m_pGameObject->GetEntity())
				if(pStates->secondaryType == RST_SMOKE_GRENADE)
				{
					SendSignal(0, "OnSmokeGrenadeThrown", pEntity, NULL);
					IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
					if(pAIActor)
						pAIActor->NotifySignalReceived( "OnSmokeGrenadeThrown" );
				}
#endif
		}
	}
	else if (!fireSec && m_firingSecondary)
	{
		m_firingSecondary=false;
	}
}


//
//----------------------------------------------------------------------------------------------------------
void  CAIProxy::QueryWeaponInfo(SAIWeaponInfo& weaponInfo)
{
	IWeapon *pWeapon(GetCurrentWeapon());
	if(!pWeapon)
		return;

	IFireMode* pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

	weaponInfo.canMelee = pWeapon->CanMeleeAttack();
	weaponInfo.outOfAmmo = pWeapon->OutOfAmmo(false);
	weaponInfo.shotIsDone = m_WeaponShotIsDone;
	weaponInfo.hasLightAccessory = weaponInfo.hasLaserAccessory = pWeapon->IsLamAttached();
	weaponInfo.hasLightAccessory = weaponInfo.hasLightAccessory || pWeapon->IsFlashlightAttached();
	weaponInfo.isReloading = pFireMode ? pFireMode->IsReloading() : false;
	weaponInfo.isFiring = pFireMode ? pFireMode->IsFiring() : false;
	weaponInfo.isTrainMounted = pWeapon->IsTrainMounted();
	weaponInfo.trainMountedDir = pWeapon->GetTrainMountedDir();
	weaponInfo.lastShotTime = m_lastShotTime;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::EnableWeaponListener(bool needsSignal)
{
	dbg_ShotsCount = 0;
	dbg_HitsCount = 0;
	m_shotBulletCount = 0;
	m_lastShotTime.SetSeconds(0.0f);
	m_NeedsShootSignal = needsSignal;
	if (IWeapon * pWeapon = GetCurrentWeapon())
		pWeapon->AddEventListener(this, __FUNCTION__);
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType,
                       const Vec3 &pos, const Vec3 &dir, const Vec3 &vel)
{
	IEntity* pEntity = m_pGameObject->GetEntity();
	if(!pEntity)
	{
		gEnv->pAISystem->Warning(">> ","CAIProxy::OnShoot null entity");
		return;
	}
	if(pEntity->GetAI() && pEntity->GetAI()->GetAIType() != AIOBJECT_PLAYER)
		gEnv->pAISystem->DebugDrawFakeTracer(pos, dir);

	m_shotBulletCount++;
	m_lastShotTime = gEnv->pTimer->GetFrameStartTime();

	++dbg_ShotsCount;
	m_WeaponShotIsDone = true;
	IPuppet *pPuppet = CastToIPuppetSafe(pEntity->GetAI());
	if(pPuppet && pPuppet->GetFirecommandHandler())
		pPuppet->GetFirecommandHandler()->OnShoot();

	if(!m_NeedsShootSignal)
		return;
	SendSignal(0, "WPN_SHOOT", pEntity, NULL);
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if(pAIActor)
		pAIActor->NotifySignalReceived( "WPN_SHOOT" );
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::OnDropped(IWeapon *pWeapon, EntityId actorId)
{
	m_NeedsShootSignal = false;
	if (pWeapon)
		pWeapon->RemoveEventListener(this);
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::OnSelected(IWeapon *pWeapon, bool selected)
{
	if (!selected)
	{
		m_NeedsShootSignal = false;
		if (pWeapon)
			pWeapon->RemoveEventListener(this);
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::Release()
{
	if( m_pAIHandler )
		m_pAIHandler->Release();
}


//
//----------------------------------------------------------------------------------------------------------
IPhysicalEntity* CAIProxy::GetPhysics(bool wantCharacterPhysics)
{
	if( wantCharacterPhysics )
	{
		ICharacterInstance* pCharacter(m_pGameObject->GetEntity()->GetCharacter(0));
		if(pCharacter)
			return pCharacter->GetISkeletonPose()->GetCharacterPhysics();
		return NULL;
	}
	return m_pGameObject->GetEntity()->GetPhysics();
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::DebugDraw(struct IRenderer *pRenderer, int iParam)
{
	if( !m_pAIHandler )
		return;

	if( iParam == 1 )
	{
		// Stats target drawing.
		pRenderer->TextToScreenColor(50,66,0.3f,0.3f,0.3f,1.f,"- Proxy Information --");

		const char *szCurrentBehaviour=0;
		const char *szPreviousBehaviour=0;
		const char *szFirstBehaviour=0;

		if (m_pAIHandler->m_pBehavior.GetPtr())
			m_pAIHandler->m_pBehavior->GetValue("Name",szCurrentBehaviour);

		if (m_pAIHandler->m_pPreviousBehavior.GetPtr())
			m_pAIHandler->m_pPreviousBehavior->GetValue("Name",szPreviousBehaviour);

		if (!m_pAIHandler->m_sFirstBehaviorName.empty())
			szFirstBehaviour = m_pAIHandler->m_sFirstBehaviorName.c_str();


		pRenderer->TextToScreen(50,74,"BEHAVIOUR: %s",szCurrentBehaviour);
		pRenderer->TextToScreen(50,76," PREVIOUS BEHAVIOUR: %s",szPreviousBehaviour);
		pRenderer->TextToScreen(50,78," DESIGNER ASSIGNED BEHAVIOUR: %s",szFirstBehaviour);


		const char *szCurrentCharacter = m_pAIHandler->m_sCharacterName.c_str();
		const char *szPreviousCharacter = m_pAIHandler->m_sPrevCharacterName.c_str();
		const char *szFirstCharacter = m_pAIHandler->m_sFirstCharacterName.c_str();

		pRenderer->TextToScreen(50,82,"CHARACTER: %s",szCurrentCharacter);
		pRenderer->TextToScreen(50,84," PREVIOUS CHARACTER: %s",szPreviousCharacter);
		pRenderer->TextToScreen(50,86," DESIGNER ASSIGNED CHARACTER: %s",szFirstCharacter);
		if( !m_readibilityName.empty() )
		{
			if( m_readibilityDelay > 0 )
				pRenderer->TextToScreen(50,88," READIBILITY: %.1fs -> (%s) [%d]", m_readibilityDelay, m_readibilityName.c_str(), m_readibilityIgnored);
			else
				pRenderer->TextToScreen(50,88," READIBILITY: %s [%d]", m_readibilityName.c_str(), m_readibilityIgnored);
		}


		if(m_pAIHandler)
		{
			const char*	szEPMsg = " - ";
			switch(m_pAIHandler->DEBUG_GetActorTargetPhase())
			{
//			case eATP_Cancel: szEPMsg = "Cancel"; break;
			case eATP_None: szEPMsg = " - "; break;
//			case eATP_Request: szEPMsg = "Request"; break;
			case eATP_Waiting: szEPMsg = "Waiting"; break;
			case eATP_Starting: szEPMsg = "Starting"; break;
			case eATP_Started: szEPMsg = "Started"; break;
			case eATP_Playing: szEPMsg = "Playing"; break;
			case eATP_StartedAndFinished: szEPMsg = "Started/Finished"; break;
			case eATP_Finished: szEPMsg = "Finished"; break;
			case eATP_Error: szEPMsg = "Error"; break;
			}

			const char*	curSignalAnim = m_pAIHandler->DEBUG_GetCurrentSignaAnimationName();
			const char*	curActionAnim = m_pAIHandler->DEBUG_GetCurrentActionAnimationName();

			pRenderer->TextToScreen(50,90,"ANIMATION: %s", m_pAIHandler->IsAnimationBlockingMovement() ? "MOVEMENT BLOCKED!" : "");
			pRenderer->TextToScreen(50,92,"           ExactPos:%s", szEPMsg);
			pRenderer->TextToScreen(50,94,"           Signal:%s", curSignalAnim ? curSignalAnim : "-");
			pRenderer->TextToScreen(50,96,"           Action:%s", curActionAnim ? curActionAnim : "-");
		}
	}
	else if( iParam == 2 )
	{
		if( !m_readibilityName.empty() )
		{
			SAIBodyInfo bodyInfo;
			QueryBodyInfo( bodyInfo );

			float	color[4] = {0.92f,1,0,1};
			if( m_readibilityDelay > 0 )
			{
				color[0] = 1;
				color[1] = 1;
				color[2] = 1;
				color[3] = 0.1f;
			}
			pRenderer->DrawLabelEx( bodyInfo.vEyePos + Vec3( 0, 0, 2 ), 1, color, true, false, m_readibilityName.c_str() );
		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::Reset(EObjectResetType type)
{
	dbg_ShotsCount = 0;
	dbg_HitsCount = 0;
	m_IsDisabled = false;
	m_shotBulletCount = 0;
	m_lastShotTime.SetSeconds(0.0f);

	//if (GetISystem()->IsEditor())
	m_pGameObject->SetAIActivation(eGOAIAM_VisibleOrInRange); // we suppose the first update will set this properly.

	m_readibilityName.clear();
	m_readibilityDelay = 0;
	m_readibilityPriority = 0;
	m_readibilityId = 0;
	m_readibilityIgnored = 0;
	m_readibilityPlayed = false;

	if(m_pAIHandler)
		m_pAIHandler->Reset(type);

	IMovementController * pMC = m_pGameObject->GetMovementController();
	if (pMC)
	{
		CMovementRequest mr;
		mr.SetAlertness(0);

		// [DAVID] Clear Actor/AC alertness
		IActor* pActor = GetActor();
		if ( pActor )
			pActor->SetFacialAlertnessLevel(0);

		mr.ClearLookTarget();
		mr.ClearActorTarget();
		mr.ClearLean();
		mr.ClearPrediction();
		// try, and retry once
		if (!pMC->RequestMovement( mr ))
			pMC->RequestMovement( mr );
	}
}

//
//----------------------------------------------------------------------------------------------------------
int& CAIProxy::DebugValue(int idx)
{
	return dbg_HitsCount;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::SetAGInput(EAIAGInput input, const char* value)
{
	if ( m_pAIHandler ) return m_pAIHandler->SetAGInput( input, value );
	return false;
}

//----------------------------------------------------------------------------------------------------------
bool CAIProxy::ResetAGInput(EAIAGInput input)
{
	if ( m_pAIHandler ) return m_pAIHandler->ResetAGInput( input );
	return false;
}

bool CAIProxy::IsSignalAnimationPlayed( const char* value )
{
	if ( m_pAIHandler ) return m_pAIHandler->IsSignalAnimationPlayed( value );
	return true;
}

bool CAIProxy::IsActionAnimationStarted( const char* value )
{
	if ( m_pAIHandler ) return m_pAIHandler->IsActionAnimationStarted( value );
	return true;
}

bool CAIProxy::IsAnimationBlockingMovement()
{
	if(m_pAIHandler) return m_pAIHandler->IsAnimationBlockingMovement();
	return false;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::UpdateMind(SOBJECTSTATE *state)
{
	if (m_pAIHandler)
		m_pAIHandler->Update();

	std::vector<AISIGNAL> nextUpdateSignals;

	if ( m_pGameObject->GetEntity()->GetAI()->IsUpdatedOnce() )
	{

		while (!state->vSignals.empty())
		{
			AISIGNAL sstruct(state->vSignals.back());
			state->vSignals.pop_back();
			int signal = sstruct.nSignal;
			if(signal == AISIGNAL_PROCESS_NEXT_UPDATE)
			{
				sstruct.nSignal = AISIGNAL_RECEIVED_PREV_UPDATE;
				nextUpdateSignals.push_back(sstruct);
			}
			else
			{
				const char *szText = sstruct.strText;
				//			IEntity* pSender= (IEntity*) sstruct.pSender;
				SendSignal(signal,szText,sstruct.pSender,sstruct.pEData);
				if ( sstruct.pEData )
					gEnv->pAISystem->FreeSignalExtraData( sstruct.pEData );
			}
		}
		
		std::vector<AISIGNAL>::iterator it = nextUpdateSignals.begin(), itEnd = nextUpdateSignals.end();
		for(; it!=itEnd; ++it)
			state->vSignals.push_back(*it);

		/*
		int size = state->vSignals.size();
		for(int i=size-1;i>=0; i--)
		{
			AISIGNAL sstruct(state->vSignals[i]);
			int signal = sstruct.nSignal;
			if(signal != 3)
			{
				const char *szText = sstruct.strText;
				//			IEntity* pSender= (IEntity*) sstruct.pSender;
				SendSignal(signal,szText,sstruct.pSender,sstruct.pEData);
				if ( sstruct.pEData )
					gEnv->pAISystem->FreeSignalExtraData( sstruct.pEData );
			}
		}
		
		DynArray<AISIGNAL>::iterator it = state->vSignals.begin();
		for(int i=0;i<size;i++)
		{
			if(it->nSignal == 3)
			{
			  it->nSignal = 1;
				++it;
			}
			else
				state->vSignals.erase(it++);
		}	
		*/

	}
	else
	{
		int size = state->vSignals.size();
		int i = size;
		while ( i-- )
		{
			AISIGNAL signal = state->vSignals[i];
			if ( signal.nSignal == AISIGNAL_INITIALIZATION )
			{
				// process only init. signals!
				state->vSignals.erase( state->vSignals.begin() + i );
				SendSignal( signal.nSignal, signal.strText, (IEntity*)signal.pSender, signal.pEData );
				if ( signal.pEData )
					gEnv->pAISystem->FreeSignalExtraData( signal.pEData );
				if ( --size != state->vSignals.size() )
				{
					// a new signal was inserted while processing this one
					// so, let's start over
					i = size = state->vSignals.size();
				}
			}
		}
	}

	UpdateAuxSignal(state);

	if (!m_pAIHandler)
		return;

	if (state->bReevaluate)
		m_pAIHandler->AIMind(state);

	if (m_IsWeaponForceAlerted != state->forceWeaponAlertness)
	{
		m_pAIHandler->UpdateWeaponAlertness();
		m_IsWeaponForceAlerted = state->forceWeaponAlertness;
	}

	state->bReevaluate = false;
	return;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::SendSignal(int signalID, const char * szText, IEntity *pSender, const IAISignalExtraData* pData)
{
	if( !m_pAIHandler )
		return;
	m_pAIHandler->AISignal( signalID, szText, pSender, pData );
}

// Updates the readibility signal handling, filters signals and calls the IAHandler to player when ready.
//----------------------------------------------------------------------------------------------------------
void CAIProxy::UpdateAuxSignal(SOBJECTSTATE* pStates)
{
	if (!m_pAIHandler)
		return;

	// Check to process a new signal.
	if (pStates->nAuxSignal)
	{
		// Filter out overlapping readibilities.
		bool	allowPlay = true;
		if (!m_readibilityName.empty())
		{
			// If the readibility has same priority or lower, skip it.
			if (pStates->nAuxPriority <= m_readibilityPriority || pStates->nAuxPriority >= 100)
				allowPlay = false;
		}

		if (allowPlay)
		{
			// No readibility playing currently, play the new one.
			m_readibilityName = pStates->szAuxSignalText;
			m_readibilityDelay = pStates->fAuxDelay;
			m_readibilityId = pStates->nAuxSignal;
			m_readibilityPriority = pStates->nAuxPriority;
			m_readibilityIgnored = 0;
			m_readibilityPlayed = false;
			gEnv->pAISystem->LogComment("AIProxy", "UpdateAuxSignal: Playing %s delay %f", m_readibilityName.c_str(), m_readibilityDelay );
		}
		else
		{
			gEnv->pAISystem->LogComment("AIProxy", "UpdateAuxSignal: Ignored %s (pri=%d) playing %s (pri=%d)", pStates->szAuxSignalText.c_str(), pStates->nAuxPriority, m_readibilityName.c_str(), m_readibilityPriority );
			m_readibilityIgnored++;
		}

		// Mark the aux signal as handled.
		pStates->nAuxSignal = 0;
	}

	// Update current redibility
	if (!m_readibilityName.empty())
	{
		if (!m_readibilityPlayed)
		{
			if (m_readibilityDelay > 0.0f)
				m_readibilityDelay -= gEnv->pAISystem->GetFrameDeltaTime();
			else
			{
				if (m_pAIHandler->DoReadibilityPack(m_readibilityName.c_str(), m_readibilityId))
				{
					// Played the readibility, wait for the sound event to completed.
					m_readibilityPlayed = true;
				}
				else
				{
					// Failed to play the readibility.
					m_readibilityName.clear();
					m_readibilityPlayed = false;
					m_readibilityDelay = 0;
					m_readibilityId = 0;
					m_readibilityPriority = 0;
				}
			}
		}
		else
		{
			if (m_pAIHandler->HasReadibilitySoundFinished())
			{
				m_readibilityName.clear();
				m_readibilityPlayed = false;
				m_readibilityDelay = 0;
				m_readibilityId = 0;
				m_readibilityPriority = 0;
			}
		}
	}
}

//
// gets the corners of the tightest projected bounding rectangle in 2D world space coordinates
//----------------------------------------------------------------------------------------------------------
void CAIProxy::GetWorldBoundingRect(Vec3& FL, Vec3& FR, Vec3& BL, Vec3& BR, float extra) const
{
	const Vec3 pos = m_pGameObject->GetEntity()->GetWorldPos();
	const Quat &quat = m_pGameObject->GetEntity()->GetRotation();
	Vec3 dir = quat * Vec3(0, 1, 0);
	dir.z = 0.0f;
	dir.NormalizeSafe();

	const Vec3 sideDir(dir.y, -dir.x, 0.0f);

	IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)m_pGameObject->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	AABB bounds;
	pRenderProxy->GetLocalBounds(bounds);

	bounds.max.x += extra;
	bounds.max.y += extra;
	bounds.min.x -= extra;
	bounds.min.y -= extra;

	FL = pos + bounds.max.y * dir + bounds.min.x * sideDir;
	FR = pos + bounds.max.y * dir + bounds.max.x * sideDir;
	BL = pos + bounds.min.y * dir + bounds.min.x * sideDir;
	BR = pos + bounds.min.y * dir + bounds.max.x * sideDir;
}

//
//----------------------------------------------------------------------------------------------------------
int CAIProxy::GetAlertnessState() const
{
	if( !m_pAIHandler )
		return 0;
	return m_pAIHandler->m_CurrentAlertness;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::IsCurrentBehaviorExclusive() const
{
	if( !m_pAIHandler )
		return false;
	return m_pAIHandler->m_CurrentExclusive;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::SetCharacter(const char* character, const char* behavior)
{
	if (!m_pAIHandler)
		return true;
	bool ret =  m_pAIHandler->SetCharacter(character);
	if (behavior)
		m_pAIHandler->SetBehavior(behavior);
	return ret;
}

//
//----------------------------------------------------------------------------------------------------------
const char* CAIProxy::GetCharacter()
{
	if (!m_pAIHandler)
		return 0;
	return m_pAIHandler->GetCharacter();
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::TestReadabilityPack(bool start, const char* szReadability)
{
	m_pAIHandler->TestReadabilityPack(start, szReadability);
}

//
//----------------------------------------------------------------------------------------------------------
const char* CAIProxy::GetCurrentReadibilityName() const
{
	if(m_readibilityName.empty())
		return 0;
	else
		return m_readibilityName.c_str();
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::GetReadabilityBlockingParams(const char* text, float& time, int& id)
{
	if(!m_pAIHandler)
	{
		time = 0;
		id = 0;
		return;
	}
	CAIHandler::s_ReadabilityManager.GetReadabilityBlockingParams(m_pAIHandler, text, time ,id);
}

//
//----------------------------------------------------------------------------------------------------------
void CAIProxy::Serialize( TSerialize ser )
{
	if (ser.IsReading())
	{
		m_pIActor = 0; // forces re-caching in QueryBodyInfo
/*		m_enabledTargetPointVerifier = false; // reenable

		// TODO: make sure animation state is serialized and then serialize all of these:
		{
			if ( m_pActorTargetParams )
				delete m_pActorTargetParams;
			m_pActorTargetParams = NULL;
			m_pActorTargetRequester = NULL;

			if ( m_pPendingActorTargetParams )
				delete m_pPendingActorTargetParams;
			m_pPendingActorTargetParams = NULL;
			m_pPendingActorTargetRequester = NULL;

			if ( m_pAGState )
				m_pAGState->RemoveListener( this );
			m_pAGState = NULL;
			m_startingQueryID = 0;
			m_startedQueryID = 0;
			m_endQueryID = 0;
			m_bAnimationStarted = false;
			m_ePhase = eATP_Error;// eATP_None; temp hack waiting for AG serialization
			m_changeActionInputQueryId = 0;
			m_changeSignalInputQueryId = 0;
			m_sAGActionSOAutoState.clear();
		}*/
	}

	if( !m_pAIHandler )
		return;
	ser.BeginGroup( "AIProxy" );

//	SActorTargetParams*				m_pActorTargetParams;
//	IAnimationGraphStateListener*	m_pActorTargetRequester;
//	SActorTargetParams*				m_pPendingActorTargetParams;
//	IAnimationGraphStateListener*	m_pPendingActorTargetRequester;

//	IAnimationGraphState* m_pAGState;
/*
	ser.Value("m_startingQueryID",m_startingQueryID);
	ser.Value("m_startedQueryID",m_startedQueryID);
	ser.Value("m_endQueryID",m_endQueryID);

	ser.Value("m_bAnimationStarted",m_bAnimationStarted);
	ser.EnumValue("m_ePhase",m_ePhase,eATP_Cancel,eATP_Error);
	ser.Value("m_changeActionInputQueryId", m_changeActionInputQueryId);
	ser.Value("m_changeSignalInputQueryId", m_changeSignalInputQueryId);
	ser.Value("m_sAGActionSOAutoState",m_sAGActionSOAutoState);
*/
//	ser.Value("m_vAnimationTargetPosition",m_vAnimationTargetPosition);
//	ser.Value("m_qAnimationTargetOrientation",m_qAnimationTargetOrientation);
	//m_pGameObject
	m_pAIHandler->Serialize(ser);
	ser.Value("m_firing",m_firing);
	ser.Value("m_firingSecondary",m_firingSecondary);
//	ser.Value("m_enabledTargetPointVerifier",m_enabledTargetPointVerifier);
	ser.Value("m_readibilityName",m_readibilityName);
	ser.Value("m_readibilityDelay",m_readibilityDelay);
	ser.Value("m_readibilityPlayed",m_readibilityPlayed);
	ser.Value("m_readibilityPriority",m_readibilityPriority);
	ser.Value("m_readibilityId",m_readibilityId);
	ser.Value("m_readibilityIgnored",m_readibilityIgnored);
	ser.Value("m_fMinFireTime",m_fMinFireTime);
	ser.Value("m_WeaponShotIsDone",m_WeaponShotIsDone);
	ser.Value("m_NeedsShootSignal",m_NeedsShootSignal);
	ser.Value("m_PrevAlertness",m_PrevAlertness);
	ser.Value("m_IsDisabled",m_IsDisabled);
	ser.Value("m_UpdateAlways",m_UpdateAlways);
	ser.Value("m_IsWeaponForceAlerted",m_IsWeaponForceAlerted);
	ser.Value("m_lastShotTime", m_lastShotTime);
	ser.Value("m_shotBulletCount", m_shotBulletCount);

	CheckUpdateStatus();

	//IActor	*m_pIActor;

	ser.EndGroup();
}


//
//---------------------------------------------------------------------------------------------------------------
bool CAIProxy::PredictProjectileHit(const Vec3& throwDir, float vel, Vec3& posOut, float& speedOut,
																		Vec3* pTrajectory, unsigned int* trajectorySizeInOut)
{
	IWeapon* pGrenadeWeapon = GetSecWeapon();
	if (!pGrenadeWeapon)
		return false;

	SAIBodyInfo bodyInfo;
	QueryBodyInfo(bodyInfo);

	return pGrenadeWeapon->PredictProjectileHit(GetPhysics(), bodyInfo.vFirePos, throwDir, Vec3(0,0,0), vel,
		posOut, speedOut, pTrajectory, trajectorySizeInOut);
}

//
//---------------------------------------------------------------------------------------------------------------
int CAIProxy::GetActorHealth()
{
	IActor*	pActor = GetActor();
	if (pActor)
		return pActor->GetHealth();

	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	IVehicle* pVehicle = pVehicleSystem->GetVehicle(m_pGameObject->GetEntityId());
	if (pVehicle)
		return (int)((1 - pVehicle->GetDamageRatio()) * 1000.0f);

	return 0;
}

//
//---------------------------------------------------------------------------------------------------------------
int CAIProxy::GetActorMaxHealth()
{
	IActor*	pActor = GetActor();
	if (pActor)
		return m_pIActor->GetMaxHealth();

	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	IVehicle* pVehicle = pVehicleSystem->GetVehicle(m_pGameObject->GetEntityId());
	if (pVehicle)
		return 1000;

	return 1;
}

//
//---------------------------------------------------------------------------------------------------------------
int CAIProxy::GetActorArmor()
{
	IActor*	pActor = GetActor();
	if (pActor)
		return pActor->GetArmor();
	return 0;
}

//
//---------------------------------------------------------------------------------------------------------------
int CAIProxy::GetActorMaxArmor()
{
	IActor*	pActor = GetActor();
	if(pActor)
		return m_pIActor->GetMaxArmor();
	return 0;
}

//
//---------------------------------------------------------------------------------------------------------------
bool CAIProxy::GetActorIsFallen() const
{
	IActor*	pActor = GetActor();
	if ( pActor == NULL )
		return false;
	return m_pIActor->IsFallen();
}

//
//---------------------------------------------------------------------------------------------------------------
bool CAIProxy::IsDead() const
{
	if( const IActor* pActor = (const_cast<CAIProxy*>(this))->GetActor() )
			return pActor->GetHealth()>0;
	IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	IVehicle* pVehicle = pVehicleSystem->GetVehicle(m_pGameObject->GetEntityId());
	if ( pVehicle )
		return pVehicle->IsDestroyed();
	return false;
}

//
//----------------------------------------------------------------------------------------------------------
int CAIProxy::PlayReadabilitySound(const char* szReadability, bool stopPreviousSound)
{
	if (!m_pAIHandler)
		return INVALID_SOUNDID;
	m_pAIHandler->DoReadibilityPack(szReadability, SIGNALFILTER_READIBILITY, true, stopPreviousSound); // Sound only
	return m_pAIHandler->m_ReadibilitySoundID;
}

/*
Vec2 CAGAnimation::GetMinMaxSpeed( IEntity * pEntity )
{
	if (ICharacterInstance * pInst = pEntity->GetCharacter )
	{
		int id = pInst->GetIAnimationSet()->GetAnimIDByName(m_animations[0].c_str());
		if (id >= 0)
		{
			return pInst->GetIAnimationSet()->GetMinMaxSpeedAsset_msec(id);
		}
	}
	return Vec2(0,0);
}
*/
//
//----------------------------------------------------------------------------------------------------------
bool CAIProxy::QueryCurrentAnimationSpeedRange(float& smin, float& smax)
{
	if (!m_pAIHandler)
		return false;

	ICharacterInstance* pCharacter(m_pGameObject->GetEntity()->GetCharacter(0));
	if (!pCharacter)
		return false;
	ISkeletonAnim* pSkeleton = pCharacter->GetISkeletonAnim();
	if (!pSkeleton)
		return false;

	float weight = 0.0f;
	smin = 0.0f;
	smax = 0.0f;
	for (int i = 0, ni = pSkeleton->GetNumAnimsInFIFO(0); i < ni; ++i)
	{
		const CAnimation& anim = pSkeleton->GetAnimFromFIFO(0, i);
		if (!anim.m_bActivated) continue;
		
		int id = 0;
		if (anim.m_LMG0.m_nLMGID >= 0)
			id = anim.m_LMG0.m_nLMGID;
		else
			id = anim.m_LMG0.m_nAnimID[0];

		Vec2 range = pCharacter->GetIAnimationSet()->GetMinMaxSpeedAsset_msec(id);
		if (range.x >= 0.0f && range.y >= 0.0f)
		{
			smin += range.x * anim.m_fTransitionWeight;
			smax += range.y * anim.m_fTransitionWeight;
			weight += anim.m_fTransitionWeight;
		}
	}

	if (weight <= 0.0f)
		return false;

	weight = 1.0f / weight;
	smin *= weight;
	smax *= weight;

	if (smin < 0.0f || smax < smin)
		return false;
	
	return true;


/*	Vec2 speeds = m_pAIHandler->GetAGState()->GetQueriedStateMinMaxSpeed();
	smin = speeds.x;
	smax = speeds.y;*/

}

//====================================================================
// 
//====================================================================
float GetTofPointOnParabula(const Vec3& A, const Vec3& V, const Vec3& P, float g)
{
	float a = -g/2;
	float b = V.z;
	float c = A.z - P.z;
	float d = b*b - 4*a*c;
	if(d<0)
		return -1.f;
	float sqrtd = sqrt(d);
	float tP = (-b + sqrtd)/ (2*a);
	float tP1 = (-b - sqrtd)/ (2*a);
	float y = V.y*tP + A.y;
	float y1 = V.y*tP1 + A.y;
	if(fabs(P.y - y) > fabs(P.y - y1) )
		return tP1;
	return tP;
}

Vec3 GetPointOnParabula(const Vec3& A, const Vec3& V, float t, float g)
{
	return Vec3(V.x * t + A.x, V.y * t + A.y, -g/2*t*t + V.z*t + A.z);
}

//====================================================================
// IntersectSweptSphere
// hitPos is optional - may be faster if 0
//====================================================================
bool IntersectSweptSphereWrapper(Vec3 *hitPos, float& hitDist, const Lineseg& lineseg, float radius,IPhysicalEntity** pSkipEnts=0, int nSkipEnts=0, int additionalFilter = 0)
{
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	primitives::sphere spherePrim;
	spherePrim.center = lineseg.start;
	spherePrim.r = radius;
	Vec3 dir = lineseg.end - lineseg.start;

	geom_contact *pContact = 0;
	geom_contact **ppContact = hitPos ? &pContact : 0;
	int geomFlagsAll=0;
	int geomFlagsAny=rwi_stop_at_pierceable|(geom_colltype_player<<rwi_colltype_bit);

	float d = pPhysics->PrimitiveWorldIntersection(spherePrim.type, &spherePrim, dir, 
		ent_static | ent_terrain | ent_ignore_noncolliding | additionalFilter, ppContact, 
		geomFlagsAll, geomFlagsAny, 0,0,0, pSkipEnts, nSkipEnts);

	if (d > 0.0f)
	{
		hitDist = d;
		if (pContact && hitPos)
			*hitPos = pContact->pt;
		return true;
	}
	else
	{
		return false;
	}
}

// 
//====================================================================
bool CAIProxy::CanJumpToPoint(Vec3& dest, float theta, float maxheight, int flags, Vec3& retVelocity, float& tDest, IPhysicalEntity* pSkipEnt, Vec3* pStartPos)
{
	IEntity* pEntity = m_pGameObject->GetEntity();
	if(!pEntity)
	{
		gEnv->pAISystem->Warning(">> ","CAIProxy::OnShoot null entity");
		return false;
	}

	IEntity* pEntity2 = NULL;

	Vec3 hitPos;
	float hitdist;


	SAIBodyInfo bodyInfo;
	QueryBodyInfo(bodyInfo);
	float radius = bodyInfo.colliderSize.GetSize().x;
	float radius2 = bodyInfo.colliderSize.GetSize().y;
	if(radius < radius2)
		radius = radius2;
	float myHeight = bodyInfo.colliderSize.GetSize().z;
	radius = sqrt(radius*radius + myHeight*myHeight);
	radius = radius*1.2f/2;

	float tB=0; // time at which entity will reach B
	Vec3 B(dest);
	/*
	int building;
	IVisArea *pArea;
	IAIActor* pActor = CastToIAIActorSafe(pAI);
	if(!pActor)
		return false;

	IAISystem::tNavCapMask navProperties =  pActor->GetMovementAbility().pathfindingProperties.navCapMask;
	IAISystem::ENavigationType navType = gEnv->pAISystem->CheckNavigationType(B,building,pArea,navProperties);
	*/
	Vec3 A(pStartPos? *pStartPos : pEntity->GetWorldPos());//initial position

	theta = DEG2RAD(theta);

	bool bCheckCollision = (flags & AI_JUMP_CHECK_COLLISION)!=0;

	IPhysicalEntity* pSkipMyEnt = pEntity->GetPhysics();
	IPhysicalEntity* SkipEnt[2];
	std::vector<IPhysicalEntity*> VSkipEnt;

	SkipEnt[0] = pSkipMyEnt;

	int nSkipMyEnts = pSkipMyEnt ? 1 : 0;
	if(pSkipEnt)
		SkipEnt[nSkipMyEnts] = pSkipEnt;

	int nSkipEnts = pSkipEnt ? nSkipMyEnts + 1 : nSkipMyEnts;


	if(flags & AI_JUMP_ON_GROUND)
	{
		// project B on terrain		
		ray_hit	hit;
		Vec3 rayDir(0,0,-30);
		float heightTerrain = gEnv->p3DEngine->GetTerrainElevation( B.x, B.y );
		if(B.z < heightTerrain)
			B.z = heightTerrain;

		if (gEnv->pPhysicalWorld->RayWorldIntersection(B, rayDir, ent_static|ent_terrain, rwi_stop_at_pierceable|(geom_colltype_player<<rwi_colltype_bit), &hit, 1,SkipEnt, nSkipEnts))
			B = hit.pt;
	}



	Vec3 AB = B-A;

	if(AB.z > maxheight)
	{
		return false;
	}

	float cosTheta = cos(theta);

	Vec3 dir(AB);
	if(fabs(cosTheta)< 0.01f)
	{ // almost vertical, let's approximate to it to avoid lua imprecisions
		cosTheta = 0;
		theta = gf_PI/2;
		dir.x = dir.y = 0;
	}

	dir.z=0;
	float x = dir.GetLength();
	dir.NormalizeSafe();

	float tanTheta = cosTheta != 0 ? tan(theta) : 0;

	// check if the initial angle is too low for destination
	float cosDestAngle = dir.Dot(AB.GetNormalizedSafe());

	// adding a threshold to avoid super velocities when destination angle is just slightly below requested angle
	float thr = 0.05f;
	// (assuming negative gravity)
	if(B.z > A.z) // destination above
	{
		if( theta<=0 || (cosTheta > cosDestAngle - thr) && cosTheta!=0)
			return false;
	}
	else if(theta<=0 && (cosTheta < cosDestAngle + thr) && cosTheta!=0)
		return false;

	float g = 9.81f;  

	IPhysicalEntity *phys = pEntity->GetPhysics();
	Vec3 vAddVelocity(ZERO);
	float addSpeed = 0;

	if(phys)
	{
		pe_player_dynamics	dyn;
		phys->GetParams(&dyn);

		// use current actual gravity	of the object
		g = -dyn.gravity.z;

		if(flags & AI_JUMP_RELATIVE)
		{
			// jump in the entity's reference frame
			pe_status_dynamics	dyns;
			phys->GetStatus(&dyns);
			vAddVelocity = dyns.v;
			addSpeed = vAddVelocity.GetLength();
		}

	}
	/*
	if((flags & AI_JUMP_MOVING_TARGET) !=0  && pEntity2)
	{
	int i=10;
	while(pEntity2->GetParent() && i--)
	pEntity2 = pEntity2->GetParent();

	IPhysicalEntity *phys = pEntity2->GetPhysics();

	if(phys)
	{
	pe_status_dynamics	dyns;
	phys->GetStatus(&dyns);
	vAddVelocity += dyns.v;
	addSpeed = vAddVelocity.GetLength();
	flags |= AI_JUMP_RELATIVE;
	}
	}
	*/

	float z0 = AB.z;
	float v; //initial velocity (module), the incognita

	if(cosTheta!=0)
	{
		float t0 = sqrt(fabs(2/g *(z0 - tanTheta *x)));
		v = x/(cosTheta*t0);
	}
	else
	{
		if(x == 0) // jumping vertically
		{	
			// use the given height as parameter, v = v(AB.z)
			if(z0<0)
				return false;
			v = sqrt(2* z0 *g);
		}
		else 
			return false;
	}

	// check max height
	float Xc; // max distance on origin's plane
	float ZMax; // max height with v
	float tMax;
	Vec3 dir90;//(dir.y,-dir.x,0);
	Vec3 VelN;//(dir.GetRotated(dir90,theta));

	if(x !=0) // not vertical
	{
		dir90.Set(dir.y,-dir.x,0);
		VelN = dir.GetRotated(dir90,theta);
		if(flags & AI_JUMP_RELATIVE)
		{
			VelN = (VelN*v + vAddVelocity);
			v = VelN.GetLength();
			if(v>0)
				VelN /= v;
		}
		Xc = sin(2*theta)*v*v /g; // max distance on origin's plane
		ZMax = Xc * tanTheta / 4; // max height with v
		if(ZMax > maxheight)
			return false;
		tMax = Xc/v;
	}
	else
	{
		VelN.Set(0,0,1);
		if(flags & AI_JUMP_RELATIVE)
		{
			VelN = (VelN*v + vAddVelocity);
			v = VelN.GetLength();
			if(v>0)
				VelN /= v;
		}
		tMax = 0; //ignored
		Xc = 0;
		ZMax = maxheight;
	}


	Vec3 V(VelN * v); // initial velocity vector;

#ifdef USER_luciano
	// shouldn't compile in gcc
	gEnv->pAISystem->AddDebugSphere(B,0.5,128,128,255,5);
	gEnv->pAISystem->AddDebugLine(B-Vec3(1,0,0),B+Vec3(1,0,0),128,128,255,5);
	gEnv->pAISystem->AddDebugLine(B-Vec3(0,1,0),B+Vec3(0,1,0),128,128,255,5);
#endif

	if(bCheckCollision)
	{

		//---------------------------------------------------
		//check obstacles in trajectory
		//---------------------------------------------------
		Vec3 P(A + VelN*radius);

		if(x< radius)
		{
			// (almost) vertical jump
			Lineseg T(P,B);
			gEnv->pAISystem->AddDebugLine(P,B,128,40,255,5);

			if(IntersectSweptSphereWrapper(&hitPos, hitdist, T, radius*1.2f, nSkipEnts ? SkipEnt : NULL, nSkipEnts))
				return false;
		}
		else if(theta>0)
		{

			float halfXc = Xc/2;

			float Xm = Xc/2;
			float invTanTheta = 1/tanTheta;
			Vec3 Zm(A.x + dir.x*Xm, A.y + dir.y*Xm, A.z + ZMax);

			/*
			x = Vx*t + Ax
			y = Vy*t + Ay
			z = -g/2*t^2 + Vz*t + Az
			*/
			// Zm is such that t = ...
			float vx = v*cosTheta;
			// x=vt
			float tZm = Xm/vx;
			float tMax = 2*tZm; // time to run the parabola to the next point with same Z as start point

			// B is such that t = ...
			float tB = GetTofPointOnParabula(A,V,B,g); // time to reach destination
			if(tB<0) // no solution
				return false;

			float radiusd = 2*radius;
			float t = 0;
			Vec3 P(A + VelN * radius); // start point for collision check
			Vec3 Vb(V.x,V.y, -g*tB + V.z); // velocity in end point B
			float speedB = Vb.GetLength();
			if(speedB==0)
				return false;

			float tB1 = tB - radiusd/speedB; // time to reach approximate point to end collision check

			float tStep = tB/4;

			//Vec3 Q(B - Vb.GetNormalizedSafe() * radiusd); // end point for collision check

			float zOffset = bodyInfo.colliderSize.GetCenter().z;
			P.z += zOffset;

			ICVar* aiDeBugDraw = gEnv->pConsole->GetCVar("ai_DebugDraw");
			if(aiDeBugDraw && aiDeBugDraw->GetIVal())
			{
				float step = tB/40;
				for(float t2= 0;t2<tB;t2+=step)
				{
					Vec3 P1 = GetPointOnParabula(A,V,t2,g);
					Vec3 P2 = GetPointOnParabula(A,V,t2+step,g);
					gEnv->pAISystem->AddDebugLine(P1,P2,255,255,0,6);
				}
			}

			ray_hit hit;

			for(int i=0;i<4;i++)
			{
				float t1 = t + tStep;
				//					Vec3 P1 = getPointOnParabula(A,V,t1,g);
				if(tB1 < t1 || i==3 && tB1>=t1) // next point will be end point
				{
					i = 4; // last loop
					t1 = tB1;
					//P1 = Q;
				}
				Vec3 P1 = GetPointOnParabula(A,V,t1,g);
				Vec3 Pr(P.x,P.y, P.z - zOffset); 
				if( gEnv->pPhysicalWorld->RayWorldIntersection( Pr, P1 - Pr, ent_static|ent_terrain|ent_living, rwi_stop_at_pierceable|(geom_colltype_player<<rwi_colltype_bit), &hit, 1,SkipEnt,nSkipEnts ) )
					return false;
				P1.z += zOffset;
				Lineseg T(P ,P1 );
				gEnv->pAISystem->AddDebugLine(P,P1,40,40,255,8);
				if(IntersectSweptSphereWrapper(&hitPos, hitdist, T, radius, nSkipEnts ? SkipEnt:NULL, nSkipEnts,ent_sleeping_rigid))
					return false;
				P = P1;	
				t = t1;
			}

		}
		else //theta <=0
		{
			// TO DO not supported yet
			// ZMax calculations above won't be valid here
			return false;
		}

	}

	gEnv->pAISystem->AddDebugLine(A,A+VelN*4,255,255,255,5);

	// compute duration
	if(tB==0)
	{
		float a = -g/2;
		float b = V.z;
		float c = A.z - B.z;
		float d = b*b - 4*a*c;
		float sqrtd = (x==0? 0:sqrt(d));
		tB = (-b + sqrtd)/ (2*a);
		float tB1 = (-b - sqrtd)/ (2*a);
		float y = V.y*tB + A.y;
		float y1 = V.y*tB1 + A.y;
		if(fabs(B.y - y) > fabs(B.y - y1) )
			tB = tB1;
	}

	retVelocity = V;
	tDest = tB;

	return true;
}
