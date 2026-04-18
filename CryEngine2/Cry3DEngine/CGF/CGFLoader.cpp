////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CGFLoader.cpp
//  Version:     v1.00
//  Created:     6/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CGFLoader.h"
#include <StringUtils.h>
#include "CryPath.h"
#include <CryTypeInfo.h>
#include "CGFNodeMerger.h"
#include "ReadOnlyChunkFile.h"

using namespace mesh_compiler;

#define VERTEX_SCALE (0.01f)
#define PHYSICS_PROXY_NODE "PhysicsProxy"
#define PHYSICS_PROXY_NODE2 "$collision"
#define PHYSICS_PROXY_NODE3 "$physics_proxy"
#define PHYSICS_TETRIDERS_PROXY_NODE "$tet"


//////////////////////////////////////////////////////////////////////////
CLoaderCGF::CLoaderCGF()
{
	m_pChunkFile = NULL;

	m_bHaveVertexColors = false;

	//////////////////////////////////////////////////////////////////////////
	// To find really used materials
	usedMaterialIds.clear();
	memset(MatIdToSubset,0,sizeof(MatIdToSubset));
	nLastChunkId = 0;
	//////////////////////////////////////////////////////////////////////////

	m_bOldMaterials = false;

	m_pCompiledCGF=0;
}

//////////////////////////////////////////////////////////////////////////
CLoaderCGF::~CLoaderCGF()
{
}

//////////////////////////////////////////////////////////////////////////
CContentCGF* CLoaderCGF::LoadCGF( const char *filename,IChunkFile &chunkFile,ILoaderCGFListener* pListener )
{
	m_pListener = pListener;

	string strPath = filename;
	CryStringUtils::UnifyFilePath(strPath);
	string fileExt = PathUtil::GetExt(strPath);
	m_IsCHR = (0 == stricmp(fileExt,"chr"));

	m_filename = filename;
	m_pChunkFile = &chunkFile;

	if (!chunkFile.Read( filename ))
	{
		m_LastError = chunkFile.GetLastError();
		return 0;
	}

	// Load mesh from chunk file.
	if (chunkFile.GetFileHeader().Version != GeomFileVersion) 
	{
		m_LastError.Format("Bad CGF file version: %s", m_filename.c_str() );
		return 0;
	}

	if (chunkFile.GetFileHeader().FileType != FileType_Geom) 
	{
		if (chunkFile.GetFileHeader().FileType != FileType_Anim) 
		{
			m_LastError.Format("Illegal File Type for .cgf file: %s", m_filename.c_str() );
			return 0;
		}
	}

	m_pCGF = new CContentCGF( m_filename.c_str() );

	if (!LoadChunks())
	{
		delete m_pCGF;
		return 0;
	}

	ProcessMaterials();
	ProcessNodes();
	if (!ProcessSkinning())
	{
		delete m_pCGF;
		return 0;
	}

	if (m_pCGF->GetExportInfo()->bMergeAllNodes && !m_pCGF->GetExportInfo()->bCompiledCGF)
	{
		string errorMessage;
		if (!CCGFNodeMerger().MergeNodes(m_pCGF, usedMaterialIds, errorMessage))
		{
			m_LastError.Format("Error merging nodes: %s", errorMessage.c_str());
			delete m_pCGF;
			return 0;
		}
	}

	m_pListener = 0;

	return m_pCGF;
}

//////////////////////////////////////////////////////////////////////////
CContentCGF* CLoaderCGF::LoadCGF_FromMemBlock( const void * pData, int nDataSize, class CReadOnlyChunkFile * pChunkFile )
{
  m_pListener = NULL;

  m_IsCHR = false;

  m_filename = "";
  m_pChunkFile = (IChunkFile*)pChunkFile;

  if (!pChunkFile->ReadFromMemBlock( pData, nDataSize ))
  {
    m_LastError = m_pChunkFile->GetLastError();
    return 0;
  }

  // Load mesh from chunk file.
  if (m_pChunkFile->GetFileHeader().Version != GeomFileVersion) 
  {
    m_LastError.Format("Bad CGF file version: %s", m_filename.c_str() );
    return 0;
  }

  if (m_pChunkFile->GetFileHeader().FileType != FileType_Geom) 
  {
    if (m_pChunkFile->GetFileHeader().FileType != FileType_Anim) 
    {
      m_LastError.Format("Illegal File Type for .cgf file: %s", m_filename.c_str() );
      return 0;
    }
  }

  m_pCGF = new CContentCGF( m_filename.c_str() );

  if (!LoadChunks())
  {
    delete m_pCGF;
    return 0;
  }

  ProcessMaterials();
  ProcessNodes();
  if (!ProcessSkinning())
  {
    delete m_pCGF;
    return 0;
  }

  if (m_pCGF->GetExportInfo()->bMergeAllNodes && !m_pCGF->GetExportInfo()->bCompiledCGF)
  {
    string errorMessage;
    if (!CCGFNodeMerger().MergeNodes(m_pCGF, usedMaterialIds, errorMessage))
    {
      m_LastError.Format("Error merging nodes: %s", errorMessage.c_str());
      delete m_pCGF;
      return 0;
    }
  }

  m_pListener = 0;

  return m_pCGF;
}

////////////////////////////////////////////////////////////////////////////
//void CLoaderCGF::MergeNodes()
//{
//	CMesh *pMergedMesh = m_pCGF->AllocMergedMesh();
//
//	AABB meshBBox;
//	meshBBox.Reset();
//	for (int i = 0; i < m_pCGF->GetNodeCount(); i++)
//	{
//		CNodeCGF *pNode = m_pCGF->GetNode(i);
//		if (!pNode->pMesh || pNode->bPhysicsProxy || pNode->type != CNodeCGF::NODE_MESH)
//			continue;
//
//		int nOldVerts = pMergedMesh->GetVertexCount();
//		if (pMergedMesh->GetVertexCount() == 0)
//		{
//			pMergedMesh->Copy( *pNode->pMesh );
//		}
//		else
//		{
//			pMergedMesh->Append( *pNode->pMesh );
//			// Keep color stream in sync size with vertex/normals stream.
//			if (pMergedMesh->m_streamSize[CMesh::COLORS_0] > 0 && pMergedMesh->m_streamSize[CMesh::COLORS_0] < pMergedMesh->GetVertexCount())
//			{
//				int nOldCount = pMergedMesh->m_streamSize[CMesh::COLORS_0];
//				pMergedMesh->ReallocStream( CMesh::COLORS_0,pMergedMesh->GetVertexCount() );
//				memset( pMergedMesh->m_pColor0+nOldCount,255,(pMergedMesh->GetVertexCount()-nOldCount)*sizeof(SMeshColor) );
//			}
//			if (pMergedMesh->m_streamSize[CMesh::COLORS_1] > 0 && pMergedMesh->m_streamSize[CMesh::COLORS_1] < pMergedMesh->GetVertexCount())
//			{
//				int nOldCount = pMergedMesh->m_streamSize[CMesh::COLORS_1];
//				pMergedMesh->ReallocStream( CMesh::COLORS_1,pMergedMesh->GetVertexCount() );
//				memset( pMergedMesh->m_pColor1+nOldCount,255,(pMergedMesh->GetVertexCount()-nOldCount)*sizeof(SMeshColor) );
//			}
//		}
//
//		AABB bbox = pNode->pMesh->m_bbox;
//		if (!pNode->bIdentityMatrix)
//			bbox.SetTransformedAABB( pNode->worldTM,bbox );
//
//		meshBBox.Add(bbox.min);
//		meshBBox.Add(bbox.max);
//		pMergedMesh->m_bbox = meshBBox;
//
//		if (!pNode->bIdentityMatrix)
//		{
//			// Transform merged mesh into the world space.
//			// Only transform newly added vertices.
//			for (int i = nOldVerts; i < pMergedMesh->GetVertexCount(); i++)
//			{
//				pMergedMesh->m_pPositions[i] = pNode->worldTM.TransformPoint( pMergedMesh->m_pPositions[i] );
//				pMergedMesh->m_pNorms[i] = pNode->worldTM.TransformVector( pMergedMesh->m_pNorms[i] ).GetNormalized();
//			}
//		}
//	}
//
//	SetupMesh( *pMergedMesh,m_pCGF->GetCommonMaterial() );
//}


//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadChunks()
{

	m_CompiledBones=false; 
	m_CompiledMesh=false;
	m_CompiledBonesBoxes=false;

	m_numBonenameList=0;
	m_numBoneInitialPos=0;
	m_numMorphTargets=0;
	m_numBoneHierarchy=0;

	// Load Nodes.	
	uint32 numChunck = m_pChunkFile->NumChunks();
	m_pCGF->GetSkinningInfo()->m_arrPhyBoneMeshes.clear();	
//	m_pCGF->GetSkinningInfo()->m_arrControllers.resize(numChunck);	
	m_pCGF->GetSkinningInfo()->m_numChunks = numChunck;	

	for (uint32 i=0; i<numChunck; i++)
	{

		const CHUNK_HEADER &hdr = m_pChunkFile->GetChunkHeader(i);

		if (m_IsCHR)
		{
			switch (hdr.ChunkType)
			{

			case ChunkType_BoneNameList:
				m_numBonenameList++;
				if (!ReadBoneNameList(m_pChunkFile->GetChunk(i)))
					return false;
				break;

			case ChunkType_BoneInitialPos:
				m_numBoneInitialPos++;
				if (!ReadBoneInitialPos(m_pChunkFile->GetChunk(i)))
					return false;
				break;

			case ChunkType_BoneAnim:
				m_numBoneHierarchy++;
				if ( !ReadBoneHierarchy(m_pChunkFile->GetChunk(i)) )
					return false;
				break;

			case ChunkType_BoneMesh:
				if (!ReadBoneMesh( m_pChunkFile->GetChunk(i)) )
					return false;
				break;        

			case ChunkType_MeshMorphTarget:
				m_numMorphTargets++;
				if (!ReadMorphTargets(m_pChunkFile->GetChunk(i)))
					return false;
				break;

				//---------------------------------------------------------
				//---       chunks for compiled characters             ----
				//---------------------------------------------------------

			case ChunkType_CompiledBones:
				if (!ReadCompiledBones( m_pChunkFile->GetChunk(i)) )
					return false;
				break;        

			case ChunkType_CompiledPhysicalBones:
				if (!ReadCompiledPhysicalBones( m_pChunkFile->GetChunk(i)) )
					return false;
				break;        

			case ChunkType_CompiledPhysicalProxies:
				if (!ReadCompiledPhysicalProxies(m_pChunkFile->GetChunk(i)))
					return false;
				break;

			case ChunkType_CompiledMorphTargets:
				if (!ReadCompiledMorphTargets(m_pChunkFile->GetChunk(i)))
					return false;
				break;


			case ChunkType_CompiledIntFaces:
				if (!ReadCompiledIntFaces( m_pChunkFile->GetChunk(i)) )
					return false;
				break;        

			case ChunkType_CompiledIntSkinVertices:
				if (!ReadCompiledIntSkinVertice( m_pChunkFile->GetChunk(i)) )
					return false;
				break;        

			case ChunkType_CompiledExt2IntMap:
				if (!ReadCompiledExt2IntMap( m_pChunkFile->GetChunk(i)) )
					return false;
				break;        

			case ChunkType_BonesBoxes:
				if (!ReadCompiledBonesBoxes( m_pChunkFile->GetChunk(i)) )
					return false;
				break;        

			}
		}

		switch (hdr.ChunkType)
		{
			//---------------------------------------------------------
			//---       chunks for CGA-objects  -----------------------
			//---------------------------------------------------------

		//case ChunkType_Controller:
		//	if (!ReadController( m_pChunkFile->GetChunk(i)) )
		//		return false;
		//	break;

		//case ChunkType_Timing:
		//	if (!ReadTiming( m_pChunkFile->GetChunk(i)) )
		//		return false;
		//	break;


		//case ChunkType_SpeedInfo:
		//	if (!ReadSpeedInfo( m_pChunkFile->GetChunk(i)) )
		//		return false;
		//	break;

		//case ChunkType_FootPlantInfo:
		//	if (!ReadFootPlantInfo(m_pChunkFile->GetChunk(i)))
		//		return false;
		//	break;

			//---------------------------------------------------------

		case ChunkType_ExportFlags:
			if (!LoadExportFlagsChunk( i ))
				return false;
			break;

		case ChunkType_Node:
			if (!LoadNodeChunk( i ))
				return false;
			break;

		case ChunkType_Mtl:

		case ChunkType_MtlName:
			LoadMaterialFromChunk( hdr.ChunkID);
			break;

		case ChunkType_BreakablePhysics:
			if (!ReadCompiledBreakablePhysics (m_pChunkFile->GetChunk(i)))
				return false;
			break;

		case ChunkType_FaceMap:
			if (!LoadFaceMapChunk(i))
				return false;
			break;
		}
	}

	return true;
}



bool CLoaderCGF::ReadBoneInitialPos (  IChunkFile::ChunkDesc *pChunkDesc  )
{
	BONEINITIALPOS_CHUNK_DESC_0001* pBIPChunk = (BONEINITIALPOS_CHUNK_DESC_0001*)pChunkDesc->data;
	SwapEndian(*pBIPChunk);
	if (pChunkDesc->hdr.ChunkVersion == pBIPChunk->VERSION)
	{
		CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
		SBoneInitPosMatrix* pDefMatrix = (SBoneInitPosMatrix*)(pBIPChunk+1);
		SwapEndian(pDefMatrix,pBIPChunk->numBones);

		uint32 numBones = pBIPChunk->numBones;
		m_arrInitPose34.resize(numBones);
		for (uint32 nBone=0; nBone<numBones; ++nBone)
		{
			m_arrInitPose34[nBone].m00=pDefMatrix[nBone].mx[0][0];		m_arrInitPose34[nBone].m01=pDefMatrix[nBone].mx[1][0];		m_arrInitPose34[nBone].m02=pDefMatrix[nBone].mx[2][0];	m_arrInitPose34[nBone].m03=pDefMatrix[nBone].mx[3][0]*0.01f;
			m_arrInitPose34[nBone].m10=pDefMatrix[nBone].mx[0][1];		m_arrInitPose34[nBone].m11=pDefMatrix[nBone].mx[1][1];		m_arrInitPose34[nBone].m12=pDefMatrix[nBone].mx[2][1];	m_arrInitPose34[nBone].m13=pDefMatrix[nBone].mx[3][1]*0.01f;
			m_arrInitPose34[nBone].m20=pDefMatrix[nBone].mx[0][2];		m_arrInitPose34[nBone].m21=pDefMatrix[nBone].mx[1][2];		m_arrInitPose34[nBone].m22=pDefMatrix[nBone].mx[2][2];	m_arrInitPose34[nBone].m23=pDefMatrix[nBone].mx[3][2]*0.01f;
			m_arrInitPose34[nBone].OrthonormalizeFast(); // for some reason Max supplies unnormalized matrices.
		} 
	} 
	else 
	{
		m_LastError.Format("BoneInitialPos chunk is of unknown version %d", pChunkDesc->hdr.ChunkVersion);
		return false; // unrecognized version
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////

bool CLoaderCGF::ReadMorphTargets( IChunkFile::ChunkDesc *pChunkDesc )
{

	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();

	uint32 version = pChunkDesc->hdr.ChunkVersion;
	uint32 id = pChunkDesc->hdr.ChunkID;

	if ( version==MESHMORPHTARGET_CHUNK_DESC_0001::VERSION)
	{

		MESHMORPHTARGET_CHUNK_DESC_0001* pMeshMorphTarget = (MESHMORPHTARGET_CHUNK_DESC_0001*)pChunkDesc->data;
		SwapEndian(*pMeshMorphTarget);

		uint32 numVertices = pMeshMorphTarget->numMorphVertices;
		assert(numVertices);

		// the chunk must at least contain its header and the name (min 2 bytes)
		int32 nDataSize = pChunkDesc->size-sizeof(MESHMORPHTARGET_CHUNK_DESC_0001);
		assert(nDataSize>0);
		if (nDataSize) 
		{
			SMeshMorphTargetVertex* pSrcVertices = (SMeshMorphTargetVertex*)(pMeshMorphTarget+1);
			SwapEndian( pSrcVertices,numVertices );


			MorphTargets *pSm = new MorphTargets;

			//store the mesh ID
			pSm->MeshID = pMeshMorphTarget->nChunkIdMesh;
			//store the name of morph-target
			const char* pName = (const char*)(pSrcVertices+numVertices);
			pSm->m_strName = "#" + string(pName);
			//store the vertices&indices of morph-target
			pSm->m_arrIntMorph.resize(numVertices);
			memcpy (&pSm->m_arrIntMorph[0], pSrcVertices, sizeof(SMeshMorphTargetVertex)*numVertices);

			pSkinningInfo->m_arrMorphTargets.push_back(pSm);

		} //if chunck-size
	}

	return 1;

}
//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadBoneNameList( IChunkFile::ChunkDesc *pChunkDesc )
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();

	uint32 version = pChunkDesc->hdr.ChunkVersion;
	uint32 id = pChunkDesc->hdr.ChunkID;
	uint32 nChunkSize = pChunkDesc->size;

	switch (version)
	{
	case BONENAMELIST_CHUNK_DESC_0744::VERSION:
		{
			// the old chunk version - fixed-size name list
			BONENAMELIST_CHUNK_DESC_0744* pNameChunk = (BONENAMELIST_CHUNK_DESC_0744*)(pChunkDesc->data);
			SwapEndian(*pNameChunk);

			int nGeomBones = pNameChunk->nEntities;

			// just fill in the pointers to the fixed-size string buffers
			const NAME_ENTITY* pGeomBoneNameTable = (const NAME_ENTITY*)(pNameChunk+1);
			if(nGeomBones < 0 || nGeomBones > 0x800)
				return false;
			m_arrBoneNameTable.resize(nGeomBones);
			for (int i = 0; i < nGeomBones; ++i)
				m_arrBoneNameTable[i] = pGeomBoneNameTable[i].name;
		}
		break;

	case BONENAMELIST_CHUNK_DESC_0745::VERSION:
		{
			// the new memory-economizing chunk with variable-length packed strings following tightly each other
			BONENAMELIST_CHUNK_DESC_0745* pNameChunk = (BONENAMELIST_CHUNK_DESC_0745*)(pChunkDesc->data);
			SwapEndian(*pNameChunk);
			uint32 nGeomBones = pNameChunk->numEntities;

			// we know how many strings there are there; the whole bunch of strings may 
			// be followed by pad zeros, to enable alignment
			m_arrBoneNameTable.resize(nGeomBones, "");

			// scan through all the strings, each string following immediately the 0 terminator of the previous one
			const char* pNameListEnd = ((const char*)pNameChunk) + nChunkSize;
			const char* pName = (const char*)(pNameChunk+1);
			uint32 numNames = 0;
			while (*pName && pName<pNameListEnd && numNames<nGeomBones)
			{
				m_arrBoneNameTable[numNames] = pName;
				pName += CryStringUtils::strnlen(pName, pNameListEnd) + 1;
				numNames++;
			}
			if (numNames < nGeomBones)
			{
				// the chunk is truncated
				m_LastError.Format( "inconsistent bone name list chunk: only %d out of %d bone names have been read.", numNames, nGeomBones);
				return false;
			}
		}
		break;

	}

	return true;
}



/////////////////////////////////////////////////////////////////////////////////////
// loads the root bone (and the hierarchy) and returns true if loaded successfully
// this gets called only for LOD 0
/////////////////////////////////////////////////////////////////////////////////////
uint32 CLoaderCGF::ReadBoneHierarchy ( IChunkFile::ChunkDesc* pChunkDesc )
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();

	BONEANIM_CHUNK_DESC_0290* pChunk = (BONEANIM_CHUNK_DESC_0290*)(pChunkDesc->data);
	SwapEndian(*pChunk);
	uint32 nChunkSize=pChunkDesc->size;

	//	uint32 numBoneDesc     = nChunkSize/sizeof(BONEANIM_CHUNK_DESC_0290);
	uint32 numBoneEntities = (nChunkSize-sizeof(BONEANIM_CHUNK_DESC_0290))/sizeof(BONE_ENTITY);

	m_pBoneAnimRawData = pChunk+1;
	m_pBoneAnimRawDataEnd = ((const char*) pChunk) + nChunkSize;

	if (nChunkSize < sizeof(*pChunk))
	{
		m_szLastError = "Couldn't read the data."	;
		m_pBoneAnimRawData = pChunk;
		return 0;
	}

	if (pChunk->nBones <= 0)
	{
		m_szLastError = "There must be at least one bone.";
		m_pBoneAnimRawData = pChunk;
		return 0;
	}

	const BONE_ENTITY* pBones = (const BONE_ENTITY*) (pChunk+1);
	if (m_pBoneAnimRawDataEnd < (const byte*)(pBones + pChunk->nBones))
	{
		m_szLastError = "Premature end of data.";
		return 0;
	}

	// check for one-and-only-one root rule
	if (pBones[0].ParentID != -1)
	{
		m_szLastError = "The first bone in the hierarchy has a parent, but it is expected to be the root bone.";
		return 0;
	}

	/*
	for (uint32 i=1; i<(uint32)pChunk->nBones; ++i)
	{
		if (pBones[i].ParentID == -1)
		{
			m_szLastError = "The skeleton has multiple roots. Only single-rooted skeletons are supported in this version.";
			return 0;
		}
	}
	*/

	pSkinningInfo->m_arrBonesDesc.resize (pChunk->nBones);
	CryBoneDescData zeroBone;
	memset(&zeroBone, 0, sizeof(zeroBone));
	std::fill(pSkinningInfo->m_arrBonesDesc.begin(), pSkinningInfo->m_arrBonesDesc.end(), zeroBone);
	m_arrIndexToId.resize (pChunk->nBones, ~0);
	m_arrIdToIndex.resize (pChunk->nBones, ~0);

	m_nNextBone = 1;
	assert (m_nNextBone <= (int)pChunk->nBones);

	m_numBones=0;

	for (uint32 i=0; i<(uint32)pChunk->nBones; ++i)
	{
		if (pBones[i].ParentID == -1)
		{
			int nRootBoneIndex = i;
			m_nNextBone = nRootBoneIndex + 1;
			RecursiveBoneLoader(nRootBoneIndex, nRootBoneIndex);
		}
	}
	assert(pChunk->nBones==m_numBones);


	//-----------------------------------------------------------------------------
	//---                  read physical information                            ---
	//-----------------------------------------------------------------------------
	assert(numBoneEntities==pChunk->nBones);
	pSkinningInfo->m_arrBoneEntities.resize(numBoneEntities);
	const BONE_ENTITY* pBoneEntity = (const BONE_ENTITY*)(pChunk+1);

	int32 test=0;
	for(uint32 i=0; i<numBoneEntities; i++)
	{
		pSkinningInfo->m_arrBoneEntities[i]=pBoneEntity[i];
		test |= pBoneEntity[i].phys.nPhysGeom;
		int aa=0;
	}
	return true;
}




