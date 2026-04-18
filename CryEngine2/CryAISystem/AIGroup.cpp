/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   AIGroup.cpp
$Id$
Description: 

-------------------------------------------------------------------------
History:
- 2006				: Created by Mikko Mononen

*********************************************************************/


#include "StdAfx.h"
#include "IAISystem.h"
#include "CAISystem.h"
#include "AIPlayer.h"
#include "AIGroup.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include <float.h>
#include "AIGroupTactic.h"
#include "AIDebugDrawHelpers.h"


//====================================================================
// CAIGroup
//====================================================================
CAIGroup::CAIGroup(int groupID) :
	m_groupID(groupID),
	m_pLeader(0),
	m_bUpdateEnemyStats(true),
	m_vEnemyPositionKnown(0,0,0),
	m_vAverageEnemyPosition(0,0,0),
	m_vAverageLiveEnemyPosition(0,0,0),
	m_lastUpdateTime(-1.0f),
	m_countMax(0),
	m_countEnabledMax(0),
	m_reinforcementSpotsAcquired(false),
	m_reinforcementState(REINF_NONE),
	m_countDead(0),
	m_countCheckedDead(0),
	m_pReinfPendingUnit(NULL)
{
}

//====================================================================
// ~CAIGroup
//====================================================================
CAIGroup::~CAIGroup()
{
	for(TacticMap::iterator it = m_tactics.begin(); it != m_tactics.end(); ++it)
		delete it->second;
	m_tactics.clear();
	/*IAIObject* pBeacon = GetAISystem()->GetBeacon(m_groupID);
	if(pBeacon)
		GetAISystem()->RemoveDummyObject((CAIObject*)pBeacon);
		*/
}

//====================================================================
// SetLeader
//====================================================================
void CAIGroup::SetLeader(CLeader* pLeader)
{
#ifdef _DEBUG
	if(m_pLeader && pLeader && m_pLeader != pLeader)
		AILogComment("CAIGroup::SetLeader(): Overriding leader '%s' with '%s' (groupId:%d).", m_pLeader->GetName(), pLeader->GetName(), m_groupID);
#endif
	m_pLeader = pLeader;
	if(pLeader)
		pLeader->SetAIGroup(this);
}

//====================================================================
// GetLeader
//====================================================================
CLeader* CAIGroup::GetLeader()
{
	return m_pLeader;
}

//====================================================================
// AddMember
//====================================================================
void	CAIGroup::AddMember(CAIActor* pMember)
{
	for(TacticMap::iterator it = m_tactics.begin(); it != m_tactics.end(); ++it)
		it->second->OnMemberAdded(pMember);

	// everywhere assumes that pMember is non-null
	AIAssert(pMember);
	// check if it's already a member of team
	if(GetUnit(pMember) != m_Units.end())
		return;
	CUnitImg	newUnit;
	newUnit.m_pUnit = pMember;
	SetUnitClass(newUnit);

	IEntity* pEntity = pMember->GetEntity();
	if(pEntity)
	{
		IPhysicalEntity* pPhysEntity = pEntity->GetPhysics();
		if(pPhysEntity)
		{
			float radius;
			pe_status_pos status;
			pPhysEntity->GetStatus( &status );
			radius = fabsf(status.BBox[1].x - status.BBox[0].x);
			float radius2 = fabsf(status.BBox[1].y - status.BBox[0].y);
			if(radius < radius2)
				radius = radius2;
			float myHeight = status.BBox[1].z - status.BBox[0].z;
			newUnit.SetWidth(radius);
			newUnit.SetHeight(myHeight);
		}
	}
	m_Units.push_back(newUnit);

	SetUnitRank((IAIObject*)pMember);

	if(m_pLeader)
	{
		m_pLeader->m_pObstacleAllocator->AddUnit(pMember);
		m_pLeader->AddUnitNotify(pMember);
	}
}

//====================================================================
// RemoveMember
//====================================================================
bool	CAIGroup::RemoveMember(CAIActor* pMember)
{
	if(pMember == m_pReinfPendingUnit)
		NotifyReinfDone( pMember, false );

	for(TacticMap::iterator it = m_tactics.begin(); it != m_tactics.end(); ++it)
		it->second->OnMemberRemoved(pMember);

	if (pMember->GetProxy() && pMember->GetProxy()->GetActorHealth() <= 0)
		m_countDead++;

	TUnitList::iterator theUnit = std::find(m_Units.begin(), m_Units.end(), pMember);

	if(m_pLeader)
	{
		CAIActor* pMemberActor = pMember->CastToCAIActor();
		if(pMember == m_pLeader->GetFormationOwner() && pMemberActor->GetGroupId() == m_pLeader->GetGroupId())
			m_pLeader->ReleaseFormation();

		if(m_pLeader->GetAssociation() == pMember)
			m_pLeader->SetAssociation(NULL);
	}

	RemoveUnitFromRankList(pMember);

	if(theUnit != m_Units.end())
	{
		m_Units.erase(theUnit);
		if(m_pLeader)
		{
			// if empty group - remove the leader; nobody owns it, will hang there forever
// ok, don't remove leader here - all serialization issues are taken care of; make sure chain-loading is fine
//			if(m_Units.empty())
//			{
//				if(deleting)
//				{
//					GetAISystem()->RemoveObject(m_pLeader);
//					m_pLeader = 0;
//				}
//			}
//			else
			if(!m_Units.empty())
			{
				m_pLeader->m_pObstacleAllocator->RemoveUnit(pMember);
				m_pLeader->DeadUnitNotify(pMember);
			}
		}
		m_bUpdateEnemyStats = true;

		return true;
	}
	return false;
}

//====================================================================
// NotifyBodyChecked
//====================================================================
void CAIGroup::NotifyBodyChecked(const CAIObject* pObject)
{
	m_countCheckedDead++;
}

//====================================================================
// GetUnit
//====================================================================
TUnitList::iterator CAIGroup::GetUnit(const CAIActor * pAIObject) 
{
	return std::find(m_Units.begin(),m_Units.end(), pAIObject);
}


//====================================================================
// SetUnitClass
//====================================================================
void CAIGroup::SetUnitClass(CUnitImg& UnitImg) const
{
	IEntity* pEntity = UnitImg.m_pUnit->GetEntity();
	if(pEntity)
	{
		IScriptTable* pTable = pEntity->GetScriptTable();
		SmartScriptTable pEntityProperties;
		if(pTable && pTable->GetValue("Properties",pEntityProperties))
		{
			const char* sCharacter=NULL;
			pEntityProperties->GetValue("aicharacter_character",sCharacter);
			IScriptSystem * pSS = pTable->GetScriptSystem();
			if(pSS)
			{
				SmartScriptTable pGlobalCharacterTable;
				if(pSS->GetGlobalValue("AICharacter",pGlobalCharacterTable))
				{
					SmartScriptTable pMyCharacterTable;
					if(pGlobalCharacterTable->GetValue(sCharacter,pMyCharacterTable))
					{
						int soldierClass;
						if(pMyCharacterTable->GetValue("Class", soldierClass))
						{
							UnitImg.SetClass(static_cast<int>(soldierClass));
							return;
						}
					}
				}
			}
		}
	}
	UnitImg.SetClass(UNIT_CLASS_UNDEFINED);
}


//====================================================================
// IsMember
//====================================================================
bool CAIGroup::IsMember(const IAIObject* pMember ) const
{
	const CAIActor* pActor = pMember->CastToCAIActor();
	for(TUnitList::const_iterator itUnit = m_Units.begin(); itUnit != m_Units.end(); ++itUnit)
		if ((*itUnit)==pActor)
			return true;
	return false;
}

