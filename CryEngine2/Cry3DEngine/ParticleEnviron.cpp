////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ParticleEnviron.cpp
//  Created:     28/08/2007 by Scott Peter
//  Description: World environment interface
// -------------------------------------------------------------------------
//  History:		 Refactored from ParticleEmitter.cpp
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleEnviron.h"
#include "ParticleEmitter.h"
#include "VisAreas.h"

//////////////////////////////////////////////////////////////////////////
// SPhysEnviron implementation.
//////////////////////////////////////////////////////////////////////////

void SPhysEnviron::Clear()
{
	m_vUniformGravity.zero();
	m_vUniformWind.zero();
	m_plUniformWater = Plane( Vec3(0,0,1), -WATER_LEVEL_UNKNOWN );
	m_tUnderWater = Trinary_If_False;
	m_nNonUniformFlags = 0;
	if (m_paNonUniformAreas)
		m_paNonUniformAreas->resize(0);
}

void SPhysEnviron::GetPhysAreas( const CParticleEmitter* pEmitter, AABB const& box, uint nFlags, DynArray<SArea>* paNonUniformAreas )
{
  FUNCTION_PROFILER_SYS(PARTICLE);

	Clear();
	m_paNonUniformAreas = paNonUniformAreas;

	Vec3 vCtr = box.GetCenter();
	Vec3 vSize = box.IsReset() ? Vec3(-1.f) : box.GetSize() * 0.5f;

	// Atomic iteration.
	for (IPhysicalEntity* pArea = 0; pArea = GetPhysicalWorld()->GetNextArea(pArea); )
	{
		// Initial check for areas of interest.
		int nAreaFlags = 0;

		pe_params_area parea;
		pe_params_buoyancy pbuoy;
		Plane plWater = m_plUniformWater;
		float fWaterDist = FLT_MAX;

		if (!pArea->GetParams(&parea))
			continue;

		if (nFlags & ENV_GRAVITY)
		{
			if (!is_unused(parea.gravity))
				nAreaFlags |= ENV_GRAVITY;
		}
		if (nFlags & (ENV_WIND | ENV_WATER))
		{
			if (pArea->GetParams(&pbuoy))
			{
				if (pbuoy.iMedium == 1)
					nAreaFlags |= ENV_WIND;
				else if (pbuoy.iMedium == 0 && !is_unused(pbuoy.waterPlane.n))
				{
					// Quick check for intersecting water.
					plWater.SetPlane(pbuoy.waterPlane.n, pbuoy.waterPlane.origin);
					fWaterDist = plWater.DistFromPlane(Vec3(vCtr.x, vCtr.y, box.min.z));
					if (fWaterDist < 0.f)
						nAreaFlags |= ENV_WATER;
				}
			}
		}

		nAreaFlags &= nFlags;
		if (nAreaFlags == 0)
			continue;

		// Skip areas created by this emitter,
		// as well as outdoor areas when indoors
		if (pEmitter)
		{
			pe_params_foreign_data fd;
			if (pArea->GetParams(&fd))
			{
				if (fd.pForeignData == pEmitter)
					continue;
				if ((fd.iForeignFlags & PFF_OUTDOOR_AREA) && pEmitter->GetVisEnviron().OriginIndoors())
					continue;
			}
		}

		// Query area for intersection and forces, uniform only.
		pe_status_area sarea;
		sarea.ctr = vCtr;
		sarea.size = vSize;
		sarea.bUniformOnly = true;
		int nStatus = pArea->GetStatus(&sarea);
		if (!nStatus)
			continue;

		if ((nAreaFlags & ENV_WATER) && m_tUnderWater == Trinary_If_False)
			m_tUnderWater = Trinary_Both;

		if (nStatus < 0)
		{
			if (paNonUniformAreas)
			{
				SArea& a = *paNonUniformAreas->push_back();
				a.m_pArea = pArea;
				a.m_nFlags = nAreaFlags;
				a.m_pLock = sarea.pLockUpdate;
				a.m_plWater = plWater;

				if (nAreaFlags & (ENV_GRAVITY | ENV_WIND))
				{
					// Test whether we can cache force information for quick evaluation.
					a.m_bCacheForce = false;
					if (parea.size.GetVolume() > 0.f && parea.pGeom && parea.pGeom->GetType() == GEOM_BOX)
					{
						pe_status_pos spos;
						Matrix34 matArea;
						spos.pMtx3x4 = &matArea;
						if (pArea->GetStatus(&spos))
						{
							a.m_bRadial = !parea.bUniform;
							if (nAreaFlags & ENV_GRAVITY)
								a.m_vGravity = parea.bUniform == 2 ? spos.q * parea.gravity : parea.gravity;
							if (nAreaFlags & ENV_WIND)
								a.m_vWind = parea.bUniform == 2 ? spos.q * pbuoy.waterFlow : pbuoy.waterFlow;
							a.m_fInnerRadius = parea.falloff0;

							// Construct world-to-unit-sphere transform.
							a.m_vCenter = spos.pos;
							a.m_matToLocal = (matArea * Matrix34::CreateScale(parea.size)).GetInverted();
							assert(!a.m_vCenter.IsZero());
							a.m_bCacheForce = true;
						}
					}
				}
			}
			m_nNonUniformFlags |= nAreaFlags;
			continue;
		}

		// Get uniform params.
		if (nAreaFlags & ENV_GRAVITY)
			m_vUniformGravity = sarea.gravity;
		if (nAreaFlags & ENV_WIND)
			m_vUniformWind += sarea.pb.waterFlow;
		if (nAreaFlags & ENV_WATER)
		{
			// Store nearest uniform water plane.
			if (fWaterDist <= m_plUniformWater.DistFromPlane(Vec3(vCtr.x, vCtr.y, box.min.z)))
				m_plUniformWater = plWater;
			if (plWater.DistFromPlane(Vec3(vCtr.x, vCtr.y, box.max.z)) <= 0.f)
				m_tUnderWater = Trinary_If_True;
		}
	}
}

