#include "StdAfx.h"


#ifndef EXCLUDE_SCALEFORM_SDK


#include "SharedStates.h"

#include "../System.h"
#include "FlashPlayerInstance.h"
#include "GFileCryPak.h"
#include "GImageInfoXRender.h"
#include "GTextureXRender.h"
#include "GImage.h"

#include <IConsole.h>
#include <StringUtils.h>
#include <CryPath.h>
#include <GFxImageResource.h>


//////////////////////////////////////////////////////////////////////////
// CryGFxFileOpener

CryGFxFileOpener::CryGFxFileOpener()
{
}


CryGFxFileOpener& CryGFxFileOpener::GetAccess()
{
	static CryGFxFileOpener s_inst;
	return s_inst;
}


CryGFxFileOpener::~CryGFxFileOpener()
{
}


GFile* CryGFxFileOpener::OpenFile(const char* pFilePath)
{
	// delegate all file I/O to ICryPak by returning a GFileCryPak object
	return new GFileCryPak(pFilePath);
}


//////////////////////////////////////////////////////////////////////////
// CryGFxURLBuilder

CryGFxURLBuilder::CryGFxURLBuilder()
{
}


CryGFxURLBuilder& CryGFxURLBuilder::GetAccess()
{
	static CryGFxURLBuilder s_inst;
	return s_inst;
}


CryGFxURLBuilder::~CryGFxURLBuilder()
{
}

void CryGFxURLBuilder::BuildURL(GFxString* pPath, const LocationInfo& loc)
{
	const char* pOrigFilePath(loc.FileName.ToCStr());

	// if pOrigFilePath ends with "_locfont.[swf|gfx]", we search for the font in Languages
	const char locFontPostFixSwf[] = "_locfont.swf";
	const char locFontPostFixGfx[] = "_locfont.gfx";
	assert(sizeof(locFontPostFixSwf) == sizeof(locFontPostFixGfx)); // both postfixes must be of same length
	const size_t locFontPostFixLen(sizeof(locFontPostFixSwf) - 1);

	size_t filePathLength(strlen(pOrigFilePath));
	if (filePathLength > locFontPostFixLen)
	{
		size_t offset(filePathLength - locFontPostFixLen);
		if (!strnicmp(locFontPostFixSwf, pOrigFilePath + offset, locFontPostFixLen) || !strnicmp(locFontPostFixGfx, pOrigFilePath + offset, locFontPostFixLen))
		{
			*pPath = "Languages/";
			*pPath += PathUtil::GetFile(pOrigFilePath);
			return;
		}
	}

	*pPath = loc.ParentPath + loc.FileName;
}


//////////////////////////////////////////////////////////////////////////
// CryGFxTranslator

CryGFxTranslator::CryGFxTranslator()
: m_pILocMan(0)
, m_localizedString()
{
	m_pILocMan = gEnv->pSystem->GetLocalizationManager();
}


CryGFxTranslator& CryGFxTranslator::GetAccess()
{
	static CryGFxTranslator s_inst;
	return s_inst;
}


CryGFxTranslator::~CryGFxTranslator()
{
}


UInt CryGFxTranslator::GetCaps() const
{
	return Cap_StripTrailingNewLines;
}


Bool CryGFxTranslator::Translate(GFxWStringBuffer* pBuffer, const wchar_t* pKey)
{
	if (!m_pILocMan || !pKey || !pBuffer)
		return false;

	if (*pKey != L'@') // this is not a localizable label
		return false;

	// Plain, quick and simple Unicode to UTF8 conversion of the key for Latin languages (our keys will always be in English!)
	char keyASCII[2048]; assert(wcslen(pKey) < sizeof(keyASCII));
	size_t idx(0);
	while (pKey[idx] /*&& pKey[idx] != L'\n'*/ && idx < sizeof(keyASCII) - 1)
	{
		keyASCII[idx] = (char) (pKey[idx] & 0xFF);
		++idx;
	}
	keyASCII[idx] = '\0';
	++idx;

	//m_localizedString = L"";
	if (m_pILocMan->LocalizeLabel(keyASCII, m_localizedString))
	{
		*pBuffer = m_localizedString.c_str();
		return true;
	}
	else
		return false;
}


//////////////////////////////////////////////////////////////////////////
// CryGFxLog

#if defined(WIN32) || defined(WIN64)
ICVar* CryGFxLog::CV_sys_flash_debuglog(0);
int CryGFxLog::ms_sys_flash_debuglog(0);
#endif


