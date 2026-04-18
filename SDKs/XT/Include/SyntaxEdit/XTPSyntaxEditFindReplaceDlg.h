// XTPSyntaxEditFindReplaceDlg.h : header file
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
#if !defined(__XTPSYNTAXEDITFINDREPLACEDLG_H__)
#define __XTPSYNTAXEDITFINDREPLACEDLG_H__
//}}AFX_CODEJOCK_PRIVATE

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CXTPSyntaxEditView;

//{{AFX_CODEJOCK_PRIVATE

//===========================================================================
// CXTPSyntaxEditFindReplaceDlg
//===========================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditFindReplaceDlg : public CDialog
{
public:

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	CXTPSyntaxEditFindReplaceDlg(CWnd* pParentWnd = NULL);

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	CXTPSyntaxEditFindReplaceDlg(UINT nIDTemplate, CWnd* pParentWnd = NULL);

	//{{AFX_DATA(CXTPSyntaxEditFindReplaceDlg)
	enum { IDD = XTP_IDD_EDIT_SEARCH_REPLACE };
	int m_nSearchDirection;
	BOOL m_bMatchWholeWord;
	BOOL m_bMatchCase;
	CString m_csFindText;
	CString m_csReplaceText;
	CButton m_btnFindNext;
	CButton m_btnRadioUp;
	CButton m_btnRadioDown;
	CButton m_btnReplace;
	CButton m_btnReplaceAll;
	CComboBox m_wndFindCombo;
	CComboBox m_wndReplaceCombo;
	//}}AFX_DATA

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	BOOL ShowDialog(CXTPSyntaxEditView* pEditView, BOOL bReplaceDlg=FALSE);

protected:

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	void UpdateHistoryCombo(const CString& csText, CStringArray& arHistory);

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	void UpdateHistoryCombo(const CString& csText, CStringArray& arHistory, CComboBox& wndCombo);

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	void InitHistoryCombo(CStringArray& arHistory, CComboBox& wndCombo);

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	BOOL LoadHistory();

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	BOOL SaveHistory();

	//-----------------------------------------------------------------------
	// Summary: TODO
	//-----------------------------------------------------------------------
	void EnableControls();

	//{{AFX_VIRTUAL(CXTPSyntaxEditFindReplaceDlg)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual void OnCancel();
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CXTPSyntaxEditFindReplaceDlg)
	afx_msg void OnEditChangeComboFind();
	afx_msg void OnSelendOkComboFind();
	afx_msg void OnChkMatchWholeWord();
	afx_msg void OnChkMatchCase();
	afx_msg void OnRadioUp();
	afx_msg void OnRadioDown();
	afx_msg void OnBtnFindNext();
	afx_msg void OnBtnReplace();
	afx_msg void OnBtnReplaceAll();
	afx_msg void OnEditChangeComboReplace();
	afx_msg void OnSelendOkComboReplace();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

protected:

	BOOL                m_bReplaceDlg;      // TODO
	CPoint              m_ptWndPos;         // TODO
	CXTPSyntaxEditView* m_pEditView;        // TODO
};

//}}AFX_CODEJOCK_PRIVATE

#endif // !defined(__XTPSYNTAXEDITFINDREPLACEDLG_H__)
