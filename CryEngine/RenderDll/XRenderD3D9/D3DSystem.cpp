/*=============================================================================
  D3DSystem.cpp : D3D initialization / system functions.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "IDirectBee.h"			// connection to D3D9 wrapper

#ifdef WIN32
	#include "../Common/ATI/atimgpud.h"					// ATI multicpu detection
	#include "../Common/NVAPI/nvapi.h"					// NVAPI multicpu support api
  #pragma message (">>> include lib: NVAPI")
  #ifdef WIN64
    #pragma comment(lib,"../Common/NVAPI/nvapi64.lib")
  #else // WIN64
    #pragma comment(lib,"../Common/NVAPI/nvapi.lib")
  #endif // WIN64
#endif

#if defined(DIRECT3D9) && (defined(WIN32) || defined(WIN64))
#	include <ddraw.h>
#endif

#if !defined(PS3)
# if defined(DIRECT3D10)
#include <dxerr.h>
# else
#include <dxerr9.h>
# endif
#endif

#ifdef XENON
  D3DDevice* g_pd3dDevice;    // Our rendering device

//--------------------------------------------------------------------------------------
// Tiling scenarios
//
// There is one tiling scenario list for each resolution - 480p, 720p, and 1080i.
// It may seem redundant to specify the screen resolution in the struct since the lists
// are video mode specific, but on final hardware, this sample will be able to generate
// any size front buffer (and there will be only one list of scenarios).
//
// Note that each list must be NULL terminated, and that it is not necessary to
// initialize all 15 possible tiling rectangles.
//
// Also note that the tiling rectangles must be texture tile aligned (64x32 on alpha
// hardware, 32x32 on final hardware), unless a rectangle coordinate matches the
// dimensions of the front buffer (i.e. the rectangle touches an edge of the screen).
//--------------------------------------------------------------------------------------

// 640x480 tiling scenarios
CONST TILING_SCENARIO g_TilingScenarios480_NoTiling[] =
{
  {
    L"640x480 without tiling",
      640,
      480,
      1,
      0,
      D3DSEQM_PRECLIP,
    {
      { 0, 0, 640, 480 },
    }
  },
  { NULL }
};
CONST TILING_SCENARIO g_TilingScenarios480[] =
{
  {
    L"640x480 Horizontal Split",
      640,
      480,
      2,
      0,
      D3DSEQM_PRECLIP,
    {
      { 0,   0, 640, 240 },
      { 0, 240, 640, 480 }
    }
  },
  { NULL }
};

// 1280x720 tiling scenarios
CONST TILING_SCENARIO g_TilingScenarios720_NoTiling[] =
{
  {
    L"1280x720 without tiling",
      1280,
      720,
      1,
      0,
      D3DSEQM_PRECLIP,
    {
      { 0, 0, 1280, 720 },
    }
  },
  { NULL }
};
CONST TILING_SCENARIO g_TilingScenarios720[] =
{
  {
    L"1280x720 Horizontal Split",
      1280,
      720,
      2,
      0,
      D3DSEQM_PRECLIP,
    {
      { 0,   0, 1280, 384 },
      { 0, 384, 1280, 720 }
    }
  },
  {
    L"1280x720 Vertical Split",
      1280,
      720,
      2,
      0,
      D3DSEQM_PRECLIP,
    {
      {   0, 0,  640, 720 },
      { 640, 0, 1280, 720 }
    }
  },
  { NULL }
};

// 1920x1080 tiling scenarios
CONST TILING_SCENARIO g_TilingScenarios1080[] =
{
  {
    L"1920x1080 Horizontal Split",
      1920,
      1080,
      4,
      0,
      D3DSEQM_PRECLIP,
    {
      { 0,   0, 1920,  288 },
      { 0, 288, 1920,  576 },
      { 0, 576, 1920,  864 },
      { 0, 864, 1920, 1080 },
    }
  },
  { NULL }
};

//--------------------------------------------------------------------------------------
// Name: SetupPresentationParameters()
// Desc: Uses the full screen width and height info from a tiling scenario to configure
//       the D3D presentation parameters.  This should only be necessary on alpha
//       hardware, since final hardware will be able to handle many different sizes of
//       front buffer, regardless of the output video mode.
//--------------------------------------------------------------------------------------
HRESULT SetupPresentationParameters(CONST TILING_SCENARIO& Scenario, D3DPRESENT_PARAMETERS& d3dpp)
{
  if(Scenario.dwScreenWidth > 1920 || Scenario.dwScreenHeight > 1080)
  {
    iLog->LogError("Error: Cannot display the current scenario, since the screen dimensions are too large.");
  }

  XVIDEO_MODE VideoMode;
  ZeroMemory(&VideoMode, sizeof(VideoMode));
  XGetVideoMode(&VideoMode);

  BOOL bEnable720p  = VideoMode.dwDisplayWidth >= 1280;
  BOOL bEnable1080i = VideoMode.dwDisplayWidth >= 1920;

  if(bEnable720p)
  {
    d3dpp.BackBufferWidth  = 1280;
    d3dpp.BackBufferHeight = 720;
  }
  else
  if(bEnable1080i)
  {
    d3dpp.BackBufferWidth  = 1920;
    d3dpp.BackBufferHeight = 1080;
  }
  else
  {
    d3dpp.BackBufferWidth  = 640;
    d3dpp.BackBufferHeight = 480;
  }

  if(Scenario.dwScreenWidth != d3dpp.BackBufferWidth || Scenario.dwScreenHeight != d3dpp.BackBufferHeight)
  {
    iLog->LogError("Error: Tiling scenarios that do not match screen resolutions are");
    return 1;
  }

  return S_OK;
}

#endif

  UINT WINAPI GetD3D9ColorBits( D3DFORMAT fmt )
  {
    switch( fmt )
    {
#if !defined(XENON)
    case D3DFMT_R8G8B8:
      return 24;
    case D3DFMT_R3G3B2:
      return 8;
    case D3DFMT_A8R3G3B2:
      return 16;
#endif
    case D3DFMT_A8R8G8B8:
      return 32;
    case D3DFMT_X8R8G8B8:
      return 32;
    case D3DFMT_R5G6B5:
      return 16;
    case D3DFMT_X1R5G5B5:
      return 16;
    case D3DFMT_A1R5G5B5:
      return 16;
    case D3DFMT_A4R4G4B4:
      return 16;
    case D3DFMT_X4R4G4B4:
      return 16;
    case D3DFMT_A2B10G10R10:
      return 32;
    case D3DFMT_A8B8G8R8:
      return 32;
    case D3DFMT_A2R10G10B10:
      return 32;
    case D3DFMT_A16B16G16R16:
      return 64;
    case D3DFMT_A16B16G16R16F:
      return 64;
    case D3DFMT_A32B32G32R32F:
      return 128;
    case D3DFMT_G16R16:
      return 32;
    case D3DFMT_G16R16F:
      return 32;
    case D3DFMT_R16F:
      return 16;
    case D3DFMT_R32F:
      return 32;
    case D3DFMT_A8:
    case D3DFMT_L8:
      return 8;
    case D3DFMT_A8L8:
    case D3DFMT_V8U8:
      return 16;
    case D3DFMT_X8L8V8U8:
    case D3DFMT_Q8W8V8U8:
      return 32;
    case D3DFMT_V16U16:
      return 32;
    case D3DFMT_L6V5U5:
      return 16;
    case D3DFMT_DXT1:
      return 8;
    case D3DFMT_DXT3:
      return 16;
    case D3DFMT_DXT5:
      return 16;
    case D3DFMT_3DC:
      return 16;
    case D3DFMT_D24S8:
      return 32;
    case D3DFMT_D16:
      return 16;
    case D3DFMT_DF16:
      return 16;
    case D3DFMT_DF24:
      return 24;
    case D3DFMT_NULL:
      return 8; //hack

    default:
      assert(0);
      return 0;
    }
  }

  int GetD3D9ColorBytes( const D3DFORMAT format )
  {
    switch( format )
    {
      // 1 byte formats
#if !defined(XENON)
    case D3DFMT_R3G3B2:
#endif
    case D3DFMT_A8:
    case D3DFMT_L8:
      {
        return( 1 );
      }	
      // 2 byte formats
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
#if !defined(XENON)
    case D3DFMT_A8R3G3B2:
    case D3DFMT_D15S1:
#endif
    case D3DFMT_X4R4G4B4:
    case D3DFMT_A8L8:
    case D3DFMT_V8U8:
    case D3DFMT_L6V5U5:
    case D3DFMT_D16:
    case D3DFMT_L16:
    case D3DFMT_INDEX16:
    case D3DFMT_R16F:
      {
        return( 2 );
      }
      // 3 byte formats
#if !defined(XENON)
    case D3DFMT_R8G8B8:
      {
        return( 3 );
      }
#endif
      // 4 byte formats
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
    case D3DFMT_A2B10G10R10:
    case D3DFMT_A8B8G8R8:
    case D3DFMT_X8B8G8R8:
    case D3DFMT_G16R16:
    case D3DFMT_A2R10G10B10:
    case D3DFMT_X8L8V8U8:
    case D3DFMT_Q8W8V8U8:
    case D3DFMT_V16U16:
    case D3DFMT_A2W10V10U10:	
    case D3DFMT_D32:
    case D3DFMT_D24S8:
    case D3DFMT_D24X8:
    case D3DFMT_D24FS8:
    case D3DFMT_INDEX32:
    case D3DFMT_G16R16F:
    case D3DFMT_R32F:
      {
        return( 4 );
      }
      // 8 byte formats
    case D3DFMT_A16B16G16R16:
    case D3DFMT_Q16W16V16U16:
    case D3DFMT_A16B16G16R16F:
    case D3DFMT_G32R32F:
      {
        return( 8 );
      }
      // 16 byte formats
    case D3DFMT_A32B32G32R32F:
      {
        return( 16 );
      }		
      // all others
    default:
      {
        assert( !"Unknown or unsupported D3DFORMAT!" );
        return( 0 );
      }
    }
  }

  int GetD3D9NumSamples( const D3DMULTISAMPLE_TYPE& msType, const unsigned int& quality )
  {
    switch( msType )
    {
    case D3DMULTISAMPLE_NONE:
      {
        return( 1 );
      }
    case D3DMULTISAMPLE_NONMASKABLE:
      {
        return( quality + 1 );
      }
    case D3DMULTISAMPLE_2_SAMPLES:
    case D3DMULTISAMPLE_4_SAMPLES:
#if !defined(XENON) && !defined(PS3)
    case D3DMULTISAMPLE_3_SAMPLES:
    case D3DMULTISAMPLE_5_SAMPLES:
    case D3DMULTISAMPLE_6_SAMPLES:
    case D3DMULTISAMPLE_7_SAMPLES:
    case D3DMULTISAMPLE_8_SAMPLES:
    case D3DMULTISAMPLE_9_SAMPLES:
    case D3DMULTISAMPLE_10_SAMPLES:
    case D3DMULTISAMPLE_11_SAMPLES:
    case D3DMULTISAMPLE_12_SAMPLES:
    case D3DMULTISAMPLE_13_SAMPLES:
    case D3DMULTISAMPLE_14_SAMPLES:
    case D3DMULTISAMPLE_15_SAMPLES:
    case D3DMULTISAMPLE_16_SAMPLES:
#endif
      {
        return( msType );
      }
    default:
      {
        assert( !"Unknown or unsupported D3DMULTISAMPLE_TYPE!" );
        return( 0 );
      }
    }
  }


void CD3D9Renderer::DisplaySplash()
{
#ifdef WIN32
  if (m_bEditor)
    return;

  HBITMAP hImage = (HBITMAP)LoadImage(GetModuleHandle(0), "splash.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

  if (hImage != INVALID_HANDLE_VALUE)
  {
    RECT rect;
    HDC hDC = GetDC(m_hWnd);
    HDC hDCBitmap = CreateCompatibleDC(hDC);
    BITMAP bm;

    GetClientRect(m_hWnd, &rect);
    GetObject(hImage, sizeof(bm), &bm);
    SelectObject(hDCBitmap, hImage);

    DWORD x = rect.left + (((rect.right-rect.left)-bm.bmWidth) >> 1);
    DWORD y = rect.top + (((rect.bottom-rect.top)-bm.bmHeight) >> 1);


//    BitBlt(hDC, x, y, bm.bmWidth, bm.bmHeight, hDCBitmap, 0, 0, SRCCOPY);

		RECT Rect;
		GetWindowRect( m_hWnd, &Rect );
		StretchBlt( hDC, 0, 0, Rect.right - Rect.left, Rect.bottom - Rect.top,  hDCBitmap, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY );

    DeleteObject(hImage);
    DeleteDC(hDCBitmap);
  }
#endif
}

bool CD3D9Renderer::CreateMSAADepthBuffer()
{
  HRESULT hr = S_OK;
#if defined (DIRECT3D9) || defined (OPENGL)
  if (CV_r_fsaa)
  {
    if (m_RP.m_FSAAData.Type != (D3DMULTISAMPLE_TYPE)CV_r_fsaa_samples || m_RP.m_FSAAData.Quality != CV_r_fsaa_quality || !m_RP.m_FSAAData.m_pZBuffer)
    {
      m_RP.m_FSAAData.Type = (D3DMULTISAMPLE_TYPE)CV_r_fsaa_samples;
      m_RP.m_FSAAData.Quality = CV_r_fsaa_quality;
      SAFE_RELEASE(m_RP.m_FSAAData.m_pZBuffer);
      hr = m_pd3dDevice->CreateDepthStencilSurface(m_width, m_height, D3DFMT_D24S8, m_RP.m_FSAAData.Type, m_RP.m_FSAAData.Quality, FALSE, &m_RP.m_FSAAData.m_pZBuffer, NULL);
      assert(hr == S_OK);
      m_DepthBufferOrigFSAA.pSurf = m_RP.m_FSAAData.m_pZBuffer;
    }
  }
  else
  {
    m_RP.m_FSAAData.Type = D3DMULTISAMPLE_NONE;
    m_RP.m_FSAAData.Quality = 0;

    SAFE_RELEASE(m_RP.m_FSAAData.m_pZBuffer);
    m_DepthBufferOrigFSAA.pSurf = m_pZBuffer;
  }
#elif defined (DIRECT3D10)
  if (CV_r_fsaa)
  {
    if (m_RP.m_FSAAData.Type != CV_r_fsaa_samples || m_RP.m_FSAAData.Quality != CV_r_fsaa_quality)
    {
      SAFE_RELEASE(m_RP.m_FSAAData.m_pZBuffer);
      SAFE_RELEASE(m_RP.m_FSAAData.m_pDepthTex);
    }
    m_RP.m_FSAAData.Type = CV_r_fsaa_samples;
    m_RP.m_FSAAData.Quality = CV_r_fsaa_quality;
    if (m_RP.m_FSAAData.Type > 1 && !m_RP.m_FSAAData.m_pZBuffer)
    {
      DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();

      // Create depth stencil texture
      ID3D10Texture2D* pDepthStencil = NULL;
      D3D10_TEXTURE2D_DESC descDepth;
      descDepth.Width = m_width;
      descDepth.Height = m_height;
      descDepth.MipLevels = 1;
      descDepth.ArraySize = 1;
      descDepth.Format = pDev->d3d10.AutoDepthStencilFormat;
      descDepth.SampleDesc.Count = m_RP.m_FSAAData.Type;
      descDepth.SampleDesc.Quality = m_RP.m_FSAAData.Quality;
      descDepth.Usage = D3D10_USAGE_DEFAULT;
      descDepth.BindFlags = D3D10_BIND_DEPTH_STENCIL;
      descDepth.CPUAccessFlags = 0;
      descDepth.MiscFlags = 0;
      hr = m_pd3dDevice->CreateTexture2D( &descDepth, NULL, &m_RP.m_FSAAData.m_pDepthTex);
      if (FAILED(hr))
        return false;
      m_DepthBufferOrigFSAA.pTex = m_RP.m_FSAAData.m_pDepthTex;

      // Create the depth stencil view
      D3D10_DEPTH_STENCIL_VIEW_DESC descDSV;
      descDSV.Format = descDepth.Format;
      descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
      descDSV.Texture2D.MipSlice = 0;
      hr = m_pd3dDevice->CreateDepthStencilView(m_RP.m_FSAAData.m_pDepthTex, &descDSV, &m_RP.m_FSAAData.m_pZBuffer);
      if (FAILED(hr))
        return false;
      m_DepthBufferOrigFSAA.pSurf = m_RP.m_FSAAData.m_pZBuffer;
    }
  }
  else
  {
    m_RP.m_FSAAData.Type = 0;
    m_RP.m_FSAAData.Quality = 0;

    SAFE_RELEASE(m_RP.m_FSAAData.m_pZBuffer);
    SAFE_RELEASE(m_RP.m_FSAAData.m_pDepthTex);
    m_DepthBufferOrigFSAA.pSurf = m_pZBuffer;
  }
#endif
  return (hr == S_OK);
}

bool CD3D9Renderer::ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen)
{
	if (m_bDeviceLost)
		return true;

#if !defined(XENON)
  iLog->Log("Change resolution: %dx%dx%d (%s)", nNewWidth, nNewHeight, nNewColDepth, bFullScreen ? "Fullscreen" : "Windowed");

  m_bInResolutionChange = false;
  int nPrevWidth = CRenderer::m_width;
  int nPrevHeight = CRenderer::m_height;
  int nPrevColorDepth = CRenderer::m_cbpp;
  bool bPrevFullScreen = m_bFullScreen;
  if (nNewWidth < 512)
    nNewWidth = 512;
  if (nNewHeight < 300)
    nNewHeight = 300;
  if (nNewColDepth < 24)
    nNewColDepth = 16;
  else
    nNewColDepth = 32;
  bool bNeedReset = (m_VSync != CV_r_vsync || nNewColDepth != nPrevColorDepth || bPrevFullScreen != bFullScreen || nNewHeight != nPrevHeight || nNewWidth != nPrevWidth);
  /*if (!bFullScreen && !m_bEditor)
  {
    if (nNewWidth > m_deskwidth-16)
      nNewWidth = m_deskwidth-16;
    if (nNewHeight > m_deskheight-32)
      nNewHeight = m_deskheight-32;
  }*/

  // Save the new dimensions
  m_width  = nNewWidth;
  m_height = nNewHeight;
  m_cbpp   = nNewColDepth;
  m_bFullScreen = bFullScreen;
  if (!m_bEditor)
    m_VSync = CV_r_vsync;
  else
    m_VSync = 0;
  if (bFullScreen && nNewColDepth == 16)
  {
    m_zbpp = 16;
    m_sbpp = 0;
  }

  DeleteContext(m_hWnd);

  DXUTDeviceSettings *pDevSettings = DXUTGetCurrentDeviceSettings();
  pDevSettings->d3d9.pp.Windowed = !m_bFullScreen;

  RestoreGamma();

  if (m_bEditor)
  {
    nNewWidth = m_deskwidth;
    nNewHeight = m_deskheight;
  }
  HRESULT hr = S_OK;
  if (bNeedReset)
  {
    pDevSettings->d3d9.pp.BackBufferWidth = nNewWidth;
    pDevSettings->d3d9.pp.BackBufferHeight = nNewHeight;
    hr = DXUTCreateDevice(!m_bFullScreen, nNewWidth, nNewHeight, m_sbpp, m_cbpp);
#if defined (DIRECT3D9) || defined (OPENGL)
    OnD3D9PostCreateDevice(DXUTGetD3D9Device());
#else
    DXUTCheckForWindowSizeChange(m_width, m_height);
    DXUTCheckForWindowChangingMonitors();
    OnD3D10PostCreateDevice(DXUTGetD3D10Device());
#endif
    nNewWidth = m_width;
    nNewHeight = m_height;
    SetViewport(0, 0, nNewWidth, nNewHeight);
  }
  AdjustWindowForChange();

#ifdef WIN32
  if (!bFullScreen && !m_bEditor)
  {
    int x = (m_deskwidth-CRenderer::m_width)/2;
    int y = (m_deskheight-CRenderer::m_height)/2;
    int wdt = GetSystemMetrics(SM_CXDLGFRAME)*2 + CRenderer::m_width;
    int hgt = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CXDLGFRAME)*2 + CRenderer::m_height;
    SetWindowPos(m_hWnd, HWND_NOTOPMOST, x, y, wdt, hgt, SWP_SHOWWINDOW);
  }
  else
  if (m_bFullScreen)
  {
    iLog->Log("Final resolution: %dx%dx%d (%s)", m_width, m_height, nNewColDepth, bFullScreen ? "Fullscreen" : "Windowed");
  }
#endif //WIN32

  CreateMSAADepthBuffer();

  CreateContext(m_hWnd, CV_r_fsaa != 0);

#if defined (DIRECT3D9) || defined (OPENGL)
  hr = m_pd3dDevice->BeginScene();
#endif

  ICryFont *pCryFont = iSystem->GetICryFont();
  if (pCryFont)
  {
    IFFont *pFont = pCryFont->GetFont("default");
  }
  /*if (m_CVWidth)
  m_CVWidth->Set(CRenderer::m_width);
  if (m_CVHeight)
  m_CVHeight->Set(CRenderer::m_height);
  if (m_CVFullScreen)
  m_CVFullScreen->Set(m_bFullScreen);
  if (m_CVColorBits)
  m_CVColorBits->Set(CRenderer::m_cbpp);
  ChangeViewport(0, 0, CRenderer::m_width, CRenderer::m_height);*/

  m_bDeviceLost = false;

  if (m_bFullScreen)
    SetGamma(CV_r_gamma+m_fDeltaGamma, CV_r_brightness, CV_r_contrast, true);
  EF_ResetPipe();
  CHWShader::m_pCurVS = NULL;
  CHWShader::m_pCurPS = NULL;

#if defined (DIRECT3D9) || defined (OPENGL)
  for (int i=0; i<224; i++)
  {
    CHWShader_D3D::m_CurPSParams[i] = Vec4(0,0,0,0);
  }
  for (int i=0; i<256; i++)
  {
    CHWShader_D3D::m_CurVSParams[i] = Vec4(0,0,0,0);
  }
#endif

  for (int i=0; i<MAX_TMU; i++)
  {
    CTexture::m_TexStages[i].m_Texture = NULL;
  }
  m_nFrameReset++;
#else
  if (CV_d3d9_predicatedtiling != m_Predicated || !m_pZBuffer)
  {
    m_Predicated = CV_d3d9_predicatedtiling;
    if (m_d3dpp.BackBufferWidth > 1280 || CV_r_fsaa)
    {
      m_Predicated = 1;
      _SetVar("d3d9_PredicatedTiling", 1);
    }
    bool bEnable720p  = m_d3dpp.BackBufferWidth >= 1280;
    bool bEnable1080i = m_d3dpp.BackBufferWidth >= 1920;
    if (!m_Predicated)
    {
      if (bEnable720p)
        m_pTilingScenarios = g_TilingScenarios720_NoTiling;
      else
        m_pTilingScenarios = g_TilingScenarios480_NoTiling;
    }
    else
    {
      bool bEnable720p  = m_d3dpp.BackBufferWidth >= 1280;
      bool bEnable1080i = m_d3dpp.BackBufferWidth >= 1920;

      if (bEnable720p)
        m_pTilingScenarios = g_TilingScenarios720;
      else
      if (bEnable1080i)
        m_pTilingScenarios = g_TilingScenarios1080;
      else
        m_pTilingScenarios = g_TilingScenarios480;

      // Set up the screen extents query mode that tiling will use.
      CONST TILING_SCENARIO& CurrentScenario = m_pTilingScenarios[m_dwTilingScenarioIndex];
      m_pd3dDevice->SetScreenExtentQueryMode(CurrentScenario.ScreenExtentQueryMode);
    }
  }

  if (CV_r_fsaa)
  {
    m_RP.m_FSAAData.Type = CV_r_fsaa_samples==2 ? D3DMULTISAMPLE_2_SAMPLES : D3DMULTISAMPLE_4_SAMPLES;
    m_RP.m_FSAAData.Quality = CV_r_fsaa_quality;
  }
  D3DSURFACE_PARAMETERS Params;
  Params.Base = (5*1024*1024) / GPU_EDRAM_TILE_SIZE;
  Params.HierarchicalZBase = 0;
  Params.ColorExpBias = 0;
  int nWidth = m_d3dpp.BackBufferWidth;
  int nHeight = m_d3dpp.BackBufferHeight;
  FX_GetRTDimensions(true, nWidth, nHeight);
  HRESULT hr = m_pd3dDevice->CreateDepthStencilSurface(nWidth, nHeight, D3DFMT_D24S8, m_RP.m_FSAAData.Type, 0, FALSE, &m_pZBuffer, &Params);
  m_RTStack[0][0].m_pDepth = m_pZBuffer;
  if(FAILED(hr))
  {
    iLog->LogError("Error: Cannot create tiling depth/stencil.");
  }
  if (CV_r_fsaa)
  {
    if (m_RP.m_FSAAData.Type != D3DMULTISAMPLE_NONE && !m_RP.m_FSAAData.m_pZBuffer)
    {
      hr = m_pd3dDevice->CreateDepthStencilSurface(nWidth, nHeight, D3DFMT_D24S8, m_RP.m_FSAAData.Type, m_RP.m_FSAAData.Quality, FALSE, &m_RP.m_FSAAData.m_pZBuffer, &Params);
      assert(hr == S_OK);
      m_DepthBufferOrigFSAA.pSurf = m_RP.m_FSAAData.m_pZBuffer;
    }
  }
  else
  {
    m_RP.m_FSAAData.Type = D3DMULTISAMPLE_NONE;

    SAFE_RELEASE(m_RP.m_FSAAData.m_pZBuffer);
    m_DepthBufferOrigFSAA.pSurf = m_pZBuffer;
    m_DepthBufferOrig.pSurf = m_pZBuffer;
  }
