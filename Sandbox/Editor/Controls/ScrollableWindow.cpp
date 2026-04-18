////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   ScrollableWindow.cpp
//  Version:     v1.00
//  Created:     25/10/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ScrollableWindow.h"

CScrollableWindow::CScrollableWindow()
{
	m_ClientSize = CRect(0, 0, 300, 300);
	m_bHVisible = false;
	m_bVVisible = false;
	showing = false;
}

CScrollableWindow::~CScrollableWindow()
{
}

BEGIN_MESSAGE_MAP(CScrollableWindow, CWnd)
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_SIZE()
END_MESSAGE_MAP()

void CScrollableWindow::OnClientSizeUpdated()
{
	UpdateScrollBars();
}

void CScrollableWindow::UpdateScrollBars()
{
	showing = false;

	if (showing)
		return;

	int cx;
	int cy;
	{
		CRect clientArea;
		GetClientRect(clientArea);
		cx = clientArea.Width();
		cy = clientArea.Height();
	}

	if (m_bHVisible)
		cy += GetSystemMetrics(SM_CYHSCROLL);
	if (m_bVVisible)
		cx += GetSystemMetrics(SM_CXVSCROLL);

	bool	newHVis = false ;
	bool	newVVis = false ;

	if (cx < m_ClientSize.Width())
		newHVis = true ;
	if (cy < m_ClientSize.Height())
		newVVis = true ;
	if (newVVis && !newHVis && cx - GetSystemMetrics(SM_CXVSCROLL) < m_ClientSize.Width())
		newHVis = true ;
	if (newHVis && !newVVis && cy - GetSystemMetrics(SM_CYHSCROLL) < m_ClientSize.Height())
		newVVis = true ;

	if (m_bHVisible)
		cy -= GetSystemMetrics(SM_CYHSCROLL);
	if (m_bVVisible)
		cx -= GetSystemMetrics(SM_CXVSCROLL);

	if (newHVis && !m_bHVisible)
		cx -= GetSystemMetrics(SM_CXVSCROLL);
	if (newVVis && !m_bVVisible)
		cy -= GetSystemMetrics(SM_CYHSCROLL);

	showing = true;
	ShowScrollBar(SB_HORZ, newHVis);
	SCROLLINFO	si;

	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_PAGE | SIF_RANGE;
	si.nPage = cx;
	si.nMax = m_ClientSize.Width();
	si.nMin = 0;
	SetScrollInfo(SB_HORZ, &si);
	EnableScrollBarCtrl(SB_HORZ, newHVis);

	ShowScrollBar(SB_VERT, newVVis);
	si.nPage = cy;
	si.nMax = m_ClientSize.Height();
	si.nMin = 0;
	SetScrollInfo(SB_VERT, &si);
	EnableScrollBarCtrl(SB_VERT, newVVis);
	showing = false;

	m_bHVisible = newHVis;
	m_bVVisible = newVVis;
}

void CScrollableWindow::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	int nCurPos = GetScrollPos(SB_HORZ);
	int nPrevPos = nCurPos;

	switch (nSBCode)
	{
	case SB_LEFT:
		nCurPos = 0;
		break;
	case SB_RIGHT:
		nCurPos = GetScrollLimit(SB_HORZ) - 1;
		break;
	case SB_LINELEFT:
		nCurPos = max(nCurPos - 6, 0);
		break;
	case SB_LINERIGHT:
		nCurPos = min(nCurPos + 6, GetScrollLimit(SB_HORZ) - 1);
		break;
	case SB_PAGELEFT:
		nCurPos = max(nCurPos - m_ClientSize.Width(), 0);
		break;
	case SB_PAGERIGHT:
		nCurPos = min(nCurPos + m_ClientSize.Width(), GetScrollLimit(SB_HORZ) - 1);
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		nCurPos = nPos ;
		break;
	}		

	SetScrollPos(SB_HORZ, nCurPos);
	ScrollWindow(nPrevPos - nCurPos, 0);
	CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CScrollableWindow::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	int nCurPos = GetScrollPos(SB_VERT);
	int nPrevPos = nCurPos;

	switch(nSBCode)
	{
	case SB_LEFT:
		nCurPos = 0;
		break;
	case SB_RIGHT:
		nCurPos = GetScrollLimit(SB_VERT)-1;
		break;
	case SB_LINELEFT:
		nCurPos = max(nCurPos - 6, 0);
		break;
	case SB_LINERIGHT:
		nCurPos = min(nCurPos + 6, GetScrollLimit(SB_VERT)-1);
		break;
	case SB_PAGELEFT:
		nCurPos = max(nCurPos - m_ClientSize.Height(), 0);
		break;
	case SB_PAGERIGHT:
		nCurPos = min(nCurPos + m_ClientSize.Height(), GetScrollLimit(SB_VERT)-1);
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		nCurPos = nPos;
		break;
	}		

	SetScrollPos(SB_VERT, nCurPos);
	ScrollWindow(0, nPrevPos - nCurPos);
	CWnd::OnVScroll(nSBCode, nPos, pScrollBar);
}


void CScrollableWindow::OnSize(UINT nType, int cx, int cy) 
{
	UpdateScrollBars();

	CWnd::OnSize(nType, cx, cy);
}
