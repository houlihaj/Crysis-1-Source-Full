// LicenseKeyDialog.cpp : implementation file
//

#include "stdafx.h"
#include "LicenseKeyDialog.h"
#include "IEngineSettingsManager.h"

#ifdef USING_LICENSE_PROTECTION

IMPLEMENT_DYNAMIC(CLicenseKeyDialog, CDialog)


//////////////////////////////////////////////////////////////////////////
CLicenseKeyDialog::CLicenseKeyDialog(CWnd* pParent /*=NULL*/) :
	CDialog(CLicenseKeyDialog::IDD, pParent),
	m_bFirstTry(true)
{
	//{{AFX_DATA_INIT(CLicenseKeyDialog)
	//}}AFX_DATA_INIT
}


//////////////////////////////////////////////////////////////////////////
CLicenseKeyDialog::CLicenseKeyDialog(bool bFirstTry) :
	CDialog(CLicenseKeyDialog::IDD, NULL),
	m_bFirstTry(bFirstTry)
{
	//{{AFX_DATA_INIT(CLicenseKeyDialog)
	//}}AFX_DATA_INIT
}


//////////////////////////////////////////////////////////////////////////
BOOL CLicenseKeyDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	if (!m_bFirstTry)
		m_lState.SetWindowText("The key you entered could not be validated.\nPlease enter an unused license key.\n\nTo request new License Keys, please go to Crytek Knowledge Network.");

	m_eKey1.SetFocus();

	return FALSE;
}


//////////////////////////////////////////////////////////////////////////
void CLicenseKeyDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLicenseKeyDialog)
	DDX_Control(pDX, IDC_EDIT1, m_eKey1);
	DDX_Control(pDX, IDC_EDIT2, m_eKey2);
	DDX_Control(pDX, IDC_EDIT3, m_eKey3);
	DDX_Control(pDX, IDC_EDIT4, m_eKey4);
	DDX_Control(pDX, IDC_STATIC1, m_lState);
	DDX_Control(pDX, IDC_STATIC1, m_lState);
	DDX_Control(pDX, IDOK, m_bOk);
	//}}AFX_DATA_MAP
}


//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CLicenseKeyDialog, CDialog)
	//{{AFX_MSG_MAP(CLicenseKeyDialog)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK,&OnOkButtonClicked)
	ON_EN_CHANGE(IDC_EDIT1, OnChangeEdit1)
	ON_EN_CHANGE(IDC_EDIT2, OnChangeEdit2)
	ON_EN_CHANGE(IDC_EDIT3, OnChangeEdit3)
	ON_EN_CHANGE(IDC_EDIT4, OnChangeEdit4)
	ON_STN_CLICKED(IDC_STATIC1, &CLicenseKeyDialog::OnStnClickedStatic1)
END_MESSAGE_MAP()


//////////////////////////////////////////////////////////////////////////
void CLicenseKeyDialog::OnOkButtonClicked()
{
	CString t1, t2, t3, t4;
	m_eKey1.GetWindowText(t1);
	m_eKey2.GetWindowText(t2);
	m_eKey3.GetWindowText(t3);
	m_eKey4.GetWindowText(t4);

	if (t1.GetLength()<4 || t2.GetLength()<4 || t3.GetLength()<4 || t4.GetLength()<4)
		return;

	IEngineSettingsManager* pEsm = gEnv->pSystem->GetEngineSettingsManager();
	string theKey = t1 +"-"+ t2 +"-"+ t3 +"-"+ t4;
	pEsm->SetKey("EDT_InstanceKey", theKey);
	pEsm->StoreData();

	EndDialog(IDOK);
}


//////////////////////////////////////////////////////////////////////////
void CLicenseKeyDialog::OnChangeEdit1()
{
	CString t1,tRem, t2;
	m_eKey1.GetWindowText(t1);	
	t1.Remove('-');
	t1.Remove(' ');
	if (t1.GetLength()==4)
		m_eKey2.SetFocus();

	if (t1.GetLength()>4)
	{
		tRem = (LPCSTR)&(t1.GetBuffer()[4]);
		t1.Truncate(4);
		m_eKey1.SetWindowText(t1);
		m_eKey2.GetWindowText(t2);
		t2 = tRem + t2;
		m_eKey2.SetWindowText(t2);
	}
}


//////////////////////////////////////////////////////////////////////////
void CLicenseKeyDialog::OnChangeEdit2()
{
	CString t2, tRem, t3;
	m_eKey2.GetWindowText(t2);
	t2.Remove('-');
	t2.Remove(' ');
	if (t2.GetLength()==4)
		m_eKey3.SetFocus();

	if (t2.GetLength()>4)
	{
		tRem = (LPCSTR)&(t2.GetBuffer()[4]);
		t2.Truncate(4);
		m_eKey2.SetWindowText(t2);
		m_eKey3.GetWindowText(t3);
		t3 = tRem + t3;
		m_eKey3.SetWindowText(t3);
	}
}


//////////////////////////////////////////////////////////////////////////
void CLicenseKeyDialog::OnChangeEdit3()
{
	CString t3,tRem, t4;
	m_eKey3.GetWindowText(t3);	
	t3.Remove('-');
	t3.Remove(' ');
	if (t3.GetLength()==4)
		m_eKey4.SetFocus();

	if (t3.GetLength()>4)
	{
		tRem = (LPCSTR)&(t3.GetBuffer()[4]);
		t3.Truncate(4);
		m_eKey3.SetWindowText(t3);
		m_eKey4.GetWindowText(t4);
		t4 = tRem + t4;
		m_eKey4.SetWindowText(t4);
	}
}


//////////////////////////////////////////////////////////////////////////
void CLicenseKeyDialog::OnChangeEdit4()
{
	CString t4;
	m_eKey4.GetWindowText(t4);
	t4.Remove('-');
	t4.Remove(' ');
	if (t4.GetLength()==4)
		m_bOk.SetFocus();

	if (t4.GetLength()>4)
	{
		t4.Truncate(4);
		m_eKey4.SetWindowText(t4);
	}
}

void CLicenseKeyDialog::OnStnClickedStatic1()
{
	// TODO: Add your control notification handler code here
}
#endif // USING_LICENSE_PROTECTION


