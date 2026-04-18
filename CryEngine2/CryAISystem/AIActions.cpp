#include "StdAfx.h"

#include <ISystem.h>
#include <ICryPak.h>
#include <IEntitySystem.h>
#include <IEntity.h>

#include "CAISystem.h"
#include "PipeUser.h"
#include "AIActions.h"


///////////////////////////////////////////////////
// CAnimationAction
///////////////////////////////////////////////////

// returns the goal pipe which executes this AI Action
IGoalPipe* CAnimationAction::GetGoalPipe() const
{
	// create the goal pipe here
	IGoalPipe* pPipe = GetAISystem()->CreateGoalPipe( string(bSignaledAnimation ? "$$" : "@") + sAnimationName );

	GoalParameters params;
	IGoalPipe::EGroupType grouped(IGoalPipe::GT_NOGROUP);

	if ( bExactPositioning )
	{
		if ( iApproachStance != -1 )
		{
			params.fValue = iApproachStance;
			pPipe->PushGoal(AIOP_BODYPOS, false, grouped, params);
			params.fValue = 0;

			grouped = IGoalPipe::GT_GROUPWITHPREV;
		}

		if ( fApproachSpeed != -1 )
		{
			params.fValue = fApproachSpeed;
			params.nValue = 0;
			pPipe->PushGoal(AIOP_RUN, false, grouped, params);
			params.fValue = 0;
			params.nValue = 0;

			grouped = IGoalPipe::GT_GROUPWITHPREV;
		}

		params.szString = "refpoint";
		pPipe->PushGoal( AIOP_LOCATE, false, grouped, params );
		params.szString.clear();

		params.bValue = bSignaledAnimation;
		params.szString = sAnimationName;
		params.m_vPosition.x = fStartRadiusTolerance;
		params.m_vPosition.y = fDirectionTolerance;
		params.m_vPosition.z = fTargetRadiusTolerance;
		params.m_vPositionAux = vApproachPos; // vAnimationPos;
		pPipe->PushGoal( AIOP_ANIMTARGET, false, IGoalPipe::GT_GROUPWITHPREV, params );
		params.bValue = false;
		params.szString.clear();
		params.m_vPosition.zero();

		params.szString = "animtarget";
		pPipe->PushGoal( AIOP_LOCATE, false, IGoalPipe::GT_GROUPWITHPREV, params );
		params.szString.clear();

		params.fValue = 1.5f;		// end distance
		params.fValueAux = 1.0f;	// end accuracy
		params.nValue = AILASTOPRES_USE;
		pPipe->PushGoal( AIOP_APPROACH, true, IGoalPipe::GT_GROUPWITHPREV, params );
		params.fValue = 0;
		params.fValueAux = 0;
		params.nValue = 0;

		params.fValue = 0;
		params.nValue = IF_LASTOP_SUCCEED;
		params.nValueAux = 2;
		pPipe->PushGoal( AIOP_BRANCH, false, IGoalPipe::GT_NOGROUP, params );
		params.nValue = 0;
		params.nValueAux = 0;

			params.fValue = 1;
			params.szString = "CANCEL_CURRENT";
			params.nValue = 0;
			pPipe->PushGoal( AIOP_SIGNAL, true, IGoalPipe::GT_NOGROUP, params );
			params.fValue = 0;
			params.szString.clear();
			params.nValue = 0;

			params.fValue = 0;
			params.nValue = BRANCH_ALWAYS;
			params.nValueAux = 1;
			pPipe->PushGoal( AIOP_BRANCH, false, IGoalPipe::GT_NOGROUP, params );
			params.nValue = 0;
			params.nValueAux = 0;

		params.fValue = 1;
		params.szString = "OnAnimTargetReached";
		params.nValue = 0;
		pPipe->PushGoal( AIOP_SIGNAL, true, IGoalPipe::GT_NOGROUP, params );
		params.fValue = 0;
		params.szString.clear();
		params.nValue = 0;

	}
	else
	{
		params.nValue = bSignaledAnimation ? AIANIM_SIGNAL : AIANIM_ACTION;
		params.szString = sAnimationName;
		pPipe->PushGoal( AIOP_ANIMATION, false, IGoalPipe::GT_NOGROUP, params );
		params.bValue = false;
		params.szString.clear();
	}

	return pPipe;
}


