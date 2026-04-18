/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004
 -------------------------------------------------------------------------
  File name:   CAISystemPhys.cpp
	$Id$
  Description:	should contaioin all the methods of CAISystem which have to deal with Physics 
  
 -------------------------------------------------------------------------
  History:
	- 2007				: Created by Kirill Bulatsev
	

*********************************************************************/

#include "StdAfx.h"

#include "IPhysics.h"
#include "IEntityRenderState.h"
#include "CAISystem.h"
#include "Puppet.h"


//===================================================================
// GetWaterOcclusionValue
//===================================================================
float CAISystem::GetWaterOcclusionValue(const Vec3& targetPos) const
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	float	occlusion(0.f);
	IPhysicalEntity** pAreas;
	int nAreas=gEnv->pPhysicalWorld->GetEntitiesInBox(targetPos, targetPos, pAreas, ent_areas);
	for(int i(0); i<nAreas; ++i)
	{
		pe_params_buoyancy pb; 
		// if this is a water area (there will be only one)
		if( pAreas[i]->GetParams(&pb) && !is_unused(pb.waterPlane.origin))
		{
			pe_status_contains_point scp; 
			scp.pt = targetPos;
			if(pAreas[i]->GetStatus(&scp))
			{
				float waterLevel( targetPos.z-pb.waterPlane.n.z*(pb.waterPlane.n*(targetPos-pb.waterPlane.origin)));
				float	inWaterDepth( waterLevel - targetPos.z );
				if(inWaterDepth<=0.f)
					return 0.f;	//not in water - above
				IWaterVolumeRenderNode  *pWaterRenderNode((IWaterVolumeRenderNode*)pAreas[i]->GetForeignData(PHYS_FOREIGN_ID_WATERVOLUME));
				if(!pWaterRenderNode)	// if in the ocean - no render node
				{
					float resValue (m_cvWaterOcclusion->GetFVal() * inWaterDepth/6.f);
					// make sure it's in 0-1 range
					return min(1.f, resValue);
				}
				float	waterFogDensity(pWaterRenderNode->GetFogDensity());
				float resValue (m_cvWaterOcclusion->GetFVal() * exp(-waterFogDensity*inWaterDepth));
				// make sure it's in 0-1 range
				resValue = min(1.f, resValue);
				return 1.f - resValue;
			}
			return 0.f;	//not in water
		}
	}
	return 0.f;	//not in water
}


//===================================================================
// IsSoundHearable
//===================================================================
bool CAISystem::IsSoundHearable(CPuppet *pPuppet, const Vec3& vSoundPos, float fSoundRadius)
{
	TICK_COUNTER_FUNCTION
	int nBuildingIDPuppet = -1, nBuildingIDSound = -1;
	IVisArea* pAreaPuppet = 0;
	IVisArea* pAreaSound = 0;
	CheckNavigationType(pPuppet->GetPos(), nBuildingIDPuppet, pAreaPuppet, pPuppet->m_movementAbility.pathfindingProperties.navCapMask);
	CheckNavigationType(vSoundPos, nBuildingIDSound, pAreaSound, pPuppet->m_movementAbility.pathfindingProperties.navCapMask);

	if (nBuildingIDPuppet < 0 && nBuildingIDSound < 0)
		return true;

	if (nBuildingIDPuppet != nBuildingIDSound)
	{
		if (nBuildingIDPuppet < 0)
		{
			// Puppet is outdoors, sound indoors
			if (pAreaSound)
			{
				if (pAreaSound->IsConnectedToOutdoor())
					return true;
				return false;
			}
		}
		else if (nBuildingIDSound < 0)
		{
			// Sound is outdoors, puppet indoors
			if (pAreaPuppet)
			{
				if (pAreaPuppet && pAreaPuppet->IsConnectedToOutdoor())
				{
					// shoot a ray to figure out if this sound will be heard
					ray_hit hit;
					int rayresult = 0;
					TICK_COUNTER_FUNCTION_BLOCK_START
					rayresult = m_pWorld->RayWorldIntersection(vSoundPos, vSoundPos - pPuppet->GetPos(), ent_static, 0, &hit,1);
					TICK_COUNTER_FUNCTION_BLOCK_END
					if (rayresult)					
						return false;
					else
						return true;
				}
				return false;
			}
		}

		if (pAreaPuppet || pAreaSound)	//only if in real indoors
			return false; // if in 2 different buildings we cannot hear the sound for sure
	}

	return true;
}


