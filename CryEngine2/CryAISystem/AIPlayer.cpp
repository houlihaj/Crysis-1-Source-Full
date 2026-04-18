#include "StdAfx.h"
#include <Cry_Camera.h>
#include "AIPlayer.h"
#include "Leader.h"
#include <ISystem.h>
#include "CAISystem.h"
#include "IEntity.h" // temporary, for getting physics of player
#include "Puppet.h"


// The variables needs to be carefully tuned to possible the player action speeds.
static const float PLAYER_ACTION_SPRINT_RESET_TIME = 0.5f;
static const float PLAYER_ACTION_JUMP_RESET_TIME = 1.3f;
static const float PLAYER_ACTION_CLOAK_RESET_TIME = 1.5f;

static const float PLAYER_IGNORE_COVER_TIME = 6.0f;


//
//---------------------------------------------------------------------------------
CAIPlayer::CAIPlayer(CAISystem* pAISystem):
	CAIActor(pAISystem),
	m_pAttentionTarget(NULL),
	m_FOV(0),
	m_playerStuntSprinting(-1.0f),
	m_playerStuntJumping(-1.0f),
	m_playerStuntCloaking(-1.0f),
	m_playerStuntUncloaking(-1.0f),
	m_stuntDir(0,0,0),
	m_mercyTimer(-1.0f),
	m_coverExposedTime(-1.0f)
{
	_fastcast_CAIPlayer = true;
	m_Tracker.SetOwner(this);
}

