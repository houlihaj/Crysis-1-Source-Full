////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CGFSaver.cpp
//  Version:     v1.00
//  Created:     7/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CGFSaver.h"

using namespace mesh_compiler;

#define SCALE_TO_CGF 100.0f

struct CChunkData
{
	char *data;
	int size;

	//////////////////////////////////////////////////////////////////////////
	CChunkData() { data = 0; size = 0; }
	~CChunkData() { free(data); }

	template <class T>
		void Add( const T& object )
	{
		AddData( &object,sizeof(object) );
	}
	void AddData( const void *pSrcData,int nSrcDataSize )
	{
		data = (char*)realloc(data,size+nSrcDataSize);
		memcpy( data+size,pSrcData,nSrcDataSize );
		size += nSrcDataSize;
	}
};

//////////////////////////////////////////////////////////////////////////
CSaverCGF::CSaverCGF( const char *filename,CChunkFile &chunkFile )
{
	assert(m_pChunkFile);
	m_filename = filename;
	m_pChunkFile = &chunkFile;
}

//////////////////////////////////////////////////////////////////////////
void CSaverCGF::SaveContent( CContentCGF *pCGF )
{
	SetContent( pCGF );

	SaveExportFlags();
	SaveMaterials();
	SaveNodes();
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveSpeedInfo( void *pData,int nSize )
{
	CHUNK_HEADER chunk;

	ZeroStruct(chunk);
	chunk.ChunkType = ChunkType_SpeedInfo;
	chunk.ChunkVersion = SPEED_CHUNK_DESC::VERSION;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pData,nSize );

	return m_pChunkFile->AddChunk( chunk,chunkData.data,chunkData.size );
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveSpeedInfo1( void *pData,int nSize )
{
	CHUNK_HEADER chunk;

	ZeroStruct(chunk);
	chunk.ChunkType = ChunkType_SpeedInfo;
	chunk.ChunkVersion = SPEED_CHUNK_DESC_1::VERSION;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pData,nSize );

	return m_pChunkFile->AddChunk( chunk,chunkData.data,chunkData.size );
}



//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveFootPlantInfo(void *pData,int nSize )
{
	CHUNK_HEADER chunk;

	ZeroStruct(chunk);
	chunk.ChunkType = ChunkType_FootPlantInfo;
	chunk.ChunkVersion = FOOTPLANT_INFO::VERSION;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pData,nSize );

	return m_pChunkFile->AddChunk( chunk,chunkData.data,chunkData.size );
}



//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveCompiledBones( void *pData,int nSize )
{
	COMPILED_BONE_CHUNK_DESC_0800 chunk;

	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_CompiledBones;
	chunk.chdr.ChunkVersion = COMPILED_BONE_CHUNK_DESC_0800::VERSION;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pData,nSize );

	return m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveCompiledPhysicalBones( void *pData,int nSize )
{
	COMPILED_PHYSICALBONE_CHUNK_DESC_0800 chunk;

	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_CompiledPhysicalBones;
	chunk.chdr.ChunkVersion = COMPILED_PHYSICALBONE_CHUNK_DESC_0800::VERSION;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pData,nSize );

	return m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveCompiledPhysicalProxis( void *pData,int nSize, uint32 numPhysicalProxies )
{
	COMPILED_PHYSICALPROXY_CHUNK_DESC_0800 chunk;

	ZeroStruct(chunk);
	chunk.chdr.ChunkType		= ChunkType_CompiledPhysicalProxies;
	chunk.chdr.ChunkVersion = COMPILED_PHYSICALPROXY_CHUNK_DESC_0800::VERSION;
	chunk.numPhysicalProxies	=	numPhysicalProxies;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pData,nSize );

	return m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveCompiledMorphTargets( void *pData,int nSize, uint32 numMorphTargets )
{
	COMPILED_MORPHTARGETS_CHUNK_DESC_0800 chunk;

	ZeroStruct(chunk);
	chunk.chdr.ChunkType		= ChunkType_CompiledMorphTargets;
	chunk.chdr.ChunkVersion = COMPILED_MORPHTARGETS_CHUNK_DESC_0800::VERSION;
	chunk.numMorphTargets		=	numMorphTargets;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pData,nSize );

	return m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
}



