/*=============================================================================
PostEffects.h : Post process effects
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* 18/06/2005: Re-organized (to minimize code dependencies/less annoying compiling times)
* Created by Tiago Sousa
=============================================================================*/

#ifndef _POSTEFFECTS_H_
#define _POSTEFFECTS_H_

#include "PostProcessUtils.h"

namespace NPostEffects
{

////////////////////////////////////////////////////////////////////////////////////////////////////
// Engine specific post-effects
////////////////////////////////////////////////////////////////////////////////////////////////////

class CMotionBlur: public CPostEffect
{
public:
  virtual int Create();
  virtual void Release();
  virtual void Render();  
  void RenderHDR();  
  virtual void Reset();
  virtual bool Preprocess();
  bool MotionDetection( float fCurrFrameTime, const Matrix44 &pCurrView, int &nQuality);

  virtual const char *GetName() const
  {
    return "MotionBlur";
  }

private:
  CMotionBlur()
  {          
    m_nRenderFlags = 0;
    // Register technique instance and it's parameters
    AddEffect( &m_pInstance );
    AddParamBool("MotionBlur_Active", m_pActive, 0);
    AddParamInt("MotionBlur_Type", m_pType, 0);
    AddParamInt("MotionBlur_Quality", m_pQuality, 1); // 0 = low, 1 = med, 2= high, 3= ultra-high, 4= crazy high, and so on
    AddParamFloatNoTransition("MotionBlur_Amount", m_pAmount, 1.0f);
    AddParamBool("MotionBlur_UseMask", m_pUseMask, 0);
    AddParamTex("MotionBlur_MaskTexName", m_pMaskTex, 0);
    AddParamFloat("MotionBlur_CameraSphereScale", m_pCameraSphereScale, 2.0f); // 2 meters by default
    AddParamFloat("MotionBlur_ExposureTime", m_pExposureTime, 0.004f);
    AddParamFloat("MotionBlur_VectorsScale", m_pVectorsScale, 1.5f);
    AddParamFloat("MotionBlur_ChromaShift", m_pChromaShift, 0.0f);  // chroma shift amount

    m_bResetPrevScreen = 1;
    m_fRotationAcc = 0.0f;
    m_pPrevView.SetIdentity();
  }

  virtual ~CMotionBlur()
  {
    Release();
  }

  // bool, int, float, float, bool, texture, float, float, int, float, float
  CEffectParam *m_pType, *m_pAmount, *m_pUseMask, *m_pMaskTex; 
  CEffectParam *m_pCameraSphereScale, *m_pVectorsScale, *m_pQuality, *m_pExposureTime, *m_pChromaShift; 
  bool m_bResetPrevScreen;

  PodArray<struct_VERTEX_FORMAT_P3F_TEX2F> m_pCamSphereVerts;

  Matrix44 m_pPrevView;    
  float m_fPrevFrameTime;
  float m_fRotationAcc;


public:
  static int m_nQualityInfo;
  static int m_nSamplesInfo;
  static float m_nRotSamplesEst, m_nTransSamplesEst;

