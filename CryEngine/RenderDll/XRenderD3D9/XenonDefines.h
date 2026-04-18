////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   XenonDefines.h
//  Version:     v1.00
//  Created:     25/9/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __XenonDefines_h__
#define __XenonDefines_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
// Defines and Enums from DirectX9 Not Supporrted by Xenon.
//////////////////////////////////////////////////////////////////////////

typedef void* RGNDATA;

//////////////////////////////////////////////////////////////////////////
// Formats not supported for Xenon.
#define D3DUSAGE_AUTOGENMIPMAP      (0x00000400L)
#define D3DFVF_XYZRHW           0x004

#define D3DDEVTYPE_REF         2
#define D3DCREATE_SOFTWARE_VERTEXPROCESSING     0x00000020L
#define D3DCREATE_MIXED_VERTEXPROCESSING        0x00000080L

#define D3DDEVTYPE_SW          3

#define D3DPRESENTFLAG_LOCKABLE_BACKBUFFER  0x00000001

#define D3DDECLUSAGE_POSITIONT ((D3DDECLUSAGE) 9)

// No W buffering
#define D3DZB_USEW             2

#define D3DBLEND_BOTHSRCALPHA  12
#define D3DBLEND_BOTHINVSRCALPHA  13
#define D3DTEXF_PYRAMIDALQUAD   6
#define D3DTEXF_GAUSSIANQUAD    7

#define D3DSTREAMSOURCE_INDEXEDDATA  (1<<30)
#define D3DSTREAMSOURCE_INSTANCEDATA (2<<30)

enum XENONE_D3DDEBUGMONITORTOKENS {
	D3DDMT_ENABLE            = 0,    // enable debug monitor
	D3DDMT_DISABLE           = 1,    // disable debug monitor
	D3DDMT_FORCE_DWORD     = 0x7fffffff,
};

#define D3DLOCK_DISCARD 0

#define D3DUSAGE_DYNAMIC 0

//////////////////////////////////////////////////////////////////////////
// Old DirectX defines.
//////////////////////////////////////////////////////////////////////////
static const D3DRENDERSTATETYPE 
	D3DRS_TEXTUREFACTOR             = (D3DRENDERSTATETYPE)60,   // D3DCOLOR used for multi-texture blend
	D3DRS_SHADEMODE                 = (D3DRENDERSTATETYPE)9,    // D3DSHADEMODE
	D3DRS_FOGENABLE                 = (D3DRENDERSTATETYPE)28,   // TRUE to enable fog blending
	D3DRS_SPECULARENABLE            = (D3DRENDERSTATETYPE)29,   // TRUE to enable specular
	D3DRS_FOGCOLOR                  = (D3DRENDERSTATETYPE)34,   // D3DCOLOR
	D3DRS_FOGTABLEMODE              = (D3DRENDERSTATETYPE)35,   // D3DFOGMODE
	D3DRS_CLIPPING                  = (D3DRENDERSTATETYPE)136,
	D3DRS_LIGHTING                  = (D3DRENDERSTATETYPE)137,
	D3DRS_AMBIENT                   = (D3DRENDERSTATETYPE)139,
	D3DRS_FOGVERTEXMODE             = (D3DRENDERSTATETYPE)140,
	D3DRS_COLORVERTEX               = (D3DRENDERSTATETYPE)141,
	D3DRS_NORMALIZENORMALS          = (D3DRENDERSTATETYPE)143,
	D3DRS_VERTEXBLEND               = (D3DRENDERSTATETYPE)151,
	D3DRS_POINTSCALEENABLE          = (D3DRENDERSTATETYPE)157,   // BOOL point size scale enable
	D3DRS_INDEXEDVERTEXBLENDENABLE  = (D3DRENDERSTATETYPE)167
	;

enum D3DSHADEMODE
{
	D3DSHADE_FLAT,
	D3DSHADE_GOURAUD,
	D3DSHADE_PHONG,
};

static const D3DMULTISAMPLE_TYPE 
	//D3DMULTISAMPLE_NONE,
	D3DMULTISAMPLE_NONMASKABLE = (D3DMULTISAMPLE_TYPE)8,
	//D3DMULTISAMPLE_2_SAMPLES,
	D3DMULTISAMPLE_3_SAMPLES,
	//D3DMULTISAMPLE_4_SAMPLES,
	D3DMULTISAMPLE_5_SAMPLES,
	D3DMULTISAMPLE_6_SAMPLES,
	D3DMULTISAMPLE_7_SAMPLES,
	D3DMULTISAMPLE_8_SAMPLES,
	D3DMULTISAMPLE_9_SAMPLES,
	D3DMULTISAMPLE_10_SAMPLES,
	D3DMULTISAMPLE_11_SAMPLES,
	D3DMULTISAMPLE_12_SAMPLES,
	D3DMULTISAMPLE_13_SAMPLES,
	D3DMULTISAMPLE_14_SAMPLES,
	D3DMULTISAMPLE_15_SAMPLES,
	D3DMULTISAMPLE_16_SAMPLES
	;

