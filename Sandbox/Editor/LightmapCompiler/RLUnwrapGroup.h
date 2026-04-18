/*=============================================================================
  RLUnwrapGroup.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __RLUNWRAPGROUP_H__
#define __RLUNWRAPGROUP_H__

#if _MSC_VER > 1000
# pragma once
#endif

#define			NUM_COMPONENTS 4 //should be matched to the Illum Handler value
#define			DILATE_SPACE	0
#define			DILATE_SPACE_POSTEFFECT	1
#define			DILATE_SPACE_POSTEFFECT_TEXSIZE	1024
//#define		ENABLE_HDD_CACHE			//save some memory, but slow down the process

#include <limits>
#include <vector>
#include <list>
#include "IllumHandler.h"
#include "GeomPrimitives.h"
#include "SceneContext.h"
#include "OneBitBitmapMask.h"
#include "SpanBuffer.h"

// The shading model for CRLUnwrapGroup instanse
typedef enum {
	SM_MATTE,
	SM_TRANSLUCENT,
	SM_CHROME,
	SM_GLASS,
	SM_WATER,
	SM_TRANSPARENT
} eShadingModel;

// The rotated versions of surface...
typedef enum {
	SD_NORMAL,
	SD_XFLIP,
	SD_MAX,

	SD_XYFLIP,
	SD_YFLIP,
} eSpanDirections;

struct TexUV
{
	f32 u,v;
};

class CRLUnwrapGroup
{
public:
	CRLUnwrapGroup();
	~CRLUnwrapGroup();

	void AddUVBound(TexUV& uv);
	void AddUVBound(f32 u, f32 v);
	void ResetUVBound();
	void AdjustUnitRatio(const CRLTriangle *pTri);
	void CalcUnitRatio( const IRenderShaderResources* pShaderResources );
	void ResetUnitRatio();
	bool CheckRasterSize( const float fLumenPerUnitU, const float fLumenPerUnitV, const int32 iMaxWidth, const int32 iMaxHeight ) const;
	bool SetRasterSize_NoMaps( const float fLumenPerUnitU, const float fLumenPerUnitV );
	bool SetRasterSize( const float fLumenPerUnitU, const float fLumenPerUnitV );
	bool ClearRaster();
	void SetComponent_DistributedMap( const int32 nIndex, Vec3* pPositions, Vec3* pNormals );
	void SetComponent_DynamicRange( const int32 iComponent, const int32 nIndex, Vec3 value );
	void SetComponent( const int32 iComponent, const int32 nIndex, Vec3 value );
	void SetComponent_Occlusion( const int32 iComponent, const int32 nIndex, Vec4 value );
	void SetComponent_RAE( const int32 nIndex, Vec4 value );
	void Dilate();
	void Pack();
	bool CheckUnwrapAxis(const int32 nAxis);
	void ReleaseMemory();
	bool AllocateMemory();
	void Blur();
	bool GenerateSpanBuffer();
	void CaculateFilePosition_Debug( int32 &nCacheFilePosition );
	bool Save( FILE *pCacheFile, int32 &nCacheFilePosition );
	bool Load( FILE *pCacheFile );
	bool LoadOnlySpanBuffers( FILE *pCacheFile );

	void SetComponent( const int32 nIndex, CIllumHandler& value )
	{
		CSceneContext& sceneCtx = CSceneContext::GetInstance();
		int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
		for(int32 iComp=0; iComp<nMaxComp; iComp++ )
		{
      SetComponent(iComp, nIndex, Vec3( value.m_vLightFlux[iComp].x,value.m_vLightFlux[iComp].y,value.m_vLightFlux[iComp].z) );
    }
	}

	void SetComponent_DynamicRange( const int32 nIndex, CIllumHandler& value )
	{
		CSceneContext& sceneCtx = CSceneContext::GetInstance();
		int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
		for(int32 iComp=0; iComp<nMaxComp; iComp++ )
		{
			SetComponent_DynamicRange(iComp, nIndex, Vec3( value.m_vLightFlux[iComp].x,value.m_vLightFlux[iComp].y,value.m_vLightFlux[iComp].z) );
		}
	}

	void SetComponent_Occlusion( const int32 nIndex, CIllumHandler& value )
	{
		CSceneContext& sceneCtx = CSceneContext::GetInstance();
		int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
		for(int32 iComp=0; iComp<nMaxComp; iComp++ )
		{
			SetComponent_Occlusion(iComp, nIndex, value.m_vLightFlux[iComp]);
		}
	}

	void SetValidPixel(  const int32 nIndex )
	{
		assert(m_pPixelMask);
		if( NULL == m_pPixelMask )
			return;
		//this is the "valid pixel" indicator...
		m_pPixelMask->SetValid( nIndex );
	}

	void CreateItemArray( int32 nSize )
	{
		arrTriangles = new CRLTriangle[ nSize ];
		m_nTriNumber = 0;
	}

	void ReleaseItemArray()
	{
//		arrItems.clear();
		SAFE_DELETE_ARRAY( arrTriangles );
	}

	void ReleaseSpanBuffers()
	{
	#ifdef ENABLE_HDD_CACHE
			for( int32 i=0; i<SD_MAX; ++i )
				SAFE_DELETE( m_pSpanBuffer[i] );
	#endif
	}

	void ClearBuffers()
	{
		CSceneContext& sceneCtx = CSceneContext::GetInstance();
		int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;

		for(int32  i=0; i<nMaxComp; i++ )
		{
			SAFE_DELETE_ARRAY(m_pOcclusionMap[i]);
			SAFE_DELETE_ARRAY(ppRastImage[i]);
		}
		SAFE_DELETE_ARRAY(m_pRAE );
		SAFE_DELETE_ARRAY(m_pDistributedMap);

		for( int32 i=0; i<SD_MAX; ++i )
			SAFE_DELETE( m_pSpanBuffer[i] );

		SAFE_DELETE( m_pPixelMask );
	}

	bool	IsNormalXYSizes( const eSpanDirections Direction ) const
	{
		switch( Direction )
		{
			default:
			case SD_NORMAL:
			case SD_XFLIP:
					return true;
			case SD_YFLIP:
			case SD_XYFLIP:
				return false;
		}
		return true;
	}

	int32	GenerateIndexFromCoordiates( const eSpanDirections Direction, const int32 nX, const int32 nY ) const
	{
		switch( Direction )
		{
			default:
			case SD_NORMAL:
				return nY*iWidth+nX;
			case SD_XFLIP:
				return (nY+1)*iWidth-(nX+1);
			case SD_XYFLIP:
				return nX*iHeight+nY;
			case SD_YFLIP:
				return (nX+1)*iHeight-(nY+1);
		}
		return nY*iWidth+nX;
	};

	int32	GenerateIndexFromCoordiatesToSurface( const eSpanDirections Direction, const int32 nX, const int32 nY, const int32 nWidth ) const
	{
		switch( Direction )
		{
		default:
		case SD_NORMAL:
			return nY*nWidth+nX;
		case SD_XFLIP:
			return (nY)*nWidth+iWidth-(nX+1);
		case SD_XYFLIP:
			return nX*nWidth+nY;
		case SD_YFLIP:
			return (nX)*nWidth+iHeight-(nY+1);
		}
		return nY*nWidth+nX;
	};

	void GenerateUVCoordinates( const f32 fU, const f32 fV, const int32 nOffsetX, const int32 nOffsetY, const int32 nSurfaceWidth, const int32 nSurfaceHeight, f32 &fNewU, f32 &fNewV ) const
	{
		f32 fU2 = fU * m_fWidth;
		f32 fV2 = fV * m_fHeight;

		switch( m_SpanDirection )
		{
			default:
			case SD_NORMAL:
				fNewU = fU2;
				fNewV = fV2;
				break;
			case SD_XFLIP:
				fNewU = m_fWidth-fU2;
				fNewV = fV2;
				break;
			case SD_XYFLIP:
				fNewU = fV2;
				fNewV = fU2;
				break;
			case SD_YFLIP:
				fNewU = m_fHeight-fV2;
				fNewV = fU2;
				break;
		}

		//offset
		fNewU += nOffsetX+DILATE_SPACE;
		fNewV += nOffsetY+DILATE_SPACE;
		//scale
		fNewU /= (f32)nSurfaceWidth;
		fNewV /= (f32)nSurfaceHeight;
  }

		void	AddVertex( const Vec3 &vVertex )
		{
			m_vBBoxMin.x = min( m_vBBoxMin.x,vVertex.x );
			m_vBBoxMin.y = min( m_vBBoxMin.y,vVertex.y );
			m_vBBoxMin.z = min( m_vBBoxMin.z,vVertex.z );

			m_vBBoxMax.x = max( m_vBBoxMax.x,vVertex.x );
			m_vBBoxMax.y = max( m_vBBoxMax.y,vVertex.y );
			m_vBBoxMax.z = max( m_vBBoxMax.z,vVertex.z );
		}

		void ResetBBox()
		{
			m_vBBoxMin.Set( std::numeric_limits<float>::max(),
											std::numeric_limits<float>::max(),
											std::numeric_limits<float>::max() );
			m_vBBoxMax.Set( -std::numeric_limits<float>::max(),
											-std::numeric_limits<float>::max(),
											-std::numeric_limits<float>::max() );
		}

		int32	CalculateTheActiveOcclusionChannelMask();												/// Calculate how many occlusion channels needed

		void	SetMaximumOcclusionChannelNumber( int32 nOcclusionChannelNumber )
		{
			m_nMaxOcclusionComponent = nOcclusionChannelNumber;
		}

		int32		AddMeshInfos( 	IRenderMesh* pRenderMesh, Matrix34* pMatrix, 	struct IRenderShaderResources* pMaterial  )
		{
			assert( pMatrix );
			SMeshInformations MeshInfo;

			MeshInfo.m_pRenderMesh = pRenderMesh;
			MeshInfo.m_mMatrix = *pMatrix;
			MeshInfo.m_pMaterial  = pMaterial;

			//mostly used datas
			MeshInfo.iVtxStride = 0;
			MeshInfo.pVertices = reinterpret_cast<Vec3 *> (pRenderMesh->GetStridedPosPtr(MeshInfo.iVtxStride));

			m_MeshInfos.push_back( MeshInfo );

			return m_MeshInfos.size() - 1;
		}

		inline const SMeshInformations *GetMeshInfo( int32 nId ) const
		{
			return &(m_MeshInfos[nId]);
		}

public:
	typedef	std::vector<SMeshInformations> t_vectMeshInfo;
	t_vectMeshInfo m_MeshInfos;

	CRLTriangle*				arrTriangles;
	int32								m_nTriNumber;
//	std::vector<uint32> arrItems;
	//world space aabbox
	Vec3								m_vBBoxMin;
	Vec3								m_vBBoxMax;
	uint8*							ppRastImage[NUM_COMPONENTS];
	uint8*							m_pOcclusionMap[NUM_COMPONENTS];
	uint8*							m_pRAE;
	uint8*							m_pDistributedMap;
	COneBitBitmapMask*	m_pPixelMask;
	CSpanBuffer*				m_pSpanBuffer[SD_MAX];													/// The span buffer for packing
	eSpanDirections			m_DefaultSpanDirectionOrder[SD_MAX];						/// default order, try to minimalize right side wasted space.
	eSpanDirections			m_SpanDirection;																/// which direction used for packing...
	int32								m_nMaxOcclusionComponent;												/// Which is the last used occlusion channel in the surface
	int32								m_nOctreeID;																		/// The ID of the octree - for distributed maps
	//resolution for rasterization 
	int32								iWidth;
	int32								iHeight;
	f64									m_dWorldArea;
	f64									m_dUVArea;
	double							m_dUnitPerU, m_dUnitPerV;
	TexUV								minUV, maxUV;																		/// Area for chunk
	int32								m_nUnwrapAxis;
	int32								m_nCacheFilePosition;														/// Unique position for this group;
	f32									m_fWidth,m_fHeight;															/// the size in float

	f32									m_fLumenPerUnitU;
	f32									m_fLumenPerUnitV;
	Vec4								m_LightCoordiates[16];
};

#endif