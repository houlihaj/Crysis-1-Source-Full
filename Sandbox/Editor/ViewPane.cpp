// ViewPane.cpp : implementation file
//

#include "stdafx.h"
#include "ViewPane.h"
#include "ViewManager.h"

#include "LayoutWnd.h"
#include "Viewport.h"
#include "LayoutConfigDialog.h"
#include "MainFrm.h"
#include "TopRendererWnd.h"

#define ID_MAXIMIZED 50000
#define ID_LAYOUT_CONFIG 50001

#define FIRST_ID_CLASSVIEW 50200
#define LAST_ID_CLASSVIEW 50299

#define VIEW_BORDER 0

/////////////////////////////////////////////////////////////////////////////
// CLayoutViewPane

IMPLEMENT_DYNCREATE(CLayoutViewPane, CView)

BEGIN_MESSAGE_MAP(CLayoutViewPane, CView)
	//{{AFX_MSG_MAP(CLayoutViewPane)
	ON_WM_CREATE()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_DESTROY()
	ON_WM_MOUSEWHEEL()
	ON_WM_RBUTTONDOWN()
	ON_WM_SETFOCUS()
	ON_MESSAGE(WM_VIEWPORT_ON_TITLE_CHANGE, OnViewportTitleChange)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CLayoutViewPane::CLayoutViewPane()
{
	m_viewport = 0;
	m_active = 0;
	m_nBorder = VIEW_BORDER;

	m_titleHeight = 16;
	m_bFullscreen = false;
	m_viewportTitleDlg.SetViewPane(this);

	m_id = -1;
}

