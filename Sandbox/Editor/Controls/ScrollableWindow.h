////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   ScrollableWindow.h
//  Version:     v1.00
//  Created:     25/10/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __SCROLLABLEWINDOW_H__
#define __SCROLLABLEWINDOW_H__

#pragma once

class CScrollableWindow : public CWnd
{
public:
	CScrollableWindow();
	virtual ~CScrollableWindow();

protected:
	void OnClientSizeUpdated();
	void UpdateScrollBars();

	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()

	CRect m_ClientSize;
	bool m_bHVisible;
	bool m_bVVisible;
	bool showing;
};

#endif // __SCROLLABLEWINDOW_H__