  static CMotionBlur m_pInstance;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CDepthOfField: public CPostEffect
{
public:

  virtual int Create();
  virtual void Release();
  virtual void Render();  
  void RenderHDR();  
  virtual bool Preprocess();
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "DepthOfField";
  }

private:
  CDepthOfField()
  {
    // todo: add user values

    // Register technique and it's parameters
    AddEffect( &m_pInstance );
    AddParamBool("Dof_Active", m_pActive, 0);
    AddParamFloatNoTransition("Dof_FocusDistance", m_pFocusDistance, 3.5f);
    AddParamFloatNoTransition("Dof_FocusRange", m_pFocusRange, 0.0f);
    AddParamFloatNoTransition("Dof_FocusMin", m_pFocusMin, 2.0f);
    AddParamFloatNoTransition("Dof_FocusMax", m_pFocusMax, 10.0f);
    AddParamFloatNoTransition("Dof_FocusLimit", m_pFocusLimit, 100.0f);
    AddParamFloatNoTransition("Dof_MaxCoC", m_pMaxCoC, 12.0f);
    AddParamFloatNoTransition("Dof_BlurAmount", m_pBlurAmount, 1.0f);
    AddParamBool("Dof_UseMask", m_pUseMask, 0);
    AddParamTex("Dof_MaskTexName", m_pMaskTex, 0);      
    AddParamBool("Dof_Debug", m_pDebug, 0); 

    AddParamBool("Dof_User_Active", m_pUserActive, 0);
    AddParamFloatNoTransition("Dof_User_FocusDistance", m_pUserFocusDistance, 3.5f);
    AddParamFloatNoTransition("Dof_User_FocusRange", m_pUserFocusRange, 5.0f);
    AddParamFloatNoTransition("Dof_User_BlurAmount", m_pUserBlurAmount, 1.0f);

    AddParamFloatNoTransition("Dof_Tod_FocusRange", m_pTimeOfDayFocusRange, 1000.0f);
    AddParamFloatNoTransition("Dof_Tod_BlurAmount", m_pTimeOfDayBlurAmount, 0.0f);

    m_fUserFocusRangeCurr = 0;
    m_fUserFocusDistanceCurr = 0;
    m_fUserBlurAmountCurr = 0;
    m_pNoise = 0;
  }

  // bool, float, float, float, float
  CEffectParam *m_pUseMask, *m_pFocusDistance, *m_pFocusRange, *m_pMaxCoC, *m_pBlurAmount; 
  // float, float, float, CTexture, bool
  CEffectParam *m_pFocusMin, *m_pFocusMax, *m_pFocusLimit, *m_pMaskTex, *m_pDebug;
  // bool, float, float, float, float
  CEffectParam *m_pUserActive, *m_pUserFocusDistance, *m_pUserFocusRange, *m_pUserBlurAmount; 
  // float, float
  CEffectParam *m_pTimeOfDayFocusRange, *m_pTimeOfDayBlurAmount;

  float m_fUserFocusRangeCurr;
  float m_fUserFocusDistanceCurr;
  float m_fUserBlurAmountCurr;

  CTexture *m_pNoise;


public:
  static CDepthOfField m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAlphaTestAA: public CPostEffect
{
public:
  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "EdgeAA";
  }

private:
  CAlphaTestAA()
  {
    AddEffect( &m_pInstance );
    AddParamBool("EdgeAA_Active", m_pActive, 1);
  }

  static CAlphaTestAA m_pInstance;
};
  
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CPerspectiveWarp : public CPostEffect
{
public:

  virtual int Create();
  virtual void Release();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "PerspectiveWarp";
  }

private:
  CPerspectiveWarp(): m_pGridVB(0), m_pGridIB(0)
  {          
    AddEffect( &m_pInstance );
    AddParamBool("PerspectiveWarp_Active", m_pActive, 0);
  }

  virtual ~CPerspectiveWarp()
  {
    Release();
  }

  void* m_pGridVB;
  void* m_pGridIB;

  static CPerspectiveWarp m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CGlitterSprites
{
public:
  CGlitterSprites():m_pShader(0), m_fDistRadius(16.0f), m_pVB(0), m_pIB(0) { }

  // Generate glitter sprite list
  int Create();
  // Free all resources
  void Release();
  // Update glitter spite list
  void Update();
  // Render visible glitter
  void Render();

private:    
  CShader *m_pShader; 
  const float m_fDistRadius;
  
  // A glitter particle
  struct SSprite
  {
    SSprite():m_pPos(0,0,0), m_fSize(0.0f), m_fAttenDist(16.0f), m_bVisible(0) { }
    Vec3  m_pPos;
    float m_fSize;
    float m_fAttenDist;
    bool  m_bVisible;
  };    
  typedef std::vector<SSprite*> SpriteVec;
  typedef SpriteVec::iterator SpriteVecItor;
  SpriteVec m_pSpriteList;    

  // vertex/index buffers 
  void *m_pVB, *m_pIB;        
};

class CGlittering: public CPostEffect
{
public:

