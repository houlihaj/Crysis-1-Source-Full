/*=============================================================================
  DriverD3D9.h : Direct3D8 Render interface declarations.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#ifndef DRIVERD3D9_H
#define DRIVERD3D9_H

#if _MSC_VER > 1000
# pragma once 
#endif

/*
===========================================
The DXRenderer interface Class
===========================================
*/
#if defined (OPENGL)
#include "OpenGL/GLBase.h"
#elif defined (XENON)
#include "xgraphics.h"
#define XENON_EDRAM_SIZE GPU_EDRAM_SIZE // 10 Mb
#define XENON_RING_SIZE  512*1024       // 512Kb

//--------------------------------------------------------------------------------------
// Name: struct TILING_SCENARIO
// Desc: Describes a configuration of MSAA quality and tiling rectangles to be used for
//       Predicated Tiling.
//--------------------------------------------------------------------------------------
struct TILING_SCENARIO
{
  CONST WCHAR*            strScenarioName;
  DWORD                   dwScreenWidth;
  DWORD                   dwScreenHeight;
  DWORD                   dwTileCount;
  DWORD                   dwTilingFlags;
  D3DSCREENEXTENTQUERYMODE ScreenExtentQueryMode;
  D3DRECT                 TilingRects[15];
};

#endif

#if !defined(XENON)
#include "DXUT/DXUT.h"
#include "DXUT/DXUTenum.h"
#include "DXUT/DXUTmisc.h"
#endif

//=======================================================================

// DRIVERD3D.H
// CRender3D Direct3D rasterizer class.

#define VERSION_D3D 2.0

#define DECLARE_INITED(typ,var) typ var; memset(&var,0,sizeof(var)); var.dwSize=sizeof(var);
#define SAFETRY(cmd) {try{cmd;}catch(...){ShError("Exception in '%s'\n", #cmd);}}
#define DX_RELEASE(x) { if(x) { (x)->Release(); (x) = NULL; } }

//#define SWITCH_RENDERTARGET_FOR_ATAA	//if enabled, it switches the rendertarget for the general shaders, if off, it just alters the channel write mask

struct SPixFormat;
class CRender3DD3D;
struct SDeviceInfo;

struct SREPointSpriteCreateParams;
struct SPointSpriteVertex;

//=======================================================================

#include "D3DSettings.h"
#include "D3DTexture.h"
#include "D3DRenderAuxGeom.h"
#include "ShadowTextureGroupManager.h"		// CShadowTextureGroupManager

#if defined(DIRECT3D9) || defined(OPENGL)

class CMyDirect3DDevice9;

#if !defined(XENON) && !defined(PS3)
#define TEXPOOL D3DPOOL_MANAGED
#else
#define TEXPOOL D3DPOOL_DEFAULT
#endif

#if !defined(XENON)
class CMyDirect3DDevice9 : public IDirect3DDevice9
#else
class CMyDirect3DDevice9 : public IXenonDirect3DDevice9
#endif
{
  /*** IUnknown methods ***/
#ifndef OPENGL
  STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG,AddRef)(THIS);
  STDMETHOD_(ULONG,Release)(THIS);
#endif

  /*** IDirect3DDevice9 methods ***/
  STDMETHOD(TestCooperativeLevel)(THIS);
  STDMETHOD_(UINT, GetAvailableTextureMem)(THIS);
  STDMETHOD(EvictManagedResources)(THIS);
  STDMETHOD(GetDirect3D)(THIS_ IDirect3D9** ppD3D9);
  STDMETHOD(GetDeviceCaps)(THIS_ D3DCAPS9* pCaps);
  STDMETHOD(GetDisplayMode)(THIS_ UINT iSwapChain,D3DDISPLAYMODE* pMode);
  STDMETHOD(GetCreationParameters)(THIS_ D3DDEVICE_CREATION_PARAMETERS *pParameters);
  STDMETHOD(SetCursorProperties)(THIS_ UINT XHotSpot,UINT YHotSpot,IDirect3DSurface9* pCursorBitmap);
#if !defined(XENON) && !defined(PS3)
  STDMETHOD_(void, SetCursorPosition)(THIS_ int X,int Y,DWORD Flags);
#endif
  STDMETHOD_(BOOL, ShowCursor)(THIS_ BOOL bShow);
  
	// Different for XBox DirectX
#ifndef _XBOX
	STDMETHOD(CreateAdditionalSwapChain)(THIS_ D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain9** pSwapChain);
  STDMETHOD(GetSwapChain)(THIS_ UINT iSwapChain,IDirect3DSwapChain9** pSwapChain);
#endif

  STDMETHOD(Reset)(THIS_ D3DPRESENT_PARAMETERS* pPresentationParameters);
  STDMETHOD(GetRasterStatus)(THIS_ UINT iSwapChain,D3DRASTER_STATUS* pRasterStatus);
  STDMETHOD_(void, SetGammaRamp)(THIS_ UINT iSwapChain,DWORD Flags,CONST D3DGAMMARAMP* pRamp);
  STDMETHOD_(void, GetGammaRamp)(THIS_ UINT iSwapChain,D3DGAMMARAMP* pRamp);
  STDMETHOD(CreateTexture)(THIS_ UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9** ppTexture,HANDLE* pSharedHandle);
  STDMETHOD(CreateVolumeTexture)(THIS_ UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9** ppVolumeTexture,HANDLE* pSharedHandle);
  STDMETHOD(CreateCubeTexture)(THIS_ UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9** ppCubeTexture,HANDLE* pSharedHandle);
  STDMETHOD(CreateVertexBuffer)(THIS_ UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer9** ppVertexBuffer,HANDLE* pSharedHandle);
  STDMETHOD(CreateIndexBuffer)(THIS_ UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9** ppIndexBuffer,HANDLE* pSharedHandle);
  STDMETHOD(GetRenderTargetData)(THIS_ IDirect3DSurface9* pRenderTarget,IDirect3DSurface9* pDestSurface);
  STDMETHOD(GetFrontBufferData)(THIS_ UINT iSwapChain,IDirect3DSurface9* pDestSurface);
  STDMETHOD(StretchRect)(THIS_ IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestSurface,CONST RECT* pDestRect,D3DTEXTUREFILTERTYPE Filter);
  STDMETHOD(ColorFill)(THIS_ IDirect3DSurface9* pSurface,CONST RECT* pRect,D3DCOLOR color);
  STDMETHOD(CreateOffscreenPlainSurface)(THIS_ UINT Width,UINT Height,D3DFORMAT Format,D3DPOOL Pool,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle);
  STDMETHOD(SetRenderTarget)(THIS_ DWORD RenderTargetIndex,IDirect3DSurface9* pRenderTarget);
  STDMETHOD(GetRenderTarget)(THIS_ DWORD RenderTargetIndex,IDirect3DSurface9** ppRenderTarget);
  STDMETHOD(SetDepthStencilSurface)(THIS_ IDirect3DSurface9* pNewZStencil);
  STDMETHOD(GetDepthStencilSurface)(THIS_ IDirect3DSurface9** ppZStencilSurface);
  STDMETHOD(BeginScene)(THIS);
  STDMETHOD(EndScene)(THIS);
  STDMETHOD(Clear)(THIS_ DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil);
  STDMETHOD(SetTransform)(THIS_ D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix);
  STDMETHOD(SetViewport)(THIS_ CONST D3DVIEWPORT9* pViewport);
  STDMETHOD(GetViewport)(THIS_ D3DVIEWPORT9* pViewport);
  STDMETHOD(SetClipPlane)(THIS_ DWORD Index,CONST float* pPlane);
  STDMETHOD(GetClipPlane)(THIS_ DWORD Index,float* pPlane);
  STDMETHOD(SetRenderState)(THIS_ D3DRENDERSTATETYPE State,DWORD Value);
  STDMETHOD(GetRenderState)(THIS_ D3DRENDERSTATETYPE State,DWORD* pValue);
  STDMETHOD(CreateStateBlock)(THIS_ D3DSTATEBLOCKTYPE Type,IDirect3DStateBlock9** ppSB);
  STDMETHOD(GetTexture)(THIS_ DWORD Stage,IDirect3DBaseTexture9** ppTexture);
  STDMETHOD(SetTexture)(THIS_ DWORD Stage,IDirect3DBaseTexture9* pTexture);
  STDMETHOD(SetTextureStageState)(THIS_ DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value);
  STDMETHOD(GetSamplerState)(THIS_ DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD* pValue);
  STDMETHOD(SetSamplerState)(THIS_ DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD Value);
  STDMETHOD(SetScissorRect)(THIS_ CONST RECT* pRect);
  STDMETHOD(GetScissorRect)(THIS_ RECT* pRect);
  STDMETHOD(DrawPrimitive)(THIS_ D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount);
  STDMETHOD(DrawIndexedPrimitive)(THIS_ D3DPRIMITIVETYPE,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount);
  STDMETHOD(DrawPrimitiveUP)(THIS_ D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride);
  STDMETHOD(DrawIndexedPrimitiveUP)(THIS_ D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride);
  STDMETHOD(CreateVertexDeclaration)(THIS_ CONST D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl);
  STDMETHOD(SetVertexDeclaration)(THIS_ IDirect3DVertexDeclaration9* pDecl);
  STDMETHOD(GetVertexDeclaration)(THIS_ IDirect3DVertexDeclaration9** ppDecl);
  STDMETHOD(SetFVF)(THIS_ DWORD FVF);
  STDMETHOD(GetFVF)(THIS_ DWORD* pFVF);
  STDMETHOD(CreateVertexShader)(THIS_ CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader);
  STDMETHOD(SetVertexShader)(THIS_ IDirect3DVertexShader9* pShader);
  STDMETHOD(GetVertexShader)(THIS_ IDirect3DVertexShader9** ppShader);
  STDMETHOD(SetVertexShaderConstantF)(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount);
  STDMETHOD(GetVertexShaderConstantF)(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount);
  STDMETHOD(SetVertexShaderConstantI)(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount);
  STDMETHOD(GetVertexShaderConstantI)(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount);
  STDMETHOD(SetVertexShaderConstantB)(THIS_ UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount);
  STDMETHOD(GetVertexShaderConstantB)(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount);
  STDMETHOD(SetStreamSource)(THIS_ UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride);
  STDMETHOD(GetStreamSource)(THIS_ UINT StreamNumber,IDirect3DVertexBuffer9** ppStreamData,UINT* OffsetInBytes,UINT* pStride);
  STDMETHOD(SetStreamSourceFreq)(THIS_ UINT StreamNumber,UINT Divider);
  STDMETHOD(GetStreamSourceFreq)(THIS_ UINT StreamNumber,UINT* Divider);
  STDMETHOD(SetIndices)(THIS_ IDirect3DIndexBuffer9* pIndexData);
  STDMETHOD(GetIndices)(THIS_ IDirect3DIndexBuffer9** ppIndexData);
  STDMETHOD(CreatePixelShader)(THIS_ CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader);
  STDMETHOD(SetPixelShader)(THIS_ IDirect3DPixelShader9* pShader);
  STDMETHOD(GetPixelShader)(THIS_ IDirect3DPixelShader9** ppShader);
  STDMETHOD(SetPixelShaderConstantF)(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount);
  STDMETHOD(GetPixelShaderConstantF)(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount);
  STDMETHOD(SetPixelShaderConstantI)(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount);
  STDMETHOD(GetPixelShaderConstantI)(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount);
  STDMETHOD(SetPixelShaderConstantB)(THIS_ UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount);
  STDMETHOD(GetPixelShaderConstantB)(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount);
  STDMETHOD(CreateQuery)(THIS_ D3DQUERYTYPE Type,IDirect3DQuery9** ppQuery);

#ifdef XENON
  STDMETHOD_(D3DVOID, Present)(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,void* hDestWindowOverride,void* pDirtyRegion);
  STDMETHOD(GetBackBuffer)(THIS_ UINT iSwapChain,UINT iBackBuffer,UINT Type,IDirect3DSurface9** ppBackBuffer);
  STDMETHOD(Resolve)(THIS_ DWORD Flags, CONST D3DRECT *pSourceRect, D3DBaseTexture *pDestTexture, CONST D3DPOINT *pDestPoint, UINT DestLevel, UINT DestSliceOrFace, CONST D3DVECTOR4* pClearColor, float ClearZ, DWORD ClearStencil, CONST D3DRESOLVE_PARAMETERS* pParameters);
  STDMETHOD(CreateRenderTarget)(THIS_ UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface,CONST D3DSURFACE_PARAMETERS* pSharedHandle);
  STDMETHOD(CreateDepthStencilSurface)(THIS_ UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,IDirect3DSurface9** ppSurface,CONST D3DSURFACE_PARAMETERS* pSharedHandle);
