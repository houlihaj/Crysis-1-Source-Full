/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
File name:   VehicleFinder.h
$Id$
Description: implementation of CVehicleFinder class

-------------------------------------------------------------------------
History:
- 06:05:2005	Created by Luciano Morpurgo

*********************************************************************/


#include "StdAfx.h"
#include "VehicleFinder.h"
#include "AIVehicle.h"
#include "IEntity.h"
#include <ISystem.h>
#include <I3DEngine.h>


CVehicleRequest::CVehicleRequest(CAIObject* pRequestor,Vec3 sourcePos, CAIObject* pTarget, Vec3 targetPos, int goal)
{
	m_bProcessed = true;
	m_bUpdated = true;

	m_pRequestor = pRequestor;

	if(pRequestor==NULL && sourcePos.IsZero() || pTarget==NULL && targetPos.IsZero())
		return;

	m_Position = (sourcePos.IsZero() ? pRequestor->GetPos() : sourcePos);

	m_pTarget = pTarget;
	m_goal = (EAIGoalType) goal;
	const float fRadius = 30;
	float	fRadiusSQR = fRadius*fRadius;

	for(AutoAIObjectIter it(GetAISystem()->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, AIOBJECT_VEHICLE)); it->GetObject(); it->Next())
	{
		IAIObject*	pAIObject = it->GetObject();
		CAIVehicle* pVehicle = pAIObject->CastToCAIVehicle();
		if (pVehicle)
		{
			Vec3 ob_pos = pVehicle->GetPos();
			float f = (ob_pos - m_Position).len2();
			//float fActivationRadius = (ai->second)->GetRadius();
			//fActivationRadius*=fActivationRadius;
			if ( f < fRadiusSQR) 
			{
				// TO DO: check if vehicle is usable and free

				//				if (pPuppet->m_mapDevaluedPoints.find(pVehicle) == pPuppet->m_mapDevaluedPoints.end())
				//				{
				//					if ((fActivationRadius>0) && (f>fActivationRadius))
				//						continue;
				//				}

				//vehicle is free and usable
				CVehicleReqItem newReqItem(pVehicle);
				m_vehicleItemList.push_back(newReqItem);
				// set final direction not important by now
				// TODO: consider final direction
				pVehicle->m_nMovementPurpose = 0;
				// TODO: what's this leader building parameter (3rd) ?
				// TODO: request path only if necessary (target is flying -> wouldn't be of much use to request a path to his current position)
				pVehicle->RequestPathTo(targetPos,Vec3(0,0,0),true,-1,1.0f,0.0f, pTarget);
				m_bProcessed = false;
				m_bUpdated = false;
			}
		}
	}
}

bool CVehicleRequest::Update()
{
	if(!m_bUpdated && !m_bProcessed)
	{
		m_bProcessed = true;
		for(TVehicleItemList::iterator itVr = m_vehicleItemList.begin();itVr!=m_vehicleItemList.end();)
		{
			CVehicleReqItem& req = *itVr;
			switch(req.GetPathRequestResult())
			{
			case PATHFINDER_NOPATH:
				// TO DO: check partial path if it's worth
				m_vehicleItemList.erase(itVr++);
				break;
			case PATHFINDER_PATHFOUND:
				// TO DO: compute cost
				req.SetStatus(VRS_OK);
				++itVr;
				break;
			default:
				m_bProcessed = false;
				++itVr;
				break;
			}
		}
		m_bUpdated = true;
	}
	return  m_bProcessed;
}


void CVehicleRequest::Finalize()
{
	std::multimap<int,CAIVehicle*> sortedVehicleMap;
	for(TVehicleItemList::iterator itv = m_vehicleItemList.begin();itv!=m_vehicleItemList.end();++itv)	
	{
		int cost = -1;
		CVehicleReqItem& vitem = *itv;
		switch(vitem.GetStatus())
		{
			case VRS_OK:
				cost = Evaluate(vitem);
				break;
			case VRS_OFFSET:
			default:
				break;
		}
		if(cost>=0)
		{
			sortedVehicleMap.insert(std::pair<int,CAIVehicle*>(cost,vitem.m_pVehicle));
		}
	}

	for(std::multimap<int,CAIVehicle*>::iterator itsvm = sortedVehicleMap.begin(); itsvm!=sortedVehicleMap.end();++itsvm)
		m_sortedVehicleList.push_back(static_cast<IAIObject*>(itsvm->second));

}

