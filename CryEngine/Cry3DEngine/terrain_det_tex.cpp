////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_det_tex.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: terrain detail textures
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain.h"
#include "terrain_sector.h"
#include "3dEngine.h"

void CTerrain::SetDetailLayerProperties(int nId, float fScaleX, float fScaleY, 
																				IMaterial::EProjAxis projAxis, const char * szSurfName, 
																				const PodArray<int> & lstnVegetationGroups, IMaterial * pMat)
{
	if(nId>=0 && nId<MAX_SURFACE_TYPES_COUNT)
	{
		strncpy(m_SSurfaceType[nId].szName, szSurfName, sizeof(m_SSurfaceType[nId].szName));
		m_SSurfaceType[nId].fScale = fScaleX;
		m_SSurfaceType[nId].defProjAxis = projAxis;
		m_SSurfaceType[nId].ucThisSurfaceTypeId = nId;
		m_SSurfaceType[nId].lstnVegetationGroups.Reset();
		m_SSurfaceType[nId].lstnVegetationGroups.AddList(lstnVegetationGroups);
		m_SSurfaceType[nId].pLayerMat = pMat;
		if(m_SSurfaceType[nId].pLayerMat && !m_bEditorMode)
			CTerrain::Get3DEngine()->PrintMessage("  Layer %d - %s has material %s", nId, szSurfName, pMat->GetName());
	}
	else
	{
		Warning("CTerrain::SetDetailTextures: LayerId is out of range: %d: %s", nId, szSurfName);
		assert(!"CTerrain::SetDetailTextures: LayerId is out of range");
	}
}

void CTerrain::LoadSurfaceTypesFromXML(XmlNodeRef pDetTexTagList)
{
  LOADING_TIME_PROFILE_SECTION(GetSystem());

	CTerrain::Get3DEngine()->PrintMessage("Loading terrain layers ...");

 	if (!pDetTexTagList)
		return;

	for (int nId = 0; nId < pDetTexTagList->getChildCount(); nId++)
	{
		XmlNodeRef pDetLayer = pDetTexTagList->getChild(nId);
		IMaterialManager* pMatMan = Get3DEngine()->GetMaterialManager();
		const char * pMatName = pDetLayer->getAttr("DetailMaterial");
		IMaterial * pMat = pMatName[0] ? pMatMan->LoadMaterial(pMatName) : NULL;

		float fScaleX=1.f; pDetLayer->getAttr("DetailScaleX",fScaleX);
		float fScaleY=1.f; pDetLayer->getAttr("DetailScaleY",fScaleY);
		uchar ucProjAxis = pDetLayer->getAttr("ProjAxis")[0];

		IMaterial::EProjAxis projAxis = ucProjAxis == 'X' ? IMaterial::ePA_X : ucProjAxis == 'Y' ? IMaterial::ePA_Y : ucProjAxis == 'Z' ? IMaterial::ePA_Z : IMaterial::ePA_None;

		if(!pMat || pMat == pMatMan->GetDefaultMaterial())
		{
			Error("CTerrain::LoadSurfaceTypesFromXML: Error loading material: %s", pMatName);
			pMat = pMatMan->GetDefaultTerrainLayerMaterial();
		}

		if(pMat)
		{
			pMat->m_fDefautMappingScale = fScaleX;
			Get3DEngine()->InitMaterialDefautMappingAxis(pMat);
		}

		PodArray<int> lstnVegetationGroups;
		for (int nGroup = 0; nGroup < pDetLayer->getChildCount(); nGroup++)
		{
			XmlNodeRef pVegetationGroup = pDetLayer->getChild(nGroup);
			int nVegetationGroupId=-1;
			if(pVegetationGroup->isTag("VegetationGroup") && pVegetationGroup->getAttr("Id",nVegetationGroupId))
				lstnVegetationGroups.Add(nVegetationGroupId);
		}

		SetDetailLayerProperties(nId, fScaleX, fScaleY, projAxis, pDetLayer->getAttr("Name"), lstnVegetationGroups, pMat);
	}
}

void CTerrain::UpdateSurfaceTypes()
{
	if(m_pParentNode)
		m_pParentNode->UpdateDetailLayersInfo(true);
}
