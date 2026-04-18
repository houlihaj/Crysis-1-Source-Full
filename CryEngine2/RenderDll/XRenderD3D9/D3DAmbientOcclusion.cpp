/*=============================================================================
D3DAmbientOcclusion.cpp : implementation of ambient occlusion related features.
Copyright 2001 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Vladimir Kajalin

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "I3DEngine.h"

#undef min
#undef max

void CD3D9Renderer::CreateDeferredUnitBox(t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
  struct_VERTEX_FORMAT_P3F_TEX2F vert;
  Vec3 vNDC;

  indBuff.clear();
  indBuff.reserve(36);

  vertBuff.clear();
  vertBuff.reserve(8);

  //Create frustum
  for (int i=0; i<8; i++ )
  {
    //Generate screen space frustum (CCW faces)
    vNDC = Vec3((i==0 || i==1 || i==4 || i==5) ? 0.0f : 1.0f,
      (i==0 || i==3 || i==4 || i==7) ? 0.0f : 1.0f,
      (i==0 || i==1 || i==2 || i==3) ? 0.0f : 1.0f
      );
    vert.xyz = vNDC;
    vert.st = Vec2(0.0f, 0.0f);
    vertBuff.push_back(vert);
  }

  //CCW faces
  uint16 nFaces[6][4] = {{0,1,2,3},
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

void CD3D9Renderer::SetDepthBoundTest(float fMin, float fMax, bool bEnable)
{
  if(!m_bDeviceSupports_NVDBT)
    return;

  if (bEnable)
  {
#if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
    m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_X,MAKEFOURCC('N','V','D','B')); 
    m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_Z,*(DWORD*)&fMin); 
    m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_W,*(DWORD*)&fMax);
#endif
#elif defined (DIRECT3D10) //transparent execution without NVDB
    assert(0); 
#endif
  }
  else // disable depth bound test
  {
#if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
    m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_X,0); 
#endif
#elif defined (DIRECT3D10)
    assert(0);  //transparent execution without NVDB
#endif
  }
}

void CD3D9Renderer::FX_DeferredShadowPassAO( CCryName & TechName, SSectorTextureSet * pSector, SFillLight * pLight, Matrix44 * pMatComposite, SDynTexture ** arrDepthTempRT )
{
  int nOffs;
  struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F *vQuad( (struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F *) GetVBPtr( 4, nOffs, POOL_P3F_TEX2F_TEX3F ) );

  if(!vQuad)
    return;

  Vec3 vBoundRectMin(0.0f, 0.0f, 0.0f), vBoundRectMax(1.0f, 1.0f, 1.0f);

  if(pSector && (CV_r_TerrainAO&4))
  {
    assert(pMatComposite); 

    AABB aabb(pSector->stencilBox[0],pSector->stencilBox[1]);

    if(!aabb.IsContainPoint(GetRCamera().Orig))
      CalcAABBScreenRect(aabb, GetRCamera(), *pMatComposite, &vBoundRectMin,  &vBoundRectMax);

    if(CV_r_TerrainAO&8)
    {
      gRenDev->Draw2dLabel( vBoundRectMin.x*GetWidth(), GetHeight() - vBoundRectMin.y*GetHeight(), 2, NULL, true, "MIN");
      gRenDev->Draw2dLabel( vBoundRectMax.x*GetWidth(), GetHeight() - vBoundRectMax.y*GetHeight(), 2, NULL, true, "MAX");
    }
  }

  if(pLight && (CV_r_FillLightsMode&4))
  {
    assert(pMatComposite); 

    AABB aabb(pLight->m_vOrigin - Vec3(pLight->m_fRadius,pLight->m_fRadius,pLight->m_fRadius),
              pLight->m_vOrigin + Vec3(pLight->m_fRadius,pLight->m_fRadius,pLight->m_fRadius));

    if(!aabb.IsContainPoint(GetRCamera().Orig))
      CalcAABBScreenRect(aabb, GetRCamera(), *pMatComposite, &vBoundRectMin,  &vBoundRectMax);

    if(CV_r_FillLightsMode&16)
    {
      gRenDev->Draw2dLabel( vBoundRectMin.x*GetWidth(), GetHeight() - vBoundRectMin.y*GetHeight(), 2, NULL, true, "MIN");
      gRenDev->Draw2dLabel( vBoundRectMax.x*GetWidth(), GetHeight() - vBoundRectMax.y*GetHeight(), 2, NULL, true, "MAX");
    }
  }

  int maskRTWidth = CTexture::m_Text_AOTarget->GetWidth();
  int maskRTHeight = CTexture::m_Text_AOTarget->GetHeight();

#if defined (DIRECT3D10)
  float offsetX( 0 );
  float offsetY( 0 );
#else
  float offsetX( 0.5f / (float) maskRTWidth );
  float offsetY( -0.5f / (float) maskRTHeight );
#endif

  Vec3 vCoords[8];

  if (m_RenderTileInfo.nGridSizeX > 1.f && m_RenderTileInfo.nGridSizeX > 1.f)
    gcpRendD3D->GetRCamera().CalcTileVerts( vCoords,  
    m_RenderTileInfo.nGridSizeX-1-m_RenderTileInfo.nPosX, 
    m_RenderTileInfo.nPosY, 
    m_RenderTileInfo.nGridSizeX,
    m_RenderTileInfo.nGridSizeY);
  else
    gcpRendD3D->GetRCamera().CalcVerts( vCoords );

  Vec3 vRT = vCoords[4] - vCoords[0];
  Vec3 vLT = vCoords[5] - vCoords[1];
  Vec3 vLB = vCoords[6] - vCoords[2];
  Vec3 vRB = vCoords[7] - vCoords[3];

  GetRCamera().CalcRegionVerts(vCoords, Vec2(vBoundRectMin), Vec2(vBoundRectMax));
  vRT = vCoords[4] - vCoords[0];
  vLT = vCoords[5] - vCoords[1];
  vLB = vCoords[6] - vCoords[2];
  vRB = vCoords[7] - vCoords[3];

  float fVertDepth = 0.f;
  if(CV_r_SSAO_depth_range && !pLight && !pSector)
    fVertDepth = CV_r_SSAO_depth_range;

  vQuad[0].p.x = vBoundRectMin.x - offsetX;
  vQuad[0].p.y = vBoundRectMin.y - offsetY;
  vQuad[0].p.z = fVertDepth;
  vQuad[0].st0[0] = vBoundRectMin.x;
  vQuad[0].st0[1] = 1 - vBoundRectMin.y;
  vQuad[0].st1 = vLB;

  vQuad[1].p.x = vBoundRectMax.x - offsetX;
  vQuad[1].p.y = vBoundRectMin.y - offsetY;
  vQuad[1].p.z = fVertDepth;
  vQuad[1].st0[0] = vBoundRectMax.x;
  vQuad[1].st0[1] = 1 - vBoundRectMin.y;
  vQuad[1].st1 = vRB;

  vQuad[3].p.x = vBoundRectMax.x - offsetX;
  vQuad[3].p.y = vBoundRectMax.y - offsetY;
  vQuad[3].p.z = fVertDepth;
  vQuad[3].st0[0] = vBoundRectMax.x;
  vQuad[3].st0[1] = 1-vBoundRectMax.y;
  vQuad[3].st1 = vRT;

  vQuad[2].p.x = vBoundRectMin.x - offsetX;
  vQuad[2].p.y = vBoundRectMax.y - offsetY;
  vQuad[2].p.z = fVertDepth;
  vQuad[2].st0[0] = vBoundRectMin.x;
  vQuad[2].st0[1] = 1-vBoundRectMax.y;
  vQuad[2].st1 = vLT;

  UnlockVB( POOL_P3F_TEX2F_TEX3F );

  FX_SetVStream( 0, m_pVB[ POOL_P3F_TEX2F_TEX3F ], 0, sizeof( struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F ) );

  // set shader
  CShader *pSH( CShaderMan::m_ShaderAmbientOcclusion );

  uint nPasses = 0;         

  pSH->FXSetTechnique(TechName);
  pSH->FXBegin( &nPasses, (pSector||pLight) ? (FEF_DONTSETSTATES | FEF_DONTSETTEXTURES) : FEF_DONTSETSTATES);

  EF_DisableATOC();

  SetCullMode(R_CULL_BACK);

  // Set shader quality
  switch (m_RP.m_nShaderQuality = CV_r_SSAO_quality)
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

  int newState = 0;

  if(fVertDepth == 0)
    newState |= GS_NODEPTHTEST;

  pSH->FXBeginPass( 0 );

  Vec4 vConst;

  if(pSector)
  { // terrain
    STexState TexStatePoint( FILTER_LINEAR, true );

    CTexture::m_Text_ZTarget->Apply( 0, CTexture::GetTexState(TexStatePoint) );

    CTexture * pTerrTex0 = CTexture::GetByID(pSector->nTex0);
    pTerrTex0->Apply( 1, CTexture::GetTexState(TexStatePoint) );

    if(pSector->nTex1>0)
    {
      CTexture * pTerrTex1 = CTexture::GetByID(pSector->nTex1);
      pTerrTex1->Apply( 2, CTexture::GetTexState(TexStatePoint) );
    }
    else
      CTexture::m_Text_White->Apply( 2, CTexture::GetTexState(TexStatePoint) );

		static CCryName AOSectorRangeName("AOSectorRange");
    vConst = Vec4(pSector->nodeBox[0].x, pSector->nodeBox[0].y, pSector->fTerrainMinZ, pSector->fTerrainMaxZ);
    pSH->FXSetPSFloat(AOSectorRangeName, &vConst, 1);

    float fSkyBr = iSystem->GetI3DEngine()->GetSkyBrightness();
		static CCryName TerrainAOInfoName("TerrainAOInfo");
		vConst = Vec4(fSkyBr, 1.f/std::max((float)CV_r_TerrainAO_FadeDist,0.1f), 0, pSector->fTexScale);
    pSH->FXSetPSFloat(TerrainAOInfoName, &vConst, 1);
  }
  else if(pLight)
  { 
    STexState TexStatePoint( FILTER_LINEAR, true );
    CTexture::m_Text_ZTarget->Apply( 0, CTexture::GetTexState(TexStatePoint) );

    // fill light
		static CCryName FillLightPosName("FillLightPos");
    vConst = Vec4(pLight->m_vOrigin.x,pLight->m_vOrigin.y,pLight->m_vOrigin.z,pLight->m_fRadius);
    pSH->FXSetPSFloat(FillLightPosName, &vConst, 1);
		static CCryName FillLightColorName("FillLightColor");
    vConst = Vec4(pLight->m_fIntensity,0,0,0);
    if(pLight->bNegative)
      vConst.x *= 2.f;
    pSH->FXSetPSFloat(FillLightColorName, &vConst, 1);

    newState |= (GS_BLSRC_ONE | GS_BLDST_ONE);		// additive

    if(CV_r_FillLightsMode&8)
      SetDepthBoundTest(vBoundRectMin.z, vBoundRectMax.z, true);
  }
  else
  { // SSAO
    STexState TexStatePoint( FILTER_LINEAR, true );
    for(int t=0; arrDepthTempRT && t<8; t++)
      if(arrDepthTempRT[t] && arrDepthTempRT[t]->m_pTexture)
        arrDepthTempRT[t]->m_pTexture->Apply( 4+t, CTexture::GetTexState(TexStatePoint) );

    static CCryName FillLightColorName("SSAO_params");
    
    float fRadiusXY = CV_r_SSAO_radius;
    if(m_RenderTileInfo.nGridSizeX > 1.f)
      fRadiusXY *= 0.5f*(m_RenderTileInfo.nGridSizeX+m_RenderTileInfo.nGridSizeY);

    vConst = Vec4(CV_r_SSAO_amount*iSystem->GetI3DEngine()->GetSSAOAmount(), CV_r_SSAO_darkening, fRadiusXY, CV_r_SSAO_radius);
    pSH->FXSetPSFloat(FillLightColorName, &vConst, 1);
  }

  if (!FAILED(EF_SetVertexDeclaration( 0, VERTEX_FORMAT_P3F_TEX2F_TEX3F )))
  {
    if(pLight && pLight->bNegative)
      newState |= (GS_NOCOLMASK_R | GS_NOCOLMASK_G | GS_NOCOLMASK_B /*| GS_NOCOLMASK_A*/);
    else if(pLight)
      newState |= (GS_NOCOLMASK_R | GS_NOCOLMASK_G | /*GS_NOCOLMASK_B | */GS_NOCOLMASK_A);
    else if(pSector)
      newState |= (GS_NOCOLMASK_R | /*GS_NOCOLMASK_G | */GS_NOCOLMASK_B | GS_NOCOLMASK_A);
    else
      newState |= (/*GS_NOCOLMASK_R |*/ GS_NOCOLMASK_G | GS_NOCOLMASK_B | GS_NOCOLMASK_A); // SSAO


    if((pSector && (CV_r_TerrainAO&1)) || (pLight && (CV_r_FillLightsMode&1)))
      newState |= GS_STENCIL;

    if(pSector||pLight)
      EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_EQUAL) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
      STENCOP_PASS(FSS_STENCOP_KEEP),
      m_nStencilMaskRef, 0xFFFFFFFF, 0xFFFFFFFF);

    if(fVertDepth > 0)
      newState |= GS_DEPTHFUNC_GREAT;

    EF_SetState( newState );
