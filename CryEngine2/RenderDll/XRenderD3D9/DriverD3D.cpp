/*=============================================================================
  DriverD3D9.cpp : Direct3D Render interface implementation.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Khonich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "I3DEngine.h"
#include "../Common/VideoPlayerInstance.h"

#ifdef WIN32
	#include "../common/NVAPI/nvapi.h"				// NVAPI
#endif

#if defined(DIRECT3D10) && (defined (WIN32) || defined(WIN64))
#	include <D3DX10core.h>
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

CCryName CTexture::m_sClassName = CCryName("CTexture");

CCryName CHWShader::m_sClassNameVS = CCryName("CHWShader_VS");
CCryName CHWShader::m_sClassNamePS = CCryName("CHWShader_PS");

CCryName CShader::m_sClassName = CCryName("CShader");

// Included only once per DLL module.
#include <platform_impl.h>

CD3D9Renderer *gcpRendD3D = NULL;

extern CTexture *gTexture;
extern CTexture *gTexture2;

int CD3D9Renderer::CV_d3d9_debugruntime;

int CD3D9Renderer::CV_d3d9_nvperfhud;
int CD3D9Renderer::CV_d3d9_vbpools;
int CD3D9Renderer::CV_d3d9_vbpoolsize;
int CD3D9Renderer::CV_d3d9_ibpools;
int CD3D9Renderer::CV_d3d9_ibpoolsize;
int CD3D9Renderer::CV_d3d9_clipplanes;
int CD3D9Renderer::CV_d3d9_triplebuffering;
int CD3D9Renderer::CV_d3d9_resetdeviceafterloading;
int CD3D9Renderer::CV_d3d9_forcesoftware;
int CD3D9Renderer::CV_d3d9_allowsoftware;
float CD3D9Renderer::CV_d3d9_pip_buff_size;
int CD3D9Renderer::CV_d3d9_rb_verts;
int CD3D9Renderer::CV_d3d9_rb_tris;
int CD3D9Renderer::CV_d3d9_null_ref_device;

#if defined (DIRECT3D10)
int CD3D9Renderer::CV_d3d10_CBUpdateStats;
int CD3D9Renderer::CV_d3d10_CBUpdateMethod;
int CD3D9Renderer::CV_d3d10_NumStagingBuffers;
#endif

#ifdef XENON
int CD3D9Renderer::CV_d3d9_predicatedtiling;
#endif
ICVar *CD3D9Renderer::CV_d3d9_texturefilter;

#ifdef WIN32
IDirectBee *CRenderer::m_pDirectBee=0;		// connection to D3D9 wrapper DLL, 0 if not established
#endif

char *resourceName[] = {
    "UNKNOWN",
    "Surfaces",
    "Volumes",
    "Textures",
    "Volume Textures",
    "Cube Textures",
    "Vertex Buffers",
    "Index Buffers"
};

// Direct 3D console variables
CD3D9Renderer::CD3D9Renderer()
#if defined (WIN32) || defined(XENON)
  : m_pAsyncShaderManager( new CAsyncShaderManager() )
#endif
{
  int i;
  m_bInitialized = false;
  gcpRendD3D = this;
  gRenDev = this;

  m_LogFile = NULL;

  RegisterVariables();

#if defined (DIRECT3D9) || defined (OPENGL)
  m_pD3D              = NULL;
#elif defined (DIRECT3D10)
  m_nCurStateBL = (uint) -1;
  m_nCurStateRS = (uint) -1;
  m_nCurStateDP = (uint) -1;
	m_CurTopology = D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED;
#endif

  m_pd3dDevice        = NULL;
  m_hWnd              = NULL;
  m_dwCreateFlags     = 0L;
  for (i=0; i<POOL_MAX; i++)
  {
    m_pVB[i] = NULL;
		m_pVBAr[i][0] = NULL;
		m_pVBAr[i][1] = NULL;
		m_pVBAr[i][2] = NULL;
		m_pVBAr[i][3] = NULL;
  }
  m_pUnitFrustumVB = NULL;
  m_pUnitFrustumIB = NULL;

  m_pUnitSphereVB = NULL;
  m_pUnitSphereIB = NULL;


  memset(&m_LumGamma, 0, sizeof(m_LumGamma));
  memset(&m_LumHistogram, 0, sizeof(m_LumHistogram));

	m_pIB = 0;

#ifdef WIN32
	m_pDirectBee=0;
#endif
  m_strDeviceStats[0] = 0;

  m_Features = RFT_DIRECTACCESSTOVIDEOMEMORY | RFT_SUPPORTZBIAS;

  iConsole->Register("d3d9_debugruntime", &CV_d3d9_debugruntime, 0);

#ifndef PUBLIC_BUILD
  iConsole->Register("d3d9_NVPerfHUD", &CV_d3d9_nvperfhud, 0);
#endif

#if defined (XENON)
  iConsole->Register("d3d9_VBPools", &CV_d3d9_vbpools, 0, VF_REQUIRE_APP_RESTART);
#else
  iConsole->Register("d3d9_VBPools", &CV_d3d9_vbpools, 1, VF_REQUIRE_APP_RESTART);
#endif

  iConsole->Register("d3d9_VBPoolSize", &CV_d3d9_vbpoolsize, 256*1024);
#if defined (DIRECT3D9) && !defined(XENON)
  iConsole->Register("d3d9_IBPools", &CV_d3d9_ibpools, 1, VF_REQUIRE_APP_RESTART);
#elif defined (DIRECT3D10)
  iConsole->Register("d3d9_IBPools", &CV_d3d9_ibpools, 0, VF_REQUIRE_APP_RESTART);
#endif
  iConsole->Register("d3d9_IBPoolSize", &CV_d3d9_ibpoolsize, 256*1024);
#if defined (DIRECT3D9) || defined (OPENGL)
  iConsole->Register("d3d9_ClipPlanes", &CV_d3d9_clipplanes, 1);
#elif defined (DIRECT3D10)
  iConsole->Register("d3d9_ClipPlanes", &CV_d3d9_clipplanes, 0);
#endif

#if defined (DIRECT3D10)
	iConsole->Register("d3d10_CBUpdateStats", &CV_d3d10_CBUpdateStats, 0, 0, "Logs constant buffer updates statistics.");
	{
		CV_d3d10_CBUpdateMethod = 0;
		ICVar* p(iConsole->RegisterInt("d3d10_CBUpdateMethod", 0, VF_REQUIRE_APP_RESTART, "Chooses constant buffer update method: 0 - Map()/Unmap(), 1 - UpdateSubresource()"));
		if (p) 
			CV_d3d10_CBUpdateMethod = p->GetIVal();
	}
	{
#if defined(PS3)
		CV_d3d10_NumStagingBuffers = 4;
#else
		CV_d3d10_NumStagingBuffers = 16;
#endif
		ICVar* p(iConsole->RegisterInt("d3d10_NumStagingBuffers", CV_d3d10_NumStagingBuffers, VF_REQUIRE_APP_RESTART, "Number of staging buffers to use for mesh updates [1, 512]."));
		if (p)
			CV_d3d10_NumStagingBuffers = clamp_tpl(p->GetIVal(), 1, MAX_STAGING_BUFFERS);
	}
#endif
  iConsole->Register("d3d9_TripleBuffering", &CV_d3d9_triplebuffering, 0, VF_REQUIRE_APP_RESTART);
  iConsole->Register("d3d9_ResetDeviceAfterLoading", &CV_d3d9_resetdeviceafterloading, 1);
  iConsole->Register("d3d9_ForceSoftware", &CV_d3d9_forcesoftware, 0, VF_REQUIRE_APP_RESTART);
  CV_d3d9_texturefilter = iConsole->RegisterString("d3d9_TextureFilter", "TRILINEAR", VF_DUMPTODISK,
    "Specifies D3D specific texture filtering type.\n"
    "Usage: d3d9_TexMipFilter [TRILINEAR/BILINEAR/LINEAR/NEAREST]\n");
  iConsole->Register("d3d9_AllowSoftware", &CV_d3d9_allowsoftware, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("d3d9_pip_buff_size", &CV_d3d9_pip_buff_size, 80, VF_REQUIRE_APP_RESTART);
  iConsole->Register("d3d9_rb_Verts", &CV_d3d9_rb_verts, 16384, VF_REQUIRE_APP_RESTART);
  iConsole->Register("d3d9_rb_Tris", &CV_d3d9_rb_tris, 32768, VF_REQUIRE_APP_RESTART);
	iConsole->Register("d3d9_NullRefDevice", &CV_d3d9_null_ref_device, 0, VF_REQUIRE_APP_RESTART);
#ifdef XENON
  iConsole->Register("d3d9_PredicatedTiling", &CV_d3d9_predicatedtiling, 1, VF_REQUIRE_APP_RESTART);
#endif

	m_pD3DRenderAuxGeom = CD3DRenderAuxGeom::Create( *this );

	CV_capture_frames = 0;
	CV_capture_folder = 0;
	CV_capture_file_format = 0;
#if defined (DIRECT3D9) || defined (OPENGL)
	for (int i=0; i<sizeof(m_pCaptureFrameSurf) / sizeof(m_pCaptureFrameSurf[0]); ++i)
		m_pCaptureFrameSurf[i] = NULL;
  m_nQueryCount = 0;
#endif

#if defined (DIRECT3D9) || defined (OPENGL)
  m_NewViewport.MinZ = 0;
  m_NewViewport.MaxZ = 1;
#elif defined (DIRECT3D10)
  m_NewViewport.MinDepth = 0;
  m_NewViewport.MaxDepth = 1;
#endif

#if defined (DIRECT3D9) || defined (OPENGL)
	for (int i=0; i<MAX_REND_LIGHT_GROUPS; ++i)
	{
		m_pCaptureScreenShadowMap[i] = NULL;
		m_ValidCaptureScreenShadowMap[i] = false;
	}
#endif
}

CD3D9Renderer::~CD3D9Renderer()
{
  //FreeResources(FRR_ALL);
  ShutDown();
}

void  CD3D9Renderer::ShareResources( IRenderer *renderer )
{
}

void	CD3D9Renderer::MakeCurrent()
{
  if (m_CurrContext == m_RContexts[0])
    return;

  m_CurrContext = m_RContexts[0];

  CHWShader::m_pCurVS = NULL;
  CHWShader::m_pCurPS = NULL;
}

bool CD3D9Renderer::SetCurrentContext(WIN_HWND hWnd)
{
  uint i;

  for (i=0; i<m_RContexts.Num(); i++)
  {
    if (m_RContexts[i]->m_hWnd == hWnd)
      break;
  }
  if (i == m_RContexts.Num())
    return false;

  if (m_CurrContext == m_RContexts[i])
    return true;

  m_CurrContext = m_RContexts[i];

  CHWShader::m_pCurVS = NULL;
  CHWShader::m_pCurPS = NULL;

  return true;
}

bool CD3D9Renderer::CreateContext(WIN_HWND hWnd, bool bAllowFSAA)
{
  uint i;

  for (i=0; i<m_RContexts.Num(); i++)
  {
    if (m_RContexts[i]->m_hWnd == hWnd)
      break;
  }
  if (i != m_RContexts.Num())
    return true;
  SD3DContext *pContext = new SD3DContext;
  pContext->m_hWnd = (HWND)hWnd;
  pContext->m_X = 0;
  pContext->m_Y = 0;
  pContext->m_Width = m_width;
  pContext->m_Height = m_height;
#if defined(DIRECT3D10)
	pContext->m_pSwapChain = 0;
	pContext->m_pBackBuffer = 0;
#endif
  m_CurrContext = pContext;
  m_RContexts.AddElem(pContext);

  return true;
}

bool CD3D9Renderer::DeleteContext(WIN_HWND hWnd)
{
  uint i, j;

  for (i=0; i<m_RContexts.Num(); i++)
  {
    if (m_RContexts[i]->m_hWnd == hWnd)
      break;
  }
  if (i == m_RContexts.Num())
    return false;
  if (m_CurrContext == m_RContexts[i])
  {
    for (j=0; j<m_RContexts.Num(); j++)
    {
      if (m_RContexts[j]->m_hWnd != hWnd)
      {
        m_CurrContext = m_RContexts[j];
        break;
      }
    }
    if (j == m_RContexts.Num())
      m_CurrContext = NULL;
  }
#if defined(DIRECT3D10)
	SAFE_RELEASE(m_RContexts[i]->m_pSwapChain);
	SAFE_RELEASE(m_RContexts[i]->m_pBackBuffer);
#endif
  delete m_RContexts[i];
  m_RContexts.Remove(i, 1);

  return true;
}



void CD3D9Renderer::RegisterVariables()
{
}

void CD3D9Renderer::UnRegisterVariables()
{
}

void CD3D9Renderer::Reset (void)
{
#if !defined(XENON) && !defined(PS3)
  if (!CV_d3d9_resetdeviceafterloading)
    return;
  m_bDeviceLost = true;
  //iLog->Log("...Reset");
  RestoreGamma();
#if defined (DIRECT3D9) || defined (OPENGL)
  HRESULT hReturn;
  if(FAILED(hReturn = DXUTReset3DEnvironment9()))
    return;
#endif
  m_bDeviceLost = false;
  m_FSAA = 0;
  CheckFSAAChange();
  if (m_bFullScreen)
    SetGamma(CV_r_gamma+m_fDeltaGamma, CV_r_brightness, CV_r_contrast, false);
#if defined (DIRECT3D9) || defined (OPENGL)
  hReturn = m_pd3dDevice->BeginScene();
#endif

#endif
}

void CD3D9Renderer::ChangeViewport(unsigned int x,unsigned int y,unsigned int width,unsigned int height)
{
  if (m_bDeviceLost)
    return;
  assert(m_CurrContext);
#if defined(DIRECT3D10)
	if (!m_CurrContext->m_pSwapChain && !m_CurrContext->m_pBackBuffer)
	{
		DXGI_SWAP_CHAIN_DESC scDesc;
		scDesc.BufferDesc.Width = width;
		scDesc.BufferDesc.Height = height;
		scDesc.BufferDesc.RefreshRate.Numerator = 0;
		scDesc.BufferDesc.RefreshRate.Denominator = 1;
		scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		scDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		scDesc.SampleDesc.Count = 1;
		scDesc.SampleDesc.Quality = 0;

		scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scDesc.BufferCount = 1;
#if defined(PS3)
		scDesc.OutputWindow = (typeof(scDesc.OutputWindow))m_CurrContext->m_hWnd;
#else
		scDesc.OutputWindow = m_CurrContext->m_hWnd;
#endif
		scDesc.Windowed = TRUE;
		scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		scDesc.Flags = 0;				

		HRESULT hr = DXUTGetDXGIFactory()->CreateSwapChain(m_pd3dDevice, &scDesc, &m_CurrContext->m_pSwapChain);
		assert(SUCCEEDED(hr) && m_CurrContext->m_pSwapChain != 0);

		ID3D10Texture2D* pBackBufferTex(0);
		hr = m_CurrContext->m_pSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**) &pBackBufferTex);
		assert(SUCCEEDED(hr) && pBackBufferTex != 0);

		D3D10_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;
		hr = m_pd3dDevice->CreateRenderTargetView(pBackBufferTex, &rtDesc, &m_CurrContext->m_pBackBuffer);
		assert(SUCCEEDED(hr) && m_CurrContext->m_pBackBuffer != 0);

		SAFE_RELEASE(pBackBufferTex);
	}
	else if (m_CurrContext->m_Width != width || m_CurrContext->m_Height != height)
	{
		assert(m_CurrContext->m_pSwapChain && m_CurrContext->m_pBackBuffer);
		
		SAFE_RELEASE(m_CurrContext->m_pBackBuffer);

		HRESULT hr = m_CurrContext->m_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

		ID3D10Texture2D* pBackBufferTex(0);
		hr = m_CurrContext->m_pSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**) &pBackBufferTex);
		assert(SUCCEEDED(hr) && pBackBufferTex != 0);

		D3D10_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;
		hr = m_pd3dDevice->CreateRenderTargetView(pBackBufferTex, &rtDesc, &m_CurrContext->m_pBackBuffer);
		assert(SUCCEEDED(hr) && m_CurrContext->m_pBackBuffer != 0);

		SAFE_RELEASE(pBackBufferTex);
	}

	if (m_CurrContext->m_pSwapChain && m_CurrContext->m_pBackBuffer)
	{
		assert(m_nRTStackLevel[0] == 0);		
		m_pBackBuffer = m_CurrContext->m_pBackBuffer;
		FX_SetRenderTarget(0, m_CurrContext->m_pBackBuffer, &m_DepthBufferOrig);
	}
#endif
	m_CurrContext->m_X = x;
	m_CurrContext->m_Y = y;
	m_CurrContext->m_Width = width;
	m_CurrContext->m_Height = height;
	m_width = width;
	m_height = height;
	SetViewport(x, y, width, height);
}
 
void CD3D9Renderer::ChangeLog()
{
#ifdef DO_RENDERLOG
  if (CV_r_log && !m_LogFile)
  {

    if (CV_r_log == 3)
      SetLogFuncs(true);

    m_LogFile = fxopen ("Direct3DLog.txt", "w");
    if (m_LogFile)
    {
      iLog->Log("Direct3D log file '%s' opened\n", "Direct3DLog.txt");
      char time[128];
      char date[128];

      _strtime( time );
      _strdate( date );

      fprintf(m_LogFile, "\n==========================================\n");
      fprintf(m_LogFile, "Direct3D Log file opened: %s (%s)\n", date, time);
      fprintf(m_LogFile, "==========================================\n");
    }
  }
  else
  if (!CV_r_log && m_LogFile)
  {
    SetLogFuncs(false);

    char time[128];
    char date[128];
    _strtime( time );
    _strdate( date );

    fprintf(m_LogFile, "\n==========================================\n");
    fprintf(m_LogFile, "Direct3D Log file closed: %s (%s)\n", date, time);
    fprintf(m_LogFile, "==========================================\n");

    fclose(m_LogFile);
    m_LogFile = NULL;
    iLog->Log("Direct3D log file '%s' closed\n", "Direct3DLog.txt");
  }

  if (CV_r_logTexStreaming && !m_LogFileStr)
  {
    m_LogFileStr = fxopen ("Direct3DLogStreaming.txt", "w");
    if (m_LogFileStr)
    {
      iLog->Log("Direct3D texture streaming log file '%s' opened\n", "Direct3DLogStreaming.txt");
      char time[128];
      char date[128];

      _strtime( time );
      _strdate( date );

      fprintf(m_LogFileStr, "\n==========================================\n");
      fprintf(m_LogFileStr, "Direct3D Textures streaming Log file opened: %s (%s)\n", date, time);
      fprintf(m_LogFileStr, "==========================================\n");
    }
  }
  else
  if (!CV_r_logTexStreaming && m_LogFileStr)
  {
    char time[128];
    char date[128];
    _strtime( time );
    _strdate( date );

    fprintf(m_LogFileStr, "\n==========================================\n");
    fprintf(m_LogFileStr, "Direct3D Textures streaming Log file closed: %s (%s)\n", date, time);
    fprintf(m_LogFileStr, "==========================================\n");

    fclose(m_LogFileStr);
    m_LogFileStr = NULL;
    iLog->Log("Direct3D texture streaming log file '%s' closed\n", "Direct3DLogStreaming.txt");
  }

  if (CV_r_logShaders && !m_LogFileSh)
  {
    m_LogFileSh = fxopen ("Direct3DLogShaders.txt", "w");
    if (m_LogFileSh)
    {
      iLog->Log("Direct3D shaders log file '%s' opened\n", "Direct3DLogShaders.txt");
      char time[128];
      char date[128];

      _strtime( time );
      _strdate( date );

      fprintf(m_LogFileSh, "\n==========================================\n");
      fprintf(m_LogFileSh, "Direct3D Shaders Log file opened: %s (%s)\n", date, time);
      fprintf(m_LogFileSh, "==========================================\n");
    }
  }
  else
  if (!CV_r_logShaders && m_LogFileSh)
  {
    char time[128];
    char date[128];
    _strtime( time );
    _strdate( date );

    fprintf(m_LogFileSh, "\n==========================================\n");
    fprintf(m_LogFileSh, "Direct3D Textures streaming Log file closed: %s (%s)\n", date, time);
    fprintf(m_LogFileSh, "==========================================\n");

    fclose(m_LogFileSh);
    m_LogFileSh = NULL;
    iLog->Log("Direct3D Shaders log file '%s' closed\n", "Direct3DLogShaders.txt");
  }
#endif
}

void CD3D9Renderer::PostMeasureOverdraw()
{
#if defined (DIRECT3D9) || defined (OPENGL)
  if (CV_r_measureoverdraw)
  {
    int iTmpX, iTmpY, iTempWidth, iTempHeight;
    GetViewport(&iTmpX, &iTmpY, &iTempWidth, &iTempHeight);   
    SetViewport(0, 0, m_width, m_height);   
    Set2DMode(true, 1, 1);

    int nOffs;
    struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs, POOL_P3F_COL4UB_TEX2F);
    vQuad[0].xyz.x = 0;
    vQuad[0].xyz.y = 0;
    vQuad[0].xyz.z = 1;
    vQuad[0].color.dcolor = (uint32)-1;
    vQuad[0].st[0] = 0;
    vQuad[0].st[1] = 0;

    vQuad[1].xyz.x = 1;
    vQuad[1].xyz.y = 0;
    vQuad[1].xyz.z = 1;
    vQuad[1].color.dcolor = (uint32)-1;
    vQuad[1].st[0] = 1;
    vQuad[1].st[1] = 0;

    vQuad[2].xyz.x = 1;
    vQuad[2].xyz.y = 1;
    vQuad[2].xyz.z = 1;
    vQuad[2].color.dcolor = (uint32)-1;
    vQuad[2].st[0] = 1;
    vQuad[2].st[1] = 1;

    vQuad[3].xyz.x = 0;
    vQuad[3].xyz.y = 1;
    vQuad[3].xyz.z = 1;
    vQuad[3].color.dcolor = (uint32)-1;
    vQuad[3].st[0] = 0;
    vQuad[3].st[1] = 1;

    SetCullMode(R_CULL_DISABLE);
    EF_SetState(GS_NODEPTHTEST);
    EnableTMU(true);
    CTexture::m_Text_White->Apply();
    UnlockVB();
    {
      uint nPasses;
      CShader *pSH = m_cEF.m_ShaderDebug;
      pSH->FXSetTechnique("ShowInstructions");
      pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
      pSH->FXBeginPass(0);
      STexState TexStateLinear = STexState(FILTER_LINEAR, true);

      if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
      {
        FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));

        STexState TexStatePoint = STexState(FILTER_POINT, true);
        EF_Commit();
        SDynTexture *pRT = new SDynTexture(m_width, m_height, eTF_A8R8G8B8, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "TempDebugRT");
        pRT->Update(m_width, m_height);
        CTexture::GetBackBuffer(pRT->m_pTexture, 0);

        pRT->Apply(0, CTexture::GetTexState(TexStatePoint));
        CTexture::m_Text_PaletteDebug->Apply(1, CTexture::GetTexState(TexStateLinear));

        m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, nOffs, 2);

        SAFE_DELETE(pRT);
      }
      Set2DMode(false, 1, 1);

      int nX = 800-100+2;
      int nY = 600-100+2;
      int nW = 96;
      int nH = 96;

      Draw2dImage(nX-2, nY-2, nW+4, nH+4, CTexture::m_Text_White->GetTextureID());

      Set2DMode(true, 800, 600);

      struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs, POOL_P3F_COL4UB_TEX2F);
      vQuad[0].xyz.x = nX;
      vQuad[0].xyz.y = nY;
      vQuad[0].xyz.z = 1;
      vQuad[0].color.dcolor = (uint32)-1;
      vQuad[0].st[0] = 0;
      vQuad[0].st[1] = 0;

      vQuad[1].xyz.x = nX + nW;
      vQuad[1].xyz.y = nY;
      vQuad[1].xyz.z = 1;
      vQuad[1].color.dcolor = (uint32)-1;
      vQuad[1].st[0] = 1;
      vQuad[1].st[1] = 0;

      vQuad[2].xyz.x = nX + nW;
      vQuad[2].xyz.y = nY + nH;
      vQuad[2].xyz.z = 1;
      vQuad[2].color.dcolor = (uint32)-1;
      vQuad[2].st[0] = 1;
      vQuad[2].st[1] = 1;

      vQuad[3].xyz.x = nX;
      vQuad[3].xyz.y = nY + nH;
      vQuad[3].xyz.z = 1;
      vQuad[3].color.dcolor = (uint32)-1;
      vQuad[3].st[0] = 0;
      vQuad[3].st[1] = 1;

      pSH->FXSetTechnique("InstructionsGrad");
      pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
      pSH->FXBeginPass(0);
      EF_Commit();

      CTexture::m_Text_PaletteDebug->Apply(0, CTexture::GetTexState(TexStateLinear));

      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, nOffs, 2);

      nX = nX * m_width / 800;
      nY = nY * m_height / 600;
      nW = 10 * m_width / 800;
      nH = 10 * m_height / 600;
      float color[4] = {1,1,1,1};
      if (CV_r_measureoverdraw == 1 || CV_r_measureoverdraw == 3)
      {
        Draw2dLabel(nX+nW, nY+nH-30, 1.2f, color, false, CV_r_measureoverdraw==1 ? "Pixel Shader:" : "Vertex Shader:");
        int n = 32 * CV_r_measureoverdrawscale;
        for (int i=0; i<8; i++)
        {
          char str[256];
          sprintf(str, "-- >%d instructions --", n);

          Draw2dLabel(nX+nW, nY+nH*(i+1), 1.2f, color, false,  str);
          n += 32 * CV_r_measureoverdrawscale;
        }
      }
      else
      {
        Draw2dLabel(nX+nW, nY+nH, 1.2f, color, false,  "-- 1 pass --");
        Draw2dLabel(nX+nW, nY+nH*2, 1.2f, color, false,  "-- 2 passes --");
        Draw2dLabel(nX+nW, nY+nH*3, 1.2f, color, false,  "-- 3 passes --");
        Draw2dLabel(nX+nW, nY+nH*4, 1.2f, color, false,  "-- 4 passes --");
        Draw2dLabel(nX+nW, nY+nH*5, 1.2f, color, false,  "-- 5 passes --");
        Draw2dLabel(nX+nW, nY+nH*6, 1.2f, color, false,  "-- 6 passes --");
        Draw2dLabel(nX+nW, nY+nH*7, 1.2f, color, false,  "-- 7 passes --");
        Draw2dLabel(nX+nW, nY+nH*8, 1.2f, color, false,  "-- >8 passes --");
      }
      //WriteXY(nX+10, nY+10, 1, 1,  1,1,1,1, "-- 10 instructions --");
    }
    Set2DMode(false, 1, 1);
  }
#endif
}

void CD3D9Renderer::BeginFrame()
{

  //////////////////////////////////////////////////////////////////////
  // Set up everything so we can start rendering
  //////////////////////////////////////////////////////////////////////

  assert(m_pd3dDevice);

  g_bProfilerEnabled = iSystem->GetIProfileSystem()->IsProfiling();

  PROFILE_FRAME(Screen_Begin);

  //////////////////////////////////////////////////////////////////////
  // Build the matrices
  //////////////////////////////////////////////////////////////////////

  m_matView->LoadIdentity();

  CheckDeviceLost();

  if (!m_bDeviceLost)
  {
    if (CV_r_gamma+m_fDeltaGamma != m_fLastGamma || CV_r_brightness != m_fLastBrightness || CV_r_contrast != m_fLastContrast)
      SetGamma(CV_r_gamma+m_fDeltaGamma, CV_r_brightness, CV_r_contrast, false);
  }

  if (m_bDeviceSupportsATOC)
  {
    m_RP.m_PersFlags2 &= ~RBPF2_ATOC;
   #if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
    m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y, 0); 
#endif
   #endif
  }

  CheckFSAAChange();
  if (!m_bDeviceSupportsInstancing)
  {
    if (CV_r_geominstancing)
    {
      iLog->Log("Device doesn't support HW geometry instancing (or it's disabled)");
      _SetVar("r_GeomInstancing", 0);
    }
  }
  if ((CV_r_shadersdynamicbranching!=0) != m_bUseDynBranching)
  {
    if (!m_bDeviceSupportsDynBranching)
    {
      if (CV_r_shadersdynamicbranching)
      {
        iLog->Log("Device doesn't support dynamic branching in shaders (or it's disabled)");
        _SetVar("r_ShadersDynamicBranching", 0);
      }
    }
    m_bUseDynBranching = CV_r_shadersdynamicbranching != 0;
    m_cEF.mfReloadAllShaders(1, SHGD_HW_DYN_BRANCHING);
  }
  if ((CV_r_shadersstaticbranching!=0) != m_bUseStatBranching)
  {
    m_bUseStatBranching = CV_r_shadersstaticbranching != 0;
    m_cEF.mfReloadAllShaders(1, SHGD_HW_STAT_BRANCHING);
  }
	if ((CV_r_usepom != 0) != m_bUsePOM)
	{
		m_bUsePOM = CV_r_usepom != 0;
		m_cEF.mfReloadAllShaders(1, SHGD_HW_ALLOW_POM);
	}

  if (CV_r_reloadshaders)
  {
    //exit(0);
    //ShutDown();
    //iConsole->Exit("Test");

    m_cEF.m_Bin.InvalidateCache();
    m_cEF.mfReloadAllShaders(CV_r_reloadshaders, 0);
    CV_r_reloadshaders = 0;
  }

  if (CV_r_usehwskinning != (int)m_bUseHWSkinning)
  {
    m_bUseHWSkinning = CV_r_usehwskinning != 0;
    CRendElement *pRE = CRendElement::m_RootGlobal.m_NextGlobal;
    for (pRE=CRendElement::m_RootGlobal.m_NextGlobal; pRE != &CRendElement::m_RootGlobal; pRE=pRE->m_NextGlobal)
    {
      if (pRE->mfIsHWSkinned())
        pRE->mfReset();
    }
  }

  if (!m_bEditor)
  {
#if defined (DIRECT3D10)
    if (m_bPendingSetWindow)
    {
      m_bPendingSetWindow = false;
      m_CVFullScreen->Set(0);
    }
#endif
    if (m_CVWidth && m_CVHeight && m_CVFullScreen && m_CVColorBits)
    {
      if (m_CVWidth->GetIVal() != m_width || m_CVHeight->GetIVal() != m_height || m_CVFullScreen->GetIVal() != (int)m_bFullScreen || m_CVColorBits->GetIVal() != m_cbpp)
        ChangeResolution(m_CVWidth->GetIVal(), m_CVHeight->GetIVal(), m_CVColorBits->GetIVal(), 75, m_CVFullScreen->GetIVal()!=0);
    }
    if (CV_r_vsync != m_VSync)
      EnableVSync(CV_r_vsync?true:false);
  }

  gRenDev->m_cEF.mfBeginFrame();


  // Always clear in wireframe mode
  if (m_polygon_mode ==R_WIREFRAME_MODE)
    m_bWireframeMode = 1;
  else
    m_bWireframeMode = 0;

  if (CV_r_PolygonMode!=m_polygon_mode)
  {
    SetPolygonMode(CV_r_PolygonMode);          
  }

	CRenderer::s_MotionBlurMode = CRenderer::CV_r_motionblur % 100;
	bool enableInMP = CRenderer::CV_r_motionblur >= 100;
	if (gEnv->bMultiplayer && !enableInMP)
		CRenderer::s_MotionBlurMode = 0;
#if !defined (DIRECT3D10)
	if( CRenderer::s_MotionBlurMode == 4 || CRenderer::s_MotionBlurMode == 3 ||CRenderer::s_MotionBlurMode == 2 )
		CRenderer::s_MotionBlurMode = 1;
#endif

  //////////////////////////////////////////////////////////////////////
  // Begin the scene
  //////////////////////////////////////////////////////////////////////

  SetMaterialColor(1,1,1,1);

  if (strcmp(CV_d3d9_texturefilter->GetString(), CTexture::m_GlobalTextureFilter.c_str()) || CV_r_texture_anisotropic_level != CTexture::m_TexStates[CTexture::m_nGlobalDefState].m_nAnisotropy)
    CTexture::SetDefTexFilter(CV_d3d9_texturefilter->GetString());

  ChangeLog ();

  ResetToDefault();

  CREImposter::m_PrevMemPostponed = CREImposter::m_MemPostponed;
  CREImposter::m_PrevMemUpdated = CREImposter::m_MemUpdated;
  CREImposter::m_MemPostponed = 0;
  CREImposter::m_MemUpdated = 0;

	m_nFrameID++;
  m_nFrameUpdateID++;
  m_RP.m_RealTime = iTimer->GetCurrTime();
  m_RP.m_PersFlags &= ~RBPF_HDR;

	CREOcclusionQuery::m_nQueriesPerFrameCounter = 0;
	CREOcclusionQuery::m_nReadResultNowCounter = 0;
	CREOcclusionQuery::m_nReadResultTryCounter = 0;

  if (CV_r_postprocess_effects)
  {
    m_RP.m_PersFlags |= RBPF_POSTPROCESS; // enable post processing
  }
  
  TArray<CRETempMesh *> *tm = &m_RP.m_TempMeshes[m_nFrameID & 1];
  for (uint i=0; i<tm->Num(); i++)
  {
    CRETempMesh *re = tm->Get(i);
    if (!re)
      continue;
    if (re->m_pVBuffer)
    {
      ReleaseBuffer(re->m_pVBuffer, re->m_nVertices);
      re->m_pVBuffer = NULL;
    }
    ReleaseIndexBuffer(&re->m_Inds, re->m_nIndices);
  }
  assert(m_matView->GetDepth() == 0);
  assert(m_matProj->GetDepth() == 0);

  tm->SetUse(0);
  g_SelectedTechs.resize(0);
  m_RP.m_CurTempMeshes = tm;
  m_RP.m_SysVertexPool.SetUse(0);
  m_RP.m_SysIndexPool.SetUse(0);

  if (CRenderer::CV_r_log)
    Logv(0, "******************************* BeginFrame %d ********************************\n", m_nFrameUpdateID);

  if (CRenderer::CV_r_logTexStreaming)
    LogStrv(0, "******************************* BeginFrame %d ********************************\n", m_nFrameUpdateID);

	if (CV_r_flush == 1)
		FlushHardware(false);

  if (!m_SceneRecurseCount)
  {
#if !defined(XENON)
 #if defined (DIRECT3D9) || defined (OPENGL)
    m_pd3dDevice->BeginScene();
 #endif
#else
    m_pd3dDevice->BeginScene();

    if (CV_d3d9_predicatedtiling > 1)
      m_pd3dDevice->SetPredication(CV_d3d9_predicatedtiling-1);

    if (!m_nRTStackLevel[0])
      FX_PushRenderTarget(0, CTexture::m_pBackBuffer, &m_DepthBufferOrig);

#endif
    m_SceneRecurseCount++;
  }
  EF_ClearBuffers(FRT_CLEAR, NULL);     
  m_nStencilMaskRef = 0;

#ifndef EXCLUDE_CRI_SDK
	CVideoPlayer::ProcessPerFrameUpdates();
#endif

  /*extern SDynTexture2 *gDT;
  extern int gnFrame;
  if (gDT)
  {
    static D3DSurface *pSurf = NULL;
    HRESULT hr;
    if (!pSurf)
    {
      hr = m_pd3dDevice->CreateRenderTarget(64, 64, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &pSurf, NULL);
    }
    int nFrame = m_nFrameSwapID;
    if ((1<<(nFrame&1)) & gnFrame)
    {
      SDynTexture2 *pDT = gDT;
      pDT->Apply(0);
      FX_PushRenderTarget(0, pSurf, &m_DepthBufferOrig);
      EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
      DrawFullScreenQuad(m_cEF.m_ShaderTreeSprites, "General_Debug", m_cEF.m_RTRect.x, m_cEF.m_RTRect.y, m_cEF.m_RTRect.x+m_cEF.m_RTRect.z, m_cEF.m_RTRect.y+m_cEF.m_RTRect.w);
      FX_PopRenderTarget(0);
      D3DLOCKED_RECT lr;
      pSurf->LockRect(&lr, NULL, 0);
      UCol *pData = (UCol *)lr.pBits;
      //for (int i=0; i<64*64; i++)
      //{
      UCol d = pData[32*64+32];
      if (d.bcolor[0] == 0xff)
      {
        //assert(0);
        int nnn = 0;
      }
      //}
      pSurf->UnlockRect();
    }
  }*/
}

