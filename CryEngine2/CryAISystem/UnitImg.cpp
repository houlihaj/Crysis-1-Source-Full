/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   UnitAction.cpp
$Id$
Description: Refactoring CLeadeActions; adding Serialization support

-------------------------------------------------------------------------
History:
- 22/05/2006 : Moved from UnitAction by Mikko

*********************************************************************/

#include "StdAfx.h"
#include "CAISystem.h"
#include "UnitImg.h"
#include "UnitAction.h"
#include "AILog.h"
#include "Leader.h"

CUnitImg::CUnitImg() :
	m_pUnit(0),
	m_TagPoint(0,0,0),
	m_Group(0),
	m_FormationPointIndex(0),
	m_SoldierClass(UNIT_CLASS_INFANTRY),
	m_properties(UPR_ALL),
	m_pCurrentAction(0),
	m_bTaskSuspended(false),
	m_flags(0),
	m_fHeight(1.6f),
	m_fWidth(0.6f),
	m_fDistance(0.f),
	m_fDistance2(0.f),
	m_lastReinforcementTime(0.f)
{
	// Empty
}

//
//----------------------------------------------------------------------------------------------------
CUnitImg::CUnitImg(CAIActor* pUnit) :
	m_pUnit(pUnit),
	m_TagPoint(0,0,0),
	m_Group(0),
	m_FormationPointIndex(0),
	m_SoldierClass(UNIT_CLASS_INFANTRY),
	m_properties(UPR_ALL),
	m_pCurrentAction(0),
	m_bTaskSuspended(false),
	m_flags(0),
	m_fHeight(1.6f),
	m_fWidth(0.6f),
	m_fDistance(0.f),
	m_fDistance2(0.f),
	m_lastReinforcementTime(0.f)
{
	m_lastMoveTime = GetAISystem()->GetFrameStartTime();
}

//
//----------------------------------------------------------------------------------------------------
CUnitImg::~CUnitImg()
{
	ClearPlanning();
}

//
//----------------------------------------------------------------------------------------------------
void	CUnitImg::TaskExecuted( )
{

	if(m_pCurrentAction)
	{
		delete m_pCurrentAction;
		m_pCurrentAction = NULL;

		if(!m_Plan.empty())//this test should be useless
			m_Plan.pop_front();
	}
	//	else
	//		AIAssert(0);
}


