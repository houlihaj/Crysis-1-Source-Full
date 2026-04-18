////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   SelectGameTokenDialog.h
//  Version:     v1.00
//  Created:     14/12/2005 by AlexL.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SELECTGAMETOKENDIALOG_H__
#define __SELECTGAMETOKENDIALOG_H__
#pragma once

#include <XTToolkitPro.h>

struct IDataBaseItem;

// CSelectGameToken dialog

class CSelectGameTokenDialog : public CXTResizeDialog
{
	DECLARE_DYNAMIC(CSelectGameTokenDialog)

public:
	CSelectGameTokenDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSelectGameTokenDialog();

	CString GetSelectedGameToken();
	void PreSelectGameToken(const CString& name) {
		m_preselect = name;
	}

	// Dialog Data
	enum { IDD = IDD_SELECT_GAMETOKEN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg void OnTvnSelchangedTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnTvnDoubleClick(NMHDR *pNMHDR, LRESULT *pResult);
	BOOL OnInitDialog();
	
	void ReloadGameTokens();

	DECLARE_MESSAGE_MAP()

	CString m_preselect;
	CTreeCtrl m_tree;
	CImageList m_imageList;
	IDataBaseItem* m_selectedItem;
	std::map<HTREEITEM,IDataBaseItem*> m_itemsMap;
};

#endif // __selectentityclsdialog_h__