#else
  STDMETHOD(CreateRenderTarget)(THIS_ UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle);
  STDMETHOD(CreateDepthStencilSurface)(THIS_ UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle);
  STDMETHOD(SetDialogBoxMode)(THIS_ BOOL bEnableDialogs);
  STDMETHOD_(UINT, GetNumberOfSwapChains)(THIS);
  STDMETHOD(UpdateSurface)(THIS_ IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestinationSurface,CONST POINT* pDestPoint);
  STDMETHOD(UpdateTexture)(THIS_ IDirect3DBaseTexture9* pSourceTexture,IDirect3DBaseTexture9* pDestinationTexture);
  STDMETHOD(GetTransform)(THIS_ D3DTRANSFORMSTATETYPE State,D3DMATRIX* pMatrix);
  STDMETHOD(MultiplyTransform)(THIS_ D3DTRANSFORMSTATETYPE,CONST D3DMATRIX*);
  STDMETHOD(SetMaterial)(THIS_ CONST D3DMATERIAL9* pMaterial);
  STDMETHOD(GetMaterial)(THIS_ D3DMATERIAL9* pMaterial);
  STDMETHOD(SetLight)(THIS_ DWORD Index,CONST D3DLIGHT9*);
  STDMETHOD(GetLight)(THIS_ DWORD Index,D3DLIGHT9*);
  STDMETHOD(LightEnable)(THIS_ DWORD Index,BOOL Enable);
  STDMETHOD(GetLightEnable)(THIS_ DWORD Index,BOOL* pEnable);
  STDMETHOD(BeginStateBlock)(THIS);
  STDMETHOD(EndStateBlock)(THIS_ IDirect3DStateBlock9** ppSB);
  STDMETHOD(SetClipStatus)(THIS_ CONST D3DCLIPSTATUS9* pClipStatus);
  STDMETHOD(GetClipStatus)(THIS_ D3DCLIPSTATUS9* pClipStatus);
  STDMETHOD(GetTextureStageState)(THIS_ DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue);
  STDMETHOD(ValidateDevice)(THIS_ DWORD* pNumPasses);
  STDMETHOD(SetPaletteEntries)(THIS_ UINT PaletteNumber,CONST PALETTEENTRY* pEntries);
  STDMETHOD(GetPaletteEntries)(THIS_ UINT PaletteNumber,PALETTEENTRY* pEntries);
  STDMETHOD(SetCurrentTexturePalette)(THIS_ UINT PaletteNumber);
  STDMETHOD(GetCurrentTexturePalette)(THIS_ UINT *PaletteNumber);
  STDMETHOD(SetSoftwareVertexProcessing)(THIS_ BOOL bSoftware);
  STDMETHOD_(BOOL, GetSoftwareVertexProcessing)(THIS);
  STDMETHOD(SetNPatchMode)(THIS_ float nSegments);
  STDMETHOD_(float, GetNPatchMode)(THIS);
  STDMETHOD(ProcessVertices)(THIS_ UINT SrcStartIndex,UINT DestIndex,UINT VertexCount,IDirect3DVertexBuffer9* pDestBuffer,IDirect3DVertexDeclaration9* pVertexDecl,DWORD Flags);
  STDMETHOD(DrawRectPatch)(THIS_ UINT Handle,CONST float* pNumSegs,CONST D3DRECTPATCH_INFO* pRectPatchInfo);
  STDMETHOD(DrawTriPatch)(THIS_ UINT Handle,CONST float* pNumSegs,CONST D3DTRIPATCH_INFO* pTriPatchInfo);
  STDMETHOD(DeletePatch)(THIS_ UINT Handle);
  STDMETHOD(Present)(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion);
  STDMETHOD(GetBackBuffer)(THIS_ UINT iSwapChain,UINT iBackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface9** ppBackBuffer);
#endif
};

#elif defined (DIRECT3D10)

interface CMyDirect3DDevice10 : public ID3D10Device
{
public:
  /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj);
    STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

  virtual void STDMETHODCALLTYPE VSSetConstantBuffers( 
    /* [in] */ UINT StartConstantSlot,
    /* [in] */ UINT NumBuffers,
    /* [size_is][in] */ ID3D10Buffer *const *ppConstantBuffers) ;

  virtual void STDMETHODCALLTYPE PSSetShaderResources( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumViews,
    /* [size_is][in] */ ID3D10ShaderResourceView *const *ppShaderResourceViews) ;

  virtual void STDMETHODCALLTYPE PSSetShader( 
    /* [in] */ ID3D10PixelShader *pPixelShader) ;

  virtual void STDMETHODCALLTYPE PSSetSamplers( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumSamplers,
    /* [size_is][in] */ ID3D10SamplerState *const *ppSamplers) ;

  virtual void STDMETHODCALLTYPE VSSetShader( 
    /* [in] */ ID3D10VertexShader *pVertexShader) ;

  virtual void STDMETHODCALLTYPE DrawIndexed( 
    /* [in] */ UINT IndexCount,
    /* [in] */ UINT StartIndexLocation,
    /* [in] */ INT BaseVertexLocation) ;

  virtual void STDMETHODCALLTYPE Draw( 
    /* [in] */ UINT VertexCount,
    /* [in] */ UINT StartVertexLocation) ;

  virtual void STDMETHODCALLTYPE PSSetConstantBuffers( 
    /* [in] */ UINT StartConstantSlot,
    /* [in] */ UINT NumBuffers,
    /* [size_is][in] */ ID3D10Buffer *const *ppConstantBuffers) ;

  virtual void STDMETHODCALLTYPE IASetInputLayout( 
    /* [in] */ ID3D10InputLayout *pInputLayout) ;

  virtual void STDMETHODCALLTYPE IASetVertexBuffers( 
    /* [in] */ UINT StartSlot,
    /* [in] */ UINT NumBuffers,
    /* [size_is][in] */ ID3D10Buffer *const *ppVertexBuffers,
    /* [size_is][in] */ const UINT *pStrides,
    /* [size_is][in] */ const UINT *pOffsets) ;

  virtual void STDMETHODCALLTYPE IASetIndexBuffer( 
    /* [in] */ ID3D10Buffer *pIndexBuffer,
    /* [in] */ DXGI_FORMAT Format,
    /* [in] */ UINT Offset) ;

  virtual void STDMETHODCALLTYPE DrawIndexedInstanced( 
    /* [in] */ UINT IndexCountPerInstance,
    /* [in] */ UINT InstanceCount,
    /* [in] */ UINT StartIndexLocation,
    /* [in] */ INT BaseVertexLocation,
    /* [in] */ UINT StartInstanceLocation) ;

  virtual void STDMETHODCALLTYPE DrawInstanced( 
    /* [in] */ UINT VertexCountPerInstance,
    /* [in] */ UINT InstanceCount,
    /* [in] */ UINT StartVertexLocation,
    /* [in] */ UINT StartInstanceLocation) ;

  virtual void STDMETHODCALLTYPE GSSetConstantBuffers( 
    /* [in] */ UINT StartConstantSlot,
    /* [in] */ UINT NumBuffers,
    /* [size_is][in] */ ID3D10Buffer *const *ppConstantBuffers) ;

  virtual void STDMETHODCALLTYPE GSSetShader( 
    /* [in] */ ID3D10GeometryShader *pShader) ;

  virtual void STDMETHODCALLTYPE IASetPrimitiveTopology( 
    /* [in] */ D3D10_PRIMITIVE_TOPOLOGY Topology) ;

  virtual void STDMETHODCALLTYPE VSSetShaderResources( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumViews,
    /* [size_is][in] */ ID3D10ShaderResourceView *const *ppShaderResourceViews) ;

  virtual void STDMETHODCALLTYPE VSSetSamplers( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumSamplers,
    /* [size_is][in] */ ID3D10SamplerState *const *ppSamplers) ;

  virtual void STDMETHODCALLTYPE SetPredication( 
    /* [in] */ ID3D10Predicate *pPredicate,
    /* [in] */ BOOL PredicateValue) ;

  virtual void STDMETHODCALLTYPE GSSetShaderResources( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumViews,
    /* [size_is][in] */ ID3D10ShaderResourceView *const *ppShaderResourceViews) ;

  virtual void STDMETHODCALLTYPE GSSetSamplers( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumSamplers,
    /* [size_is][in] */ ID3D10SamplerState *const *ppSamplers) ;

  virtual void STDMETHODCALLTYPE OMSetRenderTargets( 
    /* [in] */ UINT NumViews,
    /* [size_is][in] */ ID3D10RenderTargetView *const *ppRenderTargetViews,
    /* [in] */ ID3D10DepthStencilView *pDepthStencilView) ;

  virtual void STDMETHODCALLTYPE OMSetBlendState( 
    /* [in] */ ID3D10BlendState *pBlendState,
    /* [in] */ const FLOAT BlendFactor[ 4 ],
    /* [in] */ UINT SampleMask) ;

  virtual void STDMETHODCALLTYPE OMSetDepthStencilState( 
    /* [in] */ ID3D10DepthStencilState *pDepthStencilState,
    /* [in] */ UINT StencilRef) ;

  virtual void STDMETHODCALLTYPE SOSetTargets( 
    /* [in] */ UINT NumBuffers,
    /* [size_is][in] */ ID3D10Buffer *const *ppSOTargets,
    /* [size_is][in] */ const UINT *pOffsets) ;

  virtual void STDMETHODCALLTYPE DrawAuto( void) ;

  virtual void STDMETHODCALLTYPE RSSetState( 
    /* [in] */ ID3D10RasterizerState *pRasterizerState) ;

  virtual void STDMETHODCALLTYPE RSSetViewports( 
    /* [in] */ UINT NumViewports,
    /* [size_is][in] */ const D3D10_VIEWPORT *pViewports) ;

  virtual void STDMETHODCALLTYPE RSSetScissorRects( 
    /* [in] */ UINT NumRects,
    /* [size_is][in] */ const D3D10_RECT *pRects) ;

  virtual void STDMETHODCALLTYPE ClearRenderTargetView( 
    /* [in] */ ID3D10RenderTargetView *pRenderTargetView,
    /* [in] */ const FLOAT ColorRGBA[ 4 ]) ;

  virtual void STDMETHODCALLTYPE ClearDepthStencilView( 
    /* [in] */ ID3D10DepthStencilView *pDepthStencilView,
    /* [in] */ UINT Flags,
    /* [in] */ FLOAT Depth,
    /* [in] */ UINT8 Stencil) ;

  virtual void STDMETHODCALLTYPE GenerateMips( 
    /* [in] */ ID3D10ShaderResourceView *pShaderResourceView) ;

  virtual void STDMETHODCALLTYPE VSGetConstantBuffers( 
    /* [in] */ UINT StartConstantSlot,
    /* [in] */ UINT NumBuffers,
    /* [size_is][out] */ ID3D10Buffer **ppConstantBuffers) ;

  virtual void STDMETHODCALLTYPE PSGetShaderResources( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumViews,
    /* [size_is][out] */ ID3D10ShaderResourceView **ppShaderResourceViews) ;

  virtual void STDMETHODCALLTYPE PSGetShader( 
    /* [out][in] */ ID3D10PixelShader **ppPixelShader) ;

  virtual void STDMETHODCALLTYPE PSGetSamplers( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumSamplers,
    /* [size_is][out] */ ID3D10SamplerState **ppSamplers) ;

  virtual void STDMETHODCALLTYPE VSGetShader( 
    /* [out][in] */ ID3D10VertexShader **ppVertexShader) ;

  virtual void STDMETHODCALLTYPE PSGetConstantBuffers( 
    /* [in] */ UINT StartConstantSlot,
    /* [in] */ UINT NumBuffers,
    /* [size_is][out] */ ID3D10Buffer **ppConstantBuffers) ;

  virtual void STDMETHODCALLTYPE IAGetInputLayout( 
    /* [out][in] */ ID3D10InputLayout **ppInputLayout) ;

  virtual void STDMETHODCALLTYPE IAGetVertexBuffers( 
    /* [in] */ UINT StartSlot,
    /* [in] */ UINT NumBuffers,
    /* [size_is][out] */ ID3D10Buffer **ppVertexBuffers,
    /* [size_is][out] */ UINT *pStrides,
    /* [size_is][out] */ UINT *pOffsets) ;

  virtual void STDMETHODCALLTYPE IAGetIndexBuffer( 
    /* [out] */ ID3D10Buffer **pIndexBuffer,
    /* [out] */ DXGI_FORMAT *Format,
    /* [out] */ UINT *Offset) ;

  virtual void STDMETHODCALLTYPE GSGetConstantBuffers( 
    /* [in] */ UINT StartConstantSlot,
    /* [in] */ UINT NumBuffers,
    /* [size_is][out] */ ID3D10Buffer **ppConstantBuffers) ;

  virtual void STDMETHODCALLTYPE GSGetShader( 
    /* [out] */ ID3D10GeometryShader **ppGeometryShader) ;

  virtual void STDMETHODCALLTYPE IAGetPrimitiveTopology( 
    /* [out] */ D3D10_PRIMITIVE_TOPOLOGY *pTopology) ;

  virtual void STDMETHODCALLTYPE VSGetShaderResources( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumViews,
    /* [size_is][out] */ ID3D10ShaderResourceView **ppShaderResourceViews) ;

  virtual void STDMETHODCALLTYPE VSGetSamplers( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumSamplers,
    /* [size_is][out] */ ID3D10SamplerState **ppSamplers) ;

  virtual void STDMETHODCALLTYPE GetPredication( 
    /* [out] */ ID3D10Predicate **ppPredicate,
    /* [out] */ BOOL *pPredicateValue) ;

  virtual void STDMETHODCALLTYPE GSGetShaderResources( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumViews,
    /* [size_is][out] */ ID3D10ShaderResourceView **ppShaderResourceViews) ;

  virtual void STDMETHODCALLTYPE GSGetSamplers( 
    /* [in] */ UINT Offset,
    /* [in] */ UINT NumSamplers,
    /* [size_is][out] */ ID3D10SamplerState **ppSamplers) ;

  virtual void STDMETHODCALLTYPE OMGetRenderTargets( 
    /* [in] */ UINT NumViews,
    /* [size_is][out] */ ID3D10RenderTargetView **ppRenderTargetViews,
    /* [out] */ ID3D10DepthStencilView **ppDepthStencilView) ;

  virtual void STDMETHODCALLTYPE OMGetBlendState( 
    /* [out] */ ID3D10BlendState **ppBlendState,
    /* [out] */ FLOAT BlendFactor[ 4 ],
    /* [out] */ UINT *pSampleMask) ;

  virtual void STDMETHODCALLTYPE OMGetDepthStencilState( 
    /* [out] */ ID3D10DepthStencilState **ppDepthStencilState,
    /* [out] */ UINT *pStencilRef) ;

  virtual void STDMETHODCALLTYPE SOGetTargets( 
    /* [in] */ UINT NumBuffers,
    /* [size_is][out] */ ID3D10Buffer **ppSOTargets,
    /* [size_is][out] */ UINT *pOffsets) ;

  virtual void STDMETHODCALLTYPE RSGetState( 
    /* [out] */ ID3D10RasterizerState **ppRasterizerState) ;

  virtual void STDMETHODCALLTYPE RSGetViewports( 
    /* [out][in] */ UINT *NumViewports,
    /* [size_is][out] */ D3D10_VIEWPORT *pViewports) ;

  virtual void STDMETHODCALLTYPE RSGetScissorRects( 
    /* [out][in] */ UINT *NumRects,
    /* [size_is][out] */ D3D10_RECT *pRects) ;

  virtual HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason( void) ;

  virtual HRESULT STDMETHODCALLTYPE SetExceptionMode( 
    UINT RaiseFlags) ;

  virtual UINT STDMETHODCALLTYPE GetExceptionMode( void) ;

  virtual HRESULT STDMETHODCALLTYPE GetPrivateData( 
    /* [in] */ REFGUID guid,
    /* [out][in] */ UINT *pDataSize,
    /* [size_is][out] */ void *pData) ;

  virtual HRESULT STDMETHODCALLTYPE SetPrivateData( 
    /* [in] */ REFGUID guid,
    /* [in] */ UINT DataSize,
    /* [size_is][in] */ const void *pData) ;

  virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface( 
    /* [in] */ REFGUID guid,
    /* [in] */ const IUnknown *pData) ;

  virtual void STDMETHODCALLTYPE Flush( void) ;

  virtual HRESULT STDMETHODCALLTYPE CreateBuffer( 
    /* [in] */ const D3D10_BUFFER_DESC *pDesc,
    /* [in] */ const D3D10_SUBRESOURCE_DATA *pInitialData,
    /* [out] */ ID3D10Buffer **ppBuffer) ;

  virtual HRESULT STDMETHODCALLTYPE CreateTexture1D( 
    /* [in] */ const D3D10_TEXTURE1D_DESC *pDesc,
    /* [in] */ const D3D10_SUBRESOURCE_DATA *pInitialData,
    /* [out] */ ID3D10Texture1D **ppTexture1D) ;

  virtual HRESULT STDMETHODCALLTYPE CreateTexture2D( 
    /* [in] */ const D3D10_TEXTURE2D_DESC *pDesc,
    /* [in] */ const D3D10_SUBRESOURCE_DATA *pInitialData,
    /* [out] */ ID3D10Texture2D **ppTexture2D) ;

  virtual HRESULT STDMETHODCALLTYPE CreateTexture3D( 
    /* [in] */ const D3D10_TEXTURE3D_DESC *pDesc,
    /* [in] */ const D3D10_SUBRESOURCE_DATA *pInitialData,
    /* [out] */ ID3D10Texture3D **ppTexture3D) ;

  virtual HRESULT STDMETHODCALLTYPE CreateShaderResourceView( 
    /* [in] */ ID3D10Resource *pResource,
    /* [in] */ const D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc,
    /* [out] */ ID3D10ShaderResourceView **ppSRView) ;

  virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetView( 
    /* [in] */ ID3D10Resource *pResource,
    /* [in] */ const D3D10_RENDER_TARGET_VIEW_DESC *pDesc,
    /* [out] */ ID3D10RenderTargetView **ppRTView) ;

  virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilView( 
    /* [in] */ ID3D10Resource *pResource,
    /* [in] */ const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc,
    /* [out] */ ID3D10DepthStencilView **ppDepthStencilView) ;

  virtual HRESULT STDMETHODCALLTYPE CreateInputLayout( 
    /* [size_is][in] */ const D3D10_INPUT_ELEMENT_DESC *pInputElementDescs,
    /* [in] */ UINT NumElements,
    /* [in] */ const void *pShaderBytecodeWithInputSignature,
    /* [in] */ SIZE_T BytecodeLength,
    /* [out] */ ID3D10InputLayout **ppInputLayout) ;

  virtual HRESULT STDMETHODCALLTYPE CreateVertexShader( 
    /* [in] */ const void *pShaderBytecode,
    /* [in] */ SIZE_T BytecodeLength,
    /* [out] */ ID3D10VertexShader **ppVertexShader) ;

  virtual HRESULT STDMETHODCALLTYPE CreateGeometryShader( 
    /* [in] */ const void *pShaderBytecode,
    /* [in] */ SIZE_T BytecodeLength,
    /* [out] */ ID3D10GeometryShader **ppGeometryShader) ;

  virtual HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput( 
    /* [in] */ const void *pShaderBytecode,
    /* [in] */ SIZE_T BytecodeLength,
    /* [size_is][in] */ const D3D10_SO_DECLARATION_ENTRY *pSODeclaration,
    /* [in] */ UINT NumEntries,
    /* [in] */ UINT OutputStreamStride,
    /* [out] */ ID3D10GeometryShader **ppGeometryShader) ;

  virtual HRESULT STDMETHODCALLTYPE CreatePixelShader( 
    /* [in] */ const void *pShaderBytecode,
    /* [in] */ SIZE_T BytecodeLength,
    /* [out] */ ID3D10PixelShader **ppPixelShader) ;

  virtual HRESULT STDMETHODCALLTYPE CreateBlendState( 
    /* [in] */ const D3D10_BLEND_DESC *pBlendStateDesc,
    /* [out] */ ID3D10BlendState **ppBlendState) ;

  virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilState( 
    /* [in] */ const D3D10_DEPTH_STENCIL_DESC *pDepthStencilDesc,
    /* [out] */ ID3D10DepthStencilState **ppDepthStencilState) ;

  virtual HRESULT STDMETHODCALLTYPE CreateRasterizerState( 
    /* [in] */ const D3D10_RASTERIZER_DESC *pRasterizerDesc,
    /* [out] */ ID3D10RasterizerState **ppRasterizerState) ;

  virtual HRESULT STDMETHODCALLTYPE CreateSamplerState( 
    /* [in] */ const D3D10_SAMPLER_DESC *pSamplerDesc,
    /* [out] */ ID3D10SamplerState **ppSamplerState) ;

  virtual HRESULT STDMETHODCALLTYPE CreateQuery( 
    /* [in] */ const D3D10_QUERY_DESC *pQueryDesc,
    /* [out] */ ID3D10Query **ppQuery) ;

  virtual HRESULT STDMETHODCALLTYPE CreatePredicate( 
    /* [in] */ const D3D10_QUERY_DESC *pPredicateDesc,
    /* [out] */ ID3D10Predicate **ppPredicate) ;

  virtual HRESULT STDMETHODCALLTYPE CreateCounter( 
    /* [in] */ const D3D10_COUNTER_DESC *pCounterDesc,
    /* [out] */ ID3D10Counter **ppCounter) ;

  virtual HRESULT STDMETHODCALLTYPE CheckFormatSupport( 
    /* [in] */ DXGI_FORMAT Format,
    /* [retval][out] */ UINT *pFormatSupport) ;

  virtual HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels( 
    DXGI_FORMAT Format,
    /* [in] */ UINT SampleCount,
    /* [retval][out] */ UINT *pNumQualityLevels) ;

  virtual void STDMETHODCALLTYPE CheckCounterInfo( 
    /* [retval][out] */ D3D10_COUNTER_INFO *pCounterInfo) ;

  virtual void STDMETHODCALLTYPE CopySubresourceRegion(
    ID3D10Resource * pDstResource,
    UINT DstSubresource,
    UINT DstX,
    UINT DstY,
    UINT DstZ,
    ID3D10Resource * pSrcResource,
    UINT SrcSubresource,
    const D3D10_BOX * pSrcBox
    );

  virtual void STDMETHODCALLTYPE UpdateSubresource(
    ID3D10Resource * pDstResource,
    UINT DstSubresource,
    const D3D10_BOX * pDstBox,
    const void *pSrcData,
    UINT SrcRowPitch,
    UINT SrcDepthPitch
    );

  virtual void STDMETHODCALLTYPE ResolveSubresource(
    ID3D10Resource * pDstResource,
    UINT DstSubresource,
    ID3D10Resource * pSrcResource,
    UINT SrcSubresource,
    DXGI_FORMAT Format
    );

  virtual void STDMETHODCALLTYPE CopyResource(ID3D10Resource * pDstResource, ID3D10Resource * pSrcResource);

  virtual void STDMETHODCALLTYPE ClearState();

  virtual HRESULT STDMETHODCALLTYPE OpenSharedResource(
    HANDLE hResource,
    REFIID ReturnedInterface,
    void ** ppResource
    );

  virtual HRESULT STDMETHODCALLTYPE CheckCounter( 
    __in  const D3D10_COUNTER_DESC *pDesc,
    __out  D3D10_COUNTER_TYPE *pType,
    __out  UINT *pActiveCounters,
    __out_ecount_opt(*pNameLength)  LPSTR wszName,
    __inout_opt  UINT *pNameLength,
    __out_ecount_opt(*pUnitsLength)  LPSTR wszUnits,
    __inout_opt  UINT *pUnitsLength,
    __out_ecount_opt(*pDescriptionLength)  LPSTR wszDescription,
    __inout_opt  UINT *pDescriptionLength) ;

  virtual UINT STDMETHODCALLTYPE GetCreationFlags();

	virtual void STDMETHODCALLTYPE SetTextFilterSize( 
		__in  UINT Width,
		__in  UINT Height);

	virtual void STDMETHODCALLTYPE GetTextFilterSize( 
		__out_opt  UINT *pWidth,
		__out_opt  UINT *pHeight);
};

