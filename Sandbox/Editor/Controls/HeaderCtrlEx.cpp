////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   HeaderCtrlEx.cpp
//  Version:     v1.00
//  Created:     14/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "HeaderCtrlEx.h"


BEGIN_MESSAGE_MAP(CHeaderCtrlEx, CHeaderCtrl)
	//ON_NOTIFY_REFLECT(HDN_ENDTRACKW, OnEndTrack)
	//ON_NOTIFY_REFLECT(HDN_ENDTRACKA, OnEndTrack)
	//ON_NOTIFY_REFLECT(HDN_BEGINTRACKW, OnBeginTrack)
	//ON_NOTIFY_REFLECT(HDN_BEGINTRACKA, OnBeginTrack)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CHeaderCtrlEx::OnEndTrack( NMHDR *pNMHDR,LRESULT* pResult )
{
	NMHEADER *pIn = (NMHEADER*)pNMHDR;
	CWnd *pParent = GetParent();
	if (pParent)
	{
		NMHEADER notify = *pIn;
		notify.hdr.code = NM_HEADER_ENDTRACK;
		notify.hdr.hwndFrom = m_hWnd;
		notify.hdr.idFrom = GetDlgCtrlID();
		pParent->SendMessage( WM_NOTIFY,(WPARAM)GetDlgCtrlID(),(LPARAM)(&notify) );
	}
}
//////////////////////////////////////////////////////////////////////////
void CHeaderCtrlEx::OnBeginTrack( NMHDR *pNMHDR,LRESULT* pResult )
{
	NMHEADER *pIn = (NMHEADER*)pNMHDR;
	CWnd *pParent = GetParent();
	if (pParent)
	{
		NMHEADER notify = *pIn;
		notify.hdr.code = NM_HEADER_BEGINTRACK;
		notify.hdr.hwndFrom = m_hWnd;
		notify.hdr.idFrom = GetDlgCtrlID();
		pParent->SendMessage( WM_NOTIFY,(WPARAM)GetDlgCtrlID(),(LPARAM)(&notify) );
	}
}
