/*=============================================================================
  IllumHandler.cpp : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/

#include "StdAfx.h"
#include "IllumHandler.h"
#include "SceneContext.h"

#define cos30 ( sqrt(3.0)/2.0 )
#define sin30 ( 1.0/2.0 )

/*const Vec3 CIllumHandler::m_vBasisComponents[NUM_COMPONENTS] = 
{
	Vec3(  sqrt(sin30/2.0),  sqrt(sin30/2.0), cos30), //XY
	Vec3( -sqrt(sin30/2.0),  sqrt(sin30/2.0), cos30),	//-XY
	Vec3( -sqrt(sin30/2.0), -sqrt(sin30/2.0), cos30), //-X-Y
	Vec3(  sqrt(sin30/2.0), -sqrt(sin30/2.0), cos30)  //X-Y
};*/

const Vec3 CIllumHandler::m_vBasisComponents[4] = 
{
	Vec3(  1.0,  0.0, 0.0),
	Vec3( -sin30,  cos30, 0.0),
	Vec3( -sin30, -cos30, 0.0),
	Vec3(  0.0, 0.0, 1.0)
};


//===================================================================================
// Method				:	Add
// Description	:	add a spectrum (3 component) with direction to handler
//===================================================================================
CIllumHandler& CIllumHandler::Add(const Vec3& spectrum, const Vec3& vIrradDir)
{
  int32 numProjComp = 0;
	//assert(vIrradDir.IsNormalized());
	Vec3 vIrradDirNeg = -vIrradDir;

	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	if (sceneCtx.m_numComponents == 4)//path for occlusion maps and ordinary lightmaps
	{
		m_vLightFlux[0].x +=  spectrum.x;
		m_vLightFlux[0].y +=  spectrum.y;
		m_vLightFlux[0].z +=  spectrum.z;
		return *this;
	}

	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
	for (int32 i = 0; i < nMaxComp; ++i)
	{
		//hack: remove
		//m_vLightFlux[i] += spectrum * (vIrradDirNeg.Dot(m_vNormal));

		//hack: uncomment
		f32 fWeight = vIrradDirNeg.Dot(m_vDirComponents[i]);
		if (fWeight>0.0f)
		{
			m_vLightFlux[i].x +=  (fWeight * spectrum.x);
			m_vLightFlux[i].y +=  (fWeight * spectrum.y);
			m_vLightFlux[i].z +=  (fWeight * spectrum.z);
			numProjComp++;
		}
	}

	//fix
	//assert(numProjComp>=3);
//	assert(numProjComp>=2);

	return *this;
}

//===================================================================================
// Method				:	Add
// Description	:	add a spectrum (4 component) to handler directly
//===================================================================================
CIllumHandler& CIllumHandler::Add(const Vec4& spectrum)
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
	for (int32 i = 0; i < nMaxComp; ++i)
		m_vLightFlux[i] += spectrum;
	return *this;
}


//===================================================================================
// Method				:	+ operator
// Description	:	add together two handler
//===================================================================================
CIllumHandler CIllumHandler::operator +(const CIllumHandler& illumHandler) const
{

	CIllumHandler retHandler(0.0f);
	int32 numProjComp = 0;
	//assert(vIrradDir.IsNormalized());

	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	//fix: consider Dir Coords
	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
	for (int i = 0; i < nMaxComp; ++i)
	{
		retHandler.m_vLightFlux[i] = m_vLightFlux[i] + illumHandler.m_vLightFlux[i];
	}

	return retHandler;
}

//===================================================================================
// Method				:	+= operator
// Description	:	add a handler to this
//===================================================================================
CIllumHandler& CIllumHandler::operator +=(const CIllumHandler& illumHandler)
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	//fix: consider Dir Coords
	int32 nMaxComp = (sceneCtx.m_numComponents+3)/4;
	for (int i = 0; i < nMaxComp; ++i)
	{
		m_vLightFlux[i] +=illumHandler.m_vLightFlux[i];
	}

	return *this;
}

//===================================================================================
// Method				:	SetTangentBases
// Description	:	setup the base axes of the handler
//===================================================================================
void CIllumHandler::SetTangentBasis(Vec3& vTangent, Vec3& vBinormal, Vec3& vNormal)
{
	Matrix33 mInvRot(vTangent, vBinormal, vNormal);
  SetTangentBasis(mInvRot);

	//hack: remove
	//m_vNormal = vNormal;

	return;
}

//===================================================================================
// Method				:	SetTangentBases
// Description	:	setup the base axes of the handler
//===================================================================================
void CIllumHandler::SetTangentBasis(Matrix33& mInvRot)
{

	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	for (int i = 0; i < NUM_COMPONENTS; ++i)
	{
		m_vDirComponents[i] = mInvRot.TransformVector(m_vBasisComponents[i]);
	}

	return;
}
