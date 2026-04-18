/*=============================================================================
	RLMesh.cpp : 
	Copyright 2004 Crytek Studios. All Rights Reserved.

	Revision history:
	* Created by Nick Kasyan
=============================================================================*/
#include "stdafx.h"

#include "IStatObj.h"
#include "I3DEngine.h"
#include <CryArray.h>

#include "RLMesh.h"
#include "SimpleTriangleRasterizer.h"

bool CRLMesh::m_bSamplesCache = false;
int CRLMesh::m_numOffsetSamples = 0;
Vec2* CRLMesh::m_pvOffsetSamples = NULL;

//#define _DEBUG_RASTER_NORMALS

//===================================================================================
// CDebugColorFill
// Description	:	A debug color rasterizer callback class
//===================================================================================
class CDebugColorFill: public CSimpleTriangleRasterizer::IRasterizeSink
{
public:

	//! constructor
	CDebugColorFill( CRLUnwrapGroup	*pGroup, const Vec3& vDebugColor )
	{
		m_vDebugColor = vDebugColor;
		m_pGroup = pGroup;
	}

	virtual void Triangle( const int iniY )
	{
	}

	virtual void Line( const float infXLeft, const float infXRight,
		const int iniLeft, const int iniRight, const int iniY )
	{
		Vec3 vWhite(1,1,1);
		Vec4 vBlack4(0.25f,0.25f,0.25f,0.25f);
		Vec4 vWhite4(1,1,1,1);
		CSceneContext& sceneCtx = CSceneContext::GetInstance();
		int nIndex = iniY*m_pGroup->iWidth + iniLeft;

		int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;

		for(int x=iniLeft;x<iniRight;x++)
		{
			//checker texture
			if( (x % 2) != (iniY%2) )
			{
				if( sceneCtx.m_bMakeRAE )
						m_pGroup->SetComponent_RAE( nIndex, vWhite4 ); 

				if( sceneCtx.m_bMakeOcclMap )
					for( int32 i = 0; i < nMaxComp; ++i )
						m_pGroup->SetComponent_Occlusion(i ,nIndex, vWhite4 ); 

				if( sceneCtx.m_bMakeLightMap )
					for( int32 i = 0; i < nMaxComp; ++i )
						m_pGroup->SetComponent(i ,nIndex, m_vDebugColor ); 
			}
			else
			{
				if( sceneCtx.m_bMakeRAE )
					m_pGroup->SetComponent_RAE( nIndex, vBlack4 ); 

				if( sceneCtx.m_bMakeOcclMap )
					for( int32 i = 0; i < nMaxComp; ++i )
						m_pGroup->SetComponent_Occlusion(i ,nIndex, vBlack4 ); 

				if( sceneCtx.m_bMakeLightMap )
					for( int32 i = 0; i < nMaxComp; ++i )
						m_pGroup->SetComponent(i ,nIndex, vWhite ); 
			}

			++nIndex;
		}
	}

private:
	CRLUnwrapGroup	*m_pGroup;
	Vec3						m_vDebugColor;
};

//===================================================================================
// CValidPixelFill
// Description	:	A valid pixel rasterizer callback class
//===================================================================================
class CValidPixelFill: public CSimpleTriangleRasterizer::IRasterizeSink
{
public:

	//! constructor
	CValidPixelFill( CRLUnwrapGroup	*pGroup )
	{
		m_pGroup = pGroup;
	}

	virtual void Triangle( const int iniY )
	{
	}

	virtual void Line( const float infXLeft, const float infXRight,
		const int iniLeft, const int iniRight, const int iniY )
	{
		int32 nIndex = iniY*m_pGroup->iWidth + iniLeft;

		for(int x=iniLeft;x<iniRight;x++)
		{
			m_pGroup->SetValidPixel( nIndex ); 
			++nIndex;
		}
	}
private:
	CRLUnwrapGroup*					m_pGroup;
};



//===================================================================================
// CLightmapFill
// Description	:	A lightmap / occlusion map rasterizer callback class
//===================================================================================
class CLightmapFill: public CSimpleTriangleRasterizer::IRasterizeSink
{
public:

	//! constructor
	CLightmapFill( CRLUnwrapGroup	*pGroup,CIlluminanceIntegrator* pDirectIntegrator, CIlluminanceIntegrator* pIndirectIntegrator,
		CIlluminanceIntegrator* pSunIntegrator, CIlluminanceIntegrator* pOcclusionIntegrator, CIlluminanceIntegrator* pAmbientOcclusionIntegrator, t_pairEntityId* LightmapLightIDs, t_pairEntityId* OcclusionLightIDs, int32 nOcclLightNum, CIntersectionCacheCluster* pIntersectionCache )
	{
		m_pGroup = pGroup;

		m_pDirectIntegrator = pDirectIntegrator;
		m_pIndirectIntegrator = pIndirectIntegrator;
		m_pSunIntegrator = pSunIntegrator;
		m_pOcclusionIntegrator = nOcclLightNum ? pOcclusionIntegrator : NULL;
		m_pAmbientOcclusionIntegrator = pAmbientOcclusionIntegrator;
		m_LightmapLightIDs = LightmapLightIDs;
		m_OcclusionLightIDs = OcclusionLightIDs;
		m_nOcclLightNum = nOcclLightNum;
		m_pIntersectionCache = pIntersectionCache;

		//need to run the callcoffset before that...
		assert(CRLMesh::m_numOffsetSamples>=1);
		m_fOnePerOffsetSamples = 1.0f/CRLMesh::m_numOffsetSamples;

		m_pSceneCtx = &CSceneContext::GetInstance();
		nMaxComp = (m_pSceneCtx->m_numComponents+3)/4;

		m_pGeomCore =	m_pSceneCtx->GetGeomCore();
//		assert(m_pGeomCore!=NULL);
	}

	bool SetupTriangle( CRLTriangle Tri, float x[3], float y[3], COctreeCell* pBoundingCell )
	{
		m_Tri = Tri;
		m_fX[0] = x[0] + DILATE_SPACE;
		m_fX[1] = x[1] + DILATE_SPACE;
		m_fX[2] = x[2] + DILATE_SPACE;

		m_fY[0] = y[0] + DILATE_SPACE;
		m_fY[1] = y[1] + DILATE_SPACE;
		m_fY[2] = y[2] + DILATE_SPACE;

		double fB0 = ((m_fX[1] - m_fX[0]) * (m_fY[2] - m_fY[0]) - (m_fX[2] - m_fX[0]) * (m_fY[1] - m_fY[0])); 

		if( fabs( fB0 ) > 10e-6 )
				m_fOnePerB0 = 1.f / fB0;
		else
				return false;			//Degenerated triangle

		//create edges
		m_fEdgesX[0] = m_fX[1] - m_fX[0];
		m_fEdgesX[1] = m_fX[2] - m_fX[1];
		m_fEdgesX[2] = m_fX[0] - m_fX[2];

		m_fEdgesY[0] = m_fY[1] - m_fY[0];
		m_fEdgesY[1] = m_fY[2] - m_fY[1];
		m_fEdgesY[2] = m_fY[0] - m_fY[2];

		f32 f;

		m_nEdgeMask = 0;
		f = m_fEdgesX[0]*m_fEdgesX[0]+m_fEdgesY[0]*m_fEdgesY[0];
		if( fabsf(f) > 10e-6f )
		{
			m_nEdgeMask |= 0x1;
			m_fOnePerEdgesSize[0] = 1.f / (f);
		}

		f = m_fEdgesX[1]*m_fEdgesX[1]+m_fEdgesY[1]*m_fEdgesY[1];
		if( fabsf(f) > 10e-6f )
		{
			m_nEdgeMask |= 0x2;
			m_fOnePerEdgesSize[1] = 1.f / (f);
		}

		f = m_fEdgesX[2]*m_fEdgesX[2]+m_fEdgesY[2]*m_fEdgesY[2];
		if( fabsf(f) > 10e-6f )
		{
			m_nEdgeMask |= 0x4;
			m_fOnePerEdgesSize[2] = 1.f / (f);
		}

		Tri.GetPositions( vTriPos, m_pGroup->GetMeshInfo(Tri.m_nMeshInfo) );
		Tri.GetNormals( vTriNorm, m_pGroup->GetMeshInfo(Tri.m_nMeshInfo) );
		vTriNorm[0].NormalizeSafe(Vec3(0,0,1));
		vTriNorm[1].NormalizeSafe(Vec3(0,0,1));
		vTriNorm[2].NormalizeSafe(Vec3(0,0,1));

		if( m_pAmbientOcclusionIntegrator && m_pGeomCore )
		{
			AABB BBox( vTriPos, 3 );
//			Sphere Sph( BBox.GetCenter(), BBox.GetRadius() + 50 );//m_pSceneCtx->m_fMaxAmbientOcclusionSearchRadius );
			Sphere Sph( BBox.GetCenter(), BBox.GetRadius() + m_pSceneCtx->m_fMaxAmbientOcclusionSearchRadius );

			COctreeCell* pTriBoundingCell = pBoundingCell;
			m_pGeomCore->SearchRoot(&pTriBoundingCell, Sph);
			m_pAmbientOcclusionIntegrator->SetBoundingCell( pTriBoundingCell );
		}

		return true;
	}

	virtual void Triangle( const int iniY )
	{
	}

