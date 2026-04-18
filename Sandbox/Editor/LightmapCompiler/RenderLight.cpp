/*=============================================================================
RenderLight.cpp : 
Copyright 2004 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Nick Kasyan
=============================================================================*/
#include "stdafx.h"
#include <ILMSerializationManager.h>
#include <float.h>
#include <direct.h>
#include "I3dEngine.h"
#include <CryArray.h>
#include "RenderLight.h"
#include "IEntityRenderState.h"
#include <AABBSV.h>
#include <IIndexedMesh.h>

#include"PhotonMap.h"
#include"PhotonTrace.h"

#include"RayMap.h"
#include"AreaLight.h"

#include "3DSamplerCompiler.h"

#include <set>

//Debug defines
//#define SKIP_DISTRIBUTED_SAVE_OCTREE	1
//#define SKIP_DISTRIBUTED_SAVE	1

//#define SKIP_PACKING

//#define ENABLE_PACK_SAVE
//#define	RASTERIZE_DEBUGCOLORS

//#define RENDER_OCCLUSIONMAP 1

#define	LC_MAX_OPEN_SURFACE 2					//NEVER USE 0!

// Needs to be somewhere
ICompilerProgress *g_pIProgress = NULL;

//FIX::
CGeomCore* g_pGC;

CPhotonMap* g_pGlobalMap;
CPhotonMap* g_pCausticMap;

//===================================================================================
// Method				:	CLightScene
// Description	:	Constructor
//===================================================================================
CLightScene::CLightScene() : m_pISerializationManager(NULL),m_pOccluderGrid(NULL),m_pOccluderGridCellSize(NULL),
pDirectIllumStatement(0),
pIndirectIllumStatement(0),
pSolarIllumStatement(0),
pOcclusionIllumStatement(0),
pAmbientOcclusionIllumStatement(0)
{
	memset(&m_IndInterface, 0, sizeof(IndoorBaseInterface));
}

//===================================================================================
// Method				:	CLightScene
// Description	:	Destructor
//===================================================================================
CLightScene::~CLightScene()
{
	if (m_pISerializationManager)
	{
		m_pISerializationManager->Release();
		m_pISerializationManager = NULL;
	}

	if( m_pOccluderGrid )
	{
		uint32 nSize = m_nOccluderGridDeepness*m_nOccluderGridHeight*m_nOccluderGridWith;
		for( uint32 i = 0; i < nSize; ++i )
		{
			SAFE_DELETE_ARRAY( m_pOccluderGrid[i] );
		}
		SAFE_DELETE_ARRAY( m_pOccluderGrid );
	}
	SAFE_DELETE_ARRAY( m_pOccluderGridCellSize );

	SAFE_DELETE(pDirectIllumStatement);
	SAFE_DELETE(pIndirectIllumStatement);
	SAFE_DELETE(pSolarIllumStatement);
	SAFE_DELETE(pOcclusionIllumStatement);
	SAFE_DELETE(pAmbientOcclusionIllumStatement);

}

bool CLightScene::AddRLMesh(IRenderNode* pRenderNode,IRenderMesh* pRenderMesh,const bool bRasterize, const bool bOpaque, const f32 fLightmapQuality, const bool bOptimizeByLights, const bool bJustCreateMeshInfos, const bool bNeedToAutoGenerateTextureCoordinates,Matrix34& mNodeMatrix)
{
	if (pRenderMesh == NULL)
	{
		if( false == bJustCreateMeshInfos )
			m_BrushRenderable.push_back(0);		//not renderable
		return false;
	}

	if (pRenderMesh->IsEmpty())
	{
		if( false == bJustCreateMeshInfos )
			m_BrushRenderable.push_back(0);		//not renderable
		return false; 
	}

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	bool bAffectedByLight;

	//check is the brush affected by any valid light ? (speed,memory,texturespace optimalization)
	//not used for ram anyway, legacy stuff
	/*	if( bOptimizeByLights && !sceneCtx.m_bMakeRAE )
	{
	Vec3 vPos = pRenderNode->GetPos();
	f32 fRadius = pRenderNode->GetBBox().GetRadius();

	const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();
	assert(pListLights!=NULL);
	int32 nLightNumber			= pListLights->Count();
	int32 i;
	for( i=0; i< nLightNumber; ++i )
	{
	CDLight* pLight	=	pListLights->GetAt(i);
	if( pLight )
	{
	if (false == sceneCtx.m_bUseSunLight && (pLight->m_Flags & DLF_SUN ) )
	continue;
	if( vPos.GetDistance(pLight->m_BaseOrigin) > fRadius+pLight->m_fRadius )
	continue;

	if( !(pLight->m_Flags & DLF_LM ) && !(pLight->m_Flags & DLF_DIFFUSEOCCLUSION || pLight->m_Flags & DLF_SPECULAROCCLUSION) )
	continue;

	if( false == sceneCtx.m_bMakeLightMap && !(pLight->m_Flags & DLF_DIFFUSEOCCLUSION || pLight->m_Flags & DLF_SPECULAROCCLUSION) )
	continue;
	if( false == sceneCtx.m_bMakeOcclMap && !(pLight->m_Flags & DLF_LM) )
	continue;
	}
	break;
	}

	//no light affecting this rendernode...
	//RAE overwrite the light infos
	bAffectedByLight = ( i != nLightNumber );
	}
	else*/
	bAffectedByLight = true;


	IMaterial* pDefMaterial = pRenderNode->GetMaterial();

	bool bMeshHaveAGoodChunck = false;

	PodArray<CRenderChunk>* pRenderChunks = pRenderMesh->GetChunks();
	for (int32 nChunk=0; nChunk<pRenderChunks->Count(); nChunk++)
	{
		CRenderChunk rendChunk = (*pRenderChunks)[nChunk];

		if (rendChunk.m_nMatFlags & MTL_FLAG_NODRAW)
			continue;

		bool bRealyOpaque = bOpaque;
		SShaderItem shaderItem;
		IRenderShaderResources* pShaderResources = NULL;
		//multi-material shader retrieve
		if (pDefMaterial!= NULL)
		{
			shaderItem = pDefMaterial->GetShaderItem(rendChunk.m_nMatID);
			pShaderResources = shaderItem.m_pShaderResources;

			//no diffuse texture == no lightmap for this object (BUG: it is need a clear policy ! )
			if( NULL == pShaderResources->GetTexture(EFTT_DIFFUSE))
				continue;

			{ // test for old assets
				CString sShaderName = shaderItem.m_pShader->GetName();
				if(sShaderName.Find("templdecal") != -1)  	//do not compute decals
					continue;
				if(sShaderName.Find("nodraw") != -1)  	//do not compute decals
					continue;
			}

			//Not need, beause the designer can select from mesh paramteres to not cast shadow, if (s)he want...
			//fix:: check all opaque materials  inside CRLMesh constructor
			//check for surface opacity
			if (pShaderResources && pShaderResources->GetOpacity() != 1.0f)
				bRealyOpaque = false;
			//					continue;
			if (pShaderResources && pShaderResources->GetAlphaRef() != 0.f)
				bRealyOpaque = false;
		}

		if ( bAffectedByLight )
		{
			if( bJustCreateMeshInfos )
			{
				g_pGC->AddMeshInfos( pRenderMesh, &mNodeMatrix, pShaderResources );
			}
			else
			{
				CRLMesh* pRLMesh = new CRLMesh(pRenderMesh, &mNodeMatrix, &rendChunk, pShaderResources, bNeedToAutoGenerateTextureCoordinates, bRealyOpaque,fLightmapQuality);
				pRLMesh->m_BBox = 	pRenderNode->GetBBox();			//Setup a BBox
				m_mmapRLMeshes.insert(t_pairRLMesh(std::pair<IRenderNode*,IRenderMesh*>(pRenderNode,pRenderMesh), pRLMesh));
				bMeshHaveAGoodChunck = true;
			}
		}
	}

	if( false == bJustCreateMeshInfos )
		m_BrushRenderable.push_back( bMeshHaveAGoodChunck ? 1 : 0);		//renderable
	return true;
}

//===================================================================================
// Method				:	CreateRLMeshes
// Description	:	Create an render light mesh from rendernode
//===================================================================================
bool CLightScene::CreateRLMeshes(IRenderNode* pRenderNode, CBaseObject* pObject, const bool bRasterize, const bool bOpaque, const f32 fLightmapQuality, const bool bOptimizeByLights, const bool bJustCreateMeshInfos, const bool bNeedToAutoGenerateTextureCoordinates )
{
	if( false == bRasterize )
		return false;


	assert(pRenderNode != NULL);
	if( false == pRenderNode )
		return false;

	IRenderMesh *pRenderMesh = pRenderNode->GetRenderMesh(0);

	//Voxels vertex container used as rendermesh
	if( pObject && OBJTYPE_VOXEL == pObject->GetType() && pRenderMesh )
		pRenderMesh = pRenderMesh->GetVertexContainer();

	Matrix34 mNodeMatrix;
	pRenderNode->GetEntityStatObj(0, 0, &mNodeMatrix);

	CLogFile::FormatLine("Adding Object:\n VisArea name: %s\n", pObject->GetName() );
	//pObject->m_name	==	"Village_house1.cfg"

	if(pRenderMesh)
		return AddRLMesh(pRenderNode,pRenderMesh,bRasterize,bOpaque,fLightmapQuality,bOptimizeByLights,bJustCreateMeshInfos,bNeedToAutoGenerateTextureCoordinates,mNodeMatrix);

	//fix for multiobjects rendernode
	IStatObj* pStatObj	=	pRenderNode->GetEntityStatObj(0, 0, 0);
	if(!pStatObj)
		return false;

	bool Ret=false;
	for(uint a=0;a<pStatObj->GetSubObjectCount();a++)
	{
		IStatObj::SSubObject* pSubObject	=	pStatObj->GetSubObject(a);
		if(pSubObject->nType==STATIC_SUB_OBJECT_MESH && pSubObject->pStatObj)
		{
			if(pSubObject->pStatObj->GetRenderMesh())
				Ret|=AddRLMesh(pRenderNode,pSubObject->pStatObj->GetRenderMesh(),bRasterize,bOpaque,fLightmapQuality,bOptimizeByLights,bJustCreateMeshInfos,bNeedToAutoGenerateTextureCoordinates,pSubObject->bIdentityMatrix?mNodeMatrix:mNodeMatrix * pSubObject->tm);
		}
	}

	return Ret;
}



//===================================================================================
// Method				:	GetNeededTriangles
// Description	:	Give back how much triangle will be added to GeomCore with that RenderNode
//===================================================================================
int32 CLightScene::GetNeededTriangles(IRenderNode* pRenderNode,CBaseObject* pObject, const bool bRasterize, const bool bOpaque, const bool bOptimizeByLights)
{
	int32 nNumberOfTriangles = 0;
	Matrix34 mNodeMatrix;

	assert(pRenderNode != NULL);

	pRenderNode->GetEntityStatObj(0, 0, &mNodeMatrix);
	IRenderMesh *pRenderMesh = pRenderNode->GetRenderMesh(0);

	//Voxels vertex container used as rendermesh
	if( pObject && OBJTYPE_VOXEL == pObject->GetType() && pRenderMesh )
		pRenderMesh = pRenderMesh->GetVertexContainer();

	//fix add support of with multiobjects rendernode
	assert(pRenderMesh != NULL);
	if (pRenderMesh == NULL)
		return 0;

	if (pRenderMesh->IsEmpty())
		return 0; 

	bool bAffectedByLight;

	//check is the brush affected by any valid light ? (speed,memory,texturespace optimalization)
	if( bOptimizeByLights )
	{
		CSceneContext& sceneCtx = CSceneContext::GetInstance();

		if( sceneCtx.m_bMakeRAE )
			bAffectedByLight = true;
		else
		{
			Vec3 vPos = pRenderNode->GetPos();
			f32 fRadius = pRenderNode->GetBBox().GetRadius();

			const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();
			assert(pListLights!=NULL);
			int32 nLightNumber			= pListLights->Count();
			int32 i;
			for( i=0; i< nLightNumber; ++i )
			{
				CDLight* pLight	=	pListLights->GetAt(i);
				if( pLight )
				{
					if (false == sceneCtx.m_bUseSunLight && (pLight->m_Flags & DLF_SUN ) )
						continue;
					if( vPos.GetDistance(pLight->m_BaseOrigin) > fRadius+pLight->m_fRadius )
						continue;

					if( !(pLight->m_Flags & DLF_LM ) && !(pLight->m_Flags & DLF_DIFFUSEOCCLUSION || pLight->m_Flags & DLF_SPECULAROCCLUSION) )
						continue;

					if( false == sceneCtx.m_bMakeLightMap && !(pLight->m_Flags & DLF_DIFFUSEOCCLUSION || pLight->m_Flags & DLF_SPECULAROCCLUSION) )
						continue;
					if( false == sceneCtx.m_bMakeOcclMap && !(pLight->m_Flags & DLF_LM) )
						continue;
				}
				break;
			}

			//no light affecting this rendernode...
			bAffectedByLight = ( i != nLightNumber );
		}
	}
	else
		bAffectedByLight = true;


	IMaterial* pDefMaterial = pRenderNode->GetMaterial();

	bool bMeshHaveAGoodChunck = false;

	PodArray<CRenderChunk>* pRenderChunks = pRenderMesh->GetChunks();
	for (int32 nChunk=0; nChunk<pRenderChunks->Count(); nChunk++)
	{
		CRenderChunk rendChunk = (*pRenderChunks)[nChunk];

		if (rendChunk.m_nMatFlags & MTL_FLAG_NODRAW)
			continue;

		bool bRealyOpaque = bOpaque;
		SShaderItem shaderItem;
		IRenderShaderResources* pShaderResources = NULL;
		//multi-material shader retrieve
		if (pDefMaterial!= NULL)
		{
			shaderItem = pDefMaterial->GetShaderItem(rendChunk.m_nMatID);
			pShaderResources = shaderItem.m_pShaderResources;

			//no diffuse texture == no lightmap for this object (BUG: it is need a clear policy ! )
			if( NULL == pShaderResources->GetTexture(EFTT_DIFFUSE))
				continue;

			{ // test for old assets
				CString sShaderName = shaderItem.m_pShader->GetName();
				if(sShaderName.Find("templdecal") != -1)  	//do not compute decals
					continue;
				if(sShaderName.Find("nodraw") != -1)  	//do not compute decals
					continue;
			}

			if (pShaderResources && pShaderResources->GetOpacity() != 1.0f)
				bRealyOpaque = false;
			if (pShaderResources && pShaderResources->GetAlphaRef() != 0.f)
				bRealyOpaque = false;
			//Not need, beause the designer can select from mesh paramteres to not cast shadow, if (s)he want...
			//fix:: check all opaque materials  inside CRLMesh constructor
			//check for surface opacity
			//			if (pShaderResources->m_Opacity != 1.0f)
			//					continue;
		}

		if (bRasterize && bAffectedByLight )
			nNumberOfTriangles +=  rendChunk.nNumIndices / 3;
		else 
		{
			if( bRealyOpaque && bAffectedByLight )
				nNumberOfTriangles +=  rendChunk.nNumIndices / 3;
		}
	}

	return nNumberOfTriangles;
}


//===================================================================================
// Method				:	LoadSurfaceSpace
// Description	:	Load back all of a surface data
//===================================================================================
bool CLightScene::LoadSurfaceSpace(t_mmapRLMeshRange mmapMeshRange, CRLSurface** ppAllocRLSurface, FILE *pFile,const bool bNeedDebugInfo )
{
	//BROKEN - channel based optimalizations for occlusionmaps are not implemented.
	return false;

	assert(pFile);
	if( NULL == pFile )
		return false;

	int32 nSurfaceID,nX,nY,nDirection;

	CRLSurface* pRLSurface = NULL;
	t_mmapRLMeshes::const_iterator meshIt;
	for(meshIt = mmapMeshRange.first; meshIt!=mmapMeshRange.second; meshIt++ )
	{
		CRLMesh* rlMesh = meshIt->second;

		int16 numGroups = rlMesh->GetUnwrapGroupsNum();
		for (int16 i=0; i<numGroups; i++ )
		{
			fread( &nSurfaceID, sizeof(int32),1, pFile );
			fread( &nX, sizeof(int32),1, pFile );
			fread( &nY, sizeof(int32),1, pFile );
			fread( &nDirection, sizeof(int32),1, pFile );

			//If there aren't this surface, create it...
			if( nSurfaceID >= m_rlSurfaces.size() )
			{
				CSceneContext& sceneCtx = CSceneContext::GetInstance();
				m_rlSurfaces.push_back(new CRLSurface(sceneCtx.m_nLightmapPageWidth,sceneCtx.m_nLightmapPageHeight));
			}
			pRLSurface = m_rlSurfaces[nSurfaceID];

			pRLSurface->AddGroup(rlMesh->GetUnwrapGroups()[i], nX,nY,nDirection );
		}
	}
	if( ppAllocRLSurface )
		*ppAllocRLSurface = pRLSurface;

	return true;
}

//===================================================================================
// Method				:	AllocacteSurfaceSpace
// Description	:	Try to allocate space on a surface for the given MeshRange
//===================================================================================
bool CLightScene::AllocateSurfaceSpace(t_mmapRLMeshRange mmapMeshRange, CRLSurface** ppAllocRLSurface, FILE *pCacheFile, FILE *pFile, const bool bNeedDebugInfo, sOcclusionChannelMask* OCM, std::vector<CRLSurface*>* pSurfaceList )
{
	if( bNeedDebugInfo )
		CLogFile::FormatLine("  New mesh.\n" );

	CSceneContext& sceneCtx		 = CSceneContext::GetInstance();

	//calculate the biggest width for the mesh...
	int32 nGroupBiggestWidth = 0;
	t_mmapRLMeshes::const_iterator meshIt;
	for(meshIt = mmapMeshRange.first; meshIt!=mmapMeshRange.second; meshIt++ )
	{
		CRLMesh* rlMesh = meshIt->second;
		int16 numGroups = rlMesh->GetUnwrapGroupsNum();

		if( bNeedDebugInfo )
			CLogFile::FormatLine("  NumGroups:%d\n", numGroups );

		CRLUnwrapGroup *pUnwrapGroup = rlMesh->GetUnwrapGroups();
		assert( NULL != pUnwrapGroup );
		for (int16 i=0; i<numGroups; i++ )
		{
			nGroupBiggestWidth = ( pUnwrapGroup[i].iWidth > nGroupBiggestWidth ) ? pUnwrapGroup[i].iWidth : nGroupBiggestWidth;
		}
	} //RLMeshes loop

	if( bNeedDebugInfo )
		CLogFile::FormatLine("  GroupsBiggestWidth:%d\n", nGroupBiggestWidth );

	int32 nSurfaceID = 0;
	for(std::vector<CRLSurface*>::const_iterator lmSurfItor = pSurfaceList->begin(); lmSurfItor!=pSurfaceList->end(); lmSurfItor++ )
	{
		CRLSurface* pRLSurface = (*lmSurfItor);

		if( NULL == pRLSurface || pRLSurface->IsFinished() )
		{
			++nSurfaceID;
			continue;
		}

		if( bNeedDebugInfo )
		{
			CLogFile::FormatLine("  New surface.\n" );
			pRLSurface->GenerateDebugJPG();
		}

		if( 0 == OCM->m_ActiveChannelNumber )
		{
			if( bNeedDebugInfo )
				CLogFile::FormatLine("  No active channel.\n" );
			OCM->m_FirstChannel = 0;
			return true;
		}

		//fast rejection -> if the mesh have bigger chuck than the possible maximum free space in the surface reject the surface.
		if( pRLSurface->IsGroupInsertable( nGroupBiggestWidth ) )
		{
			//dig as deep as I can
			int32 nFirstActiveChannel = pRLSurface->m_nMaxUsedOcclusionChannel - sceneCtx.m_nSlidingChannelNumber;
			nFirstActiveChannel = nFirstActiveChannel < 0 ? 0 : nFirstActiveChannel;

			//go down, if there are so much channel needed
			if( nFirstActiveChannel + OCM->m_ActiveChannelNumber > sceneCtx.m_numComponents )
			{
				nFirstActiveChannel -= nFirstActiveChannel + OCM->m_ActiveChannelNumber - sceneCtx.m_numComponents;
				nFirstActiveChannel -= sceneCtx.m_nSlidingChannelNumber;
				nFirstActiveChannel = nFirstActiveChannel < 0 ? 0 : nFirstActiveChannel;
			}

			//sliding window + 1 -> "the last & free channel" if you realy need it...
			int32 nLastActiveChannel = nFirstActiveChannel + sceneCtx.m_nSlidingChannelNumber + 1;

			//go down, if there are so much channel needed
			if( nLastActiveChannel + OCM->m_ActiveChannelNumber > sceneCtx.m_numComponents )
			{
				nLastActiveChannel -= nLastActiveChannel + OCM->m_ActiveChannelNumber - sceneCtx.m_numComponents;
				nLastActiveChannel = nLastActiveChannel < 0 ? 0 : nLastActiveChannel;
			}
			nLastActiveChannel = nLastActiveChannel > (sceneCtx.m_numComponents-1) ? (sceneCtx.m_numComponents-1) : nLastActiveChannel;

			for( OCM->m_FirstChannel = nFirstActiveChannel; OCM->m_FirstChannel <= nLastActiveChannel; ++OCM->m_FirstChannel )
			{
				bool isAllocated = true;

				pRLSurface->BeginAlloc();
				int32 nLastElement = pRLSurface->m_RenderableGroups.size();

				for(meshIt = mmapMeshRange.first; meshIt!=mmapMeshRange.second; meshIt++ )
				{
					CRLMesh* rlMesh = meshIt->second;

					int16 numGroups = rlMesh->GetUnwrapGroupsNum();
					CRLUnwrapGroup *pGroup =rlMesh->GetUnwrapGroups();
					assert( NULL != pGroup );
					for (int16 i=0; i<numGroups; i++ )
					{
						//reconstruct the span buffers
						pGroup[i].LoadOnlySpanBuffers( pCacheFile );

						//try to allocate
						if (!( pRLSurface->AllocateGroup( pGroup[i], *OCM ) ))
						{
							pGroup[i].ReleaseSpanBuffers();
							if( bNeedDebugInfo )
								CLogFile::FormatLine("    Can't allocate the group.\n    GroupNumber:%d\n    Width:%d  Height:%d!\n",i, pGroup[i].iWidth, pGroup[i].iHeight );
							isAllocated = false;
							break;
						}
						pGroup[i].ReleaseSpanBuffers();
					}
					if (!isAllocated)
						break;
				} //RLMeshes loop

				if (isAllocated)
				{
					//ones per meshes - not / unwrapgroup
					pRLSurface->LazyUpdates();

					//attach to surface - only when the surface released can we release the mesh itself....
					for(meshIt = mmapMeshRange.first; meshIt!=mmapMeshRange.second; meshIt++ )
						pRLSurface->AttachMesh( meshIt->second );

					pRLSurface->SetLastUsedOcclusionChannel( OCM->m_FirstChannel+OCM->m_ActiveChannelNumber ); //LightIt->second.size() );

					/*				//BROKEN CODE -> Channel mask not working...
					if( pFile )
					{
					std::vector<t_pairSurfaceGroup>::iterator groupEnd = pRLSurface->m_RenderableGroups.end();
					for(std::vector<t_pairSurfaceGroup>::iterator groupIt = pRLSurface->m_RenderableGroups.begin()+ nLastElement; groupIt != groupEnd; ++groupIt )
					{
					CRLUnwrapGroup *pGroup = groupIt->first;
					SurfaceOffset *Offset = &(groupIt->second);

					fwrite( &nSurfaceID, sizeof(int32),1, pFile );
					fwrite( &Offset->X, sizeof(int32),1, pFile );
					fwrite( &Offset->Y, sizeof(int32),1, pFile );
					fwrite( &pGroup->m_SpanDirection, sizeof(int32),1, pFile );
					}
					}
					*/
					if( ppAllocRLSurface )
						*ppAllocRLSurface = pRLSurface;
					if( bNeedDebugInfo )
						CLogFile::FormatLine("  Allocated.\n" );
					return true;
				}
				else
					pRLSurface->RevertAlloc();
			}// Channel
		}// BiggestWidth
		else
		{
			if( bNeedDebugInfo )
				CLogFile::FormatLine("    Early check failed.\n" );
		}
		++nSurfaceID;
	}// RLSurfaces loop

	if( bNeedDebugInfo )
		CLogFile::FormatLine("* Can't allocated!\n" );
	return false;
}


