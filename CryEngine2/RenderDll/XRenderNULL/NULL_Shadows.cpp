//////////////////////////////////////////////////////////////////////
//
//  Crytek CryENGINE Source code
//  
//  File: PS2_Shadows.cpp
//  Description: Implementation of the shadow maps using PS2 renderer API
//  shadow map calculations
//
//  History:
//  -Jan 31,2001:Created by Vladimir Kajain
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "NULL_Renderer.h"
#include "../Common/Shadow_Renderer.h"

#include "I3DEngine.h"

IDynTexture *CNULLRenderer::MakeDynTextureFromShadowBuffer(int nSize, IDynTexture * pDynTexture)
{
  return NULL;
}

void CNULLRenderer::PrepareDepthMap(ShadowMapFrustum* SMSource, CDLight* pLight)
{
}

void CNULLRenderer::SetupShadowOnlyPass(int Num, ShadowMapFrustum* pShadowInst, Matrix34 * pObjMat)
{
}

void CNULLRenderer::DrawAllShadowsOnTheScreen()
{
}