//====================================================================
// SetUnitProperties
//====================================================================
void CAIGroup::SetUnitProperties( const IAIObject* obj, uint32 properties )
{
	const CAIActor* pActor = obj->CastToCAIActor();
	TUnitList::iterator senderUnit = std::find(m_Units.begin(), m_Units.end(), pActor);

	if (senderUnit != m_Units.end())
		senderUnit->SetProperties(properties);
}

//====================================================================
// GetAveragePosition
//====================================================================
Vec3 CAIGroup::GetAveragePosition(IAIGroup::eAvPositionMode mode, uint32 unitClassMask) const
{
	int count = 0;
	Vec3 average(ZERO);
	TUnitList::const_iterator it, itEnd = m_Units.end();
	switch(mode)
	{
	case AVMODE_ANY:
		for (it = m_Units.begin(); it != itEnd; ++it)
			if (it->m_pUnit->IsEnabled())
			{
				++count;
				average += it->m_pUnit->GetPos();
			}
			break;
	case AVMODE_CLASS:
		for (it = m_Units.begin(); it != itEnd; ++it)
			if (it->m_pUnit->IsEnabled() && (it->GetClass() & unitClassMask))
			{
				++count;
				average += it->m_pUnit->GetPos();
			}
			break;
	case AVMODE_PROPERTIES:
		for (it = m_Units.begin(); it != itEnd; ++it)
			if (it->m_pUnit->IsEnabled() && (it->GetProperties() & unitClassMask))
			{
				++count;
				average += it->m_pUnit->GetPos();
			}
			break;
	default:
		break;
	}
	return (count? average / float(count): average);
}

//====================================================================
// GetUnitCount
//====================================================================
int CAIGroup::GetUnitCount(uint32 unitPropMask) const
{

	int count = 0;
	TUnitList::const_iterator it, itEnd = m_Units.end();
	for (it = m_Units.begin(); it != itEnd; ++it)
		if (it->m_pUnit->IsEnabled() && (it->GetProperties() & unitPropMask))
			++count;
	return count;
}

//====================================================================
// GetAttentionTarget
//====================================================================
IAIObject* CAIGroup::GetAttentionTarget(bool bHostileOnly, bool bLiveOnly, const IAIObject* pSkipTarget) const
{
	// to do: give more priority to closer targets?
	IAIObject* selectedTarget = NULL;
	bool bHostileFound = false;

	TUnitList::const_iterator it, itEnd = m_Units.end();
	for (it = m_Units.begin(); it != itEnd; ++it)
	{
		CPipeUser* pPipeUser = it->m_pUnit->CastToCPipeUser();
		if (pPipeUser)
		{
			CAIObject* attentionTarget = static_cast<CAIObject*>(pPipeUser->GetAttentionTarget());
			if (attentionTarget && (attentionTarget->IsEnabled() || attentionTarget->GetAIType()==AIOBJECT_VEHICLE) && attentionTarget!=pSkipTarget)
			{	
				bool bLive = attentionTarget->IsAgent();
				if(pPipeUser->IsHostile(attentionTarget)) 
				{
					if(bLive)
						return attentionTarget;//give priority to live hostile targets
					else if(!bLiveOnly)
					{
						bHostileFound = true;
						selectedTarget = attentionTarget; // then to hostile non live targets
					}
				}
				else if(!bHostileFound && !bHostileOnly && (bLive ||!bLiveOnly))
					selectedTarget = attentionTarget; // then to non hostile targets
			}
		}
	}
	if(m_pLeader && m_pLeader->IsPlayer())
	{
		CAIPlayer* pPlayer = (CAIPlayer*)m_pLeader->GetAssociation();
		if(pPlayer)
		{
			CAIObject* attentionTarget = static_cast<CAIObject*>(pPlayer->GetAttentionTarget());
			if(attentionTarget && attentionTarget->IsEnabled() && attentionTarget!=pSkipTarget) // nothing else to check, assuming that player has only live enemy targets
			{	
				bool bLive = attentionTarget->IsAgent();
				if(pPlayer->IsHostile(attentionTarget)) 
				{
					if(bLive)
						return attentionTarget;//give priority to live hostile targets
					else if(!bLiveOnly)
						selectedTarget = attentionTarget; // then to hostile non live targets
				}
				else if(!bHostileFound && !bHostileOnly && (bLive ||!bLiveOnly))
					selectedTarget = attentionTarget; // then to non hostile targets
			}
		}
	}

	return selectedTarget;
}

//====================================================================
// GetTargetCount
//====================================================================
int CAIGroup::GetTargetCount(bool bHostileOnly, bool bLiveOnly) const
{
	int n = 0;

	TUnitList::const_iterator it, itEnd = m_Units.end();
	for (it = m_Units.begin(); it != itEnd; ++it)
	{
		CPipeUser* pPipeUser = it->m_pUnit->CastToCPipeUser();
		if (pPipeUser)
		{
			CAIObject* attentionTarget = static_cast<CAIObject*>(pPipeUser->GetAttentionTarget());
			if (attentionTarget && attentionTarget->IsEnabled())
			{	
				bool bLive = attentionTarget->IsAgent();
				bool bHostile = pPipeUser->IsHostile(attentionTarget);
				if(!(bLiveOnly && !bLive || bHostileOnly && !bHostile))
					n++;
			}
		}
	}
	if(m_pLeader && m_pLeader->IsPlayer())
	{
		CAIPlayer* pPlayer = (CAIPlayer*)m_pLeader->GetAssociation();
		if(pPlayer)
		{
			CAIObject* attentionTarget = static_cast<CAIObject*>(pPlayer->GetAttentionTarget());
			if(attentionTarget && attentionTarget->IsEnabled()) // nothing else to check, assuming that player has only live enemy targets
			{	
				bool bLive = attentionTarget->IsAgent();
				bool bHostile = pPlayer->IsHostile(attentionTarget);
				if(!(bLiveOnly && !bLive || bHostileOnly && !bHostile))
					n++;
			}
		}
	}

	return n;
}


//====================================================================
// SetUnitRank
//====================================================================
void	CAIGroup::SetUnitRank(const IAIObject* pMember, int rank)
{
	if(!pMember)
		return;
	const CAIActor* pActor = pMember->CastToCAIActor();
	TUnitList::iterator itUnit2 = GetUnit(pActor);
	if(itUnit2!=m_Units.end())
	{
		if(rank==0)
			rank = pActor->GetParameters().m_nRank;
		RemoveUnitFromRankList(pMember);
		m_RankedUnits.insert(std::pair<int,CAIObject*>(rank,(CAIObject*)pMember));
	}
}

//====================================================================
// GetUnitInRank
//====================================================================
IAIObject* CAIGroup::GetUnitInRank(int rank) const
{
	TUnitMap::const_iterator itUnit;
	if(rank<=0)
		itUnit=m_RankedUnits.begin();
	else
		itUnit = m_RankedUnits.find(rank);

	return(itUnit!=m_RankedUnits.end()? static_cast<IAIObject*>(itUnit->second) : NULL);
}

//====================================================================
// RemoveUnitFromRankList
//====================================================================
void	CAIGroup::RemoveUnitFromRankList(const IAIObject* pMember)
{
	TUnitMap::iterator itEnd = m_RankedUnits.end();
	TUnitMap::iterator itUnit= m_RankedUnits.begin();
	for(; itUnit!=itEnd;++itUnit)
		if(itUnit->second == pMember)
			break;
	if(itUnit!=itEnd)
		m_RankedUnits.erase(itUnit);
}

