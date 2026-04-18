////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   Area.cpp
//  Version:     v1.00
//  Created:     27/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Area.h"
#include <IRenderAuxGeom.h>
#include "ISound.h" // for pSound->GetId()

//////////////////////////////////////////////////////////////////////////
CArea::CArea( CAreaManager *pManager ):
m_VOrigin(0.0f),
m_VSize(0.0f),
m_PrevFade(-1.0f),
m_fProximity(5.0f),
m_fMaximumEffectRadius(0.0f),
m_AreaGroupID(-1),
m_nPriority(0),
m_AreaID(-1),
m_stepID(-1),
m_EntityID(0),
m_BoxMin(0),
m_BoxMax(0),
m_SphereCenter(0),
m_SphereRadius(0),
m_SphereRadius2(0),
m_bIsActive(false),
m_bObstructRoof(false),
m_bObstructFloor(false)
{
	m_AreaType = ENTITY_AREA_TYPE_SHAPE;
	m_InvMatrix.SetIdentity();
	m_pAreaManager = pManager;
	m_bInitialized = false;
	m_bHasSoundAttached = false;
	m_bAttachedSoundTested = false;
}


//////////////////////////////////////////////////////////////////////////
CArea::~CArea(void)
{
	ClearPoints();
	m_pAreaManager->Unregister(this);
}

//////////////////////////////////////////////////////////////////////////
void CArea::Release()
{
	delete this;
}

void CArea::SetAreaType( EEntityAreaType type ) 
{ 
	m_AreaType = type; 
	
	// a bit hacky way to prevent gravityvolumes being evaluated in the
	// AreaManager::UpdatePlayer function, that caused problems.
	if (m_AreaType == ENTITY_AREA_TYPE_GRAVITYVOLUME)
		m_pAreaManager->Unregister(this);

} 

// resets area - clears all segments in area
//////////////////////////////////////////////////////////////////////////
void	CArea::ClearPoints()
{
	a2DSegment*	carSegment;

	for(unsigned int sIdx=0; sIdx<m_vpSegments.size(); sIdx++)
	{
		carSegment = m_vpSegments[sIdx];
		delete carSegment;
	}
	m_vpSegments.clear();
	m_bInitialized = false;
}

//////////////////////////////////////////////////////////////////////////
unsigned CArea::MemStat()
{
	unsigned memSize = sizeof *this;

	memSize += m_vpSegments.size()*(sizeof(a2DSegment) + sizeof(a2DSegment*)) ;
	return memSize;	
}

//adds segment to area, calculates line parameters y=kx+b, sets horizontal/vertical flags
//////////////////////////////////////////////////////////////////////////
void	CArea::AddSegment(const a2DPoint& p0, const a2DPoint& p1)
{
	a2DSegment*	newSegment = new a2DSegment;

	//if this is horizontal line set flag. This segment is needed only for distance calculations
	if(p1.y == p0.y)
		newSegment->isHorizontal = true;
	else
		newSegment->isHorizontal = false;

	if( p0.x < p1.x )
	{
		newSegment->bbox.min.x = p0.x;
		newSegment->bbox.max.x = p1.x;
	}
	else
	{
		newSegment->bbox.min.x = p1.x;
		newSegment->bbox.max.x = p0.x;
	}

	if( p0.y < p1.y )
	{
		newSegment->bbox.min.y = p0.y;
		newSegment->bbox.max.y = p1.y;
	}
	else
	{
		newSegment->bbox.min.y = p1.y;
		newSegment->bbox.max.y = p0.y;
	}

	if(!newSegment->isHorizontal)
	{
		//if this is vertical line - spesial case
		if(p1.x == p0.x)
		{
			newSegment->k = 0;
			newSegment->b = p0.x;
		}
		else
		{
			newSegment->k = (p1.y - p0.y)/(p1.x - p0.x);
			newSegment->b = p0.y - newSegment->k*p0.x;
		}
	}
	else
	{
		newSegment->k = 0;
		newSegment->b = 0;
	}
	m_vpSegments.push_back( newSegment );
}

// calculates min distance from point within area to the border of area
// returns fade coefficient: Distance/m_Proximity
// [0 - on the very border of area,	1 - inside area, distance to border is more than m_Proximity]
//////////////////////////////////////////////////////////////////////////	
float	CArea::CalcDistToPoint(const a2DPoint& point) const
{
	if (!m_bInitialized)
		return -1;

	if (m_fProximity == 0.0f)
		return 1.0f;

	float	distMin = m_fProximity*m_fProximity;
	float	curDist;
	a2DBBox	proximityBox;

	proximityBox.max.x = point.x + m_fProximity;
	proximityBox.max.y = point.y + m_fProximity;
	proximityBox.min.x = point.x - m_fProximity;
	proximityBox.min.y = point.y - m_fProximity;

	for(unsigned int sIdx=0; sIdx<m_vpSegments.size(); sIdx++)
	{
		a2DSegment *curSg = m_vpSegments[sIdx];

		if(!m_vpSegments[sIdx]->bbox.BBoxOutBBox2D( proximityBox ))
		{
			if(m_vpSegments[sIdx]->isHorizontal)
			{
				if( point.x<m_vpSegments[sIdx]->bbox.min.x )
					curDist = m_vpSegments[sIdx]->bbox.min.DistSqr( point );
				else if( point.x>m_vpSegments[sIdx]->bbox.max.x )
					curDist = m_vpSegments[sIdx]->bbox.max.DistSqr( point );
				else
					curDist = fabsf( point.y-m_vpSegments[sIdx]->bbox.max.y );
				curDist *= curDist;
			}
			else
				if(m_vpSegments[sIdx]->k==0.0f)
				{
					if( point.y<m_vpSegments[sIdx]->bbox.min.y )
						curDist = m_vpSegments[sIdx]->bbox.min.DistSqr( point );
					else if( point.y>m_vpSegments[sIdx]->bbox.max.y )
						curDist = m_vpSegments[sIdx]->bbox.max.DistSqr( point );
					else
						curDist = fabsf( point.x-m_vpSegments[sIdx]->b );
					curDist *= curDist;
				}
				else
				{
					a2DPoint	intersection;
					float	b2, k2;
					k2 = -1.0f/m_vpSegments[sIdx]->k;
					b2 = point.y - k2*point.x;
					intersection.x = (b2 - m_vpSegments[sIdx]->b)/(m_vpSegments[sIdx]->k - k2);
					intersection.y = k2*intersection.x + b2;

					if( intersection.x<m_vpSegments[sIdx]->bbox.min.x)
						if( m_vpSegments[sIdx]->k<0 )
							curDist = point.DistSqr( m_vpSegments[sIdx]->bbox.min.x, m_vpSegments[sIdx]->bbox.max.y );
						else
							curDist = point.DistSqr( m_vpSegments[sIdx]->bbox.min );
					else if( intersection.x>m_vpSegments[sIdx]->bbox.max.x)
						if( m_vpSegments[sIdx]->k<0 )
							curDist = point.DistSqr( m_vpSegments[sIdx]->bbox.max.x, m_vpSegments[sIdx]->bbox.min.y );
						else
							curDist = point.DistSqr( m_vpSegments[sIdx]->bbox.max );
					else
						curDist = intersection.DistSqr( point );
				}
				if(curDist<distMin)
					distMin = curDist;	
		}
	}
	return cry_sqrtf(distMin)/m_fProximity;
}