#endif

  m_bInResolutionChange = false;

  return true;
}



//-----------------------------------------------------------------------------
// Name: CD3D9Renderer::AdjustWindowForChange()
// Desc: Prepare the window for a possible change between windowed mode and
//       fullscreen mode.  This function is virtual and thus can be overridden
//       to provide different behavior, such as switching to an entirely
//       different window for fullscreen mode (as in the MFC sample apps).
//-----------------------------------------------------------------------------
HRESULT CD3D9Renderer::AdjustWindowForChange()
{
#if !defined(XENON) && !defined(PS3)
  if (m_bEditor)
    return S_OK;

	if( !m_bFullScreen )
  {
    // Set windowed-mode style
    SetWindowLong( m_hWnd, GWL_STYLE, m_dwWindowStyle );
  }
  else
  {
    // Set fullscreen-mode style
    SetWindowLong( m_hWnd, GWL_STYLE, WS_POPUP|WS_VISIBLE );
  }
  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
#if defined (DIRECT3D9) || defined (OPENGL)
  if (m_width != m_pd3dpp->BackBufferWidth || m_height != m_pd3dpp->BackBufferHeight)
  {
    m_width = m_pd3dpp->BackBufferWidth;
    m_height = m_pd3dpp->BackBufferHeight;
    _SetVar("r_Width", m_width);
    _SetVar("r_Height", m_height);

    int x, y, wdt, hgt;
    if (m_bFullScreen)
    {
      x = 0;
      y = 0;
      wdt = m_width;
      hgt = m_height;
      SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, wdt, hgt, SWP_SHOWWINDOW);
    }
    else
    {
      x = (GetSystemMetrics(SM_CXFULLSCREEN)-m_width)/2;
      y = (GetSystemMetrics(SM_CYFULLSCREEN)-m_height)/2;
      wdt = GetSystemMetrics(SM_CXDLGFRAME)*2 + m_width;
      hgt = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CXDLGFRAME)*2 + m_height;
      SetWindowPos(m_hWnd, HWND_NOTOPMOST, x, y, wdt, hgt, SWP_SHOWWINDOW);
    }

    SetViewport(0, 0, m_width, m_height);
  }
#elif defined (DIRECT3D10)
  /*{
    int x, y, wdt, hgt;
    if (m_bFullScreen)
    {
      x = 0;
      y = 0;
      wdt = m_width;
      hgt = m_height;
      SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, wdt, hgt, SWP_SHOWWINDOW);
    }
    else
    {
      x = (GetSystemMetrics(SM_CXFULLSCREEN)-m_width)/2;
      y = (GetSystemMetrics(SM_CYFULLSCREEN)-m_height)/2;
      wdt = GetSystemMetrics(SM_CXDLGFRAME)*2 + m_width;
      hgt = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CXDLGFRAME)*2 + m_height;
      SetWindowPos(m_hWnd, HWND_NOTOPMOST, x, y, wdt, hgt, SWP_SHOWWINDOW);
    }

    SetViewport(0, 0, m_width, m_height);
  }*/
#endif
#endif

  return S_OK;
}

static inline bool SortDispFormat(const SDispFormat& a, const SDispFormat& b)
{
	if (a.m_Width != b.m_Width)
		return a.m_Width < b.m_Width;
	if (a.m_Height != b.m_Height)
		return a.m_Height < b.m_Height;
	return a.m_BPP < b.m_BPP;
};

int CD3D9Renderer::EnumDisplayFormats(SDispFormat* formats)
{
	int numFormats = 0;

#if !defined(XENON) && !defined(PS3)

	std::vector<SDispFormat> tmpModeList;

#if defined (DIRECT3D9) || defined (OPENGL)
  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
  CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration();
  CD3D9EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo(pDev->d3d9.AdapterOrdinal);

	tmpModeList.reserve(pAdapterInfo->displayModeList.GetSize());
	for (int i=0; i<pAdapterInfo->displayModeList.GetSize(); ++i)
	{
		D3DDISPLAYMODE* pDM = &pAdapterInfo->displayModeList.GetAt(i);
		if (pDM->Width >= 640 && pDM->Height >= 480)
		{
			unsigned int bpp = DXUTGetD3D9ColorChannelBits(pDM->Format);
			if (bpp <= 32)
			{
				SDispFormat df;
				df.m_Width = pDM->Width;
				df.m_Height = pDM->Height;
				df.m_BPP = (bpp == 24) ? 32 : bpp;
				tmpModeList.push_back(df);
			}
		}
	}
#elif defined(DIRECT3D10)
	DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
	CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
	CD3D10EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo(pDev->d3d10.AdapterOrdinal);

	if (pAdapterInfo->outputInfoList.GetSize() > 0)
	{
		CD3D10EnumOutputInfo* pModes(pAdapterInfo->outputInfoList.GetAt(0));		
		tmpModeList.reserve(pModes->displayModeList.GetSize());
		for (int i=0; i<pModes->displayModeList.GetSize(); ++i)
		{
			DXGI_MODE_DESC* pDM = &pModes->displayModeList.GetAt(i);
			if (pDM->Width >= 640 && pDM->Height >= 480)
			{
				unsigned int bpp = DXUTGetDXGIColorBits(pDM->Format);
				if (bpp <= 32)
				{
					SDispFormat df;
					df.m_Width = pDM->Width;
					df.m_Height = pDM->Height;
					df.m_BPP = (bpp == 24) ? 32 : bpp;
					tmpModeList.push_back(df);
				}
			}
		}
	}
#endif

	if (!tmpModeList.empty())
	{
		std::sort(tmpModeList.begin(), tmpModeList.end(), SortDispFormat);

		int prevWidth = 0, prevHeight = 0, prevBpp = 0;
		for (int i=0; i<tmpModeList.size(); ++i)
		{
			const SDispFormat& df = tmpModeList[i];
			if (df.m_Width != prevWidth || df.m_Height != prevHeight || df.m_BPP != prevBpp)
			{
				if (formats)
					formats[numFormats] = df;

				prevWidth = df.m_Width;
				prevHeight = df.m_Height;
				prevBpp = df.m_BPP;

				++numFormats;
			}
		}
	}

#endif

	return numFormats;
}

bool CD3D9Renderer::ChangeDisplay(unsigned int width,unsigned int height,unsigned int cbpp)
{
  return false;
}


void CD3D9Renderer::UnSetRes()
{
  m_Features |= RFT_DIRECTACCESSTOVIDEOMEMORY | RFT_SUPPORTZBIAS | RFT_DETAILTEXTURE;

#if !defined(XENON) && !defined(PS3)
  DXUTShutdown();
#endif
}

void CD3D9Renderer::DestroyWindow(void)  
{
#ifdef WIN32
  if (m_hWnd)
  {
    ::DestroyWindow(m_hWnd);
    m_hWnd = NULL;
  }
#endif
}

void CD3D9Renderer::RestoreGamma(void)
{
	//if (!m_bFullScreen)
	//	return;

	if (!(GetFeatures() & RFT_HWGAMMA))
    return;

  if (CV_r_nohwgamma)
    return;

  m_fLastGamma = 1.0f;
  m_fLastBrightness = 0.5f;
  m_fLastContrast = 0.5f;

  //iLog->Log("...RestoreGamma");

  struct
  {
    ushort red[256];
    ushort green[256];
    ushort blue[256];
  } Ramp;

  for(int i=0; i<256; i++)
  {
    Ramp.red[i] = Ramp.green[i] = Ramp.blue[i] = i << 8;
  }

#ifdef WIN32
  m_hWndDesktop = GetDesktopWindow();

  HDC dc = GetDC(m_hWndDesktop);
  SetDeviceGammaRamp(dc, &Ramp);
  ReleaseDC(m_hWndDesktop, dc);
#endif //WIN32
}

void CD3D9Renderer::SetDeviceGamma(ushort *r, ushort *g, ushort *b)  
{
  if (!(GetFeatures() & RFT_HWGAMMA))
    return;

  if (CV_r_nohwgamma)
    return;

#ifdef WIN32
  m_hWndDesktop = GetDesktopWindow();

  HDC dc = GetDC(m_hWndDesktop);

  if (!dc)
    return;

  ushort gamma[3][256];
  int i;
  for (i=0; i<256; i++)
  {
    gamma[0][i] = r[i];
    gamma[1][i] = g[i];
    gamma[2][i] = b[i];
  }
  //iLog->Log("...SetDeviceGamma");
  SetDeviceGammaRamp(dc, gamma);
  ReleaseDC(m_hWndDesktop, dc);
#endif //WIN32
}

void CD3D9Renderer::SetGamma(float fGamma, float fBrightness, float fContrast, bool bForce)
{
  int i;

  fGamma = CLAMP(fGamma, 0.1f, 3.0f);
  if (!bForce && m_fLastGamma == fGamma && m_fLastBrightness == fBrightness && m_fLastContrast == fContrast)
    return;

  //int n;
  for ( i=0; i<256; i++ )
  {
    m_GammaTable[i] = CLAMP((int)((fContrast+0.5f)*cry_powf((float)i/255.f,1.0f/fGamma)*65535.f + (fBrightness-0.5f)*32768.f - fContrast*32768.f + 16384.f), 0, 65535);
  }

  m_fLastGamma = fGamma;
  m_fLastBrightness = fBrightness;
  m_fLastContrast = fContrast;

  //iLog->Log("...SetGamma %.3f", fGamma);

  SetDeviceGamma(m_GammaTable, m_GammaTable, m_GammaTable);
}

bool CD3D9Renderer::SetGammaDelta(const float fGamma)
{
  m_fDeltaGamma = fGamma;
  SetGamma(CV_r_gamma + fGamma, CV_r_brightness, CV_r_contrast, false);
  return true;
}

SD3DSurface::~SD3DSurface()
{
#if !defined (DIRECT3D10)
  LPDIRECT3DSURFACE9 pSrf = (LPDIRECT3DSURFACE9)(pSurf);
  SAFE_RELEASE(pSrf);
  pSurf = NULL;
#endif
}

void CD3D9Renderer::ShutDownFast()
{
  CHWShader::mfFlushPendedShaders();
  EF_PipelineShutdown();
  CBaseResource::ShutDown();
  memset(&CTexture::m_TexStages[0], 0, sizeof(CTexture::m_TexStages));
  for (int i=0; i<CTexture::m_TexStates.size(); i++)
  {
    memset(&CTexture::m_TexStates[i], 0, sizeof(STexState));
  }
}

void CD3D9Renderer::ShutDown(bool bReInit)
{
  CHWShader::mfFlushPendedShaders();
  if (bReInit)
    FreeResources(FRR_SHADERS | FRR_TEXTURES | FRR_REINITHW);
  else
  {
    memset(&CTexture::m_TexStages[0], 0, sizeof(CTexture::m_TexStages));
    CTexture::m_TexStates.clear();
    FreeResources(FRR_ALL);
  }
  EF_PipelineShutdown();

#if !defined(XENON) && !defined(PS3)
  DXUTShutdown();
#endif

#if !defined(PS3) && !defined(LINUX)
  if (m_hLibHandle3DC)
  {
    ::FreeLibrary((HINSTANCE)m_hLibHandle3DC);
    m_hLibHandle3DC = NULL;
  }
#endif

  SAFE_DELETE(m_pD3DRenderAuxGeom);
  m_DepthBufferOrig.pSurf = NULL;
  m_DepthBufferOrigFSAA.pSurf = NULL;

#if defined (DIRECT3D9) || defined (OPENGL)
	assert(!m_pCaptureFrameSurf[0]);
	for (int i=0; i<sizeof(m_pCaptureFrameSurf) / sizeof(m_pCaptureFrameSurf[0]); ++i)
		SAFE_RELEASE(m_pCaptureFrameSurf[i]);
	for (int i=0; i<sizeof(m_pCaptureScreenShadowMap) / sizeof(m_pCaptureScreenShadowMap[0]); ++i)
		SAFE_RELEASE(m_pCaptureFrameSurf[i]);
#endif

  CBaseResource::ShutDown();

  //////////////////////////////////////////////////////////////////////////
  // Clear globals.
  //////////////////////////////////////////////////////////////////////////
  if (!bReInit)
  {
    iLog = NULL;
    //iConsole = NULL;
    iTimer = NULL;
    iSystem = NULL;
  }

  STLALLOCATOR_CLEANUP

	delete m_matView;
	delete m_matProj;
}

bool CD3D9Renderer::SetWindow(int width, int height, bool fullscreen, WIN_HWND hWnd)
{
#ifdef WIN32
  HWND temp = GetDesktopWindow();
  RECT trect;

  GetWindowRect(temp, &trect);

  m_deskwidth =trect.right-trect.left;
  m_deskheight=trect.bottom-trect.top;  

  DWORD style, exstyle;
  int x, y, wdt, hgt;
  
  if (width < 640)
    width = 640;
  if (height < 480)
    height = 480;

  m_dwWindowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

	// Do not allow the user to resize the window
	m_dwWindowStyle &= ~WS_MAXIMIZEBOX;
	m_dwWindowStyle &= ~WS_THICKFRAME;

  if (fullscreen)
  {
    exstyle = WS_EX_TOPMOST;
    style = WS_POPUP | WS_VISIBLE;
    x = (GetSystemMetrics(SM_CXFULLSCREEN)-width)/2;
    y = (GetSystemMetrics(SM_CYFULLSCREEN)-height)/2;
    wdt = width;
    hgt = height;
  }
  else
  {
    exstyle = WS_EX_APPWINDOW;
    style = m_dwWindowStyle;
    x = (GetSystemMetrics(SM_CXFULLSCREEN)-width)/2;
    y = (GetSystemMetrics(SM_CYFULLSCREEN)-height)/2;
    wdt = GetSystemMetrics(SM_CXFIXEDFRAME)*2 + width;
    hgt = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFIXEDFRAME)*2 + height;
  }

  if (m_bEditor)
  {
    m_dwWindowStyle = WS_OVERLAPPED;
    style = m_dwWindowStyle;
    exstyle = 0;
    x = y = 0;
    wdt = 100;
    hgt = 100;

    WNDCLASS wc;
    memset(&wc, 0, sizeof(WNDCLASS));
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = DefWindowProc;
    wc.hInstance     = m_hInst;
//    wc.hbrBackground =(HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName  = 0;
    wc.lpszClassName = "D3DDeviceWindowClassForSandbox";
    if (!RegisterClass(&wc))
    {
      CryError( "Cannot Register Window Class %s",wc.lpszClassName );
      return false;
    }
    m_hWnd = CreateWindowEx(exstyle, wc.lpszClassName, m_WinTitle, style, x, y, wdt, hgt, NULL,NULL, m_hInst, NULL);
    ShowWindow( m_hWnd, SW_HIDE );
  }
  else
  {
    if (!hWnd)
    {
      m_hWnd = CreateWindowEx(exstyle,"CryENGINE",m_WinTitle,style,x,y,wdt,hgt, NULL, NULL, m_hInst, NULL);
    }
    else
      m_hWnd = (HWND)hWnd;
    ShowWindow(m_hWnd, SW_SHOWNORMAL);
    SetFocus(m_hWnd);
    SetForegroundWindow(m_hWnd);
    //SetActiveWindow(m_hWnd);
    /*HWND hw = GetFocus();
    HWND hw1 = GetFocus();
    DWORD dwErr = GetLastError();
    LPVOID lpMsgBuf;
    FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | 
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      dwErr,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR) &lpMsgBuf,
      0, NULL );
    LocalFree(lpMsgBuf);*/
  }

  if (!m_hWnd)
    iConsole->Exit("Couldn't create window\n");

#endif //WIN32
  return true;
}

#if defined (DIRECT3D9) && !defined (XENON)
void CD3D9Renderer::SuccessfullyLaunchFromMediaCenter() const
{
	if (m_hWnd && IsWindow(m_hWnd))
	{
		typedef IDirect3D9* (WINAPI *FP_Direct3DCreate9)(UINT);
		FP_Direct3DCreate9 pd3dc9((FP_Direct3DCreate9) GetProcAddress(LoadLibrary("d3d9.dll"), "Direct3DCreate9"));		
		IDirect3D9* pD3D(pd3dc9 ? pd3dc9(D3D_SDK_VERSION) : 0);
		if (pD3D)
		{
			D3DPRESENT_PARAMETERS presentParams;
			memset(&presentParams, 0, sizeof(presentParams));
			presentParams.BackBufferWidth = 640;
			presentParams.BackBufferHeight = 480;
			presentParams.BackBufferFormat = D3DFMT_A8R8G8B8;
			presentParams.BackBufferCount = 1;
			presentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
			presentParams.MultiSampleQuality = 0;
			presentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
			presentParams.hDeviceWindow = m_hWnd;
			presentParams.Windowed = TRUE;
			presentParams.EnableAutoDepthStencil = TRUE;
			presentParams.AutoDepthStencilFormat = D3DFMT_D24S8;
			presentParams.Flags = 0;
			presentParams.FullScreen_RefreshRateInHz = 0;
			presentParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

			CTimeValue startTime(gEnv->pTimer->GetAsyncTime());
			while (true)
			{
				MSG msg;
				while (PeekMessage(&msg, m_hWnd, 0, 0, PM_REMOVE))       
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				Sleep(50);

				IDirect3DDevice9* pDev(0);
				if (SUCCEEDED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, 
					D3DCREATE_PUREDEVICE | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &presentParams, &pDev)))
				{
					SAFE_RELEASE(pDev);
					break;
				}

				CTimeValue curTime(gEnv->pTimer->GetAsyncTime());
				if ((curTime - startTime).GetSeconds() > 5.0f)
				{
					Warning("It's taken more than five seconds to create a Direct3D9 device after launching"
						" from Media Center. Successful render initialization cannot be guaranteed!");
					break;
				}
			}

			SAFE_RELEASE(pD3D);
		}
	}
}
#endif

void (*DeleteDataATI)(void *pData);
COMPRESSOR_ERROR (*CompressTextureATI)(DWORD width,
                                                 DWORD height,
                                                 UNCOMPRESSED_FORMAT sourceFormat,
                                                 COMPRESSED_FORMAT destinationFormat,
                                                 void    *inputData,
                                                 void    **dataOut,
                                                 DWORD   *outDataSize);

COMPRESSOR_ERROR (*DeCompressTextureATI)(DWORD width,
                                                 DWORD height,
                                                 COMPRESSED_FORMAT sourceFormat,
                                                 UNCOMPRESSED_FORMAT destinationFormat,
                                                 void    *inputData,
                                                 void    **dataOut,
                                                 DWORD   *outDataSize);


