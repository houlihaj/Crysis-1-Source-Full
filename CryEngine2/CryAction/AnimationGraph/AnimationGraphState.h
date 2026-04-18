#ifndef __ANIMATIONGRAPHSTATE_H__
#define __ANIMATIONGRAPHSTATE_H__

#pragma once

#include "AnimationGraph.h"
#include "AnimationGraphManager.h"
#include "MiniQueue.h"
#include "AnimationTrigger.h"
#include "STLPoolAllocator.h"

#define HEAVY_ANIM_PROFILING 1

#if HEAVY_ANIM_PROFILING
# define ANIM_PROFILE_FUNCTION FUNCTION_PROFILER(GetISystem(), PROFILE_GAME)
#else
# define ANIM_PROFILE_FUNCTION ((void*)0)
#endif

struct IAnimationStateNode;
struct IAnimationLinkNode;

class CAnimationGraphExistanceQuery;
class CExactPositioning;

class CAnimationGraphState : public IAnimationGraphState
{
	friend class CAnimationGraphDebug;
	friend class CAnimationGraphExistanceQuery;

public:
	typedef CStateIndex::StateID StateID;
	typedef CAnimationGraph::StateList StateList;
	typedef IAnimationGraphState::InputID InputID;
	static const uint16 INVALID_STATE = ~0;

	struct QueryResult
	{
		ANIMGRAPH_BIT_VAR(bool, foundResult, 1);
		ANIMGRAPH_BIT_VAR(bool, countsAsQueriedState, 1);
		ANIMGRAPH_BIT_VAR(bool, useTriggeredTransition, 1);
		StateID state;
		StateID queriedState;
	};

	CAnimationGraphState( _smart_ptr<CAnimationGraph> pGraph );
	~CAnimationGraphState();

	// IAnimationGraphState
	virtual bool SetInput( InputID, float, TAnimationGraphQueryID * pQueryID );
	virtual bool SetInput( InputID, int, TAnimationGraphQueryID * pQueryID );
	virtual bool SetInput( InputID, const char *, TAnimationGraphQueryID * pQueryID );
	virtual bool SetInputOptional( InputID, const char *, TAnimationGraphQueryID * pQueryID );
	virtual void ClearInput( InputID );
	virtual void LockInput( InputID, bool );

	virtual bool SetVariationInput( const char* name, const char* value );

	virtual void GetMemoryStatistics( ICrySizer * s );

	ILINE virtual bool DoesInputMatchState( InputID id)
	{
		return m_pGraph->m_stateIndex.StateMatchesInput( m_currentStateID, id, m_inputValueCache[m_inputValueCacheIndex][id], eSMIF_EnforceMatchesInput );
		//return (m_currentStateID==id);
	}

	// returns NULL if InputID is out of range
	virtual const char* GetInputName( InputID ) const;

	virtual void GetInput( InputID, char * ) const;
	virtual bool IsDefaultInputValue( InputID ) const;
	virtual void QueryLeaveState( TAnimationGraphQueryID * pQueryID );
	virtual void QueryChangeInput( InputID, TAnimationGraphQueryID * );

	string ExpandVariationInputs( const char* animationName ) const;

	virtual void SetAnimatedCharacter( CAnimatedCharacter* animatedCharacter, int layerIndex, IAnimationGraphState* parentLayerState )
	{
		m_pAnimatedCharacter = animatedCharacter;
		m_layerIndex = layerIndex;
		m_pParentLayerState = parentLayerState;
	}

	virtual CAnimatedCharacter* GetAnimatedCharacter() { return m_pAnimatedCharacter; }

	virtual void Release();
	virtual bool Update();
	virtual void ForceTeleportToQueriedState();
	virtual void PushForcedState( const char * state, TAnimationGraphQueryID * pQueryID );
	virtual void ClearForcedStates();
	virtual void SetBasicStateData( const SAnimationStateData& data );
	virtual float GetInputAsFloat( InputID id );
	virtual InputID GetInputId( const char *input );
	virtual void Serialize( TSerialize ser );
	IAnimationGraphPtr ChangeGraph( const char *graph );
	virtual void SetAnimationActivation( bool activated );
	virtual bool GetAnimationActivation();
	virtual const char * GetCurrentStateName();
	virtual void ForceLeaveCurrentState();
	virtual void InvalidateQueriedState();
	virtual void Pause( bool pause, EAnimationGraphPauser pauser );

