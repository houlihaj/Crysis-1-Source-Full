////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TabPanel.h
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain modification tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CTabPanel_h__
#define __CTabPanel_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Controls\TabCtrlEx.h"

//////////////////////////////////////////////////////////////////////////
class CTabPanel : public CDialog
{
	DECLARE_DYNCREATE(CTabPanel);

public:
	CTabPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTabPanel();

	int AddPage( const char *sCaption,CDialog *pDlg,bool bAutoDestroy=true );
	void RemovePage( int nPageId );
	void SelectPage( int nPageId );
	int GetPageCount();

	// Dialog Data
	enum { IDD = IDD_PANEL_PROPERTIES };

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTabSelect(NMHDR* pNMHDR, LRESULT* pResult);

	DECLARE_MESSAGE_MAP()

	CTabCtrlEx m_tab;
	static CString m_lastSelectedPage;
};

#endif //__CTabPanel_h__