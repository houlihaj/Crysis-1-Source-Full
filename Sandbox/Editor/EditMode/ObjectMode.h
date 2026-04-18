////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ObjectMode.h
//  Version:     v1.00
//  Created:     18/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Object edit mode describe viewport input behavior when operating on objects.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ObjectMode_h__
#define __ObjectMode_h__
#pragma once

// {87109FED-BDB5-4874-936D-338400079F58}
DEFINE_GUID( OBJECT_MODE_GUID, 0x87109fed, 0xbdb5, 0x4874, 0x93, 0x6d, 0x33, 0x84, 0x0, 0x7, 0x9f, 0x58);


#include "EditTool.h"

class CBaseObject;
/*!
*	CObjectMode is an abstract base class for All Editing	Tools supported by Editor.
*	Edit tools handle specific editing modes in viewports.
*/
class CObjectMode : public CEditTool
{
public:
	DECLARE_DYNCREATE(CObjectMode);

	CObjectMode();
	virtual ~CObjectMode() {};

	// Registration function.
	static void RegisterTool( CRegistrationContext &rc );

	//////////////////////////////////////////////////////////////////////////
	// CEditTool implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void BeginEditParams( IEditor *ie,int flags ) {};
	virtual void EndEditParams() {};
	virtual void Display( struct DisplayContext &dc );
	
	virtual bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnSetCursor( CViewport *vp ) { return false; };

protected:
	//////////////////////////////////////////////////////////////////////////
	// IEditorNotifyListener
	//////////////////////////////////////////////////////////////////////////
	void OnEditorNotifyEvent( EEditorNotifyEvent event );
	//////////////////////////////////////////////////////////////////////////

	enum ECommandMode
	{
		NothingMode = 0,
		ScrollZoomMode,
		SelectMode,
		MoveMode,
		RotateMode,
		ScaleMode,
		ScrollMode,
		ZoomMode,
	};

	bool OnLButtonDown( CViewport *view,int nFlags, CPoint point );
	bool OnLButtonDblClk( CViewport *view,int nFlags, CPoint point);
	bool OnLButtonUp( CViewport *view,int nFlags, CPoint point );
	bool OnRButtonDown( CViewport *view,int nFlags, CPoint point );
	bool OnRButtonUp( CViewport *view,int nFlags, CPoint point );
	bool OnMButtonDown( CViewport *view,int nFlags, CPoint point );
	bool OnMouseMove( CViewport *view,int nFlags, CPoint point );
	bool CheckVirtualKey( int virtualKey );
	void SetCommandMode( ECommandMode mode ) { m_commandMode = mode; }
	ECommandMode GetCommandMode() const { return m_commandMode; }

	//! Ctrl-Click in move mode to move selected objects to givven pos.
	void MoveSelectionToPos( CViewport *view,Vec3 &pos );
	void SetObjectCursor( CViewport *view,CBaseObject *hitObj,bool bChangeNow=false );

	virtual void DeleteThis() { delete this; };

	void UpdateStatusText();
	void AwakeObjectAtPoint( CViewport *view,CPoint point );

private:
	CPoint m_cMouseDownPos;
	ECommandMode m_commandMode;

	GUID m_MouseOverObject;
	bool m_openContext;
};



#endif //__ObjectMode_h__