//====================================================================
// GetEnemyPosition
//====================================================================
Vec3 CAIGroup::GetEnemyPosition(const CAIObject* pUnit) const
{
	Vec3 enemyPos(0,0,0);
	const CPipeUser* pPipeUser = CastToCPipeUserSafe(pUnit);
	if (pPipeUser)
	{
		IAIObject* pAttTarget = pPipeUser->GetAttentionTarget();
		if (pAttTarget)
		{
			switch (((CAIObject*)pAttTarget)->GetType())
			{
			case AIOBJECT_NONE:
				break;
			case AIOBJECT_DUMMY:
			case AIOBJECT_PLAYER:
			case AIOBJECT_PUPPET:
			case AIOBJECT_WAYPOINT: // Actually this should be Beacon
			default:
				return pAttTarget->GetPos();
			}
		}
		else 
			return m_vAverageEnemyPosition;
	}
	else 
	{
		Vec3 averagePos = GetAveragePosition();

		int memory = 0;
		bool realTarget = false;
		float distance = 10000.f;

		TUnitList::const_iterator it, itEnd = m_Units.end();
		for (it = m_Units.begin(); it != itEnd; ++it)
		{
			if (!it->m_pUnit->IsEnabled())
				continue;

			CAIActor* pActor = it->m_pUnit->CastToCAIActor();
			if(!pActor)
				continue;
			SOBJECTSTATE* state = pActor->GetState();

			IAIObject* pTarget;
			pPipeUser = CastToCPipeUserSafe(it->m_pUnit);
			if (pPipeUser &&
					(pTarget=reinterpret_cast<CAIObject*>(pPipeUser->GetAttentionTarget())) &&
					pPipeUser->IsHostile(pTarget))
			{
				if (!realTarget)
				{
					if (state->eTargetType == AITARGET_MEMORY)
					{
						enemyPos += pTarget->GetPos();
						++memory;
					}
					else
					{
						realTarget = true;
						enemyPos = pTarget->GetPos();
						distance = (enemyPos - averagePos).GetLengthSquared();
					}
				}
				else if (state->eTargetType != AITARGET_MEMORY)
				{
					float thisDistance = (it->m_pUnit->GetPos() - averagePos).GetLengthSquared();
					if (distance > thisDistance)
					{
						enemyPos = pTarget->GetPos();
						distance = thisDistance;
					}
				}
			}
		}
		if(!realTarget && m_pLeader && m_pLeader->IsPlayer())
		{
			CAIPlayer* pPlayer = (CAIPlayer*)m_pLeader->GetAssociation();
			if(pPlayer)
			{
				CAIObject* pTarget = pPlayer->GetAttentionTarget();
				if(pTarget && pTarget->IsEnabled()) // nothing else to check, assuming that player has only live enemy targets
				{
					realTarget = true;
					enemyPos = pTarget->GetPos();
				}
			}
		}

		if (!realTarget && memory)
			enemyPos /= (float) memory;

		//	if(!m_targetPoint.IsZero())
		//	{
		//		float thisDistance = (m_targetPoint - averagePos).GetLengthSquared();
		//		if (distance > thisDistance)
		//			enemyPos = m_targetPoint;
		//	}
	}
	return enemyPos;
}


//====================================================================
// GetEnemyAverageDirection
//====================================================================
Vec3 CAIGroup::GetEnemyAverageDirection(bool bLiveTargetsOnly, bool bSmart) const 
{
	Vec3 avgDir(0,0,0);
	//	TVecTargets FoundTargets;
	//	TVecTargets::iterator itf;
	TSetAIObjects::const_iterator it, itEnd = m_Targets.end();
	int count=0;
	for (it = m_Targets.begin(); it != itEnd; ++it)
	{
		const CAIObject* pTarget = *it;
		bool bLiveTarget = pTarget->IsAgent();
		if(pTarget && (!bLiveTargetsOnly || bLiveTarget) && !pTarget->GetMoveDir().IsZero())
		{	
			// add only not null (significative) directions
			avgDir+=pTarget->GetMoveDir();
			//			FoundTargets.push_back(pTarget);
			count++;
		}

	}	
	if(count)
	{
		avgDir /= float(count);
		/*if(bSmart)// clear the average if it's small enough 
		//(means that the direction vectors are spread in many opposite directions)
		{

		// compute variance
		float fVariance = 0;
		for (itf = FoundTargets.begin(); itf != FoundTargets.end(); ++itf)
		{	CAIObject* pTarget = *itf;
		fVariance+= (pTarget->GetMoveDir() - avgDir).GetLengthSquared();
		}

		if(fVariance>0.7)
		avgDir.Set(0,0,0);

		}*/
		// either normalize or clear
		float fAvgSize = avgDir.GetLength();
		if(fAvgSize> (bSmart ? 0.4f :0.1f))
			avgDir /= fAvgSize;
		else
			avgDir.Set(0,0,0);
	}
	return avgDir;
}

//====================================================================
// UpdateEnemyStats
//====================================================================
void CAIGroup::UpdateEnemyStats()
{
FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );
	Vec3 vOldAverageLiveEnemyPosition = m_vAverageLiveEnemyPosition;
	m_vAverageEnemyPosition.Set(0,0,0);
	m_vAverageLiveEnemyPosition.Set(0,0,0);

	Vec3	averageEnemyDir( 0, 0, 0 );

	//	m_vEnemySize.Set(0,0,0);
	int count = 0, liveCount = 0;
	if(m_bUpdateEnemyStats)
	{
		TUnitList::iterator it, itEnd = m_Units.end();
		m_Targets.clear();
		std::multimap<CAIObject*,CAIObject*> DummyOwnersMap;

		for (it = m_Units.begin(); it != itEnd; ++it)
		{
			//			if (!it->m_pUnit->m_bEnabled)
			//				continue;

			CPuppet* pUnitPuppet = it->m_pUnit->CastToCPuppet();

			if (pUnitPuppet)
			{	
				PotentialTargetMap::iterator itEventEnd = pUnitPuppet->m_perceptionEvents.end();
				bool bAttTargetChecked = false;
				for(PotentialTargetMap::iterator itEvent = pUnitPuppet->m_perceptionEvents.begin();itEvent!=itEventEnd || !bAttTargetChecked; )
				{
					CAIObject* pTarget= NULL;
					if(!bAttTargetChecked && itEvent!=itEventEnd )
					{
						pTarget = itEvent->second.pDummyRepresentation;
						++itEvent;
					}
					else
					{
						pTarget = (CAIObject*)pUnitPuppet->GetAttentionTarget();
						bAttTargetChecked = true;
					}

					if(pTarget && pTarget->IsEnabled() && 	pUnitPuppet->IsHostile(pTarget))
					{
						if(std::find(m_Targets.begin(),m_Targets.end(), pTarget)==m_Targets.end())
						{
							if(pTarget->IsAgent())
							{
								m_vAverageEnemyPosition += pTarget->GetPos();
								m_vAverageLiveEnemyPosition += pTarget->GetPos();
								averageEnemyDir += pTarget->GetMoveDir();
								m_Targets.push_back(pTarget);
								++count;
								++liveCount;
							}
							else if (pTarget->GetSubType()==CAIObject::STP_MEMORY || pTarget->GetSubType()==CAIObject::STP_SOUND) // dummy target
							{	// avoid adding dummy objects (memory, sound etc) with the same owner more than once 
								// (unless they're far enough from each other)
								bool bAdd = true;
								CAIObject* pTargetOwner = static_cast<CAIObject*>(pUnitPuppet->GetEventOwner(pTarget));
								std::multimap<CAIObject*,CAIObject*>::iterator itMO = DummyOwnersMap.find(pTargetOwner);
								if(itMO!=DummyOwnersMap.end())
								{
									for(; itMO!=DummyOwnersMap.end();++itMO)
									{
										if(itMO->first != pTargetOwner)
											break;
										else
										{// a target owned by the same current owner has already been added
											CAIObject* pTarget2 = itMO->second;
											if((pTarget->GetPos() - pTarget2->GetPos()).GetLengthSquared() <0.2 )
											{
												bAdd = false;
												break;
											}
										}
									}
								}
								if(bAdd)
								{
									averageEnemyDir += pTarget->GetMoveDir();
									m_vAverageEnemyPosition += pTarget->GetPos();
									m_Targets.push_back(pTarget);
									DummyOwnersMap.insert(std::make_pair(pTargetOwner,pTarget));
									++count;
								}
							}
						}
					}
				}
			}
		}
		if( m_pLeader && m_pLeader->IsPlayer())
		{
			CAIPlayer* pPlayer = (CAIPlayer*)(m_pLeader->GetAssociation());
			if(pPlayer)
			{
				CAIObject* pTarget = pPlayer->GetAttentionTarget();
				if(pTarget && pTarget->IsEnabled() && pTarget->IsHostile(pPlayer))
        {
					if(std::find(m_Targets.begin(),m_Targets.end(), pTarget)==m_Targets.end())
          {
						m_Targets.push_back(pTarget);
					  m_vAverageEnemyPosition +=pTarget->GetPos();
					  m_vAverageLiveEnemyPosition +=pTarget->GetPos();
					  averageEnemyDir += pTarget->GetMoveDir();
					  count++;
					  ++liveCount;
          }
				}
			}
		}
	}
	else //m_bUpdateEnemyStats
	{
		// update only average positions, using current target list
		TSetAIObjects::iterator it = m_Targets.begin(), itEnd = m_Targets.end();
		for(;it!=itEnd;++it)
		{
			const CAIObject* pTarget = *it;
			if(pTarget)
			{
				m_vAverageEnemyPosition += pTarget->GetPos();
				count++;
				if(pTarget->IsEnabled() && pTarget->IsAgent())
				{
					m_vAverageLiveEnemyPosition +=pTarget->GetPos();
					averageEnemyDir += pTarget->GetMoveDir();
					liveCount++;
				}
			}
		}
	}

	if(count>1)// avoid division by 1 for precision sake	
	{
		m_vAverageEnemyPosition /= (float) count;
		/*		not needed so far
		int size  = TargetList.size();
		for(int i=0; i<size;i++)
		{
		Vec3 pos1 = m_Targets[i]->GetPos();
		for(int j = i+1; j<size;j++)
		{
		Vec3 rpos = pos1 - m_Targets[j]->GetPos();
		float x = fabs(rpos.x);
		if(m_vEnemySize.x < x)
		m_vEnemySize.x = x;
		float y = fabs(rpos.y);
		if(m_vEnemySize.y < y)
		m_vEnemySize.y = y;
		float z = fabs(rpos.z);
		if(m_vEnemySize.z < z)
		m_vEnemySize.z = z;
		}
		}
		*/
	}
	if(liveCount>1)
		m_vAverageLiveEnemyPosition/= float(liveCount);


	m_bUpdateEnemyStats	= false;

	if(m_pLeader)
		m_pLeader->OnEnemyStatsUpdated(m_vAverageLiveEnemyPosition, vOldAverageLiveEnemyPosition);
}

