#include "StdAfx.h"
#include "Formation.h"
#include "PipeUser.h"

#include "IEntity.h"
#include "AIObject.h"
#include "CAISystem.h"
#include <Cry_Math.h>
#include "ITimer.h"
//#include <malloc.h>
//#include <Cry_Matrix.h>
#include <ISystem.h>
#include <ISerialize.h>
#include "ObjectTracker.h"

#include "IConsole.h"
#include <IRenderer.h>
#include <IRenderAuxGeom.h>
#include <I3DEngine.h>

const float CFormation::m_fDISTANCE_UPDATE_THR = 0.3f;
const float CFormation::m_fSIGHT_WIDTH = 1.f;

CFormation::CFormation()
{
	m_bReservationAllowed = true;
	m_bInitMovement = true;
	m_bFollowingUnits = false;
	m_pPathMarker = 0;
	m_iSpecialPointIndex = -1;
	m_fScaleFactor = 1;
	m_bUpdate = true;
	SetUpdateThresholds(m_fDISTANCE_UPDATE_THR);
	m_fUpdateThreshold = m_fDISTANCE_UPDATE_THR;
	m_pReferenceTarget = NULL;
	m_fRepositionSpeedWrefTarget = 0;
	m_bForceReachable = false;
	m_orientationType = OT_MOVE;
	m_fMaxUpdateSightTime = 0;
	m_fMinUpdateSightTime = 0;
	m_fSightRotationRange = 0;

}


CFormation::~CFormation()
{
	ReleasePoints();
	delete m_pPathMarker;
	m_pPathMarker = 0;

	/*
	if(!m_FormationPoints.empty())
	{
		CAIObject* pOwner = GetPointOwner(0);
		if(pOwner)
		{
			if(pOwner->m_pFormation == this)
				pOwner->m_pFormation = NULL;
		}
	}
	*/
}

void CFormation::ReleasePoints(void)
{
	for(TFormationPoints::iterator itr(m_FormationPoints.begin()); itr!=m_FormationPoints.end(); ++itr)
	{
		CFormationPoint &curPoint(*itr);
		GetAISystem()->RemoveDummyObject( curPoint.m_pDummyTarget );
		GetAISystem()->RemoveDummyObject( curPoint.m_pWorldPoint );
	}
}

void CFormation::OnObjectRemoved(CAIObject* pAIObject)
{
	if (GetReferenceTarget() == pAIObject)
		SetReferenceTarget(NULL);
	FreeFormationPoint(pAIObject);
}


void CFormation::AddPoint(FormationNode& desc)
{
	CFormationPoint	curPoint;
	Vec3 pos = desc.vOffset;
	char name[100];
	int i = m_FormationPoints.size();
	sprintf(name,"FORMATION_%d",i);
	CAIObject *pFormationDummy = GetAISystem()->CreateDummyObject(name);
	pFormationDummy->SetSubType(CAIObject::STP_FORMATION);

//	pFormationDummy->m_Parameters.m_bSpeciesHostility = false;

	curPoint.m_pWorldPoint = pFormationDummy;
	curPoint.m_vSight = desc.vSightDirection;
	int iClass = desc.eClass;
	if(iClass == SPECIAL_FORMATION_POINT)
		m_iSpecialPointIndex = i;
	curPoint.m_Class = iClass;

	curPoint.m_FollowHeightOffset = desc.fFollowHeightOffset;
	curPoint.m_FollowDistance = desc.fFollowDistance;
	curPoint.m_FollowOffset = desc.fFollowOffset;
	curPoint.m_FollowDistanceAlternate = desc.fFollowDistanceAlternate;
	curPoint.m_FollowOffsetAlternate = desc.fFollowOffsetAlternate;

	if(curPoint.m_FollowDistance>0)
	{
		if (m_fMaxFollowDistance < curPoint.m_FollowDistance)
			m_fMaxFollowDistance = curPoint.m_FollowDistance;

		m_bFollowingUnits = true;
		pos = Vec3(curPoint.m_FollowOffset,curPoint.m_FollowDistance,curPoint.m_FollowHeightOffset);
	}
	/*else if(m_fMaxYOffset < pos.y)
	{ // find the foremost formation unit to be followed
	m_fMaxYOffset = pos.y;
	m_iForemostNodeIndex = i;
	}*/
	curPoint.m_vPoint = pos;

  sprintf(name,"FORMATION_DUMMY_TARGET_%d",i);
	CAIObject *pNewDummyTarget = GetAISystem()->CreateDummyObject(name);
	curPoint.m_pDummyTarget = pNewDummyTarget;
	m_FormationPoints.push_back(curPoint);

}

// fills the formation class with all necessary information
void CFormation::Create(CFormationDescriptor & desc, CAIObject* pOwner, Vec3 vTargetPos)
{
	CFormationDescriptor::TVectorOfNodes::iterator vi;
	int i=0;
	m_fMaxFollowDistance = 0;
	//float m_fMaxYOffset = -1000000;
	//m_iForemostNodeIndex = -1;
	m_szDescriptor = desc.m_sName;

	for (vi=desc.m_Nodes.begin();vi!=desc.m_Nodes.end();++vi)
	{
		FormationNode& desc = *vi;
		AddPoint(desc);
	}

	// clamp max distance to a minimum decent value
	if(m_fMaxFollowDistance<1)
		m_fMaxFollowDistance=1; 

	/*if(m_iForemostNodeIndex<0)
		m_bFollowingUnits = false; // if it was true but no foremost node, it means that all the nodes
									// were set to follow
	*/
	m_bInitMovement = true;
	m_bFirstUpdate = true;
	m_bFollowPointInitialized = false;
	m_vInitialDir = vTargetPos.IsZero() ? ZERO: (vTargetPos - pOwner->GetPos()).GetNormalizedSafe();
	m_vMoveDir = m_vInitialDir;

	m_dbgVector1.Set(0,0,0);
	m_dbgVector2.Set(0,0,0);

	m_vLastUpdatePos = pOwner->GetPos();
	m_vLastPos	= pOwner->GetPos();
	m_vLastTargetPos = ZERO;
	m_vLastMoveDir = pOwner->GetMoveDir();
	m_fLastUpdateTime = 0.f;//GetAISystem()->GetCurrentTime();
	m_vDesiredTargetPos = ZERO;

//	m_FormationPoints[0].m_pReservation = pOwner;
	m_pOwner = pOwner;
	m_FormationPoints[0].m_pReservation = pOwner->CastToCAIActor();

	if(m_bFollowingUnits )
	{
		if (m_pPathMarker)
		{
			delete m_pPathMarker;
			m_pPathMarker = 0;
		}
		m_pPathMarker = new CPathMarker(m_fMaxFollowDistance + 10, m_fDISTANCE_UPDATE_THR/2);
		AIAssert(m_pPathMarker);
		m_vInitialDir.z = 0; // only in 2D
		m_vInitialDir.NormalizeSafe();
//		m_pPathMarker->Init(pOwner->GetPos() , pOwner->GetPos()- m_fMaxFollowDistance*(m_vInitialDir.IsZero() ? pOwner->GetMoveDir() : m_vInitialDir));
		m_pPathMarker->Init(pOwner->GetPos(), pOwner->GetPos() - pOwner->GetBodyDir() * 0.5f);
	}
	
	m_fSightRotationRange = 0;
	
	InitWorldPosition(m_vInitialDir);
}

//-----------------------------------------------------------------------------------------------------------------
//

float CFormation::GetMaxWidth()
{
	int s = m_FormationPoints.size();
	float maxDist = 0;
	for(int i=0; i < s; i++)
		for(int j=i+1; j < s; j++)
		{
			float dist = Distance::Point_PointSq(m_FormationPoints[i].m_vPoint,m_FormationPoints[j].m_vPoint);
			if(dist>maxDist)
				maxDist = dist;
		}
	return sqrt(maxDist);
}

//-----------------------------------------------------------------------------------------------------------------
//

bool CFormation::GetPointOffset(int i, Vec3& ret)
{
	if(i >= GetSize()) 
		return false;
	CFormationPoint& point = m_FormationPoints[i];
	if(point.m_FollowDistance !=0)
	{
		ret.x = point.m_FollowOffset;
		ret.y = point.m_FollowDistance;
		ret.z = point.m_FollowHeightOffset;
	}
	else
	{
		ret = point.m_vPoint;
	}
	ret *= m_fScaleFactor;
	return true;

}

