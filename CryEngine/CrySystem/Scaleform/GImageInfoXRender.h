/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A wrapper around Scaleform's GImageInfo interface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _GIMAGEINFO_XRENDER_H_
#define _GIMAGEINFO_XRENDER_H_

#pragma once


#ifndef EXCLUDE_SCALEFORM_SDK


#include <GImageInfo.h>


class GTextureXRender;


class GImageInfoXRender : public GImageInfoBase
{
	// GImageInfoBase interface
public:
	virtual UInt GetWidth() const;
	virtual UInt GetHeight() const;
	virtual GTexture*	GetTexture(GRenderer* pRenderer);

public:
	GImageInfoXRender(uint32 targetWidth, uint32 targetHeight);
	virtual ~GImageInfoXRender();

	void SetTexture(GTexture* pTex);

private:
	uint32 m_targetWidth;
	uint32 m_targetHeight;
	GTexture* m_pTex;
};


#endif // #ifndef EXCLUDE_SCALEFORM_SDK


#endif // #ifndef _GIMAGEINFO_XRENDER_H_