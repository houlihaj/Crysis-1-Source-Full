#include "StdAfx.h"


#ifndef EXCLUDE_SCALEFORM_SDK

#include "GRendererXRender.h"
#include "GTextureXRender.h"
#include "SharedStates.h"

#include <ISystem.h>
#include <IRenderer.h>
#include <IConsole.h>


ICVar* GRendererXRender::CV_sys_flash_debugdraw(0);
int GRendererXRender::ms_sys_flash_debugdraw(0);

ICVar* GRendererXRender::CV_sys_flash_newstencilclear(0);
int GRendererXRender::ms_sys_flash_newstencilclear(0);


GRendererXRender::GRendererXRender( IObserverLifeTime_GRenderer* pObserver )
: m_pXRender(0)
, m_pObserver(pObserver)
, m_stencilAvail(false)
, m_renderMasked(false)
, m_stencilCounter(0)
, m_scissorEnabled(false)
, m_maxVertices(0)
, m_maxIndices(0)
, m_mat()
, m_matViewport()
, m_cxform()
, m_glyphVB()
, m_blendOpStack()
, m_curBlendMode(Blend_None)
, m_renderStats()
, m_pDrawParams(0)
, m_canvasRect(0, 0, 0, 0)
, m_depth(0)
{
	m_pXRender = gEnv->pRenderer;
	assert(m_pXRender);
	m_stencilAvail = (m_pXRender->GetStencilBpp() != 0);	
	m_blendOpStack.reserve(16);	
	m_renderStats.Clear();	
	m_pDrawParams = new SSF_GlobalDrawParams;
	
	m_pXRender->SF_GetMeshMaxSize(m_maxVertices, m_maxIndices);

	if (!CV_sys_flash_debugdraw)
		CV_sys_flash_debugdraw = gEnv->pConsole->Register("sys_flash_debugdraw", &ms_sys_flash_debugdraw, 0, VF_CHEAT, "Enables/disables debug drawing of flash files.\n");

	if (!CV_sys_flash_newstencilclear)
		CV_sys_flash_newstencilclear = gEnv->pConsole->Register("sys_flash_newstencilclear", &ms_sys_flash_newstencilclear, 0, VF_CHEAT);
}


GRendererXRender::~GRendererXRender()
{
	delete m_pDrawParams;

	if (m_pObserver)
		m_pObserver->OnDestroy(this);
}


Bool GRendererXRender::GetRenderCaps( RenderCaps* pCaps )
{
	pCaps->CapBits = Cap_Index16 | Cap_FillGouraud | Cap_FillGouraudTex | Cap_CxformAdd | Cap_NestedMasks; // should we allow 32 bit indices if supported?
	pCaps->BlendModes	= (1 << Blend_None) | (1 << Blend_Normal) | (1 << Blend_Multiply) | (1 << Blend_Lighten) | (1 << Blend_Darken) | (1 << Blend_Add) | (1 << Blend_Subtract);
	pCaps->VertexFormats = (1 << Vertex_None) | (1 << Vertex_XY16i) | (1 << Vertex_XY16iC32) | (1 << Vertex_XY16iCF32);
	pCaps->MaxTextureSize = m_pXRender->GetMaxTextureSize();
	return true;
}


GTexture* GRendererXRender::CreateTexture()
{
	return CreateTextureX();
}


GTextureXRender* GRendererXRender::CreateTextureX()
{
	return new GTextureXRender(this);
}


