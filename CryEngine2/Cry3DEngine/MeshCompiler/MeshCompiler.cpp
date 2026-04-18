////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   MeshCompiler.cpp
//  Version:     v1.00
//  Created:     5/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MeshCompiler.h"
#include "TangentSpaceCalculation.h"
#include "NvTriStrip/NvTriStrip.h"

#define BEGIN_MC_NAMESPACE namespace mesh_compiler {
#define END_MC_NAMESPACE };

BEGIN_MC_NAMESPACE

//////////////////////////////////////////////////////////////////////////
CMeshCompiler::CMeshCompiler()
{
	m_pVertexMap = 0;
	m_pIndexMap = 0;
	m_pMapFaceToFace0 = 0;
}

//////////////////////////////////////////////////////////////////////////
CMeshCompiler::~CMeshCompiler()
{
}

inline void CopyMeshVertex( CMesh &newMesh,int newVertex,CMesh &oldMesh,int oldVertex,int oldTexCoord )
{
	assert( newVertex < newMesh.m_numVertices );
//	assert( newVertex < newMesh.m_nCoorCount );
	//copy from old -> new vertex buffer
	newMesh.m_pPositions[newVertex] = oldMesh.m_pPositions[oldVertex];
	newMesh.m_pNorms[newVertex] = oldMesh.m_pNorms[oldVertex];
	if (oldMesh.m_pFaceNorms)
		newMesh.m_pFaceNorms[newVertex] = oldMesh.m_pFaceNorms[oldVertex];
	if (oldMesh.m_pTexCoord && oldTexCoord>=0)
		newMesh.m_pTexCoord[newVertex] = oldMesh.m_pTexCoord[oldTexCoord];
	if (oldMesh.m_pColor0)
		newMesh.m_pColor0[newVertex] = oldMesh.m_pColor0[oldVertex];
	if (oldMesh.m_pColor1)
		newMesh.m_pColor1[newVertex] = oldMesh.m_pColor1[oldVertex];
  if (oldMesh.m_pVertMats)
    newMesh.m_pVertMats[newVertex] = oldMesh.m_pVertMats[oldVertex];
	if (oldMesh.m_pTangents)
		newMesh.m_pTangents[newVertex] = oldMesh.m_pTangents[oldVertex];
}

#define TEX_EPS 0.00005f
#define VER_EPS 0.00005f

//////////////////////////////////////////////////////////////////////////
inline bool CompareMeshVertices( CMesh &mesh,int a,int b )
{
	/*
	//Timur, just for testing.. Remove it!!
	if(IsEquivalent( mesh.m_pPositions[a],mesh.m_pPositions[b],VER_EPS ))
		return true;
	else
		return false;
			*/

	if( mesh.m_pPositions[a].IsEquivalent( mesh.m_pPositions[b],VER_EPS) 
		&& mesh.m_pNorms[a].IsEquivalent( mesh.m_pNorms[b],VER_EPS ) 
		&& (!mesh.m_pTexCoord || (fabs(mesh.m_pTexCoord[a].s - mesh.m_pTexCoord[b].s) < TEX_EPS && fabs(mesh.m_pTexCoord[a].t - mesh.m_pTexCoord[b].t) < TEX_EPS)))
	{
		if(!mesh.m_pTangents)
			return true;

		if(	 mesh.m_pTangents[a].Binormal.x==mesh.m_pTangents[b].Binormal.x	// equal comparison as input data is exact
			&& mesh.m_pTangents[a].Binormal.y==mesh.m_pTangents[b].Binormal.y	// equal comparison as input data is exact
			&& mesh.m_pTangents[a].Binormal.z==mesh.m_pTangents[b].Binormal.z	// equal comparison as input data is exact
			&& mesh.m_pTangents[a].Tangent.x==mesh.m_pTangents[b].Tangent.x	// equal comparison as input data is exact
			&& mesh.m_pTangents[a].Tangent.y==mesh.m_pTangents[b].Tangent.y	// equal comparison as input data is exact
			&& mesh.m_pTangents[a].Tangent.z==mesh.m_pTangents[b].Tangent.z)	// equal comparison as input data is exact
			return true;

		return false;
	}


	/*
	{
	Vec3 vBinormalA( tPackB2F(mesh.m_pTangents[a].Binormal.x),
	tPackB2F(mesh.m_pTangents[a].Binormal.y),
	tPackB2F(mesh.m_pTangents[a].Binormal.z));

	Vec3 vTangentA(	tPackB2F(mesh.m_pTangents[a].Tangent.x),
	tPackB2F(mesh.m_pTangents[a].Tangent.y),
	tPackB2F(mesh.m_pTangents[a].Tangent.z));

	Vec3 vBinormalB( tPackB2F(mesh.m_pTangents[b].Binormal.x),
	tPackB2F(mesh.m_pTangents[b].Binormal.y),
	tPackB2F(mesh.m_pTangents[b].Binormal.z));

	Vec3 vTangentB(	tPackB2F(mesh.m_pTangents[b].Tangent.x),
	tPackB2F(mesh.m_pTangents[b].Tangent.y),
	tPackB2F(mesh.m_pTangents[b].Tangent.z));

	if (	vBinormalA.Dot(vBinormalB) > 0 &&
	vTangentA.Dot(vBinormalB) > 0.5f)
	return true;
	}
	*/

	return false;
}

//////////////////////////////////////////////////////////////////////////
inline bool CompareVoxelMeshVertices( CMesh &mesh,int a,int b )
{

	if(mesh.m_pPositions[a].IsEquivalent( mesh.m_pPositions[b],VER_EPS ))
	{
		int32	nAxisA, nAxisB;

		Vec3& vNormA = mesh.m_pFaceNorms[a];
		Vec3& vNormB = mesh.m_pFaceNorms[b];

		Vec3 vAbsNormA = vNormA.abs();
		Vec3 vAbsNormB = vNormB.abs();

		//find orientation axis for A
		if ( vAbsNormA.x >= vAbsNormA.y && vAbsNormA.x >= vAbsNormA.z ) 
		{
			nAxisA = (vNormA.x>=0)? 0 : 3;
		} else 
			if ( vAbsNormA.y >= vAbsNormA.x && vAbsNormA.y >= vAbsNormA.z ) 
			{
				nAxisA = (vNormA.y>=0)? 1 : 4;		
			}
			else 
				{
					nAxisA = (vNormA.z>=0)? 2 : 5;
				}

		//find orientation axis for B
		if ( vAbsNormB.x >= vAbsNormB.y && vAbsNormB.x >= vAbsNormB.z ) 
		{
			nAxisB = (vNormB.x>=0)? 0 : 3;
		} else 
			if ( vAbsNormB.y >= vAbsNormB.x && vAbsNormB.y >= vAbsNormB.z ) 
			{
				nAxisB = (vNormB.y>=0)? 1 : 4;
			}
			else 
				{
					nAxisB = (vNormB.z>=0)? 2 : 5;
				}

		//compare orientation
		if (nAxisA == nAxisB)	
			return true;

	}

	return false;
}


class CMeshInputProxy :public ITriangleInputProxy
{
	// helper to get order for CVertexLoadHelper
	struct NormalCompare: public std::binary_function<Vec3, Vec3, bool>
	{
		bool operator() ( const Vec3 &a, const Vec3 &b ) const
		{
			// first sort by x
			if(a.x<b.x)
				return(true);
			if(a.x>b.x)
				return(false);

			// then by y
			if(a.y<b.y)
				return(true);
			if(a.y>b.y)
				return(false);

			// then by z
			if(a.z<b.z)
				return(true);
			if(a.z>b.z)
				return(false);

			return(false);
		}
	};

public: // ----------------------------------------------------------------

	//! constructor
	//! /param inpMesh must not be 0
	CMeshInputProxy( CMesh &inData )
	{
		m_pData = &inData;

		// remap the normals (weld them)
		uint32 dwFaceCount = GetTriangleCount();

		m_NormIndx.reserve(dwFaceCount);

		std::map<Vec3,uint32,NormalCompare>  mapNormalsToNumber;
		uint32 dwmapSize=0;

		// for every triangle
		for(uint32 i=0;i<dwFaceCount;i++)
		{
			CTriNormIndex idx;

			// for every vertex of the triangle
			for(uint32 e=0;e<3;e++)
			{
				int iNorm = m_pData->m_pFaces[i].v[e];
				Vec3 &vNorm = m_pData->m_pNorms[iNorm];

				std::map<Vec3,uint32,NormalCompare>::iterator iFind = mapNormalsToNumber.find(vNorm);

				if(iFind == mapNormalsToNumber.end())       // not found
				{
					idx.p[e] = dwmapSize;
					mapNormalsToNumber[vNorm] = dwmapSize;
					dwmapSize++;
				}
				else
					idx.p[e] = (*iFind).second;
			}

			m_NormIndx.push_back(idx);
		}
	}

	// interface ITriangleInputProxy ----------------------------------------------

	//! /return 0..
	uint32 GetTriangleCount() const
	{
		return m_pData->m_numFaces;
	}

	//! /param indwTriNo 0..
	//! /param outdwPos
	//! /param outdwNorm
	//! /param outdwUV
	void GetTriangleIndices( const uint32 indwTriNo, uint32 outdwPos[3], uint32 outdwNorm[3], uint32 outdwUV[3] ) const
	{
		const unsigned short *pIndsP = &m_pData->m_pFaces[indwTriNo].v[0];
		const unsigned short *pIndsUV = &m_pData->m_pFaces[indwTriNo].t[0];
		const CTriNormIndex &norm = m_NormIndx[indwTriNo];

		for(int i=0; i<3; i++)
		{
			outdwPos[i] = pIndsP[i];
			outdwUV[i] = pIndsUV[i];
			outdwNorm[i] = norm.p[i];
		}
	}

	//! /param indwPos 0..
	//! /param outfPos
	void GetPos( const uint32 indwPos, float outfPos[3] ) const
	{
		assert((int)indwPos < m_pData->m_numVertices);
		Vec3 &ref = m_pData->m_pPositions[indwPos];
		outfPos[0] = ref.x;
		outfPos[1] = ref.y;
		outfPos[2] = ref.z;
	}