//////////////////////////////////////////////////////////////////////////

#define D3DDepthSurface                  IDirect3DSurface9

extern D3DDevice* g_pd3dDevice;    // Our rendering device

#define DEF_IMPL { return S_OK; };

struct IXenonDirect3DDevice9 : public D3DDevice
{
	HRESULT (SetTransform)( int State,CONST D3DMATRIX* pMatrix) DEF_IMPL
	//HRESULT (SetRenderState)( int State,DWORD Value) DEF_IMPL
	HRESULT (SetTextureStageState)( DWORD Stage,int Type,DWORD Value) DEF_IMPL
	HRESULT (CreateOffscreenPlainSurface)( UINT Width,UINT Height,D3DFORMAT Format,D3DPOOL Pool,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle) DEF_IMPL
	HRESULT (GetFrontBufferData)( UINT iSwapChain,IDirect3DSurface9* pDestSurface) DEF_IMPL
	//HRESULT (CreateRenderTarget)( UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle) DEF_IMPL
	HRESULT (GetRenderTargetData)( IDirect3DSurface9* pRenderTarget,IDirect3DSurface9* pDestSurface) DEF_IMPL
	HRESULT (StretchRect)( IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestSurface,CONST RECT* pDestRect,D3DTEXTUREFILTERTYPE Filter) DEF_IMPL
	HRESULT (TestCooperativeLevel)() DEF_IMPL
	UINT    (GetAvailableTextureMem)() { return 256; }
	HRESULT (ColorFill)( IDirect3DSurface9* pSurface,CONST RECT* pRect,D3DCOLOR color) DEF_IMPL
	HRESULT (EvictManagedResources)() DEF_IMPL
	HRESULT (SetCursorProperties)( UINT XHotSpot,UINT YHotSpot,IDirect3DSurface9* pCursorBitmap) DEF_IMPL
	BOOL    (ShowCursor)(BOOL bShow) { return bShow; };

//===================================================================================

#ifdef DO_RENDERLOG

