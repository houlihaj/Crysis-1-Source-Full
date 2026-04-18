/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   ObstacleAllocator.cpp
	$Id$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 2:6:2005   16:26 : Created by Kirill Bulatsev

*********************************************************************/

#include "StdAfx.h"
#include "CAISystem.h"
#include "AIHideObject.h"
#include "NavRegion.h"
#include "PipeUser.h"
#include MATH_H
#include "IRenderAuxGeom.h"	


static const float	LOW_COVER_OFFSET = 0.7f;
static const float HIGH_COVER_OFFSET = 1.7f;

//
//-------------------------------------------------------------------------------------------------------------
CAIHideObject::CAIHideObject():
	m_bIsValid(false),
	m_isUsingCover(false),
	m_objectPos(0,0,0),
	m_objectDir(0,0,0),
	m_objectRadius(0),
	m_objectHeight(0),
	m_objectCollidable(false),
	m_vLastHidePos(0,0,0),
	m_vLastHideDir(0,0,0),
	m_bIsSmartObject(false),
	m_useCover(USECOVER_NONE),
	m_dynCoverEntityId(0),
	m_dynCoverEntityPos(0,0,0),
	m_dynCoverPosLocal(0,0,0),
	m_isCompromised(false),
	m_coverPos(0,0,0),
	m_distToCover(0),
	m_pathOrig(0,0,0),
	m_pathDir(0,0,0),
	m_pathNorm(0,0,0),
	m_pathLimitLeft(0),
	m_pathLimitRight(0),
	m_tempCover(0),
	m_tempDepth(0),
	m_pathComplete(false),
	m_highCoverValid(false),
	m_lowCoverValid(false),
	m_pathHurryUp(false),
	m_lowLeftEdgeValid(false),
	m_lowRightEdgeValid(false),
	m_highLeftEdgeValid(false),
	m_highRightEdgeValid(false),
	m_lowCoverWidth(0),
	m_highCoverWidth(0),
	m_pathUpdateIter(0),
	m_id(0)
{
}

//
//-------------------------------------------------------------------------------------------------------------
CAIHideObject::~CAIHideObject() 
{

}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::Set(const SHideSpot *hs, const Vec3& hidePos, const Vec3& hideDir)
{
	Vec3	oldObjPos(m_objectPos);

	m_bIsValid = (hs != 0);
	m_bIsSmartObject = m_bIsValid;
  if (m_bIsValid)
    m_bIsSmartObject = hs->type == SHideSpot::HST_SMARTOBJECT;

	m_dynCoverEntityId = 0;
	m_dynCoverEntityPos.Set(0,0,0);
	m_dynCoverPosLocal.Set(0,0,0);

  if (hs)
  {
		m_objectPos = hs->objPos;
		m_objectDir = hs->dir;
    m_objectRadius = 0.0f;
		m_objectHeight = 0.0f;
		if(m_objectDir.IsZero())
			m_objectDir = hideDir;

    if (hs->pObstacle)
    {
		  m_objectRadius = hs->pObstacle->fApproxRadius;
		  if(m_objectRadius > 0.001f)
			  m_objectCollidable = hs->pObstacle->bCollidable != 0;
		  else
			  m_objectCollidable = true;
			m_objectHeight = hs->pObstacle->GetApproxHeight();
    }
    if (hs->pAnchorObject)
    {
		  if(hs->pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT)
			  m_objectCollidable = true;
		  else
			{
			  m_objectCollidable = false;
				m_objectRadius = 0.05f;	// Omni directional.
			}
    }
    if (hs->type == SHideSpot::HST_SMARTOBJECT)
    {
  		m_HideSmartObject = hs->SOQueryEvent;
		  if (m_HideSmartObject.pRule->pObjectHelper)
			  m_objectPos = m_HideSmartObject.pObject->GetHelperPos(m_HideSmartObject.pRule->pObjectHelper);
		  else
			  m_objectPos = m_HideSmartObject.pObject->GetPos();
  		m_objectDir = m_HideSmartObject.pObject->GetOrientation(m_HideSmartObject.pRule->pObjectHelper);
  		m_objectCollidable = true;
    }

		if (hs->pNavNode && hs->pNavNode->navType == IAISystem::NAV_WAYPOINT_HUMAN)
		{
			if (hs->pNavNode->GetWaypointNavData()->type == WNT_HIDESECONDARY)
			{
				m_objectCollidable = false;
				m_objectRadius = 0.05f;	// Omni directional.
			}
			else
				m_objectCollidable = true;
		}

		if (hs->type == SHideSpot::HST_DYNAMIC)
		{
			// Get the initial position of the entity which is associated with the dyn hidespot.
			IEntity*	pEnt = gEnv->pEntitySystem->GetEntity(hs->entityId);
			if(pEnt)
			{
				// Store the original entity position.
				m_dynCoverEntityPos = pEnt->GetWorldPos();
				// Store the cover position in local entity space.
				m_dynCoverPosLocal = pEnt->GetWorldRotation().GetInverted() * (hs->pos - pEnt->GetWorldPos());
				// Store the entity id.
				m_dynCoverEntityId = hs->entityId;
			}
			else
			{
				// Could not find entity, 
				m_dynCoverEntityId = 0;
				m_bIsValid = false;
			}
		}
		else
		{
			m_dynCoverEntityId = 0;
		}
	}

	if(m_bIsValid)
	{
		m_vLastHidePos = hidePos;
		m_vLastHideDir = hideDir;
		// Only reset the last used cover method if changing cover object.
		if(Distance::Point_Point2DSq(oldObjPos, m_objectPos) > sqr(0.1f))
			m_useCover = USECOVER_NONE;
		m_pathUpdateIter = 0;
		m_pathComplete = false;
		m_pathHurryUp = false;
		m_isUsingCover = true;
		m_id++;
	}

	if(!m_bIsValid && hidePos.GetLength() > 0.001f)
	{
		CryLog("Trying to set invalid hidespots!");
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::Update(CPipeUser *pOperand)
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_AI );

	if(m_bIsValid && !m_bIsSmartObject)
		UpdatePathExpand(pOperand);
}


