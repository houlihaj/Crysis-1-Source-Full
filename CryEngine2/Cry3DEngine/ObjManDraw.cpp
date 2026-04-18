////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjmandraw.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Draw static objects (vegetations)
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"
#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "3dEngine.h"
#include "CullBuffer.h"

bool CObjManager::IsBoxOccluded_HeightMap( const AABB & objBox, float fDistance, EOcclusionObjectType eOcclusionObjectType )
{ // test occlusion by heightmap
  FUNCTION_PROFILER_3DENGINE;

	Vec3 vTopMax = objBox.max; 
	Vec3 vTopMin = objBox.min; vTopMin.z = vTopMax.z;

	if((vTopMax.x - vTopMin.x)>10000 || (vTopMax.y - vTopMin.y)>10000) 
		return true;

	const Vec3 & vCamPos = GetCamera().GetPosition();

	if(	vCamPos.x<=vTopMax.x && vCamPos.x>=vTopMin.x && vCamPos.y<=vTopMax.y && vCamPos.y>=vTopMin.y )
		return false; // camera inside of box

	int nMaxTestsToScip = 10000;//(GetVisAreaManager()->m_pCurPortal) ? 3 : 10000; ???

	// precision in meters for this object
	float fMaxStep = fDistance*GetCVars()->e_terrain_occlusion_culling_precision;

	if( ( fMaxStep < (vTopMax.x - vTopMin.x)*GetCVars()->e_terrain_occlusion_culling_precision_dist_ratio || 
        fMaxStep < (vTopMax.y - vTopMin.y)*GetCVars()->e_terrain_occlusion_culling_precision_dist_ratio ) && 
			objBox.min.x != objBox.max.x && objBox.min.y != objBox.max.y )
	{
		float dx = (vTopMax.x - vTopMin.x);
		while(dx>fMaxStep)
			dx*=0.5f;

		float dy = (vTopMax.y - vTopMin.y);
		while(dy>fMaxStep)
			dy*=0.5f;

		dy = max(dy,0.001f);
		dx = max(dx,0.001f);

		bool bCameraAbove = vCamPos.z>vTopMax.z;

		if(bCameraAbove && eOcclusionObjectType != eoot_TERRAIN_NODE)
		{
			for(float y=vTopMin.y; y<=vTopMax.y; y+=dy)
				for(float x=vTopMin.x; x<=vTopMax.x; x+=dx)
					if(!GetTerrain()->IntersectWithHeightMap(vCamPos, Vec3(x, y, vTopMax.z), fDistance, nMaxTestsToScip))
            return false;
		}
		else
		{
			// test only needed edges, note: there are duplicated checks on the corners

			if( (vCamPos.x>vTopMin.x) == bCameraAbove ) // test min x side
				for(float y=vTopMin.y; y<=vTopMax.y; y+=dy)
					if(!GetTerrain()->IntersectWithHeightMap(vCamPos, Vec3(vTopMin.x, y, vTopMax.z), fDistance, nMaxTestsToScip))
						return false;

			if( (vCamPos.x<vTopMax.x) == bCameraAbove ) // test max x side
				for(float y=vTopMax.y; y>=vTopMin.y; y-=dy)
					if(!GetTerrain()->IntersectWithHeightMap(vCamPos, Vec3(vTopMax.x, y, vTopMax.z), fDistance, nMaxTestsToScip))
						return false;

			if( (vCamPos.y>vTopMin.y) == bCameraAbove ) // test min y side
				for(float x=vTopMax.x; x>=vTopMin.x; x-=dx)
					if(!GetTerrain()->IntersectWithHeightMap(vCamPos, Vec3(x, vTopMin.y, vTopMax.z), fDistance, nMaxTestsToScip))
						return false;

			if( (vCamPos.y<vTopMax.y) == bCameraAbove ) // test max y side
				for(float x=vTopMin.x; x<=vTopMax.x; x+=dx)
					if(!GetTerrain()->IntersectWithHeightMap(vCamPos, Vec3(x, vTopMax.y, vTopMax.z), fDistance, nMaxTestsToScip))
						return false;
		}

		return true;
	}
	else
	{
		Vec3 vTopMid = (vTopMin+vTopMax)*0.5f; 
		if( GetTerrain()->IntersectWithHeightMap(vCamPos, vTopMid, fDistance, nMaxTestsToScip))
			return true;
	}

	return false;
}

