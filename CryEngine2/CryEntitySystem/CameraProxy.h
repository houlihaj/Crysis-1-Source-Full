////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   CameraProxy.h
//  Version:     v1.00
//  Created:     5/12/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CameraProxy_h__
#define __CameraProxy_h__
#pragma once

#include "Entity.h"
#include "EntitySystem.h"

struct SProximityElement;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles sounds in the entity.
//////////////////////////////////////////////////////////////////////////
class CCameraProxy : public IEntityCameraProxy
{
public:
	CCameraProxy( CEntity *pEntity );

	CEntity* GetEntity() const { return m_pEntity; };

	//////////////////////////////////////////////////////////////////////////
	// IEntityProxy interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetType() { return ENTITY_PROXY_CAMERA; }
	virtual void Release();
	virtual void Done() {};
	virtual	void Update( SEntityUpdateContext &ctx ) {};
	virtual	void ProcessEvent( SEntityEvent &event );
	virtual void Init( IEntity *pEntity,SEntitySpawnParams &params );
	virtual void SerializeXML( XmlNodeRef &entityNode,bool bLoading ) {};
	virtual void Serialize( TSerialize ser );
	virtual bool NeedSerialize() { return false; };
	//////////////////////////////////////////////////////////////////////////

	virtual void SetCamera( CCamera &cam );
	virtual CCamera& GetCamera() { return m_camera; };
	//////////////////////////////////////////////////////////////////////////

	void UpdateMaterialCamera();
private:
	//////////////////////////////////////////////////////////////////////////
	// Private member variables.
	//////////////////////////////////////////////////////////////////////////
	// Host entity.
	CEntity *m_pEntity;
	CCamera m_camera;
};

#endif // __CameraProxy_h__
