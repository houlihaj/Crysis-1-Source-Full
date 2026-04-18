#include "StdAfx.h"
#include "DriverD3D.h"

//=========================================================================================

#include "../CryFont/FBitmap.h"

bool CD3D9Renderer::FontUpdateTexture(int nTexId, int nX, int nY, int USize, int VSize, byte *pData)
{
  CTexture *tp = CTexture::GetByID(nTexId);
  assert (tp);

  if (tp)
  {
#ifdef FONT_USE_32BIT_TEXTURE
    tp->UpdateTextureRegion(pData, nX, nY, USize, VSize, eTF_A8R8G8B8);
#else // FONT_USE_32BIT_TEXTURE
    tp->UpdateTextureRegion(pData, nX, nY, USize, VSize, eTF_A8);
#endif // FONT_USE_32BIT_TEXTURE

		return true;
  }
  return false;
}

bool CD3D9Renderer::FontUploadTexture(class CFBitmap* pBmp, ETEX_Format eTF)
{
  if(!pBmp)
	{
		return false;
	}

	unsigned int *pData = new unsigned int[pBmp->GetWidth() * pBmp->GetHeight()];

	if (!pData)
	{
		return false;
	}

	pBmp->Get32Bpp(&pData);

	char szName[128];
	sprintf(szName, "$AutoFont_%d", m_TexGenID++);

	int iFlags = FT_TEX_FONT | FT_DONT_STREAM;
  CTexture *tp = CTexture::Create2DTexture(szName, pBmp->GetWidth(), pBmp->GetHeight(), 1, iFlags, (unsigned char *)pData, eTF, eTF);

	SAFE_DELETE_ARRAY(pData);

	pBmp->SetRenderData((void *)tp);
	
	return true;
}

int CD3D9Renderer::FontCreateTexture(int Width, int Height, byte *pData, ETEX_Format eTF, bool genMips)
{
  if (!pData)
    return -1;

  char szName[128];
  sprintf(szName, "$AutoFont_%d", m_TexGenID++);

  int iFlags = FT_TEX_FONT | FT_DONT_STREAM | FT_DONT_RESIZE | FT_DONT_ANISO;
	if( genMips )
		iFlags |= FT_FORCE_MIPS;
	CTexture *tp = CTexture::Create2DTexture(szName, Width, Height, 1, iFlags, (unsigned char *)pData, eTF, eTF);

  return tp->GetID();
}

void CD3D9Renderer::FontReleaseTexture(class CFBitmap *pBmp)
{
	if(!pBmp)
	{
		return;
	}
  
	CTexture *tp = (CTexture *)pBmp->GetRenderData();

  SAFE_RELEASE(tp);
}

void CD3D9Renderer::FontSetTexture(class CFBitmap* pBmp, int nFilterMode)
{
	if (pBmp)
	{
		CTexture *tp = (CTexture *)pBmp->GetRenderData();
    if (tp)
    {
      tp->SetFilterMode(nFilterMode);
	  	tp->Apply();
    }
  }
}

void CD3D9Renderer::FontSetTexture(int nTexId, int nFilterMode)
{
  if (nTexId <= 0)
    return;
  CTexture *tp = CTexture::GetByID(nTexId);
  assert (tp);
  if (!tp)
    return;

  if (CV_d3d9_forcesoftware)
    return;

#ifdef OPENGL
  nFilterMode = FILTER_LINEAR;
#endif
  tp->SetFilterMode(nFilterMode);
  tp->Apply();
  //CTexture::m_Text_NoTexture->Apply(0);
}

int s_InFontState = 0;

