////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjman.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: objects container, streaming, common part for indoor and outdoor sectors
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

CBasicArea::~CBasicArea()
{
	delete m_pObjectsTree;
	m_pObjectsTree = NULL;
/*
	// unregister dynamic entities
	while(m_lstEntities[DYNAMIC_OBJECTS].Count())
	{
		EERType eType = m_lstEntities[DYNAMIC_OBJECTS].GetAt(0).pNode->GetRenderNodeType();
		assert(eType != eERType_Brush && eType != eERType_Vegetation);
		int nCountBefore = m_lstEntities[DYNAMIC_OBJECTS].Count();
		//		delete m_lstEntities[DYNAMIC_OBJECTS].GetAt(0); // will also remove it from this list
		Get3DEngine()->UnRegisterEntity(m_lstEntities[DYNAMIC_OBJECTS].GetAt(0).pNode);
		assert(m_lstEntities[DYNAMIC_OBJECTS].Count() == (nCountBefore-1));
	}
	assert(m_lstEntities[DYNAMIC_OBJECTS].Count()==0);

*/
}


/*int CBasicArea::GetLastStaticElementIdWithInMaxViewDist(float fMaxViewDist)
{ // use binary search to find range of elements visible on fMaxViewDist
	int nCount = m_lstEntities[STATIC_OBJECTS].Count();
	int nCurrId = m_lstEntities[STATIC_OBJECTS].Count()/2;
	for(int nJump=2; (nCount>>nJump); nJump++)
	{
		IRenderNode * pCaster = m_lstEntities[STATIC_OBJECTS][nCurrId];
		if(pCaster->m_fWSMaxViewDist<fMaxViewDist)
			nCurrId -= nCount>>nJump;
		else
			nCurrId += nCount>>nJump;
	}

	return min(nCurrId+2,m_lstEntities[STATIC_OBJECTS].Count());
}*/

