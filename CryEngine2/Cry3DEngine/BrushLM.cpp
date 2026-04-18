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

#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "CullBuffer.h"
#include "3dEngine.h"
#include "IndexedMesh.h"
#include "WaterVolumes.h"
#include "LMCompStructures.h"
#include "Brush.h"
#include "IEntitySystem.h"

void CBrush::SetLightmap(RenderLMData *pLMData, float *pTexCoords, uint32 iNumTexCoords, const unsigned char cucOcclIDCount, /*const std::vector<std::pair<EntityId, EntityId> >& aIDs,*/ const int8 nFirstOcclusionChannel, const int32 SubObjIdx)
{
	//Occlusion loading.
	assert(cucOcclIDCount <= 16);
	/*	memset(m_arrOcclusionLightOwners,0,sizeof(m_arrOcclusionLightOwners));
	m_nFirstOcclusionChannel = nFirstOcclusionChannel;

	if(GetCVars()->e_light_maps_occlusion)
	{
	int32 nIndex = 0;
	SEntitySlotInfo slotInfo;
	for(int32 i=0; i<16 && i<cucOcclIDCount; ++i)
	{
	EntityId nEntId = aIDs[i].first;
	IEntity * pEnt;
	if(nEntId == (EntityId)-1)
	{
	m_arrOcclusionLightOwners[nIndex] = (IRenderNode*)-1; // sun
	++nIndex;
	}
	else
	pEnt = GetSystem()->GetIEntitySystem()->GetEntity(nEntId);
	if (pEnt)
	{
	//clear
	slotInfo.pLight = NULL;

	//slot 1 is for light, but try to search if not found
	if( false == pEnt->GetSlotInfo(1, slotInfo) || NULL == slotInfo.pLight )
	if( false == pEnt->GetSlotInfo(0, slotInfo) )
	{
	assert(0);
	}

	ILightSource *pLightSource = (ILightSource *)slotInfo.pLight;
	if( pLightSource )
	{
	m_arrOcclusionLightOwners[nIndex] = pLightSource->GetLightProperties().m_pOwner;
	++nIndex;
	}
	}
	}
	}
	*/
	SetLightmap(pLMData, pTexCoords, iNumTexCoords, 0, SubObjIdx );
}

void CBrush::SetLightmap(RenderLMData *pLMData, float *pTexCoords, uint32 iNumTexCoords, int nLod,const int32 SubObjIdx)
{
	// ---------------------------------------------------------------------------------------------
	// Set a reference of a DOT3 Lightmap object for this GLM
	// ---------------------------------------------------------------------------------------------

	IRenderer *pIRenderer = GetRenderer();

	IStatObj *pIStatObj = GetStatObj();

	if (pIStatObj == NULL)
		return;

	SLMData* pLocalLMData	=	&m_arrLMData[nLod];

	IRenderMesh *pRenderMesh = pIStatObj->GetRenderMesh();			

	if (pRenderMesh == NULL)
	{
		assert(SubObjIdx<pIStatObj->GetSubObjectCount());
		if(SubObjIdx>=pIStatObj->GetSubObjectCount())
			return;

		IStatObj::SSubObject* pSubObject	=	pIStatObj->GetSubObject(SubObjIdx);
		if(pSubObject->nType!=STATIC_SUB_OBJECT_MESH || !pSubObject->pStatObj)
			return;
		pRenderMesh	=	pSubObject->pStatObj->GetRenderMesh();
		//assert(!pRenderMesh);
		if(!pRenderMesh)
			return;
		if(pIStatObj->GetSubObjectCount()!=m_SubObjectLightmapData.size())
			m_SubObjectLightmapData.resize(pIStatObj->GetSubObjectCount());

		pLocalLMData	=	&m_SubObjectLightmapData[SubObjIdx];
	}

	pLocalLMData->m_pLMData = pLMData;

	if (pLocalLMData->m_pLMTCBuffer)
	{
		pIRenderer->DeleteRenderMesh(pLocalLMData->m_pLMTCBuffer);
		pLocalLMData->m_pLMTCBuffer = NULL;
	}

	if(!pLMData)
		return;

	// Renderer expect 2 floats
	std::vector<float> vTexCoord2;
	assert(iNumTexCoords);
	vTexCoord2.reserve(iNumTexCoords * 2);
	UINT i;
	for (i=0; i<iNumTexCoords; i++)
	{
		vTexCoord2.push_back(pTexCoords[i * 2 + 0]); // S
		vTexCoord2.push_back(pTexCoords[i * 2 + 1]); // T
	}

	if (pRenderMesh->GetVertCount() != iNumTexCoords)
	{
		/*
		char szBuffer[1024];
		sprintf(szBuffer, "Error: CBrush::SetLightmap: Object at position (%f, %f, %f) has" \
		" texture mismatch (%i coordinates supplied, %i required)",
		GetPos().x, GetPos().y, GetPos().z, 
		iNumTexCoords, pRenderMesh->GetSysVertCount());
		FileWarning(0,pIStatObj->GetFilePath(),szBuffer);

		*/
		// assert(pRenderMesh->GetSysVertCount() == iNumTexCoords);
		return;
	}

	// Make RenderMesh and fill it with texture coordinates
	pLocalLMData->m_pLMTCBuffer = GetRenderer()->CreateRenderMeshInitialized(
		&vTexCoord2[0], pRenderMesh->GetVertCount(), VERTEX_FORMAT_TEX2F, 
		0/*pRenderMesh->GetIndices(0)*/, 0/*pRenderMesh->GetIndicesCount()*/, 
		R_PRIMV_TRIANGLES, "LMapTexCoords",pIStatObj->GetFilePath(), eBT_Static, 1, 0, NULL, NULL, false, false);

//  C3DEngine *pEng = (C3DEngine *)Get3DEngine();
//	m_arrLMData[nLod].m_pLMTCBuffer->SetChunk(GetMatMan()->GetDefaultMaterial(),
	//	0,pRenderMesh->GetSysVertCount(), 0,pRenderMesh->GetIndicesCount());
}
