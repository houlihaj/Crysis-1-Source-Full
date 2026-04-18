////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek, 2007.
// -------------------------------------------------------------------------
//  File name:   PolygonTool.h
//  Version:     v1.00
//  Created:     11/1/2007 by Timur.
//  Description: Polygon creation edit tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __PolygonTool_h__
#define __PolygonTool_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "..\EditTool.h"

// {DE21F22D-75BF-449e-9ACD-AED83B2F469C}
DEFINE_GUID( CREATE_POLYGON_TOOL_GUID, 0xde21f22d, 0x75bf, 0x449e, 0x9a, 0xcd, 0xae, 0xd8, 0x3b, 0x2f, 0x46, 0x9c);

class CTriMesh;

//////////////////////////////////////////////////////////////////////////
class CPolygonTool : public CEditTool
{
	DECLARE_DYNCREATE(CPolygonTool)
public:
	CPolygonTool();

	// Registration function.
	static void RegisterTool( CRegistrationContext &rc );

	//////////////////////////////////////////////////////////////////////////
	// CEditTool overrides.
	//////////////////////////////////////////////////////////////////////////
	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams();

	virtual void Display( DisplayContext &dc );
	virtual bool OnSetCursor( CViewport *vp );
	virtual bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	//////////////////////////////////////////////////////////////////////////

protected:
	virtual ~CPolygonTool();
	// Delete itself.
	void DeleteThis() { delete this; };
	
private:
	Vec3 HitTest( CViewport *view,CPoint point,HitContext &hitInfo );
	void MakePoly( CViewport *view,CPoint p1,CPoint p2 );

	void MakeMesh( CTriMesh &mesh );

	// Specific mouse events handlers.
	bool OnLButtonDown( CViewport *view,UINT nFlags,CPoint point );
	bool OnLButtonUp( CViewport *view,UINT nFlags,CPoint point );
	bool OnMouseMove( CViewport *view,UINT nFlags,CPoint point );

	//! Current brush tool operation mode.
	bool m_bCreating;
	bool m_bMouseCaptured;
	CPoint m_mouseDownPos;
	CPoint m_mouseCurPos;

	_smart_ptr<CBaseObject> m_pObject;

	Vec3 m_pos1,m_pos2;

	//////////////////////////////////////////////////////////////////////////
	// Cursors.
	//////////////////////////////////////////////////////////////////////////
	HCURSOR m_hDefaultCursor;
};


#endif // __PolygonTool_h__