	//! /param indwPos 0..
	//! /param outfUV 
	void GetUV( const uint32 indwPos, float outfUV[2] ) const
	{
		assert((int)indwPos < m_pData->m_nCoorCount);
		SMeshTexCoord &ref = m_pData->m_pTexCoord[indwPos];
		outfUV[0] = ref.s;
		outfUV[1] = ref.t;
	}

private: // -------------------------------------------------------------------------

	class CTriNormIndex
	{
	public:
		uint32 p[3];															// index in m_BaseVectors
	};

	std::vector<CTriNormIndex>  m_NormIndx;			// normal indices for each triangle
	CMesh *											m_pData;				// must not be 0
};



//do not use vec3 lib to keep it the fallback as it was
static const Vec3 CrossProd( const Vec3 &a, const Vec3 &b )
{
	Vec3 ret;
	ret.x=a.y*b.z - a.z*b.y;
	ret.y=a.z*b.x - a.x*b.z;
	ret.z=a.x*b.y - a.y*b.x;
	return ret;
}

static void GetOtherBaseVec( Vec3& s, Vec3 &a, Vec3 &b )
{
	if(s.z<-0.5f || s.z>0.5f)
	{
		a.x=s.z;
		a.y=s.y;
		a.z=-s.x;
	}
	else
	{
		a.x=s.y;
		a.y=-s.x;
		a.z=s.z;
	}

	b=CrossProd(s,a);
	b.Normalize();
	a=CrossProd(b,s);
	a.Normalize();
}

