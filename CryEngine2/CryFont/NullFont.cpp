/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	Dummy font implementation (dedicated server)
-------------------------------------------------------------------------
History:
- Jun 30,2006:	Created by Sascha Demetrio

*************************************************************************/

#include "StdAfx.h"
#include "CryFont.h"

#if defined(USE_NULLFONT)
#include "NullFont.h"

void CNullFont::Reset()
{
}

void CNullFont::Release()
{
}

bool CNullFont::Load(
		const char *szFile,
		unsigned long nWidth,
		unsigned long nHeight,
		unsigned long nTTFFlags)
{
	return true;
}

bool CNullFont::Load(const char *szFile)
{
	return true;
}

void CNullFont::Free()
{
}

void CNullFont::SetEffect(const char *szEffect)
{
}

void CNullFont::SetClippingRect(float fX, float fY, float fW, float fH)
{
}

void CNullFont::EnableClipping(bool bEnable)
{
}

void CNullFont::SetColor(const ColorF& col, int nPass)
{
}

void CNullFont::UseRealPixels(bool bRealPixels)
{
	m_bRealPixels = bRealPixels;
}

bool CNullFont::UsingRealPixels()
{
	return m_bRealPixels;
}

void CNullFont::SetSize(const vector2f &size)
{
	m_Size = size;
}

vector2f &CNullFont::GetSize()
{
	return m_Size;
}

float CNullFont::GetCharWidth()
{
	return 0.1f; // Arbitrary value
}

float CNullFont::GetCharHeight()
{
	return 0.1f; // Arbitrary value
}

void CNullFont::SetSameSize(bool bSameSize)
{
	m_bSameSize = bSameSize;
}

bool CNullFont::GetSameSize()
{
	return m_bSameSize;
}

void CNullFont::SetCharWidthScale(float fScale)
{
	m_fScale = fScale;
}

float CNullFont::GetCharWidthScale()
{
	return m_fScale;
}

void CNullFont::DrawString(float x, float y, const char *szMsg, const bool bASCIIMultiLine)
{
}

void CNullFont::DrawString(float x, float y, float z, const char *szMsg, const bool bASCIIMultiLine)
{
}

vector2f CNullFont::GetTextSize(const char *szMsg, const bool bASCIIMultiLine)
{
	return vector2f(1.f, 1.f); // Arbitrary value
}

void CNullFont::DrawStringW(float x, float y, const wchar_t *swStr, const bool bASCIIMultiLine)
{
}

void CNullFont::DrawStringW(float x, float y, float z, const wchar_t *swStr, const bool bASCIIMultiLine)
{
}

void CNullFont::DrawWrappedStringW(float x, float y, float w, const wchar_t *swStr, const bool bASCIIMultiLine)
{
}

vector2f CNullFont::GetTextSizeW(const wchar_t *swStr, const bool bASCIIMultiLine)
{
	return vector2f(1.f, 1.f); // Arbitrary value
}

vector2f CNullFont::GetWrappedTextSizeW(const wchar_t *swStr, float w, const bool bASCIIMultiLine)
{
	return vector2f(1.f, 1.f); // Arbitrary value
}

int CNullFont::GetTextLength(const char *szMsg, const bool bASCIIMultiLine)
{
	return 1;
}

int CNullFont::GetTextLengthW(const wchar_t *szwMsg, const bool bASCIIMultiLine)
{
	return 1;
}

void CNullFont::GetMemoryUsage (class ICrySizer* pSizer)
{
}


void CCryNullFont::Release()
{
}

IFFont *CCryNullFont::NewFont(const char *pszName)
{
	static CNullFont nullFont;
	return &nullFont;
}

IFFont *CCryNullFont::GetFont(const char *pszName)
{
	static CNullFont nullFont;
	return &nullFont;
}

void CCryNullFont::GetMemoryUsage(class ICrySizer* pSizer)
{
}

#endif // USE_NULLFONT

// vim:ts=2

