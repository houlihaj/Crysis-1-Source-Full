/*=============================================================================
  ShaderCore.cpp : implementation of the Shaders manager.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "I3DEngine.h"
#include "CryHeaders.h"


CShader *CShaderMan::m_DefaultShader;
CShader *CShaderMan::m_shPostEffects;
#ifndef NULL_RENDERER
CShader *CShaderMan::m_ShaderFPEmu;
CShader *CShaderMan::m_ShaderFallback;

CShader *CShaderMan::m_ShaderTreeSprites;
CShader *CShaderMan::m_ShaderShadowBlur;
CShader *CShaderMan::m_ShaderShadowMaskGen;
CShader *CShaderMan::m_ShaderAmbientOcclusion;
CShader *CShaderMan::m_ShaderCloudShadowMaskGen;
CShader *CShaderMan::m_ShaderPostProcess;
CShader *CShaderMan::m_ShaderDebug;
CShader *CShaderMan::m_ShaderLightFlares;
SShaderItem CShaderMan::m_ShaderLightStyles;
CShader *CShaderMan::m_shPostEffectsGame;
CShader *CShaderMan::m_ShaderCommon;
CShader *CShaderMan::m_ShaderOcclTest;
#else
SShaderItem CShaderMan::m_DefaultShaderItem;
#endif
    
TArray<SRenderShaderResources *> CShader::m_ShaderResources_known;
TArray <CLightStyle *> CLightStyle::m_LStyles;

SResourceContainer *CShaderMan::m_pContainer;  // List/Map of objects for shaders resource class

uint64 g_HWSR_MaskBit[HWSR_MAX];

bool gbRgb;

//////////////////////////////////////////////////////////////////////////
// Global shader parser helper pointer.
CShaderParserHelper* g_pShaderParserHelper = 0;

//=================================================================================================

int CShader::GetTexId()
{
  CTexture *tp = (CTexture *)GetBaseTexture(NULL, NULL);
  if (!tp)
    return -1;
  return tp->GetTextureID();
}

#ifdef WIN64
#pragma warning( push )                 //AMD Port
#pragma warning( disable : 4267 )
#endif

int CShader::mfSize()
{
  uint i;

  int nSize = sizeof(CShader);
  nSize += m_NameFile.capacity();
  nSize += m_NameShader.capacity();
  nSize += m_HWTechniques.GetMemoryUsage();
  for (i=0; i<m_HWTechniques.Num(); i++)
  {
    nSize += m_HWTechniques[i]->Size();
  }

  nSize += m_PublicParams.size()*sizeof(SShaderParam);

  return nSize;
}

#ifdef WIN64
#pragma warning( pop )                  //AMD Port
#endif

void CShader::mfFree()
{
  uint i;

  for (i=0; i<m_HWTechniques.Num(); i++)
  {
    SAFE_DELETE(m_HWTechniques[i]);
  }
  m_HWTechniques.Free();

  SAFE_DELETE(m_ShaderGenParams);
  m_PublicParams.clear();
  m_Flags &= ~EF_PARSE_MASK;
  m_Flags2 &= ~(EF2_NODRAW);
  m_nMDV = 0;
}

CShader::~CShader()
{
  if (m_pGenShader && m_pGenShader->m_DerivedShaders)
  {
    int i;
    for (i=0; i<m_pGenShader->m_DerivedShaders->Num(); i++)
    {
      CShader *pSH = (*m_pGenShader->m_DerivedShaders)[i];
      if (pSH == this)
      {
        (*m_pGenShader->m_DerivedShaders)[i] = NULL;
        break;
      }
    }
    assert(i != m_pGenShader->m_DerivedShaders->Num());
  }
  mfFree();

  SAFE_RELEASE(m_pGenShader);
  SAFE_DELETE(m_DerivedShaders);
}

#ifdef WIN64
#pragma warning( push )                 //AMD Port
#pragma warning( disable : 4311 )           // I believe the int cast below is okay.
#endif

CShader& CShader::operator = (const CShader& src)
{
  uint i;

  mfFree();
  
  int Offs = (int)(INT_PTR)&(((CShader *)0)->m_eSHDType);
  byte *d = (byte *)this;
  byte *s = (byte *)&src;
  memcpy(&d[Offs], &s[Offs], sizeof(CShader)-Offs);

  m_NameShader = src.m_NameShader;
  m_NameFile = src.m_NameFile;

  if (src.m_HWTechniques.Num())
  {
    m_HWTechniques.Create(src.m_HWTechniques.Num());
    for (i=0; i<src.m_HWTechniques.Num(); i++)
    {
      m_HWTechniques[i] = new SShaderTechnique;
      *m_HWTechniques[i] = *src.m_HWTechniques[i];
    }
  }

  m_PublicParams = src.m_PublicParams;

  return *this;
}

#ifdef WIN64
#pragma warning( pop )                  //AMD Port
#endif

SShaderPass::SShaderPass()
{
  m_RenderState = GS_DEPTHWRITE;

  m_PassFlags = 0;
  m_AlphaRef = ~0;

  m_VShader = NULL;
  m_PShader = NULL;
  m_GShader = NULL;
}

SShaderTechnique *SShaderItem::GetTechnique() const
{
  SShaderTechnique *pTech = NULL;
  int nTech = m_nTechnique;
  if (nTech < 0)
    nTech = 0;
  CShader *pSH = (CShader *)m_pShader;
  if (!pSH)
    return NULL;
  if (nTech < (int)pSH->m_HWTechniques.Num())
    return pSH->m_HWTechniques[nTech];
  return NULL;
}

bool SShaderItem::IsMergable(SShaderItem& PrevSI)
{
  if (!PrevSI.m_pShader)
    return true;
  SRenderShaderResources *pRP = (SRenderShaderResources *)PrevSI.m_pShaderResources;
  SRenderShaderResources *pR = (SRenderShaderResources *)m_pShaderResources;
  if (pRP && pR)
  {
    if (pRP->m_AlphaRef >= 0.0001f || pR->m_AlphaRef >= 0.0001f)
      return false;
    if (pRP->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][3] != pR->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][3])
      return false;
    if (pRP->m_pDeformInfo != pR->m_pDeformInfo)
      return false;
    if ((pRP->m_ResFlags & MTL_FLAG_2SIDED) != (pR->m_ResFlags & MTL_FLAG_2SIDED))
      return false;
    if ((pRP->m_ResFlags & MTL_FLAG_NOSHADOW) != (pR->m_ResFlags & MTL_FLAG_NOSHADOW))
      return false;
    if(m_pShader->GetCull() != PrevSI.m_pShader->GetCull())
      return false;
  }
  return true;
}

uint SShaderTechnique::GetPreprocessFlags(CShader *pSH)
{
  uint i;
  uint nFlags = 0;
  if (m_Flags & FHF_RE_FLARE)
    nFlags |= FSPR_CORONA;
  if (pSH->m_Flags & EF_ENVLIGHTING)
  {
    for (i=0; i<m_RTargets.Num(); i++)
    {
      SHRenderTarget *pRT = m_RTargets[i];
      if (pRT->m_pTarget[0] && pRT->m_pTarget[0]->GetCustomID() == TO_RT_LCM)
        nFlags |= FSPR_SCANLCM;
    }
  }
  for (i=0; i<m_Passes.Num(); i++)
  {
    SShaderPass *pPass = &m_Passes[i];
    if (pPass->m_PShader)
      nFlags |= pPass->m_PShader->mfGetPreprocessFlags(this);
  }
  return nFlags;
}

SShaderTechnique *CShader::mfGetStartTechnique(int nTechnique)
{
  SShaderTechnique *pTech;
  if (m_HWTechniques.Num())
  {
    pTech = m_HWTechniques[0];
    if (nTechnique > 0)
    {
      assert(nTechnique < (int)m_HWTechniques.Num());
      if (nTechnique < (int)m_HWTechniques.Num())
        pTech = m_HWTechniques[nTechnique];
      else
        iLog->Log("ERROR: CShader::mfGetStartTechnique: Technique %d for shader '%s' is out of range", nTechnique, GetName());
    }
    else
    if (m_pSelectTech)
    {
      float fVarRef = m_pSelectTech->m_pCVarTech->GetFVal();
      int nRes = 0;
      switch(m_pSelectTech->m_eCompTech)
      {
        case eCF_NotEqual:
          nRes = fVarRef != m_pSelectTech->m_fValRefTech;
          break;
        case eCF_Equal:
          nRes = fVarRef == m_pSelectTech->m_fValRefTech;
          break;
        case eCF_Less:
          nRes = fVarRef < m_pSelectTech->m_fValRefTech;
          break;
        case eCF_Greater:
          nRes = fVarRef > m_pSelectTech->m_fValRefTech;
          break;
        case eCF_LEqual:
          nRes = fVarRef <= m_pSelectTech->m_fValRefTech;
          break;
        case eCF_GEqual:
          nRes = fVarRef >= m_pSelectTech->m_fValRefTech;
          break;
        default:
          assert(0);
      }
      pTech = m_HWTechniques[m_pSelectTech->m_nTech[nRes]];
    }
  }
  else
    pTech = NULL;

  return pTech;
}

void CShader::mfFlushCache()
{
  int n, m;

  for (m=0; m<m_HWTechniques.Num(); m++)
  {
    SShaderTechnique *pTech = m_HWTechniques[m];
    for (n=0; n<pTech->m_Passes.Num(); n++)
    {
      SShaderPass *pPass = &pTech->m_Passes[n];
      if (pPass->m_PShader)
        pPass->m_PShader->mfFlushCacheFile();
      if (pPass->m_VShader)
        pPass->m_VShader->mfFlushCacheFile();
    }
  }
}

void SRenderShaderResources::PostLoad(CShader *pSH)
{
  int i;
  m_nLastTexture = 0;
  for (i=0; i<EFTT_MAX; i++)
  {
    if (!m_Textures[i])
      continue;
    m_nLastTexture = i;
    SEfResTexture *pTex = m_Textures[i];
    pTex->m_TexModificator.m_UpdateFlags = 0;
    if (i != EFTT_DETAIL_OVERLAY)
    {
      if (pTex->m_TexModificator.m_eUMoveType==ETMM_Pan && (pTex->m_TexModificator.m_UOscAmplitude==0 || pTex->m_TexModificator.m_UOscRate==0))
        pTex->m_TexModificator.m_eUMoveType = ETMM_NoChange;
      if (pTex->m_TexModificator.m_eVMoveType==ETMM_Pan && (pTex->m_TexModificator.m_VOscAmplitude==0 || pTex->m_TexModificator.m_VOscRate==0))
        pTex->m_TexModificator.m_eVMoveType = ETMM_NoChange;
      
      if (pTex->m_TexModificator.m_eUMoveType==ETMM_Fixed && pTex->m_TexModificator.m_UOscRate==0)
        pTex->m_TexModificator.m_eUMoveType = ETMM_NoChange;
      if (pTex->m_TexModificator.m_eVMoveType==ETMM_Fixed && pTex->m_TexModificator.m_VOscRate==0)
        pTex->m_TexModificator.m_eVMoveType = ETMM_NoChange;

      if (pTex->m_TexModificator.m_eUMoveType==ETMM_Constant && (pTex->m_TexModificator.m_UOscAmplitude==0 || pTex->m_TexModificator.m_UOscRate==0))
        pTex->m_TexModificator.m_eUMoveType = ETMM_NoChange;
      if (pTex->m_TexModificator.m_eVMoveType==ETMM_Constant && (pTex->m_TexModificator.m_VOscAmplitude==0 || pTex->m_TexModificator.m_VOscRate==0))
        pTex->m_TexModificator.m_eVMoveType = ETMM_NoChange;

      if (pTex->m_TexModificator.m_eUMoveType==ETMM_Stretch && (pTex->m_TexModificator.m_UOscAmplitude==0 || pTex->m_TexModificator.m_UOscRate==0))
        pTex->m_TexModificator.m_eUMoveType = ETMM_NoChange;
      if (pTex->m_TexModificator.m_eVMoveType==ETMM_Stretch && (pTex->m_TexModificator.m_VOscAmplitude==0 || pTex->m_TexModificator.m_VOscRate==0))
        pTex->m_TexModificator.m_eVMoveType = ETMM_NoChange;

      if (pTex->m_TexModificator.m_eUMoveType==ETMM_StretchRepeat && (pTex->m_TexModificator.m_UOscAmplitude==0 || pTex->m_TexModificator.m_UOscRate==0))
        pTex->m_TexModificator.m_eUMoveType = ETMM_NoChange;
      if (pTex->m_TexModificator.m_eVMoveType==ETMM_StretchRepeat && (pTex->m_TexModificator.m_VOscAmplitude==0 || pTex->m_TexModificator.m_VOscRate==0))
        pTex->m_TexModificator.m_eVMoveType = ETMM_NoChange;

      if (pTex->m_TexModificator.m_eTGType != ETG_Stream ||
          pTex->m_TexModificator.m_eUMoveType != ETMM_NoChange ||
          pTex->m_TexModificator.m_eVMoveType != ETMM_NoChange ||
          pTex->m_TexModificator.m_eRotType != ETMR_NoChange ||
          pTex->m_TexModificator.m_Tiling[0] != 1.0f ||
          pTex->m_TexModificator.m_Tiling[1] != 1.0f ||
          pTex->m_TexModificator.m_Offs[0] != 0.0f ||
          pTex->m_TexModificator.m_Offs[1] != 0.0f)
      {
        m_ResFlags |= MTL_FLAG_NOTINSTANCED;
      }
    }
  }
  if (pSH && (pSH->m_Flags & EF_SKY))
  {
    if (m_Textures[EFTT_DIFFUSE])
    {
      char sky[128];
      char path[1024];
      strcpy(sky, m_Textures[EFTT_DIFFUSE]->m_Name.c_str());
      int size = strlen(sky);
      const char *ext = fpGetExtension(sky);
      while (sky[size] != '_')
      {
        size--;
        if (!size)
          break;
      }
      sky[size] = 0;
      if (size)
      {
        m_pSky = new SSkyInfo;
        sprintf(path, "%s_12%s", sky, ext);
        m_pSky->m_SkyBox[0] = CTexture::ForName(path, 0, eTF_Unknown);
        sprintf(path, "%s_34%s", sky, ext);
        m_pSky->m_SkyBox[1] = CTexture::ForName(path, 0, eTF_Unknown);
        sprintf(path, "%s_5%s", sky, ext);
        m_pSky->m_SkyBox[2] = CTexture::ForName(path, 0, eTF_Unknown);
      }
    }
  }
#ifdef USE_PER_MATERIAL_PARAMS
  UpdateConstants(pSH);
#endif
}

CTexture *CShader::mfFindBaseTexture(TArray<SShaderPass>& Passes, int *nPass, int *nTU)
{
  return NULL;
}

ITexture *CShader::GetBaseTexture(int *nPass, int *nTU)
{
  for (uint i=0; i<m_HWTechniques.Num(); i++)
  {
    SShaderTechnique *hw = m_HWTechniques[i];
    CTexture *tx = mfFindBaseTexture(hw->m_Passes, nPass, nTU);
    if (tx)
      return tx;
  }
  if (nPass)
    *nPass = -1;
  if (nTU)
    *nTU = -1;
  return NULL;
}

unsigned int CShader::GetUsedTextureTypes (void)
{
  uint nMask = 0xffffffff;

  return nMask;
}

//================================================================================

void CShaderMan::mfReleaseShaders ()
{
  CCryName Name = CShader::mfGetClassName();
  SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
  if (pRL)
  {
    int n = 0;
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); )
    {
      CShader *sh = (CShader *)itor->second;
      itor++;
      if (!sh)
        continue;
      if (CRenderer::CV_r_printmemoryleaks && !(sh->m_Flags & EF_SYSTEM))
        iLog->Log("Warning: CShaderMan::mfClearAll: Shader %s was not deleted (%d)", sh->GetName(), sh->GetRefCounter());
      if (!sh->m_ShaderGenParams)
      {
        SAFE_RELEASE_FORCE(sh);
      }
      else
      {
        SAFE_RELEASE(sh);
      }
      n++;
    }
  }
}

void CShaderMan::ShutDown(void)
{
  uint i;
  
  gRenDev->m_cEF.m_Bin.InvalidateCache();

  if (CRenderer::CV_r_releaseallresourcesonexit)
  {
    if (gRenDev->m_cEF.m_bInitialized)
    {
      mfReleaseShaders();
    }

    for (i=0; i<CShader::m_ShaderResources_known.Num(); i++)
    {
      SRenderShaderResources *pSR = CShader::m_ShaderResources_known[i];
      if (!pSR)
        continue;
      if (i)
      {
        if (CRenderer::CV_r_printmemoryleaks)
          iLog->Log("Warning: CShaderMan::mfClearAll: Shader resource 0x%x was not deleted", (uintptr_t)pSR);
      }
      delete pSR;
    }
    CShader::m_ShaderResources_known.Free();
  }

  m_AliasNames.clear();
  m_CustomAliasNames.clear();
  m_ShaderNames.clear();

  CCryName Name;

  CHWShader::mfBeginFrame(10000);

  SAFE_DELETE(m_pGlobalExt);

  for (i=0; i<CLightStyle::m_LStyles.Num(); i++)
  {
    delete CLightStyle::m_LStyles[i];
  }
  CLightStyle::m_LStyles.Free();
  m_SGC.clear();
  m_ShaderCacheCombinations.clear();
  SAFE_DELETE(m_pGlobalExt);
  mfCloseShadersCache();
  m_GR.ShutDown();

  gRenDev->m_cEF.m_bInitialized = false;
}

void CShaderMan::mfInitGlobal (void) 
{
  SShaderGen *pShGen = mfCreateShaderGenInfo("Shaders/RunTime.ext");
  m_pGlobalExt = pShGen;
  if (pShGen)
  {
    uint i;

    for (i=0; i<pShGen->m_BitMask.Num(); i++)
    {
      SShaderGenBit *gb = pShGen->m_BitMask[i];
      if (!gb)
        continue;
      if (gb->m_ParamName == "%_RT_FOG")
        g_HWSR_MaskBit[HWSR_FOG] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_AMBIENT")
        g_HWSR_MaskBit[HWSR_AMBIENT] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_HDR_ENCODE")
        g_HWSR_MaskBit[HWSR_HDR_ENCODE] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_ALPHATEST")
        g_HWSR_MaskBit[HWSR_ALPHATEST] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_HDR_MODE")
        g_HWSR_MaskBit[HWSR_HDR_MODE] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_FSAA_QUALITY")
        g_HWSR_MaskBit[HWSR_FSAA_QUALITY] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_FSAA")
        g_HWSR_MaskBit[HWSR_FSAA] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_NEAREST")
        g_HWSR_MaskBit[HWSR_NEAREST] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SHADOW_MIXED_MAP_G16R16")
        g_HWSR_MaskBit[HWSR_SHADOW_MIXED_MAP_G16R16] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_HW_PCF_COMPARE")
        g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE] = gb->m_Mask;
      else
  		if (gb->m_ParamName == "%_RT_SAMPLE0")
				g_HWSR_MaskBit[HWSR_SAMPLE0] = gb->m_Mask;
			else
      if (gb->m_ParamName == "%_RT_SAMPLE1")
        g_HWSR_MaskBit[HWSR_SAMPLE1] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SAMPLE2")
        g_HWSR_MaskBit[HWSR_SAMPLE2] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SAMPLE3")
        g_HWSR_MaskBit[HWSR_SAMPLE3] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SHADOW_FILTER")
        g_HWSR_MaskBit[HWSR_SHADOW_FILTER] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_ALPHABLEND")
        g_HWSR_MaskBit[HWSR_ALPHABLEND] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SPECULAR")
        g_HWSR_MaskBit[HWSR_SPECULAR] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_QUALITY")
        g_HWSR_MaskBit[HWSR_QUALITY] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_QUALITY1")
        g_HWSR_MaskBit[HWSR_QUALITY1] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_INSTANCING_ROT")
        g_HWSR_MaskBit[HWSR_INSTANCING_ROT] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_HDR_SYSLUMINANCE")
        g_HWSR_MaskBit[HWSR_HDR_SYSLUMINANCE] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_HDR_HISTOGRAMM")
        g_HWSR_MaskBit[HWSR_HDR_HISTOGRAMM] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_INSTANCING_ATTR")
        g_HWSR_MaskBit[HWSR_INSTANCING_ATTR] = gb->m_Mask;
      else
			if (gb->m_ParamName == "%_RT_NOZPASS")
				g_HWSR_MaskBit[HWSR_NOZPASS] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SKYLIGHT_BASED_FOG")
        g_HWSR_MaskBit[HWSR_SKYLIGHT_BASED_FOG] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SHAPEDEFORM")
        g_HWSR_MaskBit[HWSR_SHAPEDEFORM] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_MORPHTARGET")
        g_HWSR_MaskBit[HWSR_MORPHTARGET] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_BLEND_ALPHA_MULTI")
        g_HWSR_MaskBit[HWSR_BLEND_ALPHA_MULTI] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_OBJ_IDENTITY")
        g_HWSR_MaskBit[HWSR_OBJ_IDENTITY] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SPHERICAL")
        g_HWSR_MaskBit[HWSR_SPHERICAL] = gb->m_Mask;
			else
			if (gb->m_ParamName == "%_RT_SKELETON_SSD")
				g_HWSR_MaskBit[HWSR_SKELETON_SSD] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_NORMAL_DXT5")
        g_HWSR_MaskBit[HWSR_NORMAL_DXT5] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_DISSOLVE")
        g_HWSR_MaskBit[HWSR_DISSOLVE] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SOFT_PARTICLE")
        g_HWSR_MaskBit[HWSR_SOFT_PARTICLE] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_OCEAN_PARTICLE")
        g_HWSR_MaskBit[HWSR_OCEAN_PARTICLE] = gb->m_Mask;
      else        
      if (gb->m_ParamName == "%_RT_LIGHT_TEX_PROJ")
        g_HWSR_MaskBit[HWSR_LIGHT_TEX_PROJ] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SHADOW_JITTERING")
        g_HWSR_MaskBit[HWSR_SHADOW_JITTERING] = gb->m_Mask;
			else
			if (gb->m_ParamName == "%_RT_VARIANCE_SM")
				g_HWSR_MaskBit[HWSR_VARIANCE_SM] = gb->m_Mask;
			else
			if (gb->m_ParamName == "%_RT_PARTICLE")
				g_HWSR_MaskBit[HWSR_PARTICLE] = gb->m_Mask;
			else
			if (gb->m_ParamName == "%_RT_BLEND_WITH_TERRAIN_COLOR")
				g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR] = gb->m_Mask;
			else
			if (gb->m_ParamName == "%_RT_AMBIENT_OCCLUSION")
				g_HWSR_MaskBit[HWSR_AMBIENT_OCCLUSION] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_GSM_COMBINED")
        g_HWSR_MaskBit[HWSR_GSM_COMBINED] = gb->m_Mask;
      else
			if (gb->m_ParamName == "%_RT_PSM")
				g_HWSR_MaskBit[HWSR_PSM] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SCATTERSHADE")
        g_HWSR_MaskBit[HWSR_SCATTERSHADE] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_DEBUG0")
        g_HWSR_MaskBit[HWSR_DEBUG0] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_DEBUG1")
        g_HWSR_MaskBit[HWSR_DEBUG1] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_DEBUG2")
        g_HWSR_MaskBit[HWSR_DEBUG2] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_DEBUG3")
        g_HWSR_MaskBit[HWSR_DEBUG3] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_POINT_LIGHT")
        g_HWSR_MaskBit[HWSR_POINT_LIGHT] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_TEX_ARR_SAMPLE")
        g_HWSR_MaskBit[HWSR_TEX_ARR_SAMPLE] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_CUBEMAP0")
        g_HWSR_MaskBit[HWSR_CUBEMAP0] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_CUBEMAP1")
        g_HWSR_MaskBit[HWSR_CUBEMAP1] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_CUBEMAP2")
        g_HWSR_MaskBit[HWSR_CUBEMAP2] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_CUBEMAP3")
        g_HWSR_MaskBit[HWSR_CUBEMAP3] = gb->m_Mask;
      else
		  if (gb->m_ParamName == "%_RT_RAE_GEOMTERM")
			  g_HWSR_MaskBit[HWSR_RAE_GEOMTERM] = gb->m_Mask;
			else
		  if (gb->m_ParamName == "%_RT_VEGETATION")
			  g_HWSR_MaskBit[HWSR_VEGETATION] = gb->m_Mask;
			else
			if (gb->m_ParamName == "%_RT_DECAL_TEXGEN_2D")
				g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D] = gb->m_Mask;
			else
			if (gb->m_ParamName == "%_RT_DECAL_TEXGEN_3D")
				g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_3D] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SAMPLE4")
        g_HWSR_MaskBit[HWSR_SAMPLE4] = gb->m_Mask;
      else
      if (gb->m_ParamName == "%_RT_SHADER_LOD")
        g_HWSR_MaskBit[HWSR_SHADER_LOD] = gb->m_Mask;
			else
			if (gb->m_ParamName == "%_RT_VERTEX_SCATTER")
				g_HWSR_MaskBit[HWSR_VERTEX_SCATTER] = gb->m_Mask;
			else
        assert(0);
    }
  }
}

void CShaderMan::mfInit (void) 
{
  CTexture::Init();

  if (!m_bInitialized)
  {
    m_ShadersPath = "Shaders/HWScripts/";
    m_ShadersMergeCachePath = "Shaders/MergeCache/";
#ifdef OPENGL
    m_ShadersCache = "Cache/Shaders/";
#elif defined (DIRECT3D9)
#ifndef XENON
    m_ShadersCache = "Shaders/Cache/D3D9/";
#else
    m_ShadersCache = "Shaders/Cache/Xenon/";
#endif
#elif defined(DIRECT3D10)
    m_ShadersCache = "Shaders/Cache/D3D10/";
#elif defined(LINUX32)
    m_ShadersCache = "Shaders/Cache/LINUX32/";
#elif defined(LINUX64)
    m_ShadersCache = "Shaders/Cache/LINUX64/";
#else
    m_ShadersCache = "Shaders/Cache/";
#endif
    m_szUserPath = "%USER%/";

    fxParserInit();
    CParserBin::Init();

    mfInitShadersList();

    //CShader::m_Shaders_known.Alloc(MAX_SHADERS);
    //memset(&CShader::m_Shaders_known[0], 0, sizeof(CShader *)*MAX_SHADERS);

    m_AliasNames.clear();
    mfInitGlobal();
    mfInitShadersCache();

#if !defined(NULL_RENDERER)
    uint i, j;
    //FILE *fp = fxopen("Shaders/Aliases.txt", "r");
    FILE *fp = iSystem->GetIPak()->FOpen("Shaders/Aliases.txt", "r");
    if (fp)
    {
      while (!iSystem->GetIPak()->FEof(fp))
      {
        char name[128];
        char alias[128];
        char str[256];
        iSystem->GetIPak()->FGets(str, 256, fp);
        int n;
        uint nGenMask;
        if ((n=sscanf(str, "%s %s %x", alias, name, &nGenMask)) >= 2)
        {
          SNameAlias na;
          na.m_Alias = CCryName(alias);
          na.m_Name = CCryName(name);
          if (n == 3)
            na.m_nGenMask = nGenMask;
          else
            na.m_nGenMask = 0;
          m_AliasNames.push_back(na);
        }
      }
      iSystem->GetIPak()->FClose(fp);
    }

    int nGPU = gRenDev->GetFeatures() & RFT_HW_MASK;

    // load CustomAliases (by wat)
    m_CustomAliasNames.clear();
    fp = iSystem->GetIPak()->FOpen("Shaders/CustomAliases.txt", "r");
    if (fp)
    {
      char arg[3][256];
      bool bCond = true;
      while (!iSystem->GetIPak()->FEof(fp))
      {
        char str[256];
        iSystem->GetIPak()->FGets(str, 256, fp);

        char * p = strchr(str, '=');
        if(p)
        {
          memmove(p+2,p,strlen(p)+1);
          str[p-str] = ' '; str[p-str+1] = '='; str[p-str+2] = ' ';
        }

        int nArgs = sscanf(str, "%s %s %s", arg[0], arg[1], arg[2]);
        // parser comments
        for(int i = 0; i<nArgs; i++)
          if(arg[i][0] == ';')
          {
            nArgs = i;
            break;
          }
        // parser aliases
        if(nArgs == 2 && bCond)
        {
          SNameAlias na;
          na.m_Alias = CCryName(arg[0]);
          na.m_Name = CCryName(arg[1]);
          //if(m_CustomAliasNames.Find(na) == -1)
          m_CustomAliasNames.push_back(na);
        }
        // parser conditions vars.
        else if(nArgs == 3 && arg[1][0] == '=')
        {
          ICVar * pVar = iConsole->GetCVar(arg[0]);
          if(pVar)
          {
            if( pVar->GetType()== CVAR_INT && pVar->GetIVal() != atoi(arg[2]))
              bCond = false;
            if( pVar->GetType()== CVAR_FLOAT && pVar->GetFVal() != float(atof(arg[2])))
              bCond = false;
          }
          else
          if(!strnicmp(arg[0], "GPU", 3))
          {
            if(!strnicmp(arg[2], "NV1X", 4) && nGPU != RFT_HW_GF2)
              bCond = false;
            else
            if(!strnicmp(arg[2], "NV2X", 4) && nGPU != RFT_HW_GF3)
              bCond = false;
            else
            if(!strnicmp(arg[2], "ATI", 4) && nGPU != RFT_HW_ATI)		// was R300
              bCond = false;
            else
            if(!strnicmp(arg[2], "NV4X", 4) && nGPU != RFT_HW_NV4X)
              bCond = false;
          }
        }
        else if(arg[0][0] == '}')
          bCond = true;
      }
      iSystem->GetIPak()->FClose(fp);
    }
    for (i=0; i<m_CustomAliasNames.size(); i++)
    {
      CCryName nm = m_CustomAliasNames[i].m_Name;
      bool bChanged = false;
      for (j=i+1; j<m_CustomAliasNames.size(); j++)
      {
        if (m_CustomAliasNames[j].m_Alias == nm)
        {
          m_CustomAliasNames[i].m_Name = m_CustomAliasNames[j].m_Name;
          bChanged = true;
        }
      }
    }


    /* Old version:
    fp = iSystem->GetIPak()->FOpen("Shaders/CustomAliases.txt", "r");
    if (fp)
    {
      while (!iSystem->GetIPak()->FEof(fp))
      {
        char name[128];
        char alias[128];
        char str[256];
        iSystem->GetIPak()->FGets(str, 256, fp);
        if (sscanf(str, "%s %s", alias, name) == 2)
        {
          SNameAlias na;
          na.m_Alias = CCryName(alias, eFN_Add);
          na.m_Name = CCryName(name, eFN_Add);
          m_CustomAliasNames.AddElem(na);
        }
      }
      iSystem->GetIPak()->FClose(fp);
    }
    */

