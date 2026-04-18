/*=============================================================================
D3DPostProcess : Direct3D specific post processing special effects
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

Todo:
* Erradicate StretchRect usage
* Cleanup code
* When we have a proper static branching support use it instead of shader switches inside code

=============================================================================*/

#include "StdAfx.h"
#include "NULL_Renderer.h"
#include "I3DEngine.h"
#include "../Common/PostProcess/PostEffects.h"

using namespace NPostEffects;

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CREPostProcessData::Create() 
{  
  // Initialize all post process data

  PostEffectMgr().Create();
}

void CREPostProcessData::Release() 
{
  // Free all used resources

  PostEffectMgr().Release();
}

void CREPostProcessData:: Reset()
{  
  // Reset all post process data

  PostEffectMgr().Reset();

}

void CFilterChromaShift::Render()
{
}

bool CAlphaTestAA::Preprocess()
{
  return ( IsActive() && CRenderer::CV_r_useedgeaa != 0 );
}

void CAlphaTestAA::Render()
{
}

void CDepthOfField::Render()
{
}

bool CMotionBlur::Preprocess()
{
  return true;
}
void CMotionBlur::Render()
{
}

void CSunShafts::Render() 
{
}
void CFilterSharpening::Render()
{
}
void CFilterBlurring::Render()
{
}

void CFilterRadialBlurring::Render()
{
}

void CFilterDepthEnhancement::Render()
{
}

void CViewMode::Render()
{
}

void CUnderwaterGodRays::Render()
{
}

void CVolumetricScattering::Render()
{
}

void CWaterDroplets::Render()
{
}

void CWaterFlow::Render()
{


}

void CWaterPuddles::Render()
{
}

bool CBloodSplats::Preprocess()
{
  return true;
}

void CBloodSplats::Render()
{
}

void CScreenFrost::Render()
{
}

void CScreenCondensation::Render()
{
}

bool CRainDrops::Preprocess()
{
  return true;
}
void CRainDrops::SpawnParticle( SRainDrop *&pParticle )
{
}
void CRainDrops::UpdateParticles()
{
}
void CRainDrops::RainDropsMapGen()
{
}
void CRainDrops::Render()
{
}

bool CNightVision::Preprocess()
{
  return true;
}

void CNightVision::Render()
{
}

bool CFlashBang::Preprocess()
{
  return true;
}

void CFlashBang::Render()
{
}
void CGlow::Render() 
{
}
void CColorCorrection::Render() 
{
}
void CGlittering::Render() 
{
}
void CGlitterSprites::Render()
{
}
void CGlitterSprites::Release()
{
}
int CGlitterSprites::Create()
{
  return 0;
}
void CPerspectiveWarp::Render()
{
}
void CPerspectiveWarp::Release()
{
}
int CPerspectiveWarp::Create()
{
  return 0;
}

void CChameleonCloak::Render()
{
}

void CDistantRain::Render()
{
}

void CAlienInterference::Render()
{
}

void CFillrateProfile::Render()
{
}

void CFilterMaskedBlurring::Render()
{
}

void CFilterGrain::Render()
{
}

void CFilterStylized::Render()
{
}

void CCryVision::Render()
{
}

void CColorGrading::Render()
{

}

void CWaterVolume::Render()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CREPostProcess::mfDraw(CShader *ef, SShaderPass *sfm)
{      
  return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////