//====================================================================
// OnObjectRemoved
//====================================================================
void CAIGroup::OnObjectRemoved(CAIObject* pObject)
{
	if(!pObject)
		return;
	CAIActor* pActor = pObject->CastToCAIActor();

	for(TacticMap::iterator it = m_tactics.begin(); it != m_tactics.end(); ++it)
		it->second->OnObjectRemoved(pObject);

	TUnitList::iterator it = std::find(m_Units.begin(), m_Units.end(), pActor);
	if(it != m_Units.end())
		RemoveMember(pActor);

	TSetAIObjects::iterator ittgt = std::find(m_Targets.begin(), m_Targets.end(), pObject);
	if(ittgt != m_Targets.end())
	{
		m_bUpdateEnemyStats = true;
		UpdateEnemyStats();
	}

	for (unsigned i = 0; i < m_reinforcementSpots.size(); )
	{
		if (m_reinforcementSpots[i].pAI == pObject)
		{
			m_reinforcementSpots[i] = m_reinforcementSpots.back();
			m_reinforcementSpots.pop_back();
		}
		else
			++i;
	}
}

//====================================================================
// GetForemostUnit
//====================================================================
CAIObject* CAIGroup::GetForemostUnit(const Vec3& direction, uint32 unitProp) const
{
	Vec3 vFurthestPos(0,0,0);
	int iFurthestPosIndex = -1;
	float maxLength = 0;
	CAIObject* pForemost=NULL;

	Vec3 vAvgPos = GetAveragePosition(AVMODE_PROPERTIES,unitProp);

	for (TUnitList::const_iterator it = m_Units.begin();it!=m_Units.end(); ++it)
	{
		const CUnitImg& unit = *it;
		CAIObject* pUnit = unit.m_pUnit;
		if(pUnit->IsEnabled() && ( unit.GetProperties() & unitProp))
		{
			Vec3 vDisplace = pUnit->GetPos() - vAvgPos;
			float fNorm = vDisplace.GetLength();
			if(fNorm>0.001f)//consider an epsilon
			{
				Vec3 vNorm = vDisplace/fNorm;
				float cosine = vNorm.Dot(direction);
				if(cosine>=0) // exclude negative cosine (opposite directions)
				{
					float fProjection = cosine*fNorm;
					if(maxLength<fProjection)
					{
						maxLength = fProjection;
						pForemost = pUnit;
					}
				}
			}
			else if(!pForemost)
			{
				// the unit is exactly at the average position and no one better has been found yet
				pForemost = pUnit;
			}
		}
	}
	return (pForemost);
}

//====================================================================
// GetForemostEnemyPosition
//====================================================================
Vec3 CAIGroup::GetForemostEnemyPosition(const Vec3& direction, bool bLiveOnly) const
{
	const CAIObject* pForemost = GetForemostEnemy(direction,bLiveOnly);
	return (pForemost ? pForemost->GetPos() : ZERO);
}

//====================================================================
// GetForemostEnemy
//====================================================================
const CAIObject* CAIGroup::GetForemostEnemy(const Vec3& direction, bool bLiveOnly) const
{
	Vec3 vFurthestPos(0,0,0);
	int iFurthestPosIndex = -1;
	float maxLength = 0;
	const CAIObject* pForemost=NULL;
	Vec3 vDirNorm = direction.GetNormalizedSafe();
	TSetAIObjects::const_iterator itend = m_Targets.end();
	for (TSetAIObjects::const_iterator it = m_Targets.begin();it!= itend; ++it)
	{
		const CAIObject* pTarget = *it;
		if(!bLiveOnly || pTarget->IsAgent())
		{
			Vec3 vDisplace = pTarget->GetPos() - m_vAverageEnemyPosition;
			float fNorm = vDisplace.GetLength();
			if(fNorm>0.001f)//consider an epsilon
			{
				Vec3 vNorm = vDisplace/fNorm;
				float cosine = vNorm.Dot(vDirNorm);
				if(cosine>=0) // exclude negative cosine (opposite directions)
				{
					float fProjection = cosine*fNorm;
					if(maxLength<fProjection)
					{
						maxLength = fProjection;
						pForemost = pTarget;
					}
				}
			}
			else if(!pForemost)
			{
				// the target is exactly at the average position and no one better has been found yet
				pForemost = pTarget;
			}
		}
	}

	CAISystem* pAISystem(GetAISystem());
	if(!pForemost && !bLiveOnly)
		pForemost = static_cast<CAIObject*>(pAISystem->GetBeacon(m_groupID));

	return pForemost;
}