//===================================================================================
// Method				:	GenerateSurface
// Description	:	Physicaly generate the surface
//===================================================================================
bool CLightScene::GenerateSurface(CRLSurface* pRLSurface, FILE *pCacheFile  )
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	//not generate lightmap..
	if( false == sceneCtx.m_bMakeLightMap && sceneCtx.m_numComponents == 16 )
	{
		//if we have only occlusion map, try to use a smaller textures
		if( pRLSurface->m_nMaxUsedOcclusionChannel <= 4 )
		{
			//we use only 1 quarter of the full texture
			int nSize = pRLSurface->m_ObjectIds.size();
			for( int i = 0; i <  nSize; ++i )
				m_pISerializationManager->RescaleTexCoordData( pRLSurface->m_ObjectIds[i].first,pRLSurface->m_ObjectIds[i].second, 2, 2 );
		}
		else
		{
			if( pRLSurface->m_nMaxUsedOcclusionChannel <= 8 )
			{
				//we use only the half of the full texture
				int nSize = pRLSurface->m_ObjectIds.size();
				for( int i = 0; i <  nSize; ++i )
					m_pISerializationManager->RescaleTexCoordData( pRLSurface->m_ObjectIds[i].first,pRLSurface->m_ObjectIds[i].second, 1, 2 );
			}
		}
	}

	//allocate the surface memories
	if( false == pRLSurface->AllocateMemory() )
		return false;

	//iterate over the unwrapgroups attached to the surface, and copy the patches to the surface
	std::vector<t_pairSurfaceGroup>::const_iterator groupIt;
	std::vector<t_pairSurfaceGroup>::const_iterator groupEnd = pRLSurface->m_RenderableGroups.end();
	for(groupIt = pRLSurface->m_RenderableGroups.begin(); groupIt!=groupEnd; groupIt++ )
	{
		t_pairSurfaceGroup SurfaceGroup = *groupIt;
		CRLUnwrapGroup *Group = SurfaceGroup.first;
		SurfaceData Offset = SurfaceGroup.second;

		if( false == pRLSurface->CopyGroup( Offset.X, Offset.Y, *Group, pCacheFile, Offset.Mask) )
			return false;
	}

	return true;
}

//===================================================================================
// Method				:	RescaleTexCoords
// Description	:	Rescale the texture coordinates based on the surface
//===================================================================================
void CLightScene::RescaleTexCoords(CRLSurface* pRLSurface)
{
	m_mapValidTexCoords.clear(); //begin valid texture coords accamulating
	//iterate over the unwrapgroups attached to the surface, and rescale the texture coordinates
	for(std::vector<t_pairSurfaceGroup>::const_iterator groupIt = pRLSurface->m_OneMeshRenderableGroups.begin(); groupIt!=pRLSurface->m_OneMeshRenderableGroups.end(); groupIt++ )
	{
		t_pairSurfaceGroup SurfaceGroup = *groupIt;
		CRLUnwrapGroup *Group = SurfaceGroup.first;
		SurfaceData Offset = SurfaceGroup.second;

		AddTexCoords(Offset.X, Offset.Y, pRLSurface->m_nWidth, pRLSurface->m_nHeight, *Group);
	}
}

//===================================================================================
// Method				:	AddTexCoords
// Description	:	Rescale and offset the textures coordinates of a UnwrapGroup
//===================================================================================
void CLightScene::AddTexCoords(int32 nOffsetX,int32 nOffsetY,int32 nSurfWidth,int32 nSurfHeight,CRLUnwrapGroup& group)
{
	for (int32 i = 0; i < group.m_nTriNumber; ++i)
	{
		CRLTriangle* pTri = &group.arrTriangles[i];
		for (int32 nVert=0; nVert<=2; nVert++)
		{
			assert(pTri->m_vVert[nVert].u >=0.0f && pTri->m_vVert[nVert].u <=1.0f);
			assert(pTri->m_vVert[nVert].v >=0.0f && pTri->m_vVert[nVert].v <=1.0f);

			//actual normalization
			TexCoord2Comp texCoord;
			group.GenerateUVCoordinates( pTri->m_vVert[nVert].u, pTri->m_vVert[nVert].v, nOffsetX, nOffsetY, nSurfWidth, nSurfHeight, texCoord.s, texCoord.t );

			assert(texCoord.s>=0.0f && texCoord.s<=1.0f);
			assert(texCoord.t>=0.0f && texCoord.t<=1.0f);

			//			m_mapValidTexCoords.insert(t_pairIndTexCoord(pTri->m_nInd[nVert], texCoord));
			m_mapValidTexCoords.push_back(t_pairIndTexCoord(pTri->m_nInd[nVert], texCoord));
		}
	}

	return;
}

//===================================================================================
// Method				:	GeneareLightVolume
// Description	:	A test function - NOT WORKING PERFECTLY
//===================================================================================
bool CLightScene::GenerateLightVolume(
																			const IndoorBaseInterface &pInterface, 
																			LMGenParam sParam,		 std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, 
struct ICompilerProgress *pIProgress, 
	bool &rErrorsOccured )
{
	_TRACE(m_LogInfo, true, "RAM preprocessor started\r\n");
	Init();

	m_uiStartMemoryConsumption = 0;
	rErrorsOccured						 = false;
	CSceneContext& sceneCtx		 = CSceneContext::GetInstance();
	I3DEngine* pI3DEngine			 = GetIEditor()->Get3DEngine();
	g_pIProgress							 = pIProgress;	 // TODO
	m_sParam								 	 = sParam;       // TODO

	memcpy(&m_IndInterface, &pInterface, sizeof(IndoorBaseInterface));

	if (g_pGC !=NULL)
		delete g_pGC;

	if (g_pGlobalMap !=NULL)
		delete g_pGlobalMap;

	if (g_pCausticMap !=NULL)
		delete g_pCausticMap;

	SetupLights();


	Matrix34 mWorld;
	mWorld.SetIdentity();

	g_pGlobalMap = new CPhotonMap("global",mWorld);
	g_pCausticMap = new CPhotonMap("caustic",mWorld);
	sceneCtx.SetGlobalMap(g_pGlobalMap);
	sceneCtx.SetCausticMap(g_pCausticMap);

	g_pGC = new CGeomCore();
	sceneCtx.SetGeomCore(g_pGC);

	std::set<const CBaseObject*> vSelectionIndices;
	SetupBrushes( true, false, vNodes, vSelectionIndices, ELMMode_ALL );

	//build geom accelerator
	Caption("Building octree...");
	g_pGC->BuildAccel(this);
	g_pGC->GetAccelStats();

	t_pairEntityId LightIDs;
	const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();
	int32 nLightNumber			= pListLights->Count();

	for( int32 i=0; i< nLightNumber; ++i )
	{
		CDLight* pLight	=	pListLights->GetAt(i);

		if( NULL == pLight )
		{
			//accident?
			assert(0 && "there must be a light!");
			continue;
			LightIDs.push_back( std::pair<EntityId, EntityId>( pLight->m_nEntityId, i ) );
		}
	}

	C3DSamplerCompiler SamplerCompiler;
	SamplerCompiler.Prepare(  0.25f, _3DSMP_DIRECTLIGHT );

	//fix:: tess parameter
	CPhotonTrace* pPhotonTrace = new CPhotonTrace(sceneCtx.m_numEmitPhotons, 100);

	//Vec3 vFrom(937.0f, 893.0f, 56.0f), vC(1.0f, 1.0f, 1.0f);
	//pPhotonTrace->IlluminateTest(vFrom, vC);

	_TRACE(m_LogInfo, true, "Tracing of Photons\r\n");
	pPhotonTrace->Shade(this);
	_TRACE(m_LogInfo, true, "Balancing Photon Map\r\n");
	g_pGlobalMap->Balance();
	//	g_pGlobalMap->PreprocessShadowPhotons();

	SAFE_DELETE( pPhotonTrace );

	int32 nPercent = 0;

	ProgressSetBounds( 0 , 100 );

	I3DSampler* pSampler = SamplerCompiler.GetSampler();
	if( pSampler == NULL )
		return false;

	f32* pPosition = pSampler->GetPositionArray();
	f32* pData = pSampler->GetPointArray();

	int32 nFloatNumber = pSampler->GetNumberOfFloats();

	if( nFloatNumber < 3 )
		return false;						//not supported yet

	int32 nPointNumber = pSampler->GetPointNumber();
	Vec3 vColor,vPosition;
	for( int32 i = 0; i < nPointNumber; ++i )
	{
		//Wait Progress
		SetProgress( i / (float)nPointNumber );
		vPosition.Set( pPosition[i*3+0], pPosition[i*3+1], pPosition[i*3+2] );
		g_pGlobalMap->Lookup( &vColor, vPosition, 1000 );

		pData[ nFloatNumber*i + 0 ] = vColor.x;
		pData[ nFloatNumber*i + 1 ] = vColor.y;
		pData[ nFloatNumber*i + 2 ] = vColor.z;
	}

	string str3DSamplerExportFl = m_IndInterface.m_p3dEngine->GetFilePath(_3DSAMPLER_EXPORT_FILE_NAME);
	return SamplerCompiler.Save( str3DSamplerExportFl.c_str() );;
}

//===================================================================================
// Method				:	GenerateMaps
// Description	:	Generate a list for brushes which static light's are enabled by the normal light setup, but
//                the exact test showed that it's false
//===================================================================================
bool CLightScene::GenerateFalseLightList( const IndoorBaseInterface &pInterface, LMGenParam sParam, std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, 
struct ICompilerProgress *pIProgress, const ELMMode Mode, volatile SSharedLMEditorData* pSharedData, const std::set<const CBaseObject*>& vSelectionIndices,
	bool &rErrorsOccured, const bool bShowTexelDensity )
{
	//1. init
	m_uiStartMemoryConsumption = 0;	//new run
	rErrorsOccured = false;
	Init();
	memcpy(&m_IndInterface, &pInterface, sizeof(IndoorBaseInterface));
	if (!m_pISerializationManager)
		m_pISerializationManager = m_IndInterface.m_p3dEngine->CreateLMSerializationManager();
	assert(m_pISerializationManager != NULL);

	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator itEty;
	CRadMesh *pMesh = NULL;
	IRenderNode *pIEtyRend = NULL;
	Matrix34 matEtyMtx;
	IStatObj *pIGeom = NULL;
	CRadPoly *pCurPoly = NULL;
	DWORD dwStartTime = GetTickCount();

	g_pIProgress = pIProgress;	 // TODO
	m_sParam = sParam;           // TODO
	//////////////////////////////////////////////////////////////////////////
	// RenderNodes sort
	//////////////////////////////////////////////////////////////////////////
	//vNodes
	//////////////////////////////////////////////////////////////////////////
	_TRACE(m_LogInfo, true, "Light Interaction setup started.\r\n");

	SetupLights();

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();
	assert( pListLights );
	if( NULL == pListLights )
		return false;

	PodArray<sFalseLightSerializationStructure> m_FalseLight;

	sFalseLightSerializationStructure Data;

	//2. check the light - brush intersection - first with the "normal" (same as the 3dengine use),
	//   next remove all lights which have a real intersection...
	t_pairEntityId LightIDs,FalseLightIDs;
	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	{
		IRenderNode *pIEtyRend = itEty->first;
		if( NULL == pIEtyRend )
			continue;
		CBaseObject* pObject = itEty->second;
		assert (pObject!=NULL);
		if( NULL == pObject )
			continue;

		LightIDs.resize(0);
		CreateLightList( LightIDs, pIEtyRend );
		FalseLightIDs.resize(0);
		CheckExactLightObjInteraction( LightIDs, pIEtyRend, pObject, FalseLightIDs );

		//3. serialize
		uint32 nFalseLightNumber = FalseLightIDs.size();
		if( nFalseLightNumber )
		{
			Data.m_nObjectID = pIEtyRend->GetEditorObjectId();
			Data.m_nLightNumber = nFalseLightNumber;
			for( uint32 i = 0; i < nFalseLightNumber; ++i )
			{
				CDLight* pLight	=	pListLights->GetAt( FalseLightIDs[i].second );
				Data.m_Lights[i] = pLight->m_nEntityId;
			}
			m_FalseLight.push_back(Data);
		}
	}

	string strLMExportFl = m_IndInterface.m_p3dEngine->GetFilePath(LM_FALSE_LIGHTS_EXPORT_FILE_NAME);

	if( NSAVE_RESULT::ESUCCESS != m_pISerializationManager->SaveFalseLight(strLMExportFl.c_str(), m_FalseLight.size(), m_FalseLight.begin() ) )
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Can't save the FalseLight datas.\r\n");
		return false;
	}


	std::vector< struct IRenderNode *> m_RenderNodeVector;

	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
		m_RenderNodeVector.push_back( itEty->first );

	if( 0 != m_RenderNodeVector.size() )
		m_pISerializationManager->LoadFalseLight(strLMExportFl.c_str(), &m_RenderNodeVector.front(), m_RenderNodeVector.size() );

	return true;
}


//===================================================================================
// Method				:	CheckExactLightObjInteraction
// Description	:	Create a false light list (list of lights, which have a crosssection with the object bbox, but not realy affect the object itself)
//===================================================================================
void CLightScene::CheckExactLightObjInteraction( t_pairEntityId &LightIDs, IRenderNode *pRenderNode, CBaseObject* pObject, t_pairEntityId &FalseLightIDs )
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	//Setup mesh informations
	Matrix34 mNodeMatrix;

	assert(pRenderNode != NULL);
	if( false == pRenderNode )
		return;

	pRenderNode->GetEntityStatObj(0, 0, &mNodeMatrix);
	IRenderMesh *pRenderMesh = pRenderNode->GetRenderMesh(0);

	//Voxels vertex container used as rendermesh
	if( pObject && OBJTYPE_VOXEL == pObject->GetType() && pRenderMesh )
		pRenderMesh = pRenderMesh->GetVertexContainer();

	//fix add support of with multiobjects rendernode
	assert(pRenderMesh != NULL);
	if (pRenderMesh == NULL || pRenderMesh->IsEmpty())
		return;

	// Get indices and vertices
	int iIdxCnt = 0;
	unsigned short* pIndices = pRenderMesh->GetIndices(&iIdxCnt);
	assert( pIndices );
	if( NULL == pIndices )
		return;
	int iVtxStride = 0;
	Vec3 *pVertices = reinterpret_cast<Vec3 *> (pRenderMesh->GetStridedPosPtr(iVtxStride));
	assert( pVertices );
	if( NULL == pVertices )
		return;

	PodArray<CRenderChunk>* pRenderChunks = pRenderMesh->GetChunks();
	int32 nChunkNumber = pRenderChunks->Count();

	IMaterial* pDefMaterial = pRenderNode->GetMaterial();

	//Setup light loop
	const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();
	assert( pListLights );
	if( NULL == pListLights )
		return;
	uint32 nLightNumber = LightIDs.size();
	for( uint32 i = 0; i < nLightNumber; ++i )
	{
		CDLight* pLight	=	pListLights->GetAt( LightIDs[i].second );
		assert( pLight );
		if( NULL == pLight )
			continue;

		Sphere Sph( pLight->m_BaseOrigin, pLight->m_fRadius);

		//check the real interaction between light & object's mesh
		int32 nChunk;
		for (nChunk=0; nChunk<nChunkNumber; nChunk++)
		{
			CRenderChunk rendChunk = (*pRenderChunks)[nChunk];

			if (rendChunk.m_nMatFlags & MTL_FLAG_NODRAW)
				continue;

			SShaderItem shaderItem;
			IRenderShaderResources* pShaderResources = NULL;
			//multi-material shader retrieve
			if (pDefMaterial!= NULL)
			{
				shaderItem = pDefMaterial->GetShaderItem(rendChunk.m_nMatID);
				pShaderResources = shaderItem.m_pShaderResources;

				{ // test for old assets
					CString sShaderName = shaderItem.m_pShader->GetName();
					if(sShaderName.Find("templdecal") != -1)  	//do not compute decals
						continue;
					if(sShaderName.Find("nodraw") != -1)  	//do not compute decals
						continue;
				}
			}

			uint32 numTris =  rendChunk.nNumIndices / 3;

			// Process all triangles
			uint32 iCurTri;
			for (iCurTri=0; iCurTri<numTris; iCurTri++)
			{
				uint32 iBaseIdx = rendChunk.nFirstIndexId + iCurTri * 3;
				Vec3 v0 = mNodeMatrix.TransformPoint((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pVertices)[pIndices[iBaseIdx + 0] * iVtxStride]))));
				Vec3 v1 = mNodeMatrix.TransformPoint((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pVertices)[pIndices[iBaseIdx + 1] * iVtxStride]))));
				Vec3 v2 = mNodeMatrix.TransformPoint((* reinterpret_cast<Vec3 *> (&(reinterpret_cast<byte *>(pVertices)[pIndices[iBaseIdx + 2] * iVtxStride]))));

				//it is a degenerated tri - will fail the sphere-tri overlap test.
				if( v0.IsEquivalent( v1 ) || v1.IsEquivalent( v2 ) || v0.IsEquivalent( v2 ) )
					continue;

				Triangle tri( v0,v1,v2);

				//if there are any overlaping... don't need any other test...
				if( Overlap::Sphere_Triangle( Sph, tri ) )
					break;
			}

			if( iCurTri != numTris )
				break;
		}

		//no any interaction... this is really a false interaction...
		if( nChunk == nChunkNumber )
			FalseLightIDs.push_back( LightIDs[i] );
	}
}