void GRendererXRender::BeginDisplay( GColor backgroundColor, const GViewport& viewport, Float x0, Float x1, Float y0, Float y1 )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	assert((x1 - x0) != 0 && (y1 - y0) != 0);

	float invWidth(1.0f / (x1 - x0));
	float invHeight(1.0f / (y1 - y0));

	m_matViewport = Matrix34::CreateIdentity();
	m_matViewport.m00 = (float) viewport.Width * invWidth;
	m_matViewport.m11 = (float) viewport.Height * invHeight;
	m_matViewport.m03 = -x0 * invWidth + (float) viewport.Left;
	m_matViewport.m13 = -y0 * invHeight + (float) viewport.Top;

	m_scissorEnabled = (viewport.Flags & GViewport::View_UseScissorRect) != 0;
	if (m_scissorEnabled)
	{
		int rtX0(0), rtY0(0), rtWidth(0), rtHeight(0);
		m_pXRender->GetViewport(&rtX0, &rtY0, &rtWidth, &rtHeight);

		int scX0(rtX0), scY0(rtY0), scX1(rtX0 + rtWidth), scY1(rtY0 + rtHeight);	
		if (scX0 < viewport.ScissorLeft)
			scX0 = viewport.ScissorLeft;
		if (scY0 < viewport.ScissorTop)
			scY0 = viewport.ScissorTop;
		if (scX1 > viewport.ScissorLeft + viewport.ScissorWidth)
			scX1 = viewport.ScissorLeft + viewport.ScissorWidth;
		if (scY1 > viewport.ScissorTop + viewport.ScissorHeight)
			scY1 = viewport.ScissorTop + viewport.ScissorHeight;

		m_pXRender->SetScissor(scX0, scY0, scX1 - scX0, scY1 - scY0);
	}

	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	params.blendModeStates = 0;
	params.renderMaskedStates = 0;
	m_stencilCounter = 0;

	m_blendOpStack.resize(0);
	ApplyBlendMode(Blend_None);

	Clear(backgroundColor, x0, y0, x1, y1);	

	m_canvasRect.Set(x0, y0, x1, y1);
}


void GRendererXRender::EndDisplay()
{
	assert(!m_stencilCounter);

	if (m_scissorEnabled)
		m_pXRender->SetScissor();
}


void GRendererXRender::SetMatrix( const Matrix& m )
{
	ConvertTransMat(m, m_mat);
}


void GRendererXRender::SetUserMatrix( const Matrix& m )
{
	// Not supported/needed yet. GFx implements this for debugging anyway.
}


void GRendererXRender::SetCxform( const Cxform& cx )
{
	m_cxform = cx;
}


void GRendererXRender::PushBlendMode( BlendType mode )
{
	m_blendOpStack.push_back(m_curBlendMode);

	if ((mode > Blend_Layer) && (m_curBlendMode != mode))
	{
		ApplyBlendMode(mode);
	}
}


void GRendererXRender::PopBlendMode()
{
	assert(m_blendOpStack.size() && "GRendererXRender::PopBlendMode() -- Blend mode stack is empty!");

	if (m_blendOpStack.size())
	{								
		BlendType	newBlendMode(Blend_None);

		for (int i(m_blendOpStack.size() - 1); i >= 0;  --i)
		{
			if (m_blendOpStack[i] > Blend_Layer)
			{
				newBlendMode = m_blendOpStack[i];
				break;
			}
		}		

		m_blendOpStack.pop_back();

		if (newBlendMode != m_curBlendMode)
		{
			ApplyBlendMode(newBlendMode);
		}
	}	
}


