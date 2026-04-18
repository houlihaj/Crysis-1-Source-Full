////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ParticleEmitter.h
//  Version:     v1.00
//  Created:     18/7/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __particleemitter_h__
#define __particleemitter_h__
#pragma once

#ifdef _WIN32
#pragma warning(disable: 4355)
#endif

#include "ParticleEffect.h"
#include "ParticleEnviron.h"
#include "partman.h"
#include "GeomQuery.h"
#include "CryArray.h"
#include "PoolAllocator.h"
#include "BitFiddling.h"
#include "CryThread.h"

#undef PlaySound

class CParticle;
class CParticleContainer;
class CParticleSubEmitter;
class CParticleLocEmitter;
class CParticleEmitter;
struct SParticleRenderContext;
struct SParticleUpdateContext;
struct SParticleHistory;

#define fHUGE			1e18f		// Use for practically infinite values, to simplify comparisons.
													// Can subtract up to 1e10 without changing its value, under 32bit precision.
													// < sqrt(FLT_MAX), so safe for multiplying by itself once.
													// In seconds, > 30 billion years.
													// In meters, > 100 light years.

//////////////////////////////////////////////////////////////////////////
// A variable-sized list of items with a pre-allocated maximum.
// Allows efficient element deletion during iteration.

template<class T>
class FixedArray: public DynArray<T>
{
public:
	// Redundant declarations for confused compilers.
#if defined(PS3)
	typedef DynArray< T, FModuleAllocAlign<128> > super_type;
#else
	typedef DynArray<T> super_type;
#endif
	using super_type::size;
	using super_type::capacity;
	using super_type::begin;
	using super_type::end;
	using super_type::header;

	inline typename super_type::iterator push_back_fixed()
	{
		// Streamlined function, which maintains a fixed-capacity queue.
		assert(capacity() > 0);
		if (size() < capacity())
			header()->nSize++;
		else
		{
			begin()->~T();
			move(begin(), end()-1, begin()+1);
		}
		return new(end()-1) T;
	}

	class iterator
	{
		// Iterator that can go in either direction.
	public:
		iterator(FixedArray<T>& list, bool bReverse = false)
		{
			if (!bReverse)
			{
				m_pCur = list.begin();
				m_pEnd = list.end();
				m_nDelta = 1;
			}
			else
			{
				m_pCur = list.end()-1;
				m_pEnd = list.begin()-1;
				m_nDelta = -1;
			}
		}

		operator bool() const
		{
			return m_pCur != m_pEnd;
		}
		void operator ++()
		{
			m_pCur += m_nDelta;
		}
		T& operator *() const
		{
			return *m_pCur;
		}
		T* operator ->() const
		{
			return m_pCur;
		}

		int count_remaining() const
		{
			return (m_pEnd - m_pCur) * m_nDelta;
		}

	protected:
		T*		m_pCur;
		T*		m_pEnd;
		int		m_nDelta;
	};

	class traverser
	{
		// Iterator supports in-place erase.
	public:
		traverser(FixedArray<T>& list)
			: m_list(list)
		{
			m_pCur = m_pNext = list.begin();
		}
		~traverser()
		{
			if (m_pCur != m_pNext)
			{
				memcpy(m_pCur, m_pNext, m_list.end()-m_pNext);
				m_list.resize_raw(m_pCur - m_list.begin());
			}
		}

		operator bool() const
		{
			return m_pNext < m_list.end();
		}
		void operator ++()
		{
			m_pCur++;
			m_pNext++;
			if (m_pCur != m_pNext && m_pNext < m_list.end())
				// Copy next element to current pos.
				// Raw copy, as element was already destroyed.
				memcpy(m_pCur, m_pNext, sizeof(*m_pCur));
		}
		T& operator *() const
		{
			return *m_pCur;
		}
		T* operator ->() const
		{
			return m_pCur;
		}

		void erase()
		{
			// Destroy element, and move to previous slot, so it's overwritten.
			m_pCur->~T();
			m_pCur--;
		}

	protected:
		FixedArray<T>& m_list;
		T*						m_pCur;
		T*						m_pNext;
	};
};