  virtual void Release();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "Glittering";
  }

private:

  CGlittering()
  {      
    m_nRenderFlags = 0;
    
    AddEffect( &m_pInstance );
    AddParamBool("Glittering_Active", m_pActive, 0);
  }

  virtual ~CGlittering()
  {
    Release();
  }

  CGlitterSprites m_pGlitterSprites;

  static CGlittering m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CColorCorrection: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "ColorCorrection";
  }

private:

  CColorCorrection()
  {            
    AddEffect( &m_pInstance );
    AddParamBool("ColorCorrection_Active", m_pActive, 0);

    // uses global parameters
  }

  static CColorCorrection m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CGlow: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "Glow";
  }

private:

  CGlow()
  {      
    AddEffect( &m_pInstance );
    AddParamBool("Glow_Active", m_pActive, 0);
    AddParamFloat("Glow_Scale", m_pScale, 0.5f); // default glow scale (half strength)      
  }

  // float
  CEffectParam *m_pScale;

  static CGlow m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CSunShafts: public CPostEffect
{
public:
  
  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "SunShafts";
  }

private:
  CSunShafts()
  {      
    AddEffect( &m_pInstance );
    AddParamBool("SunShafts_Active", m_pActive, 0);
    AddParamInt("SunShafts_Type", m_pShaftsType, 0); // default shafts type - highest quality
    AddParamFloatNoTransition("SunShafts_Amount", m_pShaftsAmount, 0.25f); // shafts visibility
    AddParamFloatNoTransition("SunShafts_RaysAmount", m_pRaysAmount, 0.25f); // rays visibility
    AddParamFloatNoTransition("SunShafts_RaysAttenuation", m_pRaysAttenuation, 5.0f); // rays attenuation
  }

  // int, float, float, float
  CEffectParam *m_pShaftsType, *m_pShaftsAmount, *m_pRaysAmount, *m_pRaysAttenuation;     

  static CSunShafts m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFilterSharpening: public CPostEffect
{
public:
  
  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "FilterSharpening";
  }

private:
  CFilterSharpening()
  {
    AddEffect( &m_pInstance );     
    AddParamInt("FilterSharpening_Type", m_pType, 0);
    AddParamFloat("FilterSharpening_Amount", m_pAmount, 1.0f);
  }

  // float, int
  CEffectParam *m_pAmount, *m_pType;    

  static CFilterSharpening m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFilterBlurring: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "FilterBlurring";
  }

private:
  CFilterBlurring()
  {     
    AddEffect( &m_pInstance );      
    AddParamInt("FilterBlurring_Type", m_pType, 0);
    AddParamFloat("FilterBlurring_Amount", m_pAmount, 0.0f);
  }

  // float, int
  CEffectParam *m_pAmount, *m_pType;    

  static CFilterBlurring m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFilterRadialBlurring: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "FilterRadialBlurring";
  }

private:
  CFilterRadialBlurring()
  {     
    AddEffect( &m_pInstance );            
    AddParamFloatNoTransition("FilterRadialBlurring_Amount", m_pAmount, 0.0f);
    AddParamFloatNoTransition("FilterRadialBlurring_ScreenPosX", m_pScreenPosX, 0.5f);
    AddParamFloatNoTransition("FilterRadialBlurring_ScreenPosY", m_pScreenPosY, 0.5f);
    AddParamFloatNoTransition("FilterRadialBlurring_Radius", m_pRadius, 1.0f);      
  }

  // float, float, float, float
  CEffectParam *m_pAmount, *m_pScreenPosX, *m_pScreenPosY, *m_pRadius;    

  static CFilterRadialBlurring m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFilterMaskedBlurring: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "FilterMaskedBlurring";
  }