	virtual void Line( const float infXLeft, const float infXRight,
		const int iniLeft, const int iniRight, const int iniY )
	{
		int32 nIndex = iniY*m_pGroup->iWidth + iniLeft;

		f32 fX = iniLeft+0.5f;		//center of the pixel
		const f32 fY = iniY+0.5f;

/*
		float ffX[3],ffY[3];

		ffX[0] = m_fX[0] - fX;
		ffX[1] = m_fX[1] - fX;
		ffX[2] = m_fX[2] - fX;
		ffY[0] = m_fY[0] - fY;
		ffY[1] = m_fY[1] - fY;
		ffY[2] = m_fY[2] - fY;

		assert( CRLMesh::m_numOffsetSamples < 10 );

		int i = 0;
		for (int s = 0; s < CRLMesh::m_numOffsetSamples; s++)
		{
			// Compute new x & y //fix offsets
			f32 sY = CRLMesh::m_pvOffsetSamples[s].y;

			m_fYprecalc[i++] = ffY[0]-sY;
			m_fYprecalc[i++] = ffY[1]-sY;
			m_fYprecalc[i++] = ffY[2]-sY;
		}
*/

		Vec3 vPositions[9];
		Vec3 vNormals[9];


		for(int x=iniLeft;x<iniRight+(iniLeft==iniRight);x++)
		{

			vColor.Clear();
			vDirectHandler.Clear();
			vIndirectHandler.Clear();
			vSunHandler.Clear();
			vOcclusionHandler.Clear();
			vRAEHandler.Clear();

			int32 nNumOfValidSamples = 0;
			double b1,b2,b3;
			for (int s = 0; s < CRLMesh::m_numOffsetSamples; s++)
			{
				// Compute new x & y //fix offsets
				f32 sX = min(fX + CRLMesh::m_pvOffsetSamples[s].x,infXRight);
				f32 sY = fY + CRLMesh::m_pvOffsetSamples[s].y;

				// Find Barycentric coordinates.
				b1 = ((m_fX[1]-sX) * (m_fY[2]-sY) - (m_fX[2]-sX) * (m_fY[1]-sY)) * m_fOnePerB0;
				b2 = ((m_fX[2]-sX) * (m_fY[0]-sY) - (m_fX[0]-sX) * (m_fY[2]-sY)) * m_fOnePerB0;
				b3 = ((m_fX[0]-sX) * (m_fY[1]-sY) - (m_fX[1]-sX) * (m_fY[0]-sY)) * m_fOnePerB0;


				//just dont use this clipping, even the improved version lead to errors 'caused by the rasterized 
				//texel beeing out of range, that's why it looks more accurate with extrapolated normal, positions etc.
/*					f32 fBestDist = FLT_MAX;
					f32 fNewX,fNewY;
					f32 fNewBX,fNewBY;

				//outside the triangle fix - take care only at the second pass
				bMax = bMin = b1;
				bMin = b2 < bMin ? b2 : bMin;
				bMin = b3 < bMin ? b3 : bMin;
				bMax = b2 > bMax ? b2 : bMax;
				bMax = b3 > bMax ? b3 : bMax;
				//outside the triangle fix - take care only at the second pass
				int EscapeOut	=	5;
				while(false && (bMin < -FLT_EPSILON || bMax > 1.f+FLT_EPSILON) && --EscapeOut)  
				{
					//check with the 3 edge... we outside of the triangle, so we search the nearest point on the edges

					//if not parallel
					if( (m_nEdgeMask & 0x1) && b3<0.f)
					{
						//calculate
						//						f32 fBx = sX - ffX[0];
						//						f32 fBy = sY - ffY[0];
						f32 fBx = sX - m_fX[0];
						f32 fBy = sY - m_fY[0];

						f32 t0 = (m_fEdgesX[0]*fBx + m_fEdgesY[0]*fBy) * m_fOnePerEdgesSize[0];

						//clamp 0-1
						t0 = t0 < 0.f ? 0.f : (t0 > 1.f ? 1.f : t0);

						//the new point
						sX  = m_fEdgesX[0] * t0 + m_fX[0];
						sY  = m_fEdgesY[0] * t0 + m_fY[0];
//						fNewX  = m_fEdgesX[0] * t0 + m_fX[0];
//						fNewY  = m_fEdgesY[0] * t0 + m_fY[0];

						//half pixel back.. (like center of the pixels ??? )
//						f32 fHPx = fNewX - sX;
//						f32 fHPy = fNewY - sY;
//						f32 fHPLength = fHPx*fHPx+fHPy*fHPy;
//						if( fHPLength > 10e-8f )
//							fHPLength = 0.5f / sqrtf(fHPLength);
//						fNewBX = fNewX + fHPx*fHPLength;
//						fNewBY = fNewY + fHPy*fHPLength;

						//distance
//						f32 fDx = sX -fNewX;
//						f32 fDy = sY -fNewY;
//						fBestDist = fDx*fDx+fDy*fDy;
//						assert(!_isnan(fDx)&&!_isnan(fDy));
					}
					else
					if( (m_nEdgeMask & 0x2) && b1<0.f)
					{
						//calculate
						//						f32 fBx = sX - ffX[1];
						//						f32 fBy = sY - ffY[1];
						f32 fBx = sX - m_fX[1];
						f32 fBy = sY - m_fY[1];
						f32 t0 = (m_fEdgesX[1]*fBx + m_fEdgesY[1]*fBy) * m_fOnePerEdgesSize[1];

						//clamp 0-1
						t0 = t0 < 0.f ? 0.f : (t0 > 1.f ? 1.f : t0);

						//the new point
						sX = m_fEdgesX[1] * t0 + m_fX[1];
						sY = m_fEdgesY[1] * t0 + m_fY[1];
//					f32 fx = m_fEdgesX[1] * t0 + m_fX[1];
//						f32 fy = m_fEdgesY[1] * t0 + m_fY[1];

						//half pixel back.. (like center of the pixels ??? )
//						f32 fHPx = fx - sX;
//						f32 fHPy = fy - sY;
//						f32 fHPLength = fHPx*fHPx+fHPy*fHPy;
//						if( fHPLength > 10e-8f )
//							fHPLength = 0.5f / sqrtf(fHPLength);
//						fBx = fx + fHPx*fHPLength;
//						fBy = fy + fHPy*fHPLength;

						//distance
//						f32 fDx = sX -fx;
//						f32 fDy = sY -fy;
//						f32 fDistance = fDx*fDx+fDy*fDy;
//
//						bool bBetter = ( fDistance < fBestDist );
//						fBestDist = bBetter ? fDistance : fBestDist;
//						fNewX = bBetter ? fx : fNewX;
//						fNewY = bBetter ? fy : fNewY;
//						fNewBX = bBetter ? fBx : fNewBX;
//						fNewBY = bBetter ? fBy : fNewBY;
//						assert(!_isnan(fNewBX)&&!_isnan(fNewBY));
					}
					else
					if( (m_nEdgeMask & 0x4) && b2<0.f)
					{
						//calculate
						//						f32 fBx = sX - ffX[2];
						//						f32 fBy = sY - ffY[2];
						f32 fBx = sX - m_fX[2];
						f32 fBy = sY - m_fY[2];

						f32 t0 = (m_fEdgesX[2]*fBx + m_fEdgesY[2]*fBy) * m_fOnePerEdgesSize[2];

						//clamp 0-1
						t0 = t0 < 0.f ? 0.f : (t0 > 1.f ? 1.f : t0);

						//the new point
						sX = m_fEdgesX[2] * t0 + m_fX[2];
						sY = m_fEdgesY[2] * t0 + m_fY[2];
//						f32 fx = m_fEdgesX[2] * t0 + m_fX[2];
//						f32 fy = m_fEdgesY[2] * t0 + m_fY[2];

						//half pixel back.. (like center of the pixels ??? )
//					f32 fHPx = fx - sX;
//						f32 fHPy = fy - sY;
//						f32 fHPLength = fHPx*fHPx+fHPy*fHPy;
//						if( fHPLength > 10e-8f )
//							fHPLength = 0.5f / sqrtf(fHPLength);
//						fBx = fx + fHPx*fHPLength;
//						fBy = fy + fHPy*fHPLength;

						//distance
//						f32 fDx = sX -fx;
//						f32 fDy = sY -fy;
//						f32 fDistance = fDx*fDx+fDy*fDy;

//						bool bBetter = ( fDistance < fBestDist );
//						fBestDist = bBetter ? fDistance : fBestDist;
//						fNewX = bBetter ? fx : fNewX;
//						fNewY = bBetter ? fy : fNewY;
//  					fNewBX = bBetter ? fBx : fNewBX;
//						fNewBY = bBetter ? fBy : fNewBY;
//						assert(!_isnan(fNewBX)&&!_isnan(fNewBY));

					}

					//the fixed barycentric coordinates
//					b1 = ((m_fX[1]-fNewBX) * (m_fY[2]-fNewBY) - (m_fX[2]-fNewBX) * (m_fY[1]-fNewBY)) * m_fOnePerB0;
//					b2 = ((m_fX[2]-fNewBX) * (m_fY[0]-fNewBY) - (m_fX[0]-fNewBX) * (m_fY[2]-fNewBY)) * m_fOnePerB0;
//					b3 = ((m_fX[0]-fNewBX) * (m_fY[1]-fNewBY) - (m_fX[1]-fNewBX) * (m_fY[0]-fNewBY)) * m_fOnePerB0;

//					assert(!_isnan(b1)&&!_isnan(b2)&&!_isnan(b3));
					//outside the triangle fix - take care only at the second pass
//					bMax = bMin = b1;
//					bMin = b2 < bMin ? b2 : bMin;
//					bMin = b3 < bMin ? b3 : bMin;
//					bMax = b2 > bMax ? b2 : bMax;
//					bMax = b3 > bMax ? b3 : bMax;
					//outside the triangle fix - take care only at the second pass
//					if( bMin < 0.0 || bMax > 1.f )
					{
						//the fixed barycentric coordinates
//						b1 = ((m_fX[1]-fNewX) * (m_fY[2]-fNewY) - (m_fX[2]-fNewX) * (m_fY[1]-fNewY)) * m_fOnePerB0;
//						b2 = ((m_fX[2]-fNewX) * (m_fY[0]-fNewY) - (m_fX[0]-fNewX) * (m_fY[2]-fNewY)) * m_fOnePerB0;
//						b3 = ((m_fX[0]-fNewX) * (m_fY[1]-fNewY) - (m_fX[1]-fNewX) * (m_fY[0]-fNewY)) * m_fOnePerB0;
						b1 = ((m_fX[1]-sX) * (m_fY[2]-sY) - (m_fX[2]-sX) * (m_fY[1]-sY)) * m_fOnePerB0;
						b2 = ((m_fX[2]-sX) * (m_fY[0]-sY) - (m_fX[0]-sX) * (m_fY[2]-sY)) * m_fOnePerB0;
						b3 = ((m_fX[0]-sX) * (m_fY[1]-sY) - (m_fX[1]-sX) * (m_fY[0]-sY)) * m_fOnePerB0;
						bMax = bMin = b1;
						bMin = b2 < bMin ? b2 : bMin;
						bMin = b3 < bMin ? b3 : bMin;
						bMax = b2 > bMax ? b2 : bMax;
						bMax = b3 > bMax ? b3 : bMax;
//						sX	=	fNewX;
//						sY	=	fNewY;
						assert(!_isnan(b1)&&!_isnan(b2)&&!_isnan(b3));
					}
				}
//				b1	=	b1>1.f?1.f:b1<0.f?0.f:b1;
//				b2	=	b2>1.f?1.f:b2<0.f?0.f:b2;
//				b3	=	b3>1.f?1.f:b3<0.f?0.f:b3;*/

				// Compute position & normal
				vPos.x = vTriPos[0].x*b1 + vTriPos[1].x*b2 + vTriPos[2].x*b3;
				vPos.y = vTriPos[0].y*b1 + vTriPos[1].y*b2 + vTriPos[2].y*b3;
				vPos.z = vTriPos[0].z*b1 + vTriPos[1].z*b2 + vTriPos[2].z*b3;

				vNorm.x = vTriNorm[0].x*b1 + vTriNorm[1].x*b2 + vTriNorm[2].x*b3;
				vNorm.y = vTriNorm[0].y*b1 + vTriNorm[1].y*b2 + vTriNorm[2].y*b3;
				vNorm.z = vTriNorm[0].z*b1 + vTriNorm[1].z*b2 + vTriNorm[2].z*b3;
				vNorm.Normalize();
//				assert(fabsf(b1+b2+b3-1.f)<FLT_EPSILON);


				if(s < 9 )
				{
					vPositions[s] = vPos;
					vNormals[s] = vNorm;
				}

#ifndef _DEBUG_RASTER_NORMALS
				//generate occlusion map
				if( NULL != m_pOcclusionIntegrator && m_OcclusionLightIDs )
				{
					if( m_pOcclusionIntegrator->Illuminance( vOcclusionHandler, vPos, vNorm, 0, m_OcclusionLightIDs, m_pIntersectionCache ) )
						++nNumOfValidSamples;
				}

				if( !m_pSceneCtx->m_bMakeLightMap )
					continue;

				m_Tri.BaryInterpolateTangent(b1, b2, b3, vBinormal, vTangent, m_pGroup->GetMeshInfo(m_Tri.m_nMeshInfo));
				//handle direct light sources
				if (m_pDirectIntegrator!=NULL && m_LightmapLightIDs)
				{
					vDirectHandler.SetTangentBasis(vTangent, vBinormal, vNorm);
					m_pDirectIntegrator->Illuminance( vDirectHandler, vPos, vNorm, (gf_PI/2.0f), m_LightmapLightIDs, m_pIntersectionCache );
				}

				//handle all indirect light
				if (m_pIndirectIntegrator!=NULL && m_LightmapLightIDs)
				{
					vIndirectHandler.SetTangentBasis(vTangent, vBinormal, vNorm);
					m_pIndirectIntegrator->Illuminance( vIndirectHandler, vPos, vNorm, (gf_PI/2.0f), m_LightmapLightIDs, m_pIntersectionCache );
				}

				//handle direct sun light
				if (m_pSunIntegrator!=NULL && m_LightmapLightIDs)
				{
					vSunHandler.SetTangentBasis(vTangent, vBinormal, vNorm);
					m_pSunIntegrator->Illuminance( vSunHandler, vPos, vNorm, (gf_PI/2.0f), m_LightmapLightIDs, m_pIntersectionCache );
				}
#endif
			} // end for samples (s);

			if( NULL != m_pAmbientOcclusionIntegrator )
				m_pAmbientOcclusionIntegrator->Illuminance( vRAEHandler, vPositions[0], vNormals[0], min(9,CRLMesh::m_numOffsetSamples) , m_OcclusionLightIDs, m_pIntersectionCache);


			if( m_pSceneCtx->m_bDistributedMap )
				m_pGroup->SetComponent_DistributedMap( nIndex, vPositions, vNormals );

			// generate oclusion maps...
			if( m_pSceneCtx->m_bMakeOcclMap && nNumOfValidSamples )
			{
			const 	float fOnePerOffsetSamples = 1.f /nNumOfValidSamples;
				for( int32 i = 0; i < nMaxComp; ++i )
				{
						m_pGroup->SetComponent_Occlusion(i,nIndex, Vec4( 1-vOcclusionHandler.m_vLightFlux[i].x*fOnePerOffsetSamples,
							1-vOcclusionHandler.m_vLightFlux[i].y*fOnePerOffsetSamples,
							1-vOcclusionHandler.m_vLightFlux[i].z*fOnePerOffsetSamples,
							1-vOcclusionHandler.m_vLightFlux[i].w*fOnePerOffsetSamples ) );
				}//i
			}

//#define POSSCALE 0.5
			if( m_pSceneCtx->m_bMakeRAE )
				m_pGroup->SetComponent_RAE( nIndex, Vec4( vRAEHandler.m_vLightFlux[0].x, vRAEHandler.m_vLightFlux[0].y , vRAEHandler.m_vLightFlux[0].z, vRAEHandler.m_vLightFlux[0].w) );
//			m_pGroup->SetComponent_RAE( nIndex, Vec4( vRAEHandler.m_vLightFlux[0].x * m_fOnePerOffsetSamples, vRAEHandler.m_vLightFlux[0].y * m_fOnePerOffsetSamples, vRAEHandler.m_vLightFlux[0].z * m_fOnePerOffsetSamples, vRAEHandler.m_vLightFlux[0].w * m_fOnePerOffsetSamples ) );
//			m_pGroup->SetComponent_RAE( nIndex, Vec4( vPos.x*POSSCALE-floor(vPos.x*POSSCALE), 0,0,0) );//Vec4( vRAEHandler.m_vLightFlux[0].x * m_fOnePerOffsetSamples, vRAEHandler.m_vLightFlux[0].y * m_fOnePerOffsetSamples, vRAEHandler.m_vLightFlux[0].z * m_fOnePerOffsetSamples, vRAEHandler.m_vLightFlux[0].w * m_fOnePerOffsetSamples ) );

			if( m_pSceneCtx->m_bMakeLightMap )
			{
#ifndef _DEBUG_RASTER_NORMALS
				vColor = vDirectHandler + vIndirectHandler + vSunHandler;
#else
//				vColor.Add( Vec4( (vNorm.x+1)*0.5f, (vNorm.y+1)*0.5f, (vNorm.z+1)*0.5f, 1 ) );
//#define POSSCALE 0.5
//				vColor.Add( Vec4( vPos.y*POSSCALE-floor(vPos.y*POSSCALE), 0,0, 1 ) );
				vColor.Add( Vec4( vPos.x*POSSCALE-floor(vPos.x*POSSCALE), vPos.y*POSSCALE-floor(vPos.y*POSSCALE), vPos.z*POSSCALE-floor(vPos.z*POSSCALE), 1 ) );
//					vColor.Add( Vec4( b1, b2, b3, 1 ) );
#endif
				m_pGroup->SetComponent_DynamicRange(nIndex, vColor);
			}
			fX += 1.f;
			++nIndex;
		}
	}

private:
	//global informations
	CRLUnwrapGroup*					m_pGroup;
	CIlluminanceIntegrator* m_pDirectIntegrator;
	CIlluminanceIntegrator* m_pIndirectIntegrator;
	CIlluminanceIntegrator* m_pSunIntegrator;
	CIlluminanceIntegrator* m_pOcclusionIntegrator;
	CIlluminanceIntegrator* m_pAmbientOcclusionIntegrator;
	t_pairEntityId*					m_LightmapLightIDs;
	t_pairEntityId*					m_OcclusionLightIDs;
	f32											m_fOnePerOffsetSamples;
	int32										m_nOcclLightNum;
	//per triangle informations
	CRLTriangle							m_Tri;
	double									m_fOnePerB0;
	f32											m_fX[3];
	f32											m_fY[3];
	f32											m_fEdgesX[3];
	f32											m_fEdgesY[3];
	int32										m_nEdgeMask;
	f32											m_fOnePerEdgesSize[3];
	CIllumHandler						vColor;
	CIllumHandler						vDirectHandler;
	CIllumHandler						vIndirectHandler;
	CIllumHandler						vSunHandler;
	CIllumHandler						vOcclusionHandler;
	CIllumHandler						vRAEHandler;
	CGeomCore*							m_pGeomCore;
	CSceneContext*					m_pSceneCtx;
	Vec3										vPos;
	Vec3										vNorm;
	Vec3										vBinormal;
	Vec3										vTangent;
	Vec3										vTriPos[3];
	Vec3										vTriNorm[3];
//	f32											m_fYprecalc[3*10];
	CIntersectionCacheCluster* m_pIntersectionCache;
	int32										nMaxComp;
};


