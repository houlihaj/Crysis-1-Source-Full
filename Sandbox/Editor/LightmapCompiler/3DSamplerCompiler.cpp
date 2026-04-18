/*=============================================================================
3DSamplerCompiler.cpp : 
Copyright 2006 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/

#include "stdafx.h"
#include "I3dEngine.h"
#include "3DSamplerCompiler.h"

//-------------------------------------------------------------------------------------------
// Global function to sort the points based on location
//-------------------------------------------------------------------------------------------
static inline bool GeneratedPoints_Less(const Vec3& v1, const Vec3& v2) 
{
	if( v1.z < v2.z )
		return true;
	if( v1.y < v2.y )
		return true;
	return ( v1.x < v2.x );
}

//-------------------------------------------------------------------------------------------
// Generate a full list of points
//-------------------------------------------------------------------------------------------
bool	C3DSamplerCompiler::Prepare( const f32 fDistBetween2Points, e3DSamplingType SamplingType )
{
	I3DEngine* pI3DEngine = GetIEditor()->Get3DEngine();
	m_pSampler = NULL; // pI3DEngine->Get3DSampler();

	if( NULL == m_pSampler )
		return false;

	m_pSampler->SetSamplingType( SamplingType );

	//generate the pont list and the octree
	if( false == GeneratePointList( fDistBetween2Points ) )
		return false;

	return true;
}

//-------------------------------------------------------------------------------------------
// Generate point list's based on the visareas
//-------------------------------------------------------------------------------------------
bool	C3DSamplerCompiler::GeneratePointList( const f32 fDistBetween2Points )
{
	if( NULL == m_pSampler )
		return false;

	I3DEngine* pI3DEngine			 = GetIEditor()->Get3DEngine();
	if( NULL == pI3DEngine )
		return false;

	IVisAreaManager* pVisAreaManager = pI3DEngine->GetIVisAreaManager();
	if( NULL == pVisAreaManager )
		return false;

	int nNumberOfVisAreas = pVisAreaManager->GetNumberOfVisArea();

	if( 0 == nNumberOfVisAreas )
		return false;

	Vec3 vScale( fDistBetween2Points, fDistBetween2Points, fDistBetween2Points );
	AABB GlobalBBox;

	GlobalBBox.Reset();
	//generate global bbox for the visareas
	for( int i = 0; i < nNumberOfVisAreas; ++i )
	{
		IVisArea* pVisArea = pVisAreaManager->GetVisAreaById( i );
		if( pVisArea )
			GlobalBBox.Add( *pVisArea->GetAABBox() );
	}

	//generate valid points based on the visareas & portals
	std::vector<Vec3> m_vGeneratedPoints;
	for( int i = 0; i < nNumberOfVisAreas; ++i )
	{
		IVisArea* pVisArea = pVisAreaManager->GetVisAreaById(i);
		const AABB* pBBox = pVisArea->GetAABBox();

		//generate 3d grid based on that visarea
		int nStartX = (int)floor( (pBBox->min.x - GlobalBBox.min.x) / vScale.x );
		int nStartY = (int)floor( (pBBox->min.y - GlobalBBox.min.y) / vScale.y );
		int nStartZ = (int)floor( (pBBox->min.z - GlobalBBox.min.z) / vScale.z );

		int nEndX = (int)ceil( (pBBox->max.x - GlobalBBox.min.x) / vScale.x ) + 1;
		int nEndY = (int)ceil( (pBBox->max.y - GlobalBBox.min.y) / vScale.y ) + 1;
		int nEndZ = (int)ceil( (pBBox->max.z - GlobalBBox.min.z) / vScale.z ) + 1;

		for( int z = nStartZ; z < nEndZ; ++z )
			for( int y = nStartY; y < nEndY; ++y )
				for( int x = nStartX; x < nEndX; ++x )
				{
					//validate the position - it is inside the visarea?
					Vec3 vWorldPos( (f32)x*vScale.x + GlobalBBox.min.x, (f32)y*vScale.y + GlobalBBox.min.y,  (f32)z*vScale.z + GlobalBBox.min.z );
						if( pVisArea->IsPointInsideVisArea( vWorldPos ) )
							m_vGeneratedPoints.push_back( Vec3( (f32)x, (f32)y, (f32)z ) );
				}
	}

	if( 0 == m_vGeneratedPoints.size() )
		return false;

	//remove the duplicated points
	std::sort( m_vGeneratedPoints.begin(), m_vGeneratedPoints.end(), GeneratedPoints_Less );
	int nPointNumber = m_vGeneratedPoints.size();
	for( int i = 0; i < nPointNumber-1; ++i )
	{
		if( m_vGeneratedPoints[i].x == m_vGeneratedPoints[i+1].x &&
			  m_vGeneratedPoints[i].y == m_vGeneratedPoints[i+1].y &&
				m_vGeneratedPoints[i].z == m_vGeneratedPoints[i+1].z )
		{
			//the next is the same point.. remove it
			m_vGeneratedPoints.erase( m_vGeneratedPoints.begin() + i );
			--i;
			--nPointNumber;
		}
	}

	//put the point to the octree
	return m_pSampler->CreateIrregularSampling( GlobalBBox, vScale, nPointNumber, &m_vGeneratedPoints[0].x );
}