//====================================================================
// GetEnemyPositionKnown
//====================================================================
Vec3 CAIGroup::GetEnemyPositionKnown( ) const
{
	CAISystem* pAISystem(GetAISystem());
	Vec3 enemyPos;
	bool realTarget = false;
	TUnitList::const_iterator it, itEnd = m_Units.end();
	for (it = m_Units.begin(); it != itEnd; ++it)
	{
		if (!it->m_pUnit->IsEnabled())
			continue;

		CAIActor* pActor = it->m_pUnit->CastToCAIActor();
		if(!pActor)
			continue;

		CPipeUser *pPipeUser = it->m_pUnit->CastToCPipeUser();
		if (pPipeUser &&
				pPipeUser->GetAttentionTarget() &&
				pPipeUser->IsHostile(pPipeUser->GetAttentionTarget()))
		{
			CAIObject* pTarget = (CAIObject*)pPipeUser->GetAttentionTarget();
			if (pTarget->IsEnabled() && pTarget->IsAgent())
			{
				if (pPipeUser->GetAttentionTargetType() != AITARGET_MEMORY)
				{
					realTarget = true;
					enemyPos = pTarget->GetPos();//state->vTargetPos; Luciano - it was zero ?
					break;
				}
			}
		}
	}

	if(!realTarget)
	{
		if(m_pLeader && m_pLeader->IsPlayer())
		{
			CAIPlayer* pPlayer = (CAIPlayer*)m_pLeader->GetAssociation();
			if(pPlayer)
			{
				CAIObject* pTarget = pPlayer->GetAttentionTarget();
				if(pTarget && pTarget->IsEnabled()) // nothing else to check, assuming that player has only live enemy targets
				{
					realTarget = true;
					enemyPos = pTarget->GetPos();
				}
			}
		}
		if(!realTarget)
		{
			if(pAISystem->GetBeacon(m_groupID))
				enemyPos = pAISystem->GetBeacon(m_groupID)->GetPos();
			else
				enemyPos.Set(0,0,0);
		}
	}
	return enemyPos;
}

//====================================================================
// Update
//====================================================================
void CAIGroup::Update()
{
FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );
	CAISystem* pAISystem(GetAISystem());

	if(m_lastUpdateTime.GetSeconds() < 0.0f)
		m_lastUpdateTime = pAISystem->GetFrameStartTime();

	float	dt = (pAISystem->GetFrameStartTime() - m_lastUpdateTime).GetSeconds();

	UpdateEnemyStats();

	for(TacticMap::iterator it = m_tactics.begin(); it != m_tactics.end(); ++it)
		it->second->Update(dt);

	m_lastUpdateTime = pAISystem->GetFrameStartTime();

	// update readability sounds
	TSoundReadabilityMap::iterator it = m_ReadabilitySounds.begin();
	while ( it != m_ReadabilitySounds.end())
	{
		it->second -= dt;
		if(it->second <=0)
			m_ReadabilitySounds.erase(it++);
		else
			++it;
	}

	UpdateReinforcementLogic(dt);
}

