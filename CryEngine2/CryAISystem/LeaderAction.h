/******************************************************************** 
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   LeaderAction.h

Description: Header for CLeaderAction, CUnitImg, classes

-------------------------------------------------------------------------
History:
- 15:11:2004   17:41 : Created by Luciano Morpurgo

*********************************************************************/


#ifndef __LeaderAction_H__
#define __LeaderAction_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include "CAISystem.h"
#include "IAgent.h"
#include "AIFormationDescriptor.h"
#include "AIObject.h"
#include "Graph.h"
#include "UnitAction.h"
#include "UnitImg.h"
#include "TimeValue.h"
#include "Cry_Matrix.h"
#include <list>

class CLeader;

struct LeaderActionParams
{
	ELeaderAction type;
	ELeaderActionSubType subType;
	float fDuration;
	string name;
	Vec3	vPoint;
	Vec3	vPoint2;
	float	fSize;
	int iValue;
	uint32 unitProperties;
	CAIObject* pAIObject;
	ScriptHandle id;

	//LeaderActionParams(ELeaderAction t,ELeaderActionSubType s,float f): type(t),subType(s),fDuration(f),vPoint(ZERO),fSize(0),vPoint2(ZERO), pAIObject(NULL)  {};
	LeaderActionParams(ELeaderAction t,ELeaderActionSubType s): type(t),subType(s),fDuration(0),vPoint(ZERO),fSize(0),vPoint2(ZERO), pAIObject(NULL), iValue(0)  {};
	//LeaderActionParams(ELeaderAction t): type(t),subType(LAS_DEFAULT),fDuration(0),vPoint(ZERO),fSize(0),vPoint2(ZERO), pAIObject(NULL)  {};
	LeaderActionParams(): type(LA_LAST),subType(LAS_DEFAULT),fDuration(0),vPoint(ZERO),fSize(0),vPoint2(ZERO), pAIObject(NULL), iValue(0) {};

};


class CLeaderAction
{
protected:
	typedef std::list< std::pair< CUnitImg*, CUnitAction* > > ActionList;
	struct BlockActionList
	{
		BlockActionList(ActionList& blockers, ActionList& blocked)
		{
			ActionList::iterator i, j, iEnd = blockers.end(), jEnd = blocked.end();
			for (i = blockers.begin(); i != iEnd; ++i)
				for (j = blocked.begin(); j != jEnd; ++j)
					j->second->BlockedBy(i->second);
		}
	};
	struct PushPlanFromList
	{
		static void Do(ActionList& actionList)
		{
			ActionList::iterator it, itEnd = actionList.end();
			for(it = actionList.begin(); it != itEnd; ++it)
				it->first->m_Plan.push_back(it->second);
		}
	};

public:
	typedef enum {
		ACTION_RUNNING,
		ACTION_DONE,
		ACTION_FAILED
	} eActionUpdateResult;

	CLeaderAction() {};
	CLeaderAction(CLeader* pLeader);
	virtual ~CLeaderAction();

	ELeaderAction	GetType() const {return m_eActionType;};
	ELeaderActionSubType	GetSubType() const {return m_eActionSubType;};
	int				GetNumLiveUnits() const ;
	inline uint32	GetUnitProperties() const {return m_unitProperties;}

	bool IsUnitAvailable(const CUnitImg& unit) const;
	virtual bool	TimeOut() { return false; }

	virtual eActionUpdateResult	Update() { return ACTION_DONE; }
	virtual bool	FinalUpdate() { return false; }
	virtual void	DeadUnitNotify(CAIActor* pObj);	// CLeaderAction will manage the inter-dependencies between		
	// the died member's actions and other members ones
	virtual void	BusyUnitNotify(CUnitImg&);	// CLeaderAction will manage the inter-dependencies between		
	// the busy member's actions and other members ones
	virtual void	ResumeUnit(CUnitImg&) {};	// CLeaderAction will re-create a planning for the resumed unit
	virtual bool	ProcessSignal(const AISIGNAL& signal) { return false; }
	virtual void	OnUpdateUnitTask(CUnitImg* unit) {}
	inline void		SetPriority(int priority) {m_Priority = priority;};
	inline int		GetPriority() const {return m_Priority;};