float GetLightAmount(CDLight * pLight, const AABB & objBox)
{
  // find amount of light
  float fDist = sqrt(Distance::Point_AABBSq(pLight->m_Origin, objBox));
  float fLightAttenuation = (pLight->m_Flags & DLF_DIRECTIONAL) ? 1.f : 1.f - (fDist) / (pLight->m_fRadius);
  if(fLightAttenuation<0)
    fLightAttenuation=0;

  float fLightAmount = 
    (pLight->m_Color.r+pLight->m_Color.g+pLight->m_Color.b)*0.233f + 
    (pLight->m_SpecMult)*0.1f;

  return fLightAmount*fLightAttenuation;
}

//===================================================================================
// Method				:	CreateLightList
// Description	:	Give back the same light list as the 3dEngine
//===================================================================================
void CLightScene::CreateLightList( t_pairEntityId &LightIDs, IRenderNode *pRenderNode )
{
	if( NULL == pRenderNode )
		return;

	I3DEngine* pI3DEngine			 = GetIEditor()->Get3DEngine();

	IVisArea *pEntityVisArea = (IVisArea*)pRenderNode->GetEntityVisArea();
	AABB box = pRenderNode->GetBBox();
	Vec3 vObjPos( box.GetCenter() );
	float fObjRadius = pRenderNode->GetBBox().GetRadius();

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();
	assert(pListLights!=NULL);
	if( NULL == pListLights )
		return;

	int32 nLightNumber			= pListLights->Count();

	for( int32 i=0; i< nLightNumber; ++i )
	{
		CDLight* pLight	=	pListLights->GetAt(i);
		if( NULL == pLight )
		{
			//accident?
			assert(0 && "there must be a light!");
			continue;
		}

		if( pLight->m_pOwner )
		{
			float fMaxDist = pLight->m_pOwner->GetMaxViewDist();

			if( (vObjPos - pLight->m_BaseOrigin).len() > fObjRadius+fMaxDist )
				continue;
		}

		// todo: remove another similar check 
		if(pRenderNode && pLight->m_Flags & DLF_THIS_AREA_ONLY)
			if(pLight->m_pOwner && pLight->m_pOwner->GetEntityVisArea())
				if(pEntityVisArea != pLight->m_pOwner->GetEntityVisArea())
					if(!pEntityVisArea || !pEntityVisArea->IsPortal())
						continue; // different areas

		//		if(pLight->m_Flags & DLF_IGNORE_OWNER && pRenderNode && pRenderNode == pLight->m_pOwner)
		//			continue; // skip this object lights (vehicle lights)

		//Not skipped.
		//		if ((pLight->m_Flags & DLF_ONLY_FOR_HIGHSPEC) && (m_LightConfigSpec < CONFIG_HIGH_SPEC))
		//			continue; // Skip high spec only light on medium and low spec configurations

		/*	NO MORE A VALID OPTION
		if(pRenderNode && 
		pLight->m_Flags & DLF_LM && 
		//				!(pLight->m_Flags & DLF_CASTSHADOW_VOLUME) &&
		pRenderNode->GetRndFlags() & ERF_USERAMMAPS && 
		pRenderNode->HasLightmap(0))
		{ // in case of lightmaps
		if(pLight->m_Flags & DLF_PROJECT)
		continue; // ignore specular only lights if projector since specular projectors not supported

		if (!(pLight->m_Flags & DLF_CASTSHADOW_MAPS))
		{
		if ((pLight->m_SpecColor == Col_Black) //||
		//					((pLight->m_Flags & DLF_SPECULAR_ONLY_FOR_HIGHSPEC) && (m_LightConfigSpec < CONFIG_HIGH_SPEC))
		)
		continue; // ignore specular only lights if specular disabled
		}
		}
		*/
		if(!pRenderNode && (pLight->m_Flags & DLF_THIS_AREA_ONLY))
			if(	pLight->m_pOwner && pLight->m_pOwner->GetEntityVisArea())
				continue; // indoor lsource do not affect on vegetations

		if(pLight->m_Flags & DLF_PROJECT && pRenderNode 
			&& pLight->m_fLightFrustumAngle<90 // actual fov is twice bigger
			&& pLight->m_pLightImage!=0 && (pLight->m_pLightImage->GetFlags() & FT_FORCE_CUBEMAP))
		{ // check projector frustum
			// use pLight->m_TextureMatrix to construct Plane
			/*DrawBBox(pLight->m_Origin, pLight->m_Origin+pLight->m_TextureMatrix.GetColumn(0), DPRIM_LINE);
			GetRenderer()->DrawLabel(pLight->m_Origin+pLight->m_TextureMatrix.GetColumn(0),1,"x");
			DrawBBox(pLight->m_Origin, pLight->m_Origin+pLight->m_TextureMatrix.GetColumn(1), DPRIM_LINE);
			GetRenderer()->DrawLabel(pLight->m_Origin+pLight->m_TextureMatrix.GetColumn(1),1,"y");
			DrawBBox(pLight->m_Origin, pLight->m_Origin+pLight->m_TextureMatrix.GetColumn(2), DPRIM_LINE);
			GetRenderer()->DrawLabel(pLight->m_Origin+pLight->m_TextureMatrix.GetColumn(2),1,"z");*/					

			//It is runtime cvar dependent - we use the "wronger" possibility for that case...
			/*			if(GetCVars()->e_projector_exact_test)
			{ 
			CCamera & cam = m_arrCameraProjectors[n];
			AABB box = pRenderNode->GetBBox();
			if (!cam.IsAABBVisible_F( box ))
			continue;
			}
			else*/
			{
				Plane p;
				p.SetPlane( pLight->m_Origin, pLight->m_Origin+pLight->m_ProjMatrix.GetColumn(2), pLight->m_Origin-pLight->m_ProjMatrix.GetColumn(1) );
				if( p.DistFromPlane(vObjPos) + fObjRadius < 0 )
					continue;
			}
		}
		else if(pRenderNode)
		{ // check sphere/bbox intersection
			if(!Overlap::Sphere_AABB(Sphere(pLight->m_Origin, pLight->m_fRadius), box))
				continue;
		}

		// get amount of light
		float fLightAmount = GetLightAmount(pLight, box) ;

		if(fLightAmount>0.05f)
		{
			// if entity is inside some area - allow lightsources only from this area
			if(pRenderNode)
			{
				if(pEntityVisArea)
				{
					if(	pLight->m_pOwner && pLight->m_pOwner->GetEntityVisArea())
					{
						IVisArea * pLightArea = (IVisArea *)pLight->m_pOwner->GetEntityVisArea();
						if( pEntityVisArea != pLightArea )
						{	// try also neighbor volumes
							int nSearchDepth;
							if(pLight->m_Flags & DLF_PROJECT && !(pLight->m_Flags & DLF_THIS_AREA_ONLY))
								nSearchDepth = 5 + int(pLightArea->IsPortal()); // allow projector to go depther
							else
								nSearchDepth = 2 + int((pLight->m_Flags & DLF_THIS_AREA_ONLY)==0) + int(pLightArea->IsPortal());

							bool bNearFound = pEntityVisArea->FindVisArea(pLightArea, nSearchDepth, true);
							if(!bNearFound)
								continue; // areas do not much

							// construct frustum from light pos and portal, check that object is visible from light position
							if(!(pLight->m_Flags & DLF_THIS_AREA_ONLY) && pEntityVisArea && !pEntityVisArea->IsPortal())
							{
								/*
								################################################################
								TODO: add isAABBvisibleFromPos() to IVisArea
								################################################################

								AABB aabbReceiver = pRenderNode->GetBBox();
								Shadowvolume sv;
								int p=0;
								for(; p<pEntityVisArea->m_lstConnections.Count(); p++)
								{
								CVisArea * pArea = pEntityVisArea->m_lstConnections[p];
								if(pArea->IsPortal())
								{
								NAABB_SV::AABB_ShadowVolume(pLight->m_Origin, pArea->m_boxArea, sv, pLight->m_fRadius);
								bool bIntersect = NAABB_SV::Is_AABB_In_ShadowVolume(sv, aabbReceiver);
								if(bIntersect)
								break;
								}
								}
								if(p==pEntityVisArea->m_lstConnections.Count())
								continue; // object is not visible from light position
								*/
							}
						}
					}
					else if(!pEntityVisArea->IsAffectedByOutLights())
						continue; // outdoor lsource
				}
				else // entity is outside
					if(	pLight->m_pOwner && pLight->m_pOwner->GetEntityVisArea())
						continue; // indoor lsource should not affect outdoor entity
			}

			LightIDs.push_back( std::pair<EntityId, EntityId>( pLight->m_nEntityId, i ) );
		}
	}

}

//===================================================================================
// Method				:	GenerateMaps
// Description	:	Generate a lightmap / occlusionmap for the given set of rendernodes
//===================================================================================
bool CLightScene::GenerateMaps(const IndoorBaseInterface &pInterface, LMGenParam sParam,
															 std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, 
															 ICompilerProgress *pIProgress, const ELMMode Mode, 
															 volatile SSharedLMEditorData *pSharedData, const std::set<const CBaseObject*>& vSelectionIndices,
															 bool &rErrorsOccured, const bool bShowTexelDensity )
{
	m_uiStartMemoryConsumption = 0;	//new run
	rErrorsOccured = false;

	Init();

	memcpy(&m_IndInterface, &pInterface, sizeof(IndoorBaseInterface));

	if (!m_pISerializationManager)
		m_pISerializationManager = m_IndInterface.m_p3dEngine->CreateLMSerializationManager();
	assert(m_pISerializationManager != NULL);

	m_rlSurfaces.clear();
	m_OcclusionLightPairs.clear();
	m_LightmapLightPairs.clear();

	string strLMExportFl = m_IndInterface.m_p3dEngine->GetFilePath(LM_EXPORT_FILE_NAME);

	if(ELMMode_SEL != Mode)
	{
		//create backup files
		string strDirName = PathUtil::GetParentDirectory(strLMExportFl);
		string strPakName = strDirName + "\\" "LevelLM.pak";
		CFileUtil::BackupFile( strPakName );
	}

	//First init, the serializer - if there are any problem, don't take the time from level designers...
	if( NSAVE_RESULT::ESUCCESS != m_pISerializationManager->InitLMUpdate( strLMExportFl.c_str(), (ELMMode_SEL == Mode) ) )
	{
		SAFE_DELETE( m_pISerializationManager );
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Can't init the RAM serialization manager.\r\n");
		return false;
	}


	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator itEty;
	CRadMesh *pMesh = NULL;
	IRenderNode *pIEtyRend = NULL;
	Matrix34 matEtyMtx;
	IStatObj *pIGeom = NULL;
	CRadPoly *pCurPoly = NULL;
	DWORD dwStartTime = GetTickCount();

	g_pIProgress = pIProgress;	 // TODO
	m_sParam = sParam;           // TODO
	//////////////////////////////////////////////////////////////////////////
	// RenderNodes sort
	//////////////////////////////////////////////////////////////////////////
	//vNodes
	//////////////////////////////////////////////////////////////////////////
	_TRACE(m_LogInfo, true, "RAM preprocessor started\r\n");

	bool bReturn = true;
	unsigned int uiMeshesToProcess = 0;

	_TRACE(m_LogInfo, true, "Parameters:\r\n", m_sParam.m_fTexelSize);
	_TRACE(m_LogInfo, true, "Code Version = 4.0\r\n");
	_TRACE(m_LogInfo, true, "Texel size = %f\r\n", m_sParam.m_fTexelSize);
	_TRACE(m_LogInfo, true, "Texture resolution = %i\r\n", m_sParam.m_iTextureResolution);
	_TRACE(m_LogInfo, true, "Subsampling = %s\r\n", (m_sParam.m_iSubSampling == 9)?"on":"off");
	_TRACE(m_LogInfo, true, "Generate Occlusion maps = %s\r\n", (m_sParam.m_bGenOcclMaps)?"True":"False");
	_TRACE(m_LogInfo, true, "Shadows = %s\r\n", m_sParam.m_bComputeShadows ? "True" : "False");
	_TRACE(m_LogInfo, true, "Use sunLight = %s\r\n", m_sParam.m_bUseSunLight? "True" : "False");
	_TRACE(m_LogInfo, true, "Smoothing angle = %i degree\r\n", m_sParam.m_uiSmoothingAngle);
	_TRACE(m_LogInfo, true, "Use spotlights as pointlights = %s\r\n", m_sParam.m_bSpotAsPointlight ? "True" : "False");

	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	//################################################ Need to remove it - if lightmap compiling enabled again !
	sceneCtx.m_bMakeLightMap = false;
#ifdef RENDER_OCCLUSIONMAP
	sceneCtx.m_bMakeOcclMap = true; //sceneCtx.m_numDirectSamples ? true : false;
	sceneCtx.m_bMakeRAE = false; //sceneCtx.m_numAmbientOcclusionSamples ? true : false;
#else
	sceneCtx.m_bMakeOcclMap = false;
	sceneCtx.m_bMakeRAE = true;
#endif
	sceneCtx.m_nDistributedBlockSize = (sizeof(uint8)+(5+sceneCtx.m_numJitterSamples*sceneCtx.m_numJitterSamples)*6*sizeof(float))/sizeof(uint8);
	//	sceneCtx.m_bDistributedMap = false;

	//setup the static values because the levels can contain older values..
	sceneCtx.m_numDirectSamples =	sceneCtx.m_bMakeOcclMap ? 1 : 0;
	sceneCtx.m_numAmbientOcclusionSamples = sceneCtx.m_bMakeRAE ? ( sceneCtx.m_bUseSuperSampling ? 9*8 : 2*2 ) : 0; //18*18 : 0;
	sceneCtx.m_nLightmapPageWidth =	1024;
	sceneCtx.m_nLightmapPageHeight = 1024;
	sceneCtx.m_numRAEComponents = 1;
	sceneCtx.m_nMaxTrianglePerPass = 50000000;
	sceneCtx.m_numComponents = ( sceneCtx.m_bMakeRAE && !sceneCtx.m_bMakeOcclMap && !sceneCtx.m_bMakeLightMap ) ? 1 : 16;	//RAE needed only 1 components others can use more - better texture usage
	sceneCtx.m_nSlidingChannelNumber = 3 > sceneCtx.m_numComponents ? sceneCtx.m_numComponents : 3;

	CRLMesh::m_bSamplesCache = false;

	I3DEngine* pI3DEngine = GetIEditor()->Get3DEngine();
	SetupLights();

	Matrix34 mWorld;
	mWorld.SetIdentity();

	//sort by visareas - optimize for streaming: try to minimalize the distances between the 2 portals
	SortVisAreas( vNodes );

	//Generate Occluder Grid
	if( false == GenerateSpartialListForOccluders( vNodes ) )
		return false;

	//Generate the "ultimate" octree for distributed map - don't create new one / visarea (takes too much time)
#ifndef SKIP_DISTRIBUTED_SAVE_OCTREE
	if( sceneCtx.m_bDistributedMap )
	{
		g_pGC = new CGeomCore;
		if( NULL ==g_pGC )
			return false;
		sceneCtx.SetGeomCore(g_pGC);

		int nTriNumber = 0;
		for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
		{
			IRenderNode *pIEtyRend = itEty->first;
			CBaseObject *pObject = itEty->second;

			//add to geommeshes
			Matrix34 mNodeMatrix;
			pIEtyRend->GetEntityStatObj(0, 0, &mNodeMatrix);
			IRenderMesh *pRenderMesh = pIEtyRend->GetRenderMesh(0);

			//Voxels vertex container used as rendermesh
			if( pObject && OBJTYPE_VOXEL == pObject->GetType() && pRenderMesh )
				pRenderMesh = pRenderMesh->GetVertexContainer();

			if( NULL == pRenderMesh )
				continue;

			IMaterial* pDefMaterial = pIEtyRend->GetMaterial();
			PodArray<CRenderChunk>* pRenderChunks = pRenderMesh->GetChunks();
			for (int32 nChunk=0; nChunk<pRenderChunks->Count(); nChunk++)
			{
				CRenderChunk rendChunk = (*pRenderChunks)[nChunk];

				if (rendChunk.m_nMatFlags & MTL_FLAG_NODRAW)
					continue;

				SShaderItem shaderItem;
				IRenderShaderResources* pShaderResources = NULL;
				//multi-material shader retrieve
				if (pDefMaterial!= NULL)
				{
					shaderItem = pDefMaterial->GetShaderItem(rendChunk.m_nMatID);
					pShaderResources = shaderItem.m_pShaderResources;

					{ // test for old assets
						CString sShaderName = shaderItem.m_pShader->GetName();
						if(sShaderName.Find("templdecal") != -1)  	//do not compute decals
							continue;
						if(sShaderName.Find("nodraw") != -1)  	//do not compute decals
							continue;
					}

					if (pShaderResources && pShaderResources->GetOpacity() != 1.0f)
						continue;
					if (pShaderResources && pShaderResources->GetAlphaRef() != 0.f)
						continue;
				}

				nTriNumber += rendChunk.nNumIndices / 3;
			}
		}

		CLogFile::FormatLine("ReserveTracable: %d\n",nTriNumber );
		g_pGC->ReserveTracable( nTriNumber );


		for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
		{
			IRenderNode *pIEtyRend = itEty->first;
			CBaseObject *pObject = itEty->second;

			//add to geommeshes
			Matrix34 mNodeMatrix;
			pIEtyRend->GetEntityStatObj(0, 0, &mNodeMatrix);
			IRenderMesh *pRenderMesh = pIEtyRend->GetRenderMesh(0);

			//Voxels vertex container used as rendermesh
			if( pObject && OBJTYPE_VOXEL == pObject->GetType() && pRenderMesh )
				pRenderMesh = pRenderMesh->GetVertexContainer();

			if( NULL == pRenderMesh )
				continue;

			IMaterial* pDefMaterial = pIEtyRend->GetMaterial();
			PodArray<CRenderChunk>* pRenderChunks = pRenderMesh->GetChunks();
			for (int32 nChunk=0; nChunk<pRenderChunks->Count(); nChunk++)
			{
				CRenderChunk rendChunk = (*pRenderChunks)[nChunk];

				if (rendChunk.m_nMatFlags & MTL_FLAG_NODRAW)
					continue;

				SShaderItem shaderItem;
				IRenderShaderResources* pShaderResources = NULL;
				//multi-material shader retrieve
				if (pDefMaterial!= NULL)
				{
					shaderItem = pDefMaterial->GetShaderItem(rendChunk.m_nMatID);
					pShaderResources = shaderItem.m_pShaderResources;

					{ // test for old assets
						CString sShaderName = shaderItem.m_pShader->GetName();
						if(sShaderName.Find("templdecal") != -1)  	//do not compute decals
							continue;
						if(sShaderName.Find("nodraw") != -1)  	//do not compute decals
							continue;
					}

					if (pShaderResources && pShaderResources->GetOpacity() != 1.0f)
						continue;
					if (pShaderResources && pShaderResources->GetAlphaRef() != 0.f)
						continue;
				}

				CGeomMesh* pGeomMesh = new CGeomMesh( pRenderMesh, &mNodeMatrix, &rendChunk, pShaderResources, NULL );
				m_mmapGeomMeshes.insert(t_pairGeomMesh(pIEtyRend, pGeomMesh));
			}
		}

		Caption("Building the big octree...");
		CLogFile::FormatLine("BuildOctree\n" );
		g_pGC->BuildAccel(this);
		g_pGC->GetAccelStats();

		g_pGC->Save( "Octree.dat" );
		CLogFile::FormatLine("---\n" );

		SAFE_DELETE( g_pGC );
		sceneCtx.SetGeomCore(NULL);

		//we can easily release all geomesh....
		t_mmapGeomMeshes::iterator GmeshEndIt = m_mmapGeomMeshes.end();
		for(t_mmapGeomMeshes::iterator GmeshIt = m_mmapGeomMeshes.begin(); GmeshIt!=GmeshEndIt; GmeshIt++)
		{
			SAFE_DELETE (GmeshIt->second);
		}
		assert(m_mmapGeomMeshes.size()==0);
		m_mmapGeomMeshes.clear();

	}
#endif

	//Clear the cache file....
	FILE *pCacheFile = NULL;
	int32 nCacheFilePosition = 0;

	//calculate how much visarea affected...
	int32 nVisAreaNumber = 0;
	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator vNodesIt = vNodes.begin();
	IVisArea *pVisArea = NULL;
	bool bNotInVisAreaPass = true;
	while( pVisArea || bNotInVisAreaPass )
	{
		bNotInVisAreaPass = false;

		++nVisAreaNumber;

		//Search next vis area pointer in the list
		for( ; vNodesIt < vNodes.end(); ++vNodesIt )
		{
			IRenderNode *pRendN = vNodesIt->first;
			if( pRendN && (IVisArea*)pRendN->GetEntityVisArea() != pVisArea )
			{
				pVisArea = (IVisArea*)pRendN->GetEntityVisArea();
				break;
			}
		}
		if( vNodesIt == vNodes.end() )
			pVisArea = NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	// Obtain all visareas  - the first pass is for not in visarea objects...
	//////////////////////////////////////////////////////////////////////////
	char szCaptionText[1024];
	vNodesIt = vNodes.begin();
	pVisArea = NULL;
	bNotInVisAreaPass = true;
	int32 nVisAreaID = 0;
	int32 nOctreeID = 0;
	while( pVisArea || bNotInVisAreaPass )
	{
		++nVisAreaID;
		bNotInVisAreaPass = false;

		CLogFile::FormatLine("VisArea name: %s (0x%x)\n", pVisArea ? pVisArea->GetName() : "NULL", pVisArea );

		/*	//DEBUG VISAREA SEARCHER
		if( NULL == pVisArea || stricmp( pVisArea->GetName(), "visarea1") != 0 )
		{
		//Search next vis area pointer in the list
		for( ; vNodesIt < vNodes.end(); ++vNodesIt )
		{
		IRenderNode *pRendN = vNodesIt->first;
		if( pRendN && (IVisArea*)pRendN->m_pVisArea != pVisArea )
		{
		pVisArea = (IVisArea*)pRendN->m_pVisArea;
		break;
		}
		}
		if( vNodesIt == vNodes.end() )
		pVisArea = NULL;
		continue;
		}
		//*/

		//Triangle limit - make it runnable with very big visareas (eg. >1million tri / visareas)
		IRenderNode* pNextRenderNode = NULL;
		do {
			++nOctreeID;

			SAFE_DELETE( g_pGC );
			g_pGC = new CGeomCore();
			sceneCtx.SetGeomCore(g_pGC);

			//release all unused meshes from the last visarea pass
			ReleaseMeshes();

			//			CLogFile::FormatLine("Line1\n" );

			SAFE_DELETE( g_pGlobalMap );
			SAFE_DELETE( g_pCausticMap );
			g_pGlobalMap = new CPhotonMap("global",mWorld);
			g_pCausticMap = new CPhotonMap("caustic",mWorld);
			sceneCtx.SetGlobalMap(g_pGlobalMap);
			sceneCtx.SetCausticMap(g_pCausticMap);

			bool bUseLights = !bShowTexelDensity;			if( SetupBrushes( bUseLights, true, vNodes, vSelectionIndices, Mode, pVisArea, &pNextRenderNode,nVisAreaID,nVisAreaNumber ) )
			{
				//				CLogFile::FormatLine("GenerateLightListPerRenderNode\n" );
				sprintf(szCaptionText,"%d/%d visarea - Optimize light interaction...", nVisAreaID,nVisAreaNumber );
				Caption(szCaptionText);

				//####################################################################################
				//TODO: generate the farest "reciver" information for every light
				//####################################################################################

				GenerateLightListPerRenderNode( vNodes, bUseLights );

				//build geom accelerator
				if( false == bShowTexelDensity && false == sceneCtx.m_bDistributedMap )
				{
					sprintf(szCaptionText,"%d/%d visarea - Building octree...", nVisAreaID,nVisAreaNumber );
					Caption(szCaptionText);
					//					CLogFile::FormatLine("BuildOctree\n" );
					g_pGC->BuildAccel(this);
					g_pGC->GetAccelStats();
					//					CLogFile::FormatLine("---\n" );
				}

				if( !bShowTexelDensity && !sceneCtx.m_bDistributedMap )
				{
					if( sceneCtx.m_bMakeLightMap  )
					{
						if (sceneCtx.m_bUseSunLight && sceneCtx.m_eLightmapMode != LMMODE_PHOTONMAP_ONLY)
						{
							SAFE_DELETE(pSolarIllumStatement);
							pSolarIllumStatement = new CSunDirectIlluminance();
							//pSolarIllumStatement = NULL;//= new CSunDirectIlluminance();
						}
						else 
						{
							pSolarIllumStatement = NULL;
						}

						if (sceneCtx.m_eLightmapMode == LMMODE_PHOTONMAP_ONLY)
						{
							pDirectIllumStatement = NULL;
							pIndirectIllumStatement = g_pGlobalMap;
							sceneCtx.m_bFinalRegatharing = true;
							//always disable super sampling for  PHOTONMAP_ONLY
							sceneCtx.m_bUseSuperSampling = false;
						}
						else 
							if (sceneCtx.m_eLightmapMode == LMMODE_DIRECT_PHOTONMAP)
							{
								SAFE_DELETE(pDirectIllumStatement);
								pDirectIllumStatement = new CDirectIlluminance();
								pIndirectIllumStatement = g_pGlobalMap;
								sceneCtx.m_bFinalRegatharing = false;
							}
							else 
								if (sceneCtx.m_eLightmapMode == LMMODE_DIRECT_REGATHERING)
								{
									SAFE_DELETE(pDirectIllumStatement);
									SAFE_DELETE(pIndirectIllumStatement);
									pDirectIllumStatement = new CDirectIlluminance;
									pIndirectIllumStatement = new CIndirectIlluminance();
									sceneCtx.m_bFinalRegatharing = true;
								}
								else
								{
									CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Unexpected lightmap compilation mode\r\n");
									SAFE_DELETE( m_pISerializationManager );
									return false;
								}

								//fix:: tess parameter
								CPhotonTrace* pPhotonTrace = new CPhotonTrace(sceneCtx.m_numEmitPhotons, 100);

								_TRACE(m_LogInfo, true, "Tracing of Photons\r\n");
								pPhotonTrace->Shade(this);
								_TRACE(m_LogInfo, true, "Balancing Photon Map\r\n");
								g_pGlobalMap->Balance();
								//			g_pGlobalMap->PreprocessShadowPhotons();

								SAFE_DELETE( pPhotonTrace );
					}// if make lightmap

					if( sceneCtx.m_bMakeOcclMap )
					{
						SAFE_DELETE(pOcclusionIllumStatement);
						pOcclusionIllumStatement = new COcclusionIntegrator();
						assert(pOcclusionIllumStatement);
						if( NULL == pOcclusionIllumStatement )
						{
							SAFE_DELETE( m_pISerializationManager );
							CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Unexpected RAM compilation mode\r\n");
							return false;
						}
					}//if make occlusion map

					if( sceneCtx.m_bMakeRAE )
					{
						SAFE_DELETE(pAmbientOcclusionIllumStatement);
						pAmbientOcclusionIllumStatement = new CAmbientOcclusionIntegrator();
						assert(pAmbientOcclusionIllumStatement);
						if( NULL == pAmbientOcclusionIllumStatement )
						{
							SAFE_DELETE( m_pISerializationManager );
							CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Unexpected RAM compilation mode\r\n");
							return false;
						}
					}//if make rae
				}//bShowTexelDensity

				sprintf(szCaptionText,"%d/%d VisArea - Rasterization & Packing...", nVisAreaID,nVisAreaNumber );
				Caption(szCaptionText);

				//seq better with HDD caching, paralell better withouth HDD cache
				if( false == DoRenderingSequential( vNodes, pVisArea, bShowTexelDensity, nOctreeID ) ) return false;
				//if( false == DoRenderingParalell( vNodes, pVisArea, bShowTexelDensity, nOctreeID ) ) return false;
			}

			//continouse rendering inside the visarea - maximize how much triangle used in one pass
		} while( pNextRenderNode != NULL);

		//Search next vis area pointer in the list
		std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator vNodesEnd = vNodes.end();
		for( ; vNodesIt != vNodesEnd; ++vNodesIt )
		{
			IRenderNode *pRendN = vNodesIt->first;
			if( pRendN && (IVisArea*)pRendN->GetEntityVisArea() != pVisArea )
			{
				pVisArea = (IVisArea*)pRendN->GetEntityVisArea();
				break;
			}
		}
		if( vNodesIt == vNodesEnd )
			pVisArea = NULL;
	} //for Vis area

	CLogFile::FormatLine("FinishedAll\n" );
	Caption("RAM map update ...");

	/*
	//Open the cache file....
	pCacheFile = fopen( "LCTemp.tmp", "rb");
	if ( NULL == pCacheFile )
	{
	CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Can't open to read the rasterization temp file.\r\n");
	return false;
	}
	*/
	//Free up the not finished span buffers...
	std::vector<CRLSurface*>::const_iterator surfIt;
	std::vector<CRLSurface*>::const_iterator surfEnd = m_rlSurfaces.end();
	int nSurfaceID = 0;
	for ( surfIt = m_rlSurfaces.begin(); surfIt!= surfEnd; ++surfIt)
	{
		CRLSurface* pRLSurface = *surfIt;
		if( pRLSurface && false == pRLSurface->IsFinished() )
		{
			//debug only?				pRLSurface->GenerateDebugJPG();	
			pRLSurface->ReleaseSpanBuffer();
			GenerateSurface( pRLSurface, pCacheFile );
			UpdateRLSurface( pRLSurface );
			pRLSurface->ReleaseAttachedMeshes();

		}
		delete pRLSurface;//FREE MEM
		++nSurfaceID;
	}

	if( pCacheFile )
		fclose( pCacheFile );

	ReleaseMeshes();
	SAFE_DELETE(g_pGC); //comment it if you want to debug octree

	Caption("RAM datas saving ...");
	_TRACE(m_LogInfo, true, "Exporting RAM data to '%s'\r\n", strLMExportFl.c_str());
	if( NSAVE_RESULT::ESUCCESS != m_pISerializationManager->Save(strLMExportFl.c_str(), m_sParam, ELMMode_ALL) )
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Can't save the RAM datas.\r\n");
		return false;
	}
	Caption("RAM datas saved.");


	std::vector< struct IRenderNode *> m_RenderNodeVector;

	//clear the old lightmaps from brushes, and upload the new one...
//	t_pairEntityId vOcclNull;
//	vOcclNull.clear();

	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	{
		IRenderNode *pIEtyRend = itEty->first;
		IRenderMesh* pRenderMesh	=	pIEtyRend->GetRenderMesh(0);
		if(pRenderMesh)
		{
			pIEtyRend->SetLightmap( NULL, NULL, 0, 0, /*vOcclNull,*/ 0,0 );
		}
		else
		{
			IStatObj* pStatObj	=	pIEtyRend->GetEntityStatObj(0, 0, 0);
			if(pStatObj)
				for(uint a=0;a<pStatObj->GetSubObjectCount();a++)
				{
					IStatObj::SSubObject* pSubObject	=	pStatObj->GetSubObject(a);
					if(pSubObject->nType==STATIC_SUB_OBJECT_MESH && pSubObject->pStatObj)
						pIEtyRend->SetLightmap( NULL, NULL, 0, 0, /*vOcclNull,*/ 0,a);
				}
		}
		m_RenderNodeVector.push_back( pIEtyRend );
	}

	if( 0 == m_RenderNodeVector.size() )
		m_pISerializationManager->ApplyLightmapfile(strLMExportFl.c_str(), NULL, 0 );
	else
		m_pISerializationManager->ApplyLightmapfile(strLMExportFl.c_str(), &m_RenderNodeVector.front(), m_RenderNodeVector.size() );


#ifndef _DEBUG 
	if( g_pGlobalMap == pIndirectIllumStatement )
		pIndirectIllumStatement = NULL;
	SAFE_DELETE(g_pGlobalMap); //comment it if you want to debug photon map
	SAFE_DELETE(g_pCausticMap); //comment it if you want to debug photon map

	//delete integrator
	//SAFE_DELETE(pSolarIllumStatement);
	//SAFE_DELETE(pDirectIllumStatement);
	//SAFE_DELETE(pIndirectIllumStatement); //comment it if you want to debug photon map
	SAFE_DELETE( pOcclusionIllumStatement );
#endif

	SAFE_DELETE( m_pISerializationManager );
	return true;
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

}


