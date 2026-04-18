/*=============================================================================
D3DFXPipeline.cpp : Direct3D specific FX shaders rendering pipeline.
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <I3DEngine.h>
#include <IEntityRenderState.h>

//====================================================================================

HRESULT CD3D9Renderer::FX_SetVStream(int nID, void *pB, uint nOffs, uint nStride, uint nFreq)
{
  D3DVertexBuffer *pVB = (D3DVertexBuffer *)pB;
  HRESULT h = S_OK;
  if (m_RP.m_VertexStreams[nID].pStream != pVB || m_RP.m_VertexStreams[nID].nOffset != nOffs)
  {
    m_RP.m_VertexStreams[nID].pStream = pVB;
    m_RP.m_VertexStreams[nID].nOffset = nOffs;
#if defined (DIRECT3D9) || defined(OPENGL)
    h = m_pd3dDevice->SetStreamSource(nID, pVB, nOffs, nStride);
#elif defined (DIRECT3D10)
    m_pd3dDevice->IASetVertexBuffers(nID, 1, &pVB, &nStride, &nOffs);
#endif
  }
#if defined (DIRECT3D9) || defined(OPENGL)
  if (nFreq != -1 && m_RP.m_VertexStreams[nID].nFreq != nFreq)
  {
    m_RP.m_VertexStreams[nID].nFreq = nFreq;
#if !defined(XENON) && !defined(PS3)
    h = m_pd3dDevice->SetStreamSourceFreq(nID, nFreq);
#endif
  }
#endif
  assert(h == S_OK);
  return h;
}
HRESULT CD3D9Renderer::FX_SetIStream(void* pB)
{
  D3DIndexBuffer *pIB = (D3DIndexBuffer *)pB;
  HRESULT h = S_OK;
  if (m_RP.m_pIndexStream != pIB)
  {
    m_RP.m_pIndexStream = pIB;
#if defined (DIRECT3D9) || defined(OPENGL)
    h = m_pd3dDevice->SetIndices(pIB);
#elif defined (DIRECT3D10)
    m_pd3dDevice->IASetIndexBuffer(pIB, DXGI_FORMAT_R16_UINT, 0);
#endif
    assert(h == S_OK);
  }
  return h;
}

HRESULT CD3D9Renderer::EF_SetVertexDeclaration(int StreamMask, int nVFormat)
{
  HRESULT hr;

  assert (nVFormat>=0 && nVFormat<VERTEX_FORMAT_NUMS);

  SD3DVertexDeclaration *pDecl = &m_RP.m_D3DVertexDeclaration[(StreamMask&0xff)>>1][nVFormat];

  if (StreamMask & VSM_MORPHBUDDY)
    pDecl = pDecl->m_pMorphDeclaration;
  if (!pDecl->m_pDeclaration)
  {
#if defined (DIRECT3D9) || defined(OPENGL)
    if(FAILED(hr = m_pd3dDevice->CreateVertexDeclaration(&pDecl->m_Declaration[0], &pDecl->m_pDeclaration)))
      return hr;
#elif defined (DIRECT3D10)
		//PS3HACK
//#if defined(PS3)
		if(!CHWShader_D3D::m_pCurInstVS || !CHWShader_D3D::m_pCurInstVS->m_pShaderData || CHWShader_D3D::m_pCurInstVS->m_bFallback)
			return -1;
    assert(CHWShader_D3D::m_pCurInstVS->m_pShaderData);
//#endif
    if(FAILED(hr = m_pd3dDevice->CreateInputLayout(&pDecl->m_Declaration[0], pDecl->m_Declaration.Num(), CHWShader_D3D::m_pCurInstVS->m_pShaderData, 
      CHWShader_D3D::m_pCurInstVS->m_nShaderByteCodeSize, &pDecl->m_pDeclaration)))
    {
      assert(SUCCEEDED(hr));
      return hr;
    }
#endif
  }
  if (m_pLastVDeclaration != pDecl->m_pDeclaration)
  {
#if defined (DIRECT3D9) || defined(OPENGL)
    m_pLastVDeclaration = pDecl->m_pDeclaration;
    return m_pd3dDevice->SetVertexDeclaration(pDecl->m_pDeclaration);
#elif defined (DIRECT3D10) 
    // Don't set input layout on fallback shader (crashes in DX10 NV driver)
    if (!CHWShader_D3D::m_pCurInstVS || CHWShader_D3D::m_pCurInstVS->m_bFallback)
       return -1;
    m_pLastVDeclaration = pDecl->m_pDeclaration;
    m_pd3dDevice->IASetInputLayout(pDecl->m_pDeclaration);
#endif
  }
#if defined (DIRECT3D10)
  // Don't render fallback in DX10
  if (!CHWShader_D3D::m_pCurInstVS || !CHWShader_D3D::m_pCurInstPS || CHWShader_D3D::m_pCurInstVS->m_bFallback || CHWShader_D3D::m_pCurInstPS->m_bFallback)
    return -1;
#endif
  return S_OK;
}

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
void CD3D9Renderer::FX_TagVStreamAsDirty(int nID)
{
	m_RP.m_VertexStreams[nID].pStream = NULL;
	m_RP.m_VertexStreams[nID].nOffset = -1;
	m_RP.m_VertexStreams[nID].nFreq = -1;
}

void CD3D9Renderer::FX_TagIStreamAsDirty()
{
	m_RP.m_pIndexStream = NULL;
}

void CD3D9Renderer::FX_TagVertexDeclarationAsDirty()
{
	m_pLastVDeclaration = NULL;
}
#endif

// Clear buffers (color, depth/stencil)
void CD3D9Renderer::EF_ClearBuffers(uint nFlags, const ColorF *Colors, float fDepth)
{
  if (nFlags & FRT_CLEAR_FOGCOLOR)
    m_pNewTarget[0]->m_ReqColor = m_cClearColor;
  else
  if (nFlags & FRT_CLEAR_COLOR)
  {
    if (Colors)
      m_pNewTarget[0]->m_ReqColor = *Colors;
    else
    if(m_polygon_mode==R_WIREFRAME_MODE)
      m_pNewTarget[0]->m_ReqColor = ColorF(0.25f,0.5f,1.0f,0);
    else
      m_pNewTarget[0]->m_ReqColor = m_cClearColor;
  }

  if (nFlags & FRT_CLEAR_DEPTH)
    m_pNewTarget[0]->m_fReqDepth = fDepth;

	if (!(nFlags & FRT_CLEAR_IMMEDIATE))
    m_pNewTarget[0]->m_ClearFlags = 0;
  if (nFlags & FRT_CLEAR_DEPTH)
    m_pNewTarget[0]->m_ClearFlags |= D3DCLEAR_ZBUFFER;
  if (nFlags & FRT_CLEAR_COLOR)
    m_pNewTarget[0]->m_ClearFlags |= D3DCLEAR_TARGET;
  if (m_sbpp && (nFlags & FRT_CLEAR_STENCIL))
    m_pNewTarget[0]->m_ClearFlags |= D3DCLEAR_STENCIL;

	if (nFlags & FRT_CLEAR_IMMEDIATE)
		FX_SetActiveRenderTargets(true);
}

void CD3D9Renderer::FX_ClearRegion()
{
#if defined (DIRECT3D10)
  CRenderObject *pObj = m_RP.m_pCurObject;
  CShader *pSHSave = m_RP.m_pShader;
  SShaderTechnique *pSHT = m_RP.m_pCurTechnique;
  SShaderPass *pPass = m_RP.m_pCurPass;
  SRenderShaderResources *pShRes = m_RP.m_pShaderResources;

  m_RP.m_PersFlags |= RBPF_IN_CLEAR;
  CShader *pSH = CShaderMan::m_ShaderCommon;
  uint nPasses = 0;
  pSH->FXSetTechnique("Clear");
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  pSH->FXBeginPass(0);
  int nState = GS_NODEPTHTEST;
  if (m_pNewTarget[0]->m_ClearFlags & (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL))
  {
    nState = GS_DEPTHFUNC_GREAT;
    nState &= ~GS_NODEPTHTEST;
    nState |= GS_DEPTHWRITE ;
  }

  EF_SetState(nState, -1);
  D3DSetCull(eCULL_None);
  float fX = (float)m_CurViewport.Width;
  float fY = (float)m_CurViewport.Height;
  DrawQuad(-0.5f, -0.5f, fX-0.5f, fY-0.5f, m_pNewTarget[0]->m_ReqColor, 1.0f/*m_pNewTarget[0]->m_fReqDepth*/, fX, fY, fX, fY);
  m_RP.m_PersFlags &= ~RBPF_IN_CLEAR;

  m_RP.m_pCurObject = pObj;
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  m_RP.m_pShader = pSHSave;
  m_RP.m_pCurTechnique = pSHT;
  m_RP.m_pCurPass = pPass;
  m_RP.m_pShaderResources = pShRes;
#endif
}

void CD3D9Renderer::FX_SetActiveRenderTargets(bool bAllowDIP)
{
  if (m_RP.m_PersFlags & RBPF_IN_CLEAR)
    return;
  HRESULT hr = S_OK;
  bool bDirty = false;
  if (m_nMaxRT2Commit >= 0)
  {
    for (int i=0; i<=m_nMaxRT2Commit; i++)
    {
      if (!m_pNewTarget[i]->m_bWasSetRT)
      {
        m_pNewTarget[i]->m_bWasSetRT = true;
        if (m_pNewTarget[i]->m_pTex)
          m_pNewTarget[i]->m_pTex->SetResolved(false);
        m_pCurTarget[i] = m_pNewTarget[i]->m_pTex;
        bDirty = true;
        if (m_LogFile)
        {
          Logv(SRendItem::m_RecurseLevel, " +++ Set RT");
          if (m_pNewTarget[i]->m_pTex)
          {
            Logv(SRendItem::m_RecurseLevel, " '%s'", m_pNewTarget[i]->m_pTex->GetName());
            Logv(SRendItem::m_RecurseLevel, " Format:%s", CTexture::NameForTextureFormat( m_pNewTarget[i]->m_pTex->m_eTFDst));
            Logv(SRendItem::m_RecurseLevel, " Type:%s", CTexture::NameForTextureType( m_pNewTarget[i]->m_pTex->m_eTT));
            Logv(SRendItem::m_RecurseLevel, " W/H:%d:%d\n",  m_pNewTarget[i]->m_pTex->GetWidth(), m_pNewTarget[i]->m_pTex->GetHeight());

          }
          else
          {
            Logv(SRendItem::m_RecurseLevel, " 'Unknown'\n");
          }
        }


#if defined (DIRECT3D9) || defined(OPENGL)
				{
					PROFILE_FRAME(SetRenderTarget);
					hr = m_pd3dDevice->SetRenderTarget(i, m_pNewTarget[i]->m_pTarget);
				}
        m_RP.m_PS.m_NumRTChanges++;
        CTexture *pRT = m_pNewTarget[i]->m_pTex;
        if (CV_r_stats == 11)
          EF_AddRTStat(pRT);
        if (pRT)
        {
          if (pRT->m_nRTSetFrameID != m_nFrameUpdateID)
          {
            pRT->m_nRTSetFrameID = m_nFrameUpdateID;
            m_RP.m_PS.m_NumRTs++;
            m_RP.m_PS.m_RTSize += pRT->GetDeviceDataSize();
            if (CV_r_stats == 12)
              EF_AddRTStat(pRT);
          }
        }
#endif
      }
    }
    if (!m_pNewTarget[0]->m_bWasSetD)
    {
      m_pNewTarget[0]->m_bWasSetD = true;
      bDirty = true;
#if defined (DIRECT3D9) || defined(OPENGL)
			{
				PROFILE_FRAME(SetDepthStencilSurface);
				hr = m_pd3dDevice->SetDepthStencilSurface(m_pNewTarget[0]->m_pDepth);
			}
#endif
    }
    m_nMaxRT2Commit = -1;
  }
#if defined (DIRECT3D10)
  assert(m_pNewTarget[0]->m_pDepth);
  if (bDirty)
  {
    if (m_pNewTarget[0]->m_pTex)
    {
      // Reset all texture slots which are used as RT currently
      ID3D10ShaderResourceView *pRes = NULL;
      for (int i=0; i<MAX_TMU; i++)
      {
        if (CTexture::m_TexStages[i].m_Texture == m_pNewTarget[0]->m_pTex)
        {
          m_pd3dDevice->PSSetShaderResources(i, 1, &pRes);
          CTexture::m_TexStages[i].m_Texture = NULL;
        }
      }
    }
    m_pd3dDevice->OMSetRenderTargets(m_pNewTarget[0]->m_pTarget==NULL?0:1, &m_pNewTarget[0]->m_pTarget, m_pNewTarget[0]->m_pDepth);
    /*D3D10_RENDER_TARGET_VIEW_DESC RTV;
    if (m_pNewTarget[0]->m_pTarget)
    {
      m_pNewTarget[0]->m_pTarget->GetDesc(&RTV);
      if (m_pNewTarget[0]->m_pTex)
      {
        D3DTexture *pT = (D3DTexture *)m_pNewTarget[0]->m_pTex->m_pDeviceTexture;
        if (pT)
        {
          D3D10_RESOURCE_DIMENSION Type;
          pT->GetType(&Type);
          D3D10_TEXTURE2D_DESC TX;
          pT->GetDesc(&TX);
        }
      }
    }*/
  }
#endif

  // Set current viewport
  if (m_bViewportDirty)
  {
    m_bViewportDirty = false;
    if (m_CurViewport != m_NewViewport)
    {
      m_CurViewport = m_NewViewport;
#if defined (DIRECT3D9) || defined(OPENGL)
      hr = m_pd3dDevice->SetViewport(&m_CurViewport);
#elif defined (DIRECT3D10)
      m_pd3dDevice->RSSetViewports(1, &m_CurViewport);
#endif
    }
  }
#ifdef XENON
  if (bDirty && CV_d3d9_predicatedtiling && m_pNewTarget[0]->m_pTex && (m_pNewTarget[0]->m_pTex->GetFlags() & FT_USAGE_PREDICATED_TILING))
  {
    D3DVECTOR4 clearColor;
    clearColor.x = m_pNewTarget[0]->m_ReqColor[0];
    clearColor.y = m_pNewTarget[0]->m_ReqColor[1];
    clearColor.z = m_pNewTarget[0]->m_ReqColor[2];
    clearColor.w = m_pNewTarget[0]->m_ReqColor[3];
    BeginPredicatedTiling(&clearColor);
  }

#endif
  if (m_pNewTarget[0]->m_ClearFlags)
  {
    //m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    DWORD cColor = D3DRGBA(m_pNewTarget[0]->m_ReqColor[0], m_pNewTarget[0]->m_ReqColor[1], m_pNewTarget[0]->m_ReqColor[2], m_pNewTarget[0]->m_ReqColor[3]);
#if defined (DIRECT3D9) || defined(OPENGL)
    hr = m_pd3dDevice->Clear(0, NULL, m_pNewTarget[0]->m_ClearFlags, cColor, m_pNewTarget[0]->m_fReqDepth, 0);
#elif defined (DIRECT3D10)
    bool bEntireClear = true;
   if  (m_pNewTarget[0]->m_pTex)
    {
      int nWidth = m_pNewTarget[0]->m_pTex->GetWidth();
      int nHeight = m_pNewTarget[0]->m_pTex->GetHeight();
      if (bAllowDIP && (m_CurViewport.TopLeftX || m_CurViewport.TopLeftY || m_CurViewport.Width!=nWidth || m_CurViewport.Height!=nHeight))
      {
        // Clear region
        FX_ClearRegion();
        bEntireClear = false;
      }
    }
    if (bEntireClear)
    {
      if ( (m_pNewTarget[0]->m_pTarget!=NULL) && m_pNewTarget[0]->m_ClearFlags & D3DCLEAR_TARGET)
        m_pd3dDevice->ClearRenderTargetView(m_pNewTarget[0]->m_pTarget, &m_pNewTarget[0]->m_ReqColor[0]);
      int nFlags = m_pNewTarget[0]->m_ClearFlags & ~D3DCLEAR_TARGET;
      if (nFlags == (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL))
      {
        m_pd3dDevice->ClearDepthStencilView(m_pNewTarget[0]->m_pDepth, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, m_pNewTarget[0]->m_fReqDepth, 0);
      }
      else
      if (nFlags == D3DCLEAR_ZBUFFER)
        m_pd3dDevice->ClearDepthStencilView(m_pNewTarget[0]->m_pDepth, D3D10_CLEAR_DEPTH, m_pNewTarget[0]->m_fReqDepth, 0);
      else
      if (nFlags == D3DCLEAR_STENCIL)
        m_pd3dDevice->ClearDepthStencilView(m_pNewTarget[0]->m_pDepth, D3D10_CLEAR_STENCIL, m_pNewTarget[0]->m_fReqDepth, 0);
      else
      if (nFlags)
      {
        assert(0);
      }
    }
#endif
    m_RP.m_PS.m_RTCleared++;
    CTexture *pRT = m_pNewTarget[0]->m_pTex;
    if (CV_r_stats == 13)
      EF_AddRTStat(pRT, m_pNewTarget[0]->m_ClearFlags, m_CurViewport.Width, m_CurViewport.Height);
    if (pRT)
    {
      if (m_pNewTarget[0]->m_ClearFlags & D3DCLEAR_TARGET)
        m_RP.m_PS.m_RTClearedSize += pRT->GetDeviceDataSize();
      if (m_pNewTarget[0]->m_ClearFlags & D3DCLEAR_ZBUFFER)
        m_RP.m_PS.m_RTClearedSize += pRT->GetWidth() * pRT->GetHeight() * 4;
    }
    else
    {
      if (m_pNewTarget[0]->m_ClearFlags & D3DCLEAR_TARGET)
        m_RP.m_PS.m_RTClearedSize += m_width * m_height * 4;
      if (m_pNewTarget[0]->m_ClearFlags & D3DCLEAR_ZBUFFER)
        m_RP.m_PS.m_RTClearedSize += m_width * m_height * 4;
      else
      if (m_pNewTarget[0]->m_ClearFlags & D3DCLEAR_STENCIL)
        m_RP.m_PS.m_RTClearedSize += m_width * m_height;
    }

    m_pNewTarget[0]->m_ClearFlags = 0;
  }
}

void CD3D9Renderer::EF_Commit(bool bAllowDIP)
{
  // Commit all changed shader parameters
  CHWShader_D3D::mfCommitParams(true);

  // Commit all changed RT's
  FX_SetActiveRenderTargets(bAllowDIP);
}

// Set current geometry culling modes
void CD3D9Renderer::D3DSetCull(ECull eCull)
{ 
  if (eCull != eCULL_None)
  {
    if (m_RP.m_PersFlags & RBPF_MIRRORCULL)
      eCull = (eCull == eCULL_Back) ? eCULL_Front : eCULL_Back;
  }

  if (eCull == m_RP.m_eCull)
    return;

#if defined (DIRECT3D9) || defined(OPENGL)
  if (eCull == eCULL_None)
    m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  else
  {
    if (eCull == eCULL_Back)
      m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
    else
      m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
  }
#elif defined (DIRECT3D10)
  SStateRaster RS = m_StatesRS[m_nCurStateRS];

  RS.Desc.FrontCounterClockwise = true;

  if (eCull == eCULL_None)
    RS.Desc.CullMode = D3D10_CULL_NONE;
  else
  {
    if (eCull == eCULL_Back)
    {
      RS.Desc.CullMode = D3D10_CULL_BACK;
    }
    else
    {
      RS.Desc.CullMode = D3D10_CULL_FRONT;
    }
  }
  SetRasterState(&RS);
#endif
  m_RP.m_eCull = eCull;
}

void CRenderer::EF_SetStencilState(int st, uint nStencRef, uint nStencMask, uint nStencWriteMask)
{
#if defined (DIRECT3D9) || defined(OPENGL)
  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
  if (nStencRef != m_RP.m_CurStencRef)
  {
    m_RP.m_CurStencRef = nStencRef;
    dv->SetRenderState(D3DRS_STENCILREF, nStencRef);
  }
  if (nStencMask != m_RP.m_CurStencMask)
  {
    m_RP.m_CurStencMask = nStencMask;
    dv->SetRenderState(D3DRS_STENCILMASK, nStencMask);
  }
  if (nStencWriteMask != m_RP.m_CurStencWriteMask)
  {
    m_RP.m_CurStencWriteMask = nStencWriteMask;
    dv->SetRenderState(D3DRS_STENCILWRITEMASK, nStencWriteMask);
  }

  int Changed = st ^ m_RP.m_CurStencilState;
  if (!Changed)
    return;
  if (Changed & FSS_STENCIL_TWOSIDED)
  {
    if (st & FSS_STENCIL_TWOSIDED)
      dv->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, TRUE);
    else
      dv->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
  }
  if (Changed & FSS_STENCFUNC_MASK)
  {
    int nCurFunc = st & FSS_STENCFUNC_MASK;
    switch(nCurFunc)
    {
    case FSS_STENCFUNC_ALWAYS:
      dv->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
      break;
    case FSS_STENCFUNC_NEVER:
      dv->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NEVER);
      break;
    case FSS_STENCFUNC_LESS:
      dv->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_LESS);
      break;
    case FSS_STENCFUNC_LEQUAL:
      dv->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL);
      break;
    case FSS_STENCFUNC_GREATER:
      dv->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_GREATER);
      break;
    case FSS_STENCFUNC_GEQUAL:
      dv->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_GREATEREQUAL);
      break;
    case FSS_STENCFUNC_EQUAL:
      dv->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
      break;
    case FSS_STENCFUNC_NOTEQUAL:
      dv->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL);
      break;
    default:
      assert(false);
    }
  }
  if (Changed & FSS_STENCFAIL_MASK)
  {
    int nCurOp = (st & FSS_STENCFAIL_MASK);
    switch(nCurOp >> FSS_STENCFAIL_SHIFT)
    {
    case FSS_STENCOP_KEEP:
      dv->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
      break;
    case FSS_STENCOP_REPLACE:
      dv->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_REPLACE);
      break;
    case FSS_STENCOP_INCR:
      dv->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_INCRSAT);
      break;
    case FSS_STENCOP_DECR:
      dv->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_DECRSAT);
      break;
    case FSS_STENCOP_INCR_WRAP:
      dv->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_INCR);
      break;
    case FSS_STENCOP_DECR_WRAP:
      dv->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_DECR);
      break;
    case FSS_STENCOP_ZERO:
      dv->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_ZERO);
      break;
    default:
      assert(false);
    }
  }
  if (Changed & FSS_STENCZFAIL_MASK)
  {
    int nCurOp = (st & FSS_STENCZFAIL_MASK);
    switch(nCurOp >> FSS_STENCZFAIL_SHIFT)
    {
    case FSS_STENCOP_KEEP:
      dv->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
      break;
    case FSS_STENCOP_REPLACE:
      dv->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_REPLACE);
      break;
    case FSS_STENCOP_INCR:
      dv->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_INCRSAT);
      break;
    case FSS_STENCOP_DECR:
      dv->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_DECRSAT);
      break;
    case FSS_STENCOP_INCR_WRAP:
      dv->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_INCR);
      break;
    case FSS_STENCOP_DECR_WRAP:
      dv->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_DECR);
      break;
    case FSS_STENCOP_ZERO:
      dv->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_ZERO);
      break;
    default:
      assert(false);
    }
  }
  if (Changed & FSS_STENCPASS_MASK)
  {
    int nCurOp = (st & FSS_STENCPASS_MASK);
    switch(nCurOp >> FSS_STENCPASS_SHIFT)
    {
    case FSS_STENCOP_KEEP:
      dv->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
      break;
    case FSS_STENCOP_REPLACE:
      dv->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
      break;
    case FSS_STENCOP_INCR:
      dv->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT);
      break;
    case FSS_STENCOP_DECR:
      dv->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_DECRSAT);
      break;
    case FSS_STENCOP_INCR_WRAP:
      dv->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INCR);
      break;
    case FSS_STENCOP_DECR_WRAP:
      dv->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_DECR);
      break;
    case FSS_STENCOP_ZERO:
      dv->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_ZERO);
      break;
    default:
      assert(false);
    }
  }

  if (Changed & (FSS_STENCFUNC_MASK << FSS_CCW_SHIFT))
  {
    int nCurFunc = (st & (FSS_STENCFUNC_MASK << FSS_CCW_SHIFT));
    switch(nCurFunc >> FSS_CCW_SHIFT)
    {
    case FSS_STENCFUNC_ALWAYS:
      dv->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS);
      break;
    case FSS_STENCFUNC_NEVER:
      dv->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_NEVER);
      break;
    case FSS_STENCFUNC_LESS:
      dv->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_LESS);
      break;
    case FSS_STENCFUNC_LEQUAL:
      dv->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_LESSEQUAL);
      break;
    case FSS_STENCFUNC_GREATER:
      dv->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_GREATER);
      break;
    case FSS_STENCFUNC_GEQUAL:
      dv->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_GREATEREQUAL);
      break;
    case FSS_STENCFUNC_EQUAL:
      dv->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_EQUAL);
      break;
    case FSS_STENCFUNC_NOTEQUAL:
      dv->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_NOTEQUAL);
      break;
    default:
      assert(false);
    }
  }
  if (Changed & (FSS_STENCFAIL_MASK << FSS_CCW_SHIFT))
  {
    int nCurOp = (st & (FSS_STENCFAIL_MASK << FSS_CCW_SHIFT));
    switch(nCurOp >> (FSS_STENCFAIL_SHIFT+FSS_CCW_SHIFT))
    {
    case FSS_STENCOP_KEEP:
      dv->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP);
      break;
    case FSS_STENCOP_REPLACE:
      dv->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_REPLACE);
      break;
    case FSS_STENCOP_INCR:
      dv->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_INCRSAT);
      break;
    case FSS_STENCOP_DECR:
      dv->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_DECRSAT);
      break;
    case FSS_STENCOP_INCR_WRAP:
      dv->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_INCR);
      break;
    case FSS_STENCOP_DECR_WRAP:
      dv->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_DECR);
      break;
    case FSS_STENCOP_ZERO:
      dv->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_ZERO);
      break;
    default:
      assert(false);
    }
  }
  if (Changed & (FSS_STENCZFAIL_MASK << FSS_CCW_SHIFT))
  {
    int nCurOp = (st & (FSS_STENCZFAIL_MASK << FSS_CCW_SHIFT));
    switch(nCurOp >> (FSS_STENCZFAIL_SHIFT+FSS_CCW_SHIFT))
    {
    case FSS_STENCOP_KEEP:
      dv->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_KEEP);
      break;
    case FSS_STENCOP_REPLACE:
      dv->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_REPLACE);
      break;
    case FSS_STENCOP_INCR:
      dv->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_INCRSAT);
      break;
    case FSS_STENCOP_DECR:
      dv->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_DECRSAT);
      break;
    case FSS_STENCOP_INCR_WRAP:
      dv->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_INCR);
      break;
    case FSS_STENCOP_DECR_WRAP:
      dv->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_DECR);
      break;
    case FSS_STENCOP_ZERO:
      dv->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_ZERO);
      break;
    default:
      assert(false);
    }
  }
  if (Changed & (FSS_STENCPASS_MASK << FSS_CCW_SHIFT))
  {
    int nCurOp = (st & (FSS_STENCPASS_MASK << FSS_CCW_SHIFT));
    switch(nCurOp >> (FSS_STENCPASS_SHIFT+FSS_CCW_SHIFT))
    {
    case FSS_STENCOP_KEEP:
      dv->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP);
      break;
    case FSS_STENCOP_REPLACE:
      dv->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_REPLACE);
      break;
    case FSS_STENCOP_INCR:
      dv->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_INCRSAT);
      break;
    case FSS_STENCOP_DECR:
      dv->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_DECRSAT);
      break;
    case FSS_STENCOP_INCR_WRAP:
      dv->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_INCR);
      break;
    case FSS_STENCOP_DECR_WRAP:
      dv->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_DECR);
      break;
    case FSS_STENCOP_ZERO:
      dv->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_ZERO);
      break;
    default:
      assert(false);
    }
  }
#elif defined (DIRECT3D10)
  SStateDepth DS = gcpRendD3D->m_StatesDP[gcpRendD3D->m_nCurStateDP];
  DS.Desc.StencilReadMask = nStencMask;
  DS.Desc.StencilWriteMask = nStencWriteMask;

  int nCurFunc = st & FSS_STENCFUNC_MASK;
  switch(nCurFunc)
  {
  case FSS_STENCFUNC_ALWAYS:
    DS.Desc.FrontFace.StencilFunc = D3D10_COMPARISON_ALWAYS;
    break;
  case FSS_STENCFUNC_NEVER:
    DS.Desc.FrontFace.StencilFunc = D3D10_COMPARISON_NEVER;
    break;
  case FSS_STENCFUNC_LESS:
    DS.Desc.FrontFace.StencilFunc = D3D10_COMPARISON_LESS;
    break;
  case FSS_STENCFUNC_LEQUAL:
    DS.Desc.FrontFace.StencilFunc = D3D10_COMPARISON_LESS_EQUAL;
    break;
  case FSS_STENCFUNC_GREATER:
    DS.Desc.FrontFace.StencilFunc = D3D10_COMPARISON_GREATER;
    break;
  case FSS_STENCFUNC_GEQUAL:
    DS.Desc.FrontFace.StencilFunc = D3D10_COMPARISON_GREATER_EQUAL;
    break;
  case FSS_STENCFUNC_EQUAL:
    DS.Desc.FrontFace.StencilFunc = D3D10_COMPARISON_EQUAL;
    break;
  case FSS_STENCFUNC_NOTEQUAL:
    DS.Desc.FrontFace.StencilFunc = D3D10_COMPARISON_NOT_EQUAL;
    break;
  default:
    assert(false);
  }

  int nCurOp = (st & FSS_STENCFAIL_MASK);
  switch(nCurOp >> FSS_STENCFAIL_SHIFT)
  {
  case FSS_STENCOP_KEEP:
    DS.Desc.FrontFace.StencilFailOp = D3D10_STENCIL_OP_KEEP;
    break;
  case FSS_STENCOP_REPLACE:
    DS.Desc.FrontFace.StencilFailOp = D3D10_STENCIL_OP_REPLACE;
    break;
  case FSS_STENCOP_INCR:
    DS.Desc.FrontFace.StencilFailOp = D3D10_STENCIL_OP_INCR_SAT;
    break;
  case FSS_STENCOP_DECR:
    DS.Desc.FrontFace.StencilFailOp = D3D10_STENCIL_OP_DECR_SAT;
    break;
  case FSS_STENCOP_INCR_WRAP:
    DS.Desc.FrontFace.StencilFailOp = D3D10_STENCIL_OP_INCR;
    break;
  case FSS_STENCOP_DECR_WRAP:
    DS.Desc.FrontFace.StencilFailOp = D3D10_STENCIL_OP_DECR;
    break;
  case FSS_STENCOP_ZERO:
    DS.Desc.FrontFace.StencilFailOp = D3D10_STENCIL_OP_ZERO;
    break;
  default:
    assert(false);
  }

  nCurOp = (st & FSS_STENCZFAIL_MASK);
  switch(nCurOp >> FSS_STENCZFAIL_SHIFT)
  {
  case FSS_STENCOP_KEEP:
    DS.Desc.FrontFace.StencilDepthFailOp = D3D10_STENCIL_OP_KEEP;
    break;
  case FSS_STENCOP_REPLACE:
    DS.Desc.FrontFace.StencilDepthFailOp = D3D10_STENCIL_OP_REPLACE;
    break;
  case FSS_STENCOP_INCR:
    DS.Desc.FrontFace.StencilDepthFailOp = D3D10_STENCIL_OP_INCR_SAT;
    break;
  case FSS_STENCOP_DECR:
    DS.Desc.FrontFace.StencilDepthFailOp = D3D10_STENCIL_OP_DECR_SAT;
    break;
  case FSS_STENCOP_INCR_WRAP:
    DS.Desc.FrontFace.StencilDepthFailOp = D3D10_STENCIL_OP_INCR;
    break;
  case FSS_STENCOP_DECR_WRAP:
    DS.Desc.FrontFace.StencilDepthFailOp = D3D10_STENCIL_OP_DECR;
    break;
  case FSS_STENCOP_ZERO:
    DS.Desc.FrontFace.StencilDepthFailOp = D3D10_STENCIL_OP_ZERO;
    break;
  default:
    assert(false);
  }

  nCurOp = (st & FSS_STENCPASS_MASK);
  switch(nCurOp >> FSS_STENCPASS_SHIFT)
  {
  case FSS_STENCOP_KEEP:
    DS.Desc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_KEEP;
    break;
  case FSS_STENCOP_REPLACE:
    DS.Desc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_REPLACE;
    break;
  case FSS_STENCOP_INCR:
    DS.Desc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_INCR_SAT;
    break;
  case FSS_STENCOP_DECR:
    DS.Desc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_DECR_SAT;
    break;
  case FSS_STENCOP_INCR_WRAP:
    DS.Desc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_INCR;
    break;
  case FSS_STENCOP_DECR_WRAP:
    DS.Desc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_DECR;
    break;
  case FSS_STENCOP_ZERO:
    DS.Desc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_ZERO;
    break;
  default:
    assert(false);
  }

  nCurFunc = (st & (FSS_STENCFUNC_MASK << FSS_CCW_SHIFT));
  switch(nCurFunc >> FSS_CCW_SHIFT)
  {
  case FSS_STENCFUNC_ALWAYS:
    DS.Desc.BackFace.StencilFunc = D3D10_COMPARISON_ALWAYS;
    break;
  case FSS_STENCFUNC_NEVER:
    DS.Desc.BackFace.StencilFunc = D3D10_COMPARISON_NEVER;
    break;
  case FSS_STENCFUNC_LESS:
    DS.Desc.BackFace.StencilFunc = D3D10_COMPARISON_LESS;
    break;
  case FSS_STENCFUNC_LEQUAL:
    DS.Desc.BackFace.StencilFunc = D3D10_COMPARISON_LESS_EQUAL;
    break;
  case FSS_STENCFUNC_GREATER:
    DS.Desc.BackFace.StencilFunc = D3D10_COMPARISON_GREATER;
    break;
  case FSS_STENCFUNC_GEQUAL:
    DS.Desc.BackFace.StencilFunc = D3D10_COMPARISON_GREATER_EQUAL;
    break;
  case FSS_STENCFUNC_EQUAL:
    DS.Desc.BackFace.StencilFunc = D3D10_COMPARISON_EQUAL;
    break;
  case FSS_STENCFUNC_NOTEQUAL:
    DS.Desc.BackFace.StencilFunc = D3D10_COMPARISON_NOT_EQUAL;
    break;
  default:
    assert(false);
  }

  nCurOp = (st & (FSS_STENCFAIL_MASK << FSS_CCW_SHIFT));
  switch(nCurOp >> (FSS_STENCFAIL_SHIFT+FSS_CCW_SHIFT))
  {
  case FSS_STENCOP_KEEP:
    DS.Desc.BackFace.StencilFailOp = D3D10_STENCIL_OP_KEEP;
    break;
  case FSS_STENCOP_REPLACE:
    DS.Desc.BackFace.StencilFailOp = D3D10_STENCIL_OP_REPLACE;
    break;
  case FSS_STENCOP_INCR:
    DS.Desc.BackFace.StencilFailOp = D3D10_STENCIL_OP_INCR_SAT;
    break;
  case FSS_STENCOP_DECR:
    DS.Desc.BackFace.StencilFailOp = D3D10_STENCIL_OP_DECR_SAT;
    break;
  case FSS_STENCOP_INCR_WRAP:
    DS.Desc.BackFace.StencilFailOp = D3D10_STENCIL_OP_INCR;
    break;
  case FSS_STENCOP_DECR_WRAP:
    DS.Desc.BackFace.StencilFailOp = D3D10_STENCIL_OP_DECR;
    break;
  case FSS_STENCOP_ZERO:
    DS.Desc.BackFace.StencilFailOp = D3D10_STENCIL_OP_ZERO;
    break;
  default:
    assert(false);
  }

  nCurOp = (st & (FSS_STENCZFAIL_MASK << FSS_CCW_SHIFT));
  switch(nCurOp >> (FSS_STENCZFAIL_SHIFT+FSS_CCW_SHIFT))
  {
  case FSS_STENCOP_KEEP:
    DS.Desc.BackFace.StencilDepthFailOp = D3D10_STENCIL_OP_KEEP;
    break;
  case FSS_STENCOP_REPLACE:
    DS.Desc.BackFace.StencilDepthFailOp = D3D10_STENCIL_OP_REPLACE;
    break;
  case FSS_STENCOP_INCR:
    DS.Desc.BackFace.StencilDepthFailOp = D3D10_STENCIL_OP_INCR_SAT;
    break;
  case FSS_STENCOP_DECR:
    DS.Desc.BackFace.StencilDepthFailOp = D3D10_STENCIL_OP_DECR_SAT;
    break;
  case FSS_STENCOP_INCR_WRAP:
    DS.Desc.BackFace.StencilDepthFailOp = D3D10_STENCIL_OP_INCR;
    break;
  case FSS_STENCOP_DECR_WRAP:
    DS.Desc.BackFace.StencilDepthFailOp = D3D10_STENCIL_OP_DECR;
    break;
  case FSS_STENCOP_ZERO:
    DS.Desc.BackFace.StencilDepthFailOp = D3D10_STENCIL_OP_ZERO;
    break;
  default:
    assert(false);
  }

  nCurOp = (st & (FSS_STENCPASS_MASK << FSS_CCW_SHIFT));
  switch(nCurOp >> (FSS_STENCPASS_SHIFT+FSS_CCW_SHIFT))
  {
  case FSS_STENCOP_KEEP:
    DS.Desc.BackFace.StencilPassOp = D3D10_STENCIL_OP_KEEP;
    break;
  case FSS_STENCOP_REPLACE:
    DS.Desc.BackFace.StencilPassOp = D3D10_STENCIL_OP_REPLACE;
    break;
  case FSS_STENCOP_INCR:
    DS.Desc.BackFace.StencilPassOp = D3D10_STENCIL_OP_INCR_SAT;
    break;
  case FSS_STENCOP_DECR:
    DS.Desc.BackFace.StencilPassOp = D3D10_STENCIL_OP_DECR_SAT;
    break;
  case FSS_STENCOP_INCR_WRAP:
    DS.Desc.BackFace.StencilPassOp = D3D10_STENCIL_OP_INCR;
    break;
  case FSS_STENCOP_DECR_WRAP:
    DS.Desc.BackFace.StencilPassOp = D3D10_STENCIL_OP_DECR;
    break;
  case FSS_STENCOP_ZERO:
    DS.Desc.BackFace.StencilPassOp = D3D10_STENCIL_OP_ZERO;
    break;
  default:
    assert(false);
  }

  gcpRendD3D->SetDepthState(&DS, nStencRef);
