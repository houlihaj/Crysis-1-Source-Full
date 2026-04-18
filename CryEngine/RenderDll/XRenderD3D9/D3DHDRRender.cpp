/*=============================================================================
  D3DHDRRender.cpp : Direct3D specific high dynamic range post-processing
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

    Revision history:
      * Created by Honich Andrey
    
=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "../Common/PostProcess/PostEffects.h"
#include "I3DEngine.h"														// E3DPARAM_EYEADAPTIONCLAMP

using namespace NPostEffects;

//=============================================================================

//----------------------------------------------------------------------------------------
// Render Target Management functions
//   Allocation order affects performance.  These functions try to find an optimal allocation
//   order for an arbitrary number of render targets.
//-----------------------------------------------------------------------------------------

//  defer allocation of render targets until we have a list of all required targets...
//  then, sort the list to make "optimal" use of GPU memory
struct DeferredRenderTarget
{
  DWORD               dwWidth;
  DWORD               dwHeight;
  ETEX_Format         Format;
  DWORD               dwFlags;
  CTexture            **lplpStorage;
  char                szName[64];
  DWORD               dwPitch;
  float               fPriority;
};

std::vector< DeferredRenderTarget* > g_vAllRenderTargets;
#define SORT_RENDERTARGETS TRUE

struct DescendingRenderTargetSort
{
  bool operator()( DeferredRenderTarget* drtStart, DeferredRenderTarget *drtEnd ) { return (drtStart->dwPitch*drtStart->fPriority) > (drtEnd->dwPitch*drtEnd->fPriority); }
};

//  This function just clears the vector
void StartRenderTargetList( )
{
  std::vector< DeferredRenderTarget* >::iterator _it = g_vAllRenderTargets.begin();

  while ( _it != g_vAllRenderTargets.end() )
  {
    DeferredRenderTarget *drt = (DeferredRenderTarget*)*_it++;
    delete drt;
  }

  g_vAllRenderTargets.clear();
}


void SetHDRParams(CShader *pSH)
{
	Vec4 v;

	v[0] = CRenderer::CV_r_eyeadaptationfactor;
	v[1] = CRenderer::CV_r_hdrbrightoffset;
	v[2] = CRenderer::CV_r_hdrbrightthreshold;
	v[3] = CRenderer::CV_r_hdrlevel;
	static CCryName Param1Name("HDRParams0");
	static CCryName Param2Name("HDRParams1");
	pSH->FXSetPSFloat(Param1Name, &v, 1);

	v[0] = CRenderer::CV_r_eyeadaptationbase;
	v[1] = 0;
	v[2] = 0;
//	v[3] = CRenderer::CV_r_eyeadaptionclamp;				// old
//	v[3] = gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_EYEADAPTIONCLAMP);			// future
	v[3] = min(CRenderer::CV_r_eyeadaptionclamp,gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_EYEADAPTIONCLAMP));		// intermediate
	pSH->FXSetPSFloat(Param2Name, &v, 1);
}

//  Add a render target to the list
void AllocateDeferredRenderTarget( DWORD dwWidth, DWORD dwHeight, ETEX_Format Format, float fPriority, const char *szName, CTexture **pStorage, DWORD dwFlags=0 )
{
  DeferredRenderTarget *drt = new DeferredRenderTarget;
  drt->dwWidth          = dwWidth;
  drt->dwHeight         = dwHeight;
  drt->dwFlags          = FT_DONT_ANISO | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_DONT_RELEASE | FT_DONT_RESIZE | dwFlags;
  drt->Format           = Format;
  drt->fPriority        = fPriority;
  drt->lplpStorage      = pStorage;
  strcpy(drt->szName, szName);
  drt->dwPitch          = dwWidth * CTexture::ComponentsForFormat(Format);
  g_vAllRenderTargets.push_back( drt );
}

//  Now, sort and allocate all render targets
bool EndRenderTargetList( )
{
  if ( SORT_RENDERTARGETS )
    std::sort( g_vAllRenderTargets.begin(), g_vAllRenderTargets.end(), DescendingRenderTargetSort() );

  std::vector< DeferredRenderTarget* >::iterator _it = g_vAllRenderTargets.begin();
  bool bRes = true;

  while ( _it != g_vAllRenderTargets.end() )
  {
    DeferredRenderTarget *drt = (DeferredRenderTarget*)*_it++;
    CTexture *tp = CTexture::CreateRenderTarget(drt->szName, drt->dwWidth, drt->dwHeight, eTT_2D, drt->dwFlags, drt->Format, -1, (int8)(drt->fPriority*100.0f)-5);
    if (tp)
    {
      *drt->lplpStorage = tp;
      ColorF c = Col_Black;
      tp->Fill(c);
    }
    else
      bRes = false;
  }
  StartRenderTargetList( );
  return S_OK;
}

static STexState sTexStateLinear = STexState(FILTER_LINEAR, true);
static STexState sTexStateLinearWrap = STexState(FILTER_LINEAR, false);
static STexState sTexStatePoint = STexState(FILTER_POINT, true);
static int nTexStateLinear;
static int nTexStateLinearWrap;
static int nTexStatePoint;

void CTexture::DestroyHDRMaps()
{
  CD3D9Renderer *r = gcpRendD3D;
  int i;

  SAFE_RELEASE(m_Text_HDRTarget);
  SAFE_RELEASE(m_Text_HDRTargetScaled[0]);
  SAFE_RELEASE(m_Text_HDRTargetScaled[1]);
  SAFE_RELEASE(m_Text_HDRTargetScaled[2]);
  SAFE_RELEASE(m_Text_HDRBrightPass[0]);
  SAFE_RELEASE(m_Text_HDRBrightPass[1]);

  for (i=0; i<8; i++)
  {
    SAFE_RELEASE(m_Text_HDRAdaptedLuminanceCur[i]);
  }

  for(i=0; i<NUM_HDR_TONEMAP_TEXTURES; i++)
  {
    SAFE_RELEASE(m_Text_HDRToneMaps[i]);
  }

  SAFE_RELEASE(r->m_pSysLumSurface);
  CTexture::m_pCurLumTexture = NULL;
}

void CTexture::GenerateHDRMaps()
{
  int i;
  char szName[256];

  CD3D9Renderer *r = gcpRendD3D;
  r->m_dwHDRCropWidth = r->GetWidth() - r->GetWidth() % 8;
  r->m_dwHDRCropHeight = r->GetHeight() - r->GetHeight() % 8;

  DestroyHDRMaps();

  StartRenderTargetList();

  if (r->m_nHDRType > 1)
  {
    AllocateDeferredRenderTarget(r->GetWidth(), r->GetHeight(), eTF_A8R8G8B8, 1.0f, "$HDRTarget", &m_Text_HDRTarget);
    //AllocateDeferredRenderTarget(r->GetWidth(), r->GetHeight(), D3DFMT_A8R8G8B8, 1.0f, "$HDRTarget_K", &m_Text_HDRTarget_K);
    //AllocateDeferredRenderTarget(r->GetWidth(), r->GetHeight(), eTF_A8R8G8B8, 0.9f, "$HDRTarget_Temp", &m_Text_HDRTarget_Temp);
    //AllocateDeferredRenderTarget(r->GetWidth(), r->GetHeight(), eTF_A8R8G8B8, 0.8f, "$HDRScreenMap", &m_Text_ScreenMap_HDR);
  }
  else
  {
    AllocateDeferredRenderTarget(r->GetWidth(), r->GetHeight(), eTF_A16B16G16R16F, 1.0f, "$HDRTarget", &m_Text_HDRTarget, (CRenderer::CV_r_fsaa ? FT_USAGE_FSAA : 0) | FT_USAGE_PREDICATED_TILING);
    //AllocateDeferredRenderTarget(r->GetWidth(), r->GetHeight(), eTF_A16B16G16R16F, 0.8f, "$HDRScreenMap", &m_Text_ScreenMap_HDR);  
  }

  if (gcpRendD3D->m_RP.m_FSAAData.Type && CRenderer::CV_r_debug_extra_scenetarget_fsaa)
    AllocateDeferredRenderTarget(r->GetWidth(), r->GetHeight(), eTF_A16B16G16R16F, 1.0f, "$SceneTargetFSAA", &m_Text_SceneTargetFSAA, FT_USAGE_FSAA | FT_USAGE_PREDICATED_TILING);

  // Scaled versions of the HDR scene texture
  AllocateDeferredRenderTarget(r->m_dwHDRCropWidth>>1, r->m_dwHDRCropHeight>>1, eTF_A16B16G16R16F, 0.9f, "$HDRBrightPass0", &m_Text_HDRBrightPass[0]); 
  if (!r->m_bDeviceSupportsFP16Filter)
    AllocateDeferredRenderTarget(r->m_dwHDRCropWidth>>1, r->m_dwHDRCropHeight>>1, eTF_A8R8G8B8, 0.9f, "$HDRBrightPass1", &m_Text_HDRBrightPass[1]); 

  AllocateDeferredRenderTarget(r->m_dwHDRCropWidth>>1, r->m_dwHDRCropHeight>>1, eTF_A16B16G16R16F, 0.9f, "$HDRTargetScaled0", &m_Text_HDRTargetScaled[0]);    
  AllocateDeferredRenderTarget(r->m_dwHDRCropWidth>>2, r->m_dwHDRCropHeight>>2, eTF_A16B16G16R16F, 0.5f, "$HDRTargetScaled1", &m_Text_HDRTargetScaled[1]);
  AllocateDeferredRenderTarget(r->m_dwHDRCropWidth>>3, r->m_dwHDRCropHeight>>3, eTF_A16B16G16R16F, 0.4f, "$HDRTargetScaled2", &m_Text_HDRTargetScaled[2]);

  for (i=0; i<8; i++)
  {
    sprintf(szName, "$HDRAdaptedLuminanceCur_%d", i);
    AllocateDeferredRenderTarget(1, 1, r->m_HDR_FloatFormat_Scalar, 0.1f, szName, &m_Text_HDRAdaptedLuminanceCur[i]);
  }

  // For each scale stage, create a texture to hold the intermediate results
  // of the luminance calculation
  for(i=0; i<NUM_HDR_TONEMAP_TEXTURES; i++)
  {
    int iSampleLen = 1 << (2*i);
    sprintf(szName, "$HDRToneMap_%d", i);
    AllocateDeferredRenderTarget(iSampleLen, iSampleLen, r->m_HDR_FloatFormat_Scalar, 0.7f, szName, &m_Text_HDRToneMaps[i]);
  }

  EndRenderTargetList();

  HRESULT hr = S_OK;
#if defined (DIRECT3D9) || defined (OPENGL)
  if (r->m_FormatG16R16F.IsValid())
    hr = r->m_pd3dDevice->CreateOffscreenPlainSurface(64, 64, (D3DFORMAT)CTexture::DeviceFormatFromTexFormat(r->m_HDR_FloatFormat_Scalar), D3DPOOL_SYSTEMMEM, &r->m_pSysLumSurface, NULL);
  else
    hr = r->m_pd3dDevice->CreateOffscreenPlainSurface(64, 64, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &r->m_pSysLumSurface, NULL);
  //hr = r->m_pd3dDevice->ColorFill(r->m_pSysLumSurface, NULL, 0);
#elif defined (DIRECT3D10)
#endif
  r->m_fCurSceneScale = 1.0f;
  r->m_fAdaptedSceneScale = 1.0f;
  r->m_fCurLuminance = 1.0f;

  // $HDR
  //m_EnvTexTemp.Release();
  //m_EnvTexTemp.m_pTex = new SDynTexture(0, 0, eTF_A16B16G16R16F, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_DONT_COMPRESS, "$EnvCMap_Temp");
  /*for (i=0; i<MAX_ENVCUBEMAPS; i++)
  {
    m_EnvCMaps[i].Release();
    sprintf(szName, "$EnvCMap_%d", i);
    m_EnvCMaps[i].m_pTex = new SDynTexture(0, 0, eTF_A16B16G16R16F, eTT_Cube, FT_DONT_RELEASE | FT_DONT_STREAM | FT_DONT_COMPRESS, szName);
  }
  for (i=0; i<MAX_ENVTEXTURES; i++)
  {
    m_EnvTexts[i].Release();
    sprintf(szName, "$EnvTex_%d", i);
    m_EnvTexts[i].m_pTex = new SDynTexture(0, 0, eTF_A16B16G16R16F, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_DONT_COMPRESS, szName);
  }*/

  // Create resources if necessary - todo: refactor all this shared render targets stuff, quite cumbersome atm...
  SPostEffectsUtils::Create(); 
}

