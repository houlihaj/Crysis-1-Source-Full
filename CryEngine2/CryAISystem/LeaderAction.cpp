/********************************************************************
Crytek Source File. 
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   LeaderAction.cpp

Description: Class Implementation for CLeaderAction (and inherited)

-------------------------------------------------------------------------
History:
- 15:11:2004   17:46 : Created by Luciano Morpurgo
- 2:6:2005   15:23 : Kirill Bulatsev - serialization support added, related refactoring/cleanup


*********************************************************************/




#include "StdAfx.h"
#include "LeaderAction.h"
#include "AILog.h"
#include "GoalOp.h"
#include "Puppet.h"
#include "Leader.h"
#include "AIPlayer.h"
#include "CAISystem.h"
#include <set>
#include <algorithm>
#include <utility>
#include <IEntitySystem.h>
#include "ISystem.h"
#include "ITimer.h"
#include <IConsole.h>
#include <ISerialize.h>
#include "ObjectTracker.h"
#include "AICollision.h"

//const float CLeaderAction_Follow::m_CMaxTooDistantTime = 0.5f;
//const float CLeaderAction_Follow::m_CMaxDist = 3.0f;
//const float CLeaderAction_FollowHiding::m_CSearchDistance = 5.f;
//const float CLeaderAction_FollowHiding::m_CUpdateThreshold = 3.f;

const float CLeaderAction_Attack::m_CSearchDistance = 30.f;

const float CLeaderAction_Follow::m_CUpdateDistance = 4.f;
const float CLeaderAction_Attack_FollowLeader::m_CSearchDistance = 2.f;
const float CLeaderAction_Attack_FollowLeader::m_CUpdateDistance = 4.f;
const float CLeaderAction_Attack_LeapFrog::m_CApproachInitDistance = 20;
const float CLeaderAction_Attack_LeapFrog::m_CApproachStep = 5;
const float CLeaderAction_Attack_LeapFrog::m_CApproachMinDistance = 3;
const float CLeaderAction_Attack_LeapFrog::m_CMaxDistance = 20;
const float CLeaderAction_Attack_HideAndCover::m_CSearchDistance = 5;
const float CLeaderAction_Attack_HideAndCover::m_CApproachMinDistance = 7;
const float CLeaderAction_Attack_Row::m_CInitDistance = 12.f;
const float CLeaderAction_Attack_Row::m_CMaxDefenseDistance = 15;
const float CLeaderAction_Attack_Row::m_CCoordinatedFireDelayStep = 0.3f;
const float CLeaderAction_Search::m_CSearchDistance = 15.f;
const uint32 CLeaderAction_Search::m_CCoverUnitProperties = UPR_COMBAT_GROUND;
const float CLeaderAction_Attack_Flank::m_CSearchDistance = 3.f;
const float CLeaderAction_Attack_Chain::m_CDistanceStep = 2;
const float CLeaderAction_Attack_Chain::m_CDistanceHead = 3;
const float CLeaderAction_Attack_UseSpots::m_CApproachDistance = 4;
const float CLeaderAction_Attack_SwitchPositions::m_fDefaultMinDistance = 6;

CLeaderAction::CLeaderAction(CLeader* pLeader)
{
	m_pLeader = pLeader;
	m_eActionType = LA_NONE;
	m_unitProperties = UPR_ALL;
}

CLeaderAction::~CLeaderAction()
{
	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg &curUnit = (*unitItr);
		curUnit.ClearPlanning(m_Priority);
		curUnit.ClearFlags();
	}
}

TUnitList& CLeaderAction::GetUnitsList() const
{ 
	return m_pLeader->GetAIGroup()->GetUnits(); 
}


void CLeaderAction::ClearUnitFlags()
{
	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg &curUnit = (*unitItr);
		curUnit.ClearFlags();
		curUnit.m_Group = 0;
	}
}


bool CLeaderAction::IsUnitAvailable(const CUnitImg& unit) const
{
	return (unit.GetProperties() & m_unitProperties) && unit.m_pUnit->IsEnabled();
}

void CLeaderAction::DeadUnitNotify(CAIActor* pUnit)
{
	// by default for all Leader actions, the dead unit's remaining actions are deleted 
	// and their blocked actions are unlocked (for each Unit Action's destructor)
	TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pUnit);
	if(itUnit!=m_pLeader->GetAIGroup()->GetUnits().end())
		itUnit->ClearPlanning();
	//	if(m_pLeader->GetAssociation()==pUnit)
	//		m_pLeader->SetAssociation(NULL);
}


void CLeaderAction::BusyUnitNotify(CUnitImg& BusyUnit)
{
	// by default for all Leader actions, the dead unit's remaining actions are deleted 
	// and their blocked actions are unlocked (for each Unit Action's destructor)
	//	BusyUnit.ClearPlanning();
}

int CLeaderAction::GetNumLiveUnits()  const
{
	// since dead unit were removed from this list just return its size
	return GetUnitsList().size();

	// old code made before adding remove-dead-units functionality
	/*
	int count = 0;
	TUnitList::const_iterator it, itEnd = GetUnitsList().end();
	for (it = GetUnitsList().begin(); it != itEnd; ++it)
	if (it->m_pUnit->IsEnabled())
	++count;
	return count;
	*/
}

CUnitImg* CLeaderAction::GetFormationOwnerImg() const
{
	// Return pointer to the unit image of the group formation owner.
	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg& unit = *unitItr;
		if(unit.m_pUnit == m_pLeader->GetFormationOwner())
			return &unit;
	}
	return 0;
}

void CLeaderAction::CheckNavType(CAIActor* pMember, bool bSignal)
{
	int building;
	IVisArea *pArea;
	IAISystem::ENavigationType navType = GetAISystem()->CheckNavigationType(pMember->GetPos(),building,pArea,m_NavProperties);
	if(navType != m_currentNavType && bSignal)
	{
		IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
		pData->iValue = navType;
		GetAISystem()->SendSignal(SIGNALFILTER_SENDER,1,"OnNavTypeChanged",pMember,pData);
	}
	m_currentNavType = navType;
}


void CLeaderAction::SetTeamNavProperties()
{
	m_NavProperties = 0;
	CAIObject* pFormatonOwner = m_pLeader->GetFormationOwner();
	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator it=GetUnitsList().begin(); it!=itEnd; ++it)
	{
		CUnitImg& unit = *it;
		CAIObject* pUnit = unit.m_pUnit;
		CAIActor* pAIActor = pUnit->CastToCAIActor();
		if(pAIActor && (unit.GetProperties() & m_unitProperties) && pUnit->IsEnabled() && pUnit != pFormatonOwner)
			m_NavProperties |= pAIActor->GetMovementAbility().pathfindingProperties.navCapMask;
	}
}

void CLeaderAction::UpdateBeacon() const
{
	if(!m_pLeader->GetAIGroup()->GetLiveEnemyAveragePosition().IsZero())
		GetAISystem()->UpdateBeacon(m_pLeader->GetGroupId(), m_pLeader->GetAIGroup()->GetLiveEnemyAveragePosition(), m_pLeader);
	else if(!m_pLeader->GetAIGroup()->GetEnemyAveragePosition().IsZero())
		GetAISystem()->UpdateBeacon(m_pLeader->GetGroupId(), m_pLeader->GetAIGroup()->GetEnemyAveragePosition(), m_pLeader);
	Vec3 vEnemyDir = m_pLeader->GetAIGroup()->GetEnemyAverageDirection(true,false);
	if(!vEnemyDir.IsZero())
	{
		IAIObject *pBeacon = GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
		if (pBeacon)
		{
			pBeacon->SetBodyDir(vEnemyDir);
			pBeacon->SetMoveDir(vEnemyDir);
		}
		else
		{
			AIError("Unable to get beacon for group id %d", m_pLeader->GetGroupId());
		}
	}
}

void CLeaderAction::CheckLeaderDistance() const
{
	// Check if some unit is too back 
	CAIObject* pAILeader = (CAIObject*)(m_pLeader->GetAssociation());
	if(pAILeader)
	{
		TUnitList::iterator itEnd = GetUnitsList().end();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CAIActor* pUnitActor = CastToCAIActorSafe((*unitItr).m_pUnit);
			if(pUnitActor && pUnitActor->IsEnabled())
			{
				static const float maxdist2 = 18*18;
				float dist2 = Distance::Point_PointSq(pUnitActor->GetPos(),pAILeader->GetPos());
				if(dist2 > maxdist2 && !unitItr->IsFar())
				{
					pUnitActor->SetSignal(1,"OnLeaderTooFar",(IEntity*)(pAILeader->GetAssociation()), 0, g_crcSignals.m_nOnLeaderTooFar);
					unitItr->SetFar();
				}
				else
					unitItr->ClearFar();
			}
		}
	}
}
//-----------------------------------------------------------
//----  SEARCH
//-----------------------------------------------------------



CLeaderAction_Search::CLeaderAction_Search(CLeader* pLeader,const  LeaderActionParams& params)
: CLeaderAction(pLeader)
, m_timeRunning(0.0f)
{
	//if (GetNumLiveUnits() < 1)
	//	return;

	m_eActionType = LA_SEARCH;
	m_iSearchSpotAIObjectType = params.iValue ? params.iValue : AIANCHOR_COMBAT_HIDESPOT;
	m_bUseHideSpots = (m_iSearchSpotAIObjectType & AI_USE_HIDESPOTS)!=0;
	m_iSearchSpotAIObjectType &= ~AI_USE_HIDESPOTS;

	m_unitProperties = params.unitProperties;
	m_fSearchDistance = params.fSize>0 ? params.fSize: m_CSearchDistance;

	// TO DO : use customized m_fSearchDistance (now there's a 15 hardcoded in GetEnclosing()


	Vec3 initPos = params.vPoint;
	
	if(params.subType == LAS_SEARCH_COVER)
	{
		int numFiringUnits = m_pLeader->GetAIGroup()->GetUnitCount(m_CCoverUnitProperties);
		m_iCoverUnitsLeft = numFiringUnits*2/5;
		if(m_iCoverUnitsLeft==0 && numFiringUnits>0)
			m_iCoverUnitsLeft=1;
	}
	else
		m_iCoverUnitsLeft = 0;

	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		unitItr->ClearHiding();

	m_vEnemyPos = ZERO;
	m_bInitialized = false;
	PopulateSearchSpotList(initPos);

}


void CLeaderAction_Search::PopulateSearchSpotList(Vec3& initPos)
{
	float maxdist2 = m_fSearchDistance*m_fSearchDistance;
	Vec3 avgPos = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,m_unitProperties);
	if(initPos.IsZero())
		initPos = m_pLeader->GetMarkedPosition();
	if(initPos.IsZero())
		initPos = avgPos;
	
	Vec3 currentEnemyPos( m_pLeader->GetAIGroup()->GetEnemyAveragePosition());

	if(!currentEnemyPos.IsZero())
		m_vEnemyPos = currentEnemyPos;
	else
		m_vEnemyPos = (initPos);// + (m_vEnemyPos.IsZero()? avgPos : m_vEnemyPos))/2;
	if(m_vEnemyPos.IsZero())
	{
		IAIObject* pBeacon = GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
		if(pBeacon)
			m_vEnemyPos = pBeacon->GetPos();
		if(m_vEnemyPos.IsZero())
			return;
	}

	bool bCleared = false;
	if(m_iSearchSpotAIObjectType>0)
	{
		for(AutoAIObjectIter it(GetAISystem()->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, m_iSearchSpotAIObjectType)); it->GetObject(); it->Next())
		{
			IAIObject*	pObjectIt = it->GetObject();
			const Vec3&	objPos = pObjectIt->GetPos();
			const Vec3&	objDir = pObjectIt->GetViewDir();
			if(!pObjectIt->IsEnabled()) // || pPipeUser->IsDevalued(pObjectIt))
				continue;
			float dist2 = Distance::Point_PointSq(m_vEnemyPos, objPos);
			if( dist2 < maxdist2)
			{
				if(!bCleared)
				{ // clear the list only if some new spots are actually found around new init position
					m_HideSpots.clear();
					bCleared=true;
				}
				m_HideSpots.insert(std::make_pair(dist2,SSearchPoint(objPos,objDir)));
			}
		}
	}

	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator it = GetUnitsList().begin(); it != itEnd ; ++it)
	{
		CUnitImg& unit = *it;
		unit.m_TagPoint = ZERO;
		CPipeUser* pPiper = CastToCPipeUserSafe(unit.m_pUnit);
		if(pPiper)
		{
			CAIObject* pTarget = (CAIObject*)pPiper->GetAttentionTarget();
			if(pTarget && pTarget->GetAIType()==AIOBJECT_DUMMY && 
				(pTarget->GetSubType()==IAIObject::STP_MEMORY || pTarget->GetSubType()==IAIObject::STP_SOUND))
			{
				bool bAlreadyChecked = false;
				Vec3 memoryPos(pTarget->GetPos());
				// check if some other guy is not already checking a nearby point
				TPointMap::iterator it1 = m_HideSpots.begin(),itend1 = m_HideSpots.end();
				for (; it1 != itend1 ; ++it1)
				{
					
					if(Distance::Point_Point2DSq(memoryPos, it1->second.pos) <1)
					{
						bAlreadyChecked = true;
						break;
					}
				}
				if(!bAlreadyChecked)
				{
					float dist2 = Distance::Point_PointSq(memoryPos,m_vEnemyPos);
					if(!bCleared)
					{ // clear the list only if some new spots are actually found around new init position
						m_HideSpots.clear();
						bCleared=true;
					}
					m_HideSpots.insert(std::make_pair(dist2,SSearchPoint(memoryPos,pTarget->GetViewDir())));
				}
			}
		}
	}
	
	if(m_bUseHideSpots)
	{
		TUnitList::iterator itu = GetUnitsList().begin(); 
		if(itu != itEnd)
		{
			CUnitImg& unit = *itu;
			const CAIActor* pActor = unit.m_pUnit;

			MultimapRangeHideSpots hidespots;
			MapConstNodesDistance traversedNodes;
			GetAISystem()->GetHideSpotsInRange(hidespots, traversedNodes, m_vEnemyPos, m_fSearchDistance, 
				pActor->m_movementAbility.pathfindingProperties.navCapMask, pActor->GetParameters().m_fPassRadius, false);
			
			MultimapRangeHideSpots::iterator it = hidespots.begin(), ithend = hidespots.end();
			for (; it!=ithend; ++it)
			{
				float distance = it->first;
				const SHideSpot &hs = it->second;
				Vec3 pos(hs.pos);
				if(hs.type == SHideSpot::HST_TRIANGULAR && hs.pObstacle)
				{
					Vec3 dir = (pos - m_vEnemyPos).GetNormalizedSafe();
					pos += dir*(hs.pObstacle->fApproxRadius + 0.5 + 2*pActor->m_movementAbility.pathRadius);
				}
				if(!bCleared)
				{ // clear the list only if some new spots are actually found around new init position
					m_HideSpots.clear();
					bCleared=true;
				}
				m_HideSpots.insert(std::make_pair(distance*distance,SSearchPoint(pos,-hs.dir,true)));
			}

		}
	}

	if( !m_vEnemyPos.IsZero())
	{
		// add at least one point
		Vec3 dir(m_vEnemyPos - avgPos);
		m_HideSpots.insert(std::make_pair(dir.GetLengthSquared(),SSearchPoint(m_vEnemyPos,dir.GetNormalizedSafe())));

	}

	// DEBUG
	if (GetAISystem()->m_cvDebugDraw->GetIVal()==1)
	{
		TPointMap::iterator it = m_HideSpots.begin(), ithend = m_HideSpots.end();
		for (; it!=ithend; ++it)
		{
			GetAISystem()->AddDebugSphere(it->second.pos+Vec3(0,0,1),0.3f,255,0,255,8);
			GetAISystem()->AddDebugLine(it->second.pos+Vec3(0,0,1),it->second.pos+Vec3(0,0,-3),255,0,255,8);
		}
		GetAISystem()->AddDebugSphere(m_vEnemyPos+Vec3(0,0,2),0.5f,255,128,255,8);
		GetAISystem()->AddDebugLine(m_vEnemyPos+Vec3(0,0,2),m_vEnemyPos+Vec3(0,0,-3),255,128,255,8);
	}
}

CLeaderAction_Search::~CLeaderAction_Search()
{
}


bool CLeaderAction_Search::ProcessSignal(const AISIGNAL& signal)
{
	IEntity* pEntity = signal.pSender;
	if(signal.Compare(g_crcSignals.m_nOnUnitMoving))
	{
		CAIActor* pUnit = CastToCAIActorSafe(pEntity->GetAI());
		if(pUnit)
		{
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pUnit);
			if(itUnit != GetUnitsList().end())
			{
				Vec3 hidepos(itUnit->m_TagPoint);
				TPointMap::iterator it = m_HideSpots.begin(),itend = m_HideSpots.end();
				for(; it!=itend; ++it)
					if(it->second.pos == hidepos)
					{
						m_HideSpots.erase(it);
						itUnit->SetMoving();
						break;
					}
			}

		}
		return true;
	}
	else if(signal.Compare(g_crcSignals.m_nOnUnitStop))
	{
		// unit can't reach an assigned hidespot
		CAIActor* pUnit = CastToCAIActorSafe(pEntity->GetAI());
		if(pUnit)
		{
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pUnit);
			if(itUnit != GetUnitsList().end())
			{
				Vec3 hidepos(itUnit->m_TagPoint);
				TPointMap::iterator it = m_HideSpots.begin(),itend = m_HideSpots.end();
				for(; it!=itend; ++it)
					if(it->second.pos == hidepos)
					{
						//it->second.bReserved = false;
						m_HideSpots.erase(it);// assume that no other unit can reach it, just remove it
						//itUnit->m_TagPoint = hidepos;
						break;
					}
				itUnit->ClearPlanning();
			}

		}
		return true;
	}
	else if(signal.Compare(g_crcSignals.m_nOnUnitDamaged) || signal.Compare(g_crcSignals.m_nAIORD_SEARCH))
	{
		CAIActor* pUnit = CastToCAIActorSafe(pEntity->GetAI());
		if(pUnit)
		{
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pUnit);
			if(itUnit != GetUnitsList().end())
			{
				Vec3 hidepos(itUnit->m_TagPoint);
				TPointMap::iterator it = m_HideSpots.begin(),itend = m_HideSpots.end();
				for(; it!=itend; ++it)
					if(it->second.pos == hidepos)
					{
						it->second.bReserved = false;
						//itUnit->m_TagPoint = hidepos;
						break;
					}
				//create new spot
				if(signal.pEData)
				{
/*					// approach to the source of damage
					itUnit->ClearPlanning();
					m_vEnemyPos = (m_vEnemyPos + signal.pEData->point)/2;
					Vec3 dir(m_vEnemyPos - pUnit->GetPos());
					m_HideSpots.insert(std::make_pair(dir.GetLengthSquared(),SSearchPoint(m_vEnemyPos,dir.GetNormalizedSafe())));
					*/
					m_pLeader->ClearAllPlannings();
					PopulateSearchSpotList(signal.pEData->point);
				}
			}

		}
		return true;
	}
	return false;
}

CLeaderAction::eActionUpdateResult CLeaderAction_Search::Update()
{
	FRAME_PROFILER( "CLeaderAction_Attack::CLeaderAction_Search",GetISystem(),PROFILE_AI );

	if(m_HideSpots.empty())
	{
		bool bAllunitFinished = true;
		TUnitList::iterator itEnd = GetUnitsList().end();
		for(TUnitList::iterator unitItr =GetUnitsList().begin(); unitItr != itEnd; ++unitItr)
		{
			CUnitImg& curUnit = (*unitItr);
			if (!curUnit.IsPlanFinished())
			{
				bAllunitFinished = false;
				break;
			}
		}
		if(bAllunitFinished)
			return ACTION_DONE;
	}

	m_timeRunning = GetAISystem()->GetFrameStartTime();

	bool busy = false;

	TUnitList::iterator it, itEnd = GetUnitsList().end();
	for (it = GetUnitsList().begin(); it != itEnd ; ++it)
	{
		CUnitImg& unit = *it;
		bool bSearchingMemoryTarget = false;
		if (unit.m_pUnit->IsEnabled() &&( unit.IsPlanFinished() || !m_bInitialized))
		{
			// find cover fire units first
 			if( m_iCoverUnitsLeft>0 && (unit.GetProperties() & m_CCoverUnitProperties))
			{
				AISignalExtraData data;
				data.point = m_vEnemyPos;
				data.point2 = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,m_unitProperties);
				CUnitAction* pAction = new CUnitAction(UA_SIGNAL,true,"ORDER_COVER_SEARCH",data);
				unit.m_Plan.push_back(pAction);
				m_iCoverUnitsLeft--;
				busy = true;
			}
			else // search units
			{
				Vec3 unitPos(unit.m_pUnit->GetPos());
				TPointMap::iterator oit= m_HideSpots.begin(), oend = m_HideSpots.end();
				float maxdist2   = 100000;
				Vec3 obstaclePos(ZERO);
				TPointMap::iterator itFound = oend;
				
				//if(!unit.IsMoving())//unit hasn't searched any spot yet
				{

					for(; oit!=oend; ++oit )
					{
						SSearchPoint& hp = oit->second;
						if(!hp.bReserved && unit.m_TagPoint != hp.pos)
						{
							itFound = oit; // get the first obstacles (closer to enemy position)
							break;
						}
					}
				}
				/*else
				{
					for(; oit!=oend; ++oit ) // search for the closest obstacle to the unit
					{
						SSearchPoint& hp = oit->second;
						if(!hp.bReserved && unit.m_TagPoint != hp.pos) // check also if unit hasn't already try to reach this point
						{
							float dist2 = Distance::Point_PointSq(hp.pos,unitPos);
							if(dist2 < maxdist2)
							{
								maxdist2 = dist2;
								itFound = oit;
							}
						}
					}
				}*/

				if(itFound!=oend)
				{
					//Vec3 searchPoint = m_pLeader->GetSearchPoint(unit.m_pUnit, *itFound);
					SSearchPoint& hp = itFound->second;
					CUnitAction* action = new CUnitAction(UA_SEARCH, true,hp.pos,hp.dir);
					action->m_Tag = hp.bHideSpot ? 1: 0;
					unit.m_Plan.push_back(action);
					unit.m_TagPoint = hp.pos;
					unit.ExecuteTask();
					busy = true;
					itFound->second.bReserved = true;
				}
			}
		}
		else
			busy = true;
	}

	m_bInitialized = true;

	return (busy ? ACTION_RUNNING : ACTION_DONE);
}



//-----------------------------------------------------------
//----  ATTACK
//-----------------------------------------------------------


CLeaderAction_Attack::CLeaderAction_Attack(CLeader* pLeader): CLeaderAction(pLeader),
m_timeRunning(0.0f),
m_timeLimit(40),
m_bApproachWithNoObstacle(false),
m_vDefensePoint(ZERO),
m_vEnemyPos(ZERO),
m_bNoTarget(false)
{

	m_eActionType = LA_ATTACK;
	m_eActionSubType = LAS_DEFAULT;
	m_bInitialized = false;

	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		unitItr->ClearHiding();

	FinalUpdate();
}

CLeaderAction_Attack::CLeaderAction_Attack()
{
	memset(this,0,sizeof(*this));
}

CLeaderAction_Attack::~CLeaderAction_Attack()
{
	GetAISystem()->SendSignal(SIGNALFILTER_GROUPONLY,1,"OnFireDisabled",m_pLeader);
}

bool CLeaderAction_Attack::TimeOut()
{
	CTimeValue frameTime = GetAISystem()->GetFrameStartTime(); 
	// check if no one has a target
	if(!SomeoneHasTarget())
	{
		if(m_pLeader->IsPlayer() )
		{
			if(!m_bNoTarget)
				m_timeLimit = (frameTime - m_timeRunning).GetSeconds()+5.0f;
			m_bNoTarget = true;
			return true; // end everything immediately if no squadmates have target 
		}	
		else
		{
			m_bNoTarget = false;
			m_timeLimit = 5;
		}
	}
	else
	{
		m_bNoTarget = false;
		m_timeLimit = 5;
	}
	if( (frameTime - m_timeRunning).GetSeconds() > m_timeLimit)
	{
		TUnitList::iterator itEnd = GetUnitsList().end();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg &curUnit = (*unitItr);
			//if(curUnit.GetClass()!=UNIT_CLASS_CIVILIAN)
			curUnit.ClearPlanning(PRIORITY_NORMAL);
		}

		//m_timeLimit = 15.0f;
		//CPuppet* pLeaderPuppet = (CPuppet*)m_pLeader->GetAssociation();
		//AIWarning("LeaderAction_Attack::TimeOut() returns true!!!\n    Leader name: %s", pLeaderPuppet->GetName());

		return true;
	}
	return false;
}

bool CLeaderAction_Attack::FinalUpdate()
{
	FRAME_PROFILER( "CLeaderAction_Attack::FinalUpdate",GetISystem(),PROFILE_AI );

	m_timeRunning = GetAISystem()->GetFrameStartTime();

	bool bSquadmates = m_pLeader->IsPlayer();
	if (GetNumLiveUnits() <= (bSquadmates ? 0 : 1))
		return false;

	if(!bSquadmates && !SomeoneHasTarget())
		return false;

	CPuppet* pLeaderPuppet = CastToCPuppetSafe(m_pLeader->GetAssociation());
	Vec3 enemyPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();
	if(pLeaderPuppet && Distance::Point_Point(m_vEnemyPos,enemyPos) <5) 
		pLeaderPuppet->SetSignal(1,"OnEnemySteady", 0, 0, g_crcSignals.m_nOnEnemySteady);

	m_vEnemyPos = enemyPos;

	if (bSquadmates && m_pLeader->GetFollowState() == FS_FOLLOW)
	{
		// check if the squad-mates are too far (>25m) and cancel combat to allow them catch the player
		Vec3 pos;
		if (pLeaderPuppet)
			pos = pLeaderPuppet->GetPos();

		bool everyoneIsTooFar = true;
		TUnitList::iterator it, itEnd = GetUnitsList().end();
		for (it = GetUnitsList().begin(); everyoneIsTooFar && it != itEnd; ++it)
		{
			CUnitImg& unit = *it;

			if(! (unit.GetProperties() &UPR_COMBAT_GROUND))
				continue;

			if (it->GetClass()==UNIT_CLASS_COMPANION) 
			{
				// Companion characters use a different logic. 
				everyoneIsTooFar &= IsUnitTooFar(unit,pos);
			}
			else
				if(it->GetClass()!=UNIT_CLASS_CIVILIAN) //civilian can stay far
					everyoneIsTooFar &= !unit.m_pUnit->IsEnabled() || (unit.m_pUnit->GetPos() - pos).GetLengthSquared() > 25.0f * 25.0f;			
		}
		if (everyoneIsTooFar)
		{
			GetAISystem()->SendSignal(SIGNALFILTER_NEARESTGROUP, 10, "HANG_ON", pLeaderPuppet);
			return false;
		}
	}

	TUnitList::iterator it, itEnd = GetUnitsList().end();
	//check if there is some unit doing some high priority action, don't update
	//it's kind of a CXP hack, for rpg usage while combat - To be refined
	for (it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CUnitImg& unit = *it;
		if (unit.m_pUnit->IsEnabled() && unit.GetCurrentAction() && unit.GetCurrentAction()->m_Priority > GetPriority())
			return true;
	}

	// is leader still alive?
	//	if (GetAISystem()->m_cvAILeaderDies->GetIVal())
	{
		if (!bSquadmates && pLeaderPuppet && !pLeaderPuppet->IsEnabled())
			return false; //end this action if he's dead
	}

	bool bFollowPlayer = bSquadmates && m_pLeader->GetFollowState() == FS_FOLLOW;
	bool bAllowFire = m_pLeader->GetAllowFire();
	if (!bAllowFire)
		m_bInitialized = true;

	//	Vec3 enemyPos = m_pLeader->GetAIGroup()->GetEnemyPosition();
	//	Vec3 teamPos = m_pLeader->GetAIGroup()->GetAveragePosition();

	//	Vec3 enemyDir = enemyPos - teamPos;
	//	float enemyDistance = enemyDir.GetLengthSquared();

	typedef std::multimap< float, CUnitImg* > DistanceMap;
	DistanceMap distanceMap;

	m_pLeader->m_pObstacleAllocator->SetSearchDistance(m_CSearchDistance);

	bool someoneHasTarget=false;
	if(bSquadmates)
	{
		CAIPlayer* pPlayer = (CAIPlayer*)m_pLeader->GetAssociation();
		if(pPlayer)
		{
			CAIObject* pTarget = pPlayer->GetAttentionTarget();
			someoneHasTarget = pTarget && pTarget->IsEnabled();
		}
	}
	float fPreferredDistance = m_bApproachWithNoObstacle ? 0 : 25.f;

	CAIObject* pBeacon=static_cast<CAIObject*>(GetAISystem()->GetBeacon(m_pLeader->GetGroupId()));
	for (it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CUnitImg& unit = *it;
		CAIObject* pUnit = unit.m_pUnit;
		Vec3 vUnitEnemyPos = m_pLeader->GetAIGroup()->GetEnemyPosition(pUnit);

		if (pUnit->IsEnabled() && (unit.GetProperties() &UPR_COMBAT_GROUND))
		{
			// this unit is alive
			float distance;
			if (m_bInitialized)
			{
				if (bFollowPlayer)
					distance = - (GetAISystem()->GetPlayer()->GetPos() - pUnit->GetPos()).GetLength();
				else
					distance = - abs(fPreferredDistance - (vUnitEnemyPos - pUnit->GetPos()).GetLength());
			}
			else
			{
				distance = - (vUnitEnemyPos - pUnit->GetPos()).GetLength();
				if (!bSquadmates && !HasTarget(pUnit))
				{
					CPipeUser* pPipeUser = pUnit->CastToCPipeUser();
					if (pBeacon && pPipeUser)
						pPipeUser->SetAttentionTarget(pBeacon);
				}
			}

			//			distance += float(ai_rand() % 8);

			if (HasTarget(pUnit) && unit.GetClass()!=UNIT_CLASS_CIVILIAN && unit.GetClass()!=UNIT_CLASS_COMPANION)
				someoneHasTarget = true;
			else
				distance -= 1000.0f; // units without target - and civilians as well as companions - will change their positions first

			if (!m_bInitialized && pUnit == m_pLeader->GetPrevFormationOwner())
				distance -= 2000.0f; // force formation leader to change his positions first

			distanceMap.insert(std::make_pair(distance, &unit));
		}
	}

	if (!someoneHasTarget && bSquadmates)
	{ 
		bAllowFire = false;
		m_bInitialized = true;
	}

	ActionList fireList;
	ActionList hideList;
	ActionList holdFireList;
	ActionList fireList2;
	ActionList holdFireList2;
	ActionList timeoutList;

	// first half should change positions
	int halfUnits = distanceMap.size() / 2;
	if(bSquadmates) // consider the player in the "2nd half" (not hiding)
		halfUnits++;
	if (!halfUnits)
		halfUnits = 1;

	// as requested by Friedrich: only one unit can change cover
	//if (m_bInitialized && halfUnits > 1)
	//	halfUnits = 1;

	int i = 0;
	DistanceMap::iterator u, uEnd = distanceMap.end();
	for (u = distanceMap.begin(); u != uEnd; ++u)
	{
		CUnitImg& unit = *u->second;
		// consider only units with ground combat property
		if(!(unit.GetProperties() & UPR_COMBAT_GROUND))
			continue;

		bool bCoverFire = true;
		Vec3 vUnitEnemyPos = m_pLeader->GetAIGroup()->GetEnemyPosition(unit.m_pUnit);
		if(vUnitEnemyPos.IsZero())
			vUnitEnemyPos = m_pLeader->GetAIGroup()->GetEnemyPositionKnown();

		if(m_bApproachWithNoObstacle)
			m_pLeader->ForcePreferedPos(vUnitEnemyPos);

		CObstacleRef obstacle;

		if (unit.IsHiding() && (unit.GetClass()==UNIT_CLASS_CIVILIAN || unit.GetClass()==UNIT_CLASS_COMPANION)) 
			continue; // don't relocate civilians nor companions

		if (i < halfUnits || !bAllowFire )
		{
			// first half should change positions
			if (bAllowFire)
			{
				CObstacleRef currentObstacle = m_pLeader->m_pObstacleAllocator->GetUnitObstacle(unit.m_pUnit);
				obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit.m_pUnit, vUnitEnemyPos, true);
				if (obstacle == currentObstacle)
					obstacle = CObstacleRef();	// changing cover of this unit isn't really needed so skip it
			}
			else
				obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit.m_pUnit, unit.m_pUnit->GetPos(), true);

			if (obstacle)
			{
				Vec3 hidePoint = m_pLeader->GetHidePoint(unit.m_pUnit, obstacle);
				CUnitAction* action = new CUnitAction(UA_HIDEBEHIND, true, hidePoint);
				unit.SetHiding();
				hideList.push_back( std::make_pair(&unit, action) );

				if (m_bInitialized)
				{
					action = new CUnitAction(UA_TIMEOUT, true);
					timeoutList.push_back( std::make_pair(&unit, action) );
				}
				bCoverFire = false;
			}
			else
			{
				if(m_bApproachWithNoObstacle)
				{
					// move even without obstacle
					//TO DO: check if there is path
					Vec3 vMovePoint = vUnitEnemyPos - unit.m_pUnit->GetPos();
					float fDist = vMovePoint.GetLength();
					if(fDist>0)
						vMovePoint *= m_CSearchDistance/fDist;
					vMovePoint+=unit.m_pUnit->GetPos();

					CUnitAction* action = new CUnitAction(UA_HIDEBEHIND, true, vMovePoint);
					hideList.push_back( std::make_pair(&unit, action) );

					bCoverFire = false;
				}
				else
					++halfUnits;
			}
		}

		if (bAllowFire)
		{
			if (bCoverFire)
			{
				// other half should provide cover fire
				CUnitAction* action = new CUnitAction(UA_FIRE, true);
				fireList.push_back( std::make_pair(&unit, action) );

				obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit.m_pUnit, vUnitEnemyPos, true);

				Vec3 hidePoint = m_pLeader->GetHidePoint(unit.m_pUnit, obstacle);
				action = new CUnitAction(UA_HIDEBEHIND, true, hidePoint);
				unit.SetHiding();
				holdFireList.push_back( std::make_pair(&unit, action) );
			}
			else if (!m_bInitialized)
			{
				// now first half provides cover fire for others
				CUnitAction* action = new CUnitAction(UA_FIRE, true);
				fireList2.push_back( std::make_pair(&unit, action) );

				obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit.m_pUnit, vUnitEnemyPos, true);
				Vec3 hidePoint = m_pLeader->GetHidePoint(unit.m_pUnit, obstacle);
				action = new CUnitAction(UA_HIDEBEHIND, true, hidePoint);
				unit.SetHiding();
				holdFireList2.push_back( std::make_pair(&unit, action) );
			}
		}
		++i;
	}
	if(m_bApproachWithNoObstacle)
		m_pLeader->ForcePreferedPos(ZERO);

	if (hideList.empty() && !distanceMap.empty())
	{
		CUnitImg* unit = distanceMap.begin()->second;
		CUnitAction* action = new CUnitAction(UA_TIMEOUT, true);
		hideList.push_back( std::make_pair(unit, action) );
	}

	if (m_bInitialized)
	{
		BlockActionList(fireList, hideList);
		BlockActionList(hideList, holdFireList);
	}
	else
	{
		BlockActionList(fireList, hideList);
		BlockActionList(fireList2, holdFireList);
		BlockActionList(holdFireList, holdFireList2);
	}

	PushPlanFromList::Do(fireList);
	PushPlanFromList::Do(hideList);
	PushPlanFromList::Do(timeoutList);
	if (!m_bInitialized)
		PushPlanFromList::Do(fireList2);
	PushPlanFromList::Do(holdFireList);
	if (!m_bInitialized)
		PushPlanFromList::Do(holdFireList2);

	m_bInitialized = true;


	if (m_bNoTarget || !someoneHasTarget)
		return false;

	return true;

	/*
	obstacleList.push_back()


	// find a unit for repositioning
	int u = ai_rand();
	CUnitImg* unit = NULL;
	while (!unit)
	{
	u %= m_Units->size();
	TUnitList::iterator itRandom = m_Units->begin();
	int i = u;
	while (i--)
	++itRandom;
	unit = &*itRandom;
	if (!unit->m_pUnit->IsEnabled())
	{
	unit = NULL;
	++u;
	}
	}

	CObstacleRef obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit->m_pUnit, GetDestinationPoint(), false);
	if (true)//obstacle)
	{
	Vec3 hidePoint = m_pLeader->GetHidePoint(obstacle);

	// find a unit to provide cover fire
	CUnitImg* fireUnit = NULL;
	u = ai_rand() % m_Units->size();
	while (!fireUnit)
	{
	u %= m_Units->size();
	TUnitList::iterator itRandom = m_Units->begin();
	int i = u;
	while (i--)
	++itRandom;
	fireUnit = &*itRandom;
	if (!fireUnit->m_pUnit->IsEnabled() || unit == fireUnit)
	{
	fireUnit = NULL;
	++u;
	}
	}

	CUnitAction* fireAction = new CUnitAction(UA_FIRE, true);
	fireUnit->m_Plan.push_back(fireAction);

	CUnitAction* hideAction = new CUnitAction(UA_HIDEBEHIND, true, hidePoint);
	hideAction->BlockedBy(fireAction);
	unit->m_Plan.push_back(hideAction);

	obstacle = m_pLeader->m_pObstacleAllocator->GetOrAllocUnitObstacle(fireUnit->m_pUnit, GetDestinationPoint());
	hidePoint = m_pLeader->GetHidePoint(obstacle);
	CUnitAction* hideFireAction = new CUnitAction(UA_HIDEBEHIND, true, hidePoint);
	hideFireAction->BlockedBy(hideAction);
	fireUnit->m_Plan.push_back(hideFireAction);

	CUnitAction* timeoutAction = new CUnitAction(UA_TIMEOUT, true);
	fireUnit->m_Plan.push_back(timeoutAction);

	timeoutAction = new CUnitAction(UA_TIMEOUT, true);
	unit->m_Plan.push_back(timeoutAction);
	}
	else
	{
	}
	return true;
	*/
}

