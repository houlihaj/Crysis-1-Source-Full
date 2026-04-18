#if !defined(AFX_MATEDITMAINDLG_H__AFB020BD_DBC3_40C0_A03C_8531A6520F3B__INCLUDED_)
#define AFX_MATEDITMAINDLG_H__AFB020BD_DBC3_40C0_A03C_8531A6520F3B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MatEditMainDlg.h : header file
//

class CMaterialDialog;

/////////////////////////////////////////////////////////////////////////////
// CMatEditMainDlg dialog
class CMatEditMainDlg : public CDialog
{
	// Construction
public:
	CMatEditMainDlg( const char *title = NULL,CWnd* pParent = NULL);   // standard constructor

	// Dialog Data
	//{{AFX_DATA(CMatEditMainDlg)
	enum { IDD = IDD_MATEDITMAINDLG };
	//}}AFX_DATA

	//Functions


	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMatEditMainDlg)
protected:
	//virtual void DoDataExchange(CDataExchange* pDX);
	//}}AFX_VIRTUAL

	// Implementation
protected:

	CMaterialDialog * m_MatDlg;

	// Generated message map functions
	//{{AFX_MSG(CMatEditMainDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnMatEditSend(WPARAM, LPARAM);
	afx_msg LRESULT OnKickIdle(WPARAM wParam, LPARAM);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MATEDITMAINDLG_H__AFB020BD_DBC3_40C0_A03C_8531A6520F3B__INCLUDED_)
