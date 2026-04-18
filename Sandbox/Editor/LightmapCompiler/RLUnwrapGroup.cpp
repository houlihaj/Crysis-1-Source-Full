/*=============================================================================
RLUnwrapGroup.cpp : 
Copyright 2004 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Nick Kasyan
=============================================================================*/
#include "StdAfx.h"
#include "RLUnwrapGroup.h"
#include "BitmapDilation.h"

//===================================================================================
// Method				:	CRLUnwrapGroup
// Description	:	Constructor
//===================================================================================
CRLUnwrapGroup::CRLUnwrapGroup()
{
	int32 i;

	for( i=0; i<NUM_COMPONENTS; i++ )
	{
		m_pOcclusionMap[i] = NULL;
		ppRastImage[i] = NULL;
	}
	m_pDistributedMap = NULL;
	m_pRAE = NULL;
	m_pPixelMask = NULL;

	m_nTriNumber = 0;
	arrTriangles = NULL;
	//	arrItems.clear();

	ResetUVBound();
	ResetUnitRatio();
	ResetBBox();

	m_nUnwrapAxis = -1;
	m_nCacheFilePosition = 0;

	for( i=0; i<SD_MAX; ++i )
		m_pSpanBuffer[i] = NULL;

	m_SpanDirection = SD_NORMAL;
	m_nMaxOcclusionComponent = 0;
}

//===================================================================================
// Method				:	~CRLUnwrapGroup
// Description	:	Destructor
//===================================================================================
CRLUnwrapGroup::~CRLUnwrapGroup()
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
	for(int32  i=0; i<nMaxComp; i++ )
	{
		SAFE_DELETE_ARRAY(m_pOcclusionMap[i]);
		SAFE_DELETE_ARRAY(ppRastImage[i]);
	}
	SAFE_DELETE_ARRAY(m_pDistributedMap);
	SAFE_DELETE_ARRAY(m_pRAE );
	SAFE_DELETE( m_pPixelMask );

	SAFE_DELETE_ARRAY( arrTriangles );
	//	arrItems.clear();
	for( int32 i=0; i<SD_MAX; ++i )
		SAFE_DELETE( m_pSpanBuffer[i] );
}


//===================================================================================
// Method				:	AddUVBound
// Description	:	Add an UV to bbox of the UVs
//===================================================================================
void CRLUnwrapGroup::AddUVBound(TexUV& uv)
{
	maxUV.u = max(maxUV.u, uv.u);
	maxUV.v = max(maxUV.v, uv.v);
	minUV.u = min(minUV.u, uv.u);
	minUV.v = min(minUV.v, uv.v);
}

//===================================================================================
// Method				:	AddUVBound
// Description	:	Add an UV to bbox of the UVs
//===================================================================================
void CRLUnwrapGroup::AddUVBound(f32 u, f32 v)
{
	assert(u< 4000 || u> -4000 );
	assert(v< 4000 || v> -4000 );

	maxUV.u = max(maxUV.u, u);
	maxUV.v = max(maxUV.v, v);
	minUV.u = min(minUV.u, u);
	minUV.v = min(minUV.v, v);
}

//===================================================================================
// Method				:	ResetUVBound
// Description	:	Reset the UV bbox
//===================================================================================
void CRLUnwrapGroup::ResetUVBound()
{
	minUV.u = std::numeric_limits<float>::max();
	minUV.v = std::numeric_limits<float>::max();
	maxUV.u = -std::numeric_limits<float>::max();
	maxUV.v = -std::numeric_limits<float>::max();
}

//===================================================================================
// Method				:	ResetUnitRatio
// Description	:	Reset the data of the unit ratio
//===================================================================================
void CRLUnwrapGroup::ResetUnitRatio()
{
	m_dUnitPerU = 0.0;
	m_dUnitPerV = 0.0;
	m_dWorldArea = 0.0;
	m_dUVArea = 0.0;
}

//===================================================================================
// Method				:	CheckRasterSize
// Description	:	Check it is possible a group with the LumenPerUnit can be part of an surface with iMaxWidth,iMaxHeight size
//===================================================================================
bool CRLUnwrapGroup::CheckRasterSize( const float fLumenPerUnitU, const float fLumenPerUnitV, const int32 iMaxWidth, const int32 iMaxHeight ) const
{
	int32 nWidth = (int32)ceil(m_dUnitPerU * fLumenPerUnitU) + DILATE_SPACE_POSTEFFECT*2;
	int32 nHeight = (int32)ceil(m_dUnitPerV * fLumenPerUnitV) + DILATE_SPACE_POSTEFFECT*2;

	return (nWidth <= iMaxWidth) && (nHeight <= iMaxHeight);
}

//===================================================================================
// Method				:	SetRasterSize_NoMap
// Description	:	Set the size of the group and NOT create the arrays
//===================================================================================
bool CRLUnwrapGroup::SetRasterSize_NoMaps( const float fLumenPerUnitU, const float fLumenPerUnitV )
{
	m_fLumenPerUnitU = fLumenPerUnitU;
	m_fLumenPerUnitV = fLumenPerUnitV;

	float fU = fLumenPerUnitU * m_dUnitPerU;
	float fV = fLumenPerUnitV * m_dUnitPerV;
	m_fWidth = fU;
	m_fHeight = fV;
	iWidth = (int32)ceil(fU) + DILATE_SPACE_POSTEFFECT*2;
	iHeight = (int32)ceil(fV) + DILATE_SPACE_POSTEFFECT*2;

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
	for(int32  i=0; i<nMaxComp; i++ )
	{
		SAFE_DELETE_ARRAY(m_pOcclusionMap[i]);
		SAFE_DELETE_ARRAY(ppRastImage[i]);
	}

	SAFE_DELETE_ARRAY(m_pRAE );
	SAFE_DELETE_ARRAY(m_pDistributedMap);

	SAFE_DELETE( m_pPixelMask );
	m_pPixelMask = new COneBitBitmapMask( iWidth, iHeight );
	if ( NULL == m_pPixelMask )
	{
		assert(0);
		return false;
	}
	return true;

}


