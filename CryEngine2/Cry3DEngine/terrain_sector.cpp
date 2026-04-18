////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_sector.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: sector initialiazilation, objects rendering
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain_sector.h"
#include "terrain.h"
#include "ObjMan.h"

int CTerrainNode::GetMML(int nDist, int mmMin, int mmMax)
{
  const int nStep = 48;

  for(int i=mmMin; i<mmMax; i++)
    if(nStep<<i > nDist)
      return i;

  return mmMax;
}

void CTerrainNode::SetLOD()
{
	// Calculate geometry LOD
	const float fDist = m_arrfDistance[m_nRenderStackLevel];

  bool bHasHoles = m_bHasHoles;
  
  if(!bHasHoles && m_arrChilds[0])// && GetCVars()->e_terrain_deformations)
  {
    for(int i=0; i<4; i++)
    {
      if(m_arrChilds[i]->m_bHasHoles)
      {
        bHasHoles = true;
        break;
      }
    }
  }

  if(bHasHoles || fDist < CTerrain::GetSectorSize()+(CTerrain::GetSectorSize()>>2))
    m_cNewGeomMML = 0;
  else
  {
    float fAllowedError = ( m_fZoomFactor * GetCVars()->e_terrain_lod_ratio * fDist ) / 180.f * 2.5f;

		int nGeomMML;
    for( nGeomMML=GetTerrain()->m_nUnitsToSectorBitShift-1; nGeomMML>m_rangeInfo.nUnitBitShift; nGeomMML-- )
      if(m_pGeomErrors[nGeomMML] < fAllowedError)
        break;

		m_cNewGeomMML = min(nGeomMML, int(fDist/32));
  }

	// set right LOD around voxels
	uchar nVoxLod = min(m_cNewGeomMML, uchar(2/CTerrain::GetHeightMapUnitSize()-1));

	if(m_bHasLinkedVoxel)
		m_cNewGeomMML = uchar(2/CTerrain::GetHeightMapUnitSize()-1);

	if(nVoxLod<m_cNewGeomMML)
	{
		CTerrainNode * pNodel = GetTerrain()->GetSecInfo(m_nOriginX-GetTerrain()->m_nSectorSize, m_nOriginY);
		CTerrainNode * pNoder = GetTerrain()->GetSecInfo(m_nOriginX+GetTerrain()->m_nSectorSize, m_nOriginY);
		CTerrainNode * pNodeb = GetTerrain()->GetSecInfo(m_nOriginX, m_nOriginY-GetTerrain()->m_nSectorSize);
		CTerrainNode * pNodet = GetTerrain()->GetSecInfo(m_nOriginX, m_nOriginY+GetTerrain()->m_nSectorSize);
		if(pNodel && pNodel->m_bHasLinkedVoxel)
			m_cNewGeomMML = nVoxLod;
		if(pNoder && pNoder->m_bHasLinkedVoxel)
			m_cNewGeomMML = nVoxLod;
		if(pNodeb && pNodeb->m_bHasLinkedVoxel)
			m_cNewGeomMML = nVoxLod;
		if(pNodet && pNodet->m_bHasLinkedVoxel)
			m_cNewGeomMML = nVoxLod;
	}

	// Calculate Texture LOD
	if(!m_nRenderStackLevel)
		m_cNodeNewTexMML = GetTextureLOD(fDist);
}

uchar CTerrainNode::GetTextureLOD(float fDistance)
{
	int nDiffTexDim = GetTerrain()->m_TerrainTextureLayer[0].nSectorSizePixels;

	float fTexSizeK = nDiffTexDim ? float(nDiffTexDim)/float(GetTerrain()->GetTerrainTextureNodeSizeMeters()) : 1.f;

	uchar cNodeNewTexMML = GetMML(int(fTexSizeK*0.05f*(fDistance*m_fZoomFactor)*GetCVars()->e_terrain_texture_lod_ratio), 0, 
		m_bMergeNotAllowed ? 0 : GetTerrain()->m_pParentNode->m_nTreeLevel);

	return cNodeNewTexMML;
}

