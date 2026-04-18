////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   axisgizmo.cpp
//  Version:     v1.00
//  Created:     2/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AxisGizmo.h"
#include "DisplayContext.h"

#include "..\Viewport.h"
#include "..\DisplaySettings.h"
#include "ObjectManager.h"

#include "GizmoManager.h"
#include "Settings.h"
#include "Grid.h"
#include "EditTool.h"
#include "ViewManager.h"

#include "RenderHelpers\AxisHelper.h"

//////////////////////////////////////////////////////////////////////////
// CAxisGizmo implementation.
//////////////////////////////////////////////////////////////////////////
int CAxisGizmo::m_axisGizmoCount = 0;

//////////////////////////////////////////////////////////////////////////
CAxisGizmo::CAxisGizmo( CBaseObject *object )
{
	assert( object != 0 );
	m_object = object;
	m_pAxisHelper = new CAxisHelper;

	// Set selectable flag.
	SetFlags( EGIZMO_SELECTABLE|EGIZMO_TRANSFORM_MANIPULATOR );

	m_axisGizmoCount++;
	m_object->AddEventListener( functor(*this,&CAxisGizmo::OnObjectEvent) );

	m_localTM.SetIdentity();
	m_parentTM.SetIdentity();
	m_matrix.SetIdentity();

	m_bDragging = false;
}

//////////////////////////////////////////////////////////////////////////
CAxisGizmo::CAxisGizmo()
{
	// Set selectable flag.
	SetFlags( EGIZMO_SELECTABLE );
	m_axisGizmoCount++;
	m_pAxisHelper = new CAxisHelper;
	m_bDragging = false;
}

//////////////////////////////////////////////////////////////////////////
CAxisGizmo::~CAxisGizmo()
{
	delete m_pAxisHelper;
	if (m_object)
		m_object->RemoveEventListener( functor(*this,&CAxisGizmo::OnObjectEvent) );
	m_axisGizmoCount--;
}

