/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   Leader.cpp

Description: Class Implementation for CLeader

-------------------------------------------------------------------------
History:
Created by Kirill Bulatsev
- 01:06:2005   : serialization support added

*********************************************************************/

#include "StdAfx.h"
#include "Leader.h"
#include "LeaderAction.h"
#include "CAISystem.h"
#include "ISystem.h"
#include "I3DEngine.h"
#include "Puppet.h"
#include "AIPlayer.h"
//#include "ITimer.h"
#include "IScriptSystem.h"
#include <IEntitySystem.h>
#include <IPhysics.h>
#include <set>
#include <limits>
#include <IConsole.h>
#include <ISerialize.h>
#include "ObjectTracker.h"
#include "VehicleFinder.h"
#include "TickCounter.h"




//
//----------------------------------------------------------------------------------------------------
CLeader::CLeader(CAISystem *pAISystem, int iGroupID)
	: CAIActor(pAISystem)
	, m_pCurrentAction(NULL)
	, m_bUseEnabled(false)
	, m_pFormationOwner(NULL)
	, m_Stance(STANCE_STAND)
	, m_pPrevFormationOwner(NULL)
	, m_FollowState(FS_NONE)
	, m_LastFollowState(FS_NONE)
	, m_bAllowFire(false)
	, m_vForcedPreferredPos(ZERO)
	, m_alertStatus(AIALERTSTATUS_SAFE)
	, m_bForceSafeArea(false)
	, m_bUpdateAlertStatus(false)
	, m_bKeepEnabled(false)
	, m_AlertStatusReadyRange(20)
	, m_pEnemyTarget(NULL)
	, m_vMarkedPosition(0,0,0)
{
	_fastcast_CLeader = true;

	m_groupId = iGroupID;
	m_pObstacleAllocator = new CObstacleAllocator(this);
	SetName("TeamLeader");
	SetAssociation(0);
	m_bLeaderAlive = true;
	m_SpotList.clear();
	m_pGroup = GetAISystem()->GetAIGroup(iGroupID);
}

//
//----------------------------------------------------------------------------------------------------
CLeader::~CLeader(void)
{

	AbortExecution();
	ReleaseFormation();
}

//
//----------------------------------------------------------------------------------------------------
void CLeader::SetAssociation(CAIObject* pAssociation)
{
	CAIObject::SetAssociation(pAssociation);
	if(pAssociation && pAssociation->IsEnabled())
		m_bLeaderAlive = true;
}

//
//----------------------------------------------------------------------------------------------------
void CLeader::Reset(EObjectResetType type)
{
	m_bAllowFire = false;

	CAIObject::Reset(type);
	AbortExecution();
	ReleaseFormation();
	m_szFormationDescriptor.clear();
	m_bUpdateAlertStatus = false;
	m_LastFollowState = m_FollowState = FS_NONE;
	m_alertStatus = AIALERTSTATUS_SAFE;
	m_currentAreaSpecies = GetParameters().m_nSpecies;
	m_pEnemyTarget = NULL;
	ClearAllAttackRequests();
	m_vForcedPreferredPos.Set(0,0,0);
	m_bLeaderAlive = true;
	m_SpotList.clear();
	m_State.vSignals.clear();
}


//
//----------------------------------------------------------------------------------------------------
bool CLeader::IsPlayer() const
{
	if(GetAssociation())
		return GetAssociation()->GetAIType() == AIOBJECT_PLAYER;
	else return false;
}

//
//----------------------------------------------------------------------------------------------------
Vec3 CLeader::GetPreferedPos() const
{
	if (!m_vForcedPreferredPos.IsZero())
		return m_vForcedPreferredPos;

	CAIObject* pLeaderObject = (CAIObject*)GetAssociation();
	if (!pLeaderObject)
		return m_pGroup->GetAveragePosition();
	else if (pLeaderObject->GetType() == AIOBJECT_PLAYER)
	{
		if (pLeaderObject && m_FollowState == FS_FOLLOW)
			return pLeaderObject->GetPos() - pLeaderObject->GetMoveDir() * 10.0f;
		else
			return m_pGroup->GetAveragePosition();
	}
	else
		return pLeaderObject->GetPos();
}

//
//----------------------------------------------------------------------------------------------------
void	CLeader::Update(EObjectUpdate type)
{
FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );
	if(!GetAssociation() && !m_bKeepEnabled)
		return;

	UpdateEnemyStats();

	if(m_bUpdateAlertStatus && IsPlayer())
		UpdateAlertStatus();

	UpdateAttackRequests();

	while (!m_State.vSignals.empty())
	{
		AISIGNAL sstruct = m_State.vSignals.back();
		m_State.vSignals.pop_back();
		ProcessSignal(sstruct);
	}
	
	bool bAllUnitsFinished = true;
	for(TUnitList::iterator unitItr = m_pGroup->GetUnits().begin(); unitItr != m_pGroup->GetUnits().end(); ++unitItr)
	{
		CUnitImg& curUnit = (*unitItr);
		if (!curUnit.IsPlanFinished())
		{
			bAllUnitsFinished = false;
			if(curUnit.Idle() && !curUnit.IsBlocked())
				curUnit.ExecuteTask();
		}
	}
	// fire control
	if(m_bAllowFire && (GetAISystem()->GetFrameStartTime() - m_fAllowingFireTime).GetSeconds() > 45.0f )
	{
		GetAISystem()->SendSignal(SIGNALFILTER_GROUPONLY, 0, "OnFireDisabled", this);
		m_bAllowFire = false;
	}		

	CLeaderAction::eActionUpdateResult eUpdateResult;
	if (m_pCurrentAction && (eUpdateResult= m_pCurrentAction->Update())!=CLeaderAction::ACTION_RUNNING)
/*			(eUpdateResult= m_pCurrentAction->Update())!=CLeaderAction::ACTION_RUNNING &&
			(bAllUnitsFinished || m_pCurrentAction->TimeOut()) && 
			!m_pCurrentAction->FinalUpdate())*/
	{
		// order completed or not issued
		bool search = m_pCurrentAction->GetType() == LA_ATTACK && !IsPlayer() && !m_pGroup->GetAttentionTarget(true,true);
		ELeaderAction iActionType = m_pCurrentAction->GetType();
		ELeaderActionSubType iActionSubType = m_pCurrentAction->GetSubType();
		// get previous leaderAction's unit properties
		uint32 unitProperties = m_pCurrentAction->GetUnitProperties();
		AbortExecution();
		// inform the puppet that the order is done
/*		if (IsPlayer())
		{
			if (GetFollowState() == FS_FOLLOW)
				Follow( m_szFormationDescriptor, m_iFormOwnerGroupID, Vec3(0,0,0));
			//	else
			//		Hold(NULL);
		}
		else */
		{
			IAIObject* pTarget = m_pGroup->GetAttentionTarget(true);
			AISignalExtraData* pData = new AISignalExtraData;
			pData->iValue = iActionType;
			pData->iValue2 = iActionSubType;
			if(pTarget)
			{
				// target id
				IEntity* pTargetEntity = NULL;
				if(CPuppet* pPuppet = pTarget->CastToCPuppet())
					pTargetEntity = pPuppet->GetEntity();
				else if(CAIPlayer* pPlayer = pTarget->CastToCAIPlayer())
					pTargetEntity = pPlayer->GetEntity();
				//target name
				if(pTargetEntity)
					pData->nID = pTargetEntity->GetId();
				pData->SetObjectName(pTarget->GetName());
				//target distance
				Vec3 avgPos = m_pGroup->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,unitProperties);
				pData->fValue = (avgPos - m_pGroup->GetEnemyAveragePosition()).GetLength();
				pData->point = m_pGroup->GetEnemyAveragePosition();
			}

			int sigFilter;
			CAIActor* pDestActor = CastToCAIActorSafe(GetAssociation());
			if (pDestActor && pDestActor->IsEnabled())
				sigFilter = SIGNALFILTER_SENDER;
			else 	
			{
				sigFilter = SIGNALFILTER_SUPERGROUP;
				if(!m_pGroup->GetUnits().empty())
				{
					TUnitList::iterator itUnit = m_pGroup->GetUnits().begin();
					pDestActor = itUnit->m_pUnit;
				}
			}

			if(pDestActor)
			{
				if(eUpdateResult==CLeaderAction::ACTION_DONE)
					GetAISystem()->SendSignal(sigFilter, 10, "OnLeaderActionCompleted", pDestActor, pData);
				else
					GetAISystem()->SendSignal(sigFilter, 10, "OnLeaderActionFailed", pDestActor, pData);
			}

		}
	}
	/*CPuppet* pLeaderPuppet = (CPuppet*) GetAssociation();
	if (pLeaderPuppet && !pLeaderPuppet->IsEnabled() && m_bLeaderAlive)
	{
		GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP, 0, "OnLeaderDied", pLeaderPuppet);
		m_bLeaderAlive = false;
	}
	*/

	// leader always moves beacon to known enemy position
/*	if(GetAISystem()->GetBeacon(GetGroupId()))
	{
		Vec3 enemyPos = GetEnemyPositionKnown();
		GetAISystem()->GetBeacon(GetGroupId())->SetPos(enemyPos);
	}
	else
	
*/

}