//
//-------------------------------------------------------------------------------------

void CLeaderAction_Attack::OnUpdateUnitTask(CUnitImg* unit)
{
	if (unit->m_Plan.size() > 1)
		return;

	if ((unit->GetClass()==UNIT_CLASS_CIVILIAN) || (unit->GetClass()==UNIT_CLASS_COMPANION)) 
		return; // don't move from your obstacle

	m_pLeader->m_pObstacleAllocator->SetSearchDistance(m_CSearchDistance);
	CObstacleRef obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit->m_pUnit, m_pLeader->GetAIGroup()->GetEnemyPosition(unit->m_pUnit), false);
	if (true)//|| obstacle)
	{
		Vec3 hidePoint = m_pLeader->GetHidePoint(unit->m_pUnit, obstacle);

		int count = GetUnitsList().size();
		if (count > 1)
		{
			// find a unit to provide me cover fire
			int u = ai_rand() % count;
			TUnitList::iterator itEnd = GetUnitsList().end();
			TUnitList::iterator itUnit = GetUnitsList().begin();
			for(int i=0;i<u; ++itUnit, i++);
			if (unit == &(*itUnit))
			{
				++itUnit;
				if(itUnit==itEnd)
					itUnit = GetUnitsList().begin();
			}

			CUnitImg* fireUnit = &(*itUnit);

			CUnitAction* fireAction = new CUnitAction(UA_FIRE, true);
			fireUnit->m_Plan.push_back(fireAction);

			CUnitAction* hideAction = new CUnitAction(UA_HIDEBEHIND, true, hidePoint);
			hideAction->BlockedBy(fireAction);
			unit->m_Plan.push_back(hideAction);

			obstacle = m_pLeader->m_pObstacleAllocator->GetOrAllocUnitObstacle(fireUnit->m_pUnit, m_pLeader->GetAIGroup()->GetEnemyPosition(fireUnit->m_pUnit));
			hidePoint = m_pLeader->GetHidePoint(fireUnit->m_pUnit, obstacle);
			CUnitAction* hideFireAction = new CUnitAction(UA_HIDEBEHIND, true, hidePoint);
			hideFireAction->BlockedBy(hideAction);
			fireUnit->m_Plan.push_back(hideFireAction);

			CUnitAction* timeoutAction = new CUnitAction(UA_TIMEOUT, true);
			fireUnit->m_Plan.push_back(timeoutAction);

			CUnitAction* whatsNextAction = new CUnitAction(UA_WHATSNEXT, false);
			fireUnit->m_Plan.push_back(whatsNextAction);

			timeoutAction = new CUnitAction(UA_TIMEOUT, true);
			unit->m_Plan.push_back(timeoutAction);

			whatsNextAction = new CUnitAction(UA_WHATSNEXT, false);
			unit->m_Plan.push_back(whatsNextAction);
		}
		else
		{
			CUnitAction* action = NULL;
			switch (unit->GetCurrentAction()->m_Tag)
			{
			case 1:
				action = new CUnitAction(UA_FIRE, true);
				unit->m_Plan.push_back(action);

				action = new CUnitAction(UA_TIMEOUT, true);
				unit->m_Plan.push_back(action);

				action = new CUnitAction(UA_WHATSNEXT, false);
				action->m_Tag = 2;
				unit->m_Plan.push_back(action);
				break;

			case 2:
				{
					//			CLeader::CUnitPointMap hidingPoints;
					//			TUnitList units;
					//			CUnitImg tempCopy(unit->m_pUnit);
					//			units.push_back(tempCopy);
					//			m_pLeader->FindNearestHidingPoints(units, hidingPoints);
					action = new CUnitAction(UA_HIDEBEHIND, true);
					CObstacleRef obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit->m_pUnit, m_pLeader->GetAIGroup()->GetEnemyPosition(unit->m_pUnit), false);
					if (true)//|| obstacle)
						action->m_Point = m_pLeader->GetHidePoint(unit->m_pUnit,obstacle);
					else
						action->m_Point = unit->m_TagPoint;
					unit->m_TagPoint = action->m_Point;
					unit->m_Plan.push_back(action);

					action = new CUnitAction(UA_TIMEOUT, true);
					unit->m_Plan.push_back(action);

					action = new CUnitAction(UA_WHATSNEXT, false);
					action->m_Tag = 1;
					unit->m_Plan.push_back(action);
					break;
				}
			}
		}
	}
}

// to do: deprecated
bool CLeaderAction_Attack::SomeoneHasTarget() const
{
	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CAIObject* pUnit = (*it).m_pUnit;
		if (HasTarget(pUnit))
			return true;
	}
	return false;
}

// to do: deprecated
bool CLeaderAction_Attack::HasTarget(CAIObject* unit) const
{
	if (unit->IsEnabled())
	{
		CPipeUser* pPiper = unit->CastToCPipeUser();
		if (pPiper)
		{
			IAIObject* pTarget = pPiper->GetAttentionTarget();
			if (pTarget && ((CAIObject*)pTarget)->GetType() != AIOBJECT_WAYPOINT)
			{
				if (pTarget == m_pLeader->GetFormationPoint(unit))
					pPiper->SetAttentionTarget(NULL);
				else
					return true;
			}
		}
	}
	return false;
}

bool CLeaderAction_Attack::ProcessSignal(const AISIGNAL& signal)
{
	/*
	if (!strcmp(signal.strText, AIORD_REPORTDONE))
	{
	if (m_pSubAction->GetType() == LA_ATTACK)
	{
	//			delete m_pSubAction;
	//			m_pSubAction = new CLeaderAction_Fire(pLeader);
	}
	else
	{
	delete m_pSubAction;
	m_pSubAction = new CLeaderAction_Hide(m_pLeader);
	}
	return true;
	}
	*/
	return false;
}

//-----------------------------------------------------------
//----  ATTACK HIDE AND COVER
//-----------------------------------------------------------

CLeaderAction_Attack_HideAndCover::CLeaderAction_Attack_HideAndCover(CLeader* pLeader, const LeaderActionParams& params)
{
	m_pLeader = pLeader;
	m_unitProperties = params.unitProperties;
	m_eActionSubType = LAS_ATTACK_HIDE_COVER;
	m_timeRunning = GetAISystem()->GetFrameStartTime();
	m_bInitialized = false;
	m_timeLimit = 4;
	m_bApproachWithNoObstacle = false;
	m_bCoverApproaching = true;
}


CLeaderAction::eActionUpdateResult CLeaderAction_Attack_HideAndCover::Update()
{
	FRAME_PROFILER( "CLeaderAction_Attack_HideAndCover::Update",GetISystem(),PROFILE_AI );
	CPuppet* pLeaderPuppet = (CPuppet*)m_pLeader->GetAssociation();
	if ( pLeaderPuppet && !pLeaderPuppet->IsEnabled())
		return ACTION_FAILED; //end this action if he's dead

	int unitCount = m_pLeader->GetAIGroup()->GetUnitCount(UPR_COMBAT_GROUND);
	if(!unitCount)
		return ACTION_FAILED;

	IAIObject* pLiveTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true,true);


	if(pLiveTarget)
	{
		m_timeRunning = GetAISystem()->GetFrameStartTime();
		Vec3 vAveragePositionWLiveTarget = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,UPR_COMBAT_GROUND|UPR_COMBAT_RECON);
		m_pLeader->MarkPosition(vAveragePositionWLiveTarget);
	}

	//HideLeader(vAveragePosition,vEnemyAveragePosition,UPR_COMBAT_GROUND);

	TUnitList::iterator it, itEnd = GetUnitsList().end();

	bool bFirstUpdate=false;

	Vec3 vAveragePosition = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,UPR_COMBAT_GROUND);
	m_vEnemyAveragePosition  = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();
	Vec3 vAvgDir = vAveragePosition - m_vEnemyAveragePosition;
	m_vAvgDirN = vAvgDir.GetNormalizedSafe();

	if(!m_bInitialized)
	{
		// form the two groups

		typedef std::multimap< float, CUnitImg* > SideMapT;
		SideMapT sideMap;
		// check who's on the left and who's on the right
		for (it = GetUnitsList().begin(); it != itEnd; ++it)
		{
			CUnitImg& unit = *it;
			if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
			{
				Vec3 vDir = (unit.m_pUnit->GetPos() - m_vEnemyAveragePosition);
				vDir.NormalizeSafe();
				Vec3 vCross = vDir.Cross(m_vAvgDirN);
				sideMap.insert(std::make_pair(vCross.z,&unit));
			}
		}
		int i=0; 
		for(SideMapT::iterator its = sideMap.begin();its!=sideMap.end();++its)
		{
			CUnitImg* pUnit = its->second;
			if(pUnit->m_pUnit->IsEnabled() && (pUnit->GetProperties() & UPR_COMBAT_GROUND))
			{
				i++;
				pUnit->m_Group = (i*2-1)/unitCount;
			}
		}

		m_pLeader->m_pObstacleAllocator->SetSearchDistance(m_CSearchDistance);


		m_bInitialized = true;
		bFirstUpdate = true;
		m_vLastEnemyPos = ZERO;
	}


	if(m_pLeader->GetActiveUnitPlanCount(UPR_COMBAT_GROUND)==0)
	{
		UpdateStats();

		float distanceToCheck = m_CApproachMinDistance + m_CSearchDistance ;

		int iCloserGroup = (m_dist[0]<m_dist[1] ? 0:1);
		// the cover group will be the one with has more visibility to targets on the first update, or the closest one
		if(bFirstUpdate)
			m_iCoverGroup = (m_counttarget[0] > m_counttarget[1] ? 0 : (m_counttarget[0] < m_counttarget[1] ? 1 : iCloserGroup));
		else
			m_iCoverGroup = iCloserGroup;

		int iHideGroup = 1 - m_iCoverGroup;

		if(m_dist[iHideGroup] > distanceToCheck )
		{
			if(m_bLastHideSuccessful)
			{
				m_bLastHideSuccessful = AdvanceHideAndCover();
				if(!m_bLastHideSuccessful)
					return ACTION_DONE;//HideInPlace();
			}
		}
		else 
		{
			return ACTION_DONE;
			/*if(!m_bHidingInPlace || Distance::Point_Point2DSq(m_vEnemyAveragePosition , m_vLastEnemyPos) > 5*5)
			{
			HideInPlace();
			}*/
		}
		m_vLastEnemyPos = m_vEnemyAveragePosition;
	}
	return ACTION_RUNNING;

}


// compute distances of the two groups
// compute visibility of targets of the two groups
void CLeaderAction_Attack_HideAndCover::UpdateStats()
{

	m_count[0] = m_count[1] = 0;
	m_pos[0] = m_pos[1] = ZERO;
	m_dist[0] = m_dist[1] = 0;
	m_counttarget[0] = m_counttarget[1] = 0;

	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CUnitImg& unit = *it;
		if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
		{
			int group = unit.m_Group;
			m_pos[group]+= unit.m_pUnit->GetPos();
			m_dist[group]+= (unit.m_pUnit->GetPos() - m_pLeader->GetAIGroup()->GetEnemyAveragePosition()).GetLength();
			m_count[group]++;

			CPipeUser* pPipeUser = unit.m_pUnit->CastToCPipeUser();
			if (pPipeUser)
			{
				IAIObject* pTarget = pPipeUser->GetAttentionTarget();
				if (pTarget && pTarget->IsAgent())
					m_counttarget[group]++;
			}
		}
	}

	if(m_count[0])
	{
		m_dist[0]= floor(m_dist[0]/float(m_count[0]));
		m_pos[0]= (m_pos[0]/float(m_count[0]));
	}
	if(m_count[1])
	{
		m_dist[1]= floor(m_dist[1]/float(m_count[1]));
		m_pos[1]= (m_pos[1]/float(m_count[1]));
	}


}


bool CLeaderAction_Attack_HideAndCover::AdvanceHideAndCover()
{
	int iHideGroup = 1 - m_iCoverGroup;
	float fAdvanceStep = m_dist[iHideGroup]/2;
	if(fAdvanceStep < m_CApproachStep) 
		fAdvanceStep = m_CApproachStep;
	m_fApproachDistance = m_dist[iHideGroup] - fAdvanceStep;
	if(m_fApproachDistance < m_CApproachMinDistance + m_CSearchDistance)
		m_fApproachDistance = m_CApproachMinDistance + m_CSearchDistance;

	Vec3 vDestPos = m_pos[m_iCoverGroup] - m_vAvgDirN * (m_dist[m_iCoverGroup] - m_fApproachDistance);

	bool bHideSuccessful = false;
	int iHidingUnitCount = 0;

	Vec3 hidePoint(ZERO);
	TUnitList::iterator itEnd = GetUnitsList().end();
	bool bLastHideSuccessful = false;
	for (TUnitList::iterator it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CUnitImg& unit = *it;
		if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
		{
			if(unit.m_Group==iHideGroup)
			{
				if((iHidingUnitCount &1) ==0)
				{

					AISignalExtraData data;
					data.iValue = 2;
					CObstacleRef prevObstacle = m_pLeader->m_pObstacleAllocator->GetUnitObstacle(unit.m_pUnit);
					CObstacleRef obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit.m_pUnit, m_vEnemyAveragePosition, true, m_CApproachMinDistance,vDestPos);
					if(!obstacle)
					{
						Vec3 vDisp(m_vAvgDirN);
						if(!vDisp.IsZero())
						{
							vDisp *= 3;
							float x = vDisp.x;
							vDisp.x = -vDisp.y;
							vDisp.y = x;
							obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit.m_pUnit, m_vEnemyAveragePosition, true,m_CApproachMinDistance, vDestPos + vDisp);
							if(!obstacle)
								obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit.m_pUnit, m_vEnemyAveragePosition, true,m_CApproachMinDistance, vDestPos - vDisp);
						}
					}
					if(obstacle && obstacle != prevObstacle)
					{
						bLastHideSuccessful = true;
						hidePoint = (obstacle ? m_pLeader->GetHidePoint(unit.m_pUnit, obstacle) : vDestPos);
						data.point = hidePoint;
						unit.m_Plan.push_back(new CUnitAction(UA_SIGNAL, true, "ORDER_HIDE",data));
						unit.ExecuteTask();
					}
					else
						bLastHideSuccessful = false;

				}
				else if(bLastHideSuccessful)
				{
					// make second unit follow the first
					AISignalExtraData data;
					data.iValue = 2;
					data.point = hidePoint + m_vAvgDirN*1.5;
					unit.m_Plan.push_back(new CUnitAction(UA_SIGNAL, true, "ORDER_HIDE",data));
					unit.ExecuteTask();
				}
				iHidingUnitCount++;
			}
		}

		bHideSuccessful |= bLastHideSuccessful;
	}

	if(bHideSuccessful)
	{
		for (TUnitList::iterator it = GetUnitsList().begin(); it != itEnd; ++it)
		{
			CUnitImg& unit = *it;
			if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
			{
				if(unit.m_Group==m_iCoverGroup)
				{
					unit.m_Plan.push_back(new CUnitAction(UA_FIRE, true));
					unit.ExecuteTask();
				}
			}
		}
	}

	return bHideSuccessful;
}


void CLeaderAction_Attack_HideAndCover::HideInPlace()
{
	TUnitList::iterator itEnd = GetUnitsList().end();
	for (TUnitList::iterator it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CUnitImg& unit = *it;
		if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
		{

			CObstacleRef obstacle = m_pLeader->m_pObstacleAllocator->GetUnitObstacle(unit.m_pUnit);
			if(!obstacle)
				obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit.m_pUnit, m_pLeader->GetAIGroup()->GetEnemyAveragePosition(), true, m_CApproachMinDistance);
			Vec3 vUnitPos = (obstacle ?  m_pLeader->GetHidePoint(unit.m_pUnit, obstacle) : unit.m_pUnit->GetPos());
			AISignalExtraData data;
			data.point = vUnitPos;
			data.iValue = 0;
			unit.m_Plan.push_back(new CUnitAction(UA_SIGNAL, true, "ORDER_HIDE_FIRE",data));
			unit.ExecuteTask();

		}
	}
}


//-----------------------------------------------------------
//----  ATTACK FOLLOW LEADER
//-----------------------------------------------------------
CLeaderAction_Attack_FollowLeader::CLeaderAction_Attack_FollowLeader(CLeader* pLeader,const LeaderActionParams& params)
{
	m_pLeader = pLeader;
	m_eActionType = LA_ATTACK;
	m_eActionSubType = params.subType; // either LAS_ATTACK_ROW or LAS_ATTACK_COORDINATED_FIRE2
	m_timeRunning = GetAISystem()->GetFrameStartTime();
	m_lastTimeMoving = m_timeRunning;
	m_bInitialized = false;
	m_sFormationType = params.name;
	m_unitProperties = params.unitProperties;
	m_timeLimit = params.fDuration;
	m_bLeaderMoving = false;
	CAIObject* pAILeader = static_cast<CAIObject*>(m_pLeader->GetAssociation());
	m_vLastUpdatePos = (pAILeader ? pAILeader->GetPos(): ZERO);
	m_bStealth = params.id.n ==0;
	if(!m_bStealth)
		m_SeeingEnemyList.push_back(params.id.n);


}

bool CLeaderAction_Attack_FollowLeader::ProcessSignal(const AISIGNAL& signal)
{
	//if(!strcmp(signal.strText, "OnRequestUpdate") ) 
	if (signal.Compare(g_crcSignals.m_nOnRequestUpdate))
	{
		IEntity* pEntity = (IEntity*)signal.pSender;
		if(pEntity)
		{
			CAIActor* pUnit = CastToCAIActorSafe(pEntity->GetAI());
			if(pUnit)
			{
				pUnit->SetSignal(1,"OnEndFollow", 0,0, g_crcSignals.m_nOnEndFollow);
				TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pUnit);
				if(itUnit != GetUnitsList().end())
				{
					CAIObject* pFormatonOwner = m_pLeader->GetFormationOwner();
					if(!pFormatonOwner)
						return true;
					CAIObject* myFormationPoint = m_pLeader->GetFormationPoint(pUnit);
					Vec3 vSearchPos = myFormationPoint ? myFormationPoint->GetPos() : pFormatonOwner->GetPos();
					SearchHideSpot(*itUnit,vSearchPos);
				}
			}
		}
		return true;
	}
	else if (signal.Compare(g_crcSignals.m_nOnRequestShootSpot))/*if(!strcmp(signal.strText, "OnRequestShootSpot") ) */
	{
		CAIObject* pFormatonOwner = m_pLeader->GetFormationOwner();
		CFormation* pFormazione;
		if(!pFormatonOwner)
			return true;

		pFormazione=pFormatonOwner->m_pFormation;
		if(!pFormazione)
			return true;
		CAIGroup* pGroup = m_pLeader->GetAIGroup();
		if(!pGroup)
			return true;

		IEntity* pRequestorEntity = signal.pSender;
		if(!pRequestorEntity)
			return true;
		CAIActor* pRequestor = CastToCAIActorSafe(pRequestorEntity->GetAI());
		if(!pRequestor)
			return true;
		float mindist = 10000665.f;
		float mindistNoTarget = mindist;
		int pointFound = -1;
		int pointFoundNoTarget = -1;
		int count = pFormazione->GetSize();
		for(int i = 1; i < count; i++)
		{	// don't use civilian points, they're supposed to be not good for shooting and might be needed by civilians at any moment
			if(pFormazione->GetPointClass(i)!= UNIT_CLASS_CIVILIAN && /*pFormazione->GetPointClass(i)== SHOOTING_SPOT_POINT && m_pLeader->GetFormationPointIndex(pRequestor) != i
																																&&*/ pFormazione->IsPointFree(i))
			{
				Vec3 vPos = pFormazione->GetFormationPoint(i)->GetPos();
				// 2D only
				float dist2 = Distance::Point_Point2DSq(pRequestor->GetPos(),vPos);
				if(dist2 <mindistNoTarget && dist2>1 && m_Spots[i].reserveTime==0)
				{
					mindistNoTarget = dist2;
					pointFoundNoTarget = i;
				}
				if(dist2 <mindist && dist2>1 && m_Spots[i].reserveTime==0)
				{
					// check if some target is visible from the formation point
					CAIGroup::TSetAIObjects::const_iterator itEnd  = pGroup->GetTargets().end();
					for(CAIGroup::TSetAIObjects::const_iterator it = pGroup->GetTargets().begin(); it !=itEnd;++it)
					{
						const CAIObject* pTarget = *it;
						if(pTarget && pTarget->IsAgent())
						{
							IPhysicalWorld *pWorld = GetAISystem()->GetPhysicalWorld();
							ray_hit hit;
							int rayresult(0);
							TICK_COUNTER_FUNCTION
								TICK_COUNTER_FUNCTION_BLOCK_START
								// to do: tweak the right Z position for formation point - should be the same Z as the requestor being at the formation point
								rayresult = pWorld->RayWorldIntersection(vPos, pTarget->GetPos() - vPos, COVER_OBJECT_TYPES|ent_living, HIT_COVER, &hit, 1,pTarget->GetProxy()->GetPhysics());
							TICK_COUNTER_FUNCTION_BLOCK_END
								if(!rayresult)
								{
									pointFound = i;
									mindist = dist2;
								}
						}
					}
				}
			}
		}

		if(pointFound<=0)
			pointFound = pointFoundNoTarget;

		if(pointFound>0)
		{
			//			pFormazione->FreeFormationPoint(pRequestor);
			//		pFormazione->GetNewFormationPoint(pRequestor,pointFound);
			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			if(pData)
			{
				pData->point = pFormazione->GetFormationPoint(pointFound)->GetPos();
				m_Spots[pointFound].pOwner = pRequestor;
				/// DEBUG
				float dis = Distance::Point_Point(pData->point,pRequestor->GetPos());
				//////////
				m_Spots[pointFound].reserveTime = GetAISystem()->GetFrameStartTime().GetSeconds();
				pRequestor->SetSignal(1,"OnShootSpotFound",pFormatonOwner->GetEntity(),pData, g_crcSignals.m_nOnShootSpotFound);
			}
		}
		else
			pRequestor->SetSignal(1,"OnShootSpotNotFound",pFormatonOwner->GetEntity(), 0, g_crcSignals.m_nOnShootSpotNotFound);

	}
	else 	if (signal.Compare(g_crcSignals.m_nOnRequestHideSpot))/*if(!strcmp(signal.strText, "OnRequestHideSpot") ) */
	{
		CAIObject* pFormatonOwner = m_pLeader->GetFormationOwner();
		CFormation* pFormazione;
		if(!pFormatonOwner)
			return true;
		pFormazione=pFormatonOwner->m_pFormation;
		if(!pFormazione)
			return true;
		CAIGroup* pGroup = m_pLeader->GetAIGroup();
		if(!pGroup)
			return true;

		IEntity* pRequestorEntity = signal.pSender;
		CAIObject* pRequestor(NULL);
		if(pRequestorEntity)
			pRequestor = (CAIObject*)pRequestorEntity->GetAI();
		if(!pRequestor)
			return true;

	}
	else 	if (!strcmp(signal.strText, "OnSeenByEnemy") ) 
	{
		m_bStealth = false;
		if(signal.pEData)
		{
			bool found = false;
			if(std::find(m_SeeingEnemyList.begin(),m_SeeingEnemyList.end(),signal.pEData->nID.n)== m_SeeingEnemyList.end())
				m_SeeingEnemyList.push_back(signal.pEData->nID.n);
		}

	}
	else 	if (!strcmp(signal.strText, "OnLostByEnemy") ) 
	{
		if(signal.pEData)
		{
			std::vector<int>::iterator it = std::find(m_SeeingEnemyList.begin(),m_SeeingEnemyList.end(),signal.pEData->nID.n);
			if(it!=m_SeeingEnemyList.end())			
				m_SeeingEnemyList.erase(it);
		}
		m_bStealth = m_SeeingEnemyList.empty();
			
	}
	return false;
}

void CLeaderAction_Attack_FollowLeader::DeadUnitNotify(CAIActor* pUnit)
{
	CLeaderAction::DeadUnitNotify(pUnit);
	int size = m_Spots.size();
	for(int i=1; i<size;i++)
	{
		TSpot& spot = m_Spots[i];
		if(spot.pOwner == pUnit)
		{
			spot.pOwner = NULL;
			spot.reserveTime = 0;
		}
	}
}


CLeaderAction::eActionUpdateResult CLeaderAction_Attack_FollowLeader::Update()
{

	m_vEnemyPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();


	if(!m_bInitialized)
	{
		if(m_sFormationType!="")
		{
			if(m_pLeader->LeaderCreateFormation(m_sFormationType,ZERO))
			{
				CAIActor* pOwnerActor = m_pLeader->GetFormationOwner()->CastToCAIActor();
				AIAssert(pOwnerActor && pOwnerActor->m_pFormation);
				pOwnerActor->m_pFormation->SetReferenceTarget((CAIObject*)(GetAISystem()->GetBeacon(m_pLeader->GetGroupId())), 0.15f);
				pOwnerActor->m_pFormation->ForceReachablePoints(true);
				//m_pLeader->GetFormationOwner()->m_pFormation->SetUpdate(false);
				m_bLeaderMoving = true;
				CheckNavType(pOwnerActor,false);
			}
			else
				return ACTION_FAILED; // no formation provided, no existing formation
		}
		else //if(!m_pLeader->GetFormationOwner() || !m_pLeader->GetFormationOwner()->m_pFormation)
		{
			return ACTION_FAILED; // no formation provided, no existing formation
		}
		int size = m_pLeader->GetFormationOwner()->m_pFormation->GetSize();
		m_Spots.resize(size);
		for(int i=0;i<size;i++)
		{
			m_Spots[i].pOwner = NULL;
			m_Spots[i].reserveTime = 0;
		}
		//		m_pLeader->m_pObstacleAllocator->SetSearchDistance(m_CSearchDistance);
		UpdateUnitPositions();

		int building;
		IVisArea *pArea;
		m_currentNavType = GetAISystem()->CheckNavigationType(m_pLeader->GetFormationOwner()->GetPos(),building,pArea,m_NavProperties);

		m_bInitialized = true;
		return ACTION_RUNNING;
	}

	CAIActor* pOwnerActor = m_pLeader->GetFormationOwner()->CastToCAIActor();
	if(!pOwnerActor)
		return ACTION_FAILED;

	pe_status_dynamics  dSt;
	Vec3 velocity(ZERO);
	if (pOwnerActor && pOwnerActor->IsAgent() && pOwnerActor->GetProxy() && pOwnerActor->GetProxy()->GetPhysics())
	{
		pOwnerActor->GetProxy()->GetPhysics()->GetStatus( &dSt );
		velocity = dSt.v;
	}

	// update shoot spot map
	CFormation* pFormazione=pOwnerActor->m_pFormation;
	if(pFormazione)
	{
		int size = m_Spots.size();
		for(int i=1; i<size;i++)
		{
			TSpot& spot = m_Spots[i];

			if(spot.pOwner && spot.reserveTime>0 && GetAISystem()->GetFrameStartTime().GetSeconds() - spot.reserveTime >10	&& 
				Distance::Point_Point2DSq(pFormazione->GetFormationPoint(i)->GetPos(),spot.pOwner->GetPos())>1)
			{
				spot.reserveTime = 0;
				spot.pOwner = NULL;
			}
			//i++;
		}
	}

	float dist2 = Distance::Point_PointSq(pOwnerActor->GetPos(),m_vLastUpdatePos);

	bool bPlayerBigMoving = dist2 > m_CUpdateDistance*m_CUpdateDistance;
	bool bPlayerMoving = velocity.GetLengthSquared()>0.1f;

	CAIObject* pAILeader = static_cast<CAIObject*>(m_pLeader->GetAssociation());
	if(!bPlayerMoving) 
	{

		if ((GetAISystem()->GetFrameStartTime() - m_lastTimeMoving).GetSeconds()> 2 && m_bLeaderMoving)
		{
			m_bLeaderMoving = false;
			if(pOwnerActor->m_pFormation)
				pOwnerActor->m_pFormation->SetUpdate(false);

			if(pAILeader)
				GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP,1,"OnLeaderStop",pAILeader);
		}
	}
	else
	{
		m_lastTimeMoving = GetAISystem()->GetFrameStartTime();
		if(bPlayerBigMoving)
		{
			if(pOwnerActor->m_pFormation)
			{
				pOwnerActor->m_pFormation->SetUpdate(true);
				pOwnerActor->m_pFormation->Update();
			}

			CheckNavType(pOwnerActor,true);
		}
	}

	if(bPlayerBigMoving)
	{
		m_bLeaderMoving = true;
		if(pAILeader)
			GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP,1,"OnLeaderMoving",pAILeader);
		//		UpdateUnitPositions();
		m_vLastUpdatePos = pOwnerActor->GetPos();
	}
	else
	{
		//further check, discarding useless targets
		const CAIObject* pEnemyTarget = NULL;
		if(pAILeader)
		{
			Vec3 enemyDir(pAILeader->GetPos() - m_vEnemyPos);
			pEnemyTarget = m_pLeader->GetAIGroup()->GetForemostEnemy(enemyDir,true);
			if(pEnemyTarget)
			{
				if(Distance::Point_PointSq(pAILeader->GetPos(),pEnemyTarget->GetPos()) > 30*30)
				{
					pe_status_dynamics  dSt;
					Vec3 velocity(ZERO);
					if(pEnemyTarget->GetProxy() && pEnemyTarget->GetProxy()->GetPhysics())
					{
						pEnemyTarget->GetProxy()->GetPhysics()->GetStatus( &dSt );
						velocity = dSt.v;
					}
					if(pAILeader->GetProxy() && pAILeader->GetProxy()->GetPhysics())
					{
						pAILeader->GetProxy()->GetPhysics()->GetStatus( &dSt );
						velocity = velocity - dSt.v; // get relative velocity to the leader
					}
					if(!velocity.IsEquivalent(ZERO) && velocity.GetNormalizedSafe().Dot(enemyDir.GetNormalizedSafe()) <0.1f)
						pEnemyTarget=NULL;
				}

			}
		}
		if(pEnemyTarget)
			m_timeRunning = GetAISystem()->GetFrameStartTime();
		else if ((GetAISystem()->GetFrameStartTime() - m_timeRunning).GetSeconds()> m_timeLimit)
			return ACTION_DONE;
	}

	return ACTION_RUNNING;

}

