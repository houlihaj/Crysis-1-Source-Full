////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   particle.cpp
//  Created:     28/5/2001 by Vladimir Kajalin
//  Modified:    11/3/2005 by Scott Peter
//  Compilers:   Visual Studio.NET
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "Particle.h"
#include "terrain.h"
#include "GeomQuery.h"
#include "partman.h"

#define OT_RIGID_PARTICLE 10

static const float fMAX_COLLIDE_DEVIATION = 0.1f;
static const float fMAX_TAIL_DEVIATION = 0.01f;
static const float fMAX_NONUNIFORM_TRAVEL = 1.0f;
static const float fCOLLIDE_BUFFER_DIST = 0.001f;
static const float fREST_VEL						= 0.001f;
static const int nMAX_ITERATIONS = 5;

template<class F>
int SolveQuadratic(F results[2], F a, F b, F c)
{
	// a t^2 + b t + c = 0
	// t = ( -b/2 +- sqrt( (b/2)^2 - a c ) ) / a
	if (a == 0.0)
	{
		// b t + c == 0;  t = -c/b
		if (b == 0.0)
			return 0;
		results[0] = -c/b;
		return 1;
	}
	else if (c == 0.0)
	{
		// a t^2 + b t = 0;  t = {0, -b/a}
		results[0] = 0;
		results[1] = -b/a;
		return 2;
	}
	else
	{
		F r = sqr(0.5*b) - a*c;
		if (r < 0.0)
			return 0;
		else
		{
			r = sqrt_tpl(r);
			F ia = 1.0/a;
			results[0] = (-0.5*b - r) * ia;
			results[1] = (-0.5*b + r) * ia;
			return 2;
		}
	}
}

template<class F>
F SolveQuadratic(F fMin, F fMax, F a, F b, F c)
{
	float fRange = fMax-fMin;
	float fRes[2], fFirst = fMax+fRange;
	int n = SolveQuadratic( fRes, a, b, c );
	while (n-- > 0)
		if (fRes[n]*fRange >= fMin*fRange && fRes[n]*fRange < fFirst*fRange)
			fFirst = fRes[n];
	return fFirst;
}

inline Vec3 ComputeAccel( const Vec3& vTravel, const Vec3& vVel, float fTime, Vec3 const& vWind, float fDrag )
{
	if (fDrag > 1e-6f)
	{
		double fDecay = exp(-fDrag * fTime);
		float fT = (1.0 - fDecay) / fDrag;
		Vec3 vTerm = (vTravel - vVel * fT) / (fTime-fT);
		return (vTerm - vWind) * fDrag;
	}
	else
	{
		return (vTravel - vVel * fTime) / (fTime*fTime*0.5f);
	}
}

float InterpVariance(Vec3 const& v0, Vec3 const& v1, Vec3 const& v2)
{
	/*
		(dist(v0,v1) + dist(v1,v2))^2 - dist(v0,v2)^2
		= dist(v0,v1)^2 + dist(v1,v2)^2 + 2 dist(v0,v1) dist(v1,v2) - dist(v0,v2)^2
	*/
	Vec3 v01 = v1-v0,
			 v12 = v2-v1,
			 v02 = v2-v0;
	return v01*v01 + v12*v12 - v02*v02 + 2.f * v01.GetLength() * v12.GetLength();
}

float ComputeMaxStep( Vec3 const& vDeviate, float fStepTime, float fMaxDeviate )
{
	float fDevSqr = vDeviate.GetLengthSquared();
	if (fDevSqr > sqr(fMaxDeviate))
		return fMaxDeviate * isqrt_tpl(fDevSqr) * fStepTime;
	return fStepTime;
}

void TravelLinear( Vec3& vPos, Vec3& vVel, float& fTime, Vec3 const& vAccel, Vec3 const& vWind, float fDrag, float fMaxDev, float fMinTime )
{
	// First limit path curve deviation to max 90 deg.
	if (fMaxDev > 0.f)
	{
		float fBendTime = div_min(-(vVel*vVel), vVel*vAccel, fTime);
		if (fBendTime > 0.f)
			fTime = min(fTime, max(fBendTime, fMinTime));
	}

	// Limit travel time to stay within max deviation from linear path.
	Vec3 vPos0 = vPos, vVel0 = vVel;
	for (;;)
	{
		Travel( vPos, vVel, fTime, vAccel, vWind, fDrag );
		if (fMaxDev <= 0.f)
			return;
		Vec3 vDev = vVel-vVel0;
		float fDevSq = vDev.GetLengthSquared() * sqr(fTime*0.125f);
		if (fDevSq > sqr(fMaxDev))
		{
			Vec3 vLin = (vPos-vPos0).GetNormalized();
			vDev -= vLin * (vDev*vLin);
			fDevSq = vDev.GetLengthSquared() * sqr(fTime*0.125f);
			if (fDevSq > sqr(fMaxDev))
			{
				float fLinTime = sqrt_tpl(fMaxDev * isqrt_tpl(fDevSq)) * fTime * 0.75f;
				fLinTime = max(fLinTime, fMinTime);
				if (fLinTime < fTime)
				{
					fTime = fLinTime;
					vPos = vPos0;
					vVel = vVel0;
					continue;
				}
			}
		}
		break;
	}
}