#if !defined(EXCLUDE_SCALEFORM_SDK)
    if(pLight)
      SF_SetBlendOp(SSF_GlobalDrawParams::RevSubstract);
#endif
    EF_Commit();

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

#if !defined(EXCLUDE_SCALEFORM_SDK)
  if(pLight) // restore default blend operation
    SF_SetBlendOp(SSF_GlobalDrawParams::RevSubstract, true);
#endif

  if(pLight && (CV_r_FillLightsMode&8))
    SetDepthBoundTest(0.f, 1.f, false);

  // HACK: otherwise GS_DEPTHFUNC stays random
  EF_SetState( 0 );
  EF_Commit();
}

extern void StretchRectAO(CTexture *pSrc, CTexture *pDst, bool bFirst, bool b3x3);

bool CD3D9Renderer::FX_ProcessAOTarget()
{
  PROFILE_FRAME(FX_ProcessAOTarget);

//PS3HACK -> remove
#if defined(PS3)
	return true;
#endif 

  if(!CTexture::m_Text_ZTarget || !CTexture::m_Text_ZTarget->GetDeviceTexture())
    return true; // z-target not ready

  bool bSSAO_ON = CV_r_SSAO && (CV_r_SSAO_amount*iSystem->GetI3DEngine()->GetSSAOAmount() > 0.1f);

  // AO target is used for SSAO and also for TerrainAO
  // In order to disable AO completely you have to set to zero r_SSAO and r_TerrainAO
  if(!bSSAO_ON && !(CV_r_TerrainAO && m_TerrainAONodes.Count()) && !(CV_r_FillLights && m_RP.m_FillLights[SRendItem::m_RecurseLevel].Num()))
    return true;

  if(CV_r_SSAO > 1)
    gRenDev->Draw2dLabel(10,10,2,NULL,false,"TOD SSAOAmount = %.2f, Final amount = %.2f", 
    iSystem->GetI3DEngine()->GetSSAOAmount(), CV_r_SSAO_amount*iSystem->GetI3DEngine()->GetSSAOAmount());

  if(CV_r_FillLightsDebug)
  {
    static float fFillLightsNum = 0;
    fFillLightsNum = 0.9f*fFillLightsNum + 0.1f*m_RP.m_FillLights[SRendItem::m_RecurseLevel].Num();
    gRenDev->Draw2dLabel(10,25,2,NULL,false,"FillLights Num = %d", (int)fFillLightsNum);
  }

  m_RP.m_nAOMaskUpdateLastFrameId = GetFrameID(false);

  int nWidth = GetWidth();
  int hHeight = GetHeight();

  SDynTexture *pTempRT = NULL;

  // allocate temp RT if needed
  if(bSSAO_ON && CV_r_SSAO_blur)
  {
    pTempRT = new SDynTexture(nWidth, hHeight, eTF_A8R8G8B8, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "TempAORT", 95);
    pTempRT->Update(nWidth, hHeight);
  }

  SDynTexture *arrDepthTempRT[8] = {0,0,0,0,0,0,0,0};

  // allocate temp RT if needed
  if(bSSAO_ON && CV_r_SSAO == 3)
  {
    for(int t=0; t<8; t++)
    {
      int w = GetWidth()>>(t+1);
      int h = GetHeight()>>(t+1);
      char szName[32];
      sprintf(szName, "TempAODepthRT%d", t);
      arrDepthTempRT[t] = new SDynTexture(w, h, eTF_G16R16F, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, szName, 95);
      arrDepthTempRT[t]->Update(w, h);
    }
  }

  // generate downscaled z-target
  if(CV_r_SSAO_downscale_ztarget && bSSAO_ON)
  {
    Set2DMode(true, 1,1);

    if(CV_r_SSAO == 3)
    {
      CTexture::m_Text_ZTargetScaled->Invalidate(GetWidth()>>1, GetHeight()>>1, eTF_G16R16F);
      StretchRectAO(CTexture::m_Text_ZTarget, arrDepthTempRT[0]->m_pTexture, true, true);
      for(int t=0; t<7; t++)
        StretchRectAO(arrDepthTempRT[t]->m_pTexture, arrDepthTempRT[t+1]->m_pTexture, false, true);
      FX_ShadowBlur(CV_r_SSAO_blurriness, arrDepthTempRT[0], CTexture::m_Text_ZTargetScaled, 2);
    }
    else
    {
      CTexture::m_Text_ZTargetScaled->Invalidate(GetWidth()>>1, GetHeight()>>1, CTexture::m_eTFZ);
      StretchRectAO(CTexture::m_Text_ZTarget, CTexture::m_Text_ZTargetScaled, true, false);
    }

    Set2DMode(false, 1,1);
  }
  else
    CTexture::m_Text_ZTargetScaled->ReleaseDeviceTexture(false);

  // set target once for all ambient effects
  int nDownScaleResult = pTempRT ? CV_r_SSAO_downscale_result_mask : 0;
  CTexture::m_Text_AOTarget->Invalidate(nWidth>>nDownScaleResult, hHeight>>nDownScaleResult, eTF_A8R8G8B8);
  FX_PushRenderTarget(0, pTempRT ? pTempRT->m_pTexture : CTexture::m_Text_AOTarget, &m_DepthBufferOrig);

  // clear once
  ColorF clClear(1,1,1,1);
  EF_ClearBuffers(FRT_CLEAR_COLOR|FRT_CLEAR_IMMEDIATE, &clClear, 1);

  // render SSAO pass into Red channel
  if(bSSAO_ON)
  {
    PROFILE_FRAME(FX_ProcessAOTarget_SSAO);

    PROFILE_SHADER_START;
    m_matProj->Push();
    mathMatrixOrthoOffCenterLH( m_matProj->GetTop(), 0, 1, 0, 1, -1, 1 );
    m_matView->Push();
    m_matView->LoadIdentity();
    if(CV_r_SSAO == 3)
    {
      static CCryName TechName = "Deferred_SSAO_Pass_DepthBlurBased";
      FX_DeferredShadowPassAO( TechName, NULL, NULL, NULL, arrDepthTempRT );
    }
    else
    {
      static CCryName TechName = "Deferred_SSAO_Pass";
      FX_DeferredShadowPassAO( TechName, NULL, NULL, NULL, NULL );
    }
    m_matProj->Pop();
    m_matView->Pop();
    PROFILE_SHADER_END;
  }
  else 
  { // HACK: otherwise terrain AO or fill lights does not work
    EF_SetState( 0 );
    EF_Commit();

/*
    PROFILE_FRAME(FX_ProcessAOTarget_SSAO);
    m_matProj->Push();
    m_matView->Push();
    m_matView->LoadIdentity();
    
    static CCryName TechName = "Deferred_SSAO_Pass";
    CShader *pSH( CShaderMan::m_ShaderAmbientOcclusion );
    uint nPasses = 0;         
    pSH->FXSetTechnique(TechName);
    pSH->FXBegin( &nPasses, (FEF_DONTSETSTATES|FEF_DONTSETTEXTURES));
    pSH->FXBeginPass( 0 );
    pSH->FXEndPass();
    pSH->FXEnd();

    m_matProj->Pop();
    m_matView->Pop();
    */
  }

  // make box for stencil passes
  static t_arrDeferredMeshIndBuff arrDeferredInds;
  static t_arrDeferredMeshVertBuff arrDeferredVerts;
	arrDeferredInds.resize(0);
	arrDeferredVerts.resize(0);
  CreateDeferredUnitBox(arrDeferredInds, arrDeferredVerts);

  // render terrain AO into Green channel
  if(CV_r_TerrainAO && m_TerrainAONodes.Count())
  {
    PROFILE_FRAME(FX_ProcessAOTarget_TerrainAO);
    FX_PrepareTerrainAOTarget(arrDeferredInds, arrDeferredVerts);
  }

  // render fill lights into Blue (positive) and Alpha (negative) channel
  if(CV_r_FillLights && m_RP.m_FillLights[SRendItem::m_RecurseLevel].Num())
  {
    PROFILE_FRAME(FX_ProcessAOTarget_FillLights);
    FX_ProcessFillLights(arrDeferredInds, arrDeferredVerts);
  }

  FX_PopRenderTarget(0);

  if(pTempRT) // blur tmp target into final AO mask
  {
    PROFILE_SHADER_START;
    FX_ShadowBlur(CV_r_SSAO_blurriness, pTempRT, CTexture::m_Text_AOTarget, max(2, CV_r_SSAO_blur));
    PROFILE_SHADER_END
  }

  SAFE_DELETE(pTempRT);

  for(int t=0; t<8; t++)
    SAFE_DELETE(arrDepthTempRT[t]);

  return true;
}

