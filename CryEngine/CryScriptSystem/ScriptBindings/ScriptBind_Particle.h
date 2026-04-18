////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   ScriptBind_Particle.h
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ScriptBind_Particle_h__
#define __ScriptBind_Particle_h__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <IScriptSystem.h>

struct ISystem;
struct I3DEngine;
struct ParticleParams;
struct IParticleEffect;
struct CryEngineDecalInfo;

/*! 
	 <title Particle>
	 Syntax: Particle

	This class implements script-functions for particles and decals.
	REMARKS:
	After initialization of the script-object it will be globally accessable through scripts using the namespace "Particle".
	
	Example:
		Particle.CreateDecal(pos, normal, scale, lifetime, decal.texture, decal.object, rotation);

	IMPLEMENTATIONS NOTES:
	These function will never be called from C-Code. They're script-exclusive.
*/

class CScriptBind_Particle : public CScriptableBase
{
public:
	CScriptBind_Particle(IScriptSystem *pScriptSystem, ISystem *pSystem);
	virtual ~CScriptBind_Particle();
	virtual void GetMemoryStatistics(ICrySizer *pSizer) { pSizer->Add(this); };

	int CreateEffect(IFunctionHandler *pH, const char *name, SmartScriptTable params);
	int DeleteEffect(IFunctionHandler *pH, const char *name);
	int IsEffectAvailable(IFunctionHandler *pH, const char *name);
	int SpawnEffect(IFunctionHandler *pH, const char *effectName, Vec3 pos, Vec3 dir);
	int SpawnEffectLine(IFunctionHandler *pH, const char *effectName, Vec3 startPos, Vec3 endPos, Vec3 dir, float scale, int slices);
	int SpawnParticles(IFunctionHandler *pH, SmartScriptTable params, Vec3 pos, Vec3 dir);
	int CreateDecal(IFunctionHandler *pH, Vec3 pos, Vec3 normal, float size, float lifeTime, const char *textureName);
	int CreateMatDecal(IFunctionHandler *pH, Vec3 pos, Vec3 normal, float size, float lifeTime, const char *materialName);
	int Attach(IFunctionHandler * pH);
	int Detach(IFunctionHandler * pH);
  
private:
	void ReadParams(SmartScriptTable &table, ParticleParams *params, IParticleEffect *pEffect);
	void CreateDecalInternal( IFunctionHandler *pH, const Vec3& pos, const Vec3& normal, float size, float lifeTime, const char *name, bool nameIsMaterial );

	ISystem			*m_pSystem;
	I3DEngine		*m_p3DEngine;
};

#endif // __ScriptBind_Particle_h__