#endif
  m_RP.m_CurStencilState = st;
}

void CD3D9Renderer::EF_Scissor(bool bEnable, int sX, int sY, int sWdt, int sHgt)
{
  if (!CV_r_scissor)
    return;
#if defined (DIRECT3D9) || defined (OPENGL)
  RECT scRect;
  if (bEnable)
  {
    if (sX != m_sPrevX || sY != m_sPrevY || sWdt != m_sPrevWdt || sHgt != m_sPrevHgt)
    {
      m_sPrevX = sX;
      m_sPrevY = sY;
      m_sPrevWdt = sWdt;
      m_sPrevHgt = sHgt;
      scRect.left = sX;
      scRect.right = sX + sWdt;
      scRect.top = sY;
      scRect.bottom = sY + sHgt;
      m_pd3dDevice->SetScissorRect(&scRect);
    }
    if (bEnable != m_bsPrev)
    {
      m_bsPrev = bEnable;
      m_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    }
  }
  else
  {
    if (bEnable != m_bsPrev)
    {
      m_bsPrev = bEnable;
      m_sPrevWdt = 0;
      m_sPrevHgt = 0;
      m_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    }
  }
#elif defined (DIRECT3D10)
  D3D10_RECT scRect;
  if (bEnable)
  {
    if (sX != m_sPrevX || sY != m_sPrevY || sWdt != m_sPrevWdt || sHgt != m_sPrevHgt)
    {
      m_sPrevX = sX;
      m_sPrevY = sY;
      m_sPrevWdt = sWdt;
      m_sPrevHgt = sHgt;
      scRect.left = sX;
      scRect.right = sX + sWdt;
      scRect.top = sY;
      scRect.bottom = sY + sHgt;
      m_pd3dDevice->RSSetScissorRects(1, &scRect);
    }
    if (bEnable != m_bsPrev)
    {
      m_bsPrev = bEnable;
      SStateRaster RS = m_StatesRS[m_nCurStateRS];
      RS.Desc.ScissorEnable = bEnable;
      SetRasterState(&RS);
    }
  }
  else
  {
    if (bEnable != m_bsPrev)
    {
      m_bsPrev = bEnable;
      m_sPrevWdt = 0;
      m_sPrevHgt = 0;
      SStateRaster RS = m_StatesRS[m_nCurStateRS];
      RS.Desc.ScissorEnable = bEnable;
      SetRasterState(&RS);
    }
  }
#endif
}

int CD3D9Renderer::EF_FogCorrection()
{
  int bFogOverride = 0;
  switch (m_RP.m_CurState & GS_BLEND_MASK)
  {
  case GS_BLSRC_ONE | GS_BLDST_ONE:
    bFogOverride = 1;
    EF_SetFogColor(Col_Black);
    break;
  case GS_BLSRC_DSTALPHA | GS_BLDST_ONE:
    bFogOverride = 1;
    EF_SetFogColor(Col_Black);
    break;
  case GS_BLSRC_DSTCOL | GS_BLDST_SRCCOL:
    bFogOverride = 1;
    EF_SetFogColor(ColorF(0.5f, 0.5f, 0.5f, 1.0f));
    break;
  case GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA:
    bFogOverride = 1;
    EF_SetFogColor(Col_Black);
    break;
  case GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCCOL:
    bFogOverride = 1;
    EF_SetFogColor(Col_Black);
    break;
  case GS_BLSRC_ZERO | GS_BLDST_ONEMINUSSRCCOL:
    bFogOverride = 1;
    EF_SetFogColor(Col_Black);
    break;
  case GS_BLSRC_SRCALPHA | GS_BLDST_ONE:
    bFogOverride = 1;
    EF_SetFogColor(Col_Black);
    break;
  case GS_BLSRC_ZERO | GS_BLDST_ONE:
    bFogOverride = 1;
    EF_SetFogColor(Col_Black);
    break;
  case GS_BLSRC_DSTCOL | GS_BLDST_ZERO:
    bFogOverride = 1;
    EF_SetFogColor(Col_White);
    break;
  }
  return bFogOverride;
}

void CD3D9Renderer::EF_FogRestore(int bFogOverrided)
{
  if (bFogOverrided)
    EF_SetFogColor(m_FS.m_FogColor);
}

// Set current render states 
void CD3D9Renderer::EF_SetState(int st, int AlphaRef)
{
  int Changed;

  if (m_RP.m_Flags & RBF_SHOWLINES)
    st |= GS_NODEPTHTEST;
  if (m_pNewTarget[0] && m_pNewTarget[0]->m_bDontDraw)
    st |= GS_COLMASK_NONE;

  if (m_RP.m_PersFlags2 & RBPF2_DISABLECOLORWRITES)
  {
    st |= GS_COLMASK_NONE;
  }


  Changed = st ^ m_RP.m_CurState;
  if (!Changed && (AlphaRef==-1 || AlphaRef==m_RP.m_CurAlphaRef))
    return;

  //PROFILE_FRAME(State_RStates);

#if defined (DIRECT3D9) || defined(OPENGL)

  int src, dst;
  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
  m_RP.m_PS.m_NumStateChanges++;

  if (Changed & GS_DEPTHFUNC_MASK)
  {
    switch (st & GS_DEPTHFUNC_MASK)
    {
    case GS_DEPTHFUNC_EQUAL:
      dv->SetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
      break;
    case GS_DEPTHFUNC_LEQUAL:
      dv->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
      break;
    case GS_DEPTHFUNC_GREAT:
      dv->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATER);
      break;
    case GS_DEPTHFUNC_LESS:
      dv->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
      break;
    case GS_DEPTHFUNC_NOTEQUAL:
      dv->SetRenderState(D3DRS_ZFUNC, D3DCMP_NOTEQUAL);
      break;
    case GS_DEPTHFUNC_GEQUAL:
      dv->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATEREQUAL);
      break;
    }
  }
  
  if (Changed & GS_WIREFRAME)
  {
    if (m_polygon_mode == R_SOLID_MODE)
    {
      if (st & GS_WIREFRAME)
        dv->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);  
      else      
        dv->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    }
  }

	if (Changed & GS_COLMASK_MASK)
  {
		uint nMask = 0xfffffff0 | ((st & GS_COLMASK_MASK) >> GS_COLMASK_SHIFT);
		dv->SetRenderState(D3DRS_COLORWRITEENABLE, ~nMask);
	}
	if (Changed & GS_COLMASK_RT1)
	{
    if (st & GS_COLMASK_RT1)
			dv->SetRenderState(D3DRS_COLORWRITEENABLE1, 0xf);
		else
      dv->SetRenderState(D3DRS_COLORWRITEENABLE1, 0);
  }
  if (Changed & GS_COLMASK_RT2)
  {
		if (st & GS_COLMASK_RT2)
      dv->SetRenderState(D3DRS_COLORWRITEENABLE2, 0xf);
		else
      dv->SetRenderState(D3DRS_COLORWRITEENABLE2, 0);
  }
  if (Changed & GS_COLMASK_RT3)
  {
		if (st & GS_COLMASK_RT3)
      dv->SetRenderState(D3DRS_COLORWRITEENABLE3, 0xf);
		else
      dv->SetRenderState(D3DRS_COLORWRITEENABLE3, 0);
  }

  if (Changed & GS_BLEND_MASK)
  {
    if (m_RP.m_PersFlags2 & RBPF2_NOALPHABLEND)
    {
      st &= ~GS_BLEND_MASK;
    }
    else
    {
      if (st & GS_BLEND_MASK)
      {
        if (CV_r_measureoverdraw && (m_RP.m_nRendFlags & SHDF_ALLOWHDR))
        {
          st = (st & ~GS_BLEND_MASK) | (GS_BLSRC_ONE | GS_BLDST_ONE);
          st &= ~GS_ALPHATEST_MASK;
        }

        // Source factor
        switch (st & GS_BLSRC_MASK)
        {
        case GS_BLSRC_ZERO:
          src = D3DBLEND_ZERO;
          break;
        case GS_BLSRC_ONE:
          src = D3DBLEND_ONE;
          break;
        case GS_BLSRC_DSTCOL:
          src = D3DBLEND_DESTCOLOR;
          break;
        case GS_BLSRC_ONEMINUSDSTCOL:
          src = D3DBLEND_INVDESTCOLOR;
          break;
        case GS_BLSRC_SRCALPHA:
          src = D3DBLEND_SRCALPHA;
          break;
        case GS_BLSRC_ONEMINUSSRCALPHA:
          src = D3DBLEND_INVSRCALPHA;
          break;
        case GS_BLSRC_DSTALPHA:
          src = D3DBLEND_DESTALPHA;
          break;
        case GS_BLSRC_ONEMINUSDSTALPHA:
          src = D3DBLEND_INVDESTALPHA;
          break;
        case GS_BLSRC_ALPHASATURATE:
          src = D3DBLEND_SRCALPHASAT;
          break;
        default:
          iLog->Log("CD3D9Renderer::SetState: invalid src blend state bits '%d'", st & GS_BLSRC_MASK);
          break;
        }

        //Destination factor
        switch (st & GS_BLDST_MASK)
        {
        case GS_BLDST_ZERO:
          dst = D3DBLEND_ZERO;
          break;
        case GS_BLDST_ONE:
          dst = D3DBLEND_ONE;
          break;
        case GS_BLDST_SRCCOL:
          dst = D3DBLEND_SRCCOLOR;
          break;
        case GS_BLDST_ONEMINUSSRCCOL:
          dst = D3DBLEND_INVSRCCOLOR;
          if (m_nHDRType == 1 && (m_RP.m_PersFlags & RBPF_HDR))
            dst = D3DBLEND_ONE;
          break;
        case GS_BLDST_SRCALPHA:
          dst = D3DBLEND_SRCALPHA;
          break;
        case GS_BLDST_ONEMINUSSRCALPHA:
          dst = D3DBLEND_INVSRCALPHA;
          break;
        case GS_BLDST_DSTALPHA:
          dst = D3DBLEND_DESTALPHA;
          break;
        case GS_BLDST_ONEMINUSDSTALPHA:
          dst = D3DBLEND_INVDESTALPHA;
          break;
        default:
          iLog->Log("CD3D9Renderer::SetState: invalid dst blend state bits '%d'", st & GS_BLDST_MASK);
          break;
        }
        if (!(m_RP.m_CurState & GS_BLEND_MASK))
          dv->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dv->SetRenderState(D3DRS_SRCBLEND,  src);
        dv->SetRenderState(D3DRS_DESTBLEND, dst);
      }
      else
        dv->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    }
  }

  if (Changed & GS_DEPTHWRITE)
  {
    if (st & GS_DEPTHWRITE)
      dv->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    else
      dv->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  }

  if (Changed & GS_NODEPTHTEST)
  {
    if (st & GS_NODEPTHTEST)
      dv->SetRenderState(D3DRS_ZENABLE, FALSE);
    else
      dv->SetRenderState(D3DRS_ZENABLE, TRUE);
  }

  if (Changed & GS_STENCIL)
  {
    if (st & GS_STENCIL)
      dv->SetRenderState(D3DRS_STENCILENABLE, TRUE);
    else
      dv->SetRenderState(D3DRS_STENCILENABLE, FALSE);
  }

  if (!(m_RP.m_PersFlags2 & RBPF2_NOALPHATEST) || (m_RP.m_PersFlags2 & RBPF2_ATOC))
  {
#if !defined(XENON) && !defined(PS3)
    if (SRendItem::m_RecurseLevel == 1 && m_RP.m_nPassGroupID == EFSLIST_GENERAL && !(m_RP.m_nBatchFilter & (FB_Z|FB_GLOW)) && CV_r_usezpass)
    {
      if (m_RP.m_CurState & GS_ALPHATEST_MASK)
      {
        dv->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
      }  
      st &= ~GS_ALPHATEST_MASK;
    }
    else
#endif
    {
      if ((st & GS_ALPHATEST_MASK) && m_RP.m_CurAlphaRef != AlphaRef)
      {
        //assert(AlphaRef>=0 && AlphaRef<255);
        m_RP.m_CurAlphaRef = AlphaRef;
        dv->SetRenderState(D3DRS_ALPHAREF, AlphaRef);
      }
      if ((st ^ m_RP.m_CurState) & GS_ALPHATEST_MASK)
      {
        if (st & GS_ALPHATEST_MASK)
        {
          if (!(m_RP.m_CurState & GS_ALPHATEST_MASK))
            dv->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
          switch (st & GS_ALPHATEST_MASK)
          {
          case GS_ALPHATEST_GREATER:
            dv->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
            break;
          case GS_ALPHATEST_LESS:
            dv->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_LESS);
            break;
          case GS_ALPHATEST_GEQUAL:
            dv->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
            break;
          case GS_ALPHATEST_LEQUAL:
            dv->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
            break;
          }
        }
        else
          dv->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
      }
    }
  }
#elif defined (DIRECT3D10)
  m_RP.m_PS.m_NumStateChanges++;
  SStateDepth DS = m_StatesDP[m_nCurStateDP];
  SStateBlend BS = m_StatesBL[m_nCurStateBL];
  SStateRaster RS = m_StatesRS[m_nCurStateRS];
  bool bDirtyDS = false;
  bool bDirtyBS = false;
  bool bDirtyRS = false;

  if (Changed & GS_DEPTHFUNC_MASK)
  {
    bDirtyDS = true;
    switch (st & GS_DEPTHFUNC_MASK)
    {
    case GS_DEPTHFUNC_EQUAL:
      DS.Desc.DepthFunc = D3D10_COMPARISON_EQUAL;
      break;
    case GS_DEPTHFUNC_LEQUAL:
      DS.Desc.DepthFunc = D3D10_COMPARISON_LESS_EQUAL;
      break;
    case GS_DEPTHFUNC_GREAT:
      DS.Desc.DepthFunc = D3D10_COMPARISON_GREATER;
      break;
    case GS_DEPTHFUNC_LESS:
      DS.Desc.DepthFunc = D3D10_COMPARISON_LESS;
      break;
    case GS_DEPTHFUNC_NOTEQUAL:
      DS.Desc.DepthFunc = D3D10_COMPARISON_NOT_EQUAL;
      break;
    case GS_DEPTHFUNC_GEQUAL:
      DS.Desc.DepthFunc = D3D10_COMPARISON_GREATER_EQUAL;
      break;
    }
  }
  if (Changed & GS_WIREFRAME)
  {
    if (m_polygon_mode == R_SOLID_MODE)
    {
      bDirtyRS = true;
      if (st & GS_WIREFRAME)
        RS.Desc.FillMode = D3D10_FILL_WIREFRAME;
      else
        RS.Desc.FillMode = D3D10_FILL_SOLID;
    }
  }

  if (Changed & GS_COLMASK_MASK)
  {
    bDirtyBS = true;
    uint nMask = 0xfffffff0 | ((st & GS_COLMASK_MASK) >> GS_COLMASK_SHIFT);
    BS.Desc.RenderTargetWriteMask[0] = (~nMask) & 0xf;
  }
  if (Changed & GS_COLMASK_RT1)
  {
    bDirtyBS = true;
    if (st & GS_COLMASK_RT1)
      BS.Desc.RenderTargetWriteMask[1] = D3D10_COLOR_WRITE_ENABLE_ALL;
    else
      BS.Desc.RenderTargetWriteMask[1] = 0;
  }
  if (Changed & GS_COLMASK_RT2)
  {
    bDirtyBS = true;
    if (st & GS_COLMASK_RT2)
      BS.Desc.RenderTargetWriteMask[2] = D3D10_COLOR_WRITE_ENABLE_ALL;
    else
      BS.Desc.RenderTargetWriteMask[2] = 0;
  }
  if (Changed & GS_COLMASK_RT3)
  {
    bDirtyBS = true;
    if (st & GS_COLMASK_RT2)
      BS.Desc.RenderTargetWriteMask[3] = D3D10_COLOR_WRITE_ENABLE_ALL;
    else
      BS.Desc.RenderTargetWriteMask[3] = 0;
  }


  if (Changed & GS_BLEND_MASK)
  {
    if (m_RP.m_PersFlags2 & RBPF2_NOALPHABLEND)
    {
      st &= ~GS_BLEND_MASK;
    }
    else
    {
			bDirtyBS = true;
			if (st & GS_BLEND_MASK)
			{
				BS.Desc.BlendEnable[0] = TRUE;
				// Source factor
				switch (st & GS_BLSRC_MASK)
				{
				case GS_BLSRC_ZERO:
					BS.Desc.SrcBlend = D3D10_BLEND_ZERO;
					BS.Desc.SrcBlendAlpha = D3D10_BLEND_ZERO;
					break;
				case GS_BLSRC_ONE:
					BS.Desc.SrcBlend = D3D10_BLEND_ONE;
					BS.Desc.SrcBlendAlpha = D3D10_BLEND_ONE;
					break;
				case GS_BLSRC_DSTCOL:
					BS.Desc.SrcBlend = D3D10_BLEND_DEST_COLOR;
					BS.Desc.SrcBlendAlpha = D3D10_BLEND_DEST_ALPHA;
					break;
				case GS_BLSRC_ONEMINUSDSTCOL:
					BS.Desc.SrcBlend = D3D10_BLEND_INV_DEST_COLOR;
					BS.Desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
					break;
				case GS_BLSRC_SRCALPHA:
					BS.Desc.SrcBlend = D3D10_BLEND_SRC_ALPHA;
					BS.Desc.SrcBlendAlpha = D3D10_BLEND_SRC_ALPHA;
					break;
				case GS_BLSRC_ONEMINUSSRCALPHA:
					BS.Desc.SrcBlend = D3D10_BLEND_INV_SRC_ALPHA;
					BS.Desc.SrcBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
					break;
				case GS_BLSRC_DSTALPHA:
					BS.Desc.SrcBlend = D3D10_BLEND_DEST_ALPHA;
					BS.Desc.SrcBlendAlpha = D3D10_BLEND_DEST_ALPHA;
					break;
				case GS_BLSRC_ONEMINUSDSTALPHA:
					BS.Desc.SrcBlend = D3D10_BLEND_INV_DEST_ALPHA;
					BS.Desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
					break;
				case GS_BLSRC_ALPHASATURATE:
					BS.Desc.SrcBlend = D3D10_BLEND_SRC_ALPHA_SAT;
					BS.Desc.SrcBlendAlpha = D3D10_BLEND_SRC_ALPHA_SAT;
					break;
				default:
					iLog->Log("CD3D9Renderer::SetState: invalid src blend state bits '%d'", st & GS_BLSRC_MASK);
					break;
				}

				//Destination factor
				switch (st & GS_BLDST_MASK)
				{
				case GS_BLDST_ZERO:
					BS.Desc.DestBlend = D3D10_BLEND_ZERO;
					BS.Desc.DestBlendAlpha = D3D10_BLEND_ZERO;
					break;
				case GS_BLDST_ONE:
					BS.Desc.DestBlend = D3D10_BLEND_ONE;
					BS.Desc.DestBlendAlpha = D3D10_BLEND_ONE;
					break;
				case GS_BLDST_SRCCOL:
					BS.Desc.DestBlend = D3D10_BLEND_SRC_COLOR;
					BS.Desc.DestBlendAlpha = D3D10_BLEND_SRC_ALPHA;
					break;
				case GS_BLDST_ONEMINUSSRCCOL:
					if (m_nHDRType == 1 && (m_RP.m_PersFlags & RBPF_HDR))
					{
						BS.Desc.DestBlend = D3D10_BLEND_ONE;
						BS.Desc.DestBlendAlpha = D3D10_BLEND_ONE;
					}
					else
					{
						BS.Desc.DestBlend = D3D10_BLEND_INV_SRC_COLOR;
						BS.Desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
					}
					break;
				case GS_BLDST_SRCALPHA:
					BS.Desc.DestBlend = D3D10_BLEND_SRC_ALPHA;
					BS.Desc.DestBlendAlpha = D3D10_BLEND_SRC_ALPHA;
					break;
				case GS_BLDST_ONEMINUSSRCALPHA:
					BS.Desc.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
					BS.Desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
					break;
				case GS_BLDST_DSTALPHA:
					BS.Desc.DestBlend = D3D10_BLEND_DEST_ALPHA;
					BS.Desc.DestBlendAlpha = D3D10_BLEND_DEST_ALPHA;
					break;
				case GS_BLDST_ONEMINUSDSTALPHA:
					BS.Desc.DestBlend = D3D10_BLEND_INV_DEST_ALPHA;
					BS.Desc.DestBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
					break;
				default:
					iLog->Log("CD3D9Renderer::SetState: invalid dst blend state bits '%d'", st & GS_BLDST_MASK);
					break;
				}

				BS.Desc.BlendOp = D3D10_BLEND_OP_ADD;
				BS.Desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
      }
      else
        BS.Desc.BlendEnable[0] = FALSE;
    }
  }

  if (Changed & GS_DEPTHWRITE)
  {
    bDirtyDS = true;
    if (st & GS_DEPTHWRITE)
      DS.Desc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL;
    else
      DS.Desc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ZERO;
  }

  if (Changed & GS_NODEPTHTEST)
  {
    bDirtyDS = true;
    if (st & GS_NODEPTHTEST)
      DS.Desc.DepthEnable = FALSE;
    else
      DS.Desc.DepthEnable = TRUE;
  }

  if (Changed & GS_STENCIL)
  {
    bDirtyDS = true;
    if (st & GS_STENCIL)
      DS.Desc.StencilEnable = TRUE;
    else
      DS.Desc.StencilEnable = FALSE;
  }

  if (!(m_RP.m_PersFlags2 & RBPF2_NOALPHATEST) || (m_RP.m_PersFlags2 & RBPF2_ATOC))
  {
    // Alpha test must be handled in shader in D3D10 API
    if ((st ^ m_RP.m_CurState) & GS_ALPHATEST_MASK)
    {
      if (st & GS_ALPHATEST_MASK)
        m_RP.m_CurAlphaRef = AlphaRef;
    }
  }

	bool bCurATOC = BS.Desc.AlphaToCoverageEnable != 0;
	bool bNewATOC = ((st & GS_ALPHATEST_MASK) != 0) && ((m_RP.m_PersFlags2 & RBPF2_ATOC) != 0);
	bDirtyBS |= bNewATOC ^ bCurATOC;
	BS.Desc.AlphaToCoverageEnable = bNewATOC;

	if (bDirtyDS)
    SetDepthState(&DS, m_nCurStencRef);
  if (bDirtyRS)
    SetRasterState(&RS);
  if (bDirtyBS)
    SetBlendState(&BS);
#endif

  m_RP.m_CurState = st;
}

void CD3D9Renderer::FX_ZState(int& nState)
{
  // We cannot use z-prepass results with predicated tiling on Xenon
#ifdef XENON
  CONST TILING_SCENARIO& CurrentScenario = m_pTilingScenarios[m_dwTilingScenarioIndex];
  if (CurrentScenario.dwTileCount > 1)
    return;
#endif
  assert(m_RP.m_pRootTechnique);		// cannot be 0 here
  if (SRendItem::m_RecurseLevel == 1 && (m_RP.m_nBatchFilter & (FB_GENERAL|FB_MULTILAYERS|FB_RAIN)) && (m_RP.m_nRendFlags & (SHDF_ALLOWHDR | SHDF_ALLOWPOSTPROCESS)) && m_RP.m_nPassGroupID==EFSLIST_GENERAL && CV_r_usezpass && (m_RP.m_pRootTechnique->m_Flags & (FHF_WASZWRITE | FHF_POSITION_INVARIANT)))
  {
    if (!(m_RP.m_pRootTechnique->m_Flags & FHF_POSITION_INVARIANT))
      nState |= GS_DEPTHFUNC_EQUAL;
    nState &= ~(GS_DEPTHWRITE | GS_ALPHATEST_MASK);
  }

  if (SRendItem::m_RecurseLevel == 1 && (m_RP.m_nBatchFilter & FB_SCATTER) && m_RP.m_nPassGroupID==EFSLIST_GENERAL && CV_r_usezpass && (m_RP.m_pRootTechnique->m_Flags & (FHF_WASZWRITE | FHF_POSITION_INVARIANT)))
  {
    if (!(m_RP.m_pRootTechnique->m_Flags & FHF_POSITION_INVARIANT))
      nState |= GS_NODEPTHTEST;
    nState &= ~(GS_DEPTHWRITE | GS_ALPHATEST_MASK);
  }
}

void CD3D9Renderer::FX_CommitStates(SShaderTechnique *pTech, SShaderPass *pPass, bool bUseMaterialState)
{
  int State;
  int AlphaRef = pPass->m_AlphaRef == 0xff ? -1 : pPass->m_AlphaRef;
  
  if (m_RP.m_pCurObject->m_RState)
  {
    State = m_RP.m_pCurObject->m_RState;
    AlphaRef = m_RP.m_pCurObject->m_AlphaRef;
  }
  else
    State = pPass->m_RenderState;    

  if ( m_RP.m_PersFlags2 & RBPF2_LIGHTSTENCILCULL /*&& !(m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_AMBIENT] ) */)
  {
    State |= GS_STENCIL;
  }
  else
  {
    //reset stencil
    State &=~GS_STENCIL;
  }

  if (bUseMaterialState && m_RP.m_MaterialState != 0)
  {
    if (m_RP.m_MaterialState & GS_ALPHATEST_MASK)
      AlphaRef = m_RP.m_MaterialAlphaRef;

    // Reminder for Andrey: this will not work if zpass off
    if (m_RP.m_MaterialState & GS_BLEND_MASK)
      State = (State & ~(GS_BLEND_MASK | GS_DEPTHWRITE | GS_DEPTHFUNC_EQUAL)) | (m_RP.m_MaterialState & GS_BLEND_MASK);
    State = (State & ~GS_ALPHATEST_MASK) | (m_RP.m_MaterialState & GS_ALPHATEST_MASK);
  
    if (!(State & GS_ALPHATEST_MASK)) 
      State &= ~GS_DEPTHWRITE;
  }

  if (!(pTech->m_Flags & FHF_POSITION_INVARIANT) && !(pPass->m_PassFlags & SHPF_FORCEZFUNC))
    FX_ZState(State);
  if ((m_RP.m_pShader->m_Flags & EF_DECAL) && !(m_RP.m_FlagsShader_MDV & MDV_DEPTH_OFFSET))
    State |= GS_DEPTHFUNC_EQUAL;

  if (m_RP.m_pCurObject->m_fAlpha < 1.0f && !m_RP.m_bIgnoreObjectAlpha)
  {
    State &= ~(GS_BLEND_MASK | GS_DEPTHWRITE); 
    State |= (GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
  }

  // Specific condition for cloak transition
  if( (m_RP.m_pCurObject->m_nMaterialLayers&MTL_LAYER_BLEND_CLOAK) && !m_RP.m_bIgnoreObjectAlpha)
  {
    uint32 nResourcesNoDrawFlags = m_RP.m_pShaderResources->GetMtlLayerNoDrawFlags();
    if( !(nResourcesNoDrawFlags&MTL_LAYER_CLOAK) )
    {
      const float fCloakMinThreshold = 0.85f;
      if( (((m_RP.m_pCurObject->m_nMaterialLayers&MTL_LAYER_BLEND_CLOAK)>> 8) / 255.0f) > fCloakMinThreshold )
      {
        State &= ~(GS_BLEND_MASK|GS_DEPTHWRITE); 
        State |= (GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
      }
    }
  }

  //after the first pass we need to change the srcalpha-oneminusscralpha to scralpha-one
  if (m_RP.m_bNotFirstPass)
  {
    if ((State & GS_BLEND_MASK) == (GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA))
      State = (State & ~GS_BLEND_MASK) | GS_BLSRC_SRCALPHA | GS_BLDST_ONE;
    else
      State = (State & ~GS_BLEND_MASK) | GS_BLSRC_ONE | GS_BLDST_ONE;
  }

  if ( m_RP.m_pShader->m_Flags2 & EF2_HAIR && (m_RP.m_nPassGroupID==EFSLIST_GENERAL ||m_RP.m_nPassGroupID== EFSLIST_TRANSP ) && !(m_RP.m_PersFlags & (RBPF_SHADOWGEN|RBPF_ZPASS)))
  {
    // force per object fog
    m_RP.m_FlagsShader_RT |=(g_HWSR_MaskBit[HWSR_FOG]| g_HWSR_MaskBit[HWSR_ALPHABLEND]);
    if( pPass->m_RenderState & GS_DEPTHFUNC_LESS )
    {
      if( m_RP.m_bNotFirstPass )
      {
        State = (State & ~(GS_BLEND_MASK|GS_DEPTHFUNC_MASK|GS_DEPTHWRITE));
        if( pPass->m_RenderState & GS_DEPTHWRITE )
          State |=GS_BLSRC_SRCALPHA| GS_BLDST_ONE | GS_DEPTHFUNC_EQUAL;
        else
          State |= GS_BLSRC_SRCALPHA | GS_BLDST_ONE | GS_DEPTHFUNC_LEQUAL; //GS_BLSRC_ONESRCALPHA
      }
      else
      {
        State = (State & ~(GS_BLEND_MASK|GS_DEPTHFUNC_MASK));
        State |= GS_DEPTHFUNC_LESS | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
        
        if( pPass->m_RenderState & GS_DEPTHWRITE )
          State |= GS_DEPTHWRITE;
        else
          State &= ~GS_DEPTHWRITE;
      }
    }
    else
    {
      if( m_RP.m_bNotFirstPass ) 
      {
        State = (State & ~(GS_BLEND_MASK|GS_DEPTHFUNC_MASK|GS_DEPTHWRITE));
        State |= GS_BLSRC_ONE| GS_BLDST_ONE | GS_DEPTHFUNC_EQUAL; 
      }
      else
      {
        State = (State & ~(GS_BLEND_MASK|GS_DEPTHFUNC_MASK));
        State |= GS_DEPTHFUNC_LEQUAL | GS_DEPTHWRITE;
      }
    }
  }

	// Tbyte HACK
	if (m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_VERTEX_SCATTER])
	{
		SRenderObjData *pD = m_RP.m_pCurObject->GetObjData();
		if (pD && pD->m_pScatterBuffer)
			m_cEF.m_TempVecs[10] = pD->m_VertexScatterTransformZ;
	}

  //force no depth test for scattering passes
  if (SRendItem::m_RecurseLevel == 1 && (m_RP.m_nBatchFilter & FB_SCATTER))
  {
    if ((m_RP.m_FlagsPerFlush & RBSI_NOCULL) && (m_RP.m_PersFlags & RBPF_ZPASS)) //scattering z-only pass for proper skeleton visibility
    {
      State &= ~GS_BLEND_MASK;
      State |= GS_DEPTHWRITE;
      State &= ~GS_NODEPTHTEST;
      State |= GS_DEPTHFUNC_LEQUAL;

      State &= ~GS_ALPHATEST_MASK;

      State |= GS_COLMASK_NONE;
      //State |= GS_COLMASK_RGB;
    }
    else 
    if ((m_RP.m_FlagsPerFlush & RBSI_NOCULL) && !(m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_2SIDED)) //detect depth scattering case  
    {
      //depth estimation
      State |= GS_NODEPTHTEST;
      //State |= GS_NODEPTHTEST;
      State &= ~(GS_DEPTHWRITE | GS_ALPHATEST_MASK);
      State |= (GS_BLSRC_ONE | GS_BLDST_ONE);
      State |= GS_COLMASK_A;
    }
    else 
    {

      //internal RGB skeleton and all occluders rendering
      //note: should be drawn before all the transparent parts
      State &= ~GS_BLEND_MASK;
      if (m_RP.m_bNotFirstPass)
      {
          State |= GS_BLSRC_ONE | GS_BLDST_ONE;
      }
      State |= GS_DEPTHWRITE;
      //enable depth test
      State &= ~GS_NODEPTHTEST;
      //State |= GS_NODEPTHTEST;
      State |= GS_DEPTHFUNC_LEQUAL;

      //TD:: skeleton front faces depth should be rendered and RGB skeleton should be rendered during next pass
      //for proper depth estimation 
      //State |= GS_COLMASK_A;
      //D3DSetCull(eCULL_Front);
      //m_RP.m_FlagsPerFlush |= RBSI_NOCULL;

      //State |= GS_COLMASK_NONE;
      //for RGB occluders rendering
      State |= GS_COLMASK_RGB; 
    }
  }


#if defined (DIRECT3D9) || defined (OPENGL)
  if (m_NewViewport.MaxZ <= 0.01f)
#else
  if (m_NewViewport.MaxDepth <= 0.01f)
#endif
    State &= ~GS_DEPTHWRITE;

  if ((m_RP.m_MaterialState|State) & GS_ALPHATEST_MASK)
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];

  EF_SetState(State, AlphaRef);
  
  int nBlend;
  if (nBlend=(m_RP.m_CurState & GS_BLEND_MASK))
  {
    if (nBlend == (GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA) || nBlend == (GS_BLSRC_SRCALPHA | GS_BLDST_ONE) || nBlend == (GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA) )
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ALPHABLEND];
  }
}

//=====================================================================================

#ifdef XENON
//--------------------------------------------------------------------------------------
// Name: LargestTileRectSize()
// Desc: Returns the dimensions that fit the largest rectangle(s) out of the tiling
//       rectangles.
//--------------------------------------------------------------------------------------
void LargestTileRectSize(CONST TILING_SCENARIO& Scenario, D3DPOINT* pMaxSize)
{
  pMaxSize->x = 0;
  pMaxSize->y = 0;
  for(DWORD i=0; i<Scenario.dwTileCount; i++)
  {
    DWORD dwWidth = Scenario.TilingRects[i].x2 - Scenario.TilingRects[i].x1;
    DWORD dwHeight = Scenario.TilingRects[i].y2 - Scenario.TilingRects[i].y1;
    if(dwWidth > (DWORD)pMaxSize->x)
      pMaxSize->x = dwWidth;
    if(dwHeight > (DWORD)pMaxSize->y)
      pMaxSize->y = dwHeight;
  }
}
#endif
void CD3D9Renderer::FX_GetRTDimensions(bool bRTPredicated, int& nWidth, int& nHeight)
{
#ifdef XENON
  if (CV_d3d9_predicatedtiling && bRTPredicated)
  {
    CONST TILING_SCENARIO& CurrentScenario = m_pTilingScenarios[m_dwTilingScenarioIndex];

    // Find largest tiling rect size
    D3DPOINT LargestTileSize;
    LargestTileRectSize(CurrentScenario, &LargestTileSize);

    // Create color and depth/stencil rendertargets.
    // These rendertargets are where Predicated Tiling will render each tile.  Therefore,
    // these rendertargets should be set up with all rendering quality settings you desire,
    // such as multisample antialiasing.
    // Note how we use the dimension of the largest tile rectangle to define how big the
    // rendertargets are.
    switch (m_RP.m_FSAAData.Type)
    {
    case D3DMULTISAMPLE_NONE:
      nWidth = XGNextMultiple(LargestTileSize.x, GPU_EDRAM_TILE_WIDTH_1X);
      nHeight = XGNextMultiple(LargestTileSize.y, GPU_EDRAM_TILE_HEIGHT_1X);
      break;
    case D3DMULTISAMPLE_2_SAMPLES:
      nWidth = XGNextMultiple(LargestTileSize.x, GPU_EDRAM_TILE_WIDTH_2X);
      nHeight = XGNextMultiple(LargestTileSize.y, GPU_EDRAM_TILE_HEIGHT_2X);
      break;
    case D3DMULTISAMPLE_4_SAMPLES:
      nWidth = XGNextMultiple(LargestTileSize.x, GPU_EDRAM_TILE_WIDTH_4X);
      nHeight = XGNextMultiple(LargestTileSize.y, GPU_EDRAM_TILE_HEIGHT_4X);
      break;
    }

    // Expand tile surface dimensions to texture tile size
    nWidth  = XGNextMultiple(nWidth, GPU_TEXTURE_TILE_DIMENSION);
    nHeight = XGNextMultiple(nHeight, GPU_TEXTURE_TILE_DIMENSION);
  }
#endif
}

