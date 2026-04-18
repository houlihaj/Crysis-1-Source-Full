/********************************************************************
  CryGame Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   AIHandler.cpp
  Version:     v1.00
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 8:10:2004   12:05 : Created by Kirill Bulatsev

*********************************************************************/



#include "StdAfx.h"
#include <ISound.h>
#include <IAISystem.h>
#include <ICryAnimation.h>
#include <ISerialize.h>
#include <IActorSystem.h>
#include <IAnimationGraph.h>
#include "ScriptBind_AI.h"
#include "AIHandler.h"
#include "AIProxy.h"
#include "CryActionCVars.h"

//#include <ILipSync.h>
#if defined(__GNUC__)
#include <float.h>
#endif

//
//------------------------------------------------------------------------------
CAIHandler::CAIHandler(IGameObject *pGameObject) :
	m_ReadibilitySoundID(INVALID_SOUNDID),
	m_bSoundFinished(false),
	m_pSoundPackNormal(NULL),
	m_pSoundPackAlternative(NULL),
	m_pAGState(NULL),
	m_actorTargetStartedQueryID(0),
	m_actorTargetEndQueryID(0),
	m_eActorTargetPhase(eATP_None),
	m_changeActionInputQueryId(0),
	m_changeSignalInputQueryId(0),
	m_playingSignalAnimation(false),
	m_playingActionAnimation(false),
	m_bOwnsActionInput(false),
	m_enabledTargetPointVerifier(false),
	m_actorTargetId(0),
	m_pGameObject(pGameObject),
	m_pEntity(pGameObject->GetEntity()),
	m_FaceManager(pGameObject->GetEntity()),
	m_bDelayedCharacterConstructor(false),
	m_bDelayedBehaviorConstructor(false),
	m_vAnimationTargetPosition(ZERO)
{
	m_curActorTargetStartedQueryID = &m_actorTargetStartedQueryID;
	m_curActorTargetEndQueryID = &m_actorTargetEndQueryID;
}

//
//------------------------------------------------------------------------------
CAIHandler::~CAIHandler(void)
{
	if ( m_pAGState )
		m_pAGState->RemoveListener( this );

	if (*m_pPreviousBehavior == *m_pBehavior)
		m_pPreviousBehavior = 0;

	// stop sound
	if (gEnv->pSoundSystem)
	{
		_smart_ptr<ISound> pSound = gEnv->pSoundSystem->GetSound(m_ReadibilitySoundID);
		if (pSound)
		{
			pSound->RemoveEventListener(this);
			pSound->Stop();
		}
	}
	m_ReadibilitySoundID = INVALID_SOUNDID;
}


//
//------------------------------------------------------------------------------
const char* CAIHandler::GetInitialCharacterName()
{
	const char* szAICharacterFileName = 0;
	const char* szAICharacterName = 0;
	SmartScriptTable pEntityProperties;
	SmartScriptTable pEntityPropertiesInstance;

	if (!m_pScriptObject->GetValue("Properties", pEntityProperties))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find Properties. Entity %s", m_pEntity->GetName());
		return 0;
	}

	if (!pEntityProperties->GetValue("aicharacter_character", szAICharacterName))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find aicharacter_character. Entity %s", m_pEntity->GetName());
		return 0;
	}

	return szAICharacterName;
}

//
//------------------------------------------------------------------------------
const char* CAIHandler::GetInitialBehaviorName()
{
	SmartScriptTable pEntityProperties;
	SmartScriptTable pEntityPropertiesInstance;

	if (!m_pScriptObject->GetValue("PropertiesInstance", pEntityPropertiesInstance))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find PropertiesInstance. Entity %s", m_pEntity->GetName());
		return 0;
	}

	const char* szAIBehaviorName = 0;
	if (!pEntityPropertiesInstance.GetPtr())
		return 0;
	if (!pEntityPropertiesInstance->GetValue("aibehavior_behaviour", szAIBehaviorName))
		return 0;

	return szAIBehaviorName;
}

//
//------------------------------------------------------------------------------
void CAIHandler::Init(ISystem* pSystem)
{
	m_ActionQueryID = m_SignalQueryID = 0;
	m_sQueriedActionAnimation.clear();
	m_sQueriedSignalAnimation.clear();
	m_bSignaledAnimationStarted = false;
	m_setPlayedSignalAnimations.clear();
	m_setStartedActionAnimations.clear();

	m_pScriptObject = m_pEntity->GetScriptTable();

	//[Timur] AI Entities usually must be on radar.
	m_pEntity->SetFlags(m_pEntity->GetFlags() | ENTITY_FLAG_ON_RADAR);

	if (!SetCommonTables())
		return;

	SetupSoundPack();
	SetupAnimPack();

	SetInitialBehaviorAndCharacter();

	m_pPreviousBehavior = 0;
	m_CurrentAlertness = 0;
	m_CurrentExclusive = false;

	// Precache facial animations.
	m_FaceManager.PrecacheSequences();
}

//
//------------------------------------------------------------------------------
void CAIHandler::ResetCommonTables()
{
	m_pBehaviorTable = 0;
	m_pBehaviorTableAVAILABLE = 0;
	m_pBehaviorTableINTERNAL = 0;
	m_pDEFAULTDefaultBehavior = 0;
	m_pDefaultCharacter = 0;
}

//
//------------------------------------------------------------------------------
bool CAIHandler::SetCommonTables()
{
	// Get common tables
	if (!gEnv->pScriptSystem->GetGlobalValue("AIBehaviour", m_pBehaviorTable))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find AIBehaviour table ");
		return false;
	}

	if (!m_pBehaviorTable->GetValue("AVAILABLE", m_pBehaviorTableAVAILABLE))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find AVAILABLE TABLE");
		return false;
	}

	if (!m_pBehaviorTable->GetValue("INTERNAL",m_pBehaviorTableINTERNAL))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find AIBehavior.INTERNAL table.");
		return false;
	}

	if (!m_pBehaviorTable->GetValue("DEFAULT", m_pDEFAULTDefaultBehavior))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find DEFAULT. Entity %s", m_pEntity->GetName());
		return false;
	}

	SmartScriptTable pCharacterTable;
	if (!gEnv->pScriptSystem->GetGlobalValue("AICharacter", pCharacterTable))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find AICharacter table. Entity %s", m_pEntity->GetName());
		return false;
	}

	if (!pCharacterTable->GetValue("DEFAULT", m_pDefaultCharacter))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find DEFAULT character. Entity %s", m_pEntity->GetName());
		return false;
	}

	return true;
}

//
//------------------------------------------------------------------------------
void CAIHandler::SetInitialBehaviorAndCharacter()
{
	ResetCharacter();
	ResetBehavior();

	const char* szDefaultCharacter = GetInitialCharacterName();
	m_sFirstCharacterName = szDefaultCharacter;
	SetCharacter(szDefaultCharacter, SET_DELAYED);

	const char* szDefaultBehavior = GetInitialBehaviorName();
	m_sFirstBehaviorName = szDefaultBehavior;
	SetBehavior(szDefaultBehavior, 0, SET_DELAYED);
}