void CD3D9Renderer::FontSetRenderingState(unsigned int nVPWidth, unsigned int nVPHeight)
{
  assert(!s_InFontState);

  // setup various d3d things that we need
  FontSetState(false);

  s_InFontState++;

  D3DXMATRIX *m;
  m_matProj->Push();
  m_matProj->LoadIdentity();
  m = (D3DXMATRIX*)m_matProj->GetTop();
  
	if (m_NewViewport.Width != 0 && m_NewViewport.Height != 0)
		mathMatrixOrthoOffCenter((Matrix44*)m, 0.0f, (float)m_NewViewport.Width, (float)m_NewViewport.Height, 0.0f, -1.0f, 1.0f);
  //m_pd3dDevice->SetTransform(D3DTS_PROJECTION, m);

  m_matView->Push();
  m_matView->LoadIdentity();
  //m_pd3dDevice->SetTransform(D3DTS_VIEW, (D3DXMATRIX*)m_matView->GetTop());
}

void CD3D9Renderer::FontRestoreRenderingState()
{
  D3DXMATRIX *m;

  assert(s_InFontState == 1);
  s_InFontState--;

  m_matProj->Pop();
  m = (D3DXMATRIX*)m_matProj->GetTop();
  //m_pd3dDevice->SetTransform(D3DTS_PROJECTION, m);

  m_matView->Pop();

  FontSetState(true);
}


// match table with the blending modes
static int nBlendMatchTable[] =
{
	D3DBLEND_ZERO,
	D3DBLEND_ONE,
	D3DBLEND_SRCCOLOR,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_SRCALPHA,
	D3DBLEND_INVSRCALPHA,
	D3DBLEND_DESTALPHA,
	D3DBLEND_INVDESTALPHA,
	D3DBLEND_DESTCOLOR,
	D3DBLEND_INVDESTCOLOR,
};

void CD3D9Renderer::FontSetBlending(int blendSrc, int blendDest)
{
	m_fontBlendMode = blendSrc|blendDest;
	
	if(eTF_G16R16F == CTexture::m_eTFZ || CTexture::m_eTFZ == eTF_R32F)
		EF_SetState(m_fontBlendMode);// | GS_NODEPTHTEST);
	else
		EF_SetState(m_fontBlendMode /*| GS_NODEPTHTEST*/ | GS_ALPHATEST_GREATER, 0);
}

void CD3D9Renderer::FontSetState(bool bRestore)
{
  static DWORD polyMode;
  static D3DCOLORVALUE color;
  static bool bMatColor;
  static int State;
  
  if (CV_d3d9_forcesoftware)
    return;

  CTexture::BindNULLFrom(1);

  // grab the modes that we might need to change
  if(!bRestore)
  {
    D3DSetCull(eCULL_None);

    State = m_RP.m_CurState;
    polyMode = m_polygon_mode;

    EF_SetVertColor();
    
    if(polyMode == R_WIREFRAME_MODE)
    {
#if defined (DIRECT3D9) || defined(OPENGL)
      m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#elif defined (DIRECT3D10)
			SStateRaster RS = gcpRendD3D->m_StatesRS[gcpRendD3D->m_nCurStateRS];
			RS.Desc.FillMode = D3D10_FILL_SOLID;
			gcpRendD3D->SetRasterState(&RS);
#endif
    }

    m_RP.m_FlagsPerFlush = 0;
		if (eTF_G16R16F == CTexture::m_eTFZ || CTexture::m_eTFZ == eTF_R32F)
			EF_SetState(m_fontBlendMode);// | GS_NODEPTHTEST);
		else
			EF_SetState(m_fontBlendMode /*| GS_NODEPTHTEST*/ | GS_ALPHATEST_GREATER, 0);

    EF_SetColorOp(eCO_REPLACE, eCO_MODULATE, eCA_Diffuse | (eCA_Diffuse<<3), DEF_TEXARG0);
		//m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		//m_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
  }
  else
  {
    if(polyMode == R_WIREFRAME_MODE)
    {
#if defined (DIRECT3D9) || defined(OPENGL)
      m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
#elif defined (DIRECT3D10)
			SStateRaster RS = gcpRendD3D->m_StatesRS[gcpRendD3D->m_nCurStateRS];
			RS.Desc.FillMode = D3D10_FILL_WIREFRAME;
			gcpRendD3D->SetRasterState(&RS);
#endif
    }
		
    //EF_SetState(State);
  }
}