// loads the whole hierarchy of bones, using the state machine
// when this funciton is called, the bone is already allocated
uint32 CLoaderCGF::RecursiveBoneLoader (int nBoneParentIndex, int nBoneIndex)
{
	m_numBones++;
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();

	BONE_ENTITY* pEntity=(BONE_ENTITY*)m_pBoneAnimRawData;
	SwapEndian(*pEntity);
	m_pBoneAnimRawData = ((uint8*)m_pBoneAnimRawData) + sizeof(BONE_ENTITY);

	// initialize the next bone
	CryBoneDescData& rBoneDesc = pSkinningInfo->m_arrBonesDesc[nBoneIndex];
	//read  bone info
	assert(pEntity->nChildren<200);

	CopyPhysInfo(rBoneDesc.m_PhysInfo[0], pEntity->phys);
	int nFlags = 0;
	if (pEntity->prop[0])
	{
		nFlags = joint_no_gravity|joint_isolated_accelerations;
	}
	else
	{
		if (!CryStringUtils::strnstr(pEntity->prop,"gravity", sizeof(pEntity->prop)))
			nFlags |= joint_no_gravity;
		if (!CryStringUtils::strnstr(pEntity->prop,"active_phys",sizeof(pEntity->prop)))
			nFlags |= joint_isolated_accelerations;
	}
	(rBoneDesc.m_PhysInfo[0].flags &= ~(joint_no_gravity|joint_isolated_accelerations)) |= nFlags;

	//get bone info
	rBoneDesc.m_nControllerID = pEntity->ControllerID;

	// set the mapping entries
	m_arrIndexToId[nBoneIndex] = pEntity->BoneID;
	m_arrIdToIndex[pEntity->BoneID] = nBoneIndex;

	rBoneDesc.m_nOffsetParent = nBoneParentIndex - nBoneIndex;

	// load children
	if (pEntity->nChildren)
	{
		int  nChildrenIndexBase = m_nNextBone;
		m_nNextBone += pEntity->nChildren;
		if (nChildrenIndexBase < 0)
			return 0;
		// load the children
		rBoneDesc.m_numChildren = pEntity->nChildren;
		rBoneDesc.m_nOffsetChildren = nChildrenIndexBase - nBoneIndex;
		for (int nChild = 0; nChild < pEntity->nChildren; ++nChild)
		{
			if (!RecursiveBoneLoader(nBoneIndex, nChildrenIndexBase + nChild))
				return 0;
		}
	}
	else
	{
		rBoneDesc.m_numChildren = 0;
		rBoneDesc.m_nOffsetChildren = 0;
	}
	return m_numBones;
}

//////////////////////////////////////////////////////////////////////////

uint32 CLoaderCGF::ReadBoneMesh ( IChunkFile::ChunkDesc* pChunkDesc )
{ 
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	MESH_CHUNK_DESC_0744* pMeshChunk = (MESH_CHUNK_DESC_0744*)pChunkDesc->data;
	SwapEndian(*pMeshChunk);
	uint32 nChunkSize = pChunkDesc->size;

	if (pMeshChunk->chdr.ChunkVersion != MESH_CHUNK_DESC_0744::VERSION)
	{
		m_LastError.Format ("CLoaderCGF::ReadBoneMesh: old file format, mesh chunk version 0x%08X (expected 0x%08X)",pMeshChunk->chdr.ChunkVersion, MESH_CHUNK_DESC_0744::VERSION);
		return false;
	}


	const void *pRawData = pMeshChunk+1;
	if (nChunkSize < sizeof(*pMeshChunk))
		return 0;
	nChunkSize -= sizeof(*pMeshChunk);
	if(pMeshChunk->nVerts<=0 || pMeshChunk->nVerts>60000)
		return 0;

	PhysicalProxy pbm;

	pbm.ChunkID = pMeshChunk->chdr.ChunkID;
	pbm.m_arrPoints.resize(pMeshChunk->nVerts);
	pbm.m_arrIndices.resize(pMeshChunk->nFaces*3);
	pbm.m_arrMaterials.resize(pMeshChunk->nFaces);


	//--------------------------------------------------------------------------
	//--- copy vertices from chunk
	//--------------------------------------------------------------------------
	CryVertex* pSrcVertices = (CryVertex*)pRawData;
	SwapEndian(pSrcVertices,pMeshChunk->nVerts);
	if ((const char*)(pSrcVertices + pMeshChunk->nVerts) > (const char*)pRawData + nChunkSize)
		return 0; // the cry vertex info is truncated

	for (int i = 0; i<pMeshChunk->nVerts; ++i)	
		pbm.m_arrPoints[i] = pSrcVertices[i].p*0.01f;

	pRawData = pSrcVertices + pMeshChunk->nVerts;


	//--------------------------------------------------------------------------
	//--- copy indices and MatIDs from chunk
	//--------------------------------------------------------------------------
	CryFace* arrCryFaces=(CryFace*)pRawData;
	SwapEndian(arrCryFaces,pMeshChunk->nFaces);
	for (int f = 0; f<pMeshChunk->nFaces; ++f)
	{
		pbm.m_arrIndices[f*3+0] = arrCryFaces[f][0];
		pbm.m_arrIndices[f*3+1] = arrCryFaces[f][1];
		pbm.m_arrIndices[f*3+2] = arrCryFaces[f][2];
		pbm.m_arrMaterials[f]		= arrCryFaces[f].MatID;
	}

	//check if faces are valid
	uint32 ValidateFaceIndices=1;
	uint32 numVertices = pbm.m_arrPoints.size();
	for (int i=0; i<pMeshChunk->nFaces; i++)
	{
		if (pbm.m_arrIndices[i*3+0] >= numVertices) { assert(0); ValidateFaceIndices=0; }
		if (pbm.m_arrIndices[i*3+1] >= numVertices) { assert(0); ValidateFaceIndices=0; }
		if (pbm.m_arrIndices[i*3+2] >= numVertices) { assert(0); ValidateFaceIndices=0; }
	}

	if (ValidateFaceIndices==0)	{
		m_LastError.Format ("CLoaderCGF::ReadBoneMesh: Indices out of range");
		return 0;
	}

	pSkinningInfo->m_arrPhyBoneMeshes.push_back(pbm);

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CLoaderCGF::IsSmaller(uint32 idx, uint32 i0, uint32 i1) const 
{	
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	return (pSkinningInfo->m_arrIntVertices[idx].weights[i0] < pSkinningInfo->m_arrIntVertices[idx].weights[i1]);	
}

void CLoaderCGF::Swap(uint32 idx, uint32 i0, uint32 i1 ) 
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	f32 w=pSkinningInfo->m_arrIntVertices[idx].weights[i0];
	pSkinningInfo->m_arrIntVertices[idx].weights[i0]=pSkinningInfo->m_arrIntVertices[idx].weights[i1];
	pSkinningInfo->m_arrIntVertices[idx].weights[i1]=w;
	uint16 b=pSkinningInfo->m_arrIntVertices[idx].boneIDs[i0];
	pSkinningInfo->m_arrIntVertices[idx].boneIDs[i0]=pSkinningInfo->m_arrIntVertices[idx].boneIDs[i1];
	pSkinningInfo->m_arrIntVertices[idx].boneIDs[i1]=b;
}




//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledBones (  IChunkFile::ChunkDesc *pChunkDesc  )
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	COMPILED_BONE_CHUNK_DESC_0800* pBIPChunk = (COMPILED_BONE_CHUNK_DESC_0800*)pChunkDesc->data;
	SwapEndian(*pBIPChunk);

	if (pChunkDesc->hdr.ChunkVersion == pBIPChunk->VERSION)
	{
		uint32 nDataSize = pChunkDesc->size-sizeof(COMPILED_BONE_CHUNK_DESC_0800);
//#if SIZEOF_PTR == 4
		uint32 numBones = nDataSize/sizeof(CryBoneDescData_Comp);
		pSkinningInfo->m_arrBonesDesc.resize(numBones);
		CryBoneDescData_Comp* pSrcVertices = (CryBoneDescData_Comp*)(pBIPChunk+1);
		SwapEndian( pSrcVertices,numBones );

		for (uint32 i=0; i<numBones; i++) 
		{
			pSkinningInfo->m_arrBonesDesc[i].m_nControllerID = pSrcVertices[i].m_nControllerID;
			memcpy(&pSkinningInfo->m_arrBonesDesc[i].m_fMass, &pSrcVertices[i].m_fMass, 
				(INT_PTR)&pSrcVertices[i+1]-(INT_PTR)&pSrcVertices[i].m_fMass);
			for(uint32 j=0; j<2; j++)
			{
				pSkinningInfo->m_arrBonesDesc[i].m_PhysInfo[j].pPhysGeom = (phys_geometry*)(INT_PTR)pSrcVertices[i].m_PhysInfo[j].nPhysGeom;
				memcpy(&pSkinningInfo->m_arrBonesDesc[i].m_PhysInfo[j].flags, &pSrcVertices[i].m_PhysInfo[j].flags, 
					(INT_PTR)&pSrcVertices[i].m_PhysInfo[j+1]-(INT_PTR)&pSrcVertices[i].m_PhysInfo[j].flags);
			}
		}
/*#else
		const uint32 boneSize = StructSize<CryBoneDescData>();
		const uint32 numBones = nDataSize / boneSize;
		pSkinningInfo->m_arrBonesDesc.resize(numBones);
		CryBoneDescData *pSrcVertices = new CryBoneDescData[numBones];
		const uint8 *const pSrcData = (const uint8 *)(pBIPChunk + 1);
		for (uint32 i = 0; i < numBones; ++i)
		{
			StructUnpack(pSrcVertices + i, pSrcData + i * boneSize);			
			pSrcVertices[i].m_PhysInfo[0].pPhysGeom = (phys_geometry*)(INT_PTR)((CryBoneDescData_Comp*) (pSrcData + i * boneSize))->m_PhysInfo[0].nPhysGeom;
			pSrcVertices[i].m_PhysInfo[1].pPhysGeom = (phys_geometry*)(INT_PTR)((CryBoneDescData_Comp*) (pSrcData + i * boneSize))->m_PhysInfo[1].nPhysGeom;
			int tmp = (INT_PTR)((CryBoneDescData_Comp*) (pSrcData + i * boneSize))->m_PhysInfo[0].nPhysGeom;;
			SwapEndian(tmp);
			pSrcVertices[i].m_PhysInfo[0].pPhysGeom = (phys_geometry*)tmp;
			tmp = (INT_PTR)((CryBoneDescData_Comp*) (pSrcData + i * boneSize))->m_PhysInfo[1].nPhysGeom;;
			SwapEndian(tmp);
			pSrcVertices[i].m_PhysInfo[1].pPhysGeom = (phys_geometry*)tmp;

			//SwapEndian(pSrcVertices[i].m_PhysInfo[1].pPhysGeom);
		}


		for (uint32 i=0; i<numBones; i++) 
			pSkinningInfo->m_arrBonesDesc[i]=pSrcVertices[i];

		delete []pSrcVertices;
#endif*/

		m_CompiledBones=true; 
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledPhysicalBones (  IChunkFile::ChunkDesc *pChunkDesc  )
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	COMPILED_PHYSICALBONE_CHUNK_DESC_0800* pChunk = (COMPILED_PHYSICALBONE_CHUNK_DESC_0800*)pChunkDesc->data;
	SwapEndian(*pChunk);

	if (pChunkDesc->hdr.ChunkVersion == pChunk->VERSION)
	{
		uint32 nDataSize = pChunkDesc->size-sizeof(COMPILED_PHYSICALBONE_CHUNK_DESC_0800);
		uint32 numBones = nDataSize/sizeof(BONE_ENTITY);
		pSkinningInfo->m_arrBoneEntities.resize(numBones);
		BONE_ENTITY* pSrcVertices = (BONE_ENTITY*)(pChunk+1);
		SwapEndian( pSrcVertices,numBones );
		for (uint32 i=0; i<numBones; i++) 
			pSkinningInfo->m_arrBoneEntities[i]=pSrcVertices[i];

		m_CompiledBones=true; 
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledPhysicalProxies( IChunkFile::ChunkDesc *pChunkDesc )
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	COMPILED_PHYSICALPROXY_CHUNK_DESC_0800* pIMTChunk = (COMPILED_PHYSICALPROXY_CHUNK_DESC_0800*)pChunkDesc->data;
	SwapEndian(*pIMTChunk);

	const uint8* rawdata=(const uint8*)(pIMTChunk+1);		
	PhysicalProxy sm;
	if (pChunkDesc->hdr.ChunkVersion == pIMTChunk->VERSION)
	{
		uint32 numPhysicalProxies = pIMTChunk->numPhysicalProxies;
		for(uint32 i=0; i<numPhysicalProxies; i++)
		{
			SMeshPhysicalProxyHeader* header=(SMeshPhysicalProxyHeader*)rawdata;
			SwapEndian(*header);
			rawdata += sizeof(SMeshPhysicalProxyHeader);

			//store the chunck-id
			sm.ChunkID=header->ChunkID;

			//store the vertices
			sm.m_arrPoints.resize(header->numPoints);
			SwapEndian( (Vec3*)rawdata,header->numPoints );
			memcpy (&sm.m_arrPoints[0], rawdata, sizeof(Vec3)*header->numPoints);
			rawdata += sizeof(Vec3)*header->numPoints;

			//store the indices
			sm.m_arrIndices.resize(header->numIndices);
			SwapEndian( (uint16*)rawdata,header->numIndices );
			memcpy (&sm.m_arrIndices[0], rawdata, sizeof(uint16)*header->numIndices);
			rawdata += sizeof(uint16)*header->numIndices;

			//store the materials
			sm.m_arrMaterials.resize(header->numMaterials);
			memcpy (&sm.m_arrMaterials[0], rawdata, sizeof(uint8)*header->numMaterials);
			rawdata += sizeof(uint8)*header->numMaterials;

			pSkinningInfo->m_arrPhyBoneMeshes.push_back(sm);
		}
	}
	return 1;
}


//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledMorphTargets( IChunkFile::ChunkDesc *pChunkDesc )
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	COMPILED_MORPHTARGETS_CHUNK_DESC_0800* pIMTChunk = (COMPILED_MORPHTARGETS_CHUNK_DESC_0800*)pChunkDesc->data;
	SwapEndian(*pIMTChunk);

	const uint8* rawdata=(const uint8*)(pIMTChunk+1);		

	if (pChunkDesc->hdr.ChunkVersion == pIMTChunk->VERSION || pChunkDesc->hdr.ChunkVersion == pIMTChunk->VERSION1)
	{
		if (pChunkDesc->hdr.ChunkVersion == pIMTChunk->VERSION1)
			pSkinningInfo->m_bRotatedMorphTargets = true;
			

		uint32 numMorphTargets = pIMTChunk->numMorphTargets;
		
		for(uint32 i=0; i<numMorphTargets; i++)
		{
			MorphTargetsPtr pSm =  new MorphTargets;

			SMeshMorphTargetHeader* header=(SMeshMorphTargetHeader*)rawdata;
			SwapEndian(*header);
			rawdata += sizeof(SMeshMorphTargetHeader);

			//store the mesh ID
			pSm->MeshID = header->MeshID;
			//store the name of morph-target
			const char* pName = (const char*)(rawdata);
			pSm->m_strName = string(pName);
			rawdata += header->NameLength;

			//store the internal vertices&indices of morph-target
			pSm->m_arrIntMorph.resize(header->numIntVertices);
			SwapEndian( (SMeshMorphTargetVertex*)rawdata,header->numIntVertices );
			memcpy (&pSm->m_arrIntMorph[0], rawdata, sizeof(SMeshMorphTargetVertex)*header->numIntVertices);
			rawdata += sizeof(SMeshMorphTargetVertex)*header->numIntVertices;

			//store the external vertices&indices of morph-target
			pSm->m_arrExtMorph.resize(header->numExtVertices);
			SwapEndian( (SMeshMorphTargetVertex*)rawdata,header->numExtVertices );
			memcpy (&pSm->m_arrExtMorph[0], rawdata, sizeof(SMeshMorphTargetVertex)*header->numExtVertices);
			rawdata += sizeof(SMeshMorphTargetVertex)*header->numExtVertices;

			pSkinningInfo->m_arrMorphTargets.push_back(pSm);
		}
	}
	return 1;
}