bool CD3D9Renderer::FX_GetTargetSurfaces(CTexture *pTarget, D3DSurface*& pTargSurf, SRTStack *pCur, int nCMSide)
{
  if (pTarget)
  {
    if (!pTarget->GetDeviceTexture())
      pTarget->CreateRenderTarget(eTF_Unknown);
#ifdef XENON
    D3DSURFACE_PARAMETERS Parms;
    Parms.Base = 0;
    Parms.HierarchicalZBase = 0;
    Parms.ColorExpBias = 0;
    int dwTileWidth = pTarget->GetWidth();
    int dwTileHeight = pTarget->GetHeight();
    FX_GetRTDimensions((pTarget->GetFlags() & FT_USAGE_PREDICATED_TILING) != 0, dwTileWidth, dwTileHeight);
    D3DFORMAT d3dFmt = (D3DFORMAT)pTarget->GetPixelFormat()->DeviceFormat;
    if (d3dFmt == D3DFMT_A16B16G16R16F)
      d3dFmt = D3DFMT_A2B10G10R10F_EDRAM;
    D3DMULTISAMPLE_TYPE d3dMS = D3DMULTISAMPLE_NONE;
    if (pTarget->GetFlags() & FT_USAGE_FSAA)
      d3dMS = m_RP.m_FSAAData.Type;

    //Params.Base = XGSurfaceSize(m_d3dsdBackBuffer.Width, m_d3dsdBackBuffer.Height, m_d3dsdBackBuffer.Format, D3DMULTISAMPLE_NONE);
    HRESULT hr = m_pd3dDevice->CreateRenderTarget(dwTileWidth, dwTileHeight, d3dFmt, d3dMS, 0L, FALSE, &pTargSurf, &Parms);
    assert (hr == S_OK);
#else
    if (!pTarget->GetDeviceTexture())
      return false;
#if defined (DIRECT3D9) || defined (OPENGL)
    pTargSurf = (D3DSurface *)pTarget->GetDeviceRT();
    if (pTargSurf)
      pTargSurf->AddRef();
    else
      pTargSurf = (D3DSurface *)pTarget->GetSurface(nCMSide, 0);
#elif defined (DIRECT3D10)
    pTargSurf = (D3DSurface *)pTarget->GetSurface(nCMSide, 0);
#endif

#endif
  }
  else
    pTargSurf = NULL;
  return true;
}

bool CD3D9Renderer::FX_SetRenderTarget(int nTarget, D3DSurface *pTargetSurf, SD3DSurface *pDepthTarget, bool bClearOnResolve)
{
  if (m_nRTStackLevel[nTarget] >= MAX_RT_STACK)
    return false;
  HRESULT hr = 0;
  SRTStack *pCur = &m_RTStack[nTarget][m_nRTStackLevel[nTarget]];
  pCur->m_pTarget = pTargetSurf;
  pCur->m_pSurfDepth = pDepthTarget;
  pCur->m_pDepth = (D3DDepthSurface *)pDepthTarget->pSurf;
  pCur->m_pTex = NULL;

#ifdef _DEBUG
  if (m_nRTStackLevel[nTarget] == 0 && nTarget == 0)
  {
    assert(pCur->m_pTarget == m_pBackBuffer && pCur->m_pDepth == m_pZBuffer);
  }
#endif

  pCur->m_bNeedReleaseRT = false;
  pCur->m_bWasSetRT = false;
  pCur->m_bWasSetD = false;
  pCur->m_bClearOnResolve = bClearOnResolve;
  pCur->m_ClearFlags = 0;
  m_pNewTarget[nTarget] = pCur;
  m_nMaxRT2Commit = max(m_nMaxRT2Commit, nTarget);
#ifdef XENON
  D3DSURFACE_DESC dtdsdRT;
  pTargetSurf->GetDesc(&dtdsdRT);
#endif

  return (hr == S_OK);
}
bool CD3D9Renderer::FX_PushRenderTarget(int nTarget, D3DSurface *pTargetSurf, SD3DSurface *pDepthTarget, bool bClearOnResolve)
{
  if (m_nRTStackLevel[nTarget] >= MAX_RT_STACK)
    return false;
  m_nRTStackLevel[nTarget]++;
  return FX_SetRenderTarget(nTarget, pTargetSurf, pDepthTarget, bClearOnResolve);
}

bool CD3D9Renderer::FX_SetRenderTarget(int nTarget, CTexture *pTarget, SD3DSurface *pDepthTarget, bool bPush, bool bClearOnResolve, int nCMSide)
{
  assert(!nTarget || !pDepthTarget);

  if (pTarget && pDepthTarget)
  {
    if (pTarget->GetWidth() > pDepthTarget->nWidth || pTarget->GetHeight() > pDepthTarget->nHeight)
    {
      iLog->LogError("Error: RenderTarget '%s' size:%i x %i DepthSurface size:%i x %i \n", pTarget->GetName(), pTarget->GetWidth(), pTarget->GetHeight(), pDepthTarget->nWidth, pDepthTarget->nHeight);
    }
    assert(pTarget->GetWidth() <= pDepthTarget->nWidth);
    assert(pTarget->GetHeight() <= pDepthTarget->nHeight);
  }

  if (m_nRTStackLevel[nTarget] >= MAX_RT_STACK)
    return false;

  SRTStack *pCur = &m_RTStack[nTarget][m_nRTStackLevel[nTarget]];
  D3DSurface* pTargSurf;
  if (pCur->m_pTex)
  {
    if (pCur->m_bNeedReleaseRT)
    {
      pCur->m_bNeedReleaseRT = false;
#ifdef XENON
      if (pCur->m_bWasSetRT)
        pCur->m_pTex->Resolve();
#endif
    }
    if (pCur->m_pTarget && pCur->m_pTarget == m_pNewTarget[0]->m_pTarget)
    {
#if defined (XENON)
      HRESULT hr = m_pd3dDevice->SetRenderTarget(0, NULL);
#endif
    }
    m_pNewTarget[0]->m_bWasSetRT = false;
#if defined (DIRECT3D9) || defined (OPENGL)
    SAFE_RELEASE(pCur->m_pTarget);
#endif
    m_pNewTarget[0]->m_pTarget = NULL;

    pCur->m_pTex->Unlock();
  }

#if defined (DIRECT3D10)
  if (!pTarget)
    pTargSurf = NULL;
  else
  {
    if (!FX_GetTargetSurfaces(pTarget, pTargSurf, pCur, nCMSide))
      return false;
  }
#else
  if (!pTarget)
    return false;
  if (!FX_GetTargetSurfaces(pTarget, pTargSurf, pCur, nCMSide))
    return false;
#endif

  if (pTarget)
  {
    int nFrameID = m_nFrameUpdateID;
    if (pTarget && pTarget->m_nUpdateFrameID != nFrameID)
    {
      pTarget->m_nUpdateFrameID = nFrameID;
    }
  }

  if (!bPush && pDepthTarget && pDepthTarget->pSurf != pCur->m_pDepth)
  {
    //assert(pCur->m_pDepth == m_pCurDepth);
    //assert(pCur->m_pDepth != m_pZBuffer);   // Attempt to override default Z-buffer surface
    if (pCur->m_pSurfDepth)
      pCur->m_pSurfDepth->bBusy = false;
  }
  pCur->m_pDepth = pDepthTarget ? (D3DDepthSurface *)pDepthTarget->pSurf : NULL;
  pCur->m_ClearFlags = 0;
  pCur->m_pTarget = pTargSurf;
  pCur->m_bNeedReleaseRT = true;
  pCur->m_bWasSetRT = false;
  pCur->m_bWasSetD = false;
  pCur->m_bClearOnResolve = bClearOnResolve;
  if (pTarget)
    pTarget->Lock();
  if (pDepthTarget)
  {
    pDepthTarget->bBusy = true;
    pDepthTarget->nFrameAccess = m_nFrameUpdateID;
  }

  if (pTarget)
    pCur->m_pTex = pTarget;
  else
    pCur->m_pTex = (CTexture*)pDepthTarget->pTex;

  pCur->m_pSurfDepth = pDepthTarget;

  if (pTarget)
  {
    pCur->m_Width = pTarget->GetWidth();
    pCur->m_Height = pTarget->GetHeight();
  }
  else 
    if (pDepthTarget)
    {
      pCur->m_Width = pDepthTarget->nWidth;
      pCur->m_Height = pDepthTarget->nHeight;
    }
    if (!nTarget)
    {
#if defined (DIRECT3D9) || defined (OPENGL)
      m_CurViewport.Width = pCur->m_Width;
      m_CurViewport.Height = pCur->m_Height;
      //m_CurViewport.MinZ = 0;
      //m_CurViewport.MaxZ = 1;
#endif
      SetViewport(0, 0, pCur->m_Width, pCur->m_Height);
    }
    m_pNewTarget[nTarget] = pCur;
    m_nMaxRT2Commit = max(m_nMaxRT2Commit, nTarget);

    return true;
}
bool CD3D9Renderer::FX_PushRenderTarget(int nTarget, CTexture *pTarget, SD3DSurface *pDepthTarget, bool bClearOnResolve, int nCMSide)
{
  if (m_nRTStackLevel[nTarget] == MAX_RT_STACK)
  {
    assert(0);
    return false;
  }
  m_nRTStackLevel[nTarget]++;
  return FX_SetRenderTarget(nTarget, pTarget, pDepthTarget, true, bClearOnResolve, nCMSide);
}

bool CD3D9Renderer::FX_RestoreRenderTarget(int nTarget)
{
  if (m_nRTStackLevel[nTarget] < 0)
    return false;

  SRTStack *pCur = &m_RTStack[nTarget][m_nRTStackLevel[nTarget]];
#ifdef _DEBUG
  if (m_nRTStackLevel[nTarget] == 0 && nTarget == 0)
  {
    assert(pCur->m_pTarget == m_pBackBuffer && pCur->m_pDepth == m_pZBuffer);
  }
#endif

  SRTStack *pPrev = &m_RTStack[nTarget][m_nRTStackLevel[nTarget]+1];
  if (pPrev->m_bNeedReleaseRT)
  {
    pPrev->m_bNeedReleaseRT = false;
#ifdef XENON
    if (pPrev->m_pTex && pPrev->m_bWasSetRT)
      pPrev->m_pTex->Resolve();
#endif
    if (pPrev->m_pTarget && pPrev->m_pTarget == m_pNewTarget[0]->m_pTarget)
    {
      m_pNewTarget[0]->m_bWasSetRT = false;
#ifdef XENON
      HRESULT hr = m_pd3dDevice->SetRenderTarget(0, NULL);
#endif
#if defined (DIRECT3D9) || defined (OPENGL)
      pPrev->m_pTarget->Release();
#endif
      pPrev->m_pTarget = NULL;
      m_pNewTarget[0]->m_pTarget = NULL;
    }
  }

  if (nTarget == 0)
  {
    if (pPrev->m_pSurfDepth)
    {
      pPrev->m_pSurfDepth->bBusy = false;
      pPrev->m_pSurfDepth = NULL;
    }
  }
  if (pPrev->m_pTex)
  {
    pPrev->m_pTex->Unlock();
    pPrev->m_pTex = NULL;
  }
  if (!nTarget)
  {
#if defined (DIRECT3D9) || defined (OPENGL)
    m_CurViewport.Width = pCur->m_Width;
    m_CurViewport.Height = pCur->m_Height;
#endif
    if (!m_nRTStackLevel[nTarget])
      SetViewport(0, 0, GetWidth(), GetHeight());
    else
      SetViewport(0, 0, pCur->m_Width, pCur->m_Height);
  }
  pCur->m_bWasSetD = false;
  pCur->m_bWasSetRT = false;
  m_pNewTarget[nTarget] = pCur;
  m_nMaxRT2Commit = max(m_nMaxRT2Commit, nTarget);

  return true;
}
bool CD3D9Renderer::FX_PopRenderTarget(int nTarget)
{
  if (m_nRTStackLevel[nTarget] <= 0)
  {
    assert(0);
    return false;
  }
  m_nRTStackLevel[nTarget]--;
  return FX_RestoreRenderTarget(nTarget);
}
SD3DSurface *CD3D9Renderer::FX_GetDepthSurface(int nWidth, int nHeight, bool bAA)
{
  SD3DSurface *pSrf = NULL;
  uint i;
  int nBestX = -1;
  int nBestY = -1;
  for (i=0; i<m_TempDepths.Num(); i++)
  {
    pSrf = m_TempDepths[i];
    if (!pSrf->bBusy)
    {
      if (pSrf->nWidth == nWidth && pSrf->nHeight == nHeight)
      {
        nBestX = i;
        break;
      }
#if !defined(OPENGL)
      // Need an exact match for OpenGL.
      if (nBestX < 0 && pSrf->nWidth == nWidth && pSrf->nHeight >= nHeight)
        nBestX = i;
      else
        if (nBestY < 0 && pSrf->nWidth >= nWidth && pSrf->nHeight == nHeight)
          nBestY = i;
#endif
    }
  }
  if (nBestX >= 0)
    return m_TempDepths[nBestX];
  if (nBestY >= 0)
    return m_TempDepths[nBestY];

#if !defined(PS3)
  for (i=0; i<m_TempDepths.Num(); i++)
  {
    pSrf = m_TempDepths[i];
    if (pSrf->nWidth >= nWidth && pSrf->nHeight >= nHeight && !pSrf->bBusy)
      break;
  }
#else
  i = m_TempDepths.Num();
#endif
  if (i == m_TempDepths.Num())
  {
    pSrf = new SD3DSurface;
    pSrf->nWidth = nWidth;
    pSrf->nHeight = nHeight;
    pSrf->nFrameAccess = -1;
    pSrf->bBusy = false;

#ifdef XENON
    D3DSURFACE_PARAMETERS Parms;
    Parms.Base = (5*1024*1024) / GPU_EDRAM_TILE_SIZE;
    Parms.HierarchicalZBase = 0;
    Parms.ColorExpBias = 0;
    m_pd3dDevice->CreateDepthStencilSurface(nWidth, nHeight, m_ZFormat, D3DMULTISAMPLE_NONE, 0, FALSE, (LPDIRECT3DSURFACE9 *)(&pSrf->pSurf), &Parms);
#elif defined (DIRECT3D9) || defined(OPENGL)
    m_pd3dDevice->CreateDepthStencilSurface(nWidth, nHeight, m_ZFormat, D3DMULTISAMPLE_NONE, 0, FALSE, (LPDIRECT3DSURFACE9 *)(&pSrf->pSurf), NULL);
#elif defined (DIRECT3D10)
    HRESULT hr;
    D3D10_TEXTURE2D_DESC descDepth;
    ZeroStruct(descDepth);
    descDepth.Width = nWidth;
    descDepth.Height = nHeight;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = m_ZFormat;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D10_USAGE_DEFAULT;
    descDepth.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    hr = m_pd3dDevice->CreateTexture2D(&descDepth,       // Texture desc
      NULL,                  // Initial data
      (ID3D10Texture2D **)(&pSrf->pTex)); // [out] Texture
    assert(hr == S_OK);

    D3D10_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroStruct(descDSV);
    descDSV.Format = m_ZFormat;
    descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;

    // Create the depth stencil view
    hr = m_pd3dDevice->CreateDepthStencilView((ID3D10Texture2D *)pSrf->pTex, // Depth stencil texture
      &descDSV, // Depth stencil desc
      (ID3D10DepthStencilView **)(&pSrf->pSurf));  // [out] Depth stencil view
#endif
    m_TempDepths.AddElem(pSrf);
  }

  return pSrf;
}
SD3DSurface *CD3D9Renderer::FX_GetScreenDepthSurface(bool bAA)
{
  SD3DSurface *pSurf = FX_GetDepthSurface(m_d3dsdBackBuffer.Width, m_d3dsdBackBuffer.Height, bAA);
  assert(pSurf);
  return pSurf;
}


//============================================================================================
_inline void sCopyInds8(uint *dinds, uint *inds, int nInds8, int n)
{
  if (!nInds8)
    return;
#ifdef DO_ASM
  _asm
  {
    push       ebx
    mov        edi, dinds
    mov        esi, inds
    mov        ecx, nInds8
    mov        eax, n
align 4
_Loop:
    prefetchT0  [esi+10h]
    mov        edx, [esi]
    add        edx, eax
    mov        [edi], edx
    mov        ebx, [esi+4]
    add        edi, 16
    add        ebx, eax
    mov        [edi+4-16], ebx
    mov        edx, [esi+8]
    add        edx, eax
    add        esi, 16
    mov        [edi+8-16], edx
    mov        ebx, [esi+12-16]
    add        ebx, eax
    dec        ecx
    mov        [edi+12-16], ebx
    jne        _Loop
    pop        ebx
  }
#else
  for (int i=0; i<nInds8; i++, dinds+=4, inds+=4)
  {
    dinds[0] = inds[0] + n;
    dinds[1] = inds[1] + n;
    dinds[2] = inds[2] + n;
    dinds[3] = inds[3] + n;
  }
#endif
}

void sCopyTransf_P_C_T(byte *dst, Matrix44 *mat, int nNumVerts, byte *OffsP)
{
#ifdef DO_ASM
  _asm
  {
    mov         eax, mat
    mov         ecx, nNumVerts;
    movaps      xmm2,xmmword ptr [eax]
    mov         esi, OffsP
    movaps      xmm4,xmmword ptr [eax+10h]
    movaps      xmm6,xmmword ptr [eax+20h]
    mov         edi, dst
    movaps      xmm5,xmmword ptr [eax+30h]
align 16
_Loop1:
    prefetchT0  [esi+24]
    movlps      xmm1,qword ptr [esi]
    movss       xmm0,dword ptr [esi+8]
    shufps      xmm0,xmm0,0
    add         edi, 24
    movaps      xmm3,xmm1
    mulps       xmm0,xmm6
    mov         eax, [esi+12]
    shufps      xmm3,xmm1,55h
    mulps       xmm3,xmm4
    mov         [edi+12-24], eax
    shufps      xmm1,xmm1,0
    mulps       xmm1,xmm2
    mov         eax, [esi+16]
    addps       xmm3,xmm1
    mov         [edi+16-24], eax
    addps       xmm3,xmm0
    mov         eax, [esi+20]
    addps       xmm3,xmm5
    add         esi, 24
    movhlps     xmm1,xmm3
    movlps      qword ptr [edi-24],xmm3
    dec         ecx
    mov         [edi+20-24], eax
    movss       dword ptr [edi+8-24],xmm1
    jne         _Loop1
  }
#else
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pDst = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)dst;
  for (int i=0; i<nNumVerts; i++, OffsP+=sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F))
  {
    //pDst->xyz = mat->TransformPoint(*(Vec3 *)OffsP);
    TransformPosition(pDst->xyz, *(Vec3 *)OffsP, *mat);
    pDst->color.dcolor = *(DWORD *)&OffsP[12];
    pDst->st[0] = *(float *)&OffsP[16];
    pDst->st[1] = *(float *)&OffsP[20];
    pDst++;
  }
#endif
}

void sCopyTransf_P_T(byte *dst, Matrix44 *mat, int nNumVerts, byte *OffsP)
{
#ifdef DO_ASM
  _asm
  {
    mov         eax, mat
    mov         ecx, nNumVerts;
    movaps      xmm2,xmmword ptr [eax]
    mov         esi, OffsP
    movaps      xmm4,xmmword ptr [eax+10h]
    movaps      xmm6,xmmword ptr [eax+20h]
    mov         edi, dst
    movaps      xmm5,xmmword ptr [eax+30h]
align 16
_Loop2:
    prefetchT0  [esi+20]
    movlps      xmm1,qword ptr [esi]
    movss       xmm0,dword ptr [esi+8]
    shufps      xmm0,xmm0,0
    movaps      xmm3,xmm1
    mulps       xmm0,xmm6
    add         edi, 20
    shufps      xmm3,xmm1,55h
    mov         eax, [esi+12]
    mulps       xmm3,xmm4
    shufps      xmm1,xmm1,0
    mov         [edi+12-20], eax
    mulps       xmm1,xmm2
    addps       xmm3,xmm1
    mov         eax, [esi+16]
    addps       xmm3,xmm0
    addps       xmm3,xmm5
    add         esi, 20
    movhlps     xmm1,xmm3
    movlps      qword ptr [edi-20],xmm3
    dec         ecx
    mov         [edi+16-20], eax
    movss       dword ptr [edi+8-20],xmm1
    jne         _Loop2
  }
#else
  struct_VERTEX_FORMAT_P3F_TEX2F *pDst = (struct_VERTEX_FORMAT_P3F_TEX2F *)dst;
  for (int i=0; i<nNumVerts; i++, OffsP+=sizeof(struct_VERTEX_FORMAT_P3F_TEX2F))
  {
    //pDst->xyz = mat->TransformPoint(*(Vec3 *)OffsP);
    TransformPosition(pDst->xyz, *(Vec3 *)OffsP, *mat);
    pDst->st[0] = *(float *)&OffsP[12];
    pDst->st[1] = *(float *)&OffsP[16];
    pDst++;
  }
#endif
}

float f3 = 32767.0f;
float fi3 = 1.0f/32767.0f;
DEFINE_ALIGNED_DATA(int, val[4], 16);
void sCopyTransf_TN(byte *dst, Matrix33 *mat, int nNumVerts, byte *OffsP)
{
#ifdef DO_ASM
  _asm
  {
    mov ecx,    nNumVerts;
    mov eax,    mat
    mov esi,    OffsP
    mov edi,    dst
    movaps      xmm2,xmmword ptr [eax]
    movaps      xmm4,xmmword ptr [eax+10h]
    movaps      xmm6,xmmword ptr [eax+20h]
align 16
_Loop:
    movsx       eax, word ptr [esi+4]
    cvtsi2ss    xmm0, eax
    shufps      xmm0,xmm0,0
    prefetcht0  [esi+10h] 
    mulps       xmm0,xmm6
    movsx       eax, word ptr [esi+2]
    cvtsi2ss    xmm3, eax
    mulps       xmm3,xmm4
    movsx       eax, word ptr [esi+0]
    cvtsi2ss    xmm1, eax
    mulps       xmm1,xmm2
    addps       xmm3,xmm1
    addps       xmm3,xmm0
    mov         ax, word ptr [esi+6]
    movaps      xmm1,xmm3     // r1 = vx, vy, vz, X
    mulps		    xmm1,xmm3			// r1 = vx * vx, vy * vy, vz * vz, X
    movhlps		  xmm5,xmm1			// r5 = vz * vz, X, X, X
    movaps		  xmm0,xmm1			// r0 = r1
    mov         word ptr [edi+6], ax
    shufps	  	xmm0,xmm0, 1	// r0 = vy * vy, X, X, X
    addss	      xmm1,xmm0			// r0 = (vx * vx) + (vy * vy), X, X, X
    addss	      xmm1,xmm5			// r1 = (vx * vx) + (vy * vy) + (vz * vz), X, X, X
    sqrtss	    xmm1,xmm1			// r1 = sqrt((vx * vx) + (vy * vy) + (vz * vz)), X, X, X
    rcpss		    xmm1,xmm1			// r1 = 1/radius, X, X, X
    shufps		  xmm1,xmm1, 0	// r1 = 1/radius, 1/radius, 1/radius, X
    mulps		    xmm3,xmm1			// r3 = vx * 1/radius, vy * 1/radius, vz * 1/radius, X
    movhlps     xmm5,xmm3
    cvtss2si    eax, xmm3
    mov         word ptr [edi+0], ax
    cvtss2si    eax, xmm5
    mov         word ptr [edi+4], ax
    shufps	  	xmm3,xmm3, 1
    cvtss2si    eax, xmm3
    mov         word ptr [edi+2], ax

    movsx       eax, word ptr [esi+12]
    cvtsi2ss    xmm0, eax
    shufps      xmm0,xmm0,0
    mulps       xmm0,xmm6
    movsx       eax, word ptr [esi+10]
    cvtsi2ss    xmm3, eax
    mulps       xmm3,xmm4
    movsx       eax, word ptr [esi+8]
    cvtsi2ss    xmm1, eax
    mulps       xmm1,xmm2
    addps       xmm3,xmm1
    addps       xmm3,xmm0
    mov         ax, word ptr [esi+14]
    movaps      xmm1,xmm3     // r1 = vx, vy, vz, X
    mulps		    xmm1,xmm3			// r1 = vx * vx, vy * vy, vz * vz, X
    movhlps		  xmm5,xmm1			// r5 = vz * vz, X, X, X
    movaps		  xmm0,xmm1			// r0 = r1
    mov         word ptr [edi+14], ax
    shufps	  	xmm0,xmm0, 1	// r0 = vy * vy, X, X, X
    addss	      xmm1,xmm0			// r0 = (vx * vx) + (vy * vy), X, X, X
    addss	      xmm1,xmm5			// r1 = (vx * vx) + (vy * vy) + (vz * vz), X, X, X
    sqrtss	    xmm1,xmm1			// r1 = sqrt((vx * vx) + (vy * vy) + (vz * vz)), X, X, X
    rcpss		    xmm1,xmm1			// r1 = 1/radius, X, X, X
    shufps		  xmm1,xmm1, 0	// r1 = 1/radius, 1/radius, 1/radius, X
    mulps		    xmm3,xmm1			// r3 = vx * 1/radius, vy * 1/radius, vz * 1/radius, X
    movhlps     xmm5,xmm3
    cvtss2si    eax, xmm3
    mov         word ptr [edi+8], ax
    cvtss2si    eax, xmm5
    mov         word ptr [edi+12], ax
    shufps	  	xmm3,xmm3, 1
    cvtss2si    eax, xmm3
    mov         word ptr [edi+10], ax

    add         esi, 16
    add         edi, 16
    dec         ecx
    jne         _Loop
  }
#else
  SPipTangents *pDst = (SPipTangents *)dst;
  SPipTangents *pSrc = (SPipTangents *)OffsP;
  for (int i=0; i<nNumVerts; i++, pSrc++, pDst++)
  {
    Vec3 v;
    v.x = tPackB2F(pSrc->Binormal.x);
    v.y = tPackB2F(pSrc->Binormal.y);
    v.y = tPackB2F(pSrc->Binormal.z);
    pDst->Binormal.x = tPackF2B(v.Dot(mat->GetColumn0()));
    pDst->Binormal.y = tPackF2B(v.Dot(mat->GetColumn1()));
    pDst->Binormal.z = tPackF2B(v.Dot(mat->GetColumn2()));
    pDst->Binormal.w = pSrc->Binormal.w;

    v.x = tPackB2F(pSrc->Tangent.x);
    v.y = tPackB2F(pSrc->Tangent.y);
    v.y = tPackB2F(pSrc->Tangent.z);
    pDst->Tangent.x = tPackF2B(v.Dot(mat->GetColumn0()));
    pDst->Tangent.y = tPackF2B(v.Dot(mat->GetColumn1()));
    pDst->Tangent.z = tPackF2B(v.Dot(mat->GetColumn2()));
    pDst->Tangent.w = pDst->Tangent.w;
  }
#endif
}

// Commit changed states to the hardware before drawing
bool CD3D9Renderer::FX_CommitStreams(SShaderPass *sl, bool bSetVertexDecl)
{
  bool bRet = true;

  //PROFILE_FRAME(Draw_Predraw);

  HRESULT hr;	
  if (bSetVertexDecl)
  {
    hr = EF_SetVertexDeclaration(m_RP.m_FlagsStreams_Decl, m_RP.m_CurVFormat);
    if (FAILED(hr))
      return false;
  }

  if (!m_RP.m_pRE && m_RP.m_RendNumVerts && m_RP.m_RendNumIndices)
  {
    uint nStart;
    uint nSize = m_RP.m_Stride*m_RP.m_RendNumVerts;
    if (!(m_RP.m_FlagsPerFlush & RBSI_VERTSMERGED))
    {
      m_RP.m_FlagsPerFlush |= RBSI_VERTSMERGED;
      void *pVB = FX_LockVB(nSize, nStart);
      memcpy(pVB, m_RP.m_Ptr.Ptr, nSize);
      FX_UnlockVB();
      m_RP.m_FirstVertex = 0;
      m_RP.m_MergedStreams[0] = m_RP.m_VBs[m_RP.m_CurVB];
      m_RP.m_nStreamOffset[0] = nStart;
      m_RP.m_PS.m_DynMeshUpdateBytes += nSize;

      ushort *pIB = m_RP.m_IndexBuf->Lock(m_RP.m_RendNumIndices, nStart);
      memcpy(pIB, m_RP.m_SysRendIndices, m_RP.m_RendNumIndices*sizeof(short));
      m_RP.m_IndexBuf->Unlock();
      m_RP.m_FirstIndex = nStart;
      m_RP.m_PS.m_DynMeshUpdateBytes += m_RP.m_RendNumIndices*sizeof(short);
    }
    m_RP.m_MergedStreams[0].VBPtr_0->Bind(0, m_RP.m_nStreamOffset[0], m_RP.m_Stride);
    m_RP.m_IndexBuf->Bind();

    if (m_RP.m_FlagsStreams_Stream & VSM_TANGENTS)
    {
      if (!(m_RP.m_FlagsPerFlush & RBSI_TANGSMERGED))
      {
        uint nSize = sizeof(SPipTangents)*m_RP.m_RendNumVerts;
        m_RP.m_FlagsPerFlush |= RBSI_TANGSMERGED;
        void *pVB = FX_LockVB(nSize, nStart);
        memcpy(pVB, m_RP.m_PtrTang.Ptr, nSize);
        FX_UnlockVB();
        m_RP.m_PS.m_DynMeshUpdateBytes += nSize;
        m_RP.m_MergedStreams[VSF_TANGENTS] = m_RP.m_VBs[m_RP.m_CurVB];
        m_RP.m_nStreamOffset[VSF_TANGENTS] = nStart;
      }
      m_RP.m_MergedStreams[VSF_TANGENTS].VBPtr_0->Bind(VSF_TANGENTS, m_RP.m_nStreamOffset[VSF_TANGENTS], sizeof(SPipTangents));
      m_RP.m_PersFlags |= RBPF_USESTREAM<<VSF_TANGENTS;
    }
    else
    if (m_RP.m_PersFlags & (RBPF_USESTREAM<<VSF_TANGENTS))
    {
      m_RP.m_PersFlags &= ~(RBPF_USESTREAM<<VSF_TANGENTS);
      FX_SetVStream(1, NULL, 0, 0);
    }
  }
  else
  if (m_RP.m_pRE)
    bRet = m_RP.m_pRE->mfPreDraw(sl);

  return bRet;
}

void CD3D9Renderer::FX_DebugCheckDeclaration(SD3DVertexDeclaration *pDecl, int nVBStride, int nVBFormat)
{

}

bool CD3D9Renderer::FX_DebugCheckConsistency(int FirstVertex, int FirstIndex, int RendNumVerts, int RendNumIndices)
{
  if (CV_r_validateDraw != 2)
    return true;
  HRESULT hr = S_OK;
#if defined (DIRECT3D9) || defined (OPENGL)
  assert(m_RP.m_VertexStreams[0].pStream);
  //assert(m_RP.m_VertexStreams[0].nFreq == 1);
  LPDIRECT3DVERTEXBUFFER9 pVB = (LPDIRECT3DVERTEXBUFFER9)m_RP.m_VertexStreams[0].pStream;
  LPDIRECT3DINDEXBUFFER9 pIB = (LPDIRECT3DINDEXBUFFER9)m_RP.m_pIndexStream;
  assert(pIB);
  if (!pIB || !pVB)
    return false;
  int i;
  int nVBOffset = m_RP.m_VertexStreams[0].nOffset;
  ushort *pIBData;
  byte *pVBData;
  hr = pIB->Lock(0, 0, (void **)&pIBData, 0);
  assert(SUCCEEDED(hr));
  hr = pVB->Lock(0, 0, (void **)&pVBData, 0);
  assert(SUCCEEDED(hr));
  int nVBFormat = m_RP.m_CurVFormat;
  int nVBStride =  m_VertexSize[nVBFormat];
  SD3DVertexDeclaration *pDecl = &m_RP.m_D3DVertexDeclaration[(m_RP.m_FlagsStreams_Decl&0xff)>>1][nVBFormat];
  assert(pDecl->m_pDeclaration);
  FX_DebugCheckDeclaration(pDecl, nVBStride, nVBFormat);

  Vec3 vMax, vMin;
  vMin = Vec3(100000.0f, 100000.0f, 100000.0f);
  vMax = Vec3(-100000.0f, -100000.0f, -100000.0f);

  for (i=0; i<RendNumIndices; i++)
  {
    int nInd = pIBData[i+FirstIndex];
    assert(nInd>=FirstVertex && nInd<FirstVertex+RendNumVerts);
    byte *pV = &pVBData[nVBOffset+nInd*nVBStride];
    Vec3 *pVV = (Vec3 *)pV;
    vMin.CheckMin(*pVV);
    vMax.CheckMax(*pVV);
    Vec3 vAbs = pVV->abs();
    assert(vAbs.x < 10000.0f && vAbs.y < 10000.0f && vAbs.z < 10000.0f);
    if(vAbs.x > 10000.0f || vAbs.y > 10000.0f || vAbs.z > 10000.0f)
      hr = S_FALSE;
  }
  Vec3 vDif = vMax - vMin;
  assert(vDif.x < 10000.0f && vDif.y < 10000.0f && vDif.z < 10000.0f);
  if (vDif.x >= 10000.0f || vDif.y > 10000.0f || vDif.z > 10000.0f)
    hr = S_FALSE;

  pIB->Unlock();
  pVB->Unlock();

#elif defined (DIRECT3D10)
#endif
  if (hr != S_OK)
  {
    iLog->LogError("ERROR: CD3D9Renderer::FX_DebugCheckConsistency: Validation failed for DIP (Shader: '%s')\n", m_RP.m_pShader->GetName());
  }
  return (hr==S_OK);
}