//===================================================================================
// Method				:	SetRasterSize
// Description	:	Set the size of the group and create the arrays
//===================================================================================
bool CRLUnwrapGroup::SetRasterSize( const float fLumenPerUnitU, const float fLumenPerUnitV )
{
	m_fLumenPerUnitU = fLumenPerUnitU;
	m_fLumenPerUnitV = fLumenPerUnitV;

	float fU = fLumenPerUnitU * m_dUnitPerU;
	float fV = fLumenPerUnitV * m_dUnitPerV;
	m_fWidth = fU;
	m_fHeight = fV;
	iWidth = (int32)ceil(fU) + DILATE_SPACE_POSTEFFECT*2;
	iHeight = (int32)ceil(fV) + DILATE_SPACE_POSTEFFECT*2;

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
	for(int32  i=0; i<nMaxComp; i++ )
	{
		SAFE_DELETE_ARRAY(m_pOcclusionMap[i]);
		SAFE_DELETE_ARRAY(ppRastImage[i]);

		if( !sceneCtx.m_bDistributedMap )
		{
			if( sceneCtx.m_bMakeLightMap) 
			{
				ppRastImage[i] = new uint8[iWidth*iHeight*4];
				if ( NULL == ppRastImage[i] )
				{
					assert(0);
					return false;
				}
			}

			if( sceneCtx.m_bMakeOcclMap )
			{
				m_pOcclusionMap[i] = new uint8[iWidth*iHeight*4];
				if ( NULL == m_pOcclusionMap[i] )
				{
					assert(0);
					return false;
				}
			}
		}
	}

	SAFE_DELETE_ARRAY(m_pRAE );
	SAFE_DELETE_ARRAY(m_pDistributedMap);
	if( sceneCtx.m_bDistributedMap )
	{
		m_pDistributedMap = new uint8[iWidth*iHeight*sceneCtx.m_nDistributedBlockSize];
		if( NULL == m_pDistributedMap )
		{
			CLogFile::FormatLine("can't allocate %d byte (%dx%d) dmap\n", iWidth*iHeight*sceneCtx.m_nDistributedBlockSize, iWidth,iHeight );
			assert( NULL != m_pDistributedMap);
			return false;
		}
	}
	else
	{
		if( sceneCtx.m_bMakeRAE )
		{
			m_pRAE = new uint8[iWidth*iHeight*4];
			if ( NULL == m_pRAE )
			{
				CLogFile::FormatLine("can't allocate %d byte (%dx%d) rae\n", iWidth*iHeight*4, iWidth,iHeight );
				assert(0);
				return false;
			}
		}
	}

	SAFE_DELETE( m_pPixelMask );
	m_pPixelMask = new COneBitBitmapMask( iWidth, iHeight );
	if ( NULL == m_pPixelMask )
	{
		assert(0);
		return false;
	}
	return true;
}

//===================================================================================
// Method				:	ClearRaster
// Description	:	Clear all array that needed for rasterizer
//===================================================================================
bool CRLUnwrapGroup::ClearRaster()
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	assert( NULL != m_pPixelMask );
	if (m_pPixelMask != NULL)
		m_pPixelMask->Clear();
	else
		return false;

	if( sceneCtx.m_bDistributedMap )
	{
		//		assert( m_pDistributedMap!=NULL );

		if (m_pDistributedMap != NULL)
			memset (m_pDistributedMap, 0, sizeof (uint8)*iWidth*iHeight*sceneCtx.m_nDistributedBlockSize);
		//		else 
		//			return false;
	}
	else
	{
		int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
		for(int32 i=0; i<nMaxComp; i++ )
		{
			if( sceneCtx.m_bMakeLightMap )
			{
				//				assert( ppRastImage[i]!=NULL );

				if (ppRastImage[i] != NULL)
					memset (ppRastImage[i], 0, sizeof (uint8)*iWidth*iHeight*4);
				else 
					return false;
			}

			if( sceneCtx.m_bMakeOcclMap )
			{
				//			assert( m_pOcclusionMap[i]!=NULL );

				if (m_pOcclusionMap[i] != NULL)
					memset (m_pOcclusionMap[i], 0, sizeof (uint8)*iWidth*iHeight*4);
				else 
					return false;
			}
		}

		if( sceneCtx.m_bMakeRAE )
		{
			//			assert( m_pRAE!=NULL );

			if (m_pRAE != NULL)
				memset (m_pRAE, 0xff, sizeof (uint8)*iWidth*iHeight*4);
			else 
				return false;
		}
	}

	//ASSERT( _CrtCheckMemory() );
	return true;
}

//===================================================================================
// Method				:	SetComponent_DynamicRange
// Description	:	Set a component of the RastImage in dynamic range
//===================================================================================
void CRLUnwrapGroup::SetComponent_DynamicRange( const int32 iComponent, const int32 nIndex, Vec3 value )
{
	if( NULL ==	ppRastImage )
		return;
	assert(m_pPixelMask);
	if( NULL == m_pPixelMask )
		return;

#define	MAX_RANGE	2.f

	value.x = (value.x>MAX_RANGE) ? MAX_RANGE : ( value.x < 0.f ? 0.f : value.x );
	value.y = (value.y>MAX_RANGE) ? MAX_RANGE : ( value.y < 0.f ? 0.f : value.y );
	value.z = (value.z>MAX_RANGE) ? MAX_RANGE : ( value.z < 0.f ? 0.f : value.z );

	//you can use an other brightness function !
	//		f32 fBrightness = (value.x+value.y+value.z)/3.f;
	f32 fBrightness = max(value.x, max(value.y,value.z) );
	// we intrested in the 1..MAX_RANGE interval.
	fBrightness = fBrightness < 1.f ? 1.f : fBrightness;
	// Pack into our interval...
	if( fBrightness > 0.f )
		value /= fBrightness;

	//register the valid pixels
	//convert
#define PACKINTOBYTE_0TO1(x)		((uint8)((x)*255))
	//standart inte's order !
	ppRastImage[iComponent][nIndex*4+2] = PACKINTOBYTE_0TO1(value.x);
	ppRastImage[iComponent][nIndex*4+1] = PACKINTOBYTE_0TO1(value.y);
	ppRastImage[iComponent][nIndex*4+0] = PACKINTOBYTE_0TO1(value.z);
	ppRastImage[iComponent][nIndex*4+3] = PACKINTOBYTE_0TO1(fBrightness-1.f);

	//this is the "valid pixel" indicator...
	m_pPixelMask->SetValid( nIndex );
#undef PACKINTOBYTE_0TO1
	return;
}