//===================================================================================
// Method				:	UpdateRLSurface
// Description	:	Physicaly save the surface textures, and apply the datas to the rendernodes
//===================================================================================
void CLightScene::UpdateRLSurface(CRLSurface* rlSurface)
{
	if( NULL == rlSurface )
		return;

	//if no brush attached to surface
	if( 0 == rlSurface->m_ObjectIds.size() )
		return;

	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	string strLMExportFl = m_IndInterface.m_p3dEngine->GetFilePath(LM_EXPORT_FILE_NAME);

	int32 nLMID = 0;
	RenderLMData_AutoPtr pNewLightmap;
	pNewLightmap = m_pISerializationManager->UpdateLMData(	rlSurface->m_nWidth, rlSurface->m_nHeight, &rlSurface->m_ObjectIds[0], rlSurface->m_ObjectIds.size(),
		sceneCtx.m_bMakeLightMap ? rlSurface->GetDiffuseLightmap() : NULL, NULL, NULL, 
		sceneCtx.m_bMakeOcclMap ? rlSurface->GetOcclusionMap() : NULL, sceneCtx.m_bMakeRAE ? rlSurface->GetRAE() : NULL , strLMExportFl.c_str(), &nLMID	);


#ifndef SKIP_DISTRIBUTED_SAVE
	//Generate distributed map
	if( sceneCtx.m_bDistributedMap )
		rlSurface->SaveDistributedMap( nLMID );
#endif


	rlSurface->ReleaseBuffers();
}


//===================================================================================
// Method				:	SetupLightmapQualtiy
// Description	:	Automaticaly parametrize the "LightmapQuality" parameter of the given set of rendernodes
//===================================================================================
bool CLightScene::SetupLightmapQuality(const IndoorBaseInterface &pInterface, LMGenParam sParam,
																			 std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, 
																			 ICompilerProgress *pIProgress, const ELMMode Mode, 
																			 volatile SSharedLMEditorData *pSharedData, const std::set<const CBaseObject*>& vSelectionIndices,
																			 bool &rErrorsOccured)
{
	/*	m_uiStartMemoryConsumption = 0;	//new run
	rErrorsOccured = false;
	Init();
	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator itEty;
	CRadMesh *pMesh = NULL;
	IRenderNode *pIEtyRend = NULL;
	Matrix34 matEtyMtx;
	IStatObj *pIGeom = NULL;
	CRadPoly *pCurPoly = NULL;
	DWORD dwStartTime = GetTickCount();

	g_pIProgress = pIProgress;	 // TODO
	m_sParam = sParam;           // TODO
	//////////////////////////////////////////////////////////////////////////
	// RenderNodes sort
	//////////////////////////////////////////////////////////////////////////
	//vNodes
	//////////////////////////////////////////////////////////////////////////
	_TRACE(m_LogInfo, true, "LightmapQuality parametrizer started\r\n");

	bool bReturn = true;
	unsigned int uiMeshesToProcess = 0;

	_TRACE(m_LogInfo, true, "Parameters:\r\n", m_sParam.m_fTexelSize);
	_TRACE(m_LogInfo, true, "Code Version = 4.0\r\n");
	_TRACE(m_LogInfo, true, "Texel size = %f\r\n", m_sParam.m_fTexelSize);
	_TRACE(m_LogInfo, true, "Texture resolution = %i\r\n", m_sParam.m_iTextureResolution);

	CRLMesh::m_bSamplesCache = false;

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	I3DEngine* pI3DEngine = GetIEditor()->Get3DEngine();
	_TRACE(m_LogInfo, true, "Parametrization\r\n");

	//setup configuration for parametrizer
	bool  bLM = sceneCtx.m_bMakeLightMap;
	bool  bOM = sceneCtx.m_bMakeOcclMap;
	bool  bRAE = sceneCtx.m_bMakeRAE;
	bool  bDM = sceneCtx.m_bDistributedMap;
	sceneCtx.m_bMakeLightMap = false;
	sceneCtx.m_bMakeOcclMap = false;
	sceneCtx.m_bMakeRAE = true;							//the worst case
	sceneCtx.m_bDistributedMap = false;
	sceneCtx.m_nDistributedBlockSize = (sizeof(uint8)+9*6*sizeof(float))/sizeof(uint8);

	sceneCtx.m_numComponents = 1;						//the worst case
	sceneCtx.m_nSlidingChannelNumber = 1;
	#ifdef RENDER_OCCLUSIONMAP
	sceneCtx.m_nLightmapPageWidth =	
	sceneCtx.m_nLightmapPageHeight = 1024/2;
	#else
	sceneCtx.m_nLightmapPageWidth =	
	sceneCtx.m_nLightmapPageHeight = 1024;
	#endif
	sceneCtx.m_numRAEComponents = 1;
	sceneCtx.m_nMaxTrianglePerPass = 15000000;

	//sort by visareas
	SortVisAreas( vNodes );

	//calculate how much visarea affected...
	int32 nVisAreaNumber = 0;
	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator vNodesIt = vNodes.begin();
	IVisArea *pVisArea = NULL;
	bool bNotInVisAreaPass = true;
	while( pVisArea || bNotInVisAreaPass )
	{
	bNotInVisAreaPass = false;

	++nVisAreaNumber;

	//Search next vis area pointer in the list
	for( ; vNodesIt < vNodes.end(); ++vNodesIt )
	{
	IRenderNode *pRendN = vNodesIt->first;
	if( pRendN && (IVisArea*)pRendN->m_pVisArea != pVisArea )
	{
	pVisArea = (IVisArea*)pRendN->m_pVisArea;
	break;
	}
	}
	if( vNodesIt == vNodes.end() )
	pVisArea = NULL;
	}


	//////////////////////////////////////////////////////////////////////////
	// Obtain all visareas  - the first pass is for not in visarea objects...
	//////////////////////////////////////////////////////////////////////////
	char szCaptionText[1024];
	vNodesIt = vNodes.begin();
	pVisArea = NULL;
	bNotInVisAreaPass = true;
	int32 nVisAreaID = 0;
	while( pVisArea || bNotInVisAreaPass )
	{
	++nVisAreaID;
	bNotInVisAreaPass = false;

	CLogFile::FormatLine("VisArea name: %s (0x%x)\n", pVisArea ? pVisArea->GetName() : "NULL", pVisArea );


	//Triangle limit - make it runnable with very big visareas (eg. >1million tri / visareas)
	IRenderNode* pNextRenderNode = NULL;
	do {
	SAFE_DELETE( g_pGC );
	g_pGC = new CGeomCore();
	sceneCtx.SetGeomCore(g_pGC);

	if( SetupBrushes( false, false, vNodes, vSelectionIndices, Mode, pVisArea, &pNextRenderNode,nVisAreaID,nVisAreaNumber ) )
	{
	sprintf(szCaptionText,"%d/%d visarea - Parametrization...", nVisAreaID,nVisAreaNumber );
	Caption(szCaptionText);

	FILE *pCacheFile = NULL;
	//lightmap parametrization
	int32 numBrushes = vNodes.size();
	int32 nBrushID = 0;
	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++, nBrushID++)
	{
	//Wait Progress
	SetProgress((f32)nBrushID/(f32)numBrushes);

	if (m_BrushRenderable[nBrushID]==0)
	continue;

	IRenderNode* pRenderNode = itEty->first;
	assert (pRenderNode!=NULL);
	const CBaseObject* pObject = itEty->second;
	assert (pObject!=NULL);

	CLogFile::FormatLine("P Brush name: %s\n", pRenderNode->GetName() );

	GenerateOcclusionMasks( pRenderNode, true );

	t_mapRenderNodeOcclusionChannelMaskIterator OcclusionMaskIt = m_OcclusionChannelMaskMap.find(pRenderNode);
	sOcclusionChannelMask *OCM = &(OcclusionMaskIt->second);

	//Hack -> we don't want to be check 16 channel, 1 enougth
	OCM->m_ActiveChannelNumber = sceneCtx.m_nSlidingChannelNumber;

	t_mmapRLMeshRange rlMeshRange = m_mmapRLMeshes.equal_range(pRenderMesh);
	f32 fQuality = 1.f;
	bool bFirstimeTest = true;
	bool bDirection_Bigger = true;
	t_mmapRLMeshes::const_iterator meshIt;
	CRLSurface *pTestSurface = NULL;

	//A small binary search for quality
	while(1)
	{
	bool bAllocated = true;
	for(meshIt = rlMeshRange.first; meshIt!=rlMeshRange.second; meshIt++)
	{
	CRLMesh* rlMesh = meshIt->second;
	if( false == rlMesh->CheckValidSize( fQuality ) )
	{
	bAllocated = false;
	break;
	}
	}

	if( bAllocated ) 
	{
	pTestSurface = new CRLSurface(sceneCtx.m_nLightmapPageWidth,sceneCtx.m_nLightmapPageHeight);
	pTestSurface->SetLastUsedOcclusionChannel(sceneCtx.m_numComponents);
	m_rlSurfaces.clear();
	m_rlSurfaces.push_back(pTestSurface);

	int32 nCacheFilePosition = 0;
	for(meshIt = rlMeshRange.first; meshIt!=rlMeshRange.second; meshIt++)
	{
	CRLMesh* rlMesh = meshIt->second;
	if( false == rlMesh->RasterizeValidPixels( fQuality, pCacheFile, nCacheFilePosition ) )
	{
	sceneCtx.m_bMakeLightMap = bLM;
	sceneCtx.m_bMakeOcclMap = bOM;
	sceneCtx.m_bMakeRAE = bRAE;
	CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"RAM compiler: rasterization error.\r\n");
	return false;
	}
	}

	if( pCacheFile )
	fclose(pCacheFile);

	//try to put all groups of brush into 1 surface
	bAllocated = AllocateSurfaceSpace(rlMeshRange, NULL, pCacheFile, NULL,  false, OCM, &m_rlSurfaces );

	if( pCacheFile )
	fclose(pCacheFile);

	//release used memories
	SAFE_DELETE( pTestSurface );
	for(meshIt = rlMeshRange.first; meshIt!=rlMeshRange.second; meshIt++)
	{
	CRLMesh* rlMesh = meshIt->second;
	rlMesh->ReleaseAllGroupSpanBuffer();
	}

	//release last time used memories
	SAFE_DELETE( pTestSurface );
	}

	if( bFirstimeTest )
	{
	bFirstimeTest = false;
	bDirection_Bigger = bAllocated;
	}

	if( bDirection_Bigger )
	{
	if( bAllocated )
	{
	if( fQuality >= 1.f )
	break;
	fQuality *= 2.f;
	}
	else
	{
	fQuality *= 0.5f;
	break;
	}
	}
	else
	{
	if( bAllocated )
	break;
	fQuality *= 0.5f;
	}

	}//while

	for(meshIt = rlMeshRange.first; meshIt!=rlMeshRange.second; meshIt++)
	{
	CRLMesh* rlMesh = meshIt->second;
	rlMesh->ReleaseAllGroup();
	}

	//set a per brush parameter for maximum quality
	if( OBJTYPE_BRUSH == pObject->GetType() )
	((CBrushObject *)pObject)->SetLightmapQuality( fQuality );
	else
	if( OBJTYPE_VOXEL == pObject->GetType() )
	((CVoxelObject *)pObject)->SetLightmapQuality( fQuality );
	}//vNodes

	SAFE_DELETE(g_pGC); //comment it if you want to debug octree
	ReleaseAllMeshesDirectly();

	}//setupbrushes

	//continouse rendering inside the visarea - maximize how much triangle used in one pass
	} while( pNextRenderNode != NULL);

	//Search next vis area pointer in the list
	for( ; vNodesIt < vNodes.end(); ++vNodesIt )
	{
	IRenderNode *pRendN = vNodesIt->first;
	if( pRendN && (IVisArea*)pRendN->m_pVisArea != pVisArea )
	{
	pVisArea = (IVisArea*)pRendN->m_pVisArea;
	break;
	}
	}
	if( vNodesIt == vNodes.end() )
	pVisArea = NULL;
	} //for Vis area

	SAFE_DELETE( g_pGC );

	sceneCtx.m_bMakeLightMap = bLM;
	sceneCtx.m_bMakeOcclMap = bOM;
	sceneCtx.m_bMakeRAE = bRAE;
	sceneCtx.m_bDistributedMap = bDM;*/
	assert(0);	//not supported anymore since the multiobject-brush-support is/was added
	return true;
}