#define QUALITY_VAR(name) \
  static void OnQShaderChange_Shader##name( ICVar *pVar )\
{\
  int iQuality = eSQ_Low;\
  if(gRenDev->GetFeatures() & (/*RFT_HW_PS2X | */RFT_HW_PS30))\
  iQuality = CLAMP(pVar->GetIVal(), 0, eSQ_Max);\
  gRenDev->SetShaderQuality(eST_##name,(EShaderQuality)iQuality);\
}

QUALITY_VAR(General)
QUALITY_VAR(Metal)
QUALITY_VAR(Glass)
QUALITY_VAR(Vegetation)
QUALITY_VAR(Ice)
QUALITY_VAR(Terrain)
QUALITY_VAR(Shadow)
QUALITY_VAR(Water)
QUALITY_VAR(FX)
QUALITY_VAR(PostProcess)
QUALITY_VAR(HDR)
QUALITY_VAR(Sky)

#undef QUALITY_VAR

static void OnQShaderChange_Renderer( ICVar *pVar )
{
  int iQuality = eRQ_Low;

  if(gRenDev->GetFeatures() & (/*RFT_HW_PS2X | */RFT_HW_PS30))
    iQuality = CLAMP(pVar->GetIVal(), 0, eSQ_Max);
  else
    pVar->ForceSet("0");

  gRenDev->m_RP.m_eQuality = (ERenderQuality)iQuality;
}


static void Command_Quality(IConsoleCmdArgs* Cmd)
{
  bool bLog=false;
  bool bSet=false;

  int iQuality = -1;

  if(Cmd->GetArgCount()==2)
  {
    iQuality = CLAMP(atoi(Cmd->GetArg(1)), 0, eSQ_Max);
    bSet=true;
  }
  else bLog=true;

  if(bLog)iLog->LogWithType(IMiniLog::eInputResponse," ");
  if(bLog)iLog->LogWithType(IMiniLog::eInputResponse,"Current quality settings (0=low/1=med/2=high/3=very high):");

#define QUALITY_VAR(name) if(bLog)iLog->LogWithType(IMiniLog::eInputResponse,"  $3q_"#name" = $6%d",gEnv->pConsole->GetCVar("q_"#name)->GetIVal()); \
  if(bSet)gEnv->pConsole->GetCVar("q_"#name)->Set(iQuality);

  QUALITY_VAR(ShaderGeneral)
    QUALITY_VAR(ShaderMetal)
    QUALITY_VAR(ShaderGlass)
    QUALITY_VAR(ShaderVegetation)
    QUALITY_VAR(ShaderIce)
    QUALITY_VAR(ShaderTerrain)
    QUALITY_VAR(ShaderShadow)
    QUALITY_VAR(ShaderWater)
    QUALITY_VAR(ShaderFX)
    QUALITY_VAR(ShaderPostProcess)
    QUALITY_VAR(ShaderHDR)
    QUALITY_VAR(ShaderSky)
    QUALITY_VAR(Renderer)

#undef QUALITY_VAR

    if(bSet)iLog->LogWithType(IMiniLog::eInputResponse,"Set quality to %d",iQuality);
}

const char *sGetSQuality(const char *szName)
{
  ICVar *pVar = iConsole->GetCVar(szName);
  assert(pVar);
  if (!pVar)
    return "Unknown";
  int iQ = pVar->GetIVal();
  switch (iQ)
  {
    case eSQ_Low:
      return "Low";
    case eSQ_Medium:
      return "Medium";
    case eSQ_High:
      return "High";
    case eSQ_VeryHigh:
      return "VeryHigh";
    default:
      return "Unknown";
  }
}

WIN_HWND CD3D9Renderer::Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen,WIN_HINSTANCE hinst, WIN_HWND Glhwnd, bool bReInit, const SCustomRenderInitArgs* pCustomArgs)
{
  if (!iSystem || !iLog)
    return 0;

  bool b = false;

  m_CVWidth = iConsole->GetCVar("r_Width");
  m_CVHeight = iConsole->GetCVar("r_Height");
  m_CVFullScreen = iConsole->GetCVar("r_Fullscreen");
  m_CVDisplayInfo = iConsole->GetCVar("r_DisplayInfo");
  m_CVColorBits = iConsole->GetCVar("r_ColorBits");

#if defined (DIRECT3D9) || defined (OPENGL)
  iLog->Log ("Direct3D9 driver is creating...");
  iLog->Log ("Crytek Direct3D9 driver version %4.2f (%s <%s>).", VERSION_D3D, __DATE__, __TIME__);
  sprintf(m_WinTitle,"- CryENGINE - %s (%s)",__DATE__, __TIME__);
#elif defined (DIRECT3D10)
  iLog->Log ("Direct3D10 driver is creating...");
  iLog->Log ("Crytek Direct3D10 driver version %4.2f (%s <%s>).", VERSION_D3D, __DATE__, __TIME__);
  sprintf(m_WinTitle,"- CryENGINE DX10 - %s (%s)",__DATE__, __TIME__);
#endif

  m_hInst = (HINSTANCE)hinst;

  if (Glhwnd)
  {
    if (Glhwnd == (WIN_HWND)1)
      Glhwnd = 0;
    m_bEditor = true;
    fullscreen = false;
  }




#if !defined(PS3) && !defined(LINUX)
  m_hLibHandle3DC = ::LoadLibrary("CompressATI2.dll");
  if (m_hLibHandle3DC)
  {
    CompressTextureATI = (FnCompressTextureATI)GetProcAddress((HINSTANCE)m_hLibHandle3DC, "CompressTextureATI");
    DeCompressTextureATI = (FnDeCompressTextureATI)GetProcAddress((HINSTANCE)m_hLibHandle3DC, "DeCompressTextureATI");
    DeleteDataATI = (FnDeleteDataATI)GetProcAddress((HINSTANCE)m_hLibHandle3DC, "DeleteDataATI");
  }
#endif

  // Save the new dimensions
  CRenderer::m_width  = width;
  CRenderer::m_height = height;
  CRenderer::m_cbpp   = cbpp;
  CRenderer::m_zbpp   = zbpp; 
  CRenderer::m_sbpp   = sbits;
  m_bFullScreen       = fullscreen;
  while (true)
  {
    if (!SetWindow(width, height, fullscreen, Glhwnd))
    {
      ShutDown();
      return 0;
    }

#if defined (DIRECT3D9) && !defined (XENON)
		if (pCustomArgs && pCustomArgs->appStartedFromMediaCenter)
			SuccessfullyLaunchFromMediaCenter();
#endif

		// Creates Device here.
    if (!SetRes())
		{
			ShutDown(true);
			return 0;
		}
		break;
		/*
     // break;
    ShutDown(true);
    if (b)
      return 0;
    m_bFullScreen ? m_bFullScreen = 0 : m_bFullScreen = 1;
    b = 1;
		*/
  }

#if defined (DIRECT3D9) || defined (OPENGL)
#if !defined(XENON) && !defined(PS3)
  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
  CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration();
  CD3D9EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo(pDev->d3d9.AdapterOrdinal);
	D3DADAPTER_IDENTIFIER9 *ai = NULL;
	D3DADAPTER_IDENTIFIER9 adapter_info;
	if (pAdapterInfo)
		ai = &pAdapterInfo->AdapterIdentifier;
	else
	{
		ai = &adapter_info;
		memset(ai,0,sizeof(adapter_info));
		strcpy( ai->Description,"NULL REF" );
		strcpy( ai->DeviceName,"NULL REF" );
		strcpy( ai->Driver,"NULL REF" );
	}
  iLog->Log(" ****** D3D9 CryRender Stats ******");
  iLog->Log(" Driver description: %s", ai->Description);
  iLog->Log(" Full stats: %s", m_strDeviceStats);
  iLog->Log(" Hardware acceleration: %s", (pDev->d3d9.DeviceType == D3DDEVTYPE_HAL) ? "Yes" : "No");
#endif

  if (CV_r_fsaa)
  {
    TArray<SAAFormat> Formats;

    int nNum = GetAAFormat(Formats, false);
    iLog->Log(" Full scene AA: Enabled: %s (%d Quality)", Formats[nNum].szDescr, Formats[nNum].nQuality);
    GetAAFormat(Formats, true);
  }
  else
    iLog->Log(" Full scene AA: Disabled");
  iLog->Log(" Current Resolution: %dx%dx%d %s", CRenderer::m_width, CRenderer::m_height, CRenderer::m_cbpp, m_bFullScreen ? "Full Screen" : "Windowed");
#if !defined(XENON) && !defined(PS3)
  UINT nMinWidth, nMinHeight, nMaxWidth, nMaxHeight;
  pd3dEnum->GetResolutionMinMax(nMinWidth, nMinHeight, nMaxWidth, nMaxHeight);  
  iLog->Log(" Maximum Resolution: %dx%d", nMaxWidth, nMaxHeight);
#endif
  iLog->Log(" Maximum texture size: %dx%d (Max Aspect: %d)", m_pd3dCaps->MaxTextureWidth, m_pd3dCaps->MaxTextureHeight, m_pd3dCaps->MaxTextureAspectRatio);
  iLog->Log(" Default textures filtering type: %s", CV_d3d9_texturefilter->GetString());
  iLog->Log(" HDR Rendering: %s", m_nHDRType == 1 ? "FP16" : m_nHDRType == 2 ? "MRT" : "Disabled");
  iLog->Log(" MRT HDR Rendering: %s", (m_bDeviceSupportsFP16Separate) ? "Enabled" : "Disabled");
  iLog->Log(" Occlusion queries: %s", (m_Features & RFT_OCCLUSIONTEST) ? "Supported" : "Not supported");
  iLog->Log(" Geometry instancing: %s", (m_bDeviceSupportsInstancing) ? "Supported" : "Not supported");
  iLog->Log(" Dynamic branching in shaders: %s", (m_bDeviceSupportsDynBranching) ? "Supported" : "Not supported");
  iLog->Log(" Vertex textures: %s", (m_bDeviceSupportsVertexTexture) ? "Supported" : "Not supported");  
	iLog->Log(" R16F rendertarget: %s", (m_bDeviceSupportsR16FRendertarget) ? "Supported" : "Not supported");
	iLog->Log(" NormalMaps compression (%d): %s",m_iDeviceSupportsComprNormalmaps, m_iDeviceSupportsComprNormalmaps==1 ? "3Dc" : m_iDeviceSupportsComprNormalmaps==2 ? "V8U8" : m_iDeviceSupportsComprNormalmaps==3 ? "CxV8U8" : "Not supported");
  iLog->Log(" Gamma control: %s", (m_Features & RFT_HWGAMMA) ? "Hardware" : "Software");
  iLog->Log(" Vertex Shaders version %d.%d", D3DSHADER_VERSION_MAJOR(m_pd3dCaps->VertexShaderVersion), D3DSHADER_VERSION_MINOR(m_pd3dCaps->VertexShaderVersion));
  iLog->Log(" Pixel Shaders version %d.%d", D3DSHADER_VERSION_MAJOR(m_pd3dCaps->PixelShaderVersion), D3DSHADER_VERSION_MINOR(m_pd3dCaps->PixelShaderVersion));
  CTexture::m_nMaxtextureSize = m_pd3dCaps->MaxTextureWidth;
#elif defined (DIRECT3D10)
  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
  CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
  CD3D10EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo(pDev->d3d10.AdapterOrdinal);

  iLog->Log(" ****** D3D10 CryRender Stats ******");
  iLog->Log(" Driver description: %S", pAdapterInfo->szUniqueDescription);
  iLog->Log(" Full stats: %s", m_strDeviceStats);
  if (pDev->d3d10.DriverType == D3D10_DRIVER_TYPE_HARDWARE)
    iLog->Log(" Rasterizer: Hardware");
  else
  if (pDev->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE)
    iLog->Log(" Rasterizer: Reference");
  else
  if (pDev->d3d10.DriverType == D3D10_DRIVER_TYPE_SOFTWARE)
    iLog->Log(" Rasterizer: Software");

  //iLog->Log(" Full screen only: %s", (di->d3dCaps.Caps2 & D3DCAPS2_CANRENDERWINDOWED) ? "No" : "Yes");
  if (CV_r_fsaa)
  {
    TArray<SAAFormat> Formats;

    int nNum = GetAAFormat(Formats, false);
    iLog->Log(" Full scene AA: Enabled: %s (%d Quality)", Formats[nNum].szDescr, Formats[nNum].nQuality);
    GetAAFormat(Formats, true);
  }
  else
    iLog->Log(" Full scene AA: Disabled");
  iLog->Log(" Current Resolution: %dx%dx%d %s", CRenderer::m_width, CRenderer::m_height, CRenderer::m_cbpp, m_bFullScreen ? "Full Screen" : "Windowed");
  UINT nMinWidth, nMinHeight, nMaxWidth, nMaxHeight;
  pd3dEnum->GetResolutionMinMax(nMinWidth, nMinHeight, nMaxWidth, nMaxHeight);  
  iLog->Log(" Texture filtering type: %s", CV_d3d9_texturefilter->GetString());
  iLog->Log(" HDR Rendering: %s", m_nHDRType == 1 ? "FP16" : m_nHDRType == 2 ? "MRT" : "Disabled");
  iLog->Log(" MRT HDR Rendering: %s", (m_bDeviceSupportsFP16Separate) ? "Enabled" : "Disabled");
  iLog->Log(" Occlusion queries: %s", (m_Features & RFT_OCCLUSIONTEST) ? "Supported" : "Not supported");
  iLog->Log(" Geometry instancing: %s", (m_bDeviceSupportsInstancing) ? "Supported" : "Not supported");
  iLog->Log(" Dynamic branching in shaders: %s", (m_bDeviceSupportsDynBranching) ? "Supported" : "Not supported");
  iLog->Log(" Vertex textures: %s", (m_bDeviceSupportsVertexTexture) ? "Supported" : "Not supported");
  iLog->Log(" R16F rendertarget: %s", (m_bDeviceSupportsR16FRendertarget) ? "Supported" : "Not supported");
	iLog->Log(" NormalMaps compression (%d): %s",m_iDeviceSupportsComprNormalmaps, m_iDeviceSupportsComprNormalmaps==1 ? "3Dc" : m_iDeviceSupportsComprNormalmaps==2 ? "V8U8" : m_iDeviceSupportsComprNormalmaps==3 ? "CxV8U8" : "Not supported");
  iLog->Log(" Gamma control: %s", (m_Features & RFT_HWGAMMA) ? "Hardware" : "Software");
  iLog->Log(" Vertex Shaders version %d.%d", 4, 0);
  iLog->Log(" Pixel Shaders version %d.%d", 4, 0);
#endif

	CRenderer::ChangeGeomInstancingThreshold();		// to get log printout and to set the internal value (vendor dependent)

#if defined (DIRECT3D9) || defined (OPENGL)
  m_RP.m_nMaxLightsPerPass = 1;
  if (D3DSHADER_VERSION_MAJOR(m_pd3dCaps->PixelShaderVersion) >= 3)
  {
    m_Features |= RFT_HW_PS20 | RFT_HW_PS2X | RFT_HW_PS30;
    m_RP.m_nMaxLightsPerPass = 4;
  }
  else
  if (D3DSHADER_VERSION_MAJOR(m_pd3dCaps->PixelShaderVersion) >= 2)
  {
    m_Features |= RFT_HW_PS20;
    if (D3DSHADER_VERSION_MINOR(m_pd3dCaps->PixelShaderVersion) > 0 || m_pd3dCaps->PS20Caps.NumInstructionSlots >= 256)
    {
      m_Features |= RFT_HW_PS2X;
      m_RP.m_nMaxLightsPerPass = 4;
    }
  }
#elif defined (DIRECT3D10)
  m_Features |= RFT_HW_PS20 | RFT_HW_PS2X | RFT_HW_PS30;
  m_RP.m_nMaxLightsPerPass = 4;
#endif

#ifndef USE_HDR
  m_Features &= ~RFT_HW_HDR;
#endif

#if !defined(XENON) && !defined(PS3)
  if (!m_bDeviceSupportsInstancing)
    _SetVar("r_GeomInstancing", 0);
#endif

  char *str = "Unsupported Pixel Shaders Version";
#if defined (DIRECT3D9) || defined (OPENGL)
  if (m_Features & RFT_HW_PS30)
    str = "PS.3.0, PS.2.X, PS.2.0 and PS.1.1";
  else
  if (m_Features & RFT_HW_PS2X)
    str = "PS.2.x, PS.2.0 and PS.1.1";
	else if (m_Features & RFT_HW_PS20)
		str = "PS.2.0 and PS.1.1";
#elif defined (DIRECT3D10)
  str = "PS.4.0";
#endif
  iLog->Log(" Pixel shaders usage: %s", str);

#if defined (DIRECT3D9) || defined (OPENGL)
  if (D3DSHADER_VERSION_MAJOR(m_pd3dCaps->VertexShaderVersion) >= 3)
    str = "VS.3.0, VS.2.0 and VS.1.1";
  else
    str = "VS.2.0 and VS.1.1";
#elif defined (DIRECT3D10)
  str = "VS.4.0";
#endif
  iLog->Log(" Vertex shaders usage: %s", str);

  iLog->Log(" Shadow maps type: %s", "Mixed Depth/2D maps");
#if defined (DIRECT3D9) || defined (OPENGL)
  iLog->Log(" Stencil shadows type: %s", m_pd3dCaps->StencilCaps & D3DSTENCILCAPS_TWOSIDED ? "Two sided" : "Single sided");
#elif defined (DIRECT3D10)
  iLog->Log(" Stencil shadows type: %s", "Two sided");
#endif

	iLog->Log(" 3DC emulation: %s",DoCompressedNormalmapEmulation() ? "Enabled" : "Disabled");

  iLog->Log(" *****************************************");
	iLog->Log(" ");

  iLog->Log("Init Shaders");

  m_cEF.ParseShaderProfiles();
  m_cEF.ParseFSAAProfiles();

//  if (!(GetFeatures() & (RFT_HW_PS2X | RFT_HW_PS30)))
//    SetShaderQuality(eST_All, eSQ_Low);

  // Quality console variables --------------------------------------

#define QUALITY_VAR(name) { ICVar *pVar=iConsole->Register("q_Shader"#name,&m_cEF.m_ShaderProfiles[(int)eST_##name].m_iShaderProfileQuality,1,	\
  0, "Defines the shader quality of "#name"\nUsage: q_Shader"#name" 0=low/1=med/2=high/3=very high (default)",OnQShaderChange_Shader##name);\
  OnQShaderChange_Shader##name(pVar);\
  iLog->Log(" %s shader quality: %s", #name, sGetSQuality("q_Shader"#name));}   // clamp for lowspec

  QUALITY_VAR(General);
  QUALITY_VAR(Metal);
  QUALITY_VAR(Glass);
  QUALITY_VAR(Vegetation);
  QUALITY_VAR(Ice);
  QUALITY_VAR(Terrain);
  QUALITY_VAR(Shadow);
  QUALITY_VAR(Water);
  QUALITY_VAR(FX);
  QUALITY_VAR(PostProcess);
  QUALITY_VAR(HDR);
  QUALITY_VAR(Sky);


#undef QUALITY_VAR

    ICVar *pVar = iConsole->RegisterInt("q_Renderer",3,0,"Defines the quality of Renderer\nUsage: q_Renderer 0=low/1=med/2=high/3=very high (default)",OnQShaderChange_Renderer);
  OnQShaderChange_Renderer(pVar);   // clamp for lowspec, report renderer current value
  iLog->Log("Render quality: %s", sGetSQuality("q_Renderer"));

  iConsole->AddCommand("q_Quality",&Command_Quality,0,
    "If called with a parameter it sets the quality of all q_.. variables\n"
    "otherwise it prints their current state\n"
    "Usage: q_Quality [0=low/1=med/2=high/3=very high]");

  EF_InitVFMergeMap();
  m_cEF.mfInit();
  EF_Init();

  m_bInitialized = true;

//  Cry_memcheck();

  // Success, return the window handle
  return (m_hWnd);
}

const char *CD3D9Renderer::D3DError( HRESULT h )
{
#if !defined(OPENGL) && !defined(XENON) && !defined(PS3)
  const char* strHRESULT = "Unknown";
 #if defined(DIRECT3D10)
  strHRESULT = DXGetErrorString(h);
 #else
  strHRESULT = DXGetErrorString9(h);
 #endif
  return strHRESULT;
#else
  return "Unknown";
#endif
}

bool CD3D9Renderer::Error(char *Msg, HRESULT h)
{
  const char *str = D3DError(h);
  iLog->Log("Error: %s (%s)", Msg, str);

  //UnSetRes();

  //if (Msg)
  //  iConsole->Exit("%s (%s)", Msg, str);
  //else
  //  iConsole->Exit("(%s)", str);

  return false;
}

//=============================================================================

static char *sD3DFMT( D3DFORMAT fmt )
{
  switch( fmt )
  {
  case D3DFMT_A8R8G8B8:
    return "D3DFMT_A8R8G8B8";
  case D3DFMT_X8R8G8B8:
    return "D3DFMT_X8R8G8B8";
  case D3DFMT_R5G6B5:
    return "D3DFMT_R5G6B5";
  case D3DFMT_X1R5G5B5:
    return "D3DFMT_X1R5G5B5";
  case D3DFMT_A1R5G5B5:
    return "D3DFMT_A1R5G5B5";
  case D3DFMT_A4R4G4B4:
    return "D3DFMT_A4R4G4B4";
  case D3DFMT_X4R4G4B4:
    return "D3DFMT_X4R4G4B4";
  case D3DFMT_A2B10G10R10:
    return "D3DFMT_A2B10G10R10";
  case D3DFMT_A2R10G10B10:
    return "D3DFMT_A2R10G10B10";
  case D3DFMT_D24S8:
    return "D3DFMT_D24S8";
  case D3DFMT_D24X8:
    return "D3DFMT_D24X8";
  case D3DFMT_D16:
    return "D3DFMT_D16";
  case D3DFMT_D32:
    return "D3DFMT_D32";
  case D3DFMT_D24FS8:
    return "D3DFMT_D24FS8";
    // Xenon does not support these formats.
#if !defined(XENON)
  case D3DFMT_R8G8B8:
    return "D3DFMT_R8G8B8";
  case D3DFMT_R3G3B2:
    return "D3DFMT_R3G3B2";
  case D3DFMT_A8R3G3B2:
    return "D3DFMT_A8R3G3B2";
  case D3DFMT_D24X4S4:
    return "D3DFMT_D24X4S4";
  case D3DFMT_D16_LOCKABLE:
    return "D3DFMT_D16_LOCKABLE";
  case D3DFMT_D32F_LOCKABLE:
    return "D3DFMT_D32F_LOCKABLE";
  case D3DFMT_D15S1:
    return "D3DFMT_D15S1";
#endif
  default:
    return "Unknown";
  }
}

struct SMultiSample
{
#if defined (DIRECT3D9) || defined(OPENGL)
  D3DMULTISAMPLE_TYPE Type;
#elif defined (DIRECT3D10)
  UINT Type;
#endif
  DWORD Quality;
};

int __cdecl MS_Cmp(const void* v1, const void* v2)
{
  SMultiSample *pMS0 = (SMultiSample *)v1;
  SMultiSample *pMS1 = (SMultiSample *)v2;

  if (pMS0->Type < pMS1->Type)
    return -1;
  if (pMS0->Type > pMS1->Type)
    return 1;

  if (pMS0->Quality < pMS1->Quality)
    return -1;
  if (pMS0->Quality > pMS1->Quality)
    return 1;

  return 0;
}

int __cdecl MSAA_Cmp(const void* v1, const void* v2)
{
  SAAFormat *pMS0 = (SAAFormat *)v1;
  SAAFormat *pMS1 = (SAAFormat *)v2;
  int nD0 = isdigit(pMS0->szDescr[0]);
  int nD1 = isdigit(pMS1->szDescr[0]);
  if (nD0 < nD1)
    return -1;
  if (nD0 > nD1)
    return 1;
  nD0 = atoi(pMS0->szDescr);
  nD1 = atoi(pMS1->szDescr);
  if (nD0 < nD1)
    return -1;
  if (nD0 > nD1)
    return 1;

  return stricmp(pMS0->szDescr, pMS1->szDescr);
}

//! Return all supported by video card video AA formats
int CD3D9Renderer::EnumAAFormats(const SDispFormat &rDispFmt, SAAFormat *formats)
{
	// FSAA requires HDR mode on so we require HDR capable hardware
	// search for #LABEL_FSAA_HDR
	if(!(gRenDev->GetFeatures()&RFT_HW_HDR))
		return 0;

	// Andrey todo
	//
	// make use of rDispFmt

  TArray<SAAFormat> SupAA;

#if defined (DIRECT3D9) || defined(OPENGL)
 #if !defined(XENON) && !defined(PS3)
  uint i;
  SAAFormat DF;
  CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration();
  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
  CD3D9EnumDeviceSettingsCombo* pCombo = pd3dEnum->GetDeviceSettingsCombo(&pDev->d3d9);
  CGrowableArray<D3DMULTISAMPLE_TYPE>* pMultiSampleList = &pCombo->multiSampleTypeList;
  CD3D9EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo(pDev->d3d9.AdapterOrdinal);

  TArray <SMultiSample> MSList;
  int nNONMaskable = 0;
  int nMaskable = 0;
  int nQuality = 0;
  for (i=0; i<pMultiSampleList->GetSize(); i++)
  {
    if (pMultiSampleList->GetAt(i) == D3DMULTISAMPLE_NONE)
      continue;
    SMultiSample MS;
    MS.Type = pMultiSampleList->GetAt(i);
    if (MS.Type == D3DMULTISAMPLE_NONMASKABLE)
      nNONMaskable++;
    else
      nMaskable++;
    int nQualities = pCombo->multiSampleQualityList[i];
    for (DWORD j=0; j<nQualities; j++)
    {
      MS.Quality = j;
      MSList.AddElem(MS);
      if (MS.Type == D3DMULTISAMPLE_NONMASKABLE)
        nQuality++;
    }
  }
  qsort(&MSList[0], MSList.Num(), sizeof(MSList[0]), MS_Cmp);
  DF.nAPIType = D3DMULTISAMPLE_NONMASKABLE;
  DF.nQuality = 0;
  DF.nSamples = 1;
  char str[64];
  int nVendorID = pAdapterInfo->AdapterIdentifier.VendorId;
  int nDevID = pAdapterInfo->AdapterIdentifier.DeviceId;
  for (i=0; i<MSList.Num(); i++)
  {
    int nMSAA;
    SMultiSample *pS = &MSList[i];
    SMSAAMode *pMode = NULL;
    for (nMSAA=0; nMSAA<m_cEF.m_FSAAModes.size(); nMSAA++)
    {
      SMSAAProfile *pMSAA = &m_cEF.m_FSAAModes[nMSAA];
      SDevID *pDev = pMSAA->pDeviceGroup;
      assert(pDev);
      if (!pDev)
        continue;
      if (pDev->nVendorID != nVendorID)
        continue;
      int nDev;
      for (nDev=0; nDev<pDev->DevMinMax.size(); nDev++)
      {
        SMinMax *pMM = &pDev->DevMinMax[nDev];
        if (pMM->nMin<=nDevID && pMM->nMax>=nDevID)
          break;
      }
      if (nDev == pDev->DevMinMax.size())
        continue;
      for (int nM=0; nM<pMSAA->Modes.size(); nM++)
      {
        SMSAAMode *pM = &pMSAA->Modes[nM];
        if (pS->Type != pM->nSamples)
          continue;
        if (pS->Quality != pM->nQuality)
          continue;
        pMode = pM;
        break;
      }
    }
    if (pMode)
    {
      DF.nAPIType = pS->Type;
      DF.nQuality = pS->Quality;
      DF.nSamples = pS->Type;
      strcpy(DF.szDescr, pMode->szDescr);
      if (formats)
        formats[SupAA.Num()] = DF;
      SupAA.AddElem(DF);
    }
  }
  for (i=0; i<MSList.Num(); i++)
  {
    SMultiSample *pS = &MSList[i];
    if (pS->Quality == 0 && MSList[i].Type>=2)
    {
      sprintf(str, "%dx", pS->Type);
      int j;
      for (j=0; j<SupAA.Num(); j++)
      {
        if (!stricmp(SupAA[j].szDescr, str))
          break;
      }
      if (j == SupAA.Num())
      {
        DF.nAPIType = pS->Type;
        DF.nQuality = pS->Quality;
        DF.nSamples = pS->Type;
        strcpy(DF.szDescr, str);
        if (formats)
          formats[SupAA.Num()] = DF;
        SupAA.AddElem(DF);
      }
    }
  }
 #endif
#elif defined (DIRECT3D10)
  uint i;
  SAAFormat DF;
  CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
  CD3D10EnumDeviceSettingsCombo* pCombo = pd3dEnum->GetDeviceSettingsCombo(&pDev->d3d10);
  CD3D10EnumDeviceInfo *pDI = pCombo->pDeviceInfo;
  CGrowableArray<UINT>* pMultiSampleList = &pDI->multiSampleCountList;

  TArray <SMultiSample> MSList;
  int nNONMaskable = 0;
  int nMaskable = 0;
  int nQuality = 0;
  for (i=0; i<pMultiSampleList->GetSize(); i++)
  {
    if (pMultiSampleList->GetAt(i) == 0)
      continue;
    SMultiSample MS;
    MS.Type = pMultiSampleList->GetAt(i);
    nMaskable++;
    int nQualities = pDI->multiSampleQualityList[i];
    for (DWORD j=0; j<nQualities; j++)
    {
      MS.Quality = j;
      MSList.AddElem(MS);
    }
  }
  qsort(&MSList[0], MSList.Num(), sizeof(MSList[0]), MS_Cmp);

  DF.nAPIType = 0;
  DF.nQuality = 0;
  DF.nSamples = 1;
  char str[64];

  CD3D10EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo(pDev->d3d10.AdapterOrdinal);
  int nVendorID = pAdapterInfo->AdapterDesc.VendorId;
  int nDevID = pAdapterInfo->AdapterDesc.DeviceId;
  for (i=0; i<MSList.Num(); i++)
  {
    int nMSAA;
    SMultiSample *pS = &MSList[i];
    SMSAAMode *pMode = NULL;
    for (nMSAA=0; nMSAA<m_cEF.m_FSAAModes.size(); nMSAA++)
    {
      SMSAAProfile *pMSAA = &m_cEF.m_FSAAModes[nMSAA];
      SDevID *pDev = pMSAA->pDeviceGroup;
      assert(pDev);
      if (!pDev)
        continue;
      if (pDev->nVendorID != nVendorID)
        continue;
      int nDev;
      for (nDev=0; nDev<pDev->DevMinMax.size(); nDev++)
      {
        SMinMax *pMM = &pDev->DevMinMax[nDev];
        if (pMM->nMin<=nDevID && pMM->nMax>=nDevID)
          break;
      }
      if (nDev == pDev->DevMinMax.size())
        continue;
      for (int nM=0; nM<pMSAA->Modes.size(); nM++)
      {
        SMSAAMode *pM = &pMSAA->Modes[nM];
        if (pS->Type != pM->nSamples)
          continue;
        if (pS->Quality != pM->nQuality)
          continue;
        pMode = pM;
        break;
      }
    }
    if (pMode)
    {
      DF.nAPIType = pS->Type;
      DF.nQuality = pS->Quality;
      DF.nSamples = pS->Type;
      strcpy(DF.szDescr, pMode->szDescr);
      if (formats)
        formats[SupAA.Num()] = DF;
      SupAA.AddElem(DF);
    }
  }
  for (i=0; i<MSList.Num(); i++)
  {
    SMultiSample *pS = &MSList[i];
    if (pS->Quality == 0 && MSList[i].Type>=2)
    {
      sprintf(str, "%dx", pS->Type);
      int j;
      for (j=0; j<SupAA.Num(); j++)
      {
        if (!stricmp(SupAA[j].szDescr, str))
          break;
      }
      if (j == SupAA.Num())
      {
        DF.nAPIType = pS->Type;
        DF.nQuality = pS->Quality;
        DF.nSamples = pS->Type;
        strcpy(DF.szDescr, str);
        if (formats)
          formats[SupAA.Num()] = DF;
        SupAA.AddElem(DF);
      }
    }
  }
#endif

#ifdef XENON
  SAAFormat DF;
  ZeroStruct(DF);
  DF.nAPIType = 0;
  DF.nQuality = 0;
  DF.nSamples = 2;
  strcpy(DF.szDescr, "2X");
	if(formats)
		formats[0] = DF;
  DF.nSamples = 4;
  strcpy(DF.szDescr, "4X");
	if(formats)
    formats[SupAA.Num()] = DF;
  SupAA.AddElem(DF);
#endif

  if (formats)
    qsort(&formats[0], SupAA.Num(), sizeof(SAAFormat), MSAA_Cmp);

  return SupAA.Num();
}

int CD3D9Renderer::GetAAFormat(TArray<SAAFormat>& Formats, bool bReset)
{
	if(bReset)
	{
		Formats.Free();
		return 0;
	}

	SDispFormat DispFmt;

	DispFmt.m_Width=GetWidth();
	DispFmt.m_Height=GetHeight();
	DispFmt.m_BPP=GetColorBpp();

	int nNums = EnumAAFormats(DispFmt,NULL);
	if (nNums > 0)
	{
		SAAFormat *formatArray = new SAAFormat[nNums];
		EnumAAFormats(DispFmt,formatArray);

		for(int aa = 0; aa < nNums; ++aa)
		{
			Formats.push_back(formatArray[aa]);
		}
		delete [] formatArray;
	}

  if (bReset)
    return nNums;
  uint i;

  if (!CV_r_fsaa || nNums <= 0)
    return -1;
  for (i=0; i<Formats.Num(); i++)
  {
    if (CV_r_fsaa == 1 && Formats[i].nAPIType == D3DMULTISAMPLE_NONMASKABLE)
      continue;
    if (CV_r_fsaa_samples == Formats[i].nSamples && CV_r_fsaa_quality == Formats[i].nQuality)
      return i;
  }
  _SetVar("r_FSAA_samples", Formats[0].nSamples);
  _SetVar("r_FSAA_quality", Formats[0].nQuality);

  return 0;
}

void CD3D9Renderer::CheckFSAAChange (void)
{
  if (m_bEditor)
  {
    if (CV_r_fsaa != m_FSAA)
    {
      CV_r_fsaa = m_FSAA;
      iLog->Log("FSAA in editor mode is currently not supported");
    }
    return;
  }
  bool bChanged = false;
#ifdef XENON
  if (CV_d3d9_predicatedtiling != m_Predicated)
    bChanged = true;
#endif
  if (!CV_r_hdrrendering && CV_r_fsaa)
  {
		iLog->Log("FSAA in non-HDR mode is currently not supported (use \"r_HDRRendering 1\" or the options menu)");
    _SetVar("r_FSAA", 0);
  }
  if (CV_r_fsaa != m_FSAA || (CV_r_fsaa && (m_FSAA_quality != CV_r_fsaa_quality || m_FSAA_samples != CV_r_fsaa_samples)))
  {
    if (CV_r_fsaa && (m_bDeviceSupports_A16B16G16R16_FSAA || m_bDeviceSupports_G16R16_FSAA))
    {
      if (m_bDeviceSupports_G16R16_FSAA)
        CTexture::m_eTFZ = eTF_G16R16F;
      else
        CTexture::m_eTFZ = eTF_A16B16G16R16F;
      TArray<SAAFormat> Formats;
      int nNum = GetAAFormat(Formats, false);
      if (nNum < 0)
      {
        iLog->Log(" Full scene AA: Not supported\n");
        _SetVar("r_FSAA", 0);
        m_FSAA = 0;
      }
      else
      {
        iLog->Log(" Full scene AA: Enabled: %s (%d Quality)", Formats[nNum].szDescr, Formats[nNum].nQuality);
        if (Formats[nNum].nQuality != m_FSAA_quality || Formats[nNum].nSamples != m_FSAA_samples)
        {
          bChanged = true;
          _SetVar("r_FSAA_quality", Formats[nNum].nQuality);
          _SetVar("r_FSAA_samples", Formats[nNum].nSamples);
        }
        else
        if (!m_FSAA)
          bChanged = true;
        GetAAFormat(Formats, true);
      }
    }
    else
    {
      CTexture::m_eTFZ = eTF_R32F;
      bChanged = true;
      iLog->Log(" Full scene AA: Disabled");
    }
    m_FSAA = CV_r_fsaa;
    m_FSAA_quality = CV_r_fsaa_quality;
    m_FSAA_samples = CV_r_fsaa_samples;
  }
  if (bChanged)
  {
    int nCBPP = m_cbpp;
    int nWidth = m_width;
    int nHeight = m_height;
    ChangeResolution(nWidth, nHeight, nCBPP, 75, m_CVFullScreen->GetIVal()!=0);
#if defined (DIRECT3D9) || defined(OPENGL)
    m_pd3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, CV_r_fsaa != 0);
#endif
  }
}

//==========================================================================

void CD3D9Renderer::InitATIAPI()
{
#if defined(WIN32)
	assert(!m_bDriverHasActiveMultiGPU);
	
	int iGPU = AtiMultiGPUAdapters();

	m_bDriverHasActiveMultiGPU = iGPU>1;

	iLog->Log("ATI: Multi GPU count = %d",iGPU); 

  if (m_bDriverHasActiveMultiGPU)
  {
    m_nGPUs = iGPU;
  }

#endif // defined(WIN32)
}

void CD3D9Renderer::InitNVAPI()
{
#if defined(WIN32)
	{
		NvAPI_Status stat = NvAPI_Initialize();
		iLog->Log("  NvAPI_Initialize: %s(%d)",stat?"failed":"ok",stat);
		m_bNVLibInitialized = (stat==0);
	}

	if(!m_bNVLibInitialized) 
		return;

	assert(!m_bDriverHasActiveMultiGPU);

	NvDisplayHandle     displayHandles[NVAPI_MAX_DISPLAYS]; 
	NvU32               displayCount; 
	NvLogicalGpuHandle  logicalGPUs[NVAPI_MAX_LOGICAL_GPUS]; 
	NvU32               logicalGPUCount; 
	NvPhysicalGpuHandle physicalGPUs[NVAPI_MAX_PHYSICAL_GPUS]; 
	NvU32               physicalGPUCount; 

	NV_DISPLAY_DRIVER_VERSION       version = {0}; 
	version.version = NV_DISPLAY_DRIVER_VERSION_VER; 

	NvAPI_Status    status; 

	status = NvAPI_GetDisplayDriverVersion(NVAPI_DEFAULT_HANDLE, &version); 

	if (status != NVAPI_OK) 
		iLog->LogError("NVAPI: Unable to get Driver version (%d)",status); 

	// enumerate displays 
	displayCount = 0; 
	for (int i = 0, status = NVAPI_OK; (status == NVAPI_OK) && (i < NVAPI_MAX_DISPLAYS); i++) 
	{ 
		status = NvAPI_EnumNvidiaDisplayHandle(i, &displayHandles[i]); 
		if (status == NVAPI_OK) 
			displayCount++; 
	} 

	// enumerate logical gpus 
	status = NvAPI_EnumLogicalGPUs(logicalGPUs, &logicalGPUCount); 
	if (status != NVAPI_OK) 
		iLog->LogError("NVAPI: Unable to Enumerate Logical GPUs (%d)",status); 

	// enumerate physical gpus 
	status = NvAPI_EnumPhysicalGPUs(physicalGPUs, &physicalGPUCount); 
	if (status != NVAPI_OK) 
		iLog->LogError("NVAPI: Unable to Enumerate Physical GPUs (%d)",status); 

	// a properly configured sli system should show up as one logical device with multiple physical devices 
	if (logicalGPUCount == 1) 
	{ 
		if (physicalGPUCount > 1)
		{ 
			// we have an sli system 
			iLog->Log("NVAPI: System configured as SLI: %d logical GPUs and %d physical GPUs",logicalGPUCount,physicalGPUCount);
      m_nGPUs = min(physicalGPUCount, 31);

			m_bDriverHasActiveMultiGPU=true;

			// here we can add the SLI bit control mechanism. 
		}
		else
		{ 
			// logicalGPUCount == physicalGPUCount == 1, so it's a single GPU system 
			iLog->Log("NVAPI: Single GPU System",logicalGPUCount,physicalGPUCount);         
		} 
	} 
	else 
	{ 
		// we have more than one logical GPU; it is likely that this is a multi-gpu system with SLI disabled in the control panel
		iLog->Log("NVAPI: System with %d logical GPUs and %d physical GPUs. SLI IS DISABLED",logicalGPUCount,physicalGPUCount);
	} 
#endif // defined(WIN32)
}

bool CD3D9Renderer::SetRes(void)  
{
  HRESULT hr;

  ChangeLog();

///////////////////////////////////////////////////////////////////
#ifdef _XBOX
  // Create the D3D object.
    
  //CInterfacePtr<IDirect3D9> pD3D;
  m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
  if( !m_pD3D )
      return false;

	XVIDEO_MODE VideoMode;
	XGetVideoMode(&VideoMode);

  bool bEnable720p  = VideoMode.dwDisplayWidth >= 1280;
  bool bEnable1080i = VideoMode.dwDisplayWidth >= 1920;

  if (bEnable720p)
    m_pTilingScenarios = g_TilingScenarios720;
  else
  if (bEnable1080i)
    m_pTilingScenarios = g_TilingScenarios1080;
  else
    m_pTilingScenarios = g_TilingScenarios480;

  // Set up the structure used to create the D3DDevice.
  D3DPRESENT_PARAMETERS d3dpp; 
  ZeroMemory( &d3dpp, sizeof(d3dpp) );
  d3dpp.BackBufferWidth        = VideoMode.dwDisplayWidth;
  d3dpp.BackBufferHeight       = VideoMode.dwDisplayHeight;
  d3dpp.BackBufferFormat       = D3DFMT_A8R8G8B8;
  d3dpp.BackBufferCount        = 0;
  d3dpp.EnableAutoDepthStencil = FALSE;
  d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
  d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
  d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE; //D3DPRESENT_INTERVAL_ONE;
  d3dpp.DisableAutoBackBuffer = TRUE;
  d3dpp.DisableAutoFrontBuffer = TRUE;
  m_d3dpp = d3dpp;
  m_pd3dpp = &m_d3dpp;

  if( FAILED(SetupPresentationParameters(m_pTilingScenarios[m_dwTilingScenarioIndex], d3dpp)))
  {
    iLog->LogError("Error: Could not set up presentation parameters.");
    return false;
  }

  // Create the Direct3D device.
  if( FAILED( m_pD3D->CreateDevice(0, D3DDEVTYPE_HAL, NULL,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_BUFFER_2_FRAMES,
                                  &d3dpp, &g_pd3dDevice ) ) )
    return false;

  D3DRING_BUFFER_PARAMETERS Ring;
  ZeroMemory(&Ring, sizeof(Ring));
  Ring.pPrimary   = XPhysicalAlloc(XENON_RING_SIZE, MAXULONG_PTR, GPU_COMMAND_BUFFER_ALIGNMENT, PAGE_READWRITE | PAGE_WRITECOMBINE | MEM_LARGE_PAGES);
  Ring.pSecondary = XPhysicalAlloc(XENON_RING_SIZE, MAXULONG_PTR, GPU_COMMAND_BUFFER_ALIGNMENT, PAGE_READWRITE | PAGE_WRITECOMBINE | MEM_LARGE_PAGES);
  Ring.PrimarySize = Ring.SecondarySize = XENON_RING_SIZE;
  Ring.SegmentCount = 0;
  g_pd3dDevice->SetRingBufferParameters(&Ring);


#ifdef DO_RENDERLOG
  m_pd3dDevice = new IXenonDirect3DDevice9;
  byte *pDst = (byte *)&m_pd3dDevice->m_Pending;
  byte *pSrc = (byte *)&g_pd3dDevice->m_Pending;
  memcpy(pDst, pSrc, sizeof(D3DDevice));
#else
  m_pd3dDevice = (LPDIRECT3DDEVICE9)g_pd3dDevice;
#endif

  /////////////////////////////////////////////////////////////////////////////////
  // Hardcoded values.
  /////////////////////////////////////////////////////////////////////////////////
  m_cbpp = 32;
  m_sbpp = 8;
  m_zbpp = 24;

  m_FormatA8.BitsPerPixel = 8;
  m_FormatA8L8.BitsPerPixel = 16;

  m_width = d3dpp.BackBufferWidth;
  m_height = d3dpp.BackBufferHeight;
  m_bFullScreen = true;


  m_CVWidth->Set(m_width);
  m_CVHeight->Set(m_height);
  m_CVFullScreen->Set( (int)m_bFullScreen );
  m_CVColorBits->Set( m_cbpp );

  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  m_D3DSettings.IsWindowed = false;
  m_D3DSettings.pFullscreen_AdapterInfo = new D3DAdapterInfo;
  m_D3DSettings.pFullscreen_DeviceInfo = new D3DDeviceInfo;
  m_D3DSettings.pFullscreen_DeviceCombo = new D3DDeviceCombo;

  D3DAdapterInfo *pAI = m_D3DSettings.PAdapterInfo();
  D3DADAPTER_IDENTIFIER9 *ai = &pAI->AdapterIdentifier;
  D3DDeviceInfo *di = m_D3DSettings.PDeviceInfo();
  D3DDeviceCombo* pDeviceCombo = m_D3DSettings.PDeviceCombo();

  pDeviceCombo->AdapterFormat = d3dpp.BackBufferFormat;
  pDeviceCombo->BackBufferFormat = d3dpp.BackBufferFormat;
  pDeviceCombo->AdapterOrdinal = 0;
  pDeviceCombo->DeviceType = D3DDEVTYPE_HAL;
  pDeviceCombo->Windowed = false;

	if (FAILED(hr = m_pD3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, ai)))
		return false;

	pAI->AdapterOrdinal = 0;
	pAI->m_MaxWidth = m_width;
	pAI->m_MaxHeight = m_height;

  di->DevType = D3DDEVTYPE_HAL;
  /////////////////////////////////////////////////////////////////////////////////

  {
  // Store device Caps
    m_pd3dDevice->GetDeviceCaps(&m_d3dCaps);
    m_pd3dCaps = &m_d3dCaps;
		di->Caps = m_d3dCaps;
    m_dwCreateFlags = D3DCREATE_FPU_PRESERVE|D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE;

    // Store render target surface desc
    m_Features |= RFT_ZLOCKABLE;

    // Initialize the app's device-dependent objects
    hr = OnD3D9CreateDevice(m_pd3dDevice, NULL, NULL);
    hr = OnD3D9ResetDevice(m_pd3dDevice, NULL, NULL);
  }

  // The focus window can be a specified to be a different window than the
  // device window.  If not, use the device window as the focus window.
  CreateContext(m_hWnd);

#else //_XBOX

///////////////////////////////////////////////////////////////////

  UnSetRes();

#if defined (DIRECT3D9) || defined(OPENGL)
  DXUTSetCallbackD3D9DeviceAcceptable(IsD3D9DeviceAcceptable);
  DXUTSetCallbackD3D9DeviceCreated(OnD3D9CreateDevice);
  DXUTSetCallbackD3D9DeviceReset(OnD3D9ResetDevice);
  DXUTSetCallbackD3D9FrameRender(OnD3D9FrameRender);
  DXUTSetCallbackD3D9DeviceLost(OnD3D9LostDevice);
  DXUTSetCallbackD3D9DeviceDestroyed(OnD3D9DestroyDevice);
  DXUTSetCallbackDeviceChanging(OnD3D9DeviceChanging);
#elif defined (DIRECT3D10)
  DXUTSetCallbackD3D10DeviceAcceptable(IsD3D10DeviceAcceptable);
  DXUTSetCallbackD3D10DeviceCreated(OnD3D10CreateDevice);
  DXUTSetCallbackD3D10SwapChainResized(OnD3D10ResizedSwapChain);
  DXUTSetCallbackD3D10FrameRender(OnD3D10FrameRender);
  DXUTSetCallbackD3D10SwapChainReleasing(OnD3D10ReleasingSwapChain);
  DXUTSetCallbackD3D10DeviceDestroyed(OnD3D10DestroyDevice );
  DXUTSetCallbackDeviceChanging(OnD3D10DeviceChanging);
#endif
	WCHAR *sExtDXCmds = NULL;
	if (CD3D9Renderer::CV_d3d9_null_ref_device != 0)
	{
		sExtDXCmds = L"-forcenullref";
	}
  DXUTInit(true, true, sExtDXCmds ); // Parse the command line, show msgboxes on error, no extra command line params
  DXUTSetWindow(m_hWnd, m_hWnd, m_hWnd, false);

  int width = m_width;
  int height = m_height;
  if (m_bEditor)
  {
    width = m_deskwidth;
    height = m_deskheight;
  }

  // DirectX9 and DirectX10 device creating
  hr = DXUTCreateDevice(!m_bFullScreen, width, height, m_sbpp, m_cbpp);
  if (FAILED(hr))
    return false;

  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
#if defined (DIRECT3D9) || defined (OPENGL)
  OnD3D9PostCreateDevice(DXUTGetD3D9Device());
#elif defined (DIRECT3D10)
  //DXUTCheckForWindowSizeChange();
  //DXUTCheckForWindowChangingMonitors();
  OnD3D10PostCreateDevice(DXUTGetD3D10Device());
#endif
  AdjustWindowForChange();

  CreateContext(m_hWnd);
#endif

	// attach to d3d9 wrapper - useful for statistics and debugging - needs additional d3d9.dll path adjustment (MartinM)
#ifdef WIN32
	GetDevice()->QueryInterface(IDD_IDirectBee,(void **)&m_pDirectBee);
#endif

	m_matView = new CMatrixStack(16, 0);
  if(m_matView == NULL)
    return false;
	m_matProj = new CMatrixStack(16, 0);
  if (m_matProj == NULL)
    return false;

  return true;
} 


