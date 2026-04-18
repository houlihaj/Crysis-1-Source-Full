////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   ListCtrlEx.h
//  Version:     v1.00
//  Created:     14/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ListCtrlEx_h__
#define __ListCtrlEx_h__
#pragma once

#include "HeaderCtrlEx.h"

//////////////////////////////////////////////////////////////////////////
class CListCtrlEx : public CXTListCtrl
{
	// Construction
public:
	CListCtrlEx();

public:
	void RepositionControls();
	void InsertSomeItems();
	void CreateColumns();
	virtual ~CListCtrlEx();

	CWnd* GetItemControl( int nIndex,int nColumn );
	void  SetItemControl( int nIndex,int nColumn,CWnd *pWnd );
	BOOL 	DeleteAllItems();

	// Generated message map functions
protected:
	virtual BOOL SubclassWindow(HWND hWnd);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnWindowPosChanged(WINDOWPOS FAR* lpwndpos);
	afx_msg LRESULT OnNotifyMsg( WPARAM wParam, LPARAM lParam );
	afx_msg void OnHeaderEndTrack(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );

	//////////////////////////////////////////////////////////////////////////
	typedef std::map<int,CWnd*> ControlsMap;
	ControlsMap m_controlsMap;

	//CHeaderCtrlEx m_headerCtrl;
};


//////////////////////////////////////////////////////////////////////////
class CControlsListBox : public CListBox
{
	// Construction
public:
	CControlsListBox();
	virtual ~CControlsListBox();

public:
	void RepositionControls();

	CWnd* GetItemControl( int nIndex );
	void SetItemControl( int nIndex,CWnd *pWnd );
	void ResetContent();

	// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnWindowPosChanged(WINDOWPOS FAR* lpwndpos);
	afx_msg LRESULT OnNotifyMsg( WPARAM wParam, LPARAM lParam );
	afx_msg void OnHeaderEndTrack(NMHDR* pNMHDR, LRESULT* pResult);

	//////////////////////////////////////////////////////////////////////////
	typedef std::map<int,CWnd*> ControlsMap;
	ControlsMap m_controlsMap;
};

#endif // __ListCtrlEx_h__