#endif // defined(DIRECT3D9) || defined(OPENGL)

#define D3DRGBA(r, g, b, a) \
    (   (((int)((a) * 255)) << 24) | (((int)((r) * 255)) << 16) \
    |   (((int)((g) * 255)) << 8) | (int)((b) * 255) \
    )

_inline DWORD FLOATtoDWORD( float f )
{
  union FLOATDWORD
  {
    float f;
    DWORD dw;
  };

  FLOATDWORD val;
  val.f = f;
  return val.dw;
}

//=======================================================================

//=====================================================


struct STexFiller;

#define BUFFERED_VERTS 256

struct SD3DContext
{
  HWND m_hWnd;
  int m_X;
  int m_Y;
  int m_Width;
  int m_Height;
#if defined(DIRECT3D10)
	IDXGISwapChain* m_pSwapChain;
	ID3D10RenderTargetView* m_pBackBuffer;
#endif
};

#define D3DAPPERR_NODIRECT3D          0x82000001
#define D3DAPPERR_NOWINDOW            0x82000002
#define D3DAPPERR_NOCOMPATIBLEDEVICES 0x82000003
#define D3DAPPERR_NOWINDOWABLEDEVICES 0x82000004
#define D3DAPPERR_NOHARDWAREDEVICE    0x82000005
#define D3DAPPERR_HALNOTCOMPATIBLE    0x82000006
#define D3DAPPERR_NOWINDOWEDHAL       0x82000007
#define D3DAPPERR_NODESKTOPHAL        0x82000008
#define D3DAPPERR_NOHALTHISMODE       0x82000009
#define D3DAPPERR_NONZEROREFCOUNT     0x8200000a
#define D3DAPPERR_MEDIANOTFOUND       0x8200000b
#define D3DAPPERR_RESIZEFAILED        0x8200000c

typedef TDList<SVertPool> TVertPool;

#define SDeferMeshVert struct_VERTEX_FORMAT_P3F_TEX2F

typedef std::vector<struct_VERTEX_FORMAT_P3F_TEX2F> t_arrDeferredMeshVertBuff;
typedef std::vector<uint16> t_arrDeferredMeshIndBuff;

struct SD3DRenderTarget
{
  LPDIRECT3DSURFACE9 m_pRT;
  LPDIRECT3DSURFACE9 m_pZB;
};

const float g_fEyeAdaptionLowerPercent = 0.05f;	// 5% percentil
const float g_fEyeAdaptionMidPercent = 0.5f;		// 50% percentil
const float g_fEyeAdaptionUpperPercent = 0.95f;	// 95% percentil

// normalized range: (0..1)  means (0..MaxVal) not normalized
// (multiply normalized values with MaxVal to get the real value)
struct SHistogram
{
  float Lum[256];				// in normalized range 0..1, sum of all values is normalized to 1.0
  float LowPercentil;		// not normalized, lower percentil value (e.g. 5% percentil)
  float MidPercentil;		// not normalized, high percentil value (e.g. 50% percentil)
  float HighPercentil;	// not normalized, mid percentil value (e.g. 95% percentil)
  float MaxVal;					// max value not normalized
};

struct SGamma
{
  //byte Ramp[256];
  float LowPercentil;		// not normalized, lower percentil value (e.g. 5% percentil)
  float MidPercentil;		// not normalized, high percentil value (e.g. 50% percentil)
  float HighPercentil;	// not normalized, mid percentil value (e.g. 95% percentil)
  float Bias;						//
  float CurLuminance;		//
};

// Texture coordinate rectangle
struct CoordRect
{
  float fLeftU, fTopV;
  float fRightU, fBottomV;
};

HRESULT GetTextureRect(CTexture *pTexture, RECT* pRect);
HRESULT GetTextureCoords(CTexture *pTexSrc, RECT* pRectSrc, CTexture *pTexDest, RECT* pRectDest, CoordRect* pCoords);
HRESULT GetSampleOffsets_BloomBilinear(DWORD dwD3DTexSize, float afTexCoordOffset[15], Vec4* avColorWeight, float fDeviation, float fMultiplier);
void DrawFullScreenQuad(float fLeftU, float fTopV, float fRightU, float fBottomV, bool bClampToScreenRes = true);
void DrawFullScreenQuad(CoordRect c, bool bClampToScreenRes = true);
HRESULT GetSampleOffsets_GaussBlur5x5(DWORD dwD3DTexWidth, DWORD dwD3DTexHeight, Vec4* avTexCoordOffset, Vec4* avSampleWeight, FLOAT fMultiplier);
HRESULT GetSampleOffsets_GaussBlur5x5Bilinear(DWORD dwD3DTexWidth, DWORD dwD3DTexHeight, Vec4* avTexCoordOffset, Vec4* avSampleWeight, FLOAT fMultiplier);
float GaussianDistribution( float x, float y, float rho);

#include "../Common/3Dc/CompressorLib.h"

#ifdef OPENGL
typedef void *HINSTANCE;
#endif

#if defined (DIRECT3D10)  
struct SStateBlend
{
  UnINT64 nHashVal;
  D3D10_BLEND_DESC Desc;
  ID3D10BlendState* pState;
  SStateBlend() { memset(this, 0, sizeof(*this)); }
  static uint64 GetHash(const D3D10_BLEND_DESC& InDesc)
  {
    UnINT64 nHash;
    nHash.i.Low = InDesc.AlphaToCoverageEnable | 
                (InDesc.BlendEnable[0]<<1) | (InDesc.BlendEnable[1]<<2) | (InDesc.BlendEnable[2]<<3) | (InDesc.BlendEnable[3]<<4) |
                (InDesc.SrcBlend<<5) | (InDesc.DestBlend<<10) | 
                (InDesc.SrcBlendAlpha<<15) | (InDesc.DestBlendAlpha<<20) | 
                (InDesc.BlendOp<<25) | (InDesc.BlendOpAlpha<<28);
    nHash.i.High = InDesc.RenderTargetWriteMask[0] | (InDesc.RenderTargetWriteMask[1]<<4) | (InDesc.RenderTargetWriteMask[2]<<8) | (InDesc.RenderTargetWriteMask[3]<<12);

    return nHash.SortVal;
  }
};
struct SStateRaster
{
  uint32 nHashVal;
  uint64 nValuesHash;
  D3D10_RASTERIZER_DESC Desc;
  ID3D10RasterizerState* pState;
  SStateRaster() {
    memset(this, 0, sizeof(*this)); 
    Desc.DepthClipEnable = true;
  }
  static uint32 GetHash(const D3D10_RASTERIZER_DESC& InDesc)
  {
    uint32 nHash;
    nHash =      InDesc.FillMode | (InDesc.CullMode<<2) |
                (InDesc.DepthClipEnable<<4) | (InDesc.FrontCounterClockwise<<5) | 
                (InDesc.ScissorEnable<<6) | (InDesc.MultisampleEnable<<7) | (InDesc.AntialiasedLineEnable<<8) |
                (InDesc.DepthBias<<9);
    return nHash;
  }

