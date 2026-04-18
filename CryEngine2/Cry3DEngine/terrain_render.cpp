////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_render.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: draw vis sectors
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain.h"
#include "terrain_sector.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "CullBuffer.h"

int __cdecl CTerrain__Cmp_Sectors(const void* v1, const void* v2)
{
	CTerrainNode * p1 = *((CTerrainNode**)v1);
	CTerrainNode * p2 = *((CTerrainNode**)v2);
/*
	// sort by how far current sector texture lod from optimal lod
	int nTexLodErr1 = -p1->m_cNodeNewTexMML;
	while(!p1->m_nNodeTexSet.nTex0 && p1->m_pParent)
	{ p1 = p1->m_pParent; nTexLodErr1++; }

	int nTexLodErr2 = -p2->m_cNodeNewTexMML;
	while(!p2->m_nNodeTexSet.nTex0 && p2->m_pParent)
	{ p2 = p2->m_pParent; nTexLodErr2++; }

	if(nTexLodErr1 > nTexLodErr2)
		return -1;
	else if(nTexLodErr1 < nTexLodErr2)
		return 1;
*/
	// if same - give closest sectors higher priority
	if(p1->m_arrfDistance[p1->m_nRenderStackLevel] > p2->m_arrfDistance[p1->m_nRenderStackLevel])
		return 1;
	else if(p1->m_arrfDistance[p1->m_nRenderStackLevel] < p2->m_arrfDistance[p1->m_nRenderStackLevel])
		return -1;

	return 0;
}

float GetPointToBoxDistance(Vec3 vPos, AABB bbox);

void CTerrain::DrawVisibleSectors()
{
  FUNCTION_PROFILER_3DENGINE;

  if(!GetCVars()->e_terrain || !Get3DEngine()->m_bShowTerrainSurface)
    return;

	// sort to get good texture streaming priority
	if(m_lstVisSectors.Count())
		qsort(m_lstVisSectors.GetElements(), m_lstVisSectors.Count(), sizeof(m_lstVisSectors[0]), CTerrain__Cmp_Sectors);

  for(int i=0; i<m_lstVisSectors.Count(); i++)
  {
    CTerrainNode * pNode = (CTerrainNode *)m_lstVisSectors[i];
		pNode->RenderNodeHeightmap();
  }
}

void CTerrain::RenderAOSectors()
{
  if(m_nRenderStackLevel)
    return;

  FUNCTION_PROFILER_3DENGINE;

  static PodArray<SSectorTextureSet> AOTexNodes; AOTexNodes.Clear();

  if(GetCVars()->e_terrain_ao && (m_hdrDiffTexInfo.dwFlags == TTFHF_AO_DATA_IS_VALID))
  {
    float fRange = 1024;

    Vec3 vCamPos = GetCamera().GetPosition();
    AABB areaBox(vCamPos - Vec3(fRange,fRange,fRange), vCamPos + Vec3(fRange,fRange,fRange));

    static PodArray<CTerrainNode*> AONodes; AONodes.Clear();
    m_pParentNode->GetTerrainAOTextureNodesInBox(areaBox, &AONodes);

//    AONodes.Clear();
  //  AONodes.AddList(m_lstVisSectors);

    for(int i=0; i<AONodes.Count(); i++)
    {
      if(AONodes[i]->m_arrfDistance[m_nRenderStackLevel]<fRange)
      {
        CTerrainNode * pTexNode = AONodes[i]->GetReadyTexSourceNode(0,ett_LM);
        if(!pTexNode || !pTexNode->m_nNodeTexSet.nTex0)
          continue;

        if(GetCVars()->e_terrain_draw_this_sector_only)
        {
          if(
            GetCamera().GetPosition().x > AONodes[i]->m_boxHeigtmap.max.x || 
            GetCamera().GetPosition().x < AONodes[i]->m_boxHeigtmap.min.x ||
            GetCamera().GetPosition().y > AONodes[i]->m_boxHeigtmap.max.y || 
            GetCamera().GetPosition().y < AONodes[i]->m_boxHeigtmap.min.y )
            continue;
        }

        AABB stencilBox;
        stencilBox = AONodes[i]->GetBBoxVirtual();
        stencilBox.min.z -= 08.f;
        stencilBox.max.z += 32.f;

        if(GetCamera().IsAABBVisible_E(stencilBox))
        {
          AOTexNodes.Add(pTexNode->m_nNodeTexSet);
          AOTexNodes.Last().stencilBox[0] = stencilBox.min;
          AOTexNodes.Last().stencilBox[1] = stencilBox.max;
          AOTexNodes.Last().fTexScale = pTexNode->m_nNodeTexSet.fTexScale;

          if(GetCVars()->e_terrain_ao>1)
            DrawBBox(AONodes[i]->GetBBoxVirtual());
        }
      }
    }
  }

  GetRenderer()->SetTerrainAONodes(&AOTexNodes);
}