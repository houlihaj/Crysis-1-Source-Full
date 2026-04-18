/*=============================================================================
  D3DTexture.cpp : Direct3D specific texture manager implementation.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "D3DTexture.h"
#include "D3DCubeMaps.h"
#include "I3DEngine.h"
#include "StringUtils.h"
#include "IDirectBee.h"			// connection to D3D9 wrapper
#include <IFlashPlayer.h>
#include <IVideoPlayer.h>

#ifdef XENON
#include "xgraphics.h"
#endif

//===============================================================================

byte *CTexture::Convert(byte *pSrc, int nWidth, int nHeight, int nMips, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nOutMips, int& nOutSize, bool bLinear)
{
  nOutSize = 0;
  byte *pDst = NULL;
  if (eTFSrc == eTFDst && nMips == nOutMips)
    return pSrc;
  CD3D9Renderer *r = gcpRendD3D;
  HRESULT h;
#if defined (DIRECT3D9) || defined(OPENGL)
  D3DFORMAT DeviceFormatSRC = (D3DFORMAT)DeviceFormatFromTexFormat(eTFSrc);
  D3DFORMAT DeviceFormatDST = (D3DFORMAT)DeviceFormatFromTexFormat(eTFDst);
  if (DeviceFormatSRC == D3DFMT_UNKNOWN || DeviceFormatDST == D3DFMT_UNKNOWN)
#elif defined (DIRECT3D10)
  DXGI_FORMAT DeviceFormatSRC = (DXGI_FORMAT)DeviceFormatFromTexFormat(eTFSrc);
  if (eTFDst == eTF_L8)
    eTFDst = eTF_A8;
  DXGI_FORMAT DeviceFormatDST = (DXGI_FORMAT)DeviceFormatFromTexFormat(eTFDst);
  if (DeviceFormatSRC == DXGI_FORMAT_UNKNOWN || DeviceFormatDST == DXGI_FORMAT_UNKNOWN)
#endif
  {
    assert(0);
    return NULL;
  }
  if (nMips <= 0)
    nMips = 1;
  int nSizeDSTMips = 0;
  int i;

  if (eTFSrc == eTF_3DC)
  {
		if(eTFDst == eTF_DXT5)
		{
			// convert 3DC to DXT5 - for compressed normals
			if (!nOutMips)
				nOutMips = nMips;
			int wdt = nWidth;
			int hgt = nHeight;
			nSizeDSTMips = CTexture::TextureDataSize(wdt, hgt, 1, nOutMips, eTF_DXT5);
			byte *pDst = new byte [nSizeDSTMips];
			int nOffsDST = 0;
			int nOffsSRC = 0;
			for (i=0; i<nOutMips; i++)
			{
				if (i >= nMips)
					break;
				if (wdt <= 0)
					wdt = 1;
				if (hgt <= 0)
					hgt = 1;
				void *outSrc = &pSrc[nOffsSRC];
				DWORD outSize = CTexture::TextureDataSize(wdt, hgt, 1, 1, eTFDst);

				nOffsSRC += CTexture::TextureDataSize(wdt, hgt, 1, 1, eTFSrc);

				{
					byte *src = (byte *)outSrc;

					// 4x4 block requires 8 bytes in DXT5 or 3DC

					// DXT5:  8 bit alpha0, 8 bit alpha1, 16*3 bit alpha lerp
					//        16bit col0, 16 bit col1 (R5G6B5 low byte then high byte), 16*2 bit color lerp

					//  3DC:  8 bit x0, 8 bit x1, 16*3 bit x lerp
					//        8 bit y0, 8 bit y1, 16*3 bit y lerp

					for (int n=0; n < outSize/16; n++)			// for each block
					{
						uint8 *pSrcBlock = &src[n*16];
						uint8 *pDstBlock = &pDst[nOffsDST+n*16];

						for(uint32 dwK=0;dwK<8;++dwK)
							pDstBlock[dwK]=pSrcBlock[dwK];
						for(uint32 dwK=8;dwK<16;++dwK)
							pDstBlock[dwK]=0;

						// 6 bit green channel (highest bits)
						// by using all 3 channels with a slight offset we can get more precision but then a dot product would be needed in PS
						// because of bilinear filter we cannot just distribute bits to get perfect result
						uint16 colDst0 = (((uint16)pSrcBlock[8]+2)>>2)<<5;
						uint16 colDst1 = (((uint16)pSrcBlock[9]+2)>>2)<<5;

						bool bFlip = colDst0<=colDst1;

						if(bFlip)
						{
							uint16 help = colDst0;
							colDst0=colDst1;
							colDst1=help;
						}

						bool bEqual = colDst0==colDst1;

						// distribute bytes by hand to not have problems with endianess
						pDstBlock[8+0] = (uint8)colDst0;
						pDstBlock[8+1] = (uint8)(colDst0>>8);
						pDstBlock[8+2] = (uint8)colDst1;
						pDstBlock[8+3] = (uint8)(colDst1>>8);

						uint16 *pSrcBlock16 = (uint16 *)(pSrcBlock+10);
						uint16 *pDstBlock16 = (uint16 *)(pDstBlock+12);

						// distribute 16 3 bit values to 16 2 bit values (loosing LSB)
						for(uint32 dwK=0;dwK<16;++dwK)
						{
							uint32 dwBit0 = dwK*3+0;
							uint32 dwBit1 = dwK*3+1;
							uint32 dwBit2 = dwK*3+2;

							uint8 hexDataIn = (((pSrcBlock16[(dwBit2>>4)]>>(dwBit2&0xf))&1)<<2)			// get HSB
															| (((pSrcBlock16[(dwBit1>>4)]>>(dwBit1&0xf))&1)<<1)
															| ((pSrcBlock16[(dwBit0>>4)]>>(dwBit0&0xf))&1);					// get LSB

							uint8 hexDataOut;

							switch(hexDataIn)
							{
								case 0: hexDataOut=0;break;		// color 0
								case 1: hexDataOut=1;break;		// color 1

								case 2: hexDataOut=0;break;		// mostly color 0
								case 3: hexDataOut=2;break;
								case 4: hexDataOut=2;break;
								case 5: hexDataOut=3;break;
								case 6: hexDataOut=3;break;
								case 7: hexDataOut=1;break;		// mostly color 1

								default:
									assert(0);
							}

							if(bFlip)
							{
								if(hexDataOut<2)
									hexDataOut=1-hexDataOut;		// 0<->1
								else
									hexDataOut = 5-hexDataOut;	// 2<->3
							}

							if(bEqual)
								if(hexDataOut==3)
									hexDataOut=1;

							pDstBlock16[(dwK>>3)] |= (hexDataOut<<((dwK&0x7)<<1));
						}
					}

					nOffsDST += outSize;
				}

				wdt >>= 1;
				hgt >>= 1;
			}
			if ((nOutMips>1 && nOutMips != nMips))
			{
				assert(0);			// generate mips if needed - should not happen
				return 0;
			}
			nOutSize = nSizeDSTMips;
			return pDst;
		}
		else
		{
			if (!DeCompressTextureATI)
				return NULL;
			if (!nOutMips)
				nOutMips = nMips;
			int wdt = nWidth;
			int hgt = nHeight;
			nSizeDSTMips = CTexture::TextureDataSize(wdt, hgt, 1, nOutMips, eTF_A8R8G8B8);
			byte *pDst = new byte [nSizeDSTMips];
			int nOffsDST = 0;
			int nOffsSRC = 0;
			for (i=0; i<nOutMips; i++)
			{
				if (i >= nMips)
					break;
				if (wdt <= 0)
					wdt = 1;
				if (hgt <= 0)
					hgt = 1;
				void *outData = NULL;
				DWORD outSize = 0;
				COMPRESSOR_ERROR err = DeCompressTextureATI(wdt, hgt, FORMAT_COMP_ATI2N, FORMAT_ARGB_8888, &pSrc[nOffsSRC], &outData, &outSize);
				nOffsSRC += CTexture::TextureDataSize(wdt, hgt, 1, 1, eTFSrc);
				if (err == COMPRESSOR_ERROR_NONE)
				{
					byte *src = (byte *)outData;
					for (int n=0; n<wdt*hgt; n++)
					{
						pDst[nOffsDST+n*4+0] = src[n*4+0];
						pDst[nOffsDST+n*4+1] = src[n*4+1];
						pDst[nOffsDST+n*4+2] = src[n*4+2];
						pDst[nOffsDST+n*4+3] = src[n*4+3];
					}
					nOffsDST += outSize;
					DeleteDataATI(outData);
				}
				else
				{
					assert(0);
					return NULL;
				}
				wdt >>= 1;
				hgt >>= 1;
			}
			if ((nOutMips>1 && nOutMips != nMips) || (eTFDst != eTF_A8R8G8B8 && eTFDst != eTF_X8R8G8B8))
			{
				byte *pDstNew = Convert(pDst, nWidth, nHeight, min(nMips, nOutMips), eTF_A8R8G8B8, eTFDst, nOutMips, nOutSize, true);
				delete [] pDst;
				return pDstNew;
			}
			nOutSize = nSizeDSTMips;
			return pDst;
		}
  }
  else
  if (eTFDst == eTF_3DC)
  {
    if (!CompressTextureATI)
      return NULL;
  }
  else
  {
#ifdef XENON
    if (bLinear)
    {
      DeviceFormatDST = (D3DFORMAT)GetD3DLinFormat(DeviceFormatDST);
      DeviceFormatSRC = (D3DFORMAT)GetD3DLinFormat(DeviceFormatSRC);
    }
#endif
#if defined (DIRECT3D9) || defined(OPENGL)
    LPDIRECT3DTEXTURE9 pID3DTextureDST = NULL;
    LPDIRECT3DSURFACE9 pSurfDST;
    D3DLOCKED_RECT d3dlr;
    if(FAILED(h=D3DXCreateTexture(gcpRendD3D->GetDevice(), nWidth, nHeight, nOutMips, 0, DeviceFormatDST, D3DPOOL_SYSTEMMEM, &pID3DTextureDST)))
      return NULL;
    int wdt = nWidth;
    int hgt = nHeight;
    int nOffsSRC = 0;
    for (i=0; i<nMips; i++)
    {
      if (!wdt)
        wdt = 1;
      if (!hgt)
        hgt = 1;
      int nSizeSRC = TextureDataSize(wdt, hgt, 1, 1, eTFSrc);
      h = pID3DTextureDST->GetSurfaceLevel(i, &pSurfDST);

      int nPitch;
      if (!IsDXTCompressed(eTFSrc))
        nPitch = nSizeSRC / hgt;
      else
      {
        int blockSize = (eTFSrc == eTF_DXT1) ? 8 : 16;
        nPitch = (wdt+3)/4 * blockSize;
      }
      RECT srcRect;
      srcRect.left = 0;
      srcRect.top = 0;
      srcRect.right = wdt;
      srcRect.bottom = hgt;

      h = D3DXLoadSurfaceFromMemory(pSurfDST, NULL, NULL, &pSrc[nOffsSRC], DeviceFormatSRC, nPitch, NULL, &srcRect, D3DX_FILTER_NONE, 0);

      SAFE_RELEASE(pSurfDST);

      nOffsSRC += nSizeSRC;
      wdt >>= 1;
      hgt >>= 1;
    }
    if (nMips <= 1 && nOutMips > 1)
      h = D3DXFilterTexture(pID3DTextureDST, NULL, D3DX_DEFAULT, D3DX_FILTER_LINEAR);
    wdt = nWidth;
    hgt = nHeight;
    nSizeDSTMips = TextureDataSize(wdt, hgt, 1, nOutMips, eTFDst);
    pDst = new byte[nSizeDSTMips];
    int nOffsDST = 0;
    for (i=0; i<nOutMips; i++)
    {
      if (!wdt)
        wdt = 1;
      if (!hgt)
        hgt = 1;
      int nSizeDST = TextureDataSize(wdt, hgt, 1, 1, eTFDst);

      pID3DTextureDST->LockRect(i, &d3dlr, NULL, 0);
      memcpy(&pDst[nOffsDST], d3dlr.pBits, nSizeDST);
      pID3DTextureDST->UnlockRect(i);

      nOffsDST += nSizeDST;
      wdt >>= 1;
      hgt >>= 1;
    }
    SAFE_RELEASE(pID3DTextureDST);
#elif defined (DIRECT3D10)
    if (eTFDst == eTF_A8 && (eTFSrc == eTF_X8R8G8B8 || eTFSrc == eTF_A8R8G8B8) && nMips == nOutMips)
    {
      int nSrcSize = CTexture::TextureDataSize(nWidth, nHeight, 1, nMips, eTFSrc) / 4;
      pDst = new byte[nSrcSize];
      for (i=0; i<nSrcSize; i++)
      {
        pDst[i] = (pSrc[i*4+0] + pSrc[i*4+1] + pSrc[i*4+2]) / 3;
      }
      nSizeDSTMips = nSrcSize;
      return pDst;
    }
    D3D10_TEXTURE2D_DESC Desc;
    ZeroStruct(Desc);
    ID3D10Texture2D *pID3DTextureDST = NULL;
    Desc.Format = DeviceFormatDST;
    Desc.Width = nWidth;
    Desc.Height = nHeight;
    Desc.MipLevels = nOutMips;
    Desc.ArraySize = 1;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D10_USAGE_STAGING;
    Desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    Desc.BindFlags = 0;
    gcpRendD3D->m_pd3dDevice->CreateTexture2D(&Desc, NULL, &pID3DTextureDST);
    int nSrcSize = CTexture::TextureDataSize(nWidth, nHeight, 1, nMips, eTFSrc);
    D3DX10_IMAGE_LOAD_INFO LoadInfo;
    ZeroStruct(LoadInfo);
    LoadInfo.Width = D3DX_FROM_FILE;
    LoadInfo.Height = D3DX_FROM_FILE;
    LoadInfo.Depth = D3DX_FROM_FILE;
    LoadInfo.FirstMipLevel = 0;
    LoadInfo.MipLevels = nOutMips;
    LoadInfo.Usage = D3D10_USAGE_STAGING;
    LoadInfo.BindFlags = 0;
    LoadInfo.CpuAccessFlags = D3D10_CPU_ACCESS_WRITE | D3D10_CPU_ACCESS_READ;
    LoadInfo.MiscFlags = 0;
    LoadInfo.Format = DeviceFormatDST;
    LoadInfo.Filter = D3DX10_FILTER_NONE;
    LoadInfo.MipFilter = D3DX10_FILTER_NONE;

    D3DX10_IMAGE_INFO ImgInfo;
    ZeroStruct(ImgInfo);
    ImgInfo.Width = nWidth;
    ImgInfo.Height = nHeight;
    ImgInfo.Depth = 1;
    ImgInfo.Format = DeviceFormatSRC;
    ImgInfo.MipLevels = nMips;
    ImgInfo.ArraySize = 1;
    ImgInfo.ImageFileFormat = D3DX10_IFF_DDS;
    ImgInfo.ResourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;

    LoadInfo.pSrcInfo = &ImgInfo;

    nSizeDSTMips = 0;
    int nSizeDDS = 0;
    pDst = WriteDDS(pSrc, nWidth, nHeight, 1, NULL, eTFSrc, nMips, eTT_2D, true, &nSizeDDS);
    h = D3DX10CreateTextureFromMemory(gcpRendD3D->m_pd3dDevice, pDst, nSizeDDS, &LoadInfo, NULL, (ID3D10Resource **)&pID3DTextureDST, &h);
    assert(SUCCEEDED(h));
    SAFE_DELETE_ARRAY(pDst);
    int nDstSize = CTexture::TextureDataSize(nWidth, nHeight, 1, nMips, eTFDst);
    pDst = new byte[nDstSize];
    if (pID3DTextureDST)
    {
      int nOffsDST = 0;
      for (i=0; i<nOutMips; i++)
      {
        if (!nWidth)
          nWidth = 1;
        if (!nHeight)
          nHeight = 1;
        int nSizeDST = TextureDataSize(nWidth, nHeight, 1, 1, eTFDst);

        D3D10_MAPPED_TEXTURE2D MappedTex2D;
        pID3DTextureDST->Map(i, D3D10_MAP_READ, 0, &MappedTex2D);
        memcpy(&pDst[nOffsDST], MappedTex2D.pData, nSizeDST);
        pID3DTextureDST->Unmap(i);

        nOffsDST += nSizeDST;
        nSizeDSTMips += nSizeDST;
        nWidth >>= 1;
        nHeight >>= 1;
      }
      SAFE_RELEASE(pID3DTextureDST);
    }
#endif
  }
  nOutSize = nSizeDSTMips;
  return pDst;
}

void *CTexture::GetSurface(int nCMSide, int nLevel)
{
  if (!m_pDeviceTexture)
    return NULL;
  HRESULT hr = S_OK;
#if defined (DIRECT3D9) || defined (OPENGL)
  LPDIRECT3DSURFACE9 pTargSurf = NULL;
  if (m_eTT == eTT_Cube)
  {
    assert(nCMSide >= 0 && nCMSide < 6);
    LPDIRECT3DCUBETEXTURE9 pID3DTargetCubeTexture = (LPDIRECT3DCUBETEXTURE9)m_pDeviceTexture;
    hr = pID3DTargetCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)nCMSide, nLevel, &pTargSurf);
  }
  else
  {
    LPDIRECT3DTEXTURE9 pID3DTargetTexture = (LPDIRECT3DTEXTURE9)m_pDeviceTexture;
    hr = pID3DTargetTexture->GetSurfaceLevel(nLevel, &pTargSurf);
  }
#elif defined (DIRECT3D10)
  ID3D10Texture2D *pID3DTexture = NULL;
  ID3D10Texture3D *pID3DTexture3D = NULL;
  ID3D10Texture2D *pID3DTextureCube = NULL;
  ID3D10RenderTargetView *pTargSurf = (ID3D10RenderTargetView *)m_pDeviceRTV;
  if (!pTargSurf)
  {
    if (m_eTT == eTT_2D)
    {
      pID3DTexture = (ID3D10Texture2D *)m_pDeviceTexture;
      if (m_nFlags & FT_USAGE_FSAA)
        pID3DTexture = (ID3D10Texture2D *)m_pDeviceTextureMSAA;

      D3D10_RENDER_TARGET_VIEW_DESC DescRT;
      DescRT.Format = (DXGI_FORMAT)m_pPixelFormat->DeviceFormat;
      DescRT.ViewDimension = (m_nFlags & FT_USAGE_FSAA) ? D3D10_RTV_DIMENSION_TEXTURE2DMS : D3D10_RTV_DIMENSION_TEXTURE2D;
      DescRT.Texture2D.MipSlice = 0;
      hr = gcpRendD3D->m_pd3dDevice->CreateRenderTargetView(pID3DTexture, &DescRT, &pTargSurf);
    }
    else
    if (m_eTT == eTT_Cube)
    {
      pID3DTextureCube = (ID3D10Texture2D *)m_pDeviceTexture;

      D3D10_RENDER_TARGET_VIEW_DESC DescRT;
      DescRT.Format = (DXGI_FORMAT)m_pPixelFormat->DeviceFormat;
      DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
      DescRT.Texture2DArray.FirstArraySlice = nCMSide;
      DescRT.Texture2DArray.ArraySize = 1;
      DescRT.Texture2DArray.MipSlice = 0;
      hr = gcpRendD3D->m_pd3dDevice->CreateRenderTargetView(pID3DTextureCube, &DescRT, &pTargSurf);
    }
    m_pDeviceRTV = pTargSurf;
  }
#endif
  assert(hr == S_OK);

  return pTargSurf;
}

int CTexture::DeviceFormatFromTexFormat(ETEX_Format eTF)
{
#if defined (DIRECT3D9) || defined (OPENGL)
  switch (eTF)
  {
    case eTF_A8R8G8B8:
      return D3DFMT_A8R8G8B8;
    case eTF_X8R8G8B8:
      return D3DFMT_X8R8G8B8;
    case eTF_A8:
      return D3DFMT_A8;
    case eTF_A8L8:
      return D3DFMT_A8L8;
    case eTF_L8:
      return D3DFMT_L8;
    case eTF_A4R4G4B4:
      return D3DFMT_A4R4G4B4;
    case eTF_R5G6B5:
      return D3DFMT_R5G6B5;
    case eTF_R5G5B5:
      return D3DFMT_X1R5G5B5;
    case eTF_V8U8:
      return D3DFMT_V8U8;
#if !defined(XENON)
    case eTF_R8G8B8:
      return D3DFMT_R8G8B8;
    case eTF_CxV8U8:
      return D3DFMT_CxV8U8;
#endif
    case eTF_X8L8V8U8:
      return D3DFMT_X8L8V8U8;
    case eTF_L8V8U8:
      assert(0);
      break;
    case eTF_V16U16:
      return D3DFMT_V16U16;
    case eTF_A16B16G16R16:
      return D3DFMT_A16B16G16R16;
    case eTF_A16B16G16R16F:
      return D3DFMT_A16B16G16R16F;
    case eTF_A32B32G32R32F:
      return D3DFMT_A32B32G32R32F;
    case eTF_R32F:
      return D3DFMT_R32F;
    case eTF_R16F:
      return D3DFMT_R16F;
    case eTF_G16R16:
      return D3DFMT_G16R16;
    case eTF_G16R16F:
      return D3DFMT_G16R16F;
    case eTF_DXT1:
      return D3DFMT_DXT1;
    case eTF_DXT3:
      return D3DFMT_DXT3;
    case eTF_DXT5:
      return D3DFMT_DXT5;
    case eTF_3DC:
      return D3DFMT_3DC;
    case eTF_DEPTH16:
      return D3DFMT_D16;
    case eTF_DEPTH24:
      return D3DFMT_D24X8;
    case eTF_DF16:
      return D3DFMT_DF16;
    case eTF_DF24:
      return D3DFMT_DF24;
    case eTF_D16:
      return D3DFMT_D16;
    case eTF_D24S8:
      return D3DFMT_D24S8;
    case eTF_NULL:
      return D3DFMT_NULL;


    default:
      assert(0);
  }
#elif defined (DIRECT3D10)
  switch (eTF)
  {
  case eTF_R8G8B8:
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  case eTF_A8R8G8B8:
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  case eTF_X8R8G8B8:
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  case eTF_A8:
    return DXGI_FORMAT_A8_UNORM;
  case eTF_A8L8:
    return DXGI_FORMAT_UNKNOWN;
  case eTF_L8:
    return DXGI_FORMAT_UNKNOWN;
  case eTF_A4R4G4B4:
    return DXGI_FORMAT_UNKNOWN;
  case eTF_R5G6B5:
    return DXGI_FORMAT_B5G6R5_UNORM;
  case eTF_R5G5B5:
    return DXGI_FORMAT_B5G5R5A1_UNORM;
  case eTF_V8U8:
    return DXGI_FORMAT_R8G8_UNORM;
  case eTF_X8L8V8U8:
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  case eTF_L8V8U8:
    assert(0);
    break;
  case eTF_V16U16:
    return DXGI_FORMAT_R16G16_UNORM;
  case eTF_A16B16G16R16:
    return DXGI_FORMAT_R16G16B16A16_UNORM;
  case eTF_A16B16G16R16F:
    return DXGI_FORMAT_R16G16B16A16_FLOAT;
  case eTF_A32B32G32R32F:
    return DXGI_FORMAT_R32G32B32A32_FLOAT;
  case eTF_R32F:
    return DXGI_FORMAT_R32_FLOAT;
  case eTF_R16F:
    return DXGI_FORMAT_R16_FLOAT;
  case eTF_G16R16F:
    return DXGI_FORMAT_R16G16_FLOAT;
  case eTF_DXT1:
    return DXGI_FORMAT_BC1_UNORM;
  case eTF_DXT3:
    return DXGI_FORMAT_BC2_UNORM;
  case eTF_DXT5:
    return DXGI_FORMAT_BC3_UNORM;
  case eTF_3DC:
    return DXGI_FORMAT_BC5_UNORM;
  case eTF_DEPTH16:
    return DXGI_FORMAT_D16_UNORM;
  case eTF_D24S8:
    return DXGI_FORMAT_D24_UNORM_S8_UINT;
  case eTF_D32F:
    return DXGI_FORMAT_D32_FLOAT;

  default:
    assert(0);
  }
#endif
  return 0;
}

ETEX_Format CTexture::TexFormatFromDeviceFormat(int nFormat)
{
  switch (nFormat)
  {
#if !defined(XENON)
    case D3DFMT_R8G8B8:
      return eTF_R8G8B8;
#endif
    case D3DFMT_A8R8G8B8:
      return eTF_A8R8G8B8;
    case D3DFMT_X8R8G8B8:
      return eTF_X8R8G8B8;
    case D3DFMT_A8:
      return eTF_A8;
    case D3DFMT_A8L8:
      return eTF_A8L8;
    case D3DFMT_L8:
      return eTF_L8;
    case D3DFMT_A4R4G4B4:
      return eTF_A4R4G4B4;
    case D3DFMT_R5G6B5:
      return eTF_R5G6B5;
    case D3DFMT_X1R5G5B5:
      return eTF_R5G5B5;
    case D3DFMT_V8U8:
      return eTF_V8U8;
#if !defined(XENON)
    case D3DFMT_CxV8U8:
      return eTF_CxV8U8;
#endif
    case D3DFMT_X8L8V8U8:
      return eTF_X8L8V8U8;
    case D3DFMT_V16U16:
      return eTF_V16U16;
    case D3DFMT_A16B16G16R16:
      return eTF_A16B16G16R16;
    case D3DFMT_A16B16G16R16F:
      return eTF_A16B16G16R16F;
    case D3DFMT_A32B32G32R32F:
      return eTF_A32B32G32R32F;
    case D3DFMT_R32F:
      return eTF_R32F;
    case D3DFMT_R16F:
      return eTF_R16F;
    case D3DFMT_G16R16:
      return eTF_G16R16;
    case D3DFMT_G16R16F:
      return eTF_G16R16F;
    case D3DFMT_DXT1:
      return eTF_DXT1;
    case D3DFMT_DXT3:
      return eTF_DXT3;
    case D3DFMT_DXT5:
      return eTF_DXT5;
#if !defined(PS3)
    case D3DFMT_3DC:
      return eTF_3DC;
#endif
    case D3DFMT_D24X8:
      return eTF_DEPTH24;
		case D3DFMT_DF16:
			return eTF_DF16;
		case D3DFMT_DF24:
			return eTF_DF24;
    case D3DFMT_D16:
      return eTF_D16;
    case D3DFMT_D24S8:
      return eTF_D24S8;
		case D3DFMT_NULL:
			return eTF_NULL;
    default:
      assert(0);
  }
  return eTF_Unknown;
}

int CTexture::GetD3DLinFormat(int nFormat)
{
#ifdef XENON
  switch (nFormat)
  {
#if !defined(XENON)
    case D3DFMT_R8G8B8:
      return D3DFMT_R8G8B8;
#endif
    case D3DFMT_A8R8G8B8:
      return D3DFMT_LIN_A8R8G8B8;
    case D3DFMT_X8R8G8B8:
      return D3DFMT_LIN_X8R8G8B8;
    case D3DFMT_A8:
      return D3DFMT_LIN_A8;
    case D3DFMT_A8L8:
      return D3DFMT_LIN_A8L8;
    case D3DFMT_L8:
      return D3DFMT_LIN_L8;
    case D3DFMT_A4R4G4B4:
      return D3DFMT_LIN_A4R4G4B4;
    case D3DFMT_R5G6B5:
      return D3DFMT_LIN_R5G6B5;
    case D3DFMT_X1R5G5B5:
      return D3DFMT_LIN_X1R5G5B5;
    case D3DFMT_V8U8:
      return D3DFMT_LIN_V8U8;
    case D3DFMT_X8L8V8U8:
      return D3DFMT_LIN_X8L8V8U8;
    case D3DFMT_V16U16:
      return D3DFMT_LIN_V16U16;
    case D3DFMT_A16B16G16R16:
      return D3DFMT_LIN_A16B16G16R16;
    case D3DFMT_A16B16G16R16F:
      return D3DFMT_LIN_A16B16G16R16F;
    case D3DFMT_A32B32G32R32F:
      return D3DFMT_LIN_A32B32G32R32F;
    case D3DFMT_R32F:
      return D3DFMT_LIN_R32F;
    case D3DFMT_R16F:
      return D3DFMT_LIN_R16F;
    case D3DFMT_G16R16F:
      return D3DFMT_LIN_G16R16F;
    case D3DFMT_DXT1:
      return D3DFMT_LIN_DXT1;
    case D3DFMT_DXT3:
      return D3DFMT_LIN_DXT3;
    case D3DFMT_DXT5:
      return D3DFMT_LIN_DXT5;
    case D3DFMT_3DC:
      return D3DFMT_3DC;
    case D3DFMT_D16:
      return D3DFMT_D16;
    case D3DFMT_D24S8:
      return D3DFMT_D24X8;
    default:
      assert(0);
  }
#endif
  return nFormat;
}

#if defined (DIRECT3D10)

int CTexture::ConvertToDepthStencilFmt( int nFormat )
{
  switch ((DXGI_FORMAT)nFormat)
  {
    case (DXGI_FORMAT_R24G8_TYPELESS):    return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case (DXGI_FORMAT_R32G8X24_TYPELESS): return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    //case (DXGI_FORMAT_R32G32_TYPELESS):   return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    case (DXGI_FORMAT_R32_TYPELESS):      return DXGI_FORMAT_D32_FLOAT;
	  case (DXGI_FORMAT_R16_TYPELESS):      return DXGI_FORMAT_D16_UNORM;
    default: break;
  }
  assert( 0 );
  return 0;
}

int CTexture::ConvertToShaderResourceFmt(int nFormat)
{
  //handle special cases
  switch ((DXGI_FORMAT)nFormat)
  {
    case (DXGI_FORMAT_R24G8_TYPELESS):    return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case (DXGI_FORMAT_R32G8X24_TYPELESS): return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    //case (DXGI_FORMAT_R32G32_TYPELESS):   return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    case (DXGI_FORMAT_R32_TYPELESS):      return DXGI_FORMAT_R32_FLOAT;
    case (DXGI_FORMAT_R16_TYPELESS):      return DXGI_FORMAT_R16_UNORM;

    /*
      //128 bits
    DXGI_FORMAT_R32G32B32A32_TYPELESS,
    DXGI_FORMAT_R32G32B32_TYPELESS,

        //64 bits
    DXGI_FORMAT_R16G16B16A16_TYPELESS,
    DXGI_FORMAT_R32G32_TYPELESS,
    DXGI_FORMAT_R32G8X24_TYPELESS,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,

        //32 bits
    DXGI_FORMAT_R10G10B10A2_TYPELESS,
    DXGI_FORMAT_R8G8B8A8_TYPELESS,
    DXGI_FORMAT_R16G16_TYPELESS,
    DXGI_FORMAT_R32_TYPELESS,
    DXGI_FORMAT_R24G8_TYPELESS,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT,

        //16 bits
    DXGI_FORMAT_R8G8_TYPELESS,
    DXGI_FORMAT_R16_TYPELESS,

        //8 bits
    DXGI_FORMAT_R8_TYPELESS,

        //block formats
    DXGI_FORMAT_BC1_TYPELESS,
    DXGI_FORMAT_BC2_TYPELESS,
    DXGI_FORMAT_BC3_TYPELESS,
    DXGI_FORMAT_BC4_TYPELESS,
    DXGI_FORMAT_BC5_TYPELESS,
    */

    default: break;
  }

  //pass through
  return nFormat;
}
#else

