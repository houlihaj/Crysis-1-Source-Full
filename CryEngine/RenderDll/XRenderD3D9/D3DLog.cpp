#include "StdAfx.h"
#include "DriverD3D.h"

#ifdef DO_RENDERLOG

#define ecase(e) case e: return #e

static char sStr[1024];
#define edefault(e) default: sprintf(sStr, "0x%x", e); return sStr;

#define eflag(e)  \
  if (Flag & e)   \
{               \
    if (sStr[0])  \
      strcat (sStr, " | "); \
    strcat(sStr, #e);       \
}               \

static char *sHex (DWORD Value)
{
  sprintf(sStr, "0x%x", (uint32)Value);
  return sStr;
}
static char *sInt (DWORD Value)
{
  sprintf(sStr, "%d", (uint32)Value);
  return sStr;
}
static char *sDWFloat (DWORD Value)
{
  float fVal = *((float*)(&Value));
  sprintf(sStr, "%.3f", fVal);
  return sStr;
}

static char *sBool (DWORD Value)
{
  switch (Value)
  {
    ecase (FALSE);
    ecase (TRUE);
    edefault ((uint32)Value);
  }
}


#if defined (DIRECT3D9) || defined (OPENGL)

static char *sD3DZB (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DZB_FALSE);
    ecase (D3DZB_TRUE);
    ecase (D3DZB_USEW);
    edefault (Value);
  }
}

static char *sD3DFILL (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DFILL_POINT);
    ecase (D3DFILL_WIREFRAME);
    ecase (D3DFILL_SOLID);
    edefault (Value);
  }
}

static char *sD3DSHADE (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DSHADE_FLAT);
    ecase (D3DSHADE_GOURAUD);
    ecase (D3DSHADE_PHONG);
    edefault (Value);
  }
}

static char *sD3DBLEND (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DBLEND_ZERO);
    ecase (D3DBLEND_ONE);
    ecase (D3DBLEND_SRCCOLOR);
    ecase (D3DBLEND_INVSRCCOLOR);
    ecase (D3DBLEND_SRCALPHA);
    ecase (D3DBLEND_INVSRCALPHA);
    ecase (D3DBLEND_DESTALPHA);
    ecase (D3DBLEND_INVDESTALPHA);
    ecase (D3DBLEND_DESTCOLOR);
    ecase (D3DBLEND_INVDESTCOLOR);
    ecase (D3DBLEND_SRCALPHASAT);  
#ifndef _XBOX
		ecase (D3DBLEND_BOTHSRCALPHA);
    ecase (D3DBLEND_BOTHINVSRCALPHA);
#else		
		ecase (D3DBLEND_CONSTANTALPHA); // Xenon extension
    ecase (D3DBLEND_INVCONSTANTALPHA); // Xenon extension
#endif
	   ecase (D3DBLEND_BLENDFACTOR);
    ecase (D3DBLEND_INVBLENDFACTOR);
    edefault (Value);
  }
}

static char *sD3DCULL (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DCULL_NONE);
    ecase (D3DCULL_CW);
    ecase (D3DCULL_CCW);
    edefault (Value);
  }
}

static char *sD3DCMP (DWORD Value)
{
  switch(Value)
  {
    ecase (D3DCMP_NEVER);
    ecase (D3DCMP_LESS);
    ecase (D3DCMP_EQUAL);
    ecase (D3DCMP_LESSEQUAL);
    ecase (D3DCMP_GREATER);
    ecase (D3DCMP_NOTEQUAL);
    ecase (D3DCMP_GREATEREQUAL);
    ecase (D3DCMP_ALWAYS);
    edefault (Value);
  }
}

static char *sD3DFOG (DWORD Value)
{
  switch(Value)
  {
    ecase (D3DFOG_NONE);
    ecase (D3DFOG_EXP);
    ecase (D3DFOG_EXP2);
    ecase (D3DFOG_LINEAR);
    edefault (Value);
  }
}

static char *sD3DSTENCILOP (DWORD Value)
{
  switch(Value)
  {
    ecase (D3DSTENCILOP_KEEP);
    ecase (D3DSTENCILOP_ZERO);
    ecase (D3DSTENCILOP_REPLACE);
    ecase (D3DSTENCILOP_INCRSAT);
    ecase (D3DSTENCILOP_DECRSAT);
    ecase (D3DSTENCILOP_INVERT);
    ecase (D3DSTENCILOP_INCR);
    ecase (D3DSTENCILOP_DECR);
    edefault (Value);
  }
}

static char *sD3DMCS (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DMCS_MATERIAL);
    ecase (D3DMCS_COLOR1);
    ecase (D3DMCS_COLOR2);
    edefault (Value);
  }
}

static char *sD3DVBF (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DVBF_DISABLE);
    ecase (D3DVBF_1WEIGHTS);
    ecase (D3DVBF_2WEIGHTS);
    ecase (D3DVBF_3WEIGHTS);
    ecase (D3DVBF_TWEENING);
    ecase (D3DVBF_0WEIGHTS);
    edefault (Value);
  }
}

static char *sD3DPATCHEDGE (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DPATCHEDGE_DISCRETE);
    ecase (D3DPATCHEDGE_CONTINUOUS);
    edefault (Value);
  }
}

static char *sD3DDMT (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DDMT_ENABLE);
    ecase (D3DDMT_DISABLE);
    edefault (Value);
  }
}

static char *sD3DBLENDOP (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DBLENDOP_ADD);
    ecase (D3DBLENDOP_SUBTRACT);
    ecase (D3DBLENDOP_REVSUBTRACT);
    ecase (D3DBLENDOP_MIN);
    ecase (D3DBLENDOP_MAX);
    edefault (Value);
  }
}

static char *sD3DDEGREE (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DDEGREE_LINEAR);
    ecase (D3DDEGREE_QUADRATIC);
    ecase (D3DDEGREE_CUBIC);
    ecase (D3DDEGREE_QUINTIC);
    edefault (Value);
  }
}

static char *sD3DTOP (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DTOP_DISABLE);
    ecase (D3DTOP_SELECTARG1);
    ecase (D3DTOP_SELECTARG2);
    ecase (D3DTOP_MODULATE);
    ecase (D3DTOP_MODULATE2X);
    ecase (D3DTOP_MODULATE4X);
    ecase (D3DTOP_ADD);
    ecase (D3DTOP_ADDSIGNED);
    ecase (D3DTOP_ADDSIGNED2X);
    ecase (D3DTOP_SUBTRACT);
    ecase (D3DTOP_ADDSMOOTH);
    ecase (D3DTOP_BLENDDIFFUSEALPHA);
    ecase (D3DTOP_BLENDTEXTUREALPHA);
    ecase (D3DTOP_BLENDFACTORALPHA);
    ecase (D3DTOP_BLENDTEXTUREALPHAPM);
    ecase (D3DTOP_BLENDCURRENTALPHA);
    ecase (D3DTOP_PREMODULATE);
    ecase (D3DTOP_MODULATEALPHA_ADDCOLOR);
    ecase (D3DTOP_MODULATECOLOR_ADDALPHA);
    ecase (D3DTOP_MODULATEINVALPHA_ADDCOLOR);
    ecase (D3DTOP_MODULATEINVCOLOR_ADDALPHA);
    ecase (D3DTOP_BUMPENVMAP);
    ecase (D3DTOP_BUMPENVMAPLUMINANCE);
    ecase (D3DTOP_DOTPRODUCT3);
    ecase (D3DTOP_MULTIPLYADD);
    ecase (D3DTOP_LERP);
    edefault (Value);
  }
}

static char *sD3DPT (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DPT_POINTLIST);
    ecase (D3DPT_LINELIST);
    ecase (D3DPT_LINESTRIP);
    ecase (D3DPT_TRIANGLELIST);
    ecase (D3DPT_TRIANGLESTRIP);
    ecase (D3DPT_TRIANGLEFAN);
    edefault (Value);
  }
}

static char *sD3DTA (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DTA_CURRENT);
    ecase (D3DTA_DIFFUSE);
    ecase (D3DTA_SELECTMASK);
    ecase (D3DTA_SPECULAR);
    ecase (D3DTA_TEMP);
    ecase (D3DTA_TEXTURE);
    ecase (D3DTA_TFACTOR);
    edefault (Value);
  }
}

static char *sTEXCOORDINDEX (DWORD Value)
{
  sprintf(sStr, "%d", Value & 0xf);
  if (Value & D3DTSS_TCI_CAMERASPACENORMAL)
    strcat (sStr, " | D3DTSS_TCI_CAMERASPACENORMAL");
  else
  if (Value & D3DTSS_TCI_CAMERASPACEPOSITION)
    strcat (sStr, " | D3DTSS_TCI_CAMERASPACEPOSITION");
  else
  if (Value & D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR)
    strcat (sStr, " | D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR");
  else
  if (Value & D3DTSS_TCI_SPHEREMAP)
    strcat (sStr, " | D3DTSS_TCI_SPHEREMAP");
  else
    strcat (sStr, " | D3DTSS_TCI_PASSTHRU");

  return sStr;
}

static char *sD3DTTFF (DWORD Value)
{
  switch(Value & 0xf)
  {
    case D3DTTFF_DISABLE:
      strcpy(sStr, "D3DTTFF_DISABLE");
  	  break;
    case D3DTTFF_COUNT1:
      strcpy(sStr, "D3DTTFF_COUNT1");
  	  break;
    case D3DTTFF_COUNT2:
      strcpy(sStr, "D3DTTFF_COUNT2");
      break;
    case D3DTTFF_COUNT3:
      strcpy(sStr, "D3DTTFF_COUNT3");
      break;
    case D3DTTFF_COUNT4:
      strcpy(sStr, "D3DTTFF_COUNT4");
      break;
    default:
      sprintf(sStr, "0x%x", Value & 0xf);
  }
  if (Value & D3DTTFF_PROJECTED)
    strcat (sStr, " | D3DTTFF_PROJECTED");

  return sStr;
}

static char *sD3DTADDRESS (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DTADDRESS_WRAP);
    ecase (D3DTADDRESS_MIRROR);
    ecase (D3DTADDRESS_CLAMP);
    ecase (D3DTADDRESS_BORDER);
    ecase (D3DTADDRESS_MIRRORONCE);
    edefault (Value);
  }
}

static char *sD3DTEXF (DWORD Value)
{
  switch (Value)
  {
    ecase (D3DTEXF_NONE);
    ecase (D3DTEXF_POINT);
    ecase (D3DTEXF_LINEAR);
    ecase (D3DTEXF_ANISOTROPIC);
    ecase (D3DTEXF_PYRAMIDALQUAD);
    ecase (D3DTEXF_GAUSSIANQUAD);
    edefault (Value);
  }
}

static char *sD3DTS (DWORD Value)
{
  switch(Value)
  {
    ecase (D3DTS_VIEW);
    ecase (D3DTS_PROJECTION);
    ecase (D3DTS_TEXTURE0);
    ecase (D3DTS_TEXTURE1);
    ecase (D3DTS_TEXTURE2);
    ecase (D3DTS_TEXTURE3);
    ecase (D3DTS_TEXTURE4);
    ecase (D3DTS_TEXTURE5);
    ecase (D3DTS_TEXTURE6);
    ecase (D3DTS_TEXTURE7);
    edefault (Value);
  }
}

static char *sD3DCLEARFLAGS (DWORD Value)
{
  if (!(Value & (D3DCLEAR_TARGET | D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER)))
    sprintf(sStr, "0x%x", Value);
  else
  {
    sStr[0] = 0;
    if (Value & D3DCLEAR_TARGET)
      strcat(sStr, "D3DCLEAR_TARGET");
    if (Value & D3DCLEAR_ZBUFFER)
    {
      if (sStr[0])
        strcat (sStr, " | ");
      strcat(sStr, "D3DCLEAR_ZBUFFER");
    }
    if (Value & D3DCLEAR_STENCIL)
    {
      if (sStr[0])
        strcat (sStr, " | ");
      strcat(sStr, "D3DCLEAR_STENCIL");
    }
  }

  return sStr;
}

static char *sFVF (DWORD Flag)
{
  sStr[0] = 0;
  eflag(D3DFVF_XYZ);
  eflag(D3DFVF_XYZRHW);
  eflag(D3DFVF_NORMAL);
  eflag(D3DFVF_PSIZE);
  eflag(D3DFVF_DIFFUSE);
  eflag(D3DFVF_SPECULAR);
  char ss[128];
  if(Flag & 0xf00)
  {
    sprintf(ss, "D3DFVF_TEX%d", (Flag >> 8)&0xf);
    if (sStr[0])
      strcat (sStr, " | ");
    strcat(sStr, ss);
  }

  return sStr;
}

static char *sD3DFMT (D3DFORMAT Value)
{
  switch (Value)
  {
    ecase(D3DFMT_X8R8G8B8);
    ecase(D3DFMT_A8R8G8B8);
    ecase(D3DFMT_R16F);
    ecase(D3DFMT_G16R16F);
    ecase(D3DFMT_R32F);
    ecase(D3DFMT_A16B16G16R16F);
    edefault (Value);
  }
}

//=========================================================================================

#ifndef OPENGL
HRESULT CMyDirect3DDevice9::QueryInterface(REFIID riid, void** ppvObj)
{
#ifdef _XBOX
  return E_NOINTERFACE;
#else
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::QueryInterface");
  return gcpRendD3D->m_pActuald3dDevice->QueryInterface(riid, ppvObj);
#endif
}
ULONG CMyDirect3DDevice9::AddRef()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::AddRef");
  return gcpRendD3D->m_pActuald3dDevice->AddRef();
}
ULONG CMyDirect3DDevice9::Release()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::Release");
  return gcpRendD3D->m_pActuald3dDevice->Release();
}
#endif

  /*** IDirect3DDevice9 methods ***/
HRESULT CMyDirect3DDevice9::TestCooperativeLevel()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::TestCooperativeLevel");
  return gcpRendD3D->m_pActuald3dDevice->TestCooperativeLevel();
}


UINT CMyDirect3DDevice9::GetAvailableTextureMem()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetAvailableTextureMem");
  return gcpRendD3D->m_pActuald3dDevice->GetAvailableTextureMem();
}


