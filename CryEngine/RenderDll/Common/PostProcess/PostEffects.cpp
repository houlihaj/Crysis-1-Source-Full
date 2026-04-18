/*=============================================================================
PostEffects.cpp : Post processing effects implementation
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tiago Sousa

=============================================================================*/

#include "StdAfx.h"
#include "I3DEngine.h"
#include "PostEffects.h"

using namespace NPostEffects;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Engine specific post-effects
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMotionBlur::m_nQualityInfo = 0;
int CMotionBlur::m_nSamplesInfo = 0;
float CMotionBlur::m_nRotSamplesEst;
float CMotionBlur::m_nTransSamplesEst;

int CMotionBlur::Create()
{ 
  // prepare sphere
  int rings( 5 ); 
  int sections( 9 );  
  int radius( 10 ); 

  float sectionSlice( DEG2RAD( 360.0f / (float) sections ) );
  float ringSlice( DEG2RAD( 180.0f / (float) rings ) );

  m_pCamSphereVerts.Clear();

  struct_VERTEX_FORMAT_P3F_TEX2F vert;
  vert.st[0] = 1;
  vert.st[1] = 1;

  for(float r(1); r<= rings ; ++r)  // rings slices      
  {             
    for(int i(0); i<= sections; ++i) // section slices 
    {  
      // top/end pole cap
      if(r==1 && r == rings)
      {
        float w( sinf( r * ringSlice) );

        vert.xyz = Vec3( 0, 0, 1.0f);
        m_pCamSphereVerts.Add(vert); 

        float fVertexZ( cosf( r * ringSlice ) ); 
        vert.xyz = Vec3( cosf( i *sectionSlice ) * w , sinf( i * sectionSlice) * w, fVertexZ);
        m_pCamSphereVerts.Add(vert);

        vert.xyz = Vec3( cosf( (i+1) * sectionSlice ) * w , sinf( (i+1) * sectionSlice) * w, fVertexZ);
        m_pCamSphereVerts.Add(vert); 
      }
      else
      {
        // sections slices
        float w( sinf( r * ringSlice) );

        float fVertexZ( cosf( r * ringSlice ) );
        vert.xyz = Vec3( cosf( i *sectionSlice )* w , sinf( i * sectionSlice) * w, fVertexZ);
        m_pCamSphereVerts.Add(vert); 

        w = sinf( (r-1) * ringSlice); 
        fVertexZ = cosf( (r - 1) * ringSlice );
        vert.xyz = Vec3( cosf( (i+1) *sectionSlice )* w , sinf( (i+1) * sectionSlice) * w, fVertexZ);
        m_pCamSphereVerts.Add(vert); 
      }
    }
  }

  return 1;
}

void CMotionBlur::Release()
{ 
  m_pCamSphereVerts.Clear();
}

