////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   ShapeObject.h
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: ShapeObject object definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ShapeObject_h__
#define __ShapeObject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Entity.h"
#include "SafeObjectsArray.h"
#include "RenderHelpers\AxisHelper.h"

#define SHAPE_CLOSE_DISTANCE 0.8f

/*!
 *	CShapeObject is an object that represent named 3d position in world.
 *
 */
class CShapeObject : public CEntity
{
public:
	DECLARE_DYNCREATE(CShapeObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void InitVariables();
	void Done();

	void Display( DisplayContext &dc );

	//////////////////////////////////////////////////////////////////////////
	void SetName( const CString &name );

	void BeginEditParams( IEditor *ie,int flags );
	void EndEditParams( IEditor *ie );
	void BeginEditMultiSelParams( bool bAllOfSameType );
	void EndEditMultiSelParams();

	//! Called when object is being created.
	int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	void GetBoundBox( BBox &box );
	void GetLocalBounds( BBox &box );

	bool HitTest( HitContext &hc );
	bool HitTestRect( HitContext &hc );

	void OnEvent( ObjectEvent event );
	virtual void PostLoad( CObjectArchive &ar );

	virtual void SpawnEntity();
	void Serialize( CObjectArchive &ar );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode );
	//////////////////////////////////////////////////////////////////////////

	int GetAreaId();

	void SetClosed( bool bClosed );
	bool IsClosed() { return mv_closed; };

	//! Insert new point to shape at given index.
	//! @return index of newly inserted point.
	int InsertPoint( int index,const Vec3 &point );
	//! Set split point.
	int SetSplitPoint(int index, const Vec3 &point, int numPoint);
	//! Split shape on two shapes.
	void Split();
	//! Remove point from shape at given index.
	void RemovePoint( int index );
	//! Set edge index for merge.
	void SetMergeIndex( int index );
	//! Merge shape to this.
	void Merge( CShapeObject * shape );

	//! Get number of points in shape.
	int GetPointCount() { return m_points.size(); };
	//! Get point at index.
	const Vec3&	GetPoint( int index ) const { return m_points[index]; };
	//! Set point position at specified index.
	void SetPoint( int index,const Vec3 &pos );

	//! Reverses the points of the path
	void	ReverseShape();

	void SelectPoint( int index );
	int GetSelectedPoint() const { return m_selectedPoint;};

	//! Set shape width.
	float GetWidth() const { return mv_width; };

	//! Set shape height.
	float GetHeight() const { return mv_height; };

	//! Find shape point nearest to given point.
	int GetNearestPoint( const Vec3 &raySrc,const Vec3 &rayDir,float &distance );

	//! Find shape edge nearest to given point.
	void GetNearestEdge( const Vec3 &pos,int &p1,int &p2,float &distance,Vec3 &intersectPoint );

	//! Find shape edge nearest to given ray.
	void GetNearestEdge( const Vec3 &raySrc,const Vec3 &rayDir,int &p1,int &p2,float &distance,Vec3 &intersectPoint );
	//////////////////////////////////////////////////////////////////////////

	static CAxisHelper& GetSelelectedPointAxisHelper() { return m_selectedPointAxis; }
	bool	UseAxisHelper() const { return m_useAxisHelper; }
	void	SetUseAxisHelper(bool state) { m_useAxisHelper = state; }

	//////////////////////////////////////////////////////////////////////////
	virtual void SetSelected( bool bSelect );

	//////////////////////////////////////////////////////////////////////////
	// Binded entities.
	//////////////////////////////////////////////////////////////////////////
	//! Bind entity to this shape.
	void AddEntity( CBaseObject *entity );
	void RemoveEntity( int index );
	CBaseObject* GetEntity( int index );
	int GetEntityCount() { return m_entities.GetCount(); }
	
	virtual float GetShapeZOffset() const { return 0.1f; }

	virtual void CalcBBox();

protected:
	virtual void PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx );


	bool RayToLineDistance( const Vec3 &rayLineP1,const Vec3 &rayLineP2,const Vec3 &pi,const Vec3 &pj,float &distance,Vec3 &intPnt );
	virtual bool IsOnlyUpdateOnUnselect() const { return false; }
	virtual int GetMaxPoints() const { return 100000; };
	virtual int GetMinPoints() const { return 3; };
	////! Calculate distance between 
	//float DistanceToShape( const Vec3 &pos );
	void DrawTerrainLine( DisplayContext &dc,const Vec3 &p1,const Vec3 &p2 );
	void AlignToGrid();

	// Ignore default draw highlight.
	void DrawHighlight( DisplayContext &dc ) {};

	void EndCreation();

	//! Update game area.
	virtual void UpdateGameArea( bool bRemove=false );

	//overrided from CBaseObject.
	void InvalidateTM( int nWhyFlags );

	//! Called when shape variable changes.
	void OnShapeChange( IVariable *var );

	//! Dtor must be protected.
	CShapeObject();

	void DeleteThis() { delete this; };

