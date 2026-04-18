////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   BrushClipToolPanel.cpp
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain modification tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BrushClipTool.h"
#include "BrushClipToolPanel.h"


#include "..\Objects\ObjectManager.h"
#include "..\Objects\BrushObject.h"
#include "EditMode\VertexMode.h"

// CBrushClipToolPanel dialog

IMPLEMENT_DYNAMIC(CBrushClipToolPanel, CXTResizeDialog)
CBrushClipToolPanel::CBrushClipToolPanel(CWnd* pParent)
: CXTResizeDialog(CBrushClipToolPanel::IDD, pParent)
{
	m_nPlaneType = 0;
	m_pTool = 0;
	Create( IDD,pParent );
}

//////////////////////////////////////////////////////////////////////////
CBrushClipToolPanel::~CBrushClipToolPanel()
{
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipToolPanel::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CLIP_BACK, m_btn[0]);
	DDX_Control(pDX, IDC_CLIP_FRONT, m_btn[1]);
	DDX_Control(pDX, IDC_CLIP_SPLIT, m_btn[2]);
	DDX_Radio(pDX, IDC_CLIP_3POINTS, m_nPlaneType);
}

BEGIN_MESSAGE_MAP(CBrushClipToolPanel, CXTResizeDialog)
	ON_BN_CLICKED(IDC_CLIP_FRONT, OnClipFront)
	ON_BN_CLICKED(IDC_CLIP_BACK, OnClipBack)
	ON_BN_CLICKED(IDC_CLIP_SPLIT, OnClipSplit)
	ON_EN_UPDATE(IDC_SCALE,OnSizeChange)
	ON_EN_UPDATE(IDC_ANGLE,OnAngleChange)
	ON_BN_CLICKED(IDC_CLIP_3POINTS, OnPlaneTypeChange)
	ON_BN_CLICKED(IDC_CLIP_2POINTS, OnPlaneTypeChange)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CBrushClipToolPanel::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();

	m_sizeCtrl.Create( this,IDC_SCALE,0 );
	m_angleCtrl.Create( this,IDC_ANGLE,0 );

	return TRUE;  // return TRUE unless you set the focus to a control
}


//////////////////////////////////////////////////////////////////////////
void CBrushClipToolPanel::SetTool( CBrushClipTool *pTool )
{
	m_pTool = pTool;
	m_sizeCtrl.SetValue( pTool->GetPlaneScale() );
	m_angleCtrl.SetValue( pTool->GetPlaneAngle() );
	m_nPlaneType = pTool->GetPlaneType();

	UpdateData(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipToolPanel::OnPlaneTypeChange()
{
	UpdateData(TRUE);
	m_pTool->SetPlaneType(m_nPlaneType);
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipToolPanel::OnClipFront()
{
	m_pTool->DoClip(0);
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipToolPanel::OnClipBack()
{
	m_pTool->DoClip(1);
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipToolPanel::OnClipSplit()
{
	m_pTool->DoClip(2);
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipToolPanel::OnSizeChange()
{
	m_pTool->SetPlaneScale( m_sizeCtrl.GetValue() );
}

//////////////////////////////////////////////////////////////////////////
void CBrushClipToolPanel::OnAngleChange()
{
	m_pTool->SetPlaneAngle( m_angleCtrl.GetValue() );
}
