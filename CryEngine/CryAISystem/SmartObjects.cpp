#include "StdAfx.h"
#include <ISystem.h>
#include <IXml.h>
#include <ICryAnimation.h>
#include <IConsole.h>

#include "AIObject.h"
#include "Puppet.h"
#include "CAISystem.h"
#include "AIDebugDrawHelpers.h"

#include "SmartObjectNavRegion.h"

#include "AIActions.h"
#include "SmartObjects.h"


CSmartObject::CState::MapSmartObjectStateIds	CSmartObject::CState::g_mapStateIds;
CSmartObject::CState::MapSmartObjectStates		CSmartObject::CState::g_mapStates;

CSmartObjectClass::MapClassesByName				CSmartObjectClass::g_AllByName;

CSmartObjectClass::VectorClasses				CSmartObjectClass::g_AllUserClasses;
CSmartObjectClass::VectorClasses::iterator		CSmartObjectClass::g_itAllUserClasses = CSmartObjectClass::g_AllUserClasses.end();

CSmartObjects::MapSmartObjectsByEntity			CSmartObjects::g_AllSmartObjects;

IMaterial* CClassTemplateData::m_pHelperMtl = NULL;



// this mask should be used for finding enclosing node of navigation smart objects
#define SMART_OBJECT_ENCLOSING_NAV_TYPES (IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE | IAISystem::NAV_VOLUME)


bool CCondition:: operator == ( const CCondition& other ) const
{
	if ( iTemplateId != other.iTemplateId )								return false;
	else if ( pUserClass != other.pUserClass )							return false;
	else if ( userStatePattern != other.userStatePattern )				return false;
	else if ( pObjectClass != other.pObjectClass )						return false;
	else if ( objectStatePattern != other.objectStatePattern )			return false;
	else if ( sObjectHelper != other.sObjectHelper )					return false;
	else if ( sUserHelper != other.sUserHelper )						return false;
	else if ( fDistanceFrom != other.fDistanceFrom )					return false;
	else if ( fDistanceTo != other.fDistanceTo )						return false;
	else if ( fOrientationLimit != other.fOrientationLimit )			return false;
	else if ( fOrientationToTargetLimit != other.fOrientationToTargetLimit )	return false;
	else if ( fMinDelay != other.fMinDelay )							return false;
	else if ( fMaxDelay != other.fMaxDelay )							return false;
	else if ( fMemory != other.fMemory )								return false;
	else if ( fProximityFactor != other.fProximityFactor )				return false;
	else if ( fOrientationFactor != other.fOrientationFactor )			return false;
	else if ( fVisibilityFactor != other.fVisibilityFactor )			return false;
	else if ( fRandomnessFactor != other.fRandomnessFactor )			return false;
	else if ( fLookAtPerc != other.fLookAtPerc )						return false;
	else if ( userPostActionStates != other.userPostActionStates )		return false;
	else if ( objectPostActionStates != other.objectPostActionStates )	return false;
	else if ( eActionType != other.eActionType )						return false;
	else if ( sAction != other.sAction )								return false;
	else if ( userPreActionStates != other.userPreActionStates )		return false;
	else if ( objectPreActionStates != other.objectPreActionStates )	return false;
	else if ( iMaxAlertness != other.iMaxAlertness )					return false;
	else if ( bEnabled != other.bEnabled )								return false;
	else if ( sEvent != other.sEvent )									return false;
	else if ( sChainedUserEvent != other.sChainedUserEvent )			return false;
	else if ( sChainedObjectEvent != other.sChainedObjectEvent )		return false;
	else if ( sName != other.sName )									return false;
	else if ( sDescription != other.sDescription )						return false;
	else if ( sFolder != other.sFolder )								return false;
	else if ( iOrder != other.iOrder )									return false;
	else if ( iRuleType != other.iRuleType )							return false;
	else if ( sEntranceHelper != other.sEntranceHelper )				return false;
	else if ( sExitHelper != other.sExitHelper )						return false;
	else if ( fApproachSpeed != other.fApproachSpeed )					return false;
	else if ( iApproachStance != other.iApproachStance )				return false;
	else if ( sAnimationHelper != other.sAnimationHelper )				return false;
	else if ( sApproachHelper != other.sApproachHelper )				return false;
	else if ( fStartRadiusTolerance != other.fStartRadiusTolerance )	return false;
	else if ( fDirectionTolerance != other.fDirectionTolerance )		return false;
	else if ( fTargetRadiusTolerance != other.fTargetRadiusTolerance )	return false;
	else
		return true;
}

/////////////////////////////////////////
// CSmartObjectBase class implementation
/////////////////////////////////////////

Vec3 CSmartObjectBase::GetPos() const
{
	IAIObject* pAI = m_pEntity->GetAI();
	if ( pAI && pAI->IsEnabled() )
		return pAI->GetPos();
	return m_pEntity->GetWorldPos();
}

Vec3 CSmartObjectBase::GetHelperPos( const SmartObjectHelper* pHelper ) const
{
	IPhysicalEntity* pPE = m_pEntity->GetPhysics();
	if ( pPE && m_pEntity->GetAI() && pPE->GetType() == PE_ARTICULATED )
	{
		// it's a ragdolized actor -> special processing
		ICharacterInstance* pCharInstance = m_pEntity->GetCharacter(0);
		if ( pCharInstance )
		{
			ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
			if ( pSkeletonPose )
			{
				int boneId = pSkeletonPose->GetJointIDByName( "Bip01 Pelvis" );
				if ( boneId >= 0 )
				{
					Vec3 pos( pHelper->qt.t.z, -pHelper->qt.t.y, pHelper->qt.t.x );
			//		pos = pSkeleton->GetAbsJMatrixByID( boneId ).TransformPoint( pos );
					pos = pSkeletonPose->GetAbsJointByID(boneId) * pos;
					return m_pEntity->GetWorldTM().TransformPoint( pos );
				}
			}
		}
	}

	return m_pEntity->GetWorldTM().TransformPoint( pHelper->qt.t );
}

Vec3 CSmartObjectBase::GetOrientation( const SmartObjectHelper* pHelper ) const
{
	IPhysicalEntity* pPE = m_pEntity->GetPhysics();
	if ( pPE && m_pEntity->GetAI() && pPE->GetType() == PE_ARTICULATED )
	{
		// it's a ragdolized actor -> special processing
		ICharacterInstance* pCharInstance = m_pEntity->GetCharacter(0);
		if ( pCharInstance )
		{
			ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
			if ( pSkeletonPose )
			{
				int boneId = pSkeletonPose->GetJointIDByName( "Bip01 Pelvis" );
				if ( boneId >= 0 )
				{
					Vec3 forward = pHelper ? pHelper->qt.q * FORWARD_DIRECTION : FORWARD_DIRECTION;
					forward.Set( forward.z, -forward.y, forward.x );
		//			forward = pSkeleton->GetAbsJMatrixByID( boneId ).TransformVector( forward );
					forward = pSkeletonPose->GetAbsJointByID( boneId ).q * forward;
					return m_pEntity->GetWorldTM().TransformVector( forward );
				}
			}
		}
	}

	if ( pHelper )
		return m_pEntity->GetWorldTM().TransformVector( pHelper->qt.q * FORWARD_DIRECTION );

	CAIActor* pAIActor = CastToCAIActorSafe(m_pEntity->GetAI());
	if ( pAIActor )
	{
		if ( pAIActor->GetState()->vMoveDir.IsZero( 0.1f ) )
			return pAIActor->GetViewDir();
		else
			return pAIActor->GetState()->vMoveDir;
	}
	return m_pEntity->GetWorldTM().TransformVector(Vec3(0,1,0));
}

CPuppet* CSmartObjectBase::GetPuppet() const
{
	return CastToCPuppetSafe(m_pEntity->GetAI());
}


/////////////////////////////////////////
// CSmartObject class implementation
/////////////////////////////////////////

CSmartObject::CSmartObject( IEntity* pEntity )
	: CSmartObjectBase( pEntity )
	, m_fLookAtLimit(0.0f)
	, m_vLookAtPos(ZERO)
	, m_eValidationResult(eSOV_Unknown)
	, m_pMapTemplates(NULL)
	, m_bHidden(false)
{
	m_States.insert( "Idle" );
	m_fRandom = float(ai_rand())/(2.0f*RAND_MAX);
	CSmartObjects::g_AllSmartObjects[ pEntity ] = this;
	pEntity->SetSmartObject( this );
}

CSmartObject::~CSmartObject()
{
	CSmartObjects::g_AllSmartObjects.erase( m_pEntity );
	m_pEntity->SetSmartObject( NULL );
}

void CSmartObject::Serialize( TSerialize ser )
{
	if ( ser.IsReading() )
	{
		m_Events.clear();
		m_mapLastUpdateTimes.clear();
		m_fLookAtLimit = 0;
		m_vLookAtPos.zero();
		m_mapNavNodes.clear();
	}

	ser.Value( "m_fRandom", m_fRandom );

	if ( ser.BeginOptionalGroup("States",true) )
	{
		if ( ser.IsReading() )
		{
			m_States.clear();
			int count;
			ser.Value( "Size", count );
			while ( count-- )
			{
				string value;
				ser.BeginGroup("i");
				{
					ser.Value( "Value", value );
					m_States.insert(CState( value ));
				}
				ser.EndGroup();
			}
		}
		else
		{
			int count = m_States.size();
			ser.Value( "Size", count );
			SetStates::iterator it, itEnd = m_States.end();
			for ( it = m_States.begin(); it != itEnd; ++it )
			{
				string value = it->c_str();
				ser.BeginGroup("i");
				{
					ser.Value( "Value", value );
				}
				ser.EndGroup();
			}
		}
		ser.EndGroup(); //States
	}
}

void CSmartObjects::SerializePointer( TSerialize ser, const char* name, CSmartObject* & pSmartObject )
{
	if ( ser.IsWriting() )
	{
		EntityId id = pSmartObject ? pSmartObject->GetEntity()->GetId() : 0;
		ser.Value( name, id );
	}
	else
	{
		EntityId id;
		ser.Value( name, id );
		if ( id )
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity( id );
			AIAssert( pEntity );
			pSmartObject = IsSmartObject( pEntity );
			AIAssert( pSmartObject );
		}
		else
			pSmartObject = NULL;
	}
}

void CSmartObjects::SerializePointer( TSerialize ser, const char* name, CCondition* & pRule )
{
	if ( ser.IsWriting() )
	{
		if(pRule)
			ser.Value( name, pRule->iOrder );
		else
		{
			int nulValue = -1;
			ser.Value( name, nulValue );
		}
	}
	else
	{
		pRule = NULL;
		int iOrder;
		ser.Value( name, iOrder );
		if ( iOrder == -1 )
			return;

		MapConditions& rules = GetAISystem()->GetSmartObjects()->m_Conditions;
		MapConditions::iterator it, itEnd = rules.end();
		for( it = rules.begin(); it != itEnd; ++it )
			if ( it->second.iOrder == iOrder )
			{
				pRule = &it->second;
				return;
			}
	}
}

void CSmartObject::Use( CSmartObject* pObject, CCondition* pCondition, int eventId /*=0*/, bool bForceHighPriority /*=false*/ ) const
{
	CAIActionManager* pActionManager = GetAISystem()->GetActionManager();
	const char* sSignal = NULL;
	IAIAction* pAction = NULL;
	if ( !pCondition->sAction.empty() && pCondition->eActionType != eAT_None )
	{
		if ( pCondition->eActionType == eAT_AISignal )
			sSignal = pCondition->sAction;
		else
			pAction = pActionManager->GetAIAction( pCondition );

		if ( !pAction && !sSignal )
		{
			AIWarning( "Undefined AI action \"%s\"!", pCondition->sAction.c_str() );

			// undo states since action wasn't found
			if ( !pCondition->userPreActionStates.empty() )
				GetAISystem()->ModifySmartObjectStates( m_pEntity, pCondition->userPreActionStates.AsUndoString() );
			if ( this != pObject )
				if ( !pCondition->objectPreActionStates.empty() )
					GetAISystem()->ModifySmartObjectStates( pObject->m_pEntity, pCondition->objectPreActionStates.AsUndoString() );
			return;
		}
	}

	int maxAlertness = pCondition->iMaxAlertness;
	if ( bForceHighPriority || pCondition->eActionType == eAT_PriorityAction /*|| pCondition->eActionType == eAT_AnimationSignal || pCondition->eActionType == eAT_AnimationAction*/ )
		maxAlertness += 100;

	CAIActor* pAIObject = CastToCAIActorSafe(GetAI());
	if (pAIObject && pAIObject->IsAgent())
	{
		IEntity* pObjectEntity = pObject->GetEntity();
		if ( sSignal )
		{
			AISignalExtraData* pExtraData = new AISignalExtraData;
			if ( eventId )
				pExtraData->iValue = eventId;
			pExtraData->nID = pObjectEntity->GetId();

			pExtraData->point = pCondition->pObjectHelper ? pObject->GetHelperPos( pCondition->pObjectHelper ) : pObject->GetPos();

			pAIObject->SetSignal( 10, sSignal, pObjectEntity, pExtraData );

			// update states
			if ( !pCondition->userPostActionStates.empty() ) // check is next state non-empty
				GetAISystem()->ModifySmartObjectStates( m_pEntity, pCondition->userPostActionStates.AsString() );
			if ( this != pObject )
				if ( !pCondition->objectPostActionStates.empty() ) // check is next state non-empty
					GetAISystem()->ModifySmartObjectStates( pObject->m_pEntity, pCondition->objectPostActionStates.AsString() );

			return;
		}
		else if (pAIObject->CastToCAIPlayer())
		{
			pAIObject = NULL;
		}
		else
		{
			if ( eventId || !pAction->GetFlowGraph() )
			{
				if ( !pAction->GetFlowGraph() )
				{
					// set reference point to say where to play the animation
					CAnimationAction* pAnimAction = static_cast< CAnimationAction* >( pAction );

					if ( pCondition->pObjectClass )
					{
						const SmartObjectHelper* pHelper = pCondition->pObjectClass->GetHelper( pCondition->sAnimationHelper );
						if ( pHelper )
						{
							pAnimAction->SetTarget( pObject->GetHelperPos(pHelper), pObject->GetOrientation(pHelper) );

							// approach target might be different
							const SmartObjectHelper* pApproachHelper = pCondition->pObjectClass->GetHelper( pCondition->sApproachHelper );
							if ( pApproachHelper )
								pAnimAction->SetApproachPos( pObject->GetHelperPos(pApproachHelper) );
						}
						else
						{
							Vec3 direction = pObject->GetPos() - GetPos();
							direction.NormalizeSafe();
							pAnimAction->SetTarget( GetPos(), direction );
						}
					}
					else
					{
						pAnimAction->SetTarget( GetPos(), GetOrientation(NULL) );
					}
				}

				// if eventId was provided or animation was requested just execute the action immediately without sending a signal
				pActionManager->ExecuteAction( pAction, GetEntity(), pObject->GetEntity(), maxAlertness, eventId,
					pCondition->userPostActionStates.AsString(), pCondition->objectPostActionStates.AsString(),
					pCondition->userPreActionStates.AsUndoString(), pCondition->objectPreActionStates.AsUndoString() );
			}
			else
			{
				AISignalExtraData* pExtraData = new AISignalExtraData;
				pExtraData->SetObjectName( string(pAction->GetName()) +
					",\"" + pCondition->userPostActionStates.AsString() + '\"' +
					",\"" + pCondition->objectPostActionStates.AsString() + '\"' +
					",\"" + pCondition->userPreActionStates.AsUndoString() + '\"' +
					",\"" + pCondition->objectPreActionStates.AsUndoString() + '\"' );
				pExtraData->iValue = maxAlertness;
				pAIObject->SetSignal( 10, "OnUseSmartObject", pObjectEntity, pExtraData, g_crcSignals.m_nOnUseSmartObject);
			}

			return;
		}
	}

	if ( !pAIObject )
	{
		// the user isn't a puppet (or vehicle, or player)
		if ( sSignal )
		{
			AIWarning( "Attempt to send AI signal to an entity not registered as Puppet or Vehicle in AI system!" );

			// undo pre-action states
			if ( !pCondition->userPreActionStates.empty() ) // check is next state non-empty
				GetAISystem()->ModifySmartObjectStates( m_pEntity, pCondition->userPreActionStates.AsUndoString() );
			if ( this != pObject )
				if ( !pCondition->objectPreActionStates.empty() ) // check is next state non-empty
					GetAISystem()->ModifySmartObjectStates( pObject->m_pEntity, pCondition->objectPreActionStates.AsUndoString() );
		}
		else if ( pAction && pAction->GetFlowGraph() )
		{
			GetAISystem()->GetActionManager()->ExecuteAction( pAction, GetEntity(), pObject->GetEntity(), 100, eventId,
				pCondition->userPostActionStates.AsString(), pCondition->objectPostActionStates.AsString(),
				pCondition->userPreActionStates.AsUndoString(), pCondition->objectPreActionStates.AsUndoString() );
		}
	}
}


/////////////////////////////////////////
// CSmartObjectClass class implementation
/////////////////////////////////////////

void CSmartObjectClass::RegisterSmartObject( CSmartObject* pSmartObject )
{
	AIAssert( std::find( m_allSmartObjectInstances.begin(), m_allSmartObjectInstances.end(), pSmartObject ) == m_allSmartObjectInstances.end() );
	m_allSmartObjectInstances.push_back( pSmartObject );
	pSmartObject->GetClasses().push_back( this );
}

void CSmartObjectClass::UnregisterSmartObject( CSmartObject* pSmartObject )
{
	CSmartObjectClass::VectorSmartObjects::iterator find
		= std::find( m_allSmartObjectInstances.begin(), m_allSmartObjectInstances.end(), pSmartObject );
	AIAssert( find != m_allSmartObjectInstances.end() );
	*find = m_allSmartObjectInstances.back();
	m_allSmartObjectInstances.pop_back();
	
	CSmartObjectClasses::iterator it = std::find( pSmartObject->GetClasses().begin(), pSmartObject->GetClasses().end(), this );
	AIAssert( it != pSmartObject->GetClasses().end() );
	pSmartObject->GetClasses().erase( it );
}

void CSmartObjectClass::DeleteSmartObject( IEntity* pEntity )
{
	CSmartObjectClass::VectorSmartObjects::iterator it
		= std::find( m_allSmartObjectInstances.begin(), m_allSmartObjectInstances.end(), pEntity->GetSmartObject() );
	if ( it != m_allSmartObjectInstances.end() )
	{
		CSmartObjectClasses & vClasses = (*it)->GetClasses();
		CSmartObjectClasses::iterator itClass = std::find( vClasses.begin(), vClasses.end(), this );
		AIAssert( itClass != vClasses.end() );
		if ( itClass != vClasses.end() )
			vClasses.erase( itClass );
		if ( vClasses.empty() )
			delete *it; // deletes the smart object!

		*it = m_allSmartObjectInstances.back();
		m_allSmartObjectInstances.pop_back();
	}
//	else
//		AIAssert( 0 );
}

CSmartObjectClass::CSmartObjectClass( const char* className )
	: m_sName( className )
	, m_bSmartObjectUser( false )
	, m_bNeeded( false )
	, m_pTemplateData( NULL )
	, m_indexNextObject( -1 )
{
	g_AllByName[ className ] = this;
}