bool CParticle::Init( SParticleUpdateContext const& context, float fAge, CParticleSubEmitter* pEmitter, 
	QuatTS const& loc, bool bFixedLoc, Vec3* pVel, IStatObj* pStatObj, IPhysicalEntity* pPhysEnt )
{
	m_pEmitter = pEmitter;
	pEmitter->AddParticleRef(1);

	ResourceParticleParams const& params = GetParams();

	// Init all base values.
	float fEmissionRelAge = GetEmitter().GetRelativeAge(-fAge);

	m_fAge = m_fRelativeAge = 0.f;
	m_fLifeTime = params.fParticleLifeTime.GetVarValue(fEmissionRelAge);
	if (m_fLifeTime == 0.f)
	{
		if (params.fParticleLifeTime.GetMaxValue() == 0.f)
			m_fLifeTime = fHUGE;
		else
			return false;
	}

	// Init base mods.
	SetVarMod( m_BaseMods.Size, params.fSize, fEmissionRelAge );
	SetVarMod( m_BaseMods.TailLength, params.fTailLength, fEmissionRelAge );
	SetVarMod( m_BaseMods.Stretch, params.fStretch, fEmissionRelAge );

	SetVarMod( m_BaseMods.AirResistance, params.fAirResistance, fEmissionRelAge );
	SetVarMod( m_BaseMods.GravityScale, params.fGravityScale, fEmissionRelAge );
	SetVarMod( m_BaseMods.Turbulence3DSpeed, params.fTurbulence3DSpeed, fEmissionRelAge );
	SetVarMod( m_BaseMods.TurbulenceSize, params.fTurbulenceSize, fEmissionRelAge );
	SetVarMod( m_BaseMods.TurbulenceSpeed, params.fTurbulenceSpeed, fEmissionRelAge );

	if (params.fLightSourceIntensity.GetMaxValue() > 0.f)
	{
		SetVarMod( m_BaseMods.LightSourceIntensity, params.fLightSourceIntensity, fEmissionRelAge );
		SetVarMod( m_BaseMods.LightHDRDynamic, params.fLightHDRDynamic, fEmissionRelAge );
		SetVarMod( m_BaseMods.LightSourceRadius, params.fLightSourceRadius, fEmissionRelAge );
	}
	else
		m_BaseMods.LightSourceRadius = m_BaseMods.LightHDRDynamic = m_BaseMods.LightSourceRadius = 0xFF;
			
	SetVarBase( m_BaseColor, params.cColor, fEmissionRelAge );
	SetVarBase( m_BaseColor.a, params.fAlpha, fEmissionRelAge );

	m_nTileVariant = params.TextureTiling.nVariantCount > 1 ? Random(params.TextureTiling.nVariantCount) : 0;

	if (pPhysEnt && pStatObj)
	{
		// Pre-generated physics entity.
		m_pPhysEnt = pPhysEnt;
		m_pStatObj = pStatObj;
		pe_params_foreign_data pfd;
		pfd.iForeignData = OT_RIGID_PARTICLE;
		pfd.pForeignData = pStatObj;
		pPhysEnt->SetParams(&pfd);

		// Get initial state.
		GetPhysicsState();
	}
	else
	{
		// Set geometry.
		m_pStatObj = pStatObj ? pStatObj : (IStatObj*)params.pStatObj;
		if (!bFixedLoc || !pVel)
			InitPos( context, loc, fEmissionRelAge );
		if (bFixedLoc)
		{
			m_vPos = loc.t;
			m_qRot = loc.q;
			GetContainer().SetDynamicBounds();
		}
		if (pVel)
		{
			m_vVel = *pVel;
			GetContainer().SetDynamicBounds();
		}

		if (GetContainer().GetHistorySteps() > 0)
		{
			// Init history list.
			m_aPosHistory = GetContainer().AllocPosHistory();
			for (int n = 0; n < GetContainer().GetHistorySteps(); n++)
				m_aPosHistory[n].SetUnused();
		}

		if (params.ePhysicsType == ParticlePhysics_SimplePhysics || params.ePhysicsType == ParticlePhysics_RigidBody)
		{
			// Emitter-generated physical particles.
			Physicalize( context );
		}
	}

#ifdef PARTICLE_MOTION_BLUR
  m_vPosPrev = m_vPos;
  m_qRotPrev = m_qRot;
#endif

	// Add reference to the parent of the stat obj.
	if (m_pStatObj)
		m_pStatObj->AddRef();
	if (m_pPhysEnt && GetContainer().GetMain().GetVisEnviron().OriginIndoors())
	{
		pe_params_flags pf;
		pf.flagsOR = pef_ignore_ocean;
		m_pPhysEnt->SetParams(&pf);
	}

	// Spawn any indirect emitters.
	m_pChildEmitter = GetContainer().CreateIndirectEmitter(&GetEmitter(), fAge);
	if (m_pChildEmitter)
		UpdateChildEmitter();

	if (fAge > 0.f)
		return Update(context, fAge);
	else if (m_fLifeTime == 0.f)
		return !EndParticle();
	else
		return true;
}

