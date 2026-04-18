// VersionUpdateDialog.cpp : implementation file
//

#include "stdafx.h"
#include "VersionUpdateDialog.h"
#include "IEngineSettingsManager.h"

#ifdef USING_LICENSE_PROTECTION

IMPLEMENT_DYNAMIC(CVersionUpdateDialog, CDialog)


//////////////////////////////////////////////////////////////////////////
CVersionUpdateDialog::CVersionUpdateDialog(CWnd* pParent /*=NULL*/) :
	CDialog(CVersionUpdateDialog::IDD, pParent),
	m_sVersion("0")
{
	//{{AFX_DATA_INIT(CVersionUpdateDialog)
	//}}AFX_DATA_INIT
}


//////////////////////////////////////////////////////////////////////////
CVersionUpdateDialog::CVersionUpdateDialog(string sVersion) :
	CDialog(CVersionUpdateDialog::IDD, NULL),
	m_sVersion(sVersion)
{
	//{{AFX_DATA_INIT(CVersionUpdateDialog)
	//}}AFX_DATA_INIT
}


//////////////////////////////////////////////////////////////////////////
BOOL CVersionUpdateDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	return FALSE;
}


//////////////////////////////////////////////////////////////////////////
INT_PTR CVersionUpdateDialog::DoModal()
{
	IEngineSettingsManager* pEsm = gEnv->pSystem->GetEngineSettingsManager();
	
	if (pEsm->HasKey("EDT_SkipVersionRemind"))
	{
		string storedVersion;
		pEsm->GetValueByRef("EDT_SkipVersionRemind", storedVersion);
		if (m_sVersion==storedVersion)
			return NULL;
	}
	CDialog::DoModal();
}

//////////////////////////////////////////////////////////////////////////
void CVersionUpdateDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVersionUpdateDialog)
	DDX_Control(pDX, IDC_STATIC1, m_lState);
	DDX_Control(pDX, IDC_CHECK1, m_cDontShow);
	DDX_Control(pDX, IDOK, m_bOk);

	string msg = string("Your SDK version does not match the currently available version (v")+m_sVersion+").\nPlease check the Crytek Knowledge Network for updates.";
	m_lState.SetWindowText(msg);
	//}}AFX_DATA_MAP
}


//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CVersionUpdateDialog, CDialog)
	//{{AFX_MSG_MAP(CVersionUpdateDialog)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK,&OnOkButtonClicked)
END_MESSAGE_MAP()


//////////////////////////////////////////////////////////////////////////
void CVersionUpdateDialog::OnOkButtonClicked()
{
	bool bDontShowNextTime = m_cDontShow.GetCheck()!=0;

	if (bDontShowNextTime)
	{
		IEngineSettingsManager* pEsm = gEnv->pSystem->GetEngineSettingsManager();
		pEsm->SetKey("EDT_SkipVersionRemind", m_sVersion);
		pEsm->StoreData();
	}

	EndDialog(IDOK);
}

#endif // USING_LICENSE_PROTECTION


