////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   shapepanel.cpp
//  Version:     v1.00
//  Created:     28/2/2002 by Timur.
//  Compilers:   Visual C++.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ShapePanel.h"

#include "Viewport.h"
#include "Objects\\ShapeObject.h"
#include "EditTool.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CEditShapeObjectTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CEditShapeObjectTool)

	CEditShapeObjectTool();

	// Ovverides from CEditTool
	bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	virtual void SetUserData( void *userData );
	
	virtual void BeginEditParams( IEditor *ie,int flags ) {};
	virtual void EndEditParams() {};

	virtual void Display( DisplayContext &dc ) {};
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );

protected:
	virtual ~CEditShapeObjectTool();
	void DeleteThis() { delete this; };

private:
	CShapeObject *m_shape;
	bool m_modifying;
	CPoint m_mouseDownPos;
	Vec3 m_pointPos;
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CEditShapeObjectTool,CEditTool)

//////////////////////////////////////////////////////////////////////////
CEditShapeObjectTool::CEditShapeObjectTool()
{
	m_shape = 0;
	m_modifying = false;
}

//////////////////////////////////////////////////////////////////////////
void CEditShapeObjectTool::SetUserData( void *userData )
{
	m_shape = (CShapeObject*)userData;
	assert( m_shape != 0 );

	// Modify shape undo.
	if (!CUndo::IsRecording())
	{
		CUndo ("Modify Shape");
		m_shape->StoreUndo( "Shape Modify" );
	}

	m_shape->SelectPoint(-1);
	if (m_shape->UseAxisHelper())
	{
		((CObjectManager*)GetIEditor()->GetObjectManager())->HideTransformManipulators();
		GetIEditor()->ShowTransformManipulator(false);
	}
}

//////////////////////////////////////////////////////////////////////////
CEditShapeObjectTool::~CEditShapeObjectTool()
{
	if (m_shape)
	{
		m_shape->SelectPoint(-1);
	}
	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->CancelUndo();
}

