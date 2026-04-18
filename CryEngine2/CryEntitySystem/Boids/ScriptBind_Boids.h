////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   scriptobjectboids.h
//  Version:     v1.00
//  Created:     17/5/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __scriptobjectboids_h__
#define __scriptobjectboids_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include <IScriptSystem.h>

// forward declarations.
class CFlock;
struct SBoidsCreateContext;

// <title Boids>
// Syntax: Boids
// Description:
//    Provides access to boids flock functionality.
class CScriptBind_Boids  : public CScriptableBase
{
public:
	CScriptBind_Boids( ISystem *pSystem );
	virtual ~CScriptBind_Boids(void);

	// <title CreateFlock>
	// Syntax: Boids.CreateFlock( entity,paramsTable )
	// Description:
	//    Creates a flock of boids and binds it to the given entity.
	// Arguments:
	//    entity - Valid entity table.
	//    nType - Type of the flock, can be Boids.FLOCK_BIRDS,Boids.FLOCK_FISH,Boids.FLOCK_BUGS.
	//    paramTable - Table with parameters for flock (Look at sample scripts).
	int CreateFlock( IFunctionHandler *pH, SmartScriptTable entity,int nType,SmartScriptTable paramTable );

	// <title CreateFishFlock>
	// Syntax: Boids.CreateFishFlock( entity,paramsTable )
	// Description:
	//    Creates a fishes flock and binds it to the given entity.
	// Arguments:
	//    entity - Valid entity table.
	//    paramTable - Table with parameters for flock (Look at sample scripts).
	int CreateFishFlock(IFunctionHandler *pH, SmartScriptTable entity,SmartScriptTable paramTable );

	// <title CreateBugsFlock>
	// Syntax: Boids.CreateBugsFlock( entity,paramsTable )
	// Description:
	//    Creates a bugs flock and binds it to the given entity.
	// Arguments:
	//    entity - Valid entity table.
	//    paramTable - Table with parameters for flock (Look at sample scripts).
	int CreateBugsFlock(IFunctionHandler *pH, SmartScriptTable entity,SmartScriptTable paramTable );

	// <title SetFlockParams>
	// Syntax: Boids.SetFlockParams( entity,paramsTable )
	// Description:
	//    Modifies parameters of the existing flock in the specified enity.
	// Arguments:
	//    entity - Valid entity table containing flock.
	//    paramTable - Table with parameters for flock (Look at sample scripts).
	int SetFlockParams(IFunctionHandler *pH, SmartScriptTable entity,SmartScriptTable paramTable );

	// <title EnableFlock>
	// Syntax: Boids.EnableFlock( entity,paramsTable )
	// Description:
	//    Enables/Disables flock in the entity.
	// Arguments:
	//    entity - Valid entity table containing flock.
	//    bEnable - true to enable or false to disable flock.
	int EnableFlock(IFunctionHandler *pH,SmartScriptTable entity,bool bEnable );

	// <title SetFlockPercentEnabled>
	// Syntax: Boids.SetFlockPercentEnabled( entity,paramsTable )
	// Description:
	//    Used to gradually enable flock.
	//    Depending on the percentage more or less boid objects will be rendered in flock.
	// Arguments:
	//    entity - Valid entity table containing flock.
	//    nPercent - In range 0 to 100, 0 mean no boids will be rendered,if 100 then all boids will be rendered.
	int SetFlockPercentEnabled(IFunctionHandler *pH,SmartScriptTable entity,int nPercent );

	int OnBoidHit( IFunctionHandler *pH,SmartScriptTable flockEntity,SmartScriptTable boidEntity,SmartScriptTable hit );

private:
	bool ReadParamsTable( IScriptTable *pTable, struct SBoidContext &bc,SBoidsCreateContext &ctx );	
	IEntity* GetEntity( IScriptTable *pEntityTable );
	CFlock* GetFlock( IScriptTable *pEntityTable );

	int CommonCreateFlock( int type,IFunctionHandler *pH,SmartScriptTable entity,SmartScriptTable paramTable );

	ISystem *m_pSystem;
	IScriptSystem *m_pScriptSystem;
};

#endif // __scriptobjectboids_h__