	// the following functions are for adding UnitActions to units while performing the current LeaderAction
	//void			Use_PlantBomb(const AISignalExtraData& signalData);
	//void			Use_Rpg(const AISignalExtraData& signalData);
	TUnitList		&GetUnitsList() const ; 
	virtual void	Serialize( TSerialize ser, CObjectTracker& objectTracker );

	virtual void	SetUnitFollow(CUnitImg& unit) {};
	virtual void	SetUnitHold(CUnitImg& unit,const Vec3& point=ZERO) {};
	virtual void	UpdateBeacon() const ;
	// Returns pointer to the unit image of the group formation.
	CUnitImg*	GetFormationOwnerImg() const;
	//virtual void OnObjectRemoved(CAIObject* pObject) {}
	virtual void	AddUnitNotify(CAIActor* pUnit) {};

protected:
	CLeader*		m_pLeader;
	ELeaderAction	m_eActionType;
	ELeaderActionSubType	m_eActionSubType;
	int				m_Priority;
	uint32			m_unitProperties;
	IAISystem::ENavigationType m_currentNavType;
	IAISystem::tNavCapMask m_NavProperties;

protected:
	void	CheckLeaderDistance() const;
	const Vec3&		GetDestinationPoint();
	bool	IsUnitTooFar(const CUnitImg &tUnit,const Vec3 &vPos) const ;
	void	CheckNavType(CAIActor* pMember, bool bSignal);
	void	SetTeamNavProperties();
	void	ClearUnitFlags();

	//////////////////////////////////////////////////////////////////////////	
};

class CLeaderAction_Attack: public CLeaderAction
{
public:
	CLeaderAction_Attack(CLeader* pLeader);
	CLeaderAction_Attack();//used for derived classes' constructors
	virtual ~CLeaderAction_Attack();

	virtual bool TimeOut();
	virtual bool FinalUpdate();
	virtual bool ProcessSignal(const AISIGNAL& signal);
	virtual void OnUpdateUnitTask(CUnitImg* unit);
	virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
protected:
	bool	SomeoneHasTarget()const ;
	bool	HasTarget(CAIObject* unit) const;
	void	EndAttackAction( const char* signalText, uint32 unitCountMask=UNIT_ALL );
	void	HideLeader(const Vec3& avgPos,const Vec3& enemyPos,uint32 unitProp);
	bool	m_bInitialized;
	bool	m_bStealth;
	bool	m_bApproachWithNoObstacle;
	bool	m_bNoTarget;
	CTimeValue	m_timeRunning;
	float	m_timeLimit;
	static const float	m_CSearchDistance;
	Vec3	m_vDefensePoint;
	Vec3 m_vEnemyPos;


};


//----------------------------------------------------------
// CLeaderAction_Attack_FollowLeader: units always stay close (and possibly hide) near their formation point around leader

class CLeaderAction_Attack_FollowLeader: public CLeaderAction_Attack
{
	struct TSpot 
	{
		float reserveTime;
		CAIObject* pOwner;
	};
public:
	CLeaderAction_Attack_FollowLeader(CLeader* pLeader, const LeaderActionParams& params);
	//virtual ~CLeaderAction_Attack_FollowLeader();
	virtual bool	TimeOut() {return true;}
	virtual bool	FinalUpdate() {return false;}
	virtual eActionUpdateResult	Update();
	virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
	virtual void ResumeUnit(CUnitImg& unit);
	virtual bool ProcessSignal(const AISIGNAL& signal);
	virtual void	DeadUnitNotify(CAIActor* pObj);	// CLeaderAction will manage the inter-dependencies between		

private:
	void SearchHideSpot(CUnitImg& unit, Vec3 vSearchPos);
	void UpdateUnitPositions();
	static const float m_CSearchDistance;
	static const float m_CUpdateDistance;
	string m_sFormationType;
	Vec3 m_vLastUpdatePos;
	CTimeValue m_lastTimeMoving;
	bool 	m_bLeaderMoving;
	std::vector<TSpot> m_Spots;
	std::vector<int> m_SeeingEnemyList;
};