//===================================================================================
// Method				:	ExtendCoordinates
// Description	:	Grove the coordinates to generate a "dilated" triangle
//===================================================================================
void CRLMesh::ExtendCoordinates( float x[3], float y[3] )
{
	x[0] += DILATE_SPACE;
	x[1] += DILATE_SPACE;
	x[2] += DILATE_SPACE;
	y[0] += DILATE_SPACE;
	y[1] += DILATE_SPACE;
	y[2] += DILATE_SPACE;

	float CenterX = (x[0]+x[1]+x[2])/3.f;
	float CenterY = (y[0]+y[1]+y[2])/3.f;

	for( int nVNum = 0; nVNum < 3; ++nVNum )
	{
		float DirX = x[nVNum]-CenterX;
		float DirY = y[nVNum]-CenterY;

		//normalize
		float fLen = DirX*DirX+DirY*DirY;
		if( fLen > 0.f )
		{
			//we will have at least 1 direction
//			DirX = DirX < 0 ? -DILATE_SPACE : DirX > 0 ? DILATE_SPACE : 0;
//			DirY = DirY < 0 ? -DILATE_SPACE : DirY > 0 ? DILATE_SPACE : 0;
			if( DirX < 0 )
				DirX = -DILATE_SPACE;
			else
				DirX = DirX > 0 ? DILATE_SPACE : 0;

			if( DirY < 0 )
				DirY = -DILATE_SPACE;
			else
				DirY = DirY > 0 ? DILATE_SPACE : 0;
		}
		else
		{
			//special case: degenerated triangle - we need at least 1 pixel border around it..
			switch( nVNum )
			{
			case 0:
				DirX = 0;
				DirY = DILATE_SPACE;
				break;
			case 1:
				DirX = -DILATE_SPACE;
				DirY = -DILATE_SPACE;
				break;
			case 2:
				DirX =  DILATE_SPACE;
				DirY = -DILATE_SPACE;
				break;
			}
		}

		x[nVNum] += DirX;
		y[nVNum] += DirY;
	}
}

//===================================================================================
// Method				:	UnwrapGroupGreater
// Description	:	Comparasion helper function to sort the unwrapgroups
//===================================================================================
inline bool UnwrapGroupGreater( CRLUnwrapGroup d1,CRLUnwrapGroup d2 )
{
	return (d1.m_dUnitPerU * d1.m_dUnitPerV) > (d2.m_dUnitPerU * d2.m_dUnitPerV);
}


//===================================================================================
// Method				:	CRLMesh
// Description	:	Constructor
//===================================================================================
CRLMesh::CRLMesh(IRenderMesh* pRenderMesh, Matrix34* pNodeMatrix, CRenderChunk* pRenderChunk, IRenderShaderResources* shaderResources, const bool bNeedToAutoGenerateTextureCoordinates, const bool bOpaque, const f32 fLightmapQuality )
{
	//default (optional)
	m_fLightmapQuality = fLightmapQuality;
	m_pUnwrapGroups = NULL;
	m_pShaderResources = shaderResources;
	//assert(m_pShaderResources != NULL);

	m_pGeomCore = CSceneContext::GetInstance().GetGeomCore();
	assert(m_pGeomCore != NULL);

	//generate "auto" textures for meshes which not have textures
	if( pRenderMesh )
		m_bIsFilled = CreateMesh(pRenderMesh, pNodeMatrix, pRenderChunk, bOpaque, bNeedToAutoGenerateTextureCoordinates || !m_pShaderResources );
	else
		m_bIsFilled = false;
}


