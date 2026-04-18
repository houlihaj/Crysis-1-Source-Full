/*=============================================================================
IntersectionCacheCluster.cpp :
Copyright 2006 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/

#include "stdafx.h"
#include "IntersectionCacheCluster.h"

/////////////////////////////////////////////////////////////////////////////////////////
CIntersectionCacheCluster::~CIntersectionCacheCluster()
{
	SAFE_DELETE_ARRAY( m_pCluster );
}

/////////////////////////////////////////////////////////////////////////////////////////
void CIntersectionCacheCluster::Init( const int32 nSideSize )
{
	m_nSideSize = nSideSize;
	m_nFaceSize = nSideSize*nSideSize;
	m_fHalfSideSize = nSideSize*0.5f;


	SAFE_DELETE_ARRAY( m_pCluster );
	int32 nSize  = m_nFaceSize*6;
	m_pCluster = new COctreeCell*[nSize];
	memset( m_pCluster, 0, sizeof(COctreeCell*)*nSize);
}

/////////////////////////////////////////////////////////////////////////////////////////
int32 CIntersectionCacheCluster::GetIndex( const Vec3& vDirection ) const
{
	static int32 nProjIndex[6] =
	{
		1,2,
		2,0,
		0,1
	};
	int32 nID = 0;

	//search the biggest axis
	float fX = fabs(vDirection.x);
	float fY = fabs(vDirection.y);
	float fZ = fabs(vDirection.z);
	nID = fY > fX ? 1 : nID;
	nID = fZ > fY ? 2 : nID;
	bool bNegative = vDirection[nID] < 0.f;
	float fMax = 1.f / vDirection[nID];

	nID <<= 1;
	//project based on the biggest axis
	fX = vDirection.x * fMax;
	fY = vDirection.y * fMax;
	fZ = vDirection.z * fMax;


	//get the 2D projection to that axis
	float fSideX = vDirection[ nProjIndex[nID] ];
	float fSideY = vDirection[ nProjIndex[nID+1] ];

	//decide the face
	nID += bNegative ? 0 : 1;

	//calculate the offsets
	int32 nOffsetX = (int)floor( fSideX*m_fHalfSideSize+m_fHalfSideSize );
	int32 nOffsetY = (int)floor( fSideY*m_fHalfSideSize+m_fHalfSideSize );

	//prevent overflow
	nOffsetX -= (nOffsetX == m_nSideSize) ? 1 : 0;
	nOffsetY -= (nOffsetY == m_nSideSize) ? 1 : 0;

	//calculate
	return nID*m_nFaceSize+nOffsetY*m_nSideSize+nOffsetX;
}

/////////////////////////////////////////////////////////////////////////////////////////
void CIntersectionCacheCluster::SetCell( const int32 nIndex, COctreeCell* pCell )
{
	m_pCluster[ nIndex ] = pCell;
}