CSmartObjectClass::~CSmartObjectClass()
{
	while ( !m_allSmartObjectInstances.empty() )
	{
		DeleteSmartObject( m_allSmartObjectInstances.back()->GetEntity() );
	}

	VectorClasses::iterator it = std::find( g_AllUserClasses.begin(), g_AllUserClasses.end(), this );
	if ( it != g_AllUserClasses.end() )
	{
		// erase unordered vector element
		*it = g_AllUserClasses.back();
		g_AllUserClasses.pop_back();
	}
	g_itAllUserClasses = CSmartObjectClass::g_AllUserClasses.end();

	g_AllByName.erase( m_sName );
}

CSmartObjectClass* CSmartObjectClass::find( const char* sClassName )
{
	if ( g_AllByName.empty() )
		return NULL;
	MapClassesByName::iterator it = g_AllByName.find( CONST_TEMP_STRING(sClassName) );
	return it != g_AllByName.end() ? it->second : NULL;
}

const char* CSmartObjectClass::GetClassName( int id )
{
	MapClassesByName::iterator it = g_AllByName.begin();
	std::advance( it, id );
	if ( it != g_AllByName.end() )
		return it->first;
	return NULL;
}

void CSmartObjectClass::ClearHelperLinks()
{
	m_setNavHelpers.clear();
	m_vHelperLinks.clear();
}

bool CSmartObjectClass::AddHelperLink( CCondition* condition )
{
	AIAssert( condition->pObjectClass == this );
	SmartObjectHelper* pEntrance = GetHelper( condition->sEntranceHelper );
	SmartObjectHelper* pExit = GetHelper( condition->sExitHelper );
	if ( pEntrance && pExit )
	{
		m_setNavHelpers.insert( pEntrance );
		m_setNavHelpers.insert( pExit );

		HelperLink link = {
			condition->iRuleType == 1 ? 100 : 0,
			pEntrance,
			pExit,
			condition
		};
		m_vHelperLinks.push_back( link );
		return true;
	}
	return false;
}

int CSmartObjectClass::FindHelperLinks(
	const SmartObjectHelper* from, const SmartObjectHelper* to, const CSmartObjectClass* pClass, float radius,
	CSmartObjectClass::HelperLink** pvHelperLinks, int iCount /*=1*/ )
{
	int numFound = 0;
	int i = m_vHelperLinks.size();
	while ( i-- && numFound < iCount )
	{
		HelperLink* pLink = &m_vHelperLinks[i];
		if ( pLink->from == from && pLink->to == to && pClass == pLink->condition->pUserClass && radius <= pLink->passRadius )
			pvHelperLinks[ numFound++ ] = pLink;
	}
	return numFound;
}

int CSmartObjectClass::FindHelperLinks(
	const SmartObjectHelper* from, const SmartObjectHelper* to, const CSmartObjectClass* pClass, float radius,
	const CSmartObject::SetStates& userStates, const CSmartObject::SetStates& objectStates,
	CSmartObjectClass::HelperLink** pvHelperLinks, int iCount /*=1*/ )
{
	int numFound = 0;
	int i = m_vHelperLinks.size();
	while ( i-- && numFound < iCount )
	{
		HelperLink* pLink = &m_vHelperLinks[i];
		if ( pLink->from == from && pLink->to == to && pClass == pLink->condition->pUserClass && radius <= pLink->passRadius &&
			pLink->condition->userStatePattern.Matches( userStates ) && pLink->condition->objectStatePattern.Matches( objectStates ) )
				pvHelperLinks[ numFound++ ] = pLink;
	}
	return numFound;
}


/////////////////////////////////////////
// CSmartObjects class implementation
/////////////////////////////////////////

typedef std::list< string > ListTokens;

