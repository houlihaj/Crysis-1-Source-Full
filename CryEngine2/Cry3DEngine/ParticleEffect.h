////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ParticleEffect.h
//  Version:     v1.00
//  Created:     10/7/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __particleeffect_h__
#define __particleeffect_h__
#pragma once

#include "I3DEngine.h"
#include "Cry3DEngineBase.h"
#include "ParticleParams.h"
#include "CryArray.h"

class CPartManager;

template<class T>
class PtrArray: public DynArray<T*>
{
public:
	inline T&	operator [](int i) const					{ return *this->begin()[i]; }
};

template<class T>
class SmartPtrArray: public DynArray< _smart_ptr<T> >
{
public:
	inline T&	operator [](int i) const					{ return *this->begin()[i]; }
};

//
// Additional runtime parameters.
//
struct ResourceParticleParams: ParticleParams, Cry3DEngineBase
{
	// Texture, material, geometry params.
	
	bool									bResourcesLoaded;					// Overall flag indicating resources loaded and up to date.
	INT_PTR								nTexId;										// Texture id for sprite.
	float									fTexFracX, fTexFracY;			// Size of texture tile UVs.
	float									fTexAspectX, fTexAspectY;	// Multipliers for non-square textures (max is always 1).
	_smart_ptr<IMaterial>	pMaterial;								// Used to override the material
	_smart_ptr<IStatObj>	pStatObj;									// If it isn't set to 0, this object will be used instead of a sprite
	int										iPhysMat;									// Surface material for physicalized particles
	uint									nEnvFlags;								// Summary of environment info needed for effect.

	ResourceParticleParams()
	{
		Init();
	}

	explicit ResourceParticleParams( const ParticleParams& params )
		: ParticleParams(params)
	{
		Init();
	}

	int LoadResources( const char* sName );
	void UnloadResources();
	void UpdateTextureAspect();

	void ComputeEnvironmentFlags();

	// Computation of static loose-fitting bounding box, if possible.
	bool NeedsDynamicBounds() const
	{
		return ePhysicsType >= ParticlePhysics_SimplePhysics;
	}
	void GetStaticBounds( AABB& bbResult, const QuatTS& loc, const Vec3& vSpawnSize, bool bWithSize, float fMaxLife, const Vec3& vGravity, const Vec3& vWind ) const;
	void GetTravelBounds( AABB& bbResult, float fTime, const QuatTS& loc, bool bOmniDir, const Vec3& vGravity, const Vec3& vWind ) const;
	bool IsImmortal() const
	{
		return bEnabled && ((bContinuous && fEmitterLifeTime.GetMaxValue() == 0.f) || fPulsePeriod.GetMaxValue() > 0.f || fParticleLifeTime.GetMaxValue() == 0.f);
	}
	bool HasEquilibrium() const
	{
		// Effect reaches a steady state: No emitter lifespawn or pulsing.
		return bContinuous 
			&& fPulsePeriod.GetMaxValue() == 0.f 
			&& fEmitterLifeTime.GetMaxValue() == 0.f
			&& fParticleLifeTime.GetMaxValue() > 0.f
			&& fCount.GetMaxValue() > 0.f;
	}
	float GetEquilibriumAge() const
	{
		return fSpawnDelay.GetMaxValue() + fParticleLifeTime.GetMaxValue();
	}
	float GetMaxParticleLife() const
	{
		return fParticleLifeTime.GetMaxValue() > 0.f ? fParticleLifeTime.GetMaxValue() : fEmitterLifeTime.GetMaxValue();
	}
	inline bool Has3DRotation() const
	{
		return eFacing != ParticleFacing_Camera || !sGeometry.empty() || (sTexture.empty() && sMaterial.empty());
	}
	inline bool IsRenderable() const
	{
		return ((nTexId > 0 && nTexId < MAX_VALID_TEXTURE_ID) || pMaterial != 0 || pStatObj != 0)
			&& fSize.GetMaxValue() > 0.f
			&& (eBlendType != ParticleBlendType_AlphaBased || fAlpha.GetMaxValue() > 0.f);
	}

	int GetSubGeometryCount() const
	{
		return pStatObj ? pStatObj->GetSubObjectCount() : 0;
	}
	IStatObj::SSubObject* GetSubGeometry(int i) const
	{
		assert(i < GetSubGeometryCount());
		IStatObj::SSubObject* pSub = pStatObj->GetSubObject(i);
		return pSub && pSub->nType==STATIC_SUB_OBJECT_MESH && pSub->pStatObj ? pSub : 0;
	}

protected:

	void Init()
	{
		bResourcesLoaded = false;
		nTexId = iPhysMat = 0;
		fTexFracX = fTexFracY = 1.f;
		fTexAspectX = fTexAspectY = 0.f;
		ComputeEnvironmentFlags();
	}

	float GetMaxSpeed(const Vec3& vGravity, const Vec3& vWind) const;
};