  virtual D3DVOID WINAPI GetDirect3D(Direct3D **ppD3D9)
  {
    return g_pd3dDevice->GetDirect3D(ppD3D9);
  }
  virtual D3DVOID WINAPI GetDeviceCaps(D3DCAPS9 *pCaps)
  {
    return g_pd3dDevice->GetDeviceCaps(pCaps);
  }
  virtual D3DVOID WINAPI GetDisplayMode(UINT UnusedSwapChain, D3DDISPLAYMODE *pMode)
  {
    return g_pd3dDevice->GetDisplayMode(UnusedSwapChain, pMode);
  }
  virtual D3DVOID WINAPI GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
  {
    return g_pd3dDevice->GetCreationParameters(pParameters);
  }
  virtual HRESULT WINAPI Reset(D3DPRESENT_PARAMETERS *pPresentationParameters)
  {
    return g_pd3dDevice->Reset(pPresentationParameters);
  }
  virtual D3DVOID WINAPI Present(CONST RECT *pSourceRect, CONST RECT *pDestRect, void *hUnusedDestWindowOverride, void *pUnusedDirtyRegion)
  {
    return g_pd3dDevice->Present(pSourceRect, pDestRect, hUnusedDestWindowOverride, pUnusedDirtyRegion);
  }
  virtual D3DVOID WINAPI GetRasterStatus(UINT iUnusedSwapChain, D3DRASTER_STATUS *pRasterStatus)
  {
    return g_pd3dDevice->GetRasterStatus(iUnusedSwapChain, pRasterStatus);
  }
  virtual void    WINAPI SetGammaRamp(UINT iUnusedSwapChain, DWORD Flags, CONST D3DGAMMARAMP *pRamp)
  {
    g_pd3dDevice->SetGammaRamp(iUnusedSwapChain, Flags, pRamp);
  }
  virtual void    WINAPI GetGammaRamp(UINT iUnusedSwapChain, D3DGAMMARAMP *pRamp)
  {
    g_pd3dDevice->GetGammaRamp(iUnusedSwapChain, pRamp);
  }
  virtual HRESULT WINAPI CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL UnusedPool, D3DTexture **ppTexture, HANDLE* pUnusedSharedHandle)
  {
    return g_pd3dDevice->CreateTexture(Width, Height, Levels, Usage, Format, UnusedPool, ppTexture, pUnusedSharedHandle);
  }
  virtual HRESULT WINAPI CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL UnusedPool, D3DVolumeTexture **ppVolumeTexture, HANDLE* pUnusedSharedHandle)
  {
    return g_pd3dDevice->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, UnusedPool, ppVolumeTexture, pUnusedSharedHandle);
  }
  virtual HRESULT WINAPI CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL UnusedPool, D3DCubeTexture **ppCubeTexture, HANDLE* pUnusedSharedHandle)
  {
    return g_pd3dDevice->CreateCubeTexture(EdgeLength, Levels, Usage, Format, UnusedPool, ppCubeTexture, pUnusedSharedHandle);
  }
  virtual HRESULT WINAPI CreateArrayTexture(UINT Width, UINT Height, UINT ArraySize, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL UnusedPool, D3DArrayTexture **ppArrayTexture, HANDLE* pUnusedSharedHandle)
  {
    return g_pd3dDevice->CreateArrayTexture(Width, Height, ArraySize, Levels, Usage, Format, UnusedPool, ppArrayTexture, pUnusedSharedHandle);
  }
  virtual HRESULT WINAPI CreateVertexBuffer(UINT Length, DWORD Usage, DWORD UnusedFVF, D3DPOOL UnusedPool, D3DVertexBuffer **ppVertexBuffer, HANDLE* pUnusedSharedHandle)
  {
    return g_pd3dDevice->CreateVertexBuffer(Length, Usage, UnusedFVF, UnusedPool, ppVertexBuffer, pUnusedSharedHandle);
  }
  virtual HRESULT WINAPI CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL UnusedPool, D3DIndexBuffer **ppIndexBuffer, HANDLE* pUnusedSharedHandle)
  {
    return g_pd3dDevice->CreateIndexBuffer(Length, Usage, Format, UnusedPool, ppIndexBuffer, pUnusedSharedHandle);
  }
  virtual HRESULT WINAPI CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD UnusedMultisampleQuality, BOOL UnusedLockable, D3DSurface **ppSurface, CONST D3DSURFACE_PARAMETERS* pParameters)
  {
    return g_pd3dDevice->CreateRenderTarget(Width, Height, Format, MultiSample, UnusedMultisampleQuality, UnusedLockable, ppSurface, pParameters);
  }
  virtual HRESULT WINAPI CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD UnusedMultisampleQuality, BOOL UnusedDiscard, D3DSurface **ppSurface, CONST D3DSURFACE_PARAMETERS* pParameters)
  {
    return g_pd3dDevice->CreateDepthStencilSurface(Width, Height, Format, MultiSample, UnusedMultisampleQuality, UnusedDiscard, ppSurface, pParameters);
  }
  virtual HRESULT WINAPI SetScreenExtentQueryMode(D3DSCREENEXTENTQUERYMODE ScreenExtentQueryMode)
  {
    return g_pd3dDevice->SetScreenExtentQueryMode(ScreenExtentQueryMode);
  }
  virtual D3DVOID WINAPI SetRenderTarget(DWORD RenderTargetIndex, D3DSurface *pRenderTarget)
  {
    return g_pd3dDevice->SetRenderTarget(RenderTargetIndex, pRenderTarget);
  }
  virtual HRESULT WINAPI GetRenderTarget(DWORD RenderTargetIndex, D3DSurface **ppRenderTarget)
  {
    return g_pd3dDevice->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
  }
  virtual D3DVOID WINAPI SetDepthStencilSurface(D3DSurface *pZStencilSurface)
  {
    return g_pd3dDevice->SetDepthStencilSurface(pZStencilSurface);
  }
  virtual HRESULT WINAPI GetDepthStencilSurface(D3DSurface **ppZStencilSurface)
  {
    return g_pd3dDevice->GetDepthStencilSurface(ppZStencilSurface);
  }
  virtual HRESULT WINAPI GetBackBuffer(UINT UnusedSwapChain, UINT iUnusedBackBuffer, UINT UnusedType, D3DSurface **ppBackBuffer)
  {
    return g_pd3dDevice->GetBackBuffer(UnusedSwapChain, iUnusedBackBuffer, UnusedType, ppBackBuffer);
  }
  virtual HRESULT WINAPI GetFrontBuffer(D3DTexture **ppFrontBuffer)
  {
    return g_pd3dDevice->GetFrontBuffer(ppFrontBuffer);
  }
  virtual D3DVOID WINAPI BeginScene()
  {
    return g_pd3dDevice->BeginScene();
  }
  virtual D3DVOID WINAPI EndScene()
  {
    return g_pd3dDevice->EndScene();
  }
  virtual D3DVOID WINAPI Clear(DWORD Count, CONST D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
  {
    return g_pd3dDevice->Clear(Count, pRects, Flags, Color, Z, Stencil);
  }
  virtual D3DVOID WINAPI ClearF(DWORD Flags, CONST D3DRECT *pRect, CONST D3DVECTOR4* pColor, float Z, DWORD Stencil)
  {
    return g_pd3dDevice->ClearF(Flags, pRect, pColor, Z, Stencil);
  }
  virtual D3DVOID WINAPI SetViewport(CONST D3DVIEWPORT9 *pViewport)
  {
    return g_pd3dDevice->SetViewport(pViewport);
  }
  virtual D3DVOID WINAPI GetViewport(D3DVIEWPORT9 *pViewport)
  {
    return g_pd3dDevice->GetViewport(pViewport);
  }
  virtual D3DVOID WINAPI SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
  {
    return g_pd3dDevice->SetRenderState(State, Value);
  }
  virtual D3DVOID WINAPI GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue)
  {
    return g_pd3dDevice->GetRenderState(State, pValue);
  }
  virtual HRESULT WINAPI CreateStateBlock(D3DSTATEBLOCKTYPE Type, D3DStateBlock** ppSB)
  {
    return g_pd3dDevice->CreateStateBlock(Type, ppSB);
  }
  virtual D3DVOID WINAPI GetTexture(DWORD Sampler, D3DBaseTexture **ppTexture)
  {
    return g_pd3dDevice->GetTexture(Sampler, ppTexture);
  }
  virtual D3DVOID WINAPI SetTexture(DWORD Sampler, D3DBaseTexture *pTexture)
  {
    return g_pd3dDevice->SetTexture(Sampler, pTexture);
  }
  virtual D3DVOID WINAPI GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue)
  {
    return g_pd3dDevice->GetSamplerState(Sampler, Type, pValue);
  }
  virtual D3DVOID WINAPI SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
  {
    return g_pd3dDevice->SetSamplerState(Sampler, Type, Value);
  }
  virtual D3DVOID WINAPI DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
  {
    return g_pd3dDevice->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
  }
  virtual D3DVOID WINAPI DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT UnusedMinIndex, UINT UnusedNumIndices, UINT StartIndex, UINT PrimitiveCount)
  {
    return g_pd3dDevice->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, UnusedMinIndex, UnusedNumIndices, StartIndex, PrimitiveCount);
  }
  virtual D3DVOID WINAPI DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
  {
    return g_pd3dDevice->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
  }
  virtual D3DVOID WINAPI DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinIndex, UINT NumVertices, UINT PrimitiveCount, CONST void *pIndexData, D3DFORMAT IndexDataFormat, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
  {
    return g_pd3dDevice->DrawIndexedPrimitiveUP(PrimitiveType, MinIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
  }
  virtual D3DVOID WINAPI SetFVF(DWORD FVF)
  {
    return g_pd3dDevice->SetFVF(FVF);
  }
  virtual D3DVOID WINAPI GetFVF(DWORD* pFVF)
  {
    return g_pd3dDevice->GetFVF(pFVF);
  }
  virtual HRESULT WINAPI CreateVertexShader(CONST DWORD *pFunction, D3DVertexShader** ppShader)
  {
    return g_pd3dDevice->CreateVertexShader(pFunction, ppShader);
  }
  virtual D3DVOID WINAPI SetVertexShader(D3DVertexShader *pShader)
  {
    return g_pd3dDevice->SetVertexShader(pShader);
  }
  virtual D3DVOID WINAPI GetVertexShader(D3DVertexShader **ppShader)
  {
    return g_pd3dDevice->GetVertexShader(ppShader);
  }
  virtual D3DVOID WINAPI SetVertexShaderConstantB(UINT StartRegister, CONST BOOL *pConstantData, UINT BoolCount)
  {
    return g_pd3dDevice->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
  }
  virtual D3DVOID WINAPI SetVertexShaderConstantF(UINT StartRegister, CONST float *pConstantData, UINT Vector4fCount)
  {
    return g_pd3dDevice->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
  }
  virtual D3DVOID WINAPI SetVertexShaderConstantI(UINT StartRegister, CONST int *pConstantData, UINT Vector4iCount)
  {
    return g_pd3dDevice->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
  }
  virtual D3DVOID WINAPI GetVertexShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
  {
    return g_pd3dDevice->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
  }
  virtual D3DVOID WINAPI GetVertexShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount)
  {
    return g_pd3dDevice->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
  }
  virtual D3DVOID WINAPI GetVertexShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount)
  {
    return g_pd3dDevice->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
  }
  virtual D3DVOID WINAPI SetStreamSource(UINT StreamNumber, D3DVertexBuffer *pStreamData, UINT OffsetInBytes, UINT Stride)
  {
    return g_pd3dDevice->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
  }
  virtual D3DVOID WINAPI GetStreamSource(UINT StreamNumber, D3DVertexBuffer **ppStreamData, UINT *pOffsetInBytes, UINT *pStride)
  {
    return g_pd3dDevice->GetStreamSource(StreamNumber, ppStreamData, pOffsetInBytes, pStride);
  }
  virtual D3DVOID WINAPI SetIndices(D3DIndexBuffer *pIndexData)
  {
    return g_pd3dDevice->SetIndices(pIndexData);
  }
  virtual D3DVOID WINAPI GetIndices(D3DIndexBuffer **ppIndexData)
  {
    return g_pd3dDevice->GetIndices(ppIndexData);
  }
  virtual HRESULT WINAPI CreatePixelShader(CONST DWORD *pFunction, D3DPixelShader** ppShader)
  {
    return g_pd3dDevice->CreatePixelShader(pFunction, ppShader);
  }
  virtual D3DVOID WINAPI SetPixelShader(D3DPixelShader* pShader)
  {
    return g_pd3dDevice->SetPixelShader(pShader);
  }
  virtual D3DVOID WINAPI GetPixelShader(D3DPixelShader** ppShader)
  {
    return g_pd3dDevice->GetPixelShader(ppShader);
  }
  virtual D3DVOID WINAPI SetPixelShaderConstantB(UINT StartRegister, CONST BOOL *pConstantData, UINT BoolCount)
  {
    return g_pd3dDevice->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
  }
  virtual HRESULT SetPixelShaderConstantF(UINT StartRegister, CONST float *pConstantData, UINT Vector4fCount)
  {
    return g_pd3dDevice->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
  }
  virtual D3DVOID WINAPI SetPixelShaderConstantI(UINT StartRegister, CONST int *pConstantData, UINT Vector4iCount)
  {
    return g_pd3dDevice->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
  }
  virtual D3DVOID WINAPI GetPixelShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
  {
    return g_pd3dDevice->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
  }
  virtual D3DVOID WINAPI GetPixelShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount)
  {
    return g_pd3dDevice->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
  }
  virtual D3DVOID WINAPI GetPixelShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount)
  {
    return g_pd3dDevice->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
  }
  virtual HRESULT WINAPI CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements, D3DVertexDeclaration **ppVertexDeclaration)
  {
    return g_pd3dDevice->CreateVertexDeclaration(pVertexElements, ppVertexDeclaration);
  }
  virtual D3DVOID WINAPI SetVertexDeclaration(D3DVertexDeclaration *pDecl)
  {
    return g_pd3dDevice->SetVertexDeclaration(pDecl);
  }
  virtual D3DVOID WINAPI GetVertexDeclaration(D3DVertexDeclaration **ppDecl)
  {
    return g_pd3dDevice->GetVertexDeclaration(ppDecl);
  }
  virtual D3DVOID WINAPI SetScissorRect(CONST RECT* pRect)
  {
    return g_pd3dDevice->SetScissorRect(pRect);
  }
  virtual D3DVOID WINAPI GetScissorRect(RECT* pRect)
  {
    return g_pd3dDevice->GetScissorRect(pRect);
  }
  virtual D3DVOID WINAPI SetClipPlane(DWORD Index, CONST float* pPlane)
  {
    return g_pd3dDevice->SetClipPlane(Index, pPlane);
  }
  virtual D3DVOID WINAPI GetClipPlane(DWORD Index, float* pPlane)
  {
    return g_pd3dDevice->GetClipPlane(Index, pPlane);
  }
  virtual D3DVOID WINAPI SetStreamSourceFreq(UINT StreamNumber, UINT Divider)
  {
		// Not implemented in Xenon
    //return g_pd3dDevice->SetStreamSourceFreq(StreamNumber, Divider);
		return 0;
  }
  virtual D3DVOID WINAPI GetStreamSourceFreq(UINT StreamNumber, UINT* pDivider)
  {
		// Not implemented in Xenon
    //return g_pd3dDevice->GetStreamSourceFreq(StreamNumber, pDivider);
		return 0;
  }
  virtual HRESULT WINAPI CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery)
  {
    return g_pd3dDevice->CreateQuery(Type, ppQuery);
  }

  // The following APIs are all Xenon extensions:

  /*virtual D3DVOID WINAPI SetSamplerStateNotInline(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
  {
    return g_pd3dDevice->SetSamplerStateNotInline(Sampler, Type, Value);
  }
  virtual D3DVOID WINAPI GetSamplerStateNotInline(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue)
  {
    return g_pd3dDevice->GetSamplerStateNotInline(Sampler, Type, pValue);
  }
  virtual D3DVOID WINAPI SetRenderStateNotInline(D3DRENDERSTATETYPE State, DWORD Value)
  {
    return g_pd3dDevice->SetRenderStateNotInline(State, Value);
  }
  virtual D3DVOID WINAPI GetRenderStateNotInline(D3DRENDERSTATETYPE State, DWORD *pValue)
  {
    return g_pd3dDevice->GetRenderStateNotInline(State, pValue);
  }*/
  virtual D3DVOID WINAPI Resolve(DWORD Flags, CONST D3DRECT *pSourceRect, D3DBaseTexture *pDestTexture, CONST D3DPOINT *pDestPoint, UINT DestLevel, UINT DestSliceOrFace, CONST D3DVECTOR4* pClearColor, float ClearZ, DWORD ClearStencil, CONST D3DRESOLVE_PARAMETERS* pParameters)
  {
    return g_pd3dDevice->Resolve(Flags, pSourceRect, pDestTexture, pDestPoint, DestLevel, DestSliceOrFace, pClearColor, ClearZ, ClearStencil, pParameters);
  }
  virtual D3DVOID WINAPI AcquireThreadOwnership()
  {
    return g_pd3dDevice->AcquireThreadOwnership();
  }
  virtual D3DVOID WINAPI ReleaseThreadOwnership()
  {
    return g_pd3dDevice->ReleaseThreadOwnership();
  }
  virtual D3DVOID WINAPI SetThreadOwnership(DWORD ThreadID)
  {
    return g_pd3dDevice->SetThreadOwnership(ThreadID);
  }
  virtual BOOL    WINAPI IsBusy()
  {
    return g_pd3dDevice->IsBusy();
  }
  virtual D3DVOID WINAPI BlockUntilIdle()
  {
    return g_pd3dDevice->BlockUntilIdle();
  }
  virtual D3DVOID WINAPI InsertCallback(D3DCALLBACKTYPE Type, D3DCALLBACK pCallback, DWORD Context)
  {
    return g_pd3dDevice->InsertCallback(Type, pCallback, Context);
  }
  virtual D3DVOID WINAPI SetVerticalBlankCallback(D3DVBLANKCALLBACK pCallback)
  {
    return g_pd3dDevice->SetVerticalBlankCallback(pCallback);
  }
  /*virtual D3DVOID WINAPI SetAlphaKitMode(D3DALPHAKITMODE Mode)
  {
    return g_pd3dDevice->SetAlphaKitMode(Mode);
  }
  virtual D3DVOID WINAPI QueryAlphaKitMode(D3DALPHAKITMODE *pMode)
  {
    return g_pd3dDevice->QueryAlphaKitMode(pMode);
  }*/
  virtual D3DVOID WINAPI SynchronizeToPresentationInterval()
  {
    return g_pd3dDevice->SynchronizeToPresentationInterval();
  }
  virtual D3DVOID WINAPI Swap(D3DBaseTexture* pFrontBuffer, const D3DVIDEO_SCALER_PARAMETERS* pReserved)
  {
    return g_pd3dDevice->Swap(pFrontBuffer, pReserved);
  }
  virtual D3DVOID WINAPI RenderSystemUI()
  {
    return g_pd3dDevice->RenderSystemUI();
  }
  virtual D3DVOID WINAPI QueryBufferSpace(DWORD* pUsed, DWORD* pRemaining)
  {
    return g_pd3dDevice->QueryBufferSpace(pUsed, pRemaining);
  }
  virtual D3DVOID WINAPI SetPredication(DWORD PredicationMask)
  {
    return g_pd3dDevice->SetPredication(PredicationMask);
  }
  virtual D3DVOID WINAPI SetPatchablePredication(DWORD PredicationMask, DWORD Identifier)
  {
    return g_pd3dDevice->SetPatchablePredication(PredicationMask, Identifier);
  }
  virtual D3DVOID WINAPI BeginTiling(DWORD Flags, DWORD Count, CONST D3DRECT* pTileRects, CONST D3DVECTOR4* pClearColor, float ClearZ, DWORD ClearStencil)
  {
    return g_pd3dDevice->BeginTiling(Flags, Count, pTileRects, pClearColor, ClearZ, ClearStencil);
  }
  virtual HRESULT WINAPI EndTiling(DWORD ResolveFlags, CONST D3DRECT* pResolveRects, D3DBaseTexture* pDestTexture, CONST D3DVECTOR4* pClearColor, float ClearZ, DWORD ClearStencil, CONST D3DRESOLVE_PARAMETERS* pParameters)
  {
    return g_pd3dDevice->EndTiling(ResolveFlags, pResolveRects, pDestTexture, pClearColor, ClearZ, ClearStencil, pParameters);
  }
  virtual D3DVOID WINAPI BeginZPass(DWORD Flags)
  {
    return g_pd3dDevice->BeginZPass(Flags);
  }
  virtual HRESULT WINAPI EndZPass()
  {
    return g_pd3dDevice->EndZPass();
  }
  virtual HRESULT WINAPI InvokeRenderPass()
  {
    return g_pd3dDevice->InvokeRenderPass();
  }
  virtual D3DVOID WINAPI DrawTessellatedPrimitive(D3DTESSPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
  {
    return g_pd3dDevice->DrawTessellatedPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
  }
  virtual D3DVOID WINAPI DrawIndexedTessellatedPrimitive(D3DTESSPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT StartIndex, UINT PrimitiveCount)
  {
    return g_pd3dDevice->DrawIndexedTessellatedPrimitive(PrimitiveType, BaseVertexIndex, StartIndex, PrimitiveCount);
  }
  virtual HRESULT WINAPI SetRingBufferParameters(CONST D3DRING_BUFFER_PARAMETERS *pParameters)
  {
    return g_pd3dDevice->SetRingBufferParameters(pParameters);
  }
  virtual D3DVOID WINAPI XpsBegin(DWORD Flags)
  {
    return g_pd3dDevice->XpsBegin(Flags);
  }
  virtual HRESULT WINAPI XpsEnd()
  {
    return g_pd3dDevice->XpsEnd();
  }
  virtual D3DVOID WINAPI XpsSetCallback(D3DXpsCallback pCallback, void* pContext, DWORD Flags)
  {
    return g_pd3dDevice->XpsSetCallback(pCallback, pContext, Flags);
  }
  virtual D3DVOID WINAPI XpsSubmit(DWORD InstanceCount, CONST void* pData, DWORD Size)
  {
    return g_pd3dDevice->XpsSubmit(InstanceCount, pData, Size);
  }
  virtual HRESULT WINAPI BeginVertices(D3DPRIMITIVETYPE PrimitiveType, UINT VertexCount, UINT VertexStreamZeroStride, void**ppVertexData)
  {
    return g_pd3dDevice->BeginVertices(PrimitiveType, VertexCount, VertexStreamZeroStride, ppVertexData);
  }
  virtual D3DVOID WINAPI EndVertices()
  {
    return g_pd3dDevice->EndVertices();
  }
  virtual HRESULT WINAPI BeginIndexedVertices(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT NumVertices, UINT IndexCount, D3DFORMAT IndexDataFormat, UINT VertexStreamZeroStride, void** ppIndexData, void** ppVertexData)
  {
    return g_pd3dDevice->BeginIndexedVertices(PrimitiveType, BaseVertexIndex, NumVertices, IndexCount, IndexDataFormat, VertexStreamZeroStride, ppIndexData, ppVertexData);
  }
  virtual D3DVOID WINAPI EndIndexedVertices()
  {
    return g_pd3dDevice->EndIndexedVertices();
  }
  virtual DWORD   WINAPI InsertFence()
  {
    return g_pd3dDevice->InsertFence();
  }
  virtual D3DVOID WINAPI BlockOnFence(DWORD Fence)
  {
    return g_pd3dDevice->BlockOnFence(Fence);
  }
  virtual BOOL    WINAPI IsFencePending(DWORD Fence)
  {
    return g_pd3dDevice->IsFencePending(Fence);
  }
  virtual D3DVOID WINAPI SetBlendState(DWORD RenderTargetIndex, D3DBLENDSTATE BlendState)
  {
    return g_pd3dDevice->SetBlendState(RenderTargetIndex, BlendState);
  }
  virtual D3DVOID WINAPI GetBlendState(DWORD RenderTargetIndex, D3DBLENDSTATE* pBlendState)
  {
    return g_pd3dDevice->GetBlendState(RenderTargetIndex, pBlendState);
  }
  virtual D3DVOID WINAPI SetVertexFetchConstant(UINT VertexFetchRegister, D3DVertexBuffer* pVertexBuffer, UINT Offset)
  {
    return g_pd3dDevice->SetVertexFetchConstant(VertexFetchRegister, pVertexBuffer, Offset);
  }
  virtual D3DVOID WINAPI SetTextureFetchConstant(UINT TextureFetchRegister, D3DBaseTexture* pTexture)
  {
    return g_pd3dDevice->SetTextureFetchConstant(TextureFetchRegister, pTexture);
  }
  virtual float   WINAPI GetCounter(D3DCOUNTER CounterID)
  {
    return g_pd3dDevice->GetCounter(CounterID);
  }
  virtual D3DVOID WINAPI SetSafeLevel(DWORD Flags, DWORD Level)
  {
    return g_pd3dDevice->SetSafeLevel(Flags, Level);
  }
  virtual D3DVOID WINAPI GetSafeLevel(DWORD* pFlags, DWORD* pLevel)
  {
    return g_pd3dDevice->GetSafeLevel(pFlags, pLevel);
  }
#endif //DO_RENDERLOG
};


