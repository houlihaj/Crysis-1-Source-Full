
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SubObjSelectionPanel.h
//  Version:     v1.00
//  Created:     26/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SubObjSelectionTypePanel_h__
#define __SubObjSelectionTypePanel_h__
#pragma once

#include <XTToolkitPro.h>
// CSubObjSelectionTypePanel dialog

class CSubObjSelectionTypePanel : public CDialog
{
	DECLARE_DYNAMIC(CSubObjSelectionTypePanel)

public:
	CSubObjSelectionTypePanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSubObjSelectionTypePanel();

	void SelectElemtType( int nIndex );

// Dialog Data
	enum { IDD = IDD_PANEL_SUB_SELTYPE };

protected:
	virtual void OnOK() {};
	virtual void OnCancel();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support


	virtual BOOL OnInitDialog();
	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()

	CXTPTaskPanel m_wndTaskPanel;
	CImageList m_imageList;
};

#endif // __SubObjSelectionTypePanel_h__