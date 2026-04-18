#include "StdAfx.h"
#include "DriverD3D.h"
#include "I3DEngine.h"

bool CD3D9Renderer::FX_DeferredShadowPassSetup(const Matrix44& mShadowTexGen, float maskRTWidth, float maskRTHeight)
{

	//set ScreenToWorld Expansion Basis
	Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos ;
	bool bVPosSM30 = (GetFeatures() & (RFT_HW_PS30|RFT_HW_PS40))!=0;

	if(m_RenderTileInfo.nGridSizeX > 1.f && m_RenderTileInfo.nGridSizeX > 1.f)
	{
		CShadowUtils::ProjectScreenToWorldExpansionBasis(mShadowTexGen, GetCamera(), maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, bVPosSM30, &m_RenderTileInfo);
	}
	else
	{
		CShadowUtils::ProjectScreenToWorldExpansionBasis(mShadowTexGen, GetCamera(), maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, bVPosSM30, NULL);
	}


	//TOFIX: create PB components for these params
	//creating common projection matrix for depth reconstruction

	//save magnitudes separatly to inrease precision
	m_cEF.m_TempVecs[14].x = vWBasisX.GetLength();
	m_cEF.m_TempVecs[14].y = vWBasisY.GetLength();
	m_cEF.m_TempVecs[14].z = vWBasisZ.GetLength();
	m_cEF.m_TempVecs[14].w = 1.0f;

	//Vec4r normalization in doubles
	vWBasisX /= vWBasisX.GetLength();
	vWBasisY /= vWBasisY.GetLength();
	vWBasisZ /= vWBasisZ.GetLength();

	m_cEF.m_TempVecs[10].x = vWBasisX.x;
	m_cEF.m_TempVecs[10].y = vWBasisX.y;
	m_cEF.m_TempVecs[10].z = vWBasisX.z;
	m_cEF.m_TempVecs[10].w = vWBasisX.w;

	m_cEF.m_TempVecs[11].x = vWBasisY.x;
	m_cEF.m_TempVecs[11].y = vWBasisY.y;
	m_cEF.m_TempVecs[11].z = vWBasisY.z;
	m_cEF.m_TempVecs[11].w = vWBasisY.w;

	m_cEF.m_TempVecs[12].x = vWBasisZ.x;
	m_cEF.m_TempVecs[12].y = vWBasisZ.y;
	m_cEF.m_TempVecs[12].z = vWBasisZ.z;
	m_cEF.m_TempVecs[12].w = vWBasisZ.w;

	m_cEF.m_TempVecs[0].x =  vCamPos.x;
	m_cEF.m_TempVecs[0].y =  vCamPos.y;
	m_cEF.m_TempVecs[0].z =  vCamPos.z;
	m_cEF.m_TempVecs[0].w =  vCamPos.w;

	return true;
}

HRESULT GetSampleOffsetsGaussBlur5x5Bilinear(DWORD dwD3DTexWidth, DWORD dwD3DTexHeight, Vec4* avTexCoordOffset, Vec4* avSampleWeight, FLOAT fMultiplier)
{                 
  float tu = 1.0f / (float)dwD3DTexWidth ;
  float tv = 1.0f / (float)dwD3DTexHeight ;
  float totalWeight = 0.0f;
  Vec4 vWhite( 1.f, 1.f, 1.f, 1.f );
  float fWeights[5];

  int index = 0;
  for (int x=-2; x<=2; x++, index++)
  {
    fWeights[index] = GaussianDistribution((float)x, 0.f, 4);
  }

  //  compute weights for the 2x2 taps.  only 9 bilinear taps are required to sample the entire area.
  index = 0;
  for (int y=-2; y<=2; y+=2)
  {
    float tScale = (y==2)?fWeights[4] : (fWeights[y+2] + fWeights[y+3]);
    float tFrac  = fWeights[y+2] / tScale;
    float tOfs   = ((float)y + (1.f-tFrac)) * tv;
    for (int x=-2; x<=2; x+=2, index++)
    {
      float sScale = (x==2)?fWeights[4] : (fWeights[x+2] + fWeights[x+3]);
      float sFrac  = fWeights[x+2] / sScale;
      float sOfs   = ((float)x + (1.f-sFrac)) * tu;
      avTexCoordOffset[index] = Vec4(sOfs, tOfs, 0, 1);
      avSampleWeight[index]   = vWhite * sScale * tScale;
      totalWeight += sScale * tScale;
    }
  }

  for (int i=0; i<index; i++)
  {
    avSampleWeight[i] *= (fMultiplier / totalWeight);
  }

  return S_OK;
}

void CRenderer::EF_ApplyShadowQuality()
{
  SShaderProfile *pSP = &m_cEF.m_ShaderProfiles[eST_Shadow];
  m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_QUALITY] | g_HWSR_MaskBit[HWSR_QUALITY1]);
  int nQuality = (int)pSP->GetShaderQuality();
  m_RP.m_nShaderQuality = nQuality;
  switch (nQuality)
  {
  case eSQ_Medium:
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY];
    break;
  case eSQ_High:
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY1];
    break;
  case eSQ_VeryHigh:
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY];
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY1];
    break;
  }
}

void CD3D9Renderer::FX_ShadowBlur(float fShadowBluriness, SDynTexture *tpSrcTemp, CTexture *tpDst, int iShadowMode)
{
  if (m_LogFile)
    Logv(SRendItem::m_RecurseLevel, "   Blur shadow map...\n");

  uint i, nP;

  m_RP.m_FlagsStreams_Decl = 0;
  m_RP.m_FlagsStreams_Stream = 0;
  m_RP.m_FlagsPerFlush = 0;
  m_RP.m_pCurObject = m_RP.m_VisObjects[0];
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;

  m_RP.m_pPrevObject = NULL;
  m_RP.m_FrameObject++;
  EF_Scissor(false, 0, 0, 0, 0);
  SetCullMode(R_CULL_NONE);
  int nSizeX = tpDst->GetWidth();
  int nSizeY = tpDst->GetHeight();
  bool bCreateBlured = true;
  uint64 nMaskRT = m_RP.m_FlagsShader_RT;

  STexState sTexState = STexState(FILTER_LINEAR, true);

  fShadowBluriness *= (tpDst->GetWidth() / tpSrcTemp->GetWidth());

  float fVertDepth = 0.f;
  if(CV_r_SSAO_depth_range && iShadowMode == 4)
  {
    fVertDepth = CV_r_SSAO_depth_range;
    Set2DMode(true, 1, 1, 0, 1);
  }
  else
    Set2DMode(true, 1, 1);

  // setup screen aligned quad
  struct_VERTEX_FORMAT_P3F_TEX2F pScreenBlur[] =  
  {
    { Vec3(0, 0, fVertDepth), Vec2(0, 0) },
    { Vec3(0, 1, fVertDepth), Vec2(0, 1) },
    { Vec3(1, 0, fVertDepth), Vec2(1, 0) },
    { Vec3(1, 1, fVertDepth), Vec2(1, 1) },
  };     

  CTexture::BindNULLFrom(1);

  CShader *pSH = m_cEF.m_ShaderShadowBlur;
  if (!pSH)
  {
    Set2DMode(false, 1, 1);
    return;
  }

	if(iShadowMode<0)
	{
		iShadowMode = CV_r_shadowblur;

		if(CV_r_shadowblur==3)
		{
			if (m_RP.m_nPassGroupID == EFSLIST_TRANSP || !CTexture::m_Text_ZTarget)
				iShadowMode=2;  // blur transparent object in the standard way
		}
	}

  if (iShadowMode == 4 && CTexture::m_Text_ZTarget)   // used for SSAO, with depth lookup to avoid shadow leaking
  {
    CTexture *tpDepthSrc = CTexture::m_Text_ZTarget;

    tpDepthSrc->Apply(1, CTexture::GetTexState(sTexState)); 
    tpSrcTemp->Apply(0, CTexture::GetTexState(sTexState));

    uint nPasses = 0;

    FX_PushRenderTarget(0, tpDst, &m_DepthBufferOrig);

    if(fVertDepth)
    {
      ColorF clClear(1,1,1,1);
      EF_ClearBuffers(FRT_CLEAR_COLOR|FRT_CLEAR_IMMEDIATE, &clClear, 1);
    }

    static CCryName TechName("SSAO_Blur");
    pSH->FXSetTechnique(TechName);
    pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    SetCullMode(R_CULL_BACK);

    if(fVertDepth)
      EF_SetState(GS_DEPTHFUNC_GREAT);
    else
      EF_SetState(GS_NODEPTHTEST);
    EF_Commit();

    for (nP=0; nP<nPasses; nP++)
    {
      pSH->FXBeginPass(nP);

      float sW[9] = {0.2813f, 0.2137f, 0.1185f, 0.0821f, 0.0461f, 0.0262f, 0.0162f, 0.0102f, 0.0052f};

      Vec4 v;
#if defined (DIRECT3D10)
      v[0] = 0;
      v[1] = 0;
#else
      v[0] = 1.0f / (float)nSizeX;
      v[1] = 1.0f / (float)nSizeY;
#endif
      v[2] = 0;
      v[3] = 0;
      static CCryName Param1Name("PixelOffset");
      pSH->FXSetVSFloat(Param1Name, &v, 1);

      // X Blur
      v[0] = 1.0f / (float)nSizeX * fShadowBluriness;
      v[1] = 1.0f / (float)nSizeY * fShadowBluriness;
      static CCryName Param2Name("BlurOffset");
      pSH->FXSetPSFloat(Param2Name, &v, 1);

      Vec4 vWeight[9];
      for (i=0; i<9; i++)
      {
        vWeight[i].x = sW[i];
        vWeight[i].y = sW[i];
        vWeight[i].z = sW[i];
        vWeight[i].w = sW[i];
      }
      //static CCryName Param3Name("SampleWeights");
      //pSH->FXSetPSFloat(Param3Name, vWeight, 9);

      // Draw a fullscreen quad to sample the RT
      CVertexBuffer pVertexBuffer(pScreenBlur,VERTEX_FORMAT_P3F_TEX2F);
      DrawTriStrip(&pVertexBuffer, 4);  

      pSH->FXEndPass();
    }
    EF_SetState(0);
    SetTexture(0);
    pSH->FXEnd();
    FX_PopRenderTarget(0);
  }
  else if (iShadowMode == 3 && CTexture::m_Text_ZTarget)   // with depth lookup to avoid shadow leaking - m_Text_ZTarget might be 0 in wireframe mode
  {
    CTexture *tpDepthSrc = CTexture::m_Text_ZTarget;

    tpDepthSrc->Apply(1, CTexture::GetTexState(sTexState)); 

    uint nPasses = 0;

    EF_SetState(GS_NODEPTHTEST);

    FX_PushRenderTarget(0, tpDst, &m_DepthBufferOrig);
    tpSrcTemp->Apply(0, CTexture::GetTexState(sTexState));

    static CCryName TechName("ShadowBlurScreenOpaque");
    pSH->FXSetTechnique(TechName);
    pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    for (nP=0; nP<nPasses; nP++)
    {
      pSH->FXBeginPass(nP);

      float sW[9] = {0.2813f, 0.2137f, 0.1185f, 0.0821f, 0.0461f, 0.0262f, 0.0162f, 0.0102f, 0.0052f};

      Vec4 v;
    #if defined (DIRECT3D10)
      v[0] = 0;
      v[1] = 0;
    #else
      v[0] = 1.0f / (float)nSizeX;
      v[1] = 1.0f / (float)nSizeY;
    #endif
      v[2] = 0;
      v[3] = 0;
      static CCryName Param1Name("PixelOffset");
      pSH->FXSetVSFloat(Param1Name, &v, 1);

      // X Blur
      v[0] = 1.0f / (float)nSizeX * fShadowBluriness;
      v[1] = 1.0f / (float)nSizeY * fShadowBluriness;
      static CCryName Param2Name("BlurOffset");
      pSH->FXSetPSFloat(Param2Name, &v, 1);

      Vec4 vWeight[9];
      for (i=0; i<9; i++)
      {
        vWeight[i].x = sW[i];
        vWeight[i].y = sW[i];
        vWeight[i].z = sW[i];
        vWeight[i].w = sW[i];
      }
      //static CCryName Param3Name("SampleWeights");
      //pSH->FXSetPSFloat(Param3Name, vWeight, 9);

      // Draw a fullscreen quad to sample the RT
      CVertexBuffer pVertexBuffer(pScreenBlur,VERTEX_FORMAT_P3F_TEX2F);
      DrawTriStrip(&pVertexBuffer, 4);  

      pSH->FXEndPass();
    }
    SetTexture(0);
    pSH->FXEnd();
    FX_PopRenderTarget(0);
  }
  else if (iShadowMode == 1)
  {
    tpDst->Apply(0, CTexture::GetTexState(sTexState)); 
    tpSrcTemp->SetRT(0, true, &m_DepthBufferOrig);
    uint nPasses = 0;
    static CCryName TechName("ShadowBlurScreen");
    pSH->FXSetTechnique(TechName);
    pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    EF_SetState(GS_NODEPTHTEST);

    for (nP=0; nP<nPasses; nP++)
    {
      pSH->FXBeginPass(nP);

      float sW[9] = {0.2813f, 0.2137f, 0.1185f, 0.0821f, 0.0461f, 0.0262f, 0.0162f, 0.0102f, 0.0052f};

      Vec4 v;
    #if defined (DIRECT3D10)
      v[0] = 0;
      v[1] = 0;
    #else
      v[0] = 1.0f / (float)nSizeX;
      v[1] = 1.0f / (float)nSizeY;
    #endif
      v[2] = 0;
      v[3] = 0;
      static CCryName Param1Name("PixelOffset");
      pSH->FXSetVSFloat(Param1Name, &v, 1);

      // X Blur
      v[0] = 1.0f / (float)nSizeX * fShadowBluriness * 2.f;
      v[1] = 0;
      static CCryName Param2Name("BlurOffset");
      pSH->FXSetPSFloat(Param2Name, &v, 1);

      Vec4 vWeight[9];
			float fSumm = 0;
			for (i=0; i<9; i++)
				fSumm += sW[i];

      for (i=0; i<9; i++)
      {
        vWeight[i].x = sW[i]/fSumm;
        vWeight[i].y = sW[i]/fSumm;
        vWeight[i].z = sW[i]/fSumm;
        vWeight[i].w = sW[i]/fSumm;
      }
      static CCryName Param3Name("SampleWeights");
      pSH->FXSetPSFloat(Param3Name, vWeight, 9);

      // Draw a fullscreen quad to sample the RT
      {
        CVertexBuffer pVertexBuffer(pScreenBlur,VERTEX_FORMAT_P3F_TEX2F);
        DrawTriStrip(&pVertexBuffer, 4);  
      }

      FX_SetRenderTarget(0, tpDst, &m_DepthBufferOrig);

      // Y Blur
      v[0] = 0;
      v[1] = 1.0f / (float)nSizeY * fShadowBluriness * 2.f;
      pSH->FXSetPSFloat(Param2Name, &v, 1);

      tpSrcTemp->m_pTexture->Apply(0, CTexture::GetTexState(sTexState)); 

      // Draw a fullscreen quad to sample the RT
      {
        CVertexBuffer pVertexBuffer(pScreenBlur,VERTEX_FORMAT_P3F_TEX2F);
        DrawTriStrip(&pVertexBuffer, 4);  
      }
      pSH->FXEndPass();
    }
    SetTexture(0);
    pSH->FXEnd();
    FX_PopRenderTarget(0);
    EF_Commit();
  }
  else if (iShadowMode == 0 || iShadowMode == 2 )
  {

    FX_PushRenderTarget(0, tpDst, &m_DepthBufferOrig);
    tpSrcTemp->Apply(0, CTexture::GetTexState(sTexState));

    uint nPasses = 0;
    static CCryName TechName("ShadowGaussBlur5x5");
    pSH->FXSetTechnique(TechName);
    pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

    Vec4 avSampleOffsets[10];
    Vec4 avSampleWeights[10];

    EF_SetState(GS_NODEPTHTEST);

    RECT rectSrc;
    GetTextureRect(tpSrcTemp->m_pTexture, &rectSrc);
    InflateRect(&rectSrc, -1, -1);

    RECT rectDest;
    GetTextureRect(tpDst, &rectDest);
    InflateRect(&rectDest, -1, -1);

    CoordRect coords;
    GetTextureCoords(tpSrcTemp->m_pTexture, &rectSrc, tpDst, &rectDest, &coords);

    for (nP=0; nP<nPasses; nP++)
    {
      pSH->FXBeginPass(nP);

      Vec4 v;
    #if defined (DIRECT3D10)
      v[0] = 0;
      v[1] = 0;
    #else
      v[0] = 1.0f / (float)nSizeX;
      v[1] = 1.0f / (float)nSizeY;
    #endif
      v[2] = 0;
      v[3] = 0;
      static CCryName Param1Name("PixelOffset");
      pSH->FXSetVSFloat(Param1Name, &v, 1);

      uint n = 9;
      float fBluriness = CLAMP(fShadowBluriness, 0.01f, 16.0f);
      HRESULT hr = GetSampleOffsetsGaussBlur5x5Bilinear((int)(nSizeX/fBluriness), (int)(nSizeY/fBluriness), avSampleOffsets, avSampleWeights, 1.0f);
      static CCryName Param2Name("SampleOffsets");
      static CCryName Param3Name("SampleWeights");
      pSH->FXSetPSFloat(Param2Name, avSampleOffsets, n);
      pSH->FXSetPSFloat(Param3Name, avSampleWeights, n);

      // Draw a fullscreen quad to sample the RT
      ::DrawFullScreenQuad(coords);

      pSH->FXEndPass();
    }
    pSH->FXEnd();
    FX_PopRenderTarget(0);
  }
  Set2DMode(false, 1, 1);
  m_RP.m_FlagsShader_RT = nMaskRT;

  if (m_LogFile)
    Logv(SRendItem::m_RecurseLevel, "   End bluring of shadow map...\n");
}