// Draw current indexed mesh
void CD3D9Renderer::EF_DrawIndexedMesh (int nPrimType)
{
  HRESULT h = 0;

  if (CV_r_nodrawshaders)
    return;

  int nFaces;

  PROFILE_FRAME(Draw_DrawCall);

  EF_Commit();

#if defined (DIRECT3D9) || defined(OPENGL)
  D3DPRIMITIVETYPE nType;
  switch (nPrimType)
  {
  case R_PRIMV_LINES:
    nType = D3DPT_LINELIST;
    nFaces = m_RP.m_RendNumIndices/2;
    break;

  case R_PRIMV_TRIANGLES:
    nType = D3DPT_TRIANGLELIST;
    nFaces = m_RP.m_RendNumIndices/3;
    break;

  case R_PRIMV_TRIANGLE_STRIP:
    nType = D3DPT_TRIANGLESTRIP;
    nFaces = m_RP.m_RendNumIndices-2;
    break;

  case R_PRIMV_TRIANGLE_FAN:
    nType = D3DPT_TRIANGLEFAN;
    nFaces = m_RP.m_RendNumIndices-2;
    break;

  case R_PRIMV_MULTI_STRIPS:
    {
      PodArray<CRenderChunk> *mats = m_RP.m_pRE->mfGetMatInfoList();
      if (mats)
      {
        CRenderChunk *m = mats->Get(0);
        for (int i=0; i<mats->Count(); i++, m++)
        {
          FX_DebugCheckConsistency(m->nFirstVertId, m->nFirstIndexId+m_RP.m_IndexOffset, m->nNumVerts, m->nNumIndices);
          if (FAILED(h=m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, 0, m->nFirstVertId, m->nNumVerts, m->nFirstIndexId+m_RP.m_IndexOffset, m->nNumIndices - 2)))
          {
            Error("CD3D9Renderer::EF_DrawIndexedMesh: DrawIndexedPrimitive error", h);
            return;
          }
          m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += (m->nNumIndices - 2);
          m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
        }
      }
      return;
    }
    break;

  case R_PRIMV_HWSKIN_GROUPS:
    {
      CRenderChunk *pChunk = m_RP.m_pRE->mfGetMatInfo();
      if (pChunk)
      {
        // SHWSkinBatch *pBatch = pChunk->m_pHWSkinBatch;
        FX_DebugCheckConsistency(m_RP.m_FirstVertex, pChunk->nFirstIndexId+m_RP.m_IndexOffset, pChunk->nNumVerts, pChunk->nNumIndices);
        if (FAILED(h=m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, m_RP.m_FirstVertex, pChunk->nNumVerts, pChunk->nFirstIndexId+m_RP.m_IndexOffset, pChunk->nNumIndices / 3)))
        {
          Error("CD3D9Renderer::EF_DrawIndexedMesh: DrawIndexedPrimitive error", h);
          return;
        }
        m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += (pChunk->nNumIndices / 3);
        m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
      }
      return;
    }
    break;

  default:
    assert(0);
  }

  if (nFaces)
  {
    FX_DebugCheckConsistency(m_RP.m_FirstVertex, m_RP.m_FirstIndex+m_RP.m_IndexOffset, m_RP.m_RendNumVerts, m_RP.m_RendNumIndices);
    if (FAILED(h=m_pd3dDevice->DrawIndexedPrimitive(nType, 0, m_RP.m_FirstVertex, m_RP.m_RendNumVerts, m_RP.m_FirstIndex+m_RP.m_IndexOffset, nFaces)))
    {
      Error("CD3D9Renderer::EF_DrawIndexedMesh: DrawIndexedPrimitive error", h);
      return;
    }
    m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += nFaces;
    m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
  }
#elif defined (DIRECT3D10)
  // Don't render fallback in DX10
  if (!CHWShader_D3D::m_pCurInstVS || !CHWShader_D3D::m_pCurInstPS || CHWShader_D3D::m_pCurInstVS->m_bFallback || CHWShader_D3D::m_pCurInstPS->m_bFallback)
    return;
  if (CHWShader_D3D::m_pCurInstGS && CHWShader_D3D::m_pCurInstGS->m_bFallback)
    return;

  D3D10_PRIMITIVE_TOPOLOGY nType;
  switch (nPrimType)
  {
  case R_PRIMV_LINES:
    nType = D3D10_PRIMITIVE_TOPOLOGY_LINELIST;
    nFaces = m_RP.m_RendNumIndices/2;
    break;

  case R_PRIMV_TRIANGLES:
    nType = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    nFaces = m_RP.m_RendNumIndices/3;
    break;

  case R_PRIMV_TRIANGLE_STRIP:
    nType = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    nFaces = m_RP.m_RendNumIndices-2;
    break;

  case R_PRIMV_TRIANGLE_FAN:
    assert(0);
    break;

  case R_PRIMV_MULTI_STRIPS:
    {
      PodArray<CRenderChunk> *mats = m_RP.m_pRE->mfGetMatInfoList();
      if (mats)
      {
				SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        CRenderChunk *m = mats->Get(0);
        for (int i=0; i<mats->Count(); i++, m++)
        {
          m_pd3dDevice->DrawIndexed(m->nNumIndices, m->nFirstIndexId+m_RP.m_IndexOffset, m->nFirstVertId);
          m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += (m->nNumIndices - 2);
          m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
        }
      }
      return;
    }
    break;

  case R_PRIMV_HWSKIN_GROUPS:
    {
      CRenderChunk *pChunk = m_RP.m_pRE->mfGetMatInfo();
      if (pChunk)
      {
        // SHWSkinBatch *pBatch = pChunk->m_pHWSkinBatch;        
				SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pd3dDevice->DrawIndexed(pChunk->nNumIndices, pChunk->nFirstIndexId+m_RP.m_IndexOffset, 0);
        m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += (pChunk->nNumIndices / 3);
        m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
      }
      return;
    }
    break;

  default:
    assert(0);
  }

  if (nFaces)
  {
		SetPrimitiveTopology(nType);
    m_pd3dDevice->DrawIndexed(m_RP.m_RendNumIndices, m_RP.m_FirstIndex+m_RP.m_IndexOffset, 0);
    m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += nFaces;
    m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
  }
#endif
}


//====================================================================================

struct SShadowLight
{
  PodArray<ShadowMapFrustum*> *pSmLI;
  CDLight *pDL;
};

static bool sbHasDot3LM;
static _inline int Compare(SShadowLight &a, SShadowLight &b)
{
  if (a.pSmLI->Count() > b.pSmLI->Count())
    return -1;
  if (a.pSmLI->Count() < b.pSmLI->Count())
    return 1;
  if (sbHasDot3LM)
  {
    if ((a.pDL->m_Flags & DLF_LM) < (b.pDL->m_Flags & DLF_LM))
      return -1;
    if ((a.pDL->m_Flags & DLF_LM) > (b.pDL->m_Flags & DLF_LM))
      return -1;
  }
  return 0;
}

static void sFillLP(SLightPass *pLP, uint& i, CDLight** pLight, int nLights, const int nMaxLights, const int nMaxAmbLights, bool& bHasAmb)
{
  if (!nLights || !nMaxLights || (!nMaxAmbLights && bHasAmb))
    return;
  int j, n, m;
  int nActualMaxLights = bHasAmb ? nMaxAmbLights : nMaxLights;
  bHasAmb = false;
  m = 0;

  for (j=0; j<32; j++)
  {
    nActualMaxLights = (nLights < nActualMaxLights) ? nLights : nActualMaxLights;
    if (!nActualMaxLights)
      break;
    if (CRenderer::CV_r_shadersdynamicbranching)
    {
      // With dynamic branching we cannot access lights form different groups in single pass
      int nGroup = pLight[m]->m_Id>>2;
      for (n=0; n<nActualMaxLights; n++)
      {
        if ((pLight[m]->m_Id>>2) != nGroup)
          break;
        pLP[i].pLights[n] = pLight[m++];
      }
    }
    else
    for (n=0; n<nActualMaxLights; n++)
    {
      pLP[i].pLights[n] = pLight[m++];
    }
    pLP[i].nLights = n;
    i++;
    nLights -= n;
    nActualMaxLights = nMaxLights;
  }
}

void CD3D9Renderer::FX_StencilLights(SLightPass *pLP)
{
  if (SRendItem::m_RecurseLevel > 1)
    return;
  if (!pLP->nLights || (m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_AMBIENT]))
  {
    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
      STENCOP_PASS(FSS_STENCOP_KEEP),
      0x0, 0xF/*pLP->nStencLTMask*/, 0xFFFFFFFF
      );
  }
  else
  {

    //FX_StencilRefresh(STENC_FUNC(FSS_STENCFUNC_NOTEQUAL), 0x0, pLP->nStencLTMask);
    //uint nStencilMask = 1<<(nLod - 1);
    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_NOTEQUAL) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
      STENCOP_PASS(FSS_STENCOP_KEEP),
      0x0, 0xF/*pLP->nStencLTMask*/, 0xFFFFFFFF
    );
  }
  return;
}

int CD3D9Renderer::FX_SetupLightPasses(SShaderTechnique *pTech)
{
  uint i;
  if (!pTech->m_Passes.Num())
    return -1;
  if (pTech->m_Flags & FHF_NOLIGHTS)
  {
    if (m_RP.m_pSunLight && (m_RP.m_DynLMask & 1))
    {
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_AMBIENT];
      if ((m_RP.m_ObjFlags & FOB_AMBIENT_OCCLUSION) && m_RP.m_pCurObject->m_nTextureID1>0)
        m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_AMBIENT_OCCLUSION];
    }
    m_RP.m_LPasses[0].nLights = 0;
    m_RP.m_LPasses[0].nLTMask = 0;
    m_RP.m_nCurLightPasses = 1;
    m_RP.m_PrevLMask = 0;
    return 1;
  }
  int nLightGroup = (m_RP.m_nBatchFilter & FB_Z) ? 0 : m_RP.m_nCurLightGroup;
  uint nAffectMask = nLightGroup < 0 ? -1 : (0xf << (nLightGroup*4));
  uint nMask = m_RP.m_DynLMask & nAffectMask;
  uint nStencGroupLTMask = (nMask >> (nLightGroup*4)) & 0xf;

  bool bHasAmb = false;

  bHasAmb = !m_RP.m_bNotFirstPass;
  if (m_RP.m_nNumRendPasses > 0)
    bHasAmb = false;
  if (bHasAmb)
  {
    bHasAmb = !m_RP.m_bNotFirstPass;
    if (m_RP.m_nNumRendPasses > 0)
      bHasAmb = false;
    if (bHasAmb)
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_AMBIENT];
  }

  if (nMask == m_RP.m_PrevLMask && CV_r_optimisedlightsetup)
    return m_RP.m_nCurLightPasses;

  m_RP.m_PrevLMask = nMask;
  m_RP.m_PS.m_NumLightSetups++;
  uint nFirst = nLightGroup < 0 ? 0 : nLightGroup*4;
  SShaderPass *slw = &pTech->m_Passes[0];
  int nR = SRendItem::m_RecurseLevel;

  int nLight;
  CRenderObject *pObj = m_RP.m_pCurObject;

  CDLight *DirLights[4];
  int nDirLights = 0;
  CDLight *OmniLights[32];
  int nOmniLights = 0;
  CDLight *ProjLights[32];
  int nProjLights = 0;
  CDLight *dl;
 
  for (nLight=nFirst; nLight<m_RP.m_DLights[nR].Num(); nLight++)
  {
    if (nMask & (1<<nLight))
    {
      dl = m_RP.m_DLights[nR][nLight];
      if ((dl->m_Flags & DLF_DIRECTIONAL) && (CV_r_optimisedlightsetup==2 || CV_r_optimisedlightsetup==3))
        DirLights[nDirLights++] = dl;
      else
      if (dl->m_Flags & DLF_PROJECT)
        ProjLights[nProjLights++] = dl;
      else
        OmniLights[nOmniLights++] = dl;
      if (nLightGroup >= 0)
      {
        dl->m_ShadowChanMask = Vec4(0,0,0,0);
        assert(nLight-nFirst <= 3);
        dl->m_ShadowChanMask[nLight-nFirst] = 1;
      }
    }
  }
  if (!bHasAmb && !nDirLights && !nOmniLights && !nProjLights)
  {
    m_RP.m_nCurLightPasses = -1;
    return -1;
  }
  uint nMaxLights = m_RP.m_nMaxLightsPerPass;
  uint nMaxAmbLights = nMaxLights;
  if (CV_r_lightssinglepass || (m_RP.m_pShader && (m_RP.m_pShader->m_Flags2 & EF2_SINGLELIGHTPASS) ))
  {
    nMaxLights = 1;
    nMaxAmbLights = 1;
  }
  i = 0;
  sFillLP(&m_RP.m_LPasses[0], i, DirLights,  nDirLights,  nMaxLights, nMaxAmbLights, bHasAmb);
  sFillLP(&m_RP.m_LPasses[0], i, OmniLights, nOmniLights, nMaxLights, nMaxAmbLights, bHasAmb);
  sFillLP(&m_RP.m_LPasses[0], i, ProjLights, nProjLights, min(nMaxLights,1U), min(nMaxAmbLights,1U), bHasAmb);

  //no lightpass for ambient light, we need to cre6ate one
  if (bHasAmb)
  {
    m_RP.m_LPasses[0].nLights = 0;
    m_RP.m_LPasses[0].nLTMask = 0;
    m_RP.m_nCurLightPasses = 1;
    return 1;
  }

  int nPasses = i;
  int nPass;
  for (nPass=0; nPass<nPasses; nPass++)
  {
    int Types[4];
    SLightPass *lp = &m_RP.m_LPasses[nPass];
    lp->bRect = false;
    lp->nStencLTMask = 0;//nStencGroupLTMask;
    lp->nLTMask = lp->nLights;
    for (i=0; i<lp->nLights; i++)
    {
      dl = lp->pLights[i];

//////////////////////////////////////////////////////////////////////////
      //TOFIX: don't rely on conseq light groups
      int nSLMask = 1<<dl->m_Id;
      lp->nStencLTMask |= nSLMask >> (nLightGroup*4);
//////////////////////////////////////////////////////////////////////////
      if (dl->m_Flags & DLF_POINT)
        Types[i] = SLMF_POINT;
      else
      if (dl->m_Flags & DLF_PROJECT)
      {
        Types[i] = SLMF_PROJECTED;
        assert(i == 0);
      }
      else
        Types[i] = SLMF_DIRECT;
    }
    switch(lp->nLights)
    {
    case 2:
      if (Types[0] > Types[1])
      {
        Exchange(Types[0], Types[1]);
        Exchange(lp->pLights[0], lp->pLights[1]);
      }
      break;
    case 3:
      if (Types[0] > Types[1])
      {
        Exchange(Types[0], Types[1]);
        Exchange(lp->pLights[0], lp->pLights[1]);
      }
      if (Types[0] > Types[2])
      {
        Exchange(Types[0], Types[2]);
        Exchange(lp->pLights[0], lp->pLights[2]);
      }
      if (Types[1] > Types[2])
      {
        Exchange(Types[1], Types[2]);
        Exchange(lp->pLights[1], lp->pLights[2]);
      }
      break;
    case 4:
      {
        for (int i=0; i<4; i++)
        {
          for (int j=i; j<4; j++)
          {
            if (Types[i] > Types[j])
            {
              Exchange(Types[i], Types[j]);
              Exchange(lp->pLights[i], lp->pLights[j]);
            }
          }
        }
      }
      break;
    }

    for (i=0; i<lp->nLights; i++)
    {
      lp->nLTMask |= Types[i] << (SLMF_LTYPE_SHIFT + i*SLMF_LTYPE_BITS);
    }
  }
  m_RP.m_nCurLightPasses = nPasses;
  if (m_RP.m_nShaderQuality != eSQ_Low)
  {
    if (nPasses == 1)
      CHWShader_D3D::mfSetLightParams(0);
  }
  return nPasses;
}

void CD3D9Renderer::FX_SetLightsScissor(SLightPass *pLP, const RectI* pDef)
{
  if (SRendItem::m_RecurseLevel > 1)
    return;
  if (!pLP->nLights || (m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_AMBIENT]))
  {
		if (pDef)
			EF_Scissor(true, pDef->x, pDef->y, pDef->w, pDef->h);
		else
    EF_Scissor(false, 0, 0, 0, 0);
    return;
  }
  if (!pLP->bRect)
  {
    pLP->bRect = true;
    CDLight *pDL = pLP->pLights[0];
    pLP->rc = RectI(pDL->m_sX, pDL->m_sY, pDL->m_sWidth, pDL->m_sHeight);
    for (int i=1; i<pLP->nLights; i++)
    {
      pDL = pLP->pLights[i];
      pLP->rc.Add(pDL->m_sX, pDL->m_sY, pDL->m_sWidth, pDL->m_sHeight);
    }
  }
  if (pLP->rc.w == m_width && pLP->rc.h == m_height)
	{
		if (pDef)
			EF_Scissor(true, pDef->x, pDef->y, pDef->w, pDef->h);
		else
    EF_Scissor(false, 0, 0, 0, 0);
	}
  else
	{
		if (pDef)
		{
			int x1 = max(pDef->x, pLP->rc.x);
			int x2 = min(pDef->x + pDef->w, pLP->rc.x + pLP->rc.w);
			int y1 = max(pDef->y, pLP->rc.y);
			int y2 = min(pDef->y + pDef->h, pLP->rc.y + pLP->rc.h);
			EF_Scissor(true, x1, y1, max(x2-x1, 0), max(y2-y1, 0));
		}
  else
    EF_Scissor(true, pLP->rc.x, pLP->rc.y, pLP->rc.w, pLP->rc.h);
}
}

void CD3D9Renderer::FX_DrawMultiLightPasses(CShader *ef, SShaderTechnique *pTech, int nShadowChans)
{
  SShaderPass *slw;
  uint i;

  PROFILE_FRAME(DrawShader_MultiLight_Passes);

  int nPasses = FX_SetupLightPasses(pTech);
  if (nPasses < 0)
    return;
  CRenderObject *pObj = m_RP.m_pCurObject;

  if ((m_RP.m_ObjFlags & FOB_BLEND_WITH_TERRAIN_COLOR) && m_RP.m_pCurObject->m_nTextureID>0)
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR];

  if (m_RP.m_nPassGroupID == EFSLIST_GENERAL && (m_RP.m_ObjFlags & FOB_RAE_GEOMTERM) && CV_r_RAM)
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_RAE_GEOMTERM]; //enable rae geometry term texture
  else
  if ((m_RP.m_ObjFlags & FOB_AMBIENT_OCCLUSION) && m_RP.m_pCurObject->m_nTextureID1>0)
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_AMBIENT_OCCLUSION];

  //////////////////////////////////////////////////////////////////////////

  PodArray<ShadowMapFrustum*> * lsources = pObj->m_pShadowCasters;
  if ((ef->m_Flags2 & EF2_SUBSURFSCATTER) && lsources)
  {
    if (SRendItem::m_RecurseLevel!= 1)
    {
      assert(0);
    }
    //set separate scattering depth buffer
    for (i=0; i<lsources->Count(); i++)
    {
      if (lsources->GetAt(i)->bForSubSurfScattering)
      {
        ShadowMapFrustum* pScatterDepthBuffer = lsources->GetAt(i);

        //configure first depth buffer  
        SetupShadowOnlyPass(0, pScatterDepthBuffer, NULL);

        if (pScatterDepthBuffer->bHWPCFCompare)
          m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_HW_PCF_COMPARE ];

        m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SHADOW_MIXED_MAP_G16R16];
        //FIX: HWSR_PSM overwrites bit for HWSR_HW_PCF_COMPARE currently
        //m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_PSM];
        m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3]);
        m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

        //if (GetSkyLightRenderParams())
        //  m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SKYLIGHT_BASED_FOG];

        //break;
      }
    }
  } 

  //////////////////////////////////////////////////////////////////////////

  int nPass;
  m_RP.m_PersFlags |= RBPF_MULTILIGHTS;  
  for (nPass=0; nPass<nPasses; nPass++)
  {
    m_RP.m_nCurLightPass = nPass;
    if (nPass && (m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_AMBIENT]))
      m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_AMBIENT] | g_HWSR_MaskBit[HWSR_AMBIENT_OCCLUSION] | g_HWSR_MaskBit[HWSR_RAE_GEOMTERM]);
    if (nPasses > 1 || m_RP.m_nShaderQuality == eSQ_Low)
      CHWShader_D3D::mfSetLightParams(nPass);
    SLightPass *lp = &m_RP.m_LPasses[nPass];
		RectI object_scissor;
		bool use_object_scissor = pObj->m_nScissorX1 || pObj->m_nScissorY1 || pObj->m_nScissorX2 || pObj->m_nScissorY2;
		if (use_object_scissor)
		{
			object_scissor.x = pObj->m_nScissorX1;
			object_scissor.y = pObj->m_nScissorY1;
			object_scissor.w = pObj->m_nScissorX2 - pObj->m_nScissorX1;
			object_scissor.h = pObj->m_nScissorY2 - pObj->m_nScissorY1;
		}

		if (CV_r_optimisedlightsetup)
		{
			if (use_object_scissor)
				FX_SetLightsScissor(lp, &object_scissor);
			else
				FX_SetLightsScissor(lp, NULL);
		}

    if (m_RP.m_PersFlags2 & RBPF2_LIGHTSTENCILCULL)
      FX_StencilLights(lp);
		
    m_RP.m_FlagsShader_LT = lp->nLTMask;
#ifdef DO_RENDERLOG
    if (CRenderer::CV_r_log >= 3)
    {
      if (lp->nLights > 0)
      {
        if (nPass)
          Logv(SRendItem::m_RecurseLevel, "\n--- Lights: ");
        else
          Logv(SRendItem::m_RecurseLevel, "--- Lights: ");
        for (int i=0; i<lp->nLights; i++)
        {
          Logv(0, "%s ", lp->pLights[i]->m_sName);
        }
        Logv(0, "\n");
      }
    }
#endif

    slw = &pTech->m_Passes[0];
    const int nPasses = pTech->m_Passes.Num();
    for (i=0; i<nPasses; i++, slw++)
    {
      m_RP.m_pCurPass = slw;

      // Set all textures and HW TexGen modes for the current pass (ShadeLayer)
      //assert (slw->m_VShader && slw->m_PShader);
      if (!slw->m_VShader || !slw->m_PShader)
        continue;
      CHWShader *curVS = slw->m_VShader;
      CHWShader *curPS = slw->m_PShader;
      
      FX_CommitStates(pTech, slw, (slw->m_PassFlags & SHPF_NOMATSTATE) == 0);

      if (m_RP.m_FlagsPerFlush & RBSI_INSTANCED)
      {
        // Using geometry instancing approach
        FX_DrawShader_InstancedHW(ef, slw);
      }
      else
      {
        // Set Pixel shader and all associated textures
        bool bRes = curPS->mfSet(HWSF_SETTEXTURES);
        if (bRes)
          FX_DrawBatches(ef, slw, curVS, curPS);
      }      
    }

    // Should only be enabled per light pass (else if shader has more than 1 pass in technique will blend incorrect)
    m_RP.m_bNotFirstPass = true;
  }
  m_RP.m_bNotFirstPass = false;
  m_RP.m_FlagsShader_LT = 0;
  m_RP.m_nCurLightPass = 0;
  m_RP.m_PersFlags &= ~RBPF_MULTILIGHTS;
  m_RP.m_FrameObject++;
}

//=================================================================================

#define INST_PARAM_SIZE 4*sizeof(float)

// Actual drawing of instances
void CD3D9Renderer::FX_DrawInstances(CShader *ef, SShaderPass *slw, uint nStartInst, uint nLastInst, uint nUsedAttr, int nInstAttrMask, byte Attributes[], bool bConstBasedInstancing)
{
  uint i;

  CHWShader_D3D *vp = (CHWShader_D3D *)slw->m_VShader;

#if defined (DIRECT3D10)
  // Don't render fallback in DX10
  if (!CHWShader_D3D::m_pCurInstVS || !CHWShader_D3D::m_pCurInstPS || CHWShader_D3D::m_pCurInstVS->m_bFallback || CHWShader_D3D::m_pCurInstPS->m_bFallback)
    return;
#endif

  // Set culling mode
  if (!(m_RP.m_FlagsPerFlush & RBSI_NOCULL))
  {
    if (slw->m_eCull != -1)
      D3DSetCull((ECull)slw->m_eCull);
  }

  if (bConstBasedInstancing)
    nInstAttrMask = 0;
  if (!nStartInst)
  {
    // Set the stream 3 to be per instance data and iterate once per instance
    m_RP.m_PersFlags &= ~(RBPF_USESTREAM<<3);
    int nCompared = 0;
    FX_CommitStreams(slw, false);
    int StreamMask = m_RP.m_FlagsStreams_Decl >> 1;
    SVertexDeclaration *vd;
    for (i=0; i<m_RP.m_CustomVD.Num(); i++)
    {
      vd = m_RP.m_CustomVD[i];
      if (vd->StreamMask == StreamMask && vd->VertFormat == m_RP.m_CurVFormat && vd->InstAttrMask == nInstAttrMask)
        break;
    }
    if (i == m_RP.m_CustomVD.Num())
    {
      vd = new SVertexDeclaration;
      m_RP.m_CustomVD.AddElem(vd);
      vd->StreamMask = StreamMask;
      vd->VertFormat = m_RP.m_CurVFormat;
      vd->InstAttrMask = nInstAttrMask;
      vd->m_pDeclaration = NULL;
      int nElementsToCopy = m_RP.m_D3DVertexDeclaration[StreamMask][m_RP.m_CurVFormat].m_Declaration.Num()-1;
#if defined (DIRECT3D10)
      nElementsToCopy++;
#endif
      for (i=0; i<nElementsToCopy; i++)
      {
        vd->m_Declaration.AddElem(m_RP.m_D3DVertexDeclaration[StreamMask][m_RP.m_CurVFormat].m_Declaration[i]);
      }
      int nInstOffs = 1;
#if defined (DIRECT3D9) || defined (OPENGL)
      D3DVERTEXELEMENT9 ve;
      ve.Stream = 3;
      ve.Method = D3DDECLMETHOD_DEFAULT;
      ve.Usage = D3DDECLUSAGE_TEXCOORD;
      if (bConstBasedInstancing)
      {
        ve.Type = D3DDECLTYPE_FLOAT1;
        ve.UsageIndex = (BYTE)vp->m_pCurInst->m_nInstIndex;
        ve.Offset = 0;
        vd->m_Declaration.AddElem(ve);
      }
      else
      {
        ve.Type = D3DDECLTYPE_FLOAT4;
        for (i=0; i<nUsedAttr; i++)
        {
          ve.Offset = i*INST_PARAM_SIZE;
          ve.UsageIndex = Attributes[i]+nInstOffs;
          vd->m_Declaration.AddElem(ve);
        }
      }
      ve.Stream = 0xff;
      ve.Type = D3DDECLTYPE_UNUSED;
      ve.Usage = 0;
      ve.UsageIndex = 0;
      ve.Offset = 0;
      vd->m_Declaration.AddElem(ve);
#elif defined (DIRECT3D10)
      D3D10_INPUT_ELEMENT_DESC elemTC = {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 3, 0, D3D10_INPUT_PER_INSTANCE_DATA, 1};      // texture
      if (bConstBasedInstancing)
      {
        assert(0);
      }
      else
      {
        for (i=0; i<nUsedAttr; i++)
        {
          elemTC.AlignedByteOffset = i*INST_PARAM_SIZE;
          elemTC.SemanticIndex = Attributes[i]+nInstOffs;
          vd->m_Declaration.AddElem(elemTC);
        }
      }
#endif
    }
    HRESULT hr;
#if defined (DIRECT3D9) || defined (OPENGL)
    if (!vd->m_pDeclaration)
    {
      hr = m_pd3dDevice->CreateVertexDeclaration(&vd->m_Declaration[0], &vd->m_pDeclaration);
      assert (hr == S_OK);
    }
    if (m_pLastVDeclaration != vd->m_pDeclaration)
    {
      m_pLastVDeclaration = vd->m_pDeclaration;
      hr = m_pd3dDevice->SetVertexDeclaration(vd->m_pDeclaration);
      assert (hr == S_OK);
    }
#elif defined (DIRECT3D10)
    if (!vd->m_pDeclaration)
    {
      assert(CHWShader_D3D::m_pCurInstVS && CHWShader_D3D::m_pCurInstVS->m_pShaderData);
      if (FAILED(hr = m_pd3dDevice->CreateInputLayout(&vd->m_Declaration[0], vd->m_Declaration.Num(), CHWShader_D3D::m_pCurInstVS->m_pShaderData, CHWShader_D3D::m_pCurInstVS->m_nShaderByteCodeSize, &vd->m_pDeclaration)))
      {
        assert(SUCCEEDED(hr));
        return;
      }
    }
    if (m_pLastVDeclaration != vd->m_pDeclaration)
    {
      m_pLastVDeclaration = vd->m_pDeclaration;
      m_pd3dDevice->IASetInputLayout(vd->m_pDeclaration);
    }
#endif
  }

  int nInsts = nLastInst-nStartInst+1;
  {
    //PROFILE_FRAME(Draw_ShaderIndexMesh);
    int nPolys = m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP];
#if defined (DIRECT3D9) || defined (OPENGL)
    if (m_RP.m_pRE)
      m_RP.m_pRE->mfDraw(ef, slw);
    else
      EF_DrawIndexedMesh(R_PRIMV_TRIANGLES);
#else
    assert (m_RP.m_pRE && m_RP.m_pRE->mfGetType() == eDATA_Mesh);
    EF_Commit();
    SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pd3dDevice->DrawIndexedInstanced(m_RP.m_RendNumIndices, nInsts, m_RP.m_FirstIndex+m_RP.m_IndexOffset, 0, 0);
    m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += m_RP.m_RendNumIndices/3;
    m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;
#endif
    int nPolysPerInst = m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] - nPolys;
    m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] -= nPolysPerInst;
    int nPolysAll = nPolysPerInst*nInsts;
    m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += nPolysAll;
    m_RP.m_PS.m_RendHWInstancesPolysOne += nPolysPerInst;
    m_RP.m_PS.m_RendHWInstancesPolysAll += nPolysAll;
    m_RP.m_PS.m_NumRendHWInstances += nInsts;
    m_RP.m_PS.m_RendHWInstancesDIPs++;
  }
}

static TArray<CRenderObject *> sTempObjects[2];

#define MAX_HWINST_PARAMS_CONST (240 - VSCONST_INSTDATA)

// Draw geometry instances in single DIP using HW geom. instancing (StreamSourceFreq)
void CD3D9Renderer::FX_DrawShader_InstancedHW(CShader *ef, SShaderPass *slw)
{
  PROFILE_FRAME(DrawShader_Instanced);

  // Set culling mode
  if (!(m_RP.m_FlagsPerFlush & RBSI_NOCULL))
  {
    if (slw->m_eCull != -1)
      D3DSetCull((ECull)slw->m_eCull);
  }

  uint i, j, n;
  CHWShader_D3D *vp = (CHWShader_D3D *)slw->m_VShader;
  CHWShader_D3D *ps = (CHWShader_D3D *)slw->m_PShader;
  uint nOffs;
  Matrix44 m;
  uint nCurInst = 0;
  SCGBind bind;
  byte Attributes[32];
  sTempObjects[0].SetUse(0);
  sTempObjects[1].SetUse(0);

  m_RP.m_FlagsPerFlush |= RBSI_INSTANCED;

  int nSimple = 0;
  int nInsts[2] = {0,0};
#ifdef DO_RENDERLOG
  if (CRenderer::CV_r_log >= 3)
  {
    CRenderObject *pObj = m_RP.m_pCurObject;
    int nO = 0;
    while (true)
    {
      DynArray16<SInstanceInfo> *pII = pObj->GetInstanceInfo();
      bool bRotate = (CV_r_geominstancing == 2 || (pObj->m_ObjFlags & FOB_TRANS_ROTATE));

      if (!bRotate)
      {
        Vec3 vPos = pObj->GetTranslation();
        Logv(SRendItem::m_RecurseLevel, "+++ Instance NonRotated %d (Obj: %d [%.3f, %.3f, %.3f]) * %d\n", nInsts[0], pObj->m_VisId, vPos[0], vPos[1], vPos[2], pII ? pII->size() : 1);
        sTempObjects[0].AddElem(pObj);
        if (pII)
          nInsts[0] += pII->size();
        else
          nInsts[0]++;
      }
      else
      {
        Vec3 vPos = pObj->GetTranslation();
        Logv(SRendItem::m_RecurseLevel, "+++ Instance Rotated %d (Obj: %d [%.3f, %.3f, %.3f]) * %d\n", nInsts[1], pObj->m_VisId, vPos[0], vPos[1], vPos[2], pII ? pII->size() : 1);
        sTempObjects[1].AddElem(pObj);
        if (pII)
          nInsts[1] += pII->size();
        else
          nInsts[1]++;
      }
      nO++;
      if (nO >= m_RP.m_ObjectInstances.Num())
        break;
      pObj = m_RP.m_ObjectInstances[nO];
    }
  }
  else
#endif
  {
    CRenderObject *pObj = m_RP.m_pCurObject;
    int nO = 0;
    while (true)
    {
      DynArray16<SInstanceInfo> *pII = pObj->GetInstanceInfo();
      bool bRotate = (CV_r_geominstancing == 2 || (pObj->m_ObjFlags & FOB_TRANS_ROTATE));

      if (!bRotate)
      {
        if (pII)
          nInsts[0] += pII->size();
        else
          nInsts[0]++;
        sTempObjects[0].AddElem(pObj);
      }
      else
      {
        if (pII)
          nInsts[1] += pII->size();
        else
          nInsts[1]++;
        sTempObjects[1].AddElem(pObj);
      }
      nO++;
      if (nO >= m_RP.m_ObjectInstances.Num())
        break;
      pObj = m_RP.m_ObjectInstances[nO];
    }
  }

  bool bConstBasedInstancing = false; 
  m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_INSTANCING_ATTR];
  m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_INSTANCING_ROT];

  // Non-rotated instances
  if (nInsts[0])
  {
    int nUsedAttr = 1;

    // Set Pixel shader and all associated textures
    if (!ps->mfSet(HWSF_SETTEXTURES))
      return;

    // Set Vertex shader
    if (!vp->mfSet(HWSF_INSTANCED))
      return;

#if defined(DIRECT3D10)
		CHWShader_D3D *gs = (CHWShader_D3D *)slw->m_GShader;
		if (gs)
			gs->mfSet(0);
		else
			CHWShader_D3D::mfBindGS(0, 0);
#endif

    // Create/Update video mesh (VB management)
    m_RP.m_pRE->mfCheckUpdate(m_RP.m_CurVFormat, m_RP.m_FlagsStreams_Stream);

    CHWShader_D3D::SHWSInstance *pVPInst = vp->m_pCurInst;
    // Starting from TC1 for instanced parameters
    Attributes[0] = (byte)pVPInst->m_nInstMatrixID;
    int nInstAttrMask = 1 << Attributes[0];

    if (pVPInst->m_pParams_Inst)
    {
      int nSize = pVPInst->m_pParams_Inst->size();
      for (j=0; j<nSize; j++)
      {
        SCGParam *pr = &(*pVPInst->m_pParams_Inst)[j];
        for (uint na=0; na < (uint)pr->m_nParameters; na++)
        {
          Attributes[nUsedAttr+na] = pr->m_dwBind+na;
          nInstAttrMask |= 1<<Attributes[nUsedAttr+na];
        }
        nUsedAttr += pr->m_nParameters;
      }
    }
    if (pVPInst->m_pParams[1] && pVPInst->m_pParams[1]->size() > 1)
    {
      int nSize = pVPInst->m_pParams[1]->size();
      for (uint i=1; i<nSize; i++)
      {
        SCGParam *pr = &(*pVPInst->m_pParams[1])[i];
        if (pr->m_Flags & PF_INSTANCE)
          iLog->LogWarning("WARNING: Instance depend constant '%s' used in the vertex shader %s during instancing", pr->m_Name.c_str(), vp->GetName());
      }
    }

    // Detects ability of using attributes based instancing
    // If number of used attributes exceed 16 we can't use attributes based instancing (switch to constant based)
    int nStreamMask = m_RP.m_FlagsStreams_Decl >> 1;
    int nVFormat = m_RP.m_CurVFormat;
    int nCO = 0;
    int nCI = 0;
    if (!bConstBasedInstancing && m_RP.m_D3DVertexDeclaration[nStreamMask][nVFormat].m_Declaration.Num()+nUsedAttr-1 > 16)
      iLog->LogWarning("WARNING: Attributes based instancing cannot exceed 16 attributes (%s uses %d attr. + %d vertex decl.attr.)[VF: %d, SM: 0x%x]", vp->GetName(), nUsedAttr, m_RP.m_D3DVertexDeclaration[nStreamMask][nVFormat].m_Declaration.Num()-1, nVFormat, nStreamMask);
    else
    while (nCurInst < nInsts[0])
    {
      uint nLastInst = nInsts[0] - 1;
      uint nParamsPerDIPAllowed = bConstBasedInstancing ? MAX_HWINST_PARAMS_CONST : MAX_HWINST_PARAMS;
      if ((nLastInst-nCurInst+1)*nUsedAttr >= nParamsPerDIPAllowed)
        nLastInst = nCurInst+(nParamsPerDIPAllowed/nUsedAttr)-1;
      byte *data;
      byte *inddata = NULL;
      if (bConstBasedInstancing)
      {
#if defined (DIRECT3D9) || defined (OPENGL)
        data = (byte *)&CHWShader_D3D::m_CurVSParams[VSCONST_INSTDATA].x;
        inddata = (byte *)m_RP.m_VB_Inst->Lock((nLastInst-nCurInst+1)*sizeof(float), nOffs);
#else
        assert(0);
#endif
      }
      else
        data = (byte *)m_RP.m_VB_Inst->Lock((nLastInst-nCurInst+1)*nUsedAttr*INST_PARAM_SIZE, nOffs);
      CRenderObject *curObj = m_RP.m_pCurObject;
      n = 0;
      // Fill the stream 3 for per-instance data
      CRenderObject *pObj;
      DynArray16<SInstanceInfo> *pII = NULL;
      SInstanceInfo *pI;
      for (i=nCurInst; i<=nLastInst; i++, n++)
      {
        float *fParm = (float *)&data[n*nUsedAttr*INST_PARAM_SIZE];
        float *fSrc;
        if (pII)
        {
          pI = &(*pII)[nCI++];
          if (nCI == pII->size())
            pII = NULL;
        }
        else
        {
          pObj = sTempObjects[0][nCO++];
          m_RP.m_pCurObject = pObj;
          m_RP.m_FrameObject++;
          pII = pObj->GetInstanceInfo();
          if (pII)
          {
            nCI = 0;
            pI = &(*pII)[nCI++];
            if (nCI == pII->size())
              pII = NULL;
          }
          else
            pI = &pObj->m_II;
        }
        m_RP.m_pCurInstanceInfo = pI;
        fSrc = pI->m_Matrix.GetData();
				if (!(m_RP.m_PersFlags & RBPF_SHADOWGEN) && (m_RP.m_ObjFlags & FOB_TRANS_MASK) && !(m_RP.m_ObjFlags & FOB_CAMERA_SPACE))
        {
          fParm[0] = fSrc[3]-m_rcam.Orig.x; fParm[1] = fSrc[7]-m_rcam.Orig.y; fParm[2] = fSrc[11]-m_rcam.Orig.z; fParm[3] = fSrc[0];
        }
        else
        {
          fParm[0] = fSrc[3]; fParm[1] = fSrc[7]; fParm[2] = fSrc[11]; fParm[3] = fSrc[0];
        }
        fParm += 4;

        if (pVPInst->m_pParams_Inst)
        {
#if defined (DIRECT3D9) || defined (OPENGL)
          fParm = vp->mfSetParameters(&(*pVPInst->m_pParams_Inst)[0], pVPInst->m_pParams_Inst->size(), fParm, eHWSC_Vertex);
#else
          fParm = vp->mfSetParameters(&(*pVPInst->m_pParams_Inst)[0], pVPInst->m_pParams_Inst->size(), fParm, eHWSC_Vertex, 40);
#endif
        }
        if (bConstBasedInstancing)
        {
          float *fInd = (float *)&inddata[n*sizeof(float)];
          *fInd = (float)n*nUsedAttr;
        }
      }
      m_RP.m_pCurObject = curObj;
      m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
      m_RP.m_VB_Inst->Unlock();
      if (bConstBasedInstancing)
      {
#if defined(OPENGL)
#if defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
        CHWShader_D3D::sGlobalConsts[VSCONST_INSTDATA_OPENGL].SetConstantIntoShader
          (gcpRendD3D->GetD3DDevice(), &CHWShader_D3D::m_CurVSParams[VSCONST_INSTDATA].x, n*nUsedAttr);
#endif
#else
#if defined (DIRECT3D9) || defined (OPENGL)
        vp->mfParameterReg_NoCheck(VSCONST_INSTDATA, &CHWShader_D3D::m_CurVSParams[VSCONST_INSTDATA].x, n*nUsedAttr, eHWSC_Vertex);
#endif
#endif
      }

      // Set the first stream to be the indexed data and render N instances
      m_RP.m_ReqStreamFrequence[0] = n | D3DSTREAMSOURCE_INDEXEDDATA;
      if (bConstBasedInstancing)
        m_RP.m_VB_Inst->Bind(3, nOffs, sizeof(float), 1 | D3DSTREAMSOURCE_INSTANCEDATA);
      else
        m_RP.m_VB_Inst->Bind(3, nOffs, nUsedAttr*INST_PARAM_SIZE, 1 | D3DSTREAMSOURCE_INSTANCEDATA);

      FX_DrawInstances(ef, slw, nCurInst, nLastInst, nUsedAttr, nInstAttrMask, Attributes, bConstBasedInstancing);
      nCurInst = nLastInst+1;
    }
  }

  // Rotated instances
  if (nInsts[1])
  {
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_INSTANCING_ROT];

    // Set Pixel shader and all associated textures
    if (!ps->mfSet(HWSF_SETTEXTURES))
      return;

    // Set Vertex shader
    if (!vp->mfSet(HWSF_INSTANCED))
      return;