bool SPhysEnviron::PhysicsCollision( ray_hit& hit, Vec3 const& vStart, Vec3 const& vEnd, float fRadius, uint nEnvFlags ) const
{
	bool bHit = false;
	Vec3 vMove = vEnd-vStart;

	memset(&hit, 0, sizeof(hit));
	hit.dist = 1.f;

	// Collide terrain first.
	if ((nEnvFlags & ENV_TERRAIN) && GetTerrain())
	{
		CHeightMap::SRayTrace rt;
		if (GetTerrain()->RayTrace(vStart, vEnd, &rt))
		{
			bHit = true;
			hit.dist = rt.fInterp;
			hit.pt = rt.vHit;
			hit.n = rt.vNorm;
			hit.surface_idx = rt.pMaterial->GetSurfaceTypeId();
			hit.bTerrain = true;
		}
	}

	if (hit.dist > 0.f)
	{
		int ent_collide = 0;
		if (nEnvFlags & ENV_STATIC_ENT)
			ent_collide |= ent_static;
		if (nEnvFlags & ENV_DYNAMIC_ENT)
			ent_collide |= ent_rigid | ent_sleeping_rigid | ent_living | ent_independent;

		if (ent_collide)
		{
			// rwi_ flags copied from similar code in CParticleEntity.
			ray_hit hit_loc;
			if (GetPhysicalWorld()->RayWorldIntersection(
				vStart, vMove*hit.dist,
				ent_collide | ent_no_ondemand_activation,
				sf_max_pierceable|(geom_colltype_ray|geom_colltype13)<<rwi_colltype_bit | rwi_colltype_any | rwi_ignore_noncolliding,
				&hit_loc, 1) > 0
			)
			{
				bHit = true;
				hit = hit_loc;
				float fMoveSq = vMove.GetLengthSquared();
				if (fMoveSq > 0.f)
					hit.dist *= isqrt_tpl(fMoveSq);
				else
					hit.dist = 0.f;
			}
		}
	}

	if (bHit)
	{
		hit.pt += hit.n * fRadius;
		float fMoveN = vMove*hit.n;
		if (fMoveN <= 0.f)
			hit.dist -= div_min(fRadius, -fMoveN, 1.f);
		else
			hit.dist = 0.f;
		hit.dist = clamp(hit.dist, 0.f, 1.f);
	}

	return bHit;
}

