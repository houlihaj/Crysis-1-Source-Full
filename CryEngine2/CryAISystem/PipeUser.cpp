#include "StdAfx.h"
#include "CAISystem.h"
#include <ISystem.h>
#include <IConsole.h>
#include <ITimer.h>
#include "GoalOp.h"
#include "PipeUser.h"
#include "Puppet.h"
#include "Leader.h"
#include "ObjectTracker.h"
#include "AICollision.h"
#include "AIActions.h"
#include "PathFollower.h"
#include <ISerialize.h>



CPipeUser::CPipeUser(CAISystem* pAISystem)
: CAIActor(pAISystem)
, m_vDEBUG_VECTOR_movement(ZERO)
, m_fTimePassed(0)
, m_bHiding(false)
, m_bStartTiming(false)
, m_fEngageTime(0)
, m_pAttentionTarget(NULL)
, m_pPrevAttentionTarget(NULL)
, m_pPathFollower(0)
, m_pLastOpResult(NULL)
, m_pReservedNavPoint(NULL)
, m_nMovementPurpose( 0 )
, m_vLastMoveDir(ZERO)
, m_bKeepMoving(false)
, m_bFirstUpdate(true)
, m_bBlocked(false)
, m_pCurrentGoalPipe(NULL)
, m_fireMode(FIREMODE_OFF)
, m_pFireTarget(0)
, m_fireModeUpdated(false)
, m_bLooseAttention( false )
, m_pLooseAttentionTarget( 0 )
, m_pLookAtTarget(0)
, m_vDEBUG_VECTOR(ZERO)
, m_IsSteering(false)
, m_DEBUGmovementReason(AIMORE_UNKNOWN)
, m_AttTargetPersistenceTimeout(0.0f)
, m_AttTargetThreat(AITHREAT_NONE)
, m_AttTargetExposureThreat(AITHREAT_NONE)
, m_AttTargetType(AITARGET_NONE)
, m_bPathToFollowIsSpline( false )
, m_lastLiveTargetPos(ZERO)
, m_timeSinceLastLiveTarget(-1.0f)
, m_refShape(0)
, m_looseAttentionId(0)
, m_aimState(AI_AIM_NONE)
, m_pActorTargetRequest(0)
, m_DEBUGUseTargetPointRequest(ZERO)
, m_pPathFindTarget(0)
, m_PathDestinationPos(ZERO)
, m_navSOEarlyPathRegen(false)
, m_actorTargetReqId(1)
, m_spreadFireTime(0.0f)
, m_lastExecutedGoalop(LAST_AIOP)
{
	_fastcast_CPipeUser = true;

	//Ref point creation and registration with AI
	if(GetAISystem())
	{
		string sNameRefPoint ="_RefPoint";
		m_pRefPoint = GetAISystem()->CreateDummyObject(sNameRefPoint);
		if(m_pRefPoint)
		{
//			AIObjectParameters params;
//			m_pRefPoint->ParseParameters(params);
			m_pRefPoint->SetPos(GetPos(), GetMoveDir());
			m_pRefPoint->SetSubType(STP_REFPOINT);
		}

		m_pLookAtTarget = GetAISystem()->CreateDummyObject(m_sName + "_LookAtTarget");
		AIAssert(m_pLookAtTarget);
		m_pLookAtTarget->SetSubType(STP_LOOKAT);
		m_pLookAtTarget->SetPos(GetPos()+GetMoveDir());
	}
	else
		m_pRefPoint  = NULL;

	m_CurrentHideObject.m_HideSmartObject.pChainedUserEvent = NULL;
	m_CurrentHideObject.m_HideSmartObject.pChainedObjectEvent = NULL;

	// Init the array of special AI object pointers.
	for(unsigned i = 0; i < COUNT_AISPECIAL; ++i)
		m_pSpecialObjects[i] = 0;

	Reset(AIOBJRESET_INIT);

//	SelectPipe(1,"",NULL);
}

static bool bNotifyListenersLock = false;

CPipeUser::~CPipeUser(void)
{
	// Don't call NotifyListeners. m_pCurrentPipe has already been set to NULL!
	//NotifyListeners(m_pCurrentGoalPipe, ePN_OwnerRemoved, true);
/*
	bNotifyListenersLock = true;
	TMapGoalPipeListeners::iterator it, itEnd = m_mapGoalPipeListeners.end();
	for ( it = m_mapGoalPipeListeners.begin(); it != itEnd; ++it )
	{
		std::pair< IGoalPipeListener*, const char* > & second = it->second;
		second.first->OnGoalPipeEvent( this, ePN_OwnerRemoved, it->first );
	}
	bNotifyListenersLock = false;
*/

	while ( !m_mapGoalPipeListeners.empty() )
		UnRegisterGoalPipeListener( m_mapGoalPipeListeners.begin()->second.first, m_mapGoalPipeListeners.begin()->first );

	/*
	CLeader* pLeader = GetAISystem()->GetLeader(this);
	if(pLeader)
		pLeader->RemoveTeamMember(this);
	*/
	if (m_pRefPoint)
	{
		GetAISystem()->RemoveDummyObject(m_pRefPoint);
		m_pRefPoint = 0;
	}
	if (m_pLookAtTarget)
	{
		GetAISystem()->RemoveDummyObject(m_pLookAtTarget);
		m_pLookAtTarget = 0;
	}
	//ResetLookAt();

	// Delete special AI objects, and reset array.
	for(unsigned i = 0; i < COUNT_AISPECIAL; ++i)
	{
		if(m_pSpecialObjects[i])
			GetAISystem()->RemoveDummyObject(m_pSpecialObjects[i]);
		m_pSpecialObjects[i] = 0;
	}
  SAFE_DELETE(m_pPathFollower);
	SAFE_DELETE(m_pActorTargetRequest);
}


//
//
void CPipeUser::Reset(EObjectResetType type)
{
	CAIActor::Reset(type);
	SetAttentionTarget(0);
	m_PathDestinationPos.zero();
	m_pPathFindTarget = 0;
	SetLastOpResult(0);
	m_IsSteering = false;
	m_fireMode = FIREMODE_OFF;
	m_pFireTarget = 0;
	m_fireModeUpdated = false;
	m_outOfAmmoSent = false;
	m_actorTargetReqId = 1;
	ResetLookAt();

	m_aimState = AI_AIM_NONE;

	m_posLookAtSmartObject.zero();
	
	m_eNavSOMethod = nSOmNone;
	m_navSOEarlyPathRegen = false;
	m_pendingNavSOStates.Clear();
	m_currentNavSOStates.Clear();

	m_CurrentNodeNavType = IAISystem::NAV_UNSET;
	m_idLastUsedSmartObject = 0;
	ClearInvalidatedSOLinks();

/*	m_sCurrentAnimationDoneUserStates.clear();
	m_sCurrentAnimationDoneObjectStates.clear();
	m_sCurrentAnimationFailUserStates.clear();
	m_sCurrentAnimationFailObjectStates.clear();*/

	m_CurrentHideObject.m_HideSmartObject.Clear();
	m_forcePathGenerationFrom.zero();
	m_bFirstUpdate = true;

	m_lastLiveTargetPos.Set(0,0,0);
	m_timeSinceLastLiveTarget = -1.0f;
	m_spreadFireTime = 0.0f;

	m_recentUnreachableHideObjects.clear();

	m_bPathToFollowIsSpline = false;

	m_refShapeName.clear();
	m_refShape = 0;

	SAFE_DELETE(m_pActorTargetRequest);

	// Delete special AI objects, and reset array.
	for(unsigned i = 0; i < COUNT_AISPECIAL; ++i)
	{
		if(m_pSpecialObjects[i])
			GetAISystem()->RemoveDummyObject(m_pSpecialObjects[i]);
		m_pSpecialObjects[i] = 0;
	}

	m_notAllowedSubpipes.clear();
	SelectPipe( 0, "_first_", NULL, 0, true );
	m_Path.Clear("CPipeUser::Reset m_Path");
	m_DEBUGCanTargetPointBeReached.clear();
	m_DEBUGUseTargetPointRequest.zero();
}

void CPipeUser::SetName(const char *pName)
{
	CAIObject::SetName(pName);
	char name[256];
	if(m_pRefPoint)
	{
		sprintf(name,"%s_RefPoint",pName);
		m_pRefPoint->SetName(name);					
	}
	if(m_pLookAtTarget)
	{
		sprintf(name,"%s_LookAt",pName);
		m_pLookAtTarget->SetName(name);					
	}
}

//
//---------------------------------------------------------------------------------------------------------
void CPipeUser::SetEntityID(unsigned ID)
{
	unsigned oldID = CAIObject::GetEntityID();
	CAIObject::SetEntityID(ID);
	
	if (s_pRecorder)
	{
		if(m_pMyRecord && oldID && ID != oldID) 
			s_pRecorder->RemoveUnit(oldID);

		m_pMyRecord = s_pRecorder->AddUnit(ID);
	}
}

//
//---------------------------------------------------------------------------------------------------------
bool CPipeUser::ProcessBranchGoal(QGoal &Goal,bool &bSkipAdd)
{
	if (Goal.op != AIOP_BRANCH)
		return false;

	bSkipAdd = Goal.bBlocking;
	
	bool bNot = (Goal.params.nValue & NOT) != 0;

	if (GetBranchCondition(Goal) ^ bNot)
	{
		// string labels are already converted to integer relative offsets
		// TODO: cut the string version of Jump in a few weeks
		if (Goal.params.szString.empty())
			bSkipAdd |= m_pCurrentGoalPipe->Jump(Goal.params.nValueAux);
		else
			bSkipAdd |= m_pCurrentGoalPipe->Jump(Goal.params.szString);
	}

	return true;
}

