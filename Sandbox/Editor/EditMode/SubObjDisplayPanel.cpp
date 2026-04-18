////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SubObjDisplayPanel.h
//  Version:     v1.00
//  Created:     26/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SubObjDisplayPanel.h"
#include "Objects\SubObjSelection.h"


// CSubObjDisplayPanel dialog

IMPLEMENT_DYNAMIC(CSubObjDisplayPanel, CDialog)
CSubObjDisplayPanel::CSubObjDisplayPanel(CWnd* pParent /*=NULL*/)
: CDialog(CSubObjDisplayPanel::IDD, pParent)
{
	Create( IDD,pParent );
	m_displayType = 0;
}

CSubObjDisplayPanel::~CSubObjDisplayPanel()
{
}

void CSubObjDisplayPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX,IDC_DISPLAY_BACKFACING,m_bDisplayBackfacing);
	DDX_Check(pDX,IDC_DISPLAY_NORMALS,m_bDisplayNormals);
	DDX_Radio(pDX,IDC_WIREFRAME,m_displayType);
}


BEGIN_MESSAGE_MAP(CSubObjDisplayPanel, CDialog)
	ON_BN_CLICKED(IDC_DISPLAY_BACKFACING, OnUpdateFromUI)
	ON_BN_CLICKED(IDC_DISPLAY_NORMALS, OnUpdateFromUI)
	ON_BN_CLICKED(IDC_WIREFRAME, OnUpdateFromUI)
	ON_BN_CLICKED(IDC_FLAT, OnUpdateFromUI)
	ON_BN_CLICKED(IDC_GEOMETRY, OnUpdateFromUI)
	ON_EN_UPDATE(IDC_NORMALS_LENGTH, OnUpdateFromUI)
END_MESSAGE_MAP()


// CSubObjDisplayPanel message handlers
BOOL CSubObjDisplayPanel::OnInitDialog()
{
	BOOL res = __super::OnInitDialog();

	SSubObjSelOptions &opt = g_SubObjSelOptions;
	m_bDisplayBackfacing = opt.bDisplayBackfacing;
	m_bDisplayNormals = opt.bDisplayNormals;

	m_displayType = 0;
	switch (opt.displayType)
	{
	case SO_DISPLAY_WIREFRAME: m_displayType = 0; break;
	case SO_DISPLAY_FLAT: m_displayType = 1; break;
	case SO_DISPLAY_GEOMETRY: m_displayType = 2; break;
	}

	m_normalsLength.Create( this,IDC_NORMALS_LENGTH);
	m_normalsLength.SetRange(0,10);
	m_normalsLength.SetValue( opt.fNormalsLength );

	UpdateData(FALSE);
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CSubObjDisplayPanel::OnUpdateFromUI()
{
	SSubObjSelOptions &opt = g_SubObjSelOptions;

	UpdateData(TRUE);
	opt.fNormalsLength = m_normalsLength.GetValue();
	opt.bDisplayBackfacing = m_bDisplayBackfacing;
	opt.bDisplayNormals = m_bDisplayNormals;
	
	switch (m_displayType)
	{
	case 0: opt.displayType = SO_DISPLAY_WIREFRAME; break;
	case 1: opt.displayType = SO_DISPLAY_FLAT; break;
	case 2: opt.displayType = SO_DISPLAY_GEOMETRY; break;
	}
}

void CSubObjDisplayPanel::OnCancel()
{
	GetIEditor()->SetEditTool(0);
}