void CLeaderAction_Attack_FollowLeader::UpdateUnitPositions()
{
	CAIObject* pFormatonOwner = m_pLeader->GetFormationOwner();
	pe_status_dynamics  dSt;
	Vec3 velocity(ZERO);
	if(pFormatonOwner && pFormatonOwner->IsAgent() && pFormatonOwner->GetProxy() && pFormatonOwner->GetProxy()->GetPhysics())
	{
		pFormatonOwner->GetProxy()->GetPhysics()->GetStatus( &dSt );
		velocity = dSt.v;
	}
	SetTeamNavProperties();
	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg& unit = *unitItr;
		CAIObject* pUnit = unit.m_pUnit;
		if((unit.GetProperties() & m_unitProperties) && pUnit->IsEnabled() && pUnit != pFormatonOwner)
		{
			CAIObject* myFormationPoint = m_pLeader->GetFormationPoint(pUnit);
			Vec3 vSearchPos = myFormationPoint ? myFormationPoint->GetPos() : pFormatonOwner->GetPos();
			vSearchPos += velocity; // videogame-style physics :) search hide points more forward along moving direction
			SearchHideSpot(unit,vSearchPos);
		}
	}

}

void CLeaderAction_Attack_FollowLeader::SearchHideSpot(CUnitImg& unit, Vec3 vSearchPos)
{

	CAIObject* pUnit = unit.m_pUnit;
	Vec3 closestEnemyPos = m_pLeader->GetAIGroup()->GetForemostEnemyPosition(vSearchPos - m_vEnemyPos);
	CPuppet* pMarionette = pUnit->CastToCPuppet();
	if(pMarionette)
		pMarionette->SetRefPointPos(vSearchPos);

	unit.ClearFlags();
	unit.ClearPlanning();
	CUnitAction* action = new CUnitAction(UA_SIGNAL, true, "ORDER_FOLLOW_FIRE");
	unit.m_Plan.push_back(action);
	unit.ExecuteTask();


}

void CLeaderAction_Attack_FollowLeader::ResumeUnit(CUnitImg& unit)
{
	CAIObject* pFormatonOwner = m_pLeader->GetFormationOwner();
	CAIObject* pUnit = unit.m_pUnit;
	if(!pFormatonOwner)
		return ;
	pe_status_dynamics  dSt;
	Vec3 velocity(ZERO);

	CPuppet* pPuppetOwner = CastToCPuppetSafe(pFormatonOwner);
	if(pPuppetOwner && pPuppetOwner->GetProxy() && pPuppetOwner->GetProxy()->GetPhysics())
	{
		pPuppetOwner->GetProxy()->GetPhysics()->GetStatus( &dSt );
		velocity = dSt.v;
	}
	CAIObject* myFormationPoint = m_pLeader->GetFormationPoint(pUnit);
	if(!myFormationPoint)
		myFormationPoint = m_pLeader->GetNewFormationPoint(pUnit, unit.GetClass());
	Vec3 vSearchPos = myFormationPoint ? myFormationPoint->GetPos() : pFormatonOwner->GetPos();

	vSearchPos += velocity; // videogame-style physics :) search hide points more forward along moving direction

	SearchHideSpot(unit,vSearchPos);

}

//-----------------------------------------------------------
//----  ATTACK ROW
//-----------------------------------------------------------

CLeaderAction_Attack_Row::CLeaderAction_Attack_Row(CLeader* pLeader, const LeaderActionParams& params)
{
	m_pLeader = pLeader;
	m_eActionType = LA_ATTACK;
	m_eActionSubType = params.subType; // either LAS_ATTACK_ROW or LAS_ATTACK_COORDINATED_FIRE2
	m_unitProperties = UPR_COMBAT_GROUND;
	m_timeLimit = params.fDuration;
	m_timeRunning = GetAISystem()->GetFrameStartTime();
	m_bInitialized = false;
	m_vRowNormalDir = ZERO;
	m_vBasePoint = params.vPoint.IsZero() ? m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,UPR_COMBAT_GROUND) : 
	params.vPoint;

	m_fSize = params.fSize;
	m_bTimerRunning = false;
	m_vDefensePoint = params.vPoint2;

	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		unitItr->ClearHiding();

	m_pLeader->AssignTargetToUnits(UPR_COMBAT_RECON,2);

}


bool CLeaderAction_Attack_Row::ProcessSignal(const AISIGNAL& signal)
{
	//	if(!strcmp(signal.strText, "OnStartTimer") ) 
	if (signal.Compare(g_crcSignals.m_nOnStartTimer))
	{
		m_bTimerRunning = true;
		return true;
	}
	return false;
}

CLeaderAction::eActionUpdateResult CLeaderAction_Attack_Row::Update()
{
	// end action if leader is dead
	CPuppet* pLeaderPuppet = (CPuppet*)m_pLeader->GetAssociation();
	if ( pLeaderPuppet && !pLeaderPuppet->IsEnabled())
		return ACTION_FAILED; 

	Vec3 averagePos = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES, UPR_COMBAT_GROUND);
	Vec3 enemyAvgPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();
	if(enemyAvgPos.IsZero())
	{
		IAIObject* pBeacon = GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
		if(pBeacon)
			enemyAvgPos = pBeacon->GetPos();
	}
	Vec3 tmpPos = averagePos - enemyAvgPos;
	Vec3 enemyPos = m_pLeader->GetAIGroup()->GetForemostEnemyPosition(tmpPos);
	if(enemyPos.IsZero()) 
		return ACTION_FAILED;

	if(!m_bTimerRunning)
		m_timeRunning = GetAISystem()->GetFrameStartTime();

	int numUnits = m_pLeader->GetAIGroup()->GetUnitCount(UPR_COMBAT_GROUND);
	if(numUnits<=0)
		return ACTION_FAILED;

	if(!m_bInitialized) 
	{
		m_timeRunning = GetAISystem()->GetFrameStartTime();
		Vec3 dir = enemyPos - m_vBasePoint;
		m_fInitialDistance = dir.GetLength();
		m_vRowNormalDir  = dir;
		m_vRowNormalDir.z = 0; //2d only
		m_vRowNormalDir.NormalizeSafe();
		if(enemyPos.IsZero() )
		{	// no enemy position, no one to attack
			//			EndAttackAction("OnAttackRowTimeOut",UPR_COMBAT_GROUND);
			return ACTION_FAILED;
		}

		// check if there is a row formation already
		CFormation* pFormation;
		bool bRowFormationExisting = false;
		/*		if(m_pLeader->GetFormationOwner() && (pFormation=m_pLeader->GetFormationOwner()->m_pFormation))
		{
		bRowFormationExisting = (pFormation->GetDescriptor() == m_pLeader->GetFormationName("Row",numUnits));
		}
		*/	
		float fScale = m_fSize;// /(float)(numUnits>1? numUnits-1:1);
		if(!bRowFormationExisting && !m_pLeader->CreateFormation_Row(enemyPos,fScale,numUnits,true,static_cast<CAIObject*>(GetAISystem()->GetBeacon(m_pLeader->GetGroupId())),UPR_COMBAT_GROUND))
			return ACTION_FAILED;
		// force update for correct positioning, in case the owner won't move to reach his destination
		pFormation = m_pLeader->GetFormationOwner()->m_pFormation;
		pFormation->Update(/*m_pLeader->GetFormationOwner()*/);

		bool bMove = false;

		m_vEnemyPos = enemyPos;

		CUnitAction* pApproachAction=NULL;
		CUnitAction* pNotifyLeaderAction=NULL;
		TUnitList::iterator itEnd = GetUnitsList().end();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg& unit = *unitItr;
			if(unit.m_pUnit == m_pLeader->GetFormationOwner())
			{
				//float fMove =  m_fInitialDistance - (unit.m_pUnit->GetPos() - enemyPos).GetLength();
				//bMove = (fabs(fMove)>1.f);
				Vec3 vMovePoint = m_vBasePoint;// unit.m_pUnit->GetPos() - m_vRowNormalDir*fMove;
				if((numUnits & 1)==0)// adjust owner position for even formation (it's not in the middle of it)
					vMovePoint -= Vec3(m_vRowNormalDir.y,-m_vRowNormalDir.x,0)*fScale/2;

				pApproachAction = new CUnitAction(UA_MOVE,true, vMovePoint	);
				unit.m_Plan.push_back(pApproachAction);
				pNotifyLeaderAction = new CUnitAction(UA_SIGNAL_LEADER,false, "OnStartTimer"	);
				unit.m_Plan.push_back(pNotifyLeaderAction);
				break;
			}
		}
		AIAssert(pApproachAction);

		CAIObject* pAILeader = static_cast<CAIObject*>(m_pLeader->GetAssociation());
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg& unit = *unitItr;
			if(unit.m_pUnit == pAILeader)
			{
				CUnitAction* pAction = new CUnitAction(UA_SIGNAL, false, "OnResetFormationUpdate");
				unit.m_Plan.push_back(pAction);
				pAction->BlockedBy(pApproachAction);
			}
			if (unit.GetProperties() & UPR_COMBAT_GROUND)
			{
				if(unit.m_pUnit != m_pLeader->GetFormationOwner() )
				{
					CUnitAction* pAction = new CUnitAction(UA_FORM, true, PRIORITY_HIGH);
					unit.m_Plan.push_back(pAction);
				}

				if(m_eActionSubType == LAS_ATTACK_ROW)
				{
					CUnitAction* pAttackAction =	new CUnitAction(UA_HIDEAROUND, true,ZERO);
					unit.m_Plan.push_back(pAttackAction);
					if(unit.m_pUnit != m_pLeader->GetFormationOwner())
						pAttackAction->BlockedBy(pApproachAction);
				}
				unit.ExecuteTask();
				// else see below
			}

		}
		//sort the unit by formation point position 
		if(m_eActionSubType == LAS_ATTACK_COORDINATED_FIRE2)
		{
			AISignalExtraData data;
			data.fValue = m_CCoordinatedFireDelayStep;
			data.iValue = 0;
			IAIObject* pTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true,true);
			if(pTarget && pTarget->GetEntity())
			{
				data.nID = (pTarget->GetEntityID());
			}
			else
			{
				return ACTION_FAILED;
			}
			typedef std::map<float,CUnitImg*> TUnitDistMap;
			TUnitDistMap MapUnitsDistance;
			TUnitList::iterator itEnd = GetUnitsList().end();
			for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
			{
				CUnitImg& unit = *unitItr;
				if (unit.GetProperties() & UPR_COMBAT_GROUND)
				{
					CAIObject* pFormationPoint = pFormation->GetFormationPoint(unit.m_pUnit);
					if(pFormationPoint)
					{
						float dist0 = pFormationPoint->GetPos().GetLengthSquared();
						MapUnitsDistance.insert(std::make_pair(dist0,&unit));
					}
				}
			}
			for(TUnitDistMap::iterator it = MapUnitsDistance.begin(); it!=MapUnitsDistance.end();++it)
			{
				CUnitImg* unit = it->second;
				CUnitAction* pAttackAction =	new CUnitAction(UA_SIGNAL, true,"ORDER_COORDINATED_FIRE2",data);
				unit->m_Plan.push_back(pAttackAction);
				if(unit->m_pUnit != m_pLeader->GetFormationOwner())
					pAttackAction->BlockedBy(pApproachAction);
				data.iValue ++;
			}
		}


		m_bInitialized = true;
	}
	if(m_pLeader->GetFormationOwner()==NULL)
		return ACTION_DONE; // formation owner got killed

	if(!m_vDefensePoint.IsZero())
	{
		if(m_pLeader->GetActiveUnitPlanCount(UPR_COMBAT_GROUND)==0)
		{
			// units are in position, check coverage
			if(m_pLeader->GetCoverPercentage(m_vDefensePoint,UPR_COMBAT_GROUND, true)<0.5f)
				return ACTION_DONE;
		}
	}
	//	if((m_vEnemyPos - enemyPos).GetLength() >5) 
	//	return ACTION_DONE; // enemy has moved
	m_vEnemyPos = enemyPos;

	if(m_bTimerRunning)
	{
		if(!IsInFOV(enemyPos,UPR_COMBAT_GROUND,averagePos,numUnits))
		{
			if ((GetAISystem()->GetFrameStartTime() - m_timeRunning).GetSeconds()> 2)
				return ACTION_DONE;
		}
		else if ((GetAISystem()->GetFrameStartTime() - m_timeRunning).GetSeconds()> m_timeLimit)
			return ACTION_DONE;
	}
	//HideLeader(averagePos,enemyPos,UPR_COMBAT_GROUND);
	//vCurrentDir.z = 0; //2d only
	//vCurrentDir.NormalizeSafe();

	IAIObject* pLiveTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true,true);
	if(pLiveTarget)
	{
		Vec3 vAveragePositionWLiveTarget = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,UPR_COMBAT_GROUND|UPR_COMBAT_RECON);
		m_pLeader->MarkPosition(vAveragePositionWLiveTarget);
	}

	/*
	CFormation* pFormation = m_pLeader->GetFormationOwner()->m_pFormation;
	float cosAngle = fabs(vCurrentDir.Dot(pFormation->GetMoveDir()));

	if(!pFormation->IsUpdated() && cosAngle<0.7f ) 
	{
	// TO DO: reorienting the formation (by setting the update )
	// by sending a second attack request (signal to behavior?)
	// just updating the formation is not enough, formation points could go out of the walls/navigable space
	//pFormation->SetUpdate(true);
	//pFormation->Update(m_pLeader->GetFormationOwner());
	//pFormation->SetUpdate(false);
	//EndAttackAction("OnAttackRowTimeOut",UPR_COMBAT_GROUND);
	return ACTION_DONE;
	}
	*/

	//	if(m_pLeader->GetAIGroup()->GetAttentionTarget(true))//only hostile targets
	//		m_timeRunning = GetAISystem()->GetCurrentTime();
	else if ((GetAISystem()->GetFrameStartTime() - m_timeRunning).GetSeconds()> m_timeLimit)
	{
		//EndAttackAction("OnAttackRowTimeOut",UPR_COMBAT_GROUND);
		return ACTION_DONE;
	}
	return ACTION_RUNNING;
}

bool CLeaderAction_Attack_Row::IsInFOV(const Vec3& point, uint32 unitProp, const Vec3& averagePos, int unitCount) const
{
	TUnitList::const_iterator itEnd = GetUnitsList().end();
	TUnitList::const_iterator it = GetUnitsList().begin();

	if(unitCount==1)
	{
		const CUnitImg& unit = *it;
		if(unit.GetProperties()& unitProp)
		{
			CPuppet* pPuppet = unit.m_pUnit->CastToCPuppet();
			if(pPuppet && pPuppet->IsPointInFOVCone(point))
			{
				float distance = (pPuppet->GetPos() - point).GetLength();
				if(distance < m_fInitialDistance+5)
					return true;
			}
		}
	}
	else
	{
		// compute the field of view 
		// TO DO: optimize, compute the polygon once every time the number of units changes, using formation points
		// instead of unit positions

		// find edge units in the row
		CAIObject* pEdgeUnit[2] = {NULL,NULL};
		Vec3 vRowDirection(m_vRowNormalDir.y,-m_vRowNormalDir.x,0); //2D only
		for(int i=0;i<2;i++)
		{
			float fMaxDist=0;

			for(it=GetUnitsList().begin() ; it!=itEnd;++it)
			{
				const CUnitImg& unit = *it;
				if(unit.GetProperties()& unitProp)
				{
					Vec3 vDiff = unit.m_pUnit->GetPos() - averagePos;
					if(vDiff.Dot(vRowDirection)>0)
					{
						float dist2 = vDiff.GetLengthSquared2D();//2D only
						if(dist2>fMaxDist)
						{
							fMaxDist = dist2;
							pEdgeUnit[i] = unit.m_pUnit;
						}
					}
				}
			}
			vRowDirection = -vRowDirection;
		}
		AIAssert(pEdgeUnit[0] && pEdgeUnit[1]);
		float fRectangleHeight = m_fInitialDistance+5;
		//2d only
		Vec3 UnitPos0 =  pEdgeUnit[0]->GetPos();
		Vec3 UnitPos1 =  pEdgeUnit[1]->GetPos();
		UnitPos0.z = 0;
		UnitPos1.z = 0;
		Vec3 vActualRowDirection = UnitPos1 - UnitPos0;
		vActualRowDirection.NormalizeSafe();
		Vec3 vRowNormal(vActualRowDirection.y,-vActualRowDirection.x,0);//turn +90
		if(vRowNormal.Dot(m_vRowNormalDir)<0)
			vRowNormal = -vRowNormal;

		Vec3 PointRectangle0 = UnitPos0 + vRowNormal*fRectangleHeight;
		Vec3 PointRectangle1 = UnitPos1 + vRowNormal*fRectangleHeight;
		Vec3 PointTriangle0 = PointRectangle0 - vActualRowDirection*(fRectangleHeight/2);
		Vec3 PointTriangle1 = PointRectangle1 + vActualRowDirection*(fRectangleHeight/2);
		static std::vector<Vec3> polygon;
		polygon.resize(0);
		polygon.push_back(UnitPos0);
		polygon.push_back(UnitPos1);
		polygon.push_back(PointTriangle1);
		polygon.push_back(PointTriangle0);
		/*
		pEdgeUnit0                 pEdgeUnit1
		o-----o-----o-----o
		/                   \
		/                     \
		/                       \
		/                         \
		o----+-----------------+----o
		PointTriangle0                          PointTriangle1
		*/
		/* debug stuff
		UnitPos0.z = m_pLeader->GetFormationOwner()->GetPos().z;
		UnitPos1.z = m_pLeader->GetFormationOwner()->GetPos().z;
		PointTriangle0.z = m_pLeader->GetFormationOwner()->GetPos().z;
		PointTriangle1.z = m_pLeader->GetFormationOwner()->GetPos().z;
		GetAISystem()->AddDebugLine(UnitPos0,UnitPos1,1.0f,1.0f,1.0f,0.6f);
		GetAISystem()->AddDebugLine(UnitPos1,PointTriangle1,1.0f,1.0f,1.0f,0.6f);
		GetAISystem()->AddDebugLine(UnitPos0,PointTriangle0,1.0f,1.0f,1.0f,0.6f);
		GetAISystem()->AddDebugLine(PointTriangle0,PointTriangle1,1.0f,1.0f,1.0f,0.6f);
		*/
		return Overlap::Point_Polygon2D(point,polygon);
	}
	return false;
}

void CLeaderAction_Attack_Row::ResumeUnit(CUnitImg& unit)
{
	CUnitAction* pAction = new CUnitAction(UA_FORM, true, PRIORITY_HIGH);
	unit.m_Plan.push_back(pAction);
	CUnitAction* pAttackAction = new CUnitAction(UA_HIDEAROUND, true,ZERO);
	unit.m_Plan.push_back(pAttackAction);
	unit.ExecuteTask();

}

//-----------------------------------------------------------
//----  ATTACK FLANK
//-----------------------------------------------------------

CLeaderAction_Attack_Flank::CLeaderAction_Attack_Flank(CLeader* pLeader,float duration, bool bHiding)

{
	m_pLeader = pLeader;
	m_eActionType = LA_ATTACK;
	m_eActionSubType = bHiding? LAS_ATTACK_FLANK_HIDE : LAS_ATTACK_FLANK;
	m_unitProperties = UPR_COMBAT_GROUND;
	m_timeLimit = duration;
	m_bHiding = bHiding;
	m_timeRunning = GetAISystem()->GetFrameStartTime();
	m_bInitialized = false;

	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		unitItr->ClearHiding();

	m_pLeader->AssignTargetToUnits(UPR_COMBAT_RECON,2);

	m_vEnemyPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();

}

CLeaderAction_Attack_Flank::~CLeaderAction_Attack_Flank()
{
}

CLeaderAction::eActionUpdateResult CLeaderAction_Attack_Flank::Update()
{

	CPuppet* pLeaderPuppet = (CPuppet*)m_pLeader->GetAssociation();
	if ( pLeaderPuppet && !pLeaderPuppet->IsEnabled())
		return ACTION_FAILED; 

	int numUnits = m_pLeader->GetAIGroup()->GetUnitCount(UPR_COMBAT_GROUND);
	if(numUnits==0)
		return ACTION_FAILED;

	bool bFlankSuccessful = true;
	if(!m_bInitialized) 
	{
		if(numUnits <3)//too few units
			return ACTION_FAILED;

		m_vStartPos = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,UPR_COMBAT_GROUND);

		m_fFlankDirection = (ai_frand()>0.5f? 1.f:-1.f);

		m_iFlankingUnitsCount = 0;
		m_iFlankingUnits = (numUnits*2)/3;
		// choose the flanking units - 
		m_iStage=1;

		TUnitList::iterator itEnd = GetUnitsList().end();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
			unitItr->ClearUnitAction(UA_FORM);

		bFlankSuccessful = ChooseFlankingUnits();

		m_bInitialized = true;
	}

	Vec3 currentEnemyPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();
	Vec3 avgPos = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,UPR_COMBAT_GROUND);
	//HideLeader(avgPos,currentEnemyPos,UPR_COMBAT_GROUND);

	// check if the enemy has moved too much -> end action
	// TO DO: move all the units in formation with the beacon and keep flanking?
	if((currentEnemyPos - m_vEnemyPos).GetLength()>5) 
		return ACTION_DONE;

	float fTimePassed = (GetAISystem()->GetFrameStartTime() - m_timeRunning).GetSeconds();

	if(m_iStage<2 && (fTimePassed  >2 || !bFlankSuccessful))
	{
		m_iStage = 2;
		m_fFlankDirection = -m_fFlankDirection;
		bFlankSuccessful |= ChooseFlankingUnits();
	}
	if(!bFlankSuccessful && fTimePassed<=2)//|| m_pLeader->IsIdle())
	{// flank impossible in either sides
		//		EndAttackAction("OnAttackFlankFailed",UPR_COMBAT_GROUND);
		return ACTION_FAILED;
	}

	return (m_pLeader->GetActiveUnitPlanCount(UPR_COMBAT_GROUND) ? ACTION_RUNNING : ACTION_DONE);
}


bool CLeaderAction_Attack_Flank::ChooseFlankingUnits()
{
	bool bFlankingUnitsFound = false;
	Vec3 enemyPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();

	Vec3 vCurrentDir = enemyPos - m_vStartPos;
	vCurrentDir.z = 0; //2d only
	float fDistance = vCurrentDir.GetLength();
	if(fDistance>0)
		vCurrentDir /= fDistance;

	int maxCount = (m_iStage==1? m_iFlankingUnits/2 : m_iFlankingUnits);

	Vec3 vX(vCurrentDir.y*m_fFlankDirection, -vCurrentDir.x*m_fFlankDirection,0); //2d only
	Vec3 vForemostEnemyPos = m_pLeader->GetAIGroup()->GetForemostEnemyPosition(vX);
	CAIObject* pAILeader = (CAIObject*)m_pLeader->GetAssociation();
	// Check if the enemies are too spread in the flank direction
	if(pAILeader)
	{
		CAIActor* pAILeaderActor = pAILeader->CastToCAIActor();
		float fsize = pAILeaderActor->GetParameters().m_fPassRadius*2;
		if(fsize<1)
			fsize=1;
		if(Distance::Point_Point2D(enemyPos,vForemostEnemyPos)>fsize*7)
			return false;
	}

	Vec3 flankApproachBasePoint = vForemostEnemyPos + vX*32;//set far away aside of target, to get the most aside units in the group
	TDistanceUnitMap mapDistanceUnit;

	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg& unit = *unitItr;
		if ((unit.GetProperties() & UPR_COMBAT_GROUND) && unit.m_pUnit->IsEnabled() && unit.Idle() //!(unit.GetCurrentAction() && unit.GetCurrentAction()->m_Action ==UA_FLANK))
			&& !(m_bHiding && unit.m_pUnit==pAILeader))// TO DO: use another flag instead of m_bHiding for excluding the leader from flank?
		{
			// choose the flanking units - 
			float distance = (unit.m_pUnit->GetPos() - flankApproachBasePoint).GetLength();
			mapDistanceUnit.insert(std::pair<float,CUnitImg*>(distance,&unit));
		}
	}

	Vec3 flankBasePoint = vForemostEnemyPos + vX*8;
	TDistanceUnitMap::iterator it = mapDistanceUnit.begin();
	float fNumFlankingUnits = m_iFlankingUnits/2;
	float fStep = 3; // 3 meters to each other
	Vec3 vFlankPos = flankBasePoint + vCurrentDir*fNumFlankingUnits/2*fStep;
	int iMaxUnitIndex = m_iStage==1? m_iFlankingUnits/2 : m_iFlankingUnits;

	float fDuration = float(ai_rand()%4)+5;
	if(m_bHiding)
		m_pLeader->m_pObstacleAllocator->SetSearchDistance(m_CSearchDistance);


	for(int iFlankingUnitsCount = m_iFlankingUnitsCount ; iFlankingUnitsCount<iMaxUnitIndex && it!=mapDistanceUnit.end(); ++iFlankingUnitsCount)
	{	
		CUnitImg* unit = it->second;

		bool bCanFlank = false;
		Vec3 vFlankActualPos;
		if(GetAISystem()->IsPathWorth( unit->m_pUnit, unit->m_pUnit->GetPos(),vFlankPos,2,0.4f,&vFlankActualPos))
		{
			// check reachability of target from flank position
			if(GetAISystem()->IsPathWorth( unit->m_pUnit, vFlankActualPos,vForemostEnemyPos,2))
			{
				if(m_bHiding)
				{
					Vec3 foremostEnemyPos = m_pLeader->GetAIGroup()->GetForemostEnemyPosition(vFlankActualPos - m_pLeader->GetAIGroup()->GetEnemyAveragePosition());
					CObstacleRef obstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit->m_pUnit, foremostEnemyPos, true);
					if(obstacle)
						bCanFlank=true;
				}
				else
					bCanFlank=true;

				if(bCanFlank)
					unit->m_TagPoint = vFlankActualPos;
			}
		}

		if(bCanFlank)
			++it;
		else
		{
			m_iFlankingUnitsCount++;
			mapDistanceUnit.erase(it++);
		}
	}
	int iCount = 0;
	for(it = mapDistanceUnit.begin(); m_iFlankingUnitsCount<iMaxUnitIndex && it!=mapDistanceUnit.end(); ++m_iFlankingUnitsCount)
	{	
		CUnitImg* unit = it->second;

		CUnitAction* pAction = new CUnitAction(UA_FLANK, true, unit->m_TagPoint);
		unit->m_Plan.push_back(pAction);
		AISignalExtraData data;
		data.point = unit->m_TagPoint;
		data.fValue = fDuration;
		data.iValue = iCount & 1;
		pAction = new CUnitAction(UA_HIDEAROUND, true,"",data);

		unit->m_Plan.push_back(pAction);

		// make unit go back where it's staying now
		pAction = new CUnitAction(UA_MOVE, true,unit->m_pUnit->GetPos());
		unit->m_Plan.push_back(pAction);
		unit->ExecuteTask();
		bFlankingUnitsFound = true;
		//m_iFlankingUnitsCount++;
		++it;
		iCount++;
		vFlankPos -= vCurrentDir*fStep;
	}
	return bFlankingUnitsFound;
}


bool CLeaderAction_Attack_Flank::TimeOut()
{
	return true;
}

//-----------------------------------------------------------
//----  ATTACK APPROACH
//-----------------------------------------------------------