#if (defined(WIN32) || defined(WIN64))
		// make sure we can write to the shader cache
		if(!CheckAllFilesAreWritable((string(m_ShadersCache) + "cgpshaders").c_str())
			|| !CheckAllFilesAreWritable((string(m_ShadersCache) + "cgvshaders").c_str()))
		{
// message box causes problems in fullscreen 
//			MessageBox(0,"WARNING: Shader cache cannot be updated\n\n"
//				"files are write protected / media is read only / windows user setting don't allow file changes","CryEngine2",MB_ICONEXCLAMATION|MB_OK);
			GetISystem()->GetILog()->LogError("ERROR: Shader cache cannot be updated (files are write protected / media is read only / windows user setting don't allow file changes)");
		}
#endif	// WIN

#endif //NULL_RENDERER
    mfSetDefaults();

#ifndef XENON
    m_GR.Init();
#endif
    //mfPrecacheShaders(NULL);

		m_bInitialized = true;
  }
  //mfPrecacheShaders();
}

void CShaderMan::ParseShaderProfile(char *scr, SShaderProfile *pr)
{
  char* name;
  long cmd;
  char *params;
  char *data;

  enum {eUseNormalAlpha=1};
  static STokenDesc commands[] =
  {
    {eUseNormalAlpha, "UseNormalAlpha"},

    {0,0}
  };

  while ((cmd = shGetObject (&scr, commands, &name, &params)) > 0)
  {
    data = NULL;
    if (name)
      data = name;
    else
    if (params)
      data = params;

    switch (cmd)
    {
      case eUseNormalAlpha:
        pr->m_nShaderProfileFlags |= SPF_LOADNORMALALPHA;
        break;
    }
  }
}

//==========================================================================================

bool CShaderMan::ParseFSAADevices(char *scr, SDevID *pDev)
{
  char* name;
  long cmd;
  char *params;
  char *data;

  int bRes = 0;

  enum {eMinID=1, eMaxID};
  static STokenDesc commands[] =
  {
    {eMinID, "MinID"},
    {eMaxID, "MaxID"},

    {0,0}
  };

  SMinMax mm;

  while ((cmd = shGetObject (&scr, commands, &name, &params)) > 0)
  {
    data = NULL;
    if (name)
      data = name;
    else
    if (params)
      data = params;

    switch (cmd)
    {
    case eMinID:
      {
        mm.nMin = shGetInt(data);
        bRes |= 1;
      }
      break;
    case eMaxID:
      {
        mm.nMax = shGetInt(data);
        bRes |= 2;
      }
      break;
    }
  }
  if (bRes == 3)
    pDev->DevMinMax.push_back(mm);
  return (bRes == 3);
}

bool CShaderMan::ParseFSAADevGroup(char *scr, SDevID *pDev)
{
  char* name;
  long cmd;
  char *params;
  char *data;
  int bRes = 0;

  enum {eVendorID=1, eDevices};
  static STokenDesc commands[] =
  {
    {eVendorID, "VendorID"},
    {eDevices, "Devices"},

    {0,0}
  };

  while ((cmd = shGetObject (&scr, commands, &name, &params)) > 0)
  {
    data = NULL;
    if (name)
      data = name;
    else
    if (params)
      data = params;

    switch (cmd)
    {
    case eVendorID:
      {
        pDev->nVendorID = shGetInt(data);
        bRes |= 1;
      }
      break;
    case eDevices:
      {
        if (ParseFSAADevices(params, pDev))
          bRes |= 2;
      }
      break;
    }
  }
  return (bRes == 3);
}

bool CShaderMan::ParseFSAAMode(char *scr, SMSAAMode *pMSAA)
{
  char* name;
  long cmd;
  char *params;
  char *data;
  int bRes = 0;

  enum {eSamples=1, eQuality, eDesc};
  static STokenDesc commands[] =
  {
    {eSamples, "Samples"},
    {eQuality, "Quality"},
    {eDesc, "Desc"},

    {0,0}
  };

  while ((cmd = shGetObject (&scr, commands, &name, &params)) > 0)
  {
    data = NULL;
    if (name)
      data = name;
    else
    if (params)
      data = params;

    switch (cmd)
    {
    case eSamples:
      {
        pMSAA->nSamples = shGetInt(data);
        bRes |= 1;
      }
      break;
    case eQuality:
      {
        pMSAA->nQuality = shGetInt(data);
        bRes |= 2;
      }
      break;
    case eDesc:
      {
        pMSAA->szDescr = data;
        bRes |= 4;
      }
      break;
    }
  }
  return (bRes == 7);
}

bool CShaderMan::ParseFSAAProfile(char *scr, SMSAAProfile *pMSAA)
{
  char* name;
  long cmd;
  char *params;
  char *data;
  int bRes = 0;

  enum {eDeviceGroup=1, eMode};
  static STokenDesc commands[] =
  {
    {eDeviceGroup, "DeviceGroup"},
    {eMode, "Mode"},

    {0,0}
  };

  while ((cmd = shGetObject (&scr, commands, &name, &params)) > 0)
  {
    data = NULL;
    if (name)
      data = name;
    else
    if (params)
      data = params;

    switch (cmd)
    {
    case eDeviceGroup:
      {
        string szName = data;
        pMSAA->pDeviceGroup = NULL;
        for (int i=0; i<m_FSAADevGroups.size(); i++)
        {
          if (m_FSAADevGroups[i]->szName == szName)
          {
            pMSAA->pDeviceGroup = m_FSAADevGroups[i];
            bRes |= 1;
            break;
          }
        }
      }
      break;
    case eMode:
      {
        SMSAAMode mm;
        if (ParseFSAAMode(params, &mm))
        {
          pMSAA->Modes.push_back(mm);
          bRes |= 2;
        }
      }
      break;
    }
  }
  return (bRes == 3);
}