//
//----------------------------------------------------------------------------------------------------
void	CLeader::ProcessSignal(AISIGNAL& signal )
{
	if (m_pCurrentAction && m_pCurrentAction->ProcessSignal(signal))
		return;

	AISignalExtraData data;
	if ( signal.pEData )
	{
		data = *(AISignalExtraData*)signal.pEData;
		delete (AISignalExtraData*)signal.pEData;
		signal.pEData = NULL;
	}

	//if(!strcmp(signal.strText, AIORD_USE) ) 
	if (signal.Compare(g_crcSignals.m_nAIORD_USE) ) 
	{
		Use( data );
	}
	else if (signal.Compare(g_crcSignals.m_nAIORD_ATTACK) )/*if(!strcmp(signal.strText, AIORD_ATTACK))*/
	{
		ELeaderActionSubType attackType = static_cast<ELeaderActionSubType>(data.iValue);
		LeaderActionParams params;
		params.type = LA_ATTACK;
		params.subType =attackType;
		params.unitProperties = data.iValue2;
		params.id = data.nID;
		params.pAIObject = GetAISystem()->GetAIObjectByName(data.GetObjectName());
		params.fDuration = data.fValue;
		params.vPoint = data.point;
		params.name = data.GetObjectName();
		Attack(&params);
	}
	else if (signal.Compare(g_crcSignals.m_nAIORD_SEARCH) )/*if(!strcmp(signal.strText, AIORD_SEARCH))*/
	{
		Vec3 startPoint = data.point;
		Search((startPoint.IsZero()? m_pGroup->GetEnemyPositionKnown() : startPoint), data.fValue,(data.iValue ? data.iValue : UPR_ALL),data.iValue2);
	}
	/*else if (signal.Compare(g_crcSignals.m_nORD_ALIEN_SEARCH) )
	{
		if (m_pCurrentAction && m_pCurrentAction->GetType() == LA_ALIEN_SEARCH)
			return;
		AbortExecution();
		AIAssert(GetAssociation());
		int unitProperties = data.iValue ? data.iValue : UPR_ALL;
		m_pCurrentAction = new CLeaderAction_AlienSearch(this, unitProperties);
		m_pCurrentAction->SetPriority(PRIORITY_NORMAL);
	}*/
	else if (signal.Compare(g_crcSignals.m_nAIORD_LEAVE_VEHICLE) )/*if(!strcmp(signal.strText, AIORD_LEAVE_VEHICLE) ) */
	{
		LeaveVehicle();
	}
	else if (signal.Compare(g_crcSignals.m_nUSE_ORDER_ENABLED) )/*if(!strcmp(signal.strText, USE_ORDER_ENABLED) ) */
	{
		EnableUse(data.iValue);
	}
	else if (signal.Compare(g_crcSignals.m_nUSE_ORDER_DISABLED) )/*if(!strcmp(signal.strText, USE_ORDER_DISABLED) ) */
	{
		DisableUse(data.iValue);
	}
	else if (signal.Compare(g_crcSignals.m_nOnChangeStance) )/*if(!strcmp(signal.strText, "OnChangeStance") ) */
		ChangeStance(static_cast<EStance>(data.iValue));
	else if (signal.Compare(g_crcSignals.m_nOnEnableFire) )/*if(!strcmp(signal.strText, "OnEnableFire"))*/
	{
		CAIObject* pAILeader = (CAIObject*) GetAssociation();
		if(pAILeader)
			GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP, 0, "OnFireEnabled", pAILeader);
		m_bAllowFire = true;
		m_fAllowingFireTime = GetAISystem()->GetFrameStartTime();
	}
	else if (signal.Compare(g_crcSignals.m_nOnDisableFire) )/*if(!strcmp(signal.strText, "OnDisableFire"))*/
	{
		CAIObject* pAILeader = (CAIObject*) GetAssociation();
		if(pAILeader)
			GetAISystem()->SendSignal(SIGNALFILTER_GROUPONLY, 0, "OnFireDisabled", pAILeader);
		m_bAllowFire = false;
	}	
	else if (signal.Compare(g_crcSignals.m_nOnScaleFormation) )/*if(!strcmp(signal.strText, "OnScaleFormation"))*/
	{
		float fScale = data.fValue;
		if(m_pFormationOwner && m_pFormationOwner->m_pFormation)
			m_pFormationOwner->m_pFormation->SetScale(fScale);
	}
	else if (signal.Compare(g_crcSignals.m_nOnUnitDied) )/*if(!strcmp(signal.strText, "OnUnitDied"))*/
	{
		IEntity* pEntity = (IEntity*)signal.pSender;
		if(pEntity)
		{
			CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
	//		CAIObject* pAIObj = (CAIObject*)pEntity->GetAI();
			if(m_pCurrentAction)
				m_pCurrentAction->DeadUnitNotify(pActor);

			TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);
			if (senderUnit != m_pGroup->GetUnits().end())
				m_pGroup->RemoveMember(pActor);
			// TO DO: remove use action from list if it's owned by this unit
		}
	}
	else if (signal.Compare(g_crcSignals.m_nOnUnitBusy) )/*if(!strcmp(signal.strText, "OnUnitBusy"))*/
	{
		IEntity* pEntity = (IEntity*)signal.pSender;
		CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
		TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);

		if (senderUnit != m_pGroup->GetUnits().end())
		{
			if (m_pCurrentAction)
				m_pCurrentAction->BusyUnitNotify(*senderUnit);

			// TODO: busy units will be removed by now...
			// Now it isn't so bad because they will stay busy forever.
			// However, it's only a temporary solution!
			// They should still be there but marked as busy.
			m_pGroup->RemoveMember(pActor);
		}
	}
	else if (signal.Compare(g_crcSignals.m_nOnUnitSuspended) )/*if(!strcmp(signal.strText, "OnUnitSuspended"))*/
	{
		IEntity* pEntity = (IEntity*)signal.pSender;
		CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
		TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);

		if (senderUnit != m_pGroup->GetUnits().end())
		{
			if (m_pCurrentAction)
				m_pCurrentAction->BusyUnitNotify(*senderUnit);
		}
	}	
	else if (signal.Compare(g_crcSignals.m_nOnUnitResumed) )/*if(!strcmp(signal.strText, "OnUnitResumed"))*/
	{
		IEntity* pEntity = (IEntity*)signal.pSender;
		CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
		TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);

		if (senderUnit != m_pGroup->GetUnits().end())
		{
			if (m_pCurrentAction)
				m_pCurrentAction->ResumeUnit(*senderUnit);
		}
	}	
	else if (signal.Compare(g_crcSignals.m_nOnSetUnitProperties) )/*if(!strcmp(signal.strText, "OnSetUnitProperties"))*/
	{
		// to do: when the ILeader interface will not cause problems once modified... move this to AI script bind function
		IEntity* pEntity = (IEntity*)signal.pSender;
		CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
		TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);

		if (senderUnit != m_pGroup->GetUnits().end())
			senderUnit->SetProperties(data.iValue);
	}
	else if (signal.Compare(g_crcSignals.m_nOnJoinTeam) )/*if(!strcmp(signal.strText, "OnJoinTeam"))*/
	{
		// data.iValue = forces the new unit to follow the leader
		IEntity* pEntity = (IEntity*)signal.pSender;
		if(pEntity && pEntity->GetAI())
		{
			CPuppet* pPuppet = pEntity->GetAI()->CastToCPuppet();
			if(pPuppet)
			{
				TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pPuppet);
				if (senderUnit == m_pGroup->GetUnits().end())
				{
					pPuppet->SetGroupId(GetGroupId());
					AgentParameters myParam = pPuppet->GetParameters();
					myParam.m_bSpecial = m_Parameters.m_bSpecial;
					pPuppet->SetParameters(myParam);
					m_pGroup->AddMember(pPuppet);
					senderUnit = m_pGroup->GetUnit(pPuppet);
					GetAISystem()->SendSignal(SIGNALFILTER_SENDER,0,"OnTeamJoined",pPuppet);
				}
				if(data.iValue)
				{
					// if(!m_pCurrentAction)
						Follow(data.GetObjectName(), GetGroupId(), Vec3(0,0,0),&(*senderUnit));
					// else 
					//	AssignFormationPoint(*senderUnit,0,false,true);//not effective if there's no formation
				}
				if(m_pCurrentAction)
				{
					if(m_pCurrentAction->GetType()==LA_FOLLOW || data.iValue)
						m_pCurrentAction->SetUnitFollow(*senderUnit);
					else if(m_pCurrentAction->GetType()==LA_HOLD)
						m_pCurrentAction->SetUnitHold(*senderUnit); 

					m_pCurrentAction->ResumeUnit(*senderUnit);
				}
				/*
				TO DO : allow follow/folowhiding of one unit only, without leaderaction or with some special leaderaction
				else if(data.iValue)
				{
					LeaderCreateFormation(data.GetObjectName(),Vec3(0,0,0));
//					AssignFormationPoint(*senderUnit);
					if(m_pFormationOwner && m_pFormationOwner->m_pFormation)
						pAIObj->SetSignal(1, GetAlertStatus()==AIALERTSTATUS_SAFE ? "ORDER_FORM" : "ORDER_FOLLOW_HIDE");
					else
						pAIObj->SetSignal(1,"OnFollowRequestDenied");
					
				}*/
			}
		}
		
	}
	else if (signal.Compare(g_crcSignals.m_nRPT_LeaderDead) )/*if(!strcmp(signal.strText, "RPT_LeaderDead"))*/
	{
		if(!m_pCurrentAction) // if there is a current action, the OnLeaderDied signal will be sent after the action finishes
		{
			CAIObject* pAILeader = (CAIObject*) GetAssociation();
			if(pAILeader && m_bLeaderAlive)
			{
				GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP, 0, "OnLeaderDied", pAILeader);
				m_bLeaderAlive = false;
			}
		}
	}
	else if (signal.Compare(g_crcSignals.m_nOnVehicleRequest) )/*if(!strcmp(signal.strText, "OnVehicleRequest"))*/
	{
//		if(data.GetObjectName() && strlen(data.GetObjectName()))
/*	TEMP FIX - disable vehicle finding
		IAIObject* pAITarget = NULL;
		if(data.nID.n)
		{
			IEntity* pEntity = GetAISystem()->m_pSystem->GetIEntitySystem()->GetEntity(data.nID.n);
			if(pEntity)
				pAITarget = pEntity->GetAI();
		}
		GetAISystem()->RequestVehicle(this,data.point,pAITarget,data.point2,data.iValue);
*/
	}
	else if (signal.Compare(g_crcSignals.m_nOnVehicleListProvided) )/*if(!strcmp(signal.strText, "OnVehicleListProvided"))*/
	{
		TAIObjectList* pVehiclelist = GetAISystem()->GetVehicleList(this);
		if(pVehiclelist)
		{
			for (TAIObjectList::iterator itv= pVehiclelist->begin(); itv!=pVehiclelist->end();++itv)
			{// TO DO: further checks on vehicles?
				UseVehicle(static_cast<CAIObject*>(*itv),data);
				break;
			}

			
		}
	}
	else if (signal.Compare(g_crcSignals.m_nOnEnterEnemyArea) )/*if(!strcmp(signal.strText,"OnEnterEnemyArea"))*/
	{
		if(m_bUpdateAlertStatus)
		{
			SetAlertStatus(AIALERTSTATUS_UNSAFE);
			m_currentAreaSpecies = data.iValue;
		}
	}
	else if (signal.Compare(g_crcSignals.m_nOnLeaveEnemyArea) )/*if(!strcmp(signal.strText,"OnLeaveEnemyArea"))*/
	{
		if(m_bUpdateAlertStatus)
		{
			SetAlertStatus(AIALERTSTATUS_SAFE);
			m_currentAreaSpecies = GetParameters().m_nSpecies;
		}
	}
	else if (signal.Compare(g_crcSignals.m_nOnEnterFriendlyArea) )/*if(!strcmp(signal.strText,"OnEnterFriendlyArea"))*/
	{
		if(m_bUpdateAlertStatus)
		{
			m_bForceSafeArea = true;
			SetAlertStatus(AIALERTSTATUS_SAFE);
			m_currentAreaSpecies = GetParameters().m_nSpecies;
		}
	}
	else if (signal.Compare(g_crcSignals.m_nOnLeaveFriendlyArea) )/*if(!strcmp(signal.strText,"OnLeaveFriendlyArea"))*/
	{
		if(m_bUpdateAlertStatus)
			m_bForceSafeArea = false;
	}
	else if (signal.Compare(g_crcSignals.m_nOnEnableAlertStatus) )/*if(!strcmp(signal.strText,"OnEnableAlertStatus"))*/
	{
		m_bUpdateAlertStatus = true;
		if(signal.pEData && signal.pEData->fValue >0)
			m_AlertStatusReadyRange = signal.pEData->fValue;
	}
	else if (signal.Compare(g_crcSignals.m_nOnDisableAlertStatus) )/*if(!strcmp(signal.strText,"OnDisableAlertStatus"))*/
	{
		m_bUpdateAlertStatus = false;
		SetAlertStatus(AIALERTSTATUS_SAFE);
	}
	else if (signal.Compare(g_crcSignals.m_nOnSpotSeeingTarget) )/*if(!strcmp(signal.strText,"OnSpotSeeingTarget"))*/
	{
		AddShootSpot(data.nID.n,data.point);
	}
	else if (signal.Compare(g_crcSignals.m_nOnSpotLosingTarget) )/*if(!strcmp(signal.strText,"OnSpotLosingTarget"))*/
	{
		RemoveShootSpot(data.nID.n);
	}

	if(m_pCurrentAction)
	{
		//if(!strcmp(signal.strText, AIORD_REPORTDONE))
		if (signal.Compare(g_crcSignals.m_nAIORD_REPORTDONE) )
		{
			IEntity* pEntity = (IEntity*)signal.pSender;
			if(pEntity)
			{
				CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
				TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);
				if(senderUnit != m_pGroup->GetUnits().end())
				{
					(*senderUnit).TaskExecuted();	
					(*senderUnit).ExecuteTask();
				}
			}
		}
		else if (signal.Compare(g_crcSignals.m_nOnPause) )/*if(!strcmp(signal.strText, "OnPause"))*/
		{ // let the unit assign himself a custom task and finish it before continuing with the plan
			IEntity* pEntity = (IEntity*)signal.pSender;
			CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
			TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);
			if(senderUnit != m_pGroup->GetUnits().end())
			{
				CUnitAction* pAction = new CUnitAction(UA_WHATSNEXT,true);
				(*senderUnit).m_Plan.push_front(pAction);	
				(*senderUnit).ExecuteTask();
			}
		}
		else if (signal.Compare(g_crcSignals.m_nOnTaskSuspend) )/*if(!strcmp(signal.strText, "OnTaskSuspend"))*/
		{
			IEntity* pEntity = (IEntity*)signal.pSender;
			CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
			TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);
			if(senderUnit != m_pGroup->GetUnits().end())
			{
				(*senderUnit).SuspendTask();	
			}
		}
		else if (signal.Compare(g_crcSignals.m_nOnTaskResume) )/*if(!strcmp(signal.strText, "OnTaskResume"))*/
		{
			IEntity* pEntity = (IEntity*)signal.pSender;
			CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI());
			TUnitList::iterator senderUnit = std::find(m_pGroup->GetUnits().begin(), m_pGroup->GetUnits().end(), pActor);
			if(senderUnit != m_pGroup->GetUnits().end())
			{
				(*senderUnit).ResumeTask();
			}
		}
		else if (signal.Compare(g_crcSignals.m_nOnAbortAction) )/*if(!strcmp(signal.strText, "OnAbortAction"))*/
		{
			AbortExecution();
			CLeader* pAILeader = CastToCLeaderSafe(GetAssociation());
			if(pAILeader)
				pAILeader->SetSignal(1,"OnAbortAction", 0,0, g_crcSignals.m_nOnAbortAction);
		}
		else if (signal.Compare(g_crcSignals.m_nOnFormationPointReached) )/*if(!strcmp(signal.strText, "OnFormationPointReached"))*/
		{	// TO DO: what to do with this?
			IEntity* pEntity = (IEntity*)signal.pSender;
			CAIObject* pAIObj = (CAIObject*)pEntity->GetAI();
			TUnitList::iterator itEnd =  m_pGroup->GetUnits().end();
		}
	}

	//if(!strcmp(signal.strText, AIORD_FOLLOW))
	if (signal.Compare(g_crcSignals.m_nAIORD_FOLLOW) )
	{
		// signal.EData.sObjectName = formation descriptor name
		// signal.EData.iValue = formation's owner group id; if it's this' group id, formation is created
		//						otherwise it's joined
		// signal.EData.nID = another Leader's entity id. if it's different from this'id, the other leader 
		//					  will join the formation
		m_iFormOwnerGroupID = data.iValue; 
		int iOtherEntityID = data.nID.n;
		if (m_pCurrentAction && m_pCurrentAction->GetType()!=LA_HOLD && m_pCurrentAction->GetType()!=LA_FOLLOW)
			//TO DO: what to do with the other leader joining my formation if I'm busy with another action?
		{
//			if (m_iFormOwnerGroupID == 0 && m_pCurrentAction->GetType()==LA_ATTACK)// && m_FollowState == FS_FOLLOW)
			{
				Follow( data.GetObjectName(), m_iFormOwnerGroupID, data.point,NULL,true);
				if(m_pFormationOwner)
				{
					IEntity* pEntity = GetAISystem()->m_pSystem->GetIEntitySystem()->GetEntity(iOtherEntityID);
					if(pEntity && pEntity->GetAI())
					{
						CAIActor* pActor = pEntity->GetAI()->CastToCAIActor();
						CLeader* pOtherLeader = GetAISystem()->GetLeader(pActor->GetGroupId());
						if ( pOtherLeader && this != pOtherLeader )
						{
							AISignalExtraData* pData = new AISignalExtraData;
							pData->iValue = m_iFormOwnerGroupID;
	//						pOtherLeader->SetSignal(1,AIORD_FOLLOW,this,pData);
							pOtherLeader->SetSignal(1,AIORD_FOLLOW,NULL,pData, g_crcSignals.m_nAIORD_FOLLOW );
						}
					}
				}
			}
//			else
//				SetFollowState(FS_FOLLOW);

/*			// we're not in hold state
			//abort all the special actions like use_rpg, planting bomb etc and ma
			for(TUnitList::iterator itUnit = m_pGroup->GetUnits().begin();itUnit!=m_pGroup->GetUnits().end();++itUnit)
			{
				CUnitImg& unit = (*itUnit);
				while(unit.GetCurrentAction()->IsSpecialAction())
				{
					CUnitAction* 
					pOtherLeader->SetSignal(1,AIORD_FOLLOW,this,&(AISIGNAL_EXTRA_DATA(m_iFormOwnerGroupID)));
				}
			}
*/		}
		else
		{
			Follow( data.GetObjectName(), m_iFormOwnerGroupID, data.point);
			IEntity* pEntity = GetAISystem()->m_pSystem->GetIEntitySystem()->GetEntity(iOtherEntityID);
			if(pEntity && pEntity->GetAI())
			{
				CAIActor* pActor = pEntity->GetAI()->CastToCAIActor();
				CLeader* pOtherLeader = GetAISystem()->GetLeader(pActor->GetGroupId());
				if ( pOtherLeader && this != pOtherLeader )
				{
					AISignalExtraData* pData = new AISignalExtraData;
					pData->iValue = m_iFormOwnerGroupID;
					CAIObject* pAILeader = static_cast<CAIObject*>(GetAssociation());
					if(pAILeader)
//						pOtherLeader->SetSignal(1,AIORD_FOLLOW,pAILeader,pData);
						pOtherLeader->SetSignal(1,AIORD_FOLLOW,NULL,pData, g_crcSignals.m_nAIORD_FOLLOW);
				}
			}
		}

	}
	else if (signal.Compare(g_crcSignals.m_nAIORD_HOLD) )/*if(!strcmp(signal.strText, AIORD_HOLD))*/
	{
		if(m_pCurrentAction && m_pCurrentAction->GetType()!=LA_FOLLOW && m_pCurrentAction->GetType()!=LA_HOLD)
			SetFollowState(FS_HOLD);
	//	else
		//	Hold(data.point);
	}
	else if (signal.Compare(g_crcSignals.m_nAIORD_GOTO) )/*if(!strcmp(signal.strText, AIORD_GOTO))*/
	{
		Goto(data.point);
	}
	else if (signal.Compare(g_crcSignals.m_nAIORD_IDLE) )/*if(!strcmp(signal.strText, AIORD_IDLE))*/
		Idle();
	else if (signal.Compare(g_crcSignals.m_nOnKeepEnabled) )/* if(!strcmp(signal.strText, "OnKeepEnabled"))*/
		m_bKeepEnabled = true;

}

//
//----------------------------------------------------------------------------------------------------
void	CLeader::AbortExecution(int priority)
{

	ClearAllPlannings(UPR_ALL,priority);

	if(m_pCurrentAction)
	{

		delete m_pCurrentAction;
		m_pCurrentAction = NULL;
	}
}