bool CEditShapeObjectTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	if (nChar == VK_ESCAPE)
	{
		GetIEditor()->SetEditTool(0);
	}
	else if(nChar == VK_DELETE)
	{
		if (m_shape)
		{
			int	sel = m_shape->GetSelectedPoint();
			if(!m_modifying && sel >= 0 && sel < m_shape->GetPointCount())
			{
				GetIEditor()->BeginUndo();
				if (GetIEditor()->IsUndoRecording())
					m_shape->StoreUndo( "Delete Point" );

				m_shape->RemovePoint( sel );
				m_shape->SelectPoint( -1 );
				m_shape->CalcBBox();

				if (GetIEditor()->IsUndoRecording())
					GetIEditor()->AcceptUndo( "Shape Modify" );
			}
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEditShapeObjectTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (!m_shape)
		return false;

	if (event == eMouseLDown)
	{
		m_mouseDownPos = point;
	}

	if (event == eMouseLDown || event == eMouseMove || event == eMouseLDblClick || event == eMouseLUp)
	{
		const Matrix34 &shapeTM = m_shape->GetWorldTM();

		/*
		float fShapeCloseDistance = SHAPE_CLOSE_DISTANCE;
		Vec3 pos = view->ViewToWorld( point );
		if (pos.x == 0 && pos.y == 0 && pos.z == 0)
		{
			// Find closest point on the shape.
			fShapeCloseDistance = SHAPE_CLOSE_DISTANCE * view->GetScreenScaleFactor(pos) * 0.01f;
		}
		else
			fShapeCloseDistance = SHAPE_CLOSE_DISTANCE * view->GetScreenScaleFactor(pos) * 0.01f;
		*/


		float dist;

		Vec3 raySrc,rayDir;
		view->ViewToWorldRay( point,raySrc,rayDir );

		// Find closest point on the shape.
		int p1,p2;
		Vec3 intPnt;
		m_shape->GetNearestEdge( raySrc,rayDir,p1,p2,dist,intPnt );
		
		float fShapeCloseDistance = SHAPE_CLOSE_DISTANCE * view->GetScreenScaleFactor(intPnt) * 0.01f;


		if ((flags & MK_CONTROL) && !m_modifying)
		{
			// If control we are editing edges..
			if (p1 >= 0 && p2 >= 0 && dist < fShapeCloseDistance+view->GetSelectionTolerance())
			{
				// Cursor near one of edited shape edges.
				view->ResetCursor();
				if (event == eMouseLDown)
				{
					view->CaptureMouse();
					m_modifying = true;
					GetIEditor()->BeginUndo();
					if (GetIEditor()->IsUndoRecording())
						m_shape->StoreUndo( "Make Point" );

					// If last edge, insert at end.
					if (p2 == 0)
						p2 = -1;
					
					// Create new point between nearest edge.
					// Put intPnt into local space of shape.
					intPnt = shapeTM.GetInverted().TransformPoint(intPnt);

					int index = m_shape->InsertPoint( p2,intPnt );
					m_shape->SelectPoint( index );

					// Set construction plane for view.
					m_pointPos = shapeTM.TransformPoint( m_shape->GetPoint(index) );
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation( m_pointPos );
					view->SetConstructionMatrix( COORDS_LOCAL,tm );
				}
			}
			return true;
		}

		int index = m_shape->GetNearestPoint( raySrc,rayDir,dist );
		if(dist > fShapeCloseDistance+view->GetSelectionTolerance())
			index = -1;
		bool	hitPoint(index != -1);

		// Edit the selected point based on the axis gizmo.
		if(m_shape->UseAxisHelper() && index == -1 && m_shape->GetSelectedPoint() != -1 && !m_modifying)
		{
			CAxisHelper&	axisHelper(m_shape->GetSelelectedPointAxisHelper());
			HitContext hc;
			hc.view = view;
			hc.point2d = point;
			view->ViewToWorldRay(point,hc.raySrc,hc.rayDir);

			Vec3	selectedPointPos = shapeTM.TransformPoint(m_shape->GetPoint(m_shape->GetSelectedPoint()));

			Matrix34	axis;
			axis.SetTranslationMat(selectedPointPos);
			if(axisHelper.HitTest(axis, hc))
			{
				if (event == eMouseLDown)
				{
					m_modifying = true;
					view->CaptureMouse();
					GetIEditor()->BeginUndo();

					if (GetIEditor()->IsUndoRecording())
						m_shape->StoreUndo( "Move Point" );

					// Set construction plance for view.
					m_pointPos = selectedPointPos;
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation(selectedPointPos);
					view->SetConstructionMatrix( COORDS_LOCAL,tm );

					// Hit axis gizmo.
					GetIEditor()->SetAxisConstrains( (AxisConstrains)hc.axis );
					view->SetAxisConstrain( hc.axis );
				}
				index = m_shape->GetSelectedPoint();
				view->SetCurrentCursor( STD_CURSOR_MOVE );
			}
		}

		if (index >= 0)
		{
			// Cursor near one of edited shape points.
			if (event == eMouseLDown)
			{
				if (!m_modifying)
				{
					m_shape->SelectPoint( index );
					m_modifying = true;
					view->CaptureMouse();
					GetIEditor()->BeginUndo();

					if (GetIEditor()->IsUndoRecording())
						m_shape->StoreUndo( "Move Point" );
					
					// Set construction plance for view.
					m_pointPos = shapeTM.TransformPoint( m_shape->GetPoint(index) );
					Matrix34 tm;
					tm.SetIdentity();
					tm.SetTranslation( m_pointPos );
					view->SetConstructionMatrix( COORDS_LOCAL,tm );
				}
			}

			//GetNearestEdge

			// Delete points with double click.
			// Test if the use hit the point too so that interacting with the
			// axis helper does not allow to delete points.
			if (event == eMouseLDblClick && hitPoint)
			{
				CUndo undo( "Remove Point" );
				m_modifying = false;
				m_shape->RemovePoint( index );
				m_shape->SelectPoint( -1 );
			}

			if(hitPoint)
				view->SetCurrentCursor( STD_CURSOR_HIT );
		}
		else
		{
			if (event == eMouseLDown)
			{
				// Missed a point, deselect all.
				m_shape->SelectPoint( -1 );
			}
			view->ResetCursor();
		}

		if (m_modifying && event == eMouseLUp)
		{
			// Accept changes.
			m_modifying = false;
//			m_shape->SelectPoint( -1 );
			view->ReleaseMouse();
			m_shape->CalcBBox();

			if (GetIEditor()->IsUndoRecording())
				GetIEditor()->AcceptUndo( "Shape Modify" );
		}

		if (m_modifying && event == eMouseMove)
		{
			// Move selected point point.
			Vec3 p1 = view->MapViewToCP(m_mouseDownPos);
			Vec3 p2 = view->MapViewToCP(point);
			Vec3 v = view->GetCPVector(p1,p2);

			if (m_shape->GetSelectedPoint() >= 0)
			{
				Vec3 wp = m_pointPos;
				Vec3 newp = wp + v;
				if (GetIEditor()->GetAxisConstrains() == AXIS_TERRAIN)
				{
					// Keep height.
					newp = view->MapViewToCP(point);
					//float z = wp.z - GetIEditor()->GetTerrainElevation(wp.x,wp.y);
					//newp.z = GetIEditor()->GetTerrainElevation(newp.x,newp.y) + z;
					//newp.z = GetIEditor()->GetTerrainElevation(newp.x,newp.y) + SHAPE_Z_OFFSET;
					newp.z += m_shape->GetShapeZOffset();
				}

				if (newp.x != 0 && newp.y != 0 && newp.z != 0)
				{
					newp = view->SnapToGrid(newp);
					// Put newp into local space of shape.
					Matrix34 invShapeTM = shapeTM;
					invShapeTM.Invert();
					newp = invShapeTM.TransformPoint(newp);

					m_shape->SetPoint( m_shape->GetSelectedPoint(),newp );
				}

				view->SetCurrentCursor( STD_CURSOR_MOVE );
			}
		}

		/*
		Vec3 raySrc,rayDir;
		view->ViewToWorldRay( point,raySrc,rayDir );
		CBaseObject *hitObj = GetIEditor()->GetObjectManager()->HitTest( raySrc,rayDir,view->GetSelectionTolerance() );
		*/
		return true;
	}
	return false;
}







//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CSplitShapeObjectTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CSplitShapeObjectTool)

	CSplitShapeObjectTool();

	// Ovverides from CEditTool
	bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	virtual void SetUserData( void *userData );
	virtual void BeginEditParams( IEditor *ie,int flags ) {};
	virtual void EndEditParams() {};
	virtual void Display( DisplayContext &dc ) {};
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );

protected:
	virtual ~CSplitShapeObjectTool();
	void DeleteThis() { delete this; };

private:
	CShapeObject *m_shape;
	int m_curPoint;
};

IMPLEMENT_DYNCREATE(CSplitShapeObjectTool,CEditTool)

//////////////////////////////////////////////////////////////////////////
CSplitShapeObjectTool::CSplitShapeObjectTool()
{
	m_shape = 0;
	m_curPoint = -1;
}

//////////////////////////////////////////////////////////////////////////
void CSplitShapeObjectTool::SetUserData( void *userData )
{
	m_curPoint = -1;
	m_shape = (CShapeObject*)userData;
	assert( m_shape != 0 );

	// Modify shape undo.
	if (!CUndo::IsRecording())
	{
		CUndo ("Modify Shape");
		m_shape->StoreUndo( "Shape Modify" );
	}
}

//////////////////////////////////////////////////////////////////////////
CSplitShapeObjectTool::~CSplitShapeObjectTool()
{
	if (m_shape)
		m_shape->SetSplitPoint( -1,Vec3(0,0,0), -1 );
	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->CancelUndo();
}

//////////////////////////////////////////////////////////////////////////
bool CSplitShapeObjectTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	if (nChar == VK_ESCAPE)
		GetIEditor()->SetEditTool(0);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CSplitShapeObjectTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (!m_shape)
		return false;

	if (event == eMouseLDown || event == eMouseMove)
	{
		const Matrix34 &shapeTM = m_shape->GetWorldTM();

		float dist;
		Vec3 raySrc,rayDir;
		view->ViewToWorldRay( point,raySrc,rayDir );

		// Find closest point on the shape.
		int p1,p2;
		Vec3 intPnt;
		m_shape->GetNearestEdge( raySrc,rayDir,p1,p2,dist,intPnt );
		
		float fShapeCloseDistance = SHAPE_CLOSE_DISTANCE * view->GetScreenScaleFactor(intPnt) * 0.01f;

		// If control we are editing edges..
		if (p1 >= 0 && p2 >= 0 && dist < fShapeCloseDistance+view->GetSelectionTolerance())
		{
			view->SetCurrentCursor( STD_CURSOR_HIT );
			// If last edge, insert at end.
			if (p2 == 0)
				p2 = -1;
			// Put intPnt into local space of shape.
			intPnt = shapeTM.GetInverted().TransformPoint(intPnt);

			if (event == eMouseMove)
			{
				if(m_curPoint==0)
					m_shape->SetSplitPoint( p2,intPnt, 1);
			}
			if (event == eMouseLDown)
			{
				m_curPoint++;
				m_curPoint = m_shape->SetSplitPoint( p2, intPnt, m_curPoint) - 1;
				if(m_curPoint==1)
				{
					GetIEditor()->BeginUndo();
					m_shape->Split();
					if (GetIEditor()->IsUndoRecording())
						GetIEditor()->AcceptUndo( "Split shape" );
					GetIEditor()->SetEditTool(0);
				}
			}
		}
		else
		{
			view->ResetCursor();
			if(m_curPoint>-1)
			{
				m_shape->SetSplitPoint( -2, Vec3(0,0,0), 0 );
			}
		}
		return true;
	}
	return false;
}





