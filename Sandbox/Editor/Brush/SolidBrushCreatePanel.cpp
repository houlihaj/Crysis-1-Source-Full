////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SolidBrushCreatePanel.h
//  Version:     v1.00
//  Created:     11/1/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: CSolidBrushCreatePanel implementation file.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "SolidBrushCreatePanel.h"

#include "Brush\SolidBrushObject.h"
#include ".\SolidBrushCreatePanel.h"

// CSolidBrushCreatePanel dialog

IMPLEMENT_DYNAMIC(CSolidBrushCreatePanel, CDialog)
CSolidBrushCreatePanel::CSolidBrushCreatePanel(CWnd* pParent /*=NULL*/)
	: CDialog(CSolidBrushCreatePanel::IDD, pParent)
{
	Create( IDD,pParent );
}

CSolidBrushCreatePanel::~CSolidBrushCreatePanel()
{
}

void CSolidBrushCreatePanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control( pDX,IDC_COMBO1,m_typeCtrl );
}


BEGIN_MESSAGE_MAP(CSolidBrushCreatePanel, CDialog)
	ON_EN_UPDATE( IDC_NUM_SIDES,OnNumSidesChange )
	ON_CBN_SELENDOK( IDC_COMBO1,OnTypeChange )
END_MESSAGE_MAP()


// CSolidBrushCreatePanel message handlers
BOOL CSolidBrushCreatePanel::OnInitDialog()
{
	BOOL res = __super::OnInitDialog();
	m_numSidesCtrl.Create( this,IDC_NUM_SIDES);
	m_numSidesCtrl.SetRange( 4,1000 );
	m_numSidesCtrl.SetInteger(true);

	m_typeCtrl.AddString( "Box" );
	m_typeCtrl.AddString( "Cone" );
	m_typeCtrl.AddString( "Sphere" );
	m_typeCtrl.AddString( "Shape" );

	m_numSidesCtrl.SetValue( CSolidBrushObject::m_nCreateNumSides );

	m_typeCtrl.SetCurSel( CSolidBrushObject::m_nCreateType );

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushCreatePanel::EnableNumSides( bool bEnable )
{
	if (m_numSidesCtrl.m_hWnd)
		m_numSidesCtrl.EnableWindow( (bEnable)? TRUE : FALSE );
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushCreatePanel::OnNumSidesChange()
{
	int numSides = m_numSidesCtrl.GetValue();
	CSolidBrushObject::m_nCreateNumSides = numSides;
}

//////////////////////////////////////////////////////////////////////////
void CSolidBrushCreatePanel::OnTypeChange()
{
	int nCurSel = m_typeCtrl.GetCurSel();
	CSolidBrushObject::m_nCreateType = (ESolidBrushCreateType)nCurSel;
}
