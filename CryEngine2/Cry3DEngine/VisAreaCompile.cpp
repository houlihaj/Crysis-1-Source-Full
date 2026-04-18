////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   VisAreaCompile.cpp
//  Version:     v1.00
//  Created:     28/4/2005 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: visarea node loading/saving
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ObjMan.h"
#include "VisAreas.h"

#define VISAREA_NODE_CHUNK_VERSION 2

#define VISAREA_FLAG_OCEAN_VISIBLE 1
#define VISAREA_FLAG_IGNORE_SKY_COLOR 2

#pragma pack(push,1)

struct SVisAreaChunk
{
	int		nChunkVersion;
	AABB	boxArea,boxStatics;
	char	sName[32];
	int		nObjectsBlockSize;
#define MAX_VIS_AREA_CONNECTIONS_NUM 30
	int		arrConnectionsId[MAX_VIS_AREA_CONNECTIONS_NUM];
  uint  dwFlags;
  uint  dwReserved;
	Vec3	vConnNormals[2];
	float fHeight;
	Vec3  vAmbColor;
	bool  bAffectedByOutLights, bSkyOnly, bDoubleSide, bUseInIndoors;
	float fViewDistRatio;

	AUTO_STRUCT_INFO_LOCAL
};

#pragma pack(pop)

int CVisArea::GetData(byte * & pData, int & nDataSize, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable)
{
	if(m_pObjectsTree)
		m_pObjectsTree->CleanUpTree();

	if(pData)
	{
		// save node info
		SVisAreaChunk * pCunk = (SVisAreaChunk *)pData;
		pCunk->nChunkVersion = VISAREA_NODE_CHUNK_VERSION;
		pCunk->boxArea = m_boxArea;
		pCunk->boxStatics = m_boxStatics;
		strncpy(pCunk->sName, m_sName, sizeof(pCunk->sName));
		memcpy(pCunk->vConnNormals, m_vConnNormals, sizeof(pCunk->vConnNormals));
		pCunk->fHeight = m_fHeight;
		pCunk->vAmbColor = m_vAmbColor;
		pCunk->bAffectedByOutLights = m_bAffectedByOutLights; 
		pCunk->bSkyOnly = m_bSkyOnly; 
		pCunk->bDoubleSide = m_bDoubleSide;
		pCunk->bUseInIndoors = m_bUseInIndoors;
		pCunk->fViewDistRatio = m_fViewDistRatio;
    
    pCunk->dwFlags = 0;
    if(m_bOceanVisible)
      pCunk->dwFlags |= VISAREA_FLAG_OCEAN_VISIBLE;
    if(m_bIgnoreSky)
      pCunk->dwFlags |= VISAREA_FLAG_IGNORE_SKY_COLOR;

    pCunk->dwReserved = 0;

		// transform connections id into pointes
		PodArray<CVisArea*> & rAreas = IsPortal() ? GetVisAreaManager()->m_lstVisAreas : GetVisAreaManager()->m_lstPortals;
		for(int i=0; i<MAX_VIS_AREA_CONNECTIONS_NUM; i++)
			pCunk->arrConnectionsId[i] = -1;
		for(int i=0; i<m_lstConnections.Count() && i<MAX_VIS_AREA_CONNECTIONS_NUM; i++)
		{
			IVisArea * pArea = m_lstConnections[i];
			int nId;
			for(nId=0; nId<rAreas.Count(); nId++)
			{
				if(pArea == rAreas[nId])
					break;
			}

			if(nId<rAreas.Count())
				pCunk->arrConnectionsId[i] = nId;
			else
			{
				pCunk->arrConnectionsId[i] = -1;
				assert(!"Undefined connction");
			}
		}

		//assert(i<MAX_VIS_AREA_CONNECTIONS_NUM);

		UPDATE_PTR_AND_SIZE(pData,nDataSize,sizeof(SVisAreaChunk));

		// save shape points num
		int nPointsCount = m_lstShapePoints.Count();
		memcpy(pData, &nPointsCount, sizeof(nPointsCount)); 
		UPDATE_PTR_AND_SIZE(pData,nDataSize,sizeof(nPointsCount));

		// save shape points
		memcpy(pData, m_lstShapePoints.GetElements(), m_lstShapePoints.GetDataSize()); 
		UPDATE_PTR_AND_SIZE(pData,nDataSize,m_lstShapePoints.GetDataSize());

		// save objects
		pCunk->nObjectsBlockSize = 0;

		// get data from objects tree
		if(m_pObjectsTree)
		{
			byte * pTmp = NULL;
			m_pObjectsTree->GetData(pTmp, pCunk->nObjectsBlockSize, NULL, NULL);
			m_pObjectsTree->GetData(pData, nDataSize, pStatObjTable, pMatTable); // UPDATE_PTR_AND_SIZE is inside
		}
	}
	else // just count size
	{
		nDataSize += sizeof(SVisAreaChunk);

		nDataSize += sizeof(int);
		nDataSize += m_lstShapePoints.GetDataSize();

		if(m_pObjectsTree)
		{
			int nObjectsBlockSize = 0;
			m_pObjectsTree->GetData(pData, nObjectsBlockSize, NULL, NULL);
			nDataSize += nObjectsBlockSize;
		}
	}

	return true;
}