void CMotionBlur::Reset()
{  
  m_pActive->ResetParam(0.0f);

  m_pType->ResetParam(0.0f);  
  m_pQuality->ResetParam(2.0f);  

  m_pAmount->ResetParam(1.0f);

  m_pCameraSphereScale->ResetParam(2.0f);
  m_pExposureTime->ResetParam(0.004f);
  m_pVectorsScale->ResetParam(1.5f);  
  m_pChromaShift->ResetParam(0.0f);

  m_nQualityInfo = 0;
  m_nSamplesInfo = 0;
  m_nRotSamplesEst = 0.0f;
  m_nTransSamplesEst = 0.0f;

  m_bResetPrevScreen = 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CDepthOfField::Create()
{
  SAFE_RELEASE( m_pNoise );

  m_pNoise = CTexture::ForName("Textures/Defaults/vector_noise.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_RESIZE, eTF_Unknown);

  return true;
}

void CDepthOfField::Release()
{
  SAFE_RELEASE( m_pNoise );
}

void CDepthOfField::Reset()
{
  m_pUseMask->ResetParam(0.0f);
  m_pFocusDistance->ResetParam(3.5f);
  m_pFocusRange->ResetParam(0.0f);
  m_pMaxCoC->ResetParam(12.0f);
  m_pBlurAmount->ResetParam(1.0f);  
  m_pFocusMin->ResetParam(2.0f);
  m_pFocusMax->ResetParam(10.0f);
  m_pFocusLimit->ResetParam(100.0f);
  m_pUseMask->ResetParam(0.0f);
  m_pMaskTex->Release();    
  m_pDebug->ResetParam(0.0f);
  m_pActive->ResetParam(0.0f);
  
  m_pUserActive->ResetParam(0.0f);
  m_pUserFocusDistance->ResetParam(3.5f);
  m_pUserFocusRange->ResetParam(5.0f);
  m_pUserBlurAmount->ResetParam(1.0f);

  m_fUserFocusRangeCurr = 0;
  m_fUserFocusDistanceCurr = 0;
  m_fUserBlurAmountCurr = 0;
}

bool CDepthOfField::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_Medium, eSQ_Medium );

  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_usezpass || !CRenderer::CV_r_dof || (CRenderer::CV_r_dof == 2 && CRenderer::CV_r_hdrrendering) )  
    return false;

  if((m_pBlurAmount->GetParam() <= 0.1f && m_pUserBlurAmount->GetParam() <= 0.1f) )      
    return false;

  if( IsActive() || m_pUserActive->GetParam() )
  {
    return true;
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CAlphaTestAA::Reset()
{
  m_pActive->ResetParam(1.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CPerspectiveWarp::Reset()
{
  m_pActive->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CGlitterSprites::Update()
{
  // Create particles if required !
  if(!Create())
  {
    return;
  }
 
  Vec3 pCamPos=gRenDev->GetRCamera().Orig;  
  static Vec3 pLastCamPos=gRenDev->GetRCamera().Orig;  
  Vec3 pCamDir=(pCamPos-pLastCamPos).normalize();   
  float distAtten=1.0f/m_fDistRadius;
  
  // Update required sprites
  SpriteVecItor pItor, pItorEnd=m_pSpriteList.end();
  for(pItor=m_pSpriteList.begin(); pItor!=pItorEnd; ++pItor )
  {
    SSprite *pSprite=(*pItor);
    float fDist=pSprite->m_pPos.GetSquaredDistance(pCamPos);

    // if inside radius, update particle state
    if(fDist<m_fDistRadius*m_fDistRadius)
    { 
      // IsPointVisible -> was slowing down a bit
      pSprite->m_bVisible= 1; //gRenDev->GetCamera().IsPointVisible(pSprite->m_pPos);   
      continue; 
    }
    
    // Generate random direction, and make sure it's always in direction movement
    Vec3 randDir=Vec3( SPostEffectsUtils::srandf(), SPostEffectsUtils::srandf(), 0.0f)*m_fDistRadius; 
    
    if(pCamDir.Dot(randDir)<0.0f)     
    {
      randDir=-randDir; 
    } 

    pSprite->m_fSize=1.0f;    
    pSprite->m_bVisible=0;

    // Compute glitter position
    pSprite->m_pPos=pCamPos+randDir;     
    pSprite->m_pPos.z=gEnv->p3DEngine->GetTerrainElevation(pSprite->m_pPos.x, pSprite->m_pPos.y);

    // Store distance (used for smooth transition)
    pSprite->m_fAttenDist=pSprite->m_pPos.GetSquaredDistance(pCamPos);    
  } 

  pLastCamPos=gRenDev->GetRCamera().Orig;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CGlittering::Release()
{ 
  m_pGlitterSprites.Release();
}

void CGlittering::Reset()
{
  m_pActive->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CColorCorrection::Preprocess()
{
  if( CRenderer::CV_r_hdrrendering )
  {
    return false;
  }

  float fCyan = PostEffectMgr().GetByNameF("Global_ColorC");
  float fMagenta = PostEffectMgr().GetByNameF("Global_ColorM");
  float fYellow = PostEffectMgr().GetByNameF("Global_ColorY");
  float fLuminance = PostEffectMgr().GetByNameF("Global_ColorK");
  float fBrightness = PostEffectMgr().GetByNameF("Global_Brightness");
  float fContrast = PostEffectMgr().GetByNameF("Global_Contrast");
  float fSaturation = PostEffectMgr().GetByNameF("Global_Saturation");
  float fHue = PostEffectMgr().GetByNameF("Global_ColorHue");

  // check if any value is different from default, of it color correction is enabled (and hdr is off)
  if( fCyan != 0.0f || fMagenta != 0.0f || fYellow != 0.0f || fLuminance != 0.0f ||
      fBrightness != 1.0f || fContrast != 1.0f || fSaturation != 1.0f || fHue != 0.0f)
  {
    return true;
  }

  float fUserCyan = PostEffectMgr().GetByNameF("Global_User_ColorC");
  float fUserMagenta = PostEffectMgr().GetByNameF("Global_User_ColorM");
  float fUserYellow = PostEffectMgr().GetByNameF("Global_User_ColorY");
  float fUserLuminance = PostEffectMgr().GetByNameF("Global_User_ColorK");
  float fUserBrightness = PostEffectMgr().GetByNameF("Global_User_Brightness");
  float fUserContrast = PostEffectMgr().GetByNameF("Global_User_Contrast");
  float fUserSaturation = PostEffectMgr().GetByNameF("Global_User_Saturation");
  float fUserHue = PostEffectMgr().GetByNameF("Global_User_ColorHue");

  // check if any value is different from default, of it color correction is enabled (and hdr is off)
  if( fUserCyan != 0.0f || fUserMagenta != 0.0f || fUserYellow != 0.0f || fUserLuminance != 0.0f ||
      fUserBrightness != 1.0f || fUserContrast != 1.0f || fUserSaturation != 1.0f || fUserHue != 0.0f )
  {
    return true;
  }

  return false;
}

void CColorCorrection::Reset()
{
  m_pActive->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CGlow::Preprocess()
{
  // allowed on all specs
  return ( CRenderer::CV_r_glow != 0 && (IsActive() || !CRenderer::CV_r_hdrrendering));
}

void CGlow::Reset()
{
  m_pActive->ResetParam(0.0f);
  m_pScale->ResetParam(0.35f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CSunShafts::Preprocess()
{  
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_High, eSQ_High );

  if( !bQualityCheck )
    return false;

  if( iSystem->GetI3DEngine()->GetSunColor().len2() < 0.01f)
    return false;

  if( CRenderer::CV_r_sunshafts && IsActive() )
  {
    // Disable for interiors
    IVisArea *pCurrVisArea =  iSystem->GetI3DEngine()->GetVisAreaFromPos( gRenDev->GetRCamera().Orig );
    if( pCurrVisArea && !pCurrVisArea->IsConnectedToOutdoor() && !pCurrVisArea->IsAffectedByOutLights() )
      return false;

    return true;
  }

  return false;
}

void CSunShafts::Reset()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFilterSharpening::Preprocess()
{  
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_Medium, eSQ_Medium );

  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_postprocess_effects_filters)
    return false;

  if( fabs(m_pAmount->GetParam() - 1.0f ) > 0.09f )
  {
    return true;
  }

  return false;
}

void CFilterSharpening::Reset()
{
  m_pAmount->ResetParam(1.0f);
  m_pType->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFilterBlurring::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_Medium, eSQ_Medium );

  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_postprocess_effects_filters)
    return false;

  if( m_pAmount->GetParam() > 0.09f )
  {
    return true;
  }

  return false;
}

void CFilterBlurring::Reset()
{
  m_pAmount->ResetParam(0.0f);
  m_pType->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFilterRadialBlurring::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_Medium, eSQ_Medium );

  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_postprocess_effects_filters)
    return false;

  if( m_pAmount->GetParam() > 0.09f )
  {
    return true;
  }

  return false;
}

void CFilterRadialBlurring::Reset()
{
  m_pAmount->ResetParam(0.0f);
  m_pScreenPosX->ResetParam(0.5f);
  m_pScreenPosY->ResetParam(0.5f);
  m_pRadius->ResetParam(1.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFilterMaskedBlurring::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_Medium, eSQ_Medium );

  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_postprocess_effects_filters)
    return false;

  // do it in hdr mode instead
  if( gRenDev->m_RP.m_eQuality >= eRQ_High && CRenderer::CV_r_hdrrendering)
    return false;

  if( m_pAmount->GetParam() > 0.005f )
  {
    return true;
  }

  return false;
}

void CFilterMaskedBlurring::Reset()
{
  m_pAmount->ResetParam(0.0f);
  m_pMaskTex->Release(); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFilterDepthEnhancement::Preprocess()
{
  return false;

  if( !gRenDev->m_RP.m_eQuality || !CRenderer::CV_r_postprocess_effects_filters)
    return false;

  if( m_pAmount->GetParam() > 0.005f )
  {
    return true;
  }

  return false;
}

void CFilterDepthEnhancement::Reset()
{
  m_pAmount->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFilterChromaShift::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_Medium, eSQ_Medium );

  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_postprocess_effects_filters)
    return false;

  if( m_pAmount->GetParam() > 0.005f)
  {
    return true;
  }
  
  if( m_pUserAmount->GetParam() > 0.005f)
  {
    return true;
  }

  return false;
}

