////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   AxisHelper.cpp
//  Version:     v1.00
//  Created:     29/10/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AxisHelper.h"

#include "Objects\DisplayContext.h"
#include "Objects\ObjectManager.h"
#include "Viewport.h"
#include "Cry_Geo.h"

#define PLANE_SCALE (0.3f)
#define HIT_RADIUS (4)
#define BOLD_LINE_3D (4)
#define BOLD_LINE_2D (2)

//////////////////////////////////////////////////////////////////////////
CAxisHelper::CAxisHelper()
{
	m_nModeFlags = MOVE_MODE;
	m_currentMode = MOVE_MODE;
	m_highlightMode = 1;

	m_bNeedX = true;
	m_bNeedY = true;
	m_bNeedZ = true;
}

//////////////////////////////////////////////////////////////////////////
void CAxisHelper::SetMode( int nModeFlags )
{
	m_nModeFlags = nModeFlags;
}

//////////////////////////////////////////////////////////////////////////
void CAxisHelper::Prepare( const Matrix34 &worldTM,CViewport *view )
{
	m_fScreenScale = view->GetScreenScaleFactor(worldTM.GetTranslation());
	m_size = gSettings.gizmo.axisGizmoSize * m_fScreenScale;

	m_bNeedX = true;
	m_bNeedY = true;
	m_bNeedZ = true;

	EViewportType vpType = view->GetType();
	bool b2D = false;
	switch (vpType) {
	case ET_ViewportXY:
		b2D = true;
		m_bNeedZ = false;
		break;
	case ET_ViewportXZ:
		b2D = true;
		m_bNeedY = false;
		break;
	case ET_ViewportYZ:
		b2D = true;
		m_bNeedX = false;
		break;
	case ET_ViewportMap:
	case ET_ViewportZ:
		b2D = true;
		break;
	}
	
	m_matrix = worldTM;
	RefCoordSys refCoordSys = GetIEditor()->GetReferenceCoordSys();
	if (b2D && refCoordSys == COORDS_VIEW)
	{
		m_matrix = view->GetViewTM();
		m_matrix.SetTranslation( worldTM.GetTranslation() );
	}
	if (b2D)
	{
		if (refCoordSys == COORDS_VIEW)
		{
			m_bNeedX = true;
			m_bNeedY = true;
			m_bNeedZ = false;
		}
	}
	m_matrix.OrthonormalizeFast();
}