bool SPixFormat::CheckSupport(int Format, const char *szDescr, ETexture_Usage eTxUsage)
{
  bool bRes = true;
  CD3D9Renderer *rd = gcpRendD3D;
#if defined (DIRECT3D10)
  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
  ID3D10Device *pDevice = rd->m_pd3dDevice;

  DXGI_FORMAT d3dFmt = (DXGI_FORMAT)Format;
  UINT nOptions;
  HRESULT hr = pDevice->CheckFormatSupport(d3dFmt, &nOptions);
  if (SUCCEEDED(hr))
  {
    if (nOptions & (D3D10_FORMAT_SUPPORT_TEXTURE2D | D3D10_FORMAT_SUPPORT_TEXTURECUBE))
    {
      if (nOptions & D3D10_FORMAT_SUPPORT_MIP_AUTOGEN)
        rd->RecognizePixelFormat(*this, d3dFmt, DXUTGetDXGIColorBits(d3dFmt), szDescr, true);
      else
        rd->RecognizePixelFormat(*this, d3dFmt, DXUTGetDXGIColorBits(d3dFmt), szDescr, false);
    }
    else
      bRes = false;
  }
  else
    bRes = false;
#else
  IDirect3D9* pD3D = rd->m_pD3D;
#if !defined(XENON) && !defined(PS3)
  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
  D3DDEVTYPE DevType = pDev->d3d9.DeviceType;
  UINT nAdapter = pDev->d3d9.AdapterOrdinal;
  D3DFORMAT AdapterFormat = pDev->d3d9.AdapterFormat;
#else
  D3DDEVTYPE DevType = rd->m_D3DSettings.DevType();
  UINT nAdapter = rd->m_D3DSettings.AdapterOrdinal();
  D3DFORMAT AdapterFormat = rd->m_D3DSettings.AdapterFormat();
#endif

  D3DFORMAT d3dFmt = (D3DFORMAT)Format;
	HRESULT hr;
  switch (eTxUsage)
  {
    case (eTXUSAGE_AUTOGENMIPMAP): 
	    hr = pD3D->CheckDeviceFormat(nAdapter, DevType, AdapterFormat, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, d3dFmt);
      break;
    case (eTXUSAGE_DEPTHSTENCIL):
	    hr = pD3D->CheckDeviceFormat(nAdapter, DevType, AdapterFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, d3dFmt);
      break;
    case (eTXUSAGE_RENDERTARGET): 
 	   hr = pD3D->CheckDeviceFormat(nAdapter, DevType, AdapterFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, d3dFmt);
      break;
    default: break;
  }

  if (hr == D3D_OK)
    rd->RecognizePixelFormat(*this, d3dFmt, GetD3D9ColorBits(d3dFmt), szDescr, true);
  else
  if (hr == D3DOK_NOAUTOGEN)
    rd->RecognizePixelFormat(*this, d3dFmt, GetD3D9ColorBits(d3dFmt), szDescr, false);
  else
    bRes = false;
#endif
  return bRes;
}

void CD3D9Renderer::GetVideoMemoryUsageStats( size_t& vidMemUsedThisFrame, size_t& vidMemUsedRecently )
{
#if defined (DIRECT3D9) || defined (OPENGL)
	//int usedByD3D( m_MaxTextureMemory - m_pd3dDevice->GetAvailableTextureMem() ); // apparently on-board + AGP

	// determine front, back and z buffer sizes as they are not registered in the texture manager
	D3DSURFACE_DESC desc;		
	m_pBackBuffer->GetDesc( &desc );		
	
	size_t sizeOfFrontBackDepth( 
		desc.Height * desc.Width * GetD3D9ColorBytes( desc.Format ) * GetD3D9NumSamples( desc.MultiSampleType, desc.MultiSampleQuality ) * ( 1 + m_pd3dpp->BackBufferCount ) );

	if( m_pd3dpp->EnableAutoDepthStencil )
	{
		m_pZBuffer->GetDesc( &desc );	
		sizeOfFrontBackDepth += desc.Height * desc.Width * GetD3D9ColorBytes( desc.Format ) * GetD3D9NumSamples( desc.MultiSampleType, desc.MultiSampleQuality );
	}

	// init vid memory usage stats
	vidMemUsedThisFrame = sizeOfFrontBackDepth;
	vidMemUsedRecently = sizeOfFrontBackDepth;

	// add texture usage
	CCryName name( CTexture::mfGetClassName() );
	SResourceContainer* pTexContainer( CBaseResource::GetResourcesForClass( name ) );
	if( pTexContainer )
	{
		ResourcesMapItor itEnd( pTexContainer->m_RMap.end() );
		for( ResourcesMapItor it( pTexContainer->m_RMap.begin() ); it != itEnd; ++it )
		{
			CTexture* pTex( static_cast<CTexture*>( it->second ) );
			if( pTex && !pTex->IsNoTexture() )
			{
				if( pTex->m_nAccessFrameID == m_nFrameUpdateID )
					vidMemUsedThisFrame += pTex->GetDeviceDataSize();
				if( m_nFrameUpdateID - pTex->m_nAccessFrameID < 20 )
					vidMemUsedRecently += pTex->GetDeviceDataSize();
			}
		}
	}	
#endif
}

void CD3D9Renderer::RecognizePixelFormat(SPixFormat& Dest, uint FormatFromD3D, INT InBitsPerPixel, const char* InDesc, bool bAutoGenMips)
{
  Dest.Init();
  Dest.DeviceFormat  = FormatFromD3D;
  Dest.MaxWidth      = 2048;
  Dest.MaxHeight     = 2048;
  Dest.Desc          = InDesc;
  Dest.BitsPerPixel  = InBitsPerPixel;
  Dest.Next          = m_FirstPixelFormat;
  Dest.bCanAutoGenMips = bAutoGenMips;
  m_FirstPixelFormat = &Dest;

  if (bAutoGenMips)
    iLog->Log("  Using '%s' pixel texture format (Can autogen mips)", InDesc);
  else
    iLog->Log("  Using '%s' pixel texture format", InDesc);
}

//===========================================================================================

#if defined (DIRECT3D9) || defined(OPENGL)