//=============================================================================

// Screen quad vertex format
struct ScreenVertex
{
  D3DXVECTOR4 p; // position
  D3DCOLOR color;
  D3DXVECTOR2 t; // texture coordinate
};

// log helper
#define LOG_EFFECT(msg)\
  if (gRenDev->m_LogFile)\
  {\
  gRenDev->Logv(SRendItem::m_RecurseLevel, msg);\
  }\

// ===============================================================
// SetTexture - sets texture stage

inline void SetTexture(CD3D9Renderer *pRenderer, CTexture *pTex, int iStage, int iMinFilter, int iMagFilter, bool bClamp)
{
  pRenderer->EF_SelectTMU(iStage);
  if(pTex)
  {
    STexState TS;
    TS.m_nMinFilter = iMinFilter; 
    TS.m_nMagFilter = iMagFilter;
    int nTAddr = bClamp ? TADDR_CLAMP : TADDR_WRAP;
    TS.SetClampMode(nTAddr, nTAddr, nTAddr);
    pTex->Apply(-1, CTexture::GetTexState(TS)); 
   }
  else
  {
    pRenderer->SetTexture(0);
  }
}

void DrawQuad3D(float s0, float t0, float s1, float t1)
{
  const float fZ=0.5f;
  const float fX0=-1.0f, fX1=1.0f;
#if defined(OPENGL)
  const float fY0=-1.0f, fY1=1.0f;
#else
  const float fY0=1.0f, fY1=-1.0f;
#endif
  gcpRendD3D->DrawQuad3D(Vec3(fX0,fY1,fZ), Vec3(fX1,fY1,fZ), Vec3(fX1,fY0,fZ), Vec3(fX0,fY0,fZ), Col_White, s0, t0, s1, t1);
}

PDIRECT3DSURFACE9 GetSurfaceTP(CTexture *tp)
{
  PDIRECT3DSURFACE9 pSurf = NULL;
  if (tp)
  {
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
    LPDIRECT3DTEXTURE9 pTexture = (LPDIRECT3DTEXTURE9)tp->GetDeviceTexture();
    assert(pTexture);
    HRESULT hr = pTexture->GetSurfaceLevel(0, &pSurf);
#endif
  }
  return pSurf;
}

void DrawFullScreenQuadTR(float xpos, float ypos, float w, float h)
{
  int nOffs;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)gcpRendD3D->GetVBPtr(4, nOffs);

  DWORD col = ~0;

  // Now that we have write access to the buffer, we can copy our vertices
  // into it

  const float s0 = 0;
  const float s1 = 1;
#if defined(OPENGL)
  const float t0 = 0;
  const float t1 = 1;
#else
  const float t0 = 1;
  const float t1 = 0;
#endif

  // Define the quad
  vQuad[0].xyz.x = xpos;
  vQuad[0].xyz.y = ypos;
  vQuad[0].xyz.z = 1.0f;
  vQuad[0].color.dcolor = col;
  vQuad[0].st[0] = s0;
  vQuad[0].st[1] = 1.0f-t0;

  vQuad[1].xyz.x = xpos + w;
  vQuad[1].xyz.y = ypos;
  vQuad[1].xyz.z = 1.0f;
  vQuad[1].color.dcolor = col;
  vQuad[1].st[0] = s1;
  vQuad[1].st[1] = 1.0f-t0;

  vQuad[3].xyz.x = xpos + w;
  vQuad[3].xyz.y = ypos + h;
  vQuad[3].xyz.z = 1.0f;
  vQuad[3].color.dcolor = col;
  vQuad[3].st[0] = s1;
  vQuad[3].st[1] = 1.0f-t1;

  vQuad[2].xyz.x = xpos;
  vQuad[2].xyz.y = ypos + h;
  vQuad[2].xyz.z = 1.0f;
  vQuad[2].color.dcolor = col;
  vQuad[2].st[0] = s0;
  vQuad[2].st[1] = 1.0f-t1;

  // We are finished with accessing the vertex buffer
  gcpRendD3D->UnlockVB();

  gcpRendD3D->EF_Commit();

  // Bind our vertex as the first data stream of our device
  gcpRendD3D->FX_SetVStream(0, gcpRendD3D->m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  if (!FAILED(gcpRendD3D->EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
  {
    // Render the two triangles from the data stream
  #if defined (DIRECT3D9) || defined (OPENGL)
    HRESULT hReturn = gcpRendD3D->m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, 2);
  #elif defined (DIRECT3D10)
    gcpRendD3D->SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    gcpRendD3D->m_pd3dDevice->Draw(4, nOffs);
  #endif
  }
}

#define NV_CACHE_OPTS_ENABLED TRUE

//-----------------------------------------------------------------------------
// Name: DrawFullScreenQuad
// Desc: Draw a properly aligned quad covering the entire render target
//-----------------------------------------------------------------------------
void DrawFullScreenQuad(float fLeftU, float fTopV, float fRightU, float fBottomV, bool bClampToScreenRes)
{
  D3DSURFACE_DESC dtdsdRT;
  memset(&dtdsdRT, 0, sizeof(dtdsdRT));
  PDIRECT3DSURFACE9 pSurfRT = NULL;
  CD3D9Renderer *rd = gcpRendD3D;

  rd->EF_Commit();

  // Acquire render target width and height
#if defined (DIRECT3D9) || defined (OPENGL)
  rd->m_pd3dDevice->GetRenderTarget(0, &pSurfRT);
  pSurfRT->GetDesc(&dtdsdRT);
  pSurfRT->Release();
#elif defined (DIRECT3D10)
  dtdsdRT.Width = rd->m_NewViewport.Width;
  dtdsdRT.Height = rd->m_NewViewport.Height;
#endif

  bool bNV = NV_CACHE_OPTS_ENABLED;
#if defined (XENON) || defined (DIRECT3D10)
  bNV = false;
#endif
  // Ensure that we're directly mapping texels to pixels by offset by 0.5
  // For more info see the doc page titled "Directly Mapping Texels to Pixels"
  int nWidth = dtdsdRT.Width;
  int nHeight = dtdsdRT.Height;
	if(bClampToScreenRes)
	{
		nWidth = min(nWidth, rd->GetWidth());
		nHeight = min(nHeight, rd->GetHeight());
	}

  float fWidth5 = (float)nWidth;
  float fHeight5 = (float)nHeight;
  fWidth5 = (bNV) ?  (2.f*fWidth5) - 0.5f : fWidth5 - 0.5f;
  fHeight5 = (bNV) ? (2.f*fHeight5)- 0.5f : fHeight5 - 0.5f;

  // Draw the quad
  int nOffs;
  struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *Verts = (struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *)rd->GetVBPtr(bNV?3:4, nOffs, POOL_TRP3F_COL4UB_TEX2F);

#if defined (DIRECT3D10) || defined (XENON)
  fTopV = 1 - fTopV;
  fBottomV = 1 - fBottomV;
#endif

#ifdef XENON
  struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F SysVB[4];
  struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *pDst = Verts;
  Verts = SysVB;
#endif

  Verts[0].pos = Vec4(-0.5f, -0.5f, 0.0f, 1.0f);
  Verts[0].color.dcolor = ~0;
  Verts[0].st = Vec2(fLeftU, fTopV);
  if (bNV)
  {
    float tWidth = fRightU - fLeftU;
    float tHeight = fBottomV - fTopV;
    Verts[1].pos = Vec4(fWidth5, -0.5f, 0.0f, 1.f);
    Verts[1].color.dcolor = ~0;
    Verts[1].st = Vec2(fLeftU + (tWidth*2.f), fTopV);

    Verts[2].pos = Vec4(-0.5f, fHeight5, 0.0f, 1.f);
    Verts[2].color.dcolor = ~0;
    Verts[2].st = Vec2(fLeftU, fTopV + (tHeight*2.f));
  }
  else
  {
    Verts[1].pos = Vec4(fWidth5, -0.5f, 0.0f, 1.0f);
    Verts[1].color.dcolor = ~0;
    Verts[1].st = Vec2(fRightU, fTopV);

    Verts[2].pos = Vec4(-0.5f, fHeight5, 0.0f, 1.0f);
    Verts[2].color.dcolor = ~0;
    Verts[2].st = Vec2(fLeftU, fBottomV);

    Verts[3].pos = Vec4(fWidth5, fHeight5, 0.0f, 1.0f);
    Verts[3].color.dcolor = ~0;
    Verts[3].st = Vec2(fRightU, fBottomV);
  }
#ifdef XENON
  memcpy(pDst, SysVB, sizeof(SysVB));
#endif

  rd->UnlockVB(POOL_TRP3F_COL4UB_TEX2F);

  rd->EF_SetState(GS_NODEPTHTEST);
  if (!FAILED(rd->EF_SetVertexDeclaration(0, VERTEX_FORMAT_TRP3F_COL4UB_TEX2F)))
  {
    rd->FX_SetVStream(0, rd->m_pVB[POOL_TRP3F_COL4UB_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F));
  #if defined (DIRECT3D9) || defined (OPENGL)
    rd->m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, (bNV)?1:2);
  #elif defined (DIRECT3D10)
    rd->SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    rd->m_pd3dDevice->Draw((bNV)?3:4, nOffs);
  #endif

    rd->m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL] += (bNV)?1:2;
    rd->m_RP.m_PS.m_nDIPs[EFSLIST_GENERAL]++;
  }
}

void DrawFullScreenQuad(CoordRect c, bool bClampToScreenRes)
{
  DrawFullScreenQuad( c.fLeftU, c.fTopV, c.fRightU, c.fBottomV, bClampToScreenRes );
}

