#include "StdAfx.h"


#ifndef EXCLUDE_SCALEFORM_SDK


#include "GTextureXRender.h"
#include "GRendererXRender.h"
#include "FlashPlayerInstance.h"
#include "SharedStates.h"
#include <StringUtils.h>
#include <GImage.h>

#include <IRenderer.h>
#include <ISystem.h>
#include <ILog.h>
#include <vector>


unsigned int GTextureXRender::ms_textureMemoryUsed(0);


GTextureXRender::GTextureXRender(GRendererXRender* pRendererXRender)
: m_texID(-1)
, m_width(0)
, m_height(0)
, m_userData(0)
, m_pRendererXRender(pRendererXRender)
{
	assert(m_pRendererXRender);
}


GTextureXRender::~GTextureXRender()
{
	if(m_texID > 0)
	{
		IRenderer* pRenderer(m_pRendererXRender->GetXRender());
		ITexture* pTexture(pRenderer->EF_GetTextureByID(m_texID));
		assert(pTexture);
		if (!pTexture->IsShared())
			ms_textureMemoryUsed -= pTexture->GetDeviceDataSize();
		pRenderer->RemoveTexture(m_texID);
	}
}


Bool GTextureXRender::InitTexture(GImageBase* pIm, int targetWidth, int targetHeight)
{
	assert(m_texID == -1);
	if (InitTextureInternal(pIm))
	{
		IRenderer* pRenderer(m_pRendererXRender->GetXRender());
		ITexture* pTexture(pRenderer->EF_GetTextureByID(m_texID));
		assert(pTexture && !pTexture->IsShared());
		m_width = targetWidth != 0 ? targetWidth : pTexture->GetWidth();
		m_height = targetHeight != 0 ? targetHeight : pTexture->GetHeight();
		ms_textureMemoryUsed += pTexture->GetDeviceDataSize();
	}
	return m_texID > 0;
}


static void VerifyTextureSource(const char* pFileName)
{
	ICryPak* pPak(gEnv->pCryPak);
	FILE* f(pPak->FOpen(pFileName, "rb"));
	if (f)
	{
		const char* pOrigPakPathName(pPak->GetFileArchivePath(f));
		if (pOrigPakPathName && pOrigPakPathName[0] != '\0')
		{
			const char* pOrigPakFileName(CryStringUtils::FindFileNameInPath(pOrigPakPathName));
			if (CryStringUtils::stristr(pOrigPakFileName, "lowspec.pak"))
			{
				switch (CFlashPlayer::GetWarningLevel())
				{
				case 1:
					CryGFxLog::GetAccess().Log("Trying to load exported flash texture from lowspec.pak will result in broken display! [%s, %s]", pFileName, pOrigPakPathName);
					break;
				case 2:
					{
						const char* pFlashContext(CryGFxLog::GetAccess().GetContext());
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "!Trying to load exported flash texture from lowspec.pak will result in broken display! "
							"[%s, %s] [%s]", pFileName, pOrigPakPathName, pFlashContext ? pFlashContext : "#!NO_CONTEXT!#");
					}
					break;
				default:
					break;
				}
			}				
		}
		pPak->FClose(f);
	}
}


Bool GTextureXRender::InitTextureFromFile(const char* pFilename, int targetWidth, int targetHeight)
{
	assert(m_texID == -1);
	VerifyTextureSource(pFilename);
	IRenderer* pRenderer(m_pRendererXRender->GetXRender());
	ITexture* pTexture(pRenderer->EF_LoadTexture(pFilename, FT_DONT_STREAM | FT_DONT_RESIZE | FT_NOMIPS, eTT_2D));
	if (pTexture)
	{
		m_texID = pTexture->GetTextureID();
		m_width = targetWidth != 0 ? targetWidth : pTexture->GetWidth();
		m_height = targetHeight != 0 ? targetHeight : pTexture->GetHeight();
		if (!pTexture->IsShared())
			ms_textureMemoryUsed += pTexture->GetDeviceDataSize();
	}
	return m_texID > 0;
}


