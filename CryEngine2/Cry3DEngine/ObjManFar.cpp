////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjmanfar.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: draw far objects as sprites
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "ObjMan.h"
#include "3dEngine.h"

void CObjManager::RenderFarObjects()
{
	FUNCTION_PROFILER_3DENGINE;

  if (m_REFarTreeSprites && GetCVars()->e_vegetation_sprites && m_lstFarObjects[m_nRenderStackLevel].Count() && !GetCVars()->e_default_material)
  {
    CRenderObject * pObj = GetRenderer()->EF_GetObject(true, -1);
    if (!pObj)
      return;
    pObj->m_II.m_Matrix.SetIdentity();
		SShaderItem shIt(m_p3DEngine->m_pFarTreeSprites);
		GetRenderer()->EF_AddEf(m_REFarTreeSprites, shIt, pObj, EFSLIST_GENERAL, 1);
  }
}
/*
static _inline int Compare(CVegetation *& p1, CVegetation *& p2)
{
  if(p1->m_fDistance > p2->m_fDistance)
    return 1;
  else
  if(p1->m_fDistance < p2->m_fDistance)
    return -1;
  
  return 0;
} */

void CObjManager::DrawFarObjects(float fMaxViewDist)
{
  if(!GetCVars()->e_vegetation_sprites)
    return;

	if(m_nRenderStackLevel<0 || m_nRenderStackLevel>=MAX_RECURSION_LEVELS)
	{ assert(!"Recursion depther than MAX_RECURSION_LEVELS is not supported"); return; }

//////////////////////////////////////////////////////////////////////////////////////
//  Draw all far
//////////////////////////////////////////////////////////////////////////////////////

  PodArray<SVegetationSpriteInfo> * pList = &m_lstFarObjects[m_nRenderStackLevel];
 
  if (pList->Count())
    GetRenderer()->DrawObjSprites(pList, fMaxViewDist, this, m_fZoomFactor);
}

void CObjManager::GenerateFarObjects(float fMaxViewDist)
{
  if(!GetCVars()->e_vegetation_sprites)
    return;

  if(m_nRenderStackLevel<0 || m_nRenderStackLevel>=MAX_RECURSION_LEVELS)
  { assert(!"Recursion depther than MAX_RECURSION_LEVELS is not supported"); return; }

  //////////////////////////////////////////////////////////////////////////////////////
  //  Draw all far
  //////////////////////////////////////////////////////////////////////////////////////

  PodArray<SVegetationSpriteInfo> * pList = &m_lstFarObjects[m_nRenderStackLevel];

  if (pList->Count())
    GetRenderer()->GenerateObjSprites(pList, fMaxViewDist, this, m_fZoomFactor);
}