int Tokenize( char*& str, int& length )
{
	char* token = str;
	int tokenLength = length;

	// Skip the previous token.
	token += tokenLength;

	// Skip any leading whitespace.
	while (*token && !((*token >= 'A' && *token <= 'Z') || (*token >= 'a' && *token <= 'z') || *token == '_'))
		++token;

	// Find the end of the token.
	tokenLength = 0;
	char* end = token;
	while ((*end >= '0' && *end <= '9') || (*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') || *end == '_')
		++end;
	tokenLength = end - token;

	str = token;
	length = tokenLength;

	return length;
}

void CSmartObjects::String2Classes( const string& sClass, CSmartObjectClasses& vClasses )
{
	CSmartObjectClass* pClass;

	// Make a temporary copy of the string - more efficient to do this once without memory allocations,
	// because then we can write temporary null-terminators into the buffer to avoid copying/string allocations
	// later.
	char classString[1024];
	AIAssert(int(sClass.size()) < sizeof(classString) - 1);
	int stringLength = min(int(sClass.size()), int(sizeof(classString)) - 1);
	memcpy(classString, sClass.c_str(), stringLength);
	classString[stringLength] = 0;

	char* token = classString;
	int tokenLength = 0;
	while (Tokenize(token, tokenLength))
	{
		// Temporarily null-terminate the token.
		char oldChar = token[tokenLength];
		token[tokenLength] = 0;

		pClass = GetSmartObjectClass(token);

		if ( pClass && std::find( vClasses.begin(), vClasses.end(), pClass ) == vClasses.end() )
			vClasses.push_back( pClass );

		token[tokenLength] = oldChar;
	};
}

void CSmartObjects::String2States( const char* listStates, CSmartObject::DoubleVectorStates& states )
{
	states.positive.clear();
	states.negative.clear();

	// Make a temporary copy of the string - more efficient to do this once without memory allocations,
	// because then we can write temporary null-terminators into the buffer to avoid copying/string allocations
	// later.
	char statesString[1024];
	int inputStringLength = int(strlen(listStates));
	int stringLength = min(inputStringLength, int(sizeof(statesString)) - 1);
	AIAssert(inputStringLength < sizeof(statesString) - 1);
	memcpy(statesString, listStates, stringLength);
	statesString[stringLength] = 0;

	// Split the string into positive and negative state lists.
	int hyphenPosition = -1;
	for (int i = 0; statesString[i]; ++i)
	{
		if (statesString[i] == '-')
		{
			hyphenPosition = i;
			break;
		}
	}

	char* positiveStateList = statesString;
	char* negativeStateList = 0;
	if (hyphenPosition >= 0)
	{
		statesString[hyphenPosition] = 0;
		negativeStateList = statesString + hyphenPosition + 1;
	}

	// Examine the two lists (positive and negative) to extract a list of states for each.
	CSmartObject::VectorStates* stateVectors[2] = {&states.positive, &states.negative};
	char* stateStrings[2] = {positiveStateList, negativeStateList};

	for (int stateVectorIndex = 0; stateVectorIndex < 2; ++stateVectorIndex)
	{
		char* token = stateStrings[stateVectorIndex];
		int tokenLength = 0;
		while (token && Tokenize(token, tokenLength))
		{
			// Temporarily null-terminate the token.
			char oldChar = token[tokenLength];
			token[tokenLength] = 0;

			stateVectors[stateVectorIndex]->push_back(token);

			token[tokenLength] = oldChar;
		}
	}
}

void CSmartObjects::String2StatePattern( const char* sPattern, CSmartObject::CStatePattern& pattern )
{
	pattern.clear();
	string item;
	CSmartObject::DoubleVectorStates dvs;
	while ( sPattern )
	{
		const char* i = strchr( sPattern, '|' );
		if ( !i )
		{
			item = sPattern;
			--i;
		}
		else
			item.assign( sPattern, i-sPattern );
		
		String2States(item, dvs );

		if ( !dvs.empty() )
			pattern.push_back( dvs );
		sPattern = i+1;
	}
}

CSmartObjects::CSmartObjects()
	: m_StateSameGroup( "SameGroup" )
	, m_StateSameSpecies( "SameSpecies" )
	, m_StateAttTarget( "AttTarget" )
	, m_StateBusy( "Busy" )
	, m_pPreOnSpawnEntity( NULL )
{
	// register system states
	CSmartObject::CState( "Idle" );
	CSmartObject::CState( "Alerted" );
	CSmartObject::CState( "Combat" );
	CSmartObject::CState( "Dead" );
}

CSmartObjects::~CSmartObjects()
{
	ClearConditions();
}

void CSmartObjects::Serialize( TSerialize ser )
{
	if ( ser.IsReading() )
	{
		m_vDebugUse.clear();
	}

	if ( ser.BeginOptionalGroup("SmartObjects",true ))
	{
		if (ser.IsWriting())
		{
			int count = g_AllSmartObjects.size();
			ser.Value( "Count",count );
			MapSmartObjectsByEntity::iterator it, itEnd = g_AllSmartObjects.end();
			for ( it = g_AllSmartObjects.begin(); it != itEnd; ++it )
			{
				ser.BeginGroup("Object");
				{
					CSmartObject* pObject = it->second;
					SerializePointer( ser, "id", pObject );
					if (pObject)
						pObject->Serialize( ser );
				}
				ser.EndGroup();
			}
		}
		else
		{
			int count = 0;
			ser.Value( "Count",count );
			for (int i = 0; i < count; i++)
			{
				ser.BeginGroup("Object");
				{
					CSmartObject* pObject = 0;
					SerializePointer( ser, "id", pObject );
					if (pObject)
						pObject->Serialize( ser );
				}
				ser.EndGroup();
			}
		}

		ser.EndGroup(); //SmartObjects
	}
}

void CSmartObjects::AddSmartObjectClass( CSmartObjectClass* soClass )
{
	CSmartObjectClass::MapSmartObjectsByPos& mapByPos = soClass->m_MapObjectsByPos;

	CSmartObjectClasses vClasses;

	// we must check already created instances of this class and 'convert' them to smart objects
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	IEntityItPtr it = pEntitySystem->GetEntityIterator();
	while ( IEntity* pEntity = it->Next() )
	{
		if ( !pEntity->IsGarbage() )
		{
			if ( ParseClassesFromProperties(pEntity, vClasses) )
			{
				if ( std::find( vClasses.begin(), vClasses.end(), soClass ) != vClasses.end() )
				{
					CSmartObject* smartObject = IsSmartObject( pEntity );
					if ( !smartObject )
						smartObject = new CSmartObject( pEntity );
					soClass->RegisterSmartObject( smartObject );
					smartObject->m_fKey = smartObject->GetPos().x;
					mapByPos.insert( std::make_pair( smartObject->m_fKey, smartObject ) );
				}
			}
		}
	}
}

bool CSmartObjects::ParseClassesFromProperties( const IEntity* pEntity, CSmartObjectClasses& vClasses )
{
	vClasses.clear();

	IScriptTable* pTable = pEntity->GetScriptTable();
	if ( pTable )
	{
		// ignore AISpawners
		string className = pEntity->GetClass()->GetName();
		className = className.substr( 0, 5 );
		if ( className == "Spawn" )
			return false;

		SmartScriptTable props;
		if ( pTable->GetValue("Properties", props) )
		{
			const char* szClass = NULL;
			props->GetValue( "soclasses_SmartObjectClass", szClass );

			const char* szClassInstance = NULL;
			SmartScriptTable propsInstance;
			if ( pTable->GetValue("PropertiesInstance", propsInstance) )
				propsInstance->GetValue( "soclasses_SmartObjectClass", szClassInstance );

			if ( szClass && *szClass )
				String2Classes(szClass, vClasses );
			if ( szClassInstance && *szClassInstance )
				String2Classes(szClassInstance, vClasses );
		}
	}

	return !vClasses.empty();
}

/*
CSmartObjectClass* CSmartObjects::GetSmartObjectClass( int iAnchorType, const char* className )
{
	CSmartObjectClass* pClass = CSmartObjectClass::find( iAnchorType );
	if ( pClass )
		return pClass;

	// If you are registering a new class then className must be provided
	AIAssert( className );

	CSmartObjectClass* result = new CSmartObjectClass( iAnchorType, className );
	m_SmartObjects[ result ]; // = MapSmartObjectsByState();
	AddSmartObjectClass( result );
	return result;
}

CSmartObjectClass* CSmartObjects::GetSmartObjectClass( IEntityClass* pEntityClass, const char* className )
{
	CSmartObjectClass* pClass = CSmartObjectClass::find( pEntityClass );
	if ( pClass )
		return pClass;

	// If you are registering a new class then className must be provided
	AIAssert( className );

	CSmartObjectClass* result = new CSmartObjectClass( pEntityClass, className );
	m_SmartObjects[ result ]; // = MapSmartObjectsByState();
	AddSmartObjectClass( result );
	return result;
}

CSmartObjectClass* CSmartObjects::GetSmartObjectClass( const char* className )
{
	if ( !stricmp(className, "AIOBJECT_PUPPET") )
		return GetSmartObjectClass( AIOBJECT_PUPPET, className );

	if ( !stricmp(className, "AIOBJECT_VEHICLE") )
		return GetSmartObjectClass( AIOBJECT_VEHICLE, className );

	if ( !stricmp(className, "AIOBJECT_PLAYER") )
		return GetSmartObjectClass( AIOBJECT_PLAYER, className );

	IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( className );
	if ( pEntityClass )
		return GetSmartObjectClass( pEntityClass, className );

	SmartScriptTable anchors;
	IScriptSystem* pScriptSystem = gEnv->pScriptSystem;
	if ( !pScriptSystem->GetGlobalValue( "AIAnchorTable", anchors ) )
		if ( pScriptSystem->ExecuteFile( "Scripts/AI/anchor.lua", false, false ) )
			pScriptSystem->GetGlobalValue( "AIAnchorTable", anchors );
	AIAssert( anchors.GetPtr() );

	int anchorType;
	if (anchors->GetValue( className, anchorType ))
		return GetSmartObjectClass( anchorType, className );

	return NULL;
}
*/

CSmartObjectClass* CSmartObjects::GetSmartObjectClass( const char* className )
{
	AIAssert( className );

	if ( !*className )
		return NULL;

	CSmartObjectClass* pClass = CSmartObjectClass::find( className );
	if ( pClass )
		return pClass;

	CSmartObjectClass* result = new CSmartObjectClass( className );
	AddSmartObjectClass( result );
	return result;
}

void CSmartObjects::SetSmartObjectConditions( SmartObjectConditions& conditions )
{
	ClearConditions();

	SmartObjectConditions::iterator it, itEnd = conditions.end();
	for ( it = conditions.begin(); it != itEnd; ++it )
		AddSmartObjectCondition( *it );

	// restore links to helpers
	MapSOHelpers::iterator itHelpers, itHelpersEnd = m_mapHelpers.end();
	for ( itHelpers = m_mapHelpers.begin(); itHelpers != itHelpersEnd; ++itHelpers )
	{
		CSmartObjectClass* pClass = GetSmartObjectClass( itHelpers->first );
		pClass->AddHelper( &itHelpers->second );
	}

	// find pointers to helpers to speed-up things
	MapConditions::iterator itConditions, itConditionsEnd = m_Conditions.end();
	for ( itConditions = m_Conditions.begin(); itConditions != itConditionsEnd; ++itConditions )
	{
		CCondition& condition = itConditions->second;
		if ( condition.pUserClass && !condition.sUserHelper.empty() )
			condition.pUserHelper = condition.pUserClass->GetHelper( condition.sUserHelper );
		else
			condition.pUserHelper = NULL;
		if ( condition.pObjectClass && !condition.sObjectHelper.empty() )
			condition.pObjectHelper = condition.pObjectClass->GetHelper( condition.sObjectHelper );
		else
			condition.pObjectHelper = NULL;
	}
}

void CSmartObjects::AddSmartObjectCondition( const SmartObjectCondition& conditionData )
{
	CSmartObjectClass* pUserClass = GetSmartObjectClass( conditionData.sUserClass );
	AIAssert( pUserClass );
	if ( !pUserClass )
	{
		AIWarning( "WARNING: Smart Object class '%s' not found while trying to do CSmartObjects::AddSmartObjectCondition",
			conditionData.sUserClass.c_str() );
		return;
	}

	pUserClass->MarkAsSmartObjectUser();

	CCondition condition;
	condition.iTemplateId = conditionData.iTemplateId;

	condition.pUserClass = pUserClass;
	String2StatePattern( conditionData.sUserState, condition.userStatePattern );

	condition.pObjectClass = GetSmartObjectClass( conditionData.sObjectClass );
	AIAssert( condition.pObjectClass || conditionData.sObjectClass.empty() );
	if ( !condition.pObjectClass && !conditionData.sObjectClass.empty() )
	{
		AIWarning( "WARNING: Smart Object class '%s' not found while trying to do CSmartObjects::AddSmartObjectCondition",
			conditionData.sObjectClass.c_str() );
		return;
	}
	String2StatePattern( conditionData.sObjectState, condition.objectStatePattern );

	condition.sUserHelper = conditionData.sUserHelper.c_str();
	condition.pUserHelper = NULL;
	condition.sObjectHelper = conditionData.sObjectHelper.c_str();
	condition.pObjectHelper = NULL;

	condition.fDistanceFrom = conditionData.fDistanceFrom;
	condition.fDistanceTo = conditionData.fDistanceTo;
	condition.fOrientationLimit = conditionData.fOrientationLimit;
	condition.fOrientationToTargetLimit = conditionData.fOrientationToTargetLimit;

	condition.fMinDelay = conditionData.fMinDelay;
	condition.fMaxDelay = conditionData.fMaxDelay;
	condition.fMemory = conditionData.fMemory;

	condition.fProximityFactor = conditionData.fProximityFactor;
	condition.fOrientationFactor = conditionData.fOrientationFactor;
	condition.fVisibilityFactor = conditionData.fVisibilityFactor;
	condition.fRandomnessFactor = conditionData.fRandomnessFactor;

	condition.fLookAtPerc = conditionData.fLookAtOnPerc;
	String2States(conditionData.sUserPreActionState, condition.userPreActionStates );
	String2States(conditionData.sObjectPreActionState, condition.objectPreActionStates );
	condition.eActionType = conditionData.eActionType;
	condition.sAction = conditionData.sAction;
	condition.bEarlyPathRegeneration = conditionData.bEarlyPathRegeneration;
	String2States(conditionData.sUserPostActionState, condition.userPostActionStates );
	String2States(conditionData.sObjectPostActionState, condition.objectPostActionStates );

	condition.iMaxAlertness = conditionData.iMaxAlertness;
	condition.bEnabled = conditionData.bEnabled;
	condition.sName = conditionData.sName;
	condition.sDescription = conditionData.sDescription;
	condition.sFolder = conditionData.sFolder;
	condition.iOrder = conditionData.iOrder;
	condition.sEvent = conditionData.sEvent;
	condition.sChainedUserEvent = conditionData.sChainedUserEvent;
	condition.sChainedObjectEvent = conditionData.sChainedObjectEvent;

	condition.iRuleType = conditionData.iRuleType;
	condition.sEntranceHelper = conditionData.sEntranceHelper;
	condition.sExitHelper = conditionData.sExitHelper;

	condition.fApproachSpeed = conditionData.fApproachSpeed;
	condition.iApproachStance = conditionData.iApproachStance;
	condition.sAnimationHelper = conditionData.sAnimationHelper;
	condition.sApproachHelper = conditionData.sApproachHelper;
	condition.fStartRadiusTolerance = conditionData.fStartRadiusTolerance;
	condition.fDirectionTolerance = conditionData.fDirectionTolerance;
	condition.fTargetRadiusTolerance = conditionData.fTargetRadiusTolerance;

/*
	MapConditions::iterator it, itEnd = m_Conditions.end();
	for ( it = m_Conditions.begin(); it != itEnd; ++it )
		if ( condition.iOrder <= it->second.iOrder )
			++it->second.iOrder;
*/

	MapConditions::iterator where = m_Conditions.insert(std::make_pair( pUserClass, condition ));
	CCondition* pCondition = & where->second;

	if ( !condition.sEvent.empty() )
	{
		CEvent* pEvent = String2Event( condition.sEvent );
		pEvent->m_Conditions.insert(std::make_pair( pUserClass, pCondition ));
	}
	else if ( pCondition->bEnabled && pCondition->iRuleType == 0 )
	{
		// optimization: each user class should know what rules to use during update
		for ( int i = 0; i <= CLAMP( condition.iMaxAlertness, 0, 2 ); ++i )
			pUserClass->m_vActiveUpdateRules[i].push_back( pCondition );
	}
}

void CSmartObjects::ClearConditions()
{
	Reset();
	m_Conditions.clear();

	// only remove pointers to conditions
	// keep the events to preserve their descriptions
	MapEvents::iterator it = m_mapEvents.begin(), itEnd = m_mapEvents.end();
	MapEvents::iterator next;
	while ( it != itEnd )
	{
		next = it; ++next;
		if ( it->second.m_Conditions.size() )
		{
			it->second.m_Conditions.clear();
		}
		else
		{
			// an event not associated with any rule - delete the event
			m_mapEvents.erase( it );
		}
		it = next;
	}

	// clean the SOClasses pointing to active rules
	CSmartObjectClass::MapClassesByName::iterator itClasses = CSmartObjectClass::g_AllByName.begin();
	while ( itClasses != CSmartObjectClass::g_AllByName.end() )
	{
		itClasses->second->m_vActiveUpdateRules[0].clear();
		itClasses->second->m_vActiveUpdateRules[1].clear();
		itClasses->second->m_vActiveUpdateRules[2].clear();
		++itClasses;
	}
}

void CSmartObjects::Reset()
{
	CClassTemplateData::m_pHelperMtl = NULL;

	CSmartObjectClass::MapClassesByName::iterator itClasses = CSmartObjectClass::g_AllByName.begin();
	while ( itClasses != CSmartObjectClass::g_AllByName.end() )
	{
		delete (itClasses++)->second;
	}
	CSmartObjectClass::g_AllByName.clear();
	m_vDebugUse.clear();
}

void CSmartObjects::SoftReset()
{
	CClassTemplateData::m_pHelperMtl = NULL;

	CSmartObjectClass::MapClassesByName::iterator itClasses = CSmartObjectClass::g_AllByName.begin();
	while ( itClasses != CSmartObjectClass::g_AllByName.end() )
	{
		itClasses->second->m_MapObjectsByPos.clear();
		++itClasses;
	}
	m_vDebugUse.clear();
	//m_SmartObjects.clear();

	// re-register entities with the smart objects system
	SEntitySpawnParams params;
	IEntityIt* it = gEnv->pEntitySystem->GetEntityIterator();
	while ( !it->IsEnd() )
	{
		IEntity* pEntity = it->Next();
		DoRemove( pEntity );
		OnSpawn( pEntity, params );
	}
	it->Release();

	RebuildNavigation();
	m_bRecalculateUserSize = true;
}

void CSmartObjects::RecalculateUserSize()
{
	m_bRecalculateUserSize = false;

	// find navigation rules and add the size of the user to the used object class
	MapConditions::iterator itConditions, itConditionsEnd = m_Conditions.end();
	for ( itConditions = m_Conditions.begin(); itConditions != itConditionsEnd; ++itConditions )
	{
		// ignore event rules
		if ( !itConditions->second.sEvent.empty() )
			continue;

		CSmartObjectClass* pClass = itConditions->first;
		CCondition* pCondition = &itConditions->second;
		if ( pCondition->bEnabled && pCondition->pObjectClass && pCondition->iRuleType == 1 )
		{
			pCondition->pObjectClass->AddHelperLink( pCondition );
			pCondition->pObjectClass->AddNavUserClass( pCondition->pUserClass );
		}
	}
}

float CSmartObjects::CalculateDelayTime( CSmartObject* pUser, const Vec3& posUser,
										CSmartObject* pObject, const Vec3& posObject, CCondition* pCondition ) const
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

	if ( !pCondition->bEnabled )
		return -1.0f;

	if ( pUser == pObject )
	{
		// this is a relation on itself
		// just ignore all calculations (except randomness)

		float delta = 0.5f;

		// calculate randomness
		if ( pCondition->fRandomnessFactor )
		{
			delta *= 1.0f + ( pUser->m_fRandom + pObject->m_fRandom - 0.5f )*pCondition->fRandomnessFactor;
		}

		return pCondition->fMinDelay + (pCondition->fMaxDelay-pCondition->fMinDelay)*(1.0f-delta);
	}

	Vec3 direction = posObject - posUser;

	float limitFrom2 = pCondition->fDistanceFrom;
	float limitTo2 = pCondition->fDistanceTo;

//	if ( direction.x > limit2 || direction.x < -limit2 ||
//		 direction.y > limit2 || direction.y < -limit2 ||
//		 direction.z > limit2 || direction.z < -limit2 )
//		return -1.0f;

	limitFrom2 *= limitFrom2;
	limitTo2 *= limitTo2;
	float distance2 = direction.GetLengthSquared();

	if ( pCondition->fDistanceTo && (distance2 > limitTo2 || distance2 < limitFrom2) )
		return -1.0f;

	float dot = 2.0f;
	if ( pCondition->fOrientationLimit < 360.0f )
	{
		float cosLimit = 1.0f;
		if ( pCondition->fOrientationLimit != 0 )
			cosLimit = cos( pCondition->fOrientationLimit / 360.0f * 3.1415926536f ); // limit is expressed as FOV (360 means unlimited)

		dot = direction.normalized().Dot( pUser->GetOrientation(pCondition->pUserHelper) );
		if ( pCondition->fOrientationLimit < 0 )
		{
			dot = -dot;
			cosLimit = -cosLimit;
		}

		if ( dot < cosLimit )
			return -1.0f;
	}

	float delta = 0.0f;
	float offset = 0.0f;
	float divider = 0.0f;

	// calculate distance
	if ( pCondition->fProximityFactor )
	{
		delta += (1.0f - sqrt(distance2/limitTo2)) * pCondition->fProximityFactor;
		if ( pCondition->fProximityFactor > 0 )
			divider += pCondition->fProximityFactor;
		else
		{
			offset -= pCondition->fProximityFactor;
			divider -= pCondition->fProximityFactor;
		}
	}

	// calculate orientation
	if ( pCondition->fOrientationFactor )
	{
		// TODO: Could be optimized
		float cosLimit = -1.0f;
		if ( pCondition->fOrientationLimit != 0 && pCondition->fOrientationLimit < 360.0f && pCondition->fOrientationLimit >= -360.0f )
			cosLimit = cos( pCondition->fOrientationLimit / 360.0f * 3.1415926536f ); // limit is expressed as FOV (360 means unlimited)
		if ( pCondition->fOrientationLimit < 0 )
			cosLimit = -cosLimit;

		if ( dot == 2.0f )
		{
			dot = direction.normalized().Dot( pUser->GetOrientation(pCondition->pObjectHelper) );
			if ( pCondition->fOrientationLimit < 0 )
				dot = -dot;
		}

		float factor;
		if ( abs(pCondition->fOrientationLimit) <= 0.01f )
			factor = dot >= 0.99f ? 1.0f : 0.0f;
		else
			factor = (dot - cosLimit) / (1.0f - cosLimit);

		delta += factor * pCondition->fOrientationFactor;
		if ( pCondition->fOrientationFactor > 0 )
			divider += pCondition->fOrientationFactor;
		else
		{
			offset -= pCondition->fOrientationFactor;
			divider -= pCondition->fOrientationFactor;
		}
	}

	// calculate visibility
	if ( pCondition->fVisibilityFactor )
	{
		CPuppet* pPuppet = pUser->GetPuppet();
		if ( !pPuppet || pPuppet->IsPointInFOVCone(posObject) ) // is in visual range?
			if (GetAISystem()->CheckPointsVisibility( posUser, posObject, 0, pUser->GetPhysics(), pObject->GetPhysics() )) // is physically visible?
				delta += pCondition->fVisibilityFactor;
		if ( pCondition->fVisibilityFactor > 0 )
			divider += pCondition->fVisibilityFactor;
		else
		{
			offset -= pCondition->fVisibilityFactor;
			divider -= pCondition->fVisibilityFactor;
		}
	}

	if ( !pCondition->fProximityFactor && !pCondition->fOrientationFactor && !pCondition->fVisibilityFactor )
		delta = 0.5f;

	// calculate randomness
	if ( pCondition->fRandomnessFactor )
	{
		delta *= 1.0f + ( pUser->m_fRandom + pObject->m_fRandom - 0.5f )*pCondition->fRandomnessFactor;
	}

	if ( divider )
		delta = (delta+offset) / divider;

	return pCondition->fMinDelay + (pCondition->fMaxDelay-pCondition->fMinDelay)*(1.0f-delta);
}

/*
 * follows a try to update smart objects by condition
 *

void CSmartObjects::Update()
{
	m_UpdateStruct.m_Count = 100;

	MapConditions::const_iterator itConditionsEnd = m_Conditions.end();
	MapConditions::iterator & itCondition = m_UpdateStruct.m_itCondition;

	while ( m_UpdateStruct.m_Count-- > 0 )
	{
		if ( itCondition == itConditionsEnd )
			itCondition = m_Conditions.begin();
		if ( itCondition != itConditionsEnd )
		{
			const PairClassState& pairClassState = itCondition->first;
			CCondition* pCondition = &itCondition->second;
			if ( pCondition->bEnabled )
				UpdateCondition( pairClassState.first, pairClassState.second, pCondition );
			++itCondition;
			if ( itCondition != itConditionsEnd )
				break;
		}
		else
			break;
	}
}

void CSmartObjects::UpdateCondition( CSmartObjectClass* pClass, CSmartObject::CState state, CCondition* pCondition )
{
	MapSmartObjectsByClass::const_iterator itByClassEnd = m_SmartObjects.end();
	MapSmartObjectsByClass::iterator & itByClass = m_UpdateStruct.m_itByClass;

	while ( m_UpdateStruct.m_Count-- > 0 && itByClass != itByClassEnd && itByClass->first == pClass )
	{
		MapSmartObjectsByState & mapByState = itByClass->second;

		MapSmartObjectsByState::const_iterator itByStateEnd = mapByState.end();
		MapSmartObjectsByState::iterator & itByState = m_UpdateStruct.m_itByState;

		while ( m_UpdateStruct.m_Count-- > 0 && itByState != itByStateEnd && itByState->first == state )
		{
			CSmartObject* pSmartObjectUser = itByState->second;

			if ( pCondition->iMaxAlertness < 2 )
			{
				CAIObject* pAIObject = pSmartObjectUser->GetAIObject();
				if ( pAIObject )
				{
					int alertness = pAIObject->m_bEnabled ? pAIObject->GetProxy()->GetAlertnessState() : 1000;
					if ( alertness > pCondition->iMaxAlertness )
						continue;
				}
			}

			UpdateUser( pSmartObjectUser, pCondition );
		}
	}
}

void CSmartObjects::UpdateUser( CSmartObject* pUser, CCondition* pCondition )
{
	MapSmartObjectsByClass::iterator itClass = m_SmartObjects.find( pCondition->pObjectClass );
	if ( itClass != m_SmartObjects.end() )
	{
		Vec3 pos = pUser->GetPos();
		Vec3 bbMin = pos;
		Vec3 bbMax = pos;
		if ( pCondition->fDistanceLimit > 0 )
		{
			Vec3 d( pCondition->fDistanceLimit, pCondition->fDistanceLimit, pCondition->fDistanceLimit );
			bbMin -= d;
			bbMax += d;
		}

		MapSmartObjectsByState mapByState = itClass->second;

		MapSmartObjectsByState::const_iterator itByStateEnd = mapByState.end();
		MapSmartObjectsByState::iterator & itObject = m_UpdateStruct.m_itObject;

		if ( itObject == itByStateEnd )
			itObject = mapByState.find( pCondition->objectState );

		while ( m_UpdateStruct.m_Count-- > 0 && itObject != itByStateEnd && itObject->first == pCondition->objectState )
		{
			CSmartObject* pObject = itObject->second;

			// Check range first since it could be faster
			if ( pCondition->fDistanceLimit > 0 )
			{
				Vec3 objectPos = pObject->GetPos();
				if ( objectPos.x < bbMin.x || objectPos.x > bbMax.x ||
					objectPos.y < bbMin.y || objectPos.y > bbMax.y ||
					objectPos.z < bbMin.z || objectPos.z > bbMax.z )
						continue;
			}

			// Check the geometry model if needed
			if ( !pCondition->sObjectModel.empty() )
			{
				string strModel = pObject->GetModel();
				strModel.replace( '\\', '/' );
				if ( !strModel || stricmp( pCondition->sObjectModel, strModel ) )
					continue;
			}

			UpdateObject( pUser, pObject, pCondition );
		}
	}
}

void CSmartObjects::UpdateObject( CSmartObject* pUser, CSmartObject* pObject, CCondition* pCondition )
{
	// define a map of event updates
	typedef std::pair< CSmartObject*, CCondition* > PairObjectCondition;
	typedef std::map< PairObjectCondition, float > MapDelayTimes;
	MapDelayTimes mapDelayTimes;

	// calculated delta times should be stored only for now.
	// later existing events will be updated and for new objects new events will be added
	PairObjectCondition poc( pObject, pCondition );
	float fDelayTime = CalculateDelayTime( pUser, pObject, pCondition );
	if ( fDelayTime > 0.0f )
		mapDelayTimes.insert(std::make_pair( poc, fTimeElapsed/fDelayTime ));
	else if ( fDelayTime == 0.0f )
		mapDelayTimes.insert(std::make_pair( poc, 1.0f ));
}

*
* end of updating smart objects by condition
*/

int CSmartObjects::TriggerEvent( const char* sEventName, IEntity*& pUser, IEntity*& pObject, QueryEventMap* pQueryEvents /*= NULL*/, const Vec3* pExtraPoint /*= NULL*/, bool bHighPriority /*= false*/ )
{
	if ( !sEventName || !*sEventName )
		return 0;

	CSmartObject* pSOUser = NULL;
	if ( pUser )
	{
		if ( pUser->IsHidden() )
		{
			// ignore hidden entities
			return 0;
		}
		pSOUser = IsSmartObject( pUser );
		if ( !pSOUser )
			return 0; // requested user is not a smart object
	}
	CSmartObject* pSOObject = NULL;
	if ( pObject )
	{
		if ( pObject->IsHidden() )
		{
			// ignore hidden entities
			return 0;
		}
		pSOObject = IsSmartObject( pObject );
		if ( !pSOObject )
			return 0; // requested object is not a smart object
	}

	if ( pSOUser && pSOObject )
		return TriggerEventUserObject( sEventName, pSOUser, pSOObject, pQueryEvents, pExtraPoint, bHighPriority );
	else if ( pSOUser )
		return TriggerEventUser( sEventName, pSOUser, pQueryEvents, &pObject, pExtraPoint, bHighPriority );
	else if ( pSOObject )
		return TriggerEventObject( sEventName, pSOObject, pQueryEvents, &pUser, pExtraPoint, bHighPriority );

	// specific user or object is not requested
	// check the rules on all users and objects

	float minDelay = FLT_MAX;
	CCondition* pMinRule = NULL;
	CSmartObject* pMinUser = NULL;
	CSmartObject* pMinObject = NULL;
	CEvent* pEvent = String2Event( sEventName );

	MapPtrConditions::iterator itRules, itRulesEnd = pEvent->m_Conditions.end();
	for ( itRules = pEvent->m_Conditions.begin(); itRules != itRulesEnd; ++itRules )
	{
		CCondition* pRule = itRules->second;
		if ( !pRule->bEnabled )
			continue;

		// if we already have found a rule with a delay less than min. delay of this rule then ignore this rule
		if ( !pQueryEvents && minDelay < pRule->fMinDelay )
			continue;

		CSmartObjectClass::MapSmartObjectsByPos& mapByPos = pRule->pUserClass->m_MapObjectsByPos;
		if ( mapByPos.empty() )
			continue;

		CSmartObjectClass::MapSmartObjectsByPos::iterator itByPos, itByPosEnd = mapByPos.end();
		for ( itByPos = mapByPos.begin(); itByPos != itByPosEnd; ++itByPos )
		{
			pSOUser = itByPos->second;

			// ignore hidden entities
			if ( pSOUser->IsHidden() )
				continue;

			// proceed with next user if this one doesn't match states
			if ( !pRule->userStatePattern.Matches( pSOUser->GetStates() ) )
				continue;

			// now for this user check all objects matching the rule's conditions

			Vec3 soPos = pSOUser->GetPos();
			CPuppet* pPuppet = pSOUser->GetPuppet();
			IAIObject* pAttTarget = pPuppet ? pPuppet->GetAttentionTarget() : NULL;

			// ignore this user if it has no attention target and the rule says it's needed
			if ( !pExtraPoint && !pAttTarget && pRule->fOrientationToTargetLimit < 360.0f )
				continue;

			int alertness = pPuppet ? (pPuppet->IsEnabled() ? pPuppet->GetProxy()->GetAlertnessState() : 1000) : 0;
			if ( alertness > pRule->iMaxAlertness )
				continue; // ignore this user - too much alerted

			// first check does it maybe operate only on itself
			if ( !pRule->pObjectClass )
			{
				// calculate delay time
				float fDelayTime = CalculateDelayTime( pSOUser, soPos, pSOUser, soPos, pRule );
				if ( fDelayTime >= 0.0f && fDelayTime <= minDelay )
				{
					// mark this as best
					minDelay = fDelayTime;
					pMinUser = pMinObject = pSOUser;
					pMinRule = pRule;
				}

				// add it to the query list
				if ( pQueryEvents && fDelayTime >= 0.0f )
				{
					CQueryEvent q;
					q.pUser = pSOUser;
					q.pObject = pSOUser;
					q.pRule = pRule;
					q.pChainedUserEvent = NULL;
					q.pChainedObjectEvent = NULL;
					pQueryEvents->insert( std::make_pair(fDelayTime, q) );
				}

				// proceed with next user
				continue;
			}

			// get species and group id of the user
			int species = -1;
			int groupId = -1;
			CAIActor* pUserActor = CastToCAIActorSafe(pSOUser->GetAI());
			if ( pUserActor )
			{
				groupId = pUserActor->GetGroupId();
				species = pUserActor->GetParameters().m_nSpecies;
			}

			// adjust pos if pUserHelper is used
			Vec3 pos = pRule->pUserHelper ? pSOUser->GetHelperPos( pRule->pUserHelper ) : soPos;

			Vec3 bbMin = pos;
			Vec3 bbMax = pos;

			float limitTo = pRule->fDistanceTo;
			// adjust limit if pUserHelper is used
			if ( pRule->pUserHelper )
				limitTo += pRule->pUserHelper->qt.t.GetLength();
			// adjust limit if pObjectHelper is used
			if ( pRule->pObjectHelper )
				limitTo += pRule->pObjectHelper->qt.t.GetLength();
			Vec3 d( limitTo, limitTo, limitTo );
			bbMin -= d;
			bbMax += d;

			// calculate the limit in advance
			float orientationToTargetLimit = -2.0f; // unlimited
			if ( pRule->fOrientationToTargetLimit < 360.0f )
			{
				if ( pRule->fOrientationToTargetLimit == 0 )
					orientationToTargetLimit = 1.0f;
				else
					orientationToTargetLimit = cos( pRule->fOrientationToTargetLimit / 360.0f * 3.1415926536f ); // limit is expressed as FOV (360 means unlimited)

				if ( pRule->fOrientationToTargetLimit < 0 )
					orientationToTargetLimit = -orientationToTargetLimit;
			}

			// check all objects (but not the user) matching with condition's class and state
			CSmartObjectClass::MapSmartObjectsByPos& mapObjectByPos = pRule->pObjectClass->m_MapObjectsByPos;
			if ( mapObjectByPos.empty() )
				continue;

			CSmartObjectClass::MapSmartObjectsByPos::iterator itObjectByPos, itObjectByPosEnd = mapObjectByPos.upper_bound( bbMax.x );
			for ( itObjectByPos = mapObjectByPos.lower_bound( bbMin.x ); itObjectByPos != itObjectByPosEnd; ++itObjectByPos )
			{
				pSOObject = itObjectByPos->second;

				// the user must be different than target object!!!
				if ( pSOUser == pSOObject )
					continue;

				// ignore hidden entities
				if ( pSOObject->IsHidden() )
					continue;

				// Check range first since it could be faster
				Vec3 objectPos = pRule->pObjectHelper ? pSOObject->GetHelperPos( pRule->pObjectHelper ) : pSOObject->GetPos();
				assert( bbMin.x-limitTo <= objectPos.x && objectPos.x <= bbMax.x+limitTo );
				if ( objectPos.y < bbMin.y || objectPos.y > bbMax.y ||
					objectPos.z < bbMin.z || objectPos.z > bbMax.z )
						continue;

				// Also check the orientation limit to user's attention target
				if ( pRule->fOrientationToTargetLimit < 360.0f )
				{
					Vec3 objectDir = pSOObject->GetOrientation( pRule->pObjectHelper );
					Vec3 targetDir = (pExtraPoint ? *pExtraPoint : pAttTarget->GetPos()) - objectPos;
					targetDir.NormalizeSafe();
					float dot = objectDir.Dot( targetDir );

					if ( pRule->fOrientationToTargetLimit < 0 )
						dot = -dot;

					if ( dot < orientationToTargetLimit )
						continue;
				}

				// add virtual states
//				IAIObject* pAIObjectObject = pSOObject->GetAI();
				CAIActor* pAIObjectObject = CastToCAIActorSafe(pSOObject->GetAI());
				bool attTarget = false;
				bool sameGroupId = false;
				bool sameSpecies = false;
				if ( pAIObjectObject )
				{
					// check is the object attention target of the user
					attTarget = pPuppet && pPuppet->GetAttentionTarget() == pSOObject->GetAI();		//pAIObjectObject;
					if ( attTarget )
						pSOObject->m_States.insert( m_StateAttTarget );

					// check are the user and the object in the same group and species
					if ( groupId >= 0 || species >= 0 )
					{
						// check same species
						int spObject = pAIObjectObject->GetParameters().m_nSpecies;
						sameSpecies = species == spObject;
						if ( sameSpecies )
						{
							pSOObject->m_States.insert( m_StateSameSpecies );

							// if they are same species check are they in same group
							sameGroupId = groupId == pAIObjectObject->GetGroupId();
							if ( sameGroupId )
								pSOObject->m_States.insert( m_StateSameGroup );
						}
						else if ( species < 0 || spObject < 0 )
						{
							// if any of them has no species check are they in same group
							sameGroupId = groupId == pAIObjectObject->GetGroupId();
							if ( sameGroupId )
								pSOObject->m_States.insert( m_StateSameGroup );
						}
					}
				}

				// check object's state pattern and then remove virtual states
				bool bMatches = pRule->objectStatePattern.Matches( pSOObject->GetStates() );
				if ( attTarget )
					pSOObject->m_States.erase( m_StateAttTarget );
				if ( sameSpecies )
					pSOObject->m_States.erase( m_StateSameSpecies );
				if ( sameGroupId )
					pSOObject->m_States.erase( m_StateSameGroup );

				// ignore this object if it doesn't match precondition state
				if ( !bMatches )
					continue;

				// calculate delay time
				float fDelayTime = CalculateDelayTime( pSOUser, pos, pSOObject, objectPos, pRule );
				if ( fDelayTime >= 0.0f && fDelayTime <= minDelay )
				{
					minDelay = fDelayTime;
					pMinUser = pSOUser;
					pMinObject = pSOObject;
					pMinRule = pRule;
				}

				// add it to the query list
				if ( pQueryEvents && fDelayTime >= 0.0f )
				{
					CQueryEvent q;
					q.pUser = pSOUser;
					q.pObject = pSOObject;
					q.pRule = pRule;
					q.pChainedUserEvent = NULL;
					q.pChainedObjectEvent = NULL;
					pQueryEvents->insert( std::make_pair(fDelayTime, q) );
				}
			}
		}
	}

	if ( !pMinRule )
		return 0;

	// is querying only?
	if ( pQueryEvents )
		return -1;

	int id = GetAISystem()->AllocGoalPipeId();
	UseSmartObject( pMinUser, pMinObject, pMinRule, id, bHighPriority );
	pUser = pMinUser->GetEntity();
	pObject = pMinObject ? pMinObject->GetEntity() : NULL;
	return !pMinRule->sAction.empty() && pMinRule->eActionType != eAT_None ? id : -1;
}

int CSmartObjects::TriggerEventUserObject( const char* sEventName, CSmartObject* pUser, CSmartObject* pObject, QueryEventMap* pQueryEvents, const Vec3* pExtraPoint, bool bHighPriority /*=false*/ )
{
	float minDelay = FLT_MAX;
	CCondition* pMinRule = NULL;
	CEvent* pEvent = String2Event( sEventName );

	CPuppet* pPuppet = pUser->GetPuppet();
	IAIObject* pAttTarget = pPuppet ? pPuppet->GetAttentionTarget() : NULL;
	int alertness = pPuppet ? (pPuppet->IsEnabled() ? pPuppet->GetProxy()->GetAlertnessState() : 1000) : 0;

	CSmartObjectClasses::iterator itUserClasses, itUserClassesEnd = pUser->GetClasses().end();
	for ( itUserClasses = pUser->GetClasses().begin(); itUserClasses != itUserClassesEnd; ++itUserClasses )
	{
		CSmartObjectClass* pUserClass = *itUserClasses;
		MapPtrConditions::iterator itRules, itRulesEnd = pEvent->m_Conditions.upper_bound( pUserClass );
		for ( itRules = pEvent->m_Conditions.lower_bound( pUserClass ); itRules != itRulesEnd; ++itRules )
		{
			CCondition* pRule = itRules->second;
			if ( !pRule->bEnabled )
				continue;

			// if we already have found a rule with a delay less than min. delay of this rule then ignore this rule
			if ( !pQueryEvents && minDelay < pRule->fMinDelay )
				continue;

			// check interaction and rule types
			if ( pUser != pObject && !pRule->pObjectClass )
				continue;
			if ( pUser == pObject && pRule->pObjectClass )
				continue;

			// proceed with the next rule if user doesn't match states with this one
			if ( !pRule->userStatePattern.Matches( pUser->GetStates() ) )
				continue;

			if ( alertness > pRule->iMaxAlertness )
				continue; // ignore this rule - too much alerted

			// ignore this user if it has no attention target and the rule says it's needed
			if ( !pExtraPoint && !pAttTarget && pRule->fOrientationToTargetLimit < 360.0f )
				continue;

			// adjust pos if helpers are used
			Vec3 pos = pRule->pUserHelper ? pUser->GetHelperPos( pRule->pUserHelper ) : pUser->GetPos();

			// now for this user check the rule with requested object

			if ( pUser == pObject )
			{
				// calculate delay time
				float fDelayTime = CalculateDelayTime( pUser, pos, pUser, pos, pRule );
				if ( fDelayTime >= 0.0f && fDelayTime <= minDelay )
				{
					minDelay = fDelayTime;
					pMinRule = pRule;
				}

				// add it to the query list
				if ( pQueryEvents && fDelayTime >= 0.0f )
				{
					CQueryEvent q;
					q.pUser = pUser;
					q.pObject = pUser;
					q.pRule = pRule;
					q.pChainedUserEvent = NULL;
					q.pChainedObjectEvent = NULL;
					pQueryEvents->insert( std::make_pair(fDelayTime, q) );
				}

				continue;
			}

			// ignore rules which object's class is different
			CSmartObjectClasses& objectClasses = pObject->GetClasses();
			if ( std::find( objectClasses.begin(), objectClasses.end(), pRule->pObjectClass ) == objectClasses.end() )
				continue;

			// adjust pos if helpers are used
			Vec3 objectPos = pRule->pObjectHelper ? pObject->GetHelperPos( pRule->pObjectHelper ) : pObject->GetPos();

			// check the orientation limit to target
			if ( pRule->fOrientationToTargetLimit < 360.0f )
			{
				float cosLimit = 1.0f;
				if ( pRule->fOrientationToTargetLimit != 0 )
					cosLimit = cos( pRule->fOrientationToTargetLimit / 360.0f * 3.1415926536f ); // limit is expressed as FOV (360 means unlimited)

				Vec3 objectDir = pObject->GetOrientation( pRule->pObjectHelper );
				Vec3 targetDir = (pExtraPoint ? *pExtraPoint : pAttTarget->GetPos()) - objectPos;
				targetDir.NormalizeSafe();
				float dot = objectDir.Dot( targetDir );

				if ( pRule->fOrientationToTargetLimit < 0 )
				{
					dot = -dot;
					cosLimit = -cosLimit;
				}

				if ( dot < cosLimit )
					continue;
			}

			// get species and group id of the user
			int species = -1;
			int groupId = -1;
			CAIActor* pUserActor = CastToCAIActorSafe(pUser->GetAI());
			if ( pUserActor )
			{
				groupId = pUserActor->GetGroupId();
				species = pUserActor->GetParameters().m_nSpecies;
			}

			// add virtual states
//			IAIObject* pAIObjectObject = pObject->GetAI();
			CAIActor* pAIObjectObject = CastToCAIActorSafe(pObject->GetAI());
			bool attTarget = false;
			bool sameGroupId = false;
			bool sameSpecies = false;
			if ( pAIObjectObject )
			{
				// check is the object attention target of the user
				attTarget = pPuppet && pPuppet->GetAttentionTarget() == pObject->GetAI();
				if ( attTarget )
					pObject->m_States.insert( m_StateAttTarget );

				// check are the user and the object in the same group and species
				if ( groupId >= 0 || species >= 0 )
				{
					// check same species
					int spObject = pAIObjectObject->GetParameters().m_nSpecies;
					sameSpecies = species == spObject;
					if ( sameSpecies )
					{
						pObject->m_States.insert( m_StateSameSpecies );

						// if they are same species check are they in same group
						sameGroupId = groupId == pAIObjectObject->GetGroupId();
						if ( sameGroupId )
							pObject->m_States.insert( m_StateSameGroup );
					}
					else if ( species < 0 || spObject < 0 )
					{
						// if any of them has no species check are they in same group
						sameGroupId = groupId == pAIObjectObject->GetGroupId();
						if ( sameGroupId )
							pObject->m_States.insert( m_StateSameGroup );
					}
				}
			}

			// check object's state pattern and then remove virtual states
			bool bMatches = pRule->objectStatePattern.Matches( pObject->GetStates() );
			if ( attTarget )
				pObject->m_States.erase( m_StateAttTarget );
			if ( sameSpecies )
				pObject->m_States.erase( m_StateSameSpecies );
			if ( sameGroupId )
				pObject->m_States.erase( m_StateSameGroup );

			// ignore this object if it doesn't match precondition state
			if ( !bMatches )
				continue;

			// calculate delay time
			float fDelayTime = CalculateDelayTime( pUser, pos, pObject, objectPos, pRule );
			if ( fDelayTime >= 0.0f && fDelayTime <= minDelay )
			{
				minDelay = fDelayTime;
				pMinRule = pRule;
			}

			// add it to the query list
			if ( pQueryEvents && fDelayTime >= 0.0f )
			{
				CQueryEvent q;
				q.pUser = pUser;
				q.pObject = pObject;
				q.pRule = pRule;
				q.pChainedUserEvent = NULL;
				q.pChainedObjectEvent = NULL;
				pQueryEvents->insert( std::make_pair(fDelayTime, q) );
			}
		}
	}

	if ( !pMinRule )
		return 0;

	// is querying only?
	if ( pQueryEvents )
		return -1;

	int id = GetAISystem()->AllocGoalPipeId();
	UseSmartObject( pUser, pObject, pMinRule, id, bHighPriority );
	return !pMinRule->sAction.empty() && pMinRule->eActionType != eAT_None ? id : -1;
}

int CSmartObjects::TriggerEventUser( const char* sEventName, CSmartObject* pUser, QueryEventMap* pQueryEvents, IEntity** ppObjectEntity, const Vec3* pExtraPoint, bool bHighPriority /*=false*/ )
{
	*ppObjectEntity = NULL;
	CSmartObject* pObject = NULL;

	// check the rules for all objects

	float minDelay = FLT_MAX;
	CCondition* pMinRule = NULL;
	CSmartObject* pMinObject = NULL;
	CEvent* pEvent = String2Event( sEventName );

	CSmartObjectClasses::iterator itUserClasses, itUserClassesEnd = pUser->GetClasses().end();
	for ( itUserClasses = pUser->GetClasses().begin(); itUserClasses != itUserClassesEnd; ++itUserClasses )
	{
		CSmartObjectClass* pUserClass = *itUserClasses;

		MapPtrConditions::iterator itRules, itRulesEnd = pEvent->m_Conditions.upper_bound( pUserClass );
		for ( itRules = pEvent->m_Conditions.lower_bound( pUserClass ); itRules != itRulesEnd; ++itRules )
		{
			CCondition* pRule = itRules->second;
			if ( !pRule->bEnabled )
				continue;

			// if we already have found a rule with a delay less than min. delay of this rule then ignore this rule
			if ( !pQueryEvents && minDelay < pRule->fMinDelay )
				continue;

			// proceed with next rule if the user doesn't match states
			if ( !pRule->userStatePattern.Matches( pUser->GetStates() ) )
				continue;

			Vec3 soPos = pUser->GetPos();

			CPuppet* pPuppet = pUser->GetPuppet();
			IAIObject* pAttTarget = pPuppet ? pPuppet->GetAttentionTarget() : NULL;

			// ignore this rule if the user has no attention target and the rule says it's needed
			if ( !pExtraPoint && !pAttTarget && pRule->fOrientationToTargetLimit < 360.0f )
				continue;

			int alertness = pPuppet ? (pPuppet->IsEnabled() ? pPuppet->GetProxy()->GetAlertnessState() : 1000) : 0;
			if ( alertness > pRule->iMaxAlertness )
				continue; // ignore this rule - too much alerted

			// now for this user check all objects matching the rule's conditions

			// first check does it maybe operate only on itself
			if ( !pRule->pObjectClass )
			{
				// calculate delay time
				float fDelayTime = CalculateDelayTime( pUser, soPos, pUser, soPos, pRule );
				if ( fDelayTime >= 0.0f && fDelayTime <= minDelay )
				{
					// mark this as best
					minDelay = fDelayTime;
					pMinObject = pUser;
					pMinRule = pRule;
				}

				// add it to the query list
				if ( pQueryEvents && fDelayTime >= 0.0f )
				{
					CQueryEvent q;
					q.pUser = pUser;
					q.pObject = pUser;
					q.pRule = pRule;
					q.pChainedUserEvent = NULL;
					q.pChainedObjectEvent = NULL;
					pQueryEvents->insert( std::make_pair(fDelayTime, q) );
				}

				// proceed with next rule
				continue;
			}

			// get species and group id of the user
			int species = -1;
			int groupId = -1;
			CAIActor* pUserActor = CastToCAIActorSafe(pUser->GetAI());
			if ( pUserActor )
			{
				groupId = pUserActor->GetGroupId();
				species = pUserActor->GetParameters().m_nSpecies;
			}

			// adjust pos if pUserHelper is used
			Vec3 pos = pRule->pUserHelper ? pUser->GetHelperPos( pRule->pUserHelper ) : soPos;

			Vec3 bbMin = pos;
			Vec3 bbMax = pos;

			float limitTo = pRule->fDistanceTo;
			// adjust limit if pUserHelper is used
			if ( pRule->pUserHelper)
				limitTo += pRule->pUserHelper->qt.t.GetLength();
			// adjust limit if pObjectHelper is used
			if ( pRule->pObjectHelper )
				limitTo += pRule->pObjectHelper->qt.t.GetLength();
			Vec3 d( limitTo, limitTo, limitTo );
			bbMin -= d;
			bbMax += d;

			// calculate the limit in advance
			float orientationToTargetLimit = -2.0f; // unlimited
			if ( pRule->fOrientationToTargetLimit < 360.0f )
			{
				if ( pRule->fOrientationToTargetLimit == 0 )
					orientationToTargetLimit = 1.0f;
				else
					orientationToTargetLimit = cos( pRule->fOrientationToTargetLimit / 360.0f * 3.1415926536f ); // limit is expressed as FOV (360 means unlimited)

				if ( pRule->fOrientationToTargetLimit < 0 )
					orientationToTargetLimit = -orientationToTargetLimit;
			}

			// check all objects (but not the user) matching with condition's class and state
			CSmartObjectClass::MapSmartObjectsByPos& mapObjectByPos = pRule->pObjectClass->m_MapObjectsByPos;
			if ( mapObjectByPos.empty() )
				continue;

			CSmartObjectClass::MapSmartObjectsByPos::iterator itObjectByPos, itObjectByPosEnd = mapObjectByPos.upper_bound( bbMax.x );
			for ( itObjectByPos = mapObjectByPos.lower_bound( bbMin.x ); itObjectByPos != itObjectByPosEnd; ++itObjectByPos )
			{
				pObject = itObjectByPos->second;

				// the user can not be the target object!!!
				if ( pUser == pObject )
					continue;

				// ignore hidden entities
				if ( pObject->IsHidden() )
					continue;

				// Check range first since it could be faster
				Vec3 objectPos = pRule->pObjectHelper ? pObject->GetHelperPos( pRule->pObjectHelper ) : pObject->GetPos();
				assert( bbMin.x-limitTo <= objectPos.x && objectPos.x <= bbMax.x+limitTo );
				if ( objectPos.y < bbMin.y || objectPos.y > bbMax.y ||
					objectPos.z < bbMin.z || objectPos.z > bbMax.z )
						continue;

				// Also check the orientation limit to user's attention target
				if ( pRule->fOrientationToTargetLimit < 360.0f )
				{
					Vec3 objectDir = pObject->GetOrientation( pRule->pObjectHelper );
					Vec3 targetDir = (pExtraPoint ? *pExtraPoint : pAttTarget->GetPos()) - objectPos;
					targetDir.NormalizeSafe();
					float dot = objectDir.Dot( targetDir );

					if ( pRule->fOrientationToTargetLimit < 0 )
						dot = -dot;

					if ( dot < orientationToTargetLimit )
						continue;
				}

				// add virtual states
//				IAIObject* pAIObjectObject = pObject->GetAI();
				CAIActor* pAIObjectObject = CastToCAIActorSafe(pObject->GetAI());
				bool attTarget = false;
				bool sameGroupId = false;
				bool sameSpecies = false;
				if ( pAIObjectObject )
				{
					// check is the object attention target of the user
					attTarget = pPuppet && pPuppet->GetAttentionTarget() == pObject->GetAI();
					if ( attTarget )
						pObject->m_States.insert( m_StateAttTarget );

					// check are the user and the object in the same group and species
					if ( groupId >= 0 || species >= 0 )
					{
						// check same species
						int spObject = pAIObjectObject->GetParameters().m_nSpecies;
						sameSpecies = species == spObject;
						if ( sameSpecies )
						{
							pObject->m_States.insert( m_StateSameSpecies );

							// if they are same species check are they in same group
							sameGroupId = groupId == pAIObjectObject->GetGroupId();
							if ( sameGroupId )
								pObject->m_States.insert( m_StateSameGroup );
						}
						else if ( species < 0 || spObject < 0 )
						{
							// if any of them has no species check are they in same group
							sameGroupId = groupId == pAIObjectObject->GetGroupId();
							if ( sameGroupId )
								pObject->m_States.insert( m_StateSameGroup );
						}
					}
				}

				// check object's state pattern and then remove virtual states
				bool bMatches = pRule->objectStatePattern.Matches( pObject->GetStates() );
				if ( attTarget )
					pObject->m_States.erase( m_StateAttTarget );
				if ( sameSpecies )
					pObject->m_States.erase( m_StateSameSpecies );
				if ( sameGroupId )
					pObject->m_States.erase( m_StateSameGroup );

				// ignore this object if it doesn't match precondition state
				if ( !bMatches )
					continue;

				// calculate delay time
				float fDelayTime = CalculateDelayTime( pUser, pos, pObject, objectPos, pRule );
				if ( fDelayTime >= 0.0f && fDelayTime <= minDelay )
				{
					minDelay = fDelayTime;
					pMinObject = pObject;
					pMinRule = pRule;
				}

				// add it to the query list
				if ( pQueryEvents && fDelayTime >= 0.0f )
				{
					CQueryEvent q;
					q.pUser = pUser;
					q.pObject = pObject;
					q.pRule = pRule;
					q.pChainedUserEvent = NULL;
					q.pChainedObjectEvent = NULL;
					pQueryEvents->insert( std::make_pair(fDelayTime, q) );
				}
			}
		}
	}

	if ( !pMinRule )
		return 0;

	// is querying only?
	if ( pQueryEvents )
		return -1;

	int id = GetAISystem()->AllocGoalPipeId();
	UseSmartObject( pUser, pMinObject, pMinRule, id, bHighPriority );
	*ppObjectEntity = pMinObject->GetEntity();
	return !pMinRule->sAction.empty() && pMinRule->eActionType != eAT_None ? id : -1;
}

int CSmartObjects::TriggerEventObject( const char* sEventName, CSmartObject* pObject, QueryEventMap* pQueryEvents, IEntity** ppUserEntity, const Vec3* pExtraPoint, bool bHighPriority /*=false*/ )
{
	// WARNING: Virtual states (AttTarget, SameGroup and SameSpecies) in this function are ignored!!!
	// TODO: Add virtual states...

	if ( !sEventName || !*sEventName )
		return 0;

	*ppUserEntity = NULL;
	CSmartObject* pUser = NULL;

	// check the rules on all users

	float minDelay = FLT_MAX;
	CCondition* pMinRule = NULL;
	CSmartObject* pMinUser = NULL;
	CEvent* pEvent = String2Event( sEventName );

	MapPtrConditions::iterator itRules, itRulesEnd = pEvent->m_Conditions.end();
	for ( itRules = pEvent->m_Conditions.begin(); itRules != itRulesEnd; ++itRules )
	{
		CCondition* pRule = itRules->second;
		if ( !pRule->bEnabled )
			continue;

		// if we already have found a rule with a delay less than min. delay of this rule then ignore this rule
		if ( !pQueryEvents && minDelay < pRule->fMinDelay )
			continue;

		// ignore rules which operate only on the user (without an object)
		if ( !pRule->pObjectClass )
			continue;

		// ignore rules which object's class is different
		if ( std::find( pObject->GetClasses().begin(), pObject->GetClasses().end(), pRule->pObjectClass ) == pObject->GetClasses().end() )
			continue;

		// proceed with next rule if this one doesn't match object states
		if ( !pRule->objectStatePattern.Matches( pObject->GetStates() ) )
			continue;

		// proceed with next rule if there are no users
		CSmartObjectClass::MapSmartObjectsByPos& mapUserByPos = pRule->pUserClass->m_MapObjectsByPos;
		if ( mapUserByPos.empty() )
			continue;

		// get object's position
		Vec3 objectPos = pObject->GetPos();

		// adjust pos if pUserHelper is used
		Vec3 pos = pRule->pObjectHelper ? pObject->GetHelperPos( pRule->pObjectHelper ) : objectPos;

		Vec3 bbMin = pos;
		Vec3 bbMax = pos;

		float limitTo = pRule->fDistanceTo;
		// adjust limit if pUserHelper is used
		if ( pRule->pUserHelper)
			limitTo += pRule->pUserHelper->qt.t.GetLength();
		// adjust limit if pObjectHelper is used
		if ( pRule->pObjectHelper )
			limitTo += pRule->pObjectHelper->qt.t.GetLength();
		Vec3 d( limitTo, limitTo, limitTo );
		bbMin -= d;
		bbMax += d;

		CSmartObjectClass::MapSmartObjectsByPos::iterator itUserByPos, itUserByPosEnd = mapUserByPos.upper_bound( bbMax.x );
		for ( itUserByPos = mapUserByPos.lower_bound( bbMin.x ); itUserByPos != itUserByPosEnd; ++itUserByPos )
		{
			pUser = itUserByPos->second;

			// now check does this user can use the object

			// don't let the user to be the object
			if ( pUser == pObject )
				continue;

			// ignore hidden entities
			if ( pUser->IsHidden() )
				continue;

			// proceed with next user if this one doesn't match states
			if ( !pRule->userStatePattern.Matches( pUser->GetStates() ) )
				continue;

			CPuppet* pPuppet = pUser->GetPuppet();
			IAIObject* pAttTarget = pPuppet ? pPuppet->GetAttentionTarget() : NULL;

			// ignore this user if it has no attention target and the rule says it's needed
			if ( !pExtraPoint && !pAttTarget && pRule->fOrientationToTargetLimit < 360.0f )
				continue;

			int alertness = pPuppet ? (pPuppet->IsEnabled() ? pPuppet->GetProxy()->GetAlertnessState() : 1000) : 0;
			if ( alertness > pRule->iMaxAlertness )
				continue; // ignore this user - too much alerted

			Vec3 userPos = pUser->GetPos();

			// adjust pos if pUserHelper is used
			userPos = pRule->pUserHelper ? pUser->GetHelperPos( pRule->pUserHelper ) : userPos;

			assert( bbMin.x-limitTo <= userPos.x && userPos.x <= bbMax.x+limitTo );
			if ( userPos.y < bbMin.y || userPos.y > bbMax.y ||
				userPos.z < bbMin.z || userPos.z > bbMax.z )
					continue;

			// check the orientation limit to target
			if ( pRule->fOrientationToTargetLimit < 360.0f )
			{
				float cosLimit = 1.0f;
				if ( pRule->fOrientationToTargetLimit != 0 )
					cosLimit = cos( pRule->fOrientationToTargetLimit / 360.0f * 3.1415926536f ); // limit is expressed as FOV (360 means unlimited)

				Vec3 objectDir = pObject->GetOrientation( pRule->pObjectHelper );
				Vec3 targetDir = (pExtraPoint ? *pExtraPoint : pAttTarget->GetPos()) - pos;
				targetDir.NormalizeSafe();
				float dot = objectDir.Dot( targetDir );

				if ( pRule->fOrientationToTargetLimit < 0 )
				{
					dot = -dot;
					cosLimit = -cosLimit;
				}

				if ( dot < cosLimit )
					continue;
			}

			// calculate delay time
			float fDelayTime = CalculateDelayTime( pUser, userPos, pObject, pos, pRule );
			if ( fDelayTime >= 0.0f && fDelayTime <= minDelay )
			{
				minDelay = fDelayTime;
				pMinUser = pUser;
				pMinRule = pRule;
			}

			// add it to the query list
			if ( pQueryEvents && fDelayTime >= 0.0f )
			{
				CQueryEvent q;
				q.pUser = pUser;
				q.pObject = pObject;
				q.pRule = pRule;
				q.pChainedUserEvent = NULL;
				q.pChainedObjectEvent = NULL;
				pQueryEvents->insert( std::make_pair(fDelayTime, q) );
			}
		}
	}

	if ( !pMinRule )
		return 0;

	// is querying only?
	if ( pQueryEvents )
		return -1;

	int id = GetAISystem()->AllocGoalPipeId();
	UseSmartObject( pMinUser, pObject, pMinRule, id, bHighPriority );
	*ppUserEntity = pMinUser->GetEntity();
	return !pMinRule->sAction.empty() && pMinRule->eActionType != eAT_None ? id : -1;
}

struct SOUpdateStats
{
	int classes;
	int users;
	int pairs;

	int hiddenUsers;
	int hiddenObjects;

	int totalRules;
	int ignoredNotNeededRules;
	int ignoredAttTargetRules;
	int ignoredStatesNotMatchingRules;

	int appliedUserOnlyRules;
	int appliedUserObjectRules;

	int ignoredNotInRangeObjects;
	int ignoredAttTargetNotInRangeObjects;
	int ignoredStateDoesntMatchObjects;

	void reset()
	{
		classes = 0;
		users = 0;
		pairs = 0;

		hiddenUsers = 0;
		hiddenObjects = 0;

		totalRules = 0;
		ignoredNotNeededRules = 0;
		ignoredAttTargetRules = 0;
		ignoredStatesNotMatchingRules = 0;

		appliedUserOnlyRules = 0;
		appliedUserObjectRules = 0;
		
		ignoredNotInRangeObjects = 0;
		ignoredAttTargetNotInRangeObjects = 0;
		ignoredStateDoesntMatchObjects = 0;
	}
};

SOUpdateStats currentUpdateStats;

void CSmartObjects::Update()
{
FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI)

	currentUpdateStats.reset();

	static int totalNumPairsToUpdate = 0;
	static int maxNumPairsUpdatedPerFrame = 100;
	int pairsUpdatedThisFrame = 0;

	if ( CSmartObjectClass::g_itAllUserClasses == CSmartObjectClass::g_AllUserClasses.end() )
	{
		// this is the end of a full cycle and beginning of a new one
		maxNumPairsUpdatedPerFrame = (maxNumPairsUpdatedPerFrame + (totalNumPairsToUpdate / 6)) / 2 + 6;
		totalNumPairsToUpdate = 0;

		CSmartObjectClass::g_itAllUserClasses = CSmartObjectClass::g_AllUserClasses.begin();
		if ( CSmartObjectClass::g_itAllUserClasses != CSmartObjectClass::g_AllUserClasses.end() )
			(*CSmartObjectClass::g_itAllUserClasses)->FirstObject();
	}

	while ( pairsUpdatedThisFrame < maxNumPairsUpdatedPerFrame &&
		CSmartObjectClass::g_itAllUserClasses != CSmartObjectClass::g_AllUserClasses.end() )
	{
		CSmartObjectClass* pClass = *CSmartObjectClass::g_itAllUserClasses;
		++currentUpdateStats.classes;

		while ( pairsUpdatedThisFrame < maxNumPairsUpdatedPerFrame )
		{
			CSmartObject* pSmartObject = pClass->NextVisibleObject();
			if ( pSmartObject == NULL )
			{
				// continue with the next class
				++CSmartObjectClass::g_itAllUserClasses;
				if ( CSmartObjectClass::g_itAllUserClasses != CSmartObjectClass::g_AllUserClasses.end() )
					(*CSmartObjectClass::g_itAllUserClasses)->FirstObject();

				break;
			}
			++currentUpdateStats.users;

			int numPairsProcessed = Process( pSmartObject, pClass );
			pairsUpdatedThisFrame += numPairsProcessed;
			currentUpdateStats.pairs += numPairsProcessed;

			// update LookAt smart object position, but only if this is the last object's user class
			CSmartObjectClasses& classes = pSmartObject->GetClasses();
			int i = classes.size();
			while ( i-- )
			{
				CSmartObjectClass* current = classes[i];
				if ( pClass == current )
				{
					CPuppet* pPuppet = pSmartObject->GetPuppet();
					if ( pPuppet )
					{
						if ( pSmartObject->m_fLookAtLimit )
							pPuppet->m_posLookAtSmartObject = pSmartObject->m_vLookAtPos;
						else
							pPuppet->m_posLookAtSmartObject.zero();
					}
					pSmartObject->m_fLookAtLimit = 0.0f;
					break; // exit the loop! this was the last user class.
				}
				if ( current->IsSmartObjectUser() )
					break; // exit the loop! some other class is the last user class.
			}
		}
	}

	totalNumPairsToUpdate += pairsUpdatedThisFrame;
}

