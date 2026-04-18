////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   meshidx.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: prepare shaders for cfg object
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "IndexedMesh.h"
#include "ObjMan.h"
#include "MeshCompiler/MeshCompiler.h"

#define MAX_USHORT (65536-2)


CIndexedMesh::CIndexedMesh()
{
	m_pFaces=0;
	m_pPositions=0;
	m_pTexCoord=0;
	m_pNorms=0;
	m_pColor0=m_pColor1=0;	
	m_pVertMats=0;
	m_numFaces=0;
	m_numVertices=0;
	m_nCoorCount=0;
	m_bInvalidated = false;
}

size_t CIndexedMesh::GetMemoryUsage()
{
  return CMesh::GetMemoryUsage();
}

struct TriEdge
{
	TriEdge() { memset(this,0,sizeof(*this)); }
	bool IsRemoved() { return (fError<0); }
	void SetRemoved() { fError=-1; }

	float fError;
	union{
		ushort arrVertId[2];
		uint nVertIds;
	};
	ushort arrFaces[2];
};

struct VertInfo
{
	PodArray<int> lstFacesIds;
};

int __cdecl CIndexedMesh__Cmp_TriEdge_Errors(const void* v1, const void* v2)
{
	TriEdge * p1 = (TriEdge*)v1;
	TriEdge * p2 = (TriEdge*)v2;

	if(p1->fError>p2->fError)
		return 1;

	if(p1->fError<p2->fError)
		return -1;

	return 0;
}

int __cdecl CIndexedMesh__Cmp_Int(const void* v1, const void* v2)
{
	int p1 = *(int*)v1;
	int p2 = *(int*)v2;

	if(p1 > p2)
		return -1;

	if(p1 < p2)
		return 1;

	return 0;
}

void CIndexedMesh::ShareVertices(ISystem * pSystem, const AABB & boxBoundary)
{
	FUNCTION_PROFILER( pSystem, PROFILE_3DENGINE );

#if defined(_DEBUG) && defined(WIN32)
	_CrtCheckMemory();
#endif /*defined(_DEBUG) && defined(WIN32)*/

	// make version of mesh without duplicated vertices
	std::vector<int> lstIndices;
	lstIndices.reserve( m_numFaces*3 );
	Vec3				*pVertsShared  = (Vec3 *)			 calloc(m_numVertices,sizeof(Vec3));
	SMeshTexCoord*pCoordShared = m_pTexCoord ? (SMeshTexCoord*)calloc(m_numVertices,sizeof(SMeshTexCoord)) : NULL;
	Vec3				*pNormsShared	 = (Vec3 *)			 calloc(m_numVertices,sizeof(Vec3));
	SMeshColor	*pColorShared0 = (SMeshColor *)calloc(m_numVertices,sizeof(SMeshColor));
	SMeshColor	*pColorShared1 = (SMeshColor *)calloc(m_numVertices,sizeof(SMeshColor));
	int					*pVertMatsShared = m_pVertMats ? (int*)calloc(m_numVertices,sizeof(int)) : NULL;

	for (int i=0; i<m_numFaces; i++) for (int v=0; v<3; v++)
	{
		lstIndices.push_back(m_pFaces[i].v[v]);
		assert(m_pFaces[i].v[v] == m_pFaces[i].t[v]);
		assert(m_pFaces[i].v[v] == m_pFaces[i].v[v]);
	}
	memcpy(pVertsShared, m_pPositions, sizeof(Vec3)*m_numVertices);
	int nVertCountShared = m_numVertices;

	// complete mesh 
	mesh_compiler::CMeshCompiler meshCompiler;
	meshCompiler.WeldPositions( pVertsShared, nVertCountShared, lstIndices, 0.25f, boxBoundary );

	for(uint i=0; i<lstIndices.size(); i+=3)
	{
		if(	lstIndices[i+0] == lstIndices[i+1] || lstIndices[i+1] == lstIndices[i+2] || lstIndices[i+2] == lstIndices[i+0])
		{ // degenerative triangle
			std::vector<int>::iterator remove_first = lstIndices.begin()+i;
			std::vector<int>::iterator remove_last = lstIndices.begin()+i+3;
			lstIndices.erase(remove_first,remove_last);
			int nElemId = i/3;
			memmove(&(m_pFaces[nElemId]), &(m_pFaces[nElemId+1]), sizeof(m_pFaces[0])*(m_numFaces-nElemId-1));
			i-=3;
			continue;
		}
	}

	for(uint i=0; i<lstIndices.size(); i++)
	{
		int nOldVertId = m_pFaces[i/3].v[i-i/3*3];
		assert(IsEquivalent(pVertsShared[lstIndices[i]], m_pPositions[nOldVertId], 0.5f));
		if(m_pTexCoord && pCoordShared)
			pCoordShared[lstIndices[i]] = m_pTexCoord[m_pFaces[i/3].t[i-i/3*3]];
		pNormsShared[lstIndices[i]] += m_pNorms[nOldVertId];
		pColorShared0[lstIndices[i]] = m_pColor0[nOldVertId];
		pColorShared1[lstIndices[i]] = m_pColor1[nOldVertId];
		if (m_pVertMats)
			pVertMatsShared[lstIndices[i]] = m_pVertMats[nOldVertId];
	}

	for(int v=0; v<nVertCountShared; v++)
		pNormsShared[v].normalize();

	// rebuild mesh
	RebuildMesh(pVertsShared, pCoordShared, pNormsShared, 
		pColorShared0, 
		pColorShared1, 
		lstIndices, nVertCountShared, pVertMatsShared);

	// free memory
	free(pVertsShared);
	free(pCoordShared);
	free(pNormsShared);

	free(pColorShared0);
	free(pColorShared1);
	free(pVertMatsShared);

#if defined(_DEBUG) && defined(WIN32)
	_CrtCheckMemory();
#endif /*defined(_DEBUG) && defined(WIN32)*/
}