//===================================================================================
// Method				:	SetComponent
// Description	:	Set a component of the RastImage
//===================================================================================
void  CRLUnwrapGroup::SetComponent( const int32 iComponent, const int32 nIndex, Vec3 value )
{
	if( NULL ==	ppRastImage )
		return;
	assert(m_pPixelMask);
	if( NULL == m_pPixelMask )
		return;

	value.x = (value.x>1) ? 1 : ( value.x < 0.f ? 0.f : value.x );
	value.y = (value.y>1) ? 1 : ( value.y < 0.f ? 0.f : value.y );
	value.z = (value.z>1) ? 1 : ( value.z < 0.f ? 0.f : value.z );

	//register the valid pixels
	//convert
#define PACKINTOBYTE_0TO1(x)		((uint8)((x)*255))
	//standart inte's order !
	ppRastImage[iComponent][nIndex*4+2] = PACKINTOBYTE_0TO1(value.x);
	ppRastImage[iComponent][nIndex*4+1] = PACKINTOBYTE_0TO1(value.y);
	ppRastImage[iComponent][nIndex*4+0] = PACKINTOBYTE_0TO1(value.z);
	ppRastImage[iComponent][nIndex*4+3] = 0;
	//this is the "valid pixel" indicator...
	m_pPixelMask->SetValid( nIndex );
#undef PACKINTOBYTE_0TO1
	return;
}

//===================================================================================
// Method				:	SetComponent_DistributedMap
// Description	:	Set a component of the DistributedMap
//===================================================================================
void CRLUnwrapGroup::SetComponent_DistributedMap( const int32 nIndex, Vec3* pPositions, Vec3* pNormals )
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	m_pPixelMask->SetValid( nIndex );

	uint8* pData = &m_pDistributedMap[nIndex*sceneCtx.m_nDistributedBlockSize];
	*pData = 1;		//this is a valid pixel
	pData++;
	float* pFloatData =  (float*)pData;
	int nSampleNum = (5+sceneCtx.m_numJitterSamples*sceneCtx.m_numJitterSamples);
	for( int i = 0; i < nSampleNum; ++i )
	{
		*pFloatData++ = pPositions[i].x;
		*pFloatData++ = pPositions[i].y;
		*pFloatData++ = pPositions[i].z;
		*pFloatData++ = pNormals[i].x;
		*pFloatData++ = pNormals[i].y;
		*pFloatData++ = pNormals[i].z;
	}


}

//===================================================================================
// Method				:	SetComponent_RAE
// Description	:	Set a component of the RAE
//===================================================================================
void CRLUnwrapGroup::SetComponent_RAE( const int32 nIndex, Vec4 value )
{
	if( NULL ==	m_pRAE )
		return;
	assert(m_pPixelMask);
	if( NULL == m_pPixelMask )
		return;

	value.x = (value.x>1) ? 1 : ( value.x < 0.f ? 0.f : value.x );
	value.y = (value.y>1) ? 1 : ( value.y < 0.f ? 0.f : value.y );
	value.z = (value.z>1) ? 1 : ( value.z < 0.f ? 0.f : value.z );
	value.w = (value.w>1) ? 1 : ( value.w < 0.f ? 0.f : value.w );

	//nonlinear scale
	//	value.x = sqrtf( value.x );
	//	value.y = sqrtf( value.y );
	//	value.z = sqrtf( value.z );
	//	value.w = sqrtf( value.w );
	//	value.x *= value.x;
	//	value.y *= value.y;
	//	value.z *= value.z;
	//	value.w *= value.w;

	//convert
#define PACKINTOBYTE_0TO1(x)		((uint8)((x)*255))
	m_pRAE[nIndex*4+0] = PACKINTOBYTE_0TO1(value.x);
	m_pRAE[nIndex*4+1] = PACKINTOBYTE_0TO1(value.y);
	m_pRAE[nIndex*4+2] = PACKINTOBYTE_0TO1(value.z);
	m_pRAE[nIndex*4+3] = PACKINTOBYTE_0TO1(value.w);
	//this is the "valid pixel" indicator...
	m_pPixelMask->SetValid( nIndex );
#undef PACKINTOBYTE_0TO1
	return;
}

//===================================================================================
// Method				:	SetComponent_Occlusion
// Description	:	Set a component of the Occlusion map
//===================================================================================
void CRLUnwrapGroup::SetComponent_Occlusion( const int32 iComponent, const int32 nIndex, Vec4 value )
{
	if( NULL ==	m_pOcclusionMap )
		return;
	assert(m_pPixelMask);
	if( NULL == m_pPixelMask )
		return;

	value.x = (value.x>1) ? 1 : ( value.x < 0.f ? 0.f : value.x );
	value.y = (value.y>1) ? 1 : ( value.y < 0.f ? 0.f : value.y );
	value.z = (value.z>1) ? 1 : ( value.z < 0.f ? 0.f : value.z );
	value.w = (value.w>1) ? 1 : ( value.w < 0.f ? 0.f : value.w );

	//convert
#define PACKINTOBYTE_0TO1(x)		((uint8)((x)*255))
	m_pOcclusionMap[iComponent][nIndex*4+0] = PACKINTOBYTE_0TO1(value.x);
	m_pOcclusionMap[iComponent][nIndex*4+1] = PACKINTOBYTE_0TO1(value.y);
	m_pOcclusionMap[iComponent][nIndex*4+2] = PACKINTOBYTE_0TO1(value.z);
	m_pOcclusionMap[iComponent][nIndex*4+3] = PACKINTOBYTE_0TO1(value.w);
	//this is the "valid pixel" indicator...
	m_pPixelMask->SetValid( nIndex );
#undef PACKINTOBYTE_0TO1
	return;
}