//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveCompiledIntSkinVertices( void *pData,int nSize )
{
	COMPILED_INTSKINVERTICES_CHUNK_DESC_0800 chunk;

	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_CompiledIntSkinVertices;
	chunk.chdr.ChunkVersion = COMPILED_INTSKINVERTICES_CHUNK_DESC_0800::VERSION;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pData,nSize );

	return m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveCompiledIntFaces( void *pData,int nSize )
{
	COMPILED_INTFACES_CHUNK_DESC_0800 chunk;

	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_CompiledIntFaces;
	chunk.chdr.ChunkVersion = COMPILED_INTFACES_CHUNK_DESC_0800::VERSION;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pData,nSize );

	return m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
}


//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveCompiledExt2IntMap( void *pData,int nSize )
{
	COMPILED_EXT2INTMAP_CHUNK_DESC_0800 chunk;

	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_CompiledExt2IntMap;
	chunk.chdr.ChunkVersion = COMPILED_EXT2INTMAP_CHUNK_DESC_0800::VERSION;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pData,nSize );

	return m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
}

//////////////////////////////////////////////////////////////////////////
void CSaverCGF::SaveNodes()
{
	m_savedNodes.clear();
	uint32 numNodes = m_pCGF->GetNodeCount();
	for (int i=0; i<numNodes; i++)
	{
		CNodeCGF *pNode = m_pCGF->GetNode(i);
		const char* pNodeName = pNode->name;
		// Check if not yet saved.
		uint32 numSavedNodes = m_savedNodes.size();
		//	if (m_savedNodes.find(pNode) == m_savedNodes.end())
		SaveNode( pNode );

		if (pNode->bHasFaceMap)
		{
			FACEMAP_CHUNK_DESC chunkMap;
			ZeroStruct(chunkMap);
			chunkMap.chdr.ChunkType = ChunkType_FaceMap;
			chunkMap.chdr.ChunkVersion = FACEMAP_CHUNK_DESC::VERSION;
			strcpy(chunkMap.nodeName,pNodeName);
			chunkMap.StreamID = SaveStreamDataChunk(&pNode->mapFaceToFace0[0], CGF_STREAM_FACEMAP, 
				pNode->pMesh->GetIndexCount()/3, sizeof(pNode->mapFaceToFace0[0]));
			m_pChunkFile->AddChunk( chunkMap.chdr,&chunkMap,sizeof(chunkMap) );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveNode( CNodeCGF *pNode )
{

	const char* pNodeName = pNode->name;

	typedef std::set<CNodeCGF*> MySet;

	for( MySet::iterator it( m_savedNodes.begin() ); it != m_savedNodes.end(); ++it )
	{
		if( pNode == *it )
		{
			return pNode->nChunkId;
		}
	}

	m_savedNodes.insert(pNode);

	NODE_CHUNK_DESC chunk;
	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_Node;
	chunk.chdr.ChunkVersion = NODE_CHUNK_DESC::VERSION;

	for (int i = 0; i < m_pCGF->GetNodeCount(); i++)
	{
		CNodeCGF *pOtherNode = m_pCGF->GetNode(i);
		if (pOtherNode->pParent == pNode)
			chunk.nChildren++;
	}

	strcpy( chunk.name,pNode->name.c_str() );
	chunk.pos = pNode->pos * SCALE_TO_CGF;
	chunk.rot = pNode->rot;
	chunk.scl = pNode->scl;

	chunk.pos_cont_id = pNode->pos_cont_id;
	chunk.rot_cont_id = pNode->rot_cont_id;
	chunk.scl_cont_id = pNode->scl_cont_id;

	// Set matrix to node chunk.
	float *pMat = (float*)&chunk.tm;
	pMat[0] = pNode->localTM(0,0);
	pMat[1] = pNode->localTM(1,0);
	pMat[2] = pNode->localTM(2,0);
	pMat[4] = pNode->localTM(0,1);
	pMat[5] = pNode->localTM(1,1);
	pMat[6] = pNode->localTM(2,1);
	pMat[8] = pNode->localTM(0,2);
	pMat[9] = pNode->localTM(1,2);
	pMat[10]= pNode->localTM(2,2);
	pMat[12] = pNode->localTM.GetTranslation().x * SCALE_TO_CGF;
	pMat[13] = pNode->localTM.GetTranslation().y * SCALE_TO_CGF;
	pMat[14] = pNode->localTM.GetTranslation().z * SCALE_TO_CGF;

	if (pNode->pMaterial)
	{
		chunk.MatID = pNode->pMaterial->nChunkId;
	}

	chunk.ObjectID = -1;
	chunk.ParentID = -1;
	if (pNode->pParent)
	{
		pNode->nParentChunkId = SaveNode( pNode->pParent );
		chunk.ParentID = pNode->nParentChunkId;
	}
	if (pNode->type == CNodeCGF::NODE_MESH && pNode->pMesh)
	{
		pNode->nObjectChunkId = SaveNodeMesh( pNode );
	}
	if (pNode->type == CNodeCGF::NODE_HELPER && pNode->helperType == HP_GEOMETRY && pNode->pMesh)
	{
		pNode->nObjectChunkId = SaveNodeMesh( pNode );
	}
	else if (pNode->type == CNodeCGF::NODE_HELPER)
	{
		pNode->nObjectChunkId = SaveHelperChunk( pNode );
	}
	else if (pNode->type == CNodeCGF::NODE_LIGHT)
	{
		// Save Light chunk
	}

	chunk.ObjectID = pNode->nObjectChunkId;
	chunk.PropStrLen = pNode->properties.length();

	CChunkData chunkData;
	chunkData.Add(chunk);
	// Copy property string
	chunkData.AddData( pNode->properties.c_str(),chunk.PropStrLen );

	pNode->nChunkId = m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
	return pNode->nChunkId;
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveNodeMesh( CNodeCGF *pNode )
{
	if (m_mapMeshToChunk.find(pNode->pMesh) != m_mapMeshToChunk.end())
	{
		// This mesh already saved.
		return m_mapMeshToChunk.find(pNode->pMesh)->second;
	}
	MESH_CHUNK_DESC_0800 chunk;
	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_Mesh;
	chunk.chdr.ChunkVersion = MESH_CHUNK_DESC_0800::VERSION;

	CMesh &mesh = *pNode->pMesh;

	int nVerts = mesh.GetVertexCount();
	int nIndices = mesh.GetIndexCount();

	chunk.nVerts = nVerts;
	chunk.nIndices = nIndices;
	chunk.nSubsets = mesh.GetSubSetCount();
	chunk.bboxMin = mesh.m_bbox.min;
	chunk.bboxMax = mesh.m_bbox.max;

	chunk.nSubsetsChunkId = SaveMeshSubsetsChunk( mesh );

	if (mesh.m_pPositions)
		chunk.nStreamChunkID[CGF_STREAM_POSITIONS] = SaveStreamDataChunk( mesh.m_pPositions,CGF_STREAM_POSITIONS,mesh.GetVertexCount(),sizeof(Vec3) );
	if (mesh.m_pNorms)
		chunk.nStreamChunkID[CGF_STREAM_NORMALS] = SaveStreamDataChunk( mesh.m_pNorms,CGF_STREAM_NORMALS,mesh.GetVertexCount(),sizeof(Vec3) );
	if (mesh.m_pTexCoord)
		chunk.nStreamChunkID[CGF_STREAM_TEXCOORDS] = SaveStreamDataChunk( mesh.m_pTexCoord,CGF_STREAM_TEXCOORDS,mesh.GetVertexCount(),sizeof(CryUV) );
	if (mesh.m_pColor0)
	{
		chunk.nStreamChunkID[CGF_STREAM_COLORS] = SaveStreamDataChunk( mesh.m_pColor0,CGF_STREAM_COLORS,mesh.GetVertexCount(),sizeof(SMeshColor) );
	}
	if (mesh.m_pColor1)
	{
		chunk.nStreamChunkID[CGF_STREAM_COLORS2] = SaveStreamDataChunk( mesh.m_pColor1,CGF_STREAM_COLORS2,mesh.GetVertexCount(),sizeof(SMeshColor) );
	}
  if (mesh.m_pVertMats)
  {
    chunk.nStreamChunkID[CGF_STREAM_VERT_MATS] = SaveStreamDataChunk( mesh.m_pVertMats,CGF_STREAM_VERT_MATS,mesh.GetVertexCount(),sizeof(int) );
  }
	if (mesh.m_pIndices)
		chunk.nStreamChunkID[CGF_STREAM_INDICES] = SaveStreamDataChunk( mesh.m_pIndices,CGF_STREAM_INDICES,mesh.GetIndexCount(),sizeof(uint16) );
	if (mesh.m_pTangents)
		chunk.nStreamChunkID[CGF_STREAM_TANGENTS] = SaveStreamDataChunk( mesh.m_pTangents,CGF_STREAM_TANGENTS,mesh.GetVertexCount(),sizeof(SMeshTangents) );

	if (mesh.m_pShapeDeformation)
		chunk.nStreamChunkID[CGF_STREAM_SHAPEDEFORMATION] = SaveStreamDataChunk( mesh.m_pShapeDeformation,CGF_STREAM_SHAPEDEFORMATION,mesh.GetVertexCount(),sizeof(SMeshShapeDeformation) );
	if (mesh.m_pBoneMapping)
		chunk.nStreamChunkID[CGF_STREAM_BONEMAPPING]			= SaveStreamDataChunk( mesh.m_pBoneMapping,CGF_STREAM_BONEMAPPING,mesh.GetVertexCount(),sizeof(SMeshBoneMapping) );

	if (mesh.m_pSHInfo && mesh.m_pSHInfo->pSHCoeffs)
		chunk.nStreamChunkID[CGF_STREAM_SHCOEFFS] = SaveStreamDataChunk( mesh.m_pSHInfo->pSHCoeffs,CGF_STREAM_SHCOEFFS,mesh.GetVertexCount(),sizeof(SMeshSHCoeffs) );

	for (int i = 0; i < 4; i++)
	{
		if (!pNode->physicalGeomData[i].empty())
			chunk.nPhysicsDataChunkId[i] = SavePhysicalDataChunk( &pNode->physicalGeomData[i][0],pNode->physicalGeomData[i].size() );
	}

	int nMeshChunkId = m_pChunkFile->AddChunk( chunk.chdr,&chunk,sizeof(chunk) );
	m_mapMeshToChunk[pNode->pMesh] = nMeshChunkId;
	return nMeshChunkId;
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveHelperChunk( CNodeCGF *pNode )
{
	HELPER_CHUNK_DESC chunk;
	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_Helper;
	chunk.chdr.ChunkVersion = HELPER_CHUNK_DESC::VERSION;

	chunk.type = pNode->helperType;
	chunk.size = pNode->helperSize;

	return m_pChunkFile->AddChunk( chunk.chdr,&chunk,sizeof(chunk) );
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveBreakablePhysics( )
{
	CPhysicalizeInfoCGF * pPi = m_pCGF->GetPhysiclizeInfo();

	if(!pPi || pPi->nGranularity==-1)
		return 0;

	BREAKABLE_PHYSICS_CHUNK_DESC chunk;
	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_BreakablePhysics;
	chunk.chdr.ChunkVersion = BREAKABLE_PHYSICS_CHUNK_DESC::VERSION;

	chunk.granularity = pPi->nGranularity;
	chunk.nMode = pPi->nMode;
	chunk.nRetVtx = pPi->nRetVtx;
	chunk.nRetTets = pPi->nRetTets;

	CChunkData chunkData;
	chunkData.Add(chunk);
	if (pPi->pRetVtx)
		chunkData.AddData( pPi->pRetVtx , pPi->nRetVtx * sizeof(Vec3) );
	if (pPi->pRetTets)
		chunkData.AddData( pPi->pRetTets , pPi->nRetTets * sizeof(int)*4 );

	return m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
}


//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveMeshSubsetsChunk( CMesh &mesh )
{
	MESH_SUBSETS_CHUNK_DESC_0800 chunk;
	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_MeshSubsets;
	chunk.chdr.ChunkVersion = MESH_SUBSETS_CHUNK_DESC_0800::VERSION;

	chunk.nCount = mesh.GetSubSetCount();
	uint32 cbHasDecompressionMatrix = (mesh.m_pSHInfo && mesh.m_pSHInfo->nDecompressionCount == mesh.GetSubSetCount());	//if we have a decompression matrix available per subset, store it;
	chunk.nFlags |= (cbHasDecompressionMatrix ? MESH_SUBSETS_CHUNK_DESC_0800::SH_HAS_DECOMPR_MAT : 0);//flag it


	uint32 cbHasBoneIDs=0;
	for (int i = 0; i < mesh.GetSubSetCount(); i++)
	{
		SMeshSubset &srcSubset = mesh.m_subsets[i];
		cbHasBoneIDs|=srcSubset.m_arrGlobalBonesPerSubset.size();
	}
	if (cbHasBoneIDs)
		chunk.nFlags |=  MESH_SUBSETS_CHUNK_DESC_0800::BONEINDICES;


	CChunkData chunkData;
	chunkData.Add(chunk);


	for (int i = 0; i < mesh.GetSubSetCount(); i++)
	{
		SMeshSubset &srcSubset = mesh.m_subsets[i];

		MESH_SUBSETS_CHUNK_DESC_0800::MeshSubset subset;
		subset.nFirstIndexId = srcSubset.nFirstIndexId;
		subset.nNumIndices = srcSubset.nNumIndices;
		subset.nFirstVertId = srcSubset.nFirstVertId;
		subset.nNumVerts = srcSubset.nNumVerts;
		subset.nMatID = srcSubset.nMatID;
		subset.fRadius = srcSubset.fRadius;
		subset.vCenter = srcSubset.vCenter;
		cbHasBoneIDs|=srcSubset.m_arrGlobalBonesPerSubset.size();
		chunkData.Add(subset);
	}

	//add decompression matrices
	if(cbHasDecompressionMatrix)
	{
		for (int i = 0; i < mesh.GetSubSetCount(); i++)
			chunkData.AddData(&(mesh.m_pSHInfo->pDecompressions[i]), sizeof(SSHDecompressionMat));
	}

	//add boneindices
	if(cbHasBoneIDs)
	{
		for (int i = 0; i < mesh.GetSubSetCount(); i++)
		{
			SMeshSubset &srcSubset = mesh.m_subsets[i];
			MESH_SUBSETS_CHUNK_DESC_0800::MeshBoneIDs subset;
			subset.numBoneIDs = srcSubset.m_arrGlobalBonesPerSubset.size();
			for (uint32 b=0; b<subset.numBoneIDs; b++)
				subset.arrBoneIDs[b]=srcSubset.m_arrGlobalBonesPerSubset[b];
			chunkData.AddData(&subset, sizeof(MESH_SUBSETS_CHUNK_DESC_0800::MeshBoneIDs));
		}
	}



	return m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveStreamDataChunk( void *pStreamData,int nStreamType,int nCount,int nElemSize )
{
	STREAM_DATA_CHUNK_DESC_0800 chunk;
	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_DataStream;
	chunk.chdr.ChunkVersion = STREAM_DATA_CHUNK_DESC_0800::VERSION;

	chunk.nStreamType = nStreamType;
	chunk.nCount = nCount;
	chunk.nElementSize = nElemSize;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pStreamData,nCount*nElemSize );

	return m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SavePhysicalDataChunk( void *pData,int nSize )
{
	MESH_PHYSICS_DATA_CHUNK_DESC_0800 chunk;

	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_MeshPhysicsData;
	chunk.chdr.ChunkVersion = MESH_PHYSICS_DATA_CHUNK_DESC_0800::VERSION;

	chunk.nDataSize = nSize;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData( pData,nSize );

	return m_pChunkFile->AddChunk( chunk.chdr,chunkData.data,chunkData.size );
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveExportFlags()
{
	EXPORT_FLAGS_CHUNK_DESC chunk;

	ZeroStruct(chunk);
	chunk.chdr.ChunkType = ChunkType_ExportFlags;
	chunk.chdr.ChunkVersion = EXPORT_FLAGS_CHUNK_DESC::VERSION;

	CExportInfoCGF *pExpInfo = m_pCGF->GetExportInfo();
	if (pExpInfo->bMergeAllNodes)
		chunk.flags |= EXPORT_FLAGS_CHUNK_DESC::MERGE_ALL_NODES;

	enum {NUM_RC_VERSION_ELEMENTS = sizeof(pExpInfo->rc_version) / sizeof(pExpInfo->rc_version[0])};
	std::copy(pExpInfo->rc_version, pExpInfo->rc_version + NUM_RC_VERSION_ELEMENTS, chunk.rc_version);
	strncpy( chunk.rc_version_string,pExpInfo->rc_version_string,sizeof(chunk.rc_version_string) );

	return m_pChunkFile->AddChunk( chunk.chdr,&chunk,sizeof(chunk) );
}

//////////////////////////////////////////////////////////////////////////
void CSaverCGF::SaveMaterials()
{
	int i;
	for (i = 0; i < m_pCGF->GetNodeCount(); i++)
	{
		CMaterialCGF *pMaterialCGF = m_pCGF->GetNode(i)->pMaterial;
		if (pMaterialCGF)
		{
			SaveMaterial( pMaterialCGF );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CSaverCGF::SaveMaterial( CMaterialCGF *pMtl )
{
	// Check whether the material has already been saved.
	if (m_savedMaterials.find(pMtl) == m_savedMaterials.end())
	{
		m_savedMaterials.insert(pMtl);

		MTL_NAME_CHUNK_DESC_0800 chunk;
		ZeroStruct(chunk);
		chunk.chdr.ChunkType = ChunkType_MtlName;
		chunk.chdr.ChunkVersion = MTL_NAME_CHUNK_DESC_0800::VERSION;

		strncpy( chunk.name,pMtl->name.c_str(),sizeof(chunk.name) );
		chunk.name[sizeof(chunk.name)-1] = 0;

		chunk.nFlags = pMtl->nFlags;//(pMtl->subMaterials.empty() ? MTL_NAME_CHUNK_DESC_0800::FLAG_SUB_MATERIAL : MTL_NAME_CHUNK_DESC_0800::FLAG_MULTI_MATERIAL);
		chunk.sh_opacity = pMtl->shOpacity;
		chunk.nPhysicalizeType = pMtl->nPhysicalizeType;

		// Add the material now - it is important that the parent comes before the children,
		// since the loading code considers the first material encountered to be the name of
		// the .mtl file. However we have not finished setting up the chunk yet, so we will
		// have to update the data later.
		pMtl->nChunkId = m_pChunkFile->AddChunk( chunk.chdr,&chunk,sizeof(chunk) );

		// Recurse to the child materials.
		chunk.nSubMaterials = int(pMtl->subMaterials.size());
		if (chunk.nSubMaterials > MAX_SUB_MATERIALS)
			chunk.nSubMaterials = MAX_SUB_MATERIALS;
		for (int childIndex = 0; childIndex < chunk.nSubMaterials; ++childIndex)
		{
			int chunkID = 0;
			CMaterialCGF* childMaterial = pMtl->subMaterials[childIndex];
			if (childMaterial)
				chunkID = this->SaveMaterial(childMaterial);
			chunk.nSubMatChunkId[childIndex] = chunkID;
		}

		// Update the data for the parent chunk.
		m_pChunkFile->SetChunkData(pMtl->nChunkId, &chunk, sizeof(chunk));
	}

	return pMtl->nChunkId;
}


int CSaverCGF::SaveController(int nKeys, int nControllerId, CryKeyPQLog* pData,int nSize )
{

	CONTROLLER_CHUNK_DESC_0828 pCtrlChunk;

	
	ZeroStruct(pCtrlChunk);
	pCtrlChunk.chdr.ChunkType = ChunkType_Controller;
	pCtrlChunk.chdr.ChunkVersion = CONTROLLER_CHUNK_DESC_0828::VERSION;

	pCtrlChunk.numKeys = nKeys;
	pCtrlChunk.nControllerId = nControllerId;

	
	CChunkData chunkData;
	chunkData.Add(pCtrlChunk);
	chunkData.AddData( pData,nSize * sizeof(CryKeyPQLog) );

	return m_pChunkFile->AddChunk( pCtrlChunk.chdr,chunkData.data,chunkData.size );



	return 1;
}

int CSaverCGF::SaveController829(CONTROLLER_CHUNK_DESC_0829& pCtrlChunk, void* pData,int nSize )
{

	CChunkData chunkData;
	chunkData.Add(pCtrlChunk);
	chunkData.AddData( pData,nSize);

	return m_pChunkFile->AddChunk( pCtrlChunk.chdr,chunkData.data,chunkData.size );

}

int CSaverCGF::SaveController830(CONTROLLER_CHUNK_DESC_0830& pCtrlChunk, void* pData,int nSize )
{

	CChunkData chunkData;
	chunkData.Add(pCtrlChunk);
	chunkData.AddData( pData,nSize);

	return m_pChunkFile->AddChunk( pCtrlChunk.chdr,chunkData.data,chunkData.size );

}

int CSaverCGF::SaveControllerDB(CONTROLLER_CHUNK_DESC_0900& pCtrlChunk,void* pData,int nSize )
{

	CChunkData chunkData;
	chunkData.Add(pCtrlChunk);
	chunkData.AddData( pData,nSize);

	return m_pChunkFile->AddChunk( pCtrlChunk.chdr,chunkData.data,chunkData.size );

}

//STUPID. I know. Good place to some refactoring
int CSaverCGF::SaveController901(CONTROLLER_CHUNK_DESC_0901& pCtrlChunk,void* pData,int nSize )
{

	CChunkData chunkData;
	chunkData.Add(pCtrlChunk);
	chunkData.AddData( pData,nSize);

	return m_pChunkFile->AddChunk( pCtrlChunk.chdr,chunkData.data,chunkData.size );

}