//----------------------------------------------------------
// CLeaderAction_Attack_LeapFrog: units split in two groups and alternatively one approach the player,
// while the other provide cover fire
class CLeaderAction_Attack_LeapFrog: public CLeaderAction_Attack
{
public:
	CLeaderAction_Attack_LeapFrog(CLeader* pLeader, const LeaderActionParams& params);
	CLeaderAction_Attack_LeapFrog() {};//used for derived classes' constructors
	virtual ~CLeaderAction_Attack_LeapFrog();
	virtual bool	TimeOut();
	virtual bool	FinalUpdate();
	virtual eActionUpdateResult	Update();
	virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );

protected:
	static const float m_CApproachInitDistance;
	static const float m_CApproachStep;
	static const float m_CApproachMinDistance;
	static const float m_CMaxDistance;
	float m_dist[2];
	float m_fApproachDistance;
	//IAIObject* m_pLiveTarget;
	//bool	m_bForceFinalUpdate;
	bool	m_bApproached;
	bool	m_bCoverApproaching;

};

//----------------------------------------------------------
// CLeaderAction_Attack_HideAndCover: default group combat tactic for AI leader : half of group goes hiding towards enemy
//	while other half provides cover fire

class CLeaderAction_Attack_HideAndCover: public CLeaderAction_Attack_LeapFrog
{


public:
	CLeaderAction_Attack_HideAndCover(CLeader* pLeader,const LeaderActionParams& params);
	CLeaderAction_Attack_HideAndCover() {};//used for derived classes' constructors
	//virtual ~CLeaderAction_Attack_HideAndCover();
	virtual bool	TimeOut() {return true;}
	virtual bool	FinalUpdate() {return false;}
	virtual eActionUpdateResult	Update();
	//virtual bool ProcessSignal(const AISIGNAL& signal);
	//virtual void OnUpdateUnitTask(CUnitImg* unit);
	//virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
protected:
	void UpdateStats();
	bool AdvanceHideAndCover();
	void HideInPlace();

protected:
	static const float m_CSearchDistance;
	static const float m_CApproachMinDistance;
	int m_iCoverGroup;
	int m_counttarget[2];
	Vec3 m_pos[2];
	int m_count[2];
	bool m_bHidingInPlace;
	bool m_bLastHideSuccessful;
	Vec3 m_vEnemyAveragePosition;
	Vec3 m_vAvgDirN;
	Vec3 m_vLastEnemyPos;
};

//----------------------------------------------------------
// CLeaderAction_Attack_CoordinateFire: units select a coordinated fire attack

class CLeaderAction_Attack_CoordinatedFire: public CLeaderAction_Attack
{
public:
	CLeaderAction_Attack_CoordinatedFire(CLeader* pLeader,const LeaderActionParams& params);
	CLeaderAction_Attack_CoordinatedFire() {};
	//virtual ~CLeaderAction_Attack_CoordinatedFire();
	virtual bool	TimeOut(){return true;};
	virtual bool	FinalUpdate(){return false;};
	virtual eActionUpdateResult	Update();
	virtual bool ProcessSignal(const AISIGNAL& signal);
	virtual void UpdateBeacon() const;
	virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
private:
	eActionUpdateResult m_updateStatus;

};

//----------------------------------------------------------
// CLeaderAction_Attack_UseSpots: units chose the best shoot spots to pursue the target