//
//---------------------------------------------------------------------------------------------------------
bool CPipeUser::GetBranchCondition(QGoal &Goal)
{
	int branchType = Goal.params.nValue & (~NOT);
	
	switch(branchType)
	{
	case IF_RANDOM:
		{
			if (ai_frand() < Goal.params.fValue)
				return true;
		}
		break;
	case IF_NO_PATH:
		{
			if (m_nPathDecision == PATHFINDER_NOPATH)
				return true;
		}
		break;
	case IF_PATH_STILL_FINDING:
		{
			if (m_nPathDecision == PATHFINDER_STILLFINDING)
				return true;
		}
		break;
	case IF_IS_HIDDEN:	// branch if already at hide spot 
		{
			if (!m_CurrentHideObject.IsValid())
				return false;
			Vec3 diff(m_CurrentHideObject.GetLastHidePos() - GetPos());
			diff.z=0.f;
			if (diff.len2() < Goal.params.fValue*Goal.params.fValue)
				return true;
		}
		break;
	case IF_CAN_HIDE:	// branch if hide spot was found
		{
			if (m_CurrentHideObject.IsValid())
				return true;
		}
		break;
	case IF_CANNOT_HIDE:
		{
			if (!m_CurrentHideObject.IsValid())
				return true;
		}
		break;
	case IF_STANCE_IS:	// branch if stance is equal to params.fValue
		{
			if (m_State.bodystate == Goal.params.fValue)
				return true;
		}
		break;
	case IF_HAS_FIRED:	// jumps if the PipeUser just fired - fire flag passed to actor
		{
			if (m_State.fire)
				return true;
		}
		break;
	case IF_FIRE_IS:	// branch if last "firecmd" argument was equal to params.fValue
		{
			bool state = Goal.params.fValue > 0.5f;
			if (AllowedToFire() == state)
				return true;
		}
		break;
	case IF_NO_LASTOP:
		{
			if (!m_pLastOpResult)
				return true;
		}
		break;
	case IF_SEES_LASTOP:
		{
			if (m_pLastOpResult && GetAISystem()->CheckObjectsVisibility(this, m_pLastOpResult, Goal.params.fValue))
				return true;
		}
		break;
	case IF_SEES_TARGET:
		{
			if (Goal.params.fValueAux >= 0.0f)
			{
				if (m_pAttentionTarget)
				{
					SAIBodyInfo bi;
					if (GetProxy() && GetProxy()->QueryBodyInfo((EStance)(int)Goal.params.fValueAux, 0.0f, false, bi))
					{
						const Vec3& pos = bi.vEyePos;
						ray_hit hit;
						Vec3 dir = m_pAttentionTarget->GetPos() - pos;
						if (dir.GetLengthSquared() > sqr(Goal.params.fValue))
							dir.SetLength(Goal.params.fValue);
						if (!gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1))
							return true;
					}
				}
			}
			else
			{
				if (m_pAttentionTarget && GetAISystem()->CheckObjectsVisibility(this, m_pAttentionTarget, Goal.params.fValue))
					return true;
			}
		}
		break;

	case IF_TARGET_LOST_TIME_MORE:
		{
			CPuppet* pPuppet = CastToCPuppet();
			if (pPuppet && pPuppet->m_targetLostTime > Goal.params.fValue)
				return true;
		}
		break;

	case IF_TARGET_LOST_TIME_LESS:
		{
			CPuppet* pPuppet = CastToCPuppet();
			if (pPuppet && pPuppet->m_targetLostTime <= Goal.params.fValue)
				return true;
		}
		break;

	case IF_EXPOSED_TO_TARGET:
		{
			if (m_pAttentionTarget)
			{
				Vec3	pos = GetPos();
				SAIBodyInfo bi;
				if (GetProxy() && GetProxy()->QueryBodyInfo(STANCE_CROUCH, 0.0f, false, bi))
					pos = bi.vEyePos;

				Vec3	dir(m_pAttentionTarget->GetPos() - pos);
				dir.SetLength(Goal.params.fValue);
				float	dist;
				bool exposed = !IntersectSweptSphere(0, dist, Lineseg(pos, pos + dir), Goal.params.fValueAux, AICE_ALL);
				if (exposed)
					return true;
			}
		}
		break;
	case IF_CAN_SHOOT_TARGET_CROUCHED:
		{
			if (m_pAttentionTarget)
			{
				const Vec3&	targetPos = m_pAttentionTarget->GetPos();
				Vec3	dir, hitPos, p;
				ray_hit hit;

				Vec3	crouchPos = GetPhysicsPos();
				Vec3	standPos = crouchPos;
				crouchPos.z += 0.6f;
				standPos.z += 1.2f;

				float	crouch = 0.0f;
				float	stand = 0.0f;

				dir = targetPos - GetPos();
				Vec3	up(0,0,1);
				Vec3	right(dir.y, -dir.x, 0);
				right.NormalizeSafe();

				const unsigned iters = 4;

				for (unsigned i = 0; i < 4; i++)
				{
					float	angle = ai_frand() * gf_PI2;
					float	r = ai_frand() * 0.3f;
					float	x = sinf(angle) * r;
					float	y = cosf(angle) * r;
					Vec3	offset = right * x + up * y;
					p = crouchPos + offset;
					dir = targetPos - p;
					if (gEnv->pPhysicalWorld->RayWorldIntersection(p, dir, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1))
						crouch += 1.0f / (float)iters;
				}
				bool	canCrouch = crouch < 0.55f;
				if (canCrouch)
					return true;
			}
		}
		break;
	case IF_CAN_SHOOT_TARGET:
		{
			Vec3 pos, dir;
			CPuppet* pPuppet = CastToCPuppet();
			bool aimOK = GetAimState()!=AI_AIM_OBSTRUCTED;
			if (m_pAttentionTarget && pPuppet && pPuppet->GetFirecommandHandler() && 
					aimOK &&
					pPuppet->CanAimWithoutObstruction(m_pAttentionTarget->GetPos()) &&
					pPuppet->GetFirecommandHandler()->ValidateFireDirection(m_pAttentionTarget->GetPos() - GetFirePos(), false))
//				CheckAndGetFireTarget_Deprecated(m_pAttentionTarget, true, pos, dir))
				return true;
		}
		break;
	case IF_CAN_MELEE:
		{
			SAIWeaponInfo weaponInfo;
			GetProxy()->QueryWeaponInfo(weaponInfo);
			if (weaponInfo.canMelee)
				return true;
		}
		break;
	case IF_LASTOP_DIST_LESS:
		{
			if (m_pLastOpResult && Distance::Point_Point(GetPos(), m_pLastOpResult->GetPos()) <= Goal.params.fValue)
				return true;
		}
		break;
	case IF_LASTOP_DIST_LESS_ALONG_PATH:
		{
			if(m_pLastOpResult && m_nPathDecision==PATHFINDER_PATHFOUND)
			{
				CPuppet* pPuppet = CastToCPuppet();
				if(pPuppet)
				{
					float dist = m_movementAbility.b3DMove ?  Distance::Point_Point(GetPos(), m_pLastOpResult->GetPos()):
						Distance::Point_Point2D(GetPos(), m_pLastOpResult->GetPos());
					float distPath = pPuppet->GetDistanceAlongPath(m_pLastOpResult->GetPos(),true);
					if(dist < distPath)
						dist = distPath;
					if (dist <= Goal.params.fValue)
						return true;
				}
			}	
		}
		break;
	case IF_TARGET_DIST_LESS:
		{
			if (m_pAttentionTarget && Distance::Point_Point(GetPos(), m_pAttentionTarget->GetPos()) <= Goal.params.fValue)
				return true;
		}
		break;
	case IF_TARGET_DIST_LESS_ALONG_PATH:
		{
			if(m_pAttentionTarget && m_nPathDecision==PATHFINDER_PATHFOUND)
			{
				CPuppet* pPuppet = CastToCPuppet();
				if(pPuppet)
				{
					float dist = m_movementAbility.b3DMove ?  Distance::Point_Point(GetPos(), m_pAttentionTarget->GetPos()):
						Distance::Point_Point2D(GetPos(), m_pAttentionTarget->GetPos());
					float distPath = pPuppet->GetDistanceAlongPath(m_pAttentionTarget->GetPos(),true);
					if(dist < distPath)
						dist = distPath;
					if (dist <= Goal.params.fValue)
						return true;
				}
			}	
		}
		break;
	case IF_TARGET_DIST_GREATER:
		{
			if (m_pAttentionTarget && Distance::Point_Point(GetPos(), m_pAttentionTarget->GetPos()) > Goal.params.fValue)
				return true;
		}
		break;
	case IF_TARGET_IN_RANGE:
		{
			if (m_pAttentionTarget && Distance::Point_Point(GetPos(), m_pAttentionTarget->GetPos()) <= m_Parameters.m_fAttackRange)
				return true;
		}
		break;
	case IF_TARGET_OUT_OF_RANGE:
		{
			if (m_pAttentionTarget && Distance::Point_Point(GetPos(), m_pAttentionTarget->GetPos()) > m_Parameters.m_fAttackRange)
				return true;
		}
		break;
	case IF_TARGET_TO_REFPOINT_DIST_LESS:
		{
			if (m_pAttentionTarget && Distance::Point_Point(GetRefPoint()->GetPos(), m_pAttentionTarget->GetPos()) <= Goal.params.fValue)
				return true;
		}
		break;
	case IF_TARGET_TO_REFPOINT_DIST_GREATER:
		{
			if (m_pAttentionTarget && Distance::Point_Point(GetRefPoint()->GetPos(), m_pAttentionTarget->GetPos()) > Goal.params.fValue)
				return true;
		}
		break;
	case IF_TARGET_MOVED_SINCE_START:
	case IF_TARGET_MOVED:
		{
			CGoalPipe *pLastPipe(m_pCurrentGoalPipe->GetLastSubpipe());
			if (!pLastPipe)	// this should NEVER happen
			{
				AIError("CPipeUser::ProcessBranchGoal can get pipe. User: %s",GetName());
				return true;
			}
			if (m_pAttentionTarget)
			{
				bool ret = Distance::Point_Point(pLastPipe->m_vAttTargetPosAtStart, m_pAttentionTarget->GetPos()) > Goal.params.fValue;
				if(branchType == IF_TARGET_MOVED)
					pLastPipe->m_vAttTargetPosAtStart = m_pAttentionTarget->GetPos();
				return ret;
			}
		}
		break;
	case IF_NO_ENEMY_TARGET:
		{
			if (!m_pAttentionTarget || !IsHostile(m_pAttentionTarget))
				return true;
		}
		break;
	case IF_PATH_LONGER:	// branch if current path is longer than params.fValue
		{
			float dbgPathLength(m_Path.GetPathLength(false));
			if (dbgPathLength > Goal.params.fValue)
				return true;
		}
		break;
	case IF_PATH_SHORTER:
		{
			float dbgPathLength(m_Path.GetPathLength(false));
			if (dbgPathLength <= Goal.params.fValue)
				return true;
		}
		break;
	case IF_PATH_LONGER_RELATIVE:	// branch if current path is longer than (params.fValue) times the distance to destination
		{
			Vec3 pathDest(m_Path.GetParams().end);
			float pathLength  = m_Path.GetPathLength(false);
			float dist = Distance::Point_Point(GetPos(), pathDest);
			if (pathLength >= dist * Goal.params.fValue)
				return true;
		}
		break;
	case  IF_NAV_WAYPOINT_HUMAN:	// branch if current navigation graph is waypoint
		{
			int nBuilding;
			IVisArea *pArea;
			IAISystem::ENavigationType navType = GetAISystem()->CheckNavigationType(GetPos(), nBuilding, pArea, m_movementAbility.pathfindingProperties.navCapMask );
			if (navType == IAISystem::NAV_WAYPOINT_HUMAN)
				return true;
		}
		break;
	case  IF_NAV_TRIANGULAR:	// branch if current navigation graph is triangular
		{
			int nBuilding;
			IVisArea *pArea;
			IAISystem::ENavigationType navType = GetAISystem()->CheckNavigationType(GetPos(), nBuilding, pArea, m_movementAbility.pathfindingProperties.navCapMask );
			if (navType == IAISystem::NAV_TRIANGULAR)
				return true;
		}
	case IF_COVER_COMPROMISED:
	case IF_COVER_NOT_COMPROMISED:	// jumps if the current cover cannot be used for hiding or if the hide spots does not exists.
		{
			bool compromised = false;
			if (m_pAttentionTarget)
				compromised = m_CurrentHideObject.IsCompromised(this, m_pAttentionTarget->GetPos());
			else
				compromised = true;
			if (Goal.params.nValue == IF_COVER_COMPROMISED)
			{
				if (compromised)
					return true;
			}
			else
			{
				if (!compromised)
					return true;
			}
		}
		break;
	case IF_COVER_FIRE_ENABLED:	// branch if cover firemode is  enabled
		{
			CPuppet* pPuppet = CastToCPuppet();
			if (pPuppet)
				return !pPuppet->IsCoverFireEnabled();
		}
		break;
	case IF_COVER_SOFT:
		{
			bool isEmptyCover = m_CurrentHideObject.IsCoverPathComplete() && m_CurrentHideObject.GetCoverWidth(true) < 0.1f;
			bool soft = !m_CurrentHideObject.IsObjectCollidable() || isEmptyCover;
			if (soft)
				return true;
		}
		break;
	case IF_COVER_NOT_SOFT:
		{
			bool isEmptyCover = m_CurrentHideObject.IsCoverPathComplete() && m_CurrentHideObject.GetCoverWidth(true) < 0.1f;
			bool soft = !m_CurrentHideObject.IsObjectCollidable() || isEmptyCover;
			if (soft)
				return true;
		}
		break;

	case IF_LASTOP_FAILED:
		{
			if (m_pCurrentGoalPipe && m_pCurrentGoalPipe->GetLastResult() == AIGOR_FAILED)
				return true;
		}
		break;

	case IF_LASTOP_SUCCEED:
		{
			if (m_pCurrentGoalPipe && m_pCurrentGoalPipe->GetLastResult() == AIGOR_SUCCEED)
				return true;
		}
		break;

	case BRANCH_ALWAYS:	// branches always
		return true;
		break;

	case IF_ACTIVE_GOALS_HIDE:
		if (!m_vActiveGoals.empty() || !m_CurrentHideObject.IsValid())
			return true;
		break;

	default://IF_ACTIVE_GOALS
		if (!m_vActiveGoals.empty())
			return true;
		break;
	}

	return false;
}
//
//---------------------------------------------------------------------------------------------------------
bool CPipeUser::ProcessRandomGoal(QGoal &Goal,bool &bSkipAdd)
{
	if (Goal.op != AIOP_RANDOM)
		return false;
	bSkipAdd = true;
	if (Goal.params.fValue > ai_rand() % 100)	// 0 - never jump // 100 - always jump
		m_pCurrentGoalPipe->Jump(Goal.params.nValue);
	return true;
}

//
//---------------------------------------------------------------------------------------------------------
bool CPipeUser::ProcessClearGoal(QGoal &Goal,bool &bSkipAdd)
{
	if (Goal.op != AIOP_CLEAR)
		return false;
	ClearActiveGoals();

	//Luciano - CLeader should manage the release of formation 
	//					GetAISystem()->FreeFormationPoint(m_Parameters.m_nGroup,this);
	if (Goal.params.fValue)
	{
		SetAttentionTarget(0);
		ClearPotentialTargets();
	}
	else
		bSkipAdd = true;
	return true;
}

void CPipeUser::SetNavSOFailureStates()
{
	if ( m_eNavSOMethod != nSOmNone )
	{
		if ( !m_currentNavSOStates.IsEmpty() )
		{
			GetAISystem()->ModifySmartObjectStates( GetEntity(), m_currentNavSOStates.sAnimationFailUserStates );
			IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity( m_currentNavSOStates.objectEntId );
			if ( pObjectEntity )
				GetAISystem()->ModifySmartObjectStates( pObjectEntity, m_currentNavSOStates.sAnimationFailObjectStates );
			m_currentNavSOStates.Clear();
		}

		if ( !m_pendingNavSOStates.IsEmpty() )
		{
			GetAISystem()->ModifySmartObjectStates( GetEntity(), m_pendingNavSOStates.sAnimationFailUserStates );
			IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity( m_pendingNavSOStates.objectEntId );
			if ( pObjectEntity )
				GetAISystem()->ModifySmartObjectStates( pObjectEntity, m_pendingNavSOStates.sAnimationFailObjectStates );
			m_pendingNavSOStates.Clear();
		}
	}

	AIAssert(m_pendingNavSOStates.IsEmpty() && m_currentNavSOStates.IsEmpty());

	// Stop exact positioning.
	m_State.actorTargetReq.Reset();

	m_eNavSOMethod = nSOmNone;
	m_navSOEarlyPathRegen = false;
	m_forcePathGenerationFrom.zero();
}

void CPipeUser::ClearPath( const char* dbgString )
{
	SetNavSOFailureStates();

  // Don't change the pathfinder result since it has to reflect the result of
  // the last pathfind. If it is set to NoPath then flowgraph AIGoto nodes will
  // fail at the end of the path.
	m_Path.Clear( dbgString );
	m_OrigPath.Clear( dbgString );
	m_pPathFindTarget = 0;
}