//
//----------------------------------------------------------------------------------------------------
void	CUnitImg::ExecuteTask( )
{
	bool bContinue = true;

	while(bContinue && !m_Plan.empty())
	{
		CUnitAction* nextAction = m_Plan.front();

		bool bIsBlocked = nextAction->IsBlocked();
		bContinue = !nextAction->m_BlockingPlan && !bIsBlocked;

		if(!bIsBlocked)
		{
			m_pCurrentAction = nextAction;
			IAISignalExtraData* pData = NULL;
			switch(m_pCurrentAction->m_Action)
			{
				/*				case UA_HOLD:
				ClearFollowing();
				GetAISystem()->SendSignal(0,1,"ORDER_HOLD",m_pUnit, &(AISIGNAL_EXTRA_DATA(m_pCurrentAction->m_Point)));
				break;
				*/				
			case UA_MOVE:
				pData = GetAISystem()->CreateSignalExtraData();
				pData->point = m_pCurrentAction->m_Point;
				pData->point2 = m_pCurrentAction->m_Direction;
				pData->fValue = m_pCurrentAction->m_fDistance;
				GetAISystem()->SendSignal(0,1,"ORDER_MOVE",m_pUnit, pData);
				break;
			case UA_APPROACH:
				GetAISystem()->SendSignal(0,1,"ORDER_APPROACH",m_pUnit, new AISignalExtraData(m_pCurrentAction->m_SignalData));
				break;
			case UA_MOVE_BACK:
				if(m_pCurrentAction->m_Point.IsZero())
				{
					CAIObject* pFormation = GetAISystem()->GetLeader(m_pUnit->GetGroupId())->GetNewFormationPoint(m_pUnit);
					if(pFormation)
					{
						pData = GetAISystem()->CreateSignalExtraData();
						pData->point = pFormation->GetPos();
						GetAISystem()->SendSignal(0,1,"ORDER_MOVE",m_pUnit, pData);
					}
					// else it won't move: no formation, and no point provided
				}
				else
				{
					pData = GetAISystem()->CreateSignalExtraData();
					pData->point = m_pCurrentAction->m_Point;
					GetAISystem()->SendSignal(0,1,"ORDER_MOVE",m_pUnit, pData);
				}
				break;
			case UA_MOVE_BACK_SPECIAL:
				if(m_pCurrentAction->m_Point.IsZero())
				{
					CAIObject* pFormation = GetAISystem()->GetLeader(m_pUnit->GetGroupId())->GetNewFormationPoint(m_pUnit,SPECIAL_FORMATION_POINT);
					if(pFormation)
					{
						pData = GetAISystem()->CreateSignalExtraData();
						pData->point = pFormation->GetPos();
						pData->iValue = 1;
						pData->SetObjectName("formation_special");
						GetAISystem()->SendSignal(0,1,"ORDER_MOVE",m_pUnit,pData);
					}
					// else it won't move: no formation, and no point provided
				}
				else
				{
					pData = GetAISystem()->CreateSignalExtraData();
					pData->point = m_pCurrentAction->m_Point;
					GetAISystem()->SendSignal(0,1,"ORDER_MOVE",m_pUnit,pData);
				}
				break;
			case UA_SIGNAL:
				GetAISystem()->SendSignal(0,1,m_pCurrentAction->m_SignalText,m_pUnit,new AISignalExtraData(m_pCurrentAction->m_SignalData));
				break;
			case UA_SIGNAL_LEADER:
				//					GetAISystem()->SendSignal(SIGNALFILTER_LEADER,10,AIORD_REPORTDONE,m_pUnit);
				GetAISystem()->SendSignal(SIGNALFILTER_LEADER,1,m_pCurrentAction->m_SignalText,m_pUnit,new AISignalExtraData(m_pCurrentAction->m_SignalData));
				break;
			case UA_ACQUIRETARGET:
				GetAISystem()->SendSignal(0,1,"ORDER_ACQUIRE_TARGET",m_pUnit,new AISignalExtraData(m_pCurrentAction->m_SignalData));
				break;
			case UA_FORM:
				SetFollowing();
				GetAISystem()->SendSignal(0,1,"ORDER_FORM",m_pUnit);
				break;
			case UA_FORM_SHOOT:
				SetFollowing();
				GetAISystem()->SendSignal(0,1,"ORDER_FORM_SHOOT",m_pUnit);
				break;
			case UA_FOLLOW:
				SetFollowing();
				GetAISystem()->SendSignal(0,1,"ORDER_FOLLOW",m_pUnit);
				break;
			case UA_FOLLOWALL:
				GetAISystem()->SendSignal(SIGNALFILTER_LEADER,1,AIORD_FOLLOW,m_pUnit);
				break;
			case UA_FIRE:
				GetAISystem()->SendSignal(0,10,"ORDER_FIRE",m_pUnit);
				bContinue = false; // force blocking
				break;
			case UA_FLANK:
				pData = GetAISystem()->CreateSignalExtraData();
				pData->point = m_pCurrentAction->m_Point;
				GetAISystem()->SendSignal(0,10,"ORDER_FLANK",m_pUnit,pData);
				bContinue = false; // force blocking
				break;

			case UA_HIDEAROUND:
				GetAISystem()->SendSignal(0, 10, "ORDER_HIDE_AROUND", m_pUnit,new AISignalExtraData(m_pCurrentAction->m_SignalData));
				break;

			case UA_SEARCH:
			case UA_HIDEBEHIND:
			case UA_HIDEFOLLOW:
			case UA_HOLD:
				{
					const char* signal =
						m_pCurrentAction->m_Action == UA_HIDEBEHIND ? "ORDER_HIDE" :
						m_pCurrentAction->m_Action == UA_SEARCH ? "ORDER_SEARCH" :
						m_pCurrentAction->m_Action == UA_HIDEFOLLOW ? "ORDER_FOLLOW_HIDE" :
						"ORDER_HOLD";
					if (m_pCurrentAction->m_Point.IsZero())
					{
						Vec3 hidePosition(0,0,0);
						CLeader* pMyLeader = GetAISystem()->GetLeader(m_pUnit);
						if (pMyLeader && pMyLeader->m_pFormation)
						{
							CAIObject* pFormPoint = pMyLeader->GetNewFormationPoint(m_pUnit);
							if (pFormPoint)
								hidePosition = pFormPoint->GetPos();
						}
						if (hidePosition.IsZero())
							hidePosition = m_pUnit->GetPos();
						pData = GetAISystem()->CreateSignalExtraData();
						pData->point = hidePosition;
						pData->iValue = m_pCurrentAction->m_Tag;
						GetAISystem()->SendSignal(0, 10, signal, m_pUnit, pData);
					}
					else
					{
						pData = GetAISystem()->CreateSignalExtraData();
						pData->point = m_pCurrentAction->m_Point;
						pData->point2 = m_pCurrentAction->m_Direction;
						pData->iValue = m_pCurrentAction->m_Tag;
						GetAISystem()->SendSignal(0, 10, signal, m_pUnit, pData);
					}
					bContinue = false; // force blocking
				}
				break;
			case UA_TIMEOUT:
				pData = GetAISystem()->CreateSignalExtraData();
				pData->fValue = m_pCurrentAction->m_fDistance;
				GetAISystem()->SendSignal(0, 10, "ORDER_TIMEOUT", m_pUnit,pData);
				bContinue = false; // force blocking
				break;
			case UA_WHATSNEXT:
				//	GetAISystem()->GetLeader(m_pUnit->GetGroupId())->OnUpdateUnitTask(this);
				break;
			default:
				// unrecognized action type, skip it
				AIAssert(0);
				bContinue = true;
				break;
			}
		}

		if(bContinue )
			TaskExecuted();
	}
}

