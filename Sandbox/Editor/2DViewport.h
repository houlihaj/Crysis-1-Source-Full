////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   2dviewport.h
//  Version:     v1.00
//  Created:     5/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __2dviewport_h__
#define __2dviewport_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Viewport.h"
#include "Objects\DisplayContext.h"

/** 2D Viewport used mostly for indoor editing, Front/Top/Left viewports.
*/
class C2DViewport : public CViewport
{
	DECLARE_DYNCREATE(C2DViewport)
public:
	C2DViewport();
	virtual ~C2DViewport();

	virtual void SetType( EViewportType type );
	virtual EViewportType GetType() const { return m_viewType; }
	virtual float GetAspectRatio() const { return 1.0f; };

	virtual void ResetContent();
	virtual void UpdateContent( int flags );

	// Called every frame to update viewport.
	virtual void Update();

	//! Map world space position to viewport position.
	virtual CPoint	WorldToView( Vec3 wp );
	//! Map viewport position to world space position.
	virtual Vec3		ViewToWorld( CPoint vp,bool *collideWithTerrain=0,bool onlyTerrain=false );
	//! Map viewport position to world space ray from camera.
	virtual void		ViewToWorldRay( CPoint vp,Vec3 &raySrc,Vec3 &rayDir );

	virtual void OnTitleMenu( CMenu &menu );
	virtual void OnTitleMenuCommand( int id );

	virtual void DrawBrush( DisplayContext &dc,struct SBrush *brush,const Matrix34 &brushTM,int flags );
	virtual void DrawTextLabel( DisplayContext &dc,const Vec3& pos,float size,const ColorF& color,const char *text, const bool bCenter=false );

	virtual bool HitTest( CPoint point,HitContext &hitInfo );
	virtual bool IsBoundsVisible( const BBox &box ) const;

	// ovverided from CViewport.
	float GetScreenScaleFactor( const Vec3 &worldPoint );

	// Overrided from CViewport.
	void OnDragSelectRectangle( CPoint p1,CPoint p2,bool bNormilizeRect=false );
	void CenterOnSelection();

	/** Get 2D viewports origin.
	*/
	Vec3 GetOrigin2D() const;

	/** Assign 2D viewports origin.
	*/
	void SetOrigin2D( const Vec3 &org );

	void SetShowViewMarker( bool bEnable ) { m_bShowViewMarker = bEnable; }

protected:
	enum EViewportAxis
	{
		VPA_XY,
		VPA_XZ,
		VPA_YZ,
		VPA_YX,
	};

	void SetAxis( EViewportAxis axis );

	// Scrolling / zooming related
	virtual void SetScrollOffset( float x,float y,bool bLimits=true );
	virtual void GetScrollOffset( float &x,float &y ); // Only x and y components used.

	void SetZoom( float fZoomFactor,CPoint center );

	// overrides from CViewport.
	virtual void MakeConstructionPlane( int axis );
	virtual const Matrix34& GetConstructionMatrix( RefCoordSys coordSys );
	
	//! Calculate view transformation matrix.
	virtual void CalculateViewTM();

	// Render
	void Render();
	
	// Draw everything.
	virtual void Draw( DisplayContext &dc );
	
	// Draw elements of viewport.
	void DrawGrid( DisplayContext &dc,bool bNoXNumbers=false );
	void DrawAxis( DisplayContext &dc );
	void DrawSelection( DisplayContext &dc );
	void DrawObjects( DisplayContext &dc );
	void DrawViewerMarker( DisplayContext &dc );

	BBox GetWorldBounds( CPoint p1,CPoint p2 );

	//////////////////////////////////////////////////////////////////////////
	//! Get current screen matrix.
	//! Screen matrix transform from World space to Screen space.
	const Matrix34& GetScreenTM() const { return m_screenTM; };

		//{{AFX_MSG(C2DViewport)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	
protected:
	//////////////////////////////////////////////////////////////////////////
	// Variables.
	//////////////////////////////////////////////////////////////////////////
	IRenderer*	m_renderer;

	//! XY/XZ/YZ mode of this 2D viewport.
	EViewportType m_viewType;
	EViewportAxis m_axis;

	//! Axis to cull normals with in this Viewport.
	int m_cullAxis;

	// Viewport origin point.
	Vec3 m_origin2D;

	// Scrolling / zooming related
	CPoint m_cMousePos;
	CPoint m_RMouseDownPos;

	float m_prevZoomFactor;
	CSize m_prevScrollOffset;

	CRect m_rcSelect;
	CRect m_rcClient;

	BBox m_displayBounds;

	//! True if should display terrain.
	bool m_bShowTerrain;

	// True if shoudl show view marker
	bool m_bShowViewMarker;

	Matrix34 m_screenTM;
	Matrix34 m_screenTM_Inverted;

	float m_gridAlpha;
	COLORREF m_colorGridText;
	COLORREF m_colorAxisText;
	COLORREF m_colorBackground;
	bool m_bContentValid;

	DisplayContext m_displayContext;
};

//////////////////////////////////////////////////////////////////////////
class C2DViewport_XY : public C2DViewport
{
	DECLARE_DYNCREATE(C2DViewport_XY);
public:
	C2DViewport_XY() { SetType(ET_ViewportXY); }
};

//////////////////////////////////////////////////////////////////////////
class C2DViewport_XZ : public C2DViewport
{
	DECLARE_DYNCREATE(C2DViewport_XZ);
public:
	C2DViewport_XZ() { SetType(ET_ViewportXZ); }
};

//////////////////////////////////////////////////////////////////////////
class C2DViewport_YZ : public C2DViewport
{
	DECLARE_DYNCREATE(C2DViewport_YZ);
public:
	C2DViewport_YZ() { SetType(ET_ViewportYZ); }
};

#endif // __2dviewport_h__
