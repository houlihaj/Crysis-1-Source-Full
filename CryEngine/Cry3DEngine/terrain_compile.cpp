////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_compile.cpp
//  Version:     v1.00
//  Created:     15/04/2005 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: check vis
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"
#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "ObjectsTree.h"

#define SIGC_ALIGNTOTERRAIN		1
#define SIGC_USETERRAINCOLOR	2
#define SIGC_AFFECTED_BY_VOXELS 4
#define SIGC_MAT_ID_SHIFT			8

//! structure to vegetation group properties loading/saving
struct StatInstGroupChunk
{
	StatInstGroupChunk() 
	{ 
		nDummy_pStatObj = 0;
		szFileName[0]=0;
		bHideability = 0;
		bHideabilitySecondary = 0;
    bPickable = 0;
		fBending = 0;
		bComplexBending = 0;
		bCastShadow = 0;
		bRecvShadow = 0;
		bPrecShadow = true;
		bUseAlphaBlending = 0;
		fSpriteDistRatio = 1.f;
		fShadowDistRatio = 1.f;
		fMaxViewDistRatio= 1.f;
		fBrightness = 1.f;
		nFlagsAndMaterialId = 0;
		bUseSprites = true;
		bFadeSize_not_used = 0;
		fDensity=1;
		fElevationMax=4096;
		fElevationMin=8;
		fSize=1 ;
		fSizeVar=0 ;
		fSlopeMax=255 ;
		fSlopeMin=0 ;
		bRandomRotation = false;
    nMaterialLayers=0;
	}

	int nDummy_pStatObj;
	char  szFileName[256];
	bool	bHideability;
	bool	bHideabilitySecondary;
  bool	bPickable;
	float fBending;
	bool  bComplexBending;
	bool	bCastShadow;
	bool	bRecvShadow;
	bool	bPrecShadow;
	bool	bUseAlphaBlending;
	float fSpriteDistRatio;
	float fShadowDistRatio;
	float fMaxViewDistRatio;
	float	fBrightness;
	bool  bUseSprites;
	bool  bFadeSize_not_used;
	bool  bRandomRotation;
	uchar nMaterialLayers;

	float fDensity;
	float fElevationMax;
	float fElevationMin;
	float fSize;
	float fSizeVar;
	float fSlopeMax;
	float fSlopeMin;


	int nFlagsAndMaterialId;

	//! flags similar to entity render flags
	int m_dwRndFlags;

	AUTO_STRUCT_INFO_LOCAL
};

struct SBrushTypeChunk
{
	SBrushTypeChunk() { memset(this,0,sizeof(SBrushTypeChunk)); }

	int nDummy_pStatObj;
	char szFileName[256];

	AUTO_STRUCT_INFO_LOCAL
};

struct SMatInfoTmp
{
  SMatInfoTmp() { nDummy=0; szMatName[0] = 0; }
	int nDummy;
	char szMatName[256];

	AUTO_STRUCT_INFO_LOCAL
};