///////////////////////////////////////////////////
// CActiveAction represents single active CAIAction or CAnimationAction
///////////////////////////////////////////////////
void CActiveAction::EndAction()
{
	// end action only if it isn't canceled already
	if ( m_iThreshold >= 0 && !m_bDeleted )
	{
		m_bDeleted = true;

		// modify the states
		if ( !m_nextUserState.empty() )
		{
			GetAISystem()->ModifySmartObjectStates( m_pUserEntity, m_nextUserState );
			m_nextUserState.clear();
		}
		if ( m_pObjectEntity && !m_nextObjectState.empty() )
		{
			GetAISystem()->ModifySmartObjectStates( m_pObjectEntity, m_nextObjectState );
			m_nextObjectState.clear();
		}
	}
}

void CActiveAction::CancelAction()
{
	// cancel action only if it isn't ended already
	if ( !m_bDeleted && m_iThreshold >= 0 )
	{
		m_iThreshold = -1;

		// modify the states
		if ( !m_canceledUserState.empty() )
		{
			GetAISystem()->ModifySmartObjectStates( m_pUserEntity, m_canceledUserState );
			m_canceledUserState.clear();
		}
		if ( m_pObjectEntity && !m_canceledObjectState.empty() )
		{
			GetAISystem()->ModifySmartObjectStates( m_pObjectEntity, m_canceledObjectState );
			m_canceledObjectState.clear();
		}
	}
}

bool CActiveAction::AbortAction()
{
	// abort action only if it isn't aborted, ended or canceled already
	if ( m_bDeleted || m_iThreshold < 0 )
		return false;

	IAIObject* pUser = m_pUserEntity->GetAI();
	if ( m_pFlowGraph && pUser )
	{
		CPipeUser* pPipeUser = pUser->CastToCPipeUser();
		if (pPipeUser)
		{
			// enumerate nodes and make all AI:ActionAbort nodes RegularlyUpdated
			bool bHasAbort = false;
			int idNode = 0;
			TFlowNodeTypeId nodeTypeId;
			TFlowNodeTypeId actionAbortTypeId = gEnv->pFlowSystem->GetTypeId("AI:ActionAbort");
			while ( (nodeTypeId = m_pFlowGraph->GetNodeTypeId( idNode )) != InvalidFlowNodeTypeId )
			{
				if ( nodeTypeId == actionAbortTypeId )
				{
					m_pFlowGraph->SetRegularlyUpdated( idNode, true );
					bHasAbort = true;
				}
				++idNode;
			}

			if ( bHasAbort )
			{
				pPipeUser->AbortActionPipe( m_idGoalPipe );
				return true;
			}
		}
	}
	CancelAction();
	return false;
}

bool CActiveAction::operator == ( const CActiveAction& other ) const
{
	return
		m_pFlowGraph == other.m_pFlowGraph &&
		m_pUserEntity == other.m_pUserEntity &&
		m_pObjectEntity == other.m_pObjectEntity &&
		m_SuspendCounter == other.m_SuspendCounter;
}


///////////////////////////////////////////////////
// CAIActionManager keeps track of all AIActions
///////////////////////////////////////////////////

CAIActionManager::CAIActionManager()
{
}

CAIActionManager::~CAIActionManager()
{
	Reset();
	m_ActionsLib.clear();
}

