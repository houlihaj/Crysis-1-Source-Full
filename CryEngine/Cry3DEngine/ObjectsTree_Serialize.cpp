#include "StdAfx.h"

#include "VoxMan.h"
#include "Brush.h"
#include "RoadRenderNode.h"
#include "DecalRenderNode.h"
#include "WaterVolumeRenderNode.h"
#include "DistanceCloudRenderNode.h"
#include "WaterWaveRenderNode.h"
#include "AutoCubeMapRenderNode.h"
#include "VisAreas.h"
#include "Brush.h"
#if defined(PS3)
	#include "CryTypeInfo.h"
#endif

#define OCTREENODE_CHUNK_VERSION 2

CMemoryBlock COctreeNode::m_mbLoadingCache;

#pragma pack(push,1)

struct SRenderNodeChunk
{
	SRenderNodeChunk() { fNotUsed=0; m_nInternalFlags=0; }
	// common node data
	AABB m_WSBBox;
	float fNotUsed;
	float m_fWSMaxViewDist;
	int m_dwRndFlags; 
	unsigned char m_ucViewDistRatio;
	unsigned char m_ucLodRatio;
	unsigned char m_nInternalFlags;
};

struct SBrushChunk : public SRenderNodeChunk
{
	// brush data
	Vec3 m_vPos; 
	Ang3 m_vAngles;
	float m_fScale;
	Matrix34 m_Matrix;
	int m_nMaterialId;
	int m_nEditorObjectId;
	int m_nObjectTypeID;
  unsigned char m_nMaterialLayers;

	AUTO_STRUCT_INFO_LOCAL
};

struct SRoadChunk : public SRenderNodeChunk
{
	// brush data
	Vec3 m_arrVerts[4]; 
	float m_arrTexCoors[2];
	int m_nMaterialId;

	AUTO_STRUCT_INFO_LOCAL
};

struct SRoadChunk_NEW : public SRenderNodeChunk
{
	// brush data
	int m_nVertsNum;
	float m_arrTexCoors[2];
	float m_arrTexCoorsGlobal[2];
	int m_nMaterialId;

	AUTO_STRUCT_INFO_LOCAL
};

struct SVegetationChunk : public SRenderNodeChunk
{
	Vec3 m_vPos;
	float m_fScale;
	uchar m_nObjectTypeID;
	uchar m_ucBright;
	uchar m_ucAngle;

	AUTO_STRUCT_INFO_LOCAL
};

struct SVoxelObjectChunkVer3 : public SRenderNodeChunk
{
	SVoxelChunkVer3 m_VoxData;
	Matrix34 m_Matrix;

	AUTO_STRUCT_INFO_LOCAL
};

struct SVoxelObjectChunkVer4 : public SRenderNodeChunk
{
  SVoxelChunkVer4 m_VoxData;
  Matrix34 m_Matrix;

  AUTO_STRUCT_INFO_LOCAL
};

struct SDecalChunk : public SRenderNodeChunk
{
	int32 m_projectionType; 
	Vec3 m_pos;
	Vec3 m_normal;
	Matrix33 m_explicitRightUpFront;
	f32 m_radius;
	int32 m_materialID;

	AUTO_STRUCT_INFO_LOCAL
};

struct SWaterVolumeChunk : public SRenderNodeChunk
{
	// volume type and id
	int32 m_volumeType;
	uint64 m_volumeID;

	// material
	int32 m_materialID;	

	// fog properties
	f32 m_fogDensity;
	Vec3 m_fogColor;
	Plane m_fogPlane;

	// render geometry 
	f32 m_uTexCoordBegin;
	f32 m_uTexCoordEnd;
	f32 m_surfUScale;
	f32 m_surfVScale;
	uint32 m_numVertices;

	// physics properties
	f32 m_volumeDepth;
	f32 m_streamSpeed;
	uint32 m_numVerticesPhysAreaContour;

	AUTO_STRUCT_INFO_LOCAL
};

struct SWaterVolumeVertex
{
	Vec3 m_xyz;
	
	AUTO_STRUCT_INFO_LOCAL
};

struct SDistanceCloudChunk : public SRenderNodeChunk
{
	Vec3 m_pos;
	f32 m_sizeX;
	f32 m_sizeY;
	f32 m_rotationZ;
	int32 m_materialID;

	AUTO_STRUCT_INFO_LOCAL
};

struct SWaterWaveChunk : public SRenderNodeChunk
{
	int32 m_nID;

	// Geometry properties
	Matrix34 m_pWorldTM;
	uint32 m_nVertexCount;

	f32 m_fUScale;
	f32 m_fVScale;

	// Material
	int32 m_nMaterialID;


	// Animation properties

	f32 m_fSpeed;
	f32 m_fSpeedVar;

	f32 m_fLifetime;
	f32 m_fLifetimeVar;  

	f32 m_fPosVar;

	f32 m_fHeight;
	f32 m_fHeightVar;

	f32 m_fCurrLifetime;  
	f32 m_fCurrFrameLifetime;
	f32 m_fCurrSpeed;
	f32 m_fCurrHeight;

	AUTO_STRUCT_INFO_LOCAL
};

struct SAutoCubeMapChunk : public SRenderNodeChunk
{
	uint32 m_flags;

	Quat m_oobRot;
	Vec3 m_oobH;
	Vec3 m_oobC;

	Vec3 m_refPos;
	
	AUTO_STRUCT_INFO_LOCAL
};

AUTO_TYPE_INFO(EERType)

#pragma pack(pop)

#include "TypeInfo_impl.h"
#include "ObjectsTree_Serialize_info.h"

//////////////////////////////////////////////////////////////////////////
bool COctreeNode::CheckRenderFlagsMinSpec( uint32 dwRndFlags )
{
	int nRenderNodeMinSpec = (dwRndFlags&ERF_SPEC_BITS_MASK) >> ERF_SPEC_BITS_SHIFT;
	return CheckMinSpec(nRenderNodeMinSpec);
}

