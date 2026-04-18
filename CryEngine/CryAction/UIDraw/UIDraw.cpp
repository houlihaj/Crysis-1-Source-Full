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
#include "StdAfx.h"
#include "UIDraw.h"

//-----------------------------------------------------------------------------------------------------

CUIDraw::CUIDraw()
{
	m_pRenderer = gEnv->pRenderer;
}

//-----------------------------------------------------------------------------------------------------

CUIDraw::~CUIDraw()
{
	for(TTexturesMap::iterator iter=m_texturesMap.begin(); iter!=m_texturesMap.end(); ++iter)
	{
		SAFE_RELEASE((*iter).second);
	}
}

//-----------------------------------------------------------------------------------------------------

void CUIDraw::Release()
{
	delete this;
}

//-----------------------------------------------------------------------------------------------------

void CUIDraw::PreRender()
{
	m_pRenderer->SelectTMU(1);
	m_pRenderer->EnableTMU(false); 
	m_pRenderer->SelectTMU(0);
 	m_pRenderer->SetCullMode(R_CULL_DISABLE);
	m_pRenderer->Set2DMode(true,m_pRenderer->GetWidth(),m_pRenderer->GetHeight());
	m_pRenderer->SetColorOp(eCO_MODULATE,eCO_MODULATE,DEF_TEXARG0,DEF_TEXARG0);
	m_pRenderer->SetState(GS_BLSRC_SRCALPHA|GS_BLDST_ONEMINUSSRCALPHA|GS_NODEPTHTEST);
	m_pRenderer->EnableTMU(false); 
}

//-----------------------------------------------------------------------------------------------------

void CUIDraw::PostRender()
{
	m_pRenderer->Set2DMode(false,0,0);
}

//-----------------------------------------------------------------------------------------------------

uint CUIDraw::GetColorARGB(uint8 ucAlpha,uint8 ucRed,uint8 ucGreen,uint8 ucBlue)
{
	int iRGB = (m_pRenderer->GetFeatures() & RFT_RGBA);
	return (iRGB ? RGBA8(ucRed,ucGreen,ucBlue,ucAlpha) : RGBA8(ucBlue,ucGreen,ucRed,ucAlpha));
}

//-----------------------------------------------------------------------------------------------------

int CUIDraw::CreateTexture(const char *strName)
{
	for(TTexturesMap::iterator iter=m_texturesMap.begin(); iter!=m_texturesMap.end(); ++iter)
	{
		if(0 == strcmpi((*iter).second->GetName(),strName))
		{
			return (*iter).first;
		}
	}
	ITexture *pTexture = m_pRenderer->EF_LoadTexture(strName,FT_DONT_RELEASE,eTT_2D);
	pTexture->SetClamp(true);
	int iTextureID = pTexture->GetTextureID();
	m_texturesMap.insert(std::make_pair(iTextureID,pTexture));
	return iTextureID;
}

//-----------------------------------------------------------------------------------------------------

void CUIDraw::GetTextureSize(int iTextureID,float &rfSizeX,float &rfSizeY)
{
	TTexturesMap::iterator Iter = m_texturesMap.find(iTextureID);
	if(Iter != m_texturesMap.end())
	{
		ITexture *pTexture = (*Iter).second;
		rfSizeX = (float) pTexture->GetWidth	();
		rfSizeY = (float) pTexture->GetHeight	();
	}
	else
	{
		// Unknow texture !
		assert(0);
		rfSizeX = 0.0f;
		rfSizeY = 0.0f;
	}
}

//-----------------------------------------------------------------------------------------------------