/*bool CObjManager::IsBoxOccluded_HWOcclQuery( const AABB & objBox, float fDistance, OcclusionTestClient * pOcclTestVars )
{
	// construct RE if needed
	if(!pOcclTestVars->pREOcclusionQuery)
	{
		pOcclTestVars->pREOcclusionQuery = (CREOcclusionQuery *)GetRenderer()->EF_CreateRE(eDATA_OcclusionQuery);
		pOcclTestVars->pREOcclusionQuery->m_pRMBox = (CRenderMesh*)m_pRMBox;
	}

	// define bbox
	float fBorder = 0.01f;
	pOcclTestVars->pREOcclusionQuery->m_vBoxMin = objBox.min-Vec3(fBorder,fBorder,fBorder);
	pOcclTestVars->pREOcclusionQuery->m_vBoxMax = objBox.max+Vec3(fBorder,fBorder,fBorder);

	// if some new object enters into view - test visibility right now
	bool bForceCheckNow = false;
	if(pOcclTestVars->nLastVisibleMainFrameID < GetMainFrameID()-1 && pOcclTestVars->nLastOccludedMainFrameID < GetMainFrameID()-1)
		bForceCheckNow = true;

	bool bReadResultWasReadImmediately = false;

	if((pOcclTestVars->bOccluded && (GetCVars()->e_hw_occlusion_culling_objects<3 || pOcclTestVars->nInstantTestRequested)) || bForceCheckNow || pOcclTestVars->nInstantTestRequested)
	{ // if was not visible - make test immediately
		FRAME_PROFILER( "CObjManager::IsBoxOccluded_HWOcclQuery::_NOW_", GetSystem(), PROFILE_3DENGINE );
		bReadResultWasReadImmediately = true;
		pOcclTestVars->pREOcclusionQuery->mfDraw(NULL, NULL);
		pOcclTestVars->pREOcclusionQuery->mfReadResult_Now();
		pOcclTestVars->bOccluded = pOcclTestVars->pREOcclusionQuery->m_nVisSamples < 120;

		pOcclTestVars->nInstantTestRequested = false;
	}
	else
	{ // if was visible start next lazy test once previous one is ready
		pOcclTestVars->nInstantTestRequested = false;

		if((GetMainFrameID()&15)==((((uint64)(UINT_PTR)pOcclTestVars->pREOcclusionQuery)>>7)&15))
		if(!pOcclTestVars->pREOcclusionQuery->m_nDrawFrame || pOcclTestVars->pREOcclusionQuery->mfReadResult_Try())
		{
			bool bNewOccluded = (pOcclTestVars->pREOcclusionQuery->m_nVisSamples < 120);

			if(bNewOccluded && !pOcclTestVars->bOccluded)
			{
				pOcclTestVars->nInstantTestRequested = true;
			}
			else
			{
				pOcclTestVars->bOccluded = pOcclTestVars->pREOcclusionQuery->m_nVisSamples < 120;
				SShaderItem shIt(m_pShaderOcclusionQuery);
				GetRenderer()->EF_AddEf(pOcclTestVars->pREOcclusionQuery,shIt,GetIdentityCRenderObject(),0);
			}
		}
	}

	if(GetCVars()->e_hw_occlusion_culling_objects == 2)
	{
		float fVis = pOcclTestVars->bOccluded ? 0.f : 1.f;
		float fCol[] = {fVis,!fVis,1,1};
		char szText[32];
		strcpy(szText, fVis ? "V" : "N");
		if(bReadResultWasReadImmediately)
			strcat(szText, "_now");

		GetRenderer()->DrawLabelEx(objBox.GetCenter(), 2, fCol, true, true, "%s", szText);

		DrawBBox(pOcclTestVars->pREOcclusionQuery->m_vBoxMin,pOcclTestVars->pREOcclusionQuery->m_vBoxMax);
	}

	return pOcclTestVars->bOccluded!=0;
}*/