	virtual CTimeValue GetAnimationLength();

	virtual void SetOutput( int id );
	virtual void ClearOutput( int id );
	virtual const char * QueryOutput( const char * name );

	virtual uint32 GetCurrentToken();
	virtual const CCryName& GetCurrentStructure();
	virtual void SetCurrentStructure( const CCryName& );

	virtual void Reset();
	void ClearOverrides();

	virtual void AddListener(const char * name, IAnimationGraphStateListener * pListener);
	virtual void RemoveListener(IAnimationGraphStateListener * pListener);

	virtual IAnimationSpacialTrigger * SetTrigger( const SAnimationTargetRequest&, EAnimationGraphTriggerUser user, TAnimationGraphQueryID*, TAnimationGraphQueryID* );
	virtual void ClearTrigger( EAnimationGraphTriggerUser user );
	virtual const SAnimationTarget * GetAnimationTarget();
	virtual bool HasAnimationTarget() const;

	virtual void SetTargetPointVerifier( IAnimationGraphTargetPointVerifier * pVerifier );
	virtual Vec2 GetQueriedStateMinMaxSpeed();

	virtual void Hurry();
	virtual void SetFirstPersonMode( bool on ) { m_firstPersonMode = on; }
	virtual bool IsUpdateReallyNecessary();

	virtual IAnimationGraphExistanceQuery * CreateExistanceQuery();

  virtual uint16 GetBlendSpaceWeightFlags() { return m_blendSpaceWeightFlags; }
  virtual void  SetBlendSpaceWeightFlags(uint16 flags) { m_blendSpaceWeightFlags = flags; }

	// used by CAnimationGraphStates
	virtual bool IsSignalledInput( InputID intputId ) const;

	// ~IAnimationGraphState

	static void ChangeDebug( const char * );
	static void ChangeDebugLayer( int );
	static void Debug_SingleStep();
	static void TestPlanner() { m_testPlanner = true; }

	void SendQueryComplete( TAnimationGraphQueryID id, bool success );
	void NotifyFinishPoint( const Vec3& position, const Quat& orientation );
	IEntity * GetEntity() { return m_basicState.pEntity; }

	ILINE _smart_ptr<CAnimationGraph> GetGraph() const { return m_pGraph; }
	StateID GetCurrentState() const { return m_currentStateID; }
	StateID GetQueriedState() const { return m_queriedStateID; }

	virtual bool IsMixingAllowedForCurrentState() const;
	virtual bool DoesParentLayerAllowMixing() const;

	int GetLayerIndex() const { return m_layerIndex; }

	const InputID * GetCurrentInputs() const 
	{
		return m_inputValueCache[m_inputValueCacheIndex];
	}

	ETriState CheckTargetMovement( const SAnimationMovement& movement, float radius, CTargetPointRequest& targetPointRequest );
	IAnimationGraphTargetPointVerifier * GetPointVerifier() { return m_pPointVerifier; }

	TAnimationGraphQueryID AllocQuery() { return m_nextQueryID++; }
	const CStateIndex::InputValue * GetInputValues() const { return m_inputValueCache[m_inputValueCacheIndex]; }
	void SetInputValue( CStateIndex::InputID id, CStateIndex::InputValue encValue, float fltValue );
	void BasicQuery( const CStateIndex::InputID * inputs, CStateIndex::StateIDVec& validStates );
	ETriState BasicPathfind( StateID fromStateID, CAnimationGraph::SPathFindParams& params);

	int GetRankings( const CStateIndex::InputValue* query, std::vector<uint16>& rankings );
	void SetKeepLowValueRankings( bool bKeep );
	bool GetKeepLowValueRankings();

	int GetRandomNumber() const { return m_randomNumber; }

	CAnimationGraphManager * GetManager() { return m_pGraph->m_pManager; }

	bool IsUsingTriggeredTransition() const { return m_usingTriggerTransition; }

private:
	typedef std::vector<IAnimationStateNode*> StateNodeVec;
	typedef std::vector<IAnimationStateNodeFactory*> StateFactoryVec;

	QueryResult PerformQuery( bool allowForceFollows );
	void UpdateTransitionTimes();
	void UpdateSignalling();

	void UpdateBlendWeights( ICharacterInstance * pCharacter );

