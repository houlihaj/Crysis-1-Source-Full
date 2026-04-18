// StartupLogoDialog.cpp : implementation file
//

#include "stdafx.h"
#include "StartupLogoDialog.h"


/////////////////////////////////////////////////////////////////////////////
// CStartupLogoDialog dialog

CStartupLogoDialog *CStartupLogoDialog::s_pLogoWindow = 0;

CStartupLogoDialog::CStartupLogoDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CStartupLogoDialog::IDD, pParent)
{

}

CStartupLogoDialog::~CStartupLogoDialog()
{
	s_pLogoWindow = 0;
}

void CStartupLogoDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CStartupLogoDialog)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CStartupLogoDialog, CDialog)
	//{{AFX_MSG_MAP(CStartupLogoDialog)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStartupLogoDialog message handlers+


void CStartupLogoDialog::SetVersion( const Version &v )
{
	if (!m_hWnd)
		return;

	char pVersionText[256];
	_snprintf(pVersionText, 256, "Version %d.%d.%d - Build %d", v[3], v[2], v[1], v[0]);
	SetDlgItemText(IDC_VERSION, CString(pVersionText));
}

void CStartupLogoDialog::SetInfo( const char *text )
{
	if (m_hWnd != NULL)
	{
		::SetDlgItemText( m_hWnd,IDC_INFO_TEXT,text );
		UpdateWindow();
	}
}

void CStartupLogoDialog::SetText( const char *text )
{
	if (s_pLogoWindow)
		s_pLogoWindow->SetInfo( text );
}

void CStartupLogoDialog::SetInfoText( const char *text )
{
	SetInfo( text );
}


BOOL CStartupLogoDialog::OnInitDialog() 
{
	s_pLogoWindow = this;

	CDialog::OnInitDialog();

	CRect rcBmp, rcDlg, rcTxt;
	GetWindowRect(rcDlg);
	GetDlgItem(IDC_VERSION)->GetWindowRect(rcTxt);
	GetDlgItem(IDC_STATIC)->GetWindowRect(rcBmp);
	rcTxt.OffsetRect(rcBmp.Width()+2 - rcDlg.Width(), 0);
	ScreenToClient(rcTxt);
	SetWindowPos(NULL, 0, 0, rcBmp.Width()+2, rcDlg.Height(), SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
	GetDlgItem(IDC_VERSION)->SetWindowPos(NULL, rcTxt.left, rcTxt.top, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOOWNERZORDER|SWP_NOZORDER);

	SetWindowText( "Starting Crytek Sandbox Editor" );
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