// suspends all active AI Actions in which the entity is involved
// it's safe to pass pEntity == NULL
int CAIActionManager::SuspendActionsOnEntity( IEntity* pEntity, int goalPipeId, const IAIAction* pAction, bool bHighPriority, int& numHigherPriority )
{
	// count how many actions are suspended so we can prevent execution of too many actions at same time
	int count = 0;
	numHigherPriority = 0;

	if ( pEntity )
	{
		// if entity is registered as CPipeUser...
		IAIObject* pAI = pEntity->GetAI();
		if(pAI)
		{
			CPipeUser* pPipeUser = pAI->CastToCPipeUser();
			if ( pPipeUser) 
			{
				GetAISystem()->Record( pAI, IAIRecordable::E_ACTIONSUSPEND, "" );

				// insert the pipe
				IGoalPipe* pPipe = pAction->GetGoalPipe();
				if ( pPipe )
				{
					pPipe = pPipeUser->InsertSubPipe( bHighPriority ? AIGOALPIPE_HIGHPRIORITY : 0, pPipe->GetName(), NULL, goalPipeId );
					if ( pPipe != NULL && ((CGoalPipe*)pPipe)->GetSubpipe() == NULL )
					pPipeUser->SetRefPointPos( pAction->GetAnimationPos(), pAction->GetAnimationDir() );
				}
				else
				{
					pPipe = pPipeUser->InsertSubPipe( bHighPriority ? AIGOALPIPE_HIGHPRIORITY : 0, "_action_", NULL, goalPipeId );
					if ( !pPipe )
					{
						// create the goal pipe here
						GoalParameters params;
						pPipe = GetAISystem()->CreateGoalPipe( "_action_" );
						params.fValue = 0.1f;
						pPipe->PushGoal( AIOP_TIMEOUT, true, IGoalPipe::GT_NOGROUP, params );
						params.fValue = 0;
						params.nValue = BRANCH_ALWAYS;
						params.nValueAux = -1;
						pPipe->PushGoal( AIOP_BRANCH, true, IGoalPipe::GT_NOGROUP, params );
						
						// try to insert the pipe now once again
						pPipe = pPipeUser->InsertSubPipe( bHighPriority ? AIGOALPIPE_HIGHPRIORITY : 0, "_action_", NULL, goalPipeId );
					}
				}

				// pPipe might be NULL if the puppet is dead
				if ( !pPipe )
					return 100;
				numHigherPriority = ((CGoalPipe*)pPipe)->CountSubpipes();

				// set debug name
				string name('[');
				if ( bHighPriority )
					name += '!';
				const char* szName = pAction->GetName();
				if ( !szName )
					name += pPipe->GetName();
				else
					name += szName;
				name += ']';
				pPipe->SetDebugName( name );

				// and watch it
				pPipeUser->RegisterGoalPipeListener( this, goalPipeId, "CAIActionManager::SuspendActionsOnEntity" );
			}
		}

		TActiveActions::iterator it, itEnd = m_ActiveActions.end();
		for ( it = m_ActiveActions.begin(); it != itEnd; ++it )
		{
			if ( it->m_pUserEntity == pEntity /*|| it->m_pObjectEntity == pEntity*/ )
			{
				if ( bHighPriority == true || it->m_bHighPriority == false )
				{
					if ( !it->m_SuspendCounter )
					{
						IFlowGraph* pGraph = it->GetFlowGraph();
						if ( pGraph )
							pGraph->SetSuspended( true );
					}
					++it->m_SuspendCounter;
				}
				++count;
			}
		}
	}

	// returns the number of active actions
	return count+1;
}

// resumes all active AI Actions in which the entity is involved
// (resuming depends on it how many times it was suspended)
// note: it's safe to pass pEntity == NULL
void CAIActionManager::ResumeActionsOnEntity( IEntity* pEntity, int goalPipeId )
{
	if ( pEntity )
	{
		if ( goalPipeId )
		{
			// if entity is registered as CPipeUser...
			IAIObject* pAI = pEntity->GetAI();
			if ( pAI )
			{
				CPipeUser* pPipeUser = pAI->CastToCPipeUser();
				if(pPipeUser)
				{
					GetAISystem()->Record( pAI, IAIRecordable::E_ACTIONRESUME, "" );

					// ...stop watching it...
					pPipeUser->UnRegisterGoalPipeListener( this, goalPipeId );

					// ...remove "_action_" goal pipe
					pPipeUser->RemoveSubPipe( goalPipeId );
				}
			}
		}

		TActiveActions::iterator it, itEnd = m_ActiveActions.end();
		for ( it = m_ActiveActions.begin(); it != itEnd; ++it )
			if ( it->m_pUserEntity == pEntity /*|| it->m_pObjectEntity == pEntity*/ )
			{
				--it->m_SuspendCounter;
				if ( !it->m_SuspendCounter )
				{
					IFlowGraph* pGraph = it->GetFlowGraph();
					if ( pGraph )
						pGraph->SetSuspended( false );
				}
			}
	}
}

