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

#include "stdafx.h"
#include "CameraProxy.h"
#include "ISerialize.h"

//////////////////////////////////////////////////////////////////////////
CCameraProxy::CCameraProxy( CEntity *pEntity )
{
	m_pEntity = pEntity;
	UpdateMaterialCamera();
}


//////////////////////////////////////////////////////////////////////////
void CCameraProxy::Release()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CCameraProxy::Init( IEntity *pEntity,SEntitySpawnParams &params )
{
	UpdateMaterialCamera();
};

//////////////////////////////////////////////////////////////////////////
void CCameraProxy::ProcessEvent( SEntityEvent &event )
{
	switch (event.event)
	{
	case ENTITY_EVENT_INIT:
	case ENTITY_EVENT_XFORM:
		{
			UpdateMaterialCamera();
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraProxy::UpdateMaterialCamera()
{
	float fov = m_camera.GetFov();
	m_camera = GetISystem()->GetViewCamera();
	m_camera.SetMatrix( m_pEntity->GetWorldTM() );
	m_camera.SetFrustum( m_camera.GetViewSurfaceX(),m_camera.GetViewSurfaceZ(),fov,m_camera.GetNearPlane(),m_camera.GetFarPlane() );

	IMaterial *pMaterial = m_pEntity->GetMaterial();
	if (pMaterial)
		pMaterial->SetCamera( m_camera );
}

//////////////////////////////////////////////////////////////////////////
void CCameraProxy::SetCamera( CCamera &cam )
{
	m_camera = cam;
	UpdateMaterialCamera();
}

//////////////////////////////////////////////////////////////////////////
void CCameraProxy::Serialize( TSerialize ser )
{

}