//===================================================================================
// Method				:	CreateMesh
// Description	:	Create an RLMesh from a rendermesh
//===================================================================================
bool CRLMesh::CreateMesh(IRenderMesh* pRenderMesh, Matrix34* pNodeMatrix, CRenderChunk* pRenderChunk, const bool bOpaque, const bool bNeedToAutoGenerateTextureCoordinates)
{

	// Get indices and vertices
	int iIdxCnt = 0;
	unsigned short* pIndices = pRenderMesh->GetIndices(&iIdxCnt);
	int iUVStride = 0;
	TexUV *pUV = reinterpret_cast<TexUV *> (pRenderMesh->GetStridedUVPtr(iUVStride)); //fix

	// +++ Find non-overlaped mesh groups by index-shared groups +++
	//std::vector<uint32> arrGroupIds;
	uint32 numTris =  pRenderChunk->nNumIndices / 3;

	if (numTris<1) assert(0);

	//fix !!!!!!!!!!!!!!!!!!!
	uint32 arrGroupIds[65535];
	memset(arrGroupIds,0,sizeof(uint32)* 65535);
	//arrGroupIds.reserve(numTris);
	//arrGroupIds.assign(numTris, 0); 	// set 0 group for non-assigned triangles
	uint32 nMarkGroup = 1 ;

	//set first connectivity group for first triangle
	arrGroupIds[0] = nMarkGroup;
	bool bNewGroup = true;
	bool bNewRelation = false;

	//precalculations - need more memory, run much faster...
	bool bUseTheOldMethod;
	uint32 *pTriTriConnectionArrayPositions = NULL;
	uint32 *pTriTriConnectionIDs = NULL;


	//check we have a lot of tri.. becouse the memory allocation need time...
	if( numTris > 500 )		//FIX:: made this non hardcoded value!
	{
		pTriTriConnectionArrayPositions = new uint32[ numTris*2 ];
		pTriTriConnectionIDs = new uint32[ numTris*10 ];
		assert( NULL != pTriTriConnectionArrayPositions );
		assert( NULL != pTriTriConnectionIDs );
		bUseTheOldMethod = false;

		//if can't allocate the memory it will use the old method
		if( pTriTriConnectionArrayPositions && pTriTriConnectionIDs )
		{
			memset(pTriTriConnectionArrayPositions, 0, sizeof(uint32)*numTris*2 );
			memset(pTriTriConnectionIDs, 0, sizeof(uint32)*numTris*10 );

			uint32 nTri0,nTri1;
			uint32 nActualPosition = 0;



			//generate tri-tri connection map.. pay one time the check...
			for (nTri0 = 0; nTri0<numTris; nTri0++)
			{
				pTriTriConnectionArrayPositions[nTri0*2+0] = nActualPosition;

				uint32 nBaseIdx0 = pRenderChunk->nFirstIndexId + nTri0 * 3;
				for (nTri1=0; nTri1<numTris; nTri1++)
				{
					if ( nTri0 != nTri1 )
					{
						uint32 nBaseIdx1 = pRenderChunk->nFirstIndexId + nTri1 * 3;

						//check connectivity
						for (int16 v=0; v< 3; v++) 
						{
							if(	pIndices[nBaseIdx0+0] == pIndices[nBaseIdx1+v] ||
								pIndices[nBaseIdx0+1] == pIndices[nBaseIdx1+v] ||
								pIndices[nBaseIdx0+2] == pIndices[nBaseIdx1+v]
								)
								{
									pTriTriConnectionIDs[nActualPosition] = nTri1;
									++nActualPosition;

									//need too much space, use the old, slower method
									if( nActualPosition == numTris*10 )
																	bUseTheOldMethod = true;
									break;
								}

								if( bUseTheOldMethod )
																		break;
						}
						//end - check connectivity
					}//if( nTri0 != nTri1 )

					if( bUseTheOldMethod )
						break;
				}//nTri1

				//end position of connected triangles...
				pTriTriConnectionArrayPositions[nTri0*2+1] = nActualPosition;

				if( bUseTheOldMethod )
					break;
			}//nTri0

			//we can use the old method with precalculated datas...
			if( false == bUseTheOldMethod )
			{
				while (bNewRelation || bNewGroup) //iteratively - to obtain all possible connections
				{
					bNewRelation = false;
					bNewGroup = false;
					for ( nTri0 = 0; nTri0<numTris; nTri0++ )
					{
						uint32 nBaseIdx0 = pRenderChunk->nFirstIndexId + nTri0 * 3;
						if ( arrGroupIds[nTri0]==nMarkGroup )
						{
							uint32 nTriEndPosition = pTriTriConnectionArrayPositions[nTri0*2+1];
							for (uint32 i=pTriTriConnectionArrayPositions[nTri0*2+0]; i < nTriEndPosition; ++i )
							{
								uint32 nTri1 = pTriTriConnectionIDs[i];
								//only if it's an unused triangle
								if ( arrGroupIds[nTri1]==0 )
								{
									//put the same group the connected tri...
									arrGroupIds[ nTri1 ] = nMarkGroup;
									bNewRelation = true;
								}
							}//for connected tris

						}//if tri0 in the marked group
					}//nTri0

					//try to find new group for marking
					if (bNewRelation == false)
					{
						for (uint32 nTri = 0; nTri<numTris; nTri++)
						{
							if (arrGroupIds[nTri]==0)
							{
								nMarkGroup++;
								arrGroupIds[nTri] = nMarkGroup;
								bNewGroup = true;
								break;
							}
						}
					}
				}//while new relation or new group
			}
		}
		else bUseTheOldMethod = true;

		SAFE_DELETE_ARRAY( pTriTriConnectionArrayPositions );
		SAFE_DELETE_ARRAY( pTriTriConnectionIDs );
	}
	else
		bUseTheOldMethod = true;

	if( bUseTheOldMethod )
	{
		while (bNewRelation || bNewGroup) //iteratively - to obtain all possible connections
		{
			bNewRelation = false;
			bNewGroup = false;

			for (uint32 nTri0 = 0; nTri0<numTris; nTri0++)
			{
				uint32 nBaseIdx0 = pRenderChunk->nFirstIndexId + nTri0 * 3;

				if ( arrGroupIds[nTri0]==nMarkGroup )
				{
					for (uint32 nTri1=0; nTri1<numTris; nTri1++)
					{
						if ( nTri0 != nTri1 && arrGroupIds[nTri1]==0 )
						{
							uint32 nBaseIdx1 = pRenderChunk->nFirstIndexId + nTri1 * 3;

							//check connectivity
							for (int16 v=0; v< 3; v++) 
							{
								if(	pIndices[nBaseIdx0+0] == pIndices[nBaseIdx1+v] ||
									pIndices[nBaseIdx0+1] == pIndices[nBaseIdx1+v] ||
									pIndices[nBaseIdx0+2] == pIndices[nBaseIdx1+v]
									)
									{
										arrGroupIds[nTri1] = nMarkGroup;
										bNewRelation = true;
										break;
									}
							}
							//end - check connectivity
						}
					}
				}//end new group marking
			}//end marking for all groups

			//try to find new group for marking
			if (bNewRelation == false)
			{
				for (uint32 nTri = 0; nTri<numTris; nTri++)
				{
					if (arrGroupIds[nTri]==0)
					{
						nMarkGroup++;
						arrGroupIds[nTri] = nMarkGroup;
						bNewGroup = true;
						break;
					}
				}
			}

		} //main loop
	}
	// --- Find non-overlaped mesh groups by index-shared groups ---

	//+++ Reserve memory for rasterization of unwrap groups +++
	m_numUnwrapGroups = nMarkGroup; //check for empty mesh before
	m_pUnwrapGroups = new CRLUnwrapGroup[m_numUnwrapGroups];
	assert(m_pUnwrapGroups!=NULL);
	//--- Reserve memory for rasterization of unwrap groups ---

	{
		//for (uint32 nTri = 0; nTri<numTris; nTri++) //hack - force to use single group
		//{		arrGroupIds[nTri]=1;	}
		//nMarkGroup = 1;
	}

	int32* pInt = new int32[m_numUnwrapGroups];
	memset(pInt,0,sizeof(int32)*m_numUnwrapGroups);
	// Process all triangles
	for (uint32 iCurTri=0; iCurTri<pRenderChunk->nNumIndices / 3; iCurTri++)
	{
		pInt[arrGroupIds[iCurTri]-1]++;
	}

	int32* pMeshInfo = new int32[m_numUnwrapGroups];

	//setup the unwrap groups
	for (uint32 i=0; i<m_numUnwrapGroups; i++)
	{
		m_pUnwrapGroups[i].CreateItemArray( pInt[i] );
		pMeshInfo[i] = m_pUnwrapGroups[i].AddMeshInfos(pRenderMesh, pNodeMatrix, m_pShaderResources );
	}

	SAFE_DELETE_ARRAY(pInt);

	CRLTriangle Tri;	

	// Process all triangles
	for (uint32 iCurTri=0; iCurTri<pRenderChunk->nNumIndices / 3; iCurTri++)
	{
		uint32 iBaseIdx = pRenderChunk->nFirstIndexId + iCurTri * 3;

		uint32 groupID = arrGroupIds[iCurTri];
		assert ((groupID-1) <m_numUnwrapGroups);

		CRLUnwrapGroup& unwrapGroup = m_pUnwrapGroups[groupID-1]; //indexed by groupID-1
		Tri.m_nMeshInfo = pMeshInfo[groupID-1];

//		Tri.SetOpaque( bOpaque );

		//ASSERT( _CrtCheckMemory() );


		// Add all 3 vertices
		for (int v=0; v<=2; v++)
		{
			Tri.m_nInd[v] = pIndices[iBaseIdx + v]; //store actual vertex buffer index
			assert(Tri.m_nInd[v] >=0 && Tri.m_nInd[v] <=65535);
			//generate bboxes
			unwrapGroup.AddVertex( Tri.GetPosition( v, unwrapGroup.GetMeshInfo(Tri.m_nMeshInfo) ) );
			// Modified for smooth shading. We take the vertex normal as it is the
			// most important component, this guarantees smoothness across texture coordinate
			// discontinuities when the normals are smooth. Note that we don't orthonormalize the basis 
			// (looks good enough without)
		} // v


		if (false == bNeedToAutoGenerateTextureCoordinates) // use diffuse tex coords for unwrapping
		{
			//Normalize tiling for lightmap unwraping
			TexUV uv;
			//fetch uv's needed by tangent space calc
			uv = *(reinterpret_cast<TexUV *> (&(reinterpret_cast<byte *>(pUV)[pIndices[iBaseIdx + 0] * iUVStride])));
			Tri.m_vVert[0].u = uv.u;
			Tri.m_vVert[0].v = uv.v;
			if( _isnan(Tri.m_vVert[0].u ) )
				Tri.m_vVert[0].u =  0;
			if( _isnan(Tri.m_vVert[0].v ) )
				Tri.m_vVert[0].v =  0;
			uv = *(reinterpret_cast<TexUV *> (&(reinterpret_cast<byte *>(pUV)[pIndices[iBaseIdx + 1] * iUVStride])));
			Tri.m_vVert[1].u = uv.u;
			Tri.m_vVert[1].v = uv.v;
			if( _isnan(Tri.m_vVert[1].u ) )
				Tri.m_vVert[1].u =  Tri.m_vVert[0].u;
			if( _isnan(Tri.m_vVert[1].v ) )
				Tri.m_vVert[1].v =  Tri.m_vVert[0].v;
			uv = *(reinterpret_cast<TexUV *> (&(reinterpret_cast<byte *>(pUV)[pIndices[iBaseIdx + 2] * iUVStride])));
			Tri.m_vVert[2].u = uv.u;
			Tri.m_vVert[2].v = uv.v;
			if( _isnan(Tri.m_vVert[2].u ) )
				Tri.m_vVert[2].u =  Tri.m_vVert[0].u;
			if( _isnan(Tri.m_vVert[2].v ) )
				Tri.m_vVert[2].v =  Tri.m_vVert[0].v;
		}
		else //generate tex coords for voxel's unwrapping
		{
			//FIX - calculate lum density based on voxel BBOX
			//we have filled triangle already
			TexUV uv;
			int nAxis = 0;
			Vec3 vTriEdgeA = Tri.GetPosition( 1, unwrapGroup.GetMeshInfo(Tri.m_nMeshInfo) ) -Tri.GetPosition( 0, unwrapGroup.GetMeshInfo(Tri.m_nMeshInfo) );
			Vec3 vTriEdgeB = Tri.GetPosition( 2, unwrapGroup.GetMeshInfo(Tri.m_nMeshInfo) )  - Tri.GetPosition( 0, unwrapGroup.GetMeshInfo(Tri.m_nMeshInfo) ) ;
			Vec3 vFaceNormal = vTriEdgeA.Cross(vTriEdgeB);
			vFaceNormal.NormalizeSafe();
			vFaceNormal = vFaceNormal.abs();
			
			for (int v=2; v>=0; v--)
			{
				Vec3& vCurPos = Tri.GetPosition( v, unwrapGroup.GetMeshInfo(Tri.m_nMeshInfo) );
				Vec3& vNormal = Tri.GetNormal( v, unwrapGroup.GetMeshInfo(Tri.m_nMeshInfo) );
				Vec3& vTangent = Tri.GetTangent( v, unwrapGroup.GetMeshInfo(Tri.m_nMeshInfo) );
				Vec3& vBinormal = Tri.GetBinormal( v, unwrapGroup.GetMeshInfo(Tri.m_nMeshInfo) );
				Vec3 vNormalAbs = vNormal.abs();

				if ( vFaceNormal.x >= vFaceNormal.y && vFaceNormal.x >= vFaceNormal.z )
				{
					uv.u = vCurPos.y;
					uv.v = vCurPos.z;

					//x axis for unwrap
					nAxis = 0;
				}
 				else if ( vFaceNormal.y >= vFaceNormal.x && vFaceNormal.y >= vFaceNormal.z ) 
				{
					uv.u = vCurPos.x;
					uv.v = vCurPos.z;

					//y axis
					nAxis = 1;
				}
				else
				{
					uv.u = vCurPos.x;
					uv.v = vCurPos.y;

					//z axis
					nAxis = 2;
				}

				if ( vFaceNormal.x >= vFaceNormal.y && vFaceNormal.x >=  vFaceNormal.z )
				{
					//set TexGen tangent for voxels
					//should be matched to the CSectorInfo::SetupTexGenParams order
					vTangent.x = 0.0f;
					vTangent.y = 1.0f;
					vTangent.z = 0.0f;

					vBinormal.x = 0.0f;
					vBinormal.y = 0.0f;
					vBinormal.z = 1.0f;
				}
				else if (  vFaceNormal.y >=  vFaceNormal.x &&  vFaceNormal.y >=  vFaceNormal.z ) 
				{
					//set TexGen tangent for voxels
					//should be matched to the CSectorInfo::SetupTexGenParams order
					vTangent.x = 1.0f;
					vTangent.y = 0.0f;
					vTangent.z = 0.0f;

					vBinormal.x = 0.0f;
					vBinormal.y = 0.0f;
					vBinormal.z = 1.0f;
				}
				else
				{
					//set TexGen tangent for voxels
					//should be matched to the CSectorInfo::SetupTexGenParams order
					vTangent.x = 1.0f;
					vTangent.y = 0.0f;
					vTangent.z = 0.0f;

					vBinormal.x = 0.0f;
					vBinormal.y = 1.0f;
					vBinormal.z = 0.0f;
				}
				Tri.m_vVert[v].u = uv.u;
				Tri.m_vVert[v].v = uv.v;

				if( _isnan(Tri.m_vVert[v].u ) )
					Tri.m_vVert[v].u =  0;
				if( _isnan(Tri.m_vVert[v].v ) )
					Tri.m_vVert[v].v =  0;
				}

			//voxel's unwrap group should use the same axis only
//			assert (unwrapGroup.CheckUnwrapAxis(nAxis));
			unwrapGroup.CheckUnwrapAxis(nAxis);
		}

		//scale luxel per unit grid
		unwrapGroup.AdjustUnitRatio(&Tri);

		// Add triangle to the GeomCore 
		unwrapGroup.arrTriangles[ unwrapGroup.m_nTriNumber++ ] = Tri;
	}

	//+++  Tex coord normalization +++
//	std::vector<CRLUnwrapGroup> arrValidGroups;
//	arrValidGroups.clear();
	int16 nGroup;
	for(nGroup = 0; nGroup < m_numUnwrapGroups; nGroup++)
	{
		if (NormalizeTexCoords(m_pUnwrapGroups[nGroup]))
		{
			m_pUnwrapGroups[nGroup].CalcUnitRatio( m_pShaderResources );
//			arrValidGroups.push_back(m_pUnwrapGroups[nGroup]);
		}
	}

	//Important for packing: bigger first, smaller next...
//	std::sort( arrValidGroups.begin(), arrValidGroups.end(), UnwrapGroupGreater );

	//---  Tex coord normalization ---

	//+++  Copy valid groups +++
/*
	//reallocate new CRLUnwrapGroup storage
	delete[] m_pUnwrapGroups;
	m_numUnwrapGroups =	arrValidGroups.size();
	m_pUnwrapGroups = new CRLUnwrapGroup[m_numUnwrapGroups];

	std::vector<CRLUnwrapGroup>::iterator itGroup= arrValidGroups.begin();
	for(nGroup=0; itGroup!=arrValidGroups.end(); ++itGroup, ++nGroup)
		m_pUnwrapGroups[nGroup] = *itGroup;
*/
	//---  Copy valid groups ---

	SAFE_DELETE_ARRAY(pMeshInfo);
	return true;
}