void CShaderMan::ParseFSAAProfiles()
{
  char* name;
  long cmd;
  char *params;
  char *data;

  char *scr = NULL;
#ifdef DIRECT3D10
  FILE *fp = iSystem->GetIPak()->FOpen("Config/FSAAProfilesDX10.txt", "rb");
#else
  FILE *fp = iSystem->GetIPak()->FOpen("Config/FSAAProfiles.txt", "rb");
#endif
  if (fp)
  {
    iSystem->GetIPak()->FSeek(fp, 0, SEEK_END);
    int ln = iSystem->GetIPak()->FTell(fp);
    scr = new char [ln+1];
    if (scr)
    {
      scr[ln] = 0;
      iSystem->GetIPak()->FSeek(fp, 0, SEEK_SET);
      iSystem->GetIPak()->FRead(scr, ln, fp);
      iSystem->GetIPak()->FClose(fp);
    }
  }
  char *pScr = scr;

  enum {eVersion=1, eDeviceGroupID, eFSAAProfile};
  static STokenDesc commands[] =
  {
    {eVersion, "Version"},
    {eDeviceGroupID, "DeviceGroupID"},
    {eFSAAProfile, "FSAAProfile"},

    {0,0}
  };

  while ((cmd = shGetObject (&scr, commands, &name, &params)) > 0)
  {
    data = NULL;
    if (name)
      data = name;
    else
    if (params)
      data = params;

    switch (cmd)
    {
    case eDeviceGroupID:
      {
        SDevID *pDev = new SDevID;
        pDev->szName = name;
        if (ParseFSAADevGroup(params, pDev))
          m_FSAADevGroups.push_back(pDev);
      }
      break;
    case eFSAAProfile:
      {
        SMSAAProfile MSAA;
        MSAA.szName = name;
        if (ParseFSAAProfile(params, &MSAA))
          m_FSAAModes.push_back(MSAA);
      }
      break;
    case eVersion:
      break;
    }
  }
  SAFE_DELETE_ARRAY(pScr);
}

void CShaderMan::ParseShaderProfiles()
{
  int i;

  for (i=0; i<eSQ_Max; i++)
  {
    m_ShaderFixedProfiles[i].m_iShaderProfileQuality = i;
    m_ShaderFixedProfiles[i].m_nShaderProfileFlags = 0;
  }

  char* name;
  long cmd;
  char *params;
  char *data;

  enum {eProfile=1, eVersion};
  static STokenDesc commands[] =
  {
    {eProfile, "Profile"},
    {eVersion, "Version"},

    {0,0}
  };

  char *scr = NULL;
  FILE *fp = iSystem->GetIPak()->FOpen("Shaders/ShaderProfiles.txt", "rb");
  if (fp)
  {
    iSystem->GetIPak()->FSeek(fp, 0, SEEK_END);
    int ln = iSystem->GetIPak()->FTell(fp);
    scr = new char [ln+1];
    if (scr)
    {
      scr[ln] = 0;
      iSystem->GetIPak()->FSeek(fp, 0, SEEK_SET);
      iSystem->GetIPak()->FRead(scr, ln, fp);
      iSystem->GetIPak()->FClose(fp);
    }
  }
  char *pScr = scr;
  if (scr)
  {
    while ((cmd = shGetObject (&scr, commands, &name, &params)) > 0)
    {
      data = NULL;
      if (name)
        data = name;
      else
      if (params)
        data = params;

      switch (cmd)
      {
      case eProfile:
        {
          SShaderProfile *pr = NULL;
          if (!stricmp(name, "Low"))
            pr = &m_ShaderFixedProfiles[eSQ_Low];
          else
          {
            pr = &m_ShaderFixedProfiles[eSQ_High];
          }
          if (pr)
            ParseShaderProfile(params, pr);
          break;
        }
      case eVersion:
        break;
      }
    }
  }
  SAFE_DELETE_ARRAY(pScr);
}

const SShaderProfile &CRenderer::GetShaderProfile(EShaderType eST) const
{
	assert((int)eST < sizeof(m_cEF.m_ShaderProfiles)/sizeof(SShaderProfile));

  return m_cEF.m_ShaderProfiles[eST];
}

void CRenderer::SetShaderQuality(EShaderType eST, EShaderQuality eSQ)
{
  eSQ = CLAMP(eSQ, eSQ_Low, eSQ_VeryHigh);
  if (eST == eST_All)
  {
    for (int i=0; i<eST_Max; i++)
    {
      m_cEF.m_ShaderProfiles[i] = m_cEF.m_ShaderFixedProfiles[eSQ];
      m_cEF.m_ShaderProfiles[eST].m_iShaderProfileQuality = eSQ;      // good?
    }
  }
  else
  {
    m_cEF.m_ShaderProfiles[eST] = m_cEF.m_ShaderFixedProfiles[eSQ];
    m_cEF.m_ShaderProfiles[eST].m_iShaderProfileQuality = eSQ;      // good?
  }
  if (eST == eST_All || eST == eST_General)
  {
    bool bPS20 = ((m_Features & (RFT_HW_PS2X | RFT_HW_PS30)) == 0) || (eSQ == eSQ_Low);
    CParserBin::SetupForPS20(bPS20);
    m_cEF.m_Bin.InvalidateCache();
    m_cEF.mfReloadAllShaders(FRO_FORCERELOAD, 0);
  }
}

static byte bFirst = 1;

static bool sLoadShader(const char *szName, CShader *& pStorage)
{
  CShader *ef = NULL;
  bool bRes = true;
  if (bFirst)
    iLog->Log("Load System Shader '%s'...", szName);
  ef = gRenDev->m_cEF.mfForName(szName, EF_SYSTEM);
  if (bFirst)
  {
    if (!ef || (ef->m_Flags & EF_NOTFOUND))
    {
      iLog->LogPlus("Fail.\n");
      bRes = false;
    }
    else
      iLog->LogPlus("ok");
  }
  pStorage = ef;
  return bRes;
}

static bool sLoadShaderItem(const char *szName, SShaderItem& pStorage, SInputShaderResources *pRes=NULL)
{
  SShaderItem ef;
  bool bRes = true;
  if (bFirst)
    iLog->Log("Load System Shader '%s'...", szName);
  ef = gRenDev->m_cEF.mfShaderItemForName(szName, true, EF_SYSTEM, pRes);
  if (bFirst)
  {
    if (!ef.m_pShader || (ef.m_pShader->GetFlags() & EF_NOTFOUND))
    {
      iLog->LogPlus("Fail.\n");
      bRes = false;
    }
    else
      iLog->LogPlus("ok");
  }
  pStorage = ef;
  return bRes;
}

void CShaderMan::mfSetDefaults (void)
{
  CShader *ef;

  if (bFirst)
    iLog->Log("Construct Shader '<Default>'...");
  ef = mfNewShader("<Default>");
  m_DefaultShader = ef;
  ef->m_Flags |= EF_SYSTEM;
  if (bFirst)
    iLog->LogPlus("ok");

#ifndef NULL_RENDERER
  //ef = mfForName("AuxGeom", EF_SYSTEM, NULL);
  sLoadShader("Fallback", m_ShaderFallback);

  sLoadShader("FixedPipelineEmu", m_ShaderFPEmu);
  sLoadShaderItem("Light.LightStyles", m_ShaderLightStyles);
  sLoadShader("FarTreeSprites", m_ShaderTreeSprites);
  sLoadShader("ShadowBlur", m_ShaderShadowBlur);
	sLoadShader("ShadowMaskGen", m_ShaderShadowMaskGen);
  sLoadShader("AmbientOcclusion", m_ShaderAmbientOcclusion);
	sLoadShader("CloudShadowMaskGen", m_ShaderCloudShadowMaskGen);
  sLoadShader("PostProcess", m_ShaderPostProcess);  
  sLoadShader("PostEffectsGame", m_shPostEffectsGame);  
  sLoadShader("Debug", m_ShaderDebug);
  sLoadShader("LightFlares", m_ShaderLightFlares);
  sLoadShader("Common", m_ShaderCommon);
  sLoadShader("OcclusionTest", m_ShaderOcclTest);  
  //mfForName("Metal", EF_SYSTEM, NULL, 0x1033);
#else
  m_DefaultShaderItem.m_pShader = m_DefaultShader;
  SInputShaderResources ShR;
  m_DefaultShaderItem.m_pShaderResources = new SRenderShaderResources(&ShR);
#endif

//#ifdef DIRECT3D9
  sLoadShader("PostEffects", m_shPostEffects);
//#endif

  bFirst = 0;

  m_bInitialized = true;
}

bool CShaderMan::mfGatherShadersList(const char *szPath, bool bCheckIncludes, bool bUpdateCRC)
{
  struct _finddata_t fileinfo;
  intptr_t handle;
  char nmf[256];
  char dirn[256];

  strcpy(dirn, szPath);
  strcat(dirn, "*.*");

  bool bChanged = false;

  handle = iSystem->GetIPak()->FindFirst (dirn, &fileinfo);
  if (handle == -1)
    return bChanged;
  do
  {
    if (fileinfo.name[0] == '.')
      continue;
    if (fileinfo.attrib & _A_SUBDIR)
    {
      char ddd[256];
      sprintf(ddd, "%s%s/", szPath, fileinfo.name);

      bChanged = mfGatherShadersList(ddd, bCheckIncludes, bUpdateCRC);
      if (bChanged)
        break;
      continue;
    }
    strcpy(nmf, szPath);
    strcat(nmf, fileinfo.name);
    int len = strlen(nmf);
    while (len && nmf[len] != '.')
      len--;
    if (len <= 0)
      continue;
    if (bCheckIncludes)
    {
      if (!stricmp(&nmf[len], ".cfi"))
      {
        fpStripExtension(fileinfo.name, nmf);
        SShaderBin *pBin = m_Bin.GetBinShader(nmf, true, &bChanged);
        if (bChanged)
          break;
      }
      continue;
    }
    if (stricmp(&nmf[len], ".cfx"))
      continue;
    fpStripExtension(fileinfo.name, nmf);
    mfAddFXShaderNames(nmf, m_ShaderNames, bUpdateCRC);
  } while (iSystem->GetIPak()->FindNext(handle, &fileinfo) != -1);

  iSystem->GetIPak()->FindClose (handle);

  return bChanged;
}

void CShaderMan::mfGatherFilesList(const char *szPath, std::vector<CCryName>& Names, int nLevel, bool bUseFilter, bool bMaterial)
{
  struct _finddata_t fileinfo;
  intptr_t handle;
  char nmf[256];
  char dirn[256];

  strcpy(dirn, szPath);
  strcat(dirn, "*.*");

  handle = iSystem->GetIPak()->FindFirst (dirn, &fileinfo);
  if (handle == -1)
    return;
  do
  {
    if (fileinfo.name[0] == '.')
      continue;
    if (fileinfo.attrib & _A_SUBDIR)
    {
      if (!bUseFilter || nLevel != 1 || (m_ShadersFilter && !stricmp(fileinfo.name, m_ShadersFilter)))
      {
        char ddd[256];
        sprintf(ddd, "%s%s/", szPath, fileinfo.name);

        mfGatherFilesList(ddd, Names, nLevel+1, bUseFilter, bMaterial);
      }
      continue;
    }
    strcpy(nmf, szPath);
    strcat(nmf, fileinfo.name);
    int len = strlen(nmf);
    while (len && nmf[len] != '.')
      len--;
    if (len <= 0)
      continue;
    if (!bMaterial && stricmp(&nmf[len], ".fxcb"))
      continue;
    if (bMaterial && stricmp(&nmf[len], ".mtl"))
      continue;
    Names.push_back(CCryName(nmf));
  } while (iSystem->GetIPak()->FindNext(handle, &fileinfo) != -1);

  iSystem->GetIPak()->FindClose (handle);
}

int CShaderMan::mfInitShadersList()
{
  // Detect include changes
  bool bChanged = mfGatherShadersList(m_ShadersPath, true, false);

#ifndef NULL_RENDERER
  mfGatherShadersList(m_ShadersPath, false, bChanged);
  return m_ShaderNames.size();
#else
	return 0;
#endif
}

//===================================================================

CShader *CShaderMan::mfNewShader(const char *szName)
{
  CShader *pSH;

  CCryName className = CShader::mfGetClassName();
  CCryName Name = szName;
  CBaseResource *pBR = CBaseResource::GetResource(className, Name, false);
  if (!pBR)
  {
    pSH = new CShader;
    pSH->Register(className, Name);
  }
  else
  {
    pSH = (CShader *)pBR;
    pSH->AddRef();
  }
  if (!m_pContainer)
    m_pContainer = CBaseResource::GetResourcesForClass(className);

  if (pSH->GetID() >= MAX_REND_SHADERS)
  {
    SAFE_RELEASE(pSH);
    iLog->Log("ERROR: MAX_REND_SHADERS hit\n");
    return NULL;
  }

  return pSH;
}


//=========================================================

bool CShaderMan::mfUpdateMergeStatus(SShaderTechnique *hs, std::vector<SCGParam> *p)
{
  if (!p)
    return false;
  for (uint n=0; n<p->size(); n++)
  {
    if ((*p)[n].m_Flags & PF_DONTALLOW_DYNMERGE)
    {
      hs->m_Flags |= FHF_NOMERGE;
      break;
    }
  }
  if (hs->m_Flags & FHF_NOMERGE)
    return true;
  return false;
}


//=================================================================================================

static float sFRand()
{
  return rand() / (FLOAT)RAND_MAX;
}

SEnvTexture *SHRenderTarget::GetEnv2D()
{
  CRenderer *rd = gRenDev;
  SEnvTexture *pEnvTex = NULL;
  if (m_nIDInPool >= 0)
  {
    assert(m_nIDInPool < (int)CTexture::m_CustomRT_2D.Num());
    if (m_nIDInPool < (int)CTexture::m_CustomRT_2D.Num())
      pEnvTex = &CTexture::m_CustomRT_2D[m_nIDInPool];
  }
  else
  {
    const CCamera& cam = rd->GetCamera();
    Matrix33 orientation = Matrix33(cam.GetMatrix());
    Ang3 Angs = CCamera::CreateAnglesYPR(orientation);
    Vec3 Pos = cam.GetPosition();
    bool bReflect = false;
    if (m_nFlags & (FRT_CAMERA_REFLECTED_PLANE | FRT_CAMERA_REFLECTED_WATERPLANE))
      bReflect = true;
    pEnvTex = CTexture::FindSuitableEnvTex(Pos, Angs, true, 0, false, rd->m_RP.m_pShader, rd->m_RP.m_pShaderResources, rd->m_RP.m_pCurObject, bReflect, rd->m_RP.m_pRE, NULL);
  }
  return pEnvTex;
}

SEnvTexture *SHRenderTarget::GetEnvCM()
{
  CRenderer *rd = gRenDev;
  SEnvTexture *pEnvTex = NULL;

  if (m_nIDInPool >= 0)
  {
    assert(m_nIDInPool < (int)CTexture::m_CustomRT_CM.Num());
    pEnvTex = &CTexture::m_CustomRT_CM[m_nIDInPool];
  }
  else
  {
    Vec3 vPos = rd->m_RP.m_pCurObject->GetTranslation();
    pEnvTex = CTexture::FindSuitableEnvCMap(vPos, true, 0, 0);
  }
  return pEnvTex;
}