HRESULT CMyDirect3DDevice9::EvictManagedResources()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::EvictManagedResources");
  return gcpRendD3D->m_pActuald3dDevice->EvictManagedResources();
}
HRESULT CMyDirect3DDevice9::GetDirect3D(IDirect3D9** ppD3D9)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetDirect3D");
  return gcpRendD3D->m_pActuald3dDevice->GetDirect3D(ppD3D9);
}
HRESULT CMyDirect3DDevice9::GetDeviceCaps(D3DCAPS9* pCaps)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetDeviceCaps");
  return gcpRendD3D->m_pActuald3dDevice->GetDeviceCaps(pCaps);
}
HRESULT CMyDirect3DDevice9::GetDisplayMode(UINT iSwapChain,D3DDISPLAYMODE* pMode)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetDisplayMode");
  return gcpRendD3D->m_pActuald3dDevice->GetDisplayMode(iSwapChain, pMode);
}
HRESULT CMyDirect3DDevice9::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetCreationParameters");
  return gcpRendD3D->m_pActuald3dDevice->GetCreationParameters(pParameters);
}
HRESULT CMyDirect3DDevice9::SetCursorProperties(UINT XHotSpot,UINT YHotSpot,IDirect3DSurface9* pCursorBitmap)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetCursorProperties");
  return gcpRendD3D->m_pActuald3dDevice->SetCursorProperties(XHotSpot,YHotSpot,pCursorBitmap);
}

#if !defined(XENON) && !defined(PS3)
void CMyDirect3DDevice9::SetCursorPosition(int X,int Y,DWORD Flags)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetCursorPosition");
  return gcpRendD3D->m_pActuald3dDevice->SetCursorPosition(X,Y,Flags);
#endif
}
#endif

BOOL CMyDirect3DDevice9::ShowCursor(BOOL bShow)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::ShowCursor");
  return gcpRendD3D->m_pActuald3dDevice->ShowCursor(bShow);
#else
  return true;
#endif
}


#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain9** pSwapChain)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateAdditionalSwapChain");
  return gcpRendD3D->m_pActuald3dDevice->CreateAdditionalSwapChain( pPresentationParameters, pSwapChain);
#else
  return S_OK;
#endif
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::GetSwapChain(UINT iSwapChain,IDirect3DSwapChain9** pSwapChain)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetSwapChain");
  return gcpRendD3D->m_pActuald3dDevice->GetSwapChain(iSwapChain,pSwapChain);
#else
  return S_OK;
#endif
}
#endif

#if !defined(XENON) && !defined(PS3)
UINT CMyDirect3DDevice9::GetNumberOfSwapChains()
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetNumberOfSwapChains");
  return gcpRendD3D->m_pActuald3dDevice->GetNumberOfSwapChains();
#else
  return 1;
#endif
}    
#endif

#ifdef XENON
HRESULT CMyDirect3DDevice9::Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,void* hDestWindowOverride,void* pDirtyRegion)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::Present");
  return gcpRendD3D->m_pActuald3dDevice->Present(pSourceRect,pDestRect,hDestWindowOverride, pDirtyRegion);
}
#else
HRESULT CMyDirect3DDevice9::Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::Present");
  return gcpRendD3D->m_pActuald3dDevice->Present(pSourceRect,pDestRect,hDestWindowOverride,pDirtyRegion);
}
#endif

HRESULT CMyDirect3DDevice9::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::Reset");
  return gcpRendD3D->m_pActuald3dDevice->Reset(pPresentationParameters);
}

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::GetBackBuffer(UINT iSwapChain,UINT iBackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface9** ppBackBuffer)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetBackBuffer");
  return gcpRendD3D->m_pActuald3dDevice->GetBackBuffer(iSwapChain,iBackBuffer,Type,ppBackBuffer);
}
#else
HRESULT CMyDirect3DDevice9::GetBackBuffer(UINT iSwapChain,UINT iBackBuffer,UINT Type,IDirect3DSurface9** ppBackBuffer)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetBackBuffer");
  return gcpRendD3D->m_pActuald3dDevice->GetBackBuffer(iSwapChain,iBackBuffer,Type,ppBackBuffer);
}
#endif

HRESULT CMyDirect3DDevice9::GetRasterStatus(UINT iSwapChain,D3DRASTER_STATUS* pRasterStatus)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetRasterStatus");
  return gcpRendD3D->m_pActuald3dDevice->GetRasterStatus(iSwapChain,pRasterStatus);
}

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::SetDialogBoxMode(BOOL bEnableDialogs)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetDialogBoxMode");
  return gcpRendD3D->m_pActuald3dDevice->SetDialogBoxMode(bEnableDialogs);
#else
  return S_OK;
#endif
}
#endif

void CMyDirect3DDevice9::SetGammaRamp(UINT iSwapChain,DWORD Flags,CONST D3DGAMMARAMP* pRamp)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetGammaRamp");
  return gcpRendD3D->m_pActuald3dDevice->SetGammaRamp(iSwapChain,Flags,pRamp);
}

void CMyDirect3DDevice9::GetGammaRamp(UINT iSwapChain,D3DGAMMARAMP* pRamp)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetGammaRamp");
  return gcpRendD3D->m_pActuald3dDevice->GetGammaRamp(iSwapChain,pRamp);
}

HRESULT CMyDirect3DDevice9::CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9** ppTexture,HANDLE* pSharedHandle)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateTexture");
  return gcpRendD3D->m_pActuald3dDevice->CreateTexture(Width,Height,Levels,Usage,Format,Pool,ppTexture,pSharedHandle);
}
HRESULT CMyDirect3DDevice9::CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9** ppVolumeTexture,HANDLE* pSharedHandle)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateVolumeTexture");
  return gcpRendD3D->m_pActuald3dDevice->CreateVolumeTexture(Width,Height,Depth,Levels,Usage,Format,Pool,ppVolumeTexture,pSharedHandle);
}
HRESULT CMyDirect3DDevice9::CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9** ppCubeTexture,HANDLE* pSharedHandle)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateCubeTexture");
  return gcpRendD3D->m_pActuald3dDevice->CreateCubeTexture(EdgeLength,Levels,Usage,Format,Pool,ppCubeTexture,pSharedHandle);
}
HRESULT CMyDirect3DDevice9::CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer9** ppVertexBuffer,HANDLE* pSharedHandle)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateVertexBuffer");
  return gcpRendD3D->m_pActuald3dDevice->CreateVertexBuffer(Length,Usage,FVF,Pool, ppVertexBuffer, pSharedHandle);
}
HRESULT CMyDirect3DDevice9::CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9** ppIndexBuffer,HANDLE* pSharedHandle)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateIndexBuffer");
  return gcpRendD3D->m_pActuald3dDevice->CreateIndexBuffer(Length,Usage,Format,Pool, ppIndexBuffer,pSharedHandle);
}

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateRenderTarget");
	return gcpRendD3D->m_pActuald3dDevice->CreateRenderTarget(Width,Height,Format,MultiSample,MultisampleQuality,Lockable, ppSurface,
#if defined(__GNUC__)
	    reinterpret_cast<void **>(pSharedHandle)
#else
	    pSharedHandle
#endif
	    );
}
#else
HRESULT CMyDirect3DDevice9::CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface, CONST D3DSURFACE_PARAMETERS* pSharedHandle)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateRenderTarget");
  return gcpRendD3D->m_pActuald3dDevice->CreateRenderTarget(Width,Height,Format,MultiSample,MultisampleQuality,Lockable, ppSurface, pSharedHandle);
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateDepthStencilSurface");
	return gcpRendD3D->m_pActuald3dDevice->CreateDepthStencilSurface(Width,Height,Format,MultiSample,MultisampleQuality,Discard, ppSurface,
#if defined(__GNUC__)
	    reinterpret_cast<void **>(pSharedHandle)
#else
	    pSharedHandle
#endif
	    );
}
#else
HRESULT CMyDirect3DDevice9::CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,IDirect3DSurface9** ppSurface, CONST D3DSURFACE_PARAMETERS* pSharedHandle)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateDepthStencilSurface");
  return gcpRendD3D->m_pActuald3dDevice->CreateDepthStencilSurface(Width,Height,Format,MultiSample,MultisampleQuality,Discard, ppSurface, pSharedHandle);
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::UpdateSurface(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestinationSurface,CONST POINT* pDestPoint)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::UpdateSurface");
  return gcpRendD3D->m_pActuald3dDevice->UpdateSurface(pSourceSurface,pSourceRect,pDestinationSurface, pDestPoint);
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture,IDirect3DBaseTexture9* pDestinationTexture)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::UpdateTexture");
  return gcpRendD3D->m_pActuald3dDevice->UpdateTexture(pSourceTexture,pDestinationTexture);
}
#endif

#ifdef XENON
HRESULT CMyDirect3DDevice9::Resolve(DWORD Flags, CONST D3DRECT *pSourceRect, D3DBaseTexture *pDestTexture, CONST D3DPOINT *pDestPoint, UINT DestLevel, UINT DestSliceOrFace, CONST D3DVECTOR4* pClearColor, float ClearZ, DWORD ClearStencil, CONST D3DRESOLVE_PARAMETERS* pParameters)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::Resolve");
  return gcpRendD3D->m_pActuald3dDevice->Resolve(Flags, pSourceRect, pDestTexture, pDestPoint, DestLevel, DestSliceOrFace, pClearColor, ClearZ, ClearStencil, pParameters);
}
#endif

HRESULT CMyDirect3DDevice9::GetRenderTargetData(IDirect3DSurface9* pRenderTarget,IDirect3DSurface9* pDestSurface)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetRenderTargetData");
  return gcpRendD3D->m_pActuald3dDevice->GetRenderTargetData(pRenderTarget,pDestSurface);
}
HRESULT CMyDirect3DDevice9::GetFrontBufferData(UINT iSwapChain,IDirect3DSurface9* pDestSurface)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetFrontBufferData");
  return gcpRendD3D->m_pActuald3dDevice->GetFrontBufferData(iSwapChain,pDestSurface);
}
HRESULT CMyDirect3DDevice9::StretchRect(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestSurface,CONST RECT* pDestRect,D3DTEXTUREFILTERTYPE Filter)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::StretchRect");
  return gcpRendD3D->m_pActuald3dDevice->StretchRect(pSourceSurface,pSourceRect,pDestSurface,pDestRect,Filter);
}
HRESULT CMyDirect3DDevice9::ColorFill(IDirect3DSurface9* pSurface,CONST RECT* pRect,D3DCOLOR color)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (0x%x, 0x%x, 0x%x)\n", "D3DDevice::ColorFill", pSurface, pRect, color);
  return gcpRendD3D->m_pActuald3dDevice->ColorFill(pSurface,pRect,color);
}
HRESULT CMyDirect3DDevice9::CreateOffscreenPlainSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DPOOL Pool,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateOffscreenPlainSurface");
  return gcpRendD3D->m_pActuald3dDevice->CreateOffscreenPlainSurface(Width,Height,Format,Pool, ppSurface, pSharedHandle);
}

UINT WINAPI GetD3D9ColorBits( D3DFORMAT fmt );
HRESULT CMyDirect3DDevice9::SetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9* pRenderTarget)
{
  D3DSURFACE_DESC dtdsdRT;
  if (pRenderTarget)
  {
    pRenderTarget->GetDesc(&dtdsdRT);
    int nBPP = GetD3D9ColorBits(dtdsdRT.Format) / 8;
    gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d, 0x%x) (%dx%d x %s: %.3fMb)\n", "D3DDevice::SetRenderTarget", RenderTargetIndex, pRenderTarget, dtdsdRT.Width, dtdsdRT.Height, sD3DFMT(dtdsdRT.Format), (float)(dtdsdRT.Width*dtdsdRT.Height*nBPP) / 1024.0f / 1024.0f);
  }
  else
    gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d, 0x%x)\n", "D3DDevice::SetRenderTarget", RenderTargetIndex, pRenderTarget);
  return gcpRendD3D->m_pActuald3dDevice->SetRenderTarget(RenderTargetIndex,pRenderTarget);
}
HRESULT CMyDirect3DDevice9::GetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9** ppRenderTarget)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetRenderTarget");
  return gcpRendD3D->m_pActuald3dDevice->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
}
HRESULT CMyDirect3DDevice9::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (0x%x)\n", "D3DDevice::SetDepthStencilSurface", pNewZStencil);
  return gcpRendD3D->m_pActuald3dDevice->SetDepthStencilSurface(pNewZStencil);
}
HRESULT CMyDirect3DDevice9::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetDepthStencilSurface");
  return gcpRendD3D->m_pActuald3dDevice->GetDepthStencilSurface(ppZStencilSurface);
}
HRESULT CMyDirect3DDevice9::BeginScene()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::BeginScene");
  return gcpRendD3D->m_pActuald3dDevice->BeginScene();
}
HRESULT CMyDirect3DDevice9::EndScene()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::EndScene");
  return gcpRendD3D->m_pActuald3dDevice->EndScene();
}
HRESULT CMyDirect3DDevice9::Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d, 0x%x, %s, 0x%x, %.3f, 0x%x)\n", "D3DDevice::Clear", Count, pRects, sD3DCLEARFLAGS(Flags), Color, Z, Stencil);
  return gcpRendD3D->m_pActuald3dDevice->Clear(Count,pRects,Flags,Color,Z,Stencil);
}
HRESULT CMyDirect3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix)
{
  char *state = sD3DTS(State);
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%s, 0x%x)\n", "D3DDevice::SetTransform", state, pMatrix);
  return gcpRendD3D->m_pActuald3dDevice->SetTransform(State,pMatrix);
}

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::GetTransform(D3DTRANSFORMSTATETYPE State,D3DMATRIX* pMatrix)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetTransform");
  return gcpRendD3D->m_pActuald3dDevice->GetTransform(State,pMatrix);
#else
  return S_OK;
#endif
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::MultiplyTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::MultiplyTransform");
  return gcpRendD3D->m_pActuald3dDevice->MultiplyTransform(State,pMatrix);
#else
  return S_OK;
#endif
}
#endif

HRESULT CMyDirect3DDevice9::SetViewport(CONST D3DVIEWPORT9* pViewport)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d, %d, %d, %d, %.3f, %.3f)\n", "D3DDevice::SetViewport", pViewport->X, pViewport->Y, pViewport->Width, pViewport->Height, pViewport->MinZ, pViewport->MaxZ);
  return gcpRendD3D->m_pActuald3dDevice->SetViewport(pViewport);
}
HRESULT CMyDirect3DDevice9::GetViewport(D3DVIEWPORT9* pViewport)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetViewport");
  return gcpRendD3D->m_pActuald3dDevice->GetViewport(pViewport);
}

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::SetMaterial(CONST D3DMATERIAL9* pMaterial)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetMaterial");
  return gcpRendD3D->m_pActuald3dDevice->SetMaterial(pMaterial);