//
//----------------------------------------------------------------------------------------------------
void	CLeader::ClearAllPlannings(uint32 unitProp,int priority)
{
	for(TUnitList::iterator unitItr=m_pGroup->GetUnits().begin(); unitItr!=m_pGroup->GetUnits().end(); ++unitItr)
	{
		CUnitImg& unit = *unitItr;
		if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & unitProp))
		{
			CUnitImg &curUnit = (*unitItr);
			curUnit.ClearPlanning(priority);
		}
	}
}

//
//----------------------------------------------------------------------------------------------------
bool CLeader::IsIdle() const
{
	for(TUnitList::const_iterator itUnit=m_pGroup->GetUnits().begin();itUnit != m_pGroup->GetUnits().end();++itUnit)
		if(!itUnit->Idle() && itUnit->m_pUnit->IsEnabled())
			return false;
	return true;
}
//
//----------------------------------------------------------------------------------------------------
void CLeader::Idle()
{
	AbortExecution(PRIORITY_HIGH);

	for(TUnitList::iterator itUnit=m_pGroup->GetUnits().begin();itUnit != m_pGroup->GetUnits().end();++itUnit)
	{
		CAIActor* pAIActor = itUnit->m_pUnit->CastToCAIActor();
		pAIActor->SetSignal(0,"ORDER_IDLE", 0, 0, g_crcSignals.m_nORDER_IDLE);
	}
}



//
//----------------------------------------------------------------------------------------------------
void CLeader::Attack(const LeaderActionParams* pParams)
{

//	if (m_pCurrentAction && m_pCurrentAction->GetType() == LA_ATTACK)
//		return;
	AbortExecution(PRIORITY_NORMAL);
	//AIAssert(GetAssociation());
	LeaderActionParams	defaultParams(LA_ATTACK,LAS_DEFAULT);
	m_pCurrentAction = CreateAction(pParams ? pParams : &defaultParams);
	m_pCurrentAction->SetPriority(PRIORITY_NORMAL);
}


//
//----------------------------------------------------------------------------------------------------
void CLeader::Search(const Vec3& targetPos, float searchDistance, uint32 unitProperties, int searchAIObjectType)
{
	if (m_pCurrentAction && m_pCurrentAction->GetType() == LA_SEARCH)
		return;
//	m_targetPoint = targetPos;
	AbortExecution(PRIORITY_NORMAL);
	//AIAssert(GetAssociation());
	LeaderActionParams params(LA_SEARCH,LAS_DEFAULT);
	params.unitProperties = unitProperties;
	params.fSize = searchDistance;
	params.vPoint = targetPos;
	params.iValue = searchAIObjectType;
	m_pCurrentAction = CreateAction(&params);
	//new CLeaderAction_Search(this, unitProperties);
	m_pCurrentAction->SetPriority(PRIORITY_NORMAL);
}


//
//----------------------------------------------------------------------------------------------------
void	CLeader::Follow(const char* szFormationName , int iGroupID, const Vec3& vTargetPos, CUnitImg* pSingleUnit,bool bForceNotHiding)
{

	bool bCreateNewFormation = true;
	
	// hack: remove higher priority UA_FORM action, hasn't been removed by clearplanning
	if(pSingleUnit)
		pSingleUnit->ClearUnitAction(UA_FORM);
	else
		for(TUnitList::iterator itUnit = m_pGroup->GetUnits().begin(); itUnit!=m_pGroup->GetUnits().end();++itUnit)
			itUnit->ClearUnitAction(UA_FORM);
	
/*	if(m_pCurrentAction && m_pCurrentAction->GetType()== LA_FOLLOW)
		if(szFormationName && m_szFormationDescriptor==szFormationName)
			bCreateNewFormation = false;
	
	if(szFormationName==NULL)
		szFormationName = m_szFormationDescriptor.c_str();
	if(iGroupID<0)
		iGroupID = GetGroupId();
*/
/*	if(bCreateNewFormation)
	{
		if(iGroupID == GetGroupId())
			LeaderCreateFormation(szFormationName,vTargetPos);
		else 
			JoinFormation(iGroupID);
	}
*/
//	if(m_pFormationOwner && m_pFormationOwner->m_pFormation)
//	{
		if(m_pCurrentAction)
			AbortExecution(m_pCurrentAction->GetPriority());
		if(!m_pCurrentAction)
		{
			LeaderActionParams params;
			params.pAIObject = pSingleUnit? pSingleUnit->m_pUnit : NULL;
			params.vPoint = vTargetPos;
			params.fSize = float(iGroupID);
			params.name = szFormationName;

//			if(m_alertStatus == AIALERTSTATUS_SAFE || bForceNotHiding)
				m_pCurrentAction = new CLeaderAction_Follow(this,params);
//			else
//				m_pCurrentAction = new CLeaderAction_FollowHiding(this,params);
		}
		m_pCurrentAction->SetPriority(PRIORITY_NORMAL);
		SetFollowState(FS_FOLLOW);
		m_iFormOwnerGroupID = iGroupID;
//	}
}


//----------------------------------------------------------------------------------------------------
void	CLeader::Goto( const Vec3& point )
{ 
/*
	SetFollowState(FS_HOLD);
	if (m_pCurrentAction)
		AbortExecution(PRIORITY_HIGH);//m_pCurrentAction->GetPriority());
	AIAssert(0 == m_pCurrentAction);
	m_pCurrentAction = new CLeaderAction_Hold(this,point);
	m_pCurrentAction->SetPriority(PRIORITY_HIGH);
*/
}

//
//----------------------------------------------------------------------------------------------------
void	CLeader::Use(const AISignalExtraData& SignalData)
{
/*		if(SignalData.iValue ==AIUSEOP_PLANTBOMB)
		{
			if(m_pCurrentAction)
				m_pCurrentAction->Use_PlantBomb(SignalData);
			else
			{
				const int priority = PRIORITY_NORMAL;
				AbortExecution(priority);
				m_pCurrentAction = new CLeaderAction_Use_PlantBomb(this,SignalData);
				m_pCurrentAction->SetPriority(priority);
			}

		}
		else*/ if(SignalData.iValue ==AIUSEOP_VEHICLE)
		{
			if(GetFollowState()==FS_FOLLOW || !IsPlayer())
			{
				const int priority = PRIORITY_VERY_HIGH;
				AbortExecution(priority);
				m_pCurrentAction = new CLeaderAction_Use_Vehicle(this,SignalData);
				m_pCurrentAction->SetPriority(priority);
			}
		}
		/*else if(SignalData.iValue ==AIUSEOP_RPG)
		{
			if(m_pCurrentAction)
				m_pCurrentAction->Use_Rpg(SignalData);
		}*/
}

void	CLeader::UseVehicle(const CAIObject* pVehicle, const AISignalExtraData& data)
{
	const int priority = PRIORITY_VERY_HIGH;
	AbortExecution(priority);
	m_pCurrentAction = new CLeaderAction_Use_Vehicle(this,pVehicle, data);
	m_pCurrentAction->SetPriority(priority);

}
//----------------------------------------------------------------------------------------------------

void	CLeader::LeaveVehicle(bool bFollowingAfter)
{
	if(m_pCurrentAction )
	{
		if(m_pCurrentAction->GetType() != LA_USE_VEHICLE) 
			return;
	}
	const int priority = PRIORITY_NORMAL;
	AbortExecution();

	//m_pCurrentAction = new CLeaderAction_Use_LeaveVehicle(this);
	//m_pCurrentAction->SetPriority(priority);

	//TO DO: manage bFollowingAfter: if true, a new leaderaction Follow must be set after leaving vehicle
			
}

/*//----------------------------------------------------------------------------------------------------
void CLeader::FindNearestHidingPoints(const TUnitList& units, CUnitPointMap& hidingPoints)
{

	CPuppet* pLeaderPuppet = (CPuppet*) GetAssociation();

	// find all hide points within the search distance
	// TODO : fix hide point search algorithm to work incrementally like: "find me the next nearest hide point..."
	CGraph* pGraph = GetAISystem()->GetHideGraph();
	if (!pGraph)
	{
		// no hiding points - NO HIDE GRAPH
		// generate signal that there is no Hiding place
		pLeaderPuppet->SetSignal(1,"OnNoHidingPlace");
		pLeaderPuppet->m_bLastHideResult = false;
		return;
	}

	pGraph->SelectNodesInSphere(pLeaderPuppet->GetPos(), 30.0f, pLeaderPuppet->m_pLastNode);
	ListObstacles& nodes = pGraph->m_lstSelected;
	if (nodes.empty())
	{
		// no hiding points - Nothing within this radius
		// generate signal that there is no Hiding place
		pLeaderPuppet->SetSignal(1,"OnNoHidingPlace");
		pLeaderPuppet->m_bLastHideResult = false;
		return;
	}

	// Convert std::list to std::set
	// TODO : Instead of std::list for m_lstSelected change the graph to use std::set
	typedef std::set< ObstacleData* > SetObstacles;
	SetObstacles obstacles;
	ListObstacles::iterator itNodes, itNodesEnd = nodes.end();
	for (itNodes = nodes.begin(); itNodes != itNodesEnd; ++itNodes)
		obstacles.insert(&(*itNodes));

	typedef std::multimap< float, std::pair< const CUnitImg*, ObstacleData* > > DistanceMap;
	DistanceMap distanceMap;

	TUnitList::const_iterator itUnits, itUnitsEnd = units.end();
	typedef std::set< const CUnitImg* > SetUnits;
	SetUnits setUnits;
	for (itUnits = units.begin(); itUnits != itUnitsEnd; ++itUnits)
	{
		const CUnitImg* pUnit = &(*itUnits);
		setUnits.insert(pUnit);

		// compute (squared) distances
		SetObstacles::iterator itObstacles, itObstaclesEnd = obstacles.end();
		for (itObstacles = obstacles.begin(); itObstacles != itObstaclesEnd; ++itObstacles)
		{
			ObstacleData* pObstacle = *itObstacles;
			float distance = (pUnit->m_pUnit->GetPos() - pObstacle->vPos).GetLengthSquared();
			distanceMap.insert(std::make_pair(distance, std::make_pair(pUnit, pObstacle)));
		}
	}

	DistanceMap::iterator it = distanceMap.begin(), itEnd = distanceMap.end();
	while (!nodes.empty() && !setUnits.empty() && it != itEnd)
	{
		const CUnitImg* pUnit = it->second.first;
		ObstacleData* pObstacle = it->second.second;

		SetUnits::iterator findUnit = setUnits.find(pUnit);
		if (findUnit != setUnits.end())
		{
			SetObstacles::iterator findObstacle = obstacles.find(pObstacle);
			if (findObstacle != obstacles.end())
			{
				hidingPoints[(CUnitImg*)pUnit] = GetHidePoint(pObstacle);
				setUnits.erase(findUnit);
				obstacles.erase(findObstacle);
			}
		}

		++it;
	}

}
*/

void CLeader::OnEnemyStatsUpdated(const Vec3& avgEnemyPos, const Vec3& oldAvgEnemyPos)
{
	if(!IsPlayer())
	{
		if(avgEnemyPos.IsZero() && !oldAvgEnemyPos.IsZero())
		{
			CLeader* pAILeader = CastToCLeaderSafe(GetAssociation());
			if(pAILeader)
				pAILeader->SetSignal(1,"OnNoGroupTarget", 0, 0, g_crcSignals.m_nOnNoGroupTarget);
		}
		else if(!avgEnemyPos.IsZero() && oldAvgEnemyPos.IsZero())
		{
			CAIObject* pAILeader = static_cast<CAIObject*>(GetAssociation());
			IAIObject* pTarget = m_pGroup->GetAttentionTarget(true,true);
			if(pTarget && pAILeader)
			{
				IEntity* pEntity = pTarget->GetEntity();
				if(pEntity)
				{
					AISignalExtraData* pData = new AISignalExtraData;
					pData->nID = pEntity->GetId();
					GetAISystem()->SendSignal(SIGNALFILTER_SENDER, 0, "OnGroupTarget", pAILeader, pData);
				}
			}
		}
	}
}

//
//-------------------------------------------------------------------------------------------
void CLeader::UpdateEnemyStats()
{
	// update beacon
	if(m_pCurrentAction)
		m_pCurrentAction->UpdateBeacon();
	else 
	{
		CAIObject* pBeaconUser = static_cast<CAIObject*>(GetAssociation());
		if(!pBeaconUser || IsPlayer())
			for (TUnitList::const_iterator it = m_pGroup->GetUnits().begin();it!=m_pGroup->GetUnits().end(); ++it)
			{
				CAIObject* pUnit = it->m_pUnit;
				if(pUnit && pUnit != pBeaconUser && pUnit->IsEnabled())
				{
					pBeaconUser = pUnit;
					break;
				}
			}

		if(!pBeaconUser)
			return;

		const Vec3&	avgEnemyPos(m_pGroup->GetEnemyAveragePosition());
		const Vec3&	avgLiveEnemyPos(m_pGroup->GetLiveEnemyAveragePosition());

		if(!avgLiveEnemyPos.IsZero())
			GetAISystem()->UpdateBeacon(GetGroupId(), avgLiveEnemyPos, pBeaconUser);
		else if(!avgEnemyPos.IsZero())
			GetAISystem()->UpdateBeacon(GetGroupId(), avgEnemyPos, pBeaconUser);
		Vec3 vEnemyDir = m_pGroup->GetEnemyAverageDirection(true,false);
		if(!vEnemyDir.IsZero())
		{
			IAIObject*	pBeacon = GetAISystem()->GetBeacon(GetGroupId());
			if(pBeacon)
      {
        pBeacon->SetBodyDir(vEnemyDir);
        pBeacon->SetMoveDir(vEnemyDir);
      }
		}
	}
}

//
//--------------------------------------------------------------------------------------------
void CLeader::OnObjectRemoved(CAIObject* pObject)
{

	if(pObject == m_pAssociation)
	{
		GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP, 0, "OnLeaderDied", pObject);
		m_bLeaderAlive = false;
		m_pAssociation = NULL;
	}

	if(pObject == m_pFormationOwner)
		m_pFormationOwner = NULL;
	
	if(pObject == m_pPrevFormationOwner)
		m_pPrevFormationOwner = NULL;
	
	if(pObject == m_pEnemyTarget)
		m_pEnemyTarget = NULL;

	for (TVecLeaders::iterator it = m_FormationJoiners.begin();it!=m_FormationJoiners.end(); )
	{
		if(pObject == *it)
			m_FormationJoiners.erase(it++);
		else
			++it;
	}

	CAIActor::OnObjectRemoved(pObject);

	/*
	IEntity* pEntity = pObject->GetEntity();
	if(pEntity)
	{
		DynArray<AISIGNAL>::iterator its= m_State.vSignals.begin(), itEnd = m_State.vSignals.end();
		while (its!=itEnd)
		{
			AISIGNAL& sstruct = *its;
			if(sstruct.pSender == pEntity)
				m_State.vSignals.erase(its++);
			else
				++its;
		}

	}
	*/
}

//---------------------------------------------------------------------------------------