// aborts specific AI Action (specified by goalPipeId) or all AI Actions (goalPipeId == 0) in which pEntity is a user
void CAIActionManager::AbortActionsOnEntity( IEntity* pEntity, int goalPipeId /*=0*/ )
{
	AIAssert( pEntity );
	if ( !pEntity )
		return;

	TActiveActions::iterator it, itEnd = m_ActiveActions.end();
	for ( it = m_ActiveActions.begin(); it != itEnd; ++it )
		if ( (goalPipeId || !it->m_bHighPriority) && it->m_pUserEntity == pEntity )
			if ( !goalPipeId || goalPipeId == it->m_idGoalPipe )
				it->AbortAction();
}

// implementation of IGoalPipeListener
void CAIActionManager::OnGoalPipeEvent( IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId )
{
	TActiveActions::iterator it, itEnd = m_ActiveActions.end();
	for ( it = m_ActiveActions.begin(); it != itEnd; ++it )
	{
		CActiveAction& action = *it;
		if ( action.m_idGoalPipe == goalPipeId )
		{
			if ( event == ePN_Deselected )
			{
				action.CancelAction();
				break;
			}
			if ( !action.GetFlowGraph() )
			{
				// additional stuff for animations actions
				if ( event == ePN_Resumed )
				{
					// restore reference point
					pPipeUser->SetRefPointPos( action.GetAnimationPos(), action.GetAnimationDir() );
					break;
				}
				else if ( event == ePN_Finished )
				{
					action.EndAction();
					break;
				}
			}
		}
	}
}

// stops all active actions
void CAIActionManager::Reset()
{
	TActiveActions::iterator it, itEnd = m_ActiveActions.end();
	for ( it = m_ActiveActions.begin(); it != itEnd; ++it )
	{
		CActiveAction& action = *it;
		if ( action.GetFlowGraph() )
			action.GetFlowGraph()->SetAIAction(0);
		if ( action.GetUserEntity() )
		{
			IAIObject* pAI = action.GetUserEntity()->GetAI();
			if ( pAI )
			{
				CPipeUser* pPipeUser = pAI->CastToCPipeUser();
				if (pPipeUser)
					pPipeUser->UnRegisterGoalPipeListener( this, action.m_idGoalPipe );
			}
		}
	}
	m_ActiveActions.clear();
}

// returns an existing AI Action by its name specified in the rule
// or creates a new temp. action for playing the animation specified in the rule
IAIAction* CAIActionManager::GetAIAction( const CCondition* pRule )
{
	IAIAction* pResult = NULL;
	if ( pRule->sAction.empty() )
		return NULL;

	if ( pRule->eActionType == eAT_AnimationSignal || pRule->eActionType == eAT_AnimationAction )
	{
		if ( pRule->sAnimationHelper.empty() )
		{
			pResult = new CAnimationAction( pRule->sAction, pRule->eActionType == eAT_AnimationSignal );
		}
		else
		{
			pResult = new CAnimationAction(
				pRule->sAction,
				pRule->eActionType == eAT_AnimationSignal,
				pRule->fApproachSpeed,
				pRule->iApproachStance,
				pRule->fStartRadiusTolerance,
				pRule->fDirectionTolerance,
				pRule->fTargetRadiusTolerance );
		}
	}
	else if ( pRule->eActionType == eAT_Action || pRule->eActionType == eAT_PriorityAction )
		pResult = GetAIAction( pRule->sAction );

	return pResult;
}

