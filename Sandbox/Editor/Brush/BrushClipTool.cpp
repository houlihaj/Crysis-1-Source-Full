////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   BrushClipTool.cpp
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain Modification Tool implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BrushClipToolPanel.h"
#include <InitGuid.h>
#include "BrushClipTool.h"
#include "..\Viewport.h"

#include "Objects\ObjectManager.h"
#include "Brush.h"
#include "SolidBrushObject.h"
#include "Grid.h"
#include "Include\ITransformManipulator.h"
#include "EditMode\ObjectMode.h"

#include <IRenderAuxGeom.h>

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CBrushClipTool,CEditTool)

float CBrushClipTool::m_fPlaneScale = 10;
float CBrushClipTool::m_fPlaneAngle = 0;
int CBrushClipTool::m_nPlaneType = 0;

int CBrushClipTool::m_nPanelId = 0;

static IClassDesc* g_BrushClipToolClass = NULL;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! Undo object for CBaseObject.
class CUndoBrushClipTool : public IUndoObject
{
public:
	CUndoBrushClipTool( CBrushClipTool *pClipTool )
	{
		// Stores the current state of this object.
		for (int i = 0; i < 3; i++) m_undo[i] = pClipTool->m_points[i];
	}
protected:
	virtual int GetSize() { return sizeof(*this); }
	virtual const char* GetDescription() { return ""; };

	virtual void Undo( bool bUndo )
	{
		CEditTool *pTool = GetIEditor()->GetEditTool();
		if (!pTool || !pTool->IsKindOf(RUNTIME_CLASS(CBrushClipTool)))
			return;
		CBrushClipTool *pClipTool = (CBrushClipTool*)pTool;
		if (bUndo)
		{
			for (int i = 0; i < 3; i++) m_redo[i] = pClipTool->m_points[i];
		}
		for (int i = 0; i < 3; i++) pClipTool->m_points[i] = m_undo[i];
		pClipTool->UpdateClipPlane();
	}
	virtual void Redo()
	{
		CEditTool *pTool = GetIEditor()->GetEditTool();
		if (!pTool || !pTool->IsKindOf(RUNTIME_CLASS(CBrushClipTool)))
			return;
		CBrushClipTool *pClipTool = (CBrushClipTool*)pTool;
		for (int i = 0; i < 3; i++) pClipTool->m_points[i] = m_redo[i];
		pClipTool->UpdateClipPlane();
	}
private:
	Vec3 m_undo[3];
	Vec3 m_redo[3];
};

//////////////////////////////////////////////////////////////////////////
CBrushClipTool::CBrushClipTool()
{
	m_pClassDesc = g_BrushClipToolClass;
	SetStatusText( _T("Clip Brush(s)") );
	m_mode = SelectMode;
	m_dragPoint = 0;
	m_clipPlane.n.Set(0,0,1);
	m_clipPlane.d = 0;
	m_points[0].Set(0,0,0);
	m_points[1].Set(1,0,0);
	m_points[2].Set(1,1,0);
	m_pDragView = 0;
}

