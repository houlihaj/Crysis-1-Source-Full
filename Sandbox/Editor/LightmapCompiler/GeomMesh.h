/*=============================================================================
  CGeomMesh.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __GEOMMESH_H__
#define __GEOMMESH_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "SceneContext.h"

//////////////////////////////////////////////////////////////////////////
//	Simple mesh to keep world geometry
//////////////////////////////////////////////////////////////////////////
class CGeomMesh
{
	public:
		// Con/de-struction
		CGeomMesh(IRenderMesh* pRenderMesh, Matrix34* pNodeMatrix, CRenderChunk* pRenderChunk, IRenderShaderResources* shaderResources, uint8* pUsableTriangles );

		virtual ~CGeomMesh (void)
		{
//			m_arrItems.clear();
		};

	private:
		// Disable copying
		CGeomMesh& operator = (CGeomMesh& o) { return *this; };

		bool CreateMesh(IRenderMesh* pRenderMesh, Matrix34* pNodeMatrix, CRenderChunk* pRenderChunk, uint8* pUsableTriangles);


	public:
		// Bounding box for the CRLMesh.
		AABB m_BBox;


	private:
		struct IRenderShaderResources* m_pShaderResources;

		CGeomCore* m_pGeomCore;
		bool m_bIsFilled;

//		std::vector<uint32> m_arrItems;
					
};

#endif