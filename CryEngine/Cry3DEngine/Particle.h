#ifndef PARTICLE_H
#define PARTICLE_H

#include "CryArray.h"
#include "ParticleEmitter.h"

struct SParticleRenderData;
struct SParticleUpdateContext
{
	SPhysEnviron	PhysEnv;
	uint					nEnvFlags, nSpaceLoopFlags;
	Matrix34			matSpaceLoop, matSpaceLoopInv;

	SParticleUpdateContext()
	{
		nEnvFlags = nSpaceLoopFlags = 0;
		matSpaceLoop = matSpaceLoopInv = Matrix34::CreateIdentity();
	}
};

// For particles with tail, keeps history of previous positions.
struct SParticleHistory
{
	float	fAge;
	Vec3	vPos;

	bool IsUsed() const		{ return fAge >= 0.f; }
	void SetUnused()			{ fAge = -1.f; }
};

// dynamic particle data
// To do opt: subclass for geom particles.

#define PARTICLE_MOTION_BLUR	1

class CParticle : public Cry3DEngineBase
{
public:

  CParticle() 
  {
		memset(this, 0, sizeof(*this));
  }
	~CParticle();

	template<class T> inline
	T GetCurValueBase( TVarEPParam<T> const& param, typename TVarTraits<T>::TStorage base ) const
	{
		T val;
		TVarTraits<T>::FromStorage(val, base);
		return param.GetValueFromBase(val, m_fRelativeAge);
	}

	template<class T> inline
	T GetCurValueMod( TVarEPParam<T> const& param, typename TVarTraits<T>::TStorage mod ) const
	{
		T val;
		TVarTraits<T>::FromStorage(val, mod);
		return param.GetValueFromMod(val, m_fRelativeAge);
	}

	template<class T> inline
	T GetMaxValueMod( TVarEPParam<T> const& param, typename TVarTraits<T>::TStorage mod ) const
	{
		T val;
		TVarTraits<T>::FromStorage(val, mod);
		return val * param.GetMaxValue();
	}

	bool Init( SParticleUpdateContext const& context, float fAge, CParticleSubEmitter* pEmitter, 
		QuatTS const& loc, bool bFixedLoc, Vec3* pVel = NULL, IStatObj* pStatObj = NULL, IPhysicalEntity* pPhysEnt = NULL );
  bool Update( SParticleUpdateContext const& context, float fUpdateTime );
	void GetPhysicsState();

	inline bool IsAlive() const
	{
		return m_fAge <= m_fLifeTime || GetContainer().HasExtendedLifetime();
	}

	void Transform( QuatTS const& qp )
	{
		m_qRot = qp.q * m_qRot;
		m_vPos = qp * m_vPos;
		m_vVel = qp.q * m_vVel * qp.s;
	}

	void UpdateBounds( AABB& bb ) const;

	// Rendering functions.
	struct SVertexContext;
	bool RenderGeometry( SRendParams& RenParamsShared, SVertexContext& context );
	void AddFillLight( CCamera const& cam );
	int SetVertices( SVertexParticle aVerts[], SVertexContext& context ) const;

protected:

  // Current state/param values, needed for updating.
  float		m_fAge;
	float		m_fRelativeAge;
  Vec3		m_vPos;
  Vec3		m_vVel;
	Quat		m_qRot;										// 3D rotational position (geom particles only).
	float		m_fAngle;									// 2D rotation (screen particles only).
	Vec3		m_vRotVel;								// Angular velocity (2D and/or 3D).
	float		m_fClipDistance;					// Distance to nearest clipping border (water, vis area).

	// Collision params.
	uint16	m_nCollisionCount;				// Number of collisions this particle has had.
	int16		m_nCollideMaterial;				// Material sliding against if any.
	Vec3		m_vSlidingNormal;					// Normal of current plane particle sliding against, or 0 (SimplePhysics only)
	float		m_fSlideDistance;					// Distance particle has slid since last collision.

#ifdef PARTICLE_MOTION_BLUR
  // Previous state. Used for motion blur (geom particles only atm)
  Vec3		m_vPosPrev;
  Quat		m_qRotPrev;
#endif