enum EDefShadows_Passes
{
  DS_STENCIL_PASS,
  DS_SHADOW_PASS,
  DS_SHADOW_CULL_PASS,
  DS_SHADOW_FRUSTUM_CULL_PASS,
  DS_PASS_MAX
};

void CD3D9Renderer::FX_DeferredShadowPass(CDLight* pLight, int nLightInGroup, ShadowMapFrustum *pShadowFrustum, CShader *pShader, float fFinalRange, int nVertexOffset, bool bShadowPass, bool bStencilPrepass, int nLod, int nFrustNum)
{
  EF_DisableATOC();

  SetCullMode(R_CULL_BACK);

  //if(pShadowFrustum->nAffectsReceiversFrameId != GetFrameID(false))
  //  return;
  if (!CV_r_UseShadowsPool && (pShadowFrustum->pDepthTex==NULL && pShadowFrustum->pDepthTexArray==NULL ))
    return;


  //used in light pass only
  if (pShadowFrustum->bForSubSurfScattering)
    return;

  if (pShadowFrustum->pCastersList == NULL)
  {   
    return;
  }
  else if (pShadowFrustum->pCastersList->Count() == 0)
  {
    return;
  }

  EF_ApplyShadowQuality();

  //////////////////////////////////////////////////////////////////////////
  // set global shader RT flags
  //////////////////////////////////////////////////////////////////////////

  // set pass dependent RT flags
  m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[ HWSR_SAMPLE0 ] | g_HWSR_MaskBit[ HWSR_SAMPLE1 ] | g_HWSR_MaskBit[ HWSR_SAMPLE2 ] | g_HWSR_MaskBit[ HWSR_SAMPLE3 ] | 
                             g_HWSR_MaskBit[HWSR_CUBEMAP0] | g_HWSR_MaskBit[HWSR_CUBEMAP1] | g_HWSR_MaskBit[HWSR_CUBEMAP2] | g_HWSR_MaskBit[HWSR_CUBEMAP3] |
                             g_HWSR_MaskBit[ HWSR_HW_PCF_COMPARE ]  | g_HWSR_MaskBit[ HWSR_POINT_LIGHT ] |
                             g_HWSR_MaskBit[ HWSR_SHADOW_MIXED_MAP_G16R16 ] | g_HWSR_MaskBit[ HWSR_SHADOW_FILTER ] |
                             g_HWSR_MaskBit[HWSR_FSAA] | g_HWSR_MaskBit[HWSR_SHADOW_JITTERING] | g_HWSR_MaskBit[HWSR_FSAA_QUALITY]);

  //enable multi-sample rendering
  if (!(pShadowFrustum->bUseVarianceSM))
  {
    if (m_RP.m_FSAAData.Type==8)
    {
      m_RP.m_FlagsShader_RT |= (g_HWSR_MaskBit[HWSR_FSAA] | g_HWSR_MaskBit[HWSR_FSAA_QUALITY]);
    }
    else if (m_RP.m_FSAAData.Type==4)
    {
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_FSAA];
    }
    else if (m_RP.m_FSAAData.Type==2)
    {
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_FSAA_QUALITY];
    }
  }

  if( CV_r_shadow_jittering > 0.0f )
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SHADOW_JITTERING ];


  //depthMapSampler0 is used
  m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SAMPLE0 ];

  if( pShadowFrustum->bOmniDirectionalShadow && !pShadowFrustum->bUnwrapedOmniDirectional)
  {
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_CUBEMAP0];

    //FIX:: force using G16R16 for cubemaps for now
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SHADOW_MIXED_MAP_G16R16 ];
  }

  if(pShadowFrustum->bUseFilter)
  {
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SHADOW_FILTER ];
  }

  //enable depth precision shift for sun's FP shadow RTs
  if(!(pShadowFrustum->bNormalizedDepth) && !(pShadowFrustum->bHWPCFCompare))
  {
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SHADOW_MIXED_MAP_G16R16 ];
  }

  //FIX: hack to process TEXTURE ARRAYS properly
  if(pShadowFrustum->pDepthTexArray && pShadowFrustum->pDepthTexArray->m_eTF == eTF_G16R16)
  {
    m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[ HWSR_SHADOW_MIXED_MAP_G16R16 ];
  }

  if (!(pShadowFrustum->m_Flags & DLF_DIRECTIONAL))
  {
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_POINT_LIGHT ];  
  }

  //enable hw-pcf per frustum
  if (pShadowFrustum->bHWPCFCompare)
  {
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_HW_PCF_COMPARE ];
  }

	if(pShadowFrustum->bUseVarianceSM)
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_VARIANCE_SM ];
	else
		m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[ HWSR_VARIANCE_SM ];

#if defined (DIRECT3D10)
  if(CV_r_ShadowGenGS!=0 && (pLight->m_Flags & DLF_DIRECTIONAL))
  {
    //enable sampling from texture array
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_TEX_ARR_SAMPLE ];

    //-1 to disable custom resource view
    ConfigShadowTexgen( 0, pShadowFrustum, nLod - 1 ); 
  }
  else if ( pShadowFrustum->bOmniDirectionalShadow && (nFrustNum > -1) )
  {
    //enable unwraped shadow maps for omni lights
    ConfigShadowTexgen( 0, pShadowFrustum, -1, nFrustNum, pLight);
  }
  else
  {
    ConfigShadowTexgen( 0, pShadowFrustum);
  }
#else
  if ( pShadowFrustum->bOmniDirectionalShadow && (nFrustNum > -1) )
  {
    //enable unwraped shadow maps for omni lights
    ConfigShadowTexgen( 0, pShadowFrustum, -1, nFrustNum, pLight);
  }
  else
  {
    ConfigShadowTexgen( 0, pShadowFrustum);
  }
#endif

  int newState = m_RP.m_CurState;
  newState |= GS_NODEPTHTEST;
  newState &= ~GS_DEPTHWRITE;
  if(pShadowFrustum->bUseAdditiveBlending)
  {
    newState |= GS_BLSRC_ONE | GS_BLDST_ONE;

    static ICVar * p_e_shadows_clouds = iConsole->GetCVar("e_shadows_clouds");

    if (p_e_shadows_clouds->GetIVal() && m_nCloudShadowTexId > 0)
    {
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SAMPLE2 ]; //enable modulation by clouds shadow 
    }

  }
  else
    newState &= ~(GS_BLSRC_ONE | GS_BLDST_ONE);

  //////////////////////////////////////////////////////////////////////////
  //nvidia depth bound test
  //FIX: move states to the render interface
  //half and quarter shadow mask resolutions can not be used together with DBT due to RT - Depth Buffer mismatch
  if (CV_r_ShadowsDepthBoundNV && m_bDeviceSupports_NVDBT && CV_r_ShadowsMaskResolution==0 && !m_RP.m_FSAAData.Type)
  {
    if (!(pShadowFrustum->bOmniDirectionalShadow))
    {
      float zMin,zMax; 
      zMin = 0.0f; 
      //FIX:: clean bounds' calculation code in 3dengine
      //zMax = ppSMFrustumList[ nCaster ]->fMaxFrustumBound; //fFinalRange[0]; 
      zMax = fFinalRange;

    #if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
      m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_X,MAKEFOURCC('N','V','D','B')); 
      m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_Z,*(DWORD*)&zMin); 
      m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_W,*(DWORD*)&zMax);
#endif
    #elif defined (DIRECT3D10) //transparent execution without NVDB
      assert(0); 
    #endif
    }
    else
    {
      //disable depth bound test
    #if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
      m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_X,0); 
#endif
    #elif defined (DIRECT3D10)
      assert(0);  //transparent execution without NVDB
    #endif
    }
  }

  //////////////////////////////////////////////////////////////////////////
  //Stencil cull pre-pass for GSM
  //////////////////////////////////////////////////////////////////////////
  if ( bStencilPrepass && !(pShadowFrustum->bUseAdditiveBlending) )
  {
    newState |= GS_STENCIL;
    //Disable color writes
    newState |= GS_COLMASK_NONE;

    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
      STENCOP_PASS(FSS_STENCOP_REPLACE),
      nLod, 0xFFFFFFFF, 0xFFFFFFFF
    );

    EF_SetState(newState);


    pShader->FXBeginPass( DS_STENCIL_PASS );
    if (!FAILED(EF_SetVertexDeclaration( 0, VERTEX_FORMAT_P3F_TEX2F_TEX3F )))
    {
      EF_SetState( newState );
      EF_Commit();

      //FX_ZState( newState );

#if defined (DIRECT3D9) || defined(OPENGL)
      m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, nVertexOffset, 2 );
#elif defined (DIRECT3D10)
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      m_pd3dDevice->Draw(4, nVertexOffset);
#endif
      m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += 2;
      m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
      pShader->FXEndPass();
    }
  }

//////////////////////////////////////////////////////////////////////////
// Shadow Pass
//////////////////////////////////////////////////////////////////////////

#if defined (DIRECT3D9)
  //state to save depth func before D3DCMP_ALWAYS
  int prevState = 0;
#endif

  if (bShadowPass)
  {
    pShader->FXBeginPass( DS_SHADOW_PASS );

    if (!FAILED(EF_SetVertexDeclaration( 0, VERTEX_FORMAT_P3F_TEX2F_TEX3F )))
    {
  //////////////////////////////////////////////////////////////////////////
      //trick to update zcull with this ref value for multi-lod shadows
  #if defined (DIRECT3D9)
      if (nLod !=0 && m_bDeviceSupports_NVDBT)
      {
        newState |= GS_COLMASK_NONE;
        newState |= GS_NODEPTHTEST;
        newState &= ~GS_DEPTHWRITE;
        newState |= GS_STENCIL;

        if (pShadowFrustum->bOmniDirectionalShadow || pLight->m_Flags & DLF_PROJECT)
        {
          if (nLod>=0)
          {
            uint nStencilMask = 1<<(nLod - 1);
            EF_SetStencilState(
              STENC_FUNC(FSS_STENCFUNC_EQUAL) |
              STENCOP_FAIL(FSS_STENCOP_KEEP) |
              STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
              STENCOP_PASS(FSS_STENCOP_KEEP),
              0xFFFF, nStencilMask, 0xFFFFFFFF
            );
          }
          else
          {
            EF_SetStencilState(
              STENC_FUNC(FSS_STENCFUNC_EQUAL) |
              STENCOP_FAIL(FSS_STENCOP_KEEP) |
              STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
              STENCOP_PASS(FSS_STENCOP_KEEP),
              m_nStencilMaskRef, 0xFFFFFFFF, 0xFFFFFFFF);
          }

        }
        else
        {
          EF_SetStencilState(
            STENC_FUNC(FSS_STENCFUNC_EQUAL) |
            STENCOP_FAIL(FSS_STENCOP_KEEP) |
            STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
            STENCOP_PASS(FSS_STENCOP_KEEP),
            nLod, 0xFFFFFFFF, 0xFFFFFFFF
          );
        }

        EF_SetState( newState );
        EF_Commit();

        prevState = m_RP.m_CurState;
        m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);

    #if defined (DIRECT3D9) || defined(OPENGL)
        m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nVertexOffset, 2);
    #elif defined (DIRECT3D10)
        SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        m_pd3dDevice->Draw(4, nVertexOffset);
    #endif

        m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += 2;
        m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
      }
  #endif
  //////////////////////////////////////////////////////////////////////////

      //was stencil pre-pass
      if (nLod !=0)
      {

  #if defined (DIRECT3D9)
        //////////////////////////////////////////////////////////////////////////
        //related to reset trick to update zcull with this ref value for multi-lod shadows
        //otherwise some hardware-related tiled artifaces appear
        if (nLod !=0 && m_bDeviceSupports_NVDBT)
        {
          //restore previous depthtest state because current is D3DCMP_ALWAYS
          switch (prevState & GS_DEPTHFUNC_MASK)
          {
          case GS_DEPTHFUNC_EQUAL:
            m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
            break;
          case GS_DEPTHFUNC_LEQUAL:
            m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
            break;
          case GS_DEPTHFUNC_GREAT:
            m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATER);
            break;
          case GS_DEPTHFUNC_LESS:
            m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
            break;
          case GS_DEPTHFUNC_NOTEQUAL:
            m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_NOTEQUAL);
            break;
          case GS_DEPTHFUNC_GEQUAL:
            m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATEREQUAL);
            break;
          }
        }
        //////////////////////////////////////////////////////////////////////////
  #endif

        //Shadow pass states
        newState |= GS_STENCIL;
        if (pShadowFrustum->bOmniDirectionalShadow  || pLight->m_Flags & DLF_PROJECT)
        {
          //TODO:generalize stencil cull pass for GSM, omni-lights and projectors
          if (nLod>=0)
          {
            uint nStencilMask = 1<<(nLod - 1);
            EF_SetStencilState(
              STENC_FUNC(FSS_STENCFUNC_EQUAL) |
              STENCOP_FAIL(FSS_STENCOP_KEEP) |
              STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
              STENCOP_PASS(FSS_STENCOP_KEEP),
              0xFFFF, nStencilMask, 0xFFFFFFFF
              );
          }
          else
          {
            EF_SetStencilState(
              STENC_FUNC(FSS_STENCFUNC_EQUAL) |
              STENCOP_FAIL(FSS_STENCOP_KEEP) |
              STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
              STENCOP_PASS(FSS_STENCOP_KEEP),
              m_nStencilMaskRef, 0xFFFFFFFF, 0xFFFFFFFF);
          }
        }
        else
        {
          EF_SetStencilState(
            STENC_FUNC(FSS_STENCFUNC_EQUAL) |
            STENCOP_FAIL(FSS_STENCOP_KEEP) |
            STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
            STENCOP_PASS(FSS_STENCOP_KEEP),
            nLod, 0xFFFFFFFF, 0xFFFFFFFF
            );
          // newState |= GS_DEPTHFUNC_EQUAL;
        }
        
      }
      else
      {
        newState &= ~GS_STENCIL;
      }

      //Set LS colormask
      newState &= ~GS_COLMASK_NONE;
      newState |= ( ( ~( 1 << nLightInGroup ) ) << GS_COLMASK_SHIFT ) & GS_COLMASK_MASK;
      //newState |= ( ( ~( 1 << /*nCaster */(nLod-1)) ) << GS_COLMASK_SHIFT ) & GS_COLMASK_MASK;

      //FX_ZState( newState );
      EF_SetState( newState );
      EF_Commit();

      #if defined (DIRECT3D9) || defined(OPENGL)
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, nVertexOffset, 2 );
      #elif defined (DIRECT3D10)
        SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        m_pd3dDevice->Draw(4, nVertexOffset);
      #endif

      m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += 2;
      m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
      pShader->FXEndPass();
    }
  }
}