//
//--------------------------------------------------------------------------------------------------------
void CPipeUser::GetStateFromActiveGoals(SOBJECTSTATE &state)
{
	// Reset the movement reason at each update, it will be setup by the correct goal operation.
	m_DEBUGmovementReason = AIMORE_UNKNOWN;

	FUNCTION_PROFILER(GetAISystem()->m_pSystem,PROFILE_AI);

	if (m_bFirstUpdate)
	{
		m_bFirstUpdate = false;
		SetLooseAttentionTarget(0);
	}

	bool bSkipAdd = false;
	if (m_pCurrentGoalPipe)
	{

		if (m_pCurrentGoalPipe->CountSubpipes() >= 10)
		{
			AIWarning("%s has too many (%d) subpipes. Pipe <%s>", GetName(), m_pCurrentGoalPipe->CountSubpipes(),
											m_pCurrentGoalPipe->m_sName.c_str());
//			assert(m_pCurrentGoalPipe->CountSubpipes() < 10);
			//		AIAssert(m_vActiveGoals.size() < 100);
		}

		if (!m_bBlocked)	// if goal queue not blocked
		{
			QGoal Goal;

//			while (Goal.pGoalOp = m_pCurrentGoalPipe->PopGoal(Goal.bBlocking,Goal.name, Goal.params,this))
			EPopGoalResult pgResult;
			while ((pgResult = m_pCurrentGoalPipe->PopGoal(Goal,this)) == ePGR_Succeed)
			{
				if(ProcessBranchGoal(Goal, bSkipAdd))
				{
					if (bSkipAdd)
						break; // blocking
					// bSkipAdd = true;
					continue; // non-blocking
				}
				if(ProcessRandomGoal(Goal, bSkipAdd))
					break;
				if(ProcessClearGoal(Goal, bSkipAdd))
					break;
				EGoalOpResult result = Goal.pGoalOp->Execute(this);
				if (result == AIGOR_IN_PROGRESS)
					break;
				else
				{
					if (result != AIGOR_DONE)
						m_pCurrentGoalPipe->SetLastResult(result);
				}
			}

			if (pgResult != ePGR_BreakLoop)
			{
				if (!Goal.pGoalOp)
				{
					if(m_pCurrentGoalPipe->IsLoop())
					{
						m_pCurrentGoalPipe->Reset();
						ClearActiveGoals();
						if(GetAttentionTarget())
							m_pCurrentGoalPipe->m_vAttTargetPosAtStart = GetAttentionTarget()->GetPos();
					}
				}
				else
				{
					if (!bSkipAdd)
					{
						//FIXME
						//todo: remove this, currently happens coz "lookat" never finishes - actor can't rotate sometimes
						if(m_vActiveGoals.size()<110)
							m_vActiveGoals.push_back(Goal);
						m_bBlocked = Goal.bBlocking;
					}
				}
			}
		}
	}
	if (m_vActiveGoals.size() >= 10)
	{
		const QGoal& Goal = m_vActiveGoals.back();
		AIWarning("%s has too many (%d) active goals. Pipe <%s>; last goal <%s>", GetName(), m_vActiveGoals.size(),
			m_pCurrentGoalPipe!=0 ? m_pCurrentGoalPipe->m_sName.c_str() : "_no_pipe_",
			Goal.op == LAST_AIOP ? Goal.pipeName.c_str() : m_pCurrentGoalPipe->GetGoalOpName(Goal.op));
		assert(m_vActiveGoals.size() < 100);
//		AIAssert(m_vActiveGoals.size() < 100);
	}

	if (!m_vActiveGoals.empty())
	{
		//ListOGoals::iterator gi;

		//for (gi=m_lstActiveGoals.begin();gi!=m_lstActiveGoals.end();)
		for (size_t i = 0; i < m_vActiveGoals.size(); i++)
		{
			//QGoal Goal = (*gi);
			QGoal& Goal = m_vActiveGoals[i];

			m_lastExecutedGoalop = Goal.op;

			ITimer *pTimer = GetAISystem()->m_pSystem->GetITimer();
			int val = GetAISystem()->m_cvProfileGoals->GetIVal();

			CTimeValue timeLast;

			if (val)
				timeLast = pTimer->GetAsyncTime();

			EGoalOpResult result = Goal.pGoalOp->Execute(this);
			if (val)
			{
				CTimeValue timeCurr = pTimer->GetAsyncTime();
				float f = (timeCurr-timeLast).GetSeconds();
				timeLast=timeCurr;

				TimingMap::iterator ti;
				const char* goalName = Goal.op == LAST_AIOP ? Goal.pipeName.c_str() : m_pCurrentGoalPipe->GetGoalOpName(Goal.op);
				ti = GetAISystem()->m_mapDEBUGTimingGOALS.find(goalName);
				if (ti == GetAISystem()->m_mapDEBUGTimingGOALS.end())
					GetAISystem()->m_mapDEBUGTimingGOALS.insert(TimingMap::iterator::value_type(goalName,f));
				else
				{
					if (f > ti->second)
						ti->second = f;
				}
			}
			if (result != AIGOR_IN_PROGRESS)
			{
				if (Goal.bBlocking)
				{
					if (result != AIGOR_DONE)
						m_pCurrentGoalPipe->SetLastResult(result);
					m_bBlocked = false;
				}

				RemoveActiveGoal(i);
				if (!m_vActiveGoals.empty())
					--i;
			}
		}
	}
	m_Path.GetPath( m_State.remainingPath );

	if (m_bKeepMoving && m_State.vMoveDir.IsZero(.01f) && !m_vLastMoveDir.IsZero(.01f))
		m_State.vMoveDir = m_vLastMoveDir;
	else if (!m_State.vMoveDir.IsZero(.01f))
		m_vLastMoveDir = m_State.vMoveDir;
}

void CPipeUser::SetLastOpResult(CAIObject * pObject)
{
	m_pLastOpResult = pObject;
}


void CPipeUser::SetAttentionTarget(CAIObject *pTarget)
{
	if (pTarget == 0)
	{
		m_AttTargetThreat = AITHREAT_NONE;
		m_AttTargetExposureThreat = AITHREAT_NONE;
		m_AttTargetType = AITARGET_NONE;
		if (m_pAttentionTarget) // if I had a target previously I want to reevaluate
			m_State.bReevaluate = true;
	}
	else if (m_pAttentionTarget!=pTarget && m_pCurrentGoalPipe)
	{
		m_State.bReevaluate = true;
		AIAssert(m_pCurrentGoalPipe);
		CGoalPipe *pLastPipe(m_pCurrentGoalPipe->GetLastSubpipe());
		if(pLastPipe)
			pLastPipe->m_vAttTargetPosAtStart = pTarget->GetPos();
	}

	if(m_pAttentionTarget!=0 && m_pAttentionTarget->GetType()!=AIOBJECT_DUMMY 
		&& m_pAttentionTarget->GetType()!=200)	//FIXME  not to remember grenades - not good, needs change
		m_pPrevAttentionTarget = m_pAttentionTarget;
	m_pAttentionTarget = pTarget;

	RecorderEventData recorderEventData(pTarget!=NULL?pTarget->GetName():"<none>");
	RecordEvent(IAIRecordable::E_ATTENTIONTARGET, &recorderEventData);

//	GetAISystem()->Record(this, IAIRecordable::E_ATTENTIONTARGET, pTarget!=NULL?pTarget->GetName():NULL);

	// TO DO: move this call to AddToVisibleList when Leader will be able to read all the visual/sound events from the puppet
	// Note: Usually the group should exists, but the CPipeUser:Reset() is also called in the pipeuser constructor,
	// which later calls this method.
	CAIGroup* pGroup = GetAISystem()->GetAIGroup(GetGroupId());
	if(pGroup)
		pGroup->OnUnitAttentionTargetChanged();
}


void CPipeUser::RestoreAttentionTarget( )
{
//fixMe	-	need to do something 
return;

	SetAttentionTarget( m_pPrevAttentionTarget );
//	m_pAttentionTarget = m_pPrevAttentionTarget;
//	if (m_pAttentionTarget==0)
//		m_bHaveLiveTarget = false;

}




//====================================================================
// RequestPathTo
//====================================================================
void CPipeUser::RequestPathTo(const Vec3 &pos, const Vec3 &dir, bool allowDangerousDestination, int forceTargetBuildingId,
															float endTol, float endDistance, CAIObject* pTargetObject)
{
	m_OrigPath.Clear("CPipeUser::RequestPathTo m_OrigPath");
	m_nPathDecision=PATHFINDER_STILLFINDING;
	m_pPathFindTarget = pTargetObject;
  // add a slight offset to avoid starting below the floor
	Vec3 myPos = m_forcePathGenerationFrom.IsZero() ? GetPhysicsPos() + Vec3(0, 0, 0.05f) : m_forcePathGenerationFrom;
	
	Vec3	endDir = dir;
	// Check the type of the way the movement to the target, 0 = strict 1 = fastest, forget end direction.
	if( m_nMovementPurpose == 1 )
		endDir.Set( 0, 0, 0 );

  GetAISystem()->RequestPathTo(myPos, pos, endDir, this, allowDangerousDestination, forceTargetBuildingId, endTol, endDistance);
}

//===================================================================
// RequestPathInDirection
//===================================================================
void CPipeUser::RequestPathInDirection(const Vec3 &pos, float distance, CAIObject* pTargetObject, float endDistance)
{
	m_OrigPath.Clear("CPipeUser::RequestPathInDirection m_OrigPath");
	m_nPathDecision=PATHFINDER_STILLFINDING;
	m_pPathFindTarget = pTargetObject;
  Vec3 myPos = GetPhysicsPos() + Vec3(0, 0, 0.05f);
  GetAISystem()->RequestPathInDirection(myPos, pos, distance, this, endDistance);
}

//====================================================================
// GetGoalPipe
//====================================================================
CGoalPipe *CPipeUser::GetGoalPipe(const char *name)
{
	CGoalPipe *pPipe = (CGoalPipe*) GetAISystem()->OpenGoalPipe(name);

	if (pPipe)
		return pPipe;
	else
		return 0;
}

//====================================================================
// IsUsingPipe
//====================================================================
bool CPipeUser::IsUsingPipe(const char *name)
{
	CGoalPipe *pPipe = (CGoalPipe*) GetAISystem()->OpenGoalPipe(name);

	CGoalPipe* pExecutingPipe = m_pCurrentGoalPipe;
	while ( pExecutingPipe->IsInSubpipe() )
	{
		if( pExecutingPipe->m_sName == name )
			return true;
		pExecutingPipe = pExecutingPipe->GetSubpipe();
	}
	if( pExecutingPipe->m_sName == name )
		return true;
	return false;
}

//====================================================================
// RemoveActiveGoal
//====================================================================
void CPipeUser::RemoveActiveGoal(int nOrder)
{
	if (m_vActiveGoals.empty())
		return;
	int size = (int)m_vActiveGoals.size();

	if (size == 1)
	{
		m_vActiveGoals.front().pGoalOp->Reset(this);
		m_vActiveGoals.clear();
		return;
	}

	m_vActiveGoals[nOrder].pGoalOp->Reset(this);
	
	if (nOrder != size-1)
		m_vActiveGoals[nOrder] = m_vActiveGoals[size-1];

//	if (m_vActiveGoals.back().name == AIOP_TRACE)
//		m_pReservedNavPoint = 0;

	m_vActiveGoals.pop_back();
}

bool CPipeUser::AbortActionPipe( int goalPipeId )
{
	CGoalPipe* pPipe = m_pCurrentGoalPipe;
	while ( pPipe && pPipe->m_nEventId != goalPipeId )
		pPipe = pPipe->GetSubpipe();
	if ( !pPipe )
		return false;

	// high priority pipes are already aborted or not allowed to be aborted
	if ( pPipe->m_bHighPriority )
		return false;

	// aborted actions become high priority actions
	pPipe->m_bHighPriority = true;

	// cancel all pipes inserted after this one unless another action is found
	while ( pPipe && pPipe->GetSubpipe() )
	{
		if ( pPipe->GetSubpipe()->m_sName == "_action_" )
			break;
		else if ( pPipe->GetSubpipe()->m_nEventId )
			CancelSubPipe( pPipe->GetSubpipe()->m_nEventId );
		pPipe = pPipe->GetSubpipe();
	}
	return true;
}

bool CPipeUser::SelectPipe(int mode, const char *name, IAIObject *pArgument, int goalPipeId, bool resetAlways)
{
	if ( GetEntity() )
		GetAISystem()->GetActionManager()->AbortActionsOnEntity( GetEntity() );

	if (m_pCurrentGoalPipe && !resetAlways)
	{
		if (m_pCurrentGoalPipe->m_sName == name)
		{
			if (pArgument)
				m_pCurrentGoalPipe->m_pArgument = (CAIObject*)pArgument;
			m_pCurrentGoalPipe->SetLoop((mode & AIGOALPIPE_RUN_ONCE)==0);
			return true;
		}
	}

	CGoalPipe* pHigherPriority = m_pCurrentGoalPipe;
	while (pHigherPriority && !pHigherPriority->m_bHighPriority)
		pHigherPriority = pHigherPriority->GetSubpipe();

	if (!pHigherPriority && pArgument)
	{
		SetLastOpResult((CAIObject*)pArgument);
	}

	if (!pHigherPriority || resetAlways)
	{
		// reset some stuff with every goal-pipe change -----------------------------------------------------------------------------------
		m_State.bHurryNow = true;
		// strafing dist should be reset 
		CPuppet* pPuppet = CastToCPuppet();
		if (pPuppet)
		{
			pPuppet->SetAllowedStrafeDistances(0, 0, false);
			pPuppet->SetAdaptiveMovementUrgency(0, 0, 0);
			pPuppet->SetDelayedStance(STANCE_NULL);
		}

		// reset some stuff here
		if (GetProxy()&& (mode & AIGOALPIPE_DONT_RESET_AG)==0)
			GetProxy()->ResetAGInput(AIAG_ACTION);

		SetNavSOFailureStates();
	}

	CGoalPipe* pPipe = GetAISystem()->IsGoalPipe(name);
	if (pPipe)
	{
		if (GetAttentionTarget())
			pPipe->m_vAttTargetPosAtStart = GetAttentionTarget()->GetPos();
		else
			pPipe->m_vAttTargetPosAtStart.zero();

		pPipe->m_pArgument = (CAIObject*) pArgument;
	}
	else
	{
		AIError("SelectPipe: Goalpipe '%s' does not exists. Selecting default goalpipe '_first_' instead.", name);
		pPipe = GetAISystem()->IsGoalPipe( "_first_" );
		AIAssert( pPipe );
		pPipe->m_pArgument = NULL;
	}

	if (!pHigherPriority || resetAlways)
		ResetCurrentPipe( resetAlways );
	m_pCurrentGoalPipe = pPipe;		// this might be too slow, in which case we will go back to registration
	m_pCurrentGoalPipe->Reset();
	m_pCurrentGoalPipe->m_nEventId = goalPipeId;

	if (pHigherPriority && !resetAlways)
		m_pCurrentGoalPipe->SetSubpipe( pHigherPriority );
	else
	{
		m_pReservedNavPoint = 0;
	}
	m_pCurrentGoalPipe->SetLoop((mode & AIGOALPIPE_RUN_ONCE)==0);

	GetAISystem()->Record(this, IAIRecordable::E_GOALPIPESELECTED, name);

	if(pPipe)
	{
		RecorderEventData recorderEventData(name);
		RecordEvent(IAIRecordable::E_GOALPIPESELECTED, &recorderEventData);
	}
	else
	{
		string	str(name);
		str += "[Not Found!]";
		RecorderEventData recorderEventData(str.c_str());
		RecordEvent(IAIRecordable::E_GOALPIPESELECTED, &recorderEventData);
	}

	ClearInvalidatedSOLinks();

	//	AILogEvent("entity %s selecting goalpipe %s",m_sName,name);
	return true;
}


void CPipeUser::RegisterAttack(const char *name)
{
	/*
	CGoalPipe *pPipe = GetGoalPipe(name);

	if ((pPipe) && (m_mapAttacks.find(name)==m_mapAttacks.end()))
	{
		// clone this pipe first.. each puppet must use its own copy
		CGoalPipe *pClone = pPipe->Clone();
		m_mapAttacks.insert(GoalMap::iterator::value_type(name,pClone));
	}
	*/
}

void CPipeUser::RegisterRetreat(const char *name)
{
	/*
	CGoalPipe *pPipe = GetGoalPipe(name);

	if ((pPipe) && (m_mapRetreats.find(name)==m_mapRetreats.end()))
	{
		// clone this pipe first.. each puppet must use its own copy
		CGoalPipe *pClone = pPipe->Clone();
		m_mapRetreats.insert(GoalMap::iterator::value_type(name,pClone));
	}
	*/
}


