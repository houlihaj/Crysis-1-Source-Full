// XTPSyntaxEditSectionManager.h: interface for the CXTPSyntaxEditSectionManager class.
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
//////////////////////////////////////////////////////////////////////

//{{AFX_CODEJOCK_PRIVATE
#if !defined(__XTPSYNTAXEDITSECTIONMANAGER_H__)
#define __XTPSYNTAXEDITSECTIONMANAGER_H__
//}}AFX_CODEJOCK_PRIVATE

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//=======================================================================
// struct: XTP_EDIT_SCHEMAFILEINFO
//=======================================================================
struct _XTP_EXT_CLASS XTP_EDIT_SCHEMAFILEINFO
{
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	XTP_EDIT_SCHEMAFILEINFO();
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	XTP_EDIT_SCHEMAFILEINFO(const XTP_EDIT_SCHEMAFILEINFO& info);
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	XTP_EDIT_SCHEMAFILEINFO(const CString& srcName, const CString& srcValue);

	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	const XTP_EDIT_SCHEMAFILEINFO& operator=(const XTP_EDIT_SCHEMAFILEINFO& info);
	BOOL operator==(const XTP_EDIT_SCHEMAFILEINFO& info) const;

public:
	CString csName;     // TODO
	CString csValue;    // TODO
	CString csDesc;     // TODO
	UINT    uValue;     // TODO
};

//=======================================================================
// class: CXTPSyntaxEditSchemaFileInfoList
//=======================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditSchemaFileInfoList : public CList<XTP_EDIT_SCHEMAFILEINFO,XTP_EDIT_SCHEMAFILEINFO&>
{
public:
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	POSITION LookupName (const CString& csName, XTP_EDIT_SCHEMAFILEINFO& info);
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	POSITION LookupValue(const CString& csValue, XTP_EDIT_SCHEMAFILEINFO& info);
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	POSITION LookupValue(const UINT& uValue, XTP_EDIT_SCHEMAFILEINFO& info);
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	POSITION LookupDesc (const CString& csDesc, XTP_EDIT_SCHEMAFILEINFO& info);
};

//=======================================================================
// class: CXTPSyntaxEditSectionManager
//=======================================================================
class _XTP_EXT_CLASS CXTPSyntaxEditSectionManager
{
public:
	CXTPSyntaxEditSectionManager();
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	virtual ~CXTPSyntaxEditSectionManager();

public:
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	int GetSectionNames(CStringArray& arSections, LPCTSTR lpszFilePath);
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	int GetSectionKeyList(CXTPSyntaxEditSchemaFileInfoList& infoList, LPCTSTR lpszFilePath, LPCTSTR lpszSectionName);
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	BOOL ParseSectionKey(XTP_EDIT_SCHEMAFILEINFO& info, const CString& csBuffer);
	//-------------------------------------------------------------------
	// Summary: TODO
	//-------------------------------------------------------------------
	BOOL SplitString(CString& csLeft, CString& csRight, const CString& csBuffer, TCHAR chSep);
};


//////////////////////////////////////////////////////////////////////

#endif // !defined(__XTPSYNTAXEDITSECTIONMANAGER_H__)
