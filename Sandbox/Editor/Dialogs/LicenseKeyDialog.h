#if !defined(AFX_LICENSEKEYDIALOG_H__C8E98A11_BBE9_499F_AB62_6EA12BDBCF84__INCLUDED_)
#define AFX_LICENSEKEYDIALOG_H__C8E98A11_BBE9_499F_AB62_6EA12BDBCF84__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LicenseKeyDialog.h : header file
//

#ifdef USING_LICENSE_PROTECTION

/////////////////////////////////////////////////////////////////////////////
// CLicenseKeyDialog dialog
#include "resource.h"

class CLicenseKeyDialog : public CDialog
{
	DECLARE_DYNAMIC(CLicenseKeyDialog)

		// Construction
public:
	CLicenseKeyDialog(CWnd* pParent = NULL);   // standard constructor
	CLicenseKeyDialog(bool bFirstTry);

// Dialog Data
	//{{AFX_DATA(CLicenseKeyDialog)
	enum { IDD = IDD_LICENSE_KEY };

	CEdit m_eKey1;
	CEdit m_eKey2;
	CEdit m_eKey3;
	CEdit m_eKey4;
	CStatic m_lState;
	CButton m_bOk;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLicenseKeyDialog)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLicenseKeyDialog)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	afx_msg void OnOkButtonClicked();
	afx_msg void OnChangeEdit1();
	afx_msg void OnChangeEdit2();
	afx_msg void OnChangeEdit3();
	afx_msg void OnChangeEdit4();
	DECLARE_MESSAGE_MAP()

private:
	bool	m_bFirstTry;
public:
	afx_msg void OnStnClickedStatic1();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // USING_LICENSE_PROTECTION

#endif // !defined(AFX_LICENSEKEYDIALOG_H__C8E98A11_BBE9_499F_AB62_6EA12BDBCF84__INCLUDED_)