void CreateDeferredUnitFrustumMesh(int tessx, int tessy, t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
  ////////////////////////////////////////////////////////////////////////// 
  // Geometry generating
  //////////////////////////////////////////////////////////////////////////

  int32 nBaseVertexIndex = 0; 

  indBuff.clear();
  indBuff.reserve(indBuff.size() + (tessx*tessy-1)*6); //TOFIX: correct number of indices
  vertBuff.clear();
  vertBuff.reserve(nBaseVertexIndex + tessx*tessy);

  float pViewport[4] = {0.0f,0.0f,1.0f,1.0f};
  float fViewportMinZ = 0.0f;
  float fViewportMaxZ = 1.0f;


  float szx = 1.0f;
  float szy = 1.0f;

  float hsizex = szx/2;
  float hsizey = szy/2;
  float deltax = szx/(tessx-1.0f);
  float deltay = szy/(tessy-1.0f);

  struct_VERTEX_FORMAT_P3F_TEX2F vert;
  Vec3 tri_vert;
  Vec3 a;
  Vec3 vPos;



  //generate tessellation for far plane
  a.z = 1.0f;

  for(int i=0; i < tessy; i++)
  {
    for(int j=0; j < tessx; j++)
    {
      a.x = j*deltax;
      a.y = i*deltay;

      //pre-transform viewport transform vertices in static mesh
      vPos.x = (a.x - pViewport[0]) * 2.0f / pViewport[2] - 1.0f;
      vPos.y = 1.0f - ( (a.y - pViewport[1]) * 2.0f / pViewport[3] ); //flip coords for y axis
      vPos.z = (a.z - fViewportMinZ)/(fViewportMaxZ - fViewportMinZ);


      vert.xyz = vPos;
      vert.st = Vec2(1.0f, 1.0f); //valid extraction
      vertBuff.push_back(vert);

    }
  }

  //push light origin
  vert.xyz = Vec3(0,0,0);
  vert.st = Vec2(0.0f, 0.0f); //do not extract
  vertBuff.push_back(vert);

  //init indices for triangles drawing
  for(int i=0; i < tessy-1; i++)
  {
    for(int j=0; j < tessx-1; j++)
    {
      indBuff.push_back( (uint16)( i*tessx + j+1         + nBaseVertexIndex ));
      indBuff.push_back( (uint16)( i*tessx + j           + nBaseVertexIndex ));
      indBuff.push_back( (uint16)( (i+1)*tessx + (j + 1) + nBaseVertexIndex ));

      indBuff.push_back( (uint16)( (i+1)*tessx + j       + nBaseVertexIndex ));
      indBuff.push_back( (uint16)( (i+1)*tessx + (j + 1) + nBaseVertexIndex ));
      indBuff.push_back( (uint16)( i*tessx + j           + nBaseVertexIndex ));
    }
  }

  //Additional faces
  for(int j=0; j < tessx-1; j++)
  {
    indBuff.push_back( (uint16)( (tessy-1)*tessx + j+1 + nBaseVertexIndex ));
    indBuff.push_back( (uint16)( (tessy-1)*tessx + j   + nBaseVertexIndex ));
    indBuff.push_back( (uint16)( tessy*tessx           + nBaseVertexIndex )); //light origin - last vertex

    indBuff.push_back( (uint16)( tessy*tessx + nBaseVertexIndex ) ); //light origin - last vertex
    indBuff.push_back( (uint16)( j           + nBaseVertexIndex ) );
    indBuff.push_back( (uint16)( j+1         + nBaseVertexIndex ) );

  }


  for(int i=0; i < tessy-1; i++)
  {

    indBuff.push_back( (uint16)( (i+1)*tessx + nBaseVertexIndex ));
    indBuff.push_back( (uint16)( i*tessx     + nBaseVertexIndex ));
    indBuff.push_back( (uint16)( tessy*tessx + nBaseVertexIndex )); //light origin - last vertex

    indBuff.push_back( (uint16)( tessy*tessx             + nBaseVertexIndex )); //light origin - last vertex
    indBuff.push_back( (uint16)( i*tessx + (tessx-1)     + nBaseVertexIndex ));
    indBuff.push_back( (uint16)( (i+1)*tessx + (tessx-1) + nBaseVertexIndex ));

  }

}


//push rectangle mesh 
void CreateDeferredRectangleMesh(CDLight* pLight, ShadowMapFrustum* pFrustum, int nAxis, int tessx, int tessy, t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{

  //assert(pFrustum!=NULL);
  assert(pLight!=NULL);

  Vec3& vLightPos = pLight->m_Origin;
  f32 fLightRadius = pLight->m_fRadius;

  int32 pViewport[4] = {0,0,1,1};

  Matrix44 mProjection;
  Matrix44 mView;

  if(pFrustum == NULL)
  {
    //for light source
    CShadowUtils::GetCubemapFrustumForLight(pLight, nAxis, CShadowUtils::g_fOmniLightFov/*pLight->m_fLightFrustumAngle*/+3.0f,&mProjection,&mView, false); // 3.0f -  offset to make sure that frustums are intersected
  }
  else
  {
    if(!pFrustum->bOmniDirectionalShadow)
    {
      //temporarily disabled since mLightProjMatrix contains pre-multiplied matrix already
      //pmProjection = &(pFrustum->mLightProjMatrix);
      mProjection = sIdentityMatrix;
      mView = pFrustum->mLightViewMatrix;
    }
    else
    {
      //calc one of cubemap's frustums
      Matrix33 mRot = ( Matrix33(pLight->m_ObjMatrix) );
      //rotation for shadow frustums is disabled
      CShadowUtils::GetCubemapFrustum(FTYP_OMNILIGHTVOLUME, pFrustum, nAxis, &mProjection,&mView/*, &mRot*/);
    }
  }

  ////////////////////////////////////////////////////////////////////////// 
  // Geometry generating
  //////////////////////////////////////////////////////////////////////////

  //add geometry to the existing one
  int32 nBaseVertexIndex = vertBuff.size(); 

  indBuff.clear();
  indBuff.reserve(indBuff.size() + (tessx*tessy-1)*6); //TOFIX: correct number of indices
  vertBuff.clear();
  vertBuff.reserve(nBaseVertexIndex + tessx*tessy);


  float szx = 1.0f;
  float szy = 1.0f;

	float hsizex = szx/2;
	float hsizey = szy/2;
	float deltax = szx/(tessx-1);
	float deltay = szy/(tessy-1);

  struct_VERTEX_FORMAT_P3F_TEX2F vert;
  Vec3 tri_vert;
	Vec3 a;

  //generate tessellation for far plane
  a.z = 1.0f;

	for(int i=0; i < tessy; i++)
	{
		for(int j=0; j < tessx; j++)
		{
			a.x = j*deltax;
		  a.y = i*deltay;

      // A
      mathVec3UnProject(&tri_vert, &a, pViewport, &mProjection, &mView, &sIdentityMatrix, g_CpuFlags);

      //calc vertex expansion in world space coords
      Vec3 vLightDir = tri_vert - vLightPos;
      vLightDir.Normalize();
      vLightDir.SetLength(fLightRadius*1.05f);

      vert.xyz = vLightPos + vLightDir;
      vert.st = Vec2(0.0f, 0.0f);
      vertBuff.push_back(vert);
		}
	}

  //push light origin
  vert.xyz = vLightPos;
  vert.st = Vec2(0.0f, 0.0f);
  vertBuff.push_back(vert);

  //init indices for triangles drawing
  for(int i=0; i < tessy-1; i++)
	{
		for(int j=0; j < tessx-1; j++)
		{
      indBuff.push_back( (uint16)( i*tessx + j+1         + nBaseVertexIndex ));
			indBuff.push_back( (uint16)( i*tessx + j           + nBaseVertexIndex ));
			indBuff.push_back( (uint16)( (i+1)*tessx + (j + 1) + nBaseVertexIndex ));

			indBuff.push_back( (uint16)( (i+1)*tessx + j       + nBaseVertexIndex ));
			indBuff.push_back( (uint16)( (i+1)*tessx + (j + 1) + nBaseVertexIndex ));
			indBuff.push_back( (uint16)( i*tessx + j           + nBaseVertexIndex ));
		}
	}

  //Additional faces
  for(int j=0; j < tessx-1; j++)
  {
    indBuff.push_back( (uint16)( (tessy-1)*tessx + j+1 + nBaseVertexIndex ));
    indBuff.push_back( (uint16)( (tessy-1)*tessx + j   + nBaseVertexIndex ));
    indBuff.push_back( (uint16)( tessy*tessx           + nBaseVertexIndex )); //light origin - last vertex

    indBuff.push_back( (uint16)( tessy*tessx + nBaseVertexIndex ) ); //light origin - last vertex
    indBuff.push_back( (uint16)( j           + nBaseVertexIndex ) );
    indBuff.push_back( (uint16)( j+1         + nBaseVertexIndex ) );

  }


  for(int i=0; i < tessy-1; i++)
  {

    indBuff.push_back( (uint16)( (i+1)*tessx + nBaseVertexIndex ));
    indBuff.push_back( (uint16)( i*tessx     + nBaseVertexIndex ));
    indBuff.push_back( (uint16)( tessy*tessx + nBaseVertexIndex )); //light origin - last vertex

    indBuff.push_back( (uint16)( tessy*tessx             + nBaseVertexIndex )); //light origin - last vertex
    indBuff.push_back( (uint16)( i*tessx + (tessx-1)     + nBaseVertexIndex ));
    indBuff.push_back( (uint16)( (i+1)*tessx + (tessx-1) + nBaseVertexIndex ));

  }

}

bool CD3D9Renderer::CreateUnitFrustumMesh()
{
  /*CDLight unitLight;

  unitLight.m_fRadius = 1.0f;
  unitLight.m_Origin = Vec3(0.0f, 0.0f, 0.0f);
  unitLight.m_ObjMatrix.SetIdentity();
  unitLight.m_fLightFrustumAngle = 90.0f;*/

  t_arrDeferredMeshIndBuff arrDeferredInds;
  t_arrDeferredMeshVertBuff arrDeferredVerts;

  CreateDeferredUnitFrustumMesh(10, 10, arrDeferredInds, arrDeferredVerts);


  SAFE_RELEASE(m_pUnitFrustumVB);
  SAFE_RELEASE(m_pUnitFrustumIB);

  HRESULT hr = S_OK;

  //FIX: try default pools
#if defined (DIRECT3D9) || defined(OPENGL)
  hr = m_pd3dDevice->CreateVertexBuffer( arrDeferredVerts.size() * sizeof( SDeferMeshVert ), D3DUSAGE_WRITEONLY,
    0, D3DPOOL_DEFAULT, &m_pUnitFrustumVB, NULL );
  assert(SUCCEEDED(hr));
  if(FAILED(hr))
  {
    return false;
  }

  hr = m_pd3dDevice->CreateIndexBuffer( arrDeferredInds.size() * sizeof( uint16 ), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
    D3DPOOL_DEFAULT, &m_pUnitFrustumIB, NULL );
  assert(SUCCEEDED(hr));
  if(FAILED(hr))
  {
    return false;
  }

  SDeferMeshVert* pVerts = NULL;
  uint16* pInds = NULL;

  //allocate vertices
  hr = m_pUnitFrustumVB->Lock(0, arrDeferredVerts.size() * sizeof( SDeferMeshVert ), (void **) &pVerts, 0);
  assert(SUCCEEDED(hr));

  memcpy( pVerts, &arrDeferredVerts[0], arrDeferredVerts.size()*sizeof(SDeferMeshVert ) );

  hr = m_pUnitFrustumVB->Unlock();
  assert(SUCCEEDED(hr));

  //allocate indices
  hr = m_pUnitFrustumIB->Lock(0, arrDeferredInds.size() * sizeof( uint16 ), (void **) &pInds, 0);
  assert(SUCCEEDED(hr));

  memcpy( pInds, &arrDeferredInds[0], sizeof(uint16)*arrDeferredInds.size() );

  hr = m_pUnitFrustumIB->Unlock();
  assert(SUCCEEDED(hr));

#elif defined (DIRECT3D10)

  D3D10_BUFFER_DESC BufDesc;
  ZeroStruct(BufDesc);
  BufDesc.ByteWidth = arrDeferredVerts.size() * sizeof( SDeferMeshVert );
  BufDesc.Usage = D3D10_USAGE_IMMUTABLE;
  BufDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
  BufDesc.CPUAccessFlags = 0;
  BufDesc.MiscFlags = 0;

  D3D10_SUBRESOURCE_DATA SubResData;
  ZeroStruct(SubResData);
  SubResData.pSysMem = &arrDeferredVerts[0];
  SubResData.SysMemPitch = 0;
  SubResData.SysMemSlicePitch = 0;

  hr = m_pd3dDevice->CreateBuffer(&BufDesc, &SubResData, (ID3D10Buffer **)&m_pUnitFrustumVB);
  assert(SUCCEEDED(hr));

  ZeroStruct(BufDesc);
  BufDesc.ByteWidth = arrDeferredInds.size() * sizeof( uint16 );
  BufDesc.Usage = D3D10_USAGE_IMMUTABLE;
  BufDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
  BufDesc.CPUAccessFlags = 0;
  BufDesc.MiscFlags = 0;

  ZeroStruct(SubResData);
  SubResData.pSysMem = &arrDeferredInds[0];
  SubResData.SysMemPitch = 0;
  SubResData.SysMemSlicePitch = 0;

  hr = m_pd3dDevice->CreateBuffer(&BufDesc, &SubResData, &m_pUnitFrustumIB);
  assert(SUCCEEDED(hr));
#endif

  m_UnitFrustVBSize = arrDeferredVerts.size();
  m_UnitFrustIBSize = arrDeferredInds.size();

  return true;
}


void CreateDeferredLightFrustum(ShadowMapFrustum* pFrustum, int nFrustNum, t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
  
  //struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F
  struct_VERTEX_FORMAT_P3F_TEX2F vert;
  Vec3 vNDC;


  assert(pFrustum!=NULL);

  indBuff.clear();
  indBuff.reserve(36);

  vertBuff.clear();
  vertBuff.reserve(8);
  
  //first vertex for cone
  //vert.xyz = Vec3(0.0f, 0.0f, 0.0f);
  //vert.st = Vec2(0.0f, 0.0f);
  //vertBuff.push_back(vert);

  int32 pViewport[4] = {0,0,1,1};

  Matrix44* pmProjection;
  Matrix44* pmView;

  Matrix44 mProjectionCM;
  Matrix44 mViewCM;

  if(!pFrustum->bOmniDirectionalShadow)
  {
    //temporarily disabled since mLightProjMatrix contains pre-multiplied matrix already
    //pmProjection = &(pFrustum->mLightProjMatrix);
    pmProjection = &sIdentityMatrix;
    pmView = &(pFrustum->mLightViewMatrix);
  }
  else
  {
    //calc one of cubemap's frustums
    CShadowUtils::GetCubemapFrustum(FTYP_OMNILIGHTVOLUME, pFrustum, nFrustNum, &mProjectionCM,&mViewCM);

    pmProjection = &mProjectionCM;
    pmView = &mViewCM;
  }


  //Create frustum
  for (int i=0; i<8; i++ )
  {
    //Generate screen space frustum (CCW faces)
    vNDC = Vec3((i==0 || i==3 || i==4 || i==7) ? 0.0f : 1.0f,
                (i==0 || i==1 || i==4 || i==5) ? 0.0f : 1.0f,
                (i==0 || i==1 || i==2 || i==3) ? 0.0f : 1.0f
               );
    //TD: convert math functions to column ordered matrices
    mathVec3UnProject(&vert.xyz, &vNDC, pViewport, pmProjection, pmView, &sIdentityMatrix, g_CpuFlags);
    vert.st = Vec2(0.0f, 0.0f);
    vertBuff.push_back(vert);
  }

  
  //CCW faces
  static uint16 nFaces[6][4] = {{0,1,2,3},
                                {4,7,6,5},
                                {0,3,7,4},
                                {1,5,6,2},
                                {0,4,5,1},
                                {3,2,6,7}
                               };

  //init indices for triangles drawing
  for(int i=0; i < 6; i++)
  {
    indBuff.push_back( (uint16)  nFaces[i][0] );
    indBuff.push_back( (uint16)  nFaces[i][1] );
    indBuff.push_back( (uint16)  nFaces[i][2] );

    indBuff.push_back( (uint16)  nFaces[i][0] );
    indBuff.push_back( (uint16)  nFaces[i][2] );
    indBuff.push_back( (uint16)  nFaces[i][3] );
  }

}



void SphereTessR(Vec3& v0, Vec3& v1, Vec3& v2, int depth, t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
  if (depth == 0)
  {
    struct_VERTEX_FORMAT_P3F_TEX2F vert;

    int nVertCount = vertBuff.size();
    vert.st = Vec2(0.0f, 0.0f);

    vert.xyz = v0;
    vertBuff.push_back(vert);
    indBuff.push_back(nVertCount++);

    vert.xyz = v1;
    vertBuff.push_back(vert);
    indBuff.push_back(nVertCount++);

    vert.xyz = v2;
    vertBuff.push_back(vert);
    indBuff.push_back(nVertCount++);

  }
  else
  {
    Vec3 v01, v12, v02;

    v01 = (v0 + v1).GetNormalized();
    v12 = (v1 + v2).GetNormalized();
    v02 = (v0 + v2).GetNormalized();

    SphereTessR(v0,  v01, v02, depth-1, indBuff, vertBuff);
    SphereTessR(v01,  v1, v12, depth-1, indBuff, vertBuff);
    SphereTessR(v12, v02, v01, depth-1, indBuff, vertBuff);
    SphereTessR(v12,  v2, v02, depth-1, indBuff, vertBuff);
  }
}


void SphereTess(Vec3& v0, Vec3& v1, Vec3& v2, t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
  int depth;
  Vec3 w0, w1, w2;
  int i, j, k;

  struct_VERTEX_FORMAT_P3F_TEX2F vert;

  vert.st = Vec2(0.0f, 0.0f);
  int nVertCount = vertBuff.size();

  depth = 2;
  for (i = 0; i < depth; i++) 
  {
    for (j = 0; i+j < depth; j++) 
    {
      k = depth - i - j;

      {
        w0 = (i * v0 + j * v1 + k * v2) / depth;
        w1 = ((i + 1) * v0 + j * v1 + (k - 1) * v2)
          / depth;
        w2 = (i * v0 + (j + 1) * v1 + (k - 1) * v2)
          / depth;
      }

      w0.Normalize();
      w1.Normalize();
      w2.Normalize();

      vert.xyz = w1;
      vertBuff.push_back(vert);
      indBuff.push_back(nVertCount++);

      vert.xyz = w0;
      vertBuff.push_back(vert);
      indBuff.push_back(nVertCount++);

      vert.xyz = w2;
      vertBuff.push_back(vert);
      indBuff.push_back(nVertCount++);

      
    }
  }
}

#define X .525731112119133606f
#define Z .850650808352039932f

void CreateDeferredUnitSphere(/*CDLight* pLight, int depth, */t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{

  static Vec3 verts[12] =
  {
    Vec3(-X, 0, Z),
    Vec3(X, 0, Z),
    Vec3(-X, 0, -Z),
    Vec3(X, 0, -Z),
    Vec3(0, Z, X),
    Vec3(0, Z, -X),
    Vec3(0, -Z, X),
    Vec3(0, -Z, -X),
    Vec3(Z, X, 0),
    Vec3(-Z, X, 0),
    Vec3(Z, -X, 0),
    Vec3(-Z, -X, 0)
  };

  static int indices[20][3] =
  {
    {0, 4, 1},
    {0, 9, 4},
    {9, 5, 4},
    {4, 5, 8},
    {4, 8, 1},
    {8, 10, 1},
    {8, 3, 10},
    {5, 3, 8},
    {5, 2, 3},
    {2, 7, 3},
    {7, 10, 3},
    {7, 6, 10},
    {7, 11, 6},
    {11, 0, 6},
    {0, 1, 6},
    {6, 1, 10},
    {9, 0, 11},
    {9, 11, 2},
    {9, 2, 5},
    {7, 2, 11},
  };

  indBuff.clear();
  vertBuff.clear();

  struct_VERTEX_FORMAT_P3F_TEX2F vert;

  vert.st = Vec2(0.0f, 0.0f);

  //debug
  /*for(int i=0; i<12; i++)
  {
   vert.xyz = verts[i];
   vertBuff.push_back(vert);
  }

  for (int i = 19; i >= 0; i--) 
  {
    indBuff.push_back( (uint16)indices[i][2] ); 
    indBuff.push_back( (uint16)indices[i][1] ); 
    indBuff.push_back( (uint16)indices[i][0] ); 
  }*/

  for (int i = 19; i >= 0; i--) 
  {
    Vec3& v0 = verts[indices[i][2]];
    Vec3& v1 = verts[indices[i][1]];
    Vec3& v2 = verts[indices[i][0]];
    //SphereTess(v0, v1, v2, indBuff, vertBuff);
    SphereTessR(v0, v1, v2, 1, indBuff, vertBuff);
  }

}
#undef X
#undef Z

bool CD3D9Renderer::CreateUnitLightMeshes()
{

  t_arrDeferredMeshIndBuff arrDeferredInds;
  t_arrDeferredMeshVertBuff arrDeferredVerts;

  CreateDeferredUnitSphere(arrDeferredInds, arrDeferredVerts);

  SAFE_RELEASE(m_pUnitSphereVB);
  SAFE_RELEASE(m_pUnitSphereIB);

  HRESULT hr = S_OK;

  //FIX: try default pools
#if defined (DIRECT3D9) || defined(OPENGL)
  hr = m_pd3dDevice->CreateVertexBuffer( arrDeferredVerts.size() * sizeof( SDeferMeshVert ), D3DUSAGE_WRITEONLY,
    0, D3DPOOL_DEFAULT, &m_pUnitSphereVB, NULL );
  assert(SUCCEEDED(hr));
  if(FAILED(hr))
  {
    return false;
  }

  hr = m_pd3dDevice->CreateIndexBuffer( arrDeferredInds.size() * sizeof( uint16 ), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
    D3DPOOL_DEFAULT, &m_pUnitSphereIB, NULL );
  assert(SUCCEEDED(hr));
  if(FAILED(hr))
  {
    return false;
  }
#elif defined (DIRECT3D10)
  assert(0);
  /*D3D10_BUFFER_DESC BufDesc;
  ZeroStruct(BufDesc);
  BufDesc.ByteWidth = VBsize;
  BufDesc.Usage = D3D10_USAGE_DEFAULT;
  BufDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
  BufDesc.CPUAccessFlags = 0;
  BufDesc.MiscFlags = 0; //D3D10_RESOURCE_MISC_COPY_DESTINATION;
  HRESULT hReturn = m_pd3dDevice->CreateBuffer(&BufDesc, NULL, (ID3D10Buffer **)&m_pUnitFrustumVB);
  assert(SUCCEEDED(hReturn));
  */
#endif

  SDeferMeshVert* pVerts = NULL;
  uint16* pInds = NULL;

#if defined (DIRECT3D9) || defined(OPENGL)
  //allocate vertices
  hr = m_pUnitSphereVB->Lock(0, arrDeferredVerts.size() * sizeof( SDeferMeshVert ), (void **) &pVerts, 0);
  assert(SUCCEEDED(hr));

  memcpy( pVerts, &arrDeferredVerts[0], arrDeferredVerts.size()*sizeof(SDeferMeshVert ) );

  hr = m_pUnitSphereVB->Unlock();
  assert(SUCCEEDED(hr));

  //allocate indices
  hr = m_pUnitSphereIB->Lock(0, arrDeferredInds.size() * sizeof( uint16 ), (void **) &pInds, 0);
  assert(SUCCEEDED(hr));

  memcpy( pInds, &arrDeferredInds[0], sizeof(uint16)*arrDeferredInds.size() );

  hr = m_pUnitSphereIB->Unlock();
  assert(SUCCEEDED(hr));

#elif defined (DIRECT3D10)
  assert(0);
  /*byte *pData = (byte*) 0x12345678;
  hr = m_pVBTemp[m_nCurStagedVB]->Map(D3D10_MAP_WRITE, 0, (void **) &pData);
  m_StagedStream[nType] = m_nCurStagedVB++;
  if (m_nCurStagedVB > CV_d3d10_NumStagingBuffers-1)
  m_nCurStagedVB = 0;

  pVertices = &pData[0];*/
#endif

  m_UnitSphereVBSize = arrDeferredVerts.size();
  m_UnitSphereIBSize = arrDeferredInds.size();

  return true;

}


void CD3D9Renderer::FX_StencilCullPass(int nStencilID, int nVertOffs, int nNumVers, int nIndOffs, int nNumInds)
{
  int newState = m_RP.m_CurState;

  //Set LS colormask
  //debug states
  if (CV_r_LightVolumesDebug /*&& m_RP.m_PersFlags2 & RBPF2_LIGHTSTENCILCULL*/)
  {
    newState &= ~GS_COLMASK_NONE;
    newState &= ~GS_NODEPTHTEST;
    newState |= GS_DEPTHWRITE;
    newState |= ( ( ~( 0xF) ) << GS_COLMASK_SHIFT ) & GS_COLMASK_MASK;
    newState |= GS_WIREFRAME;
  }
  else
  {
    //Disable color writes
    newState |= GS_COLMASK_NONE;

    //setup depth test  
    newState &= ~GS_NODEPTHTEST;
    newState &= ~GS_DEPTHWRITE;
    newState |= GS_DEPTHFUNC_LEQUAL;
    newState |= GS_STENCIL;
  }

  //////////////////////////////////////////////////////////////////////////
  //draw back faces - inc when depth fail
  //////////////////////////////////////////////////////////////////////////
  if (nStencilID >= 0)
  {
    uint nStencilWriteMask = 1 << nStencilID; //0..7
    SetCullMode(R_CULL_FRONT);
#if defined (DIRECT3D10)
    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENC_CCW_FUNC(FSS_STENCFUNC_ALWAYS) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_CCW_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_REPLACE) | STENCOP_CCW_ZFAIL(FSS_STENCOP_REPLACE) |
      STENCOP_PASS(FSS_STENCOP_KEEP) | STENCOP_CCW_PASS(FSS_STENCOP_KEEP),
      0xFF, 0xFFFFFFFF, nStencilWriteMask
      );
#else
    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
      STENCOP_PASS(FSS_STENCOP_KEEP),
      0xFF, 0xFFFFFFFF, nStencilWriteMask
      );
#endif
  }
  else
  {
    m_nStencilMaskRef++;
    //m_nStencilMaskRef = m_nStencilMaskRef%255 + m_nStencilMaskRef/255;
    if (m_nStencilMaskRef>255)
    {
      EF_ClearBuffers(FRT_CLEAR_STENCIL|FRT_CLEAR_IMMEDIATE, NULL);
      m_nStencilMaskRef= 1;
    }
    //TD: Fill stencil by values=1 for drawn volumes in order to avoid overdraw
    uint nCurrRef = m_nStencilMaskRef;
    SetCullMode(R_CULL_FRONT);
#if defined (DIRECT3D10)
    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENC_CCW_FUNC(FSS_STENCFUNC_ALWAYS) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_CCW_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_REPLACE) | STENCOP_CCW_ZFAIL(FSS_STENCOP_REPLACE) |
      STENCOP_PASS(FSS_STENCOP_KEEP) | STENCOP_CCW_PASS(FSS_STENCOP_KEEP),
      nCurrRef, 0xFFFFFFFF, 0xFFFF
      );
