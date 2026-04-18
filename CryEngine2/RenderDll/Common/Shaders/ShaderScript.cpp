/*=============================================================================
  ShaderScript.cpp : loading/reloading/hashing of shader scipts.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "I3DEngine.h"
#include "CryHeaders.h"

#if defined(WIN32) || defined(WIN64)
#include <direct.h>
#include <io.h>
#endif

#ifdef WIN64
#pragma warning( push )									//AMD Port
#pragma warning( disable : 4267 )
#endif

extern char *gShObjectNotFound;

//=================================================================================================

bool CShader::Reload(int nFlags, const char *szShaderName)
{
  CShader *pShaderGen = NULL;
  if (m_ShaderGenParams)
    pShaderGen = this;
  else
  if (m_pGenShader)
    pShaderGen = m_pGenShader;
  uint nFl = EF_RELOAD;
  if (nFlags & FRO_FORCERELOAD)
    nFl |= EF_FORCE_RELOAD;
  if (pShaderGen)
  {
    for (int i=0; i<pShaderGen->m_DerivedShaders->Num(); i++)
    {
      CShader *pShader = pShaderGen->m_DerivedShaders->Get(i);
      if (!pShader)
        continue;
      if (pShader->m_nRefreshFrame == gRenDev->m_cEF.m_nFrameForceReload)
        continue;
      pShader->m_nRefreshFrame = gRenDev->m_cEF.m_nFrameForceReload;

      gRenDev->m_cEF.mfForName(szShaderName, pShader->m_Flags | nFl, NULL, pShader->m_nMaskGenFX);
    }
  }
  else
  {
    assert(!m_nMaskGenFX);
    gRenDev->m_cEF.mfForName(szShaderName, m_Flags | nFl, NULL, m_nMaskGenFX);
  }

  return true;
}

bool CShaderMan::mfReloadAllShaders(int nFlags, uint nFlagsHW)
{
  bool bState = true;
  m_nFrameForceReload++;

  CHWShader::mfFlushPendedShaders();

#ifndef NULL_RENDERER
  CCryName Name = CShader::mfGetClassName();
  SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
  if (pRL)
  {
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
    {
      CShader *pS = (CShader *)itor->second;
      if (!pS)
        continue;
      if (nFlagsHW)
      {
        if (!pS->m_pGenShader)
          continue;
        SShaderGen *pGen = pS->m_pGenShader->m_ShaderGenParams;
        assert(pGen);
        if (!pGen)
          continue;
        int i;
        SShaderGenBit *pBit;
        for (i=0; i<pGen->m_BitMask.Num(); i++)
        {
          pBit = pGen->m_BitMask[i];
          if (pBit->m_nDependencySet & nFlagsHW)
            break;
        }
        if (i == pGen->m_BitMask.Num())
          continue;
        pS->Reload(nFlags, pS->GetName());
      }
      else
      {
        char name[256];
        sprintf(name, "%sCryFX/%s.cfx", m_ShadersPath, pS->GetName());
        FILE *fp = iSystem->GetIPak()->FOpen(name, "rb");
        if (fp)
        {
          uint64 nTime = iSystem->GetIPak()->GetModificationTime(fp);
          iSystem->GetIPak()->FClose(fp);
          if ((nFlags & FRO_FORCERELOAD) || nTime != pS->m_ModifTime)
          {
            pS->m_ModifTime = nTime;
            pS->Reload(nFlags, pS->GetName());
          }
        }
      }
    }
  }
#endif

  return bState;
}

static bool sCheckAffecting_r(SShaderBin *pBin, const char *szShaderName)
{
  int nTok = 0;
  // Check first level
  while (nTok >= 0)
  {
    nTok = CParserBin::FindToken(nTok, pBin->m_Tokens.size()-1, &pBin->m_Tokens[0], eT_include);
    if (nTok >= 0)
    {
      nTok++;
      uint32 nTokName = pBin->m_Tokens[nTok];
      const char *szNameInc = CParserBin::GetString(nTokName, pBin->m_Table);
      if (!stricmp(szNameInc, szShaderName))
        break;
    }
  }
  // Check recursively
  if (nTok < 0)
  {
    nTok = 0;
    while (nTok >= 0)
    {
      nTok = CParserBin::FindToken(nTok, pBin->m_Tokens.size()-1, &pBin->m_Tokens[0], eT_include);
      if (nTok >= 0)
      {
        nTok++;
        uint32 nTokName = pBin->m_Tokens[nTok];
        const char *szNameInc = CParserBin::GetString(nTokName, pBin->m_Table);
        if (!stricmp(szNameInc, szShaderName))
          break;
        SShaderBin *pBinIncl = gRenDev->m_cEF.m_Bin.GetBinShader(szNameInc, true);
        bool bAffect = sCheckAffecting_r(pBinIncl, szShaderName);
        if (bAffect)
          break;
      }
    }

  }
  return (nTok >= 0);
}

bool CShaderMan::mfReloadFile(const char *szPath, const char *szName, int nFlags)
{
  CHWShader::mfFlushPendedShaders();

  m_nFrameForceReload++;

  const char *szExt = fpGetExtension(szName);
  if (!stricmp(szExt, ".cfx"))
  {
    m_bReload = true;
    char szShaderName[256];
    int nSize = szExt-szName;
    strncpy(szShaderName, szName, nSize);
    szShaderName[nSize] = 0;
    strlwr(szShaderName);

    // Check if this shader already loaded
    CBaseResource *pBR = CBaseResource::GetResource(CShader::mfGetClassName(), szShaderName, false);
    if (pBR)
    {
      CShader *pShader = (CShader *)pBR;
      pShader->Reload(nFlags, szShaderName);
    }
    m_bReload = false;
  }
  else
  if (!stricmp(szExt, ".cfi"))
  {
    CCryName Name = CShader::mfGetClassName();
    SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
    if (pRL)
    {
      m_bReload = true;
      char szShaderName[256];
      int nSize = szExt-szName;
      strncpy(szShaderName, szName, nSize);
      szShaderName[nSize] = 0;
      strlwr(szShaderName);
      SShaderBin *pBin = m_Bin.GetBinShader(szShaderName, true);
      bool bAffect = false;

      ResourcesMapItor itor;
      for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
      {
        CShader *sh = (CShader *)itor->second;
        if (!sh || !sh->GetName()[0])
          continue;
        SShaderBin *pBin = m_Bin.GetBinShader(sh->GetName(), false);
        if (!pBin)
          continue;
        bAffect = sCheckAffecting_r(pBin, szShaderName);
        if (bAffect)
          sh->Reload(nFlags | FRO_FORCERELOAD, sh->GetName());
      }
    }
    m_bReload = false;
  }


  return false;
}

//=============================================================================

void CShaderMan::mfFillGenMacroses(SShaderGen *shG, TArray<char>& buf, uint nMaskGen)
{
  uint i;

  if (!nMaskGen || !shG)
    return;

  for (i=0; i<shG->m_BitMask.Num(); i++)
  {
    if (shG->m_BitMask[i]->m_Mask & nMaskGen)
    {
      char macro[256];
#if defined(__GNUC__)
      sprintf(macro, "#define %s 0x%llx\n", shG->m_BitMask[i]->m_ParamName.c_str(), (unsigned long long)shG->m_BitMask[i]->m_Mask);
#else
      sprintf(macro, "#define %s 0x%I64x\n", shG->m_BitMask[i]->m_ParamName.c_str(), (uint64)shG->m_BitMask[i]->m_Mask);
#endif
      int size = strlen(macro);
      int nOffs = buf.Num();
      buf.Grow(size);
      memcpy(&buf[nOffs], macro, size);
    }
  }
}
bool CShaderMan::mfModifyGenFlags(CShader *efGen, const SRenderShaderResources *pRes, uint& nMaskGen, uint& nMaskGenH)
{
  if (!efGen || !efGen->m_ShaderGenParams)
    return false;
  uint nAndMaskHW = -1;
  uint nMaskGenHW = 0;

  // Remove non-used flags first
  unsigned int i = 0;
  SShaderGen *pGen = efGen->m_ShaderGenParams;
  int j;
  for (j=0; j<32; j++)
  {
    uint nMask = (1<<j);
    if (nMaskGen & nMask)
    {
      for (i=0; i<pGen->m_BitMask.Num(); i++)
      {
        SShaderGenBit *pBit = pGen->m_BitMask[i];
        if (pBit->m_Mask & nMask)
          break;
      }
      if (i == pGen->m_BitMask.Num())
      {
        nMaskGen &= ~nMask;
      }
    }
  }
  for (i=0; i<pGen->m_BitMask.Num(); i++)
  {
    SShaderGenBit *pBit = pGen->m_BitMask[i];
    if (!pBit || (!pBit->m_nDependencySet && !pBit->m_nDependencyReset))
      continue;
    if (pRes)
    {
      if (pBit->m_nDependencySet & SHGD_TEX_BUMP)
      {
        if (!pRes->IsEmpty(EFTT_BUMP) || !pRes->IsEmpty(EFTT_NORMALMAP))
          nMaskGen |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_TEX_BUMP)
      {
        if (pRes->IsEmpty(EFTT_BUMP) && pRes->IsEmpty(EFTT_NORMALMAP))
          nMaskGen &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_TEX_BUMPDIF)
      {
        if (!pRes->IsEmpty(EFTT_BUMP_DIFFUSE))
          nMaskGen |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_TEX_BUMPDIF)
      {
        if (pRes->IsEmpty(EFTT_BUMP_DIFFUSE))
          nMaskGen &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_TEX_GLOSS)
      {
        if (!pRes->IsEmpty(EFTT_GLOSS))
          nMaskGen |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_TEX_GLOSS)
      {
        if (pRes->IsEmpty(EFTT_GLOSS))
          nMaskGen &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_TEX_ENVCM)
      {
        if (!pRes->IsEmpty(EFTT_ENV))
          nMaskGen |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_TEX_ENVCM)
      {
        if (pRes->IsEmpty(EFTT_ENV))
          nMaskGen &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_TEX_OPACITY)
      {
        if (!pRes->IsEmpty(EFTT_OPACITY))
          nMaskGen |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_TEX_OPACITY)
      {
        if (pRes->IsEmpty(EFTT_OPACITY))
          nMaskGen &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_TEX_SUBSURFACE)
      {
        if (!pRes->IsEmpty(EFTT_SUBSURFACE))
          nMaskGen |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_TEX_SUBSURFACE)
      {
        if (pRes->IsEmpty(EFTT_SUBSURFACE))
          nMaskGen &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_TEX_DECAL)
      {
        if (!pRes->IsEmpty(EFTT_DECAL_OVERLAY))
          nMaskGen |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_TEX_DECAL)
      {
        if (pRes->IsEmpty(EFTT_DECAL_OVERLAY))
          nMaskGen &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_TEX_CUSTOM)
      {
        if (!pRes->IsEmpty(EFTT_CUSTOM))
          nMaskGen |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_TEX_CUSTOM)
      {
        if (pRes->IsEmpty(EFTT_CUSTOM))
          nMaskGen &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_TEX_CUSTOM_SECONDARY)
      {
        if (!pRes->IsEmpty(EFTT_CUSTOM_SECONDARY))
          nMaskGen |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_TEX_CUSTOM_SECONDARY)
      {
        if (pRes->IsEmpty(EFTT_CUSTOM_SECONDARY))
          nMaskGen &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_LM_DIFFUSE)
      {
        const ColorF *pMt = (ColorF *)&pRes->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL];
        if (pMt->Luminance() >= 0.05f)
          nMaskGen |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_LM_DIFFUSE)
      {
        const ColorF *pMt = (ColorF *)&pRes->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL];
        if (pMt && pMt->Luminance() < 0.05f)
          nMaskGen &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencyReset & SHGD_LM_SPECULAR)
      {
        const ColorF *pMt = (ColorF *)&pRes->m_Constants[eHWSC_Pixel][PS_SPECULAR_COL];
        if (pMt && pMt->Luminance() >= 0.05f)
          nMaskGen |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_LM_SPECULAR)
      {
        const ColorF *pMt = (ColorF *)&pRes->m_Constants[eHWSC_Pixel][PS_SPECULAR_COL];
        if (pMt && pMt->Luminance() < 0.05f)
          nMaskGen &= ~pBit->m_Mask;
      }
    }
    if (m_nCombinations < 0 || m_bActivatePhase)
    {
      if (pBit->m_nDependencySet & SHGD_HW_BILINEARFP16)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (gRenDev->m_bDeviceSupportsFP16Filter)
          nMaskGenHW |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_HW_BILINEARFP16)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (!gRenDev->m_bDeviceSupportsFP16Filter)
          nMaskGenHW &= ~pBit->m_Mask;
      }
      if (pBit->m_nDependencySet & SHGD_HW_SEPARATEFP16)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (gRenDev->m_bDeviceSupportsFP16Separate)
          nMaskGenHW |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_HW_SEPARATEFP16)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (!gRenDev->m_bDeviceSupportsFP16Separate)
          nMaskGenHW &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_HW_STAT_BRANCHING)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (CRenderer::CV_r_shadersstaticbranching)
          nMaskGenHW |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_HW_STAT_BRANCHING)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (!CRenderer::CV_r_shadersstaticbranching)
          nMaskGenHW &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_HW_DYN_BRANCHING)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (CRenderer::CV_r_shadersdynamicbranching)
          nMaskGenHW |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_HW_DYN_BRANCHING)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (!CRenderer::CV_r_shadersdynamicbranching)
          nMaskGenHW &= ~pBit->m_Mask;
      }

      bool bDynamicBranching = (gRenDev->GetFeatures() & (RFT_HW_PS30|RFT_HW_PS40)) != 0;
      if (pBit->m_nDependencySet & SHGD_HW_DYN_BRANCHING_POSTPROCESS)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (bDynamicBranching)
          nMaskGenHW |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_HW_DYN_BRANCHING_POSTPROCESS)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (!bDynamicBranching)
          nMaskGenHW &= ~pBit->m_Mask;
      }

			bool usePOM = CRenderer::CV_r_usepom != 0 && (gRenDev->GetFeatures() & (RFT_HW_PS30|RFT_HW_PS40)) != 0;
      if (pBit->m_nDependencySet & SHGD_HW_ALLOW_POM)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (usePOM)
          nMaskGenHW |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_HW_ALLOW_POM)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (!usePOM)
          nMaskGenHW &= ~pBit->m_Mask;
      }

      if (pBit->m_nDependencySet & SHGD_HW_SM30)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if ((gRenDev->GetFeatures() & (RFT_HW_PS30|RFT_HW_PS40))) //set for both sm3&sm4 for now
          nMaskGenHW |= pBit->m_Mask;
      }
      if (pBit->m_nDependencyReset & SHGD_HW_SM30)
      {
        nAndMaskHW &= ~pBit->m_Mask;
        if (!(gRenDev->GetFeatures() & (RFT_HW_PS30|RFT_HW_PS40))) //set for both sm3&sm4 for now
          nMaskGenHW &= ~pBit->m_Mask;
      }
    }
  }
  nMaskGen &= nAndMaskHW;
  nMaskGenH |= nMaskGenHW;

  return true;
}

CShader *CShaderMan::mfForName (const char *nameSh, int flags, const SRenderShaderResources *Res, uint nMaskGen)
{
  CShader *ef, *efGen, *ef1;
  int id;

  if (!nameSh || !nameSh[0])
  {
    Warning("Warning: CShaderMan::mfForName: NULL name\n");
    m_DefaultShader->AddRef();
    return m_DefaultShader;
  }

	char nameEf[256];
  char nameNew[256];
  char nameRes[256];

  uint nMaskGenHW = 0;

  if (!nMaskGen)
    nMaskGen = mfShaderNameForAlias(nameSh, nameEf, 256, nMaskGen);
  else
    strcpy(nameEf, nameSh);

  strcpy(nameRes, nameEf);
  if (CParserBin::m_bD3D10)
    strcat(nameRes, "(X)");

  ef = NULL;
  efGen = NULL;
  ef1 = NULL;

  // Check if this shader already loaded
  CBaseResource *pBR = CBaseResource::GetResource(CShader::mfGetClassName(), nameRes, false);
  bool bGenModified = false;
  CShader *pShFound = (CShader *)pBR;
  if (pShFound && pShFound->m_ShaderGenParams)
  {
    efGen = pShFound;
    mfModifyGenFlags(efGen, Res, nMaskGen, nMaskGenHW);
    bGenModified = true;
    ef = efGen;
    sprintf(nameNew, "%s(%x)", nameRes, nMaskGen);
    pBR = CBaseResource::GetResource(CShader::mfGetClassName(), nameNew, false);
    pShFound = (CShader *)pBR;

		if (pBR)
    {
      // in case shader resources passed, check if flags match
      assert(pShFound->m_nMaskGenFX == (nMaskGen | nMaskGenHW));
      assert(pShFound->m_pGenShader == efGen);
    }
  }

  if (pShFound && !(flags & EF_RELOAD))
  {
    pShFound->AddRef();
    pShFound->m_Flags |= flags;
    return pShFound;
  }

	if (pShFound)
    ef = pShFound;
  if (ef && (flags & EF_RELOAD))
  {
    if (ef->m_ShaderGenParams)
      return ef;
    ef->mfFree();
    ef->m_Flags |= EF_RELOADED;
  }

  if (!ef)
  {
    ef = mfNewShader(nameRes);
    if (!ef)
      return m_DefaultShader;
  }

  if (!efGen)
  {
    sprintf(nameNew, "Shaders/%s.ext", nameEf);
    SShaderGen *pShGen = mfCreateShaderGenInfo(nameNew);
    if (pShGen)
    {
      efGen = ef;
      ef->m_ShaderGenParams = pShGen;
    }
  }
  if (!(flags & EF_RELOAD))
  {
    if (efGen)
    {
      // Change gen flags based on dependency on resources info
      if (!bGenModified)
        mfModifyGenFlags(efGen, Res, nMaskGen, nMaskGenHW);

      sprintf(nameNew, "%s(%x)", nameRes, nMaskGen);
      ef = mfNewShader(nameNew);
      if (!ef)
        return m_DefaultShader;

      ef->m_nMaskGenFX = nMaskGen | nMaskGenHW;
      ef->m_pGenShader = efGen;
      efGen->AddRef();
    }
    if (efGen && ef)
    {
      assert(efGen != ef);
      if (!efGen->m_DerivedShaders)
        efGen->m_DerivedShaders = new TArray<CShader *>;
      efGen->m_DerivedShaders->AddElem(ef);
    }
  }
  id = ef->GetID();
  ef->m_NameShader = nameEf;

#ifndef NULL_RENDERER
  // Check for the new cryFX format
 #ifdef USE_FX
  if (!strnicmp(nameEf, "Composer::", 10))
  {
    char s[256];
    strcpy(s, &nameEf[10]);
    sprintf(nameNew, "%sComposer/Graph/%s.cgr", m_ShadersPath, s);
    FILE *fp = iSystem->GetIPak()->FOpen(nameNew, "rb");
    if (fp)
    {
      iSystem->GetIPak()->FSeek(fp, 0, SEEK_END);
      int len = iSystem->GetIPak()->FTell(fp);
      TArray<char> custMacros;
      if (efGen && efGen->m_ShaderGenParams)
        mfFillGenMacroses(efGen->m_ShaderGenParams, custMacros, nMaskGen | nMaskGenHW);
      int size = custMacros.Num();
      char *buf = new char [len+size+1];
      if (!buf)
      {
        iSystem->GetIPak()->FClose(fp);
        Warning( "Error: Can't allocate %d bytes for shader file '%s'\n", len+1, nameNew);
        return NULL;
      }
      memcpy(buf, custMacros.begin(), custMacros.Num());
      iSystem->GetIPak()->FSeek(fp, 0, SEEK_SET);
      len = iSystem->GetIPak()->FRead(&buf[size], len, fp);
      ef->m_ModifTime = iSystem->GetIPak()->GetModificationTime(fp);
      iSystem->GetIPak()->FClose(fp);
      buf[len+size] = 0;
      RemoveCR(buf);
      ef->m_NameFile = nameNew;
      ef1 = m_GR.ParseGraph(buf, ef, efGen, nMaskGen | nMaskGenHW);
      SAFE_DELETE_ARRAY(buf);
    }
  }
  else
  {
    sprintf(nameNew, "%sCryFX/%s.cfx", m_ShadersPath, nameEf);
    SShaderBin *pBin = m_Bin.GetBinShader(nameEf, false);
    ef->m_NameFile = nameNew;
    if (pBin)
    {
      if (flags & EF_FORCE_RELOAD)
        pBin->UpdateCRC(true);
      m_Bin.ParseBinFX(pBin, ef, efGen, nMaskGen | nMaskGenHW);
      ef1 = ef;
    }
  }
 #endif
#endif
  if (ef1 == ef)
  {
    ef->m_Flags |= flags;
    return ef;
  }

  ef->m_Flags |= EF_NOTFOUND;
  
  return ef;
}


