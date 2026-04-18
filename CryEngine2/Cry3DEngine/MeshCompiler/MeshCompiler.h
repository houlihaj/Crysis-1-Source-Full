////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   MeshCompiler.h
//  Version:     v1.00
//  Created:     5/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MeshCompiler_h__
#define __MeshCompiler_h__
#pragma once

#include "IIndexedMesh.h"

// Max number of sub materials in mesh.
#define MAX_SUB_MATERIALS 32

namespace mesh_compiler
{
	/*
#define TANG_FLOATS 1
#ifdef TANG_FLOATS
	typedef float uint8f;
#else
	typedef uint8 uint8f;
#endif

	typedef Vec4_tpl<uint8f> Vec4bf;		//used for tangents only

	_inline uint8f tPackF2B(float f)
	{
#ifdef TANG_FLOATS
		return f;
#else
		return (uint8f)((f + 1.0f) / 2.0f * 255.0f);
#endif
	}
	_inline float tPackB2F(uint8f i)
	{
#ifdef TANG_FLOATS
		return i;
#else
		return (float)((float)i / 255.0f * 2.0f - 1.0f);
#endif
	}
	*/

	enum EMeshCompileFlags
	{
		MESH_COMPILE_OPTIMIZE = 0x0001,
		MESH_COMPILE_TANGENTS = 0x0002,
		MESH_COMPILE_VOXEL = 0x0004,
	};

	//////////////////////////////////////////////////////////////////////////
	class CMeshCompiler
	{
	public:
		CMeshCompiler();
		~CMeshCompiler();

		// for flags see EMeshCompilerFlags
		bool Compile( CMesh &mesh,int flags=(MESH_COMPILE_TANGENTS|MESH_COMPILE_OPTIMIZE) );

		void ReduceMeshToLevel( CMesh &mesh,float fLevel );

		void Tesselate( CMesh &mesh, float fMaxEdgeLen );

		// Remap indices for all compile operations.
		void SetVertexRemapping( std::vector<uint16> *pVertexMap ) { m_pVertexMap = pVertexMap; };
		void SetIndexRemapping( std::vector<uint16> *pIndexMap ) { m_pIndexMap = pIndexMap; };
		void SetFaceRemapping( DynArray<uint16> *pMapFaceToFace0 ) { m_pMapFaceToFace0 = pMapFaceToFace0; };

		void ShareVertices( CMesh &mesh );

		// Weld positions and indices.
		// All vertices that are closer to each other then fEpsilon
		// are welded together and the new position array is re-indexed.
		// Optionally return index remaping from new to old vertex indices in pIndicesRemap.
		void WeldPositions( Vec3 *pVertices, int &nVerts, std::vector<int> &indices,float fEpsilon, const AABB & boxBoundary );

    static inline bool IsEquivalentVec3dCheckYFirst(const Vec3 & v0, const Vec3 & v1, float fEpsilon)
    {
      if(fabsf(v0.y-v1.y)<fEpsilon)
        if(fabsf(v0.x-v1.x)<fEpsilon)
          if(fabsf(v0.z-v1.z)<fEpsilon)
            return true;
      return false;
    }

    static inline int FindInPosBuffer_VERTEX_FORMAT_P3F_COL4UB_TEX2F(const Vec3 & vPosToFind, const struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F * pVertBuff, std::vector<int> *pHash, float fEpsilon )
    { 
      for(uint32 i=0; i<pHash->size(); i++) 
        if(IsEquivalentVec3dCheckYFirst(pVertBuff[(*pHash)[i]].xyz, vPosToFind, fEpsilon)) 
          return (*pHash)[i];

      return -1;
    }

