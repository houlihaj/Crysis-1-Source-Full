#if !defined(AFX_VIEWPANE_H__034B5C96_6A7E_4640_B142_4D2EAA760D3F__INCLUDED_)
#define AFX_VIEWPANE_H__034B5C96_6A7E_4640_B142_4D2EAA760D3F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ViewPane.h : header file
//

#include "ViewportTitleDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CViewPane view

class CLayoutViewPane : public CView
{
public:
	CLayoutViewPane();
	virtual ~CLayoutViewPane();

	DECLARE_DYNCREATE(CLayoutViewPane)

// Operations
public:
	// Set get this pane id.
	void SetId( int id ) { m_id = id; }
	int GetId() { return m_id; }

	void SetViewClass( const CString &sClass );
	CString GetViewClass() const;
	
	//! Assign viewport to this pane.
	void SwapViewports( CLayoutViewPane *pView );
	void SwaphViewports( CLayoutViewPane *pView );
	
	void AttachViewport( CWnd *pViewport );
	//! Detach viewport from this pane.
	void DetachViewport();
	// Releases and destroy attached viewport.
	void ReleaseViewport();

	void SetFullscren( bool f );
	bool IsFullscreen() const { return m_bFullscreen; };

	void SetFullscreenViewport( bool b );

	CWnd* GetViewport() { return m_viewport; };

	//////////////////////////////////////////////////////////////////////////
	bool IsActiveView() const { return m_active; };

	void ShowTitleMenu();
	void ToggleMaximize();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLayoutViewPane)
	public:
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	//}}AFX_VIRTUAL

// Implementation
protected:
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnDestroy();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg LRESULT OnViewportTitleChange( WPARAM wParam, LPARAM lParam );
	DECLARE_MESSAGE_MAP()

	void DrawView( CDC &dc );
	void RecalcLayout();

private:
	CString m_viewPaneClass;
	//EViewportType m_viewportType;
	bool m_bFullscreen;
	CViewportTitleDlg m_viewportTitleDlg;

	int m_id;
	int m_nBorder;

	int m_titleHeight;

	CWnd* m_viewport;
	//CToolBar m_toolBar;
	bool m_active;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VIEWPANE_H__034B5C96_6A7E_4640_B142_4D2EAA760D3F__INCLUDED_)