// Update TexGen and TexTransform matrices for current material texture
void SEfResTexture::Update(int nTSlot)
{
  int i;
  CRenderer *rd = gRenDev;

  assert( nTSlot < MAX_TMU );
  rd->m_RP.m_ShaderTexResources[nTSlot] = this;

  if (m_TexModificator.m_bDontUpdate)
  {
    if (m_Sampler.m_pTex)
    {
      ETEX_Type eTT = m_Sampler.m_pTex->GetTexType();
      if (eTT == eTT_Cube || eTT == eTT_AutoCube)
        m_TexModificator.m_UpdateFlags |= HWMD_TCTYPE0;
      else
        m_TexModificator.m_UpdateFlags &= ~HWMD_TCTYPE0;
      m_TexModificator.m_UpdateFlags &= ~(HWMD_TCPROJ0 | HWMD_TCGOL0);
			/*
      eTT = (ETEX_Type)m_Sampler.m_eTexType;
      if (eTT == eTT_Auto2D) 
      {
				SEnvTexture *pEnv = m_Sampler.m_pTarget->GetEnv2D();
				assert(pEnv && pEnv->m_pTex && pEnv->m_pTex->m_pTexture);
				if (pEnv && pEnv->m_pTex && pEnv->m_pTex->m_pTexture && pEnv->m_pTex->m_pTexture->m_Matrix)
				{
					m_TexModificator.m_UpdateFlags |= HWMD_TCGOL0;
					mathMatrixMultiply(m_TexModificator.m_TexGenMatrix.GetData(), pEnv->m_pTex->m_pTexture->m_Matrix, GetTransposed44(Matrix44(rd->m_RP.m_pCurObject->m_II.m_Matrix)).GetData(), g_CpuFlags);
					mathMatrixTranspose(m_TexModificator.m_TexGenMatrix.GetData(), m_TexModificator.m_TexGenMatrix.GetData(), g_CpuFlags);
					m_TexModificator.m_UpdateFlags |= HWMD_TCPROJ0;
				}
      }
			*/
    }
		else
		{
			if (m_Sampler.m_pDynTexSource)
			{
				m_TexModificator.m_TexMatrix.SetIdentity();
				m_Sampler.m_pDynTexSource->GetTexGenInfo(m_TexModificator.m_TexMatrix.m30, m_TexModificator.m_TexMatrix.m31, 
					m_TexModificator.m_TexMatrix.m00, m_TexModificator.m_TexMatrix.m11);
				m_TexModificator.m_UpdateFlags |= HWMD_TCM0;
			}
		}

    if (m_TexModificator.m_bTexGenProjected)
      m_TexModificator.m_UpdateFlags |= HWMD_TCPROJ0;
    if (nTSlot < 4)
      rd->m_RP.m_FlagsShader_MD |= m_TexModificator.m_UpdateFlags<<nTSlot;
    return;
  }

  int nFrameID = rd->GetFrameID();
  if (m_TexModificator.m_nFrameUpdated == nFrameID && m_TexModificator.m_nLastRecursionLevel == SRendItem::m_RecurseLevel)
  {
    if (nTSlot < 4)
      rd->m_RP.m_FlagsShader_MD |= m_TexModificator.m_UpdateFlags<<nTSlot;
    return;
  }
  m_TexModificator.m_nFrameUpdated = nFrameID;
  m_TexModificator.m_nLastRecursionLevel = SRendItem::m_RecurseLevel;
  m_TexModificator.m_UpdateFlags = 0;

  bool bTr = false;
  bool bTranspose = false;
  Plane Pl;
  Plane PlTr;
  if (m_TexModificator.m_Tiling[0] == 0)
    m_TexModificator.m_Tiling[0] = 1.0f;
  if (m_TexModificator.m_Tiling[1] == 0)
    m_TexModificator.m_Tiling[1] = 1.0f;

  if (m_TexModificator.m_eUMoveType != ETMM_NoChange || m_TexModificator.m_eVMoveType != ETMM_NoChange || m_TexModificator.m_eRotType != ETMR_NoChange 
   || m_TexModificator.m_Offs[0]!=0.0f || m_TexModificator.m_Offs[1]!=0.0f || m_TexModificator.m_Tiling[0]!=1.0f || m_TexModificator.m_Tiling[1]!=1.0f)
  {
    m_TexModificator.m_TexMatrix.SetIdentity();
    float fTime = gRenDev->m_RP.m_RealTime;

    bTr = true;

    switch(m_TexModificator.m_eRotType)
    {
      case ETMR_Fixed:
        {
          m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix *
            Matrix44(1,0,0,0,
            0,1,0,0,
            m_TexModificator.m_RotOscCenter[0],m_TexModificator.m_RotOscCenter[1],1,0,
            0,0,0,1);
          if (m_TexModificator.m_RotOscPhase[0])
            m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix * Matrix33::CreateRotationX(Word2Degr(m_TexModificator.m_RotOscPhase[0])*PI/180.0f);
          if (m_TexModificator.m_RotOscPhase[1])
            m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix * Matrix33::CreateRotationY(Word2Degr(m_TexModificator.m_RotOscPhase[1])*PI/180.0f);
          if (m_TexModificator.m_RotOscPhase[2])
            m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix * Matrix33::CreateRotationZ(Word2Degr(m_TexModificator.m_RotOscPhase[2])*PI/180.0f);
          m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix *
            Matrix44(1,0,0,0,
            0,1,0,0,
            -m_TexModificator.m_RotOscCenter[0],-m_TexModificator.m_RotOscCenter[1],1,0,
            0,0,0,1);
        }
        break;

      case ETMR_Constant:
        {
          fTime *= 1000.0f;
          float fxAmp = Word2Degr(m_TexModificator.m_RotOscAmplitude[0]) * fTime * PI/180.0f;
          float fyAmp = Word2Degr(m_TexModificator.m_RotOscAmplitude[1]) * fTime * PI/180.0f;
          float fzAmp = Word2Degr(m_TexModificator.m_RotOscAmplitude[2]) * fTime * PI/180.0f;

					//translation matrix for rotation center
          m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix *
            Matrix44(	1,0,0,0,
											0,1,0,0,
											0,0,1,0,
											-m_TexModificator.m_RotOscCenter[0],-m_TexModificator.m_RotOscCenter[1],-m_TexModificator.m_RotOscCenter[2],1);

					//rotation matrix
          if (fxAmp)
            m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix * GetTransposed44(Matrix44(Matrix33::CreateRotationX(fxAmp)));
          if (fyAmp)
            m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix * GetTransposed44(Matrix44(Matrix33::CreateRotationY(fyAmp)));
          if (fzAmp)
            m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix* GetTransposed44(Matrix44(Matrix33::CreateRotationZ(fzAmp)));

					//translation matrix for rotation center
          m_TexModificator.m_TexMatrix =  m_TexModificator.m_TexMatrix *
						Matrix44(	1,0,0,0,
						0,1,0,0,
						0,0,1,0,
						m_TexModificator.m_RotOscCenter[0],m_TexModificator.m_RotOscCenter[1],m_TexModificator.m_RotOscCenter[2],1);
									
        }
        break;

      case ETMR_Oscillated:
        {
          m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix *
            Matrix44(1,0,0,0,
            0,1,0,0,
            -m_TexModificator.m_RotOscCenter[0],-m_TexModificator.m_RotOscCenter[1],1,0,
            0,0,0,1);
          float S_X = fTime * Word2Degr(m_TexModificator.m_RotOscRate[0]);
          float d_X = Word2Degr(m_TexModificator.m_RotOscAmplitude[0]) * cry_sinf(2.0f * PI * ((S_X - cry_floorf(S_X)) + Word2Degr(m_TexModificator.m_RotOscPhase[0])));
          float S_Y = fTime * Word2Degr(m_TexModificator.m_RotOscRate[1]);
          float d_Y = Word2Degr(m_TexModificator.m_RotOscAmplitude[1]) * cry_sinf(2.0f * PI * ((S_Y - cry_floorf(S_Y)) + Word2Degr(m_TexModificator.m_RotOscPhase[1])));
          float S_Z = fTime * Word2Degr(m_TexModificator.m_RotOscRate[2]);
          float d_Z = Word2Degr(m_TexModificator.m_RotOscAmplitude[2]) * cry_sinf(2.0f * PI * ((S_Z - cry_floorf(S_Z)) + Word2Degr(m_TexModificator.m_RotOscPhase[2])));
          if (d_X)
            m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix * Matrix33::CreateRotationX(d_X);
          if (d_Y)
            m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix * Matrix33::CreateRotationY(d_Y);
          if (d_Z)
            m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix * Matrix33::CreateRotationZ(d_Z);
          m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix *
            Matrix44(1,0,0,0,
            0,1,0,0,
            -m_TexModificator.m_RotOscCenter[0],-m_TexModificator.m_RotOscCenter[1],1,0,
            0,0,0,1);
        }
        break;
    }


    float Su = rd->m_RP.m_RealTime * m_TexModificator.m_UOscRate;
    float Sv = rd->m_RP.m_RealTime * m_TexModificator.m_VOscRate;
    switch(m_TexModificator.m_eUMoveType)
    {
      case ETMM_Pan:
        {
          float  du = m_TexModificator.m_UOscAmplitude * cry_sinf(2.0f * PI * (Su - cry_floorf(Su)) + 2.f * PI * m_TexModificator.m_UOscPhase);
          m_TexModificator.m_TexMatrix(3,0) = du;
        }
        break;
      case ETMM_Fixed:
        {
          float du = m_TexModificator.m_UOscRate;
          m_TexModificator.m_TexMatrix(3,0) = du;
        }
        break;
      case ETMM_Constant:
        {
          float du = m_TexModificator.m_UOscAmplitude * Su; //(Su - cry_floorf(Su));
          m_TexModificator.m_TexMatrix(3,0) = du;
        }
        break;
      case ETMM_Jitter:
        {
          if( m_TexModificator.m_LastUTime < 1.0f || m_TexModificator.m_LastUTime > Su + 1.0f )
            m_TexModificator.m_LastUTime = m_TexModificator.m_UOscPhase + cry_floorf(Su);
          if( Su-m_TexModificator.m_LastUTime > 1.0f )
          {
            m_TexModificator.m_CurrentUJitter = sFRand() * m_TexModificator.m_UOscAmplitude;
            m_TexModificator.m_LastUTime = m_TexModificator.m_UOscPhase + cry_floorf(Su);
          }
          m_TexModificator.m_TexMatrix(3,0) = m_TexModificator.m_CurrentUJitter;
        }
        break;
      case ETMM_Stretch:
        {
          float du = m_TexModificator.m_UOscAmplitude * cry_sinf(2.0f * PI * (Su - cry_floorf(Su)) + 2.0f * PI * m_TexModificator.m_UOscPhase);
          m_TexModificator.m_TexMatrix(0,0) = 1.0f+du;
        }
        break;
      case ETMM_StretchRepeat:
        {
          float du = m_TexModificator.m_UOscAmplitude * cry_sinf(0.5f * PI * (Su - cry_floorf(Su)) + 2.0f * PI * m_TexModificator.m_UOscPhase);
          m_TexModificator.m_TexMatrix(0,0) = 1.0f+du;
        }
        break;
    }

    switch(m_TexModificator.m_eVMoveType)
    {
      case ETMM_Pan:
        {
          float dv = m_TexModificator.m_VOscAmplitude * cry_sinf(2.0f * PI * (Sv - cry_floorf(Sv)) + 2.0f * PI * m_TexModificator.m_VOscPhase);
          m_TexModificator.m_TexMatrix(3,1) = dv;
        }
        break;
      case ETMM_Fixed:
        {
          float dv = m_TexModificator.m_VOscRate;
          m_TexModificator.m_TexMatrix(3,1) = dv;
        }
        break;
      case ETMM_Constant:
        {
          float dv = m_TexModificator.m_VOscAmplitude * Sv; //(Sv - cry_floorf(Sv));
          m_TexModificator.m_TexMatrix(3,1) = dv;
        }
        break;
      case ETMM_Jitter:
        {
          if( m_TexModificator.m_LastVTime < 1.0f || m_TexModificator.m_LastVTime > Sv + 1.0f )
            m_TexModificator.m_LastVTime = m_TexModificator.m_VOscPhase + cry_floorf(Sv);
          if( Sv-m_TexModificator.m_LastVTime > 1.0f )
          {
            m_TexModificator.m_CurrentVJitter = sFRand() * m_TexModificator.m_VOscAmplitude;
            m_TexModificator.m_LastVTime = m_TexModificator.m_VOscPhase + cry_floorf(Sv);
          }
          m_TexModificator.m_TexMatrix(3,1) = m_TexModificator.m_CurrentVJitter;
        }
        break;
      case ETMM_Stretch:
        {
          float dv = m_TexModificator.m_VOscAmplitude * cry_sinf(2.0f * PI * (Sv - cry_floorf(Sv)) + 2.0f * PI * m_TexModificator.m_VOscPhase);
          m_TexModificator.m_TexMatrix(1,1) = 1.0f+dv;
        }
        break;
      case ETMM_StretchRepeat:
        {
          float dv = m_TexModificator.m_VOscAmplitude * cry_sinf(0.5f * PI * (Sv - cry_floorf(Sv)) + 2.0f * PI * m_TexModificator.m_VOscPhase);
          m_TexModificator.m_TexMatrix(1,1) = 1.0f+dv;
        }
        break;
    }

    if (m_TexModificator.m_Offs[0]!=0.0f || m_TexModificator.m_Offs[1]!=0.0f || m_TexModificator.m_Tiling[0]!=1.0f || m_TexModificator.m_Tiling[1]!=1.0f || m_TexModificator.m_Rot[0] || m_TexModificator.m_Rot[1] || m_TexModificator.m_Rot[2])
    {
      float du = m_TexModificator.m_Offs[0];
      float dv = m_TexModificator.m_Offs[1];
      float su = m_TexModificator.m_Tiling[0];
      float sv = m_TexModificator.m_Tiling[1];
      m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix *
        Matrix44(su,0,0,0,
                 0,sv,0,0,
                 0,0,1,0,
                 du,dv,0,1);
      if (m_TexModificator.m_Rot[0])
        m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix * Matrix33::CreateRotationX(Word2Degr(m_TexModificator.m_Rot[0])*PI/180.0f);
      if (m_TexModificator.m_Rot[1])
        m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix * Matrix33::CreateRotationY(Word2Degr(m_TexModificator.m_Rot[1])*PI/180.0f);
      if (m_TexModificator.m_Rot[2])
        m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix * Matrix33::CreateRotationZ(Word2Degr(m_TexModificator.m_Rot[2])*PI/180.0f);
      if (m_TexModificator.m_Rot[0] || m_TexModificator.m_Rot[1] || m_TexModificator.m_Rot[2])
        m_TexModificator.m_TexMatrix = m_TexModificator.m_TexMatrix *
        Matrix44(su,0,0,0,
        0,sv,0,0,
        0,0,1,0,
        -du,-dv,0,1);
    }
  }

  if (m_TexModificator.m_eTGType != ETG_Stream)
  {
    switch (m_TexModificator.m_eTGType)
    {
      case ETG_World:
        {
          m_TexModificator.m_UpdateFlags |= HWMD_TCGOL0;
          for (i=0; i<4; i++)
          {
            memset(&Pl, 0, sizeof(Pl));
            float *fPl = (float *)&Pl;
            fPl[i] = 1.0f;
            PlTr = TransformPlane2_NoTrans(GetTransposed44(Matrix44(rd->m_RP.m_pCurObject->m_II.m_Matrix)), Pl);
            m_TexModificator.m_TexGenMatrix(i,0) = PlTr.n.x;
            m_TexModificator.m_TexGenMatrix(i,1) = PlTr.n.y;
            m_TexModificator.m_TexGenMatrix(i,2) = PlTr.n.z;
            m_TexModificator.m_TexGenMatrix(i,3) = PlTr.d;
          }
        }
        break;
      case ETG_Camera:
        {
          m_TexModificator.m_UpdateFlags |= HWMD_TCGOL0;
          for (i=0; i<4; i++)
          {
            memset(&Pl, 0, sizeof(Pl));
            float *fPl = (float *)&Pl;
            fPl[i] = 1.0f;
            PlTr = TransformPlane2_NoTrans(rd->m_ViewMatrix, Pl);
            m_TexModificator.m_TexGenMatrix(i,0) = PlTr.n.x;
            m_TexModificator.m_TexGenMatrix(i,1) = PlTr.n.y;
            m_TexModificator.m_TexGenMatrix(i,2) = PlTr.n.z;
            m_TexModificator.m_TexGenMatrix(i,3) = PlTr.d;
          }
        }
        break;
      case ETG_WorldEnvMap:
        {
          m_TexModificator.m_UpdateFlags |= HWMD_TCGRM0;
          if (bTr)
            m_TexModificator.m_TexMatrix = rd->m_CameraMatrix * m_TexModificator.m_TexMatrix;
          else
            m_TexModificator.m_TexMatrix = rd->m_CameraMatrix;
          bTr = true;
          if (m_Sampler.m_eTexType == eTT_Cube)
            bTranspose = true;
        }
        break;
      case ETG_CameraEnvMap:
        {
          m_TexModificator.m_UpdateFlags |= HWMD_TCGRM0;
        }
        break;
      case ETG_SphereMap:
        {
          m_TexModificator.m_UpdateFlags |= HWMD_TCGSM0;
          if (bTr)
            m_TexModificator.m_TexMatrix = GetTransposed44(Matrix44(rd->m_RP.m_pCurObject->m_II.m_Matrix)) * m_TexModificator.m_TexMatrix;
          else
            m_TexModificator.m_TexMatrix = GetTransposed44(Matrix44(rd->m_RP.m_pCurObject->m_II.m_Matrix));
          //bTr = true;
          //bTranspose = true;
        }
        break;
      case ETG_NormalMap:
        {
          m_TexModificator.m_UpdateFlags |= HWMD_TCGNM0;
        }
        break;
    }
  }

  if (bTr)
  {
    m_TexModificator.m_UpdateFlags |= HWMD_TCM0;
    if (m_TexModificator.m_bTexGenProjected)
    {
      m_TexModificator.m_TexMatrix(0,3) = m_TexModificator.m_TexMatrix(0,2);
      m_TexModificator.m_TexMatrix(1,3) = m_TexModificator.m_TexMatrix(1,2);
      m_TexModificator.m_TexMatrix(2,3) = m_TexModificator.m_TexMatrix(2,2);
      m_TexModificator.m_TexMatrix(3,3) = m_TexModificator.m_TexMatrix(3,2);
    }
    else
    {
      m_TexModificator.m_TexMatrix(0,3) = m_TexModificator.m_TexMatrix(0,2);
      m_TexModificator.m_TexMatrix(1,3) = m_TexModificator.m_TexMatrix(1,2);
      m_TexModificator.m_TexMatrix(2,3) = m_TexModificator.m_TexMatrix(2,2);
    }
  }
  if (!(m_TexModificator.m_UpdateFlags & (HWMD_TCG | HWMD_TCM)))
    m_TexModificator.m_bDontUpdate = true;
  if (m_Sampler.m_pTex)
  {
    ETEX_Type eTT = m_Sampler.m_pTex->GetTexType();
    if (eTT == eTT_Cube || eTT == eTT_AutoCube)
      m_TexModificator.m_UpdateFlags |= HWMD_TCTYPE0;
    else
      m_TexModificator.m_UpdateFlags &= ~HWMD_TCTYPE0;
  }
  if (m_TexModificator.m_bTexGenProjected)
    m_TexModificator.m_UpdateFlags |= HWMD_TCPROJ0;

  if (nTSlot < 4)
    rd->m_RP.m_FlagsShader_MD |= m_TexModificator.m_UpdateFlags<<nTSlot;
}

//---------------------------------------------------------------------------
// Wave evaluator

