/*=============================================================================
  ShaderTemplate.cpp : implementation of the Shaders templates support.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "I3DEngine.h"
#include "CryHeaders.h"
#include "StringUtils.h"								// stristr()

#if defined(WIN32) || defined(WIN64)
#include <direct.h>
#include <io.h>
#elif defined(LINUX)

#endif

//===============================================================================

void SRenderShaderResources::ConvertToInputResource(SInputShaderResources *pDst)
{
  pDst->m_ResFlags = m_ResFlags;
  pDst->m_AlphaRef = m_AlphaRef;
  if (m_pDeformInfo)
    pDst->m_DeformInfo = *m_pDeformInfo;
  else
    pDst->m_DeformInfo.m_eType = eDT_Unknown;
  if (m_Constants[eHWSC_Pixel].size())
  {
    ColorF *pSrc = (ColorF *)&m_Constants[eHWSC_Pixel][0];
    pDst->m_LMaterial.m_Diffuse = pSrc[PS_DIFFUSE_COL];
    pDst->m_LMaterial.m_Specular = pSrc[PS_SPECULAR_COL];
    pDst->m_LMaterial.m_Emission = pSrc[PS_EMISSIVE_COL];
    pDst->m_LMaterial.m_SpecShininess = pSrc[PS_SPECULAR_COL][3];
    pDst->m_Opacity = pSrc[PS_DIFFUSE_COL][3];
    pDst->m_GlowAmount = pSrc[PS_EMISSIVE_COL][3];
  }

  pDst->m_TexturePath = m_TexturePath;
  if (m_pDeformInfo)
    pDst->m_DeformInfo = *m_pDeformInfo;
  for (int i=0; i<EFTT_MAX; i++)
  {
    if (m_Textures[i])
    {
      pDst->m_Textures[i] = *m_Textures[i];
    }
    else
    {
      pDst->m_Textures[i].Reset();
    }
  }
}

void SRenderShaderResources::ExportModificators(IRenderShaderResources *pT, CRenderObject *pObj)
{
  SRenderShaderResources *pTrg = (SRenderShaderResources *)pT;
  if (m_Constants[eHWSC_Vertex].size() > PS_SPECULAR_COL)
  {
    SRenderObjData *pD = pObj->GetObjData(true);
    int nSize = max(m_Constants[eHWSC_Vertex].size(), pTrg->m_Constants[eHWSC_Vertex].size());
    pD->m_Constants.resize(nSize);
    for (int i=0; i<nSize; i++)
    {
      if ((i <= PS_SPECULAR_COL || i >= 4) && i < pTrg->m_Constants[eHWSC_Vertex].size())
        pD->m_Constants[i] = pTrg->m_Constants[eHWSC_Vertex][i];
      else
      if (i < m_Constants[eHWSC_Vertex].size())
        pD->m_Constants[i] = m_Constants[eHWSC_Vertex][i];
    }
  }
}

void SRenderShaderResources::SetShaderParams(SInputShaderResources *pDst, IShader *pSH)
{
  ReleaseParams();
  m_ShaderParams = pDst->m_ShaderParams;

#ifdef USE_PER_MATERIAL_PARAMS
  assert(pSH);
  UpdateConstants((CShader *)pSH);
#endif
}

SRenderShaderResources::~SRenderShaderResources()
{
  int i;

  for (i=0; i<EFTT_MAX; i++)
  {
    SAFE_DELETE(m_Textures[i]);
  }
  SAFE_DELETE(m_pDeformInfo);
  ReleaseConstants();

  CShader::m_ShaderResources_known[m_Id] = NULL;
}

SRenderShaderResources::SRenderShaderResources(SInputShaderResources *pSrc)
{
  Reset();
  m_szMaterialName = pSrc->m_szMaterialName;
  m_TexturePath = pSrc->m_TexturePath;
  m_ResFlags = pSrc->m_ResFlags;
  m_AlphaRef = pSrc->m_AlphaRef;
  m_PostEffects = pSrc->m_PostEffects;
  m_ShaderParams = pSrc->m_ShaderParams;
  if (pSrc->m_DeformInfo.m_eType)
  {
    m_pDeformInfo = new SDeformInfo;
    *m_pDeformInfo = pSrc->m_DeformInfo;
  }

  SetInputLM(pSrc->m_LMaterial);
  Vec4 *pDst = &m_Constants[eHWSC_Pixel][0];
  pDst[PS_DIFFUSE_COL][3] = pSrc->m_Opacity;
  pDst[PS_EMISSIVE_COL][3] = pSrc->m_GlowAmount;

  for (int i=0; i<EFTT_MAX; i++)
  {
    if (pSrc && (!pSrc->m_Textures[i].m_Name.empty() || pSrc->m_Textures[i].m_Sampler.m_pTex))
    {
      if (!m_Textures[i])
        AddTextureMap(i);
      *m_Textures[i] = pSrc->m_Textures[i];
    }
    else
    {
      if (m_Textures[i])
        m_Textures[i]->Reset();
      m_Textures[i] = NULL;
    }
  }
}

SRenderShaderResources& SRenderShaderResources::operator=(const SRenderShaderResources& src)
{
  SBaseShaderResources::operator = (src);
  int i;
  for (i=0; i<EFTT_MAX; i++)
  {
    if (!src.m_Textures[i])
      continue;
    AddTextureMap(i);
    *m_Textures[i] = *src.m_Textures[i];
  }
  for (i=0; i<2; i++)
  {
    m_Constants[i] = src.m_Constants[i];
  }
  return *this;
}

SRenderShaderResources *SRenderShaderResources::Clone()
{
  SRenderShaderResources *pSR = new SRenderShaderResources();
  *pSR = *this;
  pSR->m_nRefCounter = 1;

	for (int i=0; i<CShader::m_ShaderResources_known.Num(); i++)
	{
		if (!CShader::m_ShaderResources_known[i])
		{
			pSR->m_Id = i;
			CShader::m_ShaderResources_known[i] = pSR;
			return pSR;
		}
	}

  if (CShader::m_ShaderResources_known.Num() >= MAX_REND_SHADER_RES)
  {
    Warning("ERROR: CShaderMan::mfCreateShaderResources: MAX_REND_SHADER_RESOURCES hit");
    return CShader::m_ShaderResources_known[1];
  }
  pSR->m_Id = CShader::m_ShaderResources_known.Num();
  CShader::m_ShaderResources_known.AddElem(pSR);

  return pSR;
}

void SRenderShaderResources::SetInputLM(const CInputLightMaterial& lm)
{
  if (!m_Constants[eHWSC_Pixel].size())
    m_Constants[eHWSC_Pixel].resize(NUM_PS_PM);

  ColorF *pDst = (ColorF *)&m_Constants[eHWSC_Pixel][0];
  pDst[PS_DIFFUSE_COL].r = lm.m_Diffuse.r;
  pDst[PS_DIFFUSE_COL].g = lm.m_Diffuse.g;
  pDst[PS_DIFFUSE_COL].b = lm.m_Diffuse.b;
	pDst[PS_DIFFUSE_COL].a = 0.0f;

  pDst[PS_SPECULAR_COL] = lm.m_Specular;
  pDst[PS_SPECULAR_COL][3] = lm.m_SpecShininess ? lm.m_SpecShininess : 1;

  pDst[PS_EMISSIVE_COL].r = lm.m_Emission.r;
  pDst[PS_EMISSIVE_COL].g = lm.m_Emission.g;
  pDst[PS_EMISSIVE_COL].b = lm.m_Emission.b;
	pDst[PS_EMISSIVE_COL].a = 0.0f;

  if (!m_Constants[eHWSC_Vertex].size())
    m_Constants[eHWSC_Vertex].resize(NUM_VS_PM);
  pDst = (ColorF *)&m_Constants[eHWSC_Vertex][0];
  pDst[PS_DIFFUSE_COL].r = lm.m_Diffuse.r;
  pDst[PS_DIFFUSE_COL].g = lm.m_Diffuse.g;
  pDst[PS_DIFFUSE_COL].b = lm.m_Diffuse.b;
	pDst[PS_DIFFUSE_COL].a = 0.0f;

  pDst[PS_SPECULAR_COL] = lm.m_Specular;
  pDst[PS_SPECULAR_COL][3] = lm.m_SpecShininess ? lm.m_SpecShininess : 1;

#ifdef DIRECT3D10
  if (!m_Constants[eHWSC_Geometry].size())
    m_Constants[eHWSC_Geometry].resize(NUM_GS_PM);
  pDst = (ColorF *)&m_Constants[eHWSC_Geometry][0];
  pDst[PS_DIFFUSE_COL].r = lm.m_Diffuse.r;
  pDst[PS_DIFFUSE_COL].g = lm.m_Diffuse.g;
  pDst[PS_DIFFUSE_COL].b = lm.m_Diffuse.b;

  pDst[PS_SPECULAR_COL] = lm.m_Specular;
  pDst[PS_SPECULAR_COL][3] = lm.m_SpecShininess ? lm.m_SpecShininess : 1;
#endif
}

void SRenderShaderResources::ToInputLM(CInputLightMaterial& lm)
{
  if (!m_Constants[eHWSC_Pixel].size())
    return;
  ColorF *pDst = (ColorF *)&m_Constants[eHWSC_Pixel][0];
  lm.m_Diffuse = pDst[PS_DIFFUSE_COL];
  lm.m_Specular = pDst[PS_SPECULAR_COL];
  lm.m_Emission = pDst[PS_EMISSIVE_COL];
  lm.m_SpecShininess = pDst[PS_SPECULAR_COL][3];
}

static ColorF sColBlack = Col_Black;

ColorF& SRenderShaderResources::GetDiffuseColor()
{
  if (!m_Constants[eHWSC_Pixel].size())
    return sColBlack;
  ColorF *pDst = (ColorF *)&m_Constants[eHWSC_Pixel][0];
  return pDst[PS_DIFFUSE_COL];
}

ColorF& SRenderShaderResources::GetSpecularColor()
{
  if (!m_Constants[eHWSC_Pixel].size())
    return sColBlack;
  ColorF *pDst = (ColorF *)&m_Constants[eHWSC_Pixel][0];
  return pDst[PS_SPECULAR_COL];
}

ColorF& SRenderShaderResources::GetEmissiveColor()
{
  if (!m_Constants[eHWSC_Pixel].size())
    return sColBlack;
  ColorF *pDst = (ColorF *)&m_Constants[eHWSC_Pixel][0];
  return pDst[PS_EMISSIVE_COL];
}

float& SRenderShaderResources::GetSpecularShininess()
{
  if (!m_Constants[eHWSC_Pixel].size())
    return sColBlack.a;
  ColorF *pDst = (ColorF *)&m_Constants[eHWSC_Pixel][0];
  return pDst[PS_SPECULAR_COL][3];
}

SRenderShaderResources *CShaderMan::mfCreateShaderResources(const SInputShaderResources *Res, bool bShare)
{
  uint i, j;

  SInputShaderResources RS;
  RS = *Res;

  for (i=0; i<EFTT_MAX; i++)
  {
    RS.m_Textures[i].m_Sampler.m_pTex = NULL;
    if (RS.m_Textures[i].m_TexFlags & TEXMAP_NOMIPMAP)
      RS.m_Textures[i].m_Sampler.m_nFlags |= FSAMP_NOMIPS;

    if (i == EFTT_DETAIL_OVERLAY && !RS.m_Textures[i].m_Name.empty())
    {
      RS.m_Textures[i].m_Sampler.m_pTex = mfLoadResourceTexture(RS.m_Textures[i].m_Name.c_str(), RS.m_TexturePath.c_str(), RS.m_Textures[i].m_Sampler.GetTexFlags(), eTT_2D, &RS.m_Textures[i], RS.m_Textures[i].m_Amount);
      if (!RS.m_Textures[i].m_Sampler.m_pTex->IsTextureLoaded())
      {
        TextureWarning( RS.m_Textures[i].m_Name.c_str(),"Error: CShaderMan::mfCreateShaderResources: Couldn't find detail texture\n'%s' in path '%s'\n", RS.m_Textures[i].m_Name.c_str(), RS.m_TexturePath.c_str());
        RS.m_Textures[i].m_Sampler.m_pTex = mfLoadResourceTexture("Textures/Defaults/replaceme", RS.m_TexturePath.c_str(), RS.m_Textures[i].m_Sampler.GetTexFlags(), eTT_2D, &RS.m_Textures[i], RS.m_Textures[i].m_Amount);
      }
    }

    if (i == EFTT_DECAL_OVERLAY && !RS.m_Textures[i].m_Name.empty())
    {
      RS.m_Textures[i].m_Sampler.m_pTex = mfLoadResourceTexture(RS.m_Textures[i].m_Name.c_str(), RS.m_TexturePath.c_str(), RS.m_Textures[i].m_Sampler.GetTexFlags(), eTT_2D, &RS.m_Textures[i], RS.m_Textures[i].m_Amount);
      if (!RS.m_Textures[i].m_Sampler.m_pTex->IsTextureLoaded())
        TextureWarning( RS.m_Textures[i].m_Name.c_str(),"Error: CShaderMan::mfCreateShaderResources: Couldn't find decal texture\n'%s' in path '%s'\n", RS.m_Textures[i].m_Name.c_str(), RS.m_TexturePath.c_str());
    }

    if (i == EFTT_SUBSURFACE && !RS.m_Textures[i].m_Name.empty())
    {
      RS.m_Textures[i].m_Sampler.m_pTex = mfLoadResourceTexture(RS.m_Textures[i].m_Name.c_str(), RS.m_TexturePath.c_str(), RS.m_Textures[i].m_Sampler.GetTexFlags(), eTT_2D, &RS.m_Textures[i], RS.m_Textures[i].m_Amount);
      if (!RS.m_Textures[i].m_Sampler.m_pTex->IsTextureLoaded())
        TextureWarning( RS.m_Textures[i].m_Name.c_str(),"Error: CShaderMan::mfCreateShaderResources: Couldn't find subsurface texture\n'%s' in path '%s'\n", RS.m_Textures[i].m_Name.c_str(), RS.m_TexturePath.c_str());
    }
    
    if (i == EFTT_CUSTOM && !RS.m_Textures[i].m_Name.empty())
    {
      RS.m_Textures[i].m_Sampler.m_pTex = mfLoadResourceTexture(RS.m_Textures[i].m_Name.c_str(), RS.m_TexturePath.c_str(), RS.m_Textures[i].m_Sampler.GetTexFlags(), eTT_2D, &RS.m_Textures[i], RS.m_Textures[i].m_Amount);      
      if (!RS.m_Textures[i].m_Sampler.m_pTex->IsTextureLoaded())
        TextureWarning( RS.m_Textures[i].m_Name.c_str(),"Error: CShaderMan::mfCreateShaderResources: Couldn't find custom texture\n'%s' in path '%s'\n", RS.m_Textures[i].m_Name.c_str(), RS.m_TexturePath.c_str());
    }    

    if (i == EFTT_CUSTOM_SECONDARY && !RS.m_Textures[i].m_Name.empty())
    {
      RS.m_Textures[i].m_Sampler.m_pTex = mfLoadResourceTexture(RS.m_Textures[i].m_Name.c_str(), RS.m_TexturePath.c_str(), RS.m_Textures[i].m_Sampler.GetTexFlags(), eTT_2D, &RS.m_Textures[i], RS.m_Textures[i].m_Amount);      
      if (!RS.m_Textures[i].m_Sampler.m_pTex->IsTextureLoaded())
        TextureWarning( RS.m_Textures[i].m_Name.c_str(),"Error: CShaderMan::mfCreateShaderResources: Couldn't find custom secondary texture\n'%s' in path '%s'\n", RS.m_Textures[i].m_Name.c_str(), RS.m_TexturePath.c_str());
    }    

  }

  int nFree = -1;
  for (i=1; i<CShader::m_ShaderResources_known.Num(); i++)
  {
    SRenderShaderResources *pSR = CShader::m_ShaderResources_known[i];
    if (!pSR)
    {
      nFree = i;
      if (!bShare || Res->m_ShaderParams.size())
        break;
      continue;
    }
    if (!bShare || Res->m_ShaderParams.size())
      continue;
    if (RS.m_ResFlags == pSR->m_ResFlags && RS.m_Opacity == pSR->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][3] && 
				RS.m_GlowAmount == pSR->Glow() &&
				RS.m_AlphaRef == pSR->m_AlphaRef && !stricmp(RS.m_TexturePath.c_str(), pSR->m_TexturePath.c_str()))
    {
      if ((!pSR->m_pDeformInfo && !RS.m_DeformInfo.m_eType) || (pSR->m_pDeformInfo && *pSR->m_pDeformInfo == RS.m_DeformInfo))
      {
        for (j=0; j<EFTT_MAX; j++)
        {
          if (!pSR->m_Textures[j] || pSR->m_Textures[j]->m_Name.empty())
          {
            if (RS.m_Textures[j].m_Name.empty())
              continue;
            break;
          }
          else
          if (RS.m_Textures[j].m_Name.empty())
            break;
          if (RS.m_Textures[j] != *pSR->m_Textures[j])
            break;
        }
        if (j == EFTT_MAX)
        {
          pSR->m_nRefCounter++;
          return pSR;
        }
      }
    }
  }

  SRenderShaderResources *pSR = new SRenderShaderResources(&RS);
  pSR->m_nRefCounter = 1;
  if (!CShader::m_ShaderResources_known.Num())
  {
    CShader::m_ShaderResources_known.AddIndex(1);
    SRenderShaderResources *pSRNULL = new SRenderShaderResources;
    pSRNULL->m_nRefCounter = 1;
    CShader::m_ShaderResources_known[0] = pSRNULL;
  }
  else
  if (CShader::m_ShaderResources_known.Num() >= MAX_REND_SHADER_RES)
  {
    Warning("ERROR: CShaderMan::mfCreateShaderResources: MAX_REND_SHADER_RESOURCES hit");
    return CShader::m_ShaderResources_known[1];
  }
  if (nFree > 0)
  {
    pSR->m_Id = nFree;
    CShader::m_ShaderResources_known[nFree] = pSR;
  }
  else
  {
    pSR->m_Id = CShader::m_ShaderResources_known.Num();
    CShader::m_ShaderResources_known.AddElem(pSR);
  }

  return pSR;
}

uint CShaderMan::mfShaderNameForAlias(const char *nameAlias, char *nameEf, int nSize, uint nGenMask)
{
  strncpy(nameEf, nameAlias, nSize);
  bool bFound = CCryName::find(nameAlias);
  if (!bFound)
    return nGenMask;
  CCryName cn = CCryName(nameAlias);
  SNameAlias *na;
  uint i;
  for (i=0; i<m_AliasNames.size(); i++)
  {
    na = &m_AliasNames[i];
    if (na->m_Alias == cn)
      break;
  }
  if (i<m_AliasNames.size())
  {
    strncpy(nameEf, na->m_Name.c_str(), nSize);
    nGenMask = na->m_nGenMask;
  }

  cn = CCryName(nameEf);
  for (i=0; i<m_CustomAliasNames.size(); i++)
  {
    na = &m_CustomAliasNames[i];
    if (na->m_Alias == cn)
      break;
  }
  if (i<m_CustomAliasNames.size())
  {
    strncpy(nameEf, na->m_Name.c_str(), nSize);
    nGenMask = na->m_nGenMask;
  }
  return nGenMask;
}

void SShaderItem::PostLoad()
{
  CShader *pSH = (CShader *)m_pShader;
  if (pSH->m_Flags2 & EF2_PREPR_GENSPRITES)
    m_nPreprocessFlags |= FSPR_GENSPRITES;

  SRenderShaderResources *pR = (SRenderShaderResources *)m_pShaderResources;
  if (pR)
  {
    pR->PostLoad(pSH);
    SShaderTechnique *pTech = GetTechnique();
		//if (pTech)
		//{
		//	for (int i=0; i<pTech->m_RTargets.Num(); ++i)
		//		SAFE_DELETE(pTech->m_RTargets[i]);
		//	pTech->m_RTargets.Reset();
		//}
    for (int i=0; i<EFTT_MAX; i++)
    {
      if (!pR->m_Textures[i] || !pR->m_Textures[i]->m_Sampler.m_pTarget)
        continue;

      if (gRenDev->m_RP.m_eQuality == eRQ_Low)
      {
				STexSampler& sampler(pR->m_Textures[i]->m_Sampler);
        if (sampler.m_eTexType == eTT_AutoCube)
          sampler.m_eTexType = eTT_Cube;
        else if (sampler.m_eTexType == eTT_Auto2D && !sampler.m_pDynTexSource)
          sampler.m_eTexType = eTT_2D;
      }

      if (pR->m_Textures[i]->m_Sampler.m_eTexType == eTT_AutoCube || pR->m_Textures[i]->m_Sampler.m_eTexType == eTT_Auto2D)
        pR->m_ResFlags |= MTL_FLAG_NOTINSTANCED;
      pR->m_RTargets.AddElem(pR->m_Textures[i]->m_Sampler.m_pTarget);
      if (pR->m_Textures[i]->m_Sampler.m_eTexType == eTT_AutoCube)
        m_nPreprocessFlags |= FSPR_SCANCM;
      else
      if (pR->m_Textures[i]->m_Sampler.m_eTexType == eTT_Auto2D)
        m_nPreprocessFlags |= FSPR_SCANTEX;
    }
  }
  SShaderTechnique *pTech = GetTechnique();
  if (pTech && pTech->m_Passes.Num() && (pTech->m_Passes[0].m_RenderState & GS_ALPHATEST_MASK))
  {
    if (pR && !pR->m_AlphaRef)
      pR->m_AlphaRef = 0.5f;
  }

  if (pTech)
    m_nPreprocessFlags |= pTech->GetPreprocessFlags(pSH);
}

SShaderItem CShaderMan::mfShaderItemForName (const char *nameEf, bool bShare, int flags, const SInputShaderResources *Res, uint nMaskGen)
{
  SShaderItem SI;

  if (Res)
  {
    /*if (!strnicmp(nameEf, "terrain", 7))
    {
      int nnn = 0;
    }*/
    SI.m_pShaderResources = mfCreateShaderResources(Res, bShare);
    gRenDev->m_RP.m_pShaderResources = (SRenderShaderResources *)SI.m_pShaderResources;
    m_pCurInputResources = Res;
    /*if (SI.m_pShaderResources->m_szMaterialName && !stricmp(SI.m_pShaderResources->m_szMaterialName, "asian_tank_treads"))
    {
      int nnn = 0;
    }*/
  }

  string strShader = nameEf;
  string strTechnique;
  string strNew = strShader.SpanExcluding(".");
  if (strNew.size() != strShader.size())
  {
    string second = string(&strShader.c_str()[strNew.size()+1]);
    strShader = strNew;
    strTechnique = second;
  }

	if (SI.m_pShaderResources)
	{
		mfRefreshResources((SRenderShaderResources *)SI.m_pShaderResources,true);
	}

  char sNewShaderName[256]="";
  char shadName[256];
  if (nameEf && nameEf[0])
    nMaskGen = mfShaderNameForAlias(strShader.c_str(), shadName, 256, nMaskGen);
  else
    shadName[0] = 0;

  SI.m_pShader = mfForName(shadName, flags, (SRenderShaderResources *)SI.m_pShaderResources, nMaskGen);
  CShader *pSH = (CShader *)SI.m_pShader;

  // Get technique
  if (strTechnique.size())
  {
    uint i;
    for (i=0; i<pSH->m_HWTechniques.Num(); i++)
    {
      SShaderTechnique *pTech = pSH->m_HWTechniques[i];
      //if (!(pTech->m_Flags & FHF_PUBLIC))
      //  continue;
      if (!stricmp(pTech->m_Name.c_str(), strTechnique.c_str()))
        break;
    }
    if (i == pSH->m_HWTechniques.Num())
      Warning("ERROR: CShaderMan::mfShaderItemForName: couldn't find public technique '%s' for shader '%s'", strTechnique.c_str(), pSH->GetName());
    else
      SI.m_nTechnique = i;
  }
  SI.m_nPreprocessFlags = 0;
  if (Res)
    SI.m_pShaderResources = gRenDev->m_RP.m_pShaderResources;

  SI.PostLoad();

  gRenDev->m_RP.m_pShaderResources = NULL;
  m_pCurInputResources = NULL;
  return SI;
}


