#include "StdAfx.h"
#include "CREParticle.h"

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
static TArray< CREParticle* > g_particleStorage;

CRendElement* 
CREParticle::mfCopyConstruct()
{
	return new CREParticle(*this);
}

float 
CREParticle::mfDistanceToCameraSquared( Matrix34& matInst )
{
	static float fInnerDistRatio = 0.25f;			// Amount to scale distance inside emitter bounds.

	Vec3 vDist = gRenDev->GetRCamera().Orig - m_RenInfo.m_bbEmitter.GetCenter();
	float fDist2 = vDist.GetLengthSquared();

	if (fInnerDistRatio != 1.f)
	{
		if (m_RenInfo.m_bbEmitter.GetVolume())
		{
			// For smoother results, treat the bounding volume as an ellipsoid rather than a box.
			Vec3 vSize = m_RenInfo.m_bbEmitter.GetSize() * 0.5f;
			Vec3 vUni(vDist.x/vSize.x, vDist.y/vSize.y, vDist.z/vSize.z);
			float fUni = vUni.GetLength();
			if (fUni <= 1.f)
				fDist2 *= square(fInnerDistRatio);
			else
				fDist2 *= square((fUni - 1.f + fInnerDistRatio) / fUni);
		}
	}

	return fDist2;
}


bool 
CREParticle::mfCullBox( Vec3& vmin, Vec3& vmax )
{
	return false;
}

struct CAllocRender: public IAllocRender
{
	CREParticle* m_pRE;

	CAllocRender( CREParticle* pRE )
		: m_pRE(pRE)
	{}

	// Render existing SVertices, alloc new ones.
	virtual void operator()( SVertices& alloc, int nAllocVerts, int nAllocInds )
	{
		SRenderPipeline& rp = gRenDev->m_RP;

		// Update pipeline verts based on how many emitter used.
		rp.m_NextPtr.VBPtr_8 += alloc.nVertices;
		rp.m_RendNumVerts += alloc.nVertices;
		rp.m_RendNumIndices += alloc.nIndices;

		if (nAllocVerts)
		{
			// Flush and alloc more.
			gRenDev->EF_CheckOverflow( nAllocVerts, nAllocInds, m_pRE, &alloc.nVertices, &alloc.nIndices );
			alloc.pVertices = rp.m_NextPtr.VBPtr_8;
			alloc.pIndices = rp.m_RendIndices + rp.m_RendNumIndices;
			alloc.nBaseVertexIndex = rp.m_RendNumVerts;
		}
	}
};


void CREParticle::mfPrepare()
{
	gRenDev->EF_StartMerging();
	CAllocRender alloc(this);
	SParticleRenderContext context( gRenDev->GetCamera(), !(gRenDev->GetFeatures() & RFT_RGBA), UseGeomShader() );
	m_pVertexCreator->SetVertices(alloc, context);	
  if (gRenDev->m_RP.m_RendNumVerts)
	  gRenDev->m_RP.m_pRE = this; 
}


CREParticle* CREParticle::Create(IParticleVertexCreator* pVC, SParticleRenderInfo const& info, bool bGeomShader)
{
	int index(g_particleStorage.Num());
	g_particleStorage.GrowReset(1);

	CREParticle* pParticle(g_particleStorage[index]);
	if (!pParticle)
	{
		pParticle = new CREParticle;
		g_particleStorage[index] = pParticle;
	}

	pParticle->m_pVertexCreator = pVC;
	pParticle->m_RenInfo = info;
	if (bGeomShader)
		pParticle->mfUpdateFlags(FCEF_GEOM_SHADER);
	else
		pParticle->mfClearFlags(FCEF_GEOM_SHADER);

	return pParticle;
}


void 
CRenderer::EF_RemoveParticlesFromScene()
{
	g_particleStorage.SetUse( 0 );
	
	g_particleMeshPool.m_vertexBuffer.resize( 0 );
	g_particleMeshPool.m_indexBuffer.resize( 0 );
}


void 
CRenderer::SafeReleaseParticleREs()
{
	for( uint32 i( 0 ); i < g_particleStorage.Num(); ++i )
	{
		SAFE_RELEASE( g_particleStorage[ i ] );
	}
	g_particleStorage.Free();
}


void 
CRenderer::GetMemoryUsageParticleREs( ICrySizer * pSizer )
{
	pSizer->AddObject( &g_particleStorage, g_particleStorage.GetMemoryUsage() );
	pSizer->AddObject( &g_particleMeshPool.m_vertexBuffer, g_particleMeshPool.m_vertexBuffer.capacity() * sizeof( SParticleMeshPool::ParticleVertexBuffer::value_type ) );
	pSizer->AddObject( &g_particleMeshPool.m_indexBuffer, g_particleMeshPool.m_indexBuffer.capacity() * sizeof( SParticleMeshPool::ParticleIndexBuffer::value_type ) );
}

