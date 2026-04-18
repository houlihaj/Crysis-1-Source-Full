////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   PhysCallbacks.h
//  Created:     14/11/2006 by Anton.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __PhysCallbacks_h__
#define __PhysCallbacks_h__
#pragma once

class CPhysCallbacks : public Cry3DEngineBase
{
public:
	static void Init();
	static void Done();

	static int OnFoliageTouched( const EventPhys *pEvent );
	static int OnPhysStateChange(const EventPhys *pEvent);
	static int OnPhysCollision(const EventPhys *pEvent);

	static bool TestCollisionWithRenderMesh( EventPhysCollision *pCollisionEvent, bool *pbPierceable=0 );
};

class CPhysStreamer : public IPhysicsStreamer 
{
public:
	virtual int CreatePhysicalEntity(void *pForeignData,int iForeignData,int iForeignFlags);
	virtual int DestroyPhysicalEntity(IPhysicalEntity *pent) { return 1; }
	virtual int CreatePhysicalEntitiesInBox(const Vec3 &boxMin, const Vec3 &boxMax);
	virtual int DestroyPhysicalEntitiesInBox(const Vec3 &boxMin, const Vec3 &boxMax);
};

#endif // __PhysCallbacks_h__