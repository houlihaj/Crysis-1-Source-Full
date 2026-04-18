/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
File name:   VehicleFinder.h
$Id$
Description: Header for the CVehicleFinder class

-------------------------------------------------------------------------
History:
- 06:05:2005	Created by Luciano Morpurgo

*********************************************************************/

#ifndef VEHICLE_FINDER_H
#define VEHICLE_FINDER_H

#if _MSC_VER > 1000
#pragma once
#endif

//#include "CAISystem.h"
#include "AIObject.h"
#include "AIVehicle.h"
#include "AgentParams.h"
#include <list>
#include <map>
#include <set>


//class CPipeUser;

typedef enum
{
	VRS_PENDING,
	VRS_OK,
	VRS_OFFSET,
	VRS_FAILED
} EVehicleRequestStatus;

//#define TRequestList std::list<CVehicleRequest*> 
//#define TRequestMap std::map<CAIObject*,CVehicleRequest*> 


class CVehicleReqItem
{
	public:
		CVehicleReqItem(): m_cost(0),m_pVehicle(NULL),m_status(VRS_PENDING) {};
		CVehicleReqItem(CAIVehicle* pVehicle): m_cost(0),m_pVehicle(pVehicle),m_status(VRS_PENDING) {};
		inline EVehicleRequestStatus GetStatus() {return m_status;}
		inline void SetStatus(EVehicleRequestStatus status) {m_status = status;}
		inline int GetCost() {return m_cost;}
		inline int GetPathRequestResult() {return m_pVehicle->m_nPathDecision;}
		CAIVehicle* m_pVehicle;
	private:
		int	m_cost;
		EVehicleRequestStatus m_status;

};
/*
class IVehicleEvaluator
{
	virtual int Evaluate(IAIVehicle* pVehicle,Vec3 startPos,  Vec3 targetPos, IAIObject* pTarget, int goal) = 0;
};*/

// This class is abstract, the implementation of Evaluate() function must be game-specific
class CVehicleRequest 
{
	typedef std::list<CVehicleReqItem> TVehicleItemList ;
	friend class CVehicleFinder;

public:
	//int Evaluate(CAIObject* pVehicle,const Vec3& startPos, const Vec3& targetPos, IAIObject* pTarget, int goal);
private:
	CAIObject* m_pRequestor;
	Vec3 m_Position;
	CAIObject* m_pTarget;
	Vec3 m_TargetPosition;
	EAIGoalType m_goal;
	TVehicleItemList m_vehicleItemList;
	TAIObjectList m_sortedVehicleList;
	bool m_bProcessed;
public:
	bool m_bUpdated;

public:
	CVehicleRequest(): m_pRequestor(NULL),m_Position(0,0,0),m_pTarget(NULL),m_TargetPosition(0,0,0),m_goal(AIGOALTYPE_UNDEFINED), m_bProcessed(true){	}

	CVehicleRequest(CAIObject* pRequestor,Vec3 position, CAIObject* pTarget, Vec3 targetPos, int goal);

	inline bool operator ==(const CAIObject* other) const
	{
		return (m_pRequestor == other);
	}

	int		Evaluate(CVehicleReqItem& vitem);
	inline CAIObject* GetRequestor() {return m_pRequestor;}
	void		Finalize();
	bool		Update();
	inline TAIObjectList* GetSortedVehicleList() {return &m_sortedVehicleList;}
	inline EAIGoalType GetGoalType() {return m_goal;}

};



class CVehicleFinder
{
	typedef std::list<CVehicleRequest*> TRequestList;
	typedef std::map<CAIObject*,CVehicleRequest*> TRequestMap;

	TRequestList m_pendingRequests;
	TRequestMap m_RequestsProcessed;
public: 
	CVehicleFinder() {}
	~CVehicleFinder();
	void	Update();
	void	Reset();
	bool	AddRequest(CAIObject *pAIRequestor, Vec3 &vSourcePos, CAIObject *pAIObjectTarget, const Vec3& vTargetPos, int iGoalType);
	TAIObjectList* GetSortedVehicleList(CAIObject *pAIRequestor);
private:
	bool IsValidRequest(CAIObject* pAIRequestor, const Vec3& vSourcePos, CAIObject *pAIObjectTarget, const Vec3& vTargetPos, int iGoalType);

};

class weaponParam_t
{
public:
	float damage;
	float range;
	weaponParam_t(float d, float r) { damage = d;range=r;};
	weaponParam_t() { damage = 0;range=0;};
};

#endif