void CFilterChromaShift::Reset()
{
  m_pAmount->ResetParam(0.0f);
  m_pUserAmount->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFilterGrain::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_Medium, eSQ_Medium );
  if( !bQualityCheck )
    return false;

  if(!CRenderer::CV_r_postprocess_effects_filters)
    return false;

  if( m_pAmount->GetParam() > 0.09f)
  {
    return true;
  }

  return false;
}

void CFilterGrain::Reset()
{
  m_pAmount->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFilterStylized::Preprocess()
{
  return false;

  if( !gRenDev->m_RP.m_eQuality || !CRenderer::CV_r_postprocess_effects_filters)
    return false;

  if( m_pAmount->GetParam() > 0.005f)
  {
    return true;
  }

  return false;
}

void CFilterStylized::Reset()
{
  m_pAmount->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CColorGrading::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_VeryHigh, eSQ_High );
  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_colorgrading)
    return false;

  if( m_pMinInput->GetParam() != 0.0f || m_pGammaInput->GetParam() != 1.0f || m_pMaxInput->GetParam() != 255.0f ||
      m_pMinOutput->GetParam() != 0.0f || m_pMaxOutput->GetParam() != 255.0f || m_pBrightness->GetParam() != 1.0f ||
      m_pContrast->GetParam() != 1.0f || m_pSaturation->GetParam() != 1.0f || m_pPhotoFilterColorDensity->GetParam() != 0.0f ||
      m_pSelectiveColorCyans->GetParam() != 0.0f || m_pSelectiveColorMagentas->GetParam() != 0.0f || 
      m_pSelectiveColorYellows->GetParam() != 0.0f || m_pSelectiveColorBlacks->GetParam() != 0.0f ||
      m_pGrainAmount->GetParam() != 0.0f || m_pSharpenAmount->GetParam() != 0.0f)
    return true;

  // check offsets
  if( m_pSaturationOffset->GetParam() || m_pPhotoFilterColorDensityOffset->GetParam() || m_pGrainAmountOffset->GetParam() )
    return true;

  return false;
}

