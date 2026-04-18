#include "StdAfx.h"


#ifndef EXCLUDE_SCALEFORM_SDK


#include "GImageInfoXRender.h"
#include "GTextureXRender.h"


GImageInfoXRender::GImageInfoXRender(uint32 targetWidth, uint32 targetHeight)
: m_targetWidth(targetWidth)
, m_targetHeight(targetHeight)
, m_pTex(0)
{
}


GImageInfoXRender::~GImageInfoXRender()
{
	//delete m_pTex;
	SAFE_RELEASE(m_pTex);
}


UInt GImageInfoXRender::GetWidth() const
{
	return m_targetWidth;
}


UInt GImageInfoXRender::GetHeight() const
{
	return m_targetHeight;
}


GTexture*	GImageInfoXRender::GetTexture(GRenderer* /*pRenderer*/)
{
	return m_pTex;
}


void GImageInfoXRender::SetTexture(GTexture* pTex)
{	
	//delete m_pTex;
	if (pTex)
		pTex->AddRef();
	if (m_pTex)
		m_pTex->Release();
	m_pTex = pTex;
}


#endif // #ifndef EXCLUDE_SCALEFORM_SDK