//////////////////////////////////////////////////////////////////////////
// Particle rendering helper params.
struct SPartRenderParams
{
	const CCamera&	m_Camera;
	float						m_fCamDistance;
	float						m_fViewDistRatio;
	uint16					m_nFogVolumeContribIdx;
	uint16					m_nEmitterOrder;

	SPartRenderParams( const CCamera& cam, float fCamDist, float fVDR, uint16 nFogIdx )
	: m_Camera(cam), m_fCamDistance(fCamDist), m_fViewDistRatio(fVDR), m_nFogVolumeContribIdx(nFogIdx), m_nEmitterOrder(0)
	{}
};

//////////////////////////////////////////////////////////////////////////
// Contains, updates, and renders a list of particles, of a single effect

class CParticleContainer : public _i_reference_target_t, public Cry3DEngineBase, public IParticleVertexCreator
{
public:

	CParticleContainer( CParticleContainer* pParent, CParticleSubEmitter* pSub, CParticleLocEmitter const* pLoc, IParticleEffect const* pEffect, ParticleParams const* pParams = 0 );
	~CParticleContainer();

	ResourceParticleParams const& GetParams() const		{ return *m_pParams; }
	CParticleEffect* GetEffect() const								{ return m_pEffect; }
	CParticleLocEmitter const& GetLoc() const					{ return *m_pLocEmitter; }
	CParticleEmitter& GetMain() const;

	bool IsIndirect() const
	{
		return m_pParentContainer != 0;
	}
	CParticleContainer* GetParent() const
	{
		return m_pParentContainer;
	}

	void Update();
	void OnSetLoc( QuatTS const& qp );

	void Reset();

	void AddIndirectContainer( IParticleEffect const* pEffect );
	CParticleLocEmitter* CreateIndirectEmitter( CParticleSubEmitter* pParentEmitter, float fPast = 0.f );
	void DeleteIndirectEmitter( CParticleLocEmitter* pEmitter );
	int GetParticleRefs( CParticleSubEmitter const* pSub ) const;

	CParticle* AddParticle(bool bGrow = false, bool bReuse = false);
	void DeleteParticle(CParticle* pPart);
	void Prime();
	void UpdateParticles();
	void UpdateChildEmitters();
	void EmitParticles( SParticleUpdateContext const& context );
	void EmitParticles( SParticleUpdateContext const& context, CParticleContainer* pIndirect );
	void UpdateBounds(AABB& bbMain, AABB& bbDyn);
	void InvalidateStaticBounds();
	void Render( SRendParams const& RenParams, SPartRenderParams& PRParams );
	void RenderGeometry( const SRendParams& RenParams, const SPartRenderParams& PRParams );
	void UnqueueUpdate();
	void ComputeVertices();
	void OnThreadUpdate();
	void SetDynamicBounds()
	{
		m_bDynamicBounds = true;
	}
	bool NeedsDynamicBounds() const
	{
		return m_bDynamicBounds;
	}
	bool CanThread() const;
	ParticleTarget const& GetTarget() const;
	uint GetEnvironmentFlags( bool bAll = false ) const
	{
		uint flags = m_pParams->nEnvFlags;
		if (bAll)
			for_array (i, m_IndirectContainers)
				flags |= m_IndirectContainers[i].GetEnvironmentFlags( bAll );
		return flags;
	}
	char GetRenderType() const;

	bool IsImmortal() const;
	bool HasEquilibrium() const;
	float GetEquilibriumAge(bool bAll = true) const;
	bool HasExtendedLifetime() const
	{
		return m_bExtendedLife;
	}
	float GetEmitCountScale() const
	{
		return m_fEmitCountScale;
	}
	float GetMaxParticleLife(bool bAll = true) const;
	float GetMaxParticleSize(bool bAdjusted, bool bAll) const;
	bool OwnsParams() const
	{
		return !m_pEffect;
	}
	float GetTimeToUpdate() const
	{
		if (m_timeLastUpdate == CTimeValue())
			return GetAge();
		else
			return (GetTimer()->GetFrameStartTime() - m_timeLastUpdate).GetSeconds();
	}