//
//------------------------------------------------------------------------------
void CAIHandler::ResetAnimationData()
{
	m_bOwnsActionInput = false;
	m_ActionQueryID = m_SignalQueryID = 0;
	m_sQueriedActionAnimation.clear();
	m_sQueriedSignalAnimation.clear();
	m_bSignaledAnimationStarted = false;
	m_setPlayedSignalAnimations.clear();
	m_setStartedActionAnimations.clear();

	m_playingSignalAnimation = false;
	m_currentSignalAnimName.clear();
	m_playingActionAnimation = false;
	m_currentActionAnimName.clear();
	m_sAGActionSOAutoState.clear();

	m_eActorTargetPhase = eATP_None;
	m_actorTargetId = 0;
	m_actorTargetStartedQueryID = 0;
	m_actorTargetEndQueryID = 0;
	m_curActorTargetStartedQueryID = &m_actorTargetStartedQueryID;
	m_curActorTargetEndQueryID = &m_actorTargetEndQueryID;
	m_vAnimationTargetPosition.zero();
	if ( IMovementController* pMC = (m_pGameObject ? m_pGameObject->GetMovementController() : NULL) )
	{
		// always clear actor target
		CMovementRequest mr;
		mr.ClearActorTarget();
		pMC->RequestMovement(mr);
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::Reset(EObjectResetType type)
{
	m_FaceManager.Reset();

	ResetAnimationData();

	m_bSoundFinished = false;

	Release();

	m_CurrentAlertness = 0;
	m_CurrentExclusive = false;

	if (type == AIOBJRESET_SHUTDOWN)
	{
		ResetCharacter();
		ResetBehavior();
		ResetCommonTables();
	}
	else
	{
		SetCommonTables();
		// Reset behavior and character.
		SetInitialBehaviorAndCharacter();
	}

	m_pPreviousBehavior = 0;
	m_sPrevCharacterName.clear();


/*	SmartScriptTable pCharacterTable;
	gEnv->pScriptSystem->GetGlobalValue("AICharacter",pCharacterTable);

	//character ----------------------------------------------------------
	//	-- load the character only if it is used

	SmartScriptTable	pAvailableCharacter;
	pCharacterTable->GetValue("AVAILABLE",pAvailableCharacter);
	SmartScriptTable	pInternalCharacter;
	pCharacterTable->GetValue("INTERNAL",pInternalCharacter);
	const char *aiCharacterFileName=NULL;
	const char *aiCharacterName=NULL;
	SmartScriptTable	pEntityProperties;
	SmartScriptTable	pEntityPropertiesInstance;
	if(!m_pScriptObject->GetValue("Properties",pEntityProperties))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find Properties. Entity %s", m_pEntity->GetName());
		goto	BEHAVIOR_LOADING;
	}

	if(!m_pScriptObject->GetValue("PropertiesInstance",pEntityPropertiesInstance))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find PropertiesInstance. Entity %s", m_pEntity->GetName());
		goto	BEHAVIOR_LOADING;
	}

	if(!pEntityProperties->GetValue("aicharacter_character",aiCharacterName))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find aicharacter_character. Entity %s", m_pEntity->GetName());
		goto	BEHAVIOR_LOADING;
	}
	if(!pAvailableCharacter->GetValue(aiCharacterName,aiCharacterFileName))
		if(!pInternalCharacter->GetValue(aiCharacterName,aiCharacterFileName))
		{
			gEnv->pAISystem->Warning("<CAIHandler> ", "can't find [%s] in AICharacter.AVAILABLE/INTERNAL. Entity %s", aiCharacterName, m_pEntity->GetName());
			goto	BEHAVIOR_LOADING;
		}

	if(!pCharacterTable->GetValue(aiCharacterName, m_pCharacter))	//[petar] if character script preloaded do not load
	{
		if(gEnv->pScriptSystem->ExecuteFile(aiCharacterFileName,true,true)) // [petar] if not preloaded force load
		{
			if(!pCharacterTable->GetValue(aiCharacterName, m_pCharacter))
			{
				// did not find script for character
				gEnv->pAISystem->Warning("<CAIHandler> ", "can't find script for character [%s]. Entity %s", aiCharacterFileName, m_pEntity->GetName());
			}
// now character constructor is called only from script, when initing AI
//			else
//			{
//				//call Character constructor
//				CallCharacterConstructor();
//			}

		}
		else
		{
			// could not load script for character
			gEnv->pAISystem->Warning("<CAIHandler> ", "can't load script for character [%s]. Entity %s", aiCharacterFileName, m_pEntity->GetName());
		}
	}
// now character constructor is called only from script, when initing AI
//	else
//	{
// //call Character constructor
//		CallCharacterConstructor();
//	}


	m_sCharacterName = aiCharacterName;
	m_sPrevCharacterName = "";
	m_sFirstCharacterName = aiCharacterName;

	if(!pCharacterTable->GetValue("DEFAULT", m_pDefaultCharacter))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find DEFAULT character. Entity %s", m_pEntity->GetName());
	}

BEHAVIOR_LOADING:	
	//behaviour ----------------------------------------------------------
	//	m_pBehaviorTable = gEnv->pScriptSystem->CreateEmptyObject();
	//	m_pBehaviorTable->AddRef();
	if(!gEnv->pScriptSystem->GetGlobalValue("AIBehaviour", m_pBehaviorTable))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find AIBehaviour table ");
		return;
	}

	if(!m_pBehaviorTable->GetValue("AVAILABLE",m_pBehaviorTableAVAILABLE))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find AVAILABLE TABLE");
		return;
	}

	//	m_pBehaviorTableINTERNAL = gEnv->pScriptSystem->CreateEmptyObject();
	//	m_pBehaviorTableINTERNAL->AddRef();
	if(!m_pBehaviorTable->GetValue("INTERNAL",m_pBehaviorTableINTERNAL))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find AIBehavior.INTERNAL table.");
	}

	const char *aiBehaviorFileName=NULL;
	const char *aiBehaviorName=NULL;
	if(pEntityPropertiesInstance.GetPtr() == NULL
			|| !pEntityPropertiesInstance->GetValue("aibehavior_behaviour",aiBehaviorName))
	{
		goto	BEHAVIOR_DEFAULT;
	}
	m_FirstBehaviorName = string(aiBehaviorName);
	if(!m_pBehaviorTableAVAILABLE->GetValue(aiBehaviorName,aiBehaviorFileName))
	{
		if (!m_pBehaviorTableINTERNAL)
		{
			gEnv->pAISystem->Error("<CAIHandler> ", "Internal behaviour table not found. Cannot continue...");
			return;
		}
		if(!m_pBehaviorTableINTERNAL->GetValue(aiBehaviorName,aiBehaviorFileName))
			goto	BEHAVIOR_DEFAULT;
	}

	//	m_pBehavior = gEnv->pScriptSystem->CreateEmptyObject();		
	//	m_pBehavior->AddRef();
	if(!m_pBehaviorTable->GetValue(aiBehaviorName, m_pBehavior))	//[petar] if behaviour not preloaded
	{
		if(gEnv->pScriptSystem->ExecuteFile(aiBehaviorFileName,true,true)) // [petar] force that it be loaded
		{
			if(!m_pBehaviorTable->GetValue(aiBehaviorName, m_pBehavior))
			{
				// did not find script for character
				// use default behavior
				gEnv->pAISystem->Warning("<CAIHandler> ", "can't find script for behavior [%s]. Using DEFAULT. Entity %s", aiBehaviorName, m_pEntity->GetName());		
				if(!m_pBehaviorTable->GetValue("DEFAULT", m_pBehavior))
				{
					gEnv->pAISystem->Warning("<CAIHandler> ", "can't find DEFAULT. Entity %s", m_pEntity->GetName());					
				}
			}
		}
		else
		{
			// could not load script for behavior
			gEnv->pAISystem->Warning("<CAIHandler> ", "can't load script for behavior [%s]. Entity %s", aiBehaviorFileName, m_pEntity->GetName());		
		}
	}

	// read initial alertness state
	if ( *m_pBehavior )
	{
		int alertness = 0;
		m_pBehavior->GetValue( "alertness", alertness );
		SetAlertness( alertness );
	}

	m_sBehaviorName = aiBehaviorName;

BEHAVIOR_DEFAULT:

	//	m_pDEFAULTDefaultBehavior = gEnv->pScriptSystem->CreateEmptyObject();
	//	m_pDEFAULTDefaultBehavior->AddRef();
	if(!m_pBehaviorTable->GetValue("DEFAULT", m_pDEFAULTDefaultBehavior))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find DEFAULT. Entity %s", m_pEntity->GetName());
	}

	if(aiCharacterName)
	{
		m_sDefaultBehaviorName = string(aiCharacterName) + string("Idle");

		int	jobFlag=0;
		if( *m_pBehavior && m_pBehavior->GetValue("JOB", jobFlag) )
		{
			//	m_pBehavior->SetValue("Name", m_sDefaultBehaviorName.c_str());
			m_sBehaviorName = m_sDefaultBehaviorName;
		}

		//		m_pDefaultBehavior = gEnv->pScriptSystem->CreateEmptyObject();
		//		m_pDefaultBehavior->AddRef();

		{
			//			if (!(m_pDefaultBehavior = FindOrLoadTable(m_pBehaviorTable,m_sDefaultBehaviorName.c_str())))
			if (!(FindOrLoadTable(m_pBehaviorTable,m_sDefaultBehaviorName.c_str(), m_pDefaultBehavior)))
			{
				gEnv->pAISystem->Warning("<CAIHandler> ", "can't find default behaviour %s. Entity %s", m_sDefaultBehaviorName.c_str(), m_pEntity->GetName());
			}
		}
		//		m_pDefaultBehavior->AddRef();
	}



	if(*m_pBehavior)
		m_pScriptObject->SetValue("Behaviour", m_pBehavior);
	//	if(m_pDefaultBehavior)
	m_pScriptObject->SetValue("DefaultBehaviour", m_sDefaultBehaviorName.c_str());


	// not needed anymore
	//SmartScriptTable	pAIAnchorTable;
	//if(gEnv->pScriptSystem->GetGlobalValue("AIAnchorTable",pAIAnchorTable))
	//	pAIAnchorTable->GetValue("AIOBJECT_DAMAGEGRENADE", m_DamageGrenadeType);


	//SoundPacks ----------------------------------------------------------
	//	
	UpdateSoundPack();


	//AniPacks ----------------------------------------------------------
	//	
	SmartScriptTable	pAnimPacksTable;

	if(gEnv->pScriptSystem->GetGlobalValue("ANIMATIONPACK",pAnimPacksTable))	// SOUNDPACK table 
	{
		const char *aiAnimPackName=NULL;
		if(pEntityProperties.GetPtr() && pEntityProperties->GetValue("AnimPack",aiAnimPackName))
		{
			const char *aiAnimPackFileName=NULL;
//			SmartScriptTable	pAvailableTable;
//			if(pAnimPacksTable->GetValue("AVAILABLE",pAvailableTable))
//				if(pAvailableTable->GetValue(aiAnimPackName,aiAnimPackFileName))
//					FindOrLoadTable( pAnimPacksTable, aiAnimPackFileName, m_pAnimationPackTable );
			if(!FindOrLoadTable( pAnimPacksTable, aiAnimPackName, m_pAnimationPackTable ))
				gEnv->pAISystem->Warning("<CAIHandler> ", "Could not load AnimPack table %s for entity %s.",aiAnimPackName,m_pEntity->GetName());
		}
	}
*/
}


//
//------------------------------------------------------------------------------
void	CAIHandler::AIMind( SOBJECTSTATE *state )
{
	FUNCTION_PROFILER( gEnv->pSystem, PROFILE_AI );
	HSCRIPTFUNCTION	handlerFunc = NULL;
	const char* eventString = 0;
	bool bNotifyTarget = false;

	float* value = 0;
	int targetType = -1;

	IAISignalExtraData* pExtraData = NULL;

	switch(state->eTargetType)
	{
		case AITARGET_MEMORY:
			if (state->eTargetThreat == AITHREAT_THREATENING)
			{
				value = &state->fDistanceFromTarget;
				eventString = "OnEnemyMemory";
			}
			break;
		case AITARGET_SOUND:
			if (state->eTargetThreat >= AITHREAT_THREATENING)
			{
				FRAME_PROFILER( "AI_OnThreateningSoundHeard",gEnv->pSystem,PROFILE_AI );
				value = &state->fDistanceFromTarget;
				eventString = "OnThreateningSoundHeard";
			}
			else if (state->eTargetThreat == AITHREAT_INTERESTING)
			{
				value = &state->fDistanceFromTarget;
				eventString = "OnInterestingSoundHeard";
			}
			break;
		case AITARGET_VISUAL:

			if (state->nTargetType > AIOBJECT_PLAYER) //-- grenade (or any other registered object type) seen
			{
				FRAME_PROFILER( "AI_OnObjectSeen", gEnv->pSystem, PROFILE_AI );

				value = &state->fDistanceFromTarget;
				if (!pExtraData)
					pExtraData = gEnv->pAISystem->CreateSignalExtraData();
				pExtraData->iValue = state->nTargetType;

				IPipeUser* pPipeUser = CastToIPipeUserSafe(m_pEntity->GetAI());
				if (pPipeUser)
				{
					IAIObject* pTarget = pPipeUser->GetAttentionTarget();
					if (pTarget)
					{
						IEntity* pTargetEntity = (IEntity*)(pTarget->GetEntity());
						if (pTargetEntity)
						{
							pExtraData->nID = pTargetEntity->GetId();
							pExtraData->point = pTarget->GetPos();
							pe_status_dynamics sd;
							if (pTargetEntity->GetPhysics())
							{
								pTargetEntity->GetPhysics()->GetStatus( &sd );
								pExtraData->point2 = sd.v;
							}
							else
								pExtraData->point2 = ZERO;
						}
					}
				}
				eventString = "OnObjectSeen";
			}
			else if (state->eTargetThreat == AITHREAT_AGGRESSIVE)
			{
				if (!pExtraData)
					pExtraData = gEnv->pAISystem->CreateSignalExtraData();
				pExtraData->iValue = (int)state->eTargetStuntReaction;
				state->eTargetStuntReaction = AITSR_NONE;

				FRAME_PROFILER( "AI_OnPlayerSeen2",gEnv->pSystem,PROFILE_AI );
				value = &state->fDistanceFromTarget;
				
				IPipeUser *pPipeUser = CastToIPipeUserSafe(m_pEntity->GetAI());
				if (pPipeUser)
				{
					IAIObject* pTarget = pPipeUser->GetAttentionTarget();
					// sends OnSeenByEnemy to the target 
					// only if the target can acquire the sender as his target as well
					IAIActor* pTargetActor = CastToIAIActorSafe(pTarget);
					if (pTargetActor && pTargetActor->CanAcquireTarget(m_pEntity->GetAI()))
					{
						if(pTarget->CastToIPuppet() || pTarget->CastToCAIPlayer())
						{
							int iCombatClass = pTargetActor->GetParameters().m_CombatClass;
							eventString = gEnv->pAISystem->GetCustomOnSeenSignal(iCombatClass);
							// sends OnSeenByEnemy only if the target sees the sender
							if (pTarget->IsPointInFOVCone(m_pEntity->GetAI()->GetPos()))
							{
								gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER,1,"OnSeenByEnemy",pTarget);
							}
						}
					}
				}
				// if it's a custom signal first check is there a handler for that signal
				if (eventString != 0)
				{
					gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_HANDLERNEVENT, eventString);

					IAIRecordable::RecorderEventData recorderEventData(eventString);
					m_pEntity->GetAI()->RecordEvent(IAIRecordable::E_SIGNALRECIEVED, &recorderEventData);

					if (CallBehaviorOrDefault(eventString, value, pExtraData))
					{
						// check the character table for the custom signal only if there was a signal handler in the behaviors
						if (const char* szNextBehavior = CheckAndGetBehaviorTransition(eventString))
							SetBehavior(szNextBehavior, pExtraData);

						if (pExtraData)
							gEnv->pAISystem->FreeSignalExtraData(pExtraData);

						// return now or the signal will be sent once again
						return;
					}
				}
				// otherwise revert the custom signal to OnPlayerSeen
				eventString = "OnPlayerSeen";
			}
			else if (state->eTargetThreat == AITHREAT_THREATENING)
			{
				eventString = "OnThreateningSeen";
			}
			else if (state->eTargetThreat == AITHREAT_INTERESTING)
			{
				eventString = "OnSomethingSeen";
			}
			break;
		case AITARGET_NONE:
			eventString = "OnNoTarget";
			break;
	}

//	assert(eventString);

	if (eventString)
	{
		gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_HANDLERNEVENT, eventString);

		IAIRecordable::RecorderEventData recorderEventData(eventString);
		m_pEntity->GetAI()->RecordEvent(IAIRecordable::E_SIGNALRECIEVED, &recorderEventData);

		CallBehaviorOrDefault(eventString, value, pExtraData);
		if (const char* szNextBehavior = CheckAndGetBehaviorTransition(eventString))
			SetBehavior(szNextBehavior, pExtraData);
	}

	if (pExtraData)
		gEnv->pAISystem->FreeSignalExtraData(pExtraData);

	UpdateWeaponAlertness();
}


