////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   partpolygon.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: sprite particles, big independent polygons
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "Particle.h"
#include "partman.h"
#include "ObjMan.h"
#include "3dEngine.h"
#include "CREParticle.h"

static const float fBYTE_TO_FLOAT	= 1.f/255.f;
static const float fMIN_RENDER_ALPHA = fBYTE_TO_FLOAT * 0.5f;

struct SParticleRenderData
{
	// Computed params needed for rendering.
	ColorF	cColor;						// Color & alpha.
	float		fSize;						// Particle average radius.
	float		fDistSq;					// Distance^2 from camera.
	float		fScrFill;					// Proportion of screen filled.
};

struct CParticle::SVertexContext: SParticleRenderContext
{
	UCol			m_ReservedVert00,
						m_ReservedVertNN,
						m_ReservedVertNP,
						m_ReservedVertPN,
						m_ReservedVertPP,
						m_ReservedVertGS;

	float			m_fEmitterScale;
	float			m_fViewDistRatio;
	float			m_fScrFillFactor;
	float			m_fScrFillFade;
	float			m_fDistFuncCoefs[4];			// Coefficients for alpha(dist^2) function.

	SVisEnviron const* m_pVisEnv;				// Vis env to clip against, if needed.

	SParticleCounts&	m_Counts;

	SVertexContext( SParticleRenderContext const& Context, CParticleContainer* pContainer )
		: SParticleRenderContext(Context), m_Counts(pContainer->GetCounts())
	{
		ResourceParticleParams const& params = pContainer->GetParams();

		m_fEmitterScale = pContainer->GetLoc().GetParticleScale();
		m_fViewDistRatio = pContainer->GetMain().GetViewDistRatio();
		float fScreenAngleRatio = m_Camera.GetProjRatio() / square(m_Camera.GetFov());		// 1 / HFov / VFov
		m_fScrFillFactor = 4.f * params.fTexAspectX * params.fTexAspectY * fScreenAngleRatio;
		m_fScrFillFade = 2.f / CParticleContainer::m_pPartManager->GetMaxContainerScrFill();

		m_fDistFuncCoefs[0] = 1.f;
		m_fDistFuncCoefs[1] = m_fDistFuncCoefs[2] = m_fDistFuncCoefs[3] = 0.f;

		if (params.bUseCameraBorders)
		{
			float fZoom = 1.0f / m_Camera.GetFov();
			m_fDistFuncCoefs[0] = params.fCameraMinDistance * fZoom;
			m_fDistFuncCoefs[1] = 1.0f / max(params.fCameraMinBorder * fZoom, 0.1);
			m_fDistFuncCoefs[2] = params.fCameraMaxDistance * fZoom;
			m_fDistFuncCoefs[3] = 1.0f / max(params.fCameraMaxBorder * fZoom, 0.1);
		}
		else
		{
			if (params.fCameraMaxDistance > 0.f)
			{
				float fZoom = 1.f / m_Camera.GetFov();
				float fNear = sqr(params.fCameraMinDistance * fZoom);
				float fFar = sqr(params.fCameraMaxDistance * fZoom);
				float fBorder = (fFar-fNear) * 0.1f;
				if (fNear == 0.f)
					// No border on near side.
					fNear = -fBorder;

				/*	f(x) = (1 - ((x-C)/R)²) / s		; C = (N+F)/2, R = (F-N)/2
							= (-NF + (N+F) x - x²) / s
						f(N+B) = 1 
						s = B(F-N-B)
				*/
				float fS = 1.f / (fBorder*(fFar - fNear - fBorder));
				m_fDistFuncCoefs[0] = -fNear*fFar*fS;
				m_fDistFuncCoefs[1] = (fNear+fFar)*fS;
				m_fDistFuncCoefs[2] = -fS;
			}
			else if (params.fCameraMinDistance > 0.f)
			{
				float fZoom = 1.f / m_Camera.GetFov();
				float fBorder = params.fCameraMinDistance  * fZoom * 0.25f;
				m_fDistFuncCoefs[1] = 1.f / fBorder;
				m_fDistFuncCoefs[0] = -4.f;
			}
		}

		// Pre-compute vertex encoding for backlighting and corner displacements.
		m_ReservedVert00 = colReservedVert(params.fDiffuseBacklighting, 0.f, 0.f);
		m_ReservedVertNN = colReservedVert(params.fDiffuseBacklighting, -1.f, -1.f);
		m_ReservedVertNP = colReservedVert(params.fDiffuseBacklighting, -1.f, 1.f);
		m_ReservedVertPN = colReservedVert(params.fDiffuseBacklighting, 1.f, -1.f);
		m_ReservedVertPP = colReservedVert(params.fDiffuseBacklighting, 1.f, 1.f);
		m_ReservedVertGS = colReservedVertGS(params.fDiffuseBacklighting, params.fTexFracX, params.fTexFracY);

		m_pVisEnv = &pContainer->GetMain().GetVisEnviron();
		if ((pContainer->GetCVars()->e_particles_debug & AlphaBit('c')) || !m_pVisEnv->NeedVisAreaClip())
			m_pVisEnv = 0;
	}