#else
  return S_OK;
#endif
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::GetMaterial(D3DMATERIAL9* pMaterial)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetMaterial");
  return gcpRendD3D->m_pActuald3dDevice->GetMaterial(pMaterial);
#else
  return S_OK;
#endif
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::SetLight(DWORD Index,CONST D3DLIGHT9* pLight)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d, 0x%x)\n", "D3DDevice::SetLight", Index, pLight);
  return gcpRendD3D->m_pActuald3dDevice->SetLight(Index,pLight);
#else
  return S_OK;
#endif
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::GetLight(DWORD Index,D3DLIGHT9* pLight)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetLight");
  return gcpRendD3D->m_pActuald3dDevice->GetLight(Index,pLight);
#else
  return S_OK;
#endif
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::LightEnable(DWORD Index,BOOL Enable)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d, %s)\n", "D3DDevice::LightEnable", Index, sBool(Enable));
  return gcpRendD3D->m_pActuald3dDevice->LightEnable(Index,Enable);
#else
  return S_OK;
#endif
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::GetLightEnable(DWORD Index,BOOL* pEnable)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetLightEnable");
  return gcpRendD3D->m_pActuald3dDevice->GetLightEnable(Index,pEnable);
#else
  return S_OK;
#endif
}
#endif

HRESULT CMyDirect3DDevice9::SetClipPlane(DWORD Index,CONST float* pPlane)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetClipPlane");
  return gcpRendD3D->m_pActuald3dDevice->SetClipPlane(Index,pPlane);
}
HRESULT CMyDirect3DDevice9::GetClipPlane(DWORD Index,float* pPlane)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetClipPlane");
  return gcpRendD3D->m_pActuald3dDevice->GetClipPlane(Index,pPlane);
}
HRESULT CMyDirect3DDevice9::SetRenderState(D3DRENDERSTATETYPE State,DWORD Value)
{
#define ECASE(e) case e: state = #e
  char Str[1024];
#define EDEFAULT(e) default: sprintf(Str, "0x%x", e); state = Str;
  char *state;
  char *val = NULL;
  switch(State)
  {
    ECASE (D3DRS_ZENABLE);
    val = sD3DZB(Value);
    break;
    ECASE (D3DRS_FILLMODE);
    val = sD3DFILL(Value);
    break;
#if !defined(XENON) && !defined(PS3)
    ECASE (D3DRS_SHADEMODE);
    val = sD3DSHADE(Value);
    break;
    ECASE (D3DRS_DITHERENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_FOGENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_SPECULARENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_FOGCOLOR);
    val = sHex(Value);
    break;
    ECASE (D3DRS_FOGTABLEMODE);
    val = sD3DFOG(Value);
    break;
    ECASE (D3DRS_FOGSTART);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_FOGEND);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_FOGDENSITY);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_RANGEFOGENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_TEXTUREFACTOR);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP0);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP1);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP2);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP3);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP4);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP5);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP6);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP7);
    val = sHex(Value);
    break;
    ECASE (D3DRS_CLIPPING);
    val = sBool(Value);
    break;
    ECASE (D3DRS_LIGHTING);
    val = sBool(Value);
    break;
    ECASE (D3DRS_AMBIENT);
    val = sHex(Value);
    break;
    ECASE (D3DRS_FOGVERTEXMODE);
    val = sD3DFOG(Value);
    break;
    ECASE (D3DRS_COLORVERTEX);
    val = sBool(Value);
    break;
    ECASE (D3DRS_LOCALVIEWER);
    val = sBool(Value);
    break;
    ECASE (D3DRS_NORMALIZENORMALS);
    val = sBool(Value);
    break;
    ECASE (D3DRS_DIFFUSEMATERIALSOURCE);
    val = sD3DMCS(Value);
    break;
    ECASE (D3DRS_SPECULARMATERIALSOURCE);
    val = sD3DMCS(Value);
    break;
    ECASE (D3DRS_AMBIENTMATERIALSOURCE);
    val = sD3DMCS(Value);
    break;
    ECASE (D3DRS_EMISSIVEMATERIALSOURCE);
    val = sD3DMCS(Value);
    break;
    ECASE (D3DRS_POINTSCALE_A);
    break;
    ECASE (D3DRS_POINTSCALE_B);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_POINTSCALE_C);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_DEBUGMONITORTOKEN);
    val = sD3DDMT(Value);
    break;
    ECASE (D3DRS_TWEENFACTOR);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_PATCHEDGESTYLE);
    val = sD3DPATCHEDGE(Value);
    break;
    ECASE (D3DRS_POSITIONDEGREE);
    val = sD3DDEGREE(Value);
    break;
    ECASE (D3DRS_NORMALDEGREE);
    val = sD3DDEGREE(Value);
    break;
    ECASE (D3DRS_ANTIALIASEDLINEENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_ADAPTIVETESS_X);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_ADAPTIVETESS_Y);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_ADAPTIVETESS_Z);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_ADAPTIVETESS_W);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_ENABLEADAPTIVETESSELLATION);
    val = sBool(Value);
    break;
    ECASE (D3DRS_SRGBWRITEENABLE);
    val = sBool(Value);
    break;
#endif
    ECASE (D3DRS_ZWRITEENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_ALPHATESTENABLE);
    val = sBool(Value);
    if (Value && (gcpRendD3D->m_RTStack[0][gcpRendD3D->m_nRTStackLevel[0]].m_pTex))
    {
      CTexture *pTex = gcpRendD3D->m_RTStack[0][gcpRendD3D->m_nRTStackLevel[0]].m_pTex;
      if (!(gcpRendD3D->m_RP.m_PersFlags2 & RBPF2_ATOC))
      {
        if (pTex->GetDstFormat() == eTF_G16R16F || pTex->GetDstFormat() == eTF_R32F || pTex->GetDstFormat() == eTF_R16F)
        {
          assert(0);
        }
      }
    }
    break;
    ECASE (D3DRS_SRCBLEND);
    val = sD3DBLEND(Value);
    break;
    ECASE (D3DRS_DESTBLEND);
    val = sD3DBLEND(Value);
    break;
    ECASE (D3DRS_CULLMODE);
    val = sD3DCULL(Value);
    break;
    ECASE (D3DRS_ZFUNC);
    val = sD3DCMP(Value);
    break;
    ECASE (D3DRS_ALPHAREF);
    val = sInt(Value);
    break;
    ECASE (D3DRS_ALPHAFUNC);
    val = sD3DCMP(Value);
    break;
    ECASE (D3DRS_ALPHABLENDENABLE);
    val = sBool(Value);
    if (Value && (gcpRendD3D->m_RTStack[0][gcpRendD3D->m_nRTStackLevel[0]].m_pTex))
    {
      CTexture *pTex = gcpRendD3D->m_RTStack[0][gcpRendD3D->m_nRTStackLevel[0]].m_pTex;
      if (pTex->GetDstFormat() == eTF_G16R16F || pTex->GetDstFormat() == eTF_R32F || pTex->GetDstFormat() == eTF_R16F)
      {
        assert(0);
      }
    }
    break;
    ECASE (D3DRS_STENCILENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_STENCILFAIL);
    val = sD3DSTENCILOP(Value);
    break;
    ECASE (D3DRS_STENCILZFAIL);
    val = sD3DSTENCILOP(Value);
    break;
    ECASE (D3DRS_STENCILPASS);
    val = sD3DSTENCILOP(Value);
    break;
    ECASE (D3DRS_STENCILFUNC);
    val = sD3DCMP(Value);
    break;
    ECASE (D3DRS_STENCILREF);
    val = sInt(Value);
    break;
    ECASE (D3DRS_STENCILMASK);
    val = sHex(Value);
    break;
    ECASE (D3DRS_STENCILWRITEMASK);
    val = sHex(Value);
    break;
    ECASE (D3DRS_VERTEXBLEND);
    val = sD3DVBF(Value);
    break;
    ECASE (D3DRS_CLIPPLANEENABLE);
    val = sHex(Value);
    break;
    ECASE (D3DRS_POINTSIZE);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_POINTSIZE_MIN);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_POINTSPRITEENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_POINTSCALEENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_MULTISAMPLEANTIALIAS);
    val = sBool(Value);
    break;
    ECASE (D3DRS_MULTISAMPLEMASK);
    val = sHex(Value);
    break;
    ECASE (D3DRS_POINTSIZE_MAX);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_INDEXEDVERTEXBLENDENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_COLORWRITEENABLE);
    val = sHex(Value);
    break;
    ECASE (D3DRS_BLENDOP);
    val = sD3DBLENDOP(Value);
    break;
    ECASE (D3DRS_SCISSORTESTENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_SLOPESCALEDEPTHBIAS);
    val = sHex(Value);
    break;
    ECASE (D3DRS_MINTESSELLATIONLEVEL);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_MAXTESSELLATIONLEVEL);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_TWOSIDEDSTENCILMODE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_CCW_STENCILFAIL);
    val = sD3DSTENCILOP(Value);
    break;
    ECASE (D3DRS_CCW_STENCILZFAIL);
    val = sD3DSTENCILOP(Value);
    break;
    ECASE (D3DRS_CCW_STENCILPASS);
    val = sD3DSTENCILOP(Value);
    break;
    ECASE (D3DRS_CCW_STENCILFUNC);
    val = sD3DCMP(Value);
    break;
    ECASE (D3DRS_COLORWRITEENABLE1);
    val = sHex(Value);
    break;
    ECASE (D3DRS_COLORWRITEENABLE2);
    val = sHex(Value);
    break;
    ECASE (D3DRS_COLORWRITEENABLE3);
    val = sHex(Value);
    break;
    ECASE (D3DRS_BLENDFACTOR);
    val = sHex(Value);
    break;
    ECASE (D3DRS_DEPTHBIAS);
    val = sDWFloat(Value);
    break;
    ECASE (D3DRS_WRAP8);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP9);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP10);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP11);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP12);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP13);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP14);
    val = sHex(Value);
    break;
    ECASE (D3DRS_WRAP15);
    val = sHex(Value);
    break;
    ECASE (D3DRS_SEPARATEALPHABLENDENABLE);
    val = sBool(Value);
    break;
    ECASE (D3DRS_SRCBLENDALPHA);
    val = sD3DBLEND(Value);
    break;
    ECASE (D3DRS_DESTBLENDALPHA);
    val = sD3DBLEND(Value);
    break;
    ECASE (D3DRS_BLENDOPALPHA);
    val = sD3DBLENDOP(Value);
    break;
    EDEFAULT (State);
  }
  if (!val)
    val = sHex(Value);
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%s, %s)\n", "D3DDevice::SetRenderState", state, val);
  return gcpRendD3D->m_pActuald3dDevice->SetRenderState(State,Value);
#undef ECASE
#undef EDEFAULT
}
HRESULT CMyDirect3DDevice9::GetRenderState(D3DRENDERSTATETYPE State,DWORD* pValue)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetRenderState");
  return gcpRendD3D->m_pActuald3dDevice->GetRenderState(State,pValue);
}
HRESULT CMyDirect3DDevice9::CreateStateBlock(D3DSTATEBLOCKTYPE Type,IDirect3DStateBlock9** ppSB)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateStateBlock");
  return gcpRendD3D->m_pActuald3dDevice->CreateStateBlock(Type,ppSB);
#else
  return S_OK;
#endif
}

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::BeginStateBlock()
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::BeginStateBlock");
  return gcpRendD3D->m_pActuald3dDevice->BeginStateBlock();
#else
  return S_OK;
#endif
}
HRESULT CMyDirect3DDevice9::EndStateBlock(IDirect3DStateBlock9** ppSB)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::EndStateBlock");
  return gcpRendD3D->m_pActuald3dDevice->EndStateBlock(ppSB);
#else
  return S_OK;
#endif
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::SetClipStatus(CONST D3DCLIPSTATUS9* pClipStatus)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetClipStatus");
  return gcpRendD3D->m_pActuald3dDevice->SetClipStatus(pClipStatus);
#else
  return S_OK;
#endif
}
HRESULT CMyDirect3DDevice9::GetClipStatus(D3DCLIPSTATUS9* pClipStatus)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetClipStatus");
  return gcpRendD3D->m_pActuald3dDevice->GetClipStatus(pClipStatus);
#else
  return S_OK;
#endif
}
#endif

HRESULT CMyDirect3DDevice9::GetTexture(DWORD Stage,IDirect3DBaseTexture9** ppTexture)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetTexture");
  return gcpRendD3D->m_pActuald3dDevice->GetTexture(Stage, ppTexture);
}
HRESULT CMyDirect3DDevice9::SetTexture(DWORD Stage,IDirect3DBaseTexture9* pTexture)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d, 0x%x)\n", "D3DDevice::SetTexture", Stage, pTexture);
  return gcpRendD3D->m_pActuald3dDevice->SetTexture(Stage,pTexture);
}

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::GetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetTextureStageState");
  return gcpRendD3D->m_pActuald3dDevice->GetTextureStageState(Stage,Type,pValue);
#else
  return S_OK;
#endif
}
#endif