void CColorGrading::Reset()
{
  // reset user params
  m_pSaturationOffset->ResetParam(0.0f);
  m_pPhotoFilterColorOffset->ResetParamVec4( Vec4(0.0f, 0.0f, 0.0f, 0.0f) );
  m_pPhotoFilterColorDensityOffset->ResetParam(0.0f);
  m_pGrainAmountOffset->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CUnderwaterGodRays::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_High, eSQ_High );
  if( !bQualityCheck )
    return false;

  static ICVar *pVar = iConsole->GetCVar("e_water_ocean");
  bool bOceanEnabled = (pVar && pVar->GetIVal() != 0);

  int nR = SRendItem::m_RecurseLevel - 1;
  //bool bOceanVolumeVisible = (iSystem->GetI3DEngine()->GetOceanRenderFlags() & OCR_OCEANVOLUME_VISIBLE) != 0;

  if( CRenderer::CV_r_water_godrays && m_pAmount->GetParam() > 0.005f) // && bOceanEnabled && bOceanVolumeVisible)
  {
    float fWatLevel = SPostEffectsUtils::m_fWaterLevel;
    if( fWatLevel - 0.1f > gRenDev->GetRCamera().Orig.z)
    {
      // check water level

      return true;
    }    
  }
  
  return false;
}

void CUnderwaterGodRays::Reset()
{
  m_pAmount->ResetParam(1.0f);
  m_pQuality->ResetParam(1.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CVolumetricScattering::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_High, eSQ_High );
  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_postprocess_effects_gamefx)
    return false;

  if( m_pAmount->GetParam() > 0.005f)
  {
    return true;
  }

  return false;
}

void CVolumetricScattering::Reset()
{
  m_pAmount->ResetParam(0.0f);
  m_pType->ResetParam(0.0f);
  m_pQuality->ResetParam(1.0f);
  m_pTilling->ResetParam(1.0f);
  m_pSpeed->ResetParam(1.0f);
  m_pColor->ResetParamVec4(Vec4( 0.5f, 0.75f, 1.0f, 1.0f ));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDistantRain::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_High, eSQ_High );
  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_postprocess_effects_gamefx)
    return false;

  if ( m_pAmount->GetParam() > 0.005f && CRenderer::CV_r_distant_rain )
  {
    return true;
  }

  return false;
}

