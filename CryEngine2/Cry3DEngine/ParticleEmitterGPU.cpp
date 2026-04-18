//Start [ATI / jisidoro]
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
// -------------------------------------------------------------------------
//  File name:   particleEmitterGPU.cpp
//  Version:     v1.00
//  Created:     [ATI / jisidoro]
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//    derived particle emitter class for GPU based particle physics
//   
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS

#include "Particle.h"
#include "ParticleEmitter.h"
#include "ParticleEmitterGPU.h"
#include "partman.h"
#include "3dEngine.h"
#include "ISerialize.h"
#include "FogVolumeRenderNode.h"
#include "IPhysicsGPU.h"


#define PREVENT_RENDER_TO_EDITOR_PREVIEW_WINDOW


//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
//CParticleEmitterGPU::CParticleEmitterGPU( CPartManager *pPartManager, bool bIndependent ): 
//	CParticleEmitter( pPartManager, bIndependent )	//no default constructor.. so explicitly call the constructor
CParticleEmitterGPU::CParticleEmitterGPU( bool bIndependent ): 
	CParticleEmitter( bIndependent )	//no default constructor.. so explicitly call the constructor
{
	//initialize GPU system index
	int32 i;

	m_NumGPUParticleSystems = 0;

	for( i = 0; i < PEG_MAX_GPU_SYSTEMS_PER_EMITTER; i++ )
	{
		m_GPUParticleSystemIndices[ i ] = CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX;
		m_GPUSystemParams[ i ] = NULL;

	}

	m_bGPUActive = false;

	m_bRegistered = false;

	m_fPrevTime = 0.0;
	m_fCurrTime = 0.0;


	//Rough approx for now for testing
	m_WSBBox.min = Vec3(-10.0f, -10.0f, -10.0f ) +  Matrix34( m_qpLoc ).GetTranslation();
	m_WSBBox.max = Vec3( 10.0f,  10.0f,  10.0f ) +  Matrix34( m_qpLoc ).GetTranslation();

	//
	m_bbWorld = m_WSBBox;
	m_bbWorldDyn = m_WSBBox;
}


//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
CParticleEmitterGPU::~CParticleEmitterGPU()
{
	Deallocate();
}


//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
void CParticleEmitterGPU::Deallocate()
{
	int32 i;

	IGPUPhysicsManager *pGPUManager;

	pGPUManager = GetSystem()->GetIGPUPhysicsManager();

	//
	if( pGPUManager != NULL )
	{
		//m_GPUParticleSystemIndex = CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX;

		for( i = 0; i < m_NumGPUParticleSystems; i++ )
		{
			if( m_GPUParticleSystemIndices[ i ] != CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX )
			{
				pGPUManager->DestroyParticleEmitter( m_GPUParticleSystemIndices[ i ] );
				m_GPUParticleSystemIndices[ i ] = CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX;
				m_GPUSystemParams[ i ] = NULL;
			}
		}
	}


	m_NumGPUParticleSystems = 0;

}


//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
void CParticleEmitterGPU::Update()
{
	//Rough approx for now for testing
	//
	SPhysEnviron	physEnv;

	//small bounding box around player 
	m_WSBBox.min = Matrix34( m_qpLoc ).GetTranslation();
	m_WSBBox.max = Matrix34( m_qpLoc ).GetTranslation();

	//m_WSBBox.min = Vec3(-1.0f, -1.0f, -1.0f ) +  Matrix34( m_qpLoc ).GetTranslation();
	//m_WSBBox.max = Vec3( 1.0f,  1.0f,  1.0f ) +  Matrix34( m_qpLoc ).GetTranslation();

	/*
	physEnv.GetEnvironment( 
		this,								// const CParticleEmitter* pEmitter,
		m_WSBBox,							// AABB const& box,
		ENV_GRAVITY	| ENV_WIND | ENV_WATER,	// uint check_flags,
		false								// bool bNonUniformList
		);
	*/

	m_GPUSystemParams[ 0 ]->GetStaticBounds( 
		m_WSBBox,							// AABB& bb, 
		m_qpLoc,							// const QuatTS& loc, 
		Vec3(1.0, 1.0, 1.0),				// const Vec3& vSpawnSize, 
		false,								// bool bWithSize, 
		m_GPUSystemParams[ 0 ]->fParticleLifeTime.GetBase(),			// float fMaxLife, 
		physEnv.m_vUniformGravity,			// const Vec3& vGravity, 
		physEnv.m_vUniformWind				// const Vec3& vWind
		); 


	//
	m_bbWorld = m_WSBBox;
	m_bbWorldDyn = m_WSBBox;

}


