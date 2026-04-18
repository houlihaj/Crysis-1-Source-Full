////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   IViewPane.h
//  Version:     v1.00
//  Created:     18/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __IViewPane_h__
#define __IViewPane_h__
#pragma once

struct __declspec( uuid("{7E13EC7C-F621-4aeb-B642-67D78ED468F8}") ) IViewPaneClass : public IClassDesc
{
	enum EDockingDirection
	{
		DOCK_TOP,
		DOCK_LEFT,
		DOCK_RIGHT,
		DOCK_BOTTOM,
		DOCK_FLOAT,
	};
	// Get view pane window runtime class.
	virtual CRuntimeClass* GetRuntimeClass() = 0;

	// Return text for view pane title.
	virtual const char* GetPaneTitle() = 0;

	// Return preferable initial docking position for pane.
	virtual EDockingDirection GetDockingDirection() = 0;

	// Initial pane size.
	virtual CRect GetPaneRect() = 0;
	
	// Get Minimal view size
	virtual CSize GetMinSize() { return CSize(0,0); }

	// Return true if only one pane at a time of time view class can be created.
	virtual bool SinglePane() = 0;

	// Return true if the view window wants to get ID_IDLE_UPDATE commands.
	virtual bool WantIdleUpdate() = 0;

	//////////////////////////////////////////////////////////////////////////
	// IUnknown
	//////////////////////////////////////////////////////////////////////////
	HRESULT STDMETHODCALLTYPE QueryInterface( const IID &riid, void **ppvObj )
	{
		if (riid == __uuidof(IViewPaneClass))
		{
			*ppvObj = this;
			return S_OK;
		}
		return E_NOINTERFACE ;
	}
	//////////////////////////////////////////////////////////////////////////
};

#endif //__IViewPane_h__
