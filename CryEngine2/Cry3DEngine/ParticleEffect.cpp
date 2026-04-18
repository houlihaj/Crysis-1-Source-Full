////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   particleeffect.cpp
//  Version:     v1.00
//  Created:     10/7/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleEffect.h"
#include "ParticleEmitter.h"
#include "partman.h"
#include "3dEngine.h"
#include "ISound.h"
#include "CryTypeInfo.h"

//////////////////////////////////////////////////////////////////////////
// TypeInfo XML serialisation code

AUTO_TYPE_INFO_LOCAL(RectF)

static const int nSERIALIZE_VERSION = 18;
static const int nMIN_SERIALIZE_VERSION = 13;

enum ESerializeMode { SERIALIZE_READ, SERIALIZE_WRITE, SERIALIZE_WRITE_NONDEFAULT };

// Serialize a variable using a VarInfo.
void Serialize( IXmlNode& xml, void* data, void* def_data, const CTypeInfo::CVarInfo& var, ESerializeMode mode, int nVersion )
{
	assert(var.GetDim() == 1);

	if (&var.Type == &TypeInfo((void**)0))
		// Never serialize pointers.
		return;
	
	(char*&) data += var.Offset;
	const char* name = var.GetDisplayName();

	if (name[0] == '_')
		// Do not serialize private vars.
		return;

	if (mode == SERIALIZE_READ)
	{
		cstr value = xml.getAttr(name);
		var.Type.FromString(data, value, CTypeInfo::READ_SKIP_EMPTY);
	}
	else 
	{
		string val;
		if (mode == SERIALIZE_WRITE_NONDEFAULT)
		{
			val = var.Type.ToString(data, (CTypeInfo::WRITE_SKIP_DEFAULT | CTypeInfo::WRITE_TRUNCATE_SUB), def_data ? (char*)def_data + var.Offset : 0);
			if (val.length() == 0)
				return;
		}
		else
			val = var.Type.ToString(data, CTypeInfo::WRITE_TRUNCATE_SUB);
		xml.setAttr(name, val);
	}
}

// Serialize a value using a TypeInfo.
void Serialize( IXmlNode& xml, void* data, void* def_data, const CTypeInfo& info, ESerializeMode mode, int nVersion )
{
	for AllSubVars( pVar, info )
	{
		Serialize( xml, data, def_data, *pVar, mode, nVersion );
	}
}

// Serialize a struct to an XML node.
template<class T>
void Serialize( IXmlNode& xml, T& val, T& def_val, ESerializeMode mode, int nVersion  )
{
	Serialize( xml, &val, &def_val, TypeInfo(&val), mode, nVersion );
}

template<class T>
bool FromString(T* data, cstr str, int flags = 0)
{
	return TypeInfo(data).FromString(data, str, flags);
}