#else
    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
      STENCOP_PASS(FSS_STENCOP_KEEP),
      nCurrRef, 0xFFFFFFFF, 0xFFFF
      );
#endif
  }

  EF_SetState( newState );


  EF_Commit();
#if defined (DIRECT3D9) || defined(OPENGL)
  m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, nVertOffs, 0, nNumVers, nIndOffs, nNumInds/3);
#elif defined (DIRECT3D10)
  SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_pd3dDevice->DrawIndexed(nNumInds, nIndOffs, nVertOffs);
#endif

  m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += nNumInds/3;
  m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;

  //////////////////////////////////////////////////////////////////////////
  //draw front faces - decr when depth fail
  //////////////////////////////////////////////////////////////////////////
  if (nStencilID >= 0)
  {
    uint nStencilWriteMask = 1 << nStencilID; //0..7
    SetCullMode(R_CULL_BACK);
#if defined (DIRECT3D10)
    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENC_CCW_FUNC(FSS_STENCFUNC_ALWAYS) | 
      STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_CCW_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_REPLACE) | STENCOP_CCW_ZFAIL(FSS_STENCOP_REPLACE) |
      STENCOP_PASS(FSS_STENCOP_KEEP) | STENCOP_CCW_PASS(FSS_STENCOP_KEEP),
      0x0, 0xFFFFFFFF, nStencilWriteMask
    );
#else
    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
      STENCOP_PASS(FSS_STENCOP_KEEP),
      0x0, 0xFFFFFFFF, nStencilWriteMask
      );
#endif
  }
  else
  {
    SetCullMode(R_CULL_BACK);
//TD: deferred meshed should have proper fron facing on dx10
#if defined (DIRECT3D10)
    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENC_CCW_FUNC(FSS_STENCFUNC_ALWAYS) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_CCW_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_REPLACE) | STENCOP_CCW_ZFAIL(FSS_STENCOP_REPLACE) |
      STENCOP_PASS(FSS_STENCOP_KEEP) | STENCOP_CCW_PASS(FSS_STENCOP_KEEP),
      0, 0xFFFFFFFF, 0xFFFF
      );
#else
    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
      STENCOP_PASS(FSS_STENCOP_KEEP),
      0, 0xFFFFFFFF, 0xFFFF
      );
#endif
  }


  EF_SetState( newState );
  EF_Commit();
#if defined (DIRECT3D9) || defined(OPENGL)
  m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, nVertOffs, 0, nNumVers, nIndOffs, nNumInds/3);
#elif defined (DIRECT3D10)
  SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_pd3dDevice->DrawIndexed(nNumInds, nIndOffs, nVertOffs);
#endif

  m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += nNumInds/3;
  m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;

  return;
}

void CD3D9Renderer::FX_StencilFrustumCull(int nStencilID, CDLight* pLight, ShadowMapFrustum* pFrustum, int nAxis,  CShader *pShader)
{

  Matrix44 mProjection;
  Matrix44 mView;

  assert(pLight!=NULL);

  //unprojection matrix calculation 
  if(pFrustum == NULL)
  {
    //for light source
    CShadowUtils::GetCubemapFrustumForLight(pLight, nAxis, CShadowUtils::g_fOmniLightFov/*pLight->m_fLightFrustumAngle*/+3.0f,&mProjection,&mView, false); // 3.0f -  offset to make sure that frustums are intersected
  }
  else
  {
    if(!pFrustum->bOmniDirectionalShadow)
    {
      //temporarily disabled since mLightProjMatrix contains pre-multiplied matrix already
      //pmProjection = &(pFrustum->mLightProjMatrix);
      mProjection = sIdentityMatrix;
      mView = pFrustum->mLightViewMatrix;
    }
    else
    {
      //calc one of cubemap's frustums
      Matrix33 mRot = ( Matrix33(pLight->m_ObjMatrix) );
      //rotation for shadow frustums is disabled
      CShadowUtils::GetCubemapFrustum(FTYP_OMNILIGHTVOLUME, pFrustum, nAxis, &mProjection,&mView/*, &mRot*/);
    }
  }

  //matrix concanation and inversion should be computed in doubles otherwise we have precision problems with big coords on big levels
  //which results to the incident frustum's discontinues for omni-lights
  Matrix44r mViewProj =  Matrix44r(mView) * Matrix44r(mProjection);
  Matrix44 mViewProjInv;
  //mathMatrixInverse(mViewProjInv.GetData(), mViewProj.GetData(), g_CpuFlags);
  mViewProjInv = mViewProj.GetInverted();

  gRenDev->m_TempMatrices[0][0] = GetTransposed44(mViewProjInv);

  //setup light source pos/radius
  m_cEF.m_TempVecs[5] = Vec4(pLight->m_Origin, pLight->m_fRadius * 1.1f); //increase radius slightly


  if (m_pUnitFrustumVB==NULL || m_pUnitFrustumIB==NULL)
    CreateUnitFrustumMesh();

  FX_SetVStream( 0, m_pUnitFrustumVB, 0, sizeof( struct_VERTEX_FORMAT_P3F_TEX2F ) );
  FX_SetIStream(m_pUnitFrustumIB);

  uint nPasses = 0;         
  //  shader pass
  pShader->FXBeginPass( DS_SHADOW_FRUSTUM_CULL_PASS );

  if (!FAILED(EF_SetVertexDeclaration( 0, VERTEX_FORMAT_P3F_TEX2F )))
    FX_StencilCullPass(nStencilID, 0, m_UnitFrustVBSize, 0, m_UnitFrustIBSize);

  pShader->FXEndPass();

}

