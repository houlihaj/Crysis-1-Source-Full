////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_sector_tex.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: terrain texture management
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain_sector.h"
#include "terrain.h"
#include "ObjMan.h"

// load textures from buffer
int CTerrainNode::CreateSectorTexturesFromBuffer()
{
	FUNCTION_PROFILER_3DENGINE;

	STerrainTextureLayerFileHeader * pLayers = GetTerrain()->m_TerrainTextureLayer;

	uint32 nSectorDataSize = pLayers[0].nSectorSizeBytes + pLayers[1].nSectorSizeBytes;

	// make textures
	if(m_nEditorDiffuseTex)
		m_nNodeTexSet.nTex0 = m_nEditorDiffuseTex;
	else
		m_nNodeTexSet.nTex0 = m_pTerrain->m_texCache[0].GetTexture(GetTerrain()->m_ucpDiffTexTmpBuffer);

	// make AO textures
	if(m_pTerrain->m_texCache[1].GetPoolSize())
		m_nNodeTexSet.nTex1 = m_pTerrain->m_texCache[1].GetTexture(GetTerrain()->m_ucpDiffTexTmpBuffer+pLayers[0].nSectorSizeBytes);
	else
		m_nNodeTexSet.nTex1 = 0;

	if(GetCVars()->e_terrain_texture_streaming_debug==2)
		PrintMessage("CTerrainNode::CreateSectorTexturesFromBuffer: sector %d, level=%d", GetSecIndex(), m_nTreeLevel);

	assert(m_nNodeTexSet.nTex0);

  return (m_nNodeTexSet.nTex0 > 0);
}

void CTerrainNode::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	if(pStream->IsError())
	{ // file was not loaded successfully
		Error("CTerrainNode::StreamOnComplete: sector %d, level=%d", GetSecIndex(), m_nTreeLevel);
		assert(0);
		m_eTexStreamingStatus = ecss_NotLoaded;
		m_pReadStream = NULL;
		return;
	}
	
	CreateSectorTexturesFromBuffer();

	{ // this will be used for WSAO
		float fSectorSizeScale = 1.0f;
		if(GetTerrain()->m_TerrainTextureLayer[0].nSectorSizePixels)
			fSectorSizeScale -= 1.0f/(float)(GetTerrain()->m_TerrainTextureLayer[0].nSectorSizePixels);	// we don't use half texel border so we have to compensate 
		float dCSS = fSectorSizeScale/(CTerrain::GetSectorSize()<<m_nTreeLevel);
		m_nNodeTexSet.fTexOffsetX = -dCSS*m_nOriginY;
		m_nNodeTexSet.fTexOffsetY = -dCSS*m_nOriginX;
		m_nNodeTexSet.fTexScale = dCSS;
		m_nNodeTexSet.fTerrainMinZ = m_boxHeigtmap.min.z;
		m_nNodeTexSet.fTerrainMaxZ = m_boxHeigtmap.max.z;
    m_nNodeTexSet.nodeBox[0] = m_boxHeigtmap.min;
    m_nNodeTexSet.nodeBox[1] = m_boxHeigtmap.max;
		// shift texture by 0.5 pixel
		if(GetTerrain()->m_TerrainTextureLayer[0].nSectorSizePixels)
		{
			m_nNodeTexSet.fTexOffsetX += 0.5f/GetTerrain()->m_TerrainTextureLayer[0].nSectorSizePixels;
			m_nNodeTexSet.fTexOffsetY += 0.5f/GetTerrain()->m_TerrainTextureLayer[0].nSectorSizePixels;
		}
	}

	m_eTexStreamingStatus = ecss_Ready;

	m_pReadStream = NULL;
}

void CTerrainNode::StartSectorTexturesStreaming(bool bFinishNow)
{
	assert(m_eTexStreamingStatus == ecss_NotLoaded);

	if(!GetTerrain()->m_nDiffTexIndexTableSize)
		return; // file was not opened

	STerrainTextureLayerFileHeader * pLayers = GetTerrain()->m_TerrainTextureLayer;

	uint32 nSectorDataSize = pLayers[0].nSectorSizeBytes + pLayers[1].nSectorSizeBytes;

	// calculate sector data offset in file
	int nFileOffset = sizeof(SCommonFileHeader)+sizeof(STerrainTextureFileHeader)
		+GetTerrain()->m_hdrDiffTexInfo.nLayerCount*sizeof(STerrainTextureLayerFileHeader)
		+GetTerrain()->m_nDiffTexIndexTableSize
		+m_nNodeTextureOffset*nSectorDataSize;

	// start streaming
	StreamReadParams params;
	params.nSize = (GetTerrain()->m_hdrDiffTexInfo.dwFlags == TTFHF_AO_DATA_IS_VALID) ? nSectorDataSize : pLayers[0].nSectorSizeBytes;
	params.pBuffer = GetTerrain()->m_ucpDiffTexTmpBuffer;
	params.nLoadTime = 1000;
	params.nMaxLoadTime = 0;
	params.nOffset = nFileOffset;

	m_pReadStream = GetSystem()->GetStreamEngine()->StartRead("3DEngine", 
		Get3DEngine()->GetLevelFilePath("terrain/cover.ctc"), this, &params);

	m_eTexStreamingStatus = ecss_InProgress;

	if(GetCVars()->e_terrain_texture_streaming_debug==2)
		PrintMessage("CTerrainNode::StartSectorTexturesStreaming: sector %d, level=%d, finish: %s", GetSecIndex(), m_nTreeLevel,bFinishNow ? "true" : "false");

	if(bFinishNow)
		m_pReadStream->Wait();
}