typedef std::pair< CSmartObject*, CCondition* > PairObjectCondition;
typedef std::pair< PairObjectCondition, float > PairDelayTime;
typedef std::vector< PairDelayTime > VecDelayTimes;

struct Pred_IgnoreSecond
{
	bool operator() ( const PairDelayTime& A, const PairDelayTime& B ) const
	{
		return A.first < B.first;
	}
};

int CSmartObjects::Process( CSmartObject* pSmartObjectUser, CSmartObjectClass* pClass )
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

	AIAssert( pSmartObjectUser );

	if ( pSmartObjectUser->IsHidden() )
	{
		// ignore hidden entities
		++currentUpdateStats.hiddenUsers;
		return 1;
	}

	CPuppet* pPuppet = pSmartObjectUser->GetPuppet();
	// don't do this here! it should be done in Update() after processing the last class
	//if ( pPuppet )
	//	pPuppet->m_posLookAtSmartObject.zero();

	if ( pPuppet && (pPuppet->IsEnabled() == false || pPuppet->IsUpdatedOnce() == false) )
	{
		// ignore not enabled puppet-users
		++currentUpdateStats.hiddenUsers;
		return 1;
	}

	int result = 1;

	int species = -1;
	int groupId = -1;
	CAIActor* pUserActor = CastToCAIActorSafe(pSmartObjectUser->GetAI());
	if ( pUserActor )
	{
		groupId = pUserActor->GetGroupId();
		species = pUserActor->GetParameters().m_nSpecies;
	}

	// this entity is a smart object user - update his potential smart object targets
	CTimeValue lastUpdateTime(0.0f);
	CSmartObject::MapTimesByClass::iterator itFind = pSmartObjectUser->m_mapLastUpdateTimes.find( pClass );
	if ( itFind != pSmartObjectUser->m_mapLastUpdateTimes.end() )
		lastUpdateTime = itFind->second;

	float fTimeElapsed = (GetAISystem()->GetFrameStartTime() - lastUpdateTime).GetSeconds();
	if ( lastUpdateTime.GetMilliSecondsAsInt64() == 0 )
		fTimeElapsed = 0;
	if ( fTimeElapsed > 0.2f )
		fTimeElapsed = 0.2f;
	pSmartObjectUser->m_mapLastUpdateTimes[ pClass ] = GetAISystem()->GetFrameStartTime();

	IAIObject* pAttTarget = pPuppet ? pPuppet->GetAttentionTarget() : NULL;
	int alertness = pPuppet ? CLAMP( pPuppet->GetProxy()->GetAlertnessState(), 0, 2 ) : 0;

	Vec3 soPos = pSmartObjectUser->GetPos();
	
	// define a map of event updates
	static VecDelayTimes vecDelayTimes;
	vecDelayTimes.clear();

	// check all conditions matching with his class and state
	//MapConditions::iterator itConditions = m_Conditions.find( pClass );
	// optimized: use only the active rules
	CSmartObjectClass::VectorRules& activeRules = pClass->m_vActiveUpdateRules[ alertness ];
	CSmartObjectClass::VectorRules::iterator itConditions, itConditionsEnd = activeRules.end();
	for ( itConditions = activeRules.begin(); itConditions != itConditionsEnd; ++itConditions )
	{
		CCondition* pCondition = *itConditions;
		++currentUpdateStats.totalRules;

		// optimized: active rules have these already culled out
		/*
		if ( !pCondition->bEnabled || pCondition->iMaxAlertness < alertness || pCondition->iRuleType != 0 )
		{
			//++currentUpdateStats.ignoredDisabledIdleNavRules;
			continue;
		}
		// ignore event rules
		if ( !pCondition->sEvent.empty() )
		{
			//++currentUpdateStats.ignoredEventRules;
			continue;
		}
		*/

		// optimized: ignore the rules if there are no instances of the object class
		if ( pCondition->pObjectClass && pCondition->pObjectClass->IsNeeded() == false )
		{
			++currentUpdateStats.ignoredNotNeededRules;
			continue;
		}

		// ignore this rule if the user has no attention target and the rule says it's needed
		if ( !pAttTarget && pCondition->fOrientationToTargetLimit < 360.0f )
		{
			++currentUpdateStats.ignoredAttTargetRules;
			continue;
		}

		// go to next if this one doesn't match user's states
		if ( !pCondition->userStatePattern.Matches( pSmartObjectUser->GetStates() ) )
		{
			++currentUpdateStats.ignoredStatesNotMatchingRules;
			continue;
		}

		// check does it operate only on itself
		if ( !pCondition->pObjectClass )
		{
			// count objects because this method returns the number of relations processed
			++result;
			++currentUpdateStats.appliedUserOnlyRules;

			// calculated delta times should be stored only for now.
			// later existing events will be updated and for new objects new events will be added
			float fDelayTime = CalculateDelayTime( pSmartObjectUser, soPos, pSmartObjectUser, soPos, pCondition );
			if ( fDelayTime > 0.0f )
			{
				PairObjectCondition poc( pSmartObjectUser, pCondition );
				vecDelayTimes.push_back(std::make_pair( poc, fTimeElapsed/fDelayTime ));
			}
			else if ( fDelayTime == 0.0f )
			{
				PairObjectCondition poc( pSmartObjectUser, pCondition );
				vecDelayTimes.push_back(std::make_pair( poc, 1.0f ));
			}

			// this condition is done
			continue;
		}

		// adjust pos if pUserHelper is used
		Vec3 pos = pCondition->pUserHelper ? pSmartObjectUser->GetHelperPos( pCondition->pUserHelper ) : soPos;

		Vec3 bbMin = pos;
		Vec3 bbMax = pos;

		float limitTo = pCondition->fDistanceTo;
		// adjust limit if pUserHelper is used
		if ( pCondition->pUserHelper)
			limitTo += pCondition->pUserHelper->qt.t.GetLength();
		// adjust limit if pObjectHelper is used
		if ( pCondition->pObjectHelper )
			limitTo += pCondition->pObjectHelper->qt.t.GetLength();
		Vec3 d( limitTo, limitTo, limitTo );
		bbMin -= d;
		bbMax += d;

		// calculate the limit in advance
		float orientationToTargetLimit = -2.0f; // unlimited
		if ( pCondition->fOrientationToTargetLimit < 360.0f )
		{
			if ( pCondition->fOrientationToTargetLimit == 0 )
				orientationToTargetLimit = 1.0f;
			else
				orientationToTargetLimit = cos( pCondition->fOrientationToTargetLimit / 360.0f * 3.1415926536f ); // limit is expressed as FOV (360 means unlimited)

			if ( pCondition->fOrientationToTargetLimit < 0 )
				orientationToTargetLimit = -orientationToTargetLimit;
		}

		// check all objects (but not the user) matching with condition's class and state
		CSmartObjectClass::MapSmartObjectsByPos& mapObjectsByPos = pCondition->pObjectClass->m_MapObjectsByPos;
		if ( mapObjectsByPos.empty() )
			continue;

		CSmartObjectClass::MapSmartObjectsByPos::iterator itByPos = mapObjectsByPos.lower_bound( bbMin.x );
		for ( ; itByPos != mapObjectsByPos.end() && itByPos->first <= bbMax.x; ++itByPos )
		{
			CSmartObject* pSmartObject = itByPos->second;
			++currentUpdateStats.appliedUserObjectRules;

			// the user can not be the target object!!!
			if ( pSmartObjectUser == pSmartObject )
				continue;

			// ignore hidden entities
			if ( pSmartObject->IsHidden() )
			{
				++currentUpdateStats.hiddenObjects;
				continue;
			}

			// Check range first since it could be faster
			Vec3 objectPos = pCondition->pObjectHelper ? pSmartObject->GetHelperPos(pCondition->pObjectHelper) : pSmartObject->GetPos();
			if ( objectPos.IsZero() )
				continue;
			assert( objectPos.x == 0 || bbMin.x-limitTo <= objectPos.x && objectPos.x <= bbMax.x+limitTo );
			if ( objectPos.y < bbMin.y || objectPos.y > bbMax.y ||
				objectPos.z < bbMin.z || objectPos.z > bbMax.z )
			{
				++currentUpdateStats.ignoredNotInRangeObjects;
				continue;
			}

			// Also check the orientation limit to user's attention target
			if ( pCondition->fOrientationToTargetLimit < 360.0f )
			{
				Vec3 objectDir = pSmartObject->GetOrientation( pCondition->pObjectHelper );
				Vec3 targetDir = pAttTarget->GetPos() - objectPos;
				targetDir.NormalizeSafe();
				float dot = objectDir.Dot( targetDir );

				if ( pCondition->fOrientationToTargetLimit < 0 )
					dot = -dot;

				if ( dot < orientationToTargetLimit )
				{
					++currentUpdateStats.ignoredAttTargetNotInRangeObjects;
					continue;
				}
			}

			// add virtual states
			IAIObject* pAIObjectObject = pSmartObject->GetAI();
			CAIActor* pAIObjectActor = CastToCAIActorSafe(pSmartObject->GetAI());
			bool attTarget = false;
			bool sameGroupId = false;
			bool sameSpecies = false;

			int spObject = -1;

			if ( pAIObjectActor )
			{
				// check is the object attention target of the user
				attTarget = pPuppet && pPuppet->GetAttentionTarget() == pAIObjectActor ;
				if ( attTarget )
					pSmartObject->m_States.insert( m_StateAttTarget );
				// Only actors has species.
				spObject = pAIObjectActor->GetParameters().m_nSpecies;
			}

			if ( pAIObjectObject )
			{
				// check are the user and the object in the same group and species
				if ( groupId >= 0 || species >= 0 )
				{
					// check same species
					sameSpecies = species == spObject;
					if ( sameSpecies )
					{
						pSmartObject->m_States.insert( m_StateSameSpecies );

						// if they are same species check are they in same group
						sameGroupId = groupId == pAIObjectObject->GetGroupId();
						if ( sameGroupId )
							pSmartObject->m_States.insert( m_StateSameGroup );
					}
					else if ( species < 0 || spObject < 0 )
					{
						// if any of them has no species check are they in same group
						sameGroupId = groupId == pAIObjectObject->GetGroupId();
						if ( sameGroupId )
							pSmartObject->m_States.insert( m_StateSameGroup );
					}
				}
			}

			// check object's state pattern and then remove virtual states
			bool bMatches = pCondition->objectStatePattern.Matches( pSmartObject->GetStates() );
			if ( attTarget )
				pSmartObject->m_States.erase( m_StateAttTarget );
			if ( sameSpecies )
				pSmartObject->m_States.erase( m_StateSameSpecies );
			if ( sameGroupId )
				pSmartObject->m_States.erase( m_StateSameGroup );

			// ignore this object if it doesn't match precondition state
			if ( !bMatches )
			{
				++currentUpdateStats.ignoredStateDoesntMatchObjects;
				continue;
			}

			// count objects because this method returns the number of relations processed
			++result;

			if ( pCondition->pObjectHelper )
				objectPos = pSmartObject->GetHelperPos( pCondition->pObjectHelper );

			// calculated delta times should be stored only for now.
			// later existing events will be updated and for new objects new events will be added
			float fDelayTime = CalculateDelayTime( pSmartObjectUser, pos, pSmartObject, objectPos, pCondition );
			if ( fDelayTime > 0.0f )
			{
				PairObjectCondition poc( pSmartObject, pCondition );
				vecDelayTimes.push_back(std::make_pair( poc, fTimeElapsed/fDelayTime ));
			}
			else if ( fDelayTime == 0.0f )
			{
				PairObjectCondition poc( pSmartObject, pCondition );
				vecDelayTimes.push_back(std::make_pair( poc, 1.0f ));
			}
		}
	}

	std::sort( vecDelayTimes.begin(), vecDelayTimes.end() );

	// now update his existing smart object events
	CSmartObject::VectorEvents & events = pSmartObjectUser->m_Events[ pClass ];
	int i = events.size(), use = -1, lookat = -1;
	while ( i-- )
	{
		float thisLookat = events[i].m_pCondition->fLookAtPerc;
		if ( thisLookat <= 0.0f || thisLookat >= 1.0f )
			thisLookat = FLT_MAX;

		PairObjectCondition poc( events[i].m_pObject, events[i].m_pCondition );
		VecDelayTimes::iterator itTimes = std::lower_bound( vecDelayTimes.begin(), vecDelayTimes.end(), std::make_pair(poc,0.0f), Pred_IgnoreSecond() );
		if ( itTimes != vecDelayTimes.end() && itTimes->first == poc )
		{
			events[i].m_Delay += itTimes->second;

			// remove from list. later all remained entries will be added as new events
			itTimes->second = -1.0f;
			
			// is this smart object ready to be used?
			if ( events[i].m_Delay >= 1 )
			{
				// and is it more important than current candidate?
				if ( use < 0 || events[i].m_Delay > events[use].m_Delay )
					use = i;
			}

			if ( pPuppet )
			{
				// check is this better lookat target
				if ( events[i].m_Delay > thisLookat )
				{
					thisLookat = (events[i].m_Delay - thisLookat) / (1.0f - thisLookat);
					if ( thisLookat > pSmartObjectUser->m_fLookAtLimit )
					{
						pSmartObjectUser->m_fLookAtLimit = thisLookat;
						lookat = i;
					}
				}
			}
		}
		else
		{
			// events for objects not satisfying the condition will be forgotten and then removed from this vector
			if ( events[i].m_pCondition->fMemory > 0.001f )
				events[i].m_Delay -= fTimeElapsed/events[i].m_pCondition->fMemory;
			else
				events[i].m_Delay = -1.0f;
		}

		// delete unimportant events
		if ( events[i].m_Delay < 0 )
		{
			events[i] = events.back();
			events.pop_back();

			// update 'use' if it was pointing to the last element
			if ( use == events.size() )
				use = i;

			// update 'lookat' if it was pointing to the last element
			if ( lookat == events.size() )
				lookat = i;
		}
	}

	// and finally all new smart object events will be added
	VecDelayTimes::iterator itTimes = vecDelayTimes.begin();
	while ( itTimes != vecDelayTimes.end() )
	{
		if ( itTimes->second >= 0 )
		{
			CSmartObjectEvent event =
			{
				itTimes->first.first,
				itTimes->first.second,
				0
			};
			events.push_back( event );
		}
		++itTimes;
	}

	if ( lookat >= 0 )
	{
		pSmartObjectUser->m_vLookAtPos = events[lookat].m_pObject->GetPos();
	}

	if ( use >= 0 )
	{
		UseSmartObject( pSmartObjectUser, events[use].m_pObject, events[use].m_pCondition );
		events[use] = events.back();
		events.pop_back();
	}

	// return number of relations processed
	return result;
}