void CD3D9Renderer::FX_StencilCull(int nStencilID, t_arrDeferredMeshIndBuff& arrDeferredInds, t_arrDeferredMeshVertBuff& arrDeferredVerts, CShader *pShader)
{
  PROFILE_FRAME(FX_StencilCull);

  int nVertOffs, nIndOffs;

  //CreateDeferredLightFrustum(pShadowFrustum, nSide, arrDeferredInds, arrDeferredVerts);
  //CreateDeferredRectangleMesh(pLight, pShadowFrustum, nSide, 10, 10, arrDeferredInds, arrDeferredVerts);

  //allocate vertices
  struct_VERTEX_FORMAT_P3F_TEX2F  *pVerts( (struct_VERTEX_FORMAT_P3F_TEX2F *) GetVBPtr( arrDeferredVerts.size(), nVertOffs, POOL_P3F_TEX2F) );
  memcpy( pVerts, &arrDeferredVerts[0], arrDeferredVerts.size()*sizeof(struct_VERTEX_FORMAT_P3F_TEX2F ) );
  UnlockVB( POOL_P3F_TEX2F );

  //allocate indices
  uint16 *pInds = GetIBPtr(arrDeferredInds.size(), nIndOffs);
  memcpy( pInds, &arrDeferredInds[0], sizeof(uint16)*arrDeferredInds.size() );
  UnlockIB();

  FX_SetVStream( 0, m_pVB[ POOL_P3F_TEX2F ], 0, sizeof( struct_VERTEX_FORMAT_P3F_TEX2F ) );
  FX_SetIStream(m_pIB);

  uint nPasses = 0;         
  //  shader pass
  pShader->FXBeginPass( DS_SHADOW_CULL_PASS );

  if (!FAILED(EF_SetVertexDeclaration( 0, VERTEX_FORMAT_P3F_TEX2F )))
    FX_StencilCullPass(nStencilID, nVertOffs, arrDeferredVerts.size(), nIndOffs, arrDeferredInds.size());

  pShader->FXEndPass();

}

bool CD3D9Renderer::FX_DeferredProjLights(int nGroup, bool bCheckValidMask)
{

  if (bCheckValidMask && SRendItem::m_ShadowsValidMask[SRendItem::m_RecurseLevel][nGroup] )
    return false;

  // reset render element and current render object in pipeline
  m_RP.m_pRE = 0;
  m_RP.m_pCurObject = m_RP.m_VisObjects[0];
  m_RP.m_ObjFlags = 0;
  m_RP.m_FrameObject++;


  bool bWasDrawn = false;

  Matrix44 mCurComposite = *(m_matView->GetTop()) * *(m_matProj->GetTop());
  Matrix44 mCurView = *(m_matView->GetTop()); 
  Matrix44 mCurProj = *(m_matProj->GetTop());

  Matrix44 mQuadProj;
  //init matrix for deferred quads rendering
  mathMatrixOrthoOffCenterLH( &mQuadProj , 0, 1, 0, 1, -1, 1 );

  int TempX, TempY, TempWidth, TempHeight;
  GetViewport(&TempX, &TempY, &TempWidth, &TempHeight);

  CTexture *tpTarget = NULL;
  SDynTexture *pTempBlur = NULL;

  //FX_PushRenderTarget(0, tpTarget, &m_DepthBufferOrig);
  FX_SetShadowMaskRT(nGroup, 0, tpTarget, pTempBlur);

  int maskRTWidth = tpTarget->GetWidth();
  int maskRTHeight = tpTarget->GetHeight();


  if (bCheckValidMask)
  {
    ColorF clClear(0,0,0,0);
    EF_ClearBuffers(FRT_CLEAR_COLOR | FRT_CLEAR_STENCIL, &clClear);
  }


  // loop over all lights in this light group
  for( int i = 0; i < 4; ++i )
  {
    //note: bCheckValidMask is for sharing SRendItem::m_ShadowsValidMask info with shadows
    if (bCheckValidMask && SRendItem::m_ShadowsValidMask[SRendItem::m_RecurseLevel][nGroup] & (1<<i))
      continue;

    int nLightIndex =  i + nGroup * 4 ;

    //Disabled for now since temporal blur texture can be changed for next render list
    //So we can produce unnecessary shadow generating for some cases

    //select valid light sources
    //if ( !(pGr->m_GroupLightMask & (1<<nLightIndex)) )
    //  continue;

    //FIX this unnecessary check!
    if (nLightIndex>=m_RP.m_DLights[SRendItem::m_RecurseLevel].Num())
      continue;

    // get list of shadow casters for nLightID
    CDLight* pLight( m_RP.m_DLights[ SRendItem::m_RecurseLevel ][ nLightIndex ] );

    //check for valid light to process
    if (!(pLight->m_Flags & DLF_PROJECT) || !(pLight->m_pLightImage) )
      continue;

#if defined (DIRECT3D9) || defined(OPENGL) //draw first LOD with stencil-fill enabled
    //TOFIX: add shadows stencil test variable
    //TOFIX: hack
    //m_pd3dDevice->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0);
    //EF_ClearBuffers(FRT_CLEAR_STENCIL, NULL, 1);
#endif //draw first LOD with stencil-fill enabled

    //fill light pass for projector setup
    m_RP.m_LPasses[0].nLights = 1;
    m_RP.m_LPasses[0].pLights[0] = pLight;

    //FIX: temp solution for projectors
    CDLight FakeBlackLight;
    FakeBlackLight.m_pLightImage = CTexture::m_Text_Black;
    FakeBlackLight.m_Flags |= DLF_PROJECT;

    // set shader
    CShader *pSH( CShaderMan::m_ShaderShadowMaskGen );

    int nOffs;
    uint nPasses = 0;         
    static CCryName StencilCullTechName = "DeferredShadowPass";
    static CCryName LightTechName = "DeferredLightProj";


    PROFILE_SHADER_START

    int nSides = 1;
    //TOFIX: add case for omni-lights
    //if (ppSMFrustumList[0]->bOmniDirectionalShadow)
      nSides = 6;
    m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[ HWSR_SAMPLE0 ] | g_HWSR_MaskBit[ HWSR_SAMPLE1 ] | g_HWSR_MaskBit[ HWSR_SAMPLE2 ] | g_HWSR_MaskBit[ HWSR_SAMPLE3 ] | g_HWSR_MaskBit[HWSR_CUBEMAP0] );

    EF_ClearBuffers(FRT_CLEAR_STENCIL | FRT_CLEAR_IMMEDIATE, NULL, 1);
#if defined (DIRECT3D9)
    //m_pd3dDevice->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0);
#else
    //assert(0);
#endif 

    pSH->FXSetTechnique(StencilCullTechName);
    pSH->FXBegin( &nPasses, FEF_DONTSETSTATES );

    for (int nS=0; nS<nSides; nS++)
    {
      //TODO: use light volumes IDs to avoid constant clearing
      assert(pLight!= NULL);

      t_arrDeferredMeshIndBuff arrDeferredInds;
      t_arrDeferredMeshVertBuff arrDeferredVerts;
      CreateDeferredRectangleMesh(pLight, NULL, nS, 10, 10, arrDeferredInds, arrDeferredVerts);
      //use current WorldProj matrix
      FX_StencilCull(nS, arrDeferredInds, arrDeferredVerts, pSH);

    }
    pSH->FXEnd();

    // ortho projection matrix
    m_matProj->Push();
    m_matProj->LoadMatrix(&mQuadProj);  
    m_matView->Push();
    m_matView->LoadIdentity();
    EF_DirtyMatrix();

    FX_CreateDeferredQuad(nOffs, pLight, maskRTWidth, maskRTHeight, &mCurView, &mCurComposite);
    FX_SetVStream( 0, m_pVB[ POOL_P3F_TEX2F_TEX3F ], 0, sizeof( struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F ) ); 

    for (int nS=0; nS<nSides; nS++)
    {
      uint nStencilMask = 1<<nS;
      //FX_StencilRefresh(STENC_FUNC(FSS_STENCFUNC_EQUAL), 0xFFFF, nStencilMask);

      //FIX: temp hack - fill all other sides by black texture
      if(nS>0) 
      {
        m_RP.m_LPasses[0].pLights[0] = &FakeBlackLight;
      }


      pSH->FXSetTechnique(LightTechName);
      pSH->FXBegin( &nPasses, FEF_DONTSETSTATES );

      //////////////////////////////////////////////////////////////////////////
      // set matrices
      //////////////////////////////////////////////////////////////////////////

      float fOffsetX = 0.5f;
      float fOffsetY = 0.5f;
#if defined (DIRECT3D9)
      fOffsetX += (0.5f / pLight->m_pLightImage->GetWidth());
      fOffsetY += (0.5f / pLight->m_pLightImage->GetHeight());
#endif

      Matrix44 mTexScaleBiasMat = Matrix44( 0.5f,     0.0f,     0.0f,    0.0f,  
                                            0.0f,    -0.5f,     0.0f,    0.0f,
                                            0.0f,     0.0f,     1.0f,    0.0f,
                                            fOffsetX, fOffsetY, 0.0f,    1.0f );

      Matrix44 mLightProj, mLightView;
      CShadowUtils::GetCubemapFrustumForLight(pLight, nS, CShadowUtils::g_fOmniLightFov/*pLight->m_fLightFrustumAngle*/, &mLightProj, &mLightView, false);

      Matrix44 mLightViewProj = mLightView * mLightProj; 

      Matrix44 mProjector = mLightViewProj * mTexScaleBiasMat;

      //same parameters for DeferredShadowVS
      gRenDev->m_TempMatrices[0][0] = GetTransposed44(mProjector);
      gRenDev->m_TempMatrices[0][1] = gRenDev->m_TempMatrices[0][0];
      m_cEF.m_TempVecs[1][0] = 0.0f; //disable bias
      m_cEF.m_TempVecs[2][0] = 1.f / (pLight->m_fRadius);
      //////////////////////////////////////////////////////////////////////////


      pSH->FXBeginPass(0);
      if (!FAILED(EF_SetVertexDeclaration( 0, VERTEX_FORMAT_P3F_TEX2F_TEX3F )))
      {
        int newState = m_RP.m_CurState;
        //////////////////////////////////////////////////////////////////////////
        //trick to update zcull with this ref value for multi-lod shadows
  #if defined (DIRECT3D9)
        newState |= GS_COLMASK_NONE;
        newState |= GS_NODEPTHTEST;
        newState &= ~GS_DEPTHWRITE;
        newState |= GS_STENCIL;

        EF_SetStencilState(
          STENC_FUNC(FSS_STENCFUNC_EQUAL) |
          STENCOP_FAIL(FSS_STENCOP_KEEP) |
          STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
          STENCOP_PASS(FSS_STENCOP_KEEP),
          0xFFFF, nStencilMask, 0xFFFFFFFF
        );
        EF_SetState( newState );
        EF_Commit();
        m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);

  #if defined (DIRECT3D9) || defined(OPENGL)
        m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, 2);
  #elif defined (DIRECT3D10)
        SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        m_pd3dDevice->Draw(4, nOffs);
  #endif
        m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += 2;
        m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;

        //restore depth value
        m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        newState |= GS_DEPTHFUNC_LEQUAL;
#endif
        //////////////////////////////////////////////////////////////////////////


        newState |= GS_NODEPTHTEST;
        newState &= ~GS_DEPTHWRITE;
        newState |= GS_BLSRC_ONE | GS_BLDST_ONE;
        newState |= GS_STENCIL;

        //Set LS colormask
        newState &= ~GS_COLMASK_NONE;
        newState |= ( ( ~( 1 << i ) ) << GS_COLMASK_SHIFT ) & GS_COLMASK_MASK;

        EF_SetState( newState );
        EF_Commit();

        EF_SetStencilState(
          STENC_FUNC(FSS_STENCFUNC_EQUAL) |
          STENCOP_FAIL(FSS_STENCOP_KEEP) |
          STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
          STENCOP_PASS(FSS_STENCOP_KEEP),
          0xFFFF, nStencilMask, 0xFFFFFFFF
          );

  #if defined (DIRECT3D9) || defined(OPENGL)
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, nOffs, 2 );
  #elif defined (DIRECT3D10)
        SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        m_pd3dDevice->Draw(4, nOffs);
  #endif

        m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += 2;
        m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
      }
      pSH->FXEndPass();


      pSH->FXEnd();

    }

    m_matProj->Pop();
    m_matView->Pop();

    EF_DirtyMatrix();

  //EF_SetState(~GS_STENCIL);
    EF_SetStencilState(
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
      STENCOP_PASS(FSS_STENCOP_KEEP) |
      STENC_FUNC(FSS_STENCFUNC_EQUAL),
      0x0, 0xFFFFFFFF, 0xFFFFFFFF
    );

    //shadow mask is valid flag 
    // overwrite
    SRendItem::m_ShadowsValidMask[SRendItem::m_RecurseLevel][nGroup] |= (1 << i);
    bWasDrawn = true;

    PROFILE_SHADER_END
  }

  FX_PopRenderTarget(0);

  SetViewport(TempX, TempY, TempWidth, TempHeight);

  //reset lightpass setup
  m_RP.m_LPasses[0].nLights = 0;
  m_RP.m_LPasses[0].pLights[0] = NULL;


  m_RP.m_FrameObject++;
  return bWasDrawn;
}

void CD3D9Renderer::FX_ResolveDepthTarget(CTexture* pSrcZTarget, SD3DSurface* pDstDepthBuffer)
{
#if defined (DIRECT3D10)
    int prevState = m_RP.m_CurState;

    CTexture *pTexColorBuff = NULL;
    CTexture::m_Tex_CurrentScreenShadowMap[0]->Invalidate(pDstDepthBuffer->nWidth, pDstDepthBuffer->nHeight, eTF_A8R8G8B8);
    FX_PushRenderTarget(0, CTexture::m_Tex_CurrentScreenShadowMap[0], pDstDepthBuffer);		// calls SetViewport() implicitly
    SetViewport(0, 0, pDstDepthBuffer->nWidth, pDstDepthBuffer->nHeight);

    //calculate linear-device depth convertion ratios
    //first
    //float fDevDepth = (zf-(zn/SceneDepth))/(zf-zn);
    //second
    //float c1 = zf/(zf-zn);
    //float c2 = zn/(zn-zf);
    //float fDevDepth = c1 + c2/vProjRatios.y;

    float zn = GetRCamera().Near;
    float zf = GetRCamera().Far; 
    float c1 = zf/(zf-zn);
    float c2 = zn/(zn-zf);
    m_cEF.m_TempVecs[0].x = c1;
    m_cEF.m_TempVecs[0].y = c2;

    CShader *pSH( CShaderMan::m_ShaderShadowMaskGen );

    uint nPasses = 0;         
    static CCryName TechName = "ResolveDepthTarget";
    pSH->FXSetTechnique(TechName);
    pSH->FXBegin( &nPasses, FEF_DONTSETSTATES /*| FEF_DONTSETTEXTURES */);

    pSH->FXBeginPass(0);

    //////////////////////////////////////////////////////////////////////////
    float fWidth = (float)m_NewViewport.Width;
    float fHeight = (float)m_NewViewport.Height;

    int nVertexOffset;
    struct_VERTEX_FORMAT_P3F_TEX2F *Verts = (struct_VERTEX_FORMAT_P3F_TEX2F *)GetVBPtr(4, nVertexOffset, POOL_P3F_TEX2F);

    Vec2 vBRMin(0.0f, 0.0f), vBRMax(1.0f, 1.0f);

    assert(Verts!=NULL) ;
    if( Verts )
    {
      Verts->xyz = Vec3(0.0f, 0.0f, 0.0f);
      Verts->st[0] = vBRMin.x;
      Verts->st[1] = 1 - vBRMin.y;
      Verts++;

      Verts->xyz = Vec3(fWidth, 0.0f, 0.0f);
      Verts->st[0] = vBRMax.x;
      Verts->st[1] = 1 - vBRMin.y;
      Verts++;

      Verts->xyz = Vec3(0.0f, fHeight, 0.0f);
      Verts->st[0] = vBRMin.x;
      Verts->st[1] = 1-vBRMax.y;
      Verts++;

      Verts->xyz = Vec3(fWidth, fHeight, 0.0f);
      Verts->st[0] = vBRMax.x;
      Verts->st[1] = 1-vBRMax.y;
    }
    UnlockVB(POOL_P3F_TEX2F);

    const ColorF col(0,0,0,0);
    EF_ClearBuffers(FRT_CLEAR, &col);

    //EF_Commit();

    int newState = m_RP.m_CurState;

    newState = GS_DEPTHFUNC_LESS;

    //debug depth
    //newState |= GS_COLMASK_RGB;
    newState |= GS_COLMASK_NONE;
    newState &= ~GS_NODEPTHTEST;
    newState |= GS_DEPTHWRITE;
    //newState |= GS_WIREFRAME;
    EF_SetState( newState );

    SetCullMode(R_CULL_NONE);

    EF_Commit();


    if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_TEX2F)))
    {
      FX_SetVStream(0, m_pVB[POOL_P3F_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_P3F_TEX2F));

      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      m_pd3dDevice->Draw(4, nVertexOffset);

      m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += 2;
      m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
    }
    pSH->FXEndPass();
    pSH->FXEnd();

    //restore previous state
    EF_SetState(prevState);
    FX_PopRenderTarget(0);