//
//---------------------------------------------------------------------------------
CAIPlayer::~CAIPlayer()
{
	ReleaseExposedCoverObjects();
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::Reset(EObjectResetType type)
{
	CAIActor::Reset(type);
	m_fLastUpdateTargetTime = 0.f;

	m_deathCount = 0;
	m_lastThrownItems.clear();
	m_stuntTargets.clear();
	m_stuntDir.Set(0,0,0);
	m_mercyTimer = -1.0f;
	m_coverExposedTime = -1.0f;
	
	ReleaseExposedCoverObjects();
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::ReleaseExposedCoverObjects()
{
	for (unsigned i = 0, ni = m_exposedCoverObjects.size(); i < ni; ++i)
		m_exposedCoverObjects[i].pPhysEnt->Release();
	m_exposedCoverObjects.clear();
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::AddExposedCoverObject(IPhysicalEntity* pPhysEnt)
{
	unsigned oldest = 0;
	float oldestTime = FLT_MAX; // Count down timers, find smallest value.
	for (unsigned i = 0, ni = m_exposedCoverObjects.size(); i < ni; ++i)
	{
		SExposedCoverObject& co = m_exposedCoverObjects[i];
		if (co.pPhysEnt == pPhysEnt)
		{
			co.t = PLAYER_IGNORE_COVER_TIME;
			return;
		}
		if (co.t < oldestTime)
		{
			oldest = i;
			oldestTime = co.t;
		}
	}

	// Limit the number of covers, override oldest one.
	if (m_exposedCoverObjects.size() >= 3)
	{
		// Release the previous entity
		m_exposedCoverObjects[oldest].pPhysEnt->Release();
		// Fill in new.
		pPhysEnt->AddRef();
		m_exposedCoverObjects[oldest].pPhysEnt = pPhysEnt;
		m_exposedCoverObjects[oldest].t = PLAYER_IGNORE_COVER_TIME;
	}
	else
	{
		// Add new
		pPhysEnt->AddRef();
		m_exposedCoverObjects.push_back(SExposedCoverObject(pPhysEnt, PLAYER_IGNORE_COVER_TIME));
	}
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::CollectExposedCover()
{
	if (m_coverExposedTime > 0.0f)
	{
		const Vec3& pos = GetPos();

		// Find the objects which the player is hiding in.
/*		const Vec3 rad(1,1,1);
		IPhysicalEntity **ppList = NULL;
		int	numEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(pos - rad, pos + rad, ppList, ent_static);
		for (int i = 0; i < numEntities; ++i)
		{
			pe_status_contains_point scp;
			scp.pt = pos;
			if (ppList[i]->GetStatus(&scp))
				AddExposedCoverObject(ppList[i]);
		}*/

		// Find the object directly in front of the player
		const int flags = rwi_colltype_any | (geom_colltype_obstruct << rwi_colltype_bit) | (VIEW_RAY_PIERCABILITY & rwi_pierceability_mask);
		ray_hit hit;
		if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, GetViewDir() * 3.0f, ent_static, flags, &hit, 1))
		{
			if (hit.pCollider)
				AddExposedCoverObject(hit.pCollider);
		}
	}
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::GetPhysicsEntitiesToSkip(std::vector<IPhysicalEntity*>& skips) const
{
	CAIActor::GetPhysicsEntitiesToSkip(skips);
	// Skip exposed covers
	for (unsigned i = 0, ni = m_exposedCoverObjects.size(); i < ni; ++i)
		skips.push_back(m_exposedCoverObjects[i].pPhysEnt);
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::ParseParameters(const AIObjectParameters & params)
{
	CAIActor::ParseParameters( params );
	m_Parameters = params.m_sParamStruct;
	SetProxy(params.pProxy);
}

//
//---------------------------------------------------------------------------------
IPhysicalEntity* CAIPlayer::GetPhysics(bool wantCharacterPhysics=false) const
{
	// temporary, should access a proxy instead of going directly to entity
	IEntity *pEntity = GetEntity();
	if(pEntity)
		return pEntity->GetPhysics();
	return NULL;
}

//
//---------------------------------------------------------------------------------
bool CAIPlayer::IsPointInFOVCone(const Vec3 &pos, float distanceScale)
{
	Vec3 direction = pos - GetPos();

	// lets see if it is outside of its vision range
	if (direction.GetLengthSquared() > sqr(m_Parameters.m_PerceptionParams.sightRange * distanceScale))
		return false; 

	//TODO remove this - needed now coz vehicle system puts passanger/driver at the same point
	if (direction.GetLengthSquared() < .1f )
		return false; 


	Vec3 myorievector = GetViewDir();
	float dist = direction.GetLength();
	if(dist>0)
		direction /=dist;

	float fdot = ((Vec3)direction).Dot((Vec3)myorievector);

	if ( fdot <  0 || fdot<m_FOV)
		return false;	// its outside of his FOV
	
	return true;
}


//
//---------------------------------------------------------------------------------
void CAIPlayer::UpdateAttentionTarget(CAIObject *pTarget)
{
	bool bUpdateLeaderStats = false;

	if(!bUpdateLeaderStats || m_pAttentionTarget==pTarget)
	{
		bUpdateLeaderStats = m_pAttentionTarget!=pTarget;
		m_pAttentionTarget = pTarget;
		m_fLastUpdateTargetTime = GetAISystem()->GetFrameStartTime();
	}
	else // compare the new target with the current one
	{ 
		Vec3 direction = m_pAttentionTarget->GetPos() - GetPos();

		// lets see if it is outside of its vision range

		Vec3 myorievector = GetViewDir();
		float dist = direction.GetLength();
		
		if(dist>0)
			direction /=dist;
		Vec3 directionNew(pTarget->GetPos() - GetPos());

		float distNew = directionNew.GetLength();
		if(distNew>0)
			directionNew /=dist;
		
		float fdot = ((Vec3)direction).Dot((Vec3)myorievector);
		// check if new target is more interesting, by checking if old target is still visible
		// and comparing distances if it is
		if ( fdot <  0 || fdot<m_FOV || distNew < dist)
		{
			m_pAttentionTarget = pTarget;
			m_fLastUpdateTargetTime = GetAISystem()->GetFrameStartTime();
			bUpdateLeaderStats = true;
		}
	}
	if(bUpdateLeaderStats)
	{
		CAIGroup* pGroup = GetAISystem()->GetAIGroup(GetGroupId());
		if(pGroup)
			pGroup->OnUnitAttentionTargetChanged();
	}
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::Update(EObjectUpdate type)
{
	if ((GetAISystem()->GetFrameStartTime()-m_fLastUpdateTargetTime).GetSeconds() >5.f 
		|| m_pAttentionTarget && !m_pAttentionTarget->IsEnabled())
	{
		m_pAttentionTarget = NULL;
	}

	// There should never be player without physics.
	if (!GetPhysics())
	{
		AIWarning("AIPlayer::Update Player %s does not have physics!", GetName());
		AIAssert(0);
		return;
	}

	// make sure to update direction when entity is not moved
	SAIBodyInfo bodyInfo;
	GetProxy()->QueryBodyInfo( bodyInfo);
	SetPos( bodyInfo.vEyePos );
	SetBodyDir( bodyInfo.vBodyDir );
	SetMoveDir( bodyInfo.vMoveDir );
	SetViewDir( bodyInfo.vEyeDir );

	m_bUpdatedOnce = true;

	m_FOV = cosf(GetISystem()->GetViewCamera().GetFov());
	
	if (m_pProxy)
		m_pProxy->UpdateMind(&m_State);

	m_Tracker.Update();

	m_lightLevel = GetAISystem()->GetLightManager()->GetLightLevelAt(GetPos(), this, &m_usingCombatLight);

/*
	if(GetDynamixTracker().IsMoving())
		gEnv->pLog->Log(" >>>  %.3f", GetDynamixTracker().GetSpeed());
	else
		gEnv->pLog->Log(" ---  %.3f", GetDynamixTracker().GetSpeed());
*/

	if (type == AIUPDATE_FULL)
	{
		// Health
		{
			RecorderEventData recorderEventData((float)GetProxy()->GetActorHealth());
			RecordEvent(IAIRecordable::E_HEALTH, &recorderEventData);
		}

		// Pos
		{
			RecorderEventData recorderEventData(GetPos());
			RecordEvent(IAIRecordable::E_AGENTPOS, &recorderEventData);
		}

	}

	const float dt = GetAISystem()->GetFrameDeltaTime();

	// Exposed (soft) covers. When the player fires the weapon, 
	// disable the cover where the player is hiding at or the
	// soft cover the player is shooting at.

	// Timeout the cover exposure timer.
	if (m_coverExposedTime > 0.0f)
		m_coverExposedTime -= dt;
	else
		m_coverExposedTime = -1.0f;

	if (GetProxy())
	{
		SAIWeaponInfo wi;
		GetProxy()->QueryWeaponInfo(wi);
		if (wi.isFiring)
			m_coverExposedTime = 1.0f;
	}

	// Timeout the exposed covers
	for (unsigned i = 0; i < m_exposedCoverObjects.size(); )
	{
		m_exposedCoverObjects[i].t -= dt;
		if (m_exposedCoverObjects[i].t < 0.0f)
		{
			m_exposedCoverObjects[i].pPhysEnt->Release();
			m_exposedCoverObjects[i] = m_exposedCoverObjects.back();
			m_exposedCoverObjects.pop_back();
		}
		else
			++i;
	}

	// Collect new covers.
	if (type == AIUPDATE_FULL)
		CollectExposedCover();


	if(GetAISystem()->m_cvDebugDrawDamageControl->GetIVal() > 0)
		UpdateHealthHistory();

	UpdatePlayerStuntActions();
	UpdateCloakScale();
	UpdateHostilityRange();

	// Update low health mercy pause
	if (m_mercyTimer > 0.0f)
		m_mercyTimer -= GetAISystem()->GetFrameDeltaTime();
	else
		m_mercyTimer = -1.0f;
	
}

void CAIPlayer::OnObjectRemoved(CAIObject* pObject)
{
	CPuppet* pRemovedPuppet = pObject->CastToCPuppet();
	if (!pRemovedPuppet)
		return;

	for (unsigned i = 0; i < m_stuntTargets.size(); )
	{
		if (m_stuntTargets[i].pPuppet == pRemovedPuppet)
		{
			m_stuntTargets[i] = m_stuntTargets.back();
			m_stuntTargets.pop_back();
		}
		else
			++i;
	}
}

void CAIPlayer::UpdatePlayerStuntActions()
{
	const float dt = GetAISystem()->GetFrameDeltaTime();

	// Update thrown entities
	for (unsigned i = 0; i < m_lastThrownItems.size(); )
	{
		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_lastThrownItems[i].id);
		if (pEnt)
		{
			if (IPhysicalEntity* pPhysEnt = pEnt->GetPhysics())
			{
				pe_status_dynamics statDyn;
				pPhysEnt->GetStatus(&statDyn);
				m_lastThrownItems[i].pos = pEnt->GetWorldPos();
				m_lastThrownItems[i].vel = statDyn.v;
				if (statDyn.v.GetLengthSquared() > sqr(3.0f))
					m_lastThrownItems[i].time = 0;
			}
		}

		m_lastThrownItems[i].time += dt;
		if (!pEnt || m_lastThrownItems[i].time > 1.0f)
		{
			m_lastThrownItems[i] = m_lastThrownItems.back();
			m_lastThrownItems.pop_back();
		}
		else
			++i;
	}

	Vec3 vel = GetVelocity();
	float speed = vel.GetLength();

	m_stuntDir = vel;
	if (m_stuntDir.GetLength() < 0.001f)
		m_stuntDir = GetBodyDir();
	m_stuntDir.z = 0;
	m_stuntDir.NormalizeSafe();

	if (m_playerStuntSprinting > 0.0f)
	{
		if (speed > 10.0f)
			m_playerStuntSprinting = PLAYER_ACTION_SPRINT_RESET_TIME;
		m_playerStuntSprinting -= dt;
	}

	if (m_playerStuntJumping > 0.0f)
	{
		if (speed > 7.0f)
			m_playerStuntJumping = PLAYER_ACTION_JUMP_RESET_TIME;
		m_playerStuntJumping -= dt;
	}

	if (m_playerStuntCloaking > 0.0f)
		m_playerStuntCloaking -= dt;

	if (m_playerStuntUncloaking > 0.0f)
		m_playerStuntUncloaking -= dt;


	bool checkMovement = m_playerStuntSprinting > 0.0f || m_playerStuntJumping > 0.0f;
	bool checkItems = !m_lastThrownItems.empty();

	if (checkMovement || checkItems)
	{
		float movementScale = 0.7f;
//		if (m_playerStuntJumping > 0.0f)
//			movementScale *= 2.0f;

		Lineseg	playerMovement(GetPos(), GetPos() + vel*movementScale);
		SAIBodyInfo bi;
		GetProxy()->QueryBodyInfo(bi);
		float playerRad = bi.stanceSize.GetRadius();

		// Update stunt effect on puppets
		AutoAIObjectIter it(GetAISystem()->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIOBJECT_PUPPET));
		for (; it->GetObject(); it->Next())
		{
			CPuppet* pPuppet = it->GetObject()->CastToCPuppet();
			if (!pPuppet) continue;
			if (!pPuppet->IsEnabled()) continue;
			if (!IsHostile(pPuppet)) continue;

			const float scale = pPuppet->GetParameters().m_PerceptionParams.collisionReactionScale;

			bool hit = false;

			float t, distSq;
			Vec3 threatPos;

			if (checkMovement)
			{
				// Player movement
				distSq = Distance::Point_Lineseg2DSq(pPuppet->GetPos(), playerMovement, t);
				if (distSq < sqr(playerRad * scale))
				{
					threatPos = GetPos();
					hit = true;
				}
			}

			if (checkItems)
			{
				// Thrown items
				for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
				{
					Lineseg	itemMovement(m_lastThrownItems[i].pos, m_lastThrownItems[i].pos + m_lastThrownItems[i].vel*2);
					distSq = Distance::Point_LinesegSq(pPuppet->GetPos(), itemMovement, t);
					if (distSq < sqr(m_lastThrownItems[i].r * 2.0f * scale))
					{
						threatPos = m_lastThrownItems[i].pos;
						hit = true;
					}
				}
			}

			if (hit)
			{
				bool found = false;
				for (unsigned i = 0, ni = m_stuntTargets.size(); i < ni; ++i)
				{
					if (m_stuntTargets[i].pPuppet == pPuppet)
					{
						m_stuntTargets[i].threatPos = threatPos;
						m_stuntTargets[i].exposed += dt;
						m_stuntTargets[i].t = 0;
						found = true;
						break;
					}
				}
				if (!found)
				{
					m_stuntTargets.push_back(SStuntTargetPuppet(pPuppet, threatPos));
					m_stuntTargets.back().exposed += dt;
				}
			}
		}
	}

	for (unsigned i = 0; i < m_stuntTargets.size(); )
	{
		m_stuntTargets[i].t += dt;
		if (!m_stuntTargets[i].signalled 
				&& m_stuntTargets[i].exposed > 0.15f 
				&& m_stuntTargets[i].pPuppet->GetAttentionTarget() 
				&& m_stuntTargets[i].pPuppet->GetAttentionTarget()->GetEntityID() == GetEntityID())
		{
			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			pData->iValue = 1;
			pData->fValue = Distance::Point_Point(m_stuntTargets[i].pPuppet->GetPos(), m_stuntTargets[i].threatPos);
			pData->point = m_stuntTargets[i].threatPos;
			m_stuntTargets[i].pPuppet->SetSignal(1, "OnCloseCollision", 0, pData);
			m_stuntTargets[i].pPuppet->SetAlarmed();

			m_stuntTargets[i].signalled = true;
		}
		if (m_stuntTargets[i].t > 2.0f)
		{
			m_stuntTargets[i] = m_stuntTargets.back();
			m_stuntTargets.pop_back();
		}
		else
			++i;
	}

}

bool CAIPlayer::IsDoingStuntActionRelatedTo(const Vec3& pos, float nearDistance)
{
	if (m_playerStuntCloaking > 0.0f)
		return true;

	if (m_playerStuntSprinting <= 0.0f && m_playerStuntJumping <= 0.0f)
		return false;

	// If the stunt is not done at really close range,
	// do not consider the stunt unless it is towards the specified position.
	Vec3 diff = pos - GetPos();
	diff.z = 0;
	float dist = diff.NormalizeSafe();
	const float thr = cosf(DEG2RAD(75.0f));
	if (dist > nearDistance && m_stuntDir.Dot(diff) < thr)
		return false;

	return true;
}

bool CAIPlayer::IsThrownByPlayer(EntityId id) const
{
	if (m_lastThrownItems.empty()) return false;
	for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
		if (m_lastThrownItems[i].id == id)
			return true;
	return false;
}

bool CAIPlayer::IsPlayerStuntAffectingTheDeathOf(CAIActor* pDeadActor) const
{
	if (!pDeadActor || !pDeadActor->GetEntity())
		return false;

	// If the actor is thrown/punched by the player.
	if (IsThrownByPlayer(pDeadActor->GetEntityID()))
		return true;

	// If any of the objects the player has thrown is close to the dead body.
	AABB deadBounds;
	pDeadActor->GetEntity()->GetWorldBounds(deadBounds);
	Vec3 deadPos = deadBounds.GetCenter();
	float deadRadius = deadBounds.GetRadius();

	for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
	{
		if (Distance::Point_PointSq(deadPos, m_lastThrownItems[i].pos) < sqr(deadRadius + m_lastThrownItems[i].r))
			return true;
	}

	return false;
}

EntityId CAIPlayer::GetNearestThrownEntity(const Vec3& pos)
{
	EntityId nearest = 0;
	float nearestDist = FLT_MAX;
	for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
	{
		float d = Distance::Point_Point(pos, m_lastThrownItems[i].pos) - m_lastThrownItems[i].r;
		if (d < nearestDist)
		{
			nearestDist = d;
			nearest = m_lastThrownItems[i].id;
		}
	}
	return nearest;
}

void CAIPlayer::AddThrownEntity(EntityId id)
{
	float oldestTime = 0.0f;
	unsigned oldestId = 0;
	for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
	{
		if (m_lastThrownItems[i].id == id)
		{
			m_lastThrownItems[i].time = 0;
			return;
		}
		if (m_lastThrownItems[i].time > oldestTime)
		{
			oldestTime = m_lastThrownItems[i].time;
			oldestId = i;
		}
	}

	IEntity* pEnt = gEnv->pEntitySystem->GetEntity(id);
	if (!pEnt)
		return;

	// The entity does not exists yet, add it to the list of entities to watch.
	m_lastThrownItems.push_back(SThrownItem(id));

	// Skip the nearest thrown entity, since it is potentially blocking the view to the corpse.
	IEntity* pThrownEnt = id ? gEnv->pEntitySystem->GetEntity(id) : 0;
	CAIActor* pThrownActor = CastToCAIActorSafe(pThrownEnt->GetAI());
	if (pThrownActor)
	{
		short gid = (short)pThrownActor->GetGroupId();
		AIObjects::iterator ai = GetAISystem()->m_mapGroups.find(gid);
		AIObjects::iterator end = GetAISystem()->m_mapGroups.end();
		for ( ; ai != end && ai->first == gid; ++ai)
		{
			CPuppet* pPuppet = ai->second->CastToCPuppet();
			if (!pPuppet) continue;
			if (pPuppet->GetEntityID() == pThrownActor->GetEntityID()) continue;
			float dist = FLT_MAX;
			if (!GetAISystem()->CheckVisibilityToBody(pPuppet, pThrownActor, dist))
				continue;
			pPuppet->SetSignal(1, "OnGroupMemberMutilated", pThrownActor->GetEntity(), 0);
			pPuppet->SetAlarmed();
		}
	}


	// Set initial position, radius and velocity.
	m_lastThrownItems.back().pos = pEnt->GetWorldPos();
	AABB bounds;
	pEnt->GetWorldBounds(bounds);
	m_lastThrownItems.back().r = bounds.GetRadius();

	if (IPhysicalEntity* pPhysEnt = pEnt->GetPhysics())
	{
		pe_status_dynamics statDyn;
		pPhysEnt->GetStatus(&statDyn);
		m_lastThrownItems.back().vel = statDyn.v;
	}

	// Limit the number of hot entities.
	if (m_lastThrownItems.size() > 4)
	{
		m_lastThrownItems[oldestId] = m_lastThrownItems.back();
		m_lastThrownItems.pop_back();
	}
}

void CAIPlayer::HandleCloaking()
{
	// All puppets that have the player
	// Update stunt effect on puppets
	AutoAIObjectIter it(GetAISystem()->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIOBJECT_PUPPET));
	for (; it->GetObject(); it->Next())
	{
		CPuppet* pPuppet = it->GetObject()->CastToCPuppet();
		if (!pPuppet) continue;
		if (!pPuppet->IsEnabled()) continue;
		if (!pPuppet->GetAttentionTarget()) continue;
		if (pPuppet->GetAttentionTarget()->GetEntityID() == GetEntityID())
		{
/*			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			pData->fValue = Distance::Point_Point(m_stuntTargets[i].pPuppet->GetPos(), m_stuntTargets[i].threatPos);
			pData->point = m_stuntTargets[i].threatPos;*/
			pPuppet->SetSignal(1, "OnTargetCloaked", 0, 0);
		}
	}
}

void CAIPlayer::Event(unsigned short eType, SAIEVENT *pEvent)
{
	switch (eType)
	{
	case AIEVENT_AGENTDIED:
		// make sure everybody knows I have died
		GetAISystem()->NotifyTargetDead(this);
		m_bEnabled = false;
		GetAISystem()->RemoveFromGroup(GetGroupId(), this);

		GetAISystem()->ReleaseFormationPoint(this);
		ReleaseFormation();

		m_State.vSignals.clear();

		if(GetProxy())
			GetProxy()->Reset(AIOBJRESET_SHUTDOWN);
		break;
	case AIEVENT_PLAYER_STUNT_SPRINT:
		m_playerStuntSprinting = PLAYER_ACTION_SPRINT_RESET_TIME;
		m_playerStuntJumping = -1.0f;
		break;
	case AIEVENT_PLAYER_STUNT_JUMP:
		m_playerStuntJumping = PLAYER_ACTION_JUMP_RESET_TIME;
		m_playerStuntSprinting = -1.0f;
		break;
	case AIEVENT_PLAYER_STUNT_PUNCH:
		if (pEvent)
			AddThrownEntity(pEvent->targetId);
		break;
	case AIEVENT_PLAYER_STUNT_THROW:
		if (pEvent)
			AddThrownEntity(pEvent->targetId);
		break;
	case AIEVENT_PLAYER_STUNT_THROW_NPC:
		if (pEvent)
			AddThrownEntity(pEvent->targetId);
		break;
	case AIEVENT_PLAYER_THROW:
		if (pEvent)
			AddThrownEntity(pEvent->targetId);
		break;
	case AIEVENT_PLAYER_STUNT_CLOAK:
		m_playerStuntCloaking = PLAYER_ACTION_CLOAK_RESET_TIME;
		HandleCloaking();
		break;
	case AIEVENT_PLAYER_STUNT_UNCLOAK:
		m_playerStuntUncloaking = PLAYER_ACTION_CLOAK_RESET_TIME;
		break;
	case AIEVENT_LOWHEALTH:
		m_mercyTimer = GetAISystem()->m_cvRODLowHealthMercyTime->GetFVal();
		break;

	default:
		CAIObject::Event(eType, pEvent);
		break;
	}
}

//
//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------
DynamixTracker::DynamixTracker():
m_AvrgVelocity(0.f,0.f,0.f),
m_LastUpdateTime(0.f)
{

}

//---------------------------------------------------------------------------------
void DynamixTracker::SetOwner(CAIObject* pOwner)
{
	m_pOwner = pOwner;
}

//
//---------------------------------------------------------------------------------
void DynamixTracker::Update()
{
	if (!m_pOwner)
		return;
	IPhysicalEntity*	pPhysEntity(m_pOwner->GetPhysics());

	if(!pPhysEntity)
		return;
	
	CTimeValue			deltaTime(GetAISystem()->GetFrameStartTime() - m_LastUpdateTime);
	if(deltaTime.GetSeconds()<.1f)
		return;

	m_LastUpdateTime  = GetAISystem()->GetFrameStartTime();
	pe_status_dynamics  dSt;
	pPhysEntity->GetStatus(&dSt);
	if(m_Positions.size()>=m_BufferSize)
		m_Positions.pop_front();

	m_Positions.push_back(dSt.v);

	m_AvrgVelocity.Set(0.f,0.f,0.f);
	for(t_PositionsList::const_iterator iPos(m_Positions.begin()); iPos!=m_Positions.end(); ++iPos)
		m_AvrgVelocity += (*iPos);
	m_AvrgVelocity /= static_cast<float>(m_Positions.size());
}

//
//---------------------------------------------------------------------------------
bool DynamixTracker::IsMoving() const
{
	return m_AvrgVelocity.GetLengthSquared() > square(1.5f);
}

//
//---------------------------------------------------------------------------------
DamagePartVector* CAIPlayer::GetDamageParts()
{
	UpdateDamageParts(m_damageParts);
	return &m_damageParts;
}

//
//----------------------------------------------------------------------------------------------
void	CAIPlayer::RecordSnapshot()
{
	// Currently not used
}

//
//----------------------------------------------------------------------------------------------
void	CAIPlayer::RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData)
{
	if(m_pMyRecord!=NULL)
	{
		m_pMyRecord->RecordEvent(event, pEventData);
	}
}

//
//----------------------------------------------------------------------------------------------
IAIDebugRecord*	CAIPlayer::GetAIDebugRecord()
{
	if(m_pMyRecord==NULL) 
		m_pMyRecord = s_pRecorder->AddUnit(GetEntityID());
	return m_pMyRecord;
}

//
//---------------------------------------------------------------------------------
void CAIPlayer::DebugDraw(IRenderer *pRenderer)
{
	// Draw items associated with player actions.
	for (unsigned i = 0, ni = m_lastThrownItems.size(); i < ni; ++i)
	{
//		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(m_lastThrownItems[i].id);
//		if (pEnt)
		{
/*			AABB bounds;
			pEnt->GetWorldBounds(bounds);
			pRenderer->GetIRenderAuxGeom()->DrawAABB(bounds, false, ColorB(255,0,0), eBBD_Faceted);
			bounds.Move(m_lastThrownItems[i].vel);
			pRenderer->GetIRenderAuxGeom()->DrawAABB(bounds, false, ColorB(255,0,0,128), eBBD_Faceted);*/

			AABB bounds(AABB::RESET);
			bounds.Add(m_lastThrownItems[i].pos, m_lastThrownItems[i].r);
			pRenderer->GetIRenderAuxGeom()->DrawAABB(bounds, false, ColorB(255,0,0), eBBD_Faceted);
			bounds.Move(m_lastThrownItems[i].vel);
			pRenderer->GetIRenderAuxGeom()->DrawLine(m_lastThrownItems[i].pos, ColorB(255,0,0), m_lastThrownItems[i].pos+m_lastThrownItems[i].vel, ColorB(255,0,0,128));
			pRenderer->GetIRenderAuxGeom()->DrawAABB(bounds, false, ColorB(255,0,0,128), eBBD_Faceted);

//			Vec3 dir = m_lastThrownItems[i].vel;
//			float speed = dir.NormalizeSafe(Vec3(1,0,0));

/*			float speed = m_lastThrownItems[i].vel.GetLength();
			if (speed > 0.0f)
			{
			}
			else
			{
			}*/
		}
	}

	for (unsigned i = 0, ni = m_stuntTargets.size(); i < ni; ++i)
	{
		SAIBodyInfo	bodyInfo;
		m_stuntTargets[i].pPuppet->GetProxy()->QueryBodyInfo(bodyInfo);
		Vec3	pos = m_stuntTargets[i].pPuppet->GetPhysicsPos();
		AABB	aabb(bodyInfo.stanceSize);
		aabb.Move(pos);
		pRenderer->GetIRenderAuxGeom()->DrawAABB(aabb, true, ColorB(255,255,255,m_stuntTargets[i].signalled ? 128 : 48), eBBD_Faceted);
	}

	float col[4] = { 1,1,1,1 };

	// Draw special player actions
	if (m_playerStuntSprinting > 0.0f)
	{
		Vec3 pos = GetPos();
		Vec3 vel = GetVelocity();
		SAIBodyInfo bi;
		GetProxy()->QueryBodyInfo(bi);
		float r = bi.stanceSize.GetRadius();
		AABB bounds(AABB::RESET);
		bounds.Add(pos, r);
		pRenderer->GetIRenderAuxGeom()->DrawAABB(bounds, false, ColorB(255,0,0), eBBD_Faceted);
		bounds.Move(vel);
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,0,0), pos+vel, ColorB(255,0,0,128));
		pRenderer->GetIRenderAuxGeom()->DrawAABB(bounds, false, ColorB(255,0,0,128), eBBD_Faceted);

		pRenderer->Draw2dLabel(10, 10, 2.5f, col, true, "SPRINTING");
	}
	if (m_playerStuntJumping > 0.0f)
	{
		Vec3 pos = GetPos();
		Vec3 vel = GetVelocity();
		SAIBodyInfo bi;
		GetProxy()->QueryBodyInfo(bi);
		float r = bi.stanceSize.GetRadius();
		AABB bounds(AABB::RESET);
		bounds.Add(pos, r);
		pRenderer->GetIRenderAuxGeom()->DrawAABB(bounds, false, ColorB(255,0,0), eBBD_Faceted);
		bounds.Move(vel);
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,0,0), pos+vel, ColorB(255,0,0,128));
		pRenderer->GetIRenderAuxGeom()->DrawAABB(bounds, false, ColorB(255,0,0,128), eBBD_Faceted);

		pRenderer->Draw2dLabel(10, 40, 2.5f, col, true, "JUMPING");
	}
	if (m_playerStuntCloaking > 0.0f)
	{
		pRenderer->Draw2dLabel(10, 70, 2.5f, col, true, "CLOAKING");
	}
	if (m_playerStuntUncloaking > 0.0f)
	{
		pRenderer->Draw2dLabel(10, 110, 2.5f, col, true, "UNCLOAKING");
	}
	if (!m_lastThrownItems.empty())
	{
		pRenderer->Draw2dLabel(10, 150, 2.5f, col, true, "THROWING");
	}

	if (IsLowHealthPauseActive())
		pRenderer->Draw2dLabel(10, 190, 2.0f, col, true, "Mercy t=%.2fs/%.2f", m_mercyTimer, GetAISystem()->m_cvRODLowHealthMercyTime->GetFVal());

	for (unsigned i = 0, ni = m_exposedCoverObjects.size(); i < ni; ++i)
	{
		pe_status_pos statusPos;
		m_exposedCoverObjects[i].pPhysEnt->GetStatus(&statusPos);
		AABB bounds(AABB::RESET);
		bounds.Add(statusPos.BBox[0] + statusPos.pos);
		bounds.Add(statusPos.BBox[1] + statusPos.pos);
		pRenderer->GetIRenderAuxGeom()->DrawAABB(bounds, false, ColorB(255,0,0), eBBD_Faceted);
		pRenderer->DrawLabel(bounds.GetCenter(), 1.1f, "IGNORED %.1fs", m_exposedCoverObjects[i].t);
	}
}