//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
void CParticleEmitterGPU::Register( bool b )
{


	//register the entity for drawing
	Get3DEngine()->UnRegisterEntity( this );
	Get3DEngine()->RegisterEntity( this );
}


//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
void CParticleEmitterGPU::SetParams( const ParticleParams &params )
{
	IGPUPhysicsManager *pGPUManager;

	//deallocate previous GPU systems
	Deallocate();

	//ensure resource particle params are created
	//CParticleSubEmitter::ReleaseParams();
	//CParticleEmitter::ReleaseParams();
	//CParticleEmitter::SetParams( params );

	pGPUManager = GetSystem()->GetIGPUPhysicsManager();

	//create new GPU particle system
	if( pGPUManager == NULL )
	{
		return;
	}

	//allocate GPU particle systems for parent and all child systems
	
	//use params in order to adjust parameters on the fly
	//uint32 i;

	//may have to move this to set effect in order to grab the ResourceParticleParams from the container
	AllocateGPUSystem( (ResourceParticleParams *) &params, CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX );	//, this );


	//for( i = 0; i < m_SubEmitters.size(); i++ )
	//{
		//for now all GPU systems created are independent, for later, use parenting to derive GPU system parenting
		//AllocateGPUSystem( (ResourceParticleParams *) &( m_SubEmitters[ i ].GetParams ), CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX, this );
		
	//}

	//AllocateGPUSystem( (ResourceParticleParams *) &params, CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX, this );
	//AllocateGPUSystem( m_pParams, CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX, this );

}


//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
void CParticleEmitterGPU::AllocateGPUSystem( const ResourceParticleParams *pParams, int32 nParentIdx )
{
	
	IGPUPhysicsManager *pGPUManager;
	pGPUManager = GetSystem()->GetIGPUPhysicsManager();

	if( m_NumGPUParticleSystems > PEG_MAX_GPU_SYSTEMS_PER_EMITTER )
	{
		//return a warning 
		// too few GPU particle systems
		return;
	}

	//
	if( pGPUManager != NULL )	
	{
		int32 nGPUSystemIdx;

		//keep index of particle system in GPU system 
		nGPUSystemIdx = pGPUManager->CreateParticleEmitter( m_bIndependent, Matrix34( m_qpLoc ), pParams, nParentIdx );

		if( nGPUSystemIdx == CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX )
		{
			//unable to allocate GPU system
			return;
		}

		m_GPUParticleSystemIndices[ m_NumGPUParticleSystems ] = nGPUSystemIdx;
		m_GPUSystemParams[ m_NumGPUParticleSystems ] = pParams;

		m_NumGPUParticleSystems++;

		//
		//uint32 i;

		//note, subemitter pointer used to recurse down the child emitter tree, since the child emitters are all created as subEmitters
		
		//do not traverse heirarch for now 2/6/07
		/* for( i = 0; i < pSubEmitter->m_childEmitters.size(); i++ )
		{
			//if( m_GPUParticleSystemIndices[ i ] != CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX )
			//{
				//current index is parent to child systems
				AllocateGPUSystem( pSubEmitter->m_childEmitters[i]->m_pParams, nGPUSystemIdx, pSubEmitter->m_childEmitters[i] );
				//AllocateGPUSystem( pSubEmitter->m_childEmitters[i]->m_pParams, CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX, pSubEmitter->m_childEmitters[i] );
			//}
			
			//m_GPUParticleSystemIndices[ m_NumGPUParticleSystems ] = pGPUManager->CreateParticleEmitter( m_bIndependent, Matrix34( m_qpLoc ), m_childEmitters[i]->m_pParams, CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX );
			//m_GPUSystemParams[ m_NumGPUParticleSystems ] = m_childEmitters[i]->m_pParams;
			//m_NumGPUParticleSystems++;
		}
		*/
	}
}