void SPhysEnviron::SArea::GetForcePhys( Vec3 const& vPos, Vec3& vGravity, Vec3& vWind ) const
{
  FUNCTION_PROFILER_SYS(PARTICLE);

	pe_status_area sarea;
	sarea.ctr = vPos;
	if (m_pArea->GetStatus(&sarea))
	{
		if (!is_unused(sarea.gravity))
			vGravity = sarea.gravity;
		if (sarea.pb.iMedium == 1 && !is_unused(sarea.pb.waterFlow))
			vWind += sarea.pb.waterFlow;
	}
}

void SPhysEnviron::SArea::GetForce( Vec3 const& vPos, Vec3& vGravity, Vec3& vWind ) const
{
	if (!m_bCacheForce)
		GetForcePhys( vPos, vGravity, vWind );
	else
	{
	  FUNCTION_PROFILER_SYS(PARTICLE);

		#ifdef COMPARE_PHYS
			pe_status_area sarea;
			sarea.ctr = vPos;
			int iStatus = m_pArea->GetStatus(&sarea);
		#endif

		Vec3 vPosLoc = m_matToLocal * vPos;
		float fDistSq = vPosLoc.GetLengthSquared();
		if (fDistSq >= 1.f)
		{
			#ifdef COMPARE_PHYS
				assert(!iStatus || fDistSq < 1.01f);
			#endif
			return;
		}
		float fStrength = div_min(1.f - sqrt_tpl(fDistSq), 1.f - m_fInnerRadius, 1.f);

		Vec3 vAreaGrav, vAreaWind;
		if (m_bRadial)
		{
			Vec3 vForce = (vPos - m_vCenter).GetNormalized();
			vAreaGrav = vForce * (m_vGravity.z * fStrength);
			vAreaWind = vForce * (m_vWind.z * fStrength);
		}
		else
		{
			vAreaGrav = m_vGravity * fStrength;
			vAreaWind = m_vWind * fStrength;
		}
		#ifdef COMPARE_PHYS
			assert(iStatus);
			if (m_nFlags & ENV_GRAVITY)
				assert(vAreaGrav.IsEquivalent(sarea.gravity));
			if (m_nFlags & ENV_WIND)
				assert(vAreaWind.IsEquivalent(sarea.pb.waterFlow));
		#endif
		if (m_nFlags & ENV_GRAVITY)
			vGravity = vAreaGrav;
		if (m_nFlags & ENV_WIND)
			vWind += vAreaWind;
	}
}

float SPhysEnviron::DistanceAboveWater( Vec3 const& vPos, float fMaxDist, Plane& plWater ) const
{
	// Check for non-uniform areas.
	plWater = m_plUniformWater;
	float fDistMin = m_plUniformWater.DistFromPlane(vPos);
	if ((m_nNonUniformFlags & ENV_WATER) && m_paNonUniformAreas)
	{
	  FUNCTION_PROFILER_SYS(PARTICLE);

		for_array_ptr (const SArea, p, (*m_paNonUniformAreas))
		{
			if (p->m_nFlags & ENV_WATER)
			{
				float fDist = p->m_plWater.DistFromPlane(vPos);
				if (fDist < min(fDistMin,fMaxDist))
				{
					if (GetTerrain())
					{
						// Optimisation: Before doing expensive check on complex area,
						// test if either particle or water level is under terrain.
						// If so, ignore all (non-global) areas.
						float fTrrZ = GetTerrain()->GetZSafe((int)vPos.x, (int)vPos.y);
						if (fTrrZ >= vPos.z+fMaxDist)
							break;
						if (fTrrZ >= vPos.z-fDist)
							break;
					}
					pe_status_contains_point st;
					st.pt = vPos;
					if (p->m_pArea->GetStatus(&st))
					{
						fDistMin = fDist;
						plWater = p->m_plWater;
					}
				}
			}
		}
	}

	return fDistMin;
}

/*
	Emitters render either entirely inside or entirely outside VisAreas (due to rendering architecture).
		Emitter origin in vis area:
			Params.VisibleInside == If_False? Don't render
			else entire emitter in vis area: we're good
			else (intersecting): Test each particle for dist, shrink / fade-out near boundary
		Emitter origin outside vis areas:
			Params.VisibleInside == If_True ? Don't render
			else entirely outside vis areas: we're good
			else (intersecting): Test each particle for dist, shrink / fade-out near boundary
*/

