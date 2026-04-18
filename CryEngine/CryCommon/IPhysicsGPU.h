/*
	[ATI / jisidoro, epersson] - Begin ...
*/

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS

#ifndef __iphysicsgpu_h__
#define __iphysicsgpu_h__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef _LIB
	#ifdef GPUPHYSICS_EXPORTS
		#define CRYGPUPHYSICS_API __declspec(dllexport)
	#else
		#define CRYGPUPHYSICS_API __declspec(dllimport)
		#define vector_class Vec3_tpl
	#endif
#else
	#define CRYGPUPHYSICS_API
#endif



//////////////////////////////////////////////////////////////////////////
// IDs that can be used for foreign id.
//////////////////////////////////////////////////////////////////////////

#include "Cry_Math.h"
#include "Cry_Geo.h"


#include <CrySizer.h>

#undef min
#undef max
#include "primitives.h"
#include "physinterface.h"
#include "IInput.h"
//#include "IShader.h"

#include "I3DEngine.h"
#include "ParticleParams.h"

#define CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX -1

//
struct SGPUParticleRenderShaderParams
{
	float m_vLightTypes[ 4 ];
	float m_fBackLightFraction;

	float m_vCamFront[ 4 ];
	float m_vNearFarClipDist[ 4 ];

	float m_vOSLightPos[ 4 ][ 4 ];
	float m_mLightMatrix[ 4 ][ 4 ];

	float m_vSunDir[ 4 ];
	float m_vFogParams[ 4 ];
	float m_vFogColor[ 4 ];

	float m_vSkyLightHazeColPartialRayleighInScatter[ 4 ];
	float m_vSkyLightHazeColPartialMieInScatter[ 4 ];
	float m_vSkyLightSunDirection[ 4 ];
	float m_vSkyLightPhaseFunctionConstants[ 4 ];

	int		m_nNumLights;
	bool	m_bUsesLights;
	bool	m_bHasProj;
	bool	m_bHasShadow;

/*		float3  g_vCamPos					: register( c4 );
		float4  g_vTexcoordStartScale		: register( c5 );	//.xy = texcoord start, and .zw = texcoord scale		
		float2  g_vViewportDims				: register( c6 );
		float2  g_vInvViewportDims			: register( c7 );
		float2  g_vNearFarClipDist			: register( c8 );
		float4  g_vLightTypes				: register( c10 );
		float	g_fBackLightFraction		: register( c11 );
		//object space light positions
		float4  g_vOSLightPos[4]			: register( c12 );	//c12-c15
		//only one light matrix since there can only be one projected light
		column_major float4x4 g_mLightMatrix: register( c16 );  //c16-c19
		//camera front vector
		float3  g_vCamFront				: register( c20 );  
		float4 g_vSunDir									: register( c21 );
		float4 g_vFogParams									: register( c22 );
		float3 g_vFogColor									: register( c23 );
		float3 g_vSkyLightHazeColPartialRayleighInScatter	: register( c24 );
		float3 g_vSkyLightHazeColPartialMieInScatter		: register( c25 );
		float3 g_vSkyLightSunDirection						: register( c26 );
		float3 g_vSkyLightPhaseFunctionConstants			: register( c27 );
		float4 g_vMiscCamFront								: register( c28 );
		float4 g_vMiscNearFarClipDist						: register( c29 );
*/
};


//GPUPhysicsManager
struct IGPUPhysicsManager
{
	//
	virtual int32 CreateParticleEmitter( bool bIndependent, Matrix34 const& mLoc, const ParticleParams* pParams, int32 nParentIdx ) = 0;
	virtual bool  SetMatrix( int32 nIndex, Matrix34 const& mLoc ) = 0;
	virtual bool  SetActive( int32 nIndex, bool bActive ) = 0;
	virtual bool  DestroyParticleEmitter( int32 nIndex ) = 0;

	virtual void  UpdateParticleSystem( int32 nSystemIdx, f32 fTimeStep ) = 0;
	virtual void  RenderParticleSystem( int32 nSystemIdx, CShader *pCShader, SShaderPass *pShaderPass, SGPUParticleRenderShaderParams *pRenderParams ) = 0;
	virtual int32 GetNumPolygons( int32 nSystemIdx ) = 0;

	virtual bool SetupSceneRelatedData( void ) = 0;	
	virtual void RenderMessages() = 0;

	virtual bool AreParamsGPUCapable( const ParticleParams* pParams ) = 0;

	virtual void SetUniformWind( int32 nSystemIdx, Vec3 &vWind ) = 0;
	virtual void SetUniformGravity( int32 nSystemIdx, Vec3 &vGravity ) = 0;
	virtual void SetWaterPlane( int32 nSystemIdx, Plane &plWaterPlane ) = 0;
};


//class CGPUPhysicsManager;

extern "C" {
	CRYGPUPHYSICS_API IGPUPhysicsManager *CreateGPUPhysics(ISystem *pSystem);
};

typedef IGPUPhysicsManager *(*CreateGPUPhysicsProc)(ISystem *pSystem);

#endif //__iphysicsgpu_h__

#endif