bool	ProjectedPointOnLine(float& u, const Vec3& lineOrig, const Vec3& lineDir, const Vec3& lineNorm, const Vec3& pt, const Vec3& target)
{
	Plane	plane;
	plane.SetPlane(lineNorm, lineOrig);
	Ray	r(target, pt - target);
	if (r.direction.IsZero(0.000001f))
		return false;
	Vec3	intr(pt);
	bool	res = Intersect::Ray_Plane(r, plane, intr);
	u = lineDir.Dot(intr - lineOrig);
	return res;
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::GetCoverPoints(bool useLowCover, float peekOverLeft, float peekOverRight, const Vec3& targetPos, Vec3& hidePos,
																	 Vec3& peekPosLeft, Vec3& peekPosRight, bool& peekLeftClamped, bool& peekRightClamped, bool& coverCompromised) const
{
	// Calculate the extends of the shadow cast by the cover on the walkable line.
	// The returned hide point is in the middle of the umbra, and the peek points are at the extends.
	// The umbra is clipped to the walkable line and the clipping status is returned too.

	if(m_pathNorm.Dot(targetPos - m_pathOrig) < 0.0f)
	{
		hidePos = m_pathOrig;
		peekPosLeft = m_pathOrig;
		peekPosRight = m_pathOrig;
		peekLeftClamped = false;
		peekRightClamped = false;
		coverCompromised = true;
		return;
	}

	Plane	plane;
	plane.SetPlane(m_pathNorm, m_pathOrig);

	// Initialize with the center point of the cover.
	float	umbraLeft = FLT_MAX, umbraRight = -FLT_MAX;
	if(!m_pathComplete)
	{
		float	u = 0.0f;
		if(ProjectedPointOnLine(u, m_pathOrig, m_pathDir, m_pathNorm, m_coverPos, targetPos))
			umbraLeft = umbraRight = u;
	}


	bool	coverEmpty = false;

	// Calculate the umbra (shadow of the cover).
	std::list<Vec3>::const_iterator begin, end;
	if(useLowCover)
	{
		begin = m_lowCoverPoints.begin();
		end = m_lowCoverPoints.end();
		coverEmpty = m_lowCoverPoints.empty();
	}
	else
	{
		begin = m_highCoverPoints.begin();
		end = m_highCoverPoints.end();
		coverEmpty = m_highCoverPoints.empty();
	}

	if(!coverEmpty)
	{
		int	validPts = 0;

		for(std::list<Vec3>::const_iterator it = begin; it != end; ++it)
		{
			float	u = 0.0f;
			if(ProjectedPointOnLine(u, m_pathOrig, m_pathDir, m_pathNorm, (*it), targetPos))
			{
				umbraLeft = min(umbraLeft, u);
				umbraRight = max(umbraRight, u);
				validPts++;
			}
		}

		if(!validPts)
		{
			hidePos = m_pathOrig;
			peekPosLeft = m_pathOrig;
			peekPosRight = m_pathOrig;
			peekLeftClamped = false;
			peekRightClamped = false;
			coverCompromised = true;
			return;
		}
	}


	float	mid = (umbraLeft + umbraRight) * 0.5f;

	// Allow to reach over the edge or stay almost visible.
	if(umbraLeft - peekOverLeft > mid)
		umbraLeft = mid;
	else
		umbraLeft -= peekOverLeft;

	if(umbraRight + peekOverRight < mid)
		umbraRight = mid;
	else
		umbraRight += peekOverRight;

	if(umbraLeft > umbraRight)
		umbraLeft = umbraRight = (umbraLeft + umbraRight) * 0.5f;

	// Clamp the umbra to the walkable line.
	if(umbraLeft < m_pathLimitLeft)
	{
		umbraLeft = m_pathLimitLeft;
		peekLeftClamped = true;
	}
	else if(umbraLeft > m_pathLimitRight)
	{
		umbraLeft = m_pathLimitRight;
		peekLeftClamped = true;
	}
	else
		peekLeftClamped = false;

	if(umbraRight > m_pathLimitRight)
	{
		umbraRight = m_pathLimitRight;
		peekRightClamped = true;
	}
	else if(umbraRight < m_pathLimitLeft)
	{
		umbraRight = m_pathLimitLeft;
		peekRightClamped = true;
	}
	else
		peekRightClamped = false;

	hidePos = m_pathOrig + m_pathDir * (umbraLeft + umbraRight) * 0.5f;
	peekPosLeft = m_pathOrig + m_pathDir * umbraLeft;
	peekPosRight = m_pathOrig + m_pathDir * umbraRight;

	if(coverEmpty)
		coverCompromised = m_pathComplete && fabsf((umbraLeft + umbraRight) * 0.5f) > 2.0f;
	else
		coverCompromised = m_pathComplete && (peekLeftClamped && peekRightClamped) && fabs(umbraRight - umbraLeft) < 0.001f;
}

//
//-------------------------------------------------------------------------------------------------------------
bool CAIHideObject::IsValid()
{
	if(!m_bIsValid) return false;

	// Check if the entity where the dynamic cover was found has moved.
	if(m_dynCoverEntityId != 0)
	{
		IEntity*	pEnt = gEnv->pEntitySystem->GetEntity(m_dynCoverEntityId);
		if(!pEnt)
		{
			// The entity could not be found anymore -> compromised.
			m_bIsValid = false;
			return false;
		}
		else
		{
			// Check if the entity has been moved too much.
			if(Distance::Point_PointSq(pEnt->GetWorldPos(), m_dynCoverEntityPos) > sqr(0.3f))
			{
				m_bIsValid = false;
				return false;
			}

			// Check if the cover has been moved.
			Vec3	curPos = pEnt->GetWorldPos() + pEnt->GetWorldRotation() * m_dynCoverPosLocal;
			if(Distance::Point_PointSq(m_objectPos, curPos) > sqr(0.3f))
			{
				m_bIsValid = false;
				return false;
			}
		}
	}

	return m_bIsValid;
}

//
//-------------------------------------------------------------------------------------------------------------
bool CAIHideObject::IsCompromised(const CPipeUser* requester, const Vec3& targetPos)
{
	if(!IsValid())
		return true;

	bool	compromised(false);
	Vec3	hidePos, peekPosLeft, peekPosRight;
	bool	clampedLeft, clampedRight;
	GetCoverPoints(true, 0.1f, 0.1f, targetPos, hidePos, peekPosLeft, peekPosRight, clampedLeft, clampedRight, compromised);
	float	dist = GetDistanceToCoverPath(requester->GetPhysicsPos());
	float	safeRange = requester->GetParameters().m_fPassRadius + 1.3f;
	if(dist > safeRange)
		compromised = true;

	return compromised;
}

//
//-------------------------------------------------------------------------------------------------------------
bool CAIHideObject::IsNearCover(const CPipeUser* requester) const
{
	const float	safeRange = requester->GetParameters().m_fPassRadius + 2.0f;
	if(GetDistanceToCoverPath(requester->GetPhysicsPos()) < safeRange)
		return true;
	return false;
}

//
//-------------------------------------------------------------------------------------------------------------
float CAIHideObject::GetCoverWidth(bool useLowCover)
{
	if(!m_pathComplete)
	{
		// Over estimate a bit.
		if(useLowCover)
			return max(0.5f, m_lowCoverWidth);
		else
			return max(0.5f, m_highCoverWidth);
	}
	else
	{
		if(useLowCover)
			return m_lowCoverWidth;
		else
			return m_highCoverWidth;
	}
}

//
//-------------------------------------------------------------------------------------------------------------
float	CAIHideObject::GetDistanceToCoverPath(const Vec3& pt) const
{
	Lineseg	coverPath(m_pathOrig + m_pathDir * m_pathLimitLeft, m_pathOrig + m_pathDir * m_pathLimitRight);
	float	t;
	float	distToLine = Distance::Point_Lineseg2D(pt, coverPath, t);
	Vec3	ptOnLine(coverPath.GetPoint(t));
	float	heightDist = fabsf(ptOnLine.z - pt.z);
	return max(distToLine, heightDist);
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::SetupPathExpand(CPipeUser *pOperand)
{
	const float STEP_SIZE = 1.0f;
	const int STEP_SUB_DIV = 3;
	const float LINE_LEN = GetMaxCoverPathLen();

//	const Vec3&	lastHidePos(GetLastHidePos());
	Vec3	hidePos(GetObjectPos());
	Vec3	hideDir(GetObjectDir());
	float	hideRadius(GetObjectRadius());
	float	agentRadius = pOperand->GetParameters().m_fPassRadius;
	
	// Move the path origin on ground.
	Vec3	floorPos(GetLastHidePos());
	floorPos.z += 0.5f;
	GetFloorPos(floorPos, GetLastHidePos(), walkabilityFloorUpDist, walkabilityFloorDownDist, walkabilityDownRadius, AICE_ALL);
	m_pathOrig = floorPos;
	m_distToCover = COVER_CLEARANCE + agentRadius + max(0.0f, hideRadius);

	m_pathNorm = m_vLastHideDir;
	m_coverPos = hidePos;

	m_pathNorm.z = 0;
	m_pathNorm.NormalizeSafe();
	m_pathDir.Set(m_pathNorm.y, -m_pathNorm.x, 0);

	const float SAMPLE_RADIUS = 0.15f;

	Vec3 hitPos;
	float	hitDist;
	Vec3	lowCoverOrig(m_pathOrig.x, m_pathOrig.y, m_pathOrig.z + LOW_COVER_OFFSET);
	Vec3	highCoverOrig(m_pathOrig.x, m_pathOrig.y, m_pathOrig.z + HIGH_COVER_OFFSET);
	m_lowCoverValid = IntersectSweptSphere(&hitPos, hitDist, Lineseg(lowCoverOrig, lowCoverOrig + m_pathNorm * (m_distToCover + 0.5f)), SAMPLE_RADIUS, AICE_ALL);
	m_highCoverValid = IntersectSweptSphere(&hitPos, hitDist, Lineseg(highCoverOrig, highCoverOrig + m_pathNorm * (m_distToCover + 0.5f)), SAMPLE_RADIUS, AICE_ALL);

	m_lowLeftEdgeValid = false;
	m_lowRightEdgeValid = false;
	m_highLeftEdgeValid = false;
	m_highRightEdgeValid = false;

	m_lowCoverPoints.clear();
	m_highCoverPoints.clear();

	m_pathLimitLeft = 0; //-m_objectRadius/2;
	m_pathLimitRight = 0; //m_objectRadius/2;

	m_pathUpdateIter = 0;
	m_pathComplete = false;
}

//
//-------------------------------------------------------------------------------------------------------------
bool CAIHideObject::IsSegmentValid(CPipeUser *pOperand, const Vec3& posFrom, const Vec3& posTo)
{
	CAISystem* pAISystem = GetAISystem();

	int nBuildingID;
	IVisArea* pVisArea;

	IAISystem::ENavigationType	navType = pAISystem->CheckNavigationType(posFrom, nBuildingID, pVisArea, pOperand->GetMovementAbility().pathfindingProperties.navCapMask);

	if(navType == IAISystem::NAV_VOLUME || navType == IAISystem::NAV_FLIGHT || navType == IAISystem::NAV_WAYPOINT_3DSURFACE || navType == IAISystem::NAV_TRIANGULAR)
	{
		CNavRegion *pRegion = pAISystem->GetNavRegion(navType, pAISystem->GetGraph());
		if(pRegion)
		{
			NavigationBlockers	navBlocker;
			if(pRegion->CheckPassability(posFrom, posTo, pOperand->GetParameters().m_fPassRadius, navBlocker))
			{
				if(navType == IAISystem::NAV_TRIANGULAR)
				{
					// Make sure not to enter forbidden area.
					if (pAISystem->IsPointInForbiddenRegion(posTo))
						return false;
				}
				return true;
			}
		}
	}
	else if(navType == IAISystem::NAV_WAYPOINT_HUMAN)
	{
		const SpecialArea* sa = pAISystem->GetSpecialArea(nBuildingID);
		if (sa)
		{
			const ListPositions	& buildingPolygon = sa->GetPolygon();
			return CheckWalkability(posFrom, posTo, pOperand->GetParameters().m_fPassRadius + 0.1f - walkabilityRadius, true, buildingPolygon, AICE_ALL);
		}
		else
		{
			AIWarning("COPUseCover::IsSegmentValid: Cannot find special area for building ID %d", nBuildingID);
			return false;
		}
	}

	return false;
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::SampleCover(CPipeUser *pOperand, float& maxCover, float& maxDepth,
																const Vec3& startPos, float maxWidth,
																float sampleDist, float sampleRad, float sampleDepth,
																std::list<Vec3>& points, bool pushBack, bool& reachedEdge)
{
	const int REFINE_SAMPLES = 4;

	Vec3	hitPos;
	Vec3	checkDir = m_pathNorm * sampleDepth;
	maxCover = 0;
	maxDepth = 0;

	reachedEdge = false;

	// Linear rough samples

	int n = 1 + (int)floorf(fabs(maxWidth) / sampleDist);
	float	deltaWidth = 1.0f / (float)n * maxWidth;

	for(int i = 0; i < n; i++)
	{
		float	w = deltaWidth * (i+1);
		Vec3	pos = startPos + m_pathDir * w;
		float	d = 0;

		if(!IntersectSweptSphere(&hitPos, d, Lineseg(pos, pos + checkDir), sampleRad, AICE_ALL))
		{
			reachedEdge = true;
			break;
		}
		else
		{
			maxCover = w;
			if(pushBack)
				points.push_back(pos + m_pathNorm * d);
			else
				points.push_front(pos + m_pathNorm * d);
		}
		maxDepth = max(maxDepth, d);
	}
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::SampleCoverRefine(CPipeUser *pOperand, float& maxCover, float& maxDepth,
																			const Vec3& startPos, float maxWidth,
																			float sampleDist, float sampleRad, float sampleDepth,
																			std::list<Vec3>& points, bool pushBack)
{
	const int REFINE_SAMPLES = 4;

	Vec3	hitPos;
	int n = 1 + (int)floorf(fabs(maxWidth) / sampleDist);
	float	deltaWidth = 1.0f / (float)n * maxWidth;
	Vec3	checkDir = m_pathNorm * sampleDepth;

	// Refine few iterations
	float	t = 0.0f;
	float	dt = 0.5f;
	for(int j = 0; j < REFINE_SAMPLES; j++)
	{
		Vec3	pos = startPos + m_pathDir * (maxCover + deltaWidth * t);

		float	dist = 0;
		if(!IntersectSweptSphere(&hitPos, dist, Lineseg(pos, pos + checkDir), sampleRad, AICE_ALL))
			t -= dt;
		else
			t += dt;
		maxDepth = max(maxDepth, dist);
		dt *= 0.5f;
	}

	maxCover += deltaWidth * t;

	if(maxDepth < 0.01f)
		maxDepth = sampleDepth;

	if(pushBack)
		points.push_back(startPos + m_pathDir * maxCover + m_pathNorm * maxDepth);
	else
		points.push_front(startPos + m_pathDir * maxCover + m_pathNorm * maxDepth);
}


//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::SampleLine(CPipeUser *pOperand, float& maxMove, float maxWidth, float sampleDist)
{
	// Linear rough samples
	Vec3	lastPos(m_pathOrig);
	int n = 1 + (int)floorf(fabs(maxWidth) / sampleDist);
	float	deltaWidth = 1.0f / (float)n * maxWidth;

	for(int i = 0; i < n; i++)
	{
		const float	w = deltaWidth * (i+1);
		Vec3	pos = m_pathOrig + m_pathDir * w;
		if(!IsSegmentValid(pOperand, lastPos, pos))
			break;
		else
			maxMove = w;
		lastPos = pos;
	}
}

void CAIHideObject::SampleLineRefine(CPipeUser *pOperand, float& maxMove, float maxWidth, float sampleDist)
{
	const int REFINE_SAMPLES = 4;

	// Refine few iterations
	Vec3	lastPos(m_pathOrig + m_pathDir * maxMove);
	int n = 1 + (int)floorf(fabs(maxWidth) / sampleDist);
	float	deltaWidth = 1.0f / (float)n * maxWidth;

	float	t = 0.0f;
	float	dt = 0.5f;
	for(int j = 0; j < REFINE_SAMPLES; j++)
	{
		Vec3	pos = m_pathOrig + m_pathDir * (maxMove + deltaWidth * t);
		if(!IsSegmentValid(pOperand, lastPos, pos))
			t -= dt;
		else
			t += dt;
		dt *= 0.5f;
	}

	maxMove += deltaWidth * t;
}

//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::UpdatePathExpand(CPipeUser *pOperand)
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_AI );

	if(m_pathUpdateIter == 0)
		SetupPathExpand(pOperand);

	if(m_pathComplete)
		return;

	const float STEP_SIZE = 1.0f;
	const int STEP_SUB_DIV = 3;
	const float LINE_LEN = GetMaxCoverPathLen();
	const float	SAMPLE_DIST = 0.45f;
	const float SAMPLE_RADIUS = 0.15f;

	Vec3	lowCoverOrig(m_pathOrig.x, m_pathOrig.y, m_pathOrig.z + LOW_COVER_OFFSET);
	Vec3	highCoverOrig(m_pathOrig.x, m_pathOrig.y, m_pathOrig.z + HIGH_COVER_OFFSET);
	Vec3 hitPos;

	int		maxIter = 1;
	int		iter = 0;

	if(m_pathHurryUp)
		maxIter = 2;

	while(!m_pathComplete && iter < maxIter)
	{
		switch(m_pathUpdateIter)
		{
		case 0:
			SampleLine(pOperand, m_pathLimitLeft, -LINE_LEN/2, STEP_SIZE);
			iter++;
			break;
		case 1:
			SampleLineRefine(pOperand, m_pathLimitLeft, -LINE_LEN/2, STEP_SIZE);
			iter++;
			break;
		case 2:
			SampleLine(pOperand, m_pathLimitRight, LINE_LEN/2, STEP_SIZE);
			iter++;
			break;
		case 3:
			SampleLineRefine(pOperand, m_pathLimitRight, LINE_LEN/2, STEP_SIZE);
			iter++;
			break;
		case 4:
			if(m_lowCoverValid)
			{
				SampleCover(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, false, m_lowLeftEdgeValid);
				iter++;
			}
			break;
		case 5:
			if(m_lowCoverValid)
			{
				SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, false);
				iter++;
			}
			break;
		case 6:
			if(m_lowCoverValid)
			{
				SampleCover(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, true, m_lowRightEdgeValid);
				iter++;
			}
			break;
		case 7:
			if(m_lowCoverValid)
			{
				SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, true);
				iter++;
			}
			break;
		case 8:
			if(m_highCoverValid)
			{
				SampleCover(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, false, m_highLeftEdgeValid);
				iter++;
			}
			break;
		case 9:
			if(m_highCoverValid)
			{
				SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, false);
				iter++;
			}
			break;
		case 10:
			if(m_highCoverValid)
			{
				SampleCover(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, true, m_highRightEdgeValid);
				iter++;
			}
			break;
		case 11:
			if(m_highCoverValid)
			{
				SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, true);
				iter++;
			}
			break;
		default:
			m_pathComplete = true;
			break;
		}
		m_pathUpdateIter++;
	}

	// Update cover width.
	m_lowCoverWidth = 0.0f;
	m_highCoverWidth = 0.0f;

	float	cmin, cmax;
	std::list<Vec3>::iterator	end;

	cmin = 0.0f; cmax = 0.0f;
	end = m_lowCoverPoints.end();
	for(std::list<Vec3>::iterator it = m_lowCoverPoints.begin(); it != end; ++it)
	{
		float	u = m_pathDir.Dot((*it) - m_pathOrig);
		cmin = min(cmin, u);
		cmax = max(cmax, u);
	}

	// Adjust the cover to match the non-collidable cover width if applicable.
	if(m_pathComplete && !m_objectCollidable && m_objectRadius > 0.1f)
	{
		if(cmin > -m_objectRadius)
		{
			m_lowCoverPoints.push_front(lowCoverOrig + m_pathNorm * m_distToCover + m_pathDir * -m_objectRadius);
			m_lowLeftEdgeValid = true;
			cmin = -m_objectRadius;
		}
		if(cmax < m_objectRadius)
		{
			m_lowCoverPoints.push_back(lowCoverOrig + m_pathNorm * m_distToCover + m_pathDir * m_objectRadius);
			m_lowRightEdgeValid = true;
			cmax = m_objectRadius;
		}
	}

	m_lowCoverWidth = cmax - cmin;

	cmin = 0.0f; cmax = 0.0f;
	end = m_highCoverPoints.end();
	for(std::list<Vec3>::iterator it = m_highCoverPoints.begin(); it != end; ++it)
	{
		float	u = m_pathDir.Dot((*it) - m_pathOrig);
		cmin = min(cmin, u);
		cmax = max(cmax, u);
	}

	// Adjust the cover to match the non-collidable cover width if applicable.
	if(m_pathComplete && !m_objectCollidable && m_objectRadius > 0.1f && m_objectHeight > 0.8f)
	{
		if(cmin > -m_objectRadius)
		{
			m_highCoverPoints.push_front(highCoverOrig + m_pathNorm * m_distToCover + m_pathDir * -m_objectRadius);
			m_highLeftEdgeValid = true;
			cmin = -m_objectRadius;
		}
		if(cmax < m_objectRadius)
		{
			m_highCoverPoints.push_back(highCoverOrig + m_pathNorm * m_distToCover + m_pathDir * m_objectRadius);
			m_highRightEdgeValid = true;
			cmax = m_objectRadius;
		}
	}

	m_highCoverWidth = cmax - cmin;