//
//------------------------------------------------------------------------------
void	CAIHandler::AISignal( int signalID, const char * signalText, IEntity *pSender, const IAISignalExtraData* pData )
{
	FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );

	if (!signalText)
		return;

	if(signalID != AISIGNAL_PROCESS_NEXT_UPDATE)
	{
		IAIRecordable::RecorderEventData recorderEventData(signalText);
		m_pEntity->GetAI()->RecordEvent(IAIRecordable::E_SIGNALEXECUTING, &recorderEventData);
		gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_SIGNALEXECUTING, signalText);
	}


	// Send AI event.
	if (!strcmp(signalText, "ACTION_DONE"))
	{
		SEntityEvent event(ENTITY_EVENT_AI_DONE);
		m_pEntity->SendEvent(event);
	}

//	gEnv->pAISystem->LogComment("<CAIHandler>","Entity %s processing signal %s in behaviour %s",m_pEntity->GetName(),signalText,m_sBehaviorName);

	HSCRIPTFUNCTION	singnalHandler = 0;

	//gEnv->pAISystem->Warning("<CAIHandler> ", " >> %s", signalText);

	if (!CallScript(m_pBehavior, signalText, NULL, pSender, pData))
	{	
		// try default in behavior
		if (!CallScript(m_pDefaultBehavior, signalText, NULL, pSender, pData))
		{
			// try global default
			if (CallScript(m_pDEFAULTDefaultBehavior, signalText, NULL, pSender, pData))
			{
				gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_SIGNALEXECUTING, "from defaultDefault behaviour");
			}
		}
		else
		{
			gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_SIGNALEXECUTING, "from default behaviour");
		}
	}

	if (const char* szNextBehavior = CheckAndGetBehaviorTransition(signalText))
		SetBehavior(szNextBehavior, pData);
}

//
//------------------------------------------------------------------------------
const char* CAIHandler::CheckAndGetBehaviorTransition(const char* szSignalText)
{
	FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );

	if (!szSignalText || strlen(szSignalText) < 2)
		return 0;

	SmartScriptTable	pCharacterTable;	
	SmartScriptTable	pCharacterDefaultTable;
	SmartScriptTable	pNextBehavior;	
	const char* szBehaviorName = 0;
	const char* szNextBehaviorName = 0;

	if (*m_pBehavior && *m_pCharacter)
	{
		m_pBehavior->GetValue("Name", szBehaviorName);
		bool bTable = m_pCharacter->GetValue(szBehaviorName, pCharacterTable);

		if (bTable && pCharacterTable->GetValue(szSignalText, szNextBehaviorName) && !*szNextBehaviorName)
			return 0;

		bool bDefaultTable = m_pCharacter->GetValue("AnyBehavior", pCharacterDefaultTable);

		if (bTable || bDefaultTable)
		{
			if ((bTable && pCharacterTable->GetValue(szSignalText, szNextBehaviorName)) || 
					(bDefaultTable && pCharacterDefaultTable->GetValue(szSignalText, szNextBehaviorName)) )
			{
				if (*szNextBehaviorName)
				{
					FRAME_PROFILER("Logging of the character change", gEnv->pSystem, PROFILE_AI);
					if (m_pEntity && m_pEntity->GetName())
					{
						gEnv->pAISystem->LogComment("<CAIHandler> ", "Entity %s changing behavior from %s to %s on signal %s",
							m_pEntity->GetName(), szBehaviorName ? szBehaviorName : "<null>",
							szNextBehaviorName ? szNextBehaviorName : "<null>", szSignalText ? szSignalText : "<null>");
					}
					return szNextBehaviorName;
				}
			}
		}
	}

	if (!m_pDefaultCharacter.GetPtr())
		return 0;

	bool tableIsValid = false;
	if (*m_pBehavior)
	{
		szBehaviorName = m_sBehaviorName.c_str();
		if (!(tableIsValid = m_pDefaultCharacter->GetValue(szBehaviorName, pCharacterTable)))
			tableIsValid = m_pDefaultCharacter->GetValue("NoBehaviorFound", pCharacterTable);
	}
	else
		tableIsValid = m_pDefaultCharacter->GetValue("NoBehaviorFound", pCharacterTable);

	if (tableIsValid && pCharacterTable->GetValue(szSignalText, szNextBehaviorName) && *szNextBehaviorName)
	{
		FRAME_PROFILER( "Logging of DEFAULT character change",gEnv->pSystem,PROFILE_AI );
		if(m_pEntity && m_pEntity->GetName())
		{
			gEnv->pAISystem->LogComment("<CAIHandler> ", "Entity %s changing behavior from %s to %s on signal %s [DEFAULT character]",
				m_pEntity->GetName(), szBehaviorName ? szBehaviorName : "<null>",
				szNextBehaviorName ? szNextBehaviorName : "<null>", szSignalText ? szSignalText : "<null>");
		}
		return szNextBehaviorName;
	}

	return 0;
}

//
//------------------------------------------------------------------------------
void CAIHandler::ResetCharacter()
{
	// call destructor of current character
	if (m_pCharacter.GetPtr())
	{
		HSCRIPTFUNCTION pDestructor = 0;
		if (m_pCharacter->GetValue("Destructor", pDestructor))
		{
			gEnv->pScriptSystem->BeginCall(pDestructor);
			gEnv->pScriptSystem->PushFuncParam(m_pScriptObject);
			gEnv->pScriptSystem->EndCall();
		}
	}

	m_pCharacter = 0;
	m_sCharacterName.clear();
	m_sDefaultBehaviorName = "";
	m_pScriptObject->SetValue("DefaultBehaviour", m_sDefaultBehaviorName.c_str());
	m_pDefaultBehavior = 0;
	m_bDelayedCharacterConstructor = false;
}

//
//------------------------------------------------------------------------------
bool CAIHandler::SetCharacter(const char* szCharacter, ESetFlags setFlags)
{
	if (!szCharacter || !*szCharacter)
	{
		ResetCharacter();
		return false;
	}

	// only if it's a different one
	if (m_sCharacterName == szCharacter)
		return true;

	SmartScriptTable pCharacterTable; // points to global table AICharacter
	if (!gEnv->pScriptSystem->GetGlobalValue("AICharacter", pCharacterTable))
	{
		ResetCharacter();
		return false;
	}

	// check is specified character already loaded
	SmartScriptTable pCharacter; // should point to next character table
	if (!pCharacterTable->GetValue(szCharacter, pCharacter))
	{
		SmartScriptTable pAvailableCharacter;
		if (!pCharacterTable->GetValue("AVAILABLE", pAvailableCharacter) && !pCharacterTable->GetValue("INTERNAL", pAvailableCharacter))
		{
			ResetCharacter();
			return false;
		}

		// get file name for specified character
		const char* szFileName = 0;
		if (!pAvailableCharacter->GetValue(szCharacter, szFileName))
		{
			ResetCharacter();
			return false;
		}

		// load the character table
		if (!gEnv->pScriptSystem->ExecuteFile(szFileName, false, true))
		{
			ResetCharacter();
			return false;
		}

		// now try to get the table once again
		if (!pCharacterTable->GetValue(szCharacter, pCharacter))
		{
			ResetCharacter();
			return false;
		}
	}

	// call destructor of current character
	if (setFlags != SET_ON_SERILIAZE)
	{
		if (m_pCharacter.GetPtr())
		{
			HSCRIPTFUNCTION pDestructor = 0;
			if (m_pCharacter->GetValue("Destructor", pDestructor))
			{
				gEnv->pScriptSystem->BeginCall(pDestructor);
				gEnv->pScriptSystem->PushFuncParam(m_pScriptObject);
				gEnv->pScriptSystem->EndCall();
			}
		}
	}

	m_sPrevCharacterName = m_sCharacterName;
	m_sCharacterName = szCharacter;

	m_pCharacter = pCharacter;

	// adjust default behavior name entry in entity script table
	m_sDefaultBehaviorName = szCharacter;
	m_sDefaultBehaviorName += "Idle";
	m_pScriptObject->SetValue("DefaultBehaviour", m_sDefaultBehaviorName.c_str());
	if (!m_pBehaviorTable.GetPtr())
	{
		gEnv->pAISystem->Warning( "<CAIHandler> ", "%s m_pBehaviorTable not set up", m_pEntity->GetName());
		return false;
	}
	if (!FindOrLoadTable(m_pBehaviorTable, m_sDefaultBehaviorName.c_str(), m_pDefaultBehavior))
		gEnv->pAISystem->Warning( "<CAIHandler> ", "can't find default behaviour %s", m_sDefaultBehaviorName.c_str() );

	if (setFlags == SET_DELAYED)
	{
		m_bDelayedCharacterConstructor = true;
	}
	else if (setFlags == SET_IMMEDIATE)
	{
		m_bDelayedCharacterConstructor = false;
		CallCharacterConstructor();
	}

	return true;
}