void CIndexedMesh::SimplifyMesh(float fMaxEdgeLen, const AABB & boxBoundary )
{
#if defined(_DEBUG) && defined(WIN32)
	_CrtCheckMemory();
#endif /*defined(_DEBUG) && defined(WIN32)*/

	// make version of mesh without duplicated vertices
	std::vector<int> lstIndices;
	lstIndices.reserve( m_numFaces*3 );
	Vec3		*pVertsShared = (Vec3 *)calloc(m_numVertices, sizeof(Vec3));
	SMeshTexCoord*pCoordShared = m_pTexCoord ? (SMeshTexCoord*)calloc(m_numVertices, sizeof(SMeshTexCoord)) : NULL;
	Vec3		*pNormsShared = (Vec3 *)calloc(m_numVertices, sizeof(Vec3));
	SMeshColor	*pColorShared0 = (SMeshColor *)calloc(m_numVertices, sizeof(SMeshColor));
	SMeshColor	*pColorShared1 = (SMeshColor *)calloc(m_numVertices, sizeof(SMeshColor));
  int		*pVertMatsShared = m_pVertMats ? (int *)calloc(m_numVertices, sizeof(int)) : NULL;
	for (int i=0; i<m_numFaces; i++) for (int v=0; v<3; v++)
	{
		lstIndices.push_back(m_pFaces[i].v[v]);
		assert(m_pFaces[i].v[v] == m_pFaces[i].t[v]);
		assert(m_pFaces[i].v[v] == m_pFaces[i].v[v]);
	}
	memcpy(pVertsShared, m_pPositions, sizeof(Vec3)*m_numVertices);
	int nVertCountShared = m_numVertices;

	mesh_compiler::CMeshCompiler meshCompiler;
	meshCompiler.WeldPositions( pVertsShared, nVertCountShared, lstIndices, VEC_EPSILON, boxBoundary );

	for(uint i=0; i<lstIndices.size(); i+=3)
	{
		if(	lstIndices[i+0] == lstIndices[i+1] || lstIndices[i+1] == lstIndices[i+2] || lstIndices[i+2] == lstIndices[i+0])
		{ // degenerative triangle		
			std::vector<int>::iterator remove_first = lstIndices.begin()+i;
			std::vector<int>::iterator remove_last = lstIndices.begin()+i+3;
			lstIndices.erase(remove_first,remove_last);
			int nElemId = i/3;
			memmove(&(m_pFaces[nElemId]), &(m_pFaces[nElemId+1]), sizeof(m_pFaces[0])*(m_numFaces-nElemId-1));
			i-=3;
			continue;
		}
	}

	// complete mesh and also make vert info: for each unique vertex store list of connected faces ids
	VertInfo * pVertsInfo = (VertInfo*)calloc(m_numVertices, sizeof(VertInfo));
	memset(pVertsInfo, 0, sizeof(VertInfo)*m_numVertices);
	for(uint i=0; i<lstIndices.size(); i++)
	{
		if(pVertsInfo[lstIndices[i]].lstFacesIds.Find(i/3)<0)
			pVertsInfo[lstIndices[i]].lstFacesIds.Add(i/3);

    int nVertId = m_pFaces[i/3].v[i-i/3*3];
		assert(IsEquivalent(pVertsShared[lstIndices[i]], m_pPositions[nVertId], VEC_EPSILON));
		if(m_pTexCoord && pCoordShared)
			pCoordShared[lstIndices[i]] = m_pTexCoord[m_pFaces[i/3].t[i-i/3*3]];
		pNormsShared[lstIndices[i]] = m_pNorms[nVertId];
		pColorShared0[lstIndices[i]] = m_pColor0[nVertId];
		pColorShared1[lstIndices[i]] = m_pColor1[nVertId];
    if(pVertMatsShared && m_pVertMats)
      pVertMatsShared[lstIndices[i]] = m_pVertMats[nVertId];
	}

	// make list of all edges
	static PodArray<TriEdge> lstTriEdges; lstTriEdges.Clear();
	int nDeleted=0;
	for(uint i=0; i<lstIndices.size(); i+=3)
	{
		if(	lstIndices[i+0] == lstIndices[i+1] ||
				lstIndices[i+1] == lstIndices[i+2] ||
				lstIndices[i+2] == lstIndices[i+0])
		{ // degenerative triangle
			lstIndices[i+0] = -1;
			lstIndices[i+1] = -1;
			lstIndices[i+2] = -1;
			assert(!"Degenerative triangle found during edges list creation");
			continue;
		}

		for(int nE=0; nE<3; nE++)
		{
			TriEdge e;
			// store edge vertices id
			assert(lstIndices[i+(nE+0)%3]<MAX_USHORT);
			assert(lstIndices[i+(nE+1)%3]<MAX_USHORT);
			e.arrVertId[0] = lstIndices[i+(nE+0)%3];
			e.arrVertId[1] = lstIndices[i+(nE+1)%3];		
			if(e.arrVertId[0]>e.arrVertId[1])
			{
				int tmp = e.arrVertId[0];
				e.arrVertId[0] = e.arrVertId[1];
				e.arrVertId[1] = tmp;
			}
			assert(e.arrVertId[0] != e.arrVertId[1]);

			// todo: do not add edges connected to boundary edges

			// find faces connected to both vertices at the same time
			if(InitEdgeFaces(e, pVertsInfo, pVertsShared, NULL, boxBoundary))
			{
				e.fError = GetEdgeError(e, pVertsShared, pVertsInfo, lstIndices, pColorShared0);
				if(e.fError < fMaxEdgeLen) // do not process edges with too big error
					lstTriEdges.Add(e);
				else
					nDeleted++;
			}
		}
	}

	// remove duplicated edges
	RemoveDuplicatedEdges(lstTriEdges);

	// start to remove edges
	int nPass=0;
	while(lstTriEdges.Count())
	{
		// find next edge for removing
		int nLowestId=0;
		float fLowestValue=100000;
		for(int i=0; i<lstTriEdges.Count(); i++)
		{
			if(lstTriEdges[i].fError<fLowestValue)
			{
				nLowestId = i;
				fLowestValue = lstTriEdges[i].fError;
			}
		}

		// continue until no more edges with error lower than specified
		if(lstTriEdges[nLowestId].fError>fMaxEdgeLen)
			break;

		// remove zero edge and 2 triangles
		RemoveEdge(pVertsShared, pVertsInfo, pCoordShared, pNormsShared, 
			pColorShared0, pColorShared1, pVertMatsShared,
			lstIndices, nVertCountShared,lstTriEdges, nLowestId, boxBoundary);

		nPass++;
	}

	// rebuild mesh
	RebuildMesh(pVertsShared, pCoordShared, pNormsShared, 
		pColorShared0, pColorShared1, 
		lstIndices, nVertCountShared, pVertMatsShared);

	// free memory
	for(int i=0; i<nVertCountShared; i++)
		pVertsInfo[i].lstFacesIds.Reset();
	free(pVertsInfo);
	free(pVertsShared);
	free(pCoordShared);
	free(pNormsShared);

	free(pColorShared0);
	free(pColorShared1);
  free(pVertMatsShared);

#if defined(_DEBUG) && defined(WIN32)
	_CrtCheckMemory();
#endif /*defined(_DEBUG) && defined(WIN32)*/
}