void CPipeUser::RegisterIdle(const char *name)
{
	/*
	CGoalPipe *pPipe = GetGoalPipe(name);

	if ((pPipe) && (m_mapIdles.find(name)==m_mapIdles.end()))
	{
		// clone this pipe first.. each puppet must use its own copy
		CGoalPipe *pClone = pPipe->Clone();
		m_mapIdles.insert(GoalMap::iterator::value_type(name,pClone));
	}
	*/
}



void CPipeUser::RegisterWander(const char *name)
{
	/*
	CGoalPipe *pPipe = GetGoalPipe(name);

	if ((pPipe) && (m_mapWanders.find(name)==m_mapWanders.end()))
	{
		// clone this pipe first.. each puppet must use its own copy
		CGoalPipe *pClone = pPipe->Clone();
		m_mapWanders.insert(GoalMap::iterator::value_type(name,pClone));
	}
	*/
}

//
// make sure none of the active goals uses the removed object
void CPipeUser::OnObjectRemoved(CAIObject *pObject)
{
	CAIActor::OnObjectRemoved(pObject);

	if(pObject == m_pPathFindTarget)
		m_pPathFindTarget = 0;

  if (pObject == m_pLookAtTarget)
	{
    m_pLookAtTarget = 0;
		// LookAtTArget sould not be removed - it belongs to pipeUSer
		AIAssert(m_pendingNavSOStates.IsEmpty() && m_currentNavSOStates.IsEmpty());
	}

	if (pObject == m_pLooseAttentionTarget)
		SetLooseAttentionTarget(0);
	
	if (pObject == m_pAttentionTarget)
		SetAttentionTarget(0);
	
	if (pObject == m_pLastOpResult)
		m_pLastOpResult = 0;

	if (pObject == m_pPrevAttentionTarget)
		m_pPrevAttentionTarget = 0;

	if (pObject == m_pRefPoint)
		m_pRefPoint = 0;

	if (pObject == m_pFireTarget)
		m_pFireTarget = 0;

	if (!m_vActiveGoals.empty())
	{
		//ListOGoals::iterator gi;
		for (size_t i = 0; i < m_vActiveGoals.size(); i++)
		{
			//QGoal Goal = (*gi);
			QGoal& Goal = m_vActiveGoals[i];
			Goal.pGoalOp->OnObjectRemoved(pObject);
		}
	}
}

// used by action system to remove "_action_" inserted goal pipe
// which would notify goal pipe listeners and resume previous goal pipe
bool CPipeUser::RemoveSubPipe( int goalPipeId, bool keepInserted )
{
	if ( !goalPipeId )
		return false;

	if ( !m_pCurrentGoalPipe )
	{
		m_notAllowedSubpipes.insert( goalPipeId );
		return false;
	}

	// find the last goal pipe in the list
	CGoalPipe* pTargetPipe = NULL;
	CGoalPipe* pPrevPipe = m_pCurrentGoalPipe;
	while ( pPrevPipe->GetSubpipe() )
	{
		pPrevPipe = pPrevPipe->GetSubpipe();
		if ( pPrevPipe->m_nEventId == goalPipeId )
			pTargetPipe = pPrevPipe;
	}

	if ( keepInserted == false && pTargetPipe != NULL )
	{
		while ( CGoalPipe* pSubpipe = pTargetPipe->GetSubpipe() )
		{
			if ( pSubpipe->m_bHighPriority )
				break;
			if ( pSubpipe->m_nEventId != 0 )
				CancelSubPipe( pSubpipe->m_nEventId );
			else
				pTargetPipe = pSubpipe;
		}
	}

	if ( m_pCurrentGoalPipe->RemoveSubPipe(goalPipeId, keepInserted, true) )
	{
		NotifyListeners( goalPipeId, ePN_Removed );

		// find the last goal pipe in the list
		CGoalPipe* pCurrent = m_pCurrentGoalPipe;
		while ( pCurrent->GetSubpipe() )
			pCurrent = pCurrent->GetSubpipe();

		// only if the removed pipe was the last one in the list
		if ( pCurrent != pPrevPipe )
		{
			// remove goals
			ClearActiveGoals();

			// and resume the new last goal pipe
			SetLastOpResult( pCurrent->m_pArgument );
			NotifyListeners( pCurrent, ePN_Resumed );
		}
		return true;
	}
	else
	{
		m_notAllowedSubpipes.insert( goalPipeId );
		return false;
	}
}

// used by ai flow graph nodes to cancel inserted goal pipe
// which would notify goal pipe listeners that operation was failed
bool CPipeUser::CancelSubPipe( int goalPipeId )
{
	if ( !m_pCurrentGoalPipe )
	{
		if ( goalPipeId != 0 )
			m_notAllowedSubpipes.insert( goalPipeId );
		return false;
	}

	CGoalPipe* pCurrent = m_pCurrentGoalPipe;
	while ( pCurrent->GetSubpipe() )
		pCurrent = pCurrent->GetSubpipe();
	bool bClearNeeded = !goalPipeId || pCurrent->m_nEventId == goalPipeId;

	// enable this since it might be disabled from canceled goal pipe (and planed to enable it again at pipe end)
	m_bCanReceiveSignals = true;

	if ( m_pCurrentGoalPipe->RemoveSubPipe(goalPipeId, true, true) )
	{
		NotifyListeners( goalPipeId, ePN_Deselected );

		if ( bClearNeeded )
		{
			ClearActiveGoals();

			// resume the parent goal pipe
			pCurrent = m_pCurrentGoalPipe;
			while ( pCurrent->GetSubpipe() )
				pCurrent = pCurrent->GetSubpipe();
			SetLastOpResult( pCurrent->m_pArgument );
			NotifyListeners( pCurrent, ePN_Resumed );
		}

		return true;
	}
	else
	{
		if ( goalPipeId != 0 )
			m_notAllowedSubpipes.insert( goalPipeId );
		return false;
	}
}

IGoalPipe* CPipeUser::InsertSubPipe(int mode, const char * name, IAIObject * pArgument, int goalPipeId)
{
	if (!m_pCurrentGoalPipe)
	{
		return NULL;
	}

	if ( goalPipeId != 0 )
	{
		VectorSet< int >::iterator itFind = m_notAllowedSubpipes.find( goalPipeId );
		if ( itFind != m_notAllowedSubpipes.end() )
		{
			m_notAllowedSubpipes.erase( itFind );
			return NULL;
		}
	}

	if (m_pCurrentGoalPipe->CountSubpipes() >= 8)
	{
		AIWarning("%s has too many (%d) subpipes. Pipe <%s>; inserting <%s>", GetName(), m_pCurrentGoalPipe->CountSubpipes(),
			m_pCurrentGoalPipe->m_sName.c_str(), name);
		//			assert(m_pCurrentGoalPipe->CountSubpipes() < 10);
		//		AIAssert(m_vActiveGoals.size() < 100);
	}

	bool bExclusive = (mode & AIGOALPIPE_NOTDUPLICATE) != 0;
	bool bHighPriority = (mode & AIGOALPIPE_HIGHPRIORITY) != 0; // it will be not removed when a goal pipe is selected
	bool bSamePriority = (mode & AIGOALPIPE_SAMEPRIORITY) != 0; // sets the priority to be the same as active goal pipe

//	if (m_pCurrentGoalPipe->m_sName == name && !goalPipeId)
//		return NULL;

	// first lets find the goalpipe
	CGoalPipe* pPipe = GetAISystem()->IsGoalPipe( name );
	if ( !pPipe )
	{
		AIWarning("InsertSubPipe: Goalpipe '%s' does not exists. No goalpipe inserted.", name);

		string	str(name);
		str += "[Not Found!]";
		RecorderEventData recorderEventData(str.c_str());
		RecordEvent(IAIRecordable::E_GOALPIPEINSERTED, &recorderEventData);

		return NULL;
	}

	if ( bHighPriority )
		pPipe->m_bHighPriority = true;

	// now find the innermost pipe
	CGoalPipe* pExecutingPipe = m_pCurrentGoalPipe;
	int i=0;
	while ( pExecutingPipe->IsInSubpipe() )
	{
		if( bExclusive && pExecutingPipe->m_sName == name )
			return NULL;
		if ( !bSamePriority && !bHighPriority )
		{
			if ( pExecutingPipe->GetSubpipe()->m_bHighPriority )
				break;
		}
		pExecutingPipe = pExecutingPipe->GetSubpipe();
		++i;
	}
	
	if(i>10)
	{
		CGoalPipe* pExecutingPipe = m_pCurrentGoalPipe;
		AIWarning("%s: More than 10 subpipes",GetName());
		int j=0;
		while ( pExecutingPipe->IsInSubpipe() )
		{
			AILogAlways("%d) %s",j++, pExecutingPipe->m_sDebugName.empty() ? pExecutingPipe->GetName() : pExecutingPipe->m_sDebugName.c_str());
			pExecutingPipe = pExecutingPipe->GetSubpipe();
		}
	}

	if ( bExclusive && pExecutingPipe->m_sName == name )
		return NULL;
	if ( !pExecutingPipe->GetSubpipe() )
	{
		if ( !m_vActiveGoals.empty() )
		{
			if ( m_bBlocked )
			{
				// pop the last executing goal
				ClearActiveGoals();
				//RemoveActiveGoal(m_vActiveGoals.size()-1);

				// but make sure we end up executing it again
				pExecutingPipe->ReExecuteGroup();
			}
			else
			{
				ClearActiveGoals();
			}
		}

		NotifyListeners( pExecutingPipe, ePN_Suspended );

		if ( pArgument )
			SetLastOpResult( (CAIObject*) pArgument );

		// unblock current pipe
		m_bBlocked = false;
	}

	pExecutingPipe->SetSubpipe(pPipe);

	if(GetAttentionTarget())
		pPipe->m_vAttTargetPosAtStart = GetAttentionTarget()->GetPos();
	else
		pPipe->m_vAttTargetPosAtStart.zero();
	pPipe->m_pArgument = (CAIObject*) pArgument;
	pPipe->m_nEventId = goalPipeId;

	GetAISystem()->Record(this, IAIRecordable::E_GOALPIPEINSERTED, name);

	RecorderEventData recorderEventData(name);
	RecordEvent(IAIRecordable::E_GOALPIPEINSERTED, &recorderEventData);

	return pPipe;
}

void CPipeUser::ClearActiveGoals()
{
	if (!m_vActiveGoals.empty())
	{
		VectorOGoals::iterator li, liEnd = m_vActiveGoals.end();
		for (li = m_vActiveGoals.begin(); li != liEnd; ++li)
			li->pGoalOp->Reset(this);

		m_vActiveGoals.clear();
	}
	m_bBlocked = false;
}

void CPipeUser::ResetCurrentPipe( bool resetAlways )
{
	GetAISystem()->Record(this, IAIRecordable::E_GOALPIPERESETED, m_pCurrentGoalPipe!=NULL?m_pCurrentGoalPipe->m_sName.c_str():NULL);

	RecorderEventData recorderEventData(m_pCurrentGoalPipe!=NULL?m_pCurrentGoalPipe->m_sName.c_str():NULL);
	RecordEvent(IAIRecordable::E_GOALPIPERESETED, &recorderEventData);

	CGoalPipe* pPipe = m_pCurrentGoalPipe;
	while ( pPipe )
	{
		NotifyListeners( pPipe, ePN_Deselected );
		if ( resetAlways || !pPipe->GetSubpipe() || !pPipe->GetSubpipe()->m_bHighPriority )
			pPipe = pPipe->GetSubpipe();
		else
			break;
	}

	if ( !pPipe )
		ClearActiveGoals();
	else
		pPipe->SetSubpipe( NULL );

	if ( m_pCurrentGoalPipe )
	{
		delete m_pCurrentGoalPipe;
		m_pCurrentGoalPipe = 0;
	}

	if ( !pPipe )
	{
		m_bCanReceiveSignals = true;
		m_bKeepMoving = false;
		ResetLookAt();
	}
}

int CPipeUser::GetGoalPipeId() const
{
	const CGoalPipe* pPipe = m_pCurrentGoalPipe;
	if ( !pPipe )
		return -1;
	while ( pPipe->GetSubpipe() )
		pPipe = pPipe->GetSubpipe();
	return pPipe->m_nEventId;
}

void CPipeUser::ResetLookAt()
{
	m_bPriorityLookAtRequested = false;
	if ( m_bLooseAttention )
	{
		//m_bLooseAttention = false;
		//m_pLooseAttentionTarget = NULL;
		SetLooseAttentionTarget(0);
		/*if ( m_pLookAtTarget )
			GetAISystem()->RemoveDummyObject( m_pLookAtTarget );
		m_pLookAtTarget = NULL;*/
	}
}

bool CPipeUser::SetLookAtPoint( const Vec3& point, bool priority )
{
	if(!priority&&m_bPriorityLookAtRequested)
		return false;
	m_bPriorityLookAtRequested = priority;
//	if( !m_pLookAtTarget )
	//	m_pLookAtTarget = GetAISystem()->CreateDummyObject(m_sName + "_LookAtTarget");
	m_pLookAtTarget->SetPos( point );
	m_bLooseAttention = true;
	m_pLooseAttentionTarget = m_pLookAtTarget;

	// check only in 2D
	Vec3 desired( point - GetPos() );
	desired.z = 0;
	desired.NormalizeSafe();
	SAIBodyInfo bodyInfo;
	GetProxy()->QueryBodyInfo( bodyInfo );
	Vec3 current( bodyInfo.vEyeDirAnim );
	current.z = 0;
	current.NormalizeSafe();

	// cos( 11.5deg ) ~ 0.98
	return 0.98f <= current.Dot( desired );
}

bool CPipeUser::SetLookAtDir( const Vec3& dir, bool priority )
{
	if(!priority&&m_bPriorityLookAtRequested)
		return false;
	m_bPriorityLookAtRequested = priority;
	Vec3 vDir = dir;
	if ( vDir.NormalizeSafe() )
	{
		//if( !m_pLookAtTarget )
			//m_pLookAtTarget = GetAISystem()->CreateDummyObject(m_sName + "_LookAtTarget");
		m_pLookAtTarget->SetPos( GetPos() + vDir * 100.0f );
		m_bLooseAttention = true;
		m_pLooseAttentionTarget = m_pLookAtTarget;

		// check only in 2D
		Vec3 desired( vDir );
		desired.z = 0;
		desired.NormalizeSafe();
		SAIBodyInfo bodyInfo;
		GetProxy()->QueryBodyInfo( bodyInfo );
		Vec3 current( bodyInfo.vEyeDirAnim );
		current.z = 0;
		current.NormalizeSafe();

		// cos( 11.5deg ) ~ 0.98
		if ( 0.98f <= current.Dot(desired) )
		{
			/*GetAISystem()->RemoveDummyObject( m_pLookAtTarget );
			m_pLookAtTarget = m_pLooseAttentionTarget = NULL;*/
			SetLooseAttentionTarget(0);
			return true;
		}
		else
			return false;
	}
	else
		return true;
}