int CTerrain::GetCompiledDataSize()
{
	int nDataSize = 0;
	byte * pData = NULL;

	Get3DEngine()->m_pObjectsTree->CleanUpTree();

	Get3DEngine()->m_pObjectsTree->GetData(pData, nDataSize, NULL, NULL);

	m_pParentNode->GetData(pData, nDataSize);

	// get header size
	nDataSize += sizeof(STerrainChunkHeader);

	// get height map data size
	/*nDataSize += m_arrHightMapRangeInfo.GetSize()*m_arrHightMapRangeInfo.GetSize()*sizeof(SRangeInfo_Serialiaton);

	// count data
	for(int x=0; x<m_arrHightMapRangeInfo.GetSize(); x++)
	for(int y=0; y<m_arrHightMapRangeInfo.GetSize(); y++)
	{
		SRangeInfo & ri = m_arrHightMapRangeInfo[x][y];
		nDataSize += ri.nSize*ri.nSize*sizeof(ri.pHMData[0]);
	}*/

	// get vegetation objects table size
	nDataSize += sizeof(int);
	nDataSize += GetObjManager()->m_lstStaticTypes.size()*sizeof(StatInstGroupChunk);

	// get brush objects table size
	nDataSize += sizeof(int);
  std::vector<CStatObj*> brushTypes;
  std::vector<IMaterial*> usedMats;

  { // get vegetation objects materials
    std::vector<IMaterial*> * pMatTable = &usedMats;
    std::vector<StatInstGroup> & rTable = GetObjManager()->m_lstStaticTypes;
    int nObjectsCount = rTable.size();

    // init struct values and load cgf's
    for(int i=0; i<rTable.size(); i++)
    {
      int nMaterialId = -1;
      if(rTable[i].pMaterial)
      {
        nMaterialId = CObjManager::GetItemId(pMatTable, (IMaterial*)rTable[i].pMaterial, false);
        if(nMaterialId<0)
        {
          nMaterialId = pMatTable->size();
          pMatTable->push_back(rTable[i].pMaterial);
        }
      }
    }
  }

  Get3DEngine()->m_pObjectsTree->GenerateStatObjAndMatTables(&brushTypes, &usedMats);
  GetVisAreaManager()->GenerateStatObjAndMatTables(&brushTypes, &usedMats);
	nDataSize += brushTypes.size()*sizeof(SBrushTypeChunk);
  brushTypes.clear();

	// get custom materials table size
	nDataSize += sizeof(int);
	nDataSize += usedMats.size()*sizeof(SMatInfoTmp);

	return nDataSize;
}