//////////////////////////////////////////////////////////////////////////
CLayoutViewPane::~CLayoutViewPane()
{
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::SetViewClass( const CString &sClass )
{
	if (m_viewPaneClass == sClass && m_viewport != 0)
		return;
	m_viewPaneClass = sClass;

	ReleaseViewport();

	IClassDesc *pClass = GetIEditor()->GetClassFactory()->FindClass( sClass );
	if (pClass)
	{
		IViewPaneClass *pViewClass = NULL;
		HRESULT hRes = pClass->QueryInterface( __uuidof(IViewPaneClass),(void**)&pViewClass );
		if (!FAILED(hRes))
		{
			CWnd *pNewView = (CWnd*)pViewClass->GetRuntimeClass()->CreateObject();
			pNewView->SetWindowText( m_viewPaneClass );
			AttachViewport( pNewView );
		}
	}

}

//////////////////////////////////////////////////////////////////////////
CString CLayoutViewPane::GetViewClass() const
{
	return m_viewPaneClass;
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::OnDestroy() 
{
	ReleaseViewport();

	CView::OnDestroy();
}

/////////////////////////////////////////////////////////////////////////////
// CLayoutViewPane drawing

void CLayoutViewPane::OnDraw(CDC* pDC)
{
	////////////////////////////////////////////////////////////////////////
	// Paint the window with a gray brush and draw the view controls and
	// the view's caption. Also draw the context menu button
	////////////////////////////////////////////////////////////////////////

	//CPaintDC dc(this);
	CDC &dc = *pDC;
/*
	CDC cBmpDC;
    CRect rect, rectScreen;
	CBrush cFillBrush;
	CString cWindowText;
	CFont cFont;
	CBitmap cViewTools;
	CBrush cGreenBrush(0x0022FF22);
	CBrush cWhiteBrush(0x00FFFFFF);

	//bool active = GetFocus() == m_viewport;
	bool active = m_active;
	
	// Create a font and select it into the DC. make the font bold if this is the
	// active view
	VERIFY(cFont.CreateFont(-::MulDiv(8, GetDeviceCaps(dc.m_hDC, LOGPIXELSY), 72), 0, 0, 0, 
		(active) ? FW_BOLD : FW_DONTCARE, FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
		DEFAULT_PITCH, "Tahoma"));
	dc.SelectObject(cFont);

	// Get the rect of the client window
	GetClientRect(&rect);

	CRect vpRect( m_nBorder,m_nBorder+m_titleHeight-1,rect.right-m_nBorder-1,rect.bottom-m_nBorder-m_titleHeight );

	if (!m_viewport)
		dc.FillRect(&rect, &cWhiteBrush);

	bool bHighlightTitle = false;
	if (m_viewport)
	{
		if (m_viewport->IsAVIRecording() || GetIEditor()->GetAnimation()->IsRecording())
			bHighlightTitle = true;
	}

	COLORREF cTextColor = dc.GetTextColor();
	
	// Create a Title filled rect.
	rect.bottom = m_titleHeight;
	if (!bHighlightTitle)
	{
		cFillBrush.m_hObject = ::GetSysColorBrush(COLOR_BTNFACE);
	}
	else
	{
		cFillBrush.CreateSolidBrush(RGB(255,0,0));
		cTextColor = RGB(255,255,255);
	}
	dc.FillRect(&rect, &cFillBrush);


	// Get a client rect and prepare writing the view's name	
	GetClientRect(&rect);
	GetWindowText(cWindowText);
	dc.SetBkMode(TRANSPARENT);

	CString sTitleText = cWindowText;
	sTitleText.Format( "%s (%dx%d)",(const char*)cWindowText,(int)vpRect.Width(),(int)vpRect.Height() );

	COLORREF cPrevTextColor = dc.SetTextColor( cTextColor );
	// Write the view's name
	rect.top += 1;
	dc.DrawText(sTitleText, rect, DT_CENTER);
	dc.SetTextColor( cPrevTextColor );

	// Draw the button for the view's menu
	GetClientRect(&rect);
	rect.left = rect.right - 12;
	rect.right -= 3;
	rect.top = 3;
	rect.bottom = 12;
	dc.DrawFrameControl(&rect, DFC_BUTTON, ((active) ? DFCS_BUTTONPUSH : DFCS_BUTTONPUSH) | DFCS_FLAT);
*/


	/*
	CRect rect;
	// Get the rect of the client window
	GetClientRect(&rect);
	CPen pen;
	if (m_active)
		pen.CreatePen(PS_SOLID, 1, RGB(255,255,0));
	else
		pen.CreatePen(PS_SOLID, 1, RGB(180,180,180));

	CPen *pPrevPen = dc.SelectObject( &pen );
	dc.MoveTo( 0,0 );
	dc.LineTo( rect.right-1,0 );
	dc.LineTo( rect.right-1,rect.bottom-1 );
	dc.LineTo( 0,rect.bottom-1 );
	dc.LineTo( 0,0 );

	dc.SelectObject( pPrevPen );
	*/
}

/////////////////////////////////////////////////////////////////////////////
// CLayoutViewPane diagnostics

#ifdef _DEBUG
void CLayoutViewPane::AssertValid() const
{
	//CView::AssertValid();
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::Dump(CDumpContext& dc) const
{
	//CView::Dump(dc);
}

#endif //_DEBUG

//////////////////////////////////////////////////////////////////////////
int CLayoutViewPane::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	CLogFile::WriteLine("Creating view...");
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_viewportTitleDlg.Create(CViewportTitleDlg::IDD,this);
	m_viewportTitleDlg.ShowWindow(SW_SHOW);

	CRect rc;
	m_viewportTitleDlg.GetClientRect(rc);
	m_titleHeight = rc.Height();
	
	// TODO: Add your specialized creation code here
	//m_viewport.Create( this,"EditWnd",m_editSink );

	// Create the standard toolbar
	//m_toolBar.CreateEx(this, TBSTYLE_FLAT,WS_CHILD|WS_VISIBLE|CBRS_ALIGN_TOP);
	//m_toolBar.LoadToolBar(IDR_VIEW);
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::SwapViewports( CLayoutViewPane *pView )
{
	CWnd *pViewport = pView->GetViewport();
	CWnd *pViewportOld = m_viewport;


	std::swap( m_viewPaneClass,pView->m_viewPaneClass );

	AttachViewport( pViewport );
	pView->AttachViewport(pViewportOld);
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::AttachViewport( CWnd *pViewport )
{
	if (pViewport == m_viewport)
		return;

	m_viewport = pViewport;
	if (pViewport)
	{
		m_viewport->ModifyStyle( WS_POPUP,WS_CHILD,0 );
		m_viewport->SetParent( this );

		RecalcLayout();
		m_viewport->ShowWindow( SW_SHOW );
		m_viewport->Invalidate(FALSE);

		SetWindowText( m_viewPaneClass );
		CString title;
		pViewport->GetWindowText(title);
		m_viewportTitleDlg.SetTitle( title );
		Invalidate(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::DetachViewport()
{
	m_viewport = 0;
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::ReleaseViewport()
{
	if (m_viewport && m_viewport->m_hWnd != 0)
	{
		m_viewport->DestroyWindow();
		m_viewport = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::OnSize(UINT nType, int cx, int cy) 
{
	CView::OnSize(nType, cx, cy);

	int toolHeight = 0;
	
	/*
	// TODO: Add your message handler code here
	if (m_toolBar)
	{
		CRect rc;
		m_toolBar.MoveWindow( 0,0,cx-1,20 );
		m_toolBar.GetWindowRect( rc );
		toolHeight = rc.bottom - rc.top;
	}
	*/

	RecalcLayout();

	if (m_viewportTitleDlg.m_hWnd)
	{
		int w = 0;
		int h = 0;
		if (m_viewport)
		{
			CRect rc;
			m_viewport->GetClientRect(rc);
			w = rc.Width();
			h = rc.Height();
		}
		m_viewportTitleDlg.SetViewportSize( w,h );
	}
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::RecalcLayout()
{
	CRect rc;
	GetClientRect(rc);
	int cx = rc.Width();
	int cy = rc.Height();

	int nBorder = VIEW_BORDER;

	if (m_viewportTitleDlg.m_hWnd)
	{
		m_viewportTitleDlg.MoveWindow( m_nBorder,m_nBorder,cx-m_nBorder,m_titleHeight );
	}

	if (m_viewport)
		m_viewport->MoveWindow( m_nBorder,m_nBorder+m_titleHeight,cx-m_nBorder,cy-m_nBorder*2-m_titleHeight );
}

//////////////////////////////////////////////////////////////////////////
BOOL CLayoutViewPane::OnEraseBkgnd(CDC* pDC) 
{
	// TODO: Add your message handler code here and/or call default
	// Do nothing.
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView) 
{
	// TODO: Add your specialized code here and/or call the base class
	CView::OnActivateView(bActivate, pActivateView, pDeactiveView);

	m_active = bActivate;
	m_viewportTitleDlg.SetActive( m_active );
	/*
	m_active = bActivate;
	if (bActivate)
	{
		if (m_viewport)
		{
			m_viewport->SetActive(true);
			m_viewport->SetFocus();
		}
	}
	else
	{
		if (m_viewport)
			m_viewport->SetActive(false);
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::ToggleMaximize()
{
	////////////////////////////////////////////////////////////////////////
	// Switch in and out of fullscreen mode for a edit view
	////////////////////////////////////////////////////////////////////////
	CLayoutWnd *wnd = GetIEditor()->GetViewManager()->GetLayout();
	if (wnd)
		wnd->MaximizeViewport( GetId() );
	SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::ShowTitleMenu()
{
	////////////////////////////////////////////////////////////////////////
	// Process clicks on the view buttons and the menu button
	////////////////////////////////////////////////////////////////////////
	RECT rcClient;
	// Only continue when we have a viewport.

	CPoint point;
	GetCursorPos(&point);
	// Call the event handler
	GetClientRect(&rcClient);
	//ClientToScreen( &point );
	point.x += 10;

	// Create pop up menu.
	CMenu m;
	CMenu viewsMenu;
	CMenu extViewsMenu;
	m.CreatePopupMenu();

	viewsMenu.CreatePopupMenu();
	extViewsMenu.CreatePopupMenu();

	if (m_viewport && m_viewport->IsKindOf(RUNTIME_CLASS(CViewport)))
	{
		((CViewport*)m_viewport)->OnTitleMenu( m );
	}
	if (m.GetMenuItemCount() > 0)
	{
		m.AppendMenu( MF_SEPARATOR,0,"" );
	}
	m.AppendMenu( MF_STRING|((IsFullscreen())?MF_CHECKED:MF_UNCHECKED),ID_MAXIMIZED,"Maximized" );
	m.AppendMenu( MF_POPUP,(UINT_PTR)viewsMenu.GetSafeHmenu(),"View" );
	m.AppendMenu( MF_STRING,ID_LAYOUT_CONFIG,"Configure Layout..." );

	std::vector<IViewPaneClass*> viewPaneClasses;

	int nViews = 0;
	int i;
	std::vector<CViewportDesc*> vdesc;
	GetIEditor()->GetViewManager()->GetViewportDescriptions( vdesc );
	for (i = 0; i < vdesc.size(); i++)
	{
		IViewPaneClass *pViewClass = (IViewPaneClass*)vdesc[i]->pViewClass;
		if (!pViewClass)
			continue;
		int flags = MF_STRING;
		if (pViewClass->ClassName() == m_viewPaneClass)
			flags |= MF_CHECKED;
		viewPaneClasses.push_back(pViewClass);
		viewsMenu.AppendMenu( flags, FIRST_ID_CLASSVIEW+(nViews++),vdesc[i]->name );
	}

	viewsMenu.AppendMenu( MF_SEPARATOR,0,"" );

	std::vector<IClassDesc*> classes;
	GetIEditor()->GetClassFactory()->GetClassesBySystemID( ESYSTEM_CLASS_VIEWPANE,classes );
	for (i = 0; i < classes.size(); i++)
	{
		IClassDesc *pClass = classes[i];
		IViewPaneClass *pViewClass = NULL;
		HRESULT hRes = pClass->QueryInterface( __uuidof(IViewPaneClass),(void**)&pViewClass );
		if (FAILED(hRes))
		{
			continue;
		}
		if (stl::find(viewPaneClasses,pViewClass))
			continue;
		int flags = MF_STRING;
		if (pViewClass->ClassName() == m_viewPaneClass)
			flags |= MF_CHECKED;
		viewPaneClasses.push_back(pViewClass);
		viewsMenu.AppendMenu( flags, FIRST_ID_CLASSVIEW+(nViews++),pViewClass->GetPaneTitle() );
	}

	/*
	int ViewId = 101;
	// Append other viewes.
	std::vector<CViewport*> views;
	CViewManager::Instance()->GetViews( views );
	for (int i = 0; i < views.size(); i++)
	{
	m.AppendMenu( MF_STRING|((IsFullscreen())?MF_CHECKED:MF_UNCHECKED),ViewId+i,views[i]->GetViewName() );
	}
	*/

	int id = m.TrackPopupMenu( TPM_RETURNCMD|TPM_LEFTALIGN|TPM_LEFTBUTTON,point.x,point.y,this );
	SetFocus();
	// Ids above 100 reserved for view pane commands.
	if (id < ID_MAXIMIZED)
	{
		if (m_viewport && m_viewport->IsKindOf(RUNTIME_CLASS(CViewport)))
		{
			((CViewport*)m_viewport)->OnTitleMenuCommand( id );
		}
	}
	else if (id >= FIRST_ID_CLASSVIEW && id <= LAST_ID_CLASSVIEW)
	{
		if (id-FIRST_ID_CLASSVIEW < viewPaneClasses.size())
		{
			CLayoutWnd *layout = GetIEditor()->GetViewManager()->GetLayout();
			layout->BindViewport( this,viewPaneClasses[id-FIRST_ID_CLASSVIEW]->ClassName() );
		}
	}
	else {
		CLayoutWnd *layout = GetIEditor()->GetViewManager()->GetLayout();
		switch (id)
		{
		case ID_MAXIMIZED:
			if (m_viewport && layout)
			{
				layout->MaximizeViewport( GetId() );
				//SetFullscreenViewport(true);
			}
			break;

		case ID_LAYOUT_CONFIG:
			{
				if (layout)
				{
					CLayoutConfigDialog dlg;
					dlg.SetLayout( layout->GetLayout() );
					if (dlg.DoModal() == IDOK)
					{
						// Will kill this Pane. so must be last line in this function.
						layout->CreateLayout( dlg.GetLayout() );
					}
				}
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::OnRButtonDown(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	CView::OnRButtonDown(nFlags, point);
}

void CLayoutViewPane::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	ToggleMaximize();
}

BOOL CLayoutViewPane::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
{
	// TODO: Add your message handler code here and/or call default
	
	return CView::OnMouseWheel(nFlags, zDelta, pt);
}

void CLayoutViewPane::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	if (m_viewport && m_viewport->IsKindOf(RUNTIME_CLASS(CViewport)))
	{
		((CViewport*)m_viewport)->UpdateContent( 0xFFFFFFFF );
	}
}

void CLayoutViewPane::OnSetFocus(CWnd* pOldWnd) 
{
	CView::OnSetFocus(pOldWnd);
	
	// Forward SetFocus call to child viewport.
	if (m_viewport)
		m_viewport->SetFocus();
}

BOOL CLayoutViewPane::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
	// Extend the framework's command route from the view to viewport.
   //if ((m_viewport != NULL)
     // && m_viewport->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		//return TRUE;

	
	return CView::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CLayoutViewPane::SetFullscren( bool f )
{
	m_bFullscreen = f;
}

//////////////////////////////////////////////////////////////////////////
void CLayoutViewPane::SetFullscreenViewport( bool b )
{
	if (!m_viewport)
		return;

	if (b)
	{
		m_viewport->SetParent( 0 );
		m_viewport->ModifyStyle( WS_CHILD,WS_POPUP,0 );
		
		GetIEditor()->GetRenderer()->ChangeResolution( 800,600,32,80,true );
	}
	else
	{
		m_viewport->SetParent( this );
		m_viewport->ModifyStyle( WS_POPUP,WS_CHILD,0 );
		GetIEditor()->GetRenderer()->ChangeResolution( 800,600,32,80,false );
	}
}

//////////////////////////////////////////////////////////////////////////
LRESULT CLayoutViewPane::OnViewportTitleChange( WPARAM wParam, LPARAM lParam )
{
	if (m_viewport)
	{
		CString title;
		m_viewport->GetWindowText(title);
		m_viewportTitleDlg.SetTitle( title );
	}
	return FALSE;
}