void GRendererXRender::ApplyBlendMode( BlendType blendMode )
{
	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;

	if (ms_sys_flash_debugdraw)
	{
		m_curBlendMode = Blend_None;
		params.blendModeStates = 0;
		return;
	}
		
	m_curBlendMode = blendMode;
	params.blendModeStates = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;

	switch (blendMode)
	{
	case Blend_None:
	case Blend_Normal:
	case Blend_Layer:
	case Blend_Screen:
	case Blend_Difference:
	case Blend_Invert:
	case Blend_Overlay:
	case Blend_HardLight:
		params.blendModeStates = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
		break;
	case Blend_Multiply:
		params.blendModeStates = GS_BLSRC_DSTCOL | GS_BLDST_ZERO;
		break;
	case Blend_Add:
	case Blend_Subtract:
	case Blend_Lighten:
	case Blend_Darken:
		params.blendModeStates = GS_BLSRC_SRCALPHA | GS_BLDST_ONE;
		break;
	case Blend_Alpha:
	case Blend_Erase:
		params.blendModeStates = GS_BLSRC_ZERO | GS_BLDST_ONE;
		break;
	default:
		assert(!"GRendererXRender::ApplyBlendMode() -- Unsupported blend type!");
		break;
	}

	switch (blendMode)
	{
	case Blend_Subtract:
		params.blendOp = SSF_GlobalDrawParams::RevSubstract;
		break;
	case Blend_Lighten:
		params.blendOp = SSF_GlobalDrawParams::Max;
		break;
	case Blend_Darken:
		params.blendOp = SSF_GlobalDrawParams::Min;
		break;
	default: 
		params.blendOp = SSF_GlobalDrawParams::Add;
		break;
	}	
	params.isMultiplyDarkBlendMode = m_curBlendMode == Blend_Multiply || m_curBlendMode == Blend_Darken;	
}


void GRendererXRender::ApplyColor( const GColor& src )
{
	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	
	ColorF& dst(params.colTransform1st);
	const float scale(1.0f / 255.0f);
	dst.r = src.GetRed() * scale;
	dst.g = src.GetGreen() * scale;
	dst.b = src.GetBlue() * scale;
	dst.a = src.GetAlpha() * scale;
	if (m_curBlendMode == Blend_Multiply || m_curBlendMode == Blend_Darken)
	{
		float a(dst.a);
		dst.r = 1.0f + a * (dst.r - 1.0f);
		dst.g = 1.0f + a * (dst.g - 1.0f);
		dst.b = 1.0f + a * (dst.b - 1.0f);
	}

	params.colTransform2nd = ColorF(0, 0, 0, 0);
}


void GRendererXRender::ApplyTextureInfo(unsigned int texSlot, const FillTexture* pFill)
{
	assert(texSlot < 2);
	if (texSlot >= 2)
		return;

	SSF_GlobalDrawParams::STextureInfo& texInfo(m_pDrawParams->texture[texSlot]);
	if (pFill && pFill->pTexture)
	{
		const GTextureXRender* pTex(static_cast<const GTextureXRender*>(pFill->pTexture));

		assert(pTex->GetWidth() > 0 && pTex->GetHeight() > 0);
		const Matrix& m(pFill->TextureMatrix);

		texInfo.texID = pTex->GetID();

		float invWidth(1.0f / (float) pTex->GetWidth());
		texInfo.texGenMat.m00 = m.M_[0][0] * invWidth; 
		texInfo.texGenMat.m01 = m.M_[0][1] * invWidth; 
		texInfo.texGenMat.m02 = 0; 
		texInfo.texGenMat.m03 = m.M_[0][2] * invWidth;

		float invHeight(1.0f / (float) pTex->GetHeight());
		texInfo.texGenMat.m10 = m.M_[1][0] * invHeight; 
		texInfo.texGenMat.m11 = m.M_[1][1] * invHeight; 
		texInfo.texGenMat.m12 = 0; 
		texInfo.texGenMat.m13 = m.M_[1][2] * invHeight;

		texInfo.texGenMat.m20 = 0; 
		texInfo.texGenMat.m21 = 0; 
		texInfo.texGenMat.m22 = 0; 
		texInfo.texGenMat.m23 = 1;

		texInfo.texState = (pFill->WrapMode == Wrap_Clamp) ? SSF_GlobalDrawParams::TS_Clamp : 0;
		texInfo.texState |= (pFill->SampleMode == Sample_Linear) ? SSF_GlobalDrawParams::TS_FilterLin : 0;
	}
	else
	{
		texInfo.texID = 0;
		texInfo.texGenMat.SetIdentity();
		texInfo.texState = 0;
	}
}