/*void CUIDraw::DrawTriangle(float fX0,float fY0,float fX1,float fY1,float fX2,float fY2,uint uiColor)
{
	struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F aVertices[3];

	aVertices[0].color.dcolor = uiColor;
	aVertices[0].xyz[0] = m_pRenderer->ScaleCoordX(fX0);
	aVertices[0].xyz[1] = m_pRenderer->ScaleCoordY(fY0);
	aVertices[0].xyz[2] = 0.0f;
	aVertices[0].st[0] = 0.0f;
	aVertices[0].st[1] = 0.0f;

	aVertices[1].color.dcolor = uiColor;
	aVertices[1].xyz[0] = m_pRenderer->ScaleCoordX(fX1);
	aVertices[1].xyz[1] = m_pRenderer->ScaleCoordY(fY1);
	aVertices[1].xyz[2] = 0.0f;
	aVertices[1].st[0] = 0.0f;
	aVertices[1].st[1] = 0.0f;

	aVertices[2].color.dcolor = uiColor;
	aVertices[2].xyz[0] = m_pRenderer->ScaleCoordX(fX2);
	aVertices[2].xyz[1] = m_pRenderer->ScaleCoordY(fY2);
	aVertices[2].xyz[2] = 0.0f;
	aVertices[2].st[0] = 0.0f;
	aVertices[2].st[1] = 0.0f;

	ushort ausIndices[] = {0,1,2};

	m_pRenderer->SetWhiteTexture();
	m_pRenderer->DrawDynVB(aVertices,ausIndices,3,sizeof(ausIndices)/sizeof(ausIndices[0]),R_PRIMV_TRIANGLES);
}*/

//-----------------------------------------------------------------------------------------------------

void CUIDraw::DrawQuad(	float fX,
												float fY,
												float fSizeX,
												float fSizeY,
												uint uiDiffuse,
												uint uiDiffuseTL,
												uint uiDiffuseTR,
												uint uiDiffuseDL,
												uint uiDiffuseDR,
												int iTextureID,
												float fUTexCoordsTL,
												float fVTexCoordsTL,
												float fUTexCoordsTR,
												float fVTexCoordsTR,
												float fUTexCoordsDL,
												float fVTexCoordsDL,
												float fUTexCoordsDR,
												float fVTexCoordsDR,
												bool bUse169)
{
	struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F aVertices[4];

	if(bUse169)
	{
		float fWidth43 = m_pRenderer->GetHeight() * 4.0f / 3.0f;
		float fScale = fWidth43 / (float) m_pRenderer->GetWidth();
		float fOffset = (fSizeX - fSizeX * fScale);
		fX += 0.5f * fOffset;
		fSizeX -= fOffset;
	}

	const float fOff = -0.5f;

	aVertices[0].color.dcolor = uiDiffuse ? uiDiffuse : uiDiffuseTL;
	aVertices[0].xyz[0] = m_pRenderer->ScaleCoordX(fX)+fOff;
	aVertices[0].xyz[1] = m_pRenderer->ScaleCoordY(fY)+fOff;
	aVertices[0].xyz[2] = 0.0f;
	aVertices[0].st[0] = fUTexCoordsTL;
	aVertices[0].st[1] = fVTexCoordsTL;

	aVertices[1].color.dcolor = uiDiffuse ? uiDiffuse : uiDiffuseTR;
	aVertices[1].xyz[0] = m_pRenderer->ScaleCoordX(fX+fSizeX)+fOff;
	aVertices[1].xyz[1] = m_pRenderer->ScaleCoordY(fY)+fOff;
	aVertices[1].xyz[2] = 0.0f;
	aVertices[1].st[0] = fUTexCoordsTR;
	aVertices[1].st[1] = fVTexCoordsTR;

	aVertices[2].color.dcolor = uiDiffuse ? uiDiffuse : uiDiffuseDL;
	aVertices[2].xyz[0] = m_pRenderer->ScaleCoordX(fX)+fOff;
	aVertices[2].xyz[1] = m_pRenderer->ScaleCoordY(fY+fSizeY)+fOff;
	aVertices[2].xyz[2] = 0.0f;
	aVertices[2].st[0] = fUTexCoordsDL;
	aVertices[2].st[1] = fVTexCoordsDL;

	aVertices[3].color.dcolor = uiDiffuse ? uiDiffuse : uiDiffuseDR;
	aVertices[3].xyz[0] = m_pRenderer->ScaleCoordX(fX+fSizeX)+fOff;
	aVertices[3].xyz[1] = m_pRenderer->ScaleCoordY(fY+fSizeY)+fOff;
	aVertices[3].xyz[2] = 0.0f;
	aVertices[3].st[0] = fUTexCoordsDR;
	aVertices[3].st[1] = fVTexCoordsDR;

	m_pRenderer->SelectTMU(0);

	if(iTextureID)
	{
		m_pRenderer->EnableTMU(true);  
		m_pRenderer->SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
		m_pRenderer->SetTexture(iTextureID);
	}
	else
	{
		m_pRenderer->EnableTMU(false);
		// m_pRenderer->SetWhiteTexture();
	}

	ushort ausIndices[] = {0,1,2,3};

	m_pRenderer->DrawDynVB(aVertices,ausIndices,4,4,R_PRIMV_TRIANGLE_STRIP);
}

