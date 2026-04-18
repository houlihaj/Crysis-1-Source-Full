/*=============================================================================
D3DPostProcess : Direct3D specific post processing special effects
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

Todo:
* Cleanup code - big mess
* When we have a proper static branching support use it instead of shader switches inside code

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "I3DEngine.h"
#include "../Common/PostProcess/PostEffects.h"

using namespace NPostEffects;

struct SD3DPostEffectsUtils: public SPostEffectsUtils
{
private:

  LPDIRECT3DSURFACE9 GetTextureSurface(const CTexture *pTex, int iLevel=0)
  {  
#if defined(PS3)
    assert("D3D9 call during PS3/D3D10 rendering");
    return 0;
#else
    LPDIRECT3DTEXTURE9 pTexture = (LPDIRECT3DTEXTURE9)pTex->GetDeviceTexture();
    assert(pTexture);

    LPDIRECT3DSURFACE9 pSurf = 0;
    HRESULT hr = pTexture->GetSurfaceLevel(iLevel, &pSurf);

    return pSurf;
#endif
  }

public:

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  SD3DSurface *GetDepthSurface( CTexture *pTex )
  {
    if( pTex->GetFlags() & FT_USAGE_FSAA && gRenDev->m_RP.m_FSAAData.Type)
      return &gcpRendD3D->m_DepthBufferOrigFSAA;

    return &gcpRendD3D->m_DepthBufferOrig;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  virtual void ResolveRT( CTexture *&pDst, bool bStretch=0 )
  {    
    int iTempX, iTempY, iWidth, iHeight;
    gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

#if defined (DIRECT3D9) || defined (OPENGL)
    LPDIRECT3DSURFACE9 pBackSurface= gcpRendD3D->GetBackSurface();
    LPDIRECT3DSURFACE9 pTexSurf= GetTextureSurface(pDst);

#ifdef XENON
    D3DRECT pDstRect={ 0, 0, pDst->GetWidth(), pDst->GetHeight()};

    LPDIRECT3DTEXTURE9 pTex =(bStretch)?(LPDIRECT3DTEXTURE9) CTexture::m_Text_SceneTarget->GetDeviceTexture():(LPDIRECT3DTEXTURE9) pDst->GetDeviceTexture();
    HRESULT hr=0;
    hr = gcpRendD3D->m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, &pDstRect, pTex, NULL, 0, 0, NULL, 1.0f, 0L, NULL);              

    if( bStretch )
    {
      StretchRect(CTexture::m_Text_SceneTarget, pDst);
    }

#else

    RECT pSrcRect={0, 0, iWidth, iHeight };
    RECT pDstRect={0, 0, pDst->GetWidth(), pDst->GetHeight() };

    gcpRendD3D->m_RP.m_PS.m_RTCopied++; 
    gcpRendD3D->m_RP.m_PS.m_RTCopiedSize += pDst->GetDeviceDataSize();
    gcpRendD3D->m_pd3dDevice->StretchRect(pBackSurface, &pSrcRect, pTexSurf, &pDstRect, D3DTEXF_NONE); 

#endif

    SAFE_RELEASE(pTexSurf) 

#elif defined (DIRECT3D10)

    ID3D10Resource *pDstResource = (ID3D10Resource *)pDst->GetDeviceTexture();        

    ID3D10RenderTargetView* pOrigRT = gcpRendD3D->m_pNewTarget[0]->m_pTarget;
    if (pOrigRT && pDstResource)
    {
      D3D10_BOX box;
      ZeroStruct(box);
      box.right = iWidth;
      box.bottom = iHeight;
      box.back = 1;

      ID3D10Resource *pSrcResource;
      pOrigRT->GetResource( &pSrcResource );

      HRESULT hr = 0;
      gcpRendD3D->m_RP.m_PS.m_RTCopied++; 
      gcpRendD3D->m_RP.m_PS.m_RTCopiedSize += pDst->GetDeviceDataSize();
      gcpRendD3D->m_pd3dDevice->CopySubresourceRegion(pDstResource, 0, 0, 0, 0, pSrcResource, 0, &box);      

      SAFE_RELEASE(pSrcResource);
    }

#endif    
  }

  virtual void CopyScreenToTexture(CTexture *&pDst, bool bStretch=0)
  {
    int iTempX, iTempY, iWidth, iHeight;
    gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

    ResolveRT( pDst, bStretch );
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  virtual void StretchRect(CTexture *pSrc, CTexture *&pDst) 
  {
    if(!pSrc || !pDst) 
    {
      return;
    }

    PROFILE_SHADER_START

      // Get current viewport
      int iTempX, iTempY, iWidth, iHeight;
    gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
    bool bResample=0;

    if(pSrc->GetWidth()!=pDst->GetWidth() && pSrc->GetHeight()!=pDst->GetHeight())
    {
      bResample = 1;
    }

    SD3DSurface *pCurrDepthBuffer = GetDepthSurface( pDst );

    gcpRendD3D->FX_PushRenderTarget(0, pDst, pCurrDepthBuffer); 
    gRenDev->SetViewport(0, 0, pDst->GetWidth(), pDst->GetHeight());        

    if(!bResample)
    {
      static CCryName pTechName("TextureToTexture");                 
      ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
    }
    else
    { 
      static CCryName pTechName("TextureToTextureResampled");     
      ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
    }

    gRenDev->EF_SetState(GS_NODEPTHTEST);   

    // Get sample size ratio (based on empirical "best look" approach)
    float fSampleSize = ((float)pSrc->GetWidth()/(float)pDst->GetWidth()) * 0.5f;

    // Set samples position
    float s1 = fSampleSize / (float) pSrc->GetWidth();  // 2.0 better results on lower res images resizing        
    float t1 = fSampleSize / (float) pSrc->GetHeight();       

    // Use box filtering
    //Vec4 pParams0=Vec4(-s1, -t1, s1, -t1); 
    //Vec4 pParams1=Vec4(s1, t1, -s1, t1);    

    // Use rotated grid
    Vec4 pParams0=Vec4(s1*0.95f, t1*0.25f, -s1*0.25f, t1*0.96f); 
    Vec4 pParams1=Vec4(-s1*0.96f, -t1*0.25f, s1*0.25f, -t1*0.96f);  

    static CCryName pParam0Name("texToTexParams0");
    static CCryName pParam1Name("texToTexParams1");

    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam0Name, &pParams0, 1);        
    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam1Name, &pParams1, 1); 


    CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
    SetTexture(pSrc, 0, bResample?FILTER_LINEAR: FILTER_POINT);    

    DrawFullScreenQuad(pDst->GetWidth(), pDst->GetHeight());

    ShEndPass();

    // Restore previous viewport
    gcpRendD3D->FX_PopRenderTarget(0);
    gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);        

    gcpRendD3D->FX_Flush();
    PROFILE_SHADER_END    
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  virtual void TexBlurGaussian(CTexture *pTex, int nAmount, float fScale, float fDistribution, bool bAlphaOnly, CTexture *pMask = 0 )
  {
    if(!pTex)
    {
      return;
    }

    SDynTexture *tpBlurTemp = new SDynTexture(pTex->GetWidth(), pTex->GetHeight(), pTex->GetDstFormat(), eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP | FT_USAGE_RENDERTARGET, "TempBlurRT");
    tpBlurTemp->Update( pTex->GetWidth(), pTex->GetHeight() );

    if( !tpBlurTemp->m_pTexture)
    {
      SAFE_DELETE(tpBlurTemp);
      return;
    }

    PROFILE_SHADER_START

      // Get current viewport
      int iTempX, iTempY, iWidth, iHeight;
    gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
    gRenDev->SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());        

    Vec4 vWhite( 1.0f, 1.0f, 1.0f, 1.0f );

    // TODO: Make test with Martin's idea about the horizontal blur pass with vertical offset.

    // only regular gaussian blur supporting masking
    static CCryName pTechName("GaussBlurBilinear");
    static CCryName pTechNameMasked("MaskedGaussBlurBilinear");
    static CCryName pTechName1("GaussAlphaBlur");

    //ShBeginPass(CShaderMan::m_shPostEffects, , FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
    uint nPasses;
    CShaderMan::m_shPostEffects->FXSetTechnique((!bAlphaOnly)? ((pMask)?pTechNameMasked : pTechName) :pTechName1);
    CShaderMan::m_shPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    gRenDev->EF_SetState(GS_NODEPTHTEST);   

    // setup texture offsets, for texture sampling
    float s1 = 1.0f/(float) pTex->GetWidth();     
    float t1 = 1.0f/(float) pTex->GetHeight();    

    // Horizontal/Vertical pass params
    const int nSamples = 16;
    int nHalfSamples = (nSamples>>1);

    Vec4 pHParams[32], pVParams[32], pWeightsPS[32];
    float pWeights[32], fWeightSum = 0;

    memset( pWeights,0,sizeof(*pWeights) );

    int s;
    for(s = 0; s<nSamples; ++s)
    {
      if(fDistribution != 0.0f)
        pWeights[s] = GaussianDistribution1D(s - nHalfSamples, fDistribution);      
      else
        pWeights[s] = 0.0f;
      fWeightSum += pWeights[s];
    }

    // normalize weights
    for(s = 0; s < nSamples; ++s)
    {
      pWeights[s] /= fWeightSum;  
    }

    // set bilinear offsets
    for(s = 0; s < nHalfSamples; ++s)
    {
      float off_a = pWeights[s*2];
      float off_b = ( (s*2+1) <= nSamples-1 )? pWeights[s*2+1] : 0;   
      float a_plus_b = (off_a + off_b);
      if (a_plus_b == 0)
        a_plus_b = 1.0f;
      float offset = off_b / a_plus_b;

      pWeights[s] = off_a + off_b;
      pWeights[s] *= fScale ;
      pWeightsPS[s] = vWhite * pWeights[s];

      float fCurrOffset = (float) s*2 + offset - nHalfSamples;
      pHParams[s] = Vec4(s1 * fCurrOffset , 0, 0, 0);  
      pVParams[s] = Vec4(0, t1 * fCurrOffset , 0, 0);       
    }


    STexState sTexState = STexState(FILTER_LINEAR, true);
    static CCryName pParam0Name("psWeights");
    static CCryName pParam1Name("PI_psOffsets");

    //SetTexture(pTex, 0, FILTER_LINEAR); 
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam0Name, pWeightsPS, nHalfSamples);  

    //for(int p(1); p<= nAmount; ++p)   
    {
      //Horizontal


      gcpRendD3D->FX_PushRenderTarget(0, tpBlurTemp->m_pTexture, &gcpRendD3D->m_DepthBufferOrig);
      gRenDev->SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());        

      // !force updating constants per-pass! (dx10..)
      CShaderMan::m_shPostEffects->FXBeginPass(0);

      CTexture::BindNULLFrom(0);// workaround for dx10 skipping setting correct samplers
      pTex->Apply(0, CTexture::GetTexState(sTexState)); 
      if( pMask )
        pMask->Apply(1, CTexture::GetTexState(sTexState));
      CShaderMan::m_shPostEffects->FXSetVSFloat(pParam1Name, pHParams, nHalfSamples);                
      DrawFullScreenQuad(pTex->GetWidth(), pTex->GetHeight());

      CShaderMan::m_shPostEffects->FXEndPass();

      gcpRendD3D->FX_PopRenderTarget(0);

      //Vertical
      gcpRendD3D->FX_PushRenderTarget(0, pTex, GetDepthSurface( pTex )) ;//&gcpRendD3D->m_DepthBufferOrig);

      gRenDev->SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());         

      // !force updating constants per-pass! (dx10..)
      CShaderMan::m_shPostEffects->FXBeginPass(0);

      CShaderMan::m_shPostEffects->FXSetVSFloat(pParam1Name, pVParams, nHalfSamples);
      CTexture::BindNULLFrom(0);// workaround for dx10 skipping setting correct samplers
      tpBlurTemp->m_pTexture->Apply(0, CTexture::GetTexState(sTexState)); 
      if( pMask )
        pMask->Apply(1, CTexture::GetTexState(sTexState));
      DrawFullScreenQuad(pTex->GetWidth(), pTex->GetHeight());      

      CShaderMan::m_shPostEffects->FXEndPass();

      gcpRendD3D->FX_PopRenderTarget(0);
    }             

    CShaderMan::m_shPostEffects->FXEnd(); 

    //    ShEndPass( );

    // Restore previous viewport
    gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);

    //release dyntexture
    SAFE_DELETE(tpBlurTemp);

    gcpRendD3D->FX_Flush();
    PROFILE_SHADER_END      
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  static SD3DPostEffectsUtils &GetInstance()
  {
    return m_pInstance;
  }

public:

private:
  static SD3DPostEffectsUtils m_pInstance;

  SD3DPostEffectsUtils() { }
  virtual ~SD3DPostEffectsUtils() 
  { 
    Release();
  }
};

SD3DPostEffectsUtils SD3DPostEffectsUtils::m_pInstance;

#define GetUtils() SD3DPostEffectsUtils::GetInstance()

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CD3D9Renderer::FX_PostProcessScene(bool bEnable)
{  
  if( !gcpRendD3D )
  {
    return false;
  }

  if(bEnable)
  {    
    //if(!CTexture::m_Text_BackBuffer)
    {      
      // Create all resources if required
      GetUtils().Create();
    }

    if(CTexture::m_Text_BackBuffer)
    {
      SShaderItem shItem( CShaderMan::m_shPostEffects );
      CRenderObject *pObj = EF_GetObject(true);
      if (pObj)
      {
        pObj->m_II.m_Matrix.SetIdentity();
        EF_AddEf(m_pREPostProcess, shItem, pObj, EFSLIST_POSTPROCESS, 0);
      }
    }    
  }
  else
    if(!CRenderer::CV_r_postprocess_effects && !CRenderer::CV_r_hdrrendering && CTexture::m_Text_BackBuffer)
    {
      GetUtils().Release();
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CD3D9Renderer::FX_GlowScene(bool bEnable)
{
  CTexture *pSceneTarget = (CRenderer::CV_r_debug_extra_scenetarget_fsaa && gcpRendD3D->m_RP.m_FSAAData.Type)?CTexture::m_Text_SceneTargetFSAA : CTexture::m_Text_SceneTarget;    
  SD3DSurface *pCurrDepthBuffer = GetUtils().GetDepthSurface( pSceneTarget );

  if(bEnable)
  {
    CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers

    GetUtils().Log(" +++ Begin Glow scene +++ \n"); 

    FX_PushRenderTarget(0, pSceneTarget, pCurrDepthBuffer);

    SetViewport(0, 0, GetWidth(), GetHeight());                        
    ColorF clearColor(0, 0, 0, 0);
    EF_ClearBuffers(FRT_CLEAR_COLOR|FRT_CLEAR_IMMEDIATE, &clearColor);
  }
  else  
  { 
    FX_PopRenderTarget(0);    

    EF_ResetPipe();     

    GetUtils().Log(" +++ End Glow scene +++ \n"); 

    gcpRendD3D->SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());

    if( !(m_RP.m_PersFlags2&RBPF2_GLOWPASS) )
    {             
      // No glow if nothing rendered
      return false;
    }

    m_RP.m_PersFlags2 &= ~RBPF2_GLOWPASS;
    gcpRendD3D->Set2DMode(true, 1, 1);           
    GetUtils().m_pCurDepthSurface = &gcpRendD3D->m_DepthBufferOrig;

    CTexture *pSrc = pSceneTarget;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Copy glow from framebuffer into glow texture

    gcpRendD3D->FX_PushRenderTarget(0, CTexture::m_Text_Glow, &gcpRendD3D->m_DepthBufferOrig);
    gcpRendD3D->SetViewport(0, 0, CTexture::m_Text_Glow->GetWidth(), CTexture::m_Text_Glow->GetHeight());

    static CCryName pTech0Name("TextureToTextureResampled");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTech0Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    int nRenderState( GS_NODEPTHTEST );      
    if( ( m_RP.m_PersFlags2 & RBPF2_LIGHTSHAFTS ) && !CRenderer::CV_r_hdrrendering)
    {
      nRenderState |= GS_BLSRC_ONE | GS_BLDST_ONE;
    }

    gRenDev->EF_SetState(nRenderState, 0); 

    // Get sample size ratio (based on empirical "best look" approach)
    float fSampleSize = ((float)pSrc->GetWidth()/(float)CTexture::m_Text_Glow->GetWidth()) * 0.5f;

    // Set samples position
    float s1 = fSampleSize / (float) pSrc->GetWidth();  // 2.0 better results on lower res images resizing        
    float t1 = fSampleSize / (float) pSrc->GetHeight();       

    // Use rotated grid
    Vec4 pParams0=Vec4(s1*0.95f, t1*0.25f, -s1*0.25f, t1*0.96f); 
    Vec4 pParams1=Vec4(-s1*0.96f, -t1*0.25f, s1*0.25f, -t1*0.96f);  

    static CCryName pParam0Name("texToTexParams0");
    static CCryName pParam1Name("texToTexParams1");
    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam0Name, &pParams0, 1);        
    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam1Name, &pParams1, 1); 
    CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
    GetUtils().SetTexture(pSrc, 0, FILTER_LINEAR);   
    GetUtils().DrawFullScreenQuad(CTexture::m_Text_Glow->GetWidth(), CTexture::m_Text_Glow->GetHeight());

    GetUtils().ShEndPass();

    gcpRendD3D->FX_PopRenderTarget(0);    
    SetViewport(0, 0, GetWidth(), GetHeight());

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Add glow/decode into frame-buffer

    //pSrc = CTexture::m_Text_SceneTarget;    

    static CCryName pTech1Name("GlowScene");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTech1Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    gRenDev->EF_SetState(GS_BLSRC_ONE | GS_BLDST_ONE |  GS_NODEPTHTEST | GS_ALPHATEST_GREATER, 0); 

    CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
    GetUtils().SetTexture(pSrc, 0, FILTER_LINEAR);    
    GetUtils().DrawFullScreenQuad(pSrc->GetWidth(), pSrc->GetHeight());

    GetUtils().ShEndPass();

    gcpRendD3D->Set2DMode(false, 1, 1);     
    gcpRendD3D->SetViewport(GetUtils().m_pScreenRect.left, GetUtils().m_pScreenRect.top, GetUtils().m_pScreenRect.right, GetUtils().m_pScreenRect.bottom); 

    // Enable glow postprocessing
    CEffectParam *pParam = PostEffectMgr().GetByName("Glow_Active"); 
    assert(pParam && "Parameter doesn't exist");
    pParam->SetParam(1.0f);   
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CD3D9Renderer::FX_CustomRenderScene( bool bEnable )
{
  if(bEnable)
  {
    GetUtils().Log(" +++ Begin custom render scene +++ \n"); 
    FX_PushRenderTarget(0, CTexture::m_Text_BackBuffer, &gcpRendD3D->m_DepthBufferOrig);
    SetViewport(0, 0, GetWidth(), GetHeight());                        
    ColorF clearColor(0, 0, 0, 0);
    EF_ClearBuffers(FRT_CLEAR_COLOR|FRT_CLEAR_DEPTH|FRT_CLEAR_IMMEDIATE, &clearColor); // //FRT_CLEAR_DEPTH|
    m_RP.m_PersFlags2 |= RBPF2_CUSTOM_RENDER_PASS;
  }
  else  
  { 
    FX_PopRenderTarget(0);    
    EF_ResetPipe();     

    GetUtils().Log(" +++ End custom render scene +++ \n"); 

    gcpRendD3D->SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());

    m_RP.m_PersFlags2 &= ~RBPF2_CUSTOM_RENDER_PASS;
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CCryVision::Render()
{
  //todo: use lower res rt for low spec
  bool bLowSpec = gRenDev->m_RP.m_eQuality == eRQ_Low;
  float fType = m_pType->GetParam();

  gcpRendD3D->Set2DMode(false, 1, 1);

  // render to texture all masks
  gcpRendD3D->FX_ProcessPostRenderLists(FB_CUSTOM_RENDER);

  gcpRendD3D->Set2DMode(true, 1, 1);           
  gcpRendD3D->SetViewport(GetUtils().m_pScreenRect.left, GetUtils().m_pScreenRect.top, GetUtils().m_pScreenRect.right, GetUtils().m_pScreenRect.bottom); 
  GetUtils().m_pCurDepthSurface = &gcpRendD3D->m_DepthBufferOrig;

  CTexture *pScreen = CTexture::m_Text_BackBuffer;
  CTexture *pMask = CTexture::m_Text_Black;
  CTexture *pMaskBlurred = CTexture::m_Text_Black;


  // skip processing, nothing was added to mask
  if( ( SRendItem::BatchFlags(EFSLIST_GENERAL) | SRendItem::BatchFlags(EFSLIST_TRANSP) ) & FB_CUSTOM_RENDER)
  {
    // hi specs only, else render directly to frame buffer
    {
      // store silhouetes/signature temporary render target, so that we can post process this after wards
      if( !bLowSpec )
      {
        gcpRendD3D->FX_PushRenderTarget(0, CTexture::m_Text_SceneTarget, GetUtils().GetDepthSurface( CTexture::m_Text_SceneTarget ) );
        gcpRendD3D->SetViewport(0, 0, CTexture::m_Text_SceneTarget->GetWidth(), CTexture::m_Text_SceneTarget->GetHeight());
      }

      static CCryName pTech1Name("BinocularView");
      GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTech1Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

      gRenDev->EF_SetState(GS_NODEPTHTEST |((!bLowSpec)? 0 : GS_BLSRC_ONE | GS_BLDST_ONE), 0); 

      // Set PS default params
      Vec4 pParams= Vec4(0, 0, 0, (!fType)? 1.0f : 0.0f);
      CCryName pParamName("psParams");
      CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1); 

      GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);   
      GetUtils().SetTexture(CTexture::m_Text_ZTarget, 1, FILTER_POINT);   
      GetUtils().DrawFullScreenQuad(CTexture::m_Text_SceneTarget->GetWidth(), CTexture::m_Text_SceneTarget->GetHeight());

      GetUtils().ShEndPass();
      if( !bLowSpec )
      {
        gcpRendD3D->FX_PopRenderTarget(0);    

        pMask = CTexture::m_Text_SceneTarget;
        pMaskBlurred = CTexture::m_Text_BackBufferScaled[0];
      }
    }


    // hi specs only add mask into glow render target
    if( !bLowSpec )
    {
      gcpRendD3D->FX_PushRenderTarget(0, CTexture::m_Text_BackBufferScaled[0], &gcpRendD3D->m_DepthBufferOrig);
      gcpRendD3D->SetViewport(0, 0, CTexture::m_Text_BackBufferScaled[0]->GetWidth(), CTexture::m_Text_BackBufferScaled[0]->GetHeight());

      static CCryName pTech0Name("BinocularViewGlow");
      GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTech0Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
      gRenDev->EF_SetState(GS_NODEPTHTEST, 0); 

      GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_LINEAR);    
      GetUtils().SetTexture(CTexture::m_Text_SceneTarget, 1, FILTER_LINEAR);   
      GetUtils().DrawFullScreenQuad(CTexture::m_Text_Glow->GetWidth(), CTexture::m_Text_Glow->GetHeight());

      GetUtils().ShEndPass();

      gcpRendD3D->FX_PopRenderTarget(0);    

      gcpRendD3D->SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());

      // blur - for glow
      GetUtils().TexBlurGaussian(CTexture::m_Text_BackBufferScaled[0], 1, 1.25f, 1.5f, false);                  
    }
  }

  if( !bLowSpec )
  {
    float fBlendParam = clamp_tpl<float>( m_pAmount->GetParam(), 0.0f, 1.0f) ;

    // update backbuffer
    GetUtils().CopyScreenToTexture(CTexture::m_Text_BackBuffer); 

    static CCryName pTech2Name("BinocularViewFinal");
    static CCryName pTech3Name("BinocularViewFinalNoTinting");

    GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, (!fType)? pTech2Name : pTech3Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    gRenDev->EF_SetState(GS_NODEPTHTEST, 0); 

    GetUtils().SetTexture(pScreen, 0, FILTER_POINT);   
    GetUtils().SetTexture(pMask, 1, FILTER_POINT);   
    GetUtils().SetTexture(pMaskBlurred, 2);   

    // Set PS default params
    Vec4 pParams= Vec4(0, 0, 0, fBlendParam);
    static CCryName pParamName("psParams");
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

    GetUtils().ShEndPass();

    gcpRendD3D->SetViewport(GetUtils().m_pScreenRect.left, GetUtils().m_pScreenRect.top, GetUtils().m_pScreenRect.right, GetUtils().m_pScreenRect.bottom); 
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////


bool CD3D9Renderer::FX_MotionBlurScene(bool bEnable)
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_VeryHigh, eSQ_High );
  if( !bQualityCheck || !CRenderer::CV_r_hdrrendering )
    return false;

  static uint64 nPrevFlagsShader_RT = 0;
  if( CRenderer::s_MotionBlurMode < 3 || (iSystem->IsEditorMode() && CRenderer::s_MotionBlurMode !=5 ))
    return false;

  CTexture *pSceneTarget = (CRenderer::CV_r_debug_extra_scenetarget_fsaa && gcpRendD3D->m_RP.m_FSAAData.Type)?CTexture::m_Text_SceneTargetFSAA : CTexture::m_Text_SceneTarget;    
  SD3DSurface *pCurrDepthBuffer = GetUtils().GetDepthSurface( pSceneTarget );

  if(bEnable)
  {
    // get current fp framebuffer
    //FX_ScreenStretchRect(CTexture::m_Text_SceneTarget);

    nPrevFlagsShader_RT = gcpRendD3D->m_RP.m_FlagsShader_RT;

    GetUtils().Log(" +++ Begin object motion blur scene +++ \n"); 

    FX_PushRenderTarget(0, pSceneTarget, pCurrDepthBuffer);

    // Re-use scene target rendertarget for velocity buffer

    SetViewport(0, 0, CTexture::m_Text_SceneTarget->GetWidth(), CTexture::m_Text_SceneTarget->GetHeight());                        
    ColorF clearColor(0, 0, 0, 0);
    EF_ClearBuffers(FRT_CLEAR_COLOR|FRT_CLEAR_IMMEDIATE, &clearColor);

    //else
    //FX_ScreenStretchRect(CTexture::m_Text_SceneTarget);

    m_RP.m_PersFlags2 |= RBPF2_MOTIONBLURPASS;
  }
  else  
  {     
    //    if( gRenDev->m_RP.m_eQuality >= eRQ_VeryHigh )
    {
      FX_PopRenderTarget(0);    
      EF_ResetPipe();     
      gcpRendD3D->SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());
 
      // compute motion blur
      gcpRendD3D->Set2DMode(true, 1, 1);            

      GetUtils().m_pCurDepthSurface = &gcpRendD3D->m_DepthBufferOrig;

      uint64 nFlagsShaderRT = gRenDev->m_RP.m_FlagsShader_RT;
      gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0]|g_HWSR_MaskBit[HWSR_SAMPLE1]|g_HWSR_MaskBit[HWSR_SAMPLE2]);

      if( CRenderer::s_MotionBlurMode == 3 || CRenderer::s_MotionBlurMode == 5)
      {
        CTexture *pOffsetMap = CTexture::m_Text_BackBufferScaled[1];
        CTexture *pDst = CTexture::m_Text_HDRBrightPass[0];
        CTexture *pDstTemp = CTexture::m_Text_HDRTargetScaled[0]; 

        //GetUtils().StretchRect(pSceneTarget, pDstTemp);

        // Velocity buffer rescaling
        {
          static CCryName pTechName0("OMB_VelocityIDRescale");

          // velocity dilation: iteration 1
          gcpRendD3D->FX_PushRenderTarget(0,  pDst,  GetUtils().GetDepthSurface( pDst )) ;
          gRenDev->SetViewport(0, 0, pDst->GetWidth(), pDst->GetHeight());        

          GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName0, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);
          gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

          CCryName pParamName("PI_motionBlurParams");
          Vec4 vParams=Vec4(0,0, 1.0f / pSceneTarget->GetWidth(),  1.0f / pSceneTarget->GetHeight()) ;       
          CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &vParams, 1);

          CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
          GetUtils().SetTexture(pSceneTarget, 0, FILTER_POINT);  

          GetUtils().DrawFullScreenQuad(pDst->GetWidth(), pDst->GetHeight());      
          GetUtils().ShEndPass();

          gcpRendD3D->FX_PopRenderTarget(0);
        }

        // Generate offset map
        {
          static CCryName pTechName0("OMB_OffsetMap");

          // velocity dilation: iteration 1
          gcpRendD3D->FX_PushRenderTarget(0,  pOffsetMap,  GetUtils().GetDepthSurface( pOffsetMap )) ;
          gRenDev->SetViewport(0, 0, pOffsetMap->GetWidth(), pOffsetMap->GetHeight());        

          GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName0, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);
          gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

          CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
          GetUtils().SetTexture(pDst, 0, FILTER_POINT);  

          GetUtils().DrawFullScreenQuad(pOffsetMap->GetWidth(), pOffsetMap->GetHeight());      
          GetUtils().ShEndPass();

          gcpRendD3D->FX_PopRenderTarget(0);

          GetUtils().TexBlurGaussian(pOffsetMap, 2, 1.0f, 5.0f, false);                  
          GetUtils().TexBlurGaussian(pOffsetMap, 2, 1.0f, 5.0f, false);                  
          //GetUtils().TexBlurGaussian(pOffsetMap, 2, 1.0f, 5.0f, false);                  
          //GetUtils().TexBlurGaussian(pOffsetMap, 2, 1.0f, 5.0f, false); 
        }

        // Velocity buffer dilation
        {
          static CCryName pTechName0("OMB_VelocityDilation");
          CCryName pTechName("OMB_VelocityDilation");

          static CCryName pParamName("PI_motionBlurParams");
          Vec4 vParams=Vec4(1.0f/pSceneTarget->GetWidth(), 1.0f/pSceneTarget->GetHeight(), 1.0f / pDst->GetWidth(),  1.0f / pDst->GetHeight()) ;       

          const int nIterationCount = 2;
          for(int n= 0; n < nIterationCount; ++n)
          {
            // enable horizontal pass
            gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];

            // velocity dilation: iteration 1n
            gcpRendD3D->FX_PushRenderTarget(0,  pDstTemp,  GetUtils().GetDepthSurface( pDst )) ;
            gRenDev->SetViewport(0, 0, pDstTemp->GetWidth(), pDstTemp->GetHeight());        

            GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName0, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);
            gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

            //fCurrAtten = (float)(nCurrPass++) /(float) nMaxPasses;
            //vParams.x = fCurrAtten;
            CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &vParams, 1);
            
            CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
            GetUtils().SetTexture(pDst, 0, FILTER_POINT);      
            GetUtils().SetTexture(CTexture::m_Text_ZTarget, 1, FILTER_POINT);
            GetUtils().SetTexture(pOffsetMap, 2, FILTER_LINEAR);  
            GetUtils().SetTexture(pSceneTarget, 3, FILTER_POINT);  
            

            GetUtils().DrawFullScreenQuad(pDstTemp->GetWidth(), pDstTemp->GetHeight());      
            GetUtils().ShEndPass();

            gcpRendD3D->FX_PopRenderTarget(0);

            // enable vertical pass
            gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE1];

            // velocity dilation: iteration 2n
            gcpRendD3D->FX_PushRenderTarget(0,  pDst,  GetUtils().GetDepthSurface( pDst )) ;
            gRenDev->SetViewport(0, 0, pDst->GetWidth(), pDst->GetHeight());        

            GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName0, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);
            gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

            //fCurrAtten = (float)(nCurrPass++) /(float) nMaxPasses;
            //vParams.x = fCurrAtten;

            CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &vParams, 1);
            CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
            GetUtils().SetTexture(pDstTemp, 0, FILTER_POINT);      
            GetUtils().SetTexture(CTexture::m_Text_ZTarget, 1, FILTER_POINT);
            GetUtils().SetTexture(pOffsetMap, 2, FILTER_POINT);  
            GetUtils().SetTexture(pSceneTarget, 3, FILTER_POINT);  

            GetUtils().DrawFullScreenQuad(pDst->GetWidth(), pDst->GetHeight());      
            GetUtils().ShEndPass();

            gcpRendD3D->FX_PopRenderTarget(0);
          }

           gRenDev->SetViewport(0, 0, CTexture::m_Text_SceneTarget->GetWidth(), CTexture::m_Text_SceneTarget->GetHeight());        
        }

        // Put objects ID into alpha channel
        {
          CCryName pTechName0("OMB_CopyAlphaID");

          // velocity dilation: iteration 1
          gcpRendD3D->FX_PushRenderTarget(0,  CTexture::m_Text_HDRTarget,  GetUtils().GetDepthSurface( CTexture::m_Text_HDRTarget )) ;
          gRenDev->SetViewport(0, 0, CTexture::m_Text_HDRTarget->GetWidth(), CTexture::m_Text_HDRTarget->GetHeight());        

          GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName0, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);
          
          // only write to alpha
          gcpRendD3D->EF_SetState(GS_NODEPTHTEST|GS_COLMASK_A);     

          CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
          //GetUtils().SetTexture(pDst, 0, FILTER_LINEAR);  
          GetUtils().SetTexture(pSceneTarget, 0, FILTER_POINT);  
 
          GetUtils().DrawFullScreenQuad(CTexture::m_Text_HDRTarget->GetWidth(), CTexture::m_Text_HDRTarget->GetHeight());      
          GetUtils().ShEndPass();

          gcpRendD3D->FX_PopRenderTarget(0);

          gRenDev->SetViewport(0, 0, CTexture::m_Text_SceneTarget->GetWidth(), CTexture::m_Text_SceneTarget->GetHeight());        
        }
        
        //GetUtils().StretchRect(CTexture::m_Text_HDRTarget, CTexture::m_Text_SceneTarget);
        
        gcpRendD3D->FX_PushRenderTarget(0,  CTexture::m_Text_SceneTarget,  GetUtils().GetDepthSurface( CTexture::m_Text_SceneTarget )) ; //&gcpRendD3D->m_DepthBufferOrig); 
        gRenDev->SetViewport(0, 0, CTexture::m_Text_SceneTarget->GetWidth(), CTexture::m_Text_SceneTarget->GetHeight());        

        // Render motion blurred scene
        static CCryName pTechName0("OMB_UsingVelocityDilation");

        gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE1];

        GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName0, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);
        gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

        static CCryName pParamName("motionBlurParams");
        CCryName pParamName2("PI_motionBlurParams");

        CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
        GetUtils().SetTexture(CTexture::m_Text_HDRTarget, 0, FILTER_LINEAR);       
        GetUtils().SetTexture(pDstTemp, 1, FILTER_LINEAR); 
        GetUtils().SetTexture(pOffsetMap, 2, FILTER_LINEAR); 

        Vec4 vParams=Vec4(1, 1, 1,  1) ;       
        vParams.x =0.5;  
        CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName2, &vParams, 1);

        GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());      
        GetUtils().ShEndPass();

        // Restore previous viewport/rt
        gcpRendD3D->FX_PopRenderTarget(0); 
        gRenDev->SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());        

        gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];

        GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName0, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);
        gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

        CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
        GetUtils().SetTexture(CTexture::m_Text_SceneTarget, 0, FILTER_LINEAR);       
        GetUtils().SetTexture(pDstTemp, 1, FILTER_LINEAR); 
        GetUtils().SetTexture(pOffsetMap, 2, FILTER_LINEAR);  
        

        //vParams.x = 0.25;  
        vParams.x = 1.0; 
        CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName2, &vParams, 1);

        GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());      
        GetUtils().ShEndPass();
      }
      else
      {
        if( CRenderer::CV_r_motionblurdynamicquality )
        {
          // generate mask
          gcpRendD3D->FX_PushRenderTarget(0,  CTexture::m_Text_BackBuffer,  GetUtils().GetDepthSurface( CTexture::m_Text_BackBuffer )) ; //&gcpRendD3D->m_DepthBufferOrig); 
          gRenDev->SetViewport(0, 0, CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());        

          static CCryName pTechName0("MotionBlurObjectMask");

          GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName0, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);
          gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

          CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
          GetUtils().SetTexture(pSceneTarget, 0, FILTER_POINT);      
          GetUtils().SetTexture(CTexture::m_Text_ZTarget, 1, FILTER_POINT);

          GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());      
          GetUtils().ShEndPass();

          gcpRendD3D->FX_PopRenderTarget(0);
          gRenDev->SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());        
        }

        GetUtils().StretchRect(pSceneTarget, CTexture::m_Text_HDRBrightPass[0]);
        if( CRenderer::CV_r_motionblurdynamicquality )
        {
          gcpRendD3D->FX_PushRenderTarget(0,  CTexture::m_Text_SceneTarget,  GetUtils().GetDepthSurface( CTexture::m_Text_SceneTarget )) ; //&gcpRendD3D->m_DepthBufferOrig); 
          gRenDev->SetViewport(0, 0, CTexture::m_Text_SceneTarget->GetWidth(), CTexture::m_Text_SceneTarget->GetHeight());        
        }
        else GetUtils().StretchRect(CTexture::m_Text_HDRTarget, CTexture::m_Text_SceneTarget);

        static CCryName pTechName0("MotionBlurObject");
        static CCryName pTechName1("MotionBlurObjectUsingMask");

        GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, CRenderer::CV_r_motionblurdynamicquality? pTechName1: pTechName0, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);
        gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

        static CCryName pParamName("motionBlurParams");
        CCryName pParamName2("PI_motionBlurParams");

        CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
        if( CRenderer::CV_r_motionblurdynamicquality )
          GetUtils().SetTexture(CTexture::m_Text_HDRTarget, 0, FILTER_LINEAR);      
        else
          GetUtils().SetTexture(CTexture::m_Text_SceneTarget, 0, FILTER_LINEAR);      

        GetUtils().SetTexture(CTexture::m_Text_HDRBrightPass[0], 1, FILTER_POINT);
        GetUtils().SetTexture(CTexture::m_Text_ZTarget, 2, FILTER_POINT);
        GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 3, FILTER_POINT);

        Vec4 vParams=Vec4(1, 1, 1,  1) ;       
        CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName2, &vParams, 1);

        GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());      
        GetUtils().ShEndPass();

        if( CRenderer::CV_r_motionblurdynamicquality )
        {
          // Restore previous viewport/rt
          gcpRendD3D->FX_PopRenderTarget(0);
          gRenDev->SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());        

          // enable quality skipping
          gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
        }
        else  // update scene target for 2nd pass
          GetUtils().StretchRect(CTexture::m_Text_HDRTarget, CTexture::m_Text_SceneTarget);

        GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, CRenderer::CV_r_motionblurdynamicquality? pTechName1: pTechName0, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);
        gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

        CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
        GetUtils().SetTexture(CTexture::m_Text_SceneTarget, 0, FILTER_LINEAR);      
        GetUtils().SetTexture(CTexture::m_Text_HDRBrightPass[0], 1, FILTER_POINT);
        GetUtils().SetTexture(CTexture::m_Text_ZTarget, 2, FILTER_POINT);
        GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 3, FILTER_POINT);

        vParams.x = 0.5;
        CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName2, &vParams, 1);

        GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());      

        GetUtils().ShEndPass();
      }
      

      gRenDev->m_RP.m_FlagsShader_RT =  nFlagsShaderRT;
      gcpRendD3D->Set2DMode(false, 1, 1);        
    }

    m_RP.m_PersFlags2 &= ~RBPF2_MOTIONBLURPASS;
    gcpRendD3D->m_RP.m_FlagsShader_RT = nPrevFlagsShader_RT;

    GetUtils().Log(" +++ End object motion blur scene +++ \n"); 
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Temporary hack for hdr post effects

void CD3D9Renderer::FX_PostProcessSceneHDR()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_VeryHigh, eSQ_High );
  if( !bQualityCheck )
    return;

  if ( !gcpRendD3D || !CRenderer::CV_r_hdrrendering || !CTexture::m_Text_SceneTarget)
    return;

  if( PostEffectMgr().GetEffects().empty() || gcpRendD3D->GetPolygonMode() == R_WIREFRAME_MODE) 
    return;

  if( !CShaderMan::m_shPostEffects || !CShaderMan::m_shPostEffectsGame )
    return;

  if( !CTexture::m_Text_BackBuffer || !CTexture::m_Text_BackBuffer->GetDeviceTexture() )
    return;

  if( !CTexture::m_Text_SceneTarget || !CTexture::m_Text_SceneTarget->GetDeviceTexture() ) 
    return;

  gcpRendD3D->Set2DMode(true, 1, 1);     

  GetUtils().m_pCurDepthSurface = &gcpRendD3D->m_DepthBufferOrig;

  CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers

  // do camera motion blur
  CMotionBlur::m_pInstance.RenderHDR();

  // depth of field
  CDepthOfField::m_pInstance.RenderHDR();

  // masked blurring
  CFilterMaskedBlurring::m_pInstance.Render();

  CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers

  gcpRendD3D->Set2DMode(false, 1, 1);     
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CREPostProcessData::Create() 
{  
  // Initialize all post process data

  PostEffectMgr().Create();

}

void CREPostProcessData::Release() 
{
  // Free all used resources

  PostEffectMgr().Release();

  GetUtils().Release();  

}

void CREPostProcessData:: Reset()
{  
  // Reset all post process data

  PostEffectMgr().Reset();

}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CPostEffectsMgr::Begin()
{   
  GetUtils().Log("### POST-PROCESSING BEGINS ### "); 
  GetUtils().m_pTimer=iSystem->GetITimer();    

  static EShaderQuality nPrevShaderQuality = eSQ_Low;
  static ERenderQuality nPrevRenderQuality = eRQ_Low;

  EShaderQuality nShaderQuality = (EShaderQuality) gcpRendD3D->EF_GetShaderQuality(eST_PostProcess);
  ERenderQuality nRenderQuality = gRenDev->m_RP.m_eQuality;
  if( nPrevShaderQuality != nShaderQuality || nPrevRenderQuality != nRenderQuality )
  {
    CPostEffectsMgr::Reset();
    nPrevShaderQuality = nShaderQuality;
    nPrevRenderQuality = nRenderQuality;
  }

  gcpRendD3D->GetModelViewMatrix( GetUtils().m_pView.GetData() );
  gcpRendD3D->GetProjectionMatrix( GetUtils().m_pProj.GetData() );

  // Store some commonly used per-frame data

  SPostEffectsUtils::m_pViewProj = GetUtils().m_pView * GetUtils().m_pProj;
  SPostEffectsUtils::m_pViewProj.Transpose();

  gcpRendD3D->ResetToDefault();  
  gcpRendD3D->EF_ResetPipe();

  gcpRendD3D->Set2DMode(true, 1, 1);       
  SPostEffectsUtils::m_pCurDepthSurface = gcpRendD3D->FX_GetScreenDepthSurface();

  SPostEffectsUtils::m_iFrameCounter=(GetUtils().m_iFrameCounter+1)%1000;  

  gcpRendD3D->SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());

  SPostEffectsUtils::m_fWaterLevel = iSystem->GetI3DEngine()->GetWaterLevel(&gRenDev->GetRCamera().Orig);

  if (gcpRendD3D->GetPolygonMode() == R_WIREFRAME_MODE)
  {
#if defined (DIRECT3D9) || defined (OPENGL)
    gcpRendD3D->m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

#elif defined (DIRECT3D10)

    SStateRaster RS = gcpRendD3D->m_StatesRS[gcpRendD3D->m_nCurStateRS];
    RS.Desc.FillMode = D3D10_FILL_SOLID;
    gcpRendD3D->SetRasterState(&RS);

#endif
  }

}

void CPostEffectsMgr::End()
{
  gcpRendD3D->Set2DMode(false, 1, 1);

  gcpRendD3D->SetViewport(GetUtils().m_pScreenRect.left, GetUtils().m_pScreenRect.top, GetUtils().m_pScreenRect.right, GetUtils().m_pScreenRect.bottom); 

  gcpRendD3D->EF_ResetPipe();
  gcpRendD3D->ResetToDefault();  

  gcpRendD3D->m_RP.m_PersFlags &= ~RBPF_POSTPROCESS;

  if(gcpRendD3D->GetPolygonMode() == R_WIREFRAME_MODE)
  {
#if defined (DIRECT3D9) || defined (OPENGL)
    gcpRendD3D->m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
#elif defined (DIRECT3D10)

    SStateRaster RS = gcpRendD3D->m_StatesRS[gcpRendD3D->m_nCurStateRS];
    RS.Desc.FillMode = D3D10_FILL_WIREFRAME;
    gcpRendD3D->SetRasterState(&RS);

#endif
  }

  GetUtils().Log("### POST-PROCESSING ENDS ### ");   
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Engine specific post-processing
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CMotionBlur::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_Medium, eSQ_Medium );
  if( !bQualityCheck )
    return false;

  bool bActive = (CRenderer::s_MotionBlurMode==1 || !CRenderer::CV_r_hdrrendering && (CRenderer::s_MotionBlurMode==3 || CRenderer::s_MotionBlurMode==4)) && !iSystem->IsEditorMode() || CRenderer::s_MotionBlurMode== 5;

  if( !bActive )
  {
    m_pPrevView = GetUtils().m_pView;
  }

  m_fRotationAcc *= max(1.f - CRenderer::CV_r_motionblurdynamicquality_rotationacc_stiffness * iSystem->GetITimer()->GetFrameTime(), 0.f);    
  if (CTexture::m_Text_SceneTarget == NULL || !bActive)
    return false;

  Matrix44 pCurrView( GetUtils().m_pView  ), pCurrProj( GetUtils().m_pProj );  

  m_nQualityInfo = 0;
  m_nSamplesInfo = 0;
  m_nRotSamplesEst = m_fRotationAcc;
  m_nTransSamplesEst = 0;

	const CCamera &currentCamera = gcpRendD3D->GetCamera();

	// If it's a new camera, disable motion blur for the first frame
	if (currentCamera.IsJustActivated())
	{
		m_pPrevView = pCurrView;
		return false;
	}

  // No movement, skip motion blur - why no IsEquivalent for 4x4...
  Vec3 pCurr0 = pCurrView.GetRow(0), pCurr1 = pCurrView.GetRow(1), pCurr2 = pCurrView.GetRow(2), pCurr3 = pCurrView.GetRow(3);
  Vec3 pPrev0 = m_pPrevView.GetRow(0), pPrev1 = m_pPrevView.GetRow(1), pPrev2 = m_pPrevView.GetRow(2), pPrev3 = m_pPrevView.GetRow(3);
  if( pCurr0.IsEquivalent( pPrev0, 0.025f) && pCurr1.IsEquivalent( pPrev1, 0.025f) && pCurr2.IsEquivalent( pPrev2, 0.025f) && pCurr3.IsEquivalent( pPrev3, 0.025f) ) 
    return false;

  return bActive;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CMotionBlur::MotionDetection( float fCurrFrameTime, const Matrix44 &pCurrView, int &nQuality)
{
  float fExposureTime = 0.2f * m_pAmount->GetParam() * CRenderer::CV_r_motionblur_shutterspeed;
  float fRotationDistToPrev = 1.0f;
  float fCurrDistToPrevPos = 1.0f;


  //static float fRotationAcc = 0.0f;
  if( CRenderer::CV_r_motionblurdynamicquality  )
  {
    nQuality = 0;

    float fAlpha = iszero(fCurrFrameTime) ? 0.0f : fExposureTime /fCurrFrameTime;
    Matrix33 pLerpedView = Matrix33(pCurrView)*(1-fAlpha) + Matrix33(m_pPrevView)*fAlpha;   
    Vec3 pLerpedPos = pCurrView.GetRow(3)*(1-fAlpha) + m_pPrevView.GetRow(3) * fAlpha;     

    // Compose final 'previous' viewProjection matrix
    Matrix44 pLView = pLerpedView;
    pLView.m30 = pLerpedPos.x;   
    pLView.m31 = pLerpedPos.y;
    pLView.m32 = pLerpedPos.z; 

    // Set motion blur quality/passes based on amount of camera movement
    const float fMedPosDiferenceThreshold = 0.04f * CRenderer::CV_r_motionblurdynamicquality_translationthreshold;
    const float fMaxPosDiferenceThreshold = 0.08f * CRenderer::CV_r_motionblurdynamicquality_translationthreshold;
    Vec3 pCurrPos = pCurrView.GetInverted().GetRow(3);
    Vec3 pPrevPos = pLView.GetInverted().GetRow(3);
    fCurrDistToPrevPos = (pCurrPos - pPrevPos).len();

    if( fCurrDistToPrevPos > fMaxPosDiferenceThreshold )
      nQuality = 2;
    else
      if( fCurrDistToPrevPos > fMedPosDiferenceThreshold )
        nQuality = 1;

    Vec3 frontPrev = pCurrView.TransformVector( Vec3(0,0,1) );
    frontPrev.Normalize();
    Vec3 frontCurr = pLView.TransformVector( Vec3(0,0,1) );
    frontCurr.Normalize();

    m_fRotationAcc += (frontCurr - frontPrev).len();
    fRotationDistToPrev = m_fRotationAcc;

    const float fMedDotDiferenceThreshold = 0.045f* CRenderer::CV_r_motionblurdynamicquality_rotationthreshold; 
    const float fMaxDotDiferenceThreshold = 0.1f* CRenderer::CV_r_motionblurdynamicquality_rotationthreshold;
    if( m_fRotationAcc > fMaxDotDiferenceThreshold )
      nQuality = max( nQuality , 2);
    else
      if( m_fRotationAcc > fMedDotDiferenceThreshold ) 
        nQuality = max( nQuality , 1); 
  }

  // No movement, skip motion blur
  Vec3 pCurr0 = pCurrView.GetRow(0), pCurr1 = pCurrView.GetRow(1), pCurr2 = pCurrView.GetRow(2), pCurr3 = pCurrView.GetRow(3);
  Vec3 pPrev0 = m_pPrevView.GetRow(0), pPrev1 = m_pPrevView.GetRow(1), pPrev2 = m_pPrevView.GetRow(2), pPrev3 = m_pPrevView.GetRow(3);
  bool isEquivalentMatrix = pCurr0.IsEquivalent( pPrev0, 0.025f) && pCurr1.IsEquivalent( pPrev1, 0.025f) && pCurr2.IsEquivalent( pPrev2, 0.025f) && pCurr3.IsEquivalent( pPrev3, 0.025f);
  if( CRenderer::CV_r_motionblurdynamicquality && isEquivalentMatrix && m_fRotationAcc < 0.005f  || isEquivalentMatrix ) 
  {
    //OutputDebugString("not enough motion \n"); 
    return false; 
  }

  float fsamples_pos = ceilf(clamp_tpl<float>(fCurrDistToPrevPos * 10.0f, 0.0f, 1.0f) *8.0f);
  float fsamples_rot = clamp_tpl<float>(fRotationDistToPrev * 20.0f, 0.0f, 1.0f) *8.0f;

  m_nRotSamplesEst = fsamples_rot;
  m_nTransSamplesEst = fsamples_pos;

  // not enough samples count, skip motion blur
  if( fsamples_pos <= 1.0f && fsamples_rot <= 1.0f)
    return false;

  gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0]|g_HWSR_MaskBit[HWSR_SAMPLE1]|g_HWSR_MaskBit[HWSR_SAMPLE2]);

  // slow movement - use 4 samples only
  m_nSamplesInfo = 8;
  if( fsamples_pos < 4.0f && fsamples_rot < 1.0f )
  {
    m_nSamplesInfo = 4;
    gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CMotionBlur::Render()
{
  // lower specs camera motion blur
  if ( CTexture::m_Text_SceneTarget == NULL)
    return;

  Matrix44 pCurrView( GetUtils().m_pView  ), pCurrProj( GetUtils().m_pProj );  
  int nShaderQuality = gcpRendD3D->EF_GetShaderQuality(eST_PostProcess); 
  uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;

  PROFILE_SHADER_START

    Matrix33 pLerpedView;
  pLerpedView.SetIdentity(); 

  float fCurrFrameTime = gEnv->pTimer->GetFrameTime();
  // renormalize frametime to account for time scaling
  float fTimeScale = gEnv->pTimer->GetTimeScale();
  if (fTimeScale < 1.0f)
  {
    fTimeScale = max(0.0001f, fTimeScale);
    fCurrFrameTime /= fTimeScale;
  }

  // scale down shutter speed a bit. default shutter is 0.02, so final result is 0.004 (old default value)
  float fExposureTime = 0.2f * m_pAmount->GetParam() * CRenderer::CV_r_motionblur_shutterspeed;

  float fAlpha = iszero(fCurrFrameTime) ? 0.0f : fExposureTime /fCurrFrameTime;
  if( CRenderer::CV_r_motionblurframetimescale )
  {
    float fAlphaScale = iszero(fCurrFrameTime) ? 1.0f : min(1.0f, (1.0f / fCurrFrameTime) / ( 32.0f)); // attenuate motion blur for lower frame rates
    fAlpha *= fAlphaScale;
  }

  // Interpolate matrixes and position
  pLerpedView = Matrix33(pCurrView)*(1-fAlpha) + Matrix33(m_pPrevView)*fAlpha;   

  Vec3 pLerpedPos = Vec3::CreateLerp(pCurrView.GetRow(3), m_pPrevView.GetRow(3), fAlpha);     

  // Compose final 'previous' viewProjection matrix
  Matrix44 pLView = pLerpedView;
  pLView.m30 = pLerpedPos.x;   
  pLView.m31 = pLerpedPos.y;
  pLView.m32 = pLerpedPos.z; 

  int nQuality = 1;
  if( !MotionDetection( fCurrFrameTime, pCurrView, nQuality) )
    return;

  if( nShaderQuality < eSQ_High )
    nQuality = 0;

  nQuality = min(nQuality, 1);

  // First generate motion blur mask 
  if( nShaderQuality == eSQ_High )
  {
    Vec3 pCurrDir = pCurrView.GetColumn(2);
    Vec3 pPrevDir = m_pPrevView.GetColumn(2);

    CCryName pTechName("MotionBlurMaskGen");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETSTATES);
    gcpRendD3D->EF_SetState(GS_NODEPTHTEST|GS_COLMASK_A);    

    float fRotationAmount = 1.0 - fabs( pCurrDir.Dot(pPrevDir) );
    Vec4 vParams=Vec4(1, 1, 1,  fRotationAmount) ;       
    static CCryName pParamName("motionBlurParams");
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &vParams, 1);

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());      
    GetUtils().ShEndPass();
  }

  // update backbuffer
  GetUtils().CopyScreenToTexture(CTexture::m_Text_BackBuffer, 1); 

  Matrix44 pViewProjPrev = pLView * pCurrProj;      
  pViewProjPrev.Transpose();

  CVertexBuffer vertexBuffer(&m_pCamSphereVerts[0],VERTEX_FORMAT_P3F_TEX2F);

  float fVectorsScale = m_pVectorsScale->GetParam(); 
  float fCamScale = m_pCameraSphereScale->GetParam();

  Vec4 vCamParams=Vec4(1, 1, 1, fCamScale ) ;
  float fCurrVectorsScale = fVectorsScale;
  Vec4 vParams = Vec4(0, 0, 0, 0) ;       

  Vec4 cBlurVec = PostEffectMgr().GetByNameVec4("Global_DirectionalBlur_Vec") * 0.01f;    
  fCurrVectorsScale = 1.5f;

  static CCryName pTechName("MotionBlurDispl");
  static CCryName pParam0Name("mViewProjPrev");
  static CCryName pParam1Name("motionBlurCamParams");
  static CCryName pParam3Name("vDirectionalBlur");
  static CCryName pParam4Name("PI_motionBlurParams");
  static CCryName pParam2Name("motionBlurParams");

  for( int nPasses = 0; nPasses <= nQuality; ++nPasses )  
  {	
    vParams.w = fCurrVectorsScale;       
    CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
    gcpRendD3D->Set2DMode(false, 1, 1);         

    if( !nPasses )
      gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
    else
      gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE0];

    GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETSTATES);


    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    
    gcpRendD3D->SetCullMode( R_CULL_NONE );

    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam0Name, (Vec4 *)pViewProjPrev.GetData(), 4);    
    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam1Name, &vCamParams, 1);
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam3Name, &cBlurVec, 1);
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam4Name, &vParams, 1);

    gRenDev->DrawTriStrip(&vertexBuffer,m_pCamSphereVerts.Count());

    fCurrVectorsScale *= 0.5f;

    GetUtils().ShEndPass();
    gcpRendD3D->Set2DMode(true, 1, 1);         

    if( nPasses != nQuality )
      GetUtils().CopyScreenToTexture(CTexture::m_Text_BackBuffer, 1); 
  }

  m_nQualityInfo = nQuality;


  // store previous frame data
  m_pPrevView =  pCurrView;
  gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;

  gcpRendD3D->FX_Flush();
  PROFILE_SHADER_END      
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CMotionBlur::RenderHDR()
{
  // hdr camera motion blur
  if( CRenderer::s_MotionBlurMode < 3 || (iSystem->IsEditorMode() && CRenderer::s_MotionBlurMode !=5))
    return;

  m_fRotationAcc *= max(1.f - CRenderer::CV_r_motionblurdynamicquality_rotationacc_stiffness  * iSystem->GetITimer()->GetFrameTime(), 0.f);    

  m_nQualityInfo = 0;
  m_nSamplesInfo = 0;
  m_nRotSamplesEst = m_fRotationAcc;
  m_nTransSamplesEst = 0;

  gcpRendD3D->Set2DMode(false, 1, 1);   
  gcpRendD3D->GetModelViewMatrix( GetUtils().m_pView.GetData() );
  gcpRendD3D->GetProjectionMatrix( GetUtils().m_pProj.GetData() );
  gcpRendD3D->Set2DMode(true, 1, 1);   

  // Store some commonly used per-frame data
  SPostEffectsUtils::m_pViewProj = GetUtils().m_pView * GetUtils().m_pProj;
  SPostEffectsUtils::m_pViewProj.Transpose();

  Matrix44 pCurrView( GetUtils().m_pView  ), pCurrProj( GetUtils().m_pProj );  

  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
  // Process camera motion blur
  int nShaderQuality = gcpRendD3D->EF_GetShaderQuality(eST_PostProcess); 

  PROFILE_SHADER_START

    // First generate motion blur mask 
    Vec3 pCurrDir = pCurrView.GetColumn(2);
  Vec3 pPrevDir = m_pPrevView.GetColumn(2);

  float fCurrFrameTime = gEnv->pTimer->GetFrameTime();
  // renormalize frametime to account for time scaling
  float fTimeScale = gEnv->pTimer->GetTimeScale();
  if (fTimeScale < 1.0f)
  {
    fTimeScale = max(0.0001f, fTimeScale);
    fCurrFrameTime /= fTimeScale;
  }

  // scale down shutter speed a b it. default shutter is 0.02, so final result is 0.004 (old default value)
  float fExposureTime = 0.2f * m_pAmount->GetParam() * CRenderer::CV_r_motionblur_shutterspeed;
  float fAlpha = iszero(fCurrFrameTime) ? 0.0f : fExposureTime /fCurrFrameTime;
  if( CRenderer::CV_r_motionblurframetimescale )
  {
    float fAlphaScale = iszero(fCurrFrameTime) ? 1.0f : min(1.0f, (1.0f / fCurrFrameTime) / ( 32.0f)); // attenuate motion blur for lower frame rates
    fAlpha *= fAlphaScale;
  }

  // Interpolate matrixes and position
  Matrix33 pLerpedView = Matrix33(pCurrView)*(1-fAlpha) + Matrix33(m_pPrevView)*fAlpha;   
  Vec3 pLerpedPos = pCurrView.GetRow(3)*(1-fAlpha) + m_pPrevView.GetRow(3) * fAlpha;     

  // Compose final 'previous' viewProjection matrix
  Matrix44 pLView = pLerpedView;
  pLView.m30 = pLerpedPos.x;   
  pLView.m31 = pLerpedPos.y;
  pLView.m32 = pLerpedPos.z; 

  int nQuality = 2;
  if( !MotionDetection( fCurrFrameTime, pCurrView, nQuality) )
    return;

  // Generate motion blur depth mask
  {
    gcpRendD3D->FX_PushRenderTarget(0,  CTexture::m_Text_SceneTarget,  GetUtils().GetDepthSurface( CTexture::m_Text_SceneTarget )) ; //&gcpRendD3D->m_DepthBufferOrig); 
    gRenDev->SetViewport(0, 0, CTexture::m_Text_SceneTarget->GetWidth(), CTexture::m_Text_SceneTarget->GetHeight());        

    CCryName pTechName("MotionBlurMaskGenHDR");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);
    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

    float fRotationAmount = 1.0 - fabs( pCurrDir.Dot(pPrevDir) );
    Vec4 vParams=Vec4(1, 1, 1,  fRotationAmount) ;       
    static CCryName pParamName("motionBlurParams");
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &vParams, 1);

    GetUtils().SetTexture(CTexture::m_Text_HDRTarget, 0, FILTER_POINT);  
    GetUtils().SetTexture(CTexture::m_Text_ZTarget, 1, FILTER_POINT);  

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());      
    GetUtils().ShEndPass();

    // Restore previous viewport
    gcpRendD3D->FX_PopRenderTarget(0);
    gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);     
  }

  Matrix44 pViewProjPrev = pLView * pCurrProj;      
  pViewProjPrev.Transpose();

  CVertexBuffer vertexBuffer(&m_pCamSphereVerts[0],VERTEX_FORMAT_P3F_TEX2F);

  float fCamScale = m_pCameraSphereScale->GetParam();
  float fCurrVectorsScale = 1.5f;
  Vec4 vParams = Vec4(0, 0, 0, 0) ;       

  static CCryName pParam0Name("mViewProjPrev");
  static CCryName pParam1Name("motionBlurCamParams");
  static CCryName pParam3Name("vDirectionalBlur");
  static CCryName pParam4Name("PI_motionBlurParams");

  for( int nPasses = 0; nPasses <= nQuality; ++nPasses )  
  {	
    CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
    gcpRendD3D->Set2DMode(false, 1, 1);         
    static CCryName pTechName("MotionBlurDisplHDR");

    if( !nPasses )
      gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
    else
      gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE0];

    GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);

    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    
    gcpRendD3D->SetCullMode( R_CULL_NONE );

    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam0Name, (Vec4 *)pViewProjPrev.GetData(), 4);    
    Vec4 cBlurVec = PostEffectMgr().GetByNameVec4("Global_DirectionalBlur_Vec") * 0.01f;    

    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam3Name, &cBlurVec, 1);
    Vec4 vCamParams=Vec4(1, 1, 1, fCamScale ) ;       
    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam1Name, &vCamParams, 1);

    vParams.w = fCurrVectorsScale;       
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam4Name, &vParams, 1);

    GetUtils().SetTexture(CTexture::m_Text_SceneTarget, 0, FILTER_LINEAR);  
    GetUtils().SetTexture(CTexture::m_Text_ZTarget, 1, FILTER_POINT);  

    gRenDev->DrawTriStrip(&vertexBuffer,m_pCamSphereVerts.Count());

    fCurrVectorsScale *= 0.5f;

    GetUtils().ShEndPass();

    gcpRendD3D->Set2DMode(true, 1, 1);         

    // copy hdr render target into scene target
    if(nPasses != nQuality)
      GetUtils().StretchRect(CTexture::m_Text_HDRTarget, CTexture::m_Text_SceneTarget);    
  }

  m_nQualityInfo = nQuality;

  // store previous frame data
  m_pPrevView =  pCurrView;
  m_fPrevFrameTime = fCurrFrameTime;                 
  gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;

  gcpRendD3D->FX_Flush();
  PROFILE_SHADER_END      
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CDepthOfField::Render()
{
  uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;

  gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0]|g_HWSR_MaskBit[HWSR_SAMPLE1]|g_HWSR_MaskBit[HWSR_SAMPLE2]);

  float fBlurAmount = m_pBlurAmount->GetParam();

  Vec4 vParamsFocus;
  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  PROFILE_SHADER_START

    float fPrevDist = 0, fPrevMaxCoC = 0, fPrevFocusRange = 0, fPrevBlurAmount = 0, fPrevFocusMin = 0, fPrevFocusMax = 0;
  float fPrevFocusLimit = 0, fPrevUseMask = 0;

  //// override default game settings
  //if( CRenderer::CV_r_dof == 2 && ( !IsActive())  ) 
  //{    
  //  fPrevDist = m_pFocusDistance->GetParam();  
  //  fPrevFocusRange = m_pFocusRange->GetParam();      
  //  fPrevMaxCoC = m_pMaxCoC->GetParam();  
  //  fPrevBlurAmount = m_pBlurAmount->GetParam();  
  //  fPrevFocusMin = m_pFocusMin->GetParam();  
  //  fPrevFocusMax = m_pFocusMax->GetParam();  
  //  fPrevFocusLimit = m_pFocusLimit->GetParam();  
  //  fPrevUseMask = m_pUseMask->GetParam();

  //  m_pFocusDistance->SetParam(1.0);
  //  m_pFocusRange->SetParam(900.0f);
  //  m_pMaxCoC->SetParam(12.0f);
  //  m_pBlurAmount->SetParam( 0.3f);  
  //  m_pFocusMin->SetParam(2.0f);
  //  m_pFocusMax->SetParam(10.0f);
  //  m_pFocusLimit->SetParam(100.0f);
  //  m_pUseMask->SetParam(0.0f);       
  //}

  float fFocusRange = m_pFocusRange->GetParam();
  float fMaxCoC = m_pMaxCoC->GetParam();  
  fBlurAmount = m_pBlurAmount->GetParam();

  ///////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////
  {  
    gcpRendD3D->FX_PushRenderTarget(0,  CTexture::m_Text_BackBuffer,  GetUtils().m_pCurDepthSurface); 
    gRenDev->SetViewport(0, 0, CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());        

    // Copy depth into backbuffer alpha channel
    bool bGameDof = IsActive();
    float fUserFocusRange = m_pUserFocusRange->GetParam();
    float fUserFocusDistance = m_pUserFocusDistance->GetParam();
    float fUserBlurAmount = m_pUserBlurAmount->GetParam();
    float fFrameTime = clamp_tpl<float>(iSystem->GetITimer()->GetFrameTime()*3.0f, 0.0f, 1.0f); 

    if( bGameDof )
      fUserFocusRange = fUserFocusDistance = fUserBlurAmount = 0.0f;

    m_fUserFocusRangeCurr += (fUserFocusRange - m_fUserFocusRangeCurr) * fFrameTime;
    m_fUserFocusDistanceCurr += (fUserFocusDistance - m_fUserFocusDistanceCurr) * fFrameTime;
    m_fUserBlurAmountCurr += ( fUserBlurAmount - m_fUserBlurAmountCurr) * fFrameTime;


    float fUseMask = m_pUseMask->GetParam();

    if(fFocusRange<0.0f && bGameDof)
    { 
      // Special case for gameplay
      if( fUseMask )        
      {
        static CCryName TechName("CopyDepthToAlphaBiased");
        GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
      }
      else
      {
        static CCryName TechName("CopyDepthToAlphaBiasedNoMask");
        GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);            
      }
    }
    else
    {
      // For cinematics (simplified interface, using only focus distance/range)
      static CCryName TechName("CopyDepthToAlphaNoMask");
      GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);                                   
    }

    gcpRendD3D->EF_SetState(GS_NODEPTHTEST|GS_COLMASK_A); 

    if( bGameDof )
    {
      if(fFocusRange<0.0f)
      {
        float fFocusMin = m_pFocusMin->GetParam();
        float fFocusMax = m_pFocusMax->GetParam();
        float fFocusLimit = m_pFocusLimit->GetParam();

        // near blur plane distance, far blur plane distance, focus plane distance, blur amount
        vParamsFocus=Vec4(fFocusMin, fFocusMax, fFocusLimit-fFocusMax, fBlurAmount);   
      }
      else
      {
        float fFocusRange = m_pFocusRange->GetParam();
        float fFocusDistance = m_pFocusDistance->GetParam();

        // near blur plane distance, far blur plane distance, focus plane distance, blur amount
        vParamsFocus=Vec4(-fFocusRange*0.5f, fFocusRange*0.5f, fFocusDistance, fBlurAmount);    
      }
    }
    else
    {
      // near blur plane distance, far blur plane distance, focus plane distance, blur amount
      vParamsFocus=Vec4(-m_fUserFocusRangeCurr*0.5f, m_fUserFocusRangeCurr*0.5f, m_fUserFocusDistanceCurr, m_fUserBlurAmountCurr);   
    }


    static CCryName ParamName("dofParamsFocus");
    CShaderMan::m_shPostEffects->FXSetPSFloat(ParamName, &vParamsFocus, 1);

    GetUtils().SetTexture(CTexture::m_Text_ZTarget, 0, FILTER_POINT);  

    CTexture *pMaskTex = const_cast<CTexture *> (static_cast<CParamTexture*>(m_pMaskTex)->GetParamTexture());

    if(pMaskTex && fUseMask)
    {
      float fMaskW = pMaskTex->GetWidth();
      float fMaskH = pMaskTex->GetHeight();

      Vec4 pParamTexScale = Vec4(0, 0, fMaskW, fMaskH); 

      GetUtils().SetTexture(pMaskTex, 1, FILTER_LINEAR);  
    }

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

    GetUtils().ShEndPass();                    


    // Restore previous viewport
    gcpRendD3D->FX_PopRenderTarget(0);
    gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);     

    // Update back-buffer
    GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_BackBufferScaled[1]);
  }

  GetUtils().TexBlurGaussian(CTexture::m_Text_BackBufferScaled[1], 1, 1, 1, false);

  ///////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////

  static CCryName TechDOFName("DepthOfField");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, TechDOFName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);                    

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);          

  static CCryName Param1Name("dofParamsFocus");
  CShaderMan::m_shPostEffects->FXSetPSFloat(Param1Name, &vParamsFocus, 1);

  Vec4 vParamsBlur=Vec4(fMaxCoC*0.5f, fMaxCoC, fBlurAmount, fBlurAmount);   
  static CCryName Param2Name("dofParamsBlur");
  CShaderMan::m_shPostEffects->FXSetPSFloat(Param2Name, &vParamsBlur, 1);    

  Vec4 vPixelSizes=Vec4(1.0f/(float)CTexture::m_Text_BackBuffer->GetWidth(),
    1.0f/(float)CTexture::m_Text_BackBuffer->GetHeight(),
    1.0f/(float)CTexture::m_Text_BackBufferScaled[1]->GetWidth(),
    1.0f/(float)CTexture::m_Text_BackBufferScaled[1]->GetHeight()); 
  static CCryName Param3Name("pixelSizes");
  CShaderMan::m_shPostEffects->FXSetPSFloat(Param3Name, &vPixelSizes, 1);    

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);
  GetUtils().SetTexture(CTexture::m_Text_BackBufferScaled[1], 1);  

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

  GetUtils().ShEndPass();                    

  //// override default game settings
  //if( CRenderer::CV_r_dof == 2 && ( !IsActive())  ) 
  //{    
  //  m_pFocusDistance->SetParam( fPrevDist );
  //  m_pFocusRange->SetParam( fPrevFocusRange );
  //  m_pMaxCoC->SetParam( fPrevMaxCoC );
  //  m_pBlurAmount->SetParam( fPrevBlurAmount );  
  //  m_pFocusMin->SetParam( fPrevFocusMin );
  //  m_pFocusMax->SetParam( fPrevFocusMax );
  //  m_pFocusLimit->SetParam( fPrevFocusLimit );
  //  m_pUseMask->SetParam( fPrevUseMask );       
  //}

  gcpRendD3D->FX_Flush();
  PROFILE_SHADER_END    

    gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
} 

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CDepthOfField::RenderHDR()
{
  if ( !CRenderer::CV_r_usezpass || !CRenderer::CV_r_dof || CRenderer::CV_r_dof != 2)  
    return;

  float fTimeOfDayBlurAmount = m_pTimeOfDayBlurAmount->GetParam();
  if( (m_pBlurAmount->GetParam() <= 0.1f && m_pUserBlurAmount->GetParam() <= 0.1f && fTimeOfDayBlurAmount <= 0.1f))      
    return;

  if( !IsActive() && !m_pUserActive->GetParam() && (!fTimeOfDayBlurAmount || (fTimeOfDayBlurAmount && (!CRenderer::CV_r_colorgrading || !CRenderer::CV_r_colorgrading_dof))) )
    return;

  float fBlurAmount = m_pBlurAmount->GetParam();     

  Vec4 vParamsFocus;
  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
  // Enable corresponding shader variation
  gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0]|g_HWSR_MaskBit[HWSR_SAMPLE1]|g_HWSR_MaskBit[HWSR_SAMPLE2]);

  //if( CRenderer::CV_r_colorgrading_levels && (fMinInput || fGammaInput || fMaxInput || fMinOutput ||fMaxOutput) )
  gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0]; // enable hdr version

  float fPrevDist = 0, fPrevMaxCoC = 0, fPrevFocusRange = 0, fPrevBlurAmount = 0, fPrevFocusMin = 0, fPrevFocusMax = 0;
  float fPrevFocusLimit = 0, fPrevUseMask = 0;

  float fTodFocusRange = m_pTimeOfDayFocusRange->GetParam();
  float fTodBlurAmount = m_pTimeOfDayBlurAmount->GetParam();

  float fFocusRange = m_pFocusRange->GetParam();
  float fMaxCoC = m_pMaxCoC->GetParam();  
  fBlurAmount = m_pBlurAmount->GetParam();

  //if( fBlurAmount < 0.01f)
  //return;

  PROFILE_SHADER_START

    CTexture *pSceneRT = CTexture::m_Text_SceneTarget;
  CTexture *pSceneScaledTempRT = CTexture::m_Text_HDRTargetScaled[0];
  CTexture *pSceneScaledRT = CTexture::m_Text_HDRTargetScaled[1];

  bool bGameDof = IsActive();
  float fUserBlurAmount = m_pUserBlurAmount->GetParam();

  // update scene target
  //gcpRendD3D->FX_ScreenStretchRect(pSceneRT);

  ///////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////
  //if( 0 ) 
  {  
    gcpRendD3D->FX_PushRenderTarget(0,  pSceneRT,  GetUtils().GetDepthSurface( pSceneRT )); //&gcpRendD3D->m_DepthBufferOrig); 
    gRenDev->SetViewport(0, 0, pSceneRT->GetWidth(), pSceneRT->GetHeight());        

    // Copy depth into backbuffer alpha channel

    float fUserFocusRange = m_pUserFocusRange->GetParam();
    float fUserFocusDistance = m_pUserFocusDistance->GetParam();

    float fFrameTime = clamp_tpl<float>(iSystem->GetITimer()->GetFrameTime()*3.0f, 0.0f, 1.0f); 

    if( bGameDof )
      fUserFocusRange = fUserFocusDistance = fUserBlurAmount = 0.0f;

    m_fUserFocusRangeCurr += (fUserFocusRange - m_fUserFocusRangeCurr) * fFrameTime;
    m_fUserFocusDistanceCurr += (fUserFocusDistance - m_fUserFocusDistanceCurr) * fFrameTime;
    m_fUserBlurAmountCurr += ( fUserBlurAmount - m_fUserBlurAmountCurr) * fFrameTime;

    float fUseMask = m_pUseMask->GetParam();

    if(fFocusRange<0.0f && bGameDof)
    { 
      // Special case for gameplay
      if( fUseMask )        
      {
        static CCryName TechName("CopyDepthToAlphaBiased");
        GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
      }
      else
      {
        static CCryName TechName("CopyDepthToAlphaBiasedNoMask");
        GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);            
      }
    }
    else
    {
      // For cinematics (simplified interface, using only focus distance/range)
      static CCryName TechName("CopyDepthToAlphaNoMask");
      GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);                                   
    }

    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);  //|GS_COLMASK_A

    if( bGameDof )
    {
      if(fFocusRange<0.0f)
      {
        float fFocusMin = m_pFocusMin->GetParam();
        float fFocusMax = m_pFocusMax->GetParam();
        float fFocusLimit = m_pFocusLimit->GetParam();

        // near blur plane distance, far blur plane distance, focus plane distance, blur amount
        vParamsFocus=Vec4(fFocusMin, fFocusMax, fFocusLimit-fFocusMax, fBlurAmount);   
      }
      else
      {
        float fFocusRange = m_pFocusRange->GetParam();
        float fFocusDistance = m_pFocusDistance->GetParam();

        // near blur plane distance, far blur plane distance, focus plane distance, blur amount
        vParamsFocus=Vec4(-fFocusRange*0.5f, fFocusRange*0.5f, fFocusDistance, fBlurAmount);    
      }
    }
    else
    {
      // near blur plane distance, far blur plane distance, focus plane distance, blur amount
      static float s_fTodFocusRange = 0.0f;
      static float s_fTodBlurAmount = 0.0f;
      bool bUseGameSettings = (m_pUserActive->GetParam() )? true: false;

      if( bUseGameSettings )
      {
        s_fTodFocusRange += (m_fUserFocusRangeCurr - s_fTodFocusRange) * fFrameTime;
        s_fTodBlurAmount += (m_fUserBlurAmountCurr - s_fTodBlurAmount) * fFrameTime;

        vParamsFocus=Vec4(-m_fUserFocusRangeCurr*0.5f, m_fUserFocusRangeCurr*0.5f, m_fUserFocusDistanceCurr, m_fUserBlurAmountCurr);   
      }
      else
      {
        s_fTodFocusRange += (fTodFocusRange*2.0f - s_fTodFocusRange) * fFrameTime;
        s_fTodBlurAmount += (fTodBlurAmount - s_fTodBlurAmount) * fFrameTime;

        vParamsFocus=Vec4(-s_fTodFocusRange*0.5f, s_fTodFocusRange*0.5f, 0, s_fTodBlurAmount);   
      }
    }

    static CCryName ParamName("dofParamsFocus");
    CShaderMan::m_shPostEffects->FXSetPSFloat(ParamName, &vParamsFocus, 1);

    //vParamsFocus=Vec4(0, 0, fTodFocusRange, fTodBlurAmount);   
    //static CCryName Param1Name("dofParamsFocus2");
    //CShaderMan::m_shPostEffects->FXSetPSFloat(Param1Name, &vParamsFocus, 1);

    GetUtils().SetTexture(CTexture::m_Text_ZTarget, 0, FILTER_POINT);  

    CTexture *pMaskTex = const_cast<CTexture *> (static_cast<CParamTexture*>(m_pMaskTex)->GetParamTexture());

    if(pMaskTex && fUseMask && bGameDof)
    {
      float fMaskW = pMaskTex->GetWidth();
      float fMaskH = pMaskTex->GetHeight();

      Vec4 pParamTexScale = Vec4(0, 0, fMaskW, fMaskH); 

      GetUtils().SetTexture(pMaskTex, 1, FILTER_LINEAR);  
      GetUtils().SetTexture(CTexture::m_Text_HDRTarget, 2, FILTER_POINT);
    }
    else
      GetUtils().SetTexture(CTexture::m_Text_HDRTarget, 1, FILTER_POINT);

    GetUtils().DrawFullScreenQuad(pSceneRT->GetWidth(), pSceneRT->GetHeight());

    GetUtils().ShEndPass();                    

    // Restore previous viewport
    gcpRendD3D->FX_PopRenderTarget(0);
    gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);     

    // Update back-buffer
    GetUtils().StretchRect(pSceneRT, pSceneScaledTempRT);
    //GetUtils().StretchRect(pSceneScaledTempRT, pSceneScaledRT);
  }

  //GetUtils().TexBlurGaussian(pSceneScaledRT, 1, 1, 0.5, false);  
  GetUtils().TexBlurGaussian(pSceneScaledTempRT, 1, 1, 1, false);  

  ///////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////

  static CCryName TechDOFName("DepthOfFieldHDR");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, TechDOFName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);                    

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);          

  static CCryName Param1Name("dofParamsFocus");
  CShaderMan::m_shPostEffects->FXSetPSFloat(Param1Name, &vParamsFocus, 1);

  Vec4 vParamsBlur=Vec4(fMaxCoC*0.5f, fMaxCoC, fBlurAmount, fBlurAmount);   
  static CCryName Param2Name("dofParamsBlur");
  CShaderMan::m_shPostEffects->FXSetPSFloat(Param2Name, &vParamsBlur, 1);    

  Vec4 vPixelSizes=Vec4(1.0f/(float)pSceneRT->GetWidth(),
    1.0f/(float)pSceneRT->GetHeight(),
    1.0f/(float)pSceneScaledTempRT->GetWidth(),
    1.0f/(float)pSceneScaledTempRT->GetHeight()); 
  static CCryName Param3Name("pixelSizes");
  CShaderMan::m_shPostEffects->FXSetPSFloat(Param3Name, &vPixelSizes, 1);    

  GetUtils().SetTexture(pSceneRT, 0, FILTER_POINT);
  GetUtils().SetTexture(pSceneScaledTempRT, 1);  
  GetUtils().SetTexture(m_pNoise, 2, FILTER_POINT, 0);  


  GetUtils().DrawFullScreenQuad(pSceneRT->GetWidth(), pSceneRT->GetHeight());

  GetUtils().ShEndPass();                    

  gcpRendD3D->FX_Flush();
  PROFILE_SHADER_END    

    gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CAlphaTestAA::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_High, eSQ_High );
  if( !bQualityCheck || gRenDev->m_RP.m_FSAAData.Type )
    return false;

  return ( IsActive() && CRenderer::CV_r_useedgeaa != 0 );
}

void CAlphaTestAA::Render()
{
  if(!CRenderer::CV_r_usezpass) 
  {
    return;
  }	

  PROFILE_SHADER_START

    if(CRenderer::CV_r_useedgeaa == 2)
    {
      static CCryName TechName("EdgeAA");
      GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);                    
    }
    else 
    {
      static CCryName TechName("EdgeBlurOpt");
      GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);                    
    }

    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

    GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_LINEAR);   
    GetUtils().SetTexture(CTexture::m_Text_ZTarget, 1, FILTER_POINT);        
    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());


    GetUtils().ShEndPass();                    

    gcpRendD3D->FX_Flush();
    PROFILE_SHADER_END
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

const unsigned int c_perspWarpGridSize( 64 );
const unsigned int c_perspWarpNumVertices( c_perspWarpGridSize * c_perspWarpGridSize );
const unsigned int c_perspWarpNumStripIndices( ( c_perspWarpGridSize - 1 ) * ( 2 * c_perspWarpGridSize + 2 ) - 2 );
const unsigned int c_perspWarpNumStripTriangles( c_perspWarpNumStripIndices - 2 );

int CPerspectiveWarp::Create()
{  
#if defined (DIRECT3D9) || defined (OPENGL)

  // allocate VB for grid  and fill it
  assert( 0 == m_pGridVB );
  if( FAILED( gcpRendD3D->m_pd3dDevice->CreateVertexBuffer( c_perspWarpNumVertices * sizeof( struct_VERTEX_FORMAT_P3F ), 
    D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, (IDirect3DVertexBuffer9**) &m_pGridVB, 0 ) ) )
  {
    return( 0 );
  }

  struct_VERTEX_FORMAT_P3F* pVertices( 0 );
  if( FAILED( ( (IDirect3DVertexBuffer9*) m_pGridVB )->Lock( 0, 0, (void **) &pVertices, 0 ) ) )
  {
    return( 0 );
  }

  for( unsigned int y( 0 ); y < c_perspWarpGridSize; ++y )
  {
    for( unsigned int x( 0 ); x < c_perspWarpGridSize; ++x )
    {
      pVertices->xyz.x = 2 * (float) x /  (float) ( c_perspWarpGridSize - 1 ) - 1;
      pVertices->xyz.y = 1 - 2 * (float) y /  (float) ( c_perspWarpGridSize - 1 );
      pVertices->xyz.z = 0;

      ++pVertices;
    }
  }
  ( (IDirect3DVertexBuffer9*) m_pGridVB )->Unlock();

  // allocate IB for grid and fill it
  assert( 0 == m_pGridIB );
  if( FAILED( gcpRendD3D->m_pd3dDevice->CreateIndexBuffer( c_perspWarpNumStripIndices * sizeof( WORD ), 
    D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, (IDirect3DIndexBuffer9**) &m_pGridIB, 0 ) ) )
  {
    return( 0 );
  }

  WORD* pIndices( 0 );
  if( FAILED( ( (IDirect3DIndexBuffer9*) m_pGridIB )->Lock( 0, 0, (void **) &pIndices, 0 ) ) )
  {
    return( 0 );
  }

  int iIndex = 0;
  for( unsigned int a( 0 ); a < c_perspWarpGridSize - 1; ++a )
  {
    for( unsigned int i( 0 ); i < c_perspWarpGridSize; ++i, pIndices += 2, ++iIndex )
    {
      pIndices[ 0 ] = iIndex;
      pIndices[ 1 ] = iIndex + c_perspWarpGridSize;
    }

    // connect two strips by inserting last index of previous strip
    // and first index of current strip => creates four degenerated triangles
    if( c_perspWarpGridSize - 2 > a )
    {
      pIndices[ 0 ] = iIndex + c_perspWarpGridSize - 1;
      pIndices[ 1 ] = iIndex;

      pIndices += 2;
    }
  }
  ( (IDirect3DIndexBuffer9*) m_pGridIB )->Unlock();
#elif defined (DIRECT3D10)
  struct_VERTEX_FORMAT_P3F *pVertices = new struct_VERTEX_FORMAT_P3F[c_perspWarpNumVertices];
  struct_VERTEX_FORMAT_P3F *pVerts = pVertices;
  for( unsigned int y( 0 ); y < c_perspWarpGridSize; ++y )
  {
    for( unsigned int x( 0 ); x < c_perspWarpGridSize; ++x )
    {
      pVertices->xyz.x = 2 * (float) x /  (float) ( c_perspWarpGridSize - 1 ) - 1;
      pVertices->xyz.y = 1 - 2 * (float) y /  (float) ( c_perspWarpGridSize - 1 );
      pVertices->xyz.z = 0;

      ++pVertices;
    }
  }

  D3D10_SUBRESOURCE_DATA InitData;
  InitData.pSysMem = pVerts;
  InitData.SysMemPitch = 0;
  InitData.SysMemSlicePitch = 0;

  // allocate VB for grid  and fill it
  assert( 0 == m_pGridVB );
  D3D10_BUFFER_DESC BufDescV;
  ZeroStruct(BufDescV);
  BufDescV.ByteWidth = c_perspWarpNumVertices * sizeof( struct_VERTEX_FORMAT_P3F );
  BufDescV.Usage = D3D10_USAGE_DEFAULT;
  BufDescV.BindFlags = D3D10_BIND_VERTEX_BUFFER;
  BufDescV.CPUAccessFlags = 0;
  BufDescV.MiscFlags = 0;
  if(FAILED(gcpRendD3D->m_pd3dDevice->CreateBuffer(&BufDescV, &InitData, (ID3D10Buffer**) &m_pGridVB)))
  {
    SAFE_DELETE_ARRAY(pVerts);
    return( 0 );
  }
  SAFE_DELETE_ARRAY(pVerts);

  WORD* pIndices = new WORD [c_perspWarpNumStripIndices];
  WORD *pInds = pIndices;

  InitData.pSysMem = pInds;
  InitData.SysMemPitch = 0;
  InitData.SysMemSlicePitch = 0;

  int iIndex = 0;
  for( unsigned int a( 0 ); a < c_perspWarpGridSize - 1; ++a )
  {
    for( unsigned int i( 0 ); i < c_perspWarpGridSize; ++i, pIndices += 2, ++iIndex )
    {
      pIndices[ 0 ] = iIndex;
      pIndices[ 1 ] = iIndex + c_perspWarpGridSize;
    }

    // connect two strips by inserting last index of previous strip
    // and first index of current strip => creates four degenerated triangles
    if( c_perspWarpGridSize - 2 > a )
    {
      pIndices[ 0 ] = iIndex + c_perspWarpGridSize - 1;
      pIndices[ 1 ] = iIndex;

      pIndices += 2;
    }
  }

  // allocate IB for grid and fill it
  assert( 0 == m_pGridIB );
  D3D10_BUFFER_DESC BufDescI;
  ZeroStruct(BufDescI);
  BufDescI.ByteWidth =  c_perspWarpNumStripIndices * sizeof( WORD );
  BufDescI.Usage = D3D10_USAGE_DEFAULT;
  BufDescI.BindFlags = D3D10_BIND_INDEX_BUFFER;
  BufDescI.CPUAccessFlags = 0;
  BufDescI.MiscFlags = 0;
  if(FAILED(gcpRendD3D->m_pd3dDevice->CreateBuffer(&BufDescI, &InitData, (ID3D10Buffer**) &m_pGridIB)))
  {
    SAFE_DELETE_ARRAY(pInds);
    return( 0 );
  }

  SAFE_DELETE_ARRAY(pInds);
#endif

  return( 1 );
}

template< typename T >
void SafeRelease( T& pInterface )
{
  if( 0 != pInterface )
  {
    pInterface->Release();
    pInterface = 0;
  }
}

void CPerspectiveWarp::Release()
{ 
  //SafeRelease( (IDirect3DVertexBuffer9*&) m_pGridVB );
  //SafeRelease( (IDirect3DIndexBuffer9*&) m_pGridIB );
}

void CPerspectiveWarp::Render()
{

  static CCryName pTechName("PerspectiveWarp");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETSTATES);                    

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  //Vec4 vParams=Vec4(1, 1, 1, (float)GetUtils().m_iFrameCounter);    
  //CShaderMan::m_shPostEffects->FXSetVSFloat("viewModeParams", &vParams, 1);

  if (!FAILED(gcpRendD3D->EF_SetVertexDeclaration( 0, VERTEX_FORMAT_P3F )))
  {
    gcpRendD3D->EF_Commit();


    //gcpRendD3D->m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );

    gcpRendD3D->FX_SetVStream(0, (IDirect3DVertexBuffer9*)m_pGridVB, 0, m_VertexSize[VERTEX_FORMAT_P3F]);
    gcpRendD3D->FX_SetIStream((IDirect3DIndexBuffer9*)m_pGridIB);
#if defined (DIRECT3D9) || defined (OPENGL)
    gcpRendD3D->m_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, 0, c_perspWarpNumVertices, 0, c_perspWarpNumStripTriangles );
#elif defined (DIRECT3D10)
    assert(0);
#endif
  }

  //gcpRendD3D->m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

  GetUtils().ShEndPass();                    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

int CGlitterSprites::Create()
{
  static int lastGlitterAmount=gRenDev->CV_r_glitterAmount;

  if(!gEnv->p3DEngine)
  {
    return 0;
  }

  // No terrain ? Free all glitter particles if required
  if(!gEnv->p3DEngine->GetITerrain())
  {
    Release();    
    return 0;
  }

  if(lastGlitterAmount!=gRenDev->CV_r_glitterAmount)
  {
    Release();    
  }

  // Already generated ? No need to proceed
  if(!m_pSpriteList.empty())
  {
    return 1;
  }

  // Generate main sprite list
  m_pSpriteList.reserve(gRenDev->CV_r_glitterAmount);
  for(int p=0; p<gRenDev->CV_r_glitterAmount; p++)
  {
    SSprite *pSprite= new SSprite;
    m_pSpriteList.push_back(pSprite);
  }

  lastGlitterAmount=gRenDev->CV_r_glitterAmount;

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Create vertex/index buffers

  HRESULT hr(S_OK);

#if defined (DIRECT3D9) || defined (OPENGL)

  // Make sure to always release
  SafeRelease(((IDirect3DVertexBuffer9*&) m_pVB));
  SafeRelease(((IDirect3DIndexBuffer9*&) m_pIB));

  // create vertex buffer
  if(FAILED(hr = gcpRendD3D->m_pd3dDevice->CreateVertexBuffer(4* gRenDev->CV_r_glitterAmount * sizeof(struct_VERTEX_FORMAT_P3F_TEX3F), 
    D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, (IDirect3DVertexBuffer9**) &m_pVB, 0)))
  {
    return hr;
  }

  // create index buffer and copy data
  if(FAILED(hr = gcpRendD3D->m_pd3dDevice->CreateIndexBuffer(6* gRenDev->CV_r_glitterAmount * sizeof( uint16 ), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, (IDirect3DIndexBuffer9**) &m_pIB, 0)))
  {
    return hr;
  }

  uint16* pIndices(0);
  if(FAILED( hr = ((IDirect3DIndexBuffer9*) m_pIB)->Lock( 0, 0, (void **) &pIndices, 0 ) ))
  {
    return hr;
  }

  int nIndex = 0;
  for(unsigned int a=0; a < gRenDev->CV_r_glitterAmount; a++)
  {        
    pIndices[0] = nIndex++;
    pIndices[1] = nIndex++;
    pIndices[2] = nIndex++;

    pIndices[3] = pIndices[1];    
    pIndices[4] = nIndex++;    
    pIndices[5] = pIndices[2];    

    pIndices += 6;
  }

  hr = ((IDirect3DIndexBuffer9*) m_pIB)->Unlock();

#elif defined (DIRECT3D10)
  assert(0);
#endif

  return 1;
}  

void CGlitterSprites::Release()
{
  if(m_pSpriteList.empty())
  {
    return;
  }

  SpriteVecItor pItor, pItorEnd=m_pSpriteList.end(); 
  for(pItor=m_pSpriteList.begin(); pItor!=pItorEnd; ++pItor )
  {
    SAFE_DELETE((*pItor));
  } 
  m_pSpriteList.clear(); 

#if defined(PS3)
  assert("D3D9 call during PS3/D3D10 rendering");
#else
  SafeRelease(((IDirect3DVertexBuffer9*&) m_pVB));
  SafeRelease(((IDirect3DIndexBuffer9*&) m_pIB));  
#endif
}

void CGlitterSprites::Render()
{   
  gcpRendD3D->Set2DMode(false, 1, 1); 

  float fOffsetU = (float)GetUtils().m_pScreenRect.right; 
  float fOffsetV = (float)GetUtils().m_pScreenRect.bottom;  

  static CCryName pTechName("GlitterSprites");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName);   

  // Store glitter parameters
  Vec4 pGlitterSprParams= Vec4(gcpRendD3D->CV_r_glitterVariation, gcpRendD3D->CV_r_glitterSpecularPow, 
    0, gcpRendD3D->CV_r_glitterSize);  
  static CCryName pParam0Name("glitterParams");
  CShaderMan::m_shPostEffects->FXSetVSFloat(pParam0Name, &pGlitterSprParams, 1); 

  // Store camera parameters
  Vec3 vx = gcpRendD3D->GetRCamera().X; 
  Vec3 vy = gcpRendD3D->GetRCamera().Y;  

  Vec4 pCamUpVector=Vec4(vy.x, vy.y, vy.z, 0);
  static CCryName pParam1Name("camUpVector");
  CShaderMan::m_shPostEffects->FXSetVSFloat(pParam1Name, &pCamUpVector, 1); 

  Vec4 pcamRightVector=Vec4(vx.x, vx.y, vx.z, 0);
  static CCryName pParam2Name("camRightVector");
  CShaderMan::m_shPostEffects->FXSetVSFloat(pParam2Name, &pcamRightVector, 1); 

  // Lock and store glitter position/max dist 
  struct_VERTEX_FORMAT_P3F_TEX3F *pVertices(0);
#if defined(PS3)
  assert("D3D9 call during PS3/D3D10 rendering");
#else
  if( FAILED(((IDirect3DVertexBuffer9*)m_pVB)->Lock( 0, 0, (void **) &pVertices, 0 ) ) )
  {
    return;
  }
#endif

  int nTriCount=0;
  SpriteVecItor pItor, pItorEnd=m_pSpriteList.end();   
  for(pItor=m_pSpriteList.begin(); pItor!=pItorEnd; ++pItor )
  {
    SSprite *pSprite=(*pItor);
    if(!pSprite->m_bVisible)
    {
      continue;
    }

    // Store glitter position, texture coordinates and on last texture coordinate max sprite distance(used
    // for smooth transition)
    pVertices[0].p=pSprite->m_pPos;
    pVertices[0].st=Vec3(0, 0, pSprite->m_fAttenDist);

    pVertices[1].p=pSprite->m_pPos;
    pVertices[1].st=Vec3(1, 0, pSprite->m_fAttenDist);

    pVertices[2].p=pSprite->m_pPos;
    pVertices[2].st=Vec3(0, 1, pSprite->m_fAttenDist);

    pVertices[3].p=pSprite->m_pPos;
    pVertices[3].st=Vec3(1, 1, pSprite->m_fAttenDist);

    pVertices+=4;
    nTriCount+=2;

    //Vec4 pGlitterSprParams= Vec4(pSprite->m_pPos.x, pSprite->m_pPos.y, pSprite->m_pPos.z, pSprite->m_fAttenDist);          
    //CShaderMan::m_shPostEffects->FXSetVSFloat("glitterSprParams", &pGlitterSprParams, 4);
    //gcpRendD3D->DrawTriStrip(&pGlitterQuadVB,4); 
  } 

#if defined(PS3)
  assert("D3D9 call during PS3/D3D10 rendering");
#else
  ((IDirect3DVertexBuffer9*)m_pVB)->Unlock();
#endif

  // Prepare to render batch
  gcpRendD3D->EF_Commit();
  if (!FAILED(gcpRendD3D->EF_SetVertexDeclaration( 0, VERTEX_FORMAT_P3F_TEX3F)))
  {
    gcpRendD3D->FX_SetVStream(0, (IDirect3DVertexBuffer9*)m_pVB, 0, sizeof(struct_VERTEX_FORMAT_P3F_TEX3F));
    gcpRendD3D->FX_SetIStream((IDirect3DIndexBuffer9*)m_pIB);
#if defined (DIRECT3D9) || defined (OPENGL)
    gcpRendD3D->m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4* gRenDev->CV_r_glitterAmount, 0, nTriCount); 
#elif defined (DIRECT3D10)
    assert(0);
#endif
  }

  GetUtils().ShEndPass();   

  gcpRendD3D->Set2DMode(true, 1, 1);  
}  

void CGlittering::Render() 
{

  // Update glitter sprites if needed
  m_pGlitterSprites.Update(); 
  m_pGlitterSprites.Render();

} 

///////////////////////////////////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CColorCorrection::Render() 
{
  static CCryName pTechName("ColorCorrection");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  // Compose final color matrix and set fragment program constants
  Matrix44 &pColorMat = GetUtils().GetColorMatrix();

  Vec4 pColorMatrix[4]=
  {
    Vec4(pColorMat.m00, pColorMat.m01, pColorMat.m02, pColorMat.m03),
    Vec4(pColorMat.m10, pColorMat.m11, pColorMat.m12, pColorMat.m13), 
    Vec4(pColorMat.m20, pColorMat.m21, pColorMat.m22, pColorMat.m23),
    Vec4(pColorMat.m30, pColorMat.m31, pColorMat.m32, pColorMat.m33)
  };  

  static CCryName pParamName("mColorMatrix");
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, pColorMatrix, 4);

  //  // Set PS default params
  //  Vec4 pParams= Vec4(CRenderer::CV_r_pp_grainamount, CRenderer::CV_r_pp_grainthreshold, 
  //                     fGlobalSharpening, (CRenderer::CV_r_pp_gamma>0)?1.0f/CRenderer::CV_r_pp_gamma: 0.0f);       
  //  CShaderMan::m_shPostEffects->FXSetPSFloat("renderModeParamsPS", &pParams, 1);      
  //  // Set VS default params 
  //  pParams= Vec4(0.0f, 0.0f, CRenderer::CV_r_pp_grainscale, (float)GetUtils().m_iFrameCounter);       
  //  CShaderMan::m_shPostEffects->FXSetVSFloat("renderModeParamsVS", &pParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);   
  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

  GetUtils().ShEndPass();   
} 

///////////////////////////////////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CGlow::Render() 
{    
  float fGlowScale = m_pScale->GetParam();

  if(fGlowScale<0.1f)  
  {
    return;
  }

  PROFILE_SHADER_START

    int nQuality = gcpRendD3D->EF_GetShaderQuality(eST_PostProcess);

  // Get current viewport
  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  // if not in hdr mode, to adaptive glow
  if( !CRenderer::CV_r_hdrrendering )
  {
    // Get scene average luminance
    GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_BackBufferScaled[0]);
    GetUtils().StretchRect(CTexture::m_Text_BackBufferScaled[0], CTexture::m_Text_BackBufferScaled[1]);
    GetUtils().StretchRect(CTexture::m_Text_BackBufferScaled[1], CTexture::m_Text_BackBufferScaled[2]);
    GetUtils().StretchRect(CTexture::m_Text_BackBufferScaled[2], CTexture::m_Text_BackBufferLum[0] );

    gcpRendD3D->FX_PushRenderTarget(0, CTexture::m_Text_BackBufferLum[1], GetUtils().m_pCurDepthSurface); 
    gcpRendD3D->SetViewport(0, 0, 1, 1);        

    static CCryName pTech0Name("SceneLuminancePass");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTech0Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

    gcpRendD3D->EF_SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);  

    GetUtils().SetTexture(CTexture::m_Text_BackBufferLum[0], 0, FILTER_LINEAR); 

    GetUtils().DrawFullScreenQuad(1, 1);  

    GetUtils().ShEndPass();   

    // Restore previous viewport
    gcpRendD3D->FX_PopRenderTarget(0);
    gcpRendD3D->SetViewport(iTempX, iTempY, iWidth, iHeight);

    // Compose glow and bloom
    GetUtils().StretchRect(CTexture::m_Text_Glow, CTexture::m_Text_BackBufferScaled[0]);

    // spawn particles into effects accumulation buffer&
    gcpRendD3D->FX_PushRenderTarget(0, CTexture::m_Text_Glow, GetUtils().m_pCurDepthSurface); 
    gcpRendD3D->SetViewport(0, 0, CTexture::m_Text_Glow->GetWidth(), CTexture::m_Text_Glow->GetHeight());        

    static CCryName pTech1Name("GlowBrightPass");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTech1Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);    

    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);  

    GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_LINEAR);  // backbuffer
    if( m_pActive->GetParam() )
      GetUtils().SetTexture(CTexture::m_Text_BackBufferScaled[0], 1, FILTER_LINEAR);  // glow
    else
      GetUtils().SetTexture(CTexture::m_Text_Black, 1, FILTER_LINEAR);  // glow

    GetUtils().SetTexture(CTexture::m_Text_BackBufferLum[1], 2, FILTER_LINEAR);  // eye adjustment

    Vec4 pParams= Vec4(1.0f, 1.0f, CRenderer::CV_r_glow_fullscreen_threshold, CRenderer::CV_r_glow_fullscreen_multiplier);     
    CCryName pParamName("glowParamsPS");
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_Glow->GetWidth(), CTexture::m_Text_Glow->GetHeight());  

    GetUtils().ShEndPass();   

    // Restore previous view port
    gcpRendD3D->FX_PopRenderTarget(0);
    gcpRendD3D->SetViewport(iTempX, iTempY, iWidth, iHeight);
  }

  // Main scene glow
  // Desc: using 3 textures (2, 4, 8 times smaller), first texture is slightly blurred (to keep some detail at distance),
  // the second/third texture are quite blurred so that we get a nice/smooth looking halo around main glow

  GetUtils().TexBlurGaussian(CTexture::m_Text_Glow, 1, 1.25f, 1.5f, false);                  
  GetUtils().StretchRect(CTexture::m_Text_Glow, CTexture::m_Text_BackBufferScaled[1]);      
  GetUtils().TexBlurGaussian(CTexture::m_Text_BackBufferScaled[1], 1, 1.25f, 5.0f, false);           
  GetUtils().StretchRect(CTexture::m_Text_BackBufferScaled[1], CTexture::m_Text_BackBufferScaled[2]);                 
  GetUtils().TexBlurGaussian(CTexture::m_Text_BackBufferScaled[2], 1, 1.25f, 5.0f, false);         

  static CCryName pTechName("GlowDisplay");

  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   
  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  // Set default params    
  Vec4 pParams= Vec4(1.0f, 1.0f, 1.0f, fGlowScale);     
  static CCryName pParamName("glowParamsPS");
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);    
  GetUtils().SetTexture(CTexture::m_Text_Glow, 1);    
  GetUtils().SetTexture(CTexture::m_Text_BackBufferScaled[1], 2);    
  GetUtils().SetTexture(CTexture::m_Text_BackBufferScaled[2], 3);    

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());  

  GetUtils().ShEndPass();   

  // Disable glow post processing again
  m_pActive->SetParam(0.0f);  

  gcpRendD3D->FX_Flush();
  PROFILE_SHADER_END    
} 

///////////////////////////////////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CSunShafts::Render() 
{ 
  PROFILE_SHADER_START

    float fShaftsType = m_pShaftsType->GetParam(); 

  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  // Use higher or lower texture resolution. The higher the more details, and more fillrate hit
  CTexture *pSunShafts = CTexture::m_Text_BackBufferScaled[1];
  if( gRenDev->m_RP.m_eQuality >= eRQ_High)
    pSunShafts = CTexture::m_Text_BackBufferScaled[0];

  /////////////////////////////////////////////
  // Create shafts mask texture

  gcpRendD3D->FX_PushRenderTarget(0, pSunShafts, GetUtils().m_pCurDepthSurface);  
  gcpRendD3D->SetViewport(0, 0, pSunShafts->GetWidth(), pSunShafts->GetHeight());        

  static CCryName pTech0Name("SunShaftsMaskGen");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTech0Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);

  // Get sample size ratio (based on empirical "best look" approach)
  float fSampleSize = ((float)CTexture::m_Text_BackBuffer->GetWidth()/(float)pSunShafts->GetWidth()) * 0.5f;

  // Set samples position
  float s1 = fSampleSize / (float) CTexture::m_Text_BackBuffer->GetWidth();  // 2.0 better results on lower res images resizing        
  float t1 = fSampleSize / (float) CTexture::m_Text_BackBuffer->GetHeight();       
  // Use rotated grid
  Vec4 pParams0=Vec4(s1*0.95f, t1*0.25f, -s1*0.25f, t1*0.96f); 
  Vec4 pParams1=Vec4(-s1*0.96f, -t1*0.25f, s1*0.25f, -t1*0.96f);  

  CCryName pParam3Name("texToTexParams0");
  CCryName pParam4Name("texToTexParams1");

  CShaderMan::m_shPostEffects->FXSetVSFloat(pParam3Name, &pParams0, 1);        
  CShaderMan::m_shPostEffects->FXSetVSFloat(pParam4Name, &pParams1, 1); 

  GetUtils().SetTexture(CTexture::m_Text_ZTarget, 0, FILTER_POINT);
  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 1, FILTER_LINEAR);
  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   
  GetUtils().DrawFullScreenQuad(pSunShafts->GetWidth(), pSunShafts->GetHeight());  

  GetUtils().ShEndPass();   

  // Restore previous viewport
  gcpRendD3D->FX_PopRenderTarget(0);
  gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);        

  /////////////////////////////////////////////
  // Apply local radial blur to shafts mask

  gcpRendD3D->SetViewport(0, 0, pSunShafts->GetWidth(), pSunShafts->GetHeight());        

  static CCryName pTech1Name("SunShaftsGen");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTech1Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);     

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);

  static CCryName pParam0Name("SunShafts_ViewProj");
  CShaderMan::m_shPostEffects->FXSetVSFloat(pParam0Name, (Vec4 *) GetUtils().m_pViewProj.GetData(), 4);   
  Vec3 pSunPos = gEnv->p3DEngine->GetSunDir()*1000.0f;

  Vec4 pParams= Vec4(pSunPos.x, pSunPos.y, pSunPos.z, 1.0f);       
  static CCryName pParam1Name("SunShafts_SunPos");
  CShaderMan::m_shPostEffects->FXSetVSFloat(pParam1Name, &pParams, 1);

  Vec4 pShaftParams(0,0,0,0);
  static CCryName pParam2Name("PI_sunShaftsParams");

  // big radius, project until end of screen
  pShaftParams.x = 0.1f;    
  pShaftParams.y = clamp_tpl<float>(m_pRaysAttenuation->GetParam(), 0.0f, 10.0f);  

  CShaderMan::m_shPostEffects->FXSetPSFloat(pParam2Name, &pShaftParams, 1);  
  GetUtils().SetTexture(pSunShafts, 0, FILTER_LINEAR);     
  GetUtils().DrawFullScreenQuad(pSunShafts->GetWidth(), pSunShafts->GetHeight());  
  GetUtils().CopyScreenToTexture(pSunShafts);    // 8 samples

  // interpolate between projections  
  pShaftParams.x = 0.025f;  
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParam2Name, &pShaftParams, 1);
  GetUtils().SetTexture(pSunShafts, 0, FILTER_LINEAR);    
  GetUtils().DrawFullScreenQuad(pSunShafts->GetWidth(), pSunShafts->GetHeight());  
  GetUtils().CopyScreenToTexture(pSunShafts);    // 64 samples

  // smooth out final result
  pShaftParams.x = 0.01f;  
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParam2Name, &pShaftParams, 1); 
  GetUtils().SetTexture(pSunShafts, 0, FILTER_LINEAR);   
  GetUtils().DrawFullScreenQuad(pSunShafts->GetWidth(), pSunShafts->GetHeight());        

  GetUtils().ShEndPass();   

  GetUtils().CopyScreenToTexture(pSunShafts);    // 512 samples

  /////////////////////////////////////////////
  // Display sun shafts

  gcpRendD3D->SetViewport(iTempX, iTempY, iWidth, iHeight);        

  static CCryName pTech2Name("SunShaftsDisplay");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTech2Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);

  pShaftParams.x = clamp_tpl<float>( m_pShaftsAmount->GetParam(), 0.0f, 1.0f);
  pShaftParams.y = clamp_tpl<float>(m_pRaysAmount->GetParam(), 0.0f, 10.0f);

  CCryName pParam5Name("sunShaftsParams");
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParam5Name, &pShaftParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);
  GetUtils().SetTexture(pSunShafts, 1, FILTER_LINEAR); 
  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());  

  GetUtils().ShEndPass();   

  gcpRendD3D->FX_Flush();
  PROFILE_SHADER_END    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// todo: handle diferent sharpening filters

void CFilterSharpening::Render()
{ 
  GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_BackBufferScaled[0]);    

  static CCryName pTechName("BlurInterpolation");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);     
  float fType = m_pType->GetParam();
  float fAmount = m_pAmount->GetParam();

  // Set PS default params
  Vec4 pParams= Vec4(0, 0, 0, fAmount);
  static CCryName pParamName("psParams");
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);   
  GetUtils().SetTexture(CTexture::m_Text_BackBufferScaled[0], 1);    
  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

  GetUtils().ShEndPass();   
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// todo: handle diferent blurring filters, add wavelength based blur

void CFilterBlurring::Render()
{  
  float fType = m_pType->GetParam();
  float fAmount = m_pAmount->GetParam();
  fAmount = clamp_tpl<float>(fAmount, 0.0f, 1.0f);

  // maximum blur amount to have nice results
  const float fMaxBlurAmount = 5.0f;

  GetUtils().TexBlurGaussian(CTexture::m_Text_BackBuffer, 1, 1.0f, LERP(0.0f, fMaxBlurAmount, sqrtf(fAmount) ), false);             
  GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_BackBufferScaled[0]);    
  GetUtils().TexBlurGaussian(CTexture::m_Text_BackBufferScaled[0], 1, 1.0f, LERP(0.0f, fMaxBlurAmount, fAmount), false);             

  static CCryName pTechName("BlurInterpolation");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);     

  // Set PS default params  
  Vec4 pParams= Vec4(0, 0, 0, fAmount * fAmount);
  static CCryName pParamName("psParams");
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBufferScaled[0], 0);    
  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 1, FILTER_POINT);     
  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

  GetUtils().ShEndPass();   
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CFilterMaskedBlurring::Render()
{  
  if( m_pAmount->GetParam() <= 0.005f )
    return;

  float fAmount = m_pAmount->GetParam();
  fAmount = clamp_tpl<float>(fAmount, 0.0f, 1.0f);
  CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers

  CTexture *pMaskTex = const_cast<CTexture *> (static_cast<CParamTexture*>(m_pMaskTex)->GetParamTexture());
  if( !pMaskTex )
    pMaskTex = CTexture::m_Text_White;

  // maximum blur amount to have nice results
  const float fMaxBlurAmount = 5.0f;

  CTexture *pScreen = CTexture::m_Text_BackBuffer;
  CTexture *pScreenScaled = CTexture::m_Text_BackBufferScaled[0];

  // do it in hdr mode instead
  if( gRenDev->m_RP.m_eQuality >= eRQ_High && CRenderer::CV_r_hdrrendering) 
  {
    pScreen = CTexture::m_Text_SceneTarget;
    pScreenScaled = CTexture::m_Text_HDRTargetScaled[0];
    // need to update target
    //gcpRendD3D->FX_ScreenStretchRect(pScreen);

    // draw hdr render target into scene target - saves 1 resolve (improves performance under FSAA)
    GetUtils().StretchRect(CTexture::m_Text_HDRTarget, pScreen);    
  }

  CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers

  GetUtils().TexBlurGaussian(pScreen, 1, 1.0f, LERP(0.0f, fMaxBlurAmount, sqrtf(fAmount) ), false, pMaskTex);             
  GetUtils().StretchRect(pScreen, pScreenScaled);    
  GetUtils().TexBlurGaussian(pScreenScaled, 1, 1.0f, LERP(0.0f, fMaxBlurAmount, fAmount), false, pMaskTex);             

  CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers

  static CCryName pTechName("BlurInterpolation");
  CCryName pTechName2("MaskedBlurInterpolation");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName2, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);     

  // Set PS default params  

  static CCryName pParamName("psParams");
  static CCryName pParam1Name("TexSizeInfo");
  Vec4 pParams= Vec4(0, 0, 0, fAmount * fAmount);
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);

  int w = pMaskTex->GetWidth();
  int h = pMaskTex->GetHeight();

  pParams= Vec4(w, h, 1.0f / (float) w, 1.0f/ (float) h);
  CShaderMan::m_shPostEffects->FXSetVSFloat(pParam1Name, &pParams, 1);

  GetUtils().SetTexture(pScreenScaled, 0);    
  GetUtils().SetTexture(pScreen, 1, FILTER_POINT);     
  GetUtils().SetTexture(pMaskTex, 2);     

  GetUtils().DrawFullScreenQuad(pScreen->GetWidth(), pScreen->GetHeight()); 

  GetUtils().ShEndPass();   
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// todo: add wavelength based blur

void CFilterRadialBlurring::Render()
{  
  // Get current viewport
  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  float fAmount = m_pAmount->GetParam();
  fAmount = clamp_tpl<float>(fAmount, 0.0f, 1.0f);

  float fScreenPosX = m_pScreenPosX->GetParam();
  fScreenPosX = clamp_tpl<float>(fScreenPosX, 0.0f, 1.0f);

  float fScreenPosY = m_pScreenPosY->GetParam();
  fScreenPosY = clamp_tpl<float>(fScreenPosY, 0.0f, 1.0f);

  float fRadius = m_pRadius->GetParam();
  fRadius = 1.0f / clamp_tpl<float>(fRadius*2.0f, 0.001f, 2.0f); 

  // Back-buffer radial blurring
  static CCryName pTech0Name("RadialBlurring");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTech0Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);     

  // Set PS default params  
  Vec4 pParams= Vec4(fScreenPosX, fScreenPosY, fRadius, 0.01f * fAmount);
  static CCryName pParam0Name("vRadialBlurParams");
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParam0Name, &pParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);     
  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

  GetUtils().ShEndPass();   

  // Update back-buffer texture
  GetUtils().CopyScreenToTexture(CTexture::m_Text_BackBuffer);    
  GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_BackBufferScaled[0]);    

  gcpRendD3D->SetViewport(0, 0, CTexture::m_Text_BackBufferScaled[0]->GetWidth(), CTexture::m_Text_BackBufferScaled[0]->GetHeight());        

  // Low-res back-buffer radial blurring
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTech0Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);     

  pParams= Vec4(fScreenPosX, fScreenPosY, fRadius, 0.02f * fAmount); // use 2x bigger blurring radius
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParam0Name, &pParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBufferScaled[0], 0);     
  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

  GetUtils().CopyScreenToTexture(CTexture::m_Text_BackBufferScaled[0]);    

  GetUtils().ShEndPass();   

  gcpRendD3D->SetViewport(iTempX, iTempY, iWidth, iHeight);        

  // Blurring amount blending
  static CCryName pTech1Name("BlurInterpolation");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTech1Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);     

  // Set PS default params  
  pParams= Vec4(0, 0, 0, fAmount * fAmount);
  static CCryName pParam1Name("psParams");
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParam1Name, &pParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBufferScaled[0], 0);    
  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 1, FILTER_POINT);     
  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

  GetUtils().ShEndPass();   
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CFilterDepthEnhancement::Render()
{
  float fAmount = m_pAmount->GetParam();
  fAmount = clamp_tpl<float>(fAmount, 0.0f, 1.0f);

  static CCryName pTechName("DepthEnhancement");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);     

  // Set PS default params  
  Vec4 pParams= Vec4(0, 0, 0, 1.0f);
  static CCryName pParamName("psParams");
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);     
  GetUtils().SetTexture(CTexture::m_Text_ZTarget, 1, FILTER_POINT);     
  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

  GetUtils().ShEndPass();   
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CFilterChromaShift::Render()
{
  float fAmount = m_pAmount->GetParam();
  fAmount = max(fAmount, 0.0f);

  float fUserAmount = m_pUserAmount->GetParam();
  fUserAmount = max(fUserAmount, 0.0f);

  static CCryName pTechName("ChromaShift");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);     

  // Set PS default params  
  Vec4 pParams= Vec4(1.0f, 0.5f, 0.1f, 1) * (fAmount + fUserAmount) * 0.25f;
  static CCryName pParamName("psParams");
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0);       
  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

  GetUtils().ShEndPass();   
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CFilterGrain::Render()
{
  float fAmount = m_pAmount->GetParam();
  fAmount = max(fAmount, 0.0f);

  static CCryName pTechName("GrainFilter");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);     

  // Set PS default params     
  Vec4 pParams= Vec4(rand()%1024, rand()%1024, fAmount, fAmount);
  static CCryName pParamName("psParams");
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

  GetUtils().ShEndPass();   
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CFilterStylized::Render()
{
  float fAmount = m_pAmount->GetParam();
  fAmount = max(fAmount, 0.0f);

  GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_BackBufferScaled[0]);    
  GetUtils().TexBlurGaussian(CTexture::m_Text_BackBufferScaled[0], 1, 1.0f, 1, false);             

  static CCryName pTechName("StylizedFilter");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);     

  // Set PS default params     
  Vec4 pParams= Vec4(rand()%1024, rand()%1024, fAmount, fAmount);
  static CCryName pParamName("psParams");
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName, &pParams, 1);

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

  GetUtils().ShEndPass();   
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CColorGrading::Render()
{
  uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;

  float fSharpenAmount = max(m_pSharpenAmount->GetParam(), 0.0f);
  if( fSharpenAmount && CRenderer::CV_r_colorgrading_filters )
    GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_BackBufferScaled[0]); 


  // add cvar color_grading/color_grading_levels/color_grading_selectivecolor/color_grading_filters

  // Clamp to same Photoshop min/max values
  float fMinInput = clamp_tpl<float>(m_pMinInput->GetParam(), 0.0f, 255.0f);
  float fGammaInput = clamp_tpl<float>(m_pGammaInput->GetParam(), 0.0f, 10.0f);;
  float fMaxInput = clamp_tpl<float>(m_pMaxInput->GetParam(), 0.0f, 255.0f);
  float fMinOutput = clamp_tpl<float>(m_pMinOutput->GetParam(), 0.0f, 255.0f);
  float fMaxOutput = clamp_tpl<float>(m_pMaxOutput->GetParam(), 0.0f, 255.0f);

  float fBrightness = m_pBrightness->GetParam();
  float fContrast = m_pContrast->GetParam();
  float fSaturation = m_pSaturation->GetParam() + m_pSaturationOffset->GetParam();
  Vec4  pFilterColor = m_pPhotoFilterColor->GetParamVec4() + m_pPhotoFilterColorOffset->GetParamVec4();
  float fFilterColorDensity = clamp_tpl<float>(m_pPhotoFilterColorDensity->GetParam() + m_pPhotoFilterColorDensityOffset->GetParam(), 0.0f, 1.0f);
  float fGrain = min(m_pGrainAmount->GetParam()+m_pGrainAmountOffset->GetParam(), 1.0f);

  Vec4 pSelectiveColor = m_pSelectiveColor->GetParamVec4();
  float fSelectiveColorCyans = clamp_tpl<float>(m_pSelectiveColorCyans->GetParam()*0.01f, -1.0f, 1.0f);
  float fSelectiveColorMagentas = clamp_tpl<float>(m_pSelectiveColorMagentas->GetParam()*0.01f, -1.0f, 1.0f);
  float fSelectiveColorYellows = clamp_tpl<float>(m_pSelectiveColorYellows->GetParam()*0.01f, -1.0f, 1.0f);
  float fSelectiveColorBlacks = clamp_tpl<float>(m_pSelectiveColorBlacks->GetParam()*0.01f, -1.0f, 1.0f);

  // Enable corresponding shader variation
  gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0]|g_HWSR_MaskBit[HWSR_SAMPLE1]|g_HWSR_MaskBit[HWSR_SAMPLE2]);

  if( CRenderer::CV_r_colorgrading_levels && (fMinInput || fGammaInput || fMaxInput || fMinOutput ||fMaxOutput) )
    gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

  if( CRenderer::CV_r_colorgrading_filters && (fFilterColorDensity || fGrain || fSharpenAmount))
    gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];

  if( CRenderer::CV_r_colorgrading_selectivecolor && (fSelectiveColorCyans || fSelectiveColorMagentas || fSelectiveColorYellows || fSelectiveColorBlacks))
    gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];

  static CCryName pTechName("ColorGrading");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);     

  // Saturation matrix
  Matrix44 pSaturationMat;      
  {
    float y=0.3086f, u=0.6094f, v=0.0820f, s=clamp_tpl<float>(fSaturation, -1.0f, 100.0f);  

    float a = (1.0f-s)*y + s;
    float b = (1.0f-s)*y;
    float c = (1.0f-s)*y;
    float d = (1.0f-s)*u;
    float e = (1.0f-s)*u + s;
    float f = (1.0f-s)*u;
    float g = (1.0f-s)*v;
    float h = (1.0f-s)*v;
    float i = (1.0f-s)*v + s;

    pSaturationMat.SetIdentity();
    pSaturationMat.SetRow(0, Vec3(a, d, g));  
    pSaturationMat.SetRow(1, Vec3(b, e, h));
    pSaturationMat.SetRow(2, Vec3(c, f, i));                                     
  }

  //  Brightness matrix
  Matrix44 pBrightMat;
  fBrightness=clamp_tpl<float>(fBrightness, 0.0f, 100.0f);    
  pBrightMat.SetIdentity();
  pBrightMat.SetRow(0, Vec3(fBrightness, 0, 0)); 
  pBrightMat.SetRow(1, Vec3(0, fBrightness, 0));
  pBrightMat.SetRow(2, Vec3(0, 0, fBrightness));            

  // Create Contrast matrix
  Matrix44 pContrastMat;
  {
    float c=clamp_tpl<float>(fContrast, -1.0f, 100.0f);  
    pContrastMat.SetIdentity();
    pContrastMat.SetRow(0, Vec3(c, 0, 0));  
    pContrastMat.SetRow(1, Vec3(0, c, 0));
    pContrastMat.SetRow(2, Vec3(0, 0, c));              
    pContrastMat.SetColumn(3, 0.5f*Vec3(1.0f-c, 1.0f-c, 1.0f-c));  
  }

  // Compose final color matrix and set fragment program constants
  Matrix44 pColorMat = pSaturationMat * (pBrightMat * pContrastMat);      

  Vec4 pColorMatrix[3]=
  {
    Vec4(pColorMat.m00, pColorMat.m01, pColorMat.m02, pColorMat.m03),
    Vec4(pColorMat.m10, pColorMat.m11, pColorMat.m12, pColorMat.m13), 
    Vec4(pColorMat.m20, pColorMat.m21, pColorMat.m22, pColorMat.m23),
  };

  // Set PS default params
  static CCryName pParamName0("ColorGradingParams0");
  static CCryName pParamName1("ColorGradingParams1");
  static CCryName pParamName2("ColorGradingParams2");
  static CCryName pParamName3("ColorGradingParams3");
  static CCryName pParamName4("ColorGradingParams4");
  static CCryName pParamMatrix("mColorGradingMatrix");

  Vec4 pParams0 = Vec4(fMinInput, fGammaInput, fMaxInput, fMinOutput);
  Vec4 pParams1 = Vec4(fMaxOutput, fGrain, rand()%1024, rand()%1024);
  Vec4 pParams2 = Vec4(pFilterColor.x, pFilterColor.y, pFilterColor.z, fFilterColorDensity);
  Vec4 pParams3 = Vec4(pSelectiveColor.x, pSelectiveColor.y, pSelectiveColor.z, fSharpenAmount + 1.0f);
  Vec4 pParams4 = Vec4(fSelectiveColorCyans, fSelectiveColorMagentas, fSelectiveColorYellows, fSelectiveColorBlacks);

  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName0, &pParams0, 1);
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName1, &pParams1, 1);
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName2, &pParams2, 1);
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName3, &pParams3, 1);
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName4, &pParams4, 1);
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamMatrix, pColorMatrix, 3);

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

  GetUtils().ShEndPass();

  gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CUnderwaterGodRays::Render()
{
  PROFILE_SHADER_START

    // Get current viewport
    int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  gcpRendD3D->Set2DMode(false, 1, 1);       

  float fAmount = m_pAmount->GetParam();
  float fWatLevel = SPostEffectsUtils::m_fWaterLevel;

  static CCryName pTechName("UnderwaterGodRays");
  static CCryName pParam0Name("PI_GodRaysParamsVS");
  static CCryName pParam1Name("PI_GodRaysParamsPS");
  static CCryName pParam2Name("PI_GodRaysSunDirVS");

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render god-rays into low-res render target for less fillrate hit

  gcpRendD3D->FX_PushRenderTarget(0,  CTexture::m_Text_BackBufferScaled[1],  GetUtils().m_pCurDepthSurface); 
  gRenDev->SetViewport(0, 0, CTexture::m_Text_BackBufferScaled[1]->GetWidth(), CTexture::m_Text_BackBufferScaled[1]->GetHeight());        

  ColorF clearColor(0, 0, 0, 0);
  gcpRendD3D->EF_ClearBuffers(FRT_CLEAR_COLOR|FRT_CLEAR_IMMEDIATE, &clearColor);

  //  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETSTATES);   
  uint nPasses;
  CShaderMan::m_shPostEffects->FXSetTechnique(pTechName);
  CShaderMan::m_shPostEffects->FXBegin(&nPasses, FEF_DONTSETSTATES);

  gcpRendD3D->SetCullMode( R_CULL_NONE );
  gcpRendD3D->EF_SetState(GS_BLSRC_ONE | GS_BLDST_ONE | GS_NODEPTHTEST);

  int nSlicesCount = 10;   
  bool bInVisarea = ((CVisArea *)gEnv->p3DEngine->GetVisAreaFromPos(gRenDev->GetRCamera().Orig)) != 0;
  Vec3 vSunDir = bInVisarea? Vec3(0.5f,0.5f,1.0f) :  -gEnv->p3DEngine->GetSunDirNormalized();

  for( int r(0); r < nSlicesCount; ++r)   
  {
    // !force updating constants per-pass!
    CShaderMan::m_shPostEffects->FXBeginPass(0);

    // Set per instance params  
    Vec4 pParams= Vec4(fWatLevel, fAmount, r, 1.0f / (float) nSlicesCount);
    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam0Name, &pParams, 1);
    Vec4 pParams2 = Vec4(vSunDir.x, vSunDir.y, vSunDir.z, 1.0f);
    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam2Name, &pParams2, 1);

    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam1Name, &pParams, 1);

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 

    CShaderMan::m_shPostEffects->FXEndPass();
  }


  CShaderMan::m_shPostEffects->FXEnd(); 

  //  GetUtils().ShEndPass(); 

  gcpRendD3D->Set2DMode(true, 1, 1);       

  // Restore previous viewport
  gcpRendD3D->FX_PopRenderTarget(0);
  gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);        

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Display god-rays

  CCryName pTechName0("UnderwaterGodRaysFinal");

  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName0, FEF_DONTSETSTATES);   
  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 
  GetUtils().ShEndPass(); 

  gcpRendD3D->FX_Flush();
  PROFILE_SHADER_END  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CVolumetricScattering::Render()
{
  PROFILE_SHADER_START

    // quickie-prototype
    //    - some ideas: add several types (cloudy/sparky/yadayada)

    // Get current viewport
    int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  gcpRendD3D->Set2DMode(false, 1, 1);       

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render god-rays into low-res render target for less fillrate hit

  gcpRendD3D->FX_PushRenderTarget(0,  CTexture::m_Text_BackBufferScaled[1],  GetUtils().m_pCurDepthSurface); 
  gRenDev->SetViewport(0, 0, CTexture::m_Text_BackBufferScaled[1]->GetWidth(), CTexture::m_Text_BackBufferScaled[1]->GetHeight());        

  ColorF clearColor(0, 0, 0, 0);
  gcpRendD3D->EF_ClearBuffers(FRT_CLEAR_COLOR|FRT_CLEAR_IMMEDIATE, &clearColor);


  float fAmount = m_pAmount->GetParam();
  float fTilling = m_pTilling->GetParam();
  float fSpeed = m_pSpeed->GetParam();
  Vec4 pColor = m_pColor->GetParamVec4();

  static CCryName pTechName("VolumetricScattering");
  uint nPasses;
  CShaderMan::m_shPostEffects->FXSetTechnique(pTechName);
  CShaderMan::m_shPostEffects->FXBegin(&nPasses, FEF_DONTSETSTATES);

  gcpRendD3D->SetCullMode( R_CULL_NONE );
  gcpRendD3D->EF_SetState(GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCCOL | GS_NODEPTHTEST);    

  int nSlicesCount = 10;   

  Vec4 pParams;
  pParams= Vec4(fTilling, fSpeed, fTilling, fSpeed);

  static CCryName pParam0Name("VolumetricScattering");
  static CCryName pParam1Name("VolumetricScatteringColor");

  static CCryName pParam2Name("PI_volScatterParamsVS");
  static CCryName pParam3Name("PI_volScatterParamsPS");

  for( int r(0); r < nSlicesCount; ++r)   
  {
    // !force updating constants per-pass! (dx10..)
    CShaderMan::m_shPostEffects->FXBeginPass(0);

    // Set PS default params  
    Vec4 pParamsPI = Vec4(1.0f, fAmount, r, 1.0f / (float) nSlicesCount);
    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam0Name, &pParams, 1);
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam1Name, &pColor, 1);    
    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam2Name, &pParamsPI, 1);
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam3Name, &pParamsPI, 1);

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBufferScaled[1]->GetWidth(), CTexture::m_Text_BackBufferScaled[1]->GetHeight()); 

    CShaderMan::m_shPostEffects->FXEndPass();
  }


  CShaderMan::m_shPostEffects->FXEnd(); 

  gcpRendD3D->Set2DMode(true, 1, 1);       

  // Restore previous viewport
  gcpRendD3D->FX_PopRenderTarget(0);
  gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);     

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Display volumetric scattering effect

  CCryName pTechName0("VolumetricScatteringFinal");

  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName0, FEF_DONTSETSTATES);   
  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 
  GetUtils().ShEndPass(); 

  gcpRendD3D->FX_Flush(); 
  PROFILE_SHADER_END  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDistantRain::Render()
{
  PROFILE_SHADER_START

    // Get current viewport
    int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  gcpRendD3D->Set2DMode(false, 1, 1);       

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render distant-rain into low-res render target for less fillrate hit

  gcpRendD3D->FX_PushRenderTarget(0,  CTexture::m_Text_BackBufferScaled[0],  GetUtils().m_pCurDepthSurface); 
  gRenDev->SetViewport(0, 0, CTexture::m_Text_BackBufferScaled[0]->GetWidth(), CTexture::m_Text_BackBufferScaled[0]->GetHeight());        

  ColorF clearColor(0, 0, 0, 0);
  gcpRendD3D->EF_ClearBuffers(FRT_CLEAR_COLOR|FRT_CLEAR_IMMEDIATE, &clearColor);

  float fAmount = m_pAmount->GetParam();
  float fSpeed = m_pSpeed->GetParam();
  float fDistance = m_pDistanceScale->GetParam();

  static CCryName pTechName("DistantRain");
  static CCryName pParam0Name("PI_RainParamsVS");
  static CCryName pParam1Name("PI_RainParamsPS");
  CCryName pParam2Name("cRainColor");

  Vec4 pColor = m_pColor->GetParamVec4();

  //  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETSTATES);   
  uint nPasses;
  CShaderMan::m_shPostEffects->FXSetTechnique(pTechName);
  CShaderMan::m_shPostEffects->FXBegin(&nPasses, FEF_DONTSETSTATES);

  gcpRendD3D->SetCullMode( R_CULL_NONE );
  gcpRendD3D->EF_SetState(GS_BLSRC_ONE | GS_BLDST_ONE |GS_NODEPTHTEST);     

  int nSlicesCount = 12;   

  // Get sample size ratio (based on empirical "best look" approach)
  float fSampleSize = ((float)CTexture::m_Text_BackBuffer->GetWidth()/(float)CTexture::m_Text_BackBufferScaled[0]->GetWidth()) * 0.5f;

  // Set samples position
  float s1 = fSampleSize / (float) CTexture::m_Text_BackBuffer->GetWidth();  // 2.0 better results on lower res images resizing        
  float t1 = fSampleSize / (float) CTexture::m_Text_BackBuffer->GetHeight();       
  // Use rotated grid
  Vec4 pParams0=Vec4(s1*0.95f, t1*0.25f, -s1*0.25f, t1*0.96f); 
  Vec4 pParams1=Vec4(-s1*0.96f, -t1*0.25f, s1*0.25f, -t1*0.96f);  

  CCryName pParam3Name("texToTexParams0");
  CCryName pParam4Name("texToTexParams1");

  for( int r(0); r < nSlicesCount; ++r)   
  {

    // !force updating constants per-pass! (dx10..)
    CShaderMan::m_shPostEffects->FXBeginPass(0);

    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam3Name, &pParams0, 1);        
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam4Name, &pParams1, 1); 
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam2Name, &pColor, 1); 

    // Set PS default params  
    Vec4 pParams= Vec4(fSpeed, fAmount, r, 1.0f / (float) nSlicesCount);
    pParams.y = fDistance;
    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam0Name, &pParams, 1);
    pParams.y = fAmount;
    CShaderMan::m_shPostEffects->FXSetPSFloat(pParam1Name, &pParams, 1);

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 
    CShaderMan::m_shPostEffects->FXEndPass();
  }

  CShaderMan::m_shPostEffects->FXEnd(); 
  //  GetUtils().ShEndPass(); 

  // Restore previous viewport
  gcpRendD3D->FX_PopRenderTarget(0);
  gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);     

  gcpRendD3D->Set2DMode(true, 1, 1);       
  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Display distant rain effect

  CCryName pTechName0("DistantRainFinal");

  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName0, FEF_DONTSETSTATES);   
  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight()); 
  GetUtils().ShEndPass(); 

  gcpRendD3D->FX_Flush(); 
  PROFILE_SHADER_END  

}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Game/Hud specific post-processing
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CViewMode::Render()
{
  static CCryName pTechName("ViewMode_Noisy");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  Vec4 vParams=Vec4(1, 1, 1, (float)GetUtils().m_iFrameCounter);    
  static CCryName pParamName("viewModeParams");
  CShaderMan::m_shPostEffectsGame->FXSetVSFloat(pParamName, &vParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);   
  GetUtils().SetTexture(CTexture::m_Text_NoiseBump2DMap, 1, FILTER_LINEAR);   
  GetUtils().SetTexture(CTexture::m_Text_Noise2DMap, 2, FILTER_POINT);   


  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

  GetUtils().ShEndPass();   
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CAlienInterference::Render()
{
  float fAmount = m_pAmount->GetParam();

  static CCryName pTechName("AlienInterference");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  Vec4 vParams=Vec4(1, 1, (float)GetUtils().m_iFrameCounter, fAmount);    
  static CCryName pParamName("psParams");
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

  GetUtils().ShEndPass();   

}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterDroplets::Render()
{
  //  float fDropletsAmount = m_pAmount->GetParam();

  static CCryName pTechName("WaterDroplets");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  float fUserAmount = m_pAmount->GetParam();

  float fAtten = clamp_tpl<float>(fabs( gRenDev->GetRCamera().Orig.z - SPostEffectsUtils::m_fWaterLevel ), 0.0f, 1.0f);
  Vec4 vParams=Vec4(1, 1, 1, min(fUserAmount + (1.0f - clamp_tpl<float>(m_fCurrLifeTime, 0.0f, 1.0f)), 1.0f) * fAtten);    
  static CCryName pParamName("waterDropletsParams");
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

  GetUtils().ShEndPass();   
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterFlow::Render()
{
  float fAmount = m_pAmount->GetParam();

  static CCryName pTechName("WaterFlow");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  Vec4 vParams=Vec4(1, 1, 1, fAmount);    
  static CCryName pParamName("waterFlowParams");
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

  GetUtils().ShEndPass();   
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterPuddles::Render()
{
  m_nCurrPuddleID++;
  m_nCurrPuddleID = m_nCurrPuddleID &1;
  CTexture *pPrevPuddle = CTexture::m_Text_WaterPuddles[!m_nCurrPuddleID];
  CTexture *pCurrPuddle = CTexture::m_Text_WaterPuddles[m_nCurrPuddleID];

  // Get current viewport
  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  float fCurrentTime = gEnv->pTimer->GetCurrTime();
  float fTimeDif = fCurrentTime - m_fLastSpawnTime;
  Vec4 vParams = Vec4(0.0f, 0.0f, 0.0f, 0.0f);    
  if( fTimeDif > 0.125f )
  {
    m_fLastSpawnTime = fCurrentTime;
    vParams=Vec4(GetUtils().randf(), GetUtils().randf(), GetUtils().randf(), 1);//GetUtils().randf()>0.25f);    
  }

  {
    // spawn particles into effects accumulation buffer
    gcpRendD3D->FX_PushRenderTarget(0, CTexture::m_Text_WaterPuddlesDDN, GetUtils().m_pCurDepthSurface); 
    gcpRendD3D->SetViewport(0, 0, pCurrPuddle->GetWidth(), pCurrPuddle->GetHeight()); 

    ColorF clearColor(0, 0, 0, 0);
    //gcpRendD3D->EF_ClearBuffers(FRT_CLEAR_COLOR|FRT_CLEAR_IMMEDIATE, &clearColor);

    // compute wave propagation
    static CCryName pTechName("WaterPuddlesGen");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETSTATES|FEF_DONTSETTEXTURES);   
    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);      

    static CCryName pParamName("waterPuddlesParams");
    CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

    CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
    GetUtils().SetTexture(pPrevPuddle, 0, FILTER_POINT, 0);   
    GetUtils().SetTexture(pCurrPuddle, 1, FILTER_POINT, 0);   
    GetUtils().DrawFullScreenQuad(pCurrPuddle->GetWidth(), pCurrPuddle->GetHeight());

    GetUtils().ShEndPass(); 

    //GetUtils().CopyScreenToTexture(pCurrPuddle);     

    gcpRendD3D->FX_PopRenderTarget(0);    

    // Update current puddle
    GetUtils().StretchRect(CTexture::m_Text_WaterPuddlesDDN, pCurrPuddle);     

    gcpRendD3D->SetViewport(0, 0, iWidth, iHeight);        
  }

  // make final normal map
  gcpRendD3D->FX_PushRenderTarget(0, CTexture::m_Text_WaterPuddlesDDN, GetUtils().m_pCurDepthSurface); 
  gcpRendD3D->SetViewport(0, 0, CTexture::m_Text_WaterPuddlesDDN->GetWidth(), CTexture::m_Text_WaterPuddlesDDN->GetHeight()); 

  static CCryName pTechName("WaterPuddlesDisplay");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETSTATES|FEF_DONTSETTEXTURES);   
  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  static CCryName pParamName("waterPuddlesParams");
  vParams.w = 256.0f;
  CShaderMan::m_shPostEffects->FXSetPSFloat(pParamName, &vParams, 1);

  CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
  GetUtils().SetTexture(pCurrPuddle, 0, FILTER_LINEAR, 0);   
  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

  GetUtils().ShEndPass(); 

  gcpRendD3D->FX_PopRenderTarget(0);    
  gcpRendD3D->SetViewport(0, 0, iWidth, iHeight);      

  // disable processing
  m_pAmount->SetParam(0.0f);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterVolume::Render()
{
  {
    static int nFrameID = 0;
    static bool bInitialize = true;
    static CWater pWaterSim;

    const int nGridSize = 64;

    if( nFrameID != gRenDev->GetFrameID() )
    {
      static Vec4 pParams0(0, 0, 0, 0), pParams1(0, 0, 0, 0);
      Vec4 pCurrParams0, pCurrParams1;
      gEnv->p3DEngine->GetOceanAnimationParams(pCurrParams0, pCurrParams1);

      // Update sim settings
      if( bInitialize || pCurrParams0.x != pParams0.x || pCurrParams0.y != pParams0.y ||
        pCurrParams0.z != pParams0.z || pCurrParams0.w != pParams0.w || pCurrParams1.x != pParams1.x || 
        pCurrParams1.y != pParams1.y || pCurrParams1.z != pParams1.z || pCurrParams1.w != pParams1.w )
      {
        pParams0 = pCurrParams0;
        pParams1 = pCurrParams1;
        pWaterSim.Create( 1.0, pParams0.x, pParams0.z, 1.0f, 1.0f);
        bInitialize = false;
      }

      // Create texture if required
      if( CTexture::m_Text_WaterVolumeTemp && !CTexture::m_Text_WaterVolumeTemp->GetDeviceTexture())
      {
        CTexture::m_Text_WaterVolumeTemp->Create2DTexture(64, 64, 1, 
          FT_DONT_RELEASE | FT_NOMIPS | FT_DONT_ANISO | FT_USAGE_DYNAMIC, 
          0, eTF_A32B32G32R32F, eTF_A32B32G32R32F);
        CTexture::m_Text_WaterVolumeTemp->Fill(ColorF(0, 0, 0, 0));
      }

      // Copy data..
      if( CTexture::m_Text_WaterVolumeTemp && CTexture::m_Text_WaterVolumeTemp->GetDeviceTexture() )
      {
        pWaterSim.Update(  2*0.125*iSystem->GetITimer()->GetCurrTime(), true );
        Vec4 *pDispGrid = pWaterSim.GetDisplaceGrid();

        CTexture *pTexture = CTexture::m_Text_WaterVolumeTemp;
        uint32 pitch = 4 * sizeof( float )*nGridSize; 
        uint32 width = nGridSize; 
        uint32 height = nGridSize;

#if defined (DIRECT3D9) || defined(OPENGL)
        IDirect3DTexture9* pD3DTex((IDirect3DTexture9*) pTexture->GetDeviceTexture());
        assert(pD3DTex);
        D3DLOCKED_RECT rect;
        if (SUCCEEDED(pD3DTex->LockRect(0, &rect, 0, D3DLOCK_DISCARD))) //
#else
        ID3D10Texture2D* pD3DTex((ID3D10Texture2D*) pTexture->GetDeviceTexture());
        assert(pD3DTex);
        D3D10_MAPPED_TEXTURE2D rect;
        HRESULT h = pD3DTex->Map(D3D10CalcSubresource(0, 0, 1), D3D10_MAP_WRITE_DISCARD, 0, &rect);
        if (SUCCEEDED(h))
#endif
        {
          for (uint32 h(0); h<height; ++h)
          {
            assert(pitch == 4 * sizeof(f32) * width );
            const f32* pSrc((const f32*)((size_t)pDispGrid + h * pitch));
#if defined (DIRECT3D9) || defined(OPENGL)
            float* pDst((float*)((size_t)rect.pBits + h * rect.Pitch));
#else
            float*pDst((float*)((size_t)rect.pData + h * rect.RowPitch));
#endif
            cryMemcpy(pDst, pSrc, 4 * width*sizeof(float) );        

          }
#if defined (DIRECT3D9) || defined(OPENGL)
          pD3DTex->UnlockRect(0);
#else
          pD3DTex->Unmap(0);
#endif
        }
      }

      nFrameID = gRenDev->GetFrameID();
    }
  }
  // Get current viewport
  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  // make final normal map
  gcpRendD3D->FX_PushRenderTarget(0, CTexture::m_Text_WaterVolumeDDN, GetUtils().m_pCurDepthSurface); 
  gcpRendD3D->SetViewport(0, 0, CTexture::m_Text_WaterVolumeDDN->GetWidth(), CTexture::m_Text_WaterVolumeDDN->GetHeight()); 

  static CCryName pTechName("WaterVolumesNormalGen");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES|FEF_DONTSETTEXTURES);   
  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  static CCryName pParamName("waterVolumesParams");
  Vec4 vParams = Vec4(64.0f, 64.0f, 64.0f, 64.0f);    
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_WaterVolumeTemp, 0, FILTER_LINEAR, 0);   
  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

  GetUtils().ShEndPass(); 

  gcpRendD3D->FX_PopRenderTarget(0);    
  gcpRendD3D->SetViewport(0, 0, iWidth, iHeight);      

  // disable processing
  m_pAmount->SetParam(0.0f);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CBloodSplats::Preprocess()
{
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_High, eSQ_High );
  if( !bQualityCheck )
    return false;

  float fAmount = m_pAmount->GetParam();
  float fSpawn = m_pSpawn->GetParam();

  if( fAmount > 0.005f && !(fAmount<=0.02f || (m_fSpawnTime == 0.0f && fSpawn == 0)) )
  {
    return true;
  }

  ColorF c = ColorF(1, 1, 1, 1);
  CTexture::m_Text_EffectsAccum->Fill(c);
  m_nAccumCount = 0;

  return false;
}

void CBloodSplats::Render()
{
  float fAmount = m_pAmount->GetParam();

  float fSpawn = m_pSpawn->GetParam();
  float fScale = m_pScale->GetParam();

  fScale *= 2.0;
  fScale = min(fScale, 1.0f);

  static uint nCount = 0;

  GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_BackBufferScaled[1]);   
  //GetUtils().TexBlurGaussian(CTexture::m_Text_BackBufferScaled[1], 1, 1.0f, 5.0f );              

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Generate blood splats displacement texture

  // Get current viewport
  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  if(fSpawn) 
  {
    // spawn particles into effects accumulation buffer
    gcpRendD3D->FX_PushRenderTarget(0, CTexture::m_Text_EffectsAccum, GetUtils().m_pCurDepthSurface); 
    gcpRendD3D->SetViewport(0, 0, CTexture::m_Text_EffectsAccum->GetWidth(), CTexture::m_Text_EffectsAccum->GetHeight());        

    if( !m_nAccumCount ) // clear first time using
    {
      ColorF clearColor(1, 1, 1, 1);
      gcpRendD3D->EF_ClearBuffers(FRT_CLEAR_COLOR|FRT_CLEAR_IMMEDIATE, &clearColor);
      m_nAccumCount = 1;
    }

    static CCryName pTechName("BloodSplatsGen");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);   

    gcpRendD3D->EF_SetState(GS_BLSRC_DSTCOL | GS_BLDST_ZERO | GS_NODEPTHTEST);      

    // place random sized 'blood' quads in screen
    int w = CTexture::m_Text_BackBuffer->GetWidth(), h = CTexture::m_Text_BackBuffer->GetHeight();
    for(int s(0); s<20; ++s)        
    {
      float x0 = (rand()%(w)) / (float) w;  
      float y0 = (rand()%(h)) / (float) h;

      float x1 = x0 + fScale * ((rand()%300) / (float) w);
      float y1 = y0 + fScale * ((rand()%300) / (float) h);          

      GetUtils().DrawScreenQuad(w, h, x0, y0, x1, y1);
    }

    GetUtils().ShEndPass();   

    // Restore previous view port
    gcpRendD3D->FX_PopRenderTarget(0);
    gcpRendD3D->SetViewport(iTempX, iTempY, iWidth, iHeight);

    // save spawn time and reset spawning
    m_fSpawnTime = GetUtils().m_pTimer->GetCurrTime();
    m_pSpawn->SetParam(0.0f);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Apply flow
  float fTimeDif = GetUtils().m_pTimer->GetCurrTime() - m_fSpawnTime;
  Vec4 vParams=Vec4(1, 1, min(fTimeDif, 2.0f), (fTimeDif < 0.5f)?1 : 0 );               

  static CCryName pParamName("bloodSplatsParams");

  if(fTimeDif<2.0f) 
  {
    gcpRendD3D->SetViewport(0, 0, CTexture::m_Text_EffectsAccum->GetWidth(), CTexture::m_Text_EffectsAccum->GetHeight());        

    static CCryName pTechName("BloodSplatsFlow");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);   

    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

    CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_EffectsAccum->GetWidth(), CTexture::m_Text_EffectsAccum->GetHeight());

    GetUtils().ShEndPass();   

    GetUtils().CopyScreenToTexture(CTexture::m_Text_EffectsAccum);     
    gcpRendD3D->SetViewport(iTempX, iTempY, iWidth, iHeight);
  }


  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Display splats

  float fType = m_pType->GetParam();

  if(fType == 0)
  {
    // display human blood
    static CCryName pTechName("BloodSplatsFinal");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);   

    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

    vParams=Vec4(1, 1, 1, fAmount);    
    CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

    GetUtils().ShEndPass();   
  }
  else
  {
    // display alien blood

    gcpRendD3D->SetViewport(0, 0, CTexture::m_Text_BackBufferScaled[0]->GetWidth(), CTexture::m_Text_BackBufferScaled[0]->GetHeight());        

    // Generate blood texture
    static CCryName pTech0Name("AlienBloodSplatsFinal");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTech0Name, FEF_DONTSETSTATES);   

    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

    vParams=Vec4(1, 1, 1, fAmount);    
    CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

    GetUtils().ShEndPass();   

    // Copy current blood result into a temporary texture
    GetUtils().CopyScreenToTexture(CTexture::m_Text_BackBufferScaled[0]);  
    gcpRendD3D->SetViewport(iTempX, iTempY, iWidth, iHeight);

    // Blur result
    GetUtils().StretchRect(CTexture::m_Text_BackBufferScaled[0], CTexture::m_Text_BackBufferScaled[2]);                  
    GetUtils().TexBlurGaussian(CTexture::m_Text_BackBufferScaled[2], 1, 1.25f,5.0f , false);                         

    // Display alien blood texture and add glowing
    static CCryName pTech1Name("AlienBloodSplatsGlow");
    GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTech1Name, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);   

    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

    vParams=Vec4(1, 1, 1, fAmount);    
    CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

    GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);   
    GetUtils().SetTexture(CTexture::m_Text_BackBufferScaled[0], 1);   
    GetUtils().SetTexture(CTexture::m_Text_BackBufferScaled[2], 2);    

    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

    GetUtils().ShEndPass();       
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CScreenFrost::Render()
{
  float fAmount = m_pAmount->GetParam();

  if(fAmount<=0.02f) 
  {
    m_fRandOffset = ((float) rand() / (float) RAND_MAX); 
    return;
  }

  float fCenterAmount = m_pCenterAmount->GetParam();

  GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_BackBufferScaled[1]);    

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // display frost
  static CCryName pTechName("ScreenFrost");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  static CCryName pParam0Name("screenFrostParamsVS");
  static CCryName pParam1Name("screenFrostParamsPS");

  GetUtils().ShSetParamVS(pParam0Name, Vec4(1, 1, 1, m_fRandOffset));
  GetUtils().ShSetParamPS(pParam1Name, Vec4(1, 1, fCenterAmount, fAmount));

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

  GetUtils().ShEndPass();   
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CScreenCondensation::Render()
{
  float fAmount = m_pAmount->GetParam();

  if(fAmount<=0.02f) 
  {
    m_fRandOffset = ((float) rand() / (float) RAND_MAX); 
    return;
  }

  float fCenterAmount = m_pCenterAmount->GetParam();

  GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_BackBufferScaled[1]);    
  GetUtils().TexBlurGaussian(CTexture::m_Text_BackBufferScaled[1], 1, 1.0f, 5.0f, false);                  

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // display condensation
  static CCryName pTechName("ScreenCondensation");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  static CCryName pParam0Name("vsParams");
  static CCryName pParam1Name("psParams");

  GetUtils().ShSetParamVS(pParam0Name, Vec4(1, 1, 1, m_fRandOffset));
  GetUtils().ShSetParamPS(pParam1Name, Vec4(1, 1, fCenterAmount, fAmount));

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

  GetUtils().ShEndPass();   
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CRainDrops::Preprocess()
{  
  bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality( eRQ_Medium, eSQ_Medium );
  if( !bQualityCheck )
    return false;

  static float fLastSpawnTime = 0.0f;

  if( m_pAmount->GetParam() > 0.09f || m_nAliveDrops)
  {
    fLastSpawnTime = 0.0f;
    return true;
  }

  if( fLastSpawnTime == 0.0f)
  {
    fLastSpawnTime = GetUtils().m_pTimer->GetCurrTime();
  }

  if( fabs(GetUtils().m_pTimer->GetCurrTime() - fLastSpawnTime) < 1.0f ) 
  {
    return true;
  }  

  m_pPrevView = GetUtils().m_pView;    

  SAFE_DELETE( m_pRainDrops );

  return false;
}

void CRainDrops::SpawnParticle( SRainDrop *&pParticle )
{
  static SRainDrop pNewDrop;

  static float fLastSpawnTime = 0.0f;

  float fUserSize = m_pSize->GetParam();
  float fUserSizeVar = m_pSizeVar->GetParam();

  if( Random() > 0.5f && fabs(GetUtils().m_pTimer->GetCurrTime() - fLastSpawnTime) > m_pSpawnTimeDistance->GetParam() )
  {
    pParticle->m_pPos.x = Random();
    pParticle->m_pPos.y = Random();

    pParticle->m_fLifeTime = (pNewDrop.m_fLifeTime + pNewDrop.m_fLifeTimeVar *(Random()*2.0f-1.0f) ) ;
    //pParticle->m_fSize = (pNewDrop.m_fSize + pNewDrop.m_fSizeVar * (Random()*2.0f-1.0f)) * 2.5f;    
    //pNewDrop.m_fSize + pNewDrop.m_fSizeVar
    pParticle->m_fSize = 1.0f /( 10.0f *(fUserSize  + 0.5f * (fUserSizeVar) * (Random()*2.0f-1.0f)) ); 

    pParticle->m_pPos.x -= pParticle->m_fSize  / (float)m_pRainDrops->m_pTexture->GetWidth();
    pParticle->m_pPos.y -= pParticle->m_fSize  / (float)m_pRainDrops->m_pTexture->GetHeight();

    pParticle->m_fSpawnTime = GetUtils().m_pTimer->GetCurrTime();  
    pParticle->m_fWeight = 0.0f; // default weight to force rain drop to be stopped for a while

    fLastSpawnTime = GetUtils().m_pTimer->GetCurrTime();
  }
  else
  {
    pParticle->m_fSize = 0.0f;
  }

}

void CRainDrops::UpdateParticles()
{
  SRainDropsItor pItor, pItorEnd = m_pDropsLst.end(); 

  // Store camera parameters
  Vec3 vz = gcpRendD3D->GetRCamera().Z;    // front vec
  float fDot = vz.Dot(Vec3(0, 0, -1.0f));
  float fGravity = 1.0f - fabs( fDot );

  float fCurrFrameTime = 10.0f * iSystem->GetITimer()->GetFrameTime();

  bool bAllowSpawn( m_pAmount->GetParam() > 0.005f );

  m_nAliveDrops = 0;

  for(pItor=m_pDropsLst.begin(); pItor!=pItorEnd; ++pItor )
  {
    SRainDrop *pCurr = (*pItor);

    float fCurrLifeTime = (GetUtils().m_pTimer->GetCurrTime() - pCurr->m_fSpawnTime) / pCurr->m_fLifeTime;

    //if( fDot < - 0.5 )
    {
      //pCurr->m_fLifeTime = -1.0f;
      //continue;
    }

    // particle died, spawn new 
    if( fabs(fCurrLifeTime) > 1.0f || pCurr->m_fSize < 0.01f)
    {
      if( bAllowSpawn )
      {
        SpawnParticle( pCurr );
      }
      else
      {
        pCurr->m_fSize = 0.0f; 
        continue;
      }
    }

    m_nAliveDrops++;

    // update position, etc

    // add gravity
    pCurr->m_pPos.y += m_pVelocityProj.y * ((Random() *2.0f - 1.0f)*0.6f+0.4f); 
    pCurr->m_pPos.y += fCurrFrameTime * fGravity * min( pCurr->m_fWeight, 0.5f * pCurr->m_fSize);
    // random horizontal movement and size + camera horizontal velocity    
    pCurr->m_pPos.x += m_pVelocityProj.x * ((Random() *2.0f - 1.0f)*0.6f+0.4f); 
    pCurr->m_pPos.x += fCurrFrameTime * (min(pCurr->m_fWeight, 0.25f * pCurr->m_fSize) * fGravity * ( (Random() *2.0f - 1.0f)));  

    // Increase/decrease weight randomly 
    pCurr->m_fWeight = clamp_tpl<float>(pCurr->m_fWeight + fCurrFrameTime * pCurr->m_fWeightVar * (Random()*2.0f-1.0f)*4.0f, 0.0f, 1.0f );                    
  }
}

void CRainDrops::RainDropsMapGen()
{  
  // Generate rain drops displacement texture

  // Store camera parameters
  Vec3 vx = gcpRendD3D->GetRCamera().X;    // up vec
  Vec3 vy = gcpRendD3D->GetRCamera().Y;    // right vec
  Vec3 vz = gcpRendD3D->GetRCamera().Z;    // front vec

  // Get current viewport
  float fScreenW = m_pRainDrops->m_pTexture->GetWidth();
  float fScreenH = m_pRainDrops->m_pTexture->GetHeight();

  float fInvScreenW = 1.0f / fScreenW;
  float fInvScreenH = 1.0f / fScreenH;

  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  // Store camera parameters
  float fDot = vz.Dot(Vec3(0, 0, -1.0f));
  float fGravity = 1.0f - fabs( fDot );

  gcpRendD3D->Set2DMode(false, 1, 1);       
  gcpRendD3D->Set2DMode(true, (int)fScreenW, (int)fScreenH);       

  static CCryName pParam0Name("vRainParams");

  //if( fDot >= - 0.25)
  {

    // render particles into effects rain map
    gcpRendD3D->FX_PushRenderTarget(0, m_pRainDrops->m_pTexture, GetUtils().m_pCurDepthSurface); 
    gcpRendD3D->SetViewport(0, 0, (int)fScreenW, (int)fScreenH);        

    static CCryName pTech0Name("RainDropsGen");

    GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTech0Name, FEF_DONTSETSTATES);   

    gcpRendD3D->EF_SetState(GS_BLSRC_ONE | GS_BLDST_ONE |  GS_NODEPTHTEST); //

    // override blend operation
#if defined (DIRECT3D9) || defined (OPENGL)
    gcpRendD3D->m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_MAX);
#elif defined (DIRECT3D10)
    SStateBlend bl = gcpRendD3D->m_StatesBL[gcpRendD3D->m_nCurStateBL];
    bl.Desc.BlendOp = D3D10_BLEND_OP_MAX;
    bl.Desc.BlendOpAlpha = D3D10_BLEND_OP_MAX;
    gcpRendD3D->SetBlendState(&bl);
#endif

    SRainDropsItor pItor, pItorEnd = m_pDropsLst.end(); 
    for(pItor=m_pDropsLst.begin(); pItor!=pItorEnd; ++pItor )
    {
      SRainDrop *pCurr = (*pItor);

      if( pCurr->m_fSize < 0.01f) 
      {
        continue;
      }

      // render a sprite    
      float x0 = pCurr->m_pPos.x*fScreenW;
      float y0 = pCurr->m_pPos.y*fScreenH;  

      float x1 = (pCurr->m_pPos.x + pCurr->m_fSize * (fScreenH/fScreenW)) * fScreenW ;
      float y1 = (pCurr->m_pPos.y + pCurr->m_fSize) * fScreenH ;          

      float fCurrLifeTime = (GetUtils().m_pTimer->GetCurrTime() - pCurr->m_fSpawnTime) / pCurr->m_fLifeTime;
      Vec4 vRainParams = Vec4(1.0f, 1.0f, 1.0f, 1.0f - fCurrLifeTime);                  
      CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParam0Name, &vRainParams, 1);  
      GetUtils().DrawScreenQuad(256, 256, x0, y0, x1, y1);
    }

    // restore previous blend operation
#if defined (DIRECT3D9) || defined (OPENGL)
    gcpRendD3D->m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
#elif defined (DIRECT3D10)
    bl.Desc.BlendOp = D3D10_BLEND_OP_ADD;
    bl.Desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
    gcpRendD3D->SetBlendState(&bl);
#endif

    GetUtils().ShEndPass();   

    // Restore previous view port
    gcpRendD3D->FX_PopRenderTarget(0);

    gcpRendD3D->SetViewport(iTempX, iTempY, iWidth, iHeight);
  }

  gcpRendD3D->Set2DMode(false, iWidth, iHeight);       
  gcpRendD3D->Set2DMode(true, 1, 1);       

  gcpRendD3D->SetViewport(0, 0, m_pRainDrops->m_pTexture->GetWidth(), m_pRainDrops->m_pTexture->GetHeight());

  // apply extinction
  static CCryName pTech1Name("RainDropsExtinction");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTech1Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

  float fFrameScale = 4.0f * iSystem->GetITimer()->GetFrameTime();
  Vec4 vRainNormalMapParams = Vec4(m_pVelocityProj.x * ((float) iWidth), 0, fFrameScale * fGravity, fFrameScale*1.0f + m_pVelocityProj.y * ((float) iHeight)) * 0.25f;

  static CCryName pParam1Name("vRainNormalMapParams");
  GetUtils().ShSetParamVS(pParam1Name, vRainNormalMapParams);

  vRainNormalMapParams.w = fFrameScale;
  GetUtils().ShSetParamPS(pParam0Name, vRainNormalMapParams);

  GetUtils().SetTexture(m_pRainDrops->m_pTexture, 0, FILTER_LINEAR);     

  GetUtils().DrawFullScreenQuad(m_pRainDrops->m_pTexture->GetWidth(), m_pRainDrops->m_pTexture->GetHeight());

  GetUtils().ShEndPass();   

  // update rain drops texture
  GetUtils().CopyScreenToTexture(m_pRainDrops->m_pTexture);   

  gcpRendD3D->SetViewport(iTempX, iTempY, iWidth, iHeight);
}

void CRainDrops::Render()
{
  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

  Matrix44 pCurrView( GetUtils().m_pView ), pCurrProj( GetUtils().m_pProj );    

  int nRainMapSizeW = CTexture::m_Text_BackBuffer->GetWidth()>>1;
  int nRainMapSizeH = CTexture::m_Text_BackBuffer->GetHeight()>>1;
  if( !m_pRainDrops || (m_pRainDrops->m_pTexture && (m_pRainDrops->m_pTexture->GetWidth() != nRainMapSizeW || m_pRainDrops->m_pTexture->GetHeight() != nRainMapSizeH) ) )
  {    
    SAFE_DELETE( m_pRainDrops );
    m_pRainDrops = new SDynTexture(nRainMapSizeW, nRainMapSizeW, eTF_A8R8G8B8, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "RainDropsTempRT");  
    m_pRainDrops->Update( nRainMapSizeW, nRainMapSizeH );
    ColorF c = ColorF(0.0, 0.0, 0.0, 0.0);
    m_pRainDrops->m_pTexture->Fill(c);
  }

  if( !m_pRainDrops || !m_pRainDrops->m_pTexture )
  {        
    return;
  }

  Matrix33 pLerpedView;
  pLerpedView.SetIdentity(); 

  float fCurrFrameTime = iSystem->GetITimer()->GetFrameTime();
  // scale down speed a bit
  float fAlpha = iszero (fCurrFrameTime) ? 0.0f : .0005f/( fCurrFrameTime);            

  // Interpolate matrixes and position
  pLerpedView = Matrix33(pCurrView)*(1-fAlpha) + Matrix33(m_pPrevView)*fAlpha;   

  Vec3 pLerpedPos = Vec3::CreateLerp(pCurrView.GetRow(3), m_pPrevView.GetRow(3), fAlpha);     

  // Compose final 'previous' viewProjection matrix
  Matrix44 pLView = pLerpedView;
  pLView.m30 = pLerpedPos.x;   
  pLView.m31 = pLerpedPos.y;
  pLView.m32 = pLerpedPos.z; 

  m_pViewProjPrev = pLView * pCurrProj;      
  m_pViewProjPrev.Transpose();

  // Compute camera velocity vector
  Vec3 vz = gcpRendD3D->GetRCamera().Z;    // front vec
  Vec3 vMoveDir = gcpRendD3D->GetRCamera().Orig - vz;
  Vec4 vCurrPos = Vec4(vMoveDir.x, vMoveDir.y, vMoveDir.z, 1.0f);

  Matrix44 pViewProjCurr ( GetUtils().m_pViewProj );

  Vec4 pProjCurrPos = pViewProjCurr * vCurrPos;

  pProjCurrPos.x = ( (pProjCurrPos.x + pProjCurrPos.w) * 0.5f + (1.0f / (float) iWidth) * pProjCurrPos.w) / pProjCurrPos.w;
  pProjCurrPos.y = ( (pProjCurrPos.w - pProjCurrPos.y) * 0.5f + (1.0f / (float) iHeight) * pProjCurrPos.w) / pProjCurrPos.w;

  Vec4 pProjPrevPos = m_pViewProjPrev * vCurrPos;

  pProjPrevPos.x = ( (pProjPrevPos.x + pProjPrevPos.w) * 0.5f + (1.0f / (float) iWidth) * pProjPrevPos.w) / pProjPrevPos.w;
  pProjPrevPos.y = ( (pProjPrevPos.w - pProjPrevPos.y) * 0.5f + (1.0f / (float) iHeight) * pProjPrevPos.w) / pProjPrevPos.w;

  m_pVelocityProj = Vec3(pProjCurrPos.x - pProjPrevPos.x, pProjCurrPos.y - pProjPrevPos.y, 0);

  UpdateParticles();
  RainDropsMapGen();

  // make sure to update dynamic texture if required
  if( m_pRainDrops && !m_pRainDrops->m_pTexture )
  {
    m_pRainDrops->Update( nRainMapSizeW, nRainMapSizeH );  
  }

  // display rain
  static CCryName pTechName("RainDropsFinal");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);    

  static CCryName pParam0Name("vRainNormalMapParams");
  GetUtils().ShSetParamVS(pParam0Name, Vec4(1.0f, 1.0f, 1.0f, -1.0f));
  static CCryName pParam1Name("vRainParams");
  GetUtils().ShSetParamPS(pParam1Name, Vec4(1.0f, 1.0f, 1.0f, 1.0f));

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_LINEAR);     
  GetUtils().SetTexture(m_pRainDrops->m_pTexture, 1, FILTER_LINEAR);     

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

  GetUtils().ShEndPass();   

  // store previous frame data
  m_pPrevView =  pCurrView;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CNightVision::Preprocess()
{
  m_bWasActive = IsActive();
  if ( (IsActive() || m_fActiveTime )&& m_pGradient && m_pNoise && CRenderer::CV_r_nightvision )
    return true;


  return false;
}

void CNightVision::Render()
{ 
  m_fActiveTime += ( (m_bWasActive)? 4.0f : -4.0f ) * iSystem->GetITimer()->GetFrameTime();
  m_fActiveTime = clamp_tpl<float>(m_fActiveTime, 0.0f, 1.0f);

  Vec4 vParamsPS = Vec4(1, 1, 0, m_fActiveTime);                
  Vec4 vParamsVS = Vec4(rand()%1024, rand()%1024, rand()%1024, rand()%1024 );                

  uint64 nFlagsShader_RT = gcpRendD3D->m_RP.m_FlagsShader_RT; 

  GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_BackBufferScaled[1]);    
  GetUtils().TexBlurGaussian(CTexture::m_Text_BackBufferScaled[1], 1, 1.25f, 1.5f, false);                  

  if( CRenderer::CV_r_hdrrendering )    
    gcpRendD3D->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_HDR_MODE];

  static CCryName pTechName("NightVision"); 
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  static CCryName pParamNameVS("nightVisionParamsVS");
  static CCryName pParamNamePS("nightVisionParamsPS");
  CShaderMan::m_shPostEffectsGame->FXSetVSFloat(pParamNameVS, &vParamsVS, 1);
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamNamePS, &vParamsPS, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);     
  GetUtils().SetTexture(CTexture::m_Text_BackBufferScaled[1], 1, FILTER_LINEAR);   
  GetUtils().SetTexture( m_pNoise, 2, FILTER_POINT, 0);   
  GetUtils().SetTexture( m_pGradient, 3, FILTER_LINEAR);    
  if( CRenderer::CV_r_hdrrendering )    
    GetUtils().SetTexture(CTexture::m_pCurLumTexture, 4, FILTER_POINT);   

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());
  GetUtils().ShEndPass();   

  gcpRendD3D->m_RP.m_FlagsShader_RT = nFlagsShader_RT;

  m_bWasActive = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFlashBang::Preprocess()
{

  float fActive = m_pActive->GetParam();
  if( fActive || m_fSpawnTime)
  {
    if( fActive )
      m_fSpawnTime = 0.0f;

    m_pActive->SetParam( 0.0f );

    return true;
  }


  return false;
}

void CFlashBang::Render()
{
  float fTimeDuration = m_pTime->GetParam();
  float fDifractionAmount = m_pDifractionAmount->GetParam();
  float fBlindTime = m_pBlindAmount->GetParam();

  if( !m_fSpawnTime )
  {
    m_fSpawnTime = GetUtils().m_pTimer->GetCurrTime();

    // Create temporary ghost image and capture screen
    SAFE_DELETE( m_pGhostImage );

    m_pGhostImage = new SDynTexture(CTexture::m_Text_BackBuffer->GetWidth()>>1, CTexture::m_Text_BackBuffer->GetHeight()>>1, eTF_A8R8G8B8, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "GhostImageTempRT");  
    m_pGhostImage->Update( CTexture::m_Text_BackBuffer->GetWidth()>>1, CTexture::m_Text_BackBuffer->GetHeight()>>1 );

    if( m_pGhostImage && m_pGhostImage->m_pTexture )
    {
      GetUtils().StretchRect(CTexture::m_Text_BackBuffer, m_pGhostImage->m_pTexture);        
    }
  } 

  // Update current time
  float fCurrTime = (GetUtils().m_pTimer->GetCurrTime() - m_fSpawnTime) / fTimeDuration;  

  // Effect finished
  if( fCurrTime > 1.0f )
  {
    m_fSpawnTime = 0.0f;
    m_pActive->SetParam(0.0f);

    SAFE_DELETE( m_pGhostImage );

    return;
  }  

  // make sure to update dynamic texture if required
  if( m_pGhostImage && !m_pGhostImage->m_pTexture )
  {
    m_pGhostImage->Update( CTexture::m_Text_BackBuffer->GetWidth()>>1, CTexture::m_Text_BackBuffer->GetHeight()>>1 );
  }

  if( !m_pGhostImage || !m_pGhostImage->m_pTexture )
  {        
    return;
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////    
  static CCryName pTechName("FlashBang");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETSTATES|FEF_DONTSETTEXTURES);   

  gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

  float fLuminance = 1.0  - fCurrTime; //GetUtils().InterpolateCubic(0.0f, 1.0f, 0.0f, 1.0f, fCurrTime);

  // opt: some pre-computed constants
  Vec4 vParams = Vec4( fLuminance, fLuminance * fDifractionAmount, 3.0f * fLuminance * fBlindTime, fLuminance );                
  static CCryName pParamName("vFlashBangParams");
  CShaderMan::m_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_POINT);     
  GetUtils().SetTexture(m_pGhostImage->m_pTexture, 1, FILTER_LINEAR);     

  GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

  GetUtils().ShEndPass();   
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CChameleonCloak::Render()
{
  return;

  /*
  CTexture *pDst = CTexture::m_Text_Cloak;
  // Get current viewport
  int iTempX, iTempY, iWidth, iHeight;
  gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
  bool bResample=0;

  gcpRendD3D->FX_PushRenderTarget(0, pDst, GetUtils().m_pCurDepthSurface); 
  gRenDev->SetViewport(0, 0, pDst->GetWidth(), pDst->GetHeight());        

  static CCryName pTechName("ChameleonCloakEffect");
  GetUtils().ShBeginPass(CShaderMan::m_shPostEffectsGame, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

  gRenDev->EF_SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);   

  GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_LINEAR);   
  GetUtils().DrawFullScreenQuad(pDst->GetWidth(), pDst->GetHeight());

  GetUtils().ShEndPass();

  // Restore previous viewport
  gcpRendD3D->FX_PopRenderTarget(0);
  gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);        

  // Disable chameleon post processing again
  uint8 nCurrPostEffects = PostEffectMgr().GetPostBlendEffectsFlags();
  PostEffectMgr().SetPostBlendEffectsFlags( nCurrPostEffects & ~ POST_EFFECT_CHAMELEONCLOAK );
  */
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CFillrateProfile::Render()
{  
  PROFILE_SHADER_START

    if(CRenderer::CV_r_postprocess_profile_fillrate == 1)
    {
      static CCryName TechName("FillrateProfile");
      GetUtils().ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);                    
    }

    gcpRendD3D->EF_SetState(GS_NODEPTHTEST);   

    GetUtils().SetTexture(CTexture::m_Text_BackBuffer, 0, FILTER_LINEAR);   
    GetUtils().DrawFullScreenQuad(CTexture::m_Text_BackBuffer->GetWidth(), CTexture::m_Text_BackBuffer->GetHeight());

    GetUtils().ShEndPass();                    

    gcpRendD3D->FX_Flush();
    PROFILE_SHADER_END
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CREPostProcess:: mfDraw(CShader *ef, SShaderPass *sfm)
{      
  if( !gcpRendD3D || PostEffectMgr().GetEffects().empty() || gcpRendD3D->GetPolygonMode() == R_WIREFRAME_MODE) 
  {    
    return 0;
  }

  if( !CShaderMan::m_shPostEffects || !CShaderMan::m_shPostEffectsGame )
  {
    return 0;
  }

  if( !CTexture::m_Text_BackBuffer || !CTexture::m_Text_BackBuffer->GetDeviceTexture() )
  {
    return 0;
  }

  if( !CTexture::m_Text_SceneTarget || !CTexture::m_Text_SceneTarget->GetDeviceTexture() ) 
  {
    return 0;
  }

  PostEffectMgr().Begin();   

  gcpRendD3D->EF_ApplyShaderQuality(eST_PostProcess);

  std::vector< const char *> pActiveEffects;        
  CPostEffectItor pItor, pItorEnd = PostEffectMgr().GetEffects().end();
  for(pItor = PostEffectMgr().GetEffects().begin(); pItor != pItorEnd; ++pItor)
  {
    CPostEffect *pCurrEffect = (*pItor);
    if(pCurrEffect->Preprocess())  
    {
      if( pCurrEffect->GetRenderFlags() & PSP_UPDATE_BACKBUFFER )
      {
        GetUtils().CopyScreenToTexture(CTexture::m_Text_BackBuffer); 
      }

      // Need to make list of active stuff first (some effects might get disabled after done)
      if( CRenderer::CV_r_postprocess_effects == 2 )
      {               
        if( pActiveEffects.empty() )
        {
          pActiveEffects.reserve( 32 );
        }

        pActiveEffects.push_back( pCurrEffect->GetName() );
      }

      CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
      pCurrEffect->Render();
    }
  }

  // Output active post effects
  if( CRenderer::CV_r_postprocess_effects == 2 && !pActiveEffects.empty() )
  {               
    std::vector< const char *>::iterator pNameItor;        
    std::vector< const char *>::iterator pNameEndItor = pActiveEffects.end();        

    // current output line position
    int nPosY = 5;  

    gcpRendD3D->Set2DMode(false, 1, 1);           
    SDrawTextInfo pDrawTexInfo;    

    pDrawTexInfo.color[0] = pDrawTexInfo.color[2] = 0.0f;
    pDrawTexInfo.color[1] = 1.0f;

    gcpRendD3D->Draw2dText(5, nPosY, " " , pDrawTexInfo); // hack to avoid garbage - someting broken with draw2Dtext
    gcpRendD3D->Draw2dText(5, nPosY, "Active post effects:", pDrawTexInfo);
    nPosY += 15;

    pDrawTexInfo.color[0] = pDrawTexInfo.color[1] = pDrawTexInfo.color[2] = 1.0f;

    for(pNameItor = pActiveEffects.begin(); pNameItor != pNameEndItor; ++pNameItor) 
    {
      const char *pName = (*pNameItor);
      if( pName )  
      {
        // Output active post effects
        gcpRendD3D->Draw2dText(5, nPosY, pName, pDrawTexInfo);
        nPosY += 10;
      }
    }

    pActiveEffects.clear();

    gcpRendD3D->Set2DMode(true, 1, 1);           
  }

  if( CRenderer::CV_r_debug_motionblur )
  {
    SDrawTextInfo pDrawTexInfo;    
    pDrawTexInfo.color[0] = pDrawTexInfo.color[2] = 0.0f;
    pDrawTexInfo.color[1] = 1.0f;

    gcpRendD3D->Draw2dText(5, 0, " " , pDrawTexInfo); // hack to avoid garbage - someting broken with draw2Dtext

    char pQualityInfo[256];
    sprintf( pQualityInfo, "Motion Blur: \n\nQuality: %d, Samples: %d\n", CMotionBlur::m_nQualityInfo, CMotionBlur::m_nSamplesInfo);
    gcpRendD3D->Draw2dText(5, 15, pQualityInfo, pDrawTexInfo);
    sprintf( pQualityInfo, "Samples estimation: rotation %.3f, translation %.3f", CMotionBlur::m_nRotSamplesEst, CMotionBlur::m_nTransSamplesEst);
    gcpRendD3D->Draw2dText(5, 25, pQualityInfo, pDrawTexInfo);
  }

  PostEffectMgr().End(); 

  return 1;
}