/*
BOOL CTerrainNode::IsCanBeReflected()
{
  assert(GetTerrain()->m_pViewCamera);

  if(GetTerrain()->m_pViewCamera->GetPos().x<0 || GetTerrain()->m_pViewCamera->GetPos().y<0 || GetTerrain()->m_pViewCamera->GetPos().x>=CTerrain::GetTerrainSize() || GetTerrain()->m_pViewCamera->GetPos().y>=CTerrain::GetTerrainSize())
    return 1;

  float dx = (m_nOriginX+CTerrain::GetSectorSize()_2) - GetTerrain()->m_pViewCamera->GetPos().x;
  float dy = (m_nOriginY+CTerrain::GetSectorSize()_2) - GetTerrain()->m_pViewCamera->GetPos().y;

  float tests_num = m_arrfDistance[m_nRenderStackLevel]/16;

  dx /= tests_num;
  dy /= tests_num;

  float x = GetTerrain()->m_pViewCamera->GetPos().x;
  float y = GetTerrain()->m_pViewCamera->GetPos().y;

  for(int i=0; i<tests_num; i++)
  {
    CTerrainNode * sector = GetSectorFromPoint(fastftol(x), fastftol(y));
    if(sector->minZ<CTerrain::GetWaterLevel())
      return 1;

    x += dx;
    y += dy;
  }
  
  return 0;
}*/