//
//----------------------------------------------------------------------------------------------------
bool	CUnitImg::IsBlocked() const
{
	if(m_bTaskSuspended)// consider the unit's plan blocked if he's suspended his task for 
		return true;				 // something more prioritary

	if(m_Plan.empty())
		return false;
	else
		return m_Plan.front()->IsBlocked();
}


//
//----------------------------------------------------------------------------------------------------
void CUnitImg::ClearPlanning(int priority)
{
	if(priority)
	{
		TActionList::iterator itAction = m_Plan.begin(); 
		while(itAction!=m_Plan.end())
		{
			if((*itAction)->m_Priority <= priority)
			{
				if (*itAction == m_pCurrentAction)
				{
					m_pCurrentAction = NULL;
					m_bTaskSuspended = false;
				}

				CUnitAction* pAction = *itAction;
				m_Plan.erase(itAction++);
				delete pAction;
			}
			else
				++itAction;
		}
	}
	else // unconditioned if priority==0
	{
		for(TActionList::iterator itAction = m_Plan.begin(); itAction!=m_Plan.end();++itAction)
			delete (*itAction);
		m_Plan.clear();
		m_pCurrentAction = NULL;
		m_bTaskSuspended = false;
	}


}

//
//----------------------------------------------------------------------------------------------------
void CUnitImg::ClearUnitAction(EUnitAction action)
{
	TActionList::iterator itAction = m_Plan.begin(); 
	while(itAction!=m_Plan.end())
	{
		if((*itAction)->m_Action == action)
		{
			if (*itAction == m_pCurrentAction)
			{
				m_pCurrentAction = NULL;
				m_bTaskSuspended = false;
			}

			CUnitAction* pAction = *itAction;
			m_Plan.erase(itAction++);
			delete pAction;
		}
		else
			++itAction;
	}
}