  static uint64 GetValuesHash(const D3D10_RASTERIZER_DESC& InDesc)
  {
    uint64 nHash;
    const uint32* pDepthBiasClamp = reinterpret_cast<const uint32*>(&(InDesc.DepthBiasClamp));
    const uint32* pSlopeScaledDepthBias = reinterpret_cast<const uint32*> (&(InDesc.SlopeScaledDepthBias));
    nHash = ( ( (uint64)*pDepthBiasClamp ) | 
              ( (uint64)*pSlopeScaledDepthBias) << 32 );
    return nHash;
  }

};
_inline uint32 sStencilState(const D3D10_DEPTH_STENCILOP_DESC& Desc)
{
  uint32 nST = (Desc.StencilFailOp<<0) | 
               (Desc.StencilDepthFailOp<<4) |
               (Desc.StencilPassOp<<8) |
               (Desc.StencilFunc<<12);
  return nST;
}
struct SStateDepth
{
  uint64 nHashVal;
  D3D10_DEPTH_STENCIL_DESC Desc;
  ID3D10DepthStencilState* pState;
  SStateDepth() { memset(this, 0, sizeof(*this)); }
  static uint64 GetHash(const D3D10_DEPTH_STENCIL_DESC& InDesc)
  {
    uint64 nHash;
    nHash = (InDesc.DepthEnable<<0) | 
						(InDesc.DepthWriteMask<<1) |
            (InDesc.DepthFunc<<2) | 
						(InDesc.StencilEnable<<6) |
						(InDesc.StencilReadMask<<7) | 
						(InDesc.StencilWriteMask<<15) |
            (((uint64)sStencilState(InDesc.FrontFace))<<23) |
            (((uint64)sStencilState(InDesc.BackFace))<<39);
    return nHash;
  }
};\

#define MAX_STAGING_BUFFERS 512

#endif

//======================================================================
/// Direct3D Render driver class

#if defined (WIN32) || defined(XENON)
class CAsyncShaderManager;
typedef const std::auto_ptr<CAsyncShaderManager> TConstAsyncShaderManagerPtr;
#endif

class CD3D9Renderer : public CRenderer
{
  friend struct SPixFormat;

#if defined (WIN32) || defined(XENON)
private:
	TConstAsyncShaderManagerPtr m_pAsyncShaderManager;
public:
	TConstAsyncShaderManagerPtr& GetAsyncShaderManager() const { return m_pAsyncShaderManager; }
#endif

public:
  CD3D9Renderer();
  ~CD3D9Renderer();

	const SRenderTileInfo& GetRenderTileInfo() const { return m_RenderTileInfo; }

protected:

// Windows context
  char      m_WinTitle[80];
  HINSTANCE m_hInst;            
  HWND      m_hWnd;              // The main app window
  HWND      m_hWndDesktop;       // The desktop window
  HANDLE    m_hLibHandle3DC;

#if defined (XENON)
  // This will point to the active scenario list.
  CONST TILING_SCENARIO *m_pTilingScenarios;
  DWORD             m_dwTilingScenarioIndex;
  DWORD             m_dwCurrentBuffer;
  D3DPRESENT_PARAMETERS m_d3dpp;
  D3DCAPS9          m_d3dCaps;
  CD3DSettings      m_D3DSettings;
#endif
#if defined (DIRECT3D9) || defined (OPENGL)
  LPDIRECT3D9       m_pD3D;              // The main D3D object
  //CD3DEnumeration   m_D3DEnum;

  // From D3D.
  // Main objects used for creating and rendering the 3D scene
  LPDIRECT3DSURFACE9 m_pBackBuffer;
  LPDIRECT3DSURFACE9 m_pZBuffer;
#elif defined (DIRECT3D10)  
  ID3D10RenderTargetView* m_pBackBuffer;
  ID3D10DepthStencilView* m_pZBuffer;
  IDXGISwapChain* m_pSwapChain;
#endif

  DWORD             m_dwCreateFlags;     // Indicate sw or hw vertex processing
  DWORD             m_dwWindowStyle;     // Saved window style for mode switches
  char              m_strDeviceStats[90];// String to hold D3D device stats

  int               m_SceneRecurseCount;

  D3DGAMMARAMP      m_SystemGammaRamp;

  SRenderTileInfo m_RenderTileInfo;

//==================================================================

public:
  D3DQuery  *m_pQuery[2]; 
  D3DQuery  *m_pHDRLumQuery; 
  D3DQuery  *m_pQueryPerf; 
  int        m_nQueryCount; 

#define MAX_DYNVB3D_VERTS       16384
#define POOL_P3F_COL4UB_TEX2F   0
#define POOL_P3F_TEX2F          1
#define POOL_P3F                2
#define POOL_TRP3F_COL4UB_TEX2F 3
#define POOL_TRP3F_TEX2F_TEX3F  4
#define POOL_P3F_TEX3F          5
#define POOL_P3F_TEX2F_TEX3F    6
#define POOL_MAX                7

#if defined (DIRECT3D9) || defined (OPENGL)
  const D3DPRESENT_PARAMETERS *m_pd3dpp;         // Parameters for CreateDevice/Reset
  const D3DCAPS9   *m_pd3dCaps;           // Caps for the device
  D3DSURFACE_DESC   m_d3dsdBackBuffer;      // Surface desc of the Backbuffer
  D3DFORMAT         m_ZFormat;

  LPDIRECT3DVERTEXDECLARATION9 m_pLastVDeclaration;

  IDirect3DVertexBuffer9 *m_pVBAr[POOL_MAX][4];
  IDirect3DVertexBuffer9 *m_pVB[POOL_MAX];
  IDirect3DIndexBuffer9 *m_pIB;
  int m_nCurBuf[POOL_MAX];
  int m_nVertsDMesh[POOL_MAX];
  int m_nOffsDMesh[POOL_MAX];

  IDirect3DVertexBuffer9 *m_pUnitFrustumVB;
  IDirect3DIndexBuffer9 *m_pUnitFrustumIB;

  IDirect3DVertexBuffer9 *m_pUnitSphereVB;
  IDirect3DIndexBuffer9 *m_pUnitSphereIB;

  D3DVIEWPORT9 m_CurViewport;
  D3DVIEWPORT9 m_NewViewport;
  LPDIRECT3DSURFACE9 m_pSysLumSurface;

  LPDIRECT3DDEVICE9 m_pd3dDevice;        // The D3D rendering device
  CMyDirect3DDevice9 *m_pMyd3dDevice;        // The D3D rendering device
  LPDIRECT3DDEVICE9 m_pActuald3dDevice;        // The D3D rendering device
#elif defined (DIRECT3D10)
  ID3D10Buffer *m_pVBAr[POOL_MAX][4];
  ID3D10Buffer *m_pVB[POOL_MAX];
  ID3D10Buffer *m_pIB;
  DynamicVB<byte> *m_pVBTemp[MAX_STAGING_BUFFERS];
  DynamicIB<ushort> *m_pIBTemp[MAX_STAGING_BUFFERS];
  int m_StagedStream[VSF_NUM];
  uint m_StagedStreamOffset[VSF_NUM];
	int m_nCurStagedVB;
	int m_nCurStagedIB;
  int m_nCurBuf[POOL_MAX];
  int m_nVertsDMesh[POOL_MAX];
  int m_nOffsDMesh[POOL_MAX];

  ID3D10Buffer *m_pUnitFrustumVB;
  ID3D10Buffer *m_pUnitFrustumIB;

  ID3D10Buffer *m_pUnitSphereVB;
  ID3D10Buffer *m_pUnitSphereIB;

  ID3D10InputLayout* m_pLastVDeclaration;
  ID3D10Texture2D *m_pSysLumSurface;

  DXGI_SURFACE_DESC m_d3dsdBackBuffer;   // Surface desc of the BackBuffer
  DXGI_FORMAT       m_ZFormat;

  ID3D10Device* m_pd3dDevice;        // The D3D rendering device
  CMyDirect3DDevice10 *m_pMyd3dDevice;        // The D3D rendering device
  ID3D10Device* m_pActuald3dDevice;        // The D3D rendering device

  ID3D10Debug*  m_pd3dDebug; 

	D3D10_PRIMITIVE_TOPOLOGY m_CurTopology;
  D3D10_VIEWPORT m_CurViewport;
  D3D10_VIEWPORT m_NewViewport;

  TArray<SStateBlend> m_StatesBL;
  TArray<SStateRaster> m_StatesRS;
  TArray<SStateDepth> m_StatesDP;
  uint m_nCurStateBL;
  uint m_nCurStateRS;
  uint m_nCurStateDP;
  uint8 m_nCurStencRef;
  bool m_bPendingSetWindow;
  bool SetBlendState(const SStateBlend *pNewState)
  {
    uint i;
    HRESULT hr = S_OK;
    uint64 nHash = SStateBlend::GetHash(pNewState->Desc);
    for (i=0; i<m_StatesBL.Num(); i++)
    {
      if (m_StatesBL[i].nHashVal.SortVal == nHash)
        break;
    }
    if (i == m_StatesBL.Num())
    {
      SStateBlend *pState = m_StatesBL.AddIndex(1);
      *pState = *pNewState;
      pState->nHashVal.SortVal = nHash;
      hr = m_pd3dDevice->CreateBlendState(&pState->Desc, &pState->pState);
      assert(SUCCEEDED(hr));
    }
    if (i != m_nCurStateBL)
    {
      m_nCurStateBL = i;
      m_pd3dDevice->OMSetBlendState(m_StatesBL[i].pState, 0, 0xffffffff);
    }
    return SUCCEEDED(hr);
  }
  bool SetRasterState(const SStateRaster *pNewState)
  {
    uint i;
    HRESULT hr = S_OK;
    BOOL bFSAA = m_RP.m_FSAAData.Type > 1;
    if (bFSAA != pNewState->Desc.MultisampleEnable)
    {
      static SStateRaster RS;
      RS = *pNewState;
      RS.Desc.MultisampleEnable = bFSAA;
      pNewState = &RS;
    }
    uint32 nHash = SStateRaster::GetHash(pNewState->Desc);
    uint64 nValuesHash = SStateRaster::GetValuesHash(pNewState->Desc);
    for (i=0; i<m_StatesRS.Num(); i++)
    {
      if (m_StatesRS[i].nHashVal == nHash && m_StatesRS[i].nValuesHash == nValuesHash)
        break;
    }
    if (i == m_StatesRS.Num())
    {
      SStateRaster *pState = m_StatesRS.AddIndex(1);
      *pState = *pNewState;
      pState->nHashVal = nHash;
      pState->nValuesHash = nValuesHash;
      hr = m_pd3dDevice->CreateRasterizerState(&pState->Desc, &pState->pState);
      assert(SUCCEEDED(hr));
    }
    if (i != m_nCurStateRS)
    {
      m_nCurStateRS = i;
      m_pd3dDevice->RSSetState(m_StatesRS[i].pState);
    }
    return SUCCEEDED(hr);
  }
  bool SetDepthState(const SStateDepth *pNewState, uint8 newStencRef)
  {
    uint i;
    HRESULT hr = S_OK;
    uint64 nHash = SStateDepth::GetHash(pNewState->Desc);
    for (i=0; i<m_StatesDP.Num(); i++)
    {
      if (m_StatesDP[i].nHashVal == nHash)
        break;
    }
    if (i == m_StatesDP.Num())
    {
      SStateDepth *pState = m_StatesDP.AddIndex(1);
      *pState = *pNewState;
      pState->nHashVal = nHash;
      hr = m_pd3dDevice->CreateDepthStencilState(&pState->Desc, &pState->pState);
      assert(SUCCEEDED(hr));
    }
    if (i != m_nCurStateDP || m_nCurStencRef != newStencRef)
    {
      m_nCurStateDP = i;
      m_nCurStencRef = newStencRef;
      m_pd3dDevice->OMSetDepthStencilState(m_StatesDP[i].pState, newStencRef);
    }
    return SUCCEEDED(hr);
  }
	
	void SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY topType)
	{
		if (m_CurTopology != topType)
		{
			m_CurTopology = topType;
			m_pd3dDevice->IASetPrimitiveTopology(m_CurTopology);
		}
	}