/*	if(m_lowCoverWidth < 0.01f && m_objectRadius > 0.01f)
		m_lowCoverWidth = m_objectRadius * 2.0f;
	if(m_highCoverWidth < 0.01f && m_objectRadius > 0.01f)
		m_highCoverWidth = m_objectRadius * 2.0f;*/

/*
	SampleLine(pOperand, m_pathLimitLeft, -LINE_LEN/2, STEP_SIZE);
	SampleLineRefine(pOperand, m_pathLimitLeft, -LINE_LEN/2, STEP_SIZE);
	SampleLine(pOperand, m_pathLimitRight, LINE_LEN/2, STEP_SIZE);
	SampleLineRefine(pOperand, m_pathLimitRight, LINE_LEN/2, STEP_SIZE);

	if(m_lowCoverValid)
	{
		SampleCover(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, false);
		SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, false);

		SampleCover(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, true);
		SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, lowCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_lowCoverPoints, true);
	}

	if(m_highCoverValid)
	{
		SampleCover(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, false);
		SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitLeft, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, false);

		SampleCover(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, true);
		SampleCoverRefine(pOperand, m_tempCover, m_tempDepth, highCoverOrig, m_pathLimitRight, SAMPLE_DIST, SAMPLE_RADIUS, m_distToCover + 1.0f, m_highCoverPoints, true);
	}*/

}