// check if the point is within the area
// first BBox check, then count number of intersections for horizontal ray from point and area segments
// if the number is odd - the point is inside
//////////////////////////////////////////////////////////////////////////
bool	CArea::IsPointWithin(const Vec3& point3d, bool bIgnoreHeight) const
{
	if (!m_bInitialized)
		return false;

	if( m_AreaType == ENTITY_AREA_TYPE_SPHERE )
	{
		Vec3	sPnt = point3d - m_SphereCenter;
		if ((sPnt.GetLengthSquared()) < m_SphereRadius2 )
			return true;
		return false;
	}
	if( m_AreaType == ENTITY_AREA_TYPE_BOX )
	{
		Vec3 p3d=m_InvMatrix.TransformPoint(point3d);
		if (	(p3d.x<m_BoxMin.x) ||
			(p3d.y<m_BoxMin.y) ||
			(p3d.z<m_BoxMin.z) ||
			(p3d.x>m_BoxMax.x) ||
			(p3d.y>m_BoxMax.y) ||
			(p3d.z>m_BoxMax.z))
			return false;
		return true;
	}
	if( m_AreaType == ENTITY_AREA_TYPE_SHAPE )
	{
		if (!bIgnoreHeight)
		{
			if( m_VSize>0.0f )
				if( point3d.z<m_VOrigin || point3d.z>m_VOrigin + m_VSize )
					return false;
		}

		a2DPoint	*point = (CArea::a2DPoint*)(&point3d);

		if( m_AreaBBox.PointOutBBox2D( *point ) )
			return false;

		int	cntr=0;	

		for(unsigned int sIdx=0; sIdx<m_vpSegments.size(); sIdx++)
		{
			if(m_vpSegments[sIdx]->isHorizontal)
				continue;
			if(!m_vpSegments[sIdx]->bbox.PointOutBBox2DVertical( *point ))
				if( m_vpSegments[sIdx]->IntersectsXPosVertical( *point ) )
					cntr++;
				else
					if(!m_vpSegments[sIdx]->bbox.PointOutBBox2DVertical( *point ))
						if( m_vpSegments[sIdx]->IntersectsXPos( *point ) )
							cntr++;
		}
		return (cntr & 0x1);
	}

	// to keep the compiler happy and give more robustness to the code with the assert...
	assert(0); // can never reach this part
	return false; // default behavior
}

//	for editor use - if point is withing - returns min hor distance to border
//	if point out - returns -1
//////////////////////////////////////////////////////////////////////////
float	CArea::IsPointWithinDist(const Vec3& point3d) const
{
	if (!m_bInitialized)
		return -1;

	int	cntr = 0;	
	float	minDist = -1;
	//float	dist;

	if( m_AreaType == ENTITY_AREA_TYPE_SPHERE )
	{
		Vec3	sPnt = point3d - m_SphereCenter;
		if ((sPnt.GetLengthSquared()) < m_SphereRadius2 )
		{
			minDist = m_SphereRadius - sPnt.GetLength();
		}
		return minDist;
	}
	if( m_AreaType == ENTITY_AREA_TYPE_BOX )
	{
		Vec3 vOnHull;
		float fDistanceSq = ClosestPointOnHullDistSq(point3d, vOnHull);

		minDist = sqrt(fDistanceSq);

		//Vec3 p3d = m_InvMatrix.TransformPoint(point3d);
		//if (!(p3d.x < m_BoxMin.x) ||
		//	(p3d.y < m_BoxMin.y) ||
		//	(p3d.z < m_BoxMin.z) ||
		//	(p3d.x > m_BoxMax.x) ||
		//	(p3d.y > m_BoxMax.y) ||
		//	(p3d.z > m_BoxMax.z))
		//{
		//	minDist = min(p3d.x - m_BoxMin.x, m_BoxMax.x - p3d.x);
		//	minDist = min(minDist, m_BoxMax.y - p3d.y);
		//	minDist = min(minDist, p3d.y - m_BoxMin.y);
		//	minDist = min(minDist, m_BoxMax.z - p3d.z);
		//	minDist = min(minDist, p3d.z - m_BoxMin.z);
		//}
		return minDist;
	}

	if( m_AreaType == ENTITY_AREA_TYPE_SHAPE )
	{
		if (!IsPointWithin(point3d))
			return minDist;

		//a2DPoint	*point = (CArea::a2DPoint*)(&point3d);

		//if( m_AreaBBox.PointOutBBox2D( point ) )
		//return -1;

		/*
		for(unsigned int sIdx=0; sIdx<m_vpSegments.size(); sIdx++)
		{
		if(m_vpSegments[sIdx]->isHorizontal)
		continue;
		if( m_vpSegments[sIdx]->k==0 )
		dist = m_vpSegments[sIdx]->b - point->x;
		else
		if(!m_vpSegments[sIdx]->bbox.PointOutBBox2D( *point ))
		dist = m_vpSegments[sIdx]->GetIntersectX( *point );
		else
		continue;
		if( dist<0 )
		dist = -dist;
		else
		cntr++;
		if( minDist>dist || minDist<0)
		minDist = dist;
		}
		*/

		// check distance to every line segment, remember the closest
		int nSegmentSize = m_vpSegments.size();
		for(unsigned int sIdx=0; sIdx < nSegmentSize; sIdx++)
		{
			float fT;
			a2DSegment *curSg = m_vpSegments[sIdx];

			Vec3 startSeg(curSg->GetStart().x, curSg->GetStart().y, point3d.z);
			Vec3 endSeg(curSg->GetEnd().x, curSg->GetEnd().y, point3d.z);
			Lineseg line(startSeg, endSeg);

			/// Returns distance from a point to a line segment, ignoring the z coordinates
			float fDist = Distance::Point_Lineseg2D(point3d, line, fT);
			
			if (minDist == -1)
				minDist = fDist;

			minDist = min(fDist, minDist);
		}

		if (m_VSize > 0.0f)
		{
			float fDistToRoof = minDist + 1.0f;
			float fDistToFloor = minDist + 1.0f;
			
			if (!m_bObstructFloor)
				fDistToFloor = point3d.z - m_VOrigin;

			if (!m_bObstructRoof)
				fDistToRoof = m_VOrigin + m_VSize - point3d.z;

			float	fZDist = min(fDistToFloor, fDistToRoof);
			minDist = min(minDist, fZDist);
		}
	}

	return	minDist;
}