	void BuildStateFactories( StateID state, StateFactoryVec& factories );
	void BuildStateFactories_Recurse( StateID state, StateFactoryVec& factories );
	void BuildStatesFromFactories( StateID id, const StateFactoryVec& factories, StateNodeVec& state, bool skipEntering = false );
	void TransitionStatesFromFactories();
	void ActivateNextState( bool fromRollback );
	void BeginNextTransition();
	void BeginTransitionTo( StateID state, bool fromRollback );
	void RefreshUpdateStates();
	StateID PeekNextAnimState() const;
	void CheckTransitionAnimation(StateID stateID, bool isLast, bool & pop, bool & skipTransition) const;
	bool CanLeaveState( StateID toState );
	void SetInput( InputID, CStateIndex::InputValue, float, TAnimationGraphQueryID * pQueryID );
	void DoReset( bool clearInputs );
	void RollbackState();
	void PublishLeftStates( StateID fromState );
	bool IsInDebugBreak();
	void SetCatchupFlag() { m_catchupFlag = true; }
	void PathFind( StateID from, const QueryResult& to );
	void OnEnterState( StateID id );

	void UpdateAt( CTimeValue ) {}
	// begin a transition if necessary, and return true if that was done
	bool MaybeBeginTransition( const QueryResult&, StateID currentWantId );

	// we can be in a variety of different states, they are listed here
	typedef void (CAnimationGraphState::*StateUpdateFunction)();
	// we are in our initial state, which we must escape from as soon as is possible
	void Update_InitialState();
	// we are in a steady state, monitor for changes, but otherwise just maintain the status quo
	void Update_SteadyState();
	// we are in the steady part of a multi-part transition - this is an extension of Update_SteadyState with monitoring
	// of when the transition is ready to change
	void Update_EphemeralState();
	// we are transitioning between two states... we just need to complete this transition to get to the next one
	void Update_TransitionState();

	void DebugDisplay();
	void DebugTransitionSelection( const QueryResult& qr );
	void DebugLeftState( StateID state, bool cancelled );
	void PopActiveTransitionState();

	ILINE size_t GetOverrideSlotCount() const { return m_pGraph->m_pManager->GetOverrideSlotCount(); }

	uint32 m_curSerial;

	CAnimatedCharacter* m_pAnimatedCharacter;
	int m_layerIndex;
	IAnimationGraphState* m_pParentLayerState;
	_smart_ptr<CAnimationGraph> m_pGraph;

	StateID m_debugLastLocomotionState;

	// this is the state we're currently in
	StateID m_currentStateID;
	// this is the state that optimally fits our criteria (forced states might bump this for the amount of sticky time)
	StateID m_queriedStateID;
	// this is the next state we'll be in
	StateID m_nextStateID;
	// is the current query invalidated?
	bool m_queryInvalidated;
	// alternates between 0/1 for which inputValueCache is current
	char m_inputValueCacheIndex;
	CStateIndex::InputValue m_inputValueCache[2][CAnimationGraph::MAX_INPUTS];
	float m_inputValuesAsFloats[CAnimationGraph::MAX_INPUTS];
	uint32 m_inputLocks;
	bool m_catchupFlag;
	uint32 m_queryConsideredInputs;

	// incrementing counter for the query id to be returned to higher-level code when it asks for it (never zero)
	TAnimationGraphQueryID m_nextQueryID;
	// pending query id's per input
	TAnimationGraphQueryID m_inputQueryIDs[CAnimationGraph::MAX_INPUTS];
	TAnimationGraphQueryID m_queryChangedInputIDs[CAnimationGraph::MAX_INPUTS];

	// cached memory - filled in by PerformQuery()
	CStateIndex::StateIDVec m_validStates;
	// when are we next allowed to perform a transition?
	// this is a hard lock on transitioning and even querying, for when an
	// animation simply MUST play for some amount of time
	CTimeValue m_nextAllowedTransition;
	// this is a soft lock for transitioning
	// if inputs change, and we end up needing to be in a DIFFERENT state
	// to the state that we need to be in currently (which may be different to
	// the state we are in), then we start a transition chain
	CTimeValue m_stickyTransitionCompletion;
	// when did this state start?
	CTimeValue m_stateStartTime;

	class CCountedFlag
	{
	public:
		CCountedFlag() : m_counter(0) {}

