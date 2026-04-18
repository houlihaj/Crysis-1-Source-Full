////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   cbuffer.h
//  Version:     v1.00
//  Created:     30/5/2002 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Occlusion buffer
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef _CBUFFER_H
#define _CBUFFER_H

#define Point2d Vec3

struct SPlaneObject
{
	Plane plane;
	Vec3_tpl<uchar> vIdx1,vIdx2;

	void Update()
	{
		uint32 bitX	=	*((uint32*)&plane.n.x)>>31;
		uint32 bitY	=	*((uint32*)&plane.n.y)>>31;
		uint32 bitZ	=	*((uint32*)&plane.n.z)>>31;
		vIdx1.x=bitX*3+0;	vIdx2.x=(1-bitX)*3+0;
		vIdx1.y=bitY*3+1;	vIdx2.y=(1-bitY)*3+1;
		vIdx1.z=bitZ*3+2;	vIdx2.z=(1-bitZ)*3+2;
	}
};

class CCoverageBuffer : public Cry3DEngineBase
{
public:

	CCoverageBuffer();

	// start new frame
	void BeginFrame(const CCamera & camera);

	// render into buffer
	void AddRenderMesh(IRenderMesh * pRM, Matrix34* pTranRotMatrix, IMaterial * pMaterial, bool bOutdoorOnly, bool bCompletelyInFrustum,bool bNoCull);
//	void AddRenderMeshLC(IRenderMesh * pRM, Matrix34* pTranRotMatrix, IMaterial * pMaterial, bool bOutdoorOnly, bool bCompletelyInFrustum);

	// test visibility
	bool IsObjectVisible(const AABB objBox, EOcclusionObjectType eOcclusionObjectType, float fDistance);

	// Extrude and test shadowcasterBBox visibility
	bool IsShadowcasterVisible(const AABB& objBox,Vec3& rExtrusionDir){return true;}

	// draw content to the screen for debug
	void DrawDebug(int nStep);

	// can be used by other classes
	static void ClipPolygon(PodArray<Vec3> * pPolygon, const Plane & ClipPlane);

	// update tree
	void UpdateDepthTree();

	// check nearest z value
	bool IsOutdooVisible();
	float GetZNearInMeters()const;
	float GetZFarInMeters()const;

	// return current camera
	const CCamera & GetCamera() { return m_Camera; }

	void GetMemoryUsage(ICrySizer * pSizer);

protected:

	// functions
  Point2d ProjectToScreen(const Vec3 & vIn, float * pMatrix);
	Vec3 ProjectFromScreen(const float & x, const float & y, const float & z);
  uint ScanTriangle(const Point2d * pVerts2d, int i1, int i2, int i3, bool bWriteAllowed, ECull nCullMode, bool bOutdoorOnly);
  uint ScanTriangleWithCliping(const Point2d * pVerts2d, const Vec3 * pVerts3d, int i0, int i1, int i2, 
		bool bWriteAllowed, ECull nCullMode, bool bOutdoorOnly, bool bCompletelyInFrustum = false);

	void TransformPoint(float out[4], const float m[16], const float in[4]);
	void MatMul4( float *product, const float *a, const float *b );
	static int ClipEdge(const Vec3 & v1, const Vec3 & v2, const Plane & ClipPlane, Vec3 & vRes1, Vec3 & vRes2);

	bool IsBoxVisible_OCCLUDER(const AABB objBox);
	bool IsBoxVisible_OCEAN(const AABB objBox);
	bool IsBoxVisible_OCCELL(const AABB objBox);
	bool IsBoxVisible_OCCELL_OCCLUDER(const AABB objBox);
	bool IsBoxVisible_OBJECT(const AABB objBox);
	bool IsBoxVisible_OBJECT_TO_LIGHT(const AABB objBox);
	bool IsBoxVisible_TERRAIN_NODE(const AABB objBox);
	bool IsBoxVisible_PORTAL(const AABB objBox);

	bool IsBoxVisible(const AABB objBox);

	int Render2dTriangle(const Point2d * pVerts2d, int i0, int i1, int i2, float fOutdoorOnly, bool bWrite);
	int Render2dSubTriangle(
		int x0, int y0, float z0,
		int x1, int y1, float z1, 
		int x2, int y2, float z2,
		float fOutdoorOnly, bool bWrite);
	void mySwap(int & v0, int & v1) 
	{
		int tmp = v0;
		v0 = v1;
		v1 = tmp;
	}
	void mySwapF(float & v0, float & v1) 
	{
		float tmp = v0;
		v0 = v1;
		v1 = tmp;
	}

#if defined(WIN32) && defined(_CPU_X86)
  inline int fastfround(float f) // note: only positive numbers works correct
  {
    int i;
    __asm fld [f]
    __asm fistp [i]
    return i;
  }
#else
  inline int fastfround(float f) { int i; i=(int)(f+0.5f); return i; } // note: only positive numbers works correct
#endif

	/*struct CWSNode
	{
		CWSNode(int nRecursion);
		void Update( SPlaneObject p, int x1, int x2, int y1, int y2, CCoverageBuffer * pCB, bool bVertSplit, int nRecursion );
		bool IsBoxVisible( const AABB & objBox, CCoverageBuffer * pCB, PodArray<CWSNode*> & lstNodes );
		void DrawDebug(CCoverageBuffer * pCB, bool bDrawPixels);

		SPlaneObject m_SplitPlane;
		SPlaneObject m_FarPlane;
		SPlaneObject m_NearPlane;

		int m_x1,m_x2,m_y1,m_y2;
		int m_nTreeLevel;
		float m_fCurrentZFarMeters, m_fCurrentZNearMeters;
		bool m_bOutdoorVisible;
		CWSNode * m_arrChilds[2];
	} * m_pTree;*/

	// variables
	float m_fHalfRes;
	float m_matCombined[16];
	CCamera m_Camera;

  // temp containers
  PodArray<Vec3>		 m_arrVertsWS_AddMesh;
  PodArray<Point2d> m_arrVertsSS_AddMesh;

	float m_fFrameTime;

  int m_nTrisWriten;
  int m_nObjectsWriten;
  int m_nTrisTested;
  int m_nObjectsTested, m_nObjectsTestedAndRejected;
	int m_nSelRes;
	float m_fFixedZFar;
	float m_fCurrentZNearMeters;
//	bool m_bTreeIsReady;
public:
	Array2d<float> m_farrBuffer;
	float GetFrameTime() { return m_fFrameTime; }
	void SetFrameTime(float fTime) { m_fFrameTime = fTime; }
	void TrisWritten(int TW){m_nTrisWriten=TW;}
	int	TrisWritten()const{return m_nTrisWriten;}
	void ObjectsWritten(int OW){m_nObjectsWriten=OW;}
	int ObjectsWritten()const{return m_nObjectsWriten;}
	int TrisTested()const{return m_nTrisTested;}
	int ObjectsTested()const{return m_nObjectsTested;}
	int ObjectsTestedAndRejected()const{return m_nObjectsTestedAndRejected;}
	int	SelRes()const{return m_nSelRes;}
	float FixedZFar()const{return m_fFixedZFar;}
};

#endif 
