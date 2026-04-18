////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   Roadpanel.cpp
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
#include "RoadPanel.h"

#include "Viewport.h"
#include "Objects\\RoadObject.h"
#include "EditTool.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CEditRoadObjectTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CEditRoadObjectTool)

	CEditRoadObjectTool();

	// Ovverides from CEditTool
	bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	virtual void SetUserData( void *userData );
	
	virtual void BeginEditParams( IEditor *ie,int flags ) {};
	virtual void EndEditParams() {};

	virtual void Display( DisplayContext &dc ) {};
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags ) { return false; };

protected:
	virtual ~CEditRoadObjectTool();
	// Delete itself.
	void DeleteThis() { delete this; };

private:
	CRoadObject *m_Road;
	int m_currPoint;
	bool m_modifying;
	CPoint m_mouseDownPos;
	Vec3 m_pointPos;
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CEditRoadObjectTool,CEditTool)

//////////////////////////////////////////////////////////////////////////
CEditRoadObjectTool::CEditRoadObjectTool()
{
	m_Road = 0;
	m_currPoint = -1;
	m_modifying = false;
}

//////////////////////////////////////////////////////////////////////////
void CEditRoadObjectTool::SetUserData( void *userData )
{
	m_Road = (CRoadObject*)userData;
	assert( m_Road != 0 );

	// Modify Road undo.
	if (!CUndo::IsRecording())
	{
		CUndo ("Modify Road");
		m_Road->StoreUndo( "Road Modify" );
	}

	m_Road->SelectPoint(-1);
}

//////////////////////////////////////////////////////////////////////////
CEditRoadObjectTool::~CEditRoadObjectTool()
{
	if (m_Road)
	{
		m_Road->SelectPoint(-1);
	}
	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->CancelUndo();
}

