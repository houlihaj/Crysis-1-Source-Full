////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   RoadObject.h
//  Version:     v1.00
//  Created:     25/07/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: RoadObject object definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __RoadObject_h__
#define __RoadObject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Entity.h"
#include "SafeObjectsArray.h"

#define ROAD_CLOSE_DISTANCE 0.8f
#define ROAD_Z_OFFSET 0.1f

// Road Point
struct CRoadPoint
{
	Vec3 pos;
	Vec3 back;
	Vec3 forw;
	float angle;
	float width;
	bool isDefaultWidth;
	CRoadPoint()
	{
		angle = 0;
		width = 0;
		isDefaultWidth = true;
	}
};

typedef std::vector<CRoadPoint> CRoadPointVector;

// Road Sector
struct CRoadSector
{
	std::vector<Vec3> points;
	IRenderNode * m_pRoadSector;
	float t0, t1;

	//static bool isHidden;

	CRoadSector()
	{
		m_pRoadSector = 0;
	}
	~CRoadSector()
	{
		Release();
	}
	//void Init(class CMaterial * pMat);
	void Release();

	CRoadSector(const CRoadSector & old)
	{
		points = old.points;
		t0 = old.t0;
		t1 = old.t1;

		m_pRoadSector = 0;
	}
};

typedef std::vector<CRoadSector> CRoadSectorVector;


/*!
 *	CRoadObject is an object that represent named 3d position in world.
 *
 */
class CRoadObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CRoadObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void InitVariables();
	void Done();

	void Display( DisplayContext &dc );
	void DrawBezierSpline(DisplayContext &dc, CRoadPointVector & points, COLORREF col, bool isDrawJoints, bool isDrawRoad);

	//////////////////////////////////////////////////////////////////////////
	CString GetUniqueName() const;

	void BeginEditParams( IEditor *ie,int flags );
	void EndEditParams( IEditor *ie );

	//! Called when object is being created.
	virtual int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	void GetBoundBox( BBox &box );
	void GetLocalBounds( BBox &box );

	void SetHidden( bool bHidden );
	void UpdateVisibility( bool visible );

	bool HitTest( HitContext &hc );
	bool HitTestRect( HitContext &hc );

	void Serialize( CObjectArchive &ar );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode );

	virtual void SetMaterial( CMaterial *mtl );
	virtual void SetMaterialLayersMask( uint32 nLayersMask );
	virtual void SetMinSpec( uint32 nSpec );

	void SetSelected( bool bSelect );
	//////////////////////////////////////////////////////////////////////////

	//void SetClosed( bool bClosed );
	//bool IsClosed() { return mv_closed; };

	//! Insert new point to Road at givven index.
	//! @return index of newly inserted point.
	int InsertPoint( int index,const Vec3 &point );
	//! Remve point from Road at givven index.
	void RemovePoint( int index );

	//! Get number of points in Road.
	int GetPointCount() { return m_points.size(); };
	//! Get point at index.
	const Vec3&	GetPoint( int index ) const { return m_points[index].pos; };
	//! Set point position at specified index.
	virtual void SetPoint( int index,const Vec3 &pos );

	void SelectPoint( int index );
	int GetSelectedPoint() const { return m_selectedPoint;};

	//! Set Road width.
	float GetWidth() const { return mv_width; };

	//! Find Road point nearest to givven point.
	int GetNearestPoint( const Vec3 &raySrc,const Vec3 &rayDir,float &distance );

	//! Find Road edge nearest to givven point.
	void GetNearestEdge( const Vec3 &pos,int &p1,int &p2,float &distance,Vec3 &intersectPoint );

	//! Find Road edge nearest to givven ray.
	void GetNearestEdge( const Vec3 &raySrc,const Vec3 &rayDir,int &p1,int &p2,float &distance,Vec3 &intersectPoint );
	//////////////////////////////////////////////////////////////////////////

	void CalcBBox();

	void SetRoadSectors();

	Vec3 GetBezierPos(CRoadPointVector & points, int index, float t);
	Vec3 GetLocalBezierNormal(int index, float t);
	float GetLocalWidth(int index, float t);
	Vec3 GetBezierNormal(int index, float t);
	float GetBezierSegmentLength(int index, float t = 1.0f);
	void BezierAnglesCorrection(CRoadPointVector & points, int index);
	void BezierCorrection(int index);

	void AlignHeightMap();

	float GetPointAngle( );
	void SetPointAngle( float angle );

	float GetPointWidth( );
	void SetPointWidth( float width );

	bool IsPointDefaultWidth( );
	void PointDafaultWidthIs( bool IsDefault );


protected:
	bool RayToLineDistance( const Vec3 &rayLineP1,const Vec3 &rayLineP2,const Vec3 &pi,const Vec3 &pj,float &distance,Vec3 &intPnt );
	virtual int GetMaxPoints() const { return 1000; };
	virtual int GetMinPoints() const { return 2; };

	virtual void UpdateSectors();
	virtual void UpdateSector( CRoadSector &sector );

	// Ignore default draw highlight.
	void DrawHighlight( DisplayContext &dc ) {};

	void EndCreation();

	//overrided from CBaseObject.
	void InvalidateTM( int nWhyFlags );

	//! Called when Road variable changes.
	void OnParamChange( IVariable *var );

	//! Dtor must be protected.
	CRoadObject();

	void DeleteThis() { delete this; };

	void InitBaseVariables();

protected:
	BBox m_bbox;

	CRoadPointVector m_points;
	CRoadSectorVector m_sectors;
	
 	CString m_lastGameArea;

	//////////////////////////////////////////////////////////////////////////
	// Road parameters.
	//////////////////////////////////////////////////////////////////////////
	CVariable<float> mv_width;
	CVariable<float> mv_borderWidth;
	CVariable<float> mv_step;
	CVariable<float> mv_tileLength;
	CVariable<int> mv_ratioViewDist;
	CVariable<int> mv_sortPrio;

	int m_selectedPoint;
	//! Forces Road to be always 2D. (all vertices lie on XY plane).
	bool m_bForce2D;
	bool m_bIgnoreParamUpdate;
	bool m_bNeedUpdateSectors;

  const char *m_pszEditParams;

	static int m_rollupId;
	static class CRoadPanel* m_panel;
};


/*!
 * Class Description of RoadObject.	
 */
class CRoadObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {6167DD9D-73D5-4d07-9E92-CF12AF451B08}
		static const GUID guid = { 0x6167dd9d, 0x73d5, 0x4d07, { 0x9e, 0x92, 0xcf, 0x12, 0xaf, 0x45, 0x1b, 0x8 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_ROAD; };
	const char* ClassName() { return "Road"; };
	const char* Category() { return "Misc"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CRoadObject); };
	int GameCreationOrder() { return 50; };
};

#endif // __RoadObject_h__