bool CD3D9Renderer::CheckDeviceLost()
{
#if defined (DIRECT3D9) || defined (OPENGL)
#ifdef WIN32
  HRESULT hReturn;

  // Test the cooperative level to see if it's okay to render
  if (FAILED(hReturn = m_pd3dDevice->TestCooperativeLevel()))
  {
    // If the device was lost, do not render until we get it back
    if (D3DERR_DEVICELOST == hReturn)
    {
      RestoreGamma();
      m_bDeviceLost = true;
      return true;
    }
    // Check if the device needs to be reset.
    if (D3DERR_DEVICENOTRESET == hReturn)
    {
      m_bDeviceLost = true;
      if(FAILED(hReturn = DXUTReset3DEnvironment9()))
        return true;
      m_bDeviceLost = false;
      SetGamma(CV_r_gamma+m_fDeltaGamma, CV_r_brightness, CV_r_contrast, true);
      hReturn = m_pd3dDevice->BeginScene();
      if (m_bEnd)
        CTexture::Precache();
    }
  }
#endif //WIN32
#elif defined (DIRECT3D10)
#endif
  return false;
}

void CD3D9Renderer::FlushHardware(bool bIssueBeforeSync)
{
	PROFILE_FRAME(FlushHardware);

	if (CV_d3d9_null_ref_device != 0) // No flush for null device.
		return;

  if (m_bDeviceLost)
    return;

#ifdef XENON
  return;
#endif
  m_nQueryCount++;
  int nFr = m_nFrameUpdateID & 1;
//	if(!CRenderer::IsMultiGPUModeActive())
    nFr = 0;

  HRESULT hr;
  if (CV_r_flush) // == m_nQueryCount)
  {
    m_nQueryCount = 0;
    if (m_pQuery[nFr])
    {
			if (bIssueBeforeSync)
      {
#if defined (DIRECT3D9) || defined (OPENGL)
				m_pQuery[nFr]->Issue(D3DISSUE_END);
#elif defined (DIRECT3D10)
        m_pQuery[nFr]->End();
#endif
      }

      BOOL bQuery = false;
      float fTime = iTimer->GetAsyncCurTime();
      bool bInfinite = false;
      do
      {
        float fDif = iTimer->GetAsyncCurTime() - fTime;
        if (fDif > 5.0f)
        {
          // 5 seconds in the loop
          bInfinite = true;
          break;
        }
#if defined (DIRECT3D9) || defined (OPENGL)
        hr = m_pQuery[nFr]->GetData((void *)&bQuery, sizeof(BOOL), D3DGETDATA_FLUSH);
#elif defined (DIRECT3D10)
        hr = m_pQuery[nFr]->GetData((void *)&bQuery, sizeof(BOOL), 0);
#endif
      } while(hr == S_FALSE);

      if (bInfinite)
        iLog->Log("Error: Seems like infinite loop in GPU sync query");

			if (!bIssueBeforeSync)
      {
#if defined (DIRECT3D9) || defined (OPENGL)
				m_pQuery[nFr]->Issue(D3DISSUE_END);
#elif defined (DIRECT3D10)
        m_pQuery[nFr]->End();
#endif
      }
    }
#if defined (DIRECT3D9) || defined (OPENGL)
    else
    {
      IDirect3DSurface9 * pTar = GetBackSurface();
      if (pTar)
      {
        D3DLOCKED_RECT lockedRect;
        RECT sourceRect;
        sourceRect.bottom = 1;
        sourceRect.top = 0;
        sourceRect.left = 0;
        sourceRect.right = 4;
        hr = pTar->LockRect(&lockedRect,&sourceRect,D3DLOCK_READONLY);
        if (!FAILED(hr))
        {
          volatile unsigned long a;
          memcpy((void *)&a,(unsigned char*)lockedRect.pBits,sizeof(a));
          hr = pTar->UnlockRect();
        }
      }
    }
#endif
  }
}

//#define CAPTURE_TO_DDS
#define DEPTH_BUFFER_SCALE 1024.0f

bool CD3D9Renderer::CaptureMiscBuffersToFiles(const char* pFilePath)
{
	char filename[ICryPak::g_nMaxPath];

	// Tbyte TODO DirectX10
#if defined (DIRECT3D9) || defined (OPENGL)
	struct SCaptureFormatInfo
	{
		const char* pExt;
		D3DXIMAGE_FILEFORMAT fmt;
	};

	const SCaptureFormatInfo targetformats[] =
	{
#ifdef CAPTURE_TO_DDS
		{".dds", D3DXIFF_DDS},
		{"HDR.dds", D3DXIFF_DDS},
		{"Z.dds", D3DXIFF_DDS},
		{"AO.dds", D3DXIFF_DDS},
		{"Shadow0.dds", D3DXIFF_DDS},
		{"Shadow1.dds", D3DXIFF_DDS},
		{"Shadow2.dds", D3DXIFF_DDS},
		{"Shadow3.dds", D3DXIFF_DDS},
		{"Shadow4.dds", D3DXIFF_DDS},
		{"Shadow5.dds", D3DXIFF_DDS},
		{"Shadow6.dds", D3DXIFF_DDS},
		{"Shadow7.dds", D3DXIFF_DDS},
#else
		{".tga", D3DXIFF_TGA},
		{"HDR.hdr", D3DXIFF_HDR},
		{"Z.hdr", D3DXIFF_HDR},
		{"AO.tga", D3DXIFF_TGA},
		{"Shadow0.tga", D3DXIFF_TGA},
		{"Shadow1.tga", D3DXIFF_TGA},
		{"Shadow2.tga", D3DXIFF_TGA},
		{"Shadow3.tga", D3DXIFF_TGA},
		{"Shadow4.tga", D3DXIFF_TGA},
		{"Shadow5.tga", D3DXIFF_TGA},
		{"Shadow6.tga", D3DXIFF_TGA},
		{"Shadow7.tga", D3DXIFF_TGA},
#endif
	};

	for (int i=0; i<sizeof(m_pCaptureFrameSurf)/sizeof(m_pCaptureFrameSurf[0]); ++i)
	{
		IDirect3DSurface9* pSrcSurface = NULL;
		IDirect3DTexture9* pSrcTexture = NULL;

		strcpy(filename, pFilePath);
		strcat(filename, targetformats[i].pExt);

		if (0 == i)
			m_pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pSrcSurface);
		else if (1 == i)
			pSrcTexture = CTexture::m_Text_HDRTarget ? (IDirect3DTexture9*) CTexture::m_Text_HDRTarget->GetDeviceTexture() : NULL;
		else if (2 == i)
			pSrcTexture = CTexture::m_Text_ZTarget ? (IDirect3DTexture9*) CTexture::m_Text_ZTarget->GetDeviceTexture() : NULL;
		else if (3 == i)
			pSrcTexture = CTexture::m_Text_AOTarget ? (IDirect3DTexture9*) CTexture::m_Text_AOTarget->GetDeviceTexture() : NULL;
		else if (4 <= i)
		{
			if (m_pCaptureScreenShadowMap[i-4] && m_ValidCaptureScreenShadowMap[i-4])
			{
				D3DXSaveSurfaceToFile(filename, targetformats[i].fmt, m_pCaptureScreenShadowMap[i-4], 0, NULL);
				continue;
			}
		}
		if (pSrcTexture)
			pSrcTexture->GetSurfaceLevel(0, &pSrcSurface);


		bool frameCaptureSuccessful = false;

		if (pSrcSurface)
		{
			D3DSURFACE_DESC srcDesc;
			pSrcSurface->GetDesc(&srcDesc);

			if (srcDesc.MultiSampleType == D3DMULTISAMPLE_NONE)
			{
				bool needRealloc(true);
				if (m_pCaptureFrameSurf[i])
				{
					D3DSURFACE_DESC dstDesc;
					m_pCaptureFrameSurf[i]->GetDesc(&dstDesc);
					if (dstDesc.Format == srcDesc.Format && dstDesc.Width == srcDesc.Width && dstDesc.Height == srcDesc.Height)
						needRealloc = false;
				}

				if (needRealloc)
				{
					SAFE_RELEASE(m_pCaptureFrameSurf[i]);
					m_pd3dDevice->CreateOffscreenPlainSurface(srcDesc.Width, srcDesc.Height, srcDesc.Format, D3DPOOL_SYSTEMMEM, &m_pCaptureFrameSurf[i], 0);
				}

				if (m_pCaptureFrameSurf[i] && SUCCEEDED(m_pd3dDevice->GetRenderTargetData(pSrcSurface, m_pCaptureFrameSurf[i])))
				{
					RECT rect;
					rect.left = 0; 
					rect.right = GetWidth(); 
					rect.top = 0; 
					rect.bottom = GetHeight();

#ifdef DEPTH_BUFFER_SCALE
					if (2 == i)
					{
						D3DLOCKED_RECT lockrect;
						m_pCaptureFrameSurf[i]->LockRect(&lockrect, &rect, 0);
						char* ptr = reinterpret_cast<char*>(lockrect.pBits);
						for (int y=0; y<rect.bottom; ++y)
						{
							for (int x=0; x<rect.right; ++x)
								reinterpret_cast<float*>(ptr)[x] = DEPTH_BUFFER_SCALE*reinterpret_cast<float*>(ptr)[x];
							ptr += lockrect.Pitch;
						}
						m_pCaptureFrameSurf[i]->UnlockRect();
					}
#endif
					D3DXSaveSurfaceToFile(filename, targetformats[i].fmt, m_pCaptureFrameSurf[i], 0, &rect);
					frameCaptureSuccessful = true;
				}
				SAFE_RELEASE(pSrcSurface);
			}
			if (!frameCaptureSuccessful)
				return false;
		}
	}

	for (int i=0; i<sizeof(m_ValidCaptureScreenShadowMap) / sizeof(m_ValidCaptureScreenShadowMap[0]); ++i)
		m_ValidCaptureScreenShadowMap[i] = false;
#endif

	return true;
}

bool CD3D9Renderer::CaptureFrameBufferToFile(const char* pFilePath, bool keepIntCaptureBuffer)
{
	assert(pFilePath);
	if (!pFilePath)
		return false;

#if defined (DIRECT3D9) || defined (OPENGL)
	struct SCaptureFormatInfo { const char* pExt;  D3DXIMAGE_FILEFORMAT fmt; };
	const SCaptureFormatInfo c_captureFormats[] = 
		{ {"hdr", D3DXIFF_HDR}, {"jpg", D3DXIFF_JPG}, {"bmp", D3DXIFF_BMP}, {"tga", D3DXIFF_TGA} };
#elif defined(DIRECT3D10)
	struct SCaptureFormatInfo { const char* pExt;  D3DX10_IMAGE_FILE_FORMAT fmt; };
	const SCaptureFormatInfo c_captureFormats[] = 
		{ {"jpg", D3DX10_IFF_JPG}, {"bmp", D3DX10_IFF_BMP}, {"tif", D3DX10_IFF_TIFF} };
#endif

	const char* pReqFileFormatExt(fpGetExtension(pFilePath));

	// resolve file format from file path
	int fmtIdx(-1);
	if (pReqFileFormatExt)
	{
		++pReqFileFormatExt; // skip '.'
		for (int i(0); i < sizeof(c_captureFormats) / sizeof(c_captureFormats[0]); ++i)
		{
			if (!stricmp(pReqFileFormatExt, c_captureFormats[i].pExt))
			{
				fmtIdx = i;
				break;
			}
		}
	}

	if (fmtIdx < 0)
	{
		if (iLog)
			iLog->Log("Warning: Captured frame cannot be saved as \"%s\" format is not supported!\n", pReqFileFormatExt);
		return false;
	}	

#if defined (DIRECT3D9) || defined (OPENGL)
	// capture regular or HDR frames?
	bool captureHDR(c_captureFormats[fmtIdx].fmt == D3DXIFF_HDR);

	// get access to frame
	IDirect3DSurface9* pSrcSurface(0);
	if (captureHDR)
	{
		IDirect3DTexture9* pHDRTargetTex(0);
		pHDRTargetTex = CTexture::m_Text_HDRTarget ? (IDirect3DTexture9*) CTexture::m_Text_HDRTarget->GetDeviceTexture() : 0;			
		if (pHDRTargetTex)
			pHDRTargetTex->GetSurfaceLevel(0, &pSrcSurface);
	}
	else
		m_pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pSrcSurface);

	// capture frame
	bool frameCaptureSuccessful(false);
	if (pSrcSurface)
	{
		D3DSURFACE_DESC srcDesc;
		pSrcSurface->GetDesc(&srcDesc);

		if (srcDesc.MultiSampleType == D3DMULTISAMPLE_NONE)
		{
			bool needRealloc(true);
			if (m_pCaptureFrameSurf[0])
			{
				D3DSURFACE_DESC dstDesc;
				m_pCaptureFrameSurf[0]->GetDesc(&dstDesc);
				if (dstDesc.Format == srcDesc.Format && dstDesc.Width == srcDesc.Width && dstDesc.Height == srcDesc.Height)
					needRealloc = false;
			}

			if (needRealloc)
			{
				SAFE_RELEASE(m_pCaptureFrameSurf[0]);
				m_pd3dDevice->CreateOffscreenPlainSurface(srcDesc.Width, srcDesc.Height, srcDesc.Format, D3DPOOL_SYSTEMMEM, &m_pCaptureFrameSurf[0], 0);
			}

			if (m_pCaptureFrameSurf[0] && SUCCEEDED(m_pd3dDevice->GetRenderTargetData(pSrcSurface, m_pCaptureFrameSurf[0])))
				frameCaptureSuccessful = true;
		}
	}

	// save captured frame
	if (frameCaptureSuccessful)
	{
		RECT rect;
		rect.left = 0; 
		rect.right = GetWidth(); 
		rect.top = 0; 
		rect.bottom = GetHeight();

		D3DXSaveSurfaceToFile(pFilePath, c_captureFormats[fmtIdx].fmt, m_pCaptureFrameSurf[0], 0, &rect);
	}

	// release ref count of src surface
	SAFE_RELEASE(pSrcSurface);

	// release internal capture buffer if requested
	if (!keepIntCaptureBuffer)
		SAFE_RELEASE(m_pCaptureFrameSurf[0]);

	return frameCaptureSuccessful;
#elif defined (DIRECT3D10)
	assert(m_pBackBuffer);
	assert(!m_bEditor || m_CurrContext->m_pBackBuffer == m_pBackBuffer);

	bool frameCaptureSuccessful(false);	
	ID3D10Texture2D* pBackBufferTex(0);

	m_pBackBuffer->GetResource((ID3D10Resource**) &pBackBufferTex);	
	if (pBackBufferTex)
	{
		HRESULT hr(D3DX10SaveTextureToFile(pBackBufferTex, c_captureFormats[fmtIdx].fmt, pFilePath));
		frameCaptureSuccessful = SUCCEEDED(hr);
	}

	SAFE_RELEASE(pBackBufferTex);

	return frameCaptureSuccessful;
#else
	return false;
#endif
}

void CD3D9Renderer::CaptureFrameBuffer()
{
	// cache console vars
	if (!CV_capture_frames || !CV_capture_folder || !CV_capture_file_format)
	{
		ISystem* pSystem(GetISystem());
		if (!pSystem)
			return;

		IConsole* pConsole(pSystem->GetIConsole());
		if (!pConsole)
			return;

		CV_capture_frames = !CV_capture_frames ? pConsole->GetCVar("capture_frames") : CV_capture_frames;
		CV_capture_folder = !CV_capture_folder ? pConsole->GetCVar("capture_folder") : CV_capture_folder;
		CV_capture_file_format = !CV_capture_file_format ? pConsole->GetCVar("capture_file_format") : CV_capture_file_format;

		if (!CV_capture_frames || !CV_capture_folder || !CV_capture_file_format)
			return;
	}

	int frameNum(CV_capture_frames->GetIVal());
	if (frameNum > 0)
	{
		char path[ICryPak::g_nMaxPath];
		path[sizeof(path) - 1] = 0;
		gEnv->pCryPak->AdjustFileName(CV_capture_folder->GetString(), path, ICryPak::FLAGS_NO_MASTER_FOLDER_MAPPING | ICryPak::FLAGS_FOR_WRITING);
		gEnv->pCryPak->MakeDir(path);

		size_t pathLen = strlen(path);
		if (CV_capture_misc_render_buffers)
			snprintf(&path[pathLen], sizeof(path) - 1 - pathLen, "\\Frame%06d", frameNum - 1);
		else
		snprintf(&path[pathLen], sizeof(path) - 1 - pathLen, "\\Frame%06d.%s", frameNum - 1, CV_capture_file_format->GetString());	
		CV_capture_frames->Set(frameNum + 1);

		if (CV_capture_misc_render_buffers)
		{
			if (!CaptureMiscBuffersToFiles(path))
			{
				if (iLog)
					iLog->Log("Warning: Frame capture failed!\n");
				CV_capture_frames->Set(0); // disable capturing
			}
		}
		else if (!CaptureFrameBufferToFile(path, true))
		{
			if (iLog)
				iLog->Log("Warning: Frame capture failed!\n");
			CV_capture_frames->Set(0); // disable capturing
		}
	}
#if defined (DIRECT3D9) || defined (OPENGL)
	else
	{
		for (int i=0; i<sizeof(m_pCaptureFrameSurf) / sizeof(m_pCaptureFrameSurf[0]); ++i)
			SAFE_RELEASE(m_pCaptureFrameSurf[i]);
	}
#endif
}

static float LogMap( const float fA )
{
	return logf(fA)/logf(2.0f)+5.5f;		// offset to bring values <0 in viewable range
}

void CD3D9Renderer::DebugDrawRect( float x1,float y1,float x2,float y2,float *fColor )
{
#if defined(PS3)
  float fColorRev[4] = { fColor[3],fColor[2],fColor[1],fColor[0] };
  fColor = fColorRev;
#endif
  SetMaterialColor( fColor[0],fColor[1],fColor[2],fColor[3] );
  int w = GetWidth();
  int h = GetHeight();
  float dx = 1.0f/w;
  float dy = 1.0f/h;
  x1 *= dx; x2 *= dx;
  y1 *= dy; y2 *= dy;

  ColorB col((uint8)(fColor[0]*255.0f),(uint8)(fColor[1]*255.0f),(uint8)(fColor[2]*255.0f),(uint8)(fColor[3]*255.0f));

  IRenderAuxGeom *pAux = GetIRenderAuxGeom();
  SAuxGeomRenderFlags flags = pAux->GetRenderFlags();
  flags.SetMode2D3DFlag(e_Mode2D);
  pAux->SetRenderFlags(flags);
  pAux->DrawLine( Vec3(x1,y1,0),col,Vec3(x2,y1,0),col );
  pAux->DrawLine( Vec3(x1,y2,0),col,Vec3(x2,y2,0),col );
  pAux->DrawLine( Vec3(x1,y1,0),col,Vec3(x1,y2,0),col );
  pAux->DrawLine( Vec3(x2,y1,0),col,Vec3(x2,y2,0),col );
}

