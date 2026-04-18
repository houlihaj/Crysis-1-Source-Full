////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_node.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: terrain node
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain_sector.h"
#include "terrain.h"
#include "ObjMan.h"
#include "VisAreas.h"

#define TERRAIN_NODE_CHUNK_VERSION 5

int CTerrainNode::Load(uchar * & f, int & nDataSize) { return Load_T(f, nDataSize); }
int CTerrainNode::Load(FILE * & f, int & nDataSize) { return Load_T(f, nDataSize); }

int CTerrainNode::FTell(uchar * & f) { return -1; }
int CTerrainNode::FTell(FILE * & f) { return GetPak()->FTell(f); }

template<class T> 
int CTerrainNode::Load_T(T * & f, int & nDataSize)
{
  // set node data
  STerrainNodeChunk chunk;// = StepData<STerrainNodeChunk>(pData, &nDataSize);
  if(!CTerrain::LoadDataFromFile(&chunk, 1, f, nDataSize))
    return 0;
  assert(chunk.nChunkVersion == TERRAIN_NODE_CHUNK_VERSION);
  if(chunk.nChunkVersion != TERRAIN_NODE_CHUNK_VERSION)
    return 0;

  // set error levels, bounding boxes and some flags
  m_boxHeigtmap = chunk.boxHeightmap;
  m_boxHeigtmap.max.z = max(m_boxHeigtmap.max.z, GetTerrain()->GetWaterLevel());
  m_bHasHoles = chunk.bHasHoles;
  m_rangeInfo.fOffset = chunk.fOffset;
  m_rangeInfo.fRange = chunk.fRange;
  m_rangeInfo.nSize = chunk.nSize;
  m_rangeInfo.pHMData = NULL;
  m_rangeInfo.UpdateBitShift(GetTerrain()->m_nUnitsToSectorBitShift);
  m_rangeInfo.nModified = false;

  m_nNodeHMDataOffset = -1;
  if(m_rangeInfo.nSize)
  {
    m_rangeInfo.pHMData = new ushort[m_rangeInfo.nSize*m_rangeInfo.nSize];
    m_nNodeHMDataOffset = FTell(f);
    if(!CTerrain::LoadDataFromFile(m_rangeInfo.pHMData, m_rangeInfo.nSize*m_rangeInfo.nSize, f, nDataSize))
      return 0;
  }

  // load LOD errors
  delete [] m_pGeomErrors;
  m_pGeomErrors = new float[GetTerrain()->m_nUnitsToSectorBitShift];
  //	float * pErrors = StepData<float>(pData, &nDataSize, GetTerrain()->m_nUnitsToSectorBitShift);
  //	memcpy(m_pGeomErrors, pErrors, GetTerrain()->m_nUnitsToSectorBitShift*sizeof(m_pGeomErrors[0]));
  if(!CTerrain::LoadDataFromFile(m_pGeomErrors, GetTerrain()->m_nUnitsToSectorBitShift, f, nDataSize))
    return 0;
  assert(m_pGeomErrors[0] == 0);

  // load used surf types
  for(int i=0; i<m_lstSurfaceTypeInfo.Count(); i++)
    m_lstSurfaceTypeInfo[i].DeleteRenderMeshes(GetRenderer());
  m_lstSurfaceTypeInfo.Clear();
  m_lstSurfaceTypeInfo.PreAllocate(chunk.nSurfaceTypesNum,chunk.nSurfaceTypesNum);
  uchar * pTypes = new uchar[chunk.nSurfaceTypesNum];//StepData<uchar>(pData, &nDataSize, chunk.nSurfaceTypesNum);
  if(!CTerrain::LoadDataFromFile(pTypes, chunk.nSurfaceTypesNum, f, nDataSize))
    return 0;

  for(int i=0; i<m_lstSurfaceTypeInfo.Count(); i++)
    m_lstSurfaceTypeInfo[i].pSurfaceType = &GetTerrain()->m_SSurfaceType[pTypes[i]];

  delete [] pTypes;

  // count number of nodes saved
  int nNodesNum = 1;

  // process childs
  for(int i=0; m_arrChilds[0] && i<4; i++)
    nNodesNum += m_arrChilds[i]->Load_T(f, nDataSize);

  return nNodesNum;
}

