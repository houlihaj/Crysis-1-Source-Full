/*=============================================================================
  PhotonTrace.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/

#ifndef __PHOTONTRACE_H__
#define __PHOTONTRACE_H__

#include "cry_math.h"

//dep
//#include "shading.h" //implement uniform shading interface
#include "Ray.h"
#include "GeomCore.h"
#include "CryArray.h"
#include "RLWaitProgress.h"


//===================================================================================
// Class				:	CPhotonTrace
// Description			:	This class implements photonmap shading
class	CPhotonTrace
{

	//implement dynamic photon power estimation
	typedef enum 
	{
		PHOTON_ESTIMATE,
		PHOTON_TRACE
	} EPhotonStage;

public:
		CPhotonTrace(uint32 numEmitPhotons, uint32 nTessNum);
		virtual		~CPhotonTrace();


			void		Shade(CRLWaitProgress* pWaitProgress);// Right after world end to force rendering of the entire frame
			void		IlluminateTest(Vec3& vFrom, Vec3& vC);
protected:
			void		IlluminateBegin(const Vec3& vP, const ColorF& vC,const Vec3* pvN, const float* pTheta);
			void		IlluminateEnd();
			void		SolarBegin(const Vec3& vL,const float *pTheta, Vec3& vSunColor);
			void		SolarEnd();

			void		ShadowBegin(const Vec3& vP,const Vec3* pvN, const float* pTheta);
			void		ShadowEnd();

private:
			void		TraceShadowPhoton(const Vec3& vP,const Vec3& vL);
			void		TracePhoton(const Vec3& vP,const Vec3& vL,const ColorF& vC);
			void		SampleCosineHemisphere(Vec3& vR,const Vec3& vZ,const float theta);
			void		SampleHemisphere(Vec3& vR,const Vec3& vZ,const float theta);
public:

private:
		CGeomCore* m_pGeomCore;
		uint32		m_numEmitPhotons;
		uint32		m_nTessNum;
		uint32		m_nMaxDiffuseDepth;

		uint32		m_numVertices; //num vertices for photon shading

		CPhotonMap* m_pGlobalMap;
		CPhotonMap* m_pCausticMap;

		float					m_fPhotonPower;			// The scale factor for the current batch

		EPhotonStage	m_stage;					// The current stage

		float					m_fPowerScale;				// The scaling factor for individual photon powers
		float					m_fMinPower;				// The variables to find the range of the illumination
		float					m_fMaxPower;				// for the current light
		float					m_fAvgPower;
		float					m_fNumPower;

		//fix: implement rand generators
		//////////////////////////////////////////////////////////////////////////

		float					m_fWorldRadius;			// The radius of the world
		Vec3					m_vWorldCenter;			// The center of the world

		//fix::
		std::vector<CPhotonMap *>	balanceList;			// The list of photon maps that need re-balancing

		//CSurface				*phony;					// Phony object we used on the light sources
};

#endif