// returns an existing AI Action from the library or NULL if not found
CAIAction* CAIActionManager::GetAIAction( const char* sName )
{
	if ( !sName || !*sName )
		return NULL;

	TActionsLib::iterator it = m_ActionsLib.find( CONST_TEMP_STRING(sName) );
	if ( it != m_ActionsLib.end() )
		return &it->second;
	else
		return NULL;
}

// returns an existing AI Action by its index in the library or NULL index is out of range
CAIAction* CAIActionManager::GetAIAction( size_t index )
{
	if ( index >= m_ActionsLib.size() )
		return NULL;

	TActionsLib::iterator it = m_ActionsLib.begin();
	while ( index-- )
		++it;
	return &it->second;
}

// adds an AI Action in the list of active actions
void CAIActionManager::ExecuteAction( const IAIAction* pAction, IEntity* pUser, IEntity* pObject, int maxAlertness, int goalPipeId,
	const char* userState, const char* objectState, const char* userCanceledState, const char* objectCanceledState )
{
	AIAssert( pAction );
	AIAssert( pUser );
	AIAssert( pObject );

	if ( GetAISystem()->IsRecording( pUser->GetAI(), IAIRecordable::E_ACTIONSTART ) )
	{
		string str( pAction->GetName() );
		str += " on entity ";
		str += pObject->GetName();
		GetAISystem()->Record( pUser->GetAI(), IAIRecordable::E_ACTIONSTART, str );
	}

	// don't execute the action if the user is dead
	if ( pUser->GetAI() && !pUser->GetAI()->IsEnabled() )
	{
		if ( !pAction->GetFlowGraph() )
			delete pAction;
		return;
	}

	// allocate an goal pipe id if not specified
	if ( !goalPipeId )
		goalPipeId = GetAISystem()->AllocGoalPipeId();

	// create a clone of the flow graph
	IFlowGraphPtr pFlowGraph = NULL;
	if ( pAction->GetFlowGraph() )
		pFlowGraph = pAction->GetFlowGraph()->Clone();

	// suspend all actions executing on the User or the Object
	int suspendCounter = 0;
	int numActions = SuspendActionsOnEntity( pUser, goalPipeId, pAction, maxAlertness >= 100, suspendCounter );
	if ( numActions == 100 )
	{
		// can't select the goal pipe
		if ( !pAction->GetFlowGraph() )
			delete pAction;
		return;
	}

	if ( pFlowGraph )
	{
		// create active action and add it to the list
		CActiveAction action;
		action.m_pFlowGraph = pFlowGraph;
		action.m_Name = pAction->GetName();
		action.m_pUserEntity = pUser;
		action.m_pObjectEntity = pObject;
		action.m_SuspendCounter = suspendCounter;
		action.m_idGoalPipe = goalPipeId;
		action.m_iThreshold = numActions > 10 ? -1 : maxAlertness % 100; // cancel this action if there are already 10 actions executing
		action.m_nextUserState = userState;
		action.m_nextObjectState = objectState;
		action.m_canceledUserState = userCanceledState;
		action.m_canceledObjectState = objectCanceledState;
		action.m_bDeleted = false;
		action.m_bHighPriority = maxAlertness >= 100;
		m_ActiveActions.push_front( action );
		
		// the User will be first graph entity.
		if (pUser)
			pFlowGraph->SetGraphEntity( pUser->GetId(),0 );
		// the Object will be second graph entity.
		if (pObject)
			pFlowGraph->SetGraphEntity( pObject->GetId(),1 );

		// initialize the graph
		pFlowGraph->InitializeValues();

		pFlowGraph->SetAIAction( &m_ActiveActions.front() );

		if ( action.m_SuspendCounter > 0 )
			pFlowGraph->SetSuspended( true );
	}
	else
	{
		// create active action and add it to the list
		CActiveAction action;
		action.m_pFlowGraph = NULL;
		action.m_Name = pAction->GetName();
		action.m_pUserEntity = pUser;
		action.m_pObjectEntity = pObject;
		action.m_SuspendCounter = suspendCounter;
		action.m_idGoalPipe = goalPipeId;
		action.m_iThreshold = numActions > 10 ? -1 : maxAlertness % 100; // cancel this action if there are already 10 actions executing
		action.m_nextUserState = userState;
		action.m_nextObjectState = objectState;
		action.m_canceledUserState = userCanceledState;
		action.m_canceledObjectState = objectCanceledState;
		action.m_bDeleted = false;
		action.m_bHighPriority = maxAlertness >= 100;
		action.m_vAnimationPos = pAction->GetAnimationPos();
		action.m_vAnimationDir = pAction->GetAnimationDir();
		action.m_vApproachPos = pAction->GetApproachPos();
		m_ActiveActions.push_front( action );

		// delete temp. action
		delete pAction;
	}
}