CLeaderAction_Attack_LeapFrog::CLeaderAction_Attack_LeapFrog(CLeader* pLeader, const LeaderActionParams& params)
{
	m_pLeader = pLeader;
	m_unitProperties = params.unitProperties;

	m_timeLimit = 5; // wait this time before switching to search, when nobody has a live target
	m_bNoTarget = false;
	m_timeRunning = 0.f;
	m_eActionType = LA_ATTACK;
	m_eActionSubType = LAS_ATTACK_LEAPFROG;
	//	m_bForceFinalUpdate = false;
	m_bApproached = true;
	m_fApproachDistance = m_CApproachInitDistance;
	Vec3 vEnemyPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();

	float fDist = vEnemyPos.GetLength();
	if(m_fApproachDistance > fDist)
		m_fApproachDistance = floor(fDist/m_CApproachStep)*m_CApproachStep;

	m_bInitialized = false;

	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		unitItr->ClearHiding();

	m_timeRunning = GetAISystem()->GetFrameStartTime();
	m_pLeader->AssignTargetToUnits(UPR_COMBAT_RECON,2);
	m_pLeader->m_pObstacleAllocator->SetSearchDistance(m_CSearchDistance);

	FinalUpdate();
}
/*
bool CLeaderAction_Attack_LeapFrog::TimeOut()
{
CTimeValue frameTime = GetAISystem()->GetFrameStartTime(); 
// check if no one has a target
if( (frameTime - m_timeRunning).GetSeconds() > m_timeLimit  )
{
for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=GetUnitsList().end(); ++unitItr)
{
CUnitImg &curUnit = (*unitItr);
if(curUnit.GetProperties() & UPR_COMBAT_GROUND)
curUnit.ClearPlanning(PRIORITY_HIGH);
}
m_bForceFinalUpdate = false;
return true;
}

return false;
}

CLeaderAction::eActionUpdateResult CLeaderAction_Attack_LeapFrog::Update()
{
m_pLiveTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true,true);
if(m_pLiveTarget)
{
m_timeRunning = GetAISystem()->GetFrameStartTime();
m_bForceFinalUpdate = false;
Vec3 vAveragePositionWLiveTarget = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,UPR_COMBAT_GROUND|UPR_COMBAT_RECON);
m_pLeader->MarkPosition(vAveragePositionWLiveTarget);
}
else
if(!m_bForceFinalUpdate)
{
FinalUpdate();
m_bForceFinalUpdate = true;
}
TUnitList::iterator it, itEnd = GetUnitsList().end();
// check if covering units are not getting too far due to the enemy moving
if(m_bInitialized)
{
Vec3 avgEnemyPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();
float maxdist2 = m_fApproachDistance+2*m_CApproachStep;
maxdist2 *= maxdist2;
//ActionList fireList;
//ActionList hideList;

//check if at least one of the covering units is too far
bool bApproachGroup = false;
for (it = GetUnitsList().begin(); it != itEnd; ++it)
{
CUnitImg& unit = *it;
if(unit.IsHiding() && unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
{
float fDist2 = (unit.m_pUnit->GetPos() - avgEnemyPos).GetLengthSquared();
if(fDist2 > maxdist2)
{
bApproachGroup = true;
break;
}
}
}

if(bApproachGroup) // make all covering units approach
{
for (it = GetUnitsList().begin(); it != itEnd; ++it)
{
CUnitImg& unit = *it;
if(unit.IsHiding() && unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
{
unit.ClearPlanning(PRIORITY_VERY_HIGH);
CUnitAction* pAction = new CUnitAction(UA_APPROACH,true,m_fApproachDistance+m_CApproachStep);
unit.m_Plan.push_back(pAction);
CUnitAction* actionFire = new CUnitAction(UA_FIRE, true);
CUnitAction* actionHide = new CUnitAction(UA_HIDEAROUND,true, ZERO);
unit.m_Plan.push_back(actionFire);
unit.m_Plan.push_back(actionHide);
//					fireList.push_back( std::make_pair(&unit, actionFire) );
//					holdFireList.push_back( std::make_pair(&unit, actionHide) );
unit.ExecuteTask();

unit.ClearHiding();
}
}

}
}

return ACTION_DONE;
}

bool CLeaderAction_Attack_LeapFrog::FinalUpdate()
{

FRAME_PROFILER( "CLeaderAction_Attack_LeapFrog::FinalUpdate",GetISystem(),PROFILE_AI );


m_timeRunning = GetAISystem()->GetFrameStartTime();
if(m_fApproachDistance <=m_CApproachMinDistance && !m_pLiveTarget)
return false;

TUnitList::iterator it, itEnd = GetUnitsList().end();
// is leader still alive?
//	if (GetAISystem()->m_cvAILeaderDies->GetIVal())
{
CPuppet* pLeaderPuppet = (CPuppet*)m_pLeader->GetAssociation();
if ( pLeaderPuppet && !pLeaderPuppet->IsEnabled())
return false; //end this action if he's dead
}

typedef std::multimap< float, CUnitImg* > DistanceMap;
DistanceMap distanceMap;

Vec3 vAveragePosition = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,UPR_COMBAT_GROUND);

float fAvgDistance = (vAveragePosition-m_pLeader->GetAIGroup()->GetEnemyAveragePosition()).GetLength();

CAIObject* pBeacon=static_cast<CAIObject*>(GetAISystem()->GetBeacon(m_pLeader->GetGroupId()));

for (it = GetUnitsList().begin(); it != itEnd; ++it)
{
CUnitImg& unit = *it;
CAIObject* pUnit = unit.m_pUnit;

if (pUnit->IsEnabled() && (unit.GetProperties() &UPR_COMBAT_GROUND))
{
// this unit is alive
//Vec3 vUnitEnemyPos = m_pLeader->GetAIGroup()->GetEnemyPosition(pUnit);
//float distance = (vUnitEnemyPos - pUnit->GetPos()).GetLength();
float distance = (m_pLeader->GetAIGroup()->GetEnemyAveragePosition() - pUnit->GetPos()).GetLength();
//			if (m_bInitialized)
//			{
CPipeUser *pPipeUser=NULL;
if(	pBeacon && pUnit->CanBeConvertedTo(AIOBJECT_CPIPEUSER, (void**)&pPipeUser))
if (!pPipeUser->IsHostile(pPipeUser->GetAttentionTarget()))
pPipeUser->SetAttentionTarget(pBeacon);
//			}

distanceMap.insert(std::make_pair(-distance, &unit));
}
}


typedef std::list< std::pair< CUnitImg*, CUnitAction* > > ActionList;
ActionList fireList;
ActionList hideList;
ActionList holdFireList;
ActionList fireList2;
ActionList holdFireList2;
ActionList timeoutList;

Vec3 averageCoverPos(ZERO);

// first half should change positions
int halfUnits = distanceMap.size() / 2;
if (!halfUnits)
halfUnits = 1;

// as requested by Friedrich: only one unit can change cover
//if (m_bInitialized && halfUnits > 1)
//	halfUnits = 1;

int i = 0, count=0;
DistanceMap::iterator u, uEnd = distanceMap.end();

float fApproachDistance;
if(m_pLiveTarget)
{
for (u = distanceMap.begin(); u != uEnd; ++u)
if(i++ >= halfUnits)
{
averageCoverPos+=u->second->m_pUnit->GetPos();
count++;
}
if(count)
averageCoverPos /= float(count);
else//should never happen
averageCoverPos = vAveragePosition;

float distCover = floor((m_pLeader->GetAIGroup()->GetEnemyAveragePosition() - averageCoverPos).GetLength());

float fStep = distCover/2;

if(fStep<m_CApproachStep)
fStep = m_CApproachStep;

fApproachDistance = distCover - fStep;

if(fApproachDistance<m_CApproachMinDistance)
fApproachDistance = m_CApproachMinDistance;
}
else
fApproachDistance = m_CApproachMinDistance;


// check if there's no live target and the units are already close enough to attention target (end action)
if(!m_pLiveTarget)
{
i = 0;
float fAvgHalfDist = 0;
int k = 0;
for (u = distanceMap.begin(); u != uEnd ; ++u)
{
if(i>=halfUnits)
{
fAvgHalfDist-= u->first;//distances in distancemap are negative
k++;
}
i++;
}
if(k)
{
fAvgHalfDist/=float(k);
if(fAvgHalfDist <=m_CApproachMinDistance)
return false;
}
}

i = 0;
for (u = distanceMap.begin(); u != uEnd; ++u)
{
CUnitImg& unit = *u->second;
// consider only units with ground combat property
if(!(unit.GetProperties() & UPR_COMBAT_GROUND))
continue;

Vec3 vUnitEnemyPos = m_pLeader->GetAIGroup()->GetEnemyPosition(unit.m_pUnit);
if(vUnitEnemyPos.IsZero())
vUnitEnemyPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();

if(m_pLiveTarget)
{
if (i < halfUnits)
{
// first half should change positions
CUnitAction* actionApproach = new CUnitAction(UA_APPROACH, true,  fApproachDistance);
unit.m_Plan.push_back(actionApproach);
CUnitAction* actionHide = new CUnitAction(UA_HIDEAROUND, true, ZERO);
hideList.push_back( std::make_pair(&unit, actionHide) );
//				hideList.push_back( std::make_pair(&unit, actionApproach) );
unit.ClearHiding();
}
else
{
// other half should provide cover fire
CUnitAction* actionFire = new CUnitAction(UA_FIRE, true);
CUnitAction* actionHide = new CUnitAction(UA_HIDEAROUND,true, ZERO);
if (m_bInitialized)
{
fireList.push_back( std::make_pair(&unit, actionFire) );
holdFireList.push_back( std::make_pair(&unit, actionHide) );
}
else
{
fireList2.push_back( std::make_pair(&unit, actionFire) );
holdFireList2.push_back( std::make_pair(&unit, actionHide) );
}
unit.SetHiding();
}
}
else
{// no live target: make all units get closer
if (i < halfUnits)
{
// first half should approach at distance
CUnitAction* actionApproach = new CUnitAction(UA_APPROACH, true,  fApproachDistance+m_CApproachStep);
unit.m_Plan.push_back(actionApproach);
CUnitAction* actionFire = new CUnitAction(UA_FIRE, true);
CUnitAction* actionHide = new CUnitAction(UA_HIDEAROUND,true, ZERO);
if (m_bInitialized)
{
fireList.push_back( std::make_pair(&unit, actionFire) );
//					fireList.push_back( std::make_pair(&unit, actionApproach) );
holdFireList.push_back( std::make_pair(&unit, actionHide) );
}
else
{
fireList2.push_back( std::make_pair(&unit, actionFire) );
//					fireList2.push_back( std::make_pair(&unit, actionApproach) );
holdFireList2.push_back( std::make_pair(&unit, actionHide) );
}
unit.SetHiding();
}
else
{	
// second half should approach
CUnitAction* actionApproach = new CUnitAction(UA_APPROACH, true,  fApproachDistance);
unit.m_Plan.push_back(actionApproach);
CUnitAction* actionHide = new CUnitAction(UA_HIDEAROUND, true, ZERO);
hideList.push_back( std::make_pair(&unit, actionHide) );
//				hideList.push_back( std::make_pair(&unit, actionApproach) );
unit.ClearHiding();

}
}
++i;
}

if (hideList.empty() && !distanceMap.empty())
{
CUnitImg* unit = distanceMap.begin()->second;
CUnitAction* action = new CUnitAction(UA_TIMEOUT, true);
hideList.push_back( std::make_pair(unit, action) );
}


if (m_bInitialized)
{
BlockActionList(fireList, hideList);
BlockActionList(hideList, holdFireList);
}
else
{
BlockActionList(fireList, hideList);
BlockActionList(fireList2, holdFireList);
BlockActionList(holdFireList, holdFireList2);
}

PushPlanFromList::Do(fireList);
PushPlanFromList::Do(hideList);
PushPlanFromList::Do(timeoutList);
if (!m_bInitialized)
PushPlanFromList::Do(fireList2);
PushPlanFromList::Do(holdFireList);
if (!m_bInitialized)
PushPlanFromList::Do(holdFireList2);

m_bInitialized = true;

Vec3 vEnemyPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();

if(m_fApproachDistance>= m_CApproachMinDistance)
{
m_fApproachDistance -=m_CApproachStep;
if(m_fApproachDistance< m_CApproachMinDistance)
m_fApproachDistance= m_CApproachMinDistance;
}

//	if (m_bNoTarget)// || !m_pLiveTarget)
//		return false;

return true; 

}
*/
CLeaderAction::eActionUpdateResult CLeaderAction_Attack_LeapFrog::Update()
{
	CPuppet* pLeaderPuppet = (CPuppet*)m_pLeader->GetAssociation();
	if ( pLeaderPuppet && !pLeaderPuppet->IsEnabled())
		return ACTION_FAILED; //end this action if he's dead

	int unitCount = m_pLeader->GetAIGroup()->GetUnitCount(UPR_COMBAT_GROUND);
	if(!unitCount)
		return ACTION_FAILED;

	IAIObject* pLiveTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true,true);


	if(pLiveTarget)
	{
		m_timeRunning = GetAISystem()->GetFrameStartTime();
		Vec3 vAveragePositionWLiveTarget = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,UPR_COMBAT_GROUND|UPR_COMBAT_RECON);
		m_pLeader->MarkPosition(vAveragePositionWLiveTarget);
	}

	Vec3 vAveragePosition = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,UPR_COMBAT_GROUND);
	Vec3 vEnemyAveragePosition  = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();

	//HideLeader(vAveragePosition,vEnemyAveragePosition,UPR_COMBAT_GROUND);

	TUnitList::iterator it, itEnd = GetUnitsList().end();

	if(!m_bInitialized)
	{
		// form the two groups
		typedef std::multimap< float, CUnitImg* > SideMapT;
		SideMapT sideMap;
		Vec3 vAvgDir = vAveragePosition - vEnemyAveragePosition;
		Vec3 vAvgDirN = vAvgDir.GetNormalizedSafe();
		// check who's on the left and who's on the right
		for (it = GetUnitsList().begin(); it != itEnd; ++it)
		{
			CUnitImg& unit = *it;
			if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
			{
				Vec3 vDir = (unit.m_pUnit->GetPos() - vEnemyAveragePosition);
				vDir.NormalizeSafe();
				Vec3 vCross = vDir.Cross(vAvgDirN);
				sideMap.insert(std::make_pair(vCross.z,&unit));
			}
		}
		int i=0; 
		for(SideMapT::iterator sit = sideMap.begin();sit!=sideMap.end();++sit)
		{
			CUnitImg* pUnit = sit->second;
			if(pUnit->m_pUnit->IsEnabled() && (pUnit->GetProperties() & UPR_COMBAT_GROUND))
			{
				i++;
				pUnit->m_Group = (i*2-1)/unitCount;
			}
		}
		m_bInitialized = true;
	}

	// compute distances of the two groups


	if(m_pLeader->GetActiveUnitPlanCount(UPR_COMBAT_GROUND)==0)
	{
		// all units executed their orders, proceed with next step
		int count[2] = {0,0};
		m_dist[0] = m_dist[1] = 0;
		for (it = GetUnitsList().begin(); it != itEnd; ++it)
		{
			CUnitImg& unit = *it;
			if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
			{
				int group = unit.m_Group;
				m_dist[group]+= (unit.m_pUnit->GetPos() - vEnemyAveragePosition).GetLength();
				count[group]++;
			}
		}
		m_dist[0]= floor(m_dist[0]/float(count[0]));
		m_dist[1]= floor(m_dist[1]/float(count[1]));
		int iFarGroup = m_dist[0]<m_dist[1] ? 1:0;
		int iCloseGroup = 1-iFarGroup;

		if(pLiveTarget)
		{

			if(m_dist[iCloseGroup] > m_CApproachMinDistance)
			{
				m_bApproached = false;
				m_fApproachDistance = m_dist[iCloseGroup]/2;
				if(m_fApproachDistance<m_CApproachMinDistance)
					m_fApproachDistance = m_CApproachMinDistance;
				for (it = GetUnitsList().begin(); it != itEnd; ++it)
				{
					CUnitImg& unit = *it;
					if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
					{
						if(unit.m_Group==iFarGroup)
						{
							AISignalExtraData data;
							data.fValue = m_fApproachDistance;
							data.iValue = 0;
							CUnitAction* pAction = new CUnitAction(UA_APPROACH,true,"",data);
							unit.m_Plan.push_back(pAction);
							/*						}
							else 
							{
							CUnitAction*/ pAction = new CUnitAction(UA_HIDEAROUND,true, ZERO);
							unit.m_Plan.push_back(pAction);
						}
						unit.ExecuteTask();
					}
				}
			}
			else
			{
				if(!m_bApproached)
				{
					for (it = GetUnitsList().begin(); it != itEnd; ++it)
					{
						CUnitImg& unit = *it;
						if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
						{
							CUnitAction* pAction = new CUnitAction(UA_HIDEAROUND,true, ZERO);
							unit.m_Plan.push_back(pAction);
						}
					}
					m_bApproached = true;
				}
				else // units have approached
					return ACTION_DONE;

			}
		}
		else // no live target
		{
			if(m_bNoTarget)
			{
				// no target, close enough to beacon, quit
				return ACTION_DONE;
			}
			else
			{
				// share all the (memory) targets amongst all units
				// this assigns and executes an unit action UA_ACQUIRETARGET to all units
				m_pLeader->AssignTargetToUnits(UPR_COMBAT_GROUND,100);
				// all guys approach their targets
				for (it = GetUnitsList().begin(); it != itEnd; ++it)
				{
					CUnitImg& unit = *it;
					if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & UPR_COMBAT_GROUND))
					{
						AISignalExtraData data;
						data.fValue = -1.0f;
						data.iValue = 0;
						CUnitAction* pAction = new CUnitAction(UA_APPROACH,true,"",data);// -1.f -> approach attention target
						unit.m_Plan.push_back(pAction);
						pAction = new CUnitAction(UA_HIDEAROUND,true, ZERO);
						unit.m_Plan.push_back(pAction);
					}
				}
				m_bNoTarget = true;
			}
		}
		m_bCoverApproaching = false;
	}
	else if(!m_bCoverApproaching)// not all units are idle - no new tasks assigned
	{
		float fDistCover = 0;
		int count = 0;
		int iFarGroup = m_dist[0]<m_dist[1] ? 1:0;
		for (it = GetUnitsList().begin(); it != itEnd; ++it)
		{
			CUnitImg& unit = *it;
			if(unit.m_pUnit->IsEnabled() && unit.m_Group == iFarGroup && (unit.GetProperties() & UPR_COMBAT_GROUND))
			{
				fDistCover+= (unit.m_pUnit->GetPos() - vEnemyAveragePosition).GetLength();
				count++;
			}
		}
		fDistCover /=float(count);

		// check if covering units are not getting too far due to the enemy moving
		if(fDistCover>m_CMaxDistance)
		{
			float fCloseGroupDist = min(m_dist[0],m_dist[1]);
			float fNewDistance = fCloseGroupDist+m_CApproachStep;

			for (it = GetUnitsList().begin(); it != itEnd; ++it)
			{
				CUnitImg& unit = *it;
				if(unit.m_pUnit->IsEnabled() && unit.GetProperties() & UPR_COMBAT_GROUND && unit.m_Group == iFarGroup)
				{
					AISignalExtraData data;
					data.fValue = fNewDistance;
					data.iValue = 0;
					CUnitAction* pAction = new CUnitAction(UA_APPROACH,true,"",data);
					unit.m_Plan.push_back(pAction);
					pAction = new CUnitAction(UA_HIDEAROUND,true, ZERO);
					unit.m_Plan.push_back(pAction);
					unit.ExecuteTask();
				}		
			}
			m_bCoverApproaching = true;
		}
	}

	return ACTION_RUNNING;
}

//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_LeapFrog::FinalUpdate()
{
	return false; 
}
//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_LeapFrog::TimeOut()
{
	return true;
}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction_Attack_LeapFrog::~CLeaderAction_Attack_LeapFrog()
{
	TUnitList::iterator it, itEnd = GetUnitsList().end();
	for (it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CUnitImg& unit = *it;
		unit.ClearHiding();
	}
}

//-----------------------------------------------------------
//----  ATTACK FRONT
//-----------------------------------------------------------


CLeaderAction_Attack_Front::CLeaderAction_Attack_Front(CLeader* pLeader, const LeaderActionParams& params)
{
	m_pLeader = pLeader;
	m_eActionType = LA_ATTACK;
	m_eActionSubType = LAS_ATTACK_FRONT;
	//	m_pSubAction = new CLeaderAction_Hide(pLeader);
	m_unitProperties = UPR_COMBAT_GROUND;
	m_timeLimit = params.fDuration;
	m_timeRunning = GetAISystem()->GetFrameStartTime();
	m_bInitialized = false;
	m_vBasePoint = params.vPoint;
	m_fSize = params.fSize;
	m_bTimerRunning = false;
	m_bInvestigating = false;
	m_vUpdateEnemyPos = ZERO;
	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		unitItr->ClearHiding();

	//m_pLeader->AssignTargetToUnits(UPR_COMBAT_RECON,2);

}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction_Attack_Front::~CLeaderAction_Attack_Front()
{
	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		unitItr->ClearHiding();
	m_pLeader->ReleaseFormation();	
}

//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_Front::ProcessSignal(const AISIGNAL& signal)
{
	// 	if(!strcmp(signal.strText, "OnLastKnownPositionApproached") ) 
	if (signal.Compare(g_crcSignals.m_nOnLastKnownPositionApproached))
	{
		m_vUpdateEnemyPos = ZERO; // force update
		return true;
	}
	return false;
}

//
//----------------------------------------------------------------------------------------------------

CLeaderAction::eActionUpdateResult CLeaderAction_Attack_Front::Update()
{
	int numUnits = m_pLeader->GetAIGroup()->GetUnitCount(UPR_COMBAT_GROUND);
	if(numUnits==0)
		return ACTION_FAILED;

	Vec3 averagePos = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES, UPR_COMBAT_GROUND);
	Vec3 enemyPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();
	IAIObject* pBeacon = GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
	if(enemyPos.IsZero())
	{
		if(pBeacon)
			enemyPos = pBeacon->GetPos();
	}
	bool bMove =false;
	if((m_vUpdateEnemyPos -enemyPos).GetLength()>3)
	{
		bMove = true;
		m_vUpdateEnemyPos = enemyPos;
	}

	if(!m_bTimerRunning)
		m_timeRunning = GetAISystem()->GetFrameStartTime();

	CAIObject* pEnemyLeader = m_pLeader->GetEnemyLeader();

	IAIObject* pLiveTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true,true);

	Vec3 dir = enemyPos - averagePos; //(m_vBasePoint.IsZero() ? averagePos : m_vBasePoint);

	if(bMove) 
	{
		m_bInvestigating = false;
		m_timeRunning = GetAISystem()->GetFrameStartTime();
		Vec3 dirToEnemy = enemyPos - (m_vBasePoint.IsZero() ? averagePos : m_vBasePoint);
		m_fInitialDistance = dirToEnemy.GetLength();
		float fScale = 1;

		if(enemyPos.IsZero() )
		{	// no enemy position, no one to attack
			return ACTION_RUNNING;
		}
		CFormation* pFormation =NULL;
		if(!m_bInitialized)
		{
			if(!m_pLeader->CreateFormation_FrontRandom(enemyPos,fScale,numUnits,false,pEnemyLeader,UPR_COMBAT_GROUND,static_cast<CAIObject*>(GetAISystem()->GetBeacon(m_pLeader->GetGroupId()))))
				return ACTION_FAILED;
			// force update for correct positioning, in case the owner won't move to reach his destination
			pFormation = m_pLeader->GetFormationOwner()->m_pFormation;
			pFormation->Update(/*m_pLeader->GetFormationOwner()*/);
			m_bInitialized = true;
			m_vEnemyPos = enemyPos;
		}

		pFormation = m_pLeader->GetFormationOwner()->m_pFormation;
		if(!pFormation)
			return ACTION_FAILED;

		m_numUnitsBehind = 0;

		TUnitList::iterator itEnd = GetUnitsList().end();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg& unit = *unitItr;
			if (unit.GetProperties() & UPR_COMBAT_GROUND)
			{
				unit.ClearPlanning();
				unit.ClearFollowing();
				if (unit.m_pUnit->IsEnabled() &&(unit.GetProperties() & UPR_COMBAT_GROUND))
				{
					CAIObject* pFormationPoint =  pFormation->GetFormationPoint(unit.m_pUnit);
					if(pFormationPoint)
					{
						Vec3 v1 = unit.m_pUnit->GetPos() - enemyPos;
						Vec3 v2 = pFormationPoint->GetPos() - enemyPos;
						if(v1.Dot(v2)<0)// unit is behind the enemy
						{ // make them pass aside the enemies
							AISignalExtraData data;
							data.iValue = 1;
							CUnitAction* pAction = new CUnitAction(UA_SIGNAL, true, "ORDER_MOVE",data);
							unit.m_Plan.push_back(pAction);
							unit.SetFollowing();
							m_numUnitsBehind++;
						}
					}

					CUnitAction* pAction = new CUnitAction(UA_FORM, true, PRIORITY_HIGH);
					unit.m_Plan.push_back(pAction);
					//					pAction = new CUnitAction(UA_FORM, true, PRIORITY_HIGH);
					//				unit.m_Plan.push_back(pAction);
					pAction = new CUnitAction(UA_HIDEAROUND, true,(pFormationPoint ? pFormationPoint->GetPos():ZERO));
					unit.m_Plan.push_back(pAction);
					unit.ExecuteTask();
				}
			}
		}
	}
	else // not move
	{
		if(!pLiveTarget && !m_bInvestigating && !enemyPos.IsZero())
		{
			Vec3 tmpPos = dir.GetNormalizedSafe();
			CAIActor* pSelectedActor = m_pLeader->GetAIGroup()->GetForemostUnit(tmpPos,UPR_COMBAT_GROUND)->CastToCAIActor();
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pSelectedActor);
			if(itUnit != GetUnitsList().end())
			{
				itUnit->ClearPlanning();
				Vec3 lookDir = m_pLeader->GetAIGroup()->GetEnemyAverageDirection(false,false);
				CUnitAction* pAction = new CUnitAction(UA_MOVE, true,enemyPos);
				pAction->m_Direction = lookDir;
				itUnit->m_Plan.push_back(pAction);
				pAction = new CUnitAction(UA_SIGNAL_LEADER, true,"OnLastKnownPositionApproached");
				itUnit->ExecuteTask();
			}
			m_bInvestigating = true;
		}
	}

	Vec3 vCurrentDir = enemyPos - averagePos;

	vCurrentDir.z = 0; //2d only
	vCurrentDir.NormalizeSafe();

	if(pLiveTarget)
	{
		Vec3 vAveragePositionWLiveTarget = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,UPR_COMBAT_GROUND|UPR_COMBAT_RECON);
		m_pLeader->MarkPosition(vAveragePositionWLiveTarget);
	}

	//update reference points - put them at the sides of the enemies
	Vec3 vEnemyMove = enemyPos - m_vEnemyPos;
	m_vEnemyPos = enemyPos;
	if(vEnemyMove.IsZero())
		vEnemyMove = m_pLeader->GetAIGroup()->GetEnemyAverageDirection(true,false);
	vEnemyMove.NormalizeSafe();
	Vec3 vX(vEnemyMove.y, -vEnemyMove.x,0);//2D only
	Vec3 Foremost1 = m_pLeader->GetAIGroup()->GetForemostEnemyPosition(vX);
	Vec3 vXNeg = -vX;
	Vec3 Foremost2 = m_pLeader->GetAIGroup()->GetForemostEnemyPosition(vXNeg);
	float fWidth = (Foremost2 - Foremost1).GetLength();
	if(fWidth<3)
		fWidth += 3;
	else fWidth=2;

	Vec3 point1 = Foremost1 + vX*fWidth;
	Vec3 point2 = Foremost2 - vX*fWidth;
	int count1=0;
	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg& unit = *unitItr;
		if (unit.m_pUnit->IsEnabled() &&(unit.GetProperties() & UPR_COMBAT_GROUND)
			&& unit.IsFollowing())// this unit is behind
		{
			CPuppet* pPuppet = unit.m_pUnit->CastToCPuppet();
			if(pPuppet)
			{
				if((point1 - pPuppet->GetPos()).GetLengthSquared() < (point2 - pPuppet->GetPos()).GetLengthSquared() 
					&& count1 <m_numUnitsBehind/2)
				{
					pPuppet->SetRefPointPos(point1);
					count1++;
				}
				else
					pPuppet->SetRefPointPos(point2);
			}
		}

	}

	return ACTION_RUNNING;
}

//-----------------------------------------------------------
//----  ATTACK CHAIN
//-----------------------------------------------------------


CLeaderAction_Attack_Chain::CLeaderAction_Attack_Chain(CLeader* pLeader)
{
	m_pLeader = pLeader;
	m_unitProperties = UPR_COMBAT_GROUND ;
	m_timeLimit = 5; // wait this time before switching to search, when nobody has a live target
	m_bNoTarget = false;
	m_eActionType = LA_ATTACK;
	m_eActionSubType = LAS_ATTACK_CHAIN;
	m_vUpdateEnemyPos = ZERO;
	m_bInitialized = false;
	
	m_timeRunning = GetAISystem()->GetFrameStartTime();
	m_pLeader->AssignTargetToUnits(UPR_COMBAT_RECON,2);

}

//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_Chain::FinalUpdate()
{
	return false; 
}
//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_Chain::TimeOut()
{
	return true;
}

bool CLeaderAction_Attack_Chain::IsStatusExpired() const
{
	return (GetAISystem()->GetFrameStartTime().GetSeconds() >= m_nextStatusTime && m_nextStatusTime>0);
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_Chain::DeadUnitNotify(CAIActor* pUnit)
{
	CLeaderAction::DeadUnitNotify(pUnit);
	if(pUnit ==m_pLeader->GetFormationOwner())
	{
		m_bInitialized = false;
		m_vUpdateEnemyPos = ZERO;

	}
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_Chain::UpdateBeacon() const 
{
	if(!m_vNearestEnemyPos.IsZero())
	{
		GetAISystem()->UpdateBeacon(m_pLeader->GetGroupId(), m_vNearestEnemyPos, m_pLeader);
		Vec3 vEnemyDir = m_pLeader->GetAIGroup()->GetEnemyAverageDirection(true,false);
		if(!vEnemyDir.IsZero())
		{
			IAIObject *pBeacon = GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
			pBeacon->SetBodyDir(vEnemyDir);
			pBeacon->SetMoveDir(vEnemyDir);
		}
	}
	else
		CLeaderAction::UpdateBeacon();

}

//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_Chain::ProcessSignal(const AISIGNAL& signal)
{
	//if(!strcmp(signal.strText, "OnEndApproach") ) 
	if (signal.Compare(g_crcSignals.m_nOnEndApproach))
	{
		SetStatus(STATUS_ALIGNED,3,5);

		// try to align all the positions towards the enemy
		CAIObject* pFormationOwner = m_pLeader->GetFormationOwner();
		if(!pFormationOwner)
			return true;
		Vec3 ownerPos = pFormationOwner->GetPos();
		Vec3 vDir = (ownerPos - m_vNearestEnemyPos) ;
		vDir.NormalizeSafe();

		TUnitList::iterator itEnd = GetUnitsList().end();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg& unit = *unitItr;
			if ((unit.GetProperties() & UPR_COMBAT_GROUND) && unit.m_pUnit!=pFormationOwner)
			{
				CAIObject* pFormationPoint = m_pLeader->GetFormationPoint(unit.m_pUnit);
				if(pFormationPoint)
				{
					Vec3 vFormPoint = pFormationPoint->GetPos();
					Vec3 vFormDir = (vFormPoint - ownerPos);
					float fFormDist = vFormDir.GetLength();
					float alignedDistance = (vFormDir/fFormDist).Dot(vDir) * fFormDist;
					Vec3 newPoint = ownerPos + vDir*alignedDistance;
					newPoint.z = vFormPoint.z;
					if(GetAISystem()->IsPathWorth(unit.m_pUnit,unit.m_pUnit->GetPos(),newPoint,2))
					{
						CUnitAction* pAction = new CUnitAction(UA_MOVE, true,newPoint);
						unit.m_Plan.push_back(pAction);
					}
				}
				CUnitAction* pAction = new CUnitAction(UA_FIRE, true);
				unit.m_Plan.push_back(pAction);
				unit.ExecuteTask();
			}
		}

		return true;
	}
	return false;
}
//
//----------------------------------------------------------------------------------------------------

void CLeaderAction_Attack_Chain::SetStatus(TActionStatus status,float durationMin, float durationMax)
{
	m_status = status;
	if(durationMin>0)
	{
		if(durationMax < durationMin)
			durationMax = durationMin;
		m_nextStatusTime = GetAISystem()->GetFrameStartTime().GetSeconds() + durationMin + Random()*(durationMax-durationMin);
	}
	else
		m_nextStatusTime = -1;
}
//
//----------------------------------------------------------------------------------------------------

CLeaderAction::eActionUpdateResult CLeaderAction_Attack_Chain::Update()
{
	CPuppet* pLeaderPuppet = (CPuppet*)m_pLeader->GetAssociation();
	if ( pLeaderPuppet && !pLeaderPuppet->IsEnabled())
		return ACTION_FAILED; 

	int numUnits = m_pLeader->GetAIGroup()->GetUnitCount(UPR_COMBAT_GROUND);
	if(numUnits==0)
		return ACTION_FAILED;


	Vec3 averagePos = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES, UPR_COMBAT_GROUND);
	Vec3 enemyPos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();
	IAIObject* pBeacon = GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
	if(enemyPos.IsZero())
	{
		if(pBeacon)
			enemyPos = pBeacon->GetPos();
	}

	Vec3 tmpPos;

	// get closest enemy position
	tmpPos = averagePos - enemyPos;
	m_vNearestEnemyPos = m_pLeader->GetAIGroup()->GetForemostEnemyPosition(tmpPos);

	if(!m_bInitialized)
	{
		tmpPos = enemyPos - averagePos;
		CAIObject* pNearestUnit = m_pLeader->GetAIGroup()->GetForemostUnit(tmpPos,UPR_COMBAT_GROUND);
		Vec3 vMinDist = m_vNearestEnemyPos - pNearestUnit->GetPos();
		if(vMinDist.Dot(enemyPos - averagePos) <0)
		{
			// some enemy is behind some unit
			return ACTION_FAILED;
		}
		if(!m_pLeader->CreateFormation_LineFollow(m_vNearestEnemyPos,m_CDistanceStep,numUnits,false,pNearestUnit,NULL,UPR_COMBAT_GROUND))
			return ACTION_FAILED;
		m_bInitialized = true;
		m_vUpdateEnemyPos = ZERO;

	}

	if((m_vUpdateEnemyPos -enemyPos).GetLength()>3)
	{
		SetStatus(STATUS_APPROACH);
		m_vUpdateEnemyPos = enemyPos;
		Vec3 dir =enemyPos - averagePos ;
		float fScale = 1;

		if(enemyPos.IsZero() )
		{	// no enemy position, no one to attack
			return ACTION_FAILED;
		}

		CAIObject* pAILeader = static_cast<CAIObject*>(m_pLeader->GetAssociation());
		CFormation* pFormation = m_pLeader->GetFormationOwner()->m_pFormation;

		if(!pFormation)
			return ACTION_FAILED;

		CUnitAction* pApproachAction = NULL;
		TUnitList::iterator itEnd = GetUnitsList().end();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg& unit = *unitItr;
			if(unit.m_pUnit==m_pLeader->GetFormationOwner())
			{
				AISignalExtraData data;
				data.fValue = m_CDistanceHead;
				data.iValue = 0;
				pApproachAction = new CUnitAction(UA_APPROACH,true,"",data);
				unit.m_Plan.push_back(pApproachAction);
				CUnitAction* pAction = new CUnitAction(UA_SIGNAL_LEADER,true,"OnEndApproach");
				unit.m_Plan.push_back(pAction);
				unit.ExecuteTask();
				CAIObject* pFormationPoint = pFormation->GetFormationPoint(unit.m_pUnit);
				pAction = new CUnitAction(UA_HIDEAROUND, true,(pFormationPoint ? pFormationPoint->GetPos():ZERO));
				unit.m_Plan.push_back(pAction);
				break;
			}
		}
		AIAssert(pApproachAction);

		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg& unit = *unitItr;
			if (unit.GetProperties() & UPR_COMBAT_GROUND)
			{
				if(unit.m_pUnit!=m_pLeader->GetFormationOwner())
				{
					CUnitAction* pAction = new CUnitAction(UA_FORM, true, PRIORITY_HIGH);
					unit.m_Plan.push_back(pAction);
					//		pAction->BlockedBy(pApproachAction);
				}
				unit.ExecuteTask();

			}
		}
	}
	else
	{
		// enemy hasn't moved too much
		if(IsStatusExpired())
		{
			TActionStatus oldStatus = m_status;
			SetStatus(STATUS_ALIGNED);
			switch(oldStatus)
			{
			case STATUS_ALIGNED: // time to strafe
				{
					if(numUnits >1)
					{

						typedef std::map<CAIActor*,Vec3> TObjectVectorList;
						TObjectVectorList StrafeLeftDir;
						TObjectVectorList StrafeRightDir;
						for(int i=1;i<numUnits;i++)
						{
							CAIActor* pUnitActor = m_pLeader->GetFormationOwner()->m_pFormation->GetPointOwner(i)->CastToCAIActor();
							TUnitList::iterator it = m_pLeader->GetAIGroup()->GetUnit(pUnitActor);
							if(it!=GetUnitsList().end())
							{
								StrafeLeftDir.insert(std::make_pair(pUnitActor,StrafeVector(pUnitActor,-1)));
								StrafeRightDir.insert(std::make_pair(pUnitActor,StrafeVector(pUnitActor,1)));
							}
						}
						TObjectVectorList::iterator itL = StrafeLeftDir.begin();
						TObjectVectorList::iterator itR = StrafeRightDir.begin();
						CAIActor* pUnitLeft = NULL;
						CAIActor* pUnitRight = NULL;
						int unitCount = 0;

						int firstUnitStrafing = 1+ ai_rand()%(numUnits-1);
						Vec3 vStrafeLeft(0,0,0), vStrafeRight(0,0,0);

						for(	;itL != StrafeLeftDir.end() ; ++itL)
						{
							CAIActor* pUnit = itL->first; // assuming that it's equal to itR->first;
							Vec3 vMyStrafeLeft = itL->second;
							bool bCanStrafeLeft = !vMyStrafeLeft.IsZero();
							if(bCanStrafeLeft)
							{
								if(unitCount<firstUnitStrafing )
								{
									pUnitLeft = pUnit;
									vStrafeLeft = vMyStrafeLeft;
								}
							}
							unitCount++;
						}

						unitCount=0;
						for(	;itR != StrafeRightDir.end() ; ++itR)
						{
							CAIActor* pUnit = itR->first; 
							Vec3 vMyStrafeRight = itR->second;
							bool bCanStrafeRight = !vMyStrafeRight.IsZero();
							if(bCanStrafeRight)
							{
								if(unitCount<firstUnitStrafing && pUnit!=pUnitLeft)
								{
									pUnitRight = pUnit;
									vStrafeRight = vMyStrafeRight;
								}
							}
							unitCount++;
						}

						if(pUnitLeft || pUnitRight)
						{
							SetStatus(STATUS_STRAFE,2,4);
							if(pUnitLeft)
							{
								TUnitList::iterator it = m_pLeader->GetAIGroup()->GetUnit(pUnitLeft);
								if(it!=GetUnitsList().end())
								{
									CUnitImg& unit = *it;
									CUnitAction* pAction = new CUnitAction(UA_MOVE,true,pUnitLeft->GetPos()+vStrafeLeft);
									unit.m_Plan.push_back(pAction);
									pAction = new CUnitAction(UA_FIRE,true);
									unit.m_Plan.push_back(pAction);
									pAction = new CUnitAction(UA_TIMEOUT,true,Random()*2+2);
									unit.m_Plan.push_back(pAction);
									pAction = new CUnitAction(UA_MOVE,true,pUnitLeft->GetPos());
									unit.m_Plan.push_back(pAction);
									pAction = new CUnitAction(UA_FIRE,true);
									unit.m_Plan.push_back(pAction);
								}
							}
							if(pUnitRight)
							{
								TUnitList::iterator it = m_pLeader->GetAIGroup()->GetUnit(pUnitRight);
								if(it!=GetUnitsList().end())
								{
									CUnitImg& unit = *it;
									CUnitAction* pAction = new CUnitAction(UA_MOVE,true,pUnitRight->GetPos()+vStrafeRight);
									unit.m_Plan.push_back(pAction);
									pAction = new CUnitAction(UA_FIRE,true);
									unit.m_Plan.push_back(pAction);
									pAction = new CUnitAction(UA_TIMEOUT,true,Random()*2+2);
									unit.m_Plan.push_back(pAction);
									pAction = new CUnitAction(UA_MOVE,true,pUnitRight->GetPos());
									unit.m_Plan.push_back(pAction);
									pAction = new CUnitAction(UA_FIRE,true);
									unit.m_Plan.push_back(pAction);
								}
							}
						}
					}
				}
				break;
			case STATUS_STRAFE: // time to strafe
				SetStatus(STATUS_ALIGNED,3,5);
				break;
			default:
				break;
			}
		}
	}
	if(m_pLeader->GetAIGroup()->GetAttentionTarget(true))//only hostile targets
		m_timeRunning = GetAISystem()->GetFrameStartTime();
	else if ((GetAISystem()->GetFrameStartTime() - m_timeRunning).GetSeconds()> m_timeLimit)
	{
		return ACTION_DONE;
	}
	return ACTION_RUNNING;
}

