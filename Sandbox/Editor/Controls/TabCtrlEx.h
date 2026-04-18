////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   TabCtrlEx.h
//  Version:     v1.00
//  Created:     25/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CTabCtrlEx_h__
#define __CTabCtrlEx_h__

#if _MSC_VER > 1000
#pragma once
#endif

//////////////////////////////////////////////////////////////////////////
class CTabCtrlEx : public CTabCtrl
{
	DECLARE_DYNCREATE(CTabCtrlEx);

public:
	CTabCtrlEx();
	virtual ~CTabCtrlEx();

	int AddPage( const char *sCaption,CWnd *pWnd,bool bAutoDestroy=true,bool bKeepWndHeight=false );
	void RemovePage( int nPageID );
	CWnd* SelectPage( int nPageId );

	CWnd* SelectPageByIndex( int index );

	// Dialog Data
	enum { IDD = IDD_PANEL_PROPERTIES };

protected:

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTabSelect(NMHDR* pNMHDR, LRESULT* pResult);
	void RepositionPages();

	DECLARE_MESSAGE_MAP()

	struct SPage
	{
		int nId;
		CWnd* pWnd;
		bool bAutoDestroy;
		bool bKeepHeight;
		SPage() : pWnd(0),bAutoDestroy(false),bKeepHeight(false) {}
	};
	std::vector<SPage> m_pages;
	int m_selectedCtrl;
	int m_lastID;
};

#endif //__CTabCtrlEx_h__