//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledIntFaces (  IChunkFile::ChunkDesc *pChunkDesc  )
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	COMPILED_INTFACES_CHUNK_DESC_0800* pChunk = (COMPILED_INTFACES_CHUNK_DESC_0800*)pChunkDesc->data;
	SwapEndian(*pChunk);

	if (pChunkDesc->hdr.ChunkVersion == pChunk->VERSION)
	{
		uint32 nDataSize = pChunkDesc->size-sizeof(COMPILED_INTFACES_CHUNK_DESC_0800);
		uint32 numIntFaces = nDataSize/sizeof(TFace);
		pSkinningInfo->m_arrIntFaces.resize(numIntFaces);
		const TFace* pSrc = (const TFace*)(pChunk+1);
		SwapEndian( (uint16*)pSrc,numIntFaces*3 ); // TFace is 3 uint16
		for (uint32 i=0; i<numIntFaces; i++) 
			pSkinningInfo->m_arrIntFaces[i]=pSrc[i];

		m_CompiledMesh|=2;
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledIntSkinVertice (  IChunkFile::ChunkDesc *pChunkDesc  )
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	COMPILED_INTSKINVERTICES_CHUNK_DESC_0800* pBIPChunk = (COMPILED_INTSKINVERTICES_CHUNK_DESC_0800*)pChunkDesc->data;
	SwapEndian(*pBIPChunk);

	if (pChunkDesc->hdr.ChunkVersion == pBIPChunk->VERSION)
	{
		uint32 nDataSize = pChunkDesc->size-sizeof(COMPILED_INTSKINVERTICES_CHUNK_DESC_0800);
		uint32 numIntVertices = nDataSize/sizeof(IntSkinVertex);
		pSkinningInfo->m_arrIntVertices.resize(numIntVertices);
		IntSkinVertex* pSrcVertices = (IntSkinVertex*)(pBIPChunk+1);
		SwapEndian( pSrcVertices,numIntVertices );
		for (uint32 i=0; i<numIntVertices; i++) 
			pSkinningInfo->m_arrIntVertices[i]=pSrcVertices[i];

		m_CompiledMesh|=1;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledBonesBoxes (  IChunkFile::ChunkDesc *pChunkDesc  )
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	COMPILED_BONEBOXES_CHUNK_DESC_0800* pBonesChunk = (COMPILED_BONEBOXES_CHUNK_DESC_0800*)pChunkDesc->data;
	SwapEndian(*pBonesChunk);

	if (pChunkDesc->hdr.ChunkVersion == pBonesChunk->VERSION || pChunkDesc->hdr.ChunkVersion == pBonesChunk->VERSION1)
	{
		if (pChunkDesc->hdr.ChunkVersion == pBonesChunk->VERSION1) {
			pSkinningInfo->m_bProperBBoxes = true;
		}
		char * pSrc = (char*)(pBonesChunk+1);

		pSkinningInfo->m_arrCollisions.push_back(MeshCollisionInfo());
		MeshCollisionInfo& info = pSkinningInfo->m_arrCollisions[pSkinningInfo->m_arrCollisions.size() - 1];

		SwapEndian((int*)pSrc, 1);
		memcpy(&info.m_iBoneId, pSrc, sizeof(int));
		pSrc += sizeof(int);
//		SwapEndian(&info.m_iBoneId);

		SwapEndian((AABB*)pSrc, 1);
		memcpy(&info.m_aABB, pSrc, sizeof(AABB));
		pSrc += sizeof(AABB);
//		SwapEndian(&info.m_aABB);

		int size;
		SwapEndian((int*)pSrc, 1);
		memcpy(&size, pSrc, sizeof(int));
		pSrc += sizeof(int);
//		SwapEndian(&size);

		if (size) {
			info.m_arrIndexes.resize(size);
			SwapEndian((short int*)pSrc, size);
			memcpy(&info.m_arrIndexes[0], pSrc, size * sizeof(short int));
		}
//		short int * pData = &info.m_arrIndexes[0];
//		SwapEndian( pData,size );

		m_CompiledBonesBoxes = true;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledExt2IntMap (  IChunkFile::ChunkDesc *pChunkDesc  )
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	COMPILED_EXT2INTMAP_CHUNK_DESC_0800* pChunk = (COMPILED_EXT2INTMAP_CHUNK_DESC_0800*)pChunkDesc->data;
	SwapEndian(*pChunk);

	if (pChunkDesc->hdr.ChunkVersion == pChunk->VERSION)
	{
		uint32 nDataSize = pChunkDesc->size-sizeof(COMPILED_EXT2INTMAP_CHUNK_DESC_0800);
		uint32 numExtVertices = nDataSize/sizeof(uint16);
		pSkinningInfo->m_arrExt2IntMap.resize(numExtVertices);
		uint16* pSrc = (uint16*)(pChunk+1);
		SwapEndian( pSrc,numExtVertices );
		for (uint32 i=0; i<numExtVertices; i++)
		{ 
			uint32 idx = pSrc[i];
			assert(idx!=0xffff);
			pSkinningInfo->m_arrExt2IntMap[i]=idx;
		}
		m_CompiledMesh|=4;
		return 1;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::ReadCompiledBreakablePhysics (  IChunkFile::ChunkDesc *pChunkDesc  )
{
	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();
	BREAKABLE_PHYSICS_CHUNK_DESC* pChunk = (BREAKABLE_PHYSICS_CHUNK_DESC*)pChunkDesc->data;
	SwapEndian(*pChunk);

	if (pChunkDesc->hdr.ChunkVersion == pChunk->VERSION)
	{
		if(!m_pCGF)
			return false;

		CPhysicalizeInfoCGF* pPi = m_pCGF->GetPhysiclizeInfo();

		pPi->nGranularity = pChunk->granularity;
		pPi->nMode = pChunk->nMode;
		pPi->nRetVtx = pChunk->nRetVtx;
		pPi->nRetTets = pChunk->nRetTets;
		if(pPi->nRetVtx>0)
		{
			pPi->pRetVtx = new Vec3[pPi->nRetVtx];
			char * pData = ((char *)pChunk) + sizeof(BREAKABLE_PHYSICS_CHUNK_DESC);
			SwapEndian( (Vec3*)pData,pPi->nRetVtx );
			memcpy(pPi->pRetVtx, pData, pPi->nRetVtx * sizeof(Vec3));
		}
		if(pPi->nRetTets>0)
		{
			pPi->pRetTets= new int[pPi->nRetTets*4];
			char * pData = ((char *)pChunk) + sizeof(BREAKABLE_PHYSICS_CHUNK_DESC) + pPi->nRetVtx * sizeof(Vec3);
			SwapEndian( (int*)pData,pPi->nRetTets*4 );
			memcpy(pPi->pRetTets, pData, pPi->nRetTets * sizeof(int)*4);
		}
		return 1;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

#define SIZEOF_ARRAY(arr) (sizeof(arr)/sizeof((arr)[0]))

uint32 CLoaderCGF::ProcessSkinning() 
{
	std::vector<ExtSkinVertexTemp> arrExtSkinTemp; 
	std::vector<SMeshSubset> arrMaterialGroups;  //materials of the mesh as they appear in video-mem
	std::vector<SMeshSubset> arrSubsets;         //subsets of the mesh as they appear in video-mem
	std::vector<TFace> arrExtFaces;

	CSkinningInfo* pSkinningInfo = m_pCGF->GetSkinningInfo();

	uint32 numBonenames	=	m_arrBoneNameTable.size();
	uint32 numIPos			=	m_arrInitPose34.size();
	uint32 numBones			=	pSkinningInfo->m_arrBonesDesc.size();

	//do we have a initiallised skeletton?
	if ( numBones ) 
	{	

		if (m_CompiledBones==0) 
		{
			//process raw bone-data
			assert(m_numBonenameList<2);
			assert(m_numBoneInitialPos<2);
			assert(m_numBoneHierarchy<2);

			if (numBones != numIPos)
			{
				m_LastError.Format("Skeleton-Initial-Positions are missing.");
				assert(0);
				return false;
			}

			if (numBones != numBonenames)
			{
				m_LastError.Format("Number of bones does not match in the bone hierarchy chunk (%d) and the bone name chunk (%d)", numBones,numBonenames);
				assert(0);
				return false;
			}

			//------------------------------------------------------------------------------------------
			//----     use the old chunks to initialize the bone structure  ----------------------------
			//------------------------------------------------------------------------------------------
			static const char *g_arrLimbNames[4] = { "L UpperArm","R UpperArm","L Thigh","R Thigh" };
			uint32 numBonesDesc = pSkinningInfo->m_arrBonesDesc.size();
			for (uint32 nBone=0; nBone<numBonesDesc; ++nBone)
			{
				uint32 nBoneID = m_arrIndexToId[nBone];
				if (nBoneID == -1)
					continue;

				pSkinningInfo->m_arrBonesDesc[nBone].m_DefaultW2B = m_arrInitPose34[nBoneID].GetInvertedFast();
				pSkinningInfo->m_arrBonesDesc[nBone].m_DefaultB2W = m_arrInitPose34[nBoneID];

				memset( pSkinningInfo->m_arrBonesDesc[nBone].m_arrBoneName, 0, sizeof(pSkinningInfo->m_arrBonesDesc[nBone].m_arrBoneName));
				strcpy( pSkinningInfo->m_arrBonesDesc[nBone].m_arrBoneName, m_arrBoneNameTable[nBoneID].c_str() );

				//fill in names of the bones and the limb IDs              ---
				uint32 nBoneId = m_arrIndexToId[nBone];
				pSkinningInfo->m_arrBonesDesc[nBone].m_nLimbId = -1;
				for (int j=0; j < SIZEOF_ARRAY(g_arrLimbNames) ;j++)
					if (strstr(m_arrBoneNameTable[nBoneId],g_arrLimbNames[j]))	
					{
						pSkinningInfo->m_arrBonesDesc[nBone].m_nLimbId = j;
						break;
					}
			}
		}




		if (m_CompiledMesh!=0 && m_CompiledMesh!=7 ) 
		{
			//seems we have a mix of old an new chunks
			assert(0);
		}

		if (m_CompiledMesh==0) 
		{
			//------------------------------------------------------------------------------------------
			//----     get the mesh    -----------------------------------------------------------------
			//------------------------------------------------------------------------------------------
			uint32 numNodes = m_pCGF->GetNodeCount();
			assert(numNodes);
			CNodeCGF* pNode=0;
			for(uint32 n=0; n<numNodes; n++) 
			{
				if (m_pCGF->GetNode(n)->type==CNodeCGF::NODE_MESH) 
				{
					pNode = m_pCGF->GetNode(n);
					break;
				}
			}
			assert(pNode);
			CMesh* pMesh = pNode->pMesh;
			assert(pMesh);
			uint32 numIntVertices = pMesh->m_numVertices;


			if (pMesh->m_pColor0)
			{
				//-----------------------------------------------------------------
				//--- do a color-snap on all vcolors and calculate index        ---
				//-----------------------------------------------------------------
				for (uint32 i=0; i<numIntVertices; ++i)
				{
					pMesh->m_pColor0[i].r = (pMesh->m_pColor0[i].r>0x80) ? 0xff : 0x00;
					pMesh->m_pColor0[i].g = (pMesh->m_pColor0[i].g>0x80) ? 0xff : 0x00;
					pMesh->m_pColor0[i].b = (pMesh->m_pColor0[i].b>0x80) ? 0xff : 0x00;
					pMesh->m_pColor0[i].a = 0;
					//calcultate the index (this is the only value we need in the vertex-shader)
					if (pMesh->m_pColor0[i].r) pMesh->m_pColor0[i].a|=1;
					if (pMesh->m_pColor0[i].g) pMesh->m_pColor0[i].a|=2;
					if (pMesh->m_pColor0[i].b) pMesh->m_pColor0[i].a|=4;
				}
			}

			//--------------------------------------------------------------
			//---        copy the links into geometry info               ---
			//--------------------------------------------------------------
			uint32 numLinks = m_arrLinksTmp.size();
			assert(numIntVertices==numLinks);
			uint32 Id2Index = m_arrIdToIndex.size();
			assert(Id2Index);
			for(uint32 i=0; i<numIntVertices; i++) 
			{
				CryVertexBinding& rLinks_source = m_arrLinksTmp[i];
				uint32 numVertexLinks = rLinks_source.size(); // the number of links for this vertex
				assert(numVertexLinks); 
				rLinks_source.remapBoneIds (0, &m_arrIdToIndex[0], m_arrIdToIndex.size());
			}

			//---------------------------------------------------------------------
			//---                 create internal SkinBuffer                    ---
			//---------------------------------------------------------------------
			pSkinningInfo->m_arrIntVertices.resize(numIntVertices);
			for (uint32 nVert=0; nVert<numIntVertices; ++nVert)
			{

				// the number of links for this vertex
				const CryVertexBinding& rLinks_base = m_arrLinksTmp[nVert];
				uint32 numVertexLinks = rLinks_base.size();
				assert(numVertexLinks); 

				//vertex-position of model.0
				Vec3 pos0[4]={ Vec3(ZERO),Vec3(ZERO),Vec3(ZERO),Vec3(ZERO) };
				//vertex-position of model.1
				Vec3 pos1[4]={ Vec3(ZERO),Vec3(ZERO),Vec3(ZERO),Vec3(ZERO) };
				//vertex-position of model.2 
				Vec3 pos2[4]={ Vec3(ZERO),Vec3(ZERO),Vec3(ZERO),Vec3(ZERO) };

				IntSkinVertex v;

				if (pMesh->m_pColor0)
				{
					v.color.r = pMesh->m_pColor0[nVert].r;
					v.color.g = pMesh->m_pColor0[nVert].g;
					v.color.b = pMesh->m_pColor0[nVert].b;
					v.color.a = pMesh->m_pColor0[nVert].a;
				}
				else
				{
					v.color.r = 0xff;
					v.color.g = 0xff;
					v.color.b = 0xff;
					v.color.a = 1|2|4;
				}

				v.boneIDs[0]=0;	v.boneIDs[1]=0;		v.boneIDs[2]=0;		v.boneIDs[3]=0;
				v.weights[0]=0;	v.weights[1]=0;	v.weights[2]=0;	v.weights[3]=0;
				v.wpos0 = Vec3(ZERO);
				v.wpos1 = Vec3(ZERO);
				v.wpos2 = Vec3(ZERO);

				for (uint32 l=0; l<numVertexLinks; l++ ) 
				{
					Vec3 base	=	rLinks_base[l].offset;
					Vec3 thin = Vec3(base.x, base.y*0.7f, base.z*0.7f);
					Vec3 fat  = Vec3(base.x, base.y*1.3f, base.z*1.3f);

					pos0[l] = thin;
					pos1[l] = base;
					pos2[l] = fat;
					v.boneIDs[l] = rLinks_base[l].BoneID;
					v.weights[l] = rLinks_base[l].Blending;
				}

				//---------------------------------------------------
				//---  optimize vertex-links 
				//---------------------------------------------------
				uint32 sl=numVertexLinks;
				for (uint32 x=0; x<sl; x++) {
					for (uint32 y=x+1; y<sl; y++) {
						if (v.boneIDs[x]==v.boneIDs[y]) {
							//add blending of same bone to 1st found bone in list;			
							v.weights[x]+=v.weights[y];					
							//remove link from the list;
							for (uint32 z=y; z<sl; z++) 
							{ 
								if (z<3) {
									pos0[z]=pos0[z+1];	pos1[z]=pos1[z+1];	pos2[z]=pos2[z+1];
									v.boneIDs[z]=v.boneIDs[z+1];	v.weights[z]=v.weights[z+1];
								} else {
									pos0[z]=Vec3(ZERO);	pos1[z]=Vec3(ZERO);	pos2[z]=Vec3(ZERO);
									v.boneIDs[z]=0; v.weights[z]=0;
								}
							}
							sl--;
							y--;  
						}
					}
				}
				f32 t=0;
				for (uint32 i=0; i<4; i++)	t+=v.weights[i]; //sum up all blending values
				for (uint32 i=0; i<4; i++)	v.weights[i]/=t; //normalized blending

#if defined (_DEBUG)
				uint32 counter=0;
				counter+=(v.weights[0]!=0);
				counter+=(v.weights[1]!=0);
				counter+=(v.weights[2]!=0);
				counter+=(v.weights[3]!=0);
				if (counter==2)	{
					assert(v.boneIDs[0]!=v.boneIDs[1]);
				}
				if (counter==3)	{
					assert(v.boneIDs[0]!=v.boneIDs[1]);
					assert(v.boneIDs[0]!=v.boneIDs[2]);
					assert(v.boneIDs[1]!=v.boneIDs[2]);
				}
				if (counter==4)	{
					assert(v.boneIDs[0]!=v.boneIDs[1]);
					assert(v.boneIDs[0]!=v.boneIDs[2]);
					assert(v.boneIDs[0]!=v.boneIDs[3]);
					assert(v.boneIDs[1]!=v.boneIDs[2]);
					assert(v.boneIDs[1]!=v.boneIDs[3]);
					assert(v.boneIDs[2]!=v.boneIDs[3]);
				}
				//check if summed blending of all bones is 1.0f
				f32 Blending=0;
				for (uint32 i=0; i<4; i++)	Blending+=v.weights[i];
				assert( fabsf(Blending-1.0f)<0.005f );
#endif

				//----------------------------------------------------
				//transform vertices from bone-space into world-space
				//----------------------------------------------------
				uint32 b0 = v.boneIDs[0];
				uint32 b1 = v.boneIDs[1];
				uint32 b2 = v.boneIDs[2];
				uint32 b3 = v.boneIDs[3];

				f32 w0 = v.weights[0];
				f32 w1 = v.weights[1];
				f32 w2 = v.weights[2];
				f32 w3 = v.weights[3];

				Vec3 tv0=pSkinningInfo->m_arrBonesDesc[b0].m_DefaultB2W * pos0[0] * w0;
				Vec3 tv1=pSkinningInfo->m_arrBonesDesc[b1].m_DefaultB2W * pos0[1] * w1;
				Vec3 tv2=pSkinningInfo->m_arrBonesDesc[b2].m_DefaultB2W * pos0[2] * w2;
				Vec3 tv3=pSkinningInfo->m_arrBonesDesc[b3].m_DefaultB2W * pos0[3] * w3;
				v.wpos0 = tv0+tv1+tv2+tv3;

				Vec3 bv0=pSkinningInfo->m_arrBonesDesc[b0].m_DefaultB2W * pos1[0] * w0;
				Vec3 bv1=pSkinningInfo->m_arrBonesDesc[b1].m_DefaultB2W * pos1[1] * w1;
				Vec3 bv2=pSkinningInfo->m_arrBonesDesc[b2].m_DefaultB2W * pos1[2] * w2;
				Vec3 bv3=pSkinningInfo->m_arrBonesDesc[b3].m_DefaultB2W * pos1[3] * w3;
				v.wpos1 = bv0+bv1+bv2+bv3;

				Vec3 fv0=pSkinningInfo->m_arrBonesDesc[b0].m_DefaultB2W * pos2[0] * w0;
				Vec3 fv1=pSkinningInfo->m_arrBonesDesc[b1].m_DefaultB2W * pos2[1] * w1;
				Vec3 fv2=pSkinningInfo->m_arrBonesDesc[b2].m_DefaultB2W * pos2[2] * w2;
				Vec3 fv3=pSkinningInfo->m_arrBonesDesc[b3].m_DefaultB2W * pos2[3] * w3;
				v.wpos2 = fv0+fv1+fv2+fv3;

				pSkinningInfo->m_arrIntVertices[nVert]=v;
			}

			//------------------------------------------------------------------
			//---            re-order according to weighting                 ---
			//------------------------------------------------------------------
			for (uint32 i=0; i<numIntVertices; i++)
			{
				uint32 num=0;
				num+=(pSkinningInfo->m_arrIntVertices[i].weights[0]!=0);
				num+=(pSkinningInfo->m_arrIntVertices[i].weights[1]!=0);
				num+=(pSkinningInfo->m_arrIntVertices[i].weights[2]!=0);
				num+=(pSkinningInfo->m_arrIntVertices[i].weights[3]!=0);
				if (num==2) {
					if (IsSmaller(i,0,1))	Swap(i,0,1);
				}
				if (num==3) {
					if (IsSmaller(i,0,1))	Swap(i,0,1);
					if (IsSmaller(i,0,2))	Swap(i,0,2);
					if (IsSmaller(i,1,2))	Swap(i,1,2);
				}
				if (num==4) {
					if (IsSmaller(i,0,1))	Swap(i,0,1);
					if (IsSmaller(i,0,2))	Swap(i,0,2);
					if (IsSmaller(i,0,3))	Swap(i,0,3);
					if (IsSmaller(i,1,2))	Swap(i,1,2);
					if (IsSmaller(i,1,3))	Swap(i,1,3);
					if (IsSmaller(i,2,3))	Swap(i,2,3);
				}
			}


			//--------------------------------------------------------------------------
			//---              sort faces by materials                               ---
			//--------------------------------------------------------------------------
			// for each material, construct an array of CryFaces and keep the faces (in the original order) in there
			typedef std::vector<TFace> GeomFaceArray;
			typedef std::map<uint8,GeomFaceArray> IntFaceMtlMap;
			IntFaceMtlMap mapMtlFace;

			std::vector<bool> arrMaterialUsage;
			arrMaterialUsage.resize( MAX_SUB_MATERIALS, false );

			// put each face into its material group;
			// the corresponding groups CryFaceArray will be created by the map automatically
			// upon the first request of that group

			uint32 numSubsets = pMesh->m_subsets.size();
/*			uint32 nMaxSubsetID = 0;
			for (uint32 i=0; i<numSubsets; i++) 
			{
				uint32 id = pMesh->m_subsets[i].nMatID;
				if (nMaxSubsetID<id) nMaxSubsetID = id;			
			} 
			nMaxSubsetID++;*/

			uint32 numIntFaces = pMesh->GetFacesCount();

			uint32 MaxMtlID=0;
			for (uint32 i=0; i<numIntFaces; ++i)
			{
				uint8 GeomMtlID = pMesh->m_pFaces[i].nSubset;
				if (MaxMtlID<GeomMtlID) 
					MaxMtlID=GeomMtlID;
				assert(GeomMtlID<MAX_SUB_MATERIALS);
				mapMtlFace[GeomMtlID].push_back( TFace(pMesh->m_pFaces[i].v[0],pMesh->m_pFaces[i].v[1],pMesh->m_pFaces[i].v[2]) );
				arrMaterialUsage[GeomMtlID] = true;
			}
			uint32 test = mapMtlFace.size();
			//--------------------------------------------------------------------------
			//---     create array with internal faces (sorted by materials)           ---
			//--------------------------------------------------------------------------
			uint32 nNewFace = 0;
			pSkinningInfo->m_arrIntFaces.resize(numIntFaces);
			uint32 numMaterials=mapMtlFace.size();
	//		nMaxSubsetID=numMaterials;
			for (IntFaceMtlMap::iterator itMtl=mapMtlFace.begin(); itMtl!=mapMtlFace.end(); ++itMtl)
			{
				uint32 num = itMtl->second.size();
				for (uint32 f=0; f<num; f++,nNewFace++)
					pSkinningInfo->m_arrIntFaces[nNewFace] = itMtl->second[f];
			}
			assert (nNewFace==numIntFaces);


			// Compile contents.
			std::vector<uint16> arrIRemapping;
			std::vector<uint16> arrVRemapping;
			m_pCompiledCGF = MakeCompiledCGF( m_pCGF, &arrVRemapping, &arrIRemapping  );
			if (!m_pCompiledCGF)
				return false;
			uint32 numIRemapping = arrIRemapping.size();	assert(numIRemapping);
			uint32 numVRemapping = arrVRemapping.size();	assert(numVRemapping);

			//allocates the external to internal map entries
			pSkinningInfo->m_arrExt2IntMap.resize( numVRemapping);//, ~0 );
			for (uint32 i=0; i<numIntFaces; i++)
			{		
				uint32 idx0=arrVRemapping[ arrIRemapping[i*3+0] ];
				uint32 idx1=arrVRemapping[ arrIRemapping[i*3+1] ];
				uint32 idx2=arrVRemapping[ arrIRemapping[i*3+2] ];
				if( idx0>=numVRemapping ||	idx1>=numVRemapping ||	idx2>=numVRemapping ) 
				{ 
					m_LastError.Format("Indices out of range");	
					assert(0);
					return false;
				}
				assert( pSkinningInfo->m_arrIntFaces[i].i0<numIntVertices );
				assert( pSkinningInfo->m_arrIntFaces[i].i1<numIntVertices );
				assert( pSkinningInfo->m_arrIntFaces[i].i2<numIntVertices );
				pSkinningInfo->m_arrExt2IntMap[idx0] = pSkinningInfo->m_arrIntFaces[i].i0;
				pSkinningInfo->m_arrExt2IntMap[idx1] = pSkinningInfo->m_arrIntFaces[i].i1;
				pSkinningInfo->m_arrExt2IntMap[idx2] = pSkinningInfo->m_arrIntFaces[i].i2;
			}

			for (uint32 i=0; i<numVRemapping; i++)
			{		
				if ( pSkinningInfo->m_arrExt2IntMap[i]==0xffff )
				{
					m_LastError.Format("Remapping-table is broken. One or several vertices not remapped");
					assert(0);
					return false;
				}
			}

			//-------------------------------------------------------------------------

			arrExtSkinTemp.resize(numVRemapping);
			for (uint32 i=0; i<numVRemapping; i++)
			{
				uint32 index=pSkinningInfo->m_arrExt2IntMap[i];	
				arrExtSkinTemp[i].wpos0			= pSkinningInfo->m_arrIntVertices[index].wpos0;
				arrExtSkinTemp[i].wpos1			= pSkinningInfo->m_arrIntVertices[index].wpos1;
				arrExtSkinTemp[i].wpos2			= pSkinningInfo->m_arrIntVertices[index].wpos2;
				arrExtSkinTemp[i].weight[0]	= pSkinningInfo->m_arrIntVertices[index].weights[0];
				arrExtSkinTemp[i].weight[1]	= pSkinningInfo->m_arrIntVertices[index].weights[1];
				arrExtSkinTemp[i].weight[2]	= pSkinningInfo->m_arrIntVertices[index].weights[2];
				arrExtSkinTemp[i].weight[3]	= pSkinningInfo->m_arrIntVertices[index].weights[3];
				arrExtSkinTemp[i].bone[0]		= pSkinningInfo->m_arrIntVertices[index].boneIDs[0];
				arrExtSkinTemp[i].bone[1]		= pSkinningInfo->m_arrIntVertices[index].boneIDs[1];
				arrExtSkinTemp[i].bone[2]		= pSkinningInfo->m_arrIntVertices[index].boneIDs[2];
				arrExtSkinTemp[i].bone[3]		= pSkinningInfo->m_arrIntVertices[index].boneIDs[3];
				arrExtSkinTemp[i].color			= pSkinningInfo->m_arrIntVertices[index].color;
				arrExtSkinTemp[i].u	=	pMesh->m_pTexCoord[i].s;
				arrExtSkinTemp[i].v	=	pMesh->m_pTexCoord[i].t;
				arrExtSkinTemp[i].binormal	=	pMesh->m_pTangents[i].Binormal;
				arrExtSkinTemp[i].tangent	=	pMesh->m_pTangents[i].Tangent;
			}

			//--------------------------------------------------------------------------
			// Initialize the material list for the RenderMesh
			//--------------------------------------------------------------------------
			arrMaterialGroups.resize(numMaterials);
			for (uint32 m=0; m<numMaterials; m++)
				arrMaterialGroups[m] = pMesh->m_subsets[m];






			//--------------------------------------------------------------------------
			//---         sort render-batches for hardware skinning                  ---
			//--------------------------------------------------------------------------

			std::vector<MSplit> arrMaterialSplit;
			arrMaterialSplit.resize(numMaterials);

			RBatch rbatch;

			uint32 numExtFaces=numIRemapping/3;
			std::vector<TFace> arrExtFacesBackup;
			arrExtFacesBackup.resize(numExtFaces);
			for(uint32 i=0,t=0; t<numExtFaces; t++) 
			{
				arrExtFacesBackup[t].i0 = pMesh->m_pIndices[i]; i++;		
				arrExtFacesBackup[t].i1 = pMesh->m_pIndices[i]; i++;		
				arrExtFacesBackup[t].i2 = pMesh->m_pIndices[i]; i++;		
			}


			for (uint32 m=0; m<numMaterials; m++) 
			{
				arrMaterialSplit[m].numSplits=0;
				arrMaterialSplit[m].arrRemapIDs.resize(numBones,0xffff);
				std::vector<uint32> arrUsedBones;  
				std::vector<uint32> arrUsedBonesLast;  
				arrUsedBones.resize(numBones,0);	
				arrUsedBonesLast.resize(numBones,0);	
				for (uint32 b=0; b<numBones; b++)	arrUsedBones[b]=0;

				uint32 nFirstFaceId	= arrMaterialGroups[m].nFirstIndexId/3;
				uint16* pIndices		= &arrExtFacesBackup[nFirstFaceId].i0;

				uint32 numIndices	= arrMaterialGroups[m].nNumIndices;
				uint32 numFaces		= arrMaterialGroups[m].nNumIndices/3;
				for (uint32 f=0; f<numIndices; f++)
				{
					uint32 i=pIndices[f];
					uint32 b0=arrExtSkinTemp[i].bone[0];
					uint32 b1=arrExtSkinTemp[i].bone[1];
					uint32 b2=arrExtSkinTemp[i].bone[2];
					uint32 b3=arrExtSkinTemp[i].bone[3];
					if (arrExtSkinTemp[i].weight[0]) arrUsedBones[ b0 ]=1;
					if (arrExtSkinTemp[i].weight[1]) arrUsedBones[ b1 ]=1;
					if (arrExtSkinTemp[i].weight[2]) arrUsedBones[ b2 ]=1;
					if (arrExtSkinTemp[i].weight[3]) arrUsedBones[ b3 ]=1;
				}

				uint32 Count=0;
				for (uint32 b=0; b<numBones; b++)
					Count+=arrUsedBones[b];

				//-----------------------------------------------------------
				//---          split material                             ---
				//-----------------------------------------------------------
				if (Count>MAX_BONES_IN_BATCH) 
				{
					arrUsedBones.resize(numBones);	
					arrUsedBonesLast.resize(numBones);	
					for (uint32 b=0; b<numBones; b++)	arrUsedBones[b]=0;

					uint32 numFaceCounter=0;
					for (int32 f=0; f<numIndices; f=f+3)
					{
						for (uint32 b=0; b<numBones; b++)
							arrUsedBonesLast[b]=arrUsedBones[b];

						uint32 i0=pIndices[f+0];
						uint32 i1=pIndices[f+1];
						uint32 i2=pIndices[f+2];
						if (i0==0xffff) continue;
						if (i1==0xffff) continue;
						if (i2==0xffff) continue;

						if (arrExtSkinTemp[i0].weight[0])	arrUsedBones[ arrExtSkinTemp[i0].bone[0] ]=1;
						if (arrExtSkinTemp[i0].weight[1])	arrUsedBones[ arrExtSkinTemp[i0].bone[1] ]=1;
						if (arrExtSkinTemp[i0].weight[2])	arrUsedBones[ arrExtSkinTemp[i0].bone[2] ]=1;
						if (arrExtSkinTemp[i0].weight[3])	arrUsedBones[ arrExtSkinTemp[i0].bone[3] ]=1;

						if (arrExtSkinTemp[i1].weight[0])	arrUsedBones[ arrExtSkinTemp[i1].bone[0] ]=1;
						if (arrExtSkinTemp[i1].weight[1])	arrUsedBones[ arrExtSkinTemp[i1].bone[1] ]=1;
						if (arrExtSkinTemp[i1].weight[2])	arrUsedBones[ arrExtSkinTemp[i1].bone[2] ]=1;
						if (arrExtSkinTemp[i1].weight[3])	arrUsedBones[ arrExtSkinTemp[i1].bone[3] ]=1;

						if (arrExtSkinTemp[i2].weight[0])	arrUsedBones[ arrExtSkinTemp[i2].bone[0] ]=1;
						if (arrExtSkinTemp[i2].weight[1])	arrUsedBones[ arrExtSkinTemp[i2].bone[1] ]=1;
						if (arrExtSkinTemp[i2].weight[2])	arrUsedBones[ arrExtSkinTemp[i2].bone[2] ]=1;
						if (arrExtSkinTemp[i2].weight[3])	arrUsedBones[ arrExtSkinTemp[i2].bone[3] ]=1;

						//count bones/
						uint32 iCount=0;
						uint32 lCount=0;
						for (uint32 b=0; b<numBones; b++)
						{
							iCount+=arrUsedBones[b];
							lCount+=arrUsedBonesLast[b];
						}
						numFaceCounter++;

						if (iCount>MAX_BONES_IN_BATCH) 
						{
							rbatch.numBonesPerBatch=lCount;
							rbatch.m_arrBoneIDs.clear();
							for (uint32 b=0; b<numBones; b++)	if (arrUsedBonesLast[b]) rbatch.m_arrBoneIDs.push_back(b);
							rbatch.startFace	= arrExtFaces.size();
							rbatch.numFaces		=	PushIntoNewBuffer(arrExtSkinTemp,arrExtFaces,pIndices,numIndices,arrUsedBonesLast);
							rbatch.MaterialID	=	arrMaterialGroups[m].nMatID;
						//	assert(rbatch.MaterialID<nMaxSubsetID);
							m_arrBatches.push_back( rbatch );
							arrMaterialSplit[m].numSplits++;
							for (uint32 b=0; b<numBones; b++)	arrUsedBones[b]=0;
							numFaceCounter=0;
							f=-3;//restart loop
						} 
					}

					uint32 iCount=0;
					for (uint32 b=0; b<numBones; b++)
						iCount+=arrUsedBones[b];

					if (iCount) 
					{
						rbatch.numBonesPerBatch=iCount;
						rbatch.m_arrBoneIDs.clear();
						for (uint32 b=0; b<numBones; b++)	if (arrUsedBones[b]) rbatch.m_arrBoneIDs.push_back(b);
						rbatch.startFace	= arrExtFaces.size();
						rbatch.numFaces		=	PushIntoNewBuffer(arrExtSkinTemp,arrExtFaces,pIndices,numIndices,arrUsedBones);
						rbatch.MaterialID	=	arrMaterialGroups[m].nMatID;
					//	assert(rbatch.MaterialID<nMaxSubsetID);
						m_arrBatches.push_back( rbatch );
						arrMaterialSplit[m].numSplits++;
					}
				} 
				else 
				{
					uint32 iCount=0;
					for (uint32 b=0; b<numBones; b++)
						iCount+=arrUsedBones[b];

					if (iCount) 
					{
						rbatch.numBonesPerBatch=iCount;
						rbatch.m_arrBoneIDs.clear();
						for (uint32 b=0; b<numBones; b++)	if (arrUsedBones[b]) rbatch.m_arrBoneIDs.push_back(b);
						rbatch.startFace	=	arrExtFaces.size();
						rbatch.numFaces		=	PushIntoNewBuffer(arrExtSkinTemp,arrExtFaces,pIndices,numIndices,arrUsedBones);
						rbatch.MaterialID	=	arrMaterialGroups[m].nMatID;
						//if (rbatch.MaterialID>=nMaxSubsetID) rbatch.MaterialID=(numMaterials-1);
						//assert(rbatch.MaterialID<nMaxSubsetID);
						m_arrBatches.push_back( rbatch );
						arrMaterialSplit[m].numSplits++;
					}
				}
			}





			//-------------------------------------------------------------------------
			//---                 check if material batches overlap                 ---
			//-------------------------------------------------------------------------
			for (uint32 m=0; m<numMaterials; m++) 
			{
				uint32 vmin=0xffffffff;
				uint32 vmax=0x00000000;

				uint32 nFirstFaceId	= arrMaterialGroups[m].nFirstIndexId/3;
				uint32 numFaces			= arrMaterialGroups[m].nNumIndices/3;
				for(uint32 f=0; f<numFaces; f++)
				{
					uint32 i0=arrExtFaces[nFirstFaceId+f].i0;
					uint32 i1=arrExtFaces[nFirstFaceId+f].i1;
					uint32 i2=arrExtFaces[nFirstFaceId+f].i2;
					if (vmin>i0) vmin=i0;
					if (vmin>i1) vmin=i1;
					if (vmin>i2) vmin=i2;
					if (vmax<i0) vmax=i0;
					if (vmax<i1) vmax=i1;
					if (vmax<i2) vmax=i2;
				}
				uint32 a = arrMaterialGroups[m].nFirstVertId;
				uint32 b = arrMaterialGroups[m].nNumVerts;
				assert(arrMaterialGroups[m].nFirstVertId == vmin);
				assert(arrMaterialGroups[m].nNumVerts    == (vmax-vmin+1));
			}

			for (uint32 a=0; a<numMaterials; a++) 
			{
				for (uint32 b=0; b<numMaterials; b++) 
				{
					if (a==b) continue;
					uint32 amin = arrMaterialGroups[a].nFirstVertId;
					uint32 amax = arrMaterialGroups[a].nNumVerts+amin-1;
					uint32 bmin = arrMaterialGroups[b].nFirstVertId;
					uint32 bmax = arrMaterialGroups[b].nNumVerts+bmin-1;
					assert(amax<bmin || amin>bmax);
				}
			}


			//-------------------------------------------------------------------------
			//-------------------------------------------------------------------------
			//-------------------------------------------------------------------------

			//FILE* stream = fopen( "c:\\bones.txt", "w+b" );

			uint32 numBatches=m_arrBatches.size();
			uint32 numMaterialSplits=arrMaterialSplit.size();

			uint32 base=0;
			for(uint32 m=0; m<numMaterialSplits; m++)
			{
				uint32 numSplits=arrMaterialSplit[m].numSplits;
				if (numSplits>1)
				{
					std::vector<uint32> arrBoneCount;
					arrBoneCount.resize(numBones,0);

					for (uint32 s=0; s<numSplits; s++)
					{
						m_arrBatches[s+base].m_arrBoneFlags.resize(numBones,0);
						uint32 numBonesPerBatch = m_arrBatches[s+base].numBonesPerBatch;
						for(uint32 i=0; i<numBonesPerBatch; i++)	m_arrBatches[s+base].m_arrBoneFlags[ m_arrBatches[s+base].m_arrBoneIDs[i] ]=1;
						m_arrBatches[s+base].m_arrBoneIDs.clear();
						for(uint32 i=0; i<numBones; i++)	arrBoneCount[i]+=m_arrBatches[s+base].m_arrBoneFlags[i];
					}


					//----------------------------------------------------
					//---  push bone-indices into the colums           ---
					//----------------------------------------------------
					for(uint32 l=0; l<numBones; l++) 
					{
						//find the most often used boneIDs
						uint32 max=0;	uint32 idx=0xffff;
						for(uint32 i=0; i<numBones; i++)	if (max<arrBoneCount[i])	{	max=arrBoneCount[i]; idx=i;	}

						if (max==0) break; 

						for (uint32 s=0; s<numSplits; s++)
						{
							if (m_arrBatches[s+base].m_arrBoneFlags[idx]) 
								m_arrBatches[s+base].m_arrBoneIDs.push_back(idx); 
							else 
								m_arrBatches[s+base].m_arrBoneIDs.push_back(0xffff);
						}
						arrBoneCount[idx]=0;
					}	

					//----------------------------------------------------

					/*for (uint32 s=0; s<numSplits; s++)
					{
					uint32 numIDs=m_arrBatches[s+base].m_arrBoneIDs.size();
					fprintf(stream, "numIDs:%04x = ",numIDs);
					for(uint32 i=0; i<numIDs; i++)
					fprintf(stream, "%04x ",m_arrBatches[s+base].m_arrBoneIDs[i]);
					fprintf(stream, "\r\n");
					}
					fprintf(stream, "\r\n\r\n");*/

					//----------------------------------------------------
					//---              BoneColumn Sorter               ---
					//----------------------------------------------------
					uint32 numIDs=m_arrBatches[base].m_arrBoneIDs.size();
					for (uint32 comp=1; comp<numIDs; comp++)
					{
						for (uint32 dest=0; dest<comp; dest++)
						{
							//loop over all lines
							uint32 match=1;
							for (uint32 s=0; s<numSplits; s++)
							{
								if (m_arrBatches[s+base].m_arrBoneIDs[comp] != 0xffff) 
									match &= m_arrBatches[s+base].m_arrBoneIDs[dest]==0xffff;
							}

							if (match) 
							{
								//re-arrange indices in columns
								for (uint32 s=0; s<numSplits; s++)
								{
									if (m_arrBatches[s+base].m_arrBoneIDs[comp]!=0xffff) 
									{
										m_arrBatches[s+base].m_arrBoneIDs[dest] = m_arrBatches[s+base].m_arrBoneIDs[comp];
										m_arrBatches[s+base].m_arrBoneIDs[comp] = 0xffff;;
									}
								}
							}

						}
					}

					for (uint32 s=0; s<numSplits; s++)
					{
						//find maximum used ammout of boneIDs for this batch
						uint32 numBoneIDs=m_arrBatches[s+base].m_arrBoneIDs.size();
						for(uint32 i=(numBoneIDs-1); i!=0; i--)
						{
							if (m_arrBatches[s+base].m_arrBoneIDs[i] != 0xffff) 
							{
								numBoneIDs=(i+1);
								m_arrBatches[s+base].m_arrBoneIDs.resize(numBoneIDs);
								for (uint32 d=0; d<numBoneIDs; d++) 
									if (m_arrBatches[s+base].m_arrBoneIDs[d]==0xffff) m_arrBatches[s+base].m_arrBoneIDs[d]=0;

								break;
							}
						}

						//fprintf(stream, "numBoneIDs:%04x = ",numBoneIDs);
						for(uint32 i=0; i<numBoneIDs; i++)
						{
							uint32 idx=m_arrBatches[s+base].m_arrBoneIDs[i];
							if (idx!=0xffff) arrMaterialSplit[m].arrRemapIDs[idx]=i;
							//fprintf(stream, "%04x ",idx);
						}
						//fprintf(stream, "\r\n");
					}

				} 
				else 
				{
					//find maximum used amout of boneIDs for this batch
					uint32 numIDs=m_arrBatches[base].m_arrBoneIDs.size();
					assert (numIDs); 
					for(uint32 i=(numIDs-1); i!=0; i--)
					{
						if (m_arrBatches[base].m_arrBoneIDs[i] != 0xffff) 
						{
							numIDs=i+1;
							m_arrBatches[base].m_arrBoneIDs.resize( numIDs );
							for (uint32 d=0; d<numIDs; d++) 
								if (m_arrBatches[base].m_arrBoneIDs[d]==0xffff) m_arrBatches[base].m_arrBoneIDs[d]=0;
							break;
						}
					}
					//fprintf(stream, "numIDs:%04x = ",numIDs);
					for(uint32 i=0; i<numIDs; i++)
					{
						uint32 idx=m_arrBatches[base].m_arrBoneIDs[i];
						if (idx!=0xffff) arrMaterialSplit[m].arrRemapIDs[idx]=i;
						//fprintf(stream, "%04x ",idx);
					}
					//fprintf(stream, "\r\n");
				}

				uint32 numRemaps=arrMaterialSplit[m].arrRemapIDs.size();
				//fprintf(stream, "RemapArray:   ");
				for(uint32 i=0; i<numRemaps; i++)
				{
					uint32 idx=arrMaterialSplit[m].arrRemapIDs[i];
					//fprintf(stream, "%04x ",idx);
				}
				//fprintf(stream, "\r\n");
				//fprintf(stream, "--------------------------------------------------------\r\n\r\n");
				base+=numSplits;

			} //loop over all materials

		//	fclose( stream );



			//-------------------------------------------------------------------------
			//---           set remapping-array in RBatch                            ---
			//-------------------------------------------------------------------------
			uint32 rbcounter=0;
			for (uint32 m=0; m<numMaterialSplits; m++) 
			{
				uint32 numRemapIDs= arrMaterialSplit[m].arrRemapIDs.size();
				uint32 numSplits	= arrMaterialSplit[m].numSplits;
				for (uint32 s=0; s<numSplits; s++)
				{
					m_arrBatches[rbcounter].m_arrRemapIDs.resize(numRemapIDs);
					for(uint32 i=0; i<numRemapIDs; i++)	m_arrBatches[rbcounter].m_arrRemapIDs[i]=arrMaterialSplit[m].arrRemapIDs[i];
					rbcounter++;
				}
			}


			uint32 testcheck = pMesh->m_subsets.size();
			uint32 numBatches2 = m_arrBatches.size();
			arrSubsets.resize(numBatches2);

			for (uint32 m=0; m<numBatches2; m++)
			{

				uint32 mat = m_arrBatches[m].MaterialID;
			//	assert(mat<testcheck);	
				uint32 found=0;
				uint32 r=0;
				for (r=0; r<testcheck; r++) 
				{
					if (mat==pMesh->m_subsets[r].nMatID) 
					{
						found=1;
						break;
					}
				}
				assert(found);

				arrSubsets[m]								= pMesh->m_subsets[r];
				arrSubsets[m].nMatID				= m_arrBatches[m].MaterialID;
				arrSubsets[m].nFirstIndexId	= m_arrBatches[m].startFace*3;
				arrSubsets[m].nNumIndices		= m_arrBatches[m].numFaces*3;

				//uint32 sv  = arrSubsets[m].nFirstVertId;
				//uint32 ev  = arrSubsets[m].nNumVerts+sv;

				//Make sure the all vertices are in range if indices. This is important for ATI-kards
				uint32 sml_index=0xffffffff;
				uint32 big_index=0x00000000;
				uint32 sface=m_arrBatches[m].startFace;
				uint32 eface=m_arrBatches[m].numFaces + sface;
				for(uint32 i=sface; i<eface; i++)
				{
					uint32 i0=arrExtFaces[i].i0;
					uint32 i1=arrExtFaces[i].i1;
					uint32 i2=arrExtFaces[i].i2;
					if (sml_index>i0) sml_index=i0;
					if (sml_index>i1) sml_index=i1;
					if (sml_index>i2) sml_index=i2;
					if (big_index<i0) big_index=i0;
					if (big_index<i1) big_index=i1;
					if (big_index<i2) big_index=i2;
				};
				arrSubsets[m].nFirstVertId	=	sml_index;
				arrSubsets[m].nNumVerts			=	big_index-sml_index;

				//arrSubsets[m].m_arrGlobalBonesPerSubset	= m_arrBatches[m].m_arrBoneIDs;
				{
					arrSubsets[m].m_arrGlobalBonesPerSubset.resize(m_arrBatches[m].m_arrBoneIDs.size());
					for (uint32 i=0,num = m_arrBatches[m].m_arrBoneIDs.size(); i < num; i++)
						arrSubsets[m].m_arrGlobalBonesPerSubset[i] = m_arrBatches[m].m_arrBoneIDs[i];
				}
			}


			pMesh->m_subsets.clear();
			pMesh->m_subsets.reserve( numBatches2 );
			for (uint32 m=0; m<numBatches2; m++)
			{
				pMesh->m_subsets.push_back(arrSubsets[m]);
			}


			//-------------------------------------------------------------------------
			//---           Remap all BoneIDs                                       ---
			//-------------------------------------------------------------------------
			for (uint32 m=0; m<numMaterialSplits; m++) 
			{
				uint32 start = arrMaterialGroups[m].nFirstVertId;
				uint32 end   = arrMaterialGroups[m].nNumVerts+start;
				for (uint32 v=start; v<end; v++)
				{

					for (uint32 b=0; b<4; b++)
					{
						if (arrExtSkinTemp[v].weight[b])	
						{
							uint32 o	=	arrExtSkinTemp[v].bone[b];
							uint32 rb	=	arrMaterialSplit[m].arrRemapIDs[o];
							assert(rb<MAX_BONES_IN_BATCH);
							arrExtSkinTemp[v].bone[b]=rb;
						}
					}

				}
			}


			//--------------------------------------------------------------------------
			//---              copy compiled-data back into CMesh                    ---
			//--------------------------------------------------------------------------
			int m_nFaceCount	=pMesh->m_numFaces;
			int m_nVertCount	=pMesh->m_numVertices;
			int m_nCoorCount	=pMesh->m_nCoorCount; // number of texture coordinates in m_pTexCoord array
			int m_nIndexCount	=pMesh->m_nIndexCount;

			//	free(pMesh->m_pIndices);
			uint32 numExtFaces2 = arrExtFaces.size();
			for (uint32 f=0; f<numExtFaces2; f++)
			{
				pMesh->m_pIndices[f*3+0]=arrExtFaces[f].i0;
				pMesh->m_pIndices[f*3+1]=arrExtFaces[f].i1;
				pMesh->m_pIndices[f*3+2]=arrExtFaces[f].i2;
			}

			//////////////////////////////////////////////////////////////////////////
			// Copy bone-mapping.
			//////////////////////////////////////////////////////////////////////////
			pMesh->ReallocStream( CMesh::BONEMAPPING, numVRemapping );
			for (uint32 i=0; i<numVRemapping; i++ )
			{
				pMesh->m_pBoneMapping[i].boneIDs[0] = (uint8)arrExtSkinTemp[i].bone[0];
				pMesh->m_pBoneMapping[i].boneIDs[1] = (uint8)arrExtSkinTemp[i].bone[1];
				pMesh->m_pBoneMapping[i].boneIDs[2] = (uint8)arrExtSkinTemp[i].bone[2];
				pMesh->m_pBoneMapping[i].boneIDs[3] = (uint8)arrExtSkinTemp[i].bone[3];

				pMesh->m_pBoneMapping[i].weights[0] = (uint8) ( arrExtSkinTemp[i].weight[0]*255.0f + 0.5f);
				pMesh->m_pBoneMapping[i].weights[1] = (uint8) ( arrExtSkinTemp[i].weight[1]*255.0f + 0.5f );
				pMesh->m_pBoneMapping[i].weights[2] = (uint8) ( arrExtSkinTemp[i].weight[2]*255.0f + 0.5f );
				pMesh->m_pBoneMapping[i].weights[3] = (uint8) ( arrExtSkinTemp[i].weight[3]*255.0f + 0.5f );

				uint32 sum = pMesh->m_pBoneMapping[i].weights[0]
				+pMesh->m_pBoneMapping[i].weights[1]
				+pMesh->m_pBoneMapping[i].weights[2]
				+pMesh->m_pBoneMapping[i].weights[3];

				if (sum>0xff) 
					pMesh->m_pBoneMapping[i].weights[0]--;
				if (sum<0xff) 
					pMesh->m_pBoneMapping[i].weights[0]++;

				sum = pMesh->m_pBoneMapping[i].weights[0]
				+pMesh->m_pBoneMapping[i].weights[1]
				+pMesh->m_pBoneMapping[i].weights[2]
				+pMesh->m_pBoneMapping[i].weights[3];
				if (sum==0x100) assert(0);
				if (sum <0x0ff) assert(0);


			}

			//////////////////////////////////////////////////////////////////////////
			// Copy shape-deformation.
			//////////////////////////////////////////////////////////////////////////
			pMesh->ReallocStream( CMesh::SHAPEDEFORMATION, numVRemapping );
			for (uint32 e=0; e<numVRemapping; e++ )
			{
				uint32 i=pSkinningInfo->m_arrExt2IntMap[e];
				pMesh->m_pPositions[e]							= pSkinningInfo->m_arrIntVertices[i].wpos1;
				pMesh->m_pShapeDeformation[e].thin	= pSkinningInfo->m_arrIntVertices[i].wpos0;
				pMesh->m_pShapeDeformation[e].fat		= pSkinningInfo->m_arrIntVertices[i].wpos2;
				pMesh->m_pShapeDeformation[e].index	= pSkinningInfo->m_arrIntVertices[i].color;
			}

			//--------------------------------------------------------------------------
			//---              prepare morph-targets                                 ---
			//--------------------------------------------------------------------------
			uint32 numMorphTargets = pSkinningInfo->m_arrMorphTargets.size();
			Matrix34 mat34 = pNode->localTM*Diag33(0.01f,0.01f,0.01f);
			for (uint32 it=0; it<numMorphTargets; ++it)
			{
				//init internal morph-targets
				MorphTargets* pMorphtarget = pSkinningInfo->m_arrMorphTargets[it];
				uint32 numMorphVerts =pMorphtarget->m_arrIntMorph.size();
				for (uint32 i=0; i<numMorphVerts; i++)
				{
					uint32 idx		= pMorphtarget->m_arrIntMorph[i].nVertexId;
					Vec3 mvertex	= (mat34*pMorphtarget->m_arrIntMorph[i].ptVertex) - pSkinningInfo->m_arrIntVertices[idx].wpos1;
					pMorphtarget->m_arrIntMorph[i].ptVertex	=	mvertex;
				}

				//init external morph-targets
				for (uint32 v=0; v<numMorphVerts; v++)
				{
					uint32 idx		= pMorphtarget->m_arrIntMorph[v].nVertexId;
					Vec3 mvertex	= pMorphtarget->m_arrIntMorph[v].ptVertex;

					const uint16* pExtToIntMap = &pSkinningInfo->m_arrExt2IntMap[0];
					uint32 numExtVertices = numVRemapping;
					assert(numExtVertices);
					for (uint32 i=0; i<numExtVertices; ++i)
					{
						uint32 index=pExtToIntMap[i];	
						if (index==idx) 
						{
							SMeshMorphTargetVertex mp;
							mp.nVertexId=i;
							mp.ptVertex =mvertex;
							pMorphtarget->m_arrExtMorph.push_back(mp);
						}
					}
				}
			}



		}

	}


	return true;
}




//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------
uint32 CLoaderCGF::PushIntoNewBuffer( std::vector<ExtSkinVertexTemp>& arrExtSkinTemp, std::vector<TFace>& arrExtFaces, uint16* pIndices, uint32 numIndices, std::vector<uint32>& arrUsedBonesLast) 
{
	uint32 FaceCounter=0;
	for (uint32 q=0; q<numIndices; q=q+3 )
	{
		uint32 i0=pIndices[q+0];
		uint32 i1=pIndices[q+1];
		uint32 i2=pIndices[q+2];
		if (i0==0xffff) continue;
		if (i1==0xffff) continue;
		if (i2==0xffff) continue;

		uint32 found0=1;
		if (arrExtSkinTemp[i0].weight[0]) found0 &= arrUsedBonesLast[ arrExtSkinTemp[i0].bone[0] ];
		if (arrExtSkinTemp[i0].weight[1]) found0 &= arrUsedBonesLast[ arrExtSkinTemp[i0].bone[1] ];
		if (arrExtSkinTemp[i0].weight[2]) found0 &= arrUsedBonesLast[ arrExtSkinTemp[i0].bone[2] ];
		if (arrExtSkinTemp[i0].weight[3]) found0 &= arrUsedBonesLast[ arrExtSkinTemp[i0].bone[3] ];

		uint32 found1=1;
		if (arrExtSkinTemp[i1].weight[0]) found1 &= arrUsedBonesLast[ arrExtSkinTemp[i1].bone[0] ];
		if (arrExtSkinTemp[i1].weight[1]) found1 &= arrUsedBonesLast[ arrExtSkinTemp[i1].bone[1] ];
		if (arrExtSkinTemp[i1].weight[2]) found1 &= arrUsedBonesLast[ arrExtSkinTemp[i1].bone[2] ];
		if (arrExtSkinTemp[i1].weight[3]) found1 &= arrUsedBonesLast[ arrExtSkinTemp[i1].bone[3] ];

		uint32 found2=1;
		if (arrExtSkinTemp[i2].weight[0]) found2 &= arrUsedBonesLast[ arrExtSkinTemp[i2].bone[0] ];
		if (arrExtSkinTemp[i2].weight[1]) found2 &= arrUsedBonesLast[ arrExtSkinTemp[i2].bone[1] ];
		if (arrExtSkinTemp[i2].weight[2]) found2 &= arrUsedBonesLast[ arrExtSkinTemp[i2].bone[2] ];
		if (arrExtSkinTemp[i2].weight[3]) found2 &= arrUsedBonesLast[ arrExtSkinTemp[i2].bone[3] ];

		uint32 found = found0&found1&found2;
		if (found) 
		{ 
			FaceCounter++;
			arrExtFaces.push_back( TFace(i0,i1,i2) );
			pIndices[q+0]=0xffff;
			pIndices[q+1]=0xffff;
			pIndices[q+2]=0xffff;
		}
	}
	return FaceCounter;
}

//////////////////////////////////////////////////////////////////////////
CContentCGF* CLoaderCGF::MakeCompiledCGF( CContentCGF *pCGF, std::vector<uint16> *pVertexRemapping, std::vector<uint16> *pIndexRemapping )
{
	CContentCGF *pCompiledCGF = new CContentCGF( pCGF->GetFilename() );
	*pCompiledCGF->GetExportInfo() = *pCGF->GetExportInfo(); // Copy export info.

	if (pCGF->GetMergedMesh())
	{
		//strcpy( g_sCurrentNode,"Merged" );

		// Compile single merged mesh.
		mesh_compiler::CMeshCompiler meshCompiler;

		if (pIndexRemapping)
			meshCompiler.SetIndexRemapping( pIndexRemapping );
		if (pVertexRemapping)
			meshCompiler.SetVertexRemapping( pVertexRemapping );

    //Warning( "CLoaderCGF::MakeCompiledCGF: Stripifying geometry for file %s", pCGF->GetFilename() );

		if (!meshCompiler.Compile( *pCGF->GetMergedMesh() ))
		{
			m_LastError.Format("Failed to compile geometry file %s - %s",pCGF->GetFilename(), meshCompiler.GetLastError());
			delete pCompiledCGF;
			return 0;
		}

		//////////////////////////////////////////////////////////////////////////
		// Add single node for merged mesh.
		CNodeCGF *pNode = new CNodeCGF;
		pNode->type = CNodeCGF::NODE_MESH;
		pNode->name = "Merged";
		pNode->localTM.SetIdentity();
		pNode->worldTM.SetIdentity();
		pNode->pos.Set(0,0,0);
		pNode->rot.SetIdentity();
		pNode->scl.Set(1,1,1);
		pNode->bIdentityMatrix = true;
		pNode->pMesh = new CMesh;
		pNode->pMesh->Copy( *pCGF->GetMergedMesh() );
		pNode->pParent = NULL;
		pNode->pMaterial = pCGF->GetCommonMaterial();
		pNode->nPhysicalizeFlags = 0;
		// Add node to CGF contents.
		pCompiledCGF->AddNode( pNode );
		//////////////////////////////////////////////////////////////////////////
	}
	else
	{
		// Compile meshes in nodes.
		for (int i = 0; i < pCGF->GetNodeCount(); i++)
		{
			CNodeCGF *pNodeCGF = pCGF->GetNode(i);
			if (!pNodeCGF->pMesh || pNodeCGF->bPhysicsProxy)
				continue;

			//strcpy( g_sCurrentNode,pNodeCGF->name.c_str() );

			mesh_compiler::CMeshCompiler meshCompiler;

			if (pIndexRemapping)
				meshCompiler.SetIndexRemapping( pIndexRemapping );
			if (pVertexRemapping)
				meshCompiler.SetVertexRemapping( pVertexRemapping );

      //Warning( "CLoaderCGF::MakeCompiledCGF: Stripifying geometry for file %s", pCGF->GetFilename() );

			if (!meshCompiler.Compile( *pNodeCGF->pMesh ))
			{
				m_LastError.Format("Failed to compile geometry file %s - %s",pCGF->GetFilename(), meshCompiler.GetLastError());
				delete pCompiledCGF;
				return 0;
			}
		}

		for (int i = 0; i < pCGF->GetNodeCount(); i++)
		{
			CNodeCGF *pNodeCGF = pCGF->GetNode(i);
			if (!pNodeCGF->pMesh || pNodeCGF->type != CNodeCGF::NODE_MESH)
				continue;

			pCompiledCGF->AddNode( pNodeCGF );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Compile physics proxy nodes.
	//////////////////////////////////////////////////////////////////////////
	if (pCGF->GetExportInfo()->bHavePhysicsProxy)
	{
		for (int i = 0; i < pCGF->GetNodeCount(); i++)
		{
			CNodeCGF *pNodeCGF = pCGF->GetNode(i);
			if (pNodeCGF->pMesh && pNodeCGF->bPhysicsProxy)
			{
				//strcpy( g_sCurrentNode,pNodeCGF->name.c_str() );

        //Warning( "CLoaderCGF::MakeCompiledCGF: Stripifying geometry for file %s", pCGF->GetFilename() );

				// Compile physics proxy mesh.
				mesh_compiler::CMeshCompiler meshCompiler;
				if (!meshCompiler.Compile( *pNodeCGF->pMesh,mesh_compiler::MESH_COMPILE_OPTIMIZE ))
				{
					m_LastError.Format("Failed to compile geometry in node %s in file %s - %s",pNodeCGF->name.c_str(),pCGF->GetFilename(), meshCompiler.GetLastError());
					delete pCompiledCGF;
					return 0;
				}
			}
			pCompiledCGF->AddNode(pNodeCGF);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	return pCompiledCGF;
}



//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadExportFlagsChunk( int nChunkIndex )
{
	EXPORT_FLAGS_CHUNK_DESC &chunk = *(EXPORT_FLAGS_CHUNK_DESC*)m_pChunkFile->GetChunkData(nChunkIndex);
	SwapEndian(chunk);
	if (chunk.chdr.ChunkVersion != EXPORT_FLAGS_CHUNK_DESC::VERSION)
		return false;

	CExportInfoCGF *pExportInfo = m_pCGF->GetExportInfo();
	if (chunk.flags & EXPORT_FLAGS_CHUNK_DESC::MERGE_ALL_NODES)
		pExportInfo->bMergeAllNodes = true;
	else
		pExportInfo->bMergeAllNodes = false;

	return true;
}

inline const char* stristr2(const char* szString, const char* szSubstring)
{
	int nSuperstringLength = (int)strlen(szString);
	int nSubstringLength = (int)strlen(szSubstring);

	for (int nSubstringPos = 0; nSubstringPos <= nSuperstringLength - nSubstringLength; ++nSubstringPos)
	{
		if (strnicmp(szString+nSubstringPos, szSubstring, nSubstringLength) == 0)
			return szString+nSubstringPos;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadNodeChunk( int nChunkIndex )
{
	NODE_CHUNK_DESC *nodeChunk = (NODE_CHUNK_DESC*)m_pChunkFile->GetChunkData(nChunkIndex);
	assert(nodeChunk);
	SwapEndian(*nodeChunk);

	CNodeCGF *pNodeCGF = new CNodeCGF;
	m_pCGF->AddNode( pNodeCGF );

	pNodeCGF->name = nodeChunk->name;

	// Fill node object.
	pNodeCGF->nChunkId = nodeChunk->chdr.ChunkID;
	pNodeCGF->nParentChunkId = nodeChunk->ParentID;
	pNodeCGF->nObjectChunkId = nodeChunk->ObjectID;
	pNodeCGF->pParent = 0;
	pNodeCGF->pMesh = 0;

	pNodeCGF->pos_cont_id = nodeChunk->pos_cont_id;
	pNodeCGF->rot_cont_id = nodeChunk->rot_cont_id;
	pNodeCGF->scl_cont_id = nodeChunk->scl_cont_id;

	// Copy local orientation from node chunk.
	pNodeCGF->pos = nodeChunk->pos * VERTEX_SCALE; // Scale 100 times down.
	pNodeCGF->rot = nodeChunk->rot;
	pNodeCGF->scl = nodeChunk->scl;

	pNodeCGF->bIdentityMatrix = pNodeCGF->pos.IsZero() && pNodeCGF->scl.IsEquivalent(Vec3(1,1,1)) && pNodeCGF->rot.IsIdentity();
	if (pNodeCGF->nParentChunkId > 1)
		pNodeCGF->bIdentityMatrix = false;

	pNodeCGF->pMaterial = 0;
	if (nodeChunk->MatID > 0)
	{
		pNodeCGF->pMaterial = LoadMaterialFromChunk(nodeChunk->MatID);
	}

	// Set matrix from chunk.
	float *pMat = &nodeChunk->tm[0][0];
	pNodeCGF->localTM.SetFromVectors(
		Vec3(pMat[ 0],pMat[ 1],pMat[ 2]),
		Vec3(pMat[ 4],pMat[ 5],pMat[ 6]),
		Vec3(pMat[ 8],pMat[ 9],pMat[10]),
		Vec3(pMat[12]*VERTEX_SCALE,pMat[13]*VERTEX_SCALE,pMat[14]*VERTEX_SCALE));

	if (nodeChunk->PropStrLen > 0)
	{
		pNodeCGF->properties.Format("%.*s", nodeChunk->PropStrLen, ((const char*)nodeChunk) + sizeof(*nodeChunk));
	}

	// By default node type is mesh.
	pNodeCGF->type = CNodeCGF::NODE_MESH;

	pNodeCGF->bPhysicsProxy = false;
	if (stristr2(nodeChunk->name,PHYSICS_PROXY_NODE) || stristr2(nodeChunk->name,PHYSICS_PROXY_NODE2) || stristr2(nodeChunk->name,PHYSICS_PROXY_NODE3))
	{
		pNodeCGF->type = CNodeCGF::NODE_HELPER;
		pNodeCGF->bPhysicsProxy = true;
		m_pCGF->GetExportInfo()->bHavePhysicsProxy = true;
	}
	else if (nodeChunk->name[0] == '$')
	{
		pNodeCGF->type = CNodeCGF::NODE_HELPER;
	}

	// Check if valid object node.
	if (nodeChunk->ObjectID > 0)
	{
		IChunkFile::ChunkDesc *pObjChunkDesc = m_pChunkFile->FindChunkById(nodeChunk->ObjectID);
		assert(pObjChunkDesc);
		if (!pObjChunkDesc)
			return false;
		if (pObjChunkDesc->hdr.ChunkType == ChunkType_Mesh)
		{
			if (pNodeCGF->type == CNodeCGF::NODE_HELPER)
				pNodeCGF->helperType = HP_GEOMETRY;
			if (!LoadGeomChunk( pNodeCGF,pObjChunkDesc ))
				return false;
		}
		else if (pObjChunkDesc->hdr.ChunkType == ChunkType_Helper)
		{
			pNodeCGF->type = CNodeCGF::NODE_HELPER;
			if (!LoadHelperChunk( pNodeCGF,pObjChunkDesc ))
				return false;
		}
		else if (pObjChunkDesc->hdr.ChunkType == ChunkType_Light)
		{
			pNodeCGF->type = CNodeCGF::NODE_LIGHT;
			if (!LoadLightChunk( pNodeCGF,pObjChunkDesc ))
				return false;
		}
	}
	else
	{
	//	pNodeCGF->type = CNodeCGF::NODE_HELPER;
	//	pNodeCGF->helperType = HP_POINT;
	//	pNodeCGF->helperSize = Vec3(0.01f,0.01f,0.01f);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadFaceMapChunk( int nChunkIndex )
{
	FACEMAP_CHUNK_DESC *mapChunk = (FACEMAP_CHUNK_DESC*)m_pChunkFile->GetChunkData(nChunkIndex);
	assert(mapChunk);
	SwapEndian(*mapChunk);

	CNodeCGF *pNodeCGF; int i;
	for(i=m_pCGF->GetNodeCount()-1; i>=0 && strcmp(m_pCGF->GetNode(i)->name,mapChunk->nodeName);i--);
	if (i<0 || !(pNodeCGF = m_pCGF->GetNode(i))->pMesh)
		return false;

	int nType,nCount,nElemSize;
	void *pStreamData;
	LoadStreamDataChunk( mapChunk->StreamID, pStreamData, nType,nCount,nElemSize ) ;
	pNodeCGF->bHasFaceMap = true;
	pNodeCGF->mapFaceToFace0.resize(nCount);
	memcpy(&pNodeCGF->mapFaceToFace0[0], pStreamData, nCount*nElemSize);
	SwapEndian(&pNodeCGF->mapFaceToFace0[0], nCount);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadHelperChunk( CNodeCGF *pNode,IChunkFile::ChunkDesc *pChunkDesc )
{
	HELPER_CHUNK_DESC &chunk = *(HELPER_CHUNK_DESC*)pChunkDesc->data;
	SwapEndian(chunk);
	assert(&chunk);

	// Fill node object.
	pNode->helperType = chunk.type;
	pNode->helperSize = chunk.size;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadLightChunk( CNodeCGF *pNode,IChunkFile::ChunkDesc *pChunkDesc )
{
	LIGHT_CHUNK_DESC &chunk = *(LIGHT_CHUNK_DESC*)pChunkDesc->data;
	assert(&chunk);

	//@TODO
	// Not implemented.

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CLoaderCGF::ProcessNodes()
{
	int i;
	//////////////////////////////////////////////////////////////////////////
	// Bind Nodes parents.
	//////////////////////////////////////////////////////////////////////////
	for (i = 0; i < m_pCGF->GetNodeCount(); i++)
	{
		if (m_pCGF->GetNode(i)->nParentChunkId > 0)
		{
			for (int j = 0; j < m_pCGF->GetNodeCount(); j++)
			{
				if (m_pCGF->GetNode(i)->nParentChunkId == m_pCGF->GetNode(j)->nChunkId)
				{
					m_pCGF->GetNode(i)->pParent = m_pCGF->GetNode(j);
					break;
				}
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Calculate Node world matrices.
	//////////////////////////////////////////////////////////////////////////
	for (i = 0; i < m_pCGF->GetNodeCount(); i++)
	{
		CNodeCGF *pNode = m_pCGF->GetNode(i);
		Matrix34 tm = pNode->localTM;
		for(CNodeCGF *pCurNode = pNode->pParent; pCurNode; pCurNode = pCurNode->pParent)
		{
			tm = pCurNode->localTM * tm;
		}
		pNode->worldTM = tm;
		if (pNode->pParent)
		{
			pNode->bIdentityMatrix = false;
		}

		if (pNode->pMesh)
			CCGFNodeMerger().SetupMesh( *pNode->pMesh,pNode->pMaterial,usedMaterialIds );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadGeomChunk( CNodeCGF *pNode,IChunkFile::ChunkDesc *pChunkDesc )
{
	// First check if this geometry chunk was already loaded by some node.
	int nNumNodes = m_pCGF->GetNodeCount();
	for (int i = 0; i < nNumNodes; i++)
	{
		CNodeCGF *pOldNode = m_pCGF->GetNode(i);
		if (pOldNode != pNode && pOldNode->nObjectChunkId == pChunkDesc->hdr.ChunkID)
		{
			pNode->pMesh = pOldNode->pMesh;
			pNode->pSharedMesh = pOldNode;
			return true;
		}
	}

	assert( pChunkDesc && pChunkDesc->hdr.ChunkType == ChunkType_Mesh );
	int nChunkVersion = pChunkDesc->hdr.ChunkVersion;

	if (nChunkVersion == MESH_CHUNK_DESC_0800::VERSION)
	{
		m_pCGF->GetExportInfo()->bCompiledCGF = true;
		return LoadCompiledMeshChunk( pNode,pChunkDesc );
	}
	else if (nChunkVersion == MESH_CHUNK_DESC_0744::VERSION)
	{
		m_pCGF->GetExportInfo()->bCompiledCGF = false;

		CMesh *pMesh = new CMesh;
		CMesh& mesh = *pMesh;

		AABB bbox;
		bbox.Reset();

		byte* pMeshChunkData = (byte*)pChunkDesc->data;

		MESH_CHUNK_DESC_0744 *chunk;
		StepData(chunk, pMeshChunkData);

		int nVerts = chunk->nVerts;
		int nFaces = chunk->nFaces;
		int nTVerts = chunk->nTVerts;

		// Set pointers to internal chunk structure.
		CryVertex *pVertices;
		StepData(pVertices, pMeshChunkData, NULL, nVerts);

		CryFace *pFaces;
		StepData(pFaces, pMeshChunkData, NULL, nFaces);

		CryUV *pUVs = NULL;
		CryTexFace *pTexFaces = NULL;
		if (nTVerts)
		{
			StepData(pUVs, pMeshChunkData, NULL, nTVerts);
			StepData(pTexFaces, pMeshChunkData, NULL, nFaces);
		}


		//---------------------------------------------------------

		if (chunk->flags1 & MESH_CHUNK_DESC_0744::FLAG1_BONE_INFO)
		{
			m_arrLinksTmp.resize( nVerts );

			for (uint32 i=0; i<(uint32)nVerts; i++) 
			{
				CryVertexBinding& rLinks_target = m_arrLinksTmp[i];

				uint32 numLinks = *StepData<uint32>(pMeshChunkData);
				assert(numLinks);	assert(numLinks<32);
				if( (numLinks==0) || (numLinks>32)) { 
					m_LastError.Format( "CryGeometryInfo::Load: Number of links for vertex is invalid: %u", numLinks);
					return 0;
				}

				rLinks_target.resize(numLinks);

				CryLink* rLinks_source;
				StepData(rLinks_source, pMeshChunkData, NULL, numLinks);
				for (uint32 l=0; l<numLinks; l++ )
				{	
					rLinks_target[l].BoneID		=	rLinks_source[l].BoneID;
					rLinks_target[l].offset		=	rLinks_source[l].offset*0.01f;
					rLinks_target[l].Blending	=	rLinks_source[l].Blending;
				}

				//optimize links
				rLinks_target.sortByBlendingDescending();
				if(rLinks_target.size() > 4)
					rLinks_target.resize(4);
				rLinks_target.normalizeBlendWeights();
			}

		}

		//---------------------------------------------------------

		CryIRGB *pVColors = NULL;
		unsigned char *pVertexAlpha = NULL;
		if (chunk->flags2 & MESH_CHUNK_DESC_0744::FLAG2_HAS_VERTEX_COLOR)
		{
			m_bHaveVertexColors = true;
			StepData(pVColors, pMeshChunkData, NULL, nVerts);
		}
		if (chunk->flags2 & MESH_CHUNK_DESC_0744::FLAG2_HAS_VERTEX_ALPHA)
		{
			m_bHaveVertexColors = true;
			StepData(pVertexAlpha, pMeshChunkData, NULL, nVerts);
		}


		//////////////////////////////////////////////////////////////////////////
		// Validate texture ids chunk.
		if (nTVerts != 0)
		{
			for( int c=0; c < nFaces && pTexFaces; c++)
			{
				if (pTexFaces[c].t0 > nTVerts ||
					pTexFaces[c].t1 > nTVerts ||
					pTexFaces[c].t2 > nTVerts)
				{
					m_LastError.Format( "Face texture index is greater then number of texture coordinates (%s)",m_filename.c_str() );
					delete pMesh;
					return false;
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Copy Tex Coords.
		//////////////////////////////////////////////////////////////////////////
		if (nTVerts != 0)
		{ // realloc texcoords
			mesh.SetTexCoordsCount( nTVerts );

			for (int c = 0; c < nTVerts; c++)
			{
				float u = pUVs[c].u;
				float v = pUVs[c].v; 
#if !defined(PS3)
				if(!_finite(u) || !_finite(v))
				{
					Warning("Illegal (NAN) texture coordinate %d (%s), Fix the 3d Model",c,m_filename.c_str());
					u=0.0f;v=0.0f;
				}
#endif
				mesh.m_pTexCoord[c].s = u;	
				mesh.m_pTexCoord[c].t = 1.0f - v; // flip tex coords (since it was flipped in max?)
			}
		}
		else
		{
			// Allocate single texture coord.
			mesh.SetTexCoordsCount( 1 );
			mesh.m_pTexCoord[0].s = 0;	
			mesh.m_pTexCoord[0].t = 0;
		}

		//////////////////////////////////////////////////////////////////////////
		// Copy Positions and Normals.
		//////////////////////////////////////////////////////////////////////////
		{ // realloc verts and normals
			mesh.SetVertexCount(nVerts);
			if (pVColors || pVertexAlpha)
				mesh.ReallocStream( CMesh::COLORS_0,nVerts );

			for( int c = 0; c < nVerts; c++)
			{
				CryVertex &cv = pVertices[c];
#if !defined(PS3)
				if(!_finite(cv.p.x) || !_finite(cv.p.y) || !_finite(cv.p.z))
				{
					Warning("Illegal (NAN) vertex position %d (%s), Fix the 3d Model",c,m_filename.c_str());
					cv.p.Set(0,0,0);
				}
				if(!_finite(cv.n.x) || !_finite(cv.n.y) || !_finite(cv.n.z))
				{
					Warning("Illegal (NAN) vertex normal %d (%s), Fix the 3d Model",c,m_filename.c_str());
					cv.n.Set(0,0,1);
				}
#endif
				mesh.m_pPositions[c] = cv.p * VERTEX_SCALE; // Scale positions from CGF 100 times down.
				mesh.m_pNorms[c] = cv.n; // Normilize normal vector from CGF.
				float fNormalLength = cv.n.GetLengthSquared();
				if (fNormalLength == 0)
					mesh.m_pNorms[c].Set(0,0,1);
				else if (fNormalLength != 1)
					mesh.m_pNorms[c] = cv.n.GetNormalized(); // Normilize normal vector from CGF.

				bbox.Add( mesh.m_pPositions[c] );

				if (mesh.m_pColor0)
				{
					mesh.m_pColor0[c].r = 0xFF;
					mesh.m_pColor0[c].g = 0xFF;
					mesh.m_pColor0[c].b = 0xFF;
					mesh.m_pColor0[c].a = 0xFF;
					if (pVColors)
					{
						mesh.m_pColor0[c].r = pVColors[c].r;
						mesh.m_pColor0[c].g = pVColors[c].g;
						mesh.m_pColor0[c].b = pVColors[c].b;
					}
					if (pVertexAlpha)
						mesh.m_pColor0[c].a = pVertexAlpha[c];
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Copy Faces.
		//////////////////////////////////////////////////////////////////////////
		{ // realloc faces
			mesh.SetFacesCount( nFaces );

			bool bWarningOnce = true;
			for (int c = 0; c < nFaces; c++)
			{
				int nFace = c;

				SMeshFace *pFace = &mesh.m_pFaces[nFace];
				CryFace &cf = pFaces[c];

				memset( pFace,0,sizeof(SMeshFace) );

				// check vertex id
				if (cf.v0 >= nVerts || cf.v1 >= nVerts || cf.v2 >= nVerts)
				{
					m_LastError.Format("Bad vertex index in the geometry face %d (%s), Fix the 3d Model",c,m_filename.c_str() );
					delete pMesh;
					return false;
				}

				pFace->v[0] = cf.v0;
				pFace->v[1] = cf.v1;
				pFace->v[2] = cf.v2;

				if (pTexFaces)
				{
					pFace->t[0] = pTexFaces[c].t0;
					pFace->t[1] = pTexFaces[c].t1;
					pFace->t[2] = pTexFaces[c].t2;
				}
				else
				{
					pFace->t[0] = 0;
					pFace->t[1] = 0;
					pFace->t[2] = 0;
				}

				int nMatID = cf.MatID;
				if (nMatID >= MAX_SUB_MATERIALS || nMatID < 0)
				{
					if (bWarningOnce)
					{
						Warning( "Face %d has illegal MatId=%d, Max of %d MatId supported (%s)",c,nMatID,MAX_SUB_MATERIALS,m_filename.c_str() );
						bWarningOnce = false;
					}
					nMatID = 0;
				}

				// Remap new used material ID to index of chunk id.
				if (!MatIdToSubset[nMatID])
				{
					MatIdToSubset[nMatID] = 1 + nLastChunkId++;
					// Order of material ids in usedMaterialIds correspond to the indices of chunks.
					usedMaterialIds.push_back(nMatID);
				}
				pFace->nSubset = MatIdToSubset[nMatID]-1;
			}
		}

		pMesh->m_bbox = bbox;
		// Assign mesh to node.
		pNode->pMesh = pMesh;
	}
	else
	{
		// Unknown Mesh Chunk version.
		m_LastError.Format( "Bad Mesh Chunk version %s",m_filename.c_str() );
		return false;
	}

	return true;
}

template<class T>
bool CLoaderCGF::LoadStreamChunk(	CMesh& mesh, MESH_CHUNK_DESC_0800 &chunk, ECgfStreamType Type, CMesh::EStream MStream, T*& Data, int nCount)
{
	if (chunk.nStreamChunkID[Type] > 0)
	{
		void* pStreamData;
		int nStreamType;
		int nStreamCount;
		int nElemSize;

		if (!LoadStreamDataChunk( chunk.nStreamChunkID[Type], pStreamData, nStreamType, nStreamCount, nElemSize ))
			return false;
		if (nStreamType != Type || nElemSize != sizeof(T) || nStreamCount != nCount)
			return false;

		void* pStream;
		mesh.GetStreamInfo( MStream, pStream, nElemSize );
		if (pStream != Data || nElemSize != sizeof(T))
			return false;

		mesh.ReallocStream( MStream, nStreamCount );
		memcpy(Data, pStreamData, nCount*nElemSize);
		SwapEndian(Data, nStreamCount);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadCompiledMeshChunk( CNodeCGF *pNode,IChunkFile::ChunkDesc *pChunkDesc )
{
	assert( pChunkDesc && pChunkDesc->hdr.ChunkType == ChunkType_Mesh );

	MESH_CHUNK_DESC_0800 &chunk = *(MESH_CHUNK_DESC_0800*)pChunkDesc->data;
	if (chunk.chdr.ChunkVersion != MESH_CHUNK_DESC_0800::VERSION)
		SwapEndian(chunk);
	if (pChunkDesc->hdr.ChunkVersion != MESH_CHUNK_DESC_0800::VERSION)
		return false;

	CMesh *pMesh = new CMesh;
	CMesh& mesh = *pMesh;

	mesh.SetVertexCount( chunk.nVerts );
	mesh.SetIndexCount( chunk.nIndices );
  
  if(chunk.nStreamChunkID[CGF_STREAM_TEXCOORDS]>0)
    mesh.SetTexCoordsCount( chunk.nVerts );

	if (chunk.nSubsets > 0 && chunk.nSubsetsChunkId > 0)
	{
		IChunkFile::ChunkDesc *pChunkDesc = m_pChunkFile->FindChunkById(chunk.nSubsetsChunkId);
		if (!pChunkDesc || pChunkDesc->hdr.ChunkType != ChunkType_MeshSubsets)
		{
			m_LastError.Format( "MeshSubsets Chunk not find in CGF file %s",m_filename.c_str() );
			return false;
		}
		if (!LoadMeshSubsetsChunk( mesh,pChunkDesc ))
			return false;
	}

	void* pStreamData;
	int nStreamType;
	int nStreamCount;
	int nElemSize;

	//////////////////////////////////////////////////////////////////////////
	// Read position stream.
	//////////////////////////////////////////////////////////////////////////
	LoadStreamChunk(mesh, chunk, CGF_STREAM_POSITIONS, CMesh::POSITIONS, mesh.m_pPositions, mesh.GetVertexCount());

	//////////////////////////////////////////////////////////////////////////
	// Read normals stream.
	//////////////////////////////////////////////////////////////////////////
	LoadStreamChunk(mesh, chunk, CGF_STREAM_NORMALS, CMesh::NORMALS, mesh.m_pNorms, mesh.GetVertexCount());

	//////////////////////////////////////////////////////////////////////////
	// Read Texture coordinates stream.
	//////////////////////////////////////////////////////////////////////////
	LoadStreamChunk(mesh, chunk, CGF_STREAM_TEXCOORDS, CMesh::TEXCOORDS, mesh.m_pTexCoord, mesh.GetTexCoordsCount());

	//////////////////////////////////////////////////////////////////////////
	// Read colors stream.
	//////////////////////////////////////////////////////////////////////////
	if (chunk.nStreamChunkID[CGF_STREAM_COLORS] > 0)
	{
		if (!LoadStreamDataChunk( chunk.nStreamChunkID[CGF_STREAM_COLORS],pStreamData,nStreamType,nStreamCount,nElemSize ))
			return false;
		if (nStreamType != CGF_STREAM_COLORS || nStreamCount != mesh.GetVertexCount())
			return false;
		mesh.ReallocStream( CMesh::COLORS_0,nStreamCount );
		if (nElemSize == sizeof(SMeshColor))
		{
			memcpy( mesh.m_pColor0,pStreamData,nStreamCount*nElemSize );
			SwapEndian(mesh.m_pColor0, nStreamCount);
		}
		else if (nElemSize == sizeof(CryIRGB))
		{
			for (int i = 0; i < nStreamCount; i++)
			{
				mesh.m_pColor0[i].r = ((CryIRGB*)pStreamData)[i].r;
				mesh.m_pColor0[i].g = ((CryIRGB*)pStreamData)[i].g;
				mesh.m_pColor0[i].b = ((CryIRGB*)pStreamData)[i].b;
			}
		}
	}

	if (chunk.nStreamChunkID[CGF_STREAM_COLORS2] > 0)
	{
		if (!LoadStreamDataChunk( chunk.nStreamChunkID[CGF_STREAM_COLORS2],pStreamData,nStreamType,nStreamCount,nElemSize ))
			return false;
		if (nStreamType != CGF_STREAM_COLORS2 || nStreamCount != mesh.GetVertexCount())
			return false;
		mesh.ReallocStream( CMesh::COLORS_1,nStreamCount );
		if (nElemSize == sizeof(SMeshColor))
		{
			memcpy( mesh.m_pColor1,pStreamData,nStreamCount*nElemSize );
			SwapEndian(mesh.m_pColor1, nStreamCount);
		}
		else if (nElemSize == sizeof(CryIRGB))
		{
			for (int i = 0; i < nStreamCount; i++)
			{
				mesh.m_pColor1[i].r = ((CryIRGB*)pStreamData)[i].r;
				mesh.m_pColor1[i].g = ((CryIRGB*)pStreamData)[i].g;
				mesh.m_pColor1[i].b = ((CryIRGB*)pStreamData)[i].b;
			}
		}
	}

  if (chunk.nStreamChunkID[CGF_STREAM_VERT_MATS] > 0)
  {
    if (!LoadStreamDataChunk( chunk.nStreamChunkID[CGF_STREAM_VERT_MATS],pStreamData,nStreamType,nStreamCount,nElemSize ))
      return false;
    if (nStreamType != CGF_STREAM_VERT_MATS || nStreamCount != mesh.GetVertexCount())
      return false;
    mesh.ReallocStream( CMesh::VERT_MATS,nStreamCount );
    assert(nElemSize == sizeof(int));
    if (nElemSize == sizeof(int))
    {
      memcpy( mesh.m_pVertMats,pStreamData,nStreamCount*nElemSize );
      SwapEndian(mesh.m_pVertMats, nStreamCount);
    }
  }

	if (chunk.nStreamChunkID[CGF_STREAM_TANGENTS] > 0)
	{
		if (!LoadStreamDataChunk( chunk.nStreamChunkID[CGF_STREAM_TANGENTS],pStreamData,nStreamType,nStreamCount,nElemSize ))
			return false;
		if (nStreamType != CGF_STREAM_TANGENTS || nStreamCount != mesh.GetVertexCount())
			return false;
		mesh.ReallocStream( CMesh::TANGENTS,nStreamCount );
		if (nElemSize == sizeof(SMeshTangents))
		{
			memcpy( mesh.m_pTangents,pStreamData,nStreamCount*nElemSize );
			SwapEndian((int16f *)mesh.m_pTangents, nStreamCount*2*4);
		}
		else
		{
			assert (false);
		}
	}

	LoadStreamChunk(mesh, chunk, CGF_STREAM_INDICES, CMesh::INDICES, mesh.m_pIndices, mesh.GetIndexCount());
	//LoadStreamChunk(mesh, chunk, CGF_STREAM_TANGENTS, CMesh::TANGENTS, Rmesh.m_pTangents, mesh.GetVertexCount());
	LoadStreamChunk(mesh, chunk, CGF_STREAM_BONEMAPPING, CMesh::BONEMAPPING, mesh.m_pBoneMapping, mesh.GetVertexCount());
	LoadStreamChunk(mesh, chunk, CGF_STREAM_SHAPEDEFORMATION, CMesh::SHAPEDEFORMATION, mesh.m_pShapeDeformation, mesh.GetVertexCount());

	if(mesh.m_pSHInfo)
		LoadStreamChunk(mesh, chunk, CGF_STREAM_SHCOEFFS, CMesh::SHCOEFFS, mesh.m_pSHInfo->pSHCoeffs, mesh.GetVertexCount());

	for (int nPhysGeomType = 0; nPhysGeomType < 4; nPhysGeomType++)
	{
		if (chunk.nPhysicsDataChunkId[nPhysGeomType] > 0)
		{
			LoadPhysicsDataChunk( pNode,nPhysGeomType,chunk.nPhysicsDataChunkId[nPhysGeomType] );
		}
	}

	pMesh->m_bbox = AABB(chunk.bboxMin,chunk.bboxMax);
	pNode->pMesh = pMesh;

	// Map loaded geometry chunk id to the node.

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadMeshSubsetsChunk( CMesh &mesh,IChunkFile::ChunkDesc *pChunkDesc )
{
	byte* pCurDataLoc = (byte*)pChunkDesc->data;
	MESH_SUBSETS_CHUNK_DESC_0800 &chunk = *StepData<MESH_SUBSETS_CHUNK_DESC_0800>(pCurDataLoc);
	if (chunk.chdr.ChunkType != ChunkType_MeshSubsets)
		return false;
	if (chunk.chdr.ChunkVersion != MESH_SUBSETS_CHUNK_DESC_0800::VERSION)
		return false;

	//check if decompression matrices are stored here too
	const bool cbLoadDecompressionMatrices = (chunk.nFlags & MESH_SUBSETS_CHUNK_DESC_0800::SH_HAS_DECOMPR_MAT);
	const bool cbBoneIDs = (chunk.nFlags & MESH_SUBSETS_CHUNK_DESC_0800::BONEINDICES)!=0;


	for (int i = 0; i < chunk.nCount; i++)
	{
		MESH_SUBSETS_CHUNK_DESC_0800::MeshSubset& meshSubset = *StepData<MESH_SUBSETS_CHUNK_DESC_0800::MeshSubset>(pCurDataLoc);
		SMeshSubset subset;
		subset.nFirstIndexId	= meshSubset.nFirstIndexId;
		subset.nNumIndices		= meshSubset.nNumIndices;
		subset.nFirstVertId		= meshSubset.nFirstVertId;
		subset.nNumVerts			= meshSubset.nNumVerts;
		subset.nMatID					= meshSubset.nMatID;
		subset.fRadius				= meshSubset.fRadius;
		subset.vCenter				= meshSubset.vCenter;
		mesh.m_subsets.push_back(subset);
	}

	//------------------------------------------------------------------
	if(cbLoadDecompressionMatrices)
	{
		if(!mesh.m_pSHInfo)
			mesh.m_pSHInfo = new SSHInfo;

		mesh.m_pSHInfo->nDecompressionCount = chunk.nCount;
		mesh.m_pSHInfo->pDecompressions = new SSHDecompressionMat[chunk.nCount];	assert(mesh.m_pSHInfo->pDecompressions);
		for (int i = 0; i < chunk.nCount; i++)
		{
			//it starts right after the mesh subset struct
			SSHDecompressionMat& decomprMat = *StepData<SSHDecompressionMat>(pCurDataLoc);
			mesh.m_pSHInfo->pDecompressions[i] = decomprMat;
		}
	}

	//------------------------------------------------------------------
	if (cbBoneIDs)
	{
		for (int i = 0; i < chunk.nCount; i++)
		{
			MESH_SUBSETS_CHUNK_DESC_0800::MeshBoneIDs& meshSubset = *StepData<MESH_SUBSETS_CHUNK_DESC_0800::MeshBoneIDs>(pCurDataLoc);
			mesh.m_subsets[i].m_arrGlobalBonesPerSubset.resize(meshSubset.numBoneIDs);
			for (uint32 b=0; b<meshSubset.numBoneIDs; b++)
				mesh.m_subsets[i].m_arrGlobalBonesPerSubset[b]=meshSubset.arrBoneIDs[b];
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadStreamDataChunk( int nChunkId,void* &pStreamData,int &nStreamType,int &nCount,int &nElemSize )
{
	IChunkFile::ChunkDesc *pChunkDesc = m_pChunkFile->FindChunkById(nChunkId);
	if (!pChunkDesc)
		return false;

	STREAM_DATA_CHUNK_DESC_0800 &chunk = *(STREAM_DATA_CHUNK_DESC_0800*)pChunkDesc->data;
	SwapEndian(chunk);
	if (chunk.chdr.ChunkType != ChunkType_DataStream)
		return false;
	if (chunk.chdr.ChunkVersion != STREAM_DATA_CHUNK_DESC_0800::VERSION)
		return false;

	nStreamType = chunk.nStreamType;
	nCount = chunk.nCount;
	nElemSize = chunk.nElementSize;

	// Get stream pointer from the end of the chunk.
	pStreamData = (char*)pChunkDesc->data + sizeof(chunk);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCGF::LoadPhysicsDataChunk( CNodeCGF *pNode,int nPhysGeomType,int nChunkId )
{
	IChunkFile::ChunkDesc *pChunkDesc = m_pChunkFile->FindChunkById(nChunkId);
	if (!pChunkDesc)
		return false;

	MESH_PHYSICS_DATA_CHUNK_DESC_0800 &chunk = *(MESH_PHYSICS_DATA_CHUNK_DESC_0800*)pChunkDesc->data;
	SwapEndian(chunk);
	if (chunk.chdr.ChunkType != ChunkType_MeshPhysicsData)
		return false;
	if (chunk.chdr.ChunkVersion != MESH_PHYSICS_DATA_CHUNK_DESC_0800::VERSION)
		return false;

	assert( nPhysGeomType >= 0 && nPhysGeomType < 4 );

	int nDataSize = chunk.nDataSize;
	pNode->physicalGeomData[nPhysGeomType].resize( nDataSize );
	void *pTrgPtr = &(pNode->physicalGeomData[nPhysGeomType][0]);

	// Get physics data from the end of the chunk.
	void *pPhysData = (char*)pChunkDesc->data + sizeof(chunk);
	memcpy( pTrgPtr,pPhysData,nDataSize );

	return true;
}

SFoliageInfoCGF::~SFoliageInfoCGF()
{ 
	if (pSpines) 
	{
		for(int i=1;i<nSpines;i++) // spines 1..n-1 use the same buffer, so make sure they don't delete it
			pSpines[i].pVtx=0, pSpines[i].pSegDim=0;
		delete[] pSpines; 
	}
	if (pBoneMapping) delete[] pBoneMapping; 
}

//////////////////////////////////////////////////////////////////////////
CMaterialCGF* CLoaderCGF::LoadMaterialFromChunk( int nChunkId )
{
	for (int i = 0; i < m_pCGF->GetMaterialCount(); i++)
	{
		if (m_pCGF->GetMaterial(i)->nChunkId == nChunkId)
			return m_pCGF->GetMaterial(i);
	}

	IChunkFile::ChunkDesc *pChunkDesc = m_pChunkFile->FindChunkById(nChunkId);
	if (!pChunkDesc)
		return false;

	if (pChunkDesc->hdr.ChunkType == ChunkType_MtlName)
	{
		return LoadMaterialNameChunk( pChunkDesc );
	}
	else if (pChunkDesc->hdr.ChunkType == ChunkType_Mtl)
	{
		return LoadOldMaterialChunk( pChunkDesc );
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
CMaterialCGF* CLoaderCGF::LoadMaterialNameChunk( IChunkFile::ChunkDesc *pChunkDesc )
{
	size_t size=pChunkDesc->size;

	if(size>sizeof(MTL_NAME_CHUNK_DESC_0800))
	{
		m_LastError.Format( "Illegal material name chunk size %s (%d should be %d)",m_filename.c_str(),size,sizeof(MTL_NAME_CHUNK_DESC_0800));
		return 0;
	}
	MTL_NAME_CHUNK_DESC_0800 chunk;

	// the following code ensures that we can load only chunk versions (new data is 0)
	memset(&chunk,0,sizeof(MTL_NAME_CHUNK_DESC_0800));
	memcpy(&chunk,pChunkDesc->data,size);
	SwapEndian(chunk);

	if(chunk.chdr.ChunkVersion != MTL_NAME_CHUNK_DESC_0800::VERSION)
	{
		m_LastError.Format( "Illegal material name chunk %s",m_filename.c_str() );
		return 0;
	}

	CMaterialCGF *pMtlCGF = new CMaterialCGF;
	pMtlCGF->nChunkId = chunk.chdr.ChunkID;
	m_pCGF->AddMaterial( pMtlCGF );

	pMtlCGF->nFlags = chunk.nFlags;
	for (int i = 0; i < (int)strlen(chunk.name); i++)
	{
		if (chunk.name[i] == '\\')
			chunk.name[i] = '/';
	}
	pMtlCGF->name = chunk.name;
	pMtlCGF->nPhysicalizeType = chunk.nPhysicalizeType;
	pMtlCGF->shOpacity = chunk.sh_opacity;

	if (chunk.nSubMaterials > 0 && chunk.nSubMaterials < 256)
	{
		for (int i = 0; i < chunk.nSubMaterials; i++)
		{
			CMaterialCGF *pMtl = LoadMaterialFromChunk( chunk.nSubMatChunkId[i] );
			assert(pMtl);
			if (!pMtl)
			{
				Warning("Failed to load Sub-material from file %s",m_filename.c_str());
				return 0;
			}
			pMtlCGF->subMaterials.push_back(pMtl);
		}
	}

	return pMtlCGF;
}

AUTO_TYPE_INFO(MtlTypes)

//////////////////////////////////////////////////////////////////////////
CMaterialCGF* CLoaderCGF::LoadOldMaterialChunk( IChunkFile::ChunkDesc *pChunkDesc )
{
	const CHUNK_HEADER &hdr = pChunkDesc->hdr;

	if (hdr.ChunkVersion == MTL_CHUNK_DESC_0746::VERSION)
	{
		m_bOldMaterials = true;
		MTL_CHUNK_DESC_0746 &chunk = *(MTL_CHUNK_DESC_0746*)pChunkDesc->data;

		if (SwapEndianValue(chunk.MtlType) == MTL_STANDARD)
		{
			SwapEndian(chunk);

			CMaterialCGF *pMtlCGF = new CMaterialCGF;
			m_pCGF->AddMaterial( pMtlCGF );

			pMtlCGF->bOldMaterial = true;
			pMtlCGF->nChunkId = chunk.chdr.ChunkID;
			pMtlCGF->pMatEntity = new MAT_ENTITY;
			pMtlCGF->name = chunk.name;

			MAT_ENTITY &me = *pMtlCGF->pMatEntity;
			memset(&me, 0, sizeof(MAT_ENTITY));
			me.opacity = 1.0f;
			me.alpharef = 0;
			me.m_New = 2;
			strcpy(me.name, chunk.name);
			me.IsStdMat = true;
			me.col_d = chunk.col_d;
			me.col_a = chunk.col_a;
			me.col_s = chunk.col_s;

			me.specLevel = chunk.specLevel;
			me.specShininess = chunk.specShininess*100;
			me.opacity = chunk.opacity;
			me.selfIllum = chunk.selfIllum;
			me.flags = chunk.flags;
			if (me.flags & MAT_ENTITY::MTLFLAG_CRYSHADER)
				me.alpharef = chunk.alphaTest;
			me.Dyn_Bounce = chunk.Dyn_Bounce;
			me.Dyn_StaticFriction = chunk.Dyn_StaticFriction;
			me.Dyn_SlidingFriction = chunk.Dyn_SlidingFriction;
			me.map_a = chunk.tex_a;
			me.map_d = chunk.tex_d;
			me.map_o = chunk.tex_o;
			me.map_b = chunk.tex_b;
			me.map_s = chunk.tex_s;
			me.map_g = chunk.tex_g;
			me.map_detail = chunk.tex_fl;
			me.map_e = chunk.tex_rl;
			me.map_subsurf = chunk.tex_subsurf;
			me.map_displ = chunk.tex_det;

			me.nChildren = chunk.nChildren;

			if (me.flags & MAT_ENTITY::MTLFLAG_CRYSHADER)
			{
				if (me.flags & MAT_ENTITY::MTLFLAG_PHYSICALIZE)
					pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_DEFAULT;
				else
					pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
			}
			else
			{
				if (me.Dyn_Bounce == 1.0f)
					pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_DEFAULT;
				else
					pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
			}
			if (strstr(me.name,"mat_leaves") != 0)
				pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_NO_COLLIDE;
			if (strstr(me.name,"mat_obstruct") != 0)
				pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_OBSTRUCT;

			return pMtlCGF;
		}
	}
	else if (hdr.ChunkVersion == MTL_CHUNK_DESC_0745::VERSION)
	{
		m_bOldMaterials = true;
		MTL_CHUNK_DESC_0745 &chunk = *(MTL_CHUNK_DESC_0745*)pChunkDesc->data;

		if (SwapEndianValue(chunk.MtlType) == MTL_STANDARD)
		{
			SwapEndian(chunk);
			CMaterialCGF *pMtlCGF = new CMaterialCGF;
			m_pCGF->AddMaterial( pMtlCGF );

			pMtlCGF->bOldMaterial = true;
			pMtlCGF->nChunkId = chunk.chdr.ChunkID;
			pMtlCGF->pMatEntity = new MAT_ENTITY;
			pMtlCGF->name = chunk.name;

			MAT_ENTITY &me = *pMtlCGF->pMatEntity;
			memset(&me, 0, sizeof(MAT_ENTITY));
			me.opacity = 1.0f;
			me.alpharef = 0;
			me.m_New = 2;
			strcpy(me.name, chunk.name);

			me.IsStdMat = true;
			me.col_d = chunk.col_d;
			me.col_a = chunk.col_a;
			me.col_s = chunk.col_s;

			me.specLevel = chunk.specLevel;
			me.specShininess = chunk.specShininess*100;
			me.opacity = chunk.opacity;
			me.selfIllum = chunk.selfIllum;
			me.flags = chunk.flags;
			me.Dyn_Bounce = chunk.Dyn_Bounce;
			me.Dyn_StaticFriction = chunk.Dyn_StaticFriction;
			me.Dyn_SlidingFriction = chunk.Dyn_SlidingFriction;
			me.map_a = chunk.tex_a;
			me.map_d = chunk.tex_d;
			me.map_o = chunk.tex_o;
			me.map_b = chunk.tex_b;
			me.map_s = chunk.tex_s;
			me.map_g = chunk.tex_g;
			me.map_e = chunk.tex_rl;
			me.map_subsurf = chunk.tex_subsurf;
			me.map_displ = chunk.tex_det;

			me.nChildren = chunk.nChildren;

			if (me.flags & MAT_ENTITY::MTLFLAG_CRYSHADER)
			{
				if (me.flags & MAT_ENTITY::MTLFLAG_PHYSICALIZE)
					pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_DEFAULT;
				else
					pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
			}
			else
			{
				if (me.Dyn_Bounce == 1.0f)
					pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_DEFAULT;
				else
					pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_NONE;
			}
			if (strstr(me.name,"mat_leaves") != 0)
				pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_NO_COLLIDE;
			if (strstr(me.name,"mat_obstruct") != 0)
				pMtlCGF->nPhysicalizeType = PHYS_GEOM_TYPE_OBSTRUCT;

			return pMtlCGF;
		}
	}
	else
	{
		// Cannot be other version.
		assert(0);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CLoaderCGF::ProcessMaterials()
{
	if (m_bOldMaterials)
	{
		CMaterialCGF *pMultiMtl = NULL;
		int nMtls = m_pCGF->GetMaterialCount();
		if (nMtls == 1)
		{
			pMultiMtl = m_pCGF->GetMaterial(0);
			m_pCGF->SetCommonMaterial( pMultiMtl );
		}
		if (nMtls > 1)
		{
			pMultiMtl = new CMaterialCGF;
			pMultiMtl->bOldMaterial = true;
			for (int i = 0; i < nMtls; i++)
			{
				pMultiMtl->subMaterials.push_back( m_pCGF->GetMaterial(i) );
			}
			m_pCGF->SetCommonMaterial( pMultiMtl );
		}
		bool bMergeAll = m_pCGF->GetExportInfo()->bMergeAllNodes;
		// Assign common material to each node.
		for (int i = 0; i < m_pCGF->GetNodeCount(); i++)
		{
			CNodeCGF *pNode = m_pCGF->GetNode(i);
			if (pNode && pNode->pMesh && (!pNode->pMaterial || bMergeAll))
			{
				pNode->pMaterial = pMultiMtl;
			}
		}
	}
	else
	{
		if (m_pCGF->GetMaterialCount() > 0)
		{
			m_pCGF->SetCommonMaterial( m_pCGF->GetMaterial(0) );
		}
	}
}

void CLoaderCGF::Warning(const char* szFormat, ...)
{
	if (m_pListener)
	{
		char szBuffer[1024];
		va_list args;
		va_start(args, szFormat);
		vsprintf_s(szBuffer, szFormat, args);
		m_pListener->Warning(szBuffer);
		va_end(args);
	}
}
