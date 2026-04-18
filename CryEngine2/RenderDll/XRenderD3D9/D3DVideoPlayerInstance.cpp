#include "StdAfx.h"

#include "../Common/VideoPlayerInstance.h"
#include "DriverD3D.h"


#ifndef EXCLUDE_CRI_SDK


void CVideoPlayer::ClearTexture(CTexture* pTexture, unsigned char clearValue)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);

	if (pTexture && pTexture->GetDeviceTexture())
	{
		assert(pTexture->GetDstFormat() == eTF_A8);
#if defined (DIRECT3D9) || defined(OPENGL)
		IDirect3DTexture9* pTex((IDirect3DTexture9*) pTexture->GetDeviceTexture());
		assert(pTex);
		D3DLOCKED_RECT lockedRect;
		if (SUCCEEDED(pTex->LockRect(0, &lockedRect, 0, D3DLOCK_DISCARD)))
#else
		ID3D10Texture2D* pTex((ID3D10Texture2D*) pTexture->GetDeviceTexture());
		assert(pTex);
		D3D10_MAPPED_TEXTURE2D lockedRect;
		if (SUCCEEDED(pTex->Map(0, D3D10_MAP_WRITE_DISCARD, 0, &lockedRect)))
#endif
		{
#if defined (DIRECT3D9) || defined(OPENGL)
			D3DSURFACE_DESC desc;
			pTex->GetLevelDesc(0, &desc);
			for (int y(0); y < desc.Height; ++y)
				memset((void*) ((size_t)lockedRect.pBits + y * lockedRect.Pitch), clearValue, desc.Width);
			pTex->UnlockRect(0);
#else
			D3D10_TEXTURE2D_DESC desc;
			pTex->GetDesc(&desc);
			for (int y(0); y < desc.Height; ++y)
				memset((void*) ((size_t)lockedRect.pData + y * lockedRect.RowPitch), clearValue, desc.Width);
			pTex->Unmap(0);
#endif
		}
	}
}


void CVideoPlayer::UploadTextureData(CTexture* pTexture, void* pSrcData, int srcWidth)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);

	if (pTexture && pTexture->GetDeviceTexture())
	{
		assert(pTexture->GetDstFormat() == eTF_A8);
#if defined (DIRECT3D9) || defined(OPENGL)
		IDirect3DTexture9* pTex((IDirect3DTexture9*) pTexture->GetDeviceTexture());
		assert(pTex);
		D3DLOCKED_RECT lockedRect;
		if (SUCCEEDED(pTex->LockRect(0, &lockedRect, 0, D3DLOCK_DISCARD)))
#else
		ID3D10Texture2D* pTex((ID3D10Texture2D*) pTexture->GetDeviceTexture());
		assert(pTex);
		D3D10_MAPPED_TEXTURE2D lockedRect;
		if (SUCCEEDED(pTex->Map(0, D3D10_MAP_WRITE_DISCARD, 0, &lockedRect)))
#endif
		{
#if defined (DIRECT3D9) || defined(OPENGL)
			D3DSURFACE_DESC desc;
			pTex->GetLevelDesc(0, &desc);
			assert(srcWidth >= desc.Width);
			for (int y(0); y < desc.Height; ++y)
				memcpy((void*) ((size_t)lockedRect.pBits + y * lockedRect.Pitch), (void*) ((size_t)pSrcData + y * srcWidth), desc.Width);
			pTex->UnlockRect(0);
#else
			D3D10_TEXTURE2D_DESC desc;
			pTex->GetDesc(&desc);
			assert(srcWidth >= desc.Width);
			for (int y(0); y < desc.Height; ++y)
				memcpy((void*) ((size_t)lockedRect.pData + y * lockedRect.RowPitch), (void*) ((size_t)pSrcData + y * srcWidth), desc.Width);
			pTex->Unmap(0);
#endif
		}
	}
}