IAIObject* CPipeUser::GetSpecialAIObject(const char* objName, float range)
{
	IAIObject* pObject = NULL;

	if( range <= 0.0f )
		range = 20.0f;

	if(strcmp(objName, "player") == 0)
	{
		pObject = GetAISystem()->GetPlayer();
	}
	else if(strcmp(objName, "self") == 0)
	{
		pObject = this;
	}
	else if(strcmp(objName, "beacon") == 0)
	{
		pObject = (IAIObject*) GetAISystem()->GetBeacon(GetGroupId());
	}
	else if(strcmp(objName, "refpoint") == 0)
	{
		CPipeUser* pPipeUser = CastToCPipeUser();
		if (pPipeUser)
			pObject = pPipeUser->GetRefPoint();
	}
	else if(strcmp(objName, "formation") == 0)
	{
		CLeader* pLeader = GetAISystem()->GetLeader(GetGroupId());
		// is leader managing my formation?
		if(pLeader && pLeader->GetFormationOwner() && pLeader->GetFormationOwner()->m_pFormation)
		{
			// check if I already have one
			pObject = static_cast<IAIObject*>(pLeader->GetFormationPoint(this));
			if(!pObject)
				pObject = static_cast<IAIObject*>(pLeader->GetNewFormationPoint(this));
		}
		else
		{
			CAIObject	*pLastOp(static_cast<CAIObject*>(GetLastOpResult()));
			// check if Formation point is already acquired to lastOpResult
			if(pLastOp && pLastOp->GetSubType()==CAIObject::STP_FORMATION)
				pObject = pLastOp;
			else
			{
					CAIObject *pBoss = static_cast<CAIObject*>(GetAISystem()->GetNearestObjectOfTypeInRange(this, AIOBJECT_PUPPET, 0, range, AIFAF_HAS_FORMATION|AIFAF_INCLUDE_DEVALUED|AIFAF_SAME_GROUP_ID));
					if(!pBoss)
						pBoss = static_cast<CAIObject*>(GetAISystem()->GetNearestObjectOfTypeInRange(this, AIOBJECT_VEHICLE, 0, range, AIFAF_HAS_FORMATION|AIFAF_INCLUDE_DEVALUED|AIFAF_SAME_GROUP_ID));
					if(pBoss)
					{
						// check if I already have one
						pObject = pBoss->m_pFormation->GetFormationPoint(this);
						if(!pObject)
							pObject = pBoss->m_pFormation->GetNewFormationPoint(this);
					}
			}
			if (!pObject)
			{
				// lets send a NoFormationPoint event
				SetSignal(1,"OnNoFormationPoint", 0,0,g_crcSignals.m_nOnNoFormationPoint);
				if(pLeader)
					pLeader->SetSignal(1,"OnNoFormationPoint",GetEntity(),0,g_crcSignals.m_nOnNoFormationPoint);
			}
		}
	}
	else if(strcmp(objName, "formation_id") == 0)
	{
		
		CAIObject	*pLastOp(static_cast<CAIObject*>(GetLastOpResult()));
		// check if Formation point is already aquired to lastOpResult
		
		if(pLastOp && pLastOp->GetSubType()==CAIObject::STP_FORMATION)
			pObject = pLastOp;
		else	// if not - find available formation point
		{
			int groupid((int)range);
			CAIObject *pBoss(NULL);
			AIObjects::iterator ai;
			if ( (ai=GetAISystem()->m_mapGroups.find(groupid)) != GetAISystem()->m_mapGroups.end() )
			{
				for (;ai!=GetAISystem()->m_mapGroups.end() && !pBoss;)
				{
					if (ai->first != groupid)
						break;
					if (ai->second != this && ai->second->m_pFormation)
					{
						pBoss = ai->second;
					}
					++ai;
				}
			}
			if(pBoss && pBoss->m_pFormation)
			{
				// check if I already have one

				pObject = pBoss->m_pFormation->GetFormationPoint(this);
				if(!pObject)
					pObject = pBoss->m_pFormation->GetNewFormationPoint(this);
			}

			if (!pObject)
			{
				// lets send a NoFormationPoint event
				SetSignal(1,"OnNoFormationPoint", 0,0, g_crcSignals.m_nOnNoFormationPoint);
			}
		}

	}
	else if(strcmp(objName, "formation_special") == 0)
	{
		CAIObject *pFormationPoint = NULL;
		CLeader* pLeader = GetAISystem()->GetLeader(GetGroupId());
		if(pLeader)
			pObject = static_cast<IAIObject*>(pLeader->GetNewFormationPoint((CAIObject*)this, SPECIAL_FORMATION_POINT));

		if (!pObject)
		{
			// lets send a NoFormationPoint event
			SetSignal(1,"OnNoFormationPoint", 0,0, g_crcSignals.m_nOnNoFormationPoint);
			if(pLeader)
				pLeader->SetSignal(1,"OnNoFormationPoint",GetEntity(), 0, g_crcSignals.m_nOnNoFormationPoint);
		}
	}
	else if(strcmp(objName, "formationsight") == 0)
	{
		CLeader* pLeader = GetAISystem()->GetLeader(GetGroupId());
		if(pLeader)
			pObject= static_cast<IAIObject*>(pLeader->GetFormationPointSight(this));
	}
	else if(strcmp(objName, "atttarget") == 0)
	{
		pObject= m_pAttentionTarget;
	}
	else if(strcmp(objName, "hidepoint_lastop") == 0)
	{
		int nbid;
		IVisArea *iva;

		Vec3 hideFrom;
		if (m_pAttentionTarget)
			hideFrom = m_pAttentionTarget->GetPos();
		else
			hideFrom = GetPos();

		Vec3 pos = FindHidePoint(range, hideFrom, HM_IGNORE_ENEMIES+ HM_NEAREST + HM_AROUND_LASTOP, GetAISystem()->CheckNavigationType(GetPos(),nbid,iva,m_movementAbility.pathfindingProperties.navCapMask));
		if (!IsEquivalent(pos,GetPos(),0.01f))
		{
			pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_HIDEPOINT_LASTOP));
			pObject->SetPos(pos);
		}
	}
	else if(strcmp(objName, "hidepoint") == 0)
	{
		int nbid;
		IVisArea *iva;

		Vec3 hideFrom;
		if (m_pAttentionTarget)
			hideFrom = m_pAttentionTarget->GetPos();
		else
			hideFrom = GetPos();

		Vec3 pos = FindHidePoint(range, hideFrom,HM_IGNORE_ENEMIES+HM_NEAREST, GetAISystem()->CheckNavigationType(GetPos(),nbid,iva,m_movementAbility.pathfindingProperties.navCapMask));
		if (!IsEquivalent(pos,GetPos(),0.01f))
		{
			pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_HIDEPOINT));
			pObject->SetPos(pos);
		}
	}
	else if(strcmp(objName, "last_hideobject") == 0)
	{
		if(m_CurrentHideObject.IsValid())
		{
			pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_LAST_HIDEOBJECT));
			pObject->SetPos(m_CurrentHideObject.GetObjectPos());
		}
	}	
	else if(strcmp(objName, "trackpattern") == 0)
	{
		// Get the next pint from target tracker.
		CPuppet* pPuppet = CastToCPuppet();
		if (pPuppet)
			pObject = pPuppet->GetTargetTrackPoint();
	}
	else if(strcmp(objName, "lookat_target") == 0)
	{
		if ( m_bLooseAttention && m_pLooseAttentionTarget )
			pObject = m_pLooseAttentionTarget;
	}
	else if(strcmp(objName, "probtarget") == 0)
	{
		pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_PROBTARGET));
		pObject->SetPos(GetProbableTargetPosition());
	}
	else if(strcmp(objName, "probtarget_in_territory") == 0)
	{
		pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_PROBTARGET_IN_TERRITORY));
		Vec3	pos = GetProbableTargetPosition();
		// Clamp the point to the territory shape.
		CPuppet* pPuppet = CastToCPuppet();
		if(pPuppet && pPuppet->GetTerritoryShape())
				pPuppet->GetTerritoryShape()->ConstrainPointInsideShape(pos, true);

		pObject->SetPos(pos);
	}
	else if(strcmp(objName, "probtarget_in_refshape") == 0)
	{
		pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_PROBTARGET_IN_REFSHAPE));
		Vec3	pos = GetProbableTargetPosition();
		// Clamp the point to ref shape
		if(GetRefShape())
			GetRefShape()->ConstrainPointInsideShape(pos, true);
		pObject->SetPos(pos);
	}
	else if(strcmp(objName, "probtarget_in_territory_and_refshape") == 0)
	{
		pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_PROBTARGET_IN_TERRITORY_AND_REFSHAPE));
		Vec3	pos = GetProbableTargetPosition();
		// Clamp the point to ref shape
		if(GetRefShape())
			GetRefShape()->ConstrainPointInsideShape(pos, true);
		// Clamp the point to the territory shape.
		CPuppet* pPuppet = CastToCPuppet();
		if(pPuppet && pPuppet->GetTerritoryShape())
				pPuppet->GetTerritoryShape()->ConstrainPointInsideShape(pos, true);

		pObject->SetPos(pos);
	}
	else if(strcmp(objName, "atttarget_in_territory") == 0)
	{
		pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_ATTTARGET_IN_TERRITORY));
		if(GetAttentionTarget())
		{
			Vec3	pos = GetAttentionTarget()->GetPos();
			// Clamp the point to the territory shape.
			CPuppet* pPuppet = CastToCPuppet();
			if(pPuppet && pPuppet->GetTerritoryShape())
					pPuppet->GetTerritoryShape()->ConstrainPointInsideShape(pos, true);

			pObject->SetPos(pos);
		}
		else
		{
			pObject->SetPos(GetPos());
		}
	}
	else if(strcmp(objName, "atttarget_in_refshape") == 0)
	{
		pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_ATTTARGET_IN_REFSHAPE));
		if(GetAttentionTarget())
		{
			Vec3	pos = GetAttentionTarget()->GetPos();
			// Clamp the point to ref shape
			if(GetRefShape())
				GetRefShape()->ConstrainPointInsideShape(pos, true);
			// Update pos
			pObject->SetPos(pos);
		}
		else
		{
			pObject->SetPos(GetPos());
		}
	}
	else if(strcmp(objName, "atttarget_in_territory_and_refshape") == 0)
	{
		pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_ATTTARGET_IN_TERRITORY_AND_REFSHAPE));
		if(GetAttentionTarget())
		{
			Vec3	pos = GetAttentionTarget()->GetPos();
			// Clamp the point to ref shape
			if(GetRefShape())
				GetRefShape()->ConstrainPointInsideShape(pos, true);
			// Clamp the point to the territory shape.
			CPuppet* pPuppet = CastToCPuppet();
			if(pPuppet && pPuppet->GetTerritoryShape())
					pPuppet->GetTerritoryShape()->ConstrainPointInsideShape(pos, true);

			pObject->SetPos(pos);
		}
		else
		{
			pObject->SetPos(GetPos());
		}
	}
	else if(strcmp(objName, "animtarget") == 0)
	{
		if(m_pActorTargetRequest)
		{
			pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_ANIM_TARGET));
			pObject->SetPos(m_pActorTargetRequest->approachLocation, m_pActorTargetRequest->approachDirection);
		}
	}
	else if(strcmp(objName, "group_tac_pos") == 0)
	{
		pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_GROUP_TAC_POS));
	}
	else if(strcmp(objName, "group_tac_look") == 0)
	{
		pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_GROUP_TAC_LOOK));
	}
	else if(strcmp(objName, "vehicle_avoid") == 0)
	{
		pObject = static_cast<IAIObject*>(GetOrCreateSpecialAIObject(AISPECIAL_VEHICLE_AVOID_POS));
	}
	else
	{
		pObject = GetAISystem()->GetAIObjectByName(objName);
	}
	return pObject;
}


//====================================================================
// GetTargetScore
//====================================================================
static int	GetTargetScore(CAIObject* pAI, CAIObject* pTarget)
{
	// Returns goodness value based on the target type.
	int	score = 0;

	if(pAI->IsHostile(pTarget)) score += 10;
	if(pTarget->GetSubType() == IAIObject::STP_SOUND) score += 1;
	if(pTarget->GetSubType() == IAIObject::STP_MEMORY) score += 2;
	if(pTarget->IsAgent()) score += 5;

	return score;
}