//===================================================================================
// Method				:	Dilate
// Description	:	post dilate
//===================================================================================
void CRLUnwrapGroup::Dilate()
{
	assert(m_pPixelMask);
	if( NULL == m_pPixelMask )
		return;

	if( NULL != m_pDistributedMap )
		return;

	if(!m_pRAE)
		return;

	uint vRAE[5];

	for(int y = 0; y < iHeight; ++y)
		for(int x = 0 ; x < iWidth; ++x)
			if( !m_pPixelMask->IsValid(x,y) )
			{
				vRAE[0] =
					vRAE[1] =
					vRAE[2] =
					vRAE[3] =
					vRAE[4] =	0;
				for(int yy = max(y-1,0); yy < min(y+2,iHeight); ++yy)
					for(int xx = max(x-1,0) ; xx < min(x+2,iWidth); ++xx)
					{
						if(m_pPixelMask->IsValid(xx,yy))
						{
							int nIndex = (yy*iWidth+xx)*4;
							vRAE[0] +=	m_pRAE[nIndex+0];
							vRAE[1] +=	m_pRAE[nIndex+1];
							vRAE[2] +=	m_pRAE[nIndex+2];
							vRAE[3] +=	m_pRAE[nIndex+3];
							vRAE[4]++;
						}
					}
					if(vRAE[4])
					{
						int nIndex = (y*iWidth+x)*4;
						m_pRAE[nIndex+0]	=	vRAE[0]/vRAE[4];
						m_pRAE[nIndex+1]	=	vRAE[1]/vRAE[4];
						m_pRAE[nIndex+2]	=	vRAE[2]/vRAE[4];
						m_pRAE[nIndex+3]	=	vRAE[3]/vRAE[4];
						m_pPixelMask->SetValid(x,y);
					}
			}
}

//===================================================================================
// Method				:	Pack
// Description	:	This function is try to pack the group into smaller space
//===================================================================================
void CRLUnwrapGroup::Pack()
{
	assert(m_pPixelMask);
	if( NULL == m_pPixelMask )
		return;

	//cant pack together the distributed map
	if( NULL != m_pDistributedMap )
		return;

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;

	//1. pass: Trivial, if all the valid pixels have the same color, use only 1 pixel...
	int32 x,y,nComp;
	uint8 vColor[NUM_COMPONENTS*4];
	uint8 vOcclusion[NUM_COMPONENTS*4];
	uint8 vRAE[4];

	memset( vColor, 0, sizeof(uint8)*NUM_COMPONENTS*4);
	memset( vOcclusion, 255, sizeof(uint8)*NUM_COMPONENTS*4);
	memset( vRAE, 0, sizeof(uint8)*4);

	bool bOnePixelSize = true;

	//search the first valid pixel - if not found, automaticaly generate 1 pixel size... (maybe an error ?)
	for( y = 0; y < iHeight; ++y)
	{
		for( x = 0; x < iWidth; ++x)
		{
			if( m_pPixelMask->IsValid(x,y) )
			{
				bOnePixelSize = false;
				break;
			}//isValid
		}//x
		if( false == bOnePixelSize )
			break;
	}//y


	//try to check the pixels..
	if( false == bOnePixelSize )
	{
		//get the first pixel for reference value...
		int nIndex = (y*iWidth+x)*4;

		if( m_pRAE )
		{
			vRAE[0] = m_pRAE[nIndex+0];
			vRAE[1] = m_pRAE[nIndex+1];
			vRAE[2] = m_pRAE[nIndex+2];
			vRAE[3] = m_pRAE[nIndex+3];
		}

		for( nComp = 0; nComp < nMaxComp; ++nComp )
		{
			if( ppRastImage[nComp] )
			{
				vColor[nComp*4+0] = ppRastImage[nComp][nIndex+0];
				vColor[nComp*4+1] = ppRastImage[nComp][nIndex+1];
				vColor[nComp*4+2] = ppRastImage[nComp][nIndex+2];
				vColor[nComp*4+3] = ppRastImage[nComp][nIndex+3];
			}
			if( m_pOcclusionMap[nComp] )
			{
				vOcclusion[nComp*4+0] = m_pOcclusionMap[nComp][nIndex+0];
				vOcclusion[nComp*4+1] = m_pOcclusionMap[nComp][nIndex+1];
				vOcclusion[nComp*4+2] = m_pOcclusionMap[nComp][nIndex+2];
				vOcclusion[nComp*4+3] = m_pOcclusionMap[nComp][nIndex+3];
			}
		}//nComp

		bOnePixelSize = true;
		//we have a good y position...
		for( y = 0; y < iHeight; ++y)
		{
			for( x = 0 ; x < iWidth; ++x)
			{
				if( m_pPixelMask->IsValid(x,y) )
				{
					int nIndex = (y*iWidth+x)*4;

					if( m_pRAE && (
						vRAE[0] != m_pRAE[nIndex+0] ||
						vRAE[1] != m_pRAE[nIndex+1] ||
						vRAE[2] != m_pRAE[nIndex+2] ||
						vRAE[3] != m_pRAE[nIndex+3] ) )
					{
						bOnePixelSize = false;
						break;
					}

					for( nComp = 0; nComp < nMaxComp; ++nComp )
					{
						if( ppRastImage[nComp] && (
							vColor[nComp*4+0] != ppRastImage[nComp][nIndex+0] ||
							vColor[nComp*4+1] != ppRastImage[nComp][nIndex+1] ||
							vColor[nComp*4+2] != ppRastImage[nComp][nIndex+2] ||
							vColor[nComp*4+3] != ppRastImage[nComp][nIndex+3] ) )
						{
							bOnePixelSize = false;
							break;
						}

						if( m_pOcclusionMap[nComp] && (
							vOcclusion[nComp*4+0] != m_pOcclusionMap[nComp][nIndex+0] ||
							vOcclusion[nComp*4+1] != m_pOcclusionMap[nComp][nIndex+1] ||
							vOcclusion[nComp*4+2] != m_pOcclusionMap[nComp][nIndex+2] ||
							vOcclusion[nComp*4+3] != m_pOcclusionMap[nComp][nIndex+3] ) )
						{
							bOnePixelSize = false;
							break;
						}
					}//nComp
				}//isvalid ?
			}//x
			if( false == bOnePixelSize )
				break;
		}//y
	}
	else 
		return;

	//I can generate a 1 pixel size map...
	if( bOnePixelSize )
	{
		m_fWidth = 1;
		m_fHeight = 1;
		iWidth  = 1+DILATE_SPACE_POSTEFFECT*2;
		iHeight = 1+DILATE_SPACE_POSTEFFECT*2;

		SAFE_DELETE( m_pPixelMask );
		m_pPixelMask = new COneBitBitmapMask( iWidth, iHeight);
		for( nComp = 0; nComp < nMaxComp; ++nComp )
		{
			if( ppRastImage[nComp] )
			{
				SAFE_DELETE_ARRAY( ppRastImage[nComp] );
				ppRastImage[nComp] = new uint8[4*iWidth*iHeight];
			}//if ppRast...

			if( m_pOcclusionMap[nComp] )
			{
				SAFE_DELETE_ARRAY( m_pOcclusionMap[nComp] );
				m_pOcclusionMap[nComp] = new uint8[4*iWidth*iHeight];
			}//if m_pOcclusionmap
		}//nComp

		if( m_pRAE )
		{
			SAFE_DELETE_ARRAY( m_pRAE );
			m_pRAE = new uint8[4*iWidth*iHeight];
		}//if m_pRAE

		for( int32 nIndex = 0; nIndex < iHeight*iWidth; ++nIndex )
		{

			m_pPixelMask->SetValid(nIndex);

			if( m_pRAE )
			{
				m_pRAE[nIndex*4+0] = vRAE[0];
				m_pRAE[nIndex*4+1] = vRAE[1];
				m_pRAE[nIndex*4+2] = vRAE[2];
				m_pRAE[nIndex*4+3] = vRAE[3];
			}//if m_pRAE

			for( nComp = 0; nComp < nMaxComp; ++nComp )
			{
				if( ppRastImage[nComp] )
				{
					ppRastImage[nComp][nIndex*4+0] = vColor[nComp*4+0];
					ppRastImage[nComp][nIndex*4+1] = vColor[nComp*4+1];
					ppRastImage[nComp][nIndex*4+2] = vColor[nComp*4+2];
					ppRastImage[nComp][nIndex*4+3] = vColor[nComp*4+3];
				}//if ppRast...

				if( m_pOcclusionMap[nComp] )
				{
					m_pOcclusionMap[nComp][nIndex*4+0] = vOcclusion[nComp*4+0];
					m_pOcclusionMap[nComp][nIndex*4+1] = vOcclusion[nComp*4+1];
					m_pOcclusionMap[nComp][nIndex*4+2] = vOcclusion[nComp*4+2];
					m_pOcclusionMap[nComp][nIndex*4+3] = vOcclusion[nComp*4+3];
				}//if m_pOcclusionmap
			}//nComp
		}

		//a smaller group generated...
		return;
	}//bOnePixelSize

	//2. pass: Gradient based calculation....
	//fix: implement this !
}