private:
  CFilterMaskedBlurring()
  {     
    AddEffect( &m_pInstance );      
    AddParamFloat("FilterMaskedBlurring_Amount", m_pAmount, 0.0f);
    AddParamTex("FilterMaskedBlurring_MaskTexName", m_pMaskTex, 0);      
  }

  // float, CTexture
  CEffectParam *m_pAmount, *m_pMaskTex;    

public:
  static CFilterMaskedBlurring m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFilterDepthEnhancement: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "FilterDepthEnhancement";
  }

private:
  CFilterDepthEnhancement()
  {     
    AddEffect( &m_pInstance );            
    AddParamFloat("FilterDepthEnhancement_Amount", m_pAmount, 0.0f);
  }

  // float, float, float, float
  CEffectParam *m_pAmount;    

  static CFilterDepthEnhancement m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFilterChromaShift: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "FilterChromaShift";
  }

private:
  CFilterChromaShift()
  {     
    AddEffect( &m_pInstance );            
    AddParamFloat("FilterChromaShift_Amount", m_pAmount, 0.0f);
    AddParamFloat("FilterChromaShift_User_Amount", m_pUserAmount, 0.0f);
  }

  // float, float
  CEffectParam *m_pAmount, *m_pUserAmount;

  static CFilterChromaShift m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFilterGrain: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "FilterGrain";
  }

private:
  CFilterGrain()
  {     
    AddEffect( &m_pInstance );            
    AddParamFloat("FilterGrain_Amount", m_pAmount, 0.0f);
  }

  // float, float
  CEffectParam *m_pAmount;

  static CFilterGrain m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFilterStylized: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "FilterStylized";
  }

private:
  CFilterStylized()
  {     
    AddEffect( &m_pInstance );            
    AddParamFloat("FilterStylized_Amount", m_pAmount, 0.0f);
  }

  // float, float
  CEffectParam *m_pAmount;

  static CFilterStylized m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CColorGrading: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "ColorGrading";
  }

