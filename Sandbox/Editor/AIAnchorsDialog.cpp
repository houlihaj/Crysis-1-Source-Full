// AIAnchorsDialog.cpp : implementation file
//

#include "stdafx.h"
#include "AIAnchorsDialog.h"
#include "AI\AIManager.h"

// CAIAnchorsDialog dialog

IMPLEMENT_DYNAMIC(CAIAnchorsDialog, CDialog)
CAIAnchorsDialog::CAIAnchorsDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CAIAnchorsDialog::IDD, pParent)
{
	m_sAnchor="";
}

CAIAnchorsDialog::~CAIAnchorsDialog()
{
}

void CAIAnchorsDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ANCHORS, m_wndAnchorsList);
}


BEGIN_MESSAGE_MAP(CAIAnchorsDialog, CDialog)
	ON_LBN_DBLCLK(IDC_ANCHORS, OnLbnDblClk)
	ON_LBN_SELCHANGE(IDC_ANCHORS, OnLbnSelchangeAnchors)
	ON_BN_CLICKED(IDC_NEW, OnNewBtn)
	ON_BN_CLICKED(IDEDIT, OnEditBtn)
	ON_BN_CLICKED(IDDELETE, OnDeleteBtn)
	ON_BN_CLICKED(IDREFRESH, OnRefreshBtn)
END_MESSAGE_MAP()


// CAIAnchorsDialog message handlers

void CAIAnchorsDialog::OnNewBtn()
{
	CFileUtil::EditTextFile( "Scripts\\AI\\Anchor.lua" );
}

void CAIAnchorsDialog::OnEditBtn()
{
	CFileUtil::EditTextFile( "Scripts\\AI\\Anchor.lua" );
}

void CAIAnchorsDialog::OnDeleteBtn()
{
	CString msg;
	msg.Format("Do you really want to delete AI Anchor Type %s?", m_sAnchor);
	if (AfxMessageBox(msg, MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2) == IDYES)
	{
		CAIManager *pAIMgr=GetIEditor()->GetAI();
		//pAIMgr->DeleteAnchor(m_sAnchor);
		m_wndAnchorsList.DeleteString(m_wndAnchorsList.GetCurSel());
	}
}

void CAIAnchorsDialog::OnRefreshBtn()
{
	m_wndAnchorsList.ResetContent();
	CAIManager *pAIMgr=GetIEditor()->GetAI();
	pAIMgr->ReloadScripts();
	typedef std::vector<CString> TAnchorActionsVec;
	TAnchorActionsVec vecAnchorActions;
	pAIMgr->GetAnchorActions(vecAnchorActions);
	for (TAnchorActionsVec::iterator It=vecAnchorActions.begin();It!=vecAnchorActions.end();++It)
		m_wndAnchorsList.AddString(*It);
	m_wndAnchorsList.SelectString(-1, m_sAnchor);
}

void CAIAnchorsDialog::OnLbnDblClk()
{
	if ( m_wndAnchorsList.GetCurSel() >= 0 )
		EndDialog( IDOK );
}

void CAIAnchorsDialog::OnLbnSelchangeAnchors()
{
	SetDlgItemText( IDCANCEL, "Cancel" );
	GetDlgItem( IDOK )->EnableWindow( TRUE );

	int nSel=m_wndAnchorsList.GetCurSel();
	if (nSel==LB_ERR)
	{
		m_sAnchor.Empty();
		return;
	}
	m_wndAnchorsList.GetText(nSel, m_sAnchor);
}


BOOL CAIAnchorsDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	GetDlgItem( IDC_NEW )->EnableWindow( FALSE );
	GetDlgItem( IDDELETE )->EnableWindow( FALSE );

	m_wndAnchorsList.ResetContent();
	CAIManager *pAIMgr=GetIEditor()->GetAI();
	ASSERT(pAIMgr);
	typedef std::vector<CString> TAnchorActionsVec;
	TAnchorActionsVec vecAnchorActions;
	pAIMgr->GetAnchorActions(vecAnchorActions);
	for (TAnchorActionsVec::iterator It=vecAnchorActions.begin();It!=vecAnchorActions.end();++It)
		m_wndAnchorsList.AddString(*It);
	m_wndAnchorsList.SelectString(-1, m_sAnchor);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}