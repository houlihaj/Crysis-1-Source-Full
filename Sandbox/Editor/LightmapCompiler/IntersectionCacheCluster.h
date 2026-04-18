/*=============================================================================
IntersectionCacheCluster.h :
Copyright 2006 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef __INTERSECTIONCACHECLUSTER_H__
#define __INTERSECTIONCACHECLUSTER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "CryOctree.h"


class CIntersectionCacheCluster
{
public:
	CIntersectionCacheCluster():m_pCluster(NULL)
	{};
	~CIntersectionCacheCluster();

	void					Init( const int32 nSideSize );															/// One size of a cluster
	int32					GetIndex( const Vec3& vDirection ) const;									/// Give back the offset in the cluster based on direction (mapping 3D vec to 1D) Vec3 need to be normalized!
	COctreeCell*	GetCell( const int32 nIndex ) const
	{
		return m_pCluster[ nIndex ];
	}																																					/// Get the last intersection cell based on the 1D offset
	void					SetCell( const int32 nIndex, COctreeCell* pCell );		/// Set the last intersection cell based on the 1D offset
protected:
	COctreeCell**	m_pCluster;																									/// The cluster itself
	int32					m_nFaceSize;																								/// The size of a face
	int32					m_nSideSize;																								/// The size of a side
	f32						m_fHalfSideSize;																						/// The half of a side of face
};

#endif//INTERSECTIONCACHECLUSTER