//
//------------------------------------------------------------------------------
const char* CAIHandler::GetCharacter()
{
	return m_sCharacterName.c_str();
}

//
//------------------------------------------------------------------------------
const char* CAIHandler::GetBehavior()
{
	return m_sBehaviorName.c_str();
}

//
//------------------------------------------------------------------------------
void CAIHandler::ResetBehavior()
{
	// Finish up with the old behavior.
	if (m_pBehavior.GetPtr())
	{
		int noPrevious = 0;
		if (!m_pBehavior->GetValue("NOPREVIOUS", noPrevious))
			m_pPreviousBehavior = m_pBehavior;
		// executing behavior destructor
		CallScript(m_pBehavior, "Destructor", 0, 0, 0);
		gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_BEHAVIORDESTRUCTOR, m_sBehaviorName);
	}

	m_sBehaviorName.clear();
	m_pBehavior = 0;
	SetAlertness(0, false);
	m_CurrentExclusive = false;
	m_bDelayedBehaviorConstructor = false;
}

//
//------------------------------------------------------------------------------
void	CAIHandler::SetBehavior(const char* szNextBehaviorName, const IAISignalExtraData* pData, ESetFlags setFlags)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	SmartScriptTable pNextBehavior;

	if (szNextBehaviorName)
	{
		if (strcmp(szNextBehaviorName, "PREVIOUS") == 0)
		{
			pNextBehavior = m_pPreviousBehavior;
			if (pNextBehavior.GetPtr())
			{
				const char* szCurBehaviorName;
				pNextBehavior->GetValue("Name", szCurBehaviorName);

				// Skip ctor/dtor if previous is the same as current behavior.
				if (m_sBehaviorName == szCurBehaviorName)
					return;

				m_sBehaviorName = szNextBehaviorName;
			}
		}
		else 
		{
			if (strcmp(szNextBehaviorName, "FIRST") == 0)
				szNextBehaviorName = m_sFirstBehaviorName.c_str();

			FindOrLoadBehavior(szNextBehaviorName, pNextBehavior);
/*
			if (!m_pBehaviorTable->GetValue(szNextBehaviorName, pNextBehavior))
			{
				//[petar] if behaviour not preloaded then force loading of it
				FRAME_PROFILER( "On-DemandBehaviourLoading", gEnv->pSystem, PROFILE_AI);
				const char* szAIBehaviorFileName = GetBehaviorFileName(szNextBehaviorName);
				if (szAIBehaviorFileName)
				{
					//fixme - problem with reloading!!!!
					gEnv->pScriptSystem->ExecuteFile(szAIBehaviorFileName, true, true);
				}

				if (!m_pBehaviorTable->GetValue(szNextBehaviorName, pNextBehavior))
				{
					if (m_pEntity && m_pEntity->GetName())
					{
						gEnv->pAISystem->Warning("<CAIHandler> ", "entity %s failed to change behavior to %s.",
							m_pEntity->GetName(), szNextBehaviorName ? szNextBehaviorName : "<null>");
					}
				}
			}	
*/
		}
	}

	// Finish up with the old behavior.
	if (setFlags != SET_ON_SERILIAZE)
	{
		if (m_pBehavior.GetPtr())
		{
			int noPrevious = 0;
			if (!m_pBehavior->GetValue("NOPREVIOUS", noPrevious))
			{
				m_pPreviousBehavior = m_pBehavior;
			}

			// executing behavior destructor
			CallScript(m_pBehavior, "Destructor", 0, 0, pData);
			gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_BEHAVIORDESTRUCTOR, m_sBehaviorName);

			if (m_pEntity->GetAI())
			{
				IAIRecordable::RecorderEventData recorderEventData(m_sBehaviorName);
				m_pEntity->GetAI()->RecordEvent(IAIRecordable::E_BEHAVIORDESTRUCTOR, &recorderEventData);
			}
		}
	}

	// Switch behavior
	m_sBehaviorName = szNextBehaviorName;
	m_pBehavior = pNextBehavior;

	m_pScriptObject->SetValue("Behaviour", m_pBehavior);

	gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_BEHAVIORSELECTED, m_sBehaviorName);

	if (m_pEntity->GetAI())
	{
		IAIRecordable::RecorderEventData recorderEventData(m_sBehaviorName);
		m_pEntity->GetAI()->RecordEvent(IAIRecordable::E_BEHAVIORSELECTED, &recorderEventData);
	}

	int alertness = 0;
	int exclusive = 0;

	// Initialize the new behavior
	if (m_pBehavior.GetPtr())
	{
		m_pBehavior->GetValue("alertness", alertness);
		m_pBehavior->GetValue("exclusive", exclusive);

		if (setFlags == SET_DELAYED)
		{
			m_bDelayedBehaviorConstructor = true;
			if (pData)
			{
				gEnv->pAISystem->Warning( "<CAIHandler::CallScript> ", "SetBehavior: signal extra data ignored for delayed constructor (PipeUser: '%s'; Behaviour: '%s')",
					m_pEntity->GetName(), szNextBehaviorName);
			}
		}
		else if (setFlags == SET_IMMEDIATE)
		{
			// executing behavior constructor
			IAIRecordable::RecorderEventData recorderEventData(szNextBehaviorName);
			m_pEntity->GetAI()->RecordEvent(IAIRecordable::E_BEHAVIORCONSTRUCTOR, &recorderEventData);

			m_bDelayedBehaviorConstructor = false;
			CallBehaviorConstructor(pData);
		}
	}

	// Update alertness levels
	bool bOnAlert = m_CurrentAlertness == 0 && alertness > 0;
	SetAlertness(alertness, false);

	// Update behavior exclusive flag
	m_CurrentExclusive = exclusive > 0;

	if (setFlags != SET_ON_SERILIAZE)
	{
		if (bOnAlert)
		{
			SEntityEvent event( ENTITY_EVENT_SCRIPT_EVENT );
			event.nParam[0] = (INT_PTR)"OnAlert";
			event.nParam[1] = IEntityClass::EVT_BOOL;
			bool bValue = true;
			event.nParam[2] = (INT_PTR)&bValue;
			m_pEntity->SendEvent( event );
		}
	}
}


//
//-------------------------------------------------------------------------------------------------
void CAIHandler::FindOrLoadBehavior(const char* szBehaviorName, SmartScriptTable& pBehaviorTable)
{
	if (!szBehaviorName || !szBehaviorName[0])
	{
		pBehaviorTable = 0;
		return;
	}

	if (!m_pBehaviorTable.GetPtr())
		return;

	if (!m_pBehaviorTable->GetValue(szBehaviorName, pBehaviorTable))
	{
		//[petar] if behaviour not preloaded then force loading of it
		FRAME_PROFILER( "On-DemandBehaviourLoading", gEnv->pSystem, PROFILE_AI);
		const char* szAIBehaviorFileName = GetBehaviorFileName(szBehaviorName);
		if (szAIBehaviorFileName)
		{
			//fixme - problem with reloading!!!!
			gEnv->pScriptSystem->ExecuteFile(szAIBehaviorFileName, true, true);
		}

		if (!m_pBehaviorTable->GetValue(szBehaviorName, pBehaviorTable))
		{
			if (m_pEntity && m_pEntity->GetName())
			{
				gEnv->pAISystem->Warning("<CAIHandler> ", "entity %s failed to change behavior to %s.",
					m_pEntity->GetName(), szBehaviorName ? szBehaviorName : "<null>");
			}
		}
	}	
}