//--------------------------------------------------------------------------------------------------------
// Recursive function to traverse the effect tree, and register all the child emitters.  
//
//--------------------------------------------------------------------------------------------------------
/*
void CParticleEmitterGPU::SetEffectRecurseEffectTree( const IParticleEffect* pEffect, CParticleSubEmitter *pSubEmitter )
{
	//Note: Subclass method in order to load textures and any other necessary data to make the subclass 
	// work note the code in this function is based on  CParticleSubEmitter::SetEffect( pEffect );
	// the reason I no longer call the base class function is because in the base class, nodes which 
	// have the bSecondGenerationFlag are not added to the m_childEmitters[] array, yet I need access to 
	// them as child emitters to render them
	//CParticleEmitter::SetEffect( pEffect );

	//extract pointer to parameters from the effect.
	//ParticleParams particleParams;
	//IParticleEffect *pEffectRef = (IParticleEffect *)pEffect;
	//const ParticleParams &refParticleParams = pEffectRef->GetParticleParams();
	pSubEmitter->m_pEffect = ( CParticleEffect * ) pEffect;
	pSubEmitter->m_pParams = &(pSubEmitter->m_pEffect->GetResourceParams());

	pSubEmitter->m_pEffect->LoadResources(false);
	//pSubEmitter->InvalidateStaticBounds(false);
	
	//if (m_pParams->bReceivesShadows)
	//	m_pMainEmitter->SetRndFlags(ERF_RECVSHADOWMAPS, true);
	//if (m_pParams->bCastsShadows)
	//	m_pMainEmitter->SetRndFlags(ERF_CASTSHADOWMAPS, true);
	
	//////////////////////////////////////////////////////////////////////////
	// Create child emitters
	//////////////////////////////////////////////////////////////////////////
	
	for (int i = 0; i < pEffect->GetChildCount(); i++)
	{
		CParticleEffect& childEffect = *(CParticleEffect*)pEffect->GetChild( i );
		//if (!childEffect.GetParticleParams().bSecondGeneration)
		//{
		
		// Create child emitter.
		CParticleSubEmitter *pChildEmitter = new CParticleSubEmitter( m_pPartManager, pSubEmitter );
		pSubEmitter->m_childEmitters.push_back( pChildEmitter );
		
		SetEffectRecurseEffectTree( &childEffect, pChildEmitter );
	}
}
*/


//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
void CParticleEmitterGPU::SetEffect( IParticleEffect const* pEffect, ParticleParams const* pParams  )
{

	if (!pEffect)
	{
		return;
	}

	//release the old params
	//ReleaseParams();
	//m_bOwnParams = false;

	//
	//SetEffectRecurseEffectTree( pEffect, this );
	
	//just call set effect for base class
	CParticleEmitter::SetEffect( pEffect, pParams );

	//invalidate static bounds for tree
	InvalidateStaticBounds( );

	//setup the gpu system using the current parameters from the effect
	SetParams( pEffect->GetParticleParams() );

	// SetParams( *m_pParams );
	//SetParams( *pParams );

	
}


