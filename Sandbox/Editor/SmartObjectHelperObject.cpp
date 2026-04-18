////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SmartObjectHelperObject.h
//  Version:     v1.00
//  Created:     18/09/2005 by Dejan Pavlovski
//  Compilers:   Visual C++ 7.1
//  Description: Smart Object Helper Object
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include <IAISystem.h>
#include "Objects/Entity.h"

#include "../Viewport.h"
#include "SmartObjectHelperObject.h"


//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE( CSmartObjectHelperObject, CBaseObject )

#define RADIUS 0.05f

//////////////////////////////////////////////////////////////////////////
CSmartObjectHelperObject::CSmartObjectHelperObject()
{	
	m_pVar = NULL;
	m_pSmartObjectClass = NULL;
	m_pEntity = NULL;
//	m_fromGeometry = false;
}

CSmartObjectHelperObject::~CSmartObjectHelperObject()
{ 
}

//////////////////////////////////////////////////////////////////////////
extern IVariable* GetChildVar( const IVariable* array, const char* name, bool recursive = false );

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::UpdateVarFromObject()
{
	assert( m_pVar );
	if ( !m_pVar || !m_pSmartObjectClass )
		return;

	if ( !m_pEntity )
	{
		Warning( "[CSmartObjectHelperObject::UpdateVariable] m_pEntity is null, returning" );
		return;
	}

	if (IVariable* pVar = GetChildVar( m_pVar, "name" ))
		pVar->Set( GetName() );

	if (IVariable* pVar = GetChildVar( m_pVar, "position" ))
	{ 
		Vec3 pos = m_pEntity->GetWorldTM().GetInvertedFast().TransformPoint( GetWorldPos() );
		pVar->Set( pos );
	}

	if (IVariable* pVar = GetChildVar( m_pVar, "direction" ))
	{     
		Matrix33 relTM = Matrix33( m_pEntity->GetWorldTM().GetInvertedFast() ) * Matrix33( GetWorldTM() );
		Vec3 dir = relTM.TransformVector( FORWARD_DIRECTION );
		pVar->Set( dir );
	}
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::Done()
{	
	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CSmartObjectHelperObject::Init( IEditor* ie, CBaseObject* prev, const CString& file )
{
	SetColor( RGB(255,255,0) );
	return CBaseObject::Init( ie, prev, file );
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::BeginEditParams( IEditor* ie, int flags )
{	
//	CBaseObject::BeginEditParams( ie, flags );
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::EndEditParams( IEditor *ie )
{
	CBaseObject::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
int CSmartObjectHelperObject::MouseCreateCallback( CViewport* view, EMouseEvent event, CPoint& point, int flags )
{
	if ( event == eMouseMove || event == eMouseLDown )
	{
		Vec3 pos;
		// Position 1 meter above ground when creating.
		if ( GetIEditor()->GetAxisConstrains() != AXIS_TERRAIN )
		{
			pos = view->MapViewToCP(point);
		}
		else
		{
			// Snap to terrain.
			bool hitTerrain;
			pos = view->ViewToWorld( point, &hitTerrain );
			if ( hitTerrain )
			{
				pos.z = GetIEditor()->GetTerrainElevation( pos.x, pos.y ) + 1.0f;
			}
			pos = view->SnapToGrid( pos );
		}
		SetPos( pos );
		if ( event == eMouseLDown )
			return MOUSECREATE_OK;
		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback( view, event, point, flags );
}

//////////////////////////////////////////////////////////////////////////
bool CSmartObjectHelperObject::HitTest( HitContext& hc )
{
	Vec3 origin = GetWorldPos();
	float radius = RADIUS;

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross( w );
	float d = w.GetLength();

	if ( d < radius + hc.distanceTollerance )
	{
		Vec3 i0;
		if (Intersect::Ray_SphereFirst( Ray(hc.raySrc,hc.rayDir), Sphere(origin,radius), i0 ))
		{
			hc.dist = hc.raySrc.GetDistance( i0 );
			return true;
		}
		hc.dist = hc.raySrc.GetDistance( origin );
		return true;
	}	
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::Display( DisplayContext &dc )
{
	COLORREF color = GetColor();
	float radius = RADIUS;

	//dc.SetColor( color, 0.5f );
	//dc.DrawBall( GetPos(), radius );

	if ( IsSelected() )
	{
		dc.SetSelectedColor( 0.6f );		
	}
	if ( !IsHighlighted() )
	{
		//if (dc.flags & DISPLAY_2D)
		{ 
			BBox box;
			GetLocalBounds( box );
			dc.SetColor( color, 0.8f );
			dc.PushMatrix( GetWorldTM() );
			dc.SetLineWidth( 2 );
			dc.DrawWireBox( box.min, box.max );
			dc.SetLineWidth( 0 );
			dc.PopMatrix();
		}
	}

	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::GetBoundBox( BBox& box )
{  
	// Transform local bounding box into world space.
	GetLocalBounds( box );
	box.SetTransformedAABB( GetWorldTM(), box );  
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::GetLocalBounds( BBox& box )
{ 
	// return local bounds  
	float r = RADIUS;
	box.min = -Vec3(r,r,r);
	box.max = Vec3(r,r,r);  
}

//////////////////////////////////////////////////////////////////////////
void CSmartObjectHelperObject::GetBoundSphere( Vec3& pos, float& radius )
{
	pos = GetPos();
	radius = RADIUS;
}