void GRendererXRender::ApplyCxform()
{
	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	params.colTransform1st = ColorF(m_cxform.M_[0][0], m_cxform.M_[1][0], m_cxform.M_[2][0], m_cxform.M_[3][0]);	
	params.colTransform2nd = ColorF(m_cxform.M_[0][1] / 255.0f, m_cxform.M_[1][1] / 255.0f, m_cxform.M_[2][1] / 255.0f, m_cxform.M_[3][1] / 255.0f);
}


void GRendererXRender::SetVertexData( const void* pVertices, int numVertices, VertexFormat vf, CacheProvider* /*pCache*/ )
{
	assert((!pVertices || vf == Vertex_XY16i || vf == Vertex_XY16iC32 || vf == Vertex_XY16iCF32) && "GRendererXRender::SetVertexData() -- Unsupported source vertex format!");
	if (pVertices && numVertices > 0 && numVertices <= m_maxVertices && (vf == Vertex_XY16i || vf == Vertex_XY16iC32 || vf == Vertex_XY16iCF32))
	{
		SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
		switch (vf)
		{
		case Vertex_XY16i:
			params.vertexFmt = SSF_GlobalDrawParams::Vertex_XY16i;
			break;
		case Vertex_XY16iC32:
			params.vertexFmt = SSF_GlobalDrawParams::Vertex_XY16iC32;
			break;
		case Vertex_XY16iCF32:
			params.vertexFmt = SSF_GlobalDrawParams::Vertex_XY16iCF32;
			break;
		}
		params.pVertexPtr = pVertices;
		params.numVertices = numVertices;
	}
	else
	{
		SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
		params.vertexFmt = SSF_GlobalDrawParams::Vertex_None;
		params.pVertexPtr = 0;
		params.numVertices = 0;

		if (numVertices > m_maxVertices)
			CryGFxLog::GetAccess().Log("Trying to send too many vertices at once (amount = %d, limit = %d)!", numVertices, m_maxVertices);
	}
}


void GRendererXRender::SetIndexData( const void* pIndices, int numIndices, IndexFormat idxf, CacheProvider* /*pCache*/ )
{
	assert((!pIndices || idxf == Index_16) && "GRendererXRender::SetIndexData() -- Unsupported source index format!");
	if (pIndices && numIndices > 0 && numIndices <= m_maxIndices && idxf == Index_16)
	{
		SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
		params.indexFmt = SSF_GlobalDrawParams::Index_16;
		params.pIndexPtr = pIndices;
		params.numIndices = numIndices;
	}	
	else
	{
		SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
		params.indexFmt = SSF_GlobalDrawParams::Index_None;
		params.pIndexPtr = 0;
		params.numIndices = 0;

		if (numIndices > m_maxIndices)
			CryGFxLog::GetAccess().Log("Trying to send too many indices at once (amount = %d, limit = %d)!", numIndices, m_maxIndices);
	}
}


void GRendererXRender::ReleaseCachedData( CachedData* pData, CachedDataType type )
{
	// currently we're not keeping track of cached buffers, might be implemented later
}


void GRendererXRender::DrawIndexedTriList( int baseVertexIndex, int minVertexIndex, int numVertices, int startIndex, int triangleCount )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	
	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	if ((m_renderMasked && !m_stencilAvail) || params.vertexFmt == SSF_GlobalDrawParams::Vertex_None || params.indexFmt == SSF_GlobalDrawParams::Index_None)
		return;

	assert(params.vertexFmt != SSF_GlobalDrawParams::Vertex_None);
	assert(params.indexFmt == SSF_GlobalDrawParams::Index_16);

	// setup render parameters
	params.pTransMat = &m_mat;
	
	// draw triangle list
	m_pXRender->SF_DrawIndexedTriList(baseVertexIndex, minVertexIndex, numVertices, startIndex, triangleCount, params);

	params.pVertexPtr = 0;
	params.numVertices = 0;
	params.pIndexPtr = 0;
	params.numIndices = 0;

	// update stats
	m_renderStats.Triangles += triangleCount;
	++m_renderStats.Primitives;
}