void CParticle::InitPos( SParticleUpdateContext const& context, QuatTS const& loc, float fEmissionRelAge )
{
	ResourceParticleParams const& params = GetParams();

	// Position and direction.
	m_vPos = loc.t;
	m_vVel = loc.q * Vec3(0,1,0);

	// Emit geom.
	EGeomType	eAttachType = params.bSecondGeneration ? params.eAttachType : GetSpawnParams().eAttachType;
	if (eAttachType != GeomType_None && GetLoc().m_EmitGeom)
	{
		RandomPos ran;
		QuatTS locGeom = loc;
		EGeomForm	eAttachForm = params.bSecondGeneration ? params.eAttachForm : GetSpawnParams().eAttachForm;
		if (GetRandomPos( ran, GetLoc().m_GeomQuery, GetLoc().m_EmitGeom, eAttachType, eAttachForm, locGeom ) && !ran.vNorm.IsZero())
		{
			m_vPos = ran.vPos;
			m_vVel = ran.vNorm;
		}
	}

	// Offsets.
	if (params.bSpaceLoop && params.fCameraMaxDistance > 0.f)
	{
		RandomPos ran;
		BoxRandomPos( ran, GeomForm_Volume, Vec3(0.5f) );
		m_vPos = context.matSpaceLoop * ran.vPos;
	}
	else
	{
		Vec3 vOffset = params.vPositionOffset;
		if (params.fPosRandomOffset)
			vOffset += SphereRandom(params.fPosRandomOffset);
		if (!params.vRandomOffset.IsZero())
			vOffset += BiRandom(params.vRandomOffset);

		// To world orientation/scale.
		m_vPos += loc.q * vOffset * GetScale();
	}

	// Emit direction.
	if (params.bFocusGravityDir && !context.PhysEnv.m_vUniformGravity.IsZero())
		m_vVel = -context.PhysEnv.m_vUniformGravity.GetNormalized();

	float fFocusAngle = GetEmitter().GetCurValue(params.fFocusAngle);
	if (fFocusAngle != 0.f)
	{
		float fAzimuth = GetEmitter().GetCurValue(params.fFocusAzimuth, 360.f);

		Quat q;
		if (params.bFocusGravityDir && !context.PhysEnv.m_vUniformGravity.IsZero())
			q.SetRotationV0V1(Vec3(0,1,0), m_vVel);
		else
			q = loc.q;

		// Rotate focus about X.
		Vec3 vRot = q * Vec3(1,0,0);
		m_vVel = Quat::CreateRotationAA(DEG2RAD(fAzimuth), m_vVel) * (Quat::CreateRotationAA(DEG2RAD(fFocusAngle), vRot) * m_vVel);
	}

	// Velocity.
	float fPhi = params.fEmitAngle.GetVarValue(fEmissionRelAge);
	if (fPhi > 0.f)
	{
		//
		// Adjust angle to create a uniform distribution.
		//
		//		density distribution d(phi) = sin phi
		//		cumulative density c(phi) = Int(0,phi) sin x dx = 1 - cos phi
		//		normalised cn(phi) = (1 - cos phi) / (1 - cos phiMax)
		//		reverse function phi(cn) = acos_tpl(1 + cn(cos phiMax - 1))
		//

		float fPhiMax = params.fEmitAngle.GetMaxValue();
		fPhi /= fPhiMax;
		fPhi = cry_acosf( 1.f + fPhi * (cry_cosf(DEG2RAD(fPhiMax))-1.f) );

		float fTheta = Random(DEG2RAD(360));

		float fPhiCS[2], fThetaCS[2];
		sincos_tpl(fPhi, &fPhiCS[1], &fPhiCS[0]);
		sincos_tpl(fTheta, &fThetaCS[1], &fThetaCS[0]);

		// Compute perpendicular bases.
		Vec3 vX;
		if (m_vVel.z != 0.f)
			vX( 0.f, -m_vVel.z, m_vVel.y );
		else
			vX( -m_vVel.y, m_vVel.x, 0.f );
		vX.NormalizeFast();
		Vec3 vY = m_vVel ^ vX;

		m_vVel = m_vVel * fPhiCS[0] + (vX * fThetaCS[0] + vY * fThetaCS[1]) * fPhiCS[1];
	}

	// Orientation.
	if (params.Has3DRotation())
	{
		// 3D. Compute quat from init angles.
		Vec3 vAngles = DEG2RAD( params.vInitAngles + BiRandom(params.vRandomAngles) );

		// Convert to world space.
		m_qRot = loc.q * exp( vAngles * 0.5f );

		// Angular velocity.
		m_vRotVel = loc.q * DEG2RAD( params.vRotationRate + BiRandom(params.vRandomRotationRate) );

		if (params.eFacing == ParticleFacing_Velocity)
		{
			// Track 3D rotation, ensuring Y always faces movement dir.
			RotateTo(m_vVel.GetNormalized());
		}

		// Confine horizontal particles.
		else if (params.eFacing == ParticleFacing_Horizontal)
		{
			Vec3 vY = loc.q * Vec3(0,1,0);
			m_vVel -= vY * (vY * m_vVel);
			m_qRot = loc.q;
		}

		// Confine water-aligned particles to plane.
		else if (params.eFacing == ParticleFacing_Water)
		{
			// Project point and velocity onto plane.
			const float fOFFSET = 0.01f;
			Plane plWater;
			float fDist = context.PhysEnv.DistanceAboveWater(m_vPos, -WATER_LEVEL_UNKNOWN, plWater);
			if (plWater.d < -WATER_LEVEL_UNKNOWN)
			{
				m_vPos -= plWater.n * (fDist - params.vPositionOffset.z - fOFFSET);
				m_vVel -= plWater.n * (m_vVel * plWater.n);
				RotateTo(plWater.n);
			}
			else
			{
				// No water, kill it.
				m_fLifeTime = 0.f;
			}
		}
		else if (params.eFacing == ParticleFacing_Terrain)
		{
			if (GetTerrain())
			{
				CTerrain::SRayTrace rt;
			  if (GetTerrain()->RayTrace(m_vPos, m_vPos - Vec3(0,0,1000), &rt))
				{
					const float fOFFSET = 0.01f;
					m_vPos = rt.vHit + rt.vNorm * fOFFSET;
					m_vVel -= rt.vNorm * (m_vVel * rt.vNorm);
					RotateTo(rt.vNorm);
				}
			}
		}
	}
	else
	{
		// 2D.
		m_qRot = loc.q;
		m_fAngle = DEG2RAD( params.vInitAngles.z + BiRandom(params.vRandomAngles.z) );
		m_vRotVel.x = m_vRotVel.y = 0.f;
		m_vRotVel.z = DEG2RAD( params.vRotationRate.z + BiRandom(params.vRandomRotationRate.z) );
	}
  
	// Speed.
	float fSpeed = params.fSpeed.GetVarValue(fEmissionRelAge);
	m_vVel *= fSpeed * GetSpawnParams().fSpeedScale * GetScale();
	m_vVel += GetLoc().GetVel() * params.fInheritVelocity;

	if (params.bBindToEmitter)
	{
		m_vPos = loc.t;
		m_qRot = loc.q;
		m_vVel.zero();
	}
}