Vec3 CLeader::GetHidePoint(const CAIObject* pAIObject, const CObstacleRef& obstacle) const
{
	if (obstacle.GetAnchor() || obstacle.GetNodeIndex())
		return obstacle.GetPos();

	if (obstacle.GetVertex() < 0)
		return Vec3(0.0f, 0.0f, 0.0f);
	
	// lets create the place where we will hide
	
	Vec3 vHidePos = obstacle.GetPos();
	const CAIActor* pActor = pAIObject->CastToCAIActor();
	float fDistanceToEdge = pActor->GetParameters().m_fPassRadius +  obstacle.GetApproxRadius() + 1.f;
	Vec3 enemyPosition = m_pGroup->GetEnemyPosition(pAIObject);

	Vec3 vHideDir = vHidePos - enemyPosition;
	vHideDir.z = 0.f;
	vHideDir.NormalizeSafe();

	IPhysicalWorld *pWorld = GetAISystem()->GetPhysicalWorld();
	ray_hit hit;
	while (1)
	{
		int rayresult(0);
		TICK_COUNTER_FUNCTION
		TICK_COUNTER_FUNCTION_BLOCK_START
		rayresult = pWorld->RayWorldIntersection(vHidePos - vHideDir*0.5f, vHideDir*5.f, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1);
		TICK_COUNTER_FUNCTION_BLOCK_END

		if (rayresult)
		{
			if (hit.n.Dot(vHideDir) < 0)
				vHidePos += vHideDir * fDistanceToEdge;
			else
			{
				vHidePos = hit.pt + vHideDir * 2.0f;
				break;
			}
		}
		else
		{
			vHidePos += vHideDir * 3.0f;
			break;
		}
	}

	return vHidePos;
}

Vec3 CLeader::GetSearchPoint(const CAIObject* pAIObject, const CObstacleRef& obstacle) const
{
	// find the place where someone could hide
	const Vec3& pos = pAIObject->GetPos();

	if (obstacle.GetAnchor() || obstacle.GetNode())
	{
		const Vec3& posHide = obstacle.GetPos();
	//	bool visible = GetAISystem()->CheckPointsVisibility(pos, posHide, 0); // 0 means check whole range
	//	if (visible)
	//		return Vec3(0, 0, 0);
	//	else
			return posHide;
	}

	if (obstacle.GetVertex() < 0)
		return Vec3(0.0f, 0.0f, 0.0f);

	Vec3 vHidePos = obstacle.GetPos();
	Vec3 vHideDir = vHidePos - pos;
	vHideDir.z = 0.f;
	vHideDir.NormalizeSafe();

	IPhysicalWorld *pWorld = GetAISystem()->GetPhysicalWorld();
	ray_hit hit;
	while (1)
	{
		int rayresult;
TICK_COUNTER_FUNCTION
TICK_COUNTER_FUNCTION_BLOCK_START
		rayresult = pWorld->RayWorldIntersection(vHidePos - vHideDir*0.5f, vHideDir*5.f, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1);
TICK_COUNTER_FUNCTION_BLOCK_END

		if (rayresult)
		{
			if (hit.n.Dot(vHideDir) < 0)
				vHidePos += vHideDir * 0.5f;
			else
			{
				vHidePos = hit.pt + vHideDir * 2.0f;
				break;
			}
		}
		else
		{
			vHidePos += vHideDir * 3.0f;
			break;
		}
	}

	return vHidePos;
}

Vec3 CLeader::GetHoldPoint(const CObstacleRef& obstacle, const Vec3& refPosition) const
{
	if (obstacle.GetAnchor() || obstacle.GetNode())
		return obstacle.GetPos();

	if (obstacle.GetVertex() < 0)
		return Vec3(0.0f, 0.0f, 0.0f);

	// lets create the place where we will hide
	Vec3 vHidePos = obstacle.GetPos();

	Vec3 vHideDir = refPosition - vHidePos;
	vHideDir.z = 0.f;
	vHideDir.NormalizeSafe();

	IPhysicalWorld *pWorld = GetAISystem()->GetPhysicalWorld();
	ray_hit hit;
	while (1)
	{
		int rayresult;
TICK_COUNTER_FUNCTION
TICK_COUNTER_FUNCTION_BLOCK_START
		rayresult = pWorld->RayWorldIntersection(vHidePos, vHideDir*30.f, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER, &hit, 1);
TICK_COUNTER_FUNCTION_BLOCK_END

		if (rayresult)
		{
			Vec3 nml = hit.n;
			if (nml.Dot(vHideDir) < 0)
				vHidePos += vHideDir * 0.5f;
			else
			{
				vHidePos = hit.pt + vHideDir * 2.0f;
				break;
			}
		}
		else
		{
			vHidePos += vHideDir * 3.0f;
			break;
		}
	}

	return vHidePos;
}



bool CLeader::LeaderCreateFormation(const char *szFormationDescr ,const Vec3& vTargetPos, bool bIncludeLeader, uint32 unitProp, CAIObject* pOwner)
{
	ReleaseFormation();
	int unitCount = m_pGroup->GetUnitCount(unitProp);
	//if(m_pGroup->GetUnits().size()==0)
	if(unitCount==0)
		return false;

	// Select formation owner
	CAIObject* pAILeader = static_cast<CAIObject*>(GetAssociation());
	CAIActor* pLeaderActor = CastToCAIActorSafe(pAILeader);
	
	TUnitList::iterator itLeader = m_pGroup->GetUnit(pLeaderActor);

	if(IsPlayer())
		m_pFormationOwner = pAILeader;
	else if(pOwner)
		m_pFormationOwner = pOwner;
	else if(vTargetPos.IsZero() && bIncludeLeader && itLeader!=m_pGroup->GetUnits().end() && (itLeader->GetProperties() & unitProp))
	{	
		m_pFormationOwner  = static_cast<CAIObject*>(GetAssociation()); // get Leader puppet by default
		if(!m_pFormationOwner)
			m_pFormationOwner = m_pGroup->GetUnits().begin()->m_pUnit;
	}
	else
	{
		float fMinDist = std::numeric_limits<float>::max();
		for(TUnitList::iterator itUnit = m_pGroup->GetUnits().begin(); itUnit!=m_pGroup->GetUnits().end() ;++itUnit)
		{	
			if(!bIncludeLeader && itUnit->m_pUnit == pAILeader)
				continue;
			if(!(itUnit->GetProperties() & unitProp))
				continue;
			float fDist = (itUnit->m_pUnit->GetPos() - vTargetPos).GetLengthSquared();
			if(fDist < fMinDist)
			{
				m_pFormationOwner = itUnit->m_pUnit;
				fMinDist = fDist;
			}
		}
	}
	if(!m_pFormationOwner)
		return false;

	if(m_pFormationOwner->CreateFormation(szFormationDescr,vTargetPos))
	{
		m_szFormationDescriptor = szFormationDescr;
	}

	if(m_pFormationOwner->m_pFormation)
	{
//		Vec3 vDir = (vTargetPos.IsZero() ? Vec3(0,0,0): vTargetPos - m_pFormationOwner->GetPos());
//		m_pFormationOwner->m_pFormation->InitWorldPosition(m_pFormationOwner,vDir);
		
		//assign places to team members
		m_pFormationOwner->m_pFormation->GetNewFormationPoint(m_pFormationOwner->CastToCAIActor(),0);

		//int formationSize = m_pFormationOwner->m_pFormation->GetSize();
		if(AssignFormationPoints(bIncludeLeader, unitProp)+1 <= unitCount/2)
		{ // not more than half of the points can be joined , abort
//			ReleaseFormation();
	//		return false;
		}

		m_FormationJoiners.clear();
		m_pPrevFormationOwner = m_pFormationOwner;
		return true;
	}
	m_pFormationOwner = NULL;
	return false;
}


bool CLeader::JoinFormation(int iGroupID)
{
	CLeader* pOtherLeader = GetAISystem()->GetLeader(iGroupID);
	if(this==pOtherLeader) // can't join a formation of my group
		return false;
	
	if(!pOtherLeader || !pOtherLeader->m_pFormationOwner)
		return false;
	
	CFormation* pFormation = pOtherLeader->m_pFormationOwner->m_pFormation;
	if(!pFormation)
		return false;

	int iFreePlaces = pFormation->CountFreePoints();
	if(!iFreePlaces)
		return false;

	// found places, join the formation
	m_pFormationOwner = pOtherLeader->m_pFormationOwner;

	NotifyJoiningFormation(pOtherLeader);
	AssignFormationPoints();

	return true;
}


void CLeader::NotifyJoiningFormation(CLeader* pOtherLeader) 
{
	pOtherLeader->m_FormationJoiners.push_back(this);
}

void CLeader::NotifyLeavingFormation(CLeader* pOtherLeader) const
{
	TVecLeaders::iterator itL = std::find(pOtherLeader->m_FormationJoiners.begin(),pOtherLeader->m_FormationJoiners.end(),this);
	if(itL!=pOtherLeader->m_FormationJoiners.end())
		pOtherLeader->m_FormationJoiners.erase(itL);
}


void CLeader::NotifyBreakingFormation()
{
	for(TVecLeaders::iterator itL = m_FormationJoiners.begin() ;itL!=m_FormationJoiners.end();++itL)
		(*itL)->m_pFormationOwner = NULL;

	m_FormationJoiners.clear();
}



bool CLeader::ReleaseFormation()
{
	if(m_pFormationOwner && m_pFormationOwner->m_pFormation)
	{
		CAIActor* pOwnerActor = m_pFormationOwner->CastToCAIActor();
		int iOwnerGroup = GetGroupId();
		if(pOwnerActor)
			iOwnerGroup = pOwnerActor->GetGroupId();
		else if(m_pFormationOwner->GetSubType()==IAIObject::STP_BEACON)
			iOwnerGroup = GetAISystem()->GetBeaconGroupId(m_pFormationOwner);

		if(iOwnerGroup == GetGroupId())
		{
			NotifyBreakingFormation();					
//	        m_pFormationOwner->ReleaseFormation();
		}
		else // leaving another leader's formation
		{
			CLeader* pOtherLeader = GetAISystem()->GetLeader(iOwnerGroup);
			if(pOtherLeader)
				NotifyLeavingFormation(pOtherLeader);
			for(TUnitList::iterator itUnit = m_pGroup->GetUnits().begin();itUnit!=m_pGroup->GetUnits().end();++itUnit)
				FreeFormationPoint(itUnit->m_pUnit);
		}

	}
	// force all unit to release their possible formations
	for(TUnitList::iterator itUnit = m_pGroup->GetUnits().begin();itUnit!=m_pGroup->GetUnits().end();++itUnit)
	{
		itUnit->m_FormationPointIndex = -1;
		//itUnit->m_pUnit->ReleaseFormation();
	}
	bool ret=false;
	if(m_pFormationOwner && m_pFormationOwner->m_pFormation)
		ret = m_pFormationOwner->ReleaseFormation();
	m_pFormationOwner = NULL;

	return ret;
}


CAIObject* CLeader::GetNewFormationPoint(CAIObject * pRequester, int iPointType)
{
	int i=0;
	CAIObject* pFormationPoint = NULL;
	if(m_pFormationOwner && m_pFormationOwner->m_pFormation )
	{
		if(iPointType<=0)
		{
			int iFPIndex = GetFormationPointIndex(pRequester);
			if( iFPIndex>=0)
				pFormationPoint = m_pFormationOwner->m_pFormation->GetNewFormationPoint(pRequester->CastToCAIActor(),iFPIndex);
		}
		else
			pFormationPoint = m_pFormationOwner->m_pFormation->GetClosestPoint(pRequester->CastToCAIActor(),true,iPointType);
	}		
	return pFormationPoint;
}


int CLeader::GetFormationPointIndex(const CAIObject * pAIObject)  const
{
	if(pAIObject->GetType() == AIOBJECT_PLAYER)
		return 0;
	const CAIActor* pActor = pAIObject->CastToCAIActor();
	TUnitList::const_iterator itUnit = std::find(m_pGroup->GetUnits().begin(),m_pGroup->GetUnits().end(),pActor);

	if(itUnit!=m_pGroup->GetUnits().end())
		return (*itUnit).m_FormationPointIndex;
			
	return -1;
}


//-----------------------------------------------------------------------------------------------------------
CAIObject * CLeader::GetFormationPointSight(const CPipeUser * pRequester) const
{
	if(m_pFormationOwner)
	{ 
		CFormation* pFormation =m_pFormationOwner->m_pFormation;
		if(pFormation)
			return pFormation->GetFormationDummyTarget((CAIObject *)pRequester);
	}
	return NULL;
}

//-----------------------------------------------------------------------------------------------------------
CAIObject * CLeader::GetFormationPoint(const CAIObject * pRequester) const
{
	if(m_pFormationOwner)
	{ 
		CFormation* pFormation =m_pFormationOwner->m_pFormation;
		if(pFormation)
			return pFormation->GetFormationPoint((CAIObject *)pRequester);
	}
	return NULL;
}


//-----------------------------------------------------------------------------------------------------------
void CLeader::FreeFormationPoint( const CAIObject * pRequester) const
{
	if(m_pFormationOwner && m_pFormationOwner->m_pFormation)
		m_pFormationOwner->m_pFormation->FreeFormationPoint(pRequester);

//	TUnitList::iterator itUnitImg = GetUnitImg(pRequester);
//	if(itUnitImg != m_pGroup->GetUnits().end())
//		(*itUnitImg).ClearFollowing();
}