//-----------------------------------------------------------------------------------------------------------------
//
Vec3 CFormation::GetPointSmooth(const Vec3& headPos, float followDist, float sideDist, float heightValue, int smoothValue, Vec3& smoothPosAlongCurve,CAIObject* pUser )
{
	Vec3 thePos(0,0,0),  smoothOffset(0,0,0);//,smoothPosAlongCurve(0,0,0);
	float	smoothD = .5f;
	float	weight = smoothValue;
	Vec3 peppe = m_pPathMarker->GetPointAtDistance(headPos,followDist);
	followDist -= smoothD*smoothValue/2;
	if( followDist < 0 )
		followDist = 0;
	smoothPosAlongCurve.Set(0,0,0);
	for(int cnt=0; cnt<smoothValue; ++cnt)
	{
		const float	d = followDist+smoothD*cnt;
		Vec3 vFollowDir;
		Vec3 vFollowPos;
		vFollowPos = m_pPathMarker->GetPointAtDistance(headPos,d);
		vFollowDir = m_pPathMarker->GetDirectionAtDistance(headPos,d);
		Vec3 vHOffset;
		vHOffset.x = -vFollowDir.y * sideDist;
		vHOffset.y = vFollowDir.x * sideDist;
		vHOffset.z = heightValue;
		smoothPosAlongCurve += vFollowPos;
		smoothOffset += vHOffset;
		//weight += 1.0f;
	}
//	if(weight > 0)
	{
		smoothPosAlongCurve/=weight;
		smoothOffset/=weight;
	}

//	float dist = Distance::Point_Point(smoothPosAlongCurve,peppe);
	thePos = smoothPosAlongCurve + smoothOffset;

/*	if(pUser)
	{
		// check if the point is reachable by its user
		Vec3 actualPos;
		if(GetAISystem()->IsPathWorth( pUser, smoothPosAlongCurve,thePos,2.5,0.5,&actualPos))
			thePos = actualPos;
		else
			thePos = smoothPosAlongCurve;
	}
*/
	return thePos;
}

//====================================================================
// MoveFormationPointOutOfVehicles
//====================================================================
static void MoveFormationPointOutOfVehicles(Vec3 &pos)
{
	IAIObject *obstVehicle = GetAISystem()->GetNearestObjectOfTypeInRange( pos, AIOBJECT_VEHICLE, 0, 10, NULL, AIFAF_INCLUDE_DISABLED );
	if (!obstVehicle)	// nothing to steer away from
		return;

	// get bounding rectangle of the vehicle width length 
	Vec3 FL, FR, BL, BR;
	obstVehicle->GetProxy()->GetWorldBoundingRect(FL, FR, BL, BR, 1.0f);

	// zero heights
	FL.z = FR.z = BL.z = BR.z = 0.0f;

	Vec3 fwd = (FL - BL);
	fwd.NormalizeSafe();
	Vec3 right = (FR - FL);
	right.NormalizeSafe();

	// +ve dist means we're inside
	float distToFront =  (FL - pos) | fwd;
	float distToBack  = -(BL - pos) | fwd;
	float distToRight =  (FR - pos) | right;
	float distToLeft  = -(FL - pos) | right;

	if (distToFront < 0.0f || distToBack < 0.0f ||
			distToRight < 0.0f || distToLeft < 0.0f)
			return;

	// inside - choose the smallest distance - not the most elegant way
	float bestDist = distToFront;
	Vec3 newPos = pos + fwd * distToFront;
	if (distToBack < bestDist)
	{
		bestDist = distToBack;
		newPos = pos - fwd * distToBack;
	}
	if (distToRight < bestDist)
	{
		bestDist = distToRight;
		newPos = pos + right * distToRight;
	}
	if (distToLeft < bestDist)
	{
		bestDist = distToLeft;
		newPos = pos - right * distToLeft;
	}
	pos = newPos;
}


// Update of the formation (refreshes position of formation points)
void CFormation::Update()
{
/* *	CAIObject *pOwner;
	if(m_FormationPoints.size())
		pOwner = m_FormationPoints[0].m_pReservation;
	else return;
	if(!pOwner)
		return;
*/
	if(!m_pOwner)
		return;
	if (!m_bReservationAllowed)
		return;

	Vec3 vUpdateMoveDir;
	Vec3 vUpdateTargetDir;
	Vec3 vMoveDir;

	TFormationPoints::iterator itFormEnd = m_FormationPoints.end();
	if(	m_bFirstUpdate )
	{
		m_vLastMoveDir = m_pOwner->GetMoveDir();
		m_bFirstUpdate = false;
		m_fLastUpdateTime = 0.0f;//GetAISystem()->GetFrameStartTime();
		for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint!=itFormEnd; ++itrPoint)
		{
			if(itrPoint->m_pWorldPoint)
			{
				itrPoint->m_pWorldPoint->SetAssociation(m_pOwner);
				itrPoint->m_pWorldPoint->Event(AIEVENT_DISABLE, NULL);
			}
		}
	}

		// if it's in init movement, we don't consider the update threshold, but any movement!=0
	CTimeValue fCurrentTime = GetAISystem()->GetFrameStartTime();
	float fUpdateTime = (fCurrentTime - m_fLastUpdateTime).GetSeconds();
	if (fUpdateTime < 0.01f)
		return;

	Vec3 vOwnerPos = m_pOwner->GetPhysicsPos();

	vUpdateMoveDir = vOwnerPos - m_vLastUpdatePos;
	float fMovement = vUpdateMoveDir.GetLength();

	vMoveDir = vUpdateMoveDir;

	if(m_bUpdate)
	{
		m_vLastPos	= vOwnerPos;

		if(m_pReferenceTarget)
		{
			vUpdateTargetDir = m_pReferenceTarget->GetPos() - m_vLastTargetPos;
			
			float fTargetMovement = vUpdateTargetDir.GetLength();
			if(fTargetMovement>fMovement)
				fMovement = fTargetMovement;
			if(m_fRepositionSpeedWrefTarget==0 || m_vDesiredTargetPos.IsZero())
				m_vDesiredTargetPos = m_pReferenceTarget->GetPos();
			else
			{
				Vec3 moveDir = (m_pReferenceTarget->GetPos() - m_vDesiredTargetPos);
				float length = moveDir.GetLength();
				if(length<0.1f)
					m_vDesiredTargetPos = m_pReferenceTarget->GetPos();
				else
				{
					moveDir.NormalizeSafe();
					float dist = (m_pReferenceTarget->GetPos() - vOwnerPos).GetLength();
					m_vDesiredTargetPos +=moveDir * min(length,m_fRepositionSpeedWrefTarget/10*dist);
				}
			}
			m_vMoveDir = m_vDesiredTargetPos - vOwnerPos;
			m_vMoveDir.NormalizeSafe();
			if(m_vMoveDir.x==0 && m_vMoveDir.y==0)//2D only - .IsZero())
			{
				m_vMoveDir = m_pReferenceTarget->GetMoveDir();
				m_vMoveDir.z = 0;
				m_vMoveDir.NormalizeSafe();
			}
		}
/*		else
		{
			m_vMoveDir = vMoveDir;
			if(m_vMoveDir.IsEquivalent(ZERO))
				m_vMoveDir = m_vLastMoveDir;
			m_vMoveDir.NormalizeSafe();
		}
		if(m_vMoveDir.IsZero())
			m_vMoveDir = pOwner->GetMoveDir();*/
	}

	
	if( !m_bUpdate || fMovement< m_fUpdateThreshold && !m_bInitMovement && m_fRepositionSpeedWrefTarget==0)
	{
		// always update special formation points
		Vec3 formDirXAxis(m_vMoveDir.y, -m_vMoveDir.x, 0);
		if(m_iSpecialPointIndex>=0)
		{
			CFormationPoint& frmPoint = m_FormationPoints[m_iSpecialPointIndex];
			Vec3 pos = frmPoint.m_vPoint;
			CAIObject *pFormationDummy =  frmPoint.m_pWorldPoint;
			Vec3 posRot;
			Vec3 vY = m_pOwner->GetMoveDir();
			Vec3 vX(vY.y,-vY.x,0);
			vX.NormalizeSafe();

			posRot = pos.x * vX + pos.y*vY;
			pos = posRot + vOwnerPos; 

			frmPoint.SetPos(pos,vOwnerPos,m_bForceReachable);

			CAIObject* pDummyTarget =frmPoint.m_pDummyTarget;
			if(pDummyTarget)
			{
				Vec3 vY(posRot);
				Vec3 vX(vY.y,-vY.x,0);
				vX.NormalizeSafe();
				vY.NormalizeSafe();
				Vec3 vDummyPosRelativeToOwner = frmPoint.m_vPoint + frmPoint.m_vSight; // in owner's space
				Vec3 posRot = vDummyPosRelativeToOwner.x * vX + vDummyPosRelativeToOwner.y * vY;
				Vec3 vDummyWorldPos = posRot + vOwnerPos;
				// TO DO: set the sight dummy target position relative to the point's user if it's close enough
				vDummyWorldPos.z = pos.z;
				pDummyTarget->SetPos(vDummyWorldPos);
			}

		}

		// randomly rotate the look-at dummy targets when formation is not moving
		if(m_fSightRotationRange>0)
		{
			for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint!=itFormEnd; ++itrPoint)
			{
				CFormationPoint	&frmPoint = (*itrPoint);
				float fUpdateTime = (fCurrentTime - frmPoint.m_fLastUpdateSightTime).GetSeconds();
				if(fUpdateTime > (m_fMaxUpdateSightTime - m_fMinUpdateSightTime)*rnd() + m_fMinUpdateSightTime) 
				{
					frmPoint.m_fLastUpdateSightTime = fCurrentTime;
					Vec3 pos = frmPoint.m_vPoint;// m_vPoints[i];

					CAIObject* pDummyTarget = frmPoint.m_pDummyTarget;
					if(pDummyTarget)
					{
						if(m_pReferenceTarget && frmPoint.m_Class != SPECIAL_FORMATION_POINT)
						{
							pDummyTarget->SetPos(m_pReferenceTarget->GetPos());
						}
						else
						{
							float angle = (rnd()-0.5f)*m_fSightRotationRange;
							Vec3 vSight = frmPoint.m_vSight.GetRotated(Vec3Constants<float>::fVec3_OneZ,angle);
							Vec3 vDummyPosRelativeToOwner = frmPoint.m_vPoint + vSight; // in owner's space
							Vec3 posRot = vDummyPosRelativeToOwner.x * formDirXAxis + vDummyPosRelativeToOwner.y * m_vMoveDir;
							Vec3 vDummyWorldPos = posRot + vOwnerPos;
							// TO DO: set the sight dummy target position relative to the point's user if it's close enough
							vDummyWorldPos.z = pos.z;
							pDummyTarget->SetPos(vDummyWorldPos);
						}
					}
				}
			}
		}
		
		m_fLastUpdateTime = fCurrentTime;
		return; 

	}