protected:
	BBox m_bbox;

	std::vector<Vec3> m_points;
	
	CString m_lastGameArea;
	
	bool	m_useAxisHelper;

	//! List of binded entities.
	//std::vector<uint> m_entities;

	// Entities.
	CSafeObjectsArray m_entities;

	//////////////////////////////////////////////////////////////////////////
	// Shape parameters.
	//////////////////////////////////////////////////////////////////////////
	CVariable<float> mv_width;
	CVariable<float> mv_height;
	CVariable<int> mv_areaId;
	CVariable<int> mv_groupId;
	CVariable<int> mv_priority;
	
	//! make roof completely block fading
	CVariable<bool> mv_obstructRoof;
	//! make floor completely block fading
	CVariable<bool> mv_obstructFloor;

	CVariable<bool> mv_closed;
	//! Display triangles filling closed shape.
	CVariable<bool> mv_displayFilled;

	int m_selectedPoint;
	float m_lowestHeight;
	
	uint32 m_bAreaModified : 1;
	//! Forces shape to be always 2D. (all vertices lie on XY plane).
	uint32 m_bForce2D : 1;
	uint32 m_bDisplayFilledWhenSelected : 1;
	uint32 m_bIgnoreGameUpdate : 1;
	uint32 m_bPerVertexHeight : 1;

	static int m_rollupId;
	static class CShapePanel* m_panel;
	static int m_rollupMultyId;
	static class CShapeMultySelPanel* m_panelMulty;
	static CAxisHelper	m_selectedPointAxis;

	Vec3 m_splitPoints[2];
	int m_splitIndicies[2];
	int m_numSplitPoints;

	int m_mergeIndex;

	bool m_updateSucceed;
};

/*!
 * Class Description of ShapeObject.	
 */
class CShapeObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {6167DD9D-73D5-4d07-9E92-CF12AF451B08}
		static const GUID guid = { 0x6167dd9d, 0x73d5, 0x4d07, { 0x9e, 0x92, 0xcf, 0x12, 0xaf, 0x45, 0x1b, 0x8 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_SHAPE; };
	const char* ClassName() { return "Shape"; };
	const char* Category() { return "Area"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CShapeObject); };
	int GameCreationOrder() { return 50; };
};

//////////////////////////////////////////////////////////////////////////
// Base class for all AI Shapes
//////////////////////////////////////////////////////////////////////////
class CAIShapeObjectBase : public CShapeObject
{
public:
	CAIShapeObjectBase() { m_entityClass = ""; }
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode ) { return 0; };
};
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Special object for forbidden area.
//////////////////////////////////////////////////////////////////////////
class CAIForbiddenAreaObject : public CAIShapeObjectBase
{
	DECLARE_DYNCREATE(CAIForbiddenAreaObject)
public:
	CAIForbiddenAreaObject();
	// Ovverride game area creation.
	virtual void UpdateGameArea( bool bRemove=false );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode ) { return 0; };
};

//////////////////////////////////////////////////////////////////////////
// Special object for forbidden boundary.
//////////////////////////////////////////////////////////////////////////
class CAIForbiddenBoundaryObject : public CAIShapeObjectBase
{
	DECLARE_DYNCREATE(CAIForbiddenBoundaryObject)
public:
	CAIForbiddenBoundaryObject();
	// Ovverride game area creation.
	virtual void UpdateGameArea( bool bRemove=false );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode ) { return 0; };
};

//////////////////////////////////////////////////////////////////////////
// Special object for AI Walkabale paths.
//////////////////////////////////////////////////////////////////////////
class CAIPathObject : public CAIShapeObjectBase
{
	DECLARE_DYNCREATE(CAIPathObject)
public:
	CAIPathObject();
	// Ovverride game area creation.
	virtual void UpdateGameArea( bool bRemove=false );
	void Display( DisplayContext &dc );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode ) { return 0; };
private:

	bool	IsSegmentValid(const Vec3& p0, const Vec3& p1, float rad);
	void	DrawSphere(DisplayContext &dc, const Vec3& p0, float rad, int n);
	void	DrawCylinder(DisplayContext &dc, const Vec3& p0, const Vec3& p1, float rad, int n);

	CVariable<bool> m_bRoad;
	CVariableEnum<int> m_navType;
	CVariable<CString> m_anchorType;
	CVariable<bool> m_bValidatePath;
};

//////////////////////////////////////////////////////////////////////////
// Generic AI shape
//////////////////////////////////////////////////////////////////////////
class CAIShapeObject : public CAIShapeObjectBase
{
	DECLARE_DYNCREATE(CAIShapeObject)
public:
	CAIShapeObject();
	// Ovverride game area creation.
	virtual void UpdateGameArea( bool bRemove=false );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode ) { return 0; };
private:
	CVariable<CString> m_anchorType;
	CVariableEnum<int> m_lightLevel;
};