// removes deleted AI Action from the list of active actions
void CAIActionManager::Update()
{
FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI)
	TActiveActions::iterator it = m_ActiveActions.begin();
	while ( it != m_ActiveActions.end() )
	{
		CActiveAction& action = *it;
		if ( !action.m_pUserEntity )
		{
			// the action is already deleted just remove it from the list
			m_ActiveActions.erase( it++ );
			continue;
		}
		++it;

		if ( action.m_bDeleted )
		{
			AILogComment("AIAction '%s' with '%s' and '%s' ended.\n", action.GetName(), action.m_pUserEntity->GetName(), action.m_pObjectEntity->GetName());
			ActionDone( action );
		}
		else
		{
			IAIObject* pAI = action.m_pUserEntity->GetAI();
			if ( pAI )
			{
				CAIActor* pActor = pAI->CastToCAIActor();
				IUnknownProxy* pProxy = pActor ? pActor->GetProxy() : NULL;
				int alertness = pProxy ? pProxy->GetAlertnessState() : 0;
				if ( alertness > action.m_iThreshold )
				{
					if ( action.m_iThreshold < 0 )
					{
						AILogComment("AIAction '%s' with '%s' and '%s' cancelled\n", action.GetName(), action.m_pUserEntity->GetName(), action.m_pObjectEntity->GetName());
						ActionDone( action );
					}
					else
					{
						AILogComment("AIAction '%s' with '%s' and '%s' aborted\n", action.GetName(), action.m_pUserEntity->GetName(), action.m_pObjectEntity->GetName());
						action.AbortAction();
					}
				}
			}
			else if ( action.m_iThreshold < 0 )
			{
				// action was canceled
				AILogComment("AIAction '%s' with '%s' and '%s' cancelled\n", action.GetName(), action.m_pUserEntity->GetName(), action.m_pObjectEntity->GetName());
				ActionDone( action );
			}
		}
	}
}