//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK CD3D9Renderer::IsD3D9DeviceAcceptable(D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext)
{
  // No fallback defined by crysis, so reject any device that doesn't support at least ps.2.0
  if(pCaps->PixelShaderVersion < D3DPS_VERSION(2,0))
    return false;

  // Skip backbuffer formats that don't support alpha blending
  IDirect3D9* pD3D = gcpRendD3D->m_pD3D; 
  if(FAILED(pD3D->CheckDeviceFormat(pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DRTYPE_TEXTURE, BackBufferFormat)))
    return false;

  return true;
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK CD3D9Renderer::OnD3D9FrameRender(LPDIRECT3DDEVICE9 pd3dDevice, double fTime, float fElapsedTime, void* pUserContext)
{
}

struct SNVDevice
{
  int nDevID;
  char *szDescr;
};
static SNVDevice s_NVDevList[] = 
{
  // Device ID    Chip ID name
  { 0x014F,  "GeForce 6200"}  ,
  { 0x00F3,  "GeForce 6200"}  ,
  { 0x0221,  "GeForce 6200"}  ,
  { 0x0163,  "GeForce 6200 LE"}  ,
  { 0x0162,  "GeForce 6200SE TurboCache(TM)"}  ,
  { 0x0161,  "GeForce 6200 TurboCache(TM)"}  ,
  { 0x0162,  "GeForce 6200SE TurboCache(TM)"}  ,
  { 0x0160,  "GeForce 6500"}  ,
  { 0x0141,  "GeForce 6600"}  ,
  { 0x00F2,  "GeForce 6600"}  ,
  { 0x0140,  "GeForce 6600 GT"}  ,
  { 0x00F1,  "GeForce 6600 GT"}  ,
  { 0x0142,  "GeForce 6600 LE"}  ,
  { 0x00F4,  "GeForce 6600 LE"}  ,
  { 0x0143,  "GeForce 6600 VE"}  ,
  { 0x0147,  "GeForce 6700 XL"}  ,
  { 0x0041,  "GeForce 6800"}  ,
  { 0x00C1,  "GeForce 6800"}  ,
  { 0x0047,  "GeForce 6800 GS"}  ,
  { 0x00F6,  "GeForce 6800 GS"}  ,
  { 0x00C0,  "GeForce 6800 GS"}  ,
  { 0x0045,  "GeForce 6800 GT"}  ,
  { 0x00F9,  "GeForce 6800 Series GPU"}  ,
  { 0x00C2,  "GeForce 6800 LE"}  ,
  { 0x0040,  "GeForce 6800 Ultra"}  ,
  { 0x00F9,  "GeForce 6800 Series GPU"}  ,
  { 0x0043,  "GeForce 6800 XE"}  ,
  { 0x0048,  "GeForce 6800 XT"}  ,
  { 0x0218,  "GeForce 6800 XT"}  ,
  { 0x00C3,  "GeForce 6800 XT"}  ,
  { 0x01DF,  "GeForce 7300 GS"}  ,
  { 0x0393,  "GeForce 7300 GT"}  ,
  { 0x01D1,  "GeForce 7300 LE"}  ,
  { 0x01D3,  "GeForce 7300 SE"}  ,
  { 0x01DD,  "GeForce 7500 LE"}  ,
  { 0x0392,  "GeForce 7600 GS"}  ,
  { 0x0392,  "GeForce 7600 GS"}  ,
  { 0x02E1,  "GeForce 7600 GS"}  ,
  { 0x0391,  "GeForce 7600 GT"}  ,
  { 0x0394,  "GeForce 7600 LE"}  ,
  { 0x00F5,  "GeForce 7800 GS"}  ,
  { 0x0092,  "GeForce 7800 GT"}  ,
  { 0x0091,  "GeForce 7800 GTX"}  ,
  { 0x0291,  "GeForce 7900 GT/GTO"}  ,
  { 0x0290,  "GeForce 7900 GTX"}  ,
  { 0x0293,  "GeForce 7900 GX2"}  ,
  { 0x0294,  "GeForce 7950 GX2"}  ,
  { 0x0322,  "GeForce FX 5200"}  ,
  { 0x0321,  "GeForce FX 5200 Ultra"}  ,
  { 0x0323,  "GeForce FX 5200LE"}  ,
  { 0x0326,  "GeForce FX 5500"}  ,
  { 0x0326,  "GeForce FX 5500"}  ,
  { 0x0312,  "GeForce FX 5600"}  ,
  { 0x0311,  "GeForce FX 5600 Ultra"}  ,
  { 0x0314,  "GeForce FX 5600XT"}  ,
  { 0x0342,  "GeForce FX 5700"}  ,
  { 0x0341,  "GeForce FX 5700 Ultra"}  ,
  { 0x0343,  "GeForce FX 5700LE"}  ,
  { 0x0344,  "GeForce FX 5700VE"}  ,
  { 0x0302,  "GeForce FX 5800"}  ,
  { 0x0301,  "GeForce FX 5800 Ultra"}  ,
  { 0x0331,  "GeForce FX 5900"}  ,
  { 0x0330,  "GeForce FX 5900 Ultra"}  ,
  { 0x0333,  "GeForce FX 5950 Ultra"}  ,
  { 0x0324,  "GeForce FX Go5200 64M"}  ,
  { 0x031A,  "GeForce FX Go5600"}  ,
  { 0x0347,  "GeForce FX Go5700"}  ,
  { 0x0167,  "GeForce Go 6200/6400"}  ,
  { 0x0168,  "GeForce Go 6200/6400"}  ,
  { 0x0148,  "GeForce Go 6600"}  ,
  { 0x00c8,  "GeForce Go 6800"}  ,
  { 0x00c9,  "GeForce Go 6800 Ultra"}  ,
  { 0x0098,  "GeForce Go 7800"}  ,
  { 0x0099,  "GeForce Go 7800 GTX"}  ,
  { 0x0298,  "GeForce Go 7900 GS"}  ,
  { 0x0299,  "GeForce Go 7900 GTX"}  ,
  { 0x0185,  "GeForce MX 4000"}  ,
  { 0x00FA,  "GeForce PCX 5750"}  ,
  { 0x00FB,  "GeForce PCX 5900"}  ,
  { 0x0110,  "GeForce2 MX/MX 400"}  ,
  { 0x0111,  "GeForce2 MX200"}  ,
  { 0x0110,  "GeForce2 MX/MX 400"}  ,
  { 0x0200,  "GeForce3"}  ,
  { 0x0201,  "GeForce3 Ti200"}  ,
  { 0x0202,  "GeForce3 Ti500"}  ,
  { 0x0172,  "GeForce4 MX 420"}  ,
  { 0x0171,  "GeForce4 MX 440"}  ,
  { 0x0181,  "GeForce4 MX 440 with AGP8X"}  ,
  { 0x0173,  "GeForce4 MX 440-SE"}  ,
  { 0x0170,  "GeForce4 MX 460"}  ,
  { 0x0253,  "GeForce4 Ti 4200"}  ,
  { 0x0281,  "GeForce4 Ti 4200 with AGP8X"}  ,
  { 0x0251,  "GeForce4 Ti 4400"}  ,
  { 0x0250,  "GeForce4 Ti 4600"}  ,
  { 0x0280,  "GeForce4 Ti 4800"}  ,
  { 0x0282,  "GeForce4 Ti 4800SE"}  ,
  { 0x0203,  "Quadro DCC"}  ,
  { 0x0309,  "Quadro FX 1000"}  ,
  { 0x034E,  "Quadro FX 1100"}  ,
  { 0x00FE,  "Quadro FX 1300"}  ,
  { 0x00CE,  "Quadro FX 1400"}  ,
  { 0x0308,  "Quadro FX 2000"}  ,
  { 0x0338,  "Quadro FX 3000"}  ,
  { 0x00FD,  "Quadro PCI-E Series"}  ,
  { 0x00F8,  "Quadro FX 3400/4400"}  ,
  { 0x00CD,  "Quadro FX 3450/4000 SDI"}  ,
  { 0x004E,  "Quadro FX 4000"}  ,
  { 0x00CD,  "Quadro FX 3450/4000 SDI"}  ,
  { 0x00F8,  "Quadro FX 3400/4400"}  ,
  { 0x009D,  "Quadro FX 4500"}  ,
  { 0x029F,  "Quadro FX 4500 X2"}  ,
  { 0x032B,  "Quadro FX 500/FX 600"}  ,
  { 0x014E,  "Quadro FX 540"}  ,
  { 0x014C,  "Quadro FX 540 MXM"}  ,
  { 0x032B,  "Quadro FX 500/FX 600"}  ,
  { 0X033F,  "Quadro FX 700"}  ,
  { 0x034C,  "Quadro FX Go1000"}  ,
  { 0x00CC,  "Quadro FX Go1400"}  ,
  { 0x031C,  "Quadro FX Go700"}  ,
  { 0x018A,  "Quadro NVS with AGP8X"}  ,
  { 0x032A,  "Quadro NVS 280 PCI"}  ,
  { 0x00FD,  "Quadro PCI-E Series"}  ,
  { 0x0165,  "Quadro NVS 285"}  ,
  { 0x017A,  "Quadro NVS"}  ,
  { 0x018A,  "Quadro NVS with AGP8X"}  ,
  { 0x0113,  "Quadro2 MXR/EX"}  ,
  { 0x017A,  "Quadro NVS"}  ,
  { 0x018B,  "Quadro4 380 XGL"}  ,
  { 0x0178,  "Quadro4 550 XGL"}  ,
  { 0x0188,  "Quadro4 580 XGL"}  ,
  { 0x025B,  "Quadro4 700 XGL"}  ,
  { 0x0259,  "Quadro4 750 XGL"}  ,
  { 0x0258,  "Quadro4 900 XGL"}  ,
  { 0x0288,  "Quadro4 980 XGL"}  ,
  { 0x028C,  "Quadro4 Go700"}  ,
  { 0x0295,  "NVIDIA GeForce 7950 GT"}  ,
  { 0x03D0,  "NVIDIA GeForce 6100 nForce 430"}  ,
  { 0x03D1,  "NVIDIA GeForce 6100 nForce 405"}  ,
  { 0x03D2,  "NVIDIA GeForce 6100 nForce 400"}  ,
  { 0x0241,  "NVIDIA GeForce 6150 LE"}  ,
  { 0x0242,  "NVIDIA GeForce 6100"}  ,
  { 0x0245,  "NVIDIA Quadro NVS 210S / NVIDIA GeForce 6150LE"}  ,
  { 0x029C,  "NVIDIA Quadro FX 5500"}  ,
  { 0x0191,  "NVIDIA GeForce 8800 GTX"}  ,
  { 0x0193,  "NVIDIA GeForce 8800 GTS"}  ,
  { 0x0194,  "NVIDIA GeForce 8800 GTX Ultra"}  ,
  { 0x0400,  "NVIDIA GeForce 8600 GTS"}  ,
  { 0x0402,  "NVIDIA GeForce 8600 GT"}  ,
  { 0x0421,  "NVIDIA GeForce 8500 GT"}  ,
  { 0x0422,  "NVIDIA GeForce 8400 GS"}  ,
  { 0x0423,  "NVIDIA GeForce 8300 GS"}  ,
  { 0x0,  NULL}
  // End of list
};

struct SATIDevice
{
  char *szDescr;
  char *szASIC;
  int nDevID;
};
static SATIDevice s_ATIDevList[] = 
{
  // Retail name, ASIC, Device ID
  { "ATI MOBILITY/RADEON X700" ,"RV410", 0x5653 },
  { "Radeon X1950 XTX Uber - Limited Edition" ,"R580", 0x7248 },
  { "Radeon X1950 XTX Uber - Limited Edition Secondary" ,"R580", 0x7268 },
  { "Radeon X800 CrossFire Edition" ,"R430", 0x554D },
  { "Radeon X800 CrossFire Edition Secondary" ,"R430", 0x556D },
  { "Radeon X850 CrossFire Edition" ,"R480", 0x5D52 },
  { "Radeon X850 CrossFire Edition Secondary" ,"R480", 0x5D72 },
  { "Radeon X550/X700 Series" ,"RV410", 0x564F },
  { "ATI FireGL T2" ,"RV350GL", 0x4154 },
  { "ATI FireGL T2 Secondary" ,"RV350GL", 0x4174 },
  { "ATI FireGL V3100" ,"RV370GL", 0x5B64 },
  { "ATI FireGL V3100 Secondary" ,"RV370GL", 0x5B74 },
  { "ATI FireGL V3200" ,"RV380GL", 0x3E54 },
  { "ATI FireGL V3200 Secondary" ,"RV380GL", 0x3E74 },
  { "ATI FireGL V3300" ,"RV515GL", 0x7152},
  { "ATI FireGL V3300 Secondary" ,"RV515GL", 0x7172},
  { "ATI FireGL V3350" ,"RV515GL", 0x7153},
  { "ATI FireGL V3350 Secondary" ,"RV515GL", 0x7173},
  { "ATI FireGL V3400" ,"RV530GL", 0x71D2},
  { "ATI FireGL V3400 Secondary" ,"RV530GL", 0x71F2},
  { "ATI FireGL V5000" ,"RV410GL", 0x5E48},
  { "ATI FireGL V5000 Secondary" ,"RV410GL", 0x5E68},
  { "ATI FireGL V5100" ,"R423GL", 0x5551},
  { "ATI FireGL V5100  Secondary" ,"R423GL", 0x5571},
  { "ATI FireGL V5200" ,"RV530GL", 0x71DA},
  { "ATI FireGL V5200 Secondary" ,"RV530GL", 0x71FA},
  { "ATI FireGL V5300" ,"R520GL", 0x7105},
  { "ATI FireGL V5300 Secondary" ,"R520GL", 0x7125},
  { "ATI FireGL V7100" ,"R423GL", 0x5550},
  { "ATI FireGL V7100 Secondary" ,"R423GL", 0x5570},
  { "ATI FireGL V7200" ,"R480GL", 0x5D50},
  { "ATI FireGL V7200 " ,"R520GL", 0x7104},
  { "ATI FireGL V7200 Secondary" ,"R480GL", 0x5D70},
  { "ATI FireGL V7200 Secondary " ,"R520GL", 0x7124},
  { "ATI FireGL V7300" ,"R520GL", 0x710E},
  { "ATI FireGL V7300 Secondary" ,"R520GL", 0x712E},
  { "ATI FireGL V7350" ,"R520GL", 0x710F},
  { "ATI FireGL V7350 Secondary" ,"R520GL", 0x712F},
  { "ATI FireGL X1" ,"R300GL", 0x4E47},
  { "ATI FireGL X1 Secondary" ,"R300GL", 0x4E67},
  { "ATI FireGL X2-256/X2-256t" ,"R350GL", 0x4E4B},
  { "ATI FireGL X2-256/X2-256t Secondary" ,"R350GL", 0x4E6B},
  { "ATI FireGL X3-256" ,"R420GL", 0x4A4D},
  { "ATI FireGL X3-256 Secondary" ,"R420GL", 0x4A6D},
  { "ATI FireGL Z1" ,"R300GL", 0x4147},
  { "ATI FireGL Z1 Secondary" ,"R300GL", 0x4167},
  { "ATI FireMV 2200" ,"RV370", 0x5B65},
  { "ATI FireMV 2200 Secondary" ,"RV370", 0x5B75},
  { "ATI FireMV 2250" ,"RV515", 0x719B},
  { "ATI FireMV 2250 Secondary" ,"RV515", 0x71BB},
  { "ATI FireMV 2400" ,"RV380", 0x3151},
  { "ATI FireMV 2400 Secondary" ,"RV380", 0x3171},
  { "ATI FireStream 2U" ,"R580", 0x724E},
  { "ATI FireStream 2U Secondary" ,"R580", 0x726E},
  { "ATI MOBILITY FIRE GL 7800" ,"M7", 0x4C58},
  { "ATI MOBILITY FIRE GL T2/T2e" ,"M10GL", 0x4E54},
  { "ATI MOBILITY FireGL V3100" ,"M22GL", 0x5464},
  { "ATI MOBILITY FireGL V3200" ,"M24GL", 0x3154},
  { "ATI MOBILITY FireGL V5000" ,"M26GL", 0x564A},
  { "ATI MOBILITY FireGL V5000" ,"M26GL", 0x564B},
  { "ATI MOBILITY FireGL V5100" ,"M28GL", 0x5D49},
  { "ATI MOBILITY FireGL V5200" ,"M56GL", 0x71C4},
  { "ATI MOBILITY FireGL V5250" ,"M56GL", 0x71D4},
  { "ATI MOBILITY FireGL V7100" ,"M58GL", 0x7106},
  { "ATI MOBILITY FireGL V7200" ,"M58GL", 0x7103},
  { "ATI MOBILITY RADEON" ,"M6", 0x4C59},
  { "ATI MOBILITY RADEON 7500" ,"M7", 0x4C57},
  { "ATI MOBILITY RADEON 9500" ,"M10", 0x4E52},
  { "ATI MOBILITY RADEON 9550" ,"M12", 0x4E56},
  { "ATI MOBILITY RADEON 9600/9700 Series" ,"M10", 0x4E50},
  { "ATI MOBILITY RADEON 9800" ,"M18", 0x4A4E},
  { "ATI Mobility Radeon HD 2300" ,"M71", 0x7210},
  { "ATI Mobility Radeon HD 2300" ,"M71", 0x7211},
  { "ATI Mobility Radeon HD 2400" ,"M72", 0x94C9},
  { "ATI Mobility Radeon HD 2400 XT" ,"M72", 0x94C8},
  { "ATI Mobility Radeon HD 2600" ,"M76", 0x9581},
  { "ATI Mobility Radeon HD 2600 XT" ,"M76", 0x9583},
  { "ATI Mobility Radeon X1300" ,"M52", 0x714A},
  { "ATI Mobility Radeon X1300" ,"M52", 0x7149},
  { "ATI Mobility Radeon X1300" ,"M52", 0x714B},
  { "ATI Mobility Radeon X1300" ,"M52", 0x714C},
  { "ATI Mobility Radeon X1350" ,"M52", 0x718B},
  { "ATI Mobility Radeon X1350" ,"M52", 0x718C},
  { "ATI Mobility Radeon X1350" ,"M52", 0x7196},
  { "ATI Mobility Radeon X1400" ,"M54", 0x7145},
  { "ATI Mobility Radeon X1450" ,"M54", 0x7186},
  { "ATI Mobility Radeon X1450" ,"M54", 0x718D},
  { "ATI Mobility Radeon X1600" ,"M56", 0x71C5},
  { "ATI Mobility Radeon X1700" ,"M56", 0x71D5},
  { "ATI Mobility Radeon X1700" ,"M56", 0x71DE},
  { "ATI Mobility Radeon X1700 XT" ,"M56", 0x71D6},
  { "ATI Mobility Radeon X1800" ,"M58", 0x7102},
  { "ATI Mobility Radeon X1800 XT" ,"M58", 0x7101},
  { "ATI Mobility Radeon X1900" ,"M58", 0x7284},
  { "ATI Mobility Radeon X2300" ,"M54", 0x718A},
  { "ATI Mobility Radeon X2300" ,"M54", 0x7188},
  { "ATI MOBILITY RADEON X300" ,"M22", 0x5461},
  { "ATI MOBILITY RADEON X300" ,"M22", 0x5460},
  { "ATI MOBILITY RADEON X300" ,"M24", 0x3152},
  { "ATI MOBILITY RADEON X600" ,"M24", 0x3150},
  { "ATI MOBILITY RADEON X600 SE" ,"M24C", 0x5462},
  { "ATI MOBILITY RADEON X700" ,"M26", 0x5652},
  { "ATI MOBILITY RADEON X700" ,"M26", 0x5653},
  { "ATI MOBILITY RADEON X700 Secondary" ,"M26", 0x5673},
  { "ATI MOBILITY RADEON X800" ,"M28", 0x5D4A},
  { "ATI MOBILITY RADEON X800  XT" ,"M28", 0x5D48},
  { "ATI Radeon 9550/X1050 Series","RV3500x", 4153},
  { "ATI Radeon 9550/X1050 Series Secondary","RV3500x", 4173},
  { "ATI RADEON 9600 Series","RV3500x", 0x4150},
  { "ATI RADEON 9600 Series","RV3500x", 0x4E51},
  { "ATI RADEON 9600 Series","RV3500x", 0x4151},
  { "ATI RADEON 9600 Series","RV3500x", 0x4155},
  { "ATI RADEON 9600 Series","RV3600x", 0x4152},
  { "ATI RADEON 9600 Series Secondary","RV3500x", 0x4E71},
  { "ATI RADEON 9600 Series Secondary","RV3500x", 0x4171},
  { "ATI RADEON 9600 Series Secondary","RV3500x", 0x4170},
  { "ATI RADEON 9600 Series Secondary","RV3500x", 0x4175},
  { "ATI RADEON 9600 Series Secondary","RV3600x", 0x4172},
  { "ATI Radeon HD 2900 XT" ,"R600", 0x9402},
  { "ATI Radeon HD 2900 XT" ,"R600", 0x9403},
  { "ATI Radeon HD 2900 XT" ,"R600", 0x9400},
  { "ATI Radeon HD 2900 XT" ,"R600", 0x9401},
  { "ATI Radeon X1200 Series" ,"RS690", 0x791E},
  { "ATI Radeon X1200 Series" ,"RS690M", 0x791F},
  { "ATI Radeon X1950 GT" ,"R580", 0x7288},
  { "ATI Radeon X1950 GT Secondary" ,"R580", 0x72A8},
  { "ATI RADEON X800 GT" ,"R430", 0x554E},
  { "ATI RADEON X800 GT Secondary" ,"R430", 0x556E},
  { "ATI RADEON X800 XL" ,"R430", 0x554D},
  { "ATI RADEON X800 XL Secondary" ,"R430", 0x556D},
  { "ATI RADEON X850 PRO" ,"R481", 0x4B4B},
  { "ATI RADEON X850 PRO Secondary" ,"R481", 0x4B6B},
  { "ATI RADEON X850 SE" ,"R481", 0x4B4A},
  { "ATI RADEON X850 SE Secondary" ,"R481", 0x4B6A},
  { "ATI RADEON X850 XT" ,"R481", 0x4B49},
  { "ATI RADEON X850 XT Platinum Edition" ,"R481", 0x4B4C},
  { "ATI RADEON X850 XT Platinum Edition Secondary" ,"R481", 0x4B6C},
  { "ATI RADEON X850 XT Secondary" ,"R481", 0x4B69},
  { "ATI Radeon Xpress 1200 Series" ,"RS600", 0x793F},
  { "ATI Radeon Xpress 1200 Series" ,"RS600", 0x7941},
  { "ATI Radeon Xpress 1200 Series" ,"RS600M", 0x7942},
  { "ATI Radeon Xpress Series" ,"RC410", 0x5A61},
  { "ATI Radeon Xpress Series" ,"RC410", 0x5A63},
  { "ATI Radeon Xpress Series" ,"RC410M", 0x5A62},
  { "ATI Radeon Xpress Series" ,"RS400", 0x5A41},
  { "ATI Radeon Xpress Series" ,"RS400", 0x5A43},
  { "ATI Radeon Xpress Series" ,"RS400M", 0x5A42},
  { "ATI Radeon Xpress Series" ,"RS480", 0x5954},
  { "ATI Radeon Xpress Series" ,"RS480", 0x5854},
  { "ATI Radeon Xpress Series" ,"RS480M", 0x5955},
  { "ATI Radeon Xpress Series" ,"RS482", 0x5974},
  { "ATI Radeon Xpress Series" ,"RS482", 0x5874},
  { "ATI Radeon Xpress Series" ,"RS482M", 0x5975},
  { "Radeon 9500" ,"R300", 0x4144},
  { "Radeon 9500" ,"R350", 0x4149},
  { "Radeon 9500 PRO / 9700" ,"R300", 0x4E45},
  { "Radeon 9500 PRO / 9700 Secondary" ,"R300", 0x4E65},
  { "Radeon 9500 Secondary" ,"R300", 0x4164},
  { "Radeon 9500 Secondary" ,"R350", 0x4169},
  { "Radeon 9600 TX" ,"R300", 0x4E46},
  { "Radeon 9600 TX Secondary" ,"R300", 0x4E66},
  { "Radeon 9600TX" ,"R300", 0x4146},
  { "Radeon 9600TX Secondary" ,"R300", 0x4166},
  { "Radeon 9700 PRO" ,"R300", 0x4E44},
  { "Radeon 9700 PRO Secondary" ,"R300", 0x4E64},
  { "Radeon 9800" ,"R350", 0x4E49},
  { "Radeon 9800 PRO" ,"R350", 0x4E48},
  { "Radeon 9800 PRO Secondary" ,"R350", 0x4E68},
  { "Radeon 9800 SE" ,"R350", 0x4148},
  { "Radeon 9800 SE Secondary" ,"R350", 0x4168},
  { "Radeon 9800 Secondary" ,"R350", 0x4E69},
  { "Radeon 9800 XT" ,"R360", 0x4E4A},
  { "Radeon 9800 XT Secondary" ,"R360", 0x4E6A},
  { "Radeon X1300 / X1550 Series" ,"RV515", 0x7146},
  { "Radeon X1300 / X1550 Series Secondary" ,"RV515", 0x7166},
  { "Radeon X1300 Series" ,"RV515PCI", 0x714E},
  { "Radeon X1300 Series" ,"RV515", 0x715E},
  { "Radeon X1300 Series" ,"RV515", 0x714D},
  { "Radeon X1300 Series" ,"RV535", 0x71C3},
  { "Radeon X1300 Series" ,"RV515PCI", 0x718F},
  { "Radeon X1300 Series Secondary" ,"RV515PCI", 0x716E},
  { "Radeon X1300 Series Secondary" ,"RV515", 0x717E},
  { "Radeon X1300 Series Secondary" ,"RV515", 0x716D},
  { "Radeon X1300 Series Secondary" ,"RV535", 0x71E3},
  { "Radeon X1300 Series Secondary" ,"RV515PCI", 0x71AF},
  { "Radeon X1300/X1550 Series" ,"RV515", 0x7142},
  { "Radeon X1300/X1550 Series" ,"RV515", 0x7180},
  { "Radeon X1300/X1550 Series" ,"RV515", 0x7183},
  { "Radeon X1300/X1550 Series" ,"RV515", 0x7187},
  { "Radeon X1300/X1550 Series Secondary" ,"RV515", 0x7162},
  { "Radeon X1300/X1550 Series Secondary" ,"RV515", 0x71A0},
  { "Radeon X1300/X1550 Series Secondary" ,"RV515", 0x71A3},
  { "Radeon X1300/X1550 Series Secondary" ,"RV515", 0x71A7},
  { "Radeon X1550 64-bit" ,"RV515", 0x7147},
  { "Radeon X1550 64-bit" ,"RV515", 0x715F},
  { "Radeon X1550 64-bit" ,"RV515", 0x719F},
  { "Radeon X1550 64-bit Secondary" ,"RV515", 0x7167},
  { "Radeon X1550 64-bit Secondary" ,"RV515", 0x717F},
  { "Radeon X1550 Series" ,"RV515", 0x7143},
  { "Radeon X1550 Series" ,"RV515", 0x7193},
  { "Radeon X1550 Series Secondary" ,"RV515", 0x7163},
  { "Radeon X1550 Series Secondary" ,"RV515", 0x71B3},
  { "Radeon X1600 Pro / Radeon X1300 XT" ,"RV530", 0x71CE},
  { "Radeon X1600 Pro / Radeon X1300 XT Secondary" ,"RV530", 0x71EE},
  { "Radeon X1600 Series" ,"RV515", 0x7140},
  { "Radeon X1600 Series" ,"RV530", 0x71C0},
  { "Radeon X1600 Series" ,"RV530", 0x71C2},
  { "Radeon X1600 Series" ,"RV530", 0x71C6},
  { "Radeon X1600 Series" ,"RV515", 0x7181},
  { "Radeon X1600 Series" ,"RV530", 0x71CD},
  { "Radeon X1600 Series Secondary" ,"RV515", 0x7160},
  { "Radeon X1600 Series Secondary" ,"RV530", 0x71E2},
  { "Radeon X1600 Series Secondary" ,"RV530", 0x71E6},
  { "Radeon X1600 Series Secondary" ,"RV515", 0x71A1},
  { "Radeon X1600 Series Secondary" ,"RV530", 0x71ED},
  { "Radeon X1600 Series Secondary" ,"RV530", 0x71E0},
  { "Radeon X1650 Series" ,"RV535", 0x71C1},
  { "Radeon X1650 Series" ,"R580", 0x7293},
  { "Radeon X1650 Series" ,"R580", 0x7291},
  { "Radeon X1650 Series" ,"RV535", 0x71C7},
  { "Radeon X1650 Series Secondary" ,"RV535", 0x71E1},
  { "Radeon X1650 Series Secondary" ,"R580", 0x72B3},
  { "Radeon X1650 Series Secondary" ,"R580", 0x72B1},
  { "Radeon X1650 Series Secondary" ,"RV535", 0x71E7},
  { "Radeon X1800 Series" ,"R520", 0x7100},
  { "Radeon X1800 Series" ,"R520", 0x7108},
  { "Radeon X1800 Series" ,"R520", 0x7109},
  { "Radeon X1800 Series" ,"R520", 0x710A},
  { "Radeon X1800 Series" ,"R520", 0x710B},
  { "Radeon X1800 Series" ,"R520", 0x710C},
  { "Radeon X1800 Series Secondary" ,"R520", 0x7120},
  { "Radeon X1800 Series Secondary" ,"R520", 0x7128},
  { "Radeon X1800 Series Secondary" ,"R520", 0x7129},
  { "Radeon X1800 Series Secondary" ,"R520", 0x712A},
  { "Radeon X1800 Series Secondary" ,"R520", 0x712B},
  { "Radeon X1800 Series Secondary" ,"R520", 0x712C},
  { "Radeon X1900 Series" ,"R580", 0x7243},
  { "Radeon X1900 Series" ,"R580", 0x7245},
  { "Radeon X1900 Series" ,"R580", 0x7246},
  { "Radeon X1900 Series" ,"R580", 0x7247},
  { "Radeon X1900 Series" ,"R580", 0x7248},
  { "Radeon X1900 Series" ,"R580", 0x7249},
  { "Radeon X1900 Series" ,"R580", 0x724A},
  { "Radeon X1900 Series" ,"R580", 0x724B},
  { "Radeon X1900 Series" ,"R580", 0x724C},
  { "Radeon X1900 Series" ,"R580", 0x724D},
  { "Radeon X1900 Series" ,"R580", 0x724F},
  { "Radeon X1900 Series Secondary" ,"R580", 0x7263},
  { "Radeon X1900 Series Secondary" ,"R580", 0x7265},
  { "Radeon X1900 Series Secondary" ,"R580", 0x7266},
  { "Radeon X1900 Series Secondary" ,"R580", 0x7267},
  { "Radeon X1900 Series Secondary" ,"R580", 0x7268},
  { "Radeon X1900 Series Secondary" ,"R580", 0x7269},
  { "Radeon X1900 Series Secondary" ,"R580", 0x726A},
  { "Radeon X1900 Series Secondary" ,"R580", 0x726B},
  { "Radeon X1900 Series Secondary" ,"R580", 0x726C},
  { "Radeon X1900 Series Secondary" ,"R580", 0x726D},
  { "Radeon X1900 Series Secondary" ,"R580", 0x726F},
  { "Radeon X1950 Series" ,"R580", 0x7280},
  { "Radeon X1950 Series" ,"R580", 0x7240},
  { "Radeon X1950 Series" ,"R580", 0x7244},
  { "Radeon X1950 Series Secondary" ,"R580", 0x72A0},
  { "Radeon X1950 Series Secondary" ,"R580", 0x7260},
  { "Radeon X1950 Series Secondary" ,"R580", 0x7264},
  { "Radeon X300/X550/X1050 Series" ,"RV370", 0x5B60},
  { "Radeon X300/X550/X1050 Series" ,"RV370", 0x5B63},
  { "Radeon X300/X550/X1050 Series Secondary" ,"RV370", 0x5B73},
  { "Radeon X300/X550/X1050 Series Secondary" ,"RV370", 0x5B70},
  { "Radeon X550/X700 Series" ,"RV410", 0x5657},
  { "Radeon X550/X700 Series Secondary" ,"RV410", 0x5677},
  { "Radeon X600 Series" ,"RV380x", 0x5B62},
  { "Radeon X600 Series Secondary" ,"RV380x", 0x5B72},
  { "Radeon X600/X550 Series" ,"RV380", 0x3E50},
  { "Radeon X600/X550 Series Secondary" ,"RV380", 0x3E70},
  { "Radeon X700" ,"RV410", 0x5E4D},
  { "Radeon X700 PRO" ,"RV410", 0x5E4B},
  { "Radeon X700 PRO Secondary" ,"RV410", 0x5E6B},
  { "Radeon X700 SE" ,"RV410", 0x5E4C},
  { "Radeon X700 SE Secondary" ,"RV410", 0x5E6C},
  { "Radeon X700 Secondary" ,"RV410", 0x5E6D},
  { "Radeon X700 XT" ,"RV410", 0x5E4A},
  { "Radeon X700 XT Secondary" ,"RV410", 0x5E6A},
  { "Radeon X700/X550 Series" ,"RV410", 0x5E4F},
  { "Radeon X700/X550 Series Secondary" ,"RV410", 0x5E6F},
  { "Radeon X800 GT" ,"R423", 0x554B},
  { "Radeon X800 GT Secondary" ,"R423", 0x556B},
  { "Radeon X800 GTO" ,"R423", 0x5549},
  { "Radeon X800 GTO" ,"R430", 0x554F},
  { "Radeon X800 GTO" ,"R480", 0x5D4F},
  { "Radeon X800 GTO Secondary" ,"R423", 0x5569},
  { "Radeon X800 GTO Secondary" ,"R430", 0x556F},
  { "Radeon X800 GTO Secondary" ,"R480", 0x5D6F},
  { "Radeon X800 PRO" ,"R420", 0x4A49},
  { "Radeon X800 PRO Secondary" ,"R420", 0x4A69},
  { "Radeon X800 SE" ,"R420", 0x4A4F},
  { "Radeon X800 SE Secondary" ,"R420", 0x4A6F},
  { "Radeon X800 Series" ,"R420", 0x4A48},
  { "Radeon X800 Series" ,"R420", 0x4A4A},
  { "Radeon X800 Series" ,"R420", 0x4A4C},
  { "Radeon X800 Series" ,"R423", 0x5548},
  { "Radeon X800 Series Secondary" ,"R420", 0x4A68},
  { "Radeon X800 Series Secondary" ,"R420", 0x4A6A},
  { "Radeon X800 Series Secondary" ,"R420", 0x4A6C},
  { "Radeon X800 Series Secondary" ,"R423", 0x5568},
  { "Radeon X800 VE" ,"R420", 0x4A54},
  { "Radeon X800 VE Secondary" ,"R420", 0x4A74},
  { "Radeon X800 XT" ,"R420", 0x4A4B},
  { "Radeon X800 XT" ,"R423", 0x5D57},
  { "Radeon X800 XT Platinum Edition" ,"R420", 0x4A50},
  { "Radeon X800 XT Platinum Edition" ,"R423", 0x554A},
  { "Radeon X800 XT Platinum Edition Secondary" ,"R420", 0x4A70},
  { "Radeon X800 XT Platinum Edition Secondary" ,"R423", 0x556A},
  { "Radeon X800 XT Secondary" ,"R420", 0x4A6B},
  { "Radeon X800 XT Secondary" ,"R423", 0x5D77},
  { "Radeon X850 XT" ,"R480", 0x5D52},
  { "Radeon X850 XT Platinum Edition" ,"R480", 0x5D4D},
  { "Radeon X850 XT Platinum Edition Secondary" ,"R480", 0x5D6D},
  { "Radeon X850 XT Secondary" ,"R480", 0x5D72},
  { NULL ,NULL, 0}
  // End of list
};

HRESULT CALLBACK CD3D9Renderer::OnD3D9CreateDevice(LPDIRECT3DDEVICE9 pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
  int i;

  //assert (DXUTIsCurrentDeviceD3D9());

  CD3D9Renderer *rd = gcpRendD3D;
  rd->m_pd3dDevice = pd3dDevice;
  const D3DCAPS9* pd3dCaps = rd->m_pd3dCaps;

  // Gamma correction support
  if (pd3dCaps->Caps2 & D3DCAPS2_FULLSCREENGAMMA)
    rd->m_Features |= RFT_HWGAMMA;

  IDirect3D9* pD3D = rd->m_pD3D;
#if !defined(XENON) && !defined(PS3)
  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
  CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration();
  CD3D9EnumDeviceSettingsCombo* pDeviceCombo = pd3dEnum->GetDeviceSettingsCombo(&pDev->d3d9);
  CD3D9EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo(pDev->d3d9.AdapterOrdinal);
#else
  CD3DSettings *pDev = &rd->m_D3DSettings;
  D3DDeviceCombo* pDeviceCombo = pDev->PDeviceCombo();
  D3DAdapterInfo* pAdapterInfo = pDev->PAdapterInfo();
#endif
  D3DADAPTER_IDENTIFIER9 *ai = NULL;

	D3DADAPTER_IDENTIFIER9 adapter_info;
	if (pAdapterInfo)
		ai = &pAdapterInfo->AdapterIdentifier;
	else
	{
		ai = &adapter_info;
		memset(ai,0,sizeof(adapter_info));
		strcpy( ai->Description,"NULL REF" );
		strcpy( ai->DeviceName,"NULL REF" );
		strcpy( ai->Driver,"NULL REF" );
	}

  // Device Id's
  iLog->Log ( "D3D Adapter: Driver name: %s", ai->Driver);

  iLog->Log ( "D3D Adapter: Driver description: %s", ai->Description);
#if !defined(PS3) && !defined(LINUX)
  iLog->Log ( "D3D Adapter: Driver version: %d.%02d.%02d.%04d", HIWORD( ai->DriverVersion.u.HighPart ), LOWORD( ai->DriverVersion.u.HighPart ), HIWORD(ai->DriverVersion.u.LowPart), LOWORD(ai->DriverVersion.u.LowPart));
  // Unique driver/device identifier:
  GUID *pGUID = &ai->DeviceIdentifier;
  iLog->Log ( "D3D Adapter: Driver GUID: %08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X", pGUID->Data1, pGUID->Data2, pGUID->Data3, pGUID->Data4[0], pGUID->Data4[1], pGUID->Data4[2], pGUID->Data4[3], pGUID->Data4[4], pGUID->Data4[5], pGUID->Data4[6], pGUID->Data4[7] );
#endif
  iLog->Log ( "D3D Adapter: VendorId = 0x%.4X", ai->VendorId);
  iLog->Log ( "D3D Adapter: DeviceId = 0x%.4X", ai->DeviceId);
  iLog->Log ( "D3D Adapter: SubSysId = 0x%.8X", ai->SubSysId);
  iLog->Log ( "D3D Adapter: Revision = %i", ai->Revision);
  
  // Vendor-specific initializations and workarounds for driver bugs.
  {
    bool ConstrainAspect = 1;
    if(ai->VendorId==4098)
    {
			// ATI
			rd->InitATIAPI();

			rd->m_Features &= ~RFT_HW_MASK;
      rd->m_Features |= RFT_HW_ATI;
      iLog->Log ("D3D Detected: ATI video card");
      SATIDevice *pDev = s_ATIDevList;
      while (pDev->nDevID)
      {
        if (ai->DeviceId == pDev->nDevID)
        {
          iLog->Log ("D3D Device: '%s' (ASIC: '%s')", pDev->szDescr, pDev->szASIC);
          break;
        }
        pDev++;
      }
      if (pDev->nDevID == 0)
        iLog->Log ("D3D Device: '%s'", "Unknown NVidia card");
    }
    else
    if( ai->VendorId==32902 )
    {
      iLog->Log ("D3D Detected: Intel video card");
    }
    else
    if( ai->VendorId==4318 )
    {
			rd->m_Features &= ~RFT_HW_MASK;
			rd->m_Features |= RFT_HW_NVIDIA;
			rd->InitNVAPI();

			iLog->Log ("D3D Detected: NVidia video card");
      SNVDevice *pDev = s_NVDevList;
      while (pDev->nDevID)
      {
        if (ai->DeviceId == pDev->nDevID)
        {
          iLog->Log ("D3D Device: '%s'", pDev->szDescr);
          break;
        }
        pDev++;
      }
      if (pDev->nDevID == 0)
        iLog->Log ("D3D Device: '%s'", "Unknown NVidia card");
    }
    else
    if( ai->VendorId==4139 )
    {
      iLog->Log ("D3D Detected: Matrox video card");
    }
    else
    if( ai->VendorId==0x18ca )
    {
      iLog->Log ("D3D Detected: XGI video card");
    }
    else
    {
      iLog->Log ("D3D Detected: Generic 3D accelerator");
    }
  }

  rd->m_RP.m_nMaxLightsPerPass = 1;

  // Set viewport.
  rd->SetViewport(0, 0, rd->GetWidth(), rd->GetHeight());

  // Check multitexture caps.
  iLog->Log ("D3D Driver: FFP MaxTextureBlendStages   = %i", pd3dCaps->MaxTextureBlendStages);
  iLog->Log ("D3D Driver: FFP MaxSimultaneousTextures = %i", pd3dCaps->MaxSimultaneousTextures);
  if( pd3dCaps->MaxSimultaneousTextures>=2 )
    rd->m_Features |= RFT_MULTITEXTURE;
  rd->m_numtmus = 16; //di->Caps.MaxSimultaneousTextures;

  // Handle the texture formats we need.
  {
    // Find the needed texture formats.
    {
      rd->m_FirstPixelFormat = NULL;

      // RGB uncompressed unsigned formats
      rd->m_FormatA8R8G8B8.CheckSupport(D3DFMT_A8R8G8B8, "A8R8G8B8");
      rd->m_FormatX8R8G8B8.CheckSupport(D3DFMT_X8R8G8B8, "X8R8G8B8");
#if !defined(XENON)
      rd->m_FormatR8G8B8.CheckSupport(D3DFMT_R8G8B8, "R8G8B8");
#endif

      rd->m_FormatA4R4G4B4.CheckSupport(D3DFMT_A4R4G4B4, "A4R4G4B4");
      rd->m_FormatR5G6B5.CheckSupport(D3DFMT_R5G6B5, "R5G6B5");

      // Alpha formats
      rd->m_FormatA8.CheckSupport(D3DFMT_A8, "A8");
      rd->m_FormatA8L8.CheckSupport(D3DFMT_A8L8, "A8L8");
      rd->m_FormatL8.CheckSupport(D3DFMT_L8, "L8");

      // Fat RGB uncompressed unsigned formats
      rd->m_FormatA16B16G16R16F.CheckSupport(D3DFMT_A16B16G16R16F, "A16B16G16R16F");
      rd->m_FormatA16B16G16R16.CheckSupport(D3DFMT_A16B16G16R16, "A16B16G16R16");
      rd->m_FormatG16R16F.CheckSupport(D3DFMT_G16R16F, "G16R16F");
      rd->m_FormatG16R16.CheckSupport(D3DFMT_G16R16, "G16R16");
      rd->m_FormatR16F.CheckSupport(D3DFMT_R16F, "R16F");
      rd->m_FormatR32F.CheckSupport(D3DFMT_R32F, "R32F");
      rd->m_FormatA32B32G32R32F.CheckSupport(D3DFMT_A32B32G32R32F, "A32B32G32R32F");

      // RGB compressed unsigned formats
      rd->m_FormatDXT1.CheckSupport(D3DFMT_DXT1, "DXT1");
      rd->m_FormatDXT3.CheckSupport(D3DFMT_DXT3, "DXT3");
      rd->m_FormatDXT5.CheckSupport(D3DFMT_DXT5, "DXT5");
      rd->m_Format3Dc.CheckSupport(D3DFMT_3DC, "3DC");

      // Depth formats
      rd->m_FormatDepth24.CheckSupport(D3DFMT_D24S8, "D24S8");
      rd->m_FormatDepth16.CheckSupport(D3DFMT_D16, "D16");

			//ATI DST formats
      {
        //rd->m_FormatDF16.CheckSupport(D3DFMT_DF16, "DF16");
        //rd->m_bDeviceSupports_ATI_DF16 = rd->m_FormatDF16.IsValid();
        rd->m_FormatDF24.CheckSupport(D3DFMT_DF24, "DF24", eTXUSAGE_DEPTHSTENCIL);
        rd->m_bDeviceSupports_ATI_DF24 = rd->m_FormatDF24.IsValid();

        rd->m_FormatDF16.CheckSupport(D3DFMT_DF16, "DF16", eTXUSAGE_DEPTHSTENCIL);
      }

      //NVIDIA NULL Format
      {
         rd->m_FormatNULL.CheckSupport(D3DFMT_NULL, "NULL", eTXUSAGE_RENDERTARGET);
      }

      //NVIDIA native depth buffers
      {
        rd->m_FormatD16.CheckSupport(D3DFMT_D16, "D16", eTXUSAGE_DEPTHSTENCIL);
        rd->m_FormatD24S8.CheckSupport(D3DFMT_D24S8, "D24S8", eTXUSAGE_DEPTHSTENCIL);
      }

      //NVIDIA DBT
      {
        rd->m_bDeviceSupports_NVDBT = SUCCEEDED( pD3D->CheckDeviceFormat( pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, 0, D3DRTYPE_SURFACE, (D3DFORMAT)MAKEFOURCC('N','V','D','B') ) );
      }



      if (CD3D9Renderer::CV_d3d9_forcesoftware || rd->m_FormatDepth16.IsValid() || rd->m_FormatDepth24.IsValid())
        rd->m_Features |= RFT_DEPTHMAPS;

      if (rd->m_Format3Dc.IsValid())
       rd->m_iDeviceSupportsComprNormalmaps = 1;

      if (rd->m_FormatDXT1.IsValid() || rd->m_FormatDXT3.IsValid() || rd->m_FormatDXT5.IsValid())
        rd->m_Features |= RFT_COMPRESSTEXTURE;
    }
  }

  // Verify mipmapping supported.
  if (!(pd3dCaps->TextureCaps & D3DPTEXTURECAPS_MIPMAP))
  {
    iLog->Log ("D3D Driver: Mipmapping not available with this driver");
    //rd->m_Features |= RFT_NOMIPS;
  }
  else
  {
    if(pd3dCaps->TextureFilterCaps & D3DPTFILTERCAPS_MIPFLINEAR)
      iLog->Log ("D3D Driver: Supports trilinear texture filtering");
  }

  if (!(pd3dCaps->TextureCaps & D3DPTEXTURECAPS_NOPROJECTEDBUMPENV))
  {
    iLog->Log ("D3D Driver: Allowed projected textures with Env. bump mapping");
    rd->m_Features |= RFT_HW_ENVBUMPPROJECTED;
  }
  else
    iLog->Log ("D3D Driver: projected textures with Env. bump mapping not allowed");

  if(pd3dCaps->TextureCaps & D3DPTEXTURECAPS_SQUAREONLY)
    iLog->Log ("D3D Driver: Requires square textures");
  if(pd3dCaps->RasterCaps & D3DPRASTERCAPS_FOGRANGE)
    iLog->Log ("D3D Driver: Supports range-based fog");
  if(pd3dCaps->RasterCaps & D3DPRASTERCAPS_WFOG )
    iLog->Log ("D3D Driver: Supports eye-relative fog");
  if(pd3dCaps->RasterCaps & D3DPRASTERCAPS_ANISOTROPY )
    iLog->Log ("D3D Driver: Supports anisotropic filtering");
  if(pd3dCaps->RasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS )
    iLog->Log ("D3D Driver: Supports LOD biasing");
  if(pd3dCaps->RasterCaps & D3DPRASTERCAPS_DEPTHBIAS )
    iLog->Log ("D3D Driver: Supports Z biasing");
  else
    rd->m_Features &= ~RFT_SUPPORTZBIAS;
  if(pd3dCaps->RasterCaps & D3DPRASTERCAPS_ZBUFFERLESSHSR )
    iLog->Log ("D3D Driver: Device can perform hidden-surface removal (HSR)");
  if(pd3dCaps->RasterCaps & D3DPRASTERCAPS_SCISSORTEST )
    iLog->Log ("D3D Driver: Supports scissor test");
  if (pd3dCaps->DevCaps2 & D3DDEVCAPS2_STREAMOFFSET)
    iLog->Log ("D3D Driver: Supports stream offset");

  if (!(pd3dCaps->TextureCaps & D3DPTEXTURECAPS_POW2))
    iLog->Log ("D3D Driver: Supports non-power-of-2 textures");
  if (pd3dCaps->TextureOpCaps & D3DTEXOPCAPS_ADDSIGNED2X)
    iLog->Log ("D3D Driver: Supports D3DTOP_ADDSIGNED2X TextureOp");
  if (pd3dCaps->TextureOpCaps & D3DTEXOPCAPS_BUMPENVMAP)
    iLog->Log ("D3D Driver: Supports D3DTOP_BUMPENVMAP TextureOp");
  if (pd3dCaps->TextureOpCaps & D3DTEXOPCAPS_BUMPENVMAPLUMINANCE)
    iLog->Log ("D3D Driver: Supports D3DTOP_BUMPENVMAPLUMINANCE TextureOp");
  if (pd3dCaps->TextureOpCaps & D3DTEXOPCAPS_DOTPRODUCT3)
    iLog->Log ("D3D Driver: Supports D3DTOP_DOTPRODUCT3 TextureOp");
  if (pd3dCaps->TextureOpCaps & D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR)
    iLog->Log ("D3D Driver: Supports D3DTOP_MODULATEALPHA_ADDCOLOR TextureOp");
  if (pd3dCaps->TextureOpCaps & D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA)
    iLog->Log ("D3D Driver: Supports D3DTOP_MODULATECOLOR_ADDALPHA TextureOp"); 
  if (pd3dCaps->TextureOpCaps & D3DTEXOPCAPS_ADD)
    iLog->Log ("D3D Driver: Supports D3DTOP_ADD TextureOp"); 

  rd->m_MaxAnisotropyLevel = 1;
  // Check the device for supported filtering methods
  if( pd3dCaps->RasterCaps & D3DPRASTERCAPS_ANISOTROPY )
  {
    rd->m_MaxAnisotropyLevel = pd3dCaps->MaxAnisotropy;
    iLog->Log ("D3D Driver: Supports MaxAnisotropy level %d", rd->m_MaxAnisotropyLevel); 
    rd->m_Features |= RFT_ALLOWANISOTROPIC;
  }
  rd->m_MaxAnisotropyLevel = min(rd->m_MaxAnisotropyLevel, CRenderer::CV_r_texmaxanisotropy);

  if(pd3dCaps->StencilCaps & (D3DSTENCILCAPS_KEEP|D3DSTENCILCAPS_INCR|D3DSTENCILCAPS_DECR))
  {
    iLog->Log ("D3D Driver: Supports Stencil shadows"); 
    if(pd3dCaps->StencilCaps & D3DSTENCILCAPS_TWOSIDED)
      iLog->Log ("D3D Driver: Supports Two-Sided stencil"); 
  }

  iLog->Log ("D3D Driver: Textures (%ix%i)-(%ix%i), Max aspect %i", 8, 8, pd3dCaps->MaxTextureWidth, pd3dCaps->MaxTextureHeight, pd3dCaps->MaxTextureAspectRatio);

  // Init render states.
#if !defined(XENON) && !defined(PS3)
  {
    rd->m_RP.m_eCull = eCULL_None;
    pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    pd3dDevice->SetRenderState(D3DRS_ALPHAREF, 0);
    pd3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    pd3dDevice->SetRenderState(D3DRS_FILLMODE,  D3DFILL_SOLID);
    pd3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    pd3dDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    pd3dDevice->SetRenderState(D3DRS_DITHERENABLE, TRUE);
    pd3dDevice->SetPixelShader(NULL);
    rd->m_FS.m_bEnable = false;

    // Set default stencil states
    rd->m_RP.m_CurStencilState = 0;
    rd->m_RP.m_CurStencMask = 0xffffffff;
    rd->m_RP.m_CurStencWriteMask = 0xFFFFFFFF;
    rd->m_RP.m_CurStencRef = 0;
    pd3dDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    if (pd3dCaps->StencilCaps & D3DSTENCILCAPS_TWOSIDED)
    {
#ifndef PS3
      pd3dDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
#endif
      pd3dDevice->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP);
      pd3dDevice->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_KEEP);
      pd3dDevice->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP);
      pd3dDevice->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS);
    }
    pd3dDevice->SetRenderState(D3DRS_STENCILMASK, 0xffffffff);
    pd3dDevice->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
    pd3dDevice->SetRenderState(D3DRS_STENCILREF, 0);

    pd3dDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    pd3dDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
    pd3dDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    pd3dDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
  }