/*	if(vUpdateMoveDir.GetNormalizedSafe().Dot(m_vLastMoveDir)<=0)
		m_fUpdateThreshold = m_fStaticUpdateThreshold;*/

	// formation's owner has moved

	bool bInvertDirection = false;

	if(!m_pReferenceTarget)
	{
/*		m_vMoveDir = vMoveDir;
		if(m_vMoveDir.IsEquivalent(ZERO))
			m_vMoveDir = m_vLastMoveDir;
		m_vMoveDir.NormalizeSafe();
		if(m_vMoveDir.IsZero())
			m_vMoveDir = pOwner->GetMoveDir();
	*/
		CAIObject* pReservation0 = m_FormationPoints[0].m_pReservation;
		CAIObject* pEntityOwner = m_pOwner->CastToCAIActor() || !pReservation0 ? m_pOwner : pReservation0;
		m_vMoveDir = m_orientationType == OT_MOVE ? pEntityOwner->GetMoveDir() : pEntityOwner->GetViewDir();
		m_vMoveDir.z=0;
		m_vMoveDir.NormalizeSafe();
		if(m_vMoveDir.IsEquivalent(ZERO))
			m_vMoveDir = (vOwnerPos - m_vLastUpdatePos).GetNormalizedSafe();
		if(m_vMoveDir.IsEquivalent(ZERO))
			m_vMoveDir = m_pOwner->GetViewDir();

			Vec3 vUpdateMoveDirN = vUpdateMoveDir.GetNormalizedSafe();
			if(vUpdateMoveDirN.Dot(m_vLastMoveDir.GetNormalizedSafe()) < -0.04f)
			{
				//formation is inverting direction, flip it 
				if(m_pPathMarker)
					m_pPathMarker->Init(vOwnerPos, vOwnerPos - m_fMaxFollowDistance* vUpdateMoveDirN );
				for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint!=itFormEnd; ++itrPoint)
				{
					CFormationPoint	&frmPoint = (*itrPoint);
					frmPoint.m_FollowOffset = - frmPoint.m_FollowOffset;
					frmPoint.m_vPoint.x = -frmPoint.m_vPoint.x;
					frmPoint.m_vSight.x = -frmPoint.m_vSight.x;
				}
				bInvertDirection = true;
			}
		}
	
	m_fUpdateThreshold = m_fDynamicUpdateThreshold;
	m_fLastUpdateTime = fCurrentTime;

	if(!vUpdateMoveDir.IsEquivalent(ZERO)) 
			m_vLastMoveDir = vUpdateMoveDir;
	m_vLastUpdatePos = vOwnerPos;

	if(m_pReferenceTarget)
		m_vLastTargetPos = m_pReferenceTarget->GetPos();

	m_bInitMovement = false;

	Vec3 formDirXAxis(m_vMoveDir.y, -m_vMoveDir.x, 0);
	//formDirXAxis.x = m_vMoveDir.y;
	//formDirXAxis.y = -m_vMoveDir.x;
	//formDirXAxis.z = 0;

	static float speedUpdateFrac = 0.1f;
	bool bFormationOwnerPoint = true;
	//first update the fixed offset points
	for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint!=itFormEnd; ++itrPoint)
	{
		CFormationPoint	&frmPoint = (*itrPoint);
		Vec3 pos = frmPoint.m_vPoint;// m_vPoints[i];
		CAIObject *pFormationDummy = frmPoint.m_pWorldPoint;// m_vWorldPoints[i];
		frmPoint.m_fLastUpdateSightTime = fCurrentTime;


		//Update nodes
		if(!(m_bFollowingUnits && frmPoint.m_FollowDistance>0))// m_vFollowDistances[i]>0 ))
		{	// fixed offset node
			Vec3 posRot;
			if(bFormationOwnerPoint )
			{
				//force the formation owner point to be where the owner is - skip all the checks
				frmPoint.m_pWorldPoint->SetPos(vOwnerPos);
				bFormationOwnerPoint = false;
				continue;
			}

			posRot = pos.x * formDirXAxis + pos.y*m_vMoveDir;
			pos = posRot + vOwnerPos; 
			// Make formation 3D!!!
			pos.z=vOwnerPos.z+frmPoint.m_vPoint.z;
			frmPoint.m_LastPosition  = pFormationDummy->GetPos();
			frmPoint.m_Dir = (pos - frmPoint.m_LastPosition)/fUpdateTime;
			frmPoint.SetPos(pos,vOwnerPos,m_bForceReachable);
			
			if (!pFormationDummy->IsEnabled())
				pFormationDummy->Event(AIEVENT_ENABLE, NULL);

			// blend the speed estimation to smooth it since it's very noisy
			float newSpeed = (frmPoint.m_LastPosition - pos).GetLength()/fUpdateTime;
			frmPoint.m_Speed = speedUpdateFrac * newSpeed + (1.0f - speedUpdateFrac) * frmPoint.m_Speed;

/*			formDirXAxis.x = m_vMoveDir.y;
			formDirXAxis.y = -m_vMoveDir.x;
			formDirXAxis.z = 0;
	*/		
			//Update dummy targets

			//rotate sighting
			CAIObject* pDummyTarget = frmPoint.m_pDummyTarget;
			if(pDummyTarget)
			{
				if(m_pReferenceTarget && frmPoint.m_Class != SPECIAL_FORMATION_POINT)
				{
					pDummyTarget->SetPos(m_pReferenceTarget->GetPos());
				}
				else
				{
					Vec3 vSight = frmPoint.m_vSight;
					Vec3 vDummyPosRelativeToOwner = frmPoint.m_vPoint + vSight; // in owner's space
					Vec3 posRot = vDummyPosRelativeToOwner.x * formDirXAxis + vDummyPosRelativeToOwner.y * m_vMoveDir;
					Vec3 vDummyWorldPos = posRot + vOwnerPos;
					// TO DO: set the sight dummy target position relative to the point's user if it's close enough
					vDummyWorldPos.z = pos.z;
					pDummyTarget->SetPos(vDummyWorldPos);
				}
			}
		}
	}

	if(m_bFollowingUnits)
	{
		TFormationPoints::iterator itrPoint(m_FormationPoints.begin());
		
		m_pPathMarker->Update(vOwnerPos, true);// true if 2D movement

		bool bPlayerLeader = (m_pOwner && m_pOwner->GetAIType()==AIOBJECT_PLAYER );

	// then update following-type nodes and sights
		for (; itrPoint!=itFormEnd; ++itrPoint)
		{
			CFormationPoint	&frmPoint = (*itrPoint);
			Vec3 pos = frmPoint.m_vPoint;
			CAIObject *pFormationDummy = frmPoint.m_pWorldPoint;

			if(frmPoint.m_FollowDistance>0 )
			{ 
				float fDistance = frmPoint.m_FollowDistance;

				float fTotalDistance = m_pPathMarker->GetTotalDistanceRun();

				Vec3 vFollowDir;
				if (fTotalDistance < 1.0f || fTotalDistance < fDistance)
				{
					// Keep the point disabled until it is possible to update it properly.
					if (frmPoint.m_pWorldPoint->IsEnabled())
						frmPoint.m_pWorldPoint->Event(AIEVENT_DISABLE, NULL);

					if (frmPoint.m_pReservation)
					{
						// Only update the point after it has been reserved so that the formation point
						// assignment can find the nearest points.
						frmPoint.SetPos(vOwnerPos, vOwnerPos, true, bPlayerLeader);
					}

					frmPoint.m_Speed = 0;

					vFollowDir = m_vMoveDir;
				}
				else
				{
					// There is enough path markers to update the point.
					if (!frmPoint.m_pWorldPoint->IsEnabled())
						frmPoint.m_pWorldPoint->Event(AIEVENT_ENABLE, NULL);

					// follow-type node
					vFollowDir = m_pPathMarker->GetDirectionAtDistance(vOwnerPos,fDistance);

					int numSmoothPasses = 5;

					//don't smooth formation for squad-mates
					if (m_pOwner->CastToCAIPlayer())
						numSmoothPasses = 1;

					if (vMoveDir.GetLengthSquared()>.01f && (vOwnerPos - pFormationDummy->GetPos()).Dot(vMoveDir)>0.f)
					{
						Vec3 pointAlongLeaderPath;
						Vec3 smoothedPos = GetPointSmooth(vOwnerPos, fDistance, frmPoint.m_FollowOffset, frmPoint.m_FollowHeightOffset, numSmoothPasses,pointAlongLeaderPath,frmPoint.m_pReservation);
						frmPoint.SetPos(smoothedPos,pointAlongLeaderPath,m_bForceReachable,bPlayerLeader);
					}

					float newSpeed = (frmPoint.m_LastPosition - pFormationDummy->GetPos()).GetLength() / fUpdateTime;
					frmPoint.m_Speed = speedUpdateFrac * newSpeed + (1.0f - speedUpdateFrac) * frmPoint.m_Speed;
				}

				frmPoint.m_Dir = (pFormationDummy->GetPos() - frmPoint.m_LastPosition)/fUpdateTime ;
				frmPoint.m_LastPosition = frmPoint.m_pWorldPoint->GetPos();

				//rotate 90 degrees
				formDirXAxis.x = vFollowDir.y;
				formDirXAxis.y = -vFollowDir.x;
				formDirXAxis.z = 0;
				//Update dummy targets
				Vec3 vSight = frmPoint.m_vSight;
				//rotate sighting

				Vec3 pos = pFormationDummy->GetPos();

				CAIObject* pDummyTarget = frmPoint.m_pDummyTarget;
				if(pDummyTarget)
				{
					Vec3 posRot = vSight.x * formDirXAxis + vSight.y * vFollowDir;
					Vec3 vDummyWorldPos = posRot + frmPoint.m_pWorldPoint->GetPos();
					// TO DO: set the sight dummy target position relative to the point's owner if it's close enough
					vDummyWorldPos.z = pos.z;
					pDummyTarget->SetPos(vDummyWorldPos, vFollowDir);
				}

			}
		}
	}	
}


