/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
File name:   FlowNodeAIAction.cpp
$Id$
Description: place for AI action related flow graph nodes

-------------------------------------------------------------------------
History:
- 15:6:2005   15:24 : Created by Kirill Bulatsev

*********************************************************************/



#include "StdAfx.h"
#include <IAISystem.h>
#include <IAgent.h>
#include "IActorSystem.h"
#include "IVehicleSystem.h"
#include "FlowBaseNode.h"
#include "FlowNodeAIAction.h"


//////////////////////////////////////////////////////////////////////////
// base AI Flow node 
//////////////////////////////////////////////////////////////////////////
//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking> CFlowNode_AIBase<TDerived, TBlocking>::CFlowNode_AIBase( SActivationInfo* pActInfo )
: m_bExecuted( false )
, m_bSynchronized( false )
, m_bNeedsSink( false )
, m_bNeedsExec( false )
, m_bNeedsReset( true )
, m_EntityId( 0 )
, m_UnregisterEntityId( 0 )
, m_GoalPipeId( 0 )
, m_UnregisterGoalPipeId( 0 )
{
	//		m_entityId = (EntityId) pActInfo->m_pUserData;
	m_nodeID = pActInfo->myID;
	m_pGraph = pActInfo->pGraph;
}
//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking> CFlowNode_AIBase<TDerived, TBlocking>::~CFlowNode_AIBase()
{
	UnregisterEvents();
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking> IFlowNodePtr CFlowNode_AIBase<TDerived, TBlocking>::Clone( SActivationInfo* pActInfo )
{
	//		pActInfo->m_pUserData = (void*) m_entityId;
	TDerived* p = new TDerived( pActInfo );
	p->m_bNeedsExec = m_bNeedsExec;
	p->m_bNeedsSink = m_bNeedsSink;
	return p;
}


//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking> void CFlowNode_AIBase<TDerived, TBlocking>::Serialize(SActivationInfo *, TSerialize ser)
{
	if ( ser.IsReading() )
		UnregisterEvents();

	ser.Value("GoalPipeId", m_GoalPipeId);
	ser.Value("UnregisterGoalPipeId", m_UnregisterGoalPipeId);
	ser.Value("EntityId", m_EntityId);
	ser.Value("UnregisterEntityId", m_UnregisterEntityId);
	ser.Value("bExecuted", m_bExecuted);
	ser.Value("bSynchronized", m_bSynchronized);
	ser.Value("bNeedsExec", m_bNeedsExec);
	ser.Value("bNeedsSink", m_bNeedsSink);
	ser.Value("bNeedsReset", m_bNeedsReset);

	if ( ser.IsReading() && /*m_bBlocking &&*/ m_EntityId!=0)
	{
		RegisterEvents();
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking> void CFlowNode_AIBase<TDerived, TBlocking>::ProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	if ( event == eFE_ConnectInputPort )
	{
		if ( pActInfo->connectPort == 0 )
			m_bNeedsExec = true;
		else if ( pActInfo->connectPort == 1 )
			m_bNeedsSink = true;
	}
	else if ( event == eFE_DisconnectInputPort )
	{
		if ( pActInfo->connectPort == 0 )
			m_bNeedsExec = false;
		else if ( pActInfo->connectPort == 1 )
			m_bNeedsSink = false;
	}
	else if ( event == eFE_Initialize )
	{
		if ( m_bNeedsReset )
		{
			// it may happen that updates were enabled in previous game session
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );

			UnregisterEvents();
			m_EntityId = 0;
			m_GoalPipeId = 0;
			m_bExecuted = false;
			m_bSynchronized = false;
			//		m_bNeedsExec = pActInfo->pEntity == 0;
			//		m_bNeedsSink = false;
			m_bNeedsReset = false;
		}
		if ( m_bNeedsExec && pActInfo->pEntity && IsPortActive(pActInfo, -1) )
		{
			// entity is dynamically assigned during initialization
			m_bNeedsExec = false;
		}
	}
	else if ( event == eFE_SetEntityId && pActInfo->pGraph->GetAIAction() != NULL )
	{
		m_bNeedsReset = true;
		if ( m_bNeedsExec && !m_bExecuted && pActInfo->pEntity )
		{
			m_bNeedsReset = true;
			m_bExecuted = true;
			if ( m_bSynchronized || !m_bNeedsSink )
				pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
		}
	}
	else if ( event == eFE_SetEntityId && pActInfo->pGraph->GetAIAction() == NULL )
	{
		m_bNeedsReset = true;
		if ( m_bNeedsExec && !m_bExecuted && pActInfo->pEntity )
		{
			m_bNeedsReset = true;
			m_bExecuted = true;
			if ( m_bSynchronized /*|| !m_bNeedsSink*/ )
				pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
		}
	}
	else if ( event == eFE_Activate )
	{
/*
		if ( IsPortActive(pActInfo, -1) )
		{
			m_bNeedsReset = true;
			if ( m_bNeedsExec && !m_bExecuted && pActInfo->pEntity )
			{
				m_bNeedsReset = true;
				m_bExecuted = true;
				if ( m_bSynchronized || !m_bNeedsSink )
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
			}
		}
*/
		if ( IsPortActive(pActInfo, 0) )
		{
			m_bNeedsReset = true;
			if ( m_bNeedsSink && !m_bSynchronized )
			{
				m_bSynchronized = true;
				if ( m_bExecuted || !m_bNeedsExec )
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
				PreExecute( pActInfo );
			}
		}
		if ( TBlocking && m_EntityId && m_GoalPipeId && IsPortActive(pActInfo, 1) )
		{
			IEntity* pEntity = GetEntity( pActInfo );
			if ( pEntity )
			{
				IAIObject* pAI = pEntity->GetAI();
				if ( pAI )
				{
					IPipeUser* pPipeUser = pAI->CastToIPipeUser();
					if (pPipeUser)
						OnCancelPortActivated( pPipeUser, pActInfo );
				}
			}
		}
	}
	else if ( event == eFE_Update )
	{
		m_bNeedsReset = true;
		IEntity* pEntity = GetEntity( pActInfo );
		if ( pEntity )
		{	
			// first check is ai object updated at least once
			IAIObject* pAI = pEntity->GetAI();
			if ( !pAI || !pAI->IsAgent() || pAI->IsUpdatedOnce() )
			{
				if ( !OnUpdate(pActInfo) )
				{
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
					ExecuteIfNotTooMuchAlerted( pAI, event, pActInfo );
				}
			}
			else
				m_EntityId = 0;
		}
		else
		{
			m_bExecuted = false;
			m_bSynchronized = false;
		}
	}
	else if ( event == eFE_Resume && m_EntityId )
	{
		OnResume( pActInfo );
		//	UnregisterEvents();
		//	pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
	}
}

template< class TDerived, bool TBlocking >
void CFlowNode_AIBase< TDerived, TBlocking >::ExecuteIfNotTooMuchAlerted( IAIObject* pAI, EFlowEvent event, SActivationInfo* pActInfo )
{
	// check is the node inside an entity flow graph and alerted. if so fail execution
	if ( pAI && pAI->GetProxy() && pAI->GetProxy()->GetAlertnessState() && !pActInfo->pGraph->GetAIAction() )
	{
		IEntity*	graphOwner = gEnv->pEntitySystem->GetEntity(pActInfo->pGraph->GetGraphEntity(0));
		gEnv->pAISystem->LogEvent("<Flowgraph> ", "Canceling execution of an AI flowgraph node because the agent '%s' alertness is too high (%d). Owner:%s Node:%d",
			pAI->GetName(), pAI->GetProxy()->GetAlertnessState(), graphOwner ? graphOwner->GetName() : "<unknown>", pActInfo->myID);

		Cancel();
	}
	else
		DoProcessEvent( event, pActInfo );
}

template<class TDerived, bool TBlocking> void CFlowNode_AIBase<TDerived, TBlocking>::OnCancelPortActivated( IPipeUser* pPipeUser, SActivationInfo* pActInfo )
{
	if ( m_GoalPipeId == -1 )
		Cancel();
	else
		pPipeUser->CancelSubPipe( m_GoalPipeId );
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking> void CFlowNode_AIBase<TDerived, TBlocking>::OnGoalPipeEvent( IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId )
{
	if ( m_GoalPipeId != goalPipeId )
		return;

	switch ( event )
	{
	case ePN_OwnerRemoved:
		Cancel();
		m_UnregisterGoalPipeId = 0;
		m_GoalPipeId = 0;
		break;

	case ePN_Deselected:
		Cancel();
		break;

	case ePN_Removed:
	case ePN_Finished:
		Finish();
		break;

	case ePN_Suspended:
		break;

	case ePN_Resumed:
	case ePN_RefPointMoved:
		OnResume();
		break;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking> void CFlowNode_AIBase<TDerived, TBlocking>::OnEntityEvent( IEntity* pEntity, SEntityEvent& event )
{
	switch ( event.event )
	{
	case ENTITY_EVENT_AI_DONE:
		if ( m_pGraph->IsSuspended() )
			return;
		Finish();
		break;

	case ENTITY_EVENT_RESET:
	case ENTITY_EVENT_DONE:
		Cancel();
		break;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking>void CFlowNode_AIBase<TDerived, TBlocking>::Cancel()
{
	OnCancel();

	SFlowAddress done( m_nodeID, 0, true );
	SFlowAddress fail( m_nodeID, 2, true );
	m_pGraph->ActivatePort( done, m_EntityId );
	m_pGraph->ActivatePort( fail, m_EntityId );

	if ( m_EntityId )
	{
		m_bExecuted = false;
		m_bSynchronized = false;

		m_UnregisterEntityId = m_EntityId;
		m_EntityId = 0;

		m_UnregisterGoalPipeId = m_GoalPipeId;
		m_GoalPipeId = 0;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking>void CFlowNode_AIBase<TDerived, TBlocking>::Finish()
{
	SFlowAddress done( m_nodeID, 0, true );
	SFlowAddress succeed( m_nodeID, 1, true );
	m_pGraph->ActivatePort( done, m_EntityId );
	m_pGraph->ActivatePort( succeed, m_EntityId );

	if ( m_EntityId )
	{
		m_bExecuted = false;
		m_bSynchronized = false;

		m_UnregisterEntityId = m_EntityId;
		m_EntityId = 0;

		m_UnregisterGoalPipeId = m_GoalPipeId;
		m_GoalPipeId = 0;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking>void CFlowNode_AIBase<TDerived, TBlocking>::RegisterEvents()
{
	if ( m_EntityId )
	{
		IEntitySystem* pSystem = gEnv->pEntitySystem;
		pSystem->AddEntityEventListener( m_EntityId, ENTITY_EVENT_AI_DONE, this );
		pSystem->AddEntityEventListener( m_EntityId, ENTITY_EVENT_POST_SERIALIZE, this );
		//	pSystem->AddEntityEventListener( m_EntityId, ENTITY_EVENT_DONE, this );
		//	pSystem->AddEntityEventListener( m_EntityId, ENTITY_EVENT_RESET, this );

		if ( m_GoalPipeId > 0 )
		{
			IEntity* pEntity = pSystem->GetEntity( m_EntityId );
			if ( pEntity )
			{
				IAIObject* pAIObject = pEntity->GetAI();
				if ( pAIObject )
				{
					IPipeUser* pPipeUser = pAIObject->CastToIPipeUser();
					if (pPipeUser)
					{
						pPipeUser->RegisterGoalPipeListener( this, m_GoalPipeId, "CFlowNode_AIBase::RegisterEvents" );
					}
				}
			}
		}
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking>void CFlowNode_AIBase<TDerived, TBlocking>::UnregisterEvents()
{
	if ( m_UnregisterEntityId && !m_EntityId )
		m_EntityId = m_UnregisterEntityId;

	if ( m_EntityId )
	{
		IEntitySystem* pSystem = gEnv->pEntitySystem;
		if(pSystem)
		{
			pSystem->RemoveEntityEventListener( m_EntityId, ENTITY_EVENT_AI_DONE, this );
			pSystem->RemoveEntityEventListener( m_EntityId, ENTITY_EVENT_POST_SERIALIZE, this );
		}

		if ( m_UnregisterGoalPipeId && !m_GoalPipeId )
			m_GoalPipeId = m_UnregisterGoalPipeId;
		if ( pSystem && m_GoalPipeId > 0 )
		{
			IEntity* pEntity = pSystem->GetEntity( m_EntityId );
			if ( pEntity )
			{
				IAIObject* pAIObject = pEntity->GetAI();
				if ( pAIObject )
				{
					IPipeUser* pPipeUser = pAIObject->CastToIPipeUser();
					if (pPipeUser)
					{
						pPipeUser->UnRegisterGoalPipeListener( this, m_GoalPipeId );
					}
				}
			}
		}
		if ( m_UnregisterGoalPipeId && m_GoalPipeId == m_UnregisterGoalPipeId )
			m_UnregisterGoalPipeId = 0;
		m_GoalPipeId = 0;

		//	pSystem->RemoveEntityEventListener( m_EntityId, ENTITY_EVENT_DONE, this );
		//	pSystem->RemoveEntityEventListener( m_EntityId, ENTITY_EVENT_RESET, this );
	}

	if ( m_UnregisterEntityId && m_EntityId == m_UnregisterEntityId )
		m_UnregisterEntityId = 0;
	m_EntityId = 0;
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking>IEntity* CFlowNode_AIBase<TDerived, TBlocking>::GetEntity( SActivationInfo* pActInfo )
{
	m_EntityId = 0;
	if (pActInfo->pEntity)
		m_EntityId = pActInfo->pEntity->GetId();
	return pActInfo->pEntity;
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking>bool CFlowNode_AIBase<TDerived, TBlocking>::Execute( SActivationInfo* pActInfo, const char* pSignalText, IAISignalExtraData* pData, int senderId )
{
	UnregisterEvents();

	IEntity* pEntity = GetEntity( pActInfo );
	if ( !pEntity )
		return false;

	IEntity* pSender = pEntity;
	if ( senderId )
		pSender = gEnv->pEntitySystem->GetEntity( senderId );

	bool result = false;
	if ( pEntity->GetAI() )
		result = ExecuteOnAI( pActInfo, pSignalText, pData, pEntity, pSender );
	if ( !result )
		result = ExecuteOnEntity( pActInfo, pSignalText, pData, pEntity, pSender );
	if ( !result )
		Cancel();

	return result;
}


//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking> bool CFlowNode_AIBase<TDerived, TBlocking>::ExecuteOnAI( SActivationInfo* pActInfo, const char* pSignalText,
																																																 IAISignalExtraData* pData, IEntity* pEntity, IEntity* pSender )
{
	IAIObject* pAI = pEntity->GetAI();
	assert( pAI );
	unsigned short nType=pAI->GetAIType();
	if ( nType == AIOBJECT_VEHICLE )
	{
		// activate vehicle AI, unless it's destroyed
		IVehicle* pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle( pEntity->GetId() );
		if ( !pVehicle || pVehicle->IsDestroyed() )
			return false;
		pAI->Event( AIEVENT_DRIVER_IN, NULL );// enabling vehicle's full update to process signals, even if there is no driver 
	}
	if ( nType != AIOBJECT_VEHICLE && nType != AIOBJECT_PUPPET )
	{
		if ( nType == AIOBJECT_PLAYER )
		{
			// execute on player
			m_EntityId = pEntity->GetId();
			ActivateOutput( pActInfo, 0, m_EntityId );
			ActivateOutput( pActInfo, 1, m_EntityId );
			m_bExecuted = false;
			m_bSynchronized = false;
			m_EntityId = 0;
			m_GoalPipeId = -1;
			IAIActor* pAIActor = pAI->CastToIAIActor();
			if(pAIActor)
				pAIActor->SetSignal( 10, pSignalText, pSender, pData ); // 10 means this signal must be sent (but sent[!], not set)
			// even if the same signal is already present in the queue
			return true;
		}

		// invalid AIObject type
		return false;
	}

	if ( m_bBlocking )
	{
		m_EntityId = pEntity->GetId();
		m_GoalPipeId = gEnv->pAISystem->AllocGoalPipeId();
		if ( !pData )
			pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->iValue = m_GoalPipeId;
		RegisterEvents();
	}
	else
	{
		m_EntityId = pEntity->GetId();
		m_GoalPipeId = gEnv->pAISystem->AllocGoalPipeId();
		IAISignalExtraData* pExtraData = gEnv->pAISystem->CreateSignalExtraData();
		assert( pExtraData );
		pExtraData->iValue = m_GoalPipeId;
		RegisterEvents();
		IAIActor* pAIActor = pAI->CastToIAIActor();
		if(pAIActor)
			pAIActor->SetSignal( 10, "ACT_DUMMY", pEntity, pExtraData );
		/*
		ActivateOutput( pActInfo, 0, m_EntityId );
		ActivateOutput( pActInfo, 1, m_EntityId );
		m_bExecuted = false;
		m_bSynchronized = false;
		m_EntityId = 0;
		m_GoalPipeId = 0;
		*/
	}
	IAIActor* pAIActor = pAI->CastToIAIActor();
	if(pAIActor)
		pAIActor->SetSignal( 10, pSignalText, pSender, pData ); // 10 means this signal must be sent (but sent[!], not set)
	// even if the same signal is already present in the queue
	return true;
}

//
//-------------------------------------------------------------------------------------------------------------
template<class TDerived, bool TBlocking> bool CFlowNode_AIBase<TDerived, TBlocking>::ExecuteOnEntity( SActivationInfo* pActInfo, const char* pSignalText,
																																																		 IAISignalExtraData* pData, IEntity* pEntity, IEntity* pSender )
{
	// sorry, not implemented :(
	//	assert( 0 );
	// dont assert: it's perfectly normal for this to happen at runtime for AISpawners
	return false;
}

// must sort-of match COPRunCmd::COPRunCmd
template<class TDerived, bool TBlocking> void CFlowNode_AIBase<TDerived, TBlocking>::SetSpeed(IAIObject* pAI,int iSpeed)
{
	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return;
	SOBJECTSTATE* pState = pAIActor->GetState();
	if (!pState)
		return;
	// There is code/configuration elsewhere that means we won't even move at all if we select 
	// a "pseudo-speed" value that is too low (but still non-zero). So we're forced to hard-code these 
	// numbers in the hope that stuff elsewhere won't change because the interface makes us use a floating point number 
	// ("pseudoSpeed") to represent discrete states than a finite set of values.
	if (iSpeed == -5)//workaround to stop vehicle during FollowPathSpeedStance (for expansion pack)
		pState->fMovementUrgency = AISPEED_ZERO; //leaving other values unchanged for compatibility
	if (iSpeed == -4)
		pState->fMovementUrgency = -AISPEED_SPRINT;
	if (iSpeed == -3)
		pState->fMovementUrgency = -AISPEED_RUN;
	if (iSpeed == -2)
		pState->fMovementUrgency = -AISPEED_WALK;
	if (iSpeed == -1)
		pState->fMovementUrgency = -AISPEED_SLOW;
	if (iSpeed == 0)
		pState->fMovementUrgency = AISPEED_SLOW;
	else if (iSpeed == 1)
		pState->fMovementUrgency = AISPEED_WALK;
	else if (iSpeed == 2)
		pState->fMovementUrgency = AISPEED_RUN;
	else if (iSpeed >= 3)
		pState->fMovementUrgency = AISPEED_SPRINT;
}

template<class TDerived, bool TBlocking> void CFlowNode_AIBase<TDerived, TBlocking>::SetStance(IAIObject* pAI,int stance)
{
	IAIActor* pAIActor = pAI->CastToIAIActor();
	if (!pAIActor)
		return;
	SOBJECTSTATE* pState = pAIActor->GetState();
	if ( pState )
	{
		pState->forceWeaponAlertness = false;
		switch ( stance )
		{
		case 0:
			stance = BODYPOS_PRONE;
			break;
		case 1:
			stance = BODYPOS_CROUCH;
			break;
		case 2:
			stance = BODYPOS_STAND;
			break;
		case 3:
			pState->forceWeaponAlertness = true;
			stance = BODYPOS_STAND;
			break;
		case 4:
			stance = BODYPOS_RELAX;
			break;
		case 5:
			stance = BODYPOS_STEALTH;
			break;
		}
		pState->bodystate = stance;

		if(pAI->HasFormation())
		{
			IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
			pData->iValue = stance;
			gEnv->pAISystem->SendSignal(SIGNALFILTER_FORMATION_EXCEPT,1,"OnChangeStance",pAI,pData);
		}
	}
}



//---------------------------------------------------------------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////
// generic AI order
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
template < class TDerivedFromSignalBase >
void CFlowNode_AISignalBase< TDerivedFromSignalBase >::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		{0}
	};
	config.sDescription = _HELP( "generic AI signal" );
	config.pInputPorts = in_config;
	config.pOutputPorts = 0;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
template < class TDerivedFromSignalBase >
void CFlowNode_AISignalBase< TDerivedFromSignalBase >::DoProcessEvent( IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo *pActInfo )
{
	IEntity* pEntity = this->GetEntity( pActInfo );
	if ( !pEntity )
		return;
	IAIObject* pAI = pEntity->GetAI();
	if ( !pAI )
		return;
	IAIActor* pAIActor = pAI->CastToIAIActor();
	if(!pAIActor)
		return;
	if ( pAI->GetAIType() != AIOBJECT_VEHICLE && pAI->GetAIType() != AIOBJECT_PUPPET )
		return; // invalid AIObject type

	// not needed for now:
	//if ( pAI->GetType() == AIOBJECT_VEHICLE )
	//	pAI->Event( AIEVENT_ENABLE, NULL );

	IAISignalExtraData* pExtraData = GetExtraData( pActInfo );
	pAIActor->SetSignal( 10, m_SignalText, pEntity, pExtraData );

	// allow using the node more than once
	this->m_bExecuted = false;
	this->m_bSynchronized = false;
	this->m_EntityId = 0;
}

//
//-------------------------------------------------------------------------------------------------------------
template < class TDerivedFromSignalBase >
void CFlowNode_AISignalBase< TDerivedFromSignalBase >::Cancel()
{
	// Note: This function is similar as the AIBase Cancel, but
	// does not set the (Done/Succeed/Cancel) outputs since AISignalBase does not have them.
	this->OnCancel();

	if ( this->m_EntityId )
	{
		this->m_bExecuted = false;
		this->m_bSynchronized = false;

		this->m_UnregisterEntityId = this->m_EntityId;
		this->m_EntityId = 0;

		this->m_UnregisterGoalPipeId = this->m_GoalPipeId;
		this->m_GoalPipeId = 0;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
template < class TDerivedFromSignalBase >
void CFlowNode_AISignalBase< TDerivedFromSignalBase >::Finish()
{
	// Note: This function is similar as the AIBase Finish, but
	// does not set the (Done/Succeed/Cancel) outputs since AISignalBase does not have them.
	if ( this->m_EntityId )
	{
		this->m_bExecuted = false;
		this->m_bSynchronized = false;

		this->m_UnregisterEntityId = this->m_EntityId;
		this->m_EntityId = 0;

		this->m_UnregisterGoalPipeId = this->m_GoalPipeId;
		this->m_GoalPipeId = 0;
	}
}


//////////////////////////////////////////////////////////////////////////
// prototyping AI orders
//////////////////////////////////////////////////////////////////////////
//---------------------------------------------------------------------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////
// generic AI order
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISignal::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig<string>( "signal", _HELP("order to execute") ),
		InputPortConfig<Vec3>( "posValue", _HELP("pos signal data") ),
		InputPortConfig<Vec3>( "posValue2", _HELP("pos2 signal data") ),
		InputPortConfig<int>( "iValue", _HELP("int signal data") ),
		InputPortConfig<float>( "fValue", _HELP("float signal data") ),
		InputPortConfig<string>( "sValue", _HELP("string signal data") ),
		InputPortConfig<EntityId>( "id", _HELP("entity id signal data") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "generic AI action" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISignal::DoProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->point = GetPortVec3( pActInfo, 2 );
	pData->point2 = GetPortVec3( pActInfo, 3 );
	pData->iValue = GetPortInt( pActInfo, 4 );
	pData->fValue = GetPortFloat( pActInfo, 5 );
	pData->SetObjectName( GetPortString(pActInfo, 6) );
	pData->nID = GetPortEntityId( pActInfo, 7 );
	Execute( pActInfo, GetPortString(pActInfo, 1), pData );
}


//////////////////////////////////////////////////////////////////////////
// Executes an Action
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIExecute::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution"), _HELP("Cancel") ),
		InputPortConfig<EntityId>( "objectId", _HELP("Entity ID of the object on which the agent should execute AI Action"), _HELP("ObjectId") ),
		InputPortConfig<string>( "soaction_action", _HELP("AI action to be executed"), _HELP("Action") ),
		InputPortConfig<int>( "maxAlertness", 2, _HELP("maximum alertness which allows execution (0, 1 or 2)"), _HELP("MaxAlertness"), _UICONFIG("enum_int:Idle=0,Alerted=1,Combat=2") ),
		InputPortConfig<bool>( "HighPriority", true, _HELP("action priority - use to force the action to be finished (except if alertness get higher)") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "Executes an AI Action" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIExecute::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->SetObjectName(GetPortString( pActInfo, 3 ));
	pData->nID = GetPortEntityId( pActInfo, 2 );
	pData->fValue = GetPortInt( pActInfo, 4 );
	if (GetPortBool( pActInfo, 5 ))
		pData->fValue += 100.0f;
	Execute( pActInfo, "ACT_EXECUTE", pData, GetPortEntityId(pActInfo, 2) );
}

//
// should call DoProcessEvent if owner is not too much alerted
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIExecute::ExecuteIfNotTooMuchAlerted( IAIObject* pAI, EFlowEvent event, SActivationInfo* pActInfo )
{
	// check is the node inside an entity flow graph and alerted. if so fail execution
	if ( pAI && pAI->GetProxy() && pAI->GetProxy()->GetAlertnessState() > GetPortInt( pActInfo, 4 ) )
		Cancel();
	else
		DoProcessEvent( event, pActInfo );
}

//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIExecute::OnCancelPortActivated( IPipeUser* pPipeUser, SActivationInfo* pActInfo )
{
	if ( m_EntityId )
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
		if ( pEntity && m_GoalPipeId > 0 )
		{
			m_bCancel = true;
			gEnv->pAISystem->AbortAIAction( pEntity, m_GoalPipeId );
		}
	}
}

void CFlowNode_AIExecute::Cancel()
{
	m_bCancel = false;
	TBase::Cancel();
}

void CFlowNode_AIExecute::Finish()
{
	if ( m_bCancel )
		Cancel();
	else
		TBase::Finish();
}


//////////////////////////////////////////////////////////////////////////
// Set Character
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISetCharacter::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig<string>( "aicharacter_character" ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "Changes AI Character" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISetCharacter::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	IEntity* pEntity = GetEntity( pActInfo );
	if ( pEntity )
	{
		bool success = false;
		IAIObject* pAI = pEntity->GetAI();
		if ( pAI )
		{
			IPipeUser* pPipeUser = pAI->CastToIPipeUser();
			if (pPipeUser)
				success = pPipeUser->SetCharacter( GetPortString(pActInfo, 1) );
		}
		ActivateOutput( pActInfo, 0, m_EntityId );
		ActivateOutput( pActInfo, success ? 1 : 2, m_EntityId );
		m_bExecuted = false;
		m_bSynchronized = false;
		m_EntityId = 0;
	}
}


//////////////////////////////////////////////////////////////////////////
// Set State
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISetState::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig<string>( "sostate_State" ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "Changes Smart Object State" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory( EFLN_ADVANCED );
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISetState::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	gEnv->pAISystem->SetSmartObjectState( GetEntity(pActInfo), GetPortString(pActInfo, 1) );
	ActivateOutput( pActInfo, 0, m_EntityId );
	ActivateOutput( pActInfo, 1, m_EntityId );
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}


//////////////////////////////////////////////////////////////////////////
// Modify States
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIModifyStates::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig<string>( "sostates_states" ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "Adds/removes smart object states" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIModifyStates::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	gEnv->pAISystem->ModifySmartObjectStates( GetEntity(pActInfo), GetPortString(pActInfo, 1) );
	ActivateOutput( pActInfo, 0, m_EntityId );
	ActivateOutput( pActInfo, 1, m_EntityId );
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}


//////////////////////////////////////////////////////////////////////////
// Check States
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AICheckStates::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "Sync", _HELP("for synchronization only") ),
		InputPortConfig<string>( "sopattern_Pattern", _HELP("states pattern to check") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "True", _HELP("outputs the EntityId of the owner entity if its current smart object states are matching with the pattern") ),
		OutputPortConfig<EntityId>( "False", _HELP("outputs the EntityId of the owner entity if its current smart object states are not matching with the pattern") ),
		{0}
	};
	config.sDescription = _HELP( "checks smart object states of the owner if they match the specified pattern" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory( EFLN_ADVANCED );
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AICheckStates::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	if (gEnv->pAISystem->CheckSmartObjectStates( GetEntity(pActInfo), GetPortString(pActInfo, 1) ))
		ActivateOutput( pActInfo, 0, m_EntityId );
	else
		ActivateOutput( pActInfo, 1, m_EntityId );
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}


//////////////////////////////////////////////////////////////////////////
// Follow Path  
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFollowPath::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("sync") ),
		InputPortConfig_Void( "Cancel", _HELP("cancels execution") ),
		InputPortConfig<bool>( "PathFindToStart", _HELP("Path-find to start of path") ),
		InputPortConfig<bool>( "Reverse", _HELP("Reverse the path direction") ),
		InputPortConfig<bool>( "StartNearest", false, _HELP("Starts the path at nearest point on path") ),
		InputPortConfig<int>( "Loops", 0, _HELP("Number of times to loop around the path (-1 = forever)") ),
		InputPortConfig<string>( "PathName", _HELP("Name of the path to follow") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "Done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "Succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "Fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "follow path action" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFollowPath::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	bool pathFindToStart = GetPortBool( pActInfo, 2);
	bool reverse = GetPortBool( pActInfo, 3);
	bool nearest = GetPortBool( pActInfo, 4);
	int loops = GetPortInt( pActInfo, 5);
	const string& pathName = GetPortString( pActInfo, 6 );

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
	if ( pEntity )
	{
		IAIObject* pAI = pEntity->GetAI();
		if ( !pAI || pAI->GetAIType() != AIOBJECT_VEHICLE && pAI->GetAIType() != AIOBJECT_PUPPET )
			return;

		pAI->SetPathToFollow( pathName.c_str() ); //, pathFindToStart, reverse );

		IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->fValue = loops;
		pData->point.x = pathFindToStart ? 1.0f : 0.0f;
		pData->point.y = reverse ? 1.0f : 0.0f;
		pData->point.z = nearest ? 1.0f : 0.0f;
		Execute( pActInfo, "ACT_FOLLOWPATH", pData );
	}
}

//////////////////////////////////////////////////////////////////////////
// Follow Path Speed Stance
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFollowPathSpeedStance::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution") ),
		InputPortConfig<bool>( "PathFindToStart", _HELP("Path-find to start of path") ),
		InputPortConfig<bool>( "Reverse", _HELP("Reverse the path direction") ),
		InputPortConfig<bool>( "StartNearest", false, _HELP("Starts the path at nearest point on path") ),
		InputPortConfig<int>( "Loops", 0, _HELP("Number of times to loop around the path (-1 = forever)") ),
		InputPortConfig<string>( "path_name", _HELP("Name of the path to follow") ),
		InputPortConfig<int>( "run" ,1,_HELP("\n0=very slow\n1=normal (walk)\n2=fast (run)\n3=very fast (sprint)\n-1,-2,-3,-4 reverse\n-5=stop") ),
		InputPortConfig<int>( "stance", 4, _HELP("0-prone\n1-crouch\n2-combat\n3-combat alerted\n4-relaxed\n5-stealth"),
		_HELP("Stance"), _UICONFIG("enum_int:Prone=0,Crouch=1,Combat=2,CombatAlerted=3,Relaxed=4,Stealth=5") ),
		InputPortConfig<bool>( "BypassDynamicObjects", false, _HELP("Try to bypass vehicles, wrecks in the way") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "follow path speed stance action" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFollowPathSpeedStance::ProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	if ( m_EntityId && event == eFE_Activate && IsPortActive(pActInfo, 7) )
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
		if ( pEntity )
		{
			IAIObject* pAI = pEntity->GetAI();
			if ( !pAI || pAI->GetAIType() != AIOBJECT_VEHICLE && pAI->GetAIType() != AIOBJECT_PUPPET )
				return;
			SetSpeed( pAI, GetPortInt( pActInfo, 7 ));
		}
	}
	TBase::ProcessEvent( event, pActInfo );
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFollowPathSpeedStance::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	bool pathFindToStart = GetPortBool( pActInfo, 2);
	bool reverse = GetPortBool( pActInfo, 3);
	bool nearest = GetPortBool( pActInfo, 4);
	int loops = GetPortInt( pActInfo, 5);
	const string& pathName = GetPortString( pActInfo, 6 );
	int iSpeed = GetPortInt( pActInfo, 7 );
	int iStance = GetPortInt( pActInfo, 8 );
	bool avoid = GetPortBool( pActInfo, 9);

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
	if ( pEntity )
	{
		IAIObject* pAI = pEntity->GetAI();
		if ( !pAI || pAI->GetAIType() != AIOBJECT_VEHICLE && pAI->GetAIType() != AIOBJECT_PUPPET )
			return;
		SetStance( pAI, iStance );
		SetSpeed( pAI, iSpeed );
		pAI->SetPathToFollow( pathName.c_str() );

		IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->fValue = loops;
		pData->point.x = pathFindToStart ? 1.0f : 0.0f;
		pData->point.y = reverse ? 1.0f : 0.0f;
		pData->point.z = nearest ? 1.0f : 0.0f;
		pData->point2.x=   avoid ? 1.0f : 0.0f;
		Execute( pActInfo, "ACT_FOLLOWPATH", pData );
	}
}

//////////////////////////////////////////////////////////////////////////
// GOTO 
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGoto::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution") ),
		InputPortConfig<Vec3>( "pos" ),
		InputPortConfig<float>( "distance", 0.0f, _HELP("How accurately to position at the path end"), "EndAccuracy" ),
		InputPortConfig<float>( "StopDistance", 0.0f, _HELP("Stop this amount before the end of the path")),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "goto action" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGoto::ProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	// Dynamically update the point to approach to.
	if ( m_EntityId && event == eFE_Activate && IsPortActive(pActInfo, 2) )
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
		if ( pEntity )
		{
			m_vDestination = GetPortVec3( pActInfo, 2 );
			IAIObject* pAI = pEntity->GetAI();
			if ( pAI )
			{
				IPipeUser* pPipeUser = pAI->CastToIPipeUser();
				if (pPipeUser)
				{
					// First check is the current goal pipe the one which belongs to this node
					if ( pPipeUser->GetGoalPipeId() == m_GoalPipeId )
						pPipeUser->SetRefPointPos( m_vDestination );
				}
			}
		}
	}
	TBase::ProcessEvent( event, pActInfo );
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGoto::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->point = m_vDestination = GetPortVec3( pActInfo, 2 );
	pData->fValue = GetPortFloat( pActInfo, 3 );
	pData->point2.x = GetPortFloat( pActInfo, 4 );
	Execute( pActInfo, "ACT_GOTO", pData );
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGoto::OnResume( SActivationInfo* pActInfo )
{
	if ( !m_GoalPipeId )
		return;

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
	if ( pEntity )
	{
		IAIObject* pAI = pEntity->GetAI();
		if ( !pAI || pAI->GetAIType() != AIOBJECT_VEHICLE && pAI->GetAIType() != AIOBJECT_PUPPET )
			return;

		IPipeUser* pPipeUser = pAI->CastToIPipeUser();
		if (pPipeUser)
		{
			// First check is the current goal pipe the one which belongs to this node
			if ( pPipeUser->GetGoalPipeId() == m_GoalPipeId )
			{
				// restore his refPoint position
				pPipeUser->SetRefPointPos( m_vDestination );
			}
		}
	}
}

void CFlowNode_AIGoto::Serialize( SActivationInfo* pActInfo, TSerialize ser )
{
	TBase::Serialize(pActInfo,ser);
	ser.Value("m_vDestination",m_vDestination);
}

//////////////////////////////////////////////////////////////////////////
// GOTO - Also sets speed and stance
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGotoSpeedStance::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution") ),
		InputPortConfig<Vec3>( "pos" ),
		InputPortConfig<float>( "distance", 0.0f, _HELP("How accurately to position at the path end"), "EndAccuracy" ),
		InputPortConfig<float>( "StopDistance", 0.0f, _HELP("Stop this amount before the end of the path")),
		InputPortConfig<int>( "run" ,1,_HELP("0=very slow\n1=normal (walk)\n2=fast (run)\n3=very fast (sprint) -ve should reverse") ),
		InputPortConfig<int>( "stance", 4, _HELP("0-prone\n1-crouch\n2-combat\n3-combat alerted\n4-relaxed\n5-stealth"),
		"Stance", _UICONFIG("enum_int:Prone=0,Crouch=1,Combat=2,CombatAlerted=3,Relaxed=4,Stealth=5") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "goto speed stance action" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------

void CFlowNode_AIGotoSpeedStance::OnResume( SActivationInfo* pActInfo )
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
	if ( pEntity )
	{
		IAIObject* pAI = pEntity->GetAI();
		if ( !pAI || pAI->GetAIType() != AIOBJECT_VEHICLE && pAI->GetAIType() != AIOBJECT_PUPPET )
			return;

		IPipeUser* pPipeUser = pAI->CastToIPipeUser();
		if (pPipeUser)
		{
			// First check is the current goal pipe the one which belongs to this node
			if ( pPipeUser->GetGoalPipeId() == m_GoalPipeId )
			{
				// restore his refPoint position
				pPipeUser->SetRefPointPos( m_vDestination );

				// restore his speed and stance
				SetStance( pAI, m_iStance );
				SetSpeed( pAI, m_iSpeed );
			}
		}
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGotoSpeedStance::ProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	if ( m_EntityId && event == eFE_Activate && IsPortActive(pActInfo, 2) )
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
		if ( pEntity )
		{
			m_vDestination = GetPortVec3( pActInfo, 2 );
			IAIObject* pAI = pEntity->GetAI();
			if ( pAI )
			{
				IPipeUser* pPipeUser = pAI->CastToIPipeUser();
				if (pPipeUser)
				{
					// First check is the current goal pipe the one which belongs to this node
					if ( pPipeUser->GetGoalPipeId() == m_GoalPipeId )
					{
						pPipeUser->SetRefPointPos( m_vDestination );
					}
				}
			}
		}
	}
	TBase::ProcessEvent( event, pActInfo );
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGotoSpeedStance::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	m_iSpeed = GetPortInt( pActInfo, 5 );
	m_iStance = GetPortInt( pActInfo, 6 );

	IAIObject* pAI = GetEntity( pActInfo )->GetAI();
	if ( pAI )
	{
		SetStance( pAI, m_iStance );
		SetSpeed( pAI, m_iSpeed );
	}

	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->point = m_vDestination = GetPortVec3( pActInfo, 2 );
	pData->fValue = GetPortFloat( pActInfo, 3 );
	pData->point2.x = GetPortFloat( pActInfo, 4 );
	Execute( pActInfo, "ACT_GOTO", pData );
}

void CFlowNode_AIGotoSpeedStance::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	TBase::Serialize(pActInfo,ser);
	ser.Value("m_vDestination",m_vDestination);
	ser.Value("m_iStance",m_iStance);
	ser.Value("m_iSpeed",m_iSpeed);
}

//////////////////////////////////////////////////////////////////////////
// GOTO EX - Also sets speed and stance with variable input id
//////////////////////////////////////////////////////////////////////////
void CFlowNode_AIGotoSpeedStanceEx::ProcessEvent( IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo )
{
	if (event == eFE_SetEntityId && pActInfo->pEntity && m_EntityId != pActInfo->pEntity->GetId())
	{
		m_EntityId=0;
		m_bExecuted=false;
	}

	TBase::ProcessEvent( event, pActInfo );
}

void CFlowNode_AIGotoSpeedStanceEx::GetConfiguration( SFlowNodeConfig &config )
{
	CFlowNode_AIGotoSpeedStance::GetConfiguration(config);

	config.sDescription = _HELP( "goto speed stance with dynamically changeable input entity!" );
	config.SetCategory(EFLN_WIP);
}

//////////////////////////////////////////////////////////////////////////
// Look At 
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AILookAt::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution") ),
		InputPortConfig<Vec3>( "pos", _HELP("point to look at"), _HELP("Point") ),
		InputPortConfig<Vec3>( "Direction", _HELP("direction to look at") ),
		InputPortConfig<float>( "Duration", _HELP("how long to look at [in seconds] - 0=forever") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "look at point or direction" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AILookAt::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	IEntity* pEntity = GetEntity( pActInfo );
	if ( !pEntity )
		return;

	IAIObject* pAI = pEntity->GetAI();
	if ( !pAI )
	{
		Cancel();
		return;
	}

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
	{
		Cancel();
		return;
	}

	Vec3 pos = GetPortVec3( pActInfo, 2 );
	Vec3 dir = GetPortVec3( pActInfo, 3 );
	if ( !pos.IsZero(0.001f) || !dir.IsZero(0.001f) )
	{
		// activate look at
		m_startTime = 0.f;
		m_bExecuted = true;
		pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
	}
	else
	{
		// reset look at
		pPipeUser->ResetLookAt();
		Finish();
	}
}

bool CFlowNode_AILookAt::OnUpdate( SActivationInfo* pActInfo )
{
	if ( !m_bExecuted )
		return false;

	IEntity* pEntity = GetEntity( pActInfo );
	if ( !pEntity )
		return false;

	IAIObject* pAI = pEntity->GetAI();
	if ( !pAI )
	{
		Cancel();
		return false;
	}

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
	{
		Cancel();
		return false;
	}

	// read inputs
	Vec3 pos = GetPortVec3( pActInfo, 2 );
	Vec3 dir = GetPortVec3( pActInfo, 3 );
	float duration = GetPortFloat( pActInfo, 4 );

	// this is to enable canceling
	m_GoalPipeId = -1;

	if ( !pos.IsZero(0.001f) )
	{
		// look at point
		if ( !m_startTime.GetValue() )
		{
			if ( pPipeUser->SetLookAtPoint(pos, true) )
			{
				if ( duration > 0 )
				{
					m_startTime = gEnv->pTimer->GetFrameStartTime() + duration;
				}
				else
				{
					m_bExecuted = false;
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
					Finish();
				}
			}
		}
		else
		{
			if ( gEnv->pTimer->GetFrameStartTime() >= m_startTime )
			{
				m_bExecuted = false;
				pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
				pPipeUser->ResetLookAt();
				Finish();
			}
			else
				pPipeUser->SetLookAtPoint( pos, true );
		}
	}
	else if ( !dir.IsZero(0.001f) )
	{
		// look at direction
		if ( !m_startTime.GetValue() )
		{
			if ( pPipeUser->SetLookAtDir(dir, true) )
			{
				if ( duration > 0 )
				{
					m_startTime = gEnv->pTimer->GetFrameStartTime() + duration;
				}
				else
				{
					m_bExecuted = false;
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
					Finish();
				}
			}
		}
		else
		{
			if ( gEnv->pTimer->GetFrameStartTime() >= m_startTime )
			{
				m_bExecuted = false;
				pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
				pPipeUser->ResetLookAt();
				Finish();
			}
		}
	}
	else
	{
		// reset look at
		m_bExecuted = false;
		pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
		pPipeUser->ResetLookAt();
		Finish();
	}

	return true;
}

void CFlowNode_AILookAt::OnCancel()
{
	m_bExecuted = true;
	m_pGraph->SetRegularlyUpdated( m_nodeID, false );

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
	if ( pEntity )
	{
		IAIObject* pAI = pEntity->GetAI();
		if ( pAI )
		{
			IPipeUser* pPipeUser = pAI->CastToIPipeUser();
			if (pPipeUser)
				pPipeUser->ResetLookAt();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// body stance controller
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIStance::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig<int>( "stance", 4, _HELP("0-prone\n1-crouch\n2-combat\n3-combat alerted\n4-relaxed\n5-stealth"),
		_HELP("Stance"), _UICONFIG("enum_int:Prone=0,Crouch=1,Combat=2,CombatAlerted=3,Relaxed=4,Stealth=5") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "body stance controller" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIStance::DoProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	IAIObject* pAI = GetEntity( pActInfo )->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if ( !pAIActor )
	{
		ActivateOutput( pActInfo, 0, m_EntityId );
		ActivateOutput( pActInfo, 2, m_EntityId );
		m_bExecuted = false;
		m_bSynchronized = false;
		m_EntityId = 0;
		return;
	}

	bool success = true;
	SOBJECTSTATE* pState = pAIActor->GetState();
	if ( pState )
	{
		int stance = GetPortInt( pActInfo, 1 );
		pState->forceWeaponAlertness = false;
		switch ( stance )
		{
		case 0:
			stance = BODYPOS_PRONE;
			break;
		case 1:
			stance = BODYPOS_CROUCH;
			break;
		case 2:
			stance = BODYPOS_STAND;
			break;
		case 3:
			pState->forceWeaponAlertness = true;
			stance = BODYPOS_STAND;
			break;
		case 4:
			stance = BODYPOS_RELAX;
			break;
		case 5:
			stance = BODYPOS_STEALTH;
			break;
		default:
			success = false;
			break;
		}
		pState->bodystate = stance;

		if(pAI->HasFormation())
		{
			IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
			pData->iValue = stance;
			gEnv->pAISystem->SendSignal(SIGNALFILTER_FORMATION_EXCEPT,1,"OnChangeStance",pAI,pData);
		}
	}

	ActivateOutput( pActInfo, 0, m_EntityId );
	ActivateOutput( pActInfo, success ? 1 : 2, m_EntityId );
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}


//////////////////////////////////////////////////////////////////////////
// unload vehicle
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "Cancel", _HELP("cancels execution") ),
		InputPortConfig<int>( "Seat", _HELP("seat(s) to be unloaded"), NULL, _UICONFIG("enum_int:All=0, AllExpectDriverAndGunner=-3,AllExceptDriver=-1,AllExceptGunner=-2,Driver=1,Gunner=2,Seat03=3,Seat04=4,Seat05=5,Seat06=6,Seat07=7,Seat08=8,Seat09=9,Seat10=10,Seat11=11") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "unloads vehicle" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	assert(0); // This function should not be called!
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::ProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	// Override processing of eFE_Update event
	if ( event != eFE_Update )
	{
		TBase::ProcessEvent( event, pActInfo );
	}
	else
	{
		// regular updates are not needed anymore
		pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );

		m_bNeedsReset = true;
		IEntity* pEntity = GetEntity( pActInfo );
		if ( pEntity )
		{	
			// first check is the owner a vehicle
			IAIObject* pAI = pEntity->GetAI();
			IVehicle* pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle( pEntity->GetId() );
			if ( !pVehicle )
			{
				m_EntityId = 0;
				gEnv->pLog->LogError( "The owner entity '%s' of Vehicle:Unload FG node with ID %d is not a vehicle! ", pEntity->GetName(), pActInfo->myID );
			}
			else
			{
				m_mapPassengers.clear();
				int seatsToUnload = GetPortInt( pActInfo, 2 );
				int numSeats = pVehicle->GetSeatCount();
				switch ( seatsToUnload )
				{
				case -3: // all except the driver and gunner
					for ( int i = 3; i <= numSeats; ++i )
						UnloadSeat( pVehicle, i );
					break;
				case -2: // all except the gunner
					UnloadSeat( pVehicle, 1 );
					for ( int i = 3; i <= numSeats; ++i )
						UnloadSeat( pVehicle, i );
					break;
				case -1: // all except the driver
					for ( int i = 2; i <= numSeats; ++i )
						UnloadSeat( pVehicle, i );
					break;
				case 0: // all
					for ( int i = 1; i <= numSeats; ++i )
						UnloadSeat( pVehicle, i );
					break;
				default:
					if ( seatsToUnload <= numSeats )
					{
						UnloadSeat( pVehicle, seatsToUnload );
					}
					else
					{
						m_EntityId = 0;
						gEnv->pLog->LogError( "Invalid vehicle seat id on Vehicle:Unload FG node with ID %d! ", pActInfo->myID );
					}
				}
				m_numPassengers = m_mapPassengers.size();
				if ( !m_numPassengers )
					Finish();
			}
		}
		else
		{
			m_bExecuted = false;
			m_bSynchronized = false;
		}
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::UnloadSeat( IVehicle* pVehicle, int seatId )
{
	if ( IVehicleSeat* pSeat = pVehicle->GetSeatById(seatId) )
	if ( EntityId passengerId = pSeat->GetPassenger() )
	if ( IEntity* pEntity = gEnv->pEntitySystem->GetEntity(passengerId) )
	if ( IAIObject* pAI = pEntity->GetAI() )
	if ( IAIActor* pAIActor = pAI->CastToIAIActor() )
	if ( IPipeUser* pPipeUser = pAI->CastToIPipeUser() )
	{
		int goalPipeId = gEnv->pAISystem->AllocGoalPipeId();
		pPipeUser->RegisterGoalPipeListener( this, goalPipeId, "CFlowNode_AIUnload::UnloadSeat" );
		m_mapPassengers[ goalPipeId ] = passengerId;

		IAISignalExtraData* pExtraData = gEnv->pAISystem->CreateSignalExtraData();
		pExtraData->iValue = goalPipeId;
		pAIActor->SetSignal( 10, "ACT_EXITVEHICLE", pEntity, pExtraData );
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::OnGoalPipeEvent( IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId )
{
	std::map< int, EntityId >::iterator it = m_mapPassengers.find( goalPipeId );
	if ( it == m_mapPassengers.end() )
		return;

	switch ( event )
	{
	case ePN_Deselected:
	case ePN_Removed:
	case ePN_Finished:
		if ( --m_numPassengers == 0 )
			Finish();
		break;
	case ePN_OwnerRemoved:
		m_mapPassengers.erase(it);
		break;
	case ePN_Suspended:
	case ePN_Resumed:
	case ePN_RefPointMoved:
		break;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUnload::UnregisterEvents()
{
	std::map< int, EntityId >::iterator it, itEnd = m_mapPassengers.end();
	for ( it = m_mapPassengers.begin(); it != itEnd; ++it )
	{
		if ( it->second && gEnv->pEntitySystem )
		{
			if ( gEnv->pEntitySystem )
			if ( IEntity* pEntity = gEnv->pEntitySystem->GetEntity(it->second) )
			if ( IAIObject* pAI = pEntity->GetAI() )
			if ( IPipeUser* pPipeUser = pAI->CastToIPipeUser() )
				pPipeUser->UnRegisterGoalPipeListener( this, it->first );
		}
	}
	m_mapPassengers.clear();
	TBase::UnregisterEvents();
}


//////////////////////////////////////////////////////////////////////////
// speed controller
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISpeed::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig<int>( "run",1 ,_HELP("0=very slow\n1=normal (walk)\n2=fast (run)\n3=very fast (sprint) -ve should reverse") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "movement speed controller" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}


//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AISpeed::DoProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	//	AISIGNAL_EXTRA_DATA data( GetPortInt(pActInfo, 1) );
	//	Execute( pActInfo, "ACT_SPEED", &data );

	// signal version replaced with immediate version to avoid making this action blocking
	IAIObject* pAI = GetEntity( pActInfo )->GetAI();
	if ( !pAI )
	{
		ActivateOutput( pActInfo, 0, m_EntityId );
		ActivateOutput( pActInfo, 2, m_EntityId );
		m_bExecuted = false;
		m_bSynchronized = false;
		m_EntityId = 0;
		return;
	}

	int iSpeed = GetPortInt( pActInfo, 1 );
	SetSpeed(pAI, iSpeed);

	ActivateOutput( pActInfo, 0, m_EntityId );
	ActivateOutput( pActInfo, 1, m_EntityId );
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}



//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_DebugAISpeed::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig<float>( "speed", 0.4f, _HELP("0.0 - stop\n0.1 - very slow (AI lower limit)\n0.2 - slow\n0.4 - walk\n1.0 - run\n1.3 - sprint\n1.6 - AI upper limit") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "This node should be used only for debugging!\nSets the speed urgency (pseudospeed) in the same range as AI would request it from game." );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}


//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_DebugAISpeed::DoProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	IAIActor* pAIActor = CastToIAIActorSafe(GetEntity( pActInfo )->GetAI());
	if ( !pAIActor )
	{
		ActivateOutput( pActInfo, 0, m_EntityId );
		ActivateOutput( pActInfo, 2, m_EntityId );
		m_bExecuted = false;
		m_bSynchronized = false;
		m_EntityId = 0;
		return;
	}

	SOBJECTSTATE* pState = pAIActor->GetState();
	if ( pState )
		pState->fMovementUrgency = GetPortFloat( pActInfo, 1 );

	ActivateOutput( pActInfo, 0, m_EntityId );
	ActivateOutput( pActInfo, 1, m_EntityId );
	m_bExecuted = false;
	m_bSynchronized = false;
	m_EntityId = 0;
}



//////////////////////////////////////////////////////////////////////////
// vehicles convoys linker
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFollow::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution") ),
		InputPortConfig<int>( "leaderId", _HELP("whom to follow") ),
		InputPortConfig<float>( "followDistance", 30.f, _HELP("distance from the followed") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "vehicles convoys linker" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFollow::DoProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	int leaderId = GetPortInt( pActInfo, 2 );
	float followDistance = GetPortFloat( pActInfo, 3 );
	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->fValue = followDistance;
	Execute( pActInfo, "ACT_FOLLOW", pData, leaderId );
}


//////////////////////////////////////////////////////////////////////////
// create formation
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFormation::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig<string>( "fName", _HELP("formation name") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "create formation" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}


//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFormation::DoProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
//	Execute( pActInfo, );

	IEntity* pEntity = GetEntity( pActInfo );
	IAIObject* pAI = pEntity->GetAI();
	if ( pAI == NULL )
	{
		// this can happen apparently!
		Cancel();
		return;
	}

	unsigned short nType=pAI->GetAIType();
	if ( nType == AIOBJECT_VEHICLE )
		pAI->Event( AIEVENT_ENABLE, NULL );
	if ( nType != AIOBJECT_VEHICLE && nType != AIOBJECT_PUPPET )
	{
		// invalid AIObject type
		Cancel();
		return;
	}

	pAI->Event( AIEVENT_ENABLE, NULL );

	const string& input = GetPortString( pActInfo, 1 );
	if(input.empty())
	{
		if(!pAI->ReleaseFormation())
		{
			Cancel();
			return;
		}
	}
	else if ( !pAI->CreateFormation(input) )
	{
		Cancel();
		return;
	}

	// activate notifications
	Finish();
}

//
//-------------------------------------------------------------------------------------------------------------
bool CFlowNode_AIFormation::ExecuteOnAI(SActivationInfo* pActInfo, const char* pSignalText,
																				IAISignalExtraData* pData, IEntity* pEntity, IEntity* pSender )
{
	return false;
/*
	IAIObject* pAI = pEntity->GetAI();
	assert( pAI );
	unsigned short nType=pAI->GetAIType();
	if ( nType == AIOBJECT_VEHICLE )
		pAI->Event( AIEVENT_ENABLE, NULL );
	if ( nType != AIOBJECT_VEHICLE && nType != AIOBJECT_PUPPET )
	{
		// invalid AIObject type
		return false;
	}

	pAI->Event( AIEVENT_ENABLE, NULL );

	const string& input = GetPortString( pActInfo, 1 );
	if(input.empty())
	{
		if(!pAI->ReleaseFormation())
			return false;
	}
	else if ( !pAI->CreateFormation(input) )
		return false;
	// activate notifications
	m_EntityId = pEntity->GetId();
	m_GoalPipeId = gEnv->pAISystem->AllocGoalPipeId();
	IAISignalExtraData* pExtraData = gEnv->pAISystem->CreateSignalExtraData();
	assert( pExtraData );
	pExtraData->iValue = m_GoalPipeId;
	RegisterEvents();

	IAIActor* pAIActor = pAI->CastToIAIActor();
	if ( pAIActor )
		pAIActor->SetSignal( 10, "ACT_DUMMY", pEntity, pExtraData );

	//	// pAISystem->SendAnonymousSignal()
	//	gEnv->pAISystem->SendSignal( SIGNALFILTER_GROUPONLY_EXCEPT, 1, pSignalText, pAI );
	return true;
*/
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFormationJoin::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "Cancel", _HELP("cancels execution (or stops the action)") ),
		InputPortConfig<EntityId>( "ownerId", _HELP("the owner of the formation to be joined"), _HELP("LeaderId") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "Done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "Succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "Fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "joins formation" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

/*
//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIFormationJoin::DoProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
IEntity* pEntity = GetEntity( pActInfo );
if ( !pEntity )
return;
IAIObject* pAI = pEntity->GetAI();
assert( pAI );
if ( pAI->GetType() == AIOBJECT_VEHICLE )
pAI->Event( AIEVENT_ENABLE, NULL );
if ( pAI->GetType() != AIOBJECT_VEHICLE && pAI->GetType() != AIOBJECT_PUPPET )
{
// invalid AIObject type
return;
}
pAI->Event( AIEVENT_ENABLE, NULL );
//	gEnv->pAISystem->SendSignal( SIGNALFILTER_SENDER, 1, "ACT_FOLLOW", pAI );

IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
pData->nID = GetPortEntityId( pActInfo, 1 );
pAI->SetSignal( 1, "ACT_JOINFORMATION", pEntity, pData ); 
}
*/

void CFlowNode_AIFormationJoin::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	Execute( pActInfo, "ACT_JOINFORMATION", NULL, GetPortEntityId( pActInfo, 2 ) );
}

//////////////////////////////////////////////////////////////////////////
// animation controller
//////////////////////////////////////////////////////////////////////////

CFlowNode_AIAnim::~CFlowNode_AIAnim()
{
	OnCancel();
}

void CFlowNode_AIAnim::OnCancelPortActivated( IPipeUser* pPipeUser, SActivationInfo* pActInfo )
{
	if ( m_iMethod == 2 ) // is method == Action?
	{
		pPipeUser->RemoveSubPipe( m_GoalPipeId, true );
		OnCancel(); // only to reuse unregistering code
	}
	else
		pPipeUser->CancelSubPipe( m_GoalPipeId );
}

void CFlowNode_AIAnim::OnCancel()
{
	if ( m_pAGState )
	{
		m_pAGState->RemoveListener( this );

		switch ( m_iMethod )
		{
		case 0:
			if ( !m_bStarted )
				m_pAGState->ClearForcedStates();
			break;
		case 1:
			if ( !m_bStarted )
				m_pAGState->SetInput( "Signal", "none" );
			break;
		case 2:
			m_pAGState->SetInput( "Action", "idle" );
			break;
		}

		m_pAGState = NULL;
		m_queryID = 0;
		m_bStarted = false;
	}
}

void CFlowNode_AIAnim::DestroyedState(IAnimationGraphState*)
{
	m_pAGState = NULL;
	m_queryID = 0;
	m_bStarted = false;
}

void CFlowNode_AIAnim::OnEntityEvent( IEntity* pEntity, SEntityEvent& event )
{
	TBase::OnEntityEvent( pEntity, event );

	if ( event.event == ENTITY_EVENT_POST_SERIALIZE )
	{
		if ( IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_EntityId) )
		if ( m_pAGState = pActor->GetAnimationGraphState() )
		{
			m_pAGState->AddListener( "aianim", this );

			if ( !m_bStarted )
			{
				IAnimationGraphState::InputID inputID = (IAnimationGraphState::InputID) -1;
				switch ( m_iMethod )
				{
				case 1: // use "signal"
					inputID = m_pAGState->GetInputId( "Signal" );
					break;
				case 2: // use "action"
					inputID = m_pAGState->GetInputId( "Action" );
					break;
				}
				if ( inputID != (IAnimationGraphState::InputID) -1 )
				{
					char inputValue[256];
					m_pAGState->GetInput( inputID, inputValue );
					m_pAGState->SetInput( inputID, inputValue, &m_queryID );
				}
			}
			else
			{
				if ( m_iMethod <= 1 )
					m_pAGState->QueryLeaveState( &m_queryID );
				else
					m_pAGState->QueryChangeInput( "Action", &m_queryID );
			}
		}
	}
}

void CFlowNode_AIAnim::Serialize( SActivationInfo* pActInfo, TSerialize ser )
{
	TBase::Serialize( pActInfo, ser );

	ser.Value( "m_bStarted", m_bStarted );
	ser.Value( "m_iMethod", m_iMethod );

	if ( ser.IsReading() && m_pAGState != NULL )
	{
		m_pAGState->RemoveListener( this );
		m_pAGState = NULL;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIAnim::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution (or stops the action)") ),
		InputPortConfig<string>( "animstateEx_name", _HELP("name of animation to be played") ),
		InputPortConfig<int>( "signal", 1, _HELP("which method to use"), _HELP("Method"), _UICONFIG("enum_int:Signal=1,Action=2") ),
		//InputPortConfig<int>( "count", 1, _HELP("OBSOLETE PORT!!! Don't use it! Will be removed soon...") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		OutputPortConfig<EntityId>( "start", _HELP("activated on animation start") ),
		{0}
	};
	config.sDescription = _HELP( "Plays animation" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIAnim::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor( m_EntityId );
	if ( pActor )
	{
		m_pAGState = pActor->GetAnimationGraphState();
		if (!m_pAGState)
			return;
		m_pAGState->AddListener( "aianim", this );

		m_iMethod = GetPortInt( pActInfo, 3 );
		switch ( m_iMethod )
		{
		case 0: // forced state
			m_pAGState->PushForcedState( GetPortString( pActInfo, 2 ), &m_queryID );
			break;
		case 1: // use "signal"
			m_pAGState->SetInput( "Signal", GetPortString( pActInfo, 2 ), &m_queryID );
			break;
		case 2: // use "action"
			m_pAGState->SetInput( "Action", GetPortString( pActInfo, 2 ), &m_queryID );
			break;
		}
	}
	else
	{
		assert(0);
	}

	Execute( pActInfo, "ACT_ANIM" );
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIAnim::QueryComplete( TAnimationGraphQueryID queryID, bool succeeded )
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
	if ( !pEntity )
		return;

	if ( queryID != m_queryID )
		return;

	IAIObject* pAI = pEntity->GetAI();

	if ( succeeded || m_bStarted && m_iMethod == 2 )
	{
		if ( !m_bStarted )
		{
			m_bStarted = true;

			// if "signal" wait to leave current state
			// if "action" wait for input value change
			if ( m_iMethod <= 1 )
				m_pAGState->QueryLeaveState( &m_queryID );
			else
				m_pAGState->QueryChangeInput( "Action", &m_queryID );

			// activate "Start" output port
			SFlowAddress start( m_nodeID, 3, true );
			m_pGraph->ActivatePort( start, m_EntityId );
		}
		else
		{
			if ( pAI )
			{
				IPipeUser* pPipeUser = pAI->CastToIPipeUser();
				if (pPipeUser)
					pPipeUser->RemoveSubPipe( m_GoalPipeId, true );
			}

			if ( m_pAGState )
			{
				m_pAGState->RemoveListener( this );

				m_pAGState = NULL;
				m_queryID = 0;
				m_bStarted = false;
			}
		}
	}
	else if ( pAI )
	{
		IPipeUser* pPipeUser = pAI->CastToIPipeUser();
		if (pPipeUser)
			pPipeUser->CancelSubPipe( m_GoalPipeId );
	}
}



//////////////////////////////////////////////////////////////////////////
// AIAnimEx
//////////////////////////////////////////////////////////////////////////

void CFlowNode_AIAnimEx::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "Sync", _HELP("for synchronization only") ),
		InputPortConfig_Void( "Cancel", _HELP("cancels execution") ),
		InputPortConfig<bool>( "OneShot", true, _HELP("True if it's an one shot animation (signal)\nFalse if it's a looping animation (action)") ),
		InputPortConfig<string>( "animstateEx_Animation", _HELP("name of animation to be played") ),
		InputPortConfig<int>( "Speed", -1, _HELP("speed while approaching the point"),
		NULL, _UICONFIG("enum_int:<ignore>=-1,Very Slow=0,Normal/Walk=1,Fast/Run=2,Very Fast/Sprint=3") ),
		InputPortConfig<int>( "Stance", -1, _HELP("body stance while approaching the point"),
		NULL, _UICONFIG("enum_int:<ignore>=-1,Prone=0,Crouch=1,Combat=2,CombatAlerted=3,Relaxed=4,Stealth=5") ),
		InputPortConfig<Vec3>( "Position", _HELP("where to play the animation") ),
		InputPortConfig<Vec3>( "StartRadius", Vec3(.1f,.1f,.1f), _HELP("tolerance from where to start the animation") ),
		InputPortConfig<Vec3>( "Direction", _HELP("forward dir. while playing the animation") ),
		InputPortConfig<float>( "DirTolerance", 180.0f, _HELP("direction tolerance") ),
		InputPortConfig<float>( "TargetRadius", .05f, _HELP("tolerance where to end the animation") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		OutputPortConfig<EntityId>( "start", _HELP("activated on animation start") ),
		{0}
	};
	config.sDescription = _HELP( "Plays an exactly positioned animation" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory( EFLN_ADVANCED );
}

void CFlowNode_AIAnimEx::DoProcessEvent( IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo )
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
	if ( !pEntity )
		return;

	IAIObject* pAI = pEntity->GetAI();
	if ( !pAI || pAI->GetAIType() != AIOBJECT_PUPPET )
		return;

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return;

	bool bOneShot = GetPortBool( pActInfo, 2 );
	const string& sAnimation = GetPortString( pActInfo, 3 );
	m_iSpeed = GetPortInt( pActInfo, 4 );
	m_iStance = GetPortInt( pActInfo, 5 );
	m_vPos = GetPortVec3( pActInfo, 6 );
	if ( m_vPos.IsZero() )
		m_vPos = pAI->GetPos();
	Vec3 vStartRadius = GetPortVec3( pActInfo, 7 );
	m_vDir = GetPortVec3( pActInfo, 8 );
	if ( m_vDir.IsZero() )
		m_vDir = pAI->GetMoveDir();
	float fDirTolerance = GetPortFloat( pActInfo, 9 );
	float fTargetRadius = GetPortFloat( pActInfo, 10 );

	// set the stance and speed
	if ( m_iStance >= 0 )
		SetStance( pAI, m_iStance );
	if ( m_iSpeed >= 0 )
		SetSpeed( pAI, m_iSpeed );

	// set the refPoint position and orientation
	pPipeUser->SetRefPointPos( m_vPos, m_vDir );

	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->iValue2 = bOneShot ? 1 : 0;
	pData->SetObjectName( sAnimation );
	pData->point = vStartRadius;
	pData->fValue = fDirTolerance;
	pData->point2.x = fTargetRadius;

	Execute( pActInfo, "ACT_ANIMEX", pData );
}

void CFlowNode_AIAnimEx::OnResume( IFlowNode::SActivationInfo* pActInfo )
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
	if ( !pEntity )
		return;

	IAIObject* pAI = pEntity->GetAI();
	if ( !pAI || pAI->GetAIType() != AIOBJECT_PUPPET )
		return;

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return;

	// First check is the current goal pipe the one which belongs to this node
	if ( pPipeUser->GetGoalPipeId() == m_GoalPipeId )
	{
		// restore the refPoint position
		pPipeUser->SetRefPointPos( m_vPos, m_vDir );

		// restore the speed and stance
		if ( m_iStance >= 0 )
			SetStance( pAI, m_iStance );
		if ( m_iSpeed >= 0 )
			SetSpeed( pAI, m_iSpeed );
	}
}

void CFlowNode_AIAnimEx::OnGoalPipeEvent( IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId )
{
	TBase::OnGoalPipeEvent( pPipeUser, event, goalPipeId );
	if ( m_GoalPipeId == goalPipeId && event == ePN_AnimStarted )
	{
		// activate "Start" output port
		SFlowAddress start( m_nodeID, 3, true );
		m_pGraph->ActivatePort( start, m_EntityId );
	}
}


//////////////////////////////////////////////////////////////////////////
// grab object
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGrabObject::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution") ),
		InputPortConfig<EntityId>( "objectId", _HELP("object to be grabbed") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "grabs an object" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIGrabObject::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	Execute( pActInfo, "ACT_GRAB_OBJECT", NULL, GetPortEntityId(pActInfo, 2) );
}


//////////////////////////////////////////////////////////////////////////
// drop object
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIDropObject::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution") ),
		InputPortConfig<Vec3>( "impulse", _HELP("use for throwing") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "drops grabbed object" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIDropObject::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->point = GetPortVec3( pActInfo, 2 );
	Execute( pActInfo, "ACT_DROP_OBJECT", pData );
}


//////////////////////////////////////////////////////////////////////////
// draws weapon
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponDraw::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "draws his weapon" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponDraw::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	Execute( pActInfo, "ACT_WEAPONDRAW" );
}


//////////////////////////////////////////////////////////////////////////
// holsters weapon
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponHolster::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "holsters his weapon" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponHolster::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	Execute( pActInfo, "ACT_WEAPONHOLSTER" );
}

//////////////////////////////////////////////////////////////////////////
// Shoot at given point
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIShootAt::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution") ),
		InputPortConfig<Vec3>( "targetPos", _HELP("Position to shoot or aim at (dynamically updated). If TargetId is specified it will override this parameter."), _HELP("TargetPos") ),
		InputPortConfig<float>( "time", 1.0f, _HELP("How long to shoot"), _HELP("Duration") ),
		InputPortConfig<int>( "shots", -1, _HELP("How many shots to do"), _HELP("ShotsCount") ),
		InputPortConfig<bool>( "AimOnly", _HELP("Only aim at target position instead of shooting") ),
		InputPortConfig<EntityId>( "TargetId", _HELP("Entity to shoot or aim at (dynamically updated), uses the position the AI system aims at if the entity has AI object. If not set TargetPos is used instead.") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		OutputPortConfig_Void("OnShoot", _HELP("shot is done") ),
		{0}
	};
	config.sDescription = _HELP( "shoots at point" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIShootAt::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();

	// Choose the shoot at position based on the targetPos or targetId.
	EntityId	targetId = GetPortEntityId( pActInfo, 6 );
	IEntity*	pTargetEntity = 0;
	if(targetId)
		pTargetEntity = gEnv->pEntitySystem->GetEntity(targetId);
	if(pTargetEntity)
	{
		// Use 'targetId'
		if(pTargetEntity->GetAI())
			pData->point = pTargetEntity->GetAI()->GetPos();
		else
			pData->point = pTargetEntity->GetPos();
	}
	else
	{
		// Use 'targetPos'
		pData->point = GetPortVec3( pActInfo, 2 );
	}
	pData->fValue = GetPortFloat( pActInfo, 3 );
	// if in relaxed stance now - change to combat before shooting; no animations for shooting in relaxed stance
	IAIActor* pAIActor = CastToIAIActorSafe(GetEntity( pActInfo )->GetAI());
	if (pAIActor && pAIActor->GetProxy())
	{
		SAIBodyInfo bodyInfo;
		pAIActor->GetProxy()->QueryBodyInfo(bodyInfo);
		if(bodyInfo.stance == STANCE_RELAXED)
		{
			SOBJECTSTATE* pState(pAIActor->GetState());
			if(pState)
				pState->bodystate = BODYPOS_STAND;
		}
	}

	if ( GetPortBool(pActInfo, 5) )
		Execute( pActInfo, "ACT_AIMAT", pData );
	else
	{
		Execute( pActInfo, "ACT_SHOOTAT", pData );
		// prepare shoots counting
		m_ShotsNumber = 	GetPortInt( pActInfo, 4 );
		IEntity* pEntity = GetEntity( pActInfo );
		if ( pEntity )
		{
			IAIObject* pAI = pEntity->GetAI();
			if ( pAI && pAI->GetProxy() )
			{
				if (IWeapon * pWeapon = pAI->GetProxy()->GetCurrentWeapon())
				{
					pWeapon->AddEventListener(this, __FUNCTION__);

					IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pAI->GetEntityID());
					if (pActor)
					{
						m_weaponId=0;
						if (IVehicle* pVehicle = pActor->GetLinkedVehicle())
							m_weaponId=pVehicle->GetCurrentWeaponId(pActor->GetEntityId());
						else  
						{
							IInventory *pInventory = pActor->GetInventory();
							if (pInventory)    
								m_weaponId=pInventory->GetCurrentItem(); 
						}
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CFlowNode_AIShootAt::UnregisterWithWeapon()
{
	if (m_weaponId)
	{
		IItem *pItem=gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_weaponId);
		if (pItem && pItem->GetIWeapon())
			pItem->GetIWeapon()->RemoveEventListener(this);

		m_weaponId=0;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIShootAt::ProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	if ( m_EntityId && event == eFE_Activate && IsPortActive(pActInfo, 2) )
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
		if ( pEntity )
		{
			// Choose the shoot at position based on the targetPos or targetId.
			Vec3 target;
			EntityId	targetId = GetPortEntityId( pActInfo, 6 );
			IEntity*	pTargetEntity = 0;
			if(targetId)
				pTargetEntity = gEnv->pEntitySystem->GetEntity(targetId);
			if(pTargetEntity)
			{
				// Use 'targetId'
				if(pTargetEntity->GetAI())
					target = pTargetEntity->GetAI()->GetPos();
				else
					target = pTargetEntity->GetPos();
			}
			else
			{
				// Use 'targetPos'
				target = GetPortVec3( pActInfo, 2 );
			}

			IAIObject* pAI = pEntity->GetAI();
			if ( pAI )
			{
				Vec3 dir = target - pAI->GetPos();
				dir.NormalizeSafe();
				pAI->SetViewDir( dir );
				IAIObject* pAIObject = pAI->GetSpecialAIObject( "refpoint" );
				if ( pAIObject )
					pAIObject->SetPos( target );
				pAIObject = pAI->GetSpecialAIObject( "lookat_target" );
				if ( pAIObject )
					pAIObject->SetPos( target );
			}
		}
	}
	TBase::ProcessEvent( event, pActInfo );
}

void CFlowNode_AIShootAt::OnCancel()
{
	UnregisterWithWeapon();

	if ( m_EntityId )
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId );
		if ( pEntity )
		{
			IAIObject* pAI = pEntity->GetAI();
			if ( pAI )
			{
				IPipeUser* pPipeUser = pAI->CastToIPipeUser();
				if (pPipeUser)
				{
					pPipeUser->ResetLookAt();
					pPipeUser->SetFireMode( FIREMODE_OFF );
					//if ( pPipeUser->GetFireMode() == FIREMODE_AIM )
					//	pPipeUser->SetFireMode( FIREMODE_BURST, false );
				}
			}
		}
	}
}

void CFlowNode_AIShootAt::UnregisterEvents()
{
	UnregisterWithWeapon();

	if ( m_UnregisterEntityId && !m_EntityId )
		m_EntityId = m_UnregisterEntityId;

	TBase::UnregisterEvents();
}



//
//----------------------------------------------------------------------------------------------------------
void CFlowNode_AIShootAt::OnDropped(IWeapon *pWeapon, EntityId actorId)
{
	UnregisterWithWeapon();

	if ( !m_EntityId )
		return;
	if ( IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId ) )
	{
		if ( IAIObject* pAI = pEntity->GetAI() )
		{
			IPipeUser* pPipeUser = pAI->CastToIPipeUser();
			if (pPipeUser)
				pPipeUser->CancelSubPipe( m_GoalPipeId );
		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
void CFlowNode_AIShootAt::OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType,
																	const Vec3 &pos, const Vec3 &dir, const Vec3 &vel)
{
	if ( !m_EntityId || !m_GoalPipeId )
		return;

	if(m_ShotsNumber>0)
	{
		if((--m_ShotsNumber) == 0)
		{
			if ( IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_EntityId ) )
			{
				if ( IAIObject* pAI = pEntity->GetAI() )
				{
					IPipeUser* pPipeUser = pAI->CastToIPipeUser();
					if (pPipeUser)
						pPipeUser->RemoveSubPipe( m_GoalPipeId, true );
				}
			}
		}
	}

	SFlowAddress done( m_nodeID, 3, true );
	m_pGraph->ActivatePort( done, true );
}



//////////////////////////////////////////////////////////////////////////
// Uses an object
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUseObject::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution") ),
		InputPortConfig<EntityId>( "objectId", "Entity ID of the object which should be used by the agent" ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "Uses an object" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIUseObject::DoProcessEvent( EFlowEvent event, SActivationInfo* pActInfo )
{
	Execute( pActInfo, "ACT_USEOBJECT", NULL, GetPortEntityId(pActInfo, 2) );
}


//////////////////////////////////////////////////////////////////////////
// select weapon
//////////////////////////////////////////////////////////////////////////

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponSelect::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "cancel", _HELP("cancels execution") ),
		InputPortConfig<string>( "weapon" ,_HELP("name of weapon to be selected"), 0, _UICONFIG("enum_global:weapon") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};
	config.sDescription = _HELP( "selects specified weapon" );
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.SetCategory(EFLN_APPROVED);
}


//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIWeaponSelect::DoProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	pData->SetObjectName( GetPortString(pActInfo, 2) );
	Execute( pActInfo, "ACT_WEAPONSELECT", pData );
}




//////////////////////////////////////////////////////////////////////////
// Makes ai enter specified seat of specified vehicle
//////////////////////////////////////////////////////////////////////////
void CFlowNode_AIEnterVehicle::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "sink", _HELP("for synchronization only"), _HELP("Sync") ),
		InputPortConfig_Void( "Cancel", _HELP("cancels execution") ),
		InputPortConfig<EntityId>( "VehicleId", _HELP("vehicle to be entered") ),
		InputPortConfig<int>( "SeatNumber", _HELP("seat to be entered"), _HELP("Seat"), _UICONFIG("enum_int:Any=0,Driver=1,Gunner=2,Seat03=3,Seat04=4,Seat05=5,Seat06=6,Seat07=7,Seat08=8,Seat09=9,Seat10=10,Seat11=11") ),
		InputPortConfig<bool>( "Fast", _HELP("skip approaching and playing entering animation") ),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};

	config.sDescription = _HELP( "makes ai enter specified seat of specified vehicle" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIEnterVehicle::DoProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	if (!pActInfo->pEntity)
		return;

	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();

	int seatId = GetPortInt( pActInfo, IN_SEAT );
	EntityId vehicleId = GetPortEntityId( pActInfo, IN_VEHICLEID );
	bool fast = GetPortBool( pActInfo, IN_FAST );

	if (!fast)
	{
		ICVar* pTransitionsVar = gEnv->pConsole->GetCVar("v_transitionAnimations");
		if (pTransitionsVar && pTransitionsVar->GetIVal() == 0)
			fast = true;

		if (IVehicle* pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(vehicleId))
		{
			if (IVehicleSeat* pSeat = pVehicle->GetSeatById(seatId))
			{
				if (!pSeat->IsAnimGraphStateExisting(pActInfo->pEntity->GetId()))
					fast = true;
			}
		}
	}

	pData->fValue = seatId;
	pData->nID = vehicleId;
	pData->iValue2 = fast;

	Execute( pActInfo, "ACT_ENTERVEHICLE", pData );
}

//////////////////////////////////////////////////////////////////////////
// Makes ai exit vehicle
//////////////////////////////////////////////////////////////////////////
void CFlowNode_AIExitVehicle::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "Synk", _HELP("for synchronization only") ),
		InputPortConfig_Void( "Cancel", _HELP("cancels execution") ),
		InputPortConfig<bool>("StayInVehicleIfFailed", false, _HELP("Check exit position and stay in vehicle if failed")),
		{0}
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<EntityId>( "done", _HELP("action done") ),
		OutputPortConfig<EntityId>( "succeed", _HELP("action done successfully") ),
		OutputPortConfig<EntityId>( "fail", _HELP("action failed") ),
		{0}
	};

	config.sDescription = _HELP( "makes ai exit a vehicle" );
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_AIExitVehicle::DoProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	if(GetPortBool( pActInfo, 2))
	{
		if(IEntity* pEntity = GetEntity(pActInfo))
			if(IActor * pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(m_EntityId))
				if(IVehicle* pVehicle = pActor->GetLinkedVehicle())
					if(IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(m_EntityId))				
					{
						Matrix34 worldTM = pSeat->GetExitTM(pActor, false);
						if(pSeat->TestExitPosition(pActor, worldTM.GetTranslation(), NULL))
						{
							Execute( pActInfo, "ACT_EXITVEHICLE" );
							return;
						}
					}
					ActivateOutput( pActInfo, 2, m_EntityId);
	}
	else
	Execute( pActInfo, "ACT_EXITVEHICLE" );
}

//-------------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------------
class CFlowNode_AILookAtPoint : public CFlowNode_AILookAt
{
public:
	CFlowNode_AILookAtPoint( SActivationInfo * pActInfo ) : CFlowNode_AILookAt( pActInfo ) {}

	void GetConfiguration( SFlowNodeConfig &config )
	{
		CFlowNode_AILookAt::GetConfiguration( config );
		config.SetCategory( EFLN_OBSOLETE );
	}
};

//-------------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------------
class CFlowNode_AIEnterVehicle_Old : public CFlowNode_AIEnterVehicle
{
public:
	CFlowNode_AIEnterVehicle_Old( SActivationInfo * pActInfo ) : CFlowNode_AIEnterVehicle( pActInfo ) {}

	void GetConfiguration( SFlowNodeConfig &config )
	{
		CFlowNode_AIEnterVehicle::GetConfiguration( config );
		config.SetCategory( EFLN_OBSOLETE );
	}
};

//-------------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------------
class CFlowNode_AIUnload_Old : public CFlowNode_AIUnload
{
public:
	CFlowNode_AIUnload_Old( SActivationInfo * pActInfo ) : CFlowNode_AIUnload( pActInfo ) {}

	void GetConfiguration( SFlowNodeConfig &config )
	{
		CFlowNode_AIUnload::GetConfiguration( config );
		config.SetCategory( EFLN_OBSOLETE );
	}
};

//-------------------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------------------

REGISTER_FLOW_NODE( "AI:AIExecute", CFlowNode_AIExecute )
REGISTER_FLOW_NODE( "AI:AISignal", CFlowNode_AISignal )
REGISTER_FLOW_NODE( "AI:AISetState", CFlowNode_AISetState )
REGISTER_FLOW_NODE( "AI:AIModifyStates", CFlowNode_AIModifyStates )
REGISTER_FLOW_NODE( "AI:AICheckStates", CFlowNode_AICheckStates )
REGISTER_FLOW_NODE( "AI:AIGoto", CFlowNode_AIGoto )
REGISTER_FLOW_NODE( "AI:AIGotoSpeedStance", CFlowNode_AIGotoSpeedStance )
REGISTER_FLOW_NODE( "AI:AIGotoSpeedStanceEx", CFlowNode_AIGotoSpeedStanceEx )
REGISTER_FLOW_NODE( "AI:AILookAtPoint", CFlowNode_AILookAtPoint )
REGISTER_FLOW_NODE( "AI:AILookAt", CFlowNode_AILookAt )
REGISTER_FLOW_NODE( "AI:AIStance", CFlowNode_AIStance )
REGISTER_FLOW_NODE( "AI:AISpeed", CFlowNode_AISpeed )
REGISTER_FLOW_NODE( "AI:DebugAISpeed", CFlowNode_DebugAISpeed )
REGISTER_FLOW_NODE( "AI:AIAnim", CFlowNode_AIAnim )
REGISTER_FLOW_NODE( "AI:AIAnimEx", CFlowNode_AIAnimEx )
REGISTER_FLOW_NODE( "AI:AIFollow", CFlowNode_AIFollow )
REGISTER_FLOW_NODE( "AI:AIFollowPath", CFlowNode_AIFollowPath )
REGISTER_FLOW_NODE( "AI:AIFollowPathSpeedStance", CFlowNode_AIFollowPathSpeedStance )
REGISTER_FLOW_NODE( "AI:AIFormation", CFlowNode_AIFormation )
REGISTER_FLOW_NODE( "AI:AIFormationJoin", CFlowNode_AIFormationJoin )
REGISTER_FLOW_NODE( "AI:AIUnload", CFlowNode_AIUnload_Old )
REGISTER_FLOW_NODE( "AI:AISetCharacter", CFlowNode_AISetCharacter )
REGISTER_FLOW_NODE( "AI:AIGrabObject", CFlowNode_AIGrabObject )
REGISTER_FLOW_NODE( "AI:AIDropObject", CFlowNode_AIDropObject )
REGISTER_FLOW_NODE( "AI:AIWeaponDraw", CFlowNode_AIWeaponDraw )
REGISTER_FLOW_NODE( "AI:AIWeaponHolster", CFlowNode_AIWeaponHolster )
REGISTER_FLOW_NODE( "AI:AIShootAt", CFlowNode_AIShootAt )
REGISTER_FLOW_NODE( "AI:AIUseObject", CFlowNode_AIUseObject )
REGISTER_FLOW_NODE( "AI:AIWeaponSelect", CFlowNode_AIWeaponSelect )
REGISTER_FLOW_NODE( "AI:AIEnterVehicle",CFlowNode_AIEnterVehicle_Old )
REGISTER_FLOW_NODE( "AI:AIAlertMe",CFlowNode_AISignalAlerted )
REGISTER_FLOW_NODE( "Vehicle:Enter", CFlowNode_AIEnterVehicle )
REGISTER_FLOW_NODE( "Vehicle:Unload", CFlowNode_AIUnload )
REGISTER_FLOW_NODE( "Vehicle:Exit",CFlowNode_AIExitVehicle )