//====================================================================
// GetProbableTargetPosition
//====================================================================
Vec3 CPipeUser::GetProbableTargetPosition()
{
	if(m_timeSinceLastLiveTarget >= 0.0f && m_timeSinceLastLiveTarget < 2.0f)
	{
		// The current target is fresh, use it.
		return m_lastLiveTargetPos;
	}
	else
	{
		// - Choose the nearest target that is more fresh than ours
		//   among the units sharing the same combat class in the group
		// - If no fresh targets are not available, try to choose the most
		//   important attention target
		// - If no attention target is available, try beacon
		// - If there is no beacon, just return the position of self.
		bool	foundLive = false;
		bool	foundAny = false;

		Vec3	nearestKnownTarget;
		if(m_pAttentionTarget)
			nearestKnownTarget = m_pAttentionTarget->GetPos();
		else
			nearestKnownTarget = GetPos();

		float	nearestLiveDist = FLT_MAX;
		Vec3	nearestLivePos = nearestKnownTarget;

		float	nearestAnyDist = FLT_MAX;
		Vec3	nearestAnyPos = nearestKnownTarget;
		int		nearestAnyScore = 0;

		int	combatClass = m_Parameters.m_CombatClass;

		int groupId = GetGroupId();

		AIObjects::iterator gi = GetAISystem()->m_mapGroups.find(groupId);
		AIObjects::iterator gend = GetAISystem()->m_mapGroups.end();

		for(; gi != gend; ++gi)
		{
			if(gi->first != groupId) break;
			CAIActor* pAIActor = gi->second->CastToCAIActor();
			if(!pAIActor || pAIActor->GetParameters().m_CombatClass != combatClass) continue;
			if(!gi->second->IsEnabled()) continue;
			CPuppet* pPuppet = gi->second->CastToCPuppet();
			if(pPuppet)
			{
				// Find nearest fresh live target.
				float	otherTargetTime = pPuppet->GetTimeSinceLastLiveTarget();
				if(otherTargetTime >= 0.0f && otherTargetTime < 2.0f && otherTargetTime < m_timeSinceLastLiveTarget)
				{
					float	dist = Distance::Point_PointSq(nearestKnownTarget, pPuppet->GetLastLiveTargetPosition());
					if(dist < nearestLiveDist)
					{
						nearestLiveDist = dist;
						nearestLivePos = pPuppet->GetLastLiveTargetPosition();
						foundLive = true;
					}
				}

				// Fall back.
				if(!foundLive && !m_pAttentionTarget && pPuppet->GetAttentionTarget())
				{
					const Vec3& targetPos = pPuppet->GetAttentionTarget()->GetPos();
					float	dist = Distance::Point_PointSq(nearestAnyPos, targetPos);
					int	score = GetTargetScore(this, pPuppet);
					if(score >= nearestAnyScore && dist < nearestAnyDist)
					{
						nearestAnyDist = dist;
						nearestAnyPos = targetPos;
						nearestAnyScore = score;
						foundAny = true;
					}
				}
			}
		}

		if (foundLive)
		{
			return nearestLivePos;
		}
		else if (m_pAttentionTarget)
		{
			return m_pAttentionTarget->GetPos();
		}
		else if (foundAny)
		{
			return nearestAnyPos;
		}
		else
		{
			IAIObject*	beacon = GetAISystem()->GetBeacon(GetGroupId());
			if(beacon)
				return beacon->GetPos();
			else
				return GetPos();
		}
	}
}

//====================================================================
// GetOrCreateSpecialAIObject
//====================================================================
CAIObject* CPipeUser::GetOrCreateSpecialAIObject(ESpecialAIObjects type)
{
	if(m_pSpecialObjects[type])
		return m_pSpecialObjects[type];

	ESubTypes	subType = STP_SPECIAL;

	string	name = GetName();
	switch(type)
	{
	case AISPECIAL_HIDEPOINT_LASTOP:
		name += "_*HidePointLastop"; break;
	case AISPECIAL_HIDEPOINT:
		name += "_*HidePoint"; break;
	case AISPECIAL_LAST_HIDEOBJECT:
		name += "_*LastHideObj"; break;
	case AISPECIAL_PROBTARGET:
		name += "_*ProbTgt"; break;
	case AISPECIAL_PROBTARGET_IN_TERRITORY:
		name += "_*ProbTgtInTerr"; break;
	case AISPECIAL_PROBTARGET_IN_REFSHAPE:
		name += "_*ProbTgtInRefShape"; break;
	case AISPECIAL_PROBTARGET_IN_TERRITORY_AND_REFSHAPE:
		name += "_*ProbTgtInTerrAndRefShape"; break;
	case AISPECIAL_ATTTARGET_IN_TERRITORY:
		name += "_*AttTgtInTerr"; break;
	case AISPECIAL_ATTTARGET_IN_REFSHAPE:
		name += "_*AttTgtInRefShape"; break;
	case AISPECIAL_ATTTARGET_IN_TERRITORY_AND_REFSHAPE:
		name += "_*AttTgtInTerrAndRefShape"; break;
	case AISPECIAL_ANIM_TARGET:
		name += "_*AnimTgt";
		subType = STP_ANIM_TARGET;
		break;
	case AISPECIAL_GROUP_TAC_POS:
		name += "_GroupTacPos";
		subType = STP_FORMATION;
		break;
	case AISPECIAL_GROUP_TAC_LOOK:
		name += "_GroupTacLook";
		break;
	case AISPECIAL_VEHICLE_AVOID_POS:
		name += "_VehicleAvoid";
		break;
	default:
		{
			char	buf[64];
			_snprintf(buf, 64, "_*Special%02d", (int)type);
			name += buf;
			break;
		}
	}

	m_pSpecialObjects[type] = GetAISystem()->CreateDummyObject(name, subType);

	return m_pSpecialObjects[type];
}

//====================================================================
// PathIsInvalid
//====================================================================
void CPipeUser::PathIsInvalid()
{
	ClearPath("CPipeUser::PathIsInvalid m_Path");
	m_State.vMoveDir.Set(0.0f, 0.0f, 0.0f);
	m_State.fDesiredSpeed = 0.0f;
}


void CPipeUser::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	// m_mapGoalPipeListeners must not change here! there's no need to serialize it!
	
  if(ser.IsReading())
  {
		if(m_pRefPoint)
		{
	    GetAISystem()->RemoveDummyObject(m_pRefPoint);
		  m_pRefPoint = 0;
		}

		for(unsigned i = 0; i < COUNT_AISPECIAL; ++i)
		{
			if(m_pSpecialObjects[i])
				GetAISystem()->RemoveDummyObject(m_pSpecialObjects[i]);
			m_pSpecialObjects[i] = 0;
		}
		ResetCurrentPipe( true );
	}

	CAIActor::Serialize(ser, objectTracker);

	int goalsCount = m_vActiveGoals.size();
	ser.Value("ActiveGoalsCount", goalsCount);

	// serialize members
	objectTracker.SerializeObjectPointer(ser, "m_pAttentionTarget", m_pAttentionTarget, false);
	ser.Value("m_AttTargetPersistenceTimeout",m_AttTargetPersistenceTimeout);
	ser.EnumValue("m_AttTargetThreat", m_AttTargetThreat, AITHREAT_NONE, AITHREAT_LAST);
	ser.EnumValue("m_AttTargetExposureThreat", m_AttTargetExposureThreat, AITHREAT_NONE, AITHREAT_LAST);
	ser.EnumValue("m_AttTargetType", m_AttTargetType, AITARGET_NONE, AITARGET_LAST);
	objectTracker.SerializeObjectPointer(ser, "m_pPrevAttentionTarget", m_pPrevAttentionTarget, false);
	objectTracker.SerializeObjectPointer(ser, "m_pLastOpResult", m_pLastOpResult, false);		
	objectTracker.SerializeObjectPointer(ser, "m_pReservedNavPoint", m_pReservedNavPoint, false);

	ser.Value("m_nMovementPurpose", m_nMovementPurpose);
	ser.Value("m_fTimePassed", m_fTimePassed);
	ser.Value("m_bHiding", m_bHiding);
	ser.Value("LooseAttention", m_bLooseAttention);
	objectTracker.SerializeObjectPointer(ser, "m_pLooseAttentionTarget",  m_pLooseAttentionTarget, false);
	ser.Value("m_looseAttentionId",m_looseAttentionId);

	if(ser.IsReading() && m_pLookAtTarget)
	{
		GetAISystem()->RemoveDummyObject(m_pLookAtTarget);
		m_pLookAtTarget=NULL;
	}
	objectTracker.SerializeObjectPointer(ser, "m_pLookAtTarget",m_pLookAtTarget,false);
	AIAssert(m_pLookAtTarget);

	m_CurrentHideObject.Serialize(ser,objectTracker);

	ser.Value("m_nPathDecision", m_nPathDecision);
	ser.Value("m_IsSteering",m_IsSteering);


	int	fireMode = (int)m_fireMode;
	ser.Value("m_fireMode", fireMode);
	if(ser.IsReading())
		m_fireMode = (EFireMode)fireMode;
	objectTracker.SerializeObjectPointer(ser, "m_pFireTarget", m_pFireTarget, false);
//	ser.Value("m_fireAtLastOpResult", m_fireAtLastOpResult);
	ser.Value("m_fireModeUpdated",m_fireModeUpdated);
	ser.Value("m_outOfAmmoSent",m_outOfAmmoSent);
	ser.Value("m_bFirstUpdate",m_bFirstUpdate);
	ser.Value("m_bStartTiming", m_bStartTiming);
	ser.Value("m_fEngageTime", m_fEngageTime);
	ser.Value("m_pathToFollowName",m_pathToFollowName);
	ser.Value("m_bPathToFollowIsSpline",m_bPathToFollowIsSpline);
	ser.EnumValue("m_CurrentNodeNavType",m_CurrentNodeNavType,IAISystem::NAV_UNSET,IAISystem::NAV_MAX_VALUE);
	//ser.Value( "m_fTargetAimingTime", m_fTargetAimingTime);
	objectTracker.SerializeObjectPointer(ser, "m_pRefPoint", m_pRefPoint, false);
  AIAssert(m_pRefPoint);

	// Serialize special objects.
	char	specialName[64];
	for(unsigned i = 0; i < COUNT_AISPECIAL; ++i)
	{
		//if(m_pSpecialObjects[i])
		{
			_snprintf(specialName, 64, "m_pSpecialObjects%d", i);
			objectTracker.SerializeObjectPointer(ser, specialName, m_pSpecialObjects[i], false);
		}
	}

	// serialize executing pipe
	if(ser.IsReading())
	{
		string	goalPipeName;
		ser.Value( "PipeName", goalPipeName);

		// always create a new goal pipe
		CGoalPipe *pPipe = NULL;
		if ( goalPipeName != "none" )
		{
			pPipe = new CGoalPipe(goalPipeName, GetAISystem(), true);
			ser.BeginGroup( "DynamicPipe" );
				pPipe->SerializeDynamic( ser );
			ser.EndGroup();
		}

	//	m_vActiveGoals.clear(); not needed, will be done in ResetCurrentPipe();
		m_pCurrentGoalPipe = pPipe;		// this might be too slow, in which case we will go back to registration
		if ( m_pCurrentGoalPipe )
		{
			m_pCurrentGoalPipe->Reset();
//			m_vActiveGoals.resize( goalsCount );
			m_pCurrentGoalPipe->Serialize( ser, objectTracker, m_vActiveGoals );
		}

		m_notAllowedSubpipes.clear();
	}
	else
	{
		if (m_pCurrentGoalPipe)
		{
			ser.Value( "PipeName", m_pCurrentGoalPipe->m_sName);
			ser.BeginGroup( "DynamicPipe" );
				m_pCurrentGoalPipe->SerializeDynamic( ser );
			ser.EndGroup();

			m_pCurrentGoalPipe->Serialize( ser, objectTracker, m_vActiveGoals );
		}
		else
		{
			ser.Value( "PipeName", string("none") );
		}
	}

	// this stuff can get reset when selecting pipe - so, serialize it after
	ser.Value("Blocked", m_bBlocked);
	ser.Value("KeepMoving", m_bKeepMoving);
	ser.Value("LastMoveDir", m_vLastMoveDir);

	ser.Value("m_PathDestinationPos", m_PathDestinationPos);

	m_Path.Serialize(ser, objectTracker);
	m_OrigPath.Serialize(ser, objectTracker);
  if(ser.IsWriting())
  {
    if(ser.BeginOptionalGroup("m_pPathFollower", m_pPathFollower!=NULL))
    {
      m_pPathFollower->Serialize(ser, objectTracker, &m_Path);
      ser.EndGroup();
    }
  }
  else
  {
    SAFE_DELETE(m_pPathFollower);
    if(ser.BeginOptionalGroup("m_pPathFollower", true))
    {
      m_pPathFollower = new CPathFollower();
      m_pPathFollower->Serialize(ser, objectTracker, &m_Path);
      ser.EndGroup();
    }
  }
	ser.Value( "m_posLookAtSmartObject", m_posLookAtSmartObject );
	ser.Value("m_forcePathGenerationFrom", m_forcePathGenerationFrom );
	
	ser.BeginGroup("UnreachableHideObjectList");
	{
		int count  = m_recentUnreachableHideObjects.size();
		ser.Value("UnreachableHideObjectList_size",count);
		if(ser.IsReading())
			m_recentUnreachableHideObjects.clear();
		TimeOutVec3List::iterator it = m_recentUnreachableHideObjects.begin();
		float time(0);
		Vec3 point(0);
		for(int i=0;i<count;i++)
		{
			char name[32];
			if(ser.IsWriting())
			{
				time = it->first;
				point = it->second;
				++it;
			}
			sprintf(name,"time_%d",i);
			ser.Value(name,time);
			sprintf(name,"point_%d",i);
			ser.Value(name,point);
			if(ser.IsReading())
				m_recentUnreachableHideObjects.push_back(std::make_pair(time,point));
		}
		ser.EndGroup();
	}

	ser.EnumValue( "m_eNavSOMethod", m_eNavSOMethod, nSOmNone, nSOmLast );
	ser.Value( "m_navSOEarlyPathRegen", m_navSOEarlyPathRegen );
	ser.Value( "m_idLastUsedSmartObject", m_idLastUsedSmartObject );

	m_currentNavSOStates.Serialize(ser, objectTracker);
	m_pendingNavSOStates.Serialize(ser, objectTracker);

	ser.Value( "m_actorTargetReqId", m_actorTargetReqId);

	ser.Value( "m_refShapeName", m_refShapeName);
	if(ser.IsReading())
		m_refShape = GetAISystem()->GetGenericShapeOfName(m_refShapeName.c_str());

	if(ser.IsWriting())
	{
		if(ser.BeginOptionalGroup("m_pActorTargetRequest", m_pActorTargetRequest != 0))
		{
			m_pActorTargetRequest->Serialize(ser, objectTracker);
			ser.EndGroup();
		}
	}
	else
	{
		if(ser.BeginOptionalGroup("m_pActorTargetRequest", true))
		{
			if (!m_pActorTargetRequest)
				m_pActorTargetRequest = new SAIActorTargetRequest;
			m_pActorTargetRequest->Serialize(ser, objectTracker);
			ser.EndGroup();
		}
	}
}

