/*=============================================================================
PostProcess.h : Post processing techniques base interface
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* 18/06/2005: Re-organized (to minimize code dependencies/less annoying compiling times)
* 23/02/2005: Re-factored/Converted to CryEngine 2.0
* Created by Tiago Sousa

=============================================================================*/

#ifndef _POSTPROCESS_H_
#define _POSTPROCESS_H_

class CShader;
class CTexture;

namespace NPostEffects
{
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  // Base effect parameter class, derive all new from this one
  class CEffectParam
  {
  public:
    CEffectParam(){ }

    virtual ~CEffectParam()
    {
      Release();
    }

    // Should implement where necessary. For example check CParamTexture
    virtual void Release() { }

    // Set parameters
    virtual void SetParam(float fParam, bool bForceValue=false) {}    
    virtual void SetParamVec4(const Vec4 &pParam, bool bForceValue=false) {}
    virtual void SetParamString(const char *pszParam) {}
		virtual void ResetParam(float fParam) { SetParam(fParam, true); }
		virtual void ResetParamVec4(const Vec4 &pParam) { SetParamVec4(pParam, true); }

    // Get parameters
    virtual float GetParam()  { return 1.0f; }
    virtual Vec4 GetParamVec4()  { return Vec4(1.0f, 1.0f, 1.0f, 1.0f); }
    virtual const char *GetParamString() const { return 0; }

    // Create effect parameter
    template <typename ParamT, typename T> static CEffectParam *Create(const T &pParam, bool bSmoothTransition = true)     
    {
      return new ParamT(pParam, bSmoothTransition);
    }
  };

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  // Bool type effect param
  class CParamBool: public CEffectParam
  {
  public:
    CParamBool(bool bParam, bool bSmoothTransition) 
    { 
      m_bParam=bParam;
    }

    virtual void SetParam(float fParam, bool bForceValue)
    {
      m_bParam=(fParam)?1:0;
    }

    virtual float GetParam() 
    {
      return static_cast<float>(m_bParam);
    }    

  private:
    bool m_bParam;
  };

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  // Int type effect param
  class CParamInt: public CEffectParam
  {
  public:
    CParamInt(int nParam, bool bSmoothTransition) 
    { 
      m_nParam=nParam;
    }

    virtual void SetParam(float fParam, bool bForceValue)
    {
      m_nParam=static_cast<int>(fParam);
    }

    virtual float GetParam()
    {
      return static_cast<float>(m_nParam);
    }

  private:
    int m_nParam;
  };

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  // Float type effect param
  class CParamFloat: public CEffectParam
  {
  public:
    CParamFloat(float fParam, bool bSmoothTransition)
    { 
      m_bSmoothTransition = bSmoothTransition;
      m_fParam = m_fParamDefault = m_fFrameParamAcc = fParam;
      m_nFrameSetCount = 0;
    }

    virtual void SetParam(float fParam, bool bForceValue);
    virtual float GetParam();

  private:
    float m_fParam;

    bool m_bSmoothTransition;
    float m_fParamDefault;
    float m_fFrameParamAcc;
    uint8 m_nFrameSetCount;
  };

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  // Vec4 type effect param
  class CParamVec4: public CEffectParam
  {
  public:
    CParamVec4(const Vec4 &pParam, bool bSmoothTransition) 
    { 
      m_bSmoothTransition = bSmoothTransition;
      m_pParam = m_pParamDefault = m_pFrameParamAcc = pParam;
      m_nFrameSetCount = 0;
    }

    virtual void SetParamVec4( const Vec4 &pParam, bool bForceValue );
    virtual Vec4 GetParamVec4();

  private:
    Vec4 m_pParam;
    
    bool m_bSmoothTransition;
    Vec4 m_pParamDefault;
    Vec4 m_pFrameParamAcc;
    uint8 m_nFrameSetCount;
  };

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  // CTexture type effect param
  class CParamTexture: public CEffectParam
  {
  public:
    CParamTexture(): m_pTexParam(0)
    { 
    }

