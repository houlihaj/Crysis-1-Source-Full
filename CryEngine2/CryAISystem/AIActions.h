#ifndef _AIACTIONS_H_
#define _AIACTIONS_H_

#if _MSC_VER > 1000
#pragma once
#endif


#include <IFlowSystem.h>
#include <IAIAction.h>

#include "SmartObjects.h"


// forward declaration
class CAIActionManager;


///////////////////////////////////////////////////
// CAIAction references a Flow Graph - sequence of elementary actions
// which should be executed to "use" a smart object
///////////////////////////////////////////////////
class CAIAction : public IAIAction
{
protected:
	// CAIActionManager needs right to access and modify these private data members
	friend class CAIActionManager;

	// unique name of this AI Action
	string m_Name;

	// points to a flow graph which would be used to execute the AI Action
	IFlowGraphPtr	m_pFlowGraph;

public:
	CAIAction( const CAIAction& src )
	{
		m_Name = src.m_Name;
		m_pFlowGraph = src.m_pFlowGraph;
	}
	CAIAction() {}
	virtual ~CAIAction() {}

	// returns the unique name of this AI Action
	virtual const char* GetName() const { return m_Name; }

	// returns the goal pipe which executes this AI Action
	virtual IGoalPipe* GetGoalPipe() const { return NULL; };

	// returns the Flow Graph associated to this AI Action
	virtual IFlowGraph* GetFlowGraph() const { return m_pFlowGraph; }

	// returns the User entity associated to this AI Action
	// no entities could be assigned to base class
	virtual IEntity* GetUserEntity() const { return NULL; }

	// returns the Object entity associated to this AI Action
	// no entities could be assigned to base class
	virtual IEntity* GetObjectEntity() const { return NULL; }

	// ends execution of this AI Action
	// don't do anything since this action can't be executed
	virtual void EndAction() {}

	// cancels execution of this AI Action
	// don't do anything since this action can't be executed
	virtual void CancelAction() {}

	// aborts execution of this AI Action - will start clean up procedure
	virtual bool AbortAction() { AIAssert( !"Aborting inactive AI action!" ); return false; }

	// marks this AI Action as modified
	virtual void Invalidate() { m_pFlowGraph = NULL; }

	// removes pointers to entities
	virtual void OnEntityRemove() {}

	virtual const Vec3& GetAnimationPos() const { return Vec3Constants< float >::fVec3_Zero; }
	virtual const Vec3& GetAnimationDir() const { return Vec3Constants< float >::fVec3_OneY; }

	virtual const Vec3& GetApproachPos() const { return Vec3Constants< float >::fVec3_Zero; }
};


///////////////////////////////////////////////////
// CAnimationAction references a goal pipe
// which should be executed to "use" a smart object
///////////////////////////////////////////////////
class CAnimationAction : public IAIAction
{
protected:
	// animation parameters
	bool	bSignaledAnimation;
	bool	bExactPositioning;
	string	sAnimationName;

	// approaching parameters
	Vec3	vApproachPos;
	float	fApproachSpeed;
	int		iApproachStance;

	// exact positioning parameters
	Vec3	vAnimationPos;
	Vec3	vAnimationDir;
	float	fStartRadiusTolerance;
	float	fDirectionTolerance;
	float	fTargetRadiusTolerance;

public:
	CAnimationAction(
		const char* animationName,
		bool signaled,
		float approachSpeed,
		int approachStance,
		float startRadiusTolerance,
		float directionTolerance,
		float targetRadiusTolerance )
			: sAnimationName(animationName)
			, bSignaledAnimation(signaled)
			, fApproachSpeed(approachSpeed)
			, iApproachStance(approachStance)
			, fStartRadiusTolerance(startRadiusTolerance)
			, fDirectionTolerance(directionTolerance)
			, fTargetRadiusTolerance(targetRadiusTolerance)
			, bExactPositioning(true)
			, vApproachPos(ZERO)
		{}
	CAnimationAction(
		const char* animationName,
		bool signaled )
		: sAnimationName(animationName)
		, bSignaledAnimation(signaled)
		, bExactPositioning(false)
		, vApproachPos(ZERO)
	{}
	virtual ~CAnimationAction() {}

