////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CGFLoader.h
//  Version:     v1.00
//  Created:     6/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CGFLoader_h__
#define __CGFLoader_h__
#pragma once

#include "../MeshCompiler/MeshCompiler.h"
#include "ChunkFile.h"
#include "CGFContent.h"

struct MSplit
{
	uint32 numSplits;
	std::vector<uint16> arrRemapIDs;
};


struct RBatch
{
	uint32 numBonesPerBatch;
	std::vector<uint16> m_arrBoneIDs;
	std::vector<uint16> m_arrRemapIDs;
	std::vector<uint8> m_arrBoneFlags;
	uint32 startFace;
	uint32 numFaces;
	uint32 MaterialID;
};


struct ExtSkinVertexTemp
{
	Vec3 wpos0;			//vertex-position of model.1 
	Vec3 wpos1;			//vertex-position of model.2 
	Vec3 wpos2;			//vertex-position of model.3 
	f32	 u,v;		
	Vec4sf binormal;  //stored as four uint16 
	Vec4sf tangent;   //stored as four uint16 
	uint16 bone[4];
	f32 weight[4];
	ColorB color;   //index for blend-array
};

//////////////////////////////////////////////////////////////////////////
class ILoaderCGFListener
{
public:
	virtual void Warning( const char *format ) = 0;
	virtual void Error( const char *format ) = 0;
};

class CLoaderCGF
{
public:
	CLoaderCGF();
	~CLoaderCGF();

	CContentCGF* LoadCGF( const char *filename,IChunkFile &chunkFile, ILoaderCGFListener* pListener );
  CContentCGF* LoadCGF_FromMemBlock( const void * pData, int nDataSize, class CReadOnlyChunkFile * pChunkFile );

	const char *GetLastError() { return m_LastError; }
	CContentCGF* GetCContentCGF() { return m_pCompiledCGF; } 

private:
	bool LoadChunks();
	bool LoadExportFlagsChunk( int nChunkIndex );
	bool LoadNodeChunk( int nChunkIndex );

	bool LoadHelperChunk( CNodeCGF *pNode,IChunkFile::ChunkDesc *pChunkDesc );
	bool LoadLightChunk( CNodeCGF *pNode,IChunkFile::ChunkDesc *pChunkDesc );
	bool LoadGeomChunk( CNodeCGF *pNode,IChunkFile::ChunkDesc *pChunkDesc );
	bool LoadCompiledMeshChunk( CNodeCGF *pNode,IChunkFile::ChunkDesc *pChunkDesc );
	bool LoadMeshSubsetsChunk( CMesh &mesh,IChunkFile::ChunkDesc *pChunkDesc );
	bool LoadStreamDataChunk( int nChunkId,void* &pStreamData,int &nStreamType,int &nCount,int &nElemSize );
	template<class T>
		bool LoadStreamChunk( CMesh& mesh, MESH_CHUNK_DESC_0800 &chunk, ECgfStreamType Type, CMesh::EStream MStream, T*& Data, int nCount );

	bool LoadPhysicsDataChunk( CNodeCGF *pNode,int nPhysGeomType,int nChunkId );

	bool LoadFaceMapChunk( int nChunkIndex );

	CMaterialCGF* LoadMaterialFromChunk( int nChunkId );

	CMaterialCGF* LoadOldMaterialChunk( IChunkFile::ChunkDesc *pChunkDesc );
	CMaterialCGF* LoadMaterialNameChunk( IChunkFile::ChunkDesc *pChunkDesc );

	void ProcessNodes();
	void ProcessMaterials();

	//////////////////////////////////////////////////////////////////////////
	// loading of skinned meshes
	//////////////////////////////////////////////////////////////////////////
	uint32 ProcessSkinning(); 
	CContentCGF* MakeCompiledCGF( CContentCGF *pCGF, std::vector<uint16> *p, std::vector<uint16> *q);
	void Swap(uint32 idx, uint32 i0, uint32 i1); 
	bool IsSmaller(uint32 idx, uint32 i0, uint32 i1) const;
	uint32 PushIntoNewBuffer( std::vector<ExtSkinVertexTemp>& arrExtSkinTemp, std::vector<TFace>& arrExtFaces, uint16* pIndices, uint32 numIndices, std::vector<uint32>& arrUsedBonesLast); 

	//old chunks
	bool ReadBoneNameList( IChunkFile::ChunkDesc *pChunkDesc );
	bool ReadMorphTargets( IChunkFile::ChunkDesc *pChunkDesc );
	bool ReadBoneInitialPos (  IChunkFile::ChunkDesc *pChunkDesc  );
	uint32 ReadBoneHierarchy(  IChunkFile::ChunkDesc* pChunkDesc );
	uint32 RecursiveBoneLoader (int nBoneParentIndex, int nBoneIndex);
	uint32 ReadBoneMesh ( IChunkFile::ChunkDesc* pChunkDesc );



	//new chunks
	bool ReadCompiledBones ( IChunkFile::ChunkDesc *pChunkDesc );
	bool ReadCompiledPhysicalBones ( IChunkFile::ChunkDesc *pChunkDesc );
	bool ReadCompiledPhysicalProxies (  IChunkFile::ChunkDesc *pChunkDesc  );
	bool ReadCompiledMorphTargets (  IChunkFile::ChunkDesc *pChunkDesc  );

	bool ReadCompiledIntFaces (  IChunkFile::ChunkDesc *pChunkDesc  );
	bool ReadCompiledIntSkinVertice ( IChunkFile::ChunkDesc *pChunkDesc );
	bool ReadCompiledExt2IntMap (  IChunkFile::ChunkDesc *pChunkDesc  );
	bool ReadCompiledBonesBoxes (  IChunkFile::ChunkDesc *pChunkDesc  );

	bool ReadCompiledBreakablePhysics (  IChunkFile::ChunkDesc *pChunkDesc  );



	void Warning(const char* szFormat, ...) PRINTF_PARAMS(2, 3);

	uint32 m_IsCHR;
	uint32 m_CompiledBones; 
	uint32 m_CompiledBonesBoxes; 
	uint32 m_CompiledMesh;

	uint32 m_numBonenameList;
	uint32 m_numBoneInitialPos;
	uint32 m_numMorphTargets;
	uint32 m_numBoneHierarchy;

	std::vector<uint32> m_arrIndexToId;					//the mapping BoneIndex -> BoneID
	std::vector<uint32> m_arrIdToIndex;					//the mapping BoneID -> BineIndex 	
	std::vector<string> m_arrBoneNameTable;	//names of bones
	std::vector<Matrix34> m_arrInitPose34;
	std::vector<CryVertexBinding> m_arrLinksTmp;
	std::vector<RBatch> m_arrBatches;

	CContentCGF* m_pCompiledCGF; 
	const void* m_pBoneAnimRawData, *m_pBoneAnimRawDataEnd;
	const char* m_szLastError;
	uint32 m_numBones;
	int m_nNextBone;

	//////////////////////////////////////////////////////////////////////////

private:
	string m_LastError;

	string m_filename;

	IChunkFile *m_pChunkFile;
	CContentCGF *m_pCGF;

	// To find really used materials
	std::vector<int> usedMaterialIds;
	uint16 MatIdToSubset[MAX_SUB_MATERIALS];
	int nLastChunkId;

	ILoaderCGFListener* m_pListener;

	bool m_bHaveVertexColors;
	bool m_bOldMaterials;
};

#endif //__CGFLoader_h__
