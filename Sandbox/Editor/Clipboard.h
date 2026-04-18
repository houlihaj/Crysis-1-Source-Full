////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   clipboard.h
//  Version:     v1.00
//  Created:     15/8/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __clipboard_h__
#define __clipboard_h__

#if _MSC_VER > 1000
#pragma once
#endif

/** Use this class to put and get stuff from windows clipboard.
*/
class CRYEDIT_API CClipboard
{
public:
	//! Put xml node into clipboard
	void Put( XmlNodeRef &node,const CString &title = "" );
	//! Get xml node to clipboard.
	XmlNodeRef Get() const;

	//! Put string into Windows clipboard.
	void PutString( const CString &text,const CString &title = "" );
	//! Get string from Windows clipboard.
	CString GetString() const;

	//! Return name of what is in clipboard now.
	CString GetTitle() const { return m_title; };

	//! Put image into Windows clipboard.
	void PutImage(const CImage & img);

	//! Get image from Windows clipboard.
	bool GetImage(CImage & img);

	//! Return true if clipboard is empty.
	bool IsEmpty() const;
private:
	static XmlNodeRef m_node;
	static CString m_title;
};


#endif // __clipboard_h__
