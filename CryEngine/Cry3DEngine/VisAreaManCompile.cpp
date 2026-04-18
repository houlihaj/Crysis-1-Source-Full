////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   visareamancompile.cpp
//  Version:     v1.00
//  Created:     15/04/2005 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: check vis
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ObjMan.h"
#include "VisAreas.h"

bool CVisAreaManager::GetCompiledData(byte * pData, int nDataSize, std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable)
{
	// write header
	SVisAreaManChunkHeader * pVisAreaManagerChunkHeader = (SVisAreaManChunkHeader *)pData;
	pVisAreaManagerChunkHeader->nChunkVersion = VISAREAMANAGER_CHUNK_VERSION;
	pVisAreaManagerChunkHeader->nChunkSize = nDataSize;
	
	pVisAreaManagerChunkHeader->nVisAreasNum = m_lstVisAreas.Count();
	pVisAreaManagerChunkHeader->nPortalsNum = m_lstPortals.Count();
	pVisAreaManagerChunkHeader->nOcclAreasNum = m_lstOcclAreas.Count();

	UPDATE_PTR_AND_SIZE(pData,nDataSize,sizeof(SVisAreaManChunkHeader));

	for(int i=0; i<m_lstVisAreas.Count(); i++)
		m_lstVisAreas[i]->GetData(pData, nDataSize, *ppStatObjTable, *ppMatTable);

	for(int i=0; i<m_lstPortals.Count(); i++)
		m_lstPortals[i]->GetData(pData, nDataSize, *ppStatObjTable, *ppMatTable);

	for(int i=0; i<m_lstOcclAreas.Count(); i++)
		m_lstOcclAreas[i]->GetData(pData, nDataSize, *ppStatObjTable, *ppMatTable);

  SAFE_DELETE(*ppStatObjTable);
  SAFE_DELETE(*ppMatTable);

	assert(nDataSize==0);
	return nDataSize==0;
}

int CVisAreaManager::GetCompiledDataSize()
{
	int nDataSize = 0;
	byte * pData = NULL;

	// get header size
	nDataSize += sizeof(SVisAreaManChunkHeader);

	for(int i=0; i<m_lstVisAreas.Count(); i++)
		m_lstVisAreas[i]->GetData(pData, nDataSize, NULL, NULL);

	for(int i=0; i<m_lstPortals.Count(); i++)
		m_lstPortals[i]->GetData(pData, nDataSize, NULL, NULL);

	for(int i=0; i<m_lstOcclAreas.Count(); i++)
		m_lstOcclAreas[i]->GetData(pData, nDataSize, NULL, NULL);

	return nDataSize;
}

bool CVisAreaManager::Load(FILE * & f, int & nDataSize, SVisAreaManChunkHeader * pVisAreaManagerChunkHeader, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable)
{
	if( pVisAreaManagerChunkHeader->nChunkVersion != VISAREAMANAGER_CHUNK_VERSION ||
      pVisAreaManagerChunkHeader->nChunkSize != nDataSize )
	{
    Error("CVisAreaManager::SetCompiledData: verison of file is %d, expected version is %d", pVisAreaManagerChunkHeader->nChunkVersion, (int)VISAREAMANAGER_CHUNK_VERSION);
		return 0;
	}

  { // construct areas
	  m_lstVisAreas.PreAllocate(pVisAreaManagerChunkHeader->nVisAreasNum,pVisAreaManagerChunkHeader->nVisAreasNum);
	  m_lstPortals.PreAllocate(pVisAreaManagerChunkHeader->nPortalsNum,pVisAreaManagerChunkHeader->nPortalsNum);
	  m_lstOcclAreas.PreAllocate(pVisAreaManagerChunkHeader->nOcclAreasNum,pVisAreaManagerChunkHeader->nOcclAreasNum);
	  nDataSize -= sizeof(SVisAreaManChunkHeader);

	  for(int i=0; i<m_lstVisAreas.Count(); i++)
		  m_lstVisAreas[i] = new CVisArea();
	  for(int i=0; i<m_lstPortals.Count(); i++)
		  m_lstPortals[i] = new CVisArea();
	  for(int i=0; i<m_lstOcclAreas.Count(); i++)
		  m_lstOcclAreas[i] = new CVisArea();
  }

  { // load areas content
	  for(int i=0; i<m_lstVisAreas.Count(); i++)
		  m_lstVisAreas[i]->Load(f,nDataSize, pStatObjTable, pMatTable);

	  for(int i=0; i<m_lstPortals.Count(); i++)
		  m_lstPortals[i]->Load(f,nDataSize, pStatObjTable, pMatTable);

	  for(int i=0; i<m_lstOcclAreas.Count(); i++)
		  m_lstOcclAreas[i]->Load(f,nDataSize, pStatObjTable, pMatTable);
  }

	SAFE_DELETE(m_pAABBTree);

	return nDataSize==0;
}

