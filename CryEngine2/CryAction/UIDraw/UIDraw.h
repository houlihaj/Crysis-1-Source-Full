/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: UI draw functions

-------------------------------------------------------------------------
History:
- 07:11:2005: Created by Julien Darre

*************************************************************************/
#ifndef __UIDRAW_H__
#define __UIDRAW_H__

//-----------------------------------------------------------------------------------------------------

#include "IUIDraw.h"

//-----------------------------------------------------------------------------------------------------

class CUIDraw : public IUIDraw
{
public:

	// IUIDraw

	void Release();

	void PreRender();
	void PostRender();

	uint GetColorARGB(uint8 ucAlpha,uint8 ucRed,uint8 ucGreen,uint8 ucBlue);

	int CreateTexture(const char *strName);

	void GetTextureSize(int iTextureID,float &rfSizeX,float &rfSizeY);

	void GetMemoryStatistics(ICrySizer * s);

//	void DrawTriangle(float fX0,float fY0,float fX1,float fY1,float fX2,float fY2,uint uiColor);

	void DrawQuad(float fX,
								float fY,
								float fSizeX,
								float fSizeY,
								uint uiDiffuse=0,
								uint uiDiffuseTL=0,uint uiDiffuseTR=0,uint uiDiffuseDL=0,uint uiDiffuseDR=0,
								int iTextureID=0,
								float fUTexCoordsTL=0.0f,float fVTexCoordsTL=0.0f,
								float fUTexCoordsTR=1.0f,float fVTexCoordsTR=0.0f,
								float fUTexCoordsDL=0.0f,float fVTexCoordsDL=1.0f,
								float fUTexCoordsDR=1.0f,float fVTexCoordsDR=1.0f,
								bool bUse169=true);


	void DrawImage(int iTextureID,float fX,
																float fY,
																float fSizeX,
																float fSizeY,
																float fAngleInDegrees,
																float fRed,
																float fGreen,
																float fBlue,
																float fAlpha,
																float fS0=0.0f,
																float fT0=0.0f,
																float fS1=1.0f,
																float fT1=1.0f);

	void DrawImageCentered(int iTextureID,float fX,
																				float fY,
																				float fSizeX,
																				float fSizeY,
																				float fAngleInDegrees,
																				float fRed,
																				float fGreen,
																				float fBlue,
																				float fAlpha);

	void DrawText(IFFont *pFont,
								float fX,
								float fY,
								float fSizeX,
								float fSizeY,
								const char *strText,
								float fAlpha,
								float fRed,
								float fGreen,
								float fBlue,
								EUIDRAWHORIZONTAL	eUIDrawHorizontalDocking,
								EUIDRAWVERTICAL		eUIDrawVerticalDocking,
								EUIDRAWHORIZONTAL	eUIDrawHorizontal,
								EUIDRAWVERTICAL		eUIDrawVertical);


	void GetTextDim(IFFont *pFont,
									float *fWidth,
									float *fHeight,
									float fSizeX,
									float fSizeY,
									const char *strText);

	void DrawTextW(	IFFont *pFont,
									float fX,
									float fY,
									float fSizeX,
									float fSizeY,
									const wchar_t *strText,
									float fAlpha,
									float fRed,
									float fGreen,
									float fBlue,
									EUIDRAWHORIZONTAL	eUIDrawHorizontalDocking,
									EUIDRAWVERTICAL		eUIDrawVerticalDocking,
									EUIDRAWHORIZONTAL	eUIDrawHorizontal,
									EUIDRAWVERTICAL		eUIDrawVertical);

	void DrawWrappedTextW(	IFFont *pFont,
		float fX,
		float fY,
		float fMaxWidth,
		float fSizeX,
		float fSizeY,
		const wchar_t *strText,
		float fAlpha,
		float fRed,
		float fGreen,
		float fBlue,
		EUIDRAWHORIZONTAL	eUIDrawHorizontalDocking,
		EUIDRAWVERTICAL		eUIDrawVerticalDocking,
		EUIDRAWHORIZONTAL	eUIDrawHorizontal,
		EUIDRAWVERTICAL		eUIDrawVertical);

	void GetTextDimW(	IFFont *pFont,
										float *fWidth,
										float *fHeight,
										float fSizeX,
										float fSizeY,
										const wchar_t *strText);

	void GetWrappedTextDimW(	IFFont *pFont,
														float *fWidth,
														float *fHeight,
														float fMaxWidth,
														float fSizeX,
														float fSizeY,
														const wchar_t *strText);

	// ~IUIDraw

		CUIDraw();
	~	CUIDraw();

protected:
	void InternalDrawTextW(	IFFont *pFont,
													float fX,
													float fY,
													float fMaxWidth,
													float fSizeX,
													float fSizeY,
													const wchar_t *strText,
													float fAlpha,
													float fRed,
													float fGreen,
													float fBlue,
													EUIDRAWHORIZONTAL	eUIDrawHorizontalDocking,
													EUIDRAWVERTICAL		eUIDrawVerticalDocking,
													EUIDRAWHORIZONTAL	eUIDrawHorizontal,
													EUIDRAWVERTICAL		eUIDrawVertical);

	void InternalGetTextDimW(	IFFont *pFont,
														float *fWidth,
														float *fHeight,
														float fMaxWidth,
														float fSizeX,
														float fSizeY,
														const wchar_t *strText);

	typedef std::map<int,ITexture *> TTexturesMap;
	TTexturesMap m_texturesMap;

	IRenderer *m_pRenderer;
};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------