//=================================================================================================

CTexture *CShaderMan::mfCheckTemplateTexName(const char *mapname, ETEX_Type eTT, short &nFlags)
{
  CTexture *TexPic = NULL;
  if (mapname[0] != '$')
    return NULL;
  
  if (!stricmp(mapname, "$ScreenShadowMap"))
    TexPic = CTexture::m_Text_ScreenShadowMap[0];
  else
  if (!stricmp(mapname, "$Lightmap"))
    TexPic = &CTexture::m_Templates[EFTT_LIGHTMAP];
  else
  if (!stricmp(mapname, "$LightmapDirection"))
    TexPic = &CTexture::m_Templates[EFTT_LIGHTMAP_DIR];
  else
  if (!strnicmp(mapname, "$Flare", 6))
    TexPic = CTexture::m_Text_Flare;
  else
  if (!stricmp(mapname, "$LightCubemap"))
    TexPic = CTexture::m_Text_LightCMap;
  else
  if (!strnicmp(mapname, "$ShadowID", 9))
  {
    int n = atoi(&mapname[9]);
    TexPic = CTexture::m_Text_ShadowID[n];
  }
  else
  if (!stricmp(mapname, "$FromRE") || !stricmp(mapname, "$FromRE0"))
    TexPic = CTexture::m_Text_FromRE[0];
  else
  if (!stricmp(mapname, "$FromRE1"))
    TexPic = CTexture::m_Text_FromRE[1];
  else
  if (!stricmp(mapname, "$FromRE2"))
    TexPic = CTexture::m_Text_FromRE[2];
  else
  if (!stricmp(mapname, "$FromRE3"))
    TexPic = CTexture::m_Text_FromRE[3];
	else
  if (!stricmp(mapname, "$FromRE4"))
		TexPic = CTexture::m_Text_FromRE[4];
  else
	if (!stricmp(mapname, "$FromRE5"))
		TexPic = CTexture::m_Text_FromRE[5];
	else
	if (!stricmp(mapname, "$FromRE6"))
		TexPic = CTexture::m_Text_FromRE[6];
	else
	if (!stricmp(mapname, "$FromRE7"))
		TexPic = CTexture::m_Text_FromRE[7];
	else
	if (!stricmp(mapname, "$FromSF0"))
		TexPic = CTexture::m_Text_FromSF[0];
	else
	if (!stricmp(mapname, "$FromSF1"))
		TexPic = CTexture::m_Text_FromSF[1];
	else
	if (!stricmp(mapname, "$VolObj_Density"))
		TexPic = CTexture::m_Text_VolObj_Density;
	else
	if (!stricmp(mapname, "$VolObj_Shadow"))
		TexPic = CTexture::m_Text_VolObj_Shadow;
	else
  if (!stricmp(mapname, "$FromObj") || !stricmp(mapname, "$FromObj0"))
    TexPic = CTexture::m_Text_FromObj[0];
	else
	if (!stricmp(mapname, "$FromObj1"))
		TexPic = CTexture::m_Text_FromObj[1];
  else
  if (!stricmp(mapname, "$FromLight"))
    TexPic = CTexture::m_Text_FromLight;
  else
  if (!strnicmp(mapname, "$Phong", 6))
    TexPic = &CTexture::m_Templates[EFTT_PHONG];
  else
  if (!strnicmp(mapname, "$White", 6))
    TexPic = CTexture::m_Text_White;
  else
  if (!strnicmp(mapname, "$RT_2D", 6))
  {
    TexPic = CTexture::m_Text_RT_2D;
  }
  else
  if (!strnicmp(mapname, "$RT_Cube", 8) || !strnicmp(mapname, "$RT_CM", 6))
    TexPic = CTexture::m_Text_RT_CM;
  else
  if (!strnicmp(mapname, "$RT_LCube", 9) || !strnicmp(mapname, "$RT_LCM", 7))
    TexPic = CTexture::m_Text_RT_LCM;
  else
  if (!stricmp(mapname, "$Diffuse"))
    TexPic = &CTexture::m_Templates[EFTT_DIFFUSE];
  else
  if (!stricmp(mapname, "$DecalOverlay"))
    TexPic = &CTexture::m_Templates[EFTT_DECAL_OVERLAY];
  else
  if (!stricmp(mapname, "$Detail"))
    TexPic = &CTexture::m_Templates[EFTT_DETAIL_OVERLAY];
  else
  if (!stricmp(mapname, "$Opacity"))
    TexPic = &CTexture::m_Templates[EFTT_OPACITY];
  else
  if (!strnicmp(mapname, "$Specular", 9))
    TexPic = &CTexture::m_Templates[EFTT_SPECULAR];
  else
  if (!strnicmp(mapname, "$BumpPlants", 11))
  {
    TexPic = &CTexture::m_Templates[EFTT_BUMP];
    nFlags |= FSAMP_BUMPPLANTS;
  }
  else
  if (!strnicmp(mapname, "$BumpDiffuse", 12))
    TexPic = &CTexture::m_Templates[EFTT_BUMP_DIFFUSE];
  else
  if (!strnicmp(mapname, "$BumpHeight", 10))
    TexPic = &CTexture::m_Templates[EFTT_BUMP_HEIGHT];
  else
  if (!strnicmp(mapname, "$Bump", 5))
    TexPic = &CTexture::m_Templates[EFTT_BUMP];
  else
  if (!strnicmp(mapname, "$Subsurface", 11))
    TexPic = &CTexture::m_Templates[EFTT_SUBSURFACE];
  else
  if (!stricmp(mapname, "$CustomMap"))
    TexPic = &CTexture::m_Templates[EFTT_CUSTOM];
  else
  if (!stricmp(mapname, "$CustomSecondaryMap"))
    TexPic = &CTexture::m_Templates[EFTT_CUSTOM_SECONDARY];
  else
  if (!stricmp(mapname, "$Cubemap") || !strnicmp(mapname, "$Env", 4))
    TexPic = &CTexture::m_Templates[EFTT_ENV];
  else
  if (!strnicmp(mapname, "$Occlusion", 10))
    TexPic = &CTexture::m_Templates[EFTT_OCCLUSION];
  else
	if (!strnicmp(mapname, "$RAE", 4))
		TexPic = &CTexture::m_Templates[EFTT_RAE];
	else
  if (!strnicmp(mapname, "$Gloss", 6))
    TexPic = &CTexture::m_Templates[EFTT_GLOSS];
  else 
	if (!stricmp(mapname, "$GlowMap"))
		TexPic = CTexture::m_Text_Glow;
  else
  if (!stricmp(mapname, "$ScreenTexMap"))
    TexPic = CTexture::m_Text_ScreenMap;
  else
  if (!stricmp(mapname, "$CloudsShadowTex"))
  {
    CTexture* pCloudShadowTex = gRenDev->GetCloudShadowTextureId() > 0 ? CTexture::GetByID( gRenDev->GetCloudShadowTextureId() ) 
                                                                       : CTexture::m_Text_White ;
    TexPic = pCloudShadowTex;
  }
  else
  if (!stricmp(mapname, "$BackBuffer"))
    TexPic = CTexture::m_Text_BackBuffer;
  else
  if (!stricmp(mapname, "$BackBufferScaled_d2"))
    TexPic = CTexture::m_Text_BackBufferScaled[0];
  else
  if (!stricmp(mapname, "$BackBufferScaled_d4"))
    TexPic = CTexture::m_Text_BackBufferScaled[1];
  else
  if (!stricmp(mapname, "$BackBufferScaled_d8"))
    TexPic = CTexture::m_Text_BackBufferScaled[2];
  else
  if (!stricmp(mapname, "$HDR_BackBuffer"))
    TexPic = CTexture::m_Text_SceneTarget;
  else
  if (!stricmp(mapname, "$HDR_BackBufferScaled_d2"))
    TexPic = CTexture::m_Text_HDRTargetScaled[0];
  else
  if (!stricmp(mapname, "$HDR_BackBufferScaled_d4"))
    TexPic = CTexture::m_Text_HDRTargetScaled[1];
  else
  if (!stricmp(mapname, "$HDR_BackBufferScaled_d8"))
    TexPic = CTexture::m_Text_HDRTargetScaled[2];
  else
  if (!stricmp(mapname, "$ZTarget"))
    TexPic = CTexture::m_Text_ZTarget;
  else
  if (!stricmp(mapname, "$ZTargetMS"))
    TexPic = CTexture::m_Text_ZTargetMS;
	else
  if (!stricmp(mapname, "$ZTargetScaled"))
    TexPic = CTexture::m_Text_ZTargetScaled;
  else
	if (!stricmp(mapname, "$AOTarget"))
		TexPic = CTexture::m_Text_AOTarget;
  else
  if (!stricmp(mapname, "$EffectsAccum"))
    TexPic = CTexture::m_Text_EffectsAccum;
  else
  if (!stricmp(mapname, "$ScatterLayer"))
    TexPic = CTexture::m_Text_ScatterLayer;
  else
  if (!stricmp(mapname, "$SceneTarget"))
    TexPic = CTexture::m_Text_SceneTarget;
  else
  if (!stricmp(mapname, "$TerrainLM"))
    TexPic = CTexture::m_Text_TerrainLM;
  else
  if (!stricmp(mapname, "$CloudsLM"))
    TexPic = CTexture::m_Text_CloudsLM;
  else
  if (!stricmp(mapname, "$SceneTargetFSAA"))
    TexPic = CTexture::m_Text_SceneTargetFSAA;
  else
  if (!stricmp(mapname, "$WaterVolumeDDN"))
    TexPic = CTexture::m_Text_WaterVolumeDDN;


  return TexPic;
}

