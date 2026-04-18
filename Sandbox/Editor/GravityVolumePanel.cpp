////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   GravityVolumepanel.cpp
//  Version:     v1.00
//  Created:     25/07/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Resource.h"
#include "GravityVolumePanel.h"

#include "Viewport.h"
#include "Objects\\GravityVolumeObject.h"
#include "EditTool.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CEditGravityVolumeObjectTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CEditGravityVolumeObjectTool)

	CEditGravityVolumeObjectTool();

	// Ovverides from CEditTool
	bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	virtual void SetUserData( void *userData );
	
	virtual void BeginEditParams( IEditor *ie,int flags ) {};
	virtual void EndEditParams() {};

	virtual void Display( DisplayContext &dc ) {};
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags ) { return false; };

protected:
	virtual ~CEditGravityVolumeObjectTool();
	// Delete itself.
	void DeleteThis() { delete this; };

private:
	CGravityVolumeObject *m_GravityVolume;
	int m_currPoint;
	bool m_modifying;
	CPoint m_mouseDownPos;
	Vec3 m_pointPos;
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CEditGravityVolumeObjectTool,CEditTool)

//////////////////////////////////////////////////////////////////////////
CEditGravityVolumeObjectTool::CEditGravityVolumeObjectTool()
{
	m_GravityVolume = 0;
	m_currPoint = -1;
	m_modifying = false;
}

//////////////////////////////////////////////////////////////////////////
void CEditGravityVolumeObjectTool::SetUserData( void *userData )
{
	m_GravityVolume = (CGravityVolumeObject*)userData;
	assert( m_GravityVolume != 0 );

	// Modify GravityVolume undo.
	if (!CUndo::IsRecording())
	{
		CUndo ("Modify GravityVolume");
		m_GravityVolume->StoreUndo( "GravityVolume Modify" );
	}

	m_GravityVolume->SelectPoint(-1);
}

//////////////////////////////////////////////////////////////////////////
CEditGravityVolumeObjectTool::~CEditGravityVolumeObjectTool()
{
	if (m_GravityVolume)
	{
		m_GravityVolume->SelectPoint(-1);
	}
	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->CancelUndo();
}

bool CEditGravityVolumeObjectTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	if (nChar == VK_ESCAPE)
	{
		GetIEditor()->SetEditTool(0);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEditGravityVolumeObjectTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (!m_GravityVolume)
		return false;

	if (event == eMouseLDown)
	{
		m_mouseDownPos = point;
	}

	if (event == eMouseLDown || event == eMouseMove || event == eMouseLDblClick || event == eMouseLUp)
	{
		const Matrix34 &GravityVolumeTM = m_GravityVolume->GetWorldTM();

		float dist;

		Vec3 raySrc,rayDir;
		view->ViewToWorldRay( point,raySrc,rayDir );

		// Find closest point on the GravityVolume.
		int p1,p2;
		Vec3 intPnt;
		m_GravityVolume->GetNearestEdge( raySrc,rayDir,p1,p2,dist,intPnt );
		
		float fGravityVolumeCloseDistance = GravityVolume_CLOSE_DISTANCE * view->GetScreenScaleFactor(intPnt) * 0.01f;


		if ((flags & MK_CONTROL) && !m_modifying)
		{
			// If control we are editing edges..
			if (p1 >= 0 && p2 >= 0 && dist < fGravityVolumeCloseDistance+view->GetSelectionTolerance())
			{
				// Cursor near one of edited GravityVolume edges.
				view->ResetCursor();
				if (event == eMouseLDown)
				{
					view->CaptureMouse();
					m_modifying = true;
					GetIEditor()->BeginUndo();
					if (GetIEditor()->IsUndoRecording())
						m_GravityVolume->StoreUndo( "Make Point" );

					// If last edge, insert at end.
					if (p2 == 0)
						p2 = -1;
					
					// Create new point between nearest edge.
					// Put intPnt into local space of GravityVolume.
					intPnt = GravityVolumeTM.GetInverted().TransformPoint(intPnt);

					int index = m_GravityVolume->InsertPoint( p2,intPnt );
					m_GravityVolume->SelectPoint( index );

					// Set construction plane for view.
					m_pointPos = GravityVolumeTM.TransformPoint( m_GravityVolume->GetPoint(index) );
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation( m_pointPos );
					view->SetConstructionMatrix( COORDS_LOCAL,tm );
				}
			}
			return true;
		}

		int index = m_GravityVolume->GetNearestPoint( raySrc,rayDir,dist );
		if (index >= 0 && dist < fGravityVolumeCloseDistance+view->GetSelectionTolerance())
		{
			// Cursor near one of edited GravityVolume points.
			view->ResetCursor();
			if (event == eMouseLDown)
			{
				if (!m_modifying)
				{
					m_GravityVolume->SelectPoint( index );
					m_modifying = true;
					view->CaptureMouse();
					GetIEditor()->BeginUndo();
					
					// Set construction plance for view.
					m_pointPos = GravityVolumeTM.TransformPoint( m_GravityVolume->GetPoint(index) );
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation( m_pointPos );
					view->SetConstructionMatrix( COORDS_LOCAL,tm );
				}
			}

			//GetNearestEdge

			if (event == eMouseLDblClick)
			{
				m_modifying = false;
				m_GravityVolume->RemovePoint( index );
				m_GravityVolume->SelectPoint( -1 );
			}
		}
		else
		{
			if (event == eMouseLDown)
			{
				m_GravityVolume->SelectPoint( -1 );
			}
		}

		if (m_modifying && event == eMouseLUp)
		{
			// Accept changes.
			m_modifying = false;
			//m_GravityVolume->SelectPoint( -1 );
			view->ReleaseMouse();
			m_GravityVolume->CalcBBox();

			if (GetIEditor()->IsUndoRecording())
				GetIEditor()->AcceptUndo( "GravityVolume Modify" );
		}

		if (m_modifying && event == eMouseMove)
		{
			// Move selected point point.
			Vec3 p1 = view->MapViewToCP(m_mouseDownPos);
			Vec3 p2 = view->MapViewToCP(point);
			Vec3 v = view->GetCPVector(p1,p2);

			if (m_GravityVolume->GetSelectedPoint() >= 0)
			{
				Vec3 wp = m_pointPos;
				Vec3 newp = wp + v;
				if (GetIEditor()->GetAxisConstrains() == AXIS_TERRAIN)
				{
					// Keep height.
					newp = view->MapViewToCP(point);
					//float z = wp.z - GetIEditor()->GetTerrainElevation(wp.x,wp.y);
					//newp.z = GetIEditor()->GetTerrainElevation(newp.x,newp.y) + z;
					//newp.z = GetIEditor()->GetTerrainElevation(newp.x,newp.y) + GravityVolume_Z_OFFSET;
					newp.z += GravityVolume_Z_OFFSET;
				}

				if (newp.x != 0 && newp.y != 0 && newp.z != 0)
				{
					newp = view->SnapToGrid(newp);

					// Put newp into local space of GravityVolume.
					Matrix34 invGravityVolumeTM = GravityVolumeTM;
					invGravityVolumeTM.Invert();
					newp = invGravityVolumeTM.TransformPoint(newp);

					if (GetIEditor()->IsUndoRecording())
						m_GravityVolume->StoreUndo( "Move Point" );
					m_GravityVolume->SetPoint( m_GravityVolume->GetSelectedPoint(),newp );
				}
			}
		}
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


// CGravityVolumePanel dialog

IMPLEMENT_DYNAMIC(CGravityVolumePanel, CDialog)

//////////////////////////////////////////////////////////////////////////
CGravityVolumePanel::CGravityVolumePanel( CWnd* pParent /* = NULL */)
	: CDialog(CGravityVolumePanel::IDD, pParent)
{
	Create( IDD,AfxGetMainWnd() );
}

CGravityVolumePanel::~CGravityVolumePanel()
{
}

void CGravityVolumePanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_SHAPE, m_editGravityVolumeButton);
}


BEGIN_MESSAGE_MAP(CGravityVolumePanel, CDialog)
END_MESSAGE_MAP()


// CGravityVolumePanel message handlers

BOOL CGravityVolumePanel::OnInitDialog()
{
	__super::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CGravityVolumePanel::SetGravityVolume( CGravityVolumeObject *GravityVolume )
{
	assert( GravityVolume );
	m_GravityVolume = GravityVolume;

	if (GravityVolume->GetPointCount() > 1)
		m_editGravityVolumeButton.SetToolClass( RUNTIME_CLASS(CEditGravityVolumeObjectTool),m_GravityVolume );
	else
		m_editGravityVolumeButton.EnableWindow( FALSE );

	CString str;
	str.Format( "Num Points: %d",GravityVolume->GetPointCount() );
	GetDlgItem(IDC_NUM_POINTS)->SetWindowText( str );
}

//////////////////////////////////////////////////////////////////////////
void CGravityVolumePanel::SelectPoint( int index )
{
	if(index < 0)
	{
		GetDlgItem(IDC_SELECTED_POINT)->SetWindowText("Selected Point: no selection");
	}
	else
	{
		char out[256];
		sprintf(out,"Selected Point: %d",index+1);
		GetDlgItem(IDC_SELECTED_POINT)->SetWindowText(out);
	}
}
