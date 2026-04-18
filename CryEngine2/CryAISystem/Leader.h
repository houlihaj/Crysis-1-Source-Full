/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   Leader.h
	$Id$
  Description: Header for the CLeader class
  
 -------------------------------------------------------------------------
  History:
  Created by Kirill Bulatsev
	- 01:06:2005   : serialization support added, related refactoring, moved out to separate files ObstacleAllocator, etc


*********************************************************************/


#ifndef __Leader_H__
#define __Leader_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include "AIActor.h"
#include "CAISystem.h"
#include "LeaderAction.h"
#include "IEntity.h"
// #include "ObstacleAllocator.h" (include stmt moved to CAISystem.h)
#include <list>
#include <map>



class CAIGroup;
class CLeader;
class CPipeUser;
class CLeaderAction;
struct LeaderActionParams;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct TTarget {
	Vec3 position;
	CAIObject* object;
	TTarget(Vec3& spot,CAIObject* pObject): position(spot), object(pObject) {};
};

class 	CAttackRequest {
public:
	void EndRequest(const char* signalText);
	virtual bool Update() { EndRequest("OnAttackFailed");return true;}
	void	Serialize( TSerialize ser, CObjectTracker& objectTracker, CLeader* pLeader );
	
	void operator =(const CAttackRequest& other) 
	{
		m_iUnitProp = other.m_iUnitProp;
		m_iUpdateIndex = other.m_iUpdateIndex;
		m_actionType = other.m_actionType;
		m_iMaxUpdates = other.m_iMaxUpdates;
		m_bUpdate = other.m_bUpdate;
		m_bInit = other.m_bInit;
		m_vCurrentDir = other.m_vCurrentDir;
		m_fMultiplier = other.m_fMultiplier;
		m_iCount1 = other.m_iCount1;
		m_pLeader = other.m_pLeader;
		m_fDuration = other.m_fDuration;
		m_vDefensePoint = other.m_vDefensePoint;
	}

	CAttackRequest(CLeader* pLeader, uint32 unitProp, ELeaderActionSubType ac, float fDuration,const Vec3& defensePoint=ZERO)
	{
		m_pLeader = pLeader;
		m_iUnitProp = unitProp;
		m_actionType = ac;
		m_iUpdateIndex =0;
		m_bUpdate = true;
		m_bInit = true;
		m_fDuration = fDuration;
		m_vDefensePoint = defensePoint;
	}
public:
	CLeader* m_pLeader;
	uint32	m_iUnitProp ;
	int		m_iUpdateIndex ;
	ELeaderActionSubType m_actionType;
	int		m_iMaxUpdates ;
	bool	m_bUpdate ;
	bool	m_bInit;
	Vec3	m_vCurrentDir;
	float	m_fMultiplier;
	int		m_iCount1;
	float	m_fDuration;
	Vec3	m_vDefensePoint;

};

class 	CAttackRequest_Row : public CAttackRequest {
public:
	bool Update();
	CAttackRequest_Row(CLeader* pLeader, uint32 unitProp,ELeaderActionSubType ac, float fDuration, const Vec3& defensePoint)  : 
		CAttackRequest(pLeader,unitProp,ac,fDuration,defensePoint) {};

};

class 	CAttackRequest_LeapFrog : public CAttackRequest {
public:
	CAttackRequest_LeapFrog(CLeader* pLeader, uint32 unitProp,ELeaderActionSubType ac, float fDuration, const Vec3& defensePoint)  :
		CAttackRequest(pLeader,unitProp,ac,fDuration,defensePoint) {};
	bool Update();

};

class 	CAttackRequest_Flank : public CAttackRequest {
public:
	CAttackRequest_Flank(CLeader* pLeader, uint32 unitProp,ELeaderActionSubType ac, float fDuration, const Vec3& defensePoint)  :
	  CAttackRequest(pLeader,unitProp,ac,fDuration,defensePoint) {};
	  bool Update();

};