bool CD3D9Renderer::FX_PrepareTerrainAOTarget(t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
  Matrix44 mCurComposite = *(m_matView->GetTop()) * *(m_matProj->GetTop());

  for(int i=0; i<m_TerrainAONodes.Count(); i++)
  {
    if(CV_r_TerrainAO&1)
    { // stencil pass
      PROFILE_SHADER_START;
      CShader *pSH( CShaderMan::m_ShaderShadowMaskGen );
      uint nPasses = 0;         
      static CCryName TechName0 = "DeferredShadowPass";
      pSH->FXSetTechnique(TechName0);
      pSH->FXBegin( &nPasses, FEF_DONTSETSTATES );

      Vec3 vBoxMin = m_TerrainAONodes[i].stencilBox[0];
      Vec3 vBoxMax = m_TerrainAONodes[i].stencilBox[1];

      m_matView->Push();
      Matrix34 mLocal;
      mLocal.SetIdentity();
      mLocal.SetScale(vBoxMax-vBoxMin,vBoxMin);
      Matrix44 mLocalTransposed = GetTransposed44(Matrix44(mLocal));
      m_matView->MultMatrixLocal(&mLocalTransposed);

      FX_StencilCull(-1, indBuff, vertBuff, pSH);

      m_matView->Pop();
      pSH->FXEnd();
      PROFILE_SHADER_END;
    }

    if(CV_r_TerrainAO&2)
    { // terrain AO pass
      PROFILE_SHADER_START;

      m_matProj->Push();
      mathMatrixOrthoOffCenterLH( m_matProj->GetTop(), 0, 1, 0, 1, -1, 1 );
      m_matView->Push();
      m_matView->LoadIdentity();

      static CCryName TechName1 = "Deferred_TerrainAO_Pass";
      FX_DeferredShadowPassAO( TechName1, &m_TerrainAONodes[i], NULL, &mCurComposite );

      m_matProj->Pop();
      m_matView->Pop();
      PROFILE_SHADER_END;
    }
  }

  m_TerrainAONodes.Clear();

  return true;
}

