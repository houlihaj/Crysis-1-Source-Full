#ifndef __CREPARTICLE_H__
#define __CREPARTICLE_H__

typedef struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F		SVertexParticle;

struct SParticleRenderInfo
{
	AABB				m_bbEmitter;

	SParticleRenderInfo()
	{
		m_bbEmitter.Reset();
	}
};

struct IAllocRender
{
	struct SVertices
	{
		int								nVertices;
		int								nIndices;
		SVertexParticle*	pVertices;
		uint16*						pIndices;
		int								nBaseVertexIndex;

		SVertices()
			{ memset(this, 0, sizeof(*this)); }
	};
	// Render existing SVertices, alloc new ones.
	virtual void operator()( SVertices& alloc, int nAllocVerts, int nAllocInds ) = 0; 
};

struct SParticleRenderContext
{
	// Constant params.
	CCamera const&		m_Camera;
	bool							m_bSwapRGB;
	bool							m_bGeomShader;

	SParticleRenderContext( CCamera const& cam, bool bSwapRGB, bool bGeomShader )
		: m_Camera(cam), m_bSwapRGB(bSwapRGB), m_bGeomShader(bGeomShader)
	{
	}
};

struct IParticleVertexCreator
{
	// Create the vertices for the particle emitter.
	virtual bool SetVertices( IAllocRender& alloc, SParticleRenderContext const& context ) = 0;
};

class CREParticle : public CRendElement
{
public:
	CREParticle() 
	: CRendElement()
	, m_pVertexCreator(0)
  , m_RenInfo()
	{
		mfSetType(eDATA_Particle);
	}
	virtual ~CREParticle() {};
	static CREParticle* Create(IParticleVertexCreator* pVC, SParticleRenderInfo const& context, bool bGeomShader = false );

	bool UseGeomShader() const		{ return (m_Flags & FCEF_GEOM_SHADER) != 0; }

	// interface
	virtual void mfPrepare();
	virtual bool mfCullBox( Vec3& vmin, Vec3& vmax );
	virtual float mfDistanceToCameraSquared( Matrix34& matInst );
	virtual CRendElement* mfCopyConstruct();
	virtual bool mfPreDraw(SShaderPass *sl);
	virtual bool mfDraw(CShader *ef, SShaderPass *sl);

private:
	IParticleVertexCreator*		m_pVertexCreator;
	SParticleRenderInfo				m_RenInfo;
};


#endif  // __CREPARTICLE_H__