const char *CShaderMan::mfTemplateTexIdToName(int Id)
{
  switch(Id)
  {
  case EFTT_DIFFUSE:
    return "Diffuse";
  case EFTT_GLOSS:
    return "Gloss";
  case EFTT_BUMP:
    return "Bump";
  case EFTT_ENV:
    return "Environment";
  case EFTT_OCCLUSION:
    return "Occlusion";
  case EFTT_SUBSURFACE:
    return "SubSurface";
  case EFTT_CUSTOM:
    return "CustomMap";
  case EFTT_CUSTOM_SECONDARY:
    return "CustomSecondaryMap";
  case EFTT_SPECULAR:
    return "Specular";
  case EFTT_DETAIL_OVERLAY:
    return "Detail";
  case EFTT_OPACITY:
    return "Opacity";
  case EFTT_LIGHTMAP:
  case EFTT_LIGHTMAP_HDR:
    return "Lightmap";
  case EFTT_DECAL_OVERLAY:
    return "Decal";
  default:
    return "Unknown";
  }
  return "Unknown";
}

int CShaderMan::mfReadTexSequence(TArray<CTexture *>& tl, const char *na, byte eTT, int Flags, float fAmount1, float fAmount2)
{
  char prefix[_MAX_PATH];
  char postfix[_MAX_PATH];
  char *nm;
  int i, j, l, m;
  char nam[_MAX_PATH];
  int n;
  CTexture *tx, *tp;
  int startn, endn, nums;
  char name[_MAX_PATH];

  tx = NULL;

  strcpy(name, na);
  const char *ext = fpGetExtension (na);
  fpStripExtension(name, name);

  char chSep = '#';
  nm = strchr(name, chSep);
  if (!nm)
  {
    nm = strchr(name, '$');
    if (!nm)
      return 0;
    char chSep = '$';
  }

  float fSpeed = 0.05f;
  {
    char nName[_MAX_PATH];
    strcpy(nName, name);
    nm = strchr(nName, '(');
    if (nm)
    {
      name[nm-nName] = 0;
      char *speed = &nName[nm-nName+1];
      if(nm=strchr(speed, ')'))
        speed[nm-speed] = 0;
      fSpeed = (float)atof(speed);
    }
  }

  j = 0;
  n = 0;
  l = -1;
  m = -1;
  while (name[n])
  {
    if (name[n] == chSep)
    {
      j++;
      if (l == -1)
        l = n;
    }
    else
    if (l >= 0 && m < 0)
      m = n;
    n++;
  }
  if (!j)
    return 0;

  strncpy(prefix, name, l);
  prefix[l] = 0;
  char dig[_MAX_PATH];
  l = 0;
  if (m < 0)
  {
    startn = 0;
    endn = 999;
    postfix[0] = 0;
  }
  else
  {
    while(isdigit(name[m]))
    {
      dig[l++] = name[m];
      m++;
    }
    dig[l] = 0;
    startn = strtol(dig, NULL, 10);
    m++;

    l = 0;
    while(isdigit(name[m]))
    {
      dig[l++] = name[m];
      m++;
    }
    dig[l] = 0;
    endn = strtol(dig, NULL, 10);

    strcpy(postfix, &name[m]);
  }

  nums = endn-startn+1;

  n = 0;
  char frm[256];
  char frd[4];

  frd[0] = j + '0';
  frd[1] = 0;

  strcpy(frm, "%s%.");
  strcat(frm, frd);
  strcat(frm, "d%s%s");
  for (i=0; i<nums; i++)
  {
    sprintf(nam, frm, prefix, startn+i, postfix, ext);
    tp = (CTexture*)gRenDev->EF_LoadTexture(nam, Flags, eTT, fAmount1, fAmount2);
    if (!tp || !tp->IsLoaded())
    {
      if (tp)
        tp->Release();
      break;
    }
    tl.AddElem(tp);
    tp->m_fAnimSpeed = fSpeed;
    n++;
  }

  return n;
}


