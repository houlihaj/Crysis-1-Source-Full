////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_init.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: init
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"
#include "terrain_sector.h"
#include "StatObj.h"
#include "ObjMan.h"
#include "CullBuffer.h"
#include "terrain_water.h"
//#include "detail_grass.h"

int CTerrain::m_nUnitSize    = 2;
float CTerrain::m_fInvUnitSize = 1.0f/2.0f;
int CTerrain::m_nTerrainSize = 2048; 
int CTerrain::m_nSectorSize  = 64;
int CTerrain::m_nSectorsTableSize = 32; 

void CTerrain::BuildSectorsTree(bool bBuildErrorsTable)
{
	int nCount=0;
	for(int i=0; i<TERRAIN_NODE_TREE_DEPTH; i++)
	{
		m_arrSecInfoPyramid[i].Allocate(CTerrain::GetSectorsTableSize()>>i);
		nCount += (CTerrain::GetSectorsTableSize()>>i)*(CTerrain::GetSectorsTableSize()>>i);
	}

	float fStartTime = GetCurAsyncTimeSec();
	PrintMessage(" ");

	// Log() to use LogPlus() later
	PrintMessage(bBuildErrorsTable ? "  Compiling %d nodes (%.1f MB) " : "  Constructing %d nodes (%.1f MB) ", 
		nCount, float(sizeof(CTerrainNode)*nCount)/1024.f/1024.f);

	if(m_pParentNode)
	{
		if(bBuildErrorsTable)
		{
			assert(0);
//			m_pParentNode->UpdateErrors();
		}
	}
	else
		m_pParentNode = new CTerrainNode(0,0,CTerrain::GetTerrainSize(),NULL,bBuildErrorsTable);

	if(Get3DEngine()->m_pObjectsTree)
		Get3DEngine()->m_pObjectsTree->UpdateTerrainNodes();

	assert(nCount == GetTerrainNodesAmount());

	PrintMessagePlus(" done in %.2f sec", GetCurAsyncTimeSec()-fStartTime );
}

void CTerrain::InitTerrainWater(IMaterial * pTerrainWaterShader, int nWaterBottomTexId)
{
  // make ocean surface
  SAFE_DELETE( m_pOcean );
	if( GetWaterLevel() > 0 )
		m_pOcean = new COcean(pTerrainWaterShader);
}
