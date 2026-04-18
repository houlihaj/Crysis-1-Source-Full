// XTPSyntaxEditPropertiesDlg.h : header file
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
#if !defined(__XTPSYNTAXEDITPROPERTIESDLG_H__)
#define __XTPSYNTAXEDITPROPERTIESDLG_H__
//}}AFX_CODEJOCK_PRIVATE

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//===========================================================================
// Summary:
//     TODO
//===========================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditPropertiesDlg : public CPropertySheet
{
	DECLARE_DYNAMIC(CXTPSyntaxEditPropertiesDlg)

public:
	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// Parameters:
	//     pWndParent - TODO
	//     plfEdit    - TODO
	// -------------------------------------------------------------------
	CXTPSyntaxEditPropertiesDlg(CXTPSyntaxEditView* pEditView=NULL);

	// -------------------------------------------------------------------
	// Summary:
	//     TODO
	// -------------------------------------------------------------------
	virtual ~CXTPSyntaxEditPropertiesDlg();

	//{{AFX_VIRTUAL(CXTPSyntaxEditPropertiesDlg)
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(CXTPSyntaxEditPropertiesDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CXTPSyntaxEditPropertiesPageEdit    m_Page1;        // TODO
	CXTPSyntaxEditPropertiesPageFont    m_Page2;        // TODO
	CXTPSyntaxEditPropertiesPageColor   m_Page3;        // TODO
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(__XTPSYNTAXEDITPROPERTIESDLG_H__)
