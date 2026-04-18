////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FacialSlidersDialog.cpp
//  Version:     v1.00
//  Created:     4/11/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FacialSlidersDialog.h"
#include "FacialSlidersCtrl.h"

#define IDC_TABCTRL 1
#define TAB_MORPH_TARGETS 0
#define TAB_ALL_EFFECTORS 1

IMPLEMENT_DYNAMIC(CFacialSlidersDialog,CDialog)

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CFacialSlidersDialog, CDialog)
	ON_WM_SIZE()
	ON_NOTIFY( TCN_SELCHANGE, IDC_TABCTRL, OnTabSelect )
	ON_COMMAND( ID_FACEIT_SLIDERS_CLEAR_ALL,OnClearAllSliders )
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
// CFacialSlidersDialog
//////////////////////////////////////////////////////////////////////////
CFacialSlidersDialog::CFacialSlidersDialog() : CDialog(IDD,NULL)
{
	m_pContext = 0;
	m_hAccelerators = 0;
}

//////////////////////////////////////////////////////////////////////////
CFacialSlidersDialog::~CFacialSlidersDialog()
{
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	
	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersDialog::RecalcLayout()
{
	if (m_tabCtrl.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);

		CSize sz = m_wndToolBar.CalcDockingLayout(32000, LM_HORZ|LM_HORZDOCK|LM_STRETCH|LM_COMMIT);
		m_wndToolBar.MoveWindow(CRect(0,0,sz.cx,sz.cy),FALSE);
		rc.top += sz.cy;

		CRect rcInTab = rc;

		int slidersY = 0;
		int sel = m_tabCtrl.GetCurSel();
		
		int numItems = m_slidersCtrl[sel].GetItemCount();
		CRect rcSliderItem;
		m_slidersCtrl[sel].GetItemRect(0,rcSliderItem,LVIR_BOUNDS );
		slidersY = numItems*rcSliderItem.Height() + 20;

		CRect irc;
		m_tabCtrl.GetItemRect( 0,irc );

		int tabHeaderHeight = irc.Height()+2;

		CRect rcTab(rc);
		rcTab.bottom = rcTab.top + slidersY + tabHeaderHeight+2;
//		rcTab.bottom = rc.bottom/2;
		if (rcTab.bottom > rc.bottom)
			rcTab.bottom = rc.bottom;
		m_tabCtrl.MoveWindow(rcTab, FALSE);
		m_tabCtrl.Invalidate();

		m_tabCtrl.GetClientRect(rcInTab);
		rcInTab.top = tabHeaderHeight;
		rcInTab.left += 0;
		rcInTab.right -= 0;
		rcInTab.bottom -= 0;

		m_slidersCtrl[sel].MoveWindow(rcInTab,TRUE);
		m_slidersCtrl[sel].ShowScrollBar(SB_HORZ,FALSE); // Make sure horizontal scroll bar is not shown.
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}


//////////////////////////////////////////////////////////////////////////
BOOL CFacialSlidersDialog::OnInitDialog()
{
	BOOL bRes = __super::OnInitDialog();

	m_hAccelerators = LoadAccelerators(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDR_FACED_MENU));

	CRect rc;
	GetClientRect(rc);

	//////////////////////////////////////////////////////////////////////////
	VERIFY(m_wndToolBar.CreateToolBar(WS_VISIBLE|WS_CHILD|CBRS_TOOLTIPS|CBRS_ALIGN_TOP, this, AFX_IDW_TOOLBAR));
	VERIFY(m_wndToolBar.LoadToolBar(IDR_FACEIT_SLIDERS_BAR));
	m_wndToolBar.SetFlags(xtpFlagAlignTop|xtpFlagStretched);
	m_wndToolBar.EnableCustomization(FALSE);
	{
		CXTPControl *pCtrl = m_wndToolBar.GetControls()->FindControl(xtpControlButton, ID_FACEIT_SLIDERS_CLEAR_ALL, TRUE, FALSE);
		if (pCtrl)
		{
			pCtrl->SetStyle(xtpButtonIconAndCaption);
			pCtrl->SetCaption( _T("Clear All") );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	m_tabCtrl.Create( TCS_HOTTRACK|TCS_TABS|TCS_FOCUSNEVER|TCS_SINGLELINE|WS_CHILD|WS_VISIBLE,rc,this,IDC_TABCTRL );
	m_tabCtrl.SetFont( CFont::FromHandle( (HFONT)gSettings.gui.hSystemFont) );
	m_tabCtrl.InsertItem( TAB_MORPH_TARGETS,_T("Morph Targets"),TAB_MORPH_TARGETS );
	m_tabCtrl.InsertItem( TAB_ALL_EFFECTORS,_T("All Effectors"),TAB_ALL_EFFECTORS );
	//////////////////////////////////////////////////////////////////////////

	m_slidersCtrl[0].Create( WS_CHILD|WS_VISIBLE,rc,&m_tabCtrl,0 );
	m_slidersCtrl[0].ModifyStyleEx( 0,WS_EX_CLIENTEDGE );

	m_slidersCtrl[1].Create( WS_CHILD,rc,&m_tabCtrl,1 );
	m_slidersCtrl[1].ModifyStyleEx( 0,WS_EX_CLIENTEDGE );
	m_slidersCtrl[1].SetShowExpressions(true);

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersDialog::SetContext( CFacialEdContext *pContext )
{
	assert( pContext );
	m_pContext = pContext;

	m_slidersCtrl[0].SetContext( pContext );
	m_slidersCtrl[1].SetContext( pContext );
	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersDialog::SetMorphWeight(IFacialEffector* pEffector, float fWeight)
{
	m_tabCtrl.SetCurSel(TAB_MORPH_TARGETS);
	m_slidersCtrl[0].SetMorphWeight(pEffector, fWeight);
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersDialog::ClearAllMorphs()
{
	m_tabCtrl.SetCurSel(TAB_MORPH_TARGETS);
	m_slidersCtrl[0].OnClearAll();
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersDialog::OnTabSelect(NMHDR* pNMHDR, LRESULT* pResult)
{
	int sel = m_tabCtrl.GetCurSel();
	switch (sel)
	{
	case TAB_MORPH_TARGETS:
		m_slidersCtrl[0].ShowWindow(SW_SHOW);
		m_slidersCtrl[1].ShowWindow(SW_HIDE);
		RecalcLayout();
		Invalidate();
		break;
	case TAB_ALL_EFFECTORS:
		m_slidersCtrl[0].ShowWindow(SW_HIDE);
		m_slidersCtrl[1].ShowWindow(SW_SHOW);
		RecalcLayout();
		Invalidate();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialSlidersDialog::OnClearAllSliders()
{
	int sel = m_tabCtrl.GetCurSel();
	m_slidersCtrl[sel].OnClearAll();
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialSlidersDialog::PreTranslateMessage(MSG* pMsg)
{
   if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST && m_hAccelerators)
      return ::TranslateAccelerator(m_hWnd, m_hAccelerators, pMsg);
   return CDialog::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
BOOL CFacialSlidersDialog::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CFacialEdContext::MoveToKeyDirection direction = (zDelta < 0 ? CFacialEdContext::MoveToKeyDirectionBackward : CFacialEdContext::MoveToKeyDirectionForward);
	if (m_pContext)
		m_pContext->MoveToFrame(direction);
	return TRUE;
}