typedef IXenonDirect3DDevice9* LPDIRECT3DDEVICE9_XENON;
#define LPDIRECT3DDEVICE9 LPDIRECT3DDEVICE9_XENON


//////////////////////////////////////////////////////////////////////////
// Textures.
//////////////////////////////////////////////////////////////////////////
struct IXenonDirect3DTexture9 : public D3DTexture
{
	HRESULT (SetAutoGenFilterType)( int FilterType) DEF_IMPL
	void GenerateMipSubLevels() {};
};
typedef IXenonDirect3DTexture9* LPDIRECT3DTEXTURE9_XENON;
#define LPDIRECT3DTEXTURE9 LPDIRECT3DTEXTURE9_XENON

//////////////////////////////////////////////////////////////////////////
struct IXenonIDirect3DVolumeTexture9 : public D3DVolumeTexture
{
	HRESULT (SetAutoGenFilterType)( int FilterType) DEF_IMPL
};
typedef IXenonIDirect3DVolumeTexture9* LPDIRECT3DVOLUMETEXTURE9_XENON;
#define LPDIRECT3DVOLUMETEXTURE9 LPDIRECT3DVOLUMETEXTURE9_XENON

//////////////////////////////////////////////////////////////////////////
struct IXenonDirect3DCubeTexture9 : public D3DCubeTexture
{
	HRESULT (SetAutoGenFilterType)( int FilterType) DEF_IMPL
};
typedef IXenonDirect3DCubeTexture9* LPDIRECT3DCUBETEXTURE9_XENON;
#define LPDIRECT3DCUBETEXTURE9 LPDIRECT3DCUBETEXTURE9_XENON


