/*=============================================================================
RayMap_Segment.h : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef RAYMAP_SEGMENT_H
#define RAYMAP_SEGMENT_H

#if _MSC_VER > 1000
# pragma once
#endif


class CRayMap_Segment
{
public:
	CRayMap_Segment()
	{
	}

	CRayMap_Segment( const Vec3 vStartPos, const Vec3 vEndPos, const int nStartInfo, const int nEndInfo ) : 
	m_vStartPos( vStartPos ), m_vEndPos( vEndPos ), m_nStartSurfaceInfo(nStartInfo) , m_nEndSurfaceInfo( nEndInfo)
	{
	}

	Vec3	m_vStartPos,m_vEndPos;	/// I make different between start & end pos, maybe usefull one day
	int32		m_nStartSurfaceInfo;
	int32		m_nEndSurfaceInfo;
};

#endif//RAYMAP_SEGMENT_H