float CShaderMan::EvalWaveForm(SWaveForm *wf) 
{
  int val;

  float Amp;
  float Freq;
  float Phase;
  float Level;

  if (wf->m_Flags & WFF_LERP)
  {
    val = (int)(gRenDev->m_RP.m_RealTime * 597.0f);
    val &= 0x3ff;
    float fLerp = gRenDev->m_RP.m_tSinTable[val] * 0.5f + 0.5f;

    if (wf->m_Amp != wf->m_Amp1)
      Amp = LERP(wf->m_Amp, wf->m_Amp1, fLerp);
    else
      Amp = wf->m_Amp;

    if (wf->m_Freq != wf->m_Freq1)
      Freq = LERP(wf->m_Freq, wf->m_Freq1, fLerp);
    else
      Freq = wf->m_Freq;

    if (wf->m_Phase != wf->m_Phase1)
      Phase = LERP(wf->m_Phase, wf->m_Phase1, fLerp);
    else
      Phase = wf->m_Phase;

    if (wf->m_Level != wf->m_Level1)
      Level = LERP(wf->m_Level, wf->m_Level1, fLerp);
    else
      Level = wf->m_Level;
  }
  else
  {
    Level = wf->m_Level;
    Amp = wf->m_Amp;
    Phase = wf->m_Phase;
    Freq = wf->m_Freq;
  }

  switch(wf->m_eWFType)
  {
  case eWF_None:
    Warning( "WARNING: CShaderMan::EvalWaveForm called with 'EWF_None' in Shader '%s'\n", gRenDev->m_RP.m_pShader->GetName());
    break;

  case eWF_Sin:
    val = (int)((gRenDev->m_RP.m_RealTime*Freq+Phase)*1024.0f);
    return Amp*gRenDev->m_RP.m_tSinTable[val&0x3ff]+Level;

  case eWF_HalfSin:
    val = QRound((gRenDev->m_RP.m_RealTime*Freq+Phase)*1024.0f);
    return Amp*gRenDev->m_RP.m_tHalfSinTable[val&0x3ff]+Level;

  case eWF_InvHalfSin:
    val = QRound((gRenDev->m_RP.m_RealTime*Freq+Phase)*1024.0f);
    return Amp*(1.0f-gRenDev->m_RP.m_tHalfSinTable[val&0x3ff])+Level;

  case eWF_SawTooth:
    val = QRound((gRenDev->m_RP.m_RealTime*Freq+Phase)*1024.0f);
    val &= 0x3ff;
    return Amp*gRenDev->m_RP.m_tSawtoothTable[val]+Level;

  case eWF_InvSawTooth:
    val = QRound((gRenDev->m_RP.m_RealTime*Freq+Phase)*1024.0f);
    val &= 0x3ff;
    return Amp*gRenDev->m_RP.m_tInvSawtoothTable[val]+Level;

  case eWF_Square:
    val = QRound((gRenDev->m_RP.m_RealTime*Freq+Phase)*1024.0f);
    val &= 0x3ff;
    return Amp*gRenDev->m_RP.m_tSquareTable[val]+Level;

  case eWF_Triangle:
    val = QRound((gRenDev->m_RP.m_RealTime*Freq+Phase)*1024.0f);
    val &= 0x3ff;
    return Amp*gRenDev->m_RP.m_tTriTable[val]+Level;

  case eWF_Hill:
    val = QRound((gRenDev->m_RP.m_RealTime*Freq+Phase)*1024.0f);
    val &= 0x3ff;
    return Amp*gRenDev->m_RP.m_tHillTable[val]+Level;

  case eWF_InvHill:
    val = QRound((gRenDev->m_RP.m_RealTime*Freq+Phase)*1024.0f);
    val &= 0x3ff;
    return Amp*(1.0f-gRenDev->m_RP.m_tHillTable[val])+Level;

  default:
    Warning( "WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, gRenDev->m_RP.m_pShader->GetName());
    break;
  }
  return 1;
}

float CShaderMan::EvalWaveForm(SWaveForm2 *wf) 
{
  int val;

  switch(wf->m_eWFType)
  {
  case eWF_None:
    //Warning( 0,0,"WARNING: CShaderMan::EvalWaveForm called with 'EWF_None' in Shader '%s'\n", gRenDev->m_RP.m_pShader->GetName());
    break;

  case eWF_Sin:
    val = (int)((gRenDev->m_RP.m_RealTime*wf->m_Freq+wf->m_Phase)*1024.0f);
    return wf->m_Amp*gRenDev->m_RP.m_tSinTable[val&0x3ff]+wf->m_Level;

  case eWF_HalfSin:
    val = QRound((gRenDev->m_RP.m_RealTime*wf->m_Freq+wf->m_Phase)*1024.0f);
    return wf->m_Amp*gRenDev->m_RP.m_tHalfSinTable[val&0x3ff]+wf->m_Level;

  case eWF_InvHalfSin:
    val = QRound((gRenDev->m_RP.m_RealTime*wf->m_Freq+wf->m_Phase)*1024.0f);
    return wf->m_Amp*(1.0f-gRenDev->m_RP.m_tHalfSinTable[val&0x3ff])+wf->m_Level;

  case eWF_SawTooth:
    val = QRound((gRenDev->m_RP.m_RealTime*wf->m_Freq+wf->m_Phase)*1024.0f);
    val &= 0x3ff;
    return wf->m_Amp*gRenDev->m_RP.m_tSawtoothTable[val]+wf->m_Level;

  case eWF_InvSawTooth:
    val = QRound((gRenDev->m_RP.m_RealTime*wf->m_Freq+wf->m_Phase)*1024.0f);
    val &= 0x3ff;
    return wf->m_Amp*gRenDev->m_RP.m_tInvSawtoothTable[val]+wf->m_Level;

  case eWF_Square:
    val = QRound((gRenDev->m_RP.m_RealTime*wf->m_Freq+wf->m_Phase)*1024.0f);
    val &= 0x3ff;
    return wf->m_Amp*gRenDev->m_RP.m_tSquareTable[val]+wf->m_Level;

  case eWF_Triangle:
    val = QRound((gRenDev->m_RP.m_RealTime*wf->m_Freq+wf->m_Phase)*1024.0f);
    val &= 0x3ff;
    return wf->m_Amp*gRenDev->m_RP.m_tTriTable[val]+wf->m_Level;

  case eWF_Hill:
    val = QRound((gRenDev->m_RP.m_RealTime*wf->m_Freq+wf->m_Phase)*1024.0f);
    val &= 0x3ff;
    return wf->m_Amp*gRenDev->m_RP.m_tHillTable[val]+wf->m_Level;

  case eWF_InvHill:
    val = QRound((gRenDev->m_RP.m_RealTime*wf->m_Freq+wf->m_Phase)*1024.0f);
    val &= 0x3ff;
    return wf->m_Amp*(1.0f-gRenDev->m_RP.m_tHillTable[val])+wf->m_Level;

  default:
    Warning( "WARNING: CShaderMan::EvalWaveForm: bad WaveType '%d' in Shader '%s'\n", wf->m_eWFType, gRenDev->m_RP.m_pShader->GetName());
    break;
  }
  return 1;
}

float CShaderMan::EvalWaveForm2(SWaveForm *wf, float frac) 
{
  int val;

  if (!(wf->m_Flags & WFF_CLAMP))
    switch(wf->m_eWFType)
  {
    case eWF_None:
      Warning( "CShaderMan::EvalWaveForm2 called with 'EWF_None' in Shader '%s'\n", gRenDev->m_RP.m_pShader->GetName());
      break;

    case eWF_Sin:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val &= 0x3ff;
      return wf->m_Amp*gRenDev->m_RP.m_tSinTable[val]+wf->m_Level;

    case eWF_SawTooth:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val &= 0x3ff;
      return wf->m_Amp*gRenDev->m_RP.m_tSawtoothTable[val]+wf->m_Level;

    case eWF_InvSawTooth:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val &= 0x3ff;
      return wf->m_Amp*gRenDev->m_RP.m_tInvSawtoothTable[val]+wf->m_Level;

    case eWF_Square:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val &= 0x3ff;
      return wf->m_Amp*gRenDev->m_RP.m_tSquareTable[val]+wf->m_Level;

    case eWF_Triangle:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val &= 0x3ff;
      return wf->m_Amp*gRenDev->m_RP.m_tTriTable[val]+wf->m_Level;

    case eWF_Hill:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val &= 0x3ff;
      return wf->m_Amp*gRenDev->m_RP.m_tHillTable[val]+wf->m_Level;

    case eWF_InvHill:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val &= 0x3ff;
      return wf->m_Amp*(1-gRenDev->m_RP.m_tHillTable[val])+wf->m_Level;

    default:
      Warning( "Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, gRenDev->m_RP.m_pShader->GetName());
      break;
  }
  else
    switch(wf->m_eWFType)
  {
    case eWF_None:
      Warning( "Warning: CShaderMan::EvalWaveForm2 called with 'EWF_None' in Shader '%s'\n", gRenDev->m_RP.m_pShader->GetName());
      break;

    case eWF_Sin:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val = min(val, 1023);
      return wf->m_Amp*gRenDev->m_RP.m_tSinTable[val]+wf->m_Level;

    case eWF_SawTooth:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val = min(val, 1023);
      return wf->m_Amp*gRenDev->m_RP.m_tSawtoothTable[val]+wf->m_Level;

    case eWF_InvSawTooth:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val = min(val, 1023);
      return wf->m_Amp*gRenDev->m_RP.m_tInvSawtoothTable[val]+wf->m_Level;

    case eWF_Square:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val = min(val, 1023);
      return wf->m_Amp*gRenDev->m_RP.m_tSquareTable[val]+wf->m_Level;

    case eWF_Triangle:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val = min(val, 1023);
      return wf->m_Amp*gRenDev->m_RP.m_tTriTable[val]+wf->m_Level;

    case eWF_Hill:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val = min(val, 1023);
      return wf->m_Amp*gRenDev->m_RP.m_tHillTable[val]+wf->m_Level;

    case eWF_InvHill:
      val = QRound((frac*wf->m_Freq+wf->m_Phase)*1024.0f);
      val = min(val, 1023);
      return wf->m_Amp*(1.0f-gRenDev->m_RP.m_tHillTable[val])+wf->m_Level;

    default:
      Warning( "Warning: CShaderMan::EvalWaveForm2: bad EWF '%d' in Shader '%s'\n", wf->m_eWFType, gRenDev->m_RP.m_pShader->GetName());
      break;
  }
  return 1;
}

void CShaderMan::mfBeginFrame()
{
  memset(&gRenDev->m_RP.m_PS, 0, sizeof(SPipeStat));
  gRenDev->m_RP.m_RTStats.resize(0);

  m_Frame++;
  gRenDev->m_RP.m_Profile.Free();

  gRenDev->m_RP.m_fMinDepthRange = 0;
  gRenDev->m_RP.m_fMaxDepthRange = 1.0f;

  SRendItem::m_RecurseLevel = 0;

  CHWShader::mfBeginFrame(2);
}

EHWSProfile CHWShader::mfStringToProfile(const char *profile)
{
  EHWSProfile Profile = eHWSP_Unknown;
  if (!strncmp(profile, "ps_1_1", 6))
    Profile = eHWSP_PS_1_1;
  else
  if (!strncmp(profile, "ps_1_4", 6))
    Profile = eHWSP_PS_1_4;
  else
  if (!strncmp(profile, "ps_2_0", 6))
    Profile = eHWSP_PS_2_0;
  else
  if (!strncmp(profile, "ps_2_b", 6) || !strncmp(profile, "ps_2_a", 6))
    Profile = eHWSP_PS_2_X;
  else
  if (!strncmp(profile, "ps_3_0", 6))
    Profile = eHWSP_PS_3_0;
  else
  if (!strncmp(profile, "vs_1_1", 6))
    Profile = eHWSP_VS_1_1;
  else
  if (!strncmp(profile, "vs_2_0", 6))
    Profile = eHWSP_VS_2_0;
  else
  if (!strncmp(profile, "vs_3_0", 6))
    Profile = eHWSP_VS_3_0;
  else
  if (!strncmp(profile, "vs_4_0", 6))
    Profile = eHWSP_VS_4_0;
  else
  if (!strncmp(profile, "ps_4_0", 6))
    Profile = eHWSP_PS_4_0;
  else
  if (!strncmp(profile, "gs_4_0", 6))
    Profile = eHWSP_GS_4_0;
  else
    assert(0);

  return Profile;
}
const char *CHWShader::mfProfileToString(EHWSProfile eProfile)
{
  char *szProfile = "Unknown";
  switch (eProfile)
  {
    case eHWSP_VS_1_1:
      szProfile = "vs_1_1";
      break;
    case eHWSP_VS_2_0:
      szProfile = "vs_2_0";
      break;
    case eHWSP_VS_3_0:
      szProfile = "vs_3_0";
      break;
    case eHWSP_VS_4_0:
      szProfile = "vs_4_0";
      break;
    case eHWSP_PS_1_1:
      szProfile = "ps_1_1";
      break;
    case eHWSP_PS_1_3:
      szProfile = "ps_1_3";
      break;
    case eHWSP_PS_1_4:
      szProfile = "ps_1_4";
      break;
    case eHWSP_PS_2_0:
      szProfile = "ps_2_0";
      break;
    case eHWSP_PS_2_X:
      szProfile = "ps_2_b";
      break;
    case eHWSP_PS_3_0:
      szProfile = "ps_3_0";
      break;
    case eHWSP_PS_4_0:
      szProfile = "ps_4_0";
      break;
    case eHWSP_GS_4_0:
      szProfile = "gs_4_0";
      break;
    default:
      assert(0);
  }
  return szProfile;
}

SShaderGenComb *CShaderMan::mfGetShaderGenInfo(const char *nmFX)
{
  SShaderGenComb *c = NULL;
  int i;
  char nameExt[128];
  for (i=0; i<m_SGC.size(); i++)
  {
    c = &m_SGC[i];
    if (!stricmp(c->Name.c_str(), nmFX))
      break;
  }
  SShaderGenComb cmb;
  if (i == m_SGC.size())
  {
    c = NULL;
    sprintf(nameExt, "Shaders/%s.ext", nmFX);
    cmb.pGen = mfCreateShaderGenInfo(nameExt);
    cmb.Name = CCryName(nmFX);
    m_SGC.push_back(cmb);
    c = &m_SGC[i];
  }
  return c;
}

static uint sGetGL(char **s, CCryName& name, uint& nHWFlags)
{
  uint i;

  nHWFlags = 0;
  SShaderGenComb *c = NULL;
#ifdef XENON
  const char *m = strchr(name.c_str(), '/');
#else
  const char *m = strchr(name.c_str(), '@');
#endif
  assert(m);
  char nmFX[128], nameExt[128];
  if (m)
  {
    strncpy(nmFX, name.c_str(), m-name.c_str());
    nmFX[m-name.c_str()] = 0;
  }
  else
    return -1;
  c = gRenDev->m_cEF.mfGetShaderGenInfo(nmFX);
  if (!c || !c->pGen || !c->pGen->m_BitMask.Num())
    return 0;
  uint nGL = 0;
  SShaderGen *pG = c->pGen;
  for (i=0; i<pG->m_BitMask.Num(); i++)
  {
    SShaderGenBit *pBit = pG->m_BitMask[i];
    if (pBit->m_nDependencySet & (SHGD_HW_BILINEARFP16 | SHGD_HW_SEPARATEFP16))
      nHWFlags |= pBit->m_nDependencySet;
  }
  while (true)
  {
    char theChar;
    int n = 0;
    while ((theChar = **s) != 0)
    {
      if (theChar == ')' || theChar == '|')
      {
        nameExt[n] = 0;
        break;
      }
      nameExt[n++] = theChar;
      ++*s;
    }
    if (!nameExt[0])
      break;
    for (i=0; i<pG->m_BitMask.Num(); i++)
    {
      SShaderGenBit *pBit = pG->m_BitMask[i];
      if (!stricmp(pBit->m_ParamName.c_str(), nameExt))
      {
        nGL |= pBit->m_Mask;
        break;
      }
    }
    if (i == pG->m_BitMask.Num())
    {
      if (!strncmp(nameExt, "0x", 2))
      {
        //nGL |= shGetHex(&nameExt[2]);
      }
      else
      {
        //assert(0);
        iLog->Log("WARNING: Couldn't find global flag '%s' in shader '%s' (skipped)", nameExt, c->Name.c_str());
      }
    }
    if (**s == '|')
      ++*s;
  }
  return nGL;
}

static uint64 sGetRT(char **s)
{
  uint i;

  SShaderGen *pG = gRenDev->m_cEF.m_pGlobalExt;
  if (!pG)
    return 0;
  uint64 nRT = 0;
  char nm[128];
  while (true)
  {
    char theChar;
    int n = 0;
    while ((theChar = **s) != 0)
    {
      if (theChar == ')' || theChar == '|')
      {
        nm[n] = 0;
        break;
      }
      nm[n++] = theChar;
      ++*s;
    }
    if (!nm[0])
      break;
    for (i=0; i<pG->m_BitMask.Num(); i++)
    {
      SShaderGenBit *pBit = pG->m_BitMask[i];
      if (!stricmp(pBit->m_ParamName.c_str(), nm))
      {
        nRT |= pBit->m_Mask;
        break;
      }
    }
    if (i == pG->m_BitMask.Num())
    {
      //assert(0);
//      iLog->Log("WARNING: Couldn't find runtime flag '%s' (skipped)", nm);
    }
    if (**s == '|')
      ++*s;
  }
  return nRT;
}

static int sEOF(bool bFromFile, char *pPtr, FILE *fp)
{
  int nStatus;
  if (bFromFile)
    nStatus = iSystem->GetIPak()->FEof(fp);
  else
  {
    SkipCharacters(&pPtr, kWhiteSpace);
    if (!*pPtr)
      nStatus = 1;
    else
      nStatus = 0;
  }
  return nStatus;
}

void CShaderMan::mfCloseShadersCache()
{
  if (m_FPCacheCombinations)
  {
    iSystem->GetIPak()->FClose(m_FPCacheCombinations);
    m_FPCacheCombinations = NULL;
  }
}

void sSkipLine(char *& s)
{
  char *sEnd = strchr(s, '\n');
  if (sEnd)
  {
    sEnd++;
    s = sEnd;
  }
}

static void sIterateHW_r(FXShaderCacheCombinations *Combinations, SCacheCombination& cmb, int i, int nHW, const char *szName)
{
  const char *str = gRenDev->m_cEF.mfInsertNewCombination(cmb.nGL, cmb.nRT, cmb.nLT, cmb.nMD, cmb.nMDV, cmb.ePR, szName, false);
  CCryName nm = CCryName(str);
  FXShaderCacheCombinationsItor it = Combinations->find(nm);
  if (it == Combinations->end())
  {
    cmb.CacheName = nm;
    Combinations->insert(FXShaderCacheCombinationsItor::value_type(nm, cmb));
  }
  for (int j=i; j<32; j++)
  {
    if ((1<<j) & nHW)
    {
      cmb.nGL &= ~(1<<j);
      sIterateHW_r(Combinations, cmb, j+1, nHW, szName);
      cmb.nGL |= (1<<j);
      sIterateHW_r(Combinations, cmb, j+1, nHW, szName);
    }
  }

}
void CShaderMan::mfInitShadersCache(FXShaderCacheCombinations *Combinations, const char* pCombinations)
{
  char str[2048];
  bool bFromFile = (Combinations == NULL);
  string nameComb;

  if (bFromFile)
  {
    nameComb = CRenderer::CV_r_shadersuserfolder ? m_szUserPath + string("Shaders/Cache/ShaderList.txt") : string("Shaders/Cache/ShaderList.txt");
    m_FPCacheCombinations = iSystem->GetIPak()->FOpen(nameComb.c_str(), "r+");
    if (!m_FPCacheCombinations)
      m_FPCacheCombinations = iSystem->GetIPak()->FOpen(nameComb.c_str(), "w+");
    if (!m_FPCacheCombinations)
    {
      iSystem->GetIPak()->AdjustFileName(nameComb.c_str(), str, 0);
      FILE *statusdst = fopen(str, "rb");
      if (statusdst)
      {
        fclose(statusdst);
        SetFileAttributes(str, FILE_ATTRIBUTE_ARCHIVE);
        m_FPCacheCombinations = iSystem->GetIPak()->FOpen(nameComb.c_str(), "r+");
      }
    }
    Combinations = &m_ShaderCacheCombinations;
  }

  int nLine = 0;
  char *pPtr = (char *)pCombinations;
  if (m_FPCacheCombinations || !bFromFile)
  {
    while (!sEOF(bFromFile, pPtr, m_FPCacheCombinations))
    {
      nLine++;

      str[0] = 0;
      if (bFromFile)
        iSystem->GetIPak()->FGets(str, 2047, m_FPCacheCombinations);
      else
        fxFillCR(&pPtr, str);
      if (!str[0])
        continue;

			if (str[0]=='/' && str[1]=='/')			// commented line e.g. // BadLine: Metal@Common_ShadowPS(%BIllum@IlluminationPS(%DIFFUSE|%ENVCMAMB|%ALPHAGLOW|%STAT_BRANCHING)(%_RT_AMBIENT|%_RT_BUMP|%_RT_GLOW)(101)(0)(0)(ps_2_0)
				continue;

			int size = strlen(str);
      if (str[size-1] == 0xa)
        str[size-1] = 0;
      SCacheCombination cmb;
      char *s = str;
      SkipCharacters(&s, kWhiteSpace);
      if (s[0] != '<')
        continue;
      int nVer = atoi(&s[1]);
      if (nVer != SHADER_LIST_VER)
        continue;
      if (s[2] != '>')
        continue;
      s += 3;
      char *st = s;
      s = strchr(s, '(');
      char name[128];
      FXShaderCacheCombinationsItor it;
      CCryName nm;
      if (s)
      {
        memcpy(name, st, s-st);
        name[s-st] = 0;
        cmb.Name = name;
      }
      s++;
      uint nHW = 0;
      cmb.nGL = sGetGL(&s, cmb.Name, nHW);
      if (cmb.nGL == -1)
      {
        iLog->Log("Error: Error in '%s' file (Line: %d)", nameComb.c_str(), nLine);
        sSkipLine(s);
        goto end;
      }

      s = strchr(s, '(');
      if (!s)
      {
        sSkipLine(s);
        goto end;
      }
      s++;
      cmb.nRT = sGetRT(&s);

      s = strchr(s, '(');
      if (!s)
      {
        sSkipLine(s);
        goto end;
      }
      s++;
      cmb.nLT = shGetHex(s);

      s = strchr(s, '(');
      if (!s)
      {
        sSkipLine(s);
        goto end;
      }
      s++;
      cmb.nMD = shGetHex(s);

      s = strchr(s, '(');
      if (!s)
      {
        sSkipLine(s);
        goto end;
      }
      s++;
      cmb.nMDV = shGetHex(s);

      s = strchr(s, '(');
      if (!s)
      {
        iSystem->GetIPak()->FClose(m_FPCacheCombinations);
        if (!nameComb.empty())
          m_FPCacheCombinations = iSystem->GetIPak()->FOpen(nameComb.c_str(), "w");
        Combinations->clear();
        return;
      }
      s++;
      cmb.ePR = CHWShader::mfStringToProfile(s);
      assert (cmb.ePR != eHWSP_Unknown);
      if (cmb.ePR != eHWSP_Unknown)
      {
        nm = CCryName(st);
        it = Combinations->find(nm);
        if (it != Combinations->end())
        {
          //assert(false);
        }
        else
        {
          cmb.CacheName = nm;
          Combinations->insert(FXShaderCacheCombinationsItor::value_type(nm, cmb));
        }
        if (nHW)
        {
          for (int j=0; j<32; j++)
          {
            if ((1<<j) & nHW)
            {
              cmb.nGL &= ~(1<<j);
              sIterateHW_r(Combinations, cmb, j+1, nHW, name);
              cmb.nGL |= (1<<j);
              sIterateHW_r(Combinations, cmb, j+1, nHW, name);
              cmb.nGL &= ~(1<<j);
            }
          }
        }
      }
      else
end:
        iLog->Log("Error: Error in '%s' file (Line: %d)", nameComb.c_str(), nLine);
    }
  }
}

const char *CShaderMan::mfInsertNewCombination(uint nGL, uint64 nRT, uint nLT, uint nMD, uint nMDV, EHWSProfile ePR, const char *name, bool bStore)
{
  static char str[2048];
  if (!m_FPCacheCombinations && bStore)
    return NULL;

  static CryFastLock s_cResLock;
  AUTO_LOCK(s_cResLock); // Not thread safe without this

  string sGL;
  string sRT;
  uint i, j;
  SShaderGenComb *c = NULL;
  if (nGL)
  {
#ifdef XENON
    const char *m = strchr(name, '/');
#else
    const char *m = strchr(name, '@');
#endif
    assert(m);
    char nmFX[128];
    if (m)
    {
      strncpy(nmFX, name, m-name);
      nmFX[m-name] = 0;
      c = mfGetShaderGenInfo(nmFX);
      if (c && c->pGen && c->pGen->m_BitMask.Num())
      {
        SShaderGen *pG = c->pGen;
        for (i=0; i<32; i++)
        {
          if (nGL & (1<<i))
          {
            for (j=0; j<pG->m_BitMask.Num(); j++)
            {
              SShaderGenBit *pBit = pG->m_BitMask[j];
              if (pBit->m_Mask & (nGL & (1<<i)))
              {
                if (!sGL.empty())
                  sGL += "|";
                sGL += pBit->m_ParamName;
                break;
              }
            }
            if (j == pG->m_BitMask.Num())
            {
              /*if (!sGL.empty())
                sGL += "|";
              char ss[32];
              sprintf(ss, "0x%x", 1<<i);
              sGL += ss;
              //assert(0);
              iLog->Log("WARNING: CShaderMan::mfInsertNewCombination: Mask 0x%x doesn't have associated global name for shader '%s'", 1<<i, c->Name);*/
            }
          }
        }
      }
    }
  }
  if (nRT)
  {
    SShaderGen *pG = m_pGlobalExt;
    if (pG)
    {
      for (i=0; i<pG->m_BitMask.Num(); i++)
      {
        SShaderGenBit *pBit = pG->m_BitMask[i];
        if (pBit->m_Mask & nRT)
        {
          if (!sRT.empty())
            sRT += "|";
          sRT += pBit->m_ParamName;
        }
      }
    }
  }

  if (bStore && nLT)
    nLT = 1;
  sprintf(str, "<%d>%s(%s)(%s)(%x)(%x)(%x)(%s)", SHADER_LIST_VER, name, sGL.c_str(), sRT.c_str(), nLT, nMD, nMDV, CHWShader::mfProfileToString(ePR));
  if (!bStore)
    return str;
  CCryName nm = CCryName(str);
  FXShaderCacheCombinationsItor it = m_ShaderCacheCombinations.find(nm);
  if (it != m_ShaderCacheCombinations.end())
    return NULL;
  SCacheCombination cmb;
  cmb.Name = name;
  cmb.CacheName = str;
  cmb.nGL = nGL;
  cmb.nRT = nRT;
  cmb.nLT = nLT;
  cmb.nMD = nMD;
  cmb.nMDV = nMDV;
  cmb.ePR = ePR;
  m_ShaderCacheCombinations.insert(FXShaderCacheCombinationsItor::value_type(nm, cmb));
  iSystem->GetIPak()->FPrintf(m_FPCacheCombinations, "%s\n", str);
  iSystem->GetIPak()->FFlush(m_FPCacheCombinations);

//	iSystem->GetILog()->Log("mfInsertNewCombination %s",str);

  return str;
}

inline bool sCompareComb(const SCacheCombination &a, const SCacheCombination &b)
{
  int32 dif;
  
  char str1[128], str2[128];
  strcpy(str1, a.Name.c_str());
  strcpy(str2, b.Name.c_str());
  char *c = strchr(str1, '@');
  assert(c);
  if (c)
    *c = 0;
  c = strchr(str2, '@');
  assert(c);
  if (c)
    *c = 0;
  dif = stricmp(str1, str2);
  if (dif != 0)
    return dif < 0;

  if (a.nGL != b.nGL)
    return a.nGL < b.nGL;

  return false;
}

static void sResetDepend_r(SShaderGen *pGen, SShaderGenBit *pBit, SCacheCombination& cm)
{
  if (!pBit->m_DependResets.size())
    return;
  int i, j;

  for (i=0; i<pBit->m_DependResets.size(); i++)
  {
    const char *c = pBit->m_DependResets[i].c_str();
    for (j=0; j<pGen->m_BitMask.Num(); j++)
    {
      SShaderGenBit *pBit1 = pGen->m_BitMask[j];
      if (!stricmp(pBit1->m_ParamName.c_str(), c))
      {
        cm.nRT &= ~pBit1->m_Mask;
        sResetDepend_r(pGen, pBit1, cm);
        break;
      }
    }
  }
}

static void sSetDepend_r(SShaderGen *pGen, SShaderGenBit *pBit, SCacheCombination& cm)
{
  if (!pBit->m_DependSets.size())
    return;
  int i, j;

  for (i=0; i<pBit->m_DependSets.size(); i++)
  {
    const char *c = pBit->m_DependSets[i].c_str();
    for (j=0; j<pGen->m_BitMask.Num(); j++)
    {
      SShaderGenBit *pBit1 = pGen->m_BitMask[j];
      if (!stricmp(pBit1->m_ParamName.c_str(), c))
      {
        cm.nRT |= pBit1->m_Mask;
        sSetDepend_r(pGen, pBit1, cm);
        break;
      }
    }
  }
}

static bool sIterateDL(DWORD& dwDL)
{
  int nLights = dwDL & 0xf;
  int nType[4];
  int i;

  if (!nLights)
  {
    dwDL = 1;
    return true;
  }
  for (i=0; i<nLights; i++)
  {
    nType[i] = (dwDL >> (SLMF_LTYPE_SHIFT + (i*SLMF_LTYPE_BITS))) & ((1<<SLMF_LTYPE_BITS)-1);
  }
  switch (nLights)
  {
    case 1:
			if ((nType[0]&3)<2)
      {
				nType[0]++;
      }
			else
			{
				nLights = 2;
				nType[0] = SLMF_DIRECT;
				nType[1] = SLMF_POINT;
			}
      break;
    case 2:
			if ((nType[0]&3) == SLMF_DIRECT)
      {
				nType[0] = SLMF_POINT;
        nType[1] = SLMF_POINT;
      }
			else
			{
				nLights = 3;
				nType[0] = SLMF_DIRECT;
				nType[1] = SLMF_POINT;
				nType[2] = SLMF_POINT;
			}
      break;
    case 3:
			if ((nType[0]&3) == SLMF_DIRECT)
      {
				nType[0] = SLMF_POINT;
        nType[1] = SLMF_POINT;
        nType[2] = SLMF_POINT;
      }
			else
			{
				nLights = 4;
				nType[0] = SLMF_DIRECT;
				nType[1] = SLMF_POINT;
				nType[2] = SLMF_POINT;
				nType[3] = SLMF_POINT;
			}
      break;
    case 4:
			if ((nType[0]&3) == SLMF_DIRECT)
      {
				nType[0] = SLMF_POINT;
        nType[1] = SLMF_POINT;
        nType[2] = SLMF_POINT;
        nType[3] = SLMF_POINT;
      }
			else
				return false;
  }
  dwDL = nLights;
  for (i=0; i<nLights; i++)
  {
    dwDL |= nType[i] << (SLMF_LTYPE_SHIFT + i*SLMF_LTYPE_BITS);
  }
  return true;
}

/*static bool sIterateDL(DWORD& dwDL)
{
  int nLights = dwDL & 0xf;
  int nType[4];
  int i;

  if (!nLights)
  {
    dwDL = 1;
    return true;
  }
  for (i=0; i<nLights; i++)
  {
    nType[i] = (dwDL >> (SLMF_LTYPE_SHIFT + (i*SLMF_LTYPE_BITS))) & ((1<<SLMF_LTYPE_BITS)-1);
  }
  switch (nLights)
  {
  case 1:
    if (!(nType[0] & SLMF_RAE_ENABLED))
      nType[0] |= SLMF_RAE_ENABLED;
    else
      if (nType[0]<2)
        nType[0]++;
      else
      {
        nLights = 2;
        nType[0] = SLMF_DIRECT;
        nType[1] = SLMF_POINT;
      }
      break;
  case 2:
    if (!(nType[0] & SLMF_RAE_ENABLED))
      nType[0] |= SLMF_RAE_ENABLED;
    else
      if (!(nType[1] & SLMF_RAE_ENABLED))
        nType[1] |= SLMF_RAE_ENABLED;
      else
        if (nType[0] == SLMF_DIRECT)
          nType[0] = SLMF_POINT;
        else
        {
          nLights = 3;
          nType[0] = SLMF_DIRECT;
          nType[1] = SLMF_POINT;
          nType[2] = SLMF_POINT;
        }
        break;
  case 3:
    if (!(nType[0] & SLMF_RAE_ENABLED))
      nType[0] |= SLMF_RAE_ENABLED;
    else
      if (!(nType[1] & SLMF_RAE_ENABLED))
        nType[1] |= SLMF_RAE_ENABLED;
      else
        if (!(nType[2] & SLMF_RAE_ENABLED))
          nType[2] |= SLMF_RAE_ENABLED;
        else
          if (nType[0] == SLMF_DIRECT)
            nType[0] = SLMF_POINT;
          else
          {
            nLights = 4;
            nType[0] = SLMF_DIRECT;
            nType[1] = SLMF_POINT;
            nType[2] = SLMF_POINT;
            nType[3] = SLMF_POINT;
          }
          break;
  case 4:
    if (!(nType[0] & SLMF_RAE_ENABLED))
      nType[0] |= SLMF_RAE_ENABLED;
    else
      if (!(nType[1] & SLMF_RAE_ENABLED))
        nType[1] |= SLMF_RAE_ENABLED;
      else
        if (!(nType[2] & SLMF_RAE_ENABLED))
          nType[2] |= SLMF_RAE_ENABLED;
        else
          if (!(nType[3] & SLMF_RAE_ENABLED))
            nType[3] |= SLMF_RAE_ENABLED;
          else
            if (nType[0] == SLMF_DIRECT)
              nType[0] = SLMF_POINT;
            else
              return false;
  }
  dwDL = nLights;
  for (i=0; i<nLights; i++)
  {
    dwDL |= nType[i] << (SLMF_LTYPE_SHIFT + i*SLMF_LTYPE_BITS);
  }
  return true;
}*/

void CShaderMan::mfAddLTCombination(SCacheCombination *cmb, FXShaderCacheCombinations& CmbsMapDst, DWORD dwL)
{
  char str[1024];
  char sLT[64];

  SCacheCombination cm;
  cm = *cmb;
  cm.nLT = dwL;

  const char *c = strchr(cmb->CacheName.c_str(), ')');
  c = strchr(&c[1], ')');
  int len = c-cmb->CacheName.c_str()+1;
  strncpy(str, cmb->CacheName.c_str(), len);
  str[len] = 0;
  strcat(str, "(");
  sprintf(sLT, "%x", (uint32)dwL);
  strcat(str, sLT);
  c = strchr(&c[2], ')');
  strcat(str, c);
  CCryName nm = CCryName(str);
  cm.CacheName = nm;
  FXShaderCacheCombinationsItor it = CmbsMapDst.find(nm);
  if (it == CmbsMapDst.end())
  {
    CmbsMapDst.insert(FXShaderCacheCombinationsItor::value_type(nm, cm));
  }
}

void CShaderMan::mfAddLTCombinations(SCacheCombination *cmb, FXShaderCacheCombinations& CmbsMapDst)
{
  if (!CRenderer::CV_r_shadersprecachealllights)
    return;

  DWORD dwL = 0;  // 0 lights
  bool bRes = false;
  do
  {
    // !HACK: Do not iterate multiple lights for low spec
    if ((cmb->nRT & (g_HWSR_MaskBit[HWSR_QUALITY] | g_HWSR_MaskBit[HWSR_QUALITY1])) || (dwL & 0xf)<=1)
      mfAddLTCombination(cmb, CmbsMapDst, dwL);
    bRes = sIterateDL(dwL);            	
  } while(bRes);
}


void CShaderMan::mfAddRTCombination_r(int nComb, FXShaderCacheCombinations& CmbsMapDst, SCacheCombination *cmb, CHWShader *pSH, bool bAutoPrecache)
{
  int i, j, n;
  uint32 dwType = pSH->m_dwShaderType;
  if (!dwType)
    return;
  for (i=nComb; i<m_pGlobalExt->m_BitMask.Num(); i++)
  {
    SShaderGenBit *pBit = m_pGlobalExt->m_BitMask[i];
    if (bAutoPrecache && !(pBit->m_Flags & (SHGF_AUTO_PRECACHE | SHGF_LOWSPEC_AUTO_PRECACHE)))
      continue;

    // Precache this flag on low-spec only
    if (pBit->m_Flags & SHGF_LOWSPEC_AUTO_PRECACHE)
    {
      if (cmb->nRT & (g_HWSR_MaskBit[HWSR_QUALITY] | g_HWSR_MaskBit[HWSR_QUALITY1]))
        continue;
    }
    for (n=0; n<pBit->m_PrecacheNames.size(); n++)
    {
      if (pBit->m_PrecacheNames[n] == dwType)
        break;
    }
    if (n < pBit->m_PrecacheNames.size())
    {
      SCacheCombination cm;
      cm = *cmb;
      cm.nRT &= ~pBit->m_Mask;
      cm.nRT |= (pBit->m_Mask ^ cmb->nRT) & pBit->m_Mask;
      if (!bAutoPrecache)
      {
        uint64 nBitSet = pBit->m_Mask & cmb->nRT;
        if (nBitSet)
          sSetDepend_r(m_pGlobalExt, pBit, cm);
        else
          sResetDepend_r(m_pGlobalExt, pBit, cm);
      }

      char str[1024];
      const char *c = strchr(cmb->CacheName.c_str(), '(');
      int len = c-cmb->CacheName.c_str();
      strncpy(str, cmb->CacheName.c_str(), len);
      str[len] = 0;
      const char *c1 = strchr(&c[1], '(');
      len = c1-c;
      strncat(str, c, len);
      strcat(str, "(");
      SShaderGen *pG = m_pGlobalExt;
      string sRT;
      for (j=0; j<pG->m_BitMask.Num(); j++)
      {
        SShaderGenBit *pBit = pG->m_BitMask[j];
        if (pBit->m_Mask & cm.nRT)
        {
          if (!sRT.empty())
            sRT += "|";
          sRT += pBit->m_ParamName;
        }
      }
      strcat(str, sRT.c_str());
      c1 = strchr(&c1[1], ')');
      strcat(str, c1);
      CCryName nm = CCryName(str);
      cm.CacheName = nm;
      // HACK: don't allow unsupported quality mode
      uint64 nQMask = g_HWSR_MaskBit[HWSR_QUALITY] | g_HWSR_MaskBit[HWSR_QUALITY1];
      if ((cm.nRT & nQMask) != nQMask)
      {
        FXShaderCacheCombinationsItor it = CmbsMapDst.find(nm);
        if (it == CmbsMapDst.end())
        {
          CmbsMapDst.insert(FXShaderCacheCombinationsItor::value_type(nm, cm));
        }
      }
      if (pSH->m_Flags & (HWSG_SUPPORTS_MULTILIGHTS | HWSG_SUPPORTS_LIGHTING))
        mfAddLTCombinations(&cm, CmbsMapDst);
      mfAddRTCombination_r(i+1, CmbsMapDst, &cm, pSH, bAutoPrecache);
    }
  }
}

void CShaderMan::mfAddRTCombinations(FXShaderCacheCombinations& CmbsMapSrc, FXShaderCacheCombinations& CmbsMapDst, CHWShader *pSH, bool bListOnly)
{
  if (pSH->m_nFrameLoad == gRenDev->GetFrameID())
    return;
  pSH->m_nFrameLoad = gRenDev->GetFrameID();
  uint32 dwType = pSH->m_dwShaderType;
  if (!dwType)
    return;
  const char *str2 = pSH->mfGetEntryName();
  FXShaderCacheCombinationsItor itor;
  for (itor=CmbsMapSrc.begin(); itor!=CmbsMapSrc.end(); itor++)
  {
    SCacheCombination *cmb = &itor->second;
    const char *c = strchr(cmb->Name.c_str(), '@');
    assert(c);
    if (!c)
      continue;
    if (stricmp(&c[1], str2))
      continue;
    /*if (!stricmp(str2, "MetalReflVS"))
    {
      if (cmb->nGL == 0x1093)
      {
        int nnn = 0;
      }
    }*/
    if (bListOnly)
    {
      if (pSH->m_Flags & (HWSG_SUPPORTS_MULTILIGHTS | HWSG_SUPPORTS_LIGHTING))
        mfAddLTCombinations(cmb, CmbsMapDst);
      mfAddRTCombination_r(0, CmbsMapDst, cmb, pSH, true);
    }
    else
      mfAddRTCombination_r(0, CmbsMapDst, cmb, pSH, false);
  }
}

float fTime0;
float fTime1;
float fTime2;

/*static void sResetAutoGL_r(int i, uint nMask, uint nMaskAuto, SCacheCombination *cmb, std::vector<SCacheCombination>& Cmbs)
{
  if (!nMask)
    return;
  uint nRes = (cmb->nGLAuto & nMaskAuto) & ~nMask;
  if (!nRes)
    return;
  SCacheCombination cm = *cmb;
  cm.nGLAuto &= ~nMask;
  Cmbs.push_back(cm);
  for (int j=0; j<32; j++)
  {
    if (j == i)
      continue;
    uint nM = (1<<j) & nMaskAuto;
    if (!nM)
      continue;
    sResetAutoGL_r(j, nM, nMaskAuto, &cm, Cmbs);
  }
}

static void sSetAutoGL_r(int i, uint nMask, uint nMaskAuto, SCacheCombination *cmb, std::vector<SCacheCombination>& Cmbs)
{
  if (!nMask)
    return;
  SCacheCombination cm = *cmb;
  cm.nGLAuto |= nMask;
  Cmbs.push_back(cm);
  sResetAutoGL_r(i, nMask, nMaskAuto, &cm, Cmbs);
  for (int j=0; j<32; j++)
  {
    if (j == i)
      continue;
    uint nM = (1<<j) & nMaskAuto;
    if (!nM)
      continue;
    sSetAutoGL_r(j, nM, nMaskAuto, &cm, Cmbs);
  }
}*/

void CShaderMan::_PrecacheShaders(FXShaderCacheCombinations *Combinations, bool bListOnly)
{
  float t0 = gEnv->pTimer->GetAsyncCurTime();

  if (!m_pGlobalExt)
    return;

  uint nSaveFeatures = gRenDev->m_Features;
  int nAsync = CRenderer::CV_r_shadersasynccompiling;
  CRenderer::CV_r_shadersasynccompiling = 0;

  if (!Combinations)
  {
    // Command line shaders precaching
    gRenDev->m_Features |= RFT_HW_PS20 | RFT_HW_PS2X | RFT_HW_PS30;
    m_bActivatePhase = false;
    Combinations = &m_ShaderCacheCombinations;
  }
  else
  {
    // Shaders preactivating phase
    m_bActivatePhase = true;
  }
  std::vector<SCacheCombination> Cmbs;
  std::vector<SCacheCombination> CmbsRT;
  FXShaderCacheCombinations CmbsMap;
  int i, j, jj, n, m;
  char str1[128], str2[128];

  FXShaderCacheCombinationsItor itor;
  for (itor=Combinations->begin(); itor!=Combinations->end(); itor++)
  {
    SCacheCombination *cmb = &itor->second;
    FXShaderCacheCombinationsItor it = CmbsMap.find(cmb->CacheName);
    if (it == CmbsMap.end())
    {
      CmbsMap.insert(FXShaderCacheCombinationsItor::value_type(cmb->CacheName, *cmb));
    }
  }
  for (itor=CmbsMap.begin(); itor!=CmbsMap.end(); itor++)
  {
    SCacheCombination *cmb = &itor->second;
    Cmbs.push_back(*cmb);
  }
  if (Cmbs.size() > 2)
  {
    std::sort(Cmbs.begin(), Cmbs.end(), sCompareComb);
    // Add some auto combinations
    /*char *szPrev = "";
    SShaderGen *pShGen = NULL;
    uint32 nMaskAuto = 0;
    for (i=0; i<Cmbs.size(); i++)
    {
      SCacheCombination *cmb = &Cmbs[i];
      strcpy(str1, cmb->Name.c_str());
      char *c = strchr(str1, '@');
      assert(c);
      if (c)
        *c = 0;
      if (stricmp(c, szPrev))
      {
        szPrev = c;
        nMaskAuto = 0;
        SAFE_DELETE(pShGen);
        pShGen = mfCreateShaderGenInfo(c);
        if (pShGen)
        {
          for (j=0; j<pShGen->m_BitMask.Num(); j++)
          {
            SShaderGenBit *pBit = pShGen->m_BitMask[j];
            if (pBit->m_Flags & SHGF_AUTO_PRECACHE)
              nMaskAuto |= pBit->m_Mask;
          }
        }
      }
      if (!nMaskAuto)
        continue;
      cmb->nGL &= ~nMaskAuto;
      cmb->nGLMaskAuto = nMaskAuto;
      for (j=0; j<32; j++)
      {
        uint nM = (1<<j) & nMaskAuto;
        if (!nM)
          continue;
        sSetAutoGL_r(j, nM, nMaskAuto, cmb, CmbsRT);
      }
    }
    SAFE_DELETE(pShGen);*/

    m_nCombinations = Cmbs.size();
    m_bReload = true;
    m_nCombination = 0;
    int nGLLast = -1;
    for (i=0; i<Cmbs.size(); i++)
    {
      SCacheCombination *cmb = &Cmbs[i];
      strcpy(str1, cmb->Name.c_str());
      char *c = strchr(str1, '@');
      assert(c);
      if (c)
        *c = 0;
      m_szShaderPrecache = &c[1];
      gRenDev->m_RP.m_FlagsShader_RT = 0;
      gRenDev->m_RP.m_FlagsShader_LT = 0;
      gRenDev->m_RP.m_FlagsShader_MD = 0;
      gRenDev->m_RP.m_FlagsShader_MDV = 0;
      CShader *pSH = NULL;
      /*if (!stricmp(str1, "Illum") && cmb->nGL == 0x10001003)
      {
      int nnn = 0;
      }*/
      if (!m_bActivatePhase)
        pSH = CShaderMan::mfForName(str1, 0, NULL, cmb->nGL);
      else
      {
        // Check if this shader already loaded
        CBaseResource *pBR = CBaseResource::GetResource(CShader::mfGetClassName(), str1, false);
        bool bGenModified = false;
        pSH = (CShader *)pBR;
        if (pSH && pSH->m_ShaderGenParams)
        {
          uint nGen = cmb->nGL;
          uint nGenHW = 0;
          /*if (nGen == 0x401093)
          {
          int nnn = 0;
          }*/
          mfModifyGenFlags(pSH, NULL, nGen, nGenHW);
          bGenModified = true;
          char nameNew[256];
          sprintf(nameNew, "%s(%x)", str1, nGen);
          pBR = CBaseResource::GetResource(CShader::mfGetClassName(), nameNew, false);
          pSH = (CShader *)pBR;
          nGen |= nGenHW;
        }
        //assert(pSH);
        if (!pSH)
        {
          if (cmb->nGL != nGLLast)
          {
            nGLLast = cmb->nGL;
            iLog->LogError("Error: shader '%s (0x%x)' is not existing during precache phase", str1, cmb->nGL);
          }
          continue;
        }
      }
      gRenDev->m_RP.m_pShader = pSH;
      std::vector<SCacheCombination> *pCmbs = &Cmbs;
      if (!m_bActivatePhase)
      {
        FXShaderCacheCombinations CmbsMapRTSrc;
        FXShaderCacheCombinations CmbsMapRTDst;

        for (j=i; j<Cmbs.size(); j++)
        {
          SCacheCombination *cmba = &Cmbs[j];
          strcpy(str2, cmba->Name.c_str());
          c = strchr(str2, '@');
          assert(c);
          if (c)
            *c = 0;
          if (stricmp(str1, str2) || cmb->nGL!=cmba->nGL)
            break;
          CmbsMapRTSrc.insert(FXShaderCacheCombinationsItor::value_type(cmba->CacheName, *cmba));
        }
        m_nCombinations -= CmbsMapRTSrc.size();

        for (itor=CmbsMapRTSrc.begin(); itor!=CmbsMapRTSrc.end(); itor++)
        {
          SCacheCombination *cmb = &itor->second;
          strcpy(str2, cmb->Name.c_str());
          FXShaderCacheCombinationsItor it = CmbsMapRTDst.find(cmb->CacheName);
          if (it == CmbsMapRTDst.end())
          {
            CmbsMapRTDst.insert(FXShaderCacheCombinationsItor::value_type(cmb->CacheName, *cmb));
          }
        }

        for (m=0; m<pSH->m_HWTechniques.Num(); m++)
        {
          SShaderTechnique *pTech = pSH->m_HWTechniques[m];
          for (n=0; n<pTech->m_Passes.Num(); n++)
          {
            SShaderPass *pPass = &pTech->m_Passes[n];
            if (pPass->m_PShader)
              pPass->m_PShader->m_nFrameLoad = -10;
            if (pPass->m_VShader)
              pPass->m_VShader->m_nFrameLoad = -10;
          }
        }

        for (m=0; m<pSH->m_HWTechniques.Num(); m++)
        {
          SShaderTechnique *pTech = pSH->m_HWTechniques[m];
          for (n=0; n<pTech->m_Passes.Num(); n++)
          {
            SShaderPass *pPass = &pTech->m_Passes[n];
            if (pPass->m_PShader)
              mfAddRTCombinations(CmbsMapRTSrc, CmbsMapRTDst, pPass->m_PShader, bListOnly);
            if (pPass->m_VShader)
              mfAddRTCombinations(CmbsMapRTSrc, CmbsMapRTDst, pPass->m_VShader, bListOnly);
          }
        }
        m_nCombinations += CmbsMapRTDst.size();

        CmbsRT.clear();
        for (itor=CmbsMapRTDst.begin(); itor!=CmbsMapRTDst.end(); itor++)
        {
          SCacheCombination *cmb = &itor->second;
          CmbsRT.push_back(*cmb);
        }
        pCmbs = &CmbsRT;
        CmbsMapRTDst.clear();
        CmbsMapRTSrc.clear();
      }
      else
      {
        CmbsRT.clear();
        for (j=i; j<Cmbs.size(); j++)
        {
          SCacheCombination *cmba = &Cmbs[j];
          strcpy(str2, cmba->Name.c_str());
          c = strchr(str2, '@');
          assert(c);
          if (c)
            *c = 0;
          if (stricmp(str1, str2) || cmb->nGL!=cmba->nGL)
            break;
          CmbsRT.push_back(Cmbs[j]);
        }
        pCmbs = &CmbsRT;
      }
      for (jj=0; jj<pCmbs->size(); jj++)
      {
        SCacheCombination *cmba = &(*pCmbs)[jj];
        c = (char *)strchr(cmba->Name.c_str(), '@');
        assert(c);
        if (!c)
          continue;
        m_szShaderPrecache = &c[1];
        /*if (!stricmp(m_szShaderPrecache, "IlluminationPS"))
        {
        //if (!stricmp(pSH->GetName(), "ParticlesNoMat"))
        //if (!stricmp(cmba->CacheName.c_str(), "Illum@IlluminationPS(%DIFFUSE|%SPECULAR|%GLOSS_MAP|%ENVCMSPEC|%BUMP_MAP|%VERTCOLORS)(%_RT_AMBIENT|%_RT_LIGHT_PASS|%_RT_BUMP|%_RT_SKYLIGHT_BASED_FOG|%_RT_WRITEZ)(1)(0)(0)(ps_2_0)"))
        if ((cmba->nRT == 0x10902200402)) // && cmba->nLT == 0)
        {
        int nnn = 0;
        }
        }*/
        for (m=0; m<pSH->m_HWTechniques.Num(); m++)
        {
          SShaderTechnique *pTech = pSH->m_HWTechniques[m];
          for (n=0; n<pTech->m_Passes.Num(); n++)
          {
            SShaderPass *pPass = &pTech->m_Passes[n];
            gRenDev->m_RP.m_FlagsShader_RT = cmba->nRT;
            gRenDev->m_RP.m_FlagsShader_LT = cmba->nLT;
            gRenDev->m_RP.m_FlagsShader_MD = cmba->nMD;
            gRenDev->m_RP.m_FlagsShader_MDV = cmba->nMDV;
            // Adjust some flags for low spec
            if (m_bActivatePhase)
            {
              gRenDev->EF_ApplyShaderQuality(pSH->m_eShaderType);
              if (pPass->m_PShader && !(pPass->m_PShader->m_Flags & HWSG_FP_EMULATION))
              {
                if (gRenDev->EF_GetShaderQuality(pSH->m_eShaderType) == eSQ_Low && (gRenDev->m_RP.m_FlagsShader_LT & 0xf) > 1)
                  gRenDev->m_RP.m_FlagsShader_LT = (gRenDev->m_RP.m_FlagsShader_LT & 0xf0) | 1;
              }
            }
            if (pPass->m_PShader)
            {
              if (!m_szShaderPrecache || !stricmp(m_szShaderPrecache, pPass->m_PShader->m_EntryFunc.c_str()) != 0)
              {
                bool bRes = pPass->m_PShader->mfSet(HWSF_PRECACHE);
              }
            }
            if (pPass->m_VShader)
            {
              if (!m_szShaderPrecache || !stricmp(m_szShaderPrecache, pPass->m_VShader->m_EntryFunc.c_str()) != 0)
              {
                bool bRes = pPass->m_VShader->mfSet(HWSF_PRECACHE);
              }
            }
            if (pPass->m_GShader)
            {
              if (!m_szShaderPrecache || !stricmp(m_szShaderPrecache, pPass->m_GShader->m_EntryFunc.c_str()) != 0)
              {
                bool bRes = pPass->m_GShader->mfSet(HWSF_PRECACHE);
              }
            }
#ifdef WIN32
            if (!m_bActivatePhase)
            {
              MSG msg;
              while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
              {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
              }
            }
#endif
          }
        }
        m_nCombination++;
      }
      pSH->mfFlushCache();
      if (!m_bActivatePhase)
      {
        SAFE_RELEASE(pSH);
      }
      i = j-1;
    }
  }
  // Optimise shader resources
  if (!m_bActivatePhase)
  {
    FXShaderCacheNamesItor it;
    n = 0;
    for (it=CHWShader::m_ShaderCacheList.begin(); it!=CHWShader::m_ShaderCacheList.end(); it++)
    {
      const char *szName = it->first.c_str();
      SShaderCache *c = CHWShader::mfInitCache(szName, NULL, false, it->second, true, true);
      if (c)
        CHWShader::mfOptimiseCacheFile(c);
      c->Release();
      n++;
    }
    FILE *FP = iSystem->GetIPak()->FOpen("Shaders/Cache/ShaderListDone", "w");
    if (FP)
    {
      iSystem->GetIPak()->FPrintf(FP, "done: %d", m_nCombination);
      iSystem->GetIPak()->FClose(FP);
    }
  }
  CHWShader::m_ShaderCacheList.clear();

  m_nCombinations = -1;
  m_nCombination = -1;
  m_bReload = false;
  m_szShaderPrecache = NULL;
  m_bActivatePhase = false;
  CRenderer::CV_r_shadersasynccompiling = nAsync;

  gRenDev->m_Features = nSaveFeatures;

  float t1 = gEnv->pTimer->GetAsyncCurTime();
  CryLogAlways( "All shaders combinations compiled in %.2f seconds",(t1-t0) );
}

void CShaderMan::mfPrecacheShaders(FXShaderCacheCombinations *Combinations, bool bListOnly)
{
  CHWShader::mfFlushPendedShaders();

  if (Combinations)
  {
    // Preactivation
    _PrecacheShaders(Combinations, bListOnly);
  }
  else
  {
    uint nComp = CRenderer::CV_r_shadersintcompiler;
    CRenderer::CV_r_shadersintcompiler = 0;

    CParserBin::SetupForD3D10();
    CryLogAlways("\nStarting shader compilation for D3D10");
    _PrecacheShaders(Combinations, bListOnly);

    CRenderer::CV_r_shadersintcompiler = nComp;

    CParserBin::SetupForD3D9();
    CryLogAlways("Starting shader compilation for D3D9");
    _PrecacheShaders(Combinations, bListOnly);

#if defined (DIRECT3D10)
    CParserBin::SetupForD3D10();
#elif defined (DIRECT3D9)
    CParserBin::SetupForD3D9();
#endif
  }
}

void CShaderMan::mfOptimiseShaders(const char *szFolder)
{
  CHWShader::mfFlushPendedShaders();

  float t0 = gEnv->pTimer->GetAsyncCurTime();
  SShaderCache *pCache;
  int i;

  std::vector<CCryName> Names;
  mfGatherFilesList(szFolder, Names, 0, false);

  for (i=0; i<Names.size(); i++)
  {
    const char *szName = Names[i].c_str();
    pCache = CHWShader::mfInitCache(szName, NULL, false, 0, true, false);
    if (!pCache || !pCache->m_pRes[CACHE_USER])
      continue;
    CHWShader::mfOptimiseCacheFile(pCache);
    pCache->Release();
  }

  float t1 = gEnv->pTimer->GetAsyncCurTime();
  CryLog("All shaders combinations optimized in %.2f seconds", t1-t0);
}

struct SMgData
{
  CCryName Name;
  int nSize;
  uint CRC;
  byte *pData;
  int nID;
  byte bProcessed;
};

static int snCurListID;
typedef std::map<CCryName, SMgData> ShaderData;
typedef ShaderData::iterator ShaderDataItor;

static void sAddToList(SShaderCache *pCache, ShaderData& Data)
{
  uint i;

  CResFile *pRes = pCache->m_pRes[CACHE_USER];
  TArray<SDirEntry *> Dir;
  pRes->mfGetDirectory(Dir);
  for (i=0; i<Dir.Num(); i++)
  {
    SDirEntry *pDE = Dir[i];
    const char *str = pDE->GetName();
    if (str[0] == '$')
      continue;
    ShaderDataItor it = Data.find(pDE->Name);
    if (it == Data.end())
    {
      SMgData d;
      d.nSize = pRes->mfFileRead(pDE);
      if (d.nSize < sizeof(SShaderCacheHeaderItem))
      {
        assert(0);
        continue;
      }
      d.pData = new byte[d.nSize];
      memcpy(d.pData, pDE->user.data, d.nSize);
      SShaderCacheHeaderItem *pItem = (SShaderCacheHeaderItem *)d.pData;
      d.bProcessed = 0;
      d.Name = pDE->Name;
      d.CRC = pItem->m_CRC32;
      d.nID = snCurListID++;
      Data.insert(ShaderDataItor::value_type(d.Name, d));
    }
  }

}

struct SNameData
{
  CCryName Name;
  bool bProcessed;
};

void CShaderMan::_MergeShaders()
{
  float t0 = gEnv->pTimer->GetAsyncCurTime();
  SShaderCache *pCache;
  uint i, j;

  std::vector<CCryName> NM;
  std::vector<SNameData> Names;
  mfGatherFilesList(m_ShadersMergeCachePath, NM, 0, true);
  for (i=0; i<NM.size(); i++)
  {
    SNameData dt;
    dt.bProcessed = false;
    dt.Name = NM[i];
    Names.push_back(dt);
  }

  uint32 CRC32 = 0;
  for (i=0; i<Names.size(); i++)
  {
    if (Names[i].bProcessed)
      continue;
    Names[i].bProcessed = true;
    const char *szNameA = Names[i].Name.c_str();
    iLog->Log(" Merging shader resource '%s'...", szNameA);
    char szDrv[16], szDir[256], szName[256], szExt[32], szName1[256], szName2[256];
    _splitpath(szNameA, szDrv, szDir, szName, szExt);
    sprintf(szName1, "%s%s", szName, szExt);
    uint nLen = strlen(szName1);
    pCache = CHWShader::mfInitCache(szNameA, NULL, false, CRC32, true, true);
    if (pCache->m_pRes[CACHE_USER])
      CRC32 = pCache->m_Header[CACHE_USER].m_CRC32;
    else
    if (pCache->m_pRes[CACHE_READONLY])
      CRC32 = pCache->m_Header[CACHE_READONLY].m_CRC32;
    else
    {
      assert(0);
    }
    ShaderData Data;
    snCurListID = 0;
    sAddToList(pCache, Data);
    SAFE_RELEASE(pCache);
    for (j=i+1; j<Names.size(); j++)
    {
      if (Names[j].bProcessed)
        continue;
      const char *szNameB = Names[j].Name.c_str();
      _splitpath(szNameB, szDrv, szDir, szName, szExt);
      sprintf(szName2, "%s%s", szName, szExt);
      if (!stricmp(szName1, szName2))
      {
        Names[j].bProcessed = true;
        SShaderCache *pCache1 = CHWShader::mfInitCache(szNameB, NULL, false, 0, true, true);
        assert(pCache1->m_Header[CACHE_USER].m_CRC32 == CRC32);
        if (pCache1->m_Header[CACHE_USER].m_CRC32 != CRC32)
        {
          Warning("WARNING: CRC mismatch for %s", szNameB);
        }
        sAddToList(pCache1, Data);
        SAFE_RELEASE(pCache1);
      }
    }
    char szDest[256];
    strcpy(szDest, m_ShadersCache);
    const char *p = &szNameA[strlen(szNameA)-nLen-2];
    while (*p != '/' && *p != '\\')
    {
      p--;
    }
    strcat(szDest, p+1);
    pCache = CHWShader::mfInitCache(szDest, NULL, true, CRC32, true, false);
    CResFile *pRes = pCache->m_pRes[CACHE_USER];
    pRes->mfClose();
    pRes->mfOpen(RA_CREATE);

    float fVersion = (float)FX_CACHE_VER;
    SDirEntry de;
    SShaderCacheHeader hd;
    de.Name = CCryName("$HEAD");
    de.earc = eARC_NONE;
    de.size = sizeof(SShaderCacheHeader);
    de.user.data = &hd;
    hd.m_SizeOf = sizeof(SShaderCacheHeader);
    hd.m_MinorVer = (int)(((float)fVersion - (float)(int)fVersion)*10.1f);
    hd.m_MajorVer = (int)fVersion;
    hd.m_bOptimised = false;
    hd.m_CRC32 = CRC32;
    pRes->mfFileAdd(&de);
    pRes->mfFlush(0);
    pCache->m_Header[CACHE_USER] = hd;

    int nDeviceShadersCounter = SH_UNIQ_ID;
    ShaderDataItor it;
    for (it=Data.begin(); it!=Data.end(); it++)
    {
      SMgData *pD = &it->second;
      SShaderCacheHeaderItem *pItem = (SShaderCacheHeaderItem *)pD->pData;

      SDirEntry de;
      de.Name = pD->Name;
      de.earc = eARC_LZSS;
      de.size = pD->nSize;
      byte *pNew = new byte[de.size];
      pItem->m_DeviceObjectID = nDeviceShadersCounter++;
      memcpy(pNew, pD->pData, pD->nSize);
      de.user.data = pNew;
      de.refOrg = pItem->m_DeviceObjectID;
      de.flags = RF_TEMPDATA;
      pRes->mfFileAdd(&de);
    }
    for (it=Data.begin(); it!=Data.end(); it++)
    {
      SMgData *pD = &it->second;
      delete [] pD->pData;
    }
    Data.clear();
    pRes->mfFlush(0);
    iLog->Log(" ...%d result items...", pRes->mfGetNumFiles());
    pCache->Release();
  }

  mfOptimiseShaders(gRenDev->m_cEF.m_ShadersCache);

  float t1 = gEnv->pTimer->GetAsyncCurTime();
  CryLog("All shaders files merged in %.2f seconds", t1-t0);
}

void CShaderMan::mfMergeShaders()
{
  CHWShader::mfFlushPendedShaders();

  CParserBin::SetupForD3D9();
  _MergeShaders();

  CParserBin::SetupForD3D10();
  _MergeShaders();

#if defined (DIRECT3D10)
  CParserBin::SetupForD3D10();
#elif defined (DIRECT3D9)
  CParserBin::SetupForD3D9();
#endif
}

void CShaderMan::mfFixMaterials()
{
  return;
  std::vector<CCryName> NM;
  mfGatherFilesList("Materials/", NM, 0, false, true);
  int nFailedFiles = 0;
  int nFailedMaterials = 0;
  int nChangedFiles = 0;
  int nChangedMaterials = 0;
  bool bChanged = false;
  for (int i=0; i<NM.size(); i++)
  {
    FILE *fp = iSystem->GetIPak()->FOpen(NM[i].c_str(), "rb");
    if (!fp)
    {
      Warning("Couldn't find material '%s'", NM[i].c_str());
      nFailedFiles++;
      continue;
    }
    iSystem->GetIPak()->FSeek(fp, 0, SEEK_END);
    int nSize = iSystem->GetIPak()->FTell(fp);
    char *szMat = (char *)CryModuleMalloc(nSize);
    iSystem->GetIPak()->FSeek(fp, 0, SEEK_SET);
    iSystem->GetIPak()->FReadRaw(szMat, nSize, 1, fp);
    iSystem->GetIPak()->FClose(fp);
    char *szStart = szMat;
    bChanged = false;
    while (szStart)
    {
      szStart = strstr(szStart, "<Material");
      if (!szStart)
        break;
      if (szStart[9] > 0x20)
      {
        szStart += 9;
        continue;
      }
      char *szEnd = strstr(szStart, "</Material");
      if (!szEnd)
        break;
      if (!sSkipChars[szEnd[10]])
      {
        szEnd += 10;
        while (szEnd)
        {
          szEnd = strstr(szEnd, "</Material");
          if (!szEnd)
            break;
          if (sSkipChars[szEnd[10]])
            break;
          szEnd += 10;
        }
      }
      char *szSh = strstr(szStart, "Shader=");
      if (!szSh || szSh > szEnd)
      {
        szStart = szEnd;
        continue;
      }
      char *szMask = strstr(szStart, "GenMask=");
      if (!szMask || szMask > szEnd)
      {
        szStart = szEnd;
        continue;
      }
      szStart = szEnd;
      szSh += 7;
      assert(*szSh == '"');
      char *szEndSh = strchr(&szSh[1], '"');
      assert(szEndSh);
      char szShader[256];
      int nS = szEndSh-&szSh[1];
      memcpy(szShader, &szSh[1], nS);
      szShader[nS] = 0;
      char *szDot = strchr(szShader, '.');
      if (szDot)
        *szDot = 0;
      bool bIllum = false;
      if (!stricmp(szShader, "Illum"))
        bIllum = true;
      else
      if (stricmp(szShader, "Terrain"))
        continue;
      szMask += 8;
      assert(*szMask == '"');
      szMask++;
      uint nMask = atoi(szMask);
      int nNewMask;
      if (bIllum)
      {
        nNewMask = nMask & ~(0x40000 | 0x8000000);
        if (nMask & 0x40000)
          nNewMask |= 0x8000000;
        if (nMask & 0x8000000)
          nNewMask |= 0x4000;
      }
      else
      {
        nNewMask = nMask & ~0x2000;
        if (nMask & 0x2000)
          nNewMask |= 0x8000000;
      }
      if (nMask == nNewMask)
        continue;
      if (!bChanged)
      {
        bChanged = true;
        nChangedFiles++;
        iLog->Log("Process material '%s'...", NM[i].c_str());
      }
      nChangedMaterials++;
      char szVal[64];
      sprintf(szVal, "%d", nNewMask);
      char *szE = strchr(szMask, '"');
      assert(szE);
      int n = nSize - (szE - szMat);
      memmove(szMask, szE, n);
      int nExp = strlen(szVal);
      nS = szE - szMask;
      if (nExp != nS)
      {
        int nOffs = szMask - szMat;
        szMat = (char *)CryModuleRealloc(szMat, nSize + (nExp-nS));
        nSize += nExp-nS;
        szMask = &szMat[nOffs];
      }
      memmove(&szMask[nExp], szMask, n);
      memcpy(szMask, szVal, nExp);
      szStart = szMask;
    }
    if (bChanged)
    {
      FILE *fp = iSystem->GetIPak()->FOpen(NM[i].c_str(), "wb");
      if (!fp)
      {
        Warning("***Couldn't write material '%s'", NM[i].c_str());
        nFailedFiles++;
      }
      else
      {
        iSystem->GetIPak()->FWrite(szMat, nSize, fp);
        iSystem->GetIPak()->FClose(fp);
      }
    }
    CryModuleFree(szMat);
  }
  iLog->Log("Done: %d files updated, %d materials updated [%d files failed, %d materials failed]", nChangedFiles, nChangedMaterials, nFailedFiles, nFailedMaterials);
}


//////////////////////////////////////////////////////////////////////////
bool CShaderMan::CheckAllFilesAreWritable( const char *szDir ) const
{
#if (defined(WIN32) || defined(WIN64))
	assert(szDir);

	ICryPak *pack = GetISystem()->GetIPak();			assert(pack);

	string sPathWithFilter = string(szDir) + "/*.*";

	// Search files that match filter specification.
	_finddata_t fd;
	int res;
	intptr_t handle;
	if ((handle = pack->FindFirst(sPathWithFilter.c_str(),&fd))!=-1)
	{
		do
		{
			if((fd.attrib & _A_SUBDIR)==0)
			{
				string fullpath = string(szDir) + "/" + fd.name;

        FILE *out = pack->FOpen(fullpath.c_str(),"rb");
        if (!out)
        {
          res = pack->FindNext( handle,&fd );
          continue;
        }
        if (pack->IsInPak(out))
        {
          pack->FClose(out);
          res = pack->FindNext( handle,&fd );
          continue;
        }
        pack->FClose(out);

				out = pack->FOpen(fullpath.c_str(),"ab");

				if(out)
					pack->FClose(out);
				else
				{
					gEnv->pLog->LogError("ERROR: Shader cache is not writable (file: '%s')",fullpath.c_str());
					return false;
				}
			}

			res = pack->FindNext( handle,&fd );
		} while (res >= 0);

		pack->FindClose(handle);

		gEnv->pLog->LogToFile("Shader cache directory '%s' was successfully tested for being writable",szDir);
	}
	else
    CryLog("Shader cache directory '%s' does not exist", szDir);

#endif

	return true;
}

//==========================================================================================================

inline bool sCompareRes(SRenderShaderResources* a, SRenderShaderResources* b)
{
  if (!a || !b)
    return a < b;

  if (a->m_AlphaRef != b->m_AlphaRef)
    return a->m_AlphaRef < b->m_AlphaRef;

  if (a->Opacity() != b->Opacity())
    return a->Opacity() < b->Opacity();

  if (a->m_pDeformInfo != b->m_pDeformInfo)
    return a->m_pDeformInfo < b->m_pDeformInfo;

  if (a->m_RTargets.Num() != b->m_RTargets.Num())
    return a->m_RTargets.Num() < b->m_RTargets.Num();

  if ((a->m_ResFlags&MTL_FLAG_2SIDED) != (b->m_ResFlags&MTL_FLAG_2SIDED))
    return (a->m_ResFlags&MTL_FLAG_2SIDED) < (b->m_ResFlags&MTL_FLAG_2SIDED);

  ITexture *pTA = NULL;
  ITexture *pTB = NULL;
  if (a->m_Textures[EFTT_GLOSS])
    pTA = a->m_Textures[EFTT_GLOSS]->m_Sampler.m_pITex;
  if (b->m_Textures[EFTT_GLOSS])
    pTB = b->m_Textures[EFTT_GLOSS]->m_Sampler.m_pITex;
  if (pTA != pTB)
    return pTA < pTB;

  pTA = NULL;
  pTB = NULL;
  if (a->m_Textures[EFTT_DIFFUSE])
    pTA = a->m_Textures[EFTT_DIFFUSE]->m_Sampler.m_pITex;
  if (b->m_Textures[EFTT_DIFFUSE])
    pTB = b->m_Textures[EFTT_DIFFUSE]->m_Sampler.m_pITex;
  if (pTA != pTB)
    return pTA < pTB;

  pTA = NULL;
  pTB = NULL;
  if (a->m_Textures[EFTT_BUMP])
    pTA = a->m_Textures[EFTT_BUMP]->m_Sampler.m_pITex;
  if (b->m_Textures[EFTT_BUMP])
    pTB = b->m_Textures[EFTT_BUMP]->m_Sampler.m_pITex;
  if (pTA != pTB)
    return pTA < pTB;

  pTA = NULL;
  pTB = NULL;
  if (a->m_Textures[EFTT_ENV])
    pTA = a->m_Textures[EFTT_ENV]->m_Sampler.m_pITex;
  if (b->m_Textures[EFTT_ENV])
    pTB = b->m_Textures[EFTT_ENV]->m_Sampler.m_pITex;
  if (pTA != pTB)
    return pTA < pTB;

  pTA = NULL;
  pTB = NULL;
  if (a->m_Textures[EFTT_DETAIL_OVERLAY])
    pTA = a->m_Textures[EFTT_DETAIL_OVERLAY]->m_Sampler.m_pITex;
  if (b->m_Textures[EFTT_DETAIL_OVERLAY])
    pTB = b->m_Textures[EFTT_DETAIL_OVERLAY]->m_Sampler.m_pITex;  
  return (pTA < pTB);
}

inline bool sCompareShd(CBaseResource *a, CBaseResource* b)
{
  if (!a || !b)
    return a < b;

  CShader *pA = (CShader *)a;
  CShader *pB = (CShader *)b;
  SShaderPass *pPA = NULL;
  SShaderPass *pPB = NULL;
  if (pA->m_HWTechniques.Num() && pA->m_HWTechniques[0]->m_Passes.Num())
    pPA = &pA->m_HWTechniques[0]->m_Passes[0];
  if (pB->m_HWTechniques.Num() && pB->m_HWTechniques[0]->m_Passes.Num())
    pPB = &pB->m_HWTechniques[0]->m_Passes[0];
  if (!pPA || !pPB)
    return pPA < pPB;

  if (pPA->m_VShader != pPB->m_VShader)
    return pPA->m_VShader < pPB->m_VShader;
  return (pPA->m_PShader < pPB->m_PShader);
}

void CShaderMan::mfSortResources()
{
  int i;
  for (i=0; i<MAX_TMU; i++)
  {
    gRenDev->m_RP.m_ShaderTexResources[i] = NULL;
  }
  iLog->Log("-- Presort shaders by states...");
  //return;
  std::sort(&CShader::m_ShaderResources_known.begin()[1], CShader::m_ShaderResources_known.end(), sCompareRes);

  for (i=1; i<CShader::m_ShaderResources_known.Num(); i++)
  {
    if (CShader::m_ShaderResources_known[i])
      CShader::m_ShaderResources_known[i]->m_Id = i;
  }

  SResourceContainer *pRL = CShaderMan::m_pContainer;
  assert(pRL);
  if (pRL)
  {
    std::sort(pRL->m_RList.begin(), pRL->m_RList.end(), sCompareShd);
    for (i=0; i<pRL->m_RList.size(); i++)
    {
      CShader *pSH = (CShader *)pRL->m_RList[i];
      if (pSH)
        pSH->SetID(i);
    }

    pRL->m_AvailableIDs.clear();
    for (i=0; i<pRL->m_RList.size(); i++)
    {
      CShader *pSH = (CShader *)pRL->m_RList[i];
      if (!pSH)
        pRL->m_AvailableIDs.push_back(i);
    }
  }
}
