/*=============================================================================
RayMap.cpp: 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#include "stdafx.h"
#include "RayMap.h"
#include "SceneContext.h"


void CRayMap::AddSegment( const Vec3 vStartPosition, const Vec3 vEndPosition, const int32 nStartType, const int32 nEndType )
{
	//insert to the list
	m_vRaySegmentList.push_back( CRayMap_Segment( vStartPosition,vEndPosition,nStartType,nEndType) );

	//update the bbox
	m_BBox.Add(vStartPosition);
	m_BBox.Add(vEndPosition);
}

//fix: implement
bool CRayMap::Illuminance(CIllumHandler& illumHandler, /*string category,*/ const Vec3& vPosition, const t_pairEntityId &LightIDs)
{
	return true;
}

bool CRayMap::Illuminance(CIllumHandler& illumHandler, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId &LightIDs )
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	std::vector<int32> vRayList;
	Search(vRayList, vPosition, sceneCtx.m_fMaxPhotonSearchRadius);
	//SearchWithLN(vRayList, vPosition, sceneCtx.m_fMaxPhotonSearchRadius, vAxis );

	int nNumFound = vRayList.size();

	//fix:: add constant
	if (nNumFound < 2)	
			return true;

	for (int i=0;i<=nNumFound;i++) 
	{
		Vec3 vDir( m_vRaySegmentList[ vRayList[i] ].m_vEndPos - m_vRaySegmentList[ vRayList[i] ].m_vStartPos );
		if ((vDir * vAxis) < 0) 
		{
			Vec3 vCScaled = (Vec3(1,1,1)) * ((float) (1.0 / gf_PI)/sceneCtx.m_fMaxPhotonSearchRadius);
			vDir.Normalize();
			illumHandler.Add(vCScaled, vDir);
		}
	}

	return true;
}