void CParticle::UpdateBounds( AABB& bb ) const
{
	if (!IsAlive())
		return;
	ParticleParams const& params = GetParams();
	if (m_pStatObj)
	{
		float fRadius = m_pStatObj->GetRadius();
		if (params.bNoOffset || params.ePhysicsType == ParticlePhysics_RigidBody)
			// Add object origin.
			fRadius += m_pStatObj->GetAABB().GetCenter().GetLength();
		bb.Add(m_vPos, fRadius * GetSize());
	}
	else
	{
		// Radius is size * sqrt(2), round it up slightly.
		float fRadius = GetSize() * 1.415f;
		bb.Add(m_vPos, fRadius);

		if (params.fStretch.GetMaxValue() != 0.f)
		{
			// Stretch along delta direction.
			bb.Add(m_vPos + m_vVel * (params.fStretch.GetMaxValue() + abs(params.fStretchOffsetRatio)), fRadius);
		}
		if (m_aPosHistory && m_aPosHistory[0].IsUsed())
		{
			// Add oldest element.
			bb.Add(m_aPosHistory[0].vPos, fRadius);
		}
	}
}

void CParticle::UpdateChildEmitter()
{
	if (m_pChildEmitter)
	{
		// Transfer particle's location & velocity.
		QuatTS loc;
		GetRenderLocation(loc);
		m_pChildEmitter->SetLoc(loc);
		m_pChildEmitter->SetVel(m_vVel);

		m_pChildEmitter->m_SpawnParams.fSizeScale = GetSpawnParams().fSizeScale;
		m_pChildEmitter->SetEmitGeom(m_pStatObj);
	}
}

void CParticle::AddPosHistory( SParticleHistory const& histPrev )
{
	if (!m_aPosHistory)
		return;

	// Store previous position in history.
	float fTailLength = GetCurValueMod(GetParams().fTailLength, m_BaseMods.TailLength);

	int nCount = GetContainer().GetHistorySteps();
	while (nCount > 0 && !m_aPosHistory[nCount-1].IsUsed())
		nCount--;

	// To do: subdivide for curvature: vel <> prevvel
	// Clear out old entries.
	float fMinAge = m_fAge - fTailLength;
	int nStart = 0;
	while (nStart+1 < nCount && m_aPosHistory[nStart+1].fAge < fMinAge)
		nStart++;
	
	if (nStart > 0)
	{
		for (int n = nStart; n < nCount; n++)
			m_aPosHistory[n-nStart] = m_aPosHistory[n];
		for (int n = nCount-nStart; n < nCount; n++)
			m_aPosHistory[n].SetUnused();
		nCount -= nStart;
	}

	// Interp oldest entry.
	if (nCount > 1)
	{
		float fT = div_min(fMinAge - m_aPosHistory[0].fAge, m_aPosHistory[1].fAge - m_aPosHistory[0].fAge, 1.f);
		if (fT > 0.f)
		{
			m_aPosHistory[0].fAge = fMinAge;
			m_aPosHistory[0].vPos += (m_aPosHistory[1].vPos - m_aPosHistory[0].vPos) * fT;
		}
	}

	if (nCount == GetContainer().GetHistorySteps())
	{
		// Remove least significant entry.
		int nLeast = nCount;
		float fLeastSignif = InterpVariance(m_vPos, histPrev.vPos, m_aPosHistory[nCount-1].vPos);
		for (int n = 1; n < nCount; n++)
		{
			float fSignif = InterpVariance( 
				n+1 == nCount ? histPrev.vPos : m_aPosHistory[n+1].vPos, 
				m_aPosHistory[n].vPos, 
				m_aPosHistory[n-1].vPos );
			if (fSignif < fLeastSignif)
			{
				fLeastSignif = fSignif;
				nLeast = n;
			}
		}
		if (nLeast != nCount)
		{
			// Delete entry.
			for (int n = nLeast; n < nCount-1; n++)
				m_aPosHistory[n] = m_aPosHistory[n+1];
			nCount--;
		}
	}
	if (nCount < GetContainer().GetHistorySteps())
		m_aPosHistory[nCount] = histPrev;
}