float CArea::ClosestPointOnHullDistSq(const Vec3 &Point3d, Vec3 &OnHull3d) const
{
	if (!m_bInitialized)
		return -1;

	float fClosestDistance = FLT_MAX;
	Vec3 Closest3d(0);

	switch(m_AreaType)
	{
	case	 ENTITY_AREA_TYPE_SHAPE:
		{
			a2DPoint	*point2d = (CArea::a2DPoint*)(&Point3d);

			//float fLowDist = max(Point3d.z, m_VOrigin) - Point3d.z;
			float fHighDist = 0.0f;
			float fDistToRoof = 0.0f;
			float fDistToFloor = 0.0f;
			float fZDistTemp = 0.0f;

			if (m_VSize)
			{
				// negative means from inside to hull
				fDistToRoof		= Point3d.z - (m_VOrigin+m_VSize);
				fDistToFloor	= m_VOrigin - Point3d.z;

				if (abs(fDistToFloor) < abs(fDistToRoof))
				{
					// below
					if (m_bObstructFloor)
					{
						fDistToFloor = 0.0f;
						fZDistTemp = fDistToRoof;
					}
					else
					{
						fDistToRoof = 0.0f;
						fZDistTemp = fDistToFloor;
					}
				}
				else
				{
					// above
					if (m_bObstructRoof)
					{
						fDistToRoof = 0.0f;
						fZDistTemp = fDistToFloor;
					}
					else
					{
						fDistToFloor = 0.0f;
						fZDistTemp = fDistToRoof;
					}
				}

				//fHighDist = Point3d.z - min(Point3d.z, m_VOrigin+m_VSize);
			}

			float fZDistSq = fZDistTemp*fZDistTemp;
			//float fZDistSq = (max(fLowDist, fHighDist)*max(fLowDist, fHighDist));
			float fXDistSq = 0.0f;

			bool bIsIn2DShape = IsPointWithin(Point3d, true);
			{
				//// point is not under or above area shape, so approach from the side
				// Find the line segment that is closest to the 2d point.
				for(unsigned int sIdx=0; sIdx<m_vpSegments.size(); sIdx++)
				{
					float fT;
					a2DSegment *curSg = m_vpSegments[sIdx];

					Vec3 startSeg(curSg->GetStart().x, curSg->GetStart().y, Point3d.z);
					Vec3 endSeg(curSg->GetEnd().x, curSg->GetEnd().y, Point3d.z);
					Lineseg line(startSeg, endSeg);

					/// Returns distance from a point to a line segment, ignoring the z coordinates
					fXDistSq = Distance::Point_Lineseg2DSq(Point3d, line, fT);

					float fThisDistance = 0.0f;

					if (bIsIn2DShape && fZDistSq)
						fThisDistance = min(fXDistSq, fZDistSq);
					else
						fThisDistance = fXDistSq+fZDistSq;

					// is this closer than the previous one?
					if (fThisDistance < fClosestDistance)
					{
						fClosestDistance = fThisDistance;
						// find closest point
						if (fZDistSq && fZDistSq < fXDistSq)
						{
							Closest3d = Point3d;
							Closest3d.z = Point3d.z + fDistToFloor - fDistToRoof;
						}
						else
							Closest3d = (line.GetPoint(fT));

					}
				}
			}

			OnHull3d = Closest3d;

			break;
		}

	case	 ENTITY_AREA_TYPE_SPHERE:
		{
			Vec3 Temp = Point3d - m_SphereCenter;
			OnHull3d = Temp.normalize() * m_SphereRadius;
			OnHull3d += m_SphereCenter;
			fClosestDistance = OnHull3d.GetSquaredDistance(Point3d);
			return fClosestDistance;

			break;
		}
	case	 ENTITY_AREA_TYPE_BOX:
		{
			Vec3 p3d = m_InvMatrix.TransformPoint(Point3d);

			//Matrix33 back = m_InvMatrix.Invert();
			AABB myAABB(m_BoxMin, m_BoxMax);
			//OBB myOBB = OBB::CreateOBBfromAABB(m_InvMatrix.Invert(), myAABB);

			fClosestDistance = Distance::Point_AABBSq(p3d, myAABB, OnHull3d);

			if (m_bObstructRoof && OnHull3d.z == myAABB.max.z)
			{
				// Point is on the roof plane, but may be on the edge already
				Vec2 vTop(OnHull3d.x, myAABB.max.y);
				Vec2 vLeft(myAABB.min.x, OnHull3d.y);
				Vec2 vLow(OnHull3d.x, myAABB.min.y);
				Vec2 vRight(myAABB.max.x, OnHull3d.y);

				float fDistanceToTop		= p3d.GetSquaredDistance2D(vTop);
				float fDistanceToLeft		= p3d.GetSquaredDistance2D(vLeft);
				float fDistanceToLow		= p3d.GetSquaredDistance2D(vLow);
				float fDistanceToRight	= p3d.GetSquaredDistance2D(vRight);
				float fTempMinDistance = fDistanceToTop;

				OnHull3d = vTop;

				if (fDistanceToLeft < fTempMinDistance)
				{
					OnHull3d = vLeft;
					fTempMinDistance = fDistanceToLeft;
				}

				if (fDistanceToLow < fTempMinDistance)
				{
					OnHull3d = vLow;
					fTempMinDistance = fDistanceToLow;
				}

				if (fDistanceToRight < fTempMinDistance)
				{
					OnHull3d = vRight;
					fTempMinDistance = fDistanceToRight;
				}

				OnHull3d.z = min(myAABB.max.z, p3d.z);
				fClosestDistance = OnHull3d.GetSquaredDistance(p3d);

			}

			if (m_bObstructFloor && OnHull3d.z == myAABB.min.z)
			{
				// Point is on the roof plane, but may be on the edge already
				Vec2 vTop(OnHull3d.x, myAABB.max.y);
				Vec2 vLeft(myAABB.min.x, OnHull3d.y);
				Vec2 vLow(OnHull3d.x, myAABB.min.y);
				Vec2 vRight(myAABB.max.x, OnHull3d.y);

				float fDistanceToTop		= p3d.GetSquaredDistance2D(vTop);
				float fDistanceToLeft		= p3d.GetSquaredDistance2D(vLeft);
				float fDistanceToLow		= p3d.GetSquaredDistance2D(vLow);
				float fDistanceToRight	= p3d.GetSquaredDistance2D(vRight);
				float fTempMinDistance = fDistanceToTop;

				OnHull3d = vTop;

				if (fDistanceToLeft < fTempMinDistance)
				{
					OnHull3d = vLeft;
					fTempMinDistance = fDistanceToLeft;
				}
					OnHull3d = vLeft;

				if (fDistanceToLow < fTempMinDistance)
				{
					OnHull3d = vLow;
					fTempMinDistance = fDistanceToLow;
				}
					OnHull3d = vLow;

				if (fDistanceToRight < fTempMinDistance)
				{
					OnHull3d = vRight;
					fTempMinDistance = fDistanceToRight;
				}
					OnHull3d = vRight;

				OnHull3d.z = max(myAABB.min.z, p3d.z);
				fClosestDistance = OnHull3d.GetSquaredDistance(p3d);
			}

			OnHull3d = m_InvMatrix.GetInvertedFast().TransformPoint(OnHull3d);

			// TODO transform point back to world
			break;
		}
	}

	return fClosestDistance;

}


