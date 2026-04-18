/*=============================================================================
  DirectIlluminance.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __INDIRECTILLUMINANCE_H__
#define __INDIRECTILLUMINANCE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IlluminanceIntegrator.h"
#include "SceneContext.h"

class	CIndirectIlluminance : public CIlluminanceIntegrator
{
public:

	CIndirectIlluminance (/*CSceneContext* pSceneContext,Matrix34 *x,SOCKET s,unsigned int flags*/);
	virtual	~CIndirectIlluminance()
	{
	};


	//Illuminance statements
	//fix: replace return value by multu-samplies color
	bool Illuminance(CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache );
	bool Illuminance(CIllumHandler& spectrum, /*string category,*/ const Vec3& vPosition, const Vec3& vAxis, const float angle, const t_pairEntityId *LightIDs, CIntersectionCacheCluster* pIntersectionCache );
	void	Sample(CIllumHandler& vColO,Vec3& vEnvDirO, f32& fCoverageO,
		const Vec3& vPosI,const Vec3& vNormI);
	//void HCLambertShader(Vec3& spectrum, const Vec3& vLightDir, const Vec3& vNormal, ColorF& Cl);

private:
	void HCLambertShader(Vec3& spectrum, const Vec3& vLightDir, const Vec3& vNormal, const Vec3& vCl);

	f32 FRand() const 
	{ 
		return ((f32)rand()/(f32)RAND_MAX); 
	}


private:

	CSceneContext* m_pSceneCtx;

};

#endif