//
//---------------------------------------------------------------------------
Vec3 CLeaderAction_Attack_Chain::StrafeVector(const CAIActor* pUnit,float dirCoeff) const 
{
	CAIObject* pFormationOwner = m_pLeader->GetFormationOwner();
	Vec3 vOwnerUnitDir = (pUnit->GetPos() - pFormationOwner->GetPos());
	float fDistanceToOwner = vOwnerUnitDir.GetLength();
	if(fDistanceToOwner==0)
		return ZERO;
	vOwnerUnitDir /= fDistanceToOwner; // normalize

	Vec3 vEnemyOwnerDir = (pFormationOwner->GetPos() - m_vNearestEnemyPos).GetNormalizedSafe();
	if(vEnemyOwnerDir.Dot(vOwnerUnitDir)<0.9) // unit is misaligned already, no need to strafe
		return ZERO;

	float fStrafeLength = fDistanceToOwner/3;
	if(fStrafeLength<2)
		fStrafeLength = 2;
	Vec3 vStrafeDir = dirCoeff * fStrafeLength * Vec3(vOwnerUnitDir.y,-vOwnerUnitDir.x,0); //2D only
	if(GetAISystem()->IsPathWorth(pUnit,pUnit->GetPos(),pUnit->GetPos()+vStrafeDir,2))
		return vStrafeDir;
	return ZERO;
}

//-----------------------------------------------------------
//----  ATTACK COORDINATED FIRE
//-----------------------------------------------------------

CLeaderAction_Attack_CoordinatedFire::CLeaderAction_Attack_CoordinatedFire(CLeader* pLeader, const LeaderActionParams& params)
{
	m_eActionSubType = params.subType;
	m_unitProperties = params.unitProperties;

	m_eActionType = LA_ATTACK;
	m_updateStatus = ACTION_RUNNING;
	m_pLeader = pLeader;

	AISignalExtraData* data = new AISignalExtraData();
	data->nID = params.id; 
	data->iValue = m_eActionSubType;
	IEntity* pTargetEntity = GetAISystem()->m_pSystem->GetIEntitySystem()->GetEntity(params.id.n);
	if(!pTargetEntity)
	{
		m_updateStatus = ACTION_FAILED;
		return;
	}

	CAIActor* pLeaderActor = m_pLeader->GetAssociation()->CastToCAIActor();

	Vec3 avgPos = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,m_unitProperties);

	const float CminDistance  = 6.0f;
	const float CRearDistance = 3.0f;
	Vec3 targetDir = avgPos - pTargetEntity->GetWorldPos();
	float fTargetDist = targetDir.GetLength();
	float fNewTargetDist = max(fTargetDist - CRearDistance,CminDistance);
	targetDir *= fNewTargetDist/fTargetDist;
	Vec3 leaderPos = pTargetEntity->GetWorldPos() + targetDir;

	TUnitList::iterator itLeaderUnit = m_pLeader->GetAIGroup()->GetUnit(pLeaderActor);

	CUnitAction* pLeaderMoveAction = new CUnitAction(UA_MOVE,true,leaderPos);
	itLeaderUnit->m_Plan.push_back(pLeaderMoveAction);
	itLeaderUnit->ExecuteTask();

	CUnitAction* pLeaderFireAction = new CUnitAction(UA_SIGNAL,true,"ORDER_COORDINATED_FIRE1",*data);
	itLeaderUnit->m_Plan.push_back(pLeaderFireAction);


	int rearUnitsCount = m_pLeader->GetAIGroup()->GetUnitCount(m_unitProperties);

	if(itLeaderUnit != GetUnitsList().end())
	{
		if(itLeaderUnit->GetProperties() & m_unitProperties)
			rearUnitsCount --;
	}
	else
	{
		m_updateStatus = ACTION_FAILED;
		return;
	}

	if(rearUnitsCount<2)
	{
		m_updateStatus = ACTION_FAILED;
		return;
	}

	TUnitList::iterator itEnd = GetUnitsList().end();
	/*
	std::vector<TUnitList::iterator> RearUnits;

	float totalAngle = gf_PI;
	Vec3 zAxis(0,0,1);
	Vec3 rearUnitDir = (targetDir.GetNormalizedSafe()).GetRotated(zAxis,-totalAngle/2);
	rearUnitDir*=CRearDistance;

	float angleStep = totalAngle/float(rearUnitsCount-1);

	for(int i=0; i<rearUnitsCount; i++)
	{
	Vec3 pos = avgPos + rearUnitDir;
	float minDistance = 600000;
	TUnitList::iterator itSelected = itEnd;
	for(TUnitList::iterator it = GetUnitsList().begin();it!=itEnd;++it)
	{
	CUnitImg& unit = *it;
	if(it != itLeaderUnit && unit.m_pUnit->IsEnabled() && (unit.GetProperties() & m_unitProperties) )
	{
	if (std::find(RearUnits.begin(),RearUnits.end(),it) == RearUnits.end())
	{
	float dist = Distance::Point_Point(pos,unit.m_pUnit->GetPos());
	if(dist < minDistance)
	{
	dist = minDistance;
	itSelected = it;
	}
	}
	}
	}
	if(itSelected!=itEnd)
	{
	// check reachability of flank position		
	Vec3 vDistance = leaderPos - pos;
	CAIObject* pUnit = itSelected->m_pUnit;
	CStandardHeuristic heuristic;
	AgentPathfindingProperties unitNavProperties = pUnit->GetMovementAbility().pathfindingProperties;

	unitNavProperties.exposureFactor = 0;// clear exposure factor for heuristic

	float maxCost = CRearDistance*1.2;

	float radius = pUnit->GetParameters().m_fPassRadius;
	//static float radius = 0.3f;

	IAISystem::tNavCapMask navmask = IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN ;
	static const AgentPathfindingProperties navProperties(
	navmask, 
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 
	5.0f, 10000.0f, -10000.0f, 0.0f, 20.0f, 0.0);

	heuristic.SetRequesterProperties(CPathfinderRequesterProperties(radius, unitNavProperties), pUnit);
	Vec3 actualPos = GetAISystem()->GetGraph()->GetBestPosition(heuristic, maxCost, pos, leaderPos, pUnit->m_pLastNavNode, unitNavProperties.navCapMask );//IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN);

	CUnitAction* pMoveAction = new CUnitAction(UA_MOVE,true,actualPos);
	itSelected->m_Plan.push_back(pMoveAction);
	pLeaderFireAction->BlockedBy(pMoveAction);
	itSelected->ExecuteTask();
	CUnitAction* pFireAction = new CUnitAction(UA_SIGNAL,true,signalTxt,*data);
	itSelected->m_Plan.push_back(pFireAction);

	RearUnits.push_back(itSelected);
	}

	rearUnitDir = rearUnitDir.GetRotated(zAxis,angleStep);
	}
	*/

	// move only the leader unit
	for(TUnitList::iterator it = GetUnitsList().begin();it!=itEnd;++it)
	{
		CUnitImg& unit = *it;
		if(it != itLeaderUnit && unit.m_pUnit->IsEnabled() && (unit.GetProperties() & m_unitProperties) )
		{
			CUnitAction* pFireAction = new CUnitAction(UA_SIGNAL,true,"ORDER_COORDINATED_FIRE1",*data);
			it->m_Plan.push_back(pFireAction);
			it->ExecuteTask();
			pLeaderFireAction->BlockedBy(pFireAction);
		}
	}
}

//--------------------------------------------------------------------------

void CLeaderAction_Attack_CoordinatedFire::UpdateBeacon() const
{
	// leave the beacon where it was 
}

//--------------------------------------------------------------------------
bool CLeaderAction_Attack_CoordinatedFire::ProcessSignal(const AISIGNAL& signal)
{
	//	if(!strcmp(signal.strText, "OnAbortAction") ) 
	if (signal.Compare(g_crcSignals.m_nOnAbortAction))
	{
		m_updateStatus = ACTION_FAILED;
		return true;
	}
	else if (signal.Compare(g_crcSignals.m_nOnActionCompleted)) /*if(!strcmp(signal.strText, "OnActionCompleted") ) */
	{
		m_updateStatus = ACTION_DONE;
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------
CLeaderAction::eActionUpdateResult CLeaderAction_Attack_CoordinatedFire::Update()
{
	return m_updateStatus;
}

//-----------------------------------------------------------
//----  ATTACK USE SPOTS
//-----------------------------------------------------------

CLeaderAction_Attack_UseSpots::CLeaderAction_Attack_UseSpots(CLeader* pLeader, const LeaderActionParams& params)
{
	m_eActionSubType = params.subType;
	m_unitProperties = params.unitProperties;

	m_eActionType = LA_ATTACK;
	m_pLeader = pLeader;
	m_timeLimit = params.fDuration;
	m_lastApproachTargetTime = GetAISystem()->GetFrameStartTime();
	m_lastSpotFoundTime = GetAISystem()->GetFrameStartTime();
	m_lastTimeWithTarget = GetAISystem()->GetFrameStartTime();
	m_bRequestForSpot = false;
	m_bInitialized = false;
	m_bApproachingTarget = true;
	m_gotoPos = ZERO;
}

//--------------------------------------------------------------------------

void CLeaderAction_Attack_UseSpots::UpdateBeacon() const
{
	// leave the beacon where it was 
}

//--------------------------------------------------------------------------
bool CLeaderAction_Attack_UseSpots::ProcessSignal(const AISIGNAL& signal)
{
	//	if(!strcmp(signal.strText, "OnNoPathFound") ) 
	if (signal.Compare(g_crcSignals.m_nOnNoPathFound))
	{
		m_bRequestForSpot = true;
		m_bApproachingTarget = false;
		m_bPathFindDone = true;
		return true;
	}
	else if (signal.Compare(g_crcSignals.m_nOnApproachEnd)) /*if(!strcmp(signal.strText, "OnApproachEnd") ) */
	{
		m_bApproachingTarget = false;
		return true;
	}
	return false;
}



//--------------------------------------------------------------------------
CLeaderAction::eActionUpdateResult CLeaderAction_Attack_UseSpots::Update()
{
	//
	CTimeValue currentTime = GetAISystem()->GetFrameStartTime();
	Vec3 avgPos = m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,m_unitProperties);

	IAIObject* pTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true,true);
	if(pTarget)
	{
		Vec3 targetPos = pTarget->GetPos();

		if(!m_bApproachingTarget && !m_bRequestForSpot && (avgPos -targetPos).GetLength() > m_CApproachDistance*1.6)
		{	// player has moved from when he was originally approached, redo all 
			m_bInitialized = false;
		}
		else if((currentTime - m_lastSpotFoundTime).GetSeconds() >5 && !m_bApproachingTarget)
			m_bRequestForSpot = true;
	}

	if(!m_bInitialized || m_bRequestForSpot 
		||(currentTime - m_lastApproachTargetTime).GetSeconds()> 10)
	{
		bool bMove = false;

		if(pTarget)
		{
			m_lastTimeWithTarget = currentTime;
			if(!m_bRequestForSpot) // it's an approach to target
				m_lastApproachTargetTime = currentTime;
			Vec3 targetPos = pTarget->GetPos();

			Vec3 gotoPos = (m_bRequestForSpot? m_pLeader->GetBestShootSpot(targetPos) : targetPos);

			if(gotoPos ==m_gotoPos && m_bInitialized) // same target as before, don't change it
				return ACTION_RUNNING;

			m_gotoPos = gotoPos;

			if(!gotoPos.IsZero())
			{

				if((targetPos - avgPos).GetLengthSquared() > (targetPos - gotoPos).GetLengthSquared())
					bMove = true;
				else if(m_bInitialized)
					return ACTION_RUNNING;

				CUnitAction* pApproachAction=NULL;

				if(bMove)
				{
					// find closest unit 
					/*					float minDist2 = 2570601.494f;
					CAIObject* pOwner=NULL;
					CUnitImg* pOwnerUnit = NULL;
					for(TUnitList::iterator itUnit = GetUnitsList().begin(); itUnit!=GetUnitsList().end();++itUnit)
					{
					CUnitImg& unit = *itUnit;
					CAIObject* pUnit = unit.m_pUnit;

					if(pUnit->IsEnabled() && (unit.GetProperties() & m_unitProperties))
					{	
					float dist2 = (pUnit->GetPos() - gotoPos).GetLengthSquared();
					if(dist2 < minDist2)
					{
					dist2 = minDist2;
					pOwner = pUnit;
					pOwnerUnit = &unit;
					}
					}
					}
					if(!pOwner)
					return ACTION_FAILED;
					*/	
					//					pOwnerUnit->m_Plan.push_back(pApproachAction);
					m_bPathFindDone = false;
					m_pLeader->ClearAllPlannings(m_unitProperties);

					int numUnits = m_pLeader->GetAIGroup()->GetUnitCount(m_unitProperties);

					if(!m_pLeader->CreateFormation_LineFollow(targetPos,2,numUnits,false,NULL/*pOwner*/,NULL,m_unitProperties))
						return ACTION_FAILED;

					if(m_bRequestForSpot)
					{ // approach shoot spot
						pApproachAction = new CUnitAction(UA_MOVE,true,gotoPos);
						pApproachAction->m_fDistance = 2;//fValue for speed
					}
					else
					{ // approach
						AISignalExtraData data;
						data.fValue = m_CApproachDistance;
						data.iValue = 0;
						pApproachAction = new CUnitAction(UA_APPROACH,true,"",data);
						m_bApproachingTarget = true;
					}

					if(!pApproachAction)
						return ACTION_FAILED;	

					CAIActor* pActor = m_pLeader->GetFormationOwner()->CastToCAIActor();

					TUnitList::iterator itOwner = m_pLeader->GetAIGroup()->GetUnit(pActor);
					if(itOwner!=GetUnitsList().end())
						itOwner->m_Plan.push_back(pApproachAction);
					else
					{//should never happen
						delete pApproachAction;
						return ACTION_FAILED;
					}
				}

				m_bRequestForSpot = false;

				TUnitList::iterator itEnd = GetUnitsList().end();
				for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
				{
					CUnitImg& unit = *unitItr;
					CAIObject* pUnit = unit.m_pUnit;
					if(pUnit->IsEnabled() && (unit.GetProperties() & m_unitProperties))
					{
						CUnitAction* pFireAction = new CUnitAction(UA_FIRE,true);
						if(bMove)
						{
							if(pUnit!=m_pLeader->GetFormationOwner())
							{
								CUnitAction* pFormAction = new CUnitAction(UA_FORM,true);
								unit.m_Plan.push_back(pFormAction);
								if(pApproachAction)
									pFireAction->BlockedBy(pApproachAction);
							}
						}
						unit.m_Plan.push_back(pFireAction);
						unit.ExecuteTask();
					}
				}

			}
		}
		m_bInitialized = true;
	}

	if((currentTime - m_lastTimeWithTarget).GetSeconds()> 10)
		return ACTION_DONE;
	return ACTION_RUNNING;
}


//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack::EndAttackAction( const char* signalText, uint32 unitCountMask )
{

	CAIObject* pAILeader = static_cast<CAIObject*>(m_pLeader->GetAssociation());
	if(pAILeader)
	{
		IAIObject* pTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true);
		Vec3 vEnemyPos =m_pLeader->GetAIGroup()->GetEnemyPositionKnown();
		Vec3 vAveragePos =m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES, unitCountMask);
		AISignalExtraData* pData = new AISignalExtraData;
		pData->point = vEnemyPos;
		if(pTarget)
		{
			pData->SetObjectName(pTarget->GetName());
			if(pTarget->IsAgent())
			{
				if(pTarget->GetEntity())
					pData->nID = pTarget->GetEntityID();
			}

			pData->fValue = (vEnemyPos - vAveragePos).GetLength();
			pData->iValue = m_eActionSubType;

		}
		GetAISystem()->SendSignal(SIGNALFILTER_SENDER,1,signalText,pAILeader,pData);
	}
}


void CLeaderAction_Attack::HideLeader(const Vec3& avgPos, const Vec3& enemyPos, uint32 unitProp)
{
	CAIObject* pAILeader = (CAIObject*)(m_pLeader->GetAssociation());
	if(!pAILeader || !pAILeader->IsEnabled())
		return;

	float fMinDist=1000000;
	CAIObject* pSelectedUnit = NULL;

	// take the closest unit to average position (most central one)
	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg& unit = *unitItr;
		if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & unitProp))
		{ 
			float fDist = (avgPos - unit.m_pUnit->GetPos()).GetLengthSquared() ;
			if (fDist< fMinDist)
			{
				float fDistEnemy = (enemyPos - unit.m_pUnit->GetPos()).GetLengthSquared() ;
				if(fDistEnemy>5)// check if the central unit is not too close
				{
					pSelectedUnit = unit.m_pUnit;
					fMinDist = fDist;
				}
			}
		}
	}

	if(!pSelectedUnit)
	{ // the central unit was too close to the enemy,  take the farthest unit from enemy
		float fMaxDist=0;

		TUnitList::iterator itEnd = GetUnitsList().end();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg& unit = *unitItr;
			if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & unitProp))
			{ 
				float fDist = (enemyPos - unit.m_pUnit->GetPos()).GetLengthSquared() ;
				if (fDist> fMaxDist)
				{
					pSelectedUnit = unit.m_pUnit;
					fMaxDist = fDist;
				}
			}
		}
	}

	if(pSelectedUnit)
	{
		CPuppet* pPuppet = pAILeader->CastToCPuppet();
		if (pPuppet)
		{
			Vec3 vDir = pSelectedUnit->GetPos() - enemyPos;
			//			int building;
			//		IVisArea *pArea;
			//			AgentPathfindingProperties unitNavProperties = pAILeader->GetMovementAbility().pathfindingProperties;
			//			if (GetAISystem()->CheckNavigationType(pSelectedUnit->GetPos(),building,pArea,unitNavProperties.navCapMask & (IAISystem::NAV_VOLUME | IAISystem::NAV_WAYPOINT_3DSURFACE)) == 0)
			vDir.z=0;
			vDir.NormalizeSafe();
			Vec3 vPos  = pSelectedUnit->GetPos() + 3*vDir;
			pPuppet->SetRefPointPos(vPos);
		}
	}
}


void CLeaderAction_Follow::SetUnitFollow(CUnitImg& unit)
{
	/*	if(m_pLeader->IsPlayer())
	{
	CUnitAction* action = new CUnitAction(UA_FORM,true, PRIORITY_HIGH);
	unit.m_Plan.push_back(action);
	}
	*/
	CUnitAction* action = new CUnitAction(UA_FOLLOW,true);
	unit.m_Plan.push_back(action);
	unit.ExecuteTask();
	unit.ClearFar();
	unit.m_lastMoveTime = GetAISystem()->GetFrameStartTime();
	unit.SetFollowing();

}

/*
CLeaderAction_FollowHiding::~CLeaderAction_FollowHiding()
{
TUnitList::iterator it, itEnd = GetUnitsList().end();
for (it = GetUnitsList().begin(); it != itEnd; ++it)
{
it->ClearFollowing();
it->ClearHiding();
m_pLeader->m_pObstacleAllocator->FreeObstacle(it->m_pUnit);
}
}
*/
CLeaderAction_Follow::~CLeaderAction_Follow()
{
	TUnitList::iterator it, itEnd = GetUnitsList().end();
	for (it = GetUnitsList().begin(); it != itEnd; ++it)
		it->m_flags = 0;
}

/*
void CLeaderAction_FollowHiding::SetUnitFollow(CUnitImg& unit)
{

CAIObject* pUnit = unit.m_pUnit;
Vec3 hidePoint;

CAIObject* myFormationPoint = m_pLeader->GetFormationPoint(pUnit);
if(myFormationPoint)
m_pLeader->ForcePreferedPos(myFormationPoint->GetPos());
else
m_pLeader->ForcePreferedPos(m_averagePos);

Vec3 preferedPos = m_pLeader->GetPreferedPos();
Vec3 enemyPos;
if(m_pLeader->GetAlertStatus()<=AIALERTSTATUS_UNSAFE)
enemyPos = preferedPos;
else
enemyPos = m_pLeader->GetAIGroup()->GetEnemyPositionKnown();

CObstacleRef myObstacle  = m_pLeader->m_pObstacleAllocator->GetUnitObstacle(pUnit);
CObstacleRef newObstacle;

//	bool bNeedNewObstacle = false;

//	if(!myObstacle || (pUnit->GetPos() - preferedPos).GetLengthSquared() > m_CSearchDistance*m_CSearchDistance)
{
newObstacle = m_pLeader->m_pObstacleAllocator->ReallocUnitObstacle(unit.m_pUnit, preferedPos, true,-1,preferedPos,false);
//		bNeedNewObstacle = true;
}

if(bool(newObstacle) && myObstacle != newObstacle )
{
if(m_pLeader->GetAlertStatus()>AIALERTSTATUS_UNSAFE)
hidePoint = m_pLeader->GetHidePoint(unit.m_pUnit, newObstacle);
else
hidePoint = m_pLeader->GetHoldPoint( newObstacle,m_averagePos);
unit.ClearFollowing();

unit.ClearPlanning(PRIORITY_NORMAL);
CUnitAction* action = new CUnitAction(UA_HIDEFOLLOW, true,hidePoint);
unit.m_Plan.push_back(action);

unit.ExecuteTask();

//			GetAISystem()->SendSignal(0,10,"ORDER_FOLLOW_HIDE",unit.m_pUnit,&AISIGNAL_EXTRA_DATA(hidePoint));
//unit.m_Plan.push_back(new CUnitAction(UA_HIDEFOLLOW, true, hidePoint));
}
else 
{
if(bool(myObstacle) && (myObstacle.GetPos() - preferedPos).GetLengthSquared()< m_CSearchDistance*m_CSearchDistance*2)
{
// stick to old obstacle for a while; TO DO: walk along the obstacle's edge
}
else if(!unit.m_bFollowing)
{
// stick to formation point
//if(m_pLeader->IsPlayer())
//{
//	CUnitAction*	action = new CUnitAction(UA_FORM,true);
//	unit.m_Plan.push_back(action);
//}
CUnitAction*	action = new CUnitAction(UA_FOLLOW,true);
unit.m_Plan.push_back(action);
unit.ExecuteTask();
unit.SetFollowing();
}
}

m_pLeader->ForcePreferedPos(ZERO);

unit.ClearFar();

}

*/

CLeaderAction_Follow::CLeaderAction_Follow(CLeader* pLeader, const LeaderActionParams& params): CLeaderAction(pLeader),
m_vAverageLeaderMovingDir(ZERO), m_vTargetPos(params.vPoint)
{

	m_eActionType = LA_FOLLOW;
	if(params.name.empty())
		m_szFormationName = m_pLeader->GetFormationDescriptor();// m_szFormationDescriptor.c_str();
	else
		m_szFormationName = params.name;

	m_iGroupID = int(params.fSize);
	m_pSingleUnit = params.pAIObject;
	m_bInitialized = false;

}


bool CLeaderAction_Follow::ProcessSignal(const AISIGNAL& signal)
{
	//	if(!strcmp(signal.strText, "OnEndFollow") ) 
	if (signal.Compare(g_crcSignals.m_nOnEndFollow))
	{
		IEntity* pEntity = (IEntity*)signal.pSender;
		if(pEntity)
		{
			CAIActor* pUnit = CastToCAIActorSafe(pEntity->GetAI());

			if(pUnit)
			{
				pUnit->SetSignal(1,"OnEndFollow",0,0, g_crcSignals.m_nOnEndFollow);
				TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pUnit);
				if(itUnit != GetUnitsList().end())
				{
					itUnit->ClearPlanning(PRIORITY_HIGH);
					itUnit->ClearFollowing();
					if(m_pLeader->GetFormationOwner())
					{
						CFormation* pFormation =  m_pLeader->GetFormationOwner()->m_pFormation;
						if(pFormation)
						{
							pFormation->FreeFormationPoint(pUnit);
						}
					}
				}
			}
		}
		return true;
	}
	return false;
}

void CLeaderAction_Follow::BusyUnitNotify(CUnitImg& BusyUnit)
{

}

void CLeaderAction_Follow::ResumeUnit(CUnitImg& unit)
{
	unit.ClearPlanning();
	CAIObject* pUnit = unit.m_pUnit;
	if(!pUnit)
		return;
	CAIObject* myFormationPoint = m_pLeader->GetFormationPoint(pUnit);
	if(!myFormationPoint)
		myFormationPoint = m_pLeader->GetNewFormationPoint(pUnit, unit.GetClass());
	SetUnitFollow(unit);
}

void CLeaderAction_Follow::CheckLeaderFiring() 
{
	CAIActor* pAILeader = CastToCAIActorSafe(m_pLeader->GetAssociation());
	if(!m_bLeaderFiring && pAILeader)
	{
		if(pAILeader->GetProxy()) 
		{
			SAIBodyInfo bodyinfo;
			pAILeader->GetProxy()->QueryBodyInfo(bodyinfo);
			if(bodyinfo.isFiring)// to do: is the weapon actually firing? (charge time)
			{
				Vec3	fireDir(pAILeader->GetState()->vShootTargetPos - pAILeader->GetFirePos());
				fireDir.NormalizeSafe();
				float mindist = 10;
				Vec3 pointStart(pAILeader->GetPos());
				Vec3 pointEnd(pAILeader->GetPos()+50*(m_pLeader->IsPlayer() ? pAILeader->GetViewDir() : fireDir));
				// check if leader is actually aiming someone
				CAIGroup::TSetAIObjects::const_iterator itEnd  = m_pLeader->GetAIGroup()->GetTargets().end();
				for(CAIGroup::TSetAIObjects::const_iterator it = m_pLeader->GetAIGroup()->GetTargets().begin(); it!=itEnd;++it)
				{
					const CAIObject* pTarget = *it;
					if(pTarget)
					{
						Vec3 junk;
						float dist = Distance::Point_Line(pTarget->GetPos(), pointStart, pointEnd, junk);
						if(dist<4)
						{
							m_bLeaderFiring = true;
							GetAISystem()->SendSignal(SIGNALFILTER_GROUPONLY,1,"OnLeaderFiringAtEnemy",pAILeader);
							m_FiringTime = GetAISystem()->GetFrameStartTime();
							return;
						}
					}
				}
			}
		}
	}
	else if((GetAISystem()->GetFrameStartTime() - m_FiringTime).GetSeconds() > 3) 
		m_bLeaderFiring = false;
}

void CLeaderAction_Follow::CheckLeaderNotMoving() 
{
	// Check if some unit is too back 
	const float timelimit = 15;
	CAIActor* pFormatonOwner = m_pLeader->GetFormationOwner()->CastToCAIActor();
	if(!pFormatonOwner)
		return;

	float speed2=0;
	pe_status_dynamics  dSt;
	CPuppet *pPuppet;
	Vec3 velocity(ZERO);

	if(pFormatonOwner && pFormatonOwner->IsAgent() && pFormatonOwner->GetProxy() && pFormatonOwner->GetProxy()->GetPhysics())
	{
		pFormatonOwner->GetProxy()->GetPhysics()->GetStatus( &dSt );
		velocity = dSt.v;

		speed2 = dSt.v.GetLengthSquared();
	}
	bool bPlayerMoving = false;
	if(speed2>0.1)
	{
		m_groupMovingTime = GetAISystem()->GetFrameStartTime();
		m_movingTime = m_groupMovingTime ;
		bPlayerMoving = true;
	}
	else 
	{
		TUnitList::iterator itEnd = GetUnitsList().end();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CAIObject* pUnit = (*unitItr).m_pUnit;
			pPuppet = CastToCPuppetSafe(pUnit);
			if(pPuppet && pPuppet->GetState()->fDesiredSpeed >0.1) 
			{
				m_movingTime = GetAISystem()->GetFrameStartTime();
				return;
			}
		}
	}
	if((GetAISystem()->GetFrameStartTime() - m_groupMovingTime).GetSeconds() >timelimit)
	{
		m_groupMovingTime = GetAISystem()->GetFrameStartTime();
		m_notMovingCount++;
		IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
		if(pData)
			pData->iValue = m_notMovingCount;
		GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP,1,"OnGroupStop",pFormatonOwner,pData);
		return;		
	}

	float dist2 = Distance::Point_PointSq(pFormatonOwner->GetPos(),m_vLastUpdatePos);

	bool bPlayerBigMoving = dist2 > m_CUpdateDistance*m_CUpdateDistance;

	CAIObject* pAILeader = static_cast<CAIObject*>(m_pLeader->GetAssociation());

	if(!bPlayerMoving) 
	{
		if ((GetAISystem()->GetFrameStartTime() - m_movingTime).GetSeconds()> 2 && m_bLeaderMoving)
		{
			m_bLeaderMoving = false;
			m_bLeaderBigMoving = false;
			// check if the player is kind of hiding (not indoor)
			bool bLeaderHideSpotSearched = false;
			bool bLeaderHideSpotFound = false;
			Vec3 leaderHidePos(ZERO), hideDir(ZERO);

			typedef std::list<Vec3> TListVectors;
			TListVectors HidePositions;
			HidePositions.push_back(pFormatonOwner->GetPos());
			Vec3 hidePosBehindLeader = pFormatonOwner->GetPos();
			TUnitList::iterator it, itend = m_pLeader->GetAIGroup()->GetUnits().end();

			IAIObject* pGroupTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true);
			Vec3 hideFrom = pGroupTarget ? pGroupTarget->GetPos() : 
			(!m_vAverageLeaderMovingDir.IsEquivalent(ZERO) ? m_vAverageLeaderMovingDir : pFormatonOwner->m_pFormation->GetMoveDir())*10 + pFormatonOwner->GetPos();

			for(it = m_pLeader->GetAIGroup()->GetUnits().begin();it!=itend;++it)
			{
				CUnitImg& unit = *it;
				CPuppet* pPuppet = unit.m_pUnit->CastToCPuppet();
				if (pPuppet)
				{
					if(!bLeaderHideSpotSearched)
					{
						if(m_currentNavType != IAISystem::NAV_WAYPOINT_HUMAN && m_currentNavType != IAISystem::NAV_WAYPOINT_3DSURFACE)
						{
							IAIObject* pLastOp = pPuppet->GetLastOpResult();
							pPuppet->SetLastOpResult(pFormatonOwner);

							leaderHidePos = pPuppet->FindHidePoint(4,hideFrom,HM_NEAREST+HM_AROUND_LASTOP,m_currentNavType,true);

							bLeaderHideSpotFound = pPuppet->m_CurrentHideObject.IsValid() && Distance::Point_Point2D(leaderHidePos,pFormatonOwner->GetPos())<1.5f + pPuppet->m_CurrentHideObject.GetObjectRadius();

							if(bLeaderHideSpotFound )
							{
								Vec3 hideObjectPos = pPuppet->m_CurrentHideObject.GetObjectPos();
								hideDir = pFormatonOwner->GetPos() - hideObjectPos;
								hideDir.z = 0;
								if(hideDir.IsEquivalent(ZERO))
									hideDir = -pFormatonOwner->GetViewDir();
								else
									hideDir.Normalize();
								hideDir *= 1.5f;
								//								hidePos = pFormatonOwner->GetPos() + hideDir;
								if(!pGroupTarget)
									GetAISystem()->UpdateBeacon(m_pLeader->GetGroupId(),hideFrom,pFormatonOwner);
							}
							pPuppet->SetLastOpResult((CAIObject*)pLastOp);
						}
						bLeaderHideSpotSearched = true;
					}
					IAISignalExtraData* pData=NULL;
					if(bLeaderHideSpotFound)
					{
						bool bNearHidePos = false;
						/*	Vec3 hidePos = pPuppet->FindHidePoint(5,hideFrom,HM_NEAREST_TO_ME+HM_AROUND_LASTOP,m_currentNavType,true);

						TListVectors::iterator itPEnd = HidePositions.end();
						// check if the found hidepoint is too close to others 

						if(pPuppet->m_CurrentHideObject.IsValid())
						for(TListVectors::iterator itP = HidePositions.begin(); !bNearHidePos && itP != itPEnd;++itP)
						bNearHidePos = (Distance::Point_Point2D(*itP,hidePos)<1.3f);
						if(bNearHidePos || !pPuppet->m_CurrentHideObject.IsValid())
						*/
						//{
						hidePosBehindLeader += hideDir;
						Vec3 hidePos = hidePosBehindLeader;
						//}
						//						HidePositi:(ons.push_back(hidePos);
						pData = GetAISystem()->CreateSignalExtraData();
						pData->point = hidePos;
						//						hidePos +=hideDir;
					}				
					pPuppet->SetSignal(1,"OnLeaderStop",pAILeader->GetEntity(),pData, g_crcSignals.m_nOnLeaderStop);

				}
			}
		}
	}
	else
	{
		m_vAverageLeaderMovingDir = (m_vAverageLeaderMovingDir + velocity.GetNormalizedSafe())/2;

		m_movingTime = GetAISystem()->GetFrameStartTime();
		if(bPlayerBigMoving)
		{
			if(pFormatonOwner->m_pFormation)
			{
				pFormatonOwner->m_pFormation->SetUpdate(true);
				pFormatonOwner->m_pFormation->Update();
			}

			CheckNavType(pFormatonOwner,true);
		}
	}

	if(bPlayerBigMoving)
	{
		if(!m_bLeaderBigMoving)
		{
			m_bLeaderMoving = true;
			m_bLeaderBigMoving = true;
			if(pAILeader)
				GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP,1,"OnLeaderMoving",pAILeader);
			//		UpdateUnitPositions();
		}
		m_vLastUpdatePos = pFormatonOwner->GetPos();
	}
}