//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::CheckPointsVisibility(const Vec3& from, const Vec3& to, float rayLength, IPhysicalEntity* pSkipEnt, IPhysicalEntity* pSkipEntAux)
{
	TICK_COUNTER_FUNCTION
		ray_hit hit;
	Vec3 dir = to-from;
	if (rayLength && dir.GetLengthSquared() > rayLength*rayLength)
	{
		dir *= rayLength / dir.GetLength();
	}
	TICK_COUNTER_FUNCTION_BLOCK_START
		return 0 == m_pWorld->RayWorldIntersection(from, dir, COVER_OBJECT_TYPES, HIT_COVER | HIT_SOFT_COVER, &hit, 1, pSkipEnt, pSkipEntAux);
	TICK_COUNTER_FUNCTION_BLOCK_END
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::CheckObjectsVisibility(const IAIObject *pObj1, const IAIObject *pObj2, float rayLength)
{
	TICK_COUNTER_FUNCTION

	ray_hit hit;
	Vec3 dir = pObj2->GetPos() - pObj1->GetPos();
	if (rayLength && dir.GetLengthSquared() > sqr(rayLength))
		dir *= rayLength / dir.GetLength();

	int rayresult;

	std::vector<IPhysicalEntity*> skipList;
	if (const CAIActor* pActor = pObj1->CastToCAIActor())
		pActor->GetPhysicsEntitiesToSkip(skipList);
	if (const CAIActor* pActor = pObj2->CastToCAIActor())
		pActor->GetPhysicsEntitiesToSkip(skipList);

	TICK_COUNTER_FUNCTION_BLOCK_START
		rayresult = m_pWorld->RayWorldIntersection(pObj1->GetPos(), dir, COVER_OBJECT_TYPES, HIT_COVER | HIT_SOFT_COVER,
			&hit, 1, skipList.empty() ? 0 : &skipList[0], skipList.size());
	TICK_COUNTER_FUNCTION_BLOCK_END

	// Allow small fudge in th test just in case the point is exactly on ground.
	bool	visible = !rayresult || hit.dist > (dir.GetLength() - 0.1f);
	
	return visible;
}


//
//-----------------------------------------------------------------------------------------------------------
// go trough all the disabled (dead) puppets
void CAISystem::CheckVisibilityBodies()
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );
	const int bodiesPerUpdateLimit(2);
	static int lastChecked(0);
	
	if (lastChecked >= m_Objects.size())
		lastChecked = 0;

	AIObjects::iterator ai = m_Objects.find(AIOBJECT_PUPPET);
	AIObjects::iterator end = m_Objects.end();
	std::advance(ai, lastChecked);
	int counter = 0;
	for ( ; ai != end && ai->first == AIOBJECT_PUPPET && counter < bodiesPerUpdateLimit; ++ai, ++counter)
	{
		CPuppet *pBody = (CPuppet *) ai->second;
		if (!pBody->m_bUncheckedBody)
			continue;

		CPuppet* pClosestPuppet = 0;
		float	closestDistSqr = FLT_MAX;

		for (unsigned j = 0, nj = m_enabledPuppetsSet.size(); j < nj; ++j)
		{
			CPuppet* pOtherPuppet = m_enabledPuppetsSet[j];
			if (!pOtherPuppet->IsEnabled() || pOtherPuppet == pBody || pBody->m_Parameters.m_nSpecies != pOtherPuppet->m_Parameters.m_nSpecies)
				continue;
			if (!CheckVisibilityToBody(pOtherPuppet, pBody, closestDistSqr))
				continue;
			pClosestPuppet = pOtherPuppet;
		}

		if (pClosestPuppet)
		{
			if (CAIGroup* pGroup = GetAIGroup(pBody->GetGroupId()))
				pGroup->NotifyBodyChecked(pBody);
			pClosestPuppet->SetSignal(1, "OnGroupMemberDiedNearest", pBody->GetEntity(), 0, g_crcSignals.m_nOnGroupMemberDiedNearest);
			pClosestPuppet->SetAlarmed();
			pBody->m_bUncheckedBody = false;
		}
	}
	lastChecked += counter;
}


//
//-----------------------------------------------------------------------------------------------------------
// 
bool CAISystem::CheckVisibilityToBody(CPuppet* pObserver, CAIActor* pBody, float& closestDistSq, IPhysicalEntity* pSkipEnt)
{
	int newFlags = HIT_COVER;
	newFlags |= HIT_SOFT_COVER;

	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	Vec3 bodyPos(pBody->GetPos());
	CPuppet *pClosestPuppet(NULL);

	const Vec3& puppetPos = pObserver->GetPos();
	Vec3 posDiff = bodyPos - puppetPos;
	float	distSq = posDiff.GetLengthSquared();
	if (distSq > closestDistSq)
		return false;

	if (!pObserver->IsObjectInFOVCone(pBody, pObserver->GetParameters().m_PerceptionParams.perceptionScale.visual * 0.75f))
		return false;

	ray_hit hit;
	//--------------- ACCURATE MEASURING
	float dist = sqrtf(distSq);
	int rayresult;

	std::vector<IPhysicalEntity*> skipList;
	pObserver->GetPhysicsEntitiesToSkip(skipList);
	pBody->GetPhysicsEntitiesToSkip(skipList);
	if (pSkipEnt)
		skipList.push_back(pSkipEnt);

	TICK_COUNTER_FUNCTION
	TICK_COUNTER_FUNCTION_BLOCK_START
	rayresult = m_pWorld->RayWorldIntersection(puppetPos, posDiff, COVER_OBJECT_TYPES, newFlags,
		&hit, 1, skipList.empty() ? 0 : &skipList[0], skipList.size());
	TICK_COUNTER_FUNCTION_BLOCK_END
	++m_nRaysThisUpdateFrame;

	bool	isVisible = !rayresult;
	// check if collider is the body itself
	if (!isVisible)
		isVisible = hit.pCollider == pBody->GetProxy()->GetPhysics() || hit.pCollider == pBody->GetProxy()->GetPhysics(true);

	if (!isVisible)
		return false;

	closestDistSq = distSq;
	return true;
}