int CTerrainNode::ReloadModifiedHMData(FILE * f)
{
  if(m_rangeInfo.nSize && m_nNodeHMDataOffset>=0 && m_rangeInfo.nModified)
  {
    m_rangeInfo.nModified = false;

    GetPak()->FSeek(f, m_nNodeHMDataOffset, SEEK_SET);
    int nDataSize = m_rangeInfo.nSize*m_rangeInfo.nSize*sizeof(m_rangeInfo.pHMData[0]);
    if(!CTerrain::LoadDataFromFile(m_rangeInfo.pHMData, m_rangeInfo.nSize*m_rangeInfo.nSize, f, nDataSize))
      return 0;
  }

  // process childs
  for(int i=0; m_arrChilds[0] && i<4; i++)
    m_arrChilds[i]->ReloadModifiedHMData(f);

  return 1;
}

int CTerrainNode::GetData(byte * & pData, int & nDataSize)
{
	if(pData)
	{
		// get node data
		STerrainNodeChunk * pCunk = (STerrainNodeChunk *)pData;
		pCunk->nChunkVersion = TERRAIN_NODE_CHUNK_VERSION;
		pCunk->boxHeightmap = m_boxHeigtmap;
		pCunk->bHasHoles = m_bHasHoles;	
		pCunk->fOffset = m_rangeInfo.fOffset;
		pCunk->fRange = m_rangeInfo.fRange;
		pCunk->nSize = m_rangeInfo.nSize;
		pCunk->nSurfaceTypesNum = m_lstSurfaceTypeInfo.Count();
		UPDATE_PTR_AND_SIZE(pData,nDataSize,sizeof(STerrainNodeChunk));

		// get heightmap data
		int nBlockSize = m_rangeInfo.nSize*m_rangeInfo.nSize*sizeof(m_rangeInfo.pHMData[0]);
		memcpy(pData, m_rangeInfo.pHMData, nBlockSize);
		UPDATE_PTR_AND_SIZE(pData,nDataSize,nBlockSize);

		// get heightmap error data
		memcpy(pData, m_pGeomErrors, GetTerrain()->m_nUnitsToSectorBitShift*sizeof(m_pGeomErrors[0]));
		UPDATE_PTR_AND_SIZE(pData,nDataSize,GetTerrain()->m_nUnitsToSectorBitShift*sizeof(m_pGeomErrors[0]));

		// get used surf types
		uchar arrTmp[MAX_TERRAIN_SURFACE_TYPES];
		for(int i=0; i<MAX_TERRAIN_SURFACE_TYPES && i<m_lstSurfaceTypeInfo.Count(); i++)
			arrTmp[i] = m_lstSurfaceTypeInfo[i].pSurfaceType->ucThisSurfaceTypeId;
		memcpy(pData, arrTmp, m_lstSurfaceTypeInfo.Count()*sizeof(uchar));
		UPDATE_PTR_AND_SIZE(pData,nDataSize,m_lstSurfaceTypeInfo.Count()*sizeof(uchar));
	}
	else // just count size
	{
		nDataSize += sizeof(STerrainNodeChunk);
		nDataSize += m_rangeInfo.nSize*m_rangeInfo.nSize*sizeof(m_rangeInfo.pHMData[0]);
		nDataSize += GetTerrain()->m_nUnitsToSectorBitShift*sizeof(m_pGeomErrors[0]);
		nDataSize += m_lstSurfaceTypeInfo.Count()*sizeof(uchar);
	}

	// count number of nodes saved
	int nNodesNum = 1;

	// process childs
	for(int i=0; m_arrChilds[0] && i<4; i++)
		nNodesNum += m_arrChilds[i]->GetData(pData, nDataSize);

	return nNodesNum;
}

