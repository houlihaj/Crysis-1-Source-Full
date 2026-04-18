////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CullBuffer.h
//  Version:     v1.00
//  Created:     13/8/2006 by Michael Kopietz
//  Compilers:   Visual Studio.NET
//  Description: Occlusion buffer main include
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef _CULLBUFFER_H
#define _CULLBUFFER_H

#include "ObjMan.h" // EOcclusionObjectType
#include "cbuffer.h"
#include "CullBuffer.h"
#include "COcclusionCuller.h"

#define OCCLUSIONCULLER_RUNTIME
//#define OCCLUSIONCULLER_V
//#define OCCLUSIONCULLER_M


#if defined(OCCLUSIONCULLER_RUNTIME) && !defined(OCCLUSIONCULLER_V) && !defined(OCCLUSIONCULLER_M)
class CCullBuffer : public Cry3DEngineBase
{
	CCoverageBuffer		m_BufferV;
	COcculsionCuller	m_BufferM;
	int								m_CBuffer;
public:

	// start new frame
	void											BeginFrame(const CCamera& rCam);
	// render into buffer
	void											AddRenderMesh(IRenderMesh * pRM, Matrix34* pTranRotMatrix, IMaterial * pMaterial, bool bOutdoorOnly, bool bCompletelyInFrustum,bool bNoCull);
	void											AddHeightMap(const struct SRangeInfo & m_rangeInfo, float X1, float Y1, float X2, float Y2);

	// test visibility
	bool											IsObjectVisible(const AABB objBox, EOcclusionObjectType eOcclusionObjectType, float fDistance);

	// Extrude and test shadowcasterBBox visibility
	bool											IsShadowcasterVisible(const AABB& objBox,Vec3 rExtrusionDir);

	// draw content to the screen for debug
	void											DrawDebug(int nStep);

	// update tree
	void											UpdateDepthTree();
	// return current camera
	const CCamera&						GetCamera();

	void											GetMemoryUsage(ICrySizer * pSizer);

	void											SetFrameTime(f32 fTime) ;
	f32												GetFrameTime() ;
	bool											IsOutdooVisible();
	void											TrisWritten(int n);
	int												TrisWritten()const;
	void											ObjectsWritten(int n);
	int												ObjectsWritten()const;
	int												TrisTested()const;
	int												ObjectsTested()const;
	int												ObjectsTestedAndRejected()const;
	int												SelRes()const;
	float											FixedZFar()const;
	float											GetZNearInMeters()const;
	float											GetZFarInMeters()const;
};
#elif !defined(OCCLUSIONCULLER_RUNTIME) && defined(OCCLUSIONCULLER_V) && !defined(OCCLUSIONCULLER_M)
class CCullBuffer	:	public	CCoverageBuffer
{

};
#elif !defined(OCCLUSIONCULLER_RUNTIME) && !defined(OCCLUSIONCULLER_V) && defined(OCCLUSIONCULLER_M)
class CCullBuffer	:	public	COcculsionCuller
{

};
#else
Error unsupported combination of occlusionculler compileflags
#endif


#endif 