/*int CBasicArea::SerializeObjectsIntoMemBlock(bool bSave, CMemoryBlock * pMemBlock)
{
	int nBlockSize=0;

	if(bSave)
	{
		for(int i=0; i<m_lstEntities[STATIC_OBJECTS].Count(); i++)
		{
			EERType eType = m_lstEntities[STATIC_OBJECTS].GetAt(i).pNode->GetRenderNodeType();
			if(eType == eERType_Vegetation)
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SVegetationChunk);
			}
			else if(eType == eERType_Brush)
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SBrushChunk);
			}
			else if(eType == eERType_VoxelObject)
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SVoxelObjectChunk);
			}
			else if(eType == eERType_RoadObject)
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SRoadChunk);
			}
			else if(eType == eERType_Decal)
			{
				nBlockSize += sizeof(eType);
				nBlockSize += sizeof(SDecalChunk);
			}
		//	else
			//	assert(!"Unsupported object type");
		}

		if(pMemBlock)
		{
			pMemBlock->Allocate(nBlockSize);
			byte * pPtr = (byte *)pMemBlock->GetData();

			for(int i=0; i<m_lstEntities[STATIC_OBJECTS].Count(); i++)
			{
				EERType eType = m_lstEntities[STATIC_OBJECTS].GetAt(i).pNode->GetRenderNodeType();

				IRenderNode * pEnt = m_lstEntities[STATIC_OBJECTS].GetAt(i).pNode;
				if(eType == eERType_Brush)
				{
					memcpy(pPtr,&eType,sizeof(eType)); pPtr += sizeof(eType);

					CBrush * pObj = (CBrush*)pEnt;

					SBrushChunk chunk;

					// common node data
					COPY_MEMBER(&chunk,pObj,m_WSBBox);
					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,ucLodRatio);
					COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					// brush data
					COPY_MEMBER(&chunk,pObj,m_vPos); 
//					COPY_MEMBER(&chunk,pObj,m_vAngles);
					COPY_MEMBER(&chunk,pObj,m_fScale);
					COPY_MEMBER(&chunk,pObj,m_Matrix);
					COPY_MEMBER(&chunk,pObj,m_nMaterialId);
					COPY_MEMBER(&chunk,pObj,m_nEditorObjectId);
					COPY_MEMBER(&chunk,pObj,m_nObjectTypeID);

					memcpy(pPtr,&chunk,sizeof(chunk)); pPtr += sizeof(chunk);
				}
				else if(eType == eERType_Vegetation)
				{
					memcpy(pPtr,&eType,sizeof(eType)); pPtr += sizeof(eType);

					CVegetation * pObj = (CVegetation*)pEnt;

					SVegetationChunk chunk;

					// common node data
					COPY_MEMBER(&chunk,pObj,m_WSBBox);
					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,ucLodRatio);
					COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					// vegetation data
					COPY_MEMBER(&chunk,pObj,m_vPos);
					COPY_MEMBER(&chunk,pObj,m_fScale);
					COPY_MEMBER(&chunk,pObj,m_nObjectTypeID);
					COPY_MEMBER(&chunk,pObj,m_ucBright);
					COPY_MEMBER(&chunk,pObj,m_ucAngle);

					memcpy(pPtr,&chunk,sizeof(chunk)); pPtr += sizeof(chunk);
				}
				else if(eType == eERType_VoxelObject)
				{
					memcpy(pPtr,&eType,sizeof(eType)); pPtr += sizeof(eType);

					CVoxelObject * pObj = (CVoxelObject*)pEnt;

					SVoxelObjectChunk chunk;

					// common node data
					COPY_MEMBER(&chunk,pObj,m_WSBBox);
					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,ucLodRatio);
					COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					// voxel data
					COPY_MEMBER(&chunk,pObj,m_Matrix);
					SVoxelChunk dataChunk;
					pObj->GetData(dataChunk,Vec3(0,0,0));
					chunk.m_VoxData = dataChunk;

					memcpy(pPtr,&chunk,sizeof(chunk)); pPtr += sizeof(chunk);
				}
				else if(eType == eERType_RoadObject)
				{
					memcpy(pPtr,&eType,sizeof(eType)); pPtr += sizeof(eType);

					CRoadRenderNode * pObj = (CRoadRenderNode*)pEnt;

					SRoadChunk chunk;

					// common node data
					COPY_MEMBER(&chunk,pObj,m_WSBBox);
					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,ucLodRatio);
					COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					// road data
					COPY_MEMBER(&chunk,pObj,m_nMaterialId);
					
					for(int i=0; i<4; i++)
						COPY_MEMBER(&chunk,pObj,m_arrVerts[i]);
					
					for(int i=0; i<2; i++)
						COPY_MEMBER(&chunk,pObj,m_arrTexCoors[i]);

					memcpy(pPtr,&chunk,sizeof(chunk)); pPtr += sizeof(chunk);
				}
				else if( eERType_Decal == eType )
				{					
					memcpy( pPtr, &eType, sizeof( eType ) ); 
					pPtr += sizeof( eType );

					CDecalRenderNode* pObj( (CDecalRenderNode*) pEnt );

					SDecalChunk chunk;

					// common node data
					COPY_MEMBER(&chunk,pObj,m_WSBBox);
					COPY_MEMBER(&chunk,pObj,GetRadius());
					COPY_MEMBER(&chunk,pObj,m_fWSMaxViewDist);
					COPY_MEMBER(&chunk,pObj,m_dwRndFlags); 
					COPY_MEMBER(&chunk,pObj,ucViewDistRatio);
					COPY_MEMBER(&chunk,pObj,ucLodRatio);
					COPY_MEMBER(&chunk,pObj,m_nInternalFlags);

					// decal properties
					const SDecalProperties* pDecalProperties( pObj->GetDecalProperties() );
					assert( pDecalProperties );
					chunk.m_projectionType = pDecalProperties->m_projectionType;
					chunk.m_pos = pDecalProperties->m_pos;
					chunk.m_normal = pDecalProperties->m_normal;
					chunk.m_explicitRightUpFront = pDecalProperties->m_explicitRightUpFront;
					chunk.m_radius = pDecalProperties->m_radius;
					chunk.m_materialID = pObj->GetMaterialID();

					memcpy( pPtr, &chunk, sizeof( chunk ) ); 
					pPtr += sizeof( chunk );
				}
//				else
	//				assert(!"Unsupported object type");
			}

			assert(pPtr == (byte *)pMemBlock->GetData()+nBlockSize);
		}
	}
	else
	{ // Load objects
		assert(m_lstEntities[STATIC_OBJECTS].Count()==0);

		m_lstEntities[STATIC_OBJECTS].Reset();

		m_eSStatus = eSStatus_Ready;

		byte * pPtr = (byte *)pMemBlock->GetData();
		byte * pEndPtr = pPtr + pMemBlock->GetSize();

		while(pPtr < pEndPtr)
		{
			EERType eType = *StepData<EERType>(pPtr);

			// For these structs, our Endian swapping is built in to the member copy.
			if(eType == eERType_Brush) 
			{
				CBrush * pObj = new CBrush();

				SBrushChunk * pChunk = StepData<SBrushChunk>(pPtr);

				// common node data
				COPY_MEMBER(pObj,pChunk,m_WSBBox);
				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,ucLodRatio);
				COPY_MEMBER(pObj,pChunk,m_nInternalFlags);

				// brush data
				COPY_MEMBER(pObj,pChunk,m_vPos); 
//				COPY_MEMBER(pObj,pChunk,m_vAngles);
				COPY_MEMBER(pObj,pChunk,m_fScale);
				COPY_MEMBER(pObj,pChunk,m_Matrix);
				COPY_MEMBER(pObj,pChunk,m_nMaterialId);
				COPY_MEMBER(pObj,pChunk,m_nEditorObjectId);
				COPY_MEMBER(pObj,pChunk,m_nObjectTypeID);

				// set material
				pObj->SetMaterial(GetObjManager()->GetObjectMaterialFromId(pObj->GetMaterialId()));

				pObj->Physicalize();
				Get3DEngine()->RegisterEntity(pObj);
				assert(pObj->GetEntityVisArea() == this);
			}
			else if(eType == eERType_Vegetation)
			{
				CVegetation * pObj = new CVegetation();

				SVegetationChunk * pChunk = StepData<SVegetationChunk>(pPtr);

				// common node data
				COPY_MEMBER(pObj,pChunk,m_WSBBox);
				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,ucLodRatio);
				COPY_MEMBER(pObj,pChunk,m_nInternalFlags);

				// vegetation data
				COPY_MEMBER(pObj,pChunk,m_vPos);
				COPY_MEMBER(pObj,pChunk,m_fScale);
				COPY_MEMBER(pObj,pChunk,m_nObjectTypeID);
				COPY_MEMBER(pObj,pChunk,m_ucBright);
				COPY_MEMBER(pObj,pChunk,m_ucAngle);

				pObj->Physicalize();
				Get3DEngine()->RegisterEntity(pObj);

				assert(pObj->GetEntityVisArea() == this);
			}
			else if(eType == eERType_VoxelObject)
			{
				CVoxelObject * pObj = new CVoxelObject(NULL,Vec3(0,0,0));

				SVoxelObjectChunk * pChunk = StepData<SVoxelObjectChunk>(pPtr);

				// common node data
				COPY_MEMBER(pObj,pChunk,m_WSBBox);
				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,ucLodRatio);
				COPY_MEMBER(pObj,pChunk,m_nInternalFlags);

				// voxel data
				COPY_MEMBER(pObj,pChunk,m_Matrix);
				pObj->SetData(&pChunk->m_VoxData);
				pObj->SubmitVoxelSpace();

				// rebuild
				pObj->ScheduleRebuild();

//				pObj->Physicalize();
				Get3DEngine()->RegisterEntity(pObj);
				assert(pObj->GetEntityVisArea() == this);
			}
			else if(eType == eERType_RoadObject) 
			{
				CRoadRenderNode * pObj = new CRoadRenderNode();

				SRoadChunk * pChunk = StepData<SRoadChunk>(pPtr);

				// common node data
				COPY_MEMBER(pObj,pChunk,m_WSBBox);
				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,ucLodRatio);
				COPY_MEMBER(pObj,pChunk,m_nInternalFlags);

				// road data
				COPY_MEMBER(pObj,pChunk,m_nMaterialId);

				for(int i=0; i<4; i++)
					COPY_MEMBER(pObj,pChunk,m_arrVerts[i]);

				for(int i=0; i<2; i++)
					COPY_MEMBER(pObj,pChunk,m_arrTexCoors[i]);

				pObj->SetVertices(pObj->m_arrVerts, 4, pObj->m_arrTexCoors[0], pObj->m_arrTexCoors[1]);

				// set material
				pObj->SetMaterial(GetObjManager()->GetObjectMaterialFromId(pObj->GetMaterialId()));

				Get3DEngine()->RegisterEntity(pObj);
				assert(pObj->GetEntityVisArea() == this);
			}
			else if( eERType_Decal == eType )
			{
				CDecalRenderNode* pObj( new CDecalRenderNode() );

				SDecalChunk* pChunk( StepData<SDecalChunk>( pPtr ) );

				// common node data
				COPY_MEMBER(pObj,pChunk,m_WSBBox);
				COPY_MEMBER(pObj,pChunk,GetRadius());
				COPY_MEMBER(pObj,pChunk,m_fWSMaxViewDist);
				COPY_MEMBER(pObj,pChunk,m_dwRndFlags); 
				COPY_MEMBER(pObj,pChunk,ucViewDistRatio);
				COPY_MEMBER(pObj,pChunk,ucLodRatio);
				COPY_MEMBER(pObj,pChunk,m_nInternalFlags);

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
				properties.m_explicitRightUpFront = pChunk->m_explicitRightUpFront;
				properties.m_radius = pChunk->m_radius;				
				
				IMaterial* pMaterial( GetObjManager()->GetObjectMaterialFromId( pChunk->m_materialID ) );
				assert( pMaterial );
				properties.m_pMaterialName = pMaterial->GetName();
				
				pObj->SetDecalProperties( properties );

				if( pObj->GetMaterialID() != pChunk->m_materialID )
				{
					const char* pMatName( "_can't_resolve_material_name_" );										
					int32 matID( pChunk->m_materialID );
					if( matID >= 0 && matID < GetObjManager()->m_lstObjectMaterials.Count() )					
						pMatName = &GetObjManager()->m_lstObjectMaterials[ matID ].szMatName[ 0 ];					
					Warning( "Warning: Removed placement decal at (%4.2f, %4.2f, %4.2f) with invalid material \"%s\"!\n", pChunk->m_pos.x, pChunk->m_pos.y, pChunk->m_pos.z, pMatName );
					SAFE_RELEASE( pObj );
				} 
				else
				{
					Get3DEngine()->RegisterEntity( pObj );
					GetObjManager()->m_decalsToPrecreate.push_back( pObj );
					assert( pObj->GetEntityVisArea() == this );
				}
			}
			else
				assert(!"Unsupported object type");
		}
	}

	return nBlockSize;
}*/
/*
void CBasicArea::UpdatePVS()
{
	FUNCTION_PROFILER_3DENGINE;

	if(!GetCVars()->e_cbuffer_pvs || !GetCVars()->e_cbuffer)
	{
		delete [] m_pPVS;
		m_pPVS = NULL;
		return;
	}

	if(m_nPVSSelCellSize != GetCVars()->e_cbuffer_pvs_cell_size)
	{
		delete [] m_pPVS;
		m_pPVS = NULL;
		m_nPVSSelCellSize = GetCVars()->e_cbuffer_pvs_cell_size;
	}

	m_vPVSGridSize.x = (uint)max(1.f, (m_boxArea.max.x-m_boxArea.min.x)/m_nPVSSelCellSize);
	m_vPVSGridSize.y = (uint)max(1.f, (m_boxArea.max.y-m_boxArea.min.y)/m_nPVSSelCellSize);
	m_vPVSGridSize.z = (uint)max(1.f, (m_boxArea.max.z-m_boxArea.min.z)/m_nPVSSelCellSize);

	m_vPVSCellSize.x = (m_boxArea.max.x-m_boxArea.min.x)/m_vPVSGridSize.x;
	m_vPVSCellSize.y = (m_boxArea.max.y-m_boxArea.min.y)/m_vPVSGridSize.y;
	m_vPVSCellSize.z = (m_boxArea.max.z-m_boxArea.min.z)/m_vPVSGridSize.z;

	if(!m_pPVS)
		m_pPVS = new SCell[m_vPVSGridSize.x*m_vPVSGridSize.y*m_vPVSGridSize.z];
}
*/

