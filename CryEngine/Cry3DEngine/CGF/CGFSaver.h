////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CGFSaver.h
//  Version:     v1.00
//  Created:     7/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CGFSaver_h__
#define __CGFSaver_h__
#pragma once

#include "../MeshCompiler/MeshCompiler.h"
#include "ChunkFile.h"
#include "CGFContent.h"


//////////////////////////////////////////////////////////////////////////
class CSaverCGF
{
public:
	CSaverCGF( const char *filename,CChunkFile &chunkFile );

	void SaveContent( CContentCGF *pCGF );
	void SetContent( CContentCGF *pCGF ) { m_pCGF = pCGF; };

	// Store nodes in chunk file.
	void SaveNodes();
	void SaveMaterials();
	int SaveExportFlags();

	// Return node chunk id.
	int SaveNode( CNodeCGF *pNode );
	int SaveMaterial( CMaterialCGF *pMtl );

	//special-chunks for characters
	int SaveCompiledBones( void *pData,int nSize );
	int SaveCompiledPhysicalBones( void *pData,int nSize );
	int SaveCompiledPhysicalProxis( void *pData,int nSize, uint32 numIntMorphTargets );
	int SaveCompiledMorphTargets( void *pData,int nSize, uint32 numIntMorphTargets );
	int SaveCompiledIntFaces( void *pData,int nSize );
	int SaveCompiledIntSkinVertices( void *pData,int nSize );
	int SaveCompiledExt2IntMap( void *pData,int nSize );


	int SaveBreakablePhysics( );

	int SaveSpeedInfo( void *pData,int nSize );
	int SaveSpeedInfo1( void *pData,int nSize );
	int SaveFootPlantInfo(void *pData,int nSize );
	int SaveController(int nKeys, int nControllerId, CryKeyPQLog* pData,int nSize );
	int SaveController829(CONTROLLER_CHUNK_DESC_0829& pCtrlChunk,void* pData,int nSize );
	int SaveController830(CONTROLLER_CHUNK_DESC_0830& pCtrlChunk,void* pData,int nSize );
	int SaveControllerDB(CONTROLLER_CHUNK_DESC_0900& pCtrlChunk,void* pData,int nSize );
	int SaveController901(CONTROLLER_CHUNK_DESC_0901& pCtrlChunk,void* pData,int nSize );


private:
	// Return mesh chunk id.
	
	int SaveNodeMesh( CNodeCGF *pNode );
	int SaveHelperChunk( CNodeCGF *pNode );
	int SaveMeshSubsetsChunk( CMesh &mesh );
	int SaveStreamDataChunk( void *pStreamData,int nStreamType,int nCount,int nElemSize );
	int SavePhysicalDataChunk( void *pData,int nSize );

private:
	string m_filename;
	
	CChunkFile *m_pChunkFile;
	CContentCGF *m_pCGF;
	std::set<CNodeCGF*> m_savedNodes;
	std::set<CMaterialCGF*> m_savedMaterials;
	std::map<CMesh*,int> m_mapMeshToChunk;
};

#endif //__CGFSaver_h__
