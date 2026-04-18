#if !defined(AFX_SOURCECONTROLDESCDLG_H__83F79345_0046_4B61_986F_888F473684E5__INCLUDED_)
#define AFX_SOURCECONTROLDESCDLG_H__83F79345_0046_4B61_986F_888F473684E5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SourceControlDescDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSourceControlDescDlg dialog

class CSourceControlDescDlg : public CDialog
{
// Construction
public:
	CSourceControlDescDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CSourceControlDescDlg)
	enum { IDD = IDD_SOURCECONTROL_DESC };
	// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSourceControlDescDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSourceControlDescDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedOk();

public:

	CString m_sDesc;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SOURCECONTROLDESCDLG_H__83F79345_0046_4B61_986F_888F473684E5__INCLUDED_)