	SParticleHistory*			m_aPosHistory;				// History of positions, for tail. Array allocated by container, maintained by particle.

	// Constant values.
	float									m_fLifeTime;

	typedef TVarTraits<float>::TStorage		TFloatStorage;
	struct SBaseMods
	{
		TFloatStorage
								Size,
								TailLength,
								Stretch,

								AirResistance,
								GravityScale,
								Turbulence3DSpeed,
								TurbulenceSize,
								TurbulenceSpeed,

								LightSourceIntensity,
								LightHDRDynamic,
								LightSourceRadius;

		void Init()
		{
			memset(this, 0xFF, sizeof(*this));
		}
	}											m_BaseMods;

	uint8									m_nTileVariant;
	ColorB								m_BaseColor;

	template<class T>
	void SetVarBase( typename TVarTraits<T>::TStorage& mod, TVarEPParam<T> const& param, float fEmissionRelAge )
	{
		mod = TVarTraits<T>::ToStorage( param.GetVarValue(fEmissionRelAge) );
	}

	template<class T>
	void SetVarMod( typename TVarTraits<T>::TStorage& mod, TVarEPParam<T> const& param, float fEmissionRelAge )
	{
		mod = TVarTraits<T>::ToStorage( param.GetVarMod(fEmissionRelAge) );
	}

	// External references.
	CParticleSubEmitter*	m_pEmitter;						// Emitter who spawned this particle.
	IStatObj*							m_pStatObj;						// Geometry for 3D particles.
	IPhysicalEntity*			m_pPhysEnt;						// Attached physical object.

	CParticleLocEmitter*	m_pChildEmitter;			// Secondary emitter, if effect has indirect children.

	// Functions.
	public:
	CParticleSubEmitter& GetEmitter() const					{ return *m_pEmitter; }
	protected:
	CParticleContainer& GetContainer() const				{ return m_pEmitter->GetContainer(); }
	CParticleLocEmitter& GetLoc() const							{ return m_pEmitter->GetLoc(); }
	ResourceParticleParams const& GetParams() const	{ return GetContainer().GetParams(); }
	SpawnParams const& GetSpawnParams() const				{ return GetLoc().m_SpawnParams; }
	ParticleTarget const& GetTarget() const					{ return m_pEmitter->GetTarget(); }
	inline float GetScale() const										{ return GetLoc().GetParticleScale(); }
	inline float GetSize() const										{ return GetCurValueMod(GetParams().fSize, m_BaseMods.Size) * GetScale(); }

	inline float GetBaseRadius() const
	{
		return m_pStatObj ? m_pStatObj->GetRadius() : 1.414f;
	}
	inline float GetVisibleRadius() const
	{
		return GetBaseRadius() * GetSize();
	}
	inline float GetPhysicalRadius() const
	{
		return GetVisibleRadius() * GetParams().fThickness;
	}

	void InitPos( SParticleUpdateContext const& context, QuatTS const& loc, float fEmissionRelAge );
	void AddPosHistory( SParticleHistory const& histPrev );

	void Physicalize( SParticleUpdateContext const& context );
	int GetSurfaceIndex() const;
	void GetCollisionParams(int nCollSurfaceIdx, float& fBounce, float& fDrag);

	void GetRenderLocation( QuatTS& loc ) const;
	void UpdateChildEmitter();
	bool EndParticle();

	void ComputeRenderData( SParticleRenderData& data, SVertexContext& Context ) const;
	int FillTailVertBuffer( SVertexParticle aTailVerts[], Matrix34 const& matCamera, float fSize ) const;

	void RotateTo( Vec3 const& vNorm )
	{
		m_qRot = Quat::CreateRotationV0V1( m_qRot * Vec3(0,1,0), vNorm ) * m_qRot;
	}
};

#endif // PARTICLE