//////////////////////////////////////////////////////////////////////////
CBrushClipTool::~CBrushClipTool()
{
	GetIEditor()->ShowTransformManipulator(false);
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::BeginEditParams( IEditor *ie,int flags )
{
	if (!m_nPanelId)
	{
		CBrushClipToolPanel *pPanel = new CBrushClipToolPanel();
		pPanel->SetTool( this );
		m_nPanelId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,_T("Brush Clip Tool"),pPanel );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::EndEditParams()
{
	if (m_nPanelId)
	{
		GetIEditor()->RemoveRollUpPage( ROLLUP_OBJECTS,m_nPanelId );
		m_nPanelId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBrushClipTool::Activate( CEditTool *pPreviousTool )
{
	// Previous tool must be object mode.
	//if (!pPreviousTool || !pPreviousTool->GetClassDesc() || pPreviousTool->GetClassDesc()->ClassID() != OBJECT_MODE_GUID)
		//return false;

	// Remember selection here.
	CSelectionGroup *pSel = GetIEditor()->GetSelection();
	if (pSel->IsEmpty())
	{
		return false;
	}
	//SetParentTool( pPreviousTool );

	GetIEditor()->GetObjectManager()->EndEditParams();
	((CObjectManager*)GetIEditor()->GetObjectManager())->HideTransformManipulators();

	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject *pObject = pSel->GetObject(i);
		ITransformManipulator *pManipulator = GetIEditor()->ShowTransformManipulator(true);

		BBox box;
		pObject->GetBoundBox(box);
		float s = box.GetSize().GetLength();

		m_points[0] = box.min;
		m_points[1] = Vec3(box.min.x,box.max.y,box.min.z);
		m_points[2] = Vec3(box.min.x,box.max.y,box.max.z);

		// In local space orient axis gizmo by first object.
		Matrix34 tm;
		tm.SetIdentity();
		tm.SetTranslation( pObject->GetWorldPos() );
		pManipulator->SetTransformation( COORDS_LOCAL,tm );
		pManipulator->SetTransformation( COORDS_PARENT,tm );
		pManipulator->SetTransformation( COORDS_USERDEFINED,tm );
		break;
	}

	UpdateClipBrushes();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::DrawPoint( DisplayContext &dc,int n )
{
	if (n == m_dragPoint)
		dc.SetSelectedColor();
	else
		dc.SetColor( ColorB(0,0,255,128) );
	float r = 0.01f * dc.view->GetScreenScaleFactor(m_points[n]);
	dc.DrawBall( m_points[n],r );

	if (n == m_dragPoint)
		dc.SetSelectedColor();
	else
		dc.SetColor(ColorB(0,255,0,255));
	float l = r*5;
	dc.DrawLine( m_points[n]-Vec3(l,0,0),m_points[n]+Vec3(l,0,0) );
	dc.DrawLine( m_points[n]-Vec3(0,l,0),m_points[n]+Vec3(0,l,0) );
	dc.DrawLine( m_points[n]-Vec3(0,0,l),m_points[n]+Vec3(0,0,l) );
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::Display( DisplayContext &dc )
{
	// Draw clipping plane.
	if (dc.flags & DISPLAY_2D)
	{
		// Draw points.
		Vec3 p1 = m_points[0];
		Vec3 p2 = m_points[1];
		dc.SetColor( ColorB(0,0,255,255) );
		dc.DrawPoint( p1,2 );
		//dc.DrawWireCircle2d( p1,1.40f*fScreenScale );
		dc.DrawPoint( p2,2 );
		//dc.DrawWireCircle2d( p2,1.4f*fScreenScale );
		dc.DrawWireCircle2d( dc.view->WorldToView(m_points[0]),4,0.5f );
		dc.DrawWireCircle2d( dc.view->WorldToView(m_points[1]),4,0.5f );

		Vec3 midp = (p1+p2)*0.5f;

		dc.DrawLine( p1,p2 );
		dc.SetColor( ColorB(255,255,0,255) );
		dc.DrawLine( midp,midp + m_clipPlane.n*0.05f*dc.view->GetScreenScaleFactor(midp) );

		dc.SetLineWidth(4);
		int i;
		dc.SetColor( RGB(255,255,255) );
		for (i = 0; i < m_frontBrushes.size(); i++)
		{
			SBrush *pBrush = m_frontBrushes[i];
			pBrush->Display( dc );
		}
		dc.SetColor( RGB(255,255,0) );
		for (i = 0; i < m_backBrushes.size(); i++)
		{
			SBrush *pBrush = m_backBrushes[i];
			pBrush->Display( dc );
		}
		dc.SetLineWidth(0);

		return;
	}

	int prevState = dc.GetState();
	dc.CullOff();
	dc.DepthWriteOff();
	dc.SetDrawInFrontMode(true);
	// Draw Construction plane.

	DrawPoint( dc,0 );
	DrawPoint( dc,1 );
	if (m_nPlaneType == 0)
		DrawPoint( dc,2 );

	Vec3 dir = m_points[1] - m_points[0];
	dir.NormalizeSafe();
	Vec3 u = dir * 0.5f;
	Vec3 v = (dir).Cross(m_clipPlane.n) * 0.5f;
	Vec3 p = (m_points[0] + m_points[1] + m_points[2])/3.0f;
	Vec3 vc = GetIEditor()->GetSelection()->GetCenter();
	if (p.x == 0) p.x = vc.x;
	if (p.y == 0) p.y = vc.y;
	if (p.z == 0) p.z = vc.z;
	
	float s = 1.0f * m_fPlaneScale;

	dc.SetLineWidth(4);
	int i;
	dc.SetColor( RGB(255,255,255) );
	for (i = 0; i < m_frontBrushes.size(); i++)
	{
		SBrush *pBrush = m_frontBrushes[i];
		pBrush->Display( dc );
	}
	dc.SetColor( RGB(255,255,0) );
	for (i = 0; i < m_backBrushes.size(); i++)
	{
		SBrush *pBrush = m_backBrushes[i];
		pBrush->Display( dc );
	}
	dc.SetLineWidth(0);

	dc.SetDrawInFrontMode(false);
	dc.SetColor( RGB(255,255,0),0.3f );
	dc.DrawQuad( p-u*s-v*s, p+u*s-v*s,p+u*s+v*s,p-u*s+v*s );

	//dc.SetLineWidth(2);
	dc.SetFillMode( e_FillModeWireframe );
	dc.SetColor( RGB(255,0,0),1 );
	dc.DrawQuad( p-u*s-v*s, p+u*s-v*s,p+u*s+v*s,p-u*s+v*s );

	dc.SetState(prevState);
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::EnableManipulator()
{
	ITransformManipulator *pManipulator = GetIEditor()->ShowTransformManipulator(true);

	Vec3 center = (m_points[0] + m_points[1] + m_points[2])/3.0f;
	Vec3 u = m_points[1] - m_points[0];
	Vec3 v = u.Cross(m_clipPlane.n);
	u.NormalizeSafe();
	v.NormalizeSafe();
	Matrix34 tm;
	tm.SetFromVectors( u,v,m_clipPlane.n,center );
	pManipulator->SetTransformation( COORDS_LOCAL,tm );
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::RecordUndo()
{
	if (CUndo::IsRecording())
		CUndo::Record( new CUndoBrushClipTool(this) );
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::UpdateClipPlane()
{
	if (m_pDragView)
	{
		Vec3 p1 = m_points[0];
		Vec3 p2 = m_points[1];

		Vec3 normal = m_pDragView->GetViewTM().GetColumn2();
		Vec3 dir = (p2-p1);
		dir.NormalizeSafe();

		RecordUndo();
		m_points[2] = m_points[0] + normal*m_fPlaneScale;
	}
	else
	{
		switch (m_nPlaneType)
		{
		case 0:
			// 3 points
			break;
		case 1:
			// 2 points
			{
				Vec3 u = m_points[1] - m_points[0];
				u.NormalizeSafe();
				Matrix33 tm = Matrix33::CreateOrientation(u,Vec3(0,0,1),DEG2RAD(m_fPlaneAngle));
				m_points[2] = m_points[0] + tm.GetColumn0();
			}
			break;
		};
	}

	m_clipPlane.SetPlane( m_points[0],m_points[1],m_points[2] );

	ITransformManipulator *pManipulator = GetIEditor()->GetTransformManipulator();
	if (pManipulator)
	{
		EnableManipulator();
	}

	UpdateClipBrushes();
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::SetPlaneAngle( float fScale )
{
	m_fPlaneAngle = fScale;
	UpdateClipPlane();
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::SetPlaneType( int nType )
{
	m_nPlaneType = nType;
	UpdateClipPlane();
}

//////////////////////////////////////////////////////////////////////////
int CBrushClipTool::GetNearestPoint( CViewport *view,CPoint point,Vec3 &pos )
{
	CRect rc;
	view->GetClientRect( rc );

	HitContext hit; 
	if (view->HitTest( point,hit ))
	{
		pos = hit.raySrc + hit.rayDir*hit.dist;
	}
	else
		pos = view->ViewToWorld(point);

	int n = 0;
	float d0 = pos.GetDistance(m_points[0]);
	float d1 = pos.GetDistance(m_points[1]);
	float d2 = pos.GetDistance(m_points[2]);
	if (m_nPlaneType == 0)
	{
		if (d0 < d1 && d0 < d2)
			n = 0;
		else if (d1 < d0 && d1 < d2)
			n = 1;
		else if (d2 < d0 && d2 < d1)
			n = 2;
	}
	if (m_nPlaneType == 1)
	{
		if (d1 < d0)
			n = 1;
	}
	
	pos = view->SnapToGrid(pos);

	return n;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushClipTool::OnLButtonDown( CViewport *view,UINT nFlags,CPoint point )
{
	m_mode = SelectMode;

	if (view->GetType() != ET_ViewportCamera)
	{
		GetIEditor()->ShowTransformManipulator(false);
		m_mouseDownPos = point;

		CRect rc;
		view->GetClientRect( rc );

		CPoint point1 = view->WorldToView(m_points[0]);
		CPoint point2 = view->WorldToView(m_points[1]);

		float d1 = (point.x-point1.x)*(point.x-point1.x) + (point.y-point1.y)*(point.y-point1.y);
		float d2 = (point.x-point2.x)*(point.x-point2.x) + (point.y-point2.y)*(point.y-point2.y);
		if (!rc.PtInRect(point1))
			d1 = 0;
		else if (!rc.PtInRect(point2))
			d2 = 0;
		if (d1 < d2)
			m_dragPoint = 0;
		else
			m_dragPoint = 1;

		view->BeginUndo();
		RecordUndo();

		m_pDragView = view;
		m_mode = DragMode;
		m_points[m_dragPoint] = view->SnapToGrid( view->ViewToWorld(point) );
	}
	else
	{
		GetIEditor()->ShowTransformManipulator(false);
		m_mouseDownPos = point;

		CRect rc;
		view->GetClientRect( rc );

		view->BeginUndo();
		RecordUndo();

		Vec3 pos;
		int n = GetNearestPoint( view,point,pos );
		m_dragPoint = n;
		m_points[m_dragPoint] = pos;
		m_mode = DragMode;
	}
	
	UpdateClipPlane();

	m_bMouseCaptured = true;
	view->CaptureMouse();

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushClipTool::OnMouseMove( CViewport *view,UINT nFlags,CPoint point )
{
	if (view->GetType() != ET_ViewportCamera)
	{
		view->SetCurrentCursor( STD_CURSOR_HIT );
	}
	
	if (m_mode == DragMode)
	{
		Vec3 pos;
		int n = GetNearestPoint( view,point,pos );
		m_points[m_dragPoint] = pos;
		UpdateClipPlane();
		return true;
	}
	else
	{
		Vec3 pos;
		m_dragPoint = GetNearestPoint( view,point,pos );
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushClipTool::OnLButtonDblClk( CViewport *view,UINT nFlags,CPoint point )
{
	bool bAltClick = CheckVirtualKey(VK_MENU);
	bool bCtrlClick = (nFlags & MK_CONTROL);
	bool bShiftClick = (nFlags & MK_SHIFT);
	if (bCtrlClick || bAltClick)
	{
		// Toggle object selection.
		HitContext hitInfo;
		if (view->HitTest( point,hitInfo ))
		{
			if (hitInfo.object && hitInfo.object->IsKindOf(RUNTIME_CLASS(CSolidBrushObject)))
			{
				if (bCtrlClick)
					GetIEditor()->GetObjectManager()->SelectObject( hitInfo.object );
				else
					GetIEditor()->GetObjectManager()->UnselectObject( hitInfo.object );
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushClipTool::OnLButtonUp( CViewport *view,UINT nFlags,CPoint point )
{
	bool bResult = false;
	if (m_mode == DragMode)
	{
		view->AcceptUndo( "Transform Clip Plane" );
		bResult = true;

		if (view->GetType() == ET_ViewportCamera)
			EnableManipulator();
	}
	if (m_bMouseCaptured)
	{
		view->ReleaseMouse();
	}
	m_pDragView = 0;

	m_mode = SelectMode;
	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::OnManipulatorDrag( CViewport *view,ITransformManipulator *pManipulator,CPoint &p0,CPoint &p1,const Vec3 &value )
{	
	int editMode = GetIEditor()->GetEditMode();
	if (editMode == eEditModeMove)
	{
		// Move brush.
		GetIEditor()->RestoreUndo();

		ITransformManipulator *pManipulator = GetIEditor()->ShowTransformManipulator(true);
		Matrix34 tm = pManipulator->GetTransformation(COORDS_LOCAL);
		tm.SetTranslation( tm.GetTranslation() + value );
		pManipulator->SetTransformation( COORDS_LOCAL,tm );

		RecordUndo();
		m_points[0] += value;
		m_points[1] += value;
		m_points[2] += value;

		CryLog( "%f,%f,%f",value.x,value.y,value.z);

		UpdateClipPlane();
	}
	else if (editMode == eEditModeRotate)
	{
		GetIEditor()->RestoreUndo();

		Matrix34 rotateTM = Matrix34::CreateRotationXYZ( Ang3(value) );
		Matrix34 tm = pManipulator->GetTransformation(COORDS_LOCAL);

		RecordUndo();
		Vec3 c = tm.GetTranslation();
		m_points[0] = c + rotateTM*(m_points[0]-c);
		m_points[1] = c + rotateTM*(m_points[1]-c);
		m_points[2] = c + rotateTM*(m_points[2]-c);

		tm = tm * rotateTM;
		pManipulator->SetTransformation( COORDS_LOCAL,tm );


		UpdateClipPlane();
	}
	else if (editMode == eEditModeScale)
	{
		float ax = p1.x - p0.x;
		float ay = p1.y - p0.y;
		p0 = p1;

		m_fPlaneScale = m_fPlaneScale - ay*0.1f;
		if (m_fPlaneScale < 0.1f)
			m_fPlaneScale = 0.1f;
	}

	UpdateClipBrushes();
}

//////////////////////////////////////////////////////////////////////////
bool CBrushClipTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	bool bProcessed = false;
	if (event == eMouseLDown)
	{
		bProcessed = OnLButtonDown( view,flags,point );
	}
	else if (event == eMouseLUp)
	{
		bProcessed = OnLButtonUp( view,flags,point );
	}
	else if (event == eMouseMove)
	{
		bProcessed = OnMouseMove( view,flags,point );
	}
	else if (event == eMouseLDblClick)
	{
		bProcessed = OnLButtonDblClk( view,flags,point );
	}

	// Not processed.
	return bProcessed;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushClipTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	if (nChar == VK_RETURN)
	{
		// Clip selected brushes by the clip plane.
		DoClip(1);
		return true;
	}
	// Not processed.
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushClipTool::OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	// Not processed.
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushClipTool::CheckVirtualKey( int virtualKey )
{
	GetAsyncKeyState(virtualKey);
	if (GetAsyncKeyState(virtualKey))
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::DoClip( int nType )
{
	CUndo undo( "Clip Brush" );
	CSelectionGroup *pSel = GetIEditor()->GetSelection();
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject *pObject = pSel->GetObject(i);
		if (pObject->IsKindOf(RUNTIME_CLASS(CSolidBrushObject)))
		{
			CSolidBrushObject *pBrushObject = (CSolidBrushObject*)pObject;
			SBrush *pBrush = pBrushObject->GetBrush();
			if (pBrush)
			{
				Matrix34 invTM = pObject->GetWorldTM();
				invTM.Invert();
				Plane pl;
				pl.SetPlane(invTM.TransformPoint(m_points[0]),invTM.TransformPoint(m_points[1]),invTM.TransformPoint(m_points[2]) );
				
				SBrushPlane splitPlane;
				splitPlane.normal = pl.n;
				splitPlane.dist = -pl.d;
				SBrush *pFront = 0;
				SBrush *pBack = 0;
				pBrush->SplitByPlane( &splitPlane,pFront,pBack );
				if (pFront || pBack)
					pBrushObject->StoreUndo( "Clip Brush" );
				// Assign back brush to the original object.
				if (nType == 0)
				{
					// Front.
					if (pFront)
						*pBrush = *pFront;
				}
				else if (nType == 1)
				{
					// Back.
					if (pBack)
						*pBrush = *pBack;
				}
				else if (nType == 2)
				{
					if (pBack && pFront)
					{
						*pBrush = *pBack;
						// Make new brush.
						CSolidBrushObject *pFrontSolid = (CSolidBrushObject*)GetIEditor()->GetObjectManager()->CloneObject( pBrushObject );
						if (pFrontSolid->GetBrush())
						{
							*pFrontSolid->GetBrush() = *pFront;
							pFrontSolid->InvalidateBrush();
						}
					}
				}

				delete pFront;
				delete pBack;
				pBrushObject->InvalidateBrush();
			}
		}
	}
	UpdateClipBrushes();
}

inline Plane TransformPlane( const Matrix34& m, const Plane& src )
{
	Plane pl;

	pl.n = m.TransformVector(src.n); 
	
	pl.n.x = m.m00*src.n.x + m.m01*src.n.y + m.m02*src.n.z;
	pl.n.y = m.m10*src.n.x + m.m11*src.n.y + m.m12*src.n.z;
	pl.n.z = m.m20*src.n.x + m.m21*src.n.y + m.m22*src.n.z;
	pl.d = m.m03*pl.n.x + m.m13*pl.n.y + m.m23*pl.n.z + src.d;

	/*
	float v0=src.n.x, v1=src.n.y, v2=src.n.z, v3=src.d;
	plDst.n.x = v0 * m(0,0) + v1 * m(1,0) + v2 * m(2,0);
	plDst.n.y = v0 * m(0,1) + v1 * m(1,1) + v2 * m(2,1);
	plDst.n.z = v0 * m(0,2) + v1 * m(1,2) + v2 * m(2,2);

	plDst.d = v0 * m(0,3) + v1 * m(1,3) + v2 * m(2,3) + v3;
	*/

	return pl;
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::UpdateClipBrushes()
{
	m_frontBrushes.clear();
	m_backBrushes.clear();

	CSelectionGroup *pSel = GetIEditor()->GetSelection();
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject *pObject = pSel->GetObject(i);
		if (pObject->IsKindOf(RUNTIME_CLASS(CSolidBrushObject)))
		{
			CSolidBrushObject *pBrushObject = (CSolidBrushObject*)pObject;
			SBrush *pBrush = pBrushObject->GetBrush();
			if (pBrush)
			{
				Matrix34 invTM = pObject->GetWorldTM();
				invTM.Invert();
				Plane pl;
				pl.SetPlane(invTM.TransformPoint(m_points[0]),invTM.TransformPoint(m_points[1]),invTM.TransformPoint(m_points[2]) );
				SBrushPlane splitPlane;
				splitPlane.normal = pl.n;
				splitPlane.dist = -pl.d;
				SBrush *pFront = 0;
				SBrush *pBack = 0;
				pBrush->SplitByPlane( &splitPlane,pFront,pBack );
				// Assign back brush to the original object.
				if (pBack)
				{
					pBack->BuildSolid();
					pBack->SetMatrix(pBrushObject->GetWorldTM());
					m_backBrushes.push_back( pBack );
				}
				if (pFront)
				{
					pFront->BuildSolid();
					pFront->SetMatrix(pBrushObject->GetWorldTM());
					m_frontBrushes.push_back( pFront );
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CBrushClipTool_ClassDesc : public CRefCountClassDesc
{
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }
	virtual REFGUID ClassID() { return CLIPBRUSH_TOOL_GUID; }
	virtual const char* ClassName() { return "EditTool.ClipBrush"; };
	virtual const char* Category() { return "Brush"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CBrushClipTool); }
};

//////////////////////////////////////////////////////////////////////////
void CBrushClipTool::RegisterTool( CRegistrationContext &rc )
{
	rc.pClassFactory->RegisterClass( g_BrushClipToolClass = new CBrushClipTool_ClassDesc );
}