void CSmartObjects::UseSmartObject( CSmartObject* pSmartObjectUser, CSmartObject* pSmartObject, CCondition* pCondition, int eventId /*=0*/, bool bForceHighPriority /*=false*/ )
{
	AIAssert( pSmartObject );

	CPuppet* pPuppet = pSmartObjectUser->GetPuppet();
	if ( pPuppet && pSmartObjectUser != pSmartObject )
	{
		// keep track of last used smart object
		pPuppet->m_idLastUsedSmartObject = pSmartObject->GetEntity()->GetId();
	}

	// update random factors
	pSmartObjectUser->m_fRandom = float(ai_rand())/(2.0f*RAND_MAX);
	pSmartObject->m_fRandom = float(ai_rand())/(2.0f*RAND_MAX);

	// update states
	if ( !pCondition->userPreActionStates.empty() ) // check is next state non-empty
		ModifySmartObjectStates( pSmartObjectUser, pCondition->userPreActionStates );
	if ( pSmartObjectUser != pSmartObject )
		if ( !pCondition->objectPreActionStates.empty() ) // check is next state non-empty
			ModifySmartObjectStates( pSmartObject, pCondition->objectPreActionStates );

	if ( GetAISystem()->m_cvDrawSmartObjects->GetIVal() )
	{
		if ( pCondition->eActionType != eAT_None && !pCondition->sAction.empty() )
		{
			switch ( pCondition->eActionType )
			{
			case eAT_Action:
				AILogComment("User %s should execute action %s on smart object %s!", pSmartObjectUser->GetName(), pCondition->sAction.c_str(), pSmartObject->GetName() );
				break;
			case eAT_PriorityAction:
				AILogComment("User %s should execute high priority action %s on smart object %s!", pSmartObjectUser->GetName(), pCondition->sAction.c_str(), pSmartObject->GetName() );
				break;
			case eAT_AISignal:
				AILogComment("User %s receives action signal %s to use smart object %s!", pSmartObjectUser->GetName(), pCondition->sAction.c_str(), pSmartObject->GetName() );
				break;
			case eAT_AnimationSignal:
				AILogComment("User %s is going to play one shot animation %s on smart object %s!", pSmartObjectUser->GetName(), pCondition->sAction.c_str(), pSmartObject->GetName() );
				break;
			case eAT_AnimationAction:
				AILogComment("User %s is going to play looping animation %s on smart object %s!", pSmartObjectUser->GetName(), pCondition->sAction.c_str(), pSmartObject->GetName() );
				break;
			}
		}
		else
			AILogComment("User %s should execute NULL action on smart object %s!", pSmartObjectUser->GetName(), pSmartObject->GetName() );
	}

	if ( GetAISystem()->m_cvDrawSmartObjects->GetIVal() )
	{
		// show in debug draw
		CDebugUse debugUse = { GetAISystem()->GetFrameStartTime(), pSmartObjectUser, pSmartObject };
		m_vDebugUse.push_back( debugUse );
	}

	if ( pCondition->eActionType != eAT_None && !pCondition->sAction.empty() )
		pSmartObjectUser->Use( pSmartObject, pCondition, eventId, bForceHighPriority );
}