//===================================================================================
// Method				:	CheckUnwrapAxis
// Description	:	Check is this unwrap axis are the same as the first one (used for voxels)
//===================================================================================
bool CRLUnwrapGroup::CheckUnwrapAxis(const int32 nAxis)
{
	if (m_nUnwrapAxis < 0)
	{
		m_nUnwrapAxis = nAxis;
		return true;
	}
	else 
		if (m_nUnwrapAxis == nAxis)
			return true;
	return false;
}

//===================================================================================
// Method				:	ReleaseMemory
// Description	:	Release all used big memory array if HDD cacheing enabled
//===================================================================================
void CRLUnwrapGroup::ReleaseMemory()
{
#ifdef ENABLE_HDD_CACHE
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	int32 i;
	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
	for( i = 0; i < nMaxComp; i++ )
	{
		SAFE_DELETE_ARRAY(m_pOcclusionMap[i]);
		SAFE_DELETE_ARRAY(ppRastImage[i]);
	}
	SAFE_DELETE_ARRAY( m_pDistributedMap );
	SAFE_DELETE_ARRAY( m_pRAE );
	SAFE_DELETE_ARRAY( m_pPixelMask );
	ReleaseSpanBuffers();
#endif
}

//===================================================================================
// Method				:	AllocateMemory
// Description	:	Allocate all used big memory array if HDD cacheing enabled
//===================================================================================
bool CRLUnwrapGroup::AllocateMemory()
{
#ifdef ENABLE_HDD_CACHE
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
	for(int32 i=0; i<nMaxComp; i++ )
	{
		if( sceneCtx.m_bMakeLightMap)
		{
			ppRastImage[i] = new uint8[iWidth*iHeight*4];
			if ( NULL == ppRastImage[i] )
				return false;
		}

		if( sceneCtx.m_bMakeOcclMap)
		{
			m_pOcclusionMap[i] = new uint8[iWidth*iHeight*4];
			if ( NULL == m_pOcclusionMap[i] )
				return false;
		}
	}

	m_pDistributedMap

		if( sceneCtx.m_bMakeRAE )
		{
			m_pRAE = new uint8[iWidth*iHeight*4];
			if ( NULL == m_pRAE )
				return false;
		}

		m_pPixelMask = new COneBitBitmapMask( iWidth, iHeight );
		if ( NULL == m_pPixelMask )
			return false;
		for( i = 0; i < SD_MAX; i++ )
		{
			if( IsNormalXYSizes( (eSpanDirections)i ) )
				m_pSpanBuffer[i] = new CSpanBuffer( iWidth, iHeight );
			else
				m_pSpanBuffer[i] = new CSpanBuffer( iHeight, iWidth );
			if ( NULL == m_pSpanBuffer[i] )
				return false;
		}
#endif
		return true;
}

