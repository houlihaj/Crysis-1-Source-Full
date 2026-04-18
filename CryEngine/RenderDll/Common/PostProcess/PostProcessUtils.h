/*=============================================================================
PostProcessUtils.h : Post processing common utilities
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* 18/06/2005: Re-organized (to minimize code dependencies/less annoying compiling times)
* Created by Tiago Sousa

=============================================================================*/

#ifndef _POSTPROCESSUTILS_H_
#define _POSTPROCESSUTILS_H_

struct SD3DSurface;
class CShader;

namespace NPostEffects
{

struct SPostEffectsUtils
{
public:

  // Create all resources
  static bool Create();

  // Release all used resources
  static void Release();

  // Create a render target
  static bool CreateRenderTarget(const char *szTexName, CTexture *&pTex, int iWidth, int iHeight, bool bUseAlpha, bool bMipMaps = 0, int nCustomID=-1);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // Utilities to void some code duplication
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  // Begins render pass utility - for post process stuff only pass 0 assumed to be used
  static void ShBeginPass( CShader *pShader, const CCryName& TechName, uint nFlags = 0);

  // Ends render pass utility
  static void ShEndPass();

  // Set vertex shader constant utility
  static void ShSetParamVS( const CCryName& pParamName, const Vec4 &pParam );

  // Set pixel shader constant utility
  static void ShSetParamPS( const CCryName& pParamName, const Vec4 &pParam );

  // Draws fullscreen aligned quad
  static void DrawFullScreenQuad(int nTexWidth, int nTexHeight);

  // Draws screen aligned quad
  static void DrawScreenQuad(int nTexWidth, int nTexHeight, float x0=0, float y0=0, float x1=1, float y1=1);
  
  // Draws fullscreen aligned quad and pass also camera edges (useful for world space reconstruction)
  static void DrawFullScreenQuadWPOS(int nTexWidth, int nTexHeight);

  // Sets a texture
  static void SetTexture(CTexture *pTex, int nStage, int nFilter=FILTER_LINEAR, int nClamp=1);

  // Copy a texture into other texture
  virtual void StretchRect(CTexture *pSrc, CTexture *&pDst) = 0;

  // Copy screen into texture (wrapper for StretchRect)
  virtual void CopyScreenToTexture(CTexture *&pDst, bool bStretch=0) = 0;

  // Apply gaussian blur a texture
  virtual void TexBlurGaussian(CTexture *pTex, int nAmount=1, float fScale=1.0f, float fDistribution=5.0f, bool bAlphaOnly = false, CTexture *pMask = 0) = 0;
  
  // Clear active render target region
  static void ClearScreen(float r, float g, float b, float a);

  // Reset texture states
  static void ResetTexStages()
  {
    for (int i=0; i<MAX_TMU; i++)
    {
      CTexture::m_TexStages[i].m_Texture = NULL;
    }
  }

  // Log utility
  static void Log(char *pszMsg)
  {
    if(gRenDev->m_LogFile && pszMsg)
    {
      gRenDev->Logv(SRendItem::m_RecurseLevel, pszMsg);
    }
  }

  // Get current color matrix set up by global color parameters
  static Matrix44 &GetColorMatrix();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // Math utils
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  // Linear interpolation
  static float InterpolateLinear(float p1, float p2, float t) 
  { 
    return p1 + (p2 - p1) * t; 
  };

  // Cubic interpolation
  static float InterpolateCubic(float p1, float p2, float p3, float p4, float t) 
  { 
    float t2 = t*t; 
    return (((-p1 * 2.0f) + (p2 * 5.0f) - (p3 * 4.0f) + p4) / 6.0f) * t2 * t + (p1 + p3 - (2.0f * p2)) * t2 + (((-4.0f * p1) + p2 + (p3 * 4.0f) - p4) / 6.0f) * t + p2; 
  };

  // Sine interpolation
  static float InterpolateSine(float p1, float p2, float p3, float p4, float t) 
  { 
    return p2 + (t * (p3 - p2)) + (sinf(t * PI) * ((p2 + p2) - p1 - p3 + (t * (p1-(p2+p2+p2) + (p3 + p3 + p3) - p4))) / 8.0f); 
  };

  // Return normalized random number
  static float randf()
  {
    return Random();
  }

  // Return signed normalized random number
  static float srandf()
  {
    return Random() *  2.0f - 1.0f ;
  }

  // Returns closest power of 2 size
  static int GetClosestPow2Size(int size)
  {
    float fPower= floorf(logf((float)size)/logf(2.0f));
    int nResize=int(powf(2.0f, fPower));

    // Clamp
    if( nResize >= 512)
    {
      nResize = 512;
    }

    return nResize;
  }