bool CSmartObjects::PrepareUseNavigationSmartObject( SAIActorTargetRequest& req, SNavSOStates& states, CSmartObject* pSmartObjectUser, CSmartObject* pSmartObject,
																										 const CCondition* pCondition, SmartObjectHelper* pFromHelper, SmartObjectHelper* pToHelper )
{
	if(pCondition->sAction.empty() ||
		(pCondition->eActionType != eAT_AnimationSignal && pCondition->eActionType != eAT_AnimationAction))
		return false;

	// update random factors
	pSmartObjectUser->m_fRandom = float(ai_rand())/(2.0f*RAND_MAX);
	pSmartObject->m_fRandom = float(ai_rand())/(2.0f*RAND_MAX);

	// update states
	if ( !pCondition->userPreActionStates.empty() ) // check is next state non-empty
		ModifySmartObjectStates( pSmartObjectUser, pCondition->userPreActionStates );
	if ( !pCondition->objectPreActionStates.empty() ) // check is next state non-empty
		ModifySmartObjectStates( pSmartObject, pCondition->objectPreActionStates );

	const CClassTemplateData::CTemplateHelper* exitHelper = 0;
	if (pCondition->pObjectClass->m_pTemplateData && pToHelper->templateHelperIndex >= 0 &&
			pToHelper->templateHelperIndex < pCondition->pObjectClass->m_pTemplateData->helpers.size())
	{
		exitHelper = &pCondition->pObjectClass->m_pTemplateData->helpers[pToHelper->templateHelperIndex];
	}

	req.animation = pCondition->sAction.c_str();
	req.signalAnimation = pCondition->eActionType == eAT_AnimationSignal;
	req.projectEndPoint = exitHelper ? exitHelper->project : true;
	req.startRadius = pCondition->fStartRadiusTolerance;
	req.directionRadius = DEG2RAD(pCondition->fDirectionTolerance);
	req.locationRadius = pCondition->fTargetRadiusTolerance;

	// store post-action state changes to be used after the animation
	CPuppet* pPuppet = pSmartObjectUser->GetPuppet();
	assert( states.sAnimationDoneUserStates.empty() );
	assert( states.sAnimationDoneObjectStates.empty() );
	assert( states.sAnimationFailUserStates.empty() );
	assert( states.sAnimationFailObjectStates.empty() );
	
	states.objectEntId = pSmartObject->GetEntity()->GetId();
	states.sAnimationDoneUserStates = pCondition->userPostActionStates.AsString();
	states.sAnimationDoneObjectStates = pCondition->objectPostActionStates.AsString();
	states.sAnimationFailUserStates = pCondition->userPreActionStates.AsUndoString();
	states.sAnimationFailObjectStates = pCondition->objectPreActionStates.AsUndoString();

	if ( GetAISystem()->m_cvDrawSmartObjects->GetIVal() )
	{
		AILogComment("User %s is going to play %s animation %s on navigation smart object %s!", pSmartObjectUser->GetName(),
			pCondition->eActionType == eAT_AnimationSignal ? "one shot" : "looping", pCondition->sAction.c_str(), pSmartObject->GetName() );
	}
	return true;
}

CSmartObject* CSmartObjects::IsSmartObject( IEntity* pEntity )
{
	AIAssert( pEntity );
	return pEntity ? pEntity->GetSmartObject() : 0;
}

bool CSmartObjects::CheckSmartObjectStates( IEntity* pEntity, const CSmartObject::CStatePattern& pattern ) const
{
	CSmartObject* pSmartObject = IsSmartObject( pEntity );
	if ( !pSmartObject )
		return false;
	
	return pattern.Matches( pSmartObject->GetStates() );
}

void CSmartObjects::ModifySmartObjectStates( IEntity* pEntity, const CSmartObject::DoubleVectorStates& states )
{
	CSmartObject* pSmartObject = IsSmartObject( pEntity );
	if ( !pSmartObject )
	{
		// Sometimes this function is called during initialization which is right before OnSpawn
		// so this entity is not registered as a smart object yet.
		SEntitySpawnParams params;
		OnSpawn( pEntity, params );
		pSmartObject = IsSmartObject( pEntity );

		// mark this entity as just registered so we don't register it twice
		m_pPreOnSpawnEntity = pEntity;
	}
	if ( pSmartObject )
		ModifySmartObjectStates( pSmartObject, states );
}

void CSmartObjects::ModifySmartObjectStates( CSmartObject* pSmartObject, const CSmartObject::DoubleVectorStates& states ) const
{
	CSmartObject::VectorStates::const_iterator itStates, itStatesEnd = states.negative.end();
	for ( itStates = states.negative.begin(); itStates != itStatesEnd; ++itStates )
		RemoveSmartObjectState( pSmartObject, *itStates );

	itStatesEnd = states.positive.end();
	for ( itStates = states.positive.begin(); itStates != itStatesEnd; ++itStates )
		AddSmartObjectState( pSmartObject, *itStates );
}

void CSmartObjects::SetSmartObjectState( IEntity* pEntity, CSmartObject::CState state ) const
{
	CSmartObject* pSmartObject = IsSmartObject( pEntity );
	if ( pSmartObject )
		SetSmartObjectState( pSmartObject, state );
}

void CSmartObjects::SetSmartObjectState( CSmartObject* pSmartObject, CSmartObject::CState state ) const
{
	pSmartObject->m_States.clear();
	pSmartObject->m_States.insert( state );
}

void CSmartObjects::AddSmartObjectState( IEntity* pEntity, CSmartObject::CState state ) const
{
	CSmartObject* pSmartObject = IsSmartObject( pEntity );
	if ( pSmartObject )
		AddSmartObjectState( pSmartObject, state );
}

void CSmartObjects::AddSmartObjectState( CSmartObject* pSmartObject, CSmartObject::CState state ) const
{
	if ( !pSmartObject->m_States.insert( state ).second )
		return;

	if ( state == m_StateBusy )
	{
		// check is the entity linked with an entity link named "Busy" and then set the "Busy" state to the linked entity as well
		IEntityLink* pEntityLink = pSmartObject->m_pEntity->GetEntityLinks();
		while ( pEntityLink )
		{
			if ( !strcmp("Busy", pEntityLink->name) )
			{
				if ( IEntity* pEntity = gEnv->pEntitySystem->GetEntity( pEntityLink->entityId ) )
					AddSmartObjectState( pEntity, state );
				break;
			}
			pEntityLink = pEntityLink->next;
		}
	}
}

void CSmartObjects::RemoveSmartObjectState( IEntity* pEntity, CSmartObject::CState state ) const
{
	CSmartObject* pSmartObject = IsSmartObject( pEntity );
	if ( pSmartObject )
		RemoveSmartObjectState( pSmartObject, state );
}

void CSmartObjects::RemoveSmartObjectState( CSmartObject* pSmartObject, CSmartObject::CState state ) const
{
	if ( !pSmartObject->m_States.erase( state ) )
		return;
	if ( state == m_StateBusy )
	{
		// check is the entity linked with an entity link named "Busy" and then set the "Busy" state to the linked entity as well
		IEntityLink* pEntityLink = pSmartObject->m_pEntity->GetEntityLinks();
		while ( pEntityLink )
		{
			if ( !strcmp("Busy", pEntityLink->name) )
			{
				if ( IEntity* pEntity = gEnv->pEntitySystem->GetEntity( pEntityLink->entityId ) )
					RemoveSmartObjectState( pEntity, state );
				break;
			}
			pEntityLink = pEntityLink->next;
		}
	}
}