float CArea::PointNearDistSq(const Vec3 &Point3d, Vec3 &OnHull3d) const
{
	if (!m_bInitialized)
		return -1;

	float fClosestDistance = FLT_MAX;
	Vec3 Closest3d(0);

	switch(m_AreaType)
	{
	case	 ENTITY_AREA_TYPE_SHAPE:
		{
			a2DPoint	*point = (CArea::a2DPoint*)(&Point3d);

			float fZDistSq = 0.0f;
			float fXDistSq = FLT_MAX;

			// first find the closest edge
			{
				int nSegmentSize = m_vpSegments.size();
				for(unsigned int sIdx=0; sIdx<nSegmentSize; sIdx++)
				{
					float fT;
					a2DSegment *curSg = m_vpSegments[sIdx];

					Vec3 startSeg(curSg->GetStart().x, curSg->GetStart().y, Point3d.z);
					Vec3 endSeg(curSg->GetEnd().x, curSg->GetEnd().y, Point3d.z);
					Lineseg line(startSeg, endSeg);

					/// Returns distance from a point to a line segment, ignoring the z coordinates
					float fThisXDistSq = Distance::Point_Lineseg2DSq(Point3d, line, fT);

					//+fZDistSq
					if (fThisXDistSq < fXDistSq)
					{
						// find closest point
						fXDistSq = fThisXDistSq;
						Closest3d = (line.GetPoint(fT));
						//Closest3d.z = Point3d.z;
					}
				}
			}

			// now we have Closest3d being the point on the 2D hull of the shape

			if (m_VSize)
			{
				// negative means from inside to hull
				float fDistToRoof = Point3d.z - (m_VOrigin+m_VSize);		
				float fDistToFloor = m_VOrigin - Point3d.z;

				float fZRoofSq = fDistToRoof*fDistToRoof;
				float fZFloorSq = fDistToFloor*fDistToFloor;

				bool bIsIn2DShape = IsPointWithin(Point3d, true);

				if (bIsIn2DShape)
				{
					// point is below, in, or above the area

					if ((Point3d.z < m_VOrigin+m_VSize && Point3d.z > m_VOrigin))
					{
						// Point is inside z-boundary
						if (!m_bObstructRoof && (fZRoofSq < fXDistSq) && (fZRoofSq < fZFloorSq))
						{
							// roof is closer than side
							fZDistSq = fZRoofSq;
							Closest3d = Point3d;
							fXDistSq = 0.0f;
						}

						if (!m_bObstructFloor && (fZFloorSq < fXDistSq) && (fZFloorSq < fZRoofSq))
						{
							// floor is closer than side
							fZDistSq = fZFloorSq;
							Closest3d = Point3d;
							fXDistSq = 0.0f;
						}

						// correcting z-axis value
						if (fZRoofSq < fZFloorSq)
							Closest3d.z = Point3d.z - fDistToRoof;
						else
							Closest3d.z = Point3d.z - fDistToFloor;


					}
					else
					{
						// point is above or below area

						if (abs(fDistToRoof) < abs(fDistToFloor))
						{
							// being above
							if (!m_bObstructRoof)
							{
								// perpendicular point to Roof
								fXDistSq = 0.0f;
								fZDistSq = fZRoofSq;
								Closest3d = Point3d;
							}
							// correcting z axis value
							Closest3d.z = m_VOrigin + m_VSize;
						}
						else
						{
							// being below
							if (!m_bObstructFloor)
							{
								// perpendicular point to Floor
								fXDistSq = 0.0f;
								fZDistSq = fZFloorSq;
								Closest3d = Point3d;
							}
							// correcting z axis value
							Closest3d.z = m_VOrigin;
						}
					}				
				}
				else
				{
					// outside of 2D Shape, so diagonal or only to the side
					if ((Point3d.z > m_VOrigin+m_VSize || Point3d.z < m_VOrigin))
					{
						// Point is outside z-bondary
						// point is above or below area
						if (abs(fDistToRoof) < abs(fDistToFloor))
						{
							// being above
							fZDistSq = fZRoofSq;
							Closest3d.z = m_VOrigin+m_VSize;

						}
						else
						{
							// being below
							fZDistSq = fZFloorSq;
							Closest3d.z = m_VOrigin;
						}
					}
					else
					{
						// on the side and outside of the area, so point is on the face
						// correct z-Value
						Closest3d.z = Point3d.z;
					}

				}
			}
			else
			{
				// infinite high area
				// Closest is on an edge, ZDistance is 0, so nothing really to do here
			}

			fClosestDistance = fXDistSq + fZDistSq;
			OnHull3d = Closest3d;

			break;
		}

	case	 ENTITY_AREA_TYPE_SPHERE:
		{
			Vec3 Temp = Point3d - m_SphereCenter;
			OnHull3d = Temp.normalize() * m_SphereRadius;
			OnHull3d += m_SphereCenter;
			fClosestDistance = OnHull3d.GetSquaredDistance(Point3d);
			return fClosestDistance;

			break;
		}
	case	 ENTITY_AREA_TYPE_BOX:
		{
			Vec3 p3d = m_InvMatrix.TransformPoint(Point3d);

			//Matrix33 back = m_InvMatrix.Invert();
			AABB myAABB(m_BoxMin, m_BoxMax);
			//OBB myOBB = OBB::CreateOBBfromAABB(m_InvMatrix.Invert(), myAABB);

			fClosestDistance = Distance::Point_AABBSq(p3d, myAABB, OnHull3d);
			
			if (m_bObstructRoof && OnHull3d.z == myAABB.max.z)
			{
				// Point is on the roof plane, but may be on the edge already
				Vec2 vTop(OnHull3d.x, myAABB.max.y);
				Vec2 vLeft(myAABB.min.x, OnHull3d.y);
				Vec2 vLow(OnHull3d.x, myAABB.min.y);
				Vec2 vRight(myAABB.max.x, OnHull3d.y);

				float fDistanceToTop		= p3d.GetSquaredDistance2D(vTop);
				float fDistanceToLeft		= p3d.GetSquaredDistance2D(vLeft);
				float fDistanceToLow		= p3d.GetSquaredDistance2D(vLow);
				float fDistanceToRight	= p3d.GetSquaredDistance2D(vRight);
				float fTempMinDistance = fDistanceToTop;

				OnHull3d = vTop;

				if (fDistanceToLeft < fTempMinDistance)
				{
					OnHull3d = vLeft;
					fTempMinDistance = fDistanceToLeft;
				}

				if (fDistanceToLow < fTempMinDistance)
				{
					OnHull3d = vLow;
					fTempMinDistance = fDistanceToLow;
				}

				if (fDistanceToRight < fTempMinDistance)
				{
					OnHull3d = vRight;
					fTempMinDistance = fDistanceToRight;
				}

				OnHull3d.z = min(myAABB.max.z, p3d.z);
				fClosestDistance = OnHull3d.GetSquaredDistance(p3d);

			}

			if (m_bObstructFloor && OnHull3d.z == myAABB.min.z)
			{
				// Point is on the roof plane, but may be on the edge already
				Vec2 vTop(OnHull3d.x, myAABB.max.y);
				Vec2 vLeft(myAABB.min.x, OnHull3d.y);
				Vec2 vLow(OnHull3d.x, myAABB.min.y);
				Vec2 vRight(myAABB.max.x, OnHull3d.y);

				float fDistanceToTop		= p3d.GetSquaredDistance2D(vTop);
				float fDistanceToLeft		= p3d.GetSquaredDistance2D(vLeft);
				float fDistanceToLow		= p3d.GetSquaredDistance2D(vLow);
				float fDistanceToRight	= p3d.GetSquaredDistance2D(vRight);
				float fTempMinDistance = fDistanceToTop;

				OnHull3d = vTop;

				if (fDistanceToLeft < fTempMinDistance)
				{
					OnHull3d = vLeft;
					fTempMinDistance = fDistanceToLeft;
				}

				if (fDistanceToLow < fTempMinDistance)
				{
					OnHull3d = vLow;
					fTempMinDistance = fDistanceToLow;
				}

				if (fDistanceToRight < fTempMinDistance)
				{
					OnHull3d = vRight;
					fTempMinDistance = fDistanceToRight;
				}

				OnHull3d.z = max(myAABB.min.z, p3d.z);
				fClosestDistance = OnHull3d.GetSquaredDistance(p3d);
			}

			OnHull3d = m_InvMatrix.GetInvertedFast().TransformPoint(OnHull3d);

			// TODO transform point back to world
			break;
		}
	}

	return fClosestDistance;

}

