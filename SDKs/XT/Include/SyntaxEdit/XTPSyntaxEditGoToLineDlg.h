// XTPSyntaxEditGoToLineDlg.h : header file
//
// This file is a part of the XTREME TOOLKIT PRO MFC class library.
// (c)1998-2007 Codejock Software, All Rights Reserved.
//
// THIS SOURCE FILE IS THE PROPERTY OF CODEJOCK SOFTWARE AND IS NOT TO BE
// RE-DISTRIBUTED BY ANY MEANS WHATSOEVER WITHOUT THE EXPRESSED WRITTEN
// CONSENT OF CODEJOCK SOFTWARE.
//
// THIS SOURCE CODE CAN ONLY BE USED UNDER THE TERMS AND CONDITIONS OUTLINED
// IN THE XTREME TOOLKIT PRO LICENSE AGREEMENT. CODEJOCK SOFTWARE GRANTS TO
// YOU (ONE SOFTWARE DEVELOPER) THE LIMITED RIGHT TO USE THIS SOFTWARE ON A
// SINGLE COMPUTER.
//
// CONTACT INFORMATION:
// support@codejock.com
// http://www.codejock.com
//
/////////////////////////////////////////////////////////////////////////////

//{{AFX_CODEJOCK_PRIVATE
#if !defined(__XTPSYNTAXEDITGOTOLINEDLG_H__)
#define __XTPSYNTAXEDITGOTOLINEDLG_H__
//}}AFX_CODEJOCK_PRIVATE

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//===========================================================================
// CXTPSyntaxEditGoToLineDlg
//===========================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditGoToLineDlg : public CDialog
{
public:

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	CXTPSyntaxEditGoToLineDlg(CWnd* pParent = NULL);

	//{{AFX_DATA(CXTPSyntaxEditGoToLineDlg)
	enum { IDD = XTP_IDD_EDIT_GOTOLINE };
	CEdit   m_wndEditLineNo;
	int     m_iLineNo;
	CString m_csLineNo;
	//}}AFX_DATA

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	BOOL ShowDialog(CXTPSyntaxEditCtrl* pEditCtrl, BOOL bSelectLine = FALSE, BOOL bDontClose = FALSE);

	//{{AFX_VIRTUAL(CXTPSyntaxEditGoToLineDlg)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

protected:

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual BOOL LoadPos();

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	virtual BOOL SavePos();

	//{{AFX_MSG(CXTPSyntaxEditGoToLineDlg)
	afx_msg void OnChangeEditLineNo();
	virtual BOOL OnInitDialog();
	afx_msg void OnBtnGoTo();
	virtual void OnCancel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	int                 m_iMaxLineNo;   // TODO
	BOOL                m_bHideOnFind;  // TODO
	BOOL                m_bSelectLine;  // TODO
	CPoint              m_ptWndPos;     // TODO
	CXTPSyntaxEditCtrl* m_pEditCtrl;    // TODO
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(__XTPSYNTAXEDITGOTOLINEDLG_H__)