//
//-------------------------------------------------------------------------------------------------------------
void CAIHideObject::DebugDraw(IRenderer *pRenderer)
{
	if(m_bIsSmartObject)
		return;

	ColorF	white(1, 1, 1);
	ColorF	red(1, 0, 0);
	ColorF	redTrans(1, 0, 0, 0.5f);

	pRenderer->GetIRenderAuxGeom()->DrawLine(GetLastHidePos() + Vec3(0,0,0.5f), white, GetObjectPos() + Vec3(0,0,0.5f), white);
	pRenderer->GetIRenderAuxGeom()->DrawLine(GetObjectPos() + Vec3(0,0,-0.5f), white, GetObjectPos() + Vec3(0,0,2.5f), white);

	if(m_pathUpdateIter == 0)
		return;

	// Draw walkability
	pRenderer->GetIRenderAuxGeom()->DrawLine(m_pathOrig + m_pathDir * m_pathLimitLeft, white,
		m_pathOrig + m_pathDir * m_pathLimitRight, white);

	size_t	maxPts = max(m_lowCoverPoints.size(), m_highCoverPoints.size());
	std::vector<Vec3>	tris;
	tris.resize((maxPts + 1) * 12);

	// Draw low cover
	if(m_lowCoverPoints.size() > 1)
	{
		std::list<Vec3>::iterator	cur = m_lowCoverPoints.begin(), next = m_lowCoverPoints.begin();
		++next;
		size_t i = 0;
		while(next != m_lowCoverPoints.end())
		{
			const Vec3&	left = (*cur);
			const Vec3&	right = (*next);
			// Back
			tris[i+0] = left + Vec3(0, 0, -0.5f);
			tris[i+1] = right + Vec3(0, 0, 0.5f);
			tris[i+2] = right + Vec3(0, 0, -0.5f);
			tris[i+3] = left + Vec3(0, 0, -0.5f);
			tris[i+4] = left + Vec3(0, 0, 0.5f);
			tris[i+5] = right + Vec3(0, 0, 0.5f);
			i += 6;
			// Front
			tris[i+0] = left + Vec3(0, 0, -0.5f);
			tris[i+2] = right + Vec3(0, 0, 0.5f);
			tris[i+1] = right + Vec3(0, 0, -0.5f);
			tris[i+3] = left + Vec3(0, 0, -0.5f);
			tris[i+5] = left + Vec3(0, 0, 0.5f);
			tris[i+4] = right + Vec3(0, 0, 0.5f);
			i += 6;
			cur = next;
			++next;
		}
		AIAssert(i <= tris.size());
		pRenderer->GetIRenderAuxGeom()->DrawTriangles(&tris[0], i, redTrans);

		// Draw edge markers
		if(m_lowLeftEdgeValid)
			pRenderer->GetIRenderAuxGeom()->DrawCone(m_lowCoverPoints.front() + Vec3(0,0,0.5f), Vec3(0,0,-1), 0.07f, 1.2f, red);
		if(m_lowRightEdgeValid)
			pRenderer->GetIRenderAuxGeom()->DrawCone(m_lowCoverPoints.back() + Vec3(0,0,0.5f), Vec3(0,0,-1), 0.07f, 1.2f, red);
	}

	// Draw high cover
	if(m_highCoverPoints.size() > 1)
	{
		std::list<Vec3>::iterator	cur = m_highCoverPoints.begin(), next = m_highCoverPoints.begin();
		++next;
		size_t i = 0;
		while(next != m_highCoverPoints.end())
		{
			const Vec3&	left = (*cur);
			const Vec3&	right = (*next);
			// Back
			tris[i+0] = left + Vec3(0, 0, -0.5f);
			tris[i+1] = right + Vec3(0, 0, 0.25f);
			tris[i+2] = right + Vec3(0, 0, -0.5f);
			tris[i+3] = left + Vec3(0, 0, -0.5f);
			tris[i+4] = left + Vec3(0, 0, 0.25f);
			tris[i+5] = right + Vec3(0, 0, 0.25f);
			i += 6;
			// Front
			tris[i+0] = left + Vec3(0, 0, -0.5f);
			tris[i+2] = right + Vec3(0, 0, 0.25f);
			tris[i+1] = right + Vec3(0, 0, -0.5f);
			tris[i+3] = left + Vec3(0, 0, -0.5f);
			tris[i+5] = left + Vec3(0, 0, 0.25f);
			tris[i+4] = right + Vec3(0, 0, 0.25f);
			i += 6;
			cur = next;
			++next;
		}
		AIAssert(i <= tris.size());
		pRenderer->GetIRenderAuxGeom()->DrawTriangles(&tris[0], i, redTrans);

		// Draw edge markers
		if(m_highLeftEdgeValid)
			pRenderer->GetIRenderAuxGeom()->DrawCone(m_highCoverPoints.front() + Vec3(0,0,0.25f), Vec3(0,0,-1), 0.07f, 0.75f, red);
		if(m_highRightEdgeValid)
			pRenderer->GetIRenderAuxGeom()->DrawCone(m_highCoverPoints.back() + Vec3(0,0,0.25f), Vec3(0,0,-1), 0.07f, 0.75f, red);
	}
}