void GRendererXRender::DrawLineStrip( int baseVertexIndex, int lineCount )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	if ((m_renderMasked && !m_stencilAvail) || params.vertexFmt != SSF_GlobalDrawParams::Vertex_XY16i)
		return;

	// setup render parameters
	params.pTransMat = &m_mat;

	// draw triangle list
	m_pXRender->SF_DrawLineStrip(baseVertexIndex, lineCount, params);

	params.pVertexPtr = 0;
	params.numVertices = 0;

	// update stats
	m_renderStats.Lines += lineCount;
	++m_renderStats.Primitives;
}


void GRendererXRender::FillStyleDisable()
{
}


void GRendererXRender::FillStyleColor( GColor color )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	ApplyTextureInfo(0);
	ApplyTextureInfo(1);	
	ApplyColor(m_cxform.Transform(color));

	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	params.fillType = SSF_GlobalDrawParams::SolidColor; 
}


void GRendererXRender::FillStyleBitmap( const FillTexture* pFill )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	ApplyTextureInfo(0, pFill);
	ApplyTextureInfo(1);
	ApplyCxform();

	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	params.fillType = SSF_GlobalDrawParams::Texture; 
}


void GRendererXRender::FillStyleGouraud( GouraudFillType fillType, const FillTexture* pTexture0, const FillTexture* pTexture1, const FillTexture* pTexture2 )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
	
	const FillTexture* t0(0);
	const FillTexture* t1(0);

	if (pTexture0 || pTexture1 || pTexture2)
	{
		t0 = pTexture0;
		if (!t0)
			t0 = pTexture1;				
		if (pTexture1)
			t1 = pTexture1; 			
	}

	ApplyTextureInfo(0, t0);
	ApplyTextureInfo(1, t1);
	ApplyCxform();

	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;	
	switch( fillType )
	{
	case GFill_Color: 
		params.fillType = SSF_GlobalDrawParams::GColor; 
		break;
	case GFill_1Texture: 
		params.fillType = SSF_GlobalDrawParams::G1Texture; 
		break;
	case GFill_1TextureColor: 
		params.fillType = SSF_GlobalDrawParams::G1TextureColor; 
		break;
	case GFill_2Texture: 
		params.fillType = SSF_GlobalDrawParams::G2Texture; 
		break;
	case GFill_2TextureColor: 
		params.fillType = SSF_GlobalDrawParams::G2TextureColor; 
		break;
	case GFill_3Texture: 
		params.fillType = SSF_GlobalDrawParams::G3Texture; 
		break;
	default:
		assert(0);
		break;
	}
}


void GRendererXRender::LineStyleDisable()
{
}


void GRendererXRender::LineStyleColor( GColor color )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	ApplyTextureInfo(0);
	ApplyTextureInfo(1);
	ApplyColor(m_cxform.Transform(color));

	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	params.fillType = SSF_GlobalDrawParams::SolidColor; 
}


void GRendererXRender::LineStyleWidth( Float width )
{
	// nothing to do here atm
}