//
//------------------------------------------------------------------------------
void CAIHandler::CallCharacterConstructor()
{
	// call character constructor
	if (m_pCharacter.GetPtr())
	{
		// execute character constructor
		HSCRIPTFUNCTION pConstructor = 0;
		if (m_pCharacter->GetValue("Constructor", pConstructor))
		{
			gEnv->pScriptSystem->BeginCall(pConstructor);
			gEnv->pScriptSystem->PushFuncParam(m_pCharacter);
			gEnv->pScriptSystem->PushFuncParam(m_pScriptObject);
			gEnv->pScriptSystem->EndCall();
		}
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::CallBehaviorConstructor(const IAISignalExtraData* pData)
{
	if (m_pBehavior.GetPtr())
	{
		gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_BEHAVIORCONSTRUCTOR, m_sBehaviorName);
		CallScript(m_pBehavior, "Constructor", 0, 0, pData);

		const char* szEventToCallName = 0;
		if (m_pScriptObject->GetValue("EventToCall", szEventToCallName) && *szEventToCallName)
		{
			CallScript(m_pBehavior, szEventToCallName);
			m_pScriptObject->SetValue("EventToCall", "");
		}
	}
}

//
//------------------------------------------------------------------------------
const char* CAIHandler::GetBehaviorFileName(const char* szBehaviorName)
{
	const char* szFileName = 0;
	if (m_pBehaviorTableAVAILABLE.GetPtr() && m_pBehaviorTableAVAILABLE->GetValue(szBehaviorName, szFileName))
		return szFileName;
	else if (m_pBehaviorTableINTERNAL.GetPtr() && m_pBehaviorTableINTERNAL->GetValue(szBehaviorName, szFileName))
		return szFileName;
	return 0;
}

//
//------------------------------------------------------------------------------
bool CAIHandler::CallBehaviorOrDefault(const char* signalText, float* value, IAISignalExtraData* pExtraData)
{
	bool bHandled = false;
	HSCRIPTFUNCTION	handlerFunc = 0;

	IAIActor* pAIActor = CastToIAIActorSafe(m_pEntity->GetAI());
	if (pAIActor)
		pAIActor->NotifySignalReceived( signalText, pExtraData );

	if (m_pBehavior.GetPtr())
	{
		bHandled = CallScript(m_pBehavior, signalText, value, NULL, pExtraData);
		if (!bHandled && m_pDefaultBehavior.GetPtr())
			bHandled = CallScript(m_pDefaultBehavior, signalText, value, NULL, pExtraData);
		if (!bHandled && m_pDEFAULTDefaultBehavior.GetPtr())
			bHandled = CallScript(m_pDEFAULTDefaultBehavior, signalText, value, NULL, pExtraData);
	}

	return bHandled;
}

//
//------------------------------------------------------------------------------
bool	CAIHandler::CallScript(IScriptTable* scriptTable, const char* funcName, float* value, IEntity* pSender, const IAISignalExtraData* pData)
{
	FUNCTION_PROFILER( gEnv->pSystem, PROFILE_AI );

	HSCRIPTFUNCTION	functionToCall = 0;

	if (scriptTable)
	{
		bool bFound = scriptTable->GetValue(funcName, functionToCall);
		if (!bFound)
		{
			//try parent behaviours
			const char* szTableBase;
			if (scriptTable->GetValue("Base", szTableBase))
			{
				SmartScriptTable scriptTableBase;
				if (m_pBehaviorTable->GetValue(szTableBase, scriptTableBase))
					return CallScript(scriptTableBase, funcName, value, pSender, pData);
			}
		}

		if (bFound)
		{
			//			string str="Calling behavior >> ";
			//			str+= funcName;
			//		FRAME_PROFILER( "Calling behavior signal",m_pGame->GetSystem(),PROFILE_AI );
			//		FRAME_PROFILER( str.c_str(),m_pGame->GetSystem(),PROFILE_AI );

			// only use strings which are known at compile time...
			// not doing so causes a stack corruption in the frame profiler -- CW
			//		FRAME_PROFILER( funcName,m_pGame->GetSystem(),PROFILE_AI ); 
			//sprintf(m_szSignalName,"AISIGNAL: %s",funcName); 
			FRAME_PROFILER( "AISIGNAL" , gEnv->pSystem, PROFILE_AI );

#ifdef AI_LOG_SIGNALS
			static ICVar* pCVarAILogging = NULL;
			CTimeValue start;
			uint64 pagefaults = 0;
			if(CCryActionCVars::Get().aiLogSignals)
			{
				if ( !pCVarAILogging )
					pCVarAILogging = gEnv->pConsole->GetCVar( "ai_enablewarningserrors" );
				//float start = pCVarAILogging && pCVarAILogging->GetIVal() ? gEnv->pTimer->GetAsyncCurTime() : 0;

				if (pCVarAILogging && pCVarAILogging->GetIVal() )
				{
					start = gEnv->pTimer->GetAsyncTime();
					IMemoryManager::SProcessMemInfo memCounters;
					gEnv->pSystem->GetIMemoryManager()->GetProcessMemInfo( memCounters );
					pagefaults = memCounters.PageFaultCount;
				}
			}
#endif
			gEnv->pScriptSystem->BeginCall(functionToCall);
			gEnv->pScriptSystem->PushFuncParam(scriptTable);					// self
			gEnv->pScriptSystem->PushFuncParam(m_pScriptObject);

			if (pSender && pSender->GetScriptTable())
				gEnv->pScriptSystem->PushFuncParam(pSender->GetScriptTable());
			else if (value)
				gEnv->pScriptSystem->PushFuncParam(*value);

			if (pData)
			{
				SmartScriptTable pScriptEData(gEnv->pScriptSystem);
				CScriptVector oVec(gEnv->pScriptSystem);
				CScriptVector oVec2(gEnv->pScriptSystem);

				pScriptEData->SetValue("id",pData->nID);
				pScriptEData->SetValue("fValue",pData->fValue);
				pScriptEData->SetValue("iValue",pData->iValue);
				pScriptEData->SetValue("iValue2",pData->iValue2);
				oVec.Set(pData->point);
				oVec2.Set(pData->point2);
				pScriptEData->SetValue("point",oVec);
				pScriptEData->SetValue("point2",oVec2);
				if (*pData->GetObjectName())
					pScriptEData->SetValue("ObjectName", pData->GetObjectName());
				gEnv->pScriptSystem->PushFuncParam(*pScriptEData);
			}

			gEnv->pScriptSystem->EndCall();

#ifdef AI_LOG_SIGNALS
			if(CCryActionCVars::Get().aiLogSignals)
			{
				if (pCVarAILogging && pCVarAILogging->GetIVal())
				{
					start = gEnv->pTimer->GetAsyncTime() - start;
					IMemoryManager::SProcessMemInfo memCounters;
					gEnv->pSystem->GetIMemoryManager()->GetProcessMemInfo( memCounters );
					pagefaults = memCounters.PageFaultCount - pagefaults;

				}
				if (start.GetMilliSeconds() > CCryActionCVars::Get().aiMaxSignalDuration)
				{
					char* behaviour = "";
					scriptTable->GetValue( "Name", behaviour );
					gEnv->pAISystem->Warning( "<CAIHandler::CallScript> ", "Handling AISIGNAL '%s' takes %g ms! (PipeUser: '%s'; Behaviour: '%s')",
						funcName, start.GetMilliSeconds(), m_pEntity->GetName(), behaviour );

					char buffer[256];
					sprintf(buffer,"%s.%s,%g,%d,%s",behaviour,funcName,start.GetMilliSeconds(),(int)pagefaults,m_pEntity->GetName());
					gEnv->pAISystem->Record(m_pEntity->GetAI(), IAIRecordable::E_SIGNALEXECUTEDWARNING, buffer);
				}
			}
#endif

			return true;
		}
	}
	return false;
}


//
//------------------------------------------------------------------------------
void	CAIHandler::Release( )
{
	m_curActorTargetStartedQueryID = &m_actorTargetStartedQueryID;
	m_curActorTargetEndQueryID = &m_actorTargetEndQueryID;

	if ( m_pAGState )
	{
		m_pAGState->RemoveListener( this );
		m_pAGState = NULL;
		m_actorTargetStartedQueryID = 0;
		m_actorTargetEndQueryID = 0;
		m_eActorTargetPhase = eATP_None;
	}
}

//
//------------------------------------------------------------------------------
//IScriptTable* CAIHandler::GetMostLikelyTable( IScriptTable* table )
bool CAIHandler::GetMostLikelyTable( IScriptTable* table, SmartScriptTable& destTable)
{
	int numAnims = table->Count();
	if (!numAnims)
		return false;

	int rangeMin = 0;
	int rangeMax = 0;
	int selAnim = -1;

	int usedCount = 0;
	int maxProb = 0;
	int totalProb = 0;

	// Check the available animations
//	CryLog("GetMostLikelyTable:");
	for (int i = 0; i < numAnims; ++i)
	{
		table->GetAt(i+1, destTable);
		float fProb = 0;
		destTable->GetValue("PROBABILITY",fProb);
//		CryLog("%d - %d", i+1, (int)fProb);
		totalProb += (int)fProb;

		int isUsed = 0;
		if (destTable->GetValue("USED", isUsed))
		{
			//			CryLog("%d - USED", i+1);
			usedCount++;
			continue;
		}

		maxProb += (int)fProb;
	}

	if (totalProb < 1000)
		maxProb += 1000 - totalProb;

	// If all anims has been used already, reset.
	if (usedCount == numAnims)
	{
		for (int i = 0; i < numAnims; ++i)
		{
			table->GetAt(i+1, destTable);
			destTable->SetToNull("USED");
		}
		maxProb = 1000;
	}

	// Choose among the possible choices
	int probability = cry_rand() % maxProb;

	for (int i = 0; i < numAnims; ++i)
	{
		table->GetAt(i+1, destTable);

		// Skip already used ones.
		int isUsed = 0;
		if (destTable->GetValue("USED", isUsed))
			continue;

		float fProb = 0;
		destTable->GetValue("PROBABILITY", fProb);
		rangeMin = rangeMax;
		rangeMax += (int)fProb;

		if (probability >= rangeMin && probability < rangeMax)
		{
			selAnim = i+1;
			break;
		}
	}

	if (selAnim == -1)
	{
//		CryLog(">> not found!");
		return false;
	}

//	CryLog(">> found=%d", selAnim);

	table->GetAt(selAnim, destTable);
	destTable->SetValue("USED", 1);

	return true;
}

void CAIHandler::SetAlertness( int value, bool triggerEvent /*=false*/ )
{
	if ( value == -1 || m_CurrentAlertness == value )
		return;

	bool switchToIdle = m_CurrentAlertness > 0 && value == 0;
	bool switchToWeaponAlerted = m_CurrentAlertness == 0 && value > 0;

	m_CurrentAlertness = value;

	if ( triggerEvent && switchToWeaponAlerted )
	{
		SEntityEvent event( ENTITY_EVENT_SCRIPT_EVENT );
		event.nParam[0] = (INT_PTR)"OnAlert";
		event.nParam[1] = IEntityClass::EVT_BOOL;
		bool bValue = true;
		event.nParam[2] = (INT_PTR)&bValue;
		m_pEntity->SendEvent( event );
	}

	CAIFaceManager::e_ExpressionEvent	expression(CAIFaceManager::EE_NONE);
	const char* sStates = "";
	switch ( m_CurrentAlertness )
	{
	case 0: // Idle
		sStates = "Idle-Alerted,Combat";
		expression = CAIFaceManager::EE_IDLE;
		break;
	case 1: // Alerted
		sStates = "Alerted-Idle,Combat";
		expression = CAIFaceManager::EE_ALERT;
		break;
	case 2: // Combat
		sStates = "Combat-Idle,Alerted";
		expression = CAIFaceManager::EE_COMBAT;
		break;
	}
	MakeFace(expression);

	if ( sStates )
		gEnv->pAISystem->ModifySmartObjectStates( m_pEntity, sStates );

	if ( switchToWeaponAlerted )
	{
		// if the current body stance is relaxed force it to be combat
		if ( IAnimationGraphState* pAGState = GetAGState() )
		{
			IAnimationGraph::InputID idStance = pAGState->GetInputId( "Stance" );
			char stance[64] = "";
			pAGState->GetInput( idStance, stance );
			if ( !strcmp(stance, "relaxed") )
				pAGState->SetInput( idStance, "combat" );
		}
		IAIActor* pAIActor = CastToIAIActorSafe(m_pEntity->GetAI());
		if ( pAIActor && pAIActor->GetState()->bodystate == BODYPOS_RELAX )
			pAIActor->GetState()->bodystate = BODYPOS_STAND;
	}

	if ( switchToIdle || switchToWeaponAlerted )
		UpdateWeaponAlertness();
}

void CAIHandler::UpdateWeaponAlertness()
{
	IAIActor* pAIActor = CastToIAIActorSafe(m_pEntity->GetAI());
	IPipeUser* pPipeUser = CastToIPipeUserSafe(m_pEntity->GetAI());
	if (pPipeUser)
	{
		IAIObject* pTarget = pPipeUser->GetAttentionTarget();
		bool weaponAlerted = pAIActor->GetState()->forceWeaponAlertness || (m_CurrentAlertness > 0 && pTarget != NULL);

		IAnimationGraphState* pAGState = GetAGState();
		if ( !pAGState )
			return;

		//pAGState->SetInput( "WeaponAlerted", weaponAlerted );
		pAGState->SetInput( "WeaponAlerted", 0 );

/*		IAnimationGraph::InputID idAction = pAGState->GetInputId( "Action" );
		char action[64] = "";
		pAGState->GetInput( idAction, action );

		if ( weaponAlerted && !strcmp(action,"idle") )
		{
			//if ( !pAGState->SetInput(idAction, "weaponAlerted") )
				pAGState->SetInput( idAction, "idle" );
		}
		else if ( !weaponAlerted && !strcmp(action,"weaponAlerted") )
		{
			pAGState->SetInput( idAction, "idle" );
		}*/
	}
}

//
//------------------------------------------------------------------------------
IAnimationGraphState* CAIHandler::GetAGState()
{
	if ( m_pAGState )
		return m_pAGState;

	IActorSystem* pASystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	if ( !pASystem )
		return NULL;
	IActor* pActor = pASystem->GetActor( m_pEntity->GetId() );
	if ( !pActor )
		return NULL;
	m_pAGState = pActor->GetAnimationGraphState();
	if ( !m_pAGState )
		return NULL;

	m_pAGState->QueryChangeInput( "Action", &m_changeActionInputQueryId );
	m_pAGState->QueryChangeInput( "Signal", &m_changeSignalInputQueryId );


	m_pAGState->AddListener( "AIHandler", this );
	return m_pAGState;
}

bool CAIHandler::IsAnimationBlockingMovement()
{
	if(ICharacterInstance*	pChar = m_pEntity->GetCharacter(0))
		if(ISkeletonAnim*	pSkeletonAnim = pChar->GetISkeletonAnim())
			return pSkeletonAnim->GetUserData(eAGUD_MovementControlMethodH) == eMCM_Animation && (m_playingSignalAnimation || m_playingActionAnimation);
	return false;
}

//
//------------------------------------------------------------------------------
void CAIHandler::HandleExactPositioning(SOBJECTSTATE *pStates, CMovementRequest& mr)
{
	if (!m_enabledTargetPointVerifier)
	{
		IAnimationGraphState * pAGState = GetAGState();
		if(pAGState)
		{
			pAGState->SetTargetPointVerifier(this);
			m_enabledTargetPointVerifier = true;
		}
	}

	// The AIsystem is reported back the same phase that we handle here.
	pStates->curActorTargetPhase = m_eActorTargetPhase;

	if(m_actorTargetId != pStates->actorTargetReq.id)
	{
		// Make sure everything is installed and ready to go.
		IAnimationGraphState*	pAGState = GetAGState();
		assert(pAGState);

		const SAnimationTarget*	animTgt = pAGState->GetAnimationTarget();

		if((!animTgt || !animTgt->activated) &&
			pStates->curActorTargetPhase != eATP_Started &&
			pStates->curActorTargetPhase != eATP_Playing &&
			pStates->curActorTargetPhase != eATP_StartedAndFinished &&
			pStates->curActorTargetPhase != eATP_Finished)
		{
			if(pStates->actorTargetReq.id == 0)
			{
				m_eActorTargetPhase = eATP_None;
				*m_curActorTargetStartedQueryID = 0;
				*m_curActorTargetEndQueryID = 0;
				mr.ClearActorTarget();
			}
			else
			{
				m_eActorTargetPhase = eATP_Waiting;

				// if the requester passes us IDs use them instead of the local ones.
				if(pStates->actorTargetReq.pQueryStart && pStates->actorTargetReq.pQueryEnd)
				{
					m_curActorTargetStartedQueryID = pStates->actorTargetReq.pQueryStart;
					m_curActorTargetEndQueryID = pStates->actorTargetReq.pQueryEnd;
				}
				else
				{
					m_curActorTargetStartedQueryID = &m_actorTargetStartedQueryID;
					m_curActorTargetEndQueryID = &m_actorTargetEndQueryID;
				}

				*m_curActorTargetStartedQueryID = 0;
				*m_curActorTargetEndQueryID = 0;

				SActorTargetParams actorTarget;
				actorTarget.location = pStates->actorTargetReq.animLocation;
				actorTarget.locationRadius = pStates->actorTargetReq.locationRadius;
				actorTarget.startRadius = pStates->actorTargetReq.startRadius;
				actorTarget.direction = pStates->actorTargetReq.animDirection.GetNormalizedSafe(ZERO);
				actorTarget.directionRadius = pStates->actorTargetReq.directionRadius;
				if (actorTarget.direction.IsZero() )
				{
					actorTarget.direction = FORWARD_DIRECTION;
					actorTarget.directionRadius = gf_PI;
				}
				actorTarget.speed = -1;
				actorTarget.animation = pStates->actorTargetReq.animation;
				actorTarget.vehicleName = pStates->actorTargetReq.vehicleName;
				actorTarget.vehicleSeat = pStates->actorTargetReq.vehicleSeat;
				actorTarget.signalAnimation = pStates->actorTargetReq.signalAnimation;
				actorTarget.projectEnd = actorTarget.vehicleName.empty() && pStates->actorTargetReq.projectEndPoint;
				actorTarget.navSO = pStates->actorTargetReq.lowerPrecision;

				actorTarget.stance = pStates->actorTargetReq.stance;
				actorTarget.pQueryStart = m_curActorTargetStartedQueryID;
				actorTarget.pQueryEnd = m_curActorTargetEndQueryID;

				mr.SetActorTarget(actorTarget);
			}

			m_vAnimationTargetPosition.zero();
			pStates->curActorTargetPhase = m_eActorTargetPhase;
			m_actorTargetId = pStates->actorTargetReq.id;
			return;
		}
	}

	bool	clearTarget = false;

	if ( m_eActorTargetPhase == eATP_Waiting )
	{
//		if(pStates->actorTargetPhase == eATP_Waiting)
		{
			IAnimationGraphState*	pAGState = GetAGState();
			assert( pAGState );
			const SAnimationTarget*	animTgt = pAGState->GetAnimationTarget();

			// The exact positioning target should be the same we have requested.
			if(false && animTgt && animTgt->doingSomething)
				assert(Distance::Point_Point(animTgt->position, pStates->actorTargetReq.animLocation) < 0.001f);

			if ( !animTgt )
			{
				// strange but sometimes this is possible.
				// exact positioning request was made, but cleared after some time without any notification :-(
//				pStates->actorTargetPhase = eATP_Error;
				m_eActorTargetPhase = eATP_Error;
				pStates->curActorTargetPhase = eATP_Error;
//				m_actorTargetStartedQueryID = 0;
				//			m_bAnimationStarted = false;
			}
			else if ( !pStates->remainingPath.size() )
			{
				// TODO: investigate how this happened!
				//	pStates->eActorTargetPhase = eATP_Error;
				//	m_eActorTargetPhase = eATP_None;
				//	m_actorTargetStartedQueryID = 0;
				//	m_bAnimationStarted = false;
			}
		}
	}
	else if ( m_eActorTargetPhase == eATP_Starting )
	{
		// Waiting for starting position.
//		assert( pStates->actorTargetPhase == eATP_Waiting );
//		if(m_eActorTargetPhase != eATP_None)
//			m_eActorTargetPhase = eATP_Waiting;
	}
	else if ( m_eActorTargetPhase == eATP_Started )
	{
//		assert( pStates->eActorTargetPhase == eATP_Starting );
		// Wait until we get a animation target back too.
		m_eActorTargetPhase = eATP_Playing;
//		m_actorTargetStartedQueryID = 0;
	}
	else if (m_eActorTargetPhase == eATP_Playing)
	{
		// empty
	}
	else if ( m_eActorTargetPhase == eATP_StartedAndFinished )
	{
//		m_eActorTargetPhase = eATP_None;
//		if(pStates->actorTargetPhase != eATP_None)
//			pStates->actorTargetPhase = eATP_StartedAndFinished;
//		m_actorTargetStartedQueryID = 0;
//		m_actorTargetEndQueryID = 0;
		m_eActorTargetPhase = eATP_None;
	}
	else if ( m_eActorTargetPhase == eATP_Finished )
	{
//		if(pStates->actorTargetPhase != eATP_None)
//			pStates->actorTargetPhase = eATP_Finished;
		m_eActorTargetPhase = eATP_None;
//		m_actorTargetEndQueryID = 0;
	}
	else if ( m_eActorTargetPhase == eATP_Error )
	{
		if(pStates->actorTargetReq.id == 0)
			m_eActorTargetPhase = eATP_None;
//		pStates->actorTargetPhase = eATP_Error;
//		m_eActorTargetPhase = eATP_None;
//		m_actorTargetStartedQueryID = 0;
//		m_actorTargetEndQueryID = 0;
//		m_curActorTargetStartedQueryID = &m_actorTargetStartedQueryID;
//		m_curActorTargetEndQueryID = &m_actorTargetEndQueryID;
	}

/*	if(m_eActorTargetPhase != pStates->curActorTargetPhase)
		pStates->curActorTargetPhaseTransition = m_eActorTargetPhase;
	else
		pStates->curActorTargetPhaseTransition = eATP_None;

	pStates->curActorTargetPhase = m_eActorTargetPhase;*/
	pStates->curActorTargetFinishPos = m_vAnimationTargetPosition;
	
	if ( m_eActorTargetPhase == eATP_None )
	{
		// make sure there's nothing hanging around in the system
		mr.ClearActorTarget();
		m_vAnimationTargetPosition.zero();
		m_actorTargetStartedQueryID = 0;
		m_actorTargetEndQueryID = 0;
	}
}

void CAIHandler::QueryComplete( TAnimationGraphQueryID queryID, bool succeeded )
{
	if ( queryID == m_ActionQueryID )
	{
		if ( !m_sQueriedActionAnimation.empty() )
		{
			m_setStartedActionAnimations.insert( m_sQueriedActionAnimation );
			m_sQueriedActionAnimation.clear();
		}
	}
	else if ( queryID == m_SignalQueryID )
	{
		if ( m_bSignaledAnimationStarted || !succeeded )
		{
			if ( !m_sQueriedSignalAnimation.empty() || !succeeded )
			{
				m_setPlayedSignalAnimations.insert( m_sQueriedSignalAnimation );
				m_sQueriedSignalAnimation.clear();
			}
			m_bSignaledAnimationStarted = false;
		}
		else if ( !m_sQueriedSignalAnimation.empty() )
		{
			GetAGState()->QueryLeaveState( &m_SignalQueryID );
			m_bSignaledAnimationStarted = true;
		}
	}
	else if ( queryID == m_changeActionInputQueryId )
	{
		m_bOwnsActionInput = false;

		IAnimationGraph::InputID idAction = m_pAGState->GetInputId( "Action" );
		char value[64] = "\0";
		string newValue;
		m_pAGState->GetInput( idAction, value );

		bool defaultValue = m_pAGState->IsDefaultInputValue( idAction ); // _stricmp(value,"idle") == 0 || _stricmp(value,"weaponAlerted") == 0 || _stricmp(value,"<<not set>>") == 0;
		if ( !defaultValue )
			newValue = value;

		m_playingActionAnimation = !defaultValue;

		if(m_playingActionAnimation)
			m_currentActionAnimName = value;
		else
			m_currentActionAnimName.clear();

		// update smart object state if changed
		if ( m_sAGActionSOAutoState != newValue )
		{
			if ( !m_sAGActionSOAutoState.empty() )
				gEnv->pAISystem->RemoveSmartObjectState( m_pEntity, m_sAGActionSOAutoState );
			m_sAGActionSOAutoState = newValue;
			if ( !m_sAGActionSOAutoState.empty() )
				gEnv->pAISystem->AddSmartObjectState( m_pEntity, m_sAGActionSOAutoState );
		}

		// query the next change
		m_pAGState->QueryChangeInput( idAction, &m_changeActionInputQueryId );
	}
	else if ( queryID == m_changeSignalInputQueryId )
	{
		IAnimationGraph::InputID idSignal = m_pAGState->GetInputId( "Signal" );
		char value[64] = "\0";
		m_pAGState->GetInput( idSignal, value );

		bool	defaultValue = m_pAGState->IsDefaultInputValue( idSignal ); // _stricmp(value,"none") == 0 || _stricmp(value,"<<not set>>") == 0;

		m_playingSignalAnimation = !defaultValue;
		if(m_playingSignalAnimation)
			m_currentSignalAnimName = value;
		else
			m_currentSignalAnimName.clear();

		// query the next change
		m_pAGState->QueryChangeInput( idSignal, &m_changeSignalInputQueryId );
	}
	else if ( queryID == *m_curActorTargetStartedQueryID )
	{
		if ( succeeded )
		{
			if( m_eActorTargetPhase == eATP_Waiting )
			{
				m_eActorTargetPhase = eATP_Starting;
				m_vAnimationTargetPosition.zero();
				m_qAnimationTargetOrientation.SetIdentity();
			}
			else
			{
				m_eActorTargetPhase = eATP_Error;
			}
		}
		else
		{
			m_eActorTargetPhase = eATP_Error;
		}
		//		*m_curActorTargetStartedQueryID = 0;
	}
	else if ( queryID == *m_curActorTargetEndQueryID )
	{
		if ( succeeded )
		{
			// It is possible the the animation starts and stops before the AI gets a change to update.
			if ( m_eActorTargetPhase == eATP_Starting || m_eActorTargetPhase == eATP_Started )
			{
				m_eActorTargetPhase = eATP_StartedAndFinished;
			}
			else if ( m_eActorTargetPhase == eATP_Playing)
			{
				m_eActorTargetPhase = eATP_Finished;
			}
			else
			{
				assert(0);
				m_eActorTargetPhase = eATP_Error;
			}
		}
		else
		{
			m_eActorTargetPhase = eATP_Error;
		}
		//		*m_curActorTargetEndQueryID = 0;
	}
}

void CAIHandler::DestroyedState( IAnimationGraphState* pAGState )
{
	if ( pAGState == m_pAGState )
		m_pAGState = NULL;
}


//
//----------------------------------------------------------------------------------------------------------
ETriState CAIHandler::CanTargetPointBeReached(CTargetPointRequest &request) const
{
	IAIObject* pAI = m_pEntity->GetAI();
	return pAI ? pAI->CanTargetPointBeReached(request) : eTS_false;
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIHandler::UseTargetPointRequest(const CTargetPointRequest &request)
{
	IAIObject* pAI = m_pEntity->GetAI();
	return pAI ? pAI->UseTargetPointRequest(request) : false;
}

//
//----------------------------------------------------------------------------------------------------------
void CAIHandler::NotifyFinishPoint( const Vec3& pt )
{
	m_vAnimationTargetPosition = pt;
	if(m_eActorTargetPhase == eATP_Starting || m_eActorTargetPhase == eATP_Waiting)
		m_eActorTargetPhase = eATP_Started;
}

//
//------------------------------------------------------------------------------
void CAIHandler::NotifyAllPointsNotReachable()
{
	m_eActorTargetPhase = eATP_Error;
	m_vAnimationTargetPosition.zero();
	//	*m_curActorTargetStartedQueryID = 0;
	//	*m_curActorTargetEndQueryID = 0;
}

//
//------------------------------------------------------------------------------
bool CAIHandler::SetAGInput(EAIAGInput input, const char* value )
{
	if ( m_eActorTargetPhase != eATP_None )
		return false;

	IAnimationGraphState* pAGState = GetAGState();
	if ( !pAGState )
		return false;

	if ( input == AIAG_ACTION )
	{
		if ( m_sQueriedActionAnimation != value )
		{
			if ( !m_sQueriedActionAnimation.empty() )
			{
				// mark previous animation as started to unblock code waiting for it
				m_setStartedActionAnimations.insert( m_sQueriedActionAnimation );
			}
		}
		m_sQueriedActionAnimation = value;
		bool result = pAGState->SetInput( "Action", value, &m_ActionQueryID );
		m_bOwnsActionInput |= result;
		if ( result == false )
			m_sQueriedActionAnimation.clear();

		SetStrings::iterator it = m_setStartedActionAnimations.find( m_sQueriedActionAnimation );
		if ( it != m_setStartedActionAnimations.end() )
			m_setStartedActionAnimations.erase( it );

		return result;
	}
	else if ( input == AIAG_SIGNAL )
	{
		if ( m_sQueriedSignalAnimation != value )
		{
			if ( !m_sQueriedSignalAnimation.empty() )
			{
				// mark previous animation as played to unblock code waiting for it
				m_setPlayedSignalAnimations.insert( m_sQueriedSignalAnimation );
			}
		}
		else
			return true;
		m_sQueriedSignalAnimation = value;
		m_bSignaledAnimationStarted = false;
		m_SignalQueryID = 0;

		SetStrings::iterator it = m_setPlayedSignalAnimations.find( value );
		if ( it != m_setPlayedSignalAnimations.end() )
			m_setPlayedSignalAnimations.erase( it );

		if ( pAGState->SetInput( "Signal", value, &m_SignalQueryID ) == true )
			return true;
		else
			m_sQueriedSignalAnimation.clear();
	}

	return false;
}

bool CAIHandler::ResetAGInput(EAIAGInput input)
{
	IAnimationGraphState* pAGState = GetAGState();
	if ( !pAGState )
		return false;

	if ( input == AIAG_ACTION )
	{
		// depending on alertness level override "idle" "Action" with "weaponAlerted"
//		if ( m_CurrentAlertness > 0 )
//			return pAGState->SetInput( "Action", "weaponAlerted" );
//		else
//			return pAGState->SetInput( "Action", "idle"	);

		//if ( !m_bOwnsActionInput )
		//	return true;

		m_bOwnsActionInput = false;
		return pAGState->SetInput( "Action", "idle" );
	}

	return true;
}

bool CAIHandler::IsSignalAnimationPlayed( const char* value )
{
	SetStrings::iterator it = m_setPlayedSignalAnimations.find( value );
	if ( it != m_setPlayedSignalAnimations.end() )
	{
		m_setPlayedSignalAnimations.erase( it );
		return true;
	}
	return m_sQueriedSignalAnimation != value;
}

bool CAIHandler::IsActionAnimationStarted( const char* value )
{
	SetStrings::iterator it = m_setStartedActionAnimations.find( value );
	if ( it != m_setStartedActionAnimations.end() )
	{
		m_setStartedActionAnimations.erase( it );
		return true;
	}
	return m_sQueriedActionAnimation != value;
}

bool CAIHandler::IsAllowedToPlayReadabilityAnimations()
{
	if(m_eActorTargetPhase != eATP_None)
		return false;

	IPipeUser* pPipeUser = CastToIPipeUserSafe(m_pEntity->GetAI());
	if (!pPipeUser)
		return false;

	IAnimationGraphState* pAGState = GetAGState();
	if ( !pAGState )
		return true;

	IAnimationGraph::InputID idAction = pAGState->GetInputId( "Action" );
	return pAGState->IsDefaultInputValue( idAction );
}

//
//------------------------------------------------------------------------------
bool CAIHandler::DoReadibilityPack(const char* text, int readabilityType, bool soundOnly, bool stopPreviousSound)
{
	FUNCTION_PROFILER( gEnv->pSystem, PROFILE_AI );

IEntity* pNullEntity = NULL;
	if(gEnv->pAISystem->SmartObjectEvent(text,m_pEntity,pNullEntity))
		return true;
	bool	hasAnimation = false;
	if (!soundOnly)
		hasAnimation = DoReadibilityAnimation(text, readabilityType);
	bool	hasSound = s_ReadabilityManager.PlayReadabilitySound(this, text, readabilityType, stopPreviousSound);

	if(hasSound && readabilityType != SIGNALFILTER_READIBILITYRESPONSE)
	{
		// Loop through all puppets in the group and check if any of them can respond to this readability
		// and choose nearest one to respond.
		IAIObject* actor = m_pEntity->GetAI();
		if (actor && actor->IsEnabled())
		{
			IAIActor* pAIActor = actor->CastToIAIActor();
			int groupId = actor->GetGroupId();
			int	count = gEnv->pAISystem->GetGroupCount(groupId, IAISystem::GROUP_ENABLED, AIOBJECT_PUPPET);
			float	nearestDist = FLT_MAX;
			IAIObject*	nearestInGroup = 0;
			for (int i = 0; i < count; ++i)
			{
				IAIObject* responder = gEnv->pAISystem->GetGroupMember(groupId, i, IAISystem::GROUP_ENABLED, AIOBJECT_PUPPET);
				// Skip self and puppets without proxies
				if(responder == actor || !responder->GetProxy()) continue;
				CAIProxy*	proxy = (CAIProxy*)responder->GetProxy();
				if(!proxy->GetAIHandler()) continue;
				// Skip puppets which cannot reply.
				if(!s_ReadabilityManager.HasResponseSound(proxy->GetAIHandler(), text)) continue;
				// Skip if currently playing a sound.
				if(proxy->GetAIHandler()->m_ReadibilitySoundID != INVALID_SOUNDID) continue;
				// Choose nearest
				float	d = Distance::Point_PointSq(actor->GetPos(), responder->GetPos());
				if(d < nearestDist)
				{
					nearestDist = d;
					nearestInGroup = responder;
				}
			}
			// Send response request.
			if(nearestInGroup)
			{
				IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
				pData->iValue = 0;		// Default Priority.
				pData->fValue = Random(2.5f, 4.0f);	// Delay
				gEnv->pAISystem->SendSignal(SIGNALFILTER_READIBILITYRESPONSE, 1, text, nearestInGroup, pData);
			}
		}
	}

	return hasSound || hasAnimation;
}

//
//------------------------------------------------------------------------------
bool	CAIHandler::DoReadibilityAnimation(const char* text, int readabilityType)
{
	if (!IsAllowedToPlayReadabilityAnimations())
		return false;

	if (m_pAnimationPackTable.GetPtr())
	{
		SmartScriptTable	pAnimationDirective(gEnv->pScriptSystem, true);	
		if (m_pAnimationPackTable->GetValue(text, pAnimationDirective))
		{
			SmartScriptTable pMostLikelyTable;
			if (GetMostLikelyTable(pAnimationDirective, pMostLikelyTable))
			{
				const char* aniName = 0;
				pMostLikelyTable->GetValue( "animationName", aniName );
				if (aniName != 0 && aniName[0] != '\0')
				{
					SetAGInput(AIAG_SIGNAL, aniName);

					// HACK/TODO!
					// This should really handled using a callback from animation graph. This is temporary hack to things going.
					SAIEVENT sev;
					sev.fThreat = 0.654321f;
					m_pEntity->GetAI()->Event(AIEVENT_ONBODYSENSOR, &sev);
				}


	//todo - ani length needed here
/*
				IPipeUser *puppet;
				ICharacterInstance *pCharacter = m_pEntity->GetCharacter(0);// Using 0 as character slot
				if(m_pEntity->GetAI()->CanBeConvertedTo(AIOBJECT_PIPEUSER, (void**)&puppet))
				{
//					IGoalPipe *pipe=gEnv->pAISystem->CreateGoalPipe( "special_pipe_anipack_wait" );
//					GoalParameters	par;
//					par.fValue = pCharacter ? pCharacter->GetIAnimationSet()->GetLength(aniName) : 0;
//					pipe->PushGoal( "timeout", true, false, par );

					SAIEVENT sev;
					sev.fInterest = pCharacter ? pCharacter->GetIAnimationSet()->GetLength(aniName) : 0;
					m_pEntity->GetAI()->Event(AIEVENT_ONBODYSENSOR,&sev);
//					puppet->InsertSubPipe(0,"special_pipe_anipack_wait");
				}
*/
			}
		}
	}
	return false;
}

//
//------------------------------------------------------------------------------
void CAIHandler::TestReadabilityPack(bool start, const char* szReadability)
{
	if(start)
	{
		// Start testing
		gEnv->pAISystem->LogComment("<TestReadabilityPack> ", "Start readability test for '%s'", m_pEntity->GetName());		
		if(szReadability)
		{
			// Specific readability
			s_ReadabilityManager.PrepareReadabilityPackTest(this, m_readabilityTestIter, szReadability);
			NextReadabilityTest();
		}
		else
		{
			// All sounds
			s_ReadabilityManager.PrepareReadabilityPackTest(this, m_readabilityTestIter);
			NextReadabilityTest();
		}
	}
	else
	{
		// Stop testing.
		gEnv->pAISystem->LogComment("<TestReadabilityPack> ", "Stop readability test for '%s'", m_pEntity->GetName());		
		m_readabilityTestIter.idx = -1;
		m_readabilityTestIter.sounds.clear();
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::NextReadabilityTest()
{
	CAIReadabilityManager::ETestResult	res;
	// Try until we succeed.
	do 
	{
		res = s_ReadabilityManager.TestReadabilityPack(this, m_readabilityTestIter);
	}
	while(res == CAIReadabilityManager::TEST_FAILED);

	// If done, reset iterator.
	if(res == CAIReadabilityManager::TEST_DONE)
		m_readabilityTestIter.Reset();
}

//
//------------------------------------------------------------------------------
void CAIHandler::OnSoundEvent( ESoundCallbackEvent event, ISound *pSound )
{
	if( event == SOUND_EVENT_ON_STOP )
	{
		m_bSoundFinished = true;
		m_ReadibilitySoundID = INVALID_SOUNDID;
		pSound->RemoveEventListener(this);

		// Queue new test
		if(m_readabilityTestIter.idx != -1)
			NextReadabilityTest();
	}
}


//
//------------------------------------------------------------------------------
bool CAIHandler::FindOrLoadTable(IScriptTable* pGlobalTable, const char* szTableName, SmartScriptTable& tableOut)
{
	if (pGlobalTable->GetValue(szTableName, tableOut))
		return true;

	SmartScriptTable	pAvailableTable;
	if (!pGlobalTable->GetValue("AVAILABLE", pAvailableTable))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "AVAILABLE table not found. Aborting loading table");
		return false;
	}

	const char* szFileName = 0;
	if (!pAvailableTable->GetValue(szTableName, szFileName))
	{
		SmartScriptTable	pInternalTable;
		if(	!pGlobalTable->GetValue("INTERNAL",pInternalTable) ||
				!pInternalTable->GetValue(szTableName, szFileName))
		{
			return false;
		}
	}

	if (gEnv->pScriptSystem->ExecuteFile(szFileName, true, false))
	{
		if (pGlobalTable->GetValue(szTableName, tableOut))
			return true;
	}

	// could not load script for character
	gEnv->pAISystem->Warning("<CAIHandler> ", "can't load script for character [%s]. Entity %s", szFileName, m_pEntity->GetName());

	return false;
}

//
//------------------------------------------------------------------------------
void CAIHandler::OnDialogLoaded(struct ILipSync *pLipSync)
{
}

//
//----------------------------------------------------------------------------------------------------------
void CAIHandler::OnDialogFailed(struct ILipSync *pLipSync)
{
}

//
//----------------------------------------------------------------------------------------------------------
void CAIHandler::Serialize( TSerialize ser )
{
#ifdef SP_DEMO
	const char *pLevelName = CCryAction::GetCryAction()->GetLevelName();
	string sLevel=string('i')+string('s')+string('l')+string('a')+string('n')+string('d');
	if (!strstr(pLevelName,sLevel.c_str()))
		return;
#endif

	ser.Value("m_sBehaviorName", m_sBehaviorName);
	ser.Value("m_sCharacterName", m_sCharacterName);
	// Must be serialized before the character/behavior is set below.
	ser.Value("m_CurrentAlertness", m_CurrentAlertness);
	ser.Value("m_bDelayedCharacterConstructor", m_bDelayedCharacterConstructor);
	ser.Value("m_bDelayedBehaviorConstructor", m_bDelayedBehaviorConstructor);
	ser.Value("m_CurrentExclusive", m_CurrentExclusive);
	ser.Value("m_ReadibilitySoundID", m_ReadibilitySoundID);
	ser.Value("m_bSoundFinished", m_bSoundFinished);

	string prevBehaviorName;
	if (!ser.IsReading() && m_pPreviousBehavior.GetPtr())
	{
		char* pBehaviorName = "";
		m_pPreviousBehavior->GetValue("Name", pBehaviorName);
		prevBehaviorName = pBehaviorName;
	}
	ser.Value("prevBehaviorName", prevBehaviorName);
	// serialize behavior
	if (ser.IsReading())
	{
		FindOrLoadBehavior(prevBehaviorName.c_str(), m_pPreviousBehavior);
		SetBehavior(m_sBehaviorName.c_str(), 0, SET_ON_SERILIAZE);
		SetCharacter(m_sCharacterName.c_str(), SET_ON_SERILIAZE);
	}

	SerializeScriptAI(ser);

	IAnimationGraphState* pAGState = GetAGState();
	if (ser.IsReading())
	{
		// TODO: Serialize the face manager instead of calling Reset().
		m_FaceManager.Reset();

		ResetAnimationData();
		m_changeActionInputQueryId = m_changeSignalInputQueryId = 0;
		if (pAGState)
		{
			pAGState->QueryChangeInput("Action", &m_changeActionInputQueryId);
			pAGState->QueryChangeInput("Signal", &m_changeSignalInputQueryId);
		}
	}

	ser.EnumValue( "m_eActorTargetPhase", m_eActorTargetPhase, eATP_None, eATP_Error );
	ser.Value( "m_actorTargetId", m_actorTargetId );
	ser.Value( "m_vAnimationTargetPosition", m_vAnimationTargetPosition );
	if ( ser.IsReading() )
	{
		switch ( m_eActorTargetPhase )
		{
		case eATP_Waiting:
			// exact positioning animation has been requested but not started yet.
			// since it isn't serialized in the animation graph we have to request it again
			m_eActorTargetPhase = eATP_None;
			m_actorTargetId = 0;
			break;
		case eATP_Starting:
		case eATP_Started:
		case eATP_Playing:
			// exact positioning animation has been started and playing.
			// since it isn't serialized in the animation graph we need to know when animations is done.
			// TODO: try to find a better way to track the progress of animation.
			// TODO: make this work with vehicles.
			if ( pAGState )
			{
				m_curActorTargetEndQueryID = &m_actorTargetEndQueryID;
				pAGState->QueryLeaveState( m_curActorTargetEndQueryID );
			}
			break;
		}
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::SerializeScriptAI(TSerialize& ser)
{
	CHECK_SCRIPT_STACK;

	if(m_pScriptObject && m_pScriptObject->HaveValue("OnSaveAI") && m_pScriptObject->HaveValue("OnLoadAI"))
	{
		SmartScriptTable persistTable(m_pScriptObject->GetScriptSystem());
		if (ser.IsWriting())
			Script::CallMethod(m_pScriptObject, "OnSaveAI", persistTable);
		ser.Value( "ScriptData", persistTable.GetPtr() );
		if (ser.IsReading())
			Script::CallMethod(m_pScriptObject, "OnLoadAI", persistTable);
	}
}

//
//------------------------------------------------------------------------------
void CAIHandler::Update()
{
	if (m_bDelayedCharacterConstructor)
	{
		CallCharacterConstructor();
		m_bDelayedCharacterConstructor = false;
	}
	if (m_bDelayedBehaviorConstructor)
	{
		CallBehaviorConstructor(0);
		m_bDelayedBehaviorConstructor = false;
	}

/*	if (m_sCharacterName.empty())
	{
		const char* szDefaultCharacter = GetInitialCharacterName();
		m_sFirstCharacterName = szDefaultCharacter;
		SetCharacter(szDefaultCharacter);
	}

	if (m_sBehaviorName.empty())
	{
		const char* szDefaultBehavior = GetInitialBehaviorName();
		m_sFirstBehaviorName = szDefaultBehavior;
		m_pScriptObject->SetValue("DefaultBehaviour", szDefaultBehavior);
		SetBehavior(szDefaultBehavior);
	}*/

	m_FaceManager.Update();
}

//
//------------------------------------------------------------------------------
void CAIHandler::MakeFace(CAIFaceManager::e_ExpressionEvent expression)
{
	m_FaceManager.SetExpression(expression);
}

//
//------------------------------------------------------------------------------
void CAIHandler::SetupSoundPack()
{
	SmartScriptTable pEntityProperties;
	if (!m_pScriptObject->GetValue("Properties",pEntityProperties))
		return;

	const char* szAISoundPackName = 0;
	if (pEntityProperties->GetValue("SoundPack", szAISoundPackName))
		m_pSoundPackNormal = s_ReadabilityManager.FindSoundPack(szAISoundPackName);
	const char *aiSoundPackNameAlternative=NULL;
	if(pEntityProperties->GetValue("SoundPackAlternative", aiSoundPackNameAlternative))
		m_pSoundPackAlternative = s_ReadabilityManager.FindSoundPack(aiSoundPackNameAlternative);
}

//
//------------------------------------------------------------------------------
void CAIHandler::SetupAnimPack()
{
	SmartScriptTable	pEntityProperties;
	if(!m_pScriptObject->GetValue("Properties",pEntityProperties))
	{
		gEnv->pAISystem->Warning("<CAIHandler> ", "can't find Properties. Entity %s", m_pEntity->GetName());
		return;
	}

	SmartScriptTable	pAnimPacksTable;
	if (gEnv->pScriptSystem->GetGlobalValue("ANIMATIONPACK", pAnimPacksTable))
	{
		const char* szAIAnimPackName = 0;
		if (pEntityProperties->GetValue("AnimPack", szAIAnimPackName))
		{
			if (!FindOrLoadTable(pAnimPacksTable, szAIAnimPackName, m_pAnimationPackTable) && strlen(szAIAnimPackName))
				gEnv->pAISystem->Warning("<CAIHandler> ", "Could not load AnimPack table %s for entity %s.", szAIAnimPackName, m_pEntity->GetName());
		}
	}
}
