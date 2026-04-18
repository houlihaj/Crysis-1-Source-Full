/*=============================================================================
	CGeomMesh.cpp : 
	Copyright 2004 Crytek Studios. All Rights Reserved.

	Revision history:
	* Created by Nick Kasyan
=============================================================================*/
#include "stdafx.h"

#include "IStatObj.h"
#include <CryArray.h>

#include "GeomMesh.h"


CGeomMesh::CGeomMesh(IRenderMesh* pRenderMesh, Matrix34* pNodeMatrix, CRenderChunk* pRenderChunk, IRenderShaderResources* shaderResources, uint8* pUsableTriangles)
{
//	m_arrItems.clear();

	m_pShaderResources = shaderResources;
	//assert(m_pShaderResources != NULL);

	m_pGeomCore = CSceneContext::GetInstance().GetGeomCore();
	assert(m_pGeomCore != NULL);

	m_bIsFilled = CreateMesh(pRenderMesh, pNodeMatrix, pRenderChunk, pUsableTriangles);
}

bool CGeomMesh::CreateMesh(IRenderMesh* pRenderMesh, Matrix34* pNodeMatrix, CRenderChunk* pRenderChunk, uint8* pUsableTriangles)
{
	// Get indices
	int iIdxCnt = 0;
	unsigned short* pIndices = pRenderMesh->GetIndices(&iIdxCnt);
	int32 nMeshInfo = m_pGeomCore->AddMeshInfos( pRenderMesh, pNodeMatrix, m_pShaderResources );

	CRLTriangleSmall Tri;	
//	Tri.m_pVert = NULL;
	Tri.m_nMeshInfo = nMeshInfo;
//	Tri.SetOpaque(true);
//	m_arrItems.reserve(pRenderChunk->nNumIndices / 3);

	//optional culling..
	if( pUsableTriangles )
	{
		// Process all triangles
		for (uint32 iCurTri=0; iCurTri<pRenderChunk->nNumIndices; iCurTri+=3)
		{
			if( pUsableTriangles[iCurTri] )
			{
				uint32 iBaseIdx = pRenderChunk->nFirstIndexId + iCurTri;
				Tri.m_nInd[0] = pIndices[iBaseIdx + 0];
				Tri.m_nInd[1] = pIndices[iBaseIdx + 1];
				Tri.m_nInd[2] = pIndices[iBaseIdx + 2];

				// Add triangle to the GeomCore 
				uint32 nAddInd = m_pGeomCore->Add(&Tri);
//				m_arrItems.push_back(nAddInd);
			}
		}
		return true;
	}
	else
	{
		// Process all triangles
		for (uint32 iCurTri=0; iCurTri<pRenderChunk->nNumIndices; iCurTri+=3)
		{
			uint32 iBaseIdx = pRenderChunk->nFirstIndexId + iCurTri;
			Tri.m_nInd[0] = pIndices[iBaseIdx + 0];
			Tri.m_nInd[1] = pIndices[iBaseIdx + 1];
			Tri.m_nInd[2] = pIndices[iBaseIdx + 2];

			// Add triangle to the GeomCore 
			uint32 nAddInd = m_pGeomCore->Add(&Tri);
//			m_arrItems.push_back(nAddInd);
		}
	}
	return true;
}