//====================================================================
// UpdateReinforcementLogic
//====================================================================
void CAIGroup::UpdateReinforcementLogic(float dt)
{
FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );

	if (m_reinforcementState == REINF_ALERTED_COMBAT_PENDING ||
			m_reinforcementState == REINF_DONE_PENDING )
		return;

	// Get reinforcement points for this group if not acquired yet.
	if (!m_reinforcementSpotsAcquired)
	{
		m_reinforcementSpots.clear();
		
		m_reinforcementSpotsAllAlerted = 0;
		m_reinforcementSpotsCombat = 0;
		m_reinforcementSpotsBodyCount = 0;

		for (AutoAIObjectIter it(GetAISystem()->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIANCHOR_REINFORCEMENT_SPOT)); it->GetObject(); it->Next())
		{
			CAIObject* pAI = static_cast<CAIObject*>(it->GetObject());
			if (pAI->GetGroupId() == m_groupID && pAI->GetEntity())
			{
				// Look up the extra properties.
				IScriptTable* pTable = pAI->GetEntity()->GetScriptTable();
				if (pTable)
				{
					SmartScriptTable props;
					if (pTable->GetValue("Properties", props))
					{
						m_reinforcementSpots.resize(m_reinforcementSpots.size() + 1);

						int whenAllAlerted = 0;
						int whenInCombat = 0;
						int groupBodyCount = 0;
						float avoidWhenTargetInRadius = 0.0f;
						int type = 0;
						props->GetValue("bWhenAllAlerted", whenAllAlerted);
						props->GetValue("bWhenInCombat", whenInCombat);
						props->GetValue("iGroupBodyCount", groupBodyCount);
						props->GetValue("eiReinforcementType", type);
						props->GetValue("AvoidWhenTargetInRadius", avoidWhenTargetInRadius);

						m_reinforcementSpots.back().whenAllAlerted = whenAllAlerted > 0;
						m_reinforcementSpots.back().whenInCombat = whenInCombat > 0;
						m_reinforcementSpots.back().groupBodyCount = groupBodyCount;
						m_reinforcementSpots.back().avoidWhenTargetInRadius = avoidWhenTargetInRadius;
						m_reinforcementSpots.back().type = type;
						m_reinforcementSpots.back().pAI = pAI;

						if (m_reinforcementSpots.back().whenAllAlerted)
							m_reinforcementSpotsAllAlerted++;
						if (m_reinforcementSpots.back().whenInCombat)
							m_reinforcementSpotsCombat++;
						if (m_reinforcementSpots.back().groupBodyCount > 0)
							m_reinforcementSpotsBodyCount++;

					}
				}
			}
		}

		m_reinforcementSpotsAcquired = true;
	}

	// Check if reinforcements should be called.
	if (!m_reinforcementSpots.empty() && m_reinforcementState != REINF_DONE)
	{
		int totalCount = 0;
		int enabledCount = 0;
		int alertedCount = 0;
		int combatCount = 0;
		int liveTargetCount = 0;
		for (TUnitList::const_iterator it = m_Units.begin(), end = m_Units.end(); it != end; ++it)
		{
			CPuppet* pPuppet = it->m_pUnit->CastToCPuppet();
			if (!pPuppet) continue;
			totalCount++;
			if (!pPuppet->IsEnabled()) continue;
			if (pPuppet->GetProxy()->IsCurrentBehaviorExclusive()) continue;

			if (pPuppet->GetAlertness() >= 1)
				alertedCount++;
			if (pPuppet->GetAlertness() >= 2)
				combatCount++;
			if (pPuppet->GetAttentionTargetType() == AITARGET_VISUAL &&
					pPuppet->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
				liveTargetCount++;
			enabledCount++;
		}

		bool matchWhenAllAlerted = false;
		bool matchWhenInCombat = false;
		bool matchWhenGroupSizeLess = false;

		bool tryToCall = false;

		if (m_reinforcementState == REINF_NONE)
		{
			if (m_reinforcementSpotsAllAlerted)
			{
				if ((enabledCount <= 2 && alertedCount > 0) ||
						(enabledCount > 2 && alertedCount > 1))
				{
					matchWhenAllAlerted = true;
					matchWhenInCombat = false;
					matchWhenGroupSizeLess = false;
					tryToCall = true;
				}
			}

			if (m_reinforcementSpotsCombat)
			{
				if ((enabledCount <= 2 && liveTargetCount > 0 && combatCount > 0) ||
						(enabledCount > 2 && liveTargetCount > 1 && combatCount > 1))
				{
					matchWhenAllAlerted = false;
					matchWhenInCombat = true;
					matchWhenGroupSizeLess = false;
					tryToCall = true;
				}
			}
		}

		if (m_reinforcementState == REINF_ALERTED_COMBAT)
		{
			if (m_reinforcementSpotsBodyCount && m_countDead > 0)
			{
				for (unsigned i = 0, n = m_reinforcementSpots.size(); i < n; ++i)
				{
					if (m_countDead >= m_reinforcementSpots[i].groupBodyCount)
					{
						matchWhenAllAlerted = false;
						matchWhenInCombat = false;
						matchWhenGroupSizeLess = true;
						tryToCall = true;
						break;
					}
				}
			}
		}

		if (tryToCall)
		{
			float nearestDist = FLT_MAX;
			SAIReinforcementSpot* nearestSpot = 0;
			CUnitImg* pNearestCallerImg = 0;	

			Vec3 playerPos(0,0,0), playerViewDir(0,0,0);
			CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetAISystem()->GetPlayer());
			if (pPlayer)
			{
				playerPos = pPlayer->GetPos();
				playerViewDir = pPlayer->GetViewDir();
			}

			for (unsigned i = 0, n = m_reinforcementSpots.size(); i < n; ++i)
			{
				if (!m_reinforcementSpots[i].pAI->IsEnabled()) continue;

				if (matchWhenAllAlerted && !m_reinforcementSpots[i].whenAllAlerted) continue;
				if (matchWhenInCombat && !m_reinforcementSpots[i].whenInCombat) continue;
				if (matchWhenGroupSizeLess && m_countDead >= m_reinforcementSpots[i].groupBodyCount) continue;

				const Vec3& reinfSpot = m_reinforcementSpots[i].pAI->GetPos();
				const float reinfRadiusSqr =  sqr(m_reinforcementSpots[i].pAI->GetRadius());
				const float avoidWhenTargetInRadiusSqr = sqr(m_reinforcementSpots[i].avoidWhenTargetInRadius);

				for (TUnitList::iterator it = m_Units.begin(), end = m_Units.end(); it != end; ++it)
				{
					CPuppet* pPuppet = it->m_pUnit->CastToCPuppet();
					if (!pPuppet) continue;
					if (!pPuppet->IsEnabled()) continue;
					if ((GetAISystem()->GetFrameStartTime()-it->m_lastReinforcementTime).GetSeconds()<3.f) continue;
					if (pPuppet->GetProxy()->IsCurrentBehaviorExclusive()) continue;
					if (matchWhenAllAlerted && pPuppet->GetAlertness() < 1) continue;
					if (matchWhenInCombat && pPuppet->GetAlertness() < 2) continue;

					// Skip reinforcement spots that are too close the attention target.
					if ((pPuppet->GetAttentionTargetType() == AITARGET_VISUAL || pPuppet->GetAttentionTargetType() == AITARGET_MEMORY) &&
						pPuppet->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE &&
						Distance::Point_PointSq(reinfSpot, pPuppet->GetAttentionTarget()->GetPos()) < avoidWhenTargetInRadiusSqr)
						continue;

					float distSqr = Distance::Point_PointSq(pPuppet->GetPos(), reinfSpot);

					if (distSqr < reinfRadiusSqr)
					{
						float dist = sqrtf(distSqr);

						if (!pPuppet->GetAttentionTarget())
						{
							dist += 100.0f;
						}
						else
						{
							const float preferredCallDist = 25.0f;
							float bestDist = fabsf(Distance::Point_Point(pPuppet->GetPos(), pPuppet->GetAttentionTarget()->GetPos()) - preferredCallDist);

							if (pPuppet->GetAttentionTargetType() == AITARGET_VISUAL &&
									pPuppet->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
								bestDist *= 0.5f;
							else if (pPuppet->GetAttentionTargetThreat() == AITHREAT_THREATENING)
								bestDist *= 0.75f;

							dist += bestDist;
						}

						// Prefer puppet visible to the player.
						Vec3 dirPlayerToPuppet = pPuppet->GetPos() - playerPos;
						dirPlayerToPuppet.NormalizeSafe();
						float dot = playerViewDir.Dot(dirPlayerToPuppet);
						dist += (1 - sqr((dot + 1)/2)) * 25.0f;

						if (dist < nearestDist)
						{
							nearestDist = dist;
							nearestSpot = &m_reinforcementSpots[i];
//							nearestCaller = pPuppet;
							pNearestCallerImg = &(*it);
						}
					}
				}
			}

			if (pNearestCallerImg && nearestSpot)
			{
				// Change state
				if (m_reinforcementState == REINF_NONE)
					m_reinforcementState = REINF_ALERTED_COMBAT_PENDING;
				else if (m_reinforcementState == REINF_ALERTED_COMBAT)
					m_reinforcementState = REINF_DONE_PENDING;

				// Tell the agent to call reinforcements.
				AISignalExtraData* pData = new AISignalExtraData;
				pData->nID = nearestSpot->pAI->GetEntityID();
				pData->iValue = nearestSpot->type;
				pNearestCallerImg->m_pUnit->SetSignal(1, "OnCallReinforcements", pNearestCallerImg->m_pUnit->GetEntity(), pData);
				pNearestCallerImg->m_lastReinforcementTime = GetAISystem()->GetFrameStartTime();

				m_DEBUG_reinforcementCalls.push_back(SAIRefinforcementCallDebug(pNearestCallerImg->m_pUnit->GetPos()+Vec3(0,0,0.3f), nearestSpot->pAI->GetPos()+Vec3(0,0,0.3f), 7.0f, nearestSpot->pAI->GetName()));

				m_pReinfPendingUnit = pNearestCallerImg->m_pUnit->CastToCPuppet();// pNearestCallerImg->m_pUnit;
				// Remove the spot (when confirmed)
				m_pendingDecSpotsAllAlerted = nearestSpot->whenAllAlerted;
				m_pendingDecSpotsCombat = nearestSpot->whenInCombat;
				m_pendingDecSpotsBodyCount = nearestSpot->groupBodyCount > 0;
			}
		}
	}

	for (unsigned i = 0; i < m_DEBUG_reinforcementCalls.size(); )
	{
		m_DEBUG_reinforcementCalls[i].t -= dt;
		if (m_DEBUG_reinforcementCalls[i].t < 0.0f)
		{
			m_DEBUG_reinforcementCalls[i] = m_DEBUG_reinforcementCalls.back();
			m_DEBUG_reinforcementCalls.pop_back();
		}
		else
			++i;
	}
}

//====================================================================
// FindOrCreateGroupTactic
//====================================================================
IAIGroupTactic*	CAIGroup::FindOrCreateGroupTactic(int eval)
{
	TacticMap::iterator ittac = m_tactics.find(eval);
	if(ittac != m_tactics.end())
		return ittac->second;

	IAIGroupTactic*	tactic = 0;

	switch(eval)
	{
	case 0: tactic = new CHumanGroupTactic(this); break;
	case 1: tactic = new CFollowerGroupTactic(this); break;
	case 2: tactic = new CBossGroupTactic(this); break;
	}

	if(tactic)
	{
		// Add current group members to the tactic
		for (TUnitList::const_iterator it = m_Units.begin(), end = m_Units.end(); it != end; ++it)
			tactic->OnMemberAdded(it->m_pUnit);
		m_tactics.insert(TacticMap::iterator::value_type(eval, tactic));
	}

	return tactic;
}


//====================================================================
// NotifyGroupTacticState
//====================================================================
void CAIGroup::NotifyGroupTacticState(IAIObject* pRequester, int tac, int type, float param)
{
	IAIGroupTactic*	pTactic = FindOrCreateGroupTactic(tac);
	if(!pTactic)
		return;
	pTactic->NotifyState(pRequester, type, param);
}

//====================================================================
// GetGroupTacticState
//====================================================================
int CAIGroup::GetGroupTacticState(IAIObject* pRequester, int tac, int type, float param)
{
	IAIGroupTactic*	pTactic = FindOrCreateGroupTactic(tac);
	if(!pTactic)
		return 0;
	return pTactic->GetState(pRequester, type, param);
}

//====================================================================
// GetGroupTacticPoint
//====================================================================
Vec3 CAIGroup::GetGroupTacticPoint(IAIObject* pRequester, int eval, int type, float param)
{
	IAIGroupTactic*	pTactic = FindOrCreateGroupTactic(eval);
	if(!pTactic)
		return ZERO;
	return pTactic->GetPoint(pRequester, type, param);
}