bool CShaderMan::mfCheckAnimatedSequence(STexSampler *tl, CTexture *tx)
{
  if (tl->m_pAnimInfo)
    return false;
  if (!tx)
    tx = tl->m_pTex;
  if (!tx || !tx->IsLoaded())
    return true;
  if (!tx->m_NextTxt)
    return true;

  STexAnim *ta = new STexAnim; 
  ta->m_bLoop = true;
  CTexture *tp = tx;
  while (tp)
  {
    ta->m_Time = tp->m_fAnimSpeed;

    ITexture *pTex = (ITexture *)tp;
    if(pTex)
    {
      pTex->AddRef();
    }

    ta->m_TexPics.AddElem(tp);
    tp = tp->m_NextTxt;
  }
  ta->m_NumAnimTexs = ta->m_TexPics.Num();
  tl->m_pAnimInfo = ta;
  tl->m_pTex = tx;

  return false;
}

CTexture *CShaderMan::mfTryToLoadTexture(const char *nameTex, int Flags, byte eTT, float fAmount1, float fAmount2)
{
  CTexture *tx = NULL;                    
  if (nameTex && strchr(nameTex, '#')) // test for " #" to skip max material names
  {
    TArray<CTexture *> Texs;
    int n = mfReadTexSequence(Texs, nameTex, eTT, Flags, fAmount1, fAmount2);
    if (n > 1)
    {
      CTexture *tp = NULL;
      for (uint j=0; j<Texs.Num(); j++)
      {
        CTexture *t = Texs[j];
        if (!j)
        {
          tx = t;
          t->m_NextTxt = NULL;
        }
        else
          tp->m_NextTxt = t;
        tp = t;
      }
    }
  }
  if (!tx)
  {
    tx = (CTexture *)gRenDev->EF_LoadTexture(nameTex, Flags, eTT, fAmount1, fAmount2);
    tx->m_NextTxt = NULL;
  }

  return tx;
}