	inline UCol colColor( ColorF const& color )
	{
		// 8-bit color.
		UCol col;
		if (m_bSwapRGB)
		{
			col.bcolor[0] = UnitUFloatToUInt8(color.b);
			col.bcolor[1] = UnitUFloatToUInt8(color.g);
			col.bcolor[2] = UnitUFloatToUInt8(color.r);
		}
		else
		{
			col.bcolor[0] = UnitUFloatToUInt8(color.r);
			col.bcolor[1] = UnitUFloatToUInt8(color.g);
			col.bcolor[2] = UnitUFloatToUInt8(color.b);
		}
		col.bcolor[3] = UnitUFloatToUInt8(color.a);
		return col;
	}

	static UCol	colReservedVert( float fBackLighting, float fX, float fY )
	{
		UCol col;
		col.bcolor[0] = UnitUFloatToUInt8(fBackLighting);
		col.bcolor[1] = UnitSFloatToUInt8(fY);
		col.bcolor[2] = UnitSFloatToUInt8(fX);
		col.bcolor[3] = 0;
#if defined(XENON) || defined(PS3)
    SwapEndian(col.dcolor);
#endif
		return col;
	}

	UCol	colReservedVertGS( float fBackLighting, float texRectWith, float texRectHeight )
	{
		UCol col;
		col.bcolor[0] = UnitUFloatToUInt8(texRectWith);
		col.bcolor[1] = UnitUFloatToUInt8(texRectHeight);
		col.bcolor[2] = UnitUFloatToUInt8(fBackLighting);
		col.bcolor[3] = 0;
		return col;
	}

	inline float DistFunc( float fDist ) const
	{
		if (m_fDistFuncCoefs[3] > 0)
			return clamp_tpl( (fDist - m_fDistFuncCoefs[0]) * m_fDistFuncCoefs[1], 0.0f, 1.0f ) * clamp_tpl( (m_fDistFuncCoefs[2] - fDist) * m_fDistFuncCoefs[3], 0.0f, 1.0f );
		else
			return clamp_tpl( m_fDistFuncCoefs[0] + m_fDistFuncCoefs[1]*fDist + m_fDistFuncCoefs[2]*fDist*fDist, 0.f, 1.f	);
	}
};


//////////////////////////////////////////////////////////////////////////
bool CParticle::RenderGeometry( SRendParams& RenParamsShared, SVertexContext& Context )
{
	// Render 3d object
	assert(m_pStatObj);
	if (!IsAlive() || !m_pStatObj)
		return false;

	SParticleRenderData RenderData;
	ComputeRenderData( RenderData, Context );
	RenderData.cColor.a *= RenParamsShared.fAlpha;
	if (RenderData.cColor.a < fMIN_RENDER_ALPHA)
		return false;

	const ResourceParticleParams& params = GetParams();

	// Get matrices.
	Matrix34 matPart( Vec3(RenderData.fSize), m_qRot, m_vPos );

#ifdef PARTICLE_MOTION_BLUR
  Matrix34 matPartPrev( Vec3(RenderData.fSize), m_qRotPrev, m_vPosPrev );
  
  m_qRotPrev = m_qRot;
  m_vPosPrev = m_vPos;
#endif

	if (!params.bNoOffset && params.ePhysicsType != ParticlePhysics_RigidBody)
	{
		// Recenter object pre-rotation.
		Vec3 vCenter = m_pStatObj->GetAABB().GetCenter();
    Matrix34 pTranslationMat = Matrix34::CreateTranslationMat(-vCenter);
		matPart = matPart * pTranslationMat;
#ifdef PARTICLE_MOTION_BLUR
    matPartPrev = matPartPrev * pTranslationMat;
#endif
	}

#ifdef SHARED_GEOM
	if (m_pStatObj == params.pStatObj)
	{
		// Add shared geometry instance.
		SInstanceInfo& Inst = *RenParamsShared.pInstInfo->arrMats.push_back();
		Inst.instMat				= matPart;
		Inst.instMatPrev		= matPartPrev;
		Inst.instColor			= RenderData.cColor;
	}
	else
#endif
	{
		// Render separate draw call.
		SRendParams RenParams		= RenParamsShared;
		RenParams.fAlpha				= RenderData.cColor.a;
		RenParams.pMatrix				= &matPart;            
	#ifdef PARTICLE_MOTION_BLUR
		RenParams.pPrevMatrix		= &matPartPrev;
	#endif
		m_pStatObj->Render(RenParams);
	}

	return true;
}

char CParticleContainer::GetRenderType() const
{
	if (m_pParams->fSize.GetMaxValue() > 0.f && m_pParams->fAlpha.GetMaxValue() > 0.f)
	{
		if (m_pParams->pStatObj != 0)
			return 'G';					// Geometry rendering.
		if ((m_pParams->nTexId > 0 && m_pParams->nTexId < MAX_VALID_TEXTURE_ID) || m_pParams->pMaterial != 0)
			return 'S';					// Sprite rendering.
		if (!GetEffect())
			return 'G';					// Potential per-particle geometry rendering.
	}
	return 0;					// Nothing to render.
}

