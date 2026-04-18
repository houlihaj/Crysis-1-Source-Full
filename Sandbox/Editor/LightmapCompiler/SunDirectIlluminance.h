/*=============================================================================
  SunDirectIlluminance.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __SUNDIRECTILLUMINANCE_H__
#define __SUNDIRECTILLUMINANCE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IlluminanceIntegrator.h"
#include "SceneContext.h"

class	CSunDirectIlluminance : public CIlluminanceIntegrator
{
public:
	//fix constructor
	CSunDirectIlluminance (/*CSceneContext* pSceneContext,Matrix34 *x,SOCKET s,unsigned int flags*/);
	virtual	~CSunDirectIlluminance()
	{
	};

	//Illuminance statements
	//fix: replace return value by multu-samplies color
	bool Illuminance(CIllumHandler&, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache );
	bool Illuminance(CIllumHandler&, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache );

private:
	CSceneContext* m_pSceneContext;
};

#endif