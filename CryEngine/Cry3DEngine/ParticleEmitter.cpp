////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ParticleEmitter.cpp
//  Created:     18/7/2003 by Timur.
//  Modified:    17/3/2005 by Scott Peter
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleEmitter.h"
#include "ICryAnimation.h"
#include "Particle.h"
#include "partman.h"
#include "FogVolumeRenderNode.h"
#include "CREParticle.h"
#include "ISound.h"
#include "IThreadTask.h"

static float fPARTICLE_NON_EQUILUBRIUM_RESET_TIME		= 2.f;		// Time before purging unseen mortal effects
static float fPARTICLE_EQUILIBRIUM_RESET_TIME				= 30.f;		// Time before purging unseen immortal effects
static float fMAX_EQUILIBRIUM_UPDATE_TIME						= 0.25f;	// Update time for non-changing effects.

/*
	Scheme for Emitter updating & bounding volume computation.

	Whenever possible, we update emitter particles only on viewing.
	However, the bounding volume for all particles must be precomputed, 
	and passed to the renderer,	for visibility. Thus, whenever possible, 
	we precompute a Static Bounding Box, loosely estimating the possible
	travel of all particles, without actually updating them.

	Currently, Dynamic BBs are computed for emitters that perform
	physics, or are affected by non-uniform physical forces.
*/

//////////////////////////////////////////////////////////////////////////
// Misc functions.

inline float round_up(float f, float r)
{
	float m = fmod(f, r);
	if (m > 0.f)
		f += r-m;
	return f;
}

// Compute estimated lifetime based on current motion and target.
// Currently ignore gravity and wind.
float GetTravelTime(float fDist, float fVel, float fDrag)
{
	if (fDist*fVel <= 0.f)
		return 0.f;
	if (fDrag > 0.f)
	{
		//	X = V0 (1 - e^(-d t)) / d
		//	1 - X d / V0 = e^(-d t)
		//	t = - log(1 - X d / V0) / d
		return -logf( 1.f - fDist * fDrag / fVel ) / fDrag;
	}
	else
	{
		// No drag.
		return fDist / fVel;
	}
}

//////////////////////////////////////////////////////////////////////////
CParticleContainer::CParticleContainer( CParticleContainer* pParent, CParticleSubEmitter* pDirectSub, CParticleLocEmitter const* pLoc, IParticleEffect const* pEffect, ParticleParams const* pParams )
: m_pParentContainer(pParent), m_pDirectSubEmitter(pDirectSub), m_pLocEmitter(pLoc), m_pParams(0)
, m_Lock(*m_pPartManager->AllocLock())
{
	if (pEffect)
	{
		m_pEffect = (CParticleEffect*)pEffect;
		m_pParams = &m_pEffect->GetResourceParams();
		if (!m_pParams->bResourcesLoaded)
			m_pParams->LoadResources(m_pEffect->GetName());
	}
	else if (pParams)
	{
		// Make copy of params.
		m_pParams = new ResourceParticleParams(*pParams);
		m_pParams->LoadResources("(Params)");
	}
	else
		assert(0);

	// To do: Invalidate static bounds on updating areas.
	// To do: Use dynamic bounds if in non-uniform areas ??
	m_bbWorld.Reset();
	m_bbWorldStat.Reset();
	m_bbWorldDyn.Reset();
	m_nReserveCount = 0;
	m_pHistoryFree = 0;
	m_nHistorySteps = m_pParams->fTailLength.GetBase() > 0.f ? m_pParams->nTailSteps : 0;
	m_fEmitCountScale = 1.f;
	m_nQueuedFrame = -1;
	m_bRenderGeomShader = false; 
	m_bRendered = m_bVertsComputed = false;
	m_bExtendedLife = false;
	m_bUnused = false;

	m_bDynamicBounds = m_pParams->NeedsDynamicBounds() ||
		(m_pParentContainer && m_pParentContainer->NeedsDynamicBounds());
}

CParticleContainer::~CParticleContainer()
{
	{
		AUTO_LOCK(m_Lock);
		UnqueueUpdate();
		for_array (i, m_IndirectEmitters)
			DeleteIndirectEmitter(&m_IndirectEmitters[i]);
		ReleaseParams();
	}
	m_pPartManager->FreeLock(&m_Lock);
}

//////////////////////////////////////////////////////////////////////////
void CParticleContainer::ReleaseParams()
{
	if (m_pParams && OwnsParams())
	{
		if (m_pParams->nTexId > 0)
			GetRenderer()->RemoveTexture(m_pParams->nTexId);
		m_pParams->UnloadResources();
		delete m_pParams;
	}
	m_pParams = 0;
}

void CParticleContainer::UnloadResources()
{
	if (m_pParams && OwnsParams())
		// Unload owned params.
		m_pParams->UnloadResources();
}

void CParticleContainer::AddIndirectContainer( IParticleEffect const* pEffect )
{
	CParticleContainer* pIndirect = 0;
	if (m_pPartManager->IsActive(((IParticleEffect*)pEffect)->GetParticleParams()))
	{
		for_array (i, m_IndirectContainers)
		{
			if (!m_IndirectContainers[i].IsUsed() && m_IndirectContainers[i].Is( this, m_pLocEmitter, pEffect ))
			{
				pIndirect = &m_IndirectContainers[i];
				pIndirect->SetUsed(true);
				break;
			}
		}
		if (!pIndirect)
		{
			pIndirect = new CParticleContainer(this, 0, m_pLocEmitter, pEffect);
			m_IndirectContainers.push_back(pIndirect);
		}
	}

	// Recurse effect tree.
	for (int i = 0; i < pEffect->GetChildCount(); i++)
	{
		IParticleEffect* pChild = pEffect->GetChild(i);
		if (pChild->GetParticleParams().bSecondGeneration)
		{
			if (pIndirect)
				pIndirect->AddIndirectContainer(pChild);
		}
		else
			AddIndirectContainer(pChild);
	}
}

CParticleLocEmitter* CParticleContainer::CreateIndirectEmitter( CParticleSubEmitter* pParentEmitter, float fPast )
{
	if (!m_IndirectContainers.size())
		// No child emitters needed.
		return 0;
	CParticleLocEmitter* pLoc = new(GetMain().m_poolLocEmitters) CParticleLocEmitter(&GetMain(), fPast);
	pLoc->AddEmitters(pParentEmitter, m_IndirectContainers);

	// Add to own list for processing.
	m_IndirectEmitters.push_back(pLoc);
	return pLoc;
}

void CParticleContainer::DeleteIndirectEmitter( CParticleLocEmitter* pEmitter )
{
	GetMain().m_poolLocEmitters.Delete(pEmitter);
}

int CParticleContainer::GetParticleRefs( CParticleSubEmitter const* pSub) const
{
	int nRefs = 0;
	for_array (i, m_Particles)
		if (&m_Particles[i].GetEmitter() == pSub)
			nRefs++;
	for_array (i, m_IndirectEmitters)
		nRefs += m_IndirectEmitters[i].GetParticleRefs(pSub);
	return nRefs;
}

// Reasonable limits on particle counts;
static const int	nMAX_SPRITE_PARTICLES = 0x10000,
									nMAX_GEOM_PARTICLES = 0x1000,
									nMAX_ATTACHED_PARTICLES = 0x4000;

void CParticleContainer::ComputeReserveCount()
{
	if (!m_pParentContainer)
		m_fEmitCountScale = m_pLocEmitter->GetEmitCountScale();
	else
		m_fEmitCountScale = 1.f;
	float fCount = m_pParams->fCount.GetMaxValue() * m_fEmitCountScale;
	m_nReserveCount = int_ceil(fCount);

	if (m_pParams->bGeometryInPieces && m_pParams->pStatObj != 0)
	{
		// Emit 1 particle per piece.
		int nPieces = 0;
		for (int i = m_pParams->GetSubGeometryCount()-1; i >= 0; i--)
		{
			if (m_pParams->GetSubGeometry(i))
				nPieces++;
		}
		m_nReserveCount *= max(nPieces,1);
	}

	if (m_pParentContainer)
	{
		// Multiply by parent particle count.
		m_nReserveCount *= m_pParentContainer->m_nReserveCount;
	}

	int nMaxCount = GetParams().pStatObj ? nMAX_GEOM_PARTICLES : nMAX_SPRITE_PARTICLES;
	if (GetLoc().m_SpawnParams.eAttachType != GeomType_None && GetLoc().m_EmitGeom)
		nMaxCount = min(nMaxCount, nMAX_ATTACHED_PARTICLES);
	if (m_nReserveCount > nMaxCount)
	{
		m_fEmitCountScale *= float(nMaxCount) / m_nReserveCount;
		m_nReserveCount = nMaxCount;
	}
}

CParticle* CParticleContainer::AddParticle( bool bGrow, bool bReuse )
{
	// ComputeReserveCount will pre-alloc enough space in most cases, 
	// except for scripted particles and indirect children thereof.
	AUTO_LOCK_PROFILE(m_Lock, this);
	if (m_Particles.size() >= m_Particles.capacity())
	{
		if (m_Particles.size() >= m_nReserveCount)
		{
			if (m_pParentContainer && 
			max(m_pParentContainer->m_Particles.capacity(), m_pParentContainer->m_IndirectContainers.capacity()) > m_pParentContainer->m_nReserveCount)
				bGrow = true;
			if (!bGrow)
			{
				// Disable Reuse feature for now: implementation too slow, reject particles instead.
				// better for client code to avoid over-emitting.
				/*
				if (bReuse && m_Particles.size() > 0)
				{
					m_Counts.ParticlesReuse += 1.f;
					return m_Particles.push_back_fixed();
				}
				else
				*/
				{
					return 0;
				}
			}
		}
		m_Particles.reserve(m_Particles.capacity() ? m_Particles.capacity() * 5/4 : m_nReserveCount);
	}

	return m_Particles.push_back();
}

void CParticleContainer::DeleteParticle(CParticle* pPart)
{
	AUTO_LOCK_PROFILE(m_Lock, this);
	m_Particles.erase(pPart);
}

void CParticleContainer::OnSetLoc(QuatTS const& qp)
{
	InvalidateStaticBounds();
	if (m_pParams->bMoveRelEmitter || GetLoc().GetMoveRelEmitter())
	{
		// Update particle coords.
		AUTO_LOCK_PROFILE(m_Lock, this);
		QuatTS qpMod;
		qpMod.q = qp.q * !m_pLocEmitter->m_qpLoc.q;
		qpMod.s = qp.s / m_pLocEmitter->m_qpLoc.s;
		qpMod.t = qp.t - qpMod.q * m_pLocEmitter->m_qpLoc.t * qpMod.s;
		for (TParticleList::traverser it(m_Particles); it; ++it)
			it->Transform(qpMod);
		m_bbWorldDyn.SetTransformedAABB(Matrix34(qpMod), m_bbWorldDyn);
	}
	for_all (m_IndirectContainers).OnSetLoc(qp);
}

void CParticleContainer::SetUsed(bool b, bool bAll)
{
	AUTO_LOCK(m_Lock);
	m_bUnused = !b;
	m_bVertsComputed = false;
	if (bAll)
		for_all (m_IndirectContainers).SetUsed(b, true);
}

void CParticleContainer::ClearUnused()
{
	for_array (i, m_IndirectContainers)
	{
		if (!m_IndirectContainers[i].IsUsed())
		{
			// Remove indirect child container.
			AUTO_LOCK_PROFILE(m_Lock, this);

			// First clear its particles.
			m_IndirectContainers[i].Reset();

			// Clear indirect emitters referencing unused containers.
			for_all (m_IndirectEmitters).ClearUnused();

			// Remove container.
			i = m_IndirectContainers.erase(i);
		}
		else
			m_IndirectContainers[i].ClearUnused();
	}
}