class CLeaderAction_Attack_UseSpots: public CLeaderAction_Attack
{
public:
	CLeaderAction_Attack_UseSpots(CLeader* pLeader,const LeaderActionParams& params);
	CLeaderAction_Attack_UseSpots() {};
	//virtual ~CLeaderAction_Attack_CoordinatedFire();
	virtual bool	TimeOut(){return true;};
	virtual bool	FinalUpdate(){return false;};
	virtual eActionUpdateResult	Update();
	virtual bool ProcessSignal(const AISIGNAL& signal);
	virtual void UpdateBeacon() const;
	//virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
private:
	CTimeValue m_lastTimeWithTarget;
	CTimeValue m_lastSpotFoundTime;
	CTimeValue m_lastApproachTargetTime;
	bool	m_bRequestForSpot;
	bool	m_bApproachingTarget;
	bool	m_bPathFindDone;
	static const float m_CApproachDistance;
	Vec3	m_gotoPos;
};

//----------------------------------------------------------
// CLeaderAction_Attack_Row: units move in a row formation in front of enemy and attack it

class CLeaderAction_Attack_Row: public CLeaderAction_Attack
{
public:
	CLeaderAction_Attack_Row(CLeader* pLeader, const LeaderActionParams& params);
	//virtual ~CLeaderAction_Attack_Row();
	CLeaderAction_Attack_Row() {};

	virtual bool TimeOut() {return true;}
	virtual eActionUpdateResult Update();
	virtual bool FinalUpdate() {return false;};
	virtual bool ProcessSignal(const AISIGNAL& signal);
	virtual void ResumeUnit(CUnitImg& );
	virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
protected: 
	bool		IsInFOV(const Vec3& point, uint32 unitProp, const Vec3& avgPos, int unitCount) const;
public:
	static const float m_CInitDistance;
	static const float m_CMaxDefenseDistance;
	static const float m_CCoordinatedFireDelayStep ;
protected:
	Vec3 m_vRowNormalDir;
	Vec3 m_vBasePoint;
	float m_fSize;
	float m_fInitialDistance;
	bool m_bTimerRunning;


};

//----------------------------------------------------------
// CLeaderAction_Attack_Front: units move and stay in front of the enemy

class CLeaderAction_Attack_Front: public CLeaderAction_Attack_Row
{
public:
	CLeaderAction_Attack_Front() {};
	CLeaderAction_Attack_Front(CLeader* pLeader, const LeaderActionParams& params);
	~CLeaderAction_Attack_Front();
	virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
	virtual eActionUpdateResult Update();
	virtual bool ProcessSignal(const AISIGNAL& signal);

protected:
	Vec3 m_vUpdateEnemyPos;
	int m_numUnitsBehind ;
	bool m_bInvestigating;

};


//----------------------------------------------------------
// Attack Flank: units flank the enemy

class CLeaderAction_Attack_Flank: public CLeaderAction_Attack
{
	typedef std::multimap<float,CUnitImg*> TDistanceUnitMap;
	typedef std::multimap<CUnitImg*,Vec3> TUnitPointMap;
public:
	CLeaderAction_Attack_Flank(CLeader* pLeader, float duration, bool bHiding);
	virtual ~CLeaderAction_Attack_Flank();

	virtual bool TimeOut();
	virtual eActionUpdateResult Update();
	//void	End();
	virtual bool FinalUpdate() {return false;};
	virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
private:
	bool ChooseFlankingUnits();
private:
	static const float	m_CSearchDistance;
	Vec3 m_vStartPos;
	int	m_iFlankingUnitsCount;
	int m_iFlankingUnits;
	int m_iStage;
	float m_fFlankDirection;
	Vec3 m_vAveragePos;
	bool m_bHiding;
};