#if defined(DIRECT3D10)
		CHWShader_D3D *gs = (CHWShader_D3D *)slw->m_GShader;
		if (gs)
			gs->mfSet(0);
		else
			CHWShader_D3D::mfBindGS(0, 0);
#endif

    // Create/Update video mesh (VB management)
    m_RP.m_pRE->mfCheckUpdate(m_RP.m_CurVFormat, m_RP.m_FlagsStreams_Stream);

    CHWShader_D3D::SHWSInstance *pVPInst = vp->m_pCurInst;

    int nUsedAttr = 3;
    Attributes[0] = (byte)pVPInst->m_nInstMatrixID;
    Attributes[1] = Attributes[0]+1;
    Attributes[2] = Attributes[0]+2;
    int nInstAttrMask = 0x7 << pVPInst->m_nInstMatrixID;
    if (pVPInst->m_pParams_Inst)
    {
      int nSize = pVPInst->m_pParams_Inst->size();
      for (j=0; j<nSize; j++)
      {
        SCGParam *pr = &(*pVPInst->m_pParams_Inst)[j];
        for (uint na=0; na<(uint)pr->m_nParameters; na++)
        {
          Attributes[nUsedAttr+na] = pr->m_dwBind+na;
          nInstAttrMask |= 1<<Attributes[nUsedAttr+na];
        }
        nUsedAttr += pr->m_nParameters;
      }
    }
    Matrix44 m;
    uint nCurInst = 0;
    SCGBind bind;

    // Detects possibility of using attributes based instancing
    // If number of used attributes exceed 16 we can't use attributes based instancing (switch to constant based)
    int nStreamMask = m_RP.m_FlagsStreams_Stream >> 1;
    int nVFormat = m_RP.m_CurVFormat;
    int nCO = 0;
    int nCI = 0;
    if (!bConstBasedInstancing && m_RP.m_D3DVertexDeclaration[nStreamMask][nVFormat].m_Declaration.Num()+nUsedAttr-1 > 16)
      iLog->LogWarning("WARNING: Attributes based instancing cannot exceed 16 attributes (%s uses %d attr. + %d vertex decl.attr.)[VF: %d, SM: 0x%x]", vp->GetName(), nUsedAttr, m_RP.m_D3DVertexDeclaration[nStreamMask][nVFormat].m_Declaration.Num()-1, nVFormat, nStreamMask);
    else
    while (nCurInst < nInsts[1])
    {
      uint nLastInst = nInsts[1] - 1;
      uint nParamsPerInstAllowed = bConstBasedInstancing ? MAX_HWINST_PARAMS_CONST : MAX_HWINST_PARAMS;
      if ((nLastInst-nCurInst+1)*nUsedAttr >= nParamsPerInstAllowed)
        nLastInst = nCurInst+(nParamsPerInstAllowed/nUsedAttr)-1;
      byte *data;
      byte *inddata = NULL;
      if (bConstBasedInstancing)
      {
#if defined (DIRECT3D9) || defined (OPENGL)
        data = (byte *)&CHWShader_D3D::m_CurVSParams[VSCONST_INSTDATA].x;
        inddata = (byte *)m_RP.m_VB_Inst->Lock((nLastInst-nCurInst+1)*nUsedAttr*sizeof(float), nOffs);
#else
        assert(0);
#endif
      }
      else
        data = (byte *)m_RP.m_VB_Inst->Lock((nLastInst-nCurInst+1)*nUsedAttr*INST_PARAM_SIZE, nOffs);
      CRenderObject *curObj = m_RP.m_pCurObject;
      n = 0;

      // Fill the stream 3 for per-instance data
      CRenderObject *pObj;
      DynArray16<SInstanceInfo> *pII = NULL;
      SInstanceInfo *pI;
      for (i=nCurInst; i<=nLastInst; i++, n++)
      {
        float *fParm = (float *)&data[n*nUsedAttr*INST_PARAM_SIZE];
        float *fSrc;
        if (pII)
        {
          pI = &(*pII)[nCI++];
          if (nCI == pII->size())
            pII = NULL;
        }
        else
        {
          pObj = sTempObjects[1][nCO++];
          m_RP.m_pCurObject = pObj;
          m_RP.m_FrameObject++;
          pII = pObj->GetInstanceInfo();
          if (pII)
          {
            nCI = 0;
            pI = &(*pII)[nCI++];
            if (nCI == pII->size())
              pII = NULL;
          }
          else
            pI = &pObj->m_II;
        }
        m_RP.m_pCurInstanceInfo = pI;
        fSrc = pI->m_Matrix.GetData();

        if ((m_RP.m_PersFlags & RBPF_SHADOWGEN) || !(m_RP.m_ObjFlags & FOB_TRANS_MASK) || (m_RP.m_ObjFlags & FOB_CAMERA_SPACE))
        {
          fParm[0] = fSrc[0];  fParm[1] = fSrc[1];  fParm[2] = fSrc[2];  fParm[3] = fSrc[3]; 
          fParm[4] = fSrc[4];  fParm[5] = fSrc[5];  fParm[6] = fSrc[6];  fParm[7] = fSrc[7]; 
          fParm[8] = fSrc[8];  fParm[9] = fSrc[9];  fParm[10] = fSrc[10];  fParm[11] = fSrc[11]; 
        }
        else
        {
          fParm[0] = fSrc[0];  fParm[1] = fSrc[1];  fParm[2] = fSrc[2];  fParm[3] = fSrc[3] - m_rcam.Orig.x; 
          fParm[4] = fSrc[4];  fParm[5] = fSrc[5];  fParm[6] = fSrc[6];  fParm[7] = fSrc[7] - m_rcam.Orig.y; 
          fParm[8] = fSrc[8];  fParm[9] = fSrc[9];  fParm[10] = fSrc[10];  fParm[11] = fSrc[11] - m_rcam.Orig.z; 
        }
        fParm += 3*4;

        if (pVPInst->m_pParams_Inst)
        {
#if defined (DIRECT3D9) || defined (OPENGL)
          fParm = vp->mfSetParameters(&(*pVPInst->m_pParams_Inst)[0], pVPInst->m_pParams_Inst->size(), fParm, eHWSC_Vertex);
#else
          fParm = vp->mfSetParameters(&(*pVPInst->m_pParams_Inst)[0], pVPInst->m_pParams_Inst->size(), fParm, eHWSC_Vertex, 40);
#endif
        }
        if (bConstBasedInstancing)
        {
          float *fInd = (float *)&inddata[n*sizeof(float)];
          *fInd = (float)n*nUsedAttr;
        }
      }
      m_RP.m_pCurObject = curObj;
      m_RP.m_VB_Inst->Unlock();

      if (bConstBasedInstancing)
      {
#if defined(OPENGL)
#if defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
        CHWShader_D3D::sGlobalConsts[VSCONST_INSTDATA_OPENGL].SetConstantIntoShader
          (gcpRendD3D->GetD3DDevice(), &CHWShader_D3D::m_CurVSParams[VSCONST_INSTDATA].x, n*nUsedAttr);
#endif
#else
#if defined (DIRECT3D9) || defined (OPENGL)
        vp->mfParameterReg_NoCheck(VSCONST_INSTDATA, &CHWShader_D3D::m_CurVSParams[VSCONST_INSTDATA].x, n*nUsedAttr, eHWSC_Vertex);
#endif
#endif
      }

      // Set the first stream to be the indexed data and render N instances
      m_RP.m_ReqStreamFrequence[0] = n | D3DSTREAMSOURCE_INDEXEDDATA;
      if (bConstBasedInstancing)
        m_RP.m_VB_Inst->Bind(3, nOffs, sizeof(float), 1 | D3DSTREAMSOURCE_INSTANCEDATA);
      else
        m_RP.m_VB_Inst->Bind(3, nOffs, nUsedAttr*INST_PARAM_SIZE, 1 | D3DSTREAMSOURCE_INSTANCEDATA);

      FX_DrawInstances(ef, slw, nCurInst, nLastInst, nUsedAttr, nInstAttrMask, Attributes, bConstBasedInstancing);
      nCurInst = nLastInst+1;
    }
  }

  m_RP.m_PersFlags |= RBPF_USESTREAM<<3;
  m_RP.m_ReqStreamFrequence[0] = 1;
  m_RP.m_ReqStreamFrequence[3] = 1;
}

//====================================================================================

void CD3D9Renderer::FX_DrawBatches(CShader *pSh, SShaderPass *pPass, CHWShader *curVS, CHWShader *curPS)
{
  uint j;

  bool bHWSkinning = FX_SetStreamFlags(pPass);

  // Set culling mode
  if (!(m_RP.m_FlagsPerFlush & RBSI_NOCULL))
  {
    if (pPass->m_eCull != -1)
      D3DSetCull((ECull)pPass->m_eCull);
  }
#if !defined(PS3) || defined(PS3_ACTIVATE_CONSTANT_ARRAYS)//we cannot map it yet, will be removed soon
  CRenderMesh *pWeights = (CRenderMesh *)m_RP.m_pCurObject->GetWeights();

#endif
  if (bHWSkinning && m_RP.m_pCurObject && m_RP.m_pCurObject->m_pCharInstance && !CV_r_character_nodeform)
  {
    PROFILE_FRAME(DrawShader_BatchSkinned);

    m_RP.m_PS.m_NumRendSkinnedObjects++;

    QuatTS *pGlobalSkinQuatsL = NULL;
    QuatTS *pGlobalSkinQuatsS = NULL;
    QuatTS *pMBGlobalSkinQuatsL = NULL;
    QuatTS *pMBGlobalSkinQuatsS = NULL;
    Vec4 HWShapeDeformationData[8];
    CRenderObject *pObj = m_RP.m_pCurObject;
    uint32 HWSkinningFlags = 0;
    assert(pObj->m_pCharInstance);
    CREMesh *pRE = (CREMesh *)m_RP.m_pRE;
    CRenderChunk *pChunk = pRE->m_pChunk;
    uint nBones = 0;

		uint8 * pRemapTable = 0;
    //if (pObj->m_pCharInstance && !CV_r_character_nodeform)
    {
      PROFILE_FRAME(Objects_GetSkinningData);
      int nFlags = 0;
      {
        HWSkinningFlags |= (m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS)? eHWS_MotionBlured: 0;
				HWSkinningFlags |= eHWS_MorphTarget;
        nBones = pObj->m_pCharInstance->GetSkeletonPose(pObj->m_nLod, pObj->m_II.m_Matrix, pGlobalSkinQuatsL, pGlobalSkinQuatsS, pMBGlobalSkinQuatsL, pMBGlobalSkinQuatsS, HWShapeDeformationData, HWSkinningFlags, pRemapTable);
      }
    }
    if (nBones < pChunk->m_arrChunkBoneIDs.size())
    {
      Warning ("Warning: Skinned geometry number of bones mismatch (Mesh: %d, Character instance: %d)", pChunk->m_arrChunkBoneIDs.size(), nBones);
    }
    else
    {
      if (HWSkinningFlags & eHWS_MorphTarget)
      {
        m_RP.m_FlagsStreams_Decl |= VSM_HWSKIN_MORPHTARGET;
        m_RP.m_FlagsStreams_Stream |= VSM_HWSKIN_MORPHTARGET;
      }
      if (HWSkinningFlags & eHWS_ShapeDeform)
      {
        m_RP.m_FlagsStreams_Decl |= VSM_HWSKIN_SHAPEDEFORM;
        m_RP.m_FlagsStreams_Stream |= VSM_HWSKIN_SHAPEDEFORM;
      }

      CHWShader_D3D *curVSD3D = (CHWShader_D3D *)curVS;
      m_RP.m_nNumRendPasses++;

      m_RP.m_RendNumGroup = 0;
      m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SPHERICAL] | g_HWSR_MaskBit[HWSR_SKELETON_SSD]);

      //always use 4 bone per vertex
  #if !defined(PS3) || defined(PS3_ACTIVATE_CONSTANT_ARRAYS)//we cannot map it yet, will be removed soon
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SKELETON_SSD];

      //choose linear- or spherical-skinning
      static ICVar* pCVar_SphericalSkinning = gEnv->pConsole->GetCVar( "ca_SphericalSkinning" );		assert(pCVar_SphericalSkinning);
      static ICVar* pCVar_VegetationSphericalSkinning = gEnv->pConsole->GetCVar( "e_vegetation_sphericalskinning" );		assert(pCVar_VegetationSphericalSkinning);

      int iSphericalSkinning = pCVar_SphericalSkinning->GetIVal();
      int iVegSphericalSkinning = (pCVar_VegetationSphericalSkinning->GetIVal() && (m_RP.m_FlagsShader_RT & g_HWSR_MaskBit[HWSR_VEGETATION]));

      if (iSphericalSkinning || iVegSphericalSkinning)
        m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SPHERICAL];
  #else
      int iSphericalSkinning = 0;//do not activate it for PS3 yet
      int iVegSphericalSkinning = 0;
  #endif
      m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_MORPHTARGET] | g_HWSR_MaskBit[HWSR_SHAPEDEFORM]);
  #if !defined(PS3) || defined(PS3_ACTIVATE_CONSTANT_ARRAYS)//we cannot map it yet, will be removed soon
      if (HWSkinningFlags & eHWS_MorphTarget)
        m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_MORPHTARGET];

      if (HWSkinningFlags & eHWS_ShapeDeform)
        m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SHAPEDEFORM];
  #endif

  #if !defined (DIRECT3D10)
      // Disable perlin-noise based deformations on skinned meshes
      int nMDF = m_RP.m_FlagsShader_MDV & 0xf;
      if (nMDF == eDT_Perlin3D || nMDF == eDT_Perlin2D)
        m_RP.m_FlagsShader_MDV &= ~0xf;
  #endif

			if (pRemapTable) 
			{
				// Set Vertex shader
				bool bRes = curVSD3D->mfSet(0);

				if (bRes)
				{
#if defined (DIRECT3D10)
					if (CHWShader_D3D::m_pCurInstVS && CHWShader_D3D::m_pCurInstVS->m_bFallback)
						return;
#endif
					// Create/Update video mesh (VB management)
					m_RP.m_pRE->mfCheckUpdate(m_RP.m_CurVFormat, m_RP.m_FlagsStreams_Stream);

					uint32 numBonesPerChunk=pChunk->m_arrChunkBoneIDs.size();
					assert( numBonesPerChunk <= NUM_MAX_BONES_PER_GROUP );

					if ((iSphericalSkinning || iVegSphericalSkinning) && pGlobalSkinQuatsS) 
					{
						//copy quaternions
#if defined(OPENGL) && defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
						const bool cIsHandleValid = CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].IsHandleValid();
						if (!cIsHandleValid)
						{
							int count = numBonesPerChunk;
							bool allowPrevFrameSkinning = false;
							if ((m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS)
								&& (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB))
							{
								count += NUM_MAX_BONES_PER_GROUP_WITH_MB;
								allowPrevFrameSkinning = true;
							}
							float *pFirstFrameArray = new float[count * 2 * 4];
							const size_t stride = 2 * 4 * sizeof(float);
							for (j=0; j<numBonesPerChunk; j++)
							{
								uint32 BoneID=pRemapTable[pChunk->m_arrChunkBoneIDs[j]];
								memcpy(
									&pFirstFrameArray[j * 2 * 4],
									&m_RP.pGlobalSkinQuatsS[BoneID],
									stride);
							}
							if (allowPrevFrameSkinning)
							{
								for (; j < NUM_MAX_BONES_PER_GROUP_WITH_MB; ++j)
									memset(&pFirstFrameArray[j * 2 * 4], 0, stride);
								for (; j < count; ++j)
								{
									uint32 BoneID = pRemapTable[pChunk->m_arrChunkBoneIDs[
										j - NUM_MAX_BONES_PER_GROUP_WITH_MB]];
										memcpy(
											&pFirstFrameArray[j * 2 * 4],
											&pGlobalSkinQuatsS[BoneID],
											stride);
								}
							}
							CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].SetConstantIntoShader
								(gcpRendD3D->GetD3DDevice(), pFirstFrameArray, count, 0);
							delete [] pFirstFrameArray;
						}
						else
#endif
						{
#if defined (DIRECT3D10)
							SCGBind *pBind = curVSD3D->mfGetParameterBind("_g_SkinQuat");
							assert(pBind);
#endif
							for (j=0; j<numBonesPerChunk; j++)
							{
								uint32 BoneID=pRemapTable[pChunk->m_arrChunkBoneIDs[j]];
#if defined(OPENGL)
#if defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
								CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].SetConstantIntoShader
									(gcpRendD3D->GetD3DDevice(), &pGlobalSkinQuatsS[BoneID].q.v.x, 2, j);
#endif
#else
#if defined (DIRECT3D9) || defined (OPENGL)
								curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + (j<<1), &pGlobalSkinQuatsS[BoneID].q.v.x, 2, eHWSC_Vertex);
								if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
								{ 
									// if in motion blur pass, and bones count is less than NUM_MAX_BONES_PER_GROUP_WITH_MB, allow previous frame skinning
									curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + ((j + NUM_MAX_BONES_PER_GROUP_WITH_MB)<<1), &pMBGlobalSkinQuatsS[BoneID].q.v.x, 2, eHWSC_Vertex);
								}                      
#elif defined (DIRECT3D10)
								if (pBind)
								{
									int nVecs = numBonesPerChunk * 2;
									if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
										nVecs += NUM_MAX_BONES_PER_GROUP_WITH_MB *2;
									curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + (j<<1)*4, pBind->m_dwCBufSlot, eHWSC_Vertex, &pGlobalSkinQuatsS[BoneID].q.v.x, 2*4, nVecs);
									if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
									{ 
										// if in motion blur pass, and bones count is less than NUM_MAX_BONES_PER_GROUP_WITH_MB, allow previous frame skinning
										curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + ((j + NUM_MAX_BONES_PER_GROUP_WITH_MB)<<1)*4, pBind->m_dwCBufSlot, eHWSC_Vertex, &pMBGlobalSkinQuatsS[BoneID].q.v.x, 2*4, nVecs);
									}                      
								}
#endif
#endif
							}
						}
					}
					else
					{
						//copy matrices
#if defined(OPENGL) && defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
						const bool cIsHandleValid = CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].IsHandleValid();
						if(!cIsHandleValid)
						{
							int count = numBonesPerChunk;
							bool allowPrevFrameSkinning = false;
							if ((m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS)
								&& (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB))
							{
								count += NUM_MAX_BONES_PER_GROUP_WITH_MB;
								allowPrevFrameSkinning = true;
							}
							float *pFirstFrameArray = new float[count * 2 * 4];
							const size_t stride = 2 * 4 * sizeof(float);
							for (j=0; j<numBonesPerChunk; j++)
							{
								uint32 BoneID=pRemapTable[pChunk->m_arrChunkBoneIDs[j]];
								memcpy(
									&pFirstFrameArray[j * 2 * 4],
									&pGlobalSkinQuatsL[BoneID],
									stride);
							}
							if (allowPrevFrameSkinning)
							{
								for (; j < NUM_MAX_BONES_PER_GROUP_WITH_MB; ++j)
									memset(&pFirstFrameArray[j * 2 * 4], 0, stride);
								for (; j < count; ++j)
								{
									uint32 BoneID = pRemapTable[pChunk->m_arrChunkBoneIDs[
										j - NUM_MAX_BONES_PER_GROUP_WITH_MB]];
										memcpy(
											&pFirstFrameArray[j * 2 * 4],
											&pGlobalSkinQuatsL[BoneID],
											stride);
								}
							}
							CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].SetConstantIntoShader
								(gcpRendD3D->GetD3DDevice(), pFirstFrameArray, count, 0);
							delete [] pFirstFrameArray;
#if 0
							float *pFirstFrameArray = new float[numBonesPerChunk * 3 * 4 * sizeof(float)];
							for (j=0; j<numBonesPerChunk; j++)
							{
								uint32 BoneID=pRemapTable[pChunk->m_arrChunkBoneIDs[j]];
								memcpy(&pFirstFrameArray[j * 3 * 4], HWSkinMatrices[BoneID].GetData(), 3 * 4 * sizeof(float));
							}
							CHWShader_D3D::sGlobalConsts[VSCONST_SKINMATRIX_OPENGL].SetConstantIntoShader
								(gcpRendD3D->GetD3DDevice(), pFirstFrameArray, 3 * numBonesPerChunk, 0);
							delete [] pFirstFrameArray;
#endif
						}
						else
#endif
						{
#if defined (DIRECT3D10)
							SCGBind *pBind = curVSD3D->mfGetParameterBind("_g_SkinQuat");
							assert(pBind);
#endif
							for (j=0; j<numBonesPerChunk; j++)
							{
								uint32 GlobalBoneID=pRemapTable[pChunk->m_arrChunkBoneIDs[j]];
								assert(GlobalBoneID<0x100);
								//	uint32 hwbones = m_RP.m_HWSkinMatrices[BoneID].GetData();
								if (GlobalBoneID > 0x100)
									iLog->Log("Error: BoneID is out of range (%d),Id: %d, %d bones", GlobalBoneID, j, pChunk->m_arrChunkBoneIDs.size());
								else
								{
#if defined(OPENGL)
#if defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
									CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].SetConstantIntoShader
										(gcpRendD3D->GetD3DDevice(), &pGlobalSkinQuatsL[GlobalBoneID].q.v.x, 2, j);
#endif
#else
#if defined (DIRECT3D9) || defined (OPENGL)
									curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + (j << 1), &pGlobalSkinQuatsL[GlobalBoneID].q.v.x, 2, eHWSC_Vertex);
									if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
									{ 
										// if in motion blur pass, and bones count is less than NUM_MAX_BONES_PER_GROUP_WITH_MB, allow previous frame skinning
										curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + ((j + NUM_MAX_BONES_PER_GROUP_WITH_MB)<<1), &pMBGlobalSkinQuatsL[GlobalBoneID].q.v.x, 2, eHWSC_Vertex);
									}                      
#elif defined (DIRECT3D10)
									if (pBind)
									{
										int nVecs = numBonesPerChunk * 2;
										if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
											nVecs += NUM_MAX_BONES_PER_GROUP_WITH_MB *2;
										curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + (j << 1) * 4, pBind->m_dwCBufSlot, eHWSC_Vertex, &pGlobalSkinQuatsL[GlobalBoneID].q.v.x, 2*4, nVecs);
										if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
										{ 
											// if in motion blur pass, and bones count is less than NUM_MAX_BONES_PER_GROUP_WITH_MB, allow previous frame skinning
											curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + ((j + NUM_MAX_BONES_PER_GROUP_WITH_MB)<<1)*4, pBind->m_dwCBufSlot, eHWSC_Vertex, &pMBGlobalSkinQuatsL[GlobalBoneID].q.v.x, 2*4, nVecs);
										}                      
									}
#endif
#endif
								}
							}
						}
					}

					if (HWSkinningFlags & eHWS_ShapeDeform)
					{
#if defined(OPENGL)
#if defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
						CHWShader_D3D::sGlobalConsts[VSCONST_SHAPEDEFORMATION_OPENGL].SetConstantIntoShader(gcpRendD3D->GetD3DDevice(), &m_RP.m_HWShapeDeformationData[0].x, 8);
#endif
#else
#if defined (DIRECT3D9) || defined (OPENGL)
						curVSD3D->mfParameterReg(VSCONST_SHAPEDEFORMATION, &HWShapeDeformationData[0].x, 8, eHWSC_Vertex);
#elif defined (DIRECT3D10)
						SCGBind *pBind = curVSD3D->mfGetParameterBind("_g_ShapeDeformationData");
						assert(pBind);
						if (pBind)
							curVSD3D->mfParameterReg(VSCONST_SHAPEDEFORMATION, pBind->m_dwCBufSlot, eHWSC_Vertex, &HWShapeDeformationData[0].x, 8*4, 8);
#endif
#endif
					}
					int bFogOverrided = EF_FogCorrection();

					// Unlock all VB (if needed) and set current streams
					if (FX_CommitStreams(pPass))
					{
						uint nObj = 0;
						CRenderObject *pSaveObj = m_RP.m_pCurObject;
						CRenderObject *pObj = pSaveObj;
						while (true)
						{       
							if (nObj)
							{
								m_RP.m_pCurObject = m_RP.m_ObjectInstances[nObj];
								m_RP.m_FrameObject++;
								pObj = m_RP.m_pCurObject;
							}
							m_RP.m_pCurInstanceInfo = &pObj->m_II;

#ifdef DO_RENDERLOG
							if (CRenderer::CV_r_log >= 3)
							{
								Vec3 vPos = pObj->GetTranslation();
								Logv(SRendItem::m_RecurseLevel, "+++ HWSkin Group Pass %d (Obj: %d [%.3f, %.3f, %.3f])\n", m_RP.m_nNumRendPasses, pObj->m_VisId, vPos[0], vPos[1], vPos[2]);
							}
#endif

							curVSD3D->mfSetParameters(1);

							if (curPS)
								curPS->mfSetParameters(1);

#if defined(DIRECT3D10) && !defined(PS3)
							if (pPass->m_GShader)
								pPass->m_GShader->mfSetParameters(1);
							else
								CHWShader_D3D::mfBindGS(NULL, NULL);
#endif

							{
								//PROFILE_FRAME(Draw_ShaderIndexMesh);
								if (m_RP.m_pRE)
									m_RP.m_pRE->mfDraw(pSh, pPass);
								else
									EF_DrawIndexedMesh(R_PRIMV_TRIANGLES);
							}
							nObj++;
							if (nObj >= m_RP.m_ObjectInstances.Num())
								break;
						}
						EF_FogRestore(bFogOverrided);
						m_RP.m_FlagsShader_MD &= ~(HWMD_TCM | HWMD_TCG);
						if (pSaveObj != m_RP.m_pCurObject)
						{
							m_RP.m_pCurObject = pSaveObj;
							m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
							m_RP.m_FrameObject++;
						}
					}
				}

			}
			else
			{
				// Set Vertex shader
				bool bRes = curVSD3D->mfSet(0);

				if (bRes)
				{
#if defined (DIRECT3D10)
					if (CHWShader_D3D::m_pCurInstVS && CHWShader_D3D::m_pCurInstVS->m_bFallback)
						return;
#endif
					// Create/Update video mesh (VB management)
					m_RP.m_pRE->mfCheckUpdate(m_RP.m_CurVFormat, m_RP.m_FlagsStreams_Stream);

					uint32 numBonesPerChunk=pChunk->m_arrChunkBoneIDs.size();
					assert( numBonesPerChunk <= NUM_MAX_BONES_PER_GROUP );

					if ((iSphericalSkinning || iVegSphericalSkinning) && pGlobalSkinQuatsS) 
					{
						//copy quaternions
#if defined(OPENGL) && defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
						const bool cIsHandleValid = CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].IsHandleValid();
						if (!cIsHandleValid)
						{
							int count = numBonesPerChunk;
							bool allowPrevFrameSkinning = false;
							if ((m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS)
								&& (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB))
							{
								count += NUM_MAX_BONES_PER_GROUP_WITH_MB;
								allowPrevFrameSkinning = true;
							}
							float *pFirstFrameArray = new float[count * 2 * 4];
							const size_t stride = 2 * 4 * sizeof(float);
							for (j=0; j<numBonesPerChunk; j++)
							{
								uint32 BoneID=pChunk->m_arrChunkBoneIDs[j];
								memcpy(
									&pFirstFrameArray[j * 2 * 4],
									&m_RP.pGlobalSkinQuatsS[BoneID],
									stride);
							}
							if (allowPrevFrameSkinning)
							{
								for (; j < NUM_MAX_BONES_PER_GROUP_WITH_MB; ++j)
									memset(&pFirstFrameArray[j * 2 * 4], 0, stride);
								for (; j < count; ++j)
								{
									uint32 BoneID = pChunk->m_arrChunkBoneIDs[
										j - NUM_MAX_BONES_PER_GROUP_WITH_MB];
										memcpy(
											&pFirstFrameArray[j * 2 * 4],
											&pGlobalSkinQuatsS[BoneID],
											stride);
								}
							}
							CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].SetConstantIntoShader
								(gcpRendD3D->GetD3DDevice(), pFirstFrameArray, count, 0);
							delete [] pFirstFrameArray;
						}
						else
#endif
						{
#if defined (DIRECT3D10)
							SCGBind *pBind = curVSD3D->mfGetParameterBind("_g_SkinQuat");
							assert(pBind);
#endif
							for (j=0; j<numBonesPerChunk; j++)
							{
								uint32 BoneID=pChunk->m_arrChunkBoneIDs[j];
#if defined(OPENGL)
#if defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
								CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].SetConstantIntoShader
									(gcpRendD3D->GetD3DDevice(), &pGlobalSkinQuatsS[BoneID].q.v.x, 2, j);
#endif
#else
#if defined (DIRECT3D9) || defined (OPENGL)
								curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + (j<<1), &pGlobalSkinQuatsS[BoneID].q.v.x, 2, eHWSC_Vertex);
								if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
								{ 
									// if in motion blur pass, and bones count is less than NUM_MAX_BONES_PER_GROUP_WITH_MB, allow previous frame skinning
									curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + ((j + NUM_MAX_BONES_PER_GROUP_WITH_MB)<<1), &pMBGlobalSkinQuatsS[BoneID].q.v.x, 2, eHWSC_Vertex);
								}                      
#elif defined (DIRECT3D10)
								if (pBind)
								{
									int nVecs = numBonesPerChunk * 2;
									if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
										nVecs += NUM_MAX_BONES_PER_GROUP_WITH_MB *2;
									curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + (j<<1)*4, pBind->m_dwCBufSlot, eHWSC_Vertex, &pGlobalSkinQuatsS[BoneID].q.v.x, 2*4, nVecs);
									if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
									{ 
										// if in motion blur pass, and bones count is less than NUM_MAX_BONES_PER_GROUP_WITH_MB, allow previous frame skinning
										curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + ((j + NUM_MAX_BONES_PER_GROUP_WITH_MB)<<1)*4, pBind->m_dwCBufSlot, eHWSC_Vertex, &pMBGlobalSkinQuatsS[BoneID].q.v.x, 2*4, nVecs);
									}                      
								}
#endif
#endif
							}
						}
					}
					else
					{
						//copy matrices
#if defined(OPENGL) && defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
						const bool cIsHandleValid = CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].IsHandleValid();
						if(!cIsHandleValid)
						{
							int count = numBonesPerChunk;
							bool allowPrevFrameSkinning = false;
							if ((m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS)
								&& (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB))
							{
								count += NUM_MAX_BONES_PER_GROUP_WITH_MB;
								allowPrevFrameSkinning = true;
							}
							float *pFirstFrameArray = new float[count * 2 * 4];
							const size_t stride = 2 * 4 * sizeof(float);
							for (j=0; j<numBonesPerChunk; j++)
							{
								uint32 BoneID=pChunk->m_arrChunkBoneIDs[j];
								memcpy(
									&pFirstFrameArray[j * 2 * 4],
									&pGlobalSkinQuatsL[BoneID],
									stride);
							}
							if (allowPrevFrameSkinning)
							{
								for (; j < NUM_MAX_BONES_PER_GROUP_WITH_MB; ++j)
									memset(&pFirstFrameArray[j * 2 * 4], 0, stride);
								for (; j < count; ++j)
								{
									uint32 BoneID = pChunk->m_arrChunkBoneIDs[
										j - NUM_MAX_BONES_PER_GROUP_WITH_MB];
										memcpy(
											&pFirstFrameArray[j * 2 * 4],
											&pGlobalSkinQuatsL[BoneID],
											stride);
								}
							}
							CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].SetConstantIntoShader
								(gcpRendD3D->GetD3DDevice(), pFirstFrameArray, count, 0);
							delete [] pFirstFrameArray;
#if 0
							float *pFirstFrameArray = new float[numBonesPerChunk * 3 * 4 * sizeof(float)];
							for (j=0; j<numBonesPerChunk; j++)
							{
								uint32 BoneID=pChunk->m_arrChunkBoneIDs[j];
								memcpy(&pFirstFrameArray[j * 3 * 4], HWSkinMatrices[BoneID].GetData(), 3 * 4 * sizeof(float));
							}
							CHWShader_D3D::sGlobalConsts[VSCONST_SKINMATRIX_OPENGL].SetConstantIntoShader
								(gcpRendD3D->GetD3DDevice(), pFirstFrameArray, 3 * numBonesPerChunk, 0);
							delete [] pFirstFrameArray;
#endif
						}
						else