int CTexture::ConvertToDepthStencilFmt( int nFormat )
{
  assert(0);
  return 0;
}

int CTexture::ConvertToShaderResourceFmt(int nFormat)
{
  assert(0);
  return 0;
}

#endif


bool CTexture::IsFormatSupported(ETEX_Format eTFDst)
{
  CD3D9Renderer *rd = gcpRendD3D;
  int D3DFmt = DeviceFormatFromTexFormat(eTFDst);
  if (!D3DFmt)
    return false;
  SPixFormat *pFmt;
  for (pFmt=rd->m_FirstPixelFormat; pFmt; pFmt=pFmt->Next)
  {
    if (pFmt->DeviceFormat == D3DFmt && pFmt->IsValid())
      return true;
  }
  return false;
}

ETEX_Format CTexture::ClosestFormatSupported(ETEX_Format eTFDst)
{
  CD3D9Renderer *rd = gcpRendD3D;

  switch (eTFDst)
  {
    case eTF_A8R8G8B8:
      if (rd->m_FormatA8R8G8B8.BitsPerPixel)
      {
        m_pPixelFormat = &rd->m_FormatA8R8G8B8;
        return eTF_A8R8G8B8;
      }
      if (rd->m_FormatA4R4G4B4.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA4R4G4B4;
        return eTF_A4R4G4B4;
      }
      return eTF_Unknown;

    case eTF_X8R8G8B8:
      if (rd->m_FormatX8R8G8B8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatX8R8G8B8;
        return eTF_X8R8G8B8;
      }
      if (rd->m_FormatR5G6B5.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatR5G6B5;
        return eTF_A8R8G8B8;
      }
      return eTF_Unknown;

    case eTF_A4R4G4B4:
      if (rd->m_FormatA4R4G4B4.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA4R4G4B4;
        return eTF_A4R4G4B4;
      }
      if (rd->m_FormatA8R8G8B8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA8R8G8B8;
        return eTF_A8R8G8B8;
      }
      return eTF_Unknown;

    case eTF_R5G6B5:
      if (rd->m_FormatR5G6B5.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatR5G6B5;
        return eTF_R5G6B5;
      }
      if (rd->m_FormatX8R8G8B8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatX8R8G8B8;
        return eTF_X8R8G8B8;
      }
      return eTF_Unknown;

    case eTF_A8:
      if (rd->m_FormatA8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA8;
        return eTF_A8;
      }
      if (rd->m_FormatA8L8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA8L8;
        return eTF_A8L8;
      }
      return eTF_Unknown;

    case eTF_L8:
      if (rd->m_FormatL8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatL8;
        return eTF_L8;
      }
      if (rd->m_FormatA8L8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA8L8;
        return eTF_A8L8;
      }
      return eTF_Unknown;

    case eTF_A8L8:
      if (rd->m_FormatA8L8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA8L8;
        return eTF_A8L8;
      }
      return eTF_Unknown;

    case eTF_DXT1:
      if (rd->m_FormatDXT1.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatDXT1;
        return eTF_DXT1;
      }
      if (rd->m_FormatX8R8G8B8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatX8R8G8B8;
        return eTF_X8R8G8B8;
      }
      if (rd->m_FormatR5G6B5.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatR5G6B5;
        return eTF_A8R8G8B8;
      }
      return eTF_Unknown;

    case eTF_DXT3:
      if (rd->m_FormatDXT3.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatDXT3;
        return eTF_DXT3;
      }
      if (rd->m_FormatA8R8G8B8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA8R8G8B8;
        return eTF_A8R8G8B8;
      }
      if (rd->m_FormatA4R4G4B4.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA4R4G4B4;
        return eTF_A4R4G4B4;
      }
      return eTF_Unknown;

    case eTF_DXT5:
      if (rd->m_FormatDXT5.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatDXT5;
        return eTF_DXT5;
      }
      if (rd->m_FormatA8R8G8B8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA8R8G8B8;
        return eTF_A8R8G8B8;
      }
      if (rd->m_FormatA4R4G4B4.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA4R4G4B4;
        return eTF_A4R4G4B4;
      }
      return eTF_Unknown;

    case eTF_3DC:
			if (rd->DoCompressedNormalmapEmulation())
			{
				return eTF_Unknown;																// compressed normal map xy stored in ba, z reconstructed in shader 
			}
			else if (rd->m_Format3Dc.IsValid())
			{
        m_pPixelFormat = &rd->m_Format3Dc;
        return eTF_3DC;
      }
      return eTF_Unknown;

    case eTF_A16B16G16R16F:
      if (rd->m_FormatA16B16G16R16F.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA16B16G16R16F;
        return eTF_A16B16G16R16F;
      }
      return eTF_Unknown;

    case eTF_G16R16:
      if (rd->m_FormatG16R16.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatG16R16;
        return eTF_G16R16;
      }
      return eTF_Unknown;

    case eTF_G16R16F:
      if (rd->m_FormatG16R16F.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatG16R16F;
        return eTF_G16R16F;
      }
      return eTF_Unknown;

    case eTF_A32B32G32R32F:
      if (rd->m_FormatA32B32G32R32F.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatA32B32G32R32F;
        return eTF_A32B32G32R32F;
      }
      return eTF_Unknown;

    case eTF_R32F:
      if (rd->m_FormatR32F.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatR32F;
        return eTF_R32F;
      }
      if (rd->m_FormatG16R16F.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatG16R16F;
        return eTF_G16R16F;
      }
      return eTF_Unknown;

    case eTF_R16F:
      if (rd->m_FormatR16F.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatR16F;
        return eTF_R16F;
      }
      if (rd->m_FormatG16R16F.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatG16R16F;
        return eTF_G16R16F;
      }
      return eTF_Unknown;

    case eTF_DEPTH24:
      if (rd->m_FormatDepth24.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatDepth24;
        return eTF_DEPTH24;
      }
      if (rd->m_FormatDepth16.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatDepth16;
        return eTF_DEPTH16;
      }
      return eTF_Unknown;

    case eTF_DEPTH16:
      if (rd->m_FormatDepth16.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatDepth16;
        return eTF_DEPTH16;
      }
      if (rd->m_FormatDepth24.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatDepth24;
        return eTF_DEPTH24;
      }
      return eTF_Unknown;

		case eTF_DF16:
			if (rd->m_FormatDF16.IsValid())
			{
				m_pPixelFormat = &rd->m_FormatDF16;
				return eTF_DF16;
			}
			return eTF_Unknown;
		case eTF_DF24:
			if (rd->m_FormatDF24.IsValid())
			{
				m_pPixelFormat = &rd->m_FormatDF24;
				return eTF_DF24;
			}
		case eTF_D16:
			if (rd->m_FormatD16.IsValid())
			{
				m_pPixelFormat = &rd->m_FormatD16;
				return eTF_D16;
			}
    case eTF_D24S8:
      if (rd->m_FormatD24S8.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatD24S8;
        return eTF_D24S8;
      }
    case eTF_D32F:
      if (rd->m_FormatD32F.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatD32F;
        return eTF_D32F;
      }
    case eTF_NULL:
      if (rd->m_FormatNULL.IsValid())
      {
        m_pPixelFormat = &rd->m_FormatNULL;
        return eTF_NULL;
      }
			return eTF_Unknown;

    default:
      assert(0);
  }
  return eTF_Unknown;
}

bool CTexture::CreateRenderTarget(ETEX_Format eTF)
{
  if (eTF == eTF_Unknown)
    eTF = m_eTFDst;
  if (eTF == eTF_Unknown)
    return false;
  byte *pData[6];
  for (int i=0; i<6; i++)
  {
    pData[i] = NULL;
  }
  ETEX_Format eTFDst = ClosestFormatSupported(eTF);
  if (eTF != eTFDst)
    return false;
  m_eTFDst = eTF;
  m_nFlags |= FT_USAGE_RENDERTARGET;
  bool bRes = CreateDeviceTexture(pData);
  PostCreate();

  return bRes;
}

// DXT compressed block for black image (DXT1, DXT3, DXT5)
static byte sDXTBlackBlock[3][16] = 
{
  {0,0,0,0,0,0,0,0},
  {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, 0,0,0,0,0,0,0,0},
  {0xff,0xff,0,0,0,0,0,0, 0,0,0,0,0,0,0,0}
};

// Resolve anti-aliased RT to the texture
bool CTexture::Resolve()
{
  if (m_bResolved)
    return true;
  m_bResolved = true;

#if !defined(XENON) && !defined(PS3)
  if (!(m_nFlags & FT_USAGE_FSAA))
    return true;
#endif

#ifdef XENON
  assert ((m_nFlags & FT_USAGE_RENDERTARGET) && m_pDeviceTexture);
  D3DTexture *pDestTex = (D3DTexture *)GetDeviceTexture();
  if (CD3D9Renderer::CV_d3d9_predicatedtiling && (m_nFlags & FT_USAGE_PREDICATED_TILING))
  {
    gcpRendD3D->EndPredicatedTiling(pDestTex);
  }
  else
  {
    gcpRendD3D->m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0+0, NULL, pDestTex, NULL, 0, 0, NULL, 1.0f, 0L, NULL);
  }
#elif defined (DIRECT3D9) || defined (OPENGL)
  assert ((m_nFlags & FT_USAGE_RENDERTARGET) && (m_nFlags & FT_USAGE_FSAA) && m_pDeviceRTV && m_pDeviceTexture);
  LPDIRECT3DSURFACE9 pDestSurf = (LPDIRECT3DSURFACE9)GetSurface(-1, 0);
  LPDIRECT3DSURFACE9 pSrcSurf = (LPDIRECT3DSURFACE9)m_pDeviceRTV;

  RECT pSrcRect={0, 0, m_nWidth, m_nHeight};
  RECT pDstRect={0, 0, m_nWidth, m_nHeight};

  gcpRendD3D->m_RP.m_PS.m_RTCopiedFSAA++;
  gcpRendD3D->m_RP.m_PS.m_RTCopiedSize += GetDeviceDataSize();
  gcpRendD3D->m_pd3dDevice->StretchRect(pSrcSurf, &pSrcRect, pDestSurf, &pDstRect, D3DTEXF_NONE); 
  SAFE_RELEASE(pDestSurf);
#elif defined (DIRECT3D10)
  assert ((m_nFlags & FT_USAGE_RENDERTARGET) && (m_nFlags & FT_USAGE_FSAA) && m_pDeviceShaderResource && m_pDeviceTexture && m_pDeviceTextureMSAA && m_pDeviceShaderResourceMS);
  D3DTexture *pDestSurf = (D3DTexture *)GetDeviceTexture();
  D3DTexture *pSrcSurf =  (D3DTexture *)GetDeviceTextureMSAA();

  assert(pSrcSurf!=NULL);
  assert(pDestSurf!=NULL);

  gcpRendD3D->m_RP.m_PS.m_RTCopiedFSAA++;
  gcpRendD3D->m_RP.m_PS.m_RTCopiedSize += GetDeviceDataSize();
  gcpRendD3D->m_pd3dDevice->ResolveSubresource(pDestSurf, 0, pSrcSurf, 0, (DXGI_FORMAT)m_pPixelFormat->DeviceFormat); 
#endif
  return true;
}


