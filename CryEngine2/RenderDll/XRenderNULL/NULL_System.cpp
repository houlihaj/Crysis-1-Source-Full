/*=============================================================================
  PS2_System.cpp : HW depended PS2 functions and extensions handling.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#undef THIS_FILE
static char THIS_FILE[] = __FILE__;


#include "StdAfx.h"
#include "NULL_Renderer.h"


bool CNULLRenderer::SetGammaDelta(const float fGamma)
{
  m_fDeltaGamma = fGamma;
  return true;
}

int CNULLRenderer::EnumDisplayFormats(SDispFormat* Formats)
{
  return 0;
}

bool CNULLRenderer::ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen)
{
  return false;
}

WIN_HWND CNULLRenderer::Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen,WIN_HINSTANCE hinst, WIN_HWND Glhwnd, bool bReInit, const SCustomRenderInitArgs* pCustomArgs)
{
  //=======================================
  // Add init code here
  //=======================================

  SetPolygonMode(R_SOLID_MODE);

  m_width = width;
  m_height = height;
  m_Features |= RFT_HW_GFFX;
  
  iLog->Log("Init Shaders\n");

  gRenDev->m_cEF.mfInit();
  EF_Init();

#if defined(LINUX) || defined(XENON) || defined(_PS3)
	return (WIN_HWND)this;//it just get checked against NULL anyway
#else
  return (WIN_HWND)GetDesktopWindow();
#endif
}


bool CNULLRenderer::SetCurrentContext(WIN_HWND hWnd)
{
  return true;
}

bool CNULLRenderer::CreateContext(WIN_HWND hWnd, bool bAllowFSAA)
{
  return true;
}

bool CNULLRenderer::DeleteContext(WIN_HWND hWnd)
{
  return true;
}

void CNULLRenderer::MakeCurrent()
{
}

void CNULLRenderer::ShutDown(bool bReInit)
{
  FreeResources(FRR_ALL);
  EF_PipelineShutdown();
}

void CNULLRenderer::ShutDownFast()
{
  EF_PipelineShutdown();
}