//////////////////////////////////////////////////////////////////////////
void CAxisGizmo::OnObjectEvent( CBaseObject *object,int event )
{
	if (event == CBaseObject::ON_DELETE || event == CBaseObject::ON_UNSELECT)
	{
		// This gizmo must be deleted as well.
		GetGizmoManager()->RemoveGizmo(this);
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAxisGizmo::Display( DisplayContext &dc )
{
	if (m_object)
	{
		bool bNotSelected = m_object->CheckFlags(OBJFLAG_INVISIBLE) || !m_object->IsSelected();
		if (bNotSelected)
		{
			// This gizmo must be deleted.
			DeleteThis();
			return;
		}
	}
	DrawAxis( dc );
}

//////////////////////////////////////////////////////////////////////////
void CAxisGizmo::SetWorldBounds( const AABB &bbox )
{
	m_bbox = bbox;
}

//////////////////////////////////////////////////////////////////////////
void CAxisGizmo::GetWorldBounds( BBox &bbox )
{
	if (m_object)
	{
		m_object->GetBoundBox( bbox );
	}
	else
	{
		bbox.min = Vec3(-1000000,-1000000,-1000000);
		bbox.max = Vec3(1000000,1000000,1000000);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAxisGizmo::DrawAxis( DisplayContext &dc )
{
	m_pAxisHelper->SetHighlightAxis( m_highlightAxis );
	// Only enable axis planes when editor is in Move mode.
	int nEditMode = GetIEditor()->GetEditMode();
	int nModeFlags = 0;
	if (nEditMode == eEditModeMove)
		nModeFlags |= CAxisHelper::MOVE_MODE;
	else if (nEditMode == eEditModeRotate)
		nModeFlags |= CAxisHelper::ROTATE_MODE;
	else if (nEditMode == eEditModeScale)
		nModeFlags |= CAxisHelper::SCALE_MODE;
	//else
		//nModeFlags |= CAxisHelper::MOVE_MODE;

	//nModeFlags |= CAxisHelper::MOVE_MODE | CAxisHelper::ROTATE_MODE | CAxisHelper::SCALE_MODE;

	m_pAxisHelper->SetMode( nModeFlags );

	Matrix34 tm = GetTransformation(GetIEditor()->GetReferenceCoordSys());
	m_pAxisHelper->DrawAxis( tm,dc );
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CAxisGizmo::GetMatrix() const
{
	if (m_object)
	{
		m_matrix.SetTranslation( m_object->GetWorldTM().GetTranslation() );
	}
	return m_matrix;
}

//////////////////////////////////////////////////////////////////////////
bool CAxisGizmo::HitTest( HitContext &hc )
{
	if (GetIEditor()->GetEditMode() == 0)
		return false;

	Matrix34 tm = GetTransformation(GetIEditor()->GetReferenceCoordSys());

	CAxisHelper axis;
	bool bRes = m_pAxisHelper->HitTest( tm,hc );
	if (bRes)
		hc.object = m_object;

	m_highlightAxis = m_pAxisHelper->GetHighlightAxis();

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
bool CAxisGizmo::HitTestManipulator( HitContext &hc )
{
	return HitTest( hc );
}

//////////////////////////////////////////////////////////////////////////
void CAxisGizmo::SetTransformation( RefCoordSys coordSys,const Matrix34 &tm )
{
	switch (coordSys) {
	case COORDS_WORLD:
		SetMatrix(tm);
		break;
	case COORDS_LOCAL:
		m_localTM = tm;
		{
			Matrix34 wtm;
			wtm.SetIdentity();
			wtm.SetTranslation(m_localTM.GetTranslation());
			SetMatrix(wtm);
			m_userTM = tm;
		}
		m_parentTM = m_localTM;
		break;
	case COORDS_PARENT:
		m_parentTM = tm;
		break;
	case COORDS_USERDEFINED:
		m_userTM = tm;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
Matrix34 CAxisGizmo::GetTransformation( RefCoordSys coordSys ) const
{
	if (m_object)
	{
		switch (coordSys)
		{
		case COORDS_VIEW:
			return GetMatrix();
			break;
		case COORDS_LOCAL:
			return m_object->GetWorldTM();
			break;
		case COORDS_PARENT:
			//return m_parentTM;
			if (m_object->GetParent())
			{
				Matrix34 parentTM = m_object->GetParent()->GetWorldTM();
				parentTM.SetTranslation(m_object->GetWorldTM().GetTranslation());
				return parentTM;
			}
			else
			{
				return GetMatrix();
			}
			break;
		case COORDS_WORLD:
			return GetMatrix();
			break;
		case COORDS_USERDEFINED:
			{
				Matrix34 userTM = GetIEditor()->GetViewManager()->GetGrid()->GetOrientationMatrix();
				userTM.SetTranslation(m_object->GetWorldTM().GetTranslation());
				return userTM;
			}
			break;
		}
	}
	else
	{
		switch (coordSys)
		{
		case COORDS_VIEW:
			return GetMatrix();
			break;
		case COORDS_LOCAL:
			return m_localTM;
			break;
		case COORDS_PARENT:
			return m_parentTM;
			break;
		case COORDS_WORLD:
			return GetMatrix();
			break;
		case COORDS_USERDEFINED:
			return m_userTM;
			break;
		}
	}
	return GetMatrix();
}

//////////////////////////////////////////////////////////////////////////
bool CAxisGizmo::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseLDown)
	{
		HitContext hc;
		hc.view = view;
		hc.point2d = point;
		view->ViewToWorldRay(point,hc.raySrc,hc.rayDir);
		if (HitTest(hc))
		{
			if (event != eMouseLDown)
				return false;

			// On Left mouse down.

			// Hit axis gizmo.
			GetIEditor()->SetAxisConstrains( (AxisConstrains)hc.axis );
			view->SetAxisConstrain( hc.axis );

			view->SetConstructionMatrix( COORDS_LOCAL,GetTransformation(COORDS_LOCAL) );
			view->SetConstructionMatrix( COORDS_PARENT,GetTransformation(COORDS_PARENT) );
			view->SetConstructionMatrix( COORDS_USERDEFINED,GetTransformation(COORDS_USERDEFINED) );

			view->BeginUndo();
			view->CaptureMouse();
			m_bDragging = true;
			m_cMouseDownPos = point;

			switch (hc.manipulatorMode)
			{
			case 1:
				view->SetCurrentCursor( STD_CURSOR_MOVE );
				GetIEditor()->SetEditMode( eEditModeMove );
				break;
			case 2:
				view->SetCurrentCursor( STD_CURSOR_ROTATE );
				GetIEditor()->SetEditMode( eEditModeRotate );
				break;
			case 3:
				view->SetCurrentCursor( STD_CURSOR_SCALE );
				GetIEditor()->SetEditMode( eEditModeScale );
				break;
			}
			return true;
		}
	}
	else if (event == eMouseMove)
	{
		if (m_bDragging)
		{
			Vec3 vDragValue(0,0,0);
			// Dragging transform manipulator.
			switch (GetIEditor()->GetEditMode())
			{
			case eEditModeMove:
				{
					view->SetCurrentCursor( STD_CURSOR_MOVE );

					Vec3 v;
					//m_cMouseDownPos = point;
					bool followTerrain = false;
					if (view->GetAxisConstrain() == AXIS_TERRAIN)
					{
						followTerrain = true;
						Vec3 p1 = view->SnapToGrid(view->ViewToWorld( m_cMouseDownPos ));
						Vec3 p2 = view->SnapToGrid(view->ViewToWorld( point ));
						v = p2 - p1;
						v.z = 0;
					}
					else
					{
						Vec3 p1 = view->MapViewToCP(m_cMouseDownPos);
						Vec3 p2 = view->MapViewToCP(point);
						if (p1.IsZero() || p2.IsZero())
							return true;
						v = view->GetCPVector(p1,p2);
					}
					vDragValue = v;
				}
				break;
			case eEditModeRotate:
				{
					view->SetCurrentCursor( STD_CURSOR_ROTATE );

					Ang3 ang(0,0,0);
					float ax = (point.x - m_cMouseDownPos.x);
					float ay = (point.y - m_cMouseDownPos.y);
					switch (view->GetAxisConstrain())
					{
					case AXIS_X: ang.x = ay; break;
					case AXIS_Y: ang.y = ay; break;
					case AXIS_Z: ang.z = ay; break;
					case AXIS_XY: ang(ax,ay,0); break;
					case AXIS_XZ: ang(ax,0,ay); break;
					case AXIS_YZ: ang(0,ay,ax); break;
					case AXIS_TERRAIN: ang(ax,ay,0); break;
					};
					ang = gSettings.pGrid->SnapAngle(ang);
					vDragValue = Vec3( DEG2RAD(ang) );
				}
				break;
			case eEditModeScale:
				{
					Vec3 scl(0,0,0);
					float ay = 1.0f - 0.01f*(point.y - m_cMouseDownPos.y);
					if (ay < 0.01f) ay = 0.01f;
					scl(ay,ay,ay);
					switch (view->GetAxisConstrain())
					{
					case AXIS_X: scl(ay,1,1); break;
					case AXIS_Y: scl(1,ay,1); break;
					case AXIS_Z: scl(1,1,ay); break;
					case AXIS_XY: scl(ay,ay,ay); break;
					case AXIS_XZ: scl(ay,ay,ay); break;
					case AXIS_YZ: scl(ay,ay,ay); break;
					case AXIS_XYZ: scl(ay,ay,ay); break;
					case AXIS_TERRAIN: scl(ay,ay,ay); break;
					};
					view->SetCurrentCursor( STD_CURSOR_SCALE );
					vDragValue = scl;
				}
				break;
			}
			CEditTool *pEditTool = view->GetEditTool();
			if (pEditTool)
			{
				pEditTool->OnManipulatorDrag( view,this,m_cMouseDownPos,point,vDragValue );
			}

			return true;
		}
		else
		{
			// Hit test current transform manipulator, to highlight when mouse over.
			HitContext hc;
			hc.view = view;
			hc.point2d = point;
			view->ViewToWorldRay(point,hc.raySrc,hc.rayDir);
			if (HitTest(hc))
			{
				switch (hc.manipulatorMode)
				{
				case 1: view->SetCurrentCursor( STD_CURSOR_MOVE ); break;
				case 2: view->SetCurrentCursor( STD_CURSOR_ROTATE ); break;
				case 3: view->SetCurrentCursor( STD_CURSOR_SCALE ); break;
				}
			}
		}
	}
	else if (event == eMouseLUp)
	{
		if (m_bDragging)
		{
			view->AcceptUndo( "Manipulator Drag" );
			view->ReleaseMouse();
			m_bDragging = false;
		}
	}

	return false;
}