void SVisEnviron::Update( Vec3 const& vOrigin, AABB const& bb )
{
	if (!m_bValid)
	{
		// Determine emitter's vis area, by origin.
		FUNCTION_PROFILER_SYS(PARTICLE);

		m_pVisArea = Get3DEngine()->GetVisAreaFromPos(vOrigin);
		m_pVisNodeCache = 0;

		// See whether it crosses boundaries.
		if (m_pVisArea)
			m_bCrossesVisArea = m_pVisArea->GetAABBox()->IsIntersectBox(bb);
		else
			m_bCrossesVisArea = Get3DEngine()->IntersectsVisAreas(bb, &m_pVisNodeCache);

		m_bValid = true;
	}

	m_pClipVisArea = 0;
	if (m_bCrossesVisArea)
	{
		// Clip only against cam area.
		IVisArea* pVisAreaCam = GetVisAreaManager()->GetCurVisArea();
		if (pVisAreaCam != m_pVisArea)
		{
			if (pVisAreaCam)
			{
				if (pVisAreaCam->GetAABBox()->IsIntersectBox(bb))
				{
					if (!m_pVisArea || !pVisAreaCam->FindVisArea(m_pVisArea, 3, true))
						m_pClipVisArea = pVisAreaCam;
				}
			}
			else if (m_pVisArea)
				m_pClipVisArea = m_pVisArea;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// GeomRef functions.

#include "ICryAnimation.h"
#include "GeomQuery.h"

void GetAABB( AABB& bb, QuatTS& tLoc, GeomRef const& geom )
{
	if (geom.m_pStatObj)
		bb = geom.m_pStatObj->GetAABB();
	else if (geom.m_pChar)
		bb = geom.m_pChar->GetAABB();
	else if (geom.m_pPhysEnt)
	{
		pe_status_pos pos;
		if (geom.m_pPhysEnt->GetStatus(&pos))
		{
			// Box is already oriented, but not offset.
			bb = AABB(pos.pos + pos.BBox[0], pos.pos + pos.BBox[1]);
			tLoc.SetIdentity();
		}
	}
	else
		bb = AABB(ZERO);
}

float GetExtent( GeomQuery& geo, GeomRef const& geom, EGeomType eType, EGeomForm eForm )
{
	if (eType == GeomType_BoundingBox)
	{
		AABB bb;
		QuatTS loc;
		GetAABB(bb, loc, geom);
		return BoxExtent(eForm, bb.GetSize() * (0.5f*loc.s));
	}
	else if (eType == GeomType_Render && geom.m_pStatObj)
		return geo.GetExtent(geom.m_pStatObj, eForm);
	else if (eType == GeomType_Render && geom.m_pChar)
		return geo.GetExtent(geom.m_pChar, eForm);
	else if (eType == GeomType_Physics && geom.m_pPhysEnt)
	{
		// status_extent query caches extent in geo.
		pe_status_extent se;
		se.pGeo = &geo;
		se.eForm = eForm;
		geom.m_pPhysEnt->GetStatus(&se);
		return geo.GetExtent();
	}
	return 0.f;
}

bool GetRandomPos( RandomPos& ran, GeomQuery& geo, GeomRef const& geom, EGeomType eType, EGeomForm eForm, QuatTS const& tWorld )
{
	if (eType == GeomType_BoundingBox)
	{
		AABB bb;
		QuatTS loc = tWorld;
		GetAABB(bb, loc, geom);
		BoxRandomPos(ran, eForm, bb.GetSize() * 0.5f);
		ran.vPos += bb.GetCenter();
		Transform(ran, loc);
	}
	else if (eType == GeomType_Render && geom.m_pStatObj)
	{
		geom.m_pStatObj->GetRandomPos(ran, geo, eForm);
		Transform(ran, tWorld);
	}
	else if (eType == GeomType_Render && geom.m_pChar)
	{
		geom.m_pChar->GetRandomPos(ran, geo, eForm);
		Transform(ran, tWorld);
	}
	else if (eType == GeomType_Physics && geom.m_pPhysEnt)
	{
		pe_status_random sr;
		sr.pGeo = &geo;
		sr.eForm = eForm;
		geom.m_pPhysEnt->GetStatus(&sr);
		ran = sr.ran;
	}
	else
	{
		ran.vPos = tWorld.t;
		ran.vNorm.zero();
		return false;
	}
	return true;
}
