#if !defined(AFX_GOTOPOSITIONDLG_H__45679345_0046_4354_986F_888F473684E5__INCLUDED_)
#define AFX_GOTOPOSITIONDLG_H__45679345_0046_4354_986F_888F473684E5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GotoPositionDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGotoPositionDlg dialog

class CGotoPositionDlg : public CDialog
{
// Construction
public:
	CGotoPositionDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CGotoPositionDlg)
	enum { IDD = IDD_GOTO };
	// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGotoPositionDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CGotoPositionDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedOk();
	afx_msg void OnUpdateNumbers();
	afx_msg void OnChangeEdit();

	DECLARE_MESSAGE_MAP()


public:

	CString m_sPos;
	CNumberCtrl m_dymX;
	CNumberCtrl m_dymY;
	CNumberCtrl m_dymZ;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GotoPositionDLG_H__45679345_0046_4354_986F_888F473684E5__INCLUDED_)