#endif
}


void CD3D9Renderer::FX_StencilRefresh(int StencilFunc, uint nStencRef, uint nStencMask)
{
#if defined (DIRECT3D9) && !defined (XENON)
  //check for nvidia device
  if (m_bDeviceSupports_NVDBT)
  {
    int prevState = m_RP.m_CurState;

    CShader *pSH( CShaderMan::m_ShaderShadowMaskGen );

    uint nPasses = 0;         
    static CCryName TechName = "DeferredSimpleQuad";
    pSH->FXSetTechnique(TechName);
    pSH->FXBegin( &nPasses, FEF_DONTSETSTATES | FEF_DONTSETTEXTURES );

    pSH->FXBeginPass(0);

//////////////////////////////////////////////////////////////////////////
    float fWidth5 = (float)m_NewViewport.Width-0.5f;
    float fHeight5 = (float)m_NewViewport.Height-0.5f;

    int nVertexOffset;
    struct_VERTEX_FORMAT_P3F_TEX2F *Verts = (struct_VERTEX_FORMAT_P3F_TEX2F *)GetVBPtr(4, nVertexOffset, POOL_P3F_TEX2F);

    assert(Verts!=NULL) ;
    if( Verts )
    {
      Verts->xyz = Vec3(-0.5f, -0.5f, 0.0f);
      Verts->st = Vec2(0.0f, 0.0f);
      Verts++;

      Verts->xyz = Vec3(fWidth5, -0.5f, 0.0f);
      Verts->st = Vec2(0.0f, 0.0f);
      Verts++;

      Verts->xyz = Vec3(-0.5f, fHeight5, 0.0f);
      Verts->st = Vec2(0.0f, 0.0f);
      Verts++;

      Verts->xyz = Vec3(fWidth5, fHeight5, 0.0f);
      Verts->st = Vec2(0.0f, 0.0f);
    }
    UnlockVB(POOL_P3F_TEX2F);

    EF_Commit();

    int newState = m_RP.m_CurState;

    newState |= GS_COLMASK_NONE;
    newState |= GS_NODEPTHTEST;
    newState &= ~GS_DEPTHWRITE;
    newState |= GS_STENCIL;
    EF_SetStencilState(
      StencilFunc |
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
      STENCOP_PASS(FSS_STENCOP_KEEP),
      nStencRef, nStencMask, 0xFFFFFFFF
      );
    EF_SetState( newState );
    SetCullMode(R_CULL_NONE);
    EF_Commit();

    //TOFIX - should be additional bit in states
    m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);

    if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_TEX2F)))
    {
      FX_SetVStream(0, m_pVB[POOL_P3F_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_P3F_TEX2F));

  #if defined (DIRECT3D9) || defined(OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nVertexOffset, 2);
  #elif defined (DIRECT3D10)
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      m_pd3dDevice->Draw(4, nVertexOffset);
  #endif

      m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += 2;
      m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
    }
    pSH->FXEndPass();
    pSH->FXEnd();

    //restore previous depth because current is D3DCMP_ALWAYS
    switch (prevState & GS_DEPTHFUNC_MASK)
    {
    case GS_DEPTHFUNC_EQUAL:
      m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
      break;
    case GS_DEPTHFUNC_LEQUAL:
      m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
      break;
    case GS_DEPTHFUNC_GREAT:
      m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATER);
      break;
    case GS_DEPTHFUNC_LESS:
      m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
      break;
    case GS_DEPTHFUNC_NOTEQUAL:
      m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_NOTEQUAL);
      break;
    case GS_DEPTHFUNC_GEQUAL:
      m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATEREQUAL);
      break;
    }
    //restore previous state
    EF_SetState(prevState);
  }
#endif
}

static t_arrDeferredMeshIndBuff arrDefUnitSphereInds;
static t_arrDeferredMeshVertBuff arrDefUnitSphereVerts;

void CD3D9Renderer::FX_StencilCullPassForLightGroup(int nLightGroup)
{


  CShader *pSH( CShaderMan::m_ShaderShadowMaskGen );
  static CCryName TechName = "DeferredShadowPass";

  uint nPasses = 0;

  if (m_pUnitFrustumVB==NULL || m_pUnitFrustumIB==NULL)
  {
    bool bCreated = CreateUnitFrustumMesh();
    if (!bCreated)
    {
      assert(0);
      return;
    }
  }

  if (m_pUnitSphereVB==NULL || m_pUnitSphereIB==NULL)
  {
    bool bCreated = CreateUnitLightMeshes();
    if (!bCreated)
    {
      assert(0);
      return;
    }
  }


  pSH->FXSetTechnique(TechName);
  pSH->FXBegin( &nPasses, FEF_DONTSETSTATES );

  //always force to disable current scissor test
  EF_Scissor(false, 0, 0, 0, 0);

  //TOFIX: prevents non 4 light aligned lightgroups processing
  for( int i = 0; i < 4; ++i )
  {
    int nLightIndex =  i + nLightGroup * 4 ;

    //select valid light sources
    //if ( !(pGr->m_GroupLightMask & (1<<nLightIndex)) )
    //continue;

    if (nLightIndex>=m_RP.m_DLights[SRendItem::m_RecurseLevel].Num())
      continue;

    CDLight* pLight( m_RP.m_DLights[ SRendItem::m_RecurseLevel ][ nLightIndex ] );

    //projective light
    if( pLight->m_Flags & DLF_PROJECT )
    {
      FX_StencilFrustumCull(i, pLight, NULL, 0, pSH);
    }
    else //omni light
    {
      //////////////// light sphere processing /////////////////////////////////
      m_matView->Push();

      float fExpensionRadius = pLight->m_fRadius*1.08f;

      Vec3 vScale(fExpensionRadius, fExpensionRadius, fExpensionRadius);
      Matrix34 mLocal;
      mLocal.SetIdentity();
      mLocal.SetScale(vScale, pLight->m_Origin);
      Matrix44 mLocalTransposed = GetTransposed44(Matrix44(mLocal));
      m_matView->MultMatrixLocal(&mLocalTransposed);


      FX_SetVStream( 0, m_pUnitSphereVB, 0, sizeof( struct_VERTEX_FORMAT_P3F_TEX2F ) );
      FX_SetIStream(m_pUnitSphereIB);

      //  shader pass
      pSH->FXBeginPass( DS_SHADOW_CULL_PASS );

      if (!FAILED(EF_SetVertexDeclaration( 0, VERTEX_FORMAT_P3F_TEX2F )))
        FX_StencilCullPass(i, 0, m_UnitSphereVBSize, 0, m_UnitSphereIBSize);

      pSH->FXEndPass();

      m_matView->Pop();
      //////////////////////////////////////////////////////////////////////////
    }
  }

  pSH->FXEnd();

  return;
}

void CD3D9Renderer::FX_CreateDeferredQuad(int& nOffs, CDLight* pLight, float maskRTWidth, float maskRTHeight, Matrix44* pmCurView, Matrix44* pmCurComposite)
{
  //////////////////////////////////////////////////////////////////////////
  // Create FS quad
  //////////////////////////////////////////////////////////////////////////
  struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F *vQuad( (struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F *) GetVBPtr( 4, nOffs, POOL_P3F_TEX2F_TEX3F ) );


  Vec2 vBoundRectMin(0.0f, 0.0f), vBoundRectMax(1.0f, 1.0f);

  Vec3 vCoords[8];

  Vec3 vRT, vLT, vLB, vRB;


  //calc screen quad
  if (!(pLight->m_Flags & DLF_DIRECTIONAL))
  {
    if(CV_r_ShowLightBounds)
      CShadowUtils::CalcLightBoundRect(pLight, GetRCamera(), *pmCurView, *pmCurComposite, &vBoundRectMin, &vBoundRectMax, gRenDev->GetIRenderAuxGeom());
    else
      CShadowUtils::CalcLightBoundRect(pLight, GetRCamera(), *pmCurView, *pmCurComposite, &vBoundRectMin, &vBoundRectMax, NULL);

    if (m_RenderTileInfo.nGridSizeX > 1.f && m_RenderTileInfo.nGridSizeX > 1.f)
    {

      //always render full-screen quad for hi-res screenshots
      gcpRendD3D->GetRCamera().CalcTileVerts( vCoords,  
        m_RenderTileInfo.nGridSizeX-1-m_RenderTileInfo.nPosX, 
        m_RenderTileInfo.nPosY, 
        m_RenderTileInfo.nGridSizeX,
        m_RenderTileInfo.nGridSizeY);
      vBoundRectMin = Vec2(0.0f, 0.0f);
      vBoundRectMax = Vec2(1.0f, 1.0f);

      vRT = vCoords[4] - vCoords[0];
      vLT = vCoords[5] - vCoords[1];
      vLB = vCoords[6] - vCoords[2];
      vRB = vCoords[7] - vCoords[3];

      //////////////////////////////////////////////////////////////////////////
      /*gcpRendD3D->GetRCamera().CalcTiledRegionVerts( vCoords, vBoundRectMin, vBoundRectMax,
        m_RenderTileInfo.nGridSizeX-1-m_RenderTileInfo.nPosX, 
        m_RenderTileInfo.nPosY, 
        m_RenderTileInfo.nGridSizeX,
        m_RenderTileInfo.nGridSizeY)*/;


      //////////////////////////////////////////////////////////////////////////
      /*Vec3 vFarPlaneVerts[4];
      UnProjectFromScreen( vBoundRectMax.x * m_width, vBoundRectMax.y * m_height, 1, &vFarPlaneVerts[0].x, &vFarPlaneVerts[0].y, &vFarPlaneVerts[0].z);
      UnProjectFromScreen( vBoundRectMin.x * m_width, vBoundRectMax.y	* m_height, 1, &vFarPlaneVerts[1].x, &vFarPlaneVerts[1].y, &vFarPlaneVerts[1].z);
      UnProjectFromScreen( vBoundRectMin.x * m_width, vBoundRectMin.y	* m_height, 1, &vFarPlaneVerts[2].x, &vFarPlaneVerts[2].y, &vFarPlaneVerts[2].z);
      UnProjectFromScreen( vBoundRectMax.x * m_width, vBoundRectMin.y	* m_height, 1, &vFarPlaneVerts[3].x, &vFarPlaneVerts[3].y, &vFarPlaneVerts[3].z);

      vRT = vFarPlaneVerts[0] - GetCamera().GetPosition();
      vLT = vFarPlaneVerts[1] - GetCamera().GetPosition();
      vLB = vFarPlaneVerts[2] - GetCamera().GetPosition();
      vRB = vFarPlaneVerts[3] - GetCamera().GetPosition();*/

    }
    else
    {
      GetRCamera().CalcRegionVerts(vCoords, vBoundRectMin, vBoundRectMax);
      vRT = vCoords[4] - vCoords[0];
      vLT = vCoords[5] - vCoords[1];
      vLB = vCoords[6] - vCoords[2];
      vRB = vCoords[7] - vCoords[3];
    }
  }
  else //full screen case for directional light
  {
    if (m_RenderTileInfo.nGridSizeX > 1.f && m_RenderTileInfo.nGridSizeX > 1.f)
	    gcpRendD3D->GetRCamera().CalcTileVerts( vCoords,  
				    m_RenderTileInfo.nGridSizeX-1-m_RenderTileInfo.nPosX, 
				    m_RenderTileInfo.nPosY, 
				    m_RenderTileInfo.nGridSizeX,
				    m_RenderTileInfo.nGridSizeY);
    else
	    gcpRendD3D->GetRCamera().CalcVerts( vCoords );

    vRT = vCoords[4] - vCoords[0];
    vLT = vCoords[5] - vCoords[1];
    vLB = vCoords[6] - vCoords[2];
    vRB = vCoords[7] - vCoords[3];

  }


#if defined (DIRECT3D10)
  float offsetX( 0 );
  float offsetY( 0 );
#else
  float offsetX( 0.5f / (float) maskRTWidth );
  float offsetY( -0.5f / (float) maskRTHeight );
#endif


  vQuad[0].p.x = vBoundRectMin.x - offsetX;
  vQuad[0].p.y = vBoundRectMin.y - offsetY;
  vQuad[0].p.z = 0;
  vQuad[0].st0[0] = vBoundRectMin.x;
  vQuad[0].st0[1] = 1 - vBoundRectMin.y;
  vQuad[0].st1 = vLB;

  vQuad[1].p.x = vBoundRectMax.x - offsetX;
  vQuad[1].p.y = vBoundRectMin.y - offsetY;
  vQuad[1].p.z = 0;
  vQuad[1].st0[0] = vBoundRectMax.x;
  vQuad[1].st0[1] = 1 - vBoundRectMin.y;
  vQuad[1].st1 = vRB;

  vQuad[3].p.x = vBoundRectMax.x - offsetX;
  vQuad[3].p.y = vBoundRectMax.y - offsetY;
  vQuad[3].p.z = 0;
  vQuad[3].st0[0] = vBoundRectMax.x;
  vQuad[3].st0[1] = 1-vBoundRectMax.y;
  vQuad[3].st1 = vRT;

  vQuad[2].p.x = vBoundRectMin.x - offsetX;
  vQuad[2].p.y = vBoundRectMax.y - offsetY;
  vQuad[2].p.z = 0;
  vQuad[2].st0[0] = vBoundRectMin.x;
  vQuad[2].st0[1] = 1-vBoundRectMax.y;
  vQuad[2].st1 = vLT;

  #if defined(OPENGL)
  // XXX Untested!
  vQuad[0].st1 = vLT;
  vQuad[1].st1 = vRT;
  vQuad[2].st1 = vRB;
  vQuad[3].st1 = vLB;
  #endif

  UnlockVB( POOL_P3F_TEX2F_TEX3F );
}