CryGFxLog::CryGFxLog()
: m_pLog(0)
, m_pCurFlashContext(0)
{
	m_pLog = gEnv->pLog;

#if defined(WIN32) || defined(WIN64)
	if (!CV_sys_flash_debuglog)
		CV_sys_flash_debuglog = gEnv->pConsole->Register("sys_flash_debuglog", &ms_sys_flash_debuglog, 0);
#endif
}


CryGFxLog& CryGFxLog::GetAccess()
{
	static CryGFxLog s_inst;
	return s_inst;
}


CryGFxLog::~CryGFxLog()
{
}


void CryGFxLog::LogMessageVarg(LogMessageType messageType, const char* pFmt, va_list argList)
{
	char logBuf[1024];
	int w(_vsnprintf(logBuf, sizeof(logBuf), pFmt, argList));	

	w = (w < 1) ? sizeof(logBuf) : w;
	if (logBuf[w-1] == '\n')
		logBuf[w-1] = '\0';

	char reFmt[1024];
	_snprintf(reFmt, sizeof(reFmt), "<Flash> %s [%s]\n", logBuf, m_pCurFlashContext ? m_pCurFlashContext : "#!NO_CONTEXT!#");

#if defined(WIN32) || defined(WIN64)
	if (ms_sys_flash_debuglog)
		OutputDebugString(reFmt);
#endif

	switch(messageType & (~Log_Channel_Mask))
	{
	case Log_MessageType_Error:
		{
			m_pLog->LogError(reFmt);
			break;
		}
	case Log_MessageType_Warning:
		{
			m_pLog->LogWarning(reFmt);
			break;
		}
	case Log_MessageType_Message:
	default:
		{
			m_pLog->Log(reFmt);
			break;
		}
	}
}


void CryGFxLog::SetContext(const char* pFlashContext)
{
	m_pCurFlashContext = pFlashContext;
}


const char* CryGFxLog::GetContext() const
{
	return m_pCurFlashContext;
}


void CryGFxLog::Log(const char* pFmt, ...)
{
	va_list args;
	va_start(args, pFmt);
	LogMessageVarg(GFxLog::Log_Warning, pFmt, args);
	va_end(args);
}

//////////////////////////////////////////////////////////////////////////
// CryGFxFSCommandHandler

CryGFxFSCommandHandler::CryGFxFSCommandHandler()
{
}


CryGFxFSCommandHandler& CryGFxFSCommandHandler::GetAccess()
{
	static CryGFxFSCommandHandler s_inst;
	return s_inst;
}


CryGFxFSCommandHandler::~CryGFxFSCommandHandler()
{
}


void CryGFxFSCommandHandler::Callback(GFxMovieView* pMovieView, const char* pCommand, const char* pArgs)
{
	// get flash player instance this action script command belongs to and delegate it to client
	CFlashPlayer* pFlashPlayer(static_cast<CFlashPlayer*>(pMovieView->GetUserData()));
	if (pFlashPlayer)
		pFlashPlayer->DelegateFSCommandCallback(pCommand, pArgs);
}


//////////////////////////////////////////////////////////////////////////
// CryGFxUserEventHandler

CryGFxExternalInterface::CryGFxExternalInterface()
{
}


CryGFxExternalInterface& CryGFxExternalInterface::GetAccess()
{
	static CryGFxExternalInterface s_inst;
	return s_inst;
}


CryGFxExternalInterface::~CryGFxExternalInterface()
{
}


void CryGFxExternalInterface::Callback(GFxMovieView* pMovieView, const char* pMethodName, const GFxValue* pArgs, UInt numArgs)
{
	CFlashPlayer* pFlashPlayer(static_cast<CFlashPlayer*>(pMovieView->GetUserData()));
	if (pFlashPlayer)
		pFlashPlayer->DelegateExternalInterfaceCallback(pMethodName, pArgs, numArgs);
	else
		pMovieView->SetExternalInterfaceRetVal(GFxValue(GFxValue::VT_Undefined));
}


//////////////////////////////////////////////////////////////////////////
// CryGFxUserEventHandler

CryGFxUserEventHandler::CryGFxUserEventHandler()
{
}


CryGFxUserEventHandler& CryGFxUserEventHandler::GetAccess()
{
	static CryGFxUserEventHandler s_inst;
	return s_inst;
}