#endif
						{
#if defined (DIRECT3D10)
							SCGBind *pBind = curVSD3D->mfGetParameterBind("_g_SkinQuat");
							assert(pBind);
#endif
							for (j=0; j<numBonesPerChunk; j++)
							{
								uint32 GlobalBoneID=pChunk->m_arrChunkBoneIDs[j];
								assert(GlobalBoneID<0x100);
								//	uint32 hwbones = m_RP.m_HWSkinMatrices[BoneID].GetData();
								if (GlobalBoneID > 0x100)
									iLog->Log("Error: BoneID is out of range (%d),Id: %d, %d bones", GlobalBoneID, j, pChunk->m_arrChunkBoneIDs.size());
								else
								{
#if defined(OPENGL)
#if defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
									CHWShader_D3D::sGlobalConsts[VSCONST_SKINQUATSL_OPENGL].SetConstantIntoShader
										(gcpRendD3D->GetD3DDevice(), &pGlobalSkinQuatsL[GlobalBoneID].q.v.x, 2, j);
#endif
#else
#if defined (DIRECT3D9) || defined (OPENGL)
									curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + (j << 1), &pGlobalSkinQuatsL[GlobalBoneID].q.v.x, 2, eHWSC_Vertex);
									if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
									{ 
										// if in motion blur pass, and bones count is less than NUM_MAX_BONES_PER_GROUP_WITH_MB, allow previous frame skinning
										curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + ((j + NUM_MAX_BONES_PER_GROUP_WITH_MB)<<1), &pMBGlobalSkinQuatsL[GlobalBoneID].q.v.x, 2, eHWSC_Vertex);
									}                      
#elif defined (DIRECT3D10)
									if (pBind)
									{
										int nVecs = numBonesPerChunk * 2;
										if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
											nVecs += NUM_MAX_BONES_PER_GROUP_WITH_MB *2;
										curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + (j << 1) * 4, pBind->m_dwCBufSlot, eHWSC_Vertex, &pGlobalSkinQuatsL[GlobalBoneID].q.v.x, 2*4, nVecs);
										if( m_RP.m_PersFlags2 & RBPF2_MOTIONBLURPASS && (numBonesPerChunk < NUM_MAX_BONES_PER_GROUP_WITH_MB) )
										{ 
											// if in motion blur pass, and bones count is less than NUM_MAX_BONES_PER_GROUP_WITH_MB, allow previous frame skinning
											curVSD3D->mfParameterReg(VSCONST_SKINMATRIX + ((j + NUM_MAX_BONES_PER_GROUP_WITH_MB)<<1)*4, pBind->m_dwCBufSlot, eHWSC_Vertex, &pMBGlobalSkinQuatsL[GlobalBoneID].q.v.x, 2*4, nVecs);
										}                      
									}
#endif
#endif
								}
							}
						}
					}

					if (HWSkinningFlags & eHWS_ShapeDeform)
					{
#if defined(OPENGL)
#if defined(PS3_ACTIVATE_CONSTANT_ARRAYS)
						CHWShader_D3D::sGlobalConsts[VSCONST_SHAPEDEFORMATION_OPENGL].SetConstantIntoShader(gcpRendD3D->GetD3DDevice(), &m_RP.m_HWShapeDeformationData[0].x, 8);
#endif
#else
#if defined (DIRECT3D9) || defined (OPENGL)
						curVSD3D->mfParameterReg(VSCONST_SHAPEDEFORMATION, &HWShapeDeformationData[0].x, 8, eHWSC_Vertex);
#elif defined (DIRECT3D10)
						SCGBind *pBind = curVSD3D->mfGetParameterBind("_g_ShapeDeformationData");
						assert(pBind);
						if (pBind)
							curVSD3D->mfParameterReg(VSCONST_SHAPEDEFORMATION, pBind->m_dwCBufSlot, eHWSC_Vertex, &HWShapeDeformationData[0].x, 8*4, 8);
#endif
#endif
					}
					int bFogOverrided = EF_FogCorrection();

					// Unlock all VB (if needed) and set current streams
					if (FX_CommitStreams(pPass))
					{
						uint nObj = 0;
						CRenderObject *pSaveObj = m_RP.m_pCurObject;
						CRenderObject *pObj = pSaveObj;
						while (true)
						{       
							if (nObj)
							{
								m_RP.m_pCurObject = m_RP.m_ObjectInstances[nObj];
								m_RP.m_FrameObject++;
								pObj = m_RP.m_pCurObject;
							}
							m_RP.m_pCurInstanceInfo = &pObj->m_II;

#ifdef DO_RENDERLOG
							if (CRenderer::CV_r_log >= 3)
							{
								Vec3 vPos = pObj->GetTranslation();
								Logv(SRendItem::m_RecurseLevel, "+++ HWSkin Group Pass %d (Obj: %d [%.3f, %.3f, %.3f])\n", m_RP.m_nNumRendPasses, pObj->m_VisId, vPos[0], vPos[1], vPos[2]);
							}
#endif

							curVSD3D->mfSetParameters(1);

							if (curPS)
								curPS->mfSetParameters(1);

#if defined(DIRECT3D10) && !defined(PS3)
							if (pPass->m_GShader)
								pPass->m_GShader->mfSetParameters(1);
							else
								CHWShader_D3D::mfBindGS(NULL, NULL);
#endif

							{
								//PROFILE_FRAME(Draw_ShaderIndexMesh);
								if (m_RP.m_pRE)
									m_RP.m_pRE->mfDraw(pSh, pPass);
								else
									EF_DrawIndexedMesh(R_PRIMV_TRIANGLES);
							}
							nObj++;
							if (nObj >= m_RP.m_ObjectInstances.Num())
								break;
						}
						EF_FogRestore(bFogOverrided);
						m_RP.m_FlagsShader_MD &= ~(HWMD_TCM | HWMD_TCG);
						if (pSaveObj != m_RP.m_pCurObject)
						{
							m_RP.m_pCurObject = pSaveObj;
							m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
							m_RP.m_FrameObject++;
						}
					}
				}
			}
    }
    m_RP.m_RendNumGroup = -1;
  }
  else
  {
    if (bHWSkinning && (!m_RP.m_pCurObject || !m_RP.m_pCurObject->m_pCharInstance))
      Warning ("Warning: Skinned geometry used without character instance");

    PROFILE_FRAME(DrawShader_BatchStatic);

    // Set Vertex shader
    bool bRes = curVS->mfSet(0);

#if defined (DIRECT3D10) && !defined(PS3)
    if (pPass->m_GShader)
      bRes = pPass->m_GShader->mfSet(0);
    else
      CHWShader_D3D::mfBindGS(NULL, NULL);
#endif

    if (bRes)
    {
      // Create/Update video mesh (VB management)
      if (m_RP.m_pRE)
        m_RP.m_pRE->mfCheckUpdate(m_RP.m_CurVFormat, m_RP.m_FlagsStreams_Stream);

      m_RP.m_nNumRendPasses++;

      int bFogOverrided = EF_FogCorrection();

      // Unlock all VB (if needed) and set current streams
      if (FX_CommitStreams(pPass))
      {
        uint nO = 0, nI = 0;
        CRenderObject *pSaveObj = m_RP.m_pCurObject;
        CRenderObject *pObj = NULL;
        DynArray16<SInstanceInfo> *pII = NULL;
        SInstanceInfo *pI;
        while (true)
        {
          if (pII)
          {
            pI = &(*pII)[nI++];
            if (nI == pII->size())
              pII = NULL;
          }
          else
          {
            if (nO >= m_RP.m_ObjectInstances.Num())
            {
              if (pObj)
                break;
              pObj = m_RP.m_pCurObject;
            }
            else
            {
              pObj = m_RP.m_ObjectInstances[nO++];
              m_RP.m_pCurObject = pObj;
              m_RP.m_FrameObject++;
            }
            pII = pObj->GetInstanceInfo();
            if (pII)
            {
              nI = 0;
              pI = &(*pII)[nI++];
              if (nI == pII->size())
                pII = NULL;
            }
            else
              pI = &pObj->m_II;
          }
          m_RP.m_pCurInstanceInfo = pI;

  #ifdef DO_RENDERLOG
          if (CRenderer::CV_r_log >= 3)
          {
            Vec3 vPos = pObj->GetTranslation();
            Logv(SRendItem::m_RecurseLevel, "+++ General Pass %d (Obj: %d [%.3f, %.3f, %.3f])\n", m_RP.m_nNumRendPasses, pObj->m_VisId, vPos[0], vPos[1], vPos[2]);
          }
  #endif

          curVS->mfSetParameters(1);
          if (curPS)
            curPS->mfSetParameters(1);

  #if defined (DIRECT3D10) && !defined(PS3)
          if (pPass->m_GShader)
            pPass->m_GShader->mfSetParameters(1);
          else
            CHWShader_D3D::mfBindGS(NULL, NULL);
  #endif

          {
            if (m_RP.m_pRE)
              m_RP.m_pRE->mfDraw(pSh, pPass);
            else
              EF_DrawIndexedMesh(R_PRIMV_TRIANGLES);
          }
        }
        EF_FogRestore(bFogOverrided);
        m_RP.m_FlagsShader_MD &= ~(HWMD_TCM | HWMD_TCG);
        if (pSaveObj != m_RP.m_pCurObject)
        {
          m_RP.m_pCurObject = pSaveObj;
          m_RP.m_pCurInstanceInfo = &pSaveObj->m_II;
          m_RP.m_FrameObject++;
        }
      }
    }
  }
}

//============================================================================================

void CD3D9Renderer::FX_DrawShader_General(CShader *ef, SShaderTechnique *pTech, bool bUseZState, bool bUseMaterialState)
{
  SShaderPass *slw;
  uint i;

  PROFILE_FRAME(DrawShader_Generic);

  if ((m_RP.m_nBatchFilter & FB_Z) && (m_RP.m_ObjFlags & FOB_DISSOLVE))
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DISSOLVE];

  SShaderTechnique *pPrevTech = m_RP.m_pCurTechnique;
  m_RP.m_pCurTechnique = pTech;

  EF_Scissor(false, 0,0,0,0);

  if( pTech->m_Passes.Num() )
  {
    slw = &pTech->m_Passes[0];
    const int nCount = pTech->m_Passes.Num();
    for (i=0; i<nCount; i++, slw++)
    {    
      m_RP.m_pCurPass = slw;

      // Set all textures and HW TexGen modes for the current pass (ShadeLayer)
      assert (slw->m_VShader && slw->m_PShader);
      if (!slw->m_VShader || !slw->m_PShader)
        continue;
      CHWShader *curVS = slw->m_VShader;
      CHWShader *curPS = slw->m_PShader;

      if (m_RP.m_PersFlags & RBPF_SHADOWGEN)
      {
        if (slw->m_eCull == eCULL_None)
            m_cEF.m_TempVecs[1][0] = m_RP.m_vFrustumInfo.w;
      }

      FX_CommitStates(pTech, slw, (slw->m_PassFlags & SHPF_NOMATSTATE) == 0);

      if (m_RP.m_FlagsPerFlush & RBSI_INSTANCED)
      {
        // Using HW geometry instancing approach
        FX_DrawShader_InstancedHW(ef, slw);
      }
      else
      {
        // Set Pixel shader and all associated textures
        bool bRes = curPS->mfSet(HWSF_SETTEXTURES);
        if (bRes)
          FX_DrawBatches(ef, slw, curVS, curPS);
      }
    }
  }

  m_RP.m_pCurTechnique = pPrevTech;
}

// Draw terrain pass(es)
void CD3D9Renderer::FX_DrawShader_Terrain(CShader *ef, SShaderTechnique *pTech)
{
  // Light terrain layers
  FX_DrawMultiLightPasses(ef, pTech, 0);
}



// Draw detail textures passes (used in programmable pipeline shaders)
void CD3D9Renderer::FX_DrawDetailOverlayPasses()
{    
  if (!m_RP.m_pRootTechnique || m_RP.m_pRootTechnique->m_nTechnique[TTYPE_DETAIL] < 0)
    return;

  SEfResTexture *rt = m_RP.m_pShaderResources->m_Textures[EFTT_DETAIL_OVERLAY];
  CShader *sh = m_RP.m_pShader;
  SShaderTechnique *pTech = m_RP.m_pShader->m_HWTechniques[m_RP.m_pRootTechnique->m_nTechnique[TTYPE_DETAIL]];

  PROFILE_FRAME(DrawShader_DetailPasses);

  sTempObjects[0].SetUse(0);

  float fDistToCam = 500.0f;
  float fDist = CV_r_detaildistance;
  if (m_RP.m_pRE)
  {
    CRenderObject *pObj = m_RP.m_pCurObject;
    uint nObj = 0;
    while (true)
    {
      float fDistObj = pObj->m_fDistance;
      if (fDistObj <= fDist+4.0f && !((pObj->m_nMaterialLayers&MTL_LAYER_BLEND_CLOAK)>>8)  ) // special case: skip if using  cloak layer
        sTempObjects[0].AddElem(pObj);
      nObj++;
      if (nObj >= m_RP.m_ObjectInstances.Num())
        break;
      pObj = m_RP.m_ObjectInstances[nObj];
    }
  } //
  else
    return;

  if (!sTempObjects[0].Num())
    return;

  uint64 nSaveRT = m_RP.m_FlagsShader_RT;
  uint nSaveMD = m_RP.m_FlagsShader_MD;
  //m_RP.m_FlagsShader_RT = 0;

  TArray<CRenderObject *> saveArr;
  saveArr.Assign(m_RP.m_ObjectInstances);
  CRenderObject *pSaveObject = m_RP.m_pCurObject;

  m_RP.m_ObjectInstances = sTempObjects[0];
  m_RP.m_pCurObject = m_RP.m_ObjectInstances[0];
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  m_RP.m_FlagsShader_MD &= ~(HWMD_TCM | HWMD_TCG);

  Vec4 data;
  void *pCustomData = m_RP.m_pRE->m_CustomData;
  m_RP.m_pRE->m_CustomData = &data.x;
  float fUScale = rt->m_TexModificator.m_Tiling[0];
  float fVScale = rt->m_TexModificator.m_Tiling[1];
  if (!fUScale)
    fUScale = CV_r_detailscale;
  if (!fVScale)
    fVScale = CV_r_detailscale;
  data.x = fUScale; data.y = fVScale;
  data.z = 0;
  data.w = CV_r_detaildistance;
  uint n = CLAMP(CV_r_detailnumlayers, 1, 3);
  m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3]);
  if (n >= 1)
  {
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
    if (n >= 2)
    {
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
      if (n >= 3)
        m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE3];
    }
  }
  
  FX_DrawTechnique(sh, pTech, true, false);

  m_RP.m_ObjectInstances.Assign(saveArr);
  saveArr.ClearArr();

  m_RP.m_pCurObject = pSaveObject;
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  m_RP.m_pPrevObject = NULL;
  m_RP.m_FrameObject++;

  m_RP.m_FlagsShader_RT = nSaveRT;
  m_RP.m_FlagsShader_MD = nSaveMD;
  m_RP.m_pRE->m_CustomData = pCustomData;
}

void CD3D9Renderer::FX_DrawCausticsPasses( )
{
  // todo: test if stencil pre-pass is worth when above water
//
  if( (!(m_RP.m_nRendFlags & SHDF_ALLOW_WATER) && !(m_RP.m_PersFlags2&RBPF_MIRRORCAMERA)) || !m_RP.m_pRootTechnique || m_RP.m_pRootTechnique->m_nTechnique[TTYPE_CAUSTICS] < 0 )
    return;
  
  if( !m_RP.m_pCurObject || !m_RP.m_pShader )
    return;

  if( m_RP.m_pCurObject->m_ObjFlags & FOB_DECAL || (m_RP.m_pShader->m_Flags & EF_DECAL) )
    return;
  
  static int nFrameID = 0;
  static bool bOceanVolumeVisible = false;
  static ICVar *pVar = iConsole->GetCVar("e_water_ocean");
  static bool bOceanEnabled = 0;
  static float fWatLevel = 0;
  static Vec4 pCausticsParams;

  int nCurrFrameID = GetFrameID();

  // Only get 3dengine data once..
  if( nFrameID != nCurrFrameID )
  {
    nFrameID = nCurrFrameID;
    bOceanVolumeVisible = (iSystem->GetI3DEngine()->GetOceanRenderFlags() & OCR_OCEANVOLUME_VISIBLE) != 0;
    pCausticsParams = iSystem->GetI3DEngine()->GetCausticsParams();
    fWatLevel = iSystem->GetI3DEngine()->GetWaterLevel();
  }

  if( !bOceanVolumeVisible )
    return;

  CShader *sh = m_RP.m_pShader;
  SShaderTechnique *pTech = m_RP.m_pShader->m_HWTechniques[m_RP.m_pRootTechnique->m_nTechnique[TTYPE_CAUSTICS]];

  PROFILE_FRAME(DrawShader_CausticsPasses);

  sTempObjects[0].SetUse(0);

  Vec3 pMinStart;
  float fDistToCam = 500.0f;
  float fDist = pCausticsParams.y;
  if (m_RP.m_pRE)
  {
    CRenderObject *pObj = m_RP.m_pCurObject;
    uint nObj = 0;
 
    AABB bb;
    m_RP.m_pRE->mfGetBBox(bb.min, bb.max);        
    
    Vec3 pMin, pMinOS;        
    pMinStart = bb.min;

    while (true)
    {
      float fDistObj = pObj->m_fDistance;
                  
      if (fDistObj <= fDist+4.0f && !(pObj->m_nMaterialLayers&MTL_LAYER_BLEND_CLOAK) ) // special case: disable caustics when cloak active
      {

        AABB bbObj = bb.CreateTransformedAABB(pObj->GetMatrix(), bb);  
        float fBBRadius = bbObj.GetRadius();

        if( m_RP.m_nPassGroupID != EFSLIST_TERRAINLAYER )
          pMin = bbObj.GetCenter();
        else  
          pMin = bbObj.min;
        
        if (pMin.z < fWatLevel )
          sTempObjects[0].AddElem(pObj);
      }

      nObj++;
      if (nObj >= m_RP.m_ObjectInstances.Num())
        break;

      pObj = m_RP.m_ObjectInstances[nObj];
    }
  }
  else
    return;

  if (!sTempObjects[0].Num())
    return;

  uint64 nSaveRT = m_RP.m_FlagsShader_RT;
//  m_RP.m_FlagsShader_RT = 0;

  Vec4 data = Vec4(pMinStart.x, pMinStart.y, pMinStart.z, 1.0f);
  void *pCustomData = m_RP.m_pRE->m_CustomData;
  if( m_RP.m_nPassGroupID != EFSLIST_TERRAINLAYER )
    m_RP.m_pRE->m_CustomData = &data.x;

  TArray<CRenderObject *> saveArr;
  saveArr.Assign(m_RP.m_ObjectInstances);
  CRenderObject *pSaveObject = m_RP.m_pCurObject;

  m_RP.m_ObjectInstances = sTempObjects[0];
  m_RP.m_pCurObject = m_RP.m_ObjectInstances[0];
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;

  int nPersFlags2Save = m_RP.m_PersFlags2;

  static int nStencilFrameID = 0;
  if( CRenderer::CV_r_watercaustics == 2 )
  { 
    if( nStencilFrameID != GetFrameID())
    {
      nStencilFrameID = GetFrameID();

      EF_ResetPipe();     
      // stencil pre-pass
      CShader *pSH( CShaderMan::m_ShaderShadowMaskGen );
      //EF_ClearBuffers(FRT_CLEAR_STENCIL|FRT_CLEAR_IMMEDIATE, NULL, 1); 

      // make box for stencil passes
      t_arrDeferredMeshIndBuff arrDeferredInds;
      t_arrDeferredMeshVertBuff arrDeferredVerts;
      CreateDeferredUnitBox(arrDeferredInds, arrDeferredVerts);

      Vec3 vCamPos = gRenDev->GetRCamera().Orig;
      float fWaterPlaneSize = gRenDev->GetCamera().GetFarPlane();

      m_matView->Push();
      Matrix34 mLocal;
      mLocal.SetIdentity();
      
      mLocal.SetScale(Vec3(fDist*2, fDist*2, fWatLevel + 3.0f));//,boxOcean.max);
      mLocal.SetTranslation( Vec3(vCamPos.x-fDist, vCamPos.y-fDist, -2) );
      Matrix44 mLocalTransposed = GetTransposed44(Matrix44(mLocal));
      m_matView->MultMatrixLocal(&mLocalTransposed);

      uint nPasses = 0;         
      static CCryName TechName0 = "DeferredShadowPass";
      pSH->FXSetTechnique(TechName0);
      pSH->FXBegin( &nPasses, FEF_DONTSETSTATES );
      pSH->FXBeginPass( 2 );

      int nVertOffs, nIndOffs;

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

      if (!FAILED(EF_SetVertexDeclaration( 0, VERTEX_FORMAT_P3F_TEX2F )))
        FX_StencilCullPass(-1, nVertOffs, arrDeferredVerts.size(), nIndOffs, arrDeferredInds.size());

      pSH->FXEndPass();
      pSH->FXEnd();

      m_matView->Pop();
    }

    EF_SetStencilState(
      STENC_FUNC(FSS_STENCFUNC_EQUAL) |
      STENCOP_FAIL(FSS_STENCOP_KEEP) |
      STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
      STENCOP_PASS(FSS_STENCOP_KEEP),
      m_nStencilMaskRef, 0xFFFFFFFF, 0xFFFFFFFF);

    m_RP.m_PersFlags2 |=RBPF2_LIGHTSTENCILCULL;
  }

  FX_DrawTechnique(sh, pTech, true, false);

  m_RP.m_ObjectInstances.Assign(saveArr);
  saveArr.ClearArr();

  m_RP.m_pCurObject = pSaveObject;
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  m_RP.m_pPrevObject = NULL;
  m_RP.m_FrameObject++;

  m_RP.m_FlagsShader_RT = nSaveRT;  
  m_RP.m_PersFlags2 = nPersFlags2Save;

  if( m_RP.m_nPassGroupID != EFSLIST_TERRAINLAYER )
    m_RP.m_pRE->m_CustomData = pCustomData;
}

void CD3D9Renderer::FX_SetupMultiLayers( bool bEnable )
{
  CREMesh *pRE = (CREMesh *)m_RP.m_pRE;
  CRenderObject *pObj =  m_RP.m_pCurObject;

  if ( !pObj || !(pObj->m_nMaterialLayers&MTL_LAYER_BLEND_FROZEN) || !pRE )
    return; 

  if( (SRendItem::m_RecurseLevel > 1 && !(m_RP.m_PersFlags & RBPF_MAKESPRITE)) )
    return;

  IMaterial *pObjMat = pObj->m_pCurrMaterial;  
  if( !m_RP.m_pShaderResources || !pObjMat )
    return;

  uint32 nResourcesNoDrawFlags = m_RP.m_pShaderResources->GetMtlLayerNoDrawFlags();
  if( (nResourcesNoDrawFlags&MTL_LAYER_FROZEN) )
    return;

  if( m_RP.m_pCurObject->m_ObjFlags & FOB_DECAL || (m_RP.m_pShader->m_Flags & EF_DECAL) )
    return;

  PROFILE_FRAME(SetupMultiLayers);

  // Verify if current mesh has valid data for layers  
  static IMaterial *pDefaultMtl = iSystem->GetI3DEngine()->GetMaterialManager()->GetDefaultLayersMaterial();
  static SShaderItem pLayerShaderItem;

  static SRenderShaderResources *pPrevShaderResources = 0; 
  static SEfResTexture **pPrevResourceTexs;
  static SEfResTexture *pPrevLayerResourceTexs[EFTT_MAX];

  bool bDefaultLayer = false;

  if( bEnable )
  {    
    
    m_RP.m_pReplacementShader = 0;

    CRenderChunk *pChunk = pRE->m_pChunk;
    if( !pChunk )
      return;

    // Check if chunk material has layers at all    
    IMaterial *pCurrMtl = pObjMat->GetSubMtlCount()? pObjMat->GetSubMtl( pChunk->m_nMatID ) : pObjMat; 
    if( !pCurrMtl || !pDefaultMtl || (pCurrMtl->GetFlags() & MTL_FLAG_NODRAW ) )
      return;

    // Atm only frozen layer supports replacing of general pass
    uint8 nMaterialLayers = ((pObj->m_nMaterialLayers&MTL_LAYER_BLEND_FROZEN)>>24)? MTL_LAYER_FROZEN : 0;

    IMaterialLayer *pDefaultLayer = const_cast< IMaterialLayer* >( pDefaultMtl->GetLayer( nMaterialLayers, 0 ) ); 
    IMaterialLayer *pLayer = const_cast< IMaterialLayer* >( pCurrMtl->GetLayer( nMaterialLayers, 0 ) );      
    if( !pLayer )
    {
      bDefaultLayer = true;
      pLayer = pDefaultLayer;      

      if( !pLayer )
        return;
    }    

    if( !pLayer->IsEnabled() )
      return;

    pLayerShaderItem = pLayer->GetShaderItem();

    // Check for valid shader
    CShader *pShader = static_cast< CShader * >(pLayerShaderItem.m_pShader);
    if( !pShader || !pShader->m_HWTechniques.Num() || !(pShader->m_Flags2&EF2_SUPPORTS_REPLACEBASEPASS) || !pLayerShaderItem.m_pShaderResources )
      return;

    // Custom textures replacement    
    pPrevResourceTexs = m_RP.m_pShaderResources->m_Textures;
    pPrevShaderResources = m_RP.m_pShaderResources; 

    // Keep layer resources and replace with resources from base shader
    for(int t = 0; t < EFTT_MAX; ++t) 
    {
      pPrevLayerResourceTexs[t] = ((SRenderShaderResources *)pLayerShaderItem.m_pShaderResources)->m_Textures[t];
      ((SRenderShaderResources *)pLayerShaderItem.m_pShaderResources)->m_Textures[t] = pPrevResourceTexs[t];
    }

    if( bDefaultLayer )
    {
      // Get opacity/alpha test values (only required for default layer, which has no information)
      ((SRenderShaderResources *)pLayerShaderItem.m_pShaderResources)->m_AlphaRef = pPrevShaderResources->m_AlphaRef;
      
      if( pPrevShaderResources->m_ResFlags & MTL_FLAG_2SIDED )
        ((SRenderShaderResources *)pLayerShaderItem.m_pShaderResources)->m_ResFlags |= MTL_FLAG_2SIDED;
      else
        ((SRenderShaderResources *)pLayerShaderItem.m_pShaderResources)->m_ResFlags &= ~MTL_FLAG_2SIDED;

      ((SRenderShaderResources *)pLayerShaderItem.m_pShaderResources)->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][3] = pPrevShaderResources->m_Constants[eHWSC_Pixel][PS_DIFFUSE_COL][3];
    }

    // Replace shader and resources
    m_RP.m_pShader = pShader;       
    m_RP.m_pReplacementShader = m_RP.m_pShader;
    m_RP.m_pShaderResources = (SRenderShaderResources *)pLayerShaderItem.m_pShaderResources;
  }
  else
  {
    if( pPrevShaderResources )
    {      
      // Restore custom resources
      m_RP.m_pReplacementShader = 0;

      for(int t = 0; t < EFTT_MAX; ++t)
      {
        ((SRenderShaderResources *)pLayerShaderItem.m_pShaderResources)->m_Textures[t] = pPrevLayerResourceTexs[t];
        pPrevLayerResourceTexs[t] = 0;
      }

      m_RP.m_pShaderResources = pPrevShaderResources;   
      pPrevShaderResources = 0;         

    }
  } 
}

void CD3D9Renderer::FX_DrawMultiLayers()
{
  // Verify if current mesh has valid data for layers
  CREMesh *pRE = (CREMesh *)m_RP.m_pRE;  
  if ( !m_RP.m_pCurObject || !m_RP.m_pShader || !m_RP.m_pShaderResources || !m_RP.m_pCurObject->m_nMaterialLayers )
    return;

  IMaterial *pObjMat = m_RP.m_pCurObject->m_pCurrMaterial;   
  if( (SRendItem::m_RecurseLevel > 1 && !(m_RP.m_PersFlags & RBPF_MAKESPRITE)) || !m_RP.m_pShaderResources || !pObjMat)
    return;

  CRenderChunk *pChunk = pRE->m_pChunk;
  if( !pChunk )
  {
    assert(pChunk);
    return;
  }
  
  // Check if chunk material has layers at all
  IMaterial *pDefaultMtl = iSystem->GetI3DEngine()->GetMaterialManager()->GetDefaultLayersMaterial();
  IMaterial *pCurrMtl = pObjMat->GetSubMtlCount()? pObjMat->GetSubMtl( pChunk->m_nMatID ) : pObjMat;  
  if( !pCurrMtl || !pDefaultMtl || (pCurrMtl->GetFlags() & MTL_FLAG_NODRAW ) )
    return;

  uint32 nLayerCount = pDefaultMtl->GetLayerCount();
  if( !nLayerCount )
    return;

  // Start multi-layers processing
  PROFILE_FRAME(DrawShader_MultiLayers);

  if (m_LogFile)
    Logv(SRendItem::m_RecurseLevel, "*** Start Multilayers processing ***\n");

  // Render all layers
  for(uint32 nCurrLayer(0); nCurrLayer < nLayerCount ; ++nCurrLayer) 
  { 
    IMaterialLayer *pLayer = const_cast< IMaterialLayer* >(pCurrMtl->GetLayer(nCurrLayer));
    IMaterialLayer *pDefaultLayer =  const_cast< IMaterialLayer* >(pDefaultMtl->GetLayer(nCurrLayer));
    bool bDefaultLayer = false;
    if(!pLayer)
    {
      // Replace with default layer
      pLayer =  pDefaultLayer;
      bDefaultLayer = true;

      if( !pLayer )
        continue;
    }

    if( !pLayer->IsEnabled() )
      continue;

    // Set/verify layer shader technique 
    SShaderItem &pCurrShaderItem = pLayer->GetShaderItem();      
    CShader *pSH = static_cast<CShader*>(pCurrShaderItem.m_pShader);      
    if( !pSH || pSH->m_HWTechniques.empty())
      continue;

    SShaderTechnique *pTech = pSH->m_HWTechniques[0];
    if(!pTech) 
    {
      continue;
    }

    // Re-create render object list, based on layer properties
    {
      sTempObjects[0].SetUse(0);

      float fDistToCam = 500.0f;
      float fDist = CV_r_detaildistance;
      CRenderObject *pObj = m_RP.m_pCurObject;
      uint nObj = 0;

      while (true)
      {
        uint8 nMaterialLayers = 0;
        nMaterialLayers |= ((pObj->m_nMaterialLayers&MTL_LAYER_BLEND_DYNAMICFROZEN))? MTL_LAYER_FROZEN : 0;
        nMaterialLayers |= ((pObj->m_nMaterialLayers&MTL_LAYER_BLEND_CLOAK)>>8)? MTL_LAYER_CLOAK : 0;

        if ( nMaterialLayers & (1<<nCurrLayer) )   
          sTempObjects[0].AddElem(pObj); 

        nObj++;
        if (nObj >= m_RP.m_ObjectInstances.Num())
          break;

        pObj = m_RP.m_ObjectInstances[nObj];
      }

      // nothing in render list
      if( !sTempObjects[0].Num() )
        continue;
    }

    SShaderItem &pMtlShaderItem = pCurrMtl->GetShaderItem();    

    SEfResTexture **pResourceTexs = ((SRenderShaderResources *)pCurrShaderItem.m_pShaderResources)->m_Textures;
    SEfResTexture *pPrevLayerResourceTexs[EFTT_MAX];

    if( bDefaultLayer )
    {
      // Keep layer resources and replace with resources from base shader
      for(int t = 0; t < EFTT_MAX; ++t) 
      {
        pPrevLayerResourceTexs[t] = ((SRenderShaderResources *)pCurrShaderItem.m_pShaderResources)->m_Textures[t];
        ((SRenderShaderResources *)pCurrShaderItem.m_pShaderResources)->m_Textures[t] = m_RP.m_pShaderResources->m_Textures[t];
      }
    }

    // Store current rendering data
    TArray<CRenderObject *> pPrevRenderObjLst;
    pPrevRenderObjLst.Assign( m_RP.m_ObjectInstances );
    CRenderObject *pPrevObject = m_RP.m_pCurObject;
    SRenderShaderResources *pPrevShaderResources = m_RP.m_pShaderResources;      
    CShader *pPrevSH = m_RP.m_pShader;
    uint nPrevNumRendPasses = m_RP.m_nNumRendPasses;
    uint64 nFlagsShaderRTprev = m_RP.m_FlagsShader_RT;

    SShaderTechnique *pPrevRootTech = m_RP.m_pRootTechnique;
    m_RP.m_pRootTechnique = pTech;

    bool pNotFirstPassprev = m_RP.m_bNotFirstPass;
    int nMaterialStatePrev = m_RP.m_MaterialState;
    uint nFlagsShaderLTprev = m_RP.m_FlagsShader_LT;
    int nCurLightPass = m_RP.m_nCurLightPass;

    int nPersFlagsPrev = m_RP.m_PersFlags;
    int nPersFlags2Prev = m_RP.m_PersFlags2;
    int nMaterialAlphaRefPrev = m_RP.m_MaterialAlphaRef;
    short nLightGroupPrev = m_RP.m_nCurLightGroup;
    bool bIgnoreObjectAlpha = m_RP.m_bIgnoreObjectAlpha;
    m_RP.m_bIgnoreObjectAlpha = true;

    // Reset light passes (need ambient)
    m_RP.m_nNumRendPasses = 0;
    m_RP.m_nCurLightGroup = 0;
    m_RP.m_PersFlags2 |= RBPF2_MATERIALLAYERPASS;    

    m_RP.m_pShader = pSH;
    m_RP.m_ObjectInstances = sTempObjects[0];
    m_RP.m_pCurObject = m_RP.m_ObjectInstances[0];    
    m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
    m_RP.m_pPrevObject = NULL;
    m_RP.m_pShaderResources = (SRenderShaderResources *)pCurrShaderItem.m_pShaderResources; 

    if( (1<<nCurrLayer) & MTL_LAYER_FROZEN )
    {
       m_RP.m_MaterialState = (m_RP.m_MaterialState & ~(GS_BLEND_MASK|GS_ALPHATEST_MASK))  | (GS_BLSRC_ONE | GS_BLDST_ONE);
       m_RP.m_MaterialAlphaRef = 0xff;
    }

    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];

    FX_DrawTechnique(pSH, pTech, false, true);       

    // Restore previous rendering data
    m_RP.m_ObjectInstances.Assign( pPrevRenderObjLst );
    pPrevRenderObjLst.ClearArr();
    m_RP.m_pShader = pPrevSH;
    m_RP.m_pShaderResources = pPrevShaderResources;
    m_RP.m_pCurObject = pPrevObject;
    m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
    m_RP.m_pPrevObject = NULL;
    m_RP.m_PersFlags2 = nPersFlags2Prev;    

    m_RP.m_nNumRendPasses = nPrevNumRendPasses;

    m_RP.m_bNotFirstPass = pNotFirstPassprev;
    m_RP.m_FlagsShader_LT = nFlagsShaderLTprev;
    m_RP.m_nCurLightPass = nCurLightPass;
    m_RP.m_PersFlags = nPersFlagsPrev;
    m_RP.m_FlagsShader_RT = nFlagsShaderRTprev;

    m_RP.m_nNumRendPasses = 0;
    m_RP.m_nCurLightGroup = nLightGroupPrev;

    m_RP.m_pRootTechnique = pPrevRootTech;
    m_RP.m_bIgnoreObjectAlpha = bIgnoreObjectAlpha;
    m_RP.m_MaterialState = nMaterialStatePrev;
    m_RP.m_MaterialAlphaRef = nMaterialAlphaRefPrev;

    if( bDefaultLayer )
    {
      for(int t = 0; t < EFTT_MAX; ++t)
      {
        ((SRenderShaderResources *)pCurrShaderItem.m_pShaderResources)->m_Textures[t] = pPrevLayerResourceTexs[t];
        pPrevLayerResourceTexs[t] = 0;
      }
    }

    m_RP.m_FrameObject++;    

    //break; // only 1 layer allowed
  } 

  if (m_LogFile)
    Logv(SRendItem::m_RecurseLevel, "*** End Multilayers processing ***\n");
}

void CD3D9Renderer::FX_SelectTechnique(CShader *pShader, SShaderTechnique *pTech)
{
  SShaderTechniqueStat Stat;
  Stat.pTech = pTech;
  Stat.pShader = pShader;
  if (pTech->m_Passes.Num())
  {
    SShaderPass *pPass = &pTech->m_Passes[0];
    if (pPass->m_PShader && pPass->m_VShader)
    {
      Stat.pVS = (CHWShader_D3D *)pPass->m_VShader;
      Stat.pPS = (CHWShader_D3D *)pPass->m_PShader;
      Stat.pVSInst = Stat.pVS->m_pCurInst;
      Stat.pPSInst = Stat.pPS->m_pCurInst;
      g_SelectedTechs.push_back(Stat);
    }
  }
}