//===================================================================================
// Method				:	SetupLights
// Description	:	Setup the light list
//===================================================================================
void CLightScene::SetupLights()
{
	Caption("Setup Lights ...");

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	I3DEngine* pI3DEngine = GetIEditor()->Get3DEngine();

	//Get lights
	const PodArray<CDLight*>* pListLights = pI3DEngine->GetStaticLightSources();

	//Get sun light
	if (sceneCtx.m_bUseSunLight)
	{
		sceneCtx.m_vSunColor = pI3DEngine->GetSunColor();

		sceneCtx.m_vSunPos = pI3DEngine->GetSunDir();
		sceneCtx.m_vSunDir = /*Vec3(1,2,3) */- pI3DEngine->GetSunDir();
		sceneCtx.m_vSunDir.Normalize();

#ifdef _DEBUG
		CLogFile::FormatLine("Sun Direction: %f %f %f\r\n",
			sceneCtx.m_vSunDir.x, sceneCtx.m_vSunDir.y, sceneCtx.m_vSunDir.z);
#endif
	}


	//set context parameters
	sceneCtx.SetLights(pListLights);
}


void CLightScene::AddSubOccluders(IRenderMesh* pRenderMesh, std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, const Vec3& vLightSpherePosition, const float fLightSphereRadius, const bool bHandleLocalLight, IVisArea* pLightVisArea, const bool bCalculateTriNumber, uint8 *pOccluderBitMask,int nBrushID,IRenderNode *pIEtyRend,Matrix34& mNodeMatrix)
{
	if(!pRenderMesh)
		return;

	//include it
	if( pOccluderBitMask )
		pOccluderBitMask[ (nBrushID)/8 ] |= 1 << ((nBrushID)%8);

	IMaterial* pDefMaterial = pIEtyRend->GetMaterial();
	PodArray<CRenderChunk>* pRenderChunks = pRenderMesh->GetChunks();
	for (int32 nChunk=0; nChunk<pRenderChunks->Count(); nChunk++)
	{
		CRenderChunk rendChunk = (*pRenderChunks)[nChunk];

		if (rendChunk.m_nMatFlags & MTL_FLAG_NODRAW)
			continue;

		SShaderItem shaderItem;
		IRenderShaderResources* pShaderResources = NULL;
		//multi-material shader retrieve
		if (pDefMaterial!= NULL)
		{
			shaderItem = pDefMaterial->GetShaderItem(rendChunk.m_nMatID);
			pShaderResources = shaderItem.m_pShaderResources;

			{ // test for old assets
				CString sShaderName = shaderItem.m_pShader->GetName();
				if(sShaderName.Find("templdecal") != -1)  	//do not compute decals
					continue;
				if(sShaderName.Find("nodraw") != -1)  	//do not compute decals
					continue;
			}

			if (pShaderResources && pShaderResources->GetOpacity() != 1.0f)
				continue;
			if (pShaderResources && pShaderResources->GetAlphaRef() != 0.f)
				continue;
		}

		//check / chunk the distance...
		//if( false == Overlap::Sphere_Sphere( Sph, Sphere( rendChunk.m_vCenter, rendChunk.m_fRadius) ) )
		//	continue;

		if( bCalculateTriNumber )
		{
			//							CLogFile::FormatLine("ADDOCC: %s/%s/%d: %d tri\n", pVisArea ? pVisArea->GetName() : "NULL", pIEtyRend->GetName(), nChunk, rendChunk.nNumIndices / 3 );
			m_nTriNumber +=	rendChunk.nNumIndices / 3;
			m_mmapGeomMeshesTemporaly.insert(t_pairGeomMesh(pIEtyRend, NULL));
		}
		else
		{
			CGeomMesh* pGeomMesh = new CGeomMesh( pRenderMesh, &mNodeMatrix, &rendChunk, pShaderResources, NULL );
			m_mmapGeomMeshes.insert(t_pairGeomMesh(pIEtyRend, pGeomMesh));
		}
	}
}

//===================================================================================
// Method				:	AddOccluders
// Description	:	Add occluders from a given visarea - if they aren't added before... input: a position + a radius + it is local - or global thing..
//===================================================================================
void CLightScene::AddOccluders( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, const Vec3& vLightSpherePosition, const float fLightSphereRadius, const bool bHandleLocalLight, IVisArea* pLightVisArea, const bool bCalculateTriNumber, uint8 *pOccluderBitMask )
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	IRenderNode *pIEtyRend;
	//std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator itEty;

	Sphere Sph( vLightSpherePosition, fLightSphereRadius);

	int32 nMinX,nMinY,nMinZ;
	int32 nMaxX,nMaxY,nMaxZ;
	int32 nX,nY,nZ;
	uint32* pCell;

	//convert it to int32
	nMinX = (int32)floor( (vLightSpherePosition.x - fLightSphereRadius - m_AABoxOfScene.min.x) * m_vOccluderCellSize.x );
	nMinY = (int32)floor( (vLightSpherePosition.y - fLightSphereRadius - m_AABoxOfScene.min.y) * m_vOccluderCellSize.y );
	nMinZ = (int32)floor( (vLightSpherePosition.z - fLightSphereRadius - m_AABoxOfScene.min.z) * m_vOccluderCellSize.z );
	nMaxX = (int32)ceil( (vLightSpherePosition.x + fLightSphereRadius - m_AABoxOfScene.min.x) * m_vOccluderCellSize.x )+1;
	nMaxY = (int32)ceil( (vLightSpherePosition.y + fLightSphereRadius - m_AABoxOfScene.min.y) * m_vOccluderCellSize.y )+1;
	nMaxZ = (int32)ceil( (vLightSpherePosition.z + fLightSphereRadius - m_AABoxOfScene.min.z) * m_vOccluderCellSize.z )+1;

	//clamp to the size of BBox of Scene
	nMinX = nMinX < 0 ? 0 : nMinX;
	nMinX = nMinX > m_nOccluderGridWith ? m_nOccluderGridWith : nMinX;
	nMinY = nMinY < 0 ? 0 : nMinY;
	nMinY = nMinY > m_nOccluderGridHeight ? m_nOccluderGridHeight : nMinY;
	nMinZ = nMinZ < 0 ? 0 : nMinZ;
	nMinZ = nMinZ > m_nOccluderGridDeepness ? m_nOccluderGridDeepness : nMinZ;
	nMaxX = nMaxX < 0 ? 0 : nMaxX;
	nMaxX = nMaxX > m_nOccluderGridWith ? m_nOccluderGridWith : nMaxX;
	nMaxY = nMaxY < 0 ? 0 : nMaxY;
	nMaxY = nMaxY > m_nOccluderGridHeight ? m_nOccluderGridHeight : nMaxY;
	nMaxZ = nMaxZ < 0 ? 0 : nMaxZ;
	nMaxZ = nMaxZ > m_nOccluderGridDeepness ? m_nOccluderGridDeepness : nMaxZ;

	int nActualBrushNumber = m_BrushRenderable.size();
	int nBrushID = 0;
	//generate the meshes
	//	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	for( nZ = nMinZ; nZ < nMaxZ; ++nZ )
		for( nY = nMinY; nY < nMaxY; ++nY )
			for( nX = nMinX; nX < nMaxX; ++nX )
			{
				int32 nID = (nZ*m_nOccluderGridHeight+nY)*m_nOccluderGridWith+nX;
				pCell = m_pOccluderGrid[nID];
				for( uint32 nCelli = 0; nCelli < m_pOccluderGridCellSize[ nID ]; ++nCelli )
				{
					nBrushID = pCell[ nCelli ];
					/*
					//it's a renderable mesh... don't do anything with this...
					if( nActualBrushNumber > nBrushID )
					{
					if( m_BrushRenderable[ nBrushID ] )
					continue;
					}
					*/
					//check it is already an occluder never try it 2 times
					if( pOccluderBitMask && pOccluderBitMask[ (nBrushID)/8 ] & 1 << ((nBrushID)%8) )
						continue;

					//					itEty = vNodes[ nBrushID ];

					pIEtyRend = vNodes[ nBrushID ].first;
					if( NULL == pIEtyRend )
						continue;

					IVisArea* pVisArea = (IVisArea*)pIEtyRend->GetEntityVisArea();

					//it's part of the geom meshes don't put it more than 1 time
					//		if( bCalculateTriNumber && m_mmapGeomMeshesTemporaly.find(pIEtyRend) != m_mmapGeomMeshesTemporaly.end() )
					//			continue;

					//it's part of the geom meshes don't put it more than 1 time
					//		if( m_mmapGeomMeshes.find(pIEtyRend) != m_mmapGeomMeshes.end() )
					//			continue;

					//cast shadow anyway ?
					if( !(pIEtyRend->GetRndFlags() & ERF_CASTSHADOWINTORAMMAP) )
						continue;

					//check the real interaction between the sphere & the brush
					if( false == Overlap::Sphere_AABB( Sph, pIEtyRend->GetBBox() ) )
						continue;

					//visibility tests...
					if( pVisArea  )
					{
						if( false == pVisArea->IsSphereInsideVisArea( vLightSpherePosition, fLightSphereRadius ) )
							continue;

						//if it's a realtime calulated light... and only for 1 visarea, check the distance between the 2 one
						if( bHandleLocalLight )
						{
							if( pLightVisArea )
							{
								int nMaxRecursion = 2;
								if( false == pVisArea->FindVisArea(pLightVisArea, nMaxRecursion, true) )		// try also portal volumes
									continue;
							}
							else
								continue;		//if no visarea & locale light... no way
						}
						else
						{
							int nMaxRecursion = 5;
							if( false == pVisArea->FindVisArea(pLightVisArea, nMaxRecursion, true) )		// try also portal volumes
								continue;
						}
					}
					else
					{
						if( bHandleLocalLight && pLightVisArea )
							continue;

						//check the connection between them...
						if( pVisArea )
						{
							int nMaxRecursion = 5;
							if( false == pVisArea->FindVisArea(NULL,nMaxRecursion, true) )		// try also portal volumes
								continue;
						}
					}

					//add to geommeshes
					Matrix34 mNodeMatrix;
					pIEtyRend->GetEntityStatObj(0, 0, &mNodeMatrix);
					IRenderMesh *pRenderMesh = pIEtyRend->GetRenderMesh(0);

					//Voxels vertex container used as rendermesh
					CBaseObject *pObject = vNodes[ nBrushID ].second;
					if( pObject && OBJTYPE_VOXEL == pObject->GetType() && pRenderMesh )
						pRenderMesh = pRenderMesh->GetVertexContainer();

					if(pRenderMesh )
						AddSubOccluders(pRenderMesh,vNodes,vLightSpherePosition,fLightSphereRadius,bHandleLocalLight,pLightVisArea,bCalculateTriNumber,pOccluderBitMask,nBrushID,pIEtyRend,mNodeMatrix);
					else
					{
						IStatObj* pStatObj	=	pIEtyRend->GetEntityStatObj(0, 0, 0);
						if(pStatObj)
							for(uint a=0;a<pStatObj->GetSubObjectCount();a++)
							{
								IStatObj::SSubObject* pSubObject	=	pStatObj->GetSubObject(a);
								if(pSubObject->nType==STATIC_SUB_OBJECT_MESH && pSubObject->pStatObj && pSubObject->pStatObj->GetRenderMesh())
									AddSubOccluders(pSubObject->pStatObj->GetRenderMesh(),vNodes,vLightSpherePosition,fLightSphereRadius,bHandleLocalLight,pLightVisArea,bCalculateTriNumber,pOccluderBitMask,nBrushID,pIEtyRend,pSubObject->bIdentityMatrix?mNodeMatrix:mNodeMatrix * pSubObject->tm);
							}
					}
				}
			}
}


//===================================================================================
// Method				:	SetupBrushes
// Description	:	Setup and unwrap the brushes
//===================================================================================
bool CLightScene::SetupBrushes( const bool bOptimizeByLights, const bool bUseLightmapQuality, std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, const std::set<const CBaseObject*>& vSelectionIndices, const ELMMode Mode, const IVisArea *pVisArea, IRenderNode** pLastRenderNode, int32 nVisAreaID, int32 nVisAreaNumber )
{
	//////////////////////////////////////////////////////////////////////////
	// Unwrap instances to world geometry & unwrap meshes for rasterization
	//////////////////////////////////////////////////////////////////////////
	assert(m_mmapRLMeshes.size()==0);
	assert(m_mmapGeomMeshes.size()==0);
	m_mmapRLMeshes.clear();
	m_mmapGeomMeshes.clear();
	m_BrushRenderable.clear();
	m_BrushOcclusion.clear();
	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator itEty;
	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator itEnd = vNodes.end();
	IRenderNode *pIEtyRend;
	IRenderNode *pIEtyPrevRend = NULL;

	//Skipp free visareas...
	if( Mode == ELMMode_SEL  )
	{
		//serach a "valid" brush in that visarea... if not make a short 
		for (itEty=vNodes.begin(); itEty!=itEnd; itEty++)
		{
			pIEtyRend = itEty->first;			
			if(pIEtyRend == NULL )
				continue;

			if( (IVisArea*)pIEtyRend->GetEntityVisArea() != pVisArea )
				continue;

			if( vSelectionIndices.find(itEty->second) != vSelectionIndices.end() )
				break;
		}

		//skippable visarea
		if( itEty == itEnd )
		{
			for (itEty=vNodes.begin(); itEty!=itEnd; itEty++)
				m_BrushRenderable.push_back(0);

			if( pLastRenderNode )
				*pLastRenderNode = NULL;
			//fast skipp
			return false;
		}
	}

	int32 numBrushes = 0;
	int32 nCurBrush = 0;

	char szCaptionText[1024];
	sprintf(szCaptionText,"%d/%d visarea - Generate occluder list...", nVisAreaID,nVisAreaNumber );
	Caption(szCaptionText);
	CLogFile::FormatLine("SetupBrushes - Occluder Generation\n" );
	ProgressSetBounds(0,100);
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();

	IRenderNode* pNextLastRenderNode = NULL;
	IRenderNode* pSearchRenderNode = NULL;
	int32 nPrevTriNumber = 0;

	if( pLastRenderNode )
		pSearchRenderNode = *pLastRenderNode;

	//	int32 nRenderableTriNumber = 0;
	m_nTriNumber = 0;
	m_mmapGeomMeshesTemporaly.clear();

	t_pairEntityId LightIDs,LightIDs2;

	int32 nNodeSize = (vNodes.size()+7)/8;
	uint8* pOccluderBitMask = new uint8[ nNodeSize ];
	if( pOccluderBitMask )
		memset( pOccluderBitMask, 0, sizeof(uint8)*nNodeSize );

//	m_BrushOcclusion.clear();
	int32 nBrushID = -1;
	f32 fNodeSize = 1.f / (f32)vNodes.size();
	//calculate the space needed for triangles (we need this, because the incredible number of "new" functions generate
	//a lot of memory fragmentation)
	for (itEty=vNodes.begin(); itEty!=itEnd; itEty++)
	{
//		static int ac=0;
//		CLogFile::FormatLine("Chunks: %d dummy", ac++ );
		++nBrushID;

		//Wait Progress
		SetProgress(((f32)nBrushID)*fNodeSize);

		pIEtyRend = itEty->first;
		if(pIEtyRend == NULL )
		{
			m_BrushOcclusion.push_back(0);
			continue;
		}

		if( (IVisArea*)pIEtyRend->GetEntityVisArea() != pVisArea )
		{
			m_BrushOcclusion.push_back(0);
			continue;
		}

		if( Mode == ELMMode_SEL && vSelectionIndices.find(itEty->second) == vSelectionIndices.end() )
		{
			m_BrushOcclusion.push_back(0);
			continue;
		}

		//search the last used node...
		if( pSearchRenderNode )
		{
			m_BrushOcclusion.push_back(0);
			//that was, what i search
			if( pSearchRenderNode == pIEtyRend )
				pSearchRenderNode = NULL;
			continue;
		}

		//just if not counted as occluder before...
		//		if( m_mmapGeomMeshesTemporaly.find(pIEtyRend) == m_mmapGeomMeshesTemporaly.end() )
		//		if( pOccluderBitMask && 0 ==(pOccluderBitMask[ nBrushID / 8 ] & (1<<(nBrushID%8))) )
		//		nRenderableTriNumber += GetNeededTriangles( pIEtyRend, itEty->second, pIEtyRend->GetRndFlags() & ERF_USERAMMAPS, pIEtyRend->GetRndFlags() & ERF_CASTSHADOWINTORAMMAP || sceneCtx.m_bMakeRAE, bOptimizeByLights );

		//get the occluder geometrys for that object - IT CAN BE OVERESTIMATE THE NESSESARY TRINUMBER,
		//BUT NEVER UNDERESTIMATE.
		if( bOptimizeByLights && false == sceneCtx.m_bDistributedMap )
		{

			LightIDs.resize(0);
			LightIDs2.resize(0);

			//get all light, affecting submeshes...
			//Generate Occlusion light list
			if( sceneCtx.m_bMakeOcclMap )
			{
				Rought_AttachOcclusionLightToList( LightIDs, pIEtyRend );

				//add the shadow caster vis areas into the octree.. they will create shadows...
				int32	nLightNumber = LightIDs.size();
				for(int32 i=0; i<nLightNumber; ++i)
				{
					CDLight* pLight = pListLights->GetAt( LightIDs[i].second );
					IVisArea* pLightVisArea =( pLight->m_pOwner!=(IRenderNode*)-1 && pLight->m_pOwner ) ? (IVisArea*)pLight->m_pOwner->GetEntityVisArea() : NULL;
					AddOccluders( vNodes, pLight->m_BaseOrigin, pLight->m_fRadius , pLight->m_Flags & DLF_THIS_AREA_ONLY, pLightVisArea, true, pOccluderBitMask );
				}
			}
			//TODO: ADD LIGHTMAP light list too..
			/*
			if( sceneCtx.m_bMakeLightMap )
			{
			rlMesh->AttachLightmapLightToList( LightIDs2, (IVisArea*)pRenderNode->m_pVisArea );
			nLightNumber = LightIDs2.size();
			for(int32 i=0; i<nLightNumber; ++i)
			{
			CDLight* pLight = pListLights->GetAt( LightIDs2[i].second );
			AddOccluders( vNodes, pLight->m_BaseOrigin, pLight->m_fRadius , false, NULL, true, pOccluderBitMask );
			}
			}
			*/
			if( sceneCtx.m_bMakeRAE )
			{
				m_BrushOcclusion.push_back(1);
				Vec3 vCenter = pIEtyRend->GetBBox().GetCenter();
//				float fRadius = pIEtyRend->GetRadius() + 50;//sceneCtx.m_fMaxAmbientOcclusionSearchRadius;
				float fRadius = pIEtyRend->GetBBox().GetRadius() +	sceneCtx.m_fMaxAmbientOcclusionSearchRadius;
				AddOccluders( vNodes, vCenter, fRadius, false, pIEtyRend->GetEntityVisArea(), true, pOccluderBitMask );
			}
		}

		//if the maximum size enabled: check... if the new one is grove over the enabled maximum not add to the list
		numBrushes++;
		if( pLastRenderNode && m_nTriNumber >= sceneCtx.m_nMaxTrianglePerPass ) //|| (sceneCtx.m_bMakeRAE && !sceneCtx.m_bMakeOcclMap && !sceneCtx.m_bMakeLightMap)  )
		{
			//if there are a prev brush use that one as last...
			if( pIEtyPrevRend )
			{
				pNextLastRenderNode = pIEtyPrevRend;
				m_nTriNumber = nPrevTriNumber;
			}
			else
				pNextLastRenderNode = pIEtyRend;
			break;
		}

		pIEtyPrevRend = pIEtyRend;
		nPrevTriNumber = m_nTriNumber;
	}//for( pIEtyRend )

	//reserve the memory...
	//	m_nTriNumber += nRenderableTriNumber;
	//	CLogFile::FormatLine("ReserveTracable: %d\n", m_nTriNumber );
	g_pGC->ReserveTracable( m_nTriNumber );
	//	if( false == g_pGC->ReserveVertices( nRenderableTriNumber ) )
	//		return false;


	//set again
	if( pLastRenderNode )
		pSearchRenderNode = *pLastRenderNode;
	bool bSkipAll = false;


	nCurBrush = 0;
	sprintf(szCaptionText,"%d/%d visarea - Generate UV sets...", nVisAreaID,nVisAreaNumber );
	Caption(szCaptionText);
	CLogFile::FormatLine("SetupBrushes - UVSet generation\n" );
	ProgressSetBounds(0,100);

	//generate the meshes
	for (itEty=vNodes.begin(); itEty!=itEnd; itEty++)
	{
		pIEtyRend = itEty->first;
		if(pIEtyRend == NULL)
		{
			m_BrushRenderable.push_back(0);		//not renderable
			continue;
		}

		if( (IVisArea*)pIEtyRend->GetEntityVisArea() != pVisArea )
		{
			m_BrushRenderable.push_back(0);		//not renderable
			continue;
		}

		if( Mode == ELMMode_SEL && vSelectionIndices.find(itEty->second) == vSelectionIndices.end() )
		{
			m_BrushRenderable.push_back(0);
			continue;
		}

		//search the last used node...
		if( pSearchRenderNode )
		{
			m_BrushRenderable.push_back(0);
			//that was, what i search
			if( pSearchRenderNode == pIEtyRend )
				pSearchRenderNode = NULL;
			continue;
		}

		if( bSkipAll )
		{
			m_BrushRenderable.push_back(0);
			continue;
		}

		//check the last one...
		if( pNextLastRenderNode == pIEtyRend )
			bSkipAll = true;


		float fLightmapQuality;
		const CBaseObject* pObject = itEty->second;
		bool  bNeedToAutoGenerateTextureCoordinates;
		if( OBJTYPE_BRUSH == pObject->GetType() )
		{
			const CBrushObject *pBrushObject = ((CBrushObject *)itEty->second);
			fLightmapQuality = bUseLightmapQuality ? pBrushObject->GetLightmapQuality() : 1;
			bNeedToAutoGenerateTextureCoordinates = false;
		}
		else
			if( OBJTYPE_VOXEL == pObject->GetType() )
			{
				const CVoxelObject *pVoxelObject = ((CVoxelObject *)itEty->second);
				fLightmapQuality = bUseLightmapQuality ? pVoxelObject->GetLightmapQuality() : 1;
				bNeedToAutoGenerateTextureCoordinates = true;
			}
			else
			{
				fLightmapQuality = 1;
				bNeedToAutoGenerateTextureCoordinates = false;
			}

			//		CLogFile::FormatLine("S Brush name: %s\n", pIEtyRend->GetName() );
			CreateRLMeshes(pIEtyRend, itEty->second, pIEtyRend->GetRndFlags() & ERF_USERAMMAPS, pIEtyRend->GetRndFlags() & ERF_CASTSHADOWINTORAMMAP || sceneCtx.m_bMakeRAE, fLightmapQuality, bOptimizeByLights, false, bNeedToAutoGenerateTextureCoordinates );

			//Wait Progress
			SetProgress((f32)nCurBrush/(f32)numBrushes);
			++nCurBrush;
	}

	if( pLastRenderNode )
		*pLastRenderNode = pNextLastRenderNode;

	SAFE_DELETE_ARRAY( pOccluderBitMask );
	return true;
}