#endif

	SD3DSurface m_DepthBufferOrig;
	SD3DSurface m_DepthBufferOrigFSAA;

  float m_fCurLuminance;

  float m_fCurSceneScale;
  float m_fAdaptedSceneScale;
  SHistogram m_LumHistogram;
  SGamma m_LumGamma;
  bool m_bViewportDirty;
  int m_HDR_SeparateBlend;

  int m_nIndsDMesh;
  int m_nIOffsDMesh;

  int m_UnitFrustVBSize;
  int m_UnitFrustIBSize;
  int m_UnitSphereVBSize;
  int m_UnitSphereIBSize;


  ETEX_Format m_HDR_FloatFormat_Scalar;
  ETEX_Format m_HDR_FloatFormat;
  int m_MaxAnisotropyLevel;

  ushort *GetIBPtr(int nInds, int &nOffs)
  {
    if (!m_pIB)
      return NULL;
    HRESULT hr;
    ushort *pInds = NULL;
    if (nInds > m_nIndsDMesh)
    {
      assert(0);
      return NULL;
    }
    if (nInds+m_nIOffsDMesh > m_nIndsDMesh)
    {
#if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
      hr = m_pIB->Lock(0, nInds*sizeof(short), (void **) &pInds, D3DLOCK_DISCARD);
#else
      if (m_pIB == m_RP.m_pIndexStream)
      {
        m_RP.m_pIndexStream = NULL;
        m_pd3dDevice->SetIndices(NULL);
      }
      hr = m_pIB->Lock(0, 0, (void **) &pInds, D3DLOCK_DISCARD);
#endif
#elif defined (DIRECT3D10)
      hr = m_pIB->Map(D3D10_MAP_WRITE_DISCARD, 0, (void **) &pInds);
#endif
      nOffs = 0;
      m_nIOffsDMesh = nInds;
    }
    else
    {
#if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
      hr = m_pIB->Lock(m_nIOffsDMesh*sizeof(short), nInds*sizeof(short), (void **) &pInds, D3DLOCK_NOOVERWRITE);
#else
      if (m_pIB == m_RP.m_pIndexStream)
      {
        m_RP.m_pIndexStream = NULL;
        m_pd3dDevice->SetIndices(NULL);
      }
      hr = m_pIB->Lock(0, 0, (void **) &pInds, D3DLOCK_NOOVERWRITE);
      pInds += m_nIOffsDMesh;
#endif
#elif defined (DIRECT3D10)
      hr = m_pIB->Map(D3D10_MAP_WRITE_NO_OVERWRITE, 0, (void **) &pInds);
      pInds += m_nIOffsDMesh;
#endif
      nOffs = m_nIOffsDMesh;
      m_nIOffsDMesh += nInds;
    }
    return pInds;
  }
  void UnlockIB()
  {
#if defined (DIRECT3D9) || defined(OPENGL)
    HRESULT hr = m_pIB->Unlock();
#elif defined (DIRECT3D10)
    m_pIB->Unmap();
#endif
  }
  void DiscardIB()
  {
    int nInds = 1;
    ushort *pInds = NULL;
#if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
    HRESULT hr = m_pIB->Lock(0, nInds*sizeof(short), (void **)&pInds, D3DLOCK_DISCARD);
#else
    HRESULT hr = m_pIB->Lock(0, 0, (void **)&pInds, D3DLOCK_DISCARD);
#endif
    hr = m_pIB->Unlock();
#elif defined (DIRECT3D10)
    HRESULT hr = m_pIB->Map(D3D10_MAP_WRITE_DISCARD, 0, (void **) &pInds);
    pInds += nInds;
    m_pIB->Unmap();
#endif
    m_nIOffsDMesh = nInds;
  }

	int GetDynVBSize( int vertexType = VERTEX_FORMAT_P3F_COL4UB_TEX2F )
	{
		switch( vertexType )
		{
		case VERTEX_FORMAT_P3F_COL4UB_TEX2F:
			return m_nVertsDMesh[ POOL_P3F_COL4UB_TEX2F ];
		case VERTEX_FORMAT_P3F_TEX2F:
			return m_nVertsDMesh[ POOL_P3F_TEX2F ];
		case VERTEX_FORMAT_P3F:
			return m_nVertsDMesh[ POOL_P3F ];
		case VERTEX_FORMAT_TRP3F_COL4UB_TEX2F:
			return m_nVertsDMesh[ POOL_TRP3F_COL4UB_TEX2F ];
		case VERTEX_FORMAT_TRP3F_TEX2F_TEX3F:
			return m_nVertsDMesh[ POOL_TRP3F_TEX2F_TEX3F ];
		case VERTEX_FORMAT_P3F_TEX3F:
			return m_nVertsDMesh[ POOL_P3F_TEX3F ];
		case VERTEX_FORMAT_P3F_TEX2F_TEX3F:
			return m_nVertsDMesh[ POOL_P3F_TEX2F_TEX3F ];
		default:
			return 0;
		}
	}

  void *GetVBPtr(int nVerts, int &nOffs, int Pool=0)
  {
    HRESULT hr;
    void *pVertices = NULL;
    if (nVerts > m_nVertsDMesh[Pool])
    {
      assert(0);
      return NULL;
    }
    if (!m_pVBAr[Pool][0])
      return NULL;

    int nVertSize;

    switch (Pool)
    {
      case POOL_P3F_COL4UB_TEX2F:
      default:
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
    }

#if defined (DIRECT3D10)
    if (CV_d3d9_vbpools==2 || nVerts+m_nOffsDMesh[Pool] > m_nVertsDMesh[Pool])
#else
    if (nVerts+m_nOffsDMesh[Pool] > m_nVertsDMesh[Pool])
#endif
    {
      m_nCurBuf[Pool] = (m_nCurBuf[Pool]+1)&3;
      m_pVB[Pool] = m_pVBAr[Pool][m_nCurBuf[Pool]];
#if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
      hr = m_pVB[Pool]->Lock(0, nVerts*nVertSize, &pVertices, D3DLOCK_DISCARD);
#else
      hr = m_pVB[Pool]->Lock(0, 0, &pVertices, D3DLOCK_DISCARD);
#endif
#elif defined (DIRECT3D10)
      hr = m_pVB[Pool]->Map(D3D10_MAP_WRITE_DISCARD, 0, &pVertices);
#endif
      nOffs = 0;
      m_nOffsDMesh[Pool] = nVerts;
    }
    else
    {
#if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
      hr = m_pVB[Pool]->Lock(m_nOffsDMesh[Pool]*nVertSize, nVerts*nVertSize, &pVertices, D3DLOCK_NOOVERWRITE);
#else
      hr = m_pVB[Pool]->Lock(0, 0, &pVertices, D3DLOCK_NOOVERWRITE);
      byte *pD = (byte *)pVertices;
      pVertices = (void *)&pD[m_nOffsDMesh[Pool]*nVertSize];
#endif
#elif defined (DIRECT3D10)
      hr = m_pVB[Pool]->Map(D3D10_MAP_WRITE_NO_OVERWRITE, 0, &pVertices);
      byte *pD = (byte *)pVertices;
      pVertices = (void *)&pD[m_nOffsDMesh[Pool]*nVertSize];
#endif
      nOffs = m_nOffsDMesh[Pool];
      m_nOffsDMesh[Pool] += nVerts;
    }
    return pVertices;
  }
  void UnlockVB(int Pool=0)
  {
#if defined (DIRECT3D9) || defined(OPENGL)
    HRESULT hr = m_pVB[Pool]->Unlock();
#elif defined (DIRECT3D10)
    m_pVB[Pool]->Unmap();
#endif
  }
  void DiscardVB()
  {
    int Pool;
    for (Pool=0; Pool<POOL_MAX; Pool++)
    {
      m_nCurBuf[Pool] = (m_nCurBuf[Pool]+1)&3;
      m_pVB[Pool] = m_pVBAr[Pool][m_nCurBuf[Pool]];
      int nVertSize;

      switch (Pool)
      {
        case POOL_P3F_COL4UB_TEX2F:
        default:
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
      }
      int nVerts = 1;
      void *pVertices = NULL;
#if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
      HRESULT hr = m_pVB[Pool]->Lock(0, nVerts*nVertSize, &pVertices, D3DLOCK_DISCARD);
#else
      HRESULT hr = m_pVB[Pool]->Lock(0, 0, &pVertices, D3DLOCK_DISCARD);
#endif
      hr = m_pVB[Pool]->Unlock();
#elif defined (DIRECT3D10)
      HRESULT hr = m_pVB[Pool]->Map(D3D10_MAP_WRITE_DISCARD, 0, &pVertices);
      pVertices = &((byte *)pVertices)[nVerts*nVertSize];
      m_pVB[Pool]->Unmap();
#endif
      m_nOffsDMesh[Pool] = nVerts;
    }
  }


//============================================================================================

  TVertPool *sVertPools;
  TVertPool *sIndexPools;

  void ReleaseVideoPools();
  void AllocIBInPool(int nSize, SVertexStream *pIB);
  void AllocVBInPool(int nSize, int nVFormat, SVertexStream *pVB);
  bool AllocateChunk(int size, TVertPool *Ptr, SVertexStream *pVB, const char *szSource);
  bool ReleaseVBChunk(TVertPool *Ptr, SVertexStream *pVB);

  // Pixel formats from D3D.
  SPixFormat        m_FormatA8;          //8 bit alpha
  SPixFormat        m_FormatL8;          //8 bit luminance
  SPixFormat        m_FormatA8L8;        //16
  SPixFormat        m_FormatA8R8G8B8;    //32 bit
  SPixFormat        m_FormatX8R8G8B8;    //32 bit
  SPixFormat        m_FormatR8G8B8;      //24 bit
  SPixFormat        m_FormatA4R4G4B4;     //16 bit
  SPixFormat        m_FormatR5G6B5;       //16 bit

  SPixFormat        m_FormatA16B16G16R16F;    //64 bit
  SPixFormat        m_FormatA16B16G16R16;     //64 bit
  SPixFormat        m_FormatG16R16F;          //32 bit
  SPixFormat        m_FormatG16R16;           //32 bit
  SPixFormat        m_FormatR16F;            //16 bit
  SPixFormat        m_FormatR32F;            //32 bit
  SPixFormat        m_FormatA32B32G32R32F;    //128 bit

  SPixFormat        m_Format3Dc;     //
  SPixFormat        m_FormatDXT1;    //Compressed RGB
  SPixFormat        m_FormatDXT3;    //Compressed RGBA
  SPixFormat        m_FormatDXT5;    //Compressed RGBA

	SPixFormat        m_FormatDF16;		//ATI 16-bit native depth buffer
	SPixFormat        m_FormatDF24;		//ATI 24-bit native depth buffer

  SPixFormat        m_FormatD32F;   //DX10 depth format
  SPixFormat        m_FormatD16;		//Nvidia 16-bit native depth buffer
  SPixFormat        m_FormatD24S8;	//Nvidia 24-bit native depth buffer

	SPixFormat        m_FormatNULL;		//Nvidia's NULL texture
	
  SPixFormat        m_FormatDepth24; //Depth texture
  SPixFormat        m_FormatDepth16; //Depth texture

  SPixFormat*       m_FirstPixelFormat;

  byte m_GammmaTable[256];

	int	m_fontBlendMode;

public:
#if defined (DIRECT3D9) || defined (OPENGL)
  LPDIRECT3DDEVICE9  GetDevice()
  {
    return m_pd3dDevice;
  }
  void               SetD3D(LPDIRECT3D9 pD3D) { m_pD3D = pD3D; }
  LPDIRECT3DDEVICE9  GetD3DDevice() { return m_pd3dDevice; }
  LPDIRECT3D9        GetD3D() { return m_pD3D; }
  LPDIRECT3DSURFACE9 GetZSurface() { return m_pZBuffer; }
  LPDIRECT3DSURFACE9 GetBackSurface() { return m_pBackBuffer; }
  const D3DCAPS9     *GetD3DCaps()  { return m_pd3dCaps; }
#elif defined (DIRECT3D10)
  ID3D10Device*  GetDevice()
  {
    return m_pd3dDevice;
  }
  ID3D10Device*  GetD3DDevice() { return m_pd3dDevice; }
#endif

  void SetDefaultTexParams(bool bUseMips, bool bRepeat, bool bLoad);

public:

#if defined (DIRECT3D10)
  void PrepareDepthMap(CDLight* pLightSource);
#endif
  void SetupShadowGenPass(int Num, ShadowMapFrustum * pFr);

  void BlurShadow(ShadowMapFrustum *pFr);
	void FX_DeferredShadowPassAO( CCryName & TechName, SSectorTextureSet * pParams, SFillLight * pLight, Matrix44 * pMatComposite, SDynTexture ** arr_SSAO_Tex = NULL );
  void SetDepthBoundTest(float fMin, float fMax, bool bEnable);
  void CreateDeferredUnitBox(t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff);
  void PrepareDepthMap(ShadowMapFrustum* SMSource, CDLight* pLight=NULL);
  void ConfigShadowTexgen(int Num, ShadowMapFrustum * pFr, int nResView=-1, int nFrustNum = -1, CDLight* pLight = NULL);
  virtual void SetupShadowOnlyPass(int Num, ShadowMapFrustum* pShadowInst, Matrix34 * pObjMat);
	void DrawAllShadowsOnTheScreen();
  void DrawAllDynTextures(const char *szFilter, const bool bLogNames, const bool bOnlyIfUsedThisFrame);
	void OnEntityDeleted(IRenderNode * pRenderNode);

  //=============================================================

  byte m_eCurColorOp[MAX_TMU];
  byte m_eCurAlphaOp[MAX_TMU];
  byte m_eCurColorArg[MAX_TMU];
  byte m_eCurAlphaArg[MAX_TMU];
  int msCurState;

  void D3DSetCull(ECull eCull);
  const char *D3DError( HRESULT h );
  bool Error(char *Msg, HRESULT h);

  void SetDeviceGamma(ushort *r, ushort *g, ushort *b);
  HRESULT AdjustWindowForChange();

private:

  void RegisterVariables();
  void UnRegisterVariables();

  bool SetWindow(int width, int height, bool fullscreen, WIN_HWND hWnd);
  bool SetRes();
  void UnSetRes();
	void DisplaySplash(); //!< Load a bitmap from a file, blit it to the windowdc and free it

  void RecognizePixelFormat(SPixFormat& Dest, uint FormatFromD3D, int InBitsPerPixel, const char* InDesc, bool bAutoGenMips);

  void DestroyWindow(void);
  virtual void RestoreGamma(void);
  void SetGamma(float fGamma, float fBrigtness, float fContrast, bool bForce);
  virtual char*	GetVertexProfile(bool bSupportedProfile);
  virtual char*	GetPixelProfile(bool bSupportedProfile);

  struct texture_info
  {
    texture_info() { ZeroStruct(*this); }
    char filename[256];
    int  bind_id;
    int  low_tid;
    int  type;
    int  last_time_used;
  };
  PodArray<texture_info> m_texture_registry;
  int FindTextureInRegistry(const char * filename, int * tex_type);
  int RegisterTextureInRegistry(const char * filename, int tex_type, int tid, int low_tid);
  unsigned int MakeTextureREAL(const char * filename,int *tex_type, unsigned int load_low_res);
  unsigned int CheckTexturePlus(const char * filename, const char * postfix);

