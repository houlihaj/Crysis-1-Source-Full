////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TaskPanel.h
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain modification tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CTaskPanel_h__
#define __CTaskPanel_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include <XTToolkitPro.h>

//////////////////////////////////////////////////////////////////////////
class CTaskPanel : public CDialog
{
	DECLARE_DYNCREATE(CTaskPanel);

public:
	CTaskPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTaskPanel();

	// Dialog Data
	enum { IDD = IDD_PANEL_PROPERTIES };

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);

	void UpdateTools();

	DECLARE_MESSAGE_MAP()

	CXTPTaskPanel m_wndTaskPanel;
	class CTaskPanelTheme* m_pTheme;
};

#endif //__CTaskPanel_h__