//-----------------------------------------------------------------------------------------------------

void CUIDraw::DrawImage(int iTextureID,	float fX,
																				float fY,
																				float fSizeX,
																				float fSizeY,
																				float fAngleInDegrees,
																				float fRed,
																				float fGreen,
																				float fBlue,
																				float fAlpha,
																				float fS0,
																				float fT0,
																				float fS1,
																				float fT1)
{
	float fWidth43 = m_pRenderer->GetHeight() * 4.0f / 3.0f;
	float fScale = fWidth43 / (float) m_pRenderer->GetWidth();
	float fOffset = (fSizeX - fSizeX * fScale);
	fX += 0.5f * fOffset;
	fSizeX -= fOffset;

	m_pRenderer->Draw2dImage(	fX,
														fY+fSizeY,
														fSizeX,
														-fSizeY,
														iTextureID,
														fS0,fT0,fS1,fT1,
														fAngleInDegrees,
														fRed,
														fGreen,
														fBlue,
														fAlpha);
}

//-----------------------------------------------------------------------------------------------------

void CUIDraw::DrawImageCentered(int iTextureID,	float fX,
																								float fY,
																								float fSizeX,
																								float fSizeY,
																								float fAngleInDegrees,
																								float fRed,
																								float fGreen,
																								float fBlue,
																								float fAlpha)
{
	float fImageX = fX - 0.5f * fSizeX;
	float fImageY = fY - 0.5f * fSizeY;

	DrawImage(iTextureID,fImageX,fImageY,fSizeX,fSizeY,fAngleInDegrees,fRed,fGreen,fBlue,fAlpha);
}

//-----------------------------------------------------------------------------------------------------

void CUIDraw::DrawText(	IFFont *pFont,
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
												EUIDRAWVERTICAL		eUIDrawVertical)
{
	if(NULL == pFont)
	{
		return;
	}

//	fSizeX = m_pRenderer->ScaleCoordX(fSizeX);
//	fSizeY = m_pRenderer->ScaleCoordY(fSizeY);

	// Note: First ScaleCoordY is not a mistake
	if (fSizeX<=0.0f) fSizeX=15.0f;
	if (fSizeY<=0.0f) fSizeY=15.0f;

	fSizeX = m_pRenderer->ScaleCoordY(fSizeX);
	fSizeY = m_pRenderer->ScaleCoordY(fSizeY);

	pFont->UseRealPixels(true);
	pFont->SetSize(vector2f(fSizeX,fSizeY));
	pFont->SetColor(ColorF(fRed,fGreen,fBlue,fAlpha));

	// Note: First ScaleCoordY is not a mistake

	float fTextX = m_pRenderer->ScaleCoordY(fX);
	float fTextY = m_pRenderer->ScaleCoordY(fY);

	if(UIDRAWHORIZONTAL_CENTER == eUIDrawHorizontalDocking)
	{
		fTextX += m_pRenderer->GetWidth() * 0.5f;
	}
	else if(UIDRAWHORIZONTAL_RIGHT == eUIDrawHorizontalDocking)
	{
		fTextX += m_pRenderer->GetWidth();
	}

	if(UIDRAWVERTICAL_CENTER == eUIDrawVerticalDocking)
	{
		fTextY += m_pRenderer->GetHeight() * 0.5f;
	}
	else if(UIDRAWVERTICAL_BOTTOM == eUIDrawVerticalDocking)
	{
		fTextY += m_pRenderer->GetHeight();
	}

	vector2f vDim = pFont->GetTextSize(strText);

	if(UIDRAWHORIZONTAL_CENTER == eUIDrawHorizontal)
	{
		fTextX -= vDim.x * 0.5f;
	}
	else if(UIDRAWHORIZONTAL_RIGHT == eUIDrawHorizontal)
	{
		fTextX -= vDim.x;
	}

	if(UIDRAWVERTICAL_CENTER == eUIDrawVertical)
	{
		fTextY -= vDim.y * 0.5f;
	}
	else if(UIDRAWVERTICAL_BOTTOM == eUIDrawVertical)
	{
		fTextY -= vDim.y;
	}

	pFont->DrawString(fTextX,fTextY,strText);
}