void CFormationPoint::SetPos(Vec3 pos, const Vec3& startPos, bool force, bool bPlayerLeader)
{
	if(!m_pReservation && !force)
	{
		m_pWorldPoint->SetPos(pos);
		return;
	}

	if(!bPlayerLeader && m_FollowDistance>0 && m_FollowOffset<0.2f)
	{	//optimization: point is on a walked path (offset too low)
		m_pWorldPoint->SetPos(pos);
		return;
	}

	// this line would replace all the code below
	//m_pWorldPoint->SetReachablePos(pos,m_pReservation,startPos);


	int building;
	IVisArea *pArea;

	IAISystem::tNavCapMask navMask;
	float fPassRadius;
	if(m_pReservation)
	{
		navMask = m_pReservation->GetMovementAbility().pathfindingProperties.navCapMask ;
		fPassRadius = m_pReservation->GetParameters().m_fPassRadius;
	}
	else
	{
		navMask = 0xff;
		fPassRadius = 0.6f;
	}
	IAISystem::ENavigationType navType =  GetAISystem()->CheckNavigationType(startPos,building,pArea,navMask);
	switch(navType)
	{
	case IAISystem::NAV_TRIANGULAR:
		{
			const float extraRad = GetAISystem()->m_cvExtraForbiddenRadiusDuringBeautification->GetFVal();
			const float fDistanceToEdge = fPassRadius + 0.2f; // + extraRad + 0.3f;
			pos = GetAISystem()->GetPointOutsideForbidden(pos,fDistanceToEdge,&startPos);
			break;
		}
	case IAISystem::NAV_WAYPOINT_HUMAN:
	case IAISystem::NAV_WAYPOINT_3DSURFACE:
		{
			ray_hit hit;
			IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
			int rwiResult = pWorld->RayWorldIntersection(startPos, pos-startPos, ent_static|ent_rigid|ent_sleeping_rigid|ent_terrain, HIT_COVER, &hit, 1);
			if(rwiResult>0)
				pos = hit.pt + hit.n * (fPassRadius+0.2f);
			Vec3 actualPos = pos;
			if(m_pReservation && GetAISystem()->IsPathWorth( m_pReservation, startPos,pos,1.5f,10000,&actualPos))
				pos = actualPos;
		}
		break;
	default:
		break;
	}

	m_pWorldPoint->SetPos(pos);
}

// returns an available formation point, if that exists right now by proximity
CAIObject * CFormation::GetNewFormationPoint(CAIActor *pRequester ,int index)
{
	CAIObject *pPoint = NULL;

	int size = m_FormationPoints.size();
	if (index >= size)
		return NULL;

	if(index<0)		
	{
		float mindist = FLT_MAX;
		const Vec3& requesterPos = pRequester->GetPos();
		for (int i=0;i<size;i++)
		{
			CFormationPoint &curFormationPoint(m_FormationPoints[i]);
			CAIObject* pThisPoint = curFormationPoint.m_pWorldPoint;
			if (curFormationPoint.m_pReservation == pRequester)	// 
				return pThisPoint;
			if (curFormationPoint.m_Class == SPECIAL_FORMATION_POINT)	
				continue;
			if (!curFormationPoint.m_pReservation)
			{
				float dist = Distance::Point_PointSq(pThisPoint->GetPos(), requesterPos);
				if (dist < mindist)
				{
					index = i;
					mindist = dist;
					pPoint = pThisPoint;
				}
			}
		}
	}
	else if (m_FormationPoints[index].m_pReservation == 0 || m_FormationPoints[index].m_pReservation==pRequester )
	{
		pPoint = m_FormationPoints[index].m_pWorldPoint;
	}
	if(index>=0)
  {
		m_FormationPoints[index].m_pReservation = pRequester;
    pPoint = m_FormationPoints[index].m_pWorldPoint;
		SetUpdate(true);
  }
	return pPoint;
}



