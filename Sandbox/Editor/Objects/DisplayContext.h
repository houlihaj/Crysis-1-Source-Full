////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   DisplayContext.h
//  Version:     v1.00
//  Created:     4/12/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: DisplayContext definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DisplayContext_h__
#define __DisplayContext_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "..\IconManager.h"

// forward declarations.
class CViewport;

enum DisplayFlags {
	DISPLAY_2D = 0x01,
	DISPLAY_HIDENAMES = 0x02,
	DISPLAY_BBOX = 0x04,
	DISPLAY_TRACKS = 0x08,
	DISPLAY_TRACKTICKS = 0x010,
	DISPLAY_WORLDSPACEAXIS = 0x020, //!< Set if axis must be displayed in world space.
	DISPLAY_LINKS = 0x040,
	DISPLAY_DEGRADATED = 0x080,	//!< Display Objects in degradated quality (When moving/modifying).
	DISPLAY_SELECTION_HELPERS = 0x100,	//!< Display advanced selection helpers.
};

/*!
 *  DisplayContex is a structure passed to BaseObject Display method.
 *	It contains everything the object should know to display itself in a view.
 *	All fields must be filled before passing that structure to Display call.
 */
struct DisplayContext
{
	enum ETextureIconFlags
	{
		TEXICON_ADDITIVE = 0x0001,
		TEXICON_ALIGN_BOTTOM = 0x0002,
	};

	CDisplaySettings* settings;
	CViewport* view;
	IRenderer* renderer;
	IRenderAuxGeom *pRenderAuxGeom;
	I3DEngine* engine;
	CCamera*	camera;
	BBox	box;	// Bounding box of volume that need to be repainted.
	int flags;


	//! Ctor.
	DisplayContext();
	// Helper methods.
	void SetView( CViewport *pView );
	void Flush2D();
	
	//////////////////////////////////////////////////////////////////////////
	// Draw functions
	//////////////////////////////////////////////////////////////////////////
	//! Set current materialc color.
	void SetColor( float r,float g,float b,float a=1 ) { m_color4b = ColorB(r*255.0f,g*255.0f,b*255.0f,a*255.0f); };
	void SetColor( const Vec3 &color,float a=1 ) { m_color4b = ColorB(color.x*255.0f,color.y*255.0f,color.z*255.0f,a*255.0f); };
	void SetColor( COLORREF rgb,float a=1 ) { m_color4b = ColorB(GetRValue(rgb),GetGValue(rgb),GetBValue(rgb),a*255.0f); };
	void SetColor( const ColorB &color ) { m_color4b = color; };
	void SetAlpha( float a=1 ) { m_color4b.a = a*255.0f; };
	ColorB GetColor() const { return m_color4b; }

	void SetSelectedColor( float fAlpha=1 );
	void SetFreezeColor();

	//! Get color to draw selectin of object.
	COLORREF GetSelectedColor();
	COLORREF GetFreezeColor();

	// Draw 3D quad.
	void DrawQuad( const Vec3 &p1,const Vec3 &p2,const Vec3 &p3,const Vec3 &p4 );
	// Draw 3D Triangle.
	void DrawTri( const Vec3 &p1,const Vec3 &p2,const Vec3 &p3 );
	// Draw wireframe box.
	void DrawWireBox( const Vec3 &min,const Vec3 &max );
	// Draw filled box
	void DrawSolidBox( const Vec3 &min,const Vec3 &max );
	void DrawPoint( const Vec3 &p,int nSize=1 );
	void DrawLine( const Vec3 &p1,const Vec3 &p2 );
	void DrawLine( const Vec3 &p1,const Vec3 &p2,const ColorF &col1,const ColorF &col2 );
	void DrawLine( const Vec3 &p1,const Vec3 &p2,COLORREF rgb1,COLORREF rgb2 );
	void DrawPolyLine( const Vec3 *pnts,int numPoints );
	
	void DrawWireQuad2d( const CPoint &p1,const CPoint &p2,float z );
	void DrawLine2d( const CPoint &p1,const CPoint &p2,float z );
	void DrawWireCircle2d( const CPoint &center,float radius,float z );

	// Draw circle from lines on terrain, position is in world space.
	void DrawTerrainCircle( const Vec3 &worldPos,float radius,float height, bool bIncludeOutdoorVoxels = false );

	// Draw circle.
	void DrawCircle( const Vec3 &pos,float radius );

	//! Draw rectangle on top of terrain.
	//! Coordinates are in world space.
	void DrawTerrainRect( float x1,float y1,float x2,float y2,float height );

	void DrawWireSphere( const Vec3 &pos,float radius );
	void DrawWireSphere( const Vec3 &pos,const Vec3 radius );

	void PushMatrix( const Matrix34 &tm );
	void PopMatrix();
	const Matrix34& GetMatrix();

	// Draw special 3D objects.
	void DrawBall( const Vec3 &pos,float radius );
	
	//! Draws 3d arrow.
	void DrawArrow( const Vec3 &src,const Vec3 &trg,float fHeadScale=1,bool b2SidedArrow=false );

	//! Draw 3D icon.f
	void DrawIcon( EIcon icon,const Vec3 &pos,float fScale=1 );

	// Draw texture label in 2d view coordinates.
	// w,h in pixels.
	void DrawTextureLabel( const Vec3& pos,int nWidth,int nHeight,int nTexId,int nTexIconFlags=0 );

	void RenderObject( int objectType,const Vec3 &pos,float scale );
	void RenderObject( int objectType,const Matrix34 &tm );

	void DrawTextLabel( const Vec3& pos,float size,const char *text, const bool bCenter=false );
	void Draw2dTextLabel( float x,float y,float size,const char *text );
	void SetLineWidth( float width );

	//! Is givven bbox visible in this display context.
	bool IsVisible( const BBox &bounds );

	//! Gets current render state.
	uint32 GetState() const;
	//! Set a new render state.
	//! @param returns previous render state.
	uint32 SetState( uint32 state );
	//! Set a new render state flags.
	//! @param returns previous render state.
	uint32 SetStateFlag( uint32 state );
	//! Clear specified flags in render state.
	//! @param returns previous render state.
	uint32 ClearStateFlag( uint32 state );

	void DepthTestOff();
	void DepthTestOn();
	
	void DepthWriteOff();
	void DepthWriteOn();

	void CullOff();
	void CullOn();

	// Enables drawing helper lines in front of usual geometry, adds a small z offset to all drawn lines.
	bool SetDrawInFrontMode( bool bOn );

	// Description:
	//   Changes fill mode.
	// Arguments:
	//    nFillMode is one of the values from EAuxGeomPublicRenderflags_FillMode
	int  SetFillMode( int nFillMode );

private:
	// Convert vector to world space.
	Vec3 ToWS( const Vec3 &v ) { return m_matrixStack[m_currentMatrix].TransformPoint(v); }

	void InternalDrawLine( const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1 );

	ColorB m_color4b;
	uint32 m_renderState;
	float m_thickness;
	float m_width;
	float m_height;

	int m_currentMatrix;
	//! Matrix stack.
	Matrix34 m_matrixStack[32];

	struct STextureLabel
	{
		float x,y,z; // 2D position (z in world space).
		float w,h;  // Width height.
		int nTexId; // Texture id.
		int flags;  // ETextureIconFlags
		float color[4];
	};
	std::vector<STextureLabel> m_textureLabels;
};

#endif // __DisplayContext_h__
