////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CImageListCtrl.cpp
//  Version:     v1.00
//  Created:     9/10/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ImageListCtrl.h"
#include "MemDC.h"

//////////////////////////////////////////////////////////////////////////
CImageListCtrlItem::CImageListCtrlItem()
{
	pUserData = 0;
	pUserData2 = 0;
	pImage = 0;
	bBitmapValid = false;
	bSelected = false;
}

//////////////////////////////////////////////////////////////////////////
CImageListCtrlItem::~CImageListCtrlItem()
{
	if (pImage)
		delete pImage;
}

//////////////////////////////////////////////////////////////////////////
CImageListCtrl::CImageListCtrl()
{
	m_scrollOffset = CPoint(0,0);
	m_itemSize = CSize(60,60);
	m_borderSize = CSize(4,4);

	m_bkgrBrush.CreateSysColorBrush( COLOR_WINDOW );
	m_selectedPen.CreatePen( PS_SOLID,1,RGB(255,255,0) );
}

//////////////////////////////////////////////////////////////////////////
CImageListCtrl::~CImageListCtrl()
{
}

BEGIN_MESSAGE_MAP(CImageListCtrl, CWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_PAINT()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CImageListCtrl::Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd,UINT nID )
{
	LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW,
		AfxGetApp()->LoadStandardCursor(IDC_ARROW), 
		(HBRUSH)::GetStockObject(LTGRAY_BRUSH), NULL);
	BOOL bRes = CreateEx( 0,lpClassName,NULL,dwStyle,rect,pParentWnd,nID );
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
int CImageListCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	InitializeFlatSB( GetSafeHwnd() );
	FlatSB_SetScrollProp( GetSafeHwnd(),WSB_PROP_CXVSCROLL,8,FALSE );
	FlatSB_SetScrollProp( GetSafeHwnd(),WSB_PROP_CYVSCROLL,8,FALSE );
	FlatSB_SetScrollProp( GetSafeHwnd(),WSB_PROP_VSTYLE,FSB_ENCARTA_MODE,FALSE );
	FlatSB_EnableScrollBar( GetSafeHwnd(),SB_BOTH,ESB_ENABLE_BOTH );

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnDestroy()
{
	m_items.clear();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnSize(UINT nType, int cx, int cy) 
{
	__super::OnSize(nType, cx, cy);

	if (m_offscreenBitmap.GetSafeHandle() != NULL)
		m_offscreenBitmap.DeleteObject();

	CRect rc;
	GetClientRect( rc );

	CDC *dc = GetDC();
	m_offscreenBitmap.CreateCompatibleBitmap( dc,rc.Width(),rc.Height() );
	ReleaseDC(dc);

	if (m_hWnd)
	{
		CalcLayout();
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CImageListCtrl::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnPaint()
{
	CPaintDC  PaintDC(this);

	CRect rcClient;
	GetClientRect( rcClient );
	if (m_offscreenBitmap.GetSafeHandle() == NULL)
	{
		m_offscreenBitmap.CreateCompatibleBitmap( &PaintDC,rcClient.Width(),rcClient.Height() );
	}
	CMemDC dc( PaintDC,&m_offscreenBitmap );

	dc->FillRect( &PaintDC.m_ps.rcPaint,&m_bkgrBrush );

	DrawControl( dc,PaintDC.m_ps.rcPaint );
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::DrawControl( CDC *pDC,const CRect &rcUpdate )
{
	// Get items in update rect.
	Items items;
	GetItemsInRect( rcUpdate,items );
	for (int i = 0; i < items.size(); i++)
	{
		DrawItem( pDC,rcUpdate,items[i] );
	}
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::DrawItem( CDC *pDC,const CRect &rcUpdate,CImageListCtrlItem *pItem )
{
	if (!pItem->bBitmapValid)
	{
		// Make a bitmap from image.
		OnUpdateItem( pItem );
	}

	CDC bmpDC;
	VERIFY(bmpDC.CreateCompatibleDC(pDC));

	CBitmap *pOldBitmap = bmpDC.SelectObject( &pItem->bitmap );
	pDC->BitBlt( pItem->rect.left,pItem->rect.top,pItem->rect.Width(),pItem->rect.Height(),&bmpDC,0,0,SRCCOPY);
	bmpDC.SelectObject(pOldBitmap);
	
	if (pItem->bSelected)
	{
		int x1 = pItem->rect.left;
		int y1 = pItem->rect.top;
		int x2 = pItem->rect.right;
		int y2 = pItem->rect.bottom;
		CPen *pOldPen = pDC->SelectObject(&m_selectedPen);
		
		// Draw selection border.
		for (int ofs = 1; ofs < 3; ofs++)
		{
			pDC->MoveTo(x1-ofs,y1-ofs);
			pDC->LineTo(x2+ofs,y1-ofs);
			pDC->LineTo(x2+ofs,y2+ofs);
			pDC->LineTo(x1-ofs,y2+ofs);
			pDC->LineTo(x1-ofs,y1-ofs);
		}

		pDC->SelectObject(pOldPen);
	}

	bmpDC.DeleteDC();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::CalcLayout( bool bUpdateScrollBar )
{
	static bool bNoRecurse = false;
	if (bNoRecurse)
		return;
	bNoRecurse = true;

	CRect rc;
	GetClientRect(rc);


	int nStyle = GetStyle();
	
	if (nStyle & ILC_STYLE_HORZ)
	{
		if (bUpdateScrollBar)
		{
			// Set scroll info.
			int nPage = rc.Width();
			int w = m_items.size()*(m_itemSize.cx+m_borderSize.cx) + m_borderSize.cx;

			SCROLLINFO si;
			ZeroStruct(si);
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;
			si.nMin = 0;
			si.nMax = w;
			si.nPage = nPage;
			si.nPos = m_scrollOffset.x;
			FlatSB_SetScrollInfo( GetSafeHwnd(),SB_HORZ,&si,TRUE );
			FlatSB_GetScrollInfo( GetSafeHwnd(),SB_HORZ,&si );
			m_scrollOffset.x = si.nPos;
		}

		// Horizontal Layout.
		int x = m_borderSize.cx - m_scrollOffset.x;
		int y = m_borderSize.cy;
		for (int i = 0; i < m_items.size(); i++)
		{
			CImageListCtrlItem *pItem = m_items[i];
			pItem->rect.left = x;
			pItem->rect.top = y;
			pItem->rect.right = pItem->rect.left + m_itemSize.cx;
			pItem->rect.bottom = pItem->rect.top + m_itemSize.cy;
			x += m_itemSize.cx + m_borderSize.cx;
		}
	}
	else if (nStyle & ILC_STYLE_VERT)
	{
		// Vertical Layout
	}
	else
	{
		// Normal Layout.
	}

	bNoRecurse = false;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::GetAllItems( Items &items ) const
{
	items = m_items;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::GetItemsInRect( const CRect &rect,Items &items ) const
{
	items.clear();
	CRect rcTemp;
	// Get all items inside specified rectangle.
	for (int i = 0; i < m_items.size(); i++)
	{
		CImageListCtrlItem *pItem = m_items[i];
		rcTemp.IntersectRect( pItem->rect,rect );
		if (!rcTemp.IsRectEmpty())
			items.push_back(pItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::SetItemSize( CSize size )
{
	assert( size.cx != 0 && size.cy != 0 );
	m_itemSize = size;
	CalcLayout();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::SetBorderSize( CSize size )
{
	m_borderSize = size;
	CalcLayout();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::InsertItem( CImageListCtrlItem *pItem )
{
	m_items.push_back(pItem);
	CalcLayout();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::DeleteAllItems()
{
	m_items.clear();
	CalcLayout();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnHScroll( UINT nSBCode,UINT nPos,CScrollBar* pScrollBar )
{
	SCROLLINFO si;
	ZeroStruct(si);
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	FlatSB_GetScrollInfo( GetSafeHwnd(),SB_HORZ,&si );

	// Get the minimum and maximum scroll-bar positions.
	int minpos = si.nMin;
	int maxpos = si.nMax;
	int nPage = si.nPage;

	// Get the current position of scroll box.
	int curpos = si.nPos;

	// Determine the new position of scroll box.
	switch (nSBCode)
	{
	case SB_LEFT:      // Scroll to far left.
		curpos = minpos;
		break;

	case SB_RIGHT:      // Scroll to far right.
		curpos = maxpos;
		break;

	case SB_ENDSCROLL:   // End scroll.
		break;

	case SB_LINELEFT:      // Scroll left.
		if (curpos > minpos)
			curpos--;
		break;

	case SB_LINERIGHT:   // Scroll right.
		if (curpos < maxpos)
			curpos++;
		break;

	case SB_PAGELEFT:    // Scroll one page left.
		if (curpos > minpos)
			curpos = max(minpos, curpos - (int)nPage);
		break;

	case SB_PAGERIGHT:      // Scroll one page right.
		if (curpos < maxpos)
			curpos = min(maxpos, curpos + (int)nPage);
		break;

	case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
		curpos = nPos;      // of the scroll box at the end of the drag operation.
		break;

	case SB_THUMBTRACK:   // Drag scroll box to specified position. nPos is the
		curpos = nPos;     // position that the scroll box has been dragged to.
		break;
	}

	// Set the new position of the thumb (scroll box).
	FlatSB_SetScrollPos( GetSafeHwnd(),SB_HORZ,curpos,TRUE );

	m_scrollOffset.x = curpos;
	CalcLayout(false);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO si;
	ZeroStruct(si);
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	FlatSB_GetScrollInfo( GetSafeHwnd(),SB_VERT,&si );

	// Get the minimum and maximum scroll-bar positions.
	int minpos = si.nMin;
	int maxpos = si.nMax;
	int nPage = si.nPage;

	// Get the current position of scroll box.
	int curpos = si.nPos;

	// Determine the new position of scroll box.
	switch (nSBCode)
	{
	case SB_LEFT:      // Scroll to far left.
		curpos = minpos;
		break;

	case SB_RIGHT:      // Scroll to far right.
		curpos = maxpos;
		break;

	case SB_ENDSCROLL:   // End scroll.
		break;

	case SB_LINELEFT:      // Scroll left.
		if (curpos > minpos)
			curpos--;
		break;

	case SB_LINERIGHT:   // Scroll right.
		if (curpos < maxpos)
			curpos++;
		break;

	case SB_PAGELEFT:    // Scroll one page left.
		if (curpos > minpos)
			curpos = max(minpos, curpos - (int)nPage);
		break;

	case SB_PAGERIGHT:      // Scroll one page right.
		if (curpos < maxpos)
			curpos = min(maxpos, curpos + (int)nPage);
		break;

	case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
		curpos = nPos;      // of the scroll box at the end of the drag operation.
		break;

	case SB_THUMBTRACK:   // Drag scroll box to specified position. nPos is the
		curpos = nPos;     // position that the scroll box has been dragged to.
		break;
	}

	// Set the new position of the thumb (scroll box).
	FlatSB_SetScrollPos( GetSafeHwnd(),SB_VERT,curpos,TRUE );

	m_scrollOffset.y = curpos;
	CalcLayout(false);
	Invalidate();

	//CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

//////////////////////////////////////////////////////////////////////////
CImageListCtrlItem* CImageListCtrl::HitTest( CPoint point )
{
	CRect rc;
	GetClientRect(rc);
	if (!rc.PtInRect(point))
		return 0;
	for (int i = 0; i < m_items.size(); i++)
	{
		CImageListCtrlItem *pItem = m_items[i];
		if (pItem->rect.PtInRect(point) == TRUE)
			return pItem;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	CImageListCtrlItem *pItem = HitTest(point);
	if (pItem)
	{
		OnSelectItem(pItem,true);
	}
	__super::OnLButtonDown( nFlags,point );
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	__super::OnLButtonUp( nFlags,point );
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
	CImageListCtrlItem *pItem = HitTest(point);
	if (pItem)
	{
		OnSelectItem(pItem,true);
	}
	__super::OnRButtonDown( nFlags,point );
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnRButtonUp(UINT nFlags, CPoint point)
{
	__super::OnRButtonUp( nFlags,point );
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnSelectItem( CImageListCtrlItem *pItem,bool bSelected )
{
	SelectItem(pItem,bSelected);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::SelectItem( CImageListCtrlItem *pItem,bool bSelect )
{
	assert(pItem);
	if (pItem->bSelected == bSelect)
		return;

	int nStyle = GetStyle();
  if (!(nStyle & ILC_MULTI_SELECTION))
	{
		ResetSelection();
	}
	pItem->bSelected = bSelect;
	InvalidateItemRect(pItem);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::SelectItem( int nIndex,bool bSelect )
{
	int nStyle = GetStyle();
	if (!(nStyle & ILC_MULTI_SELECTION))
	{
		ResetSelection();
	}
	if (nIndex >= 0 && nIndex < m_items.size())
	{
		ResetSelection();
		SelectItem( m_items[nIndex],bSelect );
	}
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::ResetSelection()
{
	for (int i = 0; i < m_items.size(); i++)
	{
		CImageListCtrlItem *pItem = m_items[i];
		if (pItem->bSelected)
			InvalidateItemRect(pItem);
		pItem->bSelected = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::InvalidateItemRect( CImageListCtrlItem *pItem )
{
	CRect rc = pItem->rect;
	rc.left -= 2;
	rc.top -= 2;
	rc.right += 3;
	rc.bottom += 3;
	InvalidateRect(rc,FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::OnUpdateItem( CImageListCtrlItem *pItem )
{
	// Make a bitmap from image.
	assert(pItem->pImage);

	unsigned int *pImageData = pItem->pImage->GetData();
	int w = pItem->pImage->GetWidth();
	int h = pItem->pImage->GetHeight();

	if (pItem->bitmap.GetSafeHandle() != NULL)
		pItem->bitmap.DeleteObject();
	VERIFY(pItem->bitmap.CreateBitmap(w,h,1,32, pImageData));
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::InvalidateAllItems()
{
	for (int i = 0; i < m_items.size(); i++)
	{
		m_items[i]->bBitmapValid = false;
	}
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CImageListCtrl::InvalidateAllBitmaps()
{
	for (int i = 0; i < m_items.size(); i++)
	{
		m_items[i]->bBitmapValid = false;
	}
}