//====================================================================
// UpdateGroupCountStatus
//====================================================================
void CAIGroup::UpdateGroupCountStatus()
{
	CAISystem* pAISystem(GetAISystem());
	// Count number of active and non active members.
	int count = 0;
	int	countEnabled = 0;
	for (AIObjects::iterator it = pAISystem->m_mapGroups.find(m_groupID); it != pAISystem->m_mapGroups.end() && it->first == m_groupID; ++it)
	{
		CAIObject* pObject = it->second;
		int type = pObject->GetType();
		if (type == AIOBJECT_PUPPET || type == AIOBJECT_VEHICLE)
		{
			if (pObject->IsEnabled())
				countEnabled++;
			count++;
		}
		else if (type == AIOBJECT_VEHICLE)
		{
			CAIVehicle* pVehicle = pObject->CastToCAIVehicle();
			if (pVehicle->IsEnabled() && pVehicle->IsDriverInside())
				countEnabled++;
			count++;
		}
	}

	// Store the current state
	m_count = count;
	m_countEnabled = countEnabled;
	// Keep track on the maximum 
	m_countMax = max(m_countMax, count);
	m_countEnabledMax = max(m_countEnabledMax, countEnabled);

	// add smart objects state "GroupHalved" to alive group members
	if ( m_countEnabled == m_countEnabledMax / 2 )
	{
		AIObjects::iterator it, itEnd = pAISystem->m_mapGroups.end();
		for ( it = pAISystem->m_mapGroups.lower_bound( m_groupID ); it != itEnd && it->first == m_groupID; ++it )
		{
			CAIObject* pObject = it->second;
			if ( pObject->IsEnabled() && pObject->GetType() != AIOBJECT_LEADER )
			{
				if ( IEntity* pEntity = pObject->GetEntity() )
					pAISystem->AddSmartObjectState( pEntity, "GroupHalved" );
			}
		}
	}
}

//====================================================================
// RequestReadabilitySound
//====================================================================
bool CAIGroup::RequestReadabilitySound(int id, float duration)
{
	if(m_ReadabilitySounds.find(id) != m_ReadabilitySounds.end())
		return false;
	m_ReadabilitySounds.insert(std::make_pair(id, duration));
	return true;
}

//====================================================================
// GetGroupCount
//====================================================================
int CAIGroup::GetGroupCount(int flags)
{
	if (flags == 0 || flags == IAISystem::GROUP_ALL)
		return m_count;
	else if (flags == IAISystem::GROUP_ENABLED)
		return m_countEnabled;
	else if (flags == IAISystem::GROUP_MAX)
		return m_countMax;
	else if (flags == (IAISystem::GROUP_ENABLED|IAISystem::GROUP_MAX))
		return m_countEnabledMax;
	return 0;
}

//====================================================================
// Reset
//====================================================================
void CAIGroup::Reset()
{
	m_lastUpdateTime.SetSeconds(-1.0f);
	m_count = 0;
	m_countMax = 0;
	m_countEnabled = 0;
	m_countEnabledMax = 0;
	m_countDead = 0;
	m_countCheckedDead = 0;
	m_bUpdateEnemyStats = true;

	for(TUnitList::iterator itrUnit(m_Units.begin()); itrUnit!=m_Units.end(); ++itrUnit )
		itrUnit->Reset();

	for(TacticMap::iterator it = m_tactics.begin(); it != m_tactics.end(); ++it)
		it->second->Reset();

	m_ReadabilitySounds.clear();

	m_reinforcementSpots.clear();
	m_reinforcementSpotsAcquired = false;
	m_reinforcementState = REINF_NONE;
	m_pReinfPendingUnit = NULL;

	m_DEBUG_reinforcementCalls.clear();
}

//====================================================================
// Serialize
//====================================================================
void CAIGroup::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
	// TODO: serialize and create tactical evals
	char Name[256];
	sprintf(Name, "AIGroup-%d", m_groupID);

	ser.BeginGroup(Name);

	ser.Value("AIgroup_GroupID", m_groupID);

	m_bUpdateEnemyStats = true;

	ser.Value("m_lastUpdateTime", m_lastUpdateTime);

	ser.Value("m_count", m_count);
	ser.Value("m_countMax", m_countMax);
	ser.Value("m_countEnabled", m_countEnabled);
	ser.Value("m_countEnabledMax", m_countEnabledMax);
	ser.Value("m_countDead", m_countDead);
	ser.Value("m_countCheckedDead", m_countCheckedDead);

	//	TSetAIObjects	m_Targets;
	ser.Value("m_vEnemyPositionKnown", m_vEnemyPositionKnown);
	ser.Value("m_vAverageEnemyPosition", m_vAverageEnemyPosition);
	ser.Value("m_vAverageLiveEnemyPosition", m_vAverageLiveEnemyPosition);

	//see below for m_pLeader Serialization
	//objectTracker.SerializeObjectPointer(ser,"m_pLeader",m_pLeader,false);

	objectTracker.SerializeObjectContainer(ser, "m_Units", m_Units);

	// reset ranks - m_RankedUnits is not getting serialized 
	if(ser.IsReading())
	{
		m_RankedUnits.clear();
		for(TUnitList::iterator itrUnit(m_Units.begin()); itrUnit!=m_Units.end(); ++itrUnit )
			SetUnitRank(itrUnit->m_pUnit);
		// Clear reinforcement spots.
		m_reinforcementSpotsAcquired = false;
		m_reinforcementSpots.clear();
	}
	objectTracker.SerializeValueValueContainer(ser, "m_ReadabilitySounds", m_ReadabilitySounds);
	objectTracker.SerializeObjectPointerContainer(ser, "m_Targets", m_Targets,false);
	ser.EnumValue("m_reinforcementState", m_reinforcementState, REINF_NONE, LAST_REINF);

	ser.Value("m_pendingDecSpotsAllAlerted", m_pendingDecSpotsAllAlerted);
	ser.Value("m_pendingDecSpotsCombat", m_pendingDecSpotsCombat);
	ser.Value("m_pendingDecSpotsBodyCount", m_pendingDecSpotsBodyCount);
	objectTracker.SerializeObjectPointer(ser, "ReingPendingUnit", m_pReinfPendingUnit, false);

	// Serialize group tactics.
	ser.BeginGroup("AIGroupTactics");
	int numTactics = m_tactics.size();
	ser.Value("numTactics", numTactics);

	if(ser.IsWriting())
	{
		int i = 0;
		for(TacticMap::iterator it = m_tactics.begin(); it != m_tactics.end(); ++it)
		{
			ser.BeginGroup("Tactic");
			int	type = it->first;
			ser.Value("type", type);
			it->second->Serialize(ser, objectTracker);
			ser.EndGroup();
			++i;
		}
	}
	else
	{
		for(int i = 0; i < numTactics; ++i)
		{
			ser.BeginGroup("Tactic");
			int	type = 0;
			ser.Value("type", type);
			IAIGroupTactic*	tactic = FindOrCreateGroupTactic(type);
			if(tactic)
				tactic->Serialize(ser, objectTracker);
			ser.EndGroup();
		}
		/*if(ser.IsReading())
		{
			m_pLeader = GetAISystem()->GetLeader(m_groupID);
			if(m_pLeader)
				m_pLeader->SetAIGroup(this);
		}*/
	}

	ser.EndGroup();	// AIGroupTactics

	ser.EndGroup();	// AIGroup
}

void CAIGroup::OnUnitAttentionTargetChanged()
{
	m_bUpdateEnemyStats = true;
	for(TacticMap::iterator it = m_tactics.begin(); it != m_tactics.end(); ++it)
		it->second->OnUnitAttentionTargetChanged();
}