void GRendererXRender::DrawBitmaps( BitmapDesc* pBitmapList, int /*listSize*/, int startIndex, int count, const GTexture* pTi, const Matrix& m, CacheProvider* /*pCache*/ )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	if (!pBitmapList || !pTi)
		return;

	// resize buffer
	uint32 numVertices(4 * count + ( count - 1 ) * 2);
	m_glyphVB.resize(numVertices);

	// merge multiple bitmaps into one draw call via stitching the triangle strip
	for( int bitmapIdx( 0 ), vertOffs( 0 ); bitmapIdx < count; ++bitmapIdx )
	{
		const BitmapDesc& bitmapDesc(pBitmapList[bitmapIdx + startIndex]);
		const Rect& coords(bitmapDesc.Coords);
		const Rect& uvCoords(bitmapDesc.TextureCoords);

		const GColor& srcCol(bitmapDesc.Color);		
		unsigned int color((srcCol.GetAlpha() << 24) | (srcCol.GetBlue() << 16) | (srcCol.GetGreen() << 8) | srcCol.GetRed());

		SGlyphVertex& v0(m_glyphVB[vertOffs]);
		v0.x = coords.Left; v0.y = coords.Top; v0.u = uvCoords.Left; v0.v = uvCoords.Top; v0.col = color;
		++vertOffs;

		if (bitmapIdx > 0)
		{
			m_glyphVB[vertOffs] = m_glyphVB[vertOffs-1];
			++vertOffs;
		}

		SGlyphVertex& v1(m_glyphVB[vertOffs]);
		v1.x = coords.Right; v1.y = coords.Top; v1.u = uvCoords.Right; v1.v = uvCoords.Top; v1.col = color;
		++vertOffs;

		SGlyphVertex& v2(m_glyphVB[vertOffs]);
		v2.x = coords.Left; v2.y = coords.Bottom; v2.u = uvCoords.Left; v2.v = uvCoords.Bottom; v2.col = color;
		++vertOffs;

		SGlyphVertex& v3(m_glyphVB[vertOffs]);
		v3.x = coords.Right; v3.y = coords.Bottom; v3.u = uvCoords.Right; v3.v = uvCoords.Bottom; v3.col = color;
		++vertOffs;

		if (bitmapIdx < count - 1)
		{
			m_glyphVB[vertOffs] = m_glyphVB[vertOffs-1];
			++vertOffs;
		}
	}

	// build transformation matrix
	Matrix34 mat;
	ConvertTransMat(m, mat);	

	// setup render parameters
	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	params.pTransMat = &mat;

	const GTextureXRender* pTex(static_cast<const GTextureXRender*>(pTi));
	params.texture[0].texID = pTex ? pTex->GetID() : 0;
	params.texture[0].texGenMat.SetIdentity();
	params.texture[0].texState = SSF_GlobalDrawParams::TS_FilterTriLin | SSF_GlobalDrawParams::TS_Clamp;

	ApplyTextureInfo(1);
	ApplyCxform();

	params.fillType = SSF_GlobalDrawParams::Glyph;
	params.vertexFmt = SSF_GlobalDrawParams::Vertex_Glyph;
	params.pVertexPtr = &m_glyphVB[0];
	params.numVertices = numVertices;

	// draw glyphs
	m_pXRender->SF_DrawGlyphClear(params);

	params.pVertexPtr = 0;
	params.numVertices = 0;

	// update stats
	m_renderStats.Triangles += numVertices - 2;
	++m_renderStats.Primitives;
}


void GRendererXRender::SetAntialiased( Bool enable )
{
	// nothing to do here atm
}


void GRendererXRender::BeginSubmitMask( SubmitMaskMode maskMode )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	m_renderMasked = true;
	if (!m_stencilAvail)
		return;

	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	params.renderMaskedStates = GS_STENCIL | GS_COLMASK_NONE;

	switch (maskMode)
	{
	case Mask_Clear:
		{
			if (!ms_sys_flash_newstencilclear)
				m_pXRender->ClearBuffer(FRT_CLEAR_STENCIL, 0);
			else
			{
				m_pXRender->SF_ConfigMask(IRenderer::BeginSubmitMask_Clear, 0);
				Clear(GColor(0, 0, 0, 255), m_canvasRect.m_x0, m_canvasRect.m_y0, m_canvasRect.m_x1, m_canvasRect.m_y1);
			}
			m_pXRender->SF_ConfigMask(IRenderer::BeginSubmitMask_Clear, 1);
			m_stencilCounter = 1;
			break;
		}
	case Mask_Increment:
		{
			m_pXRender->SF_ConfigMask(IRenderer::BeginSubmitMask_Inc, m_stencilCounter);
			++m_stencilCounter;
			break;
		}
	case Mask_Decrement:
		{
			m_pXRender->SF_ConfigMask(IRenderer::BeginSubmitMask_Dec, m_stencilCounter);
			--m_stencilCounter;
			break;
		}
	}
}


