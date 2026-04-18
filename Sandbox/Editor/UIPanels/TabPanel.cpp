////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TabPanel.h
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain modification tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TabPanel.h"

#define IDC_TASKPANEL 1
/////////////////////////////////////////////////////////////////////////////
// CTabPanel dialog

IMPLEMENT_DYNCREATE(CTabPanel,CDialog)

CString CTabPanel::m_lastSelectedPage;

CTabPanel::CTabPanel( CWnd* pParent )
: CDialog(CTabPanel::IDD,pParent)
{
	Create( IDD,pParent );
}

//////////////////////////////////////////////////////////////////////////
CTabPanel::~CTabPanel()
{
	
}

//////////////////////////////////////////////////////////////////////////
void CTabPanel::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTabPanel, CDialog)
	ON_WM_SIZE()
	ON_NOTIFY( (TCN_SELCHANGE-10),IDC_TAB, OnTabSelect )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTabPanel message handlers
BOOL CTabPanel::OnInitDialog() 
{
	__super::OnInitDialog();

	{
		CRect wrc;
		GetWindowRect(wrc);
		wrc.bottom = wrc.top + 500;
		MoveWindow(wrc,FALSE);
	}

	CRect rc;
	GetClientRect( rc );

//	m_tab.CreateEx(NULL, AfxRegisterWndClass(CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW,
//		AfxGetApp()->LoadStandardCursor(IDC_ARROW),NULL,NULL),"", WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,CRect(0,0,10,10),this,IDC_TAB);
	m_tab.Create(WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|TCS_MULTILINE|TCS_HOTTRACK,rc,this,IDC_TAB);
	m_tab.SetFont( CFont::FromHandle( (HFONT)gSettings.gui.hSystemFont) );

	//m_tab.GetPaintManager()->m_bHotTracking = true;
	//m_tab.GetPaintManager()->m_bOneNoteColors = true;
	//m_tab.GetPaintManager()->SetAppearance(xtpTabAppearancePropertyPage);
	//m_tab.GetPaintManager()->SetColor(xtpTabColorOffice2003);
	//m_tab.GetPaintManager()->SetLayout(xtpTabLayoutAutoSize);
	//m_tab.GetPaintManager()->m_rcButtonMargin = CRect(2,0,2,2);
	//m_tab.GetPaintManager()->m_rcClientMargin = CRect(0,4,0,0);

	// Clip children of the tab control from paint routines to reduce flicker.
	//m_tab.ModifyStyle(0L,WS_CLIPCHILDREN|WS_CLIPSIBLINGS);

	return TRUE;  // return TRUE unless you set the focus to a control
}

//////////////////////////////////////////////////////////////////////////
void CTabPanel::OnSize(UINT nType, int cx, int cy) 
{
	__super::OnSize(nType, cx, cy);

	CRect rc;
	GetClientRect( rc );

	if (m_tab.m_hWnd)
		m_tab.MoveWindow( rc,TRUE );
}

//////////////////////////////////////////////////////////////////////////
int CTabPanel::AddPage( const char *sCaption,CDialog *pDlg,bool bAutoDestroy )
{
	int nPageId = m_tab.AddPage( sCaption,pDlg,bAutoDestroy );
	if (sCaption == m_lastSelectedPage)
	{
		m_tab.SelectPage(nPageId);
	}
	return nPageId;
}

//////////////////////////////////////////////////////////////////////////
void CTabPanel::RemovePage( int nPageId )
{
	m_tab.RemovePage( nPageId );
}

//////////////////////////////////////////////////////////////////////////
void CTabPanel::SelectPage( int nPageId )
{
	m_tab.SelectPage(nPageId);
}

//////////////////////////////////////////////////////////////////////////
int CTabPanel::GetPageCount()
{
	return m_tab.GetItemCount();
}

//////////////////////////////////////////////////////////////////////////
void CTabPanel::OnTabSelect(NMHDR* pNMHDR, LRESULT* pResult)
{
	int i = m_tab.GetCurSel();
	if (i >= 0)
	{
		m_tab.SelectPageByIndex(i);

		TCITEM item;
		ZeroStruct(item);
		item.mask = TCIF_TEXT;
		char buf[1024];
		item.pszText = buf;
		item.cchTextMax = sizeof(buf);
		if (m_tab.GetItem(i,&item))
		{
			m_lastSelectedPage = item.pszText;
		}
	}
}