bool CIndexedMesh::InitEdgeFaces(TriEdge & e, VertInfo * pVertsInfo, Vec3 * pVertsShared, TriEdge * pe0, const AABB & boxBoundary)
{
	// find faces connected to both vertices at the same time
	PodArray<int> & lstFacesIds0 = pVertsInfo[e.arrVertId[0]].lstFacesIds;
	PodArray<int> & lstFacesIds1 = pVertsInfo[e.arrVertId[1]].lstFacesIds;

	assert(!pe0 || lstFacesIds0.Find(pe0->arrFaces[0])<0);
	assert(!pe0 || lstFacesIds0.Find(pe0->arrFaces[1])<0);
	assert(!pe0 || lstFacesIds1.Find(pe0->arrFaces[0])<0);
	assert(!pe0 || lstFacesIds1.Find(pe0->arrFaces[1])<0);

	int nFoundFaces=0;
	for(int f=0; f<lstFacesIds0.Count() && nFoundFaces<=2; f++)
		if(lstFacesIds1.Find(lstFacesIds0[f])>=0)
		{
			assert(lstFacesIds0[f]<MAX_USHORT);
			if(nFoundFaces<2)
				e.arrFaces[nFoundFaces] = lstFacesIds0[f];
			nFoundFaces++;
		}

		if(nFoundFaces == 2)
		{ // take only nice edges between 2 triangles and not on object borders
			bool bBorder=0;
			for(int t=0; t<2; t++)
			{
				if(	pVertsShared[e.arrVertId[t]].x <= boxBoundary.min.x || 
						pVertsShared[e.arrVertId[t]].y <= boxBoundary.min.y || 
						pVertsShared[e.arrVertId[t]].z <= boxBoundary.min.z ||
						pVertsShared[e.arrVertId[t]].x >= boxBoundary.max.x || 
						pVertsShared[e.arrVertId[t]].y >= boxBoundary.max.y || 
						pVertsShared[e.arrVertId[t]].z >= boxBoundary.max.z)
				{ 
					bBorder=true; 
					break; 
				}
			}

			if(!bBorder)
				return true;
		}

		return false;
}