void CParticleContainer::RenderGeometry( const SRendParams& RenParams, const SPartRenderParams& PRParams )
{
	AUTO_LOCK_PROFILE(m_Lock, this);
  FUNCTION_PROFILER_CONTAINER(this);

	if (!NeedsDynamicBounds())
		QueueUpdate(true);

	const ResourceParticleParams& params = GetParams();
	SRendParams RenParamsGeom = RenParams;
	RenParamsGeom.nMotionBlurAmount = clamp_tpl(int_round(params.fMotionBlurScale * 128), 0, 255);
  RenParamsGeom.dwFObjFlags |= FOB_TRANS_MASK;
	RenParamsGeom.pMaterial = params.pMaterial;
	if (params.bDrawNear)
	{
		RenParamsGeom.dwFObjFlags |= FOB_NEAREST;
		//RenParamsGeom.nRenderList = EFSLIST_POSTPROCESS;
	}
	RenParamsGeom.fCustomSortOffset = (float)clamp_tpl(params.nDrawLast, -100, 100);

#ifdef SHARED_GEOM
	RenParamsGeom.pInstInfo = &m_InstInfo;
	m_InstInfo = SInstancingInfo();
#endif

	// Set up shared and unique geom rendering.
	SParticleRenderContext RContext( PRParams.m_Camera, !(GetRenderer()->GetFeatures() & RFT_RGBA), false );
	CParticle::SVertexContext Context( RContext, this );

	m_Counts.EmittersRendered = 1.f;
	for_array (i, m_Particles)
		m_Counts.ParticlesRendered += m_Particles[i].RenderGeometry( RenParamsGeom, Context );

#ifdef SHARED_GEOM
	// Render shared geom.
	if (m_InstInfo.arrMats.size())
		params.pStatObj->Render(RenParamsGeom);
#endif
}

void CParticle::AddFillLight( CCamera const& cam )
{
	ParticleParams const& params = GetParams();

	SFillLight fl;
	fl.m_vOrigin = m_vPos;
	fl.m_fRadius = GetCurValueMod(params.fLightSourceRadius, m_BaseMods.LightSourceRadius);
	fl.m_fIntensity = GetCurValueMod(params.fLightSourceIntensity, m_BaseMods.LightSourceIntensity);

	if (GetRenderer()->EF_Query(EFQ_HDRModeEnabled))
		fl.m_fIntensity *= 
			powf( Get3DEngine()->GetHDRDynamicMultiplierInline(), GetCurValueMod(params.fLightHDRDynamic, m_BaseMods.LightHDRDynamic) );

	fl.bNegative = false;

	if (fl.m_fIntensity * fl.m_fRadius > 0.f
	&& m_vPos.GetSquaredDistance(cam.GetPosition()) < sqr(fl.m_fRadius*GetCVars()->e_view_dist_ratio_detail)
	&& cam.IsSphereVisible_F(Sphere(fl.m_vOrigin,fl.m_fRadius)))
		GetRenderer()->EF_ADDFillLight(fl);
}

//////////////////////////////////////////////////////////////////////////
inline Vec3 GetScreenDir( const Vec3& vDir, const Vec3& vPos, const Matrix34& matCamera )
{
	Vec3 vForward = (vPos - matCamera.GetTranslation()).normalized();
	Vec3 vRight = (vForward ^ matCamera.GetColumn(2)).normalized();
	Vec3 vUp = vRight ^ vForward;

	return Vec3( vDir*vRight, vDir*vUp, vDir*vForward );
}

int CParticleContainer::GetMaxVertexCount() const
{
	ResourceParticleParams const& params = GetParams();
	if (GetHistorySteps() > 0)
		return (GetHistorySteps()+3) * 2;
	return 4;
}

inline void SetQuadIndices( IAllocRender::SVertices& alloc, int nVertAdvance = 4 )
{
	uint16* pIndices = alloc.pIndices + alloc.nIndices;
	pIndices[0] = 0 + alloc.nBaseVertexIndex;
	pIndices[1] = 1 + alloc.nBaseVertexIndex;
	pIndices[2] = 2 + alloc.nBaseVertexIndex;

	pIndices[3] = 3 + alloc.nBaseVertexIndex;
	pIndices[4] = 2 + alloc.nBaseVertexIndex;
	pIndices[5] = 1 + alloc.nBaseVertexIndex;

	alloc.nIndices += 6;
	alloc.nBaseVertexIndex += nVertAdvance;
}

inline void SetQuadsIndices( IAllocRender::SVertices& alloc )
{
	int nQuads = alloc.nVertices >> 2;
	alloc.nVertices &= ~3;
	while (nQuads-- > 0)
		SetQuadIndices( alloc );
}

