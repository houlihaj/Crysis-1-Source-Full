////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   BrushClipTool.h
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain modification tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __BrushClipTool_h__
#define __BrushClipTool_h__

#if _MSC_VER > 1000
#pragma once
#endif

// {01B4ED46-F214-4f7e-988B-DF0A149F8FA4}
DEFINE_GUID(CLIPBRUSH_TOOL_GUID,0x1b4ed46, 0xf214, 0x4f7e, 0x98, 0x8b, 0xdf, 0xa, 0x14, 0x9f, 0x8f, 0xa4);

#include "..\EditTool.h"
#include "Brush.h"

//////////////////////////////////////////////////////////////////////////
class CBrushClipTool : public CEditTool
{
	DECLARE_DYNCREATE(CBrushClipTool)
public:
	CBrushClipTool();

	static void RegisterTool( CRegistrationContext &rc );

	//////////////////////////////////////////////////////////////////////////
	// CEditTool overrides.
	//////////////////////////////////////////////////////////////////////////
	virtual bool Activate( CEditTool *pPreviousTool );

	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams();

	virtual void Display( DisplayContext &dc );
	virtual bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	// Key down.
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );

	virtual void OnManipulatorDrag( CViewport *view,ITransformManipulator *pManipulator,CPoint &p0,CPoint &p1,const Vec3 &value );
	//////////////////////////////////////////////////////////////////////////

	// nType = 0, Clip Front.
	// nType = 1, Clip Back.
	// nType = 2, Clip Split.
	void DoClip( int nType );
	
	void SetPlaneScale( float fScale ) { m_fPlaneScale = fScale; };
	float GetPlaneScale() const { return m_fPlaneScale; };

	void SetPlaneAngle( float fScale );
	float GetPlaneAngle() const { return m_fPlaneAngle; };

	// nType = 0, 3 Points.
	// nType = 1, 2 Points and Angle.
	void SetPlaneType( int nType );
	float GetPlaneType() const { return m_nPlaneType; };
	
protected:
	virtual ~CBrushClipTool();
	// Delete itself.
	void DeleteThis() { delete this; };

	void UpdateClipBrushes();
	void ClearBrushes();

	void UpdateClipPlane();
	void EnableManipulator();
	
private:
	bool CheckVirtualKey( int virtualKey );
	int GetNearestPoint( CViewport *view,CPoint point,Vec3 &pos );
	void DrawPoint( DisplayContext &dc,int n );

	void RecordUndo();

	// Specific mouse events handlers.
	bool OnLButtonDown( CViewport *view,UINT nFlags,CPoint point );
	bool OnLButtonUp( CViewport *view,UINT nFlags,CPoint point );
	bool OnLButtonDblClk( CViewport *view,UINT nFlags,CPoint point );
	bool OnMouseMove( CViewport *view,UINT nFlags,CPoint point );

	friend class CUndoBrushClipTool;

	//! Operation modes of this viewport.
	enum OpMode
	{
		SelectMode,
		DragMode,
	};

	std::vector<_smart_ptr<SBrush> > m_frontBrushes;
	std::vector<_smart_ptr<SBrush> > m_backBrushes;

	int m_dragPoint;
	Vec3 m_points[3];

	Plane m_clipPlane;
	CViewport *m_pDragView;
	
	//! Current brush tool operation mode.
	OpMode m_mode;
	bool m_bMouseCaptured;
	CPoint m_mouseDownPos;
	
	static float m_fPlaneScale;
	static float m_fPlaneAngle;
	static int m_nPlaneType;

	static int m_nPanelId;
};


#endif // __BrushClipTool_h__
