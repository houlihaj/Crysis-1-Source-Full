#include "StdAfx.h"


#ifndef EXCLUDE_SCALEFORM_SDK


#include "SharedResources.h"
#include "SharedStates.h"
#include "FlashPlayerInstance.h"


//////////////////////////////////////////////////////////////////////////
// CSharedFlashPlayerResources

CSharedFlashPlayerResources::CSharedFlashPlayerResources()
: m_pLoader(0)
, m_pRenderer(0)
{
	GMemory::SetBlockAllocator(&m_cryMemBlockAllocator);
}


CSharedFlashPlayerResources::~CSharedFlashPlayerResources()
{
	//assert(!m_pLoader && !m_pRenderer);
}


CSharedFlashPlayerResources& CSharedFlashPlayerResources::GetAccess()
{
	static CSharedFlashPlayerResources s_inst;
	return s_inst;
}


GFxLoader2* CSharedFlashPlayerResources::GetLoader(bool getRawInterface)
{
	if (!getRawInterface)
	{
		if (!m_pLoader)
			m_pLoader = new GFxLoader2(this);
		else
			m_pLoader->AddRef();
	}
	return m_pLoader;
}


GRendererXRender* CSharedFlashPlayerResources::GetRenderer(bool getRawInterface)
{
	if (!getRawInterface)
	{
		if (!m_pRenderer)
			m_pRenderer = new GRendererXRender(this);
		else
			m_pRenderer->AddRef();
	}
	return m_pRenderer;
}


void CSharedFlashPlayerResources::OnDestroy(GFxLoader2*)
{
	m_pLoader = 0;
}


void CSharedFlashPlayerResources::OnDestroy(GRendererXRender*)
{
	m_pRenderer = 0;
}


//////////////////////////////////////////////////////////////////////////
// GFxLoader2

GFxLoader2::GFxLoader2(IObserverLifeTime_GFxLoader* pObserver)
: m_refCount(1)
, m_pObserver(pObserver)
{
	// set callbacks
	SetLog(&CryGFxLog::GetAccess());
	SetFileOpener(&CryGFxFileOpener::GetAccess());
	SetURLBuilder(&CryGFxURLBuilder::GetAccess());
	SetImageCreator(&CryGFxImageCreator::GetAccess());
	SetImageLoader(&CryGFxImageLoader::GetAccess());	

	// enable dynamic font cache
	SetupDynamicFontCache();

	// set parser verbosity
	UpdateParserVerbosity();
	SetParseControl(&m_parserVerbosity);
}


GFxLoader2::~GFxLoader2()
{	
	if (m_pObserver)
		m_pObserver->OnDestroy(this);
}


void GFxLoader2::AddRef()
{
	++m_refCount;
}


void GFxLoader2::Release()
{
	if (--m_refCount <= 0)
		delete this;
}


void GFxLoader2::UpdateParserVerbosity()
{
	m_parserVerbosity.SetParseFlags((CFlashPlayer::GetLogOptions() & CFlashPlayer::LO_LOADING) ? 
		GFxParseControl::VerboseParseAll : GFxParseControl::VerboseParseNone);
}


void GFxLoader2::SetupDynamicFontCache()
{
	SetFontPackParams(0);
	
	//SetFontCacheManager(&CryGFxFontCacheManager::GetAccess());

	GFxFontCacheManager* pFontCacheMan(GetFontCacheManager());
	pFontCacheMan->EnableDynamicCache(true);
	pFontCacheMan->SetMaxRasterScale(1.25f);

	GFxFontCacheManager::TextureConfig cfg;
	cfg.TextureWidth = 1024;
	cfg.TextureHeight = 1024;
	cfg.MaxNumTextures = 1;
	cfg.MaxSlotHeight = 48;
	cfg.SlotPadding = 2;
	cfg.TexUpdWidth = 256;
	cfg.TexUpdHeight = 512;
	pFontCacheMan->SetTextureConfig(cfg);
}


#endif // #ifndef EXCLUDE_SCALEFORM_SDK