bool CD3D9Renderer::FX_ProcessFillLights(t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
  Matrix44 mCurComposite = *(m_matView->GetTop()) * *(m_matProj->GetTop());

  for (int nLight=0; nLight<m_RP.m_FillLights[SRendItem::m_RecurseLevel].Num(); nLight++)
  {
    SFillLight * pLight = &m_RP.m_FillLights[SRendItem::m_RecurseLevel][nLight];

    if(CV_r_FillLightsMode&1)
    { // stencil pass
      PROFILE_SHADER_START;
      CShader *pSH( CShaderMan::m_ShaderShadowMaskGen );
      uint nPasses = 0;         
      static CCryName TechName0 = "DeferredShadowPass";
      pSH->FXSetTechnique(TechName0);
      pSH->FXBegin( &nPasses, FEF_DONTSETSTATES );

      Vec3 vBoxMin = pLight->m_vOrigin - Vec3(pLight->m_fRadius,pLight->m_fRadius,pLight->m_fRadius);
      Vec3 vBoxMax = pLight->m_vOrigin + Vec3(pLight->m_fRadius,pLight->m_fRadius,pLight->m_fRadius);

      m_matView->Push();
      Matrix34 mLocal;
      mLocal.SetIdentity();
      mLocal.SetScale(vBoxMax-vBoxMin,vBoxMin);
      Matrix44 mLocalTransposed = GetTransposed44(Matrix44(mLocal));
      m_matView->MultMatrixLocal(&mLocalTransposed);

      FX_StencilCull(-1, indBuff, vertBuff, pSH);

      m_matView->Pop();
      pSH->FXEnd();
      PROFILE_SHADER_END;
    }

    if(CV_r_FillLightsMode&2)
    { // light pass
      PROFILE_SHADER_START;

      m_matProj->Push();
      mathMatrixOrthoOffCenterLH( m_matProj->GetTop(), 0, 1, 0, 1, -1, 1 );
      m_matView->Push();
      m_matView->LoadIdentity();

      static CCryName TechName1 = "Deferred_FillLight_Pass";
      FX_DeferredShadowPassAO( TechName1, NULL, pLight, &mCurComposite );

      m_matProj->Pop();
      m_matView->Pop();
      PROFILE_SHADER_END;
    }

    if(CV_r_FillLightsDebug)
    {
      float fColor = pLight->bNegative ? (1.f - pLight->m_fIntensity*.2f) : pLight->m_fIntensity;
      GetIRenderAuxGeom()->DrawSphere(pLight->m_vOrigin,pLight->m_fRadius,ColorF(fColor,fColor,fColor,1));
      GetIRenderAuxGeom()->DrawSphere(pLight->m_vOrigin,.2f,ColorF(fColor,fColor,fColor,1));
      if(CV_r_FillLightsDebug==2)
        DrawLabel(pLight->m_vOrigin, 1.5f, "%s, Radius=%.1f, Intensity=%.1f", 
        pLight->bNegative ? "Negative" : "Positive", 
        pLight->m_fRadius, pLight->m_fIntensity);
    }
  }

  m_RP.m_FillLights[SRendItem::m_RecurseLevel].SetUse(0);

  return true;
}