bool CD3D9Renderer::FX_DeferredShadows( int nGroup, SRendLightGroup* pGr, int maskRTWidth, int maskRTHeight )
{
  // reset render element and current render object in pipeline
  m_RP.m_pRE = 0;
  m_RP.m_pCurObject = m_RP.m_VisObjects[0];
  m_RP.m_ObjFlags = 0;
  m_RP.m_FrameObject++;

  m_RP.m_FlagsShader_RT = 0;
  m_RP.m_FlagsShader_LT = 0;
  m_RP.m_FlagsShader_MD = 0;
  m_RP.m_FlagsShader_MDV = 0;

  bool bWasDrawn = false;

  Matrix44 mCurComposite = *(m_matView->GetTop()) * *(m_matProj->GetTop());
  Matrix44 mCurView = *(m_matView->GetTop()); 
  Matrix44 mCurProj = *(m_matProj->GetTop());

  //set ScreenToWorld Expansion Basis
  Vec3 vWBasisX, vWBasisY, vWBasisZ;
  bool bVPosSM30 = (GetFeatures() & (RFT_HW_PS30|RFT_HW_PS40))!=0;
  CShadowUtils::CalcScreenToWorldExpansionBasis(GetCamera(), (float)maskRTWidth, (float)maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, bVPosSM30);
  m_cEF.m_TempVecs[10] = Vec4(vWBasisX, 1.0f);
  m_cEF.m_TempVecs[11] = Vec4(vWBasisY, 1.0f);
  m_cEF.m_TempVecs[12] = Vec4(vWBasisZ, 1.0f);

  Matrix44 mQuadProj;
  //init matrix for deferred quads rendering
	mathMatrixOrthoOffCenterLH( &mQuadProj , 0, 1, 0, 1, -1, 1 );

  // loop over all lights in this light group
	for( int i = 0; i < 4; ++i )
  {
    if (SRendItem::m_ShadowsValidMask[SRendItem::m_RecurseLevel][nGroup] & (1<<i))
      continue;

    int nLightIndex =  i + nGroup * 4 ;


    //Disabled for now since temporal blur texture can be changed for next render list
    //So we can produce unnecessary shadow generating for some cases

    //select valid light sources
    //if ( !(pGr->m_GroupLightMask & (1<<nLightIndex)) )
    //  continue;

    //FIX this unnecessary check!
    if (nLightIndex>=m_RP.m_DLights[SRendItem::m_RecurseLevel].Num())
      continue;
    
    // get list of shadow casters for nLightID
    CDLight* pLight( m_RP.m_DLights[ SRendItem::m_RecurseLevel ][ nLightIndex ] );

    //check for valid light to process
    if (!(pLight->m_Flags & DLF_CASTSHADOW_MAPS /*|| pLight->m_Flags & DLF_PROJECT*/))
      continue;

    ShadowMapFrustum** ppSMFrustumList = pLight->m_pShadowMapFrustums;

		if (!ppSMFrustumList)
			continue;

		assert( ppSMFrustumList != 0);

    // determine number of casters
    int nCasters = 0;

		
    float fFinalRange[MAX_GSM_LODS_NUM];
	
    //FIX: replace m_pShadowMapFrustums in CDLight by array or list 

		bool bTerrainShadows = false;
		bool bOmniShadows = false;

    ShadowMapFrustum* pLastValidGsmFrustum = NULL;


    //////////////////////////////////////////////////////////////////////////
    //check for valid gsm frustums
    //////////////////////////////////////////////////////////////////////////
    for(/*int nCasterMax = 0 */; *ppSMFrustumList && nCasters!=MAX_GSM_LODS_NUM; ++ppSMFrustumList, ++nCasters/*++nCasterMax*/ )
    {       
      assert( (*ppSMFrustumList)->nDLightId == nLightIndex );          

      //FIX: don't use loop for rendering in this case
      /*if ((*ppSMFrustumList)->pCastersList == NULL)
      {   
        continue;
      }
      else if ((*ppSMFrustumList)->pCastersList->Count() == 0)
      {
        continue;
      }*/


      //TOFIX: remove
      //PrepareDepthMap( *ppSMFrustumList );

      //calc DBT's ranges
      if (CV_r_ShadowsDepthBoundNV && m_bDeviceSupports_NVDBT)
      {
        Vec4 vClipSp = Vec4((*ppSMFrustumList)->vMaxBoundPoint, 1.0) * mCurComposite;
        fFinalRange[nCasters] = vClipSp.z/vClipSp.w;
      }

      //stop enumeration by any non-gsm frustums
      if( (*ppSMFrustumList)->bUseAdditiveBlending ) 
      {
        if (nCasters!=MAX_GSM_LODS_NUM) 
        {
          ++nCasters;
          bTerrainShadows = true;
        }
        
        break;
      }
      else if( (*ppSMFrustumList)->bForSubSurfScattering) 
      {
        break;
      }
      else if ((*ppSMFrustumList)->pCastersList != NULL)
      {
        //TD: change from pointer to index
        if ((*ppSMFrustumList)->pCastersList->Count() > 0)
         pLastValidGsmFrustum = (*ppSMFrustumList);
      }
    }

		if( nCasters == 0)
      continue;

    ppSMFrustumList = pLight->m_pShadowMapFrustums;

    //////////////////////////////////////////////////////////////////////////
    //check for valid point light frustums
    //////////////////////////////////////////////////////////////////////////
    if (nCasters==1 && (ppSMFrustumList[0]->bOmniDirectionalShadow || !(pLight->m_Flags & DLF_DIRECTIONAL)))
    {
      if (ppSMFrustumList[0]->pCastersList == NULL)
      {
        continue;
      }
      else
      {
        if (ppSMFrustumList[0]->pCastersList->Count() <= 0)
          continue;
      }
    }


#if defined (DIRECT3D9) || defined(OPENGL) //draw first LOD with stencil-fill enabled
    //TOFIX: add shadows stencil test variable
    //TOFIX: hack
    //m_pd3dDevice->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0);
    //EF_ClearBuffers(FRT_CLEAR_STENCIL, NULL, 1);
#endif //draw first LOD with stencil-fill enabled

    // set shader
    CShader *pSH( CShaderMan::m_ShaderShadowMaskGen );
    int nOffs;
    uint nPasses = 0;         
    static CCryName TechName = "DeferredShadowPass";

    PROFILE_SHADER_START

    if (nCasters==1 && (ppSMFrustumList[0]->bOmniDirectionalShadow || !(pLight->m_Flags & DLF_DIRECTIONAL)))
    {

      if (CV_r_UseShadowsPool /*&& ppSMFrustumList[0].bUseHWShadowMap*/)
      {
        PrepareDepthMap(ppSMFrustumList[0], pLight);
      }

      pSH->FXSetTechnique(TechName);
      pSH->FXBegin( &nPasses, FEF_DONTSETSTATES );

      if (CV_r_ShadowsDeferredMode == 1)
      {
        int nSides = 1;
        if (ppSMFrustumList[0]->bOmniDirectionalShadow)
          nSides = 6;


        for (int nS=0; nS<nSides; nS++)
        {
          //TODO: use light volumes IDs to avoid constant clearing
          assert(ppSMFrustumList[0]!= NULL);
          assert(pLight!= NULL);

          //FIX: temp solution for projector's camera
          //TF enable linear shadow space and disable this back faces for projectors
          if (pLight->m_Flags & DLF_PROJECT)
          {
            m_RP.m_PersFlags &= ~RBPF_MIRRORCULL;
          }
          else
          {
            m_RP.m_PersFlags |= RBPF_MIRRORCULL;
          }

          //use current WorldProj matrix
          FX_StencilFrustumCull(-1, pLight, ppSMFrustumList[0], nS, pSH);
          //FIX: temp solution for projector's camera
          m_RP.m_PersFlags &= ~RBPF_MIRRORCULL;

          // ortho projection matrix
          m_matProj->Push();
          m_matProj->LoadMatrix(&mQuadProj);  
          m_matView->Push();
          m_matView->LoadIdentity();
          EF_DirtyMatrix();

          FX_CreateDeferredQuad(nOffs, pLight, maskRTWidth, maskRTHeight, &mCurView, &mCurComposite);
          FX_SetVStream( 0, m_pVB[ POOL_P3F_TEX2F_TEX3F ], 0, sizeof( struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F ) );

          FX_DeferredShadowPass( pLight, i, ppSMFrustumList[0], pSH, fFinalRange[0], nOffs, true, false, -1, nS);

          m_matProj->Pop();
          m_matView->Pop();
          EF_DirtyMatrix();

        }

      }
      else if (CV_r_ShadowsDeferredMode == 2)
      {
        int nSides = 1;
        if (ppSMFrustumList[0]->bOmniDirectionalShadow)
          nSides = 6;

        //EF_ClearBuffers(FRT_CLEAR_STENCIL | FRT_CLEAR_IMMEDIATE, NULL, 1);
#if defined (DIRECT3D9)
        m_pd3dDevice->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0);
#else
        EF_ClearBuffers(FRT_CLEAR_STENCIL | FRT_CLEAR_IMMEDIATE, NULL, 1);
        //assert(0);
#endif 

        //merged projector light passes
        //fill light pass for projector setup
#if 0
        if (pLight->m_Flags & DLF_PROJECT && pLight->m_pLightImage)
        {
          m_RP.m_LPasses[0].nLights = 1;
          m_RP.m_LPasses[0].pLights[0] = pLight;
			    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_LIGHT_TEX_PROJ ];
        }
        else
        {
          m_RP.m_LPasses[0].nLights = 0;
          m_RP.m_LPasses[0].pLights[0] = NULL;
          m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[ HWSR_LIGHT_TEX_PROJ ];
        }
#endif

        //FIX: temp solution for projector's camera
        //TF enable linear shadow space and disable this back faces for projectors
        if (pLight->m_Flags & DLF_PROJECT)
        {
          m_RP.m_PersFlags &= ~RBPF_MIRRORCULL;
        }
        else
        {
          m_RP.m_PersFlags |= RBPF_MIRRORCULL;
        }

        for (int nS=0; nS<nSides; nS++)
        {
          //TODO: use light volumes IDs to avoid constant clearing
          assert(ppSMFrustumList[0]!= NULL);
          assert(pLight!= NULL);

          //t_arrDeferredMeshIndBuff arrDeferredInds;
          //t_arrDeferredMeshVertBuff arrDeferredVerts;
          //CreateDeferredRectangleMesh(pLight, ppSMFrustumList[0], nS, 10, 10, arrDeferredInds, arrDeferredVerts);

          //use current WorldProj matrix
          FX_StencilFrustumCull(nS, pLight, ppSMFrustumList[0], nS, pSH);

        }

        //FIX: temp solution for projector's camera
        m_RP.m_PersFlags &= ~RBPF_MIRRORCULL;

        // ortho projection matrix
        m_matProj->Push();
        m_matProj->LoadMatrix(&mQuadProj);  
        m_matView->Push();
        m_matView->LoadIdentity();
        EF_DirtyMatrix();

        FX_CreateDeferredQuad(nOffs, pLight, maskRTWidth, maskRTHeight, &mCurView, &mCurComposite);
        FX_SetVStream( 0, m_pVB[ POOL_P3F_TEX2F_TEX3F ], 0, sizeof( struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F ) );
        for (int nS=0; nS<nSides; nS++)
        {
          FX_DeferredShadowPass( pLight, i, ppSMFrustumList[0], pSH, fFinalRange[0], nOffs, true, false, (nS+1), nS);
        }

        m_matProj->Pop();
        m_matView->Pop();
        EF_DirtyMatrix();

      }
      else
      {
        // ortho projection matrix
        m_matProj->Push();
        m_matProj->LoadMatrix(&mQuadProj);
        m_matView->Push();
        m_matView->LoadIdentity();

        //single pass per-each omni lighsource without stencil cull pass for omni lights and spot lights
        FX_CreateDeferredQuad(nOffs, pLight, maskRTWidth, maskRTHeight, &mCurView, &mCurComposite);
        FX_SetVStream( 0, m_pVB[ POOL_P3F_TEX2F_TEX3F ], 0, sizeof( struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F ) );
        FX_DeferredShadowPass( pLight, i, ppSMFrustumList[0], pSH, fFinalRange[0], nOffs, true, false, 0 );

        m_matProj->Pop();
        m_matView->Pop();
      }

      pSH->FXEnd();

    }
    else //GSM shadows
    {
#if defined (DIRECT3D9)
      m_pd3dDevice->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0);
#else
      EF_ClearBuffers(FRT_CLEAR_STENCIL | FRT_CLEAR_IMMEDIATE, NULL, 1);
      //assert(0);  
#endif

      m_matProj->Push();
      m_matProj->LoadMatrix(&mQuadProj);
      m_matView->Push();
      m_matView->LoadIdentity();

      FX_CreateDeferredQuad(nOffs, pLight, maskRTWidth, maskRTHeight, &mCurView, &mCurComposite);
      FX_SetVStream( 0, m_pVB[ POOL_P3F_TEX2F_TEX3F ], 0, sizeof( struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F ) );

      //temporal hack for processing whithout terrain shadows
      if (!bTerrainShadows)
        nCasters++;

      // loop over all casters to generate shadow mask value for light i
      if (CV_r_ShadowsStencilPrePass)
      {
        pSH->FXSetTechnique(TechName);
        pSH->FXBegin( &nPasses, FEF_DONTSETSTATES );
        //stencil pre-passes
        for( int nCaster=(nCasters-2); nCaster>=0; nCaster-- )
  		  {
          FX_DeferredShadowPass(pLight, i, ppSMFrustumList[ nCaster ], pSH, fFinalRange[nCaster], nOffs, false, true, (nCaster+1) );
		    }
        pSH->FXEnd();


        //shadows passes
        for( int nCaster=0; nCaster<(nCasters-1); nCaster++/*, m_RP.m_PS. ++ */) // for non-conservative 
        {

          //SDW-CFG_PRM
          //overload fading distance for now since we do shadowgen area based fading
          if (ppSMFrustumList[ nCaster ] == pLastValidGsmFrustum)
          {
            ppSMFrustumList[ nCaster ]->fShadowFadingDist = 1.0f;
          }
          else
          {
            ppSMFrustumList[ nCaster ]->fShadowFadingDist = 0.0f;
          }

          if (CV_r_UseShadowsPool /*&& ppSMFrustumList[ nCaster ].bUseHWShadowMap*/)
          {

            PrepareDepthMap(ppSMFrustumList[ nCaster ], pLight);

            m_RP.m_FlagsShader_RT = 0;
            m_RP.m_FlagsShader_LT = 0;
            m_RP.m_FlagsShader_MD = 0;
            m_RP.m_FlagsShader_MDV = 0;


            m_matProj->Push();
            m_matProj->LoadMatrix(&mQuadProj);
            m_matView->Push();
            m_matView->LoadIdentity();

            FX_CreateDeferredQuad(nOffs, pLight, maskRTWidth, maskRTHeight, &mCurView, &mCurComposite);
            FX_SetVStream( 0, m_pVB[ POOL_P3F_TEX2F_TEX3F ], 0, sizeof( struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F ) );


          }



          pSH->FXSetTechnique(TechName);
          pSH->FXBegin( &nPasses, FEF_DONTSETSTATES );
          FX_DeferredShadowPass(pLight, i, ppSMFrustumList[ nCaster ], pSH, fFinalRange[nCaster], nOffs, true, false, (nCaster+1) );
          pSH->FXEnd();

          if (CV_r_UseShadowsPool /*&& ppSMFrustumList[ nCaster ].bUseHWShadowMap*/)
          {
            m_matProj->Pop();
            m_matView->Pop();
          }


        }
      }
      else
      {
        //DX10 path
        for( int nCaster=(nCasters-2); nCaster>=0; nCaster--)
        {
          FX_DeferredShadowPass(pLight, i, ppSMFrustumList[ nCaster ], pSH, fFinalRange[nCaster], nOffs, true, false, 0);
        }
      }

      //terrain shadows
      //TOFIX: we assume that there are shadows from mountains all the time as a last frustum
      // should not be processed for indoors
      if (bTerrainShadows)
      {
        //enable fading for terrain GSM
        ppSMFrustumList[ nCasters-1 ]->fShadowFadingDist = 1.0f;
		    FX_DeferredShadowPass(pLight, i, ppSMFrustumList[ nCasters-1 ], pSH, fFinalRange[ nCasters-1 ], nOffs, true, false, 0);
      }

      m_matProj->Pop();
      m_matView->Pop();
    }
    EF_DirtyMatrix();

    if (CV_r_ShadowsDepthBoundNV && m_bDeviceSupports_NVDBT)
    {
      //disable depth bound test
#if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
      m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_X,0); 
#endif
#elif defined (DIRECT3D10)
      assert(0);  //transparent execution without NVDB
#endif
    }

    //stencil optimization
    //EF_SetState(~GS_STENCIL);
#if defined (DIRECT3D9) || defined(OPENGL)
    EF_SetStencilState(
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
      STENCOP_PASS(FSS_STENCOP_KEEP) |
      STENC_FUNC(FSS_STENCFUNC_EQUAL),
      0x0, 0xFFFFFFFF, 0xFFFFFFFF
      );
#elif defined (DIRECT3D10)
    //assert(0);  //transparent execution without NVDB
#endif


    //shadow mask is valid
    SRendItem::m_ShadowsValidMask[SRendItem::m_RecurseLevel][nGroup] |= (1 << i);

    bWasDrawn = true;

    PROFILE_SHADER_END
  }

  m_RP.m_FrameObject++;
  return bWasDrawn;
}