inline void SetPolyIndices( IAllocRender::SVertices& alloc, int nVerts, SVertexParticle* aVerts, bool bFinal )
{
	nVerts &= ~1;
	nVerts -= 2;
	for (int v = 0; v < nVerts; v += 2)
	{
		SetQuadIndices( alloc, 2 );
		if (aVerts[v+3].GetPolygonBreak())
		{
			// Internal polygon break.
			alloc.nBaseVertexIndex += 2;
			v += 2;
			if (v == nVerts)
				return;
		}
	}

	// Final quad.
	if (bFinal)
		alloc.nBaseVertexIndex += 2;
}

void CParticleContainer::ComputeVertices( )
{
	AUTO_LOCK_PROFILE(m_Lock, this);
	if (m_bVertsComputed)
		return;

  FUNCTION_PROFILER_CONTAINER(this);

	assert(GetRenderType() == 'S');
	ResourceParticleParams const& params = *m_pParams;
	if (params.fTexAspectX + params.fTexAspectY == 0.f)
		m_pParams->UpdateTextureAspect();

	SParticleRenderContext Context( m_pPartManager->GetRenderCamera(), !(GetRenderer()->GetFeatures() & RFT_RGBA), m_bRenderGeomShader );
	CParticle::SVertexContext vcontext( Context, this );

	int nMaxParticleVerts = !Context.m_bGeomShader ? GetMaxVertexCount() : 1;
	m_aRenderVerts.resize(m_Particles.size() * nMaxParticleVerts);

	int nUsedVerts = 0;
	for (TParticleList::iterator pPart(m_Particles, !params.bSortOldestFirst); pPart; ++pPart)
	{
		assert(nUsedVerts + nMaxParticleVerts <= m_aRenderVerts.size());
		int nVerts = pPart->SetVertices( m_aRenderVerts.begin()+nUsedVerts, vcontext );
		nUsedVerts += nVerts;
		if (nMaxParticleVerts > 4 && nVerts)
			// Mark each polygon break.
			m_aRenderVerts[nUsedVerts-1].SetPolygonBreak(true);
	}
	m_aRenderVerts.resize(nUsedVerts);
	m_bVertsComputed = true;
}

bool CParticleContainer::SetVertices( IAllocRender& AllocRender, SParticleRenderContext const& Context )
{
	// Update, if not yet done.
	assert(!m_pParams->pStatObj);
	UpdateParticles();
	if (m_Particles.empty())
		return true;

	AUTO_LOCK_PROFILE(m_Lock, this);
  FUNCTION_PROFILER_CONTAINER(this);

	QueueUpdate(true);

	ResourceParticleParams const& params = *m_pParams;
	if (params.fTexAspectX + params.fTexAspectY == 0.f)
		m_pParams->UpdateTextureAspect();

  // Add simple fill lights to the renderer if needed.
	if (params.fLightSourceIntensity.GetMaxValue() * params.fLightSourceRadius.GetMaxValue() > 0.f
		&& GetCVars()->e_particles_lights 
		&& !m_nRenderStackLevel 
		&& !GetLoc().m_SpawnParams.bIgnoreLocation)
  {
		// Must be done in a separate pass here, as cannot be done in thread.
		for (TParticleList::iterator pPart(m_Particles, !params.bSortOldestFirst); pPart; ++pPart)
			pPart->AddFillLight(Context.m_Camera);
  }

	int nMaxParticleVerts = !Context.m_bGeomShader ? GetMaxVertexCount() : 1;
	int nMinParticleVerts = !Context.m_bGeomShader ? 4 : 1;
	int nMaxParticleInds = !Context.m_bGeomShader ? (nMaxParticleVerts-2)*3 : 0;

	IAllocRender::SVertices alloc;

	m_bRendered = true;
	if (m_bVertsComputed)
	{
		// Copy pre-created vertices.
		assert(Context.m_bGeomShader == m_bRenderGeomShader);

		int nParticle = 0, nVertex = 0;
		while (nVertex < m_aRenderVerts.size())
		{
			// Alloc as many verts and inds as possible.
			int nVerts = m_aRenderVerts.size() - nVertex;

			if (nMaxParticleVerts == 1)
			{
				assert(m_bRenderGeomShader);
				AllocRender(alloc, nVerts, 0);
			}
			else if (nMaxParticleVerts == 4)
			{
				// Simple quad particles.
				AllocRender(alloc, nVerts, nVerts * 3/2);
				alloc.nVertices = min(alloc.nVertices, alloc.nIndices/3*2);
				alloc.nIndices = 0;
				SetQuadsIndices(alloc);
			}
			else
			{
				// Variable vertex count.
				AllocRender(alloc, nVerts, nVerts * 3);
				alloc.nVertices = min(alloc.nVertices, alloc.nIndices/3);
				alloc.nIndices = 0;
				SetPolyIndices(alloc, alloc.nVertices, m_aRenderVerts.begin()+nVertex, alloc.nVertices == nVerts);
			}

			// Copy vertex data.
			memcpy( alloc.pVertices, &m_aRenderVerts[nVertex], alloc.nVertices * sizeof(m_aRenderVerts[0]) );
			nVertex += alloc.nVertices;
		}
		AllocRender(alloc, 0, 0);
	}
	else
	{
		CParticle::SVertexContext vcontext( Context, this );

		int nAvailVertices = -1, nAvailIndices = 0;

		for (TParticleList::iterator pPart(m_Particles, !params.bSortOldestFirst); pPart; ++pPart)
		{
			if (alloc.nVertices > nAvailVertices || alloc.nIndices > nAvailIndices)
			{
				if (!Context.m_bGeomShader && alloc.nIndices == 0)
					SetQuadsIndices(alloc);
				assert(alloc.nVertices <= nAvailVertices+nMaxParticleVerts);
				assert(alloc.nIndices <= nAvailIndices+nMaxParticleInds);

				int nParticles = pPart.count_remaining();
				AllocRender(alloc, nParticles * nMaxParticleVerts, nParticles * nMaxParticleInds);
				nAvailVertices = alloc.nVertices - nMaxParticleVerts;
				nAvailIndices = alloc.nIndices - nMaxParticleInds;
				if (nAvailVertices < 0 || nAvailIndices < 0)
					return false;
				alloc.nVertices = alloc.nIndices = 0;
			}

			int nVerts = pPart->SetVertices( alloc.pVertices+alloc.nVertices, vcontext );
			assert(nVerts <= nMaxParticleVerts);
			if (nVerts < nMinParticleVerts)
				continue;

			// Create indices automatically
			if (nVerts > 4)
			{
				assert(!Context.m_bGeomShader);
				SetPolyIndices(alloc, nVerts, alloc.pVertices+alloc.nVertices, true);
			}

			alloc.nVertices += nVerts;
		}

		if (!Context.m_bGeomShader && alloc.nIndices == 0)
			SetQuadsIndices(alloc);
		AllocRender(alloc, 0, 0);
	}

	m_pPartManager->TrackScrFill(m_Counts.ScrFillProcessed);

	m_Counts.EmittersRendered += 1.f;
	if (m_Counts.EmittersRendered > 1.f)
	{
		float fMultiply = m_Counts.EmittersRendered / (m_Counts.EmittersRendered-1.f);
		m_Counts.ParticlesRendered *= fMultiply;
		m_Counts.ScrFillRendered *= fMultiply;
		m_Counts.ScrFillProcessed *= fMultiply;
	}

	return true;
}