//check packed tangent space and ensure some useful values, fix always according to normal
void VerifyTangentSpace(SMeshTangents& rTangents, Vec3 normal)
{
	if(normal.GetLengthSquared() < 0.1f)
		normal = Vec3(0,0,1);
	else
		if(normal.GetLengthSquared() < 0.9f)
			normal.Normalize();

	//unpack first(necessary since the quantization can introduce errors whereas the original float data were different) 
	Vec3 tangent(tPackB2F(rTangents.Tangent.x), tPackB2F(rTangents.Tangent.y), tPackB2F(rTangents.Tangent.z));
	Vec3 binormal(tPackB2F(rTangents.Binormal.x), tPackB2F(rTangents.Binormal.y), tPackB2F(rTangents.Binormal.z));

	//check if they are equal
	const bool cIsEqual = (tangent==binormal);
	//check if they are zero
	const bool cTangentIsZero = (tangent.GetLengthSquared() < 0.01f);
	const bool cBinormalIsZero = (binormal.GetLengthSquared() < 0.01f);
	const bool cbHasBeenChanged = (cIsEqual || cTangentIsZero || cBinormalIsZero);
	if(cIsEqual)
	{
		//fix case where both vec's are equal
		GetOtherBaseVec(normal, tangent, binormal);
	}
	else
	if(cTangentIsZero)
	{
		//fix case where tangent is zero
		binormal.Normalize();//just to make sure
		if(abs(binormal * normal) > 0.9f)//if angle between both vecs is to low, calc new one for both
			GetOtherBaseVec(normal, tangent, binormal);
		else
			tangent = CrossProd(normal, binormal);
	}
	else
	if(cBinormalIsZero)
	{
		//fix case where binormal is zero
		tangent.Normalize();//just to make sure
		if(abs(tangent * normal) > 0.9f)//if angle between both vecs is to low, calc new one for both
			GetOtherBaseVec(normal, tangent, binormal);
		else
			binormal = CrossProd(tangent, normal);
	}
	//pack altered tangent vecs
	if(cbHasBeenChanged)
	{
		rTangents.Tangent = Vec4sf(tPackF2B(tangent.x), tPackF2B(tangent.y), tPackF2B(tangent.z), tPackF2B(1));
		rTangents.Binormal = Vec4sf(tPackF2B(binormal.x), tPackF2B(binormal.y), tPackF2B(binormal.z), tPackF2B(1));
		if ((tangent ^ binormal)*normal < 0)
		{
			rTangents.Tangent.w = tPackF2B(-1);
			rTangents.Binormal.w = tPackF2B(-1);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Optimizes CMesh.
// IMPLEMENTATION:
// . Sort|Group faces by materials
// . Create vertex buffer with sequence of (possibly non-unique) vertices, 3 verts per face
// . For each (non-unique) vertex calculate the tangent base
// . Index the mesh (Compact Vertices): detect and delete duplicate vertices
// . Remove degenerated triangles in the generated mesh (GetIndices())
// . Sort vertices and indices for GPU cache
bool CMeshCompiler::Compile( CMesh &mesh,int flags )
{
	//DebugBreak();

	if (mesh.GetFacesCount() == 0)
	{
		const int cVertexCount = mesh.GetVertexCount();
		if(cVertexCount == 0)
			return true;	//nothing to do
		//mesh has already been compiled, likely to have a refresh here: just verify and correct tangent space
		if(mesh.m_pTangents && mesh.m_pNorms)
		{
			for(int i=0; i<cVertexCount; ++i)
				VerifyTangentSpace(mesh.m_pTangents[i], mesh.m_pNorms[i]);				
		}
		return true;
	}

	//compile mesh, has face information
	if (mesh.m_bbox.IsEmpty() && mesh.GetVertexCount() > 0)
	{
		// Calc bounding box.
		mesh.m_bbox.Reset();
		for (int i = 0; i < mesh.m_numVertices; i++)
		{
			mesh.m_bbox.Add( mesh.m_pPositions[i] );
		}
	}

	int max_vert_num = mesh.m_numFaces*3;

	uint32 i;
	uint8* vertexMatId = new uint8[max_vert_num];
	int buff_vert_count = 0;

	if (m_pVertexMap)
	{
		m_pVertexMap->clear();
		m_pVertexMap->reserve( max_vert_num );
	}

	// Sort|Group faces by materials
	// For each shader (designated by nMaterialId of an element of m_pFaces)
	// there is one PodArray in this table. In this list, there will be a sorted
	// array of faces belonging to this shader
	SMeshTangents *pBasises = NULL;
	uint16 *pBasisIndices = NULL;


//MessageBox( NULL,"Attach me","Debug",MB_OK|MB_ICONERROR );

	//////////////////////////////////////////////////////////////////////////
	// Calc Tangent Space.
	//////////////////////////////////////////////////////////////////////////
	if (flags & MESH_COMPILE_TANGENTS)
	{
		// Generate tangent basis vectors before indexing per-material
		CMeshInputProxy Input(mesh);
		CTangentSpaceCalculation<CMeshInputProxy> tangents;

		// calculate the base matrices
		unsigned int nErrorCode = tangents.CalculateTangentSpace(Input);

		if(nErrorCode!=0)
		{
			// I don't have the filename here - but if the input data was the cause the problem should be reported and fixed earlier anyway
			m_LastError.Format(" CalculateTangentSpace() failed, contact programmer, ErrorCode:%d",nErrorCode);
			return false;
		}

		uint32 dwCnt = tangents.GetBaseCount();
		// for every triangle
		uint32 dwTris = Input.GetTriangleCount();

		pBasises = new SMeshTangents[dwCnt];
		pBasisIndices = new uint16[dwTris*3];

		for(uint32 dwTri=0; dwTri<dwTris; dwTri++)
		{
			uint32 dwBaseIndx[3];

			tangents.GetTriangleBaseIndices(dwTri, dwBaseIndx);

			assert(dwBaseIndx[0]<dwCnt);
			assert(dwBaseIndx[1]<dwCnt);
			assert(dwBaseIndx[2]<dwCnt);

			// for every vertex of the triangle
			for(i=0; i<3; i++)
			{
				pBasisIndices[dwTri*3+i] = (uint16)(dwBaseIndx[i] & 0xFFFF);		// set the base vector
			}
		}

		//
		for(i=0; i<dwCnt; i++)
		{
			Vec3 Tangent, Binormal, TNormal;
			tangents.GetBase(i, (float*)&Tangent, (float*)&Binormal, (float*)&TNormal);
			pBasises[i].Tangent = Vec4sf(tPackF2B(Tangent[0]), tPackF2B(Tangent[1]), tPackF2B(Tangent[2]), tPackF2B(1));
			pBasises[i].Binormal = Vec4sf(tPackF2B(Binormal[0]), tPackF2B(Binormal[1]), tPackF2B(Binormal[2]), tPackF2B(1));

			if ((Tangent ^ Binormal)*TNormal < 0)
			{
				pBasises[i].Tangent.w = tPackF2B(-1);
				pBasises[i].Binormal.w = tPackF2B(-1);
			}
			//verify tangent space
			VerifyTangentSpace(pBasises[i], TNormal);
		}

		//////////////////////////////////////////////////////////////////////////
		// fill the table: one list of faces per one material
		for (i = 0; i < (uint32)mesh.m_numFaces; i++)
		{
			SMeshFace* pFace = &mesh.m_pFaces[i];
			SBasisFace fc;
			uint16 *v = &pBasisIndices[i*3];
			fc.v[0] = v[0];
			fc.v[1] = v[1];
			fc.v[2] = v[2];
			m_thash_table[pFace->nSubset].push_back(fc);
		}
	}
	
	//////////////////////////////////////////////////////////////////////////
	// fill the table: one list of faces per one material
	for (i = 0; i < (uint32)mesh.m_numFaces; i++)
	{
		SMeshFace* pFace =  &mesh.m_pFaces[i];
		m_vhash_table[pFace->nSubset].push_back(pFace);
	}

	CMesh outMesh;
	outMesh.Copy(mesh);
	outMesh.SetVertexCount( max_vert_num );
	if(mesh.m_pTexCoord)
		outMesh.SetTexCoordsCount( max_vert_num ); // Same number of tex coords as of vertices.
	if (mesh.m_pSHInfo && mesh.m_pSHInfo->pSHCoeffs)
		outMesh.ReallocStream( CMesh::SHCOEFFS,max_vert_num );
	if (flags & MESH_COMPILE_TANGENTS)
		outMesh.ReallocStream( CMesh::TANGENTS,max_vert_num );
	if (mesh.m_pColor0) outMesh.ReallocStream( CMesh::COLORS_0,max_vert_num );
	if (mesh.m_pColor1) outMesh.ReallocStream( CMesh::COLORS_1,max_vert_num );
  if (mesh.m_pVertMats) outMesh.ReallocStream( CMesh::VERT_MATS,max_vert_num );
	//recalculate face normals
	if (flags & MESH_COMPILE_VOXEL) 
		outMesh.ReallocStream( CMesh::FACENORMALS,max_vert_num );

	uint32 nSubsets = outMesh.GetSubSetCount();
	for(i=0; i<nSubsets; i++) // temporarily store original subset index in nNumVerts
		outMesh.m_subsets[i].nNumVerts = i;
	
	// Sort subset depending on thier physicalization type (don't do it for character meshes (with mapping)).
	if (!m_pVertexMap) 
	{
		for(i=0; i<(uint32)outMesh.m_subsets.size(); i++)
		{
			SMeshSubset &outSubset = outMesh.m_subsets[i];
			if (outSubset.nPhysicalizeType==PHYS_GEOM_TYPE_DEFAULT)
			{
				// If normal physicalize chunk, put it at the begining.
				// sort subsets to make physicalized ones go first (needed for breakable objects);
				SMeshSubset mss = outSubset;
				outMesh.m_subsets.erase(outMesh.m_subsets.begin()+i);
				outMesh.m_subsets.insert(outMesh.m_subsets.begin(), mss);
			}
		}
		// If it is a proxy put the chunk at the end.
		{
			int i;
			for(i = (int)outMesh.m_subsets.size()-1; i >= 0; i--)
			{
				SMeshSubset &outSubset = outMesh.m_subsets[i];
				if (outSubset.nPhysicalizeType!=PHYS_GEOM_TYPE_NONE && outSubset.nPhysicalizeType!=PHYS_GEOM_TYPE_DEFAULT)
				{
					// If it is a proxy put the chunk at the end.
					SMeshSubset mss = outSubset;
					outMesh.m_subsets.erase(outMesh.m_subsets.begin()+i);
					outMesh.m_subsets.push_back(mss);
				}
			}
		}
	}
	/*int mapSubset[MAX_SUB_MATERIALS];
	for(i=0; i<nSubsets; i++)
		mapSubset[outMesh.m_subsets[i].nNumVerts] = i;
	for(i=0; i<(uint32)mesh.m_numFaces; i++)
		mesh.m_pFaces[i].nSubset = mapSubset[mesh.m_pFaces[i].nSubset];*/


	// Create vertex buffer with sequence of (possibly non-unique) vertices, 3 verts per face
	// for each material id.
	for (int t = 0; t < outMesh.GetSubSetCount(); t++) 
	{
		SMeshSubset &subset = outMesh.m_subsets[t];
		// memorize the starting index of this material's face range
		subset.nFirstIndexId = buff_vert_count;

		// scan through all the faces using the shader #t
		uint32 nNumVertsInTable = m_vhash_table[subset.nNumVerts].size();
		for(uint32 i=0; i<nNumVertsInTable; ++i)
		{
			SMeshFace *pFace = m_vhash_table[subset.nNumVerts][i];

			//recalculate face normal
			Vec3 vTriEdgeA, vTriEdgeB, vFaceNormal;
			if (flags & MESH_COMPILE_VOXEL)
			{
				vTriEdgeA = mesh.m_pPositions[pFace->v[1]] - mesh.m_pPositions[pFace->v[0]];
				vTriEdgeB = mesh.m_pPositions[pFace->v[2]] - mesh.m_pPositions[pFace->v[0]];
				vFaceNormal = vTriEdgeA.Cross(vTriEdgeB);
				//vFaceNormal.Set(0.0f, 0.0f, 0.0f);
				//vFaceNormal += mesh.m_pNorms[pFace->v[2]];
				//vFaceNormal += mesh.m_pNorms[pFace->v[1]];
				//vFaceNormal += mesh.m_pNorms[pFace->v[0]];
				vFaceNormal.Normalize();
			}

			for (int v = 0; v < 3; ++v)
			{
				assert( pFace->t[v] < mesh.m_nCoorCount || !mesh.m_nCoorCount );

				CopyMeshVertex( outMesh,buff_vert_count,mesh,pFace->v[v], pFace->t[v] );
				if(mesh.m_nCoorCount)
					outMesh.m_pTexCoord[buff_vert_count] = mesh.m_pTexCoord[pFace->t[v]];
				
				//recalculate face normal for voxel
				if (flags & MESH_COMPILE_VOXEL)
				{
					outMesh.m_pFaceNorms[buff_vert_count] = vFaceNormal;
				}

				if (pBasises)
				{
					SBasisFace *pTFace = &m_thash_table[subset.nNumVerts][i];
					outMesh.m_pTangents[buff_vert_count] = pBasises[pTFace->v[v]];
				}

				// store subset id to prevent vertex sharing between materials during re-compacting
				vertexMatId[buff_vert_count] = pFace->nSubset;

				buff_vert_count++;
			}
		}

		// there are faces belonging to this material(shader) #t, if number of indices > 0
		subset.nNumIndices = buff_vert_count - subset.nFirstIndexId;
	}

	//////////////////////////////////////////////////////////////////////////
	// Reduce size of the mesh to the actual number of vertices.
	//////////////////////////////////////////////////////////////////////////
	outMesh.SetVertexCount( buff_vert_count );
	if(mesh.m_pTexCoord)
		outMesh.SetTexCoordsCount( buff_vert_count ); // Same number of tex coords as of vertices.
	
	if (flags & MESH_COMPILE_TANGENTS)
		outMesh.ReallocStream( CMesh::TANGENTS,buff_vert_count );
	if (mesh.m_pSHInfo && mesh.m_pSHInfo->pSHCoeffs)
		outMesh.ReallocStream( CMesh::SHCOEFFS,buff_vert_count );
	if (mesh.m_pColor0) outMesh.ReallocStream( CMesh::COLORS_0,buff_vert_count );
	if (mesh.m_pColor1) outMesh.ReallocStream( CMesh::COLORS_1,buff_vert_count );
  if (mesh.m_pVertMats) outMesh.ReallocStream( CMesh::VERT_MATS,buff_vert_count );
	if (flags & MESH_COMPILE_VOXEL)
		outMesh.ReallocStream( CMesh::FACENORMALS, buff_vert_count );
	//////////////////////////////////////////////////////////////////////////

	// Index the mesh (Compact Vertices): detect and delete duplicate vertices
	CompactBuffer( outMesh,vertexMatId, flags );

	if (m_pMapFaceToFace0)
	{
		m_pMapFaceToFace0->resize(outMesh.GetIndexCount()/3);
		for(int i=outMesh.GetIndexCount()/3-1;i>=0;i--)
			(*m_pMapFaceToFace0)[i] = i;
	}

//#ifndef _DEBUG
	bool bFoundDegenerateFaces = CheckForDegenerateFaces(outMesh);

	if (flags & MESH_COMPILE_OPTIMIZE)
	{
		StripifyMesh( outMesh );
	}
//#endif // _DEBUG

	FindVertexRanges( outMesh );

	//CalcFaceNormals( outMesh );

	//////////////////////////////////////////////////////////////////////////
	// Clear static hash tables for later use.
	//////////////////////////////////////////////////////////////////////////
	for (i = 0; i < MAX_SUB_MATERIALS; i++)
	{
		m_vhash_table[i].resize(0);
		m_thash_table[i].resize(0);
	}

	// Copy modified mesh back to original one.
	mesh.Copy(outMesh);

	SAFE_DELETE_ARRAY (vertexMatId);
	SAFE_DELETE_ARRAY (pBasises);
	SAFE_DELETE_ARRAY (pBasisIndices);

	if (bFoundDegenerateFaces)
	{
		m_LastError.Format("Mesh contains degenerate faces.");
		return false;
	}

	return true;
}







//////////////////////////////////////////////////////////////////////////

template<class F> int intersect_lists(F *pSrc0,int nSrc0, F *pSrc1,int nSrc1, F *pDst)
{
	int i0,i1,n; F ares;
	for(i0=i1=n=0; isneg(i0-nSrc0) & isneg(i1-nSrc1); i0+=isneg(pSrc0[i0]-ares-1),i1+=isneg(pSrc1[i1]-ares-1)) {
		pDst[n] = ares = min(pSrc0[i0],pSrc1[i1]); n += iszero(pSrc0[i0]-pSrc1[i1]);
	}
	return n;
}


bool CMeshCompiler::StripifyMesh( CMesh &mesh )
{
	int i;

	////////////////////////////////////////////////////////////////////////////////////////
	// Stripping stuff
	SetCacheSize(CACHESIZE_GEFORCE3);

	SetMinStripSize(0);
	
	SetListsOnly(true);
	SetStitchStrips(false);

	CMesh newMesh;
	newMesh.Copy( mesh );

	int vertFirst = 0;
	int nCurNewIndex = 0;

	if (m_pIndexMap)
	{
		m_pIndexMap->resize(mesh.GetIndexCount());
		memcpy( &(*m_pIndexMap)[0],mesh.m_pIndices,sizeof(uint16)*mesh.GetIndexCount() );
	}
	if (m_pVertexMap)
	{
		m_pVertexMap->resize(mesh.GetVertexCount());
	}

	int j,k,iface,iface0,*pVtxTris,*pEdgeTris,*pTris,nTris,nTris0,ivtxMin,ivtxMax;
	DynArray<uint16> oldMapFaceToFace0;
	if (m_pMapFaceToFace0)
	{
		oldMapFaceToFace0 = *m_pMapFaceToFace0;
		pTris = new int[mesh.m_numVertices+1]; // for each used vtx, points to the corresponding tri list start
		pVtxTris = new int[(mesh.m_nIndexCount/3)*4]; // holds tri lists for each used vtx
		pEdgeTris = pVtxTris+mesh.m_nIndexCount;
		iface = 0;
	}

	//stripify!
	for (i = 0; i < newMesh.GetSubSetCount(); i++)
	{
		SMeshSubset &subset = newMesh.m_subsets[i];

		if (!subset.nNumIndices)
		{
			continue;
			assert(subset.nNumIndices);
		}

		PrimitiveGroup* pOldPG = 0;
		unsigned short numGroups = 0;
		GenerateStrips( mesh.m_pIndices+subset.nFirstIndexId,subset.nNumIndices, &pOldPG, &numGroups );

		if (m_pMapFaceToFace0) 
		{
			for(j=0,ivtxMin=(1<<30),ivtxMax=-(1<<30); j<subset.nNumIndices; j++)
			{
				pTris[k = mesh.m_pIndices[subset.nFirstIndexId+j]] = 0;
				ivtxMin += k-ivtxMin & (k-ivtxMin)>>31; 
				ivtxMax += k-ivtxMax & (ivtxMax-k)>>31;
			}
			pTris[ivtxMax+1] = 0;
			for(j=0;j<subset.nNumIndices;j++)
				pTris[mesh.m_pIndices[subset.nFirstIndexId+j]]++;
			for(j=ivtxMin;j<=ivtxMax;j++) pTris[j+1] += pTris[j];
			for(j=subset.nNumIndices/3-1;j>=0;j--) for(k=0;k<3;k++) 
				pVtxTris[--pTris[mesh.m_pIndices[j*3+k+subset.nFirstIndexId]]] = j;
			iface0 = subset.nFirstIndexId/3;

			for(j=0;j<numGroups;j++) for(k=0;k<pOldPG[j].numIndices;k+=3,iface++)
			{
				nTris0 = intersect_lists(
					pVtxTris+pTris[pOldPG[j].indices[k  ]], pTris[pOldPG[j].indices[k  ]+1]-pTris[pOldPG[j].indices[k  ]],
					pVtxTris+pTris[pOldPG[j].indices[k+1]], pTris[pOldPG[j].indices[k+1]+1]-pTris[pOldPG[j].indices[k+1]], pEdgeTris);
				nTris = intersect_lists(
					pVtxTris+pTris[pOldPG[j].indices[k+2]], pTris[pOldPG[j].indices[k+2]+1]-pTris[pOldPG[j].indices[k+2]],
					pEdgeTris,nTris0, pEdgeTris+nTris0);
				(*m_pMapFaceToFace0)[iface] = oldMapFaceToFace0[pEdgeTris[nTris0]+iface0];
			}
		}

		//remap!
		PrimitiveGroup *pPrimitiveGroups = 0;
		RemapIndices(pOldPG, numGroups, mesh.GetVertexCount(), &pPrimitiveGroups );

		int nMin =  999999;
		int nMax = -999999;

		//loop through all indices, copying from oldVB -> newVB
		//note that this will do numIndices copies, instead of numVerts copies,
		// which is extraneous.  Deal with it! ;-)
		int nFirstIndex = 0;
		subset.nFirstIndexId = nCurNewIndex;
		for(int groupCtr = 0; groupCtr < numGroups; groupCtr++)
		{
			for(unsigned int indexCtr = 0; indexCtr < pPrimitiveGroups[groupCtr].numIndices; indexCtr++)
			{
				//grab old index
				int oldVertex = pOldPG[groupCtr].indices[indexCtr];
				//grab new index
				int newVertex = pPrimitiveGroups[groupCtr].indices[indexCtr] + vertFirst;
				
				nMin = min(nMin, newVertex);
				nMax = max(nMax, newVertex);

				if (nCurNewIndex >= newMesh.GetIndexCount())
				{
					newMesh.SetIndexCount( newMesh.GetIndexCount()*2 );
				}
				newMesh.m_pIndices[nCurNewIndex++] = newVertex;

				if (m_pVertexMap)
				{
					(*m_pVertexMap)[oldVertex] = newVertex;
				}

				//copy from old -> new vertex buffer
				CopyMeshVertex( newMesh,newVertex,mesh,oldVertex,oldVertex );
			}
			nFirstIndex += pPrimitiveGroups[groupCtr].numIndices;
		}
		if (pPrimitiveGroups)
			delete []pPrimitiveGroups;
		if (pOldPG)
			delete []pOldPG;

		subset.nNumIndices = nFirstIndex;
		subset.nFirstVertId = nMin;
		subset.nNumVerts = nMax-nMin+1;
		vertFirst += subset.nNumVerts;
	}

	if (m_pMapFaceToFace0)
		delete[] pVtxTris,delete[] pTris;

	// Set exact number of indices.
	newMesh.SetIndexCount( nCurNewIndex );

	// Copy new mesh to source mesh.
	mesh.Copy( newMesh );

	return true;
}

//////////////////////////////////////////////////////////////////////////
inline int FindInBuffer( CMesh &mesh,int nIndex,int nMatInfo,uint16 *hash,int nHashSize,uint8 *newVertexMatId , bool bVoxelSharing=false)
{
	for(int i = 0; i < nHashSize; i++)
  {
    int id = hash[i];

    bool bCompRes = false;

		//we need special sharing of vertices for voxels
		if (bVoxelSharing)
		{
			bCompRes = CompareVoxelMeshVertices(mesh,nIndex,id);
		}
		else
		{
			bCompRes = CompareMeshVertices(mesh,nIndex,id);			
		}

		if (bCompRes)
    {
			if (newVertexMatId[id] == nMatInfo)
				return id;
    }
  }

  return -1;
}

//////////////////////////////////////////////////////////////////////////
void CMeshCompiler::CompactBuffer( CMesh &mesh,uint8 *vertexMatId, int flags )
{
	if (!mesh.GetVertexCount())
		return;

	int vert_num_before = mesh.GetVertexCount();

	CMesh newMesh;
	newMesh.Copy( mesh );

	unsigned int newVertex = 0;
	std::vector<uint8> newVertexMatId;

	// Empty index hash table.
	for (int i = 0; i < 256; i++)
	{
		m_index_hash_table[i].resize(0);
	}

	m_tempIndices.resize(0);
	m_tempIndices.reserve( mesh.GetVertexCount()*3 );

	// Hash scale to get max resolution for hash table.
	float fHashScale = 256.0f / mesh.m_bbox.GetSize().GetLength();

	if (fHashScale > 1e8f)
		fHashScale = 1e8f;

	for(unsigned int v=0; v<(unsigned int)mesh.GetVertexCount(); v++)
	{
		Vec3 &p = mesh.m_pPositions[v];
		uint8 nHashInd = (uint8)((p.x + p.y + p.z)*fHashScale);
		int nMatId = vertexMatId[v];
		uint16 *hash = 0;
		int nHashSize = m_index_hash_table[nHashInd].size();
		if (nHashSize > 0)
			hash = &m_index_hash_table[nHashInd][0];
		uint8 *pNewVertexMatId = 0;
		if (newVertexMatId.size() > 0)
			pNewVertexMatId = &newVertexMatId[0];

		bool bVoxelMesh;
		if (flags&MESH_COMPILE_VOXEL)
			bVoxelMesh = true;
		else
			bVoxelMesh = false;

		int find = FindInBuffer( newMesh,v,nMatId,hash,nHashSize,pNewVertexMatId, bVoxelMesh );
		//if (m_pVertexMap) //Timur, Hack.
			//find = -1;
		if (find < 0)
		{ // not found
			CopyMeshVertex( newMesh,newVertex,mesh,v,v );

			m_tempIndices.push_back(newVertex);
			newVertexMatId.push_back(vertexMatId[v]);
			m_index_hash_table[nHashInd].push_back(newVertex);

			newVertex++;
		}
		else
		{ // found
			m_tempIndices.push_back(find);
		}
	}

	if (m_pVertexMap)
	{
		m_pVertexMap->resize( mesh.GetVertexCount() );
	}

	newMesh.SetVertexCount( newVertex );
	if(mesh.m_pTexCoord)
		newMesh.SetTexCoordsCount( newMesh.GetVertexCount() );
	if (mesh.m_pColor0)
		newMesh.ReallocStream( CMesh::COLORS_0,newMesh.GetVertexCount() );
	if (mesh.m_pColor1)
		newMesh.ReallocStream( CMesh::COLORS_1,newMesh.GetVertexCount() );
	if (mesh.m_pTangents)
		newMesh.ReallocStream( CMesh::TANGENTS,newMesh.GetVertexCount() );
	if (mesh.m_pSHInfo)
		newMesh.ReallocStream( CMesh::SHCOEFFS,newMesh.GetVertexCount() );

	// Copy indices.
	newMesh.SetIndexCount( m_tempIndices.size() );
	memcpy( newMesh.m_pIndices,&m_tempIndices[0],m_tempIndices.size()*sizeof(m_tempIndices[0]) );

	mesh.Copy( newMesh );

	//int ratio = 100*(newMesh.GetVertexCound())/vert_num_before;
	//Log("  Size after compression = %d %s ( %d -> %d )", ratio, "%", vert_num_before, newMesh.GetVertexCount());
}

//////////////////////////////////////////////////////////////////////////
bool CMeshCompiler::CheckForDegenerateFaces( CMesh &mesh )
{
	bool bAnyFacesFound = false;
	int iface=0,iface0=0;
	// Remove degenerated triangles in the generated mesh.
	for (int i = 0; i < mesh.GetSubSetCount(); i++)
	{
		SMeshSubset &subset = mesh.m_subsets[i];

		for (int j = subset.nFirstIndexId; j < subset.nFirstIndexId+subset.nNumIndices; j+=3,iface0++)
		{
			// the face in material #i consists of vertices i0,i1,i2:
			int i0 = mesh.m_pIndices[j+0];
			int i1 = mesh.m_pIndices[j+1];
			int i2 = mesh.m_pIndices[j+2];
			assert (i0<65536 && i1<65536 && i2<65536);
			if (i0 == i1 || i0 == i2 || i1 == i2)
				bAnyFacesFound = true;
		}
	}

	return bAnyFacesFound;
}

//////////////////////////////////////////////////////////////////////////
void CMeshCompiler::FindVertexRanges( CMesh &mesh )
{
	// Lock the index buffer and get the pointer
	int nNumIndices = mesh.GetIndexCount();

	// Find vertex range (both index and spacial ranges) for each material (needed for rendering)
	for (int i = 0; i < mesh.GetSubSetCount(); i++)
	{
		SMeshSubset &subset = mesh.m_subsets[i];
		
		if (subset.nNumIndices+subset.nFirstIndexId > nNumIndices)
		{
			assert(!subset.nNumIndices);
			continue;
		}
		int nMin =  999999;
		int nMax = -999999;
		Vec3 vMin = SetMaxBB();
		Vec3 vMax = SetMinBB();

		for (int j = subset.nFirstIndexId; j < subset.nNumIndices+subset.nFirstIndexId; j++)
		{
			int index = mesh.m_pIndices[j];
			Vec3 v = mesh.m_pPositions[index];
			vMin.CheckMin(v);
			vMax.CheckMax(v);
			nMin = min(nMin, index);
			nMax = max(nMax, index);
		}
		subset.vCenter = (vMin + vMax) * 0.5f;
		subset.fRadius = (vMin - subset.vCenter).GetLength();
		subset.nFirstVertId = nMin;
		subset.nNumVerts = nMax-nMin+1;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMeshCompiler::UpdateFaces( CMesh &mesh )
{
	mesh.SetFacesCount( mesh.GetIndexCount()/3 );

	int nface = 0;
	for (int t = 0; t < mesh.GetSubSetCount(); t++) 
	{
		SMeshSubset &subset = mesh.m_subsets[t];
		for (int i = subset.nFirstIndexId; i < subset.nFirstIndexId+subset.nNumIndices; i += 3)
		{
			SMeshFace &face = mesh.m_pFaces[nface++];
			face.v[0] = mesh.m_pIndices[i];
			face.v[1] = mesh.m_pIndices[i+1];
			face.v[2] = mesh.m_pIndices[i+2];
			face.t[0] = face.v[0];
			face.t[1] = face.v[1];
			face.t[2] = face.v[2];
			face.nSubset = t;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// Buffer optimizer
/////////////////////////////////////////////////////////////////////////////////////

inline int FindInPosBuffer(const Vec3 & vPosToFind, const Vec3 * pVertBuff, std::vector<int> *pHash,float fEpsilon )
{ 
	for(uint32 i=0,num = pHash->size(); i<num; i++) 
		if(CMeshCompiler::IsEquivalentVec3dCheckYFirst(pVertBuff[(*pHash)[i]], vPosToFind, fEpsilon)) 
			return (*pHash)[i];

	return -1;
}

//////////////////////////////////////////////////////////////////////////
void CMeshCompiler::WeldPositions( Vec3 *pVertices, int &nVerts, std::vector<int> &indices,float fEpsilon, const AABB & boxBoundary )
{
	Vec3 *pTmpVerts = new Vec3[nVerts];

	int nCurVertex = 0;
	std::vector<int> newIndices;
	std::vector<int> arrHashTable[256];
	newIndices.reserve( indices.size() );

	if (m_pIndexMap)
	{
		m_pIndexMap->clear();
		m_pIndexMap->reserve( indices.size() );
	}

  float fHashElemSize = 256.0f / max(boxBoundary.max.x - boxBoundary.min.x, 0.01f);

	for(uint32 i=0; i < indices.size(); i++)
	{
		int v = indices[i];

		assert(v<nVerts);

    float fHashValue = ((pVertices[v].x - boxBoundary.min.x)*fHashElemSize);

		Vec3 & vPos = pVertices[v];

		bool bInRange(
			vPos.x>boxBoundary.min.x && 
			vPos.y>boxBoundary.min.y && 
			vPos.z>boxBoundary.min.z && 
			vPos.x<boxBoundary.max.x && 
			vPos.y<boxBoundary.max.y && 
			vPos.z<boxBoundary.max.z);

		int find = FindInPosBuffer( vPos, pTmpVerts, &arrHashTable[(unsigned char)(fHashValue  )], bInRange ? fEpsilon : 0.01f);

		if(find<0)
		{
			arrHashTable[(unsigned char)(fHashValue	 )].push_back(nCurVertex);

			if(bInRange && fEpsilon>0.01f)
			{
				find = FindInPosBuffer( vPos, pTmpVerts, &arrHashTable[(unsigned char)(fHashValue+1)], fEpsilon );
				if(find<0)
				{
					arrHashTable[(unsigned char)(fHashValue+1)].push_back(nCurVertex);

					find = FindInPosBuffer( vPos, pTmpVerts, &arrHashTable[(unsigned char)(fHashValue-1)], fEpsilon );
					if(find<0)
						arrHashTable[(unsigned char)(fHashValue-1)].push_back(nCurVertex);
				}
			}
		}

		if(find<0)
		{
			pTmpVerts[nCurVertex] = vPos;
			newIndices.push_back(nCurVertex);
			nCurVertex++;
		}
		else
		{
			newIndices.push_back(find);
		}

		if (m_pIndexMap)
			m_pIndexMap->push_back(v);
	}

	indices = newIndices;

	nVerts = nCurVertex;
	memcpy( pVertices, pTmpVerts, nCurVertex*sizeof(Vec3));

	delete []pTmpVerts;
}

//////////////////////////////////////////////////////////////////////////
void CMeshCompiler::ShareVertices( CMesh &mesh )
{

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Progressive mesh builder implementation.

// Temporary vertex and triangle classes.
struct	PVertex;
struct	PTri;

template<class T>
class	PList : public std::vector<T>	{
public:
	// Standart constructors.
	PList() {};

	void	add( const T& t ) { push_back( t ); };
	void	addUnique( const T& t ) { if (!isContain(t)) push_back( t ); };
	void	del( int i ) { this->erase( this->begin() + i ); }
	void	del( const T& t ) {
		int i = find( t );
		if (i >= 0) this->erase( this->begin() + i );
	}

	int		find( const T& t ) const {
		for (unsigned int i = 0; i < this->size(); i++) {
			if (t == (*this)[i]) return i;
		}
		return -1;
	}

	bool	isContain( const T& t ) const {
		for (unsigned int i = 0; i < this->size(); i++) {
			if (t == (*this)[i]) return true;
		}
		return false;
	}
};

// Temp tri structure.
struct PTri
{
	PVertex	*v[3];
	Vec3	normal;
	uint16	tex[3];

	// Ctor.
	PTri( PVertex *v0,PVertex *v1,PVertex *v2,uint16 tex0,uint16 tex1,uint16 tex2 );
	~PTri();

	void	replace( PVertex *from,PVertex *to );

	// Return true if tri contain this vertex.
	bool	is_contain( PVertex *vr )	{	if (vr == v[0] || vr == v[1] || vr == v[2]) return true; else return false; }
	bool	is_neithbor( PTri *tri );	// Return true if triangle tri share edge with this traingle.
	int		get_index( PVertex *v );		// Return index of vertex in triangle.

	uint16	texat( PVertex *v );
	void		settexat( PVertex *v,uint16 t );

	// Define compare operator, (compare pointers).
	bool	operator ==( const PTri &tri ) { return this == &tri; };
};

// Temp vertex structure.
struct PVertex
{
	Vec3		pos;
	int				index;
	PVertex*	collapseTo;		// neithboard vertex,this vertex collapsed to.
	float			collapseCost;
	uint16		bone;

	PList<PVertex*>	neighbor;	// Adjacent vertices.
	PList<PTri*>		tris;			// Adjacent triangles.

	PVertex( const Vec3 &p, int i,int b=0 ) { pos = p; index = i; bone = b; };
	~PVertex();

	void	remove_ifnon_neighbor( PVertex *v );
	bool	is_border();

	// Define compare operator, (compare pointers).
	bool	operator ==( const PVertex &v ) { return this == &v; };

	// Less operator used to sort vertices by cost.
	bool	operator <( const PVertex &v ) { return collapseCost < v.collapseCost; };
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline PVertex::~PVertex()
{
	// Delete this vertex from all neighbors of this vertex.
	//for (int i = 0; i < neighbor.size(); i++)	{
	//		neighbor[i]->neighbor.del( this );
	//	}
	while (!neighbor.empty()) {
		neighbor[0]->neighbor.del( this );
		neighbor.del( neighbor[0] );
	}

	for (int i = 0; i < (int)tris.size(); i++)	{
		PTri *tri = tris[i];
		if (tri->v[0] == this) tri->v[0] = 0;
		else if (tri->v[1] == this) tri->v[1] = 0;
		else if (tri->v[2] == this) tri->v[2] = 0;
	}
};

inline void	PVertex::remove_ifnon_neighbor( PVertex *v )
{
	if (!neighbor.isContain(v)) return;
	// Skip remove if v is vertex of one of neighbor tris.
	for (int i = 0; i < (int)tris.size(); i++)	{
		if (tris[i]->is_contain(v)) return;
	}
	// delete neighbor vertex.
	neighbor.del( v );
}

inline bool	PVertex::is_border()	{
	for (int i = 0; i < (int)neighbor.size(); i++)	{
		int count = 0;
		for (int j = 0; j < (int)tris.size(); j++)	{
			if (tris[j]->is_contain(neighbor[i])) {
				count++;
			}
		}
		if (count == 1) {
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
inline PTri::PTri( PVertex *v0,PVertex *v1,PVertex *v2,uint16 tex0,uint16 tex1,uint16 tex2 )
{
	assert(v0!=v1 && v1!=v2 && v2!=v0);

	v[0] = v0; v[1] = v1; v[2] = v2;
	tex[0] = tex0; tex[1] = tex1; tex[2] = tex2;

	for (int i = 0; i < 3; i++) {
		v[i]->tris.add( this );
		for (int j = 0; j < 3; j++)	if (i != j) {
			v[i]->neighbor.addUnique( v[j] );
		}
	}

	// Calc tri normal.
	normal = ( (v[1]->pos - v[0]->pos).Cross( v[2]->pos - v[0]->pos) ).GetNormalized();
}

inline PTri::~PTri()	{
	// Remove this tri from neighbor first.
	int i;
	for (i = 0; i < 3; i++) {
		if (v[i]) v[i]->tris.del( this );
	}

	// Unlink edges.
	for (i = 0; i < 3; i++) {
		int i2 = (i+1)%3;
		if (!v[i] || !v[i2]) continue;
		v[i]->remove_ifnon_neighbor(v[i2]);
		v[i2]->remove_ifnon_neighbor(v[i]);
	}
}

inline void	PTri::replace( PVertex *from,PVertex *to )	{
	assert( from && to );
	assert( from != to );
	assert( to != v[0] && to != v[1] && to != v[2] );
	assert( from == v[0] || from == v[1] || from == v[2] );

	from->tris.del( this );	// Delete this triangle from old vertex tri list.
	to->tris.add( this );		// Add this triangle to new vertex neighbor tris.

	// Replace (from) with (to) vertex.
	if (from == v[0])	{
		v[0] = to;
	} else if (from == v[1])	{
		v[1] = to;
	} else if (from == v[2])	{
		v[2] = to;
	}

	int i;
	for (i = 0; i < 3; i++) {
		from->remove_ifnon_neighbor( v[i] );
		v[i]->remove_ifnon_neighbor( from );
	}

	for (i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++)	if (i != j) {
			v[i]->neighbor.addUnique( v[j] );
		}
	}

	// Recalc tri normal after replace.
	normal = ( (v[1]->pos - v[0]->pos).Cross(v[2]->pos - v[0]->pos) );
	normal.NormalizeSafe();
}

bool PTri::is_neithbor( PTri *tri ) {
	int shareCount = 0;
	for (int i = 0; i < 3; i++)	{
		if (tri->is_contain( v[i] )) shareCount++;
	}
	// If tri shares exactly 2 vertices (edge) if this triangle it is neithbor.
	if (shareCount == 2) return true;
	return false;
}

// Return index of vertex in triangle.
inline int	PTri::get_index( PVertex *vr )	{
	if (vr == v[0]) return 0;
	if (vr == v[1]) return 1;
	if (vr == v[2]) return 2;
	assert( 0 );
	return -1;
}

uint16	PTri::texat( PVertex *vr )
{
	if (vr == v[0]) return tex[0];
	if (vr == v[1]) return tex[1];
	if (vr == v[2]) return tex[2];
	assert( 0 );
	return ~0;
}

void	PTri::settexat( PVertex *vr,uint16 t )
{
	if (vr == v[0]) { tex[0] = t; return; }
	if (vr == v[1]) { tex[1] = t; return; }
	if (vr == v[2]) { tex[2] = t; return; }
	assert( 0 );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Progressive mesh builder class.
class	CProgMeshBuilder
{
public:
	CProgMeshBuilder();
	~CProgMeshBuilder();
	
	void	Build( CMesh &mesh,uint16 bones[],std::vector<uint16> &mapping,std::vector<uint16> &permutation );

	bool	bProtectTexture;
	bool	bProtectBones;
	bool	bProtectBorder;

private:
	float computeEdgeCollapseCost( PVertex *u,PVertex *v );
	void	computeEdgeCostAtVertex( PVertex *v );
	void	collapseVertex( PVertex *u );
	PVertex*	minumumCostEdge();

	void	addVertex( PVertex *v );
	void	removeVertex( PVertex *v );
	PVertex* getFirstVertex();

	typedef	std::multimap<float,PVertex*>	Verts;
	Verts m_verts;	// Array of vertices.
	typedef	std::set<PTri*>	Tris;
	Tris m_tris;		// Set of triangles.

	int		m_texCoordCount;
};

//////////////////////////////////////////////////////////////////////////
// class CProgressiveMesh class encapuslates mesh simplification algorithm.
//////////////////////////////////////////////////////////////////////////
class	CProgressiveMesh
{
public:
	void Create( CMesh &mesh );
	void ReduceToLevel( CMesh &mesh,float level );
	
	// Remap vertex index to be withing max vertices.
	inline uint32 RemapVertex( uint32 vert,uint32 maxVerts )
	{
		while (vert >= maxVerts) {
			vert = m_collapseMap[vert];
		}
		return vert;
	}
private:
	std::vector<uint16> m_collapseMap;
};

CProgMeshBuilder::CProgMeshBuilder()
{
	bProtectTexture = true;
	bProtectBones = true;
	bProtectBorder = false;
}

CProgMeshBuilder::~CProgMeshBuilder()	{
	for (Verts::iterator vi = m_verts.begin(); vi != m_verts.end(); vi++)	{
		delete vi->second;
	}
	for (Tris::iterator t = m_tris.begin(); t != m_tris.end(); t++) {
		delete *t;
	}
}

void	CProgMeshBuilder::addVertex( PVertex *v )	{
	m_verts.insert( Verts::value_type( v->collapseCost,v ) );
}

PVertex*	CProgMeshBuilder::getFirstVertex()	{
	return m_verts.begin()->second;
}

void	CProgMeshBuilder::removeVertex( PVertex *v )	{
	Verts::iterator low = m_verts.lower_bound( v->collapseCost );
	Verts::iterator up = m_verts.upper_bound( v->collapseCost );

	for (Verts::iterator vi = low; vi != up; vi++)	{
		if (vi->second == v)	{
			m_verts.erase( vi );
			return;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Compute cost of collapsing edge from u to v.
//
// if we collapse edge uv by moving u to v then how 
// much different will the model change, i.e. how much "error".
// Texture, vertex normal, and border vertex code was removed
// to keep this demo as simple as possible.
// The method of determining cost was designed in order 
// to exploit small and coplanar regions for
// effective polygon reduction.
// Is is possible to add some checks here to see if "folds"
// would be generated.  i.e. normal of a remaining face gets
// flipped.  I never seemed to run into this problem and
// therefore never added code to detect this case.
//
float CProgMeshBuilder::computeEdgeCollapseCost( PVertex *u,PVertex *v )
{
	float edgelength = ( v->pos - u->pos ).GetLength();
	float curvature = 0.001f;

	// find the "sides" triangles that are on the edge uv
	PTri *sides[512];	// Max 512 triangles per vertex (should be enough).
	int		numSides = 0;

	int i;
	for (i = 0; i < (int)u->tris.size(); i++) {
		if (u->tris[i]->is_contain(v)) {
			sides[numSides++] = u->tris[i];
		}
	}

	bool border = u->is_border();

	// if (u->is_border() && numSides > 1) {
	if (border && numSides > 0 && bProtectBorder) {
		return 9999.9f;
	}

	// Lock vertices attached to different bones.
	if (bProtectBones && u->bone != v->bone) {
		return 9999.9f;
	}

	if (border && numSides > 1) {
		curvature = 1;
	}

	// use the triangle facing most away from the sides 
	// to determine our curvature term
	for (i = 0; i < (int)u->tris.size(); i++) {
		float mincurv = 1; // curve for face t1 and closer side to it.
		for (int j = 0; j < numSides; j++)	{
			if (u->tris[i] == sides[j])	{
				mincurv = 0;
				//break;
			}
			float dotprod = u->tris[i]->normal * sides[j]->normal;
			float val = 0.5f*(1.002f-dotprod);
			mincurv = min(mincurv,val);
		}
		curvature = max(curvature,mincurv);
	}

	if (bProtectTexture)
	{
		int j;
		// check for texture seam ripping
		for (i = 0; i < (int)u->tris.size(); i++) {
			for (j = 0; j < numSides; j++) {
				if (u->tris[i]->texat(u) == sides[j]->texat(u)) break;
			}
			if (j == numSides) {
				// we didn't find a triangle with edge uv that shares texture coordinates with face i at vertex u
				curvature = 1;
				break;
			}
		}
	}

	// the more coplanar the lower the curvature term.
	return edgelength * curvature;
}


///////////////////////////////////////////////////////////////////////////////
// compute the edge collapse cost for all edges that start
// from vertex v.  Since we are only interested in reducing
// the object by selecting the min cost edge at each step, we
// only cache the cost of the least cost edge at this vertex
// (in member variable l) as well as the value of the 
// cost (in member variable objdist).
//
void CProgMeshBuilder::computeEdgeCostAtVertex( PVertex *v )
{
	if (v->neighbor.size() == 0) {
		// v doesn't have neighbors so it costs nothing to collapse.
		v->collapseTo = 0;
		v->collapseCost = -0.01f;
		return;
	}

	v->collapseCost = 1000000;
	v->collapseTo = 0;

	// search all neighboring edges for "least cost" edge
	for (int i = 0; i < (int)v->neighbor.size(); i++)
	{
		float cost = computeEdgeCollapseCost( v,v->neighbor[i] );
		if (cost < v->collapseCost)
		{
			v->collapseTo = v->neighbor[i];	// Set new candidate for edge collapse.
			v->collapseCost = cost;					// Cache cost of collapsing canditade edge.
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Collapse the edge uv by moving vertex u onto v
// Actually remove tris on uv, then update tris that
// have u to have v, and then remove u.
void CProgMeshBuilder::collapseVertex( PVertex *u )	{
	PVertex *v = u->collapseTo;
	if (!v) {
		// u is a vertex all by itself so just delete it
		removeVertex( u );
		delete u;
		return;
	}

	PVertex* tmp[512];
	int	numTmp = 0;

	int i;
	// make tmp a list of all the neighbors of u
	for (i = 0; i < (int)u->neighbor.size(); i++)	{
		tmp[numTmp++] = u->neighbor[i];
	}

	// Find the "sides" triangles that are on the edge uv
	PTri *sides[512];	// Max 512 triangles per vertex (should be enough).
	int		numSides = 0;
	for (i = 0; i < (int)u->tris.size(); i++) {
		if (u->tris[i]->is_contain(v)) {
			sides[numSides++] = u->tris[i];
		}
	}

	/*TIMUR: disabled for now.
	// Update texture cordinates of remaining triangles.
	for (i = 0; i < u->tris.size(); i++)
	{
	PTri *tri1 = u->tris[i];
	// Only update texture for remaining triangles (only ones that not include v vertex).
	if (tri1->is_contain(v)) continue;
	for (int j = 0; j < numSides; j++)
	{
	PTri *tri2 = sides[j];
	if (tri1 == tri2) continue;
	if (tri1->texat(u) == tri2->texat(u)) {
	tri1->settexat( u,tri2->texat(v) );
	//texmap[tri1->texat(u)] = tri2->texat(v);
	}

	if (tri1->is_neithbor( tri2 ))
	{
	// Texture coordinates from vertex u of triangle tri1 replaced
	// by texture coordinate from vertex v of triangle tri2.
	int i1 = tri1->get_index( u );
	int i2 = tri2->get_index( v );
	int srcIndex = tri1->tex[i1];
	int trgIndex = tri2->tex[i2];

	m_texCoordCount--;	// Collapse one texture coordinate.
	texpermutation[srcIndex] = m_texCoordCount;
	texmap[m_texCoordCount] = trgIndex;

	tri1->tex[i1] = tri2->tex[i2];

	break; // no more neithbors to tri1 sharing veretx u.
	}
	}
	}
	*/

	/*
	// delete triangles on edge uv:
	for (i = u->tris.size()-1; i >= 0; i--) {
	if (u->tris[i]->is_contain(v))	{
	m_tris.erase( u->tris[i] );
	delete u->tris[i];
	}
	}
	*/

	// delete triangles on edge uv.
	for(i = 0; i < numSides; i++) {
		m_tris.erase( sides[i] );
		delete sides[i];
	}

	// update remaining triangles to have v instead of u
	for (i = u->tris.size()-1; i >= 0; i--) {
		u->tris[i]->replace( u,v );
	}

	// Remove vertex u.
	removeVertex( u );
	delete u;

	// recompute the edge collapse costs for neighboring vertices
	for (i = 0; i < numTmp; i++) {
		removeVertex( tmp[i] );
		computeEdgeCostAtVertex( tmp[i] );
		addVertex( tmp[i] );
	}
}

void	CProgMeshBuilder::Build( CMesh &mesh,uint16 bones[],std::vector<uint16> &mapping,std::vector<uint16> &permutation )
{
	int i;
	int vertexCount = mesh.GetVertexCount();
	m_texCoordCount = mesh.GetTexCoordsCount();

	std::vector<PVertex*> tempv;

	tempv.resize( vertexCount );
	// Set vertices.
	if (bones) {
		for (i = 0; i < vertexCount; i++)	{
			tempv[i] = new PVertex( mesh.m_pPositions[i],i,bones[i] );
		}
	} else {
		for (i = 0; i < vertexCount; i++)	{
			tempv[i] = new PVertex( mesh.m_pPositions[i],i );
		}
	}

	// Set triangles.
	for (i = 0; i < mesh.GetFacesCount(); i++)
	{
		const SMeshFace &f = mesh.m_pFaces[i];
		assert( mesh.m_pPositions[f.v[0]] != mesh.m_pPositions[f.v[1]]  );
		assert( mesh.m_pPositions[f.v[0]] != mesh.m_pPositions[f.v[2]]  );
		assert( mesh.m_pPositions[f.v[1]] != mesh.m_pPositions[f.v[2]]  );
		if ((mesh.m_pPositions[f.v[0]] == mesh.m_pPositions[f.v[1]]) ||
				(mesh.m_pPositions[f.v[0]] == mesh.m_pPositions[f.v[2]]) ||
				(mesh.m_pPositions[f.v[1]] == mesh.m_pPositions[f.v[2]]))
				continue;
		m_tris.insert( new PTri( tempv[f.v[0]],tempv[f.v[1]],tempv[f.v[2]],f.t[0],f.t[1],f.t[2] ) );
	}

	/*
  for (i = 0; i < mesh.GetIndexCount(); i+=3)
	{
		m_tris.insert( new PTri( tempv[i],tempv[i+1],tempv[i+2],i,i+1,i+2 ) );
	}
	*/

	mapping.resize( vertexCount );
	permutation.resize( vertexCount );

	// Compute and cache cost of all edge collapses.
	for (i = 0; i < vertexCount; i++)	{
		computeEdgeCostAtVertex( tempv[i] );
		addVertex( tempv[i] );
	}

	// Reduce mesh to 0 vertices.
	while (vertexCount-- > 0)	{
		// Find the edge that when collapsed will affect model the least.
		// Since all our vertices sorted by collapse cost order,
		// first vertex will be with minmal edge collapse cost.
		PVertex *v = getFirstVertex();

		// keep track of this vertex, i.e. the collapse ordering
		permutation[v->index] = vertexCount;

		// keep track of vertex to which we collapse to
		mapping[vertexCount] = (v->collapseTo) ? v->collapseTo->index : 0xFFFF;

		// Collapse this edge
		collapseVertex( v );
	}

	// reorder the mapping data based on the collapse ordering
	for (i = 0; i < (int)mapping.size(); i++) {
		mapping[i] = (mapping[i] == 0xFFFF) ? 0 : permutation[mapping[i]];
	}
}

//////////////////////////////////////////////////////////////////////////
void CProgressiveMesh::Create( CMesh &mesh )
{
	m_collapseMap.resize(0);

	uint16 *bones = NULL;

	int i;
	std::vector<uint16> permutation;
	/////////////////////////////////////////////////////////////////////////////
	// Build progressive mesh.
	CProgMeshBuilder meshBuilder;
	meshBuilder.bProtectBorder = true;
	if (mesh.GetTexCoordsCount() > 0) {
		meshBuilder.bProtectTexture = true;
	} else {
		meshBuilder.bProtectTexture = false;
	}
	meshBuilder.bProtectBones = false;
	meshBuilder.bProtectBones = (bones != 0) ? true : false;
	meshBuilder.Build( mesh,bones,m_collapseMap,permutation );

	CMesh newMesh;
	newMesh.Copy(mesh);

	// Sort vertices in progressive mesh according to edge collapse order.
	for (i = 0; i < mesh.GetVertexCount(); i++)
	{
		int newindex = permutation[i];
		CopyMeshVertex( newMesh,newindex,mesh,i,i );
	}

	uint16 face_permutation[65536];
	uint8 face_index[65536];
	int numFacePermutation = 0;
	memset( face_index,0,sizeof(face_index) );

	for (i = 1; i <= mesh.GetVertexCount(); i++)
	{
		for (int j = 0; j < mesh.GetFacesCount(); j++)
		{
			const SMeshFace &f = mesh.m_pFaces[j];
			int v0 = RemapVertex( permutation[f.v[0]],i );
			int v1 = RemapVertex( permutation[f.v[1]],i );
			int v2 = RemapVertex( permutation[f.v[2]],i );
			if (v0 == v1 || v1 == v2 || v2 == v0) continue;
			if (face_index[j] == 0)
			{
				// Add face if not inside yet.
				face_permutation[numFacePermutation++] = j;
				face_index[j] = 1;
			}
		}
	}

	// Set faces & triangles of progressive mesh.
	for (i = 0; i < mesh.GetFacesCount(); i++)
	{
		const SMeshFace &from = mesh.m_pFaces[face_permutation[i]];
		SMeshFace &to = newMesh.m_pFaces[i];
		for (int j = 0; j < 3; j++)
		{
			int idx = permutation[from.v[j]];	// vertex index.
			to.v[j] = idx;
			to.t[j] = from.t[j];
		}
		to.nSubset = from.nSubset;
	}

	int nIndex = 0;
	newMesh.SetIndexCount( newMesh.GetFacesCount()*3 );
	// Finally set triangles of progressive mesh.
	for (i = 0; i < newMesh.GetFacesCount(); i++)
	{
		const SMeshFace &from = newMesh.m_pFaces[i];
		newMesh.m_pIndices[nIndex] = from.v[0];
		newMesh.m_pIndices[nIndex+1] = from.v[1];
		newMesh.m_pIndices[nIndex+2] = from.v[2];
		nIndex += 3;
	}

	mesh.Copy( newMesh );
}

//////////////////////////////////////////////////////////////////////////
void CProgressiveMesh::ReduceToLevel( CMesh &mesh,float level )
{
	if (level < 0) level = 0;
	if (level > 1) level = 1;

	int newVertexCount = (int)(level*mesh.GetVertexCount());
	if (newVertexCount <= 0) 
		return;
	if (newVertexCount > mesh.GetVertexCount()) newVertexCount = mesh.GetVertexCount();

	int newFaceCount = 0;
	int fcount = mesh.GetFacesCount();
	for (int i = 0; i < fcount; i++)
	{
		SMeshFace &face = mesh.m_pFaces[i];
		int i0 = RemapVertex( face.v[0],newVertexCount );
		int i1 = RemapVertex( face.v[1],newVertexCount );
		int i2 = RemapVertex( face.v[2],newVertexCount );
		face.v[0] = i0;
		face.v[1] = i1;
		face.v[2] = i2;
		face.t[0] = i0;
		face.t[1] = i1;
		face.t[2] = i2;

		if (i0==i1 || i1==i2 || i2==i0) {
			// Later faces must be collapsed.
			break;
		}
		newFaceCount++;
	}
	mesh.SetVertexCount( newVertexCount );
	mesh.SetFacesCount( newFaceCount );
}

//////////////////////////////////////////////////////////////////////////
void CMeshCompiler::ReduceMeshToLevel( CMesh &mesh,float fLevel )
{
	// If mesh have indices.
	if (mesh.GetIndexCount() != 0)
	{
		UpdateFaces(mesh);
	}
	if (mesh.GetFacesCount() == 0 || mesh.GetVertexCount() == 0)
		return;

	CProgressiveMesh progMesh;
	progMesh.Create( mesh );
	progMesh.ReduceToLevel( mesh,fLevel );

	Compile( mesh );
}

//////////////////////////////////////////////////////////////////////////
bool CMeshCompiler::CompareMeshes( CMesh &mesh1,CMesh &mesh2 )
{
	if (mesh1.m_subsets.size() != mesh2.m_subsets.size())
		return false;

	if (mesh1.m_numFaces != mesh2.m_numFaces)
		return false;
	if (mesh1.m_numVertices != mesh2.m_numVertices)
		return false;
	if (mesh1.m_nCoorCount != mesh2.m_nCoorCount)
		return false;
	if (mesh1.m_nIndexCount != mesh2.m_nIndexCount)
		return false;

	if (!mesh1.CompareStreams(mesh2))
		return false;

	/*

	if ((mesh1.m_pFaces == NULL && mesh2.m_pFaces != NULL) ||	(mesh1.m_pFaces != NULL && mesh2.m_pFaces == NULL))
			return false;
	if ((mesh1.m_pTangents == NULL && mesh2.m_pTangents != NULL) ||	(mesh1.m_pTangents != NULL && mesh2.m_pTangents == NULL))
		return false;
	if ((mesh1.m_pTexCoord == NULL && mesh2.m_pTexCoord != NULL) ||	(mesh1.m_pTexCoord != NULL && mesh2.m_pTexCoord == NULL))
		return false;
	if ((mesh1.m_pNorms == NULL && mesh2.m_pNorms != NULL) ||	(mesh1.m_pNorms != NULL && mesh2.m_pNorms == NULL))
		return false;
	if ((mesh1.m_pColor0 == NULL && mesh2.m_pColor0 != NULL) ||	(mesh1.m_pColor0 != NULL && mesh2.m_pColor0 == NULL))
		return false;
	if ((mesh1.m_pColor1 == NULL && mesh2.m_pColor1 != NULL) ||	(mesh1.m_pColor1 != NULL && mesh2.m_pColor1 == NULL))
		return false;

	unsigned int i;
	for (i = 0; i < CMesh::LAST_STREAM; i++)
	{
		if (mesh1.m_streamSize[i] != mesh2.m_streamSize[i])
			return false;
	}

	if (mesh1.m_pFaces && mesh2.m_pFaces)
	{
		if (mesh1.m_numFaces != mesh2.m_numFaces)
			return false;
		for (i = 0; i < mesh1.m_numFaces; i++)
		{
			if (mesh1.m_pFaces[i] != mesh2.m_pFaces[i])
				return false;
		}
	}
	if (mesh1.m_pIndices && mesh2.m_pIndices)
	{
		if (mesh1.m_nIndexCount != mesh2.m_nIndexCount)
			return false;
		for (i = 0; i < mesh1.m_nIndexCount; i++)
		{
			if (mesh1.m_pIndices[i] != mesh2.m_pIndices[i])
				return false;
		}
	}

	if (mesh1.m_pPositions && mesh2.m_pPositions)
	{
		if (mesh1.m_numVertices != mesh2.m_numVertices)
			return false;
		for (i = 0; i < mesh1.m_numVertices; i++)
		{
			if (mesh1.m_pVertices[i] != mesh2.m_pVertices[i])
				return false;
		}
	}
	if (mesh1.m_pNorms && mesh2.m_pNorms)
	{
		for (i = 0; i < mesh1.m_numVertices; i++)
		{
			if (mesh1.m_pNorms[i] != mesh2.m_pNorms[i])
				return false;
		}
	}
	if (mesh1.m_pFaceNorms && mesh2.m_pFaceNorms)
	{
		if (mesh1.m_numFaces != mesh2.m_numFaces)
			return false;
		for (i = 0; i < mesh1.m_numFaces; i++)
		{
			if (mesh1.m_pFaceNorms[i] != mesh2.m_pFaceNorms[i])
				return false;
		}
	}
	if (mesh1.m_pTangents && mesh2.m_pTangents)
	{
		for (i = 0; i < mesh1.m_numVertices; i++)
		{
			if (mesh1.m_pTangents[i] != mesh2.m_pTangents[i])
				return false;
		}
	}
	if (mesh1.m_pColor0 && mesh2.m_pColor0)
	{
		for (i = 0; i < mesh1.m_numVertices; i++)
		{
			if (mesh1.m_pColor0[i] != mesh2.m_pColor0[i])
				return false;
		}
	}
	if (mesh1.m_pColor1 && mesh2.m_pColor1)
	{
		for (i = 0; i < mesh1.m_numVertices; i++)
		{
			if (mesh1.m_pColor1[i] != mesh2.m_pColor1[i])
				return false;
		}
	}
	if (mesh1.m_pSHInfo && mesh2.m_pSHInfo)
	{
		for (i = 0; i < mesh1.m_numVertices; i++)
		{
			if (mesh1.m_pSHInfo[i] != mesh2.m_pSHInfo[i])
				return false;
		}
	}

*/
	return true;
}

void CMeshCompiler::InterpolateVerts(Vec3 & vNewPos, Vec3 & vNewNorm, SMeshTexCoord & NewTexCoord, SMeshTangents & NewTangents,
																		 int i0, int i1, 
																		 PodArray<Vec3> & pPositions, PodArray<Vec3> & pNorms, 
																		 PodArray<SMeshTexCoord> & pTexCoord, PodArray<SMeshTangents> & pTangents)
{
	Vec3 v0 = pPositions[i0];
	Vec3 v1 = pPositions[i1];

	Vec3 n0 = pNorms[i0];
	Vec3 n1 = pNorms[i1];

	SMeshTexCoord t0 = pTexCoord[i0];
	SMeshTexCoord t1 = pTexCoord[i1];

	SMeshTangents b0 = pTangents[i0];
	SMeshTangents b1 = pTangents[i1];

	vNewPos = (v0+v1)/2;
	vNewNorm = (n0+n1).GetNormalizedSafe();
	NewTexCoord.s = (t0.s+t1.s)*0.5f;
	NewTexCoord.t = (t0.t+t1.t)*0.5f;		

	NewTangents = b0;
}

void CMeshCompiler::Tesselate( CMesh &mesh, float fMaxEdgeLen )
{
	// If mesh have indices.
	if (mesh.GetIndexCount() != 0)
		UpdateFaces(mesh);

	if (mesh.GetFacesCount() == 0 || mesh.GetVertexCount() == 0)
		return;

	if(mesh.m_nCoorCount != mesh.m_numVertices)
		return;

	PodArray<Vec3> lstVerts;
	lstVerts.AddList(mesh.m_pPositions, mesh.m_numVertices);
	PodArray<Vec3> lstNorms;
	lstNorms.AddList(mesh.m_pNorms, mesh.m_numVertices);
	PodArray<SMeshTexCoord> lstTexCoords;
	lstTexCoords.AddList(mesh.m_pTexCoord, mesh.m_numVertices);
	PodArray<SMeshTangents> lstTangents;
	lstTangents.AddList(mesh.m_pTangents, mesh.m_numVertices);

	PodArray<SMeshFace> lstFaces;
	lstFaces.AddList(mesh.m_pFaces, mesh.m_numFaces);

	for (int i = 0; i < lstFaces.Count(); i++)
	{
		SMeshFace* pFace = &lstFaces[i];
		
		Vec3 v0 = lstVerts[pFace->v[0]];
		Vec3 v1 = lstVerts[pFace->v[1]];
		Vec3 v2 = lstVerts[pFace->v[2]];

		float fDist01 = v0.GetDistance(v1);
		float fDist02 = v0.GetDistance(v2);
		float fDist12 = v1.GetDistance(v2);

		Vec3 vNewPos;
		Vec3 vNewNorm;
		SMeshTexCoord NewTexCoord;
		SMeshTangents NewTangents;

		int i0, i1;

		if(fDist01>=fDist02 && fDist01>=fDist12 && fDist01>fMaxEdgeLen)
		{ i0=0; i1=1; }
		else if(fDist02>=fDist01 && fDist02>=fDist12 && fDist02>fMaxEdgeLen)
		{ i0=0; i1=2; }
		else if(fDist12>=fDist02 && fDist12>=fDist01 && fDist12>fMaxEdgeLen)
		{ i0=1; i1=2; }
		else
			continue;

		InterpolateVerts(vNewPos, vNewNorm,NewTexCoord, NewTangents, pFace->v[i0], pFace->v[i1], 
			lstVerts, lstNorms, lstTexCoords, lstTangents);

		int nNewIdx = lstVerts.Count();
		lstVerts.Add(vNewPos);
		lstNorms.Add(vNewNorm);
		lstTexCoords.Add(NewTexCoord);
		lstTangents.Add(NewTangents);

		SMeshFace newFace = *pFace;

		pFace->v[i1] = nNewIdx;
		pFace->t[i1] = nNewIdx;

		newFace.v[i0] = nNewIdx;
		newFace.t[i0] = nNewIdx;

		lstFaces.Add(newFace);

		i--;

		if(lstVerts.Count()>60000)
			break;
	}

	// rebuild mesh

	mesh.m_numFaces = lstFaces.Count();

	mesh.SetVertexCount( lstVerts.Count() );
	mesh.SetTexCoordsAndTangentsCount( lstVerts.Count() );
	mesh.ReallocStream(CMesh::INDICES,0);
	mesh.ReallocStream(CMesh::FACES,lstFaces.Count());

	memcpy(mesh.m_pPositions, lstVerts.GetElements(), sizeof(Vec3)*lstVerts.Count());
	memcpy(mesh.m_pTexCoord, lstTexCoords.GetElements(), sizeof(SMeshTexCoord)*lstTexCoords.Count()); 
	memcpy(mesh.m_pNorms, lstNorms.GetElements(), sizeof(Vec3)*lstNorms.Count());
	memcpy(mesh.m_pTangents, lstTangents.GetElements(), sizeof(SMeshTangents)*lstTangents.Count()); 

	memcpy(mesh.m_pFaces, lstFaces.GetElements(), sizeof(SMeshFace)*lstFaces.Count());

	Compile( mesh, MESH_COMPILE_TANGENTS );
}

END_MC_NAMESPACE