void CIndexedMesh::RemoveDuplicatedEdges(PodArray<struct TriEdge> & lstTriEdges)
{
	static PodArray<TriEdge*> lstHashTable[256];
	for(int i=0; i<256; i++)
		lstHashTable[i].Clear();

	// fill hash table
	for(int i=0; i<lstTriEdges.Count(); i++)
	{
		TriEdge & ei = lstTriEdges[i];
		lstHashTable[uchar(ei.nVertIds)].Add(&lstTriEdges[i]);
	}

	// search duplicates using hash
	for(int i=0; i<lstTriEdges.Count(); i++)
	{
		TriEdge & ei = lstTriEdges[i];
		PodArray<TriEdge*> & HashList = lstHashTable[uchar(ei.nVertIds)];

		int nFoundNum = 0;
		for(int n=0; n<HashList.Count(); n++)
		{
			TriEdge & en = *HashList[n];

			if(&en<=&ei)
				continue;

			if(ei.nVertIds == en.nVertIds)
			{ // if vertex ids are the same - delete second copy
				en.SetRemoved(); // mark as deleted
				nFoundNum++;
			}
		}
		assert(nFoundNum <= 1);
	}

	// delete edges marked for delete
	for(int i=0; i<lstTriEdges.Count(); i++)
	{
		if(lstTriEdges[i].IsRemoved())
		{
			lstTriEdges.DeleteFastUnsorted(i);
			i--;
		}
	}
}

bool CIndexedMesh::TestEdges(int nBeginWith, PodArray<TriEdge> & lstTriEdges, std::vector<int> & lstIndices, TriEdge * pEdgeForDelete)
{ // check: edges should not point to deleted faces
	for(int i=nBeginWith; i<lstTriEdges.Count(); i++)
	{
		//		assert(!pEdgeForDelete || pEdgeForDelete == &lstTriEdges[0]);
		TriEdge & e = lstTriEdges[i];
		for(int l=0; l<2; l++) for(int v=0; v<3; v++)
		{

			if(pEdgeForDelete)
			{
				if(pEdgeForDelete->arrFaces[0] == e.arrFaces[l] || pEdgeForDelete->arrFaces[1] == e.arrFaces[l])
					return false;
			}

			int nId = lstIndices[e.arrFaces[l]*3+v];
			if(nId<0)
				return false;
		}
	}

	return true;
}


void CIndexedMesh::MergeVertexFaceLists(int v0, int v1, struct VertInfo * pVertsInfo)
{
	VertInfo & vi0 = pVertsInfo[v0];
	VertInfo & vi1 = pVertsInfo[v1];

	vi1.lstFacesIds.AddList(vi0.lstFacesIds);
	vi0.lstFacesIds.Clear();
	vi0.lstFacesIds.AddList(vi1.lstFacesIds);

#ifdef _DEBUG
	for(int i=0; i<vi1.lstFacesIds.Count(); i++)
		for(int j=i+1; j<vi1.lstFacesIds.Count(); j++)
		{
			if(vi1.lstFacesIds[i] == vi1.lstFacesIds[j])
				assert(!"Duplicated faces found in merged list of vertex faces");
		}
#endif
}