//////////////////////////////////////////////////////////////////////////
int COctreeNode::SerializeObjects(bool bSave, CMemoryBlock * pMemBlock, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable)
{
	int nBlockSize=0;

	if(bSave)
	{
		FreeAreaBrushes();

    for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
		{
			IRenderNode * pRenderNode = pObj;
			EERType eType = pRenderNode->GetRenderNodeType();
			if(eType == eERType_Vegetation && !(pRenderNode->GetRndFlags()&ERF_PROCEDURAL))
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SVegetationChunk);
			}
			else if(eType == eERType_Brush && !(pRenderNode->GetRndFlags()&ERF_MERGE_RESULT))
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SBrushChunk);
			}
			else if(eType == eERType_VoxelObject && ((CVoxelObject*)pRenderNode)->GetRenderMesh(0) && ((CVoxelObject*)pRenderNode)->GetRenderMesh(0)->GetSysIndicesCount())
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SVoxelObjectChunkVer4);
				nBlockSize += ((CVoxelObject*)pRenderNode)->GetCompiledMeshDataSize();
			}
			else if(eType == eERType_Decal)
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SDecalChunk);
			}
			else if(eType == eERType_WaterVolume)
			{
				CWaterVolumeRenderNode* pWVRN( static_cast< CWaterVolumeRenderNode* >( pRenderNode ) );
				const SWaterVolumeSerialize* pSerParams( pWVRN->GetSerializationParams() );
				if( pSerParams )
				{
					nBlockSize += sizeof( eType );
					nBlockSize += sizeof( SWaterVolumeChunk );
					nBlockSize += ( pSerParams->m_vertices.size() + pSerParams->m_physicsAreaContour.size() ) * sizeof( SWaterVolumeVertex );
				}
			}
			else if(eType == eERType_RoadObject_NEW)
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SRoadChunk_NEW);
				nBlockSize += ((CRoadRenderNode*)pRenderNode)->m_arrVerts.GetDataSize();
			}
			else if(eType == eERType_DistanceCloud)
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SDistanceCloudChunk);
			}
      else if(eType == eERType_WaterWave)
      {
        CWaterWaveRenderNode* pWaveRenderNode( static_cast< CWaterWaveRenderNode* >( pRenderNode ) );
        const SWaterWaveSerialize* pSerParams( pWaveRenderNode->GetSerializationParams() );

        nBlockSize += sizeof( eType );
        nBlockSize += sizeof( SWaterWaveChunk );

        nBlockSize += ( pSerParams->m_pVertices.size() ) * sizeof( Vec3 );
      }
			else if(eType == eERType_AutoCubeMap)
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SAutoCubeMapChunk);
			}
		}

		if(pMemBlock)
		{
			pMemBlock->Allocate(nBlockSize);
			byte * pPtr = (byte *)pMemBlock->GetData();

      for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
			{
				IRenderNode * pEnt = pObj;
				EERType eType = pEnt->GetRenderNodeType();

				if(eType == eERType_Brush && !(pEnt->GetRndFlags()&ERF_MERGE_RESULT))
				{
					memcpy(pPtr,&eType,sizeof(eType)); pPtr += sizeof(eType);

					CBrush * pObj = (CBrush*)pEnt;

					SBrushChunk chunk;

					// common node data
					COPY_MEMBER(&chunk,pObj,m_WSBBox);
//					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,m_ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,m_ucLodRatio);
          //COPY_MEMBER(&chunk,pObj,m_nInternalFlags);
          COPY_MEMBER(&chunk,pObj,m_nMaterialLayers);          

					// brush data
					COPY_MEMBER(&chunk,pObj,m_vPos); 
					//					COPY_MEMBER(&chunk,pObj,m_vAngles);
					//					COPY_MEMBER(&chunk,pObj,m_fScale);
					COPY_MEMBER(&chunk,pObj,m_Matrix);

//          COPY_MEMBER(&chunk,pObj,m_nMaterialId);
          chunk.m_nMaterialId = CObjManager::GetItemId(pMatTable, pObj->GetMaterial());

					COPY_MEMBER(&chunk,pObj,m_nEditorObjectId);
//					COPY_MEMBER(&chunk,pObj,m_nObjectTypeID);

          { // find StatObj Id
            chunk.m_nObjectTypeID = CObjManager::GetItemId(pStatObjTable, pObj->GetStatObj());
            if(chunk.m_nObjectTypeID<0)
              assert(!"StatObj not found in BrushTable on exporting");
          }

					memcpy(pPtr,&chunk,sizeof(chunk)); pPtr += sizeof(chunk);
				}
				else if(eType == eERType_Vegetation  && !(pEnt->GetRndFlags()&ERF_PROCEDURAL))
				{
					memcpy(pPtr,&eType,sizeof(eType)); pPtr += sizeof(eType);

					CVegetation * pObj = (CVegetation*)pEnt;

					SVegetationChunk chunk;

					// common node data
					chunk.m_WSBBox = pObj->GetBBox();
//					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,m_ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,m_ucLodRatio);
          //COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					// vegetation data
					COPY_MEMBER(&chunk,pObj,m_vPos);
          chunk.m_fScale = pObj->GetScale();
					COPY_MEMBER(&chunk,pObj,m_nObjectTypeID);
					COPY_MEMBER(&chunk,pObj,m_ucAngle);

					memcpy(pPtr,&chunk,sizeof(chunk)); pPtr += sizeof(chunk);
				}
				else if(eType == eERType_VoxelObject && ((CVoxelObject*)pEnt)->GetRenderMesh(0) && ((CVoxelObject*)pEnt)->GetRenderMesh(0)->GetSysIndicesCount())
				{
					memcpy(pPtr,&eType,sizeof(eType)); pPtr += sizeof(eType);

					CVoxelObject * pObj = (CVoxelObject*)pEnt;

          PrintMessage("Exporting compiled voxel object (%d,%d,%d) ...", 
            (int)pObj->GetPos(true).x, 
            (int)pObj->GetPos(true).y, 
            (int)pObj->GetPos(true).z);

					SVoxelObjectChunkVer4 *chunk = new SVoxelObjectChunkVer4;

					// common node data
					COPY_MEMBER(chunk,pObj,m_WSBBox);
//					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(chunk,pObj,m_ucViewDistRatio);
					COPY_MEMBER(chunk,pObj,m_ucLodRatio);
					//COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					// voxel data
					COPY_MEMBER(chunk,pObj,m_Matrix);
					SVoxelChunkVer3* dataChunk = new SVoxelChunkVer3;
					pObj->GetData(*dataChunk,Vec3(0,0,0));
					
          chunk->m_VoxData.nChunkVersion = 4;
          chunk->m_VoxData.vOrigin = dataChunk->vOrigin;
          chunk->m_VoxData.vSize = dataChunk->vSize;
          chunk->m_VoxData.nFlags = dataChunk->nFlags;
          memcpy(chunk->m_VoxData.m_arrSurfaceNames,dataChunk->m_arrSurfaceNames,sizeof(chunk->m_VoxData.m_arrSurfaceNames));

          if(pObj->GetCompiledMeshDataSize())
            chunk->m_VoxData.nFlags |= IVOXELOBJECT_FLAG_COMPILED;

					memcpy(pPtr,chunk,sizeof(*chunk)); pPtr += sizeof(*chunk);

          // compiled mesh
          if(chunk->m_VoxData.nFlags & IVOXELOBJECT_FLAG_COMPILED)
          {
            int nSize=0;
            byte * pMesh = pObj->GetCompiledMeshData(nSize);
            memcpy(pPtr,pMesh,nSize); pPtr += nSize;
            delete[] pMesh;//avoid deleting void ptr
          }

          SAFE_DELETE(chunk);
          SAFE_DELETE(dataChunk);

          PrintMessagePlus(" ok");
				}
				else if(eType == eERType_RoadObject_NEW)
				{
					memcpy(pPtr,&eType,sizeof(eType)); pPtr += sizeof(eType);

					CRoadRenderNode * pObj = (CRoadRenderNode*)pEnt;

					SRoadChunk_NEW chunk;

					// common node data
					COPY_MEMBER(&chunk,pObj,m_WSBBox);
//					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,m_ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,m_ucLodRatio);
					//COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					// road data
					//COPY_MEMBER(&chunk,pObj,m_nMaterialId);
          int nMaterialId = CObjManager::GetItemId(pMatTable,pObj->GetMaterial());
					chunk.m_nMaterialId = (nMaterialId & 0xFFFFFF) | (pObj->m_sortPrio << 24);

					chunk.m_nVertsNum = pObj->m_arrVerts.Count();

					for(int i=0; i<2; i++)
						COPY_MEMBER(&chunk,pObj,m_arrTexCoors[i]);

					for(int i=0; i<2; i++)
						COPY_MEMBER(&chunk,pObj,m_arrTexCoorsGlobal[i]);

					memcpy(pPtr,&chunk,sizeof(chunk)); pPtr += sizeof(chunk);
					
					// copy verts
					memcpy(pPtr,pObj->m_arrVerts.GetElements(),pObj->m_arrVerts.GetDataSize()); 
					pPtr += pObj->m_arrVerts.GetDataSize();
				}
				else if( eERType_Decal == eType )
				{					
					memcpy( pPtr, &eType, sizeof( eType ) ); 
					pPtr += sizeof( eType );

					CDecalRenderNode* pObj( (CDecalRenderNode*) pEnt );

					SDecalChunk chunk;

					// common node data
					chunk.m_WSBBox = pObj->GetBBox();
//					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,m_ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,m_ucLodRatio);
					//COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					// decal properties
					const SDecalProperties* pDecalProperties( pObj->GetDecalProperties() );
					assert( pDecalProperties );
					chunk.m_projectionType = pDecalProperties->m_projectionType;
					chunk.m_pos = pDecalProperties->m_pos;
					chunk.m_normal = pDecalProperties->m_normal;
					chunk.m_explicitRightUpFront = pDecalProperties->m_explicitRightUpFront;
					chunk.m_radius = pDecalProperties->m_radius;
					//chunk.m_materialID = pObj->GetMaterialID();

          int nMaterialId = CObjManager::GetItemId(pMatTable,pObj->GetMaterial());
					chunk.m_materialID = (nMaterialId & 0xFFFFFF) | (pDecalProperties->m_sortPrio << 24);

					memcpy( pPtr, &chunk, sizeof( chunk ) ); 
					pPtr += sizeof( chunk );
				}
				else if( eERType_WaterVolume == eType )
				{
					CWaterVolumeRenderNode* pObj( (CWaterVolumeRenderNode*) pEnt );

					// get access to serialization parameters
					const SWaterVolumeSerialize* pSerData( pObj->GetSerializationParams() );
					assert( pSerData ); // trying to save level outside the editor?

					// save type
					memcpy( pPtr, &eType, sizeof( eType ) ); 
					pPtr += sizeof( eType );

					// save node data
					SWaterVolumeChunk chunk;

          chunk.m_WSBBox = pObj->GetBBox();
//					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,m_ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,m_ucLodRatio);
					//COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					chunk.m_volumeType = pSerData->m_volumeType;
					chunk.m_volumeID = pSerData->m_volumeID;

          chunk.m_materialID = CObjManager::GetItemId(pMatTable,pSerData->m_pMaterial);

					chunk.m_fogDensity = pSerData->m_fogDensity;
					chunk.m_fogColor = pSerData->m_fogColor;
					chunk.m_fogPlane = pSerData->m_fogPlane;

					chunk.m_uTexCoordBegin = pSerData->m_uTexCoordBegin;
					chunk.m_uTexCoordEnd = pSerData->m_uTexCoordEnd;
					chunk.m_surfUScale = pSerData->m_surfUScale;
					chunk.m_surfVScale = pSerData->m_surfVScale;
					chunk.m_numVertices = pSerData->m_vertices.size();

					chunk.m_volumeDepth = pSerData->m_volumeDepth;
					chunk.m_streamSpeed = pSerData->m_streamSpeed;
					chunk.m_numVerticesPhysAreaContour = pSerData->m_physicsAreaContour.size();

					memcpy( pPtr, &chunk, sizeof( chunk ) ); 
					pPtr += sizeof( chunk );

					// save vertices
					for( size_t i( 0 ); i < pSerData->m_vertices.size(); ++i )
					{
						SWaterVolumeVertex v;
						v.m_xyz = pSerData->m_vertices[i];

						memcpy( pPtr, &v, sizeof( v ) ); 
						pPtr += sizeof( v );
					}

					// save physics area contour vertices
					for( size_t i( 0 ); i < pSerData->m_physicsAreaContour.size(); ++i )
					{
						SWaterVolumeVertex v;
						v.m_xyz = pSerData->m_physicsAreaContour[i];

						memcpy( pPtr, &v, sizeof( v ) ); 
						pPtr += sizeof( v );
					}
				}
				else if (eERType_DistanceCloud == eType)
				{					
					memcpy(pPtr, &eType, sizeof(eType)); 
					pPtr += sizeof(eType);

					CDistanceCloudRenderNode* pObj((CDistanceCloudRenderNode*)pEnt);

					SDistanceCloudChunk chunk;

					// common node data
          chunk.m_WSBBox = pObj->GetBBox();
//					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,m_ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,m_ucLodRatio);
					//COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					// distance cloud properties
					SDistanceCloudProperties properties(pObj->GetProperties());
					chunk.m_pos = properties.m_pos;
					chunk.m_sizeX = properties.m_sizeX;
					chunk.m_sizeY = properties.m_sizeY;
					chunk.m_rotationZ = properties.m_rotationZ;
          chunk.m_materialID = CObjManager::GetItemId(pMatTable,pObj->GetMaterial());

					memcpy(pPtr, &chunk, sizeof(chunk)); 
					pPtr += sizeof(chunk);
				}
				else if( eERType_WaterWave == eType )
        {
          CWaterWaveRenderNode* pObj( (CWaterWaveRenderNode*) pEnt );

          // get access to serialization parameters
          const SWaterWaveSerialize* pSerData( pObj->GetSerializationParams() );
          assert( pSerData ); // trying to save level outside the editor?

          // save type
          memcpy( pPtr, &eType, sizeof( eType ) ); 
          pPtr += sizeof( eType );
          
          SWaterWaveChunk chunk;

          // common node data
          chunk.m_WSBBox = pObj->GetBBox();
//          COPY_MEMBER(&chunk,pObj,GetRadius());
          COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
          COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
          COPY_MEMBER(&chunk,pObj,m_ucViewDistRatio);
          COPY_MEMBER(&chunk,pObj,m_ucLodRatio);
          //COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

          chunk.m_nID = pSerData->m_nID;
          chunk.m_nMaterialID = CObjManager::GetItemId(pMatTable, pSerData->m_pMaterial);

          chunk.m_fUScale = pSerData->m_fUScale;
          chunk.m_fVScale = pSerData->m_fVScale;

          chunk.m_nVertexCount = pSerData->m_nVertexCount;

          chunk.m_pWorldTM = pSerData->m_pWorldTM;

          chunk.m_fSpeed = pSerData->m_pParams.m_fSpeed;
          chunk.m_fSpeedVar = pSerData->m_pParams.m_fSpeedVar;
          chunk.m_fLifetime = pSerData->m_pParams.m_fLifetime;
          chunk.m_fLifetimeVar = pSerData->m_pParams.m_fLifetimeVar;
          chunk.m_fPosVar = pSerData->m_pParams.m_fPosVar;
          chunk.m_fHeight = pSerData->m_pParams.m_fHeight;
          chunk.m_fHeightVar = pSerData->m_pParams.m_fHeightVar;
          
          chunk.m_fCurrLifetime = pSerData->m_pParams.m_fCurrLifetime;          
          chunk.m_fCurrFrameLifetime = pSerData->m_pParams.m_fCurrFrameLifetime;
          chunk.m_fCurrSpeed = pSerData->m_pParams.m_fCurrSpeed;
          chunk.m_fCurrHeight = pSerData->m_pParams.m_fCurrHeight;

          memcpy(pPtr, &chunk, sizeof(chunk)); 
          pPtr += sizeof(chunk);

          // save vertices
          for( size_t i( 0 ); i < pSerData->m_nVertexCount; ++i )
          {
            Vec3 pPos( pSerData->m_pVertices[i] );
            memcpy( pPtr, &pPos, sizeof( pPos ) ); 
            pPtr += sizeof( pPos );
          }
        }
				else if (eERType_AutoCubeMap == eType)
				{					
					memcpy(pPtr, &eType, sizeof(eType)); 
					pPtr += sizeof(eType);

					CAutoCubeMapRenderNode* pObj((CAutoCubeMapRenderNode*)pEnt);

					SAutoCubeMapChunk chunk;

					// common node data
          chunk.m_WSBBox = pObj->GetBBox();
//					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,m_ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,m_ucLodRatio);
					//COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					// auto cube map properties
					SAutoCubeMapProperties properties(pObj->GetProperties());
					chunk.m_flags = properties.m_flags;
					chunk.m_oobRot = Quat(properties.m_obb.m33);
					chunk.m_oobH = properties.m_obb.h;
					chunk.m_oobC = properties.m_obb.c;
					chunk.m_refPos = properties.m_refPos;

					memcpy(pPtr, &chunk, sizeof(chunk)); 
					pPtr += sizeof(chunk);
				}
				//else
				//	assert(!"Unsupported object type");
			}

			assert(pPtr == (byte *)pMemBlock->GetData()+nBlockSize);
		}
	}
	else
	{ // Load objects
//		assert(m_lstObjects.Count()==0);
//		m_lstObjects.Reset();

		byte * pPtr = (byte *)pMemBlock->GetData();
		byte * pEndPtr = pPtr + pMemBlock->GetSize();

		while(pPtr < pEndPtr)
		{
			EERType eType = *StepData<EERType>(pPtr);

			// For these structs, our Endian swapping is built in to the member copy.
			if(eType == eERType_Brush) 
			{
				SBrushChunk * pChunk = StepData<SBrushChunk>(pPtr);

				if (!CheckRenderFlagsMinSpec(pChunk->m_dwRndFlags))
					continue;

				CBrush * pObj = new CBrush();

				// common node data
				COPY_MEMBER(pObj,pChunk,m_WSBBox);
//				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,m_ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,m_ucLodRatio);
        //COPY_MEMBER(pObj,pChunk,m_nInternalFlags);
        COPY_MEMBER(pObj,pChunk,m_nMaterialLayers);
				pObj->m_dwRndFlags &= ~(ERF_HIDDEN|ERF_SELECTED);

				// brush data
				COPY_MEMBER(pObj,pChunk,m_vPos); 
				//				COPY_MEMBER(pObj,pChunk,m_vAngles);
				//				COPY_MEMBER(pObj,pChunk,m_fScale);
				COPY_MEMBER(pObj,pChunk,m_Matrix);
//				COPY_MEMBER(pObj,pChunk,m_nMaterialId);
				COPY_MEMBER(pObj,pChunk,m_nEditorObjectId);

        // COPY_MEMBER(pObj,pChunk,m_nObjectTypeID);
        pObj->SetStatObj(CObjManager::GetItemPtr(pStatObjTable, pChunk->m_nObjectTypeID));

				// set material
        pObj->SetMaterial(CObjManager::GetItemPtr(pMatTable, pChunk->m_nMaterialId));

				//to get correct indirect lighting the registration must be done before checking if this object is inside a VisArea
				Get3DEngine()->RegisterEntity(pObj);

				pObj->Physicalize();
			}
			else if(eType == eERType_Vegetation)
			{
				SVegetationChunk * pChunk = StepData<SVegetationChunk>(pPtr);

				if (!CheckRenderFlagsMinSpec(GetObjManager()->m_lstStaticTypes[pChunk->m_nObjectTypeID].m_dwRndFlags))
					continue;

        StatInstGroup * pGroup = &GetObjManager()->m_lstStaticTypes[pChunk->m_nObjectTypeID];
        if(!pGroup->GetStatObj() || pGroup->GetStatObj()->GetRadius()*pChunk->m_fScale < GetCVars()->e_vegetation_min_size)
          continue; // skip creation of very small objects

				CVegetation * pObj = new CVegetation();

				// common node data
//				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,m_ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,m_ucLodRatio);
        //COPY_MEMBER(pObj,pChunk,m_nInternalFlags);
				pObj->m_dwRndFlags &= ~(ERF_HIDDEN|ERF_SELECTED);

				// vegetation data
				COPY_MEMBER(pObj,pChunk,m_vPos);

				//cannot load from unaligned addresses, make a temp copy
				float scaleLoc;
				memcpy(&scaleLoc, &pChunk->m_fScale, sizeof(float));
				pObj->SetScale(scaleLoc);

				COPY_MEMBER(pObj,pChunk,m_nObjectTypeID);
//				COPY_MEMBER(pObj,pChunk,m_ucBright);
				COPY_MEMBER(pObj,pChunk,m_ucAngle);

				//cannot load from unaligned addresses, make a temp copy
				AABB wsBBoxLoc;
				memcpy(&wsBBoxLoc, &pChunk->m_WSBBox, sizeof(AABB));
        pObj->SetBBox(wsBBoxLoc);

				pObj->Physicalize();
				Get3DEngine()->RegisterEntity(pObj);

				//pObj->CalcTerrainAdaption();

				assert(!(pObj->GetRndFlags()&ERF_PROCEDURAL));
			}
			else if(eType == eERType_VoxelObject)
			{
				int nVersion = ((SVoxelObjectChunkVer3*)pPtr)->m_VoxData.nChunkVersion;
				SwapEndian(nVersion);

				SVoxelObjectChunkVer3 chunkVer3;
        if(nVersion == 4)
        {
          SVoxelObjectChunkVer4 * pChunk = StepData<SVoxelObjectChunkVer4>(pPtr);
          SVoxelObjectChunkVer4 & chunk = *pChunk;

          chunkVer3.m_WSBBox = chunk.m_WSBBox;
          chunkVer3.m_fWSMaxViewDist = chunk.m_fWSMaxViewDist;
          chunkVer3.m_dwRndFlags = chunk.m_dwRndFlags; 
          chunkVer3.m_ucViewDistRatio = chunk.m_ucViewDistRatio;
          chunkVer3.m_ucLodRatio = chunk.m_ucLodRatio;
          chunkVer3.m_Matrix = chunk.m_Matrix;

          memcpy(chunkVer3.m_VoxData.m_arrSurfaceNames,chunk.m_VoxData.m_arrSurfaceNames,sizeof(chunkVer3.m_VoxData.m_arrSurfaceNames));

          chunkVer3.m_VoxData.nChunkVersion = 3;
          chunkVer3.m_VoxData.nFlags = chunk.m_VoxData.nFlags;
          chunkVer3.m_VoxData.vOrigin = chunk.m_VoxData.vOrigin;
          chunkVer3.m_VoxData.vSize = chunk.m_VoxData.vSize;
        }
				else if(nVersion == 3)
				{
					SVoxelObjectChunkVer3 * pChunk = StepData<SVoxelObjectChunkVer3>(pPtr);
					memcpy(&chunkVer3,pChunk,sizeof(chunkVer3));
				}
				else
				{
					assert(!"nVersion unknown");
					break; // error
				}

//				if (!CheckRenderFlagsMinSpec(chunkVer3.m_dwRndFlags))
	//				continue;

				CVoxelObject * pObj = new CVoxelObject(chunkVer3.m_Matrix.GetTranslation(), DEF_VOX_UNIT_SIZE, 
					DEF_VOX_VOLUME_SIZE, DEF_VOX_VOLUME_SIZE, DEF_VOX_VOLUME_SIZE);

				// common node data
				COPY_MEMBER(pObj,&chunkVer3,m_WSBBox);
//				COPY_MEMBER(pObj,&chunkVer3,GetRadius());
				COPY_MEMBER(pObj,&chunkVer3,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,&chunkVer3,m_dwRndFlags); 
				COPY_MEMBER(pObj,&chunkVer3,m_ucViewDistRatio);
				COPY_MEMBER(pObj,&chunkVer3,m_ucLodRatio);
//				COPY_MEMBER(pObj,&chunkVer3,m_nInternalFlags);
				pObj->m_dwRndFlags &= ~(ERF_HIDDEN|ERF_SELECTED);

				// voxel data
				COPY_MEMBER(pObj,&chunkVer3,m_Matrix);

        if(chunkVer3.m_VoxData.nFlags & IVOXELOBJECT_FLAG_COMPILED)
        {
          PrintMessage("Loading compiled voxel object (%d,%d,%d) ...", 
            (int)chunkVer3.m_Matrix.GetTranslation().x, 
            (int)chunkVer3.m_Matrix.GetTranslation().y, 
            (int)chunkVer3.m_Matrix.GetTranslation().z);

          pObj->SetSurfacesInfo(&chunkVer3.m_VoxData);

          int nSize=0;
          pObj->SetCompiledMeshData(pPtr,nSize);
          nSize = pObj->GetCompiledMeshDataSize();
          pPtr += nSize;

          pObj->ReleaseMemBlocks();

          if(pObj->GetRenderMesh(0) && pObj->GetRenderMesh(0)->GetSysIndicesCount())
          {
            Get3DEngine()->RegisterEntity(pObj);
            PrintMessagePlus(" (%d KB) Ok", nSize/1024);
          }
          else
          {
            PrintMessagePlus(" (%d KB) Empty object skipped", nSize/1024);
            delete pObj;
          }
        }
        else
        {
          assert(nVersion == 3);
          pObj->SetData(&chunkVer3.m_VoxData, 0);
          Get3DEngine()->RegisterEntity(pObj);
        }
			}
			else if(eType == eERType_RoadObject_NEW) 
			{
				SRoadChunk_NEW * pChunk = StepData<SRoadChunk_NEW>(pPtr);
				if (!CheckRenderFlagsMinSpec(pChunk->m_dwRndFlags))
        {
          for( int j=0; j < pChunk->m_nVertsNum; j++ )
          {
            Vec3 * pVert = StepData<Vec3>( pPtr );
          }

					continue;
        }

				CRoadRenderNode * pObj = new CRoadRenderNode();

				// common node data
				COPY_MEMBER(pObj,pChunk,m_WSBBox);
//				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,m_ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,m_ucLodRatio);
				//COPY_MEMBER(pObj,pChunk,m_nInternalFlags);
				pObj->m_dwRndFlags &= ~(ERF_HIDDEN|ERF_SELECTED);

				// road data
				//COPY_MEMBER(pObj,pChunk,m_nMaterialId);
				pObj->SetMaterial(CObjManager::GetItemPtr(pMatTable,pChunk->m_nMaterialId & 0xFFFFFF));
				pObj->m_sortPrio = (pChunk->m_nMaterialId >> 24) & 0xFF;

				for(int i=0; i<2; i++)
					COPY_MEMBER(pObj,pChunk,m_arrTexCoors[i]);

				for(int i=0; i<2; i++)
					COPY_MEMBER(pObj,pChunk,m_arrTexCoorsGlobal[i]);

				pObj->m_arrVerts.PreAllocate(pChunk->m_nVertsNum);
				for( int j=0; j < pChunk->m_nVertsNum; j++ )
				{
					Vec3 * pVert = StepData<Vec3>( pPtr );
					pObj->m_arrVerts.Add( *pVert );
				}

				pObj->SetVertices(pObj->m_arrVerts.GetElements(), pObj->m_arrVerts.Count(), 
					pObj->m_arrTexCoors[0], pObj->m_arrTexCoors[1], 
					pObj->m_arrTexCoorsGlobal[0], pObj->m_arrTexCoorsGlobal[1]);

				Get3DEngine()->RegisterEntity(pObj);
			}
			else if( eERType_Decal == eType )
			{
				SDecalChunk* pChunk( StepData<SDecalChunk>( pPtr ) );
				if (!CheckRenderFlagsMinSpec(pChunk->m_dwRndFlags))
					continue;

				CDecalRenderNode* pObj( new CDecalRenderNode() );

				// common node data
				pObj->SetBBox(pChunk->m_WSBBox);
//				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,m_ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,m_ucLodRatio);
				//COPY_MEMBER(pObj,pChunk,m_nInternalFlags);
				pObj->m_dwRndFlags &= ~(ERF_HIDDEN|ERF_SELECTED);

				// decal properties
				SDecalProperties properties;

				switch( pChunk->m_projectionType )
				{
				case SDecalProperties::eProjectOnTerrainAndStaticObjects:
					{
						properties.m_projectionType = SDecalProperties::eProjectOnTerrainAndStaticObjects;
						break;
					}
				case SDecalProperties::eProjectOnStaticObjects:
					{
						properties.m_projectionType = SDecalProperties::eProjectOnStaticObjects;
						break;
					}
				case SDecalProperties::eProjectOnTerrain:
					{
						properties.m_projectionType = SDecalProperties::eProjectOnTerrain;
						break;
					}
				case SDecalProperties::ePlanar:
				default:
					{
						properties.m_projectionType = SDecalProperties::ePlanar;
						break;
					}
				}
				properties.m_pos = pChunk->m_pos;
				properties.m_normal = pChunk->m_normal;
        Matrix33 matTemp;
        memcpy(&matTemp, &pChunk->m_explicitRightUpFront, sizeof (matTemp));
				properties.m_explicitRightUpFront = matTemp;
        float floatTemp;
        memcpy(&floatTemp, &pChunk->m_radius, sizeof(floatTemp));
				properties.m_radius = floatTemp;				
        int32 intTemp;
        memcpy(&intTemp, &pChunk->m_materialID, sizeof(intTemp));

				uint32 matID = intTemp & 0xFFFFFF;
				IMaterial* pMaterial( CObjManager::GetItemPtr(pMatTable,matID) );
				assert(pMaterial);
				properties.m_pMaterialName = pMaterial->GetName();
				properties.m_sortPrio = (intTemp >> 24) & 0xFF;

				pObj->SetDecalProperties( properties );

				//if( pObj->GetMaterialID() != pChunk->m_materialID )
				if( pObj->GetMaterial() != pMaterial )
				{
					const char* pMatName( "_can't_resolve_material_name_" );										
					int32 matID( pChunk->m_materialID );
					if( matID >= 0 && matID < pMatTable->size() )					
						pMatName = pMaterial->GetName();					
					Warning( "Warning: Removed placement decal at (%4.2f, %4.2f, %4.2f) with invalid material \"%s\"!\n", pChunk->m_pos.x, pChunk->m_pos.y, pChunk->m_pos.z, pMatName );
					pObj->ReleaseNode();
				} 
				else
				{
					Get3DEngine()->RegisterEntity( pObj );
					GetObjManager()->m_decalsToPrecreate.push_back( pObj );
				}
			}
			else if( eERType_WaterVolume == eType )
			{
				// read common info
				SWaterVolumeChunk* pChunk( StepData<SWaterVolumeChunk>( pPtr ) );
				if (!CheckRenderFlagsMinSpec(pChunk->m_dwRndFlags))
        {
          for( int j( 0 ); j < pChunk->m_numVertices; ++j )
          {
            SWaterVolumeVertex* pVertex( StepData<SWaterVolumeVertex>( pPtr ) );
          }

          for( int j( 0 ); j < pChunk->m_numVerticesPhysAreaContour; ++j )
          {
            SWaterVolumeVertex* pVertex( StepData<SWaterVolumeVertex>( pPtr ) );
          }

					continue;
        }

				CWaterVolumeRenderNode* pObj( new CWaterVolumeRenderNode() );

				// read common node data
        pObj->SetBBox(pChunk->m_WSBBox);
//				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,m_ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,m_ucLodRatio);
				//COPY_MEMBER(pObj,pChunk,m_nInternalFlags);
				pObj->m_dwRndFlags &= ~(ERF_HIDDEN|ERF_SELECTED);

				// read vertices
				std::vector<Vec3> vertices;
				vertices.reserve( pChunk->m_numVertices );
				for( int j( 0 ); j < pChunk->m_numVertices; ++j )
				{
					SWaterVolumeVertex* pVertex( StepData<SWaterVolumeVertex>( pPtr ) );
					vertices.push_back( pVertex->m_xyz );
				}

				// read physics area contour vertices
				std::vector<Vec3> physicsAreaContour;
				physicsAreaContour.reserve( pChunk->m_numVerticesPhysAreaContour );
				for( int j( 0 ); j < pChunk->m_numVerticesPhysAreaContour; ++j )
				{
					SWaterVolumeVertex* pVertex( StepData<SWaterVolumeVertex>( pPtr ) );
					physicsAreaContour.push_back( pVertex->m_xyz );
				}

				Plane fog;
#if defined(XENON) || defined(PS3)
				memcpy(&fog, &pChunk->m_fogPlane, sizeof(Plane));
#else
				fog = pChunk->m_fogPlane;
#endif


				// create volume 
				switch( pChunk->m_volumeType )
				{
				case IWaterVolumeRenderNode::eWVT_River:
					{
						pObj->CreateRiver( pChunk->m_volumeID, &vertices[0], pChunk->m_numVertices, pChunk->m_uTexCoordBegin, pChunk->m_uTexCoordEnd, Vec2( pChunk->m_surfUScale, pChunk->m_surfVScale ), fog/*pChunk->m_fogPlane*/ );				
						break;
					}
				case IWaterVolumeRenderNode::eWVT_Area:
					{
						pObj->CreateArea( pChunk->m_volumeID, &vertices[0], pChunk->m_numVertices, Vec2( pChunk->m_surfUScale, pChunk->m_surfVScale ),  fog/*pChunk->m_fogPlane*/ );
						break;
					}
				case IWaterVolumeRenderNode::eWVT_Ocean:
					{
						assert( !"COctreeNode::SerializeObjects( ... ) -- Water volume of type \"Ocean\" not supported yet!" );
						break;
					}
				default:
					{
						assert( !"COctreeNode::SerializeObjects( ... ) -- Invalid water volume type!" );
						break;
					}
				}

				// set material
				IMaterial* pMaterial( CObjManager::GetItemPtr(pMatTable, pChunk->m_materialID) );
				assert( pMaterial );

				// set properties
				pObj->SetFogDensity( pChunk->m_fogDensity );
				pObj->SetFogColor( pChunk->m_fogColor );

				// set physics
				if( !physicsAreaContour.empty() )
				{
					switch( pChunk->m_volumeType )
					{
					case IWaterVolumeRenderNode::eWVT_River:
						{
							pObj->SetRiverPhysicsArea( &physicsAreaContour[0], physicsAreaContour.size(), pChunk->m_volumeDepth, pChunk->m_streamSpeed );
							break;
						}
					case IWaterVolumeRenderNode::eWVT_Area:
						{
							pObj->SetAreaPhysicalArea( &physicsAreaContour[0], physicsAreaContour.size(), pChunk->m_volumeDepth, pChunk->m_streamSpeed );
							break;
						}
					case IWaterVolumeRenderNode::eWVT_Ocean:
						{
							assert( !"COctreeNode::SerializeObjects( ... ) -- Water volume of type \"Ocean\" not supported yet!" );
							break;
						}
					default:
						{
							assert( !"COctreeNode::SerializeObjects( ... ) -- Invalid water volume type!" );
							break;
						}
					}

					pObj->Physicalize();
				}

				// set material
				pObj->SetMaterial( pMaterial );

				Get3DEngine()->RegisterEntity( pObj );					
			}
			else if(eERType_DistanceCloud == eType)
			{
				SDistanceCloudChunk* pChunk(StepData<SDistanceCloudChunk>(pPtr));
				if (!CheckRenderFlagsMinSpec(pChunk->m_dwRndFlags))
					continue;

				CDistanceCloudRenderNode* pObj(new CDistanceCloudRenderNode());

				// common node data
        pObj->SetBBox(pChunk->m_WSBBox);
//				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,m_ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,m_ucLodRatio);
				//COPY_MEMBER(pObj,pChunk,m_nInternalFlags);
				pObj->m_dwRndFlags &= ~(ERF_HIDDEN|ERF_SELECTED);

				// distance cloud properties
				SDistanceCloudProperties properties;

				properties.m_pos = pChunk->m_pos;
				properties.m_sizeX = pChunk->m_sizeX;
				properties.m_sizeY = pChunk->m_sizeY;
				properties.m_rotationZ = pChunk->m_rotationZ;

				IMaterial* pMaterial( CObjManager::GetItemPtr(pMatTable, pChunk->m_materialID) );
				assert(pMaterial);
				properties.m_pMaterialName = pMaterial->GetName();

				pObj->SetProperties(properties);

				if (pObj->GetMaterial() != pMaterial)
				{
					const char* pMatName("_can't_resolve_material_name_");										
					int32 matID(pChunk->m_materialID);
					if(matID >= 0 && matID < (*pMatTable).size())					
						pMatName = pMaterial->GetName();					
					Warning("Warning: Removed distance cloud at (%4.2f, %4.2f, %4.2f) with invalid material \"%s\"!\n", pChunk->m_pos.x, pChunk->m_pos.y, pChunk->m_pos.z, pMatName);
					pObj->ReleaseNode();
				} 
				else
				{
					Get3DEngine()->RegisterEntity( pObj );
				}
			}
			else if( eERType_WaterWave == eType )
      {      
        // read common info
        SWaterWaveChunk* pChunk( StepData<SWaterWaveChunk>( pPtr ) );
				if (!CheckRenderFlagsMinSpec(pChunk->m_dwRndFlags))
        {
          for( int j( 0 ); j < pChunk->m_nVertexCount; ++j )
          {
            Vec3* pVertex( StepData<Vec3>( pPtr ) );
          }

					continue;
        }

				CWaterWaveRenderNode* pObj( new CWaterWaveRenderNode() );

        // read common node data
        pObj->SetBBox(pChunk->m_WSBBox);
//        COPY_MEMBER(pObj,pChunk,GetRadius());
        COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
        COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
        COPY_MEMBER(pObj,pChunk,m_ucViewDistRatio);
        COPY_MEMBER(pObj,pChunk,m_ucLodRatio);
        //COPY_MEMBER(pObj,pChunk,m_nInternalFlags);
        pObj->m_dwRndFlags &= ~(ERF_HIDDEN|ERF_SELECTED);

        // read vertices
        std::vector<Vec3> pVertices;
        pVertices.reserve( pChunk->m_nVertexCount );
        for( int j( 0 ); j < pChunk->m_nVertexCount; ++j )
        {
          Vec3* pVertex( StepData<Vec3>( pPtr ) );
          pVertices.push_back( *pVertex );
        }

        // set material
        IMaterial* pMaterial( CObjManager::GetItemPtr(pMatTable, pChunk->m_nMaterialID) );
        assert( pMaterial );

        // set material
        pObj->SetMaterial( pMaterial );

        SWaterWaveParams pParams;
        pParams.m_fSpeed = pChunk->m_fSpeed;
        pParams.m_fSpeedVar = pChunk->m_fSpeedVar;
        pParams.m_fLifetime = pChunk->m_fLifetime;
        pParams.m_fLifetimeVar = pChunk->m_fLifetimeVar;
        pParams.m_fPosVar = pChunk->m_fPosVar;
        pParams.m_fHeight = pChunk->m_fHeight;
        pParams.m_fHeightVar = pChunk->m_fHeightVar;
        pParams.m_fCurrLifetime = pChunk->m_fCurrLifetime;          
        pParams.m_fCurrFrameLifetime = pChunk->m_fCurrFrameLifetime;
        pParams.m_fCurrSpeed = pChunk->m_fCurrSpeed;
        pParams.m_fCurrHeight = pChunk->m_fCurrHeight;

        pObj->SetParams(pParams);

        Vec2 vScaleUV( pChunk->m_fUScale, pChunk->m_fVScale );
        pObj->Create( pChunk->m_nID, &pVertices[0], pChunk->m_nVertexCount, vScaleUV, pChunk->m_pWorldTM);

        //Get3DEngine()->RegisterEntity( pObj );					
      }    
			else if(eERType_AutoCubeMap == eType)
			{
				SAutoCubeMapChunk* pChunk(StepData<SAutoCubeMapChunk>(pPtr));
				if (!CheckRenderFlagsMinSpec(pChunk->m_dwRndFlags))
					continue;

				CAutoCubeMapRenderNode* pObj(new CAutoCubeMapRenderNode());

				// common node data
        pObj->SetBBox(pChunk->m_WSBBox);
//				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,m_ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,m_ucLodRatio);
				//COPY_MEMBER(pObj,pChunk,m_nInternalFlags);
				pObj->m_dwRndFlags &= ~(ERF_HIDDEN|ERF_SELECTED);

				// auto cube map properties
				SAutoCubeMapProperties properties;
				properties.m_flags = pChunk->m_flags;
				properties.m_obb.SetOBB(Matrix33(pChunk->m_oobRot), pChunk->m_oobH, pChunk->m_oobC);
				
				pObj->SetProperties(properties);

				Get3DEngine()->RegisterEntity( pObj );
			}
			else
				assert(!"Unsupported object type");
		}
	}

	return nBlockSize;
}