//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


// CShapePanel dialog

IMPLEMENT_DYNAMIC(CShapePanel, CDialog)

//////////////////////////////////////////////////////////////////////////
CShapePanel::CShapePanel( CWnd* pParent /* = NULL */)
	: CDialog(CShapePanel::IDD, pParent)
{
}

CShapePanel::~CShapePanel()
{
}

void CShapePanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PICK, m_pickButton);
	DDX_Control(pDX, IDC_SELECT, m_selectButton);
	DDX_Control(pDX, IDC_EDIT_SHAPE, m_editShapeButton);
	DDX_Control(pDX, IDC_SPLIT, m_splitShapeButton);
	DDX_Control(pDX, IDC_ENTITIES, m_entities);
	DDX_Control(pDX, IDC_REVERSE, m_reverseButton);
}


BEGIN_MESSAGE_MAP(CShapePanel, CDialog)
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_SELECT, OnBnClickedSelect)
	ON_BN_CLICKED(IDC_REMOVE, OnBnClickedRemove)
	ON_BN_CLICKED(IDC_USE_TRANSFORM_GIZMO, OnBnClickedUseTransformGizmo)
	ON_LBN_DBLCLK(IDC_ENTITIES, OnLbnDblclkEntities)
	ON_BN_CLICKED(IDC_REVERSE, OnBnClickedReverse)
END_MESSAGE_MAP()


// CShapePanel message handlers

