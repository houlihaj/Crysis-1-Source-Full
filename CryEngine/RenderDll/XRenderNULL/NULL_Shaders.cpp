/*=============================================================================
  PS2_Shaders.cpp : PS2 specific effectors/shaders functions implementation.
  Copyright 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"
#include "NULL_Renderer.h"
#include "I3DEngine.h"


#undef THIS_FILE
static char THIS_FILE[] = __FILE__;

//============================================================================

bool CShader::FXSetTechnique(const CCryName& szName)
{
  return true;
}

bool CShader::FXSetPSFloat(const CCryName& NameParam, const Vec4* fParams, int nParams)
{
  return true;
}

bool CShader::FXSetVSFloat(const CCryName& NameParam, const Vec4* fParams, int nParams)
{
  return true;
}

bool CShader::FXBegin(uint *uiPassCount, uint nFlags)
{
  return true;
}

bool CShader::FXBeginPass(uint uiPass)
{
  return true;
}

bool CShader::FXEndPass()
{
  return true;
}

bool CShader::FXEnd()
{
  return true;
}

bool CShader::FXCommit(const uint nFlags)
{
  return true;
}

//===================================================================================

FXShaderCache CHWShader::m_ShaderCache;
FXShaderCacheNames CHWShader::m_ShaderCacheList;

SShaderCache::~SShaderCache()
{
  CHWShader::m_ShaderCache.erase(m_Name);
  SAFE_DELETE(m_pRes[CACHE_USER]);
  SAFE_DELETE(m_pRes[CACHE_READONLY]);
}

SShaderCache *CHWShader::mfInitCache(const char *name, CHWShader *pSH, bool bCheckValid, uint32 CRC32, bool bDontUseUserFolder, bool bReadOnly)
{
  return NULL;
}

bool CHWShader::mfOptimiseCacheFile(SShaderCache *pCache)
{
  return true;
}

bool CHWShader::SetCurrentShaderCombinations(const char *szCombinations)
{
  bool bRes = true;
  return bRes;
}

const char *CHWShader::GetCurrentShaderCombinations(void)
{
  return "";
}

void CHWShader::mfFlushPendedShaders()
{
}

void CHWShader::mfBeginFrame(int nMaxToFlush)
{
}

void SRenderShaderResources::UpdateConstants(IShader *pSH)
{
}

void SRenderShaderResources::CloneConstants(const IRenderShaderResources* pSrc)
{
}

void SRenderShaderResources::ReleaseConstants()
{
}
