////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_load.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: terrain loading
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "terrain.h"
#include "ObjMan.h"
#include "3dEngine.h"
/*
void CTerrain::ProcessPanorama()
{
	FUNCTION_PROFILER_3DENGINE;

	if(GetCVars()->e_panorama_clusters_num > MAX_PANORAMA_CLUSTERS_NUM)
		GetCVars()->e_panorama_clusters_num = MAX_PANORAMA_CLUSTERS_NUM;

	float fAngleStepRad = DEG2RAD(360.f/GetCVars()->e_panorama_clusters_num);

	if(GetCVars()->e_panorama_clusters_num == 6)
	{ // do 6 single floor impostors
		int nFloorId = 1;
		int nCheckSideId = GetRenderer()->GetFrameID(false)%GetCVars()->e_panorama_clusters_num;

		for(int nClusterId=0; nClusterId<GetCVars()->e_panorama_clusters_num; nClusterId++)
		{ 
			CREPanoramaCluster * & pImp = m_arrImposters[nFloorId][nClusterId];
			if(!pImp)
				pImp = (CREPanoramaCluster*)GetRenderer()->EF_CreateRE(eDATA_PanoramaCluster);

			Vec3 vCamDir(sinf(fAngleStepRad*nClusterId),cosf(fAngleStepRad*nClusterId),(nFloorId-1.f)*0.8f);
			vCamDir.Normalize();

			ProcessPanoramaCluster(pImp,vCamDir,fAngleStepRad, nCheckSideId == nClusterId);

			if(GetCVars()->e_panorama_update_distance<-1)
				break;
		}
	}
	else if(GetCVars()->e_panorama_clusters_num == 8)
	{ // do 8 x 3 impostors + plus top and bottom
		int nImpId = 0;
		int nCheckSideId = GetRenderer()->GetFrameID(false)%(GetCVars()->e_panorama_clusters_num*MAX_PANORAMA_FLOORS_NUM+2);

		for(int nFloorId=0; nFloorId<MAX_PANORAMA_FLOORS_NUM; nFloorId++)
		for(int nClusterId=0; nClusterId<GetCVars()->e_panorama_clusters_num; nClusterId++)
		{ 
			CREPanoramaCluster * & pImp = m_arrImposters[nFloorId][nClusterId];
			if(!pImp)
				pImp = (CREPanoramaCluster*)GetRenderer()->EF_CreateRE(eDATA_PanoramaCluster);

			Vec3 vCamDir(sinf(fAngleStepRad*nClusterId),cosf(fAngleStepRad*nClusterId),(nFloorId-1.f));

			vCamDir.Normalize();
			ProcessPanoramaCluster(pImp, vCamDir, fAngleStepRad, nCheckSideId == nImpId);
			nImpId++;
		}

		// top bottom
		for(int nTop=0; nTop<2; nTop++)
		{ 
			CREPanoramaCluster * & pImp = m_arrImpostersTopBottom[nTop];
			if(!pImp)
				pImp = (CREPanoramaCluster*)GetRenderer()->EF_CreateRE(eDATA_PanoramaCluster);

			Vec3 vCamDir(0,0,nTop*2-1);
			vCamDir.Normalize();
			ProcessPanoramaCluster(pImp,vCamDir,fAngleStepRad,nImpId == nCheckSideId);
			nImpId++;
		}
	}
}

void CTerrain::ProcessPanoramaCluster(CREPanoramaCluster * pImp, const Vec3 & vCamDir, float fFOV, bool bCheckUpdate)
{
	Vec3 vViewCamPos = GetCamera().GetPosition();

	// define camera
	CCamera clusterCamera;
	Matrix34 mat = Matrix33::CreateRotationVDir( vCamDir );
	mat.SetTranslation(vViewCamPos);
	clusterCamera.SetMatrix(mat);
	clusterCamera.SetFrustum( GetCVars()->e_panorama_tex_res, GetCVars()->e_panorama_tex_res, fFOV, 
		GetCamera().GetFarPlane()*0.5f, GetCVars()->e_panorama_distance);

	// find bbox of cluster
	Vec3 arrFrustVerts[8];
	memset(arrFrustVerts,0,sizeof(arrFrustVerts));
	clusterCamera.GetFrustumVertices(arrFrustVerts);
	Vec3 vClusterCamPos = clusterCamera.GetPosition();
	AABB imposterAABB;
	imposterAABB.Reset();
	for(int v=0; v<4; v++)
	{
		arrFrustVerts[v] = vClusterCamPos + (arrFrustVerts[v] - vClusterCamPos).normalized()*clusterCamera.GetNearPlane()*1.4f;
		imposterAABB.Add(arrFrustVerts[v]);
	}

	// render visible clusters
	if(GetCamera().IsAABBVisible_E(imposterAABB))
	{
		pImp->SetCamera(clusterCamera);
		pImp->SetDLDFlags(DLD_TERRAIN|DLD_STATIC_OBJECTS|DLD_FAR_SPRITES|DLD_TERRAIN_WATER|DLD_ENTITIES);

		if(GetCVars()->e_panorama_update_distance<-1)
			pImp->SetPrevCamPos(Vec3(-1000000,-1000000,-1000000));

		// set texture resolution
		pImp->SetTexRes(GetCVars()->e_panorama_tex_res);

		// update if camera moved or cvars was changed
		if(bCheckUpdate)
		{
			// used for cvars change detection
			float fNewCVarsCheckSumm = 
				GetCVars()->e_panorama_distance
				+ GetCVars()->e_panorama_clusters_num
				+ GetCVars()->e_panorama_tex_res
				+ GetCVars()->e_panorama_update_distance
				+ Get3DEngine()->GetMaxViewDistance()
				+ GetCVars()->e_sky_box
				+ GetCVars()->e_terrain
				+ GetCVars()->e_vegetation;

			Vec3 vSunDir = Get3DEngine()->GetSunDir().normalized();
			Vec3 vSunColor = Get3DEngine()->GetSunColor();
			Vec3 vSkyColor = Get3DEngine()->GetSkyColor();

			if(	pImp->m_fCVarsCheckSumm != fNewCVarsCheckSumm || 
				pImp->m_vSunDir.GetDistance(vSunDir)>0.1f ||
				pImp->m_vSunColor.GetDistance(vSunColor)>0.1f ||
				pImp->m_vSkyColor.GetDistance(vSkyColor)>0.1f ||
				pImp->GetPrevCamPos().GetDistance(vViewCamPos) > GetCVars()->e_panorama_update_distance )
			{
				pImp->SetPrevCamPos(Vec3(-1000000,-1000000,-1000000));
				pImp->m_fCVarsCheckSumm = fNewCVarsCheckSumm;
			}
		}

		GetRenderer()->EF_AddEf(pImp, GetTerrain()->m_pImposterEf->GetShaderItem(), GetIdentityCRenderObject(), 0);
	}
}*/