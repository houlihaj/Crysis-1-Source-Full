/*=============================================================================
RayMap_Tracer.cpp: 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#include "stdafx.h"
#include "RayMap_Tracer.h"
#include <CryArray.h>


bool CRayMap_Tracer::Trace_Position( CRayMap* pRayMap,const Vec3 vPosition, CGeomCore* pGeomCore, const bool bIndirectOnly )
{
	assert( pRayMap );
	if( NULL == pRayMap )
		return false;

	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	int32 nTraceNumber =	(int32) (g_PI*(int32) (sqrt(sceneCtx.m_numEmitPhotons/ g_PI) + 0.5) + 0.5);

	CRay Ray;
	for( int32 nSampleNum = 0; nSampleNum < nTraceNumber; ++nSampleNum )
	{
		Ray.vFrom = vPosition;
		//TODO: EXCHANGE!!!!!!!!!!!!!
		do 
		{
			Ray.vDir.SetRandomDirection();
		} while(Ray.vDir.Dot(Ray.vDir) > 1);
		Ray.vDir.Normalize();

		for( int32 nDepth = 0; nDepth < sceneCtx.m_nMaxDiffuseDepth; ++nDepth)
		{
			Ray.fTmin	=	0;
			Ray.fT		=	CGeomCore::m_fInf;
//			Ray.fTime	=	0;

			if( pGeomCore->Intersect(&Ray) )
			{
				if ( Ray.vN.Dot( Ray.vDir ) < 0.0f )
				{
					//only store if it's indirect or we need the direct too
					if( false == bIndirectOnly || nDepth > 0 )
						pRayMap->AddSegment( Ray.vFrom, Ray.vDir*Ray.fT, 0,0 );

					//update the start position
					Ray.vN *= -1;
					Vec3 vBinormal;
					if( fabsf(Ray.vN.x) < 0.9f )
						vBinormal = Vec3(1,0,0) - Ray.vN * Ray.vN.x;
					else
						if( fabsf(Ray.vN.y) < 0.9f )
							vBinormal = Vec3(0,1,0) - Ray.vN * Ray.vN.y;
						else
							vBinormal = Vec3(0,0,1) - Ray.vN * Ray.vN.z;
					vBinormal.Normalize();

					Vec3 vTangent( vBinormal.Cross( Ray.vN ) );

					float fcosA = sqrtf( (float)rand() /(float)RAND_MAX );
					float fsinA = sinf( acos_tpl( fcosA ) );
					float fB = g_PI2 * (float)rand() /(float)RAND_MAX;
					Ray.vDir = vBinormal * fsinA * cosf( fB ) + Ray.vN * fcosA +  vTangent * fsinA * sinf( fB );

					Ray.vFrom = Ray.vP;
				}//if direction is good..
			}//if intersect
		}//nDepth;
	}//for(nSampleNum)

	return true;
}

bool CRayMap_Tracer::Trace_AllLigths(CRLWaitProgress* pWaitProgress, CRayMap* pRayMap, CGeomCore* pGeomCore )
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	const PodArray<CDLight*>* pListLights = sceneCtx.m_pLstLightSources;
	int32	numAllLights	=	pListLights->Count();

	if( pWaitProgress )
			pWaitProgress->Caption("Rays Tracing from LightSources...");
	for( int32 i = 0; i < numAllLights; ++i )
	{
		CDLight* cLight	=	pListLights->GetAt(i);
		assert(cLight!=NULL);

		//don't want to be the part of the lightmap
		if( !(cLight->m_Flags & DLF_LM ) || cLight->m_Flags & DLF_SUN )
			continue;

		if (cLight->m_Flags & DLF_PROJECT)
			continue;


		if( false == Trace_Position( pRayMap, cLight->m_BaseOrigin, pGeomCore, false ) )
			return false;

		if( pWaitProgress )
			pWaitProgress->SetProgress((f32)i/(f32)numAllLights);

	}//for( lights )

	return true;
}
