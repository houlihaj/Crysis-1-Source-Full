/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A wrapper around Scaleform's GRenderer interface to delegate all rendering to CryEngine's IRenderer interface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _GRENDERER_XRENDER_H_
#define _GRENDERER_XRENDER_H_

#pragma once


#ifndef EXCLUDE_SCALEFORM_SDK


#include <GRenderer.h>
#include <vector>


struct IRenderer;
class ITexture;
class GTextureXRender;
class GRendererXRender;
template <class T> struct Color_tpl;
typedef Color_tpl<float> ColorF;
struct SSF_GlobalDrawParams;

struct IObserverLifeTime_GRenderer
{
	virtual void OnDestroy( GRendererXRender* ) = 0;
};


class GRendererXRender : public GRenderer
{
	// GRenderer interface
public:

	virtual	Bool GetRenderCaps( RenderCaps* pCaps );
	virtual GTexture* CreateTexture();	

	virtual void BeginDisplay( GColor backgroundColor, const GViewport& viewport, Float x0, Float x1, Float y0, Float y1 );	
	virtual void EndDisplay();

	virtual void SetMatrix( const Matrix& m );
	virtual void SetUserMatrix( const Matrix& m );
	virtual void SetCxform( const Cxform& cx );

	virtual void PushBlendMode( BlendType mode );
	virtual void PopBlendMode();

	virtual void SetVertexData( const void* pVertices, int numVertices, VertexFormat vf, CacheProvider* pCache = 0 );
	virtual void SetIndexData( const void* pIndices, int numIndices, IndexFormat idxf, CacheProvider* pCache = 0 );
	virtual void ReleaseCachedData( CachedData* pData, CachedDataType type );

	virtual void DrawIndexedTriList( int baseVertexIndex, int minVertexIndex, int numVertices, int startIndex, int triangleCount );
	virtual void DrawLineStrip( int baseVertexIndex, int lineCount );

	virtual void FillStyleDisable();
	virtual void FillStyleColor( GColor color );
	virtual void FillStyleBitmap( const FillTexture* pFill );
	virtual void FillStyleGouraud( GouraudFillType fillType, const FillTexture* pTexture0 = 0, const FillTexture* pTexture1 = 0, const FillTexture* pTexture2 = 0 );

	virtual void LineStyleDisable();
	virtual void LineStyleColor( GColor color );
	virtual void LineStyleWidth( Float width );

	virtual void DrawBitmaps( BitmapDesc* pBitmapList, int listSize, int startIndex, int count, const GTexture* pTi, const Matrix& m, CacheProvider* pCache = 0 );

	virtual void SetAntialiased( Bool enable );

	virtual void BeginSubmitMask( SubmitMaskMode maskMode );
	virtual void EndSubmitMask();
	virtual void DisableMask();

	virtual	void GetRenderStats( Stats* pStats, Bool resetStats = 0 );

public:
	GRendererXRender( IObserverLifeTime_GRenderer* pObserver );
	virtual ~GRendererXRender();
	GTextureXRender* CreateTextureX();
	IRenderer* GetXRender();
	void Clear( const GColor& backgroundColor, float x0, float y0, float x1, float y1 );
	void SetCompositingDepth( float depth );

private:
	void ConvertTransMat( const Matrix& matSrc, Matrix34& matDst ) const;
	void ApplyBlendMode( BlendType blendMode );
	void ApplyColor( const GColor& src );
	void ApplyTextureInfo( unsigned int texSlot, const FillTexture* pFill = 0 );
	void ApplyCxform();

private:
	static ICVar* CV_sys_flash_debugdraw;
	static int ms_sys_flash_debugdraw;
	
	static ICVar* CV_sys_flash_newstencilclear;
	static int ms_sys_flash_newstencilclear;

private:
	struct SRect
	{
		SRect(float x0, float y0, float x1, float y1)
		: m_x0(x0)
		, m_y0(y0)
		, m_x1(x1)
		, m_y1(y1) 
		{
		}

		void Set(float x0, float y0, float x1, float y1)
		{
			*this = SRect(x0, y0, x1, y1);
		}

		float m_x0;
		float m_y0;
		float m_x1;
		float m_y1;
	};	

private:
	IRenderer* m_pXRender;
	IObserverLifeTime_GRenderer* m_pObserver;

	bool m_stencilAvail;
	bool m_renderMasked;
	int m_stencilCounter;

	bool m_scissorEnabled;

	int m_maxVertices;
	int m_maxIndices;

	// transformation matrix 
	Matrix34 m_mat;
	Matrix34 m_matViewport;

	// current color transform for all draw modes
	Cxform m_cxform;

	// system vertex buffer for streaming glyphs
	struct SGlyphVertex
	{
		float x, y, u, v;
		unsigned int col;
	};
	std::vector< SGlyphVertex > m_glyphVB;

	// blend mode support
	std::vector< BlendType > m_blendOpStack;
	BlendType m_curBlendMode;

	// render stats
	Stats m_renderStats;

	// draw parameters
	SSF_GlobalDrawParams* m_pDrawParams;

	SRect m_canvasRect;

	float m_depth;
};


#endif // #ifndef EXCLUDE_SCALEFORM_SDK


#endif // #ifndef _GRENDERER_XRENDER_H_