	// Temp functions to update edited effects.
	inline bool IsUsed() const		
	{ 
		return !m_bUnused;
	}
	void SetUsed(bool b, bool bAll = false);
	void ClearUnused();

	bool Is( CParticleContainer const* pParent, CParticleLocEmitter const* pLoc, IParticleEffect const* pEffect ) const
	{
		return m_pParentContainer == pParent && m_pLocEmitter == pLoc && (IParticleEffect *)m_pEffect == pEffect;
	}

	// Particle tail management.
	SParticleHistory* AllocPosHistory();
	void FreePosHistory( SParticleHistory* aHist );
	int GetHistorySteps() const
	{
		return m_nHistorySteps;
	}

	// Stat/profile functions.
	SParticleCounts& GetCounts() 
	{ 
		return m_Counts; 
	}
	SParticleCounts const& GetCounts() const
	{ 
		return m_Counts; 
	}
	void GetCounts( SParticleCounts& counts, bool bAll = false, bool bEffectStats = false ) const;
	void ClearCounts()
	{
		m_Counts.Clear();
		for_all (m_IndirectContainers).ClearCounts();
	}

	// Return whether any sub-containers use static (flag 1) and/or dynamic (flag 2) bounding boxes.
	uint BoundsTypes() const
	{
		uint nFlags = NeedsDynamicBounds() ? 2 : 1;
		for_array (i, m_IndirectContainers)
			nFlags |= m_IndirectContainers[i].BoundsTypes();
		return nFlags;
	}

	void GetMemoryUsage( ICrySizer* pSizer );

	//////////////////////////////////////////////////////////////////////////
	// IParticleVertexCreator methods

	virtual bool SetVertices( IAllocRender& alloc, SParticleRenderContext const& context );

protected:

	ResourceParticleParams*					m_pParams;				// Pointer to particle params (effect or code).
	_smart_ptr<CParticleEffect>			m_pEffect;				// Particle effect used for this emitter.

	// Shared array of tail positions. Must be declared before particle list, to be destroyed after.
	DynArray<SParticleHistory>			m_aHistoryPool;
	SParticleHistory*								m_pHistoryFree;
	int															m_nHistorySteps;

	// Fixed-size array of particles.
	typedef FixedArray<CParticle>		TParticleList;
	TParticleList										m_Particles;
	int															m_nReserveCount;
	float														m_fEmitCountScale;

	// Last time when emitter updated, and static bounds validated.
	CTimeValue											m_timeLastUpdate;
	CTimeValue											m_timeStaticBounds;

	// Final bounding volume for rendering. Also separate static & dynamic volumes for sub-computation.
	AABB														m_bbWorld, m_bbWorldStat, m_bbWorldDyn;
	bool														m_bDynamicBounds;						// Requires dynamic bounds computation.
	bool														m_bExtendedLife;						// Particles lifetime extended; don't kill yet.
	bool														m_bUnused;									// Temp var used during param editing.
	DynArray<SPhysEnviron::SArea>		m_aNonUniformAreas;					// Persistent array.

	// Associated structures.
	CParticleSubEmitter*						m_pDirectSubEmitter;				// Emitter into this container, if direct.
	CParticleContainer*							m_pParentContainer;					// Parent container, if indirect.
	SmartPtrArray<CParticleContainer>		m_IndirectContainers;		// Containers for particles with indirect child effects.
	PtrArray<CParticleLocEmitter>		m_IndirectEmitters;					// Emitters into indirect child containers,
																															// controlled by this container's particles.
	CParticleLocEmitter const*			m_pLocEmitter;							// Emitter defining container's location.

#ifdef SHARED_GEOM
	SInstancingInfo									m_InstInfo;									// For geom particle rendering.
#endif
	SParticleCounts									m_Counts;										// Counts for stats.

	// Threading support.
	CryLock<CRYLOCK_RECURSIVE>&			m_Lock;
	int															m_nQueuedFrame;
	bool														m_bRenderGeomShader;				// Prepare vertices for geometry shader support.

