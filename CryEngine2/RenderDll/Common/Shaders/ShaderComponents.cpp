/*=============================================================================
ShaderComponents.cpp : FX Shaders semantic components implementation.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "I3DEngine.h"
#include "CryHeaders.h"
#include "../Shadow_Renderer.h"

#if defined(WIN32) || defined(WIN64)
#include <direct.h>
#include <io.h>
#elif defined(LINUX)
#endif

static void sParseTexMatrix(const char *szScr, const char *szAnnotations, DynArray<STexSampler>* pSamplers, SCGParam *vpp, int nComp, CShader *ef)
{
  uint i;
  const char *szSampler = gRenDev->m_cEF.mfParseFX_Parameter(szAnnotations, eType_STRING, "Sampler");
  assert (szSampler);
  if (szSampler)
  {
    for (i=0; i<pSamplers->size(); i++)
    {
      STexSampler *sm = &(*pSamplers)[i];
      if (!stricmp(sm->m_Name.c_str(), szSampler))
      {
        vpp->m_nID = (UINT_PTR)sm->m_pTarget;
        break;
      }
    }
  }
}

static void sParseTimeExpr(const char *szScr, const char *szAnnotations, DynArray<STexSampler>* pSamplers, SCGParam *vpp, int nComp, CShader *ef)
{
  if (szScr)
  {
    if (!vpp->m_pData)
      vpp->m_pData = new SParamData;
    char str[256];
    float f;
    int n = sscanf(szScr, "%s %f", str, &f);
    if (n == 2)
      vpp->m_pData->d.fData[nComp] = f;
    else
      vpp->m_pData->d.fData[nComp] = 1.0f;
  }
}

static void sParseGlobalShaderFlag(const char *szScr, const char *szAnnotations, DynArray<STexSampler>* pSamplers, SCGParam *vpp, int nComp, CShader *ef)
{
  if (!ef)
    ef = gRenDev->m_RP.m_pShader;
  if (!ef)
    return;
  if (szScr)
  {
    if (!vpp->m_pData)
      vpp->m_pData = new SParamData;
    char str[256], strval[256];
    int n = sscanf(szScr, "%s %s", str, strval);
    if (n == 2)
    {
      bool bOK = false;
      if (strval[0] == '%')
      {
        if (ef->m_pGenShader && ef->m_pGenShader->m_ShaderGenParams) 
        {
          SShaderGen *pGen = ef->m_pGenShader->m_ShaderGenParams;
          for (n=0; n<pGen->m_BitMask.Num(); n++)
          {
            SShaderGenBit *pBit = pGen->m_BitMask[n];
            if (!strcmp(pBit->m_ParamName.c_str(), strval))
            {
              vpp->m_pData->d.nData32[nComp] = pBit->m_Mask;
              break;
            }
          }
          if (n == pGen->m_BitMask.Num())
          {
            iLog->Log("Error: Couldn't find global shader flag '%s' for shader '%s'", strval, ef->GetName());
            vpp->m_pData->d.nData32[nComp] = 0;
          }
        }
        else
          vpp->m_pData->d.nData32[nComp] = 0;
      }
      else
        vpp->m_pData->d.nData32[nComp] = shGetHex(strval);
    }
    else
      vpp->m_pData->d.nData32[nComp] = 0;
  }
  else
  {
    assert(0);
  }
}

static void sParseRuntimeShaderFlag(const char *szScr, const char *szAnnotations, DynArray<STexSampler>* pSamplers, SCGParam *vpp, int nComp, CShader *ef)
{
  if (szScr)
  {
    if (!vpp->m_pData)
      vpp->m_pData = new SParamData;
    char str[256], strval[256];
    int n = sscanf(szScr, "%s %s", str, strval);
    if (n == 2)
    {
      bool bOK = false;
      if (strval[0] == '%')
      {
        SShaderGen *pGen = gRenDev->m_cEF.m_pGlobalExt;
        for (n=0; n<pGen->m_BitMask.Num(); n++)
        {
          SShaderGenBit *pBit = pGen->m_BitMask[n];
          if (!strcmp(pBit->m_ParamName.c_str(), strval))
          {
            vpp->m_pData->d.nData64[nComp] = pBit->m_Mask;
            break;
          }
        }
        if (n == pGen->m_BitMask.Num())
        {
          iLog->Log("Error: Couldn't find runtime shader flag '%s' for shader '%s'", strval, ef->GetName());
          vpp->m_pData->d.nData64[nComp] = 0;
        }
      }
      else
        vpp->m_pData->d.nData64[nComp] = shGetHex(strval);
    }
    else
      vpp->m_pData->d.nData64[nComp] = 0;
  }
  else
  {
    assert(0);
  }
}

//======================================================================================

// GL = global
// PB = Per-Batch
// PI = Per-Instance
// PF = Per-Frame
// PM = Per-Material
// SK = Skin data
// MP = Morph data

#define PARAM(a, b) #a, b
static SParamDB sParams[] = 
{  
  SParamDB(PARAM(PI_ViewProjection, ECGP_Matr_PI_ViewProj), 0),
  SParamDB(PARAM(PF_ViewProjMatrix, ECGP_Matr_PF_ViewProjMatrix), 0),
  SParamDB(PARAM(PF_ViewProjZeroMatrix, ECGP_Matr_PF_ViewProjZeroMatrix), 0),
  SParamDB(PARAM(PB_ViewProjMatrix_IT, ECGP_Matr_PB_ViewProjMatrix_IT), 0),
  SParamDB(PARAM(PB_ViewProjMatrix_I, ECGP_Matr_PB_ViewProjMatrix_I), 0),

  SParamDB(PARAM(PB_View_IT, ECGP_Matr_PB_View_IT), 0),
  SParamDB(PARAM(PB_View_I, ECGP_Matr_PB_View_I), 0),
  SParamDB(PARAM(PB_View_T, ECGP_Matr_PB_View_T ), 0),
  SParamDB(PARAM(PB_View, ECGP_Matr_PB_View), 0),

  SParamDB(PARAM(PB_CameraMatrix_IT, ECGP_Matr_PB_Camera_IT), 0),
  SParamDB(PARAM(PB_CameraMatrix_I, ECGP_Matr_PB_Camera_I), 0),
  SParamDB(PARAM(PB_CameraMatrix_T, ECGP_Matr_PB_Camera_T), 0),  
  SParamDB(PARAM(PB_CameraMatrix, ECGP_Matr_PB_Camera), 0),
      
  SParamDB(PARAM(PI_Composite, ECGP_Matr_PI_Composite), 0),
  SParamDB(PARAM(PB_UnProjMatrix, ECGP_Matr_PB_UnProjMatrix), 0),
  SParamDB(PARAM(PB_ProjMatrix, ECGP_Matr_PB_ProjMatrix), 0),
  SParamDB(PARAM(PB_TerrainBaseMatrix, ECGP_Matr_PB_TerrainBase), 0),
  SParamDB(PARAM(PB_TerrainLayerGen, ECGP_Matr_PB_TerrainLayerGen), 0),
  SParamDB(PARAM(PI_ObjMatrix, ECGP_Matr_PI_Obj), 0),  
  SParamDB(PARAM(PI_TransObjMatrix, ECGP_Matr_PI_Obj_T), 0), // Due to some bug in Parser, ObjMatrix_T or something
  
  SParamDB(PARAM(PI_MotionBlurData, ECGP_PI_MotionBlurData), 0),  
  SParamDB(PARAM(PI_CloakMotionData, ECGP_PI_CloakMotionData), 0),  
  SParamDB(PARAM(PI_CloakParams, ECGP_PI_CloakParams), 0),
   
  SParamDB(PARAM(PB_LightMatrix, ECGP_Matr_PB_LightMatrix), 0),
  SParamDB(PARAM(PB_TempMatr0, ECGP_Matr_PB_Temp4_0), PD_INDEXED),
  SParamDB(PARAM(PB_TempMatr1, ECGP_Matr_PB_Temp4_1), PD_INDEXED),
  SParamDB(PARAM(PB_TempMatr2, ECGP_Matr_PB_Temp4_2), PD_INDEXED),
  SParamDB(PARAM(PB_TempMatr3, ECGP_Matr_PB_Temp4_3), PD_INDEXED),
  SParamDB(PARAM(PB_Temp2Matr0, ECGP_Matr_PB_2Temp4_0), PD_INDEXED),
  SParamDB(PARAM(PB_Temp2Matr1, ECGP_Matr_PB_2Temp4_1), PD_INDEXED),
  SParamDB(PARAM(PB_Temp2Matr2, ECGP_Matr_PB_2Temp4_2), PD_INDEXED),
  SParamDB(PARAM(PB_Temp2Matr3, ECGP_Matr_PB_2Temp4_3), PD_INDEXED),
  SParamDB(PARAM(PI_TexMatrix, ECGP_Matr_PI_TexMatrix), 0, sParseTexMatrix),
  SParamDB(PARAM(PI_TCMMatrix, ECGP_Matr_PI_TCMMatrix), PD_INDEXED),
  SParamDB(PARAM(PI_TCGMatrix, ECGP_Matr_PI_TCGMatrix), PD_INDEXED),

  SParamDB(PARAM(PL_LightsPos, ECGP_PL_LightsPos), 0),
  SParamDB(PARAM(PI_GeomCenter, ECGP_PI_GeomCenter), 0),  
  SParamDB(PARAM(PB_RotGridScreenOff, ECGP_PB_RotGridScreenOff), 0),
  SParamDB(PARAM(PL_ShadowMasks, ECGP_PL_ShadowMasks), 0),
  SParamDB(PARAM(PB_DiffuseMulti, ECGP_PB_DiffuseMulti), 0),
  SParamDB(PARAM(PB_SpecularMulti, ECGP_PB_SpecularMulti), 0),
  SParamDB(PARAM(PL_LDiffuseColors, ECGP_PL_LDiffuseColors), 0),
  SParamDB(PARAM(PL_LSpecularColors, ECGP_PL_LSpecularColors), 0),

  SParamDB(PARAM(PM_MatDiffuseColor, ECGP_PM_MatDiffuseColor), 0),
  SParamDB(PARAM(PM_MatSpecularColor, ECGP_PM_MatSpecularColor), 0),
  SParamDB(PARAM(PM_MatEmissiveColor, ECGP_PM_MatEmissiveColor), 0),
  
  SParamDB(PARAM(PI_OSCameraPos, ECGP_PI_OSCameraPos), 0),
  SParamDB(PARAM(PB_OutdoorAOParams, ECGP_PB_OutdoorAOParams), 0),
  SParamDB(PARAM(PI_AmbientOpacity, ECGP_PI_AmbientOpacity), 0),
  SParamDB(PARAM(PI_Ambient, ECGP_PI_Ambient), 0),
  SParamDB(PARAM(PB_MeshScale, ECGP_PB_MeshScale), 0),
  SParamDB(PARAM(PB_SunSkyConstants, ECGP_PB_SunSkyConstants), 0),
	SParamDB(PARAM(PI_RAM_HQAmbientCube, ECGP_PI_RAM_HQAmbientCube), PD_INDEXED),
	SParamDB(PARAM(PI_RAM_AmbientCube, ECGP_PI_RAM_AmbientCube), PD_INDEXED),
	SParamDB(PARAM(PI_RAM_ToAABBSpaceScale, ECGP_PI_RAM_ToAABBSpaceScale), PD_INDEXED),
	SParamDB(PARAM(PI_RAM_ToAABBSpaceTranslate, ECGP_PI_RAM_ToAABBSpaceTranslate), PD_INDEXED),
  SParamDB(PARAM(PB_GlowParams, ECGP_PB_GlowParams), 0),
  SParamDB(PARAM(PB_ResourcesOpacity, ECGP_PB_ResourcesOpacity), 0),
  SParamDB(PARAM(PI_VisionParams, ECGP_PI_VisionParams), 0),
  SParamDB(PARAM(PB_RandomParams, ECGP_PB_RandomParams), 0),
  
  SParamDB(PARAM(PB_IrregKernel, ECGP_PB_IrregKernel), 0),
  SParamDB(PARAM(PB_RegularKernel, ECGP_PB_RegularKernel), 0),
  SParamDB(PARAM(PI_ObjectAmbColComp, ECGP_PI_ObjectAmbColComp), 0),  
  SParamDB(PARAM(PI_BendInfo, ECGP_PI_BendInfo), 0),
  SParamDB(PARAM(PB_DeformWaveX, ECGP_PB_DeformWaveX), 0),
  SParamDB(PARAM(PB_DeformWaveY, ECGP_PB_DeformWaveY), 0),
  SParamDB(PARAM(PB_DeformBend, ECGP_PB_DeformBend), 0),
  SParamDB(PARAM(PB_DeformNoiseInfo, ECGP_PB_DeformNoiseInfo), 0),
  SParamDB(PARAM(PB_ShadowOutputChannelMask, ECGP_PB_ShadowOutputChannelMask), 0),
  SParamDB(PARAM(PB_TFactor, ECGP_PB_TFactor), 0),
  SParamDB(PARAM(PI_AlphaTest, ECGP_PI_AlphaTest), 0),
  SParamDB(PARAM(PB_AlphaTest, ECGP_PI_AlphaTest), 0),
  SParamDB(PARAM(PI_MaterialLayersParams, ECGP_PI_MaterialLayersParams), 0),
  SParamDB(PARAM(PI_FrozenLayerParams, ECGP_PI_FrozenLayerParams), 0),  
  SParamDB(PARAM(PB_TempData, ECGP_PB_TempData), PD_INDEXED | 0),
  SParamDB(PARAM(PB_DecalZFightingRemedy, ECGP_PB_DecalZFightingRemedy), 0),
  SParamDB(PARAM(PB_CameraFront, ECGP_PB_CameraFront), 0),  
  SParamDB(PARAM(PB_CameraRight, ECGP_PB_CameraRight), 0),
  SParamDB(PARAM(PB_CameraUp, ECGP_PB_CameraUp), 0),
  SParamDB(PARAM(PB_RTRect, ECGP_PB_RTRect), 0),
  SParamDB(PARAM(PB_VolumetricFogParams, ECGP_PB_VolumetricFogParams), 0),
  SParamDB(PARAM(PB_VolumetricFogColor, ECGP_PB_VolumetricFogColor), 0),
  SParamDB(PARAM(PI_AvgFogVolumeContrib, ECGP_PI_AvgFogVolumeContrib), 0),  
  SParamDB(PARAM(PB_SkyLightHazeColorPartialRayleighInScatter, ECGP_PB_SkyLightHazeColorPartialRayleighInScatter), 0),
  SParamDB(PARAM(PB_SkyLightHazeColorPartialMieInScatter, ECGP_PB_SkyLightHazeColorPartialMieInScatter), 0),
  SParamDB(PARAM(PB_SkyLightSunDirection, ECGP_PB_SkyLightSunDirection), 0),
  SParamDB(PARAM(PB_SkyLightPhaseFunctionConstants, ECGP_PB_SkyLightPhaseFunctionConstants), 0),
  SParamDB(PARAM(PI_Opacity, ECGP_PI_Opacity), PD_INDEXED),
  SParamDB(PARAM(PB_LightInfoTC, ECGP_PB_LightInfoTC), 0),
  SParamDB(PARAM(PI_NumInstructions, ECGP_PI_NumInstructions), PD_INDEXED),
  SParamDB(PARAM(PI_ObjColor, ECGP_PI_ObjColor), 0),  
  SParamDB(PARAM(PB_LightningPos, ECGP_PB_LightningPos), 0),  
  SParamDB(PARAM(PB_LightningColSize, ECGP_PB_LightningColSize), 0),
  SParamDB(PARAM(PI_EnvColor, ECGP_PI_EnvColor), 0),
  SParamDB(PARAM(PB_FromRE, ECGP_PB_FromRE), PD_INDEXED | 0),
  SParamDB(PARAM(PB_GlobalShaderFlag, ECGP_PB_GlobalShaderFlag), 0, sParseGlobalShaderFlag),
  SParamDB(PARAM(PB_RuntimeShaderFlag, ECGP_PB_RuntimeShaderFlag), 0, sParseRuntimeShaderFlag),
  SParamDB(PARAM(PI_FromObject, ECGP_PI_FromObject), PD_INDEXED),
  SParamDB(PARAM(PI_ObjVal, ECGP_PB_ObjVal), PD_INDEXED),
  SParamDB(PARAM(PB_ObjVal, ECGP_PB_ObjVal), PD_INDEXED),
  SParamDB(PARAM(PB_LightsNum, ECGP_PB_LightsNum), 0),
  SParamDB(PARAM(PB_WaterLevel, ECGP_PB_WaterLevel), 0),
  SParamDB(PARAM(PI_Wind, ECGP_PI_Wind), 0),
  SParamDB(PARAM(PB_HDRDynamicMultiplier, ECGP_PB_HDRDynamicMultiplier), 0),
  SParamDB(PARAM(PB_CausticsParams, ECGP_PB_CausticsParams), 0),    
  SParamDB(PARAM(PB_CausticsSmoothSunDirection, ECGP_PB_CausticsSmoothSunDirection), 0),    
  SParamDB(PARAM(PF_SunDirection, ECGP_PF_SunDirection), 0),    
  SParamDB(PARAM(PF_FogColor, ECGP_PF_FogColor), 0),
  SParamDB(PARAM(PF_SunColor, ECGP_PF_SunColor), 0),
	SParamDB(PARAM(PF_SkyColor, ECGP_PF_SkyColor), 0),
  SParamDB(PARAM(PF_Time, ECGP_PF_Time), 0, sParseTimeExpr),
  SParamDB(PARAM(PB_Time, ECGP_PF_Time), 0, sParseTimeExpr),
  SParamDB(PARAM(PF_ScreenSize, ECGP_PF_ScreenSize), 0),
  SParamDB(PARAM(PB_ScreenSize, ECGP_PF_ScreenSize), 0),
  SParamDB(PARAM(PF_CameraPos, ECGP_PF_CameraPos), 0),
  SParamDB(PARAM(PB_CameraPos, ECGP_PF_CameraPos), 0),
  SParamDB(PARAM(PF_NearFarDist, ECGP_PF_NearFarDist), 0),
  SParamDB(PARAM(PB_NearFarDist, ECGP_PF_NearFarDist), 0),

	SParamDB(PARAM(PB_SFCompMat, ECGP_Matr_PB_SFCompMat), 0),  
	SParamDB(PARAM(PB_SFTexGenMat0, ECGP_Matr_PB_SFTexGenMat0), 0),
	SParamDB(PARAM(PB_SFTexGenMat1, ECGP_Matr_PB_SFTexGenMat1), 0),
	SParamDB(PARAM(PB_SFBitmapColorTransform, ECGP_PB_SFBitmapColorTransform), 0),

	SParamDB(PARAM(PI_OceanMat, ECGP_Matr_PI_OceanMat), 0),

	SParamDB(PARAM(PI_HMAGradients, ECGP_PI_HMAGradients), 0),

	SParamDB(PARAM(PB_CloudShadingColorSun, ECGP_PB_CloudShadingColorSun), 0),
	SParamDB(PARAM(PB_CloudShadingColorSky, ECGP_PB_CloudShadingColorSky), 0),

	SParamDB(PARAM(PB_ResInfoDiffuse, ECGP_PB_ResInfoDiffuse), 0),
	SParamDB(PARAM(PB_ResInfoBump, ECGP_PB_ResInfoBump), 0),

  SParamDB(PARAM(SG_FrustrumInfo, ECGP_SG_FrustrumInfo), 0),

  SParamDB(PARAM(SG_ShadowMatr0, ECGP_Matr_SG_ShadowProj_0), 0),
  SParamDB(PARAM(SG_ShadowMatr1, ECGP_Matr_SG_ShadowProj_1), 0),
  SParamDB(PARAM(SG_ShadowMatr2, ECGP_Matr_SG_ShadowProj_2), 0),
  SParamDB(PARAM(SG_ShadowMatr3, ECGP_Matr_SG_ShadowProj_3), 0),

	SParamDB(PARAM(PB_TexAtlasSize, ECGP_PB_TexAtlasSize), 0),

	SParamDB()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int SCGParamsPF::nFrameID = 0;
Vec3 SCGParamsPF::vWaterLevel; // ECGP_PB_WaterLevel

float SCGParamsPF::fHDRDynamicMultiplier; // ECGP_PB_HDRDynamicMultiplier

Matrix44 SCGParamsPF::pProjMatrix; //ECGP_Matr_PB_ProjMatrix
Matrix44 SCGParamsPF::pUnProjMatrix; //ECGP_Matr_PB_UnProjMatrix
Matrix44 SCGParamsPF::pMatrixComposite; //ECGP_Matr_PI_Composite

Vec4 SCGParamsPF::pVolumetricFogParams;  // ECGP_PB_VolumetricFogParams
Vec3 SCGParamsPF::pVolumetricFogColor; // ECGP_PB_VolumetricFogColor
Vec3 SCGParamsPF::pSkyLightHazeColorPartialMieInScatter; //ECGP_PB_SkyLightHazeColorPartialMieInScatter
Vec4 SCGParamsPF::pSkyLightHazeColorPartialRayleighInScatter; //ECGP_PB_SkyLightHazeColorPartialRayleighInScatter
SSkyLightRenderParams *SCGParamsPF::pSkyLightRenderParams;
Vec4 SCGParamsPF::pSunSkyConstants; // ECGP_PB_SunSkyConstants

Vec3 SCGParamsPF::pCameraFront; // ECGP_PB_CameraFront
Vec3 SCGParamsPF::pCameraRight; // ECGP_PB_CameraRight
Vec3 SCGParamsPF::pCameraUp; // ECGP_PB_CameraUp
Vec3 SCGParamsPF::pCameraPos; //ECGP_PF_CameraPos

Vec4 SCGParamsPF::pScreenSize; //ECGP_PF_ScreenSize
Vec4 SCGParamsPF::pNearFarDist; //ECGP_PF_NearFarDist
Vec3 SCGParamsPF::pDecalZFightingRemedy; //ECGP_PB_DecalZFightingRemedy

Vec4 SCGParamsPF::pLightningColSize; // ECGP_PB_LightningColSize
Vec3 SCGParamsPF::pLightningPos; //ECGP_PB_LightningPos
Vec3 SCGParamsPF::pCausticsParams; //ECGP_PB_CausticsParams
Vec3 SCGParamsPF::pSunColor; //ECGP_PF_SunColor
Vec3 SCGParamsPF::pSkyColor; //ECGP_PF_SkyColor
Vec3 SCGParamsPF::pSunDirection; //ECGP_PF_SunDirection

Vec3 SCGParamsPF::pCloudShadingColorSun; //ECGP_PB_CloudShadingColorSun
Vec3 SCGParamsPF::pCloudShadingColorSky; //ECGP_PB_CloudShadingColorSky

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *CShaderMan::mfGetShaderParamName(ECGParam ePR)
{
  int n = 0;
  const char *szName;
  while (szName=sParams[n].szName)
  {
    if (sParams[n].eParamType == ePR)
      return szName;
    n++;
  }
  if (ePR == ECGP_PM_Tweakable)
    return "PM_Tweakable";
  return NULL;
}

SParamDB *CShaderMan::mfGetShaderParamDB(const char *szSemantic)
{
  const char *szName;
  int n = 0;
  while (szName=sParams[n].szName)
  {
    int nLen = strlen(szName);
    if (!strnicmp(szName, szSemantic, nLen) || (sParams[n].szAliasName && !strnicmp(sParams[n].szAliasName, szSemantic, strlen(sParams[n].szAliasName))))
      return &sParams[n];
    n++;
  }
  return NULL;
}

bool CShaderMan::mfParseParamComp(int comp, SCGParam *pCurParam, DynArray<SFXParam>& Params, std::vector<SCGParam>* pParams, char *szSemantic, char *params, const char *szAnnotations, DynArray<STexSampler>* pSamplers, CShader *ef, int nComps, uint nParamFlags, EHWShaderClass eSHClass, bool bExpressionOperand)
{
  if (comp >= 4 || comp < -1)
    return false;
  if (!pCurParam)
    return false;
  if (comp > 0)
    pCurParam->m_Flags &= ~PF_SINGLE_COMP;
  else
    pCurParam->m_Flags |= PF_SINGLE_COMP;
  if (!szSemantic || !szSemantic[0])
  {
    if (!pCurParam->m_pData)
      pCurParam->m_pData = new SParamData;
    pCurParam->m_pData->d.fData[comp] = shGetFloat(params);
    if (!((nParamFlags>>comp) & PF_TWEAKABLE_0))
      pCurParam->m_eCGParamType = (ECGParam)((int)pCurParam->m_eCGParamType | (int)(ECGP_PB_Scalar << (comp*8)));
    else
    {
      pCurParam->m_eCGParamType = (ECGParam)((int)pCurParam->m_eCGParamType | (int)(ECGP_PM_Tweakable << (comp*8)));
#ifdef USE_PER_MATERIAL_PARAMS
      if (!bExpressionOperand)
      {
        pCurParam->m_eCGParamType = (ECGParam)((int)pCurParam->m_eCGParamType | (int)ECGP_PM_Tweakable);
        pCurParam->m_Flags |= PF_MATERIAL | PF_SINGLE_COMP;
      }
#endif
    }
    return true;
  }
  if (!stricmp(szSemantic, "NULL"))
    return true;
  if (szSemantic[0] == '(')
  {
    pCurParam->m_eCGParamType = ECGP_PM_Tweakable;
    pCurParam->m_Flags |= PF_SINGLE_COMP | PF_MATERIAL;
    return true;
  }
  const char *szName;
  int n = 0;
  while (szName=sParams[n].szName)
  {
    int nLen = strlen(szName);
    if (!strnicmp(szName, szSemantic, nLen) || (sParams[n].szAliasName && !strnicmp(sParams[n].szAliasName, szSemantic, strlen(sParams[n].szAliasName))))
    {
      if (sParams[n].nFlags & PD_MERGED)
        pCurParam->m_Flags |= PF_CANMERGED;
      if (!strnicmp(szName, "PI_", 3))
        pCurParam->m_Flags |= PF_INSTANCE;
      else
      if (!strnicmp(szName, "PF_", 3))
        pCurParam->m_Flags |= PF_GLOBAL;
      else
      if (!strnicmp(szName, "PM_", 3))
        pCurParam->m_Flags |= PF_MATERIAL;
      else
      if (!strnicmp(szName, "PL_", 3))
        pCurParam->m_Flags |= PF_LIGHT;
      else
      if (!strnicmp(szName, "SG_", 3))
        pCurParam->m_Flags |= PF_SHADOWGEN;
      assert(sParams[n].eParamType < 256);
      if (comp > 0)
        pCurParam->m_eCGParamType = (ECGParam)((int)pCurParam->m_eCGParamType | (int)(sParams[n].eParamType << (comp*8)));
      else
        pCurParam->m_eCGParamType = sParams[n].eParamType;
      if (pCurParam->m_nParameters > 1)
        pCurParam->m_Flags |= PF_MATRIX;
      if (sParams[n].nFlags & PD_INDEXED)
      {
        if (szSemantic[nLen] == '[')
        {
          int nID = shGetInt(&szSemantic[nLen+1]);
          assert(nID < 256);
          if (comp > 0)
            nID <<= (comp*8);
          pCurParam->m_nID |= nID;
        }
      }
      if (sParams[n].ParserFunc)
        sParams[n].ParserFunc(params ? params : szSemantic, szAnnotations, pSamplers, pCurParam, comp, ef);
      break;
    }
    n++;
  }
  if (!szName)
    return false;
  return true;
}

bool CShaderMan::mfParseCGParam(char *scr, const char *szAnnotations, DynArray<SFXParam>& Params, DynArray<STexSampler>* pSamplers, CShader *ef, std::vector<SCGParam>* pParams, int nComps, uint nParamFlags, EHWShaderClass eSHClass, bool bExpressionOperand)
{
  char* name;
  long cmd;
  char *params;
  char *data;

  int nComp = 0;

  enum {eComp=1, eParam, eName};
  static STokenDesc commands[] =
  {
    {eName, "Name"},
    {eComp, "Comp"},
    {eParam, "Param"},
    {0,0}
  };
  SCGParam vpp;
  int n = pParams->size();
  bool bRes = true;

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
        vpp.m_Name = data;
        break;
      case eComp:
        {
          if (nComp < 4)
          {
            bRes &= mfParseParamComp(nComp, &vpp, Params, pParams, name, params, szAnnotations, pSamplers, ef, nComps, nParamFlags, eSHClass, bExpressionOperand);
            nComp++;
          }
        }
        break;
      case eParam:
        if (!name)
          name = params;
        bRes &= mfParseParamComp(-1, &vpp, Params, pParams, name, params, szAnnotations, pSamplers, ef, nComps, nParamFlags, eSHClass, bExpressionOperand);
        break;
    }
  }
  if (n == pParams->size())
    pParams->push_back(vpp);
  assert(bRes == true);
  return bRes;
}

bool CShaderMan::mfParseFXParameter(DynArray<SFXParam>& Params, SFXParam *pr, DynArray<STexSampler>* pSamplers, const char *ParamName, CShader *ef, bool bInstParam, int nParams, std::vector<SCGParam>* pParams, EHWShaderClass eSHClass, bool bExpressionOperand)
{
  SCGParam CGpr;
  char scr[256];

  uint nParamFlags = pr->GetParamFlags();

  CryFixedStringT<512> Semantic = "Name=";
  Semantic += ParamName;
  Semantic += " ";
  int nComps = 0;
  if (pr->m_Assign.size())
  {
    Semantic += "Param=";
    Semantic += pr->m_Assign;
    nComps = pr->m_nComps;
  }
  else
  {
    for (int i=0; i<pr->m_nComps; i++)
    {
      if (i)
        Semantic += " ";
      string cur = pr->GetParamComp(i);
      if (!cur[0])
        break;
      nComps++;
      if ((cur[0] == '-' && isdigit(cur[1])) || isdigit(cur[0]))
        sprintf(scr, "Comp = %s", cur.c_str());
      else
        sprintf(scr, "Comp '%s'", cur.c_str());
      Semantic += scr;
    }
  }
  // Process parameters only with semantics
  if (nComps)
  {
    uint nOffs = pParams->size();
    bool bRes = mfParseCGParam((char *)Semantic.c_str(), pr->m_Annotations.c_str(), Params, pSamplers, ef, pParams, nComps, nParamFlags, eSHClass, bExpressionOperand);
    assert(bRes);
    if (pParams->size() > nOffs)
    {
      //assert(pBind->m_nComponents == 1);
      SCGParam &p = (*pParams)[nOffs];
      p.m_dwBind = -1;
      p.m_nParameters = nParams;
      p.m_Flags |= nParamFlags;
      if (p.m_Flags & PF_AUTOMERGED)
      {
        if (!p.m_pData)
          p.m_pData = new SParamData;
        const char *src = p.m_Name.c_str();
        for (uint i=0; i<4; i++)
        {
          char param[128];
          char dig = i + 0x30;
          while (*src)
          {
            if (src[0] == '_' && src[1] == '_' && src[2] == dig)
            {
              int n = 0;
              src += 3;
              while (src[n])
              {
                if (src[n] == '_' && src[n+1] == '_')
                  break;
                param[n] = src[n];
                n++;
              }
              param[n] = 0;
              src += n;
              if (param[0])
                p.m_pData->m_CompNames[i] = param;
              break;
            }
            else
              src++;
          }
        }
      }
    }
    return bRes;
  }
  // Parameter without semantic
  return false;
}

//===========================================================================================

bool SShaderParam::GetValue(const char* szName, DynArray<SShaderParam> *Params, float *v, int nID)
{
  unsigned int i;
  bool bRes = false;
  for (i=0; i<Params->size(); i++)
  {
    SShaderParam *sp = &(*Params)[i];

    if (!sp)
      continue;
    if (!stricmp(sp->m_Name, szName))
    {
      bRes = true;
      switch (sp->m_Type)
      {
      case eType_FLOAT:
        v[nID] = sp->m_Value.m_Float;
        break;
      case eType_SHORT:
        v[nID] = (float)sp->m_Value.m_Short;
        break;
      case eType_INT:
      case eType_TEXTURE_HANDLE:
        v[nID] = (float)sp->m_Value.m_Int;
        break;

      case eType_VECTOR:
        v[0] = sp->m_Value.m_Vector[0];
        v[1] = sp->m_Value.m_Vector[1];
        v[2] = sp->m_Value.m_Vector[2];
        break;

      case eType_FCOLOR:
        v[0] = sp->m_Value.m_Color[0];
        v[1] = sp->m_Value.m_Color[1];
        v[2] = sp->m_Value.m_Color[2];
        v[3] = sp->m_Value.m_Color[3];
        break;

      case eType_STRING:
        assert(0);
        bRes = false;
        break;
      }
      break;
    }
  }
  return bRes;
}

bool sGetPublic(const CCryName& n, float *v, int nID)
{
  CRenderer *rd = gRenDev;
  bool bFound = false;
  SRenderShaderResources *pRS;
  const char *cName = n.c_str();
  if (pRS = rd->m_RP.m_pShaderResources)
    bFound = SShaderParam::GetValue(cName, &pRS->m_ShaderParams, v, nID);
  if (!bFound && rd->m_RP.m_pShader)
  {
    bFound = SShaderParam::GetValue(cName, &rd->m_RP.m_pShader->m_PublicParams, v, nID);
  }
  return bFound;
}

#ifndef USE_PER_MATERIAL_PARAMS
bool SCGParam::GetTweakable(float *pData, int nComp) const
{
  bool bRes = false;
  pData[nComp] = 0;
  if (m_Flags & PF_AUTOMERGED)
  {
    CCryName n = GetParamCompName(nComp);
    bRes = sGetPublic(n, pData, nComp);
    return bRes;
  }
  // Get scalar or vector tweakable
  bRes = sGetPublic(m_Name, pData, nComp);

  return bRes;
}
#endif

SParamData::~SParamData()
{
}

SParamData::SParamData(const SParamData& sp)
{
  for (int i=0; i<4; i++)
  {
    m_CompNames[i] = sp.m_CompNames[i];
    d.nData64[i] = sp.d.nData64[i];
  }
}