#endif

  rd->msCurState = GS_DEPTHWRITE | GS_NODEPTHTEST;
  for (i=0; i<MAX_TMU; i++)
  {
    rd->m_eCurColorOp[i] = eCO_DISABLE;
    rd->m_eCurColorArg[i] = 255;
    rd->m_eCurAlphaArg[i] = 255;
  }

#if !defined(XENON) && !defined(PS3)
  // Init texture stage state.
  {
    //FLOAT LodBias=-0.5;
    //pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)&LodBias );
    pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU,   D3DTADDRESS_WRAP);
    pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV,   D3DTADDRESS_WRAP);
    rd->EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
    pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    CTexture::m_TexStages[0].m_State.m_nMinFilter = D3DTEXF_LINEAR;
    CTexture::m_TexStages[0].m_State.m_nMagFilter = D3DTEXF_LINEAR;
    CTexture::m_TexStages[0].m_State.m_nMipFilter = D3DTEXF_LINEAR;

    {
      if(rd->m_Features & RFT_MULTITEXTURE)
      {
        for( i=1; i<rd->m_numtmus; i++ )
        {
          CTexture::m_CurStage = i;
          
          // Set stage i state.
          pd3dDevice->SetSamplerState(i, D3DSAMP_ADDRESSU,  D3DTADDRESS_WRAP);
          pd3dDevice->SetSamplerState(i, D3DSAMP_ADDRESSV,  D3DTADDRESS_WRAP);
          rd->EF_SetColorOp(eCO_DISABLE, eCO_DISABLE, DEF_TEXARG1, DEF_TEXARG1);
          pd3dDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
          pd3dDevice->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
          pd3dDevice->SetSamplerState(i, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
          CTexture::m_TexStages[i].m_State.m_nMinFilter = D3DTEXF_LINEAR;
          CTexture::m_TexStages[i].m_State.m_nMagFilter = D3DTEXF_LINEAR;
          CTexture::m_TexStages[i].m_State.m_nMipFilter = D3DTEXF_LINEAR;
        }
      }
    }
  }