HRESULT CMyDirect3DDevice9::SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
{
#define ECASE(e) case e: type = #e
  char Str[1024];
#define EDEFAULT(e) default: sprintf(Str, "0x%x", e); type = Str;
  char *type;
  char *val = NULL;
  switch (Type)
  {
    ECASE (D3DTSS_COLOROP);
    val = sD3DTOP(Value);
    break;
    ECASE (D3DTSS_COLORARG1);
    val = sD3DTA(Value);
    break;
    ECASE (D3DTSS_COLORARG2);
    val = sD3DTA(Value);
    break;
    ECASE (D3DTSS_ALPHAOP);
    val = sD3DTOP(Value);
    break;
    ECASE (D3DTSS_ALPHAARG1);
    val = sD3DTA(Value);
    break;
    ECASE (D3DTSS_ALPHAARG2);
    val = sD3DTA(Value);
    break;
    ECASE (D3DTSS_BUMPENVMAT00);
    val = sDWFloat(Value);
    break;
    ECASE (D3DTSS_BUMPENVMAT01);
    val = sDWFloat(Value);
    break;
    ECASE (D3DTSS_BUMPENVMAT10);
    val = sDWFloat(Value);
    break;
    ECASE (D3DTSS_BUMPENVMAT11);
    val = sDWFloat(Value);
    break;
    ECASE (D3DTSS_TEXCOORDINDEX);
    val = sTEXCOORDINDEX(Value);
    break;
    ECASE (D3DTSS_BUMPENVLSCALE);
    val = sDWFloat(Value);
    break;
    ECASE (D3DTSS_BUMPENVLOFFSET);
    val = sDWFloat(Value);
    break;
    ECASE (D3DTSS_TEXTURETRANSFORMFLAGS);
    val = sD3DTTFF(Value);
    break;
    ECASE (D3DTSS_COLORARG0);
    val = sD3DTA(Value);
    break;
    ECASE (D3DTSS_ALPHAARG0);
    val = sD3DTA(Value);
    break;
    ECASE (D3DTSS_RESULTARG);
    val = sD3DTA(Value);
    break;
    ECASE (D3DTSS_CONSTANT);
    val = sHex(Value);
    break;
    EDEFAULT (Type);
  }
  if (!val)
    val = sHex(Value);
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d, %s, %s)\n", "D3DDevice::SetTextureStageState", Stage, type, val);
  return gcpRendD3D->m_pActuald3dDevice->SetTextureStageState(Stage,Type,Value);
#undef ECASE
#undef EDEFAULT
}
HRESULT CMyDirect3DDevice9::GetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD* pValue)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetSamplerState");
  return gcpRendD3D->m_pActuald3dDevice->GetSamplerState(Sampler,Type, pValue);
}
HRESULT CMyDirect3DDevice9::SetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD Value)
{
#define ECASE(e) case e: type = #e
  char Str[1024];
#define EDEFAULT(e) default: sprintf(Str, "0x%x", e); type = Str;
  char *type;
  char *val = NULL;
  switch(Type)
  {
    ECASE (D3DSAMP_ADDRESSU);
    val = sD3DTADDRESS(Value);
    break;
    ECASE (D3DSAMP_ADDRESSV);
    val = sD3DTADDRESS(Value);
    break;
    ECASE (D3DSAMP_ADDRESSW);
    val = sD3DTADDRESS(Value);
    break;
    ECASE (D3DSAMP_BORDERCOLOR);
    val = sHex(Value);
    break;
    ECASE (D3DSAMP_MAGFILTER);
    val = sD3DTEXF(Value);
    break;
    ECASE (D3DSAMP_MINFILTER);
    val = sD3DTEXF(Value);
    break;
    ECASE (D3DSAMP_MIPFILTER);
    val = sD3DTEXF(Value);
    break;
    ECASE (D3DSAMP_MIPMAPLODBIAS);
    val = sDWFloat(Value);
    break;
    ECASE (D3DSAMP_MAXMIPLEVEL);
    val = sInt(Value);
    break;
    ECASE (D3DSAMP_MAXANISOTROPY);
    val = sInt(Value);
    break;
#if !defined(XENON) && !defined(PS3)
    ECASE (D3DSAMP_SRGBTEXTURE);
    val = sBool(Value);
    break;
    ECASE (D3DSAMP_ELEMENTINDEX);
    val = sInt(Value);
    break;
    ECASE (D3DSAMP_DMAPOFFSET);
    val = sInt(Value);
    break;
#endif
    EDEFAULT (Type);
  }
  if (!val)
    val = sHex(Value);
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d, %s, %s)\n", "D3DDevice::SetSamplerState", Sampler, type, val);
  return gcpRendD3D->m_pActuald3dDevice->SetSamplerState(Sampler,Type,Value);
#undef ECASE
#undef EDEFAULT
}

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::ValidateDevice(DWORD* pNumPasses)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::ValidateDevice");
  return gcpRendD3D->m_pActuald3dDevice->ValidateDevice(pNumPasses);
#else
  return S_OK;
#endif
}
HRESULT CMyDirect3DDevice9::SetPaletteEntries(UINT PaletteNumber,CONST PALETTEENTRY* pEntries)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetPaletteEntries");
  return gcpRendD3D->m_pActuald3dDevice->SetPaletteEntries(PaletteNumber,pEntries);
#else
  return S_OK;
#endif
}
HRESULT CMyDirect3DDevice9::GetPaletteEntries(UINT PaletteNumber,PALETTEENTRY* pEntries)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetPaletteEntries");
  return gcpRendD3D->m_pActuald3dDevice->GetPaletteEntries(PaletteNumber,pEntries);
#else
  return S_OK;
#endif
}
HRESULT CMyDirect3DDevice9::SetCurrentTexturePalette(UINT PaletteNumber)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetCurrentTexturePalette");
  return gcpRendD3D->m_pActuald3dDevice->SetCurrentTexturePalette(PaletteNumber);
#else
  return S_OK;
#endif
}
HRESULT CMyDirect3DDevice9::GetCurrentTexturePalette(UINT *PaletteNumber)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetCurrentTexturePalette");
  return gcpRendD3D->m_pActuald3dDevice->GetCurrentTexturePalette(PaletteNumber);
#else
  return S_OK;
#endif
}
#endif

HRESULT CMyDirect3DDevice9::SetScissorRect(CONST RECT* pRect)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, %d, %d)\n", "D3DDevice::SetScissorRect", pRect->left, pRect->right, pRect->top, pRect->bottom);
  return gcpRendD3D->m_pActuald3dDevice->SetScissorRect(pRect);
}
HRESULT CMyDirect3DDevice9::GetScissorRect(RECT* pRect)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetScissorRect");
  return gcpRendD3D->m_pActuald3dDevice->GetScissorRect(pRect);
}

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::SetSoftwareVertexProcessing(BOOL bSoftware)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetSoftwareVertexProcessing");
  return gcpRendD3D->m_pActuald3dDevice->SetSoftwareVertexProcessing(bSoftware);
#else
  return S_OK;
#endif
}
BOOL CMyDirect3DDevice9::GetSoftwareVertexProcessing()
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetSoftwareVertexProcessing");
  return gcpRendD3D->m_pActuald3dDevice->GetSoftwareVertexProcessing();
#else
  return S_OK;
#endif
}
#endif

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::SetNPatchMode(float nSegments)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetNPatchMode");
  return gcpRendD3D->m_pActuald3dDevice->SetNPatchMode(nSegments);
}
float CMyDirect3DDevice9::GetNPatchMode()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetNPatchMode");
  return gcpRendD3D->m_pActuald3dDevice->GetNPatchMode();
}
#endif
HRESULT CMyDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s  (%s, %d, %d)\n", "D3DDevice::DrawPrimitive", sD3DPT(PrimitiveType), StartVertex, PrimitiveCount);
  return gcpRendD3D->m_pActuald3dDevice->DrawPrimitive(PrimitiveType,StartVertex,PrimitiveCount);
}
HRESULT CMyDirect3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%s, %d, %d, %d, %d, %d)\n", "D3DDevice::DrawIndexedPrimitive", sD3DPT(PrimitiveType), BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
  return gcpRendD3D->m_pActuald3dDevice->DrawIndexedPrimitive(PrimitiveType,BaseVertexIndex,MinVertexIndex,NumVertices,startIndex,primCount);
}
HRESULT CMyDirect3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
    gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%s, %d, 0x%x, %d)\n", "D3DDevice::DrawPrimitiveUP", sD3DPT(PrimitiveType), PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
  return gcpRendD3D->m_pActuald3dDevice->DrawPrimitiveUP(PrimitiveType,PrimitiveCount,pVertexStreamZeroData,VertexStreamZeroStride);
}
HRESULT CMyDirect3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::DrawIndexedPrimitiveUP");
  return gcpRendD3D->m_pActuald3dDevice->DrawIndexedPrimitiveUP(PrimitiveType,MinVertexIndex,NumVertices,PrimitiveCount, pIndexData,IndexDataFormat,pVertexStreamZeroData,VertexStreamZeroStride);
}

#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::ProcessVertices(UINT SrcStartIndex,UINT DestIndex,UINT VertexCount,IDirect3DVertexBuffer9* pDestBuffer,IDirect3DVertexDeclaration9* pVertexDecl,DWORD Flags)
{
#ifndef OPENGL
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::ProcessVertices");
  return gcpRendD3D->m_pActuald3dDevice->ProcessVertices(SrcStartIndex,DestIndex,VertexCount,pDestBuffer,pVertexDecl,Flags);
#else
  return S_OK;
#endif
}
#endif

HRESULT CMyDirect3DDevice9::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateVertexDeclaration");
  return gcpRendD3D->m_pActuald3dDevice->CreateVertexDeclaration(pVertexElements,ppDecl);
}
HRESULT CMyDirect3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (0x%x)\n", "D3DDevice::SetVertexDeclaration", pDecl);
  return gcpRendD3D->m_pActuald3dDevice->SetVertexDeclaration(pDecl);
}
HRESULT CMyDirect3DDevice9::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetVertexDeclaration");
  return gcpRendD3D->m_pActuald3dDevice->GetVertexDeclaration( ppDecl);
}
HRESULT CMyDirect3DDevice9::SetFVF(DWORD FVF)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%s)\n", "D3DDevice::SetFVF", sFVF(FVF));
  return gcpRendD3D->m_pActuald3dDevice->SetFVF(FVF);
}
HRESULT CMyDirect3DDevice9::GetFVF(DWORD* pFVF)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetFVF");
  return gcpRendD3D->m_pActuald3dDevice->GetFVF( pFVF);
}
HRESULT CMyDirect3DDevice9::CreateVertexShader(CONST DWORD* pFunction,IDirect3DVertexShader9** ppShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateVertexShader");
  return gcpRendD3D->m_pActuald3dDevice->CreateVertexShader(pFunction, ppShader);
}
HRESULT CMyDirect3DDevice9::SetVertexShader(IDirect3DVertexShader9* pShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (0x%x)\n", "D3DDevice::SetVertexShader", pShader);
  return gcpRendD3D->m_pActuald3dDevice->SetVertexShader( pShader);
}
HRESULT CMyDirect3DDevice9::GetVertexShader(IDirect3DVertexShader9** ppShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetVertexShader");
  return gcpRendD3D->m_pActuald3dDevice->GetVertexShader( ppShader);
}
HRESULT CMyDirect3DDevice9::SetVertexShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d", "D3DDevice::SetVertexShaderConstantF", StartRegister);
  const float *v = pConstantData;
  for (int i=0; i<(int)Vector4fCount; i++)
  {
    gcpRendD3D->Logv(0, ", [%.3f, %.3f, %.3f, %.3f]", v[0], v[1], v[2], v[3]);
    v += 4;
  }
  gcpRendD3D->Logv(0, ", %d)\n", Vector4fCount);
  return gcpRendD3D->m_pActuald3dDevice->SetVertexShaderConstantF(StartRegister,pConstantData,Vector4fCount);
}
HRESULT CMyDirect3DDevice9::GetVertexShaderConstantF(UINT StartRegister,float* pConstantData,UINT Vector4fCount)
{
  //gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetVertexShaderConstantF");
  return gcpRendD3D->m_pActuald3dDevice->GetVertexShaderConstantF(StartRegister,pConstantData,Vector4fCount);
}
HRESULT CMyDirect3DDevice9::SetVertexShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetVertexShaderConstantI");
  return gcpRendD3D->m_pActuald3dDevice->SetVertexShaderConstantI(StartRegister,pConstantData,Vector4iCount);
}
HRESULT CMyDirect3DDevice9::GetVertexShaderConstantI(UINT StartRegister,int* pConstantData,UINT Vector4iCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetVertexShaderConstantI");
  return gcpRendD3D->m_pActuald3dDevice->GetVertexShaderConstantI(StartRegister,pConstantData,Vector4iCount);
}
HRESULT CMyDirect3DDevice9::SetVertexShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetVertexShaderConstantB");
  return gcpRendD3D->m_pActuald3dDevice->SetVertexShaderConstantB(StartRegister,pConstantData,BoolCount);
}
HRESULT CMyDirect3DDevice9::GetVertexShaderConstantB(UINT StartRegister,BOOL* pConstantData,UINT BoolCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetVertexShaderConstantB");
  return gcpRendD3D->m_pActuald3dDevice->GetVertexShaderConstantB(StartRegister,pConstantData,BoolCount);
}
HRESULT CMyDirect3DDevice9::SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d, 0x%x, %d, %d)\n", "D3DDevice::SetStreamSource", StreamNumber, pStreamData, OffsetInBytes, Stride);
  return gcpRendD3D->m_pActuald3dDevice->SetStreamSource(StreamNumber,pStreamData,OffsetInBytes,Stride);
}
HRESULT CMyDirect3DDevice9::GetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9** ppStreamData,UINT* OffsetInBytes,UINT* pStride)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetStreamSource");
  return gcpRendD3D->m_pActuald3dDevice->GetStreamSource(StreamNumber, ppStreamData, OffsetInBytes, pStride);
}
HRESULT CMyDirect3DDevice9::SetStreamSourceFreq(UINT StreamNumber,UINT Divider)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d, 0x%x)\n", "D3DDevice::SetStreamSourceFreq", StreamNumber, Divider);
  return gcpRendD3D->m_pActuald3dDevice->SetStreamSourceFreq(StreamNumber,Divider);
}
HRESULT CMyDirect3DDevice9::GetStreamSourceFreq(UINT StreamNumber,UINT* Divider)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetStreamSourceFreq");
  return gcpRendD3D->m_pActuald3dDevice->GetStreamSourceFreq(StreamNumber,Divider);
}
HRESULT CMyDirect3DDevice9::SetIndices(IDirect3DIndexBuffer9* pIndexData)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (0x%x)\n", "D3DDevice::SetIndices", pIndexData);
  return gcpRendD3D->m_pActuald3dDevice->SetIndices( pIndexData);
}
HRESULT CMyDirect3DDevice9::GetIndices(IDirect3DIndexBuffer9** ppIndexData)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetIndices");
  return gcpRendD3D->m_pActuald3dDevice->GetIndices( ppIndexData);
}
HRESULT CMyDirect3DDevice9::CreatePixelShader(CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreatePixelShader");
  return gcpRendD3D->m_pActuald3dDevice->CreatePixelShader(pFunction,ppShader);
}
HRESULT CMyDirect3DDevice9::SetPixelShader(IDirect3DPixelShader9* pShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (0x%x)\n", "D3DDevice::SetPixelShader", pShader);
  return gcpRendD3D->m_pActuald3dDevice->SetPixelShader( pShader);
}
HRESULT CMyDirect3DDevice9::GetPixelShader(IDirect3DPixelShader9** ppShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (0x%x)\n", "D3DDevice::GetPixelShader", ppShader);
  return gcpRendD3D->m_pActuald3dDevice->GetPixelShader( ppShader);
}
HRESULT CMyDirect3DDevice9::SetPixelShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d", "D3DDevice::SetPixelShaderConstantF", StartRegister);
  const float *v = pConstantData;
  for (int i=0; i<(int)Vector4fCount; i++)
  {
    gcpRendD3D->Logv(0, ", [%.3f, %.3f, %.3f, %.3f]", v[0], v[1], v[2], v[3]);
    v += 4;
  }
  gcpRendD3D->Logv(0, ", %d)\n", Vector4fCount);
  return gcpRendD3D->m_pActuald3dDevice->SetPixelShaderConstantF(StartRegister,pConstantData,Vector4fCount);
}
HRESULT CMyDirect3DDevice9::GetPixelShaderConstantF(UINT StartRegister,float* pConstantData,UINT Vector4fCount)
{
  //gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetPixelShaderConstantF");
  return gcpRendD3D->m_pActuald3dDevice->GetPixelShaderConstantF(StartRegister,pConstantData,Vector4fCount);
}
HRESULT CMyDirect3DDevice9::SetPixelShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s (%d", "D3DDevice::SetPixelShaderConstantI", StartRegister);
  const int *v = pConstantData;
  for (int i=0; i<(int)Vector4iCount; i++)
  {
    gcpRendD3D->Logv(0, ", [%d, %d, %d, %d]", v[0], v[1], v[2], v[3]);
    v += 4;
  }
  gcpRendD3D->Logv(0, ", %d)\n", Vector4iCount);
  return gcpRendD3D->m_pActuald3dDevice->SetPixelShaderConstantI(StartRegister,pConstantData,Vector4iCount);
}
HRESULT CMyDirect3DDevice9::GetPixelShaderConstantI(UINT StartRegister,int* pConstantData,UINT Vector4iCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetPixelShaderConstantI");
  return gcpRendD3D->m_pActuald3dDevice->GetPixelShaderConstantI(StartRegister,pConstantData,Vector4iCount);
}
HRESULT CMyDirect3DDevice9::SetPixelShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::SetPixelShaderConstantB");
  return gcpRendD3D->m_pActuald3dDevice->SetPixelShaderConstantB(StartRegister,pConstantData,BoolCount);
}
HRESULT CMyDirect3DDevice9::GetPixelShaderConstantB(UINT StartRegister,BOOL* pConstantData,UINT BoolCount)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::GetPixelShaderConstantB");
  return gcpRendD3D->m_pActuald3dDevice->GetPixelShaderConstantB(StartRegister,pConstantData,BoolCount);
}
#if !defined(XENON) && !defined(PS3)
HRESULT CMyDirect3DDevice9::DrawRectPatch(UINT Handle,CONST float* pNumSegs,CONST D3DRECTPATCH_INFO* pRectPatchInfo)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::DrawRectPatch");
  return gcpRendD3D->m_pActuald3dDevice->DrawRectPatch(Handle,pNumSegs,pRectPatchInfo);
}
HRESULT CMyDirect3DDevice9::DrawTriPatch(UINT Handle,CONST float* pNumSegs,CONST D3DTRIPATCH_INFO* pTriPatchInfo)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::DrawTriPatch");
  return gcpRendD3D->m_pActuald3dDevice->DrawTriPatch(Handle,pNumSegs,pTriPatchInfo);
}
HRESULT CMyDirect3DDevice9::DeletePatch(UINT Handle)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::DeletePatch");
  return gcpRendD3D->m_pActuald3dDevice->DeletePatch(Handle);
}
#endif
HRESULT CMyDirect3DDevice9::CreateQuery(D3DQUERYTYPE Type,IDirect3DQuery9** ppQuery)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::CreateQuery");
  return gcpRendD3D->m_pActuald3dDevice->CreateQuery(Type, ppQuery);
}