//
//---------------------------------------------------------------------------------
bool CAIPlayer::IsLowHealthPauseActive() const
{
	if (m_mercyTimer > 0.0f)
		return true;
	return false;
}

//
//------------------------------------------------------------------------------------------------------------------------
IEntity* CAIPlayer::GetGrabbedEntity() const
{
	if (!GetProxy())
		return NULL;
	return GetProxy()->GetGrabbedEntity();
}



//
//---------------------------------------------------------------------------------
void CAIPlayer::Serialize(TSerialize ser, class CObjectTracker& objectTracker )
{
	ser.BeginGroup("AIPlayer");

	CAIActor::Serialize(ser,objectTracker);

	objectTracker.SerializeObjectPointer(ser,"m_pAttentionTarget",m_pAttentionTarget,false);

	ser.Value("m_fLastUpdateTargetTime",m_fLastUpdateTargetTime);
	ser.Value("m_FOV",m_FOV);
	m_Tracker.Serialize(ser,objectTracker);

	ser.Value("m_playerStuntSprinting", m_playerStuntSprinting);
	ser.Value("m_playerStuntJumping", m_playerStuntJumping);
	ser.Value("m_playerStuntCloaking", m_playerStuntCloaking);
	ser.Value("m_playerStuntUncloaking", m_playerStuntUncloaking);
	ser.Value("m_stuntDir", m_stuntDir);
	ser.ValueWithDefault("m_mercyTimer", m_mercyTimer, -1.0f);

	ser.EndGroup();

}