void CDistantRain::Reset()
{
  m_pAmount->ResetParam(0.0f);
  m_pSpeed->ResetParam(1.0f);
  m_pDistanceScale->ResetParam(1.0f);
  m_pColor->ResetParamVec4(Vec4( 1.0f, 1.0f, 1.0f, 1.0f ));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Game/Hud specific post-effects
////////////////////////////////////////////////////////////////////////////////////////////////////

void CViewMode::Reset()
{
  m_pActive->ResetParam(0.0f);
  m_pViewModeType->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CAlienInterference::Reset()
{
  m_pAmount->ResetParam(0.0f);
}

bool CAlienInterference::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_High, eSQ_High );
  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_postprocess_effects_gamefx)
    return false;

  if( m_pAmount->GetParam() > 0.09f)
  {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CWaterDroplets::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_High, eSQ_High );
  if( !bQualityCheck )
    return false;

  bool bUserActive = m_pAmount->GetParam() > 0.005f;

  if( iSystem->IsEditorMode() && !bUserActive )
    return false;

  if( CRenderer::CV_r_water_godrays)
  {
    float fWatLevel = SPostEffectsUtils::m_fWaterLevel;
    if( fWatLevel - 0.1f > gRenDev->GetRCamera().Orig.z)
    {
      m_fLastSpawnTime = 0.0f;
      m_fCurrLifeTime = 0.0;
      m_bWasUnderWater = true;
      return false; 
    }
    else 
      m_bWasUnderWater = false;

    // Coming out of water
    if( m_bWasUnderWater == false && m_fCurrLifeTime <1.0f)
    {
      const float fLifeTime = 1.5f;

      if( !m_fLastSpawnTime )
        m_fLastSpawnTime = gEnv->pTimer->GetCurrTime();

      m_fCurrLifeTime = (gEnv->pTimer->GetCurrTime() - m_fLastSpawnTime) / fLifeTime;
      if( m_fCurrLifeTime >= 1.0f)
      {
        if( !bUserActive ) // user enabled override
          return false;
      }

      // check water level
      return true;
    }

    if( bUserActive ) // user enabled override
      return true;
  }

  return false;
}

void CWaterDroplets::Reset()
{  
  m_fLastSpawnTime = 0.0f;
  m_fCurrLifeTime = 1000.0f;
  m_bWasUnderWater = true;
  m_pAmount->ResetParam(0.0f);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CWaterFlow::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_High, eSQ_High );
  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_postprocess_effects_gamefx)
    return false;

  if( m_pAmount->GetParam() > 0.005f)
    return true;

  return false;
}