//===================================================================================
// Method				:	AdjustUnitRatio
// Description	:	Adjusting per triangle the unit ratio
//===================================================================================
void CRLUnwrapGroup::AdjustUnitRatio(const CRLTriangle *pTri)
{
	AddUVBound(pTri->m_vVert[0].u, pTri->m_vVert[0].v);
	AddUVBound(pTri->m_vVert[1].u, pTri->m_vVert[1].v);
	AddUVBound(pTri->m_vVert[2].u, pTri->m_vVert[2].v);

	Vec3 vPos[3];
	pTri->GetPositions( vPos, GetMeshInfo(pTri->m_nMeshInfo) );
	//////////////////////////////////////////////////////////////////////////
	//source triangle
	Vec3 v1 = vPos[2] - vPos[0];
	Vec3 v2 = vPos[1] - vPos[0];
	f32 fWorldArea = (v1.Cross(v2)).GetLength() * 0.5f;
	//accumulate world area
	m_dWorldArea += fWorldArea;

	//accumulate uv area
	v1.x = pTri->m_vVert[2].u - pTri->m_vVert[0].u;
	v1.y = pTri->m_vVert[2].v - pTri->m_vVert[0].v;
	v1.z = 0;

	v2.x = pTri->m_vVert[1].u - pTri->m_vVert[0].u;
	v2.y = pTri->m_vVert[1].v - pTri->m_vVert[0].v;
	v2.z = 0;

	f32 fUVArea = (v1.Cross(v2)).GetLength() * 0.5f;
	m_dUVArea += fUVArea;
}

//===================================================================================
// Method				:	CalcUnitRatio
// Description	:	Calculate the unit ration for the group
//===================================================================================
void CRLUnwrapGroup::CalcUnitRatio( const IRenderShaderResources* pShaderResources )
{ 
	//"hole fixer" if you have a not fully covered area it try to fix it...
	f64 fU = maxUV.u - minUV.u;
	f64 fV = maxUV.v - minUV.v;
	f64 fUVScaler;

	if( m_dUVArea > 10e-6 )
		fUVScaler = sqrt( (fU*fV) / m_dUVArea );
	else
	{
		//special case: the graphics artist are useing zero texture space...
		//this isn't a good solution, but better than nothing...
		fU = 1;
		fV = 1;
		fUVScaler = 1;
	}

	f64	fSize = sqrt(m_dWorldArea);

	m_dUnitPerU = fSize * fUVScaler;
	m_dUnitPerV = fSize * fUVScaler;

	//Calculate the texture aspect ratio of diffuse texture.
	Vec2 TextureAspectRatio(1,1);

	if( pShaderResources )
	{
		SEfResTexture *pDiffuseTexture = pShaderResources->GetTexture(EFTT_DIFFUSE);

		if( pDiffuseTexture && pDiffuseTexture->m_Sampler.m_pITex )
		{
			int iWidth = pDiffuseTexture->m_Sampler.m_pITex->GetWidth();
			int iHeight = pDiffuseTexture->m_Sampler.m_pITex->GetHeight();

			if( 0 < iWidth && 0 < iHeight )
			{
				if(iWidth > iHeight )
					TextureAspectRatio.x = (float)iWidth / (float)iHeight;
				else
					TextureAspectRatio.y = (float)iHeight / (float)iWidth;
			}
		}
	}

	f32 fScaledDeltaU = fU * TextureAspectRatio.x;
	f32 fScaledDeltaV = fV * TextureAspectRatio.y;

	//notes: we need to limit the scale, because there are some "wrong" assest all the time....

	//sanity check: at least 1 pixel wide in 2048...
	const f32 Border	=	1.f/float(DILATE_SPACE_POSTEFFECT_TEXSIZE);
	fScaledDeltaU = ( fScaledDeltaU < Border ) ? Border : fScaledDeltaU;
	fScaledDeltaV = ( fScaledDeltaV < Border ) ? Border : fScaledDeltaV;

	//maximum scale ration - need a limitation
	#define UNWRAPGROUP_SCALE_MAXIMUM_LIMIT		100.f

	//rescale the UV units
	f32 fScale;
	if( fScaledDeltaU > fScaledDeltaV )
	{
		fScale = fScaledDeltaU / fScaledDeltaV;
		fScale = fScale > UNWRAPGROUP_SCALE_MAXIMUM_LIMIT ? UNWRAPGROUP_SCALE_MAXIMUM_LIMIT : fScale;
		m_dUnitPerU *= fScale;
	}
	else
	{
		fScale = fScaledDeltaV / fScaledDeltaU;
		fScale = fScale > UNWRAPGROUP_SCALE_MAXIMUM_LIMIT ? UNWRAPGROUP_SCALE_MAXIMUM_LIMIT : fScale;
		m_dUnitPerV *= fScale;
	}

	//resize to use the same area, not bigger.
	f32 fScaleSqrt = sqrt(fScale);
	m_dUnitPerU /= fScaleSqrt;
	m_dUnitPerV /= fScaleSqrt;
}

//===================================================================================
// Method				:	CalculateFilePosition_Debug
// Description	:	Calculate the file position of the group for debugging
//===================================================================================
void CRLUnwrapGroup::CaculateFilePosition_Debug( int32 &nCacheFilePosition )
{
	m_nCacheFilePosition = nCacheFilePosition;

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
	for(int32  i=0; i<nMaxComp; i++ )
	{
		if( sceneCtx.m_bMakeLightMap )
			nCacheFilePosition += iWidth*iHeight*4*sizeof(uint8);
		if( sceneCtx.m_bMakeOcclMap)
			nCacheFilePosition += iWidth*iHeight*4*sizeof(uint8);
	}

	if( sceneCtx.m_bMakeRAE)
		nCacheFilePosition += iWidth*iHeight*4*sizeof(uint8);

	nCacheFilePosition += m_pPixelMask->m_nMaskSize;
}