//int CVehicleRequest::Evaluate(CAIObject* pVehicle, const Vec3 &vPos, const Vec3& vTargetPos, IAIObject *pAIObjectTarget, int iGoalType )
int CVehicleRequest::Evaluate(CVehicleReqItem& vitem)
{
//	CAIVehicle *pIVehicleObj;
	CAIVehicle* pVehicle = vitem.m_pVehicle;

	std::vector<weaponParam_t> weaponparams;
	weaponparams.resize(10);
	
	int iScore =0;
	Vec3 vPos = pVehicle->GetPos();
	const float C_fMinHeightForFlying = 10;
	if(m_pTarget)
		m_TargetPosition = m_pTarget->GetPos();

	float fDiffHeight =   m_TargetPosition.z - vPos.z;

	IEntity* pEntity = static_cast<IEntity*>(pVehicle->GetEntity());
	IScriptTable* entityVTable = pEntity ? pEntity->GetScriptTable() : NULL;
	if(!entityVTable )
		return 0;

	CAIObject::ESubTypes vehicleType = pVehicle->GetSubType();
	float fTargetDistance = (vPos - m_TargetPosition).len();
	float fVehicleDistance = (pVehicle->GetPos() - vPos).len();

	////////////////// Get vehicle properties
	//check if it's a ground vehicle
	bool bGroundVehicle = (vehicleType == CAIObject::STP_CAR);

	//check if it has tracks
	bool bTrackedVehicle = false;
	if(bGroundVehicle)
	{
		SmartScriptTable trackTable;
		bTrackedVehicle = ( entityVTable->GetValue("Tracks",trackTable));
	}

	// Get offense power
	// to do : replace following code with the script-abstract interface which will be provided
	//	int offensePower = GetVehicleOffensePower(iVType);
	int fMaxDamage = 0;
	float fMaxRange = 0;
	IScriptTable* tSeatsArray=0;
	if(entityVTable->GetValue("Seats",tSeatsArray))
	{
		int iNumEntries = entityVTable->Count();
		for(int i=1; i<=iNumEntries; i++)
		{
			IScriptTable* tSeat=0;
			if(tSeatsArray->GetAt(i,tSeat))
			{
				IScriptTable* tWeaponArray=0;
				if(tSeat->GetValue("Weapons",tWeaponArray))
				{
					int iNumEntries2 = tWeaponArray->Count();
					for(int j=1; j<=iNumEntries2; j++)
					{
						IScriptTable* tWeapon=0;
						if(tWeaponArray->GetAt(j,tWeapon))
						{
							IScriptTable* tWeaponParams=0;
							if(tWeapon->GetValue("fm_default_params",tWeaponParams))
							{
								float fDamage;
								float fRange = 100;
								if(tWeaponParams->GetValue("damage",fDamage))
								{
									// to do: range
									weaponparams.push_back(weaponParam_t(fDamage,fRange));
									if(fDamage > fMaxDamage)
										fMaxDamage = (int)fDamage;
									if(fRange > fMaxRange)
										fMaxRange = fRange;
								}
							}
						}	
					}
				}
			}
		}
	}

	// Get defense power
	//int defensePower = GetVehicleDefensePower(iVType);
	//////////////////////////////////////////

	//check if the vehicle is closer to the target than to the driver
	bool bVehicleCloserToTarget = (fVehicleDistance < (vPos - m_TargetPosition).len());

	// check if the target is on water
	float fWaterLevel = gEnv->p3DEngine->GetWaterLevel(&m_TargetPosition); 
	float fGroundLevel =gEnv->p3DEngine->GetTerrainElevation(m_TargetPosition.x, m_TargetPosition.y);
	bool bIsTargetOverWater = (fWaterLevel - fGroundLevel) > 1.f;


	switch(vehicleType)
	{
	case CAIObject::STP_CAR:
		if(!bIsTargetOverWater)
			iScore+=10;
		break;

	case CAIObject::STP_BOAT:
		if(bIsTargetOverWater)
			iScore+=10;
		break;

	case CAIObject::STP_HELI:
		if(!m_TargetPosition.IsZero() && (fDiffHeight >C_fMinHeightForFlying))
			iScore+=10;
		if(bIsTargetOverWater)
			iScore+=10;
		break;

	}
	bool bCheckVehicleDistance = true;
	//the closest it is to the driver, the better is
	if(iScore && bCheckVehicleDistance) // the vehicle type isn't discarded
	{	
		int iDistanceScore = static_cast<int>(fVehicleDistance)/5;
		if(iDistanceScore >10) iDistanceScore = 10;
		iScore += 10 - iDistanceScore ;
	}	
	return 100-iScore; // (cost)
}