//===================================================================================
// Method				:	AttachOcclusionLightToList
// Description	:	Add a light into occlusion light list, if it lighting the mesh
//===================================================================================
void CRLMesh::AttachOcclusionLightToList( t_pairEntityId &LightIDs, IVisArea *pVisArea ) const
{
	//search light which can affect the mesh.. (I use the unwrapgroups bounding boxes, not one big one,
	//I hope this will be more precise.)

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();
	assert(pListLights!=NULL);

	//test every light.
	CRLUnwrapGroup	*pGroup;
	int32 nLightNumber			= pListLights->Count();
	Vec3	*pOrigin;
	f32		fMaxLightDistance;
	f32		fSqrMaxDistance;

	for( int32 i=0; i< nLightNumber; ++i )
	{
		CDLight* pLight	=	pListLights->GetAt(i);
		if( NULL == pLight )
		{
			//accident?
			assert(0 && "there must be a light!");
			continue;
		}

		// test the light same as LightMan.cpp do...
		if( pLight->m_Flags & DLF_THIS_AREA_ONLY )
		{
			if( NULL != pVisArea && pLight->m_pOwner!=(IRenderNode*)-1 )
			{
				if(	pLight->m_pOwner  )
				{
					if( pLight->m_pOwner->GetEntityVisArea() )
					{
						IVisArea * pLightArea = (IVisArea *)pLight->m_pOwner->GetEntityVisArea();

						if( pVisArea != pLightArea )
						{	
							int nMaxRecursion = 2;
							if( false == pVisArea->FindVisArea(pLightArea, nMaxRecursion, true) )		// try also portal volumes
								continue; // areas do not much
						}
					}
					else
						if( NULL != pVisArea ) //&& false == pVisArea->IsAffectedByOutLights() )
							continue;
				}
				else
					if( NULL != pVisArea ) //&& false == pVisArea->IsAffectedByOutLights() )
						continue;
			}
			else // entity is outside
			{
				if( NULL == pVisArea )
				{
					if( pLight->m_pOwner!=(IRenderNode*)-1  &&  NULL != pLight->m_pOwner->GetEntityVisArea() )
							continue;
				}
			}
		}

		if (false == sceneCtx.m_bUseSunLight && (pLight->m_Flags & DLF_SUN ) )
			continue;

		if( !(pLight->m_Flags & DLF_DIFFUSEOCCLUSION) && !(pLight->m_Flags & DLF_SPECULAROCCLUSION) )
			continue;


		int32 nGlobalLightNumber = LightIDs.size();

		if( 16 == nGlobalLightNumber )
															return;

		//search, if found, don't add it
		int32 j;
		for(j = 0; j < nGlobalLightNumber; ++j )
				if( LightIDs[j].second == i )
																break;

		if( j != nGlobalLightNumber )
			continue;


		fMaxLightDistance = pLight->m_fRadius;
		pOrigin = &(pLight->m_BaseOrigin);

		for(int16 nGroup=0; nGroup < m_numUnwrapGroups; ++nGroup )
		{
			pGroup = &m_pUnwrapGroups[nGroup];

			Vec3 vCenter(pGroup->m_vBBoxMin);
			vCenter += pGroup->m_vBBoxMax;
			vCenter *= 0.5f;

			Vec3 vBBoxRadius(pGroup->m_vBBoxMax);
			vBBoxRadius -= vCenter;

			fSqrMaxDistance = fMaxLightDistance + vBBoxRadius.GetLength();
			fSqrMaxDistance *= fSqrMaxDistance;

			if( pOrigin->GetSquaredDistance(vCenter)  <= fSqrMaxDistance )
			{
				LightIDs.push_back( std::pair<EntityId, EntityId>( pLight->m_nEntityId, i ) );
				break;
			}
		}
	}
}