// return number of vertices to add
int ClipEdge(const Vec3 & v1, const Vec3 & v2, const Plane & ClipPlane, Vec3 & vRes1, Vec3 & vRes2)
{
  float d1 = -ClipPlane.DistFromPlane(v1);
  float d2 = -ClipPlane.DistFromPlane(v2);
  if(d1<0 && d2<0)
    return 0; // all clipped = do not add any vertices

  if(d1>=0 && d2>=0)
  {
    vRes1 = v2;
    return 1; // both not clipped - add second vertex
  }

  // calculate new vertex
  Vec3 vIntersectionPoint = v1 + (v2-v1)*(Ffabs(d1)/(Ffabs(d2)+Ffabs(d1)));

#ifdef _DEBUG
  float fNewDist = -ClipPlane.DistFromPlane(vIntersectionPoint);
  assert(Ffabs(fNewDist)<0.01f);
#endif

  if(d1>=0 && d2<0)
  { // from vis to no vis
    vRes1 = vIntersectionPoint;
    return 1;
  }
  else if(d1<0 && d2>=0)
  { // from not vis to vis
    vRes1 = vIntersectionPoint;
    vRes2 = v2;
    return 2;
  }

  assert(0);
  return 0;
}

void ClipPolygon(PodArray<Vec3> * pPolygon, const Plane & ClipPlane)
{
  static PodArray<Vec3> PolygonOut; // Keep this list static to not perform reallocation every time.
  PolygonOut.Clear();
  // clip edges, make list of new vertices
  for(int i=0; i<pPolygon->Count(); i++)
  {
    Vec3 vNewVert1(0,0,0), vNewVert2(0,0,0);
    if(int nNewVertNum = ClipEdge(pPolygon->GetAt(i), pPolygon->GetAt((i+1)%pPolygon->Count()), ClipPlane, vNewVert1, vNewVert2))
    {
      PolygonOut.Add(vNewVert1);
      if(nNewVertNum>1)
        PolygonOut.Add(vNewVert2);
    }
  }

  // check result
  for(int i=0; i<PolygonOut.Count(); i++)
  {
    float d1 = -ClipPlane.DistFromPlane(PolygonOut.GetAt(i));
    assert(d1>=-0.01f);
  }

  assert(PolygonOut.Count()==0 || PolygonOut.Count() >= 3);

  pPolygon->Clear();
  pPolygon->AddList( PolygonOut );
}