//===================================================================================
// Method				:	Save
// Description	:	Save to HDD the group informations
//===================================================================================
bool CRLUnwrapGroup::Save( FILE *pCacheFile, int32 &nCacheFilePosition )
{
#ifdef ENABLE_HDD_CACHE
	if ( !pCacheFile )
				return false;

	//register the position, and update
	m_nCacheFilePosition = nCacheFilePosition;

	int32  i;

	//start with the span buffer infos
	for(i = 0; i < SD_MAX; ++i )
		nCacheFilePosition += m_pSpanBuffer[i]->SaveToFile( pCacheFile );

	//write the datas...
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
	for( i = 0; i<nMaxComp; i++ )
	{
		if( sceneCtx.m_bMakeLightMap )
		{
			fwrite( ppRastImage[i], 1,iWidth*iHeight*4*sizeof(uint8), pCacheFile );
			nCacheFilePosition += iWidth*iHeight*4*sizeof(uint8);
		}
		if( sceneCtx.m_bMakeOcclMap)
		{
			fwrite( m_pOcclusionMap[i], 1,iWidth*iHeight*4*sizeof(uint8), pCacheFile );
			nCacheFilePosition += iWidth*iHeight*4*sizeof(uint8);
		}
	}

	m_pDistributedMap

	if( sceneCtx.m_bMakeRAE)
	{
		fwrite( m_pRAE, 1,iWidth*iHeight*4*sizeof(uint8), pCacheFile );
		nCacheFilePosition += iWidth*iHeight*4*sizeof(uint8);
	}

	fwrite( m_pPixelMask->m_pBitMask,1,m_pPixelMask->m_nMaskSize, pCacheFile );
	nCacheFilePosition += m_pPixelMask->m_nMaskSize;

#endif
	return true;
}

//===================================================================================
// Method				:	LoadOnlySpanBuffer
// Description	:	Load back the span buffer datas from HDD
//===================================================================================
bool CRLUnwrapGroup::LoadOnlySpanBuffers( FILE *pCacheFile )
{
#ifdef ENABLE_HDD_CACHE
	if ( NULL == pCacheFile )
		return false;

	//seek to my datas...
	fseek( pCacheFile, m_nCacheFilePosition, SEEK_SET);

	int32 i;
	for( i = 0; i < SD_MAX; i++ )
	{
		//release
		SAFE_DELETE( m_pSpanBuffer[i] );
		//reallocate
		if( IsNormalXYSizes( (eSpanDirections)i ) )
			m_pSpanBuffer[i] = new CSpanBuffer( iWidth, iHeight );
		else
			m_pSpanBuffer[i] = new CSpanBuffer( iHeight, iWidth );
		if ( NULL == m_pSpanBuffer[i] )
			return false;

		//load
		m_pSpanBuffer[i]->LoadFromFile( pCacheFile );
	}

#endif
	return true;
}


//===================================================================================
// Method				:	Load
// Description	:	Load back all data of the group from HDD
//===================================================================================
bool CRLUnwrapGroup::Load( FILE *pCacheFile )
{
#ifdef ENABLE_HDD_CACHE
	if ( NULL == pCacheFile )
					return false;

	//rescale the memory allocations...
	ReleaseMemory();
	if( false == AllocateMemory() )
							return false;
	//seek to my datas...
	fseek( pCacheFile, m_nCacheFilePosition, SEEK_SET);

	int32  i;

	//read the datas...
	for( i = 0; i < SD_MAX; i++ )
		m_pSpanBuffer[i]->LoadFromFile_Dummy( pCacheFile );

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	for( i = 0; i < nMaxComp; i++ )
	{
		if( sceneCtx.m_bMakeLightMap )
			fread( ppRastImage[i], 1,iWidth*iHeight*4*sizeof(uint8), pCacheFile );
		if( sceneCtx.m_bMakeOcclMap)
			fread( m_pOcclusionMap[i], 1,iWidth*iHeight*4*sizeof(uint8), pCacheFile );
	}

	m_pDistributedMap

	if( sceneCtx.m_bMakeRAE)
		fread( m_pRAE, 1,iWidth*iHeight*4*sizeof(uint8), pCacheFile );

	fread( m_pPixelMask->m_pBitMask,1,m_pPixelMask->m_nMaskSize, pCacheFile );
#endif
	return true;
}
//===================================================================================
// Method				:	Blur
// Description	:	Blur the surface
//===================================================================================
void CRLUnwrapGroup::Blur()
{
	if( NULL == m_pRAE )
		return;

	int32 Y,X,nIndex;
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;

	//Radius can't be bigger than 9 !
	// CBitmapDilation's spiral system is used 
	CBitmapDilation DilationTool(1,1);
	uint32 dwNoEnd = DilationTool.ComputeEndNoAtCertainDistance(1);

//	uint32	vOcclusion[NUM_COMPONENTS*4];
	uint32	vRAE[4];

	//generate the valid pixel mask
	nIndex = 0;
	for( Y = 0; Y < iHeight; ++Y )
	{
		for( X = 0; X < iWidth; ++X, ++nIndex )
		{
			bool bFoundAValid = false;
			//it is a valid pixel ?
//			if( m_pPixelMask->IsValid( nIndex ) )
			{
				float fSampleWeight = 0;
	//			memset( vOcclusion, 0, sizeof(uint32)*NUM_COMPONENTS*4);
				memset( vRAE, 0, sizeof(uint32)*4);
				//get the pixels around
				DWORD dwSampleNo = 0;
				int32 iLocalX,iLocalY;
				for(uint32 dwI=0;dwI<dwNoEnd;++dwI)
				{
					bool bOk = DilationTool.GetSpiralPoint(dwSampleNo++,iLocalX,iLocalY);		assert(bOk);

					if( m_pPixelMask->IsValid( X+iLocalX, Y+iLocalY ) )
					{
						int nIndex4 = ((Y+iLocalY)*iWidth+X+iLocalX)*4;
/*
						for( int nComp = 0; nComp < nMaxComp; ++nComp )
						{
							if( m_pOcclusionMap[nComp] )
							{
								vOcclusion[nComp*4+0] += m_pOcclusionMap[nComp][nIndex4+0];
								vOcclusion[nComp*4+1] += m_pOcclusionMap[nComp][nIndex4+1];
								vOcclusion[nComp*4+2] += m_pOcclusionMap[nComp][nIndex4+2];
								vOcclusion[nComp*4+3] += m_pOcclusionMap[nComp][nIndex4+3];
							}
						}//nComp
*/
						if( m_pRAE )
						{
							vRAE[0] += m_pRAE[nIndex4+0];
							vRAE[1] += m_pRAE[nIndex4+1];
							vRAE[2] += m_pRAE[nIndex4+2];
							vRAE[3] += m_pRAE[nIndex4+3];
						}
						bFoundAValid = true;

						fSampleWeight += 1.f;
					}
				}//spiral

				if( fSampleWeight > 0 )
					fSampleWeight = 1.f / fSampleWeight;
				else
					fSampleWeight = 0;

				int nIndex4 = ((Y)*iWidth+X)*4;
/*
				for( int nComp = 0; nComp < nMaxComp; ++nComp )
				{
					if( m_pOcclusionMap[nComp] )
					{
						m_pOcclusionMap[nComp][nIndex4+0] = (uint8)( vOcclusion[nComp*4+0] * fSampleWeight );
						m_pOcclusionMap[nComp][nIndex4+1] = (uint8)( vOcclusion[nComp*4+1] * fSampleWeight );
						m_pOcclusionMap[nComp][nIndex4+2] = (uint8)( vOcclusion[nComp*4+2] * fSampleWeight );
						m_pOcclusionMap[nComp][nIndex4+3] = (uint8)( vOcclusion[nComp*4+3] * fSampleWeight );
					}
				}//nComp
*/
				if( bFoundAValid )
				{
					if( m_pRAE )
					{
						m_pRAE[nIndex4+0] = (uint8)( vRAE[0] * fSampleWeight );
						m_pRAE[nIndex4+1] = (uint8)( vRAE[1] * fSampleWeight );
						m_pRAE[nIndex4+2] = (uint8)( vRAE[2] * fSampleWeight );
						m_pRAE[nIndex4+3] = (uint8)( vRAE[3] * fSampleWeight );
					}
					m_pPixelMask->SetValid( nIndex );
				}
			}//isvalid ?
		}//X
	}//Y
}

