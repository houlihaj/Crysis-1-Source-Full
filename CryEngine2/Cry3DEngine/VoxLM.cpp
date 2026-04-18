////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   brushlm.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "VoxMan.h"
#include "IndexedMesh.h"
#include "MeshCompiler/MeshCompiler.h"
#include "CGFContent.h"

#include "CGF/ChunkFile.h"
#include "CGF/CGFSaver.h"
#include "CGF/CGFLoader.h"
#include "CGF/ReadOnlyChunkFile.h"

void CVoxelObject::SerializeMesh( CMesh * pMesh )
{
  IChunkFile *pChunkFile = NULL;
  if (SaveIndexedMeshToFile( pMesh, "VoxelMesh", &pChunkFile ))
  {
    void *pMemFile = NULL;
    int nFileSize = 0;
    pChunkFile->WriteToMemory( &pMemFile, &nFileSize );

    CMemoryBlock * pMemoryBlock = CMemoryBlock::CompressToMemBlock(pMemFile, nFileSize, GetSystem());
    if(!pMemoryBlock) // don't compress if compression failed
      pMemoryBlock = new CMemoryBlock(pMemFile, nFileSize);

    m_arrMeshesForSerialization.Add(pMemoryBlock);
    pChunkFile->Release();
  }
}

void CVoxelObject::SavePhysicalizeData( CNodeCGF *pNode )
{
  if (!m_pPhysGeom)
    return;

  CMemStream stm(false);
  IGeomManager *pGeoman = GetPhysicalWorld()->GetGeomManager();
  pGeoman->SavePhysGeometry( stm,m_pPhysGeom );

  // Add physicalized data to the node.
  pNode->physicalGeomData[0].resize(stm.GetUsedSize());
  memcpy( &pNode->physicalGeomData[0][0],stm.GetBuf(),stm.GetUsedSize() );
}

