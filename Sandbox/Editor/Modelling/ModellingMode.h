////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 2007.
// -------------------------------------------------------------------------
//  File name:   ModellingModeTool.h
//  Version:     v1.00
//  Created:     18/01/2007 by Timur.
//  Description: Modelling mode edit tool allows editing of geometry.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ModellingMode_h__
#define __ModellingMode_h__
#pragma once

// {6113E29E-613D-414c-9FB9-581EA46D1F2C}
DEFINE_GUID( MODELLING_MODE_GUID, 0x6113e29e, 0x613d, 0x414c, 0x9f, 0xb9, 0x58, 0x1e, 0xa4, 0x6d, 0x1f, 0x2c);

#include "EditTool.h"
#include "Objects\Gizmo.h"
#include "Objects\SubObjSelection.h"

class CBaseObject;

/*!
*	CVertexMode is an abstract base class for All Editing	Tools supported by Editor.
*	Edit tools handle specific editing modes in viewports.
*/
class CModellingModeTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CModellingModeTool);

	CModellingModeTool();

	// Registration function.
	static void RegisterTool( CRegistrationContext &rc );

	//////////////////////////////////////////////////////////////////////////
	// CEditTool implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams();
	virtual void Display( struct DisplayContext &dc );

	virtual bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnSetCursor( CViewport *vp ) { return false; };

	virtual void OnManipulatorDrag( CViewport *view,ITransformManipulator *pManipulator,CPoint &p0,CPoint &p1,const Vec3 &value );

	bool IsNeedMoveTool() { return true; };

protected:
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

	virtual ~CModellingModeTool();
	bool OnLButtonDown( CViewport *view,int nFlags, CPoint point );
	bool OnLButtonDblClk( CViewport *view,int nFlags, CPoint point);
	bool OnLButtonUp( CViewport *view,int nFlags, CPoint point );
	bool OnMouseMove( CViewport *view,int nFlags, CPoint point );
	bool CheckVirtualKey( int virtualKey );
	void SetCommandMode( ECommandMode mode ) { m_commandMode = mode; }
	ECommandMode GetCommandMode() const { return m_commandMode; }

	//! Ctrl-Click in move mode to move selected objects to givven pos.
	void MoveSelectionToPos( CViewport *view,Vec3 &pos );
	void SetObjectCursor( CViewport *view,CBaseObject *hitObj,bool bChangeNow=false );

	virtual void DeleteThis() { delete this; };

	bool HitTest( CViewport *view,CPoint point,CRect rc,int nFlags );
	void SetSelectType( ESubObjElementType type );

protected:
	static void Command_ModellingMode();

	void UpdateSelectionGizmo();
	bool GetSelectionReferenceFrame( Matrix34 &refFrame );

	//////////////////////////////////////////////////////////////////////////
	CPoint m_cMouseDownPos;
	ECommandMode m_commandMode;

	CBaseObject* m_pMouseOverObject;
	_smart_ptr<CBaseObject> m_pHighlightedObject;

	// Selected objects.
	typedef std::vector<_smart_ptr<CBaseObject> > SelectedObjectList;
	SelectedObjectList m_selectedObjects;
	static ESubObjElementType m_currSelectionType;

	std::vector<Vec3> m_xformedVertces;

	int m_selectionTypePanelId;
	int m_selectionPanelId;
	int m_displayPanelId;

	static CModellingModeTool *m_pModellingModeTool;
};

#endif //__ModellingMode_h__