void CPipeUser::SerializeActiveGoals(TSerialize ser, class CObjectTracker& objectTracker)
{
	//
	ser.BeginGroup("ActiveGoals");
		int goalsCount = m_vActiveGoals.size();
		ser.Value("GoalsCount", goalsCount);

		if(ser.IsWriting())
		{
			for (size_t i = 0; i < goalsCount; ++i)
			{
				QGoal& goal = m_vActiveGoals[i];
				string goalName = (goal.op == LAST_AIOP)? goal.pipeName : string(m_pCurrentGoalPipe->GetGoalOpName(goal.op));
				objectTracker.AddObject((CGoalOp*)goal.pGoalOp, goalName, false);
			}
		}
		else
		{
			ClearActiveGoals();
			m_vActiveGoals.resize( goalsCount );
		}
		char groupName[256];
		for (size_t i = 0; i < goalsCount; ++i)
		{
			sprintf(groupName, "ActiveGoal-%d", i);
			QGoal& goal = m_vActiveGoals[i];
      CGoalOp *pGoal = goal.pGoalOp;
			ser.BeginGroup(groupName);
				ser.EnumValue("op", goal.op, AIOP_ACQUIRETARGET, LAST_AIOP);
				ser.Value("pipeName", goal.pipeName);
				ser.Value("Blocking", goal.bBlocking);
				ser.EnumValue("Grouping",goal.eGrouping,IGoalPipe::GT_NOGROUP,IGoalPipe::GT_LAST);
//				ser.Value("GroupPreviouse", goal.bGroupWithPrevious);
				objectTracker.SerializeObjectPointer(ser, "GoalPointer", pGoal, false);
				goal.pGoalOp->Serialize(ser, objectTracker);
			ser.EndGroup();
		}
	ser.EndGroup();
	if(ser.IsReading())
	{
		m_listWaitGoalOps.clear();
		VectorOGoals::iterator itS = m_vActiveGoals.begin(), itEnd = m_vActiveGoals.end();
		for(;itS!=itEnd;++itS)
		{
			const QGoal& goal = *itS;
			if(goal.op == AIOP_WAITSIGNAL)
				m_listWaitGoalOps.push_back((COPWaitSignal*)(CGoalOp*)goal.pGoalOp);
		}
	}
}

bool CPipeUser::SetCharacter(const char *character, const char* behaviour)
{
	return GetProxy()->SetCharacter( character, behaviour );
}

void CPipeUser::NotifyListeners( CGoalPipe* pPipe, EGoalPipeEvent event, bool includeSubPipes )
{
	if ( pPipe && pPipe->m_nEventId )
		NotifyListeners( pPipe->m_nEventId, event );
	if(includeSubPipes && pPipe)
		NotifyListeners(pPipe->GetSubpipe(), event, true);
}

void CPipeUser::NotifyListeners( int goalPipeId, EGoalPipeEvent event )
{
	if ( !goalPipeId )
		return;

	// recursion?
	if ( bNotifyListenersLock )
		return;

	bNotifyListenersLock = true;

	TMapGoalPipeListeners::iterator it, itEnd = m_mapGoalPipeListeners.end();
	for ( it = m_mapGoalPipeListeners.find( goalPipeId ); it != itEnd && it->first == goalPipeId; ++it )
	{
		std::pair< IGoalPipeListener*, const char* > & second = it->second;
		second.first->OnGoalPipeEvent( this, event, goalPipeId );
	}

	bNotifyListenersLock = false;
}

void CPipeUser::RegisterGoalPipeListener( IGoalPipeListener* pListener, int goalPipeId, const char* debugClassName )
{
	// you can't add listeners while notifying them
	AIAssert( !bNotifyListenersLock );

	// zero means don't listen it
	AIAssert( goalPipeId );

	m_mapGoalPipeListeners.insert(std::make_pair( goalPipeId, std::make_pair(pListener, debugClassName) ));

	pListener->_vector_registered_pipes.push_back( std::pair< IPipeUser*, int >( this, goalPipeId ) );
}

void CPipeUser::UnRegisterGoalPipeListener( IGoalPipeListener* pListener, int goalPipeId )
{
	// you can't remove listeners while notifying them
	AIAssert( !bNotifyListenersLock );

	IGoalPipeListener::VectorRegisteredPipes::iterator itFind = std::find(
		pListener->_vector_registered_pipes.begin(),
		pListener->_vector_registered_pipes.end(),
		std::pair< IPipeUser*, int >( this, goalPipeId ) );
	if ( itFind != pListener->_vector_registered_pipes.end() )
		pListener->_vector_registered_pipes.erase( itFind );

	TMapGoalPipeListeners::iterator it, itEnd = m_mapGoalPipeListeners.end();
	for ( it = m_mapGoalPipeListeners.find( goalPipeId ); it != itEnd && it->first == goalPipeId; ++it )
	{
		if ( it->second.first == pListener )
		{
			m_mapGoalPipeListeners.erase( it );

			// listeners shouldn't register them twice!
#ifdef _DEBUG
			itEnd = m_mapGoalPipeListeners.end();
			for ( it = m_mapGoalPipeListeners.find( goalPipeId ); it != itEnd && it->first == goalPipeId; ++it )
				AIAssert( it->second.first != pListener );
#endif

			return;
		}
	}
	//assert( !"Trying to unregister not registered listener" );
}
/*
void CPipeUser::OnGoalPipeEvent( IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId )
{
	AIAssert ( this == pPipeUser );

	switch ( event )
	{
	case ePN_Deselected: // sent if replaced by selecting other pipe
	case ePN_Finished:   // sent if reached end of pipe
	case ePN_Removed:    // sent if inserted pipe was removed with RemovePipe()
		{
			AIAssert(m_pendingNavSOStates.IsEmpty() && m_currentNavSOStates.IsEmpty());
			m_eNavSOMethod = nSOmNone;
			m_navSOEarlyPathRegen = false;
		}
		break;

	case ePN_Suspended:  // sent if other pipe was inserted
	case ePN_Resumed:    // sent if resumed after finishing inserted pipes
		break;
	}
}
*/

//
//-------------------------------------------------------------------------------------------
bool CPipeUser::IsUsing3DNavigation()
{
	bool can3D = (m_movementAbility.pathfindingProperties.navCapMask & (IAISystem::NAV_FLIGHT | IAISystem::NAV_VOLUME | IAISystem::NAV_WAYPOINT_3DSURFACE)) != 0;
	bool can2D = (m_movementAbility.pathfindingProperties.navCapMask & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_FREE_2D | IAISystem::NAV_WAYPOINT_HUMAN)) != 0;

	if (can3D && !can2D)
		return true;
	if (can2D && !can3D)
		return false;

	bool use3D = false;
	if (!m_Path.Empty() && m_movementAbility.b3DMove)
	{
		const PathPointDescriptor *current = m_Path.GetPrevPathPoint();
		const PathPointDescriptor *next = m_Path.GetNextPathPoint();
		bool current3D = true;
		bool next3D = true;
		if (current)
		{
			m_CurrentNodeNavType = current->navType;
			if ((current->navType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_ROAD))
				|| current->navType == IAISystem::NAV_UNSET)
				current3D = false;
		}
		if (next)
		{
			if ((next->navType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_ROAD))
				|| next->navType == IAISystem::NAV_UNSET)
				next3D = false;
		}
		use3D = (current3D || next3D);
	}
	else 
	{
		if(m_CurrentNodeNavType==IAISystem::NAV_UNSET)
		{ 
			int nBuildingID;
			IVisArea *pArea;
			m_CurrentNodeNavType = GetAISystem()->CheckNavigationType(GetPhysicsPos(), nBuildingID, pArea, m_movementAbility.pathfindingProperties.navCapMask);
		}

		if (!((m_CurrentNodeNavType & (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_ROAD | IAISystem::NAV_FREE_2D))
			|| m_CurrentNodeNavType == IAISystem::NAV_UNSET))
			use3D = true;
	}
	return use3D;
}
//
//-------------------------------------------------------------------------------------------
void CPipeUser::DebugDrawGoals(IRenderer *pRenderer)
{
	// Debug draw all active goals.
	for (size_t i = 0; i < m_vActiveGoals.size(); i++)
	{
		if (m_vActiveGoals[i].pGoalOp)
			m_vActiveGoals[i].pGoalOp->DebugDraw(this, pRenderer);
	}
}

//====================================================================
// SetPathToFollow
//====================================================================
void CPipeUser::SetPathToFollow(const char *pathName)
{
  if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
    AILogAlways("CPipeUser::SetPathToFollow %s path = %s", GetName(), pathName);
	m_pathToFollowName = pathName;
	m_bPathToFollowIsSpline = false;
}

void CPipeUser::SetPathAttributeToFollow(bool bSpline)
{
	m_bPathToFollowIsSpline = bSpline;
}

//===================================================================
// GetPathEntryPoint
//===================================================================
bool CPipeUser::GetPathEntryPoint(Vec3& entryPos, bool reverse, bool startNearest) const
{
  if (GetAISystem()->m_cvDebugPathFinding->GetIVal())
    AILogAlways("CPipeUser::GetPathEntryPoint %s path = %s", GetName(), m_pathToFollowName.c_str());

	if (m_pathToFollowName.length() == 0)
		return false;

	ListPositions junk1;
	AABB junk2;
	SShape path1(junk1, junk2);
	if (!GetAISystem()->GetDesignerPath(m_pathToFollowName.c_str(), path1))
  {
    AIWarning("CPipeUser::GetPathEntryPoint %s unable to find path %s - check path is not marked as a road", GetName(), m_pathToFollowName.c_str());
		return false;
  }
	if (path1.shape.empty())
		return false;

	if (startNearest)
	{
		float	d;
		path1.NearestPointOnPath(GetPhysicsPos(), d, entryPos);
	}
	else if (reverse)
	{
		entryPos = *path1.shape.rbegin();
	}
	else
	{
		entryPos = *path1.shape.begin();
	}

	return true;
}

//====================================================================
// UsePathToFollow
//====================================================================
bool CPipeUser::UsePathToFollow(bool reverse, bool startNearest, bool loop)
{
  if (m_pathToFollowName.length() == 0)
    return false;
  ClearPath("CPipeUser::UsePathToFollow m_Path");
  ListPositions junk1;
  AABB junk2;
  SShape path1(junk1, junk2);
	if (!GetAISystem()->GetDesignerPath(m_pathToFollowName.c_str(), path1))
    return false;
  if (path1.shape.empty())
    return false;

	IAISystem::ENavigationType	navType(path1.navType);

	if(startNearest)
	{
		float	d;
		Vec3	nearestPoint;
		ListPositions::const_iterator nearest = path1.NearestPointOnPath(GetPhysicsPos(), d, nearestPoint);

		m_Path.PushBack(PathPointDescriptor(navType, nearestPoint));

		if(reverse)
		{
			ListPositions::const_reverse_iterator rnearest(nearest);
			ListPositions::const_reverse_iterator	rbegin(path1.shape.rbegin());
			ListPositions::const_reverse_iterator	rend(path1.shape.rend());
			for (ListPositions::const_reverse_iterator it = rnearest; it != rend; ++it)
			{
				const Vec3 &pos = *it;
				m_Path.PushBack(PathPointDescriptor(navType, pos));
			}
			if(loop)
			{
				for (ListPositions::const_reverse_iterator it = rbegin; it != rnearest; ++it)
				{
					const Vec3 &pos = *it;
					m_Path.PushBack(PathPointDescriptor(navType, pos));
				}
				m_Path.PushBack(PathPointDescriptor(navType, nearestPoint));
			}
		}
		else
		{
			for (ListPositions::const_iterator it = nearest; it != path1.shape.end(); ++it)
			{
				const Vec3 &pos = *it;
				m_Path.PushBack(PathPointDescriptor(navType, pos));
			}
			if(loop)
			{
				for (ListPositions::const_iterator it = path1.shape.begin(); it != nearest; ++it)
				{
					const Vec3 &pos = *it;
					m_Path.PushBack(PathPointDescriptor(navType, pos));
				}
				m_Path.PushBack(PathPointDescriptor(navType, nearestPoint));
			}
		}
	}
	else
	{
		if (reverse)
		{
			for (ListPositions::reverse_iterator it = path1.shape.rbegin() ; it != path1.shape.rend() ; ++it)
			{
				const Vec3 &pos = *it;
				m_Path.PushBack(PathPointDescriptor(navType, pos));
			}
		}
		else
		{
			for (ListPositions::const_iterator it = path1.shape.begin() ; it != path1.shape.end() ; ++it)
			{
				const Vec3 &pos = *it;
				m_Path.PushBack(PathPointDescriptor(navType, pos));
			}
		}
	}

	// convert ai path to be splined
	// this spline is not enough one and allows a possibility that a conversion fails
	// 29/10/2006 Tetsuji
	if ( m_bPathToFollowIsSpline )
	{
		ConvertPathToSpline( navType );
		return true;
	}

  SNavPathParams params;
  params.precalculatedPath = true;
  m_Path.SetParams(params);

  m_nPathDecision = PATHFINDER_PATHFOUND;
  m_Path.PushFront(PathPointDescriptor(navType, GetPhysicsPos()));
  m_OrigPath = m_Path;
  return true;
}

//===================================================================
// ConvertPathToSpline
//===================================================================
bool CPipeUser::ConvertPathToSpline( IAISystem::ENavigationType	navType )
{

	if ( m_Path.GetPath().empty() )
		return false;

	TPathPoints::const_iterator li,liend;
	li		= m_Path.GetPath().begin();
	liend	= m_Path.GetPath().end();

	std::list<Vec3> pointListLocal;

	while ( li != liend )
	{
		pointListLocal.push_back( li->vPos );
		++li;
	}

	m_Path.Clear("CPipeUser::ConvertPathToSpline");
	SetPointListToFollow( pointListLocal, navType, true );

	m_OrigPath.Clear("CPipeUser::ConvertPathBySpline");

	SNavPathParams params;
	params.precalculatedPath = true;
	m_Path.SetParams(params);

	m_nPathDecision = PATHFINDER_PATHFOUND;
	m_OrigPath = m_Path;

	return true;

}

//-------------------------------------------------------------------------------------------------