float CArea::PointNearDistSq(const Vec3 &Point3d, bool bIgnoreHeight) const
{
	if (!m_bInitialized)
		return -1;

	float fClosestDistance = FLT_MAX;

	switch(m_AreaType)
	{
	case	 ENTITY_AREA_TYPE_SHAPE:
		{
			a2DPoint	*point = (CArea::a2DPoint*)(&Point3d);

			float fZDistSq = 0.0f;
			float fXDistSq = FLT_MAX;

			for(unsigned int sIdx=0; sIdx<m_vpSegments.size(); sIdx++)
			{
				float fT;
				a2DSegment *curSg = m_vpSegments[sIdx];

				Vec3 startSeg(curSg->GetStart().x, curSg->GetStart().y, Point3d.z);
				Vec3 endSeg(curSg->GetEnd().x, curSg->GetEnd().y, Point3d.z);
				Lineseg line(startSeg, endSeg);

				/// Returns distance from a point to a line segment, ignoring the z coordinates
				float fThisXDistSq = Distance::Point_Lineseg2DSq(Point3d, line, fT);
				fXDistSq = min(fXDistSq, fThisXDistSq);
			}
			
			if (m_VSize)
			{
				// negative means from inside to hull
				float fDistToRoof = Point3d.z - (m_VOrigin+m_VSize);		
				float fDistToFloor = m_VOrigin - Point3d.z;

				float fZRoofSq = fDistToRoof*fDistToRoof;
				float fZFloorSq = fDistToFloor*fDistToFloor;

				bool bIsIn2DShape = IsPointWithin(Point3d, true);

				if (bIsIn2DShape)
				{
					// point is below, in, or above the area
					if ((Point3d.z < m_VOrigin+m_VSize && Point3d.z > m_VOrigin))
					{
						// Point is inside z-boundary
						if (!m_bObstructRoof && (fZRoofSq < fXDistSq) && (fZRoofSq < fZFloorSq))
						{
							// roof is closer than side
							fZDistSq = fZRoofSq;
							fXDistSq = 0.0f;
						}

						if (!m_bObstructFloor && (fZFloorSq < fXDistSq) && (fZFloorSq < fZRoofSq))
						{
							// floor is closer than side
							fZDistSq = fZFloorSq;
							fXDistSq = 0.0f;
						}
					}
					else
					{
						// point is above or below area
						if (abs(fDistToRoof) < abs(fDistToFloor))
						{
							// being above
							if (!m_bObstructRoof)
							{
								// perpendicular point to Roof
								fXDistSq = 0.0f;
								fZDistSq = fZRoofSq;
							}
						}
						else
						{
							// being below
							if (!m_bObstructFloor)
							{
								// perpendicular point to Floor
								fXDistSq = 0.0f;
								fZDistSq = fZFloorSq;
							}
						}				
					}
				}
				else
				{
					// outside of 2D Shape, so diagonal or only to the side
					if ((Point3d.z < m_VOrigin+m_VSize && Point3d.z > m_VOrigin))
					{
						// Point is inside z-boundary
					}
					else
					{
						// point is above or below area
						if (abs(fDistToRoof) < abs(fDistToFloor))
						{
							// being above
							fZDistSq = fZRoofSq;
						}
						else
						{
							// being below
							fZDistSq = fZFloorSq;
						}
					}
				}
			}
			else
			{
				// infinite high area
				// Closest is on an edge, ZDistance is 0, so nothing really to do here
			}

			fClosestDistance = fXDistSq;

			if (!bIgnoreHeight)
				fClosestDistance += fZDistSq;

			break;
		}

	case	 ENTITY_AREA_TYPE_SPHERE:
		{
			float fLength = 0;
			Vec3 vTemp(Point3d);

			if (bIgnoreHeight)
				vTemp.z = m_SphereCenter.z;

			fLength = m_SphereCenter.GetDistance(vTemp) - m_SphereRadius;

			return fLength*fLength;
			break;
		}
	case	 ENTITY_AREA_TYPE_BOX:
		{
			Vec3 p3d = m_InvMatrix.TransformPoint(Point3d);
			Vec3 OnHull3d;

			if (bIgnoreHeight)
				p3d.z = m_BoxMin.z;

			AABB myAABB(m_BoxMin, m_BoxMax);

			fClosestDistance = Distance::Point_AABBSq(p3d, myAABB, OnHull3d);

			if (m_bObstructRoof && OnHull3d.z == myAABB.max.z)
			{
				// Point is on the roof plane, but may be on the edge already
				Vec2 vTop(OnHull3d.x, myAABB.max.y);
				Vec2 vLeft(myAABB.min.x, OnHull3d.y);
				Vec2 vLow(OnHull3d.x, myAABB.min.y);
				Vec2 vRight(myAABB.max.x, OnHull3d.y);

				float fDistanceToTop		= p3d.GetSquaredDistance2D(vTop);
				float fDistanceToLeft		= p3d.GetSquaredDistance2D(vLeft);
				float fDistanceToLow		= p3d.GetSquaredDistance2D(vLow);
				float fDistanceToRight	= p3d.GetSquaredDistance2D(vRight);
				float fTempMinDistance = fDistanceToTop;

				OnHull3d = vTop;

				if (fDistanceToLeft < fTempMinDistance)
				{
					OnHull3d = vLeft;
					fTempMinDistance = fDistanceToLeft;
				}

				if (fDistanceToLow < fTempMinDistance)
				{
					OnHull3d = vLow;
					fTempMinDistance = fDistanceToLow;
				}

				if (fDistanceToRight < fTempMinDistance)
				{
					OnHull3d = vRight;
					fTempMinDistance = fDistanceToRight;
				}

				OnHull3d.z = min(myAABB.max.z, p3d.z);
				fClosestDistance = OnHull3d.GetSquaredDistance(p3d);

			}

			if (m_bObstructFloor && OnHull3d.z == myAABB.min.z)
			{
				// Point is on the floor plane, but may be on the edge already
				Vec2 vTop(OnHull3d.x, myAABB.max.y);
				Vec2 vLeft(myAABB.min.x, OnHull3d.y);
				Vec2 vLow(OnHull3d.x, myAABB.min.y);
				Vec2 vRight(myAABB.max.x, OnHull3d.y);

				float fDistanceToTop		= p3d.GetSquaredDistance2D(vTop);
				float fDistanceToLeft		= p3d.GetSquaredDistance2D(vLeft);
				float fDistanceToLow		= p3d.GetSquaredDistance2D(vLow);
				float fDistanceToRight	= p3d.GetSquaredDistance2D(vRight);
				float fTempMinDistance = fDistanceToTop;

				OnHull3d = vTop;

				if (fDistanceToLeft < fTempMinDistance)
				{
					OnHull3d = vLeft;
					fTempMinDistance = fDistanceToLeft;
				}

				if (fDistanceToLow < fTempMinDistance)
				{
					OnHull3d = vLow;
					fTempMinDistance = fDistanceToLow;
				}

				if (fDistanceToRight < fTempMinDistance)
				{
					OnHull3d = vRight;
					fTempMinDistance = fDistanceToRight;
				}

				OnHull3d.z = max(myAABB.min.z, p3d.z);
				fClosestDistance = OnHull3d.GetSquaredDistance(p3d);
			}

			// TODO transform point back to world
			break;
		}
	}

	return fClosestDistance;
}

//////////////////////////////////////////////////////////////////////////
void	CArea::SetPoints( const Vec3* vPoints, const int count )
{
	m_AreaType = ENTITY_AREA_TYPE_SHAPE;
	ClearPoints();
	// at least three points needed to create closed shape
	if (count>2)
	{
		m_bInitialized = true;
		float minZ = 10000000.0f;
		//////////////////////////////////////////////////////////////////////////
		for (int i = 0; i < count; i++)
		{
			if (vPoints[i].z < minZ)
				minZ = vPoints[i].z;
		}
		m_VOrigin = minZ;

		m_fShapeArea = 0.0f;
		for(int p=count-1,q=0; q<count; p=q++)
		{
			AddSegment( *((CArea::a2DPoint*)(vPoints+p)), *((CArea::a2DPoint*)(vPoints+q)) );
			m_fShapeArea += vPoints[q].x*vPoints[p].y - vPoints[p].x * vPoints[q].y;
		}
		m_fShapeArea /= 2.0f;
		CalcBBox( );
	}

	m_pAreaManager->SetAreasDirty();
}

//////////////////////////////////////////////////////////////////////////
void	CArea::SetBox( const Vec3& min,const Vec3& max,const Matrix34 &tm )
{
	m_AreaType = ENTITY_AREA_TYPE_BOX;
	m_bInitialized = true;
	m_BoxMin = min;
	m_BoxMax = max;
	m_InvMatrix = tm;
	m_InvMatrix.Invert();

/*
	uint32 nErrorCode=0;
	uint32 minValid = min.IsValid();
	if (minValid==0) nErrorCode|=0x8000;
	uint32 maxValid = max.IsValid();
	if (maxValid==0) nErrorCode|=0x8000;

	if (max.x < min.x) nErrorCode|=0x0001;
	if (max.y < min.y) nErrorCode|=0x0001;
	if (max.z < min.z) nErrorCode|=0x0001;
	if (min.x < -8000) nErrorCode|=0x0002;
	if (min.y < -8000) nErrorCode|=0x0004;
	if (min.z < -8000) nErrorCode|=0x0008;
	if (max.x > +8000) nErrorCode|=0x0010;
	if (max.y > +8000) nErrorCode|=0x0020;
	if (max.z > +8000) nErrorCode|=0x0040;
	assert(nErrorCode==0);

	if (nErrorCode)
	{
		CryFatalError("Fatal Error: BBox in EntitySystem is out of range");
		//AnimWarning("CryAnimation: Invalid BBox (%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f). ModenPath: '%s'  ErrorCode: %08x", 	m_AABB.min.x, m_AABB.min.y, m_AABB.min.z, m_AABB.max.x, m_AABB.max.y, m_AABB.max.z, 	m_pInstance->m_pModel->GetFilePathCStr(), nErrorCode);
		assert(0);
	}
	//if (rAnim.m_nEAnimID>=numAnimations)

*/

	m_pAreaManager->SetAreasDirty();
}

//////////////////////////////////////////////////////////////////////////
void CArea::SetBoxMatrix( const Matrix34 &tm )
{
	m_InvMatrix = tm;
	m_InvMatrix.Invert();

	m_pAreaManager->SetAreasDirty();
}

//////////////////////////////////////////////////////////////////////////
void CArea::GetBoxMatrix( Matrix34& tm )
{
	tm = m_InvMatrix;
	tm.Invert();
}

//////////////////////////////////////////////////////////////////////////
void	CArea::SetSphere( const Vec3& center,float fRadius )
{
	m_bInitialized = true;
	m_AreaType = ENTITY_AREA_TYPE_SPHERE;
	m_SphereCenter = center;
	m_SphereRadius = fRadius;
	m_SphereRadius2 = m_SphereRadius*m_SphereRadius;

	m_pAreaManager->SetAreasDirty();
}