int CVisArea::Load(FILE * & f, int & nDataSizeLeft, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable)
{
  SVisAreaChunk chunk;
  if(!CTerrain::LoadDataFromFile(&chunk, 1, f, nDataSizeLeft))
    return 0;

	assert(chunk.nChunkVersion == VISAREA_NODE_CHUNK_VERSION);
	if(chunk.nChunkVersion != VISAREA_NODE_CHUNK_VERSION)
		return 0;

	// get area info
	m_boxArea = chunk.boxArea;
	m_boxStatics = chunk.boxStatics;
	strncpy(m_sName, chunk.sName, sizeof(m_sName));		
	m_bThisIsPortal = strstr(m_sName,"portal") != 0;
  m_bIgnoreSky = (strstr(m_sName,"ignoresky") != 0) || ((chunk.dwFlags & VISAREA_FLAG_IGNORE_SKY_COLOR) != 0);
	memcpy(m_vConnNormals, chunk.vConnNormals, sizeof(m_vConnNormals));
	m_fHeight = chunk.fHeight;
	m_vAmbColor = chunk.vAmbColor;
	m_bAffectedByOutLights = chunk.bAffectedByOutLights; 
	m_bSkyOnly = chunk.bSkyOnly; 
	m_bDoubleSide = chunk.bDoubleSide;
	m_bUseInIndoors = chunk.bUseInIndoors;
	m_fViewDistRatio = chunk.fViewDistRatio;
  
  if(chunk.dwFlags == uint(-1))
    chunk.dwFlags = 0;
  m_bOceanVisible = (chunk.dwFlags & VISAREA_FLAG_OCEAN_VISIBLE) != 0;

	// convert connections id into pointers
	PodArray<CVisArea*> & rAreas = IsPortal() ? GetVisAreaManager()->m_lstVisAreas : GetVisAreaManager()->m_lstPortals;
	for(int i=0; i<MAX_VIS_AREA_CONNECTIONS_NUM && rAreas.Count(); i++)
	{
    assert(chunk.arrConnectionsId[i] < rAreas.Count());
		if(chunk.arrConnectionsId[i] >= 0)
			m_lstConnections.Add(rAreas[chunk.arrConnectionsId[i]]);
	}

  { // get shape points
    int nPointsCount = 0;
    if(!CTerrain::LoadDataFromFile(&nPointsCount, 1, f, nDataSizeLeft))
      return 0;

    // get shape points
    m_lstShapePoints.PreAllocate(nPointsCount,nPointsCount);
    if(!CTerrain::LoadDataFromFile(m_lstShapePoints.GetElements(), nPointsCount, f, nDataSizeLeft))
      return 0;
  }

  // mark tree as invalid since new visarea was just added
  SAFE_DELETE(GetVisAreaManager()->m_pAABBTree);

  // load content of objects tree
	if(!m_bEditorMode && chunk.nObjectsBlockSize>4)
	{
		int nCurDataSize = nDataSizeLeft;
    if (nCurDataSize > 0)
    {
      if(!m_pObjectsTree)
        m_pObjectsTree = new COctreeNode(m_boxArea, this);
      m_pObjectsTree->Load(f, nDataSizeLeft, pStatObjTable, pMatTable);
      assert(nDataSizeLeft == (nCurDataSize-chunk.nObjectsBlockSize));
    }
	}

  UpdateOcclusionFlagInTerrain();

	return true;
}

const AABB* CVisArea::GetAABBox() const
{
	return &m_boxArea;
}

const AABB* CVisArea::GetObjectsBBox() const
{
	if (m_pObjectsTree)
	{
		return &m_pObjectsTree->GetObjectsBBox();
	}
	
	return &m_boxArea;
}

#include "TypeInfo_impl.h"
#include "VisAreaCompile_info.h"


