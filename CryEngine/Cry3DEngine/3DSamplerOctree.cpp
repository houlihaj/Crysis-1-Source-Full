/*=============================================================================
3DSamplerOctree.cpp : 
Copyright 2006 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/

#include "StdAfx.h"
#include "LMSerializationManager.h"
#include "3DSamplerOctree.h"

static const f32 _3DSMP_EPSILON = 0.0001f;


static inline	uint8	GetIntersectionAndGenerateBBoxes( const AABB& ParentAABB, AABB* pChildsAABB, const Vec3& vOrigin, const f32 fHalfEdgeSize )
{
	//the ParentAABB & the AABox need to have an shared part!

	Vec3 vHalf( ( ParentAABB.max.x - ParentAABB.min.x ) * 0.5f, 
		( ParentAABB.max.y - ParentAABB.min.y ) * 0.5f,
		( ParentAABB.max.z - ParentAABB.min.z ) * 0.5f );
	Vec3 vCenter( vHalf.x + ParentAABB.min.x, vHalf.y + ParentAABB.min.y, vHalf.z + ParentAABB.min.z );

	//eveything is active
	uint8 nBits = 0xff;

	//disable which haven't got shaderd part..
	if( vCenter.x > vOrigin.x )
	{
	 if( vCenter.x > vOrigin.x+fHalfEdgeSize )
		 nBits &= 0x10 | 0x20 | 0x40 | 0x80;
	}
	else
	{
		if( vCenter.x < vOrigin.x-fHalfEdgeSize )
			nBits &= 0x1 | 0x2 | 0x4 | 0x8;
	}

	if( vCenter.y > vOrigin.y )
	{
		if( vCenter.y > vOrigin.y+fHalfEdgeSize )
			nBits &= 0x2 | 0x8 | 0x20 | 0x80;
	}
	else
	{
		if( vCenter.y < vOrigin.y-fHalfEdgeSize )
			nBits &= 0x1 | 0x4 | 0x10 | 0x40;
	}

	if( vCenter.z > vOrigin.z )
	{
		if( vCenter.z > vOrigin.z+fHalfEdgeSize )
			nBits &= 0x4 | 0x8 | 0x40 | 0x80;
	}
	else
	{
		if( vCenter.z < vOrigin.z-fHalfEdgeSize )
			nBits &= 0x1 | 0x2 | 0x10 | 0x20;
	}

	if( nBits & 0x1 )
	{
		pChildsAABB[0].min.x = vCenter.x - _3DSMP_EPSILON;
		pChildsAABB[0].max.x = vCenter.x + vHalf.x + _3DSMP_EPSILON;
		pChildsAABB[0].min.y = vCenter.y - _3DSMP_EPSILON;
		pChildsAABB[0].max.y = vCenter.y + vHalf.y + _3DSMP_EPSILON;
		pChildsAABB[0].min.z = vCenter.z - _3DSMP_EPSILON;
		pChildsAABB[0].max.z = vCenter.z + vHalf.z + _3DSMP_EPSILON;
	}

	if( nBits & 0x2 )
	{
		pChildsAABB[1].min.x = vCenter.x - _3DSMP_EPSILON;
		pChildsAABB[1].max.x = vCenter.x + vHalf.x + _3DSMP_EPSILON;
		pChildsAABB[1].min.y = vCenter.y - vHalf.y - _3DSMP_EPSILON;
		pChildsAABB[1].max.y = vCenter.y + _3DSMP_EPSILON;
		pChildsAABB[1].min.z = vCenter.z - _3DSMP_EPSILON;
		pChildsAABB[1].max.z = vCenter.z + vHalf.z + _3DSMP_EPSILON;
	}

	if( nBits & 0x4 )
	{
		pChildsAABB[2].min.x = vCenter.x - _3DSMP_EPSILON;
		pChildsAABB[2].max.x = vCenter.x + vHalf.x + _3DSMP_EPSILON;
		pChildsAABB[2].min.y = vCenter.y - _3DSMP_EPSILON;
		pChildsAABB[2].max.y = vCenter.y + vHalf.y + _3DSMP_EPSILON;
		pChildsAABB[2].min.z = vCenter.z - vHalf.z - _3DSMP_EPSILON;
		pChildsAABB[2].max.z = vCenter.z + _3DSMP_EPSILON;
	}

	if( nBits & 0x8 )
	{
		pChildsAABB[3].min.x = vCenter.x - _3DSMP_EPSILON;
		pChildsAABB[3].max.x = vCenter.x + vHalf.x + _3DSMP_EPSILON;
		pChildsAABB[3].min.y = vCenter.y - vHalf.y - _3DSMP_EPSILON;
		pChildsAABB[3].max.y = vCenter.y + _3DSMP_EPSILON;
		pChildsAABB[3].min.z = vCenter.z - vHalf.z - _3DSMP_EPSILON;
		pChildsAABB[3].max.z = vCenter.z + _3DSMP_EPSILON;
	}

	if( nBits & 0x10 )
	{
		pChildsAABB[4].min.x = vCenter.x - vHalf.x - _3DSMP_EPSILON;
		pChildsAABB[4].max.x = vCenter.x + _3DSMP_EPSILON;
		pChildsAABB[4].min.y = vCenter.y - _3DSMP_EPSILON;
		pChildsAABB[4].max.y = vCenter.y + vHalf.y + _3DSMP_EPSILON;
		pChildsAABB[4].min.z = vCenter.z - _3DSMP_EPSILON;
		pChildsAABB[4].max.z = vCenter.z + vHalf.z + _3DSMP_EPSILON;
	}

	if( nBits & 0x20 )
	{
		pChildsAABB[5].min.x = vCenter.x - vHalf.x - _3DSMP_EPSILON;
		pChildsAABB[5].max.x = vCenter.x + _3DSMP_EPSILON;
		pChildsAABB[5].min.y = vCenter.y - vHalf.y - _3DSMP_EPSILON;
		pChildsAABB[5].max.y = vCenter.y + _3DSMP_EPSILON;
		pChildsAABB[5].min.z = vCenter.z - _3DSMP_EPSILON;
		pChildsAABB[5].max.z = vCenter.z + vHalf.z + _3DSMP_EPSILON;
	}

	if( nBits & 0x40 )
	{
		pChildsAABB[6].min.x = vCenter.x - vHalf.x - _3DSMP_EPSILON;
		pChildsAABB[6].max.x = vCenter.x + _3DSMP_EPSILON;
		pChildsAABB[6].min.y = vCenter.y - _3DSMP_EPSILON;
		pChildsAABB[6].max.y = vCenter.y + vHalf.y + _3DSMP_EPSILON;
		pChildsAABB[6].min.z = vCenter.z - vHalf.z - _3DSMP_EPSILON;
		pChildsAABB[6].max.z = vCenter.z + _3DSMP_EPSILON;
	}

	if( nBits & 0x80 )
	{
		pChildsAABB[7].min.x = vCenter.x - vHalf.x - _3DSMP_EPSILON;
		pChildsAABB[7].max.x = vCenter.x + _3DSMP_EPSILON;
		pChildsAABB[7].min.y = vCenter.y - vHalf.y - _3DSMP_EPSILON;
		pChildsAABB[7].max.y = vCenter.y + _3DSMP_EPSILON;
		pChildsAABB[7].min.z = vCenter.z - vHalf.z - _3DSMP_EPSILON;
		pChildsAABB[7].max.z = vCenter.z + _3DSMP_EPSILON;
	}

	return nBits;
}


static inline void GenerateBBoxes( const AABB& ParentAABB, AABB* pChildsAABB )
{
	Vec3 vHalf( ( ParentAABB.max.x - ParentAABB.min.x ) * 0.5f, 
		( ParentAABB.max.y - ParentAABB.min.y ) * 0.5f,
		( ParentAABB.max.z - ParentAABB.min.z ) * 0.5f );
	Vec3 vCenter( vHalf.x + ParentAABB.min.x, vHalf.y + ParentAABB.min.y, vHalf.z + ParentAABB.min.z );

	pChildsAABB[0].min.x = vCenter.x - _3DSMP_EPSILON;
	pChildsAABB[0].max.x = vCenter.x + vHalf.x + _3DSMP_EPSILON;
	pChildsAABB[0].min.y = vCenter.y - _3DSMP_EPSILON;
	pChildsAABB[0].max.y = vCenter.y + vHalf.y + _3DSMP_EPSILON;
	pChildsAABB[0].min.z = vCenter.z - _3DSMP_EPSILON;
	pChildsAABB[0].max.z = vCenter.z + vHalf.z + _3DSMP_EPSILON;

	pChildsAABB[1].min.x = vCenter.x - _3DSMP_EPSILON;
	pChildsAABB[1].max.x = vCenter.x + vHalf.x + _3DSMP_EPSILON;
	pChildsAABB[1].min.y = vCenter.y - vHalf.y - _3DSMP_EPSILON;
	pChildsAABB[1].max.y = vCenter.y + _3DSMP_EPSILON;
	pChildsAABB[1].min.z = vCenter.z - _3DSMP_EPSILON;
	pChildsAABB[1].max.z = vCenter.z + vHalf.z + _3DSMP_EPSILON;

	pChildsAABB[2].min.x = vCenter.x - _3DSMP_EPSILON;
	pChildsAABB[2].max.x = vCenter.x + vHalf.x + _3DSMP_EPSILON;
	pChildsAABB[2].min.y = vCenter.y - _3DSMP_EPSILON;
	pChildsAABB[2].max.y = vCenter.y + vHalf.y + _3DSMP_EPSILON;
	pChildsAABB[2].min.z = vCenter.z - vHalf.z - _3DSMP_EPSILON;
	pChildsAABB[2].max.z = vCenter.z + _3DSMP_EPSILON;

	pChildsAABB[3].min.x = vCenter.x - _3DSMP_EPSILON;
	pChildsAABB[3].max.x = vCenter.x + vHalf.x + _3DSMP_EPSILON;
	pChildsAABB[3].min.y = vCenter.y - vHalf.y - _3DSMP_EPSILON;
	pChildsAABB[3].max.y = vCenter.y + _3DSMP_EPSILON;
	pChildsAABB[3].min.z = vCenter.z - vHalf.z - _3DSMP_EPSILON;
	pChildsAABB[3].max.z = vCenter.z + _3DSMP_EPSILON;

	pChildsAABB[4].min.x = vCenter.x - vHalf.x - _3DSMP_EPSILON;
	pChildsAABB[4].max.x = vCenter.x + _3DSMP_EPSILON;
	pChildsAABB[4].min.y = vCenter.y - _3DSMP_EPSILON;
	pChildsAABB[4].max.y = vCenter.y + vHalf.y + _3DSMP_EPSILON;
	pChildsAABB[4].min.z = vCenter.z - _3DSMP_EPSILON;
	pChildsAABB[4].max.z = vCenter.z + vHalf.z + _3DSMP_EPSILON;

	pChildsAABB[5].min.x = vCenter.x - vHalf.x - _3DSMP_EPSILON;
	pChildsAABB[5].max.x = vCenter.x + _3DSMP_EPSILON;
	pChildsAABB[5].min.y = vCenter.y - vHalf.y - _3DSMP_EPSILON;
	pChildsAABB[5].max.y = vCenter.y + _3DSMP_EPSILON;
	pChildsAABB[5].min.z = vCenter.z - _3DSMP_EPSILON;
	pChildsAABB[5].max.z = vCenter.z + vHalf.z + _3DSMP_EPSILON;

	pChildsAABB[6].min.x = vCenter.x - vHalf.x - _3DSMP_EPSILON;
	pChildsAABB[6].max.x = vCenter.x + _3DSMP_EPSILON;
	pChildsAABB[6].min.y = vCenter.y - _3DSMP_EPSILON;
	pChildsAABB[6].max.y = vCenter.y + vHalf.y + _3DSMP_EPSILON;
	pChildsAABB[6].min.z = vCenter.z - vHalf.z - _3DSMP_EPSILON;
	pChildsAABB[6].max.z = vCenter.z + _3DSMP_EPSILON;

	pChildsAABB[7].min.x = vCenter.x - vHalf.x - _3DSMP_EPSILON;
	pChildsAABB[7].max.x = vCenter.x + _3DSMP_EPSILON;
	pChildsAABB[7].min.y = vCenter.y - vHalf.y - _3DSMP_EPSILON;
	pChildsAABB[7].max.y = vCenter.y + _3DSMP_EPSILON;
	pChildsAABB[7].min.z = vCenter.z - vHalf.z - _3DSMP_EPSILON;
	pChildsAABB[7].max.z = vCenter.z + _3DSMP_EPSILON;
}
//-------------------------------------------------------------------------------------------
//Give back the interpolated datas from the octree
//-------------------------------------------------------------------------------------------
bool C3DSamplerOctree::GetInterpolatedData( const Vec3& vPosition, f32* pFloats )
{
	//set to zero => no info
	int nFloatNumber = GetNumberOfFloats();
	if( -1 == nFloatNumber )
		return false;

	memset( pFloats, 0, sizeof(f32)*nFloatNumber);

	if( NULL == m_pOctree )
		return false;

	//generate scaled, integer positions
	//go to local space
	Vec3 vLocalSpacePosition = (vPosition - m_GlobalBBox.min);
	vLocalSpacePosition.x *= m_vInvScale.x;
	vLocalSpacePosition.y *= m_vInvScale.y;
	vLocalSpacePosition.z *= m_vInvScale.z;

	uint8 nActiveChilds;
	static AABB ChildAABB[8];
	int nMaxStackSize;
	int nStackPos = 1;

	if( m_nSizeOfTheStack )
		nMaxStackSize = m_nSizeOfTheStack;
	else
	{
		nMaxStackSize = m_nSizeOfTheStack = 16;
		m_pStack = new s3DSamplerOctreeCellStack[16];
	}

	m_pStack[0].m_pCell = &m_pOctree[0];
	m_pStack[0].m_AABB = m_LocalBBox;

	static int fDistMatrix[4*4*4];
	static int nIDMatrix[4*4*4];
	int nPointFound = 0;
	f32 fAllSqrDist = 0;

	while( nStackPos > 0 )
	{
		--nStackPos;
		s3DSamplerOctreeCell* pCurrent = m_pStack[ nStackPos ].m_pCell;

		//have datas
		if( pCurrent->m_nFlags )
		{
			int nItemPos = *((int*)&pCurrent->m_pDatas[0]);
			f32* pItems = &m_pPositions[ nItemPos*3 ];

			//for every item
			for( int i = 0; i < pCurrent->m_nFlags; ++i )
			{
				Vec3 vLocalPos( pItems[0] - vLocalSpacePosition.x, pItems[1] - vLocalSpacePosition.y,pItems[2] - vLocalSpacePosition.z );
				pItems += 3;
				++nItemPos;
				//early out

				f32 fSqrDist = vLocalPos.x*vLocalPos.x + vLocalPos.y*vLocalPos.y + vLocalPos.z*vLocalPos.z;
				if( fSqrDist > 2.f*2.f )
						continue;

				//need to find the place in the 4*4*4 matrix
				fAllSqrDist += fSqrDist;
				fDistMatrix[ nPointFound ] = (int)fSqrDist;
				nIDMatrix[ nPointFound ] = nItemPos-1;
				++nPointFound;
			}
			continue;
		}

		//need to go deeper
		nActiveChilds = GetIntersectionAndGenerateBBoxes( m_pStack[ nStackPos].m_AABB, ChildAABB, vLocalSpacePosition, 2.f );
		
		//no child is active
		if( 0 == nActiveChilds )
			continue;

		for( int i = 0; i < 8; ++i )
		{
			//early skip..
			if( 0 == (nActiveChilds & (1<<i)) )
				continue;

			s3DSamplerOctreeCell* pChild = *((s3DSamplerOctreeCell**)&pCurrent->m_pDatas[i]);
			if( NULL == pChild )
				continue;

			//grow the stack
			if( nMaxStackSize == 0 )
			{
				m_nSizeOfTheStack += 16;
				s3DSamplerOctreeCellStack* pNewStack = new s3DSamplerOctreeCellStack[m_nSizeOfTheStack];
				if( NULL == pNewStack )
					return false;
				memset( pNewStack, 0, sizeof(s3DSamplerOctreeCellStack)*m_nSizeOfTheStack);
				if( m_pStack )
				{
					memcpy( pNewStack, m_pStack, sizeof(s3DSamplerOctreeCellStack)*nStackPos );
					delete [] m_pStack;
				}
				m_pStack = pNewStack;
				nMaxStackSize += 16;
			}

			m_pStack[ nStackPos ].m_pCell = pChild;
			m_pStack[ nStackPos ].m_AABB = ChildAABB[i];
			++nStackPos;
			--nMaxStackSize;
		}
	}

	//using inverse distance weighting for interpolation

	//no valid point
	if( 0 == nPointFound )
		return true;

	f32* pData = &m_pPointDatas[ nIDMatrix[0]*nFloatNumber ];

	//too close, use the first one..
	if( 10e-5f > fAllSqrDist )
	{
		memcpy( pFloats, pData, nFloatNumber*sizeof(f32) );
		return true;
	}

	f32 fInvSqrDist = 1.f / fAllSqrDist;

	f32 fWeight = fDistMatrix[0] * fInvSqrDist;
	for( int i = 0; i < nFloatNumber; ++i )
		pFloats[i] = pData[i] * fWeight;

	for( int i = 1; i < nPointFound; ++i )
	{
		pData = &m_pPointDatas[ nIDMatrix[i]*nFloatNumber ];
		fWeight = fDistMatrix[i] * fInvSqrDist;
		for( int j = 0; j < nFloatNumber; ++j )
			pFloats[j] += pData[j] * fWeight;
	}

	return true;
/*
	optimized b-spline interpolation - removed because it need a dilatation code for the data matrix - take too much time
	int nX = (int)vLocalSpacePosition.x;
	int nY = (int)vLocalSpacePosition.y;
	int nZ = (int)vLocalSpacePosition.z;

	//generate b-spline interpolation matrix

	static float fP[4];
	static float fQ[4];
	static float fR[4];

	float fT = vLocalSpacePosition.x - (float)nX;		//fract(x)

	float fsqrT = fT*fT;
	float f1T3 = 1.f / 6.f *fT *fsqrT;
	float f3T = 3.f / 6.f *fT;

	fP[0] = 1.0f/6.0f - f3T  + 3.0f/6.0f*fsqrT - f1T3;
	fP[1] = 4.0f/6.0f + (-6.0f/6.0f + f3T )*fsqrT;
	fP[2] = 1.0f/6.0f + f3T  +  ( 3.0f/6.0f - f3T )*fsqrT;
	fP[3] =  f1T3;


	fT = vLocalSpacePosition.x - (float)nY;		//fract(y)

	fsqrT = fT*fT;
	f1T3 = 1.f / 6.f *fT *fsqrT;
	f3T = 3.f / 6.f *fT;

	fQ[0] = 1.0f/6.0f - f3T  + 3.0f/6.0f*fsqrT - f1T3;
	fQ[1] = 4.0f/6.0f + (-6.0f/6.0f + f3T )*fsqrT;
	fQ[2] = 1.0f/6.0f + f3T  +  ( 3.0f/6.0f - f3T )*fsqrT;
	fQ[3] =  f1T3;


	fT = vLocalSpacePosition.x - (float)nZ;		//fract(z)

	fsqrT = fT*fT;
	f1T3 = 1.f / 6.f *fT *fsqrT;
	f3T = 3.f / 6.f *fT;

	fR[0] = 1.0f/6.0f - f3T  + 3.0f/6.0f*fsqrT - f1T3;
	fR[1] = 4.0f/6.0f + (-6.0f/6.0f + f3T )*fsqrT;
	fR[2] = 1.0f/6.0f + f3T  +  ( 3.0f/6.0f - f3T )*fsqrT;
	fR[3] =  f1T3;


	//calculate the weight matrix

	static float fWeightMatrix[4*4*4];
	float fTemp;

	for( int i = 0; i < 4; ++i )	//i==z, j ==y, k == x
	{
		fTemp = fQ[0]*fR[i]*2;
		fWeightMatrix[(i*4+0)*4+0] = fP[0]*fTemp;
		fWeightMatrix[(i*4+0)*4+1] = fP[1]*fTemp;
		fWeightMatrix[(i*4+0)*4+2] = fP[2]*fTemp;
		fWeightMatrix[(i*4+0)*4+3] = fP[3]*fTemp;


		fTemp = fQ[1]*fR[i]*2;
		fWeightMatrix[(i*4+1)*4+0] = fP[0]*fTemp;
		fWeightMatrix[(i*4+1)*4+1] = fP[1]*fTemp;
		fWeightMatrix[(i*4+1)*4+2] = fP[2]*fTemp;
		fWeightMatrix[(i*4+1)*4+3] = fP[3]*fTemp;

		fTemp = fQ[2]*fR[i]*2;
		fWeightMatrix[(i*4+2)*4+0] = fP[0]*fTemp;
		fWeightMatrix[(i*4+2)*4+1] = fP[1]*fTemp;
		fWeightMatrix[(i*4+2)*4+2] = fP[2]*fTemp;
		fWeightMatrix[(i*4+2)*4+3] = fP[3]*fTemp;

		fTemp = fQ[3]*fR[i]*2;
		fWeightMatrix[(i*4+3)*4+0] = fP[0]*fTemp;
		fWeightMatrix[(i*4+3)*4+1] = fP[1]*fTemp;
		fWeightMatrix[(i*4+3)*4+2] = fP[2]*fTemp;
		fWeightMatrix[(i*4+3)*4+3] = fP[3]*fTemp;
	}


	//now interpolate the datas
	float* pGrid;
	int nXPos = (nX-1)*(nFloatNumber);
	float* pWeight = fWeightMatrix;

	for( int z = 0; z < 4; ++z )
	{
		pGrid = m_pPointDatas;
//		pGrid = m_pGrid[ (((nZ-1+z)*m_nGridSizeY+(nY-1))*m_nGridSizeX ];
		for( int y = 0; y < 4; ++y )
		{
			float* pGrid20 = pGrid + nXPos;
			float* pGrid21 = pGrid + nXPos+(3+nFloatNumber);
			float* pGrid22 = pGrid + nXPos+(3+nFloatNumber)*2;
			float* pGrid23 = pGrid + nXPos+(3+nFloatNumber)*3;

			for( int i = 0; i < nFloatNumber; ++i )
			{
					pFloats[i] += pWeight[0] * pGrid20[i] +
												pWeight[1] * pGrid21[i] +
												pWeight[2] * pGrid22[i] +
												pWeight[3] * pGrid23[i];
			}

			pWeight += 4;
		}
//		pGrid += m_nGridSizeX;
	}
*/
	return true;
};