    CParamTexture(int nInit, bool bSmoothTransition): m_pTexParam(0)
    { 
    }

    virtual ~CParamTexture()
    { 
      Release();
    }

    // Create texture
    int Create(const char *pszFileName);
    // Release resources
    virtual void Release();

    virtual void SetParamString(const char *pParam)
    {
      Create(pParam);
    }

    virtual const char *GetParamString() const
    {
      return m_pTexParam->GetName();
    }

    const CTexture *GetParamTexture() const
    {
      return m_pTexParam;
    }    

  private:
    CTexture *m_pTexParam;
  };

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  // Post processing render flags
  enum EPostProcessRenderFlag
  {  
    PSP_UPDATE_BACKBUFFER = ( 1 << 0 ),  // updates back-buffer texture for technique
  };


  // Post effects base structure interface. All techniques derive from this one.
  class CPostEffect
  {    
  public:

    // Create/Initialize post processing technique 
    virtual int  Create() { return 1; }
    // Free resources used
    virtual void Release() { }
    // Preprocess technique
    virtual bool Preprocess() { return IsActive(); }
    // Render technique
    virtual void Render()=0;
    // Reset technique state to default
    virtual void Reset()=0;

    // Get technique render flags
    int GetRenderFlags() const
    {
      return m_nRenderFlags;
    }

    // Get effect name
    virtual const char *GetName() const
    {
      return "PostEffectDefault";
    }

    // Is technique active ?
    bool IsActive() const
    {
      float fActive = m_pActive->GetParam();
      return (fActive)?1:0;
    }

  protected:      
    CPostEffect(): m_pActive(0), m_nRenderFlags( PSP_UPDATE_BACKBUFFER )
    { 
    }

    virtual ~CPostEffect()
    {          
      Release();
    }
    
    uint8 m_nRenderFlags;    
    CEffectParam *m_pActive;
  };

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

	typedef std::map<string, CEffectParam *> StringEffectMap;    
	typedef StringEffectMap::iterator StringEffectMapItor;

  typedef std::map<uint32, CEffectParam *> KeyEffectMap;    
  typedef KeyEffectMap::iterator KeyEffectMapItor;

  typedef std::vector< CPostEffect* > CPostEffectVec;
  typedef CPostEffectVec::iterator CPostEffectItor;

  class CPostEffectsMgr;
  static CPostEffectsMgr &PostEffectMgr();

  // Post process effects manager
  class CPostEffectsMgr
  {    
  public:

    // Create/Initialize post processing effects 
    int  Create();
    // Free resources used
    void Release();
    // Reset all post effects
    void Reset();
    // Start processing effects
    void Begin();
    // End processing effects
    void End();

    // Get techniques list
    CPostEffectVec &GetEffects()
    {
      return m_pEffects;
    }

    // Get name to id map
    KeyEffectMap &GetNameIdMap()
    {
      return m_pNameIdMap;
    }

    // Given a string returns corresponding SEffectParam if exists, else returns null
    CEffectParam *GetByName(const char *pszParam);

    // Given a string returns containing value if exists, else returns 0
    float GetByNameF(const char *pszParam);
    Vec4 GetByNameVec4(const char *pszParam);
    
    // Register effect into effect name map    
    void RegisterEffect( CPostEffect *pEffect );

    // Register a parameter
    template <typename paramT, typename T> 
    void RegisterParam(const char *pszName, CEffectParam *&pParam, const T &pParamVal, bool bSmoothTransition = true);

    // Current enabled post blending effects
    uint8 GetPostBlendEffectsFlags()
    {
      return m_nPostBlendEffectsFlags;
    }

    // Enabled/disable post blending effects
    void SetPostBlendEffectsFlags( uint8 nFlags )
    {
      m_nPostBlendEffectsFlags = nFlags;
    }

    friend CPostEffectsMgr &PostEffectMgr();