//==============================================================================================

//void glSetLogFuncs(bool bSet);
#elif defined (DIRECT3D10)

static char *sD3DFMT (DXGI_FORMAT Value)
{
  switch (Value)
  {
    ecase(DXGI_FORMAT_R32G32B32A32_TYPELESS);
    ecase(DXGI_FORMAT_R32G32B32A32_FLOAT);
    ecase(DXGI_FORMAT_R32G32B32A32_UINT);
    ecase(DXGI_FORMAT_R32G32B32A32_SINT);
    ecase(DXGI_FORMAT_R32G32B32_TYPELESS);
    ecase(DXGI_FORMAT_R32G32B32_FLOAT);
    ecase(DXGI_FORMAT_R32G32B32_UINT);
    ecase(DXGI_FORMAT_R32G32B32_SINT);
    ecase(DXGI_FORMAT_R16G16B16A16_TYPELESS);
    ecase(DXGI_FORMAT_R16G16B16A16_FLOAT);
    ecase(DXGI_FORMAT_R16G16B16A16_UNORM);
    ecase(DXGI_FORMAT_R16G16B16A16_UINT);
    ecase(DXGI_FORMAT_R16G16B16A16_SNORM);
    ecase(DXGI_FORMAT_R16G16B16A16_SINT);
    ecase(DXGI_FORMAT_R32G32_TYPELESS);
    ecase(DXGI_FORMAT_R32G32_FLOAT);
    ecase(DXGI_FORMAT_R32G32_UINT);
    ecase(DXGI_FORMAT_R32G32_SINT);
    ecase(DXGI_FORMAT_R32G8X24_TYPELESS);
    ecase(DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    ecase(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS);
    ecase(DXGI_FORMAT_X32_TYPELESS_G8X24_UINT);
    ecase(DXGI_FORMAT_R10G10B10A2_TYPELESS);
    ecase(DXGI_FORMAT_R10G10B10A2_UNORM);
    ecase(DXGI_FORMAT_R10G10B10A2_UINT);
    ecase(DXGI_FORMAT_R11G11B10_FLOAT);
    ecase(DXGI_FORMAT_R8G8B8A8_TYPELESS);
    ecase(DXGI_FORMAT_R8G8B8A8_UNORM);
    ecase(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    ecase(DXGI_FORMAT_R8G8B8A8_UINT);
    ecase(DXGI_FORMAT_R8G8B8A8_SNORM);
    ecase(DXGI_FORMAT_R8G8B8A8_SINT);
    ecase(DXGI_FORMAT_R16G16_TYPELESS);
    ecase(DXGI_FORMAT_R16G16_FLOAT);
    ecase(DXGI_FORMAT_R16G16_UNORM);
    ecase(DXGI_FORMAT_R16G16_UINT);
    ecase(DXGI_FORMAT_R16G16_SNORM);
    ecase(DXGI_FORMAT_R16G16_SINT);
    ecase(DXGI_FORMAT_R32_TYPELESS);
    ecase(DXGI_FORMAT_D32_FLOAT);
    ecase(DXGI_FORMAT_R32_FLOAT);
    ecase(DXGI_FORMAT_R32_UINT);
    ecase(DXGI_FORMAT_R32_SINT);
    ecase(DXGI_FORMAT_R24G8_TYPELESS);
    ecase(DXGI_FORMAT_D24_UNORM_S8_UINT);
    ecase(DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
    ecase(DXGI_FORMAT_X24_TYPELESS_G8_UINT);
    ecase(DXGI_FORMAT_R8G8_TYPELESS);
    ecase(DXGI_FORMAT_R8G8_UNORM);
    ecase(DXGI_FORMAT_R8G8_UINT);
    ecase(DXGI_FORMAT_R8G8_SNORM);
    ecase(DXGI_FORMAT_R8G8_SINT);
    ecase(DXGI_FORMAT_R16_TYPELESS);
    ecase(DXGI_FORMAT_R16_FLOAT);
    ecase(DXGI_FORMAT_D16_UNORM);
    ecase(DXGI_FORMAT_R16_UNORM);
    ecase(DXGI_FORMAT_R16_UINT);
    ecase(DXGI_FORMAT_R16_SNORM);
    ecase(DXGI_FORMAT_R16_SINT);
    ecase(DXGI_FORMAT_R8_TYPELESS);
    ecase(DXGI_FORMAT_R8_UNORM);
    ecase(DXGI_FORMAT_R8_UINT);
    ecase(DXGI_FORMAT_R8_SNORM);
    ecase(DXGI_FORMAT_R8_SINT);
    ecase(DXGI_FORMAT_A8_UNORM);
    ecase(DXGI_FORMAT_R1_UNORM);
    ecase(DXGI_FORMAT_R9G9B9E5_SHAREDEXP);
    ecase(DXGI_FORMAT_R8G8_B8G8_UNORM);
    ecase(DXGI_FORMAT_G8R8_G8B8_UNORM);
    ecase(DXGI_FORMAT_BC1_TYPELESS);
    ecase(DXGI_FORMAT_BC1_UNORM);
    ecase(DXGI_FORMAT_BC1_UNORM_SRGB);
    ecase(DXGI_FORMAT_BC2_TYPELESS);
    ecase(DXGI_FORMAT_BC2_UNORM);
    ecase(DXGI_FORMAT_BC2_UNORM_SRGB);
    ecase(DXGI_FORMAT_BC3_TYPELESS);
    ecase(DXGI_FORMAT_BC3_UNORM);
    ecase(DXGI_FORMAT_BC3_UNORM_SRGB);
    ecase(DXGI_FORMAT_BC4_TYPELESS);
    ecase(DXGI_FORMAT_BC4_UNORM);
    ecase(DXGI_FORMAT_BC4_SNORM);
    ecase(DXGI_FORMAT_BC5_TYPELESS);
    ecase(DXGI_FORMAT_BC5_UNORM);
    ecase(DXGI_FORMAT_BC5_SNORM);
    ecase(DXGI_FORMAT_B5G6R5_UNORM);
    ecase(DXGI_FORMAT_B5G5R5A1_UNORM);
    ecase(DXGI_FORMAT_B8G8R8A8_UNORM);
    ecase(DXGI_FORMAT_B8G8R8X8_UNORM);
    edefault (Value);
  }
}

static char *sD3DPRIM_TOPLOGY(D3D10_PRIMITIVE_TOPOLOGY Topology)
{
  switch (Topology)
  {
    ecase(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
    ecase(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
    ecase(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
    ecase(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ecase(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ecase(D3D10_PRIMITIVE_TOPOLOGY_LINELIST_ADJ);
    ecase(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ);
    ecase(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ);
    ecase(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ);
    edefault (Topology);
  }
}

static char *sD3DBLEND (DWORD Value)
{
  switch (Value)
  {
    ecase (D3D10_BLEND_ZERO);
    ecase (D3D10_BLEND_ONE);
    ecase (D3D10_BLEND_SRC_COLOR);
    ecase (D3D10_BLEND_DEST_COLOR);
    ecase (D3D10_BLEND_SRC_ALPHA);
    ecase (D3D10_BLEND_SRC_ALPHA_SAT);
    ecase (D3D10_BLEND_DEST_ALPHA);
    ecase (D3D10_BLEND_INV_SRC_COLOR);
    ecase (D3D10_BLEND_INV_SRC_ALPHA);
    ecase (D3D10_BLEND_INV_DEST_ALPHA);
    ecase (D3D10_BLEND_INV_DEST_COLOR);
    ecase (D3D10_BLEND_BLEND_FACTOR);
    ecase (D3D10_BLEND_INV_BLEND_FACTOR);
    ecase (D3D10_BLEND_SRC1_COLOR);
    ecase (D3D10_BLEND_INV_SRC1_COLOR);
    ecase (D3D10_BLEND_SRC1_ALPHA);
    ecase (D3D10_BLEND_INV_SRC1_ALPHA);
    edefault ((uint32)Value);
  }
}

static char *sD3DCompareFunc (DWORD Value)
{
  switch (Value)
  {
    ecase (D3D10_COMPARISON_NEVER);
    ecase (D3D10_COMPARISON_LESS);
    ecase (D3D10_COMPARISON_EQUAL);
    ecase (D3D10_COMPARISON_LESS_EQUAL);
    ecase (D3D10_COMPARISON_GREATER);
    ecase (D3D10_COMPARISON_NOT_EQUAL);
    ecase (D3D10_COMPARISON_GREATER_EQUAL);
    ecase (D3D10_COMPARISON_ALWAYS);
    edefault ((uint32)Value);
  }
}

static char *sD3DStencilOp (DWORD Value)
{
  switch (Value)
  {
    ecase (D3D10_STENCIL_OP_KEEP);
    ecase (D3D10_STENCIL_OP_ZERO);
    ecase (D3D10_STENCIL_OP_REPLACE);
    ecase (D3D10_STENCIL_OP_INCR_SAT);
    ecase (D3D10_STENCIL_OP_DECR_SAT);
    ecase (D3D10_STENCIL_OP_INVERT);
    ecase (D3D10_STENCIL_OP_INCR);
    ecase (D3D10_STENCIL_OP_DECR);
    edefault ((uint32)Value);
  }
}


static char *sD3DCull (DWORD Value)
{
  switch (Value)
  {
    ecase (D3D10_CULL_BACK);
    ecase (D3D10_CULL_FRONT);
    ecase (D3D10_CULL_NONE);
    edefault ((uint32)Value);
  }
}

static char *sD3DTAddress (D3D10_TEXTURE_ADDRESS_MODE Value)
{
  switch (Value)
  {
    ecase (D3D10_TEXTURE_ADDRESS_CLAMP);
    ecase (D3D10_TEXTURE_ADDRESS_WRAP);
    ecase (D3D10_TEXTURE_ADDRESS_MIRROR);
    ecase (D3D10_TEXTURE_ADDRESS_BORDER);
    ecase (D3D10_TEXTURE_ADDRESS_MIRROR_ONCE);
    edefault (Value);
  }
}

static char *sD3DTFilter (D3D10_FILTER Value)
{
  switch (Value)
  {
    ecase (D3D10_FILTER_MIN_MAG_MIP_POINT);
    ecase (D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR);
    ecase (D3D10_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT);
    ecase (D3D10_FILTER_MIN_POINT_MAG_MIP_LINEAR);
    ecase (D3D10_FILTER_MIN_LINEAR_MAG_MIP_POINT);
    ecase (D3D10_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR);
    ecase (D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT);
    ecase (D3D10_FILTER_MIN_MAG_MIP_LINEAR);
    ecase (D3D10_FILTER_ANISOTROPIC);
    ecase (D3D10_FILTER_COMPARISON_MIN_MAG_MIP_POINT);
    ecase (D3D10_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR);
    ecase (D3D10_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT);
    ecase (D3D10_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR);
    ecase (D3D10_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT);
    ecase (D3D10_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR);
    ecase (D3D10_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT);
    ecase (D3D10_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);
    ecase (D3D10_FILTER_COMPARISON_ANISOTROPIC);
    edefault (Value);
  }
}

HRESULT CMyDirect3DDevice10::QueryInterface(REFIID riid, void** ppvObj)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::QueryInterface");
  return gcpRendD3D->m_pActuald3dDevice->QueryInterface(riid, ppvObj);
}
ULONG CMyDirect3DDevice10::AddRef()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::AddRef");
  return gcpRendD3D->m_pActuald3dDevice->AddRef();
}
ULONG CMyDirect3DDevice10::Release()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s\n", "D3DDevice::Release");
  return gcpRendD3D->m_pActuald3dDevice->Release();
}

void CMyDirect3DDevice10::VSSetConstantBuffers( 
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][in] */ ID3D10Buffer *const *ppConstantBuffers)
{
  int nBytes = 0;
  if (ppConstantBuffers && *ppConstantBuffers)
  {
    D3D10_BUFFER_DESC Desc;
    (*ppConstantBuffers)->GetDesc(&Desc);
    nBytes = Desc.ByteWidth;
  }
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, 0x%x) (%d bytes)\n", "D3DDevice::VSSetConstantBuffers", StartConstantSlot, NumBuffers, *ppConstantBuffers, nBytes);
  gcpRendD3D->m_pActuald3dDevice->VSSetConstantBuffers(StartConstantSlot, NumBuffers, ppConstantBuffers);
}

void CMyDirect3DDevice10::PSSetShaderResources( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][in] */ ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, 0x%x)\n", "D3DDevice::PSSetShaderResources", Offset, NumViews, *ppShaderResourceViews);
  gcpRendD3D->m_pActuald3dDevice->PSSetShaderResources(Offset, NumViews, ppShaderResourceViews);
}

void CMyDirect3DDevice10::PSSetShader( 
  /* [in] */ ID3D10PixelShader *pPixelShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::PSSetShader", pPixelShader);
  gcpRendD3D->m_pActuald3dDevice->PSSetShader(pPixelShader);
}

void CMyDirect3DDevice10::PSSetSamplers( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][in] */ ID3D10SamplerState *const *ppSamplers)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, 0x%x)\n", "D3DDevice::PSSetSamplers", Offset, NumSamplers, *ppSamplers);
  D3D10_SAMPLER_DESC Desc;
  if (ppSamplers && *ppSamplers)
  {
    (*ppSamplers)->GetDesc(&Desc);
    gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  AddressU: %s\n", sD3DTAddress(Desc.AddressU));
    gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  AddressV: %s\n", sD3DTAddress(Desc.AddressV));
    gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  AddressW: %s\n", sD3DTAddress(Desc.AddressW));
    gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  Filter: %s\n", sD3DTFilter(Desc.Filter));
    if (Desc.Filter == D3D10_FILTER_ANISOTROPIC || Desc.Filter == D3D10_FILTER_COMPARISON_ANISOTROPIC)
      gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  MaxAnisotropy: %d\n", Desc.MaxAnisotropy);
  }

  gcpRendD3D->m_pActuald3dDevice->PSSetSamplers(Offset, NumSamplers, ppSamplers);
}

