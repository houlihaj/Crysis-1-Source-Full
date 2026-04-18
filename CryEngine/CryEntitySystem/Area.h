////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   Area.h
//  Version:     v1.00
//  Created:     27/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __Area_h__
#define __Area_h__
#pragma once

#include "AreaManager.h"
#include "EntitySystem.h"

class CArea
{

public:

	struct a2DPoint
	{
		float x, y;
		a2DPoint(void):x(0.0f),y(0.0f) { }
		a2DPoint( const Vec3& pos3D ) { x=pos3D.x; y=pos3D.y; }
		float	DistSqr( const struct a2DPoint& point ) const
		{
			float xx = x-point.x;
			float yy = y-point.y;
			return (xx*xx + yy*yy);
		}
		float	DistSqr( const float px, const float py ) const
		{
			float xx = x-px;
			float yy = y-py;
			return (xx*xx + yy*yy);
		}

	};
	struct a2DBBox
	{
		a2DPoint	min;	// 2D BBox min 
		a2DPoint	max;	// 2D BBox max
		bool	PointOutBBox2D (const a2DPoint& point) const
		{
			return (point.x<min.x || point.x>max.x || point.y<min.y || point.y>max.y);
		}
		bool	PointOutBBox2DVertical (const a2DPoint& point) const
		{
			return (point.y<=min.y || point.y>max.y || point.x>max.x);
		}
		bool	BBoxOutBBox2D (const a2DBBox& box) const
		{
			return (box.max.x<min.x || box.min.x>max.x || box.max.y<min.y || box.min.y>max.y);
		}

	};
	struct a2DSegment
	{
		bool	isHorizontal;			//horizontal flag
		float	k, b;			//line parameters y=kx+b
		a2DBBox	bbox;		// segment's BBox 
		bool IntersectsXPos( const a2DPoint& point ) const
		{
			return ( point.x < (point.y - b)/k );
		}

		float GetIntersectX( const a2DPoint& point ) const
		{
			return (point.y - b)/k;
		}

		bool IntersectsXPosVertical( const a2DPoint& point ) const
		{
			if(k==0.0f)
				return ( point.x <= b );
			return false;
		}

		a2DPoint GetStart() const
		{
			a2DPoint Start;
			if( k<0 )
			{
				Start.x = bbox.min.x;
				Start.y = bbox.max.y;
			}
			else
			{
				Start.x = bbox.min.x;
				Start.y = bbox.min.y;
			}
			return Start;
		}

		a2DPoint GetEnd() const
		{
			a2DPoint End;
			if( k<0 )
			{
				End.x = bbox.max.x;
				End.y = bbox.min.y;
			}
			else
			{
				End.x = bbox.max.x;
				End.y = bbox.max.y;
			}
			return End;
		}
	};

	//////////////////////////////////////////////////////////////////////////
	CArea( CAreaManager *pManager );

	// Releases area.
	void Release();

	void	SetID( const int id ) { m_AreaID = id; }
	int		GetID() const { return m_AreaID; }

	void	SetEntityID( const EntityId id ) { m_EntityID = id; }
	int		GetEntityID() const { return m_EntityID; }

	void	SetGroup( const int id) { m_AreaGroupID = id; } 
	int		GetGroup( ) const { return m_AreaGroupID; } 

	void SetPriority ( const int nPriority) { m_nPriority = nPriority; }
	int  GetPriority ( ) const { return m_nPriority; }

	// Description:
	//    Sets if the Area should fully obstruct it's Roof or Floor (on Box or Shape)
	virtual void	SetObstruction( bool bRoof, bool bFloor ) { m_bObstructRoof = bRoof; m_bObstructFloor = bFloor; }

	void SetAreaType( EEntityAreaType type );
	EEntityAreaType GetAreaType( ) const { return m_AreaType; } 

	//////////////////////////////////////////////////////////////////////////
	// These functions also switch area type.
	//////////////////////////////////////////////////////////////////////////
	void	SetPoints( const Vec3* const vPoints, const int count );
	void	SetBox( const Vec3& min,const Vec3& max,const Matrix34 &tm );
	void	SetSphere( const Vec3& vCenter,float fRadius );
	//////////////////////////////////////////////////////////////////////////

	void	SetBoxMatrix( const Matrix34 &tm );
	void GetBoxMatrix( Matrix34& tm );

	void GetBox( Vec3 &min,Vec3 &max ) { min = m_BoxMin; max = m_BoxMax; }
	void GetSphere( Vec3& vCenter,float &fRadius ) { vCenter = m_SphereCenter; fRadius = m_SphereRadius; }
	