CryGFxUserEventHandler::~CryGFxUserEventHandler()
{
}


void CryGFxUserEventHandler::HandleEvent(class GFxMovieView* pMovieView, const class GFxEvent& event)
{
	// not implemented since it's not needed yet... would need to translate GFx event to independent representation
}


//////////////////////////////////////////////////////////////////////////
// CryGFxImageCreator

CryGFxImageCreator::CryGFxImageCreator()
{
}


CryGFxImageCreator& CryGFxImageCreator::GetAccess()
{
	static CryGFxImageCreator s_inst;
	return s_inst;
}


CryGFxImageCreator::~CryGFxImageCreator()
{
}


GImageInfoBase* CryGFxImageCreator::CreateImage(const GFxImageCreateInfo& info)
{
	GImageInfoXRender* pImageInfo(0);
	switch (info.Type)
	{
	case GFxImageCreateInfo::Input_Image:
	case GFxImageCreateInfo::Input_File:
		{
			GPtr<GTextureXRender> pTex(*CSharedFlashPlayerResources::GetAccess().GetRenderer(true)->CreateTextureX());
			if (pTex)
			{
				switch (info.Type)
				{
				case GFxImageCreateInfo::Input_Image:
					{
						// create image info and texture for internal image
						if (pTex->InitTexture(info.pImage))
							pImageInfo = new GImageInfoXRender(pTex->GetWidth(), pTex->GetHeight());
						break;
					}
				case GFxImageCreateInfo::Input_File:
					{
						// create image info and texture for external image
						if (pTex->InitTextureFromFile(info.pFileInfo->FileName.ToCStr(), info.pFileInfo->TargetWidth, info.pFileInfo->TargetHeight))
							pImageInfo = new GImageInfoXRender(pTex->GetWidth(), pTex->GetHeight());
						break;
					}
				}
			
				if (pImageInfo)
					pImageInfo->SetTexture(pTex);
			}

			break;
		}

	default:
		{
			assert(0);
			break;
		}
	}
	return pImageInfo;
}


//////////////////////////////////////////////////////////////////////////
// CryGFxImageLoader

CryGFxImageLoader::CryGFxImageLoader()
{
}


CryGFxImageLoader& CryGFxImageLoader::GetAccess()
{
	static CryGFxImageLoader s_inst;
	return s_inst;
}


CryGFxImageLoader::~CryGFxImageLoader()
{
}


GImageInfoBase* CryGFxImageLoader::LoadImage(const char *pUrl)
{
	// callback for loadMovie API
	const char* pFilePath(strstr(pUrl, "://"));
	if (!pFilePath)
		return 0;
	pFilePath += 3;

	GImageInfoXRender* pImageInfo(0);
	GPtr<GTextureXRender> pTex(*CSharedFlashPlayerResources::GetAccess().GetRenderer(true)->CreateTextureX());
	if (pTex)
	{
		if (CFlashPlayer::GetFlashLoadMovieHandler())
		{
			IFlashLoadMovieImage* pImage(CFlashPlayer::GetFlashLoadMovieHandler()->LoadMovie(pFilePath));
			if (pImage && pImage->IsValid())
			{
				IFlashLoadMovieImage::EFmt fmt(pImage->GetFormat());
				if (fmt == IFlashLoadMovieImage::eFmt_ARGB_8888 || fmt == IFlashLoadMovieImage::eFmt_RGB_888)
				{
					GImageBase img(fmt == IFlashLoadMovieImage::eFmt_ARGB_8888 ? GImage::Image_ARGB_8888 : GImage::Image_RGB_888, 
						pImage->GetWidth(), pImage->GetHeight(), pImage->GetPitch()); 
					img.pData = (UByte*) pImage->GetPtr();
					if (pTex->InitTexture(&img))
						pImageInfo = new GImageInfoXRender(pTex->GetWidth(), pTex->GetHeight());
				}
			}
			SAFE_RELEASE(pImage);
		}

		if (!pImageInfo)
		{
			if (stricmp(pFilePath,"undefined") != 0 && pTex->InitTextureFromFile(pFilePath))
				pImageInfo = new GImageInfoXRender(pTex->GetWidth(), pTex->GetHeight());
		}

		if (pImageInfo)
			pImageInfo->SetTexture(pTex);
	}

	return pImageInfo;
}


#endif // #ifndef EXCLUDE_SCALEFORM_SDK
