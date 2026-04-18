////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ViewportTitleDlg.cpp
//  Version:     v1.00
//  Created:     15/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: CViewportTitleDlg implementation file
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ViewportTitleDlg.h"
#include "ViewPane.h"
#include "DisplaySettings.h"


// CViewportTitleDlg dialog

IMPLEMENT_DYNAMIC(CViewportTitleDlg, CXTResizeDialog)

CViewportTitleDlg::CViewportTitleDlg(CWnd* pParent) : CXTResizeDialog(CViewportTitleDlg::IDD, pParent)
{
	m_pViewPane = NULL;
	GetIEditor()->RegisterNotifyListener(this);
}

CViewportTitleDlg::~CViewportTitleDlg()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::SetViewPane( CLayoutViewPane *pViewPane )
{
	m_pViewPane = pViewPane;
};

//////////////////////////////////////////////////////////////////////////
BOOL CViewportTitleDlg::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();

	m_maxBtn.SetIcon( MAKEINTRESOURCE(IDI_MAXIMIZE) );
	m_hideHelpersBtn.SetIcon( MAKEINTRESOURCE(IDI_HIDEHELPERS) );
//	m_titleBtn.SetIcon( MAKEINTRESOURCE(IDI_DOWN_ARROW),BS_RIGHT,true );
	m_titleBtn.SetWindowText(m_title);
	m_sizeTextCtrl.SetWindowText( "" );
	m_hideHelpersBtn.SetToolTip("Hide helpers toggle");
	SetResize( IDC_SIZE_TEXT,SZ_HORREPOS(1) );
	SetResize( IDC_VIEWPORT_MAXIMIZE,SZ_HORREPOS(1) );
	SetResize( IDC_VIEWPORT_HIDEHELPERS,SZ_HORREPOS(1) );
	return TRUE;
}

void CViewportTitleDlg::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX,IDC_VIEWPORT_TITLE,m_titleBtn);
	DDX_Control(pDX,IDC_VIEWPORT_MAXIMIZE,m_maxBtn);
	DDX_Control(pDX,IDC_VIEWPORT_HIDEHELPERS,m_hideHelpersBtn);
	DDX_Control(pDX,IDC_SIZE_TEXT,m_sizeTextCtrl);
}


BEGIN_MESSAGE_MAP(CViewportTitleDlg, CXTResizeDialog)
	ON_BN_SETFOCUS(IDC_VIEWPORT_TITLE,OnButtonSetFocus)
	ON_BN_CLICKED(IDC_VIEWPORT_TITLE,OnTitle)
	//ON_CONTROL(BN_PUSHED,IDC_VIEWPORT_TITLE,OnTitle)
	ON_BN_CLICKED(IDC_VIEWPORT_MAXIMIZE,OnMaximize)
	ON_BN_CLICKED(IDC_VIEWPORT_HIDEHELPERS,OnHideHelpers)
	ON_WM_RBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_SIZE()
END_MESSAGE_MAP()

// CViewportTitleDlg message handlers
//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::OnSetFocus( CWnd* )
{
	GetParent()->SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::SetTitle( const CString &title )
{
	m_title = title;
	if (m_titleBtn.m_hWnd)
		m_titleBtn.SetWindowText(m_title);
}

//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::OnButtonSetFocus()
{
	//GetParent()->SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (m_pViewPane)
		m_pViewPane->ToggleMaximize();
}

//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::OnMaximize()
{
	if (m_pViewPane)
		m_pViewPane->ToggleMaximize();
}


//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::OnHideHelpers()
{
	GetIEditor()->GetDisplaySettings()->DisplayHelpers( !GetIEditor()->GetDisplaySettings()->IsDisplayHelpers());
	GetIEditor()->Notify(eNotify_OnDisplayRenderUpdate);
}


//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::OnRButtonDown(UINT nFlags, CPoint point)
{
	if (m_pViewPane)
		m_pViewPane->ShowTitleMenu();
}

//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::OnTitle()
{
	if (m_pViewPane)
		m_pViewPane->ShowTitleMenu();
}

//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::OnSize( UINT nType,int cx,int cy )
{
	CXTResizeDialog::OnSize(nType,cx,cy);
	if (m_maxBtn.m_hWnd)
	{
		CRect rc;
		//m_maxBtn.GetWindowRect(rc);
		//m_maxBtn.MoveWindow(0,cx-rc.Width(),rc.Width(),cy);
	}
}

//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::SetViewportSize( int width,int height )
{
	if (m_sizeTextCtrl.m_hWnd)
	{
		CString str;
		str.Format( "%d x %d",width,height );
		m_sizeTextCtrl.SetWindowText( str );
	}
}

//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::SetActive( bool bActive )
{
	
}

//////////////////////////////////////////////////////////////////////////
void CViewportTitleDlg::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
	case eNotify_OnDisplayRenderUpdate:
		m_hideHelpersBtn.SetCheck(!GetIEditor()->GetDisplaySettings()->IsDisplayHelpers());
		break;
	}
}