int CFormation::GetClosestPointIndex(CAIActor *pRequester ,bool bReserve,int maxSize, int iClass, bool bCLosestToOwner ) 
{
	CAIObject *pPoint = NULL;

	int size = (maxSize==0 ? m_FormationPoints.size() : maxSize);
	int index = -1;
	float mindist = 2000000.f;
	Vec3 basePos = bCLosestToOwner ? m_FormationPoints[0].m_pWorldPoint->GetPos() : pRequester->GetPos();
	for (int i=1;i<size;i++)
		if((iClass <0 && m_FormationPoints[i].m_Class !=SPECIAL_FORMATION_POINT)|| (m_FormationPoints[i].m_Class & iClass))
		{
			if (!m_FormationPoints[i].m_pReservation || m_FormationPoints[i].m_pReservation == pRequester)
			{
				CAIObject *pThisPoint = m_FormationPoints[i].m_pWorldPoint;
				float dist = Distance::Point_PointSq(pThisPoint->GetPos(),basePos);
				if (dist < mindist)
				{
					index = i;
					mindist = dist;
					pPoint = pThisPoint;
				}
			}
		}
	if(index>=0 && bReserve)
	{
		FreeFormationPoint(pRequester);
		m_FormationPoints[index].m_pReservation = pRequester;
	}
	return index;
}



CAIObject * CFormation::GetFormationDummyTarget(const CAIObject *pRequester) const
{
	for (int i=0;i<m_FormationPoints.size();i++)
		if(m_FormationPoints[i].m_pReservation == pRequester)
			return m_FormationPoints[i].m_pDummyTarget;
	return NULL;
}


void CFormation::SetDummyTargetPosition(const Vec3& vPosition, CAIObject* pDummyTarget, const Vec3& vSight)
{
	Vec3 myDir;
	Vec3 otherDir;
	

	if (pDummyTarget)
	{ 

		Vec3 myangles = vSight;

		myangles.z = 0;

		float fi = -m_fSIGHT_WIDTH/2 + (((float)(ai_rand() & 255) / 255.0f ) * m_fSIGHT_WIDTH);

		Matrix33 m = Matrix33::CreateRotationXYZ( DEG2RAD(Ang3(0,0,fi)) ); 
		myangles = m * (Vec3)myangles;

		myangles*=20.f;

		pDummyTarget->SetPos(vPosition+(Vec3)myangles);

	}
}


void CFormation::Draw(IRenderer * pRenderer)
{
	float sp;
	ColorF lineColPathOk  (0.f,1.f,0.f,0.7f);
	ColorF lineColNoPath  (1.f,1.f,1.f,0.7f);
	ColorF lineColDiffPath(1.f,1.f,0.f,0.7f);
	for (unsigned int i = 0; i < m_FormationPoints.size(); ++i)
	{
		CFormationPoint& fp = m_FormationPoints[i];
		CAIObject* pReservoir = fp.m_pReservation;
		float alfaSud  = pReservoir ? 1:0.4f;
		Vec3 pos = fp.m_pWorldPoint->GetPos();
		ColorF col;
		if (!fp.m_pWorldPoint->IsEnabled())
			col.Set(1,0,0,alfaSud);
		else if((sp=fp.m_Speed)>0)
			col.Set(0,1,0,alfaSud);
		else
			col.Set(1,1,0,alfaSud);
		pRenderer->GetIRenderAuxGeom()->DrawSphere(pos,i==m_iSpecialPointIndex ? 0.2f:0.3f, col);
		
		if(pReservoir)
		{
			ColorF lineCol;
			CPipeUser* pPiper = pReservoir->CastToCPipeUser();
			if (pPiper && pPiper->m_pPathFindTarget == fp.m_pWorldPoint)
				lineCol = (pPiper->m_nPathDecision == PATHFINDER_NOPATH ? lineColNoPath : lineColPathOk);
			else
				lineCol = lineColDiffPath;
			pRenderer->GetIRenderAuxGeom()->DrawLine(pos,lineCol,pReservoir->GetPos(),lineCol);
			pos.z+=1;
			if(pPiper && pPiper->m_pPathFindTarget)
				pRenderer->DrawLabel(pos,1,"%s (%s->%s)",fp.m_pWorldPoint->GetName(),pReservoir->GetName(),pPiper->m_pPathFindTarget->GetName());
			else
				pRenderer->DrawLabel(pos,1,"%s (%s)",fp.m_pWorldPoint->GetName(),pReservoir->GetName());
		}
		else
		{
			pos.z+=1;
			pRenderer->DrawLabel(pos,1,"%s",fp.m_pWorldPoint->GetName());
		}

		// Draw the initial position, this is to help the designers to match the initial character locations with formation points.
		pRenderer->GetIRenderAuxGeom()->DrawLine(fp.m_DEBUG_vInitPos-Vec3(0,0,10), ColorB(255,196,0), fp.m_DEBUG_vInitPos+Vec3(0,0,10), ColorB(255,196,0));
		pRenderer->GetIRenderAuxGeom()->DrawCone(fp.m_DEBUG_vInitPos+Vec3(0,0,0.5f), Vec3(0,0,-1), 0.2f, 0.5f, ColorB(255,196,0));
		if (Distance::Point_PointSq(fp.m_DEBUG_vInitPos, pos) > 5.0f)
			pRenderer->DrawLabel(fp.m_DEBUG_vInitPos+Vec3(0,0,0.5f), 1, "Init\n%s", fp.m_pWorldPoint->GetName());
	}

	for (unsigned int i = 0;i<m_FormationPoints.size();i++)
	{
		CFormationPoint& fp = m_FormationPoints[i];
		float alfaSud  = fp.m_pReservation ? 1:0.4f;
		ColorF colSight;
		colSight.Set(0,1.f,0.7f,alfaSud);
		Vec3 pos = fp.m_pWorldPoint->GetPos();
		Vec3 posd =  fp.m_pDummyTarget->GetPos() - pos; 
		posd.NormalizeSafe();
		Vec3 possight = pos+posd/2;
		pRenderer->GetIRenderAuxGeom()->DrawCone(possight,posd,0.05f,0.2f,colSight);
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos, colSight, possight, colSight);
		pRenderer->SetMaterialColor(1,1.f,0.0f,alfaSud);
	}

	if(m_FormationPoints.size())
	{
		ColorF col(1,0.5,1.0f,1.f);
		Vec3 pos = m_FormationPoints[0].m_pWorldPoint->GetPos();
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos, col, pos+3*m_dbgVector1, col);
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos, col, pos+3*m_dbgVector2, col);
	}

	if (m_pPathMarker)
		m_pPathMarker->DebugDraw(pRenderer);

}

void CFormation::FreeFormationPoint(const CAIObject * pCurrentHolder)
{
	for (int i=0;i<m_FormationPoints.size();i++)
	{
		if(m_FormationPoints[i].m_pReservation == pCurrentHolder)
			m_FormationPoints[i].m_pReservation = NULL;
		if(m_FormationPoints[i].m_pWorldPoint->GetAssociation() == pCurrentHolder)
			m_FormationPoints[i].m_pWorldPoint->SetAssociation(NULL);
		if(m_FormationPoints[i].m_pDummyTarget->GetAssociation() == pCurrentHolder)
			m_FormationPoints[i].m_pDummyTarget->SetAssociation(NULL);
	}
}

void CFormation::FreeFormationPoint(int i)
{
	m_FormationPoints[i].m_pReservation = NULL;
	m_FormationPoints[i].m_pWorldPoint->SetAssociation(NULL);
	m_FormationPoints[i].m_pDummyTarget->SetAssociation(NULL);
}
//----------------------------------------------------------------------

void CFormation::Reset( )
{
	for (int i=0;i<m_FormationPoints.size();i++)
		m_FormationPoints[i].m_pReservation = NULL;
}

void CFormation::InitWorldPosition(Vec3 vDir)
{
	if(vDir.IsZero())
		vDir = m_pOwner->GetViewDir();
	vDir.NormalizeSafe();
	if(vDir.IsZero())
		vDir = m_pOwner->GetMoveDir();

	m_vInitialDir = vDir;

	SetOffsetPointsPos(vDir);
	InitFollowPoints(vDir);//,pOwner);
}


