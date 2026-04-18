`////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   BrushTextureTool.cpp
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain Modification Tool implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <InitGuid.h>
#include "BrushTextureTool.h"
#include "..\Viewport.h"

#include "Objects\ObjectManager.h"
#include "Brush.h"
#include "SolidBrushObject.h"
#include "Grid.h"
#include "Include\ITransformManipulator.h"
#include "EditMode\VertexMode.h"

#include <IRenderAuxGeom.h>

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CBrushTextureTool,CEditTool)

class CBrushTextureToolPanel;

float CBrushTextureTool::m_fGizmoScale = 1.0f;

namespace 
{
	IClassDesc* g_BrushTextureToolClass = NULL;
	int g_BrushTextureToolPanelId = 0;
	CBrushTextureToolPanel *g_BrushTextureToolPanel = 0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CBrushTextureToolPanel : public CDialog
{
public:
	CNumberCtrl m_offset[2];
	CNumberCtrl m_scale[2];
	CNumberCtrl m_rotate;

	CNumberCtrl m_MatId;

	CNumberCtrl m_fitTiling[2];
	CBrushTextureTool* m_pBrushTextureTool;
	CButton m_pickSelectedBtn;
	CButton m_absoluteBtn;
	CButton m_relativeBtn;

	CCustomButton m_btn[8];

	SBrushTexInfo m_lastInfo;

	//////////////////////////////////////////////////////////////////////////
	CBrushTextureToolPanel(CBrushTextureTool *pTool,CWnd* pParent = NULL) : CDialog(IDD, pParent)
	{
		m_pBrushTextureTool = pTool;
		Create( IDD,pParent );
	}

	void EnableTextureInfo( bool bEnable );
	void SetTextureInfo( SBrushTexInfo &texInfo );
	bool IsRelative() const { return m_relativeBtn.GetCheck() == BST_CHECKED; }

	// Dialog Data
	enum { IDD = IDD_PANEL_BRUSH_TEXTURETOOL };

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);

	virtual BOOL OnInitDialog();
	afx_msg void OnValueChange();
	afx_msg void OnValueUpdate();
	afx_msg void OnTextureFit();
	afx_msg void OnViewAlign() {};
	afx_msg void OnNormalAlign() {};
	afx_msg void OnReset() {};
	afx_msg void OnApply();
	afx_msg void OnSelectMatID() { m_pBrushTextureTool->SelectMatID( (int)m_MatId.GetValue() ); };
	afx_msg void OnAssignMatID() { m_pBrushTextureTool->AssignMatID( (int)m_MatId.GetValue() ); };
	afx_msg void OnRelative();
	afx_msg void OnAbsolute();

	DECLARE_MESSAGE_MAP()
};

void CBrushTextureToolPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control( pDX,IDC_TEXTURE_FIT,m_btn[0] );
	DDX_Control( pDX,IDC_TEXTURE_VIEWALIGN,m_btn[1] );
	DDX_Control( pDX,IDC_TEXTURE_NORMALALIGN,m_btn[2] );
	DDX_Control( pDX,IDC_TEXTURE_RESET,m_btn[3] );
	DDX_Control( pDX,IDC_TEXTURE_APPLY,m_btn[4] );
	DDX_Control( pDX,IDC_SELECT_MATID,m_btn[5] );
	DDX_Control( pDX,IDC_ASSIGN_MATID,m_btn[6] );
	DDX_Control( pDX,IDC_TEXTURE_PICKSELECTED,m_pickSelectedBtn );
	DDX_Control( pDX,IDC_ABSOLUTE,m_absoluteBtn );
	DDX_Control( pDX,IDC_RELATIVE,m_relativeBtn );
}


BEGIN_MESSAGE_MAP(CBrushTextureToolPanel, CDialog)
	ON_EN_UPDATE( IDC_TEXTURE_OFFSETX,OnValueUpdate )
	ON_EN_UPDATE( IDC_TEXTURE_OFFSETY,OnValueUpdate )
	ON_EN_UPDATE( IDC_TEXTURE_SCALEX,OnValueUpdate )
	ON_EN_UPDATE( IDC_TEXTURE_SCALEY,OnValueUpdate )
	ON_EN_UPDATE( IDC_TEXTURE_ROTATE,OnValueUpdate )

	ON_EN_CHANGE( IDC_TEXTURE_OFFSETX,OnValueChange )
	ON_EN_CHANGE( IDC_TEXTURE_OFFSETY,OnValueChange )
	ON_EN_CHANGE( IDC_TEXTURE_SCALEX,OnValueChange )
	ON_EN_CHANGE( IDC_TEXTURE_SCALEY,OnValueChange )
	ON_EN_CHANGE( IDC_TEXTURE_ROTATE,OnValueChange )

	ON_BN_CLICKED( IDC_TEXTURE_FIT,OnTextureFit )
	ON_BN_CLICKED( IDC_TEXTURE_VIEWALIGN,OnViewAlign )
	ON_BN_CLICKED( IDC_TEXTURE_NORMALALIGN,OnNormalAlign )
	ON_BN_CLICKED( IDC_TEXTURE_RESET,OnReset )
	ON_BN_CLICKED( IDC_TEXTURE_APPLY,OnApply )
	ON_BN_CLICKED( IDC_SELECT_MATID,OnSelectMatID )
	ON_BN_CLICKED( IDC_ASSIGN_MATID,OnAssignMatID )
	ON_BN_CLICKED( IDC_RELATIVE,OnRelative )
	ON_BN_CLICKED( IDC_ABSOLUTE,OnAbsolute )
END_MESSAGE_MAP()


//////////////////////////////////////////////////////////////////////////
BOOL CBrushTextureToolPanel::OnInitDialog()
{
	BOOL bRes = __super::OnInitDialog();

	m_absoluteBtn.SetCheck( BST_CHECKED );
	m_relativeBtn.SetCheck( BST_UNCHECKED );

	m_offset[0].Create( this,IDC_TEXTURE_OFFSETX );
	m_offset[1].Create( this,IDC_TEXTURE_OFFSETY );
	m_scale[0].Create( this,IDC_TEXTURE_SCALEX );
	m_scale[1].Create( this,IDC_TEXTURE_SCALEY );
	m_rotate.Create( this,IDC_TEXTURE_ROTATE );

	m_fitTiling[0].Create( this,IDC_TEXTURE_TILEX );
	m_fitTiling[1].Create( this,IDC_TEXTURE_TILEY );
	m_fitTiling[0].SetValue(1);
	m_fitTiling[1].SetValue(1);

	m_offset[0].SetInternalPrecision(3);
	m_offset[1].SetInternalPrecision(3);
	m_scale[0].SetInternalPrecision(3);
	m_scale[1].SetInternalPrecision(3);

	m_offset[0].SetRange( -1000,1000 );
	m_offset[1].SetRange( -1000,1000 );
	m_scale[0].SetRange( -1000,1000 );
	m_scale[1].SetRange( -1000,1000 );
	m_rotate.SetRange( -1000,1000 );

	m_offset[0].EnableUndo( "Tex OffsetX Modified" );
	m_offset[1].EnableUndo( "Tex OffsetY Modified" );
	m_scale[0].EnableUndo( "Tex ScaleX Modified" );
	m_scale[1].EnableUndo( "Tex ScaleY Modified" );
	m_rotate.EnableUndo( "Tex Rotate Modified" );

	m_MatId.Create( this,IDC_MATID );
	m_MatId.SetInteger(true);
	m_MatId.SetRange(0,32);

	m_pickSelectedBtn.SetCheck( BST_CHECKED );

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureToolPanel::EnableTextureInfo( bool bEnable )
{
	if (m_pickSelectedBtn.GetCheck() != BST_CHECKED)
		return;
	BOOL on = bEnable?TRUE:FALSE;
	m_offset[0].EnableWindow(on);
	m_offset[1].EnableWindow(on);
	m_scale[0].EnableWindow(on);
	m_scale[1].EnableWindow(on);
	m_rotate.EnableWindow(on);
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureToolPanel::OnRelative()
{
	m_offset[0].SetValue( 0 );
	m_offset[1].SetValue( 0 );
	m_scale[0].SetValue( 0 );
	m_scale[1].SetValue( 0 );
	m_rotate.SetValue( 0 );
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureToolPanel::OnAbsolute()
{
	SetTextureInfo( m_lastInfo );
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureToolPanel::SetTextureInfo( SBrushTexInfo &texInfo )
{
	if (m_pickSelectedBtn.GetCheck() != BST_CHECKED)
		return;
	
	m_lastInfo = texInfo;

	EnableTextureInfo(true);
	if (!IsRelative())
	{
		m_offset[0].SetValue( texInfo.shift[0] );
		m_offset[1].SetValue( texInfo.shift[1] );
		m_scale[0].SetValue( texInfo.scale[0] );
		m_scale[1].SetValue( texInfo.scale[1] );
		m_rotate.SetValue( texInfo.rotate );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureToolPanel::OnValueChange()
{
	SBrushTexInfo texInfo;
	texInfo.shift[0] = m_offset[0].GetValue();
	texInfo.shift[1] = m_offset[1].GetValue();
	texInfo.scale[0] = m_scale[0].GetValue();
	texInfo.scale[1] = m_scale[1].GetValue();
	texInfo.rotate = m_rotate.GetValue();
	m_pBrushTextureTool->ApplyTextureMapping( texInfo,IsRelative() );
	if (IsRelative())
	{
		m_offset[0].SetValue( 0 );
		m_offset[1].SetValue( 0 );
		m_scale[0].SetValue( 0 );
		m_scale[1].SetValue( 0 );
		m_rotate.SetValue( 0 );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureToolPanel::OnValueUpdate()
{
	SBrushTexInfo texInfo;
	texInfo.shift[0] = m_offset[0].GetValue();
	texInfo.shift[1] = m_offset[1].GetValue();
	texInfo.scale[0] = m_scale[0].GetValue();
	texInfo.scale[1] = m_scale[1].GetValue();
	texInfo.rotate = m_rotate.GetValue();
	m_pBrushTextureTool->ApplyTextureMapping( texInfo,IsRelative() );
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureToolPanel::OnApply()
{
	if (IsRelative())
		return;
	SBrushTexInfo texInfo;
	texInfo.shift[0] = m_offset[0].GetValue();
	texInfo.shift[1] = m_offset[1].GetValue();
	texInfo.scale[0] = m_scale[0].GetValue();
	texInfo.scale[1] = m_scale[1].GetValue();
	texInfo.rotate = m_rotate.GetValue();
	if (!IsRelative() && texInfo.scale[0] == 0 && texInfo.scale[1] == 0)
		return;
	m_pBrushTextureTool->ApplyTextureMapping( texInfo,false );
}


//////////////////////////////////////////////////////////////////////////
void CBrushTextureToolPanel::OnTextureFit()
{
	float tileu = m_fitTiling[0].GetValue();
	float tilev = m_fitTiling[1].GetValue();
	m_pBrushTextureTool->FitTexture( tileu,tilev );
}

//////////////////////////////////////////////////////////////////////////
// CBrushTextureTool implementation.
//////////////////////////////////////////////////////////////////////////
CBrushTextureTool::CBrushTextureTool()
{
	m_pClassDesc = g_BrushTextureToolClass;
	SetStatusText( _T("Texture Brush(s)") );
	m_mode = SelectMode;
}

//////////////////////////////////////////////////////////////////////////
CBrushTextureTool::~CBrushTextureTool()
{
	for (int i = 0; i < m_objects.size(); i++)
	{
		m_objects[i]->EndSubObjectSelection();
	}
	GetIEditor()->ShowTransformManipulator(false);
}

//////////////////////////////////////////////////////////////////////////
bool CBrushTextureTool::Activate( CEditTool *pPreviousTool )
{
	// Remember selection here.
	CSelectionGroup *pSel = GetIEditor()->GetSelection();
	if (pSel->IsEmpty())
	{
		return false;
	}
	SetParentTool( pPreviousTool );

	GetIEditor()->GetObjectManager()->EndEditParams();
	((CObjectManager*)GetIEditor()->GetObjectManager())->HideTransformManipulators();

	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject *pObject = pSel->GetObject(i);

		AddObjectToSelection( pObject );

		/*
		ITransformManipulator *pManipulator = GetIEditor()->ShowTransformManipulator(true);
		// In local space orient axis gizmo by first object.
		Matrix34 tm;
		tm.SetIdentity();
		tm.SetTranslation( pObject->GetWorldPos() );
		pManipulator->SetTransformation( COORDS_LOCAL,tm );
		pManipulator->SetTransformation( COORDS_PARENT,tm );
		break;
		*/
	}
	if (m_objects.empty())
		return false;

	g_SubObjSelOptions.displayType = SO_DISPLAY_GEOMETRY;
	OnSelectionChange();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureTool::AddObjectToSelection( CBaseObject *pObject )
{
	if (!pObject->IsKindOf(RUNTIME_CLASS(CSolidBrushObject)))
		return;
	CSolidBrushObject *pBrush = (CSolidBrushObject*)pObject;
	if (!pBrush->GetBrush())
		return;
	if (!pBrush->StartSubObjSelection( SO_ELEM_FACE ))
		return;
	m_objects.push_back( pBrush );
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureTool::BeginEditParams( IEditor *ie,int flags )
{
	if (!g_BrushTextureToolPanelId)
	{
		g_BrushTextureToolPanel = new CBrushTextureToolPanel(this,AfxGetMainWnd());
		g_BrushTextureToolPanelId = GetIEditor()->AddRollUpPage( ROLLUP_OBJECTS,"Texture Mapping",g_BrushTextureToolPanel );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureTool::EndEditParams()
{
	if (g_BrushTextureToolPanelId)
	{
		GetIEditor()->RemoveRollUpPage(ROLLUP_OBJECTS,g_BrushTextureToolPanelId);
		g_BrushTextureToolPanelId = 0;
	}
	g_BrushTextureToolPanel = 0;
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureTool::Display( DisplayContext &dc )
{
	/*
	// Draw clipping plane.
	if (dc.flags & DISPLAY_2D)
	{
	}
	else
	{

	}
	*/

	int prevState = dc.GetState();
	dc.CullOff();
	dc.DepthWriteOff();
	dc.DepthTestOff();
	// Draw Construction plane.

	if (m_numSelectedFaces == 1)
	{
		ITransformManipulator *pManipulator = GetIEditor()->GetTransformManipulator();
		if (!pManipulator)
			return;
		Matrix34 tm = pManipulator->GetTransformation(COORDS_LOCAL);

		SBrushFace *pSelFace = 0;
		CSolidBrushObject *pSelBrushObject = 0;
		for (int i = 0; i < m_objects.size(); i++)
		{
			SBrush *pBrush = m_objects[i]->GetBrush();
			// Clear face selection.
			for (int j = 0; j < pBrush->m_Faces.size(); j++)
			{
				if (pBrush->m_Faces[j]->m_bSelected)
				{
					pSelBrushObject = m_objects[i];
					pSelFace = pBrush->m_Faces[j];
					break;
				}
			}
		}
		if (pSelFace)
		{
			Vec3 tu,tv;
			Vec3 facemin,facemax;
			pSelFace->CalcTextureBasis(tu,tv);
			pSelFace->m_Poly->CalcBounds( facemin,facemax );

			dc.PushMatrix( pSelBrushObject->GetWorldTM() );

			SBrushTexInfo &texInfo = pSelFace->m_TexInfo;

			Vec3 u = tu * texInfo.scale[0];
			Vec3 v = tv * texInfo.scale[1];

			/*
			Vec3 p = (facemin + facemax) / 2.0f;
			float minu = 1e20f;
			// find vertex with smallest uv coords.
			for (int i = 0; i < pSelFace->m_Poly->m_Pts.size(); i++)
			{
				if (pSelFace->m_Poly->m_Pts[i].st[0] < minu)
				{
					minu = pSelFace->m_Poly->m_Pts[i].st[0];
					p = pSelFace->m_Poly->m_Pts[i].xyz;
				}
			}
			*/
			Vec3 p = pSelFace->m_Plane.normal*pSelFace->m_Plane.dist;
			p = p - tu*texInfo.shift[0]*texInfo.scale[0] - tv*texInfo.shift[1]*texInfo.scale[1];

			Vec3 texBasisNormal = tu.Cross(tv);
			Matrix34 texbasisTM;
			texbasisTM.SetFromVectors( tu,tv,texBasisNormal,pSelBrushObject->GetWorldTM().TransformPoint(p) );
			pManipulator->SetTransformation(COORDS_LOCAL,texbasisTM );

			float s = 1.0f;

			dc.SetLineWidth(3);
			dc.SetColor( RGB(255,255,0),1 );

			dc.DrawLine( p,p+u );
			dc.DrawLine( p+u,p+u+v );
			dc.DrawLine( p+u+v,p+v );
			dc.DrawLine( p,p+v );
			
			// Draw vertical line mark.
			Vec3 topleft = p;
			Vec3 topright = p+u;
			Vec3 topmid = (topleft + topright) / 2.0f;
			dc.DrawLine( topmid,topmid - v*0.2f );

			dc.SetLineWidth(0);

			dc.PopMatrix();
		}
	}

	dc.SetState(prevState);
}

//////////////////////////////////////////////////////////////////////////
bool CBrushTextureTool::HitTest( CViewport *view,CPoint point,CRect rc,int nFlags )
{
	bool bModifySelection = (nFlags & (SO_HIT_SELECT_ADD|SO_HIT_SELECT_REMOVE)) != 0;

	if (nFlags & SO_HIT_SELECT)
		GetIEditor()->BeginUndo();

	bool bAnyHit = false;

	HitContext hit;
	hit.point2d = point;
	hit.rect = rc;
	hit.view = view;
	hit.nSubObjFlags = nFlags;
	view->ViewToWorldRay(point,hit.raySrc,hit.rayDir);

	for (int i = 0; i < m_objects.size(); i++)
	{
		if (m_objects[i]->HitTest(hit))
		{
			if (!(nFlags & SO_HIT_SELECT))
				return true;
			bAnyHit = true;
		}
	}
	
	if (nFlags & SO_HIT_SELECT)
	{
		GetIEditor()->AcceptUndo( "Select Brush Face" );
		OnSelectionChange();
	}

	return bAnyHit;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushTextureTool::OnLButtonDown( CViewport *view,UINT nFlags,CPoint point )
{
	m_mode = SelectMode;

	// Save the mouse down position
	m_mouseDownPos = point;

	view->ResetSelectionRegion();

	// Get contrl key status.
	bool bAltClick = CheckVirtualKey(VK_MENU);
	bool bCtrlClick = (nFlags & MK_CONTROL);
	bool bShiftClick = (nFlags & MK_SHIFT);
	bool bModifySelection = bCtrlClick || bAltClick;
	bool bAddSelect = bCtrlClick;
	bool bUnselect = bAltClick;

	bool bLockSelection = GetIEditor()->IsSelectionLocked();

	if (!bLockSelection)
	{
		int nHitFlags = SO_HIT_SELECT|SO_HIT_POINT;
		if (bAddSelect)
			nHitFlags |= SO_HIT_SELECT_ADD;
		else if (bUnselect)
			nHitFlags |= SO_HIT_SELECT_REMOVE;

		CPoint hitSize = CPoint(5,5);
		CRect hitRC( point.x-hitSize.x,point.y-hitSize.y,point.x+hitSize.x,point.y+hitSize.y );
		bool bHitSelected = HitTest( view,point,hitRC,nHitFlags );

		if (!bHitSelected || bModifySelection)
		{
			m_mode = DragSelectRectMode;
			m_bMouseCaptured = true;
			view->CaptureMouse();
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushTextureTool::OnLButtonUp( CViewport *view,UINT nFlags,CPoint point )
{
	bool bResult = false;
	if (m_mode == DragMode)
	{
		view->AcceptUndo( "Transform Clip Plane" );
		bResult = true;
	}
	if (m_mode == DragSelectRectMode && (!GetIEditor()->IsSelectionLocked()))
	{
		bool bAddSelection = (nFlags & MK_CONTROL);
		bool bUnselect = CheckVirtualKey(VK_MENU);
		CRect selectRect = view->GetSelectionRectangle();
		if (!selectRect.IsRectEmpty())
		{
			// Ignore too small rectangles.
			if (selectRect.Width() > 5 && selectRect.Height() > 5)
			{
				int nHitFlags = SO_HIT_SELECT;
				if (bAddSelection)
					nHitFlags |= SO_HIT_SELECT_ADD;
				else if (bUnselect)
					nHitFlags |= SO_HIT_SELECT_REMOVE;
				HitTest( view,point,selectRect,nHitFlags );
			}
		}
	}
	if (m_bMouseCaptured)
	{
		view->ReleaseMouse();
	}
	view->ResetCursor();

	m_mode = SelectMode;
	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushTextureTool::OnMouseMove( CViewport *view,int nFlags, CPoint point)
{
	if (!(nFlags & MK_RBUTTON))
	{
		//SetObjectCursor(view,0);
	}

	if (m_mode == DragSelectRectMode)
	{
		// Ignore select when selection locked.
		if (GetIEditor()->IsSelectionLocked())
			return true;

		CRect rc( m_mouseDownPos,point );
		if (GetIEditor()->GetEditMode() == eEditModeSelectArea)
			view->OnDragSelectRectangle( CPoint(rc.left,rc.top),CPoint(rc.right,rc.bottom),false );
		else
		{
			view->SetSelectionRectangle( rc.TopLeft(),rc.BottomRight() );
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureTool::OnManipulatorDrag( CViewport *view,ITransformManipulator *pManipulator,CPoint &p0,CPoint &p1,const Vec3 &value )
{	
	int editMode = GetIEditor()->GetEditMode();
	if (editMode == eEditModeMove)
	{
		// Move brush.
		GetIEditor()->RestoreUndo();

		Vec3 pos1 = view->MapViewToCP(p0);
		Vec3 pos2 = view->MapViewToCP(p1);
		Vec3 v = view->GetCPVector(pos1,pos2);

		pManipulator = GetIEditor()->ShowTransformManipulator(true);
		Matrix34 tm = pManipulator->GetTransformation(COORDS_LOCAL);
		tm.SetTranslation( tm.GetTranslation() + value );
		pManipulator->SetTransformation( COORDS_LOCAL,tm );

		SBrushTexInfo texInfo;
		texInfo.shift[0] = -v.x;
		texInfo.shift[1] = v.z;
		texInfo.scale[0] = 0;
		texInfo.scale[1] = 0;
		texInfo.rotate  = 0;
		ApplyTextureMapping( texInfo,true );
	}
	else if (editMode == eEditModeRotate)
	{
		GetIEditor()->RestoreUndo();

		Matrix34 rotateTM = Matrix34::CreateRotationXYZ( Ang3(value) );
		Matrix34 tm = pManipulator->GetTransformation(COORDS_LOCAL);
		tm = tm * rotateTM;
		pManipulator->SetTransformation( COORDS_LOCAL,tm );
	}
	else if (editMode == eEditModeScale)
	{
		GetIEditor()->RestoreUndo();

		float ax = p1.x - p0.x;
		float ay = p1.y - p0.y;

		m_fGizmoScale = m_fGizmoScale - ay*0.01f;
		if (m_fGizmoScale < 0.1f)
			m_fGizmoScale = 0.1f;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBrushTextureTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
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

	// Not processed.
	return bProcessed;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushTextureTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	// Not processed.
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushTextureTool::OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	// Not processed.
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBrushTextureTool::CheckVirtualKey( int virtualKey )
{
	GetAsyncKeyState(virtualKey);
	if (GetAsyncKeyState(virtualKey))
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureTool::OnSelectionChange()
{
	if (!g_BrushTextureToolPanel)
		return;

	SBrushFace *pLastSelectedFace = 0;
	int i;
	m_numSelectedFaces = 0;
	for (i = 0; i < m_objects.size(); i++)
	{
		SBrush *pBrush = m_objects[i]->GetBrush();
		for (int j = 0; j < pBrush->m_Faces.size(); j++)
		{
			if (pBrush->m_Faces[j]->m_bSelected)
			{
				pLastSelectedFace = pBrush->m_Faces[j];
				m_numSelectedFaces++;
			}
		}
	}
	if (m_numSelectedFaces == 0)
		g_BrushTextureToolPanel->EnableTextureInfo(false);
	else if (m_numSelectedFaces == 1)
	{
		g_BrushTextureToolPanel->SetTextureInfo( pLastSelectedFace->m_TexInfo );
	}
	else
	{
		SBrushTexInfo ti;
		ti.shift[0] = ti.shift[1] = 0;
		ti.scale[0] = ti.scale[1] = 0;
		ti.rotate = 0;
		g_BrushTextureToolPanel->SetTextureInfo( ti );
	}
	if (pLastSelectedFace)
	{
		g_BrushTextureToolPanel->m_MatId.SetValue( pLastSelectedFace->m_nMatID );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBrushTextureTool::HasSelectedFaces( CSolidBrushObject *pObject )
{
	for (int i = 0; i < m_objects.size(); i++)
	{
		SBrush *pBrush = m_objects[i]->GetBrush();
		for (int j = 0; j < pBrush->m_Faces.size(); j++)
		{
			if (pBrush->m_Faces[j]->m_bSelected)
			{
				return true;
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureTool::ApplyTextureMapping( SBrushTexInfo &texInfo,bool bAdd )
{
	if (!m_numSelectedFaces)
		return;
	
	if (m_numSelectedFaces == 0)
		return;

	bool bUndoStarted = false;
	if (!CUndo::IsRecording())
	{
		bUndoStarted = true;
		GetIEditor()->BeginUndo();
	}
	else
		GetIEditor()->RestoreUndo();

	for (int i = 0; i < m_objects.size(); i++)
	{
		if (!HasSelectedFaces(m_objects[i]))
			continue;

		m_objects[i]->StoreUndo( "Texture Mapping Modified" );
		
		SBrush *pBrush = m_objects[i]->GetBrush();
		for (int j = 0; j < pBrush->m_Faces.size(); j++)
		{
			if (pBrush->m_Faces[j]->m_bSelected)
			{
				if (bAdd)
				{
					SBrushTexInfo ti = pBrush->m_Faces[j]->m_TexInfo;
					ti.shift[0] += texInfo.shift[0];
					ti.shift[1] += texInfo.shift[1];
					ti.scale[0] += texInfo.scale[0];
					ti.scale[1] += texInfo.scale[1];
					ti.rotate += texInfo.rotate;
					if (ti.scale[0] != 0 && ti.scale[1] != 0)
						pBrush->m_Faces[j]->m_TexInfo = ti;
				}
				else
				{
					if (texInfo.scale[0] != 0 && texInfo.scale[1] != 0)
						pBrush->m_Faces[j]->m_TexInfo = texInfo;
				}
			}
		}
		pBrush->RecalcTexCoords();
		m_objects[i]->InvalidateBrush(false);
	}

	if (bUndoStarted)
		GetIEditor()->AcceptUndo( "Texture Mapping Modified" );
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureTool::FitTexture( float tileU,float tileV )
{
	if (m_numSelectedFaces == 0)
		return;

	CUndo undo( "Fit Texture" );

	for (int i = 0; i < m_objects.size(); i++)
	{
		if (!HasSelectedFaces(m_objects[i]))
			continue;

		m_objects[i]->StoreUndo( "Fit Texture" );
		SBrush *pBrush = m_objects[i]->GetBrush();
		for (int j = 0; j < pBrush->m_Faces.size(); j++)
		{
			if (pBrush->m_Faces[j]->m_bSelected)
			{
				pBrush->m_Faces[j]->FitTexture(tileU,tileV);
			}
		}
		pBrush->RecalcTexCoords();
		m_objects[i]->InvalidateBrush(false);
	}
	OnSelectionChange();
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureTool::SelectMatID( int matId )
{
	CUndo undo( "Select MatID" );
	for (int i = 0; i < m_objects.size(); i++)
	{
		m_objects[i]->StoreUndo( "Select MatID" );
		SBrush *pBrush = m_objects[i]->GetBrush();
		pBrush->ClearSelection();
		for (int j = 0; j < pBrush->m_Faces.size(); j++)
		{
			if (pBrush->m_Faces[j]->m_nMatID == matId)
				pBrush->m_Faces[j]->m_bSelected = true;
		}
		pBrush->BuildSolid();
	}
	OnSelectionChange();
}

//////////////////////////////////////////////////////////////////////////
void CBrushTextureTool::AssignMatID( int matId )
{
	if (!m_numSelectedFaces)
		return;

	if (m_numSelectedFaces == 0)
		return;

	CUndo undo( "Assign MatID" );

	for (int i = 0; i < m_objects.size(); i++)
	{
		if (!HasSelectedFaces(m_objects[i]))
			continue;

		m_objects[i]->StoreUndo( "Assign MatID" );

		SBrush *pBrush = m_objects[i]->GetBrush();
		for (int j = 0; j < pBrush->m_Faces.size(); j++)
		{
			if (pBrush->m_Faces[j]->m_bSelected)
			{
				pBrush->m_Faces[j]->m_nMatID = matId;
			}
			pBrush->BuildSolid();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CBrushTextureTool_ClassDesc : public CRefCountClassDesc
{
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }
	virtual REFGUID ClassID() { return BRUSHTEXTURE_TOOL_GUID; }
	virtual const char* ClassName() { return "EditTool.TextureBrush"; };
	virtual const char* Category() { return "Brush"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CBrushTextureTool); }
};

//////////////////////////////////////////////////////////////////////////
void CBrushTextureTool::RegisterTool( CRegistrationContext &rc )
{
	rc.pClassFactory->RegisterClass( g_BrushTextureToolClass = new CBrushTextureTool_ClassDesc );
}