//----------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------
void CParticleEmitterGPU::SetLoc( QuatTS const& qp )
{

	m_qpLoc = qp;
//	m_qpInvLoc = qp.GetInverted();

	/*
	//for later, implement particles that move relative to the emitter
	if (m_pParams->bMoveRelEmitter)
	{
		// Update particle coords.
		QuatTS qpMod = qp * m_pLocEmitter->m_qpInvLoc;
		for (TParticleList::iterator it(m_Particles); it; ++it)
			it->Transform(qpMod);
	}
	*/

	//reflect location changes in subemitters?
	/*if (m_qpLastLoc.s < 0.f)
	{
		// First move.
		m_qpLastLoc = qp;
	}
	else if (!m_bMoved)
	{
		m_qpLastLoc = m_qpLoc;		//m_pLocEmitter->m_qpLoc;
		m_bMoved = true;
	}*/



	IGPUPhysicsManager *pGPUManager;
	
	pGPUManager = GetSystem()->GetIGPUPhysicsManager();

	//create new GPU particle system
	if( pGPUManager == NULL )
	{
		return;
	}

	int32 i;

	//set pos for system and all child systems
	for( i = 0; i < m_NumGPUParticleSystems; i++ )
	{
		if( m_GPUParticleSystemIndices[ i ] != CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX )
		{
			pGPUManager->SetMatrix( m_GPUParticleSystemIndices[ i ], Matrix34( m_qpLoc ) );
		}
	}


}


//
void CParticleEmitterGPU::SetMatrix( const Matrix34& mat )
{
	SetLoc(QuatTS(mat));

}


//
void CParticleEmitterGPU::Activate( bool bActive )
{
	IGPUPhysicsManager *pGPUManager;
	
	pGPUManager = GetSystem()->GetIGPUPhysicsManager();

	//create new GPU particle system if possible
	if( pGPUManager == NULL )
	{
		return;
	}

	uint32 i;
	
	//
	for( i = 0; i < m_NumGPUParticleSystems; i++ )
	{
		//activate or deactivate GPU particle system
		if( m_GPUParticleSystemIndices[ i ] != CPG_NULL_GPU_PARTICLE_SYSTEM_INDEX )
		{
			pGPUManager->SetActive( m_GPUParticleSystemIndices[ i ], bActive );			
		}
	}

	//no need to activate SubEmitter tree, since it is not used
	//TParent::Activate(bActive ? eAct_Activate : eAct_Deactivate );	

	if ( bActive )
	{
		m_pPartManager->RegisterEmitter(this);
	}

	m_bGPUActive = bActive;


	/*
	if( bActive == true )
	{
	
	}

	if( bActive == false )
	{
		//Deallocate();
	}
	*/
}


//----------------------------------------------------------------------------------------------------------
// IsAlive
//
//----------------------------------------------------------------------------------------------------------
bool CParticleEmitterGPU::IsAlive() const
{
	//just for now
	return m_bGPUActive;

}