void CFormation::SetOffsetPointsPos(const Vec3& vMoveDir)
{
	Vec3 vBasePos = m_pOwner->GetPos();
	Vec3 moveDirXAxis(-vMoveDir.y,vMoveDir.x,0);

	// update the fixed offset points
	for(TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint!=m_FormationPoints.end(); ++itrPoint)
	{
		bool bFirst = true;
		CFormationPoint& formPoint = (*itrPoint);
		Vec3 pos = formPoint.m_vPoint;
		CAIObject *pFormationDummy = formPoint.m_pWorldPoint;


		//Update nodes
		if(!(m_bFollowingUnits && formPoint.m_FollowDistance>0 ))
		{	// fixed offset node
			Vec3 posRot;
			posRot = pos.x * moveDirXAxis + pos.y*vMoveDir;
			pos = posRot+ vBasePos; 
			pos.z = GetAISystem()->m_pSystem->GetI3DEngine()->GetTerrainElevation(pos.x,pos.y)+1.75f;
			
			if(bFirst)
				pFormationDummy->SetPos(pos);
			else
				formPoint.SetPos(pos,vBasePos);
		
			formPoint.m_DEBUG_vInitPos = pos;

			formPoint.m_LastPosition = pos;
			//Update dummy targets
			Vec3 vSight = formPoint.m_vSight;
					
			CAIObject* pDummyTarget = formPoint.m_pDummyTarget;
			if(pDummyTarget)
			{
				if(m_pReferenceTarget && formPoint.m_Class != SPECIAL_FORMATION_POINT)
				{
					pDummyTarget->SetPos(m_pReferenceTarget->GetPos());
				}
				else
				{
					Vec3 vDummyPosRelativeToOwner = formPoint.m_vPoint + vSight; // in owner's space
					Vec3 posRot = vDummyPosRelativeToOwner.x * moveDirXAxis + vDummyPosRelativeToOwner.y * vMoveDir;
					Vec3 vDummyWorldPos = posRot + vBasePos;
					vDummyWorldPos.z = pos.z;
					pDummyTarget->SetPos(vDummyWorldPos);
				}
			}
		}
	}
}

//----------------------------------------------------------------------
void CFormation::InitFollowPoints(const Vec3& vDirection)//, const CAIObject* pRequestor)
{
	// update the follow points
	Vec3 vInitPos = m_pOwner->GetPos();
	Vec3 vDir(vDirection.IsZero() ? (m_vLastMoveDir.IsZero() ? m_vMoveDir : m_vLastMoveDir) :vDirection);
	vDir.z=0; //2d only
	vDir.NormalizeSafe();

	Vec3 vX(-vDir.y, vDir.x,0);
	Vec3 vCurrentPos = vInitPos;
	float lastFollowDist = 0;
	Vec3 vPrevFollowPoint = vInitPos;
	bool bReachable = true;

	// The initial follow points are used for assigning the formation positions.
	// For this reason the formation points are slightly contracted, to allow a bit more setup in the level.

	// Find the nearest follow distance.
	float nearestFollowDistance = FLT_MAX;
	for (TFormationPoints::iterator itrPoint = m_FormationPoints.begin(), end = m_FormationPoints.end(); itrPoint != end; ++itrPoint)
	{
		CFormationPoint& formPoint = (*itrPoint);
		if (formPoint.m_FollowDistance > 0)
			nearestFollowDistance = min(nearestFollowDistance, formPoint.m_FollowDistance);
	}

	bool bPlayerLeader = (m_pOwner && m_pOwner->GetAIType()==AIOBJECT_PLAYER);
	for(TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint!=m_FormationPoints.end(); ++itrPoint)
	{
		CFormationPoint& formPoint = (*itrPoint);
		if(formPoint.m_FollowDistance > 0)
		{
			float totalFollowDist = formPoint.m_FollowDistance - nearestFollowDistance;	// Contracted formation.
			Vec3	requestedPosAlongCurve(vInitPos - vDir*totalFollowDist );
			Vec3 offsetPos = vX*formPoint.m_FollowOffset + Vec3(0,0,formPoint.m_FollowHeightOffset);
			Vec3	requestedPos(requestedPosAlongCurve + offsetPos);
			formPoint.SetPos(requestedPos, vInitPos, true,bPlayerLeader);
			formPoint.m_LastPosition = requestedPos;
			formPoint.m_DEBUG_vInitPos = requestedPos;
			Vec3 vSight = formPoint.m_vSight;

			CAIObject* pDummyTarget = formPoint.m_pDummyTarget;
			if(pDummyTarget)
			{
				Vec3 vDummyPosRelativeToOwner = formPoint.m_vPoint + vSight; // in owner's space
				Vec3 posRot = -vDummyPosRelativeToOwner.x * vX + vDummyPosRelativeToOwner.y * vDir;
				Vec3 vDummyWorldPos = posRot + vInitPos;
				vDummyWorldPos.z = requestedPos.z;
				pDummyTarget->SetPos(vDummyWorldPos);
			}

		}
	}
}


//----------------------------------------------------------------------
CAIObject* CFormation::GetFormationPoint(const CAIObject* pRequester) const
{
	for(TFormationPoints::const_iterator itrPoint(m_FormationPoints.begin()); itrPoint!=m_FormationPoints.end(); ++itrPoint)
		if((*itrPoint).m_pReservation==pRequester)
			return (*itrPoint).m_pWorldPoint;
	return NULL;
}

//----------------------------------------------------------------------
float CFormation::GetFormationPointSpeed(const CAIObject* pRequester) const
{
	for(TFormationPoints::const_iterator itrPoint(m_FormationPoints.begin()); itrPoint!=m_FormationPoints.end(); ++itrPoint)
		if((*itrPoint).m_pReservation==pRequester)
			return (*itrPoint).m_Speed;
	return 0;
}



//----------------------------------------------------------------------
float CFormation::GetDistanceToOwner(int i) const
{
	
	if((unsigned)i>=m_FormationPoints.size())
		return 0;
	if(m_FormationPoints[i].m_FollowDistance)
		return sqrt(m_FormationPoints[i].m_FollowDistance*m_FormationPoints[i].m_FollowDistance + 
								m_FormationPoints[i].m_FollowOffset*m_FormationPoints[i].m_FollowOffset +
								m_FormationPoints[i].m_FollowHeightOffset*m_FormationPoints[i].m_FollowHeightOffset);
	else
		return (m_FormationPoints[i].m_vPoint - m_FormationPoints[0].m_vPoint).GetLength();

}


//----------------------------------------------------------------------
int	CFormation::CountFreePoints() const
{
	int iSize = GetSize();
	int iFree = 0;
	for(int i=0;i<iSize; i++)
		if(!m_FormationPoints[i].m_pReservation)
			iFree++;
	return iFree;
}

//----------------------------------------------------------------------
float CFormationDescriptor::GetNodeDistanceToOwner(const FormationNode& nodeDescriptor) const
{
	if(m_Nodes.size())
	{
		if(nodeDescriptor.fFollowDistance)
			return sqrt(nodeDescriptor.fFollowDistance*nodeDescriptor.fFollowDistance + 
				nodeDescriptor.fFollowOffset *nodeDescriptor.fFollowOffset);
		else
			return nodeDescriptor.vOffset.GetLength();
	}
	else
		return 0;
}

//----------------------------------------------------------------------
void CFormationDescriptor::AddNode(const FormationNode& nodeDescriptor) 
{
	// insert new node descriptor in a distance-to-owner sorted vector
	if(m_Nodes.size() && nodeDescriptor.eClass != SPECIAL_FORMATION_POINT)
	{
		TVectorOfNodes::iterator itNext = m_Nodes.begin();
		float fDist = GetNodeDistanceToOwner(nodeDescriptor);
		for(TVectorOfNodes::iterator it = m_Nodes.begin(); it!=m_Nodes.end();++it)
		{
			++itNext;
			if(itNext!=m_Nodes.end())
			{
				float fDistNext = GetNodeDistanceToOwner(*itNext);
				if(fDist<fDistNext || itNext->eClass ==SPECIAL_FORMATION_POINT)
				// leave the special formation points at the bottom
				{
					m_Nodes.insert(itNext,nodeDescriptor);
					break;
				}
			}
			else
			{
				m_Nodes.push_back(nodeDescriptor);
				break;
			}
		}
	}
	else
		m_Nodes.push_back(nodeDescriptor);
}

