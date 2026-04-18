//--------------------------------------------------------------------------
// Render element for GPU particle systems
//  Created:     [ATI / jisidoro]
//--------------------------------------------------------------------------
#include "StdAfx.h"

//note... to ifdef out a file.. the ifdef needs to be after the precompiled header include

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS

#include "CREParticleGPU.h"
#include "ISystem.h"
#include "IPhysicsGPU.h"
#include <I3DEngine.h>
//#include "../../XRenderD3D9/DriverD3D.h"

#define PREVENT_PREVIEW_RENDER

/*
struct SParticleMeshPool
{
public:
	typedef std::vector< struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F > ParticleVertexBuffer;
	typedef std::vector< uint8 > ParticleIndexBuffer;

	enum SParticleMeshPoolSize
	{
		e_NumDefaultParticles = 2048
	};

public:
	ParticleVertexBuffer m_vertexBuffer;
	ParticleIndexBuffer m_indexBuffer;

public:
	SParticleMeshPool()
		: m_vertexBuffer()
		, m_indexBuffer()
	{
		m_vertexBuffer.reserve( e_NumDefaultParticles * 4 );
		m_indexBuffer.reserve( e_NumDefaultParticles * 6 );
	}
};

static SParticleMeshPool g_particleMeshPool;
*/

//storage for GPU particles
static TArray< CREParticleGPU* > g_CREParticleGPUStorage;


CRendElement* 
CREParticleGPU::mfCopyConstruct()
{
	return new CREParticleGPU( *this );
}

float 
CREParticleGPU::mfDistanceToCameraSquared( Matrix34& matInst )
{
	static float fInnerDistRatio = 0.25f;			// Amount to scale distance inside emitter bounds.

	Vec3 vDist = gRenDev->GetRCamera().Orig - m_BoundingBox.GetCenter();
	float fDist2 = vDist.GetLengthSquared();

	if (fInnerDistRatio != 1.f)
	{
		if (m_BoundingBox.GetVolume())
		{
			// For smoother results, treat the bounding volume as an ellipsoid rather than a box.
			Vec3 vSize = m_BoundingBox.GetSize() * 0.5f;
			Vec3 vUni(vDist.x/vSize.x, vDist.y/vSize.y, vDist.z/vSize.z);
			float fUni = vUni.GetLength();
			if (fUni <= 1.f)
				fDist2 *= square(fInnerDistRatio);
			else
				fDist2 *= square( (fUni - 1.f + fInnerDistRatio) / fUni );
		}
	}

	return fDist2;
}


bool 
CREParticleGPU::mfCullBox( Vec3& vmin, Vec3& vmax )
{
	return false;
}


void 
CREParticleGPU::mfPrepare()
{
	PROFILE_FRAME(CREParticleGPU_mfPrepare);


	//hookup this renderer element to the renderer, to ensure that CD3D9Renderer::EF_Flush()
	// calls  CREParticleGPU::mfDraw()
	gRenDev->m_RP.m_pRE = this;

	//
	//gRenDev->m_RP.


	//gRenDev->m_RP.m_NextPtr.PtrB +
	//gRenDev->m_RP.m_RendNumVerts +
	//gRenDev->m_RP.m_RendNumIndices



	/*
	int nElems = m_pVertexCreator->GetNumElements();
	int nMaxVerts = m_pVertexCreator->GetMaxVertexCount();
	int nMaxIndices = nMaxVerts*3 - 6;

	Matrix34 const& matCam = gRenDev->GetCamera().GetMatrix();

	for (int e = 0; e < nElems; e++)
	{
		// Allocate verts & indices.
		gRenDev->EF_CheckOverflow( nMaxVerts, nMaxIndices, this );

		SVertexParticle* pDstVertices = gRenDev->m_RP.m_NextPtr.VBPtr_14;

		// Set vertices.
		int nVerts = m_pVertexCreator->SetVertices(e, pDstVertices, matCam);
		if (nVerts < 4)
			continue;

		if (!gbRgb)
		{
			for (int i = 0; i < nVerts; i++)
				pDstVertices[i].color.dcolor = COLCONV(pDstVertices[i].color.dcolor);
		}
		//assert(nVerts <= nMaxVerts);
		int nIndices = nVerts*3 - 6;

		// Create indices automatically
		int baseVertexIndex = gRenDev->m_RP.m_RendNumVerts;
		uint16* pDstIndices = gRenDev->m_RP.m_RendIndices + gRenDev->m_RP.m_RendNumIndices;

		for (int i = 0; i < nVerts/2-1; i++)
		{
			*pDstIndices++ = 0 + baseVertexIndex;
			*pDstIndices++ = 1 + baseVertexIndex;
			*pDstIndices++ = 2 + baseVertexIndex;

			*pDstIndices++ = 1 + baseVertexIndex;
			*pDstIndices++ = 2 + baseVertexIndex;
			*pDstIndices++ = 3 + baseVertexIndex;

			baseVertexIndex += 2;
		}

		// Update buffer info
		gRenDev->m_RP.m_NextPtr.PtrB += nVerts * sizeof(SVertexParticle);
		gRenDev->m_RP.m_RendNumVerts += nVerts;
		gRenDev->m_RP.m_RendNumIndices += nIndices;
	}
	*/
}


//
CREParticleGPU*
CREParticleGPU::Create( int32 nGPUParticleIdx, AABB const& bb )
{
	CREParticleGPU* pParticle = NULL;

	int index( g_CREParticleGPUStorage.Num() );
	g_CREParticleGPUStorage.GrowReset( 1 );

	pParticle = g_CREParticleGPUStorage[ index ];
	
	if( pParticle == NULL )
	{
		pParticle = new CREParticleGPU;
		g_CREParticleGPUStorage[ index ] = pParticle;
	}

	//pParticle->m_pVertexCreator = pVC;
	pParticle->m_BoundingBox = bb;

	//set particle index
	pParticle->m_nGPUParticleIdx = nGPUParticleIdx;

	return pParticle;
}


//
bool CREParticleGPU::mfPreDraw( SShaderPass *sl )
{

	//CD3D9Renderer* rd(gcpRendD3D);
	//SRenderPipeline& rp(rd->m_RP);

	return true;
}


void 
CRenderer::EF_RemoveGPUParticlesFromScene()
{
	g_CREParticleGPUStorage.SetUse( 0 );
	//g_particleMeshPool.m_vertexBuffer.resize( 0 );
	//g_particleMeshPool.m_indexBuffer.resize( 0 );
}


void 
CRenderer::SafeReleaseParticleGPUREs()
{
	uint32 i;

	for( i = 0; i < g_CREParticleGPUStorage.Num(); ++i )
	{
		SAFE_RELEASE( g_CREParticleGPUStorage[ i ] );
	}

	g_CREParticleGPUStorage.Free();

}


void 
CRenderer::GetMemoryUsageParticleGPUREs( ICrySizer * pSizer )
{
	pSizer->AddObject( &g_CREParticleGPUStorage, g_CREParticleGPUStorage.GetMemoryUsage() );

}

#endif