struct SParticleCounts
{
	float		ParticlesAlloc, ParticlesActive, ParticlesRendered;
	float		ParticlesReiterate, ParticlesOverflow, ParticlesReuse, ParticlesReject;
	float		ParticlesCollideTerrain, ParticlesCollideObjects, ParticlesClip;
	float		ScrFillProcessed, ScrFillRendered;
	float		EmittersAlloc, EmittersActive, EmittersRendered;

	SParticleCounts()
	{
		Clear();
	}
	void Clear()
	{
		memset(this, 0, sizeof(*this));
	}
	static int size()
	{
		return sizeof(SParticleCounts)/sizeof(float);
	}
	float& operator[] (int i)
	{
		assert(i < size());
		return ((float*)this)[i];
	}
	float operator[] (int i) const
	{
		assert(i < size());
		return ((float*)this)[i];
	}

	void Add( SParticleCounts const& p )
	{
		for (int i = 0; i < size(); i++)
			(*this)[i] += p[i];
	}
	void Blend( SParticleCounts const& p, float fThis, float fOther )
	{
		for (int i = 0; i < size(); i++)
			(*this)[i] = (*this)[i] * fThis + p[i] * fOther;
	}
};

/*!	CParticleEffect implements IParticleEffect interface and contain all components necessary to 
		to create the effect
 */
class CParticleEffect : public IParticleEffect, public Cry3DEngineBase
{
public:
	CParticleEffect();
	~CParticleEffect();

	void Release()
	{
		--m_nRefCounter;
		if (m_nRefCounter <= 0)
			delete this;
	}

	// IParticle interface.k
	virtual IParticleEmitter* Spawn( bool bIndependent, const Matrix34& mat );

	virtual void SetName( const char *sFullName );
	virtual const char* GetName() const		{ return m_strFullName.c_str(); }
	virtual const char* GetBaseName() const;

	virtual void SetEnabled( bool bEnabled );
	virtual bool IsEnabled() const { return m_particleParams.bEnabled; };

	//////////////////////////////////////////////////////////////////////////
	//! Load resources, required by this particle effect (Textures and geometry).
	bool LoadResources();
	void UnloadResources();

	//////////////////////////////////////////////////////////////////////////
	// Child particle systems.
	//////////////////////////////////////////////////////////////////////////
	int GetChildCount() const	 { return (int)m_childs.size(); }
	IParticleEffect* GetChild( int index ) const  { return &m_childs[index]; }

	virtual void AddChild( IParticleEffect *pEffect );
	virtual void RemoveChild( IParticleEffect *pEffect );
	virtual void ClearChilds();
	virtual void InsertChild( int slot,IParticleEffect *pEffect );
	virtual int FindChild( IParticleEffect *pEffect ) const;
	virtual IParticleEffect* GetParent() const { return m_parent; };

	//////////////////////////////////////////////////////////////////////////
	virtual void SetParticleParams( const ParticleParams &params );
	virtual const ParticleParams& GetParticleParams() const { return m_particleParams; };

	void Serialize( XmlNodeRef node, bool bLoading, bool bChildren );
	void GetMemoryUsage( ICrySizer* pSizer );
	
	// Further interface.

	ResourceParticleParams& GetResourceParams() { return m_particleParams; }

	bool IsImmortal() const;
	bool IsIndirect() const;

	SParticleCounts	m_statsCur;
	SParticleCounts	m_statsAll;

	void ClearStats();
	void SumStats();
	void PrintStats( bool bHeader = false );

private:

	//////////////////////////////////////////////////////////////////////////
	string													m_strFullName;
	ResourceParticleParams					m_particleParams;

	//! Parenting.
	CParticleEffect*								m_parent;
	SmartPtrArray<CParticleEffect>	m_childs;
};

//
// Utilities
//
inline float TravelDistance( float fV, float fDrag, float fT )
{
	if (fDrag == 0.f)
		// Without drag, it's much simpler.
		return fV*fT;

	// Compute distance traveled with drag.
	return fV / fDrag * (1.f - expf(-fDrag*fT));
}

inline void Travel( Vec3& vPos, Vec3& vVel, float fTime, Vec3 const& vAccel, Vec3 const& vWind, float fDrag )
{
	// Analytically compute new velocity and position, accurate for any time step.
	if (fDrag > 1e-6f)
	{
		//
		// Air resistance proportional to velocity is typical for slower laminar movement.
		// For drag d (units in 1/time), wind W, and gravity G:
		//
		//		V' = d (W-V) + G
		//
		//	The analytic solution is:
		//
		//		VT = G/d + W,									terminal velocity
		//		V = (V0-VT) e^(-d t) + VT
		//		X = (V0-VT) (1 - e^(-d t))/d + VT t
		//
		Vec3 vTerm = vWind + vAccel / fDrag;
		double fDecay = exp(-fDrag * fTime);
		float fT = (1.0 - fDecay) / fDrag;
		vPos += vVel * fT + vTerm * (fTime-fT);
		vVel = vVel * fDecay + vTerm * (1.0 - fDecay);
	}
	else
	{
		// Simple non-drag computation.
		vPos += vVel * fTime + vAccel * (fTime*fTime*0.5f);
		vVel += vAccel * fTime;
	}
}

#endif //__particleeffect_h__
