////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TagPoint.cpp
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: CTagPoint implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TagPoint.h"
#include <IAgent.h>

#include "..\Viewport.h"
#include "Settings.h"

#include <IAISystem.h>



//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTagPoint,CEntity)
IMPLEMENT_DYNCREATE(CRespawnPoint,CTagPoint)
IMPLEMENT_DYNCREATE(CSpawnPoint,CTagPoint)

#define TAGPOINT_RADIUS 0.5f

//////////////////////////////////////////////////////////////////////////
float CTagPoint::m_helperScale = 1;

//////////////////////////////////////////////////////////////////////////
CTagPoint::CTagPoint()
{
	m_entityClass = "TagPoint";
}

//////////////////////////////////////////////////////////////////////////
bool CTagPoint::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	SetColor( RGB(0,0,255) );
	bool res = CEntity::Init( ie,prev,file );

	// Entity can overwrite icon.
	SetTextureIcon( GetClassDesc()->GetTextureIconId() );
	UseMaterialLayersMask(false);
	
	return res;
}

//////////////////////////////////////////////////////////////////////////
float CTagPoint::GetRadius()
{
	return TAGPOINT_RADIUS * m_helperScale * gSettings.gizmo.helpersScale;
}

//////////////////////////////////////////////////////////////////////////
void CTagPoint::SetScale( const Vec3 &scale )
{
	// Ignore scale.
}

//////////////////////////////////////////////////////////////////////////
void CTagPoint::BeginEditParams( IEditor *ie,int flags )
{
	CBaseObject::BeginEditParams( ie,flags );
}

//////////////////////////////////////////////////////////////////////////
void CTagPoint::EndEditParams( IEditor *ie )
{
	CBaseObject::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
int CTagPoint::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown)
	{
		Vec3 pos;
		if (GetIEditor()->GetAxisConstrains() != AXIS_TERRAIN)
		{
			pos = view->MapViewToCP(point);
		}
		else
		{
			// Snap to terrain.
			bool hitTerrain;
			pos = view->ViewToWorld( point,&hitTerrain );
			if (hitTerrain)
			{
				pos.z = GetIEditor()->GetTerrainElevation(pos.x,pos.y) + 1.0f;
			}
			pos = view->SnapToGrid(pos);
		}

		pos = view->SnapToGrid(pos);
		SetPos( pos );
		if (event == eMouseLDown)
			return MOUSECREATE_OK;
		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback( view,event,point,flags );
}

//////////////////////////////////////////////////////////////////////////
void CTagPoint::Display( DisplayContext &dc )
{
	const Matrix34 &wtm = GetWorldTM();

	float fHelperScale = 1*m_helperScale*gSettings.gizmo.helpersScale;
	Vec3 dir = wtm.TransformVector( Vec3(0,fHelperScale,0) );
	Vec3 wp = wtm.GetTranslation();
	dc.SetColor( 1,1,0 );
	dc.DrawArrow( wp,wp+dir*2,fHelperScale );

	if (IsFrozen())
		dc.SetFreezeColor();
	else
	{
		dc.SetColor( GetColor(),0.8f );
	}

	dc.DrawBall( wp,GetRadius() );

	if (IsSelected())
	{
		dc.SetSelectedColor(0.6f);
		dc.DrawBall( wp,GetRadius()+0.02f );
	}

	// Entity can overwrite icon.
//	SetTextureIcon( GetClassDesc()->GetTextureIconId() );
	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
bool CTagPoint::HitTest( HitContext &hc )
{
	// Must use icon..

	Vec3 origin = GetWorldPos();
	float radius = GetRadius();

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross( w );
	float d = w.GetLengthSquared();

	if (d < radius*radius + hc.distanceTollerance)
	{
		Vec3 i0;
		if (Intersect::Ray_SphereFirst(Ray(hc.raySrc,hc.rayDir),Sphere(origin,radius),i0))
		{
			hc.dist = hc.raySrc.GetDistance(i0);
			return true;
		}
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTagPoint::GetBoundBox( BBox &box )
{
	Vec3 pos = GetWorldPos();
	float r = GetRadius();
	box.min = pos - Vec3(r,r,r);
	box.max = pos + Vec3(r,r,r);
}

//////////////////////////////////////////////////////////////////////////
void CTagPoint::GetLocalBounds( BBox &box )
{
	float r = GetRadius();
	box.min = -Vec3(r,r,r);
	box.max = Vec3(r,r,r);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CRespawnPoint::CRespawnPoint()
{
	m_entityClass = "RespawnPoint";
}

CSpawnPoint::CSpawnPoint()
{
	m_entityClass = "SpawnPoint";
}
