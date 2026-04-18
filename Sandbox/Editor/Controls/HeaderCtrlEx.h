////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   HeaderCtrlEx.h
//  Version:     v1.00
//  Created:     14/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: Extended ListCtrl Header control.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __HeaderCtrlEx_h__
#define __HeaderCtrlEx_h__
#pragma once

// Notification sent to the parent when

#define NM_HEADER_ENDTRACK (NM_FIRST-2001)
#define NM_HEADER_BEGINTRACK (NM_FIRST-2002)

//////////////////////////////////////////////////////////////////////////
// Extended ListCtrl Header control.
//////////////////////////////////////////////////////////////////////////
class CHeaderCtrlEx : public CHeaderCtrl
{
public:
	CHeaderCtrlEx() {};
	~CHeaderCtrlEx() {};

protected:
	DECLARE_MESSAGE_MAP()

	afx_msg void OnBeginTrack( NMHDR *pNMHDR,LRESULT* pResult );
	afx_msg void OnEndTrack( NMHDR *pNMHDR,LRESULT* pResult );
};

#endif // __HeaderCtrlEx_h__
