// XTPSyntaxEditColorComboBox.h : header file
//
// This file is a part of the XTREME TOOLKIT PRO MFC class library.
// (c)1998-2007 Codejock Software, All Rights Reserved.
//
// THIS SOURCE FILE IS THE PROPERTY OF CODEJOCK SOFTWARE AND IS NOT TO BE
// RE-DISTRIBUTED BY ANY MEANS WHATSOEVER WITHOUT THE EXPRESSED WRITTEN
// CONSENT OF CODEJOCK SOFTWARE.
//
// THIS SOURCE CODE CAN ONLY BE USED UNDER THE TERMS AND CONDITIONS OUTLINED
// IN THE XTREME SYNTAX EDIT LICENSE AGREEMENT. CODEJOCK SOFTWARE GRANTS TO
// YOU (ONE SOFTWARE DEVELOPER) THE LIMITED RIGHT TO USE THIS SOFTWARE ON A
// SINGLE COMPUTER.
//
// CONTACT INFORMATION:
// support@codejock.com
// http://www.codejock.com
//
/////////////////////////////////////////////////////////////////////////////

//{{AFX_CODEJOCK_PRIVATE
#if !defined(__XTPSYNTAXEDITCOLORCOMBOBOX_H__)
#define __XTPSYNTAXEDITCOLORCOMBOBOX_H__
//}}AFX_CODEJOCK_PRIVATE

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//{{AFX_CODEJOCK_PRIVATE

//===========================================================================
// Summary:
//     TODO
//===========================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditColorComboBox : public CComboBox
{
	DECLARE_DYNAMIC(CXTPSyntaxEditColorComboBox)

public:
	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	CXTPSyntaxEditColorComboBox();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	virtual ~CXTPSyntaxEditColorComboBox();

public:

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	COLORREF GetSelColor();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     crColor - TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	int SetSelColor(COLORREF crColor);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     crColor - TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	int DeleteColor(COLORREF crColor);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     crColor - TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	int FindColor(COLORREF crColor);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     crColor - TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	int SetUserColor(COLORREF crColor, LPCTSTR lpszUserText=NULL);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	COLORREF GetUserColor() const;

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     crColor - TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	int SetAutoColor(COLORREF crColor, LPCTSTR lpszAutoText=NULL);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	COLORREF GetAutoColor() const;

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	int SelectUserColor();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	virtual bool Init();

	//{{AFX_VIRTUAL(CXTPSyntaxEditColorComboBox)
	public:
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDIS);
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMIS);
	virtual int CompareItem(LPCOMPAREITEMSTRUCT lpCIS);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void PreSubclassWindow();
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

protected:

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     crColor - TODO
	//     nID     - TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	virtual int AddColor(COLORREF crColor, UINT nID);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	virtual void NotifyOwner(UINT nCode);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	virtual int GetLBCurSel() const;

	//{{AFX_MSG(CXTPSyntaxEditColorComboBox)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnCloseUp();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:
	int      m_iPrevSel;
	bool     m_bPreInit;
	COLORREF m_crAuto;
	COLORREF m_crUser;
};

/////////////////////////////////////////////////////////////////////////////

AFX_INLINE COLORREF CXTPSyntaxEditColorComboBox::GetUserColor() const {
	return m_crUser;
}
AFX_INLINE COLORREF CXTPSyntaxEditColorComboBox::GetAutoColor() const {
	return m_crAuto;
}

//===========================================================================
// Summary:
//     TODO
// Parameters:
//     pDX   - TODO
//     nIDC  - TODO
//     value - TODO
//===========================================================================
_XTP_EXT_CLASS void AFXAPI DDX_CBSyntaxColor(CDataExchange* pDX, int nIDC, COLORREF& value);


//}}AFX_CODEJOCK_PRIVATE

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(__XTPSYNTAXEDITCOLORCOMBOBOX_H__)