void CD3D9Renderer::DebugDrawStats1()
{
  const int nYstep = 10;
  int nY = 30; // initial Y pos
  int nX = 20; // initial X pos

  ColorF col = Col_Yellow;
  Draw2dLabel(nX, nY, 2.0f, &col.r, false, "Per-frame stats:");

  col = Col_White;
  nX += 10; nY += 25;
  //DebugDrawRect(nX-2, nY, nX+180, nY+150, &col.r);
  Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Draw-calls:");

  nX += 5; nY += 10;
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "General: %d (%d polys)", m_RP.m_PS.m_nDIPs[EFSLIST_GENERAL], m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL]);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Decals: %d (%d polys)", m_RP.m_PS.m_nDIPs[EFSLIST_DECAL], m_RP.m_PS.m_nPolygons[EFSLIST_DECAL]);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Terrain layers: %d (%d polys)", m_RP.m_PS.m_nDIPs[EFSLIST_TERRAINLAYER], m_RP.m_PS.m_nPolygons[EFSLIST_TERRAINLAYER]);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Transparent: %d (%d polys)", m_RP.m_PS.m_nDIPs[EFSLIST_TRANSP], m_RP.m_PS.m_nPolygons[EFSLIST_TRANSP]);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Shadow-gen: %d (%d polys)", m_RP.m_PS.m_nDIPs[EFSLIST_SHADOW_GEN], m_RP.m_PS.m_nPolygons[EFSLIST_SHADOW_GEN]);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Shadow-pass: %d (%d polys)", m_RP.m_PS.m_nDIPs[EFSLIST_SHADOW_PASS], m_RP.m_PS.m_nPolygons[EFSLIST_SHADOW_PASS]);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Water: %d (%d polys)", m_RP.m_PS.m_nDIPs[EFSLIST_WATER], m_RP.m_PS.m_nPolygons[EFSLIST_WATER]);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Imposters: %d (Updates: %d)", m_RP.m_PS.m_NumCloudImpostersDraw, m_RP.m_PS.m_NumCloudImpostersUpdates);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Sprites: %d (%d dips, %d updates, %d polys)", m_RP.m_PS.m_NumSprites, m_RP.m_PS.m_NumSpriteDIPS, m_RP.m_PS.m_NumSpriteUpdates, m_RP.m_PS.m_NumSpritePolys);

  Draw2dLabel(nX-5, nY+20, 1.4f, &col.r, false, "Total: %d (%d polys)", GetCurrentNumberOfDrawCalls(), GetPolyCount());

  col = Col_Cyan;
  nX -= 5; nY += 45;
  //DebugDrawRect(nX-2, nY, nX+180, nY+160, &col.r);
  Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Device resource switches:");

  nX += 5; nY += 10;
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "VShaders: %d (%d unique)", m_RP.m_PS.m_NumVShadChanges, m_RP.m_PS.m_NumVShaders);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "PShaders: %d (%d unique)", m_RP.m_PS.m_NumPShadChanges, m_RP.m_PS.m_NumPShaders);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Textures: %d (%d unique)", m_RP.m_PS.m_NumTextChanges, m_RP.m_PS.m_NumTextures);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "RT's: %d (%d unique), cleared: %d times, copies: %d, fsaa copies: %d", m_RP.m_PS.m_NumRTChanges, m_RP.m_PS.m_NumRTs, m_RP.m_PS.m_RTCleared, m_RP.m_PS.m_RTCopied, m_RP.m_PS.m_RTCopiedFSAA);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "States: %d", m_RP.m_PS.m_NumStateChanges);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Batches: %d", m_RP.m_PS.m_NumRendBatches);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Instances: %d", m_RP.m_PS.m_NumRendInstances);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Light setups: %d", m_RP.m_PS.m_NumLightSetups);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "HW Instances: DIP's: %d, Instances: %d (polys: %d/%d)", m_RP.m_PS.m_RendHWInstancesDIPs, m_RP.m_PS.m_NumRendHWInstances, m_RP.m_PS.m_RendHWInstancesPolysOne, m_RP.m_PS.m_RendHWInstancesPolysAll);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Skinned instances: %d", m_RP.m_PS.m_NumRendSkinnedObjects);

  CHWShader_D3D *ps = (CHWShader_D3D *)m_RP.m_PS.m_pMaxPShader;
  CHWShader_D3D::SHWSInstance *pInst = (CHWShader_D3D::SHWSInstance *)m_RP.m_PS.m_pMaxPSInstance;
  if (ps)
    Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "MAX PShader: %s (instructions: %d, lights: %d)", ps->GetName(), pInst->m_nInstructions, pInst->m_LightMask & 0xf);
  CHWShader_D3D *vs = (CHWShader_D3D *)m_RP.m_PS.m_pMaxVShader;
  pInst = (CHWShader_D3D::SHWSInstance *)m_RP.m_PS.m_pMaxVSInstance;
  if (vs)
    Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "MAX VShader: %s (instructions: %d, lights: %d)", vs->GetName(), pInst->m_nInstructions, pInst->m_LightMask & 0xf);

  col = Col_Green;
  nX -= 5; nY += 35;
  //DebugDrawRect(nX-2, nY, nX+180, nY+160, &col.r);
  Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Device resource sizes:");

  nX += 5; nY += 10;
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Managed textures: %.3f Mb", m_RP.m_PS.m_ManagedTexturesSize/1024.0f/1024.0f);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "RT textures: Used: %.3f Mb, Updated: %.3f Mb, Cleared: %.3f Mb, Copied: %.3f Mb", m_RP.m_PS.m_DynTexturesSize/1024.0f/1024.0f, m_RP.m_PS.m_RTSize/1024.0f/1024.0f, m_RP.m_PS.m_RTClearedSize/1024.0f/1024.0f, m_RP.m_PS.m_RTCopiedSize/1024.0f/1024.0f);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Meshes updated: Static: %.3f Mb, Dynamic: %.3f Mb", m_RP.m_PS.m_MeshUpdateBytes/1024.0f/1024.0f, m_RP.m_PS.m_DynMeshUpdateBytes/1024.0f/1024.0f);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Cloud textures updated: %.3f Mb", m_RP.m_PS.m_CloudImpostersSizeUpdate/1024.0f/1024.0f);


  nY = 30;  // initial Y pos
  nX = 440; // initial X pos
  col = Col_Yellow;
  Draw2dLabel(nX, nY, 2.0f, &col.r, false, "Global stats:");

  col = Col_YellowGreen;
  nX += 10; nY += 55;
  Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Mesh size:");

  CRenderMesh *pRM = CRenderMesh::m_Root.m_Prev;
  int nMemApp = 0;
  int nMemDevVB = 0;
  int nMemDevIB = 0;
  int nMemDevVBPool = 0;
  int nMemDevIBPool = 0;
  while (pRM != &CRenderMesh::m_Root)
  {
    nMemApp += pRM->Size(0);
    nMemDevVB += pRM->Size(1);
    nMemDevIB += pRM->Size(2);
    pRM = pRM->m_Prev;
  }
  TVertPool *Pool;
  for (Pool=sVertPools; Pool; Pool=Pool->Next)
  {
    if (Pool->m_pVB)
      nMemDevVBPool += Pool->m_nBufSize;
  }
  for (Pool=sIndexPools; Pool; Pool=Pool->Next)
  {
    if (Pool->m_pVB)
      nMemDevIBPool += Pool->m_nBufSize;
  }
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Static: (app: %.3f Mb, dev VB: %.3f Mb, dev IB: %.3f Mb, dev VB pools: %.3f, dev IB pools: %.3f)", nMemApp/1024.0f/1024.0f, nMemDevVB/1024.0f/1024.0f, nMemDevIB/1024.0f/1024.0f, nMemDevVBPool/1024.0f/1024.0f, nMemDevIBPool/1024.0f/1024.0f);

  nMemDevVB = 0;
  nMemDevIB = 0;
  nMemApp = 0;
  int i, j;
  for (i=0; i<POOL_MAX; i++)
  {
    int nVertSize = 0;
    switch (i)
    {
    case POOL_P3F_COL4UB_TEX2F:
      nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F);
      break;
    case POOL_P3F_TEX2F:
      nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_TEX2F);
      break;
    case POOL_P3F:
      nVertSize = sizeof(struct_VERTEX_FORMAT_P3F);
      break;
    case POOL_TRP3F_COL4UB_TEX2F:
      nVertSize = sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F);
      break;
    case POOL_TRP3F_TEX2F_TEX3F:
      nVertSize = sizeof(struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F);
      break;
    case POOL_P3F_TEX3F:
      nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_TEX3F);
      break;
    case POOL_P3F_TEX2F_TEX3F:
      nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F);
      break;
      default:
        assert(0);
    }
    nMemDevVB += m_nVertsDMesh[i] * nVertSize;
  }
  nMemDevIB += m_nIndsDMesh * sizeof(short);
  nMemApp += m_RP.m_SizeSysArray;
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Dynamic: (app: %.3f Mb, dev VB: %.3f Mb, dev IB: %.3f Mb)", nMemApp/1024.0f/1024.0f, nMemDevVB/1024.0f/1024.0f, nMemDevIB/1024.0f/1024.0f, nMemDevVBPool/1024.0f/1024.0f);

  CCryName Name;
  SResourceContainer *pRL;
  int n = 0;
  int nSize = 0;
  Name = CShader::mfGetClassName();
  pRL = CBaseResource::GetResourcesForClass(Name);
  if (pRL)
  {
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
    {
      CShader *sh = (CShader *)itor->second;
      if (!sh)
        continue;
      nSize += sh->Size(0);
      n++;
    }
  }
  nY += nYstep;
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "FX Shaders: %d (size: %.3f Mb)", n, nSize/1024.0f/1024.0f);

  nSize = 0;
  n = 0;
  for (i=0; i<CShader::m_ShaderResources_known.Num(); i++)
  {
    SRenderShaderResources *pSR = CShader::m_ShaderResources_known[i];
    if (!pSR)
      continue;
    nSize += pSR->Size();
    n++;
  }
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Shader resources: %d (size: %.3f Mb)", n, nSize/1024.0f/1024.0f);

  int nSharedVSInst = 0;
  int nSharedPSInst = 0;
  int nSharedVSDev = 0;
  int nSharedPSDev = 0;
  TArray<void *> ShadersVS;
  TArray<void *> ShadersPS;
  CHWShader_D3D::InstanceMapItor itInst;
  for (itInst=CHWShader_D3D::m_SharedInsts.begin(); itInst!=CHWShader_D3D::m_SharedInsts.end(); itInst++)
  {
    CHWShader_D3D::SHWSSharedList *pInst = itInst->second;
    for (i=0; i<pInst->m_SharedInsts.Num(); i++)
    {
      CHWShader_D3D::SHWSSharedInstance *pSI = &pInst->m_SharedInsts[i];
      for (j=0; j<pSI->m_Insts.Num(); j++)
      {
        CHWShader_D3D::SHWSInstance *p = &pSI->m_Insts[j];
        if (p->m_eProfileType <= eHWSP_VS_Auto)
        {
          nSharedVSInst++;
          if (p->m_Handle.m_pShader)
          {
            for (n=0; n<ShadersVS.Num(); n++)
            {
              if (ShadersVS[n] == p->m_Handle.m_pShader->m_pHandle)
                break;
            }
            if (n == ShadersVS.Num())
            {
              ShadersVS.AddElem(p->m_Handle.m_pShader->m_pHandle);
            }
          }
        }
        else
        {
          nSharedPSInst++;
          if (p->m_Handle.m_pShader)
          {
            for (n=0; n<ShadersPS.Num(); n++)
            {
              if (ShadersPS[n] == p->m_Handle.m_pShader->m_pHandle)
                break;
            }
            if (n == ShadersPS.Num())
            {
              ShadersPS.AddElem(p->m_Handle.m_pShader->m_pHandle);
            }
          }
        }
      }
    }

  }
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Shared VShader instances: %d, Device VShaders: %d", nSharedVSInst, ShadersVS.Num());
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Shared PShader instances: %d, Device PShaders: %d", nSharedPSInst, ShadersPS.Num());

  nSize = 0;
  n = 0;
  int nInsts = 0;
  ShadersVS.SetUse(0);
  Name = CHWShader::mfGetClassName(eHWSC_Vertex);
  pRL = CBaseResource::GetResourcesForClass(Name);
  if (pRL)
  {
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
    {
      CHWShader *vs = (CHWShader *)itor->second;
      if (!vs)
        continue;
      nSize += vs->Size();
      n++;

      CHWShader_D3D *pD3D = (CHWShader_D3D *)vs;
      nInsts += pD3D->m_Insts.Num();
      for (i=0; i<pD3D->m_Insts.Num(); i++)
      {
        CHWShader_D3D::SHWSInstance *pInst = &pD3D->m_Insts[i];
        if (pInst->m_Handle.m_pShader)
        {
          for (j=0; j<ShadersVS.Num(); j++)
          {
            if (ShadersVS[j] == pInst->m_Handle.m_pShader->m_pHandle)
              break;
          }
          if (j == ShadersVS.Num())
          {
            ShadersVS.AddElem(pInst->m_Handle.m_pShader->m_pHandle);
          }
        }
      }
    }
  }
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "VShaders: %d (size: %.3f Mb), Instances: %d, Device VShaders: %d", n, nSize/1024.0f/1024.0f, nInsts, ShadersVS.Num());

  ShadersPS.SetUse(0);
  nInsts = 0;
  Name = CHWShader::mfGetClassName(eHWSC_Pixel);
  pRL = CBaseResource::GetResourcesForClass(Name);
  if (pRL)
  {
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
    {
      CHWShader *ps = (CHWShader *)itor->second;
      if (!ps)
        continue;
      nSize += ps->Size();
      n++;

      CHWShader_D3D *pD3D = (CHWShader_D3D *)ps;
      nInsts += pD3D->m_Insts.Num();
      for (i=0; i<pD3D->m_Insts.Num(); i++)
      {
        CHWShader_D3D::SHWSInstance *pInst = &pD3D->m_Insts[i];
        if (pInst->m_Handle.m_pShader)
        {
          for (j=0; j<ShadersPS.Num(); j++)
          {
            if (ShadersPS[j] == pInst->m_Handle.m_pShader->m_pHandle)
              break;
          }
          if (j == ShadersPS.Num())
          {
            ShadersPS.AddElem(pInst->m_Handle.m_pShader->m_pHandle);
          }
        }
      }
    }
  }
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "PShaders: %d (size: %.3f Mb), Instances: %d, Device PShaders: %d", n, nSize/1024.0f/1024.0f, nInsts, ShadersPS.Num());

  FXShaderCacheItor FXitor;
  int nCache = 0;
  nSize = 0;
  for (FXitor=CHWShader::m_ShaderCache.begin(); FXitor!=CHWShader::m_ShaderCache.end(); FXitor++)
  {
    SShaderCache *sc = FXitor->second;
    if (!sc)
      continue;
    nCache++;
    nSize += sc->Size();
  }
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Shader Cache: %d (size: %.3f Mb)", nCache, nSize/1024.0f/1024.0f);

  nSize = 0;
  n = 0;
  CRendElement *pRE = CRendElement::m_RootGlobal.m_NextGlobal;
  while (pRE != &CRendElement::m_RootGlobal)
  {
    n++;
    nSize += pRE->Size();
    pRE = pRE->m_NextGlobal;
  }
  nY += nYstep;
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Render elements: %d (size: %.3f Mb)", n, nSize/1024.0f/1024.0f);

  int nSAll = 0;
  int nSOneMip = 0;
  int nSNM = 0;
  int nSRT = 0;
  int nObjSize = 0;
  int nStreamed = 0;
  int nStreamedSys = 0;
  int nStreamedUnload = 0;
  n = 0;
  Name = CTexture::mfGetClassName();
  pRL = CBaseResource::GetResourcesForClass(Name);
  if (pRL)
  {
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
    {
      CTexture *tp = (CTexture *)itor->second;
      if (!tp || tp->IsNoTexture())
        continue;
      n++;
      nObjSize += tp->GetSize();
      int nS = tp->GetDeviceDataSize();
      if (tp->IsStreamed())
      {
        int nSizeSys = tp->GetDataSize();
        if (tp->IsUnloaded())
        {
          assert(nS == 0);
          nStreamedUnload += nSizeSys;
        }
        else
          nStreamedSys += nSizeSys;
        nStreamed += nS;
      }
      if (!nS)
        continue;
      if (tp->GetFlags() & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET))
        nSRT += nS;
      else
      {
        if (!tp->IsStreamed())
        {
          int nnn = 0;
        }
        if (tp->GetName()[0] != '$' && tp->GetNumMips() <= 1)
          nSOneMip += nS;
        if (tp->GetFlags() & FT_TEX_NORMAL_MAP)
          nSNM += nS;
        else
          nSAll += nS;
      }
    }
  }
  nY += nYstep;
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, "Textures: %d, ObjSize: %.3f Mb...", n, nObjSize/1024.0f/1024.0f);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, " Managed Size: %.3f Mb (Normals: %.3f Mb + Other: %.3f Mb), One mip: %.3f", (nSNM+nSAll)/1024.0f/1024.0f, nSNM/1024.0f/1024.0f, nSAll/1024.0f/1024.0f, nSOneMip/1024.0f/1024.0f);
  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, " Streamed Size: Video: %.3f, System: %.3f, Unloaded: %.3f", nStreamed/1024.0f/1024.0f, nStreamedSys/1024.0f/1024.0f, nStreamedUnload/1024.0f/1024.0f);

  SDynTexture_Shadow *pTXSH = SDynTexture_Shadow::m_RootShadow.m_NextShadow;
  int nSizeSH = 0;
  while (pTXSH != &SDynTexture_Shadow::m_RootShadow)
  {
    if (pTXSH->m_pTexture)
      nSizeSH += pTXSH->m_pTexture->GetDeviceDataSize();
    pTXSH = pTXSH->m_NextShadow;
  }

  int nSizeAtlasClouds = SDynTexture2::m_nMemoryOccupied[eTP_Clouds];
  int nSizeAtlasSprites = SDynTexture2::m_nMemoryOccupied[eTP_Sprites];
  int nSizeAtlas = nSizeAtlasClouds + nSizeAtlasSprites;
  int nSizeManagedDyn = SDynTexture::m_nMemoryOccupied;

  Draw2dLabel(nX, nY+=nYstep, 1.2f, &col.r, false, " Dynamic DataSize: %.3f Mb (Atlases: %.3f Mb, Managed: %.3f Mb (Shadows: %.3f Mb), Other: %.3f Mb)", nSRT/1024.0f/1024.0f, nSizeAtlas/1024.0f/1024.0f, nSizeManagedDyn/1024.0f/1024.0f, nSizeSH/1024.0f/1024.0f, (nSRT-nSizeManagedDyn-nSizeAtlas)/1024.0f/1024.0f);

  /*crend->WriteXY(10, nYpos, 1,1,1,1,1,1, "Unique Render Items=%d (%d)",m_RP.m_PS.m_NumRendItems, m_RP.m_PS.m_NumRendBatches);   
  crend->WriteXY(10, nYpos += nYstep*3, 1,1,1,1,1,1, "Unique CVShaders=%d",m_RP.m_PS.m_NumVShaders);
  crend->WriteXY(10, nYpos += nYstep, 1,1,1,1,1,1, "Unique CPShaders=%d",m_RP.m_PS.m_NumPShaders);
  crend->WriteXY(10, nYpos += nYstep, 1,1,1,1,1,1, "Unique Textures=%d",m_RP.m_PS.m_NumTextures);
  crend->WriteXY(10, nYpos += nYstep, 1,1,1,1,1,1, "PerFrame: ManagedTexturesDeviceSize=%.03f Kb, ManagedTexturesFullSize=%.03f Kb, DynTexturesSize=%.03f Kb",m_RP.m_PS.m_ManagedTexturesSize/1024.0f, m_RP.m_PS.m_ManagedTexturesFullSize/1024.0f, m_RP.m_PS.m_DynTexturesSize/1024.0f);
  crend->WriteXY(10, nYpos += nYstep, 1,1,1,1,1,1, "TexStreamed=%.03f, TexNonStreamed=%.03f, DynTexturesSize=%.03f Kb (Max=%.03f Kb)",CTexture::m_StatsCurManagedStreamedTexMem/1024.0f, CTexture::m_StatsCurManagedNonStreamedTexMem/1024.0f, CTexture::m_StatsCurDynamicTexMem/1024.0f);
  crend->WriteXY(10, nYpos += nYstep, 1,1,1,1,1,1, "Mesh update=%.03f Kb",m_RP.m_PS.m_MeshUpdateBytes/1024.0f);
  crend->WriteXY(10, nYpos += nYstep, 1,1,1,1,1,1, "Cloud impostors: Update Size=%.03f Kb, #Upd=%d, #Draw=%d",m_RP.m_PS.m_CloudImpostersSizeUpdate/1024.0f, m_RP.m_PS.m_NumCloudImpostersUpdates, m_RP.m_PS.m_NumCloudImpostersDraw);
  crend->WriteXY(10, nYpos += nYstep, 1,1,1,1,1,1, "Impostors: Update Size=%.03f Kb, #Upd=%d, #Draw=%d",m_RP.m_PS.m_ImpostersSizeUpdate/1024.0f, m_RP.m_PS.m_NumImpostersUpdates, m_RP.m_PS.m_NumImpostersDraw);
  {
    CHWShader_D3D *ps = (CHWShader_D3D *)m_RP.m_PS.m_pMaxPShader;
    if (ps)
      crend->WriteXY(10, nYpos += nYstep, 1,1,1,1,1,1, "MAX PShader: %s (instructions: %d, lights: %d)", ps->GetName(), ps->m_Insts[m_RP.m_PS.m_nMaxPSInstance].m_nInstructions, ps->m_Insts[m_RP.m_PS.m_nMaxPSInstance].m_LightMask & 0xf);
    CHWShader_D3D *vs = (CHWShader_D3D *)m_RP.m_PS.m_pMaxVShader;
    if (vs)
      crend->WriteXY(10, nYpos += nYstep, 1,1,1,1,1,1, "MAX VShader: %s (instructions: %d, lights: %d)", vs->GetName(), vs->m_Insts[m_RP.m_PS.m_nMaxVSInstance].m_nInstructions, vs->m_Insts[m_RP.m_PS.m_nMaxVSInstance].m_LightMask & 0xf);
  }*/
}

void CD3D9Renderer::DebugPrintShader(CHWShader_D3D *pSH, void *pI, int nX, int nY, ColorF colSH)
{
  if (!pSH)
    return;
  CHWShader_D3D::SHWSInstance *pInst = (CHWShader_D3D::SHWSInstance *)pI;
  if (!pInst)
    return;

  char str[512];
  pSH->m_pCurInst = pInst;
  strcpy(str, pSH->m_EntryFunc.c_str());
  uint nSize = strlen(str);
  pSH->mfGenName(pInst, &str[nSize], 512, 1);

  ColorF col = Col_Green;
  Draw2dLabel(nX, nY, 1.6f, &col.r, false, "Shader: %s (%d instructions)", str, pInst->m_nInstructions);
  nX += 10; nY += 25;

  SD3DShader *pHWS = pInst->m_Handle.m_pShader;
  if (!pHWS || !pHWS->m_pHandle)
    return;
#if defined(DIRECT3D9)
  HRESULT hr = S_OK;
  byte *pData = NULL;
  if (pInst->m_eProfileType < eHWSP_VS_Auto)
  {
    IDirect3DVertexShader9 *pVS = (IDirect3DVertexShader9 *)pHWS->m_pHandle;
    hr = pVS->GetFunction(NULL, &nSize);
    if (FAILED(hr))
      return;
    pData = new byte[nSize];
    hr = pVS->GetFunction(pData, &nSize);
  }
  else
  {
    IDirect3DPixelShader9 *pPS = (IDirect3DPixelShader9 *)pHWS->m_pHandle;
    hr = pPS->GetFunction(NULL, &nSize);
    if (FAILED(hr))
      return;
    pData = new byte[nSize];
    hr = pPS->GetFunction(pData, &nSize);
  }
  LPD3DXBUFFER pAsm = NULL;
  hr = D3DXDisassembleShader((DWORD *)pData, FALSE, NULL, &pAsm);
  if (!pAsm)
  {
    SAFE_DELETE_ARRAY(pData);
    return;
  }
  char *szAsm = (char *)pAsm->GetBufferPointer();
#elif defined(DIRECT3D10)
  if (!pInst->m_pShaderData)
    return;
  byte *pData = NULL;
  ID3D10Blob* pAsm = NULL;
  D3D10DisassembleShader((UINT *)pInst->m_pShaderData, pInst->m_nShaderByteCodeSize, false, NULL, &pAsm);
  if (!pAsm)
    return;
  char *szAsm = (char *)pAsm->GetBufferPointer();
#endif
  int nMaxY = m_height;
  int nM = 0;
  while (szAsm[0])
  {
    fxFillCR(&szAsm, str);
    Draw2dLabel(nX, nY, 1.2f, &colSH.r, false, "%s", str);
    nY += 11;
    if (nY+12 > nMaxY)
    {
      nX += 280;
      nY = 120;
      nM++;
    }
    if (nM == 2)
      break;
  }

  SAFE_RELEASE(pAsm);
  SAFE_DELETE_ARRAY(pData);
}

void CD3D9Renderer::DebugDrawStats2()
{
  const int nYstep = 10;
  int nY = 30; // initial Y pos
  int nX = 20; // initial X pos
  static int snTech = 0;

  if (!g_SelectedTechs.size())
    return;

#if defined(XENON)
  XINPUT_KEYSTROKE keyStroke;
  int result = XInputGetKeystroke(XUSER_INDEX_ANY, XINPUT_FLAG_KEYBOARD, &keyStroke);
  if (result == ERROR_SUCCESS)
  {
    if (!(keyStroke.Flags & XINPUT_KEYSTROKE_REPEAT))
    {
      if (keyStroke.Flags & XINPUT_KEYSTROKE_KEYDOWN)
      {
        if (keyStroke.VirtualKey >= '0' && keyStroke.VirtualKey <= '9')
          snTech = keyStroke.VirtualKey - '0';
      }
    }
  }
#elif defined(PS3)
	if (CryGetAsyncKeyState(CELL_KEYC_0) & 0x1)
		snTech = 0;
	if (CryGetAsyncKeyState(CELL_KEYC_1) & 0x1)
		snTech = 1;
	if (CryGetAsyncKeyState(CELL_KEYC_2) & 0x1)
		snTech = 2;
	if (CryGetAsyncKeyState(CELL_KEYC_3) & 0x1)
		snTech = 3;
	if (CryGetAsyncKeyState(CELL_KEYC_4) & 0x1)
		snTech = 4;
	if (CryGetAsyncKeyState(CELL_KEYC_5) & 0x1)
		snTech = 5;
	if (CryGetAsyncKeyState(CELL_KEYC_6) & 0x1)
		snTech = 6;
	if (CryGetAsyncKeyState(CELL_KEYC_7) & 0x1)
		snTech = 7;
	if (CryGetAsyncKeyState(CELL_KEYC_8) & 0x1)
		snTech = 8;
	if (CryGetAsyncKeyState(CELL_KEYC_9) & 0x1)
		snTech = 9;
#else
	if (CryGetAsyncKeyState('0') & 0x1)
		snTech = 0;
	if (CryGetAsyncKeyState('1') & 0x1)
		snTech = 1;
	if (CryGetAsyncKeyState('2') & 0x1)
		snTech = 2;
	if (CryGetAsyncKeyState('3') & 0x1)
		snTech = 3;
	if (CryGetAsyncKeyState('4') & 0x1)
		snTech = 4;
	if (CryGetAsyncKeyState('5') & 0x1)
		snTech = 5;
	if (CryGetAsyncKeyState('6') & 0x1)
		snTech = 6;
	if (CryGetAsyncKeyState('7') & 0x1)
		snTech = 7;
	if (CryGetAsyncKeyState('8') & 0x1)
		snTech = 8;
	if (CryGetAsyncKeyState('9') & 0x1)
		snTech = 9;
#endif

  TArray<SShaderTechniqueStat *> Techs;
  int i, j;
  for (i=0; i<g_SelectedTechs.size(); i++)
  {
    SShaderTechniqueStat *pTech = &g_SelectedTechs[i];
    for (j=0; j<Techs.Num(); j++)
    {
      if (Techs[j]->pVSInst == pTech->pVSInst && Techs[j]->pPSInst == pTech->pPSInst)
        break;
    }
    if (j == Techs.Num())
      Techs.AddElem(pTech);
  }

  if (snTech >= Techs.Num())
    snTech = Techs.Num()-1;
  SShaderTechniqueStat *pTech = Techs[snTech];

  ColorF col = Col_Yellow;
  Draw2dLabel(nX, nY, 2.0f, &col.r, false, "FX Shader: %s, Technique: %s (%d out of %d), Pass: %d", pTech->pShader->GetName(), pTech->pTech->m_Name.c_str(), snTech, Techs.Num(), 0);
  nY += 25;

  CHWShader_D3D *pVS = pTech->pVS;
  CHWShader_D3D *pPS = pTech->pPS;
  DebugPrintShader(pTech->pVS, pTech->pVSInst, nX-10, nY, Col_White);
  DebugPrintShader(pPS, pTech->pPSInst, nX+450, nY, Col_Cyan);
}

void CD3D9Renderer::DebugDrawStats()
{
  if (!CV_r_stats)
    return;
  CRenderer *crend=gRenDev;

  CCryName Name;
  SResourceContainer *pRL;
  switch (CV_r_stats)
  {
  case 1:
    DebugDrawStats1();
    break;
  case 2:
    DebugDrawStats2();
    break;
  case 11:
    EF_PrintRTStats("Used Render Targets:");
    break;
  case 12:
    EF_PrintRTStats("Used unique Render Targets:");
    break;
  case 13:
    EF_PrintRTStats("Cleared Render Targets:");
    break;
  case 3:
    {
      CRenderMesh *pRM = CRenderMesh::m_Root.m_Prev;
      int nMem = 0;
      while (pRM != &CRenderMesh::m_Root)
      {
        nMem += pRM->Size(1);
        pRM = pRM->m_Prev;
      }
      crend->WriteXY(550,140, 1,1,1,1,1,1,"RenderMesh Vert. size=%0.3fMb",(float)nMem/1024.0f/1024.0f);
    }
    break;

  case 4:
    {
      uint i;
      int nX = 500;

      int nSize = 0;
      int n = 0;
      Name = CShader::mfGetClassName();
      pRL = CBaseResource::GetResourcesForClass(Name);
      if (pRL)
      {
        ResourcesMapItor itor;
        for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
        {
          CShader *sh = (CShader *)itor->second;
          if (!sh)
            continue;
          nSize += sh->Size(0);
          n++;
        }
      }
      crend->WriteXY(nX,160, 1,1,1,1,1,1,"Shaders: %d, size=%0.3fMb",n , (float)nSize/1024.0f/1024.0f);

      nSize = 0;
      n = 0;
      for (i=0; i<CShader::m_ShaderResources_known.Num(); i++)
      {
        SRenderShaderResources *pSR = CShader::m_ShaderResources_known[i];
        if (!pSR)
          continue;
        nSize += pSR->Size();
        n++;
      }
      crend->WriteXY(nX,175, 1,1,1,1,1,1,"Shader Res: %d, size=%0.3fMb",n , (float)nSize/1024.0f/1024.0f);

#if !defined (NULL_RENDERER)
      nSize = 0;
      n = 0;
      Name = CHWShader::mfGetClassName(eHWSC_Vertex);
      pRL = CBaseResource::GetResourcesForClass(Name);
      if (pRL)
      {
        ResourcesMapItor itor;
        for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
        {
          CHWShader *vs = (CHWShader *)itor->second;
          if (!vs)
            continue;
          nSize += vs->Size();
          n++;
        }
      }
      crend->WriteXY(nX,205, 1,1,1,1,1,1,"VProgramms: %d, size=%0.3fMb",n,(float)nSize/1024.0f/1024.0f);

      Name = CHWShader::mfGetClassName(eHWSC_Pixel);
      pRL = CBaseResource::GetResourcesForClass(Name);
      if (pRL)
      {
        ResourcesMapItor itor;
        for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
        {
          CHWShader *ps = (CHWShader *)itor->second;
          if (!ps)
            continue;
          nSize += ps->Size();
          n++;
        }
      }
      crend->WriteXY(nX,220, 1,1,1,1,1,1,"PShaders: %d, size=%0.3fMb",n,(float)nSize/1024.0f/1024.0f);
#endif

      nSize = 0;
      n = 0;
      for (i=0; i<CLightStyle::m_LStyles.Num(); i++)
      {
        nSize += CLightStyle::m_LStyles[i]->Size();
        n++;
      }
      crend->WriteXY(nX,235, 1,1,1,1,1,1,"LStyles: %d, size=%0.3fMb",n,(float)nSize/1024.0f/1024.0f);

      nSize = 0;
      int nSizeVid = 0;
      int nSizeInds = 0;
      n = 0;
      int nVB = 0;
      int nVI = 0;
      CRenderMesh *pRM = CRenderMesh::m_RootGlobal.m_NextGlobal;
      while (pRM != &CRenderMesh::m_RootGlobal)
      {
        n++;
        nSize += pRM->Size(0);
        nSizeVid += pRM->Size(1);
        if (pRM->m_pVertexBuffer)
          nVB++;
        nSizeInds += pRM->Size(2);
        if (pRM->m_Indices.m_VertBuf.m_pPtr)
          nVI++;
        pRM = pRM->m_NextGlobal;
      }
      crend->WriteXY(nX,280, 1,1,1,1,1,1,"Mesh (LB): %d, SysSize=%0.3fMb",n,(float)nSize/1024.0f/1024.0f);
      crend->WriteXY(nX,295, 1,1,1,1,1,1,"VidBuf: %d, Size: %0.3fMb", nVB, (float)nSizeVid/1024.0f/1024.0f);
      crend->WriteXY(nX,310, 1,1,1,1,1,1,"VidInds: %d, Size: %0.3fMb", nVI, (float)nSizeInds/1024.0f/1024.0f);

      nSize = 0;
      n = 0;
      CRendElement *pRE = CRendElement::m_RootGlobal.m_NextGlobal;
      while (pRE != &CRendElement::m_RootGlobal)
      {
        n++;
        nSize += pRE->Size();
        pRE = pRE->m_NextGlobal;
      }
      crend->WriteXY(nX,325, 1,1,1,1,1,1,"Rend. Elements: %d, size=%0.3fMb",n,(float)nSize/1024.0f/1024.0f);

      nSize = 0;
      int nObjSize = 0;
      n = 0;
      Name = CTexture::mfGetClassName();
      pRL = CBaseResource::GetResourcesForClass(Name);
      if (pRL)
      {
        ResourcesMapItor itor;
        for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
        {
          CTexture *tp = (CTexture *)itor->second;
          if (!tp || tp->IsNoTexture())
            continue;
          n++;
          nObjSize += tp->GetSize();
          nSize += tp->GetDeviceDataSize();
        }
      }
      crend->WriteXY(nX,350, 1,1,1,1,1,1,"Textures: %d, ObjSize=%0.3fMb",n,(float)nObjSize/1024.0f/1024.0f);
      crend->WriteXY(nX,365, 1,1,1,1,1,1,"TexturesDataSize=%.03f Mb",nSize/1024.0f/1024.0f);
      crend->WriteXY(nX,380, 1,1,1,1,1,1,"CurTexturesDataSize: (Managed=%.03f Mb, Dynamic=%.03f Mb)",m_RP.m_PS.m_ManagedTexturesSize/1024.0f/1024.0f, m_RP.m_PS.m_DynTexturesSize/1024.0f/1024.0f);
      crend->WriteXY(nX,405, 1,1,1,1,1,1,"Mesh update=%.03f Kb",m_RP.m_PS.m_MeshUpdateBytes/1024.0f);
    }
    break;

  case 5:
    {
      const int nYstep = 30;
      int nYpos = 270; // initial Y pos
      crend->WriteXY(10, nYpos+=nYstep, 2,2,1,1,1,1, "CREOcclusionQuery stats:",
        CREOcclusionQuery::m_nQueriesPerFrameCounter);   
      crend->WriteXY(10, nYpos+=nYstep, 2,2,1,1,1,1, "CREOcclusionQuery::m_nQueriesPerFrameCounter=%d",
        CREOcclusionQuery::m_nQueriesPerFrameCounter);   
      crend->WriteXY(10, nYpos+=nYstep, 2,2,1,1,1,1, "CREOcclusionQuery::m_nReadResultNowCounter=%d",
        CREOcclusionQuery::m_nReadResultNowCounter);   
      crend->WriteXY(10, nYpos+=nYstep, 2,2,1,1,1,1, "CREOcclusionQuery::m_nReadResultTryCounter=%d",
        CREOcclusionQuery::m_nReadResultTryCounter);   
    }
    break;
  }

}