#endif

  // Check to see if device supports visibility query
  IDirect3DQuery9 *pOcclQuery = 0;;
  if (D3DERR_NOTAVAILABLE == pd3dDevice->CreateQuery (D3DQUERYTYPE_OCCLUSION, &pOcclQuery))
    rd->m_Features &= ~RFT_OCCLUSIONTEST;
  else
  {
    rd->m_Features |= RFT_OCCLUSIONTEST;
    pOcclQuery->Release();
  }

  CTexture::m_CurStage = 0;
  CTexture::SetDefTexFilter(CV_d3d9_texturefilter->GetString());

  // For safety, lots of drivers don't handle tiny texture sizes too tell.
#if defined(WIN32) || defined(WIN64)
	typedef HRESULT (WINAPI *FP_DirectDrawCreate)(GUID FAR*, LPDIRECTDRAW FAR* , IUnknown*);		
	FP_DirectDrawCreate pddc((FP_DirectDrawCreate) GetProcAddress(LoadLibrary("ddraw.dll"), "DirectDrawCreate"));		
	IDirectDraw* pDD(0);
	if (pddc)
		pddc(0, &pDD, 0);
	rd->m_MaxTextureMemory = 0;
	if (pDD)
	{
		IDirectDraw7* pDD7(0);
		if (SUCCEEDED(pDD->QueryInterface(IID_IDirectDraw7, (void**) &pDD7)) && pDD7)
		{
			DDSCAPS2 sDDSCaps2;
			ZeroMemory(&sDDSCaps2, sizeof(sDDSCaps2));
			sDDSCaps2.dwCaps = DDSCAPS_LOCALVIDMEM;

			unsigned long localMem(0);
			pDD7->GetAvailableVidMem(&sDDSCaps2, &localMem, 0);
			SAFE_RELEASE(pDD7);
			rd->m_MaxTextureMemory = localMem;
		}
		SAFE_RELEASE(pDD);
	}
#else
	rd->m_MaxTextureMemory = pd3dDevice->GetAvailableTextureMem();
#endif
  if (CRenderer::CV_r_texturesstreampoolsize <= 0)
    CRenderer::CV_r_texturesstreampoolsize = (int)(rd->m_MaxTextureMemory/1024.0f/1024.0f*0.75f);

  rd->m_MaxTextureSize = min(pd3dCaps->MaxTextureHeight, pd3dCaps->MaxTextureWidth); //min(wdt, hgt);

  rd->m_Features &= ~RFT_DEPTHMAPS;
  if (pd3dCaps->MaxSimultaneousTextures <= 2)
  {
    rd->m_Features &= ~RFT_HW_MASK;
    rd->m_Features |= RFT_HW_GF2;
  }
  else
  {
    if (pd3dCaps->MaxSimultaneousTextures == 4 && D3DSHADER_VERSION_MAJOR(pd3dCaps->PixelShaderVersion) == 1)
    {
      rd->m_Features &= ~RFT_HW_MASK;
      rd->m_Features |= RFT_HW_GF3;
      if (ai->VendorId==4318)
        rd->m_Features |= RFT_DEPTHMAPS;
    }
    else
    if( ai->VendorId==4318 )		// NVIDIA
    {
      if (pd3dCaps->MaxSimultaneousTextures >= 8 && (D3DSHADER_VERSION_MAJOR(pd3dCaps->PixelShaderVersion) >= 3 || (D3DSHADER_VERSION_MAJOR(pd3dCaps->PixelShaderVersion) >= 2 && D3DSHADER_VERSION_MINOR(pd3dCaps->PixelShaderVersion) >= 1)))
      {
        rd->m_Features &= ~RFT_HW_MASK;
        rd->m_Features |= RFT_HW_NV4X;
      }
      else
      if (pd3dCaps->MaxSimultaneousTextures >= 8 && D3DSHADER_VERSION_MAJOR(pd3dCaps->PixelShaderVersion) >= 2)
      {
        rd->m_Features &= ~RFT_HW_MASK;
        rd->m_Features |= RFT_HW_GFFX;
      }
      else
      {
        rd->m_Features &= ~RFT_HW_MASK;
        rd->m_Features |= RFT_HW_GF3;
      }
      rd->m_Features |= RFT_DEPTHMAPS;
    }
    else
    if (!(rd->m_Features & RFT_HW_MASK))
      rd->m_Features |= RFT_HW_GF3;
  }

	rd->m_bDeviceSupportsR16FRendertarget = SUCCEEDED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_R16F));
  
  rd->m_bDeviceSupportsATOC = SUCCEEDED(pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pDeviceCombo->AdapterFormat, 0, D3DRTYPE_SURFACE, (D3DFORMAT)MAKEFOURCC('A','T','O','C') ));

  rd->m_bDeviceSupportsVertexTexture = SUCCEEDED(pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pDeviceCombo->AdapterFormat, D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F));
  if( rd->m_bDeviceSupportsVertexTexture)
    rd->m_Features |= RFT_HW_VERTEXTEXTURES;

  rd->m_bDeviceSupportsDynBranching = (D3DSHADER_VERSION_MAJOR(pd3dCaps->VertexShaderVersion) >= 3);


  // Disable dynamic branching for NVidia
  //if (ai->VendorId==4318)
  //  rd->m_bDeviceSupportsDynBranching = false;
  if (!rd->m_bDeviceSupportsDynBranching && CV_r_shadersdynamicbranching)
  {
    iLog->Log("Device doesn't support dynamic branching in shaders (or it's disabled)");
    _SetVar("r_ShadersDynamicBranching", 0);
  }
  rd->m_bUseDynBranching = CV_r_shadersdynamicbranching != 0;
  rd->m_bUseStatBranching = CV_r_shadersstaticbranching != 0;
	rd->m_bUsePOM = CV_r_usepom != 0;

  DWORD msQuality;
#if !defined(XENON) && !defined(PS3)
	if (pDeviceCombo->DeviceType != D3DDEVTYPE_NULLREF)		// to prevent crash
#endif
	{
		if (SUCCEEDED(pD3D->CheckDeviceMultiSampleType( pDeviceCombo->AdapterOrdinal, 
			pDeviceCombo->DeviceType, D3DFMT_G16R16F, 
			pDeviceCombo->Windowed, D3DMULTISAMPLE_4_SAMPLES, &msQuality ) ) )
		{
			rd->m_bDeviceSupports_G16R16_FSAA = true;
		}
		if (SUCCEEDED(pD3D->CheckDeviceMultiSampleType( pDeviceCombo->AdapterOrdinal, 
			pDeviceCombo->DeviceType, D3DFMT_A16B16G16R16F, 
			pDeviceCombo->Windowed, D3DMULTISAMPLE_4_SAMPLES, &msQuality ) ) )
		{
			rd->m_bDeviceSupports_A16B16G16R16_FSAA = true;
		}
	}

  rd->m_bDeviceSupportsInstancing = (D3DSHADER_VERSION_MAJOR(pd3dCaps->VertexShaderVersion) >= 3);
  if (!rd->m_bDeviceSupportsInstancing)
  {
    D3DFORMAT instanceSupport = (D3DFORMAT)MAKEFOURCC('I', 'N', 'S', 'T');
    if(SUCCEEDED(pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_SURFACE, instanceSupport)))
    {
      rd->m_bDeviceSupportsInstancing = 2;
      // Notify the driver that instancing support is expected
      pd3dDevice->SetRenderState(D3DRS_POINTSIZE, instanceSupport);
    }
  }
  rd->m_bDeviceSupportsFP16Separate = (pd3dCaps->NumSimultaneousRTs>=4) && SUCCEEDED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_G16R16F));
  // Don't use separate bit planes anymore
  rd->m_bDeviceSupportsFP16Separate = false;

  rd->m_bDeviceSupportsFP16Filter = true;

  rd->m_bDeviceSupportsG16R16Filter = false;
  int nHDRSupported = 1;
  rd->m_HDR_FloatFormat_Scalar = eTF_R16F;
  rd->m_HDR_FloatFormat = eTF_A16B16G16R16F;

  if(FAILED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_FILTER, D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F)))
    rd->m_bDeviceSupportsFP16Filter = false;
  if(FAILED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_FILTER, D3DRTYPE_TEXTURE, D3DFMT_G16R16F)))
    rd->m_bDeviceSupportsFP16Filter = false;

  if(SUCCEEDED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_FILTER, D3DRTYPE_TEXTURE, D3DFMT_G16R16)))
    rd->m_bDeviceSupportsG16R16Filter = true;


  {
    // Check support for HDR rendering
#if defined(PS3)
    rd->m_HDR_FloatFormat_Scalar = eTF_A16B16G16R16F;
#else
		if (rd->m_FormatR32F.IsValid())
      rd->m_HDR_FloatFormat_Scalar = eTF_R32F;
		else
			rd->m_HDR_FloatFormat_Scalar = eTF_R16F;
#endif
    if(FAILED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DRTYPE_SURFACE, D3DFMT_A16B16G16R16F)))
      nHDRSupported = 0;
    else
    if(FAILED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_R16F)))
    {
      rd->m_HDR_FloatFormat_Scalar = eTF_R32F;
      if(FAILED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_R32F)))
        nHDRSupported = 0;
    }

    if (!nHDRSupported && pd3dCaps->NumSimultaneousRTs>=2 && (pd3dCaps->PrimitiveMiscCaps & D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING) && D3DSHADER_VERSION_MAJOR(pd3dCaps->PixelShaderVersion) >= 2)
    {
      nHDRSupported = 2;
      rd->m_bDeviceSupportsFP16Separate = false;
      if(FAILED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F)))
        nHDRSupported = 3;
      else
      if(FAILED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_R16F)))
      {
        rd->m_HDR_FloatFormat_Scalar = eTF_R32F;
        if(FAILED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_R32F)))
          nHDRSupported = 3;
      }
    }
  }
#ifdef XENON
  if (SUCCEEDED(pD3D->CheckDeviceFormat(pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat, D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DRTYPE_SURFACE, D3DFMT_A2B10G10R10F_EDRAM)))
  {
    nHDRSupported = 1;
    rd->m_HDR_FloatFormat_Scalar = eTF_A16B16G16R16F;
  }
#endif

  if (!nHDRSupported)
  {
#ifdef USE_HDR
    CV_r_hdrrendering = 0;
#endif
    rd->m_Features &= ~RFT_HW_HDR;
  }
  else
    rd->m_Features |= RFT_HW_HDR;

	iLog->Log("  Renderer HDR_Scalar: %s",CTexture::NameForTextureFormat(rd->m_HDR_FloatFormat_Scalar));
  
  rd->m_nHDRType = nHDRSupported;
  _SetVar("r_HDRType", rd->m_nHDRType);

  rd->m_DepthBufferOrig.pSurf = rd->m_pZBuffer;
  rd->m_DepthBufferOrig.nWidth = rd->m_d3dsdBackBuffer.Width;
  rd->m_DepthBufferOrig.nHeight = rd->m_d3dsdBackBuffer.Height;
  rd->m_DepthBufferOrig.bBusy = true;
  rd->m_DepthBufferOrig.nFrameAccess = -2;

  rd->m_DepthBufferOrigFSAA.pSurf = rd->m_pZBuffer;
  rd->m_DepthBufferOrigFSAA.nWidth = rd->m_d3dsdBackBuffer.Width;
  rd->m_DepthBufferOrigFSAA.nHeight = rd->m_d3dsdBackBuffer.Height;
  rd->m_DepthBufferOrigFSAA.bBusy = true;
  rd->m_DepthBufferOrigFSAA.nFrameAccess = -2;

#if !defined(XENON) && !defined(PS3)
  pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0, 1.0f, 0);
#endif

	for(TListRendererEventListeners::iterator iter=rd->m_listRendererEventListeners.begin(); iter!=rd->m_listRendererEventListeners.end(); ++iter)
	{
		(*iter)->OnPostCreateDevice();
	}

  return S_OK;
}

inline bool CompareTexs(const CTexture *a, const CTexture *b)
{
  return (a->GetPriority() > b->GetPriority());
}

//--------------------------------------------------------------------------------------
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK CD3D9Renderer::OnD3D9ResetDevice(LPDIRECT3DDEVICE9 pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
  HRESULT hr = S_OK;
  uint i, j;

  //InitTexFillers();
  //Flush();

  //iLog->Log("...RestoreDeviceObjects");
  CD3D9Renderer *rd = gcpRendD3D;

  pd3dDevice->GetDepthStencilSurface(&rd->m_pZBuffer);
	if (rd->m_pZBuffer)
	{
		D3DSURFACE_DESC Desc;
		rd->m_pZBuffer->GetDesc(&Desc);
		rd->m_ZFormat = Desc.Format;
	}
	else
		rd->m_ZFormat = D3DFMT_UNKNOWN;

#ifdef XENON
  // Setup predicated tiling rendering

  // Destroy frontbuffer textures.
  // Setup the presentation parameters
  CONST TILING_SCENARIO& CurrentScenario = rd->m_pTilingScenarios[rd->m_dwTilingScenarioIndex];
  SetupPresentationParameters(CurrentScenario, rd->m_d3dpp);

  rd->m_d3dsdBackBuffer.Width = rd->m_d3dpp.BackBufferWidth;
  rd->m_d3dsdBackBuffer.Height = rd->m_d3dpp.BackBufferHeight;
  rd->m_d3dsdBackBuffer.Format = D3DFMT_A8R8G8B8;

  // Set up tiling front buffer textures.
  // These are the size of your desired rendering surface (in this case, the whole
  // screen).
  // Predicated Tiling will resolve to these textures after each tile is rendered.
  // We need to create 2 front buffers to avoid visual tearing.
  for (i=0; i<2; i++)
  {
    D3DTexture *pFrontTexture = (D3DTexture *)CTexture::m_FrontTexture[i].m_pDeviceTexture;
    SAFE_RELEASE(pFrontTexture);
    rd->m_pd3dDevice->CreateTexture(rd->m_d3dpp.BackBufferWidth, rd->m_d3dpp.BackBufferHeight, 1, 0, D3DFMT_LE_X8R8G8B8, D3DPOOL_DEFAULT, &pFrontTexture, NULL);
    CTexture::m_FrontTexture[i].m_pDeviceTexture = pFrontTexture;
    CTexture::m_FrontTexture[i].m_nWidth = rd->m_d3dpp.BackBufferWidth;
    CTexture::m_FrontTexture[i].m_nHeight = rd->m_d3dpp.BackBufferHeight;
    CTexture::m_FrontTexture[i].ClosestFormatSupported(eTF_A8R8G8B8);
    assert(CTexture::m_FrontTexture[i].m_pPixelFormat);
    CTexture::m_FrontTexture[i].SetFlags(FT_USAGE_RENDERTARGET);
  }

  CTexture::m_pBackBuffer = &CTexture::m_FrontTexture[0];
  rd->ChangeResolution(rd->m_d3dpp.BackBufferWidth, rd->m_d3dpp.BackBufferHeight, 32, 60, true);
#else
  pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &rd->m_pBackBuffer);
  if (rd->m_pBackBuffer)
    rd->m_pBackBuffer->GetDesc(&rd->m_d3dsdBackBuffer);
#endif

  rd->m_nRTStackLevel[0] = 0;
  rd->m_RTStack[0][0].m_pDepth = rd->m_pZBuffer;
  rd->m_RTStack[0][0].m_pSurfDepth = &rd->m_DepthBufferOrig;
  rd->m_RTStack[0][0].m_pTarget = rd->m_pBackBuffer;
  rd->m_RTStack[0][0].m_Width = rd->m_d3dsdBackBuffer.Width;
  rd->m_RTStack[0][0].m_Height = rd->m_d3dsdBackBuffer.Height;
  rd->m_RTStack[0][0].m_pTex = CTexture::m_pBackBuffer;
  //CTexture::m_BackBuffer.SetFlags(FT_USAGE_PREDICATED_TILING);

  for (i=0; i<4; i++)
  {
    rd->m_pNewTarget[i] = &rd->m_RTStack[i][0];
    rd->m_pCurTarget[i] = rd->m_pNewTarget[0]->m_pTex;
  }

  rd->m_DepthBufferOrig.pSurf = rd->m_pZBuffer;
  rd->m_DepthBufferOrig.nWidth = rd->m_d3dsdBackBuffer.Width;
  rd->m_DepthBufferOrig.nHeight = rd->m_d3dsdBackBuffer.Height;
  rd->m_DepthBufferOrig.bBusy = true;
  rd->m_DepthBufferOrig.nFrameAccess = -2;

  rd->m_DepthBufferOrigFSAA.pSurf = rd->m_pZBuffer;
  rd->m_DepthBufferOrigFSAA.nWidth = rd->m_d3dsdBackBuffer.Width;
  rd->m_DepthBufferOrigFSAA.nHeight = rd->m_d3dsdBackBuffer.Height;
  rd->m_DepthBufferOrigFSAA.bBusy = true;
  rd->m_DepthBufferOrigFSAA.nFrameAccess = -2;

  rd->m_nIndsDMesh = 32768;
  rd->m_nIOffsDMesh = rd->m_nIndsDMesh;
  hr = pd3dDevice->CreateIndexBuffer(rd->m_nIndsDMesh*sizeof(short), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &rd->m_pIB, NULL);
  if (FAILED(hr))
    return hr;

  hr = pd3dDevice->CreateQuery(D3DQUERYTYPE_EVENT, &rd->m_pQuery[0]);
  if(hr != D3DERR_NOTAVAILABLE)
  {
    assert(rd->m_pQuery[0]);
    rd->m_pQuery[0]->Issue(D3DISSUE_END);
    hr = pd3dDevice->CreateQuery(D3DQUERYTYPE_EVENT, &rd->m_pQuery[1]);
    rd->m_pQuery[1]->Issue(D3DISSUE_END);
  }
  hr = pd3dDevice->CreateQuery(D3DQUERYTYPE_EVENT, &rd->m_pQueryPerf);
  rd->m_pQueryPerf->Issue(D3DISSUE_END);
  hr = pd3dDevice->CreateQuery(D3DQUERYTYPE_EVENT, &rd->m_pHDRLumQuery);
  if(hr != D3DERR_NOTAVAILABLE)
  {
    assert(rd->m_pHDRLumQuery);
    rd->m_pHDRLumQuery->Issue(D3DISSUE_END);
  }

  for (j=0; j<POOL_MAX; j++)
  {
    int nVertSize;
    int fvf;
    switch (j)
    {
      case POOL_P3F_COL4UB_TEX2F:
      default:
        rd->m_nVertsDMesh[j] = MAX_DYNVB3D_VERTS;
        nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F);
        fvf = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
        break;
      case POOL_P3F_TEX2F:
        rd->m_nVertsDMesh[j] = 16384;
        nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_TEX2F);
        fvf = D3DFVF_XYZ | D3DFVF_TEX1;
        break;
      case POOL_P3F:
        rd->m_nVertsDMesh[j] = 16384;
        nVertSize = sizeof(struct_VERTEX_FORMAT_P3F);
        fvf = 0;
        break;
      case POOL_TRP3F_COL4UB_TEX2F:
        rd->m_nVertsDMesh[j] = 8192;
        nVertSize = sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F);
        fvf = 0;
        break;
      case POOL_TRP3F_TEX2F_TEX3F:
        rd->m_nVertsDMesh[j] = 1024;
        nVertSize = sizeof(struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F);
        fvf = 0;
        break;
      case POOL_P3F_TEX3F:
        rd->m_nVertsDMesh[j] = 4096;
        nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_TEX3F);
        fvf = 0;
        break;
			case POOL_P3F_TEX2F_TEX3F:
				rd->m_nVertsDMesh[j] = 256;
				nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F);
				fvf = 0;
				break;
    }
    rd->m_nOffsDMesh[j] = rd->m_nVertsDMesh[j];
    for (i=0; i<4; i++)
    {
      hr = pd3dDevice->CreateVertexBuffer(rd->m_nVertsDMesh[j]*nVertSize, D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, fvf, D3DPOOL_DEFAULT, &rd->m_pVBAr[j][i], NULL);
      if (FAILED(hr))
        return hr;
    }
    rd->m_pVB[j] = rd->m_pVBAr[j][0];
  }

  rd->EF_Restore();
  
  CCryName Name = CTexture::mfGetClassName();
  SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
  TArray<CTexture *> Texs;
  if (pRL)
  {
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
    {
      CTexture *tp = (CTexture *)itor->second;
      if (!tp || tp->IsNoTexture())
        continue;
      if (!tp->IsNeedRestoring())
        continue;
      Texs.AddElem(tp);
    }
  }
  rd->m_bDeviceLost = false;
  if (Texs.Num())
  {
    std::sort(&Texs[0], &Texs[0]+Texs.Num(), CompareTexs);
    ColorF col = ColorF(0,1,0,0);
    for (i=0; i<Texs.Num(); i++)
    {
      CTexture *tp = Texs[i];
      tp->ResetNeedRestoring();
      tp->Reload(0);
      if (tp->GetTextureType() == eTT_2D)
        tp->Fill(col);
    }
  }

  rd->m_RP.m_eCull = eCULL_None;
  pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

  //m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0xffffffff );
  // ATI instancing
#if !defined(XENON) && !defined(PS3)
  if (rd->m_bDeviceSupportsInstancing == 2)
  {
    // Notify the driver that instancing support is expected
    D3DFORMAT instanceSupport = (D3DFORMAT)MAKEFOURCC('I', 'N', 'S', 'T');
    rd->m_pd3dDevice->SetRenderState(D3DRS_POINTSIZE, instanceSupport);
  }
#endif

  rd->m_pLastVDeclaration = NULL;

  assert(0 != rd->m_pD3DRenderAuxGeom);
  if (FAILED(hr=rd->m_pD3DRenderAuxGeom->RestoreDeviceObjects()))
  {
    return( hr );
  }

  rd->m_FSAA = 0;

	for(TListRendererEventListeners::iterator iter=rd->m_listRendererEventListeners.begin(); iter!=rd->m_listRendererEventListeners.end(); ++iter)
	{
		(*iter)->OnPostResetDevice();
	}

  for (i=0; i<MAX_TMU; i++)
  {
    pd3dDevice->SetSamplerState(i, D3DSAMP_ADDRESSU,  D3DTADDRESS_WRAP);
    pd3dDevice->SetSamplerState(i, D3DSAMP_ADDRESSV,  D3DTADDRESS_WRAP);
    pd3dDevice->SetSamplerState(i, D3DSAMP_ADDRESSW,  D3DTADDRESS_WRAP);
    rd->EF_SetColorOp(eCO_DISABLE, eCO_DISABLE, DEF_TEXARG1, DEF_TEXARG1);
    pd3dDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    pd3dDevice->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    pd3dDevice->SetSamplerState(i, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    CTexture::m_TexStages[i].m_State.m_nMinFilter = D3DTEXF_LINEAR;
    CTexture::m_TexStages[i].m_State.m_nMagFilter = D3DTEXF_LINEAR;
    CTexture::m_TexStages[i].m_State.m_nMipFilter = D3DTEXF_LINEAR;
    CTexture::m_TexStages[i].m_State.m_nAddressU = D3DTADDRESS_WRAP;
    CTexture::m_TexStages[i].m_State.m_nAddressV = D3DTADDRESS_WRAP;
    CTexture::m_TexStages[i].m_State.m_nAddressW = D3DTADDRESS_WRAP;
    CTexture::m_TexStages[i].m_State.m_nAnisotropy = 1;
  }

	CHWShader::m_pCurVS = 0;
	CHWShader::m_pCurPS = 0;
	CTexture::BindNULLFrom(0);

  return S_OK;
}