bool IsEnabled( IXmlNode const& node )
{
	for (int i = 0; i < node.getChildCount(); i++)
	{
		XmlNodeRef child = node.getChild(i);
		if (child->isTag("Params"))
		{
			bool bEnabled = true;
			child->getAttr("Enabled", bEnabled);
			if (bEnabled)
				return true;
		}
		else if (IsEnabled(*child))
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// ResourceParticleParams implementation

static const float fTRAVEL_SAFETY = 0.1f;
static const float fSIZE_SAFETY = 0.01f;

struct SEmitParams
{
	Vec3	vAxis;
	float	fCosMin, fCosMax;
	float fSpeedMin, fSpeedMax;
};

struct SForceParams
{
	Vec3		vAccel;
	Vec3		vWind;
	float		fDragMin, fDragMax;
};

void AddTravel( AABB& bb, Vec3 vVel, Vec3 const& vAccel, float fDrag, Vec3 const& vWind, float fTime )
{
	Vec3 vTravel(ZERO);
	Travel( vTravel, vVel, fTime, vAccel, vWind, fDrag );
	bb.Add( vTravel );
}

void AddTravel( AABB& bb, Vec3 const& vVel, SForceParams const& force, float fTime )
{
	// Add end point.
	float fDrag = vVel*force.vWind > 0.f ? force.fDragMax : force.fDragMin;
	AddTravel( bb, vVel, force.vAccel, fDrag, force.vWind, fTime );

	// Find time of min/max vals of each component, by solving for v.x etc = 0
	for (int i = 0; i < 3; i++)
	{
		float fT = 0.f;

		if (fDrag != 0)
		{
			// VT = G/d + W
			// Vt = 0 = (V-VT) * e^(-d t) + VT
			// e^(-d t) = -VT/(V-VT)
			// t = -log(VT/(VT-V)) / d
			float fVT = force.vAccel[i] / fDrag + force.vWind[i];
			if (fVT != vVel[i])
			{
				float f = fVT / (fVT - vVel[i]);
				if (f > 0.f && f < 1.f)
					fT = -logf(f);
			}
		}
		else if (force.vAccel[i] != 0.f)
		{
			// Vt = 0 = V + G t
			// t = -V/G
			fT = -vVel[i] / force.vAccel[i];
		}

		if (fT > 0.f && fT < fTime)
			AddTravel( bb, vVel, force.vAccel, fDrag, force.vWind, fT );
	}
}

Vec3 GetExtremeEmitVec( Vec3 const& vRefDir, SEmitParams const& emit )
{
	float fEmitCos = vRefDir * emit.vAxis;
	if (fEmitCos >= emit.fCosMin && fEmitCos <= emit.fCosMax)
	{
		// Emit dir is in the cone.
		return vRefDir * emit.fSpeedMax;
	}
	else
	{
		// Find dir in emission cone closest to ref dir.
		Vec3 vEmitPerp = vRefDir - emit.vAxis*fEmitCos;
		vEmitPerp.Normalize();
		float fCos = clamp_tpl(fEmitCos, emit.fCosMin, emit.fCosMax);
		Vec3 vEmitMax = emit.vAxis*fCos + vEmitPerp*sqrt(1.f - fCos*fCos);
		vEmitMax *= (vEmitMax*vRefDir > 0.f ? emit.fSpeedMax : emit.fSpeedMin);
		return vEmitMax;
	}
}

void AddEmitDirs( AABB& bb, Vec3 const& vRefDir, SEmitParams const& emit, SForceParams const& force, float fTime )
{
	Vec3 vEmit = GetExtremeEmitVec( vRefDir, emit );
	AddTravel( bb, vEmit, force, fTime );
	vEmit = GetExtremeEmitVec( -vRefDir, emit );
	AddTravel( bb, vEmit, force, fTime );
}

inline float GetSinMax(float fCosMin, float fCosMax)
{
	return fCosMin*fCosMax < 0.f ? 1.f : sqrt(1.f - min(fCosMin*fCosMin, fCosMax*fCosMax));
}

inline float MaxComponent(Vec3 const& v)
{
	return max(max(abs(v.x), abs(v.y)), abs(v.z));
}

// Compute bounds of a cone of emission, with applied gravity.
AABB TravelBB( SEmitParams const& emit, SForceParams const& force, float fTime )
{
	AABB bb(Vec3(0.f), Vec3(0.f));

	// First expand box from emission in cardinal directions.
	AddEmitDirs( bb, Vec3(1,0,0), emit, force, fTime );
	AddEmitDirs( bb, Vec3(0,1,0), emit, force, fTime );
	AddEmitDirs( bb, Vec3(0,0,1), emit, force, fTime );

	// Add extreme dirs along gravity.
	if (!force.vAccel.IsZero())
	{
		Vec3 vDir = force.vAccel.GetNormalized();
		if (MaxComponent(vDir) < 0.999f)
			AddEmitDirs( bb, vDir, emit, force, fTime );
	}

	// And wind.
	if (force.fDragMax > 0.f && !force.vWind.IsZero())
	{
		Vec3 vDir = force.vWind.GetNormalized();
		if (MaxComponent(vDir) < 0.999f)
			AddEmitDirs( bb, vDir, emit, force, fTime );
	}

	// Expand by a safety factor.
	bb.min *= (1.f+fTRAVEL_SAFETY);
	bb.max *= (1.f+fTRAVEL_SAFETY);

	return bb;
}

void ResourceParticleParams::GetStaticBounds( AABB& bb, const QuatTS& loc, const Vec3& vSpawnSize, bool bWithSize, float fMaxLife, const Vec3& vGravity, const Vec3& vWind ) const
{
	fMaxLife = min(fMaxLife, GetMaxParticleLife());

	bb.Reset(); 

	if (bBindToEmitter)
	{
		bb.Add(loc * vPositionOffset);
	}
	else if (bSpaceLoop)
	{
		if (fCameraMaxDistance > 0.f)
		{
			bb.Add(loc.t, fCameraMaxDistance*loc.s);
		}
		else
		{
			bb.Add(vPositionOffset-vRandomOffset);
			bb.Add(vPositionOffset+vRandomOffset);
			bb.SetTransformedAABB( Matrix34(loc), bb );
		}
	}
	else
	{
		// Compute spawn source box.
		bb.Add(vPositionOffset-vRandomOffset, fPosRandomOffset);
		bb.Add(vPositionOffset+vRandomOffset, fPosRandomOffset);

		if (!vSpawnSize.IsZero())
		{
			// Expand spawn geometry by emitter geom, and emit in all directions.
			float fRadius = max(bb.min.GetLength(), bb.max.GetLength());
			Vec3 vSize = vSpawnSize + Vec3(fRadius);
			bb = AABB(-vSize, vSize);
		}

		bb.SetTransformedAABB( Matrix34(loc), bb );

		if (eFacing == ParticleFacing_Water)
			// Add slack for positioning emitter above water.
			bb.min.z -= 5.f;

		AABB bbTrav;
		GetTravelBounds( bbTrav, fMaxLife, loc, !vSpawnSize.IsZero(), vGravity, vWind );
		bb.Augment(bbTrav);
	}

	// Particle size.
	if (bWithSize)
	{
		float fMaxSize = fSize.GetMaxValue();
		if (pStatObj)
		{
			float fRadius = pStatObj->GetRadius();
			if (bNoOffset || ePhysicsType == ParticlePhysics_RigidBody)
				// Add object origin.
				fRadius += pStatObj->GetAABB().GetCenter().GetLength();
			fMaxSize *= fRadius;
		}
		else
		{
			if (fStretch.GetMaxValue() > 0.f)
				// Max stretch based on max speed.
				fMaxSize += (fStretch.GetMaxValue() + abs(fStretchOffsetRatio)) * GetMaxSpeed(vGravity, vWind);
			// Radius is size * sqrt(2), round it up slightly.
			fMaxSize *= 1.415f;
		}
		fMaxSize *= loc.s * (1.f + fSIZE_SAFETY);
		bb.Expand( Vec3(fMaxSize) );
	}
}

void ResourceParticleParams::GetTravelBounds( AABB& bbResult, float fTime, const QuatTS& loc, bool bOmniDir, const Vec3& vGravity, const Vec3& vWind ) const
{
	if (fTime <= 0.f)
	{
		bbResult = AABB(Vec3(0), Vec3(0));
		return;
	}

	// Emission direction.
	float fCosMin, fCosMax;
	float fSpeedMul = loc.s;

	if (bOmniDir)
	{
		fCosMin = -1.f;
		fCosMax = 1.f;
	}
	else
	{
		// Max sin and cos of emission relative to emit dir.
		fCosMin = cos(DEG2RAD( fEmitAngle.GetMaxValue() + fFocusAngle.GetMaxValue() ));
		fCosMax = cos(DEG2RAD( fEmitAngle.GetMinValue() + fFocusAngle.GetMinValue() ));
	}
	if (eFacing == ParticleFacing_Horizontal)
	{
		fSpeedMul *= GetSinMax(fCosMin, fCosMax);
		fCosMin = fCosMax = 0.f;
	}

	Vec3 vAccel = vGravity * fGravityScale.GetMaxValue() + vAcceleration * loc.s;
	Vec3 vEmitAxis = bFocusGravityDir ? -vGravity.GetNormalizedSafe(Vec3(0,0,-1)) : loc.q * Vec3(0,1,0);

	SEmitParams emit = { vEmitAxis, fCosMin, fCosMax, fSpeed.GetMinValue()*fSpeedMul, fSpeed.GetMaxValue()*fSpeedMul };
	SForceParams force = { vAccel, vWind, fAirResistance.GetMinValue(), fAirResistance.GetMaxValue() };

	bbResult = TravelBB( emit, force, fTime );

	if (fTurbulence3DSpeed.GetMaxValue())
	{
		// Expansion from 3D turbulence.
		float fAccel = fTurbulence3DSpeed.GetMaxValue() * isqrt_tpl(fTime);

		Vec3 vTrav(ZERO), vVel(ZERO);
		Travel( vTrav, vVel, fTime, Vec3(fAccel), Vec3(ZERO), fAirResistance.GetMinValue() );
		bbResult.Expand( vTrav );
	}

	// Expansion from spiral turbulence.
	if (fTurbulenceSpeed.GetMaxValue())
	{
		Vec3 vExp;
		vExp.x = vExp.y = fTurbulenceSize.GetMaxValue() * loc.s;
		vExp.z = 0.f;
		bbResult.Expand( vExp );
	}
}

void ResourceParticleParams::ComputeEnvironmentFlags()
{
	// Needs updated environ if particles interact with gravity or air.
	nEnvFlags = 0;

	if (tVisibleUnderwater != Trinary_Both || eFacing == ParticleFacing_Water)
		nEnvFlags |= ENV_WATER;
	if (fAirResistance.GetMaxValue() != 0.f)
		nEnvFlags |= ENV_WIND;
	if (fGravityScale.GetMaxValue() != 0.f || bFocusGravityDir)
		nEnvFlags |= ENV_GRAVITY;
	if (eForceGeneration != ParticleForce_None)
		nEnvFlags |= ENV_FORCE;
	if (ePhysicsType == ParticlePhysics_SimpleCollision)
	{
		if (bCollideTerrain)
			nEnvFlags |= ENV_TERRAIN;
		if (bCollideStaticObjects)
			nEnvFlags |= ENV_STATIC_ENT;
		if (bCollideDynamicObjects)
			nEnvFlags |= ENV_DYNAMIC_ENT;
	}
	if (bReceiveShadows)
		nEnvFlags |= ENV_SHADOWS;
	if (bCastShadows)
		nEnvFlags |= ENV_CAST_SHADOWS;
}

float ResourceParticleParams::GetMaxSpeed(const Vec3& vGravity, const Vec3& vWind) const
{
	Vec3 vAccel = vAcceleration + vGravity * fGravityScale.GetMaxValue();
	float fAccel = vAccel.GetLength();

	float fDrag = fAirResistance.GetMinValue();
	if (fDrag > 0.f)
	{
		float fWind = vWind.GetLength();
		float fTerm = fWind + fAccel / fDrag;
		return max( fSpeed.GetMaxValue(), fTerm );
	}
	else
	{
		// Simple non-drag computation.
		return fSpeed.GetMaxValue() + fAccel * fParticleLifeTime.GetMaxValue();
	}
}

//////////////////////////////////////////////////////////////////////////
int ResourceParticleParams::LoadResources( const char* sName )
{
	// Load only what is not yet loaded. Check everything, but caller may check params.bResourcesLoaded first.
	// Call UnloadResources to force unload/reload.

	if (!bEnabled)
		return 0;

	ComputeEnvironmentFlags();

	if (bResourcesLoaded || gEnv->pSystem->IsDedicated())
		return 0;

	int nLoaded = 0;

	// Load material.
	if (!pMaterial && !nTexId && !sMaterial.empty())
	{
		pMaterial = Get3DEngine()->GetMaterialManager()->LoadMaterial( sMaterial );
		if (!pMaterial || pMaterial == Get3DEngine()->GetMaterialManager()->GetDefaultMaterial())
		{
			CryWarning(VALIDATOR_MODULE_3DENGINE,VALIDATOR_WARNING,"Particle effect material not found: %s (%s)", sMaterial.c_str(), sName );

			// Load error texture for artist debugging.
			pMaterial = 0;
			nTexId = GetRenderer()->EF_LoadTexture("!Error", 0, eTT_2D)->GetTextureID();
		}
		else
			nLoaded++;
	}

	// Load texture.
	if (!nTexId && !sTexture.empty())
	{
#if !defined(NULL_RENDERER)
		nTexId = GetRenderer()->EF_LoadTexture(sTexture, 0, eTT_2D)->GetTextureID();
		if (!nTexId)
			CryLog( "Particle effect texture not found: %s (%s)", sTexture.c_str(), sName );
		else
			nLoaded++;
#endif
	}

	if (!TextureTiling.IsValid())
		CryLog( "Particle effect texture tile counts invalid (%s)", sName );

	// Set aspect ratio.
	if (fTexAspectX * fTexAspectY == 0.f)
		UpdateTextureAspect();

	// Load geometry.
	if (!pStatObj && !sGeometry.empty())
	{
		pStatObj = Get3DEngine()->LoadStatObj( sGeometry );
		if (!pStatObj)
			CryLog( "Particle effect geometry not found: %s (%s)", sGeometry.c_str(), sName );
		else
			nLoaded++;
	}

	// Find material type.
	if (!iPhysMat && !sSurfaceType.empty())
	{
		ISurfaceType *pSurface = Get3DEngine()->GetMaterialManager()->GetSurfaceTypeByName( sSurfaceType );
		if (pSurface)
			iPhysMat = pSurface->GetId();
	}

	// Process sound
	if (!sSound.empty() && gEnv->pSoundSystem)
	{
		// TODO needs to be properly replaced by the dependency tracker
		gEnv->pSoundSystem->Precache(sSound.c_str(), 0, 0);
	}

	bResourcesLoaded = true;
	return nLoaded;
}

//////////////////////////////////////////////////////////////////////////
void ResourceParticleParams::UpdateTextureAspect()
{
	fTexFracX = 1.f / TextureTiling.nTilesX;
	fTexFracY = 1.f / TextureTiling.nTilesY;
	fTexAspectX = fTexAspectY = 1.f;
	ITexture* pTexture = GetRenderer()->EF_GetTextureByID( nTexId );
	if (pTexture)
	{
		float fWidth = pTexture->GetWidth() * fTexFracX;
		float fHeight = pTexture->GetHeight() * fTexFracY;
		if (fWidth < fHeight)
			fTexAspectX = fWidth / fHeight;
		else if (fHeight < fWidth)
			fTexAspectY = fHeight / fWidth;
		else if (fWidth+fHeight == 0.f)
			fTexAspectX = fTexAspectY = 0.f;
	}
}

//////////////////////////////////////////////////////////////////////////
void ResourceParticleParams::UnloadResources()
{
// To do: Handle materials
	if (nTexId != 0)
	{
		GetRenderer()->RemoveTexture( nTexId );
		nTexId = 0;
	}
	pStatObj = 0;
	pMaterial = 0;
	iPhysMat = 0;
	fTexAspectX = fTexAspectY = 0.f;
	bResourcesLoaded = false;
}

//////////////////////////////////////////////////////////////////////////
// ParticleEffect implementation

//////////////////////////////////////////////////////////////////////////
CParticleEffect::CParticleEffect() : m_parent(0)
{
}

//////////////////////////////////////////////////////////////////////////
CParticleEffect::~CParticleEffect()
{
	UnloadResources();
}

void CParticleEffect::SetEnabled( bool bEnabled )
{
	m_particleParams.bEnabled = bEnabled;
	m_pPartManager->UpdateEmitters(this);
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::SetName( const char *sFullName )
{
	string strFullName;

	if (!m_parent)
	{
		// Top-level effect. Should be fully qualified with library and group names.
		strFullName = sFullName;
	}
	else
	{
		// Child effect. Use only final component.
		const char* sBaseName = strrchr(sFullName, '.');
		sBaseName = sBaseName ? sBaseName+1 : sFullName;

		// Qualify name by parents.
		strFullName = m_parent->GetName();
		if (strFullName.length() > 0)
			strFullName += ".";

		// Ensure unique name.
		int nBaseOffset = strFullName.length();

		string sNewBase;
		for_array (i, m_parent->m_childs)
		{
			if (&m_parent->m_childs[i] != this
			&& strcmp(m_parent->m_childs[i].GetName()+nBaseOffset, sBaseName) == 0)
			{
				// Extract and replace number.
				const char* p = sBaseName+strlen(sBaseName); 
				while (p > sBaseName && (p[-1] >= '0' && p[-1] <= '9'))
					p--;
				int nIndex = atoi(p);
				sNewBase = string(sBaseName, p);
				sNewBase.append(ToString(nIndex+1, 0));
				sBaseName = sNewBase;
				i = -1;
			}
		}

	 	strFullName += sBaseName;
	}

	if (strFullName != m_strFullName)
	{
		if (!m_parent)
			m_pPartManager->RenameEffect( this, strFullName );
		m_strFullName = strFullName;
	}
}

const char* CParticleEffect::GetBaseName() const
{
	int nBase = m_parent ? m_strFullName.rfind('.') 	// For child effects, return only last component.
		: m_strFullName.find('.');											// For top effects, return everything after lib name.
	if (nBase == string::npos)
		nBase = 0;
	else
		nBase++;
	return &m_strFullName[nBase];
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::AddChild( IParticleEffect *pEffect )	
{
	assert( pEffect );
	CParticleEffect* pEff = (CParticleEffect*) pEffect;
	m_childs.push_back(pEff);
	pEff->m_parent = this;
	//AddRef();
	m_pPartManager->UpdateEmitters(this);
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::RemoveChild( IParticleEffect *pEffect )
{
	assert( pEffect );
	stl::find_and_erase( m_childs,pEffect );
	m_pPartManager->UpdateEmitters(this);
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::ClearChilds()
{
	m_childs.clear();
	m_pPartManager->UpdateEmitters(this);
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::InsertChild( int slot, IParticleEffect *pEffect )
{
	if (slot < 0)
		slot = 0;
	if (slot > (int)m_childs.size())
		slot = (int)m_childs.size();

	assert( pEffect );
	CParticleEffect* pEff = (CParticleEffect*) pEffect;
	pEff->m_parent = this;
//	AddRef();
	m_childs.insert( m_childs.begin() + slot, pEff );
	m_pPartManager->UpdateEmitters(this);
}

//////////////////////////////////////////////////////////////////////////
int CParticleEffect::FindChild( IParticleEffect *pEffect ) const
{
	for_array (i, m_childs)
	{
		if (&m_childs[i] == pEffect)
			return i;
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
bool CParticleEffect::LoadResources()
{
	int nLoaded = 0;
	if (!m_particleParams.bResourcesLoaded && m_particleParams.bEnabled)
		nLoaded = m_particleParams.LoadResources(GetName());
	for_array (i, m_childs)
		nLoaded += (int)m_childs[i].LoadResources();
	return nLoaded > 0;
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::UnloadResources()
{
	m_particleParams.UnloadResources();
	for_all (m_childs).UnloadResources();
}

void CParticleEffect::SetParticleParams( const ParticleParams &params )
{
	if (params.sTexture != m_particleParams.sTexture || params.sMaterial != m_particleParams.sMaterial)
	{
		if (m_particleParams.nTexId != 0)
		{
			GetRenderer()->RemoveTexture( m_particleParams.nTexId );
			m_particleParams.nTexId = 0;
			m_particleParams.fTexAspectX = m_particleParams.fTexAspectY = 0.f;
		}
		m_particleParams.pMaterial = 0;
		m_particleParams.bResourcesLoaded = false;
	}
	if (params.sGeometry != m_particleParams.sGeometry)
	{
		m_particleParams.pStatObj = 0;
		m_particleParams.bResourcesLoaded = false;
	}
	if (params.sSurfaceType != m_particleParams.sSurfaceType)
	{
		m_particleParams.iPhysMat = 0;
		m_particleParams.bResourcesLoaded = false;
	}
	if (params.TextureTiling.nTilesX != m_particleParams.TextureTiling.nTilesX
	|| params.TextureTiling.nTilesY != m_particleParams.TextureTiling.nTilesY)
	{
		m_particleParams.fTexAspectX = m_particleParams.fTexAspectY = 0.f;
		m_particleParams.bResourcesLoaded = false;
	}

	static_cast<ParticleParams&>(m_particleParams) = params;
	m_particleParams.LoadResources(GetName());
	m_pPartManager->UpdateEmitters(this);
}

//////////////////////////////////////////////////////////////////////////
IParticleEmitter* CParticleEffect::Spawn( bool bIndependent, const Matrix34& loc )
{
	return m_pPartManager->CreateEmitter( bIndependent, loc, this );
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::Serialize( XmlNodeRef node,bool bLoading,bool bChilds )
{
	XmlNodeRef root = node;
	while (root->getParent())
		root = root->getParent();

	if (bLoading)
	{
		if (m_parent)
			// Set simple name, will be automatically qualified with hierarchy.
			SetName( node->getAttr("Name") );
		else
		{
			// Qualify with library name.
			string sFullName;
			string sRootTag = root->getTag();
			if (sRootTag == "ParticleLibrary" || sRootTag == "LevelLibrary")
				sFullName = root->getAttr("Name");
			if (sFullName.length() > 0)
				sFullName += ".";
			sFullName += node->getAttr("Name");
			SetName(sFullName);
		}

		int nVersion = 0;
		root->getAttr( "ParticleVersion", nVersion );
		if (nVersion < nMIN_SERIALIZE_VERSION || nVersion > nSERIALIZE_VERSION)
		{
			Warning( "Particle Effect %s not loaded: version (%d) out of supported range (%d-%d)",
				GetName(), nVersion, nMIN_SERIALIZE_VERSION, nSERIALIZE_VERSION );
			m_particleParams.bEnabled = false;
			return;
		}

		for (int i = 0; i < node->getChildCount(); i++)
		{
			XmlNodeRef paramsNode = node->getChild(i);
			if (!paramsNode->isTag("Params"))
				continue;

			// Init params, then read from XML.
			new(&m_particleParams) ResourceParticleParams();
			ParticleParams &params = m_particleParams;
			::Serialize( *paramsNode, params, params, SERIALIZE_READ, nVersion );

			CTypeInfo const& info = TypeInfo(&params);
			if (nVersion < 18)
			{
				// Correct bad random number range.
				for AllSubVars( pVar, info )
				{
					if (pVar->Type.IsType< TVarParam<float> >()
					|| pVar->Type.IsType< TVarEParam<float> >()
					|| pVar->Type.IsType< TVarEPParam<float> >())
					{
						TVarParam<float>& var = *(TVarParam<float>*)pVar->GetAddress(&params);
						float fBase = var.GetBase(), fRand = var.GetRandom();
						if (fRand < 0.f)
						{
							var.SetBase(fBase * (1.f - fRand));
							var.SetRandomVar(fRand / (fRand-1.f));
						}
						else if (fRand > 1.f)
						{
							var.SetRandomVar(1.f);
						}
						else
							continue;

						CryLog( "Invalid random var found: %s %s: %g (%g) changed to %g (%g)",
							GetName(), pVar->Name, fBase, fRand, var.GetBase(), var.GetRandom() );
					}
				}
			}

			if (nVersion < 14)
			{
				// Texture tile conversion.
				cstr sRect = paramsNode->getAttr("TextureUVRect");
				if (*sRect)
				{
					RectF rect;
					TypeInfo(&rect).FromString(&rect, sRect, CTypeInfo::READ_SKIP_EMPTY);
					params.TextureTiling.nTilesX = rect.w > 0.f ? int_round(1.f / rect.w) : 1;
					params.TextureTiling.nTilesY = rect.h > 0.f ? int_round(1.f / rect.h) : 1;
					params.TextureTiling.nFirstTile = rect.w*rect.h > 0.f ? int_round((rect.x + rect.y / rect.h) / rect.w) : 0;
				}
				
				paramsNode->getAttr( "TextureVariantCount", params.TextureTiling.nVariantCount );
				paramsNode->getAttr( "TexAnimFramesCount", params.TextureTiling.nAnimFramesCount );
				paramsNode->getAttr( "AnimFramerate", params.TextureTiling.fAnimFramerate );
				FromString(&params.TextureTiling.bAnimCycle, node->getAttr("AnimCycle"));

				if (params.bEnabled && !params.TextureTiling.IsValid())
					CryLog("Particle Effect %s: texture tile counts invalid", GetName());
			}
			if (nVersion < 15)
			{
				FromString(&params.bSpawnOnParentCollision, paramsNode->getAttr( "SpawnOnParentDeath"), CTypeInfo::READ_SKIP_EMPTY);
				if (string(paramsNode->getAttr("PhysicsType")) == "FullCollision")
					params.ePhysicsType = ParticlePhysics_SimpleCollision;
				else if (params.ePhysicsType == ParticlePhysics_SimpleCollision)
					params.bCollideTerrain = true;
			}
			if (nVersion < 16)
			{
				if (string(paramsNode->getAttr("OnlyUnderWater")) == "true")
					params.tVisibleUnderwater = Trinary_If_True;
				else if (string(paramsNode->getAttr("OnlyAboveWater")) == "true")
					params.tVisibleUnderwater = Trinary_If_False;
				if (string(paramsNode->getAttr("OnlyOutdoors")) == "true")
					params.tVisibleIndoors = Trinary_If_False;
			}
			if (nVersion < 17)
			{
				params.TextureTiling.Correct();
				if (params.ePhysicsType >= ParticlePhysics_SimpleCollision)
				{
					// Apply previous version's defaults.
					params.bCollideTerrain = params.bCollideStaticObjects = true;
					paramsNode->getAttr("CollideTerrain", params.bCollideTerrain);
					paramsNode->getAttr("CollideObjects", params.bCollideStaticObjects);
					params.bCollideDynamicObjects = params.bCollideStaticObjects; 
				}
			}

			if (params.bEnabled && params.sSound.Right(4) == ".wav")
			{
				CryLog("Particle Effect %s uses legacy wav sound %s, legacy params ignored", GetName(), params.sSound.c_str());
			}

			if (params.bSpaceLoop && params.fCameraMaxDistance == 0.f && params.vRandomOffset.GetVolume() == 0.f)
			{
				Warning( "Particle Effect %s has zero space loop volume: disabled", GetName() );
				m_particleParams.bEnabled = false;
			}

			if (!params.sSound.empty() && gEnv->pSoundSystem)
			{
				gEnv->pSoundSystem->Precache(params.sSound.c_str(), 0, 0);
			}
		}

		if (bChilds)
		{
			// Serialize childs.
			XmlNodeRef childsNode = node->findChild( "Childs" );
			if (childsNode)
			{
				for (int i = 0; i < childsNode->getChildCount(); i++)
				{
					XmlNodeRef xchild = childsNode->getChild(i);
					if (!GetSystem()->IsEditor() && !::IsEnabled(*xchild))
						// Skip disabled leaf effects.
						continue;
					IParticleEffect* pChildEffect = new CParticleEffect();
					AddChild( pChildEffect );
					pChildEffect->Serialize( xchild,bLoading,bChilds );
				}
			}
		}
	}
	else
	{
		// Saving.
		node->setAttr( "Name", GetBaseName() );
		root->setAttr( "ParticleVersion", nSERIALIZE_VERSION );

		// Save particle params.
		XmlNodeRef paramsNode = node->newChild( "Params" );

		ParticleParams paramsDef;
		::Serialize( *paramsNode, static_cast<ParticleParams&>(m_particleParams), paramsDef, SERIALIZE_WRITE_NONDEFAULT, nSERIALIZE_VERSION );

		if (bChilds && !m_childs.empty())
		{
			// Serialize childs.
			XmlNodeRef childsNode = node->newChild( "Childs" );
			for_array (i, m_childs)
			{
				XmlNodeRef xchild = childsNode->newChild( "Particles" );
				m_childs[i].Serialize( xchild,bLoading,bChilds );
			}
		}
	}
}

void CParticleEffect::GetMemoryUsage(ICrySizer* pSizer) 
{
	pSizer->AddString(m_strFullName);
	pSizer->AddObject(&m_particleParams, ::TypeInfo(&m_particleParams).GetMemoryUsage(pSizer, &m_particleParams));
	AddPtrContainer(pSizer, m_childs);
}

bool CParticleEffect::IsImmortal() const
{
	if (m_particleParams.IsImmortal())
		return true;
	for_array (i, m_childs)
	{
		if (!m_childs[i].GetParticleParams().bSecondGeneration && m_childs[i].IsImmortal())
			return true;
	}
	return false;
}

bool CParticleEffect::IsIndirect() const
{
	if (m_parent)
	{
		for (CParticleEffect const* pEffect = this; pEffect; pEffect = pEffect->m_parent)
			if (pEffect->m_particleParams.bSecondGeneration)
				return true;
	}
	return false;
}

void CParticleEffect::ClearStats()
{
	m_statsAll.Clear();
	m_statsCur.Clear();

	for_all (m_childs).ClearStats();
}

void CParticleEffect::SumStats()
{
	m_statsAll = m_statsCur;

	for_array (i, m_childs)
	{
		m_childs[i].SumStats();
		m_statsAll.Blend(m_childs[i].m_statsAll, 1.f, 1.f);
	}
}

void CParticleEffect::PrintStats( bool bHeader )
{
	if (bHeader)
		CryLogAlways(
			"Effect, "
			"Ems ren, Ems act, Ems all, "
			"Parts ren, Parts act, Parts all, "
			"Coll trr, Coll obj, Clip, "
			"Reiter, Ovf, Reuse, Reject"
		);
	if (m_statsAll.EmittersAlloc || m_statsAll.ParticlesAlloc)
	{
		int iLevel = 0;
		for (CParticleEffect* p = m_parent; p; p = p->m_parent)
			iLevel++;
		if (iLevel == 0 || m_statsCur.EmittersAlloc || m_statsCur.ParticlesAlloc)
		{
			CryLogAlways(
				"%*s%s, "
				"%.0f, %.0f, %.0f, "
				"%.0f, %.0f, %.0f, "
				"%.2f, %.2f, %.2f, "
				"%.0f, %.2f, %.2f, %.2f",
				iLevel, "", GetName(), 
				m_statsCur.EmittersRendered, m_statsCur.EmittersActive, m_statsCur.EmittersAlloc,
				m_statsCur.ParticlesRendered, m_statsCur.ParticlesActive, m_statsCur.ParticlesAlloc, 
				m_statsCur.ParticlesCollideTerrain, m_statsCur.ParticlesCollideObjects, m_statsCur.ParticlesClip,
				m_statsCur.ParticlesReiterate, m_statsCur.ParticlesOverflow, m_statsCur.ParticlesReuse, m_statsCur.ParticlesReject);
		}
		for_all (m_childs).PrintStats();
	}
}