void CParticleContainer::Update()
{
  m_Counts.Clear();
	m_bExtendedLife = GetTarget().bExtendLife ||
		(GetParams().bRemainWhileVisible && GetLoc().GetMain().TimeNotRendered() < 0.5f);
	if (NeedsDynamicBounds())
	{
		if (GetCVars()->e_particles)
			UpdateParticles();
	}
	else if (m_Particles.capacity() 
	&& GetTimeToUpdate() > GetMaxParticleLife() 
	+ (HasEquilibrium() ? fPARTICLE_EQUILIBRIUM_RESET_TIME : fPARTICLE_NON_EQUILUBRIUM_RESET_TIME))
		// Dealloc particle array when obsolete.
		Reset();

	m_bRendered = m_bVertsComputed = false;

	// Update indirect children.
	for_all (m_IndirectContainers).Update();
}

void CParticleContainer::UpdateChildEmitters()
{
	for_array (i, m_IndirectEmitters)
	{
		CParticleLocEmitter& Ind = m_IndirectEmitters[i];
		Ind.Update();
		if (!Ind.IsActive() && !Ind.IsAlive())
		{
			// Remove emitter only when deactivated by parent particle AND no longer alive.
			DeleteIndirectEmitter(&Ind);
			i = m_IndirectEmitters.erase(i);
		}
	}
}

void CParticleContainer::UnqueueUpdate()
{
	m_nQueuedFrame = -1;
	m_pPartManager->UnqueueUpdate(this);
}

void CParticleContainer::OnThreadUpdate()
{
	assert(CanThread());

	m_nQueuedFrame = -1;
	if (!m_bRendered && m_Lock.TryLock())
	{
		UpdateParticles();
		int tRender = GetRenderType();

		// Unlock it in case SetVertices waiting for it.
		m_Lock.Unlock();

		if (!(GetCVars()->e_particles_thread & AlphaBit('u')))
		{
			if (tRender == 'S')
			{
				// Need vertices.
				if (!m_bRendered && m_Lock.TryLock())
				{
					ComputeVertices();
					m_Lock.Unlock();
				}
			}
		}
	}
}

float CParticleContainer::GetMaxParticleLife(bool bAll) const
{
	float fParticleLife = m_pParams->GetMaxParticleLife();
	if (bAll) for_array (i, m_IndirectContainers)
	{
		CParticleContainer const& child = m_IndirectContainers[i];
		float fChildLife = child.GetMaxParticleLife();
		if (child.GetParams().bSpawnOnParentCollision)
			fChildLife += m_pParams->GetMaxParticleLife();
		else
		{
			fChildLife += child.GetParams().fSpawnDelay.GetMaxValue();
			if (child.GetParams().bContinuous)
			{
				// Add emitter lifetime.
				if (child.GetParams().fEmitterLifeTime.GetMaxValue() > 0.f)
					fChildLife += min(child.GetParams().fEmitterLifeTime.GetMaxValue(), m_pParams->GetMaxParticleLife());
				else
					fChildLife += m_pParams->GetMaxParticleLife();
			}
		}
		fParticleLife = max(fParticleLife, fChildLife);
	}
	return fParticleLife;
}

float CParticleContainer::GetMaxParticleSize(bool bAdjusted, bool bAll) const
{
	float fSize = GetParams().fSize.GetMaxValue();
	if (GetParams().pStatObj)
		fSize *= GetParams().pStatObj->GetRadius();
	if (bAdjusted)
		fSize *= GetParams().fViewDistanceAdjust;
	fSize *= GetLoc().GetParticleScale();

	// Recurse indirect containers.
	if (bAll)
		for_array (i, m_IndirectContainers)
			fSize = max(fSize, m_IndirectContainers[i].GetMaxParticleSize(bAdjusted, true));
	return fSize;
}

bool CParticleContainer::NeedsExtendedBounds() const
{
	// Bounds need extending on movement unless bounds always restricted relative to emitter.
	return !(m_pParams->bMoveRelEmitter || GetLoc().GetMoveRelEmitter()) && !m_pParams->bSpaceLoop;
}

void CParticleContainer::ComputeStaticBounds( AABB& bb, bool bWithSize, float fMaxLife )
{
  FUNCTION_PROFILER_SYS(PARTICLE);

	QuatTS loc = m_pLocEmitter->m_qpLoc;
	AABB bbSpawn(AABB::RESET);

	SPhysEnviron const& PhysEnv = GetMain().GetUniformPhysEnv();

	if (m_pParentContainer)
	{
		// Expand by parent spawn volume.
		bool bParentSize = m_pParams->eAttachType != GeomType_None;
		float fMaxParentLife = m_pParams->bSpawnOnParentCollision ? fHUGE : m_pParams->fSpawnDelay.GetMaxValue();
		if (m_pParams->bContinuous)
		{
			if (m_pParams->fEmitterLifeTime.GetMaxValue() > 0.f)
				fMaxParentLife += m_pParams->fEmitterLifeTime.GetMaxValue();
			else
				fMaxParentLife = fHUGE;
		}
		m_pParentContainer->ComputeStaticBounds( bbSpawn, bParentSize, fMaxParentLife );
		loc.SetIdentity();
		loc.t = bbSpawn.GetCenter();
		bbSpawn.Move(-loc.t);
	}
	else if (m_pLocEmitter->m_EmitGeom)
	{
		// Expand by attached geom.
		GetAABB(bbSpawn, loc, m_pLocEmitter->m_EmitGeom);
	}

	if (GetTarget().bTarget)
	{
		bbSpawn.Add(Vec3(ZERO));
		bbSpawn.Add(loc.GetInverted() * GetTarget().vTarget);
	}

	Vec3 vSpawnSize(ZERO);
	if (!bbSpawn.IsReset())
	{
		loc.t = loc * bbSpawn.GetCenter();
		vSpawnSize = bbSpawn.GetSize() * 0.5f;
	}

	loc.s = GetLoc().GetParticleScale();
	m_pParams->GetStaticBounds(bb, loc, vSpawnSize, bWithSize, fMaxLife, PhysEnv.m_vUniformGravity, PhysEnv.m_vUniformWind);
	assert(bb.GetRadius() < 1e5f);
}

void CParticleContainer::GetDynamicBounds( AABB& bb )
{
	bb.Reset();
	for_array (i, m_Particles)
	{
		CParticle* pPart = &m_Particles[i];
		pPart->GetPhysicsState();
		pPart->UpdateBounds( bb );
	}
	assert(bb.IsReset() || bb.GetRadius() < 1e5f);
}

void CParticleContainer::UpdateBounds(AABB& bbMain, AABB& bbDyn)
{
	if (NeedsDynamicBounds())
	{
		GetDynamicBounds(m_bbWorldDyn);
		m_bbWorld = m_bbWorldDyn;
	}
	else
	{
		if (m_bbWorldStat.IsReset())
			ComputeStaticBounds(m_bbWorldStat);
		if (StaticBoundsStable())
		{
			m_bbWorld = m_bbWorldStat;
		}
		else
		{
			if (!m_bbWorldDyn.IsReset())
				m_bbWorld = m_bbWorldDyn;
			m_bbWorld.Add(m_bbWorldStat);
		}
	}

	if (GetRenderType())
	{
		bbMain.Add(m_bbWorld);
		bbDyn.Add(m_bbWorldDyn);
	}

	m_bbWorldDyn.Reset();

	for_all (m_IndirectContainers).UpdateBounds(bbMain, bbDyn);
}

// To do: Add flags for movement/gravity/wind.
void CParticleContainer::InvalidateStaticBounds()
{
	if (!NeedsDynamicBounds() && !m_bbWorldStat.IsReset())
	{
		m_bbWorldStat.Reset();
		if (NeedsExtendedBounds())
			m_timeStaticBounds = GetTimer()->GetFrameStartTime() + m_pParams->GetMaxParticleLife();
	}
	for_all (m_IndirectContainers).InvalidateStaticBounds();
}