//////////////////////////////////////////////////////////////////////////
// Different definition.
//////////////////////////////////////////////////////////////////////////
inline D3DXLoadSurfaceFromMemory(
													LPDIRECT3DSURFACE9        pDestSurface,
													CONST PALETTEENTRY*       pDestPalette,
													CONST RECT*               pDestRect,
													LPCVOID                   pSrcMemory,
													D3DFORMAT                 SrcFormat,
													UINT                      SrcPitch,
													CONST PALETTEENTRY*       pSrcPalette,
													CONST RECT*               pSrcRect,
													DWORD                     Filter,
													D3DCOLOR                  ColorKey)
{
	BOOL SrcPacked = FALSE;
	UINT SrcParentWidth = pSrcRect->right - pSrcRect->left;
	UINT SrcParentHeight = pSrcRect->bottom - pSrcRect->top;
	return ::D3DXLoadSurfaceFromMemory( pDestSurface,pDestPalette,pDestRect,pSrcMemory,SrcFormat,SrcPitch,
		pSrcPalette,pSrcRect,SrcPacked,SrcParentWidth,SrcParentHeight,Filter,ColorKey  );
}

//////////////////////////////////////////////////////////////////////////
inline D3DXCreateTexture(
									LPDIRECT3DDEVICE9         pDevice,
									UINT                      Width,
									UINT                      Height,
									UINT                      MipLevels,
									DWORD                     Usage,
									D3DFORMAT                 Format,
									D3DPOOL                   Pool,
									LPDIRECT3DTEXTURE9_XENON*       ppTexture)
{
	return ::D3DXCreateTexture( pDevice,Width,Height,MipLevels,Usage,Format,Pool,(D3DTexture**)ppTexture );
}