//////////////////////////////////////////////////////////////////////////
BOOL CShapePanel::OnInitDialog()
{
	__super::OnInitDialog();

	if (m_pickButton.m_hWnd)
		m_pickButton.SetPickCallback( this,"Pick Entity" );
	if (m_entities.m_hWnd)
		m_entities.SetBkColor( RGB(0xE0,0xE0,0xE0) );

	m_useTransforGizmo = AfxGetApp()->GetProfileInt(AfxGetApp()->m_pszAppName, "ShapePanel_UseTransformGizmo", FALSE) == TRUE;

	if (GetDlgItem(IDC_USE_TRANSFORM_GIZMO))
	{
		if(m_useTransforGizmo)
			CheckDlgButton(IDC_USE_TRANSFORM_GIZMO, BST_CHECKED);
		else
			CheckDlgButton(IDC_USE_TRANSFORM_GIZMO, BST_UNCHECKED);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CShapePanel::OnDestroy()
{
	AfxGetApp()->WriteProfileInt(AfxGetApp()->m_pszAppName, "ShapePanel_UseTransformGizmo", m_useTransforGizmo ? TRUE : FALSE);
	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CShapePanel::SetShape( CShapeObject *shape )
{
	assert( shape );
	m_shape = shape;

	if (shape->GetPointCount() > 1)
	{
		m_editShapeButton.SetToolClass( RUNTIME_CLASS(CEditShapeObjectTool),m_shape );
		m_editShapeButton.EnableWindow( TRUE );
	}
	else
		m_editShapeButton.EnableWindow( FALSE );

	if (shape->GetPointCount() > 2)
	{
		m_splitShapeButton.SetToolClass( RUNTIME_CLASS(CSplitShapeObjectTool),m_shape );
		m_splitShapeButton.EnableWindow( TRUE );
	}
	else
		m_splitShapeButton.EnableWindow( FALSE );

	ReloadEntities();

	CString str;
	str.Format( "Num Points: %d",shape->GetPointCount() );
	GetDlgItem(IDC_NUM_POINTS)->SetWindowText( str );

	m_shape->SetUseAxisHelper(m_useTransforGizmo);
}

//////////////////////////////////////////////////////////////////////////
void CShapePanel::OnPick( CBaseObject *picked )
{
	assert( m_shape );
	CUndo undo("[Shape] Add Entity");
	m_shape->AddEntity( picked );
	ReloadEntities();
//	m_entityName.SetWindowText( picked->GetName() );
}

//////////////////////////////////////////////////////////////////////////
bool CShapePanel::OnPickFilter( CBaseObject *picked )
{
	assert( picked != 0 );
	return picked->GetType() == OBJTYPE_ENTITY;
}

//////////////////////////////////////////////////////////////////////////
void CShapePanel::OnCancelPick()
{
}

//////////////////////////////////////////////////////////////////////////
void CShapePanel::OnBnClickedSelect()
{
	assert( m_shape );
	int sel = m_entities.GetCurSel();
	if (sel != LB_ERR)
	{
		CBaseObject *obj = m_shape->GetEntity(sel);
		if (obj)
		{
			GetIEditor()->ClearSelection();
			GetIEditor()->SelectObject( obj );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapePanel::ReloadEntities()
{
	if (!m_shape)
		return;
	if (!m_entities.m_hWnd)
		return;

	m_entities.ResetContent();
	for (int i = 0; i < m_shape->GetEntityCount(); i++)
	{
		CBaseObject *obj = m_shape->GetEntity(i);
		if (obj)
			m_entities.AddString( obj->GetName() );
		else
			m_entities.AddString( "<Null>" );
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapePanel::OnBnClickedRemove()
{
	assert( m_shape );
	int sel = m_entities.GetCurSel();
	if (sel != LB_ERR)
	{
		CUndo undo("[Shape] Remove Entity");
		if (sel < m_shape->GetEntityCount())
			m_shape->RemoveEntity(sel);
		ReloadEntities();
	}
}

//////////////////////////////////////////////////////////////////////////
void CShapePanel::OnBnClickedReverse()
{
	assert( m_shape );
	CUndo undo("[Shape] Reverse Shape");
	m_shape->ReverseShape();
}

//////////////////////////////////////////////////////////////////////////
void CShapePanel::OnBnClickedUseTransformGizmo()
{
	m_useTransforGizmo = IsDlgButtonChecked(IDC_USE_TRANSFORM_GIZMO) != 0;
	m_shape->SetUseAxisHelper(m_useTransforGizmo);
}

//////////////////////////////////////////////////////////////////////////
void CShapePanel::OnLbnDblclkEntities()
{
	// Select current entity.
	OnBnClickedSelect();
}






//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CMergeShapeObjectsTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CMergeShapeObjectsTool)

	CMergeShapeObjectsTool();

	// Ovverides from CEditTool
	bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	virtual void SetUserData( void *userData );
	virtual void BeginEditParams( IEditor *ie,int flags ) {};
	virtual void EndEditParams() {};
	virtual void Display( DisplayContext &dc ) {};
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );

protected:
	virtual ~CMergeShapeObjectsTool();
	void DeleteThis() { delete this; };

	int m_curPoint;
	CShapeObject * m_shape;

private:
};

IMPLEMENT_DYNCREATE(CMergeShapeObjectsTool,CEditTool)

//////////////////////////////////////////////////////////////////////////
CMergeShapeObjectsTool::CMergeShapeObjectsTool()
{
	m_curPoint = -1;
	m_shape = 0;
}

//////////////////////////////////////////////////////////////////////////
void CMergeShapeObjectsTool::SetUserData( void *userData )
{
	/*
	// Modify shape undo.
	if (!CUndo::IsRecording())
	{
		CUndo ("Modify Shape");
		m_shape->StoreUndo( "Shape Modify" );
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
CMergeShapeObjectsTool::~CMergeShapeObjectsTool()
{
	if(m_shape)
		m_shape->SetMergeIndex( -1 );
	m_shape = 0;
	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->CancelUndo();
}

//////////////////////////////////////////////////////////////////////////
bool CMergeShapeObjectsTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	if (nChar == VK_ESCAPE)
		GetIEditor()->SetEditTool(0);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMergeShapeObjectsTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	//return true;
	if (event == eMouseLDown || event == eMouseMove)
	{
		CSelectionGroup *pSel = GetIEditor()->GetSelection();

		bool foundSel = false;

		for (int i = 0; i < pSel->GetCount(); i++)
		{
			CBaseObject *pObj = pSel->GetObject(i);
			if(!pObj->IsKindOf(RUNTIME_CLASS(CShapeObject)))
				continue;

			CShapeObject * shape = (CShapeObject*) pObj;

			const Matrix34 &shapeTM = shape->GetWorldTM();

			float dist;
			Vec3 raySrc,rayDir;
			view->ViewToWorldRay( point,raySrc,rayDir );

			// Find closest point on the shape.
			int p1,p2;
			Vec3 intPnt;
			shape->GetNearestEdge( raySrc,rayDir,p1,p2,dist,intPnt );
			
			float fShapeCloseDistance = SHAPE_CLOSE_DISTANCE * view->GetScreenScaleFactor(intPnt) * 0.01f;

			// If control we are editing edges..
			if (p1 >= 0 && p2 >= 0 && dist < fShapeCloseDistance+view->GetSelectionTolerance())
			{
				view->SetCurrentCursor( STD_CURSOR_HIT );
				if (event == eMouseLDown)
				{
					if(m_curPoint==0)
					{
						if(shape!=m_shape)
							m_curPoint++;
						shape->SetMergeIndex( p1 );
					}

					if(m_curPoint==-1)
					{
						m_shape = shape;
						m_curPoint++;
						shape->SetMergeIndex( p1 );
					}
					
					if(m_curPoint==1)
					{
						GetIEditor()->BeginUndo();
						if(m_shape)
							m_shape->Merge(shape);
						if (GetIEditor()->IsUndoRecording())
							GetIEditor()->AcceptUndo( "Merge shapes" );
						GetIEditor()->SetEditTool(0);
					}
				}
				foundSel = true;
				break;
			}
		}

		if(!foundSel)
			view->ResetCursor();

		return true;
	}
	return false;
}





//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// CShapeMultySelPanel dialog

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CShapeMultySelPanel, CDialog)

//////////////////////////////////////////////////////////////////////////
CShapeMultySelPanel::CShapeMultySelPanel( CWnd* pParent /* = NULL */)
	: CDialog(CShapeMultySelPanel::IDD, pParent)
{
	Create( IDD,AfxGetMainWnd() );
}

CShapeMultySelPanel::~CShapeMultySelPanel()
{
}

void CShapeMultySelPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MERGE, m_mergeButton);
}


BEGIN_MESSAGE_MAP(CShapeMultySelPanel, CDialog)

END_MESSAGE_MAP()


// CShapeMultySelPanel message handlers

//////////////////////////////////////////////////////////////////////////
BOOL CShapeMultySelPanel::OnInitDialog()
{
	__super::OnInitDialog();

	m_mergeButton.SetToolClass( RUNTIME_CLASS(CMergeShapeObjectsTool), 0 );
	//m_editShapeButton.EnableWindow( TRUE );


	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


//////////////////////////////////////////////////////////////////////////
// RopePanel
//////////////////////////////////////////////////////////////////////////

CRopePanel::CRopePanel( CWnd* pParent ) : CShapePanel( pParent )
{
}

//////////////////////////////////////////////////////////////////////////
void CRopePanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_SHAPE, m_editShapeButton);
	DDX_Control(pDX, IDC_SPLIT, m_splitShapeButton);
}