	bool														m_bVertsComputed;						// Whether vertices are computed and current.
	bool														m_bRendered;
	DynArray<SVertexParticle>				m_aRenderVerts;							// Prepared render vertices, allocated when threading.

	//
	// Time functions.
	//

	float GetAge() const;

	// Bounds functions.
	bool NeedsExtendedBounds() const;
	void ComputeStaticBounds( AABB& bb, bool bWithSize = true, float fMaxLife = fHUGE );
	inline bool StaticBoundsStable() const
	{
		return m_timeStaticBounds < GetTimer()->GetFrameStartTime();
	}
	void GetDynamicBounds( AABB& bb );
	void ComputeUpdateContext( SParticleUpdateContext& context );

	void ComputeReserveCount();
	int GetMaxVertexCount() const;

	// Other functions.
	void ReleaseParams();
	void UnloadResources();
	void QueueUpdate( bool bNext );
};

//////////////////////////////////////////////////////////////////////////
// Maintains an emitter source state, emits particles to a container

enum EActivateMode
{ 
	eAct_Activate,
	eAct_Deactivate,
	eAct_Reactivate,
	eAct_Restart,
	eAct_Pause,
	eAct_Resume,
	eAct_ParentCollision,
	eAct_ParentDeath
};

enum EEmitterState
{
	eEmitter_Dead,				// No current or future particles.
	eEmitter_Dormant,			// No current particles, but may in future.
	eEmitter_Particles,		// Currently has particles.
	eEmitter_Active,			// Currently emitting particles
};

class CParticleSubEmitter: public _i_reference_target_t, public Cry3DEngineBase
{
public:

	CParticleSubEmitter( CParticleSubEmitter const* pParent, CParticleContainer* pCont, CParticleLocEmitter* pLoc );
	~CParticleSubEmitter();

	ResourceParticleParams const& GetParams() const	{ return *m_pParams; }
	CParticleContainer& GetContainer() const				{ return *m_pContainer; }
	CParticleLocEmitter& GetLoc() const							{ return *m_pLocEmitter; }
	ParticleTarget const& GetTarget() const					{ return m_Target; }
	CParticleEmitter& GetMain() const;

	// State.
	EEmitterState GetState() const;
	bool IsAlive() const														{ return GetState() > eEmitter_Dead; }
	void Activate( EActivateMode mode, float fPast = 0.f );

	// Special reference counting.
	void AddParticleRef( int nRefs ) const	
	{
		m_nParticleRefs += nRefs; 
		assert(m_nParticleRefs >= 0);
	}
	bool IsIndirectChild( CParticleSubEmitter const* pParent ) const
	{
		return m_pParentEmitter && m_pParentEmitter == pParent && GetContainer().GetParent() == &m_pParentEmitter->GetContainer();
	}

	// Timing.
	float GetAge() const;
	float GetRelativeAge( float fAgeAdjust = 0.f ) const
	{
		float fEnd = m_fEndAge < fHUGE ? m_fEndAge : m_fRepeatAge;
		if (fEnd <= m_fStartAge)
			return 0.f;
		return (GetAge() + fAgeAdjust - m_fStartAge) / (fEnd - m_fStartAge);
	}
	float GetEquilibriumAge(bool bAll = true) const
	{
		return m_fStartAge + GetContainer().GetMaxParticleLife(bAll);
	}

	// Param evaluation.
	template<class T> inline
	T GetCurValue( TVarParam<T> const& param ) const
	{
		return param.GetVarValue( m_ChaosKey );
	}

	template<class T> inline
	T GetCurValue( TVarEParam<T> const& param, T Abs = T(0) ) const
	{
		return param.GetVarValue( m_ChaosKey, m_fRelativeAge, Abs );
	}

	template<class T> inline
	T GetCurValue( float fMinMax, TVarEParam<T> const& param ) const
	{
		return param.GetVarValue( fMinMax, m_fRelativeAge );
	}

	float GetEmitRate() const;
	bool GetParamTarget( ParticleTarget& target ) const;