bool CObjManager::IsBoxOccluded( const AABB & objBox, float fDistance, OcclusionTestClient * pOcclTestVars, bool bIndoorOccludersOnly, EOcclusionObjectType eOcclusionObjectType )
{
	assert(pOcclTestVars);
	assert(fDistance>=0);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Check if we can return cached results
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// if object was visible during last frames
  if(/*eOcclusionObjectType == eoot_OBJECT && */pOcclTestVars->nLastVisibleMainFrameID > GetMainFrameID() - 16)
  {
    // prevent checking all objects in same frame
    int nId = (uint64(pOcclTestVars)/256);
    if((nId&7) != (GetMainFrameID()&7))
      return false;
  }

  FUNCTION_PROFILER_3DENGINE;

  if(GetCVars()->e_terrain_occlusion_culling==2)
  {
    bool bOccl = pOcclTestVars->nLastOccludedMainFrameID == GetMainFrameID()-1;
    DrawBBox(objBox, bOccl ? Col_Black : Col_White);
  }

	// in recursion return result of base level test if result is not too old
	if(m_nRenderStackLevel)
	{
		if(GetCVars()->e_recursion_occlusion_culling && pOcclTestVars->nLastOccludedMainFrameID > GetMainFrameID()-32)
			return true;
		else
			return false;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Check if camera is inside of object bbox
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////

	if(objBox.IsContainSphere(GetCamera().GetPosition(), -0.05f))
	{
		pOcclTestVars->nLastVisibleMainFrameID = GetMainFrameID();
		return false;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Anti-portals
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////

	if(GetVisAreaManager()->IsOccludedByOcclVolumes(objBox, bIndoorOccludersOnly))
	{
		pOcclTestVars->nLastOccludedMainFrameID = GetMainFrameID();
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// C-Buffer
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////

  if(	GetCVars()->e_cbuffer && 
			fDistance > Get3DEngine()->GetCoverageBuffer()->GetZNearInMeters() &&
			!Get3DEngine()->GetCoverageBuffer()->IsObjectVisible(objBox, eOcclusionObjectType, fDistance))
	{
		pOcclTestVars->nLastOccludedMainFrameID = GetMainFrameID();
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// HM
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////

  if(GetCVars()->e_terrain_occlusion_culling_version)
  {
    if(!bIndoorOccludersOnly && GetCVars()->e_terrain_occlusion_culling && (fDistance>32.f) &&
      GetTerrain()->IsBoxOccluded( objBox, fDistance, (eOcclusionObjectType == eoot_TERRAIN_NODE), pOcclTestVars ))
    {
      pOcclTestVars->nLastOccludedMainFrameID = GetMainFrameID();
      return true;
    }
  }
  else
  {
	  if(!bIndoorOccludersOnly && GetCVars()->e_terrain_occlusion_culling && (fDistance>32.f) &&
      IsBoxOccluded_HeightMap( objBox, fDistance, eOcclusionObjectType ))
	  {
		  pOcclTestVars->nLastOccludedMainFrameID = GetMainFrameID();
		  return true;
	  }
  }

	if(/*GetCVars()->e_cbuffer!=2 || */(eOcclusionObjectType != eoot_OCCLUDER && eOcclusionObjectType != eoot_OCCELL_OCCLUDER))
	{
		// mark as visible
		pOcclTestVars->nLastVisibleMainFrameID = GetMainFrameID();
	}

  return false;
}

void CObjManager::PrefechObjects()
{
	for (LoadedObjects::iterator it = m_lstLoadedObjects.begin(); it != m_lstLoadedObjects.end(); ++it)
	{
		CStatObj * pStatObj = (*it);
		SRendParams params;
		params.nDLightMask = 1;
		GetRenderer()->EF_StartEf();
		pStatObj->Render(params);
		GetRenderer()->EF_EndEf3D(false);
	}
}