void CParticleContainer::ComputeUpdateContext( SParticleUpdateContext& context )
{
	context.nEnvFlags = GetEnvironmentFlags();
	if (GetCVars()->e_particles_object_collisions < 2)
	{
		context.nEnvFlags &= ~ENV_DYNAMIC_ENT;
		if (GetCVars()->e_particles_object_collisions < 1)
			context.nEnvFlags &= ~ENV_STATIC_ENT;
	}
	if (m_pParams->bSpaceLoop)
	{
		// Check for special space-loop collide behavior.
		if (!(GetCVars()->e_particles_debug & AlphaBit('i')))
		if (m_pParams->fBounciness < 0.f)
		{
			context.nSpaceLoopFlags = context.nEnvFlags & (ENV_TERRAIN | ENV_STATIC_ENT | ENV_DYNAMIC_ENT);
			context.nEnvFlags &= ~context.nSpaceLoopFlags;
		}

		CCamera const& cam = m_pPartManager->GetRenderCamera();
		AABB bbSpaceLocal;

		if (m_pParams->fCameraMaxDistance > 0.f)
		{
			// Cam-local bounds.
			// Scale cam dist limits to handle zoom.
			float fZoom = 1.f / cam.GetFov();
			float fHSinCos[2]; cry_sincosf( cam.ComputeHorizontalFov()*0.5f, &fHSinCos[0], &fHSinCos[1] );
			float fVSinCos[2]; cry_sincosf( cam.GetFov()*0.5f, &fVSinCos[0], &fVSinCos[1] );
			float fMin = fHSinCos[1] * fVSinCos[1] * (m_pParams->fCameraMinDistance * fZoom);

			bbSpaceLocal.max = Vec3( fHSinCos[0], 1.f, fVSinCos[0] ) * (m_pParams->fCameraMaxDistance * fZoom);
			bbSpaceLocal.min = Vec3( -bbSpaceLocal.max.x, fMin, -bbSpaceLocal.max.z );
		}
		else
		{
			// Use emission bounding volume.
			bbSpaceLocal.min = m_pParams->vPositionOffset - m_pParams->vRandomOffset;
			bbSpaceLocal.max = m_pParams->vPositionOffset + m_pParams->vRandomOffset;
		}

		// Transform to centered cube, -.5 .. +.5
		context.matSpaceLoop = GetLoc().GetMatrix() * Matrix34::CreateScale( bbSpaceLocal.GetSize(), bbSpaceLocal.GetCenter() );
		context.matSpaceLoopInv = context.matSpaceLoop.GetInverted();
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleContainer::UpdateParticles()
{
	if (GetCVars()->e_particles_debug & AlphaBit('z'))
		return;

	AUTO_LOCK_PROFILE(m_Lock, this);

	if (m_pParentContainer)
		m_pParentContainer->UpdateParticles();

	float fUpdateTime = GetTimeToUpdate();
	if (fUpdateTime > 0.f || m_timeLastUpdate == CTimeValue())
	{
	  FUNCTION_PROFILER_CONTAINER(this);

		// Limit update time for steady state.
		if (fUpdateTime > fMAX_EQUILIBRIUM_UPDATE_TIME && m_timeLastUpdate != CTimeValue() && HasEquilibrium())
		{
			if (GetAge() - fUpdateTime >= GetEquilibriumAge())
			{
				fUpdateTime = fMAX_EQUILIBRIUM_UPDATE_TIME;
				m_timeLastUpdate = GetTimer()->GetFrameStartTime() - CTimeValue(fUpdateTime);
			}
		}

		SParticleUpdateContext context;
		ComputeUpdateContext(context);

		if (GetMain().GetUniformPhysEnv().m_nNonUniformFlags & context.nEnvFlags)
			context.PhysEnv.GetPhysAreas( &GetMain(), m_bbWorld, context.nEnvFlags, &m_aNonUniformAreas );
		else
			context.PhysEnv = GetMain().GetUniformPhysEnv();

		if (fUpdateTime > 0.f)
		{
			// Evolve & cull existing sprites.
			for (TParticleList::traverser it(m_Particles); it; ++it)
			{
				if (!it->Update( context, fUpdateTime ))
					it.erase();
			}
		}

		// Emit new particles.
		EmitParticles( context );

		// Update indirect children.
		UpdateChildEmitters();

		m_timeLastUpdate = GetTimer()->GetFrameStartTime();

		context.PhysEnv.Clear();

		// Update dynamic bounds if required.
		if (!StaticBoundsStable() || GetMain().IsSelected() || (GetCVars()->e_particles_debug & AlphaBit('b')))
			GetDynamicBounds(m_bbWorldDyn);

		m_Counts.EmittersActive = 1.f;
	}
}

void CParticleContainer::EmitParticles( SParticleUpdateContext const& context )
{
  FUNCTION_PROFILER_CONTAINER(this);

	ComputeReserveCount();

	if (m_pDirectSubEmitter)
		m_pDirectSubEmitter->EmitParticles( context );
	if (m_pParentContainer)
		m_pParentContainer->EmitParticles( context, this );
}

void CParticleContainer::EmitParticles( SParticleUpdateContext const& context, CParticleContainer* pIndirect )
{
	// Emit from our particles into the indirect container.
	AUTO_LOCK_PROFILE(m_Lock, this);
	for_all (m_IndirectEmitters).EmitParticles( context, pIndirect );
}

void CParticleContainer::QueueUpdate( bool bNext )
{
	// Queue for update next frame, in render order.
	if (CanThread() && m_nQueuedFrame != GetMainFrameID()+bNext)
	{
		m_nQueuedFrame = GetMainFrameID()+bNext;
		m_pPartManager->QueueUpdate(this, bNext);
	}
}

/* Water sorting / filtering:

									Effect:::::::::::::::::::::::::::
Emitter	Camera		above					both					below
-------	------		-----					----					-----
above		above			AFTER					AFTER					skip
				below			BEFORE				BEFORE				skip

both		above			AFTER\below		AFTER[\below]	BEFORE\above
				below			BEFORE\below	AFTER[\above]	AFTER\above

below		above			skip					BEFORE				BEFORE
				below			skip					AFTER					AFTER
*/

void CParticleContainer::Render( SRendParams const& RenParams, SPartRenderParams& PRParams )
{
  FUNCTION_PROFILER_CONTAINER(this);

	char tRender = GetRenderType();

	if (m_pParams->eFacing == ParticleFacing_Water && m_nRenderStackLevel > 0)
		tRender = 0;
	if (RenParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP)
	{
		if (!m_pParams->bCastShadows || tRender == 'S')
			tRender = 0;;
	}

	bool bAfterWater = true;
	if (!GetLoc().m_SpawnParams.bIgnoreLocation)
	{
		if (!TrinaryMatch(m_pParams->tVisibleUnderwater, GetMain().GetUniformPhysEnv().m_tUnderWater))
			tRender = 0;
		else
			bAfterWater = TrinaryMatch(m_pParams->tVisibleUnderwater, Get3DEngine()->IsCameraUnderWater())
				&& TrinaryMatch(GetMain().GetUniformPhysEnv().m_tUnderWater, Get3DEngine()->IsCameraUnderWater());

		if (!TrinaryMatch(m_pParams->tVisibleIndoors, GetMain().GetVisEnviron().OriginIndoors()))
			tRender = 0;;
	}

  // hack: special case for when inside amphibious vehicle
  if ((m_pParams->bOceanParticle || m_pParams->eFacing == ParticleFacing_Water) && 
		(Get3DEngine()->GetOceanRenderFlags() & OCR_NO_DRAW) )
    tRender = 0;;

	// Artist tweakable max distance culling.
	if (m_pParams->fCameraMaxDistance > 0 && PRParams.m_fCamDistance > m_pParams->fCameraMaxDistance)
		tRender = 0;

	// Individual container distance culling.
	if (tRender && PRParams.m_fCamDistance > GetMaxParticleSize(true, false) * PRParams.m_fViewDistRatio)
		tRender = 0;

	if (tRender)
	{
		if (tRender == 'G')
		{
			// Geometry particles.
			UpdateParticles();
			RenderGeometry( RenParams, PRParams );
		}
		else
		{
			if (m_bbWorld.IsReset())
				UpdateBounds(m_bbWorld, m_bbWorldDyn);

			SParticleRenderInfo RenInfo;

			// Use main emitter bounding box for sorting, so all sub-emitters are sorted consistently by DrawLast.
			RenInfo.m_bbEmitter = GetMain().GetBBox();

			// Create emitter RenderObject.
			CRenderObject* pOb = GetIdentityCRenderObject();
      if (!pOb)
        return;

			switch( m_pParams->eBlendType )
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

			if (m_pParams->bReceiveShadows && RenParams.pShadowMapCasters != 0 && RenParams.pShadowMapCasters->Count() != 0 )
			{
				pOb->m_pShadowCasters = RenParams.pShadowMapCasters;
				pOb->m_ObjFlags |= FOB_INSHADOW;
			}
			
			// Ambient color for shader incorporates actual ambient lighting, as well as the constant emissive value.
			float fEmissive = m_pParams->fEmissiveLighting;
			if( GetRenderer()->EF_Query( EFQ_HDRModeEnabled ) )
				fEmissive *= powf( Get3DEngine()->GetHDRDynamicMultiplier(), m_pParams->fEmissiveHDRDynamic );
			pOb->m_II.m_AmbColor = ColorF(fEmissive) + RenParams.AmbientColor * m_pParams->fDiffuseLighting;

			// Ambient.a is used to modulate dynamic lighting.
			if (m_pParams->eFacing != ParticleFacing_Camera)
				// Disable directional lighting for 3D particles, proper normals are not yet supported.
				pOb->m_II.m_AmbColor.a = 0;
			else
				pOb->m_II.m_AmbColor.a = m_pParams->fDiffuseLighting;

			if (m_pParams->fDiffuseLighting > 0.f)
				pOb->m_DynLMMask = RenParams.nDLightMask;
			else
				pOb->m_DynLMMask = 0;

			pOb->m_nTextureID = m_pParams->pMaterial ? -1 : m_pParams->nTexId;
			pOb->m_nTextureID1 = -1;
			pOb->m_FogVolumeContribIdx = PRParams.m_nFogVolumeContribIdx;
			pOb->m_AlphaRef = 0;
			pOb->m_nMotionBlurAmount = clamp_tpl(int_round(m_pParams->fMotionBlurScale * 128), 0, 255);

			// Construct sort offset, using DrawLast param & effect order.
			pOb->m_fDistance = PRParams.m_fCamDistance;
			pOb->m_fSort = -(max(-100,min(100,m_pParams->nDrawLast)) + PRParams.m_nEmitterOrder * 0.01f);
			PRParams.m_nEmitterOrder++;

			if (m_pParams->bDrawNear)
				pOb->m_ObjFlags |= FOB_NEAREST;
			if (m_pParams->bNotAffectedByFog)
				pOb->m_ObjFlags |= FOB_NO_FOG;

			bool bCanUseGeomShader = false;
			if (m_pParams->eFacing == ParticleFacing_Camera)
			{
				if (GetHistorySteps() == 0)
					bCanUseGeomShader = true;
				if (m_pParams->bSoftParticle)
					pOb->m_ObjFlags |= FOB_SOFT_PARTICLE;
			}

      if (m_pParams->bOceanParticle)
      {
        if( GetCVars()->e_water_ocean_soft_particles )
        {
          pOb->m_ObjFlags |= FOB_OCEAN_PARTICLE;
          pOb->m_RState |= GS_NODEPTHTEST;              
        }
      }

			IMaterial* pMatToUse = m_pParams->pMaterial;
			if (!pMatToUse)
				pMatToUse = m_pPartManager->GetLightShader();
			{
				FRAME_PROFILER( "EF_AddParticlesToScene", gEnv->pSystem, PROFILE_PARTICLE)
				pOb = GetRenderer()->EF_AddParticlesToScene( pMatToUse->GetShaderItem(), pOb, this, RenInfo, bAfterWater, bCanUseGeomShader );
			}
			// Determine vertex creation method (for threaded update).
			m_bRenderGeomShader = bCanUseGeomShader;

			if (!NeedsDynamicBounds())
				QueueUpdate(false);
		}
	}

	for_all (m_IndirectContainers).Render( RenParams, PRParams );
}

ParticleTarget const& CParticleContainer::GetTarget() const
{
	if (m_pDirectSubEmitter)
		return m_pDirectSubEmitter->GetTarget();
	else
		return GetMain().GetTarget();
}

bool CParticleContainer::CanThread() const
{
	if (GetLoc().m_EmitGeom.m_pChar)
		// Avoid accessing character data in thread.
		return false;
	if (NeedsDynamicBounds())
		// Container updated every frame synchronously.
		// Also avoids creating / querying physical entities in thread.
		return false;
	if (m_pDirectSubEmitter )
	{
		ResourceParticleParams const& params = m_pDirectSubEmitter->GetParams();
		if (!params.sSound.empty() )
			//using the sound system is not thread safe
			return false;
		if (params.pStatObj)
			//using the pStatObj is not thread safe
			return false;
	}
	return true;
}

bool CParticleContainer::IsImmortal() const
{
	return GetParams().IsImmortal() || GetLoc().m_SpawnParams.fPulsePeriod != 0.f;
}

bool CParticleContainer::HasEquilibrium() const
{
	if (m_pParentContainer)
		return m_pParentContainer->HasEquilibrium();
	if (GetLoc().m_SpawnParams.fPulsePeriod > 0.f)
		return false;
	if (GetAge() >= GetLoc().GetStopAge())
		return false;
	return GetParams().HasEquilibrium();
}

float CParticleContainer::GetEquilibriumAge(bool bAll) const
{
	float fStableAge = (m_timeStaticBounds - GetLoc().GetTimeCreated()).GetSeconds();
	if (m_pDirectSubEmitter)
		return max(m_pDirectSubEmitter->GetEquilibriumAge(bAll), fStableAge);
	else if (m_pParentContainer)
		return max(
			m_pParentContainer->GetEquilibriumAge(false) 
			+ GetParams().fSpawnDelay.GetMaxValue()
			+ GetMaxParticleLife(bAll), 
			fStableAge);
	else
		return fHUGE;
}

SParticleHistory* CParticleContainer::AllocPosHistory()
{
	// Get free list.
	if (m_pHistoryFree)
	{
		SParticleHistory* aFree = m_pHistoryFree;
		m_pHistoryFree = *(SParticleHistory**)m_pHistoryFree;
		return aFree;
	}

	// Use pool.
	if (!m_aHistoryPool.capacity())
	{
		// Init history pool with original tail step size.
		m_aHistoryPool.reserve( m_nHistorySteps * m_Particles.capacity() );
	}
	if (m_aHistoryPool.size()+m_nHistorySteps <= m_aHistoryPool.capacity())
	{
		m_aHistoryPool.resize(m_aHistoryPool.size()+m_nHistorySteps);
		return m_aHistoryPool.end() - m_nHistorySteps;
	}

	// Unexpected overflow, use general heap.
	return new SParticleHistory[m_nHistorySteps];
}

void CParticleContainer::FreePosHistory( SParticleHistory* aHist)
{
	if (aHist >= m_aHistoryPool.begin() && aHist < m_aHistoryPool.end())
	{
		// Add to head of free list.
		*(SParticleHistory**)aHist = m_pHistoryFree;
		m_pHistoryFree = aHist;
	}
	else
		// Was allocated on general heap.
		delete[] aHist;
}

void CParticleContainer::Reset()
{
	AUTO_LOCK_PROFILE(m_Lock, this);
	for_all (m_IndirectContainers).Reset();
	m_Particles.clear();
	m_aHistoryPool.clear();
	for_array (i, m_IndirectEmitters)
		DeleteIndirectEmitter(&m_IndirectEmitters[i]);
	m_IndirectEmitters.clear();
	m_pHistoryFree = 0;
	m_timeLastUpdate = CTimeValue();
}

// Stat functions.
void CParticleContainer::GetCounts( SParticleCounts& counts, bool bAll, bool bEffectStats ) const
{
	counts.Add(m_Counts);

	counts.EmittersAlloc += 1.f;
	counts.ParticlesAlloc += m_Particles.capacity();
	if (m_Counts.EmittersActive > 0.f)
	{
		counts.ParticlesActive += m_Particles.size();
	  counts.ParticlesOverflow += max(m_Particles.size() - m_nReserveCount, 0);
	}

	if (bEffectStats && m_pEffect)
		GetCounts(m_pEffect->m_statsCur, false, false);

	if (bAll)
		for_all (m_IndirectContainers).GetCounts(counts, true, bEffectStats);
}

void CParticleContainer::GetMemoryUsage(ICrySizer* pSizer)
{
	AddContainer(pSizer, m_Particles);
	AddContainer(pSizer, m_aHistoryPool);
	AddContainer(pSizer, m_aRenderVerts);
	AddPtrContainer(pSizer, m_IndirectContainers);
	AddPtrContainer(pSizer, m_IndirectEmitters);
}

//////////////////////////////////////////////////////////////////////////
// CParticleSubEmitter implementation.
//////////////////////////////////////////////////////////////////////////

CParticleSubEmitter::CParticleSubEmitter( CParticleSubEmitter const* pParent, CParticleContainer* pCont, CParticleLocEmitter* pLoc )
	: m_ChaosKey(0U), m_qpLastLoc(IDENTITY), 
	m_pParams(&pCont->GetParams()), m_pContainer(pCont), m_pLocEmitter(pLoc), m_pParentEmitter(pParent)
{
	m_qpLastLoc.s = -1.f;
	m_bStarted = m_bMoved = false;
	m_nParticleRefs = 0;
	m_fStartAge = m_fEndAge = fHUGE;
	m_fRepeatAge = 0.f;
	m_fRelativeAge = 0.f;
	m_fToEmit = 0.f;
	m_pForce = NULL;
	if (IsIndirectChild(m_pParentEmitter))
		m_pParentEmitter->AddParticleRef(1);
}

CParticleSubEmitter::~CParticleSubEmitter()
{
	// Make sure no remaining children.
	assert(m_nParticleRefs == 0);
	assert(GetContainer().GetParticleRefs(this) == m_nParticleRefs);
	Activate(eAct_Deactivate);
	if (IsIndirectChild(m_pParentEmitter))
		m_pParentEmitter->AddParticleRef(-1);
}

//////////////////////////////////////////////////////////////////////////
void CParticleSubEmitter::Initialize( float fPast )
{
	m_bStarted = false;
	m_fToEmit = 0.f;

	// Reseed randomness.
	m_ChaosKey = CChaosKey(cry_rand32());

	// Compute lifetime params.
	m_fStartAge = m_fEndAge = GetAge() - fPast + GetCurValue(m_pParams->fSpawnDelay);
	if (m_pParams->bContinuous)
	{
		if (m_pParams->fEmitterLifeTime.GetMaxValue() > 0.f)
			m_fEndAge += GetCurValue(m_pParams->fEmitterLifeTime);
		else
			m_fEndAge = fHUGE;
	}

	// Compute next repeat age. First of individual sub-effect repetition, or emitter entity repetition.
	float fRepeat = GetCurValue(m_pParams->fPulsePeriod);
	if (fRepeat > 0.f)
	{
		m_fRepeatAge = GetAge() - fPast + fRepeat;
		if (m_fRepeatAge < GetAge())
			m_fRepeatAge += round_up( GetAge()-m_fRepeatAge, fRepeat );
	}
	else if (fRepeat < 0.f && m_pParentEmitter)
		// Inherit from parent.
		m_fRepeatAge = m_pParentEmitter->m_fRepeatAge;
	else
		m_fRepeatAge = 0.f;
}

//////////////////////////////////////////////////////////////////////////
void CParticleSubEmitter::Deactivate()
{
	if (m_pSound != 0)
	{
		m_pSound->Stop();
		m_pSound = 0;
	}
	if (m_pForce)
	{
		GetPhysicalWorld()->DestroyPhysicalEntity(m_pForce);
		m_pForce = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleSubEmitter::Activate( EActivateMode mode, float fPast )
{
	switch (mode)
	{
		case eAct_Deactivate:
		case eAct_ParentDeath:
			Deactivate();
			break;
		case eAct_Restart:
			Deactivate();
			// continue;
		case eAct_Activate:
			if (!GetParams().bSpawnOnParentCollision && !m_bStarted)
				Initialize(fPast);
			break;
		case eAct_Reactivate:
			GetContainer().Reset();
			m_fToEmit = 0.f;
			m_bStarted = false;
			if (GetAge()-fPast < m_fStartAge)
			{
				Deactivate();
				Initialize();
			}
			break;
		case eAct_Pause:
			Deactivate();
			break;
		case eAct_ParentCollision:
			if (GetParams().bSpawnOnParentCollision && !m_bStarted)
				// Spawn delayed emitters.
				Initialize(fPast);
			break;
	}
}

void CParticleSubEmitter::OnSetLoc( QuatTS const& qp )
{
	if (HasDirectContainer())
		GetContainer().OnSetLoc(qp);

	if (m_qpLastLoc.s < 0.f)
		// First move.
		m_qpLastLoc = qp;
	else if (!m_bMoved && GetTimeToUpdate() > 0.f)
	{
		m_qpLastLoc = m_pLocEmitter->m_qpLoc;
		m_bMoved = true;
	}
	if (m_pSound)
		// Update sound position.
		m_pSound->SetPosition( qp * m_pParams->vPositionOffset );
}

EEmitterState CParticleSubEmitter::GetState() const
{
	float fAge = GetAge();
	if (fAge >= m_fStartAge)
	{
		float fEndAge = GetEndAge();
		if (fAge <= fEndAge)
			// Emitter still active.
			return eEmitter_Active;

		if (GetContainer().HasExtendedLifetime())
			return eEmitter_Particles;

		float fParticleLife = GetContainer().GetMaxParticleLife();
		if (fParticleLife == 0.f || fAge <= fEndAge + fParticleLife)
			// Particles still alive.
			return eEmitter_Particles;
	}

	// Particles finished, see if scheduled to repeat.
	if (GetLoc().IsActive() || fAge <= GetLoc().GetStopAge())
	{
		if (fAge < m_fStartAge)
			return eEmitter_Dormant;
		if (GetLoc().m_SpawnParams.fPulsePeriod != 0.f || GetParams().fPulsePeriod.GetMaxValue() != 0.f)
			return eEmitter_Dormant;
	}

	if (m_nParticleRefs)
		return eEmitter_Dormant;

	return eEmitter_Dead;
}

void CParticleSubEmitter::EmitParticles( SParticleUpdateContext const& context )
{
	// Emit new particles only for enabled effects.
	float fAge = GetAge();
	if (fAge >= m_fStartAge)
	{
		UpdateRelativeAge();

		// Target total particles in system.
		float fEmitRate = GetEmitRate();
		if (fEmitRate > 0.f)
		{
			float fUpdateTime = GetTimeToUpdate();
			float fAge0 = fAge - fUpdateTime;
			fAge0 = max(fAge0, m_fStartAge);
			float fLife = GetContainer().GetMaxParticleLife();

			if (m_pParams->bContinuous)
			{
				// Determine time window to update.
				float fAge1 = min(fAge, GetEndAge());

				// Adjust emit rate downward for current framerate to avoid most particle rejection.
				// Necessary because particles stay alive 1 extra frame after lifetime.
				fEmitRate *= fLife / (fLife + GetTimer()->GetFrameTime());
				float fAgeIncrement = 1.f / fEmitRate;
				// Compute time of next emission.
				float fNextAge = fAge0 - m_fToEmit * fAgeIncrement;
				if (fNextAge < fAge1)
				{
					fAge0 = fNextAge;

					// Skip time before emitted particles would still be alive.
					// To to opt: avoid even this much updating for steady-state emitters (freeze).
					float fSkip = round_up(fAge-fLife-fAge0, fAgeIncrement);
					if (fSkip > 0)
						fAge0 += fSkip;
					float fPartAge = fAge-fAge0; for (; fPartAge > fAge-fAge1; fPartAge -= fAgeIncrement)
						if (!EmitParticle(context, false, fPartAge))
						{
							GetContainer().GetCounts().ParticlesReject += fPartAge * fEmitRate;
							break;
						}
					fAge0 = fAge-fPartAge;
					m_fToEmit = 0.f;
				}
				m_fToEmit += (fAge1-fAge0) * fEmitRate;
			}
			else if (m_fToEmit == 0.f)
			{
				// Emit only once, if still valid.
				m_fToEmit = -1.f;
				if (fLife == 0.f || fAge < m_fStartAge + GetContainer().GetMaxParticleLife())
				{
					for (int nEmit = int_round(fEmitRate); nEmit > 0; nEmit--)
						if (!EmitParticle(context, false, fAge - fAge0))
						{
							GetContainer().GetCounts().ParticlesReject += nEmit;
							break;
						}
				}
			}
		}
	}
	m_bMoved = false;
	// assert(GetContainer().GetParticleRefs(this) == m_nParticleRefs);
}

bool CParticleSubEmitter::GetParamTarget( ParticleTarget& target ) const
{
	if (m_pParams->eForceGeneration == ParticleForce_Target)
	{
		if (GetState() >= eEmitter_Active)
		{
			target.bTarget = true;
			target.bExtendSpeed = true;
			target.vTarget = m_pLocEmitter->GetWorldPosition(m_pParams->vPositionOffset);
			target.vVelocity = m_pLocEmitter->GetVel();
			return true;
		}
	}
	return false;
}

void CParticleSubEmitter::UpdateTarget()
{
	if (GetParams().bIgnoreAttractor)
	{
		m_Target = ParticleTarget();
		return;
	}

	// Inherit from external target first.
	ParticleTarget target = GetMain().GetTarget();
	if (!m_Target.bPriority)
	{
		// Override with local param target.
		for (const CParticleSubEmitter* pEmitter = m_pParentEmitter; pEmitter; pEmitter = pEmitter->m_pParentEmitter)
		{
			if (pEmitter->GetParamTarget(target))
				break;
		}
	}
	if (HasDirectContainer() && target.bTarget != m_Target.bTarget || target.vTarget != m_Target.vTarget)
		GetContainer().InvalidateStaticBounds();
	m_Target = target;
}

void CParticleSubEmitter::UpdateForce()
{
	float fAge = GetAge();
	UpdateRelativeAge();

	// Set or clear physical force.
	if (m_pParams->eForceGeneration != ParticleForce_None
	&& m_pParams->eForceGeneration != ParticleForce_Target
	&& GetState() >= eEmitter_Particles)
	{
		struct SForceGeom
		{
			QuatTS	qpLoc;							// world placement of force
			AABB		bbOuter, bbInner;		// local boundaries of force.
			Vec3		vForce3;
			float		fForceW;
		} force;

		//
		// Compute force geom.
		//

		SPhysEnviron const& PhysEnv = GetMain().GetUniformPhysEnv();

		// Location.
		force.qpLoc = GetLoc().m_qpLoc;
		force.qpLoc.s *= GetLoc().GetParticleScale();
		force.qpLoc.t = force.qpLoc * m_pParams->vPositionOffset;

		// Direction.
		Vec3 vFocus = force.qpLoc.q * Vec3(0,1,0);
		if (m_pParams->bFocusGravityDir)
			vFocus = (-PhysEnv.m_vUniformGravity).GetNormalizedSafe(vFocus);

		float fFocusAngle = GetCurValue(m_pParams->fFocusAngle);
		if (fFocusAngle != 0.f)
		{
			float fAzimuth = GetCurValue(m_pParams->fFocusAzimuth, 360.f);

			// Rotate focus about X.
			Vec3 vRot(1,0,0);
			vFocus = Quat::CreateRotationAA(DEG2RAD(fAzimuth), Vec3(0,1,0)) 
						 * Quat::CreateRotationAA(DEG2RAD(fFocusAngle), vRot) * vFocus;
		}
		force.qpLoc.q = Quat::CreateRotationV0V1(Vec3(0,1,0),vFocus);

		// Set inner box from spawn geometry.
		Quat qToLocal = force.qpLoc.q.GetInverted() * m_pLocEmitter->m_qpLoc.q;
		Vec3 vOffset = qToLocal * Vec3(m_pParams->vRandomOffset);
		force.bbInner.Reset();
		force.bbInner.Add(vOffset, m_pParams->fPosRandomOffset);
		force.bbInner.Add(-vOffset, m_pParams->fPosRandomOffset);

		// Emission directions.
		float fPhiMax = DEG2RAD(GetCurValue(1.f, m_pParams->fEmitAngle)), 
					fPhiMin = DEG2RAD(GetCurValue(0.f, m_pParams->fEmitAngle));

		AABB bbTrav;
		bbTrav.max.y = cos(fPhiMin);
		bbTrav.min.y = cos(fPhiMax);
		float fCosAvg = (bbTrav.max.y + bbTrav.min.y) * 0.5f;
		bbTrav.max.x = bbTrav.max.z = (bbTrav.min.y * bbTrav.max.y < 0.f ? 1.f : sin(fPhiMax));
		bbTrav.min.x = bbTrav.min.z = -bbTrav.max.x;
		bbTrav.Add(Vec3(ZERO));

		// Force magnitude: speed times relative particle density.
		float fSpeed = GetCurValue(1.f, m_pParams->fSpeed) * GetLoc().m_SpawnParams.fSpeedScale;
		float fForce = fSpeed * GetCurValue(1.f, m_pParams->fAlpha) * force.qpLoc.s;

		float fPLife = GetCurValue(1.f, m_pParams->fParticleLifeTime);
		float fTime = fAge-m_fStartAge;
		if (m_pParams->bContinuous && fPLife > 0.f)
		{
			// Ramp up/down over particle life.
			float fEndAge = GetEndAge();
			if (fTime < fPLife)
				fForce *= fTime/fPLife;
			else if (fTime > fEndAge)
				fForce *= 1.f - (fTime-fEndAge) / fPLife;
		}

		// Force direction.
		force.vForce3.zero();
		force.vForce3.y = fCosAvg * fForce;
		force.fForceW = sqrt(1.f - square(fCosAvg)) * fForce;

		// Travel distance.
		float fDist = TravelDistance( abs(fSpeed), GetCurValue(1.f, m_pParams->fAirResistance), min(fTime,fPLife) );
		bbTrav.min *= fDist;
		bbTrav.max *= fDist;

		// Set outer box.
		force.bbOuter = force.bbInner;
		force.bbOuter.Augment(bbTrav);

		// Expand by size.
		float fSize = GetCurValue(1.f, m_pParams->fSize);
		force.bbOuter.Expand( Vec3(fSize) );

		// Scale: Normalise box size, so we can handle some geom changes through scaling.
		Vec3 vSize = force.bbOuter.GetSize()*0.5f;
		float fRadius = max(max(vSize.x, vSize.y), vSize.z);

		if (fForce * fRadius == 0.f)
		{
			// No force.
			if (m_pForce)
			{
				GetPhysicalWorld()->DestroyPhysicalEntity(m_pForce);
				m_pForce = NULL;
			}
			return;
		}

		force.qpLoc.s *= fRadius;
		float fIRadius = 1.f / fRadius;
		force.bbOuter.min *= fIRadius;
		force.bbOuter.max *= fIRadius;
		force.bbInner.min *= fIRadius;
		force.bbInner.max *= fIRadius;

		//
		// Create physical area for force.
		//

		primitives::box geomBox;
		geomBox.Basis.SetIdentity();
		geomBox.bOriented = 0;
		geomBox.center = force.bbOuter.GetCenter();
		geomBox.size = force.bbOuter.GetSize() * 0.5f;

		pe_status_pos spos;
		if (m_pForce)
		{
			// Check whether shape changed.
			m_pForce->GetStatus(&spos);
			if (spos.pGeom)
			{
				primitives::box curBox;
				spos.pGeom->GetBBox(&curBox);
				if (!curBox.center.IsEquivalent(geomBox.center, 0.001f)
				 || !curBox.size.IsEquivalent(geomBox.size, 0.001f))
				 spos.pGeom = NULL;
			}
			if (!spos.pGeom)
			{
				GetPhysicalWorld()->DestroyPhysicalEntity(m_pForce);
				m_pForce = NULL;
			}
		}

		if (!m_pForce)
		{
			IGeometry *pGeom = m_pPhysicalWorld->GetGeomManager()->CreatePrimitive( primitives::box::type, &geomBox );
			m_pForce = m_pPhysicalWorld->AddArea( pGeom, force.qpLoc.t, force.qpLoc.q, force.qpLoc.s );
			if (!m_pForce)
				return;

			// Tag area with this emitter, so we can ignore it in the emitter family.
			pe_params_foreign_data fd;
			fd.pForeignData = (void*)&GetMain();
			fd.iForeignData = fd.iForeignFlags = 0;
			m_pForce->SetParams(&fd);
		}
		else
		{
			// Update position & box size as needed.
			if (!spos.pos.IsEquivalent(force.qpLoc.t, 0.01f)
				|| !spos.q.IsEquivalent(force.qpLoc.q)
				|| spos.scale != force.qpLoc.s)
			{
				pe_params_pos pos;
				pos.pos = force.qpLoc.t;
				pos.q = force.qpLoc.q;
				pos.scale = force.qpLoc.s;
				m_pForce->SetParams(&pos);
			}
		}

		// To do: 4D flow
		pe_params_area area;
		float fVMagSqr = force.vForce3.GetLengthSquared(),
					fWMagSqr = square(force.fForceW);
		float fMag = sqrt(fVMagSqr + fWMagSqr);
		area.bUniform = (fVMagSqr > fWMagSqr) * 2;
		if (area.bUniform)
		{
			force.vForce3 *= fMag / sqrt(fVMagSqr);
		}
		else
		{
			force.vForce3.z = fMag * (force.fForceW < 0.f ? -1.f : 1.f);
			force.vForce3.x = force.vForce3.y = 0.f;
		}
		area.falloff0 = force.bbInner.GetRadius();
		area.size.x = max( abs(force.bbOuter.min.x), abs(force.bbOuter.max.x) );
		area.size.y = max( abs(force.bbOuter.min.y), abs(force.bbOuter.max.y) );
		area.size.z = max( abs(force.bbOuter.min.z), abs(force.bbOuter.max.z) );

		if (m_pParams->eForceGeneration == ParticleForce_Gravity)
			area.gravity = force.vForce3;
		m_pForce->SetParams(&area);

		if (m_pParams->eForceGeneration == ParticleForce_Wind)
		{
			pe_params_buoyancy buoy;
			buoy.iMedium = 1;
			buoy.waterDensity = buoy.waterResistance = 0;
			buoy.waterFlow = force.vForce3;
			buoy.waterPlane.n = PhysEnv.m_plUniformWater.n;
			buoy.waterPlane.origin = PhysEnv.m_plUniformWater.n * -PhysEnv.m_plUniformWater.d;
			m_pForce->SetParams(&buoy);
		}
	}
	else
	{
		if (m_pForce)
		{
			GetPhysicalWorld()->DestroyPhysicalEntity(m_pForce);
			m_pForce = NULL;
		}
	}
}

void CParticleSubEmitter::Update()
{
	if (!(GetMain().GetRndFlags() & ERF_HIDDEN))
	{
		// Evolve emitter state.
		float fAge = GetAge();

		// Handle pulsing.
		if (GetLoc().m_SpawnParams.fPulsePeriod > 0.f)
		{
			float fRepeat = round_up(fAge, GetLoc().m_SpawnParams.fPulsePeriod);
			m_fRepeatAge = m_fRepeatAge > 0 ? min(m_fRepeatAge, fRepeat) : fRepeat;
		}

		if (m_fRepeatAge != 0.f && fAge >= m_fRepeatAge)
			Initialize(GetAge() - m_fRepeatAge);

		if (fAge >= m_fStartAge)
		{
			// Handle sound start/stop.
			if (!m_bStarted)
			{
				m_bStarted = true;
				if (m_pSound)
				{
					m_pSound->Stop();
					m_pSound = 0;
				}
				if (!m_pParams->sSound.empty())
				{
					if (gEnv->pSoundSystem)
					{
						// Start sound.
						int nSndFlags = FLAG_SOUND_DEFAULT_3D;
						if (m_pParams->bContinuous)
							// Will apply only to legacy .wav sounds, not events.
							nSndFlags |= FLAG_SOUND_LOOP;

						m_pSound = gEnv->pSoundSystem->CreateSound( m_pParams->sSound, nSndFlags );
						if (m_pSound)
						{
							m_pSound->SetSemantic(eSoundSemantic_Particle);

							if (m_pSound->GetFlags() & FLAG_SOUND_LOOP)
							{
								// Only play looping sounds if emitter continuous.
								if (m_pParams->bContinuous)
									PlaySound();
							}
							else
							{
								// Only play non-looping sound if not started too late
								if (fAge - m_fStartAge <= 0.25f)
									PlaySound();
								m_pSound = 0;
							}
						}
					}
				}
			}
			else if (m_pSound != 0)
			{
				if (GetAge() > GetEndAge())
				{
					m_pSound->Stop();
					m_pSound = 0;
				}
				else if (m_pParams->fSoundFXParam.GetMaxValue() != 0.f)
				{
					m_pSound->SetParam("particlefx", GetCurValue(m_pParams->fSoundFXParam), false);
				}
			}
		}

		UpdateTarget();
	}

	if (HasDirectContainer())
	{
		GetContainer().Update();
		if (!IsAlive())
			GetContainer().Reset();
	}
}

void Interp( QuatTS& out, QuatTS const& a, QuatTS const& b, float t )
{
	out.t.SetLerp( a.t, b.t, t );
	out.s = a.s * (1.f-t) + b.s * t;
	out.q.SetNlerp( a.q, b.q, t );
}

int CParticleSubEmitter::EmitParticle( SParticleUpdateContext const& context, bool bGrow, float fAge, IStatObj* pStatObj, IPhysicalEntity* pPhysEnt, QuatTS* pLocation, Vec3* pVel )
{
	QuatTS qpLoc;
	if (pLocation)
		qpLoc = *pLocation;
	else if (m_bMoved && fAge > 0.f)
	{
		// Interpolate emission position.
		float fTime = GetTimeToUpdate();
		float fBackT = fTime > 0.f ? fAge / fTime : 0.f;
		Interp(qpLoc, m_pLocEmitter->m_qpLoc, m_qpLastLoc, fBackT);
	}
	else
		qpLoc = m_pLocEmitter->m_qpLoc;

	if (!pStatObj && !pPhysEnt && m_pParams->pStatObj != 0 && m_pParams->pStatObj->GetSubObjectCount())
	{
		// If bGeomInPieces, emit one particle per piece.
		// Else iterate in 2 passes to count pieces, and emit a random piece.
		int nPiece = -1;
		while (!pStatObj)
		{
			int nPieces = 0;
			for (int i = 0; i < m_pParams->GetSubGeometryCount(); i++)
			{
				IStatObj::SSubObject* pSub = m_pParams->GetSubGeometry(i);
				if (pSub)
				{
					if (m_pParams->bGeometryInPieces)
					{
						CParticle* pPart = m_pContainer->AddParticle(true);
						if (!pPart)
							return nPieces;
						if (!pPart->Init( context, fAge, this, qpLoc * QuatTS(pSub->localTM), pLocation != 0, pVel, pSub->pStatObj ))
						{
							m_pContainer->DeleteParticle(pPart);
							continue;
						}
					}
					else if (nPieces == nPiece)
					{
						pStatObj = pSub->pStatObj;
						break;
					}
					nPieces++;
				}
			}
			if (m_pParams->bGeometryInPieces)
				return nPieces;
			if (nPieces == 0)
				return 0;
			nPiece = Random(nPieces);
		}
	}

  // Allow particle growth if specified; otherwise, re-use oldest.
	CParticle* pPart = m_pContainer->AddParticle(bGrow || m_Target.bExtendCount, true);
	if (pPart)
	{
		if (!pPart->Init( context, fAge, this, qpLoc, pLocation != 0, pVel, pStatObj, pPhysEnt ))
			m_pContainer->DeleteParticle( pPart );
		return 1;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CParticleSubEmitter::PlaySound()
{
	if (!m_pSound)
		return;

	m_pSound->SetPosition( m_pLocEmitter->GetWorldPosition(m_pParams->vPositionOffset) );
	m_pSound->Play();
}

float CParticleSubEmitter::GetEmitRate() const
{
	float fCount = GetCurValue(m_pParams->fCount) * GetContainer().GetEmitCountScale();
	float fLife = GetContainer().GetMaxParticleLife();
	float fPulse = min(
		(GetLoc().m_SpawnParams.fPulsePeriod != 0.f ? GetLoc().m_SpawnParams.fPulsePeriod : fHUGE),
		(GetParams().fPulsePeriod.GetBase() != 0.f ? GetParams().fPulsePeriod.GetMinValue() : fHUGE)
		);

	if (GetParams().bContinuous)
	{
		if (fLife <= 0.f)
			return 0.f;

		float fEmitterLife = GetCurValue(GetParams().fEmitterLifeTime);
		if (fEmitterLife > 0.f && fEmitterLife < fPulse)
		{
			// Actually pulsing.
			if (fEmitterLife < fLife)
				// Ensure enough particles emitted.
				fCount *= fLife / fEmitterLife;

			// Reduce emit rate for overlapping pulsing.
			if (fLife >= fPulse)
			{
				float fLdP = floor(fLife/fPulse);
				float fLmP = fLife - fLdP*fPulse;
				float fOverlap = (fLdP * fEmitterLife + min(fLmP,fEmitterLife)) / min(fLife,fEmitterLife);
				fLife *= fOverlap;
			}
		}

		if (m_Target.bExtendLife && !m_Target.bExtendCount)
		{
			// Compute required life time to reach target. Decrease emission rate if greater than standard life.
			float fTravelDistance = (m_Target.vTarget - GetLoc().m_qpLoc.t).GetLength();
			float fTime = GetTravelTime( fTravelDistance, GetParams().fSpeed.GetMaxValue() * GetLoc().m_SpawnParams.fSpeedScale, GetParams().fAirResistance.GetMaxValue() );
			if (fTime > fLife)
				fCount *= fLife / fTime;
		}

		// Compute continual emission rate which maintains fCount particles.
		return fCount / fLife;
	}
	else
	{
		if (fPulse < fLife)
		{
			// Reduce emit count for overlapping pulsing.
			fLife += GetParams().fSpawnDelay.GetMaxValue();
			int nOverlap = int_ceil(fLife / fPulse);
			fCount /= nOverlap;
		}
		return fCount;
	}
}

//////////////////////////////////////////////////////////////////////////
// CParticleLocEmitter implementation.
//////////////////////////////////////////////////////////////////////////

CParticleLocEmitter::CParticleLocEmitter( CParticleEmitter* pMainEmitter, float fAge )
	: m_pMainEmitter(pMainEmitter), m_qpLoc(IDENTITY), m_vVel(ZERO)
{
	m_timeCreated = GetTimer()->GetFrameStartTime() - CTimeValue(fAge);
	m_fStopAge = fHUGE;
	m_bActive = true;
	m_bMoveRelEmitter=false;
}

CParticleLocEmitter::~CParticleLocEmitter()
{
	// Clean up in reverse order, as some emitters will have earlier ones as parents.
	for (int i = m_SubEmitters.size()-1; i >= 0; i--)
	{
		if (this != m_pMainEmitter)
			GetMain().m_poolSubEmitters.Delete(&m_SubEmitters[i]);
		else
			delete &m_SubEmitters[i];
	}
	if (m_EmitGeom.m_pStatObj)
		m_EmitGeom.m_pStatObj->Release();
	if (m_EmitGeom.m_pChar)
		m_EmitGeom.m_pChar->Release();
	if (m_EmitGeom.m_pPhysEnt)
		m_EmitGeom.m_pPhysEnt->Release();
}

//////////////////////////////////////////////////////////////////////////
void CParticleLocEmitter::AddEffect( CParticleSubEmitter* pParent, IParticleEffect const* pEffect, ParticleParams const* pParams )
{
	if (pEffect)
		pParams = &const_cast<IParticleEffect*>(pEffect)->GetParticleParams();
	else if (!pParams)
		return;

	float fAge = GetAge();
	if (!pParams->bSecondGeneration)
	{
		CParticleSubEmitter* pEmitter = 0;
		if (m_pPartManager->IsActive(*pParams))
		{
			// Add direct effect, if not existing.
			for_array (i, m_SubEmitters)
			{
				if (!m_SubEmitters[i].GetContainer().IsUsed() && m_SubEmitters[i].Is( pParent, pEffect ))
				{
					pEmitter = &m_SubEmitters[i];
					pEmitter->GetContainer().SetUsed(true);
					break;
				}
			}
			if (!pEmitter)
			{
				pEmitter = new CParticleDirectEmitter( pParent, this, pEffect, pParams );
				m_SubEmitters.push_back(pEmitter);
				pEmitter->Activate(eAct_Activate, fAge);
			}
		}

		// Recurse effect tree.
		if (pEffect)
			for (int i = 0; i < pEffect->GetChildCount(); i++)
				AddEffect( pEmitter, pEffect->GetChild(i) );
	}
	else
	{
		if (pParent && pEffect)
		{
			// Add indirect effect.
			pParent->GetContainer().AddIndirectContainer(pEffect);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleLocEmitter::SetEffect( IParticleEffect const* pEffect, ParticleParams const* pParams )
{
	m_SubEmitters.clear();
	AddEffect(0, pEffect, pParams);
}

void CParticleLocEmitter::SetSpawnParams( SpawnParams const& spawnParams, GeomRef geom )
{
	m_SpawnParams = spawnParams;
	SetEmitGeom(geom);
}

int CParticleLocEmitter::AddEmitters( CParticleSubEmitter* pParent, SmartPtrArray<CParticleContainer>& IndirectContainers, int iStart )
{
	float fAge = GetAge();
	while (iStart < IndirectContainers.size())
	{
		CParticleSubEmitter* pEmitter = new(GetMain().m_poolSubEmitters) CParticleSubEmitter(pParent, &IndirectContainers[iStart], this);
		m_SubEmitters.push_back(pEmitter);
		pEmitter->Activate(eAct_Activate, fAge);
		int iChild = iStart+1; 
		while (iChild < IndirectContainers.size())
		{
			if (IndirectContainers[iChild].GetEffect()->GetParent() == IndirectContainers[iStart].GetEffect())
				// Child effect.
 				iChild = AddEmitters( pEmitter, IndirectContainers, iChild );
			else
 				break;
		}
		iStart = iChild;
	}
	return iStart;
}

//////////////////////////////////////////////////////////////////////////
void CParticleLocEmitter::SetEmitGeom( GeomRef geom )
{
	GeomRef geomNew = geom;

	if (geomNew.m_pStatObj == m_EmitGeom.m_pStatObj
	 && geomNew.m_pChar == m_EmitGeom.m_pChar
	 && geomNew.m_pPhysEnt == m_EmitGeom.m_pPhysEnt)
	 return;

	if (m_EmitGeom.m_pStatObj)
		m_EmitGeom.m_pStatObj->Release();
	if (m_EmitGeom.m_pChar)
		m_EmitGeom.m_pChar->Release();
	if (m_EmitGeom.m_pPhysEnt)
		m_EmitGeom.m_pPhysEnt->Release();

	m_EmitGeom = geomNew;

	if (m_EmitGeom)
	{
		if (m_EmitGeom.m_pStatObj)
			m_EmitGeom.m_pStatObj->AddRef();
		if (m_EmitGeom.m_pChar)
			m_EmitGeom.m_pChar->AddRef();
		if (m_EmitGeom.m_pPhysEnt)
			m_EmitGeom.m_pPhysEnt->AddRef();
	}

	InvalidateStaticBounds();
}

EEmitterState CParticleLocEmitter::GetState() const
{
	EEmitterState eState = eEmitter_Dead;
	for_array (i, m_SubEmitters)
		eState = max(eState, m_SubEmitters[i].GetState());
	return eState;
}

void CParticleLocEmitter::Activate( EActivateMode mode, float fPast )
{
	switch (mode)
	{
		case eAct_Activate:
			m_fStopAge = fHUGE;
			if (m_bActive)
				return;
			m_bActive = true;
			break;
		case eAct_Deactivate:
		case eAct_ParentDeath:
			m_bActive = false;
			// continue
		case eAct_Pause:
		{
			float fStopAge = (GetTimer()->GetFrameStartTime() - m_timeCreated).GetSeconds() - fPast;
			m_fStopAge = min(m_fStopAge, fStopAge);
			break;
		}
		case eAct_Resume:
			m_fStopAge = fHUGE;
			m_bActive = true;
			break;
	}

	for_all (m_SubEmitters).Activate(mode, fPast);
}

void CParticleLocEmitter::Prime()
{
	float fEqTime = 0.f;
	for_array (i, m_SubEmitters)
	{
		ResourceParticleParams const& params = m_SubEmitters[i].GetParams();
		if (params.HasEquilibrium())
			// Continuous immortal effect.
			fEqTime = max(fEqTime, m_SubEmitters[i].GetEquilibriumAge());
	}

	CTimeValue timePrime = GetTimer()->GetFrameStartTime() - CTimeValue(fEqTime);
	if (timePrime < m_timeCreated)
		m_timeCreated = timePrime;
}

float CParticleLocEmitter::GetEmitCountScale() const
{
	float fCount = m_SpawnParams.fCountScale;
	if (m_SpawnParams.bCountPerUnit)
	{
		// Count multiplied by geometry extent.
		fCount *= GetExtent(m_GeomQuery, m_EmitGeom, m_SpawnParams.eAttachType, m_SpawnParams.eAttachForm);
		fCount *= ScaleExtent(m_SpawnParams.eAttachForm, m_qpLoc.s);
	}
	fCount *= max(0.75f, GetCVars()->e_particles_lod);		// clamped to low spec value to prevent cheating
	return fCount;
}

void CParticleLocEmitter::SetLoc( QuatTS const& qp )
{
	if (!m_qpLoc.IsEquivalent(qp, 1e-5f))
		for_all (m_SubEmitters).OnSetLoc(qp);
	m_qpLoc = qp;
}

void CParticleLocEmitter::ClearUnused()
{
	for_array (i, m_SubEmitters)
	{
		if (!m_SubEmitters[i].GetContainer().IsUsed())
			i = m_SubEmitters.erase(i);
		else if (!m_SubEmitters[i].GetParams().bSecondGeneration)
			m_SubEmitters[i].GetContainer().ClearUnused();
	}
}

float CParticleLocEmitter::GetParticleScale() const
{
	// Somewhat obscure. But top-level emitters spawned from entities, 
	// and not attached to other objects, should apply the entity scale to their particles.
	if (m_pMainEmitter->m_SpawnParams.eAttachType == GeomType_None)
		return m_SpawnParams.fSizeScale * m_pMainEmitter->m_qpLoc.s;
	else
		return m_SpawnParams.fSizeScale;
}

void CParticleLocEmitter::GetMemoryUsage(ICrySizer* pSizer)
{
	AddPtrContainer(pSizer, m_SubEmitters);
}

//////////////////////////////////////////////////////////////////////////
// CParticleEmitter implementation.
////////////////////////////////////////////////////////////////////////

CParticleEmitter::CParticleEmitter( bool bIndependent )
	: CParticleLocEmitter(this), m_bIndependent(bIndependent)
{
	m_nEntityId = 0;
	m_nEntitySlot = 0;
	m_bRegistered = false;
	m_bSelected = false;
	m_pMainEmitter = this;
	m_nEnvFlags = 0;
	m_WSBBox.Reset();
	m_bbWorld.Reset();
	m_bbWorldDyn.Reset();
	m_bbWorldEnv.Reset();
}

//////////////////////////////////////////////////////////////////////////
CParticleEmitter::~CParticleEmitter() 
{ 
	Reset();
  Get3DEngine()->FreeRenderNodeState(this); // Also does unregister entity.
}

//////////////////////////////////////////////////////////////////////////
void CParticleEmitter::Register(bool b)
{
	m_bRegistered = b;
	if (!b)
	{
		Reset();
		if (!m_WSBBox.IsReset())
		{
			Get3DEngine()->UnRegisterEntity(this);
			m_WSBBox.Reset();
		}
	}
	else
	{
		// Register top-level render node if applicable.
		if (m_SpawnParams.bIgnoreLocation)
			Get3DEngine()->UnRegisterEntity(this);
		else if (!IsEquivalent(m_WSBBox, m_bbWorld))
		{
			if (!m_WSBBox.IsReset())
				Get3DEngine()->UnRegisterEntity(this);
			m_WSBBox = m_bbWorld;
			if (!m_WSBBox.IsReset())
				Get3DEngine()->RegisterEntity(this);
		}

		if (!m_WSBBox.IsReset())
		{
			if (!(m_nEnvFlags & ENV_CAST_SHADOWS))
				SetRndFlags(ERF_CASTSHADOWMAPS, false);
			else if (m_bIndependent)
				SetRndFlags(ERF_CASTSHADOWMAPS, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
string CParticleEmitter::GetDebugString(char type) const
{
	string s = GetName();
	if (type == 's')
	{
		// Serialization debugging.
		IEntity* pEntity = GetSystem()->GetIEntitySystem()->GetEntity(m_nEntityId);
		if (pEntity)
			s += string().Format(" entity=%s slot=%d", pEntity->GetName(), m_nEntitySlot);
		if (m_bIndependent)
			s += " indep";
	}
	else
	{
		SParticleCounts counts;
		GetCounts(counts, false);
		s += string().Format(" E=%.0f P=%.0f R=%0.f", counts.EmittersActive, counts.ParticlesActive, counts.ParticlesRendered);
	}
	
	switch (GetState())
	{
		case eEmitter_Dead:
			s += " dead";
			break;
		case eEmitter_Dormant:
			s += " dormant";
			break;
		case eEmitter_Particles:
			s += " inactive";
			break;
	}
	s += string().Format(" age=%.3f", GetAge());
	return s;
}

void CParticleEmitter::SetEffect( IParticleEffect const* pEffect, ParticleParams const* pParams )
{
  FUNCTION_PROFILER_SYS(PARTICLE);

	if (m_pTopEffect != (CParticleEffect const *)pEffect)
	{
		// Clear any existing emitters only if changing effect.
		m_pTopEffect = (CParticleEffect*)pEffect;
		m_SubEmitters.clear();
	}

	// Create or update emitters to reflect effect tree. Works correctly in game or from editor changes.
	for_all (m_SubEmitters).GetContainer().SetUsed(false, true);
	AddEffect(0, pEffect, pParams);
	ClearUnused();
	m_nEnvFlags = GetEnvironmentFlags() | ENV_WATER;

	if (BoundsTypes() & 1)
		// Normally, use emitter's designated position for sorting, and bounds are not controllable by designer.
		// However, if all bounds are computed dynamically, they might not contain the origin, so avoid this option.
		SetRndFlags(ERF_REGISTER_BY_POSITION, true);
	else
		SetRndFlags(ERF_REGISTER_BY_POSITION, false);

	if (m_bIndependent)
	{
		// Do not allow immortal effects on independent emitters.
		bool bImmortal = false;
		if (m_pTopEffect)
			bImmortal = m_pTopEffect->IsImmortal();
		else if (m_SubEmitters.size() > 0)
			bImmortal = m_SubEmitters[0].GetParams().IsImmortal();
		if (bImmortal)
			Warning("Independent immortal particle effect spawned: %s", pEffect->GetName());
	}
}

void CParticleEmitter::SetLoc( QuatTS const& qp )
{
	CParticleLocEmitter::SetLoc(qp);
	m_VisEnviron.Invalidate();
}

void CParticleEmitter::SetSpawnParams( SpawnParams const& spawnParams, GeomRef geom )
{
	TParent::SetSpawnParams(spawnParams, geom);
	if (m_SpawnParams.fPulsePeriod > 0.f && m_SpawnParams.fPulsePeriod < 0.1f)
		Warning("Particle emitter (effect %s) PulsePeriod %g too low to be useful", 
			GetName(), m_SpawnParams.fPulsePeriod);
}

void CParticleEmitter::Activate( bool bActive )
{
	TParent::Activate(bActive ? eAct_Activate : eAct_Deactivate );
	if (bActive)
		m_pPartManager->RegisterEmitter(this);
}

void CParticleEmitter::UpdateFromEntity() 
{
  FUNCTION_PROFILER_SYS(PARTICLE);

	m_bSelected = false;

	// Get emitter entity.
	IEntity* pEntity = GetSystem()->GetIEntitySystem()->GetEntity(m_nEntityId);

	// Set external target.
	if (!m_Target.bPriority)
	{
		ParticleTarget target;
		if (pEntity)
		{
			for (IEntityLink* pLink = pEntity->GetEntityLinks(); pLink; pLink = pLink->next)
			{
				if (!stricmp(pLink->name, "Target") || !strnicmp(pLink->name, "Target-", 7))
				{
					IEntity* pTarget = GetSystem()->GetIEntitySystem()->GetEntity(pLink->entityId);
					if (pTarget)
					{
						target.vTarget = pTarget->GetPos();
						target.bTarget = true;
						if (pLink->name[6] == '-')
						{
							for (int c = 7; c < sizeof(pLink->name) && pLink->name[c]; c++)
							{
								switch (pLink->name[c])
								{
									case 's':
									case 'S':
										target.bExtendSpeed = true;
										break;
									case 'l':
									case 'L':
										target.bExtendLife = true;
										break;
									case 'c':
									case 'C':
										target.bExtendCount = true;
										break;
								}
							}
						}
						break;
					}
				}
			}
		}
		SetTarget(target);
	}

	// Get entity of attached parent.
	if (pEntity)
	{
		if (pEntity->GetParent())
			pEntity = pEntity->GetParent();

		if (m_SpawnParams.eAttachType != GeomType_None)
		{
			// If entity attached, find attached geometry on parent.
			GeomRef geom;

			int nStart = m_SpawnParams.nAttachSlot < 0 ? 0 : m_SpawnParams.nAttachSlot;
			int nEnd = m_SpawnParams.nAttachSlot < 0 ? 	pEntity->GetSlotCount() : m_SpawnParams.nAttachSlot+1;
			for (int nSlot = nStart; nSlot < nEnd; nSlot++)
			{
				SEntitySlotInfo slotInfo;
				if (pEntity->GetSlotInfo( nSlot, slotInfo ))
				{
					geom.m_pStatObj = slotInfo.pStatObj;
					geom.m_pChar = slotInfo.pCharacter;
					if (geom)
					{
						if (slotInfo.pWorldTM)
							SetMatrix(*slotInfo.pWorldTM);
						break;
					}
				}
			}

			// Set physical entity as well.
			geom.m_pPhysEnt = pEntity->GetPhysics();

			SetEmitGeom(geom);
		}

		// Get velocity from entity.
		IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();
		if (pPhysEnt)
		{
			pe_status_dynamics dyn;
			if (pPhysEnt->GetStatus(&dyn))
			{
				m_vVel = dyn.v;
			}
		}

		// Flag whether selected.
		if (GetSystem()->IsEditorMode())
		{
			IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
			if (pRenderProxy)
			{
				IRenderNode *pRenderNode = pRenderProxy->GetRenderNode();
				if (pRenderNode)
					m_bSelected = (pRenderNode->GetRndFlags() & ERF_SELECTED) != 0;
			}
		}
	}
}

void CParticleEmitter::GetLocalBounds( AABB& bbox )
{
	if (m_bbWorld.IsReset())
	{
		bbox.min = bbox.max = Vec3(0);
	}
	else
	{
		bbox.min = m_bbWorld.min - m_qpLoc.t;
		bbox.max = m_bbWorld.max - m_qpLoc.t;
	}
}

float CParticleEmitter::GetViewDistRatio() const
{
	return m_pPartManager->GetRenderCamera().GetAngularResolution() / 
		max(GetCVars()->e_particles_min_draw_pixels, 0.125f) * 
		GetViewDistRatioNormilized();
}

float CParticleEmitter::GetMaxViewDist()
{
	return GetMaxParticleSize(true) * GetViewDistRatio();
}

void CParticleEmitter::Update()
{
  FUNCTION_PROFILER_SYS(PARTICLE);

	if (GetCVars()->e_particles_debug & AlphaBit('z'))
		return;

	UpdateFromEntity();

	if (m_SubEmitters.size() && m_SubEmitters[0].GetParams().bBindEmitterToCamera)
		SetMatrix( GetRenderer()->GetCamera().GetMatrix() );

	static const float fENV_BOX_INITIAL = 20.f, fENV_BOX_EXPAND = 10.f;
	if (!m_SpawnParams.bIgnoreLocation && m_bbWorldEnv.IsReset())
	{
		// For first-computed bounds, query environment in an area around origin.
		m_bbWorldEnv.Add(m_qpLoc.t, fENV_BOX_INITIAL);
		m_PhysEnviron.GetPhysAreas( this, m_bbWorldEnv, m_nEnvFlags );
	}

	// Update subemitters, and generate emitter bounding box(es).
	m_bbWorld.Reset();
	m_bbWorldDyn.Reset();

	for_array (i, m_SubEmitters)
	{
		m_SubEmitters[i].Update();
		if (m_SubEmitters[i].GetState() >= eEmitter_Particles)
			m_SubEmitters[i].GetContainer().UpdateBounds(m_bbWorld, m_bbWorldDyn);
	}

	if (!m_SpawnParams.bIgnoreLocation)
	{
		m_VisEnviron.Update(m_qpLoc.t, m_bbWorld);
		if (!m_bbWorldEnv.ContainsBox(m_bbWorld) || m_pPartManager->GetUpdatedAreaBB().IsIntersectBox(m_bbWorld))
		{
			// When emitter leaves valid range, or areas have been updated, requery environment.
			m_bbWorldEnv = m_bbWorld;
			m_bbWorldEnv.Expand( Vec3(fENV_BOX_EXPAND) );
			m_PhysEnviron.GetPhysAreas( this, m_bbWorldEnv, m_nEnvFlags );
		}
	}
}

void CParticleEmitter::EmitParticle( IStatObj* pStatObj, IPhysicalEntity* pPhysEnt, QuatTS* pLocation, Vec3* pVel )
{
	if (m_bRegistered && m_SubEmitters.size())
	{
		SParticleUpdateContext context;
		m_SubEmitters[0].EmitParticle( context, true, 0.f, pStatObj, pPhysEnt, pLocation, pVel );
	}
}

void CParticleEmitter::SetEntity( IEntity* pEntity, int nSlot )
{
	m_nEntityId = pEntity ? pEntity->GetId() : 0;
	m_nEntitySlot = nSlot;
	UpdateFromEntity();
	if (m_bIndependent)
		m_nEntityId = 0;
	for_all (m_SubEmitters).ResetLoc();
}

bool CParticleEmitter::Render( SRendParams const& RenParams )
{
	if (!GetCVars()->e_particles)
		return false;

	if (m_nRenderStackLevel > 0 && m_SubEmitters.size() && m_SubEmitters[0].GetParams().bBindEmitterToCamera)
		return false;

  FUNCTION_PROFILER_SYS(PARTICLE);

	// Calculate contribution of fog volumes in scene
	m_timeLastRendered = GetTimer()->GetFrameStartTime();
	ColorF fogVolumeContrib;
	CFogVolumeRenderNode::TraceFogVolumes( m_qpLoc.t, fogVolumeContrib );
	uint16 nFogIdx = GetRenderer()->PushFogVolumeContribution( fogVolumeContrib );
	float fCamDist = m_bbWorld.GetDistance(GetRenderer()->GetCamera().GetPosition());
	SPartRenderParams PRParams( GetRenderer()->GetCamera(), fCamDist, GetViewDistRatio(), nFogIdx );

	// Render all (direct) subemitters, and indirect children.
	for_all (m_SubEmitters).GetContainer().Render( RenParams, PRParams );

	// Signal the particle thread to update - we do this here to reduce the overhead.
	m_pPartManager->WakeParticleThread(false);

	return true;
}

void CParticleEmitter::RenderDebugInfo()
{
	if (TimeNotRendered() < 0.25f && (m_bSelected || (GetCVars()->e_particles_debug & AlphaBit('b'))))
	{
		// Draw bounding box.
		CCamera const& cam = GetRenderer()->GetCamera();
		ColorF color(1,1,1,1);

		// Compute label position, in bb clipped to camera.
		AABB const& bb = GetBBox();
		Vec3 vLabel = bb.GetCenter();

		// Clip to cam frustum.
		float fBorder = 1.f; 
		for (int i = 0; i < FRUSTUM_PLANES; i++)
		{
			Plane const& pl = *cam.GetFrustumPlane(i);
			float f = pl.DistFromPlane(vLabel) + fBorder;
			if (f > 0.f)
				vLabel -= pl.n * f;
		}
		Vec3 vDist = vLabel - cam.GetPosition();
		vLabel += (cam.GetViewdir() * (vDist * cam.GetViewdir()) - vDist) * 0.1f;
		vLabel.CheckMax(bb.min);
		vLabel.CheckMin(bb.max);

		if (!m_bSelected)
		{
			// Randomize hue by entity.
			uint32 uRand = UINT_PTR(this);
			if (BoundsTypes() & 2)
			{
				color.r = 1.f;
				color.g = ((uRand>>8)&0x3F) * (0.5f/63.f) + 0.25f;
				color.b = ((uRand>>14)&0x3F) * (0.5f/63.f) + 0.25f;
			}
			else
			{
				color.r = ((uRand>>2)&0x3F) * (0.5f/63.f) + 0.25f;
				color.g = ((uRand>>8)&0x3F) * (0.75f/63.f) + 0.25f;
				color.b = ((uRand>>14)&0x3F) * (0.75f/63.f) + 0.25f;
			}

			// Adjust by view angle and distance.
			Vec3 vDist = vLabel - cam.GetPosition();
			float fDist = vDist.NormalizeSafe();
			float fDistEmph = 1.f / max(fDist * 0.05f, 1.f);

			float fViewCos = cam.GetViewdir().GetNormalized() * vDist;
			float fAngleEmph = sqr(sqr(fViewCos));

			color.a = max((fDistEmph + fAngleEmph) * 0.5f, 0.4f);
		}

		IRenderAuxGeom* pRenAux = GetRenderer()->GetIRenderAuxGeom();
		pRenAux->SetRenderFlags(SAuxGeomRenderFlags());

		// Compare static and dynamic boxes.
		if (!m_bbWorldDyn.IsReset())
		{
			if (m_bbWorldDyn.min != bb.min || m_bbWorldDyn.max != bb.max)
			{
				// Separate dynamic BB.
				// Color outlying points bright red, draw connecting lines.
				ColorF colorGood = color * 0.6f;
				ColorF colorBad = Col_Red;
				Vec3 vStat[8], vDyn[8];
				ColorB clrDyn[8];
				for (int i = 0; i < 8; i++)
				{
					vStat[i] = Vec3( i&1 ? bb.max.x : bb.min.x, i&2 ? bb.max.y : bb.min.y, i&4 ? bb.max.z : bb.min.z );
					vDyn[i] = Vec3( i&1 ? m_bbWorldDyn.max.x : m_bbWorldDyn.min.x, i&2 ? m_bbWorldDyn.max.y : m_bbWorldDyn.min.y, i&4 ? m_bbWorldDyn.max.z : m_bbWorldDyn.min.z );
					clrDyn[i] = bb.IsContainPoint(vDyn[i]) ? colorGood : colorBad;
					pRenAux->DrawLine(vStat[i], color, vDyn[i], clrDyn[i]);
				}
				
				// Draw dyn bb.
				if (bb.ContainsBox(m_bbWorldDyn))
				{
					pRenAux->DrawAABB(m_bbWorldDyn, false, colorGood, eBBD_Faceted);
				}
				else
				{
					pRenAux->DrawLine(vDyn[0], clrDyn[0], vDyn[1], clrDyn[1]);
					pRenAux->DrawLine(vDyn[0], clrDyn[0], vDyn[2], clrDyn[2]);
					pRenAux->DrawLine(vDyn[0], clrDyn[0], vDyn[4], clrDyn[4]);

					pRenAux->DrawLine(vDyn[1], clrDyn[1], vDyn[3], clrDyn[3]);
					pRenAux->DrawLine(vDyn[1], clrDyn[1], vDyn[5], clrDyn[5]);

					pRenAux->DrawLine(vDyn[2], clrDyn[2], vDyn[3], clrDyn[3]);
					pRenAux->DrawLine(vDyn[2], clrDyn[2], vDyn[6], clrDyn[6]);

					pRenAux->DrawLine(vDyn[3], clrDyn[3], vDyn[7], clrDyn[7]);

					pRenAux->DrawLine(vDyn[4], clrDyn[4], vDyn[5], clrDyn[5]);
					pRenAux->DrawLine(vDyn[4], clrDyn[4], vDyn[6], clrDyn[6]);

					pRenAux->DrawLine(vDyn[5], clrDyn[5], vDyn[7], clrDyn[7]);

					pRenAux->DrawLine(vDyn[6], clrDyn[6], vDyn[7], clrDyn[7]);
				}
			}
			else
			{
				// Identical static & dynamic, presumably dynamic, render in brighter color.
				color = Col_Yellow;
			}
		}

		pRenAux->DrawAABB(bb, false, color, eBBD_Faceted);

		SParticleCounts counts;
		GetCounts(counts, false);
		char sLabel[256];
		sprintf(sLabel, "%s: E=%.0f P=%.0f R=%.0f", 
			GetName(), counts.EmittersActive, counts.ParticlesActive, counts.ParticlesRendered );
		if (counts.ParticlesCollideTerrain)
			sprintf(sLabel+strlen(sLabel), " ColTr=%.0f", counts.ParticlesCollideTerrain);
		if (counts.ParticlesCollideObjects)
			sprintf(sLabel+strlen(sLabel), " ColOb=%.0f", counts.ParticlesCollideObjects);
		if (counts.ParticlesClip)
			sprintf(sLabel+strlen(sLabel), " Clip=%.0f", counts.ParticlesClip);
		if (BoundsTypes() & 2)
		{
			strcat(sLabel, " Dyn");
			if (BoundsTypes() & 1)
				strcat(sLabel, "+Stat");
		}
		GetRenderer()->DrawLabelEx( vLabel, 2.f * color.a, (float*)&color, true, true, sLabel );
	}
}

void CParticleEmitter::Serialize(TSerialize ser)
{
	ser.BeginGroup("Emitter");

	// Effect.
	string sEffect = GetEffect() ? GetEffect()->GetName() : "";
	ser.Value("Effect", sEffect);

	// Time value.
	CTimeValue timeCreated = m_timeCreated;
	ser.Value("CreationTime", timeCreated);
	ser.Value("StopAge", m_fStopAge);
	ser.Value("Active", m_bActive);

	// Location.
	ser.Value("Pos", m_qpLoc.t);
	ser.Value("Rot", m_qpLoc.q);
	ser.Value("Scale", m_qpLoc.s);

	// Spawn params.
	ser.Value("SizeScale", m_SpawnParams.fSizeScale);
	ser.Value("SpeedScale", m_SpawnParams.fSpeedScale);
	ser.Value("CountScale", m_SpawnParams.fCountScale);
	ser.Value("CountPerUnit", m_SpawnParams.bCountPerUnit);
	ser.Value("PulsePeriod", m_SpawnParams.fPulsePeriod);

	if (ser.IsReading())
	{
		SetEffect(m_pPartManager->FindEffect(sEffect));
		m_timeCreated = timeCreated;
	}

	ser.EndGroup();
}

void CParticleEmitter::GetMemoryUsage(ICrySizer* pSizer)
{
	CParticleLocEmitter::GetMemoryUsage(pSizer);
	pSizer->AddObject(&m_poolLocEmitters, m_poolLocEmitters.GetTotalAllocatedMemory());
	pSizer->AddObject(&m_poolSubEmitters, m_poolSubEmitters.GetTotalAllocatedMemory());
}
