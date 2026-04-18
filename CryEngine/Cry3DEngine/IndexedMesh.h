////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   meshidx.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: prepare shaders for object
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef IDX_MESH_H
#define IDX_MESH_H

#include "CryArray.h"
#include "CryHeaders.h"
#include "IIndexedMesh.h"

struct StatHelperInfo
{
	StatHelperInfo(char * new_name, int new_type, IShader *pShader, const Matrix34 & newMat)
	{
		//    vPos = new_pos;
		//    qRot = new_rot;
		strncpy(sName,new_name,64);
		nType = new_type;
		m_pShader = pShader;
		//  m_pObject = NULL;
		tMat = newMat;
	}

	//Vec3 vPos; CryQuat qRot;
	int nType;
	char sName[64];
	IShader *m_pShader;
	//  CRenderObject *m_pObject;
	Matrix34 tMat;
};

#define SFACE_FLAG_REMOVE 1

class CIndexedMesh : public IIndexedMesh, public CMesh, public Cry3DEngineBase
{
public:
	bool m_bInvalidated;

public:

	// constructor
	CIndexedMesh();

	virtual void Release() { delete this; }

	// gives read-only access to mesh data
	virtual void GetMesh(SMeshDesc & MeshDesc)
	{
		MeshDesc.m_nCoorCount = m_nCoorCount;
		MeshDesc.m_nVertCount = m_numVertices;
		MeshDesc.m_nFaceCount = m_numFaces;
		MeshDesc.m_pFaces = m_pFaces;
		MeshDesc.m_pVerts = m_pPositions;
		MeshDesc.m_pNorms = m_pNorms;
		MeshDesc.m_pColor = m_pColor0;
		MeshDesc.m_pTexCoord = m_pTexCoord;
		MeshDesc.m_pIndices = m_pIndices;
		MeshDesc.m_nIndexCount = m_nIndexCount;
	}

	virtual int GetFacesCount() { return m_numFaces; }
	virtual int GetVertexCount() { return m_numVertices; }
	virtual int GetTexCoordsCount() { return m_nCoorCount; }

	virtual void Invalidate();

	virtual void SetFacesCount(int nNewCount)
	{
		CMesh::SetFacesCount( nNewCount );
	}

	virtual void SetVertexCount(int nNewCount)
	{
		CMesh::SetVertexCount( nNewCount );
	}

	virtual void SetColorsCount(int nNewCount)
	{
		CMesh::ReallocStream( COLORS_0,nNewCount );
	}

	virtual void SetTexCoordsCount(int nNewCount)
	{
		CMesh::SetTexCoordsCount( nNewCount );
	}

	virtual void SetTexCoordsAndTangentsCount(int nNewCount)
	{
		CMesh::SetTexCoordsAndTangentsCount( nNewCount );
	}

	virtual void SetIndexCount( int nNewCount )
	{
		CMesh::SetIndexCount(nNewCount);
	}

	virtual int GetIndexCount()
	{
		return m_nIndexCount;
	}

	virtual void AllocateBoneMapping()
	{
		m_pBoneMapping = (SMeshBoneMapping *)malloc(m_numVertices*sizeof(SMeshBoneMapping));
	}

	virtual void AllocateSHData()
	{
		m_pSHInfo = new SSHInfo;
		m_pSHInfo->pSHCoeffs = new SMeshSHCoeffs[m_numVertices];
		m_pSHInfo->pDecompressions = new SSHDecompressionMat[m_subsets.size()];
	}

	size_t GetMemoryUsage();

	void DoBoxTexGen(float fScale, Vec3 * pvNormal = NULL);
	void SimplifyMesh(float fMaxEdgeLen, const AABB & boxBoundary);
	float GetEdgeError(struct TriEdge & tEdge, Vec3 * pVertsShared, struct VertInfo * pVertsInfo, std::vector<int> &lstIndices, SMeshColor * pColorShared);
	void RebuildMesh(Vec3 * pVertsShared, SMeshTexCoord * pCoordShared,Vec3 * pNormsShared, 
		SMeshColor * pColorShared0, 
		SMeshColor * pColorShared1, 
		std::vector<int> &lstIndices, int nVertCountShared, int * pVertMatsShared);
	bool RemoveEdge(Vec3 * pVertsShared, struct VertInfo * pVertsInfo, SMeshTexCoord * pCoordShared,Vec3 * pNormsShared, 
		SMeshColor * pColorShared0, 
		SMeshColor * pColorShared1, 
    int * pVertMatsShared,
		std::vector<int> & lstIndices, int nVertCountShared,
		PodArray<struct TriEdge> & lstTriEdges, int nEdgeForRemoveId, const AABB & boxBoundary);
	void RemoveDuplicatedEdges(PodArray<struct TriEdge> & lstTriEdges);
	bool TestEdges(int nBeginWith, PodArray<TriEdge> & lstTriEdges, std::vector<int> & lstIndices, TriEdge * pEdgeForDelete);
	void MergeVertexFaceLists(int v0, int v1, struct VertInfo * pVertsInfo);
	bool InitEdgeFaces(TriEdge & e, VertInfo * pVertsInfo, Vec3 * pVertsShared, TriEdge * pe0, const AABB & boxBoundary);
	void MergeVertexComponents(TriEdge & e0, Vec3 * pVertsShared, struct VertInfo * pVertsInfo, 
		SMeshTexCoord * pCoordShared,Vec3 * pNormsShared, SMeshColor * pColorShared0, SMeshColor * pColorShared1, int * pVertMatsShared);
	void InitStreamSize();
	void ShareVertices(ISystem * pSystem, const AABB & boxBoundary);


	virtual CMesh* GetMesh() { return this; };
	virtual void SetMesh( CMesh &mesh )
	{
		Copy( mesh );
	};

	virtual void SetSubSetCount( int nSubsets ) { m_subsets.resize(nSubsets); };
	virtual int GetSubSetCount() { return m_subsets.size(); };
	virtual SMeshSubset& GetSubSet( int nIndex ) { return m_subsets[nIndex]; };
	virtual void SetSubsetBoneIds( int idx, const PodArray<uint16> &boneIds ) { m_subsets[idx].m_arrGlobalBonesPerSubset = boneIds; }

	virtual void SetBBox( const AABB &box ) { m_bbox = box; };
	virtual AABB GetBBox() { return m_bbox; };
	virtual void CalcBBox();

	//////////////////////////////////////////////////////////////////////////
	virtual void Optimize( const char * szComment, std::vector<uint16> *pVertexRemapping=NULL,std::vector<uint16> *pIndexRemapping=NULL );
	virtual void SimplifyMesh( float fLevel );
};

#endif // IDX_MESH_H