int COctreeNode::Load(FILE * & f, int & nDataSize, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable) { return Load_T(f, nDataSize, pStatObjTable, pMatTable); }
int COctreeNode::Load(uchar * & f, int & nDataSize, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable) { return Load_T(f, nDataSize, pStatObjTable, pMatTable); }

template <class T>
int COctreeNode::Load_T(T * & f, int & nDataSize, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable)
{
  SOcTreeNodeChunk chunk;
  if(!CTerrain::LoadDataFromFile(&chunk, 1, f, nDataSize))
    return 0;

  assert(chunk.nChunkVersion == OCTREENODE_CHUNK_VERSION);
  if(chunk.nChunkVersion != OCTREENODE_CHUNK_VERSION)
    return 0;


  if(chunk.nObjectsBlockSize)
  {
    // TODO: do not reallocate mem
    m_mbLoadingCache.Allocate(chunk.nObjectsBlockSize);

    if(!CTerrain::LoadDataFromFile((uchar*)m_mbLoadingCache.GetData(), chunk.nObjectsBlockSize, f, nDataSize))
      return 0;

    if(!m_bEditorMode)
      SerializeObjects(false,&m_mbLoadingCache, pStatObjTable, pMatTable);
  }

  // count number of nodes loaded
  int nNodesNum = 1;

  // process childs
  for(int nChildId=0; nChildId<8; nChildId++)
  {
    if(chunk.ucChildsMask & (1<<nChildId))
    {
      if(!m_arrChilds[nChildId])
        m_arrChilds[nChildId] = new COctreeNode(GetChildBBox(nChildId), m_pVisArea, this);

      int nNewNodesNum = m_arrChilds[nChildId]->Load_T(f, nDataSize, pStatObjTable, pMatTable);

      if(!nNewNodesNum)
        return 0; // data error

      nNodesNum += nNewNodesNum;
    }
  }

  return nNodesNum;
}