bool CD3D9Renderer::FX_SetShadowMaskRT(int nGroup, int nBlurMode, CTexture *&tpSrc, SDynTexture *&pTempBlur)
{
  bool bBlur = false;
  int TempX, TempY, TempWidth, TempHeight;
  GetViewport(&TempX, &TempY, &TempWidth, &TempHeight);

  //half of current viewport
  if (CV_r_ShadowsMaskResolution == 1)
  {
    TempHeight /= 2;
  }
  else
  if (CV_r_ShadowsMaskResolution == 2)
  {
    TempWidth /= 2;
    TempHeight /= 2;
  }

  //try to reuse other RTs
  if ( nGroup==0 && CTexture::m_Text_BackBuffer!=NULL && 
       CTexture::m_Text_BackBuffer->GetWidth() == TempWidth &&
       CTexture::m_Text_BackBuffer->GetHeight() == TempHeight)
  {
    CTexture::m_Tex_CurrentScreenShadowMap[nGroup] =  CTexture::m_Text_BackBuffer ;
  }
  else
  if ( nGroup==0 && CTexture::m_Text_BackBufferScaled[0]!=NULL && 
      CTexture::m_Text_BackBufferScaled[0]->GetWidth() == TempWidth &&
      CTexture::m_Text_BackBufferScaled[0]->GetHeight() == TempHeight)
  {
    CTexture::m_Tex_CurrentScreenShadowMap[nGroup] =  CTexture::m_Text_BackBufferScaled[0];
  }
  else
  {
    //allocate separate RT
    CTexture::m_Tex_CurrentScreenShadowMap[nGroup] = CTexture::m_Text_ScreenShadowMap[nGroup];
  }

  //does not support RTs sharing
  int nShadowsMaskDownScale = (nGroup==0 && CTexture::m_Text_BackBuffer!=NULL) ? 0 : CV_r_ShadowsMaskDownScale;
  CTexture::m_Tex_CurrentScreenShadowMap[nGroup]->Invalidate(TempWidth>>nShadowsMaskDownScale, TempHeight>>nShadowsMaskDownScale, eTF_Unknown);

  bBlur = true;
  if (!nBlurMode)
    bBlur = false;

  if (nBlurMode == 1 || !bBlur)
    tpSrc = CTexture::m_Tex_CurrentScreenShadowMap[nGroup];
  else
  {
    pTempBlur = new SDynTexture(TempWidth, TempHeight, eTF_A8R8G8B8, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "TempShadowRT", 95);
    pTempBlur->Update(TempWidth, TempHeight);
    tpSrc = pTempBlur->m_pTexture;
  }
  FX_PushRenderTarget(0, tpSrc, &m_DepthBufferOrig);

  SetViewport(0, 0, TempWidth, TempHeight);
  ColorF clClear(0,0,0,0);		// clear shadowmask black (not in shadow) so we can combine mutliple shadow masks
  int nClear = 0;

  //invalidate shadowmask for transparent pass always
  if(m_RP.m_nPassGroupID == EFSLIST_TRANSP)
  {
    SRendItem::m_ShadowsValidMask[SRendItem::m_RecurseLevel][nGroup] &= ~(0xF << (nGroup*4));
  }

  //clear only once for lightgroup per frame
  //Check if Shadow Mask was cleared for this frame already
  bool bInvalidatedShadMask = ( !( SRendItem::m_ShadowsValidMask[SRendItem::m_RecurseLevel][nGroup] ) && SRendItem::m_RecurseLevel == 1 /*&& m_RP.m_nPassGroupID == EFSLIST_GENERAL && CV_r_usezpass*/);

  
  if (bInvalidatedShadMask)
    nClear = FRT_CLEAR_COLOR | FRT_CLEAR_STENCIL;
  else
    nClear = FRT_CLEAR_STENCIL;

  //#ifdef _DEBUG
  /*#else
  if (!bZpass && m_RP.m_nPassGroupID != EFSLIST_TRANSP_ID)
  nClear = FRT_CLEAR_DEPTH | FRT_CLEAR_STENCIL;
  if (bBlur || (m_RP.m_PersFlags2 & RBPF2_CLEAR_SHADOW_MASK))
  nClear |= FRT_CLEAR_COLOR;
  #endif*/

  EF_ClearBuffers(nClear, &clClear);

  return bBlur;
}

bool CD3D9Renderer::FX_ProcessShadowsListsForLightGroup(int nGroup, int nOffsRI)
{
	uint i, j;

	if (CV_r_ShadowPass == 0)
		return false;

	if (m_polygon_mode == R_WIREFRAME_MODE || !CV_r_usezpass)
	{
		return false;
	}

	//Currently all deferred passes are disabled for other recursions by default 
	if (SRendItem::m_RecurseLevel!=1)
		return false;

#ifdef XENON
  // We have to move all shadow passes before general scene passes
  if (m_RP.m_nPassGroupID == EFSLIST_TRANSP)
    return false;
#endif

	PROFILE_FRAME(DrawShader_Shadows_Passes);

  bool bDrawn = false;
  bool bSetRT = false;
	bool bDefSunLG = false;
  bool bOpaqueDrawn = false;
  int nR = SRendItem::m_RecurseLevel;
  bool bBlur = false;
  int TempX, TempY, TempWidth, TempHeight;
  int nPrevGroup = m_RP.m_nCurLightGroup;
  //m_RP.m_PersFlags2 |= RBPF2_DRAWSHADOWS;
  m_RP.m_nCurLightGroup = nGroup;

  void (*pSaveRenderFunc)();
  pSaveRenderFunc = m_RP.m_pRenderFunc;
  m_RP.m_pRenderFunc = EF_FlushShadow;
  EF_PreRender(0);

  int nPassGroup = m_RP.m_nPassGroupID;
  int nPassGroup2 = m_RP.m_nPassGroupDIP;
  int nAW = m_RP.m_nSortGroupID;

  static ICVar * p_e_shadows_clouds = iConsole->GetCVar("e_shadows_clouds");

  CTexture *tpSrc = NULL;
  SDynTexture *pTempBlur = NULL;
  for (int n=0; n<SRendItem::m_GroupProperty[nR][nAW][nPassGroup].m_nSortGroups; n++)
  {
    SRendLightGroup *pGr = &SRendItem::m_RenderLightGroups[nR][nPassGroup][nAW][n][nGroup];

    //Special case for transparent geometry 
    if (!pGr->m_GroupLightMask /*&& !(nPassGroup == EFSLIST_TRANSP)*/)
      continue;

    //Test for sun light group
    uint nFirstLight = nGroup < 0 ? 0 : nGroup*4;
    CDLight *pGroupFirstLight = m_RP.m_DLights[SRendItem::m_RecurseLevel][nFirstLight];

    //Special case for transparent geometry
    if (nPassGroup == EFSLIST_TRANSP)
    //if ( !( CRenderer::CV_r_ShadowPassFS && (pGr->m_GroupLightMask == 1) && (pGroupFirstLight->m_Flags & DLF_DIRECTIONAL) ) )
    {
      if (!pGr->RendItemsShadows[0].size() && !pGr->RendItemsShadows[1].size() && !pGr->RendItemsShadows[2].size() && !pGr->RendItemsShadows[3].size())
        continue;
    }

    m_RP.m_pShader = NULL;
    m_RP.m_pShaderResources = NULL;
    m_RP.m_pCurObject = m_RP.m_VisObjects[0];
    m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;

    m_RP.m_pPrevObject = NULL;

    if (!bSetRT)
    {
      if (m_LogFile)
        Logv(SRendItem::m_RecurseLevel, "\n   +++ Draw shadow maps for group %d\n", nGroup); 

      GetViewport(&TempX, &TempY, &TempWidth, &TempHeight);
      #if defined(DIRECT3D9)
        //disable shadow blur with nvidia filtered PCF for now
        if (CV_r_shadowtexformat==4 && m_FormatD24S8.IsValid())
        {
          bBlur = 0;
          FX_SetShadowMaskRT(nGroup, 0,tpSrc, pTempBlur);
        }
        else
        {
          bBlur = CV_r_shadowblur != 0;
          FX_SetShadowMaskRT(nGroup, CV_r_shadowblur,tpSrc, pTempBlur);
        }
      #else
        //disable shadow blur for dx10
        bBlur = 0;
        FX_SetShadowMaskRT(nGroup, 0,tpSrc, pTempBlur);
      #endif


      bSetRT = true;
    }

    //FIX:: should be prepeared already
    //if (!CV_r_ShadowsForwardPass)
    //  FX_PrepareDepthMapsForLightGroup(nGroup, pGr, nOffsRI);

    m_RP.m_nCurLightGroup = nGroup;
    m_RP.m_nPassGroupID = nPassGroup;
    m_RP.m_nPassGroupDIP = EFSLIST_SHADOW_PASS;
    m_RP.m_pRenderFunc = EF_FlushShadow;


    if ( nPassGroup != EFSLIST_TRANSP )
    {
       //viewport for current render target
       int vpX, vpY, vpWidth, vpHeight;
       GetViewport( &vpX, &vpY, &vpWidth, &vpHeight );
       assert( vpX == 0 && vpY == 0 && vpWidth > 0 && vpHeight > 0 );
       bDrawn = FX_DeferredShadows( nGroup, pGr, vpWidth, vpHeight );
       //additional flag for sharing SRendItem::m_ShadowsValidMask for projector validation
       bOpaqueDrawn = bDrawn;
    }
    else
    { //draw all others shadows
      CShader *pShader, *pCurShader;
      SRenderShaderResources *pRes;
      CRenderObject *pObject, *pCurObject;
      int nTech;

			for (i=/*bDefSunLG?1:*/0; i<4; i++) //draw shadows for transparent for sun only
      {
        //note: moved to the actual draw in FX_DrawShadowPasses
        //shadow mask is valid
        //SRendItem::m_ShadowsValidMask[SRendItem::m_RecurseLevel][nGroup] |= (1 << i);

        if (!(pGr->m_GroupLightMask & (1<<(i+nGroup*4))))
          continue;
        //int nR = SRendItem::m_RecurseLevel;
        CDLight* pLight = m_RP.m_DLights[nR][nGroup*4+i];

        //skip all except sun for now
        if ( !(pLight ->m_Flags & DLF_DIRECTIONAL || pLight->m_Flags & DLF_PROJECT) )
          continue;

        m_RP.m_nCurLightChan = i;
        uint oldVal = ~0;
        pCurObject = NULL;
        pCurShader = NULL;
        bool bIgnore = false;
        bool bChanged;
        int nAW = m_RP.m_nSortGroupID;

        for (j=0; j<pGr->RendItemsShadows[i].size(); j++)
        {
          int nRI = pGr->RendItemsShadows[i][j];
          nRI = (nRI & 0xffffff) + nOffsRI;
          SRendItem *ri = &SRendItem::m_RendItems[nAW][m_RP.m_nPassGroupID][nRI];
          CRendElement *pRE = ri->Item;
          if (oldVal != ri->SortVal)
          {
            SRendItem::mfGet(ri->SortVal, nTech, pShader, pRes);
            bChanged = true;
          }
          else
            bChanged = false;
          pObject = ri->pObj;
          oldVal = ri->SortVal;
          if (pObject != pCurObject)
          {
            if (!bChanged)
            {
              if (EF_TryToMerge(pObject, pCurObject, pRE))
                continue;
            }
            if (pCurShader)
            {
              m_RP.m_pRenderFunc();
              pCurShader = NULL;
              bChanged = true;
            }
            if (!EF_ObjectChange(pShader, pRes, pObject, pRE))
            {
              bIgnore = true;
              continue;
            }
            bIgnore = false;
            pCurObject = pObject;
          }

          if (bChanged)
          {
            if (pCurShader)
              m_RP.m_pRenderFunc();
            EF_Start(pShader, nTech, pRes, pRE);
            pCurShader = pShader;
          }

          {
            //PROFILE_FRAME_TOTAL(Mesh_REPrepare);
            pRE->mfPrepare();
          }
        }
        if (pCurShader)
          m_RP.m_pRenderFunc();

        bDrawn = true;
      }
    }
  }
  //m_RP.m_PersFlags2 &= ~RBPF2_DRAWSHADOWS;
  m_RP.m_nPassGroupDIP = nPassGroup2;
  EF_PostRender();

	// Store shadow mask
#if defined (DIRECT3D9) || defined (OPENGL)
	if (CV_capture_misc_render_buffers && CV_capture_frames->GetIVal() && EFSLIST_GENERAL == m_RP.m_nPassGroupID && CTexture::m_Tex_CurrentScreenShadowMap[nGroup]->GetDeviceTexture())
	{
		IDirect3DSurface9* pSrcSurface;
		reinterpret_cast<IDirect3DTexture9*>(CTexture::m_Tex_CurrentScreenShadowMap[nGroup]->GetDeviceTexture())->GetSurfaceLevel(0, &pSrcSurface);
		if (pSrcSurface)
		{
			D3DSURFACE_DESC srcDesc;
			pSrcSurface->GetDesc(&srcDesc);

			if (D3DMULTISAMPLE_NONE == srcDesc.MultiSampleType)
			{
				bool needRealloc(true);
				if (m_pCaptureScreenShadowMap[nGroup])
				{
					D3DSURFACE_DESC dstDesc;
					m_pCaptureScreenShadowMap[nGroup]->GetDesc(&dstDesc);
					if (dstDesc.Format == srcDesc.Format && dstDesc.Width == srcDesc.Width && dstDesc.Height == srcDesc.Height)
						needRealloc = false;
				}

				if (needRealloc)
				{
					SAFE_RELEASE(m_pCaptureScreenShadowMap[nGroup]);
					m_pd3dDevice->CreateOffscreenPlainSurface(srcDesc.Width, srcDesc.Height, srcDesc.Format, D3DPOOL_SYSTEMMEM, &m_pCaptureScreenShadowMap[nGroup], 0);
				}

				if (m_pCaptureScreenShadowMap[nGroup])
				{
					m_pd3dDevice->GetRenderTargetData(pSrcSurface, m_pCaptureScreenShadowMap[nGroup]);
					m_ValidCaptureScreenShadowMap[nGroup] = true;
				}
			}
		}
	}
#endif

  CTexture *pTexMask = CTexture::m_Tex_CurrentScreenShadowMap[nGroup];

  //these passes are drawn for SUN lightgroup only
  if ((nPassGroup == EFSLIST_GENERAL) && (nGroup==0) && !(m_RP.m_PersFlags & RBPF_MAKESPRITE) && SRendItem::m_RecurseLevel == 1)
  {
    //Draw sprites shadows if shadows there is no deferred shadow pass for this lightgroup and 
    // there are sprites to draw
    if (bDrawn && !bDefSunLG )
    {
      //DrawSpritesShadows(bBlur, bSetRT, tpSrc);
    }
  }
  else
  if (!bSetRT && nPassGroup==EFSLIST_TRANSP && nGroup==0 && !(m_RP.m_PersFlags & RBPF_MAKESPRITE) && SRendItem::m_RecurseLevel == 1 && p_e_shadows_clouds->GetIVal())
  {
    ColorF cBlack(0,0,0);
    if (pTexMask && pTexMask->GetDeviceTexture())
      pTexMask->Fill(cBlack);
  }

  if (bSetRT && bDrawn)
  {
    FX_PopRenderTarget(0);

    if (bBlur)
    {
      if (CV_r_shadowblur == 1)
      {
        int nSizeX = tpSrc->GetWidth();
        int nSizeY = tpSrc->GetHeight();
        if (!pTempBlur)
          pTempBlur = new SDynTexture(nSizeX, nSizeY, eTF_A8R8G8B8, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "TempShadowRT", 95);
        FX_ShadowBlur(CV_r_shadowbluriness, pTempBlur, pTexMask);
      }
      else
      {
        assert(pTempBlur);
        FX_ShadowBlur(CV_r_shadowbluriness, pTempBlur, pTexMask);
      }    
		}

    SetViewport(TempX, TempY, TempWidth, TempHeight);

    if (m_LogFile)
      Logv(SRendItem::m_RecurseLevel, "\n   +++ End shadow maps for group %d\n", nGroup); 
  }
  else 
  if(bSetRT)
  {
    FX_PopRenderTarget(0);
    SetViewport(TempX, TempY, TempWidth, TempHeight);

    if (m_LogFile)
      Logv(SRendItem::m_RecurseLevel, "\n   +++ End shadow maps for group %d\n", nGroup); 
  }

  //projectors 
  if (CV_r_ShadowsDeferredMode>2)
  {
    for (int n=0; n<SRendItem::m_GroupProperty[nR][nAW][nPassGroup].m_nSortGroups; n++)
    {
      SRendLightGroup *pGr = &SRendItem::m_RenderLightGroups[nR][nPassGroup][nAW][n][nGroup];

      //Special case for transparent geometry 
      if (!pGr->m_GroupLightMask)
        continue;

      FX_DeferredProjLights(nGroup, !bOpaqueDrawn);
    }
  }


  SAFE_DELETE(pTempBlur);

  m_RP.m_nNumRendPasses = 0;
  m_RP.m_nCurLightGroup = nPrevGroup;
  m_RP.m_pRenderFunc = pSaveRenderFunc;

  return true;
}