//-----------------------------------------------------------------------------------------------------------
int CLeader::AssignFormationPoints(bool bIncludeLeader, uint32 unitProp)
{
	std::vector<bool> vPointTaken;
//	m_mapDistFormationPoints.clear();
	TClassUnitMap mapClassUnits;
	
//	for(TUnitList::iterator itUnit=m_pGroup->GetUnits().begin();itUnit!=m_pGroup->GetUnits().end();++itUnit)
//		mapClassUnits.insert(std::pair<int,TUnitList::iterator>((*itUnit).m_SoldierClass,itUnit));
		
	if(!m_pFormationOwner)
		return 0;
	CFormation* pFormation = m_pFormationOwner->m_pFormation;
	if(!pFormation)
		return 0;

	TUnitList::iterator itUnitEnd = m_pGroup->GetUnits().end();
	for(TUnitList::iterator it=m_pGroup->GetUnits().begin();it!=itUnitEnd;++it)
		if(it->m_pUnit == m_pFormationOwner)
			it->m_FormationPointIndex = 0;
		else
			it->m_FormationPointIndex = -1;

	int iFreePoints = pFormation->CountFreePoints();
	int iFormationSize = pFormation->GetSize();
	int iTeamSize = m_pGroup->GetUnitCount(unitProp); //m_pGroup->GetUnits().size();
	if(!m_pGroup->IsMember(m_pFormationOwner))
		iTeamSize++;
	if (iTeamSize >iFreePoints)
		iTeamSize = iFreePoints;
	/*
	TClassUnitMap::iterator itUnitClass = mapClassUnits.begin();
	TClassUnitMap::iterator itUnitEnd = mapClassUnits.end();
	int iFoundPoints = 0;

	// first assign formation points according to units' class
	for(;itUnitClass!=itUnitEnd && iFoundPoints<iFreePoints; ++itUnitClass)
	{
		TUnitList::iterator itUnit = itUnitClass->second;
		if((*itUnit) != m_pFormationOwner) 
		{
			itUnit->m_FormationPointIndex = -1;
			if(AssignFormationPoint(*itUnit,0,false))
				iFoundPoints++;
		}
	}
	// then assign unused points for each remaining unit who didn't found any yet (no matter the class this time)
	// this means that all the undefined class points have been used, and a unit of a given class may find a (different)defined-class point 
	for(itUnitClass = mapClassUnits.begin();itUnitClass!=itUnitEnd && iFoundPoints<iFreePoints; ++itUnitClass)
	{
		TUnitList::iterator itUnit = itUnitClass->second;
		if((*itUnit) != m_pFormationOwner && itUnit->m_FormationPointIndex == -1) 
			if(AssignFormationPoint(*itUnit,0,true))
				iFoundPoints++;
	}
	*/
	int building;
	IVisArea *pArea;
	CAIActor* pOwnerActor = m_pFormationOwner->CastToCAIActor();
	IAISystem::tNavCapMask navMask = (IsPlayer() || !pOwnerActor ? IAISystem::NAVMASK_ALL : pOwnerActor->GetMovementAbility().pathfindingProperties.navCapMask);
	//ownerNavProperties.exposureFactor = 0;
	bool bOwnerInTriangularRegion= (GetAISystem()->CheckNavigationType(m_pFormationOwner->GetPos(),building,pArea,navMask) == IAISystem::NAV_TRIANGULAR);
	TUnitList::iterator itUnit = m_pGroup->GetUnits().begin();

	std::vector<TUnitList::iterator> AssignedUnit;

	//assigned point 0 to owner
	TUnitList::iterator itOwner = std::find(m_pGroup->GetUnits().begin(),m_pGroup->GetUnits().end(),pOwnerActor);
	AssignedUnit.push_back(itOwner); // could be itUnitEnd if the owner is not in the units (i.e.player, beacon)

	for(int i=1;i<iFormationSize;i++)
		AssignedUnit.push_back(itUnitEnd);

	CAIObject* pAILeader = static_cast<CAIObject*>(GetAssociation());

	int iAssignedCount = 0;
	

	for(int k=0;k<3;k++) // this index determines the type of test
	{
		for(int i=1;i<iFormationSize;i++)
		{
			//formation points are sorted by distance to point 0
			//try to assign closest points to owner first, depending on their class
			CAIObject* pPoint = pFormation->GetFormationPoint(i);
			float fMinDist = 987654321.f;
			int index = -1;
			TUnitList::iterator itSelected = itUnitEnd;

			
			for(itUnit = m_pGroup->GetUnits().begin();itUnit!=itUnitEnd ; ++itUnit)
			{
				CUnitImg& unit = (*itUnit);
				if((unit.GetProperties() & unitProp) && unit.m_pUnit != m_pFormationOwner 
					&& unit.m_FormationPointIndex<0 && 
					(k==0 && (unit.GetClass() == pFormation->GetPointClass(i)) || ///first give priority to points that have ONLY the requestor's class
					 k==1 && AssignedUnit[i] == itUnitEnd && (unit.GetClass()& pFormation->GetPointClass(i)) ||// then consider points that have ALSO the requestor's class
					 k==2 && AssignedUnit[i] == itUnitEnd && pFormation->GetPointClass(i) != SPECIAL_FORMATION_POINT)) // finally don't consider the class, excluding SPECIAL_FORMATION_POINT anyway
				{
					if(unit.m_pUnit == pAILeader && !bIncludeLeader) 
						continue;
					Vec3 endPos(pPoint->GetPos());
					if(bOwnerInTriangularRegion)
						endPos.z = GetAISystem()->m_pSystem->GetI3DEngine()->GetTerrainElevation( endPos.x, endPos.y );

					float fDist = Distance::Point_PointSq(pPoint->GetPos() , unit.m_pUnit->GetPos());
/*					if(fDist>50*50) // too far
						continue;
*/
					if(fDist < fMinDist)// && GetAISystem()->IsPathWorth(unit.m_pUnit,unit.m_pUnit->GetPos(),endPos,2))
					{
						fMinDist = fDist;
						itSelected = itUnit;
						index = i;
					}
				}
			}
			if(index>0)
			{
				itSelected->m_FormationPointIndex = index;
				m_pFormationOwner->m_pFormation->GetNewFormationPoint(itSelected->m_pUnit,index);
				iAssignedCount++;;
		//			AssignedUnit.push_back(itSelected);
			}
			else
			{
	//				AssignedUnit.push_back(itUnitEnd);
			}
		}
	}
	// then re-assign the members who has found a single class point, but can find a free multiple class point closer to the owner
	for(itUnit = m_pGroup->GetUnits().begin();itUnit!=itUnitEnd ; ++itUnit)
	{
		CUnitImg& unit = (*itUnit);
		//formation points are sorted by distance to point 0
		if(unit.m_pUnit == pAILeader && !bIncludeLeader) 
			continue;

		float fMinDist = 987654321.f;
		int index = -1;
		for(int i=1;i<itUnit->m_FormationPointIndex;i++)
		{
			CAIObject* pPoint = pFormation->GetFormationPoint(i);
			if((unit.GetProperties() & unitProp) && unit.m_pUnit != m_pFormationOwner 
				&& (unit.GetClass() & pFormation->GetPointClass(i)) && !pFormation->GetPointOwner(i))
			{
				int currentIndex = itUnit->m_FormationPointIndex;
				//AssignedUnit[currentIndex] = itUnitEnd;
				//AssignedUnit[i] = itUnit;
				pFormation->FreeFormationPoint(unit.m_pUnit);
				pFormation->GetNewFormationPoint(unit.m_pUnit,i);
				itUnit->m_FormationPointIndex = i;
			}
		}
	}

	return iAssignedCount;
}

bool CLeader::AssignFormationPoint(CUnitImg& unit, int teamSize, bool bAnyClass, bool bClosestToOwner )
{
	CFormation* pFormation;

	int myPointIndex = -1;
	if(m_pFormationOwner && (pFormation = m_pFormationOwner->m_pFormation))
	{
		if(bAnyClass)
			myPointIndex = pFormation->GetClosestPointIndex(unit.m_pUnit,true, teamSize, -1, bClosestToOwner);
		else
		{
			int myClass = unit.m_SoldierClass;
			if (myClass)
			{
				myPointIndex = pFormation->GetClosestPointIndex(unit.m_pUnit,true, teamSize, myClass,bClosestToOwner );
			}
			if(myPointIndex<0) // if your class is not found, try searching an undefined class point
			{
				myPointIndex = pFormation->GetClosestPointIndex(unit.m_pUnit,true,teamSize, UNIT_CLASS_UNDEFINED,bClosestToOwner);
			}
		}
	}

	if(myPointIndex>=0)
	{
		unit.m_FormationPointIndex = myPointIndex;
		return true;
	}

	return false;
}

void CLeader::AssignFormationPointIndex(CUnitImg& unit, int index )
{
	unit.m_FormationPointIndex = index;
	CFormation* pFormation;
	if(m_pFormationOwner && (pFormation = m_pFormationOwner->m_pFormation))
	{
		pFormation->FreeFormationPoint(unit.m_pUnit);
		pFormation->GetNewFormationPoint(unit.m_pUnit,index);
	}
}

void CLeader::EnableUse(int action)
{
	TUsableObjectList::iterator itU = std::find(m_UsableObjects.begin(),m_UsableObjects.end(),action);
	if(itU!= m_UsableObjects.end())
		m_UsableObjects.erase(itU);
	m_bUseEnabled = true;
	m_UsableObjects.push_front(action);
}


void CLeader::DisableUse(int action)
{
	if(!m_UsableObjects.empty())
	{
		TUsableObjectList::iterator itU; 
		if (action<0)
			// if empty parameter, disable use of the top object in list, no matter the name
			itU = m_UsableObjects.begin();
		else
			itU = std::find(m_UsableObjects.begin(),m_UsableObjects.end(),action);
		if(itU !=m_UsableObjects.end() )
			m_UsableObjects.erase(itU);
	}
	if(m_UsableObjects.empty())
		m_bUseEnabled = false;

}

bool CLeader::IsUsable(int action) const
{
	// only the top object in the list (stack) can be used
	if(!m_UsableObjects.empty())
	{
		if(*(m_UsableObjects.begin())==action)
			return true;
	}

	return false;
}


/*
// singleton CharacterClass class implementation - done if we want one-on-one association 
// between AI characters and Soldier classes

CharacterClass* CharacterClass::m_spInstance = 0;
CharacterClass::CharacterClass()
{
	m_CharacterClassMap.insert(std::pair<string,eSoldierClass>("Cover",UNIT_CLASS_COVER));
	m_CharacterClassMap.insert(std::pair<string,eSoldierClass>("Scout",UNIT_CLASS_SCOUT));
	m_CharacterClassMap.insert(std::pair<string,eSoldierClass>("TLDefense",UNIT_CLASS_LEADER));
	m_CharacterClassMap.insert(std::pair<string,eSoldierClass>("Engineer",UNIT_CLASS_ENGINEER));
	m_CharacterClassMap.insert(std::pair<string,eSoldierClass>("Medic",UNIT_CLASS_MEDIC));
}
*/

//
//--------------------------------------------------------------------------------------------------------------
void CLeader::ChangeStance(EStance stance)
{
	string signal="";
	switch(stance)
	{
	case STANCE_STAND:
		signal="LEADER_STAND";
		break;
	case STANCE_CROUCH:
		signal="LEADER_CROUCH";
		break;
	case STANCE_PRONE:
		signal="LEADER_PRONE";
		break;
	default:
		break;
	}
	if(m_Stance != stance)
	{
		m_Stance = stance;
		CAIObject* pAILeader = static_cast<CAIObject*>(GetAssociation());
		if(pAILeader)
			GetAISystem()->SendSignal( SIGNALFILTER_GROUPONLY,0,signal.c_str(),pAILeader);
	}
}

//
//--------------------------------------------------------------------------------------------------------------
void	CLeader::SetFollowState(EFollowState state)
{
	m_LastFollowState =  m_FollowState ; 
	m_FollowState = state;
}

/*
// Alert Spot list managing
void CLeader::AddAlertSpot(Vec3& spot, CAIObject* pAITarget)
{
	CTimeValue fCurrentTime =  GetAISystem()->GetFrameStartTime();

//	for( TAlertSpotMap::iterator it = m_AlertSpots.begin(); it!=m_AlertSpots.end(); )
//		if((it->second.position - spot).GetLengthSquared()<9)
//		{
//			//update the existent - close enough alert spot
//			m_AlertSpots.erase(it++);
//		}
//		else
//			++it;
	m_AlertSpots.insert(std::pair<float,TTarget>(fCurrentTime,TTarget(spot,pAITarget)));
	return;
}


void CLeader::UpdateAlertSpotList()
{
	CTimeValue fCurrentTime =  GetAISystem()->GetFrameStartTime();
	for( TAlertSpotMap::iterator it = m_AlertSpots.begin(); it!=m_AlertSpots.end(); )
		if(fCurrentTime - it->first >10000.f)
			m_AlertSpots.erase(it++);
		else
			++it;
	
	// to do: consider target AIObjects, last time spotted and compute the average threat position
}
*/
//
//--------------------------------------------------------------------------------------------------------------
CLeaderAction* CLeader::CreateAction(const LeaderActionParams* params, const char* signalText) 
{
	CLeaderAction* pAction(NULL);
	
	switch(params->type)	
	{
	case LA_ATTACK:
		switch(params->subType)
		{
			case LAS_ATTACK_ROW:
			case LAS_ATTACK_COORDINATED_FIRE2:
				pAction = new CLeaderAction_Attack_Row(this,*params);
				break;
			case LAS_ATTACK_FRONT:
				pAction = new CLeaderAction_Attack_Front(this,*params);
				break;
			case LAS_ATTACK_FOLLOW_LEADER:
				pAction = new CLeaderAction_Attack_FollowLeader(this,*params);
				break;
			case LAS_ATTACK_FLANK:
				pAction = new CLeaderAction_Attack_Flank(this,params->fDuration, false);
				break;
			case LAS_ATTACK_FLANK_HIDE:
				pAction = new CLeaderAction_Attack_Flank(this,params->fDuration,true);
				break;
			case LAS_ATTACK_CHAIN:
				pAction = new CLeaderAction_Attack_Chain(this);
				break;
			case LAS_ATTACK_LEAPFROG:
				pAction = new CLeaderAction_Attack_LeapFrog(this,*params);
				break;
			case LAS_ATTACK_HIDE_COVER:
				pAction = new CLeaderAction_Attack_HideAndCover(this,*params);
				break;
			case LAS_ATTACK_COORDINATED_FIRE1:
				pAction = new CLeaderAction_Attack_CoordinatedFire(this, *params);
				break;
			case LAS_ATTACK_USE_SPOTS:
				pAction = new CLeaderAction_Attack_UseSpots(this, *params);
				break;
			case LAS_ATTACK_SWITCH_POSITIONS:
				pAction = new CLeaderAction_Attack_SwitchPositions(this, *params);
				break;
			case LAS_ATTACK_CHASE:
				pAction = new CLeaderAction_Attack_Chase(this, *params);
				break;
				/*		case LA_ATTACK_CIRCLE:
				m_pCurrentAction = new CLeaderAction_Attack_Circle(this);
				break;
				case LA_ATTACK_FUNNEL:
				m_pCurrentAction = new CLeaderAction_Attack_Funnel(this);
				break;
				*/
			case LAS_DEFAULT:
			default:
				pAction = new CLeaderAction_Attack(this);
				break;
		}
		break;
	case LA_FOLLOW:
		pAction = new CLeaderAction_Follow(this,*params);
		break;
	case LA_USE:
		pAction = new CLeaderAction_Use(this);
		break;
	case LA_USE_VEHICLE:
		pAction = new CLeaderAction_Use_Vehicle(this, AISignalExtraData());
		break;
	case LA_SEARCH:
		pAction = new CLeaderAction_Search(this,*params);
		break;
	default:
		AIAssert(!"Action type not added");
	}
	if(signalText)
	{
		CAIObject* pAILeader = (CAIObject*) GetAssociation();
		if(pAILeader)
			GetAISystem()->SendSignal(SIGNALFILTER_SENDER, 1, signalText, pAILeader);
	}

	if(pAction)
		m_vMarkedPosition.Set(0,0,0);

	return pAction;
}


//
//--------------------------------------------------------------------------------------------------------------
void CLeader::PopulateObjectTracker(CObjectTracker& objectTracker)
{
	for(TUnitList::iterator itrUnit(m_pGroup->GetUnits().begin()); itrUnit!=m_pGroup->GetUnits().end(); ++itrUnit )
	{
		CUnitImg &unit((*itrUnit));
		for(TActionList::iterator itAction(unit.m_Plan.begin()); itAction!=unit.m_Plan.end(); ++itAction)
			objectTracker.AddObject((*itAction), false);
	}
}