void CParticle::ComputeRenderData( SParticleRenderData& RenderData, SVertexContext& Context ) const
{
	ResourceParticleParams const& params = GetParams();

	// Color and alpha.
	RenderData.cColor = GetCurValueBase(params.cColor, m_BaseColor);
	RenderData.cColor.a = GetCurValueBase(params.fAlpha, m_BaseColor.a);

	RenderData.fDistSq = m_vPos.GetSquaredDistance(Context.m_Camera.GetPosition());

	RenderData.fSize = GetCurValueMod(params.fSize, m_BaseMods.Size) * Context.m_fEmitterScale;
	float fObjectSize = m_pStatObj ? m_pStatObj->GetRadius() : 1.f;

	if (params.fMinPixels > 0.f)
	{
		float fMinSize = params.fMinPixels * 0.5f * sqrt(RenderData.fDistSq) / Context.m_Camera.GetAngularResolution();
		float fMaxSize = params.fSize.GetBase() * Context.m_fEmitterScale * fObjectSize;
		if (fMaxSize < fMinSize)
			RenderData.fSize *= fMinSize / fMaxSize;
	}
	float fVisSize = RenderData.fSize * GetBaseRadius();

	// Shrink particle when approaching visible limits.
	float fClipRadius = m_fClipDistance;
	if (Context.m_pVisEnv && fClipRadius > 0.f)
	{
		// Particles clipped against visibility areas. Shrink only.
		fClipRadius = min(fClipRadius, fVisSize);
		Sphere sphere(m_vPos, fClipRadius);
		Vec3 vNormal;
		if (params.bSoftParticle)
			vNormal.zero();
		else if (params.eFacing == ParticleFacing_Camera)
			vNormal = Context.m_Camera.GetViewdir();
		else
			vNormal = Vec3(0,1,0) * m_qRot;
		if (Context.m_pVisEnv->ClipVisAreas(sphere, vNormal))
		{
			fClipRadius = sphere.radius;
			GetContainer().GetCounts().ParticlesClip += 1.f;
		}
	}

	if (fClipRadius < fVisSize)
	{
		fClipRadius = max(fClipRadius, 0.f);
		RenderData.fSize *= fClipRadius / fVisSize;
		fVisSize = fClipRadius;
	}

	// Area of particle in square radians.
	RenderData.fScrFill = div_min(square(RenderData.fSize * fObjectSize) * Context.m_fScrFillFactor, RenderData.fDistSq, 1.f);

	// Adjust alpha by distance fading.
	RenderData.cColor.a *= clamp_tpl(RenderData.fScrFill * square(Context.m_fViewDistRatio) - 1.f, 0.f, 1.f);

	// Fade near distance limits..
	RenderData.cColor.a *= Context.DistFunc(RenderData.fDistSq);
}