//===================================================================================
// Method				:	AttachLightmapLightToList
// Description	:	Add the lights into lightmap light list, if it's lighting the mesh
//===================================================================================
void CRLMesh::AttachLightmapLightToList( t_pairEntityId &LightIDs, IVisArea *pVisArea  ) const
{
	//search light which can affect the mesh.. (I use the unwrapgroups bounding boxes, not one big one,
	//I hope this will be more precise.)

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();
	assert(pListLights!=NULL);

	//test every light.
	CRLUnwrapGroup	*pGroup;
	int32 nLightNumber			= pListLights->Count();
	Vec3	*pOrigin;
	f32		fMaxLightDistance;
	f32		fSqrMaxDistance;

	for( int32 i=0; i< nLightNumber; ++i )
	{
		CDLight* pLight	=	pListLights->GetAt(i);
		if( NULL == pLight )
		{
			//accident?
			assert(0 && "there must be a light!");
			continue;
		}

		//in lightmap version every light can affect the surfaces - no visarea optimalizations

		if (false == sceneCtx.m_bUseSunLight && (pLight->m_Flags & DLF_SUN ) )
			continue;

		if( !(pLight->m_Flags & DLF_LM) )
			continue;

		int32 nGlobalLightNumber = LightIDs.size();

		//search, if found, don't add it
		int32 j;
		for(j = 0; j < nGlobalLightNumber; ++j )
			if( LightIDs[j].second == i )
				break;

		if( j != nGlobalLightNumber )
			continue;


		fMaxLightDistance = pLight->m_fRadius;
		pOrigin = &(pLight->m_BaseOrigin);

		for(int16 nGroup=0; nGroup < m_numUnwrapGroups; ++nGroup )
		{
			pGroup = &m_pUnwrapGroups[nGroup];

			Vec3 vCenter(pGroup->m_vBBoxMin);
			vCenter += pGroup->m_vBBoxMax;
			vCenter *= 0.5f;

			Vec3 vBBoxRadius(pGroup->m_vBBoxMax);
			vBBoxRadius -= vCenter;

			fSqrMaxDistance = fMaxLightDistance + vBBoxRadius.GetLength();
			fSqrMaxDistance *= fSqrMaxDistance;

			if( pOrigin->GetSquaredDistance(vCenter)  <= fSqrMaxDistance )
			{
				LightIDs.push_back( std::pair<EntityId, EntityId>( pLight->m_nEntityId, i ) );
				break;
			}
		}
	}
}


//===================================================================================
// Method				:	NormalizeTexCoords
// Description	:	Rescale the texture coordinates to [0..1]
//===================================================================================
bool CRLMesh::NormalizeTexCoords(CRLUnwrapGroup& group)
{
	f32 fDeltaU = group.maxUV.u - group.minUV.u;
	f32 fDeltaV = group.maxUV.v - group.minUV.v;

	//	avoid degenerated triangles during rasterization
	//	and filter all possible empty lightmaps
	if (fDeltaU<CGeomCore::m_fEps || 
			fDeltaV<CGeomCore::m_fEps )
	{
		for (int32 i = 0; i < group.m_nTriNumber; ++i)
		{
			CRLTriangle* pTri = &group.arrTriangles[i];
			for (int32 nVert=0; nVert<=2; nVert++)
			{
				//actual normalization
				pTri->m_vVert[nVert].u = 0 ;
				pTri->m_vVert[nVert].v = 0 ; 
			}
		}
		return true;
	}

	assert(group.maxUV.u > group.minUV.u);
	assert(group.maxUV.v > group.minUV.v);

//	const float DilateScale	=	static_cast<f32>(DILATE_SPACE_POSTEFFECT_TEXSIZE-DILATE_SPACE_POSTEFFECT*2)/static_cast<f32>(DILATE_SPACE_POSTEFFECT_TEXSIZE);
//	const float DilateAdd		=	static_cast<f32>(DILATE_SPACE_POSTEFFECT)/static_cast<f32>(DILATE_SPACE_POSTEFFECT_TEXSIZE);

	for (int32 i = 0; i < group.m_nTriNumber; ++i)
	{
		CRLTriangle* pTri = &group.arrTriangles[i];

		// CRLMesh works with CRLTriangle only, dynamic_cast is acceptable
		for (int32 nVert=0; nVert<=2; nVert++)
		{
			//actual normalization
			pTri->m_vVert[nVert].u = (pTri->m_vVert[nVert].u - group.minUV.u) / fDeltaU ;
			pTri->m_vVert[nVert].v = (pTri->m_vVert[nVert].v - group.minUV.v) / fDeltaV ; 
			assert(pTri->m_vVert[nVert].u>=0.0f && pTri->m_vVert[nVert].u<=1.0f);
			assert(pTri->m_vVert[nVert].v>=0.0f && pTri->m_vVert[nVert].v<=1.0f);
//			pTri->m_vVert[nVert].u = pTri->m_vVert[nVert].u*DilateScale+DilateAdd;
//			pTri->m_vVert[nVert].v = pTri->m_vVert[nVert].v*DilateScale+DilateAdd;
//			assert(pTri->m_vVert[nVert].u>=0.0f && pTri->m_vVert[nVert].u<=1.0f);
//			assert(pTri->m_vVert[nVert].v>=0.0f && pTri->m_vVert[nVert].v<=1.0f);
		}
	}

	return true;
}



//===================================================================================
// Method				:	CheckValidSize
// Description	:	Try the mesh groups are smaller then the maximum valid size
//===================================================================================
bool CRLMesh::CheckValidSize( const f32 fQualityModifier )
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	int32 nWidth;
	int32 nHeight;
	if( sceneCtx.m_numComponents == 16 )
	{
		nWidth = sceneCtx.m_nLightmapPageWidth/2;
		nHeight = sceneCtx.m_nLightmapPageHeight/2;
	}
	else
	{
		nWidth = sceneCtx.m_nLightmapPageWidth;
		nHeight = sceneCtx.m_nLightmapPageHeight;
	}

	//none of a group can be bigger than the quater of the whole page
	nWidth /= 2;
	nHeight /= 2;

	f32 fLumenPerUnitU = sceneCtx.m_fLumenPerUnitU * m_fLightmapQuality*fQualityModifier;
	f32 fLumenPerUnitV = sceneCtx.m_fLumenPerUnitV * m_fLightmapQuality*fQualityModifier;

	for(int16 nGroup = 0; nGroup < m_numUnwrapGroups; nGroup++)
	{
		if( false == m_pUnwrapGroups[nGroup].CheckRasterSize(fLumenPerUnitU, fLumenPerUnitV, nWidth,nHeight) )
			return false;
	}

	return true;
}

//===================================================================================
// Method				:	RasterizeValidPixels
// Description	:	Rasterize the mesh groups as valid pixels
//===================================================================================
bool CRLMesh::RasterizeValidPixels( const f32 fQualityModifier, FILE *pCacheFile, int32 &nCacheFilePosition )
{
	float x[3];
	float y[3];
	//std::vector<CRLTriangle>::iterator itTri;
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	f32 fLumenPerUnitU = CSceneContext::GetInstance().m_fLumenPerUnitU * m_fLightmapQuality*fQualityModifier;
	f32 fLumenPerUnitV = CSceneContext::GetInstance().m_fLumenPerUnitV * m_fLightmapQuality*fQualityModifier;

	for(int16 nGroup = 0; nGroup < m_numUnwrapGroups; nGroup++)
	{
		CRLUnwrapGroup* pGroup = &m_pUnwrapGroups[nGroup];

		//new memory allocation scheme: per group.
		if( false == pGroup->SetRasterSize_NoMaps(fLumenPerUnitU, fLumenPerUnitV) )
		{
			//todo: error message
			assert(0);
			return false;
		}
		// Clear the memory
		pGroup->ClearRaster();

		//register the used maximum used occlusion channel number
		pGroup->SetMaximumOcclusionChannelNumber( sceneCtx.m_numComponents );

		//Rasterize
		CSimpleTriangleRasterizer Rasterizer( pGroup->iWidth, pGroup->iHeight );
		CValidPixelFill ValidPixelFill( pGroup );

		for (int32 i = 0; i < pGroup->m_nTriNumber; ++i)
		{
			CRLTriangle* pTri = &pGroup->arrTriangles[i];

			x[0] = pTri->m_vVert[0].u*pGroup->m_fWidth;
			x[1] = pTri->m_vVert[1].u*pGroup->m_fWidth;
			x[2] = pTri->m_vVert[2].u*pGroup->m_fWidth;
			y[0] = pTri->m_vVert[0].v*pGroup->m_fHeight;
			y[1] = pTri->m_vVert[1].v*pGroup->m_fHeight;
			y[2] = pTri->m_vVert[2].v*pGroup->m_fHeight;

			ExtendCoordinates( x, y );
			Rasterizer.CallbackFillConservative( x, y, &ValidPixelFill );
		}

		//dilate...
		//pGroup->Blur();

		//generate span buffer
		if( false == pGroup->GenerateSpanBuffer() )
		{
			//todo: error message
			assert(0);
			return false;
		}

		//save to HDD temporaly & free the memory... (NOTE: if not all the unwrapgroup from a mesh try to put into 1 surface
		//we can directly render to surface easily, and not need the hdd at all.)
		if( false == pGroup->Save(pCacheFile, nCacheFilePosition ) )
		{
			//todo: error message
			assert(0);
			return false;
		}

		pGroup->ReleaseMemory();

		//debug only?
		//char szFile[128];
		//sprintf(szFile, "SpanBuffer_%i_%i.jpg",(uint32) (this),(uint32) (pGroup->m_pSpanBuffer[0]) );
		//pGroup->m_pSpanBuffer[0]->GenerateDebugJPG( szFile );
	}
	return true;
}

//===================================================================================
// Method				:	ReadBack_Debug
// Description	:	Debuging helper function - you can skip the rendering - 
//===================================================================================
bool CRLMesh::ReadBack_Debug( CRLWaitProgress* pWaitProgress, FILE *pCacheFile, int32 &nCacheFilePosition )
{
	f32 fLumenPerUnitU = CSceneContext::GetInstance().m_fLumenPerUnitU * m_fLightmapQuality;
	f32 fLumenPerUnitV = CSceneContext::GetInstance().m_fLumenPerUnitV * m_fLightmapQuality;

	for(int16 nGroup = 0; nGroup < m_numUnwrapGroups; nGroup++)
	{
		if( false == m_pUnwrapGroups[nGroup].SetRasterSize(fLumenPerUnitU, fLumenPerUnitV) )
		{
			//todo: error message
			assert(0);
			return false;
		}

		m_pUnwrapGroups[nGroup].CaculateFilePosition_Debug( nCacheFilePosition );
		m_pUnwrapGroups[nGroup].Load( pCacheFile );

		//generate span buffer
		if( false == m_pUnwrapGroups[nGroup].GenerateSpanBuffer() )
		{
			//todo: error message
			assert(0);
			return false;
		}

		//free up the memory
		m_pUnwrapGroups[nGroup].ReleaseMemory();

		pWaitProgress->SetProgress((f32)(nGroup+1)/(f32)m_numUnwrapGroups);
	}

	return true;
}