//
//--------------------------------------------------------------------------------------------------------------
void CLeader::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{

	char name[32];
	sprintf(name,"AILeader-%d",GetGroupId());
	ser.BeginGroup(name);
	{
		CAIActor::Serialize(ser,objectTracker);

		m_pObstacleAllocator->Serialize(ser, objectTracker);

		//ser.Value("m_targetPoint", m_targetPoint);
	//	ser.Value("m_mapDistFormationPoints", m_mapDistFormationPoints);
		objectTracker.SerializeObjectPointer(ser, "m_pFormationOwner", m_pFormationOwner, false);
		objectTracker.SerializeObjectPointer(ser, "m_pPrevFormationOwner", m_pPrevFormationOwner, false);
		objectTracker.SerializeObjectPointerContainer(ser, "m_FormationJoiners", m_FormationJoiners, false);
		objectTracker.SerializeValueContainer(ser,"m_UsableObjects", m_UsableObjects);
		ser.Value("m_szFormationDescriptor", m_szFormationDescriptor);
		ser.EnumValue("m_Stance", m_Stance, STANCE_NULL, STANCE_LAST);
		ser.EnumValue("m_FollowState", m_FollowState, FS_NONE, FS_LAST);
		ser.EnumValue("m_LastFollowState", m_LastFollowState, FS_NONE, FS_LAST);
		ser.Value("m_iFormOwnerGroupID",	m_iFormOwnerGroupID);
		ser.Value("m_bUseEnabled", m_bUseEnabled);
		ser.Value("m_bAllowFire",	m_bAllowFire);
		ser.Value("m_fAllowingFireTime",	m_fAllowingFireTime);

		ser.EnumValue("m_alertStatus",	m_alertStatus,AIALERTSTATUS_SAFE,AIALERTSTATUS_ACTION);
		ser.Value("m_vForcedPreferredPos",m_vForcedPreferredPos);
		ser.Value("m_currentAreaSpecies",m_currentAreaSpecies);
		ser.Value("m_bForceSafeArea",	m_bForceSafeArea);
		objectTracker.SerializeObjectPointer(ser, "m_pEnemyTarget", m_pEnemyTarget, false);
		ser.Value("m_bUpdateAlertStatus",	m_bUpdateAlertStatus);
		ser.Value("m_AlertStatusReadyRange",m_AlertStatusReadyRange);
		ser.Value("m_vMarkedPosition",m_vMarkedPosition);
		ser.Value("m_bLeaderAlive",m_bLeaderAlive);
		ser.Value("m_bKeepEnabled",m_bKeepEnabled);
		//objectTracker.SerializeObjectPointer(ser,"m_pGroup",m_pGroup,false);
		/*if(ser.IsReading())
		{
			m_pGroup = GetAISystem()->GetAIGroup(m_Parameters.m_nGroup);
			if(m_pGroup)
				m_pGroup->SetLeader(this);
		}*/

		ser.BeginGroup("m_pCurrentAction");
		{
			ELeaderAction actionType = ser.IsWriting() ? (m_pCurrentAction? m_pCurrentAction->GetType() : LA_NONE):LA_NONE;
			ELeaderActionSubType subType = ser.IsWriting() ? (m_pCurrentAction? m_pCurrentAction->GetSubType(): LAS_DEFAULT) :LAS_DEFAULT;
			ser.EnumValue("ActionType",actionType,LA_NONE,LA_LAST);
			ser.EnumValue("ActionSubType",subType,LAS_DEFAULT,LAS_LAST);
			if(ser.IsReading())
			{
				AbortExecution();//clears current action
				if(actionType!=LA_NONE)
				{
					LeaderActionParams	params(actionType ,subType);
					m_pCurrentAction = CreateAction(&params);
				}
			}

			if (m_pCurrentAction)
				m_pCurrentAction->Serialize(ser, objectTracker);

			ser.EndGroup();
		}

	/*
			objectTracker.SerializeObjectContainer(ser, "m_pGroup->GetUnits()", m_pGroup->GetUnits());

		// reset ranks - m_RankedUnits is not getting serialized 
		if(ser.IsReading())
		{
			for(TUnitList::iterator itrUnit(m_pGroup->GetUnits().begin()); itrUnit!=m_pGroup->GetUnits().end(); ++itrUnit )
				m_pGroup->SetUnitRank((*itrUnit).m_pUnit);
			m_bUpdateEnemyStats = true;
		}
	*/

		// Attack requests
	//	objectTracker.SerializeObjectContainer(ser, "m_AttackRequestMap", m_AttackRequestMap);
		int iAttReqMapSize;
		
		if(ser.IsReading())
			ClearAllAttackRequests();
		else
			iAttReqMapSize = m_AttackRequestMap.size();
		ser.Value("AttackRequestMap_size",iAttReqMapSize);

		TAttackRequestMap::iterator it = m_AttackRequestMap.begin();

		for(int i=0;i<iAttReqMapSize;i++)
		{
			char buffer[32];
			uint32 iUnitProp;
			ELeaderActionSubType actionType;
			float duration;
			Vec3 defensePoint;
			CAttackRequest* pAR;

			sprintf(buffer,"AttackRequest_%d",i);
			ser.BeginGroup(buffer);
			if(ser.IsWriting())
			{
				iUnitProp = it->first;
				pAR = it->second;
				duration = pAR->m_fDuration;
				defensePoint = pAR->m_vDefensePoint;
			}
			ser.Value("UnitProp",iUnitProp);
			ser.EnumValue("ActionType",actionType,LAS_DEFAULT,LAS_LAST);
			ser.Value("Duration",duration);
			ser.Value("DefensePoint",defensePoint);
			if(ser.IsReading())
				pAR = CreateAttackRequest(iUnitProp,actionType,duration,defensePoint);
			else
				pAR = it->second;
			if(pAR)
			{
				pAR->Serialize(ser,objectTracker,this);
				if(ser.IsReading())
					m_AttackRequestMap.insert(std::make_pair(iUnitProp,pAR));
			}
			ser.EndGroup();
			++it;
		}

		int iSpotListSize;
		if(ser.IsReading())
			m_SpotList.clear();
		else
			iSpotListSize = m_SpotList.size();
		ser.Value("SpotList_size",iSpotListSize);

		TSpotList::iterator itS = m_SpotList.begin();
		for(int i=0;i<iSpotListSize;i++)
		{
			char buffer[32];
			Vec3 point;
	#if defined(PS3)
			uint64 n;
	#else
			INT_PTR n;
	#endif
			sprintf(buffer,"Spot_%d",i);
			ser.BeginGroup(buffer);
			if(ser.IsWriting())
			{
				n = itS->first;
				point = itS->second;
			}
			ser.Value("SpotID",n);
			ser.Value("SpotPoint",point);
			if(ser.IsReading())
				m_SpotList.insert(std::make_pair(n,point));
			ser.EndGroup();
			++itS;
		}
		
		ser.EndGroup(/*AILeader*/);
	}
}

//
//--------------------------------------------------------------------------------------------------------------
void CLeader::SetAlertStatus(EAIAlertStatus status) 
{ 
	if( status != m_alertStatus)
	{
		if(status>AIALERTSTATUS_SAFE)//
		{
			if(status <= AIALERTSTATUS_UNSAFE && (m_pEnemyTarget && !m_bForceSafeArea))
				return;
			if(status >= AIALERTSTATUS_READY && (!m_pEnemyTarget || m_bForceSafeArea))
				return;
		}
		m_alertStatus = status;
		if(m_pCurrentAction && m_pCurrentAction->GetType()==LA_FOLLOW )
		{
			AbortExecution(m_pCurrentAction->GetPriority());
			Follow();
		}
	}
};
//
//------------------------------------------------------------------------------------------------------------
void CLeader::UpdateAlertStatus()
{
	IAIObject* pBeacon = GetAISystem()->GetBeacon(GetGroupId());
	CAIObject* pAILeader = (CAIObject*) GetAssociation();

	if(m_alertStatus==AIALERTSTATUS_READY && pBeacon && pAILeader && Distance::Point_PointSq(pAILeader->GetPos(), pBeacon->GetPos())<m_AlertStatusReadyRange*m_AlertStatusReadyRange)
		return; // keep alert status to ready if enemy is close enough, no matter the attention target
	m_pEnemyTarget = m_pGroup->GetAttentionTarget(true);
	if(m_pEnemyTarget && !m_bForceSafeArea)
		SetAlertStatus(AIALERTSTATUS_READY);
	else
		SetAlertStatus(m_currentAreaSpecies==GetParameters().m_nSpecies ? AIALERTSTATUS_SAFE:AIALERTSTATUS_UNSAFE);
}
//
//------------------------------------------------------------------------------------------------------------
string CLeader::GetFormationName(char* type,int size)
{
	string name = "_leader_"+string(type);
	char c[5];
	itoa(size, c, 10);
	name.append(c);
	return name;
}
//
//------------------------------------------------------------------------------------------------------------
bool CLeader::CreateFormation_Row(const Vec3& pos, float scale, int size,bool bIncludeLeader, const CAIObject* pReferenceTarget,uint32 unitProp)
{
	FormationNode node;
	if(size==0)
		size = m_pGroup->GetUnitCount(unitProp);
	if(size==0)
		return false;

	string name = GetFormationName("Row",size);
	
	
	if(!GetAISystem()->IsFormationDescriptorExistent(name.c_str()))
	{
		GetAISystem()->CreateFormationDescriptor(name);
		GetAISystem()->AddFormationPoint(name,node);
		int s = 1;
		for(int i=2;i<=size;i++)
		{
			node.vOffset.Set(s*i/2,0,0);
//			node.fFollowDistance = node.fFollowDistanceAlternate = 0.11f; 
//			node.fFollowOffset = node.fFollowOffsetAlternate = s*i/2;
			node.vSightDirection.Set(0,60,0);
			GetAISystem()->AddFormationPoint(name,node);
			s = -s;
		}
	}

	if(LeaderCreateFormation(name,pos,bIncludeLeader, unitProp))
	{

		CPuppet* pPuppetOwner = m_pFormationOwner->CastToCPuppet();
		if(pPuppetOwner)
		{
			if(scale != 1)
				m_pFormationOwner->m_pFormation->SetScale(scale);
			m_pFormationOwner->m_pFormation->SetReferenceTarget(pReferenceTarget);
			return true;
		}
		//else
		//	ReleaseFormation();
	}
	return false;
}

//
//------------------------------------------------------------------------------------------------------------
bool CLeader::CreateFormation_LineFollow(const Vec3& pos, float scale, int size,bool bIncludeLeader, CAIObject* pOwner, CAIObject* pReferenceTarget,uint32 unitProp)
{
	FormationNode node;
	if(size==0)
		size = m_pGroup->GetUnits().size();
	string name = "_leader_LineFollow";
	char c[5];
	itoa(size, c, 10);
	name.append(c);

	if(!GetAISystem()->IsFormationDescriptorExistent(name.c_str()))
	{
		GetAISystem()->CreateFormationDescriptor(name);
		GetAISystem()->AddFormationPoint(name,node);
		float followDist = 0;
		for(int i=1;i<size;i++)
		{
			followDist+=scale;
			node.fFollowDistance = followDist;
			node.fFollowDistanceAlternate = followDist;
			GetAISystem()->AddFormationPoint(name,node);
		}
	}

	if(LeaderCreateFormation(name,pos,bIncludeLeader, unitProp,pOwner))
	{

		CPuppet* pPuppetOwner = m_pFormationOwner->CastToCPuppet();
		if (pPuppetOwner)
		{
			if(scale != 1)
				m_pFormationOwner->m_pFormation->SetScale(scale);
			m_pFormationOwner->m_pFormation->SetReferenceTarget(pReferenceTarget);
			return true;
		}
		//else
		//	ReleaseFormation();
	}
	return false;
}


//
//------------------------------------------------------------------------------------------------------------
bool CLeader::CreateFormation_FrontRandom(const Vec3& pos, float scale, int size,bool bIncludeLeader, CAIObject* pReferenceTarget,uint32 unitProp, CAIObject* pOwner)
{
	FormationNode node;
	if(size==0)
		size = m_pGroup->GetUnits().size();
	string name = "_leader_FrontRandom";
	char c[5];
	itoa(size, c, 10);
	name.append(c);
	if(pOwner)
	{
		name.append("_o");
		if(!m_pGroup->IsMember(pOwner))
			size++;
	};

	std::vector<Vec3> Offsets;
	Vec3 vOwnerOffset(0,5,0);
	Offsets.push_back(Vec3(0,0,0));
	if(pOwner)
		Offsets.push_back(Vec3(0,0,0));
	Offsets.push_back(Vec3(1.5f,1.f,0));
	Offsets.push_back(Vec3(-1.5f,1.5f,0));
	Offsets.push_back(Vec3(3.f,1.7f,0));
	Offsets.push_back(Vec3(-.3f,2.f,0));
	Offsets.push_back(Vec3(4.f,3,0));
	Offsets.push_back(Vec3(-4.f,3.5f,0));
	Offsets.push_back(Vec3(5.f,3.8f,0));
	Offsets.push_back(Vec3(-5.f,4.5,0));
	Offsets.push_back(Vec3(6.f,5,0));
	if(!GetAISystem()->IsFormationDescriptorExistent(name.c_str()))
	{
		Vec3 vOffset(0,0,0);
		GetAISystem()->CreateFormationDescriptor(name);
		GetAISystem()->AddFormationPoint(name,node);
		for(int i=1;i<=size;i++)
		{
			vOffset = Offsets[i];
			if(pOwner)
				vOffset+= vOwnerOffset;
			node.vOffset = vOffset;
			node.vSightDirection.Set(0,0,0);
			GetAISystem()->AddFormationPoint(name,node);
		}
	}
	if(LeaderCreateFormation(name,pos,bIncludeLeader, unitProp,pOwner))
	{
		if(scale != 1)
			m_pFormationOwner->m_pFormation->SetScale(scale);
		m_pFormationOwner->m_pFormation->SetReferenceTarget(pReferenceTarget);
		return true;
	}
	return false;
}

//
//------------------------------------------------------------------------------------------------------------
bool CLeader::CreateFormation_Saw(const Vec3& pos, float scale, int size)
{
	FormationNode node;
	if(size==0)
		size = m_pGroup->GetUnits().size();
	string name = "_leader_Saw";
	char c[5];
	itoa(size, c, 10);
	name.append(c);

	if(!GetAISystem()->IsFormationDescriptorExistent(name.c_str()))
	{
		GetAISystem()->CreateFormationDescriptor(name);
		GetAISystem()->AddFormationPoint(name,node);
		int s = 1;
		for(int i=2;i<=size;i++)
		{
			node.vOffset.Set(s*i/2,float(i&2)/4,0);
			GetAISystem()->AddFormationPoint(name,node);
			s = -s;
		}
	}

	if(LeaderCreateFormation(name,pos))
	{
		CPuppet* pPuppetOwner = m_pFormationOwner->CastToCPuppet();
		if (pPuppetOwner)
		{
			m_pFormationOwner->m_pFormation->SetScale(scale);
			m_pFormationOwner->m_pFormation->SetUpdate(false);
			return true;
		}
		else
			ReleaseFormation();
	}
	return false;
}

//
//------------------------------------------------------------------------------------------------------------
bool CLeader::CreateFormation_Circle(float scale,int size )
{
	FormationNode node;
	if(size==0)
		size = m_pGroup->GetUnits().size();
	string name = "_leader_Circle"+size;
	if(!GetAISystem()->IsFormationDescriptorExistent(name.c_str()))
	{
		GetAISystem()->CreateFormationDescriptor(name);
		GetAISystem()->AddFormationPoint(name,node);
		float step = gf_PI*2/float(size-1);
		float angle = 0;
		for(int i=1;i<size;i++)
		{
			node.vOffset.Set(cosf(angle),sinf(angle),0);
			GetAISystem()->AddFormationPoint(name,node);
			angle += step;
		}
	}

	if(LeaderCreateFormation(name,m_pGroup->GetEnemyPositionKnown()))
	{
		m_pFormationOwner->m_pFormation->SetScale(scale);
		return true;
	}
	return false;
	
}