//////////////////////////////////////////////////////////////////////////
int CParticle::SetVertices( SVertexParticle aVerts[], SVertexContext& Context ) const
{
	if (!IsAlive())
		return 0;

	SVertexParticle& BaseVert = aVerts[0];

	const ResourceParticleParams& params = GetParams();

	SParticleRenderData RenderData;
	ComputeRenderData( RenderData, Context );

	// Track scr fill.
	RenderData.fScrFill *= params.fFillRateCost;
	Context.m_Counts.ScrFillProcessed += RenderData.fScrFill;

	// Cull oldest particles to enforce max screen fill.
	float fAdjust = clamp_tpl(2.f - Context.m_Counts.ScrFillProcessed * Context.m_fScrFillFade, 0.f, 1.f);
	RenderData.cColor.a *= fAdjust;

	if (RenderData.cColor.a < fMIN_RENDER_ALPHA)
		return 0;

	// Stats.
	Context.m_Counts.ParticlesRendered++;
	Context.m_Counts.ScrFillRendered += RenderData.fScrFill;

	// For non-alpha blend modes, Alpha is just a redundant color reducer.
	if (params.eBlendType == ParticleBlendType_Additive || params.eBlendType == ParticleBlendType_ColorBased)
	{
		RenderData.cColor.r *= RenderData.cColor.a;
		RenderData.cColor.g *= RenderData.cColor.a;
		RenderData.cColor.b *= RenderData.cColor.a;
	}

	// 8-bit color.
	BaseVert.color = Context.colColor(RenderData.cColor);
#ifdef XENON
  SwapEndian(BaseVert.color.dcolor);
#endif

	// Texture coords.
	BaseVert.st[0] = 0.f;
	BaseVert.st[1] = 0.f;

	int nTex = m_nTileVariant;
	if (params.TextureTiling.nAnimFramesCount > 1)
	{
		// Select tile based on particle age.
		nTex *= params.TextureTiling.nAnimFramesCount;
		if (params.TextureTiling.fAnimFramerate > 0.f)
		{
			if (params.TextureTiling.bAnimCycle)
				nTex += int(m_fAge * params.TextureTiling.fAnimFramerate) % params.TextureTiling.nAnimFramesCount;
			else
        nTex += __min( uint(m_fAge * params.TextureTiling.fAnimFramerate), params.TextureTiling.nAnimFramesCount-1 );
		}
		else
			nTex += int(m_fRelativeAge * params.TextureTiling.nAnimFramesCount * 0.999f);
	}

	nTex += params.TextureTiling.nFirstTile;

	if (nTex > 0)
	{
		if (params.TextureTiling.nTilesX > 1)
		{
			BaseVert.st[0] = float(nTex % params.TextureTiling.nTilesX) * params.fTexFracX;
			BaseVert.st[1] = float(nTex / params.TextureTiling.nTilesX) * params.fTexFracY;
		}
		else
		{
			BaseVert.st[1] = nTex * params.fTexFracY;
		}
	}

	BaseVert.xyz = m_vPos;

	// 3D oriented particles (Y facing).
	Matrix33 matRot;
	if (params.eFacing != ParticleFacing_Camera)
		matRot = Matrix33(m_qRot);

	// Stretch.
	float fStretch = GetCurValueMod(params.fStretch, m_BaseMods.Stretch);
	if (GetTarget().bStretch && params.bContinuous)
	{
		// Extend stretch to compensate for speed.
		float fEmitRate = m_pEmitter->GetEmitRate();
		if (fEmitRate > 0.f)
			fStretch += 1.f / fEmitRate;
	}

	// Compute base screen orientation and size.
	sincos_tpl(m_fAngle, &BaseVert.xaxis.y, &BaseVert.xaxis.x);
	BaseVert.yaxis.x = -BaseVert.xaxis.y;
	BaseVert.yaxis.y = BaseVert.xaxis.x;

	BaseVert.xaxis *= RenderData.fSize * params.fTexAspectX;
	BaseVert.yaxis *= RenderData.fSize * params.fTexAspectY;

	if (params.bOrientToVelocity || params.bEncodeVelocity || fStretch != 0.f)
	{
		// Project velocity dir into screen space.
		Vec3 vVelPlane;
		if (params.eFacing == ParticleFacing_Camera)
			vVelPlane = GetScreenDir( m_vVel, m_vPos, Context.m_Camera.GetMatrix() );
		else if (params.eFacing == ParticleFacing_Velocity)
			vVelPlane.zero();
		else
			vVelPlane(	matRot.m00*m_vVel.x + matRot.m10*m_vVel.y + matRot.m20*m_vVel.z,
									matRot.m02*m_vVel.x + matRot.m12*m_vVel.y + matRot.m22*m_vVel.z,
									matRot.m01*m_vVel.x + matRot.m11*m_vVel.y + matRot.m21*m_vVel.z );
		Vec2 vVelPlane2(vVelPlane.x, vVelPlane.y);

		float xy = vVelPlane2.GetLength();
		if (xy != 0.f)
		{
			Vec2 vVelNorm = vVelPlane2 / xy;
			if (params.bOrientToVelocity || params.bEncodeVelocity)
			{
				// Rotate into velocity-screen direction.
				BaseVert.xaxis.set( vVelNorm.y * BaseVert.xaxis.x + vVelNorm.x * BaseVert.xaxis.y,
														-vVelNorm.x * BaseVert.xaxis.x + vVelNorm.y * BaseVert.xaxis.y );
				BaseVert.yaxis.set( vVelNorm.y * BaseVert.yaxis.x + vVelNorm.x * BaseVert.yaxis.y,
														-vVelNorm.x * BaseVert.yaxis.x + vVelNorm.y * BaseVert.yaxis.y );
			}
			if (fStretch != 0.f)
			{
				// Stretch along speed direction.
				vVelPlane2 *= fStretch;
				BaseVert.xaxis += vVelPlane2 * ((BaseVert.xaxis*vVelNorm) / (RenderData.fSize * params.fTexAspectX));
				BaseVert.yaxis += vVelPlane2 * ((BaseVert.yaxis*vVelNorm) / (RenderData.fSize * params.fTexAspectY));
				BaseVert.xyz += m_vVel * (fStretch * params.fStretchOffsetRatio);
			}
		}
		if (params.bEncodeVelocity)
		{
			BaseVert.xaxis.x = xy / RenderData.fSize;
			BaseVert.xaxis.y = vVelPlane.z;
		}
	}

	if (params.eFacing == ParticleFacing_Camera)
	{
		// Solid camera-aligned particle.
		if (GetContainer().GetHistorySteps() > 0)
		{
			BaseVert.reserved = Context.m_ReservedVertPP;
			return FillTailVertBuffer( aVerts, Context.m_Camera.GetMatrix(), RenderData.fSize );
		}
		else if (Context.m_bGeomShader)
		{
			BaseVert.reserved = Context.m_ReservedVertGS;
			return 1;
		}
		else
		{
			aVerts[3] = aVerts[2] = aVerts[1] = aVerts[0];

			aVerts[0].reserved = Context.m_ReservedVertPP;
			aVerts[0].st[0] += params.fTexFracX;

			aVerts[1].reserved = Context.m_ReservedVertNP;

			aVerts[2].reserved = Context.m_ReservedVertPN;
			aVerts[2].st[0] += params.fTexFracX;
			aVerts[2].st[1] += params.fTexFracY;

			aVerts[3].reserved = Context.m_ReservedVertNN;
			aVerts[3].st[1] += params.fTexFracY;

			return 4;
		}
	}
	else
	{
		// Planar 3D-oriented particle.
		Vec3 vRight(	BaseVert.xaxis.x * matRot.m00 + BaseVert.xaxis.y * matRot.m02,
									BaseVert.xaxis.x * matRot.m10 + BaseVert.xaxis.y * matRot.m12,
									BaseVert.xaxis.x * matRot.m20 + BaseVert.xaxis.y * matRot.m22 );
		Vec3 vUp(			BaseVert.yaxis.x * matRot.m00 + BaseVert.yaxis.y * matRot.m02,
									BaseVert.yaxis.x * matRot.m10 + BaseVert.yaxis.y * matRot.m12,
									BaseVert.yaxis.x * matRot.m20 + BaseVert.yaxis.y * matRot.m22 );

		BaseVert.reserved = Context.m_ReservedVert00;
		BaseVert.xaxis = BaseVert.yaxis = Vec2(0,0);

		aVerts[3] = aVerts[2] = aVerts[1] = aVerts[0];

		aVerts[0].xyz += vRight; aVerts[0].xyz += vUp;
		aVerts[0].st[0] += params.fTexFracX;

		aVerts[1].xyz -= vRight; aVerts[1].xyz += vUp;

		aVerts[2].xyz += vRight; aVerts[2].xyz -= vUp;
		aVerts[2].st[0] += params.fTexFracX;
		aVerts[2].st[1] += params.fTexFracY;

		aVerts[3].xyz -= vRight; aVerts[3].xyz -= vUp;
		aVerts[3].st[1] += params.fTexFracY;

		return 4;
	}
}