	// returns the unique name of this AI Action
	virtual const char* GetName() const { return NULL; }

	// returns the goal pipe which executes this AI Action
	virtual IGoalPipe* GetGoalPipe() const;

	// returns the Flow Graph associated to this AI Action
	virtual IFlowGraph* GetFlowGraph() const { return NULL; }

	// returns the User entity associated to this AI Action
	// no entities could be assigned to base class
	virtual IEntity* GetUserEntity() const { return NULL; }

	// returns the Object entity associated to this AI Action
	// no entities could be assigned to base class
	virtual IEntity* GetObjectEntity() const { return NULL; }

	// ends execution of this AI Action
	// don't do anything since this action can't be executed
	virtual void EndAction() {}

	// cancels execution of this AI Action
	// don't do anything since this action can't be executed
	virtual void CancelAction() {}

	// aborts execution of this AI Action - will start clean up procedure
	virtual bool AbortAction() { AIAssert( !"Aborting inactive AI action!" ); return false; }

	// marks this AI Action as modified
	virtual void Invalidate() {}

	// removes pointers to entities
	virtual void OnEntityRemove() {}

	void SetTarget( const Vec3& position, const Vec3& direction )
	{
		vAnimationPos = position;
		vAnimationDir = direction;
	}

	void SetApproachPos( const Vec3& position )
	{
		vApproachPos = position;
	}

	virtual const Vec3& GetAnimationPos() const { return vAnimationPos; }
	virtual const Vec3& GetAnimationDir() const { return vAnimationDir; }

	virtual const Vec3& GetApproachPos() const { return vApproachPos; }
};


///////////////////////////////////////////////////
// CActiveAction represents single active CAIAction
///////////////////////////////////////////////////
class CActiveAction
	: public CAIAction
{
protected:
	// CAIActionManager needs right to access and modify these private data members
	friend class CAIActionManager;

	// entities participants in this AI Action
	IEntity* m_pUserEntity;
	IEntity* m_pObjectEntity;

	// AI Action is suspended if this counter isn't zero
	// it shows by how many other AI Actions this was suspended
	int m_SuspendCounter;

	// id of goal pipe used for tracking
	int m_idGoalPipe;

	// alertness threshold level
	int m_iThreshold;

	// next user's and object's states
	string m_nextUserState;
	string m_nextObjectState;

	// canceled user's and object's states
	string m_canceledUserState;
	string m_canceledObjectState;

	// set to true if action should be deleted on next update
	bool m_bDeleted;

	// set to true if action is high priority
	bool m_bHighPriority;

	// exact positioning parameters
	Vec3	m_vAnimationPos;
	Vec3	m_vAnimationDir;
	Vec3	m_vApproachPos;

public:
	CActiveAction() : m_vAnimationPos(0.0f,0.0f,0.0f),m_vAnimationDir(0.0f,0.0f,0.0f),m_vApproachPos(0.0f,0.0f,0.0f) {}
	CActiveAction( const CActiveAction& src )
		: CAIAction()
	{
		*this = src;
	}

	CActiveAction& operator = ( const CActiveAction& src )
	{
		m_Name = src.m_Name;
		m_pFlowGraph = src.m_pFlowGraph;

		m_pUserEntity = src.m_pUserEntity;
		m_pObjectEntity = src.m_pObjectEntity;
		m_SuspendCounter = src.m_SuspendCounter;
		m_idGoalPipe = src.m_idGoalPipe;
		m_iThreshold = src.m_iThreshold;
		m_nextUserState = src.m_nextUserState;
		m_nextObjectState = src.m_nextObjectState;
		m_canceledUserState = src.m_canceledUserState;
		m_canceledObjectState = src.m_canceledObjectState;
		m_bDeleted = src.m_bDeleted;
		m_bHighPriority = src.m_bHighPriority;
		m_vAnimationPos = src.m_vAnimationPos;
		m_vAnimationDir = src.m_vAnimationDir;
		m_vApproachPos = src.m_vApproachPos;
		return *this;
	}

	// returns the User entity associated to this AI Action
	virtual IEntity* GetUserEntity() const { return m_pUserEntity; }

	// returns the Object entity associated to this AI Action
	virtual IEntity* GetObjectEntity() const { return m_pObjectEntity; }

	// returns true if action is active and marked as high priority
	virtual bool IsHighPriority() const { return m_bHighPriority; }

	// ends execution of this AI Action
	virtual void EndAction();

	// cancels execution of this AI Action
	virtual void CancelAction();

	// aborts execution of this AI Action - will start clean up procedure
	virtual bool AbortAction();

	// removes pointers to entities
	virtual void OnEntityRemove() { m_pUserEntity = m_pObjectEntity = NULL; }

	bool operator == ( const CActiveAction& other ) const;

	virtual const Vec3& GetAnimationPos() const { return m_vAnimationPos; }
	virtual const Vec3& GetAnimationDir() const { return m_vAnimationDir; }

	virtual const Vec3& GetApproachPos() const { return m_vApproachPos; }

	void Serialize( TSerialize ser );
};