	// Actions.
	void OnSetLoc( QuatTS const& qp );
	void Update();
	void ResetLoc()
	{
		m_bMoved = false;
	}
	void UpdateTarget();
	void UpdateForce();
	int EmitParticle( SParticleUpdateContext const& context, bool bGrow, float fAge, IStatObj* pStatObj = NULL, IPhysicalEntity* pPhysEnt = NULL, QuatTS* pLocation = NULL, Vec3* pVel = NULL );
	void EmitParticles( SParticleUpdateContext const& context );

	bool Is( CParticleSubEmitter const* pParent, IParticleEffect const* pEffect ) const
	{
		return m_pParentEmitter == pParent && GetContainer().GetEffect() == pEffect;
	}

	void GetMemoryUsage(ICrySizer* pSizer)
	{
		if (HasDirectContainer())
			GetContainer().GetMemoryUsage(pSizer);
	}

protected:

	// Associated structures.
	ResourceParticleParams const*		m_pParams;
	CParticleContainer*							m_pContainer;					// Direct or shared container to emit particles into.
	CParticleSubEmitter const*			m_pParentEmitter;
	CParticleLocEmitter*						m_pLocEmitter;

	ParticleTarget									m_Target;							// Target attractor.

	// State.
	mutable int		m_nParticleRefs;	// Number of child objects referencing this.
	bool					m_bStarted;				// Has passed start age.
	bool					m_bMoved;					// Moved since last update.

	float					m_fStartAge;			// Relative age when scheduled to start (default 0).
	float					m_fEndAge;				// Relative age when scheduled to end (0 if never).
	float					m_fRepeatAge;			// Relative age when scheduled to repeat (0 if never).

	float					m_fRelativeAge;		// Current relative age, updated each frame.

	CChaosKey			m_ChaosKey;				// Seed for randomising; inited every pulse.
	float					m_fToEmit;				// Fractional particles to emit.
	QuatTS				m_qpLastLoc;

	// External objects.
	_smart_ptr<ISound> m_pSound;
	IPhysicalEntity*	m_pForce;

	// Methods.
	void Initialize( float fPast = 0.f );
	void Deactivate();
	void PlaySound();

	float GetEndAge() const;

	inline bool HasDirectContainer() const
	{
		return !m_pContainer->IsIndirect();
	}

	inline float GetTimeToUpdate() const
	{
		return min( GetContainer().GetTimeToUpdate(), GetAge() );
	}
	void UpdateRelativeAge()
	{
		m_fRelativeAge = min(GetRelativeAge(), 1.f);
	}
};

//////////////////////////////////////////////////////////////////////////
// Combines an emitter and container (standard direct emitter)

class CParticleDirectEmitter : private CParticleContainer, public CParticleSubEmitter
{
public:
	CParticleDirectEmitter( CParticleSubEmitter const* pParent, CParticleLocEmitter* pLoc, IParticleEffect const* pEffect, ParticleParams const* pParams = 0 )
	: CParticleContainer(0, this, pLoc, pEffect, pParams), CParticleSubEmitter(pParent, this, pLoc)
	{
	}
};

//////////////////////////////////////////////////////////////////////////
// An emitter system, maintains several emitters, with a single world location and presence.
// Can be top-level, or attached to a particle (indirect emitter)

class CParticleLocEmitter: public Cry3DEngineBase
{
public:

	// World location.
	QuatTS							m_qpLoc;

	// Custom particle-emission shape, options, and targeting.
	SpawnParams					m_SpawnParams;
	GeomRef							m_EmitGeom;
	mutable GeomQuery		m_GeomQuery;

public:

	CParticleLocEmitter( CParticleEmitter* pMainEmitter, float fAge = 0.f );
	~CParticleLocEmitter();

	// Add direct effect.
	void AddEffect( CParticleSubEmitter* pParent, IParticleEffect const* pEffect, ParticleParams const* pParams = 0 );
	void SetEffect( IParticleEffect const* pEffect, ParticleParams const* pParams );