bool CEditRoadObjectTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	if (nChar == VK_ESCAPE)
	{
		GetIEditor()->SetEditTool(0);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEditRoadObjectTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (!m_Road)
		return false;

	if (event == eMouseLDown)
	{
		m_mouseDownPos = point;
	}

	if (event == eMouseLDown || event == eMouseMove || event == eMouseLDblClick || event == eMouseLUp)
	{
		const Matrix34 &RoadTM = m_Road->GetWorldTM();

		float dist;

		Vec3 raySrc,rayDir;
		view->ViewToWorldRay( point,raySrc,rayDir );

		// Find closest point on the Road.
		int p1,p2;
		Vec3 intPnt;
		m_Road->GetNearestEdge( raySrc,rayDir,p1,p2,dist,intPnt );
		
		float fRoadCloseDistance = ROAD_CLOSE_DISTANCE * view->GetScreenScaleFactor(intPnt) * 0.01f;


		if ((flags & MK_CONTROL) && !m_modifying)
		{
			// If control we are editing edges..
			if (p1 >= 0 && p2 >= 0 && dist < fRoadCloseDistance+view->GetSelectionTolerance())
			{
				// Cursor near one of edited Road edges.
				view->ResetCursor();
				if (event == eMouseLDown)
				{
					view->CaptureMouse();
					m_modifying = true;
					GetIEditor()->BeginUndo();
					if (GetIEditor()->IsUndoRecording())
						m_Road->StoreUndo( "Make Point" );

					// If last edge, insert at end.
					if (p2 == 0)
						p2 = -1;
					
					// Create new point between nearest edge.
					// Put intPnt into local space of Road.
					intPnt = RoadTM.GetInverted().TransformPoint(intPnt);

					int index = m_Road->InsertPoint( p2,intPnt );
					m_Road->SelectPoint( index );

					// Set construction plane for view.
					m_pointPos = RoadTM.TransformPoint( m_Road->GetPoint(index) );
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation( m_pointPos );
					view->SetConstructionMatrix( COORDS_LOCAL,tm );
				}
			}
			return true;
		}

		int index = m_Road->GetNearestPoint( raySrc,rayDir,dist );
		if (index >= 0 && dist < fRoadCloseDistance+view->GetSelectionTolerance())
		{
			// Cursor near one of edited Road points.
			view->ResetCursor();
			if (event == eMouseLDown)
			{
				if (!m_modifying)
				{
					m_Road->SelectPoint( index );
					m_modifying = true;
					view->CaptureMouse();
					GetIEditor()->BeginUndo();
					
					// Set construction plance for view.
					m_pointPos = RoadTM.TransformPoint( m_Road->GetPoint(index) );
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
				m_Road->RemovePoint( index );
				m_Road->SelectPoint( -1 );
			}
		}
		else
		{
			if (event == eMouseLDown)
			{
				m_Road->SelectPoint( -1 );
			}
		}

		if (m_modifying && event == eMouseLUp)
		{
			// Accept changes.
			m_modifying = false;
			//m_Road->SelectPoint( -1 );
			view->ReleaseMouse();
			m_Road->CalcBBox();
			m_Road->SetRoadSectors();

			if (GetIEditor()->IsUndoRecording())
				GetIEditor()->AcceptUndo( "Road Modify" );
		}

		if (m_modifying && event == eMouseMove)
		{
			// Move selected point point.
			Vec3 p1 = view->MapViewToCP(m_mouseDownPos);
			Vec3 p2 = view->MapViewToCP(point);
			Vec3 v = view->GetCPVector(p1,p2);

			if (m_Road->GetSelectedPoint() >= 0)
			{
				Vec3 wp = m_pointPos;
				Vec3 newp = wp + v;
				if (GetIEditor()->GetAxisConstrains() == AXIS_TERRAIN)
				{
					// Keep height.
					newp = view->MapViewToCP(point);
					//float z = wp.z - GetIEditor()->GetTerrainElevation(wp.x,wp.y);
					//newp.z = GetIEditor()->GetTerrainElevation(newp.x,newp.y) + z;
					//newp.z = GetIEditor()->GetTerrainElevation(newp.x,newp.y) + ROAD_Z_OFFSET;
					newp.z += ROAD_Z_OFFSET;
				}

				if (newp.x != 0 && newp.y != 0 && newp.z != 0)
				{
					newp = view->SnapToGrid(newp);
					// Put newp into local space of Road.
					Matrix34 invRoadTM = RoadTM;
					invRoadTM.Invert();
					newp = invRoadTM.TransformPoint(newp);

					if (GetIEditor()->IsUndoRecording())
						m_Road->StoreUndo( "Move Point" );
					m_Road->SetPoint( m_Road->GetSelectedPoint(),newp );
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


// CRoadPanel dialog

IMPLEMENT_DYNAMIC(CRoadPanel, CDialog)

//////////////////////////////////////////////////////////////////////////
CRoadPanel::CRoadPanel( CWnd* pParent /* = NULL */)
	: CDialog(CRoadPanel::IDD, pParent)
{
	Create( IDD,AfxGetMainWnd() );
}

CRoadPanel::~CRoadPanel()
{
}

void CRoadPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ALIGN_HM, m_alignHmapButton);
	DDX_Control(pDX, IDC_EDIT_SHAPE, m_editRoadButton);
}


BEGIN_MESSAGE_MAP(CRoadPanel, CDialog)
	ON_BN_CLICKED(IDC_ALIGN_HM, OnAlignHeightMap)
	ON_BN_CLICKED(IDC_DEFAULT_WIDTH, OnDefaultWidth)
END_MESSAGE_MAP()


// CRoadPanel message handlers

BOOL CRoadPanel::OnInitDialog()
{
	__super::OnInitDialog();

	m_width.Create( this,IDC_WIDTH,CNumberCtrl::LEFTALIGN );
	m_width.SetRange(0.0f, 99999.0f);

	m_angle.Create( this,IDC_ANGLE );
	m_angle.SetRange(-25.0f, 25.0f);
	GetDlgItem(IDC_ANGLE)->EnableWindow( FALSE );
	GetDlgItem(IDC_WIDTH)->EnableWindow( FALSE );
	GetDlgItem(IDC_DEFAULT_WIDTH)->EnableWindow( FALSE );
	m_angle.SetUpdateCallback( functor(*this,&CRoadPanel::OnUpdateParams) );
	m_width.SetUpdateCallback( functor(*this,&CRoadPanel::OnUpdateParams) );

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CRoadPanel::SetRoad( CRoadObject *Road )
{
	assert( Road );
	m_Road = Road;

	if (Road->GetPointCount() > 1)
		m_editRoadButton.SetToolClass( RUNTIME_CLASS(CEditRoadObjectTool),m_Road );
	else
		m_editRoadButton.EnableWindow( FALSE );

	CString str;
	str.Format( "Num Points: %d",Road->GetPointCount() );
	if (GetDlgItem(IDC_NUM_POINTS))
		GetDlgItem(IDC_NUM_POINTS)->SetWindowText( str );
}

//////////////////////////////////////////////////////////////////////////
void CRoadPanel::OnAlignHeightMap()
{
	m_Road->AlignHeightMap();
}

//////////////////////////////////////////////////////////////////////////
void CRoadPanel::SelectPoint( int index )
{
	if(index < 0)
	{
		GetDlgItem(IDC_ANGLE)->EnableWindow( FALSE );
		GetDlgItem(IDC_WIDTH)->EnableWindow( FALSE );
		GetDlgItem(IDC_DEFAULT_WIDTH)->EnableWindow( FALSE );
		GetDlgItem(IDC_SELECTED_POINT)->SetWindowText("Selected Point: no selection");
	}
	else
	{
		GetDlgItem(IDC_ANGLE)->EnableWindow( TRUE );
		float val = m_Road->GetPointAngle();
		m_angle.SetValue(val);

		GetDlgItem(IDC_DEFAULT_WIDTH)->EnableWindow( TRUE );

		bool isDefault = m_Road->IsPointDefaultWidth();
		((CButton *)GetDlgItem(IDC_DEFAULT_WIDTH))->SetCheck( isDefault  );
		GetDlgItem(IDC_WIDTH)->EnableWindow( !isDefault );
		val = m_Road->GetPointWidth();
		m_width.SetValue(val);

		char out[256];
		sprintf(out,"Selected Point: %d",index+1);
		GetDlgItem(IDC_SELECTED_POINT)->SetWindowText(out);
	}
	m_width.SetValue(m_Road->GetPointWidth());
}

void CRoadPanel::OnUpdateParams( CNumberCtrl *ctrl )
{
	m_Road->SetPointAngle(m_angle.GetValue());
	m_Road->SetPointWidth(m_width.GetValue());
}

void CRoadPanel::OnDefaultWidth()
{
	BOOL isDefault = ((CButton *)GetDlgItem(IDC_DEFAULT_WIDTH))->GetCheck( );
	GetDlgItem(IDC_WIDTH)->EnableWindow( !isDefault );
	m_Road->PointDafaultWidthIs(isDefault);
	m_width.SetValue(m_Road->GetPointWidth());
}