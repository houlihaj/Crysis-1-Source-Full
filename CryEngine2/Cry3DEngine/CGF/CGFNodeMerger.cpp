#include "StdAfx.h"
#include "CGFNodeMerger.h"
#include "CGFContent.h"

//////////////////////////////////////////////////////////////////////////
bool CCGFNodeMerger::MergeNodes(CContentCGF* pCGF, std::vector<int>& usedMaterialIds, string& errorMessage)
{
	CMesh *pMergedMesh = pCGF->AllocMergedMesh();

	// Check whether there are too many vertices to merge.
	int totalVerts = 0;
	for (int i = 0; i < pCGF->GetNodeCount(); i++)
	{
		CNodeCGF *pNode = pCGF->GetNode(i);
		if (!pNode->pMesh || pNode->bPhysicsProxy || pNode->type != CNodeCGF::NODE_MESH)
			continue;
		totalVerts += pNode->pMesh->GetVertexCount();
	}
	if (totalVerts >= (1 << 16))
	{
		errorMessage = "Too many total vertices to merge.";
		return false;
	}

	AABB meshBBox;
	meshBBox.Reset();
	for (int i = 0; i < pCGF->GetNodeCount(); i++)
	{
		CNodeCGF *pNode = pCGF->GetNode(i);
		if (!pNode->pMesh || pNode->bPhysicsProxy || pNode->type != CNodeCGF::NODE_MESH)
			continue;

		int nOldVerts = pMergedMesh->GetVertexCount();
		if (pMergedMesh->GetVertexCount() == 0)
		{
			pMergedMesh->Copy( *pNode->pMesh );
		}
		else
		{
			pMergedMesh->Append( *pNode->pMesh );
			// Keep color stream in sync size with vertex/normals stream.
			if (pMergedMesh->m_streamSize[CMesh::COLORS_0] > 0 && pMergedMesh->m_streamSize[CMesh::COLORS_0] < pMergedMesh->GetVertexCount())
			{
				int nOldCount = pMergedMesh->m_streamSize[CMesh::COLORS_0];
				pMergedMesh->ReallocStream( CMesh::COLORS_0,pMergedMesh->GetVertexCount() );
				memset( pMergedMesh->m_pColor0+nOldCount,255,(pMergedMesh->GetVertexCount()-nOldCount)*sizeof(SMeshColor) );
			}
			if (pMergedMesh->m_streamSize[CMesh::COLORS_1] > 0 && pMergedMesh->m_streamSize[CMesh::COLORS_1] < pMergedMesh->GetVertexCount())
			{
				int nOldCount = pMergedMesh->m_streamSize[CMesh::COLORS_1];
				pMergedMesh->ReallocStream( CMesh::COLORS_1,pMergedMesh->GetVertexCount() );
				memset( pMergedMesh->m_pColor1+nOldCount,255,(pMergedMesh->GetVertexCount()-nOldCount)*sizeof(SMeshColor) );
			}
		}

		AABB bbox = pNode->pMesh->m_bbox;
		if (!pNode->bIdentityMatrix)
			bbox.SetTransformedAABB( pNode->worldTM,bbox );

		meshBBox.Add(bbox.min);
		meshBBox.Add(bbox.max);
		pMergedMesh->m_bbox = meshBBox;

		if (!pNode->bIdentityMatrix)
		{
			// Transform merged mesh into the world space.
			// Only transform newly added vertices.
			for (int j = nOldVerts; j < pMergedMesh->GetVertexCount(); j++)
			{
				pMergedMesh->m_pPositions[j] = pNode->worldTM.TransformPoint( pMergedMesh->m_pPositions[j] );
				pMergedMesh->m_pNorms[j] = pNode->worldTM.TransformVector( pMergedMesh->m_pNorms[j] ).GetNormalized();
			}
		}
	}

	bool setupMeshSuccessful = true;
	if (!SetupMesh(*pMergedMesh, pCGF->GetCommonMaterial(), usedMaterialIds))
	{
		errorMessage = "SetupMesh() failed.";
		setupMeshSuccessful = false;
	}

	return setupMeshSuccessful;
}

//////////////////////////////////////////////////////////////////////////
bool CCGFNodeMerger::SetupMesh(CMesh &mesh,CMaterialCGF *pMaterialCGF, std::vector<int>& usedMaterialIds)
{
	unsigned int i;
	if (mesh.m_subsets.empty())
	{
		//////////////////////////////////////////////////////////////////////////
		// Setup mesh subsets.
		//////////////////////////////////////////////////////////////////////////
		mesh.m_subsets.clear();
		for (i = 0; i < usedMaterialIds.size(); i++)
		{
			SMeshSubset meshSubset;
			int nMatID = usedMaterialIds[i];
			meshSubset.nMatID = nMatID;
			meshSubset.nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
			mesh.m_subsets.push_back( meshSubset );
		}
	}
	//////////////////////////////////////////////////////////////////////////
	// Setup physicalization type from materials.
	//////////////////////////////////////////////////////////////////////////
	if (pMaterialCGF)
	{
		for (i = 0; i < mesh.m_subsets.size(); i++)
		{
			SMeshSubset &meshSubset = mesh.m_subsets[i];
			if (pMaterialCGF->subMaterials.size() > 0)
			{
				if (meshSubset.nMatID >= 0 && meshSubset.nMatID < (int)pMaterialCGF->subMaterials.size())
				{
					meshSubset.nPhysicalizeType = pMaterialCGF->subMaterials[meshSubset.nMatID]->nPhysicalizeType;
				}
			}
			else
			{
				meshSubset.nPhysicalizeType = pMaterialCGF->nPhysicalizeType;
			}
		}
	}
	if (mesh.m_subsets.empty())
	{
		//////////////////////////////////////////////////////////////////////////
		// Setup mesh subsets.
		//////////////////////////////////////////////////////////////////////////
		mesh.m_subsets.clear();
		for (i = 0; i < usedMaterialIds.size(); i++)
		{
			SMeshSubset meshSubset;
			int nMatID = usedMaterialIds[i];
			meshSubset.nMatID = nMatID;
			meshSubset.nPhysicalizeType = PHYS_GEOM_TYPE_DEFAULT;
			if (pMaterialCGF)
			{
				if (pMaterialCGF->subMaterials.size() > 0)
				{
					if (nMatID >= 0 && nMatID < (int)pMaterialCGF->subMaterials.size())
					{
						meshSubset.nPhysicalizeType = pMaterialCGF->subMaterials[i]->nPhysicalizeType;
					}
				}
				else
				{
					meshSubset.nPhysicalizeType = pMaterialCGF->nPhysicalizeType;
				}
			}
			mesh.m_subsets.push_back( meshSubset );
		}
	}

	return true;
}