//===================================================================================
// Method				:	GenerateLightListPerRenderNode
// Description	:	Generate a light list per rendernode (rendering speedup & needed for occlusion mapping )
//===================================================================================
void CLightScene::GenerateLightListPerRenderNode( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes,bool bAddOccluders )
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	if( sceneCtx.m_bDistributedMap )
		return;


	const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();
	t_pairEntityId LightIDs,LightIDs2;
	t_mmapRLMeshes::const_iterator meshIt;

	bool bLightOcclusion = sceneCtx.m_bMakeLightMap || sceneCtx.m_bMakeOcclMap;

	int32 nNodeSize = (vNodes.size()+7)/8;
	uint8* pOccluderBitMask = new uint8[ nNodeSize ];

	//get all triangle number
	int32 nBrushID = 0;
	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator itEty;
	if( bAddOccluders && bLightOcclusion ) 
	{
		/*		if( pOccluderBitMask )
		memset( pOccluderBitMask, 0, sizeof(uint8)*nNodeSize );

		for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
		{
		if (m_BrushRenderable[nBrushID]==0)
		{
		++nBrushID;
		continue;
		}
		++nBrushID;

		IRenderNode* pRenderNode = itEty->first;
		assert (pRenderNode!=NULL);

		LightIDs.resize(0);
		LightIDs2.resize(0);

		t_mmapRLMeshRange rlMeshRange = m_mmapRLMeshes.equal_range(pRenderMesh);
		for(meshIt = rlMeshRange.first; meshIt!=rlMeshRange.second; meshIt++ )
		{
		//get all light, affecting submeshes...
		CRLMesh* rlMesh = meshIt->second;
		//Generate Occlusion light list
		if( sceneCtx.m_bMakeOcclMap )
		rlMesh->AttachOcclusionLightToList( LightIDs, (IVisArea*)pRenderNode->m_pVisArea );
		if( sceneCtx.m_bMakeLightMap )
		rlMesh->AttachLightmapLightToList( LightIDs2, (IVisArea*)pRenderNode->m_pVisArea );
		} //RLMeshes loop
		}*/
		assert(0);	//not supported anymore since the multiobject-brush-support is/was added

	}

	m_mmapGeomMeshesTemporaly.clear();
	if( pOccluderBitMask )
		memset( pOccluderBitMask, 0, sizeof(uint8)*nNodeSize );

	f32 fNodeSize = 1.f / (f32)vNodes.size();

	//generate the lights / rendernode pairs
	nBrushID = 0;
	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	{
//		static int ac=0;
//		CLogFile::FormatLine("Chunks: %d dummy", ac++ );
		//Wait Progress
		SetProgress(((f32)nBrushID)*fNodeSize);

		if (m_BrushRenderable[nBrushID]==0)
		{
			++nBrushID;
			continue;
		}
		++nBrushID;

		IRenderNode* pRenderNode = itEty->first;
		assert (pRenderNode!=NULL);

		LightIDs.resize(0);
		LightIDs2.resize(0);

		if( bLightOcclusion )
		{
			/*			t_mmapRLMeshRange rlMeshRange = m_mmapRLMeshes.equal_range(pRenderMesh);
			for(meshIt = rlMeshRange.first; meshIt!=rlMeshRange.second; meshIt++ )
			{
			//get all light, affecting submeshes...
			CRLMesh* rlMesh = meshIt->second;
			//Generate Occlusion light list
			if( sceneCtx.m_bMakeOcclMap )
			rlMesh->AttachOcclusionLightToList( LightIDs, (IVisArea*)pRenderNode->m_pVisArea );
			if( sceneCtx.m_bMakeLightMap )
			rlMesh->AttachLightmapLightToList( LightIDs2, (IVisArea*)pRenderNode->m_pVisArea );
			} //RLMeshes loop

			if( bAddOccluders )
			{
			//add the shadow caster vis areas into the octree.. they will create shadows...
			int32	nLightNumber = LightIDs.size();
			for(int32 i=0; i<nLightNumber; ++i)
			{
			CDLight* pLight = pListLights->GetAt( LightIDs[i].second );
			IVisArea* pLightVisArea =( pLight->m_pOwner!=(IRenderNode*)-1 && pLight->m_pOwner ) ? (IVisArea*)pLight->m_pOwner->m_pVisArea : NULL;
			AddOccluders( vNodes, pLight->m_BaseOrigin, pLight->m_fRadius , pLight->m_Flags & DLF_THIS_AREA_ONLY, pLightVisArea, false, pOccluderBitMask );
			}

			nLightNumber = LightIDs2.size();
			for(int32 i=0; i<nLightNumber; ++i)
			{
			CDLight* pLight = pListLights->GetAt( LightIDs2[i].second );
			AddOccluders( vNodes, pLight->m_BaseOrigin, pLight->m_fRadius , false, NULL, false, pOccluderBitMask );
			}
			}//bAddOccluders*/
			assert(0);	//not supported anymore since the multiobject-brush-support is/was added
		}//bLightInteraction

		if( bAddOccluders && sceneCtx.m_bMakeRAE )
		{
			Vec3 vCenter = pRenderNode->GetBBox().GetCenter();
//			float fRadius = pRenderNode->GetBBox().GetRadius() + 50;//sceneCtx.m_fMaxAmbientOcclusionSearchRadius;
			float fRadius = pRenderNode->GetBBox().GetRadius() +	sceneCtx.m_fMaxAmbientOcclusionSearchRadius;
			AddOccluders( vNodes, vCenter, fRadius, false, pRenderNode->GetEntityVisArea(), false, pOccluderBitMask );
		}

		//store
		m_OcclusionLightPairs.insert( t_pairRenderNodeEntityIdPair(pRenderNode, LightIDs) );
		m_LightmapLightPairs.insert( t_pairRenderNodeEntityIdPair(pRenderNode, LightIDs2) );
	}
	SAFE_DELETE_ARRAY( pOccluderBitMask );
}

//===================================================================================
// Method				:	Parametrizer
// Description	:	Parametrize the actual render meshes
//===================================================================================
bool CLightScene::SubParameterizer(IRenderNode* pRenderNode,IRenderMesh* pRenderMesh)
{
	t_mmapRLMeshRange rlMeshRange = m_mmapRLMeshes.equal_range(std::pair<IRenderNode*,IRenderMesh*>(pRenderNode,pRenderMesh));
	bool bFirstimeTest = true;
	bool bDirection_Bigger = true;
	t_mmapRLMeshes::const_iterator meshIt;
	CRLSurface *pTestSurface = NULL;
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	FILE *pCacheFile = NULL;
	int32 nCacheFilePosition = 0;

	sOcclusionChannelMask OCM;
	OCM.m_ActiveChannelNumber = 1;
	OCM.m_FirstChannel = 0;
	OCM.m_ChannelMap[0] = 0;
	for( int i = 1; i < NUM_COMPONENTS*4; ++i )
		OCM.m_ChannelMap[i] = (uint8)0xff;

	f32 fQualityModifier = 1.f;

	//Fast resizer
	bool bAllocated;
	do 
	{
		bAllocated = true;

		for(meshIt = rlMeshRange.first;meshIt!=rlMeshRange.second; meshIt++)
		{
			CRLMesh* rlMesh = meshIt->second;
			if( false == rlMesh->CheckValidSize( fQualityModifier ) )
			{
				fQualityModifier *= 0.75f;
				bAllocated = false;
				break;
			}
		}
	} while( !bAllocated);

	std::vector<CRLSurface*> LocalSurfaceList;

	//A small binary search for quality
	while(fQualityModifier<1.f)
	{
		pTestSurface = new CRLSurface(sceneCtx.m_nLightmapPageWidth,sceneCtx.m_nLightmapPageHeight);
		pTestSurface->SetLastUsedOcclusionChannel(sceneCtx.m_numComponents);
		LocalSurfaceList.clear();
		LocalSurfaceList.push_back(pTestSurface);

		for(meshIt = rlMeshRange.first; meshIt!=rlMeshRange.second; meshIt++)
		{
			CRLMesh* rlMesh = meshIt->second;
			if( false == rlMesh->RasterizeValidPixels( fQualityModifier, pCacheFile, nCacheFilePosition ) )
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"RAM compiler: rasterization error.\r\n");
				return false;
			}
		}
		//try to put all groups of brush into 1 surface
		bAllocated = AllocateSurfaceSpace(rlMeshRange, NULL, pCacheFile, NULL,  false, &OCM, &LocalSurfaceList );

		//release used memories
		SAFE_DELETE( pTestSurface );
		for(meshIt = rlMeshRange.first; meshIt!=rlMeshRange.second; meshIt++)
		{
			CRLMesh* rlMesh = meshIt->second;
			rlMesh->ReleaseAllGroupSpanBuffer();
		}

		//if allocated - it is a good size
		if( bAllocated )
			break;

		//resize if it is a problematic one...
		fQualityModifier *= 0.5f;
	}//while*/

	for(meshIt = rlMeshRange.first; meshIt!=rlMeshRange.second; meshIt++)
	{
		CRLMesh* rlMesh = meshIt->second;
		rlMesh->SetLightmapQuality( rlMesh->GetLightmapQuality() * fQualityModifier );
	}
	return true;
}

bool CLightScene::Parametrizer( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, IVisArea *pVisArea )
{
	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator itEty;
	int32 nCurBrush = 0;
	int32 nBrushID = 0;
	int32 numBrushes = 0;
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	ProgressSetBounds(0,100);

	//get the real numBrushes
	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	{
		if (m_BrushRenderable[nBrushID]!=0)
			numBrushes++;
		++nBrushID;
	}



	nBrushID = 0;
	//////////////////////////////////////////////////////////////////////////
	// Process all brushes
	//////////////////////////////////////////////////////////////////////////
	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	{
		if (m_BrushRenderable[nBrushID++]==0)
			continue;

		//Wait Progress
		SetProgress((f32)nCurBrush/(f32)numBrushes);
		nCurBrush++;

		//std::vector<t_pairInt> arrOffsetXY;
		IRenderNode* pRenderNode = itEty->first;

		CLogFile::FormatLine("PARAM name: %s\n", pRenderNode->GetName() );

		if(pRenderNode->GetRenderMesh(0))
			SubParameterizer(pRenderNode,pRenderNode->GetRenderMesh(0));
		else
		{
			IStatObj* pStatObj	=	pRenderNode->GetEntityStatObj(0, 0, 0);
			if(pStatObj)
				for(uint a=0;a<pStatObj->GetSubObjectCount();a++)
				{
					IStatObj::SSubObject* pSubObject	=	pStatObj->GetSubObject(a);
					if(pSubObject->nType==STATIC_SUB_OBJECT_MESH && pSubObject->pStatObj && pSubObject->pStatObj->GetRenderMesh())
						SubParameterizer(pRenderNode,pSubObject->pStatObj->GetRenderMesh());
				}
		}

	}//vNodes
//	assert(0);	//not supported anymore since the multiobject-brush-support is/was added

	return true;
}

bool CLightScene::SubSurfacePacker(FILE *pSurfaceOutputFile,FILE *pCacheFile,IRenderNode* pRenderNode,int32 SubObjIdx,IRenderMesh* pRenderMesh,IVisArea *pVisArea)
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	CLogFile::FormatLine("RAM COMPILER DEBUG:\n %x Object: %s\n", pRenderNode->GetEditorObjectId(),pRenderNode->GetName() );

	GenerateOcclusionMasks( pRenderNode );

	t_mapRenderNodeOcclusionChannelMaskIterator OcclusionMaskIt = m_OcclusionChannelMaskMap.find(pRenderNode);
	sOcclusionChannelMask *OCM = &(OcclusionMaskIt->second);

	t_mmapRLMeshRange rlMeshRange = m_mmapRLMeshes.equal_range(std::pair<IRenderNode*,IRenderMesh*>(pRenderNode,pRenderMesh));

	CRLSurface* pRLSurface = NULL; 
#ifdef SKIP_PACKING
	if (LoadSurfaceSpace(rlMeshRange, &pRLSurface,pSurfaceOutputFile))
#else
	if (AllocateSurfaceSpace(rlMeshRange, &pRLSurface,pCacheFile,pSurfaceOutputFile, false, OCM, &m_rlSurfaces))
#endif
	{
		assert(pRLSurface!=NULL);
		RescaleTexCoords( pRLSurface );

		pRLSurface->AddObjectId(std::pair<int32,int32>(pRenderNode->GetEditorObjectId(),SubObjIdx));
		pRLSurface->AddRenderNodePtr(pRenderNode);

		pRLSurface->ResetMeshBuffer();
	}
	else //unsuccessful allocation //try to create new Lightmap Surface
	{
		m_rlSurfaces.push_back(new CRLSurface(sceneCtx.m_nLightmapPageWidth,sceneCtx.m_nLightmapPageHeight));

		//try to amortize the used memory of surfaces -> at maximum 4 surface "open" at the same time
		if( m_rlSurfaces.size() > LC_MAX_OPEN_SURFACE )
		{
			int32 nSurfaceID = m_rlSurfaces.size() - (LC_MAX_OPEN_SURFACE+1);
			CRLSurface **pClosableSurface = &(m_rlSurfaces[ nSurfaceID ] );
			if( NULL != (*pClosableSurface) )
			{
				CRLSurface *pS = *pClosableSurface;
				if( false == pS->IsFinished() )
				{
					//debug only?						pS->GenerateDebugJPG();	
					GenerateSurface( pS, pCacheFile );
					UpdateRLSurface( pS );
					pS->ReleaseAttachedMeshes();
				}
				SAFE_DELETE( (*pClosableSurface) );
			}
		}
#ifdef SKIP_PACKING
		if (LoadSurfaceSpace(rlMeshRange, &pRLSurface,pSurfaceOutputFile))
#else
		if (AllocateSurfaceSpace(rlMeshRange, &pRLSurface,pCacheFile,pSurfaceOutputFile, false, OCM, &m_rlSurfaces))
#endif
		{
			assert(pRLSurface!=NULL);
			RescaleTexCoords( pRLSurface );

			pRLSurface->AddObjectId(std::pair<int32,int32>(pRenderNode->GetEditorObjectId(),SubObjIdx)); 
			pRLSurface->AddRenderNodePtr(pRenderNode);

			pRLSurface->ResetMeshBuffer();
		}
		else
		{
			//DEBUG messages for easy reproduction....
			if( pVisArea )
				CLogFile::FormatLine("RAM COMPILER DEBUG:\n VisArea name: %s\n", pVisArea->GetName() );
			else
				CLogFile::FormatLine("RAM COMPILER DEBUG:\n VisArea name: NONAME\n" );

			// This function will report datas into log file, to help determine the bug.
			AllocateSurfaceSpace(rlMeshRange, &pRLSurface, pCacheFile,NULL,true, OCM, &m_rlSurfaces);

			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"The packer can not fit a brush into the current RAM map size %ix%i\r\nPlease run the parametrizer!\n",
				sceneCtx.m_nLightmapPageWidth,
				sceneCtx.m_nLightmapPageHeight );

			SAFE_DELETE( m_pISerializationManager );
			return false;
		}//2nd allocation

	} //1th allocation

	//lm tex-coords buffer should match to the pRenderMesh->GetSysVertCount()
	std::vector<TexCoord2Comp> arrMeshLMCoords;
	arrMeshLMCoords.assign(pRenderMesh->GetVertCount(), TexCoord2Comp(0.0f,0.0f));

	t_mapIndTexCoord::const_iterator itTexCoord = m_mapValidTexCoords.begin();
	for(;itTexCoord!=m_mapValidTexCoords.end();itTexCoord++)
	{
		const t_pairIndTexCoord* TexCoord = &*itTexCoord;
		assert(TexCoord->first < pRenderMesh->GetVertCount());

		if( TexCoord->first < pRenderMesh->GetVertCount() )
			arrMeshLMCoords[TexCoord->first] = TexCoord->second;
	}

	assert (arrMeshLMCoords.size()<=65535);

	//ID for Ambient occlusion
	t_mapRenderNodeLightIDsIterator LightIt = m_OcclusionLightPairs.find(pRenderNode);
	if( LightIt == m_OcclusionLightPairs.end() )
	{
		if( arrMeshLMCoords.size() )
			m_pISerializationManager->AddTexCoordData(&arrMeshLMCoords.front(), arrMeshLMCoords.size() , pRenderNode->GetEditorObjectId(),SubObjIdx,((DWORD)(pRenderNode)*pRenderNode->GetEditorObjectId()), NULL,NULL,0, 0);
		else
			m_pISerializationManager->AddTexCoordData(NULL,0 , pRenderNode->GetEditorObjectId(),SubObjIdx,((DWORD)(pRenderNode)*pRenderNode->GetEditorObjectId()), NULL,NULL,0, 0);
	}
	else
	{
		assert( OCM->m_ActiveChannelNumber >= (LightIt->second).size() );
		//fix: make proper hash value 
		//save lm coords for this RenderNode
		EntityId Occls[32];
		int32 nOcclNum = LightIt->second.size();
		nOcclNum = nOcclNum > 16 ? 16 : nOcclNum;
		for( int32 i = 0; i < nOcclNum; ++i )
		{
			Occls[i] = LightIt->second[i].first;
			Occls[i+16] = LightIt->second[i].second;
		}
		if( arrMeshLMCoords.size() )
			m_pISerializationManager->AddTexCoordData(&arrMeshLMCoords.front(), arrMeshLMCoords.size(), pRenderNode->GetEditorObjectId(),SubObjIdx,((DWORD)(pRenderNode)*pRenderNode->GetEditorObjectId()), Occls, &Occls[16], nOcclNum, OCM->m_FirstChannel);
		else
			m_pISerializationManager->AddTexCoordData(NULL,0, pRenderNode->GetEditorObjectId(),SubObjIdx,((DWORD)(pRenderNode)*pRenderNode->GetEditorObjectId()), Occls, &Occls[16], nOcclNum, OCM->m_FirstChannel);
	}
	return true;
}