void CMyDirect3DDevice10::VSSetShader( 
  /* [in] */ ID3D10VertexShader *pVertexShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::VSSetShader", pVertexShader);
  gcpRendD3D->m_pActuald3dDevice->VSSetShader(pVertexShader);
}

void CMyDirect3DDevice10::DrawIndexed( 
  /* [in] */ UINT IndexCount,
  /* [in] */ UINT StartIndexLocation,
  /* [in] */ INT BaseVertexLocation)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, %d)\n", "D3DDevice::DrawIndexed", IndexCount, StartIndexLocation, BaseVertexLocation);
  gcpRendD3D->m_pActuald3dDevice->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}


void CMyDirect3DDevice10::Draw( 
                                    /* [in] */ UINT VertexCount,
                                    /* [in] */ UINT StartVertexLocation)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::Draw", VertexCount, StartVertexLocation);
  gcpRendD3D->m_pActuald3dDevice->Draw(VertexCount, StartVertexLocation);
}


void CMyDirect3DDevice10::PSSetConstantBuffers( 
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][in] */ ID3D10Buffer *const *ppConstantBuffers)
{
  int nBytes = 0;
  if (ppConstantBuffers && *ppConstantBuffers)
  {
    D3D10_BUFFER_DESC Desc;
    (*ppConstantBuffers)->GetDesc(&Desc);
    nBytes = Desc.ByteWidth;
  }
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, 0x%x) (%d bytes)\n", "D3DDevice::PSSetConstantBuffers", StartConstantSlot, NumBuffers, *ppConstantBuffers, nBytes);
  gcpRendD3D->m_pActuald3dDevice->PSSetConstantBuffers(StartConstantSlot, NumBuffers, ppConstantBuffers);
}

void CMyDirect3DDevice10::IASetInputLayout( 
  /* [in] */ ID3D10InputLayout *pInputLayout)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::IASetInputLayout", pInputLayout);
  gcpRendD3D->m_pActuald3dDevice->IASetInputLayout(pInputLayout);
}

void CMyDirect3DDevice10::IASetVertexBuffers( 
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][in] */ ID3D10Buffer *const *ppVertexBuffers,
  /* [size_is][in] */ const UINT *pStrides,
  /* [size_is][in] */ const UINT *pOffsets)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, 0x%x, %d, %d)\n", "D3DDevice::IASetVertexBuffers", StartSlot, NumBuffers, *ppVertexBuffers, *pStrides, *pOffsets);
  gcpRendD3D->m_pActuald3dDevice->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

void CMyDirect3DDevice10::IASetIndexBuffer( 
  /* [in] */ ID3D10Buffer *pIndexBuffer,
  /* [in] */ DXGI_FORMAT Format,
  /* [in] */ UINT Offset)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, %s, %d)\n", "D3DDevice::IASetIndexBuffer", pIndexBuffer, sD3DFMT(Format), Offset);
  gcpRendD3D->m_pActuald3dDevice->IASetIndexBuffer(pIndexBuffer, Format, Offset);
}

void CMyDirect3DDevice10::DrawIndexedInstanced( 
  /* [in] */ UINT IndexCountPerInstance,
  /* [in] */ UINT InstanceCount,
  /* [in] */ UINT StartIndexLocation,
  /* [in] */ INT BaseVertexLocation,
  /* [in] */ UINT StartInstanceLocation)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, %d, %d, %d)\n", "D3DDevice::DrawIndexedInstanced", IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
  gcpRendD3D->m_pActuald3dDevice->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void CMyDirect3DDevice10::DrawInstanced( 
  /* [in] */ UINT VertexCountPerInstance,
  /* [in] */ UINT InstanceCount,
  /* [in] */ UINT StartVertexLocation,
  /* [in] */ UINT StartInstanceLocation)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, %d, %d)\n", "D3DDevice::DrawInstanced", VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
  gcpRendD3D->m_pActuald3dDevice->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void CMyDirect3DDevice10::GSSetConstantBuffers( 
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][in] */ ID3D10Buffer *const *ppConstantBuffers)
{
  int nBytes = 0;
  if (ppConstantBuffers && *ppConstantBuffers)
  {
    D3D10_BUFFER_DESC Desc;
    (*ppConstantBuffers)->GetDesc(&Desc);
    nBytes = Desc.ByteWidth;
  }
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, 0x%x) (%d bytes)\n", "D3DDevice::GSSetConstantBuffers", StartConstantSlot, NumBuffers, *ppConstantBuffers, nBytes);
  gcpRendD3D->m_pActuald3dDevice->GSSetConstantBuffers(StartConstantSlot, NumBuffers, ppConstantBuffers);
}

void CMyDirect3DDevice10::GSSetShader( 
  /* [in] */ ID3D10GeometryShader *pShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::GSSetShader", pShader);
  gcpRendD3D->m_pActuald3dDevice->GSSetShader(pShader);
}

void CMyDirect3DDevice10::IASetPrimitiveTopology( 
  /* [in] */ D3D10_PRIMITIVE_TOPOLOGY Topology)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%s)\n", "D3DDevice::IASetPrimitiveTopology", sD3DPRIM_TOPLOGY(Topology));
  gcpRendD3D->m_pActuald3dDevice->IASetPrimitiveTopology(Topology);
}

void CMyDirect3DDevice10::VSSetShaderResources( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][in] */ ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, 0x%x)\n", "D3DDevice::VSSetShaderResources", Offset, NumViews, *ppShaderResourceViews);
  gcpRendD3D->m_pActuald3dDevice->VSSetShaderResources(Offset, NumViews, ppShaderResourceViews);
}

void CMyDirect3DDevice10::VSSetSamplers( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][in] */ ID3D10SamplerState *const *ppSamplers)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, 0x%x)\n", "D3DDevice::VSSetSamplers", Offset, NumSamplers, *ppSamplers);
  gcpRendD3D->m_pActuald3dDevice->VSSetSamplers(Offset, NumSamplers, ppSamplers);
}

void CMyDirect3DDevice10::SetPredication( 
  /* [in] */ ID3D10Predicate *pPredicate,
  /* [in] */ BOOL PredicateValue)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, %d)\n", "D3DDevice::SetPredication", pPredicate, PredicateValue);
  gcpRendD3D->m_pActuald3dDevice->SetPredication(pPredicate, PredicateValue);
}

void CMyDirect3DDevice10::GSSetShaderResources( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][in] */ ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, 0x%x)\n", "D3DDevice::GSSetShaderResources", Offset, NumViews, *ppShaderResourceViews);
  gcpRendD3D->m_pActuald3dDevice->GSSetShaderResources(Offset, NumViews, ppShaderResourceViews);
}

void CMyDirect3DDevice10::GSSetSamplers( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][in] */ ID3D10SamplerState *const *ppSamplers)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, 0x%x)\n", "D3DDevice::GSSetSamplers", Offset, NumSamplers, *ppSamplers);
  gcpRendD3D->m_pActuald3dDevice->GSSetSamplers(Offset, NumSamplers, ppSamplers);
}

void CMyDirect3DDevice10::OMSetRenderTargets( 
  /* [in] */ UINT NumViews,
  /* [size_is][in] */ ID3D10RenderTargetView *const *ppRenderTargetViews,
  /* [in] */ ID3D10DepthStencilView *pDepthStencilView)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d, 0x%x)\n", "D3DDevice::OMSetRenderTargets", NumViews, *ppRenderTargetViews, pDepthStencilView);
  gcpRendD3D->m_pActuald3dDevice->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
}

void CMyDirect3DDevice10::OMSetBlendState( 
  /* [in] */ ID3D10BlendState *pBlendState,
  /* [in] */ const FLOAT BlendFactor[ 4 ],
  /* [in] */ UINT SampleMask)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::OMSetBlendState", pBlendState);
  D3D10_BLEND_DESC Desc;
  pBlendState->GetDesc(&Desc);

  if (!Desc.BlendEnable[0])
     gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  Blending: Disabled\n");
  else
    gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  Blending: (%s, %s)\n", sD3DBLEND(Desc.SrcBlend), sD3DBLEND(Desc.DestBlend));

  for (int i=1;i<8;i++)
  {
    if (Desc.BlendEnable[i])
      gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "RT:  Blending: (%s, %s)\n", i, sD3DBLEND(Desc.SrcBlend), sD3DBLEND(Desc.DestBlend));
  }

  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  WriteMask: %d\n", Desc.RenderTargetWriteMask[0]);
  gcpRendD3D->m_pActuald3dDevice->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}

void CMyDirect3DDevice10::OMSetDepthStencilState( 
  /* [in] */ ID3D10DepthStencilState *pDepthStencilState,
  /* [in] */ UINT StencilRef)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::OMSetDepthStencilState", pDepthStencilState);
  D3D10_DEPTH_STENCIL_DESC Desc;
  pDepthStencilState->GetDesc(&Desc);
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  DepthTest: %s\n", Desc.DepthEnable ? "TRUE" : "FALSE");
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  DepthWrite: %s\n", Desc.DepthWriteMask ? "TRUE" : "FALSE");
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  DepthFunc: %s\n", sD3DCompareFunc(Desc.DepthFunc));

  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  StencilEnable: %s\n", Desc.StencilEnable ? "TRUE" : "FALSE");

  if (Desc.StencilEnable)
  {
    gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  StencilRef: 0x%x\n", StencilRef );
    gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  StencilReadMask: 0x%x\n", Desc.StencilReadMask);
    gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "  StencilWriteMask: 0x%x\n", Desc.StencilWriteMask );


    gcpRendD3D->Logv( SRendItem::m_RecurseLevel, "  FrontFace Stencil: \n");
    gcpRendD3D->Logv( SRendItem::m_RecurseLevel, "    StencilFailOp: %s\n", sD3DStencilOp(Desc.FrontFace.StencilFailOp) );
    gcpRendD3D->Logv( SRendItem::m_RecurseLevel, "    StencilDepthFailOp: %s\n", sD3DStencilOp(Desc.FrontFace.StencilDepthFailOp) );
    gcpRendD3D->Logv( SRendItem::m_RecurseLevel, "    StencilPassOp: %s\n", sD3DStencilOp(Desc.FrontFace.StencilPassOp) );
    gcpRendD3D->Logv( SRendItem::m_RecurseLevel, "    StencilFunc: %s\n", sD3DCompareFunc(Desc.FrontFace.StencilFunc) );

    gcpRendD3D->Logv( SRendItem::m_RecurseLevel, "  BackFace Stencil: \n");
    gcpRendD3D->Logv( SRendItem::m_RecurseLevel, "    StencilFailOp: %s\n", sD3DStencilOp(Desc.BackFace.StencilFailOp) );
    gcpRendD3D->Logv( SRendItem::m_RecurseLevel, "    StencilDepthFailOp: %s\n", sD3DStencilOp(Desc.BackFace.StencilDepthFailOp) );
    gcpRendD3D->Logv( SRendItem::m_RecurseLevel, "    StencilPassOp: %s\n", sD3DStencilOp(Desc.BackFace.StencilPassOp) );
    gcpRendD3D->Logv( SRendItem::m_RecurseLevel, "    StencilFunc: %s\n", sD3DCompareFunc(Desc.BackFace.StencilFunc) );
  }

  gcpRendD3D->m_pActuald3dDevice->OMSetDepthStencilState(pDepthStencilState, StencilRef);
}