void CIndexedMesh::MergeVertexComponents(TriEdge & e0, Vec3 * pVertsShared, struct VertInfo * pVertsInfo, 
																				 SMeshTexCoord * pCoordShared,Vec3 * pNormsShared, 
																				 SMeshColor * pColorShared0, SMeshColor * pColorShared1, 
                                         int * pVertMatsShared)
{
	// position
	Vec3 vNewPos = (pVertsShared[e0.arrVertId[0]]+pVertsShared[e0.arrVertId[1]])*0.5f;

	// normal
	Vec3 vNewNorm = (pNormsShared[e0.arrVertId[0]]+pNormsShared[e0.arrVertId[1]]).normalized();

	// texcoords
	SMeshTexCoord tcNewCoords;
	if(pCoordShared)
	{
		tcNewCoords.s = (pCoordShared[e0.arrVertId[0]].s+pCoordShared[e0.arrVertId[1]].s)*0.5f;
		tcNewCoords.t = (pCoordShared[e0.arrVertId[0]].t+pCoordShared[e0.arrVertId[1]].t)*0.5f;
	}

	// color0
	SMeshColor ucNewColor0;
	ucNewColor0.r = uchar(0.5f*pColorShared0[e0.arrVertId[0]].r + 0.5f*pColorShared0[e0.arrVertId[1]].r);
//	ucNewColor0.g = uchar(0.5f*pColorShared0[e0.arrVertId[0]].g + 0.5f*pColorShared0[e0.arrVertId[1]].g);
	ucNewColor0.g = pColorShared0[e0.arrVertId[0]].g; // material id and base color source
	ucNewColor0.b = uchar(0.5f*pColorShared0[e0.arrVertId[0]].b + 0.5f*pColorShared0[e0.arrVertId[1]].b);
	ucNewColor0.a = uchar(0.5f*pColorShared0[e0.arrVertId[0]].a + 0.5f*pColorShared0[e0.arrVertId[1]].a);

	// color1
	SMeshColor ucNewColor1;
	ucNewColor1.r = uchar(0.5f*pColorShared1[e0.arrVertId[0]].r + 0.5f*pColorShared1[e0.arrVertId[1]].r);
	ucNewColor1.g = uchar(0.5f*pColorShared1[e0.arrVertId[0]].g + 0.5f*pColorShared1[e0.arrVertId[1]].g);
	ucNewColor1.b = uchar(0.5f*pColorShared1[e0.arrVertId[0]].b + 0.5f*pColorShared1[e0.arrVertId[1]].b);
	ucNewColor1.a = uchar(0.5f*pColorShared1[e0.arrVertId[0]].a + 0.5f*pColorShared1[e0.arrVertId[1]].a);

	// update shared verts pos, vertex 0 should not be used in the end
	pVertsShared[e0.arrVertId[0]] = vNewPos;//+Vec3(0,0,8)*float(gEnv->pRenderer->GetFrameID()&1);
	pVertsShared[e0.arrVertId[1]] = vNewPos;

	// normals
	pNormsShared[e0.arrVertId[0]] = vNewNorm;
	pNormsShared[e0.arrVertId[1]] = vNewNorm;

  // vert mat id
  pVertMatsShared[e0.arrVertId[0]] = pVertMatsShared[e0.arrVertId[1]];

	// texcoords
	if(pCoordShared)
	{
		pCoordShared[e0.arrVertId[0]].s = tcNewCoords.s;
		pCoordShared[e0.arrVertId[0]].t = tcNewCoords.t;
		pCoordShared[e0.arrVertId[1]].s = tcNewCoords.s;
		pCoordShared[e0.arrVertId[1]].t = tcNewCoords.t;
	}

	// color0
	pColorShared0[e0.arrVertId[0]] = ucNewColor0;
	pColorShared0[e0.arrVertId[1]] = ucNewColor0;

	// color1
	pColorShared1[e0.arrVertId[0]] = ucNewColor1;
	pColorShared1[e0.arrVertId[1]] = ucNewColor1;
}