//===================================================================================
// Method				:	RasterizeDebugCilor
// Description	:	Rasterize the mesh groups as debug colored surface
//===================================================================================
bool CRLMesh::RasterizeDebugColor( CRLWaitProgress* pWaitProgress, FILE *pCacheFile, int32 &nCacheFilePosition, const bool bGenerateRandomColors, t_pairEntityId &OcclusionLightIDs, int32 nOcclLightNum )
{
	f32 x[3];
	f32 y[3];
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	Vec3 vDebugColor(1,1,1 );

	f32 fLumenPerUnitU = CSceneContext::GetInstance().m_fLumenPerUnitU * m_fLightmapQuality;
	f32 fLumenPerUnitV = CSceneContext::GetInstance().m_fLumenPerUnitV * m_fLightmapQuality;

	for(int16 nGroup = 0; nGroup < m_numUnwrapGroups; nGroup++)
	{
		CRLUnwrapGroup* pGroup = &m_pUnwrapGroups[nGroup];

		//new memory allocation scheme: per group.
		if( false == pGroup->SetRasterSize(fLumenPerUnitU, fLumenPerUnitV) )
		{
			//todo: error message
			assert(0);
			return false;
		}
		// Clear the memory
		pGroup->ClearRaster();

		//rasterize
		if( bGenerateRandomColors )
			vDebugColor.Set( ((float)(rand() & 255)) / 255.f, ((float)(rand() & 255)) / 255.f, ((float)(rand() & 255)) / 255.f );

		//register the used maximum used occlusion channel number
		pGroup->SetMaximumOcclusionChannelNumber( nOcclLightNum );

		CSimpleTriangleRasterizer Rasterizer( pGroup->iWidth, pGroup->iHeight );
		CDebugColorFill DebugColorFill( pGroup, vDebugColor );

		for (int32 i = 0; i < pGroup->m_nTriNumber; ++i)
		{
			CRLTriangle* pTri = &pGroup->arrTriangles[i];

			x[0] = pTri->m_vVert[0].u*pGroup->m_fWidth;
			x[1] = pTri->m_vVert[1].u*pGroup->m_fWidth;
			x[2] = pTri->m_vVert[2].u*pGroup->m_fWidth;
			y[0] = pTri->m_vVert[0].v*pGroup->m_fHeight;
			y[1] = pTri->m_vVert[1].v*pGroup->m_fHeight;
			y[2] = pTri->m_vVert[2].v*pGroup->m_fHeight;

			ExtendCoordinates( x, y );
			Rasterizer.CallbackFillConservative( x, y, &DebugColorFill );
		}

		//dilate...
		//pGroup->Dilate();

		//generate span buffer
		if( false == pGroup->GenerateSpanBuffer() )
		{
			//todo: error message
			assert(0);
			return false;
		}

		//save to HDD temporaly & free the memory... (NOTE: if not all the unwrapgroup from a mesh try to put into 1 surface
		//we can directly render to surface easily, and not need the hdd at all.)
		if( false == pGroup->Save(pCacheFile, nCacheFilePosition ) )
		{
			//todo: error message
			assert(0);
			return false;
		}


		//free up the memory
		pGroup->ReleaseMemory();

		pWaitProgress->SetProgress((f32)(nGroup+1)/(f32)m_numUnwrapGroups);
	}
	return true;
}


//===================================================================================
// Method				:	RasterizeGroup_ValidPixel
// Description	:	Rasterize a group as valid pixels
//===================================================================================
bool CRLMesh::RasterizeGroup_ValidPixel( CRLUnwrapGroup* pGroup,  FILE *pCacheFile, int32 &nCacheFilePosition, const int32 nVisAreaID )
{
	float x[3];
	float y[3];
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	f32 fLumenPerUnitU = CSceneContext::GetInstance().m_fLumenPerUnitU * m_fLightmapQuality;
	f32 fLumenPerUnitV = CSceneContext::GetInstance().m_fLumenPerUnitV * m_fLightmapQuality;

	//new memory allocation scheme: per group.
	if( false == pGroup->SetRasterSize_NoMaps(fLumenPerUnitU, fLumenPerUnitV) )
	{
		//todo: error message
		assert(0);
		return false;
	}

	pGroup->m_nOctreeID = nVisAreaID;
	for( int32 i = 0; i < 16; ++i )
		pGroup->m_LightCoordiates[i] = Vec4(-FLT_MAX,-FLT_MAX,-FLT_MAX,0);

	// Clear the memory
	pGroup->ClearRaster();

	//register the used maximum used occlusion channel number
	pGroup->SetMaximumOcclusionChannelNumber( 1 );

	//Rasterize
	CSimpleTriangleRasterizer Rasterizer( pGroup->iWidth, pGroup->iHeight );
	CValidPixelFill VPixelFill( pGroup  );

	//first pass: a border around the triangle
	for (int32 i = 0; i < pGroup->m_nTriNumber; ++i)
	{
		CRLTriangle* pTri = &pGroup->arrTriangles[i];

		x[0] = pTri->m_vVert[0].u*pGroup->m_fWidth;
		x[1] = pTri->m_vVert[1].u*pGroup->m_fWidth;
		x[2] = pTri->m_vVert[2].u*pGroup->m_fWidth;
		y[0] = pTri->m_vVert[0].v*pGroup->m_fHeight;
		y[1] = pTri->m_vVert[1].v*pGroup->m_fHeight;
		y[2] = pTri->m_vVert[2].v*pGroup->m_fHeight;

		ExtendCoordinates( x, y );
		Rasterizer.CallbackFillConservative( x, y, &VPixelFill );
	}

	//generate span buffer
	if( false == pGroup->GenerateSpanBuffer() )
	{
		//todo: error message
		assert(0);
		return false;
	}

	//save to HDD temporaly & free the memory... (NOTE: if not all the unwrapgroup from a mesh try to put into 1 surface
	//we can directly render to surface easily, and not need the hdd at all.)
	if( false == pGroup->Save(pCacheFile, nCacheFilePosition ) )
	{
		//todo: error message
		assert(0);
		return false;
	}
	//free up the memory
	pGroup->ReleaseMemory();
	return true;
}

//===================================================================================
// Method				:	RasterizeGroup
// Description	:	Rasterize a group as lightmap / occlusionmap / rae
//===================================================================================
bool CRLMesh::RasterizeGroup( CRLUnwrapGroup* pGroup, CIlluminanceIntegrator* pDirectIntegrator, CIlluminanceIntegrator* pIndirectIntegrator,
														CIlluminanceIntegrator* pSunIntegrator, CIlluminanceIntegrator* pOcclusionIntegrator,  CIlluminanceIntegrator* pAmbientOcclusionIntegrator,
														CRLWaitProgress* pWaitProgress, FILE *pCacheFile, int32 &nCacheFilePosition, t_pairEntityId *LightmapLightIDs, t_pairEntityId *OcclusionLightIDs, int32 nOcclLightNum, const int32 nVisAreaID, bool bGenerateSpanBuffer )
{
	float x[3];
	float y[3];
	CRLTriangle* pTri;
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	f32 fLumenPerUnitU = CSceneContext::GetInstance().m_fLumenPerUnitU * m_fLightmapQuality;
	f32 fLumenPerUnitV = CSceneContext::GetInstance().m_fLumenPerUnitV * m_fLightmapQuality;

	CIntersectionCacheCluster IntersectionCache;
	IntersectionCache.Init(10);

	COctreeCell* pBoundingCell = NULL;
	if( pAmbientOcclusionIntegrator )
	{
		Sphere Sph( m_BBox.GetCenter(), m_BBox.GetRadius() + sceneCtx.m_fMaxAmbientOcclusionSearchRadius );
		m_pGeomCore->SearchRoot(&pBoundingCell, Sph);
	}

	const PodArray<CDLight*>* pListLights	=	sceneCtx.GetLights();

	//new memory allocation scheme: per group.
	if( false == pGroup->SetRasterSize(fLumenPerUnitU, fLumenPerUnitV) )
	{
		//todo: error message
		assert(0);
		return false;
	}

	pGroup->m_nOctreeID = nVisAreaID;
	for( int32 i = 0; i < nOcclLightNum; ++i )
	{
		CDLight* pLight	=	pListLights->GetAt( (*OcclusionLightIDs)[i].second );
		pGroup->m_LightCoordiates[i] = Vec4( pLight->m_BaseOrigin.x, pLight->m_BaseOrigin.y,pLight->m_BaseOrigin.z, pLight->m_fRadius );
	}
	for( int32 i = nOcclLightNum; i < 16; ++i )
		pGroup->m_LightCoordiates[i] = Vec4(-FLT_MAX,-FLT_MAX,-FLT_MAX,0);

	// Clear the memory
	pGroup->ClearRaster();

	//register the used maximum used occlusion channel number
	pGroup->SetMaximumOcclusionChannelNumber( nOcclLightNum );

	///recalculate if necessary
	CalcOffsetSamples();

	//Rasterize
	CSimpleTriangleRasterizer Rasterizer( pGroup->iWidth+2*DILATE_SPACE_POSTEFFECT, pGroup->iHeight+2*DILATE_SPACE_POSTEFFECT );
	CLightmapFill LightmapFill( pGroup, pDirectIntegrator, pIndirectIntegrator, pSunIntegrator, pOcclusionIntegrator, pAmbientOcclusionIntegrator, LightmapLightIDs, OcclusionLightIDs, nOcclLightNum, &IntersectionCache  );

	//first pass: a border around the triangle
	for (int32 i = 0; i < pGroup->m_nTriNumber; ++i)
	{
		pTri = &pGroup->arrTriangles[i];

		if(!pTri->m_nIsDilated)
		{
			f32 AddX	=	DILATE_SPACE_POSTEFFECT/pGroup->m_fWidth;
			f32 AddY	=	DILATE_SPACE_POSTEFFECT/pGroup->m_fHeight;
			f32 ScaleX	=	(pGroup->m_fWidth-DILATE_SPACE_POSTEFFECT*2)/pGroup->m_fWidth;
			f32 ScaleY	=	(pGroup->m_fHeight-DILATE_SPACE_POSTEFFECT*2)/pGroup->m_fHeight;
			if(ScaleX<0.f)
				ScaleX	=	0.f;
			if(ScaleY<0.f)
				ScaleY	=	0.f;
			if(AddX>0.5*pGroup->m_fWidth)
				AddX	=	0.5*pGroup->m_fWidth;
			if(AddY>0.5*pGroup->m_fHeight)
				AddY	=	0.5*pGroup->m_fHeight;

			pTri->m_vVert[0].u	*=	ScaleX;
			pTri->m_vVert[1].u	*=	ScaleX;
			pTri->m_vVert[2].u	*=	ScaleX;
			pTri->m_vVert[0].v	*=	ScaleY;
			pTri->m_vVert[1].v	*=	ScaleY;
			pTri->m_vVert[2].v	*=	ScaleY;
			pTri->m_vVert[0].u	+=	AddX;
			pTri->m_vVert[1].u	+=	AddX;
			pTri->m_vVert[2].u	+=	AddX;
			pTri->m_vVert[0].v	+=	AddY;
			pTri->m_vVert[1].v	+=	AddY;
			pTri->m_vVert[2].v	+=	AddY;
			pTri->m_nIsDilated	=	1;
			assert(pTri->m_vVert[0].u >=0.0f && pTri->m_vVert[0].u <=1.0f);
			assert(pTri->m_vVert[0].v >=0.0f && pTri->m_vVert[0].v <=1.0f);
			assert(pTri->m_vVert[1].u >=0.0f && pTri->m_vVert[1].u <=1.0f);
			assert(pTri->m_vVert[1].v >=0.0f && pTri->m_vVert[1].v <=1.0f);
			assert(pTri->m_vVert[2].u >=0.0f && pTri->m_vVert[2].u <=1.0f);
			assert(pTri->m_vVert[2].v >=0.0f && pTri->m_vVert[2].v <=1.0f);
		}
		else
		{
			assert(0);
		}


		x[0] = pTri->m_vVert[0].u*pGroup->m_fWidth;
		x[1] = pTri->m_vVert[1].u*pGroup->m_fWidth;
		x[2] = pTri->m_vVert[2].u*pGroup->m_fWidth;
		y[0] = pTri->m_vVert[0].v*pGroup->m_fHeight;
		y[1] = pTri->m_vVert[1].v*pGroup->m_fHeight;
		y[2] = pTri->m_vVert[2].v*pGroup->m_fHeight;

		if( LightmapFill.SetupTriangle( *pTri, x,y, pBoundingCell ) )
		{
//			ExtendCoordinates( x, y );
//			Rasterizer.CallbackFillConservative( x, y, &LightmapFill );
			Rasterizer.CallbackFillSubpixelCorrect(x,y,&LightmapFill);
		}
	}

	//Blur...
	//pGroup->Blur();

	//try to make smaller surface...
	for(uint a=0;a<DILATE_SPACE_POSTEFFECT;a++)
		pGroup->Dilate(); 
	pGroup->Pack(); 

	//generate span buffer
	if( bGenerateSpanBuffer)
		if( false == pGroup->GenerateSpanBuffer() )
		{
			//todo: error message
			assert(0);
			return false;
		}
/*
	//save to HDD temporaly & free the memory... (NOTE: if not all the unwrapgroup from a mesh try to put into 1 surface
	//we can directly render to surface easily, and not need the hdd at all.)
	if( false == pGroup->Save(pCacheFile, nCacheFilePosition ) )
	{
		//todo: error message
		assert(0);
		return false;
	}
	//free up the memory
	pGroup->ReleaseMemory();
*/
	return true;
}


