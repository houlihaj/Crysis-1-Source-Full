////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   TabCtrlEx.cpp
//  Version:     v1.00
//  Created:     25/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TabCtrlEx.h"

IMPLEMENT_DYNCREATE(CTabCtrlEx,CTabCtrl)

BEGIN_MESSAGE_MAP(CTabCtrlEx, CTabCtrl)
	ON_WM_SIZE()
	ON_NOTIFY_REFLECT( TCN_SELCHANGE, OnTabSelect )
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CTabCtrlEx::CTabCtrlEx()
{
	m_selectedCtrl = -1;
	m_lastID = 0;
}

//////////////////////////////////////////////////////////////////////////
CTabCtrlEx::~CTabCtrlEx()
{
	for (int i = 0; i < (int)m_pages.size(); i++)
	{
		if (m_pages[i].bAutoDestroy)
		{
			delete m_pages[i].pWnd;
		}
		else if (m_pages[i].pWnd)
		{
			m_pages[i].pWnd->ShowWindow(SW_HIDE);
			m_pages[i].pWnd->SetParent(NULL);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTabCtrlEx::OnSize(UINT nType, int cx, int cy) 
{
	__super::OnSize(nType, cx, cy);

	RepositionPages();
}

//////////////////////////////////////////////////////////////////////////
void CTabCtrlEx::RepositionPages()
{
	int nItems = GetItemCount();
	int i;
	int bottom = 0;
	for (i = 0; i < nItems; i++)
	{
		CRect rc;
		GetItemRect( i,rc );
		if (rc.bottom > bottom)
			bottom = rc.bottom;
	}
	nItems = min(nItems,(int)m_pages.size());
	CRect rc;
	for (i = 0; i < nItems; i++)
	{
		CRect irc;
		GetItemRect( i,irc );
		GetClientRect( rc );
		if (m_pages[i].pWnd)
		{
			rc.left += 1;
			rc.right -= 2;
			rc.top = bottom+2;
			rc.bottom -= 2;
			if (m_pages[i].bKeepHeight)
			{
				CRect wndrc;
				m_pages[i].pWnd->GetWindowRect(wndrc);
				rc.bottom = min(rc.bottom,rc.top+wndrc.Height());
			}
			m_pages[i].pWnd->MoveWindow( rc,TRUE );
		}
	}
	Invalidate(TRUE);
}

//////////////////////////////////////////////////////////////////////////
int CTabCtrlEx::AddPage( const char *sCaption,CWnd *pWnd,bool bAutoDestroy,bool bKeepWndHeight )
{
	int nPageID = ++m_lastID;

	TCITEM item;
	ZeroStruct(item);
	item.mask = TCIF_TEXT|TCIF_PARAM;
	item.lParam = (UINT_PTR)nPageID;
	item.pszText = const_cast<char*>(sCaption);
	int nItem = InsertItem( GetItemCount(),&item );

	if (nItem >= (int)m_pages.size())
	{
		m_pages.resize( nItem+1 );
	}

	SPage page;
	page.bAutoDestroy = bAutoDestroy;
	page.bKeepHeight = bKeepWndHeight;
	page.pWnd = pWnd;
	page.nId = nPageID;

	pWnd->ShowWindow( SW_HIDE );
	pWnd->SetParent( this );
	m_pages[nItem] = page;

	RepositionPages();

	if (nItem == 0)
	{
		SelectPageByIndex(0);
	}

	return nPageID;
}

//////////////////////////////////////////////////////////////////////////
void CTabCtrlEx::RemovePage( int nPageID )
{
	for (int i = 0; i < (int)m_pages.size(); i++)
	{
		if (nPageID == m_pages[i].nId)
		{
			if (m_pages[i].bAutoDestroy)
			{
				delete m_pages[i].pWnd;
			}
			else if (m_pages[i].pWnd)
			{
				m_pages[i].pWnd->ShowWindow(SW_HIDE);
				m_pages[i].pWnd->SetParent(NULL);
			}
			m_pages.erase(m_pages.begin()+i);
			DeleteItem(i);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CWnd* CTabCtrlEx::SelectPageByIndex( int num )
{
	if (GetCurSel() != num)
	{
		SetCurSel(num);
	}
	CWnd *pSelected = 0;
	m_selectedCtrl = num;
	for (int i = 0; i < (int)m_pages.size(); i++)
	{
		if (!m_pages[i].pWnd)
			continue;
		if (i == num)
		{
			pSelected = m_pages[i].pWnd;
			m_pages[i].pWnd->ShowWindow( SW_SHOW );
		}
		else
		{
			m_pages[i].pWnd->ShowWindow( SW_HIDE );
		}
	}
	return pSelected;
}

//////////////////////////////////////////////////////////////////////////
CWnd* CTabCtrlEx::SelectPage( int nPageId )
{
	for (int i = 0; i < (int)m_pages.size(); i++)
	{
		if (m_pages[i].nId == nPageId)
		{
			return SelectPageByIndex(i);
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CTabCtrlEx::OnTabSelect(NMHDR* pNMHDR, LRESULT* pResult)
{
	int sel = GetCurSel();
	SelectPageByIndex( sel );
	if (GetParent())	
	{
		pNMHDR->code = (TCN_SELCHANGE-10);
		GetParent()->SendMessage( WM_NOTIFY,(WPARAM)GetDlgCtrlID(),(LPARAM)(pNMHDR) );
	}
}