//////////////////////////////////////////////////////////////////////////
void CAxisHelper::DrawAxis( const Matrix34 &worldTM,DisplayContext &dc )
{
	Prepare( worldTM,dc.view );

	Vec3 x(m_size,0,0);
	Vec3 y(0,m_size,0);
	Vec3 z(0,0,m_size);

	int prevRState = dc.GetState();

	if (!(dc.flags & DISPLAY_2D))
		dc.DepthTestOff();

	dc.PushMatrix(m_matrix);
	dc.SetDrawInFrontMode(true);

	Vec3 colSelected(1,1,0);
	Vec3 axisColor(1,1,1);

	if (dc.flags & DISPLAY_2D)
	{
		//axisColor = Vec3(0,0,0);
		//colSelected = Vec3(0.8f,0.8f,0);
	}

	float textSize = 1.4f;
	dc.SetColor(axisColor);
	if (m_bNeedX && gSettings.gizmo.axisGizmoText)
		dc.DrawTextLabel( x,textSize,"x" );
	if (m_bNeedY && gSettings.gizmo.axisGizmoText)
		dc.DrawTextLabel( y,textSize,"y" );
	if (m_bNeedZ && gSettings.gizmo.axisGizmoText)
		dc.DrawTextLabel( z,textSize,"z" );

	int axis = GetIEditor()->GetAxisConstrains();
	if (m_highlightAxis)
		axis = m_highlightAxis;

	int nBoldWidth = BOLD_LINE_3D;
	if (dc.flags & DISPLAY_2D)
	{
		nBoldWidth = BOLD_LINE_2D;
	}

	float linew[3];
	linew[0] = linew[1] = linew[2] = 0;
	Vec3 colX(1,0,0),colY(0,1,0),colZ(0,0,1);
	Vec3 colXArrow=colX,colYArrow=colY,colZArrow=colZ;
	if (axis)
	{
		float col[4] = { 1,0,0,1 };
		if (axis == AXIS_X || axis == AXIS_XY || axis == AXIS_XZ || axis == AXIS_XYZ)
		{
			colX = colSelected;
			dc.SetColor(colSelected);
			if (m_bNeedX && gSettings.gizmo.axisGizmoText)
				dc.DrawTextLabel( x,textSize,"x" );
			linew[0] = nBoldWidth;
		}
		if (axis == AXIS_Y || axis == AXIS_XY || axis == AXIS_YZ || axis == AXIS_XYZ)
		{
			colY = colSelected;
			dc.SetColor(colSelected);
			if (m_bNeedY && gSettings.gizmo.axisGizmoText)
				dc.DrawTextLabel( y,textSize,"y" );
			linew[1] = nBoldWidth;
		}
		if (axis == AXIS_Z || axis == AXIS_XZ || axis == AXIS_YZ || axis == AXIS_XYZ)
		{
			colZ = colSelected;
			dc.SetColor(colSelected);
			if (m_bNeedZ && gSettings.gizmo.axisGizmoText)
				dc.DrawTextLabel( z,textSize,"z" );
			linew[2] = nBoldWidth;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Calc sizes.
	//////////////////////////////////////////////////////////////////////////
	float headOfs[8]; float headScl[8];
	headOfs[MOVE_MODE] = 0;
	headScl[MOVE_MODE] = 0.07f*m_fScreenScale;
	headOfs[ROTATE_MODE] = 0;
	headScl[ROTATE_MODE] = 0.01f*m_fScreenScale;
	headOfs[SCALE_MODE] = 0;
	headScl[SCALE_MODE] = 0.01f*m_fScreenScale;
	if (m_nModeFlags == (MOVE_MODE|ROTATE_MODE|SCALE_MODE))
	{
		headOfs[ROTATE_MODE] += 0.05f*m_fScreenScale;
		headOfs[SCALE_MODE] += 0.1f*m_fScreenScale;
	}
	//////////////////////////////////////////////////////////////////////////

	if (m_bNeedX)
	{
		dc.SetColor( colX );
		dc.SetLineWidth( linew[0] );
		dc.DrawLine( Vec3(0,0,0),x+Vec3(headOfs[SCALE_MODE],0,0) );
	}
	if (m_bNeedY)
	{
		dc.SetColor( colY );
		dc.SetLineWidth( linew[1] );
		dc.DrawLine( Vec3(0,0,0),y+Vec3(0,headOfs[SCALE_MODE],0) );
	}
	if (m_bNeedZ)
	{
		dc.SetColor( colZ );
		dc.SetLineWidth( linew[2] );
		dc.DrawLine( Vec3(0,0,0),z+Vec3(0,0,headOfs[SCALE_MODE]) );
	}

	//////////////////////////////////////////////////////////////////////////
	// Draw Move Arrows.
	//////////////////////////////////////////////////////////////////////////
	if (m_nModeFlags & MOVE_MODE)
	{
		if (m_bNeedX)
		{
			if (m_highlightMode == 1) dc.SetColor( colX ); else dc.SetColor( colXArrow );
			dc.DrawArrow( x-x*0.1f,x,headScl[MOVE_MODE] );
		}
		if (m_bNeedY)
		{
			if (m_highlightMode == 1) dc.SetColor( colY ); else dc.SetColor( colYArrow );
			dc.DrawArrow( y-y*0.1f,y,headScl[MOVE_MODE] );
		}
		if (m_bNeedZ)
		{
			if (m_highlightMode == 1) dc.SetColor( colZ ); else dc.SetColor( colZArrow );
			dc.DrawArrow( z-z*0.1f,z,headScl[MOVE_MODE] );
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Draw Rotate Spheres.
	//////////////////////////////////////////////////////////////////////////
	if (m_nModeFlags & ROTATE_MODE)
	{
		if (m_bNeedX)
		{
			if (m_highlightMode == 2) dc.SetColor( colX ); else dc.SetColor( colXArrow );
			dc.DrawBall( x+Vec3(headOfs[ROTATE_MODE],0,0),headScl[ROTATE_MODE] );
		}
		if (m_bNeedY)
		{
			if (m_highlightMode == 2) dc.SetColor( colY ); else dc.SetColor( colYArrow );
			dc.DrawBall( y+Vec3(0,headOfs[ROTATE_MODE],0),headScl[ROTATE_MODE] );
		}
		if (m_bNeedZ)
		{
			if (m_highlightMode == 2) dc.SetColor( colZ ); else dc.SetColor( colZArrow );
			dc.DrawBall( z+Vec3(0,0,headOfs[ROTATE_MODE]),headScl[ROTATE_MODE] );
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Draw Scale Boxes.
	//////////////////////////////////////////////////////////////////////////
	if (m_nModeFlags & SCALE_MODE)
	{
		dc.SetColor( RGB(128,128,0) );
		Vec3 boxsz = Vec3(headScl[SCALE_MODE],headScl[SCALE_MODE],headScl[SCALE_MODE]);
		dc.DrawSolidBox( -boxsz,boxsz );
		if (m_bNeedX)
		{
			if (m_highlightMode == 3) dc.SetColor( colX ); else dc.SetColor( colXArrow );
			dc.DrawSolidBox( x+boxsz+Vec3(headOfs[SCALE_MODE],0,0),x-boxsz+Vec3(headOfs[SCALE_MODE],0,0) );
		}
		if (m_bNeedY)
		{
			if (m_highlightMode == 3) dc.SetColor( colY ); else dc.SetColor( colYArrow );
			dc.DrawSolidBox( y+boxsz+Vec3(0,headOfs[SCALE_MODE],0),y-boxsz+Vec3(0,headOfs[SCALE_MODE],0) );
		}
		if (m_bNeedZ)
		{
			if (m_highlightMode == 3) dc.SetColor( colZ ); else dc.SetColor( colZArrow );
			dc.DrawSolidBox( z+boxsz+Vec3(0,0,headOfs[SCALE_MODE]),z-boxsz+Vec3(0,0,headOfs[SCALE_MODE]) );
		}
	}
	//////////////////////////////////////////////////////////////////////////

	dc.SetLineWidth(0);

	// If only in move mode.
	if (m_nModeFlags == MOVE_MODE)
	{
		//////////////////////////////////////////////////////////////////////////
		// Draw axis planes.
		//////////////////////////////////////////////////////////////////////////
		Vec3 colXY[2];
		Vec3 colXZ[2];
		Vec3 colYZ[2];

		colX = Vec3(1,0,0);
		colY = Vec3(0,1,0);
		colZ = Vec3(0,0,1);
		colXY[0] = colX;
		colXY[1] = colY;
		colXZ[0] = colX;
		colXZ[1] = colZ;
		colYZ[0] = colY;
		colYZ[1] = colZ;

		linew[0] = linew[1] = linew[2] = 0;
		if (axis)
		{
			if (axis == AXIS_XY)
			{
				colXY[0] = colSelected;
				colXY[1] = colSelected;
				linew[0] = nBoldWidth;
			}
			else if (axis == AXIS_XZ)
			{
				colXZ[0] = colSelected;
				colXZ[1] = colSelected;
				linew[1] = nBoldWidth;
			}
			else if (axis == AXIS_YZ)
			{
				colYZ[0] = colSelected;
				colYZ[1] = colSelected;
				linew[2] = nBoldWidth;
			}
		}

		dc.SetColor( RGB(255,255,0),0.5f );
		//dc.DrawQuad( org,org+z*0.5f,org+z*0.5f+x*0.5f,org+x*0.5f );
		float sz = m_size*PLANE_SCALE;
		Vec3 p1(sz,sz,0);
		Vec3 p2(sz,0,sz);
		Vec3 p3(0,sz,sz);

		float colAlpha = 1.0f;
		x *= PLANE_SCALE; y *= PLANE_SCALE; z *= PLANE_SCALE;

		// XY
		if (m_bNeedX && m_bNeedY)
		{
			dc.SetLineWidth( linew[0] );
			dc.SetColor( colXY[0],colAlpha );
			dc.DrawLine( p1,p1-x );
			dc.SetColor( colXY[1],colAlpha );
			dc.DrawLine( p1,p1-y );
		}

		// XZ
		if (m_bNeedX && m_bNeedZ)
		{
			dc.SetLineWidth( linew[1] );
			dc.SetColor( colXZ[0],colAlpha );
			dc.DrawLine( p2,p2-x );
			dc.SetColor( colXZ[1],colAlpha );
			dc.DrawLine( p2,p2-z );
		}

		// YZ
		if (m_bNeedY && m_bNeedZ)
		{
			dc.SetLineWidth( linew[2] );
			dc.SetColor( colYZ[0],colAlpha );
			dc.DrawLine( p3,p3-y );
			dc.SetColor( colYZ[1],colAlpha );
			dc.DrawLine( p3,p3-z );
		}

		dc.SetLineWidth(0);

		colAlpha = 0.25f;

		if (axis == AXIS_XY && m_bNeedX && m_bNeedY)
		{
			dc.CullOff();
			dc.SetColor( colSelected,colAlpha );
			dc.DrawQuad( p1,p1-x,p1-x-y,p1-y );
			dc.CullOn();
		}
		else if (axis == AXIS_XZ && m_bNeedX && m_bNeedZ)
		{
			dc.CullOff();
			dc.SetColor( colSelected,colAlpha );
			dc.DrawQuad( p2,p2-x,p2-x-z,p2-z );
			dc.CullOn();
		}
		else if (axis == AXIS_YZ && m_bNeedY && m_bNeedZ)
		{
			dc.CullOff();
			dc.SetColor( colSelected,colAlpha );
			dc.DrawQuad( p3,p3-y,p3-y-z,p3-z );
			dc.CullOn();
		}
	}

	if (!(dc.flags & DISPLAY_2D))
		dc.DepthTestOn();

	dc.PopMatrix();
	dc.SetState( prevRState );
}

//////////////////////////////////////////////////////////////////////////
bool CAxisHelper::HitTest( const Matrix34 &worldTM,HitContext &hc )
{
	if (hc.distanceTollerance != 0)
		return 0;

	Prepare( worldTM,hc.view );

	Vec3 x(m_size,0,0);
	Vec3 y(0,m_size,0);
	Vec3 z(0,0,m_size);

	float hitDist = 0.01f * m_fScreenScale;

	Vec3 pos = m_matrix.GetTranslation();

	Vec3 intPoint;
	Ray ray(hc.raySrc,hc.rayDir);
	Sphere sphere( pos,m_size + 0.1f*m_fScreenScale );
	if (!Intersect::Ray_SphereFirst( ray,sphere,intPoint ))
	{
		m_highlightAxis = 0;
		return false;
	}

	x = m_matrix.TransformVector(x);
	y = m_matrix.TransformVector(y);
	z = m_matrix.TransformVector(z);

	float sz = m_size*PLANE_SCALE;
	Vec3 p1(sz,sz,0);
	Vec3 p2(sz,0,sz);
	Vec3 p3(0,sz,sz);

	p1 = m_matrix.TransformPoint(p1);
	p2 = m_matrix.TransformPoint(p2);
	p3 = m_matrix.TransformPoint(p3);

	Vec3 planeX = x*PLANE_SCALE;
	Vec3 planeY = y*PLANE_SCALE;
	Vec3 planeZ = z*PLANE_SCALE;

	hc.manipulatorMode = 0;

	int hitRadius = HIT_RADIUS;
	int axis = 0;
	if (hc.view->HitTestLine( pos,pos+x,hc.point2d,hitRadius ))
		axis = AXIS_X;
	else if (hc.view->HitTestLine( pos,pos+y,hc.point2d,hitRadius ))
		axis = AXIS_Y;
	else if (hc.view->HitTestLine( pos,pos+z,hc.point2d,hitRadius ))
		axis = AXIS_Z;
	else if (m_nModeFlags == MOVE_MODE)
	{
		// If only in move mode.
		if (hc.view->HitTestLine( p1,p1-planeX,hc.point2d,hitRadius ))
			axis = AXIS_XY;
		else if (hc.view->HitTestLine( p1,p1-planeY,hc.point2d,hitRadius ))
			axis = AXIS_XY;
		else if (hc.view->HitTestLine( p2,p2-planeX,hc.point2d,hitRadius ))
			axis = AXIS_XZ;
		else if (hc.view->HitTestLine( p2,p2-planeZ,hc.point2d,hitRadius ))
			axis = AXIS_XZ;
		else if (hc.view->HitTestLine( p3,p3-planeY,hc.point2d,hitRadius ))
			axis = AXIS_YZ;
		else if (hc.view->HitTestLine( p3,p3-planeZ,hc.point2d,hitRadius ))
			axis = AXIS_YZ;
		if (axis != 0)
			hc.manipulatorMode = 1;
	}

	//////////////////////////////////////////////////////////////////////////
	// Calc sizes.
	//////////////////////////////////////////////////////////////////////////
	float headOfs[8]; float headScl[8];
	headOfs[MOVE_MODE] = 0;
	headScl[MOVE_MODE] = 0.07f*m_fScreenScale;
	headOfs[ROTATE_MODE] = 0;
	headScl[ROTATE_MODE] = 0.01f*m_fScreenScale;
	headOfs[SCALE_MODE] = 0;
	headScl[SCALE_MODE] = 0.01f*m_fScreenScale;
	if (m_nModeFlags == (MOVE_MODE|ROTATE_MODE|SCALE_MODE))
	{
		headOfs[ROTATE_MODE] += 0.05f*m_fScreenScale;
		headOfs[SCALE_MODE] += 0.1f*m_fScreenScale;
	}
	//////////////////////////////////////////////////////////////////////////
	if (axis == 0 && (m_nModeFlags & ROTATE_MODE))
	{
		Vec3 x(m_size,0,0);
		Vec3 y(0,m_size,0);
		Vec3 z(0,0,m_size);
		int hitr = 10;
		if (m_bNeedX)
		{
			if (GetDistance2D(hc.view,hc.point2d,m_matrix.TransformPoint(x+Vec3(headOfs[ROTATE_MODE],0,0))) < hitr)
			{
				axis = AXIS_X;
				hc.manipulatorMode = 2;
			}
		}
		if (m_bNeedY)
		{
			if (GetDistance2D(hc.view,hc.point2d,m_matrix.TransformPoint(y+Vec3(0,headOfs[ROTATE_MODE],0))) < hitr)
			{
				axis = AXIS_Y;
				hc.manipulatorMode = 2;
			}
		}
		if (m_bNeedZ)
		{
			if (GetDistance2D(hc.view,hc.point2d,m_matrix.TransformPoint(z+Vec3(0,0,headOfs[ROTATE_MODE]))) < hitr)
			{
				axis = AXIS_Z;
				hc.manipulatorMode = 2;
			}
		}
	}
	if (m_nModeFlags & SCALE_MODE)
	{
		Vec3 x(m_size,0,0);
		Vec3 y(0,m_size,0);
		Vec3 z(0,0,m_size);
		int hitr = 14;
		if (GetDistance2D(hc.view,hc.point2d,m_matrix.GetTranslation()) < hitr+2)
		{
			axis = AXIS_XYZ;
			hc.manipulatorMode = 3;
		}
		if (axis == 0)
		{
			if (m_bNeedX)
			{
				if (GetDistance2D(hc.view,hc.point2d,m_matrix.TransformPoint(x+Vec3(headOfs[SCALE_MODE],0,0))) < hitr)
				{
					axis = AXIS_X;
					hc.manipulatorMode = 3;
				}
			}
			if (m_bNeedY)
			{
				if (GetDistance2D(hc.view,hc.point2d,m_matrix.TransformPoint(y+Vec3(0,headOfs[SCALE_MODE],0))) < hitr)
				{
					axis = AXIS_Y;
					hc.manipulatorMode = 3;
				}
			}
			if (m_bNeedZ)
			{
				if (GetDistance2D(hc.view,hc.point2d,m_matrix.TransformPoint(z+Vec3(0,0,headOfs[SCALE_MODE]))) < hitr)
				{
					axis = AXIS_Z;
					hc.manipulatorMode = 3;
				}
			}
		}
	}

	if (axis != 0)
	{
		if (hc.manipulatorMode == 0)
		{
			if (m_nModeFlags & MOVE_MODE)
				hc.manipulatorMode = 1;
			else if (m_nModeFlags & ROTATE_MODE)
				hc.manipulatorMode = 2;
			else if (m_nModeFlags & SCALE_MODE)
				hc.manipulatorMode = 3;
		}
		hc.axis = axis;
		hc.dist = 0;
		m_highlightMode = hc.manipulatorMode;
	}

	m_highlightAxis = axis;

	return axis != 0;
}

//////////////////////////////////////////////////////////////////////////
float CAxisHelper::GetDistance2D( CViewport *view,CPoint p,Vec3 &wp )
{
	CPoint vp = view->WorldToView( wp );
	return sqrtf( (p.x-vp.x)*(p.x-vp.x) + (p.y-vp.y)*(p.y-vp.y) );
}