//===================================================================================
// Method				:	GenerateSpanBuffer
// Description	:	Generate the span buffers of the group
//===================================================================================
bool CRLUnwrapGroup::GenerateSpanBuffer()
{
	int32 Y,X,nIndex;
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	for( int32 i=0; i<SD_MAX; i++ )
	{
		SAFE_DELETE( m_pSpanBuffer[i] );

		//clear...
		COneBitBitmapMask *pValidPixels;

		if( IsNormalXYSizes( (eSpanDirections)i ) )
		{
			m_pSpanBuffer[i] = new CSpanBuffer( iWidth, iHeight );
			pValidPixels = new COneBitBitmapMask( iWidth, iHeight );
		}
		else
		{
			m_pSpanBuffer[i] = new CSpanBuffer( iHeight, iWidth );
			pValidPixels = new COneBitBitmapMask( iHeight, iWidth );
		}

		//memory allocation checks
		assert( NULL != m_pSpanBuffer[i] );
		if( NULL == m_pSpanBuffer[i] )
		{
			for( int32 j = 0; j < i; ++j )
				SAFE_DELETE( m_pSpanBuffer[j] );
			SAFE_DELETE( pValidPixels );
			return false;
		}
		assert( NULL != pValidPixels );
		if( NULL == pValidPixels )
		{
			for( int32 j = 0; j <= i; ++j )
				SAFE_DELETE( m_pSpanBuffer[j] );
			return false;
		}


		//generate the valid pixel mask
		nIndex = 0;
		for( Y = 0; Y < iHeight; ++Y )
		{
			for( X = 0; X < iWidth; ++X )
			{
				if( m_pPixelMask->IsValid( nIndex ) )
					pValidPixels->SetValid( GenerateIndexFromCoordiates( (eSpanDirections)i, X, Y ) );
				++nIndex;
			}
		}

		//generate the span buffer
		m_pSpanBuffer[i]->ConvertOneBitBitmapMaskToSpanBuffer( *pValidPixels );

		SAFE_DELETE( pValidPixels );
	}

	if( iWidth < iHeight )
	{
		int i = 0;
		if( SD_NORMAL < SD_MAX )
				m_DefaultSpanDirectionOrder[i++] = SD_NORMAL;
		if( SD_XFLIP < SD_MAX )
			m_DefaultSpanDirectionOrder[i++] = SD_XFLIP;
		if( SD_XYFLIP < SD_MAX )
			m_DefaultSpanDirectionOrder[i++] = SD_XYFLIP;
		if( SD_YFLIP < SD_MAX )
			m_DefaultSpanDirectionOrder[i++] = SD_YFLIP;
	}
	else
	{
		int i = 0;
		if( SD_XYFLIP < SD_MAX )
			m_DefaultSpanDirectionOrder[i++] = SD_XYFLIP;
		if( SD_YFLIP < SD_MAX )
			m_DefaultSpanDirectionOrder[i++] = SD_YFLIP;
		if( SD_NORMAL < SD_MAX )
			m_DefaultSpanDirectionOrder[i++] = SD_NORMAL;
		if( SD_XFLIP < SD_MAX )
			m_DefaultSpanDirectionOrder[i++] = SD_XFLIP;
	}
	return true;
}


//===================================================================================
// Method				:	CaculateTheNeededOcclusionChannels
// Description	:	Calculate which occlusion channels used
//===================================================================================
int32	CRLUnwrapGroup::CalculateTheActiveOcclusionChannelMask()												/// Calculate which occlusion channels used
{
	int32 Mask = 0;
#if 1
	//For test, a simpler one
	for( int i = 0; i < m_nMaxOcclusionComponent; ++i )
		Mask |= 1<<i;
#else
	//This is a full dynamic version

	for( int y = 0; y < iHeight; ++y)
	{
		for( int x = 0; x < iWidth; ++x)
		{
			if( m_pPixelMask->IsValid(x,y) )
			{
				int nIndex = (y*iWidth+x)*4;
				for( int nComp = 0; nComp < NUM_COMPONENTS; ++nComp )
				{
					if( m_pOcclusionMap[nComp] )
					{
						if( m_pOcclusionMap[nComp][ nIndex + 0 ] != 255 )
							Mask |= 1<<(nComp*4+0);
						if( m_pOcclusionMap[nComp][ nIndex + 1 ] != 255 )
							Mask |= 1<<(nComp*4+1);
						if( m_pOcclusionMap[nComp][ nIndex + 2 ] != 255 )
							Mask |= 1<<(nComp*4+2);
						if( m_pOcclusionMap[nComp][ nIndex + 3 ] != 255 )
							Mask |= 1<<(nComp*4+3);
					}//if Occlusionmap
				}//nComp
			}//isValid
		}//x
	}//y
#endif

	return Mask;
}