void CMyDirect3DDevice10::SOSetTargets( 
  /* [in] */ UINT NumBuffers,
  /* [size_is][in] */ ID3D10Buffer *const *ppSOTargets,
  /* [size_is][in] */ const UINT *pOffsets)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, 0x%x, %d)\n", "D3DDevice::SOSetTargets", NumBuffers, *ppSOTargets, *pOffsets);
  gcpRendD3D->m_pActuald3dDevice->SOSetTargets(NumBuffers, ppSOTargets, (UINT*)pOffsets);
}

void CMyDirect3DDevice10::DrawAuto( void)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::DrawAuto");
  gcpRendD3D->m_pActuald3dDevice->DrawAuto();
}

void CMyDirect3DDevice10::RSSetState( 
  /* [in] */ ID3D10RasterizerState *pRasterizerState)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::RSSetState", pRasterizerState);
  D3D10_RASTERIZER_DESC Desc;
  pRasterizerState->GetDesc(&Desc);
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "   CullMode: %s\n", sD3DCull(Desc.CullMode));
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "   Scissor: %s\n", Desc.ScissorEnable ? "TRUE" : "FALSE");
  gcpRendD3D->m_pActuald3dDevice->RSSetState(pRasterizerState);
}

void CMyDirect3DDevice10::RSSetViewports( 
  /* [in] */ UINT NumViewports,
  /* [size_is][in] */ const D3D10_VIEWPORT *pViewports)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, 0x%x) (%d, %d, %d, %d [%.2f, %.2f])\n", "D3DDevice::RSSetViewports", NumViewports, pViewports, pViewports->TopLeftX, pViewports->TopLeftY, pViewports->Width, pViewports->Height, pViewports->MinDepth, pViewports->MaxDepth);
  gcpRendD3D->m_pActuald3dDevice->RSSetViewports(NumViewports, pViewports);
}

void CMyDirect3DDevice10::RSSetScissorRects( 
  /* [in] */ UINT NumRects,
  /* [size_is][in] */ const D3D10_RECT *pRects)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, 0x%x)\n", "D3DDevice::RSSetScissorRects", NumRects, pRects);
  gcpRendD3D->m_pActuald3dDevice->RSSetScissorRects(NumRects, pRects);
}

void CMyDirect3DDevice10::ClearRenderTargetView( 
  /* [in] */ ID3D10RenderTargetView *pRenderTargetView,
  /* [in] */ const FLOAT ColorRGBA[ 4 ])
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x (%f, %f, %f, %f))\n", "D3DDevice::ClearRenderTargetView", pRenderTargetView, ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);
  gcpRendD3D->m_pActuald3dDevice->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}

void CMyDirect3DDevice10::ClearDepthStencilView( 
  /* [in] */ ID3D10DepthStencilView *pDepthStencilView,
  /* [in] */ UINT Flags,
  /* [in] */ FLOAT Depth,
  /* [in] */ UINT8 Stencil)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, 0x%x, %f, %d)\n", "D3DDevice::ClearDepthStencilView", pDepthStencilView, Flags, Depth, Stencil);
  gcpRendD3D->m_pActuald3dDevice->ClearDepthStencilView(pDepthStencilView, Flags, Depth, Stencil);
}

void CMyDirect3DDevice10::GenerateMips( 
  /* [in] */ ID3D10ShaderResourceView *pShaderResourceView)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::GenerateMips", pShaderResourceView);
  gcpRendD3D->m_pActuald3dDevice->GenerateMips(pShaderResourceView);
}

void CMyDirect3DDevice10::VSGetConstantBuffers( 
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][out] */ ID3D10Buffer **ppConstantBuffers)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::VSGetConstantBuffers", StartConstantSlot, NumBuffers);
  gcpRendD3D->m_pActuald3dDevice->VSGetConstantBuffers(StartConstantSlot, NumBuffers, ppConstantBuffers);
}

void CMyDirect3DDevice10::PSGetShaderResources( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][out] */ ID3D10ShaderResourceView **ppShaderResourceViews)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::PSGetShaderResources", Offset, NumViews);
  gcpRendD3D->m_pActuald3dDevice->PSGetShaderResources(Offset, NumViews, ppShaderResourceViews);
}

void CMyDirect3DDevice10::PSGetShader( 
  /* [out][in] */ ID3D10PixelShader **ppPixelShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::PSGetShader");
  gcpRendD3D->m_pActuald3dDevice->PSGetShader(ppPixelShader);
}

void CMyDirect3DDevice10::PSGetSamplers( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][out] */ ID3D10SamplerState **ppSamplers)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::PSGetSamplers", Offset, NumSamplers);
  gcpRendD3D->m_pActuald3dDevice->PSGetSamplers(Offset, NumSamplers, ppSamplers);
}

void CMyDirect3DDevice10::VSGetShader( 
  /* [out][in] */ ID3D10VertexShader **ppVertexShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::VSGetShader");
  gcpRendD3D->m_pActuald3dDevice->VSGetShader(ppVertexShader);
}

void CMyDirect3DDevice10::PSGetConstantBuffers( 
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][out] */ ID3D10Buffer **ppConstantBuffers)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::PSGetConstantBuffers", StartConstantSlot, NumBuffers);
  gcpRendD3D->m_pActuald3dDevice->PSGetConstantBuffers(StartConstantSlot, NumBuffers, ppConstantBuffers);
}

void CMyDirect3DDevice10::IAGetInputLayout( 
  /* [out][in] */ ID3D10InputLayout **ppInputLayout)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::IAGetInputLayout");
  gcpRendD3D->m_pActuald3dDevice->IAGetInputLayout(ppInputLayout);
}

void CMyDirect3DDevice10::IAGetVertexBuffers( 
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][out] */ ID3D10Buffer **ppVertexBuffers,
  /* [size_is][out] */ UINT *pStrides,
  /* [size_is][out] */ UINT *pOffsets)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::IAGetVertexBuffers", StartSlot, NumBuffers);
  gcpRendD3D->m_pActuald3dDevice->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

void CMyDirect3DDevice10::IAGetIndexBuffer( 
  /* [out] */ ID3D10Buffer **pIndexBuffer,
  /* [out] */ DXGI_FORMAT *Format,
  /* [out] */ UINT *Offset)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::IAGetIndexBuffer");
  gcpRendD3D->m_pActuald3dDevice->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
}

void CMyDirect3DDevice10::GSGetConstantBuffers( 
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][out] */ ID3D10Buffer **ppConstantBuffers)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::GSGetConstantBuffers", StartConstantSlot, NumBuffers);
  gcpRendD3D->m_pActuald3dDevice->GSGetConstantBuffers(StartConstantSlot, NumBuffers, ppConstantBuffers);
}

void CMyDirect3DDevice10::GSGetShader( 
  /* [out] */ ID3D10GeometryShader **ppGeometryShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::GSGetShader");
  gcpRendD3D->m_pActuald3dDevice->GSGetShader(ppGeometryShader);
}

void CMyDirect3DDevice10::IAGetPrimitiveTopology( 
  /* [out] */ D3D10_PRIMITIVE_TOPOLOGY *pTopology)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::IAGetPrimitiveTopology");
  gcpRendD3D->m_pActuald3dDevice->IAGetPrimitiveTopology(pTopology);
}

void CMyDirect3DDevice10::VSGetShaderResources( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][out] */ ID3D10ShaderResourceView **ppShaderResourceViews)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::VSGetShaderResources", Offset, NumViews);
  gcpRendD3D->m_pActuald3dDevice->VSGetShaderResources(Offset, NumViews, ppShaderResourceViews);
}

void CMyDirect3DDevice10::VSGetSamplers( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][out] */ ID3D10SamplerState **ppSamplers)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::VSGetSamplers", Offset, NumSamplers);
  gcpRendD3D->m_pActuald3dDevice->VSGetSamplers(Offset, NumSamplers, ppSamplers);
}

void CMyDirect3DDevice10::GetPredication( 
  /* [out] */ ID3D10Predicate **ppPredicate,
  /* [out] */ BOOL *pPredicateValue)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::GetPredication");
  gcpRendD3D->m_pActuald3dDevice->GetPredication(ppPredicate, pPredicateValue);
}

void CMyDirect3DDevice10::GSGetShaderResources( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][out] */ ID3D10ShaderResourceView **ppShaderResourceViews)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::GSGetShaderResources", Offset, NumViews);
  gcpRendD3D->m_pActuald3dDevice->GSGetShaderResources(Offset, NumViews, ppShaderResourceViews);
}

void CMyDirect3DDevice10::GSGetSamplers( 
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][out] */ ID3D10SamplerState **ppSamplers)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::GSGetSamplers", Offset, NumSamplers);
  gcpRendD3D->m_pActuald3dDevice->GSGetSamplers(Offset, NumSamplers, ppSamplers);
}

void CMyDirect3DDevice10::OMGetRenderTargets( 
  /* [in] */ UINT NumViews,
  /* [size_is][out] */ ID3D10RenderTargetView **ppRenderTargetViews,
  /* [out] */ ID3D10DepthStencilView **ppDepthStencilView)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d)\n", "D3DDevice::OMGetRenderTargets", NumViews);
  gcpRendD3D->m_pActuald3dDevice->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
}

void CMyDirect3DDevice10::OMGetBlendState( 
  /* [out] */ ID3D10BlendState **ppBlendState,
  /* [out] */ FLOAT BlendFactor[ 4 ],
  /* [out] */ UINT *pSampleMask)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::OMGetBlendState");
  gcpRendD3D->m_pActuald3dDevice->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
}

void CMyDirect3DDevice10::OMGetDepthStencilState( 
  /* [out] */ ID3D10DepthStencilState **ppDepthStencilState,
  /* [out] */ UINT *pStencilRef)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::OMGetDepthStencilState");
  gcpRendD3D->m_pActuald3dDevice->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
}

void CMyDirect3DDevice10::SOGetTargets( 
  /* [in] */ UINT NumBuffers,
  /* [size_is][out] */ ID3D10Buffer **ppSOTargets,
  /* [size_is][out] */ UINT *pOffsets)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d)\n", "D3DDevice::SOGetTargets", NumBuffers);
  gcpRendD3D->m_pActuald3dDevice->SOGetTargets(NumBuffers, ppSOTargets, pOffsets);
}

void CMyDirect3DDevice10::RSGetState( 
  /* [out] */ ID3D10RasterizerState **ppRasterizerState)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::RSGetState");
  gcpRendD3D->m_pActuald3dDevice->RSGetState(ppRasterizerState);
}

void CMyDirect3DDevice10::RSGetViewports( 
  /* [out][in] */ UINT *NumViewports,
  /* [size_is][out] */ D3D10_VIEWPORT *pViewports)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::RSGetViewports");
  gcpRendD3D->m_pActuald3dDevice->RSGetViewports(NumViewports, pViewports);
}

void CMyDirect3DDevice10::RSGetScissorRects( 
  /* [out][in] */ UINT *NumRects,
  /* [size_is][out] */ D3D10_RECT *pRects)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::RSGetScissorRects");
  gcpRendD3D->m_pActuald3dDevice->RSGetScissorRects(NumRects, pRects);
}

HRESULT CMyDirect3DDevice10::GetDeviceRemovedReason( void)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::GetDeviceRemovedReason");
  return gcpRendD3D->m_pActuald3dDevice->GetDeviceRemovedReason();
}

HRESULT CMyDirect3DDevice10::SetExceptionMode( 
  UINT RaiseFlags)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::SetExceptionMode", RaiseFlags);
  return gcpRendD3D->m_pActuald3dDevice->SetExceptionMode(RaiseFlags);
}

UINT CMyDirect3DDevice10::GetExceptionMode( void)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::GetExceptionMode");
  return gcpRendD3D->m_pActuald3dDevice->GetExceptionMode();
}

HRESULT CMyDirect3DDevice10::GetPrivateData( 
  /* [in] */ REFGUID guid,
  /* [out][in] */ UINT *pDataSize,
  /* [size_is][out] */ void *pData)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::GetPrivateData");
  return gcpRendD3D->m_pActuald3dDevice->GetPrivateData(guid, pDataSize, pData);
}

HRESULT CMyDirect3DDevice10::SetPrivateData( 
  /* [in] */ REFGUID guid,
  /* [in] */ UINT DataSize,
  /* [size_is][in] */ const void *pData)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::SetPrivateData");
  return gcpRendD3D->m_pActuald3dDevice->SetPrivateData(guid, DataSize, pData);
}

HRESULT CMyDirect3DDevice10::SetPrivateDataInterface( 
  /* [in] */ REFGUID guid,
  /* [in] */ const IUnknown *pData)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::SetPrivateDataInterface");
  return gcpRendD3D->m_pActuald3dDevice->SetPrivateDataInterface(guid, pData);
}

void CMyDirect3DDevice10::Flush( void)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::Flush");
  gcpRendD3D->m_pActuald3dDevice->Flush();
}

HRESULT CMyDirect3DDevice10::CreateBuffer( 
  /* [in] */ const D3D10_BUFFER_DESC *pDesc,
  /* [in] */ const D3D10_SUBRESOURCE_DATA *pInitialData,
  /* [out] */ ID3D10Buffer **ppBuffer)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, 0x%x)\n", "D3DDevice::CreateBuffer", pDesc, pInitialData);
  return gcpRendD3D->m_pActuald3dDevice->CreateBuffer(pDesc, pInitialData, ppBuffer);
}