bool CTexture::CreateDeviceTexture(byte *pData[6])
{
  HRESULT hr;
  if (!m_pPixelFormat)
  {
    ETEX_Format eTF = ClosestFormatSupported(m_eTFDst);

#if defined(_XBOX) || defined(PS3) || defined(LINUX)
	if (eTF == eTF_Unknown)
		return false;
#endif

    assert(eTF != eTF_Unknown);
    assert(eTF == m_eTFDst);
  }
  assert(m_pPixelFormat);
	if (!m_pPixelFormat)
		return false;

  ReleaseDeviceTexture(false);

  CD3D9Renderer *r = gcpRendD3D;
  int i;
  int nWdt = m_nWidth;
  int nHgt = m_nHeight;
  int nDepth = m_nDepth;
  int nMips = m_nMips;
  int nSize;

  byte *pTemp = NULL;

#if defined (DIRECT3D9) || defined (OPENGL)
  LPDIRECT3DDEVICE9 dv = r->GetD3DDevice();
  int D3DUsage = 0;
  D3DPOOL D3DPool = TEXPOOL;
  D3DLOCKED_RECT d3dlr;

  if (m_nFlags & FT_USAGE_RENDERTARGET)
  {
#if !defined(XENON) && !defined(PS3)
    D3DUsage |= D3DUSAGE_RENDERTARGET;
#endif
    D3DPool = D3DPOOL_DEFAULT;
  }
  if (m_nFlags & FT_USAGE_DYNAMIC)
  {
    D3DUsage |= D3DUSAGE_DYNAMIC;
		D3DPool = D3DPOOL_DEFAULT;
  }
  if (m_eTFDst == eTF_DEPTH16 || m_eTFDst == eTF_DEPTH24)
  {
    D3DUsage |= D3DUSAGE_DEPTHSTENCIL;
    D3DUsage &= ~D3DUSAGE_RENDERTARGET;
    D3DPool = D3DPOOL_DEFAULT;
  }
  if (nMips <= 1 && (m_nFlags & FT_FORCE_MIPS))
  {
    if (m_pPixelFormat->bCanAutoGenMips)
    {
      D3DUsage |= D3DUSAGE_AUTOGENMIPMAP;
      nMips = 1;
    }
    else
    if (pData[0])
    {
      LPDIRECT3DTEXTURE9 pID3DSrcTexture = NULL;
      if(FAILED(hr = D3DXCreateTexture(gcpRendD3D->GetDevice(), nWdt, nHgt, D3DX_DEFAULT, 0, (D3DFORMAT)m_pPixelFormat->DeviceFormat, D3DPOOL_SYSTEMMEM, &pID3DSrcTexture)))
      {
        m_nFlags &= ~FT_FORCE_MIPS;
        nMips = 1;
        assert(0);
      }
      else
      {
        int nSize = TextureDataSize(nWdt, nHgt, 1, 1, m_eTFDst);
        hr = pID3DSrcTexture->LockRect(0, &d3dlr, NULL, 0);
        // Copy data to src texture
        memcpy((byte *)d3dlr.pBits, pData[0], nSize);
        // Unlock the system texture
        pID3DSrcTexture->UnlockRect(0);
        hr = D3DXFilterTexture(pID3DSrcTexture, NULL, 0, D3DX_FILTER_LINEAR);
        nMips = CalcNumMips(nWdt, nHgt);
        nSize = TextureDataSize(nWdt, nHgt, 1, nMips, m_eTFDst);
        pTemp = new byte[nSize];
        int n = pID3DSrcTexture->GetLevelCount();
        int w = nWdt;
        int h = nHgt;
        int nOffs = 0;
        i = 0;
        while (w || h)
        {
          if (!w)
            w = 1;
          if (!h)
            h = 1;
          nSize = TextureDataSize(w, h, 1, 1, m_eTFDst);
          hr = pID3DSrcTexture->LockRect(i, &d3dlr, NULL, 0);
          // Copy data to src texture
          cryMemcpy(&pTemp[nOffs], (byte *)d3dlr.pBits, nSize);
          // Unlock the system texture
          hr = pID3DSrcTexture->UnlockRect(i);
          w >>= 1;
          h >>= 1;
          nOffs  += nSize;
          i++;
        }
        assert(i == nMips);
        m_nMips = nMips;
        pData[0] = pTemp;
        SAFE_RELEASE(pID3DSrcTexture);
      }
    }
  }

  LPDIRECT3DTEXTURE9 pID3DTexture = NULL;
  LPDIRECT3DCUBETEXTURE9 pID3DCubeTexture = NULL;
  LPDIRECT3DVOLUMETEXTURE9 pID3DVolTexture = NULL;
  LPDIRECT3DSURFACE9 pSurf = NULL;

  D3DTEXTUREFILTERTYPE MipFilter = D3DTEXF_LINEAR;
  D3DFORMAT D3DFormat = (D3DFORMAT)m_pPixelFormat->DeviceFormat;

  // Create the video texture
  if (m_eTT == eTT_2D)
  {
		if (m_eTFDst == eTF_DF16 || m_eTFDst == eTF_DF24 || m_eTFDst == eTF_D16 || m_eTFDst == eTF_D24S8)
		{
      //FIX: should pool be changed to the D3DPool
			if( FAILED( hr=dv->CreateTexture (nWdt, nHgt, 1, D3DUSAGE_DEPTHSTENCIL, D3DFormat, D3DPOOL_DEFAULT, (D3DTexture**)&pID3DTexture, NULL ) ) )
			{
        iLog->Log("Error: CTexture::CreateDeviceTexture: failed to create the texture %s (%s)", GetSourceName(), r->D3DError(hr));
				return false;
			}
		}
		else
    //if (!(m_eTFDst == eTF_3DC) || !r->DoCompressedNormalmapEmulation())
    if (m_eTFDst != eTF_3DC)
    {
      if (FAILED(hr=dv->CreateTexture(nWdt, nHgt, nMips, D3DUsage, D3DFormat, D3DPool, (D3DTexture**)&pID3DTexture, NULL)))
      {
        iLog->Log("Error: CTexture::CreateDeviceTexture: failed to create the texture %s (%s)", GetSourceName(), r->D3DError(hr));
      //  assert(0);
        return false;
      }
    }
    else
    if (m_eTFDst == eTF_3DC)
    {
#if !defined(XENON) && !defined(PS3)
      int nM = 0;
      int w = nWdt;
      int h = nHgt;
      if (w < 4 || h < 4)
      {
        iLog->Log("Error: 3DC texture '%s' couldn't be loaded due to small size (%dx%d) - skipped", GetName(), w, h);
        return false;
      }
      while (w && h)
      {
        if (!w)
          w = 1;
        if (!h)
          h = 1;
        if (w >= 4 && h >= 4)
          nM++;
        w >>= 1;
        h >>= 1;
      }
			nMips = min(nM, nMips);
#endif
      if (FAILED(hr=dv->CreateTexture(nWdt, nHgt, nMips, D3DUsage, D3DFormat, D3DPool, (D3DTexture**)&pID3DTexture, NULL)))
      {
        iLog->Log("Error: CD3D9TexMan::D3DCreateVideoTexture: failed to create the texture %s (%s)", GetSourceName(), r->D3DError(hr));
        return false;
      }
    }
    if (D3DUsage & D3DUSAGE_AUTOGENMIPMAP)
    {
      hr = pID3DTexture->SetAutoGenFilterType(MipFilter);
    }
    m_pDeviceTexture = pID3DTexture;
  }
  else
  if (m_eTT == eTT_Cube)
  {
    if (!(r->m_pd3dCaps->TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP))
    {
      nMips = 1;
      m_nMips = 1;
    }
    if( FAILED(hr = D3DXCreateCubeTexture(gcpRendD3D->GetDevice(), nWdt, nMips, D3DUsage, (D3DFORMAT)m_pPixelFormat->DeviceFormat, D3DPool, &pID3DCubeTexture)))
      return false;
    m_pDeviceTexture = pID3DCubeTexture;
    if (D3DUsage & D3DUSAGE_AUTOGENMIPMAP)
      hr = pID3DCubeTexture->SetAutoGenFilterType(MipFilter);
  }
  else
  if (m_eTT == eTT_3D)
  {
    if (!(r->m_pd3dCaps->TextureCaps & D3DPTEXTURECAPS_MIPVOLUMEMAP))
    {
      nMips = 1;
      m_nMips = 1;
    }
    if( FAILED(hr=D3DXCreateVolumeTexture(gcpRendD3D->GetDevice(), nWdt, nHgt, nDepth, nMips, D3DUsage, D3DFormat, D3DPool, &pID3DVolTexture)))
      return false;
    m_pDeviceTexture = pID3DVolTexture;
    if (D3DUsage & D3DUSAGE_AUTOGENMIPMAP)
      hr = pID3DVolTexture->SetAutoGenFilterType(MipFilter);
  }
  if (m_nFlags & FT_USAGE_FSAA)
  {
    assert(m_nFlags & FT_USAGE_RENDERTARGET);
    LPDIRECT3DSURFACE9 pSurf = NULL;
    hr = gcpRendD3D->GetDevice()->CreateRenderTarget(nWdt, nHgt, D3DFormat, r->m_RP.m_FSAAData.Type, r->m_RP.m_FSAAData.Quality, FALSE, &pSurf, NULL);
    if (FAILED(hr))
    {
      assert(0);
      iLog->LogError("Error: Cannot create MSAA render-target (MSAA switched off).");
      _SetVar("r_FSAA", 0);
    }
    m_nFSAASamples = r->m_RP.m_FSAAData.Type;
    m_nFSAAQuality = r->m_RP.m_FSAAData.Quality;
    m_pDeviceRTV = pSurf;
    m_bResolved = false;
  }
#elif defined (DIRECT3D10)
  ID3D10Device *dv = r->GetD3DDevice();

  ID3D10Texture2D *pID3DTexture = NULL;
  ID3D10Texture3D *pID3DTexture3D = NULL;
  ID3D10Texture2D *pID3DTextureCube = NULL;
  if (m_eTT == eTT_2D)
  {
    //nMips = 1;
    D3D10_TEXTURE2D_DESC Desc;
    ZeroStruct(Desc);
    Desc.Width = nWdt;
    Desc.Height = nHgt;
    Desc.MipLevels = nMips;
    Desc.Format = (DXGI_FORMAT)m_pPixelFormat->DeviceFormat;
    Desc.ArraySize = m_nArraySize;
    assert(m_nArraySize<=4);

    if (m_nFlags & FT_USAGE_DEPTHSTENCIL)
    {
      Desc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    }
    else
    {
      Desc.BindFlags = (m_nFlags & FT_USAGE_RENDERTARGET) ? D3D10_BIND_RENDER_TARGET : 0;
    }

    Desc.BindFlags |= D3D10_BIND_SHADER_RESOURCE;
		Desc.CPUAccessFlags = (m_nFlags & FT_USAGE_DYNAMIC) ? D3D10_CPU_ACCESS_WRITE : 0;
    Desc.Usage = (m_nFlags & FT_USAGE_DYNAMIC) ? D3D10_USAGE_DYNAMIC : D3D10_USAGE_DEFAULT;
    Desc.SampleDesc.Count = (m_nFlags & FT_USAGE_FSAA) ? r->m_RP.m_FSAAData.Type : 1;
    Desc.SampleDesc.Quality = (m_nFlags & FT_USAGE_FSAA) ? r->m_RP.m_FSAAData.Quality : 0;
    Desc.MiscFlags = 0; //(m_nFlags & FT_TEX_FONT) ? D3D10_RESOURCE_MISC_COPY_DESTINATION : 0;

    if (m_nFlags & FT_USAGE_FSAA)
    {
      hr = dv->CreateTexture2D(&Desc, NULL, (ID3D10Texture2D **)&m_pDeviceTextureMSAA);
      assert(SUCCEEDED(hr));
      m_bResolved = false;

      m_nFSAASamples = r->m_RP.m_FSAAData.Type;
      m_nFSAAQuality = r->m_RP.m_FSAAData.Quality;
      Desc.SampleDesc.Count = 1;
      Desc.SampleDesc.Quality = 0;
    }


    if (pData[0])
    {
      assert(Desc.ArraySize==1); //there is no implementation for tex array data

      //if (!IsDXTCompressed(m_eTFSrc))
      {
        D3D10_SUBRESOURCE_DATA InitData[20];
        int w = nWdt;
        int h = nHgt;
        int nOffset = 0;
        byte *src = pData[0];
        for (i=0; i<nMips; i++)
        {
          if (!w && !h)
            break;
          if (!w) w = 1;
          if (!h) h = 1;

          nSize = TextureDataSize(w, h, 1, 1, m_eTFSrc);
          InitData[i].pSysMem = &src[nOffset];
          if (!IsDXTCompressed(m_eTFSrc))
            InitData[i].SysMemPitch = TextureDataSize(w, 1, 1, 1, m_eTFSrc);
          else
          {
            int blockSize = (m_eTFSrc == eTF_DXT1) ? 8 : 16;
            InitData[i].SysMemPitch = (w + 3) / 4 * blockSize;
          }

          //ignored
          InitData[i].SysMemSlicePitch = nSize;

          w >>= 1;
          h >>= 1;
          nOffset += nSize;
        }
        hr = dv->CreateTexture2D(&Desc, &InitData[0], &pID3DTexture);
      }
      /*else
      {
        // HACK: Uploading of DXT textures works only in this way
        nSize = 0;
        byte *pBuf = WriteDDS(pData[0], nWdt, nHgt, nDepth, NULL, m_eTFSrc, nMips, eTT_2D, true, &nSize);
        D3DX10_IMAGE_LOAD_INFO IL;
        ZeroStruct(IL);
        IL.BindFlags = Desc.BindFlags;
        IL.CpuAccessFlags = Desc.CPUAccessFlags;
        IL.Depth = D3DX_FROM_FILE;
        IL.Filter = D3DX10_FILTER_NONE;
        IL.MipFilter = D3DX10_FILTER_NONE;
        IL.MipLevels = nMips;
        IL.Format = Desc.Format;
        IL.Width = Desc.Width;
        IL.Height = Desc.Height;
        IL.MiscFlags = Desc.MiscFlags;
        IL.Usage = Desc.Usage;
        IL.FirstMipLevel = 0;

        ID3D10Resource *pRes;
        hr = D3DX10CreateTextureFromMemory(dv, pBuf, nSize, &IL, NULL, &pRes);
        if (SUCCEEDED(hr))
          pRes->QueryInterface(__uuidof( ID3D10Texture2D ), (LPVOID*)&pID3DTexture);
        else
        {
          assert(0);
        }
        SAFE_DELETE_ARRAY(pBuf);
      }*/

      /*static int sss;
      if (!sss)
      {
        sss = 1;
        ID3D10Resource *pRes;
        D3DX10CreateTextureFromFile(dv, "Game\\Textures\\Defaults\\replaceme.dds", NULL, NULL, &pRes);
        pRes->QueryInterface( __uuidof( ID3D10Texture2D ), (LPVOID*)&pID3DTexture );
        pID3DTexture->GetDesc(&Desc);
        nMips = Desc.MipLevels;
      }*/
    }
    else
		{
			//hr = dv->CreateTexture2D(&Desc, 0, 0); // validates parameters
			//assert(hr == S_FALSE);
			hr = dv->CreateTexture2D(&Desc, NULL, &pID3DTexture);
		}
    if (FAILED(hr))
    {
      assert(0);
      return false;
    }
    m_pDeviceTexture = pID3DTexture;

    //////////////////////////////////////////////////////////////////////////
    //ShaderResourceViews creation 
    ID3D10ShaderResourceView* pSRVMS = NULL;
    if (m_nFlags & FT_USAGE_FSAA)
    {
      D3D10_SHADER_RESOURCE_VIEW_DESC SRVDescMS;
      ZeroStruct(SRVDescMS);

      SRVDescMS.Format = (DXGI_FORMAT)ConvertToShaderResourceFmt( Desc.Format );
      SRVDescMS.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMS;
      hr = dv->CreateShaderResourceView((ID3D10Texture2D*)m_pDeviceTextureMSAA, &SRVDescMS, &pSRVMS);
      if (FAILED(hr))
      {
        assert(0);
        return false;
      }
    }
    m_pDeviceShaderResourceMS = pSRVMS;

    ID3D10ShaderResourceView *pSRV = NULL;
    ID3D10ShaderResourceView* ppSRVs[4];
    for( int i = 0; i < 4; ++i ) ppSRVs[i] = NULL;

    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroStruct(SRVDesc);
    SRVDesc.Format = (DXGI_FORMAT)ConvertToShaderResourceFmt( Desc.Format );

    if (m_nArraySize > 1)
    {
      SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
      SRVDesc.Texture2DArray.FirstArraySlice = 0;
      SRVDesc.Texture2DArray.ArraySize = m_nArraySize;
      SRVDesc.Texture2DArray.MipLevels = nMips;
      SRVDesc.Texture2DArray.MostDetailedMip = 0;

      // Create the multi-slice shader resource view
      hr = dv->CreateShaderResourceView(pID3DTexture, &SRVDesc, &pSRV);
      if (FAILED(hr))
      {
        assert(0);
        return false;
      }
      // Create the one-slice shader resource views
      SRVDesc.Texture2DArray.ArraySize = 1;
      for( int i = 0; i < m_nArraySize; ++i )
      {
        SRVDesc.Texture2DArray.FirstArraySlice = i;
        hr = dv->CreateShaderResourceView( pID3DTexture, &SRVDesc, &ppSRVs[i]);
        if (FAILED(hr))
        {
          assert(0);
          return false;
        }
      }

      //assign shader resource views
      m_pDeviceShaderResource = pSRV;
      for( int i = 0; i < m_nArraySize; ++i )
      {
        m_pShaderResourceViews[i] = ppSRVs[i];
      }
    }
    else
    {
      SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
      SRVDesc.Texture2D.MipLevels = nMips;
      SRVDesc.Texture2D.MostDetailedMip = 0;
      hr = dv->CreateShaderResourceView(pID3DTexture, &SRVDesc, &pSRV);
      if (FAILED(hr))
      {
        assert(0);
        return false;
      }
      //assign shader resource views
      m_pDeviceShaderResource = pSRV;
    }


    //////////////////////////////////////////////////////////////////////////
    //DepthStencilViews creation
    ID3D10DepthStencilView* pDSV = NULL;
    ID3D10DepthStencilView* ppDSVs[4];
    for( int i = 0; i < 4; ++i ) ppDSVs[i] = NULL;

    if (m_nFlags & FT_USAGE_DEPTHSTENCIL)
    {
      D3D10_DEPTH_STENCIL_VIEW_DESC DsvDesc;
      ZeroStruct(DsvDesc);
      DsvDesc.Format = (DXGI_FORMAT)ConvertToDepthStencilFmt( Desc.Format );

      if (m_nArraySize > 1)
      {
        DsvDesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
        DsvDesc.Texture2DArray.FirstArraySlice = 0;
        DsvDesc.Texture2DArray.ArraySize = m_nArraySize;
        DsvDesc.Texture2DArray.MipSlice = 0;

        //multi-faces render target view
        hr = dv->CreateDepthStencilView(pID3DTexture, &DsvDesc, &pDSV);
        if (FAILED(hr) )
        {
          assert(0);
          return false;
        }

        // Create the one-face render target views
        DsvDesc.Texture2DArray.ArraySize = 1;
        for( int i = 0; i < m_nArraySize; ++i )
        {
          DsvDesc.Texture2DArray.FirstArraySlice = i;
          hr = dv->CreateDepthStencilView( pID3DTexture, &DsvDesc, &ppDSVs[i] );
          if (FAILED(hr) )
          {
            assert(0);
            return false;
          }
        }

        //assign depth stencil views
        m_pDeviceDepthStencilSurf = pDSV;
        for( int i = 0; i < m_nArraySize; ++i )
        {
          m_pDeviceDepthStencilCMSurfs[i] = ppDSVs[i];
        }
      }
      else
      {
        DsvDesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
        DsvDesc.Texture2D.MipSlice = 0;
        hr = dv->CreateDepthStencilView(pID3DTexture, &DsvDesc, &pDSV);
        if (FAILED(hr) )
        {
          assert(0);
          return false;
        }
        m_pDeviceDepthStencilSurf = pDSV;
      }

    }

  }
  else if (m_eTT == eTT_Cube)
  {
    //nMips = 1;
    D3D10_TEXTURE2D_DESC Desc;
    ZeroStruct(Desc);
    Desc.Width = nWdt;
    Desc.Height = nHgt;
    Desc.MipLevels = nMips;
    Desc.Format = (DXGI_FORMAT)m_pPixelFormat->DeviceFormat;
    Desc.ArraySize = 6;
    Desc.SampleDesc.Count = 1;
    Desc.SampleDesc.Quality = 0;
    if (m_nFlags & FT_USAGE_DEPTHSTENCIL)
    {
      Desc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    }
    else
    {
      Desc.BindFlags = (m_nFlags & FT_USAGE_RENDERTARGET) ? D3D10_BIND_RENDER_TARGET : 0;
    }

    Desc.BindFlags |= D3D10_BIND_SHADER_RESOURCE;
    Desc.CPUAccessFlags = (m_nFlags & FT_USAGE_DYNAMIC) ? D3D10_CPU_ACCESS_WRITE : 0;
    Desc.Usage = (m_nFlags & FT_USAGE_DYNAMIC) ? D3D10_USAGE_DYNAMIC : D3D10_USAGE_DEFAULT;
    Desc.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE; //(m_nFlags & FT_TEX_FONT) ? D3D10_RESOURCE_MISC_COPY_DESTINATION : 0;

    if (pData[0])
    {
        D3D10_SUBRESOURCE_DATA InitData[6 * 15];
        bool bAllocInitData[6 * 15];
        memset(bAllocInitData, 0, 6 * 15);

        for (int nSide=0; nSide<6; nSide++)
        {
          int w = nWdt;
          int h = nHgt;
          int nOffset = 0;
          byte *src = pData[nSide];
          for (i=0; i<nMips; i++)
          {
            if (!w && !h)
              break;
            if (!w) w = 1;
            if (!h) h = 1;

            int nSubresInd = nSide * nMips + i;
            int nSize = TextureDataSize(w, h, 1, 1, m_eTFSrc);
            if (nSide && !src)
            {
              if ((m_nFlags & FT_FORCE_CUBEMAP) && !(m_nFlags & FT_REPLICATE_TO_ALL_SIDES))
              {
                InitData[nSubresInd].pSysMem = new byte[nSize];
                bAllocInitData[nSubresInd] = true;
                // Fill black image
                if (IsDXTCompressed(m_eTFDst))
                {
                  int nBlocks = ((w+3)/4)*((h+3)/4);
                  int blockSize = m_eTFDst == eTF_DXT1 ? 8 : 16;
                  int nOffsData = m_eTFDst - eTF_DXT1;
                  int nCur = 0;
                  byte *pDst = (byte *)InitData[nSubresInd].pSysMem;
                  for (int n=0; n<nBlocks; n++)
                  {
                    cryMemcpy(&pDst[nCur], sDXTBlackBlock[nOffsData], blockSize);
                    nCur += blockSize;
                  }
                }
                else
                {
                  memset((byte *)InitData[nSubresInd].pSysMem, 0, nSize);
                }
              }
              else
              {
                assert(0);
              }
            }
            else
              InitData[nSubresInd].pSysMem = &src[nOffset];

            if (!IsDXTCompressed(m_eTFSrc))
              InitData[nSubresInd].SysMemPitch = TextureDataSize(w, 1, 1, 1, m_eTFSrc);
            else
            {
              int blockSize = (m_eTFSrc == eTF_DXT1) ? 8 : 16;
              InitData[nSubresInd].SysMemPitch = (w + 3) / 4 * blockSize;
            }

            //ignored
            InitData[nSubresInd].SysMemSlicePitch = nSize;

            w >>= 1;
            h >>= 1;
            nOffset += nSize;
          }
        }

        hr = dv->CreateTexture2D(&Desc, InitData, &pID3DTexture);
        for (i=0; i<6*15; i++)
        {
          if (bAllocInitData[i])
          {
            SAFE_DELETE_VOID_ARRAY(InitData[i].pSysMem);
          }
        }
    }
    else
    {
      hr = dv->CreateTexture2D(&Desc, NULL, &pID3DTexture);
    }
    if (FAILED(hr))
    {
      assert(0);
      return false;
    }
    m_pDeviceTexture = pID3DTexture;

    ID3D10ShaderResourceView *pRes = NULL;
    D3D10_SHADER_RESOURCE_VIEW_DESC ResDesc;
    ZeroStruct(ResDesc);
    ResDesc.Format = (DXGI_FORMAT)ConvertToShaderResourceFmt( Desc.Format );
    ResDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
    ResDesc.TextureCube.MipLevels = nMips;
    ResDesc.TextureCube.MostDetailedMip = 0;
    hr = dv->CreateShaderResourceView(pID3DTexture, &ResDesc, &pRes);
    if (FAILED(hr))
    {
      assert(0);
      return false;
    }
    m_pDeviceShaderResource = pRes;

    //depth-stencil views for cubemaps
    ID3D10DepthStencilView* pDSV = NULL;
    ID3D10DepthStencilView* ppDSVs[6];

    for( int i = 0; i < 6; ++i ) ppDSVs[i] = NULL;

    if (m_nFlags & FT_USAGE_DEPTHSTENCIL)
    {
      D3D10_DEPTH_STENCIL_VIEW_DESC DsvDesc;
      ZeroStruct(DsvDesc);
      DsvDesc.Format = (DXGI_FORMAT)ConvertToDepthStencilFmt( Desc.Format );
      DsvDesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
      DsvDesc.Texture2DArray.FirstArraySlice = 0;
      DsvDesc.Texture2DArray.ArraySize = 6;
      DsvDesc.Texture2DArray.MipSlice = 0;

      //six-faces render target view
      hr = dv->CreateDepthStencilView(pID3DTexture, &DsvDesc, &pDSV);
      if (FAILED(hr) )
      {
        assert(0);
        return false;
      }

      // Create the one-face render target views
      DsvDesc.Texture2DArray.ArraySize = 1;
      DsvDesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
      for( int i = 0; i < 6; ++i )
      {
        DsvDesc.Texture2DArray.FirstArraySlice = i;
        hr = dv->CreateDepthStencilView( pID3DTexture, &DsvDesc, &ppDSVs[i] );
        if (FAILED(hr) )
        {
          assert(0);
          return false;
        }
      }

    }

    //assign depth stencil views
    m_pDeviceDepthStencilSurf = pDSV;

    for( int i = 0; i < 6; ++i )
    {
      m_pDeviceDepthStencilCMSurfs[i] = ppDSVs[i];
    }


  }
  else if( m_eTT == eTT_3D)
  {
    D3D10_TEXTURE3D_DESC Desc;
    ZeroStruct(Desc);
    Desc.Width = nWdt;
    Desc.Height = nHgt;
    Desc.Depth = nDepth;
    Desc.MipLevels = nMips;
    Desc.Format = (DXGI_FORMAT)m_pPixelFormat->DeviceFormat;
    Desc.BindFlags = (m_nFlags & FT_USAGE_RENDERTARGET) ? D3D10_BIND_RENDER_TARGET : 0;
    Desc.BindFlags |= D3D10_BIND_SHADER_RESOURCE;
    Desc.CPUAccessFlags = (m_nFlags & FT_USAGE_DYNAMIC) ? D3D10_CPU_ACCESS_WRITE : 0;
    Desc.Usage = (m_nFlags & FT_USAGE_DYNAMIC) ? D3D10_USAGE_DYNAMIC : D3D10_USAGE_DEFAULT;
    Desc.MiscFlags = 0; //(m_nFlags & FT_TEX_FONT) ? D3D10_RESOURCE_MISC_COPY_DESTINATION : 0;


    if(pData[0])
    {
      D3D10_SUBRESOURCE_DATA InitData[15];

      int w = nWdt;
      int h = nHgt;
      int d = nDepth;
      int nOffset = 0;

      byte *src = pData[0];
      for (i=0; i<nMips; i++)
      {
        if (!w && !h && !d)
          break;

        if (!w) w = 1;
        if (!h) h = 1;
        if (!d) d = 1;

        int nSubresInd = i;

        nSize = TextureDataSize(w, h, d, 1, m_eTFSrc);
        
        InitData[nSubresInd].pSysMem = &src[nOffset];
        if (!IsDXTCompressed(m_eTFSrc))
        {
          InitData[nSubresInd].SysMemPitch = TextureDataSize(w, 1, 1, 1, m_eTFSrc);
          InitData[nSubresInd].SysMemSlicePitch = TextureDataSize(w, h, 1, 1, m_eTFSrc);
        }
        else
        {
          int blockSize = (m_eTFSrc == eTF_DXT1) ? 8 : 16;
          InitData[nSubresInd].SysMemPitch = (w + 3) / 4 * blockSize;
          InitData[nSubresInd].SysMemSlicePitch = ((w+3)/4)*((h+3)/4)*blockSize;;
        }


        w >>= 1;
        h >>= 1;
        d >>= 1;
        nOffset += nSize;
      }

      hr = dv->CreateTexture3D(&Desc, InitData, &pID3DTexture3D);

    }
    else
    {
      hr = dv->CreateTexture3D(&Desc, NULL, &pID3DTexture3D);
    }

    if (FAILED(hr))
    {
      assert(0);
      return false;
    }
    m_pDeviceTexture = pID3DTexture3D;

    ID3D10ShaderResourceView *pRes = NULL;
    D3D10_SHADER_RESOURCE_VIEW_DESC ResDesc;
    ZeroStruct(ResDesc);
    ResDesc.Format = Desc.Format;
    ResDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE3D;
    ResDesc.Texture3D.MipLevels = nMips;
    ResDesc.Texture3D.MostDetailedMip = 0;
    hr = dv->CreateShaderResourceView(pID3DTexture3D, &ResDesc, &pRes);
    if (FAILED(hr))
    {
      assert(0);
      return false;
    }
    m_pDeviceShaderResource = pRes;

  }
  else 
  {
    assert(0);
    return false;
  }
#endif

  SetTexStates();

  assert(!IsStreamed());
  if (m_pDeviceTexture)
  {
    m_nDeviceSize = CTexture::TextureDataSize(nWdt, nHgt, nDepth, nMips, m_eTFDst);
    m_nDeviceSize *= m_nArraySize;
    if (m_nFlags & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET))
      CTexture::m_StatsCurDynamicTexMem += m_nDeviceSize;
    else
      CTexture::m_StatsCurManagedNonStreamedTexMem += m_nDeviceSize;
  }

  if (!pData[0])
    return true;

  int nOffset = 0;