//----------------------------------------------------------
// CLeaderAction_Attack_Chain: units place themselves in a line towards the enemy
// the closest unit to the enemy always fires, the rear units strafe and fire
class CLeaderAction_Attack_Chain: public CLeaderAction_Attack
{
	typedef enum 
	{
		STATUS_APPROACH,
		STATUS_ALIGNED,
		STATUS_STRAFE
	} TActionStatus;

public:
	CLeaderAction_Attack_Chain(CLeader* pLeader);
	//virtual ~CLeaderAction_Attack_Chain();
	virtual bool	TimeOut();
	virtual bool	FinalUpdate();
	virtual eActionUpdateResult	Update();
	virtual void	Serialize( TSerialize ser, CObjectTracker& objectTracker );
	virtual void	DeadUnitNotify(CAIActor* pObj);	
	virtual void	UpdateBeacon() const;
	virtual bool	ProcessSignal(const AISIGNAL& signal);

protected:
	void		SetStatus(TActionStatus status, float durationMin=-1, float durationMax=-1);
	inline bool		IsStatusExpired() const;
	Vec3		StrafeVector(const CAIActor* pUnit,float dirCoeff) const;
protected:
	static const float m_CDistanceStep;
	static const float m_CDistanceHead;
	Vec3	m_vUpdateEnemyPos;
	Vec3	m_vNearestEnemyPos;
	TActionStatus m_status;
	float	m_nextStatusTime;


};


typedef std::list< CObstacleRef > ListObstacleRefs;

class CLeaderAction_Search: public CLeaderAction
{
public:
	struct SSearchPoint
	{
		Vec3 pos;
		Vec3 dir;
		bool bReserved;
		bool bHideSpot;
		SSearchPoint() {bReserved = false; bHideSpot = false;}
		SSearchPoint(const Vec3& p,const Vec3& d): pos(p),dir(d) {bReserved = false;bHideSpot = false;}
		SSearchPoint(const Vec3& p,const Vec3& d, bool hideSpot): pos(p),dir(d) {bReserved = false;bHideSpot = hideSpot;}

		virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker )
		{
			ser.Value("pos",pos);
			ser.Value("dir",dir);
			ser.Value("bReserved",bReserved);
			ser.Value("bHideSpot",bHideSpot);
		}

	};
	typedef std::multimap<float,SSearchPoint> TPointMap;
	CLeaderAction_Search(CLeader* pLeader,const  LeaderActionParams& params);
	virtual ~CLeaderAction_Search();

	virtual bool	TimeOut() { return true; }
	virtual bool	FinalUpdate() { return false; }
	virtual eActionUpdateResult	Update();

	virtual bool ProcessSignal(const AISIGNAL& signal);
//	virtual void OnUpdateUnitTask(CUnitImg* unit);
	virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
private:
	//	bool SomeoneHasTarget();
	//	bool HasTarget(CAIObject* unit) const;
	void PopulateSearchSpotList(Vec3& initPos);

	CTimeValue	m_timeRunning;
	enum  { m_timeLimit = 20 };
	static const float	m_CSearchDistance;
	static const uint32 m_CCoverUnitProperties;
	int	m_iCoverUnitsLeft;
	Vec3 m_vEnemyPos;
	float		m_fSearchDistance;
	//SetObstacleRefs		m_Passed;
	//ListObstacleRefs	m_Obstacles;
	TPointMap m_HideSpots;
	bool m_bInitialized;
	int m_iSearchSpotAIObjectType;
	bool m_bUseHideSpots;
	CAIActor* pSelectedUnit;

};


class CLeaderAction_Follow: public CLeaderAction
{
private:
	//	std::vector<bool>	m_bTooDistant;
	//	std::vector<float>	m_TooDistantTime;
	//static const float	m_CMaxDist;
	//	static const float	m_CMaxTooDistantTime;
	//	float				m_fLastTime;
	//std::vector<float>	m_fPrevDistance;
public:
	// TO DO: create and destroy formation inside CLeaderAction_Follow
	CLeaderAction_Follow() {};
	CLeaderAction_Follow(CLeader* pLeader, const LeaderActionParams& params);
	virtual void SetUnitFollow(CUnitImg& unit);
	virtual bool ProcessSignal(const AISIGNAL& signal);
	virtual void ResumeUnit(CUnitImg& BusyUnit);
	virtual void BusyUnitNotify(CUnitImg& BusyUnit);
	virtual eActionUpdateResult Update();
	virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
	~CLeaderAction_Follow();
protected:
	void CheckLeaderNotMoving() ;
	void	CheckUnitsNotMoving();
	void CheckLeaderFiring() ;

