////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SubObjSelectionPanel.h
//  Version:     v1.00
//  Created:     26/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SubObjSelectionPanel.h"
#include "Objects\SubObjSelection.h"


// CSubObjSelectionPanel dialog

IMPLEMENT_DYNAMIC(CSubObjSelectionPanel, CDialog)
CSubObjSelectionPanel::CSubObjSelectionPanel(CWnd* pParent /*=NULL*/)
	: CDialog(CSubObjSelectionPanel::IDD, pParent)
{
	Create( IDD,pParent );
}

CSubObjSelectionPanel::~CSubObjSelectionPanel()
{
}

void CSubObjSelectionPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX,IDC_SELECT_BY_VERTEX,m_bSelectByVertex);
	DDX_Check(pDX,IDC_IGNORE_BACKFACE,m_bIgnoreBackfacing);
	DDX_Check(pDX,IDC_ENABLE_SOFTSEL,m_bSoftSelection);
}


BEGIN_MESSAGE_MAP(CSubObjSelectionPanel, CDialog)
	ON_BN_CLICKED(IDC_SELECT_BY_VERTEX, OnUpdateFromUI)
	ON_BN_CLICKED(IDC_IGNORE_BACKFACE, OnUpdateFromUI)
	ON_BN_CLICKED(IDC_ENABLE_SOFTSEL, OnUpdateFromUI)
	ON_EN_CHANGE(IDC_FALLOFF, OnUpdateFromUI)
END_MESSAGE_MAP()


// CSubObjSelectionPanel message handlers
BOOL CSubObjSelectionPanel::OnInitDialog()
{
	BOOL res = __super::OnInitDialog();

	SSubObjSelOptions &opt = g_SubObjSelOptions;
	m_bSelectByVertex = opt.bSelectByVertex;
	m_bIgnoreBackfacing = opt.bIgnoreBackfacing;
	m_bSoftSelection = opt.bSoftSelection;

	m_falloff.Create( this,IDC_FALLOFF);
	m_falloff.SetRange(0,100);
	m_falloff.SetValue( opt.fSoftSelFalloff );

	UpdateData(FALSE);
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CSubObjSelectionPanel::OnUpdateFromUI()
{
	SSubObjSelOptions &opt = g_SubObjSelOptions;
	
	UpdateData(TRUE);
	opt.fSoftSelFalloff = m_falloff.GetValue();
	opt.bSelectByVertex = m_bSelectByVertex;
	opt.bIgnoreBackfacing = m_bIgnoreBackfacing;
	opt.bSoftSelection = m_bSoftSelection;
}

void CSubObjSelectionPanel::OnCancel()
{
	GetIEditor()->SetEditTool(0);
}