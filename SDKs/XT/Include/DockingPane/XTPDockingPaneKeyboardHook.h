// XTPDockingPaneKeyboardHook.h : interface for the CXTPDockingPaneKeyboardHook class.
//
// This file is a part of the XTREME DOCKINGPANE MFC class library.
// (c)1998-2007 Codejock Software, All Rights Reserved.
//
// THIS SOURCE FILE IS THE PROPERTY OF CODEJOCK SOFTWARE AND IS NOT TO BE
// RE-DISTRIBUTED BY ANY MEANS WHATSOEVER WITHOUT THE EXPRESSED WRITTEN
// CONSENT OF CODEJOCK SOFTWARE.
//
// THIS SOURCE CODE CAN ONLY BE USED UNDER THE TERMS AND CONDITIONS OUTLINED
// IN THE XTREME TOOLKIT PRO LICENSE AGREEMENT. CODEJOCK SOFTWARE GRANTS TO
// YOU (ONE SOFTWARE DEVELOPER) THE LIMITED RIGHT TO USE THIS SOFTWARE ON A
// SINGLE COMPUTER.
//
// CONTACT INFORMATION:
// support@codejock.com
// http://www.codejock.com
//
/////////////////////////////////////////////////////////////////////////////

//{{AFX_CODEJOCK_PRIVATE
#if !defined(__XTPDOCKINGPANEKEYBOARDHOOK_H__)
#define __XTPDOCKINGPANEKEYBOARDHOOK_H__
//}}AFX_CODEJOCK_PRIVATE

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

//{{AFX_CODEJOCK_PRIVATE

#include "XTPDockingPaneManager.h"

class CXTPDockingPaneWindowSelect;

class _XTP_EXT_CLASS CXTPDockingPaneKeyboardHook : public CNoTrackObject
{
public:
	CXTPDockingPaneKeyboardHook();
	~CXTPDockingPaneKeyboardHook();

public:
	void SetupKeyboardHook(CXTPDockingPaneManager* pManager, BOOL bSetup);
	CXTPDockingPaneManager* FindFocusedManager();

	static CXTPDockingPaneKeyboardHook* AFX_CDECL GetThreadState();

protected:
	static LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);

	CXTPDockingPaneManager* Lookup(HWND hSite) const;

protected:
	HHOOK m_hHookKeyboard;          // Keyboard hook

	static CThreadLocal<CXTPDockingPaneKeyboardHook> _xtpKeyboardThreadState;           // Instance of Keyboard hook

	CMap<HWND, HWND, CXTPDockingPaneManager*, CXTPDockingPaneManager*> m_mapSites;
#ifdef _AFXDLL
	AFX_MODULE_STATE* m_pModuleState; // Module state
#endif

	CXTPDockingPaneWindowSelect* m_pWindowSelect;
};


AFX_INLINE CXTPDockingPaneKeyboardHook* AFX_CDECL CXTPDockingPaneKeyboardHook::GetThreadState() {
	return _xtpKeyboardThreadState.GetData();
}



//}}AFX_CODEJOCK_PRIVATE

#endif //#if !defined(__XTPDOCKINGPANEKEYBOARDHOOK_H__)
