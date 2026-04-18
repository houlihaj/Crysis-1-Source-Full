/*=============================================================================
  PS2_REOcean.cpp : implementation of the Ocean Rendering.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"
#include "NULL_Renderer.h"
#include "I3DEngine.h"

#undef THIS_FILE
static char THIS_FILE[] = __FILE__;

//=======================================================================

void CREOcean::mfReset()
{
}

bool CREOcean::mfPreDraw(SShaderPass *sl)
{
  return true;
}

void CREOcean::UpdateTexture()
{
}

void CREOcean::DrawOceanSector(SOceanIndicies *oi)
{
}

float *CREOcean::mfFillAdditionalBuffer(SOceanSector *os, int nSplashes, SSplash *pSplashes[], int& nCurSize, int nLod, float fSize)
{
  return NULL;
}

bool CREOcean::mfDraw(CShader *ef, SShaderPass *sfm)
{ 
  return true;
}


