// ConsoleDialog.cpp : implementation file
//

#include "stdafx.h"
#include "ConsoleDialog.h"

/////////////////////////////////////////////////////////////////////////////
// CConsoleDialog dialog


CConsoleDialog::CConsoleDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CConsoleDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConsoleDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CConsoleDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConsoleDialog)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConsoleDialog, CDialog)
	ON_WM_SIZE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConsoleDialog message handlers

//////////////////////////////////////////////////////////////////////////
BOOL CConsoleDialog::OnInitDialog()
{
	CRect rc;
	GetClientRect(rc);
	m_consoleWnd.Create( NULL,NULL,WS_CHILD|WS_VISIBLE,rc,this,0 );
	m_consoleWnd.MoveWindow(rc);
	m_consoleWnd.ShowWindow(SW_SHOW);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CConsoleDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType,cx,cy);
	if (m_consoleWnd.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		m_consoleWnd.MoveWindow(rc);
	}
}

//////////////////////////////////////////////////////////////////////////
void CConsoleDialog::OnDestroy()
{
}

//////////////////////////////////////////////////////////////////////////
void CConsoleDialog::OnCancel()
{
	if (GetISystem())
		GetISystem()->Quit();
}

//////////////////////////////////////////////////////////////////////////
void CConsoleDialog::SetInfoText( const char *text )
{
	CryLogAlways( text );
}