//
//-----------------------------------------------------------------------------------------------------------
// 
bool CAISystem::ValidateBigObstacles()
{
	float	trhSize(m_cvBigBrushLimitSize->GetFVal());
	Vec3 min,max;
	float fTSize = (float) m_pSystem->GetI3DEngine()->GetTerrainSize();
	AILogProgress(" Checking for big obstacles out of forbidden areas. Terrain size = %.0f",fTSize);

	min.Set(0,0,-5000);
	max.Set(fTSize,fTSize,5000.0f);

	// get only static physical entities (trees, buildings etc...)
	IPhysicalEntity** pObstacles;
	int	flags = ent_static | ent_ignore_noncolliding;
	int count = m_pWorld->GetEntitiesInBox(min,max,pObstacles,flags);
	for (int i = 0 ; i < count ; ++i)
	{
		IPhysicalEntity *pCurrent = pObstacles[i];

		// don't add entities (only triangulate brush-like objects, and brush-like
		// objects should not be entities!)
		IEntity * pEntity = (IEntity*) pCurrent->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
		if (pEntity)
			continue;

		pe_params_foreign_data pfd;
		if(pCurrent->GetParams(&pfd)==0)
			continue;

		// skip trees
		IRenderNode* pRenderNode = (IRenderNode*) pCurrent->GetForeignData(PHYS_FOREIGN_ID_STATIC);
		if (!pRenderNode)
			continue;
		if(	pRenderNode && pRenderNode->GetRenderNodeType() == eERType_Vegetation )
				continue;
		if (pCurrent->GetForeignData() || (pfd.iForeignFlags & PFF_EXCLUDE_FROM_STATIC))
				continue;

		pe_status_pos sp; 
		sp.ipart = -1;
		sp.partid = -1;
		if(pCurrent->GetStatus(&sp)==0)
			continue;

		pe_status_pos status;
		if(pCurrent->GetStatus(&status)==0)
			continue;

		Vec3 calc_pos = status.pos;

		IVisArea *pArea;
		int buildingID;
		if (CheckNavigationType(calc_pos,buildingID,pArea,IAISystem::NAV_WAYPOINT_HUMAN) == IAISystem::NAV_WAYPOINT_HUMAN)
			continue;

		if (!IsPointInTriangulationAreas(calc_pos))
			continue;

		if (IsPointInForbiddenRegion(calc_pos, true))
			continue;

		if(	status.BBox[1].x-status.BBox[0].x > trhSize 
			||status.BBox[1].y-status.BBox[0].y > trhSize )
		{
			Matrix34 TM;
			IStatObj* pStatObj = pRenderNode->GetEntityStatObj(0, 0, &TM);
			if (pStatObj)
			{
				char msg[256];
				_snprintf(msg, 256, "Big object\n(%.f, %.f, %.f).", calc_pos.x, calc_pos.y, calc_pos.z);
				OBB	obb;
				obb.SetOBBfromAABB(Matrix33(TM), pStatObj->GetAABB());
				m_validationErrorMarkers.push_back(SValidationErrorMarker(msg, TM.GetTranslation(), obb, ColorB(255,196,0)));

				AIWarning("BIG object not enclosed in forbidden area. (%.f   %.f   %.f). Make sure it has NoTrinagulate flag (or is withing forbidden area)", calc_pos.x, calc_pos.y, calc_pos.z);
			}
			else
			{
				AIWarning("BIG object not enclosed in forbidden area. (%.f   %.f   %.f). Make sure it has NoTrinagulate flag (or is withing forbidden area). The geometry does not have stat object, maybe a solid?", calc_pos.x, calc_pos.y, calc_pos.z);
			}
//			AIError("Forbidden area/boundary ");
		}
//		static float edgeTol = 0.3f;
//		if (IsPointOnForbiddenEdge(calc_pos, edgeTol))
//			continue;
	}
	return true;
}
