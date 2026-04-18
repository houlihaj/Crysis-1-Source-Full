////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   imagepainter.cpp
//  Version:     v1.00
//  Created:     11/10/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ImagePainter.h"
#include "Image.h"
#include "Heightmap.h"				// CHeightmap for mask computations
#include "Layer.h"						// CLayer for mask computations

SEditorPaintBrush::SEditorPaintBrush( CHeightmap &rHeightmap, CLayer &rLayer,
	const bool bMaskByLayerSettings, const uint32 dwLayerIdMask ) 
	:bBlended(true), m_rHeightmap(rHeightmap), m_rLayer(rLayer), m_cFilterColor(1,1,1), m_dwLayerIdMask(dwLayerIdMask)
{
	if(bMaskByLayerSettings)
	{
		m_fMinAltitude = m_rLayer.GetLayerStart();
		m_fMaxAltitude = m_rLayer.GetLayerEnd();
		m_fMinSlope = tan(m_rLayer.GetLayerMinSlopeAngle()/90.1f*g_PI/2.0f);	// 0..90 -> 0..~1/0
		m_fMaxSlope = tan(m_rLayer.GetLayerMaxSlopeAngle()/90.1f*g_PI/2.0f);	// 0..90 -> 0..~1/0
	}
	else
	{
		m_fMinAltitude = -FLT_MAX;
		m_fMaxAltitude = FLT_MAX;
		m_fMinSlope = 0;
		m_fMaxSlope = FLT_MAX;
	}
}

float SEditorPaintBrush::GetMask( const float fX, const float fY ) const
{
	int iX = (int)(fX*m_rHeightmap.GetWidth()+0.5f), iY = (int)(fY*m_rHeightmap.GetHeight()+0.5f);

	float fAltitude = m_rHeightmap.GetZInterpolated(fX*m_rHeightmap.GetWidth(),fY*m_rHeightmap.GetHeight());

	// Check if altitude is within brush min/max altitude
	if(fAltitude<m_fMinAltitude || fAltitude>m_fMaxAltitude)
		return 0;

	float fSlope = m_rHeightmap.GetAccurateSlope(fX*m_rHeightmap.GetWidth(),fY*m_rHeightmap.GetHeight());

	// Check if slope is within brush min/max slope
	if(fSlope<m_fMinSlope || fSlope>m_fMaxSlope)
		return 0;

	//	Soft slope test
	//	float fSlopeAplha = 1.f;
	//	fSlopeAplha *= CLAMP((m_fMaxSlope-fSlope)*4 + 0.25f,0,1);
	//	fSlopeAplha *= CLAMP((fSlope-m_fMinSlope)*4 + 0.25f,0,1);

	if(m_dwLayerIdMask!=0xffffffff)
	{
		uint32 dwLayerId = m_rHeightmap.GetLayerIdAt(iX,iY);
		
		if(dwLayerId!=m_dwLayerIdMask)
			return 0;
	}

	return 1;
}


//////////////////////////////////////////////////////////////////////////
void CImagePainter::PaintBrush( const float fpx, const float fpy, TImage<unsigned char> &image, const SEditorPaintBrush &brush )
{
	float fX=fpx*image.GetWidth(), fY=fpy*image.GetHeight(); 

	const float fScaleX=1.0f/image.GetWidth();
	const float fScaleY=1.0f/image.GetHeight();

	////////////////////////////////////////////////////////////////////////
	// Draw an attenuated spot on the map
	////////////////////////////////////////////////////////////////////////
	float fMaxDist, fAttenuation, fYSquared;
	float fHardness = brush.hardness;

	unsigned int pos;

	unsigned char *src = image.GetData();

	int value = brush.color;

	// Calculate the maximum distance
	fMaxDist = brush.fRadius*image.GetWidth();

	assert(image.GetWidth()==image.GetHeight());

	int width = image.GetWidth();
	int height = image.GetHeight();

	int iMinX=(int)floor(fX-fMaxDist), iMinY=(int)floor(fY-fMaxDist); 
	int iMaxX=(int)ceil(fX+fMaxDist), iMaxY=(int)ceil(fY+fMaxDist); 

	for (int iPosY = iMinY; iPosY <= iMaxY; iPosY++)
	{
		// Skip invalid locations
		if(iPosY < 0 || iPosY > height - 1)
			continue;

		float fy = (float)iPosY-fY;

		// Precalculate
		fYSquared = (float)(fy*fy);

		for (int iPosX = iMinX; iPosX <= iMaxX; iPosX++)
		{
			float fx = (float)iPosX-fX;

			// Skip invalid locations
			if(iPosX < 0 || iPosX > width - 1)
				continue;

			// Only circle.
			float dist = sqrtf(fYSquared + fx*fx);
			if (dist > fMaxDist)
				continue;

			float fMask = brush.GetMask(iPosX*fScaleX,iPosY*fScaleY);

			if(fMask<0.5f)
				continue;

			// Calculate the array index
			pos = iPosX + iPosY * width;

			if(brush.bBlended) // true=shades of the value are stores, false=the value is either stored or not
			{
				// Calculate attenuation factor
				fAttenuation = 1.0f - __min(1.0f, dist/fMaxDist);

				int h = src[pos];
				int dh = value - h;
				float fh = (fAttenuation)*dh*fHardness + h;
				
				if(src[pos] & HEIGHTMAP_INFO_HOLE)
					src[pos] = ftoi(fh) | HEIGHTMAP_INFO_HOLE;
				else
					src[pos] = ftoi(fh);
			}
			else
			{
				if(src[pos] & HEIGHTMAP_INFO_HOLE)
					src[pos] = value | HEIGHTMAP_INFO_HOLE;
				else
					src[pos] = value;
			}
		}
	}
}