private:
  CColorGrading()
  {     
    AddEffect( &m_pInstance );            

    // levels adjustment
    AddParamFloatNoTransition("ColorGrading_minInput", m_pMinInput, 0.0f);
    AddParamFloatNoTransition("ColorGrading_gammaInput", m_pGammaInput, 1.0f);
    AddParamFloatNoTransition("ColorGrading_maxInput", m_pMaxInput, 255.0f);
    AddParamFloatNoTransition("ColorGrading_minOutput", m_pMinOutput, 0.0f);
    AddParamFloatNoTransition("ColorGrading_maxOutput", m_pMaxOutput, 255.0f);

    // generic color adjustment
    AddParamFloatNoTransition("ColorGrading_Brightness", m_pBrightness, 1.0f);
    AddParamFloatNoTransition("ColorGrading_Contrast", m_pContrast, 1.0f);
    AddParamFloatNoTransition("ColorGrading_Saturation", m_pSaturation, 1.0f);

    // filter color
    m_pDefaultPhotoFilterColor = Vec4( 0.952f, 0.517f, 0.09f, 1.0f );
    AddParamVec4NoTransition("clr_ColorGrading_PhotoFilterColor", m_pPhotoFilterColor,  m_pDefaultPhotoFilterColor);  // use photoshop default orange
    AddParamFloatNoTransition("ColorGrading_PhotoFilterColorDensity", m_pPhotoFilterColorDensity, 0.0f);

    // selective color
    AddParamVec4NoTransition("clr_ColorGrading_SelectiveColor", m_pSelectiveColor, Vec4(0.0f,0.0f,0.0f,0.0f)),
    AddParamFloatNoTransition("ColorGrading_SelectiveColorCyans", m_pSelectiveColorCyans, 0.0f),
    AddParamFloatNoTransition("ColorGrading_SelectiveColorMagentas", m_pSelectiveColorMagentas, 0.0f),
    AddParamFloatNoTransition("ColorGrading_SelectiveColorYellows", m_pSelectiveColorYellows, 0.0f),
    AddParamFloatNoTransition("ColorGrading_SelectiveColorBlacks", m_pSelectiveColorBlacks, 0.0f),

    // mist adjustment
    AddParamFloatNoTransition("ColorGrading_GrainAmount", m_pGrainAmount, 0.0f);
    AddParamFloatNoTransition("ColorGrading_SharpenAmount", m_pSharpenAmount, 1.0f);

    // user params
    AddParamFloatNoTransition("ColorGrading_Saturation_Offset", m_pSaturationOffset, 0.0f);
    AddParamVec4NoTransition("ColorGrading_PhotoFilterColor_Offset", m_pPhotoFilterColorOffset, Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    AddParamFloatNoTransition("ColorGrading_PhotoFilterColorDensity_Offset", m_pPhotoFilterColorDensityOffset, 0.0f);
    AddParamFloatNoTransition("ColorGrading_GrainAmount_Offset", m_pGrainAmountOffset, 0.0f);
  }

  // levels adjustment
  CEffectParam *m_pMinInput;
  CEffectParam *m_pGammaInput;
  CEffectParam *m_pMaxInput;
  CEffectParam *m_pMinOutput;
  CEffectParam *m_pMaxOutput;

  // generic color adjustment
  CEffectParam *m_pBrightness;
  CEffectParam *m_pContrast;
  CEffectParam *m_pSaturation;
  CEffectParam *m_pSaturationOffset;

  // filter color
  CEffectParam *m_pPhotoFilterColor;
  CEffectParam *m_pPhotoFilterColorDensity;
  CEffectParam *m_pPhotoFilterColorOffset;
  CEffectParam *m_pPhotoFilterColorDensityOffset;
  Vec4 m_pDefaultPhotoFilterColor;

  // selective color
  CEffectParam *m_pSelectiveColor;
  CEffectParam *m_pSelectiveColorCyans;
  CEffectParam *m_pSelectiveColorMagentas;
  CEffectParam *m_pSelectiveColorYellows;
  CEffectParam *m_pSelectiveColorBlacks;

  // misc adjustments
  CEffectParam *m_pGrainAmount;
  CEffectParam *m_pGrainAmountOffset;

  CEffectParam *m_pSharpenAmount;

  static CColorGrading m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CUnderwaterGodRays: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "UnderwaterGodRays";
  }

private:
  CUnderwaterGodRays()
  {     
    AddEffect( &m_pInstance );            
    AddParamFloat("UnderwaterGodRays_Amount", m_pAmount, 1.0f);
    AddParamInt("UnderwaterGodRays_Quality", m_pQuality, 1); // 0 = low, 1 = med, 2= high, 3= ultra-high, 4= crazy high, and so on

  }

  // float, int
  CEffectParam *m_pAmount, *m_pQuality;

  static CUnderwaterGodRays m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CVolumetricScattering: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "VolumetricScattering";
  }

private:

  CVolumetricScattering()
  {     
    AddEffect( &m_pInstance );            
    AddParamFloat("VolumetricScattering_Amount", m_pAmount, 0.0f);
    AddParamFloat("VolumetricScattering_Tilling", m_pTilling, 1.0f);
    AddParamFloat("VolumetricScattering_Speed", m_pSpeed, 1.0f);
    AddParamVec4("clr_VolumetricScattering_Color", m_pColor, Vec4( 0.5f, 0.75f, 1.0f, 1.0f ) );    

    AddParamInt("VolumetricScattering_Type", m_pType, 0); // 0 = alien environment, 1 = ?, 2 = ?? ???
    AddParamInt("VolumetricScattering_Quality", m_pQuality, 1); // 0 = low, 1 = med, 2= high, 3= ultra-high, 4= crazy high, and so on    
  }

  // float, int, int
  CEffectParam *m_pAmount, *m_pTilling, *m_pSpeed, *m_pColor, *m_pType, *m_pQuality;

  static CVolumetricScattering m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CDistantRain: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "DistantRain";
  }