HRESULT CMyDirect3DDevice10::CreateTexture1D( 
  /* [in] */ const D3D10_TEXTURE1D_DESC *pDesc,
  /* [in] */ const D3D10_SUBRESOURCE_DATA *pInitialData,
  /* [out] */ ID3D10Texture1D **ppTexture1D)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, 0x%x)\n", "D3DDevice::CreateTexture1D", pDesc, pInitialData);
  return gcpRendD3D->m_pActuald3dDevice->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
}

HRESULT CMyDirect3DDevice10::CreateTexture2D( 
  /* [in] */ const D3D10_TEXTURE2D_DESC *pDesc,
  /* [in] */ const D3D10_SUBRESOURCE_DATA *pInitialData,
  /* [out] */ ID3D10Texture2D **ppTexture2D)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, 0x%x)\n", "D3DDevice::CreateTexture2D", pDesc, pInitialData);
  return gcpRendD3D->m_pActuald3dDevice->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
}

HRESULT CMyDirect3DDevice10::CreateTexture3D( 
  /* [in] */ const D3D10_TEXTURE3D_DESC *pDesc,
  /* [in] */ const D3D10_SUBRESOURCE_DATA *pInitialData,
  /* [out] */ ID3D10Texture3D **ppTexture3D)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, 0x%x)\n", "D3DDevice::CreateTexture3D", pDesc, pInitialData);
  return gcpRendD3D->m_pActuald3dDevice->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
}

HRESULT CMyDirect3DDevice10::CreateShaderResourceView( 
  /* [in] */ ID3D10Resource *pResource,
  /* [in] */ const D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc,
  /* [out] */ ID3D10ShaderResourceView **ppSRView)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, 0x%x)\n", "D3DDevice::CreateShaderResourceView", pResource, pDesc);
  return gcpRendD3D->m_pActuald3dDevice->CreateShaderResourceView(pResource, pDesc, ppSRView);
}

HRESULT CMyDirect3DDevice10::CreateRenderTargetView( 
  /* [in] */ ID3D10Resource *pResource,
  /* [in] */ const D3D10_RENDER_TARGET_VIEW_DESC *pDesc,
  /* [out] */ ID3D10RenderTargetView **ppRTView)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, 0x%x)\n", "D3DDevice::CreateRenderTargetView", pResource, pDesc);
  return gcpRendD3D->m_pActuald3dDevice->CreateRenderTargetView(pResource, pDesc, ppRTView);
}

HRESULT CMyDirect3DDevice10::CreateDepthStencilView( 
  /* [in] */ ID3D10Resource *pResource,
  /* [in] */ const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc,
  /* [out] */ ID3D10DepthStencilView **ppDepthStencilView)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, 0x%x)\n", "D3DDevice::CreateDepthStencilView", pResource, pDesc);
  return gcpRendD3D->m_pActuald3dDevice->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
}

HRESULT CMyDirect3DDevice10::CreateInputLayout( 
  /* [size_is][in] */ const D3D10_INPUT_ELEMENT_DESC *pInputElementDescs,
  /* [in] */ UINT NumElements,
  /* [in] */ const void *pShaderBytecodeWithInputSignature,
	/* [in] */ SIZE_T BytecodeLength,
  /* [out] */ ID3D10InputLayout **ppInputLayout)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, %d, 0x%x, %d)\n", "D3DDevice::CreateInputLayout", pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength);
  return gcpRendD3D->m_pActuald3dDevice->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
}

HRESULT CMyDirect3DDevice10::CreateVertexShader( 
  /* [in] */ const void *pShaderBytecode,
	/* [in] */ SIZE_T BytecodeLength,
  /* [out] */ ID3D10VertexShader **ppVertexShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, %d)\n", "D3DDevice::CreateVertexShader", pShaderBytecode, BytecodeLength);
  return gcpRendD3D->m_pActuald3dDevice->CreateVertexShader(pShaderBytecode, BytecodeLength, ppVertexShader);
}

HRESULT CMyDirect3DDevice10::CreateGeometryShader( 
  /* [in] */ const void *pShaderBytecode,
	/* [in] */ SIZE_T BytecodeLength,
  /* [out] */ ID3D10GeometryShader **ppGeometryShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, %d)\n", "D3DDevice::CreateGeometryShader", pShaderBytecode, BytecodeLength);
  return gcpRendD3D->m_pActuald3dDevice->CreateGeometryShader(pShaderBytecode, BytecodeLength, ppGeometryShader);
}

HRESULT CMyDirect3DDevice10::CreateGeometryShaderWithStreamOutput( 
  /* [in] */ const void *pShaderBytecode,
	/* [in] */ SIZE_T BytecodeLength,
  /* [size_is][in] */ const D3D10_SO_DECLARATION_ENTRY *pSODeclaration,
  /* [in] */ UINT NumEntries,
  /* [in] */ UINT OutputStreamStride,
  /* [out] */ ID3D10GeometryShader **ppGeometryShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, %d, 0x%x, %d, %d)\n", "D3DDevice::CreateGeometryShader", pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, OutputStreamStride);
  return gcpRendD3D->m_pActuald3dDevice->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, OutputStreamStride, ppGeometryShader);
}

HRESULT CMyDirect3DDevice10::CreatePixelShader( 
  /* [in] */ const void *pShaderBytecode,
	/* [in] */ SIZE_T BytecodeLength,
  /* [out] */ ID3D10PixelShader **ppPixelShader)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x, %d)\n", "D3DDevice::CreatePixelShader", pShaderBytecode, BytecodeLength);
  return gcpRendD3D->m_pActuald3dDevice->CreatePixelShader(pShaderBytecode, BytecodeLength, ppPixelShader);
}

HRESULT CMyDirect3DDevice10::CreateBlendState( 
  /* [in] */ const D3D10_BLEND_DESC *pBlendStateDesc,
  /* [out] */ ID3D10BlendState **ppBlendState)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::CreateBlendState", pBlendStateDesc);
  return gcpRendD3D->m_pActuald3dDevice->CreateBlendState(pBlendStateDesc, ppBlendState);
}

HRESULT CMyDirect3DDevice10::CreateDepthStencilState( 
  /* [in] */ const D3D10_DEPTH_STENCIL_DESC *pDepthStencilDesc,
  /* [out] */ ID3D10DepthStencilState **ppDepthStencilState)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::CreateDepthStencilState", pDepthStencilDesc);
  return gcpRendD3D->m_pActuald3dDevice->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
}

HRESULT CMyDirect3DDevice10::CreateRasterizerState( 
  /* [in] */ const D3D10_RASTERIZER_DESC *pRasterizerDesc,
  /* [out] */ ID3D10RasterizerState **ppRasterizerState)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::CreateRasterizerState", pRasterizerDesc);
  return gcpRendD3D->m_pActuald3dDevice->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
}

HRESULT CMyDirect3DDevice10::CreateSamplerState( 
  /* [in] */ const D3D10_SAMPLER_DESC *pSamplerDesc,
  /* [out] */ ID3D10SamplerState **ppSamplerState)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::CreateSamplerState", pSamplerDesc);
  return gcpRendD3D->m_pActuald3dDevice->CreateSamplerState(pSamplerDesc, ppSamplerState);
}

HRESULT CMyDirect3DDevice10::CreateQuery( 
  /* [in] */ const D3D10_QUERY_DESC *pQueryDesc,
  /* [out] */ ID3D10Query **ppQuery)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::CreateQuery", pQueryDesc);
  return gcpRendD3D->m_pActuald3dDevice->CreateQuery(pQueryDesc, ppQuery);
}

HRESULT CMyDirect3DDevice10::CreatePredicate( 
  /* [in] */ const D3D10_QUERY_DESC *pPredicateDesc,
  /* [out] */ ID3D10Predicate **ppPredicate)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::CreatePredicate", pPredicateDesc);
  return gcpRendD3D->m_pActuald3dDevice->CreatePredicate(pPredicateDesc, ppPredicate);
}

HRESULT CMyDirect3DDevice10::CreateCounter( 
  /* [in] */ const D3D10_COUNTER_DESC *pCounterDesc,
  /* [out] */ ID3D10Counter **ppCounter)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::CreateCounter", pCounterDesc);
  return gcpRendD3D->m_pActuald3dDevice->CreateCounter(pCounterDesc, ppCounter);
}

HRESULT CMyDirect3DDevice10::CheckFormatSupport( 
  /* [in] */ DXGI_FORMAT Format,
  /* [retval][out] */ UINT *pFormatSupport)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%s)\n", "D3DDevice::CheckFormatSupport", sD3DFMT(Format));
  return gcpRendD3D->m_pActuald3dDevice->CheckFormatSupport(Format, pFormatSupport);
}

HRESULT CMyDirect3DDevice10::CheckMultisampleQualityLevels( 
  DXGI_FORMAT Format,
  /* [in] */ UINT SampleCount,
  /* [retval][out] */ UINT *pNumQualityLevels)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%s, %d)\n", "D3DDevice::CheckMultisampleQualityLevels", sD3DFMT(Format), SampleCount);
  return gcpRendD3D->m_pActuald3dDevice->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
}

void CMyDirect3DDevice10::CheckCounterInfo( 
  /* [retval][out] */ D3D10_COUNTER_INFO *pCounterInfo)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::CheckCounterInfo", pCounterInfo);
  gcpRendD3D->m_pActuald3dDevice->CheckCounterInfo(pCounterInfo);
}

HRESULT CMyDirect3DDevice10::CheckCounter( 
  /*  */ 
  __in  const D3D10_COUNTER_DESC *pDesc,
  /*  */ 
  __out  D3D10_COUNTER_TYPE *pType,
  /*  */ 
  __out  UINT *pActiveCounters,
  /*  */ 
  __out_ecount_opt(*pNameLength)  LPSTR wszName,
  /*  */ 
  __inout_opt  UINT *pNameLength,
  /*  */ 
  __out_ecount_opt(*pUnitsLength)  LPSTR wszUnits,
  /*  */ 
  __inout_opt  UINT *pUnitsLength,
  /*  */ 
  __out_ecount_opt(*pDescriptionLength)  LPSTR wszDescription,
  /*  */ 
  __inout_opt  UINT *pDescriptionLength)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(0x%x)\n", "D3DDevice::CheckCounter", pDesc);
  return gcpRendD3D->m_pActuald3dDevice->CheckCounter(pDesc, pType, pActiveCounters, wszName, pNameLength, wszUnits, pUnitsLength, wszDescription, pDescriptionLength);
}

void CMyDirect3DDevice10::CopySubresourceRegion(
                                   ID3D10Resource * pDstResource,
                                   UINT DstSubresource,
                                   UINT DstX,
                                   UINT DstY,
                                   UINT DstZ,
                                   ID3D10Resource * pSrcResource,
                                   UINT SrcSubresource,
                                   const D3D10_BOX * pSrcBox
                                   )
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::CopySubresourceRegion");
  gcpRendD3D->m_pActuald3dDevice->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
}

void CMyDirect3DDevice10::UpdateSubresource(
  ID3D10Resource * pDstResource,
  UINT DstSubresource,
  const D3D10_BOX * pDstBox,
  const void *pSrcData,
  UINT SrcRowPitch,
  UINT SrcDepthPitch
  )
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::UpdateSubresource");
  gcpRendD3D->m_pActuald3dDevice->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}

void CMyDirect3DDevice10::ResolveSubresource(
  ID3D10Resource * pDstResource,
  UINT DstSubresource,
  ID3D10Resource * pSrcResource,
  UINT SrcSubresource,
  DXGI_FORMAT Format
  )
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::ResolveSubresource");
  gcpRendD3D->m_pActuald3dDevice->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}

void CMyDirect3DDevice10::CopyResource(ID3D10Resource * pDstResource, ID3D10Resource * pSrcResource)
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::CopyResource");
  gcpRendD3D->m_pActuald3dDevice->CopyResource(pDstResource, pSrcResource);
}

void CMyDirect3DDevice10::ClearState()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::ClearState");
  gcpRendD3D->m_pActuald3dDevice->ClearState();
}

HRESULT CMyDirect3DDevice10::OpenSharedResource(
  HANDLE hResource,
  REFIID ReturnedInterface,
  void ** ppResource
  )
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::OpenSharedResource");
  return gcpRendD3D->m_pActuald3dDevice->OpenSharedResource(hResource, ReturnedInterface, ppResource);
}

UINT CMyDirect3DDevice10::GetCreationFlags()
{
  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s()\n", "D3DDevice::GetCreationFlags");
  return gcpRendD3D->m_pActuald3dDevice->GetCreationFlags();
}

void CMyDirect3DDevice10::SetTextFilterSize(UINT Width, UINT Height)
{
	gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%d, %d)\n", "D3DDevice::SetTextFilterSize", Width, Height);
	gcpRendD3D->m_pActuald3dDevice->SetTextFilterSize(Width, Height);
}

void CMyDirect3DDevice10::GetTextFilterSize(UINT *pWidth, UINT *pHeight)
{
	gcpRendD3D->m_pActuald3dDevice->GetTextFilterSize(pWidth, pHeight);
	gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "%s(%p(=%d), %p(=%d)\n", "D3DDevice::GetTextFilterSize", pWidth, pHeight, pWidth ? *pWidth : -1, pHeight ? *pHeight : 0);
}

#endif  // (DIRECT3D9) || defined (OPENGL)

void CD3D9Renderer::SetLogFuncs(bool bSet)
{
  static bool sSet = 0;

#ifdef OPENGL
//  glSetLogFuncs(bSet);
  return;
#else
  if (bSet == sSet)
    return;

  sSet = bSet;

#if defined (DIRECT3D9) || defined (OPENGL)
  if (bSet)
  {
    m_pActuald3dDevice = m_pd3dDevice;
    if (!m_pMyd3dDevice)
      m_pMyd3dDevice = new CMyDirect3DDevice9;
    m_pd3dDevice = m_pMyd3dDevice;
  }
  else
  {
    m_pd3dDevice = m_pActuald3dDevice;
  }
#elif defined (DIRECT3D10)
  if (bSet)
  {
    m_pActuald3dDevice = m_pd3dDevice;
    if (!m_pMyd3dDevice)
      m_pMyd3dDevice = new CMyDirect3DDevice10;
    m_pd3dDevice = m_pMyd3dDevice;
  }
  else
  {
    m_pd3dDevice = m_pActuald3dDevice;
  }
#endif

#endif
}

#endif //DO_RENDERLOG
