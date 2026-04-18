
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   AreaProxy.h
//  Version:     v1.00
//  Created:     27/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AreaProxy_h__
#define __AreaProxy_h__
#pragma once

#include "Area.h"

// forward declarations.
struct SEntityEvent;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles sounds in the entity.
//////////////////////////////////////////////////////////////////////////
struct CAreaProxy : public IEntityAreaProxy
{
	CAreaProxy( CEntity *pEntity );
	CEntity* GetEntity() const { return m_pEntity; };

	//////////////////////////////////////////////////////////////////////////
	// IEntityProxy interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetType() { return ENTITY_PROXY_AREA; }
	virtual void Release();
	virtual void Done() {};
	virtual	void Update( SEntityUpdateContext &ctx ) {}
	virtual	void ProcessEvent( SEntityEvent &event );
	virtual void Init( IEntity *pEntity,SEntitySpawnParams &params ) {};
	virtual void SerializeXML( XmlNodeRef &entityNode,bool bLoading );
	virtual void Serialize( TSerialize ser );
	virtual bool NeedSerialize() { return false; };
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityAreaProxy interface.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetFlags( int nAreaProxyFlags ) { m_nFlags = nAreaProxyFlags; };
	virtual int  GetFlags() { return m_nFlags; } 

	virtual EEntityAreaType GetAreaType() const { return m_pArea->GetAreaType(); };

	virtual void	SetPoints( const Vec3* const vPoints, int count,float fHeight );
	virtual void	SetBox( const Vec3& min,const Vec3& max );
	virtual void	SetSphere( const Vec3& center,float fRadius );

	virtual int   GetPointsCount() { return m_localPoints.size(); };
	virtual const Vec3* GetPoints();
	virtual float GetHeight() { return m_pArea->GetHeight(); };
	virtual void	GetBox( Vec3& min,Vec3& max ) { m_pArea->GetBox(min,max); }
	virtual void	GetSphere( Vec3& vCenter,float &fRadius ) { m_pArea->GetSphere(vCenter,fRadius); };
	virtual float GetSize() { return m_pArea->GetAreaSize(); };

	virtual void SetGravityVolume(const Vec3 * pPoints, int nNumPoints, float fRadius, float fGravity, bool bDontDisableInvisible, float fFalloff, float fDamping);

	virtual void	SetID( const int id ) { m_pArea->SetID(id); };
	virtual int		GetID() const  { return m_pArea->GetID(); };
	virtual void	SetGroup( const int id) { m_pArea->SetGroup(id); };
	virtual int		GetGroup( ) const { return m_pArea->GetGroup(); };
	virtual void	SetPriority( const int nPriority) { m_pArea->SetPriority(nPriority); };
	virtual int		GetPriority( ) const { return m_pArea->GetPriority(); };
	virtual void	SetObstruction( bool bRoof, bool bFloor ) { return m_pArea->SetObstruction(bRoof, bFloor); };

	virtual void	AddEntity( EntityId id ) { m_pArea->AddEntity(id); };
	virtual void	ClearEntities() { m_pArea->ClearEntities(); };

	virtual void	SetProximity( float prx ) { m_pArea->SetProximity(prx); };
	virtual float	GetProximity() { return m_pArea->GetProximity(); };

	virtual float ClosestPointOnHullDistSq(const Vec3 &Point3d, Vec3 &OnHull3d) {return m_pArea->ClosestPointOnHullDistSq(Point3d, OnHull3d); };
	virtual float PointNearDistSq(const Vec3 &Point3d, Vec3 &OnHull3d) {return m_pArea->PointNearDistSq(Point3d, OnHull3d); };
	virtual bool	IsPointWithin(const Vec3& Point3d, bool bIgnoreHeight=false) const { return m_pArea->IsPointWithin(Point3d, bIgnoreHeight); }
	
private:
	void OnMove();

	void OnEnable(bool bIsEnable, bool bIsCallScript = true);

private:
	//////////////////////////////////////////////////////////////////////////
	// Private member variables.
	//////////////////////////////////////////////////////////////////////////
	// Host entity.
	CEntity *m_pEntity;

	int m_nFlags;
	// Managed area.
	CArea *m_pArea;
	std::vector<Vec3> m_localPoints;
	Vec3 m_vCenter;
	float m_fRadius;
	float m_fGravity;
	float m_fFalloff;
	float m_fDamping;
	float m_bDontDisableInvisible;

	pe_params_area m_gravityParams;

	std::vector<Vec3> m_bezierPoints;
	std::vector<Vec3> m_bezierPointsTmp;
	SEntityPhysicalizeParams::AreaDefinition m_areaDefinition;
	bool m_bIsEnable;
	bool m_bIsEnableInternal;
	float m_lastFrameTime;
};

#endif //__AreaProxy_h__