private:
  CDistantRain()
  {     
    AddEffect( &m_pInstance );            
    AddParamFloat("DistantRain_Amount", m_pAmount, 0.0f);
    AddParamFloatNoTransition("DistantRain_Speed", m_pSpeed, 1.0f);
    AddParamFloat("DistantRain_DistanceScale", m_pDistanceScale, 1.0f);
    AddParamVec4("clr_DistantRain_Color", m_pColor, Vec4( 1.0f, 1.0f, 1.0f, 1.0f ) );    
  }

  // float, float, float, vec4
  CEffectParam *m_pAmount, *m_pSpeed, *m_pDistanceScale, *m_pColor;

  static CDistantRain m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Game/Hud specific post-effects
////////////////////////////////////////////////////////////////////////////////////////////////////

// todo: add support for custom textures
class CViewMode: public CPostEffect
{
public:

  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "ViewMode";
  }

private:
  CViewMode()
  {     
    AddEffect( &m_pInstance );
    AddParamBool("ViewMode_Active", m_pActive, 0);
    AddParamInt("ViewMode_Type", m_pViewModeType, 0);
  }

  // int
  CEffectParam *m_pViewModeType; 

  static CViewMode m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAlienInterference: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "AlienInterference";
  }

private:
  CAlienInterference()
  {     
    AddEffect( &m_pInstance );
    AddParamFloat("AlienInterference_Amount", m_pAmount, 0);
  }

  // float
  CEffectParam *m_pAmount; 

  static CAlienInterference m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CWaterDroplets: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "WaterDroplets";
  }

private:
  CWaterDroplets()
  {     
    AddEffect( &m_pInstance );      
    AddParamFloat("WaterDroplets_Amount", m_pAmount, 0.0f);

    m_fLastSpawnTime = 0.0f;
    m_fCurrLifeTime = 1000.0f;
    m_bWasUnderWater = true;
  }

  bool m_bWasUnderWater;
  float m_fLastSpawnTime;
  float m_fCurrLifeTime;

  // float
  CEffectParam *m_pAmount;

  static CWaterDroplets m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CWaterFlow: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "WaterFlow";
  }

private:
  CWaterFlow()
  {     
    AddEffect( &m_pInstance );      
    AddParamFloat("WaterFlow_Amount", m_pAmount, 0.0f);
  }

  // float
  CEffectParam *m_pAmount;

  static CWaterFlow m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CWaterPuddles: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();
  int GetCurrentPuddle() 
  {
    return m_nCurrPuddleID;
  }

  virtual const char *GetName() const
  {
    return "WaterPuddles";
  }

private:
  CWaterPuddles()
  {     
    m_nRenderFlags = 0;
    AddEffect( &m_pInstance );      
    AddParamFloat("WaterPuddles_Amount", m_pAmount, 0.0f);
    m_nCurrPuddleID = 0;
    m_fLastSpawnTime = 0.0f;
  }

  // float
  CEffectParam *m_pAmount;
  int m_nCurrPuddleID;
  float m_fLastSpawnTime;

  static CWaterPuddles m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CWaterVolume: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();
  int GetCurrentPuddle() 
  {
    return m_nCurrPuddleID;
  }

  virtual const char *GetName() const
  {
    return "WaterVolume";
  }

private:
  CWaterVolume()
  {     
    m_nRenderFlags = 0;
    AddEffect( &m_pInstance );      
    AddParamFloat("WaterVolume_Amount", m_pAmount, 0.0f);
    m_nCurrPuddleID = 0;
  }

  // float
  CEffectParam *m_pAmount;
  int m_nCurrPuddleID;

  static CWaterVolume m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

//todo: use generic screen particles

class CBloodSplats: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "BloodSplats";
  }