void	CFormation::SetUpdate(bool bUpdate)
{
	m_bUpdate = bUpdate;
	if(bUpdate)
		m_fLastUpdateTime.SetValue(0); // force update the next time, when SetUpdate(true) is called
}

//----------------------------------------------------------------------
void CFormation::SetUpdateThresholds(float value)
{
	m_fDynamicUpdateThreshold = value;
	m_fStaticUpdateThreshold = 5*value;
}

//----------------------------------------------------------------------
CAIObject* CFormation::GetClosestPoint( CAIActor *pRequester, bool bReserve,int iClass)
{
	int index = GetClosestPointIndex(pRequester,bReserve,0,iClass);
	return (index<0? NULL:m_FormationPoints[index].m_pWorldPoint);
}

//
//--------------------------------------------------------------------------------------------------------------
void CFormation::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	ser.BeginGroup("AIFormation");	
	{

		objectTracker.SerializeObjectContainer(ser, "m_FormationPoints", m_FormationPoints);
		objectTracker.SerializeObjectPointer(ser,"m_pOwner",m_pOwner,false);

		ser.Value("m_bReservationAllowed", m_bReservationAllowed);

		ser.Value("m_vLastUpdatePos", m_vLastUpdatePos);
		ser.Value("m_vLastPos", m_vLastPos);
		ser.Value("m_vLastTargetPos", m_vLastTargetPos);
		ser.Value("m_vLastMoveDir", m_vLastMoveDir);
		ser.Value("m_vInitialDir", m_vInitialDir);
		ser.Value("m_vMoveDir", m_vMoveDir);
		ser.Value("m_bInitMovement", m_bInitMovement);
		ser.Value("m_bFirstUpdate",m_bFirstUpdate);
		ser.Value("m_bUpdate",m_bUpdate);
		ser.Value("m_bForceReachable",m_bForceReachable);
		ser.Value("m_bFollowingUnits", m_bFollowingUnits);
		ser.Value("m_bFollowingPointInitialized", m_bFollowPointInitialized);
		ser.Value("m_fUpdateThreshold", m_fUpdateThreshold);
		ser.Value("m_fDynamicUpdateThreshold",m_fDynamicUpdateThreshold);
		ser.Value("m_fStaticUpdateThreshold", m_fStaticUpdateThreshold);
		ser.Value("m_fScaleFactor",m_fScaleFactor);
		ser.Value("m_iSpecialPointIndex", m_iSpecialPointIndex);
		ser.Value("m_fMaxFollowDistance", m_fMaxFollowDistance);
		ser.Value("m_fLastUpdateTime", m_fLastUpdateTime);
		ser.Value("m_fMaxUpdateSightTime",m_fMaxUpdateSightTime);
		ser.Value("m_fMinUpdateSightTime",m_fMinUpdateSightTime);
		ser.Value("m_fSightRotationRange",m_fSightRotationRange);
		ser.Value("m_szDescriptor",m_szDescriptor);
		objectTracker.SerializeObjectPointer(ser,"m_pReferenceTarget",m_pReferenceTarget,false);
		ser.Value("m_fRepositionSpeedWrefTarget",m_fRepositionSpeedWrefTarget);
		ser.Value("m_vDesiredTargetPos",m_vDesiredTargetPos);
		ser.EnumValue("m_orientationType",m_orientationType,OT_MOVE,OT_VIEW);

		if(ser.IsWriting())
		{
			if(ser.BeginOptionalGroup("FormationPathMarker", m_pPathMarker!=NULL))
			{
				m_pPathMarker->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		else
		{
			m_fLastUpdateTime = 0.0f;
			if(ser.BeginOptionalGroup("FormationPathMarker",true))
			{
				if (m_pPathMarker)
				{
					delete m_pPathMarker;
					m_pPathMarker = 0;
				}
				m_pPathMarker = new CPathMarker(m_fMaxFollowDistance + 10, m_fDISTANCE_UPDATE_THR / 2);
				m_pPathMarker->Serialize(ser, objectTracker);
				ser.EndGroup();
			}
		}
		ser.EndGroup();
	}

}

//
//--------------------------------------------------------------------------------------------------------------
CFormationPoint::CFormationPoint():
m_pWorldPoint(NULL),
m_pReservation(NULL),
m_pDummyTarget(NULL),
m_Speed(0),
m_LastPosition(0,0,0),
m_Dir(0,0,0),
m_DEBUG_vInitPos(0,0,0)
{

}


//
//--------------------------------------------------------------------------------------------------------------
CFormationPoint::~CFormationPoint()
{

}


//
//--------------------------------------------------------------------------------------------------------------
void CFormationPoint::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	ser.BeginGroup("FormationPoint");
	ser.Value("m_vPoint", m_vPoint); 
	ser.Value("m_vSight",	m_vSight);	
	ser.Value("m_FollowDistance",	m_FollowDistance);	
	ser.Value("m_FollowOffset",	m_FollowOffset);		
	ser.Value("m_FollowHeightOffset",	m_FollowHeightOffset);		
	ser.Value("m_FollowDistanceAlternate",	m_FollowDistanceAlternate);	
	ser.Value("m_FollowOffsetAlternate",	m_FollowOffsetAlternate);
	ser.Value("m_Speed",	m_Speed);	
	ser.Value("m_LastPosition",	m_LastPosition);	
	ser.Value("m_Dir",	m_Dir);	
	ser.Value("m_Class",	m_Class);
	objectTracker.SerializeObjectPointer(ser, "m_pWorldPoint",m_pWorldPoint, false);	
	objectTracker.SerializeObjectPointer(ser, "m_pREservation",	m_pReservation, false);	
	objectTracker.SerializeObjectPointer(ser, "m_pDummyTarget",	m_pDummyTarget, false);
	ser.Value("m_FollowFlag",	m_FollowFlag);	
	ser.EndGroup();
}

