/********************************************************************
  CryGame Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   AIHandler.h
  Version:     v1.00
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 8:10:2004   12:04 : Created by Kirill Bulatsev

*********************************************************************/


#ifndef __AIHandler_H__
#define __AIHandler_H__


#pragma once

#include <IAgent.h>
#include "AIReadabilityManager.h"
#include "IMovementController.h"
#include "AIFaceManager.h"

struct IScriptSystem;
struct IScriptTable;
struct ISystem;
struct IAISignalExtraData;
class CAIProxy;
//
//---------------------------------------------------------------------------------------------------

class CAIHandler
	: public ISoundEventListener
	, IAnimationGraphStateListener
	, IAnimationGraphTargetPointVerifier
{
friend class CAIProxy;
friend class CAIReadabilityManager;

	// implementation of IAnimationGraphStateListener
	virtual void SetOutput( const char * output, const char * value ) {}
	virtual void QueryComplete( TAnimationGraphQueryID queryID, bool succeeded );
	virtual void DestroyedState(IAnimationGraphState* );

	//------------------  IAnimationGraphTargetPointVerifier
	virtual ETriState CanTargetPointBeReached(class CTargetPointRequest &request) const;
	virtual bool UseTargetPointRequest(const class CTargetPointRequest &request);
	virtual void NotifyFinishPoint( const Vec3& pt );
	virtual void NotifyAllPointsNotReachable();
	//------------------  ~IAnimationGraphTargetPointVerifier


public:
	CAIHandler(IGameObject *pGameObject);
	~CAIHandler(void);

	void	Init(ISystem *pSystem);
	void	Reset(EObjectResetType type);

	void	HandleExactPositioning(SOBJECTSTATE *pStates, CMovementRequest& mr);

	void	Update();
	void	AIMind(SOBJECTSTATE *state);
	void	AISignal(int signalID, const char * signalText, IEntity *pSender, const IAISignalExtraData* pData);
	void	Release();

	bool	DoReadibilityPack(const char* text, int readabilityType, bool soundOnly = false, bool stopPreviousSound = true);
	bool	HasReadibilitySoundFinished() const { return m_bSoundFinished; }
	/// Toggles readability testing on/off (called from a console command).
	void TestReadabilityPack(bool start, const char* szReadability);

	bool SetAGInput(EAIAGInput input, const char* value);
	bool ResetAGInput(EAIAGInput input);
	IAnimationGraphState* GetAGState();
	bool IsSignalAnimationPlayed( const char* value );
	bool IsActionAnimationStarted( const char* value );
	bool IsAnimationBlockingMovement();

	void OnDialogLoaded(struct ILipSync *pLipSync);
	void OnDialogFailed(struct ILipSync *pLipSync);

	enum ESetFlags
	{
		SET_IMMEDIATE,
		SET_DELAYED,
		SET_ON_SERILIAZE,
	};

	bool SetCharacter(const char* character, ESetFlags setFlags = SET_IMMEDIATE);
	void SetBehavior(const char* szBehavior, const IAISignalExtraData* pData = 0, ESetFlags setFlags = SET_IMMEDIATE);

	const char* GetCharacter();
	const char* GetBehavior();

	void SetupSoundPack();
	void SetupAnimPack();

	void CallCharacterConstructor();
	void CallBehaviorConstructor(const IAISignalExtraData* pData);

	ILINE CAIReadabilityManager::SoundReadabilityPack* GetSoundPack() const
	{ 
		ICVar *pAltSP = gEnv->pConsole->GetCVar("ai_UseAlternativeReadability");
		if(m_pSoundPackAlternative && pAltSP && pAltSP->GetIVal())
			return m_pSoundPackAlternative; 
		return m_pSoundPackNormal; 
	}

	static CAIReadabilityManager	s_ReadabilityManager;

	// For temporary debug info retrieving in AIProxy.
	bool	DEBUG_IsPlayingSignalAnimation() const { return m_playingSignalAnimation; }
	bool	DEBUG_IsPlayingActionAnimation() const { return m_playingActionAnimation; }
	const char*	DEBUG_GetCurrentSignaAnimationName() const { return m_currentSignalAnimName.c_str(); }
	const char*	DEBUG_GetCurrentActionAnimationName() const { return m_currentActionAnimName.c_str(); }
	EActorTargetPhase DEBUG_GetActorTargetPhase() const { return m_eActorTargetPhase; }

protected:

	const char* GetInitialCharacterName();
	const char* GetInitialBehaviorName();
	const char* GetBehaviorFileName(const char* szBehaviorName);

	void ResetCharacter();
	void ResetBehavior();
	void ResetAnimationData();

	void SetInitialBehaviorAndCharacter();

	bool	DoReadibilityAnimation( const char* text, int readabilityType);
	virtual void OnSoundEvent( ESoundCallbackEvent event,ISound *pSound );

	const char* CheckAndGetBehaviorTransition(const char* szSignalText);
	bool CallScript(IScriptTable* scriptTable, const char* funcName, float* value = 0, IEntity* pSender = 0, const IAISignalExtraData* pData = 0);
	bool CallBehaviorOrDefault(const char* signalText, float* value = 0, IAISignalExtraData* pExtradata = NULL);
	bool GetMostLikelyTable(IScriptTable* table, SmartScriptTable& dest);
	bool FindOrLoadTable(IScriptTable* pGlobalTable, const char* szTableName, SmartScriptTable& tableOut);
	void FindOrLoadBehavior(const char* szBehaviorName, SmartScriptTable& pBehaviorTable);

	void Serialize( TSerialize ser );
	void SerializeScriptAI(TSerialize& ser);

	void SetAlertness( int value, bool triggerEvent = false );
	void UpdateWeaponAlertness();
	/// Iterates to the next valid readability sound while testing readability sounds.
	void NextReadabilityTest();

	void ResetCommonTables();
	bool SetCommonTables();

	// returns true if AG input "Action" is set to "idle", "weaponAlerted", "" or "<<not set>>"
	// which means no special looping animation is playing at this time
	bool IsAllowedToPlayReadabilityAnimations();
	void MakeFace(CAIFaceManager::e_ExpressionEvent expression);

	IScriptTable*			m_pScriptObject;
	IEntity*					m_pEntity;
	IGameObject*			m_pGameObject;
	SmartScriptTable	m_pAnimationPackTable;

	tSoundID m_ReadibilitySoundID;
	bool m_bSoundFinished;
	CAIReadabilityManager::ReadabilityTestIter m_readabilityTestIter;

	SmartScriptTable m_pCharacter;
	SmartScriptTable m_pBehavior;
	SmartScriptTable m_pPreviousBehavior;
	SmartScriptTable m_pDefaultBehavior;

	SmartScriptTable m_pDefaultCharacter;
	SmartScriptTable m_pDEFAULTDefaultBehavior;
	SmartScriptTable m_pBehaviorTable;
	SmartScriptTable m_pBehaviorTableAVAILABLE;
	SmartScriptTable m_pBehaviorTableINTERNAL;

	int	m_CurrentAlertness;
	bool m_CurrentExclusive;

	bool	m_bDelayedCharacterConstructor;
	bool	m_bDelayedBehaviorConstructor;

	string m_sBehaviorName;
	string m_sDefaultBehaviorName;
	string m_sCharacterName;
	string m_sPrevCharacterName;

	string m_sFirstCharacterName;
	string m_sFirstBehaviorName;

	IAnimationGraphState* m_pAGState;
	TAnimationGraphQueryID m_ActionQueryID;
	TAnimationGraphQueryID m_SignalQueryID;
	string m_sQueriedActionAnimation;
	string m_sQueriedSignalAnimation;
	bool m_bSignaledAnimationStarted;

	typedef std::set< string > SetStrings;
	SetStrings m_setPlayedSignalAnimations;
	SetStrings m_setStartedActionAnimations;

	TAnimationGraphQueryID m_actorTargetStartedQueryID;
	TAnimationGraphQueryID m_actorTargetEndQueryID;
	TAnimationGraphQueryID* m_curActorTargetStartedQueryID;
	TAnimationGraphQueryID* m_curActorTargetEndQueryID;
	bool m_bAnimationStarted;
	EActorTargetPhase m_eActorTargetPhase;
	int m_actorTargetId;

	TAnimationGraphQueryID m_changeActionInputQueryId;
	TAnimationGraphQueryID m_changeSignalInputQueryId;

	string m_sAGActionSOAutoState;

	bool	m_playingSignalAnimation;
	bool	m_playingActionAnimation;
	bool	m_bOwnsActionInput;
	string	m_currentSignalAnimName;
	string	m_currentActionAnimName;

	Vec3 m_vAnimationTargetPosition;
	Quat m_qAnimationTargetOrientation;
	bool m_enabledTargetPointVerifier;

	CAIFaceManager	m_FaceManager;

private:

	CAIReadabilityManager::SoundReadabilityPack* m_pSoundPackNormal;
	CAIReadabilityManager::SoundReadabilityPack* m_pSoundPackAlternative;
};









#endif __AIHandler_H__