//----------------------------------------------------------------------------------------------------------
// Render GPU particle system
//  create and register GPU partcle render element code based on:
//   void CParticleSubEmitter::Render( const SRendParams& RenParams, PartProcessParams& ProcParams )
//
//   virtual bool Render( const SRendParams &rendParams );  //, PartProcessParams& ProcParams );
//
//----------------------------------------------------------------------------------------------------------
bool CParticleEmitterGPU::Render( const SRendParams &rendParams )	//, PartProcessParams& procParams )
{
	// PartProcessParams procParams;
	IGPUPhysicsManager *pGPUManager;	
	pGPUManager = GetSystem()->GetIGPUPhysicsManager();

	//ensure GPU manager is ok
	if( pGPUManager == NULL )
	{
		return false;
	}
	
#ifdef PREVENT_RENDER_TO_EDITOR_PREVIEW_WINDOW
	IRenderer *pRenderer;

	pRenderer = (IRenderer *)GetSystem()->GetIRenderer();

	//
	uint32 nWidth;
	uint32 nHeight;

	//
	nWidth =  pRenderer->GetWidth();
	nHeight = pRenderer->GetHeight();
	
	if( ( nWidth < 200 ) || ( nHeight < 200 ) )
	{
		//nothing drawn, no update
		return false;	
	}

#endif

	//compute time step to use for this iteration of particle system
	ITimer *pTimer = m_pSystem->GetITimer();
	f64 fTimeStep;

	m_fPrevTime = m_fCurrTime;
	m_fCurrTime = pTimer->GetAsyncCurTime();

	fTimeStep = m_fCurrTime - m_fPrevTime;

	if( fTimeStep > PEG_MAX_TIME_STEP )
	{
		fTimeStep = PEG_MAX_TIME_STEP;
	}
	
	//probably not needed
	if( fTimeStep < 0.0 ) 
	{
		fTimeStep = 0.0;
	}

	//#define PEG_MAX_TIME_STEP 0.2

	//fixed timestep for now, for later use timer
	//fTimeStep = PEG_MAX_TIME_STEP;			//0.05f;


	//precompute render params for fog
	ColorF fogVolumeContrib;
	CFogVolumeRenderNode::TraceFogVolumes( m_WSBBox.GetCenter(), fogVolumeContrib );
	uint16 nFogIdx = GetRenderer()->PushFogVolumeContribution( fogVolumeContrib );
	float fCamDist = m_bbWorld.GetDistance(GetRenderer()->GetCamera().GetPosition());
	SPartRenderParams PRParams( GetRenderer()->GetCamera(), fCamDist, GetViewDistRatio(), nFogIdx );

	//
	SPhysEnviron	physEnv;

	/*
	physEnv.GetEnvironment( 
		this,								// const CParticleEmitter* pEmitter,
		m_WSBBox,							// AABB const& box,
		ENV_GRAVITY	| ENV_WIND | ENV_WATER,	// uint check_flags,
		false								// bool bNonUniformList
		);
	*/

	//ensure GPU particle system exists
	int32 i;

	//physEnv.m_pWaterPlane

	for( i = 0; i < m_NumGPUParticleSystems ; i++  )
	{
		// set particle system wind and gravity
		pGPUManager->SetUniformWind( m_GPUParticleSystemIndices[ i ], physEnv.m_vUniformWind );		
		pGPUManager->SetUniformGravity( m_GPUParticleSystemIndices[ i ], physEnv.m_vUniformGravity );
		pGPUManager->SetWaterPlane( m_GPUParticleSystemIndices[ i ], physEnv.m_plUniformWater );

		//update the particle system
		pGPUManager->UpdateParticleSystem( m_GPUParticleSystemIndices[ i ], fTimeStep );
	}

	for( i = 0; i < m_NumGPUParticleSystems; i++ )
	{
		const ResourceParticleParams *pParams = m_GPUSystemParams[ i ];
	
		//add render element to the render to render this system
		if( pParams->bEnabled )
		{
			bool bRender = true;

			/*if (( pParams->eDrawType == ParticleDrawType_Horizontal ) && 
				( m_nRenderStackLevel > 0 )
				)
			{
				bRender = false;
			}*/
			
			//
			if ((rendParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP) && 
				!(pParams->bCastShadows) )
			{
				bRender = false;
			}

			// Individual container distance culling.
			/*
			float fMinPix = GetCVars()->e_particles_min_draw_pixels;
			if (fMinPix > 0.f)
			{
				float fSize = GetMaxParticleSize(false) * 2.f;
				
				float fDist = m_bbWorld.GetDistance(PRParams.m_Camera.GetPosition());
				
				if (fSize * PRParams.m_Camera.GetAngularResolution() < fDist * fMinPix)
				{
					bRender = false;
				}
			}
			*/

			if( bRender == true )
			{

				IMaterial* pMatToUse = GetMaterial();
				bool bCustomMat = pMatToUse != 0;
				if (!pMatToUse)
					pMatToUse = m_pPartManager->GetLightShader();
			
				// Create emitter RenderObject.
				CRenderObject* pOb = GetIdentityCRenderObject();
        if (!pOb)
          return false;

				switch( pParams->eBlendType )
				{
					case ParticleBlendType_AlphaBased:
						pOb->m_RState = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_ALPHATEST_GREATER;
					break;
					case ParticleBlendType_ColorBased:
						pOb->m_RState = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCCOL;
					break;
					case ParticleBlendType_Additive:
						pOb->m_RState = GS_BLSRC_ONE | GS_BLDST_ONE;
					break;
				}


					if (pParams->bReceiveShadows && rendParams.pShadowMapCasters != 0 && rendParams.pShadowMapCasters->Count() != 0 )
					{
						pOb->m_pShadowCasters = rendParams.pShadowMapCasters;
						pOb->m_ObjFlags |= FOB_INSHADOW;
					}
					
					// Ambient color for shader incorporates actual ambient lighting, as well as the constant emissive value.
					pOb->m_II.m_AmbColor = ColorF( pParams->fEmissiveLighting )
							* powf( Get3DEngine()->GetHDRDynamicMultiplier(), pParams->fEmissiveHDRDynamic );
					pOb->m_II.m_AmbColor += rendParams.AmbientColor * pParams->fDiffuseLighting;

					// Ambient.a is used to modulate dynamic lighting.
					//if ( pParams->eDrawType > ParticleDrawType_AlignToDir)
					if ( pParams->eFacing != ParticleFacing_Camera)
					{
						// Disable directional lighting for 3D particles, proper normals are not yet supported.
						pOb->m_II.m_AmbColor.a = 0;
					}
					else
					{
						pOb->m_II.m_AmbColor.a = pParams->fDiffuseLighting;
					}

					pOb->m_DynLMMask = ( pParams->fDiffuseLighting > 0.0f ) ? rendParams.nDLightMask : 0;

					pOb->m_nTextureID = bCustomMat ? -1 : pParams->nTexId;
					pOb->m_nTextureID1 = -1;
					pOb->m_FogVolumeContribIdx = PRParams.m_nFogVolumeContribIdx;
					pOb->m_AlphaRef = 0;
					pOb->m_nMotionBlurAmount = clamp_tpl( int_round( pParams->fMotionBlurScale * 128), 0, 255 );

					// Construct sort offset, using DrawLast param & effect order.
					pOb->m_fSort = -( max( -100, min( 100, pParams->nDrawLast ) ) + PRParams.m_nEmitterOrder * 0.01f );
					PRParams.m_nEmitterOrder++;
	
				if ( pParams->bDrawNear )
				{
					pOb->m_ObjFlags |= FOB_NEAREST;
				}
				
				if ( pParams->bNotAffectedByFog )
				{
					pOb->m_ObjFlags |= FOB_NO_FOG;
				}

				//if ( m_GPUSystemParams[ i ]->bSoftParticle )
				//{
				//	pOb->m_ObjFlags |= FOB_SOFT_PARTICLE;
				//}

				bool bCanUseGeomShader = false;
				//if ( //( pParams->eDrawType == ParticleDrawType_Billboard ) || 
				//	 //( pParams->eDrawType == ParticleDrawType_AlignToDir )
				//	 )
				if ( pParams->eFacing == ParticleFacing_Camera )
				{
					// no geom shader for GPU system yet
					//if (m_pParams->fTailLength.GetBase() == 0.f)
					//{
					//	bCanUseGeomShader = true;
					//}

					if ( pParams->bSoftParticle )
					{
						pOb->m_ObjFlags |= FOB_SOFT_PARTICLE;
					}
				}
			
				//	assert( Get3DEngine()->IsCameraUnderWater() == Get3DEngine()->IsUnderWater(GetCamera().GetPosition()) );
				//	bool bAfterWater = Get3DEngine()->IsCameraUnderWater() == m_pParams->bOnlyUnderWater;

				bool bAfterWater = true;

				GetRenderer()->EF_AddGPUParticlesToScene( m_GPUParticleSystemIndices[ i ], m_pMainEmitter->GetBBox(), pMatToUse->GetShaderItem(), pOb, bAfterWater, bCanUseGeomShader );

			}
		}

	}
	return true;
}


#endif 
