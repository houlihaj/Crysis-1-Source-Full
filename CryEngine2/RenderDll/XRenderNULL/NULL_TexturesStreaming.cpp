/*=============================================================================
  PS2_TexturesStreaming.cpp : PS2 specific texture streaming technology.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"
#include "NULL_Renderer.h"

//===============================================================================

int CTexture::StreamSetLod(int nToMip, bool bUnload)
{
	return 0;
}

// Just remove item from the texture object and keep Item in Pool list for future use
// This function doesn't release API texture
void CTexture::StreamRemoveFromPool()
{
}

bool CTexture::StreamCopyMips(int nStartMip, int nEndMip, bool bToDevice)
{
  return false;
}

bool CTexture::StreamRestoreMipsData()
{
  return false;
}

bool CTexture::StreamAddToPool(int nStartMip, int nMips)
{
  return true;
}

STexPool *CTexture::StreamCreatePool(int nWidth, int nHeight, int nMips, ETEX_Format eTF, ETEX_Type eTT)
{
  return NULL;
}

void CTexture::CreatePools()
{
}

void CTexture::StreamUnloadOldTextures(CTexture *pExclude)
{
}


void CTexture::StreamPrecacheAsynchronously(float fDist, int Flags)
{
}


void CTexture::StreamCheckTexLimits(CTexture *pExclude)
{
}

