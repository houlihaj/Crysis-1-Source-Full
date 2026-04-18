////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   PhysicsEventListener.h
//  Version:     v1.00
//  Created:     18/8/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __PhysicsEventListener_h__
#define __PhysicsEventListener_h__
#pragma once

// forward declaration.
class  CEntitySystem;
struct IPhysicalWorld;

//////////////////////////////////////////////////////////////////////////
class CPhysicsEventListener
{
public:
	CPhysicsEventListener( CEntitySystem *pEntitySystem,IPhysicalWorld *pPhysics );
	~CPhysicsEventListener();

	static int OnBBoxOverlap( const EventPhys *pEvent );
	static int OnStateChange( const EventPhys *pEvent );
	static int OnPostStep( const EventPhys *pEvent );
	static int OnUpdateMesh( const EventPhys *pEvent );
	static int OnCreatePhysEntityPart( const EventPhys *pEvent );
	static int OnRemovePhysEntityParts( const EventPhys *pEvent );
	static int OnCollision( const EventPhys *pEvent );
	static int OnJointBreak( const EventPhys *pEvent );

private:
	static CEntity* GetEntity( void *pForeignData,int iForeignData );
	static CEntity* GetEntity( IPhysicalEntity *pPhysEntity );

	CEntitySystem *m_pEntitySystem;
	IPhysicalWorld *m_pPhysics;
};

#endif // __PhysicsEventListener_h__