CContentCGF * CVoxelObject::LoadFromMemBlock( CMemoryBlock * pMemBlock, CMesh * & pMesh, std::vector<char> * physData )
{
  PrintComment("Loading %s", "VoxObject");

  CLoaderCGF cgfLoader;

  CReadOnlyChunkFile chunkFile;

  CContentCGF *pCGF = cgfLoader.LoadCGF_FromMemBlock( pMemBlock->GetData(), pMemBlock->GetSize(), &chunkFile );
  if (!pCGF)
  {
    FileWarning( 0,"VoxObject","%s",cgfLoader.GetLastError() );
    return 0;
  }
  //////////////////////////////////////////////////////////////////////////

  CExportInfoCGF *pExportInfo = pCGF->GetExportInfo();
  CNodeCGF *pFirstMeshNode = NULL;

  bool bCompiledCGF = pExportInfo->bCompiledCGF;
  bool bHasJoints = false;

  //////////////////////////////////////////////////////////////////////////
  // Find out number of meshes, and get pointer to the first found mesh.
  //////////////////////////////////////////////////////////////////////////

  string m_szProperties;
  int m_nSubObjectMeshCount=0;

  for (int i = 0; i < pCGF->GetNodeCount(); i++)
  {
    CNodeCGF *pNode = pCGF->GetNode(i);
    if (pNode->pMesh && (pNode->type == CNodeCGF::NODE_MESH))
    {
      if (m_szProperties.empty())
      {
        m_szProperties = pNode->properties; // Take properties from the first mesh node.
        m_szProperties.MakeLower();
      }
      m_nSubObjectMeshCount++;
      if (!pFirstMeshNode)
        pFirstMeshNode = pNode;
    }	
    else if (strncmp(pNode->name,"$joint",6)==0)
      bHasJoints = true;
  }

  // Common of all sub nodes bbox.
  AABB commonBBox;
  commonBBox.Reset();

  bool bHaveMeshNamedMain = false;

  //////////////////////////////////////////////////////////////////////////
  // Create SubObjects.
  //////////////////////////////////////////////////////////////////////////
  if (pCGF->GetNodeCount() > 1 || m_nSubObjectMeshCount > 0)
  {
    std::vector<CNodeCGF*> nodes;
    nodes.reserve( pCGF->GetNodeCount() );

    int nNumMeshes = 0;
    int i;
    for (i = 0; i < pCGF->GetNodeCount(); i++)
    {
      CNodeCGF *pNode = pCGF->GetNode(i);

      IStatObj::SSubObject subObject;
      subObject.pStatObj = 0;
      subObject.bIdentityMatrix = pNode->bIdentityMatrix;
      subObject.bHidden = false;
      subObject.tm = pNode->worldTM;
      subObject.localTM = pNode->localTM;
      subObject.name = pNode->name;
      subObject.properties = pNode->properties;
      subObject.nParent = -1;
      subObject.pWeights = 0;

      if (pNode->type == CNodeCGF::NODE_MESH)
      {
        if (pExportInfo->bMergeAllNodes || m_nSubObjectMeshCount == 0) // Only add helpers, ignore meshes.
          continue;

        nNumMeshes++;
        subObject.nType = STATIC_SUB_OBJECT_MESH;

        if (stricmp(pNode->name,"vox") == 0)
          bHaveMeshNamedMain = true;
      }
      else if (pNode->type == CNodeCGF::NODE_LIGHT)
        subObject.nType = STATIC_SUB_OBJECT_LIGHT;
      else if (pNode->type == CNodeCGF::NODE_HELPER)
      {
      }

      // Only when multiple meshes inside.
      // If only 1 mesh inside, Do not create a separate CStatObj for it.
      if ((m_nSubObjectMeshCount > 0 && pNode->type == CNodeCGF::NODE_MESH && pNode->pMesh != NULL) || 
        (subObject.nType == STATIC_SUB_OBJECT_HELPER_MESH))
      {
        if (!pNode->pSharedMesh) // If shared mesh, then do not create static object.
        {

          pNode->pMesh ;

        }

        // Calc bbox.
        if (pNode->pMesh && pNode->type == CNodeCGF::NODE_MESH)
        {
          AABB box = pNode->pMesh->m_bbox;
          box.SetTransformedAABB( subObject.tm,box );
          commonBBox.Add( box.min );
          commonBBox.Add( box.max );
        }
      }

      //////////////////////////////////////////////////////////////////////////
      // Check for LOD
      //////////////////////////////////////////////////////////////////////////
      if (subObject.nType == STATIC_SUB_OBJECT_HELPER_MESH)
      {
      }
      //////////////////////////////////////////////////////////////////////////

      if (subObject.pStatObj)
        ((CStatObj*)subObject.pStatObj)->m_nUsers++; // AddRef

      nodes.push_back(pNode);
    }

    // Assign SubObject parent pointers.
    int nNumCgfNodes = (int)nodes.size();

  }

  //////////////////////////////////////////////////////////////////////////
  // Create StatObj from Mesh.
  //////////////////////////////////////////////////////////////////////////
  if (pExportInfo->bMergeAllNodes || m_nSubObjectMeshCount == 0)
  {
    if (pCGF->GetMergedMesh())
      pMesh = pCGF->GetMergedMesh();
    if (!pMesh && pFirstMeshNode)
      pMesh = pFirstMeshNode->pMesh;

    // Not support not merged mesh yet.
    if (!pMesh)
    {
      delete pCGF;
      return 0;
    }
  }
  //////////////////////////////////////////////////////////////////////////

  for (int i = 0; i < pCGF->GetNodeCount(); i++) if (strstr(pCGF->GetNode(i)->properties,"deformable"))
    m_nFlags |= STATIC_OBJECT_DEFORMABLE;
  if (m_nSubObjectMeshCount > 0)
    m_nFlags |= STATIC_OBJECT_COMPOUND;


  if(physData && pFirstMeshNode && pFirstMeshNode->physicalGeomData[0].size())
    *physData = pFirstMeshNode->physicalGeomData[0];

  return pCGF;
}

bool CVoxelObject::SaveIndexedMeshToFile( CMesh * pMesh, const char *sFilename, IChunkFile** pOutChunkFile )
{
  CContentCGF *pCGF = new CContentCGF(sFilename);

  CChunkFile *pChunkFile = new CChunkFile;
  if (pOutChunkFile)
    *pOutChunkFile = pChunkFile;

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
  pNode->pMesh->Copy( *pMesh );
  pNode->pParent = 0;
  pNode->pMaterial = NULL;
  pNode->nPhysicalizeFlags = 0;
  SavePhysicalizeData( pNode );

  // Add node to CGF contents.
  pCGF->AddNode( pNode );

  CSaverCGF cgfSaver( sFilename,*pChunkFile );
  cgfSaver.SaveContent( pCGF );

  bool bResult = true;
  if (!pOutChunkFile)
  {
    bResult = pChunkFile->Write( sFilename );
    pChunkFile->Release();
  }

  delete pCGF;

  return bResult;
}

void CVoxelMesh::DeleteCContentCGF(CContentCGF * pObj)
{
  delete pObj;
}