	// Add indirect effect.
	int AddEmitters( CParticleSubEmitter* pParent, SmartPtrArray<CParticleContainer>& IndirectContainers, int iStart = 0 );

	// Location.
	void SetLoc( QuatTS const& qp );

	Matrix34 GetMatrix() const													{ return Matrix34( Vec3(m_qpLoc.s), m_qpLoc.q, m_qpLoc.t ); }
	Vec3 GetWorldPosition( Vec3 const& relpos ) const		{ return m_qpLoc * relpos; }
	Vec3 GetWorldOffset( Vec3 const& relpos ) const			{ return m_qpLoc.q * relpos * m_qpLoc.s; }
	float GetParticleScale() const;									

	CParticleEmitter& GetMain() const										{ return *m_pMainEmitter; }

	void SetVel( Vec3 const& v )												{ m_vVel = v; }
	Vec3 const& GetVel() const													{ return m_vVel; }

	void SetSpawnParams( SpawnParams const& spawnParams, GeomRef geom = GeomRef() );
	void SetEmitGeom( GeomRef geom );
	float GetEmitCountScale() const;

	// Time.
	inline CTimeValue GetTimeCreated() const
	{
		return m_timeCreated;
	}
	inline float GetAge() const
	{
		return (GetTimer()->GetFrameStartTime() - m_timeCreated).GetSeconds();
	}
	inline float GetStopAge() const
	{
		return m_fStopAge;
	}
	inline bool IsActive() const
	{
		return m_bActive;
	}

	// State.
	EEmitterState GetState() const;
	bool IsAlive() const																{ return GetState() > eEmitter_Dead; }
	void Activate( EActivateMode mode, float fPast = 0.f );
	void SetUsed( bool b )															{ for_all (m_SubEmitters).GetContainer().SetUsed(b, true); }
	void ClearUnused();
	void Prime();

	// Action.
	void Update()																				{ for_all (m_SubEmitters).Update(); }
	void InvalidateStaticBounds()												{ for_all (m_SubEmitters).GetContainer().InvalidateStaticBounds(); }

	void EmitParticles( SParticleUpdateContext const& context, CParticleContainer* pCont )
	{
		for_array (i, m_SubEmitters)
			if (&m_SubEmitters[i].GetContainer() == pCont)
				m_SubEmitters[i].EmitParticles(context);
	}

	void RemoveEmitter( CParticleContainer* pCont )
	{
		for_array (i, m_SubEmitters)
			if (&m_SubEmitters[i].GetContainer() == pCont)
				i = m_SubEmitters.erase(i);
	}

	uint GetEnvironmentFlags() const
	{
		uint nFlags = 0;
		for_array (i, m_SubEmitters)
			nFlags |= m_SubEmitters[i].GetContainer().GetEnvironmentFlags(true);
		return nFlags;
	}

	uint BoundsTypes() const
	{
		uint nFlags = 0;
		for_array (i, m_SubEmitters)
			nFlags |= m_SubEmitters[i].GetContainer().BoundsTypes();
		return nFlags;
	}

	float GetMaxParticleSize(bool bAdjusted) const
	{
		float fSize = 0.f;
		for_array (i, m_SubEmitters)
			fSize = max(fSize, m_SubEmitters[i].GetContainer().GetMaxParticleSize(bAdjusted, true));
		return fSize;
	}

	int GetParticleRefs(CParticleSubEmitter const* pSub) const
	{
		int nRefs = 0;
		for_array (i, m_SubEmitters)
			if (m_SubEmitters[i].IsIndirectChild(pSub))
				nRefs++;
		return nRefs;
	}

	void GetMemoryUsage(ICrySizer* pSizer);
	bool GetMoveRelEmitter() const {return m_bMoveRelEmitter;}

protected:

	CParticleEmitter*											m_pMainEmitter;

	Vec3																	m_vVel;								// World velocity (transfered to particles).

	CTimeValue														m_timeCreated;				// Creation and deactivation time for emitter system.
	float																	m_fStopAge;						// Age prematurely paused or deactivated, or fHUGE.
	bool																	m_bActive;						// Allowed to live.