Vec3 CPipeUser::SetPointListToFollowSub( Vec3 a, Vec3 b, Vec3 c, std::list<Vec3>& newPointList, float step )
{
	float ideallength = 4.0f;

	if ( m_bPathToFollowIsSpline == true )
		ideallength = 1.0f;

	// calculate normalized spline(2) ( means each line has the same length )
	// x(1-t)^2 + 2yt(1-t)+ zt^2

	// when we suppose x=(0,0,0) then spline(2) is below
	// 2yt(1-t) +zt^2 =s
	// (z-2y)t^t + 2yt - s = 0;

	// to get t from a length
	// t = -2y +- sqrt( 4y^2 - 4(z-2y)s ) / 2(z-2y)

	// these 3 points (a,b,c) makes a spline(2)
	// if there is a problem, 
	// point a will be skipped then ( b, c, d ) will be calculated next time.

	Vec3 oa = (a + b) / 2.0f;
	Vec3 ob = b;
	Vec3 oc = (b + c) / 2.0f;

	Vec3 s = ob - oa;	// src
	Vec3 d = oc - oa;	// destination

	// when points are ideantical
	if ( s.GetLength() < 0.0001f )
		return b;

	// when (0,0,0),d and s make line, we just pass these point to the path system

	float	x,y,z,len;
	Vec3	nl =  s * ideallength / s.GetLength();

	for ( int i = 0; i< 3; i++ )
	{
		// select a stable element for the calculation.
		if (i==0)
			x =0.0f ,y = s.x, z = d.x, len =nl.x;
		if (i==1)
			x =0.0f ,y = s.y, z = d.y, len =nl.y;
		if (i==2)
			x =0.0f ,y = s.z, z = d.z, len =nl.z;
		
		// solve a formula for get t from length (ax*x + bx + c = 0)
		float ca = ( z - 2.0f * y );
		float cb = 2.0f * y;
		float cc = -len;

		float sq = cb * cb  - 4.0f * ca * cc ;
		if ( sq >= 0.0f && ca != 0.0f )
		{
			sq = sqrtf( sq );
			float t1 = ( -cb + sq ) / ( 2.0f * ca );
			float t2 = ( -cb - sq ) / ( 2.0f * ca );
			float t = ( t1 > 0.0f && t1 < step ) ? t1 : t2;

			if ( t > 0.0f && t < step )
			{
				for ( float u = 0.0f; u < step; u += t )
				{
					Vec3 np = 2.0f * u * ( 1.0f - u ) * s + u * u * d ;
					np += oa;
					newPointList.push_back( np );
				}
				if ( !newPointList.empty() )
				{
					Vec3 ret = newPointList.back();
					ret = ( ret  - c ) * 2.0f + c;
					newPointList.pop_back();
					return ret;
				}
			}
		}
	}
	newPointList.push_back( oa );
	return b;

}

//===================================================================
// SetPointListToFollow
//===================================================================
void CPipeUser::SetPointListToFollow( const std::list<Vec3>& pointList,IAISystem::ENavigationType navType,bool bSpline )
{
	ClearPath("CPipeUser::SetPointListToFollow m_Path");

	static bool debugsw = false;

	if ( debugsw == true )
	{
		for ( std::list<Vec3>::const_iterator it = pointList.begin(); it != pointList.end() ; ++it )
		{
			const Vec3 &pos = *it;
			m_Path.PushBack(PathPointDescriptor(navType, pos));
		}
		return;
	}

	// To guarantee at least 3 points for B-Spline(2)
	int	howmanyPoints = pointList.size();

	if ( howmanyPoints < 2)
		return;

	std::list<Vec3>::const_iterator itLocal;

	if ( howmanyPoints == 2 || bSpline == false )
	{
		for ( itLocal = pointList.begin(); itLocal != pointList.end() ; ++itLocal )
		{
			const Vec3 &pos = *itLocal;
			m_Path.PushBack(PathPointDescriptor(navType, pos));
		}
		return;
	}

	std::list<Vec3>::const_iterator itX = pointList.begin();
	std::list<Vec3>::const_iterator itY = itX;
	std::list<Vec3>::const_iterator itZ = itX;

	++itY;
	++itZ,++itZ;

	Vec3 nextStart = *itX;
	Vec3 mid = *itY;
	Vec3 end = *itZ;

	nextStart = ( nextStart - mid ) *2.0f + mid;

	std::list<Vec3> pointListLocal;

	// divde each line

	for ( int i = 0 ; i < howmanyPoints - 3 ; ++itX,++itY,++itZ,i++ )
	{
		pointListLocal.clear();
		nextStart = SetPointListToFollowSub( nextStart, *itY, *itZ, pointListLocal, 1.0f );

		for ( itLocal = pointListLocal.begin(); itLocal != pointListLocal.end() ; ++itLocal )
		{
			const Vec3 &pos = *itLocal;
			m_Path.PushBack(PathPointDescriptor(navType, pos));
		}
	}

	// at the last point, we need special treatment to make the curve reach the end point.
	mid = *itY;
	end = *itZ;
	end = ( end - mid ) * 2.0f + mid;

	pointListLocal.clear();
	nextStart = SetPointListToFollowSub( nextStart, *itY, end, pointListLocal, 1.0f );
	if ( pointListLocal.empty() )
	{
		const Vec3 &pos = nextStart;
		m_Path.PushBack(PathPointDescriptor(navType, pos));
	}
	else
	{
		for ( itLocal = pointListLocal.begin(); itLocal != pointListLocal.end() ; ++itLocal )
		{
			const Vec3 &pos = *itLocal;
			m_Path.PushBack(PathPointDescriptor(navType, pos));
		}
	}

	// terminate the end point ( skip if the final point of the spline is close to the end )
	{
		const Vec3 &pos = *itZ;
		if ( ( pos - nextStart ).GetLength() > 1.0f )
			m_Path.PushBack(PathPointDescriptor(navType, pos));
	}

	//m_Path.PushFront(PathPointDescriptor(navType, GetPhysicsPos()));
}

//===================================================================
// UsePointListToFollow
//===================================================================
bool CPipeUser::UsePointListToFollow( void )
{
	m_OrigPath.Clear("CPipeUser::UsePointListToFollow m_OrigPath");

	SNavPathParams params;
	params.precalculatedPath = true;
	m_Path.SetParams(params);

	m_nPathDecision = PATHFINDER_PATHFOUND;
	m_OrigPath = m_Path;

	return true;

}
//-------------------------------------------------------------------------------------------------
void CPipeUser::SetFireMode(EFireMode mode)
{
//	m_fireAtLastOpResult = shootLastOpResult;
	m_outOfAmmoSent = false;
	if(m_fireMode == mode)
		return;
	m_fireMode = mode;
	m_fireModeUpdated = true;
	m_spreadFireTime = ai_frand()*10.0f;
}

//-------------------------------------------------------------------------------------------------
void CPipeUser::SetFireTarget(CAIObject* pTargetObject)
{
	m_pFireTarget = pTargetObject;
}

//-------------------------------------------------------------------------------------------------
void CPipeUser::SetRefPointPos( const Vec3& pos )
{
	if(!m_pRefPoint)
	{
		AIError("CPipeUser::SetRefPointPos_A m_pRefPoint==0");
		return;
	}

	static bool bSetRefPointPosLock = false;
	bool bNotify = !bSetRefPointPosLock && m_pCurrentGoalPipe && !m_pRefPoint->GetPos().IsEquivalent( pos, 0.001f );
	m_pRefPoint->SetPos(pos, GetMoveDir());

	if ( GetAISystem()->IsRecording( this, IAIRecordable::E_REFPOINTPOS ) )
	{
		static char buffer[32];
		sprintf(buffer,"<%.0f %.0f %.0f>", pos.x, pos.y, pos.z);
		GetAISystem()->Record(this, IAIRecordable::E_REFPOINTPOS, buffer);
	}

	RecorderEventData recorderEventData(pos);
	RecordEvent(IAIRecordable::E_REFPOINTPOS, &recorderEventData);

	if ( bNotify )
	{
		bSetRefPointPosLock = true;
		NotifyListeners( m_pCurrentGoalPipe->GetLastSubpipe(), ePN_RefPointMoved );
		bSetRefPointPosLock = false;
	}
}

void CPipeUser::SetRefPointPos( const Vec3& pos, const Vec3& dir )
{
	if(!m_pRefPoint)
	{
		AIError("CPipeUser::SetRefPointPos_B m_pRefPoint==0");
		return;
	}

	static bool bSetRefPointPosDirLock = false;
	bool bNotify = !bSetRefPointPosDirLock && m_pCurrentGoalPipe && !m_pRefPoint->GetPos().IsEquivalent( pos, 0.001f );
	m_pRefPoint->SetPos( pos, dir );

	if ( GetAISystem()->IsRecording( this, IAIRecordable::E_REFPOINTPOS ) )
	{
		static char buffer[32];
		sprintf(buffer,"<%.0f %.0f %.0f>", pos.x, pos.y, pos.z);
		GetAISystem()->Record(this, IAIRecordable::E_REFPOINTPOS, buffer);
	}

	RecorderEventData recorderEventData(pos);
	RecordEvent(IAIRecordable::E_REFPOINTPOS, &recorderEventData);

	if ( bNotify )
	{
		bSetRefPointPosDirLock = true;
		NotifyListeners( m_pCurrentGoalPipe->GetLastSubpipe(), ePN_RefPointMoved );
		bSetRefPointPosDirLock = false;
	}
}

//
//---------------------------------------------------------------------------------------------------------------------------
void CPipeUser::SetCurrentHideObjectUnreachable()
{
	const Vec3&	pos(m_CurrentHideObject.GetObjectPos());

	// Check if the object is already in the list (should not).
	TimeOutVec3List::const_iterator end(m_recentUnreachableHideObjects.end());
	for(TimeOutVec3List::const_iterator it = m_recentUnreachableHideObjects.begin(); it != end; ++it)
	{
		if(Distance::Point_PointSq(it->second, pos) < sqr(0.1f))
			return;
	}

	// Add the object pos to the list.
	m_recentUnreachableHideObjects.push_back(FloatVecPair(20.0f, pos));

	if(m_recentUnreachableHideObjects.size() > 5)
		m_recentUnreachableHideObjects.pop_front();
}

//
//---------------------------------------------------------------------------------------------------------------------------
bool CPipeUser::WasHideObjectRecentlyUnreachable(const Vec3& pos) const
{
	TimeOutVec3List::const_iterator end(m_recentUnreachableHideObjects.end());
	for(TimeOutVec3List::const_iterator it = m_recentUnreachableHideObjects.begin(); it != end; ++it)
	{
		if(Distance::Point_PointSq(it->second, pos) < sqr(0.1f))
			return true;
	}
	return false;
}


//
//---------------------------------------------------------------------------------------------------------------------------
int CPipeUser::SetLooseAttentionTarget(CAIObject* pObject,int id)
{	
	if(pObject || (id == m_looseAttentionId || id==-1))
	{
		m_bLooseAttention = pObject!=NULL;
		if(m_bLooseAttention)
		{
			m_pLooseAttentionTarget = pObject;
			++m_looseAttentionId ;
		}
		else 
			m_pLooseAttentionTarget = NULL;
	}
	return m_looseAttentionId;
}

//
//---------------------------------------------------------------------------------------------------------------------------
int	CPipeUser::CountGroupedActiveGoals()
{
int count(0);

	for (size_t i = 0; i < m_vActiveGoals.size(); i++)
	{
		QGoal& goal = m_vActiveGoals[i];
		if (goal.eGrouping == IGoalPipe::GT_GROUPED && goal.op != AIOP_WAIT)
		 ++count;
	}
	return count;
}

//
//---------------------------------------------------------------------------------------------------------------------------
void	CPipeUser::ClearGroupedActiveGoals()
{
	for (size_t i = 0; i < m_vActiveGoals.size(); i++)
	{
		QGoal& goal = m_vActiveGoals[i];
		if (goal.eGrouping == IGoalPipe::GT_GROUPED && goal.op != AIOP_WAIT)
		{
			RemoveActiveGoal(i);
			if (!m_vActiveGoals.empty())
				--i;
		}
	}
}


//
//----------------------------------------------------------------------------------------------
void CPipeUser::SetRefShapeName(const char* shapeName)
{
	// Set and resolve the shape name.
	m_refShapeName = shapeName;
	m_refShape = GetAISystem()->GetGenericShapeOfName(m_refShapeName.c_str());
	if(!m_refShape)
		m_refShapeName.clear();
}

//
//----------------------------------------------------------------------------------------------
const char* CPipeUser::GetRefShapeName() const
{
	return m_refShapeName.c_str();
}

//
//----------------------------------------------------------------------------------------------
SShape*	CPipeUser::GetRefShape()
{
	return m_refShape;
}

//
//----------------------------------------------------------------------------------------------
void	CPipeUser::RecordSnapshot()
{
	// Currently not used
}

//
//----------------------------------------------------------------------------------------------
void	CPipeUser::RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData)
{
	if(m_pMyRecord!=NULL)
	{
		m_pMyRecord->RecordEvent(event, pEventData);
	}
}

//
//----------------------------------------------------------------------------------------------
IAIDebugRecord*	CPipeUser::GetAIDebugRecord()
{
	if(m_pMyRecord==NULL) 
		m_pMyRecord = s_pRecorder->AddUnit(GetEntityID());
	return m_pMyRecord;
}

//===================================================================
// GetPathFollower
//===================================================================
CPathFollower *CPipeUser::GetPathFollower()
{
  // only create this if appropriate (human - set by config etc)
  if (!m_pPathFollower && m_movementAbility.usePredictiveFollowing)
  {
    // various things get over-ridden each update
    m_pPathFollower = new CPathFollower(CPathFollower::SPathFollowerParams(
      0.0f, m_movementAbility.pathRadius, m_movementAbility.pathLookAhead, 
      0.0f, 0.0f, 0.0f, 0.0f, true, !m_movementAbility.b3DMove));
    m_pPathFollower->AttachToPath(&m_Path);
  }
  return m_pPathFollower;
}

//
//----------------------------------------------------------------------------------------------
EAimState CPipeUser::GetAimState() const
{
	return m_aimState;
}

void CPipeUser::ClearInvalidatedSOLinks()
{
	m_invalidatedSOLinks.clear();
}

void CPipeUser::InvalidateSOLink( CSmartObject* pObject, SmartObjectHelper* pFromHelper, SmartObjectHelper* pToHelper ) const
{
	m_invalidatedSOLinks.insert(std::make_pair( pObject, std::make_pair(pFromHelper, pToHelper) ));
}

bool CPipeUser::IsSOLinkInvalidated( CSmartObject* pObject, SmartObjectHelper* pFromHelper, SmartObjectHelper* pToHelper ) const
{
	return m_invalidatedSOLinks.find(std::make_pair( pObject, std::make_pair(pFromHelper, pToHelper) )) != m_invalidatedSOLinks.end();
}

void CPipeUser::SetActorTargetRequest(const SAIActorTargetRequest& req)
{
	CAIObject*	pTarget = GetOrCreateSpecialAIObject(AISPECIAL_ANIM_TARGET);

	if(!m_pActorTargetRequest)
		m_pActorTargetRequest = new SAIActorTargetRequest;
	*m_pActorTargetRequest = req;
	pTarget->SetPos(m_pActorTargetRequest->approachLocation, m_pActorTargetRequest->approachDirection);
}

const SAIActorTargetRequest* CPipeUser::GetActiveActorTargetRequest() const
{
	if(m_pPathFindTarget && m_pPathFindTarget->GetSubType() == STP_ANIM_TARGET && m_pActorTargetRequest)
		return m_pActorTargetRequest;
	return 0;
}

void CPipeUser::ClearActorTargetRequest()
{
	SAFE_DELETE(m_pActorTargetRequest);
}