// marks AI Action from the list of active actions as deleted
void CAIActionManager::ActionDone( CActiveAction& action, bool bRemoveAction /*= true*/ )
{
	if ( GetAISystem()->IsRecording( action.m_pUserEntity->GetAI(), IAIRecordable::E_ACTIONEND ) )
	{
		string str( action.GetName() );
		str += " on entity ";
		str += action.m_pObjectEntity->GetName();
		GetAISystem()->Record( action.m_pUserEntity->GetAI(), IAIRecordable::E_ACTIONEND, str );
	}

	// remove the pointer to this action in the flow graph
	IFlowGraph* pFlowGraph = action.GetFlowGraph();
	if ( pFlowGraph )
		pFlowGraph->SetAIAction( NULL );

	// make a copy before removing
	CActiveAction copy = action;

	// find the action in the list of active actions
	if ( bRemoveAction )
		m_ActiveActions.remove( copy );

	// resume last suspended action executing on the User or the Object
	ResumeActionsOnEntity( copy.m_pUserEntity, copy.m_idGoalPipe );

	// notify the current behavior script that action was finished
	IAIObject* pAI = copy.m_pUserEntity->GetAI();
	if ( pAI )
	{
		CAIActor* pAIActor = pAI->CastToCAIActor();
		if(pAIActor)
		{
			IAISignalExtraData* pData = GetAISystem()->CreateSignalExtraData();
			pData->SetObjectName( copy.GetName() );
			pData->nID = copy.m_pObjectEntity->GetId();
			pData->iValue = copy.m_bDeleted ? 1 : 0; // if m_bDeleted is true it means the action was succeeded
			pAIActor->SetSignal( 10, "OnActionDone", NULL, pData, g_crcSignals.m_nOnActionDone );
			pAIActor->SetLastActionStatus( copy.m_bDeleted ); // if m_bDeleted is true it means the action was succeeded
		}
	}

	// if it was deleted it also means that it was succesfully finished
	if ( copy.m_bDeleted )
	{
		if ( !copy.m_nextUserState.empty() )
			GetAISystem()->ModifySmartObjectStates( copy.m_pUserEntity, copy.m_nextUserState );
		if ( copy.m_pObjectEntity && !copy.m_nextObjectState.empty() )
			GetAISystem()->ModifySmartObjectStates( copy.m_pObjectEntity, copy.m_nextObjectState );
	}
	else
	{
		if ( !copy.m_canceledUserState.empty() )
			GetAISystem()->ModifySmartObjectStates( copy.m_pUserEntity, copy.m_canceledUserState );
		if ( copy.m_pObjectEntity && !copy.m_canceledObjectState.empty() )
			GetAISystem()->ModifySmartObjectStates( copy.m_pObjectEntity, copy.m_canceledObjectState );
	}

//	if ( copy.m_pUserEntity != copy.m_pObjectEntity )
//		ResumeActionsOnEntity( copy.m_pObjectEntity );
}

// loads the library of AI Action Flow Graphs
void CAIActionManager::LoadLibrary( const char* sPath )
{
	m_ActiveActions.clear();
	
	// don't delete all actions - only those which are added or modified will be reloaded
	//m_ActionsLib.clear();

	string path( sPath );
	ICryPak * pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	string filename;

	path.TrimRight("/\\");
	string search = path + "/*.xml";
	intptr_t handle = pCryPak->FindFirst( search.c_str(), &fd );
	if (handle != -1)
	{
		do
		{
			filename = path;
			filename += "/" ;
			filename += fd.name;

			XmlNodeRef root = GetISystem()->LoadXmlFile( filename );
			if ( root )
			{
				if( GetISystem()->GetIFlowSystem() )
				{
					filename = PathUtil::GetFileName( filename );
					CAIAction& action = m_ActionsLib[ filename ]; // this creates a new entry in m_ActionsLib
					if ( !action.m_pFlowGraph )
					{
						action.m_Name = filename;
						action.m_pFlowGraph = GetISystem()->GetIFlowSystem()->CreateFlowGraph();
						action.m_pFlowGraph->SetSuspended( true );
						action.m_pFlowGraph->SetAIAction( &action );
						action.m_pFlowGraph->SerializeXML( root, true );
					}
				}
			}
		} while ( pCryPak->FindNext( handle, &fd ) >= 0 );

		pCryPak->FindClose( handle );
	}
}

// notification sent by smart objects system
void CAIActionManager::OnEntityRemove( IEntity* pEntity )
{
	bool bFound = false;
	TActiveActions::iterator it, itEnd = m_ActiveActions.end();
	for ( it = m_ActiveActions.begin(); it != itEnd; ++it )
	{
		CActiveAction& action = *it;

		if ( pEntity == action.GetUserEntity() || pEntity == action.GetObjectEntity() )
		{
			action.CancelAction();
			bFound = true;

			AILogComment( "AIAction '%s' with '%s' and '%s' canceled because '%s' was deleted.\n", action.GetName(),
				action.m_pUserEntity->GetName(), action.m_pObjectEntity->GetName(), pEntity->GetName() );
			ActionDone( action, false );
			action.OnEntityRemove();
		}
	}

// calling Update() from here causes a crash if the entity gets deleted while the action flow graph is being updated
//	if ( bFound )
//	{
//		// Update() will remove canceled actions
//		Update();
//	}
}


