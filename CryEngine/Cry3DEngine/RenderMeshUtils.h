////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   RenderMeshUtils.h
//  Created:     14/11/2006 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __RenderMeshUtils_h__
#define __RenderMeshUtils_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
// RenderMesh utilities.
//////////////////////////////////////////////////////////////////////////
class CRenderMeshUtils : public Cry3DEngineBase
{
public:
	// Do a render-mesh vs Ray intersection, return true for intersection.
	static bool RayIntersection( IRenderMesh *pRenderMesh, SRayHitInfo &hitInfo,IMaterial *pCustomMtl=0 );

	//////////////////////////////////////////////////////////////////////////
	//void FindCollisionWithRenderMesh( IRenderMesh *pRenderMesh, SRayHitInfo &hitInfo );
//	void FindCollisionWithRenderMesh( IPhysiIRenderMesh *pRenderMesh, SRayHitInfo &hitInfo );
};



#endif // __RenderMeshUtils_h__