void CLeaderAction_Follow::CheckUnitsNotMoving() 
{
	TUnitList::iterator it, itEnd = GetUnitsList().end();
	CTimeValue now = GetAISystem()->GetFrameStartTime();
	Vec3 interestPos(ZERO);
	bool bPointOfInterestSearched = false;
	CAIObject* pFormatonOwner = m_pLeader->GetFormationOwner();
	if(!pFormatonOwner)
		return;

	for (it = GetUnitsList().begin(); it != itEnd; ++it)
	{
		CUnitImg& unit = *it;
		CAIObject* pUnit = unit.m_pUnit;
		CPuppet* pPuppet = pUnit->CastToCPuppet();
		if (pPuppet)
		{
			if(!pPuppet->m_Path.Empty())
			{
				unit.m_lastMoveTime = now;
				if(!unit.IsFollowing())
					pPuppet->SetSignal(1,"OnUnitMoving", 0, 0, g_crcSignals.m_nOnUnitMoving);
				unit.SetFollowing();
			}
			else if ((now - unit.m_lastMoveTime).GetSeconds() > 2 && unit.IsFollowing())
			{
				m_bLeaderMoving = false;
				m_bLeaderBigMoving = false;
				// check if the player is kind of hiding (not indoor)
				bool bLeaderHideSpotSearched = false;
				bool bLeaderHideSpotFound = false;
				Vec3 leaderHidePos(ZERO), hideDir(ZERO);

				typedef std::list<Vec3> TListVectors;
				TListVectors HidePositions;
				HidePositions.push_back(pFormatonOwner->GetPos());
				Vec3 hidePosBehindLeader = pFormatonOwner->GetPos();
				TUnitList::iterator it, itend = m_pLeader->GetAIGroup()->GetUnits().end();

				IAIObject* pGroupTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true);
				Vec3 hideFrom = pGroupTarget ? pGroupTarget->GetPos() : 
				(!m_vAverageLeaderMovingDir.IsEquivalent(ZERO) ? m_vAverageLeaderMovingDir : pFormatonOwner->m_pFormation->GetMoveDir())*10 + pFormatonOwner->GetPos();

				for(it = m_pLeader->GetAIGroup()->GetUnits().begin();it!=itend;++it)
				{
					IAISignalExtraData* pData=NULL;
					CUnitImg& unit = *it;
					CAIObject* pUnit = unit.m_pUnit;
					unit.ClearFollowing();
					CPuppet* pPuppet = CastToCPuppetSafe(pUnit);
					if(pPuppet)
					{
						if(!bLeaderHideSpotSearched)
						{
							if(m_currentNavType != IAISystem::NAV_WAYPOINT_HUMAN && m_currentNavType != IAISystem::NAV_WAYPOINT_3DSURFACE)
							{
								IAIObject* pLastOp = pPuppet->GetLastOpResult();
								pPuppet->SetLastOpResult(pFormatonOwner);

								leaderHidePos = pPuppet->FindHidePoint(4,hideFrom,HM_NEAREST+HM_AROUND_LASTOP,m_currentNavType,true);

								bLeaderHideSpotFound = pPuppet->m_CurrentHideObject.IsValid() && Distance::Point_Point2D(leaderHidePos,pFormatonOwner->GetPos())<1.5f + pPuppet->m_CurrentHideObject.GetObjectRadius();

								if(bLeaderHideSpotFound )
								{
									Vec3 hideObjectPos = pPuppet->m_CurrentHideObject.GetObjectPos();
									hideDir = pFormatonOwner->GetPos() - hideObjectPos;
									hideDir.z = 0;
									if(hideDir.IsEquivalent(ZERO))
										hideDir = -pFormatonOwner->GetViewDir();
									else
										hideDir.Normalize();
									hideDir *= 1.5f;
									//								hidePos = pFormatonOwner->GetPos() + hideDir;
									if(!pGroupTarget)
										GetAISystem()->UpdateBeacon(m_pLeader->GetGroupId(),hideFrom,pFormatonOwner);
								}
								pPuppet->SetLastOpResult((CAIObject*)pLastOp);
							}
							bLeaderHideSpotSearched = true;
						}
						if(bLeaderHideSpotFound)
						{
							hidePosBehindLeader += hideDir;
							Vec3 hidePos = hidePosBehindLeader;
							pData = GetAISystem()->CreateSignalExtraData();
							pData->point = hidePos;
							pData->iValue = m_currentNavType;
						}				
						else
						{
							pData = GetAISystem()->CreateSignalExtraData();
							pData->iValue = m_currentNavType;
							IAIObject* pTarget = pPuppet->GetAttentionTarget();
							if(!pUnit->IsHostile(pTarget))
							{
								// select a point to look at
								if(!bPointOfInterestSearched)
								{
									// to do: look at next objective position if there are no enemies near
									interestPos= m_pLeader->GetAIGroup()->GetEnemyAveragePosition();
									if(Distance::Point_PointSq(pUnit->GetPos(),interestPos) > 900)
										interestPos = pUnit->GetPos() - 5*pFormatonOwner->GetViewDir();
									bPointOfInterestSearched = true;
								}
								pData->point2 = interestPos;
							}
							else if(pTarget && pTarget->GetEntity())
								pData->nID = pTarget->GetEntityID();

						}
					}
					CAIActor* pUnitActor = CastToCAIActorSafe(pUnit);
					if(pUnitActor)
						pUnitActor->SetSignal(1,"OnUnitStop",pUnit->GetEntity(),pData, g_crcSignals.m_nOnUnitStop);
				}

			}
		}

	}
}


CLeaderAction::eActionUpdateResult CLeaderAction_Follow::Update()
{

	if(!m_bInitialized)
	{

		int iMyGroupId = m_pLeader->GetGroupId();
		if(m_iGroupID<0)
			m_iGroupID = iMyGroupId;
		if(m_iGroupID == iMyGroupId)
			m_pLeader->LeaderCreateFormation(m_szFormationName.c_str(),m_vTargetPos);
		else 
			m_pLeader->JoinFormation(m_iGroupID);

		int i=0;
		if(m_pLeader->IsPlayer())
		{
			//first formation point, assuming that it's not follow type and it's the foremost one
			CAIObject* pPlayerFormPoint = m_pLeader->GetNewFormationPoint(static_cast<CAIObject*>(m_pLeader->GetAssociation()));
			if(!pPlayerFormPoint)
				return ACTION_FAILED; // Error: no formation point assigned to the player
		}

		CAIActor* pFormationOwner = m_pLeader->GetFormationOwner()->CastToCAIActor();

		SetTeamNavProperties();
		CheckNavType(pFormationOwner,false);

		CTimeValue now = GetAISystem()->GetFrameStartTime();

		TUnitList::iterator itEnd = GetUnitsList().end();
		TUnitList::iterator firstUnit;

		if(m_pSingleUnit)
		{
			CAIActor* pSingleUnitActor = m_pSingleUnit->CastToCAIActor();
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pSingleUnitActor);
			if(itUnit != itEnd)
				SetUnitFollow(*itUnit);
		}
		else
		{
			for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr, i++)
			{
				if(unitItr->m_pUnit!=pFormationOwner )
				{
					CUnitImg &curUnit = (*unitItr);

					CUnitAction*	action = new CUnitAction(UA_FOLLOW,true);
					curUnit.m_Plan.push_back(action);

					curUnit.ExecuteTask();
					curUnit.ClearFar();

					curUnit.m_lastMoveTime = now;
					curUnit.SetFollowing();
				}
			}
		}
		m_bLeaderFiring = false;
		m_bLeaderMoving = false;
		m_bLeaderBigMoving = false;
		m_movingTime = GetAISystem()->GetFrameStartTime();
		m_groupMovingTime = m_movingTime;
		m_notMovingCount = 0;

		CAIObject* pAILeader = static_cast<CAIObject*>(m_pLeader->GetAssociation());
		m_vLastUpdatePos = pAILeader ? pAILeader->GetPos() : ZERO;

		pe_status_dynamics  dSt;
		Vec3 velocity(ZERO);
		if(pAILeader && pAILeader->GetProxy() && pAILeader->GetProxy()->GetPhysics())
		{
			pAILeader->GetProxy()->GetPhysics()->GetStatus( &dSt );
			velocity = dSt.v;
			if(!velocity.IsEquivalent(ZERO))
			{
				m_vAverageLeaderMovingDir = velocity.GetNormalizedSafe();
				m_bLeaderMoving = true;
				m_bLeaderBigMoving = true;
			}
		}

		m_bInitialized = true;
	}

	CAIObject* pAILeader = static_cast<CAIObject*>(m_pLeader->GetAssociation());
	if(!pAILeader || (pAILeader && !pAILeader->IsEnabled()))
	{
		return ACTION_DONE;
	}
	CAIActor* pActor = m_pLeader->GetFormationOwner()->CastToCAIActor();

	CheckNavType(pActor,true);
	CheckLeaderFiring();
	CheckLeaderDistance();
	CheckLeaderNotMoving();
	CheckUnitsNotMoving();

	return ACTION_RUNNING;
}
/*
CLeaderAction_FollowHiding::CLeaderAction_FollowHiding(CLeader* pLeader, CUnitImg* pSingleUnit)
{
m_pLeader = pLeader;
m_eActionType = LA_FOLLOW;

m_pSingleUnit = pSingleUnit;
m_lastLeaderUpdatePos = ZERO;

m_bLeaderFiring = false;
m_movingTime = GetAISystem()->GetFrameStartTime();
m_notMovingCount = 0;

}
//--------------------------------------------------------------------------

CLeaderAction::eActionUpdateResult CLeaderAction_FollowHiding::Update()
{

m_pLeader->m_pObstacleAllocator->SetSearchDistance(m_CSearchDistance, false);

m_averagePos = m_pLeader->GetAIGroup()->GetAveragePosition();

CAIObject * pAILeader = static_cast<CAIObject*>(m_pLeader->GetAssociation());
if(!pAILeader)
return ACTION_FAILED;
if(Distance::Point_PointSq(m_lastLeaderUpdatePos, pAILeader->GetPos()) < m_CUpdateThreshold*m_CUpdateThreshold)
return ACTION_RUNNING;
m_lastLeaderUpdatePos = pAILeader->GetPos();

if(m_pSingleUnit)
SetUnitFollow(*m_pSingleUnit);
else
{
TUnitList::iterator it, itEnd = GetUnitsList().end();
for (it = GetUnitsList().begin(); it != itEnd; ++it)
{
CUnitImg& unit = *it;
SetUnitFollow(unit);
}
}
CheckLeaderFiring();
CheckLeaderDistance();
CheckLeaderNotMoving();

return ACTION_RUNNING;
}
*/

//--------------------------------------------------------------------------

CLeaderAction_Use::CLeaderAction_Use(CLeader* pLeader, const AISignalExtraData& SignalData): CLeaderAction(pLeader)
{
	m_eActionType =	LA_USE;
	m_Data = SignalData;	
}



CLeaderAction_Use_Vehicle::CLeaderAction_Use_Vehicle(CLeader* pLeader, const AISignalExtraData& SignalData): CLeaderAction_Use(pLeader,  SignalData)
{
	m_eActionType =	LA_USE_VEHICLE;
	/*	SignalData.nID = vehicle id
	SignalData.fValue = if >0, leader is the driver
	SignalData.iValue2 = if 1, driver has entered the veihcle already
	*/
	bool bLeaderDriver = (SignalData.fValue>0);
	if(bLeaderDriver)
		m_pDriver = static_cast<CAIObject*>(m_pLeader->GetAssociation());
	else
	{
		TUnitList::iterator unitItr = GetUnitsList().begin();
		if(unitItr!=GetUnitsList().end())
			m_pDriver = (*unitItr).m_pUnit;
		else
			return;
	}
	// select first unit as driver (if it's not the leader) 
	// TO DO: some better organization to minimize the units' distances from the seats

	m_VehicleID = (EntityId)(SignalData.nID.n);


	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg &curUnit = (*unitItr);
		AISignalExtraData data;
		data.nID = SignalData.nID;
		//data.fValue = (float)(curUnit==m_pDriver);
		data.iValue2 = 1;// request most prioritary seat

		CUnitAction*	action1 = new CUnitAction(UA_SIGNAL, true, PRIORITY_VERY_HIGH, "ORDER_ENTER_VEHICLE", data);
		curUnit.m_Plan.push_back(action1);
		curUnit.ExecuteTask();

	}

	m_vInitPos = (SignalData.iValue2 ? m_pDriver->GetPos() : ZERO);

}

void CLeaderAction_Use_Vehicle::DeadUnitNotify(CAIActor* pUnit)
{
	CLeaderAction::DeadUnitNotify(pUnit);
	if(pUnit==m_pDriver)
		m_pDriver=NULL;
}


CLeaderAction_Use_Vehicle::CLeaderAction_Use_Vehicle(CLeader* pLeader, const CAIObject* pVehicle, const AISignalExtraData& SignalData): CLeaderAction_Use(pLeader)
{
	m_eActionType =	LA_USE_VEHICLE;
	// SignalData.sObjectName = target name (optional)
	// SignalData.point2 = target position
	// SignalData.iValue = goal type
	// SignalData.iValue2 = if 1, driver has entered the veihcle already
	if(!pVehicle)
		return;

	TUnitList::iterator unitItr = GetUnitsList().begin();
	TUnitList::iterator itEnd = GetUnitsList().end();
	if(unitItr==itEnd)
		return;

	// select first unit as driver (if it's not the leader) 
	// TO DO: some better organization to minimize the units' distances from the seats
	// and assign a not-so-random seat to the leader

	m_pDriver = (*unitItr).m_pUnit;

	IEntity* pVehicleEntity = pVehicle->GetEntity();
	if(!pVehicleEntity)
		return;
	ScriptHandle VehicleID;
	pVehicleEntity->GetScriptTable()->GetValue("id",VehicleID);
	m_VehicleID = pVehicleEntity->GetId();
	m_eActionType =	LA_USE;

	AISignalExtraData dataToSend(SignalData);
	dataToSend.nID = VehicleID;

	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg &curUnit = (*unitItr);
		CUnitAction*	action1 = new CUnitAction(UA_SIGNAL, true, PRIORITY_VERY_HIGH, "ORDER_ENTER_VEHICLE", dataToSend);
		curUnit.m_Plan.push_back(action1);
		curUnit.ExecuteTask();

	}

	m_vInitPos = (SignalData.iValue2 ? m_pDriver->GetPos() : ZERO);
}

//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Use_Vehicle::ProcessSignal(const AISIGNAL& signal)
{
	//	if (!strcmp(signal.strText, "OnDriverEntered"))
	if (signal.Compare(g_crcSignals.m_nOnDriverEntered))
	{
		m_vInitPos = m_pDriver->GetPos();
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------
void CLeaderAction_Use_Vehicle::ResumeUnit(CUnitImg& unit)
{
	unit.ClearPlanning();
	AISignalExtraData data ;
	data.nID = m_VehicleID;
	CUnitAction*	action1 = new CUnitAction(UA_SIGNAL, true, PRIORITY_VERY_HIGH, "ORDER_ENTER_VEHICLE", data);
	unit.m_Plan.push_back(action1);
	unit.ExecuteTask();
}


//----------------------------------------------------------------------------------------------------
CLeaderAction::eActionUpdateResult CLeaderAction_Use_Vehicle::Update()
{
	if(!m_pDriver)
		return ACTION_FAILED;

	if(!m_vInitPos.IsZero() && Distance::Point_Point2DSq(m_vInitPos,m_pDriver->GetPos())>16)
	{
		CAIObject* pAILeader = static_cast<CAIObject*>(m_pLeader->GetAssociation());
		if(pAILeader)
		{
			GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP,1,"OnVehicleLeaving",pAILeader);
			m_vInitPos = ZERO;//avoid sending the signal repeatedly
		}
	}
	return ACTION_RUNNING;
}

//----------------------------------------------------------------------------------------------------
CLeaderAction_Use_LeaveVehicle::CLeaderAction_Use_LeaveVehicle(CLeader* pLeader): CLeaderAction(pLeader)
{
	m_eActionType =	LA_USE_VEHICLE;
	TUnitList::iterator itEnd = GetUnitsList().end();
	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg &curUnit = (*unitItr);
		/*	don't create a task, just send a signal - this actually will finish the previous task EnterVehicle
		CUnitAction*	action1 = new CUnitAction(UA_SIGNAL, true, PRIORITY_HIGH, "ORDER_EXIT_VEHICLE");
		curUnit.m_Plan.push_back(action1);
		curUnit.ExecuteTask();
		*/
		CAIActor* pUnitActor = CastToCAIActorSafe(curUnit.m_pUnit);
		if (pUnitActor)
			pUnitActor->SetSignal(0,"ORDER_EXIT_VEHICLE", 0, 0, g_crcSignals.m_nORDER_EXIT_VEHICLE);
	}
}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction_Attack_SwitchPositions::CLeaderAction_Attack_SwitchPositions(CLeader* pLeader, const LeaderActionParams& params)
{
	m_pLeader = pLeader;
	m_eActionType = LA_ATTACK;
	m_eActionSubType = LAS_ATTACK_SWITCH_POSITIONS;
	m_bInitialized = false;
	m_timeLimit = params.fDuration;
	m_timeRunning = GetAISystem()->GetFrameStartTime(); 
	m_sFormationType = params.name;
	m_unitProperties = params.unitProperties;
	// TO DO: make it variable and tweakable from script
	m_fMinDistanceToNextPoint = m_fDefaultMinDistance;
	m_PointProperties.reserve(50);
	InitNavTypeData();
	m_vUpdatePointTarget = ZERO;
	m_fDistanceToTarget = 6;
	m_fMinDistanceToTarget = 3;// below this distance, position will be discarded
	m_pLiveTarget = NULL;

}

void CLeaderAction_Attack_SwitchPositions::UpdateBeaconWithTarget(const CAIObject* pTarget) const 
{
	if(pTarget)
		GetAISystem()->UpdateBeacon(m_pLeader->GetGroupId(), pTarget->GetPos(), m_pLeader);
	else
		CLeaderAction::UpdateBeacon();
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::InitNavTypeData()
{
	m_TargetData.clear();
	TUnitList::iterator itEnd=GetUnitsList().end();
	for(TUnitList::iterator it=GetUnitsList().begin(); it!=itEnd; ++it)
	{
		CUnitImg& unit = *it;
		CAIObject* pUnit = unit.m_pUnit;
		CAIActor* pAIActor = CastToCAIActorSafe(pUnit);
		if(pAIActor)
			m_TargetData.insert(std::make_pair(pAIActor,STargetData()));
	}
}

void CLeaderAction_Attack_SwitchPositions::OnObjectRemoved(CAIObject* pObject)
{

	TMapTargetData::iterator it = m_TargetData.begin(), itEnd = m_TargetData.end();
	for(;it!=itEnd;++it)// maybe this is not necessary
	{
		if(it->second.pTarget == pObject)
			it->second.pTarget = NULL;
	}

	CFormation* pFormation = (m_pLeader->GetFormationOwner() ? m_pLeader->GetFormationOwner()->m_pFormation : NULL);
	int formSize = (pFormation ? pFormation->GetSize(): 0);
	int size = m_PointProperties.size();
	for(int i = formSize; i<size;i++)
	{
		if(m_PointProperties[i].pOwner == pObject)
			m_PointProperties[i].pOwner = NULL;
	}
	const CAIActor* pActor = CastToCAIActorSafe(pObject);
	if(pActor)
	{
		TSpecialActionMap::iterator its = m_SpecialActions.find(pActor);
		if(its != m_SpecialActions.end())
			m_SpecialActions.erase(its);
	}

}
//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::CheckNavType(CUnitImg& unit)
{
	CPipeUser* pMember = unit.m_pUnit->CastToCPipeUser();
	if(!pMember)
		return;
	CAIObject* pTarget = (CAIObject*)pMember->GetAttentionTarget();
	TMapTargetData::iterator it = m_TargetData.find(pMember);

	if(it==m_TargetData.end())
		return;

	STargetData& tData = it->second;

	//if(pTarget == tData.pTarget)
	//	return;

	tData.pTarget = pTarget;

	int building;
	IVisArea *pArea;
	IAISystem::ENavigationType navType = GetAISystem()->CheckNavigationType(pMember->GetPos(),building,pArea,m_NavProperties);
	if(pTarget && pTarget->GetAssociation())
		//kind of cheat, but it's just to let the AI know in which navigation type the enemy went
		if(pTarget->GetSubType()==CAIObject::STP_MEMORY)// || pTarget->GetSubType()==CAIObject::STP_SOUND) 
			pTarget = (CAIObject*)pTarget->GetAssociation();

	IAISystem::ENavigationType targetNavType = (pTarget ? GetAISystem()->CheckNavigationType(pTarget->GetPos(),building,pArea,m_NavProperties) : 
	IAISystem::NAV_UNSET);

	if(navType == IAISystem::NAV_TRIANGULAR || navType == IAISystem::NAV_WAYPOINT_HUMAN)
	{
		if(navType != tData.navType && (tData.navType == IAISystem::NAV_TRIANGULAR || tData.navType == IAISystem::NAV_WAYPOINT_HUMAN))
		{
			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			pData->iValue = navType;
			pData->iValue2 = targetNavType;
			GetAISystem()->SendSignal(SIGNALFILTER_SENDER,1,"OnNavTypeChanged",pMember,pData);
		}
		tData.navType = navType;
	}

	if(targetNavType == IAISystem::NAV_TRIANGULAR || targetNavType == IAISystem::NAV_WAYPOINT_HUMAN) 
	{

		if(targetNavType != tData.targetNavType && tData.pTarget && (tData.targetNavType == IAISystem::NAV_TRIANGULAR || tData.targetNavType == IAISystem::NAV_WAYPOINT_HUMAN))
		{
			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			pData->iValue = navType;
			pData->iValue2 = targetNavType;
			GetAISystem()->SendSignal(SIGNALFILTER_SENDER,1,"OnTargetNavTypeChanged",pMember,pData);
		}
		tData.targetNavType = targetNavType;
	}

}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction_Attack_SwitchPositions::STargetData* CLeaderAction_Attack_SwitchPositions::GetTargetData(CUnitImg& unit)
{
	TMapTargetData::iterator it = m_TargetData.find(unit.m_pUnit);
	if(it!=m_TargetData.end())
		return &(it->second);
	return NULL;
}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction_Attack_SwitchPositions::~CLeaderAction_Attack_SwitchPositions()
{
	m_pLeader->ReleaseFormation();
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::AddUnitNotify(CAIActor* pUnit)
{
	CPipeUser* pPiper = pUnit->CastToCPipeUser();
	if(pPiper)
	{

		CAIObject* pTarget = (CAIObject*)pPiper->GetAttentionTarget();

		UpdatePointList(pTarget);

		TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pPiper);
		if(itUnit!=GetUnitsList().end())
		{
			itUnit->ClearFollowing();
			itUnit->m_TagPoint = ZERO;
			itUnit->SetMoving();
		}
	}
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::UpdatePointList(CAIObject* pTarget)
{

	IEntity* pTargetEntity = NULL;
	if(!pTarget) 
		pTarget = (CAIObject*)m_pLeader->GetAIGroup()->GetAttentionTarget(true,true);
	if(pTarget)
		pTargetEntity= pTarget->GetEntity();
	if(!pTargetEntity )
		return;

	IEntity* pDummyEntity = NULL;
	if(!m_vUpdatePointTarget.IsEquivalent(pTarget->GetPos()))
	{
		m_vUpdatePointTarget = pTarget->GetPos();
		// target has moved, update additional shoot spot list
		QueryEventMap queryEvents;
		GetAISystem()->GetSmartObjects()->TriggerEvent("CheckTargetNear",pTargetEntity,pDummyEntity,&queryEvents);

		CFormation* pFormation = m_pLeader->GetFormationOwner() ? m_pLeader->GetFormationOwner()->m_pFormation : NULL;
		int fsize= pFormation ? pFormation->GetSize(): 0;
		int size = m_PointProperties.size();

		const QueryEventMap::const_iterator itEnd = queryEvents.end();

		TPointPropertiesList::iterator itp = m_PointProperties.begin(), itpEnd = m_PointProperties.end();
		itp +=size;
		QueryEventMap::iterator itFound = queryEvents.end();
		for(; itp!=itpEnd;)						
		{
			bool bFound = false;
			for (QueryEventMap::iterator itq = queryEvents.begin() ; itq != itEnd ; ++itq)
			{
				Vec3 pos;
				const CQueryEvent* pQueryEvent = &itq->second;
				if (pQueryEvent->pRule->pObjectHelper)
					pos = pQueryEvent->pObject->GetHelperPos(pQueryEvent->pRule->pObjectHelper);
				else
					pos = pQueryEvent->pObject->GetPos();

				if(itp->point == pos)
				{
					bFound = true;
					itFound = itq;
					break;
				}
			}		
			if(bFound)
			{
				queryEvents.erase(itFound);
				++itp;
			}
			else
				m_PointProperties.erase(itp++);
		}

		// only not found points in queryEvents now

		for (QueryEventMap::const_iterator it = queryEvents.begin() ; it != itEnd ; ++it)
		{
			const CQueryEvent* pQueryEvent = &it->second;

			Vec3 pos;
			if (pQueryEvent->pRule->pObjectHelper)
				pos = pQueryEvent->pObject->GetHelperPos(pQueryEvent->pRule->pObjectHelper);
			else
				pos = pQueryEvent->pObject->GetPos();

			m_PointProperties.push_back(SPointProperties(pos));
		}

	}

}
//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_SwitchPositions::ProcessSignal(const AISIGNAL& signal)
{
	if (signal.Compare(g_crcSignals.m_nOnFormationPointReached))
	{
		IEntity* pUnitEntity = signal.pSender;
		CAIObject* pUnit;
		if(pUnitEntity && (pUnit = (CAIObject*)(pUnitEntity->GetAI())))
		{
			CAIActor* pAIActor = pUnit->CastToCAIActor();
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pAIActor);
			if(itUnit!=GetUnitsList().end())
				itUnit->SetFollowing();
		}
		return true;
	}
	else if(signal.Compare(g_crcSignals.m_nAIORD_ATTACK))
	{
		if(signal.pEData && static_cast<ELeaderActionSubType>(signal.pEData->iValue) == m_eActionSubType)
		{
			// unit is requesting this tactic which is active already,
			// just give him instructions to go
			IEntity* pUnitEntity = signal.pSender;
			CAIObject* pUnit;
			if(pUnitEntity && (pUnit = (CAIObject*)(pUnitEntity->GetAI())))
			{
				CPipeUser* pPiper = pUnit->CastToCPipeUser();
				if(pPiper)
				{
					CAIObject* pTarget = (CAIObject*)pPiper->GetAttentionTarget();

					UpdatePointList(pTarget);

					TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pPiper);
					if(itUnit!=GetUnitsList().end())
					{
						itUnit->ClearFollowing();
						itUnit->m_TagPoint = ZERO;
						itUnit->SetMoving();
					}
				}
			}
			return true;
		}
	}
	else 	if(signal.Compare(g_crcSignals.m_nOnRequestUpdate))
	{
		IEntity* pUnitEntity = signal.pSender;
		CAIObject* pUnit;
		if(pUnitEntity && (pUnit = (CAIObject*)(pUnitEntity->GetAI())))
		{
			CPipeUser* pPiper = pUnit->CastToCPipeUser();
			if(pPiper)
			{
				CAIObject* pTarget = (CAIObject*)pPiper->GetAttentionTarget();

				UpdatePointList(pTarget);

				TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pPiper);
				if(itUnit!=GetUnitsList().end())
				{
					itUnit->ClearFollowing();
					itUnit->m_TagPoint = ZERO;
					itUnit->SetMoving();

					if(signal.pEData)
					{
						GetBaseSearchPosition(*itUnit,pTarget,signal.pEData->iValue,signal.pEData->fValue);
						if(signal.pEData->iValue2>0)
							itUnit->m_fDistance2 = (float)signal.pEData->iValue2;

					}
				}
			}
		}
		return true;
	}
	else 	if(signal.Compare(g_crcSignals.m_nOnRequestUpdateAlternative))
	{
		IEntity* pUnitEntity = signal.pSender;
		CAIObject* pUnit;
		if(pUnitEntity && (pUnit = (CAIObject*)(pUnitEntity->GetAI())))
		{
			const CPipeUser* pPiper = pUnit->CastToCPipeUser();
			if(pPiper)
			{
				CAIObject* pTarget = (CAIObject*)pPiper->GetAttentionTarget();

				UpdatePointList(pTarget);

				TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pPiper);
				if(itUnit!=GetUnitsList().end())
				{
					itUnit->ClearFollowing();
					itUnit->m_TagPoint = ZERO;
					itUnit->SetMoving();

					if(signal.pEData)
					{
						m_ActorForbiddenSpotManager.AddSpot(pPiper,signal.pEData->point);
						GetBaseSearchPosition(*itUnit,pTarget,signal.pEData->iValue,signal.pEData->fValue);
					}
				}
			}
		}
		return true;
	}
	else 	if(signal.Compare(g_crcSignals.m_nOnClearSpotList))
	{
		IEntity* pUnitEntity = signal.pSender;
		CAIObject* pUnit;
		if(pUnitEntity && (pUnit = (CAIObject*)(pUnitEntity->GetAI())))
		{
			const CAIActor* pActor = pUnit->CastToCAIActor();
			if(pActor)
				m_ActorForbiddenSpotManager.RemoveAllSpots(pActor);
		}
		return true;
	}
	else 	if(signal.Compare(g_crcSignals.m_nOnRequestUpdateTowards))
	{
		IEntity* pUnitEntity = signal.pSender;
		CAIObject* pUnit;
		if(pUnitEntity && (pUnit = (CAIObject*)(pUnitEntity->GetAI())))
		{
			CPipeUser* pPiper = pUnit->CastToCPipeUser();
			if(pPiper)
			{
				CAIObject* pTarget = (CAIObject*)pPiper->GetAttentionTarget();

				UpdatePointList(pTarget);

				TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pPiper);
				if(itUnit!=GetUnitsList().end())
				{
					itUnit->ClearFollowing();
					itUnit->SetMoving();
					if(signal.pEData)
					{
						if(signal.pEData->iValue) // unit is asking for a particular direction 
							itUnit->m_Group = signal.pEData->iValue;
/*						if (signal.pEData->fValue>0)
						{
							itUnit->SetHiding();
							m_fMinDistanceToNextPoint = signal.pEData->fValue;
						}*/
					}
				}
			}
		}
		return true;
	}
	else 	if(signal.Compare(g_crcSignals.m_nOnCheckDeadTarget))
	{
		IEntity* pUnitEntity = signal.pSender;
		CAIObject* pUnit;
		if(pUnitEntity && (pUnit = (CAIObject*)(pUnitEntity->GetAI())))
		{
			CPipeUser* pPiper = pUnit->CastToCPipeUser();
			if(pPiper)
			{
				IAIObject* pTarget = NULL;
				if(signal.pEData)
				{
					IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(signal.pEData->nID.n);
					if(pTargetEntity)
						pTarget = pTargetEntity->GetAI();
				}
				IAIObject* pGroupTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true,true,pTarget);
				CAIActor* pInvestigator = NULL;
				if(!pGroupTarget && pTarget && pTarget->GetAIType()==AIOBJECT_PLAYER)
				{
					// last touch, no more live targets and the last one was the player, go to investigate him
					float mindist2 = 1000000000.f;
					float dist2;
					Vec3 targetPos(pTarget->GetPos());
					TUnitList::iterator itUEnd = GetUnitsList().end();
					for(TUnitList::iterator itUnit=GetUnitsList().begin(); itUnit!= itUEnd; ++itUnit)
					{
						if((dist2 = Distance::Point_PointSq(targetPos, itUnit->m_pUnit->GetPos()) <= mindist2))
						{
							pInvestigator = itUnit->m_pUnit;
							mindist2 = dist2;
						}
					}

				}
				if(pInvestigator)
				{
					IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
					pData->nID = pTarget->GetEntityID();
					pInvestigator->SetSignal(0,"OnCheckDeadBody",pInvestigator->GetEntity(),pData,g_crcSignals.m_nOnCheckDeadBody);
				}	
				else
					pPiper->SetSignal(0,"OnNoGroupTarget",0,0,g_crcSignals.m_nOnNoGroupTarget);
			}
		}
		return true;
	}

	/*
	else if(signal.Compare(g_crcSignals.m_nOnShootSpotFound))
	{
	if(signal.pEData && !signal.pEData->point.IsZero())
	{
	Vec3 point = signal.pEData->point;
	//			TShootSpotList::iterator it= std::find(m_AdditionalShootSpots.begin(),m_AdditionalShootSpots.end(), point);
	CFormation* pFormation = m_pLeader->GetFormationOwner() ? m_pLeader->GetFormationOwner()->m_pFormation : NULL;
	int fsize= pFormation ? pFormation->GetSize(): 0;
	TPointPropertiesList::iterator it = m_PointProperties.begin(),itEnd = m_PointProperties.end();
	int i=0;
	bool bFound = false;
	for(; it!=itEnd; ++it)
	if(i++ >=fsize && it->point == point)
	{
	bFound = true;
	break;
	}

	if(!bFound)
	m_PointProperties.push_back(SPointProperties(point));
	}
	}
	else if(signal.Compare(g_crcSignals.m_nOnShootSpotNotFound))
	{
	if(signal.pEData && !signal.pEData->point.IsZero())
	{
	Vec3 point = signal.pEData->point;

	//TShootSpotList::iterator it= std::find(m_AdditionalShootSpots.begin(),m_AdditionalShootSpots.end(), point);
	CFormation* pFormation = m_pLeader->GetFormationOwner() ? m_pLeader->GetFormationOwner()->m_pFormation : NULL;
	int fsize= pFormation ? pFormation->GetSize(): 0;

	TPointPropertiesList::iterator it = m_PointProperties.begin(),itEnd = m_PointProperties.end();
	int i=0;
	for(; it!=itEnd; ++it)
	if(i++ >=fsize && it->point == point)
	{
	m_PointProperties.erase(it);
	break;
	}
	}
	}
	*/
	else if(signal.Compare(g_crcSignals.m_nAddDangerPoint))
	{
		IAISignalExtraData* pData = signal.pEData;
		if(pData)
		{
			Vec3 dpoint = pData->point;
			float radius =  pData->fValue;
			bool bFound = false;
			TDangerPointList::iterator it = m_DangerPoints.begin(), itEnd = m_DangerPoints.end();
			for(; it != itEnd; ++it)
				if(it->point == dpoint)
				{
					m_DangerPoints.erase(it);
					bFound = true;
					break;
				}

				m_DangerPoints.push_back(SDangerPoint(float(pData->iValue),pData->fValue,dpoint));

				if(!bFound)
				{
					TUnitList::iterator itUEnd = GetUnitsList().end();
					for(TUnitList::iterator itUnit=GetUnitsList().begin(); itUnit!= itUEnd; ++itUnit)
						if(Distance::Point_PointSq(dpoint,itUnit->m_pUnit->GetPos()) <= radius*radius)
						{
							itUnit->ClearFollowing();
							itUnit->ClearMoving();
						}
				}
				m_bPointsAssigned = false; // reassign all points
				m_bAvoidDanger = true;
		}
		return true;
	}
	else if(!strcmp(signal.strText,"SetDistanceToTarget"))
	{
		if(signal.pEData)
		{
			const CAIActor* pAIActor = CastToCAIActorSafe(signal.pSender->GetAI());
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pAIActor);
			if(itUnit!=GetUnitsList().end())
				itUnit->m_fDistance = signal.pEData->fValue;
		}
		return true;
	}	
	else if(!strcmp(signal.strText,"OnExecutingSpecialAction"))
	{
		const CAIActor* pAIActor = CastToCAIActorSafe(signal.pSender->GetAI());
		TSpecialActionMap::iterator its = m_SpecialActions.begin(),itsEnd = m_SpecialActions.end();
		for(; its != itsEnd; ++its)
		{
			SSpecialAction& action = its->second;
			if(action.pOwner == pAIActor && action.status == AS_WAITING_CONFIRM)
				action.status = AS_ON;
		}

		return true;
	}
	else if(!strcmp(signal.strText,"OnSpecialActionDone"))
	{
		const CAIActor* pAIActor = CastToCAIActorSafe(signal.pSender->GetAI());
		TSpecialActionMap::iterator its = m_SpecialActions.begin(),itsEnd = m_SpecialActions.end();
		for(; its != itsEnd; ++its)
		{
			SSpecialAction& action = its->second;
			if(action.pOwner == pAIActor)
				action.status = AS_OFF;
		}

		TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pAIActor);
		if(itUnit!=GetUnitsList().end())
		{
			itUnit->ClearSpecial();
			// will request a new position later
			itUnit->ClearFollowing();
			itUnit->m_TagPoint = ZERO;
			itUnit->SetMoving();
		}

		return true;
	}
	else if(!strcmp(signal.strText,"SetMinDistanceToTarget"))
	{
		if(signal.pEData)
		{
			m_fMinDistanceToTarget = signal.pEData->fValue;
		}
		return true;
	}
	return false;
}