CTexture *CShaderMan::mfLoadResourceTexture(const char *nameTex, const char *path, int Flags, byte eTT, SEfResTexture *Tex, float fAmount1, float fAmount2)
{
  Flags &= ~FT_FILTER_MASK;
  if (Tex)
  {
    STexState ST;
    ST.SetFilterMode(Tex->m_Filter);
    ST.SetClampMode(Tex->m_bUTile ? TADDR_WRAP : TADDR_CLAMP, Tex->m_bVTile ? TADDR_WRAP : TADDR_CLAMP, Tex->m_bUTile ? TADDR_WRAP : TADDR_CLAMP);
    Tex->m_Sampler.m_nTexState = CTexture::GetTexState(ST);
  }
  CTexture *tx = mfTryToLoadTexture(nameTex, Flags, eTT, fAmount1, fAmount2);

  if (Tex && tx && tx->IsTextureLoaded())
    mfCheckAnimatedSequence(&Tex->m_Sampler, tx);

  return tx;
}

void ProcessDDNDIFName( char *szOutName, CTexture *pTex )
{
	if(!pTex)
		return;

	if((pTex->GetFlags()&FT_FILESINGLE)!=0)		// build process can tag assets so we can avoid loading additionally files
		return;

	const char *szSrcName = pTex->GetSourceName();

	if(!szSrcName)
		return;

	const char *str = CryStringUtils::stristr(szSrcName, "_ddn");

	if(!str)
		return;

	int nSize = str - szSrcName;

	// make sure to add only 'ddndif', if texture filename hasn't got ddndif
	if(!CryStringUtils::stristr(szSrcName, "_ddndif")) 
	{
		memcpy(szOutName, szSrcName, nSize);
		memcpy(&szOutName[nSize], "_ddndif", 7);
		//                strcpy(&nameBump[nSize+7], &str[4]);
		strcpy(&szOutName[nSize+7],".tif");
	}
	else
		strcpy(szOutName, szSrcName);  

	FILE *fp = iSystem->GetIPak()->FOpen(szOutName, "rb",ICryPak::FOPEN_HINT_QUIET);

	if(!fp)
	{
		strcpy(&szOutName[nSize+7],".dds");
		fp = iSystem->GetIPak()->FOpen(szOutName, "rb",ICryPak::FOPEN_HINT_QUIET);
	}

	if(!fp)
		*szOutName = 0;
	else
		iSystem->GetIPak()->FClose(fp);
}