static inline ETEX_Format MapImageType(const GImageBase::ImageFormat& fmt)
{
	switch (fmt)
	{
	case GImageBase::Image_ARGB_8888: 
		return eTF_A8R8G8B8;
	case GImageBase::Image_RGB_888:
		return eTF_R8G8B8;
	case GImageBase::Image_L_8:
		return eTF_L8;
	case GImageBase::Image_A_8:
		return eTF_A8;
	case GImageBase::Image_DXT1:
		return eTF_DXT1;
	case GImageBase::Image_DXT3:
		return eTF_DXT3;
	case GImageBase::Image_DXT5:
		return eTF_DXT5;
	default:
		return eTF_Unknown;
	}
}


bool GTextureXRender::InitTextureInternal(const GImageBase* pIm)
{
	ETEX_Format texFmt(MapImageType(pIm->Format));
	if (texFmt != eTF_Unknown)
	{
		return InitTextureInternal(texFmt, pIm->Width, pIm->Height, pIm->Pitch, pIm->pData);
	}
	else
	{
		gEnv->pLog->LogWarning("<Scaleform> GTextureXRender::InitTextureInternal( ... ) -- Attempted to load texture with unsupported texture format!\n");
		return false;
	}
}


bool GTextureXRender::InitTextureInternal(ETEX_Format texFmt, int32 width, int32 height, int32 pitch, uint8* pData)
{
	IRenderer* pRenderer(m_pRendererXRender->GetXRender());

	bool rgba((pRenderer->GetFeatures() & RFT_RGBA) != 0 || pRenderer->GetRenderType() == eRT_DX10);
	bool swapRB(!rgba && texFmt == eTF_A8R8G8B8);
	ETEX_Format texFmtOrig(texFmt);

	// expand RGB to RGBA if necessary
	std::vector<uint8> m_expandRGB8;
	if (texFmt == eTF_R8G8B8)
	{
		m_expandRGB8.reserve(width * height * 4);
		for (uint32 y(0); y<height; ++y)
		{
			uint8* pCol(&pData[pitch * y]);
			if (rgba)
			{
				for (uint32 x(0); x<width; ++x, pCol += 3)
				{							
					m_expandRGB8.push_back(pCol[0]);
					m_expandRGB8.push_back(pCol[1]);
					m_expandRGB8.push_back(pCol[2]);
					m_expandRGB8.push_back(255);
				}
			}
			else
			{
				for (uint32 x(0); x<width; ++x, pCol += 3)
				{							
					m_expandRGB8.push_back(pCol[2]);
					m_expandRGB8.push_back(pCol[1]);
					m_expandRGB8.push_back(pCol[0]);
					m_expandRGB8.push_back(255);
				}
			}
		}
		
		pData = &m_expandRGB8[0];
		texFmt = eTF_X8R8G8B8;
		swapRB = false;
	}

	if (swapRB)
		SwapRB(pData, width, height, pitch);

	// Create texture...
	// Note that mip-maps should be generated for font textures (A8/L8) as 
	// Scaleform generates font textures only once and relies on mip-mapping
	// to implement various font sizes.
	m_texID = pRenderer->SF_CreateTexture(width, height, (texFmt == eTF_A8 || texFmt == eTF_L8) ? 0 : 1, pData, texFmt);

	if (m_texID > 0)
	{
		m_width = width;
		m_height = height;
	}
	else
	{
		static const char* s_fomats[] =
		{
			"Unknown",
			"A8R8G8B8",
			"R8G8B8->X8R8G8B8",
			"L8",
			"A8",
			"DXT1",
			"DXT3",
			"DXT5",
			0
		};

		int fmtIdx(0);
		switch (texFmtOrig)
		{
		case eTF_A8R8G8B8:
				fmtIdx = 1;
				break;
		case eTF_X8R8G8B8:
				fmtIdx = 2;
				break;
		case eTF_L8:
				fmtIdx = 3;
				break;
		case eTF_A8:
				fmtIdx = 4;
				break;
		case eTF_DXT1:
				fmtIdx = 5;
				break;
		case eTF_DXT3:
				fmtIdx = 6;
				break;
		case eTF_DXT5:
				fmtIdx = 7;
				break;
		}

		gEnv->pLog->LogWarning("<Scaleform> GTextureXRender::InitTextureInternal( ... ) "
			"-- Texture creation failed (%dx%d, %s)\n", width, height, s_fomats[fmtIdx]);
	}

	if (swapRB)
		SwapRB(pData, width, height, pitch);

	return m_texID > 0;
}