//
//------------------------------------------------------------------------------------------------------------
bool CLeader::CreateFormation_Funnel(float scale, int size)
{
	FormationNode node;
	if(size==0)
		size = m_pGroup->GetUnits().size();
	string name = "_leader_Funnel"+size;
	if(!GetAISystem()->IsFormationDescriptorExistent(name.c_str()))
	{
		GetAISystem()->CreateFormationDescriptor(name);
		GetAISystem()->AddFormationPoint(name,node);
		// TO DO
	}

	if(LeaderCreateFormation(name,m_pGroup->GetEnemyPositionKnown()))
	{
		m_pFormationOwner->m_pFormation->SetScale(scale);
		return true;
	}
	return false;
}

//
//------------------------------------------------------------------------------------------------------------
bool CLeader::CreateFormation_HorizWedge(const Vec3& pos, float scale, int size )
{
	// Check if automatic formation size was requested.
	if( size == 0 )
		size = m_pGroup->GetUnits().size();		// Create formation for all available units.

	// Sanity check.
	if( size == 0 )
		return false;

	// Check if the formation descriptor of requested size exists.
	string name = "_leader_HorizWedge" + size;
	if( !GetAISystem()->IsFormationDescriptorExistent( name.c_str() ) )
	{
		// The formation descriptor does not exists, create it.
		GetAISystem()->CreateFormationDescriptor( name );

		FormationNode node;

		// Add the formation owner point (the node was reset correctly in the constructor).
		GetAISystem()->AddFormationPoint( name, node );
		size--;

		// Create the wedge points (90 degrees). 
		//          .
		// n        :        n+1
		//    3     :     4
		//       1  :  2
		//          x
		int	halfSize = (size + 1) / 2;

		for( int i = 0; i < halfSize; i++ )
		{
			// Create two points at a time, one left and one right.
			const float offset = (float)(i + 1) / (float)halfSize;

			node.fFollowDistance = node.fFollowDistanceAlternate = offset; 

			// Odd points go the left side.
			node.fFollowOffset = node.fFollowOffsetAlternate = offset;
			GetAISystem()->AddFormationPoint( name, node );

			// Even points go the right side.
			// If there is even number of points in the formation the last last wedge row will have only one point.
			if( i * 2 + 1 < size )
			{
				node.fFollowOffset = node.fFollowOffsetAlternate = -offset;
				GetAISystem()->AddFormationPoint( name, node );
			}
		}
	}

	// Create the formation
	if( LeaderCreateFormation( name, m_pGroup->GetEnemyPositionKnown() ) )
	{
		// Scale the formation based on the creation parameters.
		m_pFormationOwner->m_pFormation->SetScale( scale );
		return true;
	}

	// The formation could not be created. Could be group size of zero or could not find formation leader.
	return false;
}

//
//------------------------------------------------------------------------------------------------------------
bool CLeader::CreateFormation_VertWedge(const Vec3& pos, float scale, int size )
{
	// Check if automatic formation size was requested.
	if( size == 0 )
		size = m_pGroup->GetUnits().size();		// Create formation for all available units.

	// Sanity check.
	if( size == 0 )
		return false;

	// Check if the formation descriptor of requested size exists.
	string name = "_leader_VertWedge" + size;
	if( !GetAISystem()->IsFormationDescriptorExistent( name.c_str() ) )
	{
		// The formation descriptor does not exists, create it.
		GetAISystem()->CreateFormationDescriptor( name );

		FormationNode node;

		// Add the formation owner point (the node was reset correctly in the constructor).
		GetAISystem()->AddFormationPoint( name, node );
		size--;

		// Create the wedge points (90 degrees). 
		//          .
		// n        :        n+1
		//    3     :     4
		//       1  :  2
		//          x
		int	halfSize = (size + 1) / 2;

		for( int i = 0; i < halfSize; i++ )
		{
			// Create two points at a time, one up and one down.
			const float offset = (float)(i + 1) / (float)halfSize;

			node.fFollowDistance = node.fFollowDistanceAlternate = offset; 

			// Odd points go the down side.
			node.fFollowHeightOffset = offset;
			GetAISystem()->AddFormationPoint( name, node );

			// Even points go the up side.
			// If there is even number of points in the formation the last last wedge row will have only one point.
			if( i * 2 + 1 < size )
			{
				node.fFollowHeightOffset = -offset;
				GetAISystem()->AddFormationPoint( name, node );
			}
		}
	}

	// Create the formation
	if( LeaderCreateFormation( name, m_pGroup->GetEnemyPositionKnown() ) )
	{
		// Scale the formation based on the creation parameters.
		m_pFormationOwner->m_pFormation->SetScale( scale );
		return true;
	}

	// The formation could not be created. Could be group size of zero or could not find formation leader.
	return false;
}

//
//------------------------------------------------------------------------------------------------------------
void CLeader::AddShootSpot(INT_PTR n,  Vec3 pos)
{
	m_SpotList.insert(std::make_pair(n,pos));
}

//
//------------------------------------------------------------------------------------------------------------
void CLeader::RemoveShootSpot(INT_PTR n)
{
	TSpotList::iterator it = m_SpotList.find(n);
	if(it!=m_SpotList.end())
		m_SpotList.erase(it);
}

//
//------------------------------------------------------------------------------------------------------------
Vec3 CLeader::GetBestShootSpot(const Vec3& position) const
{
	float minDist2 = 10000;
	Vec3 bestPos(ZERO);
	for(TSpotList::const_iterator it = m_SpotList.begin(); it!=m_SpotList.end(); ++it)
	{
		float dist2= (it->second - position).GetLengthSquared();
		if(dist2 < minDist2)
		{
			minDist2 = dist2;
			bestPos = it->second;
		}
	}
	return bestPos;
}

//
//------------------------------------------------------------------------------------------------------------
void CLeader::RequestAttack(uint32 unitProp, int actionType, float duration, const Vec3& defensePoint)
{
	if(actionType && unitProp)
	{
		//		ClearAttackRequest(unitProp);
		TAttackRequestMap::iterator it = m_AttackRequestMap.find(unitProp) ;
		if(it != m_AttackRequestMap.end())
		{
			delete it->second;
			m_AttackRequestMap.erase(it);
		}
		CAttackRequest* pAttackRequest = CreateAttackRequest(unitProp,ELeaderActionSubType(actionType),duration, defensePoint);
		if(pAttackRequest)
			m_AttackRequestMap.insert(std::pair<uint32,CAttackRequest*>(unitProp,pAttackRequest));
	}
}

//
//------------------------------------------------------------------------------------------------------------
CAttackRequest* CLeader::CreateAttackRequest(uint32 unitProp, ELeaderActionSubType actionType, float duration, const Vec3& defensePoint)
{
	CAttackRequest* pAttackRequest = NULL;
	switch(actionType)
	{
	case LAS_ATTACK_ROW:
		pAttackRequest = new CAttackRequest_Row(this,unitProp,(ELeaderActionSubType) actionType,duration,defensePoint);
		break;
	case LAS_ATTACK_LEAPFROG:
		pAttackRequest = new CAttackRequest_LeapFrog(this,unitProp,(ELeaderActionSubType) actionType,duration,defensePoint);
		break;
	case LAS_ATTACK_FLANK:
		pAttackRequest = new CAttackRequest_Flank(this,unitProp,(ELeaderActionSubType) actionType,duration,defensePoint);
		break;
	case LAS_ATTACK_FLANK_HIDE:
		pAttackRequest = new CAttackRequest_Flank(this,unitProp,(ELeaderActionSubType) actionType,duration,defensePoint);
		break;
	default:
		AIWarning("Wrong Attack Request type: %d",actionType);
		break;
	}
	return pAttackRequest;
}

//
//------------------------------------------------------------------------------------------------------------
void CLeader::ClearAllAttackRequests()
{
	for(TAttackRequestMap::iterator it = m_AttackRequestMap.begin(); it!= m_AttackRequestMap.end();++it)
		delete it->second;
	m_AttackRequestMap.clear();
}

//
//------------------------------------------------------------------------------------------------------------
void CLeader::UpdateAttackRequests()
{
	if(!m_AttackRequestMap.empty())
	{
		TAttackRequestMap::iterator it = m_AttackRequestMap.begin();
		CAttackRequest* pRequest = it->second;
		if(pRequest->Update())
		{
			delete pRequest;
			m_AttackRequestMap.erase(it);
		}
	}
}


//
//------------------------------------------------------------------------------------------------------------
// ATTACK REQUESTS
//------------------------------------------------------------------------------------------------------------

void CAttackRequest::EndRequest(const char* signalText)
{
	m_bUpdate = false;
	CAIObject* pAILeader = (CAIObject*) m_pLeader->GetAssociation();
	if(pAILeader)
	{
		AISignalExtraData* pData = new AISignalExtraData;
		pData->iValue = LA_ATTACK;
		pData->iValue2 = m_actionType;
		pData->point = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();
		IAIObject* pAITarget = m_pLeader->GetAIGroup()->GetAttentionTarget(true);
		if(pAITarget)
		{
			pData->SetObjectName(pAITarget->GetName());
			unsigned short nType=((CAIObject*)pAITarget)->GetType();
			if(nType==AIOBJECT_PLAYER ||nType==AIOBJECT_PUPPET || nType==AIOBJECT_VEHICLE)
			{
				IEntity* pEntity = (IEntity*)(pAITarget->GetEntity());
				if(pEntity)
					pData->nID = pEntity->GetId();
			}
		}
		GetAISystem()->SendSignal(SIGNALFILTER_SENDER, 0, signalText, pAILeader, pData);
	}
}


bool CAttackRequest_LeapFrog::Update()
{
	LeaderActionParams params;
	params.type = LA_ATTACK;
	params.subType = LAS_ATTACK_LEAPFROG;
	m_pLeader->Attack(&params);
	EndRequest("OnAttackLeapFrog");
	m_bUpdate = false;
	return true;
}


bool CAttackRequest_Row::Update()
{
	bool bFailed = false;
	Vec3 vBasePos, vEnemyPos, vEnemyAveragePos;

	bool bDefend = !m_vDefensePoint.IsZero();
	if(bDefend)
		vBasePos = m_vDefensePoint;
	else
		vBasePos =  m_pLeader->GetAIGroup()->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,m_iUnitProp);
	
	vEnemyAveragePos = m_pLeader->GetAIGroup()->GetEnemyAveragePosition();

	if(vEnemyAveragePos.IsZero())
	{
		IAIObject* pBeacon = GetAISystem()->GetBeacon(m_pLeader->GetGroupId());
		if(pBeacon)
			vEnemyAveragePos = pBeacon->GetPos();
		else
		{
			EndRequest("OnLeaderActionFailed");
			return true;
		}
	}

  Vec3 direction = vBasePos - vEnemyAveragePos;
	vEnemyPos = m_pLeader->GetAIGroup()->GetForemostEnemyPosition(direction);
	if(vEnemyPos.IsZero())
		vEnemyPos = vEnemyAveragePos;
	// TO DO: improve with enemy total area instead of average position
//	if(fDist>7)
	{
		if(m_bInit)
		{
			m_bInit = false;
			m_iMaxUpdates = 5;
			m_vCurrentDir = vEnemyPos - vBasePos;
			m_vCurrentDir.z = 0; //2D only
			m_vCurrentDir.NormalizeSafe();

			if(m_vCurrentDir.IsZero())
			{
				EndRequest("OnLeaderActionFailed");
				return true;
			}
			m_fMultiplier = 0;
			m_iCount1 = 0;
		}
		// check for attack row
		Vec3 vCurrentDir = m_vCurrentDir;
		ray_hit hit;
		IPhysicalWorld *pWorld = GetAISystem()->GetPhysicalWorld();
		
		int	unitcount = m_pLeader->GetAIGroup()->GetUnitCount(UPR_COMBAT_GROUND);
		if(unitcount==0)
		{
			EndRequest("OnLeaderActionFailed");
			return true;
		}
		else if(unitcount==1)
		{
			LeaderActionParams params;
			params.vPoint = vBasePos;
			params.vPoint2 = m_vDefensePoint;
			params.type = LA_ATTACK;
			params.subType = LAS_ATTACK_ROW;
			params.fSize = 1;
			params.fDuration = m_fDuration;
			m_pLeader->Attack(&params);
			EndRequest("OnAttackRow");
			m_bUpdate = false;
			return true;
		}

		float fInitDistance;
		if(bDefend )
		{
			Vec3 vDistEnemyDefensePoint = m_vDefensePoint - vEnemyPos;
			float fDefenseDistanceFromEnemy = vDistEnemyDefensePoint.GetLength();
			fInitDistance = fDefenseDistanceFromEnemy - CLeaderAction_Attack_Row::m_CMaxDefenseDistance;
			if(fInitDistance<2)
				fInitDistance=2;
			else if(fInitDistance>CLeaderAction_Attack_Row::m_CInitDistance)
				fInitDistance = CLeaderAction_Attack_Row::m_CInitDistance;
		}
		else
			fInitDistance = CLeaderAction_Attack_Row::m_CInitDistance;

		float fUnitMaxSize = 0;
		for(TUnitList::const_iterator itUnit = m_pLeader->GetAIGroup()->GetUnits().begin(); itUnit != m_pLeader->GetAIGroup()->GetUnits().end();++itUnit)
		{
			const CUnitImg& unit = *itUnit;
			if(unit.m_pUnit->IsEnabled() && (unit.GetProperties() & m_iUnitProp))
			{
				float fMySize = 2*(unit.m_pUnit->GetParameters().m_fPassRadius);
				if(fMySize>fUnitMaxSize)
					fUnitMaxSize = fMySize;
			}
		}

		float fRowSemiWidthByDistance = fInitDistance/2;
		float fRowSemiWidthByUnitSize = fUnitMaxSize*float(unitcount-1)/2;
		if(fRowSemiWidthByDistance > fRowSemiWidthByUnitSize*4)
			fRowSemiWidthByDistance = fRowSemiWidthByUnitSize*4;

		float fRowSemiWidth = max(fRowSemiWidthByDistance,fRowSemiWidthByUnitSize);

		const float CRowDistanceStep = 1.5f;
		const float CMinUnitDistance = 2.0f;  
		
		float dispArray[3] = {0,CRowDistanceStep, -CRowDistanceStep };

		for(int i=0;i<3;i++)
		{
			float disp = dispArray[i];
			float fWidth ;
			float fDist = fInitDistance;
			Vec3 vAttackRowBase = vEnemyPos - vCurrentDir*(fDist  + disp);///fDist;
			//vAttackRowBase.z +=1;
			Vec3 vRowSegment(vCurrentDir.y,-vCurrentDir.x,0); //2D only, rotate 90 around Z
			vRowSegment *= fRowSemiWidth;
			Vec3 vEdge1 = vAttackRowBase + vRowSegment;
			Vec3 vEdge2 = vAttackRowBase - vRowSegment;
//			float fUnitDistance = fWidth/float(unitcount);
//			if(fUnitDistance>=CMinUnitDistance) //units fit in the row
			{
				//check visibility from the row
				const int numStep = 4;
				int iHits = 0;
				Vec3 vStep = (vEdge2 - vEdge1)/float(numStep*2+1);
				Vec3 vPoint1 = vEdge1;
				Vec3 vPoint2 = vEdge2;
				//vEnemyPoint.z = vPoint1.z;
				bool bEdgeHit1 = true;
				bool bEdgeHit2 = true;
				int rayresult;
				for(int k=0;k<numStep;k++)
				{
					float fTerrainHeight =GetAISystem()->m_pSystem->GetI3DEngine()->GetTerrainElevation( vPoint1.x, vPoint1.y );
					Vec3 vPointStart = vPoint1;
					if(vPointStart.z <fTerrainHeight)
						vPointStart.z = fTerrainHeight;
					vPointStart.z +=1;
					Vec3 vPointEnd = vEnemyPos;
					vPointEnd.z += 1;
TICK_COUNTER_FUNCTION
TICK_COUNTER_FUNCTION_BLOCK_START
					rayresult = pWorld->RayWorldIntersection(vPointStart,vPointEnd-vPointStart, ent_terrain+ent_static+ent_rigid+ent_sleeping_rigid,rwi_ignore_noncolliding, &hit, 1);
TICK_COUNTER_FUNCTION_BLOCK_END

					vPoint1+=vStep;
					if(rayresult)// && hit.dist<fMaxDist)
					{
						iHits++;
						if(bEdgeHit1)
							vEdge1 = vPoint1;
					}
					else
						bEdgeHit1  =false;

//					fMaxDist = (vEnemyPoint - vPoint2).GetLength();
					fTerrainHeight = GetAISystem()->m_pSystem->GetI3DEngine()->GetTerrainElevation( vPoint2.x, vPoint2.y );
					vPointStart = vPoint2;
					if(vPointStart.z <fTerrainHeight)
						vPointStart.z = fTerrainHeight;
					vPointStart.z +=1;
TICK_COUNTER_FUNCTION_BLOCK_START
					rayresult = pWorld->RayWorldIntersection(vPointStart,vPointEnd-vPointStart, ent_terrain+ent_static+ent_rigid+ent_sleeping_rigid,rwi_ignore_noncolliding, &hit, 1);
TICK_COUNTER_FUNCTION_BLOCK_END

					vPoint2-=vStep;
					if(rayresult)// && hit.dist<fMaxDist)
					{
						iHits++;
						if(bEdgeHit2)
							vEdge2 = vPoint2;
					}
					else
						bEdgeHit2 = false;
				}
				//if(iHits<= numStep/2) // is there enough visibility?
				fWidth = (vEdge2 - vEdge1).GetLength();
				float fUnitDistance = fWidth/float(unitcount>1 ? unitcount-1: 1);
				if(fUnitDistance>=fUnitMaxSize || unitcount==1)//is the row wide enough?
				{
					// TO DO: check reachability of points
					// success!
					vAttackRowBase = (vEdge1 + vEdge2)/2;
					LeaderActionParams params;
					params.vPoint = vAttackRowBase;
					params.vPoint2 = m_vDefensePoint;
					params.type = LA_ATTACK;
					params.subType = LAS_ATTACK_ROW;
					params.fSize = fWidth/float(unitcount-1);
					params.fDuration = m_fDuration;
					m_pLeader->Attack(&params);
					EndRequest("OnAttackRow");
					m_bUpdate = false;
					return true;
				}
				
			}

		}

	}
