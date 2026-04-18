/*=============================================================================
  IlluminanceIntegrator.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __ILLUMINANCEINTEGRATOR_H__
#define __ILLUMINANCEINTEGRATOR_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IEntity.h"												// EntityId

#include "IllumHandler.h" 
#include "Cry_Matrix.h"
#include "IntersectionCacheCluster.h"

struct	CIlluminanceSample
{
  Vec3	vDir;			// Direction in the camera coordinate system //fix ?
	float	fInvDepth;		// 1 / intersection depth
	float	fDepth;			// The depth
	float	fCoverage;		// Coverage amount 0..1
	Vec3	vEnvDir;			// The envdir amount
	Vec3	vIrradiance;		// The irradiance amount
};

typedef std::vector<std::pair<EntityId, EntityId> > t_pairEntityId;


class	CIlluminanceIntegrator 
{
public:
	//fix constructor
	CIlluminanceIntegrator (/*COptions *options,Matrix34 *x,SOCKET s,unsigned int flags*/)
	{
	};
	virtual	~CIlluminanceIntegrator()
	{
	};

	virtual void Init()
	{
	};

	//Illuminance statements
	//fix: replace return value by multu-samplies color
	virtual bool Illuminance( CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache ) = 0;
	virtual bool Illuminance( CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache ) = 0;

	//set the tighest cell it can be use
	virtual void SetBoundingCell( COctreeCell* pCell )
	{
	};

	//int					nRefCount;
	Matrix34		m_mTransform;

};

#endif