bool CParticle::Update( SParticleUpdateContext const& context, float fFrameTime )
{
	static const float fTIME_EPSILON = 1e-5f;

	ResourceParticleParams const& params = GetParams();
	ParticleTarget const& target = GetTarget();

	bool bExtended = GetContainer().HasExtendedLifetime();

	// Process only up to lifetime of particle.
	if (!bExtended)
	{
		if (m_fAge >= m_fLifeTime)
			return !EndParticle();
		fFrameTime = min(fFrameTime, m_fLifeTime-m_fAge);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Move
	////////////////////////////////////////////////////////////////////////////////////////////////

  if (m_pPhysEnt)
  {
		// Use physics engine to move particles.
		GetPhysicsState();

		// Get collision status.
		pe_status_collisions status;
		coll_history_item item;
		status.pHistory = &item;
		status.age = 1.f;
		status.bClearHistory = 1;
		if (m_pPhysEnt->GetStatus(&status) > 0)
		{
			if (m_pChildEmitter)
				m_pChildEmitter->Activate(eAct_ParentCollision);
			if (params.fBounciness < 0)
				// Kill particle on collision.
				return !EndParticle();
		}
  }
	else if (params.bBindToEmitter)
	{
		m_vPos = GetLoc().m_qpLoc.t;
		m_qRot = GetLoc().m_qpLoc.q;
	}
	else if (fFrameTime > 0.f)
	{
		uint nFlags = context.nEnvFlags & (ENV_GRAVITY | ENV_WIND);
		uint nCollideFlags = context.nEnvFlags & (ENV_TERRAIN | ENV_STATIC_ENT | ENV_DYNAMIC_ENT);
		float fMaxStepTime = fFrameTime;
		float fMaxDev = 0.f;

		// Test for excessive tail curvature (undesirable for purebreds).
		if (fFrameTime * GetContainer().GetHistorySteps() > params.fTailLength.GetMaxValue())
			fMaxDev = fMAX_TAIL_DEVIATION;
		if (nCollideFlags)
			fMaxDev = fMAX_COLLIDE_DEVIATION;

		int nIterations = 0;
		int nCollisions = 0;

		while (fFrameTime > fTIME_EPSILON && nIterations < nMAX_ITERATIONS)
		{
			float fMinStepTime = fFrameTime / (nMAX_ITERATIONS-nIterations);
			nIterations++;
			Vec3 vPrevPos = m_vPos;
			Vec3 vPrevVel = m_vVel;

			SParticleHistory histPrev;
			histPrev.fAge = m_fAge;
			histPrev.vPos = m_vPos;

#ifdef _DEBUG
			// Backup point.
			m_vPos = vPrevPos;
			m_vVel = vPrevVel;
#endif

			// Apply previously computed MaxStepTime.
			float fStepTime = min(fFrameTime, fMaxStepTime);
			fMaxStepTime = fFrameTime;

			// Legacy 2D spiral turbulence.
			if (params.fTurbulenceSize.GetBase()*params.fTurbulenceSpeed.GetBase() != 0.f)
			{
				// p(t) = TSize e^(i TSpeed t) (t max 1)
				// Do not adjust velocity, this is an artificial spiral motion.

				// Subtract pos from start angle.
				float fVortexSpeed = DEG2RAD( GetCurValueMod(params.fTurbulenceSpeed, m_BaseMods.TurbulenceSpeed) );
				float fVortexSize = GetCurValueMod( params.fTurbulenceSize, m_BaseMods.TurbulenceSize ) * GetScale();

				Vec2 vPos;
				cry_sincosf( fVortexSpeed * m_fAge, &vPos.y, &vPos.x );  vPos *= fVortexSize;		
				m_vPos.x -= vPos.x;   m_vPos.y -= vPos.y;

				// Add pos from end angle.
				float fEndAge = m_fAge + fStepTime;
				float fEndRelAge = div_min(fEndAge, m_fLifeTime, 1.f);
				fVortexSpeed = DEG2RAD( GetCurValueMod(params.fTurbulenceSpeed, m_BaseMods.TurbulenceSpeed) );
				fVortexSize = GetCurValueMod( params.fTurbulenceSize, m_BaseMods.TurbulenceSize ) * GetScale();

				cry_sincosf( fVortexSpeed * fEndAge, &vPos.y, &vPos.x );  vPos *= fVortexSize;		
				m_vPos.x += vPos.x;   m_vPos.y += vPos.y;
			}

			// Evaluate params at halfway point thru frame.
			m_fRelativeAge = div_min(m_fAge + fStepTime*0.5f, m_fLifeTime, 1.f);

			Vec3 vGrav(ZERO);
			Vec3 vWind(ZERO);
			float fDrag = GetCurValueMod(params.fAirResistance, m_BaseMods.AirResistance);
			Vec3 vAccel = params.vAcceleration;

			if (nFlags)
			{
				context.PhysEnv.CheckPhysAreas( m_vPos, vGrav, vWind, nFlags );
				float fGrav = GetCurValueMod(params.fGravityScale, m_BaseMods.GravityScale);
				vAccel += vGrav * fGrav;
			}

			if (params.fTurbulence3DSpeed.GetMaxValue() != 0.f)
			{
				/*
					Random impulse dvm applied every time quantum tm.
					From random walk formula, the average velocity magnitude after time t
					(without other forces such as air resistance):
							dv(t) = dvm sqrt(t/tm)
										= vs sqrt(t),			vs := dvm/sqrt(tm)
					The average distance traveled (again with no forces) is then
							d(t) = Int[s=0..t] dv(s)
									 = vs/1.5 t^1.5
					Converting this to an acceleration applied over time dt so it can integrate with other forces:
							a(dt) = dv(dt) / dt = vs / sqrt(dt)
										= d(1) / sqrt(dt)
				*/
				float fMag = GetCurValueMod(params.fTurbulence3DSpeed, m_BaseMods.Turbulence3DSpeed) * isqrt_tpl(fStepTime);
				vAccel += SphereRandom(fMag);
			}

			// Compute targeting force.
			if (target.bTarget)
			{
				// Compute current travel time to closest distance to target.
				float fTargetTime = fHUGE;
				Vec3 vTravel = target.vTarget - m_vPos;
				float fTravSq = vTravel.GetLengthSquared();
				Vec3 vVelRel = m_vVel - target.vVelocity;
				float fProj = vTravel * vVelRel;
				if (fProj > 0.f)
					fTargetTime = vTravel.GetLengthSquared() / fProj;

				if (!target.bExtendLife && fTargetTime > m_fLifeTime-m_fAge)
				{
					// Shrink time to arrive within lifetime.
					fTargetTime = m_fLifeTime - m_fAge;
				}
				else
				{
					// Age particle prematurely based on target time.
					m_fLifeTime = m_fAge + fTargetTime;
					m_fRelativeAge = div_min( m_fAge + fStepTime*0.5f, m_fLifeTime, 1.f );
				}

				// Allow 1-frame tolerance for goal.
				if (fTargetTime <= fStepTime)
					return !EndParticle();

				// Compute acceleration to reach target, gradually apply it over lifetime.
				Vec3 vTargetAccel = ComputeAccel( vTravel, vVelRel, fTargetTime, vWind, fDrag );
				vAccel += (vTargetAccel-vAccel) * m_fRelativeAge; 
			}

			TravelLinear( m_vPos, m_vVel, fStepTime, vAccel, vWind, fDrag, fMaxDev, fMinStepTime );

			Vec3 vMove = m_vPos-vPrevPos;

			if (nFlags & context.PhysEnv.m_nNonUniformFlags && nIterations < nMAX_ITERATIONS)
			{
				// Subdivide travel in non-uniform areas.
				float fMoveSqr = vMove.GetLengthSquared();
				if (fMoveSqr > sqr(fMAX_NONUNIFORM_TRAVEL))
				{
					Vec3 vGrav2, vWind2;
					context.PhysEnv.CheckPhysAreas( m_vPos, vGrav2, vWind2, nFlags );
					Vec3 vErrorEst = (vGrav2-vGrav)*(0.5f*sqr(fStepTime));

					if (fDrag > 0.f)
					{
						double fDecay = exp(-fDrag * fStepTime);
						float fT = fStepTime - (1.f - fDecay) / fDrag;
						vErrorEst += (vWind2-vWind)*fT;
					}

					fMaxStepTime = ComputeMaxStep(vErrorEst, fStepTime, 0.75f*fMAX_NONUNIFORM_TRAVEL);
					fMaxStepTime = max(fMaxStepTime, fMinStepTime);
					if (fMaxStepTime < fStepTime)
					{
						m_vPos = vPrevPos;
						m_vVel = vPrevVel;
						continue;
					}
				}
			}

			if (params.bSpaceLoop)
			{
				// Wrap the particle into camera-aligned BB.
				Vec3 vPosLocal = context.matSpaceLoopInv * m_vPos;

				Vec3 vCurPos = m_vPos;

				int nCorrect;
				if ((nCorrect = int_round(vPosLocal.x)) != 0)
					m_vPos -= context.matSpaceLoop.GetColumn(0) * nCorrect;
				if ((nCorrect = int_round(vPosLocal.y)) != 0)
					m_vPos -= context.matSpaceLoop.GetColumn(1) * nCorrect;
				if ((nCorrect = int_round(vPosLocal.z)) != 0)
					m_vPos -= context.matSpaceLoop.GetColumn(2) * nCorrect;

				if (context.nSpaceLoopFlags && (m_fAge == 0.f || vCurPos != m_vPos))
				{
					// On init or wrap, set particle lifetime based on object collision.
					float fTBefore = 10.f, fTAfter = m_fLifeTime - m_fAge - fStepTime;
					Vec3 vEndPos = m_vPos, vEndVel = m_vVel;
					if (fTAfter > 0.f)
						Travel( vEndPos, vEndVel, fTAfter, vAccel, vWind, fDrag );
					ray_hit hit;
					if (context.PhysEnv.PhysicsCollision( hit, m_vPos - vEndVel * fTBefore, vEndPos, GetVisibleRadius(), context.nSpaceLoopFlags ))
					{
						float fHitTime = hit.dist * (fTBefore + fTAfter) - fTBefore;
						if (fHitTime <= 0.f)
							return !EndParticle();
						else
							m_fLifeTime = min(m_fLifeTime, m_fAge+fStepTime+fHitTime);
					}
				}

				if (vCurPos != m_vPos)
				{
					// Move prev pos, and expand to space border.
					Vec3 vPrevDelta = vPrevPos-vCurPos;
					if (nCollideFlags)
						vPrevDelta = vPrevDelta.GetNormalized() * params.fCameraMaxDistance;
					vPrevPos = m_vPos + vPrevDelta;
					m_vSlidingNormal.zero();
				}
			}

			////////////////////////////////////////////////////////////////////////////////////////////////
			// Rotation.
			if (params.Has3DRotation())
			{
				// 3D.
				m_qRot = exp(m_vRotVel*(fStepTime*0.5f)) * m_qRot;
			}
			else
			{
				// 2D.
				m_fAngle += m_vRotVel.z*fStepTime;
			}

			if ((params.eFacing == ParticleFacing_Velocity || m_pChildEmitter) && !nCollisions && !m_vVel.IsZero())
				// Track 3D rotation, ensuring Y always faces movement dir.
				RotateTo(m_vVel.GetNormalized());

			////////////////////////////////////////////////////////////////////////////////////////////////
			// Simple collision with physics entities
			////////////////////////////////////////////////////////////////////////////////////////////////

			if (nCollideFlags && 
				(params.nMaxCollisionEvents == 0 || m_nCollisionCount < params.nMaxCollisionEvents) && 
				!vMove.IsZero())
			{
				// Apply sliding plane found in previous iteration.
				float fPen = vMove*m_vSlidingNormal;
				if (fPen < 0.f)
				{
					vMove -= m_vSlidingNormal*fPen;
					fPen = m_vVel*m_vSlidingNormal;
					if (fPen < 0.f)
						m_vVel -= m_vSlidingNormal*fPen;

					// Apply sliding drag.
					float fBounce, fSlidingDrag;
					GetCollisionParams(m_nCollideMaterial, fBounce, fSlidingDrag);
					if (fSlidingDrag > 0.f)
					{
						double fDecay = exp(-fSlidingDrag * fStepTime);
						m_vVel *= fDecay;
						vMove *= (1.0 - fDecay) / (fSlidingDrag*fStepTime);
					}

					if (m_vVel.GetLengthSquared() < sqr(fREST_VEL))
					{
						m_vVel.zero();
						vMove.zero();
					}

					m_vPos = vPrevPos + vMove;

					m_fSlideDistance += vMove.GetLength();
					if (m_fSlideDistance >= fMAX_COLLIDE_DEVIATION)
					{
						m_vSlidingNormal.zero();
						m_fSlideDistance = 0.f;
					}
				}
				else
				{
					m_vSlidingNormal.zero();
					m_fSlideDistance = 0.f;
				}

				ray_hit hit;
				if (context.PhysEnv.PhysicsCollision( hit, vPrevPos, m_vPos, fCOLLIDE_BUFFER_DIST, nCollideFlags ))
				{
					if (hit.bTerrain)
						GetContainer().GetCounts().ParticlesCollideTerrain += 1.f;
					else
						GetContainer().GetCounts().ParticlesCollideObjects += 1.f;

					// Set particle to collision point.
					// Linearly interpolate velocity based on distance.
					m_vVel = vPrevVel*(1.f-hit.dist) + m_vVel*hit.dist;
					m_vPos = hit.pt;

					if (m_pChildEmitter)
					{
						// Momentarily orient the particle to surface normal.
						RotateTo(hit.n);
						UpdateChildEmitter();
						m_pChildEmitter->Activate(eAct_ParentCollision, fFrameTime - fStepTime*hit.dist);
					}

					if (params.fBounciness < 0.f)
					{
						m_fAge += fStepTime*hit.dist;
						m_fRelativeAge = div_min( m_fAge, m_fLifeTime, 1.f );
						fFrameTime = 0;
						return !EndParticle();
					}
					else
					{
						nCollisions++;
						m_nCollisionCount++;
						m_nCollideMaterial = hit.surface_idx;

						// Get phys params from material, or particle params.
						float fBounce, fSlidingDrag;
						GetCollisionParams(m_nCollideMaterial, fBounce, fSlidingDrag);

						// Remove perpendicular velocity component.
						float fVelPerp = m_vVel * hit.n;
						m_vVel -= hit.n * fVelPerp;

						float fVelBounce = fVelPerp * -fBounce;
						if (fVelBounce >= 0.f)
						{
							float fAccelPerp = vAccel * hit.n;
							if (fAccelPerp < 0.f)
							{
								// Sliding occurs when the bounce distance would be less than the collide buffer.
								// d = - v^2 / 2a
								float fBounceDist =  -0.5f * sqr(fVelBounce) / fAccelPerp;
								if (fBounceDist <= fCOLLIDE_BUFFER_DIST*2.f)
								{
									// Evolve over remaining frame time with sliding.
									fBounce = fVelBounce = 0.f;
									m_vSlidingNormal = hit.n;
									m_fSlideDistance = 0.f;
								}
							}
							if (fVelBounce > 0.f && fSlidingDrag > 0.f)
							{
								// Apply sliding drag to parallel velocity component,
								// for period that particle is within collision buffer.
								float fBounceTime = fCOLLIDE_BUFFER_DIST / fVelBounce;
								m_vVel *= exp(-fSlidingDrag * fBounceTime);
							}
						}

						// Bounce the particle.
						m_vVel += hit.n * fVelBounce;
						m_vRotVel *= fBounce;
					}

					fStepTime *= hit.dist;
				}
			}

			fFrameTime -= fStepTime;
			m_fAge += fStepTime;
			m_fRelativeAge = div_min( m_fAge, m_fLifeTime, 1.f );

			AddPosHistory(histPrev);
		}

		GetContainer().GetCounts().ParticlesReiterate += float(nIterations-1);
  }

	m_fAge += fFrameTime;
	m_fRelativeAge = div_min( m_fAge, m_fLifeTime, 1.f );

	// Shrink particle when approaching visible limits.
	m_fClipDistance = FLT_MAX;
	if (!(GetCVars()->e_particles_debug & AlphaBit('c')))
	{
		if (params.tVisibleUnderwater != Trinary_Both)
		{
			// Particles not allowed to cross water boundary, shrink them as they get close.
			if (context.PhysEnv.m_tUnderWater == Trinary_Both)
			{
				Plane pl;
				m_fClipDistance = context.PhysEnv.DistanceAboveWater(m_vPos, GetVisibleRadius(), pl);
				if (params.tVisibleUnderwater == Trinary_If_True)
					m_fClipDistance = -m_fClipDistance;
				if (m_fClipDistance <= 0.f && !params.bSpaceLoop)
					// Kill particle when it crosses water, unless recycled in space loop.
					return !EndParticle();
			}
			else if (context.PhysEnv.m_tUnderWater != params.tVisibleUnderwater)
				return !EndParticle();
		}
	}

	// Update emitter location for indirect particles.
	if (m_pChildEmitter)
		UpdateChildEmitter();

	return true;
}

void CParticle::GetCollisionParams(int nCollSurfaceIdx, float& fBounce, float& fDrag)
{
	// Get phys params from material, or particle params.
	fBounce = GetParams().fBounciness;
	fDrag = GetParams().fDynamicFriction;

	int iSurfaceIndex = GetSurfaceIndex();
	if (iSurfaceIndex  > 0)
	{
		ISurfaceType *pSurfPart = Get3DEngine()->GetMaterialManager()->GetSurfaceType(iSurfaceIndex);
		if (pSurfPart)
		{
			ISurfaceType::SPhysicalParams const& physPart = pSurfPart->GetPhyscalParams();

			// Combine with hit surface params.
			ISurfaceType *pSurfHit = Get3DEngine()->GetMaterialManager()->GetSurfaceType(nCollSurfaceIdx);
			if (!pSurfHit)
				pSurfHit = Get3DEngine()->GetMaterialManager()->GetDefaultTerrainLayerMaterial()->GetSurfaceType();
			ISurfaceType::SPhysicalParams const& physHit = pSurfHit->GetPhyscalParams();

			fBounce = clamp_tpl( (physPart.bouncyness+physHit.bouncyness)*0.5f, 0.f, 1.f );
			fDrag = max( (physPart.friction+physHit.friction)*0.5f, 0.f );
		}
	}
}

bool CParticle::EndParticle()
{
	if (m_fAge < m_fLifeTime)
		m_fLifeTime = m_fAge;
	m_fAge = m_fLifeTime + 0.001f;

  if (m_pPhysEnt)
	{
		GetPhysicalWorld()->DestroyPhysicalEntity(m_pPhysEnt);
		m_pPhysEnt = 0;
	}

	if (m_pChildEmitter)
	{
		// Deactivate (and release) child effects.
		m_pChildEmitter->Activate(eAct_ParentDeath, m_pChildEmitter->GetAge() - m_fAge);
		m_pChildEmitter = 0;
	}

	return true;
}

void CParticle::GetRenderLocation( QuatTS& loc ) const
{
	ResourceParticleParams const& params = GetParams();

	loc.s = GetSize();
	loc.t = m_vPos;
	loc.q = m_qRot;
	if (m_pStatObj && !params.bNoOffset && params.ePhysicsType != ParticlePhysics_RigidBody)
	{
		// Recenter object pre-rotation.
		Vec3 vCenter = m_pStatObj->GetAABB().GetCenter();
		loc.t = loc * -vCenter;
	}
}

CParticle::~CParticle()
{
	// Release emitter reference.
	GetEmitter().AddParticleRef(-1);

	if (m_pChildEmitter)
		// Release child emitter
		m_pChildEmitter->Activate(eAct_ParentDeath, m_pChildEmitter->GetAge() - m_fAge);

  if (m_pPhysEnt)
		GetPhysicalWorld()->DestroyPhysicalEntity(m_pPhysEnt);

	if (m_pStatObj)
		m_pStatObj->Release();

	if (m_aPosHistory)
		GetContainer().FreePosHistory(m_aPosHistory);
}

char GetMinAxis(Vec3 const& vVec)
{
	float x = fabs(vVec.x);
	float y = fabs(vVec.y);
	float z = fabs(vVec.z);

	if(x<y && x<z)
		return 'x';
	
	if(y<x && y<z)
		return 'y';
	
	return 'z';
}

static const float fSPHERE_VOLUME = 4.f / 3.f * g_PI;

int CParticle::GetSurfaceIndex() const
{
	ResourceParticleParams const& params = GetParams();
	if (params.iPhysMat == 0 && m_pStatObj)
	{
		phys_geometry* pGeom = m_pStatObj->GetPhysGeom();
		if (pGeom && pGeom->surface_idx < pGeom->nMats)
			return pGeom->pMatMapping[pGeom->surface_idx];
	}
	return params.iPhysMat;
}

void CParticle::Physicalize( SParticleUpdateContext const& context )
{
	ResourceParticleParams const& params = GetParams();

	Vec3 vGravity = context.PhysEnv.m_vUniformGravity * GetCurValueMod(params.fGravityScale, m_BaseMods.GravityScale) + params.vAcceleration;

	pe_params_pos par_pos;
	par_pos.pos = m_vPos;
	par_pos.q = m_qRot;

	if (params.ePhysicsType == ParticlePhysics_RigidBody)
	{
		if (!m_pStatObj)
			return;

		// Make Physical Rigid Body.
		m_pPhysEnt = GetPhysicalWorld()->CreatePhysicalEntity(PE_RIGID,&par_pos,m_pStatObj,OT_RIGID_PARTICLE );
		 
		pe_geomparams partpos;
		partpos.density = params.fDensity;
		partpos.scale = GetSize();
		partpos.flagsCollider = geom_colltype_debris;
		partpos.flags &= ~geom_colltype_debris; // don't collide with other particles.
		partpos.surface_idx = params.iPhysMat;
		m_pPhysEnt->AddGeometry( m_pStatObj->GetPhysGeom(),&partpos,0 );
		
		pe_simulation_params symparams;
		symparams.minEnergy = (0.2f)*(0.2f);
		symparams.damping = symparams.dampingFreefall = GetCurValueMod(params.fAirResistance, m_BaseMods.AirResistance);

		// Note: Customized gravity currently doesn't work for rigid body.
		symparams.gravity = symparams.gravityFreefall = vGravity;
		//symparams.softness = symparams.softnessGroup = 0.003f;
		//symparams.softnessAngular = symparams.softnessAngularGroup = 0.01f;
		symparams.maxLoggedCollisions = params.nMaxCollisionEvents;
		m_pPhysEnt->SetParams(&symparams);

		pe_action_set_velocity velparam;
		velparam.v = m_vVel;
		velparam.w = m_vRotVel;
		m_pPhysEnt->Action(&velparam);
	}
	else
	{
		// Make Physical Particle.
		m_pPhysEnt = GetPhysicalWorld()->CreatePhysicalEntity(PE_PARTICLE,&par_pos);
		pe_params_particle part;

		// Compute particle mass from volume of object.
		part.size = GetSize();
		// part.size = GetMaxValueMod(params.fSize, m_BaseMods.Size) * GetScale();

		part.mass = params.fDensity * part.size * part.size * part.size;
		if (m_pStatObj)
		{
			part.size *= m_pStatObj->GetRadius() + 0.05f;
			phys_geometry* pgeom = m_pStatObj->GetPhysGeom();
			if (pgeom)
				part.mass *= pgeom->V;
			else
				part.mass *= m_pStatObj->GetAABB().GetVolume() * fSPHERE_VOLUME / 8.f;
		}
		else
		{
			// Assume spherical volume.
			part.mass *= fSPHERE_VOLUME;
		}

		part.thickness = params.fThickness * part.size;
		part.velocity = m_vVel.GetLength();
		if (part.velocity > 0.f)
			part.heading = m_vVel / part.velocity;
		part.wspin = m_vRotVel;
		
		if (m_pStatObj)
		{
			Vec3 vSize = m_pStatObj->GetBoxMax() - m_pStatObj->GetBoxMin();
			char cMinAxis = GetMinAxis(vSize);
			part.normal = Vec3((cMinAxis=='x')?1.f:0,(cMinAxis=='y')?1.f:0,(cMinAxis=='z')?1.f:0);
		}

		part.surface_idx = GetSurfaceIndex();
		part.flags = /*particle_no_roll|*/particle_no_path_alignment;
		part.kAirResistance = GetCurValueMod(params.fAirResistance, m_BaseMods.AirResistance);
		part.gravity = vGravity;

		m_pPhysEnt->SetParams(&part);
	}

	// Common settings.
	if (m_pPhysEnt)
	{
		pe_params_flags pf;
		pf.flagsOR = pef_never_affect_triggers;
		if (params.nMaxCollisionEvents > 0)
			pf.flagsOR |= pef_log_collisions;
		m_pPhysEnt->SetParams(&pf);
	}
}

void CParticle::GetPhysicsState()
{
	if (m_pPhysEnt)
	{
		pe_status_pos status_pos;
		if (m_pPhysEnt->GetStatus(&status_pos))
		{
			m_vPos = status_pos.pos;
			m_qRot = status_pos.q;
		}
		pe_status_dynamics status_dyn;
		if (m_pPhysEnt->GetStatus(&status_dyn))
		{
			m_vVel = status_dyn.v;
		}
	}
}

#ifdef _DEBUG

// Test for distribution evenness.
struct CChaosTest
{
	CChaosTest()
	{
		CChaosKey keyBase(0U);
		float f[100];
		for (uint i = 0; i < 100; i++)
		{
			f[i] = keyBase.Jumble(CChaosKey(i)) * 1.f;
		}
	}
};

static CChaosTest ChaosTest;

#endif
