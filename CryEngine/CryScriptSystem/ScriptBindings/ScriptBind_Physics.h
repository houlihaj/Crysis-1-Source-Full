////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   ScriptBind_Physics.h
//  Version:     v1.00
//  Created:     22/10/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ScriptBind_Physics_h__
#define __ScriptBind_Physics_h__
#pragma once

#include <IScriptSystem.h>

struct ISystem;
struct I3DEngine;
struct IPhysicalWorld;

/*! 
<title Physics>
Syntax: Physics

This class implements script-functions for Physicss and decals.
REMARKS:
After initialization of the script-object it will be globally accessable through scripts using the namespace "Physics".

Example:
Physics.CreateDecal(pos, normal, scale, lifetime, decal.texture, decal.object, rotation);

IMPLEMENTATIONS NOTES:
These function will never be called from C-Code. They're script-exclusive.
*/
class CScriptBind_Physics : public CScriptableBase
{
public:
	CScriptBind_Physics(IScriptSystem *pScriptSystem, ISystem *pSystem);
	virtual ~CScriptBind_Physics();
	virtual void GetMemoryStatistics(ICrySizer *pSizer) { pSizer->Add(this); };

	// <title SimulateExplosion>
	// Syntax: Physics.SimulateExplosion( Table explosionParams )
	// Description:
	//    Simulate physical explosion.
	//    Does not apply any game related explosion damages, this function only apply physical forces.
	// Arguments:
	//    explosionParams - Table with explosion simulation parameters.
	//      <TABLE>
	//          Explosion Params                 Meaning
	//          -------------                    -----------
	//          pos                              Explosion epicenter position.
	//          radius                           Explosion radius.
	//          direction                        Explosion impulse direction.
	//          impulse_pos                      Explosion impulse position (Can be different from explosion epicenter).
	//          impulse_presure                  Explosion impulse presur at radius distance from epicenter.
	//          impulse_pos                      Explosion impulse position (Can be different from explosion epicenter).
	//          rmin                             Explosion minimal radius, at this radius full pressure is applied.
	//          rmax                             Explosion maximal radius, at this radius impulse pressure will be reaching zero.
	//          hole_size                        Size of the explosion hole to create in breakable objects.
	//       </TABLE>
	int SimulateExplosion(IFunctionHandler *pH,SmartScriptTable explosionParams );
	
	// <title RegisterExplosionShape>
	// Syntax: Physics.RegisterExplosionShape( const char *sGeometryFile,float fSize,int nIdMaterial,float fProbability )
	// Description:
	//    Register a new explosion shape from the static geometry.
	//    Does not apply any game related explosion damages, this function only apply physical forces.
	// Arguments:
	//    sGeometryFile - Static geometry file name (CGF).
	//    fSize - Scale for the static geometry.
	//    nMaterialId - ID of the breakable material to apply this shape on.
	//    fProbability - Preference ratio of using this shape then other registered shape.
	//		sSplintersFile - additional non-physicalized splinters cgf to place on cut surfaces
	//    fSplintersOffset - offset of the lower splinters piece wrt to the upper one
	//		sSplintersCloudEffect - particle effect when the splinters constraint breaks
	int RegisterExplosionShape(IFunctionHandler *pH,const char *sGeometryFile,float fSize,int nIdMaterial,float fProbability,
														 const char *sSplintersFile, float fSplintersOffset, const char *sSplintersCloudEffect);

	// <title RegisterExplosionCrack>
	// Syntax: Physics.RegisterExplosionCrack( const char *sGeometryFile,int nIdMaterial )
	// Description:
	//    Register a new crack for breakable object.
	// Arguments:
	//    sGeometryFile - Static geometry file name fro the crack (CGF).
	//    nMaterialId - ID of the breakable material to apply this crack on.
	int RegisterExplosionCrack(IFunctionHandler *pH,const char *sGeometryFile,int nIdMaterial );

	//bool ReadPhysicsTable(IScriptTable *pITable, PhysicsParams &sParamOut) {};

	// <title RayWorldIntersection>
	// Syntax: Physics.RayWorldIntersection( Vec3 vPos,Vec3 vDir,int nMaxHits,int iEntTypes, [optional] skipEntityId1, [optional] skipEntityId2 )
	// Description:
	//    Check if ray segment from src to dst intersect anything.
	// Arguments:
	//    vPos - Ray origin point.
	//    vDir - Ray direction.
	//    nMaxHits - Max number of hits to return, sorted in nearest to farest order.
	//    iEntTypes - Physical Entity types bitmask, ray will only intersect with entities specified by this mask (ent_all,...).
	//    skipEntityId1 - Entity id to skip when checking for intersection.
	//    skipEntityId2 - Entity id to skip when checking for intersection.
	int RayWorldIntersection(IFunctionHandler *pH);

	// <title RayTraceCheck>
	// Syntax: Physics.RayTraceCheck( Vec3 src,Vec3 dst,ScriptHandle skipEntityId1,ScriptHandle skipEntityId2 )
	// Description:
	//    Check if ray segment from src to dst intersect anything.
	// Arguments:
	//    src - Ray segment origin point.
	//    dst - Ray segment end point.
	//    skipEntityId1 - Entity id to skip when checking for intersection.
	//    skipEntityId2 - Entity id to skip when checking for intersection.
	int RayTraceCheck(IFunctionHandler *pH,Vec3 src,Vec3 dst,ScriptHandle skipEntityId1,ScriptHandle skipEntityId2 );

	// <title SamplePhysEnvironment>
	// Syntax: Physics.SamplePhysEnvironment( Vec3 pt, float r, int objtypes )
	// Description:
	//    Find physical entities touched by a sphere
	// Arguments:
	//    pt - sphere center
	//    r - radius
	//    objtypes - phys entity types (optional)
	int SamplePhysEnvironment(IFunctionHandler *pH);

private:
	ISystem *m_pSystem;
	I3DEngine *m_p3DEngine;
	IPhysicalWorld *m_pPhysicalWorld;
};

#endif // __ScriptBind_Physics_h__