//-------------------------------------------------------------------------------------------
// Save
//-------------------------------------------------------------------------------------------
bool C3DSamplerOctree::Save( const char* szFilePath )
{
	string strDirName = PathUtil::GetParentDirectory(szFilePath);
	const char* pFileName = szFilePath + (strDirName.empty()?0:strDirName.length()+1);
	string strPakName = strDirName + "\\" LEVELLM_PAK_NAME;
	GetPak()->ClosePack(strPakName.c_str());
	// make sure the pak file in which this LM data resides is opened
	CrySetFileAttributes(strPakName.c_str(), FILE_ATTRIBUTE_NORMAL);
	ICryArchive_AutoPtr pPak = GetPak()->OpenArchive (strPakName.c_str(), ICryArchive::FLAGS_RELATIVE_PATHS_ONLY);
	if (!pPak)
		return false;

	//setup the header
	sHeader_3DSamplerOctree sHeader;

	sHeader.m_nRevisionNumber = OCTREE_3D_SAMPLER_FILE_VERSION;
	sHeader.m_SamplingType = m_eSamplingType;
	sHeader.m_GlobalBBox = m_GlobalBBox;
	sHeader.m_vScale = m_vScale;
	sHeader.m_nNumberOfCells = m_nOctreeCellNumber;
	sHeader.m_nNumberOfPoints = m_nPointNumber;

	CTempFile fMem;
	fMem.Write (sHeader);	//write the header

	//write the sample positions
	fMem.WriteData( m_pPositions, sizeof(f32)*m_nPointNumber*3 );
	//write the sample datas
	fMem.WriteData( m_pPointDatas, sizeof(f32)*m_nPointNumber*GetNumberOfFloats() );

	//write the cells
	s3DSamplerOctreeCell_Serialize TempCell;
	for( int i = 0; i < m_nOctreeCellNumber; ++i )
	{
		TempCell.m_nFlags = m_pOctree[i].m_nFlags;

		if( 0 == TempCell.m_nFlags )
		{
			//the leaf case..
			TempCell.m_pDatas[0] = (int32) *((int*)&(m_pOctree[i].m_pDatas[0]));
			memset( &(TempCell.m_pDatas[1]), 0, sizeof(int32)*7 );
		}
		else
		{
			//exchange the pointers to offset... (childs created later than the original cell..)
			for( int j = 0; j < 8; ++j )
				if( m_pOctree[i].m_pDatas[j] )
				{
					s3DSamplerOctreeCell* pChild = (s3DSamplerOctreeCell*)(m_pOctree[i].m_pDatas[j]);
					int k;
					for( k = i+1; k < m_nOctreeCellNumber; ++k )
						if( pChild == &m_pOctree[k] )
							break;

					TempCell.m_pDatas[j] = ( k == m_nOctreeCellNumber ) ? 0 : (k+1);		//0 == NULL pointer, 1 is the first element!
				}
				else
					TempCell.m_pDatas[j] = 0;
		}


		fMem.Write( TempCell );
	}


	return (0 != pPak->UpdateFile(pFileName, fMem.GetData(), fMem.GetSize()));
}