void CD3D9Renderer::RenderDebug()
{
	int i;

	if (CV_r_showtimegraph)
	{
		static byte *fg;
		static float fPrevTime = iTimer->GetCurrTime();
		static int sPrevWidth = 0;
		static int sPrevHeight = 0;
		static int nC;

		float fCurTime = iTimer->GetCurrTime();
		float frametime = fCurTime - fPrevTime;
		fPrevTime = fCurTime;
		int wdt = m_width;
		int hgt = m_height;

		if (sPrevHeight != hgt || sPrevWidth != wdt)
		{
			if (fg)
			{
				delete [] fg;
				fg = NULL;
			}
			sPrevWidth = wdt;
			sPrevHeight = hgt;
		}

		if (!fg)
		{
			fg = new byte[wdt];
			memset(fg, -1, wdt);
      nC = 0;
		}

		int type = CV_r_showtimegraph;
		float f;
		float fScale;
		if (type > 1)
		{
			type = 1;
			fScale = (float)CV_r_showtimegraph / 1000.0f;
		}
		else
			fScale = 0.1f;
		f = frametime / fScale;
		f = 255.0f - CLAMP(f*255.0f, 0, 255.0f);
		if (fg)
		{
			fg[nC] = (byte)f;
			ColorF c = Col_Green;
			Graph(fg, 0, hgt-280, wdt, 256, nC, type, "Frame Time", c, fScale);
		}
		nC++;
		if (nC >= wdt)
			nC = 0;
	}
	else
	if (CV_r_showtextimegraph)
	{
		static byte *fgUpl;
		static byte *fgStreamSync;
		static byte *fgTimeUpl;
		static byte *fgDistFact;
		static byte *fgTotalMem;
		static byte *fgCurMem;

		static float fScaleUpl = 10;  // in Mb
		static float fScaleStreamSync = 10;  // in Mb
		static float fScaleTimeUpl = 75;  // in Ms
		static float fScaleDistFact = 4;  // Ratio
		static FLOAT fScaleTotalMem = 0;  // in Mb
		static float fScaleCurMem = 80;   // in Mb

		static ColorF ColUpl = Col_White;
		static ColorF ColStreamSync = Col_Cyan;
		static ColorF ColTimeUpl = Col_SeaGreen;
		static ColorF ColDistFact = Col_Orchid;
		static ColorF ColTotalMem = Col_Red;
		static ColorF ColCurMem = Col_Yellow;

		static int sMask = -1;

		fScaleTotalMem = (float)CRenderer::CV_r_texturesstreampoolsize - 1;

		static float fPrevTime = iTimer->GetCurrTime();
		static int sPrevWidth = 0;
		static int sPrevHeight = 0;
		static int nC;

		int wdt = m_width;
		int hgt = m_height;
		int type = 2;

		if (sPrevHeight != hgt || sPrevWidth != wdt)
		{
			SAFE_DELETE_ARRAY(fgUpl);
			SAFE_DELETE_ARRAY(fgStreamSync);
			SAFE_DELETE_ARRAY(fgTimeUpl);
			SAFE_DELETE_ARRAY(fgDistFact);
			SAFE_DELETE_ARRAY(fgTotalMem);
			SAFE_DELETE_ARRAY(fgCurMem);
			sPrevWidth = wdt;
			sPrevHeight = hgt;
		}

		if (!fgUpl)
		{
			fgUpl = new byte[wdt];
			memset(fgUpl, -1, wdt);
			fgStreamSync = new byte[wdt];
			memset(fgStreamSync, -1, wdt);
			fgTimeUpl = new byte[wdt];
			memset(fgTimeUpl, -1, wdt);
			fgDistFact = new byte[wdt];
			memset(fgDistFact, -1, wdt);
			fgTotalMem = new byte[wdt];
			memset(fgTotalMem, -1, wdt);
			fgCurMem = new byte[wdt];
			memset(fgCurMem, -1, wdt);
		}

		Set2DMode(true, m_width, m_height);
		ColorF col = Col_White;
		int num = CTexture::m_Text_White->GetID();
		DrawImage((float)nC, (float)(hgt-280), 1, 256, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
		Set2DMode(false, m_width, m_height);

		float f;
		if (sMask & 1)
		{
			f = (CTexture::m_UploadBytes/1024.0f/1024.0f) / fScaleUpl;
			f = 255.0f - CLAMP(f*255.0f, 0, 255.0f);
			fgUpl[nC] = (byte)f;
			Graph(fgUpl, 0, hgt-280, wdt, 256, nC, type, NULL, ColUpl, fScaleUpl);
			col = ColUpl;
			WriteXY(4,hgt-280, 1,1,col.r,col.g,col.b,1, "UploadMB (%d-%d)", (int)(CTexture::m_UploadBytes/1024.0f/1024.0f), (int)fScaleUpl);
		}

		if (sMask & 2)
		{
			f = m_RP.m_PS.m_fTexUploadTime / fScaleTimeUpl;
			f = 255.0f - CLAMP(f*255.0f, 0, 255.0f);
			fgTimeUpl[nC] = (byte)f;
			Graph(fgTimeUpl, 0, hgt-280, wdt, 256, nC, type, NULL, ColTimeUpl, fScaleTimeUpl);
			col = ColTimeUpl;
			WriteXY(4,hgt-280+16, 1,1,col.r,col.g,col.b,1, "Upload Time (%.3fMs - %.3fMs)", m_RP.m_PS.m_fTexUploadTime, fScaleTimeUpl);
		}

		if (sMask & 4)
		{
			f = (CTexture::m_LoadBytes/1024.0f/1024.0f) / fScaleStreamSync;
			f = 255.0f - CLAMP(f*255.0f, 0, 255.0f);
			fgStreamSync[nC] = (byte)f;
			Graph(fgStreamSync, 0, hgt-280, wdt, 256, nC, type, NULL, ColStreamSync, fScaleStreamSync);
			col = ColStreamSync;
			WriteXY(4,hgt-280+16*2, 1,1,col.r,col.g,col.b,1, "StreamMB (%d-%d)", (int)(CTexture::m_LoadBytes/1024.0f/1024.0f), (int)fScaleStreamSync);
		}

		if (sMask & 8)
		{
			f = CTexture::m_fAdaptiveStreamDistRatio / fScaleDistFact;
			f = 255.0f - CLAMP(f*255.0f, 0, 255.0f);
			fgDistFact[nC] = (byte)f;
			Graph(fgDistFact, 0, hgt-280, wdt, 256, nC, type, NULL, ColDistFact, fScaleDistFact);
			col = ColDistFact;
			WriteXY(4,hgt-280+16*3, 1,1,col.r,col.g,col.b,1, "Adaptive Dist. ratio (Upload) (%.3f-%d)", CTexture::m_fAdaptiveStreamDistRatio, (int)fScaleDistFact);
		}

		if (sMask & 32)
		{
			f = (CTexture::m_StatsCurManagedStreamedTexMem/1024.0f/1024.0f) / fScaleTotalMem;
			f = 255.0f - CLAMP(f*255.0f, 0, 255.0f);
			fgTotalMem[nC] = (byte)f;
			Graph(fgTotalMem, 0, hgt-280, wdt, 256, nC, type, NULL, ColTotalMem, fScaleTotalMem);
			col = ColTotalMem;
			WriteXY(4,hgt-280+16*5, 1,1,col.r,col.g,col.b,1, "Cur loaded textures size: Stat. (Mb) (%d-%d)", (int)(CTexture::m_StatsCurManagedStreamedTexMem/1024.0f/1024.0f), (int)fScaleTotalMem);
		}
		if (sMask & 64)
		{
			f = ((m_RP.m_PS.m_ManagedTexturesSize+m_RP.m_PS.m_DynTexturesSize)/1024.0f/1024.0f) / fScaleCurMem;
			f = 255.0f - CLAMP(f*255.0f, 0, 255.0f);
			fgCurMem[nC] = (byte)f;
			Graph(fgCurMem, 0, hgt-280, wdt, 256, nC, type, NULL, ColCurMem, fScaleCurMem);
			col = ColCurMem;
			WriteXY(4,hgt-280+16*6, 1,1,col.r,col.g,col.b,1, "Cur Scene Size: Dyn. + Stat. (Mb) (%d-%d)", (int)((m_RP.m_PS.m_ManagedTexturesSize+m_RP.m_PS.m_DynTexturesSize)/1024.0f/1024.0f), (int)fScaleCurMem);
		}

		nC++;
		if (nC == wdt)
			nC = 0;
	}
	if (CV_r_showlumhistogram)
	{
		static byte *fg;
		static float fPrevTime = iTimer->GetCurrTime();
		static int sPrevWidth = 0;
		static int sPrevHeight = 0;
		static int nC;

		float fCurTime = iTimer->GetCurrTime();
		float frametime = fCurTime - fPrevTime;
		fPrevTime = fCurTime;
		int wdt = m_width;
		int hgt = m_height;



		Set2DMode(true, m_width, m_height);
		EF_SetState(GS_NODEPTHTEST);

		SHistogram *pHG = &m_LumHistogram;

		float fHistWidth = 100;

		float fy = (float)hgt-280;
		float fx = (float)10;
		float fhgt = (float)100;

		ColorF col = Col_Blue;
		int num = CTexture::m_Text_White->GetTextureID();

		// black background
		DrawImage(0, fy, 4000, fHistWidth+34, num, 0, 0, 1, 1, 0,0,0,0.5f);

		// bottom line
		DrawImage(0, fy+fHistWidth-2, 4000, 2, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);

		col = Col_Yellow;

		float fMaxInLum = 0.01f;		// to normalize histogram height
		for (i=0; i<256; i++)
			fMaxInLum = max(fMaxInLum,pHG->Lum[i]/(1+LogMap(pHG->MaxVal*i/255.0f)));

		// Display Ruler
		for(float fRulerVal=1.0f/32.0f;fRulerVal<=128.0f;fRulerVal*=2.0f)
		{
			DrawImage(fx+LogMap(fRulerVal)*fHistWidth, fy, 2, fhgt, num, 0, 0, 1, 1, 0.5f, 0, 0, 1);
			WriteXY((int)((fx+LogMap(fRulerVal)*fHistWidth)*800.0f/m_width),405, 0.75f,0.75f,1,1,1,1, "%g",fRulerVal);
		}

		// logarithmic scale
		Vec3 vp[256];
		for (i=0; i<256; i++)
		{
			float fLogVal=LogMap(pHG->MaxVal*i/255.0f);

			vp[i][0] = fx + fLogVal*fHistWidth;
			vp[i][1] = fy + fhgt - pHG->Lum[i] / (1+fLogVal) * fhgt/fMaxInLum;
			vp[i][2] = 0;
		} 

		DrawLines(&vp[0], 256, col, 0, fy+fhgt);

		// current meatured values
		DrawLineColor(Vec3(LogMap(pHG->LowPercentil)*fHistWidth+fx,fy+fhgt+12,0),Col_Red,Vec3(LogMap(pHG->MidPercentil)*fHistWidth+fx,fy+fhgt+4,0),Col_Red);
		DrawLineColor(Vec3(LogMap(pHG->MidPercentil)*fHistWidth+fx,fy+fhgt+4,0),Col_Red,Vec3(LogMap(pHG->HighPercentil)*fHistWidth+fx,fy+fhgt+12,0),Col_Red);
		DrawLineColor(Vec3(LogMap(pHG->LowPercentil)*fHistWidth+fx,fy+fhgt+12,0),Col_Red,Vec3(LogMap(pHG->HighPercentil)*fHistWidth+fx,fy+fhgt+12,0),Col_Red);

		// current eye adaption
		DrawLineColor(Vec3(LogMap(m_LumGamma.LowPercentil)*fHistWidth+fx,fy+fhgt+16,0),Col_Green,Vec3(LogMap(m_LumGamma.MidPercentil)*fHistWidth+fx,fy+fhgt+24,0),Col_Green);
		DrawLineColor(Vec3(LogMap(m_LumGamma.MidPercentil)*fHistWidth+fx,fy+fhgt+24,0),Col_Green,Vec3(LogMap(m_LumGamma.HighPercentil)*fHistWidth+fx,fy+fhgt+16,0),Col_Green);
		DrawLineColor(Vec3(LogMap(m_LumGamma.LowPercentil)*fHistWidth+fx,fy+fhgt+16,0),Col_Green,Vec3(LogMap(m_LumGamma.HighPercentil)*fHistWidth+fx,fy+fhgt+16,0),Col_Green);

		// valid range
		DrawLineColor(Vec3(LogMap(CRenderer::CV_r_eyeadaptionmin)*fHistWidth+fx,fy+fhgt+16,0),Col_White,Vec3(LogMap(CRenderer::CV_r_eyeadaptionmin)*fHistWidth+fx+14,fy+fhgt+30,0),Col_White);
		DrawLineColor(Vec3(LogMap(CRenderer::CV_r_eyeadaptionmax)*fHistWidth+fx,fy+fhgt+16,0),Col_White,Vec3(LogMap(CRenderer::CV_r_eyeadaptionmax)*fHistWidth+fx-14,fy+fhgt+30,0),Col_White);
		DrawLineColor(Vec3(LogMap(CRenderer::CV_r_eyeadaptionmin)*fHistWidth+fx+14,fy+fhgt+30,0),Col_White,Vec3(LogMap(CRenderer::CV_r_eyeadaptionmax)*fHistWidth+fx-14,fy+fhgt+30,0),Col_White);

		Set2DMode(false, 0, 0);
	}
	if (CV_r_envlightcmdebug)
	{
		SEnvTexture *cm = NULL;
		Vec3 Pos = m_cam.GetPosition();
		cm = CTexture::FindSuitableEnvLCMap(Pos, true, 0, 0);
		if (cm && cm->m_bReady)
		{
			EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
			EF_SetState(GS_NODEPTHTEST);
			CTexture::m_Text_White->Apply();
			DrawQuad(0,0,64,64,			cm->m_EnvColors[0], 1.0f);
			DrawQuad(64,0,128,64,		cm->m_EnvColors[2], 1.0f);
			DrawQuad(128,0,192,64,	cm->m_EnvColors[4], 1.0f);
			DrawQuad(0,64,64,128,		cm->m_EnvColors[1], 1.0f);
			DrawQuad(64,64,128,128,	cm->m_EnvColors[3], 1.0f);
			DrawQuad(128,64,192,128,cm->m_EnvColors[5], 1.0f);
			PrintToScreen(5,5,12,"Pos X");
			PrintToScreen(5+54,5,12,"Pos Y");
			PrintToScreen(5+108,5,12,"Pos Z");
			PrintToScreen(5,5+64,12,"Neg X");
			PrintToScreen(5+54,5+64,12,"Neg Y");
			PrintToScreen(5+108,5+64,12,"Neg Z");
		}
	}

  PostMeasureOverdraw();

	double time = 0;
	ticks(time);

  DebugDrawStats();

  if( CRenderer::CV_r_waterreflections == 2 )
  {
    SDrawTextInfo pDrawTexInfo;    
    pDrawTexInfo.color[0] = pDrawTexInfo.color[2] = 0.0f;
    pDrawTexInfo.color[1] = 1.0f;

    gcpRendD3D->Draw2dText(5, 0, " " , pDrawTexInfo); // hack to avoid garbage - someting broken with draw2Dtext

    char pQualityInfo[256];
    sprintf( pQualityInfo, "Water reflections: \n\nupdated: %d%d%d%d%d%d%d%d%d%d\n", gcpRendD3D->m_RP.m_bFrameUpdated[0],
      gcpRendD3D->m_RP.m_bFrameUpdated[1], gcpRendD3D->m_RP.m_bFrameUpdated[2], gcpRendD3D->m_RP.m_bFrameUpdated[3],
      gcpRendD3D->m_RP.m_bFrameUpdated[4], gcpRendD3D->m_RP.m_bFrameUpdated[5], gcpRendD3D->m_RP.m_bFrameUpdated[6],
      gcpRendD3D->m_RP.m_bFrameUpdated[7], gcpRendD3D->m_RP.m_bFrameUpdated[8], gcpRendD3D->m_RP.m_bFrameUpdated[9]);
    gcpRendD3D->Draw2dText(5, 15, pQualityInfo, pDrawTexInfo);

    sprintf( pQualityInfo, "upd type: %d\nupd factor: %.3f\nupd dist: %.3f\n", gcpRendD3D->m_RP.m_nCurrUpdateType , 
      gcpRendD3D->m_RP.m_fCurrUpdateFactor,
      gcpRendD3D->m_RP.m_fCurrUpdateDistance);
    gcpRendD3D->Draw2dText(5, 50, pQualityInfo, pDrawTexInfo);
  }

	if (CV_r_profileshaders)
		EF_PrintProfileInfo();

	if(CV_r_ShowRenderTarget)
	{
		EF_SetState(GS_NODEPTHTEST);
		int iTmpX, iTmpY, iTempWidth, iTempHeight;
		GetViewport(&iTmpX, &iTmpY, &iTempWidth, &iTempHeight);
		EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
		Set2DMode(true, 1, 1);

		//SEnvTexture *pEnvTex = &CTexture::m_CustomRT_2D[0];
		//CTexture *pTex = CTexture::m_Text_ScreenShadowMap; // pEnvTex->m_pTex;
		//CTexture *pTex = CTexture::m_Text_HDRTarget; //CTexture::m_Text_SceneTarget;

		CTexture *pTex = NULL;
		if(CV_r_ShowRenderTarget == 1)
			pTex = CTexture::m_Text_ZTarget;
		else
		if(CV_r_ShowRenderTarget == 2)
			pTex = CTexture::m_Text_HDRTarget;
		else
		if(CV_r_ShowRenderTarget == 3)
			pTex = CTexture::m_Tex_CurrentScreenShadowMap[0];
		else
		if(CV_r_ShowRenderTarget == 4)
			pTex = CTexture::m_Text_ScreenShadowMap[1];
		else
		if(CV_r_ShowRenderTarget == 5)
			pTex = CTexture::m_Text_ScreenShadowMap[2];
		else
		if(CV_r_ShowRenderTarget == 6)
			pTex = gTexture;
		else
		if(CV_r_ShowRenderTarget == 7)
			pTex = gTexture2;
		else
		if(CV_r_ShowRenderTarget == 8)
			pTex = CTexture::m_Text_ScatterLayer;
		else
		if(CV_r_ShowRenderTarget == 9) //CUSTOMTEXTURE
		{
			if (CTexture::m_CustomRT_2D.size() > 4)
			{
				SEnvTexture *pEnvTex = &CTexture::m_CustomRT_2D[4];
				pTex = pEnvTex->m_pTex->m_pTexture;
			}
		}
		else
		if (CV_r_ShowRenderTarget == 10)
			pTex = CTexture::m_Text_LightInfo[0];
		else
			if(CV_r_ShowRenderTarget == 11)
				pTex = CTexture::m_Text_AOTarget;
		else
			if(CV_r_ShowRenderTarget == 12)
				pTex = CTexture::m_Text_BackBufferScaled[0];
		else
			if(CV_r_ShowRenderTarget == 13)
				pTex = CTexture::m_Text_BackBufferScaled[1];
		else
			if(CV_r_ShowRenderTarget == 14)
				pTex = CTexture::m_Text_BackBufferScaled[2];
    else
      if(CV_r_ShowRenderTarget == 15)
      {
        int nCustomRT = (int)CTexture::m_CustomRT_2D.Num() ;
        if ( nCustomRT && CTexture::m_CustomRT_2D[0].m_pTex)
          pTex = CTexture::m_CustomRT_2D[0].m_pTex->m_pTexture;
        else
          pTex = CTexture::m_Text_White;
      }
    else
      if(CV_r_ShowRenderTarget == 16)
        pTex = CTexture::m_Text_ZTargetScaled;

		if (pTex)
		{
			if(CV_r_ShowRenderTarget_FullScreen)
				SetViewport(0, 0, m_width, m_height);
			else
				SetViewport(10, m_height-m_height/3-10, m_width/3, m_height/3);

			DrawImage(0, 0, 1, 1, pTex->GetID(), 0, 1, 1, 0, 1,1,1,1);

			SetViewport(iTmpX, iTmpY, iTempWidth, iTempHeight);
		}

		Set2DMode(false, 1, 1);
	}

	// print dyn. textures
	{ 
		static bool bWasOn=false;

		if(CV_r_showdyntextures)
		{
			DrawAllDynTextures(CV_r_showdyntexturefilter->GetString(),!bWasOn,CV_r_showdyntextures==2);
			bWasOn=true;
		}
		else
			bWasOn=false;
	}
}

void CD3D9Renderer::EndFrame()
{
  //////////////////////////////////////////////////////////////////////
  // End the scene and update
  //////////////////////////////////////////////////////////////////////

  // Check for the presence of a D3D device
  assert(m_pd3dDevice);

  if (!m_SceneRecurseCount)
  {
    iLog->Log("EndScene without BeginScene\n");
    return;
  }

  PROFILE_FRAME(Screen_Update);
  HRESULT hReturn;

  CTexture::Update();

#if !defined(XENON) && !defined(PS3)
  if (m_CVDisplayInfo && m_CVDisplayInfo->GetIVal() && iSystem && iSystem->IsDevMode())
  {
    int nIconSize = 16; 

    if (SShaderAsyncInfo::s_nPendingAsyncShaders > 0 && CV_r_shadersasynccompiling > 0 && CTexture::m_Text_IconShaderCompiling)
    {
      Draw2dImage(0, 0, nIconSize, nIconSize, CTexture::m_Text_IconShaderCompiling->GetID(), 0, 1, 1, 0);
    }

    if (CTexture::m_bStreamingShow && CTexture::m_Text_IconStreaming)
    {
      CTexture::m_bStreamingShow = false;
      Draw2dImage(nIconSize, 0, nIconSize, nIconSize, CTexture::m_Text_IconStreaming->GetID(), 0, 1, 1, 0);
    }

    // indicate terrain texture streaming activity
    if (gEnv->p3DEngine && CTexture::m_Text_IconStreamingTerrainTexture)
      if(ITerrain * pTerrain = gEnv->p3DEngine->GetITerrain())
        if(pTerrain->GetNotReadyTextureNodesCount())
          Draw2dImage(nIconSize*2, 0, nIconSize, nIconSize, CTexture::m_Text_IconStreamingTerrainTexture->GetID(), 0, 1, 1, 0);
  }
#endif

  m_prevCamera = m_cam;

  if( SRendItem::m_RecurseLevel == 0 )
  {
    m_CameraMatrixPrev = GetTransposed44(Matrix44(Matrix33::CreateRotationX(-gf_PI/2) * m_cam.GetMatrix().GetInverted()));
  }
  

	//////////////////////////////////////////////////////////////////////////

	if (1 == m_SceneRecurseCount)
	{
		m_pD3DRenderAuxGeom->Flush( true );
		m_pD3DRenderAuxGeom->DiscardTransMatrices();
	}
	//else
	//	m_pD3DRenderAuxGeom->Flush( false );

	//////////////////////////////////////////////////////////////////////////

  // draw debug bboxes and lines
  std :: vector<BBoxInfo>::iterator itBBox = m_arrBoxesToDraw.begin(), itBBoxEnd = m_arrBoxesToDraw.end();
  for( ; itBBox != itBBoxEnd; ++itBBox)
  {
    BBoxInfo& rBBox = *itBBox;
    SetMaterialColor( rBBox.fColor[0], rBBox.fColor[1],
      rBBox.fColor[2], rBBox.fColor[3] );

    // set blend for transparent objects
    if(rBBox.fColor[3]!=1.f)
      EF_SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

    switch(rBBox.nPrimType)
    {
    case DPRIM_LINE:
      DrawLine(rBBox.vMins, rBBox.vMaxs);
      break;

    case DPRIM_WHIRE_BOX:
      Flush3dBBox(rBBox.vMins, rBBox.vMaxs, false);
      break;

    case DPRIM_SOLID_BOX:
      Flush3dBBox(rBBox.vMins, rBBox.vMaxs, true);
      break;

    case DPRIM_SOLID_SPHERE:
      {
        Vec3 vPos = (rBBox.vMins+rBBox.vMaxs)*0.5f;
        float fRadius = (rBBox.vMins-rBBox.vMaxs).GetLength();
        ColorF col = ColorF(rBBox.fColor[0], rBBox.fColor[1], rBBox.fColor[2], rBBox.fColor[3]);
        m_pD3DRenderAuxGeom->DrawSphere(vPos, fRadius, ColorB(col), false);
      }
      break;
    }
  }

  EF_SetState(GS_NODEPTHTEST);

  m_arrBoxesToDraw.clear();

	{ // print shadow maps on the screen
		static ICVar *pVar = iConsole->GetCVar("e_shadows_debug");
		if(pVar && pVar->GetIVal()>=1 && pVar->GetIVal()<=2)
			DrawAllShadowsOnTheScreen();
	}


#ifdef USE_HDR
  if (CV_r_hdrdebug > 1)
    HDR_DrawDebug();
#endif

  if (CRenderer::CV_r_log)
    Logv(0, "******************************* EndFrame ********************************\n");

  // End the scene
#ifdef XENON
  m_pd3dDevice->EndScene();
#else
 #if defined (DIRECT3D9) || defined (OPENGL)
  m_pd3dDevice->EndScene();
 #endif
#endif
  m_SceneRecurseCount--;

#if defined (DIRECT3D9) || defined (OPENGL)
#if !defined(XENON) && !defined(PS3)
  if (CV_r_ShowVideoMemoryStats)
  {
    HRESULT hr;
    CV_r_ShowVideoMemoryStats = FALSE;
    IDirect3DQuery9 *resourceQuery;
    BOOL timeOut;

    D3DDEVINFO_RESOURCEMANAGER resourceStats;
    hr = m_pd3dDevice->CreateQuery(D3DQUERYTYPE_RESOURCEMANAGER, &resourceQuery);
    if (hr == D3D_OK)
    {
      resourceQuery->Issue(D3DISSUE_END);
    	float timeoutTime = iTimer->GetCurrTime() + 2.0f;
      timeOut = FALSE;
      // D3DGETDATA_FLUSH
      while (resourceQuery->GetData((void *)&resourceStats, sizeof(D3DDEVINFO_RESOURCEMANAGER), 0) != S_OK)
      {
        if (iTimer->GetCurrTime() < timeoutTime)
        {
          timeOut = TRUE;
          break;
        }
      }
      resourceQuery->Release();

      // make sure succeeded
      if (!timeOut)
      {
        for (int i=0; i<D3DRTYPECOUNT; i++)
        {
          iLog->Log("%s - Thrashing = %s, Approx Bytes Downloaded %d, Number Evictions %d",
              resourceName[i], resourceStats.stats[i].bThrashing ? "TRUE" : "FALSE",
              resourceStats.stats[i].ApproxBytesDownloaded, resourceStats.stats[i].NumEvicts);
              iLog->Log("    Number Video Creates %d, Last Priority %d",
                resourceStats.stats[i].NumVidCreates, resourceStats.stats[i].LastPri);
              iLog->Log("    Number set to Device %d, Number used In Vid mem %d",
                resourceStats.stats[i].NumUsed, resourceStats.stats[i].NumUsedInVidMem);

          iLog->Log("%s - %d object(s) with %d bytes in video memory.\n",
            resourceName[i], resourceStats.stats[i].WorkingSet, resourceStats.stats[i].WorkingSetBytes);
          iLog->Log("%s - %d object(s) with %d bytes in managed memory.\n",
            resourceName[i], resourceStats.stats[i].TotalManaged, resourceStats.stats[i].TotalBytes);
        }
      }
    }
  }
#endif
#endif

  //assert (m_nRTStackLevel[0] == 0 && m_nRTStackLevel[1] == 0 && m_nRTStackLevel[2] == 0 && m_nRTStackLevel[3] == 0);
  EF_Commit();

    // Get the present params from the swap chain
    /*IDirect3DSurface9* pBackBuffer = NULL;
    HRESULT hr = m_pd3dDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
    if( SUCCEEDED(hr) )
    {
        IDirect3DSwapChain9* pSwapChain = NULL;
        hr = pBackBuffer->GetContainer( IID_IDirect3DSwapChain9, (void**) &pSwapChain );
        if( SUCCEEDED(hr) )
        {
          D3DPRESENT_PARAMETERS PresentationParameters;
            pSwapChain->GetPresentParameters( &PresentationParameters );
            SAFE_RELEASE( pSwapChain );
        }

        SAFE_RELEASE( pBackBuffer );
    }*/

  /*extern SDynTexture2 *gDT;
  extern int gnFrame;
  if (gDT)
  {
    static D3DSurface *pSurf = NULL;
    HRESULT hr;
    if (!pSurf)
    {
      hr = m_pd3dDevice->CreateRenderTarget(64, 64, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &pSurf, NULL);
    }
    int nFrame = m_nFrameSwapID;
    if ((1<<(nFrame&1)) & gnFrame)
    {
      SDynTexture2 *pDT = gDT;
      pDT->Apply(0);
      FX_PushRenderTarget(0, pSurf, &m_DepthBufferOrig);
      EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
      DrawFullScreenQuad(m_cEF.m_ShaderTreeSprites, "General_Debug", m_cEF.m_RTRect.x, m_cEF.m_RTRect.y, m_cEF.m_RTRect.x+m_cEF.m_RTRect.z, m_cEF.m_RTRect.y+m_cEF.m_RTRect.w);
      FX_PopRenderTarget(0);
      D3DLOCKED_RECT lr;
      pSurf->LockRect(&lr, NULL, 0);
      UCol *pData = (UCol *)lr.pBits;
      //for (int i=0; i<64*64; i++)
      //{
      UCol d = pData[32*64+32];
      if (d.bcolor[0] == 0xff)
      {
        //assert(0);
        int nnn = 0;
      }
      //}
      pSurf->UnlockRect();
    }
  }*/

  // Flip the back buffer to the front
  if(m_bSwapBuffers)
	{
		CaptureFrameBuffer();

		if (!m_bEditor)
    {
#ifdef XENON
      FX_PopRenderTarget(0);

      // Swap to the current front buffer, so we can see it on screen.
      m_pd3dDevice->SynchronizeToPresentationInterval();

      D3DTexture *pTex = (D3DTexture *)CTexture::m_FrontTexture[m_dwCurrentBuffer].GetDeviceTexture();
      m_pd3dDevice->Swap(pTex, NULL);

      // Swap buffer usage
      m_dwCurrentBuffer = m_dwCurrentBuffer ? 0L : 1L;
      CTexture::m_pBackBuffer = &CTexture::m_FrontTexture[m_dwCurrentBuffer];

      assert(!m_nRTStackLevel[0]);
      if (!m_nRTStackLevel[0])
        FX_PushRenderTarget(0, CTexture::m_pBackBuffer, &m_DepthBufferOrig);
#elif defined (DIRECT3D9) || defined (OPENGL)
			hReturn = m_pd3dDevice->Present(NULL, NULL, NULL, NULL);
#elif defined(PS3)
			hReturn = m_pSwapChain->Present( 0, 0 );
#elif defined (DIRECT3D10)
			const DXUTD3D10DeviceSettings& d3d10Settings(DXUTGetCurrentDeviceSettings()->d3d10);
			DWORD syncInterval = d3d10Settings.SyncInterval;
			DWORD presentFlags = d3d10Settings.PresentFlags;
			if (DXUTD3D10GetRenderingOccluded() || DXUTD3D10GetRenderingNonExclusive())
			{
				syncInterval = 0;
				presentFlags = DXGI_PRESENT_TEST;
			}
			hReturn = m_pSwapChain->Present(syncInterval, presentFlags);
      if (DXGI_ERROR_NONEXCLUSIVE == hReturn)
        DXUTD3D10SetRenderingNonExclusive(true);
      else
      if (DXGI_STATUS_OCCLUDED == hReturn)
        DXUTD3D10SetRenderingOccluded(true);
      else
      if (DXGI_ERROR_DEVICE_RESET == hReturn)
      {
        HRESULT hr;
        if (FAILED(hr = DXUTReset3DEnvironment10()))
        {
          if (hr == DXUTERR_RESETTINGDEVICEOBJECTS)
          {
            assert(0);
          }
          else
          {
            DXUTDeviceSettings *pDeviceSettings = DXUTGetCurrentDeviceSettings();
            if (FAILED(DXUTChangeDevice(pDeviceSettings, NULL, NULL, true, false)))
            {
              DXUTShutdown();
              return;
            }
          }
        }
      }
      else
      if (DXGI_ERROR_DEVICE_REMOVED == hReturn)
      {
        if (FAILED(DXUTHandleDeviceRemoved()))
        {
          assert(0);
        }
      }
      else
      if (SUCCEEDED(hReturn))
      {
        if (DXUTD3D10GetRenderingOccluded() )
        {
          // Now that we're no longer occluded
          // allow us to render again
          DXUTD3D10SetRenderingOccluded (false);
        }
        if (DXUTD3D10GetRenderingNonExclusive())
        {
          // Now that the other app has gone non-exclusive
          // allow us to render again
          DXUTD3D10SetRenderingNonExclusive (false);
        }
      }
#endif
    }
		else
		{
#if defined (DIRECT3D9) || defined (OPENGL)
			RECT ClientRect;
			ClientRect.top		= 0;
			ClientRect.left		= 0;
			ClientRect.right	= m_CurrContext->m_Width;
			ClientRect.bottom	= m_CurrContext->m_Height;
			hReturn = m_pd3dDevice->Present(&ClientRect, &ClientRect, m_CurrContext->m_hWnd, NULL);
#elif defined(PS3)
			CRY_ASSERT_MESSAGE(0,"Case in EndFrame() not implemented yet");
#elif defined (DIRECT3D10)
      DWORD dwFlags = 0;
      if(DXUTD3D10GetRenderingOccluded() || DXUTD3D10GetRenderingNonExclusive() )
        dwFlags = DXGI_PRESENT_TEST;
      else
        dwFlags = DXUTGetCurrentDeviceSettings()->d3d10.PresentFlags;
      RECT ClientRect;
      ClientRect.top		= 0;
      ClientRect.left		= 0;
      ClientRect.right	= m_CurrContext->m_Width;
      ClientRect.bottom	= m_CurrContext->m_Height;
      //hReturn = m_pSwapChain->Present(0, dwFlags);
			if (m_CurrContext->m_pSwapChain)
				hReturn = m_CurrContext->m_pSwapChain->Present(0, dwFlags);
      if (hReturn == DXGI_ERROR_INVALID_CALL)
      {
        assert(0);
      }
      else
      if (DXGI_ERROR_NONEXCLUSIVE == hReturn)
        DXUTD3D10SetRenderingNonExclusive(true);
      else
      if (DXGI_STATUS_OCCLUDED == hReturn)
        DXUTD3D10SetRenderingOccluded(true);
      else
      if (DXGI_ERROR_DEVICE_RESET == hReturn)
      {
        assert(0);
      }
      else
      if (DXGI_ERROR_DEVICE_REMOVED == hReturn)
      {
        assert(0);
      }
      else
      if(SUCCEEDED(hReturn))
      {
        if(DXUTD3D10GetRenderingOccluded() )
        {
          // Now that we're no longer occluded
          // allow us to render again
          DXUTD3D10SetRenderingOccluded (false);
        }
        if(DXUTD3D10GetRenderingNonExclusive())
        {
          // Now that the other app has gone non-exclusive
          // allow us to render again
          DXUTD3D10SetRenderingNonExclusive (false);
        }
      }
#endif
		}
    m_nFrameSwapID++;
	}

#if defined (DIRECT3D9) || defined (OPENGL)
  hReturn = m_pd3dDevice->BeginScene();
#endif

	CheckDeviceLost();

  if (CV_r_envlightcmdebug)
  {
    for (int j=0; j<MAX_ENVLIGHTCUBEMAPS; j++)
    {
      SEnvTexture *pTex = &CTexture::m_EnvLCMaps[j];
      if (!pTex->m_bReady)
        continue;
      for (int i=0; i<6; i++)
      {
        if (CTexture::m_EnvLCMaps[j].m_nFrameCreated[i]>0)
        {
          CTexture::GetAverageColor(&CTexture::m_EnvLCMaps[j], i);
        }
      }
    }
  }


  if (CV_r_GetScreenShot)
  {
    ScreenShot();
    CV_r_GetScreenShot = 0;
  }
  
  if(m_bWireframeMode != m_bWireframeModePrev)
  {
    if(m_bWireframeMode)  // disable zpass in wireframe mode
    {
      m_bUseZpass = CV_r_usezpass != 0;
      CV_r_usezpass = 0;
    }
    else
    {
      CV_r_usezpass = m_bUseZpass;
    }
  }

  if (CRenderer::CV_r_logTexStreaming)
  {
    LogStrv(0, "******************************* EndFrame ********************************\n");
    LogStrv(0, "Loaded: %.3f Kb, UpLoaded: %.3f Kb, UploadTime: %.3fMs\n\n", CTexture::m_LoadBytes/1024.0f, CTexture::m_UploadBytes/1024.0f, m_RP.m_PS.m_fTexUploadTime);
  }

  /*if (CTexture::m_LoadBytes > 3.0f*1024.0f*1024.0f)
    CTexture::m_fStreamDistFactor = min(2048.0f, CTexture::m_fStreamDistFactor*1.2f);
  else
    CTexture::m_fStreamDistFactor = max(1.0f, CTexture::m_fStreamDistFactor/1.2f);*/
  CTexture::m_UploadBytes = 0;
  CTexture::m_LoadBytes = 0;

  m_bWireframeModePrev = m_bWireframeMode;

  m_SceneRecurseCount++;

  /*{
    char *str = (char *)EF_Query(EFQ_GetShaderCombinations);
    if (str)
    {
      EF_Query(EFQ_SetShaderCombinations, (INT_PTR)str);
      EF_Query(EFQ_DeleteMemoryArrayPtr, (INT_PTR)str);
    }
  }*/
}

//////////////////////////////////////////////////////////////////////////
bool CD3D9Renderer::ScreenShot(const char *filename, int iPreWidth)
{
  bool bRet = true;

#ifdef WIN32
	char path[ICryPak::g_nMaxPath];
	path[sizeof(path) - 1] = 0;
	gEnv->pCryPak->AdjustFileName(filename != 0 ? filename : "%USER%/ScreenShots", path, ICryPak::FLAGS_NO_MASTER_FOLDER_MAPPING | ICryPak::FLAGS_FOR_WRITING);

	if (!filename)
	{
		size_t pathLen = strlen(path);
		const char* pSlash = (!pathLen || path[pathLen - 1] == '/' || path[pathLen - 1] == '\\') ? "" : "\\";

		int i = 0;
		for (; i<10000; i++)
		{
			snprintf(&path[pathLen], sizeof(path) - 1 - pathLen, "%sScreenShot%04d.tga", pSlash, i);	

			FILE* fp = fxopen(path, "rb");
			if (!fp)
				break; // file doesn't exist

			fclose(fp);
		}

		if (i == 10000)
		{
			iLog->Log("Cannot save screen shot! Too many image files.");
			return false;
		}
	}

	if (!gEnv->pCryPak->MakeDir(PathUtil::GetParentDirectory(path)))
	{
		iLog->Log("Cannot save screen shot! Failed to create directory \"%s\".", path);
		return false;
	}

	iLog->Log(" ");
	iLog->Log("Screen shot: %s", path);
	gEnv->pConsole->ExecuteString("sys_RestoreSpec test*");		// to get useful debugging information about current spec settings to the log file
	iLog->Log(" ");

	const char *pExt = fpGetExtension(path);

	if (!pExt)
	{
		// no extension -> use jpg
		strcpy(&path[strlen(path)],".jpg");
		pExt = fpGetExtension(path);
	}

#if defined (DIRECT3D9) || defined (OPENGL)
#ifdef WIN64																						// we don't have JPEG lib for WIN64 so we call DirectX (currently less quality)
	if(stricmp(pExt,".jpg")==0)
	{
	  iLog->LogWarning("JPEG quality on WIN64 might be less than WIN32 (different library)\n");
		return CaptureFrameBufferToFile(path, false);
	}
#endif

	if(stricmp(pExt,".bmp")==0)
		return CaptureFrameBufferToFile(path, false);		// used for automated screenshot comparisons

	bool bTGA=false;
	if(stricmp(pExt,".tga")==0)
		bTGA=true;
/*
#ifdef WIN64
	bTGA=true;	  //TODO: we need a lib for AMD64 to create JPEG-files
#endif
*/
  LPDIRECT3DSURFACE9 pSysDeskSurf;
  D3DLOCKED_RECT d3dlrSys;
  int wdt = m_deskwidth;
  int hgt = m_deskheight;
  if (m_bFullScreen)
  {
    wdt = m_width;
    hgt = m_height;
  }
  HRESULT h = m_pd3dDevice->CreateOffscreenPlainSurface(wdt, hgt, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pSysDeskSurf, NULL);
  if (FAILED(h))
    return false;

  h = m_pd3dDevice->GetFrontBufferData(0, pSysDeskSurf);
  if (FAILED(h))
    return false;

	POINT WndP;
  WndP.x = 0;
  WndP.y = 0;
  ClientToScreen(m_CurrContext->m_hWnd, &WndP);
  h = pSysDeskSurf->LockRect(&d3dlrSys, NULL, D3DLOCK_READONLY);
  if (FAILED(h))
    return false;

  byte *src = (byte *)d3dlrSys.pBits;
	int height = m_height;
	int width = m_width;

	if (WndP.y < 0)
  {
    height += WndP.y;
    WndP.y = 0;
  }
  if (WndP.x < 0)
  {
    width += WndP.x;
    WndP.x = 0;
  }
  if (width+WndP.x >= wdt)
    width = wdt-WndP.x;
  if (height+WndP.y >= hgt)
    height = hgt-WndP.y;

  //iLog->Log("wdt: %d, hgt: %d, width: %d, height: %d, WndP.x: %d, WndP.y: %d", wdt, hgt, width, height, WndP.x, WndP.y);

	unsigned char *pic=new unsigned char [width*height*4];
  byte *dst = pic;

	if(bTGA)
	{
		// upside down
		src += (height+WndP.y-1)* d3dlrSys.Pitch;
		for (int i=0; i<height; i++)
		{
			for (int j=0; j<width; j++)
				*(uint *)&dst[j*4] = *(uint *)&src[(WndP.x+j)*4];
			dst += width*4;
			src -= d3dlrSys.Pitch;
		}
	}
	else
	{
		src += WndP.y * d3dlrSys.Pitch;
		// flip Red with Blue
		for (int i=0; i<height; i++)
		{
			for (int j=0; j<width; j++)
			{
				*(uint *)&dst[j*4] = *(uint *)&src[(WndP.x+j)*4];
				Exchange(dst[j*4+0], dst[j*4+2]);
			}
			dst += width*4;
			src += d3dlrSys.Pitch;
		}
	}
	int iPreHeight = (height * iPreWidth) / width;
	
	CryLogAlways("iPreWidth=%d",iPreWidth);
	CryLogAlways("iPreHeight=%d",iPreHeight);

	unsigned char *tmppic=new unsigned char [iPreWidth*iPreHeight*4];
	byte *tmp = tmppic;
	byte *tmpsrc = pic;

	if(iPreWidth>0 && iPreHeight>0)
	{
		int ilastyscale = 0;
		for (int i=0; i<iPreHeight; i++) 
		{
			for (int j=0; j<iPreWidth; j++)
			{
				int xscale = j*(width/iPreWidth);
				*(uint *)&tmp[j*4] = *(uint *)&tmpsrc[xscale*4];
			}
			int yscale = i*(height/iPreHeight);
			tmp += iPreWidth*4;
			tmpsrc += (yscale-ilastyscale)*width*4;
			ilastyscale = yscale;
		}
		width=iPreWidth;
		height=iPreHeight;
		pic = tmppic;
	}



  pSysDeskSurf->UnlockRect();
  SAFE_RELEASE(pSysDeskSurf);

	if(bTGA)
		bRet=::WriteTGA(pic, width, height, path, 32, 32);
	 else
		bRet=::WriteJPG(pic, width, height, path, 32);

  delete [] pic;

#elif defined (DIRECT3D10)
	return CaptureFrameBufferToFile(path, false);
#endif
#endif //WIN32

	return bRet;
}

int CD3D9Renderer::CreateRenderTarget (int nWidth, int nHeight, ETEX_Format eTF)
{
  // check if parameters are valid
  if(!nWidth || !nHeight)
    return -1;

#ifdef XENON
  if (nWidth > 512)
    nWidth = 512;
  if (nHeight > 512)
    nHeight = 512;
#endif

  if (!m_RTargets.Num())
  {
    m_RTargets.AddIndex(1);
  }

  int n = m_RTargets.Num();
  for (uint i=1; i<m_RTargets.Num(); i++)
  {
    if (!m_RTargets[i])
    {
      n = i;
      break;
    }
  }

  if(n == m_RTargets.Num())
  {
    m_RTargets.AddIndex(1);
  }
	
	char pName[128];
	sprintf(pName, "$RenderTarget_%d", n);
	m_RTargets[n] = CTexture::CreateRenderTarget(pName, nWidth, nHeight, eTT_2D, FT_NOMIPS, eTF);
 
  return m_RTargets[n]->GetID();
}

bool CD3D9Renderer::DestroyRenderTarget (int nHandle)
{
  CTexture *pTex=CTexture::GetByID(nHandle);  
  SAFE_RELEASE(pTex);
  return true;
}

bool CD3D9Renderer::SetRenderTarget (int nHandle)
{
 if (nHandle == 0)
 {
	  // Check: Restore not working
    FX_PopRenderTarget(0);
    return true;
  }

  CTexture *pTex=CTexture::GetByID(nHandle);
  gTexture2 = pTex;
  if(!pTex)
  {
    return false;
  }
#ifdef XENON
  //bool bRes = FX_PushRenderTarget(0, pTex, &CTexture::m_DepthBufferOrig, false);
  SD3DSurface *pDepthSurf = &m_DepthBufferOrig; //FX_GetDepthSurface(pTex->GetWidth(), pTex->GetHeight(), false);
  bool bRes = FX_PushRenderTarget(0, pTex, pDepthSurf, false);
#else
  SD3DSurface *pDepthSurf = FX_GetDepthSurface(pTex->GetWidth(), pTex->GetHeight(), false);
  bool bRes = FX_PushRenderTarget(0, pTex, pDepthSurf, false);
#endif

  return bRes;
}

void CD3D9Renderer::ReadFrameBuffer(unsigned char * pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX, int nScaledY)
{
#if defined (DIRECT3D9) || defined (OPENGL)
  int i;

  LPDIRECT3DSURFACE9 pSysSurf = NULL;
	LPDIRECT3DSURFACE9 pTmpSurface = NULL;
  D3DLOCKED_RECT d3dlrSys;
	D3DSURFACE_DESC desc;
  HRESULT hr;
  if (eRBType == eRB_BackBuffer || eRBType == eRB_ShadowBuffer)
  {
    LPDIRECT3DSURFACE9 pSurf;
    if (eRBType == eRB_BackBuffer)
      pSurf = m_RTStack[0][0].m_pTarget;
    else
    {
      CTexture *pTex = CTexture::m_Text_BackBuffer;
      LPDIRECT3DTEXTURE9 pID3DTexture = (LPDIRECT3DTEXTURE9)pTex->GetDeviceTexture();
      hr = pID3DTexture->GetSurfaceLevel(0, &pSurf);
    }
    if (!pSurf)
      return;
	  hr = pSurf->GetDesc(&desc);
	  if (FAILED(hr))
      return;
    POINT WndP;
    WndP.x = 0;
    WndP.y = 0;
    //if (m_bEditor)
    //  ClientToScreen(m_hWnd, &WndP);
    RECT srcRect, dstRect;
    srcRect.left = WndP.x;
    srcRect.right = nSizeX;
    srcRect.top = WndP.y;
    srcRect.bottom = nSizeY;
    if (nScaledX <= 0)
    {
      nScaledX = nSizeX;
      nScaledY = nSizeY;
    }
    dstRect.left = 0;
    dstRect.right = nScaledX;
    dstRect.top = 0;
    dstRect.bottom = nScaledY;
		hr = m_pd3dDevice->CreateRenderTarget(nScaledX, nScaledY, desc.Format, D3DMULTISAMPLE_NONE, 0, TRUE, &pTmpSurface, NULL);
		if ( FAILED(hr) )
      return;

    gcpRendD3D->m_RP.m_PS.m_RTCopied++;
    gcpRendD3D->m_RP.m_PS.m_RTCopiedSize += nScaledX * nScaledY * 4;
		hr = m_pd3dDevice->StretchRect(pSurf, &srcRect, pTmpSurface, &dstRect, D3DTEXF_LINEAR);
		if ( FAILED(hr) )
			return;
    if (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8)
    {
  	  // Create a buffer the same size and format
	    hr = m_pd3dDevice->CreateOffscreenPlainSurface(nScaledX, nScaledY, desc.Format, D3DPOOL_SYSTEMMEM, &pSysSurf, NULL);
      D3DXLoadSurfaceFromSurface(pSysSurf, NULL, NULL, pTmpSurface, NULL, NULL, D3DX_FILTER_NONE, 0);
      hr = pSysSurf->LockRect(&d3dlrSys, NULL, 0);
    }
    else
      hr = pTmpSurface->LockRect(&d3dlrSys, NULL, 0);
    byte *src = (byte *)d3dlrSys.pBits;
    byte *dst = pRGB;
		if (src && dst)
		{
			if (bRGBA)
			{
				for (i=0; i<nScaledY; i++)
				{
					int ni0 = (nScaledY-i-1)*nImageX*4;
					int ni1 = i * d3dlrSys.Pitch;
					for (int j=0; j<nScaledX; j++)
					{
						dst[ni0+j*4+0] = src[ni1+j*4+0];
						dst[ni0+j*4+1] = src[ni1+j*4+1];
						dst[ni0+j*4+2] = src[ni1+j*4+2];
						dst[ni0+j*4+3] = 255;
					}
				}
			}
			else
			{
				for (i=0; i<nScaledY; i++)
				{
					int ni0 = (nScaledY-i-1)*nImageX*3;
					int ni1 = i * d3dlrSys.Pitch;
					for (int j=0; j<nScaledX; j++)
					{
						dst[ni0+j*3+0] = src[ni1+j*4+0];
						dst[ni0+j*3+1] = src[ni1+j*4+1];
						dst[ni0+j*3+2] = src[ni1+j*4+2];
					}
				}
			}
    }
    if (pSysSurf)
      pSysSurf->UnlockRect();
    else
    if (pTmpSurface)
      pTmpSurface->UnlockRect();
    if (pSurf != m_RTStack[0][0].m_pTarget)
    {
      SAFE_RELEASE(pSurf);
    }
  }
  else
  {
    hr = m_pd3dDevice->CreateOffscreenPlainSurface(m_deskwidth, m_deskheight, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pSysSurf, NULL);
    hr = m_pd3dDevice->GetFrontBufferData(0, pSysSurf);
    POINT WndP;
    WndP.x = 0;
    WndP.y = 0;

#ifdef WIN32
    ClientToScreen(m_hWnd, &WndP);
#endif

    hr = pSysSurf->LockRect(&d3dlrSys, NULL, 0);
    byte *src = (byte *)d3dlrSys.pBits;
    byte *dst = pRGB;
    for (i=0; i<nSizeY; i++)
    {
      int ni0 = (nSizeY-i-1)*nImageX*4;
      for (int j=0; j<nSizeX; j++)
      {
        *(uint *)&dst[ni0+j*4] = *(uint *)&src[WndP.y*d3dlrSys.Pitch+(WndP.x+j)*4];
        Exchange(dst[ni0+j*4+0], dst[ni0+j*4+2]);
      }
      src += d3dlrSys.Pitch;
    }
    pSysSurf->UnlockRect();
  }
  SAFE_RELEASE(pTmpSurface);
  SAFE_RELEASE(pSysSurf);
#elif defined (DIRECT3D10) && (defined (WIN32) || defined(WIN64))
	if (!pRGB || nImageX <= 0 || nSizeX <= 0 || nSizeY <= 0 || eRBType != eRB_BackBuffer)
		return;

	assert(m_pBackBuffer);
	assert(!m_bEditor || m_CurrContext->m_pBackBuffer == m_pBackBuffer);

	ID3D10Texture2D* pBackBufferTex(0);

	D3D10_RENDER_TARGET_VIEW_DESC bbDesc;
	m_pBackBuffer->GetDesc(&bbDesc);
	if (bbDesc.ViewDimension == D3D10_RTV_DIMENSION_TEXTURE2DMS)
	{
		// TODO: resolve
	}

	m_pBackBuffer->GetResource((ID3D10Resource**) &pBackBufferTex);
	if (pBackBufferTex)
	{
		D3D10_TEXTURE2D_DESC dstDesc;
		dstDesc.Width = nScaledX <= 0 ? nSizeX : nScaledX;
		dstDesc.Height = nScaledY <= 0 ? nSizeY : nScaledY;
		dstDesc.MipLevels = 1;
		dstDesc.ArraySize = 1;
		dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstDesc.SampleDesc.Count = 1;
		dstDesc.SampleDesc.Quality = 0;
		dstDesc.Usage = D3D10_USAGE_STAGING;
		dstDesc.BindFlags = 0;
		dstDesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ/* | D3D10_CPU_ACCESS_WRITE*/;
		dstDesc.MiscFlags = 0;

		ID3D10Texture2D* pDstTex(0);
		if (SUCCEEDED(m_pd3dDevice->CreateTexture2D(&dstDesc, 0, &pDstTex)))
		{			
			D3D10_BOX srcBox;
			srcBox.left = 0; srcBox.right = nSizeX;
			srcBox.top = 0; srcBox.bottom = nSizeY;
			srcBox.front = 0; srcBox.back = 1;

			D3D10_BOX dstBox;
			dstBox.left = 0; dstBox.right = dstDesc.Width;
			dstBox.top = 0; dstBox.bottom = dstDesc.Height;
			dstBox.front = 0; dstBox.back = 1;

			D3DX10_TEXTURE_LOAD_INFO loadInfo;
			loadInfo.pSrcBox = &srcBox;
			loadInfo.pDstBox = &dstBox;
			loadInfo.SrcFirstMip = 0;
			loadInfo.DstFirstMip = 0;
			loadInfo.NumMips = 1;
			loadInfo.SrcFirstElement = D3D10CalcSubresource(0, 0, 1);
			loadInfo.DstFirstElement = D3D10CalcSubresource(0, 0, 1);
			loadInfo.NumElements  = 0;
			loadInfo.Filter = D3DX10_FILTER_LINEAR;
			loadInfo.MipFilter = D3DX10_FILTER_LINEAR;

#if D3DX10_SDK_VERSION < 35
			// D3DX10LoadTextureFromTexture() is broken in DXSDK from June so use August instead
			// TODO: To be remove once we can safely switch to a newer SDK
			typedef HRESULT (WINAPI *FP_D3DX10LoadTextureFromTexture)(ID3D10Resource*, D3DX10_TEXTURE_LOAD_INFO*, ID3D10Resource*);
			FP_D3DX10LoadTextureFromTexture pD3DX10LTFT((FP_D3DX10LoadTextureFromTexture) GetProcAddress(LoadLibrary("d3dx10_35.dll"), "D3DX10LoadTextureFromTexture"));
			if (pD3DX10LTFT && SUCCEEDED(pD3DX10LTFT(pBackBufferTex, &loadInfo, pDstTex)))
#else
			if (SUCCEEDED(D3DX10LoadTextureFromTexture(pBackBufferTex, &loadInfo, pDstTex)))
#endif
			{
				D3D10_MAPPED_TEXTURE2D mappedTex;
				if (SUCCEEDED(pDstTex->Map(0, D3D10_MAP_READ, 0, &mappedTex)))
				{
					if (bRGBA)
					{
						for (unsigned int i=0; i<dstDesc.Height; ++i)
						{
							uint8* pSrc((uint8*) mappedTex.pData + i * mappedTex.RowPitch);
							uint8* pDst((uint8*) pRGB + (dstDesc.Height - 1 - i) * nImageX * 4);
							for (unsigned int j=0; j<dstDesc.Width; ++j, pSrc += 4, pDst += 4)
							{
								pDst[0] = pSrc[2];
								pDst[1] = pSrc[1];
								pDst[2] = pSrc[0];
								pDst[3] = 255;
							}
						}
					}
					else
					{
						for (unsigned int i=0; i<dstDesc.Height; ++i)
						{
							uint8* pSrc((uint8*) mappedTex.pData + i * mappedTex.RowPitch);
							uint8* pDst((uint8*) pRGB + (dstDesc.Height - 1 - i) * nImageX * 3);
							for (unsigned int j=0; j<dstDesc.Width; ++j, pSrc += 4, pDst += 3)
							{
								pDst[0] = pSrc[2];
								pDst[1] = pSrc[1];
								pDst[2] = pSrc[0];
							}
						}
					}
					pDstTex->Unmap(0);
				}
			}
		}
		SAFE_RELEASE(pDstTex);
	}
	SAFE_RELEASE(pBackBufferTex);
#endif
}

void CD3D9Renderer::ReadFrameBufferFast(unsigned int* pDstARGBA8, int dstWidth, int dstHeight)
{
	if (!pDstARGBA8 || dstWidth <=0 || dstHeight <= 0)
		return;

#if defined(DIRECT3D9) || defined(OPENGL)
	IDirect3DSurface9* pSrcSurface(0);
	m_pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pSrcSurface);
	if (pSrcSurface)
	{
		D3DSURFACE_DESC srcDesc;
		pSrcSurface->GetDesc(&srcDesc);

		if (srcDesc.MultiSampleType == D3DMULTISAMPLE_NONE)
		{
			IDirect3DSurface9* pDstSurface(0);
			if (SUCCEEDED(m_pd3dDevice->CreateOffscreenPlainSurface(srcDesc.Width, srcDesc.Height, srcDesc.Format, D3DPOOL_SYSTEMMEM, &pDstSurface, 0)))
			{
				if (SUCCEEDED(m_pd3dDevice->GetRenderTargetData(pSrcSurface, pDstSurface)))
				{
					D3DLOCKED_RECT lrect;
					if (SUCCEEDED(pDstSurface->LockRect(&lrect, 0, 0 )))
					{
						int width(dstWidth > GetWidth() ? GetWidth() : dstWidth);
						int height(dstHeight > GetHeight() ? GetHeight() : dstHeight);
						
						for( int y(0); y < height; ++y )
						{
							unsigned int* pSrcData((unsigned int*)((size_t)lrect.pBits + y * lrect.Pitch));
							unsigned int* pDstData(&pDstARGBA8[y * dstWidth]);
							for( int x(0); x < width; ++x )
								pDstData[x] = pSrcData[x];
						}

						pDstSurface->UnlockRect();
					}
				}
			}
			SAFE_RELEASE(pDstSurface);
		}
	}
	SAFE_RELEASE(pSrcSurface);
#elif defined (DIRECT3D10) && (defined (WIN32) || defined(WIN64))

	assert(m_pBackBuffer);
	assert(!m_bEditor || m_CurrContext->m_pBackBuffer == m_pBackBuffer);

	D3D10_RENDER_TARGET_VIEW_DESC bbDesc;
	m_pBackBuffer->GetDesc(&bbDesc);
	if (bbDesc.ViewDimension == D3D10_RTV_DIMENSION_TEXTURE2DMS)
		return;

	ID3D10Texture2D* pBackBufferTex(0);
	m_pBackBuffer->GetResource((ID3D10Resource**) &pBackBufferTex);
	if (pBackBufferTex)
	{
		D3D10_TEXTURE2D_DESC dstDesc;
		dstDesc.Width = dstWidth > GetWidth() ? GetWidth() : dstWidth;
		dstDesc.Height = dstHeight > GetHeight() ? GetHeight() : dstHeight;
		dstDesc.MipLevels = 1;
		dstDesc.ArraySize = 1;
		dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstDesc.SampleDesc.Count = 1;
		dstDesc.SampleDesc.Quality = 0;
		dstDesc.Usage = D3D10_USAGE_STAGING;
		dstDesc.BindFlags = 0;
		dstDesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ/* | D3D10_CPU_ACCESS_WRITE*/;
		dstDesc.MiscFlags = 0;

		ID3D10Texture2D* pDstTex(0);
		if (SUCCEEDED(m_pd3dDevice->CreateTexture2D(&dstDesc, 0, &pDstTex)))
		{
			D3D10_TEXTURE2D_DESC srcDesc;
			pBackBufferTex->GetDesc(&srcDesc);

			D3D10_BOX srcBox;
			srcBox.left = 0; srcBox.right = dstDesc.Width;
			srcBox.top = 0; srcBox.bottom = dstDesc.Height;
			srcBox.front = 0; srcBox.back = 1;
			m_pd3dDevice->CopySubresourceRegion(pDstTex, 0, 0, 0, 0, pBackBufferTex, 0, &srcBox);

			D3D10_MAPPED_TEXTURE2D mappedTex;
			if (SUCCEEDED(pDstTex->Map(0, D3D10_MAP_READ, 0, &mappedTex)))
			{
				for (unsigned int i=0; i<dstDesc.Height; ++i)
				{
					uint8* pSrc((uint8*) mappedTex.pData + i * mappedTex.RowPitch);
					uint8* pDst((uint8*) pDstARGBA8 + i * dstWidth * 4);
					for (unsigned int j=0; j<dstDesc.Width; ++j, pSrc += 4, pDst += 4)
					{
						pDst[0] = pSrc[2];
						pDst[1] = pSrc[1];
						pDst[2] = pSrc[0];
						pDst[3] = 255;
					}
				}
				pDstTex->Unmap(0);
			}
		}
		SAFE_RELEASE(pDstTex);
	}
	SAFE_RELEASE(pBackBufferTex);
#endif
}