//////////////////////////////////////////////////////////////////////////
void	CArea::CalcBBox( )
{
	m_AreaBBox.min.x = m_vpSegments[0]->bbox.min.x;
	m_AreaBBox.min.y = m_vpSegments[0]->bbox.min.y;
	m_AreaBBox.max.x = m_vpSegments[0]->bbox.max.x;
	m_AreaBBox.max.y = m_vpSegments[0]->bbox.max.y;
	for(unsigned int sIdx=1; sIdx<m_vpSegments.size(); sIdx++)
	{
		if( m_AreaBBox.min.x>m_vpSegments[sIdx]->bbox.min.x )
			m_AreaBBox.min.x = m_vpSegments[sIdx]->bbox.min.x;
		if( m_AreaBBox.min.y>m_vpSegments[sIdx]->bbox.min.y )
			m_AreaBBox.min.y = m_vpSegments[sIdx]->bbox.min.y;
		if( m_AreaBBox.max.x<m_vpSegments[sIdx]->bbox.max.x )
			m_AreaBBox.max.x = m_vpSegments[sIdx]->bbox.max.x;
		if( m_AreaBBox.max.y<m_vpSegments[sIdx]->bbox.max.y )
			m_AreaBBox.max.y = m_vpSegments[sIdx]->bbox.max.y;
	}
}

//////////////////////////////////////////////////////////////////////////
void	CArea::AddEntity( const EntityId entId )
{
	m_vEntityID.push_back( entId );
	m_bAttachedSoundTested = false;
	// invalidating effect radius
	m_fMaximumEffectRadius = -1.0f;

	m_pAreaManager->SetAreasDirty();
}

void	CArea::GetEntites( std::vector<EntityId> &entIDs )
{

	int nSize = m_vEntityID.size();
	for( unsigned int eIdx=0; eIdx<nSize; eIdx++ )
	{
		entIDs.push_back(m_vEntityID[eIdx]);
	}
}

float CArea::GetMaximumEffectRadius()
{
	if (m_fMaximumEffectRadius < 0.0f || gEnv->bEditor)
	{
		// recalculate
		m_fMaximumEffectRadius = 0.0f;
		int nSize = m_vEntityID.size();
		for( unsigned int eIdx=0; eIdx<nSize; eIdx++ )
		{
			IEntity *pEntity = GetEntitySystem()->GetEntity(m_vEntityID[eIdx]);
			if (pEntity)
			{
				IEntitySoundProxy *pSoundProxy = (IEntitySoundProxy*)pEntity->GetProxy(ENTITY_PROXY_SOUND);
				if (pSoundProxy)
				{
					m_fMaximumEffectRadius = max(m_fMaximumEffectRadius, pSoundProxy->GetEffectRadius());
				}
			}
		}
	}

		return m_fMaximumEffectRadius;

}
 	


//////////////////////////////////////////////////////////////////////////
bool	CArea::HasSoundAttached( )
{
	if (m_vEntityID.empty())
		return false;

	if (!m_bAttachedSoundTested)
	{
		for( unsigned int eIdx=0; eIdx<m_vEntityID.size(); eIdx++ )
		{
			IEntity* pAreaAttachedEntity = GetEntitySystem()->GetEntity(m_vEntityID[eIdx]);
			//		ASSERT(pEntity);
			if (pAreaAttachedEntity)
			{
				IEntityClass *pEntityClass = pAreaAttachedEntity->GetClass();
				string sClassName = pEntityClass->GetName();
				if (sClassName == "AmbientVolume" || sClassName == "SoundSpot" || sClassName == "ReverbVolume")
					m_bHasSoundAttached = true;
			}
		}
		m_bAttachedSoundTested = true;
	}

	return m_bHasSoundAttached;
}

//////////////////////////////////////////////////////////////////////////
void CArea::GetBBox(Vec2& vMin, Vec2& vMax)
{
	// Only valid for shape areas.
	vMin = Vec2(m_AreaBBox.min.x, m_AreaBBox.min.y);
	vMax = Vec2(m_AreaBBox.max.x, m_AreaBBox.max.y);
}

//////////////////////////////////////////////////////////////////////////
void	CArea::AddEntites( const std::vector<EntityId> &entIDs )
{
	for(unsigned int i=0; i<entIDs.size(); i++)
		AddEntity(entIDs[i]);

	// invalidating effect radius
	m_fMaximumEffectRadius = -1.0f;
}

//////////////////////////////////////////////////////////////////////////
void	CArea::ClearEntities( )
{
	m_vEntityID.clear();
	m_PrevFade = -1.0f;
	m_bHasSoundAttached = false;
	
	// invalidating effect radius
	m_fMaximumEffectRadius = -1.0f;
}

//////////////////////////////////////////////////////////////////////////
void CArea::SetCachedEvent (const SEntityEvent* pNewEvent)
{
	if (pNewEvent)
	{
		m_CachedEvent = *pNewEvent;
		//m_CachedEvent.fParam = pNewEvent->fParam;
		//m_CachedEvent.nParam = pNewEvent->nParam;
	}
	else
		m_CachedEvent.event = ENTITY_EVENT_LAST;
}

//////////////////////////////////////////////////////////////////////////
void CArea::SendCachedEvent	()
{
	if (!m_bInitialized || m_CachedEvent.event == ENTITY_EVENT_LAST)
		return;

	IEntity* pAreaAttachedEntity = NULL;

	for( unsigned int eIdx=0; eIdx<m_vEntityID.size(); eIdx++ )
	{
		pAreaAttachedEntity = GetEntitySystem()->GetEntity(m_vEntityID[eIdx]);
		//		ASSERT(pEntity);
		if (pAreaAttachedEntity)
		{
			m_CachedEvent.nParam[1] = m_AreaID;
			m_CachedEvent.nParam[2] = m_EntityID;
			m_CachedEvent.fParam[0] = m_PrevFade;
			pAreaAttachedEntity->SendEvent( m_CachedEvent );
		}
	}
	m_CachedEvent.event = ENTITY_EVENT_LAST;
}


// do enter area - player was outside, now is inside
// calls entity OnEnterArea which calls script OnEnterArea( player, AreaID )	
//////////////////////////////////////////////////////////////////////////
void	CArea::EnterArea( IEntity* pEntity )
{
	if (!m_bInitialized)
		return;

	IEntity* pAreaAttachedEntity;

	m_PrevFade = -1;
	m_bIsActive = true;

	for( unsigned int eIdx=0; eIdx<m_vEntityID.size(); eIdx++ )
	{
		pAreaAttachedEntity = GetEntitySystem()->GetEntity(m_vEntityID[eIdx]);
		//		ASSERT(pEntity);
		if (pAreaAttachedEntity)
		{
			SEntityEvent event;
			event.event = ENTITY_EVENT_ENTERAREA;
			event.nParam[0] = pEntity->GetId();
			event.nParam[1] = m_AreaID;
			event.nParam[2] = m_EntityID;
			pAreaAttachedEntity->SendEvent( event );

			m_CachedEvent.event = ENTITY_EVENT_LAST;
		}
	}
}


// do leave area - player was inside, now is outside
// calls entity OnLeaveArea wich calls script OnLeaveArea( player, AreaID )	
//////////////////////////////////////////////////////////////////////////
void	CArea::LeaveArea( IEntity *pSrcEntity )
{
	if (!m_bInitialized)
		return;

	IEntity* pEntity;

	for( unsigned int eIdx=0; eIdx<m_vEntityID.size(); eIdx++ )
	{
		pEntity = GetEntitySystem()->GetEntity(m_vEntityID[eIdx]);
		//		ASSERT(pEntity);
		if (pEntity)
		{
			SEntityEvent event;
			event.event = ENTITY_EVENT_LEAVEAREA;
			event.nParam[0] = pSrcEntity->GetId();
			event.nParam[1] = m_AreaID;
			event.nParam[2] = m_EntityID;
			pEntity->SendEvent( event );

			m_CachedEvent.event = ENTITY_EVENT_LAST;
		}
	}
	m_PrevFade = -1;
	m_bIsActive = false;
}