inline D3DXCreateCubeTexture(
											LPDIRECT3DDEVICE9         pDevice,
											UINT                      Size,
											UINT                      MipLevels,
											DWORD                     Usage,
											D3DFORMAT                 Format,
											D3DPOOL                   Pool,
											LPDIRECT3DCUBETEXTURE9_XENON*   ppCubeTexture)
{
	return ::D3DXCreateCubeTexture( pDevice,Size,MipLevels,Usage,Format,Pool,(D3DCubeTexture**)ppCubeTexture );
}

inline D3DXCreateVolumeTexture(
												LPDIRECT3DDEVICE9         pDevice,
												UINT                      Width,
												UINT                      Height,
												UINT                      Depth,
												UINT                      MipLevels,
												DWORD                     Usage,
												D3DFORMAT                 Format,
												D3DPOOL                   Pool,
												LPDIRECT3DVOLUMETEXTURE9_XENON* ppVolumeTexture)
{
	return ::D3DXCreateVolumeTexture( pDevice,Width,Height,Depth,MipLevels,Usage,Format,Pool,(D3DVolumeTexture**)ppVolumeTexture );
}

//////////////////////////////////////////////////////////////////////////
// Placeholder implementation for nvDXT library.
//////////////////////////////////////////////////////////////////////////
inline unsigned char * nvDXTdecompress(int & w, int & h, int & depth, int & total_width, int & rowBytes, int & src_format,
																int SpecifiedMipMaps )
{
	return (unsigned char *)&rowBytes;
}

typedef HRESULT (*MIPcallback)(void * data, int miplevel, DWORD size, int width, int height, void * user_data);
inline HRESULT nvDXTcompress(unsigned char * raw_data, // pointer to data (24 or 32 bit)
											unsigned long w, // width in texels
											unsigned long h, // height in texels
											DWORD byte_pitch,
											struct CompressionOptions * options,
											DWORD planes, // 3 or 4
											MIPcallback callback,  // callback for generated levels
											RECT * rect )
{
	return S_OK;
}


HRESULT nvDXTcompressBGRA(unsigned char * image_data, 
													unsigned long image_width,
													unsigned long image_height, 
													DWORD byte_pitch,
													CompressionOptions * options,
													DWORD planes,
													MIPcallback callback,
													RECT * rect)
{
	return S_OK;
}

#endif // __XenonDefines_h__