int COctreeNode::GetData(byte * & pData, int & nDataSize, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable)
{
	if(pData)
	{
		// get node data
		SOcTreeNodeChunk * pCunk = (SOcTreeNodeChunk *)pData;
		pCunk->nChunkVersion = OCTREENODE_CHUNK_VERSION;
		pCunk->nodeBox = m_nodeBox;
		
		// fill ChildsMask
		pCunk->ucChildsMask = 0;
		for(int i=0; i<8; i++)
			if(m_arrChilds[i])
				pCunk->ucChildsMask |= (1<<i);

		UPDATE_PTR_AND_SIZE(pData,nDataSize,sizeof(SOcTreeNodeChunk));

		CMemoryBlock memblock;
		SerializeObjects(true,&memblock,pStatObjTable,pMatTable);

		pCunk->nObjectsBlockSize = memblock.GetSize();

		memcpy(pData, memblock.GetData(), memblock.GetSize()); 
		UPDATE_PTR_AND_SIZE(pData,nDataSize,memblock.GetSize());
	}
	else // just count size
	{
		nDataSize += sizeof(SOcTreeNodeChunk);
		nDataSize += SerializeObjects(true,NULL,NULL,NULL);
	}

	// count number of nodes loaded
	int nNodesNum = 1;

	// process childs
	for(int i=0; i<8; i++)
		if(m_arrChilds[i])
			nNodesNum += m_arrChilds[i]->GetData(pData, nDataSize, pStatObjTable, pMatTable);

	return nNodesNum;
}