void CD3D9Renderer::FX_DrawTechnique(CShader *ef, SShaderTechnique *pTech, bool bGeneral, bool bUseMaterialState)
{
  switch(ef->m_eSHDType)
  {
  case eSHDT_General:
    FX_DrawShader_General(ef, pTech, true, bUseMaterialState);
    break;
  case eSHDT_Light:
    if (bGeneral)
      FX_DrawShader_General(ef, pTech, false, bUseMaterialState);
    else
      FX_DrawMultiLightPasses(ef, pTech, 0);
    break;
  case eSHDT_Terrain:
    if (bGeneral)  
      FX_DrawShader_General(ef, pTech, false, bUseMaterialState);
    else
      FX_DrawShader_Terrain(ef, pTech);
    break;
  case eSHDT_Fur:
    break;
  case eSHDT_CustomDraw:
  case eSHDT_Sky:
    if (m_RP.m_pRE) 
    {
      EF_Scissor(false, 0, 0, 0, 0);
      if (pTech && pTech->m_Passes.Num())
        m_RP.m_pRE->mfDraw(ef, &pTech->m_Passes[0]);
      else
        m_RP.m_pRE->mfDraw(ef, NULL);
    }
    break;
  default:
    assert(0);
  }
  if (m_RP.m_ObjFlags & FOB_SELECTED)
    FX_SelectTechnique(ef, pTech);
}

static int sNumBits[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
void sDetectInstancing(CShader *pShader, CRenderObject *pObj)
{
	assert(CRenderer::m_iGeomInstancingThreshold>=1);			// call ChangeGeomInstancingThreshold();

  // Hardware instancing works only if:
  // 1. no projected light
  // 2. number of instances exceeds m_iGeomInstancingThreshold
  // 3. shader supports instancing
  CRenderer *rd = gRenDev;
	if (CRenderer::CV_r_geominstancing && (rd->m_RP.m_ObjectInstances.Num()>CRenderer::m_iGeomInstancingThreshold || (rd->m_RP.m_FlagsPerFlush & RBSI_INSTANCED)) && (pShader->m_Flags & EF_SUPPORTSINSTANCING) && !CRenderer::CV_r_measureoverdraw)
    rd->m_RP.m_FlagsPerFlush |= RBSI_INSTANCED;
  else
    rd->m_RP.m_FlagsPerFlush &= ~RBSI_INSTANCED;
}


void CD3D9Renderer::EF_FlushShadow()
{
  CD3D9Renderer *rd = gcpRendD3D;
  if (!rd->m_RP.m_pRE && !rd->m_RP.m_RendNumVerts)
    return;

  CShader *pShader = rd->m_RP.m_pShader;

  if (!pShader || !pShader->m_HWTechniques.Num())
    return;
  //  if (rd->m_RP.m_nPassGroupID == EFSLIST_TRANSP_ID)
  {
    if (!rd->EF_SetResourcesState())
      return;
  }
  SShaderTechnique *pTech = pShader->mfGetStartTechnique(rd->m_RP.m_nShaderTechnique);
  if (!pTech || pTech->m_nTechnique[TTYPE_SHADOWPASS] < 0)
    return;

  rd->m_RP.m_pRootTechnique = pTech;
  assert(pTech->m_nTechnique[TTYPE_SHADOWPASS] < (int)pShader->m_HWTechniques.Num());
  pTech = pShader->m_HWTechniques[pTech->m_nTechnique[TTYPE_SHADOWPASS]];
  SShaderTechnique *pBaseTech = pTech;
  if (rd->m_RP.m_ObjFlags & FOB_SELECTED)
    FX_SelectTechnique(pShader, pTech);

  rd->EF_ApplyShadowQuality();

  if (rd->m_RP.m_PersFlags2 & (RBPF2_VSM | RBPF2_DRAWTOCUBE | RBPF2_DISABLECOLORWRITES))
  {
    if (rd->m_RP.m_PersFlags2 & RBPF2_DISABLECOLORWRITES)
      rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_HW_PCF_COMPARE ];
    if (rd->m_RP.m_PersFlags2 & RBPF2_VSM)
      rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_VARIANCE_SM];
    if (rd->m_RP.m_PersFlags2 & RBPF2_DRAWTOCUBE)
      rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
  }

  if (rd->m_RP.m_ObjFlags & FOB_VEGETATION)
  {
    rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_VEGETATION];
    if (!(rd->m_RP.m_PersFlags & RBPF_MAKESPRITE) && (rd->m_RP.m_ObjFlags & FOB_BENDED))
      rd->m_RP.m_FlagsShader_MDV |= MDV_BENDING;
  }
  if (pShader->m_eShaderType == eST_Particle)
    rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_PARTICLE];

  if (rd->m_RP.m_pShaderResources)
  {
    if (rd->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_2SIDED)
    {
      rd->D3DSetCull(eCULL_None);
      rd->m_RP.m_FlagsPerFlush |= RBSI_NOCULL;      
    }
    else
    if (rd->m_RP.m_pShader->m_eCull != -1)
    {
      rd->D3DSetCull(rd->m_RP.m_pShader->m_eCull);
      rd->m_RP.m_FlagsPerFlush |= RBSI_NOCULL;
    }
  }

  sDetectInstancing(pShader, rd->m_RP.m_pCurObject);

  PROFILE_SHADER_START

#ifdef DO_RENDERLOG
  if (rd->m_LogFile && CV_r_log >= 3)
    rd->Logv(SRendItem::m_RecurseLevel, "\n");
#endif

  rd->FX_DrawShadowPasses(pShader, pTech, rd->m_RP.m_nCurLightChan);

  PROFILE_SHADER_END
#ifdef DO_RENDERLOG
  if (rd->m_LogFile)
  {
    if (CV_r_log == 4)
    {
      uint nAffectMask = rd->m_RP.m_nCurLightGroup < 0 ? -1 : (0xf << (rd->m_RP.m_nCurLightGroup*4));
      uint nMask = (nAffectMask);
      for (uint n=0; n<rd->m_RP.m_DLights[SRendItem::m_RecurseLevel].Num(); n++)
      {
        CDLight *dl = rd->m_RP.m_DLights[SRendItem::m_RecurseLevel][n];
        if (nMask & (1<<n))
          rd->Logv(SRendItem::m_RecurseLevel, "   Light %d (\"%s\")\n", n, dl->m_sName ? dl->m_sName : "<Unknown>");
      }
    }

    char *str = "FlushHW";
    if (pBaseTech)
      rd->Logv(SRendItem::m_RecurseLevel, "%s: '%s.%s', Id:%d, ResId:%d, Obj:%d, Cp: %d, VF:%d\n", str, pShader->GetName(), pBaseTech->m_Name.c_str(), pShader->GetID(), rd->m_RP.m_pShaderResources ? rd->m_RP.m_pShaderResources->m_Id : -1, rd->m_RP.m_pCurObject->m_VisId, rd->m_RP.m_ClipPlaneEnabled, pShader->m_VertexFormatId);
    else
      rd->Logv(SRendItem::m_RecurseLevel, "%s: '%s', Id:%d, ResId:%d, Obj:%d, Cp: %d, VF:%d\n", str, pShader->GetName(), pShader->GetID(), rd->m_RP.m_pShaderResources ? rd->m_RP.m_pShaderResources->m_Id : -1, rd->m_RP.m_pCurObject->m_VisId, rd->m_RP.m_ClipPlaneEnabled, pShader->m_VertexFormatId);
    if (rd->m_RP.m_ObjFlags & FOB_SELECTED)
    {
      if (rd->m_RP.m_MaterialState & GS_ALPHATEST_MASK)
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x), LM: %d, (AT) (Selected)\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_DynLMask);
      else
      if (rd->m_RP.m_MaterialState & GS_BLEND_MASK)
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x) (AB), LM: %d (Dist: %.3f) (Selected)\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_DynLMask, rd->m_RP.m_pCurObject->m_fDistance);
      else
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x), RE: 0x%x, LM: 0x%x (Selected)\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_pRE, rd->m_RP.m_DynLMask);
    }
    else
    {
      if (rd->m_RP.m_MaterialState & GS_ALPHATEST_MASK)
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x) (AB), Inst: %d, LM: %d (Dist: %.3f)\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_ObjectInstances.Num(), rd->m_RP.m_DynLMask, rd->m_RP.m_pCurObject->m_fDistance);
      else
      if (rd->m_RP.m_MaterialState & GS_BLEND_MASK)
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x) (AB), Inst: %d, LM: %d (Dist: %.3f)\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_ObjectInstances.Num(), rd->m_RP.m_DynLMask, rd->m_RP.m_pCurObject->m_fDistance);
      else
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x), Inst: %d, RE: 0x%x, LM: 0x%x\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_ObjectInstances.Num(), rd->m_RP.m_pRE, rd->m_RP.m_DynLMask);
    }
  }
#endif
}

// Set/Restore shader resources overrided states
bool CD3D9Renderer::EF_SetResourcesState()
{
  if (!m_RP.m_pShader)
    return false;
  m_RP.m_MaterialState = 0;
  if (m_RP.m_pShader->m_Flags2 & EF2_IGNORERESOURCESTATES)
    return true;
  if (!m_RP.m_pShaderResources)
    return true;
  if (m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_NOTINSTANCED)
    m_RP.m_FlagsPerFlush &= ~RBSI_INSTANCED;

  bool bRes = true;
  if (m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_2SIDED)
  {
    D3DSetCull(eCULL_None);
    m_RP.m_FlagsPerFlush |= RBSI_NOCULL;    
  }
  m_RP.m_ShaderTexResources[EFTT_DECAL_OVERLAY] = NULL;

  if( !(m_RP.m_nBatchFilter & FB_Z) )
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_NOZPASS ];

  if (m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_SCATTER) // enable scatter pass
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_SCATTERSHADE ];

  if ((m_RP.m_pShaderResources->m_AlphaRef ) && !(m_RP.m_ObjFlags & FOB_FOGPASS))
  {
    if (!(m_RP.m_PersFlags2 & RBPF2_NOALPHATEST) || (m_RP.m_PersFlags2 & RBPF2_ATOC))
    {
      int nAlphaRef = (int)(m_RP.m_pShaderResources->m_AlphaRef*255.0f);
      m_RP.m_MaterialAlphaRef = nAlphaRef;
      m_RP.m_MaterialState = GS_ALPHATEST_GEQUAL | GS_DEPTHWRITE;
    }
    else
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];
  }
  float fOpacity;
  if ((fOpacity=m_RP.m_pShaderResources->Opacity()) != 1.0f)
  {
    if (!(m_RP.m_PersFlags2 & RBPF2_NOALPHABLEND))
    {
      if (m_RP.m_MaterialState != 0)
        m_RP.m_MaterialState &= ~GS_DEPTHWRITE;
      if (m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_ADDITIVE)
      {
        m_RP.m_MaterialState |= GS_BLSRC_ONE | GS_BLDST_ONE;
        m_RP.m_CurGlobalColor[0] = fOpacity;
        m_RP.m_CurGlobalColor[1] = fOpacity;
        m_RP.m_CurGlobalColor[2] = fOpacity;
      }
      else
      {
        m_RP.m_MaterialState |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
        m_RP.m_CurGlobalColor[3] = fOpacity;
      }
      m_RP.m_fCurOpacity = fOpacity;
      if (m_RP.m_nMaxPasses)
        m_RP.m_fCurOpacity = fOpacity / (float)(m_RP.m_nMaxPasses);
    }
  }
  if (!(m_RP.m_PersFlags & RBPF_MAKESPRITE))
  {
    if (m_RP.m_pShaderResources->m_pDeformInfo)
      m_RP.m_FlagsShader_MDV |= m_RP.m_pShaderResources->m_pDeformInfo->m_eType;
    m_RP.m_FlagsShader_MDV |= m_RP.m_pCurObject->m_nMDV | m_RP.m_pShader->m_nMDV;
    if (m_RP.m_ObjFlags & FOB_OWNER_GEOMETRY)
      m_RP.m_FlagsShader_MDV &= ~MDV_DEPTH_OFFSET;
  }

  return true;
}


static char sNM[1024];

// Flush current render item
void CD3D9Renderer::EF_Flush()
{
  CD3D9Renderer *rd = gcpRendD3D;
  if (!rd->m_RP.m_pRE && !rd->m_RP.m_RendNumVerts)
    return;

  // Shader overriding with layer
  if ( (rd->m_RP.m_nBatchFilter &  FB_GENERAL ) && CV_r_usemateriallayers)
    rd->FX_SetupMultiLayers(true);

  CShader *ef = rd->m_RP.m_pShader;
  if (!ef)
    return;

  if (!rd->m_RP.m_sExcludeShader.empty())
  {
    strcpy(sNM, ef->GetName());
    strlwr(sNM);
    if (strstr(rd->m_RP.m_sExcludeShader.c_str(), sNM))
      return;
  }

  if (!rd->m_RP.m_sShowOnlyShader.empty())
  {
    strcpy(sNM, ef->GetName());
    strlwr(sNM);
    if (!strstr(rd->m_RP.m_sShowOnlyShader.c_str(), sNM))
      return;
  }

#ifdef DO_RENDERLOG
  if (rd->m_LogFile && CV_r_log == 3)
    rd->Logv(SRendItem::m_RecurseLevel, "\n\n.. Start Flush: '%s' ..\n", ef->GetName());
#endif

  rd->m_RP.m_PS.m_NumRendBatches++;
  if (rd->m_RP.m_ObjectInstances.Num())
    rd->m_RP.m_PS.m_NumRendInstances += rd->m_RP.m_ObjectInstances.Num();
  else
    rd->m_RP.m_PS.m_NumRendInstances++;

  CRenderObject *pObj = rd->m_RP.m_pCurObject;
  if (CV_r_postprocess_effects && rd->m_RP.m_pShaderResources && rd->m_RP.m_pShaderResources->m_PostEffects && !(pObj->m_ObjFlags & FOB_RENDER_INTO_SHADOWMAP))
  {       
    using namespace NPostEffects;

    // Check effect type, enable corresponding post processing
    uint8 nCurrPostEffects = PostEffectMgr().GetPostBlendEffectsFlags();
    PostEffectMgr().SetPostBlendEffectsFlags( nCurrPostEffects | rd->m_RP.m_pShaderResources->m_PostEffects );
  }

  PROFILE_SHADER_START

#ifdef DO_RENDERLOG
  if (rd->m_LogFile && CV_r_log >= 3)
    rd->Logv(SRendItem::m_RecurseLevel, "\n");
#endif

  sDetectInstancing(ef, pObj);

  // Techniques draw cycle...
  SShaderTechnique *pTech = ef->mfGetStartTechnique(rd->m_RP.m_nShaderTechnique);

  if (pTech)
  {
    if (rd->m_RP.m_pShaderResources && !(rd->m_RP.m_nBatchFilter & (FB_Z | FB_GLOW | FB_CUSTOM_RENDER | FB_MOTIONBLUR | FB_SCATTER|FB_RAIN)) && !(rd->m_RP.m_PersFlags & RBPF_SHADOWGEN))
    {
      uint i;
      // Update render targets if necessary
      if (!(rd->m_RP.m_PersFlags & RBPF_DRAWTOTEXTURE))
      {
        for (i=0; i<rd->m_RP.m_pShaderResources->m_RTargets.Num(); i++)
        {
          SHRenderTarget *pTarg = rd->m_RP.m_pShaderResources->m_RTargets[i];
          if (pTarg->m_eOrder == eRO_PreDraw)
            rd->FX_DrawToRenderTarget(ef, rd->m_RP.m_pShaderResources, pObj, pTech, pTarg, 0, rd->m_RP.m_pRE);
        }
        for (i=0; i<pTech->m_RTargets.Num(); i++)
        {
          SHRenderTarget *pTarg = pTech->m_RTargets[i];
          if (pTarg->m_eOrder == eRO_PreDraw)
            rd->FX_DrawToRenderTarget(ef, rd->m_RP.m_pShaderResources, pObj, pTech, pTarg, 0, rd->m_RP.m_pRE);
        }
      }
    }
    rd->m_RP.m_pRootTechnique = pTech;

    // Skip z-pass if appropriate technique is not exist
    bool bGeneral = false;
    if (rd->m_RP.m_PersFlags & RBPF_SHADOWGEN)
    {
      bGeneral = true;

      //increase pixel density(from both sides of terrain) for large variance shadow maps
      if (ef->m_eSHDType == eSHDT_Terrain)
      {
				if (rd->m_RP.m_pCurObject->m_ObjFlags & FOB_TRANS_TRANSLATE &&  // Check if voxel object (terrain has no transformation) 
				    rd->m_RP.m_pCurShadowFrustum->m_Flags & DLF_DIRECTIONAL)
				{	
					rd->D3DSetCull(eCULL_Back); 
					rd->m_RP.m_FlagsPerFlush |= RBSI_NOCULL;
				}
				else if (rd->m_RP.m_PersFlags2 & RBPF2_VSM)
        {
          rd->D3DSetCull(eCULL_None); 
          rd->m_RP.m_FlagsPerFlush |= RBSI_NOCULL;
        }
        else if (rd->m_RP.m_pCurShadowFrustum->m_Flags & DLF_DIRECTIONAL)
        {
          rd->D3DSetCull(eCULL_Front); 
          rd->m_RP.m_FlagsPerFlush |= RBSI_NOCULL;
        }
        else 
        {
          //Flipped matrix for point light sources
          //front faces culling by default for terrain
          rd->D3DSetCull(eCULL_Back); 
          rd->m_RP.m_FlagsPerFlush |= RBSI_NOCULL;
          //reset slope bias here as well
        }
      }

      if (rd->m_RP.m_PersFlags2 & (RBPF2_VSM | RBPF2_DRAWTOCUBE | RBPF2_DISABLECOLORWRITES))
      {
        if (rd->m_RP.m_PersFlags2 & RBPF2_DISABLECOLORWRITES)
          rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[ HWSR_HW_PCF_COMPARE ];
        if (rd->m_RP.m_PersFlags2 & RBPF2_VSM)
          rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_VARIANCE_SM];
        if (rd->m_RP.m_PersFlags2 & RBPF2_DRAWTOCUBE)
          rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
      }

      //per-object bias for Shadow Generation
      rd->m_cEF.m_TempVecs[1][0] = 0.0f;
      if(!(rd->m_RP.m_PersFlags2 & RBPF2_VSM))
      {
        if (rd->m_RP.m_pShaderResources)
          if (rd->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_2SIDED || rd->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_FORCESHADOWBIAS)
          {
            //handle terrain self-shadowing and two-sided geom
            rd->m_cEF.m_TempVecs[1][0] = rd->m_RP.m_vFrustumInfo.w;
          }


        //don't make per-object bias for global VSM
        //if (rd->m_RP.m_pShader->m_eSHDType == eSHDT_Terrain /*&& m_RP.m_vFrustumInfo.x > 100.0f*/) //check for sun && terrain shadows
        //{
        //  rd->m_cEF.m_TempVecs[1][0] = -(rd->m_RP.m_vFrustumInfo.w);
        //}
      }
    }
    else
    if (rd->m_RP.m_nBatchFilter & (FB_Z | FB_GLOW | FB_MOTIONBLUR | FB_CUSTOM_RENDER|FB_RAIN))
    {
      bGeneral = true;
      if (rd->m_RP.m_nBatchFilter & FB_Z)
      {
        if (pTech && pTech->m_nTechnique[TTYPE_Z] > 0)
        {
          assert(pTech->m_nTechnique[TTYPE_Z] < (int)ef->m_HWTechniques.Num());
          pTech = ef->m_HWTechniques[pTech->m_nTechnique[TTYPE_Z]];
        }
      }
      else
      if (rd->m_RP.m_nBatchFilter & FB_GLOW)
      {
        rd->m_RP.m_PersFlags2 |= RBPF2_GLOWPASS; 
        if (pTech && pTech->m_nTechnique[TTYPE_GLOWPASS] > 0)
        {
          assert(pTech->m_nTechnique[TTYPE_GLOWPASS] < (int)ef->m_HWTechniques.Num());
          pTech = ef->m_HWTechniques[pTech->m_nTechnique[TTYPE_GLOWPASS]];
        }
      }
      else
      if (rd->m_RP.m_nBatchFilter & FB_MOTIONBLUR)
      {
        if (pTech && pTech->m_nTechnique[TTYPE_MOTIONBLURPASS] > 0)
        {
          assert(pTech->m_nTechnique[TTYPE_MOTIONBLURPASS] < (int)ef->m_HWTechniques.Num());
          pTech = ef->m_HWTechniques[pTech->m_nTechnique[TTYPE_MOTIONBLURPASS]];
        }
      }
      else
      if (rd->m_RP.m_nBatchFilter & FB_CUSTOM_RENDER)
      {
        if (pTech && pTech->m_nTechnique[TTYPE_CUSTOMRENDERPASS] > 0)
        {
          assert(pTech->m_nTechnique[TTYPE_CUSTOMRENDERPASS] < (int)ef->m_HWTechniques.Num());
          pTech = ef->m_HWTechniques[pTech->m_nTechnique[TTYPE_CUSTOMRENDERPASS]];
        }
      }
      else
      if (rd->m_RP.m_nBatchFilter & FB_RAIN)
      {
        if (pTech && pTech->m_nTechnique[TTYPE_RAINPASS] > 0)
        {
          assert(pTech->m_nTechnique[TTYPE_RAINPASS] < (int)ef->m_HWTechniques.Num());
          pTech = ef->m_HWTechniques[pTech->m_nTechnique[TTYPE_RAINPASS]];
        }
      }
    }
    if (!(rd->m_RP.m_nBatchFilter & FB_Z) && CV_r_debugrendermode)
    {
      if (CV_r_debugrendermode & 1)
        rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG0];
      if (CV_r_debugrendermode & 2)
        rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG1];
      if (CV_r_debugrendermode & 4)
        rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG2];
      if (CV_r_debugrendermode & 8)
        rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG3];
    }

    if (!rd->EF_SetResourcesState())
    {
      if (rd->m_RP.m_pReplacementShader)
        rd->FX_SetupMultiLayers(false); 
      return;
    }

    if (rd->m_RP.m_nBatchFilter & FB_SCATTER)
    {
      if (pTech && pTech->m_nTechnique[TTYPE_SCATTERPASS] > 0)
      {
        assert(pTech->m_nTechnique[TTYPE_SCATTERPASS] < (int)ef->m_HWTechniques.Num());
        pTech = ef->m_HWTechniques[pTech->m_nTechnique[TTYPE_SCATTERPASS]];

        bGeneral = true;

        //overwrite cull mode
        if (rd->m_RP.m_PersFlags2 & RBPF2_SCATTERPASS)
          rd->D3DSetCull(eCULL_Back);
        else
          rd->D3DSetCull(eCULL_Front);

        rd->m_RP.m_FlagsPerFlush |= RBSI_NOCULL;
      }
      else
      {
        //TOFIX: uncomment for having proper scattering accumulation
        //////////////////////////////////////////////////////////////////////////
        //test for occluders(skeleton) rendering
        //////////////////////////////////////////////////////////////////////////
        /*if (pTech && pTech->m_nTechnique[TTYPE_Z] > 0)
        {
          assert(pTech->m_nTechnique[TTYPE_Z] < (int)ef->m_HWTechniques.Num());
          pTech = ef->m_HWTechniques[pTech->m_nTechnique[TTYPE_Z]];

          bGeneral = true;
        }*/
        //////////////////////////////////////////////////////////////////////////

        if (!(rd->m_RP.m_PersFlags2 & RBPF2_SCATTERPASS))
          return;
      }
    }

    if (rd->m_RP.m_ObjFlags & (FOB_BENDED | FOB_SOFT_PARTICLE | FOB_OCEAN_PARTICLE | FOB_DECAL_TEXGEN_2D | FOB_DECAL_TEXGEN_3D | FOB_VEGETATION | FOB_NEAREST))
    {
      if (!(rd->m_RP.m_PersFlags & RBPF_MAKESPRITE) && (rd->m_RP.m_ObjFlags & FOB_BENDED))
        rd->m_RP.m_FlagsShader_MDV |= MDV_BENDING;
      if (rd->m_RP.m_ObjFlags & FOB_VEGETATION)
        rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_VEGETATION];                

      if( CV_r_usesoftparticles )
      {
        if((rd->m_RP.m_ObjFlags & FOB_SOFT_PARTICLE) && SRendItem::m_RecurseLevel==1)
          rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SOFT_PARTICLE];
        if((rd->m_RP.m_ObjFlags & FOB_OCEAN_PARTICLE)  && SRendItem::m_RecurseLevel==1)
          rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_OCEAN_PARTICLE];
      }

      assert( ( FOB_DECAL_TEXGEN_2D | FOB_DECAL_TEXGEN_3D ) != ( rd->m_RP.m_ObjFlags & ( FOB_DECAL_TEXGEN_2D | FOB_DECAL_TEXGEN_3D ) ) );
      if(rd->m_RP.m_ObjFlags & FOB_DECAL_TEXGEN_2D)
        rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D];
      if(rd->m_RP.m_ObjFlags & FOB_DECAL_TEXGEN_3D)
        rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_3D];
      if(rd->m_RP.m_ObjFlags & FOB_NEAREST)
        rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_NEAREST];
    }    
    if (!rd->m_RP.m_ObjectInstances.Num() && !(rd->m_RP.m_ObjFlags & FOB_TRANS_MASK))
      rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_OBJ_IDENTITY];
    if (!(rd->m_RP.m_PersFlags2 & RBPF2_NOSHADERFOG) && rd->m_FS.m_bEnable && !(rd->m_RP.m_ObjFlags & FOB_NO_FOG))
    {
      rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_FOG];

      if (rd->GetSkyLightRenderParams())
        rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SKYLIGHT_BASED_FOG];
    }

    rd->m_RP.m_pCurTechnique = pTech;

		if (rd->m_RP.m_pShader->m_Flags2 & EF2_SUBSURFSCATTER && (eSQ_Low == rd->m_RP.m_nShaderQuality || rd->m_RP.m_pShader->m_nMaskGenFX & GENFLAGS_FORCE_VERTEX_SCATTER || rd->m_RP.m_pShader->m_Flags2 & EF2_BLENDSUBSURFSCATTER))
			rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_VERTEX_SCATTER];

    if (rd->m_RP.m_nBatchFilter & (FB_DETAIL | FB_CAUSTICS) || (rd->m_RP.m_nBatchFilter & FB_MULTILAYERS) && !rd->m_RP.m_pReplacementShader)
    {
      if (rd->m_RP.m_nBatchFilter & FB_DETAIL)
        rd->FX_DrawDetailOverlayPasses();

      if (rd->m_RP.m_nBatchFilter & FB_MULTILAYERS && CV_r_usemateriallayers)
        rd->FX_DrawMultiLayers();
      
      if (rd->m_RP.m_nBatchFilter & FB_CAUSTICS)
        rd->FX_DrawCausticsPasses();
    }
    else
      rd->FX_DrawTechnique(ef, pTech, bGeneral, true);


//////////////////////////////////////////////////////////////////////////
    //depth pass for scattered objects
    if (rd->m_RP.m_nBatchFilter & FB_SCATTER  && !(rd->m_RP.m_PersFlags2 & RBPF2_SCATTERPASS))
    {
      pTech = ef->mfGetStartTechnique(rd->m_RP.m_nShaderTechnique);
      if (pTech && pTech->m_nTechnique[TTYPE_SCATTERPASS] > 0)
      {
        //assert(pTech->m_nTechnique[TTYPE_SCATTERPASS] < (int)ef->m_HWTechniques.Num());
        pTech = ef->m_HWTechniques[pTech->m_nTechnique[TTYPE_SCATTERPASS]];

        bGeneral = true;

        //FB_SCATTER should not be processed during z-pass
        //assert(rd->m_RP.m_PersFlags & RBPF_ZPASS);
        rd->m_RP.m_PersFlags |= RBPF_ZPASS;

        rd->D3DSetCull(eCULL_Front);

        rd->m_RP.m_FlagsPerFlush |= RBSI_NOCULL;

        rd->FX_DrawTechnique(ef, pTech, bGeneral, true);
        rd->m_RP.m_PersFlags &= ~RBPF_ZPASS;
      }
    }

//////////////////////////////////////////////////////////////////////////

  }
  else
  if (ef->m_eSHDType == eSHDT_CustomDraw)
    rd->FX_DrawTechnique(ef, 0, true, true);

  if ( rd->m_RP.m_pReplacementShader)
  {
    rd->FX_SetupMultiLayers(false); 
  }

  rd->m_RP.m_pShader = ef;
  if (ef->m_Flags & EF_LOCALCONSTANTS)
    rd->m_RP.m_PrevLMask = -1;

  PROFILE_SHADER_END

#ifdef DO_RENDERLOG
  if (rd->m_LogFile)
  {
    if (CV_r_log == 4 && rd->m_RP.m_DynLMask)
    {
      uint nAffectMask = rd->m_RP.m_nCurLightGroup < 0 ? -1 : (0xf << (rd->m_RP.m_nCurLightGroup*4));
      uint nMask = (rd->m_RP.m_DynLMask & nAffectMask);
      for (uint n=0; n<rd->m_RP.m_DLights[SRendItem::m_RecurseLevel].Num(); n++)
      {
        CDLight *dl = rd->m_RP.m_DLights[SRendItem::m_RecurseLevel][n];
        if (nMask & (1<<n))
          rd->Logv(SRendItem::m_RecurseLevel, "   Light %d (\"%s\")\n", n, dl->m_sName ? dl->m_sName : "<Unknown>");
      }
    }

    char *str;
    if (ef->m_HWTechniques.Num())
      str = "FlushHW";
    else
      str = "Flush";
    if (rd->m_RP.m_pCurTechnique)
      rd->Logv(SRendItem::m_RecurseLevel, "%s: '%s.%s', Id:%d, ResId:%d, Obj:%d, Cp: %d, VF:%d\n", str, ef->GetName(), rd->m_RP.m_pCurTechnique->m_Name.c_str(), ef->GetID(), rd->m_RP.m_pShaderResources ? rd->m_RP.m_pShaderResources->m_Id : -1, rd->m_RP.m_pCurObject->m_VisId, rd->m_RP.m_ClipPlaneEnabled, ef->m_VertexFormatId);
    else
      rd->Logv(SRendItem::m_RecurseLevel, "%s: '%s', Id:%d, ResId:%d, Obj:%d, Cp: %d, VF:%d\n", str, ef->GetName(), ef->GetID(), rd->m_RP.m_pShaderResources ? rd->m_RP.m_pShaderResources->m_Id : -1, rd->m_RP.m_pCurObject->m_VisId, rd->m_RP.m_ClipPlaneEnabled, ef->m_VertexFormatId);
    if (rd->m_RP.m_ObjFlags & FOB_SELECTED)
    {
      if (rd->m_RP.m_MaterialState & GS_ALPHATEST_MASK)
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x), LM: %d, (AT) (Selected)\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_DynLMask);
      else
      if (rd->m_RP.m_MaterialState & GS_BLEND_MASK)
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x) (AB), LM: %d (Dist: %.3f) (Selected)\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_DynLMask, rd->m_RP.m_pCurObject->m_fDistance);
      else
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x), RE: 0x%x, LM: 0x%x (Selected)\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_pRE, rd->m_RP.m_DynLMask);
    }
    else
    {
      if (rd->m_RP.m_MaterialState & GS_ALPHATEST_MASK)
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x) (AT), Inst: %d, RE: 0x%x, LM: %d (Dist: %.3f)\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_ObjectInstances.Num(), rd->m_RP.m_pRE, rd->m_RP.m_DynLMask, rd->m_RP.m_pCurObject->m_fDistance);
      else
      if (rd->m_RP.m_MaterialState & GS_BLEND_MASK)
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x) (AB), Inst: %d, RE: 0x%x, LM: %d (Dist: %.3f)\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_ObjectInstances.Num(), rd->m_RP.m_pRE, rd->m_RP.m_DynLMask, rd->m_RP.m_pCurObject->m_fDistance);
      else
        rd->Logv(SRendItem::m_RecurseLevel, "  %.3f, %.3f, %.3f (0x%x), Inst: %d, RE: 0x%x, LM: 0x%x\n", rd->m_RP.m_pCurObject->m_II.m_Matrix(0,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(1,3), rd->m_RP.m_pCurObject->m_II.m_Matrix(2,3), rd->m_RP.m_pCurObject->m_ObjFlags, rd->m_RP.m_ObjectInstances.Num(), rd->m_RP.m_pRE, rd->m_RP.m_DynLMask);
    }
  }
#endif
}

//===================================================================================================

int sLimitSizeByScreenRes(uint size)
{
  CD3D9Renderer *r = gcpRendD3D;
#if defined (DIRECT3D9) || defined (OPENGL)
  while(true)
  {
    if (size > (uint) r->m_pd3dpp->BackBufferWidth || size > (uint) r->m_pd3dpp->BackBufferHeight)
      size >>= 1;
    else
      break;
  }
#endif
  return size;
}

static int sTexLimitRes(uint nSrcsize, uint nDstSize)
{
  while(true)
  {
    if (nSrcsize > nDstSize)
      nSrcsize >>= 1;
    else
      break;
  }
  return nSrcsize;
}

ILINE Matrix34 CreateReflectionMat3 ( const Plane& p )
{
  Matrix34 m; 
  f32 vxy   = -2 * p.n.x * p.n.y;
  f32 vxz   = -2 * p.n.x * p.n.z;
  f32 vyz   = -2 * p.n.y * p.n.z;
  f32 pdotn = -2 * p.d;

  m.m00=1-2*p.n.x*p.n.x;  m.m01=vxy;              m.m02=vxz;              m.m03=pdotn*p.n.x;
  m.m10=vxy;              m.m11=1-2*p.n.y*p.n.y;  m.m12=vyz;              m.m13=pdotn*p.n.y;
  m.m20=vxz;              m.m21=vyz;              m.m22=1-2*p.n.z*p.n.z;  m.m23=pdotn*p.n.z;

  return m;
}


static float sScaleBiasMat[16] = 
{
  0.5f, 0,   0,   0,
  0,   -0.5f, 0,   0,
  0,   0,   0.5f, 0,
  0.5f, 0.5f, 0.5f, 1.0f
};

static Matrix34 sMatrixLookAt( const Vec3 &dir,const Vec3 &up,float rollAngle=0 )
{
  Matrix34 M;
  // LookAt transform.
  Vec3 xAxis,yAxis,zAxis;
  Vec3 upVector = up;

  yAxis = -dir.GetNormalized();

  //if (zAxis.x == 0.0 && zAxis.z == 0) up.Set( -zAxis.y,0,0 ); else up.Set( 0,1.0f,0 );

  xAxis = upVector.Cross(yAxis).GetNormalized();
  zAxis = xAxis.Cross(yAxis).GetNormalized();

  // OpenGL kind of matrix.
  M(0,0) = xAxis.x;
  M(0,1) = yAxis.x;
  M(0,2) = zAxis.x;
  M(0,3) = 0;

  M(1,0) = xAxis.y;
  M(1,1) = yAxis.y;
  M(1,2) = zAxis.y;
  M(1,3) = 0;

  M(2,0) = xAxis.z;
  M(2,1) = yAxis.z;
  M(2,2) = zAxis.z;
  M(2,3) = 0;

  if (rollAngle != 0)
  {
    Matrix34 RollMtx;
    RollMtx.SetIdentity();

    float cossin[2];
 //   cry_sincosf(rollAngle, cossin);
		sincos_tpl(rollAngle, &cossin[1],&cossin[0]);

    RollMtx(0,0) = cossin[0]; RollMtx(0,2) = -cossin[1];
    RollMtx(2,0) = cossin[1]; RollMtx(2,2) = cossin[0];

    // Matrix multiply.
    M = RollMtx * M;
  }

  return M;
}

// this is bad: we should re-factor post process stuff to be more general/shareable
static void DrawFullScreenQuad(int nTexWidth, int nTexHeight)
{    
  static struct_VERTEX_FORMAT_P3F_TEX2F pScreenQuad[] =
  {
    { Vec3(0, 0, 0), Vec2(0, 0) },   
    { Vec3(0, 1, 0), Vec2(0, 1) },    
    { Vec3(1, 0, 0), Vec2(1, 0) },   
    { Vec3(1, 1, 0), Vec2(1, 1) },   
  };

  // No offsets required in d3d10
  float fOffsetU = 0.0f;
  float fOffsetV = 0.0f;

#if defined (DIRECT3D9)

  fOffsetU = 0.5f/(float)nTexWidth;
  fOffsetV = 0.5f/(float)nTexHeight;  

#endif

  pScreenQuad[0].xyz = Vec3(-fOffsetU, -fOffsetV, 0);
  pScreenQuad[1].xyz = Vec3(-fOffsetU, 1-fOffsetV, 0);
  pScreenQuad[2].xyz = Vec3(1-fOffsetU, -fOffsetV, 0);
  pScreenQuad[3].xyz = Vec3(1-fOffsetU, 1-fOffsetV, 0);

  CVertexBuffer strip(pScreenQuad, VERTEX_FORMAT_P3F_TEX2F);
  gRenDev->DrawTriStrip(&strip, 4);     
}


