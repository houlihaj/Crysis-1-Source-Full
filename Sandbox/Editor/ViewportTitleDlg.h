////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ViewportTitleDlg.h
//  Version:     v1.00
//  Created:     15/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ViewportTitleDlg_h__
#define __ViewportTitleDlg_h__
#pragma once

#include <XTToolkitPro.h>

// CViewportTitleDlg dialog
class CLayoutViewPane;

//////////////////////////////////////////////////////////////////////////
class CViewportTitleDlg : public CXTResizeDialog, public IEditorNotifyListener
{
	DECLARE_DYNAMIC(CViewportTitleDlg)

public:
	CViewportTitleDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CViewportTitleDlg();

	void SetViewPane( CLayoutViewPane *pViewPane );
	void SetTitle( const CString &title );
	void SetViewportSize( int width,int height );
	void SetActive( bool bActive );

// Dialog Data
	enum { IDD = IDD_VIEWPORT_TITLE };

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );

	afx_msg void OnSetFocus( CWnd* );
	afx_msg void OnButtonSetFocus();
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnTitle();
	afx_msg void OnMaximize();
	afx_msg void OnHideHelpers();
	afx_msg void OnSize( UINT nType,int cx,int cy );

	DECLARE_MESSAGE_MAP()

	CString m_title;
	CCustomButton m_maxBtn;
	CColorCheckBox m_hideHelpersBtn;
	CStatic m_sizeTextCtrl;

	CLayoutViewPane *m_pViewPane;

	CStatic m_titleBtn;
};

#endif //__ViewportTitleDlg_h__