//-----------------------------------------------------------------------------
// Name: GetTextureRect()
// Desc: Get the dimensions of the texture
//-----------------------------------------------------------------------------
HRESULT GetTextureRect(CTexture *pTexture, RECT* pRect)
{
  pRect->left = 0;
  pRect->top = 0;
  pRect->right = pTexture->GetWidth();
  pRect->bottom = pTexture->GetHeight();

  return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetTextureCoords()
// Desc: Get the texture coordinates to use when rendering into the destination
//       texture, given the source and destination rectangles
//-----------------------------------------------------------------------------
HRESULT GetTextureCoords(CTexture *pTexSrc, RECT* pRectSrc, CTexture *pTexDest, RECT* pRectDest, CoordRect* pCoords)
{
  float tU, tV;

  // Validate arguments
  if( pTexSrc == NULL || pTexDest == NULL || pCoords == NULL )
    return E_INVALIDARG;

  // Start with a default mapping of the complete source surface to complete 
  // destination surface
  pCoords->fLeftU = 0.0f;
  pCoords->fTopV = 0.0f;
  pCoords->fRightU = 1.0f; 
  pCoords->fBottomV = 1.0f;

  // If not using the complete source surface, adjust the coordinates
  if(pRectSrc != NULL)
  {
    // These delta values are the distance between source texel centers in 
    // texture address space
    tU = 1.0f / pTexSrc->GetWidth();
    tV = 1.0f / pTexSrc->GetHeight();

    pCoords->fLeftU += pRectSrc->left * tU;
    pCoords->fTopV += pRectSrc->top * tV;
    pCoords->fRightU -= (pTexSrc->GetWidth() - pRectSrc->right) * tU;
    pCoords->fBottomV -= (pTexSrc->GetHeight() - pRectSrc->bottom) * tV;
  }

  // If not drawing to the complete destination surface, adjust the coordinates
  if(pRectDest != NULL)
  {
    // These delta values are the distance between source texel centers in 
    // texture address space
    tU = 1.0f / pTexDest->GetWidth();
    tV = 1.0f / pTexDest->GetHeight();

    pCoords->fLeftU -= pRectDest->left * tU;
    pCoords->fTopV -= pRectDest->top * tV;
    pCoords->fRightU += (pTexDest->GetWidth() - pRectDest->right) * tU;
    pCoords->fBottomV += (pTexDest->GetHeight() - pRectDest->bottom) * tV;
  }

  return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GaussianDistribution
// Desc: Helper function for GetSampleOffsets function to compute the 
//       2 parameter Gaussian distrubution using the given standard deviation
//       rho
//-----------------------------------------------------------------------------
float GaussianDistribution( float x, float y, float rho )
{
  float g = 1.0f / sqrtf(2.0f * D3DX_PI * rho * rho);
  g *= expf( -(x*x + y*y)/(2*rho*rho) );

  return g;
}

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_GaussBlur5x5
// Desc: Get the texture coordinate offsets to be used inside the GaussBlur5x5
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_GaussBlur5x5(DWORD dwD3DTexWidth, DWORD dwD3DTexHeight, Vec4* avTexCoordOffset, Vec4* avSampleWeight, FLOAT fMultiplier)
{
  float tu = 1.0f / (float)dwD3DTexWidth ;
  float tv = 1.0f / (float)dwD3DTexHeight ;

  Vec4 vWhite( 1.0f, 1.0f, 1.0f, 1.0f );

  float totalWeight = 0.0f;
  int index=0;
  for(int x=-2; x<=2; x++)
  {
    for(int y=-2; y<=2; y++)
    {
      // Exclude pixels with a block distance greater than 2. This will
      // create a kernel which approximates a 5x5 kernel using only 13
      // sample points instead of 25; this is necessary since 2.0 shaders
      // only support 16 texture grabs.
      if( abs(x) + abs(y) > 2 )
        continue;

      // Get the unscaled Gaussian intensity for this offset
      avTexCoordOffset[index] = Vec4(x*tu, y*tv, 0, 1);
      avSampleWeight[index] = vWhite * GaussianDistribution( (float)x, (float)y, 1.0f);
      totalWeight += avSampleWeight[index].x;

      index++;
    }
  }

  // Divide the current weight by the total weight of all the samples; Gaussian
  // blur kernels add to 1.0f to ensure that the intensity of the image isn't
  // changed when the blur occurs. An optional multiplier variable is used to
  // add or remove image intensity during the blur.
  for(int i=0; i<index; i++)
  {
    avSampleWeight[i] /= totalWeight;
    avSampleWeight[i] *= fMultiplier;
  }

  return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_GaussBlur5x5Bilinear
// Desc: Get the texture coordinate offsets to be used inside the GaussBlur5x5
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_GaussBlur5x5Bilinear(DWORD dwD3DTexWidth, DWORD dwD3DTexHeight, Vec4* avTexCoordOffset, Vec4* avSampleWeight, FLOAT fMultiplier)
{
  float tu = 1.0f / (float)dwD3DTexWidth ;
  float tv = 1.0f / (float)dwD3DTexHeight ;
  float totalWeight = 0.0f;
  Vec4 vWhite( 1.f, 1.f, 1.f, 1.f );
  float fWeights[5];

  int index = 0;
  for (int x=-2; x<=2; x++, index++)
  {
    fWeights[index] = GaussianDistribution((float)x, 0.f, 4.f);
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

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_Bloom
// Desc: Get the texture coordinate offsets to be used inside the Bloom
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_Bloom(DWORD dwD3DTexSize, float afTexCoordOffset[15], Vec4* avColorWeight, float fDeviation, float fMultiplier)
{
  int i=0;
  float tu = 1.0f / (float)dwD3DTexSize;

  // Fill the center texel
  float weight = fMultiplier * GaussianDistribution(0, 0, fDeviation);
  avColorWeight[0] = Vec4(weight, weight, weight, 1.0f);

  afTexCoordOffset[0] = 0.0f;

  // Fill the first half
  for(i=1; i<8; i++)
  {
    // Get the Gaussian intensity for this offset
    weight = fMultiplier * GaussianDistribution( (float)i, 0, fDeviation );
    afTexCoordOffset[i] = i * tu;

    avColorWeight[i] = Vec4(weight, weight, weight, 1.0f);
  }

  // Mirror to the second half
  for(i=8; i<15; i++)
  {
    avColorWeight[i] = avColorWeight[i-7];
    afTexCoordOffset[i] = -afTexCoordOffset[i-7];
  }

  return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_BloomBilinear
// Desc: Get the texture coordinate offsets to be used inside the Bloom
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_BloomBilinear(DWORD dwD3DTexSize, float afTexCoordOffset[15], Vec4* avColorWeight, float fDeviation, float fMultiplier)
{
  int i=0;
  float tu = 1.0f / (float)dwD3DTexSize;

  //  store all the intermediate offsets & weights, then compute the bilinear
  //  taps in a second pass
  //  NOTE: always go left-to-right.
  float tmpWeightArray[15];
  float tmpOffsetArray[15];

  // Fill the center texel
  tmpWeightArray[7] = fMultiplier * GaussianDistribution( 0, 0, fDeviation );
  tmpOffsetArray[7] = 0.0f;

  // Fill the first half
  for(i=0; i<7; i++)
  {
    // Get the Gaussian intensity for this offset
    tmpWeightArray[i] = fMultiplier * GaussianDistribution( (float)(7-i), 0, fDeviation );
    tmpOffsetArray[i] = -(float)(7-i) * tu;
  }

  // Mirror to the second half
  for(i=8; i<15; i++)
  {
    tmpWeightArray[i] = tmpWeightArray[14-i];
    tmpOffsetArray[i] =-tmpOffsetArray[14-i];
  }

  //  now, munge these into bilinear weights

  for(i=0; i<7; i++)
  {
    float sScale = tmpWeightArray[i*2] + tmpWeightArray[i*2+1];
    float sFrac  = tmpWeightArray[i*2] / sScale;
    afTexCoordOffset[i] = tmpOffsetArray[i*2] + (1.f-sFrac)*tu;
    avColorWeight[i] = Vec4(sScale, sScale, sScale, 2.f);
  }

  afTexCoordOffset[7] = tmpOffsetArray[14];
  avColorWeight[7] = Vec4( tmpWeightArray[14], tmpWeightArray[14], tmpWeightArray[14], 1.f );

  return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_DownScale4x4
// Desc: Get the texture coordinate offsets to be used inside the DownScale4x4
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_DownScale4x4( DWORD dwWidth, DWORD dwHeight, Vec4 avSampleOffsets[])
{
  if(NULL == avSampleOffsets)
    return E_INVALIDARG;

  float tU = 1.0f / dwWidth;
  float tV = 1.0f / dwHeight;

  // Sample from the 16 surrounding points. Since the center point will be in
  // the exact center of 16 texels, a 0.5f offset is needed to specify a texel
  // center.
  int index=0;
  for(int y=0; y<4; y++)
  {
    for(int x=0; x<4; x++)
    {
      avSampleOffsets[index].x = (x - 1.5f) * tU;
      avSampleOffsets[index].y = (y - 1.5f) * tV;
      avSampleOffsets[index].z = 0;
      avSampleOffsets[index].w = 1;

      index++;
    }
  }

  return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_DownScale4x4Bilinear
// Desc: Get the texture coordinate offsets to be used inside the DownScale4x4
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_DownScale4x4Bilinear( DWORD dwWidth, DWORD dwHeight, Vec4 avSampleOffsets[])
{
  if ( NULL == avSampleOffsets )
    return E_INVALIDARG;

  float tU = 1.0f / dwWidth;
  float tV = 1.0f / dwHeight;

  // Sample from the 16 surrounding points.  Since bilinear filtering is being used, specific the coordinate
  // exactly halfway between the current texel center (k-1.5) and the neighboring texel center (k-0.5)

  int index=0;
  for(int y=0; y<4; y+=2)
  {
    for(int x=0; x<4; x+=2, index++)
    {
      avSampleOffsets[index].x = (x - 1.f) * tU;
      avSampleOffsets[index].y = (y - 1.f) * tV;
      avSampleOffsets[index].z = 0;
      avSampleOffsets[index].w = 1;
    }
  }

  return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_DownScale2x2
// Desc: Get the texture coordinate offsets to be used inside the DownScale2x2
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_DownScale2x2( DWORD dwWidth, DWORD dwHeight, Vec4 avSampleOffsets[] )
{
  if( NULL == avSampleOffsets )
    return E_INVALIDARG;

  float tU = 1.0f / dwWidth;
  float tV = 1.0f / dwHeight;

  // Sample from the 4 surrounding points. Since the center point will be in
  // the exact center of 4 texels, a 0.5f offset is needed to specify a texel
  // center.
  int index=0;
  for( int y=0; y < 2; y++ )
  {
    for( int x=0; x < 2; x++ )
    {
      avSampleOffsets[index].x = (x - 0.5f) * tU;
      avSampleOffsets[index].y = (y - 0.5f) * tV;
      avSampleOffsets[index].z = 0;
      avSampleOffsets[index].w = 1;

      index++;
    }
  }

  return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_DownScale2x2Bilinear
// Desc: Get the texture coordinate offsets to be used inside the DownScale2x2
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_DownScale2x2Bilinear( DWORD dwWidth, DWORD dwHeight, Vec4 avSampleOffsets[] )
{
  if( NULL == avSampleOffsets )
    return E_INVALIDARG;

  float tU = 1.0f / dwWidth;
  float tV = 1.0f / dwHeight;

  // Sample from the 4 surrounding points. Since the center point will be in
  // the exact center of 4 texels, a 0.5f offset is needed to specify a texel
  // center.
  int index=0;
  for(int y=0; y<2; y+=2 )
  {
    for(int x=0; x<2; x+=2)
    {
      avSampleOffsets[index].x = x * tU;
      avSampleOffsets[index].y = y * tV;
      avSampleOffsets[index].z = 0;
      avSampleOffsets[index].w = 1;

      index++;
    }
  }

  return S_OK;
}

LPDIRECT3DSURFACE9 GetTextureSurface(const CTexture *pTex, int iLevel=0)
{  
  LPDIRECT3DTEXTURE9 pTexture = (LPDIRECT3DTEXTURE9)pTex->GetDeviceTexture();
  assert(pTexture);

  LPDIRECT3DSURFACE9 pSurf = 0;
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
  HRESULT hr = pTexture->GetSurfaceLevel(iLevel, &pSurf);
#endif
  return pSurf;
}

void StretchRect(CTexture *pSrc, CTexture *pDst) 
{
  if(!pSrc || !pDst) 
  {
    return;
  }

  // Get current viewport
  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
  bool bResample=0;

  if(pSrc->GetWidth()!=pDst->GetWidth() && pSrc->GetHeight()!=pDst->GetHeight())
  {
    bResample = 1;
  }

  gcpRendD3D->FX_SetRenderTarget(0, pDst, &gcpRendD3D->m_DepthBufferOrig); 

  if(!bResample)
  {
    static CCryName TechName("TextureToTexture");
    SPostEffectsUtils::ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  }
  else
  {      
    static CCryName TechName("TextureToTextureResampled");
    SPostEffectsUtils::ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  }

  gRenDev->EF_SetState(GS_NODEPTHTEST);   

  // Get sample size ratio (based on empirical "best look" approach)
  float fSampleSize = ((float)pSrc->GetWidth()/(float)pDst->GetWidth()) * 0.5f;

  // Set samples position
  float s1 = fSampleSize / (float) pSrc->GetWidth();  // 2.0 better results on lower res images resizing        
  float t1 = fSampleSize / (float) pSrc->GetHeight();       

  // Use rotated grid
  Vec4 pParams0=Vec4(s1*0.95f, t1*0.25f, -s1*0.25f, t1*0.96f); 
  Vec4 pParams1=Vec4(-s1*0.96f, -t1*0.25f, s1*0.25f, -t1*0.96f);  

  static CCryName Param1Name("texToTexParams0");
  static CCryName Param2Name("texToTexParams1");
  CShaderMan::m_shPostEffects->FXSetVSFloat(Param1Name, &pParams0, 1);        
  CShaderMan::m_shPostEffects->FXSetVSFloat(Param2Name, &pParams1, 1); 


  CTexture::BindNULLFrom(0);// workaround for dx10 skipping setting correct samplers
  SPostEffectsUtils::SetTexture(pSrc, 0, FILTER_LINEAR);    
  

  SPostEffectsUtils::DrawFullScreenQuad(pDst->GetWidth(), pDst->GetHeight());

  SPostEffectsUtils::ShEndPass();

  // Restore previous viewport
}

void StretchRectAO(CTexture *pSrc, CTexture *pDst, bool bFirst, bool b3x3) 
{
  if(!pSrc || !pDst) 
  {
    return;
  }

  // Get current viewport
  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
  bool bResample=0;

  if(pSrc->GetWidth()!=pDst->GetWidth() && pSrc->GetHeight()!=pDst->GetHeight())
  {
    bResample = 1;
  }

  gcpRendD3D->FX_PushRenderTarget(0, pDst, &gcpRendD3D->m_DepthBufferOrig); 

  if(!bResample)
  {
    static CCryName TechName("TextureToTexture");
    SPostEffectsUtils::ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  }
  else if(!b3x3)
  {
    static CCryName TechName("TextureToTextureResampledAO");
    SPostEffectsUtils::ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  }
  else if(bFirst)
  {      
    static CCryName TechName("TextureToTextureResampledAO_3x3_First");
    SPostEffectsUtils::ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  }
  else
  {      
    static CCryName TechName("TextureToTextureResampledAO_3x3_Second");
    SPostEffectsUtils::ShBeginPass(CShaderMan::m_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  }

  gRenDev->EF_SetState(GS_NODEPTHTEST);   

  // Get sample size ratio (based on empirical "best look" approach)
  float fSampleSize = ((float)pSrc->GetWidth()/(float)pDst->GetWidth()) * 0.5f;

  // Set samples position
  float s1 = fSampleSize / (float) pSrc->GetWidth();  // 2.0 better results on lower res images resizing        
  float t1 = fSampleSize / (float) pSrc->GetHeight();       

  // Use rotated grid
  Vec4 pParams0=Vec4(s1*0.95f, t1*0.25f, -s1*0.25f, t1*0.96f); 
  Vec4 pParams1=Vec4(-s1*0.96f, -t1*0.25f, s1*0.25f, -t1*0.96f);  

  static CCryName Param1Name("texToTexParams0");
  static CCryName Param2Name("texToTexParams1");
  CShaderMan::m_shPostEffects->FXSetVSFloat(Param1Name, &pParams0, 1);        
  CShaderMan::m_shPostEffects->FXSetVSFloat(Param2Name, &pParams1, 1); 


  CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers
  SPostEffectsUtils::SetTexture(pSrc, 0, FILTER_LINEAR);    
  

  SPostEffectsUtils::DrawFullScreenQuad(pDst->GetWidth(), pDst->GetHeight());

  SPostEffectsUtils::ShEndPass();

  // Restore previous viewport
  gcpRendD3D->FX_PopRenderTarget(0);
  gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);        
}

void TexBlurGaussian(CTexture *pTex, int nAmount, float fScale, float fDistribution, bool bEncoded = true)
{
  if(!pTex)
  {
    return;
  }
  
  // "ping-pong" temporary buffer
  SDynTexture BlurTemp(pTex->GetWidth(), pTex->GetHeight(), pTex->GetDstFormat(), eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP | FT_USAGE_RENDERTARGET , "TempGaussBlurRT");
  
  // Get current viewport
  int iTempX, iTempY, iWidth, iHeight;
  gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
  gRenDev->SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());        

  Vec4 vWhite( 1.0f, 1.0f, 1.0f, 1.0f );

  static CCryName TechName("GaussBlurBilinear");
  static CCryName TechName2("GaussBlurBilinearEncoded");

  uint nPasses;
  CShaderMan::m_shPostEffects->FXSetTechnique((gcpRendD3D->m_bDeviceSupportsFP16Filter || !bEncoded)? TechName:TechName2 );
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

  int s;
  for(s = 0; s<nSamples; ++s)
  {
    pWeights[s] = SPostEffectsUtils::GaussianDistribution1D(s - nHalfSamples, fDistribution);      
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
    float offset = off_b / (off_a + off_b);

    pWeights[s] = off_a + off_b;
    pWeights[s] *= fScale ;
    pWeightsPS[s] = vWhite * pWeights[s];

    float fCurrOffset = (float) s*2 + offset - nHalfSamples;
    pHParams[s] = Vec4(s1 * fCurrOffset , 0, 0, 0);  
    pVParams[s] = Vec4(0, t1 * fCurrOffset , 0, 0);       
  }
    
  static CCryName Param1Name("psWeights");
  static CCryName Param2Name("PI_psOffsets");
  CShaderMan::m_shPostEffects->FXSetPSFloat(Param1Name, pWeightsPS, nHalfSamples);  
  for(int p(1); p<= nAmount; ++p)   
  {
    // Horizontal pass      
    BlurTemp.SetRT(0, false, &gcpRendD3D->m_DepthBufferOrig);              

    // !force updating constants per-pass! (dx10..)
    CShaderMan::m_shPostEffects->FXBeginPass(0);

    CTexture::BindNULLFrom(0);// workaround for dx10 skipping setting correct samplers
    pTex->Apply(0, nTexStateLinear); 

    CShaderMan::m_shPostEffects->FXSetVSFloat(Param2Name, pHParams, nHalfSamples);                
    SPostEffectsUtils::DrawFullScreenQuad(pTex->GetWidth(), pTex->GetHeight());

    CShaderMan::m_shPostEffects->FXEndPass();

    // Vertical pass      
    gcpRendD3D->FX_SetRenderTarget(0, pTex, &gcpRendD3D->m_DepthBufferOrig);
    // !force updating constants per-pass! (dx10..)
    CShaderMan::m_shPostEffects->FXBeginPass(0);

    CTexture::BindNULLFrom(0);// workaround for dx10 skipping setting correct samplers
    BlurTemp.Apply(0, nTexStateLinear); 

    CShaderMan::m_shPostEffects->FXSetVSFloat(Param2Name, pVParams, nHalfSamples);             
    SPostEffectsUtils::DrawFullScreenQuad(pTex->GetWidth(), pTex->GetHeight());      

    CShaderMan::m_shPostEffects->FXEndPass();
  }             

  CShaderMan::m_shPostEffects->FXEnd(); 
  //SPostEffectsUtils::ShEndPass();

  // Restore previous viewport
  gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);
}

void HDR_EncodeToLDR( CTexture *pHdrSrc, CTexture *pLdrDst ) 
{
  uint nPasses;

  gcpRendD3D->FX_SetRenderTarget(0, pLdrDst, &gcpRendD3D->m_DepthBufferOrig);

  // Get current viewport
  int iTempX, iTempY, iWidth, iHeight;
  gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
  gRenDev->SetViewport(0, 0, pLdrDst->GetWidth(), pLdrDst->GetHeight());  


  static CCryName TechName("EncodeHDRtoLDR");
  CShaderMan::m_shPostEffects->FXSetTechnique(TechName);
  CShaderMan::m_shPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  CShaderMan::m_shPostEffects->FXBeginPass(0);

  CTexture::BindNULLFrom(0);// workaround for dx10 skipping setting correct samplers
  pHdrSrc->Apply(0, CTexture::GetTexState(sTexStateLinear)); 
  

  SPostEffectsUtils::DrawFullScreenQuad( pHdrSrc->GetWidth(), pHdrSrc->GetHeight() );

  CShaderMan::m_shPostEffects->FXEndPass();
  CShaderMan::m_shPostEffects->FXEnd();        

  // Restore previous viewport
  gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);
}

void HDR_SceneToSceneScaled()
{  
	gcpRendD3D->Set2DMode(true, 1, 1);   
	StretchRect( CTexture::m_Text_HDRTarget, CTexture::m_Text_HDRTargetScaled[0]);
	gcpRendD3D->Set2DMode(false, 1, 1);      
}

bool HDR_SceneToSceneScaled(CShader *pSH)
{  
  HRESULT hr = S_OK;
  Vec4 avSampleOffsets[16];
  CD3D9Renderer *rd = gcpRendD3D;

  // Place the rectangle in the center of the back buffer surface
  RECT rectSrc;
  rectSrc.left = (rd->GetWidth() - rd->m_dwHDRCropWidth) / 2;
  rectSrc.top = (rd->GetHeight() - rd->m_dwHDRCropHeight) / 2;
  rectSrc.right = rectSrc.left + rd->m_dwHDRCropWidth;
  rectSrc.bottom = rectSrc.top + rd->m_dwHDRCropHeight;

  // Get the texture coordinates for the render target
  CoordRect coords;
  GetTextureCoords(CTexture::m_Text_HDRTarget, &rectSrc, CTexture::m_Text_HDRTargetScaled[0], NULL, &coords);

  uint nPasses;
  static CCryName TechName("HDRDownScale4x4MRTSafe");
  pSH->FXSetTechnique(TechName);
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

  int nBitPlanes = rd->m_bDeviceSupportsFP16Separate ? 2 : 1;
  for (int i=0; i<nBitPlanes; i++)
  {
    if (i)
      rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
    else
      rd->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE1];

    rd->FX_SetRenderTarget(0, CTexture::m_Text_HDRTargetScaled[i], &gcpRendD3D->m_DepthBufferOrig);
    rd->EF_ClearBuffers(FRT_CLEAR, NULL);
    //rd->SetViewport(0, 0, CTexture::m_Text_HDRTargetScaled[i]->GetWidth(), CTexture::m_Text_HDRTargetScaled[i]->GetHeight());

    pSH->FXBeginPass(0);

    if (!i)
    {
      // Get the sample offsets used within the pixel shader
      static CCryName Param1Name("SampleOffsets");
      if (rd->m_bDeviceSupportsFP16Filter || rd->m_nHDRType > 1)
      {
        GetSampleOffsets_DownScale4x4Bilinear(rd->GetWidth(), rd->GetHeight(), avSampleOffsets);
        pSH->FXSetPSFloat(Param1Name, avSampleOffsets, 4);
        CTexture::m_Text_HDRTarget->Apply(0, nTexStateLinear);
        if (rd->m_nHDRType > 1)
        {
          assert(0);
          /*CTexture::m_Text_HDRTarget_Temp->Apply(1, nTexStateLinear);
          if (rd->m_RP.m_PersFlags2 & RBPF2_HDR_MRT_WASALPHABLEND)
          {
            CTexture::m_Text_SceneTarget->Apply(2, nTexStateLinear);
            CTexture::m_Text_SceneTarget_HDR->Apply(3, nTexStateLinear);
          }*/
        }
      }
      else
      {
        GetSampleOffsets_DownScale4x4(rd->GetWidth(), rd->GetHeight(), avSampleOffsets);
        pSH->FXSetPSFloat(Param1Name, avSampleOffsets, 16);
        CTexture::m_Text_HDRTarget->Apply(0, nTexStatePoint);
      }
    }

    // Draw a fullscreen quad
    DrawFullScreenQuad(coords);
    //DrawQuad3D(coords.fLeftU, coords.fBottomV, coords.fRightU, coords.fTopV);

    pSH->FXEndPass();
  }

  pSH->FXEnd();

  return true;
}

bool HDR_SysCalculateGamma()
{
  CD3D9Renderer *rd = gcpRendD3D;
  SHistogram *pHG = &rd->m_LumHistogram;
  SGamma *pGR = &rd->m_LumGamma;
  float Dt = iTimer->GetFrameTime();

	float fAdaptionSpeedRatio = 1.0f - powf(0.01f*CLAMP(100.0f-CRenderer::CV_r_eyeadaptionspeed,0,100),Dt);


  // Update the current min, max, and average over time
  pGR->LowPercentil = pGR->LowPercentil + (pHG->LowPercentil - pGR->LowPercentil) * fAdaptionSpeedRatio;
  pGR->MidPercentil = pGR->MidPercentil + (pHG->MidPercentil - pGR->MidPercentil) * fAdaptionSpeedRatio;
  pGR->HighPercentil = pGR->HighPercentil + (pHG->HighPercentil - pGR->HighPercentil) * fAdaptionSpeedRatio;
  pGR->CurLuminance = pGR->CurLuminance + (rd->m_fCurLuminance - pGR->CurLuminance) * fAdaptionSpeedRatio;

  if (!_finite(pGR->CurLuminance))
    pGR->CurLuminance = 1.0f;

	// limit eye adaption by console var
	pGR->LowPercentil = CLAMP(pGR->LowPercentil,0,CRenderer::CV_r_eyeadaptionmax);
	pGR->MidPercentil = CLAMP(pGR->MidPercentil,pGR->LowPercentil,CRenderer::CV_r_eyeadaptionmax);
	pGR->HighPercentil = CLAMP(pGR->HighPercentil,CRenderer::CV_r_eyeadaptionmin,CRenderer::CV_r_eyeadaptionmax);
	
	if(CRenderer::CV_r_hdrhistogram==2)
		pGR->LowPercentil=0;
	if(CRenderer::CV_r_hdrhistogram==3)
	{
		pGR->LowPercentil=0;
	  pGR->MidPercentil=0.5*pGR->HighPercentil;
	}

	// limit low and mid within reasonable limits
	pGR->LowPercentil = CLAMP(pGR->LowPercentil,0.0f,pGR->HighPercentil*0.2f);
	pGR->MidPercentil = CLAMP(pGR->MidPercentil,pGR->HighPercentil*0.2,pGR->HighPercentil*0.8f);

  // Bias is used to slightly saturate the visible color values
  // to keep them from appearing washed out
  const float BiasMask = 0.5f;
  float Diff = (pGR->MidPercentil-pGR->LowPercentil) / (pGR->HighPercentil-pGR->LowPercentil);
  float Bias = 1.0f + Diff * BiasMask;
  pGR->Bias = Bias;

  rd->m_fCurSceneScale = 1.0f - Diff;
  //rd->m_fCurSceneScale = 0.1f;
  rd->m_fAdaptedSceneScale = 1.0f; //rd->m_fAdaptedSceneScale + (rd->m_fCurSceneScale - rd->m_fAdaptedSceneScale) * (1.0f - powf(0.98f, 60.0f*Dt));

  // Discrette min and max
  /*byte Min = (byte)(pGR->Min*255.0f);
  byte Max = (byte)(pGR->Max*255.0f);

  // Set the gamma ramp based on the new min, max and bias.
  // All values less that min are set to 0
  // All values between min and max set along a curve determined by the bias value
  // All values greater that max are set to 255
  byte c = 0;
  for (; c<Min; c++)
  {
    pGR->Ramp[c] = 0;
  }
  for (; c<Max; c++)
  {
    pGR->Ramp[c] = (byte)(powf((float)(c-Min) / (Max-Min), Bias) * 255.0f);
  }
  for (; c<255; c++)
  {
    pGR->Ramp[c] = 255;
  }*/

  /*ushort GammaTable[256];
  for (int i=0; i<256; i++)
  {
    GammaTable[i] = (pGR->Ramp[i] << 8);
  }
  rd->SetDeviceGamma(GammaTable, GammaTable, GammaTable);
*/

  return true;
}

bool HDR_MeasureLuminance(CShader *pSH);

bool HDR_SysCalculateHistogram(CShader *pSH,CTexture *pTex)
{
#if defined (DIRECT3D9) || defined (OPENGL)
  HRESULT hr = S_OK;
  CD3D9Renderer *rd = gcpRendD3D;
  float fDt = iTimer->GetFrameTime();
  if (rd->m_pHDRLumQuery)
  {
    BOOL bQuery = false;
    hr = rd->m_pHDRLumQuery->GetData((void *)&bQuery, sizeof(BOOL), 0);
    if (hr == S_FALSE)
      return true;
  }

  int i, j;
  LPDIRECT3DSURFACE9 pSurf = NULL;
  D3DLOCKED_RECT d3dlrSys;
  LPDIRECT3DTEXTURE9 pID3DTexture = (LPDIRECT3DTEXTURE9)pTex->GetDeviceTexture();
  int nX = pTex->GetWidth();
  int nY = pTex->GetHeight();

  float *pPixels = (float *)malloc(nX*nY*4);
  // Calculate average luminance
  hr = rd->m_pSysLumSurface->LockRect(&d3dlrSys, NULL, 0);
  byte *src = (byte *)d3dlrSys.pBits;
  float fLum = 0;
  float fMax = -999999.0f;
  if (rd->m_FormatG16R16F.IsValid())
  {
    for (i=0; i<nY; i++)
    {
      float *fsrc = (float *)src;
      float *dst = &pPixels[i*nX];
      for (j=0; j<nX; j++)
      {
        float fVal = fsrc[j]*CRenderer::CV_r_eyeadaptionscale+CRenderer::CV_r_eyeadaptionbase;
				if(fVal>8.0)fVal=8.0f;
        if (fMax < fVal) fMax = fVal;
        fLum += fVal;
        dst[j] = fVal;
      }
      src += d3dlrSys.Pitch;
    }
  }
  else
  {
    for (i=0; i<nY; i++)
    {
      byte *fsrc = (byte *)src;
      float *dst = &pPixels[i*nX];
      for (j=0; j<nX; j++)
      {
        float fVal = fsrc[j*4] / 255.0f;
				if(fVal>8.0f/255.0f)fVal=8.0f/255.0f;
        if (fMax < fVal) fMax = fVal;
        fLum += fVal;
        dst[j] = fVal;
      }
      src += d3dlrSys.Pitch;
    }
  }
  hr = rd->m_pSysLumSurface->UnlockRect();
  fLum = fLum/(nX * nY);
  rd->m_fCurLuminance = fLum;

  // Calculate Histogramm

  // Build the normalized histogram by summing the
  // fraction of pixels per luminance value
  SHistogram *pHG = &rd->m_LumHistogram;
  memset(&pHG->Lum, 0, sizeof(pHG->Lum));
  pHG->MaxVal = fMax;
  int nNumPixels = nX*nY;
  float fInvNumPixels = 1.0f / (float)(nNumPixels);

	for (i=0; i<nNumPixels; i++)
    pHG->Lum[(byte)(pPixels[i]*255.0f/fMax)] += fInvNumPixels;

  // Find the histogram min, max, and average
	{
		float fSum = 0.0f;

		int iLow=0,iMed=128,iHigh=255;

		for(i=0;i<255;i++)
		{
			fSum += pHG->Lum[i];

			     if (fSum <= g_fEyeAdaptionLowerPercent)		iLow = i;
			else if (fSum <= g_fEyeAdaptionMidPercent)			iMed = i;
			else if (fSum <= g_fEyeAdaptionUpperPercent)		iHigh = i;
			else break;
		}

		pHG->LowPercentil		= iLow*pHG->MaxVal/255.0f;
		pHG->MidPercentil		= iMed*pHG->MaxVal/255.0f;
		pHG->HighPercentil	= iHigh*pHG->MaxVal/255.0f;
	}

  // Preparing for system lock
  hr = pID3DTexture->GetSurfaceLevel(0, &pSurf);
  hr = rd->m_pd3dDevice->GetRenderTargetData(pSurf, rd->m_pSysLumSurface);
  SAFE_RELEASE(pSurf);

  SAFE_DELETE_ARRAY(pPixels);

	if (rd->m_pHDRLumQuery)
    rd->m_pHDRLumQuery->Issue(D3DISSUE_END);
#elif defined (DIRECT3D10)
  assert(0);
#endif
  return true;
}

bool HDR_MeasureLuminance(CShader *pSH)
{
  HRESULT hr = S_OK;
  int x, y, index;
  Vec4 avSampleOffsets[16];
  CD3D9Renderer *rd = gcpRendD3D;
  bool bResult = false;

  DWORD dwCurTexture = NUM_HDR_TONEMAP_TEXTURES-1;
  static CCryName Param1Name("SampleOffsets");

  // Initialize the sample offsets for the initial luminance pass.
  float tU, tV;
  tU = 1.0f / (3.0f * CTexture::m_Text_HDRToneMaps[dwCurTexture]->GetWidth());
  tV = 1.0f / (3.0f * CTexture::m_Text_HDRToneMaps[dwCurTexture]->GetHeight());

  index=0;
  for(x=-1; x<=1; x++)
  {
    for(y=-1; y<=1; y++)
    {
      avSampleOffsets[index].x = x * tU;
      avSampleOffsets[index].y = y * tV;
      avSampleOffsets[index].z = 0;
      avSampleOffsets[index].w = 1;

      index++;
    }
  }

  uint nPasses;
	static ICVar *e_debug_mask = iConsole->GetCVar( "e_debug_mask" );	
	assert(e_debug_mask);

	if(e_debug_mask->GetIVal()&0x8)
  {
    static CCryName TechName("HDRCutPercentileInitial");
		pSH->FXSetTechnique(TechName);
  }
	else
  {
    static CCryName TechName("HDRSampleLumInitial");
	  pSH->FXSetTechnique(TechName);
  }
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  rd->FX_SetRenderTarget(0, CTexture::m_Text_HDRToneMaps[dwCurTexture], &gcpRendD3D->m_DepthBufferOrig);
  CTexture::m_Text_HDRTargetScaled[0]->Apply(0, nTexStatePoint);
  if (rd->m_bDeviceSupportsFP16Separate)
    CTexture::m_Text_HDRTargetScaled[1]->Apply(1, nTexStatePoint);
	if(CTexture::m_pCurLumTexture)
		CTexture::m_pCurLumTexture->Apply(2,nTexStateLinear);

	CTexture::m_Text_ZTarget->Apply(3,nTexStateLinear);		// sceneMap3 scene depth

	pSH->FXBeginPass(0);

  pSH->FXSetPSFloat(Param1Name, avSampleOffsets, 9);

	SetHDRParams(pSH);

  // Draw a fullscreen quad to sample the RT
  DrawFullScreenQuad(0.0f, 0.0f, 1.0f, 1.0f);
  //DrawQuad3D(0.0f, 0.0f, 1.0f, 1.0f);

  pSH->FXEndPass();
  
  if (rd->m_bDeviceSupportsFP16Separate)
  {
    rd->EF_SelectTMU(1);
    rd->EnableTMU(false);
    rd->EF_SelectTMU(0);
  }

  dwCurTexture--;

  // Initialize the sample offsets for the iterative luminance passes
  while(dwCurTexture > 0)
  {
    rd->FX_SetRenderTarget(0, CTexture::m_Text_HDRToneMaps[dwCurTexture], &gcpRendD3D->m_DepthBufferOrig);

		if(e_debug_mask->GetIVal()&0x8)
    {
      static CCryName TechName("HDRCutPercentileIterative");
			pSH->FXSetTechnique(TechName);
    }
		else
    {
      static CCryName TechName("HDRSampleLumIterative");
	    pSH->FXSetTechnique(TechName);
    }
    pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
    pSH->FXBeginPass(0);
    if (rd->m_bDeviceSupportsFP16Filter)
    {
      GetSampleOffsets_DownScale4x4Bilinear(CTexture::m_Text_HDRToneMaps[dwCurTexture+1]->GetWidth(), CTexture::m_Text_HDRToneMaps[dwCurTexture+1]->GetHeight(), avSampleOffsets);
      pSH->FXSetPSFloat(Param1Name, avSampleOffsets, 4);
      CTexture::m_Text_HDRToneMaps[dwCurTexture+1]->Apply(0, nTexStateLinear);
    }
    else
    {
      GetSampleOffsets_DownScale4x4(CTexture::m_Text_HDRToneMaps[dwCurTexture+1]->GetWidth(), CTexture::m_Text_HDRToneMaps[dwCurTexture+1]->GetHeight(), avSampleOffsets);

      pSH->FXSetPSFloat(Param1Name, avSampleOffsets, 16);
      CTexture::m_Text_HDRToneMaps[dwCurTexture+1]->Apply(0, nTexStatePoint);
    }

    // Draw a fullscreen quad to sample the RT
    DrawFullScreenQuad(0.0f, 0.0f, 1.0f, 1.0f);
    //DrawQuad3D(0.0f, 0.0f, 1.0f, 1.0f);

    pSH->FXEndPass();

    dwCurTexture--;
  }

	if(e_debug_mask->GetIVal()&0x8)
  {
    static CCryName TechName("HDRCutPercentileFinal");
		pSH->FXSetTechnique(TechName);
  }
	else
  {
    static CCryName TechName("HDRSampleLumFinal");
		pSH->FXSetTechnique(TechName);
  }
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

  rd->FX_SetRenderTarget(0, CTexture::m_Text_HDRToneMaps[0], &gcpRendD3D->m_DepthBufferOrig);

  pSH->FXBeginPass(0);

  if (rd->m_bDeviceSupportsFP16Filter)
  {
    GetSampleOffsets_DownScale4x4Bilinear(CTexture::m_Text_HDRToneMaps[1]->GetWidth(), CTexture::m_Text_HDRToneMaps[1]->GetHeight(), avSampleOffsets);
    pSH->FXSetPSFloat(Param1Name, avSampleOffsets, 4);
    CTexture::m_Text_HDRToneMaps[1]->Apply(0, nTexStateLinear);
  }
  else
  {
    GetSampleOffsets_DownScale4x4(CTexture::m_Text_HDRToneMaps[1]->GetWidth(), CTexture::m_Text_HDRToneMaps[1]->GetHeight(), avSampleOffsets);
    pSH->FXSetPSFloat(Param1Name, avSampleOffsets, 16);
    CTexture::m_Text_HDRToneMaps[1]->Apply(0, nTexStatePoint);
  }

  // Draw a fullscreen quad to sample the RT
  DrawFullScreenQuad(0.0f, 0.0f, 1.0f, 1.0f);
  //DrawQuad3D(0.0f, 1.0f, 1.0f, 0.0f);

  pSH->FXEndPass();

  return bResult;
}

//-----------------------------------------------------------------------------
// Name: HDR_CalculateAdaptation()
// Desc: Increment the user's adapted luminance
//-----------------------------------------------------------------------------
bool HDR_CalculateAdaptation(CShader *pSH)
{
  HRESULT hr = S_OK;
  CD3D9Renderer *rd = gcpRendD3D;
  bool bResult = false;

  // Swap current & last luminance
  int nCurLum = CTexture::m_nCurLumTexture++;
  nCurLum &= 7;
  CTexture *pTexPrev = CTexture::m_Text_HDRAdaptedLuminanceCur[(nCurLum-1)&7];
  CTexture *pTexCur = CTexture::m_Text_HDRAdaptedLuminanceCur[nCurLum];
  CTexture::m_pCurLumTexture = pTexCur;
  assert(pTexCur);

  uint nPasses;
  static CCryName TechName("HDRCalculateAdaptedLum");
  pSH->FXSetTechnique(TechName);
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

  rd->FX_SetRenderTarget(0, pTexCur, &gcpRendD3D->m_DepthBufferOrig);

  pSH->FXBeginPass(0);

  Vec4 v;
  v[0] = iTimer->GetFrameTime();
  v[1] = v[2] = v[3] = 0;
  static CCryName Param1Name("ElapsedTime");
  pSH->FXSetPSFloat(Param1Name, &v, 1);
  pTexPrev->Apply(0, nTexStatePoint);
  CTexture::m_Text_HDRToneMaps[0]->Apply(1, nTexStatePoint);

  // Draw a fullscreen quad to sample the RT
  DrawFullScreenQuad(0.0f, 0.0f, 1.0f, 1.0f);
  //DrawQuad3D(0.0f, 1.0f, 1.0f, 0.0f);

  pSH->FXEndPass();

  bResult = true;

  rd->EF_SelectTMU(1);
  rd->EnableTMU(false);
  rd->EF_SelectTMU(0);

  /*if (!rd->m_pQueryLum)
  {
    hr = rd->m_pd3dDevice->CreateQuery(D3DQUERYTYPE_EVENT, &rd->m_pQueryLum);
    assert(rd->m_pQueryLum);
    rd->m_pQueryLum->Issue(D3DISSUE_END);
    CTexture::m_pIssueLumTexture = pTexCur;
  }*/
  if (!CTexture::m_pIssueLumTexture && nCurLum >= 7)
    CTexture::m_pIssueLumTexture = pTexCur;
  if (CTexture::m_pIssueLumTexture)
  {
    CTexture::m_pIssueLumTexture = CTexture::m_Text_HDRAdaptedLuminanceCur[(nCurLum-7)&7];
    //BOOL bQuery = false;
    //hr = rd->m_pQueryLum->GetData((void *)&bQuery, sizeof(BOOL), 0);
    //if (bQuery)
    /*{
      assert(hr == S_OK);
      D3DLOCKED_RECT d3dlrSys;
      LPDIRECT3DSURFACE9 pSurf, pSysSurf;
      CTexture *pTex = CTexture::m_pIssueLumTexture;
      LPDIRECT3DTEXTURE9 pID3DTexture = (LPDIRECT3DTEXTURE9)pTex->GetDeviceTexture();
      int nX = pTex->GetWidth();
      int nY = pTex->GetHeight();
      hr = pID3DTexture->GetSurfaceLevel(0, &pSurf);
      hr = rd->m_pd3dDevice->CreateRenderTarget(nX, nY, (D3DFORMAT)CTexture::DeviceFormatFromTexFormat(rd->m_HDR_FloatFormat_Scalar), D3DMULTISAMPLE_NONE, 0, TRUE, &pSysSurf, NULL);
      hr = rd->m_pd3dDevice->StretchRect(pSurf, NULL, pSysSurf, NULL, D3DTEXF_NONE);
      hr = pSysSurf->LockRect(&d3dlrSys, NULL, 0);
      float *src = (float *)d3dlrSys.pBits;
      float nLum = 0;
      for (int i=0; i<nY*nX; i++)
      {
        nLum += src[0];
        src++;
      }
      nLum /= nX * nY;
      rd->m_CurLuminance = (int)(nLum * 255);
      SAFE_RELEASE(pSysSurf);
      SAFE_RELEASE(pSurf);
      //rd->m_pQueryLum->Issue(D3DISSUE_END);
      //CTexture::m_pIssueLumTexture = pTexCur;
    }*/
  }


  return bResult;
}



//-----------------------------------------------------------------------------
// Name: SceneScaled_To_BrightPass
// Desc: Run the bright-pass filter on CTexman::m_Text_HDRTargetScaled and place the result
//       in CTexMan::m_Text_HDRBrightPass[0]
//-----------------------------------------------------------------------------

bool HDR_SceneScaledToBrightPass(CShader *pSH)
{
  HRESULT hr = S_OK;
  CD3D9Renderer *rd = gcpRendD3D;
  bool bResult = false;
  uint nPasses;

  CTexture *pShaftsRT = CTexture::m_Text_Black;

  if( rd->m_RP.m_PersFlags2 & RBPF2_LIGHTSHAFTS )
  {    
    gcpRendD3D->Set2DMode(true, 1, 1);      
    
    pShaftsRT = CTexture::m_Text_HDRTargetScaled[2];
    
    // If device doens't support f16 filtering, encode into ldr format
    if( !gcpRendD3D->m_bDeviceSupportsFP16Filter ) 
    {      
      HDR_EncodeToLDR( pShaftsRT, CTexture::m_Text_BackBufferScaled[2] );
      pShaftsRT = CTexture::m_Text_BackBufferScaled[2];
    }

    TexBlurGaussian( pShaftsRT, 1, 1.0, 1.0f);

    gcpRendD3D->Set2DMode(false, 1, 1);      
  }

  
  // Get the rectangle describing the sampled portion of the source texture.
  // Decrease the rectangle to adjust for the single pixel black border.
  RECT rectSrc;
  GetTextureRect(CTexture::m_Text_HDRTargetScaled[0], &rectSrc);
  InflateRect(&rectSrc, -1, -1);

  // Get the destination rectangle.
  // Decrease the rectangle to adjust for the single pixel black border.
  RECT rectDest;
  GetTextureRect(CTexture::m_Text_HDRBrightPass[0], &rectDest);
  InflateRect(&rectDest, -1, -1);

  // Get the correct texture coordinates to apply to the rendered quad in order 
  // to sample from the source rectangle and render into the destination rectangle
  CoordRect coords;
  GetTextureCoords(CTexture::m_Text_HDRTargetScaled[0], &rectSrc, CTexture::m_Text_HDRBrightPass[0], &rectDest, &coords);
  
  static CCryName TechName("HDRBrightPassFilter");
  pSH->FXSetTechnique(TechName);
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

  rd->FX_SetRenderTarget(0, CTexture::m_Text_HDRBrightPass[0], &gcpRendD3D->m_DepthBufferOrig);
  pSH->FXBeginPass(0);
  if (CRenderer::CV_r_hdrhistogram)
  {
	  Vec4 v;
    v[0] = rd->m_LumGamma.CurLuminance;
    static CCryName Param1Name("HDRLuminance");
    pSH->FXSetPSFloat(Param1Name, &v, 1);
  }
	SetHDRParams(pSH);

  if (rd->m_bDeviceSupportsFP16Separate)
  {
    CTexture::m_Text_HDRTargetScaled[0]->Apply(0, nTexStatePoint);
    CTexture::m_Text_HDRTargetScaled[1]->Apply(1, nTexStatePoint);
    if (CTexture::m_pCurLumTexture)
      CTexture::m_pCurLumTexture->Apply(2, nTexStatePoint);
  }
  else
  {
    CTexture::m_Text_HDRTargetScaled[0]->Apply(0, nTexStatePoint);
    if (CTexture::m_pCurLumTexture)
      CTexture::m_pCurLumTexture->Apply(1, nTexStatePoint);

    pShaftsRT->Apply(2, nTexStateLinear);
  }

  // Draw a fullscreen quad to sample the RT
  DrawFullScreenQuad(coords);

  pSH->FXEndPass();

  return bResult;
}

bool HDR_RenderGlowAndBloom()
{
  gcpRendD3D->Set2DMode(true, 1, 1);      

  CD3D9Renderer *rd = gcpRendD3D; 

  // How its working:
  //  - In case hardware supports fp16 filtering, we do forward approach, using hardware
  //  - Otherwise, HDR is first RGBS+PPP encoded into RGB8 and all post process done in RGB8 

  uint nPasses;
  CShader *pSh = CShaderMan::m_shPostEffects;
  
  // HDR source
  CTexture *pHDRSrc = CTexture::m_Text_HDRBrightPass[0];
  // Bloom generation/intermediate render-targets
  CTexture *pBloomGen[3];         
  // Final bloom RT
  CTexture *pBloom = 0; 

  // If HW doens't support fp16 filter or shader quality below high use low-spec hdr bloom
  if( gcpRendD3D->m_bDeviceSupportsFP16Filter )
  {
    pBloom = CTexture::m_Text_HDRBrightPass[0];             // 2x smaller than frame buffer
    pBloomGen[0] = CTexture::m_Text_HDRTargetScaled[0];  // 2x         ''
    pBloomGen[1] = CTexture::m_Text_HDRTargetScaled[1];  // 4x         ''
    pBloomGen[2] = CTexture::m_Text_HDRTargetScaled[2];  // 8x         ''

    StretchRect( pHDRSrc, pBloomGen[0] );      
  }
  else
  {
    // In case hardware with no fp16 filtering support, we reuse textures from post-effects

    pBloom = CTexture::m_Text_HDRBrightPass[1];           // 2x smaller than frame buffer    
    pBloomGen[0] = CTexture::m_Text_BackBufferScaled[0];  // 2x         ''
    pBloomGen[1] = CTexture::m_Text_BackBufferScaled[1];  // 4x         ''
    pBloomGen[2] = CTexture::m_Text_BackBufferScaled[2];  // 8x         ''

    HDR_EncodeToLDR( pHDRSrc, pBloomGen[0] );
  }
      
  // Bloom/Glow generation: 
  //  - using 3 textures, each with half resolution of previous, Gaussian blurred and result summed up at end

  TexBlurGaussian( pBloomGen[0], 1, 1.0f, 2.0f );                  

  StretchRect( pBloomGen[0], pBloomGen[1] );      
  TexBlurGaussian( pBloomGen[1], 1, 1.0f, 5.0f );           

  StretchRect( pBloomGen[1], pBloomGen[2] );                 
  TexBlurGaussian( pBloomGen[2], 1, 1.0f, 5.0f );    
  
  // Compose final result (would be better in final HDR pass, but already too much textures used there)  

  gcpRendD3D->FX_SetRenderTarget(0, pBloom, &gcpRendD3D->m_DepthBufferOrig);

  static CCryName TechName("ComposeFinalHDRGlow");
  CShaderMan::m_ShaderPostProcess->FXSetTechnique(TechName);
  CShaderMan::m_ShaderPostProcess->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

  CShaderMan::m_ShaderPostProcess->FXBeginPass(0);

  CTexture::BindNULLFrom(0);// workaround for dx10 skipping setting correct samplers
  pBloomGen[0]->Apply( 0, nTexStateLinear ); 
  pBloomGen[1]->Apply( 1, nTexStateLinear ); 
  pBloomGen[2]->Apply( 2, nTexStateLinear ); 
  

  SPostEffectsUtils::DrawFullScreenQuad( pBloom->GetWidth(), pBloom->GetHeight() );

  CShaderMan::m_ShaderPostProcess->FXEndPass();
  CShaderMan::m_ShaderPostProcess->FXEnd();  

  gcpRendD3D->Set2DMode(false, 1, 1);      

  return true;
}

bool HDR_RenderFinalScene(CShader *pSH)
{
  // Create color transformation matrices
  
  // Compose final color matrix and set fragment program constants
  Matrix44 &pColorMat = SPostEffectsUtils::GetColorMatrix();

  Vec4 pColorMatrix[4]=
  {
    Vec4(pColorMat.m00, pColorMat.m01, pColorMat.m02, pColorMat.m03),
    Vec4(pColorMat.m10, pColorMat.m11, pColorMat.m12, pColorMat.m13), 
    Vec4(pColorMat.m20, pColorMat.m21, pColorMat.m22, pColorMat.m23),
    Vec4(pColorMat.m30, pColorMat.m31, pColorMat.m32, pColorMat.m33)
  };  

  HRESULT hr = S_OK;  
  CD3D9Renderer *rd = gcpRendD3D;
  bool bResult = false;

  // Final bloom RT
  CTexture *pBloom = CTexture::m_Text_HDRBrightPass[0];
  if( !gcpRendD3D->m_bDeviceSupportsFP16Filter )
    pBloom = CTexture::m_Text_HDRBrightPass[1];

  if( !CRenderer::CV_r_glow )
  {
    pBloom = CTexture::m_Text_Black;
  }

  // Draw the high dynamic range scene texture to the low dynamic range
  // back buffer. As part of this final pass, the scene will be tone-mapped
  // using the user's current adapted luminance, blue shift will occur
  // if the scene is determined to be very dark, and the post-process lighting
  // effect textures will be added to the scene.

  uint nPasses;
  static CCryName TechFinalName("HDRFinalPass");
  pSH->FXSetTechnique(TechFinalName);
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  pSH->FXBeginPass(0);

	SetHDRParams(pSH);

  if (CRenderer::CV_r_hdrhistogram)
  {
	  Vec4 v;
		v[0] = max(0.0f,rd->m_LumGamma.LowPercentil - rd->m_LumGamma.HighPercentil*g_fEyeAdaptionLowerPercent);	// extrapolate 0% from 5%
    v[1] = rd->m_LumGamma.MidPercentil;
    v[2] = rd->m_LumGamma.HighPercentil / g_fEyeAdaptionUpperPercent;	// extrapolate 95% from 100%
    v[3] = rd->m_LumGamma.Bias;
    static CCryName Param1Name("HDRGamma");
    pSH->FXSetPSFloat(Param1Name, &v, 1);

    v[0] = rd->m_LumGamma.CurLuminance;
    static CCryName Param2Name("HDRLuminance");
    pSH->FXSetPSFloat(Param2Name, &v, 1);
  }
  
  static CCryName Param3Name("HDRColorMatrix");
  pSH->FXSetPSFloat(Param3Name, pColorMatrix, 4);

  if (rd->m_bDeviceSupportsFP16Separate)
  {
    if (rd->m_nHDRType > 1)
      CTexture::m_Text_HDRTarget->Apply(0, nTexStateLinear);
    else
      CTexture::m_Text_HDRTarget->Apply(0, nTexStatePoint);
    pBloom->Apply(1, nTexStateLinear);
    if (CTexture::m_pCurLumTexture)
      CTexture::m_pCurLumTexture->Apply(2, nTexStatePoint);

		static uint32 dwNoiseOffsetX=0;
		static uint32 dwNoiseOffsetY=0;

		dwNoiseOffsetX = (dwNoiseOffsetX+27)&0x3f;
		dwNoiseOffsetY = (dwNoiseOffsetY+19)&0x3f;

		Vec4 pFrameRand(dwNoiseOffsetX/64.0f, dwNoiseOffsetX/64.0f, (rand()%1024)/1024.0f, (rand()%1024)/1024.0f );
//		Vec4 pFrameRand((rand()%1024)/1024.0f, (rand()%1024)/1024.0f, (rand()%1024)/1024.0f, (rand()%1024)/1024.0f );

//    pSH->FXSetPSFloat("FrameRand", &pFrameRand, 4);
    static CCryName Param4Name("FrameRand");
    pSH->FXSetVSFloat(Param4Name, &pFrameRand, 1);
		CTexture::m_Text_ScreenNoiseMap->Apply(4, nTexStateLinearWrap);

  }
  else
  {
    CTexture::m_Text_HDRTarget->Apply(0, nTexStatePoint);
    pBloom->Apply(1, nTexStateLinear);
    CTexture::m_Text_Black->Apply(3, nTexStateLinear);
    if (CTexture::m_pCurLumTexture)
      CTexture::m_pCurLumTexture->Apply(2, nTexStatePoint);
    static uint32 dwNoiseOffsetX=0;
    static uint32 dwNoiseOffsetY=0;

    dwNoiseOffsetX = (dwNoiseOffsetX+27)&0x3f;
    dwNoiseOffsetY = (dwNoiseOffsetY+19)&0x3f;

    Vec4 pFrameRand(dwNoiseOffsetX/64.0f, dwNoiseOffsetX/64.0f, (rand()%1024)/1024.0f, (rand()%1024)/1024.0f );
    //		Vec4 pFrameRand((rand()%1024)/1024.0f, (rand()%1024)/1024.0f, (rand()%1024)/1024.0f, (rand()%1024)/1024.0f );

    //    pSH->FXSetPSFloat("FrameRand", &pFrameRand, 4);
    static CCryName Param4Name("FrameRand");
    pSH->FXSetVSFloat(Param4Name, &pFrameRand, 1);
    CTexture::m_Text_ScreenNoiseMap->Apply(4, nTexStateLinearWrap);

    if (rd->m_nHDRType > 1)
    {
      assert(0);
      /*CTexture::m_Text_HDRTarget_Temp->Apply(5, nTexStateLinear);
      if (rd->m_RP.m_PersFlags2 & RBPF2_HDR_MRT_WASALPHABLEND)
      {
        CTexture::m_Text_SceneTarget->Apply(6, nTexStateLinear);
        CTexture::m_Text_SceneTarget_HDR->Apply(7, nTexStateLinear);
      }*/
    }
  }

	DrawQuad3D(0, 1, 1, 0);
  //DrawFullScreenQuad( 0.0f, 0.0f, 1.0f, 1.0f );

  rd->EF_SelectTMU(0);

  return true;
}

bool HDR_RenderFinalDebugScene(CShader *pSH)
{
  HRESULT hr = S_OK;
  CD3D9Renderer *rd = gcpRendD3D;
  Vec4 avSampleOffsets[4];

  uint nPasses;
	static CCryName techName("HDRFinalDebugPass");
  pSH->FXSetTechnique(techName);
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  pSH->FXBeginPass(0);

  GetSampleOffsets_DownScale2x2(CTexture::m_Text_HDRTarget->GetWidth(), CTexture::m_Text_HDRTarget->GetHeight(), avSampleOffsets);
 	static CCryName SampleOffsetsName("SampleOffsets");
  pSH->FXSetPSFloat(SampleOffsetsName, avSampleOffsets, 4);

  if (rd->m_nHDRType > 1)
    CTexture::m_Text_HDRTarget->Apply(0, nTexStateLinear);
  else
    CTexture::m_Text_HDRTarget->Apply(0, nTexStatePoint);

  DrawFullScreenQuad( 0.0f, 0.0f, 1.0f, 1.0f );

  return true;
}


void HDR_DrawDebug()
{
#ifdef USE_HDR
  if (!CRenderer::CV_r_hdrrendering)
    return;
#endif

  CD3D9Renderer *rd = gcpRendD3D;

  rd->EF_SetState(GS_NODEPTHTEST);
  int iTmpX, iTmpY, iTempWidth, iTempHeight;
  rd->GetViewport(&iTmpX, &iTmpY, &iTempWidth, &iTempHeight);   
	
  rd->EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
  rd->Set2DMode(true, 1, 1);

  CShader *pSH = CShaderMan::m_ShaderDebug;

  /*if (rd->m_RP.m_PersFlags2 & RBPF2_HDR_MRT_WASALPHABLEND)
  {
    rd->SetViewport(10, 280, 100, 100);   
    rd->DrawImage(0, 0, 1, 1, CTexture::m_Text_SceneTarget->GetID(), 0, 1, 1, 0, 1,1,1,1);

    rd->SetViewport(120, 280, 100, 100);   
    rd->DrawImage(0, 0, 1, 1, CTexture::m_Text_SceneTarget_HDR->GetID(), 0, 1, 1, 0, 1,1,1,1);
  }*/

  // 1
  rd->SetViewport(10, 400, 100, 100);   
  rd->DrawImage(0, 0, 1, 1, CTexture::m_Text_HDRTarget->GetID(), 0, 1, 1, 0, 1,1,1,1);

  // 2
  rd->SetViewport(120, 400, 100, 100);   
  uint nPasses;
  if (rd->m_bDeviceSupportsFP16Separate)
  {
    pSH->FXSetTechnique("Debug_ShowMRT");
    pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
    pSH->FXBeginPass(0);

    CTexture::m_Text_HDRTargetScaled[0]->Apply(0, nTexStatePoint);
    CTexture::m_Text_HDRTargetScaled[1]->Apply(1, nTexStatePoint);

    DrawFullScreenQuadTR(0.0f, 0.0f, 1.0f, 1.0f);
    rd->EF_SelectTMU(0);
  }
  else
    rd->DrawImage(0, 0, 1, 1, CTexture::m_Text_HDRTargetScaled[0]->GetID(), 0, 1, 1, 0, 1,1,1,1);

  pSH->FXSetTechnique("Debug_ShowR");
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  pSH->FXBeginPass(0);

  CTexture::m_Text_HDRToneMaps[3]->Apply(0, nTexStatePoint);
  rd->SetViewport(10, 510, 100, 100);   
  DrawFullScreenQuadTR(0.0f, 0.0f, 1.0f, 1.0f);

  if (CTexture::m_Text_HDRToneMaps[2])
  {
    CTexture::m_Text_HDRToneMaps[2]->Apply(0, nTexStatePoint);
    rd->SetViewport(120, 510, 100, 100);   
    DrawFullScreenQuadTR(0.0f, 0.0f, 1.0f, 1.0f);
  }

  if (CTexture::m_Text_HDRToneMaps[1])
  {
    CTexture::m_Text_HDRToneMaps[1]->Apply(0, nTexStatePoint);
    rd->SetViewport(230, 510, 100, 100);   
    DrawFullScreenQuadTR(0.0f, 0.0f, 1.0f, 1.0f);
  }

  if (CTexture::m_Text_HDRToneMaps[0])
  {
    CTexture::m_Text_HDRToneMaps[0]->Apply(0, nTexStatePoint);
    rd->SetViewport(340, 510, 100, 100);   
    DrawFullScreenQuadTR(0.0f, 0.0f, 1.0f, 1.0f);
  }

  if (CTexture::m_pCurLumTexture)
  {
    CTexture::m_pCurLumTexture->Apply(0, nTexStatePoint);
    rd->SetViewport(450, 510, 100, 100);   
    DrawFullScreenQuadTR(0.0f, 0.0f, 1.0f, 1.0f);
  }

  // 3
  rd->SetViewport(230, 400, 100, 100);   
  rd->DrawImage(0, 0, 1, 1, CTexture::m_Text_HDRBrightPass[0]->GetID(), 0, 1, 1, 0, 1,1,1,1);

  // 5
  if( CRenderer::CV_r_glow )
  {    
    // Bloom generation/intermediate render-targets
    CTexture *pBloomGen[3];         
    // Final bloom RT
    CTexture *pBloom = 0; 
    
    // If HW doens't support fp16 filter or shader quality below high use low-spec hdr bloom
    if( gcpRendD3D->m_bDeviceSupportsFP16Filter )
    {      
      pBloom = CTexture::m_Text_HDRBrightPass[0];             // 2x smaller than frame buffer
      pBloomGen[0] = CTexture::m_Text_HDRTargetScaled[0];  // 2x         ''
      pBloomGen[1] = CTexture::m_Text_HDRTargetScaled[1];  // 4x         ''
      pBloomGen[2] = CTexture::m_Text_HDRTargetScaled[2];  // 8x         ''
    }
    else
    {
      // In case hardware with no fp16 filtering support, we reuse textures from post-effects

      pBloom = CTexture::m_Text_HDRBrightPass[1];             // 2x smaller than frame buffer
      pBloomGen[0] = CTexture::m_Text_BackBufferScaled[0];  // 2x         ''
      pBloomGen[1] = CTexture::m_Text_BackBufferScaled[1];  // 4x         ''
      pBloomGen[2] = CTexture::m_Text_BackBufferScaled[2];  // 8x         ''
    }

    rd->SetViewport(340, 400, 100, 100);   
    rd->DrawImage(0, 0, 1, 1, pBloom->GetID(), 0, 1, 1, 0, 1,1,1,1);

    rd->SetViewport(450, 400, 100, 100);
    rd->DrawImage(0, 0, 1, 1, pBloomGen[0]->GetID(), 0, 1, 1, 0, 1,1,1,1);

    rd->SetViewport(560, 400, 100, 100);
    rd->DrawImage(0, 0, 1, 1, pBloomGen[1]->GetID(), 0, 1, 1, 0, 1,1,1,1);

    rd->SetViewport(670, 400, 100, 100);
    rd->DrawImage(0, 0, 1, 1, pBloomGen[2]->GetID(), 0, 1, 1, 0, 1,1,1,1);
  }

  rd->Set2DMode(false, 1, 1);
  rd->SetViewport(iTmpX, iTmpY, iTempWidth, iTempHeight);   

	{
		char str[256];

		SDrawTextInfo ti;

		sprintf(str,"r_HDRRendering: %d", CRenderer::CV_r_hdrrendering);
		rd->Draw2dText(5,230,str,ti);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CD3D9Renderer::FX_HDRPostProcessing()
{
  PROFILE_FRAME(Draw_HDR_PostProcessing);

  nTexStateLinear = CTexture::GetTexState(sTexStateLinear);
  nTexStateLinearWrap = CTexture::GetTexState(sTexStateLinearWrap);
  nTexStatePoint = CTexture::GetTexState(sTexStatePoint);

  if (m_polygon_mode == R_WIREFRAME_MODE)
  {
#if defined (DIRECT3D9) || defined (OPENGL)
    m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#elif defined (DIRECT3D10)
		SStateRaster RS = gcpRendD3D->m_StatesRS[gcpRendD3D->m_nCurStateRS];
		RS.Desc.FillMode = D3D10_FILL_SOLID;
		gcpRendD3D->SetRasterState(&RS);
#endif
  }

  if (!CTexture::m_Text_HDRTarget || !CTexture::m_Text_HDRTarget->GetDeviceTexture())
   return;
  bool bSceneTargetFSAA = gcpRendD3D->m_RP.m_FSAAData.Type && CRenderer::CV_r_debug_extra_scenetarget_fsaa;
  if (CTexture::m_Text_HDRTarget->GetWidth() != GetWidth() || CTexture::m_Text_HDRTarget->GetHeight() != GetHeight() || 
    bSceneTargetFSAA && (CTexture::m_Text_SceneTargetFSAA->GetWidth() != GetWidth() || CTexture::m_Text_SceneTargetFSAA->GetHeight() != GetHeight()) )
    CTexture::GenerateHDRMaps();

  bool bMeasureLuminance = true;

  EF_ResetPipe();

  FX_PostProcessSceneHDR();

  assert(m_RTStack[0][m_nRTStackLevel[0]].m_pTex == CTexture::m_Text_HDRTarget);

  if(CRenderer::CV_r_GetScreenShot==1)
  {
#if defined (DIRECT3D9) || defined (OPENGL)
		char path[ICryPak::g_nMaxPath];
		path[sizeof(path) - 1] = 0;
		gEnv->pCryPak->AdjustFileName("%USER%/ScreenShots", path, ICryPak::FLAGS_NO_MASTER_FOLDER_MAPPING | ICryPak::FLAGS_FOR_WRITING);

		if (!gEnv->pCryPak->MakeDir(path))
		{
			iLog->Log("Cannot save screen shot! Failed to create directory \"%s\".", path);
		}
		else
		{
			char pszFileName[1024];

			int i=0;
			for(i=0 ; i<10000; i++)
			{
				snprintf(pszFileName, sizeof(pszFileName), "%s/ScreenShot%04d.hdr", path, i);
				pszFileName[sizeof(pszFileName)-1] = '\0';

				FILE *fp= fxopen(pszFileName, "rb");
				if(!fp)
					break;

				fclose(fp);
			}

			if(i==10000)
			{
				iLog->Log("Cannot save ScreenShot: Too many HDR files");
				return;
			}


			LPDIRECT3DTEXTURE9 pHDRTex = (LPDIRECT3DTEXTURE9)CTexture::m_Text_HDRTarget->GetDeviceTexture();
			LPDIRECT3DSURFACE9 pHDRSurf = 0;
			HRESULT hr = pHDRTex->GetSurfaceLevel(0, &pHDRSurf);

			D3DXSaveSurfaceToFile(pszFileName, D3DXIFF_HDR, pHDRSurf, 0, 0);

			SAFE_RELEASE(pHDRSurf);
		}
#else
		iLog->Log("HDR screen shots are not yet supported on DX10!");		// TODO: D3DXSaveSurfaceToFile()
#endif
  }


  CShader *pSH = CShaderMan::m_ShaderPostProcess;
  m_RP.m_FlagsShader_RT = 0;

  //if (CV_r_srgbtexture)
  //  m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SRGB];

  if (CRenderer::CV_r_hdrhistogram)
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_HDR_HISTOGRAMM];
  else
    m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_HDR_HISTOGRAMM];

  EF_ApplyShaderQuality(eST_HDR);  

  // Create a scaled copy of the scene
  HDR_SceneToSceneScaled();

  // Setup tone mapping technique
  if (bMeasureLuminance)
    HDR_MeasureLuminance(pSH);

  // Calculate the current luminance adaptation level
  HDR_CalculateAdaptation(pSH);

  if (CRenderer::CV_r_hdrhistogram)
  {
    HDR_SysCalculateHistogram(pSH,CTexture::m_Text_HDRToneMaps[NUM_HDR_TONEMAP_TEXTURES-1]);
    HDR_SysCalculateGamma();
  }

  // Now that luminance information has been gathered, the scene can be bright-pass filtered
  // to remove everything except bright lights and reflections.
  HDR_SceneScaledToBrightPass(pSH);

  if( CRenderer::CV_r_glow )
  {
    HDR_RenderGlowAndBloom();
  }
  
  FX_PopRenderTarget(0);
  EF_ClearBuffers(0, NULL);

  CTexture::BindNULLFrom(0);// workaround for dx10 skipping setting correct samplers

  // Render final scene to the back buffer
  if (CRenderer::CV_r_hdrdebug != 1)
    HDR_RenderFinalScene(pSH);
  else
    HDR_RenderFinalDebugScene(pSH);

  //if( CRenderer::CV_r_glow && CTexture::m_Text_Glow)
  //{
  //  // Disable glow again
  //  CEffectParam *pParam = PostEffectMgr().GetByName("Glow_Active"); 
  //  pParam->SetParam(0.0f);   
  //}
  
  m_RP.m_PersFlags &= ~RBPF_HDR;
  m_RP.m_PersFlags2 &= ~(RBPF2_HDR_FP16 | RBPF2_LIGHTSHAFTS);

  EF_ResetPipe();

  if(m_polygon_mode == R_WIREFRAME_MODE)
  {
#if defined (DIRECT3D9) || defined (OPENGL)
    m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
#elif defined (DIRECT3D10)
		SStateRaster RS = gcpRendD3D->m_StatesRS[gcpRendD3D->m_nCurStateRS];
		RS.Desc.FillMode = D3D10_FILL_WIREFRAME;
		gcpRendD3D->SetRasterState(&RS);
#endif
  }
}