void CSmartObjects::DebugDraw( IRenderer * pRenderer )
{
	SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
	SAuxGeomRenderFlags	newFlags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

	float drawDist2 = GetAISystem()->m_cvAgentStatsDist->GetFVal();
	if ( drawDist2 <= 0 || !GetAISystem()->GetPlayer() )
		return;
	drawDist2 *= drawDist2;
	Vec3 playerPos = GetDebugCameraPos();

	CSmartObjectClass::MapClassesByName::iterator itClass, itClassesEnd = CSmartObjectClass::g_AllByName.end();
	for ( itClass = CSmartObjectClass::g_AllByName.begin(); itClass != itClassesEnd; ++itClass )
	{
		CSmartObjectClass::MapSmartObjectsByPos& mapByPos = itClass->second->m_MapObjectsByPos;
		CSmartObjectClass::MapSmartObjectsByPos::iterator itByPos;
		for ( itByPos = mapByPos.begin(); itByPos != mapByPos.end(); ++itByPos )
		{
			CSmartObject* pCurrentObject = itByPos->second;
			Vec3 from = pCurrentObject->GetPos();

			// don't draw too far objects
			if ( drawDist2 < (from-playerPos).GetLengthSquared() )
				continue;

			DrawTemplate( pCurrentObject, false );

			CSmartObject::MapEventsByClass::iterator itEvents, itEventsEnd = pCurrentObject->m_Events.end();
			for ( itEvents = pCurrentObject->m_Events.begin(); itEvents != itEventsEnd; ++itEvents )
			{
				CSmartObject::VectorEvents & events = itEvents->second;
				float alpha = 2.0f;
				int i = events.size();
				while ( i-- )
				{
					CSmartObjectEvent& event = events[i];
					float delay = event.m_Delay;
					if (delay)
					{
						Vec3 to = event.m_pObject->GetPos();

						SmartObjectHelper* pFromHelper = event.m_pCondition->pUserHelper;
						SmartObjectHelper* pToHelper = event.m_pCondition->pObjectHelper;
						Vec3 fromPos = pFromHelper ? pCurrentObject->GetEntity()->GetWorldTM().TransformPoint( pFromHelper->qt.t ) : from;
						Vec3 toPos = pToHelper ? event.m_pObject->GetEntity()->GetWorldTM().TransformPoint( pToHelper->qt.t ) : to;

						Vec3 dir = toPos - fromPos;
						dir *= 0.5f*delay;
						Vec3 pt = fromPos + dir;
						ColorF yellow(0xFF00FFFF), green(0xFF00FF00);
						yellow.a = min( 1.0f, alpha );
						green.a = max( 0.01f, min(1.0f, alpha) );
						alpha *= 0.667f;
						//pRenderer->DrawLineColor( fromPos, yellow, pt, green );
						pt = toPos - dir;
						//pRenderer->DrawLineColor( toPos, yellow, pt, green );
					}
				}
			}

			string sClasses;

			ColorF pink(0xFF8080FFu), white(0xFFFFFFFFu), green(0x6080FF80u);

			Matrix34 matrix = pCurrentObject->GetEntity()->GetWorldTM();
			CSmartObjectClasses& classes = pCurrentObject->GetClasses();
			CSmartObjectClasses::iterator itClasses, itClassesEnd =  classes.end();
			for ( itClasses = classes.begin(); itClasses != itClassesEnd; ++itClasses )
			{
				CSmartObjectClass* pClass = *itClasses;

				if ( !sClasses.empty() )
					sClasses += ", ";
				sClasses += pClass->GetName();

				// draw arrows to show smart object helpers
				CSmartObjectClass::VectorHelpers& vHelpers = pClass->m_vHelpers;
				CSmartObjectClass::VectorHelpers::iterator itHelpers, itHelpersEnd = vHelpers.end();
				for ( itHelpers = vHelpers.begin(); itHelpers != itHelpersEnd; ++itHelpers )
				{
					SmartObjectHelper* pHelper = *itHelpers;

					Vec3 fromPos = pCurrentObject->GetHelperPos( pHelper );
					Vec3 toPos = fromPos + pCurrentObject->GetOrientation( pHelper ) * 0.2f;

					pRenderer->GetIRenderAuxGeom()->DrawSphere( fromPos, 0.05f, green );
					//pRenderer->DrawLineColor( fromPos, green, toPos, green );
					pRenderer->GetIRenderAuxGeom()->DrawCone( toPos, toPos-fromPos, 0.05f, 0.1f, green );
				}


				// draw lines to show smart object helper links
				CSmartObjectClass::THelperLinks::iterator itLinks, itLinksEnd = pClass->m_vHelperLinks.end();
				for ( itLinks = pClass->m_vHelperLinks.begin(); itLinks != itLinksEnd; ++itLinks )
				{
					CSmartObjectClass::HelperLink& helperLink = *itLinks;
					SmartObjectHelper* pFromHelper = helperLink.from;
					SmartObjectHelper* pToHelper = helperLink.to;

					Vec3 fromPos = pCurrentObject->GetHelperPos( pFromHelper ); // matrix.TransformPoint( pFromHelper->qt.t );
					Vec3 toPos = pCurrentObject->GetHelperPos( pToHelper ); // matrix.TransformPoint( pToHelper->qt.t );

					//pRenderer->DrawLineColor( fromPos, pink, toPos, pink );
					pRenderer->GetIRenderAuxGeom()->DrawCone( toPos, toPos-fromPos, 0.05f, 0.2f, pink );
				}

				CGraph* pGraph = GetAISystem()->GetGraph();
				if ( pGraph )
				{
					// first check if the graph is still empty
					CAllNodesContainer::Iterator it( pGraph->GetAllNodes(), IAISystem::NAV_TRIANGULAR );
					if ( it.GetNode() )
					{
						// draw lines to show how smart object helpers are linked to outside world
						CSmartObjectClass::SetHelpers::iterator itSetHelpers, itSetHelpersEnd = pClass->m_setNavHelpers.end();
						for ( itSetHelpers = pClass->m_setNavHelpers.begin(); itSetHelpers != itSetHelpersEnd; ++itSetHelpers )
						{
							const SmartObjectHelper* pHelper = *itSetHelpers;
							Vec3 helperPos = pCurrentObject->GetHelperPos( pHelper );
							//unsigned nodeIndices[2];
							//if ( pCurrentObject->GetNavNodes( nodeIndices, pHelper ) )
							//	pRenderer->DrawLineColor( helperPos, pink, GetAISystem()->GetGraph()->GetNodeManager().GetNode(nodeIndices[0])->GetPos(), white );
						}
					}
				}
			}

			float fYellow[4] = {1,1,0,1};
			float fGreen[4] = {0,1,0,1};
			const float fontSize = 1.15f;

			string s;
			CPuppet* pPuppet = pCurrentObject->GetPuppet();
			if ( pPuppet )
				s += "\n\n\n\n";

			// to prevent buffer overrun
			if ( sClasses.length() > 120 )
			{
				sClasses = sClasses.substr( 0, 120 );
				sClasses += "...";
			}

			pRenderer->DrawLabelEx( from, fontSize, fGreen, true, true, s + sClasses );

			s += "\n";
			const CSmartObject::SetStates& states = pCurrentObject->GetStates();
			CSmartObject::SetStates::const_iterator itStates, itStatesEnd = states.end();
			for ( itStates = states.begin(); itStates != itStatesEnd; ++itStates )
			{
				s += itStates->c_str();
				s += ' ';
			}

			// to prevent buffer overrun
			if ( s.length() > 124 )
			{
				s = s.substr( 0, 124 );
				s += "...";
			}

			pRenderer->DrawLabelEx( from, fontSize, fYellow, true, true, s );
		}
	}
	
	// debug use actions
	CTimeValue fTime = GetAISystem()->GetFrameStartTime();
	int i = m_vDebugUse.size();
	while ( i-- )
	{
		float age = (fTime - m_vDebugUse[i].m_Time).GetSeconds();
		if ( age < 0.5f )
		{
			Vec3 from = m_vDebugUse[i].m_pUser->GetPos();
			Vec3 to = m_vDebugUse[i].m_pObject->GetPos();
			age = abs( 1.0f - 4.0f*age );
			age = abs( 1.0f - 2.0f*age );
			ColorF color1(age, age, 1.0f, abs(1.0f-2.0f*age)), color2(1.0f-age, 1.0f-age, 1.0f, abs(1.0f-2.0f*age));
			//pRenderer->DrawLineColor( from, color1, to, color2 );
		}
		else
		{
			m_vDebugUse[i] = m_vDebugUse.back();
			m_vDebugUse.pop_back();
		}
	}
}

void CSmartObjects::RescanSOClasses( IEntity* pEntity )
{
	CSmartObject* pSmartObject = IsSmartObject( pEntity );
	CSmartObjectClasses vClasses;
	if ( !ParseClassesFromProperties(pEntity, vClasses) )
	{
		if ( pSmartObject )
			DoRemove( pEntity );
	}
	if ( pSmartObject )
	{
		if ( vClasses.size() == pSmartObject->GetClasses().size() )
		{
			CSmartObjectClasses::iterator it1, it1End = vClasses.end();
			CSmartObjectClasses::iterator it2, it2Begin = pSmartObject->GetClasses().begin(), it2End = pSmartObject->GetClasses().end();
			for ( it1 = vClasses.begin(); it1 != it1End; ++it1 )
			{
				it2 = std::find( it2Begin, it2End, *it1 );
				if ( it2 == it2End )
					break;
			}
			if ( it1 == it1End )
				return;
		}
		DoRemove( pEntity );
		pSmartObject = NULL;
	}
	SEntitySpawnParams params;
	OnSpawn( pEntity, params );
}

// Implementation of IEntitySystemSink methods
///////////////////////////////////////////////
void CSmartObjects::OnSpawn( IEntity* pEntity, SEntitySpawnParams& params )
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

	if (gEnv->bMultiplayer)
		return;

	if ( pEntity == m_pPreOnSpawnEntity )
	{
		// the entity for which OnSpawn was called just right before the current OnSpawn call
		m_pPreOnSpawnEntity = NULL;
		return;
	}
	m_pPreOnSpawnEntity = NULL;

	CSmartObjectClasses vClasses;
	if ( !ParseClassesFromProperties(pEntity, vClasses) )
		return;
	
	CSmartObject* smartObject = IsSmartObject( pEntity );
	if ( !smartObject )
		smartObject = new CSmartObject( pEntity );

	Vec3 size = smartObject->MeasureUserSize();
	size.y += size.z;
	bool hasSize = !size.IsZero();

	for ( int i = 0; i < vClasses.size(); ++i )
	{
		CSmartObjectClass* pClass = vClasses[i];

		pClass->MarkAsNeeded();

		CSmartObjectClasses::iterator it = std::find( smartObject->GetClasses().begin(), smartObject->GetClasses().end(), pClass );
		if ( it == smartObject->GetClasses().end() )
		{
			pClass->RegisterSmartObject( smartObject );
			smartObject->m_fKey = pEntity->GetWorldPos().x;
			pClass->m_MapObjectsByPos.insert( std::make_pair( smartObject->m_fKey, smartObject ) );
		}

		// register each class in navigation
		RegisterInNavigation( smartObject, pClass );

		if ( hasSize )
		{
			CSmartObjectClass::UserSize userSize( size.x, size.y, size.z );
			pClass->m_StanceMaxSize += userSize;
		}
	}
}


void CSmartObjects::RemoveEntity( IEntity* pEntity )
{
	DoRemove(pEntity);
}

// Implementation of IEntitySystemSink methods
///////////////////////////////////////////////
bool CSmartObjects::OnRemove( IEntity* pEntity )
{
	return true;
}

void CSmartObjects::DoRemove( IEntity* pEntity )
{
	// remove all actions in which this entity is participating
	GetAISystem()->GetActionManager()->OnEntityRemove( pEntity );

	CSmartObject* pSmartObject = IsSmartObject( pEntity );
	if ( pSmartObject )
	{
		// let's make sure AI navigation is not using it
		// in case this is a navigation smart object entity!
		UnregisterFromNavigation( pSmartObject );

		CDebugUse debugUse = { 0.0f, pSmartObject, pSmartObject };
		VectorDebugUse::iterator it = m_vDebugUse.begin();
		while ( it != m_vDebugUse.end() )
		{
			if ( it->m_pUser == pSmartObject || it->m_pObject == pSmartObject )
			{
				*it = m_vDebugUse.back();
				m_vDebugUse.pop_back();
			}
			else
				++it;
		}

		// must remove all references to removed smart object within events lists kept by smart objects registered as users
		CSmartObjectClass::VectorClasses::iterator itAllClasses, itAllClassesEnd = CSmartObjectClass::g_AllUserClasses.end();
		for ( itAllClasses = CSmartObjectClass::g_AllUserClasses.begin(); itAllClasses != itAllClassesEnd; ++itAllClasses )
		{
			CSmartObjectClass* pUserClass = *itAllClasses;

			CSmartObjectClass::MapSmartObjectsByPos& objects = pUserClass->m_MapObjectsByPos;
			CSmartObjectClass::MapSmartObjectsByPos::iterator itObjects, itObjectsEnd = objects.end();
			for ( itObjects = objects.begin(); itObjects != itObjectsEnd; ++itObjects )
			{
				CSmartObject* pCurrentObject = itObjects->second;
				CSmartObject::VectorEvents & events = pCurrentObject->m_Events[ pUserClass ];
				int i = events.size();
				while ( i-- )
				{
					if ( events[i].m_pObject == pSmartObject )
					{
						events[i] = events.back();
						events.pop_back();
					}
				}
			}
		}

		float x = pSmartObject->m_fKey;

		// don't keep a reference to vector classes, coz it will be modified on
		// each call to DeleteSmartObject and at the end it will be even deleted
		CSmartObjectClasses vClasses = pSmartObject->GetClasses();

		CSmartObjectClasses::iterator itClasses, itClassesEnd = vClasses.end();
		for ( itClasses = vClasses.begin(); itClasses != itClassesEnd; ++itClasses )
		{
			CSmartObjectClass::MapSmartObjectsByPos& objects = (*itClasses)->m_MapObjectsByPos;

			CSmartObjectClass::MapSmartObjectsByPos::iterator itByPos, itByPosEnd = objects.end();
			for( itByPos = objects.find(x); itByPos != itByPosEnd && itByPos->first == x; ++itByPos )
			{
				if ( itByPos->second == pSmartObject )
				{
					objects.erase( itByPos );
					break;
				}
			}

			(*itClasses)->DeleteSmartObject( pEntity );
		}

		//delete pSmartObject;
	}
}

// Implementation of IEntitySystemSink methods
///////////////////////////////////////////////
void CSmartObjects::OnEvent( IEntity* pEntity, SEntityEvent& event )
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

	switch (event.event)
	{
	case ENTITY_EVENT_DONE:
		DoRemove( pEntity );
		break;
	case ENTITY_EVENT_HIDE:
		{
			CSmartObject* pSmartObject = IsSmartObject( pEntity );
			if ( pSmartObject )
				pSmartObject->Hide( true );
			break;
		}
	case ENTITY_EVENT_UNHIDE:
		{
			CSmartObject* pSmartObject = IsSmartObject( pEntity );
			if ( pSmartObject )
				pSmartObject->Hide( false );
			break;
		}
	case ENTITY_EVENT_INIT:
		{
			SEntitySpawnParams params;
			OnSpawn( pEntity, params );
			break;
		}
	case ENTITY_EVENT_XFORM:
		{
			CSmartObject* pSmartObject = IsSmartObject( pEntity );
			if ( pSmartObject )
			{
				pSmartObject->m_mapNavNodes.clear();
				pSmartObject->m_eValidationResult = eSOV_Unknown;

				float oldX = pSmartObject->m_fKey;
				float newX = pSmartObject->GetPos().x;
				if ( newX == oldX )
					return;

				pSmartObject->m_fKey = newX;

				CSmartObjectClasses& classes = pSmartObject->GetClasses();
				CSmartObjectClasses::iterator it, itEnd = classes.end();
				for ( it = classes.begin(); it != itEnd; ++it )
				{
					CSmartObjectClass::MapSmartObjectsByPos& mapByPos = (*it)->m_MapObjectsByPos;
					CSmartObjectClass::MapSmartObjectsByPos::iterator itByPos = mapByPos.find( oldX );

					//assert( itByPos != mapByPos.end() );
					// Warning!!! This is only workaround for the problem!
					if ( itByPos == mapByPos.end() )
						itByPos = mapByPos.begin();
					if ( itByPos == mapByPos.end() )
						return;
					
					while ( itByPos->second != pSmartObject )
					{
						++itByPos;
						//assert( itByPos != mapByPos.end() );
						if ( itByPos == mapByPos.end() )
						{
							// Warning!!! This is only workaround for the problem!
							itByPos = mapByPos.begin();
							//assert( itByPos != mapByPos.end() );

							while ( itByPos->second != pSmartObject )
							{
								++itByPos;
								//assert( itByPos != mapByPos.end() );
								if ( itByPos != mapByPos.end() )
									return;
							}
						}
						//assert( itByPos->first == oldX );
					}

					mapByPos.erase( itByPos );
					mapByPos.insert( std::make_pair(newX, pSmartObject) );
				}
			}
			break;
		}
	}
}

void CSmartObjects::RebuildNavigation()
{
	// find pointers to helpers to speed-up things
	MapConditions::iterator itConditions, itConditionsEnd = m_Conditions.end();
	for ( itConditions = m_Conditions.begin(); itConditions != itConditionsEnd; ++itConditions )
	{
		CCondition& condition = itConditions->second;

		// ignore event rules
		//	if ( !condition.sEvent.empty() )
		//		continue;

		if ( condition.pUserClass && !condition.sUserHelper.empty() )
			condition.pUserHelper = condition.pUserClass->GetHelper( condition.sUserHelper );
		else
			condition.pUserHelper = NULL;
		if ( condition.pObjectClass && !condition.sObjectHelper.empty() )
			condition.pObjectHelper = condition.pObjectClass->GetHelper( condition.sObjectHelper );
		else
			condition.pObjectHelper = NULL;
	}

	CAISystem* pAISystem = GetAISystem();
	CGraph* pGraph = pAISystem->GetGraph();
	CSmartObjectNavRegion* pNavRegion = pAISystem->GetSmartObjectsNavRegion();

	pNavRegion->Clear();

	// reset list of links in all classes
	CSmartObjectClass::MapClassesByName::iterator itClasses, itClassesEnd = CSmartObjectClass::g_AllByName.end();
	for ( itClasses = CSmartObjectClass::g_AllByName.begin(); itClasses != itClassesEnd; ++itClasses )
	{
		itClasses->second->ClearHelperLinks();
	}

	// we need a list of potential users to 
	std::set< CSmartObjectClass* > setNavUserClasses;

	// add each navigation condition to object's class
	// MapConditions::iterator itConditions, itConditionsEnd = m_Conditions.end();
	for ( itConditions = m_Conditions.begin(); itConditions != itConditionsEnd; ++itConditions )
	{
		// ignore event rules
		if ( !itConditions->second.sEvent.empty() )
			continue;

		CCondition* pCondition = &itConditions->second;
		if ( pCondition->bEnabled && pCondition->pObjectClass && pCondition->iRuleType == 1 )
			pCondition->pObjectClass->AddHelperLink( pCondition );
	}

	// for each smart object instance create navigation graph nodes and links
	for ( itClasses = CSmartObjectClass::g_AllByName.begin(); itClasses != itClassesEnd; ++itClasses )
	{
		CSmartObjectClass* pClass = itClasses->second;
		if ( !pClass->m_setNavHelpers.empty() )
		{
			pClass->FirstObject();
			CSmartObject* pSmartObject;
			while ( pSmartObject = pClass->NextObject() )
			{
				RegisterInNavigation( pSmartObject, pClass );
			}
		}
	}
}

