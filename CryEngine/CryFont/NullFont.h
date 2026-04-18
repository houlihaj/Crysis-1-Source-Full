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

#ifndef __NULLFONT_H__
#define __NULLFONT_H__ 1

#include <IFont.h>

#if defined(USE_NULLFONT)

class CNullFont : public IFFont
{
	float m_fScale;
	vector2f m_Size;
	bool m_bRealPixels;
	bool m_bSameSize;

public:
	CNullFont()
		: m_fScale(1.f), m_Size(1.f, 1.f), m_bRealPixels(false), m_bSameSize(false)
	{ }

	void Reset();
	void Release();
	bool Load(const char *szFile, unsigned long nWidth, unsigned long nHeight,
			unsigned long nTTFFlags);
	bool Load(const char *szFile);
	void Free();
	void SetEffect(const char *szEffect);
	void SetClippingRect(float fX, float fY, float fW, float fH);
	void EnableClipping(bool bEnable);
	void SetColor(const ColorF& col, int nPass);
	void UseRealPixels(bool bRealPixels = true);
	bool UsingRealPixels();
	void SetSize(const vector2f &size);
	vector2f &GetSize();
	float GetCharWidth();
	float GetCharHeight();
	void SetSameSize(bool bSameSize);
	bool GetSameSize();
	void SetCharWidthScale(float fScale = 1.0f);
	float GetCharWidthScale();
	void DrawString(float x, float y, const char *szMsg, const bool bASCIIMultiLine = true);
	void DrawString(float x, float y, float z, const char *szMsg, const bool bASCIIMultiLine = true);
	vector2f GetTextSize(const char *szMsg, const bool bASCIIMultiLine = true);
	void DrawStringW(float x, float y, const wchar_t *swStr, const bool bASCIIMultiLine = true);
	void DrawStringW(float x, float y, float z, const wchar_t *swStr, const bool bASCIIMultiLine = true);
	void DrawWrappedStringW(float x, float y, float w, const wchar_t *swStr, const bool bASCIIMultiLine = true);
	vector2f GetTextSizeW(const wchar_t *swStr, const bool bASCIIMultiLine = true);	
	vector2f GetWrappedTextSizeW(const wchar_t *swStr, float w, const bool bASCIIMultiLine = true);
	int GetTextLength(const char *szMsg, const bool bASCIIMultiLine = true);
	int GetTextLengthW(const wchar_t *szwMsg, const bool bASCIIMultiLine = true);
	void GetMemoryUsage(class ICrySizer* pSizer);
};

class CCryNullFont : public ICryFont
{
public:
	void Release();
	struct IFFont *NewFont(const char *pszName);
	struct IFFont *GetFont(const char *pszName);
	void GetMemoryUsage(class ICrySizer* pSizer);
};

#endif // USE_NULLFONT

#endif

// vim:ts=2

