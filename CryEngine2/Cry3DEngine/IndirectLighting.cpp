////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   IndirectLighting.cpp
//  Version:     v1.00
//  Created:     13/6/2005 by Michael Glueck
//  Compilers:   Visual Studio.NET
//  Description: Implementation of Indirect Lighting engine
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "IndirectLighting.h"
#include "3dEngine.h"
#include "terrain.h"
#include "terrain_sector.h"
#include "BasicArea.h"
#include "ObjMan.h"
#include "Vegetation.h" 
#include "Brush.h"
#include "VoxMan.h"

CIndirectLighting::CIndirectLighting() 
{}

void CIndirectLighting::RetrieveTerrainAcc(uint8 *pDst, const uint32 cWidth, const uint32 cHeight)
{
	return;
/*
	if(!pDst)
		return;
	memset(pDst, 255, cWidth * cHeight);
	if(cWidth != cHeight || !GetTerrain())
		return;
	const uint32 cFullWidth = GetTerrain()->GetTerrainSize() * GetTerrain()->GetHeightMapUnitSize();
	if(cWidth > cFullWidth)
		return;
	const uint32 cScale = cFullWidth / cWidth;

	int32 scaleShift = 0;//replace / cScale by >> scaleShift
	while((1 << scaleShift) != cScale)
		++scaleShift;

	Array2d<uchar> lightMap;
	const Array2d<struct CTerrainNode*> *pArrSecInfoPyramid = GetTerrain()->m_arrSecInfoPyramid;
	for(int treeLevel = TERRAIN_NODE_TREE_DEPTH-1; treeLevel>=0; --treeLevel)
	{
		if(!pArrSecInfoPyramid[treeLevel].m_nSize)
			continue;
		for(int x=0; x<CTerrain::GetSectorsTableSize() >> treeLevel; ++x)
			for(int y=0; y<CTerrain::GetSectorsTableSize() >> treeLevel; ++y)
			{
				CTerrainNode * pNode = pArrSecInfoPyramid[treeLevel][x][y];
				if(pNode)
				{
					//set all data into the destination, deeper levels will refine the data
					Array2d<uchar>& rLightMap = pNode->m_AccMap;
					uint32 lightInfoSize = pNode->m_AccMap.GetSize();
					if(lightInfoSize == 0 && !pNode->GetLightmapData(lightMap))
						continue;
					if(lightInfoSize == 0)
						rLightMap = lightMap;
					const uint32 cSectorSizeInM = (uint32)(CTerrain::GetSectorSize() << treeLevel);
					const uint32 cOffsetX = (pNode->m_nOriginX >> scaleShift);
					const uint32 cOffsetY = (pNode->m_nOriginY >> scaleShift);
					const uint32 cSectorPixelCount = (cSectorSizeInM >> scaleShift);
					const float cPixelScale = (float)lightInfoSize / (float)cSectorPixelCount;
					for(uint32 lmX=0; lmX<cSectorPixelCount; ++lmX)
						for(uint32 lmY=0; lmY<cSectorPixelCount; ++lmY)
						{
							//set texel
							const uint32 cBakLightX = min((int)(lightInfoSize-1), (int)((float)lmX * cPixelScale + 0.5f));
							const uint32 cBakLightY = min((int)(lightInfoSize-1), (int)((float)lmY * cPixelScale + 0.5f));
							pDst[(cOffsetX + lmX) * cWidth + cOffsetY + lmY] = rLightMap[cBakLightX][cBakLightY];
						}
				}
			}
	}
	*/
}

void CIndirectLighting::GetTerrainVoxelInfo(SVoxelInfo *pVoxels, uint32& rCount)
{
	int count = 0;
	Get3DEngine()->GetVoxelRenderNodes(NULL, count);
	if(!pVoxels || count == 0)
	{
		rCount = count;
		return;//just retrieve count
	}
	std::vector<IRenderNode*> voxelRenderNodes(count);
	count = 0;
	rCount = 0;
	Get3DEngine()->GetVoxelRenderNodes(&voxelRenderNodes[0], count);
	const std::vector<IRenderNode*>::const_iterator cVoxelEnd = voxelRenderNodes.end();
	for(std::vector<IRenderNode*>::const_iterator iter = voxelRenderNodes.begin(); iter != cVoxelEnd; ++iter)
	{
		IRenderNode *pRenderNode = *iter;
		if(!pRenderNode || pRenderNode->GetEntityVisArea() != NULL)
			continue;
		SVoxelInfo& rVoxelInfo = pVoxels[rCount++];
		CVoxelObject *pVoxArea = (CVoxelObject*)pRenderNode;
		rVoxelInfo.pRenderNode = pRenderNode;
		rVoxelInfo.pRenderMesh = pVoxArea->GetRenderMesh(0);
		rVoxelInfo.mat34 = pVoxArea->GetMatrix();
	}
}