bool CIndexedMesh::RemoveEdge(Vec3 * pVertsShared, struct VertInfo * pVertsInfo, SMeshTexCoord * pCoordShared,Vec3 * pNormsShared, 
															SMeshColor * pColorShared0, SMeshColor * pColorShared1, int * pVertMatsShared,
															std::vector<int> & lstIndices, int nVertCountShared,
															PodArray<struct TriEdge> & lstTriEdges, int nEdgeForRemoveId, const AABB & boxBoundary)
{
	int nEdgesNumBefore = lstTriEdges.Count();
	// we will remove edge 0
	TriEdge e0 = lstTriEdges[nEdgeForRemoveId];
	lstTriEdges.Delete(nEdgeForRemoveId);

	assert(e0.arrVertId[0] != e0.arrVertId[1]);

	// move both vertices into new position
	MergeVertexComponents(e0, pVertsShared, pVertsInfo, pCoordShared, pNormsShared, pColorShared0, pColorShared1, pVertMatsShared);

	// Make list of involved vertices
	static PodArray<int> lstInvolvedVerts; lstInvolvedVerts.Clear();
	for(int l=0; l<2; l++) for(int v=0; v<3; v++)
	{
		int nId = lstIndices[e0.arrFaces[l]*3+v];
		assert(nId>=0);
		if(nId>=0 && lstInvolvedVerts.Find(nId)<0)
			lstInvolvedVerts.Add(nId);
	}

	assert(lstInvolvedVerts.Find(e0.arrVertId[0])>=0);
	assert(lstInvolvedVerts.Find(e0.arrVertId[1])>=0);

	/*
	if(lstTriEdges.Count()==489)
	for(int i=0; i<lstInvolvedVerts.Count(); i++)
	{
	PodArray<int> & lstFacesIds = pVertsInfo[lstInvolvedVerts[i]].lstFacesIds;
	for(int f=0; f<lstFacesIds.Count(); f++) 
	{
	for(int v=0; v<3; v++)
	{
	Vec3 vPos = pVertsShared[lstIndices[lstFacesIds[f]*3+v]];
	gEnv->pRenderer->DrawLabel(vPos+m_vOrigin,2,"%d",lstIndices[lstFacesIds[f]*3+v]);
	}
	}
	}

	if(lstTriEdges.Count()==489)
	{
	lstTriEdges.Clear();
	return false;
	}
	*/

	// remove references to deleted faces from pVertInfo and redirect indices
	for(int i=0; i<lstInvolvedVerts.Count(); i++)
	{
		PodArray<int> & lstFacesIds = pVertsInfo[lstInvolvedVerts[i]].lstFacesIds;
		for(int f=0; f<lstFacesIds.Count(); f++) 
		{
			if(lstFacesIds[f] == e0.arrFaces[0] || lstFacesIds[f] == e0.arrFaces[1])
			{
				lstFacesIds.Delete(f,1); f--;
				continue;
			}

			for(int v=0; v<3; v++)
			{
				if(lstIndices[lstFacesIds[f]*3+v] == e0.arrVertId[0])
					lstIndices[lstFacesIds[f]*3+v] = e0.arrVertId[1];
			}
		}
	}

#ifdef _DEBUG
	// check no references to deleted faces from pVertInfo
	for(int i=0; i<nVertCountShared; i++)
	{
		VertInfo & vi =	pVertsInfo[i];
		Vec3 vPos = pVertsShared[i];
		assert(vi.lstFacesIds.Find(e0.arrFaces[0])<0);
		assert(vi.lstFacesIds.Find(e0.arrFaces[1])<0);
	}
#endif

	// combine face lists of merged vertices
	MergeVertexFaceLists(e0.arrVertId[0], e0.arrVertId[1], pVertsInfo);

	// redirect edges to new vertex, fix edge face id
	PodArray<int> lstInvolvedEdges; lstInvolvedEdges.Clear();
	for(int nEdge=0; nEdge<lstTriEdges.Count(); nEdge++)
	{
		TriEdge & e = lstTriEdges[nEdge];
		TriEdge tOrigEdge = e;
		assert(e.arrVertId[0] != e.arrVertId[1]);
		bool bUpdate = false;

		if(e.arrVertId[0] == e0.arrVertId[0] || e.arrVertId[0] == e0.arrVertId[1])
		{
			e.arrVertId[0] = e0.arrVertId[1];
			bUpdate = true;
		}

		if(e.arrVertId[1] == e0.arrVertId[0] || e.arrVertId[1] == e0.arrVertId[1])
		{
			e.arrVertId[1] = e0.arrVertId[1];
			bUpdate = true;
		}

		if(
			e.arrFaces[0] == e0.arrFaces[0] ||
			e.arrFaces[0] == e0.arrFaces[1] ||
			e.arrFaces[1] == e0.arrFaces[0] ||
			e.arrFaces[1] == e0.arrFaces[1])
			bUpdate = true;

		if(bUpdate)
		{
			bool bGood = InitEdgeFaces(e, pVertsInfo, pVertsShared, &e0, boxBoundary);
			if(!bGood)
			{
				lstTriEdges.Delete(nEdge);
				nEdge--;
				continue;
			}

			if(e.arrVertId[0]>e.arrVertId[1])
			{
				int tmp = e.arrVertId[0];
				e.arrVertId[0] = e.arrVertId[1];
				e.arrVertId[1] = tmp;
			}

			lstInvolvedEdges.Add(nEdge);
		}

		assert(e.arrVertId[0] != e.arrVertId[1]);
	}

#ifdef _DEBUG
	TestEdges(0, lstTriEdges, lstIndices, &e0);
#endif


	// mark face as deleted in array of compiled indices 
	lstIndices[e0.arrFaces[0]*3+0]=-1; lstIndices[e0.arrFaces[0]*3+1]=-1; lstIndices[e0.arrFaces[0]*3+2]=-1;
	lstIndices[e0.arrFaces[1]*3+0]=-1; lstIndices[e0.arrFaces[1]*3+1]=-1; lstIndices[e0.arrFaces[1]*3+2]=-1;

	// remove duplicated edges
	float fNextOldMinLen = (lstTriEdges.Count()) ? lstTriEdges[0].fError : 0;
	static PodArray<int> lstDeletedEdges; lstDeletedEdges.Clear();
	for(int nEdgeI=0; nEdgeI<lstInvolvedEdges.Count(); nEdgeI++)
	{
		int nEdge = lstInvolvedEdges[nEdgeI];
		TriEdge & tEdge = lstTriEdges[nEdge];

		assert(	tEdge.arrVertId[0] != tEdge.arrVertId[1] );
		assert( tEdge.arrVertId[0] != e0.arrVertId[0] );
		assert( tEdge.arrVertId[1] != e0.arrVertId[0] );
		assert(	tEdge.arrVertId[0] == e0.arrVertId[1] || tEdge.arrVertId[1] == e0.arrVertId[1] );

		// update error value
		// todo: make errors never go down or never change and try to not sort them
		if(tEdge.fError == -1)
			continue;

		tEdge.fError = GetEdgeError(tEdge, pVertsShared, pVertsInfo, lstIndices, pColorShared0);

		int nFoundNum = 0;
		TriEdge & ei = lstTriEdges[nEdge];
		for(int nI=nEdgeI+1; nI<lstInvolvedEdges.Count(); nI++)
		{
			int n = lstInvolvedEdges[nI];
			TriEdge & en = lstTriEdges[n];
			if(ei.nVertIds == en.nVertIds)
			{ // if vertex ids are the same - delete second copy
				nFoundNum++;
				lstDeletedEdges.Add(n);
				lstTriEdges[n].fError = -1;
			}
		}
		assert(nFoundNum <= 1);
	}

	// delete edges
	if(lstDeletedEdges.Count())
		qsort(&lstDeletedEdges[0], lstDeletedEdges.Count(), sizeof(lstDeletedEdges[0]), CIndexedMesh__Cmp_Int);
	for(int nI=0; nI<lstDeletedEdges.Count(); nI++)
	{
		assert(lstTriEdges[lstDeletedEdges[nI]].fError == -1);
		lstTriEdges.Delete(lstDeletedEdges[nI]);
	}

#ifdef _DEBUG
	// check no references to deleted faces from pVertInfo
	for(int i=0; i<nVertCountShared; i++)
	{
		VertInfo & vi =	pVertsInfo[i];
		Vec3 vPos = pVertsShared[i];
		assert(vi.lstFacesIds.Find(e0.arrFaces[0])<0);
		assert(vi.lstFacesIds.Find(e0.arrFaces[1])<0);
	}
	TestEdges(0, lstTriEdges, lstIndices, &e0);
#endif

	// merge face lists of merged vertices into vertex 1
	PodArray<int> & lstFacesIds0 = pVertsInfo[e0.arrVertId[0]].lstFacesIds;
	PodArray<int> & lstFacesIds1 = pVertsInfo[e0.arrVertId[1]].lstFacesIds;
	assert(lstFacesIds1.Count() == lstFacesIds0.Count());
	for(int m=0; m<lstFacesIds0.Count(); m++)
		assert(lstFacesIds1[m] == lstFacesIds0[m]);
	pVertsInfo[e0.arrVertId[0]].lstFacesIds.Clear();


	int nRemovedEdges = nEdgesNumBefore - lstTriEdges.Count();

	if(nRemovedEdges != 3)
		int ttt=0;

	return 0;
}