void CheckTriangle(Vec3 * pVerts3d, int i1, int i2, int i3, const Plane & nearPlane, Vec3 & vMin, Vec3 & vMax, const Matrix44 & mViewProj)
{
  // make polygon
  static PodArray<Vec3> arrTriangle;
  arrTriangle.Clear();
  arrTriangle.Add(pVerts3d[i1]);
  arrTriangle.Add(pVerts3d[i2]);
  arrTriangle.Add(pVerts3d[i3]);

  // clip by near plane
  ClipPolygon(&arrTriangle, nearPlane);

  // check 2d bounds
  for(int i=0; i<arrTriangle.Count(); i++)
  {
    Vec4 vScreenPoint = Vec4(arrTriangle[i], 1.0) * mViewProj;

    //projection space clamping
    vScreenPoint.w = max(vScreenPoint.w, 0.00000000000001f);
    vScreenPoint.x = max(vScreenPoint.x, -(vScreenPoint.w));
    vScreenPoint.x = min(vScreenPoint.x, vScreenPoint.w);
    vScreenPoint.y = max(vScreenPoint.y, -(vScreenPoint.w));
    vScreenPoint.y = min(vScreenPoint.y, vScreenPoint.w);

    //NDC
    vScreenPoint /= vScreenPoint.w;

    //output coords
    //generate viewport (x=0,y=0,height=1,width=1)
    Vec3 vWin;
    vWin.x = (1 + vScreenPoint.x)/ 2;
    vWin.y = (1 + vScreenPoint.y)/ 2;  //flip coords for y axis
    vWin.z = vScreenPoint.z;

    assert(vWin.x>=0.0f && vWin.x<=1.0f);
    assert(vWin.y>=0.0f && vWin.y<=1.0f);

    vMin.x = min( vMin.x,vWin.x );
    vMin.y = min( vMin.y,vWin.y );
    vMin.z = min( vMin.z,vWin.z );
    
    vMax.x = max( vMax.x,vWin.x );
    vMax.y = max( vMax.y,vWin.y );
    vMax.z = max( vMax.z,vWin.z );
  }
}