Bool GTextureXRender::InitTexture(int width, int height, GImage::ImageFormat format, int mipmaps, int targetWidth, int targetHeight)
{
	IRenderer* pRenderer(m_pRendererXRender->GetXRender());
	assert(m_texID == -1);
	m_texID = pRenderer->SF_CreateTexture(width, height, mipmaps + 1, 0, MapImageType(format));
	if (m_texID > 0)
	{
		ITexture* pTexture(pRenderer->EF_GetTextureByID(m_texID));
		assert(pTexture && !pTexture->IsShared());
		m_width = targetWidth != 0 ? targetWidth : pTexture->GetWidth();
		m_height = targetHeight != 0 ? targetHeight : pTexture->GetHeight();
		ms_textureMemoryUsed += pTexture->GetDeviceDataSize();
	}
	return m_texID > 0;
}


void GTextureXRender::Update(int level, int n, const UpdateRect* pRects, const GImageBase* pIm)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	assert(m_texID > 0);
	if (!pRects || !n || !pIm || m_texID <= 0)
		return;
	
	IRenderer::SUpdateRect* pSrcRects((IRenderer::SUpdateRect*) alloca(n * sizeof(IRenderer::SUpdateRect)));
	if (pSrcRects)
	{
		for (int i(0); i<n; ++i)
			pSrcRects[i].Set(pRects[i].dest.x, pRects[i].dest.y, pRects[i].src.Left, pRects[i].src.Top, pRects[i].src.Width(), pRects[i].src.Height());

		IRenderer* pRenderer(m_pRendererXRender->GetXRender());
		pRenderer->SF_UpdateTexture(m_texID, level, n, pSrcRects, pIm->pData, pIm->Pitch, MapImageType(pIm->Format));
	}
}


GRenderer* GTextureXRender::GetRenderer() const
{
	return m_pRendererXRender;
}


Bool GTextureXRender::IsDataValid() const
{
	return m_texID > 0;
}


Handle GTextureXRender::GetUserData() const
{
	return m_userData;
}


void GTextureXRender::SetUserData(Handle hData)
{
	m_userData = hData;
}


void GTextureXRender::AddChangeHandler(ChangeHandler* pHandler)
{
}


void GTextureXRender::RemoveChangeHandler(ChangeHandler* pHandler)
{
}


int32 GTextureXRender::GetID() const
{
	return m_texID;
}


int32 GTextureXRender::GetWidth() const
{
	return m_width;
}


int32 GTextureXRender::GetHeight() const
{
	return m_height;
}


uint32 GTextureXRender::GetTextureMemoryUsed()
{
	return ms_textureMemoryUsed;
}


void GTextureXRender::SwapRB(uint8* pImageData, uint32 width, uint32 height, uint32 pitch)
{
	for (uint32 y(0); y<height; ++y)
	{
		uint8* pCol(&pImageData[pitch * y]);
		for (uint32 x(0); x<width; ++x, pCol += 4)
		{							
			uint8 s(pCol[0]); 
			pCol[0] = pCol[2]; 
			pCol[2] = s;
		}
	}
}


#endif // #ifndef EXCLUDE_SCALEFORM_SDK