void CShaderMan::mfRefreshResources(SRenderShaderResources *Res, const bool bLoadNormalAlpha )
{
  int i;
  const char *patch = Res->m_TexturePath.c_str();
  SEfResTexture *Tex;
  if (Res)
  {
    for (i=0; i<EFTT_MAX; i++)
    {
      int Flags = 0;
      if (i == EFTT_NORMALMAP)
        continue;
      if (i == EFTT_BUMP)
      {
        if ((!Res->m_Textures[EFTT_BUMP] || Res->m_Textures[EFTT_BUMP]->m_Name.empty()) && (!Res->m_Textures[EFTT_NORMALMAP] || Res->m_Textures[EFTT_NORMALMAP]->m_Name.empty()))
          continue;

//				if (gRenDev->m_iDeviceSupportsComprNormalmaps)
//         Flags |= FT_ALLOW_3DC;

				Flags |= FT_TEX_NORMAL_MAP;
        if (Res->m_Textures[EFTT_BUMP] && !Res->m_Textures[EFTT_BUMP]->m_Name.empty() && Res->m_Textures[EFTT_NORMALMAP] && !Res->m_Textures[EFTT_NORMALMAP]->m_Name.empty())
        {
          char fullname[1024];
          sprintf(fullname, "%s+%s", Res->m_Textures[EFTT_BUMP]->m_Name.c_str(), Res->m_Textures[EFTT_NORMALMAP]->m_Name.c_str());
          Res->m_Textures[EFTT_BUMP]->m_Sampler.m_pTex = mfLoadResourceTexture(fullname, patch, Res->m_Textures[EFTT_BUMP]->m_Sampler.GetTexFlags() | Flags, eTT_2D, Res->m_Textures[EFTT_BUMP], (float)Res->m_Textures[EFTT_BUMP]->m_Amount, (float)Res->m_Textures[EFTT_NORMALMAP]->m_Amount);
        }
        if (!Res->m_Textures[EFTT_BUMP])
          Res->AddTextureMap(EFTT_BUMP);
        Tex = Res->m_Textures[EFTT_BUMP];
        if (!Tex->m_Sampler.m_pTex || !Tex->m_Sampler.m_pTex->IsTextureLoaded())
        {
          if (!Tex->m_Name.empty())
            Tex->m_Sampler.m_pTex = mfLoadResourceTexture(Tex->m_Name.c_str(), patch, Tex->m_Sampler.GetTexFlags() | Flags, eTT_2D, Tex, (float)Tex->m_Amount);
          if (!Tex->m_Sampler.m_pTex || !Tex->m_Sampler.m_pTex->IsTextureLoaded())
          {
            if (Res->m_Textures[EFTT_NORMALMAP])
            {
              Tex->m_Name = Res->m_Textures[EFTT_NORMALMAP]->m_Name;
							Tex->m_Sampler.m_pTex = mfLoadResourceTexture(Res->m_Textures[EFTT_NORMALMAP]->m_Name.c_str(), patch, Res->m_Textures[EFTT_NORMALMAP]->m_Sampler.GetTexFlags() | Flags, eTT_2D, Res->m_Textures[EFTT_BUMP], (float)Res->m_Textures[EFTT_NORMALMAP]->m_Amount);
              SAFE_DELETE(Res->m_Textures[EFTT_NORMALMAP]);
            }
          }
          if (Tex->m_Sampler.m_pTex && !Tex->m_Sampler.m_pTex->IsTextureLoaded())
            Tex->m_Sampler.m_pTex = CTexture::m_Text_FlatBump;
        }
        continue;
      }
      else
      if (i == EFTT_BUMP_HEIGHT)
      {
				if(!bLoadNormalAlpha)
					continue;
        if (!Res->m_Textures[EFTT_BUMP] || !Res->m_Textures[EFTT_BUMP]->m_Sampler.m_pTex)
          continue;
				// we load the alpha channel from the normal map (e.g. attached L8 for 3DC)
				CTexture *pTexN = Res->m_Textures[EFTT_BUMP]->m_Sampler.m_pTex;
				if (!(pTexN->GetFlags() & FT_HAS_ATTACHED_ALPHA))
					continue;
				Flags = FT_ALPHA;
				if (!Res->m_Textures[EFTT_BUMP_HEIGHT])
					Res->AddTextureMap(EFTT_BUMP_HEIGHT);
				Tex = Res->m_Textures[EFTT_BUMP_HEIGHT];
				if (!Tex->m_Sampler.m_pTex || !Tex->m_Sampler.m_pTex->IsTextureLoaded())
					Tex->m_Sampler.m_pTex = mfLoadResourceTexture(pTexN->GetSourceName(), patch, Tex->m_Sampler.GetTexFlags() | Flags, eTT_2D, Tex, (float)Tex->m_Amount);
        continue;
      }
      if (i == EFTT_BUMP_DIFFUSE)
      {
        char nameBump[256];
        char nameNorm[256];
        nameBump[0] = 0;
        nameNorm[0] = 0;

				if (!Res->IsEmpty(EFTT_BUMP))
					ProcessDDNDIFName(nameBump,Res->m_Textures[EFTT_BUMP]->m_Sampler.m_pTex);

				if (!Res->IsEmpty(EFTT_NORMALMAP))
					ProcessDDNDIFName(nameNorm,Res->m_Textures[EFTT_NORMALMAP]->m_Sampler.m_pTex);

				if (nameBump[0] || nameNorm[0])
        {
          Flags |= FT_TEX_NORMAL_MAP;
          if (!Res->m_Textures[EFTT_BUMP_DIFFUSE])
            Res->AddTextureMap(EFTT_BUMP_DIFFUSE);
          if (nameBump[0] && nameNorm[0])
          {
            char fullname[256];
            sprintf(fullname, "%s+norm_%s", nameBump, nameNorm);
            Res->m_Textures[EFTT_BUMP_DIFFUSE]->m_Sampler.m_pTex = mfLoadResourceTexture(fullname, patch, Res->m_Textures[EFTT_BUMP]->m_Sampler.GetTexFlags() | Flags, eTT_2D, Res->m_Textures[EFTT_BUMP_DIFFUSE], (float)Res->m_Textures[EFTT_BUMP]->m_Amount, (float)Res->m_Textures[EFTT_NORMALMAP]->m_Amount);
          }
          Tex = Res->m_Textures[EFTT_BUMP_DIFFUSE];
          if (!Tex->m_Sampler.m_pTex || !Tex->m_Sampler.m_pTex->IsTextureLoaded())
          {
            if (nameBump[0])
              Tex->m_Sampler.m_pTex = mfLoadResourceTexture(nameBump, patch, Res->m_Textures[EFTT_BUMP]->m_Sampler.GetTexFlags() | Flags, eTT_2D, Tex, (float)Res->m_Textures[EFTT_BUMP]->m_Amount);
            else
            if (nameNorm[0])
              Tex->m_Sampler.m_pTex = mfLoadResourceTexture(nameNorm, patch, Res->m_Textures[EFTT_BUMP]->m_Sampler.GetTexFlags() | Flags, eTT_2D, Tex, (float)Res->m_Textures[EFTT_BUMP]->m_Amount);
            if (!Tex->m_Sampler.m_pTex || !Tex->m_Sampler.m_pTex->IsTextureLoaded())
            {
              SAFE_RELEASE(Tex->m_Sampler.m_pTex);
            }
          }
        }
      }
      Tex = Res->m_Textures[i];
      if (!Tex)
        continue;
			
      if (!Tex->m_Sampler.m_pTex)
      {
        if (Tex->m_Sampler.m_eTexType == eTT_AutoCube || Tex->m_Sampler.m_eTexType == eTT_Auto2D)
        {
          if (i == EFTT_ENV || i == EFTT_DIFFUSE)
          {
            STexState ST;
            ST.SetFilterMode(Tex->m_Filter);
            ST.SetClampMode(Tex->m_bUTile ? TADDR_WRAP : TADDR_CLAMP, Tex->m_bVTile ? TADDR_WRAP : TADDR_CLAMP, Tex->m_bUTile ? TADDR_WRAP : TADDR_CLAMP);
            Tex->m_Sampler.m_nTexState = CTexture::GetTexState(ST);
            SAFE_RELEASE(Tex->m_Sampler.m_pTarget);
            Tex->m_Sampler.m_pTarget = new SHRenderTarget;
            const char* pExt = fpGetExtension(Tex->m_Name.c_str());
            if (pExt && (!stricmp(pExt, ".swf") || !stricmp(pExt, ".gfx")))
            {
              Tex->m_Sampler.m_pTarget->m_refSamplerID = i;
              Tex->m_Sampler.m_pDynTexSource = new CFlashTextureSource(Tex->m_Name.c_str());
            }
						else if (pExt && !stricmp(pExt, ".sfd"))
						{
							Tex->m_Sampler.m_pTarget->m_refSamplerID = i;
							Tex->m_Sampler.m_pDynTexSource = new CVideoTextureSource(Tex->m_Name.c_str());
						}
            else
            {
              if (Tex->m_Sampler.m_eTexType == eTT_AutoCube)
              {
                Tex->m_Sampler.m_pTex = CTexture::m_Text_RT_CM;
                Tex->m_Sampler.m_pTarget->m_pTarget[0] = CTexture::m_Text_RT_CM;
              }
              else
              {
                Tex->m_Sampler.m_pTex = CTexture::m_Text_RT_2D;
                Tex->m_Sampler.m_pTarget->m_pTarget[0] = CTexture::m_Text_RT_2D;
              }		
            }
            Tex->m_Sampler.m_pTarget->m_bTempDepth = true;
            Tex->m_Sampler.m_pTarget->m_eOrder = eRO_PreProcess;
            Tex->m_Sampler.m_pTarget->m_eTF = eTF_A8R8G8B8;
            Tex->m_Sampler.m_pTarget->m_nIDInPool = -1;
            Tex->m_Sampler.m_pTarget->m_nFlags |= FRT_RENDTYPE_RECURSIVECURSCENE | FRT_CAMERA_CURRENT;
            Tex->m_Sampler.m_pTarget->m_nFlags |= FRT_CLEAR_DEPTH | FRT_CLEAR_STENCIL | FRT_CLEAR_COLOR;
          }
        }
        else
        if (Tex->m_Sampler.m_eTexType == eTT_User)
          Tex->m_Sampler.m_pTex = NULL;
        else
        {
          Tex->m_Sampler.m_pTex = mfLoadResourceTexture(Tex->m_Name.c_str(), patch, Tex->m_Sampler.GetTexFlags() | Flags, Tex->m_Sampler.m_eTexType, Tex);
          /*if (Tex->m_Sampler.m_pTex && Tex->m_Sampler.m_pTex->IsTextureLoaded())
          {
            if (i == EFTT_DIFFUSE)
              Tex->m_Sampler.m_pTex->SetFlags(FT_TEX_DIFFUSE);
          }*/
        }
      }
      else
        mfCheckAnimatedSequence(&Res->m_Textures[i]->m_Sampler, Res->m_Textures[i]->m_Sampler.m_pTex);

			//if (i == EFTT_DIFFUSE)
			//{
			//	CTexture* pTex = (CTexture*) Tex->m_Sampler.m_pTex;
			//	if (pTex  && pTex->UseDecalBorderCol())
			//	{
			//		STexState ST;
			//		ST.SetFilterMode(Tex->m_Filter);
			//		ST.SetClampMode(TADDR_BORDER, TADDR_BORDER, TADDR_BORDER);
			//		ST.SetBorderColor(ColorF(1,1,1,0).pack_argb8888());
			//		Tex->m_Sampler.m_nTexState = CTexture::GetTexState(ST);
			//	}
			//}
    }
  }
}

