////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   AreaSphere.cpp
//  Version:     v1.00
//  Created:     25/10/2002 by Lennert.
//  Compilers:   Visual C++ 6.0
//  Description: CAreaSphere implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AreaSphere.h"
#include "ObjectManager.h"
#include "..\Viewport.h"
#include <IEntitySystem.h>

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CAreaSphere,CEntity)

int CAreaSphere::m_nRollupId=0;
CPickEntitiesPanel* CAreaSphere::m_pPanel=NULL;

//////////////////////////////////////////////////////////////////////////
CAreaSphere::CAreaSphere()
{
	m_areaId = -1;
	m_edgeWidth=0;
	m_radius = 3;
	mv_groupId = 0;
	mv_priority = 0;
	m_entityClass = "AreaSphere";
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::InitVariables()
{
	AddVariable( m_areaId,"AreaId",functor(*this,&CAreaSphere::OnAreaChange) );
	AddVariable( m_edgeWidth,"FadeInZone",functor(*this,&CAreaSphere::OnSizeChange) );
	AddVariable( m_radius,"Radius",functor(*this,&CAreaSphere::OnSizeChange) );
	AddVariable( mv_groupId,"GroupId",functor(*this,&CAreaSphere::OnAreaChange) );
	AddVariable( mv_priority,"Priority",functor(*this,&CAreaSphere::OnAreaChange) );
	AddVariable( mv_filled,"Filled",functor(*this,&CAreaSphere::OnAreaChange) );
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::Done()
{
	__super::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CAreaSphere::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	bool res = __super::Init( ie,prev,file );
	SetColor( RGB(0,0,255) );

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CAreaSphere::CreateGameObject()
{
	bool bRes = __super::CreateGameObject();
	if (bRes)
	{
		UpdateGameArea();
	}
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::GetLocalBounds( BBox &box )
{
	box.min = Vec3(-m_radius, -m_radius, -m_radius);
	box.max = Vec3(m_radius, m_radius, m_radius);
}

void CAreaSphere::SetAngles( const Ang3& angles )
{
	// Ignore angles on CAreaBox.
}
	
void CAreaSphere::SetScale( const Vec3 &scale )
{
	// Ignore scale on CAreaBox.
}

//////////////////////////////////////////////////////////////////////////
bool CAreaSphere::HitTest( HitContext &hc )
{
	Vec3 origin = GetWorldPos();
	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross( w );
	float d = w.GetLength();

	Matrix34 invertWTM = GetWorldTM();
	Vec3 worldPos = invertWTM.GetTranslation();
	float epsilonDist = hc.view->GetScreenScaleFactor( worldPos ) * 0.01f;
	if ((d < m_radius + epsilonDist) &&
			(d > m_radius - epsilonDist))
	{
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::BeginEditParams( IEditor *ie,int flags )
{
	__super::BeginEditParams( ie,flags );
	if (!m_pPanel)
	{
		m_pPanel = new CPickEntitiesPanel;
		m_pPanel->Create( CPickEntitiesPanel::IDD,AfxGetMainWnd() );
		m_nRollupId = ie->AddRollUpPage( ROLLUP_OBJECTS,"Attached Entities",m_pPanel );
	}
	if (m_pPanel)
		m_pPanel->SetOwner(this);
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::EndEditParams( IEditor *ie )
{
	if (m_nRollupId)
		ie->RemoveRollUpPage( ROLLUP_OBJECTS, m_nRollupId);
	m_nRollupId = 0;
	m_pPanel = NULL;
	__super::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::OnAreaChange(IVariable *pVar)
{
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::OnSizeChange(IVariable *pVar)
{
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::Display( DisplayContext &dc )
{
	COLORREF wireColor,solidColor;
	float wireOffset = 0;
	float alpha = 0.3f;
	if (IsSelected())
	{
		wireColor = dc.GetSelectedColor();
		solidColor = GetColor();
		wireOffset = -0.1f;
	}
	else
	{
		wireColor = GetColor();
		solidColor = GetColor();
	}
	

	const Matrix34 &tm = GetWorldTM();
	Vec3 pos = GetWorldPos();	
	
	bool bFrozen = IsFrozen();
	
	if (bFrozen)
		dc.SetFreezeColor();
	//////////////////////////////////////////////////////////////////////////
	if (!bFrozen)
		dc.SetColor( solidColor,alpha );

	if (IsSelected() || (bool)mv_filled)
	{
		dc.CullOff();
		dc.DepthWriteOff();
		//int rstate = dc.ClearStateFlag( GS_DEPTHWRITE );
		dc.DrawBall( pos, m_radius );
		//dc.SetState( rstate );
		dc.DepthWriteOn();
		dc.CullOn();
	}

	if (!bFrozen)
		dc.SetColor( wireColor,1 );
	dc.DrawWireSphere( pos, m_radius );
	if (m_edgeWidth)
		dc.DrawWireSphere( pos, m_radius-m_edgeWidth );
	//////////////////////////////////////////////////////////////////////////

	if (!m_entities.empty())
	{
		Vec3 vcol = Vec3(1, 1, 1);
		for (int i = 0; i < m_entities.size(); i++)
		{
			CBaseObject *obj = GetEntity(i);
			if (!obj)
				continue;
			dc.DrawLine(GetWorldPos(), obj->GetWorldPos(), ColorF(vcol.x,vcol.y,vcol.z,0.7f), ColorF(1,1,1,0.7f) );
		}
	}

	DrawDefault(dc);
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::Serialize( CObjectArchive &ar )
{
	__super::Serialize( ar );
	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		m_entities.clear();

		BBox box;
		// Load Entities.
		XmlNodeRef entities = xmlNode->findChild( "Entities" );
		if (entities)
		{
			for (int i = 0; i < entities->getChildCount(); i++)
			{
				XmlNodeRef ent = entities->getChild(i);
				GUID entityId;
				ent->getAttr( "Id",entityId );
				entityId = ar.ResolveID(entityId);
				m_entities.push_back(entityId);
			}
		}
		SetAreaId( m_areaId );
	}
	else
	{
		// Saving.
		// Save Entities.
		if (!m_entities.empty())
		{
			XmlNodeRef nodes = xmlNode->newChild( "Entities" );
			for (int i = 0; i < m_entities.size(); i++)
			{
				XmlNodeRef entNode = nodes->newChild( "Entity" );
				entNode->setAttr( "Id",m_entities[i] );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CAreaSphere::Export( const CString &levelPath,XmlNodeRef &xmlNode )
{
	XmlNodeRef objNode = __super::Export( levelPath,xmlNode );
	return objNode;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::SetAreaId(int nAreaId)
{
	m_areaId=nAreaId;
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
int CAreaSphere::GetAreaId()
{
	return m_areaId;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::SetRadius(float fRadius)
{
	m_radius=fRadius;
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
float CAreaSphere::GetRadius()
{
	return m_radius;
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::PostLoad( CObjectArchive &ar )
{
	// After loading Update game structure.
	UpdateGameArea();
}

//////////////////////////////////////////////////////////////////////////
void CAreaSphere::UpdateGameArea()
{
	if (!m_entity)
		return;

	IEntityAreaProxy *pArea = (IEntityAreaProxy*)m_entity->CreateProxy( ENTITY_PROXY_AREA );
	if (!pArea)
		return;

	pArea->SetSphere( Vec3(0,0,0),m_radius );
	pArea->SetProximity( m_edgeWidth );
	pArea->SetID( m_areaId );
	pArea->SetGroup( mv_groupId );
	pArea->SetPriority( mv_priority );

	pArea->ClearEntities();
	for (int i = 0; i < GetEntityCount(); i++)
	{
		CEntity *pEntity = GetEntity(i);
		if (pEntity && pEntity->GetIEntity())
			pArea->AddEntity( pEntity->GetIEntity()->GetId() );
	}
}