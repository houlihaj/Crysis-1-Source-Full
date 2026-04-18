/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A wrapper around Scaleform's GTexture interface to build textures via CryEngine's ITexture interface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _GTEXTURE_XRENDER_H_
#define _GTEXTURE_XRENDER_H_

#pragma once


#ifndef EXCLUDE_SCALEFORM_SDK

#include <platform.h>
#include <IRenderer.h>
#include <IShader.h>
#include <GRenderer.h>


class GRendererXRender;
class GImageBase;
class ITexture;


class GTextureXRender : public GTexture
{
	// GTexture interface
public: 
	virtual Bool InitTexture(GImageBase* pIm, int targetWidth = 0, int targetHeight = 0);
	virtual Bool InitTextureFromFile(const char* pFilename, int targetWidth = 0, int targetHeight = 0); 

	virtual Bool InitTexture(int width, int height, GImage::ImageFormat format, int mipmaps, int targetWidth = 0, int targetHeight = 0);
	virtual void Update(int level, int n, const UpdateRect* pRects, const GImageBase* pIm);

	virtual GRenderer* GetRenderer() const;
	virtual	Bool IsDataValid() const;

	virtual Handle GetUserData() const;
	virtual void SetUserData(Handle hData);

	virtual void AddChangeHandler(ChangeHandler* pHandler);
	virtual void RemoveChangeHandler(ChangeHandler* pHandler);

public:
	GTextureXRender(GRendererXRender* pRendererXRender);
	virtual ~GTextureXRender();
	
	int32 GetID() const;
	int32 GetWidth() const;
	int32 GetHeight() const;

	static uint32 GetTextureMemoryUsed();

private:
	bool InitTextureInternal(const GImageBase* pIm);
	bool InitTextureInternal(ETEX_Format texFmt, int32 width, int32 height, int32 pitch, uint8* pData);
	void SwapRB(uint8* pImageData, uint32 width, uint32 height, uint32 pitch);

private:
	static uint32 ms_textureMemoryUsed;

private:
	int32 m_texID;
	int32 m_width;
	int32 m_height;
	Handle m_userData;
	GRendererXRender* m_pRendererXRender;
};


#endif // #ifndef EXCLUDE_SCALEFORM_SDK


#endif // #ifndef _GTEXTURE_XRENDER_H_