//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK CD3D9Renderer::OnD3D9LostDevice(void* pUserContext)
{
  uint i = 0;
  uint j;
  HRESULT hr;

  CD3D9Renderer *rd = gcpRendD3D;

#ifndef EXCLUDE_SCALEFORM_SDK
  rd->SF_ResetResources();
#endif

  rd->m_nFrameReset++;
  SAFE_RELEASE(rd->m_pIB);
  SAFE_RELEASE(rd->m_pQuery[0]);
  SAFE_RELEASE(rd->m_pQuery[1]);
  SAFE_RELEASE(rd->m_pQueryPerf);
  SAFE_RELEASE(rd->m_pHDRLumQuery);
  for (j=0; j<POOL_MAX; j++)
  {
    for (i=0; i<4; i++)
    {
      SAFE_RELEASE(rd->m_pVBAr[j][i]);
    }
    rd->m_pVB[j] = NULL;
  }

  SAFE_RELEASE(rd->m_pUnitFrustumVB);
  SAFE_RELEASE(rd->m_pUnitFrustumIB);

  SAFE_RELEASE(rd->m_pUnitSphereVB);
  SAFE_RELEASE(rd->m_pUnitSphereIB);

  rd->EF_Invalidate();

  bool bDL = rd->m_bDeviceLost;
  rd->m_bDeviceLost = false;
  // Unload vertex/index buffers
  CRenderMesh *pRM = CRenderMesh::m_RootGlobal.m_NextGlobal;
  while (pRM != &CRenderMesh::m_RootGlobal)
  {
    CRenderMesh *Next = pRM->m_NextGlobal;
    pRM->Unload(true);
    pRM = Next;
  }

  CRendElement *pRE = CRendElement::m_RootGlobal.m_NextGlobal;
  while (pRE != &CRendElement::m_RootGlobal)
  {
    if( pRE->mfGetType() != eDATA_PostProcess )
      pRE->mfReset();

    pRE = pRE->m_NextGlobal;
  }
  rd->m_bDeviceLost = bDL;

  for (i=0; i<256; i++)
  {
    CHWShader_D3D::m_CurVSParams[i].x = -99999.9f;
    CHWShader_D3D::m_CurVSParams[i].y = -99999.9f;
    CHWShader_D3D::m_CurVSParams[i].z = -99999.9f;
    CHWShader_D3D::m_CurVSParams[i].w = -99999.9f;
  }
  for (i=0; i<64; i++)
  {
    CHWShader_D3D::m_CurPSParams[i].x = -99999.9f;
    CHWShader_D3D::m_CurPSParams[i].y = -99999.9f;
    CHWShader_D3D::m_CurPSParams[i].z = -99999.9f;
    CHWShader_D3D::m_CurPSParams[i].w = -99999.9f;
  }
  CCryName Name = CTexture::mfGetClassName();
  SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
  if (pRL)
  {
    ResourcesMapItor itor;
    for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
    {
      CTexture *tp = (CTexture *)itor->second;
      if (!tp || !tp->GetDeviceTexture() || tp->IsNoTexture() )
        continue;
      IDirect3DTexture9 *pID3DTexture = NULL;
      IDirect3DCubeTexture9 *pID3DCubeTexture = NULL;
      IDirect3DVolumeTexture9 *pID3DVolTexture = NULL;
      LPDIRECT3DSURFACE9 pSurf = NULL;
      D3DSURFACE_DESC Desc;
      if (tp->GetTexType() == eTT_Cube || tp->GetTexType() == eTT_AutoCube)
      {
        pID3DCubeTexture = (IDirect3DCubeTexture9*)tp->GetDeviceTexture();
        hr = pID3DCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)0, 0, &pSurf);
      }
      else
      if (tp->GetTexType() == eTT_2D || tp->GetTexType() == eTT_1D)
      {
        pID3DTexture = (IDirect3DTexture9*)tp->GetDeviceTexture();
        hr = pID3DTexture->GetSurfaceLevel(0, &pSurf);
      }
      else
      if (tp->GetTexType() == eTT_3D)
      {
        pID3DVolTexture = (IDirect3DVolumeTexture9*)tp->GetDeviceTexture();
        //hr = pID3DVolTexture->GetSurfaceLevel(0, &pSurf);
        //assert(0);
      }
      if (!pSurf)
        continue;
      hr = pSurf->GetDesc(&Desc);
      SAFE_RELEASE(pSurf);
      if (Desc.Pool != D3DPOOL_DEFAULT)
        continue;

      tp->ReleaseDeviceTexture(false);
      tp->SetNeedRestoring();
    }
    for (i=0; i<MAX_ENVLIGHTCUBEMAPS; i++)
    {
      SEnvTexture *cur = &CTexture::m_EnvLCMaps[i];
      cur->ReleaseDeviceObjects();
    }
  }
  for (i=0; i<rd->m_TempDepths.Num(); i++)
  {
    SD3DSurface *pS = rd->m_TempDepths[i];
    SAFE_DELETE(pS);
  }
  rd->m_TempDepths.Free();

  SAFE_RELEASE (rd->m_pZBuffer);
  SAFE_RELEASE(rd->m_RP.m_FSAAData.m_pZBuffer);
  SAFE_RELEASE (rd->m_pBackBuffer);

  assert( 0 != rd->m_pD3DRenderAuxGeom );
  rd->m_pD3DRenderAuxGeom->ReleaseDeviceObjects();
  rd->ReleaseVideoPools();
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK CD3D9Renderer::OnD3D9DestroyDevice(void* pUserContext)
{
}

#ifndef XENON
bool CALLBACK CD3D9Renderer::OnD3D9DeviceChanging(DXUTDeviceSettings* pDeviceSettings, void* pUserContext)
{
  IDirect3D9 * pD3D9 = DXUTGetD3D9Object();
#ifndef PUBLIC_BUILD
  if (pD3D9 != NULL && CV_d3d9_nvperfhud)
  {
    OutputDebugString( TEXT("Opting in to nvPerfHUD!\n"));
    pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
    pDeviceSettings->d3d9.AdapterOrdinal = pD3D9->GetAdapterCount() - 1;
    pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
  }
#endif
  return true;
}
#endif

HRESULT CALLBACK CD3D9Renderer::OnD3D9PostCreateDevice(D3DDevice *pd3dDevice)
{
  CD3D9Renderer *rd = gcpRendD3D;
#ifndef XENON
  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();

  rd->m_pD3D = DXUTGetD3D9Object();
  rd->m_pd3dDevice = DXUTGetD3D9Device();
  rd->m_pd3dCaps = DXUTGetD3D9DeviceCaps();
  rd->m_pd3dpp = &pDev->d3d9.pp;
#endif

  // DXUT doesn't create a device when started from Media Center (DEVICE_LOST) but returns S_OK in DXUTCreateDevice() 
  // call above! Shouldn't happen as we call SuccessfullyLaunchFromMediaCenter() to wait until a device can be created 
  // again but if that fails too check for a NULL pointer here to prevent crashes
  if (!rd->m_pD3D || !rd->m_pd3dDevice) 
    return S_FALSE;

  return S_OK;
}

#elif defined (DIRECT3D10)

//--------------------------------------------------------------------------------------
// Reject any D3D10 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK CD3D9Renderer::IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
  return true;
}

bool    CALLBACK CD3D9Renderer::OnD3D10DeviceChanging(DXUTDeviceSettings* pDeviceSettings, void* pUserContext)
{
#ifndef PUBLIC_BUILD
  if (CV_d3d9_nvperfhud)
  {
    IDXGIFactory *pDXGIFactory = DXUTGetDXGIFactory();
    if (!pDXGIFactory)
      return true;

    // Search for a PerfHUD adapter.  
    UINT nAdapter = 0; 
    IDXGIAdapter* adapter = NULL; 
    int nSelectedAdapter = -1; 
    D3D10_DRIVER_TYPE driverType = D3D10_DRIVER_TYPE_HARDWARE; 
    bool isPerfHUD = false;

    while (pDXGIFactory->EnumAdapters(nAdapter, &adapter) != DXGI_ERROR_NOT_FOUND) 
    { 
      if (adapter) 
      { 
        DXGI_ADAPTER_DESC adaptDesc; 
        if (SUCCEEDED(adapter->GetDesc(&adaptDesc))) 
        { 
          isPerfHUD = wcscmp(adaptDesc.Description, L"NVIDIA PerfHUD") == 0; 

          // Select the first adapter in normal circumstances or the PerfHUD one if it exists. 
          if(nAdapter == 0 || isPerfHUD) 
            nSelectedAdapter = nAdapter; 

          ++nAdapter; 
        } 
      } 
    }
    if (isPerfHUD)
    {
      pDeviceSettings->d3d10.AdapterOrdinal = nSelectedAdapter;
      pDeviceSettings->d3d10.DriverType = D3D10_DRIVER_TYPE_REFERENCE;
      OutputDebugString( TEXT("Opting in to nvPerfHUD!\n"));
    }
  }
#endif
  return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK CD3D9Renderer::OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC *pBackBufferSurfaceDesc, void* pUserContext )
{
  CD3D9Renderer *rd = gcpRendD3D;

  rd->m_Features |= RFT_OCCLUSIONQUERY | RFT_MULTITEXTURE | RFT_ALLOWRECTTEX |
                RFT_ALLOWANISOTROPIC | RFT_HW_PS20 | RFT_HW_PS2X | RFT_HW_PS30 | RFT_HW_PS40 |
                RFT_DEPTHMAPS | RFT_HW_ENVBUMPPROJECTED;
  rd->m_RP.m_nMaxLightsPerPass = 4;

  rd->m_pd3dDevice = pd3dDevice;
  rd->m_pd3dDebug = DXUTGetD3D10Debug();

  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
  CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
  CD3D10EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo(pDev->d3d10.AdapterOrdinal);

#if defined(WIN32) || defined(WIN64)
	LARGE_INTEGER driverVersion; driverVersion.LowPart = 0; driverVersion.HighPart = 0;
	pAdapterInfo->m_pAdapter->CheckInterfaceSupport(__uuidof(ID3D10Device), &driverVersion);
	iLog->Log ( "D3D Adapter: Description: %ls", pAdapterInfo->AdapterDesc.Description);
	iLog->Log ( "D3D Adapter: Driver version (UMD): %d.%02d.%02d.%04d", HIWORD(driverVersion.u.HighPart), LOWORD(driverVersion.u.HighPart), HIWORD(driverVersion.u.LowPart), LOWORD(driverVersion.u.LowPart));
	iLog->Log ( "D3D Adapter: VendorId = 0x%.4X", pAdapterInfo->AdapterDesc.VendorId);
	iLog->Log ( "D3D Adapter: DeviceId = 0x%.4X", pAdapterInfo->AdapterDesc.DeviceId);
	iLog->Log ( "D3D Adapter: SubSysId = 0x%.8X", pAdapterInfo->AdapterDesc.SubSysId);
	iLog->Log ( "D3D Adapter: Revision = %i", pAdapterInfo->AdapterDesc.Revision);
#endif

	// Vendor-specific initializations and workarounds for driver bugs.
	{
		if(pAdapterInfo->AdapterDesc.VendorId==4098)
		{
			// ATI
			rd->InitATIAPI();

			rd->m_Features |= RFT_HW_ATI;
			iLog->Log ("D3D Detected: ATI video card");
		}
		else if( pAdapterInfo->AdapterDesc.VendorId==4318 )
		{
			rd->m_Features |= RFT_HW_NV4X;
			rd->InitNVAPI();

			iLog->Log ("D3D Detected: NVidia video card");
		}
	}

  rd->m_numtmus = 16;
	rd->m_MaxAnisotropyLevel = 16;
  rd->m_MaxAnisotropyLevel = min(rd->m_MaxAnisotropyLevel, CRenderer::CV_r_texmaxanisotropy);

  CTexture::m_CurStage = 0;
  CTexture::SetDefTexFilter(CV_d3d9_texturefilter->GetString());

#if defined(WIN32) || defined(WIN64)
	HWND hWndDesktop = GetDesktopWindow();
	HDC dc = GetDC(hWndDesktop);
	ushort gamma[3][256];
	if (GetDeviceGammaRamp(dc, gamma))
		rd->m_Features |= RFT_HWGAMMA;
	ReleaseDC(hWndDesktop, dc);
#endif

  // For safety, lots of drivers don't handle tiny texture sizes too tell.
#if defined(WIN32) || defined(WIN64)
	rd->m_MaxTextureMemory = pAdapterInfo->AdapterDesc.DedicatedVideoMemory;
#else
	rd->m_MaxTextureMemory = 256 * 1024 * 1024;
#endif
  if (CRenderer::CV_r_texturesstreampoolsize <= 0)
    CRenderer::CV_r_texturesstreampoolsize = (int)(rd->m_MaxTextureMemory/1024.0f/1024.0f*0.75f);

  rd->m_MaxTextureSize = 8192;

  rd->m_bDeviceSupportsR16FRendertarget = true;
  rd->m_bDeviceSupportsInstancing = true;
  rd->m_bDeviceSupports_G16R16_FSAA = true;

  rd->m_bDeviceSupportsVertexTexture = true;
  if( rd->m_bDeviceSupportsVertexTexture)
    rd->m_Features |= RFT_HW_VERTEXTEXTURES;

  rd->m_Features |= RFT_OCCLUSIONTEST;

  rd->m_bDeviceSupportsDynBranching = true;
  if (!rd->m_bDeviceSupportsDynBranching && CV_r_shadersdynamicbranching)
  {
    iLog->Log("Device doesn't support dynamic branching in shaders (or it's disabled)");
    _SetVar("r_ShadersDynamicBranching", 0);
  }
  rd->m_bUseDynBranching = CV_r_shadersdynamicbranching != 0;
  rd->m_bUseStatBranching = CV_r_shadersstaticbranching != 0;
	rd->m_bUsePOM = CV_r_usepom != 0;

	rd->m_bDeviceSupportsATOC = true;
  rd->m_bDeviceSupportsFP16Separate = false;
  rd->m_bDeviceSupportsFP16Filter = true;

  // Handle the texture formats we need.
  {
    // Find the needed texture formats.
    {
      rd->m_FirstPixelFormat = NULL;

      // RGB uncompressed unsigned formats
      rd->m_FormatA8R8G8B8.CheckSupport(DXGI_FORMAT_R8G8B8A8_UNORM, "A8R8G8B8");
      rd->m_FormatX8R8G8B8.CheckSupport(DXGI_FORMAT_R8G8B8A8_UNORM, "X8R8G8B8");
      rd->m_FormatR8G8B8.CheckSupport(DXGI_FORMAT_R8G8B8A8_UNORM, "R8G8B8");

      //rd->m_FormatA1R5G5B5.CheckSupport(DXGI_FORMAT_B5G5R5A1_UNORM, "A1R5G5B5");
      //rd->m_FormatR5G6B5.CheckSupport(DXGI_FORMAT_B5G6R5_UNORM, "R5G6B5");
      //rd->m_FormatX1R5G5B5.CheckSupport(DXGI_FORMAT_B5G5R5A1_UNORM, "X1R5G5B5");

      // Alpha formats
      rd->m_FormatA8.CheckSupport(DXGI_FORMAT_A8_UNORM, "A8");

      // Fat RGB uncompressed unsigned formats
      rd->m_FormatA16B16G16R16F.CheckSupport(DXGI_FORMAT_R16G16B16A16_FLOAT, "A16B16G16R16F");
      rd->m_FormatA16B16G16R16.CheckSupport(DXGI_FORMAT_R16G16B16A16_UNORM, "A16B16G16R16");
      rd->m_FormatG16R16F.CheckSupport(DXGI_FORMAT_R16G16_FLOAT, "G16R16F");
      rd->m_FormatR16F.CheckSupport(DXGI_FORMAT_R16_FLOAT, "R16F");
      rd->m_FormatR32F.CheckSupport(DXGI_FORMAT_R32_FLOAT, "R32F");
      rd->m_FormatA32B32G32R32F.CheckSupport(DXGI_FORMAT_R32G32B32A32_FLOAT, "A32B32G32R32F");

      // RGB compressed unsigned formats
      rd->m_FormatDXT1.CheckSupport(DXGI_FORMAT_BC1_UNORM, "DXT1");
      rd->m_FormatDXT3.CheckSupport(DXGI_FORMAT_BC2_UNORM, "DXT3");
      rd->m_FormatDXT5.CheckSupport(DXGI_FORMAT_BC3_UNORM, "DXT5");
      rd->m_Format3Dc.CheckSupport(DXGI_FORMAT_BC5_UNORM, "3DC");

      // Depth formats
      rd->m_FormatDepth24.CheckSupport(DXGI_FORMAT_D24_UNORM_S8_UINT, "D24S8");
      rd->m_FormatDepth16.CheckSupport(DXGI_FORMAT_D16_UNORM, "D16");

      rd->m_FormatD32F.CheckSupport(DXGI_FORMAT_R32_TYPELESS, "D32F");
      rd->m_FormatD24S8.CheckSupport(DXGI_FORMAT_R24G8_TYPELESS, "D24S8");
      rd->m_FormatD16.CheckSupport(DXGI_FORMAT_R16_TYPELESS, "D16");


      if (rd->m_Format3Dc.IsValid())
        rd->m_iDeviceSupportsComprNormalmaps = 1;

      if (rd->m_FormatDXT1.IsValid() || rd->m_FormatDXT3.IsValid() || rd->m_FormatDXT5.IsValid())
        rd->m_Features |= RFT_COMPRESSTEXTURE;
    }
  }

  rd->m_HDR_FloatFormat_Scalar = eTF_R32F;
  int nHDRSupported = 1;
  if (!nHDRSupported)
  {
  #ifdef USE_HDR
    CV_r_hdrrendering = 0;
  #endif
    rd->m_Features &= ~RFT_HW_HDR;
  }
  else
    rd->m_Features |= RFT_HW_HDR;

  iLog->Log("  Renderer HDR_Scalar: %s", CTexture::NameForTextureFormat(rd->m_HDR_FloatFormat_Scalar));

  rd->m_nHDRType = nHDRSupported;
  _SetVar("r_HDRType", rd->m_nHDRType);

  rd->m_NewViewport.TopLeftX = 0;
  rd->m_NewViewport.TopLeftY = 0;
  rd->m_NewViewport.Width = rd->m_width;
  rd->m_NewViewport.Height = rd->m_height;
  rd->m_NewViewport.MinDepth = 0;
  rd->m_NewViewport.MaxDepth = 1.0f;
  pd3dDevice->RSSetViewports(1, &rd->m_NewViewport);

  return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK CD3D9Renderer::OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
  return S_OK;
}

HRESULT CALLBACK CD3D9Renderer::OnD3D10PostCreateDevice(ID3D10Device* pd3dDevice)
{
  CD3D9Renderer *rd = gcpRendD3D;
  HRESULT hr;

  rd->m_pd3dDevice = pd3dDevice;
  rd->m_pBackBuffer = DXUTGetD3D10RenderTargetView();
  rd->m_pZBuffer = DXUTGetD3D10DepthStencilView();
  rd->m_d3dsdBackBuffer = *DXUTGetDXGIBackBufferSurfaceDesc();
  rd->m_width = rd->m_d3dsdBackBuffer.Width;
  rd->m_height = rd->m_d3dsdBackBuffer.Height;
  rd->m_CVWidth->Set(rd->m_width);
  rd->m_CVHeight->Set(rd->m_height);
  DXUTDeviceSettings* pDev = DXUTGetCurrentDeviceSettings();
  rd->m_ZFormat = pDev->d3d10.AutoDepthStencilFormat;
  rd->m_pSwapChain = DXUTGetDXGISwapChain();

  rd->m_nRTStackLevel[0] = 0;
  rd->m_RTStack[0][0].m_pDepth = rd->m_pZBuffer;
  rd->m_RTStack[0][0].m_pSurfDepth = &rd->m_DepthBufferOrig;
  rd->m_RTStack[0][0].m_pTarget = rd->m_pBackBuffer;
  rd->m_RTStack[0][0].m_Width = rd->m_d3dsdBackBuffer.Width;
  rd->m_RTStack[0][0].m_Height = rd->m_d3dsdBackBuffer.Height;

  for (int i=0; i<4; i++)
  {
    rd->m_pNewTarget[i] = &rd->m_RTStack[i][0];
    rd->m_pCurTarget[i] = rd->m_pNewTarget[0]->m_pTex;
  }

  rd->m_DepthBufferOrig.pSurf = rd->m_pZBuffer;
  rd->m_DepthBufferOrig.pTex = DXUTGetD3D10DepthStencil();
  rd->m_pZBuffer->AddRef();
  rd->m_DepthBufferOrig.nWidth = rd->m_d3dsdBackBuffer.Width;
  rd->m_DepthBufferOrig.nHeight = rd->m_d3dsdBackBuffer.Height;
  rd->m_DepthBufferOrig.bBusy = true;
  rd->m_DepthBufferOrig.nFrameAccess = -2;

  rd->m_DepthBufferOrigFSAA.pSurf = rd->m_pZBuffer;
  rd->m_DepthBufferOrig.pTex = DXUTGetD3D10DepthStencil();
  rd->m_DepthBufferOrigFSAA.nWidth = rd->m_d3dsdBackBuffer.Width;
  rd->m_DepthBufferOrigFSAA.nHeight = rd->m_d3dsdBackBuffer.Height;
  rd->m_DepthBufferOrigFSAA.bBusy = true;
  rd->m_DepthBufferOrigFSAA.nFrameAccess = -2;

  SAFE_RELEASE(rd->m_RP.m_FSAAData.m_pDepthTex);
  SAFE_RELEASE(rd->m_RP.m_FSAAData.m_pZBuffer);

  rd->CreateMSAADepthBuffer();

  SAFE_RELEASE(rd->m_pIB);
  int i, j;
  for (j=0; j<POOL_MAX; j++)
  {
    for (i=0; i<4; i++)
    {
      SAFE_RELEASE(rd->m_pVBAr[j][i]);
    }
    rd->m_pVB[j] = NULL;
  }

  SAFE_RELEASE(rd->m_pUnitFrustumVB);
  SAFE_RELEASE(rd->m_pUnitFrustumIB);

  SAFE_RELEASE(rd->m_pUnitSphereVB);
  SAFE_RELEASE(rd->m_pUnitSphereIB);

  D3D10_BUFFER_DESC BufDescI;
  BufDescI.Usage = D3D10_USAGE_DYNAMIC;
  BufDescI.BindFlags = D3D10_BIND_INDEX_BUFFER;
  BufDescI.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
  BufDescI.MiscFlags = 0;

  D3D10_BUFFER_DESC BufDescV;
  ZeroStruct(BufDescV);
  BufDescV.Usage = D3D10_USAGE_DYNAMIC;
  BufDescV.BindFlags = D3D10_BIND_VERTEX_BUFFER;
  BufDescV.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
  BufDescV.MiscFlags = 0;

  rd->m_nIndsDMesh = 32768;
  rd->m_nIOffsDMesh = rd->m_nIndsDMesh;
  BufDescI.ByteWidth = rd->m_nIndsDMesh*sizeof(short);
  hr = pd3dDevice->CreateBuffer(&BufDescI, NULL, &rd->m_pIB);
  if (FAILED(hr))
    return hr;

  for (j=0; j<POOL_MAX; j++)
  {
    int nVertSize;
    int fvf;
    switch (j)
    {
    case POOL_P3F_COL4UB_TEX2F:
    default:
      rd->m_nVertsDMesh[j] = MAX_DYNVB3D_VERTS;
      nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F);
      fvf = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
      break;
    case POOL_P3F_TEX2F:
      rd->m_nVertsDMesh[j] = 16384;
      nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_TEX2F);
      fvf = D3DFVF_XYZ | D3DFVF_TEX1;
      break;
    case POOL_P3F:
      rd->m_nVertsDMesh[j] = 16384;
      nVertSize = sizeof(struct_VERTEX_FORMAT_P3F);
      fvf = 0;
      break;
    case POOL_TRP3F_COL4UB_TEX2F:
      rd->m_nVertsDMesh[j] = 8192;
      nVertSize = sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F);
      fvf = 0;
      break;
    case POOL_TRP3F_TEX2F_TEX3F:
      rd->m_nVertsDMesh[j] = 1024;
      nVertSize = sizeof(struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F);
      fvf = 0;
      break;
    case POOL_P3F_TEX3F:
      rd->m_nVertsDMesh[j] = 4096;
      nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_TEX3F);
      fvf = 0;
      break;
    case POOL_P3F_TEX2F_TEX3F:
      rd->m_nVertsDMesh[j] = 256;
      nVertSize = sizeof(struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F);
      fvf = 0;
      break;
    }
    rd->m_nOffsDMesh[j] = rd->m_nVertsDMesh[j];
    for (i=0; i<4; i++)
    {
      BufDescV.ByteWidth = rd->m_nVertsDMesh[j]*nVertSize;
      hr = pd3dDevice->CreateBuffer(&BufDescV, 0, &rd->m_pVBAr[j][i]);
      if (FAILED(hr))
        return hr;
    }
    rd->m_pVB[j] = rd->m_pVBAr[j][0];
  }

	for (int j=0; j<CV_d3d10_NumStagingBuffers; j++)
	{
		SAFE_DELETE(rd->m_pVBTemp[j]);
		rd->m_pVBTemp[j] = new DynamicVB <byte>(rd->m_pd3dDevice, 0, CV_d3d9_vbpoolsize);
		SAFE_DELETE(rd->m_pIBTemp[j]);
		rd->m_pIBTemp[j] = new DynamicIB <ushort>(rd->m_pd3dDevice, 128*1024);
	}
	//PS3HACK
#if !defined(PS3)
  D3D10_QUERY_DESC QDesc;
  QDesc.Query = D3D10_QUERY_EVENT;
  QDesc.MiscFlags = 0;
  hr = pd3dDevice->CreateQuery(&QDesc, &rd->m_pQuery[0]);
  assert(rd->m_pQuery[0]);
  rd->m_pQuery[0]->End();
  hr = pd3dDevice->CreateQuery(&QDesc, &rd->m_pQuery[1]);
  rd->m_pQuery[1]->End();
  assert(rd->m_pQuery[1]);
#endif
  rd->EF_Restore();

  rd->m_bDeviceLost = false;
  rd->m_pLastVDeclaration = NULL;

  assert(0 != rd->m_pD3DRenderAuxGeom);
  if (FAILED(hr=rd->m_pD3DRenderAuxGeom->RestoreDeviceObjects()))
  {
    return( hr );
  }
  CHWShader_D3D::mfSetGlobalParams();
  rd->ResetToDefault();

  return S_OK;
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK CD3D9Renderer::OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK CD3D9Renderer::OnD3D10ReleasingSwapChain( void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK CD3D9Renderer::OnD3D10DestroyDevice( void* pUserContext )
{
}

#endif