//-------------------------------------------------------------------------------------------
// Load
//-------------------------------------------------------------------------------------------
bool C3DSamplerOctree::Load( const char* szFileName )
{
	string strDirName = PathUtil::GetParentDirectory(szFileName);
	string lmFilename = (strDirName + "\\" LEVELLM_PAK_NAME);
	{
		// Check if lightmap pak file exist.
		_finddata_t fd;
		intptr_t handle = GetPak()->FindFirst( lmFilename,&fd );
		if (handle == -1)
		{
			return false;
		}
		GetPak()->FindClose( handle );
	}

	// ---------------------------------------------------------------------------------------------
	// Reconstruct lightmap data from pszFileName
	// ---------------------------------------------------------------------------------------------
	PrintMessage("Loading 3DSampler ...");

	// make sure the pak file in which this LM data resides is opened
	GetPak()->OpenPack( lmFilename.c_str() );

	sHeader_3DSamplerOctree sHeader;

	FILE* hFile = GetPak()->FOpen(szFileName, "rb");
	if (hFile == NULL)
	{
		PrintMessage("Could not load 3DSampler file");
		return false;
	}

	// Read header
	size_t iNumItemsRead = GetPak()->FRead(&sHeader, 1, hFile);

	if( sHeader.m_nRevisionNumber != OCTREE_3D_SAMPLER_FILE_VERSION )
	{
		PrintMessage("This 3DSampler file version not supported");
		GetPak()->FClose(hFile);
		return false;
	}

	sHeader.m_nNumberOfCells = m_nOctreeCellNumber;
	sHeader.m_nNumberOfPoints = m_nPointNumber;

	//set default parameters
	SetSamplingType( sHeader.m_SamplingType );
	DefineLocalSpace( sHeader.m_GlobalBBox, sHeader.m_vScale );

	//read the sample positions
	GetPak()->FRead( m_pPositions, sHeader.m_nNumberOfPoints*3, hFile );
	//read the sample datas
	GetPak()->FRead( m_pPointDatas, sHeader.m_nNumberOfPoints*GetNumberOfFloats(), hFile );
	//read the cells
	s3DSamplerOctreeCell_Serialize TempCell;
	for( int i = 0; i < m_nOctreeCellNumber; ++i )
	{
		GetPak()->FRead( &TempCell, 1, hFile );

		m_pOctree[i].m_nFlags = TempCell.m_nFlags;

		//remap the pointers in the cells
		if( 0 == TempCell.m_nFlags )
		{
			//the leaf case..
			*((int*)&m_pOctree[i].m_pDatas[0]) = (int) *((int32*)&(TempCell.m_pDatas[0]));
			memset( &(m_pOctree[i].m_pDatas[1]), 0, sizeof(int)*7 );
		}
		else
		{
			//exchange the offset to pointers
			for( int j = 0; j < 8; ++j )
			{
				if( TempCell.m_pDatas[j] )
					*((s3DSamplerOctreeCell**)&m_pOctree[i].m_pDatas[i]) = &m_pOctree[ TempCell.m_pDatas[i]-1 ];
				else
					m_pOctree[i].m_pDatas[j] = NULL;
			}
		}
	}

	GetPak()->FClose(hFile);
	return true;
}

