////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   AIDebugger.h
//  Version:     v1.00
//  Created:     08-03-2005 by Kirill Bulatsev
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History: 26-06-2006 AI Signal View replaces AI Debugger class
//           29-06-2006 Most existing functionality is moved into a View subwindow
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AIDebugger_h__
#define __AIDebugger_h__
#pragma once

#include <list>
#include <IAISystem.h>


class CAIDebuggerView;

//////////////////////////////////////////////////////////////////////////
//
// Main Dialog for AI Debugger
//
//////////////////////////////////////////////////////////////////////////
class CAIDebugger
	: public CXTPFrameWnd
	, public IEditorNotifyListener
{
	DECLARE_DYNCREATE(CAIDebugger)
public:
	static void RegisterViewClass();

	CAIDebugger(CWnd* pParent = NULL);
	~CAIDebugger();

	void RecalcLayout( BOOL bNotify = TRUE );

protected:
	DECLARE_MESSAGE_MAP()

	BOOL Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd );

	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy();  

	afx_msg void OnDestroy();
	afx_msg void OnClose();  
	// Have to catch this and pass it down. Don't know why.
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg BOOL OnEraseBkgnd( CDC* pDC );
	afx_msg LRESULT OnTaskPanelNotify(WPARAM wParam, LPARAM lParam);    
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra,AFX_CMDHANDLERINFO* pHandlerInfo);

	afx_msg void FileLoad(void);	
	afx_msg void FileSave(void);	
	afx_msg void FileLoadAs(void);	
	afx_msg void FileSaveAs(void);	

	// Called by the editor to notify the listener about the specified event.
	void OnEditorNotifyEvent( EEditorNotifyEvent event );

	void UpdateAll(void);

protected:
	
	CAIDebuggerView *m_pView;

};

#endif // __AIDebugger_h__