    template<class T> 
    void WeldPos_VERTEX_FORMAT_P3F_COL4UB_TEX2F( PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> & vertices, 
      PodArray<SPipTangents> & tangents, PodArray<T> &indices, float fEpsilon, const AABB & boxBoundary )
    {
      struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pTmpVerts = new struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F[vertices.Count()];
      SPipTangents *pTmpTangents = new SPipTangents[tangents.Count()];

      int nCurVertex = 0;
      PodArray<T> newIndices;
      std::vector<int> arrHashTable[256];
      newIndices.reserve( indices.size() );
      std::vector<int> *pHash = 0;

      float fHashElemSize = 256.0f / max(boxBoundary.max.x - boxBoundary.min.x, 0.01f);

      for(uint32 i=0; i < indices.size(); i++)
      {
        int v = indices[i];

        assert(v<vertices.Count());

        struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F & vPos = vertices[v];
        SPipTangents & vTang = tangents[v];

        bool bInRange(
          vPos.xyz.x>boxBoundary.min.x && vPos.xyz.y>boxBoundary.min.y && vPos.xyz.z>boxBoundary.min.z && 
          vPos.xyz.x<boxBoundary.max.x && vPos.xyz.y<boxBoundary.max.y && vPos.xyz.z<boxBoundary.max.z);

        int nHashValue = int((vPos.xyz.x - boxBoundary.min.x)*fHashElemSize);

        pHash = &arrHashTable[(unsigned char)(nHashValue)];
        int nFind = FindInPosBuffer_VERTEX_FORMAT_P3F_COL4UB_TEX2F( vPos.xyz, pTmpVerts, pHash, bInRange ? fEpsilon : 0.01f);
        if(nFind<0)
        {
          pHash->push_back(nCurVertex);
          
          // make sure neighbor hashes also have this vertex
          if(bInRange && fEpsilon>0.01f)
          { 
            pHash = &arrHashTable[(unsigned char)(nHashValue+1)];
            if(FindInPosBuffer_VERTEX_FORMAT_P3F_COL4UB_TEX2F( vPos.xyz, pTmpVerts, pHash, fEpsilon )<0)
              pHash->push_back(nCurVertex);

            pHash = &arrHashTable[(unsigned char)(nHashValue-1)];
            if(FindInPosBuffer_VERTEX_FORMAT_P3F_COL4UB_TEX2F( vPos.xyz, pTmpVerts, pHash, fEpsilon )<0)
              pHash->push_back(nCurVertex);
          }
        
          // add new vertex
          pTmpVerts[nCurVertex] = vPos;
          pTmpTangents[nCurVertex] = vTang;
          newIndices.push_back(nCurVertex);
          nCurVertex++;
        }
        else
        {
          newIndices.push_back(nFind);
        }
      }

      indices.Clear();
      indices.AddList(newIndices);

      vertices.Clear();
      vertices.AddList(pTmpVerts,nCurVertex);

      tangents.Clear();
      tangents.AddList(pTmpTangents,nCurVertex);

      delete [] pTmpVerts;
      delete [] pTmpTangents;
    }

		bool CompareMeshes( CMesh &mesh1,CMesh &mesh2 );
		const char* GetLastError() { return m_LastError; }

	private:
		void CompactBuffer( CMesh &mesh,uint8 *vertexMatId, int flags );
		bool CheckForDegenerateFaces( CMesh &mesh );
		void FindVertexRanges( CMesh &mesh );
		bool StripifyMesh( CMesh &mesh );
		void UpdateFaces( CMesh &mesh );
		void InterpolateVerts(Vec3 & vNewPos, Vec3 & vNewNorm, SMeshTexCoord & NewTexCoord, SMeshTangents & NewTangents, 
			int i0, int i1, 
			PodArray<Vec3> & pPositions, PodArray<Vec3> & pNorms, 
			PodArray<SMeshTexCoord> & pTexCoord, PodArray<SMeshTangents> & pTangents);

	private:
		struct SBasisFace {
			uint16 v[3];
		};
		std::vector<uint16> m_tempIndices;
		std::vector<SMeshFace*> m_vhash_table[MAX_SUB_MATERIALS];
		std::vector<SBasisFace> m_thash_table[MAX_SUB_MATERIALS];
		std::vector<uint16> m_index_hash_table[256];
		
		std::vector<uint16> *m_pVertexMap;
		std::vector<uint16> *m_pIndexMap;
		DynArray<uint16> *m_pMapFaceToFace0;

		string m_LastError;
	};
};

#endif // __MeshCompiler_h__
