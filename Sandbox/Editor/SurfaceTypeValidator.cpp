////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   SurfaceTypeValidator.cpp
//  Version:     v1.00
//  Created:     15/8/2006 by MichaelS.
//  Compilers:   Visual Studio .NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SurfaceTypeValidator.h"
#include "IMaterial.h"
#include "IMaterialEffects.h"
#include "IGame.h"
#include "IGameFramework.h"
#include "Material/Material.h"

void CSurfaceTypeValidator::Validate()
{
	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
	ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
	std::set<string> reportedMaterialNames;

	CBaseObjectsArray objects;
	pObjectManager->GetObjects(objects);
	for (CBaseObjectsArray::iterator itObject = objects.begin(); itObject != objects.end(); ++itObject)
	{
		CBaseObject* pObject = *itObject;
		IPhysicalEntity* pPhysicalEntity = pObject ? pObject->GetCollisionEntity() : 0;

		// query part number with GetStatus(pe_status_nparts)
		pe_status_nparts numPartsQuery;
		int numParts = pPhysicalEntity ? pPhysicalEntity->GetStatus(&numPartsQuery) : 0;

		// iterate the parts, query GetParams(pe_params_part) for each part index (0..n-1)
		// (set ipart to the part index and MARK_UNUSED partid before each getstatus call)
		pe_params_part partQuery;
		for (partQuery.ipart = 0; partQuery.ipart < numParts; ++partQuery.ipart)
		{
			MARK_UNUSED partQuery.partid;
			int queryResult = pPhysicalEntity->GetParams(&partQuery);

			// if flags & (geom_colltype0|geom_colltype_player), check nMats entries in pMatMapping. they should contain valid game mat ids
			if (queryResult != 0 && partQuery.flagsAND & (geom_colltype0 | geom_colltype_player))
			{
				CMaterial* pEditorMaterial = pObject ? pObject->GetRenderMaterial() : 0;
				IMaterial* pMaterial = pEditorMaterial ? pEditorMaterial->GetMatInfo() : 0;

				unsigned subMtlFlags = 0;
				if (pMaterial && reportedMaterialNames.insert(pMaterial->GetName()).second)
					subMtlFlags = GetUsedSubMaterialFlags(&partQuery);

				int surfaceTypeIDs[MAX_SUB_MATERIALS];
				memset( surfaceTypeIDs,0,sizeof(surfaceTypeIDs) );
				int numSurfaces = pMaterial ? pMaterial->FillSurfaceTypeIds(surfaceTypeIDs) : 0;
				if ((1 << numSurfaces) <= subMtlFlags)
					subMtlFlags |= 1;
				for (int surfaceIDIndex = 0; surfaceIDIndex < numSurfaces; ++surfaceIDIndex)
				{
					string materialSpec;
					materialSpec.Format("%s:%d", pMaterial->GetName(), surfaceIDIndex + 1);
					if ((subMtlFlags & (1 << surfaceIDIndex)) && surfaceTypeIDs[surfaceIDIndex] <= 0 && reportedMaterialNames.insert(materialSpec).second)
					{
						CErrorRecord err;
						err.error.Format("Shootable object has material (%s) with invalid surface type.", materialSpec.c_str());
						err.pObject = pObject;
						err.severity = CErrorRecord::ESEVERITY_WARNING;
						GetIEditor()->GetErrorReport()->ReportError(err);
					}
				}
			}
		}

		pe_status_placeholder spc;
		pe_action_reset ar;
		if (pPhysicalEntity && pPhysicalEntity->GetStatus(&spc) && spc.pFullEntity)
			pPhysicalEntity->Action(&ar,1);
	}
}

unsigned CSurfaceTypeValidator::GetUsedSubMaterialFlags(pe_params_part* pPart)
{
	phys_geometry* pGeometriesToCheck[2];
	int numGeometriesToCheck = 0;
	if (pPart->pPhysGeom)
		pGeometriesToCheck[numGeometriesToCheck++] = pPart->pPhysGeom;
	if (pPart->pPhysGeomProxy && pPart->pPhysGeomProxy != pPart->pPhysGeom)
		pGeometriesToCheck[numGeometriesToCheck++] = pPart->pPhysGeomProxy;

	unsigned usedMtls = 0;
	for (int geometryToCheck = 0; geometryToCheck < numGeometriesToCheck; ++geometryToCheck)
	{
		phys_geometry* pGeometry = pGeometriesToCheck[geometryToCheck];

		IGeometry* pCollGeometry = pGeometry ? pGeometry->pGeom : 0;
		mesh_data* pMesh = (mesh_data*)(pCollGeometry && pCollGeometry->GetType() == GEOM_TRIMESH ? pCollGeometry->GetData() : 0);

		unsigned geometryUsedMtls = 0;
		if (pMesh && pMesh->pMats)
		{
			for (int i = 0; i < pMesh->nTris; ++i)
			{
				if (pMesh->pMats[i] >= 0 && pMesh->pMats[i] < MAX_SUB_MATERIALS)
					geometryUsedMtls |= (1 << pMesh->pMats[i]);
			}
		}
		else
		{
			if (pGeometry && pGeometry->surface_idx >= 0 && pGeometry->surface_idx < MAX_SUB_MATERIALS)
				geometryUsedMtls |= (1 << pGeometry->surface_idx);
		}

		usedMtls |= geometryUsedMtls;
	}

	return usedMtls;
}
