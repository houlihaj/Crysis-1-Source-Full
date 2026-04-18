////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjshadow.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: shadow maps
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "StatObj.h"
#include "../RenderDll/Common/shadow_renderer.h"

/*void CStatObj::PrepareShadowMaps(const Vec3 & obj_pos, ShadowMapSource * pLSource, const Vec3 & vSunPos)
{
	if(!GetCVars()->e_shadow_maps)
		return;

  // reset frustums
  pLSource->m_LightFrustums.Reset();

  { // define new frustum
    ShadowMapFrustum new_lof;
		new_lof.fRadius = 500000;  
		new_lof.vLightSrcRelPos = vSunPos.GetNormalized()*10000;//0;
		new_lof.pOwnerGroup = new_lof.pStatObj = this;

    MakeShadowMapFrustum(&new_lof, pLSource, EST_DEPTH_BUFFER);
    pLSource->m_LightFrustums.Add(new_lof);

    // this list are in another object now
    assert(!new_lof.pEntityList);
  }
 
  int nTexSize = GetCVars()->e_max_shadow_map_size;
  while(nTexSize > GetRadius()*200)
    nTexSize/=2;

  if(nTexSize<16)
    nTexSize=16; // in case of error

  pLSource->m_LightFrustums[0].nTexSize = nTexSize;
}*/

/*void CStatObj::FreeVegetationShadowMap()
{
  if(m_pSMLSource)
  {
    for(int f=0; f<m_pSMLSource->m_LightFrustums.Count(); f++)
    { 
      if(m_pSMLSource->m_LightFrustums[f].pDepthTex)
      {
				// shadow maps textures can be deleted only on shadow maps pool destruction in renderer
//        m_pSMLSource->m_LightFrustums[f].pDepthTex=NULL;
      }
    }
    m_pSMLSource->m_LightFrustums.Reset();
  }

  delete m_pSMLSource;
	m_pSMLSource = NULL;
}*/

/*void CStatObj::MakeVegetationShadowMap(const Vec3 vSunPos)
{
	if(!GetCVars()->e_shadow_maps)
		return;

	FreeVegetationShadowMap();
  m_pSMLSource = new ShadowMapSource;
  PrepareShadowMaps(Vec3(0,0,0), m_pSMLSource, vSunPos);
}

ShadowMapFrustum * CStatObj::MakeShadowMapFrustum(ShadowMapFrustum * pFrustum, ShadowMapSource * pSMSource, int shadow_type)
{   
	Vec3 dir = - pFrustum->vLightSrcRelPos;

	float distance = dir.GetLength();

	float radius = GetRadius();
	float radiusXY = Vec3(m_vBoxMax.x,m_vBoxMax.y,0).GetDistance(Vec3(m_vBoxMin.x,m_vBoxMin.y,0)) * 0.5f;

	pFrustum->fFOV = RAD2DEG(radius/distance)*1.8f;
	pFrustum->fProjRatio = radiusXY/radius;
	if(pFrustum->fProjRatio>1.f)
		pFrustum->fProjRatio=1.f;

	if(pFrustum->fFOV>90)
		pFrustum->fFOV=90;

	pFrustum->fNearDist = distance - radius;
	if(pFrustum->fNearDist<0.1f)
		pFrustum->fNearDist=0.1f;

	pFrustum->fFarDist = distance + radius;

	pFrustum->pSMSource = pSMSource;
	pFrustum->eShadowType = (EShadowType)shadow_type;

	return pFrustum;
}*/