//-----------------------------------------------------------------------------------------------------
void CUIDraw::GetTextDim(	IFFont *pFont,
													float *fWidth,
													float *fHeight,
													float fSizeX,
													float fSizeY,
													const char *strText)
{
	if(NULL == pFont)
	{
		return;
	}

	//	fSizeX = m_pRenderer->ScaleCoordX(fSizeX);
	//	fSizeY = m_pRenderer->ScaleCoordY(fSizeY);

	// Note: First ScaleCoordY is not a mistake
	if (fSizeX<=0.0f) fSizeX=15.0f;
	if (fSizeY<=0.0f) fSizeY=15.0f;

	fSizeX = m_pRenderer->ScaleCoordY(fSizeX);
	fSizeY = m_pRenderer->ScaleCoordY(fSizeY);

	pFont->UseRealPixels(true);
	pFont->SetSize(vector2f(fSizeX,fSizeY));

	vector2f dim=pFont->GetTextSize(strText);
	
	float fScaleBack=1.0f/m_pRenderer->ScaleCoordY(1.0f);
	if (fWidth)
		*fWidth=dim.x*fScaleBack;
	if (fHeight)
		*fHeight=dim.y*fScaleBack;
}


//-----------------------------------------------------------------------------------------------------

void CUIDraw::DrawTextW(IFFont *pFont,
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
												EUIDRAWVERTICAL		eUIDrawVertical)
{
	InternalDrawTextW(pFont, fX, fY, 0.0f,
		fSizeX, fSizeY, 
		strText, 
		fAlpha, fRed, fGreen, fBlue, 
		eUIDrawHorizontalDocking, eUIDrawVerticalDocking, eUIDrawHorizontal, eUIDrawVertical);
}

void CUIDraw::DrawWrappedTextW(	IFFont *pFont,
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
																EUIDRAWVERTICAL		eUIDrawVertical
												)
{
	InternalDrawTextW(pFont, fX, fY, fMaxWidth,
		fSizeX, fSizeY, 
		strText, 
		fAlpha, fRed, fGreen, fBlue, 
		eUIDrawHorizontalDocking, eUIDrawVerticalDocking, eUIDrawHorizontal, eUIDrawVertical);
}


//-----------------------------------------------------------------------------------------------------