void GRendererXRender::EndSubmitMask()
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	m_renderMasked = true;
	if (!m_stencilAvail)
		return;

	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	params.renderMaskedStates = GS_STENCIL;
	
	m_pXRender->SF_ConfigMask(IRenderer::EndSubmitMask, m_stencilCounter);
}


void GRendererXRender::DisableMask()
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	m_renderMasked = false;
	if (!m_stencilAvail)
		return;

	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;
	params.renderMaskedStates = 0;

	m_pXRender->SF_ConfigMask(IRenderer::DisableMask, 0);
	m_stencilCounter = 0;
}


void GRendererXRender::GetRenderStats( Stats* pStats, Bool resetStats )
{
	if (pStats)
		memcpy(pStats, &m_renderStats, sizeof(Stats));
	if (resetStats)
		m_renderStats.Clear();
}


IRenderer* GRendererXRender::GetXRender()
{
	return m_pXRender;
}


void GRendererXRender::Clear( const GColor& backgroundColor, float x0, float y0, float x1, float y1 )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	if (backgroundColor.GetAlpha() == 0)
		return;

	SSF_GlobalDrawParams& params = *(SSF_GlobalDrawParams*) m_pDrawParams;

	// setup quad
	uint16 quad[4*2] = 
	{
		(uint16)x0, (uint16)y0,
		(uint16)x1, (uint16)y0,
		(uint16)x0, (uint16)y1,
		(uint16)x1, (uint16)y1
	};

	// setup render parameters
	ApplyTextureInfo(0);
	ApplyTextureInfo(1);
	ApplyColor(backgroundColor);

	params.pTransMat = &m_matViewport;
	params.fillType = SSF_GlobalDrawParams::SolidColor;
	params.vertexFmt = SSF_GlobalDrawParams::Vertex_XY16i;
	params.pVertexPtr = &quad[0];
	params.numVertices = 4;

	int oldBendState(params.blendModeStates);
	params.blendModeStates = backgroundColor.GetAlpha() < 255 ? GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA : 0;
	int oldRenderMaskedStates(params.renderMaskedStates);
	params.renderMaskedStates = m_renderMasked ? params.renderMaskedStates : 0;

	// draw quad
	m_pXRender->SF_DrawGlyphClear(params);

	params.pVertexPtr = 0;
	params.numVertices = 0;
	params.blendModeStates = oldBendState;
	params.renderMaskedStates = oldRenderMaskedStates;

	// update stats
	m_renderStats.Triangles += 2;
	++m_renderStats.Primitives;
}


void GRendererXRender::SetCompositingDepth( float depth )
{
	m_depth = (depth >= 0 && depth <= 1) ? depth : 0;
}


void GRendererXRender::ConvertTransMat( const Matrix& matSrc, Matrix34& matDst ) const
{
	matDst.m00 = matSrc.M_[0][0]; matDst.m01 = matSrc.M_[0][1]; matDst.m02 = 0; matDst.m03 = matSrc.M_[0][2];
	matDst.m10 = matSrc.M_[1][0]; matDst.m11 = matSrc.M_[1][1];	matDst.m12 = 0; matDst.m13 = matSrc.M_[1][2];
	matDst.m20 = 0;								matDst.m21 = 0;								matDst.m22 = 0; matDst.m23 = m_depth;

	matDst = m_matViewport * matDst;
}


#endif // #ifndef EXCLUDE_SCALEFORM_SDK