#if defined (DIRECT3D9) || defined (OPENGL)
  if (m_eTT == eTT_3D)
  {
    // Fill the volume texture
    D3DLOCKED_BOX LockedBox;

    int w = nWdt;
    int h = nHgt;
    int d = nDepth;
    nOffset = 0;
    byte *src = pData[0];
    for (i=0; i<m_nMips; i++)
    {
      if (!w && !h && !d)
        break;
      hr = pID3DVolTexture->LockBox(i, &LockedBox, 0, 0);
      if (!w) w = 1;
      if (!h) h = 1;
      if (!d) d = 1;
      nSize = TextureDataSize(w, 1, 1, 1, m_eTFDst);
#ifdef XENON
      XGTEXTURE_DESC dstDesc;
      ZeroMemory(&dstDesc, sizeof(XGTEXTURE_DESC));
      XGGetTextureDesc((D3DBaseTexture*)pID3DVolTexture, i, &dstDesc);
      XGPOINT3D stPoint = { 0, 0, 0 };	
      D3DBOX stBox = { 0, 0, w, h, 0, d };
      XGTileVolume(LockedBox.pBits, dstDesc.WidthInBlocks, dstDesc.HeightInBlocks, dstDesc.DepthInBlocks, &stPoint, &src[nOffset], nSize, nSize*h, &stBox, dstDesc.BytesPerBlock);
      nOffset += nSize * h * d;
#else
      for (int r=0; r<d; r++)
      {
        byte* pSliceStart = (byte*)LockedBox.pBits;
        for (int t=0; t<h; t++)
        {
          memcpy((byte *)LockedBox.pBits, &src[nOffset], nSize);
          LockedBox.pBits = (BYTE*)LockedBox.pBits + LockedBox.RowPitch;
          nOffset += nSize;
        }
        LockedBox.pBits = pSliceStart + LockedBox.SlicePitch;
      }
#endif
      w >>= 1;
      h >>= 1;
      d >>= 1;

      pID3DVolTexture->UnlockBox(i);
    }
  }
  else
  if (m_eTT == eTT_Cube)
  {
#ifdef XENON
    XGTEXTURE_DESC dstDesc;
    ZeroMemory(&dstDesc, sizeof(XGTEXTURE_DESC));
    XGGetTextureDesc((D3DBaseTexture*)pID3DCubeTexture, 0, &dstDesc);
#endif
    for (int nSide=0; nSide<6; nSide++)
    {
      int w = nWdt;
      int h = nHgt;
      nOffset = 0;
      byte *src = pData[nSide];
      for (i=0; i<m_nMips; i++)
      {
        if (!w && !h)
          break;
        DWORD lockFlag = 0;
        if (!w) w = 1;
        if (!h) h = 1;
        if (nSide && !src)
        {
          if ((m_nFlags & FT_FORCE_CUBEMAP) && !(m_nFlags & FT_REPLICATE_TO_ALL_SIDES))
          {
            hr = pID3DCubeTexture->LockRect((D3DCUBEMAP_FACES)nSide, i, &d3dlr, 0, lockFlag);
            // Fill black image
            if (IsDXTCompressed(m_eTFDst))
            {
              int nBlocks = ((w+3)/4)*((h+3)/4);
              int blockSize = m_eTFDst == eTF_DXT1 ? 8 : 16;
              int nOffsData = m_eTFDst - eTF_DXT1;
              int nCur = 0;
              byte *pDst = (byte *)d3dlr.pBits;
              for (int n=0; n<nBlocks; n++)
              {
                cryMemcpy(&pDst[nCur], sDXTBlackBlock[nOffsData], blockSize);
                nCur += blockSize;
              }
            }
            else
            {
              nSize = TextureDataSize(w, h, 1, 1, m_eTFDst);
              memset((byte *)d3dlr.pBits, 0, nSize);
            }
            pID3DCubeTexture->UnlockRect((D3DCUBEMAP_FACES)nSide, i);
          }
          else
          {
            assert(0);
          }
        }
        else
        {
          hr = pID3DCubeTexture->LockRect((D3DCUBEMAP_FACES)nSide, i, &d3dlr, 0, lockFlag);
          if (!src)
          {
            if (m_nFlags & FT_REPLICATE_TO_ALL_SIDES)
              src = pData[0];
            assert(src);
          }
          nSize = TextureDataSize(w, h, 1, 1, m_eTFDst);
#ifdef XENON
          UINT uWidthInBlocks = d3dlr.Pitch / dstDesc.BytesPerBlock;
          UINT uHeightInBlocks = dstDesc.HeightInBlocks;
          POINT stPoint = { 0, 0 };	
          RECT stRect = { 0, 0, w, h };
          int nPitch;
          if (IsDXTCompressed(m_eTFSrc))
          {
            int blockSize = (m_eTFSrc == eTF_DXT1) ? 8 : 16;
            nPitch = (w+3)/4 * blockSize;
            stRect.right = stRect.right>=4 ? stRect.right/4 : 1;
            stRect.bottom = stRect.bottom>=4 ? stRect.bottom/4 : 1;
          }
          else
          {
            int size = TextureDataSize(w, h, 1, 1, m_eTFSrc);
            nPitch = size / h;
          }
          XGTileSurface(d3dlr.pBits, uWidthInBlocks, uHeightInBlocks, &stPoint, &src[nOffset], nPitch, &stRect, dstDesc.BytesPerBlock );
#else
          cryMemcpy((byte *)d3dlr.pBits, &src[nOffset], nSize);
#endif
          pID3DCubeTexture->UnlockRect((D3DCUBEMAP_FACES)nSide, i);
        }
        w >>= 1;
        h >>= 1;
        nOffset += nSize;
      }
    }
  }
  else
  if (m_eTT == eTT_2D)
  {
    int w = nWdt;
    int h = nHgt;
    nOffset = 0;
    byte *src = pData[0];
#ifdef XENON
    XGTEXTURE_DESC dstDesc;
    ZeroMemory(&dstDesc, sizeof(XGTEXTURE_DESC));

    for (i=0; i<nMips; i++)
    {
      if (!w && !h)
        break;
      if (!w) w = 1;
      if (!h) h = 1;
      nSize = TextureDataSize(w, h, 1, 1, m_eTFSrc);

      //if (!IsDXTCompressed(m_eTFDst))
      if (m_eTFDst != eTF_3DC)
      {
        D3DSurface *pSurf = NULL;
        pID3DTexture->GetSurfaceLevel(i, &pSurf);
        D3DFORMAT LinFormat = (D3DFORMAT)MAKELINFMT(D3DFormat);
        int nPitch;
        if (IsDXTCompressed(m_eTFSrc))
        {
          int blockSize = (m_eTFSrc == eTF_DXT1) ? 8 : 16;
          nPitch = (w+3)/4 * blockSize;
        }
        else
          nPitch = TextureDataSize(w, 1, 1, 1, m_eTFSrc);
        RECT stRect = { 0, 0, w, h };
        HRESULT h = D3DXLoadSurfaceFromMemory(pSurf, NULL, NULL, &src[nOffset], LinFormat, nPitch, NULL, &stRect, FALSE, 0, 0, D3DX_FILTER_NONE, 0);
        assert(SUCCEEDED(h));
        SAFE_RELEASE(pSurf);
      }
      else
      {
        DWORD lockFlag = 0;
        XGGetTextureDesc((D3DBaseTexture*)pID3DTexture, i, &dstDesc);
        hr = pID3DTexture->LockRect(i, &d3dlr, 0, lockFlag);
        UINT uWidthInBlocks;
        UINT uHeightInBlocks = dstDesc.HeightInBlocks;
        POINT stPoint = { 0, 0 };	
        RECT stRect = { 0, 0, w, h };
        int nPitch;
        if (IsDXTCompressed(m_eTFSrc))
        {
          uWidthInBlocks = d3dlr.Pitch / dstDesc.BytesPerBlock;
          int blockSize = (m_eTFSrc == eTF_DXT1) ? 8 : 16;
          nPitch = (w+3)/4 * blockSize;
          stRect.right = stRect.right>=4 ? stRect.right/4 : 1;
          stRect.bottom = stRect.bottom>=4 ? stRect.bottom/4 : 1;
        }
        else
        {
          uWidthInBlocks = d3dlr.Pitch;
          nPitch = TextureDataSize(w, 1, 1, 1, m_eTFSrc);
        }
        XGTileSurface(d3dlr.pBits, uWidthInBlocks, uHeightInBlocks, &stPoint, &src[nOffset], nPitch, &stRect, dstDesc.BytesPerBlock );

        pID3DTexture->UnlockRect(i);
      }
      w >>= 1;
      h >>= 1;
      nOffset += nSize;
    }
#else
    for (i=0; i<nMips; i++)
    {
      if (!w && !h)
        break;
      DWORD lockFlag = 0;
      if (!w) w = 1;
      if (!h) h = 1;
      nSize = TextureDataSize(w, h, 1, 1, m_eTFDst);
      bool bLoaded = false;
      hr = pID3DTexture->LockRect(i, &d3dlr, 0, lockFlag);
#ifdef _DEBUG
      if (w >= 4)
      {
        int nD3DSize;
        if (IsDXTCompressed(GetDstFormat()))
          nD3DSize = CTexture::TextureDataSize(w, h, 1, 1, GetDstFormat());
        else
          nD3DSize = d3dlr.Pitch * h;
        assert(nD3DSize == nSize);
      }
#endif
      if (w < 4 && !IsDXTCompressed(m_eTFDst))
      {
        int nD3DSize = d3dlr.Pitch * h;
        if (nD3DSize != nSize)
        {
          bLoaded = true;
          byte *pDst = (byte *)d3dlr.pBits;
          byte *pSrc = &src[nOffset];
          int nPitchSrc = CTexture::TextureDataSize(w, 1, 1, 1, m_eTFDst);
          for (int j=0; j<j; j++)
          {
            memcpy(pDst, pSrc, nPitchSrc);
            pSrc += nPitchSrc;
            pDst += d3dlr.Pitch;
          }
        }
      }
      if (!bLoaded)
      {
        cryMemcpy((byte *)d3dlr.pBits, &src[nOffset], nSize);
      }
      pID3DTexture->UnlockRect(i);
      w >>= 1;
      h >>= 1;
      nOffset += nSize;
    }
#endif
  }
#endif

  SAFE_DELETE_ARRAY(pTemp);

  return true;
}

void CTexture::ReleaseDeviceTexture(bool bKeepLastMips)
{
	PROFILE_FRAME(CTexture_ReleaseDeviceTexture);

  if (m_pDeviceTexture)
  {
    int nnn = 0;
  }
  if (!m_bNoTexture)
  {
    IDirect3DBaseTexture9 *pTex = (IDirect3DBaseTexture9*)m_pDeviceTexture;
    SAFE_RELEASE(pTex);

#if defined (DIRECT3D10)
    ID3D10Texture2D * pTexMSAA = (ID3D10Texture2D*)m_pDeviceTextureMSAA;
    SAFE_RELEASE( pTexMSAA );
    m_pDeviceTextureMSAA = NULL;

    ID3D10ShaderResourceView* pSRV = (ID3D10ShaderResourceView*)m_pDeviceShaderResource;
    SAFE_RELEASE( pSRV );
    m_pDeviceShaderResource = NULL;

    ID3D10ShaderResourceView* pSRVMS = (ID3D10ShaderResourceView*)m_pDeviceShaderResourceMS;
    SAFE_RELEASE( pSRVMS );
    m_pDeviceShaderResourceMS = NULL;

    ID3D10RenderTargetView* pRTV = (ID3D10RenderTargetView*)m_pDeviceRTV;
    SAFE_RELEASE( pRTV );
    m_pDeviceRTV = NULL;

#else
    LPDIRECT3DSURFACE9 pSurf = (LPDIRECT3DSURFACE9)m_pDeviceRTV;
    if (pSurf)
    {
      SAFE_RELEASE(pSurf);
      m_pDeviceRTV = NULL;
    }
#endif

#if defined (DIRECT3D10)
      ID3D10DepthStencilView* pDSV = (ID3D10DepthStencilView*)m_pDeviceDepthStencilSurf;
      SAFE_RELEASE( pDSV );
      m_pDeviceDepthStencilSurf = NULL;
      for( int i = 0; i < 6; ++i )
      {
        pDSV = (ID3D10DepthStencilView*)m_pDeviceDepthStencilCMSurfs[i];
        SAFE_RELEASE( pDSV );
        m_pDeviceDepthStencilCMSurfs[i] = NULL;
      }

      for( int i = 0; i < 4; ++i )
      {
        pSRV = (ID3D10ShaderResourceView*)m_pShaderResourceViews[i];
        SAFE_RELEASE( pSRV );
        m_pShaderResourceViews[i] = NULL;
      }
      
#endif

    m_pDeviceTexture = NULL;
    //IsHeapValid();
    if (IsStreamed())
      CTexture::m_StatsCurManagedStreamedTexMem -= m_nDeviceSize;
    else
    if (IsDynamic())
      CTexture::m_StatsCurDynamicTexMem -= m_nDeviceSize;
    else
      CTexture::m_StatsCurManagedNonStreamedTexMem -= m_nDeviceSize;
    int nSides = m_eTT == eTT_Cube ? 6 : 1;
    if (m_pFileTexMips)
    {
      if (bKeepLastMips)
      {
        for (int nS=0; nS<nSides; nS++)
        {
          for (int i=0; i<m_nMips; i++)
          {
            SMipData *mp = &m_pFileTexMips[i].m_Mips[nS];
            if (i >= m_nMips-4)
            {
              mp->m_bUploaded = false;
              mp->m_bLoading = false;
            }
            else
              mp->Free();
          }
        }
      }
      else
      {
        SAFE_DELETE_ARRAY(m_pFileTexMips);
        m_bStream = false;
        m_bStreamPrepared = false;
        m_bPartyallyLoaded = false;
        m_bWasUnloaded = false;
        m_bStreamFromDDS = false;
        m_bStreamingInProgress = false;
        m_bStreamRequested = false;
      }
    }
    Unlink();
    if (m_pPoolItem)
    {
      STexPoolItem *pPoolItem = m_pPoolItem;
      StreamRemoveFromPool();
      pPoolItem->Unlink();
      delete pPoolItem;
    }
    m_nDeviceSize = 0;
  }
  else
  {
    m_pDeviceTexture = NULL;
    m_pDeviceRTV = NULL;
#if defined (DIRECT3D10)
    m_pDeviceShaderResource = NULL;
#endif
  }
  m_bNoTexture = false;
  m_nMinMipSysUploaded = 100;
  m_nMinMipVidUploaded = 100;
  m_nFSAASamples = 0;
  m_nFSAAQuality = 0;
}

void CTexture::SetTexStates()
{
  m_sDefState = m_sGlobalDefState;
  if (m_nFlags & FT_FILTER_MASK)
  {
    int nFilter = m_nFlags & FT_FILTER_MASK;
    if (nFilter == FT_FILTER_POINT)
      SetFilter(FILTER_POINT);
    else
    if (nFilter == FT_FILTER_LINEAR)
      SetFilter(FILTER_LINEAR);
    else
    if (nFilter == FT_FILTER_BILINEAR)
      SetFilter(FILTER_BILINEAR);
    else
    if (nFilter == FT_FILTER_TRILINEAR)
      SetFilter(FILTER_TRILINEAR);
    else
    if (nFilter == FT_FILTER_ANISO2)
      SetFilter(FILTER_ANISO2X);
    else
    if (nFilter == FT_FILTER_ANISO4)
      SetFilter(FILTER_ANISO4X);
    else
    if (nFilter == FT_FILTER_ANISO8)
      SetFilter(FILTER_ANISO8X);
    else
    if (nFilter == FT_FILTER_ANISO16)
      SetFilter(FILTER_ANISO16X);
  }
  if (m_nFlags & FT_DONT_ANISO)
  {
    if (m_sDefState.m_nMinFilter == D3DTEXF_ANISOTROPIC)
      m_sDefState.m_nMinFilter = D3DTEXF_LINEAR;
    if (m_sDefState.m_nMagFilter == D3DTEXF_ANISOTROPIC)
      m_sDefState.m_nMagFilter = D3DTEXF_LINEAR;
    m_sDefState.m_nAnisotropy = 1;
  }
  if (m_nMips <= 1 && !(m_nFlags & FT_FORCE_MIPS))
    m_sDefState.m_nMipFilter = D3DTEXF_NONE;
  if (m_nFlags & FT_STATE_CLAMP)
    SetClampingMode(TADDR_CLAMP, TADDR_CLAMP, TADDR_CLAMP);
  m_nDefState = CTexture::GetTexState(m_sDefState);
}

static signed char sAddressMode(int nAddress)
{
  signed char nMode = -1;
  if (nAddress < 0)
    return nMode;
#if defined (DIRECT3D9) || defined (OPENGL)
  switch (nAddress)
  {
    case TADDR_WRAP:
      nMode = D3DTADDRESS_WRAP;
      break;
    case TADDR_CLAMP:
      nMode = D3DTADDRESS_CLAMP;
      break;
    case TADDR_BORDER:
      nMode = D3DTADDRESS_BORDER;
      break;
    case TADDR_MIRROR:
      nMode = D3DTADDRESS_MIRROR;
      break;
    default:
      assert(0);
      return D3DTADDRESS_WRAP;
  }
#elif defined (DIRECT3D10)
  switch (nAddress)
  {
    case TADDR_WRAP:
      nMode = D3D10_TEXTURE_ADDRESS_WRAP;
      break;
    case TADDR_CLAMP:
      nMode = D3D10_TEXTURE_ADDRESS_CLAMP;
      break;
    case TADDR_BORDER:
      nMode = D3D10_TEXTURE_ADDRESS_BORDER;
      break;
    case TADDR_MIRROR:
      nMode = D3D10_TEXTURE_ADDRESS_MIRROR;
      break;
    default:
      assert(0);
      return D3D10_TEXTURE_ADDRESS_WRAP;
  }
#endif
  return nMode;
}

void STexState::SetComparisonFilter(bool bEnable)
{
#if defined (DIRECT3D10)
  if (m_pDeviceState)
  {
    ID3D10SamplerState *pSamp = (ID3D10SamplerState *)m_pDeviceState;
    SAFE_RELEASE(pSamp);
    m_pDeviceState = NULL;
  }
#endif
  m_bComparison = bEnable;
}

bool STexState::SetClampMode(int nAddressU, int nAddressV, int nAddressW)
{

#if defined (DIRECT3D10)
  if (m_pDeviceState)
  {
    ID3D10SamplerState *pSamp = (ID3D10SamplerState *)m_pDeviceState;
    SAFE_RELEASE(pSamp);
    m_pDeviceState = NULL;
  }
#endif

  m_nAddressU = sAddressMode(nAddressU);
  m_nAddressV = sAddressMode(nAddressV);
  m_nAddressW = sAddressMode(nAddressW);
  if (m_nAddressU < 0)
    m_nAddressU = CTexture::m_TexStates[CTexture::m_nGlobalDefState].m_nAddressU;
  if (m_nAddressV < 0)
    m_nAddressV = CTexture::m_TexStates[CTexture::m_nGlobalDefState].m_nAddressV;
  if (m_nAddressW < 0)
    m_nAddressW = CTexture::m_TexStates[CTexture::m_nGlobalDefState].m_nAddressW;
  return true;
}

bool STexState::SetFilterMode(int nFilter)
{
  if (nFilter < 0)
  {
    STexState *pTS = &CTexture::m_TexStates[CTexture::m_nGlobalDefState];
    m_nMinFilter = pTS->m_nMinFilter;
    m_nMagFilter = pTS->m_nMagFilter;
    m_nMipFilter = pTS->m_nMipFilter;
    return true;
  }

#if defined (DIRECT3D9) || defined (OPENGL)
  switch(nFilter)
  {
    case FILTER_POINT:
    case FILTER_NONE:
      m_nMinFilter = D3DTEXF_POINT;
      m_nMagFilter = D3DTEXF_POINT;
      m_nMipFilter = D3DTEXF_NONE;
      break;
    case FILTER_LINEAR:
      m_nMinFilter = D3DTEXF_LINEAR;
      m_nMagFilter = D3DTEXF_LINEAR;
      m_nMipFilter = D3DTEXF_NONE;
      break;
    case FILTER_BILINEAR:
      m_nMinFilter = D3DTEXF_LINEAR;
      m_nMagFilter = D3DTEXF_LINEAR;
      m_nMipFilter = D3DTEXF_POINT;
      break;
    case FILTER_TRILINEAR:
      m_nMinFilter = D3DTEXF_LINEAR;
      m_nMagFilter = D3DTEXF_LINEAR;
      m_nMipFilter = D3DTEXF_LINEAR;
      break;
    case FILTER_ANISO2X:
    case FILTER_ANISO4X:
    case FILTER_ANISO8X:
    case FILTER_ANISO16X:
      if (gcpRendD3D->m_pd3dCaps->TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC)
        m_nMinFilter = D3DTEXF_ANISOTROPIC;
      else
        m_nMinFilter = D3DTEXF_LINEAR;
      if (gcpRendD3D->m_pd3dCaps->TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC)
        m_nMagFilter = D3DTEXF_ANISOTROPIC;
      else
        m_nMagFilter = D3DTEXF_LINEAR;
      m_nMipFilter = D3DTEXF_LINEAR;
      if (nFilter == FILTER_ANISO2X)
        m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 2);
      else
      if (nFilter == FILTER_ANISO4X)
        m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 4);
      else
      if (nFilter == FILTER_ANISO8X)
        m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 8);
      else
      if (nFilter == FILTER_ANISO16X)
        m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 16);
      break;
    default:
      assert(0);
      return false;
  }
#elif defined (DIRECT3D10)

  if (m_pDeviceState)
  {
    ID3D10SamplerState *pSamp = (ID3D10SamplerState *)m_pDeviceState;
    SAFE_RELEASE(pSamp);
    m_pDeviceState = NULL;
  }

  switch(nFilter)
  {
    case FILTER_POINT:
    case FILTER_NONE:
      m_nMinFilter = FILTER_POINT;
      m_nMagFilter = FILTER_POINT;
      m_nMipFilter = FILTER_NONE;
      break;
    case FILTER_LINEAR:
      m_nMinFilter = FILTER_LINEAR;
      m_nMagFilter = FILTER_LINEAR;
      m_nMipFilter = FILTER_NONE;
      break;
    case FILTER_BILINEAR:
      m_nMinFilter = FILTER_LINEAR;
      m_nMagFilter = FILTER_LINEAR;
      m_nMipFilter = FILTER_POINT;
      break;
    case FILTER_TRILINEAR:
      m_nMinFilter = FILTER_LINEAR;
      m_nMagFilter = FILTER_LINEAR;
      m_nMipFilter = FILTER_LINEAR;
      break;
    case FILTER_ANISO2X:
    case FILTER_ANISO4X:
    case FILTER_ANISO8X:
    case FILTER_ANISO16X:
      m_nMinFilter = nFilter;
      m_nMagFilter = nFilter;
      m_nMipFilter = nFilter;
			if (nFilter == FILTER_ANISO2X)
				m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 2);
			else
			if (nFilter == FILTER_ANISO4X)
				m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 4);
			else
			if (nFilter == FILTER_ANISO8X)
				m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 8);
			else
			if (nFilter == FILTER_ANISO16X)
				m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 16);
      break;
    default:
      assert(0);
      return false;
  }
#endif
  if (CRenderer::CV_r_texturesfilteringquality >= 2 && m_nAnisotropy > 2)
    m_nAnisotropy = 2;
  else
  if (CRenderer::CV_r_texturesfilteringquality == 1 && m_nAnisotropy > 4)
    m_nAnisotropy = 4;
  return true;
}

void STexState::SetBorderColor(DWORD dwColor) 
{ 
#if defined (DIRECT3D10)
  if (m_pDeviceState)
  {
    ID3D10SamplerState *pSamp = (ID3D10SamplerState *)m_pDeviceState;
    SAFE_RELEASE(pSamp);
    m_pDeviceState = NULL;
  }
#endif

  m_dwBorderColor = dwColor; 
}