    static bool CheckPostProcessQuality( ERenderQuality nMinRQ, EShaderQuality nMinSQ )
    {
      if( gRenDev->m_RP.m_eQuality >= nMinRQ && gRenDev->EF_GetShaderQuality(eST_PostProcess) >= nMinSQ )
        return true;

      return false;
    }

  private:      
    CPostEffectsMgr(): m_nPostBlendEffectsFlags(0)
    {
    }

    virtual ~CPostEffectsMgr()
    {     
      Release();
    }

		// Pass a text string to this function and it will return the CRC
    uint32 GetCRC( const char *pszName );
		// Used only when creating the crc table
		uint32	CRC32Reflect(uint32 ref, char ch);

  protected:      

    bool m_bPostReset;
    uint8 m_nPostBlendEffectsFlags;

    // Shared parameters
    CEffectParam *m_pBrightness, *m_pContrast, *m_pSaturation, *m_pSharpening;
    CEffectParam *m_pColorC, *m_pColorY, *m_pColorM, *m_pColorK, *m_pColorHue;

    CEffectParam *m_pUserBrightness, *m_pUserContrast, *m_pUserSaturation, *m_pUserSharpening;
    CEffectParam *m_pUserColorC, *m_pUserColorY, *m_pUserColorM, *m_pUserColorK, *m_pUserColorHue;
    CEffectParam *m_pDirectionalBlurVec;

    CPostEffectVec m_pEffects;
    KeyEffectMap m_pNameIdMap;  
StringEffectMap m_pNameIdMapGen;

		uint32 m_nCRC32Table[256];  // Lookup table array 
    
    static CPostEffectsMgr m_pInstance;
  };

  static CPostEffectsMgr &PostEffectMgr()
  {    
    return CPostEffectsMgr::m_pInstance;
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Some nice utilities for handling post effects containers
  //////////////////////////////////////////////////////////////////////////////////////////////////

  #define AddEffect( ef ) PostEffectMgr().RegisterEffect( ( ef ) )  
  #define AddParamBool( szName, pParam, val ) PostEffectMgr().RegisterParam<CParamBool, bool>( (szName), (pParam), val)
  #define AddParamInt( szName, pParam, val ) PostEffectMgr().RegisterParam<CParamInt, int>( (szName), (pParam), val)
  #define AddParamFloat( szName, pParam, val ) PostEffectMgr().RegisterParam<CParamFloat, float>( (szName), (pParam), val)
  #define AddParamVec4( szName, pParam, val ) PostEffectMgr().RegisterParam<CParamVec4, Vec4>( (szName), (pParam), val)
  #define AddParamFloatNoTransition( szName, pParam, val ) PostEffectMgr().RegisterParam<CParamFloat, float>( (szName), (pParam), val, false)
  #define AddParamVec4NoTransition( szName, pParam, val ) PostEffectMgr().RegisterParam<CParamVec4, Vec4>( (szName), (pParam), val, false)
  #define AddParamTex( szName, pParam, val ) PostEffectMgr().RegisterParam<CParamTexture, int>( (szName), (pParam), val)
  
  struct container_object_safe_delete
  {
    template<typename T>
    void operator()( T* pObj ) const
    {
      SAFE_DELETE( pObj );
    }
  };

  struct container_object_safe_release
  {
    template<typename T>
    void operator()( T* pObj ) const
    {
      SAFE_RELEASE( pObj );
    }
  };

  struct SContainerKeyEffectParamDelete
  {
    void operator()( KeyEffectMap::value_type &pObj )
    {
      SAFE_DELETE( pObj.second );
    }
  };

  struct SContainerPostEffectCreate
  {    
    void operator() ( CPostEffect *pObj ) const
    {
      if( pObj)
      {
        pObj->Create();
      }
    }
  };

  struct SContainerPostEffectReset
  {    
    void operator()( CPostEffect *pObj ) const
    {
      if( pObj )
      {
        pObj->Reset();
      }      
    }
  };	
}

#endif
