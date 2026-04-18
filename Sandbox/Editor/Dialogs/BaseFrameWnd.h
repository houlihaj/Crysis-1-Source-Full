#ifndef __BaseFrameWnd__
#define __BaseFrameWnd__

#pragma once

//////////////////////////////////////////////////////////////////////////
//
// Base class for Sandbox frame windows
//
//////////////////////////////////////////////////////////////////////////
class CBaseFrameWnd : public CXTPFrameWnd
{
public:
	CBaseFrameWnd();

	CXTPDockingPaneManager* GetDockingPaneManager() { return &m_paneManager; }
	BOOL Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd );

	// Will automatically save and restore frame docking layout.
	// Place at the end of the window dock panes creation.
	void AutoLoadFrameLayout( const CString &name,int nVersion=0 );

protected:

	// This methods are used to save and load layout of the frame to the registry.
	// Activated by the call to AutoSaveFrameLayout
	virtual void LoadFrameLayout();
	virtual void SaveFrameLayout();

	DECLARE_MESSAGE_MAP()

	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void PostNcDestroy();

	afx_msg void OnDestroy();

	//////////////////////////////////////////////////////////////////////////
	// Implement in derived class
	//////////////////////////////////////////////////////////////////////////
	virtual BOOL OnInitDialog() { return TRUE; };
	virtual LRESULT OnDockingPaneNotify(WPARAM wParam, LPARAM lParam) { return FALSE; }
	//////////////////////////////////////////////////////////////////////////

protected:
	// Docking panes manager.
	CXTPDockingPaneManager m_paneManager;
	CString m_frameLayoutKey;
	int m_frameLayoutVersion;
};

#endif //__BaseFrameWnd__ 
