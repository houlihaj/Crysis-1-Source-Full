#ifndef __EXACTPOSITIONING_H__
#define __EXACTPOSITIONING_H__

#pragma once

#include "AnimationGraphState.h"

class CExactPositioning : public IAnimationSpacialTrigger
{
public:
	typedef CAnimationGraphState::StateID StateID;
	typedef CAnimationGraphState::StateList StateList;
	static const uint16 INVALID_STATE = CAnimationGraphState::INVALID_STATE;
	typedef CAnimationGraphState::QueryResult QueryResult;

	CExactPositioning( CAnimationGraphState * pState );
	~CExactPositioning();

	// IAnimationSpacialTrigger
	virtual IAnimationGraphState * GetState() { return m_pState; }
	virtual void SetInput( InputID, float );
	virtual void SetInput( InputID, int );
	virtual void SetInput( InputID, const char * );
	// ~IAnimationSpacialTrigger

	void Update();
	void FrameUpdate();

	void InvalidatePositions() { m_triggerRecalculate = true; }
	void CheckEphemeralToken();
	bool AllowRollbacks();

	bool SetTrigger( const SAnimationTargetRequest& req, EAnimationGraphTriggerUser user, TAnimationGraphQueryID * pQueryStart, TAnimationGraphQueryID * pQueryEnd );
	void ClearTrigger( EAnimationGraphTriggerUser user );
	ETriState CheckTargetMovement( const SAnimationMovement& movement, float radius, CTargetPointRequest& targetPointRequest );
	bool IsUpdateReallyNecessary();
	const SAnimationTarget * GetAnimationTarget();
	bool HasTargetRequest() const { return m_triggerCommit; }

	void PopFrontTransition()
	{
		assert(!m_ds.transition.Empty());
		//assert(m_activeTransition.front() == m_ds.transition.front());
		m_ds.transition.Pop();
	}

	const StateList& GetTriggerTransition() { return m_ds.transition; }
	bool OverrideQuery( QueryResult& qr, bool isSteady );
	ILINE TAnimationGraphQueryID GetTriggerQueryStart() {return m_triggerQueryStart;}
	bool CanDestroy() const;

	void DebugDraw_Status( IRenderer * pRend, int& x, int& y, int YSPACE );
	void DebugDraw_Transition( IRenderer * pRend, int& x, int& y, int YSPACE );

private:
	CAnimationGraphState * m_pState;

	enum ETriggerState
	{
		eTS_Disabled,
		eTS_Considering, // we're a long way from the target, not really doing anything yet
		eTS_Waiting, // we know about where we want to start the animation, but we are a still some ways away
		eTS_Preparing, // we're very close, start forcing the character to the right point
		eTS_FinalPreparation, // preparing, and have been in the zone to start
		eTS_Running, // we've started the animation
		eTS_Completing // we've finished the animation, but are still in the first loop of the following animation (cleanup time!)
	};
	void SetTriggerState( ETriggerState state );
	void CompleteTriggering( bool success );
	SAnimationTarget CalculateTarget( const SAnimationMovement& movement ) const;
	bool RecalculateTriggering();
	void MaybeRecalculateTriggering();
	void CommitTriggering();
	void CheckTriggers();
	bool IsMovingAnimation( ICharacterInstance * pChar );
	CAnimationGraphManager * GetManager() { return m_pState->GetManager(); }

	struct SDynamicState
	{
		SDynamicState();

		ETriggerState state;
		StateID startStateID;
		StateID targetStateID;
		SAnimationTarget target;
		StateList transition;
		bool pathFindPerformed;
		bool gotInitialPosition;
		uint32 token;
		float disabledTime;
		float checkTime;
		CAnimationTrigger trigger;
		float pathfindRetryTimer;
	};

	SDynamicState m_ds;

	bool m_triggerRecalculate;
	bool m_triggerCommit;

	CStateIndex::InputValue m_triggerRequestInputs[CAnimationGraph::MAX_INPUTS];
	CStateIndex::InputValue m_triggerInputs[CAnimationGraph::MAX_INPUTS];
	float m_triggerInputsAsFloats[CAnimationGraph::MAX_INPUTS];
	float m_triggerRequestInputsAsFloats[CAnimationGraph::MAX_INPUTS];

	SAnimationMovement m_triggerMovement;
	SAnimationTargetRequest m_triggerRequest;
	SAnimationTargetRequest m_trigger;
	TAnimationGraphQueryID * m_pTriggerQueryStartRequest;
	TAnimationGraphQueryID * m_pTriggerQueryEndRequest;
	TAnimationGraphQueryID m_triggerQueryStart;
	TAnimationGraphQueryID m_triggerQueryEnd;
	EAnimationGraphTriggerUser m_triggerUser;

	static ITimer * m_pTimer;
};

#endif