void CSmartObjects::RegisterInNavigation( CSmartObject* pSmartObject, CSmartObjectClass* pClass ) const
{
	CGraph* pGraph = GetAISystem()->GetGraph();

	// first check is the graph loaded
	CAllNodesContainer::Iterator it( pGraph->GetAllNodes(), IAISystem::NAV_TRIANGULAR );
	if ( !it.GetNode() )
		return;

	typedef std::map< SmartObjectHelper*, unsigned > MapHelpersToNodes;
	MapHelpersToNodes helpersToNodes;

	// create the nodes
	CSmartObjectClass::SetHelpers::iterator itHelpers, itHelpersEnd = pClass->m_setNavHelpers.end();
	for ( itHelpers = pClass->m_setNavHelpers.begin(); itHelpers != itHelpersEnd; ++itHelpers )
	{
		SmartObjectHelper* pHelper = *itHelpers;
		IEntity* pEntity = pSmartObject->GetEntity();

		// calculate position
		Vec3 pos = pEntity->GetWorldTM().TransformPoint( pHelper->qt.t );

		// find nearest existing graph node
		unsigned nodeIndex = pGraph->GetEnclosing( pos, SMART_OBJECT_ENCLOSING_NAV_TYPES );

		// create a new node
		unsigned newNodeIndex = pGraph->CreateNewNode( IAISystem::NAV_SMARTOBJECT, pos );
		GraphNode* pNewNode = pGraph->GetNodeManager().GetNode(newNodeIndex);
		pNewNode->GetSmartObjectNavData()->pSmartObject = pSmartObject;
		pNewNode->GetSmartObjectNavData()->pClass = pClass;
		pNewNode->GetSmartObjectNavData()->pHelper = pHelper;

		// connect the new node with the enclosing node (both ways)
		pGraph->Connect( nodeIndex, newNodeIndex );

		// store the new node in our map
		helpersToNodes[ pHelper ] = newNodeIndex;
	}

	// link the nodes
	CSmartObjectClass::THelperLinks::iterator itLinks, itLinksEnd = pClass->m_vHelperLinks.end();
	for ( itLinks = pClass->m_vHelperLinks.begin(); itLinks != itLinksEnd; ++itLinks )
	{
		unsigned fromNodeIndex = helpersToNodes[ itLinks->from ];
		unsigned toNodeIndex = helpersToNodes[ itLinks->to ];
		if ( fromNodeIndex != toNodeIndex )
		{
			unsigned link;
			pGraph->Connect( fromNodeIndex, toNodeIndex, 100.0f, 0.0f, &link );
			pGraph->GetLinkManager().SetRadius( link, itLinks->passRadius );
			pSmartObject->m_vNavNodes.push_back( link );
		}
	}
}

void CSmartObjects::UnregisterFromNavigation( CSmartObject* pSmartObject ) const
{
	// first check is the graph loaded
	CGraph* pGraph = GetAISystem()->GetGraph();
	CAllNodesContainer::Iterator it( pGraph->GetAllNodes(), IAISystem::NAV_TRIANGULAR );
	if ( !it.GetNode() )
		return;

	// then disable all links
	CGraphLinkManager& manager = pGraph->GetLinkManager();
	CSmartObject::VectorNavNodes::iterator itLinks, itLinksEnd = pSmartObject->m_vNavNodes.end();
	for ( itLinks = pSmartObject->m_vNavNodes.begin(); itLinks != itLinksEnd; ++itLinks )
		manager.ModifyRadius( *itLinks, -1.0f );
}

void CSmartObjects::GetTemplateIStatObj( IEntity* pEntity, std::vector< IStatObj* >& statObjects )
{
	statObjects.clear();

	CSmartObject* pSmartObject = IsSmartObject( pEntity );
	if ( !pSmartObject )
		return;

	CSmartObjectClasses& classes = pSmartObject->GetClasses();
	CSmartObjectClasses::iterator it, itEnd = classes.end();
	for ( it = classes.begin(); it != itEnd; ++it )
	{
		CSmartObjectClass* pClass = *it;
		if ( !pClass->m_pTemplateData )
			continue;
		IStatObj* pStatObj = pClass->m_pTemplateData->GetIStateObj();
		if ( !pStatObj )
			continue;
		std::vector< IStatObj* >::iterator itFind = std::find( statObjects.begin(), statObjects.end(), pStatObj );
		if ( itFind == statObjects.end() )
			statObjects.push_back( pStatObj );
	}
}

CSmartObject::MapTemplates& CSmartObject::GetMapTemplates()
{
	if ( m_pMapTemplates )
		return *m_pMapTemplates;

	m_pMapTemplates = new MapTemplates;

	CSmartObjectClasses::iterator itClasses, itClassesEnd = m_vClasses.end();
	for ( itClasses = m_vClasses.begin(); itClasses != itClassesEnd; ++itClasses )
	{
		CSmartObjectClass* pClass = *itClasses;
		if ( !pClass->m_NavUsersMaxSize )
			continue; // this class is not used for navigation

		CClassTemplateData* pTemplate = pClass->m_pTemplateData;
		if ( !pTemplate || pTemplate->helpers.empty() )
			continue; // ignore classes for which there's no template defined or there are no helpers in the template

		std::pair< MapTemplates::iterator, bool > insertResult = m_pMapTemplates->insert( std::make_pair( pTemplate, CSmartObject::CTemplateData() ) );
		CSmartObject::CTemplateData& data = insertResult.first->second;
		data.pClass = pClass;
		if ( insertResult.second )
		{
			// new entry - only copy the user
			data.userRadius = pClass->m_NavUsersMaxSize.radius;
			data.userBottom = pClass->m_NavUsersMaxSize.bottom;
			data.userTop = pClass->m_NavUsersMaxSize.top;
		}
		else
		{
			// an entry was found - find the maximum user size
			data.userRadius = max( data.userRadius, pClass->m_NavUsersMaxSize.radius );
			data.userBottom = min( data.userBottom, pClass->m_NavUsersMaxSize.bottom );
			data.userTop = max( data.userTop, pClass->m_NavUsersMaxSize.top );
		}
	}
	return *m_pMapTemplates;
}

bool CSmartObjects::ValidateTemplate( IEntity* pEntity, bool bStaticOnly, IEntity* pUserEntity /*=NULL*/, int fromTemplateHelperIdx /*=-1*/, int toTemplateHelperIdx /*=-1*/ )
{
	CSmartObject* pSmartObject = IsSmartObject( pEntity );
	if ( pSmartObject )
		return ValidateTemplate( pSmartObject, bStaticOnly, pUserEntity, fromTemplateHelperIdx, toTemplateHelperIdx );
	return true;
}

bool CSmartObjects::ValidateTemplate( CSmartObject* pSmartObject, bool bStaticOnly, IEntity* pUserEntity /*=NULL*/, int fromTemplateHelperIdx /*=-1*/, int toTemplateHelperIdx /*=-1*/ )
{
	if ( m_bRecalculateUserSize )
		RecalculateUserSize();

	CSmartObject::MapTemplates& templates = pSmartObject->GetMapTemplates();
	if ( templates.empty() )
		return true;

	if ( !bStaticOnly && pSmartObject->m_eValidationResult == eSOV_Unknown )
		ValidateTemplate( pSmartObject, true, pUserEntity );

	const Matrix34& wtm = pSmartObject->m_pEntity->GetWorldTM();
	bool result = true;
	pSmartObject->m_eValidationResult = eSOV_Valid;

	CSmartObject::MapTemplates::iterator itTemplates, itTemplatesEnd = templates.end();
	for ( itTemplates = templates.begin(); itTemplates != itTemplatesEnd; ++itTemplates )
	{
		CClassTemplateData* pTemplate = itTemplates->first;

		CSmartObject::CTemplateData& data = itTemplates->second;
		if ( pTemplate->helpers.size() != data.helperInstances.size() )
			data.helperInstances.resize( pTemplate->helpers.size() );
		CSmartObject::TTemplateHelperInstances::iterator itInstances = data.helperInstances.begin();

		CClassTemplateData::TTemplateHelpers::iterator it, itEnd = pTemplate->helpers.end();
		int idx = 0;
		for ( it = pTemplate->helpers.begin(); it != itEnd; ++it, ++itInstances, ++idx )
		{
			CClassTemplateData::CTemplateHelper& helper = *it;

			if (!bStaticOnly)
			{
				// When testing dynamic helpers, check if a pair is specified and only check that pair.
				if (fromTemplateHelperIdx != -1 && toTemplateHelperIdx != -1)
				{
					if (idx != fromTemplateHelperIdx && idx != toTemplateHelperIdx)
						continue;
				}
			}

			if ( !bStaticOnly && itInstances->validationResult == eSOV_InvalidStatic )
			{
				pSmartObject->m_eValidationResult = eSOV_InvalidStatic;
				result = false;
				continue;
			}

			CAISystem::SSOTemplateArea area;
			area.pt = wtm * helper.qt.t;
			area.radius = helper.radius;
			area.projectOnGround = helper.project;

			CAISystem::SSOUser user = { pUserEntity, data.userRadius, data.userTop-data.userBottom, data.userBottom };
			if ( !CAISystem::ValidateSmartObjectArea( area, user, bStaticOnly ? AICE_ALL : AICE_ALL_INLUDING_LIVING, itInstances->position ) )
			{
				itInstances->validationResult = bStaticOnly ? eSOV_InvalidStatic : eSOV_InvalidDynamic;
				pSmartObject->m_eValidationResult = bStaticOnly ? eSOV_InvalidStatic : eSOV_InvalidDynamic;
				result = false;
			}
			else
				itInstances->validationResult = eSOV_Valid;
		}
	}

	return result;
}

void CSmartObjects::DrawTemplate( IEntity* pEntity )
{
	CSmartObject* pSmartObject = IsSmartObject( pEntity );
	if ( pSmartObject )
		DrawTemplate( pSmartObject, true );
}

struct TDebugLink
{
	SmartObjectHelper* pFromHelper;
	SmartObjectHelper* pToHelper;
	CSmartObject::CTemplateHelperInstance* pFromInstance;
	CSmartObject::CTemplateHelperInstance* pToInstance;
	float fFromRadius;
	float fToRadius;
	float fUserRadius;
	bool bTwoWay;
};

void CSmartObjects::DrawTemplate( CSmartObject* pSmartObject, bool bStaticOnly )
{
	bool valid = ValidateTemplate( pSmartObject, bStaticOnly );
	
	const Matrix34& wtm = pSmartObject->m_pEntity->GetWorldTM();
	ColorB colors[] = {
		0x40808080u, 0xff808080u, //eSOV_Unknown,
		0x408080ffu, 0xff8080ffu, //eSOV_InvalidStatic,
		0x4080ffffu, 0xff80ffffu, //eSOV_InvalidDynamic,
		0x4080ff80u, 0xff80ff80u, //eSOV_Valid,
	};
	IRenderAuxGeom* pDC = gEnv->pRenderer->GetIRenderAuxGeom();
	Vec3 UP(0,0,1.f);

	typedef std::vector< TDebugLink > VectorDebugLinks;
	VectorDebugLinks links;

	CSmartObject::MapTemplates& templates = pSmartObject->GetMapTemplates();
	CSmartObject::MapTemplates::iterator itTemplates, itTemplatesEnd = templates.end();
	for ( itTemplates = templates.begin(); itTemplates != itTemplatesEnd; ++itTemplates )
	{
		CClassTemplateData* pTemplate = itTemplates->first;
		CSmartObject::CTemplateData& data = itTemplates->second;

		CSmartObjectClass::THelperLinks::iterator itLinks, itLinksEnd = data.pClass->m_vHelperLinks.end();
		for ( itLinks = data.pClass->m_vHelperLinks.begin(); itLinks != itLinksEnd; ++itLinks )
		{
			CSmartObjectClass::HelperLink& helperLink = *itLinks;
			SmartObjectHelper* pFromHelper = helperLink.from;
			SmartObjectHelper* pToHelper = helperLink.to;

			// check is this link already processed
			VectorDebugLinks::iterator it, itEnd = links.end();
			for ( it = links.begin(); it != itEnd; ++it )
			{
				if ( it->pFromHelper == pFromHelper && it->pToHelper == pToHelper )
					break;
				if ( it->pFromHelper == pToHelper && it->pToHelper == pFromHelper )
				{
					it->bTwoWay = true;
					break;
				}
			}
			if ( it == itEnd )
			{
				TDebugLink link =
				{
					pFromHelper,     //	SmartObjectHelper* pFromHelper;
					pToHelper,       //	SmartObjectHelper* pToHelper;
					NULL,            //	CSmartObject::CTemplateHelperInstance* pFromInstance;
					NULL,            //	CSmartObject::CTemplateHelperInstance* pToInstance;
					0,               //	float fFromRadius;
					0,               //	float fToRadius;
					data.userRadius, //	float fUserRadius;
					false            //	bool bTwoWay;
				};
				links.push_back( link );
			}
		}

		CClassTemplateData::TTemplateHelpers::iterator itHelpers = pTemplate->helpers.begin();
		CSmartObject::TTemplateHelperInstances::iterator itInstances, itInstancesEnd = data.helperInstances.end();
		for ( itInstances = data.helperInstances.begin(); itInstances != itInstancesEnd; ++itInstances, ++itHelpers )
		{
			float radius = itHelpers->radius + data.userRadius;
			Vec3 pos = itInstances->position;
			DebugDrawRangeCircle( gEnv->pRenderer, pos, radius, radius, colors[2*itInstances->validationResult], colors[1+2*itInstances->validationResult], true );

			if ( bStaticOnly )
			{
				pos.z += (data.userTop + data.userBottom)*0.5f;
				pDC->DrawCylinder( pos, UP, radius, data.userTop - data.userBottom, colors[2*itInstances->validationResult] );
			}

			VectorDebugLinks::iterator it, itEnd = links.end();
			for ( it = links.begin(); it != itEnd; ++it )
			{
				if ( !it->pFromInstance && it->pFromHelper->name == itHelpers->name )
				{
					it->pFromInstance = &*itInstances;
					it->fFromRadius = radius;
				}
				if ( !it->pToInstance && it->pToHelper->name == itHelpers->name )
				{
					it->pToInstance = &*itInstances;
					it->fToRadius = radius;
				}
			}
		}
	}

	// draw arrows to show smart object links
	VectorDebugLinks::iterator it, itEnd = links.end();
	for ( it = links.begin(); it != itEnd; ++it )
	{
		if ( !it->pFromInstance || !it->pToInstance )
			continue;
		Vec3 pos, length;
		pos = it->pFromInstance->position;
		length = it->pToInstance->position - pos;
		if ( length.GetLengthSquared2D() > sqr(it->fFromRadius + it->fToRadius) )
		{
			Vec3 n = length;
			n.z = 0;
			n.Normalize();
			pos += n * it->fFromRadius;
			length -= n * ( it->fFromRadius + it->fToRadius );
		}
		if ( it->bTwoWay )
		{
			length *= 0.5f;
			pos += length;
		}
		DebugDrawArrow( gEnv->pRenderer, pos, length, 2*it->fUserRadius, colors[2*pSmartObject->m_eValidationResult] );
		if ( it->bTwoWay )
			DebugDrawArrow( gEnv->pRenderer, pos, -length, 2*it->fUserRadius, colors[2*pSmartObject->m_eValidationResult] );
	}
}

#define CLASS_TEMPLATES_FOLDER "Libs/SmartObjects/ClassTemplates/"

void CSmartObjects::LoadSOClassTemplates()
{
	m_mapClassTemplates.clear();

	string sLibPath = PathUtil::Make( CLASS_TEMPLATES_FOLDER, "*", "xml" );
	ICryPak *pack = gEnv->pCryPak;
	_finddata_t fd;
	intptr_t handle = pack->FindFirst( sLibPath, &fd );
	int nCount = 0;
	if ( handle < 0 )
		return;

	do
	{
		XmlNodeRef root = GetISystem()->LoadXmlFile( string(CLASS_TEMPLATES_FOLDER) + fd.name );
		if ( !root )
			continue;

		CClassTemplateData classTemplateData;
		classTemplateData.name =  PathUtil::GetFileName( fd.name );
		classTemplateData.name.MakeLower();
		CClassTemplateData* data =  & m_mapClassTemplates.insert(std::make_pair( classTemplateData.name, classTemplateData )).first->second;

		int count = root->getChildCount();
		for ( int i = 0; i < count; ++i )
		{
			XmlNodeRef node = root->getChild(i);
			if ( node->isTag("object") )
			{
				data->model = node->getAttr( "model" );
			}
			else if ( node->isTag("helper") )
			{
				CClassTemplateData::CTemplateHelper helper;
				helper.name = node->getAttr( "name" );
				node->getAttr( "pos", helper.qt.t );
				node->getAttr( "rot", helper.qt.q );
				node->getAttr( "radius", helper.radius );
				node->getAttr( "projectOnGround", helper.project );
				data->helpers.push_back( helper );
			}
		}
	} while ( pack->FindNext( handle, &fd ) >= 0 );

	pack->FindClose(handle);
}


//===================================================================
// GetNavNodes
//===================================================================
unsigned CSmartObject::GetNavNodes( unsigned nodeIndices[2], const SmartObjectHelper* pHelper )
{
	std::pair< MapNavNodeIds::iterator, bool > result = m_mapNavNodes.insert( std::make_pair(pHelper, 0) );
	if ( result.second )
	{
		// new entry - find enclosing node in store the result
		result.first->second = GetAISystem()->GetGraph()->GetEnclosing(
			pHelper ? GetHelperPos(pHelper) : GetPos(), SMART_OBJECT_ENCLOSING_NAV_TYPES );
	}
	nodeIndices[0] = result.first->second;
	return nodeIndices[0] ? 1 : 0;
}

Vec3 CSmartObject::MeasureUserSize() const
{
	Vec3 result( ZERO );

	CAIObject* pObject = GetAI();
	if ( !pObject || pObject->GetType() != AIOBJECT_PUPPET )
		return result;

	IUnknownProxy* pProxy = pObject->GetProxy();
	if ( !pProxy )
		return result;

	EStance stances[] =
	{
		STANCE_STAND,
		STANCE_CROUCH,
		STANCE_RELAXED,
		STANCE_STEALTH,
	};
	unsigned numStances = sizeof(stances) / sizeof(EStance);

	AABB stanceSize( AABB::RESET );
	AABB colliderSize( AABB::RESET );

	for ( unsigned i = 0; i < numStances; ++i )
	{
		SAIBodyInfo bi;
		if ( pProxy->QueryBodyInfo(stances[i], 0.0f, true, bi) && bi.colliderSize.min.z >= 0.0f )
		{
			stanceSize.Add( bi.stanceSize );
			colliderSize.Add( bi.colliderSize );
		}
	}

	if ( !stanceSize.IsEmpty() && !colliderSize.IsEmpty() )
	{
		result.x = max( colliderSize.GetSize().x, colliderSize.GetSize().y ) * 0.5f; // radius
		result.y = colliderSize.GetSize().z; // height
		result.z = stanceSize.GetSize().z - colliderSize.GetSize().z; // distance from ground
	}
	return result;
}


//===================================================================
// CQueryEvent::Serialize
//===================================================================
void CQueryEvent::Serialize(TSerialize ser)
{
	ser.BeginGroup("QueryEvent");

	CSmartObjects::SerializePointer(ser,"pUser",pUser);
	CSmartObjects::SerializePointer(ser,"pObject",pObject);
	CSmartObjects::SerializePointer(ser,"pRule",pRule);
		// TO DO: serialize the following when they'll be used
		// pChainedUserEvent;
		// pChainedObjectEvent;

	ser.EndGroup();
}