// do enter near area - entity was "far", now is "near"
// calls entity OnEnterNearArea which calls script OnEnterNearArea( entity(player), AreaID )	
//////////////////////////////////////////////////////////////////////////
void	CArea::EnterNearArea( IEntity *pSrcEntity, float fDistanceSq )
{
	if (!m_bInitialized)
		return;

	IEntity* pEntity;

	for( unsigned int eIdx=0; eIdx<m_vEntityID.size(); eIdx++ )
	{
		pEntity = GetEntitySystem()->GetEntity(m_vEntityID[eIdx]);
		//		ASSERT(pEntity);
		if (pEntity)
		{
			SEntityEvent event;
			event.event = ENTITY_EVENT_ENTERNEARAREA;
			event.nParam[0] = pSrcEntity->GetId();
			event.nParam[1] = m_AreaID;
			event.nParam[2] = m_EntityID;
			pEntity->SendEvent( event );

			m_CachedEvent.event = ENTITY_EVENT_LAST;
		}
	}
	//m_PrevFade = -1;
	//m_bIsActive = true;
}

// do leave near area - entity was "near", now is "far"
// calls entity OnLeaveNearArea which calls script OnLeaveNearArea( entity(player), AreaID )	
//////////////////////////////////////////////////////////////////////////
void	CArea::LeaveNearArea( IEntity *pSrcEntity )
{
	if (!m_bInitialized)
		return;

	IEntity* pEntity;

	for( unsigned int eIdx=0; eIdx<m_vEntityID.size(); eIdx++ )
	{
		pEntity = GetEntitySystem()->GetEntity(m_vEntityID[eIdx]);
		//		ASSERT(pEntity);
		if (pEntity)
		{
			SEntityEvent event;
			event.event = ENTITY_EVENT_LEAVENEARAREA;
			event.nParam[0] = pSrcEntity->GetId();
			event.nParam[1] = m_AreaID;
			event.nParam[2] = m_EntityID;
			pEntity->SendEvent( event );

			m_CachedEvent.event = ENTITY_EVENT_LAST;
		}
	}
	//m_PrevFade = -1;
	//m_bIsActive = true;
}



//calculate distance to area - proceed fade. player is inside of the area
//////////////////////////////////////////////////////////////////////////
void	CArea::UpdateArea( const Vec3 &vPos,IEntity* pEntity )
{
	if (!m_bInitialized)
		return;

	Vec3 pos3D = vPos;
	ProceedFade( pEntity, CalculateFade( pos3D ) );
}


//calculate distance to area - proceed fade. player is inside of the area
//////////////////////////////////////////////////////////////////////////
void	CArea::UpdateAreaInside( IEntity* pSrcEntity )
{
	if (!m_bInitialized)
		return;

	IEntity* pEntity;

	for( unsigned int eIdx=0; eIdx<m_vEntityID.size(); eIdx++ )
	{
		pEntity = GetEntitySystem()->GetEntity(m_vEntityID[eIdx]);
		//		ASSERT(pEntity);
		if (pEntity)
		{
			SEntityEvent event;
			event.event = ENTITY_EVENT_MOVEINSIDEAREA;
			event.nParam[0] = pSrcEntity->GetId();
			event.nParam[1] = m_AreaID;
			event.nParam[2] = m_EntityID;			
			event.fParam[0] = m_PrevFade;
			pEntity->SendEvent( event );
		}
	}
}

// pEntity moves in an area that has a higher priority than this area here
//////////////////////////////////////////////////////////////////////////
void	CArea::ExclusiveUpdateAreaInside(  IEntity* pSrcEntity, EntityId AreaHighEntityID)
{
	if (!m_bInitialized)
		return;

	IEntity* pEntity;

	for( unsigned int eIdx=0; eIdx<m_vEntityID.size(); eIdx++ )
	{
		pEntity = GetEntitySystem()->GetEntity(m_vEntityID[eIdx]);
		//		ASSERT(pEntity);
		if (pEntity)
		{
			SEntityEvent event;
			event.event = ENTITY_EVENT_MOVEINSIDEAREA;
			event.nParam[0] = pSrcEntity->GetId();
			event.nParam[1] = m_AreaID;
			event.nParam[2] = m_EntityID;	// AreaLowEntityID
			event.nParam[3] = AreaHighEntityID;			
			event.fParam[0] = m_PrevFade;
			pEntity->SendEvent( event );
		}
	}
}

// Entity is "near" the area
//////////////////////////////////////////////////////////////////////////
void	CArea::UpdateAreaNear( IEntity* pSrcEntity, float fDistanceSq )
{
	if (!m_bInitialized)
		return;

	IEntity* pEntity;

	for( unsigned int eIdx=0; eIdx<m_vEntityID.size(); eIdx++ )
	{
		pEntity = GetEntitySystem()->GetEntity(m_vEntityID[eIdx]);
		//		ASSERT(pEntity);
		if (pEntity)
		{
			SEntityEvent event;
			event.event = ENTITY_EVENT_MOVENEARAREA;
			event.nParam[0] = pSrcEntity->GetId();
			event.nParam[1] = m_AreaID;
			event.nParam[2] = m_EntityID;
			event.fParam[0] = m_PrevFade;
			event.fParam[1] = fDistanceSq;
			pEntity->SendEvent( event );
		}
	}
}

//calculate distance to area. player is inside of the area 
//////////////////////////////////////////////////////////////////////////
float	CArea::CalculateFade( const Vec3& pos3D )
{
	if (!m_bInitialized)
		return 0;

	//	a2DPoint pos = *((CArea::a2DPoint*)(&pos3D));
	a2DPoint pos = CArea::a2DPoint(pos3D);
	float fadeCoeff = 0.0f;

	switch(m_AreaType)
	{
	case	 ENTITY_AREA_TYPE_SHAPE:
		fadeCoeff = CalcDistToPoint( pos );		
		break;
	case	 ENTITY_AREA_TYPE_SPHERE:
		{
			if (m_fProximity <= 0.0f)
			{
				fadeCoeff = 1.0f;
				break;
			}
			Vec3 Delta = pos3D - m_SphereCenter;
			fadeCoeff = (m_SphereRadius - Delta.GetLength()) / m_fProximity;
			if (fadeCoeff > 1.0f)
				fadeCoeff = 1.0f;
			break;
		}
	case	 ENTITY_AREA_TYPE_BOX:
		{
			if (m_fProximity <= 0.0f)
			{
				fadeCoeff = 1.0f;
				break;
			}
			Vec3 p3D = m_InvMatrix.TransformPoint(pos3D);
			Vec3 MinDelta = p3D-m_BoxMin;
			Vec3 MaxDelta = m_BoxMax-p3D;
			Vec3 EdgeDist = (m_BoxMax - m_BoxMin) / 2.0f;
			if ((!EdgeDist.x) || (!EdgeDist.y) || (!EdgeDist.z))
			{
				fadeCoeff = 1.0f;
				break;
			}

			float fFadeScale = m_fProximity / 100.0f;
			EdgeDist *= fFadeScale;

			//float fFade=MinDelta.x/EdgeDist.x;
			float fMinFade = MinDelta.x / EdgeDist.x;					

			for (int k=0; k<3; k++)
			{
				float fFade1 = MinDelta[k] / EdgeDist[k];
				float fFade2 = MaxDelta[k] / EdgeDist[k];
				fMinFade = min(fMinFade, min(fFade1, fFade2));
			} //k

			/*
			fFade=MinDelta.y/EdgeDist.y;
			if (fFade<fMinFade)
			fMinFade=fFade;

			fFade=MinDelta.z/EdgeDist.z;
			if (fFade<fMinFade)
			fMinFade=fFade;

			fFade=MaxDelta.x/EdgeDist.x;
			if (fFade<fMinFade)
			fMinFade=fFade;

			fFade=MaxDelta.y/EdgeDist.y;
			if (fFade<fMinFade)
			fMinFade=fFade;

			fFade=MaxDelta.z/EdgeDist.z;
			if (fFade<fMinFade)
			fMinFade=fFade;
			*/

			fadeCoeff = fMinFade;
			if (fadeCoeff > 1.0f)
				fadeCoeff = 1.0f;
			break;
		}
	}

	return fadeCoeff;
}

