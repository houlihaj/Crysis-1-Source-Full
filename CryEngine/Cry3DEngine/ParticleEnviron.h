#ifndef PARTICLE_ENVIRON_H
#define PARTICLE_ENVIRON_H

#include "ParticleParams.h"

class CParticleEmitter;

//////////////////////////////////////////////////////////////////////////
// Physical or visual phenomena affecting particles.
enum EParticleEnviron
{
	ENV_GRAVITY				= 1,
	ENV_WIND					= 2,
	ENV_WATER					= 4,
	ENV_FORCE					= 8,
	ENV_TERRAIN				= 16,
	ENV_STATIC_ENT		= 32,
	ENV_DYNAMIC_ENT		= 64,
	ENV_SHADOWS				= 128,
	ENV_CAST_SHADOWS	= 256,
};

//////////////////////////////////////////////////////////////////////////
// Locking pointer.

class SpinLockPtr
{
public:
	SpinLockPtr(volatile int* pLock = 0)
		: m_pSpinLock(pLock)
	{
		if (m_pSpinLock)
			CryInterlockedAdd(m_pSpinLock, 1);
	}
	~SpinLockPtr()
	{
		if (m_pSpinLock)
			CryInterlockedAdd(m_pSpinLock, -1);
	}
	SpinLockPtr( const SpinLockPtr& sp )
		: m_pSpinLock(sp.m_pSpinLock)
	{
		if (m_pSpinLock)
			CryInterlockedAdd(m_pSpinLock, 1);
	}
	SpinLockPtr& operator=( const SpinLockPtr& sp )
	{
		if (m_pSpinLock)
			CryInterlockedAdd(m_pSpinLock, -1);
		m_pSpinLock = sp.m_pSpinLock;
		if (m_pSpinLock)
			CryInterlockedAdd(m_pSpinLock, 1);
		return *this;
	}

protected:
	volatile int*		m_pSpinLock;
};

//////////////////////////////////////////////////////////////////////////
// Physical environment management.
//
struct SPhysEnviron: Cry3DEngineBase
{
	// PhysArea caching.
	Vec3							m_vUniformGravity;
	Vec3							m_vUniformWind;
	Plane							m_plUniformWater;
	ETrinary					m_tUnderWater;
	uint							m_nNonUniformFlags;		// EParticleEnviron flags of non-uniform areas found.

	// Nonuniform area support.
	// Only collected if array member non 0.
	struct SArea
	{
		IPhysicalEntity*	m_pArea;
		uint							m_nFlags;
		SpinLockPtr				m_pLock;

		// Area params, for simple evaluation.
		bool							m_bCacheForce;				// Quick cached force info.
		bool							m_bRadial;						// Forces are radial from center.
		Vec3							m_vCenter;						// Area position (for radial forces).
		Matrix34					m_matToLocal;					// Convert to unit sphere space.
		float							m_fInnerRadius;				// Inner fraction of sphere at max strength.
		Vec3							m_vGravity, m_vWind;	// World-space vectors.
		Plane							m_plWater;				

		SArea()
			{ memset(this, 0, sizeof(*this)); }
		void GetForce( Vec3 const& vPos, Vec3& vGravity, Vec3& vWind ) const;
		void GetForcePhys( Vec3 const& vPos, Vec3& vGravity, Vec3& vWind ) const;
	};

	SPhysEnviron()
		: m_paNonUniformAreas(0)
	{ 
		Clear();
	}

	~SPhysEnviron()
	{
		if (m_paNonUniformAreas)
			m_paNonUniformAreas->resize(0);
	}

	// Phys areas
	void Clear();

	void GetPhysAreas( const CParticleEmitter* pEmitter, AABB const& box, uint nFlags, DynArray<SArea>* paNonUniformAreas = 0 );

	inline void CheckPhysAreas( Vec3 const& vPos, Vec3& vGravity, Vec3& vWind, uint nFlags ) const
	{
		vGravity = m_vUniformGravity;
		vWind = m_vUniformWind;
		if (m_paNonUniformAreas)
			for_array (i, (*m_paNonUniformAreas))
			{
				if ((*m_paNonUniformAreas)[i].m_nFlags & nFlags)
					(*m_paNonUniformAreas)[i].GetForce( vPos, vGravity, vWind );
			}
	}

	float DistanceAboveWater( Vec3 const& vPos, float fMaxDist, Plane& plWater ) const;

	// Phys collision
	bool PhysicsCollision( ray_hit& hit, Vec3 const& vStart, Vec3 const& vEnd, float fRadius, uint nEnvFlags ) const;

protected:

	DynArray<SArea>*	m_paNonUniformAreas;
};

//////////////////////////////////////////////////////////////////////////
// Vis area management.
//
struct SVisEnviron: Cry3DEngineBase
{
	SVisEnviron()
	{
		Clear();
	}

	void Clear()
	{
		memset(this, 0, sizeof(*this));
	}

	void Invalidate()
	{
		m_bValid = false;
	}	
	void Update( Vec3 const& vOrigin, AABB const& bbExtents );

	bool NeedVisAreaClip() const
	{
		return m_pClipVisArea != 0;
	}

	bool ClipVisAreas( Sphere& sphere, Vec3 const& vNormal ) const
	{
		if (!m_pClipVisArea)
			return false;
		// Clip inside or outside specified area.
		return m_pClipVisArea->ClipToVisArea(m_pClipVisArea == m_pVisArea, sphere, vNormal);
	}

	bool OriginIndoors() const
	{
		return m_pVisArea != 0;
	}

	void OnVisAreaDeleted(IVisArea* pVisArea)
	{
		if (m_pVisArea == pVisArea || m_pClipVisArea == pVisArea)
			Clear();
	}

protected:
	bool							m_bValid;							// True if VisArea determination up to date.
	bool							m_bCrossesVisArea;
	IVisArea*					m_pVisArea;						// VisArea emitter is in, if needed and if any.
	IVisArea*					m_pClipVisArea;				// VisArea needed to clip against (-1 if all).
	void*							m_pVisNodeCache;
};

//////////////////////////////////////////////////////////////////////////
// GeomRef functions.

void GetAABB( AABB& bb, QuatTS& tLoc, GeomRef const& geom );
float GetExtent( GeomQuery& geo, GeomRef const& geom, EGeomType eType, EGeomForm eForm );
bool GetRandomPos( RandomPos& ran, GeomQuery& geo, GeomRef const& geom, EGeomType eType, EGeomForm eForm, QuatTS const& tWorld );

#endif
