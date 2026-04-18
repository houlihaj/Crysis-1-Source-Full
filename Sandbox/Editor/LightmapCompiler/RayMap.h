/*=============================================================================
RayMap.h : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef RAYMAP_H
#define RAYMAP_H

#if _MSC_VER > 1000
# pragma once
#endif

#include "RayMap_Segment.h"
#include "RayMap_DimensionArray.h"
#include "IlluminanceIntegrator.h"


class CRayMap : public CIlluminanceIntegrator
{
public:
	//===================================================================================
	// Constructor
	//===================================================================================
	CRayMap()
	{
		m_BBox.Reset();
	}

	//===================================================================================
	// Destructor
	//===================================================================================
	~CRayMap()
	{
	}

	void		AddSegment( const Vec3 vStartPosition, const Vec3 vEndPosition, const int32 nStartType, const int32 nEndType );
	void		Search( std::vector<int32>& vRayList, const Vec3 vPosition, const float fRadius )
	{
		vRayList.clear();
		m_vDimensions[0].Search( m_vRaySegmentList, vRayList, vPosition, fRadius );
		m_vDimensions[1].Search( m_vRaySegmentList, vRayList, vPosition, fRadius );
		m_vDimensions[2].Search( m_vRaySegmentList, vRayList, vPosition, fRadius );

		//todo: segment - position distance check...
		//sort segments by distance
	} /// Search the rays around a point

	void		SearchWithLN( std::vector<int32>& vRayList, const Vec3 vPosition, const float fRadius, const Vec3 vNormal )
	{
		vRayList.clear();
		m_vDimensions[0].SearchWithLN( m_vRaySegmentList, vRayList, vPosition, fRadius, vNormal );
		m_vDimensions[1].SearchWithLN( m_vRaySegmentList, vRayList, vPosition, fRadius, vNormal );
		m_vDimensions[2].SearchWithLN( m_vRaySegmentList, vRayList, vPosition, fRadius, vNormal );
	}		/// Search the rays around a point with normal direction

	bool		GenerateIndex()
	{
		if( m_vDimensions[0].GenerateFromList( m_vRaySegmentList, m_BBox, 0 ) )
			if( m_vDimensions[1].GenerateFromList( m_vRaySegmentList, m_BBox, 1 ) )
				if( m_vDimensions[2].GenerateFromList( m_vRaySegmentList, m_BBox, 2 ) )
							return true;
		return false;
	}		/// Generate index list per dimensions for raysegments


	//Illumimance Intagrator's functions
	bool Illuminance(CIllumHandler& illumHandler, /*string category,*/ const Vec3& vPosition, const t_pairEntityId &LightIDs );
	bool Illuminance(CIllumHandler& illumHandler, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId &LightIDs );

public:
	std::vector<CRayMap_Segment> m_vRaySegmentList;			/// the original list of segments

private:
	AABB												 m_BBox;								/// AABbox of the raymap
	CRayMap_DimensionArray			 m_vDimensions[3];			/// 3 dimension array list of the ray segments 
};

#endif//RAYMAP_H