	void  SetHeight( float fHeight ) { m_VSize = fHeight; }
	float GetHeight() const { return m_VSize; }
	float GetAreaSize();

	void	SetFade( const float fFade) { m_PrevFade = fFade; }
	float GetFade( ) const { return m_PrevFade; }

	void	AddEntity( const EntityId entId );
	void	AddEntites( const std::vector<EntityId> &entIDs );
	void	GetEntites( std::vector<EntityId> &entIDs );

	void	ClearEntities( );

	void	SetProximity( float prx ) {m_fProximity = prx;}
	float	GetProximity( ) { return m_fProximity;}

	float GetMaximumEffectRadius();

	float	IsPointWithinDist(const Vec3& point3d) const;
	bool	IsPointWithin(const Vec3& point3d, bool bIgnoreHeight=false) const;
	float	CalcDistToPoint( const a2DPoint& point ) const;

	float ClosestPointOnHullDistSq(const Vec3 &Point3d, Vec3 &OnHull3d) const;

	// squared-distance returned works only if point32 is not within the area
	float PointNearDistSq(const Vec3 &Point3d, Vec3 &OnHull3d) const;
	float PointNearDistSq(const Vec3 &Point3d, bool bIgnoreHeight = false) const;


	// events
	void					SetCachedEvent	(const SEntityEvent* pNewEvent);
	SEntityEvent* GetCachedEvent	() { return &m_CachedEvent; };
	void					SendCachedEvent	();

	// Inside
	void	EnterArea( IEntity* pEntity );
	void	LeaveArea( IEntity* pEntity );
	void	UpdateArea( const Vec3 &vPos,IEntity* pEntity );
	void	UpdateAreaInside(  IEntity* pEntity);
	void	ExclusiveUpdateAreaInside(  IEntity* pEntity, EntityId AreaHighID);
	float	CalculateFade( const Vec3& pos3D );
	void	ProceedFade( IEntity *pEntity, const float fadeValue );

	//Near
	void	EnterNearArea( IEntity* pEntity , float fDistanceSq);
	void	LeaveNearArea( IEntity* pEntity );
	void	UpdateAreaNear(  IEntity* pEntity, float fDistanceSq );

	void	Draw( const int idx );

	unsigned MemStat();

	void SetStepId( int id ) { m_stepID = id;}
	int  GetStepId() const { return m_stepID; }
	void SetActive( bool bActive ) { m_bIsActive = bActive; }
	bool IsActive() const { return m_bIsActive; }
	bool HasSoundAttached();
	void DrawDebugInfo(const Vec3& center);

	void GetBBox(Vec2& vMin, Vec2& vMax);

private:
	~CArea();
	void	AddSegment(const a2DPoint& p0, const a2DPoint& p1);
	void	CalcBBox();
	void	ClearPoints();
	IEntitySystem* GetEntitySystem() { return m_pAreaManager->GetEntitySystem(); }

private:
	CAreaManager *m_pAreaManager;
	float	m_fProximity;
	float m_fMaximumEffectRadius;

	//	attached entities IDs list
	std::vector<EntityId>			m_vEntityID;
	SEntityEvent							m_CachedEvent;

	float	m_PrevFade;

	int				m_AreaID;
	int				m_AreaGroupID;
	EntityId	m_EntityID;
	int				m_nPriority;

	Matrix34	m_InvMatrix;

	EEntityAreaType	m_AreaType;

	// for shape areas ----------------------------------------------------------------------
	// area's bbox
	a2DBBox	m_AreaBBox;
	// the area segments
	std::vector<a2DSegment*>	m_vpSegments;
	float m_fShapeArea;

	// for sector areas ----------------------------------------------------------------------
	//	int	m_Building;
	//	int	m_Sector;
	//	IVisArea *m_Sector;

	// for box areas ----------------------------------------------------------------------
	Vec3 m_BoxMin;
	Vec3 m_BoxMax;

	// for sphere areas ----------------------------------------------------------------------
	Vec3 m_SphereCenter;
	float	m_SphereRadius;
	float	m_SphereRadius2;

	bool m_bObstructRoof;
	bool m_bObstructFloor;

	//	area vertical origin - the lowest point of area
	float	m_VOrigin;
	//	area height (vertical size). If (m_VSize<=0) - not used, only 2D check is done. Otherwise 
	//	additional check for Z to be in [m_VOrigin, m_VOrigin + m_VSize] range is done
	float	m_VSize;

	int		m_stepID;
	bool	m_bIsActive;
	bool  m_bInitialized;
	bool  m_bHasSoundAttached;
	bool  m_bAttachedSoundTested; // can be replaced later with an OnAfterLoad
};

#endif //__Area_h__