/*void CTerrainNode::SetHeightmapAABBAndHoleFlag()
{
  m_bHasHoles = false;

	float fMinHeightMapZ, fMaxHeightMapZ;
  fMinHeightMapZ = fMaxHeightMapZ = GetTerrain()->GetZ(m_nOriginX,m_nOriginY);

	int nUnitSize = CTerrain::GetHeightMapUnitSize();

  // calculate min, max, mid, hole
  for( int x=0; x<=CTerrain::GetSectorSize(); x+=nUnitSize)
  {
    for( int y=0; y<=CTerrain::GetSectorSize(); y+=nUnitSize)
    {
			float fZ = GetTerrain()->GetZ(m_nOriginX+x,m_nOriginY+y);
      
			if(fMaxHeightMapZ < fZ)
        fMaxHeightMapZ = fZ;
      else if(fMinHeightMapZ > fZ)
        fMinHeightMapZ = fZ;

      if(GetTerrain()->GetHole(m_nOriginX+x,m_nOriginY+y))
        m_bHasHoles = true;
    }
  }

	// init ground bbox, it will include only heightmap
	m_boxHeigtmap.min = Vec3((float)m_nOriginX, (float)m_nOriginY, fMinHeightMapZ);
	m_boxHeigtmap.max = Vec3((float)m_nOriginX+CTerrain::GetSectorSize(),
													 (float)m_nOriginY+CTerrain::GetSectorSize(), fMaxHeightMapZ);

	// is needed for water visibility detection
	if(m_boxHeigtmap.max.z < GetTerrain()->GetWaterLevel())
		m_boxHeigtmap.max.z = GetTerrain()->GetWaterLevel();
}*/
/*
void CTerrainNode::BuildErrorsTable()
{
	memset(m_arrGeomErrors,0,sizeof(m_arrGeomErrors));
	int nUnitSize = CTerrain::GetHeightMapUnitSize();
	int nSectorSize = CTerrain::GetSectorSize();
	int nTerrainSize = CTerrain::GetTerrainSize();

	for(int nLod=0; nLod<MAX_GEOM_MML_LEVELS; nLod++)
	{ // calculate max difference between detail levels and actual height map
		float fMaxDiff = 0;

		int nCellSize = (1<<nLod)*nUnitSize;
		if(nCellSize <= nSectorSize)
		{
			int x1 = max(0,m_nOriginX-nCellSize);
			int x2 = min(nTerrainSize,m_nOriginX+nSectorSize+nCellSize);
			int y1 = max(0,m_nOriginY-nCellSize);
			int y2 = min(nTerrainSize,m_nOriginY+nSectorSize+nCellSize);

			for(int X=x1; X<x2; X+=nCellSize) for(int Y=y1; Y<y2; Y+=nCellSize)
			{
				for( int x=0; x<=nCellSize; x+=nUnitSize)
				{
					float kx = (float)x/(float)nCellSize;

					float z1 = (1.f-kx)*GetTerrain()->GetZ(X+0,Y+        0) + (kx)*GetTerrain()->GetZ(X+nCellSize,Y+        0);
					float z2 = (1.f-kx)*GetTerrain()->GetZ(X+0,Y+nCellSize) + (kx)*GetTerrain()->GetZ(X+nCellSize,Y+nCellSize);

					for( int y=0; y<=nCellSize; y+=nUnitSize)
					{
						// skip map borders
						int nBorder = (nSectorSize>>2);
						if((X+x) < nBorder || (Y+y) < nBorder)
							continue;
						if((X+x) > (nTerrainSize-nBorder) || (Y+y) > (nTerrainSize-nBorder))
							continue;

						float ky = (float)y/nCellSize;
						float fInterpolatedZ = (1.f-ky)*z1 + ky*z2;
						float fRealZ = GetTerrain()->GetZ(X+x,Y+y);
						float fDiff = fabs(fRealZ-fInterpolatedZ);

						if(fDiff > fMaxDiff)
							fMaxDiff = fDiff;
					}
				}
			}
			// note: values in m_arrGeomErrors table may be non incremental - this is correct
			m_arrGeomErrors[nLod] = fMaxDiff;
		}
		else
			m_arrGeomErrors[nLod] = 1000000.f; // this lod is not supported for current sector and unit sizes
	}
}
*/
/*
void CTerrainNode::InitSectorBoundsAndErrorLevels(bool bBuildErrorsTable)
{
	if(bBuildErrorsTable)
	{
		SetHeightmapAABBAndHoleFlag();
		BuildErrorsTable();
	}
	else
		memset(m_arrGeomErrors,0,sizeof(m_arrGeomErrors));

  // set m_arrfDistance[m_nRenderStackLevel] to very far to force loading only low res texture first time
  m_arrfDistance[m_nRenderStackLevel] = 2.f*CTerrain::GetTerrainSize();
  assert(m_arrfDistance[m_nRenderStackLevel]);

  m_nLastTimeUsed = (uint)-1;
}
*/
int CTerrainNode::GetSecIndex() 
{ 
	int nSectorSize = CTerrain::GetSectorSize()<<m_nTreeLevel;
	int nSectorsTableSize = CTerrain::GetSectorsTableSize()>>m_nTreeLevel;
	return (m_nOriginX/nSectorSize)*nSectorsTableSize + (m_nOriginY/nSectorSize); 
}
/*
void CTerrainNode::UpdateObjectsMaxVewDist(int nObjType, float fSomeObjMaxViewDist)
{
	assert(nObjType>=0 && nObjType<3);

	CTerrainNode * pNode = this;
	while(pNode)
	{
		if(pNode->m_arrfObjectsMaxViewDist[nObjType] < fSomeObjMaxViewDist)
		{
			pNode->m_arrfObjectsMaxViewDist[nObjType] = fSomeObjMaxViewDist;
			pNode->m_arrbEntitiesCompiled[nObjType] = false;
			pNode = pNode->m_pParent;
		}
		else
			break;
	}
}*/

void CTerrainNode::CheckInitAffectingLights()
{
  if(m_nLightMaskFrameId != GetFrameID())
  {
    m_lstAffectingLights.Clear();
    PodArray<CDLight> * pSceneLights = Get3DEngine()->GetDynamicLightSources();
    if(pSceneLights->Count() && (pSceneLights->Get(0)->m_Flags & DLF_SUN))
      m_lstAffectingLights.Add(pSceneLights->Get(0));

    m_nLightMaskFrameId = GetFrameID();
  }
}

PodArray<CDLight*> * CTerrainNode::GetAffectingLights()
{ 
  CheckInitAffectingLights();

	return &m_lstAffectingLights; 
}

void CTerrainNode::AddLightSource(CDLight * pSource)
{ 
  CheckInitAffectingLights();

	m_lstAffectingLights.Add(pSource); 
}