void CImagePainter::PaintBrushWithPattern( const float fpx, const float fpy, CImage &outImage,
	const uint32 dwOffsetX, const uint32 dwOffsetY, const float fScaleX, const float fScaleY,
	const SEditorPaintBrush &brush, const CImage &imgPattern )
{
	float fX=fpx*fScaleX, fY=fpy*fScaleY; 

	////////////////////////////////////////////////////////////////////////
	// Draw an attenuated spot on the map
	////////////////////////////////////////////////////////////////////////
	float fMaxDist, fAttenuation, fYSquared;
	float fHardness = brush.hardness;

	unsigned int pos;

	uint32 *src = outImage.GetData();
	uint32 *pat = imgPattern.GetData();

	int value = brush.color;

	// Calculate the maximum distance
	fMaxDist = brush.fRadius;

	int width = outImage.GetWidth();
	int height = outImage.GetHeight();

	int patwidth = imgPattern.GetWidth();
	int patheight = imgPattern.GetHeight();

	int iMinX=(int)floor(fX-fMaxDist), iMinY=(int)floor(fY-fMaxDist); 
	int iMaxX=(int)ceil(fX+fMaxDist), iMaxY=(int)ceil(fY+fMaxDist); 

	for (int iPosY = iMinY; iPosY < iMaxY; iPosY++)
	{
		// Skip invalid locations
		if(iPosY-dwOffsetY < 0 || iPosY-dwOffsetY > height - 1)
			continue;

		float fy = (float)iPosY-fY;

		// Precalculate
		fYSquared = (float)(fy*fy);

		int32 iPatY = ((uint32)iPosY)%patheight;				assert(iPatY>=0 && iPatY<patheight);

		for (int iPosX = iMinX; iPosX < iMaxX; iPosX++)
		{
			float fx = (float)iPosX-fX;

			// Skip invalid locations
			if(iPosX-dwOffsetX < 0 || iPosX-dwOffsetX > width - 1)
				continue;

			// Only circle.
			float dist = sqrtf(fYSquared + fx*fx);

			if (dist > fMaxDist)
				continue;

			// Calculate the array index
			pos = (iPosX-dwOffsetX) + (iPosY-dwOffsetY) * width;

			// Calculate attenuation factor
			fAttenuation = 1.0f - __min(1.0f, dist/fMaxDist);		assert(fAttenuation>=0.0f && fAttenuation<=1.0f);

			float fMask = brush.GetMask(iPosX/fScaleX,iPosY/fScaleX);

			uint32 cDest = src[pos];

			int32 iPatX = ((uint32)iPosX)%patwidth;				assert(iPatX>=0 && iPatX<patwidth);

			uint32 cSrc = pat[iPatX+iPatY*patwidth];

			float s=fAttenuation*fHardness*fMask;								assert(s>=0.0f && s<=1.0f);

			float SrcR = (cSrc&0x0ff);					// RGB flipped
			float SrcG = (cSrc&0x0ff00)>>8;
			float SrcB = (cSrc&0x0ff0000)>>16;

			SrcR*=brush.m_cFilterColor.r;
			SrcG*=brush.m_cFilterColor.g;
			SrcB*=brush.m_cFilterColor.b;

			SrcR = min(SrcR,255.0f);
			SrcG = min(SrcG,255.0f);
			SrcB = min(SrcB,255.0f);
			
			float DestR = (cDest&0x0ff0000)>>16;
			float DestG = (cDest&0x0ff00)>>8;
			float DestB = (cDest&0x0ff);

			uint32 r = SrcR*s+DestR*(1.0f-s);
			uint32 g = SrcG*s+DestG*(1.0f-s);
			uint32 b = SrcB*s+DestB*(1.0f-s);

			src[pos] = (r<<16) | (g<<8) | b;
		}
	}
}


void CImagePainter::FillWithPattern( CImage &outImage, const uint32 dwOffsetX, const uint32 dwOffsetY, 
	const CImage &imgPattern )
{
	unsigned int pos;

	uint32 *src = outImage.GetData();
	uint32 *pat = imgPattern.GetData();

	int width = outImage.GetWidth();
	int height = outImage.GetHeight();

	int patwidth = imgPattern.GetWidth();
	int patheight = imgPattern.GetHeight();

	if (patheight == 0 || patwidth == 0)
		return;

	for (int iPosY = 0; iPosY < height; iPosY++)
	{
		int32 iPatY = ((uint32)iPosY+dwOffsetY)%patheight;				assert(iPatY>=0 && iPatY<patheight);

		for (int iPosX = 0; iPosX < width; iPosX++)
		{
			// Calculate the array index
			pos = iPosX + iPosY*width;

			int32 iPatX = ((uint32)iPosX+dwOffsetX)%patwidth;				assert(iPatX>=0 && iPatX<patwidth);

			uint32 cSrc = pat[iPatX+iPatY*patwidth];

			float SrcR = (cSrc&0x0ff);					// RGB flipped
			float SrcG = (cSrc&0x0ff00)>>8;
			float SrcB = (cSrc&0x0ff0000)>>16;

			uint32 r=SrcR, g=SrcG, b=SrcB;

			src[pos] = (r<<16) | (g<<8) | b;
		}
	}
}