void STexState::PostCreate()
{
#if defined (DIRECT3D10)
  if (m_pDeviceState)
    return;

  D3D10_SAMPLER_DESC Desc;
  ZeroStruct(Desc);
  ID3D10SamplerState *pSamp = NULL;
  Desc.AddressU = (D3D10_TEXTURE_ADDRESS_MODE)m_nAddressU;
  Desc.AddressV = (D3D10_TEXTURE_ADDRESS_MODE)m_nAddressV;
  Desc.AddressW = (D3D10_TEXTURE_ADDRESS_MODE)m_nAddressW;
  ColorF col((uint)m_dwBorderColor);
  Desc.BorderColor[0] = col.r;
  Desc.BorderColor[1] = col.g;
  Desc.BorderColor[2] = col.b;
  Desc.BorderColor[3] = col.a;
  if (m_bComparison)
    Desc.ComparisonFunc = D3D10_COMPARISON_LESS;
  else
    Desc.ComparisonFunc = D3D10_COMPARISON_ALWAYS;

  Desc.MaxAnisotropy = 1;
  Desc.MinLOD = 0;
  if (m_nMipFilter == FILTER_NONE)
  {
    Desc.MaxLOD = 0.0f;
  }
  else
  {
    Desc.MaxLOD = 100.0f;
  }
  Desc.MipLODBias = 0;

  if (m_bComparison)
  {

    if (m_nMinFilter == FILTER_LINEAR && m_nMagFilter == FILTER_LINEAR && m_nMipFilter == FILTER_LINEAR)
    {
      Desc.Filter = D3D10_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    }
    else
    if (m_nMinFilter == FILTER_LINEAR && m_nMagFilter == FILTER_LINEAR && m_nMipFilter == FILTER_NONE)
      Desc.Filter = D3D10_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    else
    if (m_nMinFilter == FILTER_LINEAR && m_nMagFilter == FILTER_LINEAR && m_nMipFilter == FILTER_POINT)
      Desc.Filter = D3D10_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    else
    if (m_nMinFilter == FILTER_POINT && m_nMagFilter == FILTER_POINT && (m_nMipFilter == FILTER_NONE || m_nMipFilter == FILTER_POINT))
      Desc.Filter = D3D10_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
    else
    if (m_nMinFilter >= FILTER_ANISO2X && m_nMagFilter >= FILTER_ANISO2X && m_nMipFilter >= FILTER_ANISO2X)
    {
      Desc.Filter = D3D10_FILTER_COMPARISON_ANISOTROPIC;
      Desc.MaxAnisotropy = m_nAnisotropy;
    }
  }
  else
  {
    if (m_nMinFilter == FILTER_LINEAR && m_nMagFilter == FILTER_LINEAR && m_nMipFilter == FILTER_LINEAR)
    {
      Desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
    }
    else
    if (m_nMinFilter == FILTER_LINEAR && m_nMagFilter == FILTER_LINEAR && m_nMipFilter == FILTER_NONE)
      Desc.Filter = D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    else
    if (m_nMinFilter == FILTER_LINEAR && m_nMagFilter == FILTER_LINEAR && m_nMipFilter == FILTER_POINT)
      Desc.Filter = D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    else
    if (m_nMinFilter == FILTER_POINT && m_nMagFilter == FILTER_POINT && (m_nMipFilter == FILTER_NONE || m_nMipFilter == FILTER_POINT))
      Desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
    else
    if (m_nMinFilter >= FILTER_ANISO2X && m_nMagFilter >= FILTER_ANISO2X && m_nMipFilter >= FILTER_ANISO2X)
    {
      Desc.Filter = D3D10_FILTER_ANISOTROPIC;
      Desc.MaxAnisotropy = m_nAnisotropy;
    }
    else
      assert(0);
  }
  HRESULT hr = gcpRendD3D->m_pd3dDevice->CreateSamplerState(&Desc, &pSamp);
  if (SUCCEEDED(hr))
    m_pDeviceState = pSamp;
  else
    assert(0);
#endif
}

STexState::~STexState()
{
#if defined (DIRECT3D10)
  if (m_pDeviceState)
  {
    ID3D10SamplerState *pSamp = (ID3D10SamplerState *)m_pDeviceState;
    SAFE_RELEASE(pSamp);
    m_pDeviceState = NULL;
  }
#endif
}

STexState::STexState(const STexState& src)
{
  memcpy(this, &src, sizeof(src));
#if defined (DIRECT3D10)
  if (m_pDeviceState)
  {
    ID3D10SamplerState *pSamp = (ID3D10SamplerState *)m_pDeviceState;
    pSamp->AddRef();
  }
#endif
}

bool CTexture::SetClampingMode(int nAddressU, int nAddressV, int nAddressW)
{
  return m_sDefState.SetClampMode(nAddressU, nAddressV, nAddressW);
}

bool CTexture::SetFilterMode(int nFilter)
{
  return m_sDefState.SetFilterMode(nFilter);
}

void CTexture::UpdateTexStates() 
{ 
	m_nDefState = CTexture::GetTexState(m_sDefState); 
}

bool CTexture::SetDefTexFilter(const char *szFilter)
{
  uint i;
  bool bRes = false;
  struct textype
  {
    char *name;
    uint filter;
  };

  static textype tt[] =
  {
    {"NEAREST", FILTER_POINT},
    {"LINEAR", FILTER_LINEAR},
    {"BILINEAR", FILTER_BILINEAR},
    {"TRILINEAR", FILTER_TRILINEAR},
  };

  m_sGlobalDefState.m_nAnisotropy = CLAMP(CRenderer::CV_r_texture_anisotropic_level, 1, gcpRendD3D->m_MaxAnisotropyLevel);
  if (!(gcpRendD3D->GetFeatures() & RFT_ALLOWANISOTROPIC))
    m_sGlobalDefState.m_nAnisotropy = 1;
  _SetVar("r_Texture_Anisotropic_Level", m_sGlobalDefState.m_nAnisotropy);
  m_GlobalTextureFilter = szFilter;
  if ((gcpRendD3D->GetFeatures() & RFT_ALLOWANISOTROPIC) && m_sDefState.m_nAnisotropy > 1)
    szFilter = "TRILINEAR";

  m_sGlobalDefState.SetClampMode(TADDR_WRAP, TADDR_WRAP, TADDR_WRAP);

  if (m_sGlobalDefState.m_nAnisotropy > 1)
  {
    if (m_sGlobalDefState.m_nAnisotropy == 2)
      m_sGlobalDefState.SetFilterMode(FILTER_ANISO2X);
    else
    if (m_sGlobalDefState.m_nAnisotropy == 4)
      m_sGlobalDefState.SetFilterMode(FILTER_ANISO4X);
    else
    if (m_sGlobalDefState.m_nAnisotropy == 8)
      m_sGlobalDefState.SetFilterMode(FILTER_ANISO8X);
    else
    if (m_sGlobalDefState.m_nAnisotropy == 16)
      m_sGlobalDefState.SetFilterMode(FILTER_ANISO16X);
    else
    {
      assert(0);
      m_sGlobalDefState.m_nAnisotropy = 2;
      m_sGlobalDefState.SetFilterMode(FILTER_ANISO2X);
    }
    bRes = true;
  }
  else
  {
    for (i=0; i<4; i++)
    {
      if (!stricmp(szFilter, tt[i].name) )
      {
        m_sGlobalDefState.SetFilterMode(tt[i].filter);
        bRes = true;
        break;
      }
    }
  }
  m_nGlobalDefState = CTexture::GetTexState(m_sGlobalDefState);
  if (bRes)
  {
      CCryName Name = CTexture::mfGetClassName();
      SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
      if (pRL)
      {
        ResourcesMapItor itor;
        for (itor=pRL->m_RMap.begin(); itor!=pRL->m_RMap.end(); itor++)
        {
          CTexture *tx = (CTexture *)itor->second;
          if (!tx)
            continue;
          tx->SetTexStates();
        }
      }
      return true;
    }
  
  assert(0);
  Warning("Bad texture filter name <%s>\n", szFilter);
  return false;
}

void CTexture::SetSamplerState(int nTUnit, int nTS, int nSSlot, int nVtxTexOffSet)
{  
  STexStageInfo* TexStages = m_TexStages;
#if defined (DIRECT3D9) || defined (OPENGL)

  if (CRenderer::CV_r_texnoaniso)
  {
    STexState *pTS = &m_TexStates[nTS];
    if (pTS->m_nAnisotropy > 1)
    {
      STexState TS = *pTS;
      TS.SetFilterMode(FILTER_BILINEAR);
      TS.PostCreate();
      nTS = CTexture::GetTexState(TS);
    }
  }
  if (TexStages[nTUnit].m_nCurState != nTS)
  {
    TexStages[nTUnit].m_nCurState = nTS;
    LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
    STexState *pTS = &m_TexStates[nTS];
    STexState *pDTS = &TexStages[nTUnit].m_State;
    if(pTS->m_nMipFilter!=pDTS->m_nMipFilter)
    {
      pDTS->m_nMipFilter = pTS->m_nMipFilter;
      dv->SetSamplerState(nTUnit + nVtxTexOffSet, D3DSAMP_MIPFILTER, pTS->m_nMipFilter);            
    }
    if(pTS->m_nMinFilter!=pDTS->m_nMinFilter || pTS->m_nMagFilter!=pDTS->m_nMagFilter || pTS->m_nAnisotropy!=pDTS->m_nAnisotropy)
    {
      pDTS->m_nMinFilter = pTS->m_nMinFilter;
      pDTS->m_nMagFilter = pTS->m_nMagFilter;
      pDTS->m_nAnisotropy = pTS->m_nAnisotropy;
      dv->SetSamplerState(nTUnit + nVtxTexOffSet, D3DSAMP_MINFILTER, pTS->m_nMinFilter);
      dv->SetSamplerState(nTUnit + nVtxTexOffSet, D3DSAMP_MAGFILTER, pTS->m_nMagFilter);
      if (pTS->m_nMinFilter == D3DTEXF_ANISOTROPIC)
        dv->SetSamplerState(nTUnit + nVtxTexOffSet, D3DSAMP_MAXANISOTROPY, pTS->m_nAnisotropy);
    }
    if (pTS->m_nAddressU != pDTS->m_nAddressU)
    {
      pDTS->m_nAddressU = pTS->m_nAddressU;
      dv->SetSamplerState(nTUnit + nVtxTexOffSet, D3DSAMP_ADDRESSU, pTS->m_nAddressU);
    }
    if (pTS->m_nAddressV != pDTS->m_nAddressV)
    {
      pDTS->m_nAddressV = pTS->m_nAddressV;
      dv->SetSamplerState(nTUnit + nVtxTexOffSet, D3DSAMP_ADDRESSV, pTS->m_nAddressV);
    }
    if (pTS->m_dwBorderColor != pDTS->m_dwBorderColor)
    {
      pDTS->m_dwBorderColor = pTS->m_dwBorderColor;
      dv->SetSamplerState(nTUnit + nVtxTexOffSet, D3DSAMP_BORDERCOLOR, pTS->m_dwBorderColor);
    }
    if ((m_eTT == eTT_Cube || m_eTT == eTT_3D) && pTS->m_nAddressW != pDTS->m_nAddressW)
    {
      pDTS->m_nAddressW = pTS->m_nAddressW;
      dv->SetSamplerState(nTUnit + nVtxTexOffSet, D3DSAMP_ADDRESSW, pTS->m_nAddressW);
    }
    /*if (!CRenderer::CV_r_srgbtexture)
    bSRGB = false;
    if (bSRGB != pDTS->m_bSRGBLookup)
    {
      pDTS->m_bSRGBLookup = bSRGB;
      dv->SetSamplerState(nTUnit, D3DSAMP_SRGBTEXTURE, bSRGB);
    }*/
  }
#elif defined (DIRECT3D10)
//PS3 hack, setting samplers every frame for debugging reasons
#if !defined(PS3)
  if (m_TexStateIDs[nSSlot] != nTS)
#endif
  {
    m_TexStateIDs[nSSlot] = nTS;
    STexState *pTS = &m_TexStates[nTS];
    ID3D10SamplerState *pSamp = (ID3D10SamplerState *)pTS->m_pDeviceState;
    assert(pSamp);
    if (pSamp)
    {
       if(!m_bVertexTexture)
        gcpRendD3D->GetD3DDevice()->PSSetSamplers(nSSlot, 1, &pSamp);
       else
        gcpRendD3D->GetD3DDevice()->VSSetSamplers(nSSlot, 1, &pSamp);
    }
      
  }
#endif
}

void CTexture::Apply(int nTUnit, int nTS, int nTSlot, int nSSlot, int nResView)
{
  static int sRecursion = 0;
  if (!sRecursion)
  {
    /*if (this != m_Text_NoTexture)
    {
      m_Text_NoTexture->Apply(nTUnit, pTS, nTSlot, bSRGB);
      return;
    }*/
    int nT = nTSlot;
    if (nT < 0)
      nT = 0;
    if (CRenderer::CV_r_texgrid==nT+1 && !(m_nFlags & (FT_DONT_RELEASE | FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET | FT_TEX_FONT)) && m_eTT == eTT_2D)
    {
      sRecursion++;
      CTexture::SetGridTexture(nTUnit, this, nTSlot);
      sRecursion--;
      return;
    }
    else
    if (CRenderer::CV_r_texbindmode>=1)
    {
      if (CRenderer::CV_r_texbindmode==1 && !(m_nFlags & FT_TEX_FONT))
      {
        sRecursion++;
        CTexture::m_Text_NoTexture->Apply();
        sRecursion--;
        return;
      }
      if (CRenderer::CV_r_texbindmode==3 && (m_nFlags & FT_FROMIMAGE) && !(m_nFlags & FT_DONT_RELEASE))
      {
        sRecursion++;
        //if (m_Flags & FT_TEX_NORMAL_MAP)
        //  CTexture::SetGridTexture(this);
        //else
          CTexture::m_Text_Gray->Apply();
        sRecursion--;
        return;
      }
      if (CRenderer::CV_r_texbindmode==4 && (m_nFlags & FT_FROMIMAGE) && !(m_nFlags & FT_DONT_RELEASE) && m_eTT == eTT_2D)
      {
        sRecursion++;
        CTexture::m_Text_Gray->Apply();
        sRecursion--;
        return;
      }
      if (CRenderer::CV_r_texbindmode==5 && (m_nFlags & FT_FROMIMAGE) && !(m_nFlags & FT_DONT_RELEASE) && (m_nFlags & FT_TEX_NORMAL_MAP))
      {
        sRecursion++;
        CTexture::m_Text_FlatBump->Apply();
        sRecursion--;
        return;
      }
      if (CRenderer::CV_r_texbindmode==6 && nTSlot==EFTT_DIFFUSE)
      {
        sRecursion++;
        CTexture::m_Text_White->Apply();
        sRecursion--;
        return;
      }
    }
  }

  CD3D9Renderer *rd = gcpRendD3D;
  if (nTUnit < 0)
    nTUnit = m_CurStage;
  if (nSSlot < 0)
    nSSlot = nTUnit;
  assert(nTUnit >= 0 && nTUnit < rd->m_numtmus);

  if (m_bStream && CRenderer::CV_r_texturesstreamingignore == 0)
  {
    Relink(&CTexture::m_Root);
    if (m_bWasUnloaded || m_bPartyallyLoaded)
    {
      PROFILE_FRAME(Texture_StreamRestore);
      if (!m_nStreamed || !m_bStreamPrepared)
        Load(m_eTFDst, -1, -1);
      if (m_nStreamed && m_bStreamPrepared)
        StreamRestore();
    }
  }

  if (!m_pDeviceTexture)
  {
    assert(!"m_pDeviceTexture is NULL");
    return;
  }

  // Resolve multisampled RT to texture
  if (!m_bResolved)
    Resolve();

  int nFrameID = rd->m_nFrameUpdateID;
  if (m_nAccessFrameID != nFrameID)
  { 
    m_nAccessFrameID = nFrameID;
    rd->m_RP.m_PS.m_NumTextures++;
    if (m_nFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DYNAMIC))
      rd->m_RP.m_PS.m_DynTexturesSize += m_nDeviceSize;
    else
    {
      rd->m_RP.m_PS.m_ManagedTexturesSize += m_nDeviceSize;
      rd->m_RP.m_PS.m_ManagedTexturesFullSize += m_nSize;
    }
  }

  if (nTS < 0)
    nTS = m_nDefState;
  assert(nTS >= 0 && nTS < m_TexStates.size());

  int nVtxTexOffSet = 0;
#if defined (DIRECT3D9) || defined (OPENGL)  
  if (m_bVertexTexture)
    nVtxTexOffSet = D3DVERTEXTEXTURESAMPLER0;
#endif
  SetSamplerState(nTUnit, nTS, nSSlot, nVtxTexOffSet);
  STexStageInfo *TexStages = m_TexStages;

#if defined(DIRECT3D10)
	//PS3HACK force setting texture every call (for debugging)
 #if !defined(PS3)
  if (this == TexStages[nTUnit].m_Texture && nResView == TexStages[nTUnit].m_nCurResView)
    return;
 #endif
  TexStages[nTUnit].m_nCurResView = nResView;
  // in DX10 don't allow to set texture while it still bound as RT
  if (rd->m_pCurTarget[0] == this)
  {
    assert(rd->m_pCurTarget[0]->m_pDeviceRTV);
    rd->m_pNewTarget[0]->m_bWasSetRT = false;
    rd->m_pd3dDevice->OMSetRenderTargets(1, &rd->m_pNewTarget[0]->m_pTarget, rd->m_pNewTarget[0]->m_pDepth);
  }
#else
  if (this == TexStages[nTUnit].m_Texture )
    return;
#endif

  TexStages[nTUnit].m_Texture = this;
  rd->m_RP.m_PS.m_NumTextChanges++;

#ifdef DO_RENDERLOG
  if (CRenderer::CV_r_log >= 3)
  {
    if (IsNoTexture())
      rd->Logv(SRendItem::m_RecurseLevel, "CTexture::Apply(): (%d) \"%s\" (Not found)\n", nTUnit, m_SrcName.c_str());
    else
      rd->Logv(SRendItem::m_RecurseLevel, "CTexture::Apply(): (%d) \"%s\"\n", nTUnit, m_SrcName.c_str());
  }
#endif

#if defined (DIRECT3D9) || defined (OPENGL)  
  HRESULT hr = rd->GetD3DDevice()->SetTexture(nTUnit+nVtxTexOffSet, (IDirect3DBaseTexture9*)m_pDeviceTexture);
#elif defined (DIRECT3D10)
  if (m_pDeviceTexture)
  {
    ID3D10ShaderResourceView *pRes = NULL;
    if (nResView>=0)
    {
      assert(nResView<5);
      if (nResView==4) //multisample view
      {
        pRes = (ID3D10ShaderResourceView *)m_pDeviceShaderResourceMS;
      }
      else
      {
      pRes = (ID3D10ShaderResourceView *)m_pShaderResourceViews[nResView];
      }
    }
    else
    {
      pRes = (ID3D10ShaderResourceView *)m_pDeviceShaderResource;
    }

#ifdef DO_RENDERLOG
    if (CRenderer::CV_r_log >= 3 && nResView>=0)
    {
      rd->Logv(SRendItem::m_RecurseLevel, "CTexture::Apply(): Shader Resource View: %d \n", nResView);
    }
#endif

    //test
    //D3D10_SHADER_RESOURCE_VIEW_DESC Desc;
    //pRes->GetDesc( &Desc );

    assert(pRes);
    if ( pRes )
    {
      if( !m_bVertexTexture  )
        rd->GetD3DDevice()->PSSetShaderResources(nTUnit, 1, &pRes);
      else
        rd->GetD3DDevice()->VSSetShaderResources(nTUnit, 1, &pRes);
    }
  }
#endif
  m_pCurrent = this;
}

void CTexture::BindNULLFrom(int nFrom)
{
  int n = m_nCurStages;
  m_nCurStages = nFrom;
#if defined (DIRECT3D10) && !defined(PS3)
  ID3D10ShaderResourceView *RV[MAX_TMU];
  memset(&RV[0], 0, sizeof(RV));
  int nStages = MAX_TMU-nFrom;
  gcpRendD3D->GetD3DDevice()->PSSetShaderResources(nFrom, nStages, RV);
  for (int i=nFrom; i<nStages; i++)
  {
    m_TexStages[i].m_Texture = NULL;
  }
#else
  for (; nFrom<n; nFrom++)
  {
#if defined (DIRECT3D9) || defined(OPENGL)
    HRESULT hr = gcpRendD3D->GetD3DDevice()->SetTexture(nFrom, NULL);
#endif
    m_TexStages[nFrom].m_Texture = NULL;
  }
#endif
}

