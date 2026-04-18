////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   TerrainTextureCache.cpp
//  Version:     v1.00
//  Created:     28/1/2007 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: terrain texture management
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain_sector.h"
#include "terrain.h"

CTextureCache::CTextureCache()
{
	m_eTexFormat = eTF_Unknown;
	m_nDim = 0;
}

CTextureCache::~CTextureCache()
{
	if(GetPoolSize())
		assert(m_FreeTextures.Count() + m_Quarantine.Count() + m_UsedTextures.Count() == TERRAIN_TEX_CACHE_POOL_SIZE);

	ResetTexturePool();
}

void CTextureCache::ResetTexturePool()
{
	m_FreeTextures.AddList(m_UsedTextures); m_UsedTextures.Clear();
	m_FreeTextures.AddList(m_Quarantine); m_Quarantine.Clear();
	for(int i=0; i<m_FreeTextures.Count(); i++)
		GetRenderer()->RemoveTexture(m_FreeTextures[i]);
	m_FreeTextures.Clear();
}

uint CTextureCache::GetTexture(byte * pData)
{
  if(!m_FreeTextures.Count())
  	Update();
  
  if(!m_FreeTextures.Count())
	{
		Error("CTextureCache::GetTexture: !m_FreeTextures.Count()");
		return 0;
	}

	int nTexId = m_FreeTextures.Last();

	m_FreeTextures.DeleteLast();

	nTexId = GetRenderer()->DownLoadToVideoMemory(pData, m_nDim, m_nDim, m_eTexFormat, m_eTexFormat, 0, false, FILTER_NONE, nTexId);

	m_UsedTextures.Add(nTexId);

	assert(m_FreeTextures.Count() + m_Quarantine.Count() + m_UsedTextures.Count() == TERRAIN_TEX_CACHE_POOL_SIZE);

	return nTexId;
}

void CTextureCache::ReleaseTexture(uint nTexId)
{
	if(m_UsedTextures.Delete(nTexId))
		m_Quarantine.Add(nTexId);
	else
		assert(!"Attempt to release non pooled texture");

	assert(m_FreeTextures.Count() + m_Quarantine.Count() + m_UsedTextures.Count() == TERRAIN_TEX_CACHE_POOL_SIZE);
}

bool CTextureCache::Update()
{
	m_FreeTextures.AddList(m_Quarantine);
	m_Quarantine.Clear();

	return GetPoolSize() == TERRAIN_TEX_CACHE_POOL_SIZE;
}

int CTextureCache::GetPoolSize()
{
	return (m_FreeTextures.Count() + m_Quarantine.Count() + m_UsedTextures.Count());
}

void CTextureCache::InitPool(byte * pData, int nDim, ETEX_Format eTexFormat)
{
	if(m_eTexFormat != eTexFormat || m_nDim != nDim)
	{
		ResetTexturePool();

		m_eTexFormat = eTexFormat;
		m_nDim = nDim;

		for(int i=0; i<TERRAIN_TEX_CACHE_POOL_SIZE; i++)
		{
			uint nTexId = GetRenderer()->DownLoadToVideoMemory(pData, nDim, nDim, eTexFormat, eTexFormat, 0, false, FILTER_NONE);
			m_FreeTextures.Add(nTexId);

      if(nTexId<=0)
      {
				if (!gEnv->pSystem->IsDedicated())
	        Error("Debug: CTextureCache::InitPool: GetRenderer()->DownLoadToVideoMemory returned %d", nTexId);
        memset(pData, 0, nDim*nDim*sizeof(pData[0]));
				if (!gEnv->pSystem->IsDedicated())
	        Error("Debug: DownLoadToVideoMemory() params: dim=%d, eTexFormat=%d", nDim, eTexFormat);
      }
		}
	}

	assert(m_FreeTextures.Count() + m_Quarantine.Count() + m_UsedTextures.Count() == TERRAIN_TEX_CACHE_POOL_SIZE);
}