static inline void SetSegment(	SVertexParticle aVerts[2], SVertexParticle const& BaseVert, ResourceParticleParams const& params,
																Vec3 const& vDir, Matrix34 const& matCamera, float ty, float ny )
{
	aVerts->xyz = BaseVert.xyz;
	aVerts->st[0] = BaseVert.st[0] + params.fTexFracX;
	aVerts->st[1] = BaseVert.st[1] + params.fTexFracY * ty;
	aVerts->color = BaseVert.color;
	aVerts->reserved = BaseVert.reserved;
	aVerts->SetDisplacement(1.f, ny);

	aVerts->yaxis = Vec2(GetScreenDir( vDir, BaseVert.xyz, matCamera ));
	aVerts->yaxis.NormalizeSafe();
	aVerts->xaxis.set( aVerts->yaxis.y, -aVerts->yaxis.x );
	aVerts->xaxis *= BaseVert.xaxis.x;
	aVerts->yaxis *= BaseVert.xaxis.y;

	aVerts[1] = aVerts[0];
	aVerts++;

	aVerts->st[0] = BaseVert.st[0];
	aVerts->SetDisplacement(-1.f, ny);
}

inline Vec4 DirLen( Vec3 const& v )
{
	return Vec4( v.x, v.y, v.z, v.GetLengthSquared() );
}

