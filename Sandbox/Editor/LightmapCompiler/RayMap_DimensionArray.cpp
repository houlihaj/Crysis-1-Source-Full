/*=============================================================================
RayMap_DimensionArray.cpp: 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#include "stdafx.h"
#include "RayMap_DimensionArray.h"
#include "SimpleTriangleRasterizer.h"

class CRayMap_FirstPass: public CSimpleTriangleRasterizer::IRasterizeSink
{
public:

	//! constructor
	CRayMap_FirstPass( int32 *pCounter, DWORD indwWidth, DWORD indwHeight )
	{
		assert(pCounter);
		m_pCounter = pCounter;
		m_dwWidth=indwWidth;
		m_dwHeight=indwHeight;
	}

	//!
	virtual void Line( const float infXLeft, const float infXRight,
		const int iniLeft, const int iniRight, const int iniY )
	{
		assert(iniLeft>=0);
		assert(iniY>=0);
		assert(iniRight<=(int)m_dwWidth);
		assert(iniY<(int)m_dwHeight);

		int32 *pMem = &m_pCounter[2*(iniY*m_dwWidth+iniLeft)+1];
		for(int x=iniLeft;x<iniRight;x++)
		{
			*pMem = *pMem+1;	//add a new element
			pMem+=2;					//next one...
		}
	}

	int32*			m_pCounter;				/// the cell number counter
	DWORD			m_dwWidth;				//!< x= [0,m_dwWidth[
	DWORD			m_dwHeight;				//!< y= [0,m_dwHeight[
};

class CRayMap_SecondPass: public CSimpleTriangleRasterizer::IRasterizeSink
{
public:

	//! constructor
	CRayMap_SecondPass( int32* pList,int32 *pCounter, DWORD indwWidth, DWORD indwHeight ) : m_nActualElement(0)
	{
		assert(pCounter);
		assert(pList);
		m_pList = pList;
		m_pCounter = pCounter;
		m_dwWidth=indwWidth;
		m_dwHeight=indwHeight;
	}

	void SetActualElement(int32 pElement) { m_nActualElement = pElement; }

	//!
	virtual void Line( const float infXLeft, const float infXRight,
		const int iniLeft, const int iniRight, const int iniY )
	{
		assert(iniLeft>=0);
		assert(iniY>=0);
		assert(iniRight<=(int)m_dwWidth);
		assert(iniY<(int)m_dwHeight);

		int32 *pMem = &m_pCounter[2*(iniY*m_dwWidth+iniLeft)+1];
		for(int x=iniLeft;x<iniRight;x++)
		{
			//inlcude the pointer
			m_pList[ m_pCounter[2*(iniY*m_dwWidth+x)+0] + m_pCounter[2*(iniY*m_dwWidth+x)+1] ] = m_nActualElement;
			//incrase the number of elements
			*pMem = *pMem+1;	//add a new element
			pMem+=2;					//next one...
		}
	}

	int32			m_nActualElement;	///the actual element
	int32*		m_pList;					/// List of the element ids
	int32*		m_pCounter;				/// the cell number counter
	DWORD			m_dwWidth;				//!< x= [0,m_dwWidth[
	DWORD			m_dwHeight;				//!< y= [0,m_dwHeight[
};


bool CRayMap_DimensionArray::GenerateFromList( std::vector<CRayMap_Segment>& vRaySegmentList, const AABB& BBox, const int nDimensionID )
{
	m_nDimensionID = nDimensionID;
	m_nNextDimID = (m_nDimensionID + 1) % 3;
	m_fStartX = BBox.min[m_nDimensionID];
	m_fStartY = BBox.min[m_nNextDimID];

	float fEndX = BBox.max[m_nDimensionID];
	float fEndY = BBox.max[m_nNextDimID];
	float fXSize = fEndX - m_fStartX;
	float fYSize = fEndY - m_fStartY;

	//determine the cell numbers
#define RMD_MAXSIZE	0.5f
	m_fCellWidth = RMD_MAXSIZE;
	m_fCellHeight = RMD_MAXSIZE;
	m_fOnePerCellWidth = 1.f / m_fCellWidth;
	m_fOnePerCellHeight = 1.f / m_fCellHeight;

	m_nXDimension = (int)floor(fXSize / RMD_MAXSIZE);
	if( 0 != fXSize-m_nXDimension * RMD_MAXSIZE )
		++m_nXDimension;
	m_nYDimension = fYSize / RMD_MAXSIZE;
	if( 0 != fYSize-m_nYDimension * RMD_MAXSIZE )
		++m_nYDimension;

	SAFE_DELETE_ARRAY( m_pDimensionIndex );

	m_pDimensionIndex = new int32[2*m_nXDimension*m_nYDimension];
	if( NULL == m_pDimensionIndex )
	{
		m_nYDimension = m_nXDimension = 0;
		return false;
	}

	memset( m_pDimensionIndex,0,sizeof(int32)*2*m_nXDimension*m_nYDimension);


	//generate intersection list - 2 pass to prevent the memory fragmentation:
	//1.pass: check how big memory needed.
	CRayMap_FirstPass FirstPassSink( m_pDimensionIndex, m_nXDimension,m_nYDimension);
	CSimpleTriangleRasterizer Rasterizer(m_nXDimension,m_nYDimension);

	std::vector<CRayMap_Segment>::iterator itEnd = vRaySegmentList.end();
	std::vector<CRayMap_Segment>::iterator it;
	float fX[3],fY[3];
	for( it = vRaySegmentList.begin(); it != itEnd; ++it )
	{
		float fSegmentStartX,fSegmentStartY;
		float fSegmentEndX,fSegmentEndY;
		ConvertToDAUnits(it->m_vStartPos, fSegmentStartX,fSegmentStartY );
		ConvertToDAUnits(it->m_vEndPos, fSegmentEndX,fSegmentEndY );

		fX[0] = fX[1] = fSegmentStartX;
		fX[2] = fSegmentEndX;
		fY[0] = fY[1] = fSegmentStartY;
		fY[2] = fSegmentEndY;
		Rasterizer.CallbackFillConservative(fX,fY,&FirstPassSink);
	}

	//2.pass: initialize datas
	int32 nListSize = 0;
	int32 nSize = m_nXDimension*m_nYDimension;
	for( int32 i = 0; i <nSize;++i)
	{
		m_pDimensionIndex[i*2] = nListSize;
		nListSize+= m_pDimensionIndex[i*2+1];
		m_pDimensionIndex[i*2+1] = 0;		//clear again
	}
	SAFE_DELETE_ARRAY( m_pList );
	m_pList = new int32[nListSize];
	if( NULL == m_pList )
	{
		m_nYDimension = m_nXDimension = 0;
		return false;
	}
	memset( m_pList,0,sizeof(int32)*nListSize );

	//insert the segments
	CRayMap_SecondPass SecondPassSink( m_pList, m_pDimensionIndex, m_nXDimension,m_nYDimension);
	int nID = 0;
	for( it = vRaySegmentList.begin(); it != itEnd; ++it, ++nID )
	{
		//project to the 2 dimension we check & calculate how much cell intersect
		float fSegmentStartX,fSegmentStartY;
		float fSegmentEndX,fSegmentEndY;
		ConvertToDAUnits(it->m_vStartPos, fSegmentStartX,fSegmentStartY );
		ConvertToDAUnits(it->m_vEndPos, fSegmentEndX,fSegmentEndY );
		//include it
		fX[0] = fX[1] = fSegmentStartX;
		fX[2] = fSegmentEndX;
		fY[0] = fY[1] = fSegmentStartY;
		fY[2] = fSegmentEndY;
		SecondPassSink.SetActualElement(nID);
		Rasterizer.CallbackFillConservative(fX,fY,&SecondPassSink);
	}

	return true;
}

void CRayMap_DimensionArray::Search( std::vector<CRayMap_Segment>& vRaySegmentList, std::vector<int32>& vRayList, const Vec3 vPosition, const float fRadius ) const
{
	//0 == just add else check the old list...
	bool bCheckTheOriginalList = ( 0 != m_nDimensionID );

	float fX,fY;
	ConvertToDAUnits(vPosition, fX,fY );

	float fDARadiusX = fRadius * m_fOnePerCellWidth;
	float fDARadiusY = fRadius * m_fOnePerCellHeight;

	int32 nStartX = (int32)floor(fX-fDARadiusX);
	int32 nStartY = (int32)floor(fY-fDARadiusY);
	int32 nEndX = (int32)ceil(fX+fDARadiusX);
	int32 nEndY = (int32)ceil(fY+fDARadiusY);

	if(bCheckTheOriginalList)
	{
		float fT;
		float fSqrRadius = fRadius*fRadius;
		//gather the possible rays
		for( int32 Y = nStartY; Y < nEndY; ++Y )
			for( int32 X = nStartX; X < nEndX; ++X )
			{
				int32 *pList  = &m_pList[ m_pDimensionIndex[ (Y*m_nXDimension+X)*2 ] ];
				int32 nNumber = m_pDimensionIndex[ (Y*m_nXDimension+X)*2+1 ];
				for( int32 i = 0; i < nNumber; ++i, ++pList)
				{
					//that point, where I check the distance between the segment & the point
					CRayMap_Segment* pSegment = &vRaySegmentList[*pList];
					if( Distance::Point_LinesegSq( vPosition, Lineseg( pSegment->m_vStartPos, pSegment->m_vEndPos ), fT ) <= fSqrRadius )
							vRayList.push_back( *pList );
				}
			}
	}
	else
	{
		std::vector<int32> vTemp = vRayList;
		vRayList.clear();

		//gather the possible rays
		for( int32 Y = nStartY; Y < nEndY; ++Y )
			for( int32 X = nStartX; X < nEndX; ++X )
			{
				int32 *pList  = &m_pList[ m_pDimensionIndex[ (Y*m_nXDimension+X)*2 ] ];
				int32 nNumber = m_pDimensionIndex[ (Y*m_nXDimension+X)*2+1 ];
				for( int32 i = 0; i < nNumber; ++i, ++pList)
				{
					//it's in the last list
					if( std::find( vTemp.begin(), vTemp.end(), *pList) != vTemp.end() )
					{
						vRayList.push_back( *pList );
					}
				}
			}
	}
}