//////////////////////////////////////////////////////////////////////////
// CVehicleFinder functions
//////////////////////////////////////////////////////////////////////////


CVehicleFinder::~CVehicleFinder()
{
	Reset();
}

void CVehicleFinder::Reset()
{
	// delete all requests
	TRequestList::iterator itReq = m_pendingRequests.begin();
	for(;itReq!=m_pendingRequests.end();++itReq)
	{
		CVehicleRequest* request = *itReq;
		if(request)
			delete request;
	} 
	TRequestMap::iterator itPR = m_RequestsProcessed.begin();
	for(;itPR!=m_RequestsProcessed.end();++itPR)
	{
		CVehicleRequest* request = itPR->second;
		if(request)
			delete request;
	} 
	m_pendingRequests.clear();
	m_RequestsProcessed.clear();
}


void CVehicleFinder::Update()
{
FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI)
	TRequestList::iterator itReq = m_pendingRequests.begin();
	for(;itReq!=m_pendingRequests.end();++itReq)
	{
		CVehicleRequest* request = *itReq;
		request->m_bUpdated = false;
	}

	itReq = m_pendingRequests.begin();
	while(itReq!=m_pendingRequests.end())
	{
		CVehicleRequest* pRequest = *itReq;
		if(pRequest->Update())
		{
			pRequest->Finalize();
			m_RequestsProcessed.insert(std::pair<CAIObject*,CVehicleRequest*>(pRequest->GetRequestor(),pRequest));
			IAISignalExtraData* data = GetAISystem()->CreateSignalExtraData();
			if(pRequest->m_pTarget )
			{
				IEntity* pTargetEntity = static_cast<IEntity*>(pRequest->m_pTarget->GetEntity());
				if(pTargetEntity)
					data->SetObjectName(pTargetEntity->GetName());
				else
					data->SetObjectName(pRequest->m_pTarget->GetName());
			}
			
			data->point = pRequest->m_TargetPosition;
			data->iValue = pRequest->m_goal;

			CAIActor* pRequesterActor = CastToCAIActorSafe(pRequest->GetRequestor());
			if(pRequesterActor)
				pRequesterActor->SetSignal(1,"OnVehicleListProvided",NULL,data, g_crcSignals.m_nOnVehicleListProvided);
			m_pendingRequests.erase(itReq++);
		}
		else
			++itReq;
	}
}

bool CVehicleFinder::AddRequest(CAIObject* pAIRequestor, Vec3 &vSourcePos, CAIObject *pAIObjectTarget, const Vec3& vTargetPos, int iGoalType)
{
	if(pAIRequestor && IsValidRequest(pAIRequestor,vSourcePos,pAIObjectTarget,vTargetPos,iGoalType))// preliminary check: filter out pointless requests
	{
		CVehicleRequest* request = new CVehicleRequest( pAIRequestor,vSourcePos,pAIObjectTarget,vTargetPos,iGoalType);
		m_pendingRequests.push_back(request);
		TRequestMap::iterator itPr = m_RequestsProcessed.find(pAIRequestor);
		if(itPr!=m_RequestsProcessed.end())
		{
			CVehicleRequest* pRequest = itPr->second;
			if(pRequest)
				delete pRequest;
			m_RequestsProcessed.erase(itPr);
		}
	}
	else 
		return false;

	return true;
}


bool CVehicleFinder::IsValidRequest(CAIObject* pAIRequestor, const Vec3& vSourcePos, CAIObject *pAIObjectTarget, const Vec3& vTargetPos, int iGoalType)
{
	Vec3 vSourcePosition, vTargetPosition;
	if(vSourcePos.IsZero())
		vSourcePosition = pAIRequestor->GetPos(); // it's not passed by reference
	else
		vSourcePosition = vSourcePos;
	if(vTargetPos.IsZero() && pAIObjectTarget)
		vTargetPosition = pAIObjectTarget->GetPos(); // it's not passed by reference
	else
		vTargetPosition = vTargetPos;
	if((vSourcePosition - vTargetPosition).GetLength() <30 )//&& iGoalType!=AIGOALTYPE_ATTACK)
		return false;
	return true;
}


TAIObjectList* CVehicleFinder::GetSortedVehicleList(CAIObject* pAIRequestor)
{
	TRequestMap::iterator itR = m_RequestsProcessed.find(pAIRequestor);
	return (itR==m_RequestsProcessed.end() ? NULL: itR->second->GetSortedVehicleList());
}