bool CTerrain::GetCompiledData(byte * pData, int nDataSize, std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable)
{
	// write header
	STerrainChunkHeader * pTerrainChunkHeader = (STerrainChunkHeader *)pData;
	pTerrainChunkHeader->nChunkVersion = TERRAIN_CHUNK_VERSION;
	pTerrainChunkHeader->nChunkSize = nDataSize;
	pTerrainChunkHeader->TerrainInfo.nHeightMapSize_InUnits			= m_nSectorSize*m_nSectorsTableSize/m_nUnitSize;
	pTerrainChunkHeader->TerrainInfo.nUnitSize_InMeters					= m_nUnitSize;
	pTerrainChunkHeader->TerrainInfo.nSectorSize_InMeters				= m_nSectorSize;
	pTerrainChunkHeader->TerrainInfo.nSectorsTableSize_InSectors= m_nSectorsTableSize;
	pTerrainChunkHeader->TerrainInfo.fHeightmapZRatio						=	m_fHeightmapZRatio;
	pTerrainChunkHeader->TerrainInfo.fOceanWaterLevel						= (m_fOceanWaterLevel>WATER_LEVEL_UNKNOWN) ? m_fOceanWaterLevel : 0;

	UPDATE_PTR_AND_SIZE(pData,nDataSize,sizeof(STerrainChunkHeader));

  std::vector<struct IMaterial*> * pMatTable = new std::vector<struct IMaterial*>;

	{ // get vegetation objects count
		std::vector<StatInstGroup> & rTable = GetObjManager()->m_lstStaticTypes;
		int nObjectsCount = rTable.size();

		// prepare temporary chunks array for saving
		PodArray<StatInstGroupChunk> lstFileChunks;
		lstFileChunks.PreAllocate(nObjectsCount,nObjectsCount);

		// init struct values and load cgf's
		for(int i=0; i<rTable.size(); i++)
		{
			strncpy(lstFileChunks[i].szFileName,rTable[i].szFileName,sizeof(lstFileChunks[i].szFileName));
      COPY_MEMBER(&lstFileChunks[i],&rTable[i],bHideability);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],bHideabilitySecondary);
      COPY_MEMBER(&lstFileChunks[i],&rTable[i],bPickable);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fBending);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],bComplexBending);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],bCastShadow);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],bRecvShadow);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],bPrecShadow);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],bUseAlphaBlending);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fSpriteDistRatio);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fShadowDistRatio);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fMaxViewDistRatio);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fBrightness);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],bUseSprites);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],bRandomRotation);

			{
				lstFileChunks[i].nFlagsAndMaterialId = 0;
				
				if(rTable[i].bAlignToTerrain)
					lstFileChunks[i].nFlagsAndMaterialId |= SIGC_ALIGNTOTERRAIN;

				if(rTable[i].bUseTerrainColor)
					lstFileChunks[i].nFlagsAndMaterialId |= SIGC_USETERRAINCOLOR;

        if(rTable[i].bAffectedByVoxels)
          lstFileChunks[i].nFlagsAndMaterialId |= SIGC_AFFECTED_BY_VOXELS;

        int nMaterialId = -1;
        if(rTable[i].pMaterial)
        {
          nMaterialId = CObjManager::GetItemId(pMatTable, (IMaterial*)rTable[i].pMaterial, false);
          if(nMaterialId<0)
          {
            nMaterialId = pMatTable->size();
            pMatTable->push_back(rTable[i].pMaterial);
          }
        }

				lstFileChunks[i].nFlagsAndMaterialId |= ((nMaterialId+1)<<SIGC_MAT_ID_SHIFT);
			}

			COPY_MEMBER(&lstFileChunks[i],&rTable[i],nMaterialLayers);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fDensity);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fElevationMax);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fElevationMin);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fSize);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fSizeVar);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fSlopeMax);
			COPY_MEMBER(&lstFileChunks[i],&rTable[i],fSlopeMin);

			COPY_MEMBER(&lstFileChunks[i],&rTable[i],m_dwRndFlags);
		}

		memcpy(pData, &nObjectsCount, sizeof(int));
		UPDATE_PTR_AND_SIZE(pData,nDataSize,sizeof(int));

		// get table content
		memcpy(pData, lstFileChunks.GetElements(), lstFileChunks.Count()*sizeof(StatInstGroupChunk));	
		UPDATE_PTR_AND_SIZE(pData,nDataSize,lstFileChunks.Count()*sizeof(StatInstGroupChunk));
	}

  std::vector<struct CStatObj*> * pStatObjTable = new std::vector<struct CStatObj*>;
  Get3DEngine()->m_pObjectsTree->GenerateStatObjAndMatTables(pStatObjTable,pMatTable);
  GetVisAreaManager()->GenerateStatObjAndMatTables(pStatObjTable,pMatTable);

	{ // get brush objects count
		int nObjectsCount = pStatObjTable->size();
		memcpy(pData, &nObjectsCount, sizeof(int));
		UPDATE_PTR_AND_SIZE(pData,nDataSize,sizeof(int));

		PodArray<SBrushTypeChunk> lstFileChunks;
		lstFileChunks.PreAllocate(nObjectsCount,nObjectsCount);

		for(int i=0; i<pStatObjTable->size(); i++)
		{
			strncpy(lstFileChunks[i].szFileName, (*pStatObjTable)[i]->GetFilePath(), sizeof(lstFileChunks[i].szFileName));
		}

		// get table content
		memcpy(pData, lstFileChunks.GetElements(), lstFileChunks.Count()*sizeof(SBrushTypeChunk));	
		UPDATE_PTR_AND_SIZE(pData,nDataSize,lstFileChunks.Count()*sizeof(SBrushTypeChunk));
	}

	{ // get brush materials count
		std::vector<IMaterial*> & rTable = *pMatTable;
		int nObjectsCount = rTable.size();

		// count
		memcpy(pData, &nObjectsCount, sizeof(int));
		UPDATE_PTR_AND_SIZE(pData,nDataSize,sizeof(int));

		// get table content
		for(uint32 dwI=0;dwI<nObjectsCount;++dwI)
		{
			SMatInfoTmp tmp;

			assert(strlen(rTable[dwI] ? rTable[dwI]->GetName() : "")<sizeof(tmp.szMatName));
      strcpy(tmp.szMatName,rTable[dwI] ? rTable[dwI]->GetName() : "");

			memcpy(pData,&tmp,sizeof(SMatInfoTmp));
			UPDATE_PTR_AND_SIZE(pData,nDataSize,sizeof(SMatInfoTmp));
		}
	}

	// get nodes data
	int nNodesLoaded = m_pParentNode->GetData(pData, nDataSize);

	Get3DEngine()->m_pObjectsTree->CleanUpTree();

	int nOcNodesLoaded = Get3DEngine()->m_pObjectsTree->GetData(pData, nDataSize, pStatObjTable, pMatTable);

  if(ppStatObjTable)
    *ppStatObjTable = pStatObjTable;
  else
    SAFE_DELETE(pStatObjTable);

  if(ppMatTable)
    *ppMatTable = pMatTable;
  else
    SAFE_DELETE(pMatTable);

	assert(nNodesLoaded && nDataSize==0);
  return (nNodesLoaded && nDataSize==0);
}

