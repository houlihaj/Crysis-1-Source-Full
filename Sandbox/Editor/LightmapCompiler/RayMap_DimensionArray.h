/*=============================================================================
RayMap_DimensionArray.h : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef RAYMAP_DIMENSIONARRAY_H
#define RAYMAP_DIMENSIONARRAY_H

#if _MSC_VER > 1000
# pragma once
#endif

#include "RayMap_Segment.h"

class CRayMap_DimensionArray
{
public:
	//===================================================================================
	// Constructor
	//===================================================================================
	CRayMap_DimensionArray():m_pDimensionIndex(NULL),m_pList(NULL),m_nXDimension(0),
		m_nYDimension(0),m_fCellWidth(0),m_fCellHeight(0), m_fStartX(0), m_fStartY(0),m_fEndX(0),m_fEndY(0),
		m_fOnePerCellWidth(0),m_fOnePerCellHeight(0)
	{
	}

	//===================================================================================
	// Destructor
	//===================================================================================
	~CRayMap_DimensionArray()
	{
		SAFE_DELETE_ARRAY( m_pDimensionIndex );
		SAFE_DELETE_ARRAY( m_pList );
	}

	bool		GenerateFromList( std::vector<CRayMap_Segment>& vRaySegmentList, const AABB& BBox, const int nDimensionID );
	void		Search( std::vector<CRayMap_Segment>& vRaySegmentList, std::vector<int32>& vRayList, const Vec3 vPosition, const float fRadius )  const;
	void		SearchWithLN( std::vector<CRayMap_Segment>& vRaySegmentList, std::vector<int32>& vRayList, const Vec3 vPosition, const float fRadius, const Vec3 vNormal )  const;

	void		ConvertToDAUnits( const Vec3 vVector, float& fNewX, float &fNewY)  const
	{
		float fX = vVector[m_nDimensionID];
		float fY = vVector[m_nNextDimID];

		fNewX = fX < m_fStartX ? m_fStartX : ( fX > m_fEndX ? m_fEndX : fX);
		fNewY = fY < m_fStartY ? m_fStartY : ( fY > m_fEndY ? m_fEndY : fY);
		fNewX = (fNewX-m_fStartX) * m_fOnePerCellWidth;
		fNewY = (fNewY-m_fStartY) * m_fOnePerCellHeight;
	}
private:
	int32		m_nXDimension,m_nYDimension;						/// the dimension of this table
	float		m_fStartX,m_fStartY;										/// X,Y starting position
	float		m_fEndX,m_fEndY;										/// X,Y end position
	float		m_fCellWidth,m_fCellHeight;							/// X,Y cell size
	float		m_fOnePerCellWidth,m_fOnePerCellHeight;	/// helps convert coordiantes from world to dimensionarray units
	int32*	m_pDimensionIndex;											/// The index list (start-number of element) of the pointer list
	int32*	m_pList;																/// The direct list of element ids
	int32		m_nDimensionID;													/// X dimension ID
	int32		m_nNextDimID;														/// Y dimension ID
};


#endif//RAYMAP_DIMENSIONARRAY_H