//
//--------------------------------------------------------------------------------------------------------------
void CFormation::Change(CFormationDescriptor & desc, float fScale)
{
	if(desc.m_sName !=m_szDescriptor)
	{
		
		if(desc.m_Nodes.empty() || m_FormationPoints.empty())
			return;

		m_szDescriptor = desc.m_sName ;
		CFormationDescriptor::TVectorOfNodes::iterator vi;
		TFormationPoints::iterator fi;

		std::vector<bool> vDescChanged;
		std::vector<bool> vPointChanged;

		for (vi=desc.m_Nodes.begin();vi!=desc.m_Nodes.end();++vi)
			vDescChanged.push_back(false);

		for (fi= m_FormationPoints.begin();fi!=m_FormationPoints.end();++fi)
			vPointChanged.push_back(false);
		
		m_bFollowingUnits = false;

		m_fMaxFollowDistance = 0;
//		float m_fMaxYOffset = -1000000;
//		m_iForemostNodeIndex = -1;
		m_iSpecialPointIndex = -1;

		m_fScaleFactor = 1;
		
		float oldMaxFollowDistance = m_fMaxFollowDistance;
		m_fMaxFollowDistance = 0;
		int i =0;
		for(int iteration = 0; iteration<2; iteration++)
		{
			// iteration = 0 : consider only current points with a reservation (change them first)
			// iteration = 1 : consider all current points
			int i =0;
			TFormationPoints::iterator itrPoint(m_FormationPoints.begin());

			for (++itrPoint; itrPoint!=m_FormationPoints.end(); ++itrPoint)
			{
				CFormationPoint	&frmPoint = (*itrPoint);

				if(iteration==0 && !frmPoint.m_pReservation || iteration==1 && vPointChanged[i])
					continue;

				int iClass = frmPoint.m_Class;
				
				bool bFound = false;
				for(int iTestType=0; iTestType<5 && !bFound; iTestType++)
				{
					int j = 1;
					vi=desc.m_Nodes.begin();
					for (++vi; !bFound && vi!=desc.m_Nodes.end(); ++vi)
					{
						int iNewClass = (*vi).eClass;
						if(!vDescChanged[j] && 
							(iTestType==0 && iNewClass == iClass  || 
							 iTestType==1 && (iNewClass & iClass)==min(iNewClass,iClass) || 
							 iTestType==2 && (iNewClass & iClass)!=0 || 
							 iTestType==3 && (iNewClass == UNIT_CLASS_UNDEFINED) || 
							 iTestType==4)) 
						{
							Vec3 pos = (*vi).vOffset;
							float fDistance = (*vi).fFollowDistance;
							frmPoint.m_FollowDistance = fDistance;

							if(frmPoint.m_FollowDistance>0)
							{
								pos = Vec3((*vi).fFollowOffset,fDistance,0);
								if (m_fMaxFollowDistance < frmPoint.m_FollowDistance)
									m_fMaxFollowDistance = frmPoint.m_FollowDistance;
							}
							/*else if(m_fMaxYOffset < pos.y)
							{ // find the foremost formation unit to be followed
								m_fMaxYOffset = pos.y;
								m_iForemostNodeIndex = i;
							}*/
							if(iClass == SPECIAL_FORMATION_POINT)
								m_iSpecialPointIndex = i;
							frmPoint.m_vPoint = pos;
							frmPoint.m_FollowOffset = (*vi).fFollowOffset ;

							frmPoint.m_FollowDistanceAlternate = (*vi).fFollowDistanceAlternate;
							frmPoint.m_FollowOffsetAlternate = (*vi).fFollowOffsetAlternate;
							frmPoint.m_FollowHeightOffset = (*vi).fFollowHeightOffset;

							vDescChanged[j] = true;
							vPointChanged[i] = true;
							bFound = true;
						}
						j++;
					} // vi

				} // testType

				++i;
			} // itrpoint
		} // iteration
//		if(m_iForemostNodeIndex<0)
//			m_bFollowingUnits = false; // if it was true but no foremost node, it means that all the nodes
		// were set to follow
		i=0;

		// Delete old points not used and set the flag m_bFollowingUnits
		for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint!=m_FormationPoints.end(); )
		{
			CFormationPoint	&frmPoint = (*itrPoint);
			if(!vPointChanged[i] && !frmPoint.m_pReservation)
			{	
				if(frmPoint.m_pWorldPoint)
					GetAISystem()->RemoveDummyObject(frmPoint.m_pWorldPoint);
				if(frmPoint.m_pDummyTarget)
					GetAISystem()->RemoveDummyObject(frmPoint.m_pDummyTarget);

				itrPoint = m_FormationPoints.erase(itrPoint);
			}
			else
			{

				if(frmPoint.m_FollowDistance>0)
					m_bFollowingUnits = true;
				++itrPoint;
			}
			i++;
		}
	
		// Add new points
		int j=1;
		vi=desc.m_Nodes.begin();
		for (++vi; vi!=desc.m_Nodes.end(); ++vi)
		{
			if(!vDescChanged[j])
			{
				FormationNode& desc = *vi;
				AddPoint(desc);
			}
			j++;
		}

		m_bInitMovement = true;
		m_bFirstUpdate = true;
		m_bFollowPointInitialized = false;
//		m_vInitialDir.Set(0,0,0);

//		CAIObject* pOwner = m_FormationPoints[0].m_pReservation;
		if(m_pOwner)
		{
			if(m_bFollowingUnits && oldMaxFollowDistance < m_fMaxFollowDistance)
			{
				if (m_pPathMarker)
				{
					delete m_pPathMarker;
					m_pPathMarker = 0;
				}
				m_pPathMarker = new CPathMarker(m_fMaxFollowDistance + 10, m_fDISTANCE_UPDATE_THR / 2);
				m_pPathMarker->Init(m_pOwner->GetPos(), m_pOwner->GetPos()- m_fMaxFollowDistance* m_vMoveDir );
			}
			InitWorldPosition(m_vMoveDir);
		}
	}

	if(fScale!=m_fScaleFactor)
		SetScale(fScale);

}
//
//--------------------------------------------------------------------------------------------------------------
void CFormation::SetScale(float fScale) 
{
	// to do: retrieve the original offsets/distances when the old scale factor is 0?
	if(fScale==0)
		return;

	if(fScale==m_fScaleFactor)
		return;
	
	float fActualScale = fScale/m_fScaleFactor;
	m_fMaxFollowDistance *=fActualScale;
	m_fScaleFactor = fScale;
	
	if(m_FormationPoints.size())
	{
		// modify all points
		for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint!=m_FormationPoints.end(); ++itrPoint)
		{
			itrPoint->m_vPoint *= fActualScale;
			itrPoint->m_FollowDistance *= fActualScale;
			itrPoint->m_FollowOffset *= fActualScale;
			itrPoint->m_FollowDistanceAlternate *= fActualScale;
			itrPoint->m_FollowOffsetAlternate *= fActualScale;
			itrPoint->m_FollowHeightOffset *= fActualScale;
		}

		CAIObject *pOwner = m_FormationPoints[0].m_pReservation;
		if(!pOwner)
			return;
		// find the formation orientation
		Vec3 vDir =m_vMoveDir;
		if(vDir.IsZero())
		{
			if(m_bFollowingUnits && m_pPathMarker)
			{
				vDir = m_pPathMarker->GetPointAtDistance(pOwner->GetPos(),1);
				vDir -= pOwner->GetPos();
				vDir.z = 0; //2d only
				vDir.NormalizeSafe();
				if(vDir.IsZero())
					vDir = pOwner->GetMoveDir();
			}
			else
			{
				pe_status_dynamics  dSt;
				pOwner->GetProxy()->GetPhysics()->GetStatus(&dSt);
				vDir = -dSt.v;
				if(!vDir.IsZero())
					vDir.Normalize();
				else
					vDir = pOwner->GetMoveDir();

			}
		}
		SetOffsetPointsPos(vDir);
		// modify path marker if the new formation is bigger
		if(m_bFollowingUnits && fActualScale>1)
		{
			vDir.z = 0;
			vDir.NormalizeSafe();
			m_vInitialDir = vDir;

			if(m_pPathMarker)
			{
				delete m_pPathMarker;
				m_pPathMarker = 0;
			}

			m_pPathMarker = new CPathMarker(m_fMaxFollowDistance + 10, m_fDISTANCE_UPDATE_THR / 2);
			m_pPathMarker->Init(pOwner->GetPos(), pOwner->GetPos()- m_fMaxFollowDistance* vDir );
			InitFollowPoints();//,vDir,pOwner);
		}
	}
}

Vec3 CFormation::GetPredictedPointPosition(const CAIObject* pRequestor, const Vec3& ownerPos, const Vec3& ownerLookDir, Vec3 ownerMoveDir) const
{
	const CFormationPoint* pFormPoint = NULL;
	if(ownerMoveDir.IsZero())
		ownerMoveDir = ownerLookDir;

	for (TFormationPoints::const_iterator itrPoint(m_FormationPoints.begin()); itrPoint!=m_FormationPoints.end(); ++itrPoint)
		if(itrPoint->m_pReservation ==pRequestor)
		{
			pFormPoint = &(*itrPoint);
			break;
		}

	if(!pFormPoint)
		return ZERO;
	Vec3 vPredictedPos;
	if(pFormPoint->m_FollowDistance>0)
	{
		// approximate, assumes that the follow points will be aligned along ownerDir
		vPredictedPos.Set(pFormPoint->m_FollowOffset*(ownerMoveDir.x),
			pFormPoint->m_FollowDistance*(-ownerMoveDir.y),
			pFormPoint->m_FollowHeightOffset);
		vPredictedPos+=ownerPos;
		return vPredictedPos;
	}
	else
	{
		vPredictedPos.Set(pFormPoint->m_vPoint.x * ownerLookDir.y,pFormPoint->m_vPoint.y * (-ownerLookDir.x),pFormPoint->m_vPoint.z);
		vPredictedPos+=ownerPos;
		return vPredictedPos;
	}
}

void CFormation::SetReferenceTarget(const CAIObject* pTarget,float speed) 
{
	m_pReferenceTarget = pTarget;
	m_vLastTargetPos = ZERO;
	m_fRepositionSpeedWrefTarget = speed;
	m_vDesiredTargetPos= ZERO;
	m_vLastUpdatePos = ZERO; //to force update
	Update();
}

