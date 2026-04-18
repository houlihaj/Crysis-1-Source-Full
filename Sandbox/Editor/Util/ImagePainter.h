////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   imagepainter.h
//  Version:     v1.00
//  Created:     11/10/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __imagepainter_h__
#define __imagepainter_h__

#if _MSC_VER > 1000
#pragma once
#endif


// Brush structure used for painting.
struct SEditorPaintBrush
{
	// constructor
	SEditorPaintBrush( class CHeightmap &rHeightmap, class CLayer &rLayer,
		const bool bMaskByLayerSettings, const uint32 dwLayerIdMask ) ;

	CHeightmap & 			m_rHeightmap;		// for mask support
	unsigned char			color;					// Painting color
	float							fRadius;				// outer radius (0..1 for the whole terrain size)
	float							hardness;				// 0-1 hardness of brush
	bool							bBlended;				// true=shades of the value are stores, false=the value is either stored or not
	uint32						m_dwLayerIdMask;// reference Value for the mask, 0xffffffff if not used
	CLayer &					m_rLayer;				// layer we paint with
	ColorF						m_cFilterColor;	// (1,1,1) if not used, multiplied with brightness

	// Arguments:
	//   fX - 0..1 in the whole terrain
	//   fY - 0..1 in the whole terrain
	// Return:
	//   0=paint there 0% .. 1=paint there 100%
	float GetMask( const float fX, const float fY ) const;

protected: // --------------------------------------------------------------------------

	float							m_fMinSlope;				// in m per m
	float							m_fMaxSlope;				// in m per me
	float							m_fMinAltitude;			// in m
	float							m_fMaxAltitude;			// in m
};


// Contains image painting functions.
class CImagePainter
{
public:

	// Paint spot on image at position px,py with specified paint brush parameters (to a layer)
	// Arguments:
	//   fpx - 0..1 in the whole terrain (used for the mask)
	//   fpy - 0..1 in the whole terrain (used for the mask)
	void PaintBrush( const float fpx, const float fpy, CByteImage &image, const SEditorPaintBrush &brush );

	// Paint spot with pattern (to an RGB image)
	// real spot is drawn to (fpx-dwOffsetX,fpy-dwOffsetY) - to get the pattern working we need this info split up like this
	// Arguments:
	//   fpx - 0..1 in the whole terrain (used for the mask)
	//   fpy - 0..1 in the whole terrain (used for the mask)
	void PaintBrushWithPattern( const float fpx, const float fpy, CImage &outImage, const uint32 dwOffsetX, const uint32 dwOffsetY,
		const float fScaleX, const float fScaleY, const SEditorPaintBrush &brush, const CImage &imgPattern );

	//
	void FillWithPattern( CImage &outImage, const uint32 dwOffsetX, const uint32 dwOffsetY, const CImage &imgPattern );
};


#endif // __imagepainter_h__
