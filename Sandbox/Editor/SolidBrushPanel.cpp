////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SolidBrushPanel.h
//  Version:     v1.00
//  Created:     11/1/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: CSolidBrushPanel implementation file.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "SolidBrushPanel.h"

#include "Brush\SolidBrushObject.h"
#include ".\solidbrushpanel.h"

// CSolidBrushPanel dialog

IMPLEMENT_DYNAMIC(CSolidBrushPanel, CDialog)
CSolidBrushPanel::CSolidBrushPanel(CWnd* pParent /*=NULL*/)
	: CDialog(CSolidBrushPanel::IDD, pParent)
{
	Create( IDD,pParent );
}

CSolidBrushPanel::~CSolidBrushPanel()
{
}

void CSolidBrushPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control( pDX,IDC_GEOMETRY_EDIT,m_geomEdit );
	DDX_Control( pDX,IDC_SAVE_TO_CGF,m_btn[0] );
	DDX_Control( pDX,IDC_RESET_TRANSFORM,m_btn[2] );
	DDX_Control( pDX,IDC_MAKE_HOLLOW,m_btn[3] );
	DDX_Control( pDX,IDC_CSG_COMBINE,m_btn[4] );
	DDX_Control( pDX,IDC_CSG_INTERSECT,m_btn[5] );
	DDX_Control( pDX,IDC_CSG_SUB_AB,m_btn[6] );
	DDX_Control( pDX,IDC_CSG_SUB_BA,m_btn[7] );
	DDX_Control( pDX,IDC_SNAP_TO_GRID,m_btn[8] );
}


BEGIN_MESSAGE_MAP(CSolidBrushPanel, CDialog)
	ON_BN_CLICKED(IDC_SAVE_TO_CGF, OnSaveToCgf)

	ON_BN_CLICKED(IDC_RESET_TRANSFORM, OnBnClickedResetTransform)
	ON_BN_CLICKED(IDC_MAKE_HOLLOW, OnBnClickedMakeHollow)
	ON_BN_CLICKED(IDC_CSG_COMBINE, OnBnClickedCsgCombine)
	ON_BN_CLICKED(IDC_CSG_INTERSECT, OnBnClickedCsgIntersect)
	ON_BN_CLICKED(IDC_CSG_SUB_AB, OnBnClickedCsgSubAb)
	ON_BN_CLICKED(IDC_CSG_SUB_BA, OnBnClickedCsgSubBa)
	ON_BN_CLICKED(IDC_SNAP_TO_GRID, OnBnClickedSnapToGrid)
END_MESSAGE_MAP()


// CSolidBrushPanel message handlers
BOOL CSolidBrushPanel::OnInitDialog()
{
	BOOL res = __super::OnInitDialog();

	m_geomEdit.SetToolName( "EditTool.SubObjMode" );
	
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushPanel::SetBrush( CSolidBrushObject *pObject )
{
	m_pObject = pObject;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushPanel::OnSaveToCgf()
{
	if (!m_pObject)
		return;

	CString filename;	
	if (CFileUtil::SelectSaveFile( "CGF Files|*.cgf","*.cgf",Path::GetGameFolder()+CString("\\objects"),filename ))
	{
		m_pObject->SaveToCgf( filename );
	}
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushPanel::OnBnClickedResetTransform()
{
	GetIEditor()->ExecuteCommand( "Brush.ResetTransform" );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushPanel::OnBnClickedMakeHollow()
{
	GetIEditor()->ExecuteCommand( "Brush.MakeHollow" );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushPanel::OnBnClickedCsgCombine()
{
	GetIEditor()->ExecuteCommand( "Brush.CSGCombine" );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushPanel::OnBnClickedCsgIntersect()
{
	GetIEditor()->ExecuteCommand( "Brush.CSGIntersect" );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushPanel::OnBnClickedCsgSubAb()
{
	GetIEditor()->ExecuteCommand( "Brush.CSGSubstruct" );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushPanel::OnBnClickedCsgSubBa()
{
	GetIEditor()->ExecuteCommand( "Brush.CSGSubstruct2" );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushPanel::OnBnClickedSnapToGrid()
{
	GetIEditor()->ExecuteCommand( "Brush.SnapPointsToGrid" );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushPanel::OnCancel()
{
	GetIEditor()->ClearSelection();
}