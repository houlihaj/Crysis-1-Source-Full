////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   VertexMode.h
//  Version:     v1.00
//  Created:     18/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Object edit mode describe viewport input behavior when operating on objects.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __VertexMode_h__
#define __VertexMode_h__
#pragma once

// {58DFF2CC-9295-427f-A208-2BA74FC1B919}
DEFINE_GUID( SUBOBJ_MODE_GUID,0x58dff2cc, 0x9295, 0x427f, 0xa2, 0x8, 0x2b, 0xa7, 0x4f, 0xc1, 0xb9, 0x19);
/*
// {7503BF30-D830-4219-9B8F-37660F8F189B}
DEFINE_GUID( EDGE_MODE_GUID,0x7503bf30, 0xd830, 0x4219, 0x9b, 0x8f, 0x37, 0x66, 0xf, 0x8f, 0x18, 0x9b);
// {2CEE9F70-7059-4ca6-A50F-74298E3956C4}
DEFINE_GUID( FACE_MODE_GUID,0x2cee9f70, 0x7059, 0x4ca6, 0xa5, 0xf, 0x74, 0x29, 0x8e, 0x39, 0x56, 0xc4);
// {73CE8A49-2A81-47b9-8394-71D7D2CCD080}
DEFINE_GUID( POLYGON_MODE_GUID,0x73ce8a49, 0x2a81, 0x47b9, 0x83, 0x94, 0x71, 0xd7, 0xd2, 0xcc, 0xd0, 0x80);
*/

#include "EditTool.h"
#include "Objects\Gizmo.h"
#include "Objects\SubObjSelection.h"

class CBaseObject;
class CSubObjectSelection;
struct SSubObjectHitInfo;
class CSubObjSelectionTypePanel;

/*!
*	CVertexMode is an abstract base class for All Editing	Tools supported by Editor.
*	Edit tools handle specific editing modes in viewports.
*/
class CSubObjectModeTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CSubObjectModeTool);

	CSubObjectModeTool();

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

	virtual ~CSubObjectModeTool();
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
	static void Command_SelectVertex();
	static void Command_SelectEdge();
	static void Command_SelectFace();
	static void Command_SelectPolygon();

	void UpdateSelectionGizmo();
	bool GetSelectionReferenceFrame( Matrix34 &refFrame );

	//////////////////////////////////////////////////////////////////////////
	CPoint m_cMouseDownPos;
	ECommandMode m_commandMode;

	CBaseObject* m_pMouseOverObject;

	CSubObjSelectionTypePanel * m_pTypePanel;

	// Selected objects.
	typedef std::vector<_smart_ptr<CBaseObject> > SelectedObjectList;
	SelectedObjectList m_selectedObjects;
	static ESubObjElementType m_currSelectionType;

	std::vector<Vec3> m_xformedVertces;

	int m_selectionTypePanelId;
	int m_selectionPanelId;
	int m_displayPanelId;

	struct CDisplayPanelUI *m_pDisplayPanelUI;
	
	static CSubObjectModeTool *m_pCurrentSubObjModeTool;
};

#endif //__VertexMode_h__