///////////////////////////////////////////////////
// CAIActionManager keeps track of all AIActions
///////////////////////////////////////////////////
class CAIActionManager
	: public IGoalPipeListener
{
private:
	// library of all defined AI Actions
	typedef std::map< string, CAIAction > TActionsLib;
	TActionsLib m_ActionsLib;

	// list of all active AI Actions (including suspended)
	typedef std::list< CActiveAction > TActiveActions;
	TActiveActions m_ActiveActions;

protected:
	// suspends all active AI Actions in which the entity is involved
	// note: it's safe to pass pEntity == NULL
	int SuspendActionsOnEntity( IEntity* pEntity, int goalPipeId, const IAIAction* pAction, bool bHighPriority, int& numHigherPriority );

	// resumes all active AI Actions in which the entity is involved
	// (resuming depends on it how many times it was suspended)
	// note: it's safe to pass pEntity == NULL
	void ResumeActionsOnEntity( IEntity* pEntity, int goalPipeId );

	// implementing IGoalPipeListener interface
	virtual void OnGoalPipeEvent( IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId );

public:
	CAIActionManager();
	~CAIActionManager();

	// stops all active actions
	void Reset();

	// returns an existing AI Action from the library or NULL if not found
	CAIAction* GetAIAction( const char* sName );

	// returns an existing AI Action by its index in the library or NULL index is out of range
	CAIAction* GetAIAction( size_t index );

	// returns an existing AI Action by its name specified in the rule
	// or creates a new temp. action for playing the animation specified in the rule
	IAIAction* GetAIAction( const CCondition* pRule );

	// adds an AI Action in the list of active actions
	void ExecuteAction( const IAIAction* pAction, IEntity* pUser, IEntity* pObject, int maxAlertness, int goalPipeId,
		const char* userState, const char* objectState, const char* userCanceledState, const char* objectCanceledState );

	// aborts specific AI Action (specified by goalPipeId) or all AI Actions (goalPipeId == 0) in which pEntity is a user
	void AbortActionsOnEntity( IEntity* pEntity, int goalPipeId = 0 );

	// marks AI Action from the list of active actions as deleted
	void ActionDone( CActiveAction& action, bool bRemoveAction = true );

	// removes deleted AI Action from the list of active actions
	void Update();

	// loads the library of AI Action Flow Graphs
	void LoadLibrary( const char* sPath );

	// notification sent by smart objects system
	void OnEntityRemove( IEntity* pEntity );

	void Serialize( TSerialize ser );
};


#endif