//====================================================================
// DebugDraw
//====================================================================
void CAIGroup::DebugDraw(IRenderer* pRend)
{
	for(TacticMap::iterator it = m_tactics.begin(); it != m_tactics.end(); ++it)
		it->second->DebugDraw(pRend);

	// Debug draw reinforcement logic.
	if (GetAISystem()->m_cvDebugDrawReinforcements->GetIVal() == m_groupID)
	{

		int totalCount = 0;
		int enabledCount = 0;
		int alertedCount = 0;
		int combatCount = 0;
		int liveTargetCount = 0;
		for (TUnitList::const_iterator it = m_Units.begin(), end = m_Units.end(); it != end; ++it)
		{
			CPuppet* pPuppet = it->m_pUnit->CastToCPuppet();
			if (!pPuppet) continue;
			totalCount++;
			if (!pPuppet->IsEnabled()) continue;
			if (pPuppet->GetProxy()->IsCurrentBehaviorExclusive()) continue;

			if (pPuppet->GetAlertness() >= 1)
				alertedCount++;
			if (pPuppet->GetAlertness() >= 2)
				combatCount++;
			if (pPuppet->GetAttentionTargetType() == AITARGET_VISUAL &&
					pPuppet->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
				liveTargetCount++;
			enabledCount++;
		}

		for (unsigned i = 0, n = m_reinforcementSpots.size(); i < n; ++i)
		{
			if (m_reinforcementSpots[i].pAI->IsEnabled())
			{
				string text;
				char msg[64];

				if (m_reinforcementState == REINF_NONE)
				{
					if (m_reinforcementSpots[i].whenAllAlerted)
					{
						_snprintf(msg, 64, "ALERTED: Alerted >= Enabled | %d >= %d", alertedCount, enabledCount);
						text += msg;
						if (alertedCount >= enabledCount)
							text += " Active!\n";
						else
							text += "\n";
					}

					if (m_reinforcementSpots[i].whenInCombat)
					{
						if (enabledCount > 1)
							_snprintf(msg, 64, "COMBAT: InCombat > 0 && LiveTarget > 1 | %d > 0 && %d > 1", combatCount, liveTargetCount);
						else
							_snprintf(msg, 64, "COMBAT: InCombat > 0 && LiveTarget > 0 | %d > 0 && %d > 0", combatCount, liveTargetCount);
						text += msg;

						if (combatCount > 0 && ((enabledCount > 1 && liveTargetCount > 1) || (enabledCount == 1 && liveTargetCount > 0)))
							text += " Active!\n";
						else
							text += "\n";
					}
				}

				if (m_reinforcementState == REINF_NONE || m_reinforcementState == REINF_ALERTED_COMBAT)
				{
					if (m_reinforcementSpots[i].groupBodyCount > 0)
					{
						_snprintf(msg, 64, "SIZE: Bodies < Count | %d < %d",
							m_reinforcementSpots[i].groupBodyCount, m_countDead);
						text += msg;

						if (m_countDead >= m_reinforcementSpots[i].groupBodyCount)
							text += " >>>\n";
						else
							text += "\n";
					}
				}
				pRend->DrawLabel(m_reinforcementSpots[i].pAI->GetPos()+Vec3(0,0,1), 1.5f, text.c_str());
			}
			else
			{
				pRend->DrawLabel(m_reinforcementSpots[i].pAI->GetPos()+Vec3(0,0,1), 1.0f, "Disabled");
			}

			ColorB	col, colTrans;
			int c = m_reinforcementSpots[i].type % 3;
			if (c == 0)
				col.Set(255,64,64,255);
			else if (c == 1)
				col.Set(64,255,64,255);
			else if (c == 2)
				col.Set(64,64,255,255);
			colTrans = col;
			colTrans.a = 64;

			if (!m_reinforcementSpots[i].pAI->IsEnabled())
			{
				col.a /= 4;
				colTrans.a /= 4;
			}

			pRend->GetIRenderAuxGeom()->DrawCylinder(m_reinforcementSpots[i].pAI->GetPos()+Vec3(0,0,0.5f), Vec3(0,0,1), 0.2f, 1.0f, col);
			DebugDrawRangeCircle(pRend, m_reinforcementSpots[i].pAI->GetPos(), m_reinforcementSpots[i].pAI->GetRadius(), 1.0f, colTrans, col, true);

			if (m_reinforcementSpots[i].pAI->IsEnabled())
			{
				for (TUnitList::const_iterator it = m_Units.begin(), end = m_Units.end(); it != end; ++it)
				{
					CPuppet* pPuppet = it->m_pUnit->CastToCPuppet();
					if (!pPuppet) continue;
					totalCount++;
					if (!pPuppet->IsEnabled()) continue;
					if (pPuppet->GetProxy()->IsCurrentBehaviorExclusive()) continue;

					if (Distance::Point_PointSq(pPuppet->GetPos(), m_reinforcementSpots[i].pAI->GetPos()) < sqr(m_reinforcementSpots[i].pAI->GetRadius()))
						pRend->GetIRenderAuxGeom()->DrawLine(pPuppet->GetPos(), col+Vec3(0,0,0.5f), m_reinforcementSpots[i].pAI->GetPos()+Vec3(0,0,0.5f), col);
				}
			}
		}

		for (unsigned i = 0, n = m_DEBUG_reinforcementCalls.size(); i < n; ++i)
		{
			const Vec3& pos = m_DEBUG_reinforcementCalls[i].from;
			Vec3 dir = m_DEBUG_reinforcementCalls[i].to - m_DEBUG_reinforcementCalls[i].from;
			DebugDrawArrow(pRend, pos, dir, 0.5f, ColorB(255,255,255));
			
			float len = dir.NormalizeSafe();
			dir *= min(2.0f, len);

			pRend->DrawLabel(pos+dir, 1.0f, m_DEBUG_reinforcementCalls[i].performer.c_str());
		}

	}
}

void CAIGroup::Validate()
{
	for(TacticMap::iterator it = m_tactics.begin(); it != m_tactics.end(); ++it)
		((CHumanGroupTactic*)(it->second))->Validate();
}

void CAIGroup::Dump()
{
	AILogAlways(">>----------------------------");
	for(TacticMap::iterator it = m_tactics.begin(); it != m_tactics.end(); ++it)
		((CHumanGroupTactic*)(it->second))->Dump();
}

//====================================================================
// NotifyGroupTacticState
//====================================================================
void CAIGroup::NotifyReinfDone(const IAIObject* obj, bool isDone)
{
	if(!m_pReinfPendingUnit)
		return;
	if(m_pReinfPendingUnit && m_pReinfPendingUnit!=obj)
	{
		AIWarning("CAIGroup::NotifyReinfDone - pending unit missMatch (internal %s ; passed %s)", m_pReinfPendingUnit->GetName(), obj->GetName());
		AIAssert(0);
		return;
	}
	if (m_reinforcementState != REINF_ALERTED_COMBAT_PENDING &&
			m_reinforcementState != REINF_DONE_PENDING )
	{
		AIWarning("CAIGroup::NotifyReinfDone - not pending state (Puppet %s ; state %d)", obj->GetName(), m_reinforcementState);
		AIAssert(0);
		return;
	}
	if(isDone)
	{
		// Change state
		if (m_reinforcementState == REINF_ALERTED_COMBAT_PENDING)
			m_reinforcementState = REINF_ALERTED_COMBAT;
		else if (m_reinforcementState == REINF_DONE_PENDING)
			m_reinforcementState = REINF_DONE;

		// Remove the spot
		if (m_pendingDecSpotsAllAlerted)
			m_reinforcementSpotsAllAlerted--;
		if (m_pendingDecSpotsCombat)
			m_reinforcementSpotsCombat--;
		if (m_pendingDecSpotsBodyCount)
			m_reinforcementSpotsBodyCount--;
	}
	else
	{
		// Change state
		if (m_reinforcementState == REINF_ALERTED_COMBAT_PENDING)
			m_reinforcementState = REINF_NONE;
		else if (m_reinforcementState == REINF_DONE_PENDING)
			m_reinforcementState = REINF_ALERTED_COMBAT;
	}
	m_pReinfPendingUnit = NULL;
}