private:
  CBloodSplats()
  {     
    AddEffect( &m_pInstance );      
    AddParamInt("BloodSplats_Type", m_pType, 0); // default blood type - human
    AddParamFloatNoTransition("BloodSplats_Amount", m_pAmount, 1.0f); // amount of visible blood
    AddParamBool("BloodSplats_Spawn", m_pSpawn, 0);  // spawn particles
    AddParamFloatNoTransition("BloodSplats_Scale", m_pScale, 1.0f);  // particles scaling

    m_fSpawnTime = 0;
    m_nAccumCount = 0;
  }

  // int, float, bool
  CEffectParam *m_pType, *m_pAmount, *m_pSpawn, *m_pScale;
  float m_fSpawnTime;
  int m_nAccumCount; // how many accumulated frames

  static CBloodSplats m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CScreenFrost: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "ScreenFrost";
  }

private:
  CScreenFrost()
  {     
    AddEffect( &m_pInstance );      
    AddParamFloat("ScreenFrost_Amount", m_pAmount, 0.0f); // amount of visible frost
    AddParamFloat("ScreenFrost_CenterAmount", m_pCenterAmount, 1.0f); // amount of visible frost in center

    m_fRandOffset = 0;
  }

  // float, float
  CEffectParam *m_pAmount, *m_pCenterAmount;
  float m_fRandOffset;

  static CScreenFrost m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CScreenCondensation: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "ScreenCondensation";
  }

private:
  CScreenCondensation()
  {     
    AddEffect( &m_pInstance );      
    AddParamFloat("ScreenCondensation_Amount", m_pAmount, 0.0f); // amount of visible condensation
    AddParamFloat("ScreenCondensation_CenterAmount", m_pCenterAmount, 1.0f); // amount of visible condensation in center

    m_fRandOffset = 0;
  }

  // float, float
  CEffectParam *m_pAmount, *m_pCenterAmount;
  float m_fRandOffset;

  static CScreenCondensation m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CRainDrops: public CPostEffect
{
public:

  virtual int Create();
  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();
  virtual void Release();

  virtual const char *GetName() const
  {
    return "RainDrops";
  }

private:

  CRainDrops()
  {     
    AddEffect( &m_pInstance );      
    AddParamFloat("RainDrops_Amount", m_pAmount, 0.0f); // amount of visible droplets
    AddParamFloat("RainDrops_SpawnTimeDistance", m_pSpawnTimeDistance, 0.35f); // amount of visible droplets
    AddParamFloat("RainDrops_Size", m_pSize, 5.0f); // drop size
    AddParamFloat("RainDrops_SizeVariation", m_pSizeVar, 2.5f); // drop size variation

    m_fRandOffset = 0;
    m_pRainDrops = 0;      

    m_pVelocityProj = Vec3(0,0,0);

    m_nAliveDrops = 0;

    m_pPrevView.SetIdentity();    
    m_pViewProjPrev.SetIdentity();
  }

  virtual ~CRainDrops()
  {
    Release();
  }

  // Rain particle properties
  struct SRainDrop  
  {      
    // set default data
    SRainDrop(): m_pPos(0,0,0), m_fSize(5.0f), m_fSizeVar(2.5f), m_fSpawnTime(0.0f), m_fLifeTime(2.0f), m_fLifeTimeVar(1.0f),
      m_fWeight(1.0f), m_fWeightVar(0.25f)
    {

    }

    // Screen position
    Vec3 m_pPos;               
    // Size and variation (bigger also means more weight)
    float m_fSize, m_fSizeVar; 
    // Spawn time
    float m_fSpawnTime;                
    // Life time and variation
    float m_fLifeTime, m_fLifeTimeVar; 
    // Weight and variation
    float m_fWeight, m_fWeightVar; 
  };

  // Spawn a particle
  void SpawnParticle( SRainDrop *&pParticle );   
  // Update all particles
  void UpdateParticles();
  // Generate rain drops map
  void RainDropsMapGen();

  // float
  CEffectParam *m_pAmount;
  CEffectParam *m_pSpawnTimeDistance;
  CEffectParam *m_pSize;
  CEffectParam *m_pSizeVar;
  float m_fRandOffset;

  SDynTexture *m_pRainDrops;

  //todo: use generic screen particles
  typedef std::vector<SRainDrop*> SRainDropsVec;
  typedef SRainDropsVec::iterator SRainDropsItor;
  SRainDropsVec m_pDropsLst;    

  Vec3 m_pVelocityProj;
  Matrix44 m_pPrevView;    
  Matrix44 m_pViewProjPrev;

  int m_nAliveDrops;
  static CRainDrops m_pInstance;
  static const int m_nMaxDropsCount = 100;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CNightVision: public CPostEffect
{
public:

  virtual int Create();
  virtual void Release();
  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "NightVision";
  }

private:
  CNightVision()
  {     
    AddEffect( &m_pInstance );
    AddParamBool("NightVision_Active", m_pActive, 0);
    AddParamFloat("NightVision_BlindAmount", m_pAmount, 0.0f);

    m_bWasActive = false;
    m_fRandOffset = 0;    
    m_fPrevEyeAdaptionBase = 0.25f;
    m_fPrevEyeAdaptionSpeed = 100.0f;

    m_pGradient = 0;
    m_pNoise = 0;
    m_fActiveTime = 0.0f;
  }

  // float
  CEffectParam *m_pAmount;

  bool m_bWasActive;
  float m_fActiveTime;

  float m_fRandOffset;
  float m_fPrevEyeAdaptionSpeed;
  float m_fPrevEyeAdaptionBase;

  CTexture *m_pGradient;
  CTexture *m_pNoise;

  static CNightVision m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CCryVision: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "CryVision";
  }