int CD3D9Renderer::ScreenToTexture()
{
  return 0;
}

void	CD3D9Renderer::Draw2dImage(float xpos,float ypos,float w,float h,int textureid,float s0,float t0,float s1,float t1,float angle,float r,float g,float b,float a,float z)
{
  if (CV_d3d9_forcesoftware)
    return;

  if (m_bDeviceLost)
    return;

  //////////////////////////////////////////////////////////////////////
  // Draw a textured quad, texture is assumed to be in video memory
  //////////////////////////////////////////////////////////////////////

  // Check for the presence of a D3D device
  assert(m_pd3dDevice);

  if (!m_SceneRecurseCount)
  {
    iLog->Log("ERROR: CD3D9Renderer::Draw2dImage before BeginScene");
    return;
  }

  PROFILE_FRAME(Draw_2DImage);

  DWORD col = D3DRGBA(r,g,b,a);

  bool bSaveZTest = ((m_RP.m_CurState & GS_NODEPTHTEST) == 0);
  SetCullMode(R_CULL_DISABLE);

  HRESULT hReturn = S_OK;

  xpos = (float)ScaleCoordX(xpos); w = (float)ScaleCoordX(w);
  ypos = (float)ScaleCoordY(ypos); h = (float)ScaleCoordY(h);
  float fx = xpos-0.5f;
  float fy = ypos-0.5f;
  float fw = w;
  float fh = h;

  EF_SelectTMU(0);

  if(textureid>0)
  {
    SetTexture(textureid);
    EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
  }
  else
  {
    EnableTMU(false);
    EF_SetColorOp(eCO_REPLACE, eCO_REPLACE, (eCA_Diffuse|(eCA_Diffuse<<3)), (eCA_Diffuse|(eCA_Diffuse<<3)));
  }

  //CTexture::m_Text_Fog->Apply(0);


  m_matProj->Push();
  Matrix44 *m = m_matProj->GetTop();
  mathMatrixOrthoOffCenterLH(m, 0.0f, (float)m_width, (float)m_height, 0.0f, -1e30f, 1e30f);
  m_matView->Push();
  m = m_matView->GetTop();
  m_matView->LoadIdentity();

  // Lock the entire buffer and obtain a pointer to the location where we have to
  // write the vertex data. Note that we specify zero here to lock the entire
  // buffer.

  int nOffs;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
  if (!vQuad)
  {
    m_matProj->Pop();
    m_matView->Pop();
    return;
  }

  // Now that we have write access to the buffer, we can copy our vertices
  // into it

  if (angle!=0)
  {
    float xsub=(float)(xpos+w/2.0f);
    float ysub=(float)(ypos+h/2.0f);

    float x,y,x1,y1;
    float mcos=cry_cosf(DEG2RAD(angle));
    float msin=cry_sinf(DEG2RAD(angle));

    x=xpos-xsub;
    y=ypos-ysub;
    x1=x*mcos-y*msin;
    y1=x*msin+y*mcos;
    x1+=xsub;y1+=ysub;

    // Define the quad
    vQuad[0].xyz.x = x1;
    vQuad[0].xyz.y = y1;

    x=xpos+w-xsub;
    y=ypos-ysub;
    x1=x*mcos-y*msin;
    y1=x*msin+y*mcos;
    x1+=xsub;y1+=ysub;

    vQuad[1].xyz.x = x1;//fx + fw;
    vQuad[1].xyz.y = y1;// fy;

    x=xpos+w-xsub;
    y=ypos+h-ysub;
    x1=x*mcos-y*msin;
    y1=x*msin+y*mcos;
    x1+=xsub;y1+=ysub;

    vQuad[3].xyz.x = x1;//fx + fw;
    vQuad[3].xyz.y = y1;//fy + fh;

    x=xpos-xsub;
    y=ypos+h-ysub;
    x1=x*mcos-y*msin;
    y1=x*msin+y*mcos;
    x1+=xsub;y1+=ysub;

    vQuad[2].xyz.x = x1;//fx;
    vQuad[2].xyz.y = y1;//fy + fh;
  }
  else
  {
    // Define the quad
    vQuad[0].xyz.x = fx;
    vQuad[0].xyz.y = fy;

    vQuad[1].xyz.x = fx + fw;
    vQuad[1].xyz.y = fy;

    vQuad[2].xyz.x = fx;
    vQuad[2].xyz.y = fy + fh;

    vQuad[3].xyz.x = fx + fw;
    vQuad[3].xyz.y = fy + fh;
  }

  // set uv's
#if defined(OPENGL)
  vQuad[0].st[0] = s0;
  vQuad[0].st[1] = t0;
  vQuad[1].st[0] = s1;
  vQuad[1].st[1] = t0;
  vQuad[2].st[0] = s0;
  vQuad[2].st[1] = t1;
  vQuad[3].st[0] = s1;
  vQuad[3].st[1] = t1;
#else
  vQuad[0].st[0] = s0;
  vQuad[0].st[1] = 1.0f-t0;

  vQuad[1].st[0] = s1;
  vQuad[1].st[1] = 1.0f-t0;

  vQuad[2].st[0] = s0;
  vQuad[2].st[1] = 1.0f-t1;

  vQuad[3].st[0] = s1;
  vQuad[3].st[1] = 1.0f-t1;
#endif

  // set data
	for(int i=0;i<4;i++)
	{
	  vQuad[i].color.dcolor = col;
		vQuad[i].xyz.z = z;
	}

  // We are finished with accessing the vertex buffer
  UnlockVB();

  FX_SetFPMode();
  // Bind our vertex as the first data stream of our device
  FX_SetVStream(0, m_pVB[POOL_P3F_COL4UB_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  if (FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
  {
    m_matView->Pop();
    m_matProj->Pop();
    return;
  }

  // Render the two triangles from the data stream
#if defined (DIRECT3D9) || defined (OPENGL)
  hReturn = m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, 2);
#elif defined (DIRECT3D10)
  SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  m_pd3dDevice->Draw(4, nOffs);
#endif

  EF_PopMatrix();
  m_matProj->Pop();
  if (FAILED(hReturn))
  {
    assert(hReturn);
    return;
  }
}

void	CD3D9Renderer::DrawImage(float xpos,float ypos,float w,float h,int texture_id,float s0,float t0,float s1,float t1,float r,float g,float b,float a)
{
	float s[4],t[4];

	s[0]=s0;	t[0]=1.0f-t0;
	s[1]=s1;	t[1]=1.0f-t0;
	s[2]=s1;	t[2]=1.0f-t1;
	s[3]=s0;	t[3]=1.0f-t1;

	DrawImageWithUV(xpos,ypos,0,w,h,texture_id,s,t,r,g,b,a);
}


void	CD3D9Renderer::DrawImageWithUV(float xpos,float ypos,float z,float w,float h,int texture_id,float s[4],float t[4],float r,float g,float b,float a)
{
	assert(s);
	assert(t);

	if (m_bDeviceLost)
    return;

  //////////////////////////////////////////////////////////////////////
  // Draw a textured quad, texture is assumed to be in video memory
  //////////////////////////////////////////////////////////////////////

  // Check for the presence of a D3D device
  assert(m_pd3dDevice);

  if (!m_SceneRecurseCount)
  {
    iLog->LogError("CD3D9Renderer::DrawImage before BeginScene");
    return;
  }

  PROFILE_FRAME(Draw_2DImage);

  HRESULT hReturn = S_OK;

  float fx = xpos;
  float fy = ypos;
  float fw = w;
  float fh = h;


  //if (!pID3DTexture)
  //  D3DXCreateTextureFromFile(m_pd3dDevice, "d:\\Textures\\Console\\DefaultConsole.tga", &pID3DTexture);

  SetCullMode(R_CULL_DISABLE);
  EnableTMU(true);
  EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);

  // Lock the entire buffer and obtain a pointer to the location where we have to
  // write the vertex data. Note that we specify zero here to lock the entire
  // buffer.
  int nOffs;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);

  DWORD col = D3DRGBA(r,g,b,a);

  // Now that we have write access to the buffer, we can copy our vertices
  // into it

  // Define the quad
#if !defined(OPENGL)
  vQuad[0].xyz.x = xpos;
  vQuad[0].xyz.y = ypos;
	vQuad[0].xyz.z = z;

  vQuad[1].xyz.x = xpos + w;
  vQuad[1].xyz.y = ypos;
	vQuad[1].xyz.z = z;

  vQuad[2].xyz.x = xpos + w;
  vQuad[2].xyz.y = ypos + h;
	vQuad[2].xyz.z = z;

  vQuad[3].xyz.x = xpos;
  vQuad[3].xyz.y = ypos + h;
	vQuad[3].xyz.z = z;
#else
  vQuad[0].xyz.x = xpos;
  vQuad[0].xyz.y = ypos + h;
	vQuad[0].xyz.z = z;

  vQuad[1].xyz.x = xpos + w;
  vQuad[1].xyz.y = ypos + h;
	vQuad[1].xyz.z = z;

  vQuad[2].xyz.x = xpos + w;
  vQuad[2].xyz.y = ypos;
	vQuad[2].xyz.z = z;

  vQuad[3].xyz.x = xpos;
  vQuad[3].xyz.y = ypos;
	vQuad[3].xyz.z = z;
#endif

	for(uint32 dwI=0;dwI<4;++dwI)
	{
		vQuad[dwI].color.dcolor = col;

		vQuad[dwI].st[0] = s[dwI];
	  vQuad[dwI].st[1] = t[dwI];
	}

  std::swap(vQuad[2], vQuad[3]);

  // We are finished with accessing the vertex buffer
  UnlockVB();

  // Activate the image texture
  SetTexture(texture_id);

  FX_SetFPMode();

  // Bind our vertex as the first data stream of our device
  FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  if (FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
    return;

  // Render the two triangles from the data stream
#if defined (DIRECT3D9) || defined (OPENGL)
  hReturn = m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, 2);
#elif defined (DIRECT3D10)
  SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  m_pd3dDevice->Draw(4, nOffs);
#endif

  if (FAILED(hReturn))
  {
    assert(hReturn);
    return;
  }
}