	CTimeValue m_FiringTime;
	CTimeValue m_movingTime;
	CTimeValue m_groupMovingTime;
	bool m_bLeaderFiring;
	bool m_bLeaderMoving;
	bool m_bLeaderBigMoving;

	int	m_notMovingCount;
	Vec3 m_vLastUpdatePos;
	Vec3 m_vAverageLeaderMovingDir;
	static const float m_CUpdateDistance;

	string m_szFormationName;
	int m_iGroupID;
	Vec3 m_vTargetPos;
	CAIObject* m_pSingleUnit;
	bool m_bInitialized;
};
/*
class CLeaderAction_FollowHiding: public CLeaderAction_Follow
{
public:
CLeaderAction_FollowHiding(CLeader* pLeader, CUnitImg* pSingleUnit=NULL) ;
eActionUpdateResult Update();
~CLeaderAction_FollowHiding();
private:
virtual void SetUnitFollow(CUnitImg& unit);
virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );

static const float	m_CSearchDistance, m_CUpdateThreshold;
//	bool m_bForceUpdate;
CUnitImg* m_pSingleUnit;
Vec3	m_averagePos;
Vec3	m_lastLeaderUpdatePos;
};
*/
class CLeaderAction_Use: public CLeaderAction
{
private:
	AISignalExtraData m_Data;
public:
	CLeaderAction_Use(CLeader* pLeader, const AISignalExtraData& SignalData);
	CLeaderAction_Use(CLeader* pLeader) : CLeaderAction(pLeader) {};
	//	EAIUseAction GetUseType() {return static_cast<EAIUseAction>(m_Data.iValue);}

};

class CLeaderAction_Use_Vehicle: public CLeaderAction_Use
{
public:
	CLeaderAction_Use_Vehicle(CLeader* pLeader, const AISignalExtraData& SignalData);
	CLeaderAction_Use_Vehicle(CLeader* pLeader, const CAIObject* pVehicle,  const AISignalExtraData& SignalData);
	virtual eActionUpdateResult Update();
	virtual bool ProcessSignal(const AISIGNAL& signal);
	virtual void	Serialize( TSerialize ser, CObjectTracker& objectTracker );
	virtual void ResumeUnit(CUnitImg& unit);
	virtual void	DeadUnitNotify(CAIActor* pObj);	
private:
	Vec3 m_vInitPos;
	CAIObject* m_pDriver;
	EntityId m_VehicleID;
};
/*
class CLeaderAction_Use_PlantBomb: public CLeaderAction_Use
{
public:
CLeaderAction_Use_PlantBomb(CLeader* pLeader, const AISignalExtraData& SignalData);
};

class CLeaderAction_Use_Rpg: public CLeaderAction_Use
{
public:
CLeaderAction_Use_Rpg(CLeader* pLeader, const AISignalExtraData& SignalData);
};
*/

class CLeaderAction_Use_LeaveVehicle: public CLeaderAction
{
public:
	CLeaderAction_Use_LeaveVehicle(CLeader* pLeader);
};

//----------------------------------------------------------
// CLeaderAction_Attack_SwitchPositions: units stays around the targets and switch positions 
class CLeaderAction_Attack_SwitchPositions: public CLeaderAction_Attack
{
public:


	CLeaderAction_Attack_SwitchPositions(CLeader* pLeader, const LeaderActionParams& params);
	CLeaderAction_Attack_SwitchPositions() {};//used for derived classes' constructors
	virtual ~CLeaderAction_Attack_SwitchPositions();
	virtual bool	TimeOut() { return true; }
	virtual bool	FinalUpdate() { return false; }
	virtual eActionUpdateResult	Update();
	virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
	bool ProcessSignal(const AISIGNAL& signal);
	virtual void OnObjectRemoved(CAIObject* pObject);
	virtual void	AddUnitNotify(CAIActor* pUnit);
	void	UpdateBeaconWithTarget(const CAIObject* pTarget=NULL) const ;

protected:
	struct SDangerPoint
	{
		SDangerPoint() {};
		SDangerPoint(float t, float r, Vec3& p)
		{
			time = t;
			radius = r;
			point = p;
		};

		virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker)
		{
			ser.Value("time",time);
			ser.Value("radius",radius);
			ser.Value("point",point);
		}
		float time;
		float radius;
		Vec3	point;
	};
	struct STargetData
	{
		IAIObject* pTarget;
		IAISystem::ENavigationType navType;
		IAISystem::ENavigationType targetNavType;
		STargetData(): pTarget(NULL),navType(IAISystem::NAV_UNSET),targetNavType(IAISystem::NAV_UNSET) {}

		virtual void Serialize (TSerialize ser, CObjectTracker& objectTracker )
		{
			ser.EnumValue("navType",navType,IAISystem::NAV_UNSET,IAISystem::NAV_MAX_VALUE);
			ser.EnumValue("targetNavType",targetNavType,IAISystem::NAV_UNSET,IAISystem::NAV_MAX_VALUE);
			objectTracker.SerializeObjectPointer(ser,"pTarget",pTarget,false);
		}
	};
	
	typedef enum
	{
		AS_OFF,
		AS_WAITING_CONFIRM,
		AS_ON
	} eActionStatus;

	struct SSpecialAction
	{
		eActionStatus status;
		CAIActor* pOwner;
		CTimeValue lastTime;

		SSpecialAction() 
		{
			status = AS_OFF; 
			pOwner = NULL;
			lastTime.SetValue(0);
		}

		SSpecialAction(CAIActor* pA)
		{
			status = AS_OFF;
			pOwner = pA;
			lastTime.SetValue(0);
		}

		virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker)
		{
			objectTracker.SerializeObjectPointer(ser,"pOwner",pOwner,false);
			ser.Value("lastTime",lastTime);
			ser.EnumValue("status",status,AS_OFF, AS_ON);
		}

	};

	struct SPointProperties {

		SPointProperties() 
		{
			bTargetVisible = false;
			fAngle = 0.f;
			fValue = 0.f;
			point.Set(0,0,0);
			pOwner = NULL;
		}

		SPointProperties(Vec3& p) : point(p)
		{
			bTargetVisible = false;
			fAngle = 0.f;
			fValue = 0.f;
			pOwner = NULL;
		}

		virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker)
		{
			ser.Value("bTargetVisible",bTargetVisible);
			ser.Value("fAngle",fAngle);
			ser.Value("fValue",fValue);
			ser.Value("point",point);
			objectTracker.SerializeObjectPointer(ser,"pOwner",pOwner,false);
		}

		bool bTargetVisible;
		float fAngle;
		float fValue;
		Vec3 point;
		CAIObject* pOwner;

	};

	class CActorSpotListManager
	{
		typedef std::multimap<const CAIActor*,Vec3> TActorSpotList;
		
		TActorSpotList m_ActorSpotList;
	public:
		void AddSpot(const CAIActor* pActor,Vec3& point)
		{
			m_ActorSpotList.insert(std::make_pair(pActor,point));
		}

		void RemoveAllSpots(const CAIActor* pActor)
		{
			m_ActorSpotList.erase(pActor);
		}
		
		bool IsForbiddenSpot(const CAIActor* pActor, Vec3& point) 
		{
			TActorSpotList::iterator it = m_ActorSpotList.find(pActor), itEnd = m_ActorSpotList.end();
			while(it!=itEnd && it->first == pActor)
			{
				if(Distance::Point_PointSq(it->second,point)< 1.f)
					return true;

				++it;
			}
			return false;
		}

		virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker)
		{
			ser.BeginGroup("ActorSpotList");
			int size = m_ActorSpotList.size();
			ser.Value("size",size);
			TActorSpotList::iterator it = m_ActorSpotList.begin();
			if(ser.IsReading())
				m_ActorSpotList.clear();

			Vec3 point;
			const CAIActor* pUnitActor;
			char name[32];

			for(int i=0;i<size;i++)
			{
				sprintf(name,"ActorSpot_%d",i);
				ser.BeginGroup(name);
				{
					if(ser.IsWriting())
					{
						pUnitActor = it->first;
						point = it->second;
						++it;
					}
					objectTracker.SerializeObjectPointer(ser,"pActor",pUnitActor,false);
					ser.Value("point",point);

					if(ser.IsReading())
						m_ActorSpotList.insert(std::make_pair(pUnitActor,point));
				}
				ser.EndGroup();

			}
			ser.EndGroup();

		}
	};

	typedef std::vector<Vec3> TShootSpotList;
	typedef std::vector<SDangerPoint> TDangerPointList;
	typedef std::map<CAIActor*,STargetData> TMapTargetData; // deprecated
	typedef std::vector<SPointProperties> TPointPropertiesList;
	
	typedef std::multimap<const CAIActor*,SSpecialAction> TSpecialActionMap;

	int GetFormationPointWithTarget(CUnitImg& unit);
	void InitPointProperties();
	void	CheckNavType(CUnitImg& unit);
	void InitNavTypeData();
	STargetData* GetTargetData(CUnitImg& unit);
	void AssignNewShootSpot(CAIObject* pUnit, int i);
	void UpdatePointList(CAIObject* pTarget=NULL);
	bool UpdateFormationScale(CFormation* pFormation);
	void SetFormationSize(const CFormation* pFormation);
	Vec3 GetBaseDirection(CAIObject* pTarget, bool bUseTargetMoveDir=false);
	void GetBaseSearchPosition(CUnitImg& unit, CAIObject* pTarget,int method, float distance=0);
	void UpdateSpecialActions();
	bool IsVehicle(const CAIActor* pTarget, IEntity** ppVehicleEntity=NULL) const;
	bool IsSpecialActionConsidered(const CAIActor* pUnit, const CAIActor* pUnitLiveTarget) const ;