void CWaterFlow::Reset()
{  
  m_pAmount->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CWaterPuddles::Preprocess()
{
  if(!CRenderer::CV_r_postprocess_effects_gamefx)
    return false;

  if( m_pAmount->GetParam() > 0.005f)
  {
    return true;
  }

  return false;  
}

void CWaterPuddles::Reset()
{
  m_pAmount->ResetParam(0.0f);
  m_fLastSpawnTime = 0.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CWaterVolume::Preprocess()
{
  if( !gRenDev->m_RP.m_eQuality )
    return false;

  if( m_pAmount->GetParam() > 0.005f)
  {
    return true;
  }

  return false;  
}

void CWaterVolume::Reset()
{
  m_pAmount->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CBloodSplats::Reset()
{  
  m_pType->ResetParam(0.0f);
  m_pAmount->ResetParam(0.0f);
  m_pSpawn->ResetParam(0.0f);
  m_pScale->ResetParam(1.0f);
  m_fSpawnTime = 0;
  m_nAccumCount = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CScreenFrost::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_Medium, eSQ_Medium );
  if( !bQualityCheck )
    return false;

  if( !CRenderer::CV_r_postprocess_effects_gamefx)
    return false;

  if( m_pAmount->GetParam() > 0.09f)
  {
    return true;
  }

  return false;
}

void CScreenFrost::Reset()
{  
  m_pAmount->ResetParam(0.0f);
  m_pCenterAmount->ResetParam(1.0f);
  m_fRandOffset = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CScreenCondensation::Preprocess()
{
  return false;

  if( !gRenDev->m_RP.m_eQuality  || !CRenderer::CV_r_postprocess_effects_gamefx)
    return false;

  if( m_pAmount->GetParam() > 0.005f)
  {
    return true;
  }

  return false;
}

void CScreenCondensation::Reset()
{  
  m_pAmount->ResetParam(0.0f);
  m_pCenterAmount->ResetParam(1.0f);
  m_fRandOffset = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CRainDrops::Create()
{
  // Already generated ? No need to proceed
  if( !m_pDropsLst.empty() )
  {
    return 1;
  }

  m_pDropsLst.reserve( m_nMaxDropsCount );
  for(int p=0; p< m_nMaxDropsCount; p++)
  {
    SRainDrop *pDrop = new SRainDrop;
    m_pDropsLst.push_back( pDrop );
  }

  return 1;
}

void CRainDrops::Release()
{ 
  SAFE_DELETE( m_pRainDrops );

  if(m_pDropsLst.empty())
  {
    return;
  }

  SRainDropsItor pItor, pItorEnd = m_pDropsLst.end(); 
  for(pItor=m_pDropsLst.begin(); pItor!=pItorEnd; ++pItor )
  {
    SAFE_DELETE((*pItor));
  } 
  m_pDropsLst.clear(); 
}

void CRainDrops::Reset()
{  
  SAFE_DELETE( m_pRainDrops );
  m_pAmount->ResetParam(0.0f);  
  m_pSpawnTimeDistance->ResetParam(0.35f);
  m_pSize->ResetParam(5.0f);
  m_pSizeVar->ResetParam(2.5f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CNightVision::Create()
{
  SAFE_RELEASE( m_pGradient );
  SAFE_RELEASE( m_pNoise );

  m_pGradient = CTexture::ForName("Textures/Defaults/nightvis_grad.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_RESIZE| FT_DONT_STREAM, eTF_Unknown);
  m_pNoise = CTexture::ForName("Textures/Defaults/vector_noise.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_RESIZE| FT_DONT_STREAM, eTF_Unknown);
  //m_pNoise = CTexture::ForName("Textures/Defaults/JumpNoiseHighFrequency_x27y19.dds", FT_DONT_ANISO | FT_DONT_RELEASE | FT_DONT_RESIZE, eTF_Unknown);

  return true;
}

void CNightVision::Release()
{
  SAFE_RELEASE( m_pGradient );
  SAFE_RELEASE( m_pNoise );
}

void CNightVision::Reset()
{
  m_pActive->ResetParam(0.0f);
  m_pAmount->ResetParam(1.0f);  

  m_bWasActive = false;
  m_fActiveTime = 0.0f;
  m_fRandOffset = 0;
  m_fPrevEyeAdaptionSpeed = 0.25f;
  m_fPrevEyeAdaptionBase = 100.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CCryVision::Reset()
{
  m_pActive->ResetParam(0.0f);
  m_pAmount->ResetParam(1.0f);  
  m_pType->ResetParam(1);
}

bool CCryVision::Preprocess()
{
  if( !CRenderer::CV_r_postprocess_effects_gamefx || !CRenderer::CV_r_customvisions)
    return false;

  // no need to procedd..
  float fType = m_pType->GetParam();
  uint32 nBatchMask = SRendItem::BatchFlags(EFSLIST_GENERAL) | SRendItem::BatchFlags(EFSLIST_TRANSP); 
  if ( (!(nBatchMask & FB_CUSTOM_RENDER)) && fType == 1.0f )
    return false;

  // if nightvision active, disable this
  CEffectParam *pParam = PostEffectMgr().GetByName("NightVision_Active"); 
  if( pParam && pParam->GetParam() )
    return false;

  if( m_pAmount->GetParam() > 0.005f )
    return true;

  return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CFlashBang::Release()
{ 
  SAFE_DELETE(m_pGhostImage);
}

void CFlashBang::Reset()
{
  SAFE_DELETE( m_pGhostImage );
  m_pActive->ResetParam(0.0f);  
  m_pTime->ResetParam(2.0f);
  m_pDifractionAmount->ResetParam(1.0f);
  m_pBlindAmount->ResetParam(0.5f);
  m_fBlindAmount = 1.0f;
  m_fSpawnTime = 0.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CChameleonCloak::Preprocess()
{
  return false;

  uint8 nCurrEffects = PostEffectMgr().GetPostBlendEffectsFlags();

  if( nCurrEffects & POST_EFFECT_CHAMELEONCLOAK )
  {
    return true;
  }

  return false;
}

void CChameleonCloak::Reset()
{
  m_pActive->ResetParam(0.0f);  
  m_pBlendAmount->ResetParam(0.5f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFillrateProfile::Preprocess()
{
  if( CRenderer::CV_r_postprocess_profile_fillrate == 1 )
    return true;

  return false;
}

void CFillrateProfile::Reset()
{
  m_pActive->ResetParam(0.0f);  
}