void CTexture::UpdateTextureRegion(byte *data, int nX, int nY, int USize, int VSize, ETEX_Format eTFSrc)
{
	PROFILE_FRAME(UpdateTextureRegion);

#if defined (DIRECT3D9) || defined(OPENGL)
  LPDIRECT3DTEXTURE9 pID3DTexture = NULL;
  LPDIRECT3DCUBETEXTURE9 pID3DCubeTexture = NULL;
  LPDIRECT3DVOLUMETEXTURE9 pID3DVolTexture = NULL;
  HRESULT h;
  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
  LPDIRECT3DSURFACE9 pDestSurf = NULL;
  if (m_eTT == eTT_2D)
  {
    pID3DTexture = (LPDIRECT3DTEXTURE9)GetDeviceTexture();
    assert(pID3DTexture);
  	if (!pID3DTexture)
	  	return;
  }
  else
  if (m_eTT == eTT_Cube)
  {
    pID3DCubeTexture = (LPDIRECT3DCUBETEXTURE9)GetDeviceTexture();
    assert(pID3DCubeTexture);
  }
  else
  if (m_eTT == eTT_3D)
  {
    pID3DVolTexture = (LPDIRECT3DVOLUMETEXTURE9)GetDeviceTexture();
    assert(pID3DVolTexture);
  }
  D3DFORMAT frmtSrc = (D3DFORMAT)CTexture::DeviceFormatFromTexFormat(eTFSrc);
  bool bDone = false;
  RECT rc = {nX, nY, nX+USize, nY+VSize};
  if (m_eTT == eTT_2D)
  {
    if (!IsDXTCompressed(m_eTFDst))
    {
      int nBPPSrc = CTexture::BitsPerPixel(eTFSrc);
      int nBPPDst = CTexture::BitsPerPixel(m_eTFDst);
      if(nBPPSrc == nBPPDst)
      {
        IDirect3DTexture9 *tex = (IDirect3DTexture9 *)GetDeviceTexture();
        IDirect3DSurface9 *pSurf;
        tex->GetSurfaceLevel(0, &pSurf);
        D3DLOCKED_RECT lr;
#ifdef XENON
        if(pSurf->LockRect(&lr, NULL, 0)==D3D_OK)
        {
          POINT stPoint = { nX, nY };	
          RECT stRect = { 0, 0, USize, VSize };
          int nSizeLine = CTexture::TextureDataSize(USize, 1, 1, 1, eTFSrc);
          XGTileSurface(lr.pBits, USize, VSize, &stPoint, data, nSizeLine, &stRect, nBPPSrc/8);
          pSurf->UnlockRect();
        }
#else
        if(pSurf->LockRect(&lr, &rc, 0)==D3D_OK)
        {
          byte * p = (byte*)lr.pBits;
          int nOffsSrc = 0;
          int nSizeLine = CTexture::TextureDataSize(USize, 1, 1, 1, eTFSrc);
          for(int y=0; y<VSize; y++)
          {
            cryMemcpy(&p[y*lr.Pitch], &data[nOffsSrc], nSizeLine);
            nOffsSrc += nSizeLine;
          }
          pSurf->UnlockRect();
        }
#endif
        SAFE_RELEASE(pSurf);
        bDone = true;
      }
      else
      if((eTFSrc==eTF_A8R8G8B8 || eTFSrc==eTF_X8R8G8B8) && (m_eTFDst==eTF_R5G6B5))
      {
        IDirect3DTexture9 *tex = (IDirect3DTexture9 *)GetDeviceTexture();
        IDirect3DSurface9 *pSurf;
        tex->GetSurfaceLevel(0, &pSurf);
        D3DLOCKED_RECT lr;
        if(pSurf->LockRect(&lr, &rc, 0)==D3D_OK)
        {
          byte *pBits = (byte *)lr.pBits;
          int nOffsSrc = 0;
          int nSizeLine = CTexture::TextureDataSize(USize, 1, 1, 1, eTFSrc);
          for (int i=0; i<VSize; i++)
          {
            uint *src = (uint *)&data[nOffsSrc];
            ushort *dst = (ushort *)&pBits[i*lr.Pitch];
            for (int j=0; j<USize; j++)
            {
              uint argb = *src++;
              *dst++ =  ((argb >> 8) & 0xF800) |
                        ((argb >> 5) & 0x07E0) |
                        ((argb >> 3) & 0x001F);
            }
            nOffsSrc += nSizeLine;
          }
          pSurf->UnlockRect();
        }
        SAFE_RELEASE(pSurf);
        bDone = true;
      }
    }
  }
  if (!bDone)
  {
    int nPitch;
    if (IsDXTCompressed(eTFSrc))
    {
      int blockSize = (eTFSrc == eTF_DXT1) ? 8 : 16;
      nPitch = (USize+3)/4 * blockSize;
    }
    else
    {
      int size = TextureDataSize(USize, VSize, 1, 1, eTFSrc);
      nPitch = size / VSize;
    }
    RECT rcs;
    rcs.left = 0;
    rcs.right = USize;
    rcs.top = 0;
    rcs.bottom = VSize;
    h = pID3DTexture->GetSurfaceLevel(0, &pDestSurf);
    assert(SUCCEEDED(h) && pDestSurf);
    if (pDestSurf)
      h = D3DXLoadSurfaceFromMemory(pDestSurf, NULL, &rc, data, frmtSrc, nPitch, NULL, &rcs, D3DX_FILTER_NONE, 0);
    SAFE_RELEASE(pDestSurf);
  }
#elif defined (DIRECT3D10)
  ID3D10Texture2D* pID3DTexture = NULL;
  ID3D10Texture2D* pID3DCubeTexture = NULL;
  ID3D10Texture3D* pID3DVolTexture = NULL;
  ID3D10Device* dv = gcpRendD3D->GetD3DDevice();
  HRESULT hr = 0;
  if (m_eTT == eTT_2D)
  {
    pID3DTexture = (ID3D10Texture2D *)GetDeviceTexture();
    assert(pID3DTexture);
  	if (!pID3DTexture)
	  	return;
  }
  else
  if (m_eTT == eTT_Cube)
  {
    pID3DCubeTexture = (ID3D10Texture2D *)GetDeviceTexture();
    assert(pID3DCubeTexture);
  }
  else
  if (m_eTT == eTT_3D)
  {
    pID3DVolTexture = (ID3D10Texture3D *)GetDeviceTexture();
    assert(pID3DVolTexture);
  }
  DXGI_FORMAT frmtSrc = (DXGI_FORMAT)CTexture::DeviceFormatFromTexFormat(eTFSrc);
  bool bDone = false;
  D3D10_BOX rc = {nX, nY, 0, nX+USize, nY+VSize, 1};
  if (m_eTT == eTT_2D)
  {
    if (!IsDXTCompressed(m_eTFDst))
    {
      int nBPPSrc = CTexture::BitsPerPixel(eTFSrc);
      int nBPPDst = CTexture::BitsPerPixel(m_eTFDst);
      if(nBPPSrc == nBPPDst)
      {
        int nRowPitch = CTexture::TextureDataSize(USize, 1, 1, 1, eTFSrc);
        gcpRendD3D->m_pd3dDevice->UpdateSubresource(pID3DTexture, 0, &rc, data, nRowPitch, 0);
        bDone = true;
      }
      else
      if((eTFSrc==eTF_A8R8G8B8 || eTFSrc==eTF_X8R8G8B8) && (m_eTFDst==eTF_R5G6B5))
      {
  assert(0);
        bDone = true;
      }
    }
  }
  if (!bDone)
  {
    CTexture t;
    byte *pData[6] = {data,0,0,0,0,0};
    t.m_eTFDst = t.m_eTFSrc = eTFSrc;
    t.m_nWidth = USize;
    t.m_nHeight = VSize;
    t.CreateDeviceTexture(pData);
    assert(t.GetDeviceTexture());
    ID3D10Resource *pSrcRes = (ID3D10Resource *)t.GetDeviceTexture();
    gcpRendD3D->m_pd3dDevice->CopySubresourceRegion(pID3DTexture, 0, nX, nY, 0, pSrcRes, 0, NULL);
  }
#endif
}

bool CTexture::Fill(const ColorF& color)
{
  if (!(m_nFlags & FT_USAGE_RENDERTARGET))
    return false;

#if defined (DIRECT3D9) || defined(OPENGL)
  LPDIRECT3DTEXTURE9 pTexture = (LPDIRECT3DTEXTURE9)GetDeviceTexture();
  assert(pTexture);
  if (!pTexture)
    return false;
  LPDIRECT3DSURFACE9 pSurf;
  HRESULT hr = pTexture->GetSurfaceLevel(0, &pSurf);
  if (FAILED(hr))
    return false;
  hr = gcpRendD3D->m_pd3dDevice->ColorFill(pSurf, NULL, D3DRGBA(color.r, color.g, color.b, color.a));
  SAFE_RELEASE(pSurf);
  if (FAILED(hr))
    return false;
  return true;
#elif defined (DIRECT3D10)
  //assert(0);
  return false;
#endif
}

void SEnvTexture::Release()
{
  ReleaseDeviceObjects();
  SAFE_DELETE(m_pTex);
}

byte *CTexture::LockData(int& nPitch, int nSide, int nLevel)
{
#if defined (DIRECT3D9) || defined(OPENGL)
  LPDIRECT3DTEXTURE9 pID3DTexture = NULL;
  LPDIRECT3DCUBETEXTURE9 pID3DCubeTexture = NULL;
  LPDIRECT3DVOLUMETEXTURE9 pID3DVolTexture = NULL;
  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();

  if (m_eTT == eTT_2D)
  {
    pID3DTexture = (LPDIRECT3DTEXTURE9)GetDeviceTexture();
    assert(pID3DTexture);
  	if (!pID3DTexture)
	  	return NULL;
  }
  else
  if (m_eTT == eTT_Cube)
  {
    pID3DCubeTexture = (LPDIRECT3DCUBETEXTURE9)GetDeviceTexture();
    assert(pID3DCubeTexture);
    if (!pID3DCubeTexture)
      return NULL;
  }
  else
  if (m_eTT == eTT_3D)
  {
    pID3DVolTexture = (LPDIRECT3DVOLUMETEXTURE9)GetDeviceTexture();
    assert(pID3DVolTexture);
    if (!pID3DVolTexture)
      return NULL;
  }
  if (m_eTT == eTT_2D)
  {
    D3DLOCKED_RECT lr;
    if(pID3DTexture->LockRect(nLevel, &lr, NULL, 0)==D3D_OK)
    {
      nPitch = lr.Pitch;
      return (byte *)lr.pBits;
    }
  }
  else
  if (m_eTT == eTT_Cube)
  {
    D3DLOCKED_RECT lr;
    if(pID3DCubeTexture->LockRect((D3DCUBEMAP_FACES)nSide, nLevel, &lr, NULL, 0)==D3D_OK)
    {
      nPitch = lr.Pitch;
      return (byte *)lr.pBits;
    }
  }
  else
  if (m_eTT == eTT_3D)
  {
    D3DLOCKED_BOX lr;
    if(pID3DVolTexture->LockBox(nLevel, &lr, NULL, 0)==D3D_OK)
    {
      nPitch = lr.RowPitch;
      return (byte *)lr.pBits;
    }
  }
#elif defined (DIRECT3D10)
  assert(0);
#endif
  return NULL;
}

void CTexture::UnlockData(int nSide, int nLevel)
{
#if defined (DIRECT3D9) || defined(OPENGL)
  LPDIRECT3DTEXTURE9 pID3DTexture = NULL;
  LPDIRECT3DCUBETEXTURE9 pID3DCubeTexture = NULL;
  LPDIRECT3DVOLUMETEXTURE9 pID3DVolTexture = NULL;
  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();

  if (m_eTT == eTT_2D)
  {
    pID3DTexture = (LPDIRECT3DTEXTURE9)GetDeviceTexture();
    assert(pID3DTexture);
  	if (!pID3DTexture)
	  	return;
  }
  else
  if (m_eTT == eTT_Cube)
  {
    pID3DCubeTexture = (LPDIRECT3DCUBETEXTURE9)GetDeviceTexture();
    assert(pID3DCubeTexture);
    if (!pID3DCubeTexture)
      return;
  }
  else
  if (m_eTT == eTT_3D)
  {
    pID3DVolTexture = (LPDIRECT3DVOLUMETEXTURE9)GetDeviceTexture();
    assert(pID3DVolTexture);
    if (!pID3DVolTexture)
      return;
  }

  if (m_eTT == eTT_2D)
    pID3DTexture->UnlockRect(nLevel);
  else
  if (m_eTT == eTT_Cube)
    pID3DCubeTexture->UnlockRect((D3DCUBEMAP_FACES)nSide, nLevel);
  else
  if (m_eTT == eTT_3D)
    pID3DVolTexture->UnlockBox(nLevel);
#elif defined (DIRECT3D10)
  assert(0);
#endif
}

void SEnvTexture::ReleaseDeviceObjects()
{
  //if (m_pTex)
  //  m_pTex->ReleaseDynamicRT(true);
}

bool CTexture::ScanEnvironmentCM(const char *name, int size, Vec3& Pos)
{
  int RendFlags = -1;
  RendFlags &= ~DLD_ENTITIES;
  int vX, vY, vWidth, vHeight;
  gRenDev->GetViewport(&vX, &vY, &vWidth, &vHeight);

#if defined (DIRECT3D9) || defined(OPENGL)

  int n;
  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
  LPDIRECT3DCUBETEXTURE9 pID3DSysTexture = NULL;
  D3DLOCKED_RECT d3dlrTarg, d3dlrSys;
  HRESULT h;

  //CRenderer::CV_r_log = 3;
  //gcpRendD3D->ChangeLog();
  LPDIRECT3DSURFACE9 pTargetSurf;
  h = dv->CreateRenderTarget(size, size, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &pTargetSurf, NULL);
  if (FAILED(h))
    return false;

  SDynTexture *pDynT = new SDynTexture(size, size, eTF_A16B16G16R16F, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, "$TempEnv");
  pDynT->Update(size, size);
  CTexture *tpSrc = pDynT->m_pTexture;

  h = D3DXCreateCubeTexture(gcpRendD3D->GetDevice(), size, D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pID3DSysTexture);
  if (FAILED(h))
    return false;

  int *pFR = (int *)gRenDev->EF_Query(EFQ_Pointer2FrameID);
  I3DEngine *eng = (I3DEngine *)iSystem->GetI3DEngine();
  float fMaxDist = eng->GetMaxViewDistance();
	ColorF black(Col_Black);
  SD3DSurface *srfDepth = gcpRendD3D->FX_GetDepthSurface(size, size, false);
  int nPFlags = gcpRendD3D->m_RP.m_PersFlags;
  //gcpRendD3D->m_RP.m_PersFlags |= RBPF_HDR;
  gcpRendD3D->EF_StartEf();
  for(n=0; n<6; n++)
  { 
    (*pFR)++;

    gcpRendD3D->FX_PushRenderTarget(0, tpSrc, srfDepth);
    //gcpRendD3D->FX_PushRenderTarget(0, pTargetSurf, gcpRendD3D->FX_GetDepthSurface(size, size, false));

    gcpRendD3D->SetViewport(0, 0, size, size);
    gcpRendD3D->EF_ClearBuffers(FRT_CLEAR, &black);

    gcpRendD3D->EF_Commit();
    DrawCubeSide(Pos, size, n, RendFlags, fMaxDist);
    gcpRendD3D->FX_PopRenderTarget(0);

    // Post process (bluring)
    if (pTargetSurf)
    {
      STexState TexStateLinear = STexState(FILTER_LINEAR, true);
      tpSrc->Apply(0, CTexture::GetTexState(TexStateLinear));
      gcpRendD3D->FX_PushRenderTarget(0, pTargetSurf, srfDepth);
      gcpRendD3D->SetCullMode(R_CULL_NONE);
      gcpRendD3D->SetViewport(0, 0, size, size);
      gcpRendD3D->EF_Commit();

			static CCryName techName("EncodeHDR");
      gcpRendD3D->DrawFullScreenQuad(CShaderMan::m_ShaderPostProcess, techName, 0, 0, 1, 1);
      gcpRendD3D->FX_PopRenderTarget(0);
      gcpRendD3D->EF_Commit();

      // Copy data
      h = pTargetSurf->LockRect(&d3dlrTarg, NULL, 0);
      h = pID3DSysTexture->LockRect((D3DCUBEMAP_FACES)n, 0, &d3dlrSys, NULL, 0);
      byte *pSys = (byte *)d3dlrSys.pBits;
      byte *pTarg = (byte *)d3dlrTarg.pBits;
      memcpy(pSys, pTarg, size*size*4);
      h = pID3DSysTexture->UnlockRect((D3DCUBEMAP_FACES)n, 0);
      h = pTargetSurf->UnlockRect();
    }
  }
  SRendItem::m_RecurseLevel--;
  TArray<byte> Data;
  h = D3DXFilterCubeTexture(pID3DSysTexture, NULL, D3DX_DEFAULT, D3DTEXF_LINEAR);
  int nMips = CTexture::CalcNumMips(size, size);
  for (n=0; n<6; n++)
  {
    int wd = size;
    int hg = size;
    for (int i=0; i<nMips; i++)
    {
      assert(wd || hg);
      if (!wd)
        wd = 1;
      if (!hg)
        hg = 1;
      pID3DSysTexture->LockRect((D3DCUBEMAP_FACES)n, i, &d3dlrSys, NULL, 0);
      Data.Copy((byte *)d3dlrSys.pBits, CTexture::TextureDataSize(wd, hg, 1, 1, eTF_A8R8G8B8));
      h = pID3DSysTexture->UnlockRect((D3DCUBEMAP_FACES)n, i);
      wd >>= 1;
      hg >>= 1;
    }
  }
  ::WriteDDS(&Data[0], size, size, 1, name, eTF_A8R8G8B8, nMips, eTT_Cube);

  gRenDev->SetViewport(vX, vY, vWidth, vHeight);
  gcpRendD3D->m_RP.m_PersFlags = nPFlags;

  SAFE_RELEASE(pTargetSurf);
  SAFE_RELEASE(pID3DSysTexture);
  SAFE_DELETE(pDynT);

#elif defined (DIRECT3D10)
  assert(0);
#endif

  //CRenderer::CV_r_log = 0;
  //gcpRendD3D->ChangeLog();

  return true;
}

//////////////////////////////////////////////////////////////////////////

void CTexture::DrawCubeSide(Vec3& Pos, int tex_size, int side, int RendFlags, float fMaxDist)
{
  static float sCubeVector[6][7] = 
  {
    {1,0,0,  0,0,1, -90}, //posx
    {-1,0,0, 0,0,1,  90}, //negx
    {0,1,0,  0,0,-1, 0},  //posy
    {0,-1,0, 0,0,1,  0},  //negy
    {0,0,1,  0,1,0,  0},  //posz
    {0,0,-1, 0,1,0,  0},  //negz
  };

  if (!iSystem)
    return;

  CRenderer *r = gRenDev;
  CCamera prevCamera = r->GetCamera();
  CCamera tmpCamera = prevCamera;

  I3DEngine *eng = iSystem->GetI3DEngine();
  float fMinDist = 0.25f;

  Vec3 vForward = Vec3(sCubeVector[side][0], sCubeVector[side][1], sCubeVector[side][2]);
  Vec3 vUp      = Vec3(sCubeVector[side][3], sCubeVector[side][4], sCubeVector[side][5]);
  Matrix33 matRot = Matrix33::CreateOrientation(vForward, vUp, DEG2RAD(sCubeVector[side][6]));

	tmpCamera.SetMatrix(Matrix34(matRot, Pos));
  tmpCamera.SetFrustum(tex_size, tex_size, 90.0f*gf_PI/180.0f, fMinDist, fMaxDist);
  int nPersFlags = r->m_RP.m_PersFlags;
  int nPersFlags2 = r->m_RP.m_PersFlags2;
  r->m_RP.m_PersFlags |= RBPF_MIRRORCAMERA | RBPF_MIRRORCULL | RBPF_DRAWTOTEXTURE;
  r->m_RP.m_PersFlags2 |= RBPF2_ENCODE_HDR;
  int nOldZ = CRenderer::CV_r_usezpass;
  CRenderer::CV_r_usezpass = 0;

  r->SetViewport(0, 0, tex_size, tex_size);

#ifdef DO_RENDERLOG
  if (CRenderer::CV_r_log)
    r->Logv(SRendItem::m_RecurseLevel, ".. DrawLowDetail .. (DrawCubeSide %d)\n", side);
#endif

  eng->RenderWorld(SHDF_ALLOWHDR | SHDF_SORT, tmpCamera, "DrawCubeSide", RendFlags, -1);

#ifdef DO_RENDERLOG
  if (CRenderer::CV_r_log)
    r->Logv(SRendItem::m_RecurseLevel, ".. End DrawLowDetail .. (DrawCubeSide %d)\n", side);
#endif

  r->m_RP.m_PersFlags = nPersFlags;
  r->m_RP.m_PersFlags2 = nPersFlags2;
  r->SetCamera(prevCamera);
  CRenderer::CV_r_usezpass = nOldZ;
}

#define HDR_FAKE_MAXOVERBRIGHT 16

void CTexture::GetAverageColor(SEnvTexture *cm, int nSide)
{
  assert (nSide>=0 && nSide<=5);

  //if (!cm->m_pRenderTargets[nSide])
  //  return;

  if (!cm->m_pTex || !cm->m_pTex->m_pTexture)
    return;
  if (cm->m_nFrameCreated[nSide] == -1)
    return;

  HRESULT hr = S_OK;
  /*assert(cm->m_pQuery);
  if (cm->m_pQuery[nSide])
  {
    LPDIRECT3DQUERY9 pQuery = (LPDIRECT3DQUERY9)cm->m_pQuery[nSide];
    BOOL bQuery = false;
    hr = pQuery->GetData((void *)&bQuery, sizeof(BOOL), 0);
    if (hr == S_FALSE)
      return;
    //int nFrame = gcpRendD3D->GetFrameID(false);
    //nFrame -= cm->m_nFrameCreated[nSide];
    //if (nFrame < 100)
    //  return;
  }*/
  ColorF col = ColorF(0,0,0,0);

#if defined (DIRECT3D9) || defined(OPENGL)

  SDynTexture *pTex = cm->m_pTex;
  CTexture *pT = pTex->m_pTexture;
  LPDIRECT3DSURFACE9 pTargSurf, pSrcSurf;
  //pTargSurf = (LPDIRECT3DSURFACE9)cm->m_pRenderTargets[nSide];
  gcpRendD3D->m_pd3dDevice->CreateOffscreenPlainSurface(pTex->GetWidth(), pTex->GetHeight(), (D3DFORMAT)pT->m_pPixelFormat->DeviceFormat, D3DPOOL_SYSTEMMEM, &pTargSurf, NULL);
  pSrcSurf = (LPDIRECT3DSURFACE9)pT->GetSurface(nSide, 0);
  gcpRendD3D->m_pd3dDevice->GetRenderTargetData(pSrcSurf, pTargSurf);

  int i, j;
  D3DLOCKED_RECT d3dlr;
  hr = pTargSurf->LockRect(&d3dlr, NULL, D3DLOCK_READONLY);
  //if (hr == D3DERR_WASSTILLDRAWING)
  //  return;

  byte *pData = (byte *)d3dlr.pBits;
  for (i=0; i<cm->m_TexSize; i++)
  {
    byte *pSrc = &pData[d3dlr.Pitch*i];
    for (j=0; j<cm->m_TexSize; j++)
    {
      float fMul = (float)pSrc[3] / 255.0f * HDR_FAKE_MAXOVERBRIGHT;
      col[0] += pSrc[2] / 255.0f * fMul;
      col[1] += pSrc[1] / 255.0f * fMul;
      col[2] += pSrc[0] / 255.0f * fMul;

      pSrc += 4; 
    }
  }
  pTargSurf->UnlockRect();
  SAFE_RELEASE(pTargSurf);
  SAFE_RELEASE(pSrcSurf);
  cm->m_pTex->UnLock();
#elif defined (DIRECT3D10)
  assert(0);
#endif
  int size = cm->m_TexSize * cm->m_TexSize;
  cm->m_nFrameCreated[nSide] = -1;
  cm->m_EnvColors[nSide] = col / (float)size;
  //if (cm->m_pQuery[nSide])
  //{
  //  LPDIRECT3DQUERY9 pQuery = (LPDIRECT3DQUERY9)cm->m_pQuery[nSide];
  //  pQuery->Issue(D3DISSUE_END);
  //}

  if (cm->m_EnvColors[nSide][0] > 1 || cm->m_EnvColors[nSide][1] > 1 || cm->m_EnvColors[nSide][2] > 1)
    cm->m_EnvColors[nSide].NormalizeCol(cm->m_EnvColors[nSide]);
}

HRESULT GetSampleOffsets_DownScale4x4( DWORD dwWidth, DWORD dwHeight, Vec4 avSampleOffsets[]);

static void sSceneToSceneScaled8x8(CShader *pSH, CTexture *pDstTex, int nSrcTexSize, int nSide)
{
  assert(nSrcTexSize == 8);
  CD3D9Renderer *r = gcpRendD3D;
  int nTempTexSize = nSrcTexSize >> 1;

  ETEX_Format eTF = r->IsHDRModeEnabled() ? eTF_A16B16G16R16F : eTF_A8R8G8B8;
  SDynTexture *pTempTex = new SDynTexture(nTempTexSize, nTempTexSize, eTF, eTT_2D, FT_NOMIPS, "TempEnvCMap");
  pTempTex->Update(nTempTexSize, nTempTexSize);
  CTexture *tpDst = pTempTex->m_pTexture;

  Vec4 avSampleOffsets[16];
  SD3DSurface *srfDepth = r->FX_GetDepthSurface(nTempTexSize, nTempTexSize, false);
  r->FX_SetRenderTarget(0, tpDst, srfDepth);
  r->SetViewport(0, 0, nTempTexSize, nTempTexSize);
  GetSampleOffsets_DownScale4x4(nSrcTexSize, nSrcTexSize, avSampleOffsets);
	static CCryName SampleOffsetsName("SampleOffsets");
  pSH->FXSetPSFloat(SampleOffsetsName, avSampleOffsets, 16);
	static CCryName DownScale4x4_techName("DownScale4x4");
  r->DrawFullScreenQuad(CShaderMan::m_ShaderPostProcess, DownScale4x4_techName, 0, 0, 1, 1);

  r->FX_SetRenderTarget(0, pDstTex, srfDepth, false, false, nSide);
  STexState TexStatePoint = STexState(FILTER_POINT, true);
  tpDst->Apply(0, CTexture::GetTexState(TexStatePoint));
  r->SetViewport(0, 0, 1, 1);
	static CCryName DownScale4x4_EncodeLDR_techName("DownScale4x4_EncodeLDR");
  r->DrawFullScreenQuad(CShaderMan::m_ShaderPostProcess, DownScale4x4_EncodeLDR_techName, 0, 0, 1, 1);

  SAFE_DELETE(pTempTex);
}