//===================================================================================
// Method				:	SurfacePacker
// Description	:	Generate the lightmap / occlusionmap surfaces
//===================================================================================
bool CLightScene::SurfacePacker( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, IVisArea *pVisArea )
{
	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator itEty;
	int32 nCurBrush = 0;
	int32 nBrushID = 0;
	int32 numBrushes = 0;
	CSceneContext& sceneCtx = CSceneContext::GetInstance();


	//lightmap packing
	ProgressSetBounds(0,100);

	//Open the cache file....
	FILE *pCacheFile = NULL;
/*
	pCacheFile = fopen( "LCTemp.tmp", "rb");
	if ( NULL == pCacheFile )
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Can't open to read the rasterization temp file.\r\n");
		return false;
	}
*/
	//if empty, add one,free surface
	if( m_rlSurfaces.empty() )
			m_rlSurfaces.push_back(new CRLSurface(sceneCtx.m_nLightmapPageWidth,sceneCtx.m_nLightmapPageHeight));

	FILE *pSurfaceOutputFile;
#ifdef SKIP_PACKING
	pSurfaceOutputFile = fopen( "PackTemp.tmp", "rb");
	if( NULL == pSurfaceOutputFile )
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Can't open to read the packer temp file.\r\n");
		fclose( pCacheFile );
		return false;
	}
#else
#ifdef ENABLE_PACK_SAVE
	pSurfaceOutputFile = fopen( "PackTemp.tmp", "wb");
	if( NULL == pSurfaceOutputFile )
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Can't open to write the packer temp file.\r\n");
		fclose( pCacheFile );
		return false;
	}
#else
	pSurfaceOutputFile = NULL;
#endif
#endif

	//get the real numBrushes
	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	{
		if (m_BrushRenderable[nBrushID]==0)
		{
			++nBrushID;
			continue;
		}
		++nBrushID;
		numBrushes++;
	}

	nBrushID = 0;
	//////////////////////////////////////////////////////////////////////////
	// Process all brushes
	//////////////////////////////////////////////////////////////////////////
	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	{
		if (m_BrushRenderable[nBrushID]==0)
		{
			++nBrushID;
			continue;
		}
		++nBrushID;
		//Wait Progress
		SetProgress((f32)nCurBrush/(f32)numBrushes);
		nCurBrush++;

		//std::vector<t_pairInt> arrOffsetXY;
		IRenderNode* pRenderNode = itEty->first;
		assert (pRenderNode!=NULL);
		IRenderMesh *pRenderMesh = pRenderNode->GetRenderMesh(0);

		//Voxels vertex container used as rendermesh
		CBaseObject *pObject = itEty->second;
		if( pObject && OBJTYPE_VOXEL == pObject->GetType() && pRenderMesh )
			pRenderMesh = pRenderMesh->GetVertexContainer();

//		assert(pRenderMesh != NULL);
		//hack for multiobjects meshes
//		static std::map<std::pair<int,int>,CBaseObject* > DebugMap;
		if (pRenderMesh!=NULL)
		{
//			int32 ObjGuidHash	=	pObject->GetId().Data1;
//			assert(pRenderNode->GetEditorObjectId()==ObjGuidHash);
//			std::map<std::pair<int,int>, CBaseObject*>::iterator it	=	DebugMap.find(std::pair<int32,int32>(pRenderNode->GetEditorObjectId(),ObjGuidHash));
//			CBaseObject* V=it->second;
//			assert(it==DebugMap.end());
//			DebugMap[std::pair<int32,int32>(pRenderNode->GetEditorObjectId(),ObjGuidHash)]	=	itEty->second;

			if(!SubSurfacePacker(pSurfaceOutputFile,pCacheFile,pRenderNode,0,pRenderMesh,pVisArea))
				return false;
		}
		else
		{
			IStatObj* pStatObj	=	pRenderNode->GetEntityStatObj(0, 0, 0);
			if(pStatObj)
				for(uint a=0;a<pStatObj->GetSubObjectCount();a++)
				{
//					std::map<std::pair<int,int>, CBaseObject*>::iterator it	=	DebugMap.find(std::pair<int32,int32>(pRenderNode->GetEditorObjectId(),a));
//					CBaseObject* V=it->second;
//					assert(it==DebugMap.end());
//					DebugMap[std::pair<int32,int32>(pRenderNode->GetEditorObjectId(),a)]	=	itEty->second;

					IStatObj::SSubObject* pSubObject	=	pStatObj->GetSubObject(a);
					if(pSubObject->nType==STATIC_SUB_OBJECT_MESH && pSubObject->pStatObj)
						if(pSubObject->pStatObj->GetRenderMesh())
							if(!SubSurfacePacker(pSurfaceOutputFile,pCacheFile,pRenderNode,a,pSubObject->pStatObj->GetRenderMesh(),pVisArea))
								return false;
				}
		}
	}

	if( pSurfaceOutputFile )
		fclose( pSurfaceOutputFile );

	if( pCacheFile )
		fclose( pCacheFile );
	return true;
}


//===================================================================================
// Method				:	ReleaseAllMeshesDirectly
// Description	:	Release all memory used by the meshes
//===================================================================================
void	CLightScene::ReleaseAllMeshesDirectly()
{
	t_mmapRLMeshes::iterator RLmeshEndIt = m_mmapRLMeshes.end();
	for(t_mmapRLMeshes::iterator RLmeshIt = m_mmapRLMeshes.begin(); RLmeshIt!=RLmeshEndIt; RLmeshIt++)
	{
		SAFE_DELETE (RLmeshIt->second);
	}
	m_mmapRLMeshes.clear();

	//we can easily release all geomesh....
	t_mmapGeomMeshes::iterator GmeshEndIt = m_mmapGeomMeshes.end();
	for(t_mmapGeomMeshes::iterator GmeshIt = m_mmapGeomMeshes.begin(); GmeshIt!=GmeshEndIt; GmeshIt++)
	{
		SAFE_DELETE (GmeshIt->second);
	}
	m_mmapGeomMeshes.clear();
}

//===================================================================================
// Method				:	ReleaseMeshes
// Description	:	Release all memory used by the meshes
//===================================================================================
void	CLightScene::ReleaseMeshes()
{
	//the meshes are attached to surfaces, so only the surface know when a mesh will be released
	m_mmapRLMeshes.clear();

	//we can easily release all geomesh....
	t_mmapGeomMeshes::iterator GmeshEndIt = m_mmapGeomMeshes.end();
	for(t_mmapGeomMeshes::iterator GmeshIt = m_mmapGeomMeshes.begin(); GmeshIt!=GmeshEndIt; GmeshIt++)
	{
		SAFE_DELETE (GmeshIt->second);
	}
	m_mmapGeomMeshes.clear();
}

//===================================================================================
// Method				:	NodeVisAreaSortFirstPass
// Description	:	Compare function for sorting vis areas
//===================================================================================
inline bool NodeVisAreaSortFirstPass( std::pair<IRenderNode*, CBaseObject*> l ,std::pair<IRenderNode*, CBaseObject*> r )
{
	if( l.first->GetEntityVisArea() == r.first->GetEntityVisArea() )
	{
		Matrix34 mNodeMatrix;

		//check the distance ..
		l.first->GetEntityStatObj(0, 0, &mNodeMatrix);
		f32 fLDistance = (mNodeMatrix.GetTranslation()).GetLengthSquared();
		r.first->GetEntityStatObj(0, 0, &mNodeMatrix);
		f32 fRDistance = (mNodeMatrix.GetTranslation()).GetLengthSquared();
		return (fLDistance < fRDistance);
	}
/*
	//if inside... is connected with the outdoor...
	bool b1 = ((IVisArea*)l.first->m_pVisArea)->IsConnectedToOutdoor();
	bool b2 = ((IVisArea*)r.first->m_pVisArea)->IsConnectedToOutdoor();

	if( b1 != b2 )
				return b1;
*/
	//leave untouched the other ones
	return ( l.first->GetEntityVisArea()  < r.first->GetEntityVisArea() );
}

	//check the distance ..
//	int iLDistance = g_pActualVisArea->GetDistance( l.first->m_pVisArea );
//	int iRDistance = g_pActualVisArea->GetDistance( r.first->m_pVisArea );
//	return iLDistance > iRDistance ? true :  ( iLDistance == iRDistance ?  true : false );
/*
	//check the distance ..
	l.first->GetEntityStatObj(0, &mNodeMatrix);
	f32 fLDistance = (mNodeMatrix.GetTranslation()).GetSquaredDistance();
	r.first->GetEntityStatObj(0, &mNodeMatrix);
	f32 fRDistance = (mNodeMatrix.GetTranslation()).GetSquaredDistance();
	return fLDistance > fRDistance ? true :  ( fLDistance < fRDistance ?  false : true );
*/



//===================================================================================
// Method				:	SortVisAreas
// Description	:	Try to minimalize the distances between the vis areas
//===================================================================================
void CLightScene::SortVisAreas( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes )
{
	//first pass: differnce the outdoor and the indoor areas & the indoor areas with connection to outdoor
	//third pass: we try to get the distances of the vis areas
	//second pass: we don't know anything about outdoor nodes - try to locate based on object center
	std::sort( vNodes.begin(), vNodes.end(), NodeVisAreaSortFirstPass );
}


void CLightScene::GenerateOcclusionMasks( IRenderNode* pRenderNode, bool bForceAllChannel )
{
	sOcclusionChannelMask OCM;
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	//INIT
	OCM.m_ActiveChannelNumber = 0;
	OCM.m_FirstChannel = 0;
	for( int i = 0; i < NUM_COMPONENTS*4; ++i )
		OCM.m_ChannelMap[i] = (uint8)0xff;

	if( sceneCtx.m_bMakeOcclMap  )
	{
		/*
		t_mmapRLMeshRange mmapMeshRange = m_mmapRLMeshes.equal_range(pRenderMesh);

		int32 nOcclusionMask = 0;
		t_mmapRLMeshes::const_iterator meshIt;
		for(meshIt = mmapMeshRange.first; meshIt!=mmapMeshRange.second; meshIt++ )
		{
			CRLMesh* rlMesh = meshIt->second;

			int16 numGroups = rlMesh->GetUnwrapGroupsNum();
			for (int16 i=0; i<numGroups; i++ )
			{
				CRLUnwrapGroup* pGroup = &(rlMesh->GetUnwrapGroups()[i]);
				assert( NULL != pGroup );
				nOcclusionMask |= pGroup->CalculateTheActiveOcclusionChannelMask();
			}
		}

		for( int i = 0; i < sceneCtx.m_numComponents; ++i )
		{
			//if have any information in the mask
			if( nOcclusionMask & (1<<i) )
				OCM.m_ChannelMap[OCM.m_ActiveChannelNumber++] = i;
		}

		//FIX: need real a solution!
		if( 0 == OCM.m_ActiveChannelNumber )
		{
				OCM.m_ChannelMap[OCM.m_ActiveChannelNumber++] = 0;
		}

#ifdef DEBUG
		t_mapRenderNodeLightIDsIterator LightIt = m_OcclusionLightPairs.find(pRenderNode);
		assert( LightIt == m_OcclusionLightPairs.end() || OCM.m_ActiveChannelNumber == (LightIt->second).size() );
#endif
		*/
		assert(0);	//not supported anymore since the multiobject-brush-support is/was added

	}


	//rae: need 1
	if( sceneCtx.m_bMakeRAE && OCM.m_ActiveChannelNumber < sceneCtx.m_numRAEComponents )
	{
		OCM.m_ActiveChannelNumber = sceneCtx.m_numRAEComponents;
		for( int i = 0; i < sceneCtx.m_numRAEComponents; ++i )
			OCM.m_ChannelMap[i] = i;
	}
	 
	//lightmap: need numcomp*4
	if( sceneCtx.m_bMakeLightMap || bForceAllChannel )
	{
		OCM.m_ActiveChannelNumber = sceneCtx.m_numComponents;
		for( int i = 0; i < sceneCtx.m_numComponents; ++i )
			OCM.m_ChannelMap[i] = i;
	}

	m_OcclusionChannelMaskMap.insert( t_pairRenderNodeOcclusionChannelMask(pRenderNode, OCM) );
}


bool CLightScene::DoRenderingSequential( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, IVisArea *pVisArea, const bool bShowTexelDensity, const int32 nVisAreaID )
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	int32 nCacheFilePosition = 0;
	FILE *pCacheFile = NULL;

	_TRACE(m_LogInfo, true, "Parametrizer\r\n");
	CLogFile::FormatLine("Parametrizer\n" );
	sceneCtx.m_numComponents = 
	sceneCtx.m_nSlidingChannelNumber = 1;

	if( false == Parametrizer( vNodes, pVisArea ) )
		return false;

	_TRACE(m_LogInfo, true, "Rasterization\r\n");
	CLogFile::FormatLine("Raster\n" );
	sceneCtx.m_numComponents = ( sceneCtx.m_bMakeRAE && !sceneCtx.m_bMakeOcclMap && !sceneCtx.m_bMakeLightMap ) ? 1 : 16;	//RAE needed only 1 components others can use more - better texture usage
	sceneCtx.m_nSlidingChannelNumber = 3 > sceneCtx.m_numComponents ? sceneCtx.m_numComponents : 3;

//	CRLMesh::m_bSamplesCache = false;

	//initialize per mesh parameters
	CAreaLight::Init();
	if( pSolarIllumStatement ) pSolarIllumStatement->Init();
	if( pDirectIllumStatement ) pDirectIllumStatement->Init();
	if( pIndirectIllumStatement ) pIndirectIllumStatement->Init();
	if( pOcclusionIllumStatement ) pOcclusionIllumStatement->Init();
	if( pAmbientOcclusionIllumStatement ) pAmbientOcclusionIllumStatement->Init();

	//lightmap rasterization
	int32 numRLMeshes = m_mmapRLMeshes.size();
	int32 nCurMesh = 0;
	for(t_mmapRLMeshes::const_iterator meshIt = m_mmapRLMeshes.begin(); meshIt!=m_mmapRLMeshes.end(); meshIt++)
	{
		//Wait Progress
		ProgressSetBounds(100*((f32)nCurMesh/(f32)numRLMeshes), 100*((f32)(nCurMesh+1)/(f32)numRLMeshes));
		nCurMesh++;

//		IRenderNode *pRenderNode = meshIt->first;
//		CLogFile::FormatLine("RENDER   name: %s\n", pRenderNode->GetName() );


		CRLMesh* rlMesh = meshIt->second;

		//t_mapRenderNodeLightIDsIterator OcclusionLightIt = m_OcclusionLightPairs.find(meshIt->first);
		//t_mapRenderNodeLightIDsIterator LightmapLightIt = m_LightmapLightPairs.find(meshIt->first);

		//error visualization
		int nOcclLightNum=0;
/*		if( OcclusionLightIt == m_OcclusionLightPairs.end() )
			nOcclLightNum = 0;
		else
			nOcclLightNum = ((t_pairEntityId)OcclusionLightIt->second).size();

		if( nOcclLightNum > 16 || bShowTexelDensity )
		{
			//resize...
			if( nOcclLightNum > 16 ) 
				((t_pairEntityId)OcclusionLightIt->second).resize( 16 );

			if( false == rlMesh->RasterizeDebugColor( this, pCacheFile, nCacheFilePosition, false, OcclusionLightIt->second, nOcclLightNum ) )
			{
				SAFE_DELETE( m_pISerializationManager );
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Lightmap compiler: rasterization error.\r\n");
				return false;
			}
		}
		else*/
		{
			t_pairEntityId *OcclusionLightIDs;
			//if( OcclusionLightIt != m_OcclusionLightPairs.end() )
//				OcclusionLightIDs = &(OcclusionLightIt->second);
//			else
				OcclusionLightIDs = NULL;

			t_pairEntityId *LightmapLightIDs;
//			if( LightmapLightIt )
//				LightmapLightIDs = *(LightmapLightIt->second);
//			else
				LightmapLightIDs = NULL;

			if( false == rlMesh->RasterizeMesh(pDirectIllumStatement, pIndirectIllumStatement, pSolarIllumStatement, pOcclusionIllumStatement, pAmbientOcclusionIllumStatement, this, pCacheFile, nCacheFilePosition, LightmapLightIDs, OcclusionLightIDs, nOcclLightNum , nVisAreaID) )
			{
				SAFE_DELETE( m_pISerializationManager );
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Lightmap compiler: rasterization error.\r\n");
				return false;
			}
		}
	}

	if( pCacheFile )
		fclose( pCacheFile );

	CLogFile::FormatLine("SurfPacking\n" );
	if( false == SurfacePacker( vNodes, pVisArea ) )
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Lightmap compiler: texture packer error.\r\n");
		return false;
	}

	return true;
}