#if defined XENON
  static HRESULT CALLBACK OnD3D9CreateDevice(LPDIRECT3DDEVICE9 pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
  static HRESULT CALLBACK OnD3D9ResetDevice(LPDIRECT3DDEVICE9 pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
  static bool    CALLBACK IsD3D9DeviceAcceptable(D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext);
  static void    CALLBACK OnD3D9FrameRender(LPDIRECT3DDEVICE9 pd3dDevice, double fTime, float fElapsedTime, void* pUserContext);
  static void    CALLBACK OnD3D9LostDevice(void* pUserContext );
  static void    CALLBACK OnD3D9DestroyDevice(void* pUserContext );
  static HRESULT CALLBACK OnD3D9PostCreateDevice(D3DDevice* pd3dDevice);
#elif defined (DIRECT3D9) || defined(OPENGL)
  static bool    CALLBACK OnD3D9DeviceChanging(DXUTDeviceSettings* pDeviceSettings, void* pUserContext);
  static HRESULT CALLBACK OnD3D9CreateDevice(LPDIRECT3DDEVICE9 pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
  static HRESULT CALLBACK OnD3D9ResetDevice(LPDIRECT3DDEVICE9 pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
  static bool    CALLBACK IsD3D9DeviceAcceptable(D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext);
  static void    CALLBACK OnD3D9FrameRender(LPDIRECT3DDEVICE9 pd3dDevice, double fTime, float fElapsedTime, void* pUserContext);
  static void    CALLBACK OnD3D9LostDevice(void* pUserContext );
  static void    CALLBACK OnD3D9DestroyDevice(void* pUserContext );
  static HRESULT CALLBACK OnD3D9PostCreateDevice(D3DDevice* pd3dDevice);
#elif defined (DIRECT3D10)
  static bool    CALLBACK IsD3D10DeviceAcceptable(UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext);
  static HRESULT CALLBACK OnD3D10CreateDevice(ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
  static HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
  static void    CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext);
  static void    CALLBACK OnD3D10DestroyDevice(void* pUserContext);
  static void    CALLBACK OnD3D10FrameRender(ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext);
  static bool    CALLBACK OnD3D10DeviceChanging(DXUTDeviceSettings* pDeviceSettings, void* pUserContext);
#endif

#if defined (DIRECT3D9) && !defined (XENON)
	void SuccessfullyLaunchFromMediaCenter() const;
#endif

public:  
  static HRESULT CALLBACK OnD3D10PostCreateDevice(D3DDevice* pd3dDevice);

  CMatrixStack* m_matView;  
  CMatrixStack* m_matProj;  
  D3DXMATRIX m_matPsmWarp;
  D3DXMATRIX m_matViewInv;
  int m_MatDepth;
  
  static int CV_d3d9_debugruntime;

  static int CV_d3d9_nvperfhud;
  static int CV_d3d9_vbpools;
  static int CV_d3d9_vbpoolsize;
  static int CV_d3d9_ibpools;
  static int CV_d3d9_ibpoolsize;
  static int CV_d3d9_forcesoftware;
  static ICVar *CV_d3d9_texturefilter;
  static int CV_d3d9_allowsoftware;
  static float CV_d3d9_pip_buff_size;
  static int CV_d3d9_rb_verts;
  static int CV_d3d9_rb_tris;
	static int CV_d3d9_null_ref_device;
#ifdef XENON
  static int CV_d3d9_predicatedtiling;
#endif
  static int CV_d3d9_clipplanes;
  static int CV_d3d9_triplebuffering;
  static int CV_d3d9_resetdeviceafterloading;
  
#if defined (DIRECT3D10)
	static int CV_d3d10_CBUpdateStats;
	static int CV_d3d10_CBUpdateMethod;
	static int CV_d3d10_NumStagingBuffers;
#endif

//============================================================
// Renderer interface

  bool m_bInitialized;
	string m_Description;
  bool m_bFullScreen;

  TArray<SD3DContext *> m_RContexts;
  SD3DContext *m_CurrContext;
  TArray<CTexture*> m_RTargets;
  int m_nRecurs;

public:
#ifndef PS2	
  virtual WIN_HWND Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen,WIN_HINSTANCE hinst, WIN_HWND Glhwnd=0, bool bReInit=false, const SCustomRenderInitArgs* pCustomArgs=0);
#else //PS2
  virtual bool Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen, bool bReInit=false);
#endif  //endif	

	virtual void GetVideoMemoryUsageStats( size_t& vidMemUsedThisFrame, size_t& vidMemUsedRecently );

  virtual bool SetCurrentContext(WIN_HWND hWnd);
  virtual bool CreateContext(WIN_HWND hWnd, bool bAllowFSAA=false);
  virtual bool DeleteContext(WIN_HWND hWnd);

  virtual int  CreateRenderTarget (int nWidth, int nHeight, ETEX_Format eTF=eTF_A8R8G8B8);
  virtual bool DestroyRenderTarget (int nHandle);
  virtual bool SetRenderTarget (int nHandle);

  virtual void  ShareResources( IRenderer *renderer );
  virtual void	MakeCurrent();

	virtual bool ChangeDisplay(unsigned int width,unsigned int height,unsigned int cbpp);
  virtual void ChangeViewport(unsigned int x,unsigned int y,unsigned int width,unsigned int height);
	virtual int	 EnumDisplayFormats(SDispFormat *formats);
  //! Return all supported by video card video AA formats
  virtual int	EnumAAFormats(const SDispFormat &rDispFmt, SAAFormat *formats);

  //! Changes resolution of the window/device (doen't require to reload the level
  virtual bool	ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen);
  virtual void	Reset(void);

  virtual void BeginFrame();
  virtual void ShutDown(bool bReInit=false);
  virtual void ShutDownFast();
  virtual void RenderDebug();  
  
  void DebugPrintShader(class CHWShader_D3D *pSH, void *pInst, int nX, int nY, ColorF colSH);
  void DebugDrawStats1();
  void DebugDrawStats2();
  void DebugDrawStats();
  void DebugDrawRect(float x1,float y1,float x2,float y2,float *fColor);

	virtual void EndFrame();  
	virtual void GetMemoryUsage(ICrySizer* Sizer);
	void GetLogVBuffers();
  virtual void Draw2dImage(float xpos,float ypos,float w,float h,int textureid,float s0=0,float t0=0,float s1=1,float t1=1,float angle=0,float r=1,float g=1,float b=1,float a=1,float z=1);
	virtual void DrawImage(float xpos,float ypos,float w,float h,int texture_id,float s0,float t0,float s1,float t1,float r,float g,float b,float a);
  virtual void DrawImageWithUV(float xpos,float ypos,float z,float w,float h,int texture_id,float s[4],float t[4],float r,float g,float b,float a);
	virtual void SetCullMode(int mode=R_CULL_BACK);
    
  virtual bool EnableFog(bool enable);
  virtual void SetFog(float density, float fogstart, float fogend, const float *color, int fogmode);

  virtual void SelectTMU(int tnum);
  virtual void EnableTMU(bool enable);

  virtual unsigned int DownLoadToVideoMemory(unsigned char *data,int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat=true, int filter=FILTER_BILINEAR, int Id=0, char *szCacheName=NULL, int flags=0);
  virtual	void UpdateTextureInVideoMemory(uint tnum, unsigned char *newdata,int posx,int posy,int w,int h,ETEX_Format eTF=eTF_X8R8G8B8);
  virtual void RemoveTexture(unsigned int TextureId);

  void SetRCamera(const CRenderCamera &cam);
  virtual void SetCamera(const CCamera &cam);
  virtual	void SetViewport(int x, int y, int width, int height);
  virtual void GetViewport(int *x, int *y, int *width, int *height);
  virtual	void SetScissor(int x=0, int y=0, int width=0, int height=0);
	virtual	void SetRenderTile(f32 nTilesPosX,f32 nTilesPosY,f32 nTilesGridSizeX,f32 nTilesGridSizeY);

  virtual void Draw3dBBox(const Vec3 &mins,const Vec3 &maxs, int nPrimType);
	virtual void Draw3dPrim(const Vec3 &mins,const Vec3 &maxs, int nPrimType, const float* pColor = NULL);
  virtual void Flush3dBBox(const Vec3 &mins,const Vec3 &maxs,const bool bSolid);

  void DrawLines(Vec3 v[], int nump, ColorF& col, int flags, float fGround);
  void DrawLineColor(const Vec3 & vPos1, const ColorF & vColor1, const Vec3 & vPos2, const ColorF & vColor2);
  virtual void DrawLine(const Vec3 & vPos1, const Vec3 & vPos2);
  virtual void Graph(byte *g, int x, int y, int wdt, int hgt, int nC, int type, char *text, ColorF& color, float fScale);

  virtual void *GetDynVBPtr(int nVerts, int &nOffs, int Pool);
  virtual void DrawDynVB(int nOffs, int Pool, int nVerts);
  virtual void DrawDynVB(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pBuf, ushort *pInds, int nVerts, int nInds, int nPrimType);
	virtual CVertexBuffer	*CreateBuffer(int  buffersize,int vertexformat, const char *szSource, bool bDynamic=false);
  virtual void CreateBuffer(int size, int vertexformat, CVertexBuffer *buf, int Type, const char *szSource, bool bDynamic=false);
	virtual void	DrawBuffer(CVertexBuffer *src,SVertexStream *indicies,int numindices, int offsindex, int prmode,int vert_start=0,int vert_stop=0, CRenderChunk *pChunk=NULL);
  virtual void UnlockBuffer(CVertexBuffer *dest, int Type, int nVerts);
  virtual void UpdateBuffer(CVertexBuffer *dest,const void *src,int vertexcount, bool bUnLock, int offs=0, int Type=0);
  virtual void  CreateIndexBuffer(SVertexStream *dest,const void *src,int indexcount, int oldIndexCount);
  virtual void  UpdateIndexBuffer(SVertexStream *dest,const void *src,int indexcount, int oldIndexCount, bool bUnLock=true, bool bDynamic=false);
  virtual void  ReleaseIndexBuffer(SVertexStream *dest, int nIndices);
  virtual void ReleaseBuffer(CVertexBuffer *bufptr, int nVerts);
  virtual void CheckError(const char *comment);
  virtual int SetPolygonMode(int mode);

  virtual void PushMatrix();
  virtual void PopMatrix();

  virtual void EnableVSync(bool enable);
  virtual void DrawTriStrip(CVertexBuffer *src, int vert_num);
  virtual void ResetToDefault();

  virtual void SetMaterialColor(float r, float g, float b, float a);
  virtual char * GetStatusText(ERendStats type);

  virtual void ProjectToScreen(float ptx, float pty, float ptz,float *sx, float *sy, float *sz );
  virtual int UnProject(float sx, float sy, float sz, float *px, float *py, float *pz, const float modelMatrix[16], const float projMatrix[16], const int    viewport[4]);
  virtual int UnProjectFromScreen( float  sx, float  sy, float  sz, float *px, float *py, float *pz);

  virtual void SetClipPlane( int id, float * params ){ EF_SetClipPlane(params ? true : false, params, false); }

  virtual void GetModelViewMatrix(float * mat);
  virtual void GetProjectionMatrix(float * mat);

  void MakeSprites(TArray<SSpriteGenInfo>& SGI);
  //void MakeSprite(IDynTexture * &rTexturePtr, float _fSpriteDistance, int nTexSize, float angle, float angle2, IStatObj * pStatObj, const float fBrightnessMultiplier, 	SRendParams& rParms);
  virtual void DrawObjSprites (PodArray<struct SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor);
  virtual void GenerateObjSprites(PodArray<struct SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor);
  void GenerateSpritesList (PodArray<struct SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor);
  void DrawObjSprites_NoBend_Merge_Technique (TArray<SSpriteInfo>& sSPInfo, bool bZ, bool bShadows);
  void ObjSpritesFlush (TArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F>& Verts, TArray<ushort>& Inds, void *&pCurVB, SShaderTechnique *pTech);
  void DrawSpritesShadows(bool& bBlur, bool& bSetRT, CTexture*& tpSrc);

  virtual void DrawQuad(const Vec3 &right, const Vec3 &up, const Vec3 &origin,int nFlipMode=0);
  virtual void DrawQuad(float dy,float dx, float dz, float x, float y, float z);
  void    DrawQuad(float x0, float y0, float x1, float y1, const ColorF & color, float z = 1.0f, float s0=0, float t0=0, float s1=1, float t1=1);
  void DrawQuad3D(const Vec3 & v0, const Vec3 & v1, const Vec3 & v2, const Vec3 & v3, const ColorF & color, 
    float ftx0 = 0,  float fty0 = 0,  float ftx1 = 1,  float fty1 = 1);
  void DrawFullScreenQuad(CShader *pSH, const CCryName& TechName, float s0, float t0, float s1, float t1, uint nState = GS_NODEPTHTEST);

 //fog	
  void SetFogColor(float * color);

  virtual void SetPerspective(const CCamera &cam);

  virtual void ClearBuffer(uint nFlags, ColorF *vColor, float depth = 1.0f);
  virtual void ReadFrameBuffer(unsigned char * pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX=-1, int nScaledY=-1);  
	virtual void ReadFrameBufferFast(unsigned int* pDstARGBA8, int dstWidth, int dstHeight);

  //misc	
  virtual bool ScreenShot(const char *filename=NULL, int width=0);  

	virtual uint RenderOccludersIntoBuffer(const CCamera & viewCam, int nTexSize, PodArray<IRenderNode*> & lstOccluders, float * pBuffer);

  virtual void UnloadOldTextures(){};

  virtual void Set2DMode(bool enable, int ortox, int ortoy,float znear=-1e10f,float zfar=1e10f);

  virtual int ScreenToTexture();

  virtual void EnableAALines(bool bEnable);

	virtual	bool	SetGammaDelta(const float fGamma);

  //////////////////////////////////////////////////////////////////////
  // Replacement functions for the Font engine
  virtual	bool FontUploadTexture(class CFBitmap*, ETEX_Format eTF=eTF_A8R8G8B8);
	virtual	int  FontCreateTexture(int Width, int Height, byte *pData, ETEX_Format eTF=eTF_A8R8G8B8, bool genMips=false);
  virtual	bool FontUpdateTexture(int nTexId, int X, int Y, int USize, int VSize, byte *pData);
  virtual	void FontReleaseTexture(class CFBitmap *pBmp);
  virtual void FontSetTexture(class CFBitmap*, int nFilterMode);
  virtual void FontSetTexture(int nTexId, int nFilterMode);
  virtual void FontSetRenderingState(unsigned int nVirtualScreenWidth, unsigned int nVirtualScreenHeight);
  virtual void FontSetBlending(int src, int dst);
  virtual void FontRestoreRenderingState();
  
  void FontSetState(bool bRestore);

	//////////////////////////////////////////////////////////////////////

#ifndef EXCLUDE_SCALEFORM_SDK
	struct SSF_ResourcesD3D& SF_GetResources();
	void SF_ResetResources();
	bool SF_SetVertexDeclaration(SSF_GlobalDrawParams::EVertexFmt vertexFmt);
	CShader* SF_SetTechnique(const CCryName& techName);
	void SF_SetBlendOp(SSF_GlobalDrawParams::EAlphaBlendOp blendOp, bool reset = false);
	uint32 SF_AdjustBlendStateForMeasureOverdraw(uint32 blendModeStates);

	virtual void SF_DrawIndexedTriList(int baseVertexIndex, int minVertexIndex, int numVertices, int startIndex, int triangleCount, const SSF_GlobalDrawParams& params);
	virtual void SF_DrawLineStrip(int baseVertexIndex, int lineCount, const SSF_GlobalDrawParams& params);
	virtual void SF_DrawGlyphClear(const SSF_GlobalDrawParams& params);
	virtual void SF_Flush();
	virtual bool SF_UpdateTexture(int texId, int mipLevel, int numRects, const SUpdateRect* pRects, unsigned char* pData, size_t pitch, ETEX_Format eTF);
#endif // #ifndef EXCLUDE_SCALEFORM_SDK

  //////////////////////////////////////////////////////////////////////

  void	CheckFSAAChange(void);

  // FX Shaders pipeline
  void FX_DrawInstances(CShader *ef, SShaderPass *slw, uint nCurInst, uint nLastInst, uint nUsedAttr, int nInstAttrMask, byte Attributes[], bool bConstBasedInstancing);
  void FX_DrawShader_InstancedHW(CShader *ef, SShaderPass *slw);

  void FX_DrawShader_General(CShader *ef, SShaderTechnique *pTech, bool bUseZState, bool bUseMaterialState);
  void FX_DrawShader_Terrain(CShader *ef, SShaderTechnique *pTech);  
  void FX_DrawMultiLightPasses(CShader *ef, SShaderTechnique *pTech, int nShadowChans);
  void FX_DrawShadowPasses(CShader *ef, SShaderTechnique *pTech, int nChanNum);
  bool FX_DrawToRenderTarget(CShader *pShader, SRenderShaderResources* pRes, CRenderObject *pObj, SShaderTechnique *pTech, SHRenderTarget *pTarg, int nPreprType, CRendElement *pRE);
  void FX_ScreenStretchRect( CTexture *pDst );

  void FX_ResolveDepthTarget(CTexture* pSrcZTarget, SD3DSurface* pDstDepthBuffer);
  void FX_StencilLights(SLightPass *pLP);
  void FX_SetLightsScissor(SLightPass *pLP, const RectI* pDef);
  int  FX_SetupLightPasses(SShaderTechnique *pTech);
	bool FX_ProcessAOTarget();
  void FX_ProcessShadows(int nGroup, int nAW);
  bool FX_ProcessShadowsListsForLightGroup(int nGroup, int nOffsRI);
	void FX_PrepareAllDepthMaps();
  void FX_StencilCullPass(int nStencilID, int nVertOffs, int nNumVers, int nIndOffs, int nNumInds);
  void FX_StencilFrustumCull(int nStencilID, CDLight* pLight, ShadowMapFrustum* pFrustum, int nAxis,  CShader *pShader);
  void FX_StencilCull(int nStencilID, t_arrDeferredMeshIndBuff& arrDeferredInds, t_arrDeferredMeshVertBuff& arrDeferredVerts, CShader *pShader);
  void FX_StencilRefresh(int StencilFunc, uint nStencRef, uint nStencMask);
  void FX_StencilCullPassForLightGroup(int nLightGroup);
  bool CreateUnitFrustumMesh();
  bool CreateUnitLightMeshes();
  void FX_CreateDeferredQuad(int& nOffs, CDLight* pLight, float maskRTWidth, float maskRTHeight, Matrix44* pmCurView, Matrix44* pmCurComposite);
  void FX_DeferredShadowPass(CDLight* pLight, int nLightInGroup, ShadowMapFrustum *pShadowFrustum, CShader *pShader, float fFinalRange, int nVertexOffset, bool bShadowPass, bool bStencilPrepass, int nLod, int nFrustNum = -1);
  bool FX_DeferredProjLights(int nGroup, bool bCheckValidMask);
	bool FX_DeferredShadowPassSetup(const Matrix44& mShadowTexGen, float maskRTWidth, float maskRTHeight);
	bool FX_DeferredShadows( int nGroup, SRendLightGroup* pGr, int maskRTWidth, int maskRTHeight );
	void FX_SetDeferredShadows();
  bool FX_ProcessLightsListForLightGroup(int nGroup, SRendLightGroup *pGr, int nOffsRI);
  void FX_ShadowBlur(float fShadowBluriness, SDynTexture *tpSrc, CTexture *tpDst, int iShadowMode = -1);
  bool FX_SetShadowMaskRT(int nGroup, int nBlurMode, CTexture *&tpSrc, SDynTexture *&pTempBlur);

  void FX_DrawDetailOverlayPasses();  
  void FX_DrawCausticsPasses();

  void FX_SetupMultiLayers( bool bEnable = false );  
  void FX_DrawMultiLayers( );  
  
  void FX_DrawTechnique(CShader *ef, SShaderTechnique *pTech, bool bGeneral, bool bUseMaterialState);

  bool FX_PrepareLightInfoTexture(bool bEnable);
  bool FX_HDRScene(bool bEnable);
  bool FX_ZScene(bool bEnable, bool bUseHDR, bool bClearZBuffer);  
  bool FX_FogScene();
  bool FX_GlowScene(bool bEnable);  
  bool FX_ScatterScene(bool bEnable, bool ScatterPass);
  bool FX_DisplayFogScene();  
  bool FX_MotionBlurScene(bool bEnable);  
  bool FX_CustomRenderScene(bool bEnable);  
  void FX_PostProcessSceneHDR();
  bool FX_PostProcessScene(bool bEnable);

	void Hint_DontSync( CTexture &rTex );

  // Shaders pipeline
  //=======================================================================
  virtual void EF_PipelineShutdown();
  void EF_ClearBuffers(uint nFlags, const ColorF *Colors, float fDepth=1.0f);

  void EF_Invalidate();
  void EF_Restore();
  virtual void EF_SetState(int st, int AlphaRef=-1);

  void ChangeLog();

	void SetLogFuncs(bool bEnable);

  bool CreateMSAADepthBuffer();
  void FlushHardware(bool bIssueBeforeSync);
  void PostMeasureOverdraw();
  virtual bool CheckDeviceLost();

  short m_nPrepareShadowFrame;

  _inline void EF_DirtyMatrix()
  {
    m_RP.m_PersFlags |= RBPF_FP_MATRIXDIRTY | RBPF_FP_DIRTY;
    m_RP.m_FrameObject++;
  }
  _inline void EF_PushMatrix()
  {
    m_matView->Push();
    m_MatDepth++;
  }
  _inline void EF_PopMatrix()
  {
    m_matView->Pop();
    m_MatDepth--;
    EF_DirtyMatrix();
  }
  _inline void EF_ResetPipe()
  {
    int i;
    EF_SetState(GS_NODEPTHTEST);
    D3DSetCull(eCULL_None);
    CTexture::BindNULLFrom(1);
    EF_SelectTMU(0);
    m_RP.m_FlagsStreams_Decl = 0;
    m_RP.m_FlagsStreams_Stream = 0;
    m_RP.m_FlagsPerFlush = 0;
    m_RP.m_FlagsShader_RT = 0;
    m_RP.m_FlagsShader_MD = 0;
    m_RP.m_FlagsShader_MDV = 0;
    m_RP.m_FlagsShader_LT = 0;
    HRESULT h = FX_SetIStream(NULL);
    EF_Scissor(false, 0, 0, 0, 0);
    m_RP.m_pShader = NULL;
    m_RP.m_PrevLMask = -1;
    for (i=1; i<VSF_NUM; i++)
    {
      if (m_RP.m_PersFlags & (RBPF_USESTREAM<<i))
      {
        m_RP.m_PersFlags &= ~(RBPF_USESTREAM<<i);
        h = FX_SetVStream(i, NULL, 0, 0, m_RP.m_ReqStreamFrequence[i]);
      }
    }
  }

  inline void EF_EnableATOC()
  {
    if (CV_r_atoc && m_bDeviceSupportsATOC)
    {
      if (!(m_RP.m_PersFlags2 & RBPF2_ATOC))
      {
        m_RP.m_PersFlags2 |= RBPF2_ATOC;
      #if defined (DIRECT3D9) || defined(OPENGL)
#if  !defined (XENON)
        m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y, CV_r_atoc == 1 ? MAKEFOURCC('A', 'T', 'O', 'C') : MAKEFOURCC('S', 'S', 'A', 'A')); 
#endif
      #endif
      }
    }
  }
  inline void EF_DisableATOC()
  {
    if (m_bDeviceSupportsATOC)
    {
      if (m_RP.m_PersFlags2 & RBPF2_ATOC)
      {
        m_RP.m_PersFlags2 &= ~RBPF2_ATOC;
      #if defined (DIRECT3D9) || defined(OPENGL)
#if  !defined (XENON)
        m_pd3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y, 0); 
#endif
      #endif
      }
    }
  }

  _inline void EF_SetGlobalColor(float r, float g, float b, float a)
  {
    EF_SelectTMU(0);
    EF_SetColorOp(255, 255, eCA_Texture | (eCA_Constant<<3), eCA_Texture | (eCA_Constant<<3));
    if (m_RP.m_CurGlobalColor[0] != r || m_RP.m_CurGlobalColor[1] != g || m_RP.m_CurGlobalColor[2] != b || m_RP.m_CurGlobalColor[3] != a)
    {
      m_RP.m_CurGlobalColor[0] = r;
      m_RP.m_CurGlobalColor[1] = g;
      m_RP.m_CurGlobalColor[2] = b;
      m_RP.m_CurGlobalColor[3] = a;
      m_RP.m_PersFlags |= RBPF_FP_DIRTY;
    }
  }
  _inline void EF_SetVertColor()
  {
    EF_SelectTMU(0);
    EF_SetColorOp(255, 255, eCA_Texture | (eCA_Diffuse<<3), eCA_Texture | (eCA_Diffuse<<3));
  }
  _inline void EF_DisableTextureAndColor()
  {
    if ((m_eCurColorArg[CTexture::m_CurStage]&7) == eCA_Texture || (m_eCurColorArg[CTexture::m_CurStage]&7) == eCA_Diffuse)
    {
      m_eCurColorArg[CTexture::m_CurStage] = (m_eCurColorArg[CTexture::m_CurStage] & ~7) | eCA_Constant;
      m_RP.m_PersFlags |= RBPF_FP_DIRTY;
    }
    if (((m_eCurColorArg[CTexture::m_CurStage]>>3)&7) == eCA_Texture || ((m_eCurColorArg[CTexture::m_CurStage]>>3)&7) == eCA_Diffuse)
    {
      m_eCurColorArg[CTexture::m_CurStage] = (m_eCurColorArg[CTexture::m_CurStage] & ~0x38) | (eCA_Constant<<3);
      m_RP.m_PersFlags |= RBPF_FP_DIRTY;
    }
    if ((m_eCurAlphaArg[CTexture::m_CurStage]&7) == eCA_Texture || (m_eCurAlphaArg[CTexture::m_CurStage]&7) == eCA_Diffuse)
    {
      m_eCurAlphaArg[CTexture::m_CurStage] = (m_eCurAlphaArg[CTexture::m_CurStage] & ~7) | eCA_Constant;
      m_RP.m_PersFlags |= RBPF_FP_DIRTY;
    }
    if (((m_eCurAlphaArg[CTexture::m_CurStage]>>3)&7) == eCA_Texture || ((m_eCurAlphaArg[CTexture::m_CurStage]>>3)&7) == eCA_Diffuse)
    {
      m_eCurAlphaArg[CTexture::m_CurStage] = (m_eCurAlphaArg[CTexture::m_CurStage] & ~0x38) | (eCA_Constant<<3);
      m_RP.m_PersFlags |= RBPF_FP_DIRTY;
    }
  }

  _inline void EF_SelectTMU(int Stage)
  {
    CTexture::m_CurStage = Stage;
  }

  //================================================================================

  int m_nCurVPStackLevel;
  D3DViewPort m_VPStack[8];
  _inline void EF_PushVP()
  {
    int nLevel = m_nCurVPStackLevel;
    if (nLevel >= 8)
      return;
    memcpy(&m_VPStack[nLevel], &m_NewViewport, sizeof(D3DViewPort));
    m_nCurVPStackLevel++; 
  }
  _inline void EF_PopVP()
  {
    int nLevel = m_nCurVPStackLevel;
    if (nLevel <= 0)
      return;
    nLevel--;
    if (m_NewViewport != m_VPStack[nLevel])
    {
      memcpy(&m_NewViewport, &m_VPStack[nLevel], sizeof(D3DViewPort));
      m_bViewportDirty = true;
    }
    m_nCurVPStackLevel--;
  }

  //================================================================================

  HRESULT EF_SetVertexDeclaration(int StreamMask, int nVFormat);
  inline bool FX_SetStreamFlags(SShaderPass *pPass)
  {
    bool bSkinned = false;
    //m_RP.m_FlagsStreams |= pPass->m_Flags & (VSM_MASK & ~VSM_HWSKIN & ~VSM_HWSKIN_SHAPEDEFORM & ~VSM_HWSKIN_MORPHTARGET);
#if !defined(PS3) || defined(PS3_ACTIVATE_CONSTANT_ARRAYS)//we cannot map it yet, will be removed soon
    if (CV_r_usehwskinning && m_RP.m_pRE && m_RP.m_pRE->mfIsHWSkinned())
    {
      m_RP.m_FlagsStreams_Decl |= VSM_HWSKIN;
      m_RP.m_FlagsStreams_Stream |= VSM_HWSKIN;
      bSkinned = true;
    }
#endif
    return bSkinned;
  }
  HRESULT FX_SetVStream(int nID, void *pB, uint nOffs, uint nStride, uint nFreq=1);
  HRESULT FX_SetIStream(void* pB);

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
	void FX_TagVStreamAsDirty(int nID);
	void FX_TagIStreamAsDirty();
	void FX_TagVertexDeclarationAsDirty();
