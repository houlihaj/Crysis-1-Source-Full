#pragma once

class CRenderMeshMerger : public Cry3DEngineBase
{
public:
	CRenderMeshMerger(void);
	~CRenderMeshMerger(void);

	struct SDecalClipInfo { Vec3 vPos; float fRadius; Vec3 vProjDir; };

	struct SMergeInfo
	{
		const char *sMeshType;
		const char *sMeshName;
		bool bCompactVertBuffer;
		bool bPrintDebugMessages;
		bool bMakeNewMaterial;
		bool bMergeToOneRenderMesh;
		bool bPlaceInstancePositionIntoVertexNormal;
		IMaterial *pUseMaterial; // Force to use this material
		
		SDecalClipInfo * pDecalClipInfo;
		AABB *pClipCellBox;
		
		SMergeInfo() : 
			bCompactVertBuffer(false),
			bPrintDebugMessages(false),
			bMakeNewMaterial(true),
			sMeshType(""),sMeshName(""),pDecalClipInfo(0),pClipCellBox(0),pUseMaterial(0),
			bPlaceInstancePositionIntoVertexNormal(0) {}
	};

  IRenderMesh * MergeRenderMeshes(SRenderMeshInfoInput *pRMIArray, int nRMICount,PodArray<SRenderMeshInfoOutput> &outRenderMeshes,SMergeInfo &info );

	// Merge render meshes with same material.
	//IRenderMesh* FastMergeRenderMeshes( SRenderMeshInfoInput * pRMIArray, int nRMICount,const char * pMeshType,const char * pMeshName );

public:
	struct SMergedChunk
	{
		CRenderChunk rChunk;
		IMaterial *pMaterial;
	};

protected:
	PodArray<SRenderMeshInfoInput> m_lstRMIChunks;
	PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> m_lstVerts;
	PodArray<SPipTangents> m_lstTangBasises;
	PodArray<Vec2> m_lstLMTexCoords;
	PodArray<uint> m_lstIndices;
	PodArray<SMergedChunk> m_lstChunks;

	PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> m_lstNewVerts;
	PodArray<SPipTangents> m_lstNewTangBasises;
	PodArray<Vec2> m_lstNewLMTexCoords;
	PodArray<ushort> m_lstNewIndices;
	PodArray<SMergedChunk> m_lstNewChunks;
	
	PodArray<SMergedChunk> m_lstChunksMergedTemp;

	int m_nTotalVertexCount;
	int m_nTotalIndexCount;

	void MakeRenderMeshInfoListOfAllChunks( SRenderMeshInfoInput * pRMIArray, int nRMICount, SMergeInfo &info );
	void MakeListOfAllCRenderChunks( SMergeInfo &info );
	void IsChunkValid(CRenderChunk & Ch, PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> & lstVerts, PodArray<uint> & lstIndices);

	void ClipDecals( SMergeInfo &info );
	void CompactVertices( SMergeInfo &info );
	void TryMergingChunks( SMergeInfo &info );

	bool ClipTriangle(int nStartIdxId, Plane * pPlanes, int nPlanesNum);

	static int Cmp_Materials(IMaterial *pMat1, IMaterial *pMat2);
	static int Cmp_RenderChunks_(const void* v1, const void* v2);
	static int Cmp_RenderChunksInfo(const void* v1, const void* v2);
};