//
//----------------------------------------------------------------------------------------------------
bool CAIHideObject::HasLowCover() const
{
	return !m_lowCoverPoints.empty() && m_lowCoverPoints.size() > 1;
}

bool CAIHideObject::HasHighCover() const
{
	return !m_highCoverPoints.empty() && m_highCoverPoints.size() > 1;
}

bool CAIHideObject::IsLeftEdgeValid(bool useLowCover) const
{
	return useLowCover ? m_lowLeftEdgeValid : m_highLeftEdgeValid;
}

bool CAIHideObject::IsRightEdgeValid(bool useLowCover) const
{
	return useLowCover ? m_lowRightEdgeValid : m_highRightEdgeValid;
}

//
//----------------------------------------------------------------------------------------------------
void CAIHideObject::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup("AIHideObject");
	{
		ser.Value("m_bIsValid",m_bIsValid);
		if(m_bIsValid)
		{
			ser.Value("m_isUsingCover",m_isUsingCover);
			ser.Value("m_objectPos",m_objectPos);
			ser.Value("m_objectDir",m_objectDir);
			ser.Value("m_objectRadius",m_objectRadius);
			ser.Value("m_objectCollidable",m_objectCollidable);
			ser.Value("m_vLastHidePos",	m_vLastHidePos);
			ser.Value("m_vLastHideDir",m_vLastHideDir);
			ser.Value("m_bIsSmartObject",m_bIsSmartObject);
			m_HideSmartObject.Serialize(ser);
			ser.Value("m_useCover",m_useCover);
			ser.Value("m_coverPos",m_coverPos);
			ser.Value("m_distToCover",m_distToCover);
			ser.Value("m_pathOrig",m_pathOrig);
			ser.Value("m_pathDir",m_pathDir);
			ser.Value("m_pathNorm",m_pathNorm);
			ser.Value("m_pathLimitLeft",m_pathLimitLeft);
			ser.Value("m_pathLimitRight",m_pathLimitRight);
			ser.Value("m_tempCover",m_tempCover);
			ser.Value("m_tempDepth",m_tempDepth);
			ser.Value("m_pathComplete",m_pathComplete);
			ser.Value("m_highCoverValid",m_highCoverValid);
			ser.Value("m_lowCoverValid",m_lowCoverValid);
			ser.Value("m_pathHurryUp",m_pathHurryUp);
			ser.Value("m_lowLeftEdgeValid",m_lowLeftEdgeValid);
			ser.Value("m_lowRightEdgeValid",m_lowRightEdgeValid);
			ser.Value("m_highLeftEdgeValid",m_highLeftEdgeValid);
			ser.Value("m_highRightEdgeValid",m_highRightEdgeValid);
			ser.Value("m_lowCoverPoints",m_lowCoverPoints);
			ser.Value("m_highCoverPoints",m_highCoverPoints);
			ser.Value("m_lowCoverWidth",m_lowCoverWidth);
			ser.Value("m_highCoverWidth",m_highCoverWidth);
			ser.Value("m_pathUpdateIter",m_pathUpdateIter);
			ser.Value("m_id",m_id);
			ser.Value("m_dynCoverEntityId", m_dynCoverEntityId);
			ser.Value("m_dynCoverEntityPos", m_dynCoverEntityPos);
			ser.Value("m_dynCoverPosLocal", m_dynCoverPosLocal);
		}
		ser.EndGroup();
	}
}