void CD3D9Renderer::DrawLines(Vec3 v[], int nump, ColorF& col, int flags, float fGround)
{
  if (m_bDeviceLost)
    return;
  if (nump <= 1)
    return;

  int i;
  int st;
  HRESULT hr = S_OK;

  EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, eCA_Texture | (eCA_Diffuse<<3), eCA_Texture | (eCA_Diffuse<<3));
  st = GS_NODEPTHTEST;
  if (flags & 1)
    st |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
  EF_SetState(st);
  CTexture::m_Text_White->Apply();

  DWORD c = D3DRGBA(col.r, col.g, col.b, col.a);
  int nOffs;

  if (fGround >= 0)
  {
    struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(nump*2, nOffs);

    for (i=0; i<nump; i++)
    {
      vQuad[i*2+0].xyz.x = v[i][0]; vQuad[i*2+0].xyz.y = fGround; vQuad[i*2+0].xyz.z = 0;
      vQuad[i*2+0].color.dcolor = c;
      vQuad[i*2+0].st[0] = 0; vQuad[i*2+0].st[1] = 0;
      vQuad[i*2+1].xyz = v[i];
      vQuad[i*2+1].color.dcolor = c;
      vQuad[i*2+1].st[0] = 0; vQuad[i*2+1].st[1] = 0;
    }
    // We are finished with accessing the vertex buffer
    UnlockVB();

    // Bind our vertex as the first data stream of our device
    FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
    FX_SetFPMode();
    if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
    {
  #if defined (DIRECT3D9) || defined (OPENGL)
      hr = m_pd3dDevice->DrawPrimitive(D3DPT_LINELIST, nOffs, nump);
  #elif defined (DIRECT3D10)
		  SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
		  m_pd3dDevice->Draw(nump, nOffs);
  #endif
    }
    if (FAILED(hr))
    {
      assert(hr);
      return;
    }
  }
  else
  {
    struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(nump, nOffs);

    for (i=0; i<nump; i++)
    {
      vQuad[i].xyz = v[i];
      vQuad[i].color.dcolor = c;
      vQuad[i].st[0] = 0; vQuad[i].st[1] = 0;
    }
    // We are finished with accessing the vertex buffer
    UnlockVB();

    // Bind our vertex as the first data stream of our device
    FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
    FX_SetFPMode();
    if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
    {
  #if defined (DIRECT3D9) || defined (OPENGL)
      hr = m_pd3dDevice->DrawPrimitive(D3DPT_LINESTRIP, nOffs, nump-1);
  #elif defined (DIRECT3D10)
		  SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
		  m_pd3dDevice->Draw(nump, nOffs);
  #endif
    }
    if (FAILED(hr))
    {
      assert(hr);
      return;
    }
  }
}

void CD3D9Renderer::DrawLine(const Vec3 & vPos1, const Vec3 & vPos2)
{
  if (m_bDeviceLost)
    return;

  if (!m_SceneRecurseCount)
  {
    iLog->Log("ERROR: CD3D9Renderer::DrawLine before BeginScene");
    return;
  }

  HRESULT hr = S_OK;

  SetCullMode(R_CULL_DISABLE);
  EnableTMU(true);
  CTexture::m_Text_White->Apply();

  // Lock the entire buffer and obtain a pointer to the location where we have to
  // write the vertex data. Note that we specify zero here to lock the entire
  // buffer.
  int nOffs;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(2, nOffs);

  // Define the line
  vQuad[0].xyz = vPos1;
  vQuad[0].color.dcolor = 0xffffffff;

  vQuad[1].xyz = vPos2;
  vQuad[1].color.dcolor = 0xffffffff;

  // We are finished with accessing the vertex buffer
  UnlockVB();

  // Bind our vertex as the first data stream of our device
  FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  FX_SetFPMode();
  if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
  {
  #if defined (DIRECT3D9) || defined (OPENGL)
    hr = m_pd3dDevice->DrawPrimitive(D3DPT_LINELIST, nOffs, 1);
  #elif defined (DIRECT3D10)
	  SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
	  m_pd3dDevice->Draw(2, nOffs);
  #endif
  }
  if (FAILED(hr))
  {
    assert(hr);
    return;
  }

}