inline Vec3 DirAvg( Vec4 const& v1, Vec4 const& v2 )
{
	return Vec3(v1.x, v1.y, v1.z) * v2.w + Vec3(v2.x, v2.y, v2.z) * v1.w;
}

int CParticle::FillTailVertBuffer( SVertexParticle aTailVerts[], const Matrix34& matCamera, float fSize ) const
{
	const ResourceParticleParams& params = GetParams();

	int nVertCount = 0;
	float fTailLength = min(GetCurValueMod(params.fTailLength, m_BaseMods.TailLength), m_fAge);

	SVertexParticle BaseVert = aTailVerts[0];
	BaseVert.xaxis.x = fSize * params.fTexAspectX;
	BaseVert.xaxis.y = fSize * params.fTexAspectY;

	// Current pos expanded to half a particle, in direction of travel.
	Vec3 vDir = m_vVel;
	if (vDir.IsZero())
	{
		// No vel, extrapolate from history.
		for (int nPos = GetContainer().GetHistorySteps()-1; nPos >= 0; nPos--)
		{
			if (m_aPosHistory[nPos].IsUsed())
			{
				vDir = BaseVert.xyz - m_aPosHistory[nPos].vPos;
				if (!vDir.IsZero())
					break;
			}
		}
		if (vDir.IsZero())
			return 0;
	}
	vDir.Normalize();
	vDir *= BaseVert.xaxis.y;
	Vec4 vDirCur = DirLen(vDir);

	SetSegment( aTailVerts+nVertCount, BaseVert, params, vDir, matCamera, 0.f, 1.f );
	nVertCount += 2;

	// Center point.
	int nPos = GetContainer().GetHistorySteps()-1;
	for (; nPos >= 0; nPos--)
	{
		if (m_aPosHistory[nPos].IsUsed())
		{
			// Average direction.
			Vec4 vDirNext = DirLen(BaseVert.xyz - m_aPosHistory[nPos].vPos);
			if (vDirNext.w != 0.f)
			{
				vDir = DirAvg(vDirCur, vDirNext);
				vDirCur = vDirNext;
				break;
			}
		}
	}
	SetSegment( aTailVerts+nVertCount, BaseVert, params, vDir, matCamera, 0.5f, 0.f );
	nVertCount += 2;

  if (fTailLength > 0.f && m_fAge > 0.f)
	{
		// Fill with past positions.
		float fInvTail = 0.9999f / fTailLength;
		for (; nPos >= 0; nPos--)
		{
			float fAgeDelta = m_fAge - m_aPosHistory[nPos].fAge;
			BaseVert.xyz = m_aPosHistory[nPos].vPos;
			if (nPos > 0)
			{
				Vec4 vDirNext = DirLen(m_aPosHistory[nPos].vPos - m_aPosHistory[nPos-1].vPos);
				if (vDirCur.w == 0.f)
				{
					vDirCur = vDirNext;
					continue;
				}
				if (vDirNext.w != 0.f)
				{
					vDir = DirAvg(vDirCur, vDirNext);
					vDirCur = vDirNext;
				}
			}
			else
				vDir = Vec3(vDirCur.x, vDirCur.y, vDirCur.z);
			if (fAgeDelta > fTailLength)
			{
				// Interpolate between last vertices.
				if (nPos+1 < GetContainer().GetHistorySteps() && m_aPosHistory[nPos+1].IsUsed())
				{
					float fAgePrev = m_fAge - m_aPosHistory[nPos+1].fAge;
					float fT = (fAgeDelta - fTailLength) / (fAgeDelta - fAgePrev);
					BaseVert.xyz += (m_aPosHistory[nPos+1].vPos-BaseVert.xyz) * fT;
				}
				else
				{
					float fT = (fAgeDelta - fTailLength) / fAgeDelta;
					BaseVert.xyz += (aTailVerts[0].xyz-BaseVert.xyz) * fT;
				}
				break;
			}

			SetSegment( aTailVerts+nVertCount, BaseVert, params, vDir, matCamera, 0.5f + 0.5f * fAgeDelta * fInvTail, -fAgeDelta * fInvTail );
			nVertCount += 2;
		}
	}

	// Half particle at tail end.
	SetSegment( aTailVerts+nVertCount, BaseVert, params, vDir, matCamera, 1.f, -1.f );
	nVertCount += 2;

	return nVertCount;
}
