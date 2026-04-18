
#ifndef __CREFLARES_H__
#define __CREFLARES_H__

//=================================================
// Flares

enum ELightRGB
{
  eLIGHT_Identity,
  eLIGHT_Fixed,
  eLIGHT_Poly,
  eLIGHT_Style,
  eLIGHT_Object,
};

struct SDynTexture;

class CREFlare : public CRendElement
{
  friend class CRenderer;

  SDynTexture *m_pSunRaysMask;

public:
  Vec3 m_Normal;
  float m_fScaleCorona;
  ColorF m_Color;
  float m_fMinLight;
  float m_fDistSizeFactor;
  float m_fDistIntensityFactor;
  bool  m_bBlind;
  float m_fSizeBlindBias;
  float m_fSizeBlindScale;
  float m_fIntensBlindBias;
  float m_fIntensBlindScale;
  float m_fFadeTime;
  float m_fVisAreaScale;
  ELightRGB m_eLightRGB;
  uint m_LightStyle;
  ColorF m_fColor;
  int m_UpdateFrame;
  int m_nFrameQuery;
  SShaderPass *m_Pass;
  int m_Importance;

  CREFlare()
  {
    mfSetType(eDATA_Flare);
    mfUpdateFlags(FCEF_TRANSFORM);
    m_fMinLight = 0.0f;
    m_eLightRGB = eLIGHT_Identity;
    m_fColor = Col_White;
    m_UpdateFrame = -1;
    m_fDistSizeFactor = 1.0f;
    m_fDistIntensityFactor = 1.0f;
    m_fSizeBlindBias = 0;
    m_nFrameQuery = -1;
    m_fSizeBlindScale = 1.0f;
    m_fIntensBlindBias = 0;
    m_fIntensBlindScale = 1.0f;
    m_fVisAreaScale = 1.0f;
    m_fFadeTime = -1.0f;
    m_bBlind = false;
    m_fScaleCorona = 0.6f;
    m_Pass = NULL;
    m_Importance = 1;
    m_pSunRaysMask = 0;
    m_Color = Col_White;
  }
  virtual ~CREFlare()
  {
    SAFE_DELETE(m_Pass);
  }
  virtual bool mfCompile(CParserBin& Parser, SParserFrame& Frame);
  virtual void mfPrepare(void);
  virtual bool mfDraw(CShader *ef, SShaderPass *sfm);
  virtual float mfDistanceToCameraSquared(Matrix34& matInst);

  void mfDrawCorona(CShader *ef, ColorF &col);
  bool mfCheckVis(CRenderObject *obj);
};

#endif  // __CREFLARES_H__