//===================================================================================
// Method				:	RasterizeMesh
// Description	:	Rasterize the mesh groups as lightmap / occlusionmap
//===================================================================================
bool CRLMesh::RasterizeMesh(CIlluminanceIntegrator* pDirectIntegrator, CIlluminanceIntegrator* pIndirectIntegrator,
														CIlluminanceIntegrator* pSunIntegrator, CIlluminanceIntegrator* pOcclusionIntegrator,  CIlluminanceIntegrator* pAmbientOcclusionIntegrator,
														CRLWaitProgress* pWaitProgress, FILE *pCacheFile, int32 &nCacheFilePosition, t_pairEntityId *LightmapLightIDs, t_pairEntityId *OcclusionLightIDs, int32 nOcclLightNum, const int32 nVisAreaID )
{
	if( CSceneContext::GetInstance().m_bDistributedMap )
	{
		for(int16 nGroup = 0; nGroup < m_numUnwrapGroups; nGroup++)
		{
			pWaitProgress->SetProgress((f32)(nGroup+1)/(f32)m_numUnwrapGroups);

			CRLUnwrapGroup* pGroup = &m_pUnwrapGroups[nGroup];
			//new memory allocation scheme: per group.
			if( false == RasterizeGroup_ValidPixel( pGroup, pCacheFile, nCacheFilePosition, nVisAreaID ) )
				return false;
		}
	}
	else
	{
		for(int16 nGroup = 0; nGroup < m_numUnwrapGroups; nGroup++)
		{
			pWaitProgress->SetProgress((f32)(nGroup+1)/(f32)m_numUnwrapGroups);

			CRLUnwrapGroup* pGroup = &m_pUnwrapGroups[nGroup];
			//new memory allocation scheme: per group.
			if( false == RasterizeGroup( pGroup, pDirectIntegrator, pDirectIntegrator, pSunIntegrator, pOcclusionIntegrator, pAmbientOcclusionIntegrator,
																	 pWaitProgress, pCacheFile, nCacheFilePosition, LightmapLightIDs, OcclusionLightIDs, nOcclLightNum, nVisAreaID, true ) )
				return false;
		}
	}


	return true;
}

//===================================================================================
// Method				:	ReleaseAllGroupSpanBuffer
// Description	:	Free up the memory used by span buffers of the mesh groups
//===================================================================================
void CRLMesh::ReleaseAllGroupSpanBuffer()
{
	for(int16 nGroup = 0; nGroup < m_numUnwrapGroups; nGroup++)
	{
		m_pUnwrapGroups[nGroup].ClearBuffers();
	}
}

//===================================================================================
// Method				:	ReleaseAllGroup
// Description	:	Free up the memory used by the mesh groups
//===================================================================================
void CRLMesh::ReleaseAllGroup()
{
	m_numUnwrapGroups = 0;
	SAFE_DELETE_ARRAY( m_pUnwrapGroups );
}

//===================================================================================
// Method				:	GetUnwrapGroups
// Description	:	Give back the pointer of the array of the groups
//===================================================================================
CRLUnwrapGroup* CRLMesh::GetUnwrapGroups()
{
	return m_pUnwrapGroups;
}

//===================================================================================
// Method				:	GetUnwrapGroupsNum
// Description	:	Give back the number of the groups of the mesh
//===================================================================================
int16 CRLMesh::GetUnwrapGroupsNum()
{
	return m_numUnwrapGroups;
}


//===================================================================================
// Method				:	CalcOffsetSamples
// Description	:	calculate samples' offsets for supersampling
//===================================================================================
void CRLMesh::CalcOffsetSamples()
{

	//is it prepeared
	if (m_bSamplesCache)
		return;

	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	//ignore Super-Sampling
	if (!sceneCtx.m_bUseSuperSampling)
	{
		m_pvOffsetSamples = new Vec2[1];
		assert(m_pvOffsetSamples!=NULL);

		m_pvOffsetSamples[0].x =  0.0; m_pvOffsetSamples[0].y =  0.0;
		m_numOffsetSamples = 1;
		m_bSamplesCache = true;
		return;
	}


	//prepare samples for super-sampling
  m_numOffsetSamples = sceneCtx.m_numJitterSamples;

	// Figure out if we need extra samples.
	int32 nExtraSamples = 1; // center texel
	if (true/*gAddTexelCorners*/) //fix
	{
 		nExtraSamples += 4; // corners.
	}

	// Figure out how many samples we really need.
	m_numOffsetSamples++;
	int32 num = nExtraSamples + (m_numOffsetSamples*m_numOffsetSamples);
	if (m_numOffsetSamples == 1)
	{
		num = nExtraSamples;
	}

	// Allocate sample memory
	m_pvOffsetSamples = new Vec2[num];
	if ((*m_pvOffsetSamples) == NULL)
	{
		CLogFile::WriteLine("Can not allocate offsets for super-sampling\n");
		return;
	}

	m_pvOffsetSamples[0].x =  0.0; m_pvOffsetSamples[0].y =  0.0;
	if (true/*gAddTexelCorners*/) //fix
	{
		/*m_pvOffsetSamples[1].x =  0.5; m_pvOffsetSamples[1].y =  0.5;
		m_pvOffsetSamples[2].x =  0.5; m_pvOffsetSamples[2].y = -0.5;
		m_pvOffsetSamples[3].x = -0.5; m_pvOffsetSamples[3].y =  0.5;
		m_pvOffsetSamples[4].x = -0.5; m_pvOffsetSamples[4].y = -0.5;*/

/*		m_pvOffsetSamples[1].x =  1.0; m_pvOffsetSamples[1].y =  1.0;  // caused errors and useless sampling of neighbour pixels that are sampled as well
		m_pvOffsetSamples[2].x =  1.0; m_pvOffsetSamples[2].y = -1.0;
		m_pvOffsetSamples[3].x = -1.0; m_pvOffsetSamples[3].y =  1.0;
		m_pvOffsetSamples[4].x = -1.0; m_pvOffsetSamples[4].y = -1.0;    */

		m_pvOffsetSamples[1].x =  0.33f; m_pvOffsetSamples[1].y =  0.33f;
		m_pvOffsetSamples[2].x =  0.33f; m_pvOffsetSamples[2].y = -0.33f;
		m_pvOffsetSamples[3].x = -0.33f; m_pvOffsetSamples[3].y =  0.33f;
		m_pvOffsetSamples[4].x = -0.33f; m_pvOffsetSamples[4].y = -0.33f;
	}

	//grid  - 	//fix replace by jittering
	int32 idx = nExtraSamples;
	if (m_numOffsetSamples > 1)
	{
		for (int32 y = 0; y < m_numOffsetSamples; y++)
		{
			f32 sy = ((1.0/((f32)m_numOffsetSamples + 1.0))*((f32)(y) + 1.0)) - 0.5;
			sy *= 2.0; 

			for (int32 x = 0; x < m_numOffsetSamples; x++)
			{
				f32 sx = ((1.0/((f32)m_numOffsetSamples + 1.0))*((f32)(x) + 1.0))- 0.5;
				sx *= 2.0; 

				m_pvOffsetSamples[idx].x = sx;
				m_pvOffsetSamples[idx].y = sy;
				idx++;
			}
		}
	}

	assert(idx == num);

	num	=5;
	// Update number of samples
	m_numOffsetSamples = num;

	m_bSamplesCache = true;

	return;
}