// brute force clip and check all triangles of AABB, TODO: use edges
void ClipAABBByPlane(const AABB & objBox, const Plane & nearPlane, const Vec3 & vCamPos, Vec3 & vMin, Vec3 & vMax, const Matrix44 & mViewProj)
{
  Vec3 arrVerts3d[8] = 
  {
    Vec3(objBox.min.x,objBox.min.y,objBox.min.z),
    Vec3(objBox.min.x,objBox.max.y,objBox.min.z),
    Vec3(objBox.max.x,objBox.min.y,objBox.min.z),
    Vec3(objBox.max.x,objBox.max.y,objBox.min.z),
    Vec3(objBox.min.x,objBox.min.y,objBox.max.z),
    Vec3(objBox.min.x,objBox.max.y,objBox.max.z),
    Vec3(objBox.max.x,objBox.min.y,objBox.max.z),
    Vec3(objBox.max.x,objBox.max.y,objBox.max.z)
  };

  CheckTriangle(arrVerts3d, 2, 3, 6, nearPlane, vMin, vMax, mViewProj);
  CheckTriangle(arrVerts3d, 6, 3, 7, nearPlane, vMin, vMax, mViewProj);
  CheckTriangle(arrVerts3d, 1, 0, 4, nearPlane, vMin, vMax, mViewProj);
  CheckTriangle(arrVerts3d, 1, 4, 5, nearPlane, vMin, vMax, mViewProj);
  CheckTriangle(arrVerts3d, 3, 1, 7, nearPlane, vMin, vMax, mViewProj);
  CheckTriangle(arrVerts3d, 7, 1, 5, nearPlane, vMin, vMax, mViewProj);
  CheckTriangle(arrVerts3d, 0, 2, 4, nearPlane, vMin, vMax, mViewProj);
  CheckTriangle(arrVerts3d, 4, 2, 6, nearPlane, vMin, vMax, mViewProj);
  CheckTriangle(arrVerts3d, 5, 4, 6, nearPlane, vMin, vMax, mViewProj);
  CheckTriangle(arrVerts3d, 5, 6, 7, nearPlane, vMin, vMax, mViewProj);
  CheckTriangle(arrVerts3d, 0, 1, 2, nearPlane, vMin, vMax, mViewProj);
  CheckTriangle(arrVerts3d, 2, 1, 3, nearPlane, vMin, vMax, mViewProj);
}

void CD3D9Renderer::CalcAABBScreenRect(const AABB & aabb, const CRenderCamera& RCam, Matrix44& mViewProj, Vec3* pvMin,  Vec3* pvMax)
{
  * pvMin = Vec3(1.f,1.f,1.f);
  * pvMax = Vec3(0.f,0.f,0.f);

  ClipAABBByPlane(aabb, 
    *GetCamera().GetFrustumPlane(FR_PLANE_NEAR), GetCamera().GetPosition(), 
    *pvMin, *pvMax, mViewProj);
}