#endif

#ifdef XENON
  void BeginPredicatedTiling(D3DVECTOR4 *pColor)
  {
    assert(!(m_RP.m_PersFlags2 & RBPF2_IN_PREDICATED_TILING));
    CONST TILING_SCENARIO& CurrentScenario = m_pTilingScenarios[m_dwTilingScenarioIndex];
    D3DVECTOR4 ClearColor = { 0, 0, 0, 0 };
    if (!pColor)
      pColor = &ClearColor;
    m_RP.m_PersFlags2 |= RBPF2_IN_PREDICATED_TILING;
    // Begin tiling.  We specify the tile count, the tiling rectangles, and the
    // color and Z/stencil values to clear the tiling rendertarget with for each tile.
    m_pd3dDevice->BeginTiling(CurrentScenario.dwTilingFlags, CurrentScenario.dwTileCount, CurrentScenario.TilingRects, pColor, 1.0f, 0L);
  }
  void EndPredicatedTiling(D3DTexture *pTex)
  {
    assert(m_RP.m_PersFlags2 & RBPF2_IN_PREDICATED_TILING);
    // End tiling.  This will cause the accumulated command buffer to be replayed
    // for each tile, predicated upon the boundaries of each tile.  At the end of
    // each tile's rendering, it will be resolved into a section of the front buffer
    // texture.
    m_RP.m_PersFlags2 &= ~RBPF2_IN_PREDICATED_TILING;
    D3DVECTOR4 ClearColor = { 0, 0, 0, 0 };
    m_pd3dDevice->EndTiling(D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS, NULL, pTex, &ClearColor, 1.0f, 0L, NULL);
  }