template <class T>
bool CTerrain::Load_T(T * & f, int & nDataSize, STerrainChunkHeader * pTerrainChunkHeader, std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable)
{
  assert(pTerrainChunkHeader->nChunkVersion == TERRAIN_CHUNK_VERSION);
  if(pTerrainChunkHeader->nChunkVersion != TERRAIN_CHUNK_VERSION)
  {
    Error("CTerrain::SetCompiledData: version of file is %d, expected version is %d", pTerrainChunkHeader->nChunkVersion, (int)TERRAIN_CHUNK_VERSION);
    return 0;
  }

  if(pTerrainChunkHeader->nChunkSize != nDataSize+sizeof(STerrainChunkHeader))
    return 0;

  // get terrain settings
  m_nUnitSize					= pTerrainChunkHeader->TerrainInfo.nUnitSize_InMeters;
  m_fInvUnitSize			= 1.f/m_nUnitSize;
  m_nTerrainSize			= pTerrainChunkHeader->TerrainInfo.nHeightMapSize_InUnits*pTerrainChunkHeader->TerrainInfo.nUnitSize_InMeters;
  m_nSectorSize				= pTerrainChunkHeader->TerrainInfo.nSectorSize_InMeters;
  m_nSectorsTableSize = pTerrainChunkHeader->TerrainInfo.nSectorsTableSize_InSectors;
  m_fHeightmapZRatio	=	pTerrainChunkHeader->TerrainInfo.fHeightmapZRatio;
  m_fOceanWaterLevel  = pTerrainChunkHeader->TerrainInfo.fOceanWaterLevel ? pTerrainChunkHeader->TerrainInfo.fOceanWaterLevel : WATER_LEVEL_UNKNOWN;

  m_nUnitsToSectorBitShift = 0;
  while(m_nSectorSize>>m_nUnitsToSectorBitShift > m_nUnitSize)
    m_nUnitsToSectorBitShift++;

  // build nodes tree in fast way
  BuildSectorsTree(false);

  // pass heightmap to the physics
  InitHeightfieldPhysics();

  // setup physics grid
  if(!m_bEditorMode)
  {
    int nCellSize = 8;
    GetPhysicalWorld()->SetupEntityGrid(2,Vec3(0,0,0), // this call will destroy all physicalized stuff
      CTerrain::GetTerrainSize()/nCellSize,CTerrain::GetTerrainSize()/nCellSize,(float)nCellSize,(float)nCellSize);
  }

  PodArray<StatInstGroupChunk> lstStatInstGroupChunkFileChunks;

  { // get vegetation objects count
    int nObjectsCount = 0;//*StepData<int>(pData, &nDataSize);
    if(!LoadDataFromFile(&nObjectsCount, 1, f, nDataSize))
      return 0;

    assert(nObjectsCount == 0 || nObjectsCount == 1024);

    // get vegetation objects table
    if(!m_bEditorMode)
    {
      PrintMessage("===== Loading vegetation models =====");

      // load chunks into temporary array
      PodArray<StatInstGroupChunk> & lstFileChunks = lstStatInstGroupChunkFileChunks;
      lstFileChunks.PreAllocate(nObjectsCount,nObjectsCount);
      //			StepDataCopy(lstFileChunks.GetElements(), pData, &nDataSize, lstFileChunks.Count());	
      if(!LoadDataFromFile(lstFileChunks.GetElements(), lstFileChunks.Count(), f, nDataSize))
        return 0;

      // preallocate real array
      std::vector<StatInstGroup> & rTable = GetObjManager()->m_lstStaticTypes;
      rTable.resize(nObjectsCount);//,nObjectsCount);

      // init struct values and load cgf's
      for(int i=0; i<rTable.size(); i++)
      {
        //				COPY_MEMBER(&rTable[i],&lstFileChunks[i],nDummy_pStatObj);
        strncpy(rTable[i].szFileName,lstFileChunks[i].szFileName,sizeof(rTable[i].szFileName));
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],bHideability);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],bHideabilitySecondary);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],bPickable);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fBending);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],bComplexBending);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],bCastShadow);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],bRecvShadow);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],bPrecShadow);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],bUseAlphaBlending);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fSpriteDistRatio);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fShadowDistRatio);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fMaxViewDistRatio);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fBrightness);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],bUseSprites);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],bRandomRotation);

        rTable[i].bUseTerrainColor = (lstFileChunks[i].nFlagsAndMaterialId & SIGC_USETERRAINCOLOR)!=0;
        rTable[i].bAlignToTerrain = (lstFileChunks[i].nFlagsAndMaterialId & SIGC_ALIGNTOTERRAIN)!=0;
        rTable[i].bAffectedByVoxels = (lstFileChunks[i].nFlagsAndMaterialId & SIGC_AFFECTED_BY_VOXELS)!=0;

        COPY_MEMBER(&rTable[i],&lstFileChunks[i],nMaterialLayers);

        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fDensity);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fElevationMax);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fElevationMin);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fSize);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fSizeVar);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fSlopeMax);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],fSlopeMin);

        //				COPY_MEMBER(&rTable[i],&lstFileChunks[i],nDummy_pMaterial);
        COPY_MEMBER(&rTable[i],&lstFileChunks[i],m_dwRndFlags);

        int nMinSpec = (rTable[i].m_dwRndFlags&ERF_SPEC_BITS_MASK) >> ERF_SPEC_BITS_SHIFT;
        rTable[i].minConfigSpec = (ESystemConfigSpec)nMinSpec;

        if(rTable[i].szFileName[0])
        {
          ReleaseOwnership(rTable[i].pStatObj);
          rTable[i].pStatObj = GetObjManager()->LoadStatObj(rTable[i].szFileName);
        }

        rTable[i].Update(GetCVars(), Get3DEngine()->m_nGeomDetailScreenRes);
      }
    }
    else
    { // skip data
      PodArray<StatInstGroupChunk> & lstFileChunks = lstStatInstGroupChunkFileChunks;
      lstFileChunks.PreAllocate(nObjectsCount,nObjectsCount);
      if(!LoadDataFromFile(lstFileChunks.GetElements(), lstFileChunks.Count(), f, nDataSize))
        return 0;
      lstFileChunks.Clear();
    }
  }

  std::vector<CStatObj*> * pStatObjTable = new std::vector<CStatObj*>;

  { // get brush objects count
    int nObjectsCount = 0;//*StepData<int>(pData, &nDataSize);
    if(!LoadDataFromFile(&nObjectsCount, 1, f, nDataSize))
      return 0;

    // get brush objects table
    if(!m_bEditorMode)
    {
      PrintMessage("===== Loading brush models ===== ");

      PodArray<SBrushTypeChunk> lstFileChunks;
      lstFileChunks.PreAllocate(nObjectsCount,nObjectsCount);
      if(!LoadDataFromFile(lstFileChunks.GetElements(), lstFileChunks.Count(), f, nDataSize))
        return 0;

      pStatObjTable->resize(nObjectsCount);//PreAllocate(nObjectsCount,nObjectsCount);

      // load cgf's
      for(int i=0; i<pStatObjTable->size(); i++)
      {
        if(lstFileChunks[i].szFileName[0])
          (*pStatObjTable)[i] = GetObjManager()->LoadStatObj(lstFileChunks[i].szFileName);
      }
    }
    else
    { // skip data
      PodArray<SBrushTypeChunk> lstFileChunks;
      lstFileChunks.PreAllocate(nObjectsCount,nObjectsCount);
      if(!LoadDataFromFile(lstFileChunks.GetElements(), lstFileChunks.Count(), f, nDataSize))
        return 0;
    }
  }

  std::vector<IMaterial*> * pMatTable = new std::vector<IMaterial*>;

  { // get brush materials count
    int nObjectsCount = 0;//*StepData<int>(pData, &nDataSize);
    if(!LoadDataFromFile(&nObjectsCount, 1, f, nDataSize))
      return 0;

    // get vegetation objects table
    if(!m_bEditorMode)
    {
      PrintMessage("===== Loading brush materials ===== ");

      std::vector<IMaterial*> & rTable = *pMatTable;
      rTable.clear();
      rTable.resize(nObjectsCount);//PreAllocate(nObjectsCount,nObjectsCount);

      const uint32 cTableCount = rTable.size();
      for (uint32 tableIndex = 0; tableIndex < cTableCount; ++tableIndex)
      {
        SMatInfoTmp matInfoTmp;
        if(!LoadDataFromFile(&matInfoTmp, 1, f, nDataSize))
          return 0;

        rTable[tableIndex] = matInfoTmp.szMatName[0] ? GetMatMan()->LoadMaterial(matInfoTmp.szMatName) : NULL;
      }

      // assign real material to vegetation group
      std::vector<StatInstGroup> & rStaticTypes = GetObjManager()->m_lstStaticTypes;
      for(int i=0; i<rStaticTypes.size(); i++)
      {
        int nMaterialId = (lstStatInstGroupChunkFileChunks[i].nFlagsAndMaterialId>>SIGC_MAT_ID_SHIFT)-1;
        if(nMaterialId>=0)
          rStaticTypes[i].pMaterial = rTable[nMaterialId];
        else
          rStaticTypes[i].pMaterial = NULL;
      }
    }
    else
    { // skip data
      SMatInfoTmp matInfoTmp;
      for (uint32 tableIndex = 0; tableIndex < nObjectsCount; ++tableIndex)
      {
        if(!LoadDataFromFile(&matInfoTmp, 1, f, nDataSize))
          return 0;
      }
    }
  }

  // set nodes data
  PrintMessage("===== Initializing terrain nodes ===== ");
  int nNodesLoaded = m_pParentNode->Load(f, nDataSize);

  if(!Get3DEngine()->m_pObjectsTree)
    Get3DEngine()->m_pObjectsTree = new COctreeNode(AABB(Vec3(0,0,0),Vec3(GetTerrainSize(),GetTerrainSize(),GetTerrainSize())), NULL);

  // load object instances (in case of editor just check input data do no create object instances)
  int nOcNodesLoaded = Get3DEngine()->m_pObjectsTree->Load(f, nDataSize, pStatObjTable, pMatTable);

  if(m_bEditorMode)
  { // editor will re-insert all objects
    delete Get3DEngine()->m_pObjectsTree;
    Get3DEngine()->m_pObjectsTree = NULL;
  }

  if(ppStatObjTable)
    *ppStatObjTable = pStatObjTable;
  else
    SAFE_DELETE(pStatObjTable);

  if(ppMatTable)
    *ppMatTable = pMatTable;
  else
    SAFE_DELETE(pMatTable);

  m_pParentNode->UpdateRangeInfoShift();

  assert(nNodesLoaded && nDataSize==0);
  return (nNodesLoaded && nDataSize==0);
}

bool CTerrain::SetCompiledData(byte * pData, int nDataSize, std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable)
{
  STerrainChunkHeader * pTerrainChunkHeader = (STerrainChunkHeader *)pData;
  pData += sizeof(STerrainChunkHeader);
  nDataSize -= sizeof(STerrainChunkHeader);
  return Load_T(pData, nDataSize, pTerrainChunkHeader, ppStatObjTable, ppMatTable);
}

bool CTerrain::Load(FILE * f, int nDataSize, STerrainChunkHeader * pTerrainChunkHeader, std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable)
{
  return Load_T(f, nDataSize, pTerrainChunkHeader, ppStatObjTable, ppMatTable);
}

bool CTerrain::Compile()
{
	assert(0);
//	BuildSectorsTree(true);
	return true;
}

#ifdef ENABLE_TYPE_INFO
#include "TypeInfo_impl.h"
#include "terrain_compile_info.h"
#endif