void CIndexedMesh::RebuildMesh(Vec3 * pVertsShared, SMeshTexCoord * pCoordShared,Vec3 * pNormsShared, 
															 SMeshColor * pColorShared0, SMeshColor * pColorShared1, 
															 std::vector<int> &lstIndices, int nVertCountShared, int * pVertMatsShared)
{
	SetFacesCount( lstIndices.size()/3 );
//	memset(m_pFaces,0,lstIndices.size()/3*sizeof(SMeshFace)); // flags should stay

	int nFaceId = 0;
	for(uint i=0; i<lstIndices.size(); i+=3) 
	{
		if(lstIndices[i]>=0) // all deleted faces has -1 here
		{
			for(int v=0; v<3; v++)
			{
				assert(lstIndices[i+v]>=0 && lstIndices[i+v]<nVertCountShared);
				m_pFaces[nFaceId].v[v] = m_pFaces[nFaceId].v[v] = m_pFaces[nFaceId].t[v] = lstIndices[i+v];
			}
			nFaceId++;
		}
	}

	m_numFaces = nFaceId;

	SetVertexCount( nVertCountShared );
	SetTexCoordsCount( pCoordShared ? nVertCountShared : 0 );
	ReallocStream( COLORS_0,nVertCountShared );
  ReallocStream( COLORS_1,nVertCountShared );

  if(pVertMatsShared)
    ReallocStream( VERT_MATS,nVertCountShared );

	memcpy(m_pPositions, pVertsShared, sizeof(Vec3)*nVertCountShared); m_numVertices = nVertCountShared;
	if(m_pTexCoord && pCoordShared)
	{
		memcpy(m_pTexCoord, pCoordShared, sizeof(SMeshTexCoord)*nVertCountShared); 
		m_nCoorCount = nVertCountShared;
	}
	memcpy(m_pNorms, pNormsShared, sizeof(Vec3)*nVertCountShared);
	memcpy(m_pColor0, pColorShared0, sizeof(SMeshColor)*nVertCountShared); 
	memcpy(m_pColor1, pColorShared1, sizeof(SMeshColor)*nVertCountShared); 
	if(m_pVertMats && pVertMatsShared)
		memcpy(m_pVertMats, pVertMatsShared, sizeof(int)*nVertCountShared); 
}

