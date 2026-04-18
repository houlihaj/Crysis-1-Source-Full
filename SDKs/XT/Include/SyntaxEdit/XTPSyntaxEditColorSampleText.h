// XTPSyntaxEditColorSampleText.h : header file
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
#if !defined(__XTPSYNTAXEDITPROPERTIESSAMPLETEXT_H__)
#define __XTPSYNTAXEDITPROPERTIESSAMPLETEXT_H__
//}}AFX_CODEJOCK_PRIVATE

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Common/XTPColorManager.h"

//{{AFX_CODEJOCK_PRIVATE

//===========================================================================
// Summary:
//     TODO
//===========================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditColorSampleText : public CStatic
{
public:
	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	CXTPSyntaxEditColorSampleText();

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	virtual ~CXTPSyntaxEditColorSampleText();

public:
	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     crText - TODO
	// -------------------------------------------------------------------
	void SetTextColor(COLORREF crText);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	COLORREF GetTextColor() const;

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     crBack - TODO
	// -------------------------------------------------------------------
	void SetBackColor(COLORREF crBack);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	COLORREF GetBackColor() const;

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	void SetBorderColor(COLORREF crBorder);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Returns:
	//     TODO
	// -------------------------------------------------------------------
	COLORREF GetBorderColor() const;

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	void Refresh();

	//{{AFX_VIRTUAL(CXTPSyntaxEditColorSampleText)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CXTPSyntaxEditColorSampleText)
	afx_msg void OnNcPaint();
	afx_msg void OnPaint();
	afx_msg void OnSysColorChange();
	afx_msg void OnEnable(BOOL bEnable);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:
	CXTPPaintManagerColor m_crBack;
	CXTPPaintManagerColor m_crText;
	CXTPPaintManagerColor m_crBorder;
};
//}}AFX_CODEJOCK_PRIVATE


#endif // !defined(__XTPSYNTAXEDITPROPERTIESSAMPLETEXT_H__)
