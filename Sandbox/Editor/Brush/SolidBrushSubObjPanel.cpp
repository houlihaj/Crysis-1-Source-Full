////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SolidBrushSubObjPanel.h
//  Version:     v1.00
//  Created:     11/1/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: CSolidBrushSubObjPanel implementation file.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "SolidBrushSubObjPanel.h"

#include "SolidBrushObject.h"

// CSolidBrushSubObjPanel dialog

IMPLEMENT_DYNAMIC(CSolidBrushSubObjPanel, CDialog)
CSolidBrushSubObjPanel::CSolidBrushSubObjPanel(CWnd* pParent /*=NULL*/)
	: CDialog(CSolidBrushSubObjPanel::IDD, pParent)
{
	Create( IDD,pParent );
}

CSolidBrushSubObjPanel::~CSolidBrushSubObjPanel()
{
}

void CSolidBrushSubObjPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control( pDX,IDC_SNAP_TO_GRID,m_btn[0] );
	DDX_Control( pDX,IDC_VERTEX_DELETE,m_btn[2] );
	DDX_Control( pDX,IDC_FACE_SPLIT,m_btn[3] );
	DDX_Control( pDX,IDC_FACE_DELETE,m_btn[4] );
	DDX_Control( pDX,IDC_SET_MATID,m_btn[5] );
	DDX_Control( pDX,IDC_SELECT_MATID,m_btn[6] );
	DDX_Control( pDX,IDC_PIVOTTOVERTICES,m_btn[7] );
	DDX_Control( pDX,IDC_PIVOTTOCENTER,m_btn[8] );
}


BEGIN_MESSAGE_MAP(CSolidBrushSubObjPanel, CDialog)
	ON_BN_CLICKED(IDC_SNAP_TO_GRID, OnVertexSnapToGrid)
	ON_BN_CLICKED(IDC_VERTEX_DELETE, OnVertexDelete)
	ON_BN_CLICKED(IDC_FACE_SPLIT, OnFaceSplit)
	ON_BN_CLICKED(IDC_FACE_DELETE, OnFaceDelete)
	ON_BN_CLICKED(IDC_SET_MATID, OnSetMatId)
	ON_BN_CLICKED(IDC_SELECT_MATID, OnSelectMatId)
	ON_BN_CLICKED(IDC_PIVOTTOVERTICES, OnPivotToVertices)
	ON_BN_CLICKED(IDC_PIVOTTOCENTER, OnPivotToCenter)
END_MESSAGE_MAP()


// CSolidBrushSubObjPanel message handlers
BOOL CSolidBrushSubObjPanel::OnInitDialog()
{
	BOOL res = __super::OnInitDialog();

	m_matIdCtrl.Create( this,IDC_MATID );
	m_matIdCtrl.SetInteger(true);
	m_matIdCtrl.SetRange(0,32);

	m_btn[7].SetToolTip("Allow for a Grid");
	m_btn[8].SetToolTip("Allow for a Grid");
	
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushSubObjPanel::SetObject( CSolidBrushObject *pObject )
{
	m_pObject = pObject;
	m_matIdCtrl.SetValue( 0 );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushSubObjPanel::SetCurrentMatId( int nMatId )
{
	m_matIdCtrl.SetValue( nMatId );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushSubObjPanel::OnVertexSnapToGrid()
{
	CUndo undo("Snap Brush Points To Grid");
	m_pObject->GetBrush()->Selection_SnapToGrid();
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushSubObjPanel::OnVertexDelete()
{
	CUndo undo("Delete Brush Vertex");
	m_pObject->GetBrush()->Selection_DeleteVertex();
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushSubObjPanel::OnFaceSplit()
{
	CUndo undo("Split Brush Face");
	m_pObject->GetBrush()->Selection_SplitFace();
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushSubObjPanel::OnFaceDelete()
{
	CUndo undo("Delete Brush Face");
	m_pObject->GetBrush()->Selection_DeleteFace();
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushSubObjPanel::OnSetMatId()
{
	CUndo undo("Set Brush Face MatId");
	m_pObject->GetBrush()->Selection_SetMatId( (int)m_matIdCtrl.GetValue() );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushSubObjPanel::OnSelectMatId()
{
	CUndo undo("Select Brush Faces by MatId");
	m_pObject->GetBrush()->Selection_SelectMatId( (int)m_matIdCtrl.GetValue() );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushSubObjPanel::OnPivotToVertices()
{
	CUndo undo("Align Pivot to Vertices");
	m_pObject->PivotToVertices();
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushSubObjPanel::OnPivotToCenter()
{
	CUndo undo("Align Pivot to Center");
	m_pObject->PivotToCenter();
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushSubObjPanel::OnCancel()
{
	GetIEditor()->SetEditTool(0);
}