/*=============================================================================
AreaLight.h : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef AREALIGHT_H
#define AREALIGHT_H

#if _MSC_VER > 1000
# pragma once
#endif

#include "Cry_Math.h"
#include "IShader.h"
#include "IllumHandler.h"
#include "IntersectionCacheCluster.h"
#include "GeomCore.h"
#include "SceneContext.h"


// Small helper functions to get information from lights. Because just the lightmap compiler use this function
// not implemented inside the CDLight, just here.
class CAreaLight
{
public:

	//Give you back a virutal point light inside the light
	static void	GetRandomPosition( Vec3 &vResult, const CDLight *pLight )
	{
		assert( pLight );
		if( NULL == pLight )
		{
				vResult.Set(0,0,0);
				return;
		}

		switch( pLight->m_AreaLightType )
		{
			case DLAT_POINT:
				vResult = pLight->m_BaseOrigin;
				break;
			case DLAT_SPHERE:
			{
				Vec3 vX( 1, 0, 0 );
				Vec3 vY( 0, 1, 0 );
				Vec3 vZ( 0, 0, 1 );

				pLight->m_ObjMatrix.TransformVector( vX );
				pLight->m_ObjMatrix.TransformVector( vY );
				pLight->m_ObjMatrix.TransformVector( vZ );

				vResult.SetRandomDirection();
				vX *= vResult.x;
				vY *= vResult.y;
				vZ *= vResult.z;
				vResult = vX+vY+vZ+pLight->m_BaseOrigin;
			}//end of sphere
			break;
			case DLAT_RECTANGLE:
			{//Rectangle
				//I use the light ObjMatrix to determine the 3 direction...
				Vec3 vX( 1, 0, 0 );
				Vec3 vY( 0, 1, 0 );
				Vec3 vZ( 0, 0, 1 );

				pLight->m_ObjMatrix.TransformVector( vX );
				pLight->m_ObjMatrix.TransformVector( vY );
				pLight->m_ObjMatrix.TransformVector( vZ );
	/*
				//remove the scaleing
				vRadius.x = vX.NormalizeSafe();
				vRadius.y = vY.NormalizeSafe();
				vRadius.z = vZ.NormalizeSafe();

				vX *= ((f32)rand()/RAND_MAX-0.5f)*pLight->m_vAreaSize.x;
				vY *= ((f32)rand()/RAND_MAX-0.5f)*pLight->m_vAreaSize.y;
				vZ *= ((f32)rand()/RAND_MAX-0.5f)*pLight->m_vAreaSize.z;
	*/
				vX *= ((f32)rand()/RAND_MAX-0.5f);
				vY *= ((f32)rand()/RAND_MAX-0.5f);
				vZ *= ((f32)rand()/RAND_MAX-0.5f);
				vResult = vX;
				vResult += vY;
				vResult += vZ;
				vResult += pLight->m_BaseOrigin;
			}//end of Rectangle
			break;
		}
	}


	static f32 GetLightAttenuation( const CDLight *pLight, const Vec3& vVector )
	{
		assert( pLight );
		if( NULL == pLight )
			return 0.f;

		float fSqrDist;
		if( DLAT_RECTANGLE == pLight->m_AreaLightType )
		{
			fSqrDist = 0;
			Vec3 vX( 1, 0, 0 );
			Vec3 vY( 0, 1, 0 );
			Vec3 vZ( 0, 0, 1 );
			Vec3 vRadius;

			pLight->m_ObjMatrix.TransformVector( vX );
			pLight->m_ObjMatrix.TransformVector( vY );
			pLight->m_ObjMatrix.TransformVector( vZ );

			//remove the scaleing
			vRadius.x = vX.NormalizeSafe();
			vRadius.y = vY.NormalizeSafe();
			vRadius.z = vZ.NormalizeSafe();

			f32 fDelta;
			
			Vec3 vClosest( vVector.Dot( vX ),vVector.Dot( vY ), vVector.Dot( vZ ) );
			Vec3 vExtent( vRadius.x * 0.5f, vRadius.y * 0.5f,	vRadius.z * 0.5f );

			if( vClosest.x < -vExtent.x )
			{
				fDelta = vClosest.x + vExtent.x;
				fSqrDist += fDelta*fDelta;
			}
			else
				if( vClosest.x > vExtent.x ) 
				{
					fDelta = vClosest.x - vExtent.x;
					fSqrDist += fDelta*fDelta;
				}

			if( vClosest.y < -vExtent.y )
			{
				fDelta = vClosest.y + vExtent.y;
				fSqrDist += fDelta*fDelta;
			}
			else
				if( vClosest.y > vExtent.y ) 
				{
					fDelta = vClosest.y - vExtent.y;
					fSqrDist += fDelta*fDelta;
				}

			if( vClosest.z < -vExtent.z )
			{
				fDelta = vClosest.z + vExtent.z;
				fSqrDist += fDelta*fDelta;
			}
			else
				if( vClosest.z > vExtent.z ) 
				{
					fDelta = vClosest.z - vExtent.z;
					fSqrDist += fDelta*fDelta;
				}
		}//RECTANGLE
		else
			fSqrDist = vVector.GetLengthSquared();


		f32 fAttenuation;
		//based on light attenuation function...
		if( pLight-> m_bLightmapLinearAttenuation  )
			fAttenuation = 1.f - min( sqrt_tpl( fSqrDist), pLight->m_fRadius ) / pLight->m_fRadius;
		else
			fAttenuation = pLight->m_fRadius / fSqrDist;


		//Spot lights
		Vec3 vDir( vVector);
		vDir.normalize();
		f32 fCa = pLight->m_vSpotDirection.Dot( vDir );
		f32 fInCos = cosf( pLight->m_fInAngle );
		f32 fOutCos = cosf( pLight->m_fOutAngle );

		//smoothstep...	
		if( fCa < fOutCos )
			fAttenuation = 0;
		else
			if( fCa < fInCos )
			{
				f32 fTemp  = ( fCa - fOutCos ) / ( fInCos - fOutCos );
				fAttenuation *= fTemp*fTemp*(3-2*fTemp);
			}

		return fAttenuation;
	}
	static void Init()
	{
		CSceneContext& sceneCtx = CSceneContext::GetInstance();
		if( sceneCtx.m_fLumenPerUnitU != 0 )
			m_fOnePixel = 1.f / sceneCtx.m_fLumenPerUnitU;
		else
			m_fOnePixel = 0.f;
		pGeomCore		=	sceneCtx.GetGeomCore();
		m_bWhite		= (sceneCtx.m_numDirectSamples == 0);
	}

	static bool GetVisibility( const CDLight *pLight, const Vec3& vPosition, const Vec3& vNormal, f32 &fVisibility, CIntersectionCacheCluster *pIntersectionCache )
	{
		if( NULL == pLight )
			return false;

		f32 fDist = pLight->m_fRadius+m_fOnePixel;
		//test the maximum distance
		Vec3 vRealtimeLightDir(vPosition);
		vRealtimeLightDir-=pLight->m_BaseOrigin;
		if( vRealtimeLightDir.GetLengthSquared() > fDist*fDist )
			return false;

		//test realtime light cos value...  Problem with the perpixel normals
//		if( vRealtimeLightDir.Dot( vNormal ) > 0 )
//				return false;

		if( m_bWhite )
		{
			fVisibility = 1.f;
			return true;
		}

		fVisibility = 0;

		//calculate the visibility factor
		switch( pLight->m_AreaLightType )
		{
		case DLAT_POINT:
			{
				CRay Ray;
				Ray.vFrom = vPosition;

				Ray.vDir = pLight->m_BaseOrigin;
				Ray.vDir -= vPosition;
				f32 fDist			= Ray.vDir.NormalizeSafe();

//				if( Ray.vDir.Dot( vNormal ) >= 0 )
				{
					Ray.fT					=	fDist;
					Ray.vOrigNormal = vNormal;
					if (!pGeomCore->ShadowIntersect(&Ray, pIntersectionCache))
						fVisibility = 1;
				}
			}
			break;
		case DLAT_SPHERE:
			{
				int32 iM1 = (int32)sqrtf(pLight->m_nAreaSampleNumber);
				iM1 = iM1 > 0 ? iM1 : 1;
				int32 iM2 = pLight->m_nAreaSampleNumber / iM1;
				iM2 = iM2 > 0 ? iM2 : 1;

				CRay Ray;
				Ray.vFrom = vPosition;

				Vec3 vX( 1, 0, 0 );
				Vec3 vY( 0, 1, 0 );
				Vec3 vZ( 0, 0, 1 );
				Vec3 vRadius;

				pLight->m_ObjMatrix.TransformVector( vX );
				pLight->m_ObjMatrix.TransformVector( vY );
				pLight->m_ObjMatrix.TransformVector( vZ );

				vRadius.x = vX.NormalizeSafe();
				vRadius.y = vY.NormalizeSafe();
				vRadius.z = vZ.NormalizeSafe();

				f32 fWeightSum = 1.f / (f32)(iM1*iM2);
				for( int32 i = 0; i < iM1; ++i )
					for( int32 j = 0; j < iM2; ++j )
					{
						Vec3 vLocalRadius( vRadius.x * (f32)rand()/RAND_MAX, vRadius.y * (f32)rand()/RAND_MAX, vRadius.z * (f32)rand()/RAND_MAX );

						f32 fPhi = gf_PI2*( i +  (f32)rand()/RAND_MAX ) / (f32) iM1;
						f32 fTheta = gf_PI*( j +  (f32)rand()/RAND_MAX ) / (f32) iM2;
						f32 fSinTheta = sinf(fTheta);

						Ray.vDir.x = sinf( fPhi ) * fSinTheta * vLocalRadius.x + pLight->m_BaseOrigin.x;
						Ray.vDir.y = cosf( fTheta )  * vLocalRadius.y + pLight->m_BaseOrigin.y;
						Ray.vDir.z = cosf( fPhi ) * fSinTheta * vLocalRadius.z + pLight->m_BaseOrigin.z;
						Ray.vDir -= vPosition;
						f32 fDist			= Ray.vDir.NormalizeSafe();

//						if( Ray.vDir.Dot( vNormal ) >= 0 )
						{
							Ray.fT					=	fDist;
							Ray.vOrigNormal = vNormal;
							if (!pGeomCore->ShadowIntersect(&Ray, pIntersectionCache))
								fVisibility += fWeightSum;
						}
					}//j,i
			}//DLAT_SPHERE
			break;
		case DLAT_RECTANGLE:
			{//Rectangle
				CRay Ray;
				Ray.vFrom = vPosition;

				int32 iSampleNumber =pLight->m_nAreaSampleNumber;
				f32 fWeightSum = 1.f / (f32)(iSampleNumber);
				for( int32 i = 0; i < iSampleNumber; ++i )
				{
					GetRandomPosition( Ray.vDir,pLight );
					Ray.vDir -= Ray.vFrom;
					f32 fDist = Ray.vDir.NormalizeSafe();

					if( Ray.vDir.Dot( vNormal ) >= 0 )
					{
						Ray.fT					=	fDist;
						Ray.vOrigNormal = vNormal;
						if (!pGeomCore->ShadowIntersect(&Ray, pIntersectionCache))
							fVisibility += fWeightSum;
					}
				}//i
			}//DLAT_RECTANGLE
			break;
		}
		return true;
	}

	static void Shade( const CDLight *pLight, const Vec3 vPosition, const Vec3& vNormal, CIllumHandler &spectrum, CIntersectionCacheCluster *pIntersectionCache )
	{
		if( NULL == pLight )
			return;

		float fAttenuation = GetLightAttenuation( pLight, pLight->m_BaseOrigin - vPosition );
		if( fAttenuation <= 0.f )
			return;
		ColorF Cl =	pLight->m_BaseColor * fAttenuation;

		CSceneContext& sceneCtx = CSceneContext::GetInstance();
		CGeomCore*	pGeomCore =	sceneCtx.GetGeomCore();

		CRay Ray;
		Ray.vFrom = vPosition;

		switch( pLight->m_AreaLightType )
		{
		case DLAT_POINT:
			{
				Ray.vDir = pLight->m_BaseOrigin;
				Ray.vDir -= vPosition;
				f32 fDist			= Ray.vDir.NormalizeSafe();
				Ray.fT					=	fDist;
				Ray.vOrigNormal = vNormal;
				if( !pGeomCore->ShadowIntersect(&Ray, pIntersectionCache ))
					spectrum.Add( Vec3( Cl.r, Cl.g, Cl.b), -Ray.vDir );
			}
			break;
		case DLAT_SPHERE:
			{
				int32 iM1 = (int32)sqrtf(pLight->m_nAreaSampleNumber);
				iM1 = iM1 > 0 ? iM1 : 1;
				int32 iM2 = pLight->m_nAreaSampleNumber / iM1;
				iM2 = iM2 > 0 ? iM2 : 1;

				Vec3 vX( 1, 0, 0 );
				Vec3 vY( 0, 1, 0 );
				Vec3 vZ( 0, 0, 1 );
				Vec3 vRadius;

				pLight->m_ObjMatrix.TransformVector( vX );
				pLight->m_ObjMatrix.TransformVector( vY );
				pLight->m_ObjMatrix.TransformVector( vZ );

				vRadius.x = vX.NormalizeSafe();
				vRadius.y = vY.NormalizeSafe();
				vRadius.z = vZ.NormalizeSafe();

				f32 fWeightSum = 1.f/ (f32)(iM1*iM2);
				for( int32 i = 0; i < iM1; ++i )
					for( int32 j = 0; j < iM2; ++j )
					{
						Vec3 vLocalRadius( vRadius.x * (f32)rand()/RAND_MAX, vRadius.y * (f32)rand()/RAND_MAX, vRadius.z * (f32)rand()/RAND_MAX );

						f32 fPhi = gf_PI2*( i +  (f32)rand()/RAND_MAX ) / (f32) iM1;
						f32 fTheta = gf_PI*( j +  (f32)rand()/RAND_MAX ) / (f32) iM2;
						f32 fSinTheta = sinf(fTheta);

						Ray.vDir.x = sinf( fPhi ) * fSinTheta * vLocalRadius.x + pLight->m_BaseOrigin.x;
						Ray.vDir.y = cosf( fTheta ) * vLocalRadius.y + pLight->m_BaseOrigin.y;
						Ray.vDir.z = cosf( fPhi ) * fSinTheta * vLocalRadius.z + pLight->m_BaseOrigin.z;
						Ray.vDir -= vPosition;
						f32 fDist			= Ray.vDir.NormalizeSafe();
						Ray.fT					=	fDist;
						Ray.vOrigNormal = vNormal;
						if( !pGeomCore->ShadowIntersect(&Ray, pIntersectionCache ) )
							spectrum.Add( Vec3( Cl.r*fWeightSum, Cl.g*fWeightSum, Cl.b*fWeightSum ), -Ray.vDir );
					}//j,i
			}
			break;
		case DLAT_RECTANGLE:
			{
				int32 iSampleNumber = pLight->m_nAreaSampleNumber;
				f32 fWeightSum = 1.f/ (f32)(iSampleNumber);
				for( int32 i = 0; i < iSampleNumber; ++i )
				{
					GetRandomPosition( Ray.vDir,pLight );
					Ray.vDir -= Ray.vFrom;
					float fDist = Ray.vDir.NormalizeSafe();
					{
						Ray.fT					=	fDist;
						Ray.vOrigNormal = vNormal;
						if (!pGeomCore->ShadowIntersect(&Ray, pIntersectionCache))
							spectrum.Add( Vec3( Cl.r*fWeightSum, Cl.g*fWeightSum, Cl.b*fWeightSum ), -Ray.vDir );
					}
				}//i
			}
			break;
		}
	}

protected:
	static f32					m_fOnePixel;
	static CGeomCore*	pGeomCore;
	static bool				m_bWhite;
};

#endif