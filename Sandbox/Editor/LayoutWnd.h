#if !defined(AFX_LAYOUTWND_H__9DAFD237_0411_41BB_B7E5_8ED2D6A18670__INCLUDED_)
#define AFX_LAYOUTWND_H__9DAFD237_0411_41BB_B7E5_8ED2D6A18670__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LayoutWnd.h : header file
//

#include "Viewport.h"
#include "InfoBar.h"

class CLayoutViewPane;

/////////////////////////////////////////////////////////////////////////////
// CLayoutWnd window

enum EViewLayout
{
	ET_Layout0,
	ET_Layout1,
	ET_Layout2,
	ET_Layout3,
	ET_Layout4,
	ET_Layout5,
	ET_Layout6,
	ET_Layout7,
	ET_Layout8
};

#define MAX_VIEWPORTS 9

/** Window used as splitter window in Layout.
*/
class CLayoutSplitter : public CSplitterWnd
{
// Construction
	DECLARE_DYNCREATE(CLayoutSplitter)
public:
	CLayoutSplitter();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLayoutWnd)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CLayoutSplitter();

	// Generated message map functions
protected:
	//{{AFX_MSG(CLayoutWnd)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	//////////////////////////////////////////////////////////////////////////
	// Ovveride this for flat look.
	void OnDrawSplitter(CDC* pDC, ESplitType nType, const CRect& rectArg);
	//////////////////////////////////////////////////////////////////////////

	void CreateLayoutView( int row,int col,int id,CCreateContext* pContext );
	void CreateSplitView( int row,int col,ESplitType splitType,CCreateContext* pContext );

private:
	friend class CLayoutWnd;
};

/** Main layout window.
*/
class CLayoutWnd : public CWnd
{
// Construction
	DECLARE_DYNCREATE(CLayoutWnd)
public:
	CLayoutWnd();

// Attributes
public:
	CLayoutViewPane *GetViewPane( int id );

	//! Maximize viewport with specified type.
	void MaximizeViewport( int paneId );

	//! Create specific layout.
	void CreateLayout( EViewLayout layout,bool bBindViewports=true,EViewportType defaultView=ET_ViewportCamera );
	EViewLayout GetLayout() const { return m_layout; }

	//! Save layout window configuration to registry.
	void SaveConfig();
	//! Load layout window configuration from registry.
	bool LoadConfig();

	CLayoutViewPane* FindViewByClass( const CString &viewClassName );
	void ShowViewports( bool bShow );
	void BindViewport( CLayoutViewPane *vp,CString viewClassName,CWnd *pViewport=NULL );
	CString ViewportTypeToClassName( EViewportType viewType );

	//! Switch 2D viewports.
	void Cycle2DViewport();

// Operations
public:
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLayoutWnd)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CLayoutWnd();

	// Generated message map functions
protected:
	//{{AFX_MSG(CLayoutWnd)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	// Bind viewports to split panes.
	void BindViewports();
	void UnbindViewports();

	void CreateSubSplitView( int row,int col,EViewLayout splitType,CCreateContext* pContext );
	void CreateLayoutView( CLayoutSplitter *wndSplitter,int row,int col,int id,EViewportType viewType,CCreateContext* pContext );

	bool CycleViewport( EViewportType from,EViewportType to );

private:
	bool m_bMaximized;

	//! What view type is current maximized.
	EViewLayout m_layout;

	// ViewPane id to viewport type
	CString m_viewType[MAX_VIEWPORTS];

	//! Primary split window.
	CLayoutSplitter *m_splitWnd;
	//! Secondary split window.
	CLayoutSplitter *m_splitWnd2;

	//! View pane for maximized layout.
	CLayoutViewPane *m_maximizedView;
	// Id of maximized view pane.
	int m_maximizedViewId;

	CInfoBar m_infoBar;
	CSize m_infoBarSize;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LAYOUTWND_H__9DAFD237_0411_41BB_B7E5_8ED2D6A18670__INCLUDED_)
