/*=============================================================================
  ShaderParse.cpp : implementation of the Shaders parser part of shaders manager.
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
#elif defined(LINUX)
#endif


//============================================================
// Compile functions
//============================================================

SShaderGenBit *CShaderMan::mfCompileShaderGenProperty(char *scr)
{
  char* name;
  long cmd;
  char *params;
  char *data;

  SShaderGenBit *shgm = new SShaderGenBit;

  enum {eName=1, eProperty, eDescription, eMask, eHidden, ePrecache, eDependencySet, eDependencyReset, eDependFlagSet, eDependFlagReset, eAutoPrecache, eLowSpecAutoPrecache};
  static STokenDesc commands[] =
  {
    {eName, "Name"},
    {eProperty, "Property"},
    {eDescription, "Description"},
    {eMask, "Mask"},
    {eHidden, "Hidden"},
    {ePrecache, "Precache"},
    {eAutoPrecache, "AutoPrecache"},
    {eLowSpecAutoPrecache, "LowSpecAutoPrecache"},
    {eDependencySet, "DependencySet"},
    {eDependencyReset, "DependencyReset"},
    {eDependFlagSet, "DependFlagSet"},
    {eDependFlagReset, "DependFlagReset"},

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
      case eName:
        shgm->m_ParamName = data;
        shgm->m_dwToken = GetCRC32Gen().GetCRC32(data);
        break;

      case eProperty:
        shgm->m_ParamProp = data;
        break;

      case eDescription:
        shgm->m_ParamDesc = data;
        break;

      case eHidden:
        shgm->m_Flags |= SHGF_HIDDEN;
        break;

      case eAutoPrecache:
        shgm->m_Flags |= SHGF_AUTO_PRECACHE;
        break;
      case eLowSpecAutoPrecache:
        shgm->m_Flags |= SHGF_LOWSPEC_AUTO_PRECACHE;
        break;

      case ePrecache:
        shgm->m_PrecacheNames.push_back(CParserBin::GetCRC32(data));
        shgm->m_Flags |= SHGF_PRECACHE;
        break;

      case eDependFlagSet:
        shgm->m_DependSets.push_back(data);
        break;

      case eDependFlagReset:
        shgm->m_DependResets.push_back(data);
        break;

      case eMask:
        if (data && data[0])
        {
          if (data[0] == '0' && (data[1] == 'x' || data[1] == 'X'))
            shgm->m_Mask = shGetHex64(&data[2]);
          else
            shgm->m_Mask = shGetInt(data);
        }
        break;

      case eDependencySet:
        if (data && data[0])
        {
          if (!stricmp(data, "$LM_Diffuse"))
            shgm->m_nDependencySet |= SHGD_LM_DIFFUSE;
          else
          if (!stricmp(data, "$LM_Specular"))
            shgm->m_nDependencySet |= SHGD_LM_SPECULAR;
          else
          if (!stricmp(data, "$TEX_Bump"))
            shgm->m_nDependencySet |= SHGD_TEX_BUMP;
          else
          if (!stricmp(data, "$TEX_BumpDiffuse"))
            shgm->m_nDependencySet |= SHGD_TEX_BUMPDIF;
          else
          if (!stricmp(data, "$TEX_Gloss"))
            shgm->m_nDependencySet |= SHGD_TEX_GLOSS;
          else
          if (!stricmp(data, "$TEX_EnvCM"))
            shgm->m_nDependencySet |= SHGD_TEX_ENVCM;
          else
          if (!stricmp(data, "$TEX_Opacity"))
            shgm->m_nDependencySet |= SHGD_TEX_OPACITY;
          else
          if (!stricmp(data, "$TEX_Subsurface"))
            shgm->m_nDependencySet |= SHGD_TEX_SUBSURFACE;
          else
          if (!stricmp(data, "$HW_SM30"))
            shgm->m_nDependencySet |= SHGD_HW_SM30;
          else
          if (!stricmp(data, "$HW_BilinearFP16"))
            shgm->m_nDependencySet |= SHGD_HW_BILINEARFP16;
          else
          if (!stricmp(data, "$HW_SeparateFP16"))
            shgm->m_nDependencySet |= SHGD_HW_SEPARATEFP16;
          else
          if (!stricmp(data, "$HW_DynamicBranching"))
            shgm->m_nDependencySet |= SHGD_HW_DYN_BRANCHING;
          else
          if (!stricmp(data, "$HW_StaticBranching"))
            shgm->m_nDependencySet |= SHGD_HW_STAT_BRANCHING;          
          else
          if (!stricmp(data, "$HW_PostProcessDynamicBranching"))
            shgm->m_nDependencySet |= SHGD_HW_DYN_BRANCHING_POSTPROCESS;          
          else
          if (!stricmp(data, "$TEX_Custom"))
            shgm->m_nDependencySet |= SHGD_TEX_CUSTOM;
          else
          if (!stricmp(data, "$TEX_CustomSecondary"))
            shgm->m_nDependencySet |= SHGD_TEX_CUSTOM_SECONDARY;
          else
          if (!stricmp(data, "$TEX_Decal"))
            shgm->m_nDependencySet |= SHGD_TEX_DECAL;
					else
					if (!stricmp(data, "$HW_AllowPOM"))
						shgm->m_nDependencySet |= SHGD_HW_ALLOW_POM;          
          else
            assert(0);
        }
        break;

      case eDependencyReset:
        if (data && data[0])
        {
          if (!stricmp(data, "$LM_Diffuse"))
            shgm->m_nDependencyReset |= SHGD_LM_DIFFUSE;
          else
          if (!stricmp(data, "$LM_Specular"))
            shgm->m_nDependencyReset |= SHGD_LM_SPECULAR;
          else
          if (!stricmp(data, "$TEX_Bump"))
            shgm->m_nDependencyReset |= SHGD_TEX_BUMP;
          else
          if (!stricmp(data, "$TEX_BumpDiffuse"))
            shgm->m_nDependencyReset |= SHGD_TEX_BUMPDIF;
          else
          if (!stricmp(data, "$TEX_Gloss"))
            shgm->m_nDependencyReset |= SHGD_TEX_GLOSS;
          else
          if (!stricmp(data, "$TEX_EnvCM"))
            shgm->m_nDependencyReset |= SHGD_TEX_ENVCM;
          else
          if (!stricmp(data, "$TEX_Opacity"))
            shgm->m_nDependencyReset |= SHGD_TEX_OPACITY;
          else
          if (!stricmp(data, "$TEX_Subsurface"))
            shgm->m_nDependencyReset |= SHGD_TEX_SUBSURFACE;
          else
          if (!stricmp(data, "$HW_SM30"))
            shgm->m_nDependencyReset |= SHGD_HW_SM30;
          else
          if (!stricmp(data, "$HW_BilinearFP16"))
            shgm->m_nDependencyReset |= SHGD_HW_BILINEARFP16;
          else
          if (!stricmp(data, "$HW_SeparateFP16"))
            shgm->m_nDependencyReset |= SHGD_HW_SEPARATEFP16;
          else
          if (!stricmp(data, "$HW_DynamicBranching"))
            shgm->m_nDependencyReset |= SHGD_HW_DYN_BRANCHING;
          else
          if (!stricmp(data, "$HW_StaticBranching"))
            shgm->m_nDependencyReset |= SHGD_HW_STAT_BRANCHING;
          else
          if (!stricmp(data, "$HW_PostProcessDynamicBranching"))
            shgm->m_nDependencyReset |= SHGD_HW_DYN_BRANCHING_POSTPROCESS;          
          else        
          if (!stricmp(data, "$TEX_Custom"))
            shgm->m_nDependencyReset |= SHGD_TEX_CUSTOM;
          else
          if (!stricmp(data, "$TEX_CustomSecondary"))
            shgm->m_nDependencyReset |= SHGD_TEX_CUSTOM_SECONDARY;
          else
          if (!stricmp(data, "$TEX_Decal"))
            shgm->m_nDependencyReset |= SHGD_TEX_DECAL;
					else
					if (!stricmp(data, "$HW_AllowPOM"))
						shgm->m_nDependencyReset |= SHGD_HW_ALLOW_POM;          
          else
            assert(0);
        }
        break;
    }
  }
  shgm->m_NameLength = strlen(shgm->m_ParamName.c_str());

  return shgm;
}

bool CShaderMan::mfCompileShaderGen(SShaderGen *shg, char *scr)
{
  char* name;
  long cmd;
  char *params;
  char *data;

  SShaderGenBit *shgm;

  enum {eProperty=1, eVersion};
  static STokenDesc commands[] =
  {
    {eProperty, "Property"},
    {eVersion, "Version"},

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
      case eProperty:
        shgm = mfCompileShaderGenProperty(params);
        if (shgm)
          shg->m_BitMask.AddElem(shgm);
        break;

      case eVersion:
        break;
    }
  }

  return shg->m_BitMask.Num() != 0;
}


SShaderGen *CShaderMan::mfCreateShaderGenInfo(const char *szName)
{
  SShaderGen *pShGen = NULL;
  FILE *fp = iSystem->GetIPak()->FOpen(szName, "rb",ICryPak::FOPEN_HINT_QUIET);
  if (fp)
  {
    pShGen = new SShaderGen;
    iSystem->GetIPak()->FSeek(fp, 0, SEEK_END);
    int ln = iSystem->GetIPak()->FTell(fp);
    char *buf = new char [ln+1];
    if (buf)
    {
      buf[ln] = 0;
      iSystem->GetIPak()->FSeek(fp, 0, SEEK_SET);
      iSystem->GetIPak()->FRead(buf, ln, fp);
      iSystem->GetIPak()->FClose(fp);
      mfCompileShaderGen(pShGen, buf);
      delete [] buf;
    }
    else
      iSystem->GetIPak()->FClose(fp);
  }
  return pShGen;
}