class 	CAttackRequest_AlienEightball : public CAttackRequest {
public:
	CAttackRequest_AlienEightball( CLeader* pLeader, uint32 unitProp, ELeaderActionSubType ac, float fDuration )  :
		CAttackRequest( pLeader, unitProp, ac, fDuration ) {};
	bool Update();

};

class 	CAttackRequest_AlienLeapfrog : public CAttackRequest {
public:
	CAttackRequest_AlienLeapfrog( CLeader* pLeader, uint32 unitProp, ELeaderActionSubType ac, float fDuration )  :
		CAttackRequest( pLeader, unitProp, ac, fDuration ) {};
	bool Update();

};


enum EFollowState
{
	FS_NONE,
	FS_HOLD,
	FS_FOLLOW,
	FS_LAST,	// make sure this one is always the last!
};


class CObstacleAllocator;

class CLeader :
	public CAIActor,public ILeader
{
//	class CUnitAction;

	friend class CLeaderAction;

public:

	typedef std::map<CUnitImg*, Vec3 > CUnitPointMap;
	typedef std::vector<CLeader*> TVecLeaders;
	typedef std::vector<CAIObject*> TVecTargets;
	typedef std::list<int> TUsableObjectList;
	typedef std::multimap<int,TUnitList::iterator> TClassUnitMap;
	typedef std::multimap<float,TTarget > TAlertSpotMap;
	typedef std::map<uint32,CAttackRequest*> TAttackRequestMap;
	typedef std::map<INT_PTR,Vec3> TSpotList;

	CLeader(CAISystem *pAISystem, int iGroupID);
	~CLeader(void);

	////////////////////////////////////////////////////////////////////////////////////////////////
	// SYSTEM/AI SYSTEM RELATED FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////

	void		Update(EObjectUpdate type);
	void		Reset(EObjectResetType type);
	void		OnObjectRemoved(CAIObject* pObject);
	//void		DebugDraw(IRenderer* pRenderer) const;
	void		Serialize( TSerialize ser, CObjectTracker& objectTracker );
	void		PopulateObjectTracker(CObjectTracker& objectTracker);

	void		SetAIGroup(CAIGroup* pGroup) { m_pGroup = pGroup; }
	CAIGroup*	GetAIGroup() { return m_pGroup; }
	virtual void SetAssociation(CAIObject* pAssociation);

	////////////////////////////////////////////////////////////////////////////////////////////////
	// CLEADER PROPERTIES FUNCTION
	////////////////////////////////////////////////////////////////////////////////////////////////

	// <Title IsPlayer>
	// Description: returns true if this is associated to the player AI object
	bool		IsPlayer() const;

	// <Title IsIdle>
	// Description: returns true if there's no LeaderAction active
	bool		IsIdle() const;

	// <Title SetFollowState>
	// Description: Set the follow state 
	// Arguments: 
	// EFollowState state - next follow state (FS_NONE, FS_HOLD, FS_FOLLOW)
	void		SetFollowState(EFollowState state); 

	// <Title GetFollowState>
	// Description: Returns the follow state (FS_NONE, FS_HOLD, FS_FOLLOW)
	inline EFollowState	GetFollowState() const {return m_FollowState;}

	// <Title GetLastFollowState>
	// Description: Returns the previous follow state (FS_NONE, FS_HOLD, FS_FOLLOW)
	inline EFollowState	GetLastFollowState() const {return m_LastFollowState;}

	// <Title GetAllowFire>
	// Description: Returns true if the team is allowed to fire (used for Player's team only)
	bool		GetAllowFire() const { return !IsPlayer() || m_bAllowFire; }

	// <Title SetAlertStatus>
	// Description: Set the team alert status
	// Arguments:
	// EAIAlertStatus status - alert status 
	//   AIALERTSTATUS_SAFE - the team is in a friendly area and no enemies are in sight
	//   AIALERTSTATUS_UNSAFE - the team is in an enemy area, and/or there are enemies in sight
	//   AIALERTSTATUS_READY - enemy is engaging the team or has them in sight
	void	SetAlertStatus(EAIAlertStatus status);

	// <Title GetAlertStatus>
	// Description: Returns the team alert status
	// See also SetAlertStatus()
	inline EAIAlertStatus GetAlertStatus() const {return m_alertStatus;};


	////////////////////////////////////////////////////////////////////////////////////////////////
	// TEAM MANAGEMENT FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////

	// <Title GetActiveUnitPlanCount>
	// Description: return the number of units with at least one of the specified properties, which have an active plan (i.e. not idle)
	// Arguments: 
	//	uint32 unitPropMask - binary mask of properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON) 
	//		only the unit with any of the specified properties will be considered
	//		(default = UPR_ALL : all units are checked)
	int GetActiveUnitPlanCount(uint32 unitProp = UPR_ALL) const;

	// <Title OnUpdateUnitTask>
	// Description: ... Dejan? :)
	// Arguments: 
	//	CUnitImg* unit - unit to update the task during the current LeaderAction
	void	OnUpdateUnitTask(CUnitImg* unit);

	// <Title DeadUnitNotify>
	// Description: to be called when a unit in the group dies
	// Arguments:
	// CAIObject* pObj = Dying AIObject 
	void		DeadUnitNotify(CAIActor* pAIObj);


	// <Title AddUnitNotify>
	// Description: to be called when an AI is added to the leader's group
	// Arguments:
	// CAIObject* pObj = new AIObject joining group
	inline void		AddUnitNotify(CAIActor* pAIObj) 
	{
		if(m_pCurrentAction)
			m_pCurrentAction->AddUnitNotify(pAIObj);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////
	// POSITIONING/TACTICAL FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////

	// <Title GetPreferedPos>
	// Description: Returns the preferred start position for finding obstacles (Dejan?)
	Vec3		GetPreferedPos() const;

	// <Title ForcePreferedPos>
	// Description: 
	inline void	ForcePreferedPos(const Vec3& pos){m_vForcedPreferredPos= pos;};

	// <Title MarkPosition>
	// Description: marks a given point to be retrieved later
	// Arguments:
	//	Vec3& pos - point to be marked
	inline void	MarkPosition(const Vec3& pos) { m_vMarkedPosition = pos;}

	// <Title GetMarkedPosition>
	// Description: return the previously marked point
	inline const Vec3&	GetMarkedPosition() const {return m_vMarkedPosition;}

	// <Title AssignTargetToUnits>
	// Description: redistribute known attention targets amongst the units with the specified properties
	// Arguments:
	//	uint32 unitProp = binary selection mask for unit properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//		only the unit with any of the specified properties will be considered
	//	int maxUnitsPerTarget = specifies the maximum number of units that can have the same attention target
	void		AssignTargetToUnits(uint32 unitProp, int maxUnitsPerTarget);

	// <Title GetEnemyLeader>
	// Description: returns the leader puppet (CAIOject*) of the current Leader's attention target if there is
	//	(see CLeader::GetAttentionTarget()
	CAIObject* GetEnemyLeader() const;

	// <Title GetHidePoint>
	// Description: Find the point to hide for a unit, given the obstacle, using the enemy position as a reference
	// Arguments:
	//	CAIObject* pAIObject - unit's AI object
	//  const CObstacleRef& obstacle - obstacle reference
	// TODO : this method should be moved to the graph
	Vec3		GetHidePoint(const CAIObject* pAIObject, const CObstacleRef& obstacle) const;

	// <Title GetSearchPoint>
	// Description: Find the point to search for a unit, near the given obstacle
	// Arguments:
	//	CAIObject* pAIObject - unit's AI object
	//  const CObstacleRef& obstacle - obstacle reference
	Vec3		GetSearchPoint(const CAIObject* pAIObject, const CObstacleRef& obstacle) const;

	// <Title GetHoldPoint>
	// Description: Find a point to hold position near an obstacle, given a "center" position around which to stay
	// Arguments:
	//  const CObstacleRef& obstacle - obstacle reference
	//	Vec3 refPosition - center position 
	Vec3		GetHoldPoint(const CObstacleRef& obstacle,const Vec3&  refPosition) const;

	// <Title GetCoverPercentage>
	// Description: given a point to defend, returns the percentage of covering (0..1) of the units from the enemies
	// If result is 0, the point is totally uncovered. If it's 1, it's fully covered. 
	// Arguments:
	//  const Vec3& defensePoint - point to defend
	//	uint32 unitProp = binary selection mask for unit properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//		only the unit with any of the specified properties will be considered
	//  bool bUsePredictedPositions - if true, the predicted units positions will be used, depending on their current task
	//		it works only with move tasks and form, the latter is 100% accurate with fixed offset form. points only
	float GetCoverPercentage(const Vec3& defensePoint,uint32 unitProp, bool bUsePredictedPositions) const;

	// <Title GetBestShootSpot>
	// Description: returns the closest active shoot spot (if available in the list) to the given point. 
	//	Shoot spots are points added/removed to a spot list, by processing the signals "OnSpotSeeingTarget"/"OnSpotLosingTarget"
	// Arguments:
	//	const Vec3& position - point near which to find the shoot spot
	Vec3	GetBestShootSpot(const Vec3& position) const;

	// <Title ShootSpotsAvailable>
	// Description: returns true if there are active shoot spots available
	inline bool	ShootSpotAvailable() {return !m_SpotList.empty();}

	// TODO : this method should be moved to the tactical analyzer
	//void		FindNearestHidingPoints(const TUnitList& units, CUnitPointMap& hidingPoints);

	////////////////////////////////////////////////////////////////////////////////////////////////
	// ACTION RELATED FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////


	// <Title Attack>
	// Description: starts an attack action (stopping the current one)
	// Arguments:
	//  LeaderActionParams* pParams: leader action parameters - used depending on the action subtype (default: NULL -> default attack)
	//	pParams->subType - leader Action subtype (LAS_ATTACK_FRONT,LAS_ATTACK_ROW etc) see definition of ELeaderActionSubType
	//  pParams->fDuration - maximum duration of the attack action
	//  pParams->unitProperties -  binary selection mask for unit properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//		only the unit with any of the specified properties will be selected for the action
	void		Attack(const LeaderActionParams* pParams=NULL);

	// <Title Search>
	// Description: starts a search action (stopping the current one)
	// Arguments:
	//	const Vec3& targetPos - last known target position reference
	//	float distance - maximum distance to search (not working yet - it's hardcoded in AI System)
	//	uint32 unitProperties = binary selection mask for unit properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//		only the unit with any of the specified properties will be selected for the action
	void		Search(const Vec3& targetPos, float distance, uint32 unitProperties, int searchSpotAIObjectType);


	// <Title Goto>
	// Description: starts a Goto action (stopping the current one) - same as Hold, but unconditionally 
	//		- not depending on current action
	//	TO DO: check if both Hold and Goto are necessary
	// Arguments:
	//	const Vec3& point - center position to hold position around (default = ZERO -> leader position if there is, or average position)
	void		Goto( const Vec3& point = ZERO );

	// <Title Idle>
	// Description: stops the current action and sends the ORDER_IDLE signal to the units 
	void		Idle();

	// <Title Follow>
	// Description: starts a Follow action (stopping the current one) - unit will place in a formation around a selected unit (formation owner)
	// Arguments:
	//	const char* szFormationName - formation description name
	//	int iGroupId - group id of the followers (default = -1 ) if the groupId ==  -1, the leader will create
	//	 a formation for one of his units - if iGroupId>0 and different from this leader's, the units of this group
	//	 will join this leader's formation (if there is)
	//	const Vec3& vTargetPos - target position reference to set the formation initial orientation 
	//		(default = ZERO -> formation owner's orientation)
	//	CUnitImg* pSingleUnit - if not NULL, it assumes that there is a formation already and the specified unit will join it
	//	bool bForceNotHiding - (default = false) if true, the units will follow the owner not in a stealth/hide mode 
	//		otherwise, units will follow normally only if alert status is safe, or follow in stealth/hide mode if it's not
	//		(see CLeaderAction_Follow and CLeaderAction_FollowHiding)
	void		Follow(const char* szFormationName = NULL, int iGroupID = -1, const Vec3& vTargetPos = ZERO, CUnitImg* pSingleUnit = NULL, bool bForceNotHiding=false );

	// <Title Use>
	// Description: starts a Use action
	// Arguments:
	//	AISignalExtraData& SignalData
	//		SignalData.iValue = Use action type (AIUSEOP_*) see definition of EAIUseAction in IAgent.h
	void		Use(const AISignalExtraData& SignalData);

	void		UseVehicle(const CAIObject* pVehicle, const AISignalExtraData& data);
	void		LeaveVehicle(bool bFollowAfter = false);

	// <Title AbortExecution>
	// Description: clear all units' actions and cancel (delete) the current LeaderAction
	// Arguments:
	//	int priority (default 0) max priority value for the unit actions to be canceled (0 = all priorities)
	//		if an unit action was created with a priority value higher than this value, it won't be canceled
	void		AbortExecution(int priority = 0);

	// <Title ClearAllPlannings>
	// Description: clear all actions of all the unit with specified unit properties
	// Arguments:
	//	int priority (default 0) max priority value for the unit actions to be canceled (0 = all priorities)
	//		if an unit action was created with a priority value higher than this value, it won't be canceled
	//	uint32 unitProp = binary selection mask for unit properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//		only the unit with any of the specified properties will be considered
	void		ClearAllPlannings(uint32 unitProp = UPR_ALL, int priority = 0);

	void		RequestAttack(uint32 unitProp,int type, float fDuration,const Vec3& defensePoint);
	CAttackRequest* CreateAttackRequest(uint32 unitProp, ELeaderActionSubType actionType, float duration, const Vec3& defensePoint);

	////////////////////////////////////////////////////////////////////////////////////////////////
	// FORMATION RELATED FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////

	bool		LeaderCreateFormation(const char* szFormationName, const Vec3& vTargetPos, bool bIncludeLeader = true, uint32 unitProp = UNIT_ALL, CAIObject* pOwner=NULL);
	bool		JoinFormation(int iGroupID);
	bool		ReleaseFormation();
	bool		CreateFormation_Row(const Vec3& pos, float scale,int size=0, bool bIncludeLeader = true, const CAIObject* pReferenceTarget=NULL, uint32 unitProp = UNIT_ALL);
	bool		CreateFormation_LineFollow(const Vec3& pos, float scale,int size=0, bool bIncludeLeader = true,CAIObject* pOwner=NULL, CAIObject* pReferenceTarget=NULL, uint32 unitProp = UNIT_ALL);
	bool		CreateFormation_FrontRandom(const Vec3& pos, float scale,int size=0, bool bIncludeLeader = true, CAIObject* pReferenceTarget=NULL, uint32 unitProp = UNIT_ALL, CAIObject* pOwner=NULL);
	bool		CreateFormation_Saw(const Vec3& pos, float scale,int size=0 );
	bool		CreateFormation_Circle(float scale,int size=0 );
	bool		CreateFormation_Funnel(float scale,int size=0 );
	string	GetFormationName(char* type, int size);
	// Creates horizontal wedge shaped formation.
	//	pos		- The position of the wedge tip.
	//	scale - The scale of the whole formation. The formation is create to fit inside the normalized [-1,1] coordinates.
	//	size	- The number of points in the formation. If zero, the number of available units are used.
	bool		CreateFormation_HorizWedge( const Vec3& pos, float scale, int size = 0 );

	// Creates vertical wedge shaped formation.
	//	pos		- The position of the wedge tip.
	//	scale - The scale of the whole formation. The formation is create to fit inside the normalized [-1,1] coordinates.
	//	size	- The number of points in the formation. If zero, the number of available units are used.
	bool		CreateFormation_VertWedge( const Vec3& pos, float scale, int size = 0 );

	CAIObject*	GetNewFormationPoint(CAIObject * pRequester,int iPointType = 0);
	inline CAIObject*	GetFormationOwner( CAIObject * pRequester) const {return m_pFormationOwner;	}
	CAIObject*	GetFormationPoint(const CAIObject * pRequester) const;
	int			GetFormationPointIndex(const CAIObject * pAIObject) const;
	CAIObject * GetFormationPointSight(const CPipeUser * pRequester) const;
	void		FreeFormationPoint(const CAIObject * pRequester) const;
	
	inline CAIObject* GetFormationOwner() const { return m_pFormationOwner;}
	inline CAIObject* GetPrevFormationOwner() const { return m_pPrevFormationOwner;}

	inline const char* GetFormationDescriptor() const {return m_szFormationDescriptor.c_str();}
	int		AssignFormationPoints(bool bIncludeLeader = true, uint32 unitProp=UPR_ALL);
	bool	AssignFormationPoint(CUnitImg& unit, int iTeamSize=0, bool bAnyClass = false,bool bClosestToOwner = false);
	void	AssignFormationPointIndex(CUnitImg& unit, int index );
	void	NotifyJoiningFormation(CLeader* pOtherLeader) ;
	void	NotifyBreakingFormation();
	void	NotifyLeavingFormation(CLeader* pOtherLeader) const;

	// Called by CAIGroup.
	void	OnEnemyStatsUpdated(const Vec3& avgEnemyPos, const Vec3& oldAvgEnemyPos);

protected:
	void	UpdateEnemyStats();
	void	UpdateAlertStatus();
	void	ProcessSignal(AISIGNAL& signal );
	void	EnableUse(int action);
	void	DisableUse(int action = -1);
	bool	IsUsable(int action) const;
	void	UpdateAttackRequests();
	void	ClearAllAttackRequests();
	void	NewAttackRequest(uint32 unitProp=UPR_COMBAT_GROUND);
	CLeaderAction* CreateAction(const LeaderActionParams* params, const char* signalText=NULL) ;
	void	ClearAttackRequest(uint32 unitprop);
	void	AddShootSpot(INT_PTR n, Vec3 pos);
	void	RemoveShootSpot(INT_PTR n);

private:
	void	ChangeStance(EStance stance);

public:
	CObstacleAllocator*	m_pObstacleAllocator;

protected:
	CLeaderAction*	m_pCurrentAction;
	//Vec3			m_targetPoint;
//	TMapDistPoint	m_mapDistFormationPoints;
	CAIObject*		m_pFormationOwner;
	CAIObject*		m_pPrevFormationOwner;
	TVecLeaders		m_FormationJoiners;
	TUsableObjectList m_UsableObjects;
	string			m_szFormationDescriptor;
	EStance			m_Stance;
	EFollowState	m_FollowState;
	EFollowState	m_LastFollowState;
	int				m_iFormOwnerGroupID;
	bool			m_bUseEnabled;
	bool			m_bAllowFire;
	CTimeValue		m_fAllowingFireTime;
	//TAlertSpotMap	m_AlertSpots;
	EAIAlertStatus	m_alertStatus;
	Vec3			m_vForcedPreferredPos;
	int				m_currentAreaSpecies;
	bool			m_bForceSafeArea;
	IAIObject*		m_pEnemyTarget;
	bool			m_bUpdateAlertStatus;
	float			m_AlertStatusReadyRange;
	Vec3			m_vMarkedPosition;	//general purpose memory of a position - used in CLeaderAction_Search, as
										// last average units' position when there was a live target
	//Vec3			m_vEnemySize;
	TAttackRequestMap m_AttackRequestMap;
	bool			m_bLeaderAlive;
	bool			m_bKeepEnabled; // if true, CLeader::Update() will be executed even if the leader puppet is dead
	TSpotList m_SpotList;

	CAIGroup*	m_pGroup;
};

inline const CLeader* CastToCLeaderSafe(const IAIObject* pAI) { return pAI ? pAI->CastToCLeader() : 0; }
inline CLeader* CastToCLeaderSafe(IAIObject* pAI) { return pAI ? pAI->CastToCLeader() : 0; }

#endif __Leader_H__