bool COctreeNode::CleanUpTree()
{
	bool bChildObjectsFound = false;
	for(int i=0; i<8; i++)
	{
		if(m_arrChilds[i])
		{
			if(!m_arrChilds[i]->CleanUpTree())
			{
				delete m_arrChilds[i];
				m_arrChilds[i] = NULL;
			}
			else
				bChildObjectsFound = true;
		}
	}

	// update max view distances

	m_fObjectsMaxViewDist = 0.f;
	m_objectsBox = m_nodeBox;

  for(IRenderNode * pObj = m_lstObjects.m_pFirstNode; pObj; pObj = pObj->m_pNext)
	{
		pObj->m_fWSMaxViewDist = pObj->GetMaxViewDist();
		m_fObjectsMaxViewDist = max(m_fObjectsMaxViewDist, pObj->m_fWSMaxViewDist);
		m_objectsBox.Add(pObj->GetBBox());
	}

	for(int i=0; i<8; i++)
	{
		if(m_arrChilds[i])
		{
			m_fObjectsMaxViewDist = max(m_fObjectsMaxViewDist, m_arrChilds[i]->m_fObjectsMaxViewDist);
			m_objectsBox.Add(m_arrChilds[i]->m_objectsBox);
		}
	}

	return (bChildObjectsFound || m_lstObjects.m_pFirstNode);
}

void COctreeNode::FreeLoadingCache() 
{ 
  m_mbLoadingCache.Free(); 
}