//
//----------------------------------------------------------------------------------------------------
void CUnitImg::SuspendTask()
{
	m_bTaskSuspended = true;
}

//
//----------------------------------------------------------------------------------------------------
void CUnitImg::ResumeTask()
{
	m_bTaskSuspended = false;
}

//
//----------------------------------------------------------------------------------------------------
template <typename T_Value> void SerializeListOfPointers(TSerialize ser, CObjectTracker& objectTracker, const char* pName, std::list<T_Value*> list)
{
	if(ser.IsReading())
		list.clear();
	if(ser.BeginOptionalGroup(pName, !list.empty()))
	{
		uint32 count(list.size());
		char groupName[32];
		ser.Value("Count", count);
		while(count--)
		{
			sprintf(groupName,"Unit_%d",count);
			ser.BeginGroup(groupName);
			{
				if(ser.IsReading())
				{
					T_Value *pCurUnit(new T_Value());
					objectTracker.SerializeObjectPointer(ser, "pointer", pCurUnit, true);
					pCurUnit->Serialize( ser, objectTracker );
					list.push_back( pCurUnit );
				}
				else
				{
					typename std::list<T_Value*>::iterator itrCurUnit(list.begin());
					std::advance( itrCurUnit, count );
					objectTracker.SerializeObjectPointer(ser, "pointer", (*itrCurUnit), true);
					(*itrCurUnit)->Serialize( ser, objectTracker );
				}
			}
			ser.EndGroup();
		}
		ser.EndGroup();
	}
}

bool CUnitImg::IsPlanFinished() const
{
	return m_Plan.empty() || !m_pUnit->IsEnabled();
}

//
//----------------------------------------------------------------------------------------------------
void	CUnitImg::Reset( )
{
	m_lastReinforcementTime.SetSeconds(0.f);
}

//
//----------------------------------------------------------------------------------------------------
void	CUnitImg::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	objectTracker.SerializeObjectPointer(ser, "m_pUnit", m_pUnit, false);
	SerializeListOfPointers(ser, objectTracker, "Plan", m_Plan);
	ser.Value("m_TagPoint", m_TagPoint);
	ser.Value("m_flags", m_flags);
	ser.Value("m_Group", m_Group);
	ser.Value("m_FormationPointIndex", m_FormationPointIndex);
	ser.Value("m_SoldierClass", m_SoldierClass);
	ser.Value("m_bTaskSuspended", m_bTaskSuspended);
	ser.Value("m_lastMoveTime",m_lastMoveTime);
	ser.Value("m_fDistance",m_fDistance);
	ser.Value("m_fDistance2",m_fDistance2);
	ser.Value("m_lastReinforcementTime",m_lastReinforcementTime);
	
	if(ser.BeginOptionalGroup("CurrentAction", m_pCurrentAction!=NULL))
	{
		if(ser.IsReading())
			m_pCurrentAction = m_Plan.empty() ? NULL : m_Plan.front();
		ser.EndGroup();
	}
}



//
//----------------------------------------------------------------------------------------------------
void CUnitAction::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	ser.EnumValue("m_Action", m_Action, UA_NONE, UA_LAST);
	ser.Value("m_BlockingPlan",	m_BlockingPlan);
	//	objectTracker.SerializeObjectPointer(ser, "pointer", this, true);
	objectTracker.SerializeObjectPointerContainer(ser, "BlockingActions", m_BlockingActions, false);
	objectTracker.SerializeObjectPointerContainer(ser, "BlockedActions", m_BlockedActions, false);
	//	SerializeListOfPointers(ser,"BlockingActions",m_BlockingActions);
	//	SerializeListOfPointers(ser,"BlockedActions",m_BlockedActions);
	ser.Value("m_Priority", m_Priority);
	ser.Value("m_Point", m_Point);
	ser.Value("m_SignalText", m_SignalText);
	m_SignalData.Serialize(ser);
	ser.Value("m_Tag", m_Tag);
}

//
//----------------------------------------------------------------------------------------------------