#endif

  void FX_DrawBatches(CShader *pSh, SShaderPass *pPass, CHWShader *curVS, CHWShader *curPS);

  //========================================================================================

  void FX_SetActiveRenderTargets(bool bAllowDIP=false);
  void FX_ClearRegion();

#define MAX_RT_STACK 8
  struct SRTStack
  {
    D3DSurface* m_pTarget;
    D3DDepthSurface* m_pDepth;

    CTexture *m_pTex;
    SD3DSurface *m_pSurfDepth;
    int m_Width;
    int m_Height;
    bool m_bNeedReleaseRT;
    bool m_bWasSetRT;
    bool m_bWasSetD;
    bool m_bClearOnResolve;
    bool m_bDontDraw;

    uint m_ClearFlags;
    ColorF m_ReqColor;
    float m_fReqDepth;
  };
  int m_nRTStackLevel[4];
  SRTStack m_RTStack[4][MAX_RT_STACK];

  int m_nMaxRT2Commit;
  SRTStack* m_pNewTarget[4];
  CTexture* m_pCurTarget[4];

  TArray<SD3DSurface *> m_TempDepths;

  void FX_GetRTDimensions(bool bRTPredicated, int& nWidth, int& nHeight);
  bool FX_GetTargetSurfaces(CTexture *pTarget, D3DSurface*& pTargSurf, SRTStack *pCur, int nCMSide=-1);
  bool FX_SetRenderTarget(int nTarget, D3DSurface *pTargetSurf, SD3DSurface *pDepthTarget, bool bClearOnResolve=false);
  bool FX_PushRenderTarget(int nTarget, D3DSurface *pTargetSurf, SD3DSurface *pDepthTarget, bool bClearOnResolve=false);
  bool FX_SetRenderTarget(int nTarget, CTexture *pTarget, SD3DSurface *pDepthTarget, bool bPush=false, bool bClearOnResolve=false, int nCMSide=-1);
  bool FX_PushRenderTarget(int nTarget, CTexture *pTarget, SD3DSurface *pDepthTarget, bool bClearOnResolve=false, int nCMSide=-1);
  bool FX_RestoreRenderTarget(int nTarget);
  bool FX_PopRenderTarget(int nTarget);
  SD3DSurface *FX_GetDepthSurface(int nWidth, int nHeight, bool bAA);
  SD3DSurface *FX_GetScreenDepthSurface(bool bAA=false);

//========================================================================================


  void EF_Commit(bool bAllowDIP=false);
  void FX_ZState(int& nState);
  bool FX_SetFPMode();
  inline void *FX_LockVB(uint nSize, uint& nStart);
  inline void FX_UnlockVB();

  void FX_CommitStates(SShaderTechnique *pTech, SShaderPass *pPass, bool bUseMaterialState);

  _inline void EF_DrawBatch(CShader *sh, SShaderPass *sl)
  {
    int bFogOverrided = 0;
    // Unlock all VB (if needed) and set current streams
    FX_CommitStreams(sl);
    {
      bFogOverrided = EF_FogCorrection();
      if (m_RP.m_pRE)
        m_RP.m_pRE->mfDraw(sh, sl);
      else
        EF_DrawIndexedMesh(R_PRIMV_TRIANGLES);
      EF_FogRestore(bFogOverrided);
    }
  }

  bool FX_CommitStreams(SShaderPass *sl, bool bSetVertexDecl=true);

	// get the anti aliasing formats available for current width, height and BPP
  int GetAAFormat(TArray<SAAFormat>& Formats, bool bReset);

  void EF_SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa);

  // Clip Planes support 
  void EF_SetClipPlane(bool bEnable, float *pPlane, bool bRefract);

  void FX_HDRPostProcessing();

  void EF_Init();
  void EF_InitLightInfotable_DB();
  void EF_InitVFMergeMap();
  void EF_PreRender(int Stage);
  void EF_PostRender();
  void EF_InitRandTables();
  void EF_InitWaveTables();
#if defined (DIRECT3D9) || defined (OPENGL)
  void EF_CreateVertexDeclarations(int nStreamMask, D3DVERTEXELEMENT9 *pElements[]);
#elif defined (DIRECT3D10)
  void EF_CreateVertexDeclarations(int nStreamMask, D3D10_INPUT_ELEMENT_DESC *pElements[], int nNumElements[]);
#endif
  void EF_InitD3DVertexDeclarations();
  void EF_SetCameraInfo();
  void EF_SetObjectTransform(CRenderObject *obj, CShader *pSH, int nTransFlags);
  bool EF_ObjectChange(CShader *Shader, SRenderShaderResources *pRes, CRenderObject *pObject, CRendElement *pRE);

  void EF_DrawDebugLights();
  void EF_DrawDebugTools();

  static void EF_DrawWire();
  static void EF_DrawNormals();
  static void EF_DrawTangents();
  static void EF_Flush();
  static void EF_FlushShadow();

  static void FX_SelectTechnique(CShader *pShader, SShaderTechnique *pTech);

  _inline void FX_Flush()
  {
#ifdef DIRECT3D9
    if (CV_r_profileshaders && m_pQueryPerf && CV_r_profileDIPs == 1)
    {
      m_pQueryPerf->Issue(D3DISSUE_END);
      float fTime = iTimer->GetAsyncCurTime();
      bool bInfinite = false;
      HRESULT hr;
      do
      {
        float fDif = iTimer->GetAsyncCurTime() - fTime;
        if (fDif > 2.0f)
        {
          // 2 seconds in the loop
          bInfinite = true;
          break;
        }
        hr = m_pQueryPerf->GetData(NULL, 0, D3DGETDATA_FLUSH);
      } while(hr == S_FALSE);
    }
#endif
  }
  int m_sPrevX, m_sPrevY, m_sPrevWdt, m_sPrevHgt;
  bool m_bsPrev;
  void EF_Scissor(bool bEnable, int sX, int sY, int sWdt, int sHgt);

  void EF_SetFogColor(ColorF Color);
  int EF_FogCorrection();
  void EF_FogRestore(int bFogOverrided);

  void EF_DrawIndexedMesh (int nPrimType);
  bool FX_DebugCheckConsistency(int FirstVertex, int FirstIndex, int RendNumVerts, int RendNumIndices);
  void FX_DebugCheckDeclaration(SD3DVertexDeclaration *pDecl, int nVBStride, int nVBFormat);
  void EF_FlushHW();
  bool EF_SetResourcesState();

  int  FX_Preprocess(SRendItem *ri, uint nums, uint nume);
  void FX_SortRenderLists();
  void FX_SortRenderList(int nList, int nAW, int nR);
  void FX_ProcessBatchesList(int nums, int nume, uint32 nBatchFilter);
  void FX_ProcessZPassRenderLists();
  void FX_ProcessScatterRenderLists();
  void FX_ProcessRainRenderLists(int nList, int nAW, void (*RenderFunc)());
  void FX_ProcessPostRenderLists(uint32 nBatchFilter);
  void FX_ProcessRenderList(int nList, uint32 nBatchFilter);
  void FX_ProcessRenderList(int nums, int nume, int nList, int nAW, void (*RenderFunc)(), bool bLighting);
  void FX_ProcessRenderList(int nList, int nAW, void (*RenderFunc)(), bool bLighting)
  {
    FX_ProcessRenderList(SRendItem::m_StartRI[SRendItem::m_RecurseLevel-1][nAW][nList], SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][nAW][nList], nList, nAW, RenderFunc, bLighting);
  }
  void FX_ProcessPostGroups(int nums, int nume);
  void FX_ProcessAllRenderLists(void (*RenderFunc)(), int nFlags);
  void FX_ProcessLightGroups(int nums, int nume);
	  
  void EF_PrintProfileInfo();

  virtual void EF_EndEf3D (int nFlags);
  virtual void EF_EndEf2D(bool bSort);  // 2d only
  
  virtual WIN_HWND GetHWND() { return  m_hWnd; }
	virtual WIN_HWND GetCurrentContextHWND() { return m_CurrContext ? m_CurrContext->m_hWnd : NULL; }
    
	virtual void SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa);

  virtual void SetTerrainAONodes(PodArray<SSectorTextureSet> * terrainAONodes)
  {
    m_TerrainAONodes.Clear();
    m_TerrainAONodes.AddList(*terrainAONodes);
  }

  PodArray<SSectorTextureSet> m_TerrainAONodes;
  void MakeUnitCube();
  bool FX_PrepareTerrainAOTarget(t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff);
  bool FX_ProcessFillLights(t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff);

  void CalcAABBScreenRect(const AABB & aabb, const CRenderCamera& RCam, Matrix44& mViewProj, Vec3* pvMin,  Vec3* pvMax);

//////////////////////////////////////////////////////////////////////////
public:
	bool IsDeviceLost() { return( m_bDeviceLost ); }
	void InitNVAPI();
	void InitATIAPI();

	virtual IRenderAuxGeom* GetIRenderAuxGeom()
	{
		return( m_pD3DRenderAuxGeom );
	}


private:
	bool CaptureFrameBufferToFile(const char* pFilePath, bool keepIntCaptureBuffer = false);
	bool CaptureMiscBuffersToFiles(const char* pFilePath);
	void CaptureFrameBuffer();
	ICVar* CV_capture_frames;
	ICVar* CV_capture_folder;
	ICVar* CV_capture_file_format;
#if defined (DIRECT3D9) || defined (OPENGL)
	IDirect3DSurface9* m_pCaptureFrameSurf[12];
	IDirect3DSurface9* m_pCaptureScreenShadowMap[MAX_REND_LIGHT_GROUPS];
	bool m_ValidCaptureScreenShadowMap[MAX_REND_LIGHT_GROUPS];
#endif

	CD3DRenderAuxGeom* m_pD3DRenderAuxGeom;

	CShadowTextureGroupManager			m_ShadowTextureGroupManager;			// to combine multiple shadowmaps into one texture

public:
	virtual void BeginPerfEvent(uint32 color, const wchar_t* pstrMessage) const { DXUT_Dynamic_D3DPERF_BeginEvent(color, pstrMessage); }
	virtual void EndPerfEvent() const { DXUT_Dynamic_D3DPERF_EndEvent(); }
};

extern CD3D9Renderer *gcpRendD3D;

//=========================================================================================

#include "D3DHWShader.h"
#include "DynamicVB.h"
#include "DynamicIB.h"
#include "StaticVB.h"
#include "StaticIB.h"

//=========================================================================================
inline void *CD3D9Renderer::FX_LockVB(uint nSize, uint& nStart)
{
  int nCurVB = m_RP.m_CurVB;
  if (m_RP.m_VBs[nCurVB].VBPtr_0->GetBytesOffset()+nSize >= m_RP.m_VBs[nCurVB].VBPtr_0->GetBytesCount())
  {
    m_RP.m_VBs[nCurVB].VBPtr_0->Reset();
    m_RP.m_CurVB = (nCurVB + 1) & (MAX_DYNVBS-1);
  }
  nCurVB = m_RP.m_CurVB;
  void *pVB = (void *)m_RP.m_VBs[nCurVB].VBPtr_0->Lock(nSize, nStart);
  return pVB;
}
inline void CD3D9Renderer::FX_UnlockVB()
{
  m_RP.m_VBs[m_RP.m_CurVB].VBPtr_0->Unlock();
}

//=========================================================================================

void HDR_DrawDebug();

#ifdef _DEBUG
struct SDynVB
{
#if defined (DIRECT3D9) || defined(OPENGL)
  IDirect3DResource9 *m_pRes;
#elif defined (DIRECT3D10)
  ID3D10Buffer *m_pRes;
#endif
  SVertexStream *m_pStr;
  CVertexBuffer *m_pBuf;
  char m_szDescr[256];
  int m_nOffs;
};

extern TArray<SDynVB> gDVB;

#if defined (DIRECT3D9) || defined(OPENGL)
_inline void sAddVB(IDirect3DResource9 *pRes, SVertexStream *pStr, CVertexBuffer *pBuf, const char *szDesc, int nOffs)
#elif defined (DIRECT3D10)
_inline void sAddVB(ID3D10Buffer *pRes, SVertexStream *pStr, CVertexBuffer *pBuf, const char *szDesc, int nOffs)
#endif
{
  return;
  //if (!pStr->m_bDynamic)
  //  return;

  uint i;
  for (i=0; i<gDVB.Num(); i++)
  {
    if (gDVB[i].m_pRes == pRes && gDVB[i].m_nOffs == nOffs)
    {
      assert(0);
    }
  }
  SDynVB VB;
  VB.m_pRes = pRes;
  VB.m_pBuf = pBuf;
  VB.m_pStr = pStr;
  VB.m_nOffs = nOffs;
  if (szDesc)
    strcpy(VB.m_szDescr, szDesc);
  else
    VB.m_szDescr[0] = 0;
  gDVB.AddElem(VB);
}
#if defined (DIRECT3D9) || defined(OPENGL)
_inline void sRemoveVB(IDirect3DResource9 *pRes, SVertexStream *pStr, int nOffs)
#elif defined (DIRECT3D10)
_inline void sRemoveVB(ID3D10Buffer *pRes, SVertexStream *pStr, int nOffs)
#endif
{
  return;
  //if (!pStr->m_bDynamic)
  //  return;

  uint i;
  for (i=0; i<gDVB.Num(); i++)
  {
    if (gDVB[i].m_pRes == pRes && gDVB[i].m_nOffs == nOffs)
    {
      gDVB.Remove(i);
      return;
    }
  }
  assert(0);
}
#endif // _DEBUG

#endif // DRIVERD3D9_H