bool CTexture::ScanEnvironmentCube(SEnvTexture *pEnv, uint nRendFlags, int Size, bool bLightCube)
{
  if (pEnv->m_bInprogress)
    return true;

  HRESULT hr = S_OK;
  CD3D9Renderer *r = gcpRendD3D;
  int vX, vY, vWidth, vHeight;
  r->GetViewport(&vX, &vY, &vWidth, &vHeight);
  r->EF_PushFog();

  int Start, End;
  if (!pEnv->m_bReady || CRenderer::CV_r_envcmwrite)
  {
    Start = 0;
    End = 6;
  }
  else
  {
    Start = pEnv->m_MaskReady;
    End = pEnv->m_MaskReady+1;
    //Start = 0;
    //End = 6;
  }
  int nPersFlags = r->m_RP.m_PersFlags;
  int nPersFlags2 = r->m_RP.m_PersFlags2;
  r->m_RP.m_PersFlags |= RBPF_DRAWTOTEXTURE;
  r->m_RP.m_PersFlags2 |= RBPF2_ENCODE_HDR;

  int nTexSize = 0;
  int nTempTexSize = nTexSize;
  if (bLightCube)
  {
    nTexSize = 1;
    nTempTexSize = 8;
  }
  else
  {
    switch (CRenderer::CV_r_envcmresolution)
    {
      case 0:
        nTexSize = 64;
        break;
      case 1:
        nTexSize = 128;
        break;
      case 2:
      default:
        nTexSize = 256;
        break;
      case 3:
        nTexSize = 512;
        break;
    }
    nTexSize = sLimitSizeByScreenRes(nTexSize);
    nTempTexSize = nTexSize;
  }

  int n;
  Vec3 cur_pos;

  pEnv->m_bInprogress = true;
  pEnv->m_pTex->Update(nTexSize, nTexSize);
  ETEX_Format eTF = r->IsHDRModeEnabled() ? eTF_A16B16G16R16F : eTF_A8R8G8B8;
  SDynTexture *pTempTex = new SDynTexture(nTempTexSize, nTempTexSize, eTF, eTT_2D, FT_NOMIPS, "TempEnvCMap");
  pTempTex->Update(nTempTexSize, nTempTexSize);
  pEnv->m_TexSize = nTexSize;

  CTexture *tpDst = pEnv->m_pTex->m_pTexture;
  CTexture *tpSrc = pTempTex->m_pTexture;
  Vec3 Pos;
  if (r->m_RP.m_pRE)
    r->m_RP.m_pRE->mfCenter(Pos, r->m_RP.m_pCurObject);
  else
    Pos = pEnv->m_ObjPos;
  nRendFlags &= ~DLD_ENTITIES;

  I3DEngine *eng = (I3DEngine *)iSystem->GetI3DEngine();
  float fMaxDist = eng->GetMaxViewDistance();
  if (bLightCube)
    fMaxDist *= 0.25f;
  int *pFR = (int *)r->EF_Query(EFQ_Pointer2FrameID);
  for(n=Start; n<End; n++)
  { 
    (*pFR)++;

    if (bLightCube)
    {
      /*if (!pEnv->m_pRenderTargets[n])
      {
        IDirect3DSurface9* pSurface;
        hr = r->m_pd3dDevice->CreateRenderTarget(tex_size, tex_size, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &pSurface, NULL);
        if (!FAILED(hr))
          pEnv->m_pRenderTargets[n] = pSurface;
      }*/
      /*if (!pEnv->m_pQuery[n])
      {
        LPDIRECT3DQUERY9 pQuery;
        hr = r->m_pd3dDevice->CreateQuery(D3DQUERYTYPE_EVENT, &pQuery);
        pEnv->m_pQuery[n] = pQuery;
        pQuery->Issue(D3DISSUE_END);
      }
      else*/
      //if (pEnv->m_nFrameCreated[n] >= 0)
      //  continue;
    }

    // render object
    SD3DSurface *srfDepth = r->FX_GetDepthSurface(nTempTexSize, nTempTexSize, false);
    bool bRes = r->FX_PushRenderTarget(0, tpSrc, srfDepth);
		ColorF col = r->m_cClearColor;
    col.a = 0;
    r->EF_ClearBuffers(FRT_CLEAR, &col);
    r->EF_Commit();
    DrawCubeSide(Pos, nTempTexSize, n, nRendFlags, fMaxDist);

    // Post process (bluring)
    if (tpDst) // || pEnv->m_pRenderTargets[n])
    {
      STexState TexStatePoint = STexState(FILTER_POINT, true);
      tpSrc->Apply(0, CTexture::GetTexState(TexStatePoint));

      if (!bLightCube)
      {
#if defined(DIRECT3D10)
				// COMMENT: CarstenW
				// Can no longer bind 2d depth buffer when rendering to cube map faces. As per DX10 spec a cube map depth 
				// buffer needs to be used (driver crashed when binding the targets in EF_Commit/FX_SetActiveRenderTargets). 
				// For the following blitting op there's no depth buffer needed anyway so simply don't bind one to fix the 
				// problem. Needs to be verified once auto cube map rendering is fully supported again (if ever).
				r->EF_SetState(GS_NODEPTHTEST);
        r->FX_SetRenderTarget(0, tpDst, 0, false, false, n);
#else
				r->FX_SetRenderTarget(0, tpDst, srfDepth, false, false, n);
#endif
        r->SetViewport(0, 0, nTexSize, nTexSize);
				static CCryName techName("Encode_ToLDR");
        r->DrawFullScreenQuad(CShaderMan::m_ShaderPostProcess, techName, 0, 0, 1, 1);
      }
      else
        sSceneToSceneScaled8x8(CShaderMan::m_ShaderPostProcess, tpDst, nTempTexSize, n);
      pEnv->m_nFrameCreated[n] = r->GetFrameID(false);
    }
    r->FX_PopRenderTarget(0);
  }

  if (CRenderer::CV_r_envcmwrite)
  {
    /*for(n=Start; n<End; n++)
    { 
      static char* cubefaces[6] = {"posx","negx","posy","negy","posz","negz"};
      char str[64];
      int width = tex_size;
      int height = tex_size;
      byte *pic = new byte [width * height * 4];
      LPDIRECT3DSURFACE9 pSysSurf, pTargetSurf;
      LPDIRECT3DTEXTURE9 pID3DSysTexture;
      D3DLOCKED_RECT d3dlrSys;
      if (!pID3DTargetTextureCM)
        pTargetSurf = (LPDIRECT3DSURFACE9)pEnv->m_pRenderTargets[n];
      else
        h = pID3DTargetTextureCM->GetCubeMapSurface((D3DCUBEMAP_FACES)n, 0, &pTargetSurf);
      h = D3DXCreateTexture(r->GetDevice(), tex_size, tex_size, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pID3DSysTexture);
      h = pID3DSysTexture->GetSurfaceLevel(0, &pSysSurf);
      if (pID3DTargetTexture)
        h = D3DXLoadSurfaceFromSurface(pSysSurf, NULL, NULL, pTargetSurf, NULL, NULL,  D3DX_FILTER_NONE, 0);
      else
        h = r->m_pd3dDevice->GetRenderTargetData(pTargetSurf, pSysSurf);
      h = pID3DSysTexture->LockRect(0, &d3dlrSys, NULL, 0);
      byte *src = (byte *)d3dlrSys.pBits;
      for (int i=0; i<width*height; i++)
      {
        *(uint *)&pic[i*4] = *(uint *)&src[i*4];
        Exchange(pic[i*4+0], pic[i*4+2]);
      }
      h = pID3DSysTexture->UnlockRect(0);
      sprintf(str, "Cube_%s.jpg", cubefaces[n]);
      WriteJPG(pic, width, height, str, 24); 
      delete [] pic;
      if (pID3DTargetTextureCM)
        SAFE_RELEASE (pTargetSurf);
      SAFE_RELEASE (pSysSurf);
      SAFE_RELEASE (pID3DSysTexture);
    }*/
    CRenderer::CV_r_envcmwrite = 0;
  }
  SAFE_DELETE(pTempTex);

  r->SetViewport(vX, vY, vWidth, vHeight);
  pEnv->m_bInprogress = false;
  pEnv->m_MaskReady = End;
  if (pEnv->m_MaskReady == 6)
  {
    pEnv->m_MaskReady = 0;
    pEnv->m_bReady = true;
  }
  r->m_RP.m_PersFlags = nPersFlags;
  r->m_RP.m_PersFlags2 = nPersFlags2;

  r->EF_PopFog();

  return true;
}

void CTexture::GetBackBuffer(CTexture *pTex, int nRendFlags)
{
#if defined (DIRECT3D9) || defined(OPENGL)

  if(!pTex && ! pTex->GetDeviceTexture())
  {
    //iLog->Log("Error: %s", pTex->GetName());
    return;
  }

  // Beware with e_widescreen usage. Only copy visible screen area.
  int iTempX, iTempY, iWidth, iHeight;
  gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
  RECT pSrcRect={iTempX, iTempY, iTempX+iWidth, iTempY+iHeight };
  RECT pDstRect={0, 0, iWidth, iHeight };

  // recreate texture when necessary
  pTex->Invalidate(iWidth, iHeight, eTF_Unknown);

  ETEX_Format eTF = eTF_A8R8G8B8;
  // if not created yet, create texture
  if(!pTex->GetDeviceTexture())
  {
    if (!pTex->CreateRenderTarget(eTF))
    {
      return;
    }
    // Clear render target surface before using it
    ColorF c = ColorF(0, 0, 0, 1);
    pTex->Fill(c);
  }

  gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "Copying backbuffer into texture: %s\n", pTex->GetName()); 

  // get texture surface
  LPDIRECT3DSURFACE9 pTexSurfCopy;  
  LPDIRECT3DTEXTURE9 plD3DTextureCopy = (LPDIRECT3DTEXTURE9) pTex->GetDeviceTexture();  
  HRESULT hr = plD3DTextureCopy->GetSurfaceLevel(0, &pTexSurfCopy);

  if(FAILED(hr))
    return;

  LPDIRECT3DSURFACE9 pBackSurface=gcpRendD3D->GetBackSurface(); 
  gcpRendD3D->m_RP.m_PS.m_RTCopied++;
  gcpRendD3D->m_RP.m_PS.m_RTCopiedSize += pTex->GetDeviceDataSize();
  hr = gcpRendD3D->m_pd3dDevice->StretchRect(pBackSurface, &pSrcRect, pTexSurfCopy, &pDstRect, D3DTEXF_NONE);

  SAFE_RELEASE(pTexSurfCopy);

#elif defined (DIRECT3D10)
  assert(0);
#endif
}

CTexture *CTexture::MakeSpecularTexture(float fExp)
{
  assert(0);
  return NULL;
}

//================================================================================

VOID CFnMap::Fill2DWrapper(D3DXVECTOR4* pOut, const D3DXVECTOR2* pTexCoord, const D3DXVECTOR2* pTexelSize, LPVOID pData)
{
  CFnMap* map = (CFnMap*)pData;
  const D3DXCOLOR& c = map->Function(pTexCoord, pTexelSize);
  *pOut = D3DXVECTOR4((const float*)c);
}

int CFnMap::Initialize()
{
#if defined (DIRECT3D9) || defined(OPENGL)

  LPDIRECT3DTEXTURE9 pTexture;
  HRESULT hr;
  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();

  hr = D3DXCreateTexture(gcpRendD3D->GetDevice(), m_dwWidth, m_dwHeight, m_nLevels, 0, m_Format, TEXPOOL, &pTexture);
  if (FAILED(hr))
  {
    OutputDebugString("Can't create texture\n");
    return hr;
  }

  if (FAILED(hr=D3DXFillTexture(pTexture, Fill2DWrapper, (void*)this)))
    return hr;

  CTexture *tp = m_pTex;
  SAFE_DELETE(tp->m_pFunc);
  tp->m_pFunc = this;
  tp->m_pDeviceTexture = pTexture;
  tp->m_nWidth = m_dwWidth;
  tp->m_nHeight = m_dwHeight;
  tp->m_nMips = m_nLevels;
#elif defined (DIRECT3D10)
  byte *pData[6] = {0,0,0,0,0,0};
  CTexture *tp = m_pTex;
  SAFE_DELETE(tp->m_pFunc);
  tp->m_pFunc = this;
  tp->m_nWidth = m_dwWidth;
  tp->m_nHeight = m_dwHeight;
  tp->m_nMips = m_nLevels;
  tp->CreateDeviceTexture(pData);
#endif
  return S_OK;
}

VOID CFnMap3D::Fill3DWrapper(D3DXVECTOR4* pOut, const D3DXVECTOR3* pTexCoord, const D3DXVECTOR3* pTexelSize, LPVOID pData)
{
  CFnMap3D* map = (CFnMap3D*)pData;
  const D3DXCOLOR& c = map->Function(pTexCoord, pTexelSize);
  *pOut = D3DXVECTOR4((const float*)c);
}

int CFnMap3D::Initialize()
{
#if defined (DIRECT3D9) || defined(OPENGL)

  LPDIRECT3DVOLUMETEXTURE9 pID3DVolTexture = NULL;
  HRESULT hr;
  LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();

  hr = D3DXCreateVolumeTexture (gcpRendD3D->GetDevice(), m_dwWidth, m_dwHeight, m_dwDepth, m_nLevels, 0, m_Format, TEXPOOL, &pID3DVolTexture);
  if (FAILED(hr))
  {
    OutputDebugString("Can't create texture\n");
    return hr;
  }

  if (FAILED(hr=D3DXFillVolumeTexture(pID3DVolTexture, Fill3DWrapper, (void*)this)))
    return hr;

  CTexture *tp = m_pTex;
  SAFE_DELETE(tp->m_pFunc);
  tp->m_pFunc = this;
  tp->m_pDeviceTexture = pID3DVolTexture;
  tp->m_nWidth = m_dwWidth;
  tp->m_nHeight = m_dwHeight;
  tp->m_nDepth = m_dwDepth;
  tp->m_nMips = m_nLevels;
#elif defined (DIRECT3D10)
  byte *pData[6] = {0,0,0,0,0,0};
  CTexture *tp = m_pTex;
  SAFE_DELETE(tp->m_pFunc);
  tp->m_pFunc = this;
  tp->m_nWidth = m_dwWidth;
  tp->m_nHeight = m_dwHeight;
  tp->m_nDepth = m_dwDepth;
  tp->m_nMips = m_nLevels;
  tp->CreateDeviceTexture(pData);
#endif
  return S_OK;
}

//=======================================================================

D3DXCOLOR CMipsColorMap::Function(const D3DXVECTOR2* pTexCoord, const D3DXVECTOR2* pTexelSize)
{
  D3DXCOLOR vOut;

  float d = 1.0/((pTexCoord->x+pTexCoord->y)/2.0f);
  Vec3 p = Vec3(0,0,0);
  if (d < 8)
  {
    p = Vec3(1,1,1);
  }
  else
  {
    float q = logf(d)/logf(2);
    q = floorf(q+.5f);
    if (fmodf(q,3) == 0)
    {
      p.x = .5;
    }
    if (fmodf(q,6) == 0)
    {
      p.x = 1;
    }
    if (fmodf(q,2) == 0)
    {
      p.y = .5;
    }
    if (fmodf(q,4) == 0)
    {
      p.y = 1;
    }
    q = q / m_nLevelMips;
    p.z = q;
  }
  vOut.r = p.x;
  vOut.g = p.y;
  vOut.b = p.z;
  vOut.a = 1;

  return vOut;
}

CTexture *CTexture::GenerateMipsColorMap(int nWidth, int nHeight)
{
  CD3D9Renderer *rd = gcpRendD3D;
  bool bMips = false;
  ETEX_Format eTF = eTF_A8R8G8B8;
  char str[256];
  sprintf(str, "$MipColors_%dx%d", nWidth, nHeight);
  CCryName Name = GenName(str, 0, -1, -1);
  CTexture *pTex = GetByName(Name.c_str());
  if (pTex)
    return pTex;
  pTex = CTexture::CreateTextureObject(str, nWidth, nHeight, 1, eTT_2D, FT_DONT_ANISO | FT_DONT_RELEASE, eTF);
  CMipsColorMap *pMap = new CMipsColorMap(nWidth, nHeight, (D3DFORMAT)CTexture::DeviceFormatFromTexFormat(eTF), pTex, true);
  pMap->m_nLevelMips = LogBaseTwo(nWidth);
  pMap->m_nLevelMips = max(LogBaseTwo(nHeight), pMap->m_nLevelMips);
  pMap->Initialize();
  pTex->SetTexStates();

  return pTex;
}

//==========================================================================================

#define NOISE_DIMENSION 128
#define NOISE_MIP_LEVELS 1

#if !defined(LINUX)
static double drand48(void)
{
  double dVal;
  dVal=(double)rand()/(double)RAND_MAX;

  return dVal;
}
#endif

void CTexture::GenerateNoiseVolumeMap()
{
  m_Text_NoiseVolumeMap = CTexture::ForName("Textures/Defaults/Noise3D.dds", FT_DONT_RELEASE | FT_DONT_ANISO | FT_DONT_RESIZE, eTF_Unknown);
}

void CTexture::GenerateNormalizationCubeMap()
{
  int nVolTexSize=CVectorNoiseVolMap::m_nTexSize;

  ETEX_Format eTF = eTF_X8R8G8B8;
  if(!m_Text_NormalizationCubeMap)
  {
    m_Text_NormalizationCubeMap = CTexture::CreateTextureObject("$NormalizationCubeMap", 64, 64, 64, eTT_3D, FT_DONT_ANISO | FT_DONT_RELEASE, eTF);
  }

#if defined (DIRECT3D9) || defined (OPENGL)
  LPDIRECT3DCUBETEXTURE9 pDestSurf = CD3DTexture::CreateNormalizationCubeMap(gcpRendD3D->m_pd3dDevice, 64, 1);
  m_Text_NormalizationCubeMap->AssignDeviceTexture( pDestSurf );
#endif

}

//==========================================================================================
// Volume Map Creation: Noise vectors
//==========================================================================================

Vec3 CVectorNoiseVolMap::GetNoiseVector()
{      
  float alpha = ((float)drand48()) * 2.0f * 3.1415926535f;
  float beta = ((float)drand48()) * 3.1415926535f;
  return Vec3(cosf(alpha) * sinf(beta), sinf(alpha) * sinf(beta), cosf(beta));
}

D3DXCOLOR CVectorNoiseVolMap::Function(const D3DXVECTOR3* p, const D3DXVECTOR3* s)
{
  Vec3 pRandVec=GetNoiseVector();
  pRandVec.x=pRandVec.x*0.5f+0.5f;
  pRandVec.y=pRandVec.y*0.5f+0.5f;
  pRandVec.z=pRandVec.z*0.5f+0.5f;

  return D3DXCOLOR(pRandVec.x, pRandVec.y, pRandVec.z, 0);
}

void CTexture::GenerateVectorNoiseVolMap()
{
  int nVolTexSize=CVectorNoiseVolMap::m_nTexSize;

  ETEX_Format eTF = eTF_X8R8G8B8;
  if(!m_Text_VectorNoiseVolMap)
  {
    m_Text_VectorNoiseVolMap = CTexture::CreateTextureObject("$VectorNoiseVolume", nVolTexSize, nVolTexSize, nVolTexSize, eTT_3D, FT_DONT_ANISO | FT_DONT_RELEASE, eTF);
  }

  CVectorNoiseVolMap *pMap = new CVectorNoiseVolMap(nVolTexSize, nVolTexSize, nVolTexSize, (D3DFORMAT)CTexture::DeviceFormatFromTexFormat(eTF), m_Text_VectorNoiseVolMap, 1);
  pMap->Initialize();
  m_Text_VectorNoiseVolMap->SetTexStates();
}

void CTexture::GenerateFlareMap()
{
  int i, j;

  byte data[4][32][4];
  for (i=0; i<32; i++)
  {
    float f = 1.0f - ((fabsf((float)i - 15.5f) - 0.5f) / 16.0f);
    int n = (int)(f*f*255.0f);
    for (j=0; j<4; j++)
    {
      byte b = n;
      if (n < 0)
        b = 0;
      else
        if (n > 255)
          b = 255;
      data[j][i][0] = b;
      data[j][i][1] = b;
      data[j][i][2] = b;
      data[j][i][3] = 255;
    }
  }
  CTexture::m_Text_Flare = CTexture::Create2DTexture("$Flare", 32, 4, 1, FT_STATE_CLAMP | FT_DONT_RELEASE | FT_DONT_ANISO, &data[0][0][0], eTF_A8R8G8B8, eTF_A8R8G8B8);
}
/*
// Generate N points on sphere
void CTexture::Generate_16_PointsOnSphere()
{
  const int nTexSize = 4;

  const int nSamples = nTexSize*nTexSize;

  Vec3 arrSphere[nSamples];

  float fMaxMinDist = 0;

  Vec3 arrSphereGood[16];

  Vec2i nBadIds(0,0);

  int nCounter=0;

  float fMinQuality;
  if(nTexSize==3)
    fMinQuality = 1.15f;
  if(nTexSize==4)
    fMinQuality = 0.85f;

  while(fMaxMinDist<fMinQuality && nCounter<10000)
  {
    if(fMaxMinDist<fMinQuality/2)
    { // generate new
      for(int i=0; i<nSamples; i++)
        arrSphere[i] = Vec3(rnd()-0.5f,rnd()-0.5f,rnd()-0.5f).GetNormalized();
    }
    else
    { // refine old
      Vec3 vMid = (arrSphere[nBadIds.x]+arrSphere[nBadIds.y])*0.5f;
      arrSphere[nBadIds.x] = vMid + (arrSphere[nBadIds.x] - vMid)*1.01f;
      arrSphere[nBadIds.y] = vMid + (arrSphere[nBadIds.y] - vMid)*1.01f;
      arrSphere[nBadIds.x].Normalize();
      arrSphere[nBadIds.y].Normalize();
    }

    // find how good result and find 2 close to each other points
    float fMinDist = 1000;
    for(int i=0; i<nSamples; i++)
    {
      for(int j=0; j<nSamples; j++)
      {
        if(i != j)
        {
          float fDist = arrSphere[i].GetDistance(arrSphere[j]);
          if(fDist<fMinDist)
          {
            fMinDist = fDist;
            nBadIds.x = i;
            nBadIds.y = j;
          }
        }
      }
    }

    // store if better than before
    if(fMaxMinDist<fMinDist)
    {
      fMaxMinDist = fMinDist;
      memcpy(arrSphereGood, arrSphere, sizeof(arrSphereGood));
    }

    nCounter++;
  }

  byte data[nSamples][4];

  for(int i=0; i<nSamples; i++)
  {
    Vec3 vConv = (arrSphereGood[i]+Vec3(1,1,1))*127.5f;
    data[i][0] = (byte)SATURATEB(vConv.x);
    data[i][1] = (byte)SATURATEB(vConv.y);
    data[i][2] = (byte)SATURATEB(vConv.z);
    data[i][3] = 255;
  }

  CTexture::m_Text_16_PointsOnSphere = CTexture::Create2DTexture("$16PointsOnSphere", nTexSize, nTexSize, 1, FT_STATE_CLAMP | FT_DONT_RELEASE | FT_DONT_ANISO, &data[0][0], eTF_A8R8G8B8, eTF_A8R8G8B8);
}*/

void CTexture::Generate_16_PointsOnSphere()
{
	Vec3 arrSphere[16] =
	{
		Vec3(0.9848f,-0.0322f,0.1704f),
		Vec3(-0.2461f,-0.9692f,-0.0058f),
		Vec3(-0.2289f,0.9296f,0.2888f),
		Vec3(-0.8953f,0.3900f,0.2151f),
		Vec3(0.7299f,-0.3444f,-0.5905f),
		Vec3(0.1176f,0.1141f,-0.9865f),
		Vec3(-0.3800f,-0.5460f,-0.7467f),
		Vec3(-0.7415f,0.2527f,-0.6216f),
		Vec3(-0.0995f,0.8258f,-0.5551f),
		Vec3(0.5936f,0.7014f,0.3945f),
		Vec3(0.7074f,0.5440f,-0.4513f),
		Vec3(-0.3606f,0.2985f,0.8837f),
		Vec3(-0.3319f,-0.5565f,0.7617f),
		Vec3(0.4873f,-0.2974f,0.8210f),
		Vec3(0.5897f,-0.7974f,0.1277f),
		Vec3(-0.9044f,-0.4200f,-0.0759f),
	};

	byte data[16][4];

	for(int i=0; i<16; i++)
	{
		Vec3 vConv = (arrSphere[i]+Vec3(1,1,1))*127.5f;
		data[i][0] = (byte)SATURATEB(vConv.x);
		data[i][1] = (byte)SATURATEB(vConv.y);
		data[i][2] = (byte)SATURATEB(vConv.z);
		data[i][3] = 255;
	}

	CTexture::m_Text_16_PointsOnSphere = CTexture::Create2DTexture("$16PointsOnSphere", 4, 4, 1, FT_STATE_CLAMP | FT_DONT_RELEASE | FT_DONT_ANISO, &data[0][0], eTF_A8R8G8B8, eTF_A8R8G8B8);
}

void CTexture::GenerateFuncTextures()
{
 // GenerateFlareMap();
 // GenerateNoiseVolumeMap();
  GenerateNormalizationCubeMap();
	Generate_16_PointsOnSphere();
}

