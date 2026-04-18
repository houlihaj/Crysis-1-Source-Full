#if !defined(AFX_VERSIONUPDATEDIALOG_H__C8E98A11_BBE9_499F_AB62_6EA12BDBCF84__INCLUDED_)
#define AFX_VERSIONUPDATEDIALOG_H__C8E98A11_BBE9_499F_AB62_6EA12BDBCF84__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// VersionUpdateDialog.h : header file
//

#ifdef USING_LICENSE_PROTECTION

/////////////////////////////////////////////////////////////////////////////
// CVersionUpdateDialog dialog
#include "resource.h"

class CVersionUpdateDialog : public CDialog
{
	DECLARE_DYNAMIC(CVersionUpdateDialog)

		// Construction
public:
	CVersionUpdateDialog(CWnd* pParent = NULL);   // standard constructor
	CVersionUpdateDialog(string sVersion);

	virtual INT_PTR DoModal();

// Dialog Data
	//{{AFX_DATA(CVersionUpdateDialog)
	enum { IDD = IDD_VERSION_UPDATE };

	CStatic m_lState;
	CButton m_cDontShow;
	CButton m_bOk;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVersionUpdateDialog)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CVersionUpdateDialog)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	afx_msg void OnOkButtonClicked();
	DECLARE_MESSAGE_MAP()

private:
	string m_sVersion;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // USING_LICENSE_PROTECTION

#endif // !defined(AFX_VERSIONUPDATEDIALOG_H__C8E98A11_BBE9_499F_AB62_6EA12BDBCF84__INCLUDED_)
