// MatEditMainDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "MatEditMainDlg.h"

#include "Material/MaterialDialog.h"
#include "Material/MaterialManager.h"

#include "MaterialSender.h"

/////////////////////////////////////////////////////////////////////////////
// CMatEditMainDlg dialog

//#define WM_MATEDITSEND (WM_USER+315)
#define WM_MATEDITPICK (WM_USER+316)

enum DataToDriveCommand{
	dtdConnect = 1,

};


struct DataToDrive
{
	DataToDriveCommand com;
	char messaga[128];
};



CMatEditMainDlg::CMatEditMainDlg( const char *title,CWnd* pParent /*=NULL*/)
	: CDialog(CMatEditMainDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMatEditMainDlg)
	//}}AFX_DATA_INIT

	m_MatDlg = 0;
	//BOOL b = Create( IDD_MATEDITMAINDLG, pParent);
	//b = b;
}

/*
void CMatEditMainDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMatEditMainDlg)
	//}}AFX_DATA_MAP
}
*/


BEGIN_MESSAGE_MAP(CMatEditMainDlg, CDialog)
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_MESSAGE(WM_MATEDITSEND, OnMatEditSend)
	ON_MESSAGE(WM_KICKIDLE,OnKickIdle)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMatEditMainDlg message handlers



LRESULT CMatEditMainDlg::OnMatEditSend(WPARAM w, LPARAM l)
{
	GetIEditor()->GetMaterialManager()->SyncMaterialEditor();
	return 0;
}


BOOL CMatEditMainDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	if (XTPPaintManager() == NULL)
	{		
		CXTPPaintManager::SetTheme(xtpThemeOffice2000);
	}

	m_MatDlg = new CMaterialDialog();
	m_MatDlg->SetParent(this);
	RECT rc;
	GetClientRect(&rc);
	m_MatDlg->MoveWindow(&rc);
	m_MatDlg ->ShowWindow(SW_SHOW);



	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CMatEditMainDlg::OnDestroy()
{
	if(m_MatDlg)
	{
		delete m_MatDlg;
		m_MatDlg = 0;
	}
	CDialog::OnDestroy();
}

void CMatEditMainDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	RECT rc;
	GetClientRect(&rc);
	if(m_MatDlg)
		m_MatDlg->MoveWindow(&rc);
}

//////////////////////////////////////////////////////////////////////////
LRESULT CMatEditMainDlg::OnKickIdle(WPARAM wParam, LPARAM)
{
	GetIEditor()->Notify( eNotify_OnIdleUpdate );
	SendMessageToDescendants(WM_IDLEUPDATECMDUI,1);
	return 0;
}