void CLeaderAction_Attack_SwitchPositions::AssignNewShootSpot(CAIObject* pUnit, int index)
{
	CFormation* pFormation = m_pLeader->GetFormationOwner() ? m_pLeader->GetFormationOwner()->m_pFormation : NULL;
	int fsize= pFormation ? pFormation->GetSize(): 0;
	TPointPropertiesList::iterator it = m_PointProperties.begin(),itEnd = m_PointProperties.end();
	int i=0;
	bool bFound = false;
	for(; it!=itEnd; ++it)
		if(i++ >=fsize && it->pOwner == pUnit)
		{
			it->pOwner = NULL;
			break;
		}

	m_PointProperties[index].pOwner = pUnit;

}

//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_SwitchPositions::IsVehicle(const CAIActor* pTarget, IEntity** ppVehicleEntity) const
{
	// assuming not null pTarget
	if(pTarget->GetAIType()==AIOBJECT_VEHICLE)
	{
		if(ppVehicleEntity)
			*ppVehicleEntity = pTarget->GetEntity();
		return true;
	}
	else
	{
		IEntity* pEntity = pTarget->GetEntity();
		if(pEntity)
		{
			while(pEntity->GetParent())
				pEntity = pEntity->GetParent();
			if(pEntity->GetAI() && pEntity->GetAI()->GetAIType() == AIOBJECT_VEHICLE)
			{
				if(ppVehicleEntity)
					*ppVehicleEntity = pEntity;
				return true;
			}
		}
	}
	return false;
}

//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_SwitchPositions::IsSpecialActionConsidered(const CAIActor* pUnit, const CAIActor* pUnitLiveTarget) const 
{
	TSpecialActionMap::const_iterator its = m_SpecialActions.find(pUnitLiveTarget), itEnd = m_SpecialActions.end();
	if(its != itEnd)
	{
		// if it's a vehicle, more units can have a special action with it
		if(IsVehicle(pUnitLiveTarget))
		{
			while(its!= itEnd && its->first==pUnitLiveTarget)
			{
				if(pUnit==its->second.pOwner)
					return true;
				++its;
			}
		}
		else
			return true;
	}
	return false;
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::UpdateSpecialActions()
{
	CTimeValue Time = GetAISystem()->GetFrameStartTime(); 
	TUnitList::iterator it = GetUnitsList().begin(),itEnd = GetUnitsList().end();
	int size = m_pLeader->GetAIGroup()->GetGroupCount(IAISystem::GROUP_ENABLED);
	size = max(1,size/2); // max half of the group can have special action
	// update special action list with new live targets
	for(; it!= itEnd; ++it)
	{
		CPipeUser* pUnit = CastToCPipeUserSafe(it->m_pUnit);
		if(pUnit && pUnit->IsEnabled())
		{
			IAIObject* pUnitTarget = pUnit->GetAttentionTarget();
			const CAIActor* pUnitLiveTarget = CastToCAIActorSafe(pUnitTarget);
			if(pUnitLiveTarget && pUnitLiveTarget->IsHostile(pUnit) && !IsSpecialActionConsidered(pUnit,pUnitLiveTarget))
				m_SpecialActions.insert(std::make_pair(pUnitLiveTarget,SSpecialAction(pUnit)));
			
		}
	}
	
	TSpecialActionMap::iterator its = m_SpecialActions.begin(),itsEnd = m_SpecialActions.end();
	for(; size>0 && its != itsEnd; ++its)
	{
		const CAIActor* pTarget = its->first;
		SSpecialAction& action = its->second;
		if(action.status == AS_OFF && (Time - action.lastTime).GetSeconds() > 2)
		{
			float maxscore = -1000000;
			TUnitList::iterator itUnitSelected = itEnd;
			for(it = GetUnitsList().begin();  it!= itEnd; ++it)
			{
				CPipeUser* pUnit = CastToCPipeUserSafe(it->m_pUnit);
				if(pUnit && pUnit->IsEnabled())
				{
					IAIObject* pUnitTarget = pUnit->GetAttentionTarget();
					if(pUnitTarget && pUnitTarget == (IAIObject*)pTarget)
					{
						float score = -Distance::Point_PointSq(pTarget->GetPos(),pUnit->GetPos());
						if(action.pOwner != pUnit)
							score +=49; // like it was 7m closer
						if(score> maxscore)
						{
							maxscore = score;
							itUnitSelected = it;

						}
					}
				}
			}
			if(itUnitSelected != itEnd)
			{
				action.status = AS_WAITING_CONFIRM;
				action.lastTime = Time;
				action.pOwner = itUnitSelected->m_pUnit;
				itUnitSelected->m_pUnit->SetSignal(1,"OnSpecialAction",itUnitSelected->m_pUnit->GetEntity(),NULL,g_crcSignals.m_nOnSpecialAction);
				itUnitSelected->SetSpecial();
				size--;
			}
		}

		switch(action.status)
		{
			case AS_ON:
				action.lastTime = Time;
				break;
			case AS_WAITING_CONFIRM:
				if((Time - action.lastTime).GetSeconds() > 3)
				{ // if unit doesn't confirm doing the special action,clear it 
					action.status = AS_OFF;
					TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(action.pOwner);
					if(itUnit!=GetUnitsList().end())
						itUnit->ClearSpecial();
				}
				break;
			default:
				break;
		}

	}
}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction::eActionUpdateResult CLeaderAction_Attack_SwitchPositions::Update()
{
	m_pLiveTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true,true);

	CTimeValue frameTime = GetAISystem()->GetFrameStartTime(); 
	if(m_pLiveTarget)
		m_timeRunning = frameTime;
	else	if( (frameTime - m_timeRunning).GetSeconds() > m_timeLimit)
		return ACTION_DONE;

	CAIObject* pBeacon = (CAIObject*)GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
	if(!pBeacon)
		UpdateBeacon();
	if(!pBeacon)
		return ACTION_FAILED;

	m_bVisibilityChecked = false;

	bool bFormationUpdated = false;

	//Vec3 targetPos(pLiveTarget ? pLiveTarget->GetPos()+pLiveTarget->GetViewDir() : ZERO);
	TUnitList::iterator itEnd = GetUnitsList().end();
	if(!m_bInitialized)
	{
		if(!m_pLiveTarget)
			return ACTION_FAILED;
		if(!m_pLeader->LeaderCreateFormation(m_sFormationType, ZERO, false, m_unitProperties, pBeacon))
			return ACTION_FAILED;
		CFormation* pFormation = pBeacon->m_pFormation;
		assert(pFormation);
		//pFormation->SetReferenceTarget((CAIObject*) pLiveTarget);
		pFormation->SetOrientationType(CFormation::OT_VIEW);
		pFormation->ForceReachablePoints(true);
		pFormation->GetNewFormationPoint(m_pLiveTarget->CastToCAIActor(),0);

		m_fFormationScale = 1;
		m_fFormationSize = pFormation->GetMaxWidth();
		m_lastScaleUpdateTime.SetSeconds(0.f);
		UpdateFormationScale(pFormation);

		pFormation->SetUpdate(true);
		pFormation->Update();
		pFormation->SetUpdate(false);

		bFormationUpdated = true;

		m_vEnemyPos = pBeacon->GetPos();
		if(m_vEnemyPos.IsZero())
			return ACTION_FAILED;

		int size = pFormation->GetSize();
		m_PointProperties.resize(size);
		m_bInitialized = true;
		m_bPointsAssigned = false;
		m_bAvoidDanger = false;

		ClearUnitFlags();

		UpdatePointList();
	}

	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg &curUnit = (*unitItr);
		CheckNavType(curUnit);

		if(m_bPointsAssigned && !curUnit.IsMoving())
		{
			CAIActor* pUnit = curUnit.m_pUnit;

			CAIActor* pUnitObstructing = NULL;

			// check Behind status - the unit is behind some other unit in the grioup
			Vec3 pos = pUnit->GetPos();
			CPipeUser* pPiper = pUnit->CastToCPipeUser();
			IAIObject* pTarget = pPiper->GetAttentionTarget();
			Vec3 targetPos = (pTarget && pTarget->GetEntity() ? pTarget->GetPos() : m_vEnemyPos) ;
			Vec3 dirToEnemy = (targetPos - pos);
			Vec3 proj;

			for(TUnitList::iterator otherItr=GetUnitsList().begin(); otherItr!=itEnd; ++otherItr)
			{
				CUnitImg &otherUnit = (*otherItr);
				if(otherUnit != curUnit && !otherUnit.IsMoving())
				{
					Vec3 otherPos = otherUnit.m_pUnit->GetPos();
					Vec3 dirToUnit = (otherPos - pos).GetNormalizedSafe();
					if(dirToUnit.Dot(dirToEnemy) > 0.f)
						if(fabs(pos.z - otherPos.z) < otherUnit.GetHeight()*1.1f)
							if(Distance::Point_Line2D(otherPos,pos,targetPos,proj) < otherUnit.GetWidth()/2)
							{
								float distanceToEnemy = Distance::Point_PointSq(targetPos,pos);
								float unitToEnemy = Distance::Point_PointSq(otherUnit.m_pUnit->GetPos(),targetPos);
								if(unitToEnemy > 2*2)
								{
									pUnitObstructing = otherUnit.m_pUnit;
									break;
								}
							}
				}
			}	

			if(curUnit.IsBehind() && !pUnitObstructing)
			{
				pUnit->SetSignal(1,"OnNotBehind",pUnit->GetEntity());
				curUnit.ClearBehind();
			}
			else if(!curUnit.IsBehind() && pUnitObstructing)
			{
				IAISignalExtraData* pData = NULL;
				IEntity* pOtherEntity = pUnitObstructing->GetEntity();
				if(pOtherEntity)
				{
					pData = GetAISystem()->CreateSignalExtraData();
					pData->nID = pOtherEntity->GetId();
				}
				pUnit->SetSignal(1,"OnBehind",pUnit->GetEntity(),pData);
				curUnit.SetBehind();
			}
		}
	}	

	if(!m_bPointsAssigned)
	{
		CFormation* pFormation = pBeacon->m_pFormation;
		int s = pFormation->GetSize();
		for(int i = 1; i < s; i++)
			pFormation->FreeFormationPoint(i);

		ClearUnitFlags();

		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg &curUnit = (*unitItr);
			curUnit.m_FormationPointIndex = -1;

			STargetData* pUnitTData = GetTargetData(curUnit);
			bool bAssignPoint = !(pUnitTData &&  
				(pUnitTData->pTarget && (pUnitTData->targetNavType == IAISystem::NAV_WAYPOINT_HUMAN)));

			CAIObject* pUnit = curUnit.m_pUnit;
			CPipeUser* pPiper = pUnit->CastToCPipeUser();
			int iPointIndex = bAssignPoint ? GetFormationPointWithTarget(curUnit) : -1;
			if(pPiper)
			{
				CAIObject* pTarget = (CAIObject*)pPiper->GetAttentionTarget();
				if(!pTarget)
					pTarget = pBeacon;
				GetBaseSearchPosition(curUnit,pTarget,AI_MOVE_BACKWARD);
				if(iPointIndex>0 )
				{
					if(iPointIndex<s)
					{

						m_pLeader->AssignFormationPointIndex(curUnit,iPointIndex);
						pPiper->SetSignal(1,"OnAttackSwitchPosition");
						// DEBUG
						/*					Vec3 point = pBeacon->m_pFormation->GetFormationPoint(iPointIndex)->GetPos();
						GetAISystem()->AddDebugLine(pUnit->GetPos(),point,255,60,60,5);
						GetAISystem()->AddDebugLine(point-Vec3(0,0,1),point+Vec3(0,0,3),255,60,60,5);
						*/
						curUnit.ClearFollowing();
					}
					else
					{
						CAIActor* pAIActor = pUnit->CastToCAIActor();
						IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
						if(pData)
						{
							pData->point = m_PointProperties[iPointIndex].point;
							AssignNewShootSpot(pUnit,iPointIndex);
							CPipeUser* pPiper = pUnit->CastToCPipeUser();
							pPiper->SetRefPointPos(pData->point);
							pPiper->SetSignal(1,"OnAttackShootSpot",pUnit->GetEntity(),pData);
							curUnit.ClearFollowing();
						}
					}
				}
				else if(m_bAvoidDanger)
				{
					//IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
					//if(pData)
					{
						// to do: find a point outside all dangers and send 'em there
						Vec3 worstPoint(ZERO);
						Vec3 unitPos(pUnit->GetPos());
						float mindist = 1000000.f;
						TDangerPointList::iterator it = m_DangerPoints.begin(), itEnd = m_DangerPoints.end();
						for(; it != itEnd; ++it)
						{
							float d = Distance::Point_Point2DSq(unitPos,it->point);
							if(d < mindist)
							{
								mindist = d;
								worstPoint = it->point;
							}
						}
						pPiper->SetRefPointPos(worstPoint);

						pPiper->SetSignal(1,"OnAvoidDanger",NULL,NULL,g_crcSignals.m_nOnAvoidDanger);
					}
				}
			}
			m_pLeader->AssignFormationPointIndex(curUnit,iPointIndex);
			curUnit.ClearFollowing();	
			curUnit.ClearSpecial(); // just to avoid sending the OnAttackSwitchPosition/ShootSpot signal now
		}
		m_bAvoidDanger = false;
		m_bPointsAssigned = true;

		return ACTION_RUNNING;
	}	


	CFormation* pFormation = pBeacon->m_pFormation;
	if(pFormation)
	{
		int size = pFormation->GetSize();
		for(int i=0; i < size; i++)
			m_PointProperties[i].bTargetVisible = false;
	}
	else
		return ACTION_FAILED;


	if(UpdateFormationScale(pFormation))
	{
		bFormationUpdated = true;
		pFormation->SetUpdate(true);
		pFormation->Update();
		pFormation->SetUpdate(false);
		m_vEnemyPos = pBeacon->GetPos();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
			unitItr->SetMoving(); // force all units to move to (new) formation points when target and formation move
	}

	// move the formation if enemy avg position has moved
	if(!bFormationUpdated && Distance::Point_Point2DSq(pBeacon->GetPos(),m_vEnemyPos) >4*4)
	{
		pFormation->SetUpdate(true);
		pFormation->Update();
		pFormation->SetUpdate(false);
		m_vEnemyPos = pBeacon->GetPos();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
			unitItr->SetMoving(); // force all units to move to (new) formation points when target and formation move
	}

	UpdateSpecialActions();

	if(m_DangerPoints.empty())
	{
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg &curUnit = (*unitItr);
			CAIObject* pUnit = curUnit.m_pUnit;
			CPuppet* pPuppet = pUnit->CastToCPuppet();
			if(pUnit->IsEnabled() && pPuppet)
			{
				IAIObject* pTarget = pPuppet->GetAttentionTarget();
				if(pTarget) 
				{
					const CAIActor* pTargetActor = pTarget->CastToCAIActor();

					IEntity* pVehicleEntity=NULL;
					if(pTargetActor && IsVehicle( pTargetActor,&pVehicleEntity))
					{
						// use refpoint for vehicles
						AABB aabb;
						pVehicleEntity->GetLocalBounds(aabb);
						float x = (aabb.max.x - aabb.min.x)/2;
						float y = (aabb.max.y - aabb.min.y)/2;
						float radius = sqrt(x*x+y*y) + 1.f;
						Vec3 center(pVehicleEntity->GetWorldPos());
						center.z+=1.f;
						Vec3 dir(center - pUnit->GetPos());
						dir.z = 0;
						dir.NormalizeSafe();
						Vec3 hitdir(dir* radius);
						Vec3 pos(center - hitdir);
						IUnknownProxy* pProxy = pPuppet->GetProxy();
						IPhysicalEntity* pPhysics = pProxy ? pProxy->GetPhysics(): NULL;

						ray_hit hit;
						int rayresult=0;
						IPhysicalWorld *pWorld = GetAISystem()->GetPhysicalWorld();
						if(pWorld)
						{
							TICK_COUNTER_FUNCTION
							TICK_COUNTER_FUNCTION_BLOCK_START
								rayresult = pWorld->RayWorldIntersection(pos, hitdir, COVER_OBJECT_TYPES, HIT_COVER, &hit, 1,pPhysics);
							TICK_COUNTER_FUNCTION_BLOCK_END
							if(rayresult)
								pos = hit.pt - dir;
						}
						pPuppet->SetRefPointPos(pos);
						continue;
					}
				}
				if(!curUnit.IsSpecial() && 
					(curUnit.IsMoving() || 
					curUnit.IsFollowing() && (!pTarget || !pTarget->IsAgent())))
				{
					curUnit.ClearMoving();
					pe_status_dynamics  dSt;
					Vec3 velocity(ZERO);
					IUnknownProxy*  pProxy;
					IPhysicalEntity* pPhysics;
					if((pProxy = pUnit->GetProxy()) && (pPhysics = pProxy->GetPhysics()))
					{
						pPhysics->GetStatus( &dSt );
						velocity = dSt.v;
						//if(velocity.GetLengthSquared() < 1)
						{
							// change position, can't see any live target and I'm not moving
							int iPointIndex = GetFormationPointWithTarget(curUnit);
							if(iPointIndex>0 && iPointIndex != m_pLeader->GetFormationPointIndex(pUnit))
							{
								if(iPointIndex < pFormation->GetSize())
								{
									m_pLeader->AssignFormationPointIndex(curUnit,iPointIndex);
									//pPiper->SetLastOpResult(pMyFormationPoint);
									CAIActor* pAIActor = pUnit->CastToCAIActor();
									pAIActor->SetSignal(1,"OnAttackSwitchPosition");
								}
								else
								{
									CAIActor* pAIActor = pUnit->CastToCAIActor();
									IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
									if(pData)
									{
										pData->point = m_PointProperties[iPointIndex].point;
										AssignNewShootSpot(pUnit,iPointIndex);
										CPipeUser* pPiper = pUnit->CastToCPipeUser();
										pPiper->SetRefPointPos(pData->point);
										pPiper->SetSignal(1,"OnAttackShootSpot",pUnit->GetEntity(),pData);
										curUnit.ClearFollowing();
									}
								}
							}
						}
					}
				}
			}
		}
	}
	// update danger points
	CTimeValue frameDTime = GetAISystem()->GetFrameDeltaTime();
	TDangerPointList::iterator itp = m_DangerPoints.begin();
	while( itp != m_DangerPoints.end() )
	{
		itp->time -= frameDTime.GetSeconds();
		if(itp->time<=0)
			itp = m_DangerPoints.erase(itp);
		else
			++itp;
	}

	m_fMinDistanceToNextPoint = m_fDefaultMinDistance;

	return ACTION_RUNNING;
}

bool CLeaderAction_Attack_SwitchPositions::UpdateFormationScale(CFormation* pFormation)
{
	// scale the formation depending on enemy occupied area
	float t = (GetAISystem()->GetFrameStartTime() - m_lastScaleUpdateTime).GetSeconds();
	bool ret = false;
	if(t > 2.f)
	{
		m_lastScaleUpdateTime = GetAISystem()->GetFrameStartTime();

		Vec3 pos1(m_pLeader->GetAIGroup()->GetForemostEnemyPosition(Vec3Constants<float>::fVec3_OneX ));
		Vec3 pos2(m_pLeader->GetAIGroup()->GetForemostEnemyPosition(Vec3Constants<float>::fVec3_OneY ));
		Vec3 pos3(m_pLeader->GetAIGroup()->GetForemostEnemyPosition(-Vec3Constants<float>::fVec3_OneX ));
		Vec3 pos4(m_pLeader->GetAIGroup()->GetForemostEnemyPosition(-Vec3Constants<float>::fVec3_OneY ));
		float s = Distance::Point_Point(pos1,pos2);
		float s1 = Distance::Point_Point(pos3,pos4);
		if(s < s1)
			s = s1;
		float s2 = Distance::Point_Point(pos2,pos4);
		if(s < s2)
			s = s2;

		if(s > m_fFormationSize)
			s = m_fFormationSize;

		float scale = (m_fFormationSize + s)/m_fFormationSize;
		float scaleRatio = scale/m_fFormationScale;
		if(scaleRatio < 0.8f || scaleRatio>1.2f)
		{
			m_fFormationScale = scale;
			pFormation->SetScale(scale);
			ret = true;
		}

		// compute distance to target
		int si = pFormation->GetSize();
		float mindist2 = 100000;
		bool bfound = false;
		Vec3 point;
		for(int i=0;i<si ; i++)
		{
			if(pFormation->GetPointOffset(i,point))
			{
				float dist2 = point.GetLengthSquared();
				if(dist2 < mindist2)
				{
					mindist2 = dist2;
					bfound = true;
				}
			}
		}
		if(bfound)
			m_fDistanceToTarget = sqrt(mindist2);
	}

	return ret;
}

int CLeaderAction_Attack_SwitchPositions::GetFormationPointWithTarget(CUnitImg& unit)
{
	CAIObject* pBeacon = (CAIObject*)GetAISystem()->GetBeacon(m_pLeader->GetGroupId());

	CPipeUser* pUnit = unit.m_pUnit->CastToCPipeUser();
	if(!pUnit)
		return -1;

	int index = -1;
	Vec3 groundDir(0,0,-6);
	int myCurrentPointIndex = unit.m_FormationPointIndex;
	if(pBeacon)
	{
		// TO DO: get the formation from the leader directly (beacon will not own the formation after refactoring)
		CFormation* pFormation = pBeacon->m_pFormation;
		if(pFormation)
		{
			//CAIObject* pMyPoint = myCurrentPointIndex ? pFormation->GetFormationPoint(myCurrentPointIndex) : NULL;
			//float myPointPosZ(pMyPoint ? pMyPoint->GetPos().z : pUnit->GetPos().z );
			//float zDisp = pUnit->GetPos().z - myPointPosZ;
			SAIBodyInfo bodyInfo;
			pUnit->GetProxy()->QueryBodyInfo( bodyInfo);
			IEntity* pUnitEntity = pUnit->GetEntity();
			float zDisp = (pUnitEntity ? bodyInfo.vEyePos.z - pUnitEntity->GetWorldPos().z : 1.2f);

			int formationSize = pFormation->GetSize();

			// total size includes all points, including shoot spots (m_PointProperties.size()) only if there's no live target
			// otherwise, only formation points (first <formationSize> points in the m_PointProperties list
			int size = (m_pLiveTarget ? formationSize : m_PointProperties.size() );

			IPhysicalWorld *pWorld = GetAISystem()->GetPhysicalWorld();
			ray_hit hit;
			int rayresult(0);

			CAIObject* pTarget = (CAIObject*)pUnit->GetAttentionTarget();
			if(!pTarget)
				pTarget = pBeacon;
			IUnknownProxy* pProxy = pTarget->GetProxy();
			IPhysicalEntity* pTargetPhysics = pProxy ? pProxy->GetPhysics(): NULL;
			if(!pTargetPhysics)
			{
				IAIObject* pAssociation;
				if(pTarget->GetSubType()==IAIObject::STP_MEMORY && (pAssociation = pTarget->GetAssociation()))
				{
					if((pProxy = pAssociation->GetProxy()))
						pTargetPhysics = pProxy->GetPhysics();
				}
			}

			Vec3 beaconpos = pTarget->GetPos();
			
			if(unit.m_TagPoint.IsZero())
				GetBaseSearchPosition(unit,pTarget,unit.m_Group);
			Vec3 unitpos = unit.m_TagPoint;

			float mindist = 100000000.f;			
			float fMinDistanceToNextPoint = unit.m_fDistance2 > 0? unit.m_fDistance2 : m_fMinDistanceToNextPoint;
			for(int i=1; i<size;i++)//exclude owner's point
			{
				Vec3 pos;
				if(i<formationSize)
				{
					CAIObject* pPointOwner = pFormation->GetPointOwner(i);
					if(pPointOwner == pUnit)
						myCurrentPointIndex = i;
					if(pPointOwner)
						continue;
					CAIObject* pFormationPoint = pFormation->GetFormationPoint(i);
					if(pFormationPoint)
					{
						pos = pFormationPoint->GetPos();
					}
					else
						continue;
				}
				else
					pos = m_PointProperties[i].point;

				if(Distance::Point_Point2DSq(pos,beaconpos) < m_fMinDistanceToTarget*m_fMinDistanceToTarget)
					continue;

				if(!m_ActorForbiddenSpotManager.IsForbiddenSpot(pUnit,pos))
				{
					{
						// rough visibility check with raytrace

						// check if the current point is not in a danger region
						bool bDanger = false;
						TDangerPointList::iterator itp = m_DangerPoints.begin(), itpEnd = m_DangerPoints.end();
						for(; itp != itpEnd;++itp)
						{
							float r = itp->radius;
							r *=r;
							if(Distance::Point_Point2DSq(itp->point,pos)<=r)
							{
								bDanger = true;
								break;
							}
							else if((pos - unitpos).GetNormalizedSafe().Dot((itp->point - unitpos).GetNormalizedSafe()) >0.1f)
							{
								bDanger = true;
								break;
							}
						}

						if(bDanger)
							continue;

						// tweak the right Z position for formation point - the same Z as the requestor being at the formation point
						int rayresult = 0;
						if(!m_bVisibilityChecked && !unit.IsFar())
						{
							TICK_COUNTER_FUNCTION
								TICK_COUNTER_FUNCTION_BLOCK_START
								if(i<formationSize)
								{
									rayresult = pWorld->RayWorldIntersection(pos, groundDir, COVER_OBJECT_TYPES, HIT_COVER, &hit, 1,pTargetPhysics);
									if(rayresult)
										pos.z = hit.pt.z;
								}
								Vec3 rayPos(pos.x,pos.y,pos.z+zDisp);
								rayresult = pWorld->RayWorldIntersection(rayPos, pTarget->GetPos() - rayPos, COVER_OBJECT_TYPES|ent_living, HIT_COVER, &hit, 1,pTargetPhysics);
							TICK_COUNTER_FUNCTION_BLOCK_END
						}
						// TO DO: optimization with m_bVisibilityChecked and m_Targetvisiblep[] 
						// works perfectly only if there's one enemy
						if(!rayresult || m_PointProperties[i].bTargetVisible) 
						{
							float dist=0;
							//if(unit.m_Group==AI_BACKOFF_FROM_TARGET || unit.GetClass()==UNIT_CLASS_LEADER)
							//	dist2 -=Distance::Point_Point2DSq(beaconpos,pos);
							if(unit.m_fDistance>0)
								dist = 2*fabs(unit.m_fDistance - Distance::Point_Point2D(beaconpos,pos)) ;
							float d1 = Distance::Point_Point2D(pUnit->GetPos(),pos) - fMinDistanceToNextPoint;
							if(d1<0)
								dist -=d1;
							else 
								dist +=d1;
							//GetAISystem()->AddDebugLine(beaconpos,unit.m_TagPoint,255,200,220,5);
							if(!unit.m_TagPoint.IsZero())
								dist += Distance::Point_Point2D(unit.m_TagPoint,pos);
							
							if(unit.m_Group==AI_BACKOFF_FROM_TARGET) 
							{
								//try to stay in front of target
								Vec3 targetDir(pos - beaconpos);
								targetDir.NormalizeSafe();
								float dot = targetDir.Dot(pTarget->GetViewDir());
								if(dot<0.3f)
									dist += (1-dot)*m_fFormationSize;
							}

							if(dist < mindist)
							{
								mindist = dist;
								m_PointProperties[i].bTargetVisible = true;
								index = i;
							}
						}
					}
				}
			}
			m_bVisibilityChecked = true;
		}
	}
	
	unit.m_Group =0;
	unit.m_fDistance2 = 0;
	unit.ClearFar();

	unit.m_TagPoint = ZERO;

	if(index<0 && myCurrentPointIndex>0)
		return myCurrentPointIndex;

	return index;
}