void CD3D9Renderer::Graph(byte *g, int x, int y, int wdt, int hgt, int nC, int type, char *text, ColorF& color, float fScale)
{
  ColorF col;
  Vec3* vp = (Vec3*) alloca(wdt * sizeof(Vec3));
  int i;

  Set2DMode(true, m_width, m_height);
  EF_SetState(GS_NODEPTHTEST);
#if defined(PS3)
  col = ColorF(0.0f, 0.7f, 0.0f, 0.0f);
#else
  col = Col_Blue;
#endif
  int num = CTexture::m_Text_White->GetTextureID();

  float fy = (float)y;
  float fx = (float)x;
  float fwdt = (float)wdt;
  float fhgt = (float)hgt;

  DrawImage(fx, fy, fwdt, 2, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
  DrawImage(fx, fy+fhgt, fwdt, 2, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
  DrawImage(fx, fy, 2, fhgt, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
  DrawImage(fx+fwdt-2, fy, 2, fhgt, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);

  float fGround = CV_r_graphstyle ? fy+fhgt : -1;

  for (i=0; i<wdt; i++)
  {
    vp[i][0] = (float)i+fx;
    vp[i][1] = fy + (float)(g[i])*fhgt/255.0f;
    vp[i][2] = 0;
  }
  if (type == 1)
  {
    col = color;
    DrawLines(&vp[0], nC, col, 3, fGround);
    col = ColorF(1.0f) - col;
    col[3] = 1;
    DrawLines(&vp[nC], wdt-nC, col, 3, fGround);
  }
  else
  if (type == 2)
  {
    col = color;
    DrawLines(&vp[0], wdt, col, 3, fGround);
  }

  if (text)
  {
    WriteXY(4,y-18, 0.5f,1,1,1,1,1, text);
    WriteXY(wdt-260,y-18, 0.5f,1,1,1,1,1, "%d ms", (int)(1000.0f*fScale));
  }

  Set2DMode(false, 0, 0);
}


void CD3D9Renderer::DrawLineColor(const Vec3 & vPos1, const ColorF & vColor1, const Vec3 & vPos2, const ColorF & vColor2)
{
  if (m_bDeviceLost)
    return;

  if (!m_SceneRecurseCount)
  {
    iLog->Log("ERROR: CD3D9Renderer::DrawLine before BeginScene");
    return;
  }

  HRESULT hr = S_OK;

  //hr = D3DXCreateLine(m_pd3dDevice, &m_pLine);

  EnableTMU(true);
  CTexture::m_Text_White->Apply();
  EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);

  // tiago - fixed, it wasn't passing alpha parameter...
  DWORD col1 = D3DRGBA(vColor1[0],vColor1[1],vColor1[2], vColor1[3]);
  DWORD col2 = D3DRGBA(vColor2[0],vColor2[1],vColor2[2], vColor1[3]);

  int nOffs;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(2, nOffs);
  if (!vQuad)
    return;

  // Define the line
  vQuad[0].xyz = vPos1;
  vQuad[0].color.dcolor = col1;

  vQuad[1].xyz = vPos2;
  vQuad[1].color.dcolor = col2;

  // We are finished with accessing the vertex buffer
  UnlockVB();

  // Bind our vertex as the first data stream of our device
  FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  FX_SetFPMode();
  if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
  {
  #if defined (DIRECT3D9) || defined (OPENGL)
    hr = m_pd3dDevice->DrawPrimitive(D3DPT_LINELIST, nOffs, 1);
  #elif defined (DIRECT3D10)
	  SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
	  m_pd3dDevice->Draw(2, nOffs);
  #endif
  }
  if (FAILED(hr))
  {
    assert(hr);
    return;
  }
}

//*********************************************************************

void CD3D9Renderer::GetModelViewMatrix(float * mat)
{
  memcpy(mat, (D3DXMATRIX *)m_matView->GetTop(), 4*4*sizeof(float));
}

void CD3D9Renderer::GetProjectionMatrix(float * mat)
{
  memcpy(mat, (D3DXMATRIX *)m_matProj->GetTop(), 4*4*sizeof(float));
}

///////////////////////////////////////////
void CD3D9Renderer::PushMatrix()
{
  assert(m_pd3dDevice);

  EF_PushMatrix();
}

///////////////////////////////////////////
void CD3D9Renderer::PopMatrix()
{
  assert(m_pd3dDevice);

  EF_PopMatrix();
}

void CD3D9Renderer::SetPerspective(const CCamera &cam)
{
  Matrix44 *m = m_matProj->GetTop();
//  D3DXMatrixPerspectiveFovRH(m, cam.GetFov()*cam.GetProjRatio(), 1.0f/cam.GetProjRatio(), cam.GetNearPlane(), cam.GetFarPlane());
	mathMatrixPerspectiveFov(m, cam.GetFov(), cam.GetProjRatio(), cam.GetNearPlane(), cam.GetFarPlane());
  EF_DirtyMatrix();
}


//-----------------------------------------------------------------------------
// coded by ivo:
// calculate parameter for an off-center projection matrix.
// the projection matrix itself is calculated by D3D9.
//-----------------------------------------------------------------------------
D3DXMATRIX OffCenterProjection(const CCamera& cam, const Vec3& nv, unsigned short max, unsigned short win_width,  unsigned short win_height) {

  //get the size of near plane
  float l=+nv.x;
  float r=-nv.x;
  float b=-nv.z;
  float t=+nv.z;

  //---------------------------------------------------

  float max_x=(float)max;
  float max_z=(float)max;

  float win_x=(float)win_width;
  float win_z=(float)win_height;

  if ((win_x<max_x) && (win_z<max_z) ) {
    //calculate parameters for off-center projection-matrix
    float ext_x=-nv.x*2;
    float ext_z=+nv.z*2;
    l=+nv.x+(ext_x/max_x)*win_x;
    r=+nv.x+(ext_x/max_x)*(win_x+1);
    t=+nv.z-(ext_z/max_z)*win_z;
    b=+nv.z-(ext_z/max_z)*(win_z+1);
  }

  D3DXMATRIX m;
  mathMatrixPerspectiveOffCenter((Matrix44*)&m, l,r,b,t, cam.GetNearPlane(), cam.GetFarPlane());
  return m;
}

void CD3D9Renderer::SetRCamera(const CRenderCamera &cam)
{
  m_rcam = cam;
  D3DXMATRIX *m = (D3DXMATRIX*)m_matView->GetTop();
  cam.GetModelviewMatrix(*m);
  if (m_RP.m_PersFlags & RBPF_MIRRORCAMERA)
    m_matView->Scale(1,-1,1);
  m = (D3DXMATRIX*)m_matProj->GetTop();
  //cam.GetProjectionMatrix(*m);
  mathMatrixPerspectiveOffCenter((Matrix44*)m, cam.wL, cam.wR, cam.wB, cam.wT, cam.Near, cam.Far);
  EF_DirtyMatrix();
}

void CD3D9Renderer::SetCamera(const CCamera &cam)
{
  assert(m_pd3dDevice);

  const Matrix34& m34 = cam.GetMatrix();

  CRenderCamera c;
  float fov=cam.GetFov(); //*cam.GetProjRatio();
  c.Perspective(fov, cam.GetProjRatio(), cam.GetNearPlane(), cam.GetFarPlane());
  Vec3 vEye = cam.GetPosition();
  Vec3 vAt = vEye + Vec3(m34(0,1), m34(1,1), m34(2,1));
  Vec3 vUp = Vec3(m34(0,2), m34(1,2), m34(2,2));
  c.LookAt(vEye, vAt, vUp);
  SetRCamera(c);

  Matrix44 *m = m_matProj->GetTop();
  mathMatrixPerspectiveFov(m, fov, cam.GetProjRatio(), cam.GetNearPlane(), cam.GetFarPlane());
	//D3DXMatrixPerspectiveFovRH(m, fov, cam.GetProjRatio(), cam.GetNearPlane(), cam.GetFarPlane());

	if( SRendItem::m_RecurseLevel <= 1 && (m_RenderTileInfo.nGridSizeX > 1.f || m_RenderTileInfo.nGridSizeY > 1.f))
	{ // shift and scale viewport
		(*m).m00 *= m_RenderTileInfo.nGridSizeX;
		(*m).m11 *= m_RenderTileInfo.nGridSizeY;
		(*m).m20 =   (m_RenderTileInfo.nGridSizeX-1)-m_RenderTileInfo.nPosX*2.0f;
		(*m).m21 = -((m_RenderTileInfo.nGridSizeY-1)-m_RenderTileInfo.nPosY*2.0f);
	}

  Matrix44 mat = GetTransposed44(Matrix44(Matrix33::CreateRotationX(-gf_PI/2) * cam.GetMatrix().GetInverted()));
  m = m_matView->GetTop();
  m_matView->LoadMatrix(&mat);

  mat = cam.GetMatrix();
  mat(0, 3) = 0;
  mat(1, 3) = 0;
  mat(2, 3) = 0;
  m_CameraZeroMatrix = GetTransposed44(Matrix44(Matrix33::CreateRotationX(-gf_PI/2) * mat.GetInverted()));

  if (m_RP.m_PersFlags & RBPF_MIRRORCAMERA)
    m_matView->Scale(1,-1,1);

  m_RP.m_PersFlags |= RBPF_FP_MATRIXDIRTY;
  EF_SetCameraInfo();

#if defined (DIRECT3D9) || defined(OPENGL)
  m_NewViewport.MinZ = cam.GetZRangeMin();
  m_NewViewport.MaxZ = cam.GetZRangeMax();
  //m_NewViewport.MinZ = 0.1f;
#elif defined (DIRECT3D10)
  m_NewViewport.MinDepth = cam.GetZRangeMin();
  m_NewViewport.MaxDepth = cam.GetZRangeMax();
#endif
  m_bViewportDirty = true;

  m_cam = cam;
}

void CD3D9Renderer::SetRenderTile(f32 nTilesPosX, f32 nTilesPosY, f32 nTilesGridSizeX, f32 nTilesGridSizeY)
{
	m_RenderTileInfo.nPosX = nTilesPosX;
	m_RenderTileInfo.nPosY = nTilesPosY;
	m_RenderTileInfo.nGridSizeX = nTilesGridSizeX;
	m_RenderTileInfo.nGridSizeY = nTilesGridSizeY;
}

void CD3D9Renderer::SetViewport(int x, int y, int width, int height)
{
#if defined (DIRECT3D9) || defined (OPENGL)
  m_NewViewport.X = x;
  m_NewViewport.Y = y;
  m_NewViewport.Width = width;
  m_NewViewport.Height = height;
#elif defined (DIRECT3D10)
  m_NewViewport.TopLeftX = x;
  m_NewViewport.TopLeftY = y;
  m_NewViewport.Width = width;
  m_NewViewport.Height = height;
#endif
	m_RP.m_PersFlags2 |= RBPF2_COMMIT_PF;
  float fResolution = (float)((width + height) >> 1);
	if (fResolution != 0)
		CTexture::m_fResolutionRatio = log(max(1024.0f / fResolution, 1.0f));
	else
		CTexture::m_fResolutionRatio = 1.0f;
  m_bViewportDirty = true;
}

void CD3D9Renderer::GetViewport(int *x, int *y, int *width, int *height)
{
#if defined (DIRECT3D9) || defined (OPENGL)
  *x = m_NewViewport.X;
  *y = m_NewViewport.Y;
  *width = m_NewViewport.Width;
  *height = m_NewViewport.Height;
#elif defined (DIRECT3D10)
  *x = m_NewViewport.TopLeftX;
  *y = m_NewViewport.TopLeftY;
  *width = m_NewViewport.Width;
  *height = m_NewViewport.Height;
#endif
}

void CD3D9Renderer::SetScissor(int x, int y, int width, int height)
{
  if (!x && !y && !width && !height)
    EF_Scissor(false, x, y, width, height);
  else
    EF_Scissor(true, x, y, width, height);
}

void CD3D9Renderer::Flush3dBBox(const Vec3 &mins,const Vec3 &maxs,const bool bSolid)
{
  SetCullMode(R_CULL_DISABLE);
  EnableTMU(true);
  CTexture::m_Text_White->Apply();

  EF_Commit();

  HRESULT hr;

  int nOffs;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad;
  if(bSolid)
  {
    if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
    {
      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = maxs.x; vQuad[0].xyz.y = mins.y; vQuad[0].xyz.z = mins.z; //3
      vQuad[1].xyz.x = maxs.x; vQuad[1].xyz.y = mins.y; vQuad[1].xyz.z = maxs.z; //2
      vQuad[2].xyz.x = mins.x; vQuad[2].xyz.y = mins.y; vQuad[2].xyz.z = maxs.z; //1
      vQuad[3].xyz.x = mins.x; vQuad[3].xyz.y = mins.y; vQuad[3].xyz.z = mins.z; //0
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif

      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = mins.x; vQuad[0].xyz.y = mins.y; vQuad[0].xyz.z = mins.z; //0
      vQuad[1].xyz.x = mins.x; vQuad[1].xyz.y = mins.y; vQuad[1].xyz.z = maxs.z; //1
      vQuad[2].xyz.x = mins.x; vQuad[2].xyz.y = maxs.y; vQuad[2].xyz.z = maxs.z; //6
      vQuad[3].xyz.x = mins.x; vQuad[3].xyz.y = maxs.y; vQuad[3].xyz.z = mins.z; //4
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif

      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = mins.x; vQuad[0].xyz.y = maxs.y; vQuad[0].xyz.z = mins.z; //4
      vQuad[1].xyz.x = mins.x; vQuad[1].xyz.y = maxs.y; vQuad[1].xyz.z = maxs.z; //6
      vQuad[2].xyz.x = maxs.x; vQuad[2].xyz.y = maxs.y; vQuad[2].xyz.z = maxs.z; //7
      vQuad[3].xyz.x = maxs.x; vQuad[3].xyz.y = maxs.y; vQuad[3].xyz.z = mins.z; //5
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif

      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = maxs.x; vQuad[0].xyz.y = maxs.y; vQuad[0].xyz.z = mins.z; //54
      vQuad[1].xyz.x = maxs.x; vQuad[1].xyz.y = maxs.y; vQuad[1].xyz.z = maxs.z; //73
      vQuad[2].xyz.x = maxs.x; vQuad[2].xyz.y = mins.y; vQuad[2].xyz.z = maxs.z; //22
      vQuad[3].xyz.x = maxs.x; vQuad[3].xyz.y = mins.y; vQuad[3].xyz.z = mins.z; //31
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif

      // top
      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = maxs.x; vQuad[0].xyz.z = maxs.z; vQuad[0].xyz.y = mins.y; //3
      vQuad[1].xyz.x = maxs.x; vQuad[1].xyz.z = maxs.z; vQuad[1].xyz.y = maxs.y; //2
      vQuad[2].xyz.x = mins.x; vQuad[2].xyz.z = maxs.z; vQuad[2].xyz.y = maxs.y; //1
      vQuad[3].xyz.x = mins.x; vQuad[3].xyz.z = maxs.z; vQuad[3].xyz.y = mins.y; //0
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif

      // bottom
      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = maxs.x; vQuad[0].xyz.z = mins.z; vQuad[0].xyz.y = mins.y; //3
      vQuad[1].xyz.x = maxs.x; vQuad[1].xyz.z = mins.z; vQuad[1].xyz.y = maxs.y; //2
      vQuad[2].xyz.x = mins.x; vQuad[2].xyz.z = mins.z; vQuad[2].xyz.y = maxs.y; //1
      vQuad[3].xyz.x = mins.x; vQuad[3].xyz.z = mins.z; vQuad[3].xyz.y = mins.y; //0
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif
    }
  }
  else
  {
    if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
    {
      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = maxs.x; vQuad[0].xyz.y = mins.y; vQuad[0].xyz.z = mins.z; //3
      vQuad[1].xyz.x = maxs.x; vQuad[1].xyz.y = mins.y; vQuad[1].xyz.z = maxs.z; //2
      vQuad[2].xyz.x = mins.x; vQuad[2].xyz.y = mins.y; vQuad[2].xyz.z = maxs.z; //1
      vQuad[3].xyz.x = mins.x; vQuad[3].xyz.y = mins.y; vQuad[3].xyz.z = mins.z; //0
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_LINELIST, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif

      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = mins.x; vQuad[0].xyz.y = mins.y; vQuad[0].xyz.z = mins.z; //0
      vQuad[1].xyz.x = mins.x; vQuad[1].xyz.y = mins.y; vQuad[1].xyz.z = maxs.z; //1
      vQuad[2].xyz.x = mins.x; vQuad[2].xyz.y = maxs.y; vQuad[2].xyz.z = maxs.z; //6
      vQuad[3].xyz.x = mins.x; vQuad[3].xyz.y = maxs.y; vQuad[3].xyz.z = mins.z; //4
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_LINELIST, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif

      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = mins.x; vQuad[0].xyz.y = maxs.y; vQuad[0].xyz.z = mins.z; //4
      vQuad[1].xyz.x = mins.x; vQuad[1].xyz.y = maxs.y; vQuad[1].xyz.z = maxs.z; //6
      vQuad[2].xyz.x = maxs.x; vQuad[2].xyz.y = maxs.y; vQuad[2].xyz.z = maxs.z; //7
      vQuad[3].xyz.x = maxs.x; vQuad[3].xyz.y = maxs.y; vQuad[3].xyz.z = mins.z; //5
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_LINELIST, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif

      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = maxs.x; vQuad[0].xyz.y = mins.y; vQuad[0].xyz.z = mins.z; //54
      vQuad[1].xyz.x = maxs.x; vQuad[1].xyz.y = mins.y; vQuad[1].xyz.z = maxs.z; //73
      vQuad[2].xyz.x = maxs.x; vQuad[2].xyz.y = maxs.y; vQuad[2].xyz.z = maxs.z; //22
      vQuad[3].xyz.x = maxs.x; vQuad[3].xyz.y = maxs.y; vQuad[3].xyz.z = mins.z; //31
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_LINELIST, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif

      // top
      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(5, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = maxs.x; vQuad[0].xyz.z = maxs.z; vQuad[0].xyz.y = mins.y; //3
      vQuad[1].xyz.x = maxs.x; vQuad[1].xyz.z = maxs.z; vQuad[1].xyz.y = maxs.y; //2
      vQuad[2].xyz.x = mins.x; vQuad[2].xyz.z = maxs.z; vQuad[2].xyz.y = maxs.y; //1
      vQuad[3].xyz.x = mins.x; vQuad[3].xyz.z = maxs.z; vQuad[3].xyz.y = mins.y; //0
      vQuad[4].xyz.x = maxs.x; vQuad[4].xyz.z = maxs.z; vQuad[4].xyz.y = mins.y; //3
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_LINELIST, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif

      // bottom
      vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(5, nOffs);
      hr = FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      vQuad[0].xyz.x = maxs.x; vQuad[0].xyz.z = mins.z; vQuad[0].xyz.y = mins.y; //3
      vQuad[1].xyz.x = maxs.x; vQuad[1].xyz.z = mins.z; vQuad[1].xyz.y = maxs.y; //2
      vQuad[2].xyz.x = mins.x; vQuad[2].xyz.z = mins.z; vQuad[2].xyz.y = maxs.y; //1
      vQuad[3].xyz.x = mins.x; vQuad[3].xyz.z = mins.z; vQuad[3].xyz.y = mins.y; //0
      vQuad[4].xyz.x = maxs.x; vQuad[4].xyz.z = mins.z; vQuad[4].xyz.y = mins.y; //3
      UnlockVB();
  #if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_LINELIST, nOffs, 2);
  #elif defined (DIRECT3D10)
      assert(0);
  #endif
    }
  }
}

void CD3D9Renderer::Draw3dBBox(const Vec3 &mins,const Vec3 &maxs, int nPrimType)
{
  Draw3dPrim(mins, maxs, nPrimType);
}

void CD3D9Renderer::Draw3dPrim(const Vec3 &mins,const Vec3 &maxs, int nPrimType, const float* pRGBA)
{
  BBoxInfo info;
  info.vMins = mins;
  info.vMaxs = maxs;
  info.nPrimType = nPrimType;
  if (pRGBA)
  {
    for (int i = 0; i < 4; ++i)
      info.fColor[i] = pRGBA[i];
  }
  else
  {
    info.fColor[0] = m_RP.m_CurGlobalColor[0];
    info.fColor[1] = m_RP.m_CurGlobalColor[1];
    info.fColor[2] = m_RP.m_CurGlobalColor[2];
    info.fColor[3] = m_RP.m_CurGlobalColor[3];
  }
  m_arrBoxesToDraw.push_back(info);
}


void CD3D9Renderer::SetCullMode(int mode)
{
  //////////////////////////////////////////////////////////////////////
  // Change the culling mode
  //////////////////////////////////////////////////////////////////////

  assert(m_pd3dDevice);

  switch (mode)
  {
    case R_CULL_DISABLE:
      D3DSetCull(eCULL_None);
      break;
    case R_CULL_BACK:
      D3DSetCull(eCULL_Back);
      break;
    case R_CULL_FRONT:
      D3DSetCull(eCULL_Front);
      break;
  }
}

void CD3D9Renderer::SetFog(float density, float fogstart, float fogend, const float *color, int fogmode)
{
  //////////////////////////////////////////////////////////////////////
  // Configure the fog settings
  //////////////////////////////////////////////////////////////////////

  assert(m_pd3dDevice);

  m_FS.m_FogDensity = density;
  m_FS.m_FogStart = fogstart;
  m_FS.m_FogEnd = fogend;
  m_FS.m_nFogMode = fogmode;
  m_FS.m_nCurFogMode = fogmode;
  ColorF col = ColorF(color[0], color[1], color[2], 1.0f);
  m_FS.m_FogColor = col;

  // Fog color
  EF_SetFogColor(m_FS.m_FogColor);
}

void CD3D9Renderer::SetFogColor(float * color)
{
  m_FS.m_FogColor = ColorF(color[0],color[1],color[2],color[3]	);

  // Fog color
  EF_SetFogColor(m_FS.m_FogColor);
}

bool CD3D9Renderer::EnableFog(bool enable)
{
  //////////////////////////////////////////////////////////////////////
  // Enable or disable fog
  //////////////////////////////////////////////////////////////////////

  assert(m_pd3dDevice);

  bool bPrevFog = m_FS.m_bEnable; // remember fog value

  m_FS.m_bEnable = enable;

  return bPrevFog;
}

void CD3D9Renderer::SelectTMU(int tnum)
{
  EF_SelectTMU(tnum);
}

void CD3D9Renderer::EnableTMU(bool enable)
{
  assert(m_pd3dDevice);

  byte eCO = (enable ? eCO_MODULATE : eCO_REPLACE);
  EF_SetColorOp(eCO, eCO, 255, 255);
  if (!enable)
  {
#if defined (DIRECT3D9) || defined (OPENGL)
    m_pd3dDevice->SetTexture(CTexture::m_CurStage, NULL);
#elif defined (DIRECT3D10)
#endif
    CTexture::m_TexStages[CTexture::m_CurStage].m_Texture = NULL;
    if ((m_eCurColorArg[CTexture::m_CurStage]&7) == eCA_Texture)
    {
      m_eCurColorArg[CTexture::m_CurStage] = (m_eCurColorArg[CTexture::m_CurStage] & ~7) | eCA_Diffuse;
      m_RP.m_PersFlags |= RBPF_FP_DIRTY;
    }
    if ((m_eCurAlphaArg[CTexture::m_CurStage]&7) == eCA_Texture)
    {
      m_eCurAlphaArg[CTexture::m_CurStage] = (m_eCurAlphaArg[CTexture::m_CurStage] & ~7) | eCA_Diffuse;
      m_RP.m_PersFlags |= RBPF_FP_DIRTY;
    }
  }
}

///////////////////////////////////////////
void CD3D9Renderer::CheckError(const char *comment)
{
}

///////////////////////////////////////////
int CD3D9Renderer::SetPolygonMode(int mode)
{
  int prev_mode = m_polygon_mode;
  m_polygon_mode = mode;

  if (CV_d3d9_forcesoftware)
    return prev_mode;

#if defined (DIRECT3D9) || defined (OPENGL)
  if (m_polygon_mode == R_WIREFRAME_MODE)
    m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
  else
    m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#elif defined (DIRECT3D10)
  SStateRaster RS = m_StatesRS[m_nCurStateRS];
  RS.Desc.FillMode = m_polygon_mode == R_WIREFRAME_MODE ? D3D10_FILL_WIREFRAME : D3D10_FILL_SOLID;
  SetRasterState(&RS);
#endif

  return prev_mode;
}

///////////////////////////////////////////
void CD3D9Renderer::EnableVSync(bool enable)
{
  ChangeResolution(m_CVWidth->GetIVal(), m_CVHeight->GetIVal(), m_CVColorBits->GetIVal(), 75, m_CVFullScreen->GetIVal()!=0);
}

void CD3D9Renderer::DrawQuad(const Vec3 &right, const Vec3 &up, const Vec3 &origin,int nFlipMode/*=0*/)
{
  PROFILE_FRAME(Draw_2DImage);

  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F Verts[4];

  Vec3 curr;
  curr=origin+(-right-up);
  Verts[0].xyz[0] = curr[0]; Verts[0].xyz[1] = curr[1]; Verts[0].xyz[2] = curr[2];
  Verts[0].st[0] = -1; Verts[0].st[1] = 0;

  curr=origin+(right-up);
  Verts[1].xyz[0] = curr[0]; Verts[1].xyz[1] = curr[1]; Verts[1].xyz[2] = curr[2];
  Verts[1].st[0] = 0; Verts[1].st[1] = 0;

  curr=origin+(right+up);
  Verts[2].xyz[0] = curr[0]; Verts[2].xyz[1] = curr[1]; Verts[2].xyz[2] = curr[2];
  Verts[2].st[0] = 0; Verts[2].st[1] = 1;

  curr=origin+(-right+up);
  Verts[3].xyz[0] = curr[0]; Verts[3].xyz[1] = curr[1]; Verts[3].xyz[2] = curr[2];
  Verts[3].st[0] = -1; Verts[3].st[1] = 1;

  int nOffs;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
  memcpy(vQuad, Verts, 4*sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));

  UnlockVB();

  EF_Commit();

  FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F);
	FX_SetFPMode();
#if defined (DIRECT3D9) || defined (OPENGL)
  m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, nOffs, 2);
#elif defined (DIRECT3D10)
  assert(0);
#endif
  m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL] += 2;
  m_RP.m_PS.m_nDIPs[EFSLIST_GENERAL]++;
}

void CD3D9Renderer::DrawQuad(float dy,float dx, float dz, float x, float y, float z)
{
  PROFILE_FRAME(Draw_2DImage);

  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F Verts[4];

  Verts[0].xyz[0] = -dx+x; Verts[0].xyz[1] = dy+y; Verts[0].xyz[2] = -dz+z;
  Verts[0].st[0] = 0; Verts[0].st[1] = 0;

  Verts[1].xyz[0] = dx+x; Verts[1].xyz[1] = -dy+y; Verts[1].xyz[2] = -dz+z;
  Verts[1].st[0] = 1; Verts[1].st[1] = 0;

  Verts[2].xyz[0] = dx+x; Verts[2].xyz[1] = -dy+y; Verts[2].xyz[2] = dz+z;
  Verts[2].st[0] = 1; Verts[2].st[1] = 1;

  Verts[3].xyz[0] = -dx+x; Verts[3].xyz[1] = dy+y; Verts[3].xyz[2] = dz+z;
  Verts[3].st[0] = 0; Verts[3].st[1] = 1;

  int nOffs;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);
  memcpy(vQuad, Verts, 4*sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));

  UnlockVB();

  EF_Commit();

  FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  FX_SetFPMode();
  if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
  {
  #if defined (DIRECT3D9) || defined (OPENGL)
    m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, nOffs, 2);
  #elif defined (DIRECT3D10)
    assert(0);
  #endif

    m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL] += 2;
    m_RP.m_PS.m_nDIPs[EFSLIST_GENERAL]++;
  }
}

void CD3D9Renderer::DrawQuad(float x0, float y0, float x1, float y1, const ColorF & color, float z, float s0, float t0, float s1, float t1)
{
  PROFILE_FRAME(Draw_2DImage);

  ColorF c = color;
  c.NormalizeCol(c);
  DWORD col = D3DRGBA(c.r, c.g, c.b, c.a);

  int nOffs;
  struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs, POOL_TRP3F_COL4UB_TEX2F);

  float ftx0 = s0;
  float fty0 = t0;
  float ftx1 = s1;
  float fty1 = t1;

#ifdef XENON
  struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F Quad[4];
  struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *pDst = vQuad;
  vQuad = Quad;
#endif

  // Define the quad
  vQuad[0].pos = Vec4(x0, y0, z, 1.0f);
  vQuad[0].color.dcolor = col;
  vQuad[0].st = Vec2(ftx0, fty0);

  vQuad[1].pos = Vec4(x1, y0, z, 1.0f);
  vQuad[1].color.dcolor = col;
  vQuad[1].st = Vec2(ftx1, fty0);

  vQuad[3].pos = Vec4(x1, y1, z, 1.0f);
  vQuad[3].color.dcolor = col;
  vQuad[3].st = Vec2(ftx1, fty1);

  vQuad[2].pos = Vec4(x0, y1, z, 1.0f);
  vQuad[2].color.dcolor = col;
  vQuad[2].st = Vec2(ftx0, fty1);

#ifdef XENON
  memcpy(pDst, Quad, 4*sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F));
#endif

  UnlockVB(POOL_TRP3F_COL4UB_TEX2F);

  EF_Commit();

  FX_SetVStream(0, m_pVB[POOL_TRP3F_COL4UB_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F));
  if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_TRP3F_COL4UB_TEX2F)))
  {
  #if defined (DIRECT3D9) || defined (OPENGL)
    m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, 2);
  #elif defined (DIRECT3D10)
    SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pd3dDevice->Draw(4, nOffs);
  #endif

    m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL] += 2;
    m_RP.m_PS.m_nDIPs[EFSLIST_GENERAL]++;
  }
}

void CD3D9Renderer::DrawFullScreenQuad(CShader *pSH, const CCryName& TechName, float s0, float t0, float s1, float t1, uint nState)
{
  uint nPasses;
  pSH->FXSetTechnique(TechName);
  pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
  pSH->FXBeginPass(0);

  float fWidth5 = (float)m_NewViewport.Width-0.5f;
  float fHeight5 = (float)m_NewViewport.Height-0.5f;

  int nOffs;
  struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *Verts = (struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs, POOL_TRP3F_COL4UB_TEX2F);
  if( Verts )
  {
#if defined (DIRECT3D10) || defined (XENON)
    std::swap(t0, t1);
#endif
#ifdef XENON
    struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F SysVB[4];
    struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *pDst = Verts;
    Verts = &SysVB[0];
#endif
    Verts->pos = Vec4(-0.5f, -0.5f, 0.0f, 1.0f);
    Verts->st = Vec2(s0, t0);
    Verts->color.dcolor = (uint32)-1;
    Verts++;

    Verts->pos = Vec4(fWidth5, -0.5f, 0.0f, 1.0f);
    Verts->st = Vec2(s1, t0);
    Verts->color.dcolor = (uint32)-1;
    Verts++;

    Verts->pos = Vec4(-0.5f, fHeight5, 0.0f, 1.0f);
    Verts->st = Vec2(s0, t1);
    Verts->color.dcolor = (uint32)-1;
    Verts++;

    Verts->pos = Vec4(fWidth5, fHeight5, 0.0f, 1.0f);
    Verts->st = Vec2(s1, t1);
    Verts->color.dcolor = (uint32)-1;
#ifdef XENON
    memcpy(pDst, SysVB, sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F)*4);
#endif
  }
  UnlockVB(POOL_TRP3F_COL4UB_TEX2F);

  EF_Commit();

  // Draw a fullscreen quad to sample the RT
  EF_SetState(nState);
  if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_TRP3F_COL4UB_TEX2F)))
  {
    FX_SetVStream(0, m_pVB[POOL_TRP3F_COL4UB_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F));

  #if defined (DIRECT3D9) || defined(OPENGL)
    m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, 2);
  #elif defined (DIRECT3D10)
    SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pd3dDevice->Draw(4, nOffs);
  #endif
  }
  pSH->FXEndPass();

  EF_SelectTMU(0);

  m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL] += 2;
  m_RP.m_PS.m_nDIPs[EFSLIST_GENERAL]++;
}

void CD3D9Renderer::DrawQuad3D(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3,
                               const ColorF & color, float ftx0,  float fty0,  float ftx1,  float fty1)
{
  assert(color.r>=0.0f && color.r<=1.0f);
  assert(color.g>=0.0f && color.g<=1.0f);
  assert(color.b>=0.0f && color.b<=1.0f);
  assert(color.a>=0.0f && color.a<=1.0f);

  DWORD col = D3DRGBA(color.r, color.g, color.b, color.a);

  int nOffs;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(4, nOffs);

  // Define the quad
  vQuad[0].xyz.x = v0.x;
  vQuad[0].xyz.y = v0.y;
  vQuad[0].xyz.z = v0.z;
  vQuad[0].color.dcolor = col;
  vQuad[0].st[0] = ftx0;
  vQuad[0].st[1] = fty0;

  vQuad[1].xyz.x = v1.x;
  vQuad[1].xyz.y = v1.y;
  vQuad[1].xyz.z = v1.z;
  vQuad[1].color.dcolor = col;
  vQuad[1].st[0] = ftx1;
  vQuad[1].st[1] = fty0;

  vQuad[3].xyz.x = v2.x;
  vQuad[3].xyz.y = v2.y;
  vQuad[3].xyz.z = v2.z;
  vQuad[3].color.dcolor = col;
  vQuad[3].st[0] = ftx1;
  vQuad[3].st[1] = fty1;

  vQuad[2].xyz.x = v3.x;
  vQuad[2].xyz.y = v3.y;
  vQuad[2].xyz.z = v3.z;
  vQuad[2].color.dcolor = col;
  vQuad[2].st[0] = ftx0;
  vQuad[2].st[1] = fty1;

  UnlockVB();

  EF_Commit();
  FX_SetVStream(0, m_pVB[0], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
  if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB_TEX2F)))
  {
  #if defined (DIRECT3D9) || defined (OPENGL)
    m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, 2);
  #elif defined (DIRECT3D10)
    SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pd3dDevice->Draw(4, nOffs);
  #endif

    m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL] += 2;
    m_RP.m_PS.m_nDIPs[EFSLIST_GENERAL]++;
  }
}

///////////////////////////////////////////
void CD3D9Renderer::DrawTriStrip(CVertexBuffer *src, int vert_num)
{
  EF_Commit();
  if (!m_SceneRecurseCount)
  {
    iLog->Log("ERROR: CD3D9Renderer::DrawTriStrip before BeginScene");
    return;
  }
  HRESULT h;

  if (FAILED(EF_SetVertexDeclaration(0, src->m_nVertexFormat)))
    return;
  //FX_SetFPMode();

  switch (src->m_nVertexFormat)
  {
	case VERTEX_FORMAT_P3F:
		{
			struct_VERTEX_FORMAT_P3F *dt = (struct_VERTEX_FORMAT_P3F *)src->m_VS[VSF_GENERAL].m_VData;
			int nOffs;
			struct_VERTEX_FORMAT_P3F *Verts = (struct_VERTEX_FORMAT_P3F *)GetVBPtr(vert_num, nOffs, POOL_P3F);
			memcpy(Verts, dt, vert_num*sizeof(struct_VERTEX_FORMAT_P3F));
			UnlockVB(POOL_P3F);
			h = FX_SetVStream(0, m_pVB[POOL_P3F], 0, sizeof(struct_VERTEX_FORMAT_P3F));
#if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, vert_num-2);
#elif defined (DIRECT3D10)
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      m_pd3dDevice->Draw(vert_num, nOffs);
#endif
		}
		break;
  case VERTEX_FORMAT_P3F_TEX2F:
    {
      struct_VERTEX_FORMAT_P3F_TEX2F *dt = (struct_VERTEX_FORMAT_P3F_TEX2F *)src->m_VS[VSF_GENERAL].m_VData;
      int nOffs;
      struct_VERTEX_FORMAT_P3F_TEX2F *Verts = (struct_VERTEX_FORMAT_P3F_TEX2F *)GetVBPtr(vert_num, nOffs, POOL_P3F_TEX2F);
      memcpy(Verts, dt, vert_num*sizeof(struct_VERTEX_FORMAT_P3F_TEX2F));
      UnlockVB(POOL_P3F_TEX2F);

      h = FX_SetVStream(0, m_pVB[POOL_P3F_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_P3F_TEX2F));
#if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, vert_num-2);
#elif defined (DIRECT3D10)
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      m_pd3dDevice->Draw(vert_num, nOffs);
#endif
    }
    break;
  case VERTEX_FORMAT_P3F_COL4UB_TEX2F:
    {
      struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *dt = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)src->m_VS[VSF_GENERAL].m_VData;
      EF_SetVertColor();

      int nOffs;
      struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *Verts = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)GetVBPtr(vert_num, nOffs);
      memcpy(Verts, dt, vert_num*sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
      UnlockVB(POOL_P3F_COL4UB_TEX2F);

      h = FX_SetVStream(0, m_pVB[POOL_P3F_COL4UB_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
#if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, vert_num-2);
#elif defined (DIRECT3D10)
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      m_pd3dDevice->Draw(vert_num, nOffs);
#endif
    }
    break;
  case VERTEX_FORMAT_TRP3F_COL4UB_TEX2F:
    {
      struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *dt = (struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *)src->m_VS[VSF_GENERAL].m_VData;
      EF_SetVertColor();

      int nOffs;
      struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *Verts = (struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *)GetVBPtr(vert_num, nOffs, POOL_TRP3F_COL4UB_TEX2F);
      memcpy(Verts, dt, vert_num*sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F));
      UnlockVB(POOL_TRP3F_COL4UB_TEX2F);

      h = FX_SetVStream(0, m_pVB[POOL_TRP3F_COL4UB_TEX2F], 0, sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F));
#if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, vert_num-2);
#elif defined (DIRECT3D10)
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      m_pd3dDevice->Draw(vert_num, nOffs);
#endif
    }
    break;
  case VERTEX_FORMAT_P3F_TEX3F:
    {
      struct_VERTEX_FORMAT_P3F_TEX3F *dt = (struct_VERTEX_FORMAT_P3F_TEX3F *)src->m_VS[VSF_GENERAL].m_VData;
      int nOffs;
      struct_VERTEX_FORMAT_P3F_TEX3F *Verts = (struct_VERTEX_FORMAT_P3F_TEX3F *)GetVBPtr(vert_num, nOffs, POOL_P3F_TEX3F);
      memcpy(Verts, dt, vert_num*sizeof(struct_VERTEX_FORMAT_P3F_TEX3F));
      UnlockVB(POOL_P3F_TEX3F);

      h = FX_SetVStream(0, m_pVB[POOL_P3F_TEX3F], 0, sizeof(struct_VERTEX_FORMAT_P3F_TEX3F));

#if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, vert_num-2);
#elif defined (DIRECT3D10)
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      m_pd3dDevice->Draw(vert_num, nOffs);
#endif
    }
    break;
  case VERTEX_FORMAT_P3F_TEX2F_TEX3F:
    {
      struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F *dt = (struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F *)src->m_VS[VSF_GENERAL].m_VData;
      int nOffs;
      struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F *Verts = (struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F *)GetVBPtr(vert_num, nOffs, POOL_P3F_TEX2F_TEX3F);
      memcpy(Verts, dt, vert_num*sizeof(struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F));
      UnlockVB(POOL_P3F_TEX2F_TEX3F);

      h = FX_SetVStream(0, m_pVB[POOL_P3F_TEX2F_TEX3F], 0, sizeof(struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F));

#if defined (DIRECT3D9) || defined (OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, vert_num-2);
#elif defined (DIRECT3D10)
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      m_pd3dDevice->Draw(vert_num, nOffs);
#endif
    }
    break;

  default:
    assert(0);
    break;
  }

  m_RP.m_PS.m_nPolygons[EFSLIST_GENERAL] += vert_num-2;
  m_RP.m_PS.m_nDIPs[EFSLIST_GENERAL]++;
}