void CVideoPlayer::Display()
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	CD3D9Renderer* rd(gcpRendD3D);
	assert(m_pCriMwShader);

	bool isFullAlphaMovie(IsFullAlphaMovie());

	// setup matrices
	rd->m_matView->Push();
	rd->m_matView->LoadIdentity();

	int viewportX0, viewportY0, viewportWidth, viewportHeight;
	rd->GetViewport(&viewportX0, &viewportY0, &viewportWidth, &viewportHeight);

	rd->m_matProj->Push();
	Matrix44* m(rd->m_matProj->GetTop());
	mathMatrixOrthoOffCenterLH(m, viewportX0, viewportX0 + viewportWidth, viewportY0 + viewportHeight, viewportY0, -1, 1);

	bool texResourcesOk(!DynTexDataLost());
	if (texResourcesOk)
	{
		CTexture* pTex((CTexture*) m_pCurFrameY);
		texResourcesOk &= pTex && pTex->GetDeviceTexture();

		pTex = (CTexture*) m_pCurFrameCb;
		texResourcesOk &= pTex && pTex->GetDeviceTexture();

		pTex = (CTexture*) m_pCurFrameCr;
		texResourcesOk &= pTex && pTex->GetDeviceTexture();

		if (isFullAlphaMovie)
		{
			pTex = (CTexture*) m_pCurFrameA;
			texResourcesOk &= pTex && pTex->GetDeviceTexture();
		}
	}

	if (texResourcesOk)
	{
		if (isFullAlphaMovie)
			m_pCriMwShader->FXSetTechnique("CCIR601_ColAlpha");
		else
			m_pCriMwShader->FXSetTechnique("CCIR601");
	}
	else
		m_pCriMwShader->FXSetTechnique("Fallback");

	uint passes(0);
	m_pCriMwShader->FXBegin(&passes, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	m_pCriMwShader->FXBeginPass(0);
	
	if (texResourcesOk)
	{
		((CTexture*) m_pCurFrameY)->Apply(0);
		((CTexture*) m_pCurFrameCb)->Apply(1);
		((CTexture*) m_pCurFrameCr)->Apply(2);
		if (isFullAlphaMovie)
			((CTexture*) m_pCurFrameA)->Apply(3);
	}

	int nOffs;
	struct_VERTEX_FORMAT_P3F_TEX2F* pQuad((struct_VERTEX_FORMAT_P3F_TEX2F*) rd->GetVBPtr(4, nOffs, POOL_P3F_TEX2F));
  if (!pQuad)
  {
    rd->m_matView->Pop();
    rd->m_matProj->Pop();
    return;
  }

#if defined(DIRECT3D9)
	const float halfPixelOffset(0.5f);
#else
	const float halfPixelOffset(0.0f);
#endif

	pQuad[0].xyz.x = m_viewportX0 + m_viewportWidth - halfPixelOffset;
	pQuad[0].xyz.y = m_viewportY0 - halfPixelOffset;
	pQuad[0].xyz.z = 0;
	pQuad[0].st[0] = 1;
	pQuad[0].st[1] = 0;

	pQuad[1].xyz.x = m_viewportX0 - halfPixelOffset;
	pQuad[1].xyz.y = m_viewportY0 - halfPixelOffset;
	pQuad[1].xyz.z = 0;
	pQuad[1].st[0] = 0;
	pQuad[1].st[1] = 0;

	pQuad[2].xyz.x = m_viewportX0 + m_viewportWidth - halfPixelOffset;
	pQuad[2].xyz.y = m_viewportY0 + m_viewportHeight - halfPixelOffset;
	pQuad[2].xyz.z = 0;
	pQuad[2].st[0] = 1;
	pQuad[2].st[1] = 1;

	pQuad[3].xyz.x = m_viewportX0 - halfPixelOffset;
	pQuad[3].xyz.y = m_viewportY0 + m_viewportHeight - halfPixelOffset;
	pQuad[3].xyz.z = 0;
	pQuad[3].st[0] = 0;
	pQuad[3].st[1] = 1;

	rd->UnlockVB(POOL_P3F_TEX2F);

	int state(GS_NODEPTHTEST);
	if (isFullAlphaMovie)
		state |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
	if (rd->CV_r_measureoverdraw)
	{
		state = (state & ~GS_BLEND_MASK) | (GS_BLSRC_ONE | GS_BLDST_ONE);
		state &= ~GS_ALPHATEST_MASK;
	}
	rd->EF_SetState(state);

	rd->D3DSetCull(eCULL_None);

	rd->EF_Commit();
	rd->FX_SetVStream(0, rd->m_pVB[POOL_P3F_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_P3F_TEX2F));
	if (!FAILED(rd->EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_TEX2F)))
  {
  #if defined (DIRECT3D9) || defined (OPENGL)
	  rd->m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, 2);
  #elif defined (DIRECT3D10)
	  rd->SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	  rd->m_pd3dDevice->Draw(4, nOffs);
  #endif
  }

	m_pCriMwShader->FXEndPass();
	m_pCriMwShader->FXEnd();

	rd->m_matView->Pop();
	rd->m_matProj->Pop();
	rd->EF_DirtyMatrix();
}


#endif // #ifndef EXCLUDE_CRI_SDK