	PtrArray<CParticleSubEmitter>					m_SubEmitters;
	bool																	m_bMoveRelEmitter; //hack for on train particles move with train
};

//////////////////////////////////////////////////////////////////////////
// A top-level emitter system, interfacing to 3D engine

class CParticleEmitter : public IParticleEmitter, public CParticleLocEmitter
{
	typedef CParticleLocEmitter	TParent;

public:

	CParticleEmitter( bool bIndependent );
	~CParticleEmitter();

	//////////////////////////////////////////////////////////////////////////
	// IRenderNode implementation.
	//////////////////////////////////////////////////////////////////////////

	virtual void ReleaseNode()													{ Register(false); }	
	virtual EERType GetRenderNodeType()									{ return eERType_ParticleEmitter; }
	virtual char const* GetName() const									{ return GetEffect() ? GetEffect()->GetName() : "Emitter"; }
	virtual char const* GetEntityClassName() const			{ return "ParticleEmitter"; }
	virtual string GetDebugString(char type = 0) const;

	virtual Vec3 GetPos(bool bWorldOnly = true) const 
																											{ return m_qpLoc.t; }
	virtual void GetLocalBounds( AABB &bbox );
	virtual float GetMaxViewDist();
	virtual void SetMatrix( Matrix34 const& mat )				{ SetLoc(QuatTS(mat)); }

	virtual void SetMaterial( IMaterial* pMaterial )		{ m_pMaterial = pMaterial; }
	virtual IMaterial* GetMaterial(Vec3 * pHitPos = NULL)							
	{ 
		return m_pMaterial ? (IMaterial *)m_pMaterial 
			: m_SubEmitters.size() > 0 ? (IMaterial *)m_SubEmitters[0].GetParams().pMaterial
			: 0;
	}

	virtual IPhysicalEntity *GetPhysics() const					{ return 0; }
	virtual void SetPhysics(IPhysicalEntity *)					{}

	virtual bool Render( SRendParams const& rParam );
	virtual void Hide( bool bHide )											{ Activate(bHide ? eAct_Pause : eAct_Resume); }

	//////////////////////////////////////////////////////////////////////////
	// IParticleEmitter implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetEffect( IParticleEffect const* pEffect, ParticleParams const* pParams );
	virtual void SetEffect( IParticleEffect const* pEffect )	
	{ 
		SetEffect(pEffect, 0);
	}
	IParticleEffect* GetEffect() const												{ return m_pTopEffect; }
	inline ParticleTarget const& GetTarget() const						{ return m_Target; }

	virtual void SetTarget( ParticleTarget const& target )
	{
		if ((int)target.bPriority >= (int)m_Target.bPriority)
			m_Target = target;
	}
	virtual void SetSpawnParams( SpawnParams const& spawnParams, GeomRef geom = GeomRef() );
	virtual bool IsAlive() const															{ return TParent::IsAlive(); }
	virtual void Prime()																			{ TParent::Prime(); }
	virtual void Activate( bool bActive );
	virtual void Restart()																		{ Activate(eAct_Restart); }
	virtual void Update();
	virtual void EmitParticle( IStatObj* pStatObj = NULL, IPhysicalEntity* pPhysEnt = NULL, QuatTS* pLocation = NULL, Vec3* pVel = NULL );
	virtual void SetEntity( IEntity* pEntity, int nSlot );
	virtual void Serialize( TSerialize ser );
	virtual void GetMemoryUsage(ICrySizer* pSizer);
  virtual const AABB GetBBox() const { return m_WSBBox; }
  virtual void SetBBox( const AABB& WSBBox ) { m_WSBBox = WSBBox; }
	virtual void SetMoveRelEmitterTrue() {m_bMoveRelEmitter=true;}

	//////////////////////////////////////////////////////////////////////////
	// Other methods.
	//////////////////////////////////////////////////////////////////////////

	using CParticleLocEmitter::Activate;

	inline bool IsRegistered() const			{ return m_bRegistered; }

	void Register( bool b );