void CAIActionManager::Serialize( TSerialize ser )
{
	if ( ser.BeginOptionalGroup("AIActionManager",true) )
	{
		int numActiveActions = m_ActiveActions.size();
		ser.Value( "numActiveActions", numActiveActions );
		if ( ser.IsReading() )
		{
			Reset();
			while ( numActiveActions-- )
				m_ActiveActions.push_back( CActiveAction() );
		}

		TActiveActions::iterator it, itEnd = m_ActiveActions.end();
		for ( it = m_ActiveActions.begin(); it != itEnd; ++it )
			it->Serialize( ser );
		ser.EndGroup(); //AIActionManager
	}
}

void CActiveAction::Serialize( TSerialize ser )
{
	ser.BeginGroup("ActiveAction");
	{
		ser.Value( "m_Name", m_Name );

		EntityId userId = m_pUserEntity && !ser.IsReading() ? m_pUserEntity->GetId() : 0;
		ser.Value( "userId", userId );
		if ( ser.IsReading() )
			m_pUserEntity = gEnv->pEntitySystem->GetEntity( userId );

		EntityId objectId = m_pObjectEntity && !ser.IsReading() ? m_pObjectEntity->GetId() : 0;
		ser.Value( "objectId", objectId );
		if ( ser.IsReading() )
			m_pObjectEntity = gEnv->pEntitySystem->GetEntity( objectId );

		ser.Value( "m_SuspendCounter", m_SuspendCounter );
		ser.Value( "m_idGoalPipe", m_idGoalPipe );
		if ( m_idGoalPipe && ser.IsReading() )
		{
			IAIObject* pAI = m_pUserEntity->GetAI();
			AIAssert( pAI );
			if ( pAI )
			{
				CPipeUser* pPipeUser = pAI->CastToCPipeUser();
				if(pPipeUser)
				{
					pPipeUser->RegisterGoalPipeListener( GetAISystem()->GetActionManager(), m_idGoalPipe, "CActiveAction::Serialize" );
					CGoalPipe* pPipe = pPipeUser->GetCurrentGoalPipe();
					while ( pPipe )
					{
						if ( pPipe->m_nEventId == m_idGoalPipe )
						{
							if ( m_bHighPriority )
								pPipe->m_sDebugName = '[' + m_Name + ']';
							else
								pPipe->m_sDebugName = "[!" + m_Name + ']';
							break;
						}
						pPipe = pPipe->GetSubpipe();
					}
				}
			}
		}
		ser.Value( "m_iThreshold", m_iThreshold );
		ser.Value( "m_nextUserState", m_nextUserState );
		ser.Value( "m_nextObjectState", m_nextObjectState );
		ser.Value( "m_canceledUserState", m_canceledUserState );
		ser.Value( "m_canceledObjectState", m_canceledObjectState );
		ser.Value( "m_bDeleted", m_bDeleted );
		ser.Value( "m_bHighPriority", m_bHighPriority );
		ser.Value( "m_vAnimationPos", m_vAnimationPos );
		ser.Value( "m_vAnimationDir", m_vAnimationDir );
		ser.Value( "m_vApproachPos", m_vApproachPos );

		if (ser.BeginOptionalGroup( "m_pFlowGraph", m_pFlowGraph ))
		{
			if ( ser.IsReading() )
			{
				CAIAction* pAction = GetAISystem()->GetActionManager()->GetAIAction( m_Name );
				AIAssert( pAction );
				if ( pAction )
					m_pFlowGraph = pAction->GetFlowGraph()->Clone();
				if ( m_pFlowGraph )
					m_pFlowGraph->SetAIAction( this );
			}
			if ( m_pFlowGraph )
			{
				m_pFlowGraph->SetGraphEntity( userId,0 );
				m_pFlowGraph->SetGraphEntity( objectId,1 );
				m_pFlowGraph->Serialize( ser );
			}
			ser.EndGroup(); //m_pFlowGraph
		}
	}
	ser.EndGroup();
}