protected:


	TMapTargetData m_TargetData;
	TSpecialActionMap m_SpecialActions;
	//TShootSpotList m_AdditionalShootSpots;
	float m_fDistanceToTarget;
	float m_fMinDistanceToTarget;
	
	string m_sFormationType;
	TPointPropertiesList m_PointProperties;
	bool m_bVisibilityChecked;
	bool m_bPointsAssigned;
	bool m_bAvoidDanger;
	float m_fMinDistanceToNextPoint;
	static const float m_fDefaultMinDistance;
	TDangerPointList m_DangerPoints;
	Vec3 m_vUpdatePointTarget;
	float m_fFormationSize;
	float m_fFormationScale;
	CTimeValue m_lastScaleUpdateTime;

	IAIObject* m_pLiveTarget; // no need to be serialized, it's initialized at each update

	CActorSpotListManager m_ActorForbiddenSpotManager;
};


//----------------------------------------------------------
// CLeaderAction_Attack_Chase: units try to keep up with a moving target anticipating it 
class CLeaderAction_Attack_Chase: public CLeaderAction_Attack
{
public:


	CLeaderAction_Attack_Chase(CLeader* pLeader, const LeaderActionParams& params);
	CLeaderAction_Attack_Chase() {};//used for derived classes' constructors
	virtual ~CLeaderAction_Attack_Chase();
	virtual bool	TimeOut() { return true; }
	virtual bool	FinalUpdate() { return false; }
	virtual eActionUpdateResult	Update();
	virtual void Serialize( TSerialize ser, CObjectTracker& objectTracker );
	bool ProcessSignal(const AISIGNAL& signal);
	virtual void UpdateBeacon() const;
//	virtual void OnObjectRemoved(CAIObject* pObject);


protected:

	int GetFormationPointWithTarget(CUnitImg& unit);
	float GetTargetSpeed(IAIObject* pTarget) const;

	//TShootSpotList m_AdditionalShootSpots;

	string m_sFormationType;
	bool m_bPointsAssigned;
	float m_fPredictionTimeSpan;
	float m_fMinDistance;
	float m_fMinSpeed;
	float m_fUpdateFormationThr;
	Vec3	m_vOldPosition;
};

#endif __LeaderAction_H__
