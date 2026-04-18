/*=============================================================================
  NULL_Font.cpp : NULL font functions.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#undef THIS_FILE
static char THIS_FILE[] = __FILE__;


#include "StdAfx.h"
#include "NULL_Renderer.h"

#include "../CryFont/FBitmap.h"

bool CNULLRenderer::FontUpdateTexture(int nTexId, int X, int Y, int USize, int VSize, byte *pData)
{
  return true;
}

bool CNULLRenderer::FontUploadTexture(class CFBitmap* pBmp, ETEX_Format eTF) 
{
  return true;
}
void CNULLRenderer::FontReleaseTexture(class CFBitmap *pBmp)
{
}

void CNULLRenderer::FontSetTexture(class CFBitmap* pBmp, int nTexFiltMode)
{
}

void CNULLRenderer::FontSetTexture(int nTexId, int nFilterMode)
{
}

void CNULLRenderer::FontSetRenderingState(unsigned int nVPWidth, unsigned int nVPHeight)
{
}

void CNULLRenderer::FontSetBlending(int blendSrc, int blendDest)
{
}

void CNULLRenderer::FontRestoreRenderingState()
{
}

int CNULLRenderer::FontCreateTexture(int Width, int Height, byte *pData, ETEX_Format eTF, bool genMips)
{
  return CTexture::m_Text_NoTexture->GetTextureID();
}