//proceed fade. player is inside of the area
//////////////////////////////////////////////////////////////////////////
void	CArea::ProceedFade( IEntity *pSrcEntity, const float fadeCoeff )
{
	if (!m_bInitialized)
		return;

	// no update if fade coefficient hasn't changed
	if(m_PrevFade == fadeCoeff)
		return;
	m_PrevFade = fadeCoeff;

	IEntity* pEntity;
	for( unsigned int eIdx=0; eIdx<m_vEntityID.size(); eIdx++ )
	{
		pEntity = GetEntitySystem()->GetEntity(m_vEntityID[eIdx]);
		//		ASSERT(pEntity);
		if (pEntity)
		{
			SEntityEvent event;
			event.event = ENTITY_EVENT_MOVEINSIDEAREA;
			event.nParam[0] = pSrcEntity->GetId();
			event.nParam[1] = m_EntityID;
			event.fParam[0] = fadeCoeff;
			pEntity->SendEvent( event );
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void	CArea::Draw( const int idx) 
{
	I3DEngine *p3DEngine = gEnv->p3DEngine;
	IRenderAuxGeom *pRC = gEnv->pRenderer->GetIRenderAuxGeom();
	pRC->SetRenderFlags( e_Def3DPublicRenderflags );

	ColorB	colorsArray[] = {	ColorB(255,0,0,255),
		ColorB(0, 255, 0, 255),
		ColorB(0, 0, 255, 255),
		ColorB(255, 255, 0, 255),
		ColorB(255, 0, 255, 255),
		ColorB(0, 255, 255, 255),
		ColorB(255, 255, 255, 255),
	};
	ColorB	color = colorsArray[idx%(sizeof(colorsArray)/sizeof(ColorB))];
	ColorB	color1 = colorsArray[(idx+1)%(sizeof(colorsArray)/sizeof(ColorB))];
	ColorB	color2 = colorsArray[(idx+2)%(sizeof(colorsArray)/sizeof(ColorB))];

	switch(m_AreaType)
	{
	case	 ENTITY_AREA_TYPE_SHAPE:
		{		
			Vec3	v0, v1, vCenter=Vec3(0,0,0);
			float	deltaZ = 0.1f;
			int nSize = m_vpSegments.size();
			for(unsigned int sIdx=0; sIdx<nSize; sIdx++)
			{
				if( m_vpSegments[sIdx]->k < 0 )
				{
					v0.x = m_vpSegments[sIdx]->bbox.min.x;
					v0.y = m_vpSegments[sIdx]->bbox.max.y;

					v1.x = m_vpSegments[sIdx]->bbox.max.x;
					v1.y = m_vpSegments[sIdx]->bbox.min.y;
				}
				else
				{
					v0.x = m_vpSegments[sIdx]->bbox.min.x;
					v0.y = m_vpSegments[sIdx]->bbox.min.y;

					v1.x = m_vpSegments[sIdx]->bbox.max.x;
					v1.y = m_vpSegments[sIdx]->bbox.max.y;
				}

				v0.z = max(m_VOrigin, p3DEngine->GetTerrainElevation(v0.x, v0.y) + deltaZ);
				v1.z = max(m_VOrigin, p3DEngine->GetTerrainElevation(v1.x, v1.y) + deltaZ);
				vCenter += v0+v1;

				// draw lower line segments
				pRC->DrawLine( v0, color, v1, color);

				// Draw upper line segments and vertical edges
				if( m_VSize > 0.0f )
				{
					Vec3 v0Z = Vec3(v0.x, v0.y, m_VOrigin + m_VSize);
					Vec3 v1Z = Vec3(v1.x, v1.y, m_VOrigin + m_VSize);

					pRC->DrawLine( v0, color, v0Z, color );
					//pRC->DrawLine( v1, color, v1Z, color );
					pRC->DrawLine( v0Z, color, v1Z, color );

				}
			}

			vCenter /= nSize*2;
			DrawDebugInfo(vCenter);
			break;
		}
	case	 ENTITY_AREA_TYPE_SPHERE:
		{
			ColorB  color3 = color;
			color3.a = 64;

			pRC->SetRenderFlags( e_Def3DPublicRenderflags|e_AlphaBlended );
			pRC->DrawSphere(m_SphereCenter, m_SphereRadius, color3);

			DrawDebugInfo(m_SphereCenter);
			break;
		}
	case	 ENTITY_AREA_TYPE_BOX:
		{
			float fLength = m_BoxMax.x - m_BoxMin.x;
			float fWidth	= m_BoxMax.y - m_BoxMin.y;
			float fHeight = m_BoxMax.z - m_BoxMin.z;

			Vec3 v0 = m_BoxMin;
			Vec3 v1 = Vec3(m_BoxMin.x+fLength,	m_BoxMin.y,					m_BoxMin.z);
			Vec3 v2 = Vec3(m_BoxMin.x+fLength,	m_BoxMin.y+fWidth,	m_BoxMin.z);
			Vec3 v3 = Vec3(m_BoxMin.x,					m_BoxMin.y+fWidth,	m_BoxMin.z);
			Vec3 v4 = Vec3(m_BoxMin.x,					m_BoxMin.y,					m_BoxMin.z+fHeight);
			Vec3 v5 = Vec3(m_BoxMin.x+fLength,	m_BoxMin.y,					m_BoxMin.z+fHeight);
			Vec3 v6 = Vec3(m_BoxMin.x+fLength,	m_BoxMin.y+fWidth,	m_BoxMin.z+fHeight);
			Vec3 v7 = Vec3(m_BoxMin.x,					m_BoxMin.y+fWidth,	m_BoxMin.z+fHeight);

			v0 = m_InvMatrix.GetInvertedFast().TransformPoint(v0);
			v1 = m_InvMatrix.GetInvertedFast().TransformPoint(v1);
			v2 = m_InvMatrix.GetInvertedFast().TransformPoint(v2);
			v3 = m_InvMatrix.GetInvertedFast().TransformPoint(v3);
			v4 = m_InvMatrix.GetInvertedFast().TransformPoint(v4);
			v5 = m_InvMatrix.GetInvertedFast().TransformPoint(v5);
			v6 = m_InvMatrix.GetInvertedFast().TransformPoint(v6);
			v7 = m_InvMatrix.GetInvertedFast().TransformPoint(v7);

			// draw lower half of box
			pRC->DrawLine( v0, color1, v1 , color1);
			pRC->DrawLine( v1, color1, v2 , color1);
			pRC->DrawLine( v2, color1, v3 , color1);
			pRC->DrawLine( v3, color1, v0 , color1);

			// draw upper half of box
			pRC->DrawLine( v4, color1, v5 , color1);
			pRC->DrawLine( v5, color1, v6 , color1);
			pRC->DrawLine( v6, color1, v7 , color1);
			pRC->DrawLine( v7, color1, v4 , color1);

			// draw vertical edges
			pRC->DrawLine( v0, color1, v4 , color1);
			pRC->DrawLine( v1, color1, v5 , color1);
			pRC->DrawLine( v2, color1, v6 , color1);
			pRC->DrawLine( v3, color1, v7 , color1);

			DrawDebugInfo((v0+v1+v2+v3)/4.0f);
			break;
		}
	default:
		break;
	}

}

//////////////////////////////////////////////////////////////////////////
void CArea::DrawDebugInfo(const Vec3& center)
{
	if (gEnv->pConsole->GetCVar("es_DrawAreas")->GetIVal()==2)
	{
		float area = fabs(GetAreaSize());
		char info[256];
		sprintf(info,"Area: %.2f unit^2",area);
		float col[4] = {1,1,1,1};
		gEnv->pRenderer->DrawLabelEx( center, 1.3f, col,true,true, info);
	}
}

//////////////////////////////////////////////////////////////////////////
float	CArea::GetAreaSize()
{
	switch(GetAreaType())
	{
	case ENTITY_AREA_TYPE_SHAPE:
		return m_fShapeArea;

	case ENTITY_AREA_TYPE_BOX:
		{
			float fLength = m_BoxMax.x - m_BoxMin.x;
			float fWidth	= m_BoxMax.y - m_BoxMin.y;
			return fLength * fWidth;
		}

	case ENTITY_AREA_TYPE_SPHERE:
		return m_SphereRadius*m_SphereRadius*3.1415f;

	default:
		return -1;
	}
}