  static float GaussianDistribution1D(float x, float rho)
  {
    float g = 1.0f / ( rho * sqrtf(2.0f * PI)); 
    g *= expf( -(x * x)/(2.0f * rho * rho) );
    return g;
  }

public:

  static SD3DSurface *m_pCurDepthSurface;
  static RECT m_pScreenRect;
  static ITimer *m_pTimer;           
  static int m_iFrameCounter;
  static int m_nFrameID;
  
  static CShader *m_pCurrShader;

  static Matrix44 m_pView;
  static Matrix44 m_pProj;
  static Matrix44 m_pViewProj;

  static Matrix44 m_pColorMat;
  static float m_fWaterLevel;

protected:
  
  SPostEffectsUtils() { }
  virtual ~SPostEffectsUtils() 
  { 
    Release();
  }


};

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// Simple particle system implementation for common screen based particles usage

// Particle properties
struct SScreenParticle
{      
  // set default data
  SScreenParticle(): m_pPos(0,0,0), m_pPrevPos(0,0,0), m_pPosStart(0,0,0), m_fSize(5.0f), m_fSizeVar(2.5f), m_fSpawnTime(0.0f), m_fLifeTime(2.0f), m_fLifeTimeVar(1.0f),
    m_fWeight(1.0f), m_fWeightVar(0.25f), m_pVelocity(0, 0, 0), m_fVelocityVar(0.0f)
  {

  }

  // Screen position
  Vec3 m_pPos, m_pPrevPos, m_pPosStart;               
  
  // Size and variation (bigger also means more weight)
  float m_fSize, m_fSizeVar; 
  
  // Velocity
  Vec3 m_pVelocity;               
  float m_fVelocityVar;

  // Spawn time
  float m_fSpawnTime;                
  
  // Life time and variation
  float m_fLifeTime, m_fLifeTimeVar; 
  
  // Weight and variation
  float m_fWeight, m_fWeightVar; 
};

typedef std::vector<SScreenParticle*> SScreenParticleVec;
typedef SScreenParticleVec::iterator SScreenParticleItor;

class SScreenParticles
{
public:

  SScreenParticles(): m_bUseCamVelocity( false ), m_pVB( 0 ), m_pIB( 0 ), m_pSumVelocity(0.0f, 0.0f, 0.0f), m_pShader(0), m_pszTechnique(0)
  {
    m_pPrevView.SetIdentity();
    m_pViewProjPrev.SetIdentity();   
  }

  virtual ~SScreenParticles()
  {
    Release();
  }

  // Create particle system
  void Create(const SScreenParticle &pDefaultSetup, int nCount = 32, bool bUseCamVelocity = false);

  // Release used resources
  void Release();

  // Sets shader technique
  void SetShaderTechnique(CShader *pShader, const char *pszTechnique);
  
  // Update all particles
  void Update();

  // Render all particles 
  //    - client should set shader technique before usage
  //    - render should be done with single drawcall    
  //    - particles use render state defined in shader technique  
  
  void Render();    

  // Get particle list - use for custom rendering
  const SScreenParticleVec GetParticles() const
  {
    return m_pParticles;
  }

  // Get camera velocity vector
  static const Vec3 GetCamVelocity();

  // Get gravity vector
  static const Vec3 GetGravity();

  
  class ParticleSpawntimeComparator
  {
  public:
    bool operator()(SScreenParticle* pA, SScreenParticle *pB)
    {
      return ((pA->m_fSpawnTime) < (pB->m_fSpawnTime));
    }
  };

private:

  bool m_bUseCamVelocity;          
  SScreenParticle m_pDefaultSettings;

  // Camera force/gravity is shared among all particle systems
  static int m_nFrameID;

  // Keep velocity's separated - might be useful for some post process effect
  static Vec3 m_pCamVelocity;
  static Vec3 m_pGravity;

  // Velocity sum
  Vec3 m_pSumVelocity;

  static Matrix44 m_pPrevView;
  static Matrix44 m_pViewProjPrev;    

  // Particle list
  SScreenParticleVec m_pParticles;   

  // Rendering data

  // Vertex/Index buffers 
  void *m_pVB, *m_pIB;  
  
  const char *m_pszTechnique;    
  CShader *m_pShader;
      
  // Particle system vertex list    
  //PodArray<struct_VERTEX_FORMAT_P3F_TEX2F> m_pParticleVerts;

private:

  // Spawns a particle using default particle settings
  void SpawnParticle( SScreenParticle *&pParticle ) ;   

  // Custom particle updating
  //    - if custom update required derive screen particles and write custom update.. 
  //    - this will get called per particle in Update()
  //    - by default particle update will use camera velocity / gravity
  virtual void ParticleUpdate( SScreenParticle *&pParticle);

};

}

#endif