void CUIDraw::InternalDrawTextW(IFFont *pFont,
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
																EUIDRAWVERTICAL		eUIDrawVertical
																)
{
	if(NULL == pFont)
	{
		return;
	}

	const bool bWrapText = fMaxWidth > 0.0f;
	if (bWrapText)
		fMaxWidth = m_pRenderer->ScaleCoordX(fMaxWidth);

	//	fSizeX = m_pRenderer->ScaleCoordX(fSizeX);
	//	fSizeY = m_pRenderer->ScaleCoordY(fSizeY);

	// Note: First ScaleCoordY is not a mistake
	if (fSizeX<=0.0f) fSizeX=15.0f;
	if (fSizeY<=0.0f) fSizeY=15.0f;

	fSizeX = m_pRenderer->ScaleCoordY(fSizeX);
	fSizeY = m_pRenderer->ScaleCoordY(fSizeY);

	pFont->UseRealPixels(true);
	pFont->SetSize(vector2f(fSizeX,fSizeY));
	pFont->SetColor(ColorF(fRed,fGreen,fBlue,fAlpha));

	// Note: First ScaleCoordY is not a mistake

	float fTextX = m_pRenderer->ScaleCoordY(fX);
	float fTextY = m_pRenderer->ScaleCoordY(fY);

	if(UIDRAWHORIZONTAL_CENTER == eUIDrawHorizontalDocking)
	{
		fTextX += m_pRenderer->GetWidth() * 0.5f;
	}
	else if(UIDRAWHORIZONTAL_RIGHT == eUIDrawHorizontalDocking)
	{
		fTextX += m_pRenderer->GetWidth();
	}

	if(UIDRAWVERTICAL_CENTER == eUIDrawVerticalDocking)
	{
		fTextY += m_pRenderer->GetHeight() * 0.5f;
	}
	else if(UIDRAWVERTICAL_BOTTOM == eUIDrawVerticalDocking)
	{
		fTextY += m_pRenderer->GetHeight();
	}

	vector2f vDim = bWrapText ? pFont->GetWrappedTextSizeW(strText, fMaxWidth) : pFont->GetTextSizeW(strText);

	if(UIDRAWHORIZONTAL_CENTER == eUIDrawHorizontal)
	{
		fTextX -= vDim.x * 0.5f;
	}
	else if(UIDRAWHORIZONTAL_RIGHT == eUIDrawHorizontal)
	{
		fTextX -= vDim.x;
	}

	if(UIDRAWVERTICAL_CENTER == eUIDrawVertical)
	{
		fTextY -= vDim.y * 0.5f;
	}
	else if(UIDRAWVERTICAL_BOTTOM == eUIDrawVertical)
	{
		fTextY -= vDim.y;
	}

	if (bWrapText)
		pFont->DrawWrappedStringW(fTextX, fTextY, fMaxWidth, strText);
	else
		pFont->DrawStringW(fTextX, fTextY, strText);
}

//-----------------------------------------------------------------------------------------------------

void CUIDraw::InternalGetTextDimW(IFFont *pFont,
													float *fWidth,
													float *fHeight,
													float fMaxWidth,
													float fSizeX,
													float fSizeY,
													const wchar_t *strText)
{
	if(NULL == pFont)
	{
		return;
	}

	const bool bWrapText = fMaxWidth > 0.0f;
	if (bWrapText)
		fMaxWidth = m_pRenderer->ScaleCoordX(fMaxWidth);

	//	fSizeX = m_pRenderer->ScaleCoordX(fSizeX);
	//	fSizeY = m_pRenderer->ScaleCoordY(fSizeY);

	// Note: First ScaleCoordY is not a mistake
	if (fSizeX<=0.0f) fSizeX=15.0f;
	if (fSizeY<=0.0f) fSizeY=15.0f;

	fSizeX = m_pRenderer->ScaleCoordY(fSizeX);
	fSizeY = m_pRenderer->ScaleCoordY(fSizeY);

	pFont->UseRealPixels(true);
	pFont->SetSize(vector2f(fSizeX,fSizeY));

	vector2f dim= bWrapText ? pFont->GetWrappedTextSizeW(strText, fMaxWidth) : pFont->GetTextSizeW(strText);

	float fScaleBack=1.0f/m_pRenderer->ScaleCoordY(1.0f);
	if (fWidth)
		*fWidth=dim.x*fScaleBack;
	if (fHeight)
		*fHeight=dim.y*fScaleBack;
}

//-----------------------------------------------------------------------------------------------------

void CUIDraw::GetTextDimW(IFFont *pFont,
																 float *fWidth,
																 float *fHeight,
																 float fSizeX,
																 float fSizeY,
																 const wchar_t *strText)
{
	InternalGetTextDimW(pFont, fWidth, fHeight, 0.0f, fSizeX, fSizeY, strText);
}

void CUIDraw::GetWrappedTextDimW(IFFont *pFont,
																 float *fWidth,
																 float *fHeight,
																 float fMaxWidth,
																 float fSizeX,
																 float fSizeY,
																 const wchar_t *strText)
{
	InternalGetTextDimW(pFont, fWidth, fHeight, fMaxWidth, fSizeX, fSizeY, strText);
}


//-----------------------------------------------------------------------------------------------------

void CUIDraw::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "UIDraw");
	s->Add(*this);
	s->AddContainer(m_texturesMap);
}

//-----------------------------------------------------------------------------------------------------