static void TexBlurAnisotropicVertical(CTexture *pTex, int nAmount, float fScale, float fDistribution, bool bAlphaOnly)
{
  if(!pTex)
  {
    return;
  }
  
  SDynTexture *tpBlurTemp = new SDynTexture(pTex->GetWidth(), pTex->GetHeight(), pTex->GetDstFormat(), eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "TempBlurAnisoVertRT");
  tpBlurTemp->Update( pTex->GetWidth(), pTex->GetHeight() );

  if( !tpBlurTemp->m_pTexture)
  {
    SAFE_DELETE(tpBlurTemp);
    return;
  }
  
  gcpRendD3D->Set2DMode(true, 1, 1);     


  PROFILE_SHADER_START

    // Get current viewport
    int iTempX, iTempY, iWidth, iHeight;
  gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
  gRenDev->SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());        

  Vec4 vWhite( 1.0f, 1.0f, 1.0f, 1.0f );

  static CCryName pTechName("AnisotropicVertical");
  CShader *m_pCurrShader = CShaderMan::m_shPostEffects;

  uint nPasses;
  m_pCurrShader->FXSetTechnique(pTechName);
  m_pCurrShader->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  m_pCurrShader->FXBeginPass(0);

  gRenDev->EF_SetState(GS_NODEPTHTEST);   

  // setup texture offsets, for texture sampling
  float s1 = 1.0f/(float) pTex->GetWidth();     
  float t1 = 1.0f/(float) pTex->GetHeight();    

  Vec4 pWeightsPS;
  pWeightsPS.x = 0.25 * t1;
  pWeightsPS.y = 0.5 * t1;
  pWeightsPS.z = 0.75 * t1;
  pWeightsPS.w = 1.0 * t1;

  
  pWeightsPS *= -fScale;


  STexState sTexState = STexState(FILTER_LINEAR, true);
  static CCryName pParam0Name("blurParams0");

  //SetTexture(pTex, 0, FILTER_LINEAR); 
  
  for(int p(1); p<= nAmount; ++p)   
  {
    //Horizontal

    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam0Name, &pWeightsPS, 1);  
    gcpRendD3D->FX_PushRenderTarget(0, tpBlurTemp->m_pTexture, &gcpRendD3D->m_DepthBufferOrig);
    gRenDev->SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());        

    pTex->Apply(0, CTexture::GetTexState(sTexState)); 
    DrawFullScreenQuad(pTex->GetWidth(), pTex->GetHeight());

    gcpRendD3D->FX_PopRenderTarget(0);

    //Vertical

    pWeightsPS *= 2.0f;

    gcpRendD3D->FX_PushRenderTarget(0, pTex, &gcpRendD3D->m_DepthBufferOrig);
    gRenDev->SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());         

    CShaderMan::m_shPostEffects->FXSetVSFloat(pParam0Name, &pWeightsPS, 1);  
    tpBlurTemp->m_pTexture->Apply(0, CTexture::GetTexState(sTexState)); 
    DrawFullScreenQuad(pTex->GetWidth(), pTex->GetHeight());      

    gcpRendD3D->FX_PopRenderTarget(0);
  }             

  m_pCurrShader->FXEndPass();
  m_pCurrShader->FXEnd(); 

  // Restore previous viewport
  gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);

  //release dyntexture
  SAFE_DELETE(tpBlurTemp);

  gcpRendD3D->FX_Flush();
  PROFILE_SHADER_END      

  gcpRendD3D->Set2DMode(false, 1, 1);     
}

bool CD3D9Renderer::FX_DrawToRenderTarget(CShader *pShader, SRenderShaderResources* pRes, CRenderObject *pObj, SShaderTechnique *pTech, SHRenderTarget *pRT, int nPreprType, CRendElement *pRE)
{
   if (!pRT)   
    return false;
   
   uint nPrFlags = pRT->m_nFlags;
   if ( nPrFlags&FRT_RENDTYPE_CURSCENE )
     return false;

  if (!pRT->m_pTarget[0] && !pRT->m_pTarget[1])
	{
		if (pRT->m_refSamplerID >=0 && pRT->m_refSamplerID < EFTT_MAX)
		{
			IDynTextureSource* pDynTexSrc(pRes->m_Textures[pRT->m_refSamplerID]->m_Sampler.m_pDynTexSource);
			assert(pDynTexSrc);
			if (pDynTexSrc)
				return pDynTexSrc->Update(pObj->m_fDistance);
		}
    return false;
	}
  
  CRenderObject *pPrevIgn = m_RP.m_pIgnoreObject;
  CTexture *Tex = pRT->m_pTarget[0];
  SEnvTexture *pEnvTex = NULL;

  CRendElement *pSaveRE = m_RP.m_pRE;
  CShader *pSaveSH = m_RP.m_pShader;
  CRenderObject *pSaveObj = m_RP.m_pCurObject;
  SShaderTechnique *pSaveTech = m_RP.m_pCurTechnique;
  SRenderShaderResources *pSaveRes = m_RP.m_pShaderResources;
  m_RP.m_pRE = pRE;
  m_RP.m_pShader = pShader;
  m_RP.m_pCurObject = pObj;
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  m_RP.m_pCurTechnique = pTech;
  m_RP.m_pShaderResources = pRes;
  
  if (nPreprType == SPRID_SCANTEX)
  {
    nPrFlags |= FRT_CAMERA_REFLECTED_PLANE;
    pRT->m_nFlags = nPrFlags;
  }

  if( nPrFlags & FRT_RENDTYPE_CURSCENE )
    return false;
  
  uint nWidth = pRT->m_nWidth;
  uint nHeight = pRT->m_nHeight;

  if (pRT->m_nIDInPool >= 0)
  {
    assert((int)CTexture::m_CustomRT_2D.Num() > pRT->m_nIDInPool);
    pEnvTex = &CTexture::m_CustomRT_2D[pRT->m_nIDInPool];

    if (nWidth == -1)
      nWidth = GetWidth();
    if (nHeight == -1)
      nHeight = GetHeight();

    nWidth = sTexLimitRes(nWidth, uint(GetWidth() * 0.5));
    nHeight = sTexLimitRes(nHeight, uint(GetHeight() * 0.5));    

    ETEX_Format eTF = pRT->m_eTF;
    // $HDR
    if (eTF == eTF_A8R8G8B8 && IsHDRModeEnabled() && m_nHDRType <= 1)
      eTF = eTF_A16B16G16R16F;
    if (pEnvTex && (!pEnvTex->m_pTex || pEnvTex->m_pTex->GetFormat() != eTF))
    {
      char name[128];
      sprintf(name, "$RT_2D_%d", m_TexGenID++);
      int flags = FT_NOMIPS | FT_STATE_CLAMP | FT_DONT_STREAM;
      pEnvTex->m_pTex = new SDynTexture(nWidth, nHeight, eTF, eTT_2D, flags, name);
    }
    assert(nWidth > 0 && nWidth <= m_d3dsdBackBuffer.Width);
    assert(nHeight > 0 && nHeight <= m_d3dsdBackBuffer.Height);
    pEnvTex->m_pTex->Update(nWidth, nHeight);
    Tex = pEnvTex->m_pTex->m_pTexture;
  }
  else
  if (Tex)
  {
    if (Tex->GetCustomID() == TO_RT_2D)
    {
      bool bReflect = false;
      if (nPrFlags & (FRT_CAMERA_REFLECTED_PLANE | FRT_CAMERA_REFLECTED_WATERPLANE))
        bReflect = true;
      Matrix33 orientation = Matrix33(GetCamera().GetMatrix());
      Ang3 Angs = CCamera::CreateAnglesYPR(orientation);
      Vec3 Pos = GetCamera().GetPosition();
      bool bNeedUpdate = false;
			pEnvTex = CTexture::FindSuitableEnvTex(Pos, Angs, false, -1, false, pShader, pRes, pObj, bReflect, m_RP.m_pRE, &bNeedUpdate);

      m_RP.m_pRE = pSaveRE;
      m_RP.m_pShader = pSaveSH;
      m_RP.m_pCurObject = pSaveObj;
      m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
      m_RP.m_pCurTechnique = pSaveTech;
      m_RP.m_pShaderResources = pSaveRes;

      if (!bNeedUpdate)
      {
        if (!pEnvTex)
          return false;
        if (pEnvTex->m_pTex && pEnvTex->m_pTex->m_pTexture)
          return true;
      }
      m_RP.m_pIgnoreObject = pObj;
      switch (CRenderer::CV_r_envtexresolution)
      {
      case 0:
        nWidth = 64;
        break;
      case 1:
        nWidth = 128;
        break;
      case 2:
      default:
        nWidth = 256;
        break;
      case 3:
        nWidth = 512;
        break;
      }
      nWidth = sLimitSizeByScreenRes(nWidth);
      nHeight = nWidth;
      if (!pEnvTex->m_pTex->m_pTexture)
      {
        pEnvTex->m_pTex->Update(nWidth, nHeight);
      }
      Tex = pEnvTex->m_pTex->m_pTexture;
    }
    else
    if (Tex->GetCustomID() == TO_RT_CM)
    {
      Vec3 vPos = pObj->GetTranslation();
      float fDistToCam = (vPos-m_cam.GetPosition()).len();
      CRenderObject *pPrevIgn = m_RP.m_pIgnoreObject;
      m_RP.m_pIgnoreObject = pObj;
      pEnvTex = CTexture::FindSuitableEnvCMap(vPos, false, m_RP.m_pShaderResources->m_nReflectionMask, fDistToCam);
      m_RP.m_pIgnoreObject = pPrevIgn;

      m_RP.m_pRE = pSaveRE;
      m_RP.m_pShader = pSaveSH;
      m_RP.m_pCurObject = pSaveObj;
      m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
      m_RP.m_pCurTechnique = pSaveTech;
      m_RP.m_pShaderResources = pSaveRes;

      if (pEnvTex && pEnvTex->m_pTex->m_pTexture)
        return true;
      return false;
    }
    else
    if (Tex->GetCustomID() == TO_RT_LCM)
    {
      Vec3 vPos = pObj->GetTranslation();
      float fDistToCam = (vPos-m_cam.GetPosition()).len();
      CRenderObject *pPrevIgn = m_RP.m_pIgnoreObject;
      m_RP.m_pIgnoreObject = pObj;
      int nRendFlags = -1;
      nRendFlags &= ~(DLD_SHADOW_MAPS | DLD_ENTITIES | DLD_TERRAIN_LIGHT | DLD_PARTICLES | DLD_FAR_SPRITES | DLD_DETAIL_TEXTURES | DLD_DETAIL_OBJECTS);
      pEnvTex = CTexture::FindSuitableEnvLCMap(vPos, false, nRendFlags, fDistToCam, pObj);
      m_RP.m_pIgnoreObject = pPrevIgn;

      m_RP.m_pRE = pSaveRE;
      m_RP.m_pShader = pSaveSH;
      m_RP.m_pCurObject = pSaveObj;
      m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
      m_RP.m_pCurTechnique = pSaveTech;
      m_RP.m_pShaderResources = pSaveRes;

      if (pEnvTex && pEnvTex->m_pTex->m_pTexture)
        return true;
      return false;
    }
  }
  assert(Tex);
  if (!Tex)
    return false;

  if (Tex->IsLocked())
    return true;

  I3DEngine *eng = (I3DEngine *)iSystem->GetI3DEngine();

  static int nReflFrameID = 0;
  bool bMGPUAllowNextUpdate = (!nReflFrameID) && (CRenderer::CV_r_waterreflections_mgpu );

  // always allow for non-mgpu
  if( gRenDev->m_nGPUs == 1 || !CRenderer::CV_r_waterreflections_mgpu )
    bMGPUAllowNextUpdate = true;  

  ETEX_Format eTF = pRT->m_eTF;
  // $HDR
  if (eTF == eTF_A8R8G8B8 && IsHDRModeEnabled() && m_nHDRType <= 1)
    eTF = eTF_A16B16G16R16F;
  if (pEnvTex && (!pEnvTex->m_pTex || pEnvTex->m_pTex->GetFormat() != eTF))
  {
    SAFE_DELETE(pEnvTex->m_pTex);
    char name[128];
    sprintf(name, "$RT_2D_%d", m_TexGenID++);
    int flags = FT_NOMIPS | FT_STATE_CLAMP | FT_DONT_STREAM;
    pEnvTex->m_pTex = new SDynTexture(nWidth, nHeight, eTF, eTT_2D, flags, name);
    assert(nWidth > 0 && nWidth <= m_d3dsdBackBuffer.Width);
    assert(nHeight > 0 && nHeight <= m_d3dsdBackBuffer.Height);
    pEnvTex->m_pTex->Update(nWidth, nHeight);
  }


  float matProj[16], matView[16];

  switch (pRT->m_eUpdateType)
  {
  case eRTUpdate_WaterReflect:
    {
      if( !CRenderer::CV_r_waterreflections )
      {
        ColorF c = ColorF(0, 0, 0, 1);
        pEnvTex->m_pTex->m_pTexture->Fill(c);
        return true;
      }

      if( m_RP.m_nLastWaterFrameID == GetFrameID() )
        // water reflection already created this frame, share it
        return true;

//      int nVisibleWaterPixelsCount = eng->GetOceanVisiblePixelsCount() / 2; // bug in occlusion query returns 2x more
//      int nPixRatioThreshold = (int)(GetWidth() * GetHeight() * CRenderer::CV_r_waterreflections_min_visible_pixels_update);

//      m_RP.m_nCurrUpdateType = 0;
//      float fUpdateFactorMul = 1.0f;
//      float fUpdateDistanceMul = 1.0f;
//      if( nVisibleWaterPixelsCount < nPixRatioThreshold /4) 
//      {
//        m_RP.m_nCurrUpdateType = 2;
//        fUpdateFactorMul = CV_r_waterreflections_minvis_updatefactormul * 10.0f;
//        fUpdateDistanceMul = CV_r_waterreflections_minvis_updatedistancemul * 5.0f;
//      }
//      else
//      if( nVisibleWaterPixelsCount < nPixRatioThreshold) 
//      {
//        m_RP.m_nCurrUpdateType = 1;
//        fUpdateFactorMul = CV_r_waterreflections_minvis_updatefactormul;
//        fUpdateDistanceMul = CV_r_waterreflections_minvis_updatedistancemul;
//      }

//      float fMGPUScale = 1.0f;//CRenderer::CV_r_waterreflections_mgpu? (1.0f / (float) gRenDev->m_nGPUs) : 1.0f;
//      float fWaterUpdateFactor = CV_r_waterupdateFactor * fUpdateFactorMul * fMGPUScale;
//      float fWaterUpdateDistance = CV_r_waterupdateDistance * fUpdateDistanceMul * fMGPUScale;

      
//      m_RP.m_fCurrUpdateFactor = fWaterUpdateFactor;
//      m_RP.m_fCurrUpdateDistance = fWaterUpdateDistance;

//      float fTimeUpd = min(0.3f, eng->GetDistanceToSectorWithWater());
//      fTimeUpd *= fWaterUpdateFactor;
      //if (fTimeUpd > 1.0f)
      //fTimeUpd = 1.0f; 

			float fWaterDist = eng->GetDistanceToSectorWithWater();
			float fWaterDistScaled = 0.01f * ( max( fWaterDist, 1.001f ) - 1.0f );
			fWaterDistScaled *= fWaterDistScaled;
			float fTimeUpd = min( CV_r_waterUpdateTimeMin + fWaterDistScaled * CV_r_waterUpdateTimeMax, 0.3f );

      Vec3 camView = m_rcam.ViewDir();
      Vec3 camUp = m_rcam.Y;

      Vec3 camPos = GetCamera().GetPosition();

//      float fDistCam = (camPos - m_RP.m_LastWaterPosUpdate).GetLength();
//      float fDotView = camView * m_RP.m_LastWaterViewdirUpdate;
//      float fDotUp = camUp * m_RP.m_LastWaterUpdirUpdate;
//      float fFOV = GetCamera().GetFov();

			Matrix44 mProj, mView, mViewInv, mViewProj;
			GetProjectionMatrix(mProj.GetData());
			GetModelViewMatrix(mView.GetData());
			mViewInv = mView.GetInverted();
			mViewProj = mView * mProj;

			mProj.Transpose();
			mView.Transpose();
			mViewInv.Transpose();
			mViewProj.Transpose();

			Vec3 vFrustumVecs[4];
			vFrustumVecs[0] = mViewInv.TransformVector( Vec3(  1.0f / mProj.m00,  1.0f / mProj.m11, 1.0f ));
			vFrustumVecs[1] = mViewInv.TransformVector( Vec3( -1.0f / mProj.m00,  1.0f / mProj.m11, 1.0f ));
			vFrustumVecs[2] = mViewInv.TransformVector( Vec3(  1.0f / mProj.m00, -1.0f / mProj.m11, 1.0f ));
			vFrustumVecs[3] = mViewInv.TransformVector( Vec3( -1.0f / mProj.m00, -1.0f / mProj.m11, 1.0f ));

			float max_t = GetCamera().GetFarPlane();
			float min_t = GetCamera().GetNearPlane();
			float water_level = eng->GetWaterLevel();

			Vec3 vFrustumPoints[4];
			for ( int i = 0; i < 4; i++ )
			{
				vFrustumVecs[i].Normalize();

				float t = ( camPos.z - water_level ) / vFrustumVecs[i].z;
				t = ( t <= 0.0f ) ? max_t : min( max( min_t, t ), max_t );
				vFrustumPoints[i] = camPos + t * vFrustumVecs[i];
				vFrustumPoints[i].z = water_level;
			}

			float fFrustumPointsDist = 0;
			Vec3 vFrustumPointsProj, vLastFrustumPointsProj;
			for ( int i = 0; i < 4; i++ )
			{
				vFrustumPointsProj = mViewProj.TransformPoint( vFrustumPoints[i] );
				vFrustumPointsProj.x /= vFrustumPointsProj.z;
				vFrustumPointsProj.y /= vFrustumPointsProj.z;

				vLastFrustumPointsProj = mViewProj.TransformPoint( m_RP.m_vLastWaterFrustumPoints[i] );
				vLastFrustumPointsProj.x /= vLastFrustumPointsProj.z;
				vLastFrustumPointsProj.y /= vLastFrustumPointsProj.z;

				float frustum_point_dist = (vFrustumPointsProj - vLastFrustumPointsProj).GetLength2D();
				fFrustumPointsDist = max( fFrustumPointsDist, frustum_point_dist );
			}

      if (m_RP.m_fLastWaterUpdate-1.0f > m_RP.m_RealTime)
        m_RP.m_fLastWaterUpdate = m_RP.m_RealTime;

//      const float fMaxFovDiff = 0.1f;		// no exact test to prevent slowly changing fov causing per frame water reflection updates

//      if (m_RP.m_RealTime-m_RP.m_fLastWaterUpdate >= fTimeUpd || fDistCam > fWaterUpdateDistance || fDotView<0.9f
//        || fabs(fFOV-m_RP.m_fLastWaterFOVUpdate)>fMaxFovDiff)

      static bool bUpdateReflection = true;
      //if( bMGPUAllowNextUpdate )
      {
        bUpdateReflection = m_RP.m_RealTime-m_RP.m_fLastWaterUpdate >= fTimeUpd || fFrustumPointsDist > CV_r_waterUpdateChange;
        // bUpdateReflection = m_RP.m_RealTime-m_RP.m_fLastWaterUpdate >= fTimeUpd || fDistCam > fWaterUpdateDistance;
        // bUpdateReflection = bUpdateReflection || fDotView<0.9f || fabs(fFOV-m_RP.m_fLastWaterFOVUpdate)>fMaxFovDiff;
      }

      int nCurrFrameDebug = GetFrameID(false)%10;

      if ( bUpdateReflection )
      {
        m_RP.m_fLastWaterUpdate = m_RP.m_RealTime;

//        m_RP.m_LastWaterViewdirUpdate = camView;
//        m_RP.m_LastWaterUpdirUpdate = camUp;
//        m_RP.m_fLastWaterFOVUpdate = fFOV;
//        m_RP.m_LastWaterPosUpdate = camPos;
//        m_RP.m_bLastWaterAnisotropicRefl = (nVisibleWaterPixelsCount > nPixRatioThreshold /4);

				m_RP.m_vLastWaterFrustumPoints[0] = vFrustumPoints[0];
				m_RP.m_vLastWaterFrustumPoints[1] = vFrustumPoints[1];
				m_RP.m_vLastWaterFrustumPoints[2] = vFrustumPoints[2];
				m_RP.m_vLastWaterFrustumPoints[3] = vFrustumPoints[3];

        // Reset masks for next frame updates
        nReflFrameID = 0;
        bMGPUAllowNextUpdate = true;
      }
      else
      if ( !CRenderer::CV_r_waterreflections_mgpu || CRenderer::CV_r_waterreflections_mgpu && nReflFrameID >=  gRenDev->m_nGPUs )
      {
          m_RP.m_bFrameUpdated[nCurrFrameDebug] = false;
          return true;
      }

      m_RP.m_bFrameUpdated[nCurrFrameDebug] = true;

      nReflFrameID ++;
    }
    break;
  }

  // Just copy current BB to the render target and exit
  if (nPrFlags & FRT_RENDTYPE_COPYSCENE)
  {
    // Get current render target from the RT stack
    if( !CRenderer::CV_r_debugrefraction )
      FX_ScreenStretchRect( Tex ); // should encode hdr format
    else
    {
      ColorF c = ColorF(1, 0, 0, 1);
      Tex->Fill(c);
    }

    return true;
  }

//  I3DEngine *eng = (I3DEngine *)iSystem->GetI3DEngine();
//  float matProj[16], matView[16];

  Matrix44 m;
  
  float plane[4];
  bool bUseClipPlane = false;
  bool bChangedCamera = false;

  int nPersFlags = m_RP.m_PersFlags;
  int nPersFlags2 = m_RP.m_PersFlags2;
  
  static CCamera tmp_cam_mgpu = GetCamera();
  CCamera tmp_cam = GetCamera();
  CCamera prevCamera = tmp_cam;
  bool bMirror = false;
  bool bOceanRefl = false;

  // Set the camera
  if (nPrFlags & FRT_CAMERA_REFLECTED_WATERPLANE)
  {
    bOceanRefl = true;

    m_RP.m_pIgnoreObject = pObj;
    float fMinDist = min(SKY_BOX_SIZE*0.5f, eng->GetDistanceToSectorWithWater()); // 16 is half of skybox size
    float fMaxDist = eng->GetMaxViewDistance();

    Vec3 vPrevPos = tmp_cam.GetPosition();
    Vec4 pOceanParams0, pOceanParams1;
    eng->GetOceanAnimationParams(pOceanParams0, pOceanParams1);

    Plane Pl;
    Pl.n = Vec3(0,0,1);
    Pl.d = eng->GetOceanWaterLevel(vPrevPos); // + CRenderer::CV_r_waterreflections_offset;// - pOceanParams1.x;         
    if ((vPrevPos | Pl.n) - Pl.d < 0)
    {
      Pl.d = -Pl.d;
      Pl.n = -Pl.n;
    }

    plane[0] = Pl.n[0];
    plane[1] = Pl.n[1];
    plane[2] = Pl.n[2];
    plane[3] = -Pl.d ;

    Matrix44 camMat;
    GetModelViewMatrix(camMat.GetData());
    Vec3 vPrevDir = Vec3(-camMat(0,2), -camMat(1,2), -camMat(2,2));
    Vec3 vPrevUp = Vec3(camMat(0,1), camMat(1,1), camMat(2,1));
    Vec3 vNewDir = Pl.MirrorVector(vPrevDir);
    Vec3 vNewUp = Pl.MirrorVector(vPrevUp);
    float fDot = vPrevPos.Dot(Pl.n) - Pl.d;
    Vec3 vNewPos = vPrevPos - Pl.n * 2.0f*fDot;
    Matrix34 mTmp = sMatrixLookAt( vNewDir, vNewUp, tmp_cam.GetAngles()[2] );
    mTmp.SetTranslation(vNewPos);
    tmp_cam.SetMatrix(mTmp);

    //this is the new code to calculate the reflection matrix
    //Matrix34 RefMatrix34 = CreateReflectionMat3( Pl );
    //Matrix34 matMir=RefMatrix34*tmp_cam.GetMatrix();
    //tmp_cam.SetMatrix(matMir);

    float fDistOffset = fMinDist;
    if( CV_r_waterreflections_use_min_offset )
    {
      fDistOffset = max( fMinDist, 2.0f * iSystem->GetI3DEngine()->GetDistanceToSectorWithWater() );
      if ( fDistOffset  >= fMaxDist ) // engine returning bad value
        fDistOffset = fMinDist; 
    }

    tmp_cam.SetFrustum((int)(Tex->GetWidth()*tmp_cam.GetProjRatio()), Tex->GetHeight(), tmp_cam.GetFov(), fDistOffset, fMaxDist); //tmp_cam.GetFarPlane());

    // Allow camera update
    if( bMGPUAllowNextUpdate ) 
      tmp_cam_mgpu = tmp_cam;

    SetCamera( tmp_cam_mgpu );
    bChangedCamera = true;
    bUseClipPlane = true;
    bMirror = true;
    //m_RP.m_PersFlags |= RBPF_MIRRORCULL;
  }
  else
  if (nPrFlags & FRT_CAMERA_REFLECTED_PLANE)
  {
    m_RP.m_pIgnoreObject = pObj;
    float fMinDist = 0.25f;
    float fMaxDist = eng->GetMaxViewDistance();

    Vec3 vPrevPos = tmp_cam.GetPosition();

    if (pRes && pRes->m_pCamera)
    {
      tmp_cam = *pRes->m_pCamera; // Portal case
      //tmp_cam.SetPosition(Vec3(310, 150, 30));
      //tmp_cam.SetAngles(Vec3(-90,0,0));
      //tmp_cam.SetFrustum((int)(Tex->GetWidth()*tmp_cam.GetProjRatio()), Tex->GetHeight(), tmp_cam.GetFov(), fMinDist, tmp_cam.GetFarPlane());

      SetCamera(tmp_cam);
      bUseClipPlane = false;
      bMirror = false;
      SAFE_DELETE_ARRAY(Tex->m_Matrix);
    }
    else
    { // Mirror case
      Plane Pl;
      pRE->mfGetPlane(Pl);
      //Pl.d = -Pl.d;
      if (pObj)
      {
        m_RP.m_FrameObject++;
        Matrix44 mat = GetTransposed44(Matrix44(pObj->m_II.m_Matrix));
        Pl = TransformPlane(mat, Pl);
      }
      if ((vPrevPos | Pl.n) - Pl.d < 0)
      {
        Pl.d = -Pl.d;
        Pl.n = -Pl.n;
      }

      plane[0] = Pl.n[0];
      plane[1] = Pl.n[1];
      plane[2] = Pl.n[2];
      plane[3] = -Pl.d;

      //this is the new code to calculate the reflection matrix

      Matrix44 camMat;
      GetModelViewMatrix(camMat.GetData());
      Vec3 vPrevDir = Vec3(-camMat(0,2), -camMat(1,2), -camMat(2,2));
      Vec3 vPrevUp = Vec3(camMat(0,1), camMat(1,1), camMat(2,1));
      Vec3 vNewDir = Pl.MirrorVector(vPrevDir);
      Vec3 vNewUp = Pl.MirrorVector(vPrevUp);
      float fDot = vPrevPos.Dot(Pl.n) - Pl.d;
      Vec3 vNewPos = vPrevPos - Pl.n * 2.0f*fDot;
      Matrix34 m = sMatrixLookAt( vNewDir, vNewUp, tmp_cam.GetAngles()[2] );
      m.SetTranslation(vNewPos);
      tmp_cam.SetMatrix(m);

      //Matrix34 RefMatrix34 = CreateReflectionMat3(Pl);
      //Matrix34 matMir=RefMatrix34*tmp_cam.GetMatrix();
      //tmp_cam.SetMatrix(matMir);
      tmp_cam.SetFrustum((int)(Tex->GetWidth()*tmp_cam.GetProjRatio()), Tex->GetHeight(), tmp_cam.GetFov(), fMinDist, fMaxDist); //tmp_cam.GetFarPlane());
      bMirror = true;
      bUseClipPlane = true;
    }
    SetCamera(tmp_cam);
    bChangedCamera = true;
    //m_RP.m_PersFlags |= RBPF_MIRRORCULL;
  }
  else
  if (((nPrFlags & FRT_CAMERA_CURRENT) || (nPrFlags & FRT_RENDTYPE_CURSCENE)) && pRT->m_eOrder == eRO_PreDraw && !(nPrFlags & FRT_RENDTYPE_CUROBJECT))
  {
    // Always restore stuff after explicitly changing...

    m_RP.m_pRE = pSaveRE;
    m_RP.m_pShader = pSaveSH;
    m_RP.m_pCurObject = pSaveObj;
    m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
    m_RP.m_pCurTechnique = pSaveTech;
    m_RP.m_pShaderResources = pSaveRes;

    // get texture surface
    // Get current render target from the RT stack
    if( !CRenderer::CV_r_debugrefraction )
      FX_ScreenStretchRect( Tex ); // should encode hdr format
    else
    {
      ColorF c = ColorF(1, 0, 0, 1);
      Tex->Fill(c);
    }

    m_RP.m_pIgnoreObject = pPrevIgn;    
    return true;
  }

//  if (pRT->m_nFlags & FRT_CAMERA_CURRENT)
//  {
//  //m_RP.m_pIgnoreObject = pObj;
//
//  SetCamera(tmp_cam);
//  bChangedCamera = true;
//  bUseClipPlane = true;
//  }

  bool bRes = true;

  int TempX, TempY, TempWidth, TempHeight;
  GetViewport(&TempX, &TempY, &TempWidth, &TempHeight);
  //SetViewport(0, 0, Tex->GetWidth(), Tex->GetHeight());
  EF_PushFog();
  m_RP.m_PersFlags |= RBPF_DRAWTOTEXTURE;
  m_RP.m_PersFlags2 |= RBPF2_ENCODE_HDR;

  if (m_LogFile)
    Logv(SRendItem::m_RecurseLevel, "*** Set RT for Water reflections ***\n");

  FX_PushRenderTarget(0, Tex, pRT->m_bTempDepth ? FX_GetDepthSurface(Tex->GetWidth(), Tex->GetHeight(), false) : &m_DepthBufferOrig);
  float fClearDepth = pRT->m_fClearDepth;

  //
  EF_ClearBuffers(pRT->m_nFlags|FRT_CLEAR_IMMEDIATE, &pRT->m_ClearColor, fClearDepth);

  float fAnisoScale = 1.0f;
  if (pRT->m_nFlags & FRT_RENDTYPE_CUROBJECT)
  {
    CCryName nameTech = pTech->m_Name;
    char newTech[128];
    sprintf(newTech, "%s_RT", nameTech.c_str());
    SShaderTechnique *pTech = pShader->mfFindTechnique(newTech);
    if (!pTech)
      iLog->Log("Error: CD3D9Renderer::FX_DrawToRenderTarget: Couldn't find technique '%s' in shader '%s'\n", newTech, pShader->GetName());
    else
    {
      EF_ObjectChange(pShader, pRes, pObj, pRE);
      EF_Start(pShader, -1, pRes, pRE);
      pRE->mfPrepare();
      FX_DrawShader_General(pShader, pTech, false, true);
    }
    m_RP.m_FrameObject++;
  }
  else
  {
    if (bMirror)
    {
      if (!Tex->m_Matrix)
        Tex->m_Matrix = new float[16];
      GetModelViewMatrix(matView);
      GetProjectionMatrix(matProj);

      float matScaleBias[16]={
        0.5f,     0,    0,   0,
        0, -0.5f,    0,   0,
        0,     0, 0.5f,   0,
        // texel alignment - also push up y axis reflection up a bit
        0.5f+0.5f/Tex->GetWidth(), 0.5f+ 1.0f/Tex->GetHeight(), 0.5f, 1.0f  
      };

      mathMatrixMultiply(m.GetData(), matScaleBias, matProj, g_CpuFlags);
      mathMatrixMultiply(Tex->m_Matrix, m.GetData(), matView, g_CpuFlags);
    }
    m_RP.m_PersFlags |= RBPF_OBLIQUE_FRUSTUM_CLIPPING;
    m_RP.m_PersFlags2 |= RBPF2_NOCLEARBUF | RBPF_MIRRORCAMERA;// | RBPF_MIRRORCULL;

    //  if (bUseClipPlane)
    //      EF_SetClipPlane(true, plane, false);

    Plane p;
    p.n[0] = plane[0];
    p.n[1] = plane[1];
    p.n[2] = plane[2];
    p.d = plane[3]; // +0.25f;    
    fAnisoScale = plane[3];
    fAnisoScale = fabs(fabs(fAnisoScale) - GetCamera().GetPosition().z); 
    m_RP.m_bObliqueClipPlane = true;

    // put clipplane in clipspace..
    m_RP.m_pObliqueClipPlane = TransformPlane2(gRenDev->m_InvCameraProjMatrix, p);

    int RendFlags = (gRenDev->m_RP.m_eQuality)? DLD_TERRAIN : 0;    

    int nReflQuality = ( bOceanRefl )? CV_r_waterreflections_quality : CV_r_reflections_quality; 

    // set reflection quality setting
    switch( nReflQuality )
    {
      case 1: RendFlags |= DLD_ENTITIES;   break;
      case 2: RendFlags |= DLD_DETAIL_TEXTURES | DLD_ENTITIES ;   break;
      case 3: 
        RendFlags |= DLD_STATIC_OBJECTS | DLD_ENTITIES|DLD_DETAIL_TEXTURES;
          break;
      case 4: 
        RendFlags |= DLD_STATIC_OBJECTS | DLD_ENTITIES|DLD_DETAIL_TEXTURES|DLD_PARTICLES;
        break;
      case 5: 
        RendFlags = -1;
        break;
    }

    // Disable first person hands
    int nWaterCaustics = CRenderer::CV_r_watercaustics;
    int nNoDrawNear = CRenderer::CV_r_nodrawnear;
    CRenderer::CV_r_nodrawnear = 1;

    // disable caustics if camera above water
    if( p.d < 0)
      CRenderer::CV_r_watercaustics = 0;

    eng->RenderWorld(SHDF_ALLOWHDR | SHDF_SORT, bOceanRefl? tmp_cam_mgpu : tmp_cam, "FX_DrawToRenderTarget", RendFlags, pRT->m_nFilterFlags);
    
    CRenderer::CV_r_nodrawnear = nNoDrawNear; 
    CRenderer::CV_r_watercaustics = nWaterCaustics;

    m_RP.m_bObliqueClipPlane = false;
    m_RP.m_PersFlags &= ~RBPF_OBLIQUE_FRUSTUM_CLIPPING;

    //if (bUseClipPlane)
    //EF_SetClipPlane(false, plane, false);
  }
  FX_PopRenderTarget(0);

  // Very Hi specs get anisotropic reflections
  if( gRenDev->m_RP.m_eQuality >= eRQ_VeryHigh && m_RP.m_bLastWaterAnisotropicRefl)
    TexBlurAnisotropicVertical(Tex, 1, 8 * max( 1.0f - min( fAnisoScale / 100.0f, 1.0f), 0.2f), 1, false);

  if (m_LogFile)
    Logv(SRendItem::m_RecurseLevel, "*** End RT for Water reflections ***\n");

  // todo: encode hdr format

  m_RP.m_PersFlags = nPersFlags;
  m_RP.m_PersFlags2 = nPersFlags2;

  if (bChangedCamera)
    SetCamera(prevCamera);

  SetViewport(TempX, TempY, TempWidth, TempHeight);
  EF_PopFog();

  m_RP.m_pIgnoreObject = pPrevIgn;

  // increase frame id to support multiple recursive draws
  m_nFrameID++;

  CHWShader_D3D::mfSetGlobalParams();

  m_RP.m_pRE = pSaveRE;
  m_RP.m_pShader = pSaveSH;
  m_RP.m_pCurObject = pSaveObj;
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  m_RP.m_pCurTechnique = pSaveTech;
  m_RP.m_pShaderResources = pSaveRes;

  return bRes;
}

