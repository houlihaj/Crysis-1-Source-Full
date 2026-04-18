/*=============================================================================
  DirectIlluminance.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __DIRECTILLUMINANCE_H__
#define __DIRECTILLUMINANCE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IlluminanceIntegrator.h"
#include "SceneContext.h"

#include "CryArray.h"

class	CDirectIlluminance : public CIlluminanceIntegrator
{
public:
	//fix constructor
	CDirectIlluminance (/*CSceneContext* pSceneContext,Matrix34 *x,SOCKET s,unsigned int flags*/);
	virtual	~CDirectIlluminance()
	{
	};

	//Illuminance statements
	//fix: replace return value by multu-samplies color
	bool Illuminance(CIllumHandler&, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache );
	bool Illuminance(CIllumHandler&, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache );
	void HCLambertShader(Vec3& spectrum, const Vec3& vLightDir, const Vec3& vNormal, ColorF& Cl);
};

#endif