///////////////////////////////////////////
void CD3D9Renderer::ResetToDefault()
{
  if (m_LogFile)
    Logv(SRendItem::m_RecurseLevel, ".... ResetToDefault ....\n");

  m_RP.m_PersFlags |= RBPF_FP_DIRTY;

  EF_Scissor(false, 0, 0, 0, 0);

  for (int i=0; i<m_numtmus; i++)
  {
    EF_SelectTMU(i);

    if (!i)
    {
      EnableTMU(true);
    }
    else
    {
      EnableTMU(false);
    }
  }
  EF_SelectTMU(0);
  CTexture::m_nCurStages = 1;

#if defined (DIRECT3D10)
  SStateDepth DS;
  SStateBlend BS;
	SStateRaster RS;
  DS.Desc.DepthEnable = TRUE;
  DS.Desc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL;
  DS.Desc.DepthFunc = D3D10_COMPARISON_LESS_EQUAL;
  DS.Desc.StencilEnable = FALSE;
  SetDepthState(&DS, 0);

	RS.Desc.CullMode = (m_nCurStateRS != (uint) -1) ? m_StatesRS[m_nCurStateRS].Desc.CullMode : D3D10_CULL_BACK;
  RS.Desc.FillMode = D3D10_FILL_SOLID;
  SetRasterState(&RS);

  BS.Desc.BlendEnable[0] = FALSE;
  BS.Desc.BlendEnable[1] = FALSE;
  BS.Desc.BlendEnable[2] = FALSE;
  BS.Desc.BlendEnable[3] = FALSE;
  BS.Desc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
  BS.Desc.RenderTargetWriteMask[1] = D3D10_COLOR_WRITE_ENABLE_ALL;
  BS.Desc.RenderTargetWriteMask[2] = D3D10_COLOR_WRITE_ENABLE_ALL;
  BS.Desc.RenderTargetWriteMask[3] = D3D10_COLOR_WRITE_ENABLE_ALL;
  SetBlendState(&BS);
  m_pd3dDevice->GSSetShader(NULL);

#elif defined (DIRECT3D9) || defined (OPENGL)
  m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);

  m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  m_pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
  m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);

  //m_pd3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);

  if (m_pd3dCaps->RasterCaps & (D3DPRASTERCAPS_DEPTHBIAS | D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS))
    m_pd3dDevice->SetRenderState(D3DRS_DEPTHBIAS, 0);
  m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xf);
  EF_SetVertColor();
#endif

  m_RP.m_CurState = GS_DEPTHWRITE;
  EF_ResetPipe();

  m_RP.m_PersFlags &= ~RBPF_WASWORLDSPACE;
  m_RP.m_PersFlags2 &= ~RBPF2_LIGHTSTENCILCULL;

  m_FS.m_nCurFogMode = m_FS.m_nFogMode;

  if (m_LogFile && CV_r_log == 3)
    Logv(SRendItem::m_RecurseLevel, ".... End ResetToDefault ....\n");
}


///////////////////////////////////////////
void CD3D9Renderer::SetMaterialColor(float r, float g, float b, float a)
{
  EF_SetGlobalColor(r, g, b, a);
}

///////////////////////////////////////////
char * CD3D9Renderer::GetStatusText(ERendStats type)
{
  return "No status yet";
}

void sLogTexture (char *name, int Size);

void CD3D9Renderer::ProjectToScreen(float ptx, float pty, float ptz,float *sx, float *sy, float *sz )
{
  D3DXVECTOR3 vOut, vIn;
  vIn.x = ptx;
  vIn.y = pty;
  vIn.z = ptz;

	int32 vp[4];
#if defined (DIRECT3D9) || defined (OPENGL)
	vp[0] = m_NewViewport.X;
	vp[1] = m_NewViewport.Y;
#elif defined (DIRECT3D10)
  vp[0] = m_NewViewport.TopLeftX;
  vp[1] = m_NewViewport.TopLeftY;
#endif
	vp[2] = m_NewViewport.Width;
	vp[3] = m_NewViewport.Height;

  Matrix44 mIdent;
	mIdent.SetIdentity();
  mathVec3Project((Vec3*)&vOut, (Vec3*)&vIn, vp, m_matProj->GetTop(), m_matView->GetTop(), &mIdent);
  *sx = vOut.x * 100/m_NewViewport.Width;
  *sy = vOut.y * 100/m_NewViewport.Height;
  *sz = vOut.z;
}

/*
* Transform a point (column vector) by a 4x4 matrix.  I.e.  out = m * in
* Input:  m - the 4x4 matrix
*         in - the 4x1 vector
* Output:  out - the resulting 4x1 vector.
*/
static void transform_point(float out[4], const float m[16], const float in[4])
{
#define M(row,col)  m[col*4+row]
  out[0] = M(0, 0) * in[0] + M(0, 1) * in[1] + M(0, 2) * in[2] + M(0, 3) * in[3];
  out[1] = M(1, 0) * in[0] + M(1, 1) * in[1] + M(1, 2) * in[2] + M(1, 3) * in[3];
  out[2] = M(2, 0) * in[0] + M(2, 1) * in[1] + M(2, 2) * in[2] + M(2, 3) * in[3];
  out[3] = M(3, 0) * in[0] + M(3, 1) * in[1] + M(3, 2) * in[2] + M(3, 3) * in[3];
#undef M
}

static int sUnProject(float winx, float winy, float winz, const float model[16], const float proj[16], const int viewport[4], float * objx, float * objy, float * objz)
{
  /* matrice de transformation */
  float m[16], A[16];
  float in[4], out[4];
  /* transformation coordonnees normalisees entre -1 et 1 */
  in[0] = (winx - viewport[0]) * 2 / viewport[2] - 1.0f;
  in[1] = (winy - viewport[1]) * 2 / viewport[3] - 1.0f;
  in[2] = winz;//2.0f * winz - 1.0f;
  in[3] = 1.0;
  /* calcul transformation inverse */
  matmul4(A, proj, model);
  QQinvertMatrixf(m, A);
  /* d'ou les coordonnees objets */
  transform_point(out, m, in);
  if (out[3] == 0.0)
    return false;
  *objx = out[0] / out[3];
  *objy = out[1] / out[3];
  *objz = out[2] / out[3];
  return true;
}

int CD3D9Renderer::UnProject(float sx, float sy, float sz, float *px, float *py, float *pz, const float modelMatrix[16], const float projMatrix[16], const int viewport[4])
{
  return sUnProject(sx, sy, sz, modelMatrix, projMatrix, viewport, px, py, pz);
}

int CD3D9Renderer::UnProjectFromScreen( float sx, float sy, float sz, float *px, float *py, float *pz)
{
  float modelMatrix[16];
  float projMatrix[16];
  int viewport[4];

  GetModelViewMatrix(modelMatrix);
  GetProjectionMatrix(projMatrix);
  GetViewport(&viewport[0], &viewport[1], &viewport[2], &viewport[3]);
  return sUnProject(sx, sy, sz, modelMatrix, projMatrix, viewport, px, py, pz);
}

//////////////////////////////////////////////////////////////////////

void CD3D9Renderer::ClearBuffer(uint nFlags, ColorF *vColor, float depth)
{
  EF_ClearBuffers(nFlags, vColor, depth);
}

void CD3D9Renderer::EnableAALines(bool bEnable)
{
  assert(0);
}

int s_In2DMode = 0;

void CD3D9Renderer::Set2DMode(bool enable, int ortox, int ortoy,float znear,float zfar )
{
  Matrix44* m;
  if(enable)
  {
    assert(s_In2DMode == 0);
    s_In2DMode++;
    m_matProj->Push();
    m = m_matProj->GetTop();
    //mathMatrixOrthoOffCenterLH(m, 0.0, (float)ortox, (float)ortoy, 0.0, -1e30f, 1e30f);
		mathMatrixOrthoOffCenterLH(m, 0.0, (float)ortox, (float)ortoy, 0.0, znear,zfar );
    //D3DXMatrixOrthoOffCenterRH((D3DXMATRIX *)m, 0.0f, (float)ortox, 0.0f, (float)ortoy, 0.1f, 10);
		m_pD3DRenderAuxGeom->SetOrthoMode( true, (D3DXMATRIX*)m );
    EF_PushMatrix();
    m = m_matView->GetTop();
    m_matView->LoadIdentity();
  }
  else
  {
    assert(s_In2DMode == 1);
    s_In2DMode--;
		m_pD3DRenderAuxGeom->SetOrthoMode( false );
    m_matProj->Pop();
    EF_PopMatrix();
  }
  EF_SetCameraInfo();
}

void CD3D9Renderer::RemoveTexture(unsigned int TextureId)
{
  bool bShadow = false;

  SDynTexture_Shadow *pTX, *pNext;
  for (pTX=SDynTexture_Shadow::m_RootShadow.m_NextShadow; pTX != &SDynTexture_Shadow::m_RootShadow; pTX=pNext)
  {
    pNext = pTX->m_NextShadow;
    if (pTX->m_pTexture && pTX->m_pTexture->GetID() == TextureId)
    {
      delete pTX;
      bShadow = true;
    }
  }
  if (!bShadow)
  {
    CTexture *tp = CTexture::GetByID(TextureId);
    SAFE_RELEASE(tp);
  }
}

void CD3D9Renderer::UpdateTextureInVideoMemory(uint tnum, unsigned char *newdata,int posx,int posy,int w,int h,ETEX_Format eTFSrc)
{
  if (m_bDeviceLost)
    return;

  CTexture *pTex = CTexture::GetByID(tnum);
  pTex->UpdateTextureRegion(newdata, posx, posy, w, h, eTFSrc);
}

unsigned int CD3D9Renderer::DownLoadToVideoMemory(unsigned char *data,int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat, int filter, int Id, char *szCacheName, int flags)
{
	PROFILE_FRAME(DownLoadToVideoMemory);

  char name[128];
  if (!szCacheName)
    sprintf(name, "$AutoDownload_%d", m_TexGenID++);
  else
    strcpy(name, szCacheName);
  if (!nummipmap)
  {
    if (filter != FILTER_BILINEAR && filter != FILTER_TRILINEAR)
      flags |= FT_NOMIPS;
    nummipmap = 1;
  }
  else
  if (nummipmap < 0)
    nummipmap = CTexture::CalcNumMips(w, h);

  if (!repeat)
    flags |= FT_STATE_CLAMP;
  if (!szCacheName)
    flags |= FT_DONT_STREAM;
  flags |= FT_DONT_ANISO | FT_DONT_RESIZE;

#ifdef XENON
  int nSize = CTexture::TextureDataSize(w, h, 1, nummipmap, eTFSrc);
  switch (eTFSrc)
  {
    case eTF_DXT1:
      SwapEndian((int16 *)data, nSize/2);
      break;
    case eTF_DXT5:
      SwapEndian((int16 *)data, nSize/2);
      break;
    case eTF_A4R4G4B4:
      SwapEndian((int16 *)data, nSize/2);
      break;
    case eTF_A8R8G8B8:
      SwapEndian((int32 *)data, nSize/4);
      break;
  }
#endif

  if (Id > 0)
  {
    CTexture *pTex = CTexture::GetByID(Id);
    if (pTex)
    {
//      pTex->Create2DTexture(w, h, nummipmap, flags, data, eTFSrc, eTFDst);
			pTex->UpdateTextureRegion(data, 0, 0, w, h, eTFSrc);

      return Id;
    }
    else
      return 0;
  }
  else
  {
    CTexture *tp = CTexture::Create2DTexture(name, w, h, nummipmap, flags, data, eTFSrc, eTFDst);

    return tp->GetID();
  }
}

char*	CD3D9Renderer::GetVertexProfile(bool bSupportedProfile)
{
  EHWSProfile pr = eHWSP_VS_1_1;

  if (bSupportedProfile)
  {
    if (GetFeatures() & RFT_HW_PS20)
      pr = eHWSP_VS_2_0;
  }

  switch(pr)
  {
    case eHWSP_VS_1_1:
      return "PROFILE_VS_1_1";
    case eHWSP_VS_2_0:
      return "PROFILE_VS_2_0";
    default:
      return "Unknown";
  }
}

void CD3D9Renderer::Hint_DontSync( CTexture &rTex )
{
	if(!m_bNVLibInitialized)
		return;
/* 
	// commented as NVAPI needs a fix before the code becomes useful

	NVDX_ObjectHandle g_RTHandle = NVDX_OBJECT_NONE; 

	IDirect3DDevice9 *pDev = (IDirect3DDevice9 *)gGet_D3DDevice();

	DWORD dwHintValue = 0; 

	IDirect3DTexture9 *pTex = (IDirect3DTexture9 *)rTex.GetDeviceTexture();

	// test
//	D3DSURFACE_DESC desc;
//	pTex->GetLevelDesc(0,&desc);

	LPDIRECT3DSURFACE9 pTexSurf=0;
	HRESULT hRes = pTex->GetSurfaceLevel(0, &pTexSurf);

	if(!FAILED(hRes))
	{
		D3DSURFACE_DESC desc2;

		pTexSurf->GetDesc(&desc2);

		// get nvapi handle 
//		NvAPI_Status hr = NvAPI_D3D9_GetSurfaceHandle(pTexSurf, &g_RTHandle); 
		LPDIRECT3DSURFACE9 help=0;
		hRes = pDev->GetRenderTarget(0,&help);			assert(!FAILED(hRes));
		hRes = pDev->SetRenderTarget(0,pTexSurf);		assert(!FAILED(hRes));
		NvAPI_Status hr = NvAPI_D3D9_GetCurrentRenderTargetHandle(pDev, &g_RTHandle); 
		hRes = pDev->SetRenderTarget(0,help);	assert(!FAILED(hRes));
		
		if(help)
			help->Release();

		if(hr)
			iLog->LogError("NvAPI_D3D9_GetSurfaceHandle failed (%d)",hr);
		else
		{
			// send the hint to the driver. If the gpu  buffers were rendered just one frame apart we 
			// allow to skip sync 
			dwHintValue = 1; 
			hr = NvAPI_D3D9_SetResourceHint( 
				pDev, 
				g_RTHandle, 
				NvApiHints_Sli, 
				NvApiHints_Sli_InterfarameAwareForTexturing, 
				&dwHintValue); 

			if(hr)
				iLog->LogError("NvAPI_D3D9_SetResourceHint failed (%d)",hr);
//			else
//				iLog->Log("NvAPI_D3D9_SetResourceHint succeed");
		}

		SAFE_RELEASE(pTexSurf);
	}
*/
}


char*	CD3D9Renderer::GetPixelProfile(bool bSupportedProfile)
{
  EHWSProfile pr = eHWSP_PS_1_1;

  if (bSupportedProfile)
  {
    if (GetFeatures() & RFT_HW_PS20)
    {
      if ((GetFeatures() & RFT_HW_MASK) == RFT_HW_GFFX)
        pr = eHWSP_PS_2_X;
      else
        pr = eHWSP_PS_2_0;
    }
  }

  switch(pr)
  {
    case eHWSP_PS_1_1:
    case eHWSP_PS_1_3:
      return "PROFILE_PS_1_1";
    case eHWSP_PS_2_0:
    case eHWSP_PS_2_X:
      return "PROFILE_PS_2_0";
    default:
      return "Unknown";
  }
}

//CV_r_logVBuffers


const char *sStreamNames[] = {
	"VSF_GENERAL",
	"VSF_TANGENTS",
	"VSF_LMTC",
	"VSF_HWSKIN_INFO",
	"VSF_SH_INFO",
	"VSF_HWSKIN_SHAPEDEFORM_INFO",
	"VSF_HWSKIN_MORPHTARGET_INFO",
};

void CD3D9Renderer::GetLogVBuffers()
{
	CRenderMesh *pRM = CRenderMesh::m_RootGlobal.m_NextGlobal;
	int nNums = 0;
	while (pRM != &CRenderMesh::m_RootGlobal)
	{
		//pRM->GetMemoryUsage(Sizer,IRenderMesh::MEM_USAGE_ONLY_SYSTEM);
		if (pRM->m_pSysVertBuffer) 
		{
			int nTotal = 0;
			std::string final; 
			char tmp[128];
			//CryLog("%s. ", pRM->m_sSource);

			for (int i=0; i<VSF_NUM; i++)
			{
				int nSize = 0;

				if (pRM->m_pSysVertBuffer->m_VS[i].m_VData )//	GetStream(i, &nOffs))
				{
					if (i == VSF_GENERAL)
						nSize = pRM->m_nVertCount * m_VertexSize[pRM->m_pSysVertBuffer->m_nVertexFormat];
					else
						nSize = pRM->m_nVertCount * m_StreamSize[i];

					sprintf(tmp, "| %s | %d ", sStreamNames[i], nSize);
					final += tmp;
					nTotal += nSize;
				}
			}
			if (nTotal) {
				CryLog("%s | Total | %d %s",pRM->m_sSource, nTotal, final.c_str());
			}
		}
		pRM = pRM->m_NextGlobal;
		nNums++;
	}
}

void CD3D9Renderer::GetMemoryUsage(ICrySizer* Sizer)
{
  uint i, j, nSize;

  assert (Sizer);

  //SIZER_COMPONENT_NAME(Sizer, "GLRenderer");
  {
    SIZER_COMPONENT_NAME(Sizer, "Renderer static");
    for (j=0; j<MAX_LIST_ORDER; j++)
    {
      for (i=0; i<EFSLIST_NUM; i++)
      {
        Sizer->AddObject(&SRendItem::m_RendItems[j][i], SRendItem::m_RendItems[j][i].GetMemoryUsage());
      }
    }
    Sizer->AddObject(SRendItem::m_StartRI, sizeof(SRendItem::m_StartRI));
    Sizer->AddObject(SRendItem::m_EndRI, sizeof(SRendItem::m_EndRI));
    Sizer->AddObject(SRendItem::m_RenderLightGroups, sizeof(SRendItem::m_RenderLightGroups));
    Sizer->AddObject(SRendItem::m_ShadowsValidMask, sizeof(SRendItem::m_ShadowsValidMask));
    Sizer->AddObject(SRendItem::m_GroupProperty, sizeof(SRendItem::m_GroupProperty));
  }
  {
    SIZER_COMPONENT_NAME(Sizer, "Renderer dynamic");

    Sizer->AddObject(this, sizeof(CD3D9Renderer));
    Sizer->AddObject(&CRenderObject::m_Waves, CRenderObject::m_Waves.GetMemoryUsage());
    Sizer->AddObject(m_RP.m_VisObjects, MAX_REND_OBJECTS*sizeof(CRenderObject *));
    Sizer->AddObject(&m_RP.m_TempObjects, m_RP.m_TempObjects.GetMemoryUsage());
    Sizer->AddObject(&m_RP.m_Objects, m_RP.m_Objects.GetMemoryUsage());
    Sizer->AddObject(&m_RP.m_ObjectsPool, m_RP.m_nNumObjectsInPool * sizeof(CRenderObject));
    Sizer->AddObject(&m_RP.m_DLights, m_RP.m_DLights[1].GetMemoryUsage());
    Sizer->AddObject(&m_RP.m_Splashes, m_RP.m_Splashes.GetMemoryUsage());
		GetMemoryUsageParticleREs( Sizer );
    Sizer->AddObject(&CREClientPoly2D::m_PolysStorage, CREClientPoly2D::m_PolysStorage.GetMemoryUsage());
    for (i=0; i<4; i++)
    {
      Sizer->AddObject(&CREClientPoly::m_PolysStorage[i], CREClientPoly::m_PolysStorage[i].GetMemoryUsage());
    }
    Sizer->AddObject(&m_RP.m_Sprites, m_RP.m_Sprites.GetMemoryUsage());
    Sizer->AddObject(&m_RP.m_Polygons, m_RP.m_Polygons.GetMemoryUsage());
    nSize = 0;
    for (i=0; i<2; i++)
    {
      nSize += m_RP.m_TempMeshes[i].GetMemoryUsage();
    }
    Sizer->AddObject(&m_RP.m_TempMeshes, nSize);
    Sizer->AddObject(&gCurLightStyles, gCurLightStyles.GetMemoryUsage());

		m_pD3DRenderAuxGeom->GetMemoryUsage( Sizer );
  }
  {
    SIZER_COMPONENT_NAME(Sizer, "Renderer CryName");
    static CCryName sName;

    Sizer->AddObject(&sName, CCryName::GetMemoryUsage());
  }


  {
    SIZER_COMPONENT_NAME(Sizer, "Shaders");
    CCryName Name = CShader::mfGetClassName();
    SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
    if (pRL)
    {
      ResourcesMapItor itor;
      for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
      {
        CShader *sh = (CShader *)itor->second;
        if (!sh)
          continue;
        nSize = sh->Size(0);
        Sizer->AddObject(sh, nSize);
      }
    }
    {
      SIZER_COMPONENT_NAME(Sizer, "Shader manager");
      Sizer->AddObject(&m_cEF, m_cEF.Size());
    }
    {
      SIZER_COMPONENT_NAME(Sizer, "Shader resources");
      nSize = CShader::m_ShaderResources_known.GetMemoryUsage();
      Sizer->AddObject(&CShader::m_ShaderResources_known, nSize);
      for (i=0; i<CShader::m_ShaderResources_known.Num(); i++)
      {
        SRenderShaderResources *pSHR = CShader::m_ShaderResources_known[i];
        if (!pSHR)
          continue;
        nSize = pSHR->Size();
        Sizer->AddObject(pSHR, nSize);
      }
    }
    {
      SIZER_COMPONENT_NAME(Sizer, "Shader shared components");

      CCryName Name = CHWShader::mfGetClassName(eHWSC_Vertex);
      SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
      if (pRL)
      {
        ResourcesMapItor itor;
        for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
        {
          CHWShader *vs = (CHWShader *)itor->second;
          if (!vs)
            continue;
          nSize = vs->Size();
          Sizer->AddObject(vs, nSize);
        }
      }

      Name = CHWShader::mfGetClassName(eHWSC_Pixel);
      pRL = CBaseResource::GetResourcesForClass(Name);
      if (pRL)
      {
        ResourcesMapItor itor;
        for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
        {
          CHWShader *ps = (CHWShader *)itor->second;
          if (!ps)
            continue;
          nSize = ps->Size();
          Sizer->AddObject(ps, nSize);
        }
      }

      for (i=0; i<CLightStyle::m_LStyles.Num(); i++)
      {
        nSize = CLightStyle::m_LStyles[i]->Size();
        Sizer->AddObject(CLightStyle::m_LStyles[i], nSize);
      }
    }
  }
  {
    SIZER_COMPONENT_NAME(Sizer, "Mesh");

		for (CRenderMesh *pRM = CRenderMesh::m_RootGlobal.m_NextGlobal; pRM != &CRenderMesh::m_RootGlobal; pRM = pRM->m_NextGlobal)
			pRM->GetMemoryUsage(Sizer,IRenderMesh::MEM_USAGE_NO_BUFFERS);

    {
			SIZER_COMPONENT_NAME(Sizer, "Index buffers");

			for (CRenderMesh *pRM = CRenderMesh::m_RootGlobal.m_NextGlobal; pRM != &CRenderMesh::m_RootGlobal; pRM = pRM->m_NextGlobal)
				pRM->GetMemoryUsage(Sizer,IRenderMesh::MEM_USAGE_INDEX_BUFFERS);
		}

		for (int i=0; i<VERTEX_FORMAT_NUMS; i++)
		{
			static const char *names[] =
			{
				"SPipTangents",

				"VERTEX_FORMAT_P3F",
				"VERTEX_FORMAT_P3F_COL4UB",
				"VERTEX_FORMAT_P3F_TEX2F",
				"VERTEX_FORMAT_P3F_COL4UB_TEX2F",
				"VERTEX_FORMAT_P3F_N4B_COL4UB",
				"VERTEX_FORMAT_P4S_TEX2F",
				"VERTEX_FORMAT_P4S_COL4UB_TEX2F",

				"VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F",
				"VERTEX_FORMAT_TRP3F_COL4UB_TEX2F",
				"VERTEX_FORMAT_TRP3F_TEX2F_TEX3F",
				"VERTEX_FORMAT_P3F_TEX3F",
				"VERTEX_FORMAT_P3F_TEX2F_TEX3F",

				"VERTEX_FORMAT_TEX2F",
				"VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F",
				"VERTEX_FORMAT_COL4UB_COL4UB",
				"VERTEX_FORMAT_2xP3F_INDEX4UB",
				"VERTEX_FORMAT_SCATTER",
			};

			SIZER_COMPONENT_NAME(Sizer, names[i]);

			for (CRenderMesh *pRM = CRenderMesh::m_RootGlobal.m_NextGlobal; pRM != &CRenderMesh::m_RootGlobal; pRM = pRM->m_NextGlobal)
				pRM->GetMemoryUsage(Sizer, (IRenderMesh::EMemoryUsageArgument)(IRenderMesh::MEM_USAGE_VERTEX_BUFFERS + i));
    }
  }
  {
    SIZER_COMPONENT_NAME(Sizer, "Render elements");

    CRendElement *pRE = CRendElement::m_RootGlobal.m_NextGlobal;
    while (pRE != &CRendElement::m_RootGlobal)
    {
      nSize = pRE->Size();
      Sizer->AddObject(pRE, nSize);
      pRE = pRE->m_NextGlobal;
    }
  }
  {
    SIZER_COMPONENT_NAME(Sizer, "Texture Objects");

    CCryName Name = CTexture::mfGetClassName();
    SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
    if (pRL)
    {
      ResourcesMapItor itor;
      for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
      {
        CTexture *tp = (CTexture *)itor->second;
        if (!tp || tp->IsNoTexture())
          continue;
				tp->GetMemoryUsage(Sizer);
      }
    }
    Sizer->AddObject(&CTexture::m_EnvCMaps[0], sizeof(SEnvTexture)*MAX_ENVCUBEMAPS);
    Sizer->AddObject(&CTexture::m_EnvTexts[0], sizeof(SEnvTexture)*MAX_ENVTEXTURES);
    Sizer->AddObject(&CTexture::m_Templates[0], sizeof(CTexture)*EFTT_MAX);
  }
}

//====================================================================

ILog     *iLog;
IConsole *iConsole;
ITimer   *iTimer;
ISystem  *iSystem;

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_Render : public ISystemEventListener
{
public:
	virtual void OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
	{
    static bool bInside = false;
    if (bInside)
      return;
    bInside = true;
		switch (event)
		{
    case ESYSTEM_EVENT_LEVEL_LOAD_START:
      {
        if (gRenDev)
        {
          gRenDev->m_cEF.m_bActivated = false;
          gRenDev->m_bEnd = false;
        }
        STLALLOCATOR_CLEANUP
        if (CRenderer::CV_r_texpostponeloading)
          CTexture::m_bPrecachePhase = true;
      }
      break;
    case ESYSTEM_EVENT_LEVEL_LOAD_END:
      {
        if (gRenDev)
        {
          gRenDev->m_nFrameLoad++;
          if (!gRenDev->m_bEditor && !gRenDev->CheckDeviceLost())
            gRenDev->Reset();
          //STLALLOCATOR_CLEANUP
          gRenDev->m_bTemporaryDisabledSFX = false;
          gRenDev->m_cEF.m_Bin.InvalidateCache();
          gRenDev->m_cEF.mfSortResources();
          gRenDev->m_bEnd = true;
        }
        CTexture::Precache();
      }
      break;
		case ESYSTEM_EVENT_LEVEL_RELOAD:
			{
				STLALLOCATOR_CLEANUP;
				break;
			}
    case ESYSTEM_EVENT_RESIZE:
      {
#ifdef DIRECT3D10
#ifndef PS3
        if (!gcpRendD3D->m_bEditor && !gcpRendD3D->m_bInResolutionChange)
        {
          DXUTCheckChange();
        }
# endif
#endif
      }
      break;
    case ESYSTEM_EVENT_CHANGE_FOCUS:
      {
#ifndef EXCLUDE_CRI_SDK
				CVideoPlayer::OnAppFocusChanged(wparam == TRUE);
#endif
#ifdef DIRECT3D10
        if (!gcpRendD3D->m_bEditor && !gcpRendD3D->m_bInResolutionChange)
        {
          DXUTCheckChange();
        }
#endif
      }
      break;

		}
    bInside = false;
	}
};

static CSystemEventListner_Render g_system_event_listener_render;

extern "C" DLL_EXPORT IRenderer* PackageRenderConstructor(int argc, char* argv[], SCryRenderInterface *sp);
DLL_EXPORT IRenderer* PackageRenderConstructor(int argc, char* argv[], SCryRenderInterface *sp)
{
  gbRgb = false;

  iConsole = sp->ipConsole;
  iLog = sp->ipLog;
  iTimer = sp->ipTimer;
  iSystem   = sp->ipSystem;

	ModuleInitISystem(iSystem);

#ifdef DEBUGALLOC
#undef new
#endif
  gRenDev = (CRenderer *) (new CD3D9Renderer());
#ifdef DEBUGALLOC
#define new DEBUG_CLIENTBLOCK
#endif

  srand( GetTickCount() );

  g_CpuFlags = iSystem->GetCPUFlags();

	iSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_render);
  return gRenDev;
}

void *gGet_D3DDevice()
{
  return (void *)gcpRendD3D->m_pd3dDevice;
}
void *gGet_glReadPixels()
{
  return NULL;
}

//=========================================================================================