bool CLightScene::DoRenderingParalell( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, IVisArea *pVisArea, const bool bShowTexelDensity, const int32 nVisAreaID )
{
/*	int32 nCacheFilePosition = 0;
	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator itEty;

	_TRACE(m_LogInfo, true, "Rasterization & packing\r\n");
	CLogFile::FormatLine("Raster\n" );

//	CRLMesh::m_bSamplesCache = false;

	//lightmap packing
	ProgressSetBounds(0,100);

	//"Compatibility"
	FILE *pCacheFile = NULL;
	FILE *pSurfaceOutputFile = NULL;
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	//if empty, add one,free surface
	if( m_rlSurfaces.empty() )
		m_rlSurfaces.push_back(new CRLSurface(sceneCtx.m_nLightmapPageWidth,sceneCtx.m_nLightmapPageHeight));

	//get the real numBrushes
	int32 numBrushes = vNodes.size();
	int32 nCurBrush = 0;
	int32 nBrushID = 0;
	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	{
		if (m_BrushRenderable[nBrushID]==0)
		{
			++nBrushID;
			continue;
		}
		++nBrushID;
		numBrushes++;
	}

	nBrushID = 0;
	//////////////////////////////////////////////////////////////////////////
	// Process all brushes
	//////////////////////////////////////////////////////////////////////////
	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	{
		if (m_BrushRenderable[nBrushID]==0)
		{
			++nBrushID;
			continue;
		}
		++nBrushID;
		//Wait Progress
		SetProgress((f32)nCurBrush/(f32)numBrushes);
		nCurBrush++;

		//std::vector<t_pairInt> arrOffsetXY;
		IRenderNode* pRenderNode = itEty->first;
		assert (pRenderNode!=NULL);
		IRenderMesh *pRenderMesh = pRenderNode->GetRenderMesh(0);

		//Voxels vertex container used as rendermesh
		CBaseObject *pObject = itEty->second;
		if( pObject && OBJTYPE_VOXEL == pObject->GetType() && pRenderMesh )
			pRenderMesh = pRenderMesh->GetVertexContainer();

		assert(pRenderMesh != NULL);
		//hack for multiobjects meshes
		if (pRenderMesh==NULL)
			continue;

		CLogFile::FormatLine("RP Brush name: %s\n", pRenderNode->GetName() );
		t_mmapRLMeshRange rlMeshRange = m_mmapRLMeshes.equal_range(pRenderMesh);

		//lightmap rasterization
		int32 numRLMeshes = 0;
		for(t_mmapRLMeshes::const_iterator meshIt = rlMeshRange.first; meshIt!=rlMeshRange.second; meshIt++)
			++numRLMeshes;

		int32 nCurMesh = 0;
		for(t_mmapRLMeshes::const_iterator meshIt = rlMeshRange.first; meshIt!=rlMeshRange.second; meshIt++)
		{
			CRLMesh* rlMesh = meshIt->second;

			while( false == rlMesh->CheckValidSize() )
				rlMesh->SetLightmapQuality( rlMesh->GetLightmapQuality() * 0.5f );

			assert(0);	//not supported anymore since the multiobject-brush-support is/was added
*			if( bShowTexelDensity )
			{
				CLogFile::FormatLine("	%d\n", nCurMesh );
				t_mapRenderNodeLightIDsIterator OcclusionLightIt = m_OcclusionLightPairs.find(meshIt->first);

				int nOcclLightNum;
				if( OcclusionLightIt == m_OcclusionLightPairs.end() )
					nOcclLightNum = 0;
				else
					nOcclLightNum = ((t_pairEntityId)OcclusionLightIt->second).size();

				if( false == rlMesh->RasterizeDebugColor( this, pCacheFile, nCacheFilePosition, true, OcclusionLightIt->second, nOcclLightNum ) )
				{
					SAFE_DELETE( m_pISerializationManager );
					CLogFile::FormatLine("RasterFailed\n" );
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Lightmap compiler: rasterization error.\r\n");
					return false;
				}
			}
			else
			{
#ifdef  RASTERIZE_DEBUGCOLORS
				t_mapRenderNodeLightIDsIterator OcclusionLightIt = m_OcclusionLightPairs.find(meshIt->first);
				int nOcclLightNum;
				if( OcclusionLightIt == m_OcclusionLightPairs.end() )
					nOcclLightNum = 0;
				else
					nOcclLightNum = ((t_pairEntityId)OcclusionLightIt->second).size();

				if( false == rlMesh->RasterizeDebugColor( this, pCacheFile, nCacheFilePosition,true, OcclusionLightIt->second ) )
				{
					SAFE_DELETE( m_pISerializationManager );
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Lightmap compiler: rasterization error.\r\n");
					return false;
				}
#else
				t_mapRenderNodeLightIDsIterator OcclusionLightIt = m_OcclusionLightPairs.find(meshIt->first);
				t_mapRenderNodeLightIDsIterator LightmapLightIt;// = m_LightmapLightPairs.find(meshIt->first);
				int nOcclLightNum;
				if( OcclusionLightIt == m_OcclusionLightPairs.end() )
					nOcclLightNum = 0;
				else
					nOcclLightNum = ((t_pairEntityId)OcclusionLightIt->second).size();

				//error visualization
				if( ((t_pairEntityId)OcclusionLightIt->second).size() > 16 )
				{
					//resize...
					((t_pairEntityId)OcclusionLightIt->second).resize( 16 );
					if( false == rlMesh->RasterizeDebugColor( this, pCacheFile, nCacheFilePosition, false, OcclusionLightIt->second, nOcclLightNum ) )
					{
						SAFE_DELETE( m_pISerializationManager );
						CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Lightmap compiler: rasterization error.\r\n");
						return false;
					}
				}
				else
				{
					t_pairEntityId *OcclusionLightIDs;
					if( OcclusionLightIt != m_OcclusionLightPairs.end() )
						OcclusionLightIDs = &(OcclusionLightIt->second);
					else
						OcclusionLightIDs = NULL;

					t_pairEntityId *LightmapLightIDs;
		//			if( LightmapLightIt )
		//				LightmapLightIDs = *(LightmapLightIt->second);
		//			else
						LightmapLightIDs = NULL;

					if( false == rlMesh->RasterizeMesh(pDirectIllumStatement, pIndirectIllumStatement, pSolarIllumStatement, pOcclusionIllumStatement, pAmbientOcclusionIllumStatement, this, pCacheFile, nCacheFilePosition, LightmapLightIDs, OcclusionLightIDs, nOcclLightNum, nVisAreaID ) )
					{
						SAFE_DELETE( m_pISerializationManager );
						CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"Lightmap compiler: rasterization error.\r\n");
						return false;
					}
				}
#endif
			}*
		}



		m_mapValidTexCoords.clear(); //begin valid texture coords accamulating

		GenerateOcclusionMasks( pRenderNode );

		t_mapRenderNodeOcclusionChannelMaskIterator OcclusionMaskIt = m_OcclusionChannelMaskMap.find(pRenderNode);
		sOcclusionChannelMask *OCM = &(OcclusionMaskIt->second);


		CRLSurface* pRLSurface = NULL; 
#ifdef SKIP_PACKING
		if (LoadSurfaceSpace(rlMeshRange, &pRLSurface,pSurfaceOutputFile))
#else
		if (AllocateSurfaceSpace(rlMeshRange, &pRLSurface,pCacheFile,pSurfaceOutputFile, false, OCM, &m_rlSurfaces))
#endif
		{
			assert(pRLSurface!=NULL);
			RescaleTexCoords( pRLSurface );

			pRLSurface->AddObjectId(std::pair<int32,int32>(pRenderNode->GetEditorObjectId(),SubObjIdx));
			pRLSurface->AddRenderNodePtr(pRenderNode);

			pRLSurface->ResetMeshBuffer();
		}
		else //unsuccessful allocation //try to create new Lightmap Surface
		{
			m_rlSurfaces.push_back(new CRLSurface(sceneCtx.m_nLightmapPageWidth,sceneCtx.m_nLightmapPageHeight));

			//try to amortize the used memory of surfaces -> at maximum 4 surface "open" at the same time
			if( m_rlSurfaces.size() > LC_MAX_OPEN_SURFACE )
			{
				int32 nSurfaceID = m_rlSurfaces.size() - (LC_MAX_OPEN_SURFACE+1);
				CRLSurface **pClosableSurface = &(m_rlSurfaces[ nSurfaceID ] );
				if( NULL != (*pClosableSurface) )
				{
					CRLSurface *pS = *pClosableSurface;
					if( false == pS->IsFinished() )
					{
//debug only?						pS->GenerateDebugJPG();	
						GenerateSurface( pS, pCacheFile );
						UpdateRLSurface( pS );
						pS->ReleaseAttachedMeshes();
					}
					SAFE_DELETE( (*pClosableSurface) );
				}
			}
#ifdef SKIP_PACKING
			if (LoadSurfaceSpace(rlMeshRange, &pRLSurface,pSurfaceOutputFile))
#else
			if (AllocateSurfaceSpace(rlMeshRange, &pRLSurface,pCacheFile,pSurfaceOutputFile, false, OCM, &m_rlSurfaces))
#endif
			{
				assert(pRLSurface!=NULL);
				RescaleTexCoords( pRLSurface );

				pRLSurface->AddObjectId(std::pair<int32,int32>(pRenderNode->GetEditorObjectId(),SubObjIdx)); 
				pRLSurface->AddRenderNodePtr(pRenderNode);

				pRLSurface->ResetMeshBuffer();
			}
			else
			{
				//DEBUG messages for easy reproduction....
				if( pVisArea )
					CLogFile::FormatLine("LIGHTMAP COMPILER DEBUG:\n VisArea name: %s\n", pVisArea->GetName() );
				else
					CLogFile::FormatLine("LIGHTMAP COMPILER DEBUG:\n VisArea name: NONAME\n" );

				// This function will report datas into log file, to help determine the bug.
				AllocateSurfaceSpace(rlMeshRange, &pRLSurface, pCacheFile,NULL,true, OCM, &m_rlSurfaces);

				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,"GLM can not fit into lightmap size %ix%i\r\n",
					sceneCtx.m_nLightmapPageWidth,
					sceneCtx.m_nLightmapPageHeight );

				SAFE_DELETE( m_pISerializationManager );
				return false;
			}//2nd allocation

		} //1th allocation

		//lm tex-coords buffer should match to the pRenderMesh->GetSysVertCount()
		std::vector<TexCoord2Comp> arrMeshLMCoords;
		arrMeshLMCoords.assign(pRenderMesh->GetSysVertCount(), TexCoord2Comp(0.0f,0.0f));

		t_mapIndTexCoord::const_iterator itTexCoord = m_mapValidTexCoords.begin();
		for(;itTexCoord!=m_mapValidTexCoords.end();itTexCoord++)
		{
			assert(itTexCoord->first < pRenderMesh->GetSysVertCount());

			if( itTexCoord->first < pRenderMesh->GetSysVertCount() )
				arrMeshLMCoords[itTexCoord->first] = itTexCoord->second;
		}

		assert (arrMeshLMCoords.size()<=65535);

		//ID for Ambient occlusion
		t_mapRenderNodeLightIDsIterator LightIt = m_OcclusionLightPairs.find(pRenderNode);
		if( LightIt == m_OcclusionLightPairs.end() )
		{
			if( arrMeshLMCoords.size() )
				m_pISerializationManager->AddTexCoordData(&arrMeshLMCoords.front(), arrMeshLMCoords.size(), pRenderNode->GetEditorObjectId(),0,((DWORD)(pRenderNode)*pRenderNode->GetEditorObjectId()), NULL,NULL,0, 0);
			else
				m_pISerializationManager->AddTexCoordData(NULL,0, pRenderNode->GetEditorObjectId(),0,((DWORD)(pRenderNode)*pRenderNode->GetEditorObjectId()), NULL,NULL,0, 0);
		}
		else
		{
			assert( OCM->m_ActiveChannelNumber == (LightIt->second).size() );
			EntityId Occls[32];
			int32 nOcclNum = LightIt->second.size();
			nOcclNum = nOcclNum > 16 ? 16 : nOcclNum;
			for( int32 i = 0; i < nOcclNum; ++i )
			{
				Occls[i] = LightIt->second[i].first;
				Occls[i+16] = LightIt->second[i].second;
			}
			if( arrMeshLMCoords.size() )
				m_pISerializationManager->AddTexCoordData(&arrMeshLMCoords.front(), arrMeshLMCoords.size(), pRenderNode->GetEditorObjectId(),0,((DWORD)(pRenderNode)*pRenderNode->GetEditorObjectId()), Occls, &Occls[16], nOcclNum, OCM->m_FirstChannel);
			else
				m_pISerializationManager->AddTexCoordData(NULL,0, pRenderNode->GetEditorObjectId(),0,((DWORD)(pRenderNode)*pRenderNode->GetEditorObjectId()), Occls, &Occls[16], nOcclNum, OCM->m_FirstChannel);
		}
	}
	return true;*/
	assert(0);		//not supported currently, since changes for multi subojbject brushes
	return false;
}


//===================================================================================
// Method				:	Rought_AttachOcclusionLightToList
// Description	:	Add a light into occlusion light list, if it lighting the mesh
//===================================================================================
void CLightScene::Rought_AttachOcclusionLightToList( t_pairEntityId &LightIDs, IRenderNode *pRenderNode ) const
{
	//search light which can affect the mesh.. (I use the unwrapgroups bounding boxes, not one big one,
	//I hope this will be more precise.)

	IVisArea *pVisArea = (IVisArea*)pRenderNode->GetEntityVisArea();

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	const PodArray<CDLight*>*	pListLights	=	sceneCtx.GetLights();
	assert(pListLights!=NULL);

	//test every light.
	int32 nLightNumber			= pListLights->Count();
	Vec3	*pOrigin;
	f32		fMaxLightDistance;
	f32		fSqrMaxDistance;

	for( int32 i=0; i< nLightNumber; ++i )
	{
		CDLight* pLight	=	pListLights->GetAt(i);
		if( NULL == pLight )
		{
			//accident?
			assert(0 && "there must be a light!");
			continue;
		}

		// test the light same as LightMan.cpp do...
		if( pLight->m_Flags & DLF_THIS_AREA_ONLY )
		{
			if( NULL != pVisArea && pLight->m_pOwner!=(IRenderNode*)-1 )
			{
				if(	pLight->m_pOwner  )
				{
					if( pLight->m_pOwner->GetEntityVisArea() )
					{
						IVisArea * pLightArea = (IVisArea *)pLight->m_pOwner->GetEntityVisArea();

						if( pVisArea != pLightArea )
						{	
							int nMaxRecursion = 2;
							if( false == pVisArea->FindVisArea(pLightArea, nMaxRecursion, true) )		// try also portal volumes
								continue; // areas do not much
						}
					}
					else
						if( NULL != pVisArea ) //&& false == pVisArea->IsAffectedByOutLights() )
							continue;
				}
				else
					if( NULL != pVisArea ) //&& false == pVisArea->IsAffectedByOutLights() )
						continue;
			}
			else // entity is outside
			{
				if( NULL == pVisArea )
				{
					if( pLight->m_pOwner!=(IRenderNode*)-1  &&  NULL != pLight->m_pOwner->GetEntityVisArea() )
						continue;
				}
			}
		}

		if (false == sceneCtx.m_bUseSunLight && (pLight->m_Flags & DLF_SUN ) )
			continue;

		if( !(pLight->m_Flags & DLF_DIFFUSEOCCLUSION) && !(pLight->m_Flags & DLF_SPECULAROCCLUSION) )
			continue;


		int32 nGlobalLightNumber = LightIDs.size();

		if( 16 == nGlobalLightNumber )
			return;

		//search, if found, don't add it
		int32 j;
		for(j = 0; j < nGlobalLightNumber; ++j )
			if( LightIDs[j].second == i )
				break;

		if( j != nGlobalLightNumber )
			continue;


		fMaxLightDistance = pLight->m_fRadius;
		pOrigin = &(pLight->m_BaseOrigin);

		Vec3 vCenter = pRenderNode->GetBBox().GetCenter();
		float fRadius = pRenderNode->GetBBox().GetRadius();
		fSqrMaxDistance = fMaxLightDistance + fRadius;
		fSqrMaxDistance *= fSqrMaxDistance;

		if( pOrigin->GetSquaredDistance(vCenter)  <= fSqrMaxDistance )
		{
			LightIDs.push_back( std::pair<EntityId, EntityId>( pLight->m_nEntityId, i ) );
		}
	}
}

bool	CLightScene::GenerateSpartialListForOccluders( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes )
{
	std::vector<std::pair<IRenderNode*, CBaseObject*> >::const_iterator itEty;
	IRenderNode *pRenderNode;

	//even 16 can speed up a lot..
	m_nOccluderGridWith =
	m_nOccluderGridHeight = 64;
	m_nOccluderGridDeepness = 32;
	int32 nBufferSize = m_nOccluderGridWith * m_nOccluderGridHeight * m_nOccluderGridDeepness;
	m_pOccluderGrid = new uint32*[ nBufferSize ];
	m_pOccluderGridCellSize = new uint32[ nBufferSize ];
	uint32* pOccluderGridCellFreeSize = new uint32[ nBufferSize ];
	if( NULL == m_pOccluderGrid || NULL == m_pOccluderGridCellSize || NULL == pOccluderGridCellFreeSize )
		return false;
	memset( m_pOccluderGrid, 0, sizeof(uint32*)*nBufferSize );
	memset( m_pOccluderGridCellSize, 0, sizeof(uint32)*nBufferSize );
	memset( pOccluderGridCellFreeSize, 0, sizeof(uint32)*nBufferSize );

	//setup the AABoxOfScene
	m_AABoxOfScene.Reset();
	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	{
		pRenderNode = itEty->first;
		if( NULL == pRenderNode )
			continue;
			m_AABoxOfScene.Add( pRenderNode->GetBBox() );
	}

	//Calculate a cell size
	m_vOccluderCellSize = m_AABoxOfScene.GetSize();

	if( m_vOccluderCellSize.x < 10e-6f )
		m_vOccluderCellSize.x = 1;
	else
		m_vOccluderCellSize.x = (m_nOccluderGridWith-1) / m_vOccluderCellSize.x;

	if( m_vOccluderCellSize.y < 10e-6f )
		m_vOccluderCellSize.y = 1;
	else
		m_vOccluderCellSize.y = (m_nOccluderGridHeight-1) / m_vOccluderCellSize.y;

	if( m_vOccluderCellSize.z < 10e-6f )
		m_vOccluderCellSize.z = 1;
	else
		m_vOccluderCellSize.z = (m_nOccluderGridDeepness-1) / m_vOccluderCellSize.z;

	AABB BBox;
	int32 nMinX,nMinY,nMinZ;
	int32 nMaxX,nMaxY,nMaxZ;
	int32 nX,nY,nZ;
	uint32* pCell;

	//setup the grid
	uint32 nBrushID = 0;
	for (itEty=vNodes.begin(); itEty!=vNodes.end(); itEty++)
	{
		++nBrushID;
		pRenderNode = itEty->first;
		if( NULL == pRenderNode )
			continue;

		BBox = pRenderNode->GetBBox();

		BBox.min -= m_AABoxOfScene.min;
		BBox.max -= m_AABoxOfScene.min;
		//convert it to int32
		nMinX = (int32)floor( BBox.min.x * m_vOccluderCellSize.x );
		nMinY = (int32)floor( BBox.min.y * m_vOccluderCellSize.y );
		nMinZ = (int32)floor( BBox.min.z * m_vOccluderCellSize.z );
		nMaxX = (int32)ceil( BBox.max.x * m_vOccluderCellSize.x )+1;
		nMaxY = (int32)ceil( BBox.max.y * m_vOccluderCellSize.y )+1;
		nMaxZ = (int32)ceil( BBox.max.z * m_vOccluderCellSize.z )+1;

		//clamp to the size of BBox of Scene
		nMinX = nMinX < 0 ? 0 : nMinX;
		nMinX = nMinX > m_nOccluderGridWith ? m_nOccluderGridWith : nMinX;
		nMinY = nMinY < 0 ? 0 : nMinY;
		nMinY = nMinY > m_nOccluderGridHeight ? m_nOccluderGridHeight : nMinY;
		nMinZ = nMinZ < 0 ? 0 : nMinZ;
		nMinZ = nMinZ > m_nOccluderGridDeepness ? m_nOccluderGridDeepness : nMinZ;
		nMaxX = nMaxX < 0 ? 0 : nMaxX;
		nMaxX = nMaxX > m_nOccluderGridWith ? m_nOccluderGridWith : nMaxX;
		nMaxY = nMaxY < 0 ? 0 : nMaxY;
		nMaxY = nMaxY > m_nOccluderGridHeight ? m_nOccluderGridHeight : nMaxY;
		nMaxZ = nMaxZ < 0 ? 0 : nMaxZ;
		nMaxZ = nMaxZ > m_nOccluderGridDeepness ? m_nOccluderGridDeepness : nMaxZ;

#define OCCLUDER_GRID_GROW_SIZE 16

		//include it to the lists
		for( nZ = nMinZ; nZ < nMaxZ; ++nZ )
			for( nY = nMinY; nY < nMaxY; ++nY )
				for( nX = nMinX; nX < nMaxX; ++nX )
				{
					int32 nID = (nZ*m_nOccluderGridHeight+nY)*m_nOccluderGridWith+nX;

					if( 0 == pOccluderGridCellFreeSize[ nID ] )
					{
						//time to grow...
						pCell = m_pOccluderGrid[ nID ];
						uint32* pNewCell = new uint32[ m_pOccluderGridCellSize[ nID ] + OCCLUDER_GRID_GROW_SIZE ];
						memset( pNewCell, 0, sizeof(uint32)*(m_pOccluderGridCellSize[ nID ] + OCCLUDER_GRID_GROW_SIZE));
						if( pCell )
						{
							memcpy( pNewCell, pCell, sizeof(uint32)* m_pOccluderGridCellSize[ nID ] );
							delete [] pCell;
						}
						m_pOccluderGrid[ nID ] = pNewCell;
						pOccluderGridCellFreeSize[ nID ] = OCCLUDER_GRID_GROW_SIZE;
					}

					pCell = m_pOccluderGrid[ nID ];
					pCell[ m_pOccluderGridCellSize[ nID ] ] = (nBrushID-1);
					++m_pOccluderGridCellSize[ nID ];
					--pOccluderGridCellFreeSize[ nID ];
				}
	}

	SAFE_DELETE_ARRAY( pOccluderGridCellFreeSize );

	return true;
}