void CTexture::DestroyZMaps()
{
  //SAFE_RELEASE(m_Text_ZTarget);
}

void CTexture::GenerateZMaps()
{
  int nWidth = gcpRendD3D->GetWidth(); //m_d3dsdBackBuffer.Width;
  int nHeight = gcpRendD3D->GetHeight(); //m_d3dsdBackBuffer.Height;
  ETEX_Format eTFZ = CTexture::m_eTFZ;
  uint nFlags = FT_DONT_ANISO | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_DONT_RELEASE | FT_USAGE_PREDICATED_TILING;
  if (CRenderer::CV_r_fsaa)
    nFlags |= FT_USAGE_FSAA;
  if (!m_Text_ZTarget)
    m_Text_ZTarget = CreateRenderTarget("$ZTarget", nWidth, nHeight, eTT_2D, nFlags, eTFZ, -1, 100);
  else
  {
    m_Text_ZTarget->m_nFlags = nFlags;
    m_Text_ZTarget->m_nWidth = nWidth;
    m_Text_ZTarget->m_nHeight = nHeight;
    m_Text_ZTarget->CreateRenderTarget(eTFZ);
  }
}

void CTexture::DestroySceneMap()
{
  //SAFE_RELEASE(m_Text_SceneTarget);
}

void CTexture::GenerateSceneMap(ETEX_Format eTF)
{
  int nWidth = gcpRendD3D->GetWidth(); //m_d3dsdBackBuffer.Width;
  int nHeight = gcpRendD3D->GetHeight(); // m_d3dsdBackBuffer.Height;
  uint nFlags = FT_DONT_ANISO | FT_USAGE_PREDICATED_TILING | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_DONT_RELEASE;
  if (CRenderer::CV_r_fsaa && !CRenderer::CV_r_debug_extra_scenetarget_fsaa)
    nFlags |= FT_USAGE_FSAA;
  if (!m_Text_SceneTarget)
    m_Text_SceneTarget = CreateRenderTarget("$SceneTarget", nWidth, nHeight, eTT_2D, nFlags, eTF, -1, 100);
  else
  {
    m_Text_SceneTarget->m_nFlags = nFlags;
    m_Text_SceneTarget->m_nWidth = nWidth;
    m_Text_SceneTarget->m_nHeight = nHeight;
    m_Text_SceneTarget->CreateRenderTarget(eTF);
  }

  nFlags &= ~FT_USAGE_FSAA;
  // This RT used for glow pass and shadow mask (group 0) as well
  if (!m_Text_BackBuffer || !m_Text_BackBuffer->GetDeviceTexture())
    m_Text_BackBuffer = CreateRenderTarget("$BackBuffer", nWidth, nHeight, eTT_2D, nFlags, eTF_A8R8G8B8, TO_BACKBUFFERMAP, 100);
  else
  {
    m_Text_BackBuffer->m_nFlags = nFlags;
    m_Text_BackBuffer->m_nWidth = nWidth;
    m_Text_BackBuffer->m_nHeight = nHeight;
    m_Text_BackBuffer->CreateRenderTarget(eTF_A8R8G8B8);
  }
}

void CTexture::DestroyLightInfo()
{
  m_Text_LightInfo[SRendItem::m_RecurseLevel-1]->ReleaseDeviceTexture(false);
}

void CTexture::GenerateLightInfo(ETEX_Format eTF, int nWidth, int nHeight)
{
  DestroyLightInfo();

  int nFlags = FT_DONT_ANISO | FT_DONT_STREAM | FT_DONT_RELEASE;
  if (CRenderer::CV_r_shadersdynamicbranching == 2)
    nFlags |= FT_USAGE_DYNAMIC;
  else
    nFlags |= FT_USAGE_RENDERTARGET;
  m_Text_LightInfo[SRendItem::m_RecurseLevel-1]->m_nFlags = nFlags;
  m_Text_LightInfo[SRendItem::m_RecurseLevel-1]->Create2DTexture(nWidth, nHeight, 1, nFlags, NULL, eTF, eTF);
}

void CTexture::GenerateSkyDomeTextures(uint32 width, uint32 height)
{
	DestroySkyDomeTextures();

	int creationFlags = FT_STATE_CLAMP | FT_DONT_RELEASE | FT_NOMIPS | FT_DONT_ANISO;	
#if !defined(DIRECT3D10) || defined(PS3)
	creationFlags |= FT_USAGE_DYNAMIC;
#endif

	m_SkyDomeTextureMie = Create2DTexture("$SkyDomeTextureMie", width, height, 1, creationFlags, 0, eTF_A16B16G16R16F, eTF_A16B16G16R16F);
	m_SkyDomeTextureMie->Fill(ColorF(0, 0, 0, 0));
	m_SkyDomeTextureMie->SetFilterMode(gcpRendD3D->m_bDeviceSupportsFP16Filter ? FILTER_LINEAR : FILTER_POINT);
	m_SkyDomeTextureMie->SetClampingMode(0, 1, 1);
	m_SkyDomeTextureMie->UpdateTexStates();

	m_SkyDomeTextureRayleigh = Create2DTexture("$SkyDomeTextureRayleigh", width, height, 1, creationFlags, 0, eTF_A16B16G16R16F, eTF_A16B16G16R16F);
	m_SkyDomeTextureRayleigh->Fill(ColorF(0, 0, 0, 0));
	m_SkyDomeTextureRayleigh->SetFilterMode(gcpRendD3D->m_bDeviceSupportsFP16Filter ? FILTER_LINEAR : FILTER_POINT);
	m_SkyDomeTextureRayleigh->SetClampingMode(0, 1, 1);
	m_SkyDomeTextureRayleigh->UpdateTexStates();
}

void CTexture::DestroySkyDomeTextures()
{
	SAFE_RELEASE(m_SkyDomeTextureMie);
	SAFE_RELEASE(m_SkyDomeTextureRayleigh);
}

bool SDynTexture::SetRT(int nRT, bool bPush, SD3DSurface *pDepthSurf)
{
  Update(m_nWidth, m_nHeight);

  assert(m_pTexture);
  if (m_pTexture)
  {
    if (bPush)
      return gcpRendD3D->FX_PushRenderTarget(nRT, m_pTexture, pDepthSurf);
    else
      return gcpRendD3D->FX_SetRenderTarget(nRT, m_pTexture, pDepthSurf);
  }
  return false;
}

bool SDynTexture::RestoreRT(int nRT, bool bPop)
{
  if (bPop)
    return gcpRendD3D->FX_PopRenderTarget(nRT);
  else
    return gcpRendD3D->FX_RestoreRenderTarget(nRT);
}


bool SDynTexture2::SetRT(int nRT, bool bPush, SD3DSurface *pDepthSurf)
{
  Update(m_nWidth, m_nHeight);

  assert(m_pTexture);
  if (m_pTexture)
  {
    bool bRes = false;
    if (bPush)
      bRes = gcpRendD3D->FX_PushRenderTarget(nRT, m_pTexture, pDepthSurf);
    else
      bRes = gcpRendD3D->FX_SetRenderTarget(nRT, m_pTexture, pDepthSurf);
    SetRectStates();
		gcpRendD3D->EF_Commit();
  }
  return false;
}

bool SDynTexture2::SetRectStates()
{
  assert(m_pTexture);
  gcpRendD3D->SetViewport(m_nX, m_nY, m_nWidth, m_nHeight);
  gcpRendD3D->EF_Scissor(true, m_nX, m_nY, m_nWidth, m_nHeight);
  return true;
}

bool SDynTexture2::RestoreRT(int nRT, bool bPop)
{
	bool bRes = false;
  gcpRendD3D->EF_Scissor(false, m_nX, m_nY, m_nWidth, m_nHeight);
  if (bPop)
    bRes = gcpRendD3D->FX_PopRenderTarget(nRT);
  else
    bRes = gcpRendD3D->FX_RestoreRenderTarget(nRT);
  gcpRendD3D->EF_Commit();

	return bRes;
}

void _DrawText(ISystem * pSystem, int x, int y, const float fScale, const char * format, ...);

static int __cdecl RTCallback(const VOID* arg1, const VOID* arg2)
{
  CTexture **pi1 = (CTexture **)arg1;
  CTexture **pi2 = (CTexture **)arg2;

	return strcmp((*pi1)->GetName(),(*pi2)->GetName());
/*  if (*pi1 > *pi2)
    return -1;
  if (*pi1 < *pi2)
    return 1;
	return 0;
*/
}

void CD3D9Renderer::DrawAllDynTextures(const char *szFilter, const bool bLogNames, const bool bOnlyIfUsedThisFrame)
{
  int i;
  SDynTexture2::TextureSet2Itor itor;
  char name[256]; //, nm[256];
  strcpy(name, szFilter);
  strlwr(name);
  TArray<CTexture *> UsedRT;
  int nMaxCount = 50;

  float width = 800;
  float height = 600;
  float fArrDim = max(4.f, sqrtf((float)nMaxCount));
  float fPicDimX = width/fArrDim;
  float fPicDimY = height/fArrDim;
  float x = 0;
  float y = 0;
  Set2DMode(true, (int)width, (int)height);
  EF_SelectTMU(0);
  EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
  if (name[0] == '*' && !name[1])
  {
    CCryName Name = CTexture::mfGetClassName();
    SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
    ResourcesMapItor it;
    for (it=pRL->m_RMap.begin(); it!=pRL->m_RMap.end(); it++)
    {
      CTexture *tp = (CTexture *)it->second;
      if (tp && !tp->IsNoTexture())
      {
        if ((tp->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DYNAMIC)) && tp->GetDeviceTexture())
          UsedRT.AddElem(tp);
      }
    }
  }
  else
  {
    CCryName Name = CTexture::mfGetClassName();
    SResourceContainer *pRL = CBaseResource::GetResourcesForClass(Name);
    ResourcesMapItor it;
    for (it=pRL->m_RMap.begin(); it!=pRL->m_RMap.end(); it++)
    {
      CTexture *tp = (CTexture *)it->second;
      if (!tp || tp->IsNoTexture())
				continue;
			if ((tp->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DYNAMIC)) && tp->GetDeviceTexture())
			{
				char nameBuffer[128];
				strncpy(nameBuffer, tp->GetName(), sizeof nameBuffer);
				nameBuffer[sizeof nameBuffer - 1] = 0;
				strlwr(nameBuffer);
				if (CryStringUtils::MatchWildcard(nameBuffer, name))
					UsedRT.AddElem(tp);
      }
    }
#if 0
    for (itor=SDynTexture2::m_TexturePool.begin(); itor!=SDynTexture2::m_TexturePool.end(); itor++)
    {
      if (UsedRT.Num() >= nMaxCount)
        break;
      STextureSetFormat *pTS = (STextureSetFormat *)itor->second;
      SDynTexture2 *pDT;
      int nID = 0;
      for (pDT=pTS->m_pRoot; pDT; pDT=pDT->m_Next)
      {
        if (nID && pDT==pTS->m_pRoot)
          break;
        nID++;
        if (UsedRT.Num() >= nMaxCount)
          break;
        if (!pDT->m_sSource)
          continue;
        strcpy(nm, pDT->m_sSource);
        strlwr(nm);
        if (name[0]!='*' && !strstr(nm, name))
          continue;
        for (i=0; i<UsedRT.Num(); i++)
        {
          if (pDT->m_pTexture == UsedRT[i])
            break;
        }
        if (i == UsedRT.Num())
        {
          UsedRT.AddElem(pDT->m_pTexture);

          x += fPicDimX;
          if (x >= width-10)
          {
            x = 0;
            y += fPicDimY;
          }
        }
      }
    }
#endif
  }
  if (true /* szFilter[0] == '*' */)
  {
    if (UsedRT.Num() > 1)
    {
      qsort(&UsedRT[0], UsedRT.Num(), sizeof(CTexture *), RTCallback);
    }
  }
  fPicDimX = width/fArrDim;
  fPicDimY = height/fArrDim;
  x = 0;
  y = 0;
  for (i=0; i<UsedRT.Num(); i++)
  {
    SetState(GS_NODEPTHTEST);
    CTexture *tp = UsedRT[i];
    int nSavedAccessFrameID = tp->m_nAccessFrameID;

		if(bOnlyIfUsedThisFrame)
			if(nSavedAccessFrameID != gRenDev->m_nFrameUpdateID)
				continue;

    if (tp->GetTextureType() == eTT_2D)
      Draw2dImage(x, y, fPicDimX-2, fPicDimY-2, tp->GetID(), 0,1,1,0,0);
    else
    {
      float fSizeX = fPicDimX / 3;
      float fSizeY = fPicDimY / 2;
      float fx = ScaleCoordX(x); fSizeX = ScaleCoordX(fSizeX);
      float fy = ScaleCoordY(y); fSizeY = ScaleCoordY(fSizeY);
      float fOffsX[] = {fx, fx+fSizeX, fx+fSizeX*2, fx, fx+fSizeX, fx+fSizeX*2};
      float fOffsY[] = {fy, fy, fy, fy+fSizeY, fy+fSizeY, fy+fSizeY};
      Vec3 vTC0[] = {Vec3(1,1,1), Vec3(-1,1,-1), Vec3(-1,1,-1), Vec3(-1,-1,1), Vec3(-1,1,1), Vec3(1,1,-1)};
      Vec3 vTC1[] = {Vec3(1,1,-1), Vec3(-1,1,1), Vec3(1,1,-1), Vec3(1,-1,1), Vec3(1,1,1), Vec3(-1,1,-1)};
      Vec3 vTC2[] = {Vec3(1,-1,-1), Vec3(-1,-1,1), Vec3(1,1,1), Vec3(1,-1,-1), Vec3(1,-1,1), Vec3(-1,-1,-1)};
      Vec3 vTC3[] = {Vec3(1,-1,1), Vec3(-1,-1,-1), Vec3(-1,1,1), Vec3(-1,-1,-1), Vec3(-1,-1,1), Vec3(1,-1,-1)};

      m_matProj->Push();
      EF_PushMatrix();
      Matrix44 *m = m_matProj->GetTop();
      mathMatrixOrthoOffCenterLH(m, 0.0f, (float)m_width, (float)m_height, 0.0f, -1e30f, 1e30f);
      m = m_matView->GetTop();
      m_matView->LoadIdentity();

      EF_SetColorOp(eCO_REPLACE, eCO_REPLACE, DEF_TEXARG0, DEF_TEXARG0);
      STexState ts(FILTER_LINEAR, false);
      ts.m_nAnisotropy = 1;
      tp->Apply(0, CTexture::GetTexState(ts));
      SetCullMode(R_CULL_NONE);

      for (int i=0; i<6; i++)
      {
        int nOffs;
        struct_VERTEX_FORMAT_P3F_TEX3F *vQuad = (struct_VERTEX_FORMAT_P3F_TEX3F *)GetVBPtr(4, nOffs, POOL_P3F_TEX3F);
        if (!vQuad)
          return;
#if defined(OPENGL)
        vQuad[0].p = Vec3(fOffsX[i], fOffsY[i]+fSizeY-1, 1);
        vQuad[0].st = vTC0[i];
        vQuad[1].p = Vec3(fOffsX[i]+fSizeX-1, fOffsY[i]+fSizeY-1, 1);
        vQuad[1].st = vTC1[i];
        vQuad[2].p = Vec3(fOffsX[i]+fSizeX-1, fOffsY[i], 1);
        vQuad[2].st = vTC2[i];
        vQuad[3].p = Vec3(fOffsX[i], fOffsY[i], 1);
        vQuad[3].st = vTC3[i];
#else
        vQuad[0].p = Vec3(fOffsX[i], fOffsY[i], 1);
        vQuad[0].st = vTC0[i];
        vQuad[1].p = Vec3(fOffsX[i]+fSizeX-1, fOffsY[i], 1);
        vQuad[1].st = vTC1[i];
        vQuad[2].p = Vec3(fOffsX[i]+fSizeX-1, fOffsY[i]+fSizeY-1, 1);
        vQuad[2].st = vTC2[i];
        vQuad[3].p = Vec3(fOffsX[i], fOffsY[i]+fSizeY-1, 1);
        vQuad[3].st = vTC3[i];
#endif

        // We are finished with accessing the vertex buffer
        UnlockVB(POOL_P3F_TEX3F);

        if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_TEX3F)))
        {
          // Bind our vertex as the first data stream of our device
          FX_SetVStream(0, m_pVB[POOL_P3F_TEX3F], 0, sizeof(struct_VERTEX_FORMAT_P3F_TEX3F));
          FX_SetFPMode();

          // Render the two triangles from the data stream
  #if defined (DIRECT3D9) || defined(OPENGL)
          HRESULT hReturn = m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, nOffs, 2);
  #elif defined (DIRECT3D10)
          assert(0);
  #endif
        }
      }
      EF_PopMatrix();
      m_matProj->Pop();
    }
    tp->m_nAccessFrameID = nSavedAccessFrameID;

		const char *name = tp->GetName();
		char nameBuffer[128];
		memset(nameBuffer, 0, sizeof nameBuffer);
		for (int i = 0, j = 0; name[i] && j < sizeof nameBuffer - 1; ++i)
		{
			if (name[i] == '$')
			{
				nameBuffer[j] = '$';
				nameBuffer[j + 1] = '$';
				j += 2;
			}
			else
			{
				nameBuffer[j] = name[i];
				++j;
			}
		}
		nameBuffer[sizeof nameBuffer - 1] = 0;
		name = nameBuffer;
    _DrawText(iSystem, (int)(x/width*800.f), (int)(y/height*600.f), 0.7f, "%8s", name);
    _DrawText(iSystem, (int)(x/width*800.f), (int)(y/height*600.f+10), 0.7f, "%d-%d", tp->m_nUpdateFrameID, tp->m_nAccessFrameID);
    _DrawText(iSystem, (int)(x/width*800.f), (int)(y/height*600.f+20), 0.7f, "%dx%d", tp->GetWidth(), tp->GetHeight());

		if(bLogNames)
		{
			iLog->Log("Mem:%d  %dx%d  Type:%s  Format:%s (%s)",
				tp->GetDeviceDataSize(),tp->GetWidth(), tp->GetHeight(),tp->NameForTextureType(tp->GetTextureType()), tp->NameForTextureFormat(tp->GetDstFormat()), tp->GetName());
		}

    x += fPicDimX;
    if (x >= width-10)
    {
      x = 0;
      y += fPicDimY;
    }
  }

  Set2DMode(false, m_width, m_height);
}

//////////////////////////////////////////////////////////////////////////
bool CFlashTextureSource::Update(float distToCamera)
{
	// sanity check: need player and some texture to draw to
	assert(m_pFlashPlayer && m_pDynTexture);
	if (!m_pFlashPlayer || !m_pDynTexture)
		return false;

	// determine if texture resize is require
	int newWidth(0), newHeight(0);
	CalcSize(newWidth, newHeight, distToCamera);
	bool needResize(newWidth != m_pDynTexture->GetWidth() || newHeight != m_pDynTexture->GetHeight());

	// resize texture
	bool forceUpdate(false);
	if (!m_pDynTexture->IsValid() || needResize)
	{
		if (!m_pDynTexture->Update(newWidth, newHeight))
			return false;

		forceUpdate = true;
	}

	// update texture if required
	float currentTime(gEnv->pTimer->GetCurrTime());
	if (forceUpdate || currentTime > m_lastUpdateTime + CRenderer::CV_r_envtexupdateinterval || currentTime < m_lastUpdateTime)
	{
		float ar((float)m_pFlashPlayer->GetWidth() / (float)m_pFlashPlayer->GetHeight());
		m_pFlashPlayer->SetViewport(m_pDynTexture->m_nX, m_pDynTexture->m_nY, m_pDynTexture->m_nWidth, m_pDynTexture->m_nHeight, ar);
		m_pFlashPlayer->SetBackgroundAlpha(0.0f);

		uint32 dynTexRectX(0), dynTexRectY(0), dynTexRectWidth(0), dynTexRectHeight(0);
		m_pDynTexture->GetImageRect(dynTexRectX, dynTexRectY, dynTexRectWidth, dynTexRectHeight);		
		m_pDynTexture->SetRT(0, true, gcpRendD3D->FX_GetDepthSurface(dynTexRectWidth, dynTexRectHeight, false));
#if defined(DIRECT3D10)
		int clearFlags(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE);
#else
		int clearFlags(FRT_CLEAR_COLOR);
#endif
		ColorF clearCol(0, 0, 0, 0);
		gcpRendD3D->EF_ClearBuffers(clearFlags, &clearCol);
		m_pFlashPlayer->Advance(max(currentTime - m_lastUpdateTime, 0.0f));
		m_pFlashPlayer->Render();
		m_pDynTexture->RestoreRT(0, true);

		m_pDynTexture->SetUpdateMask();
		m_lastUpdateTime = currentTime;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CVideoTextureSource::Update(float distToCamera)
{
	// sanity check: need player and some texture to draw to
	assert(m_pVideoPlayer && m_pDynTexture);
	if (!m_pVideoPlayer || !m_pDynTexture)
		return false;

	// determine if texture resize is require
	int newWidth(0), newHeight(0);
	CalcSize(newWidth, newHeight, distToCamera);
	bool needResize(newWidth != m_pDynTexture->GetWidth() || newHeight != m_pDynTexture->GetHeight());

	// resize texture
	bool forceUpdate(false);
	if (!m_pDynTexture->IsValid() || needResize)
	{
		if (!m_pDynTexture->Update(newWidth, newHeight))
			return false;

		forceUpdate = true;
	}

	// update texture if required
	float currentTime(gEnv->pTimer->GetCurrTime());
	if (forceUpdate || currentTime > m_lastUpdateTime + CRenderer::CV_r_envtexupdateinterval || currentTime < m_lastUpdateTime)
	{
		m_pVideoPlayer->SetViewport(m_pDynTexture->m_nX, m_pDynTexture->m_nY, m_pDynTexture->m_nWidth, m_pDynTexture->m_nHeight);

		m_pDynTexture->SetRT(0, true, 0);
#if defined(DIRECT3D10)
		int clearFlags(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE);
#else
		int clearFlags(FRT_CLEAR_COLOR);
#endif
		ColorF clearCol(0, 0, 0, 0);
		gcpRendD3D->EF_ClearBuffers(clearFlags, &clearCol);
		m_pVideoPlayer->Render();
		m_pDynTexture->RestoreRT(0, true);

		m_pDynTexture->SetUpdateMask();
		m_lastUpdateTime = currentTime;
	}

	return true;
}

bool CTexture::SaveDDS(const char *szName, bool bMips)
{
  if (!m_pDeviceTexture)
    return false;
  HRESULT hr = S_OK;
#if defined (DIRECT3D9) || defined (OPENGL)
#ifdef XENON
  char name[256];
  _ConvertNameForXBox(name, szName);
  szName = name;
#endif

  hr = D3DXSaveTextureToFile(szName, D3DXIFF_DDS, (LPDIRECT3DBASETEXTURE9)m_pDeviceTexture, NULL);
#elif defined (DIRECT3D10)
  hr = D3DX10SaveTextureToFile((ID3D10Resource *)m_pDeviceTexture, D3DX10_IFF_DDS, szName);
#endif
  return (hr == S_OK);
}

bool CTexture::SaveJPG(const char *szName, bool bMips)
{
  if (!m_pDeviceTexture)
    return false;
  HRESULT hr = S_OK;
#if defined (DIRECT3D9) || defined (OPENGL)
  D3DXSaveTextureToFile(szName, D3DXIFF_JPG, (LPDIRECT3DBASETEXTURE9)m_pDeviceTexture, NULL);
#elif defined (DIRECT3D10)
  hr = D3DX10SaveTextureToFile((ID3D10Resource *)m_pDeviceTexture, D3DX10_IFF_JPG, szName);
#endif
  return (hr == S_OK);
}

bool CTexture::SaveTGA(const char *szName, bool bMips)
{
  if (!m_pDeviceTexture)
    return false;
  HRESULT hr = S_OK;
#if defined (DIRECT3D9) || defined (OPENGL)
  hr = D3DXSaveTextureToFile(szName, D3DXIFF_TGA, (LPDIRECT3DBASETEXTURE9)m_pDeviceTexture, NULL);
#elif defined (DIRECT3D10)
  hr = D3DX10SaveTextureToFile((ID3D10Resource *)m_pDeviceTexture, D3DX10_IFF_TIFF, szName);
#endif
  return (hr == S_OK);
}