//////////////////////////////////////////////////////////////////////////
// Special object for AI Walkabale paths.
//////////////////////////////////////////////////////////////////////////
class CAINavigationModifierObject : public CAIShapeObjectBase
{
	DECLARE_DYNCREATE(CAINavigationModifierObject)
public:
	CAINavigationModifierObject();
	// Ovverride game area creation.
	virtual void UpdateGameArea( bool bRemove=false );
  /// override serialisation just to handle the legacy waypointConnections
 	void Serialize( CObjectArchive &ar );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode ) { return 0; };
private:
	CVariable<bool> m_bCalculate3DNav;
  CVariable<float> m_f3DNavVolumeRadius;
	CVariableEnum<int> m_navType;
	CVariableEnum<int> m_lightLevel;
	CVariableEnum<int> m_waypointConnections;
	int m_oldWaypointConnections;
	CVariable<float> m_fNodeAutoConnectDistance;
	CVariable<float> m_fExtraLinkCostFactor;
	CVariable<float> m_fTriangulationSize;
	CVariable<bool> m_bVehiclesInHumanNav;
};

//////////////////////////////////////////////////////////////////////////
// Special object for AI Walkabale paths.
//////////////////////////////////////////////////////////////////////////
class CAIOcclusionPlaneObject : public CAIShapeObjectBase
{
	DECLARE_DYNCREATE(CAIOcclusionPlaneObject)
public:
	CAIOcclusionPlaneObject();
	// Ovverride game area creation.
	virtual void UpdateGameArea( bool bRemove=false );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode ) { return 0; };
};

/*!
 * Class Description of CForbiddenAreaObject.
 */
class CAIForbiddenAreaObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {9C9D5FD9-761B-4734-B2AA-38990E4C2EB9}
		static const GUID guid = { 0x9c9d5fd9, 0x761b, 0x4734, { 0xb2, 0xaa, 0x38, 0x99, 0xe, 0x4c, 0x2e, 0xb9 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_SHAPE; };
	const char* ClassName() { return "ForbiddenArea"; };
	const char* Category() { return "AI"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAIForbiddenAreaObject); };
	int GameCreationOrder() { return 50; };
};

/*!
 * Class Description of CForbiddenBoundaryObject.
 */
class CAIForbiddenBoundaryObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {97AF0F55-DE0A-4208-8698-D8CF2131F95D}
		static const GUID guid = { 0x97af0f55, 0xde0a, 0x4208, { 0x86, 0x98, 0xd8, 0xcf, 0x21, 0x31, 0xf9, 0x5d } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_SHAPE; };
	const char* ClassName() { return "ForbiddenBoundary"; };
	const char* Category() { return "AI"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAIForbiddenBoundaryObject); };
	int GameCreationOrder() { return 50; };
};

/*!
 * Class Description of CAIPathObject.
 */
class CAIPathObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {06380C56-6ECB-416f-9888-60DE08F0280B}
		static const GUID guid = { 0x6380c56, 0x6ecb, 0x416f, { 0x98, 0x88, 0x60, 0xde, 0x8, 0xf0, 0x28, 0xb } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_SHAPE; };
	const char* ClassName() { return "AIPath"; };
	const char* Category() { return "AI"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAIPathObject); };
	int GameCreationOrder() { return 50; };
};

/*!
* Class Description of CAIShapeObject.
*/
class CAIShapeObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {2DDF910A-3D86-4f13-BF9D-2F4C1F5F6334}
		static const GUID guid =  { 0x2ddf910a, 0x3d86, 0x4f13, { 0xbf, 0x9d, 0x2f, 0x4c, 0x1f, 0x5f, 0x63, 0x34 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_SHAPE; };
	const char* ClassName() { return "AIShape"; };
	const char* Category() { return "AI"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAIShapeObject); };
	int GameCreationOrder() { return 50; };
};

/*!
 * Class Description of CAINavigationModifierObject.
 */
class CAINavigationModifierObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {2AD95C7B-5548-435b-BF0C-63632D7FEA40}
		static const GUID guid = { 0x2ad95c7b, 0x5548, 0x435b, { 0xbf, 0xc, 0x63, 0x63, 0x2d, 0x7f, 0xea, 0x40 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_SHAPE; };
	const char* ClassName() { return "AINavigationModifier"; };
	const char* Category() { return "AI"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAINavigationModifierObject); };
	int GameCreationOrder() { return 50; };
};

/*!
* Class Description of CAIOcclusionPlaneObject.
*/
class CAIOcclusionPlaneObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {85F4FEB1-1E97-4d78-BC0F-CACEA0D539C3}
		static const GUID guid = 
		{ 0x85f4feb1, 0x1e97, 0x4d78, { 0xbc, 0xf, 0xca, 0xce, 0xa0, 0xd5, 0x39, 0xc3 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_SHAPE; };
	const char* ClassName() { return "AIHorizontalOcclusionPlane"; };
	const char* Category() { return "AI"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAIOcclusionPlaneObject); };
	int GameCreationOrder() { return 50; };
};

#endif // __ShapeObject_h__