//-------------------------------------------------------------------------------------------
// Define the translation / scale from world space to local space
//-------------------------------------------------------------------------------------------
void	C3DSamplerOctree::DefineLocalSpace( const AABB& GlobalBBox, const Vec3& vScale )
{
	m_GlobalBBox = GlobalBBox;
	m_vScale = vScale;

	if( fabsf(m_vScale.x) > 10e-7f )
		m_vInvScale.x = 1.f / m_vScale.x;
	else
		m_vInvScale.x = 0;

	if( fabsf(m_vScale.y) > 10e-7f )
		m_vInvScale.y = 1.f / m_vScale.y;
	else
		m_vInvScale.y = 0;

	if( fabsf(m_vScale.z) > 10e-7 )
		m_vInvScale.z = 1.f / m_vScale.z;
	else
		m_vInvScale.z = 0;

	m_LocalBBox.min.Set(0,0,0);
	m_LocalBBox.max.Set( (GlobalBBox.max.x-GlobalBBox.min.x) * m_vInvScale.x , (GlobalBBox.max.y-GlobalBBox.min.y) * m_vInvScale.y, (GlobalBBox.max.z-GlobalBBox.min.z) * m_vInvScale.z );
}

//-------------------------------------------------------------------------------------------
//Create the octree for that points
//-------------------------------------------------------------------------------------------
bool	C3DSamplerOctree::CreateIrregularSampling( const AABB& GlobalBBox, const Vec3& vScale , const int nPointNumber, const f32* pPoints )
{
	if( 0 == nPointNumber )
		return false;

	//define local space
	DefineLocalSpace( GlobalBBox, vScale );

	//delete all array, no resizing...
	SAFE_DELETE( m_pOctree );

	//Generate all array
	if( false == ResizePointArray( nPointNumber ) )
		return false;

	//copy the points
	memcpy( m_pPositions, pPoints, sizeof(f32)*3*nPointNumber );

	//Generate the octree
	int nOctreeCellPosition = 1;
	m_nOctreeCellNumber = 16;
	m_pOctree = new s3DSamplerOctreeCell[m_nOctreeCellNumber];

	memset( &m_pOctree[0], 0, sizeof(s3DSamplerOctreeCell) );
	//include all the items to the root
	m_pOctree[0].m_nFlags = nPointNumber;
	*((int*)&m_pOctree[0].m_pDatas[0]) = 0;

	//include it to the stack
	if( 0 == m_nSizeOfTheStack )
	{
		m_nSizeOfTheStack = 16;
		m_pStack = new s3DSamplerOctreeCellStack[m_nSizeOfTheStack];
	}

	int nStackPos = 1;
	m_pStack[0].m_pCell = &m_pOctree[0];
	m_pStack[0].m_AABB = m_LocalBBox;


	//create a 2nd buffer to sort the position based on which bbox included it
	int nTempIndicatorSize = (nPointNumber+7)/8;
	int8* pTempIndicator = new int8[ nTempIndicatorSize ];
	if(NULL == pTempIndicator )
		return false;
	f32* pTempPositions = new f32[3*nPointNumber ];
	if(NULL == pTempPositions )
		return false;

	int nTempPos;
	AABB ChildAABB[8];

	while( nStackPos > 0 )
	{
		--nStackPos;
		s3DSamplerOctreeCell* pCurrent = m_pStack[ nStackPos ].m_pCell;
		GenerateBBoxes( m_pStack[nStackPos].m_AABB, &ChildAABB[0] );

		int nItemNumber = pCurrent->m_nFlags;
		int nItemsStart = *((int*)&pCurrent->m_pDatas[0]);
		f32* pItems = &m_pPositions[ nItemsStart*3 ];

		//no indication
		memset( pTempIndicator,0,sizeof(int8)*nTempIndicatorSize );
		nTempPos = 0;

		for( int i = 0; i < 8; ++i )
		{
				//create more octree cells
				if( m_nOctreeCellNumber == nOctreeCellPosition )
				{
					m_nOctreeCellNumber += 16;
					s3DSamplerOctreeCell* pNewList = new s3DSamplerOctreeCell[m_nOctreeCellNumber];
					if( NULL == pNewList )
						return false;
					memset( pNewList, 0, sizeof(s3DSamplerOctreeCell)*m_nOctreeCellNumber);
					if( m_pOctree )
					{
						memcpy( pNewList, m_pOctree, sizeof(s3DSamplerOctreeCell)*nOctreeCellPosition );
						delete [] m_pOctree;
					}
					m_pOctree = pNewList;
				}

				s3DSamplerOctreeCell* pChild = &m_pOctree[ nOctreeCellPosition ];

				//register where WILL be the data start
				*((int32*)&pChild->m_pDatas[0]) = (int32)(nItemsStart + nTempPos);

				int nStartPos = nTempPos;

				//check is there any items in that child
				for( int j = 0; j < nItemNumber; ++j )
					if( 0 == (pTempIndicator[j>>3] & (1 << (j & 0x7) ) ) && ChildAABB[i].IsContainPoint( Vec3(pItems[3*j+0],pItems[3*j+1],pItems[3*j+2] ) ) )
					{
						//not included to any other child.. + it's contain it..
						pTempIndicator[ j>> 3 ] |= 1 << ( j& 0x7);
						pTempPositions[ nTempPos*3+0 ] = pItems[3*j+0 ];
						pTempPositions[ nTempPos*3+1 ] = pItems[3*j+1 ];
						pTempPositions[ nTempPos*3+2 ] = pItems[3*j+2 ];
						++nTempPos;
					}

				//set the number of items here
				pChild->m_nFlags = nTempPos - nStartPos;

				//no items included give up this one..
				if( 0 == pChild->m_nFlags )
				{
					pCurrent->m_pDatas[i] = NULL;		//the no child for this space in the parent
					continue;
				}

				//put to the stack + validate as a real cell
				++nOctreeCellPosition;

				//include it as a child to the parent's list
				*((s3DSamplerOctreeCell**)&pCurrent->m_pDatas[i]) = pChild;

				//if it's not small enough include it to the stack too
				if( pChild->m_nFlags > 8 )
				{
					//grow the stack
					if( nStartPos == m_nSizeOfTheStack )
					{
						m_nSizeOfTheStack += 16;
						s3DSamplerOctreeCellStack* pNewStack = new s3DSamplerOctreeCellStack[m_nSizeOfTheStack];
						if( NULL == pNewStack )
							return false;
						memset( pNewStack, 0, sizeof(s3DSamplerOctreeCellStack)*m_nSizeOfTheStack);
						if( m_pStack )
						{
							memcpy( pNewStack, m_pStack, sizeof(s3DSamplerOctreeCellStack)*nStackPos );
							delete [] m_pStack;
						}
						m_pStack = pNewStack;
					}

					m_pStack[ nStackPos ].m_pCell = pChild;
					m_pStack[ nStackPos ].m_AABB = ChildAABB[i];
					++nStackPos;
				}
		}
		//copy back the new, sorted items
		if( nTempPos )
			memcpy( pItems, pTempPositions, nTempPos*3*sizeof(f32) );

		//this is a leaf
		pCurrent->m_nFlags = 0;
	};
	return true;
}