		bool EnterState( bool flag )
		{
			if (flag)
			{
				m_counter++;
				return m_counter==1;
			}
			return false;
		}
		bool LeaveState( bool flag )
		{
			if (flag)
			{
				assert(m_counter);
				m_counter--;
				return m_counter == 0;
			}
			return false;
		}
		bool Enabled()
		{
			return m_counter != 0;
		}
		void Reset()
		{
			m_counter = 0;
		}
		uint32 Debug() { return m_counter; }

	private:
		uint32 m_counter;
	};
	CCountedFlag m_noPhysicalCollider;

	SAnimationStateData m_basicState;


	bool m_isStateNodeCacheValid;
	void InvalidateStateNodeCache( bool leaveStates );

	StateNodeVec m_currentStateNodes;
	StateFactoryVec m_currentStateNodeFactories;
	StateNodeVec m_nextStateNodes;
	StateFactoryVec m_nextStateNodeFactories;
	StateNodeVec m_updateNodes;
#ifndef NDEBUG
	typedef std::vector<int> BoolVec;
#else
	typedef std::vector<bool> BoolVec;
#endif
	BoolVec m_nextNeedEnter;
	BoolVec m_currentNeedExit;




	struct SForcedState
	{
		SForcedState(StateID id = INVALID_STATE) : stateID(id), queryID(0)
		{
		}
		StateID stateID;
		TAnimationGraphQueryID queryID;
	};
	typedef std::deque<SForcedState> TForcedStates;
	TForcedStates m_forcedStates;
	uint32 m_token;
	CCryName m_structure;
	uint32 m_pauseState;
	float m_blendSpaceWeights[9];
  uint16 m_blendSpaceWeightFlags;
	//float m_hurry; // controls whether we need to hurry out of animations or not
	bool m_bHurry;
	StateID m_queriedStateIDWhenHurriedSet;
	static bool m_debugBreak;

	struct SListener
	{
		IAnimationGraphStateListener * pListener;
		char name[16 - sizeof(IAnimationGraphStateListener*)];

		bool operator==(const SListener& other) const
		{
			return pListener == other.pListener;
		}
	};

  std::vector<SListener> m_listeners;
  std::vector<SListener> m_callingListeners;
	typedef std::multimap<uint32, IAnimationStateNode*, std::less<uint32>, stl::STLPoolAllocator<std::pair<uint32, IAnimationStateNode*>, stl::PoolAllocatorSynchronizationSinglethreaded> > PendingLeftStates;
	PendingLeftStates m_pendingLeftStates;
	std::vector<StateID> m_pendingLeftStateIDs;
	typedef std::vector<TAnimationGraphQueryID> PendingLeftQueries;
	PendingLeftQueries m_callingPendingLeftQueries;
	PendingLeftQueries m_pendingLeftQueries;
	PendingLeftQueries m_pendingLeftStateQueries;
	void CallLeftQueries( PendingLeftQueries& qrys );
	bool m_currentStateWasEntered;
	StateID m_lastEnteredState;
	uint32 m_lastEnteredToken;
	IAnimationGraphTargetPointVerifier * m_pPointVerifier;

	static const int NUM_OLD_STATES = 20;
	struct SOldState
	{
		SOldState() : state(INVALID_STATE), cancelled(false) {}
		SOldState( StateID s, bool c ) : state(s), cancelled(c) {}
		StateID state;
		ANIMGRAPH_BIT_VAR(bool,cancelled,1);
	};
	typedef MiniQueue<SOldState, NUM_OLD_STATES> OldStateQueue;
	OldStateQueue m_oldStates;

	// state data
	StateUpdateFunction m_state;
	StateList m_activeTransition;
	bool m_usingTriggerTransition;

	bool m_resetLock;
	bool m_firstPersonMode;

	// outputs
	std::map<string, string> m_outputs;

	// variation inputs
	std::vector< string > m_variationInputValues;

	// profiling
	CStateIndex::QueryStats m_stateIndexQueryStats;
	CTimeValue m_lastQuery;
	CAnimationGraph::PathFindStats m_pathFindStats;
	CTimeValue m_lastPathFind;

	std::auto_ptr<CExactPositioning> m_pExactPositioning;

	// activation/deactivation
	bool m_bActivated;

	bool m_randomNumberLock;
	int m_randomNumber;

	static ITimer * m_pTimer;
	static EntityId m_debugEntity;
	static int m_debugLayer;
	static bool m_testPlanner;

	class CStateNameMapper;
	class CUpdateStateNameMapper;
};

#endif