private:
  CCryVision()
  {     
    m_nRenderFlags = 0;
    AddEffect( &m_pInstance );
    AddParamBool("CryVision_Active", m_pActive, 0);
    AddParamFloatNoTransition("CryVision_Amount", m_pAmount, 1.0f); //0.0f gives funky blending result ? investigate
    AddParamInt("CryVision_Type", m_pType, 1);
  }

  // float
  CEffectParam *m_pAmount, *m_pType;

  static CCryVision m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFlashBang: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Release();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "FlashBang";
  }

private:
  CFlashBang()
  {     
    AddEffect( &m_pInstance );
    AddParamBool("FlashBang_Active", m_pActive, 0);     
    AddParamFloat("FlashBang_DifractionAmount", m_pDifractionAmount, 1.0f);     
    AddParamFloat("FlashBang_Time", m_pTime, 2.0f); // flashbang time duration in seconds 
    AddParamFloat("FlashBang_BlindAmount", m_pBlindAmount, 0.5f); // flashbang blind time (fraction of frashbang time)

    m_pGhostImage = 0;
    m_fBlindAmount = 1.0f;
    m_fSpawnTime = 0.0f;
  }

  virtual ~CFlashBang()
  {
    Release();
  }

  SDynTexture *m_pGhostImage;

  float m_fBlindAmount;
  float m_fSpawnTime;

  // float, float
  CEffectParam *m_pTime, *m_pDifractionAmount, *m_pBlindAmount;

  static CFlashBang m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CChameleonCloak: public CPostEffect
{
public:

  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "ChameleonCloak";
  }

private:
  CChameleonCloak()
  {     
    AddEffect( &m_pInstance );
    AddParamBool("ChameleonCloak_Active", m_pActive, 0);     
    AddParamFloat("ChameleonCloak_BlendAmount", m_pBlendAmount, 0.5f);     
  }

  virtual ~CChameleonCloak()
  {
    Release();
  }

  // float
  CEffectParam *m_pBlendAmount;

  static CChameleonCloak m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFillrateProfile: public CPostEffect
{
public:
  virtual bool Preprocess();
  virtual void Render();  
  virtual void Reset();

  virtual const char *GetName() const
  {
    return "FillrateProfile";
  }

private:
  CFillrateProfile()
  {
    AddEffect( &m_pInstance );
    AddParamBool("FillrateProfile_Active", m_pActive, 1);
  }

  static CFillrateProfile m_pInstance;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

}

#endif