// 
//----------------------------------------------------------------------------------------------------
Vec3 CLeaderAction_Attack_SwitchPositions::GetBaseDirection(CAIObject* pTarget, bool bUseTargetMoveDir)
{
	Vec3 vY;
	if(bUseTargetMoveDir)
	{
		vY = pTarget->GetMoveDir();
		vY.z=0; // 2D only
		vY.NormalizeSafe(pTarget->GetBodyDir()); 
	}
	else
	{
		vY = pTarget->GetViewDir();
		if(fabs(vY.z)>0.95f)
			vY = pTarget->GetBodyDir();
		vY.z=0; // 2D only
		vY.NormalizeSafe(); 
	}
	return vY;
}
// 
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::GetBaseSearchPosition(CUnitImg& unit, CAIObject* pTarget,int method, float distance)
{
	Vec3 unitpos = unit.m_pUnit->GetPos();
	if(!pTarget)
	{
		pTarget = (CAIObject*)m_pLeader->GetAIGroup()->GetAttentionTarget();
		if(!pTarget)
		{
			pTarget = (CAIObject*)GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
			if(!pTarget)
			{
				unit.m_TagPoint = unitpos;
				return;
			}
		}
	}

	Vec3 beaconpos = pTarget->GetPos();
	

	if(method<0)
	{
		method = -method;
		unit.SetFar();
	}
	else
		unit.ClearFar();

	bool bUseTargetMoveDir = (method & AI_USE_TARGET_MOVEMENT) !=0;
	method &= ~AI_USE_TARGET_MOVEMENT;

	if(method!= AI_BACKOFF_FROM_TARGET)
	{
		if(distance<=0)
			distance = 2*m_fMinDistanceToNextPoint;
		else
			distance +=m_fMinDistanceToNextPoint;
	}

	unit.m_Group = method;

	switch(method)
	{
	case AI_BACKOFF_FROM_TARGET:
		{
			unit.m_fDistance = distance;
			Vec3 vY(unitpos - beaconpos)  ;
			vY.NormalizeSafe();
			unitpos = beaconpos + vY*distance;
		}
		break;
	case AI_MOVE_BACKWARD: // move back to the target
		{
			Vec3 vY(GetBaseDirection(pTarget,bUseTargetMoveDir));
			unitpos = beaconpos + vY*distance;
		}
		break;
	case AI_MOVE_LEFT: // move left to the target
		{
			Vec3 vY(GetBaseDirection(pTarget,bUseTargetMoveDir));
			Vec3 vX = vY % Vec3(0,0,1);
			unitpos = beaconpos - vX*distance;
		}
		break;
	case AI_MOVE_RIGHT: // move left to the target
		{
			Vec3 vY(GetBaseDirection(pTarget,bUseTargetMoveDir));
			Vec3 vX = vY % Vec3(0,0,1);
			unitpos = beaconpos + vX*distance;
		}
		break;
	case AI_MOVE_RIGHT+AI_MOVE_BACKWARD: // move left to the target
		{
			Vec3 vY(GetBaseDirection(pTarget,bUseTargetMoveDir));
			Vec3 vX = vY % Vec3(0,0,1);
			unitpos = beaconpos + (vX+vY).GetNormalizedSafe()*distance;
		}
		break;
	case AI_MOVE_LEFT+AI_MOVE_BACKWARD: // move left to the target
		{
			Vec3 vY(GetBaseDirection(pTarget,bUseTargetMoveDir));
			Vec3 vX = vY % Vec3(0,0,1);
			unitpos = beaconpos + (-vY-vX).GetNormalizedSafe()*distance;
		}
		break;
	default:
		break;
	}
	unit.m_TagPoint = unitpos;

}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction_Attack_Chase::CLeaderAction_Attack_Chase(CLeader* pLeader, const LeaderActionParams& params)
{
	m_pLeader = pLeader;
	m_eActionType = LA_ATTACK;
	m_eActionSubType = LAS_ATTACK_CHASE;
	m_bInitialized = false;
	m_timeLimit = 4;
	m_timeRunning = GetAISystem()->GetFrameStartTime(); 
	m_sFormationType = params.name;
	m_unitProperties = params.unitProperties;
	m_fPredictionTimeSpan = params.fDuration > 0 ? params.fDuration : 4;
	m_fMinDistance = params.vPoint.z;
	m_fUpdateFormationThr = params.vPoint.y;
	m_fMinSpeed = params.vPoint.x;
}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction_Attack_Chase::~CLeaderAction_Attack_Chase()
{
	m_pLeader->ReleaseFormation();
}

//
//----------------------------------------------------------------------------------------------------
CLeaderAction::eActionUpdateResult CLeaderAction_Attack_Chase::Update()
{
	IAIObject* pLiveTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true,true);
	CTimeValue frameTime = GetAISystem()->GetFrameStartTime(); 
	if(pLiveTarget)
	{
		float speed = GetTargetSpeed(pLiveTarget);
		if(speed>m_fMinSpeed)
			m_timeRunning = frameTime.GetSeconds();
	}
	
	if( (frameTime - m_timeRunning).GetSeconds() > m_timeLimit)
		return ACTION_DONE;

	CAIObject* pBeacon = (CAIObject*)GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
	if(!pBeacon)
		return ACTION_FAILED;


	//Vec3 targetPos(pLiveTarget ? pLiveTarget->GetPos()+pLiveTarget->GetViewDir() : ZERO);
	TUnitList::iterator itEnd = GetUnitsList().end();
	if(!m_bInitialized)
	{
		/*IAIObject* pTarget = pLiveTarget ? pLiveTarget : m_pLeader->GetAIGroup()->GetAttentionTarget(true,false);
		if(!pTarget)
			return ACTION_FAILED;
		*/
		if(!m_pLeader->LeaderCreateFormation(m_sFormationType, ZERO, false, m_unitProperties, pBeacon))
			return ACTION_FAILED;
		CFormation* pFormation = pBeacon->m_pFormation;
		assert(pFormation);
		//pFormation->SetReferenceTarget((CAIObject*) pLiveTarget);
		pFormation->SetOrientationType(CFormation::OT_VIEW);
		pFormation->ForceReachablePoints(true);
		//pFormation->GetNewFormationPoint(pTarget->CastToCAIActor(),0);
		pFormation->SetUpdate(true);
		pFormation->Update();
		pFormation->SetUpdate(false);

		m_vEnemyPos = pBeacon->GetPos();
		if(m_vEnemyPos.IsZero())
			return ACTION_FAILED;

		m_vOldPosition = ZERO;

		m_bInitialized = true;
		m_bPointsAssigned = false;

		ClearUnitFlags();

	}

	for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
	{
		CUnitImg &curUnit = (*unitItr);

		if(m_bPointsAssigned && !curUnit.IsMoving())
		{
			CAIActor* pUnit = curUnit.m_pUnit;

			CAIActor* pUnitObstructing = NULL;

			// check Behind status - the unit is behind some other unit in the grioup
			Vec3 pos = pUnit->GetPos();
			CPipeUser* pPiper = pUnit->CastToCPipeUser();
			IAIObject* pTarget = pPiper->GetAttentionTarget();
			Vec3 targetPos = (pTarget && pTarget->GetEntity() ? pTarget->GetPos() : m_vEnemyPos) ;
			Vec3 dirToEnemy = (targetPos - pos);
			Vec3 proj;

			for(TUnitList::iterator otherItr=GetUnitsList().begin(); otherItr!=itEnd; ++otherItr)
			{
				CUnitImg &otherUnit = (*otherItr);
				if(otherUnit != curUnit && !otherUnit.IsMoving())
				{
					Vec3 otherPos = otherUnit.m_pUnit->GetPos();
					Vec3 dirToUnit = (otherPos - pos).GetNormalizedSafe();
					if(dirToUnit.Dot(dirToEnemy) > 0.f)
						if(fabs(pos.z - otherPos.z) < otherUnit.GetHeight()*1.1f)
							if(Distance::Point_Line2D(otherPos,pos,targetPos,proj) < otherUnit.GetWidth()/2)
							{
								float distanceToEnemy = Distance::Point_PointSq(targetPos,pos);
								float unitToEnemy = Distance::Point_PointSq(otherUnit.m_pUnit->GetPos(),targetPos);
								if(unitToEnemy > 2*2)
								{
									pUnitObstructing = otherUnit.m_pUnit;
									break;
								}
							}
				}
			}	

			if(curUnit.IsBehind() && !pUnitObstructing)
			{
				pUnit->SetSignal(1,"OnNotBehind",pUnit->GetEntity());
				curUnit.ClearBehind();
			}
			else if(!curUnit.IsBehind() && pUnitObstructing)
			{
				IAISignalExtraData* pData = NULL;
				IEntity* pOtherEntity = pUnitObstructing->GetEntity();
				if(pOtherEntity)
				{
					pData = GetAISystem()->CreateSignalExtraData();
					pData->nID = pOtherEntity->GetId();
				}
				pUnit->SetSignal(1,"OnBehind",pUnit->GetEntity(),pData);
				curUnit.SetBehind();
			}
		}
	}	

	if(!m_bPointsAssigned)
	{
		CFormation* pFormation = pBeacon->m_pFormation;
		int s = pFormation->GetSize();
		for(int i = 1; i < s; i++)
			pFormation->FreeFormationPoint(i);

		ClearUnitFlags();

		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg &curUnit = (*unitItr);
			curUnit.m_FormationPointIndex = -1;

			CAIObject* pUnit = curUnit.m_pUnit;

			int iPointIndex = GetFormationPointWithTarget(curUnit);
			if(iPointIndex>0 )
			{
				CPipeUser* pPiper = pUnit->CastToCPipeUser();
				if(pPiper)
				{
					m_pLeader->AssignFormationPointIndex(curUnit,iPointIndex);
					pPiper->SetSignal(1,"OnAttackChase");
					// DEBUG
					/*					Vec3 point = pBeacon->m_pFormation->GetFormationPoint(iPointIndex)->GetPos();
					GetAISystem()->AddDebugLine(pUnit->GetPos(),point,255,60,60,5);
					GetAISystem()->AddDebugLine(point-Vec3(0,0,1),point+Vec3(0,0,3),255,60,60,5);
					*/
				}
				curUnit.ClearFollowing();
				m_pLeader->AssignFormationPointIndex(curUnit,iPointIndex);
			}
		}
		//m_bAvoidDanger = false;
		m_bPointsAssigned = true;

		return ACTION_RUNNING;
	}	


	CFormation* pFormation = pBeacon->m_pFormation;
	if(!pFormation)
		return ACTION_FAILED;

	if(Distance::Point_Point2DSq(pBeacon->GetPos(),m_vOldPosition) >m_fUpdateFormationThr*m_fUpdateFormationThr)
	{
		m_vOldPosition = pBeacon->GetPos();
		pFormation->SetUpdate(true);
		pFormation->Update();
		pFormation->SetUpdate(false);
		m_vEnemyPos = pBeacon->GetPos();
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
			unitItr->SetMoving(); // force all units to move to (new) formation points when target and formation move
	}

//	if(m_DangerPoints.empty())
	{
		for(TUnitList::iterator unitItr=GetUnitsList().begin(); unitItr!=itEnd; ++unitItr)
		{
			CUnitImg &curUnit = (*unitItr);
			CAIObject* pUnit = curUnit.m_pUnit;
			CPuppet* pPuppet = pUnit->CastToCPuppet();
			if(pUnit->IsEnabled() && pPuppet)
			{
				IAIObject* pTarget = pPuppet->GetAttentionTarget();
				if(curUnit.IsMoving() || 
					curUnit.IsFollowing() && (!pTarget || !pTarget->IsAgent()))
				{
					curUnit.ClearMoving();
					pe_status_dynamics  dSt;
					Vec3 velocity(ZERO);
					IUnknownProxy*  pProxy;
					IPhysicalEntity* pPhysics;
					if((pProxy = pUnit->GetProxy()) && (pPhysics = pProxy->GetPhysics()))
					{
						pPhysics->GetStatus( &dSt );
						velocity = dSt.v;
						if(velocity.GetLengthSquared() < 1)
						{
							// change position, can't see any live target and I'm not moving
							int iPointIndex = GetFormationPointWithTarget(curUnit);
							if(iPointIndex>0 && iPointIndex != m_pLeader->GetFormationPointIndex(pUnit))
							{
								m_pLeader->AssignFormationPointIndex(curUnit,iPointIndex);
								//pPiper->SetLastOpResult(pMyFormationPoint);
								CAIActor* pAIActor = pUnit->CastToCAIActor();
								pAIActor->SetSignal(1,"OnAttackChase");
								curUnit.ClearFollowing();
							}
						}
					}
				}
			}
		}
	}

	return ACTION_RUNNING;
}

int CLeaderAction_Attack_Chase::GetFormationPointWithTarget(CUnitImg& unit)
{
	const float fMinDistanceToNextPoint = 5;
	CAIObject* pBeacon = (CAIObject*)GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
	
	CPipeUser* pUnit = unit.m_pUnit->CastToCPipeUser();
	if(!pUnit)
		return -1;

	int index = -1;
	int myCurrentPointIndex = unit.m_FormationPointIndex;
	if(pBeacon)
	{
		// TO DO: get the formation from the leader directly (beacon will not own the formation after refactoring)
		CFormation* pFormation = pBeacon->m_pFormation;
		if(pFormation)
		{
			//CAIObject* pMyPoint = myCurrentPointIndex ? pFormation->GetFormationPoint(myCurrentPointIndex) : NULL;
			//float myPointPosZ(pMyPoint ? pMyPoint->GetPos().z : pUnit->GetPos().z );
			//float zDisp = pUnit->GetPos().z - myPointPosZ;
			SAIBodyInfo bodyInfo;
			pUnit->GetProxy()->QueryBodyInfo( bodyInfo);
			IEntity* pUnitEntity = pUnit->GetEntity();
			float zDisp = (pUnitEntity ? bodyInfo.vEyePos.z - pUnitEntity->GetWorldPos().z : 0);

			int size = pFormation->GetSize();

			IPhysicalWorld *pWorld = GetAISystem()->GetPhysicalWorld();
			ray_hit hit;
			int rayresult(0);

			CAIObject* pTarget = (CAIObject*)pUnit->GetAttentionTarget();
			if(!pTarget)
				pTarget = pBeacon;
			IUnknownProxy* pProxy = pTarget->GetProxy();
			IPhysicalEntity* pTargetPhysics = pProxy ? pProxy->GetPhysics(): NULL;
			if(!pTargetPhysics)
			{
				IAIObject* pAssociation;
				if(pTarget->GetSubType()==IAIObject::STP_MEMORY && (pAssociation = pTarget->GetAssociation()))
				{
					if((pProxy = pAssociation->GetProxy()))
						pTargetPhysics = pProxy->GetPhysics();
				}
			}

			Vec3 beaconpos = pTarget->GetPos();

			float mindist = 100000000.f;			

			Vec3 unitpos = pUnit->GetPos();

			switch(unit.m_Group)
			{
			case AI_MOVE_BACKWARD: // move back to the target
				{
					Vec3 vY = pTarget->GetViewDir();
					if(fabs(vY.z)<0.0001f)
						vY = pTarget->GetBodyDir();
					vY.z=0; // 2D only
					vY.NormalizeSafe(); 
					unitpos = beaconpos - vY*10;
				}
				break;
			case AI_MOVE_LEFT: // move left to the target
				{
					Vec3 vY = pTarget->GetViewDir();
					if(fabs(vY.z)<0.0001f)
						vY = pTarget->GetBodyDir();
					vY.z=0; // 2D only
					vY.NormalizeSafe(); 
					Vec3 vX = vY % Vec3(0,0,1);
					unitpos = beaconpos - vX*2*fMinDistanceToNextPoint;
				}
				break;
			case AI_MOVE_RIGHT: // move left to the target
				{
					Vec3 vY = pTarget->GetViewDir();
					if(fabs(vY.z)<0.0001f)
						vY = pTarget->GetBodyDir();
					vY.z=0; // 2D only
					vY.NormalizeSafe(); 
					Vec3 vX = vY % Vec3(0,0,1);
					unitpos = beaconpos + vX*2*fMinDistanceToNextPoint;
				}
				break;
			case AI_MOVE_RIGHT+AI_MOVE_BACKWARD: // move left to the target
				{
					Vec3 vY = pTarget->GetViewDir();
					if(fabs(vY.z)<0.0001f)
						vY = pTarget->GetBodyDir();
					vY.z=0; // 2D only
					vY.NormalizeSafe(); 
					Vec3 vX = vY % Vec3(0,0,1);
					unitpos = beaconpos + (vX-vY).GetNormalizedSafe()*2*fMinDistanceToNextPoint;
				}
				break;
			case AI_MOVE_LEFT+AI_MOVE_BACKWARD: // move left to the target
				{
					Vec3 vY = pTarget->GetViewDir();
					if(fabs(vY.z)<0.0001f)
						vY = pTarget->GetBodyDir();
					vY.z=0; // 2D only
					vY.NormalizeSafe(); 
					Vec3 vX = vY % Vec3(0,0,1);
					unitpos = beaconpos - (vX+vY).GetNormalizedSafe()*2*fMinDistanceToNextPoint;
				}
				break;
			default:
				break;
			}

			float fMinDistanceToNextPoint2 = fMinDistanceToNextPoint*fMinDistanceToNextPoint;
			for(int i=1; i<size;i++)//exclude owner's point
			{
				Vec3 pos;
				CAIObject* pPointOwner = pFormation->GetPointOwner(i);
				if(pPointOwner == pUnit)
					myCurrentPointIndex = i;
				if(pPointOwner)
					continue;
				CAIObject* pFormationPoint = pFormation->GetFormationPoint(i);
				if(pFormationPoint)
					pos = pFormationPoint->GetPos();
				else
					continue;

				{
					{
						// rough visibility check with raytrace
/*
						// check if the current point is not in a danger region
						bool bDanger = false;
						TDangerPointList::iterator itp = m_DangerPoints.begin(), itpEnd = m_DangerPoints.end();
						for(; itp != itpEnd;++itp)
						{
							float r = itp->radius;
							r *=r;
							if(Distance::Point_Point2DSq(itp->point,pos)<=r)
							{
								bDanger = true;
								break;
							}
							else if((pos - unitpos).GetNormalizedSafe().Dot((itp->point - unitpos).GetNormalizedSafe()) >0.1f)
							{
								bDanger = true;
								break;
							}
						}

						if(bDanger)
							continue;

						// tweak the right Z position for formation point - the same Z as the requestor being at the formation point
						pos.z += zDisp;
						int rayresult = 0;
						if(!m_bVisibilityChecked)
						{
							TICK_COUNTER_FUNCTION
								TICK_COUNTER_FUNCTION_BLOCK_START
								rayresult = pWorld->RayWorldIntersection(pos, pTarget->GetPos() - pos, COVER_OBJECT_TYPES|ent_living, HIT_COVER, &hit, 1,pTargetPhysics);
							TICK_COUNTER_FUNCTION_BLOCK_END
						}
*/
						// TO DO: optimization with m_bVisibilityChecked and m_Targetvisiblep[] 
						// works perfectly only if there's one enemy
//						if(!rayresult || m_PointProperties[i].bTargetVisible) 
						{
							float dist2 = fabs(Distance::Point_Point2DSq(unitpos,pos) - fMinDistanceToNextPoint2);
							if(unit.m_Group==AI_BACKOFF_FROM_TARGET || unit.GetClass()==UNIT_CLASS_LEADER)
								dist2 -=Distance::Point_Point2DSq(beaconpos,pos);
							if(dist2 < mindist)
							{
								mindist = dist2;
//								m_PointProperties[i].bTargetVisible = true;
								index = i;
							}
						}
					}
				}
			}
			//m_bVisibilityChecked = true;
		}
	}

	unit.m_Group =0;

	if(index<0 && myCurrentPointIndex>0)
		return myCurrentPointIndex;

	return index;
}

//
//----------------------------------------------------------------------------------------------------
bool CLeaderAction_Attack_Chase::ProcessSignal(const AISIGNAL& signal)
{
	if (signal.Compare(g_crcSignals.m_nOnFormationPointReached))
	{
		IEntity* pUnitEntity = signal.pSender;
		CAIObject* pUnit;
		if(pUnitEntity && (pUnit = (CAIObject*)(pUnitEntity->GetAI())))
		{
			CAIActor* pAIActor = pUnit->CastToCAIActor();
			TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pAIActor);
			if(itUnit!=GetUnitsList().end())
				itUnit->SetFollowing();
		}
		return true;
	}
	else if(signal.Compare(g_crcSignals.m_nAIORD_ATTACK))
	{
		if(signal.pEData && static_cast<ELeaderActionSubType>(signal.pEData->iValue) == m_eActionSubType)
			return true;
	}
	else 	if(signal.Compare(g_crcSignals.m_nOnRequestUpdate))
	{
		IEntity* pUnitEntity = signal.pSender;
		CAIObject* pUnit;
		if(pUnitEntity && (pUnit = (CAIObject*)(pUnitEntity->GetAI())))
		{
			CPipeUser* pPiper = pUnit->CastToCPipeUser();
			if(pPiper)
			{
				CAIObject* pTarget = (CAIObject*)pPiper->GetAttentionTarget();

//				UpdatePointList(pTarget);

				TUnitList::iterator itUnit = m_pLeader->GetAIGroup()->GetUnit(pPiper);
				if(itUnit!=GetUnitsList().end())
				{
					itUnit->ClearFollowing();
					itUnit->SetMoving();
/*					if(signal.pEData)
					{
						if (signal.pEData->fValue>0)
						{
							itUnit->SetHiding();
							m_fMinDistanceToNextPoint = signal.pEData->fValue;
						}
					}
*/
				}
			}
		}
		return true;
	}
	else 	if(signal.Compare(g_crcSignals.m_nOnSetPredictionTime))
	{
		if(signal.pEData)
			m_fPredictionTimeSpan = signal.pEData->fValue;
		return true;
	}
	
	return false;
}

void CLeaderAction_Attack_Chase::UpdateBeacon() const
{
	IAIObject* pTarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true,false);
	if(pTarget)
	{
		if(pTarget->GetAIType()==AIOBJECT_DUMMY)
			pTarget = (static_cast<CAIObject*>(pTarget))->GetAssociation();

		//check if target is on vehicle
		float speed = GetTargetSpeed(pTarget);

		CPuppet* pPuppet = CastToCPuppetSafe(pTarget);
		if(!pPuppet)
			return;

		Vec3 predictedPos(pTarget->GetPos());
		if(speed>0.1f)
		{
			float distance = speed*m_fPredictionTimeSpan;
			float distToEnemy = Distance::Point_Point(predictedPos,m_pLeader->GetAIGroup()->GetAveragePosition());
			if(distance < distToEnemy)
				distance = distToEnemy;
			if(distance < m_fMinDistance)
				distance = m_fMinDistance;
			pPuppet->m_Path.GetPosAlongPath(predictedPos,distance,false,false);
			GetAISystem()->UpdateBeacon(m_pLeader->GetGroupId(), predictedPos, m_pLeader);
		}
	}	

}

float CLeaderAction_Attack_Chase::GetTargetSpeed(IAIObject* pTarget) const
{
	IEntity* pTargetEntity = pTarget->GetEntity();
	if(pTargetEntity)
	{
		int i=10; //just for precautions
		bool bParent = false;
		while(pTargetEntity->GetParent() && --i>0)
		{
			bParent = true;
			pTargetEntity = pTargetEntity->GetParent();
		}
		if(bParent)
			pTarget = pTargetEntity->GetAI();
	}

	float speed=0;
	IPhysicalEntity *phys = pTargetEntity->GetPhysics();
	if(phys)
	{
		pe_status_dynamics	dyn;
		phys->GetStatus(&dyn);
		speed = dyn.v.GetLength();
	}
	return speed;
}

//-------------------------------------------------------------------------------------
// SERIALIZATION FUNCTIONS
//-------------------------------------------------------------------------------------


//
//----------------------------------------------------------------------------------------------------
void CLeaderAction::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	//objectTracker.SerializeObjectPointer(ser, "m_pLeader", m_pLeader, false);
	ser.EnumValue("m_ActionType", m_eActionType, LA_NONE, LA_LAST);
	ser.EnumValue("m_eActionSubType", m_eActionSubType, LAS_DEFAULT, LAS_LAST);
	ser.Value("m_Priority", m_Priority);
	ser.Value("m_unitProperties", m_unitProperties);
	ser.EnumValue("m_currentNavType", m_currentNavType, IAISystem::NAV_UNSET, IAISystem::NAV_MAX_VALUE);
	ser.Value("m_NavProperties", m_NavProperties);
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Follow::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction::Serialize(ser,objectTracker);
	ser.Value("m_FiringTime", m_FiringTime);
	ser.Value("m_movingTime", m_movingTime);
	ser.Value("m_groupMovingTime", m_groupMovingTime);
	ser.Value("m_bLeaderFiring", m_bLeaderFiring);
	ser.Value("m_bLeaderMoving", m_bLeaderMoving);
	ser.Value("m_bLeaderBigMoving", m_bLeaderBigMoving);
	ser.Value("m_notMovingCount",m_notMovingCount);
	ser.Value("m_vLastUpdatePos", m_vLastUpdatePos);
	ser.Value("m_vAverageLeaderMovingDir",m_vAverageLeaderMovingDir);
	ser.Value("m_szFormationName",m_szFormationName);
	ser.Value("m_iGroupID",m_iGroupID);
	ser.Value("m_vTargetPos",m_vTargetPos);
	objectTracker.SerializeObjectPointer(ser,"m_pSingleUnit",m_pSingleUnit,false);
	ser.Value("m_bInitialized",m_bInitialized);

}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Use_Vehicle::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction::Serialize( ser, objectTracker );
	objectTracker.SerializeObjectPointer(ser, "m_pDriver", m_pDriver, false);
	ser.Value("m_VehicleID", m_VehicleID);
	ser.Value("m_vInitPos", m_vInitPos);
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction::Serialize( ser, objectTracker );
	ser.Value("m_bInitialized", m_bInitialized);
	ser.Value("m_bNoTarget", m_bNoTarget);
	ser.Value("m_timeRunning", m_timeRunning);
	ser.Value("m_timeLimit", m_timeLimit);
	ser.Value("m_bApproachWithNoObstacle",m_bApproachWithNoObstacle);
	ser.Value("m_vDefensePoint", m_vDefensePoint);
	ser.Value("m_vEnemyPos", m_vEnemyPos);

}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_Row::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction_Attack::Serialize( ser, objectTracker );
	ser.Value("m_vRowNormalDir", m_vRowNormalDir);
	ser.Value("m_vBasePoint",m_vBasePoint);
	ser.Value("m_fSize",m_fSize);
	ser.Value("m_fInitialDistance",m_fInitialDistance);
	ser.Value("m_bTimerRunning",m_bTimerRunning);
}
//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_Front::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction_Attack_Row::Serialize( ser, objectTracker );
	ser.Value("m_vUpdateEnemyPos", m_vUpdateEnemyPos);
	ser.Value("m_numUnitsBehind", m_numUnitsBehind);
	ser.Value("m_bInvestigating",m_bInvestigating);

}
//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_Chain::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction_Attack::Serialize( ser, objectTracker );
	ser.Value("m_vUpdateEnemyPos", m_vUpdateEnemyPos);
	ser.Value("m_nextStatusTime",m_nextStatusTime);
	ser.EnumValue("m_status",m_status,STATUS_APPROACH,STATUS_STRAFE);
	ser.Value("m_vNearestEnemyPos",m_vNearestEnemyPos);
}
//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_Flank::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction_Attack::Serialize( ser, objectTracker );
	ser.Value("m_vStartPos",m_vStartPos);
	ser.Value("m_iFlankingUnitsCount",m_iFlankingUnitsCount);
	ser.Value("m_iFlankingUnits",m_iFlankingUnits);
	ser.Value("m_iStage",m_iStage);
	ser.Value("m_fFlankDirection",m_fFlankDirection);
	ser.Value("m_vAveragePos",m_vAveragePos);
	ser.Value("m_bHiding",m_bHiding);

}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_FollowLeader::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction_Attack::Serialize( ser, objectTracker );
	ser.Value("m_vLastUpdatePos",m_vLastUpdatePos);
	ser.Value("m_sFormationType",m_sFormationType);
	ser.Value("m_lastTimeMoving", m_lastTimeMoving);
	ser.Value("m_bLeaderMoving",m_bLeaderMoving);

	ser.BeginGroup("CLeaderAction_Attack_FollowLeader_spots");
	{
		char name[32];
		int spotsize=m_Spots.size();
		ser.Value("Spots_size",spotsize);
		if(ser.IsReading())
		{
			m_Spots.clear();
			m_Spots.resize(spotsize);
		}
		for(int i=0;i<spotsize;i++)
		{
			sprintf(name,"Spot_reserveTime_%d",i);
			ser.Value(name,m_Spots[i].reserveTime);
			sprintf(name,"Spot_Owner_%d",i);
			objectTracker.SerializeObjectPointer(ser,name,m_Spots[i].pOwner,false);
		}
		ser.EndGroup();
	}
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Search::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction::Serialize( ser, objectTracker );
	ser.Value("m_timeRunning", m_timeRunning);
	ser.Value("m_iCoverUnitsLeft",m_iCoverUnitsLeft);
	ser.Value("m_vEnemyPos",m_vEnemyPos);
	ser.Value("m_fSearchDistance",m_fSearchDistance);
	ser.Value("m_bInitialized",m_bInitialized);
	ser.Value("m_bUseHideSpots",m_bUseHideSpots);
	ser.Value("m_iSearchSpotAIObjectType",m_iSearchSpotAIObjectType);

	ser.BeginGroup("HideSpots");
	if(ser.IsReading())
		m_HideSpots.clear();
	TPointMap::iterator it = m_HideSpots.begin();
	char name[16];
	int count = m_HideSpots.size();
	SSearchPoint hp;
	float dist2;
	ser.Value("count",count);
	for(int i=0;i<count;++i)
	{
		if(ser.IsWriting())
		{
			dist2 = it->first;
			hp.pos = it->second.pos;
			hp.dir = it->second.dir;
			hp.bReserved = it->second.bReserved;
			++it;
		}
		sprintf(name,"HideSpot%d",i);
		ser.BeginGroup(name);
		ser.Value("distance2",dist2);
		hp.Serialize(ser,objectTracker);
		ser.EndGroup();
		if(ser.IsReading())
			m_HideSpots.insert(std::make_pair(dist2,hp));
	}
	ser.EndGroup();
}


/*
//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_FollowHiding::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
CLeaderAction::Serialize( ser, objectTracker );
ser.Value("m_lastLeaderUpdatePos", m_lastLeaderUpdatePos);
objectTracker.SerializeObjectPointer(ser, "m_pSingleUnit", m_pSingleUnit, false);

}
*/
//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_LeapFrog::Serialize( TSerialize ser,  CObjectTracker& objectTracker )
{
	CLeaderAction_Attack::Serialize(ser,objectTracker);
	ser.Value("m_fApproachDistance", m_fApproachDistance);
	ser.Value("m_bApproached", m_bApproached);
	ser.Value("m_bCoverApproaching", m_bCoverApproaching);
	ser.Value("m_dist0", m_dist[0]);
	ser.Value("m_dist1", m_dist[1]);
	//objectTracker.SerializeObjectPointer(ser, "m_pLiveTarget", m_pLiveTarget, false);
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_CoordinatedFire::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction_Attack::Serialize(ser,objectTracker);
	ser.EnumValue("m_updateStatus", m_updateStatus,ACTION_RUNNING,ACTION_FAILED);
}

//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_SwitchPositions::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction_Attack::Serialize(ser,objectTracker);
	objectTracker.SerializeObjectContainer(ser,"m_PointProperties",m_PointProperties);
	ser.Value("m_sFormationType",m_sFormationType);
	ser.Value("m_bVisibilityChecked",m_bVisibilityChecked);
	ser.Value("m_fMinDistanceToNextPoint",m_fMinDistanceToNextPoint);
	ser.Value("m_bPointsAssigned",m_bPointsAssigned);
	objectTracker.SerializeObjectContainer(ser,"m_DangerPoints",m_DangerPoints);
	ser.Value("m_bAvoidDanger",m_bAvoidDanger);
	ser.Value("m_vUpdatePointTarget",m_vUpdatePointTarget);
	ser.Value("m_fFormationSize",m_fFormationSize);
	ser.Value("m_fFormationScale",m_fFormationScale);
	ser.Value("m_fDistanceToTarget",m_fDistanceToTarget);
	ser.Value("m_fMinDistanceToTarget",m_fMinDistanceToTarget);
	ser.Value("m_lastScaleUpdateTime",m_lastScaleUpdateTime);


	ser.BeginGroup("TargetData");
	{
		int size = m_TargetData.size();
		ser.Value("size",size);
		TMapTargetData::iterator it = m_TargetData.begin();
		if(ser.IsReading())
			m_TargetData.clear();

		STargetData targetData;
		CAIActor* pUnitActor;
		char name[32];

		for(int i=0;i<size;i++)
		{
			sprintf(name,"target_%d",i);
			ser.BeginGroup(name);
			{
				if(ser.IsWriting())
				{
					pUnitActor = it->first;
					targetData = it->second;
					++it;
				}
				objectTracker.SerializeObjectPointer(ser,"pUnitActor",pUnitActor,false);
				targetData.Serialize(ser,objectTracker);
				if(ser.IsReading())
					m_TargetData.insert(std::make_pair(pUnitActor,targetData));
			}
			ser.EndGroup();

		}
		ser.EndGroup();
	}

	ser.BeginGroup("SpecialActions");
	{
		int size = m_SpecialActions.size();
		ser.Value("size",size);
		TSpecialActionMap::iterator it = m_SpecialActions.begin();
		if(ser.IsReading())
			m_SpecialActions.clear();

		SSpecialAction specialAction;
		const CAIActor* pTargetActor;
		char name[32];

		for(int i=0;i<size;i++)
		{
			sprintf(name,"target_%d",i);
			ser.BeginGroup(name);
			{
				if(ser.IsWriting())
				{
					pTargetActor = it->first;
					specialAction = it->second;
					++it;
				}
				objectTracker.SerializeObjectPointer(ser,"pTargetActor",pTargetActor,false);
				specialAction.Serialize(ser,objectTracker);
				if(ser.IsReading())
					m_SpecialActions.insert(std::make_pair(pTargetActor,specialAction));
			}
			ser.EndGroup();

		}
		ser.EndGroup();
	}

	m_ActorForbiddenSpotManager.Serialize(ser,objectTracker);
}


//
//----------------------------------------------------------------------------------------------------
void CLeaderAction_Attack_Chase::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	CLeaderAction_Attack::Serialize(ser,objectTracker);
	ser.Value("m_sFormationType",m_sFormationType);
	ser.Value("m_bPointsAssigned",m_bPointsAssigned);
	ser.Value("m_fPredictionTimeSpan",m_fPredictionTimeSpan);
	ser.Value("m_fMinDistance",m_fMinDistance);
	ser.Value("m_fMinSpeed",m_fMinSpeed);
	ser.Value("m_vOldPosition",m_vOldPosition);
}