//	else 
	//	bFailed = true;

	m_fMultiplier = -(m_fMultiplier+1);
	float angle = m_fMultiplier*g_PI/3/float(m_iMaxUpdates);
	m_vCurrentDir = m_vCurrentDir.GetRotated(Vec3(0,0,1),angle);

	if(bFailed || ++m_iUpdateIndex >= m_iMaxUpdates)
	{
		EndRequest("OnLeaderActionFailed");
		return true;
	}
	return false;
}


bool CAttackRequest_Flank::Update()
{
	LeaderActionParams params;
	params.type = LA_ATTACK;
	params.subType = m_actionType;
	m_pLeader->Attack( &params );
	EndRequest( "OnAttackFlank" );
	m_bUpdate = false;
	return true;
}



void	CAttackRequest::Serialize( TSerialize ser, CObjectTracker& objectTracker, CLeader* pLeader )
{
	ser.Value("m_iUnitProp", m_iUnitProp);
	ser.EnumValue("m_actionType", m_actionType, LAS_DEFAULT, LAS_LAST);
	ser.Value("m_iUpdateIndex",	m_iUpdateIndex);
	ser.Value("m_fMultiplier", m_fMultiplier);
	ser.Value("m_bUpdate",	m_bUpdate);
	ser.Value("m_iMaxUpdates",	m_iMaxUpdates);
	ser.Value("m_bInit",	m_bInit);
	ser.Value("m_fDuration",	m_fDuration);
	ser.Value("m_iCount1",	m_iCount1);
	ser.Value("m_vCurrentDir",	m_vCurrentDir);
	m_pLeader = pLeader;


}
//
//----------------------------------------------------------------------------------------------------------

void CLeader::AssignTargetToUnits(uint32 unitProp, int maxUnitsPerTarget)
{
	//if(!m_pCurrentAction)
	//	return;

	CAIGroup::TSetAIObjects::iterator itTarget = m_pGroup->GetTargets().begin();
	std::vector<int> AssignedUnits;

	int indexTarget=0;
	const CAIObject* pTarget ;
	bool bBeacon = false;
	int numUnits = m_pGroup->GetUnitCount(unitProp);
	int numtargets = m_pGroup->GetTargets().size();

	if(numtargets==0)
		numtargets=1;
	for(int i=0;i<numtargets;i++)
		AssignedUnits.push_back(0);

	if(itTarget!= m_pGroup->GetTargets().end())
		pTarget = *itTarget;
	else
	{
		pTarget = static_cast<CAIObject*>(GetAISystem()->GetBeacon(GetGroupId()));
		bBeacon = true;
		if(!pTarget) // no beacon, no targets
			return;
	}

	int unitsPerTarget = numUnits/numtargets;
	if(unitsPerTarget > maxUnitsPerTarget)
		unitsPerTarget = maxUnitsPerTarget;

	for(TUnitList::iterator it=m_pGroup->GetUnits().begin(); it!=m_pGroup->GetUnits().end();++it)
	{
		CUnitImg& unit = *it;
		if(unit.GetProperties() & unitProp)
		{
			AISignalExtraData data;
			unit.ClearPlanning();
			IEntity* pTargetEntity = NULL;

			if(const CPuppet* pPuppet = pTarget->CastToCPuppet())
				pTargetEntity = (IEntity*)pPuppet->GetAssociation();
			else if(const CAIPlayer* pPlayer = pTarget->CastToCAIPlayer())
				pTargetEntity = (IEntity*)pPlayer->GetAssociation();

			if(pTargetEntity)
				data.nID = pTargetEntity->GetId();
			data.SetObjectName(pTarget->GetName());

			CUnitAction* pAcquire = new CUnitAction(UA_ACQUIRETARGET,true,"",data);
			unit.m_Plan.push_back(pAcquire);
			unit.ExecuteTask();
			AssignedUnits[indexTarget] = AssignedUnits[indexTarget]+1;
			if(AssignedUnits[indexTarget]>= unitsPerTarget)
			{
				indexTarget++;
				if(indexTarget>=numtargets)
					break;
				if(itTarget!= m_pGroup->GetTargets().end())
					++itTarget;
				if(itTarget== m_pGroup->GetTargets().end())
					break;
				else
					pTarget = *itTarget;
			}

		}
	}
	UpdateEnemyStats();
}

int CLeader::GetActiveUnitPlanCount( uint32 unitProp ) const
{
	int	active = 0;
	for(TUnitList::const_iterator it=m_pGroup->GetUnits().begin(); it!=m_pGroup->GetUnits().end();++it)
	{
		const CUnitImg& unit = *it;
		if((unit.GetProperties() & unitProp) && !unit.m_Plan.empty())
			active++;
	}
	return active;
}

void CLeader::OnUpdateUnitTask(CUnitImg* unit)
{	// called on UA_WHATSNEXT
	if (m_pCurrentAction)
		m_pCurrentAction->OnUpdateUnitTask(unit);
}

//
//-------------------------------------------------------------------------------------

CAIObject* CLeader::GetEnemyLeader() const
{
	CAIActor* pTarget = CastToCAIActorSafe(m_pGroup->GetAttentionTarget(true));
	CAIObject* pAILeader = NULL;
	if(pTarget)
	{
		CLeader* pLeader = GetAISystem()->GetLeader(pTarget);
		if(pLeader)
			pAILeader = (CAIObject*)pLeader->GetAssociation();
	}
	return pAILeader;
}

//
//-------------------------------------------------------------------------------------
float CLeader::GetCoverPercentage(const Vec3& defensePoint,uint32 unitProp, bool bUsePredictedPositions) const
{
	
	if(m_pGroup->GetTargets().empty())
		return 1;

	TUnitList::const_iterator itUnit, itUnitEnd = m_pGroup->GetUnits().end();

	CAIGroup::TSetAIObjects::const_iterator itTarget =  m_pGroup->GetTargets().begin();
	CAIGroup::TSetAIObjects::const_iterator itTargetEnd =  m_pGroup->GetTargets().end();
	float avgCover = 0;
	int enemycount = 0;

	Vec3 vOwnerPredictedPos(0,0,0), vOwnerPredictedMoveDir(0,0,0),vOwnerPredictedLookDir(0,0,0);
	// pre-compute formation prediction data
	if(bUsePredictedPositions && m_pFormationOwner && m_pFormationOwner->m_pFormation)
	{
		CPipeUser* pPiper = m_pFormationOwner->CastToCPipeUser();
		if (m_pFormationOwner)
		{

			TPathPoints::const_reverse_iterator itPath = pPiper->m_OrigPath.GetPath().rbegin();
			if(itPath!=pPiper->m_OrigPath.GetPath().rend())
			{
				vOwnerPredictedPos = itPath->vPos;
				itPath++;
				if(itPath!=pPiper->m_OrigPath.GetPath().rend())
				{
					vOwnerPredictedMoveDir = (vOwnerPredictedPos - itPath->vPos);
					vOwnerPredictedMoveDir.NormalizeSafe();
				}
			}
			if(vOwnerPredictedPos.IsZero())
				vOwnerPredictedPos = m_pFormationOwner->GetPos();

			Vec3 enemyPos = m_pGroup->GetEnemyAveragePosition();
			if(enemyPos.IsZero())
			{
				if(IAIObject* pBeacon=GetAISystem()->GetBeacon(GetGroupId()))
					enemyPos = pBeacon->GetPos();
				else
					return 1;
			}
			
			if(vOwnerPredictedMoveDir.IsZero())
			{
				vOwnerPredictedMoveDir = vOwnerPredictedPos - m_pFormationOwner->GetPos();
				if(vOwnerPredictedMoveDir.IsZero())
				{
					vOwnerPredictedMoveDir = m_pFormationOwner->GetMoveDir();
					vOwnerPredictedMoveDir.z=0; //2d only
					vOwnerPredictedMoveDir.NormalizeSafe();
				}
			}

			vOwnerPredictedLookDir = enemyPos - vOwnerPredictedPos;

		}
		else
			return 0; // not a pipeuser leading the formation, should never happen
	}

	for(;itTarget!=itTargetEnd; ++itTarget)
	{
		float maxCover = 0;
		Vec3 vEnemyPos = (*itTarget)->GetPos();
		Vec3 vEnemyDir =  defensePoint - vEnemyPos;
		float enemyDist = vEnemyDir.GetLength();
		if(enemyDist<0.1f) // enemy is on defense point, no cover at all
			return 0;
		
		vEnemyDir/=enemyDist;

		// find the unit which covers this enemy the most
		for(itUnit= m_pGroup->GetUnits().begin();itUnit!= itUnitEnd; ++itUnit)
		{
			const CUnitImg& unit = *itUnit;
			if(unit.m_pUnit->IsEnabled() && (unit.GetProperties()& unitProp))
			{
				Vec3 vUnitPos;
				if(bUsePredictedPositions)
				{
					vUnitPos = ZERO;
					const CUnitAction* pCurrentAction = unit.GetCurrentAction();
					if(pCurrentAction)
						switch(pCurrentAction->m_Action)
						{
						case UA_FORM:
						case UA_FORM_SHOOT:
							vUnitPos = m_pFormationOwner->m_pFormation->GetPredictedPointPosition(unit.m_pUnit,vOwnerPredictedPos,vOwnerPredictedLookDir,vOwnerPredictedMoveDir);
							break;
						case UA_MOVE:
						case UA_FLANK:
							if (CPuppet* pPuppet = unit.m_pUnit->CastToCPuppet())
							{
								vUnitPos = pPuppet->GetRefPoint()->GetPos();
							}
							else 
								vUnitPos = unit.m_pUnit->GetPos();
							break;
							// TO DO: predict for other unit actions
						default:
							vUnitPos = unit.m_pUnit->GetPos();
							break;
						}
					else
						vUnitPos = unit.m_pUnit->GetPos();
				}
				else
					vUnitPos = unit.m_pUnit->GetPos();

				Vec3 vUnitEnemyDir = vUnitPos - vEnemyPos;
				vUnitEnemyDir.NormalizeSafe();
				float fCover = vEnemyDir.Dot(vUnitEnemyDir);
				if(fCover>0) // else the unit is behind the enemy
				{
					fCover *=fCover; // tweaking scale - approximation (ideal: 1 - arccos(fCover)/pi/2)
					if(fCover>maxCover)
						maxCover = fCover;
				}
			}
		}

		enemycount++;
		avgCover += maxCover;
	}
	return(avgCover/float(enemycount));
}

void CLeader::DeadUnitNotify(CAIActor* pAIObj)
{
	if(pAIObj && pAIObj== GetAssociation())
	{
		GetAISystem()->SendSignal(SIGNALFILTER_SUPERGROUP, 0, "OnLeaderDied", pAIObj);
		m_bLeaderAlive = false;
	}

	if(m_pCurrentAction) m_pCurrentAction->DeadUnitNotify(pAIObj);

}

/*
//
//------------------------------------------------------------------------------------------------------------
void CLeader::ClearAttackRequest(uint32 unitprop)
{
TAttackRequestMap::iterator it = m_AttackRequestMap.find(unitprop) ;
if(it != m_AttackRequestMap.end())
{
m_AttackRequestMap.erase(it);
}
}
*/

/*
//--------------------------------------------------------------------------------------------------------------
CAttackRequest::CAttackRequest(uint32 unitprop,TSubActionList& ActionList)
{
	m_unitProp = unitprop;
	for(TSubActionTable::iterator it = ActionList.begin(); iterator!= ActionList.end();++it)
		m_ActionList.push_back(*it);
	m_ActionIt = m_ActionList.begin();
}

//--------------------------------------------------------------------------------------------------------------
bool CAttackRequest::Update()
{

}
*/