	void UpdateForce()																	
	{ 
		if (m_nEnvFlags & ENV_FORCE)
			for_all (m_SubEmitters).UpdateForce(); 
	}

	void SetLoc( QuatTS const& qp );
	float GetCreationTime() const												
	{ 
		return m_timeCreated.GetMilliSeconds(); 
	}
	CTimeValue GetCreationTimeValue() const
	{
		return m_timeCreated;
	}

	SPhysEnviron const& GetUniformPhysEnv() const
	{
		return m_PhysEnviron;
	}
	SVisEnviron const& GetVisEnviron() const
	{
		return m_VisEnviron;
	}
	void OnVisAreaDeleted( IVisArea* pVisArea )
	{
		m_VisEnviron.OnVisAreaDeleted(pVisArea);
	}

	void Reset()
	{ 
		for_all (m_SubEmitters).GetContainer().Reset(); 
	}

	void RenderDebugInfo();
	void UpdateFromEntity();
	float GetViewDistRatio() const;
	bool IsIndependent() const
	{
		return m_bIndependent;
	}
	bool NeedSerialize() const
	{
		return m_bIndependent && GetEffect() != 0;
	}
	bool IsSelected() const
	{
		return m_bSelected;
	}
	float TimeNotRendered() const
	{
		return (GetTimer()->GetFrameStartTime() - m_timeLastRendered).GetSeconds();
	}

	void GetCounts( SParticleCounts& counts, bool bEffectStats = false ) const	
	{ 
		for_all (m_SubEmitters).GetContainer().GetCounts(counts, true, bEffectStats); 
	}
	void ClearCounts()
	{ 
		for_all (m_SubEmitters).GetContainer().ClearCounts();; 
	}

public:
	// Pools of items used by associated structures.
	stl::TPoolAllocator<CParticleLocEmitter>
															m_poolLocEmitters;		// Location emitters for particles with children.
	stl::TPoolAllocator<CParticleSubEmitter>
															m_poolSubEmitters;		// Sub-emitters for particles with children.
protected:

	_smart_ptr<CParticleEffect>	m_pTopEffect;
	_smart_ptr<IMaterial>				m_pMaterial;			// Override material for this emitter.

	uint												m_nEnvFlags;			// Union of environment flags affecting emitters.
	ParticleTarget							m_Target;					// Target set from external source.

	AABB												m_bbWorld,				// World bbox.
															m_bbWorldDyn,			// Dynamically-computed bbox (normally just for debug).
															m_bbWorldEnv;			// Last-queried phys environ bbox.

	// Entity connection params.
	int													m_nEntityId;
	int													m_nEntitySlot;
	bool												m_bIndependent;
	bool												m_bRegistered;		// Registered in particle manager.
																								// Else orphaned, may be reactivated.
	bool												m_bSelected;			// Whether entity selected in editor.
	CTimeValue									m_timeLastRendered;

  AABB												m_WSBBox;
	SPhysEnviron								m_PhysEnviron;		// Common physical environment (uniform forces only) for emitter.
	SVisEnviron									m_VisEnviron;
};

// inline CParticleContainer implementations.
inline float CParticleContainer::GetAge() const
{
	return GetLoc().GetAge();
}

inline CParticleEmitter& CParticleContainer::GetMain() const
{ 
	return GetLoc().GetMain(); 
}

// inline CParticleSubEmitter implementations.
inline float CParticleSubEmitter::GetAge() const
{
	return GetLoc().GetAge();
}

inline CParticleEmitter& CParticleSubEmitter::GetMain() const
{ 
	return GetLoc().GetMain(); 
}

inline float CParticleSubEmitter::GetEndAge() const
{
	return min(m_fEndAge, GetLoc().GetStopAge());
}

//////////////////////////////////////////////////////////////////////////

// Specific particle object specialisations
Declare_GetMemoryUsage(CParticleEffect)
Declare_GetMemoryUsage(CParticleEmitter)
Declare_GetMemoryUsage(CParticleSubEmitter)
Declare_GetMemoryUsage(CParticleContainer)

//////////////////////////////////////////////////////////////////////////

#endif // __particleemitter_h__