float CIndexedMesh::GetEdgeError(TriEdge & tEdge, Vec3 * pVertsShared, VertInfo * pVertsInfo, std::vector<int> &lstIndices, SMeshColor * pColorShared0)
{
	// remember vert positions
	Vec3 arrvOrigPos[2] = 
	{
		pVertsShared[tEdge.arrVertId[0]],
		pVertsShared[tEdge.arrVertId[1]]
	};

	// find new vertex
	Vec3 vPosNew = (arrvOrigPos[0]+arrvOrigPos[1])*0.5f;

	// temporary replace positions with new one
	pVertsShared[tEdge.arrVertId[0]] = pVertsShared[tEdge.arrVertId[1]] = vPosNew;

	float surfTypeError = 0;
	float fMaxDistFromOldVertToNewTriangle = 0;
	for(int p=0; p<2; p++)
	{
		VertInfo & Inf = pVertsInfo[tEdge.arrVertId[p]];
		for(int f=0; f<Inf.lstFacesIds.Count(); f++)
		{
			int nFaceId = Inf.lstFacesIds[f];
			if(nFaceId == tEdge.arrFaces[0] || nFaceId == tEdge.arrFaces[1])
				continue; // skip faces of this edge

			assert(lstIndices[nFaceId*3+0]>=0);

			const Vec3 & v0 = pVertsShared[lstIndices[nFaceId*3+0]];
			const Vec3 & v1 = pVertsShared[lstIndices[nFaceId*3+1]];
			const Vec3 & v2 = pVertsShared[lstIndices[nFaceId*3+2]];

			Vec3 n = ((v1-v0)%(v0-v2));
			float fLen = n.GetLength();
			if(fLen>0.01f)
			{
				n /= fLen;
				Plane plane;
				plane.n = n;
				plane.d	=	-(n | v0);

				const float fDistFromOldVertToNewTriangle = fabsf(plane.DistFromPlane(arrvOrigPos[p]));
				if(fDistFromOldVertToNewTriangle>0.01f)
					if(fDistFromOldVertToNewTriangle>fMaxDistFromOldVertToNewTriangle)
						fMaxDistFromOldVertToNewTriangle = fDistFromOldVertToNewTriangle;
			}

			const uchar surfType0 = pColorShared0[lstIndices[nFaceId*3+0]].g;
			const uchar surfType1 = pColorShared0[lstIndices[nFaceId*3+1]].g;
			const uchar surfType2 = pColorShared0[lstIndices[nFaceId*3+2]].g;
			if(surfType0 != surfType1 || surfType1 != surfType2 || surfType2 != surfType0)
				surfTypeError = 0.05f;
		}
	}

	// restore original positions
	pVertsShared[tEdge.arrVertId[0]] = arrvOrigPos[0];
	pVertsShared[tEdge.arrVertId[1]] = arrvOrigPos[1];

//	assert(fAlphaMax>=fAlphaMin);

	float fError = 
		5.f*fMaxDistFromOldVertToNewTriangle + 
		0.5f*pVertsShared[tEdge.arrVertId[0]].GetDistance(pVertsShared[tEdge.arrVertId[1]]) +
		surfTypeError;

	assert(fError>=0);

	return fError;
}

//////////////////////////////////////////////////////////////////////////
void CIndexedMesh::Invalidate()
{
	m_bInvalidated = true;
	CalcBBox();
}

//////////////////////////////////////////////////////////////////////////
void CIndexedMesh::Optimize( const char * szComment, std::vector<uint16> *pVertexRemapping,std::vector<uint16> *pIndexRemapping )
{
	mesh_compiler::CMeshCompiler meshCompiler;
	if (pIndexRemapping)
		meshCompiler.SetIndexRemapping( pIndexRemapping );
	if (pVertexRemapping)
		meshCompiler.SetVertexRemapping( pVertexRemapping );

  // Stripification is very slow and should not be called at run time
  if(szComment)
    Warning( "CIndexedMesh::Optimize is called at run time by %s", szComment );

	meshCompiler.Compile( *this );
}

//////////////////////////////////////////////////////////////////////////
void CIndexedMesh::SimplifyMesh( float fLevel )
{
	mesh_compiler::CMeshCompiler meshCompiler;
	meshCompiler.ReduceMeshToLevel( *this,fLevel );

}

//////////////////////////////////////////////////////////////////////////
void CIndexedMesh::CalcBBox()
{
	if (m_numVertices == 0 || !m_pPositions)
	{
		m_bbox = AABB( Vec3(0,0,0),Vec3(0,0,0) );
		return;
	}

	m_bbox.Reset();
	int i,v;
	if (m_numFaces) for (i=0; i<m_numFaces; i++) for (v=0; v<3; v++)
	{
		int nIndex = m_pFaces[i].v[v];
		assert(nIndex>=0 && nIndex<m_numVertices);
		m_bbox.Add( m_pPositions[nIndex] );
	}
	else for (i=0; i<m_nIndexCount; i++)
		m_bbox.Add( m_pPositions[m_pIndices[i]] );
}

void CIndexedMesh::InitStreamSize()
{
	m_streamSize[POSITIONS] = m_pPositions ? m_numVertices : 0;
	m_streamSize[NORMALS]		= m_pPositions ? m_numVertices : 0;
	m_streamSize[TEXCOORDS] = m_pTexCoord ? m_numVertices : 0;
	m_streamSize[FACES]			= m_pFaces ? m_numFaces : 0;
	
	m_streamSize[COLORS_0] = m_pColor0 ? m_numVertices : 0;
	m_streamSize[COLORS_1] = m_pColor1 ? m_numVertices : 0;
  m_streamSize[VERT_MATS] = m_pVertMats ? m_numVertices : 0;
	
	m_streamSize[INDICES] = m_pIndices ? m_nIndexCount : 0;
	m_streamSize[TANGENTS] = m_pTangents ? m_numVertices : 0;
	if(m_pSHInfo && m_pSHInfo->pSHCoeffs)
		m_streamSize[SHCOEFFS] = m_numVertices;
	else
		m_streamSize[SHCOEFFS] = 0;
}
