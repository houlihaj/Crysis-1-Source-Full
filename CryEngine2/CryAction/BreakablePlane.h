/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2005.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 26:5:2005   : Created by Anton Knyazyev
*************************************************************************/

#ifndef __BreakablePlane_H__
#define __BreakablePlane_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IIndexedMesh.h"
#include "IPhysics.h"


class CBreakablePlane 
{
 public:
	CBreakablePlane() 
	{ 
		m_pGrid=0; m_pMat=0;
		m_nCells.set(20,20);
		m_cellSize = 1;
		m_maxPatchTris = 20;
		m_jointhresh = 0.3f;
		m_density = 900;
		m_bStatic = 1;
		m_pGeom = 0; m_pSampleRay = 0;
	}
	~CBreakablePlane()
	{
		if (m_pGrid) m_pGrid->Release();
		if (m_pSampleRay) m_pSampleRay->Release();
	}

	bool SetGeometry(IStatObj *pStatObj, int bStatic, int seed);
	void FillVertexData(CMesh *pMesh,int ivtx, const vector2df &pos, int iside);
	IStatObj *CreateFlatStatObj(int *&pIdx, vector2df *pt, vector2df *bounds, const Matrix34 &mtxWorld, IParticleEffect *pEffect=0, bool bNoPhys=false);
	int *Break(const Vec3 &pthit, float r, vector2df *&ptout, int seed);
	static IStatObj *ProcessImpact(const Vec3 &pthit, const Vec3 &hitvel,float hitmass,float hitradius, IStatObj *pStatObj, const Matrix34 &mtx, 
		ISurfaceType *pMat,IMaterial *pRenderMat,int bStatic, struct SBreakEvent &bev,std::vector<EntityId> &chunkIds,bool bLoading);

	float m_cellSize;
	vector2di m_nCells;
	float m_nudge;
	int m_maxPatchTris;
	float m_jointhresh;
	float m_density;
	IBreakableGrid2d *m_pGrid;
	Matrix33 m_R;
	Vec3 m_center;
	int m_bAxisAligned;
	int m_matId;
	IMaterial *m_pMat;
	int m_matSubindex;
	int m_matFlags;
	float m_z[2];
	float m_thicknessOrg;
	float m_refArea[2];
	vector2df m_ptRef[2][3];
	SMeshTexCoord m_texRef[2][3];
	SMeshTangents	m_TangentRef[2][3];
	int m_bStatic;
	IGeometry *m_pGeom,*m_pSampleRay;
	static int g_nPieces;
	static float g_maxPieceLifetime;
};

#endif //__BreakablePlane_H__