/*
bool CBasicArea::IsObjectOccludedInPVS(IRenderNode * pNode, int & nLMask)
{
	if(!m_pPVS)
		return false;

	AABB ASBox = pNode->m_WSBBox; 
	ASBox.min -= m_boxArea.min;
	ASBox.max -= m_boxArea.min;

	int x1 = CLAMP((int)floor(ASBox.min.x/m_vPVSCellSize.x), 0, m_vPVSGridSize.x);
	int y1 = CLAMP((int)floor(ASBox.min.y/m_vPVSCellSize.y), 0, m_vPVSGridSize.y);
	int z1 = CLAMP((int)floor(ASBox.min.z/m_vPVSCellSize.z), 0, m_vPVSGridSize.z);

	int x2 = CLAMP((int)ceil (ASBox.max.x/m_vPVSCellSize.x), 0, m_vPVSGridSize.x);
	int y2 = CLAMP((int)ceil (ASBox.max.y/m_vPVSCellSize.y), 0, m_vPVSGridSize.y);
	int z2 = CLAMP((int)ceil (ASBox.max.z/m_vPVSCellSize.z), 0, m_vPVSGridSize.z);

	uint nZoneLightMask = 0;
	bool bOccluded = true;

	for(int x=x1; x<x2; x++)
	for(int y=y1; y<y2; y++)
	for(int z=z1; z<z2; z++)
	{
		SCell & rCell = m_pPVS[x*m_vPVSGridSize.y*m_vPVSGridSize.z + y*m_vPVSGridSize.z + z];
		if(abs(rCell.nFrameId) != GetFrameID())
		{ // this sector is not checked yet
			Vec3 vCellMin = m_boxArea.min + Vec3(m_vPVSCellSize.x*x,m_vPVSCellSize.y*y,m_vPVSCellSize.z*z);
			Vec3 vCellMax = vCellMin + Vec3(m_vPVSCellSize.x,m_vPVSCellSize.y,m_vPVSCellSize.z);

			rCell.nLightMaskAffecting = 0;

			// update vis frame id
			if(Get3DEngine()->GetCoverageBuffer()->IsBBoxVisible(vCellMin, vCellMax))
			{
				rCell.nFrameId = GetFrameID();

				// update light mask
				PodArray<CDLight> * pLights = Get3DEngine()->GetDynamicLightSources();
				for(int nId=0; nId<Get3DEngine()->GetRealLightsNum(); nId++)
				{
					CDLight * pLight = pLights->Get(nId);
					CLightEntity * pEnt = (CLightEntity*)pLight->m_pOwner;
					if(!(pLight->m_Flags&DLF_SUN) && pEnt->IsBoxAffected(AABB(vCellMin,vCellMax)))
					{
						rCell.nLightMaskAffecting += (1<<nId);

						if(GetCVars()->e_cbuffer_pvs>1)
							GetRenderer()->GetIRenderAuxGeom()->DrawAABB(AABB(vCellMin, vCellMax),false,ColorB(0,0,255,255),eBBD_Faceted);
					}
				}
			}
			else
				rCell.nFrameId = - GetFrameID();
		}

		nZoneLightMask |= rCell.nLightMaskAffecting;

		if(rCell.nFrameId == GetFrameID())
			bOccluded = false;
	}
	
	nLMask &= nZoneLightMask;

	if(nLMask && pNode == (IRenderNode*)0x039ee098)
		int o=0;

	return bOccluded;
}
*/
