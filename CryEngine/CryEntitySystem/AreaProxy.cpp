////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   AreaProxy.cpp
//  Version:     v1.00
//  Created:     27/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AreaProxy.h"
#include "Entity.h"
#include "ISerialize.h"

//////////////////////////////////////////////////////////////////////////
CAreaProxy::CAreaProxy( CEntity *pEntity )
{
	m_nFlags = 0;
	m_pEntity = pEntity;
	
	m_pArea = pEntity->GetCEntitySystem()->GetAreaManager()->CreateArea();
	m_pArea->SetEntityID(pEntity->GetId());

	m_vCenter.Set(0,0,0);
	m_fRadius = 0;
	m_fGravity = 0;
	m_fFalloff = 0.8f;
	m_fDamping = 1.0f;
	m_bDontDisableInvisible = false;

	m_bIsEnable = true;
	m_bIsEnableInternal = true;
	m_lastFrameTime = 0.f;
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::Release()
{
	// Release Area.
	if (m_pArea)
		m_pArea->Release();
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::OnMove()
{
	if (!(m_nFlags & FLAG_NOT_UPDATE_AREA))
	{
		EEntityAreaType type = m_pArea->GetAreaType();
		if (type == ENTITY_AREA_TYPE_SHAPE)
		{
			static std::vector<Vec3> worldPoints;
			const Matrix34 &tm = m_pEntity->GetWorldTM();
			worldPoints.resize(m_localPoints.size());
			//////////////////////////////////////////////////////////////////////////
			for (unsigned int i = 0; i < m_localPoints.size(); i++)
			{
				worldPoints[i] = tm.TransformPoint(m_localPoints[i]);
			}

			if (!worldPoints.empty())
				m_pArea->SetPoints( &worldPoints[0],worldPoints.size() );
			else
				m_pArea->SetPoints( 0,0 );
		}
		else if (type == ENTITY_AREA_TYPE_BOX)
		{
			m_pArea->SetBoxMatrix( m_pEntity->GetWorldTM() );
		}
		else if (type == ENTITY_AREA_TYPE_SPHERE)
		{
			m_pArea->SetSphere( m_pEntity->GetWorldTM().TransformPoint(m_vCenter),m_fRadius );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::OnEnable(bool bIsEnable, bool bIsCallScript)
{
	m_bIsEnable = bIsEnable;
	if(m_pArea->GetAreaType()==ENTITY_AREA_TYPE_GRAVITYVOLUME)
	{
		SEntityPhysicalizeParams physparams;
		if(bIsEnable && m_bIsEnableInternal)
		{
			physparams.pAreaDef = &m_areaDefinition;
			m_areaDefinition.areaType = SEntityPhysicalizeParams::AreaDefinition::AREA_SPLINE;
			m_bezierPointsTmp.resize(m_bezierPoints.size());
			memcpy( &m_bezierPointsTmp[0],&m_bezierPoints[0],m_bezierPoints.size()*sizeof(Vec3) );
			m_areaDefinition.pPoints = &m_bezierPointsTmp[0];
			m_areaDefinition.nNumPoints = m_bezierPointsTmp.size();
			m_areaDefinition.fRadius = m_fRadius;
			m_gravityParams.gravity = Vec3(0,0,m_fGravity);
			m_gravityParams.falloff0 = m_fFalloff;
			m_gravityParams.damping = m_fDamping;
			physparams.type = PE_AREA;
			m_areaDefinition.pGravityParams = &m_gravityParams;

			m_pEntity->SetTimer(0, 11000);
		}
		m_pEntity->Physicalize(physparams);

		if(bIsCallScript)
		{
			//call the OnEnable function in the script, to set game flags for this entity and such.
			IScriptTable *pScriptTable = m_pEntity->GetScriptTable();
			if (pScriptTable)
			{
				HSCRIPTFUNCTION scriptFunc(NULL);	
				pScriptTable->GetValue("OnEnable", scriptFunc);

				if (scriptFunc)
					Script::Call(gEnv->pScriptSystem,scriptFunc,pScriptTable,bIsEnable);

				gEnv->pScriptSystem->ReleaseFunc(scriptFunc);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CAreaProxy::ProcessEvent( SEntityEvent &event )
{
	switch(event.event) {
	case ENTITY_EVENT_XFORM:
		OnMove();
		break;
	case ENTITY_EVENT_SCRIPT_EVENT:
		{
			const char * pName = (const char*)event.nParam[0];
			if(!stricmp(pName, "Enable"))
				OnEnable(true);
			else if(!stricmp(pName, "Disable"))
				OnEnable(false);
		}
		break;
	case ENTITY_EVENT_RENDER:
		{
			if(m_pArea->GetAreaType()==ENTITY_AREA_TYPE_GRAVITYVOLUME)
			{
				if(!m_bDontDisableInvisible)
				{
					m_lastFrameTime = gEnv->pTimer->GetCurrTime();
				}
				if(!m_bIsEnableInternal)
				{
					m_bIsEnableInternal = true;
					OnEnable(m_bIsEnable, false);
					m_pEntity->SetTimer(0, 11000);
				}
			}
		}
		break;
	case ENTITY_EVENT_TIMER:
		{
			if(m_pArea->GetAreaType()==ENTITY_AREA_TYPE_GRAVITYVOLUME)
			{
				if(!m_bDontDisableInvisible)
				{
					bool bOff=false;
					if(gEnv->pTimer->GetCurrTime() - m_lastFrameTime > 10.0f)
					{
						bOff=true;
						IEntityRenderProxy * pEntPr = (IEntityRenderProxy *)m_pEntity->GetProxy(ENTITY_PROXY_RENDER);
						if(pEntPr)
						{
							IRenderNode * pRendNode = pEntPr->GetRenderNode();
							if(pRendNode)
							{
								if(pEntPr->IsRenderProxyVisAreaVisible())
									bOff = false;
							}
						}
						if(bOff)
						{
							m_bIsEnableInternal = false;
							OnEnable(m_bIsEnable, false);
						}
					}
					if(!bOff)
						m_pEntity->SetTimer(0, 11000);
				}
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::SerializeXML( XmlNodeRef &entityNode,bool bLoading )
{
	if (m_nFlags & FLAG_NOT_SERIALIZE)
		return;

	if (bLoading)
	{
		XmlNodeRef areaNode = entityNode->findChild( "Area" );
		if (!areaNode)
			return;

		int nId=0,nGroup=0,nPriority=0;
		float fProximity = 0;
		float fHeight = 0;
		
		areaNode->getAttr( "Id",nId );
		areaNode->getAttr( "Group",nGroup );
		areaNode->getAttr( "Proximity",fProximity );
		areaNode->getAttr( "Priority",nPriority );
		m_pArea->SetID(nId);
		m_pArea->SetGroup(nGroup);
		m_pArea->SetProximity(fProximity);
		m_pArea->SetPriority(nPriority);

		XmlNodeRef pointsNode = areaNode->findChild( "Points" );
		if (pointsNode)
		{
			for (int i = 0; i < pointsNode->getChildCount(); i++)
			{
				XmlNodeRef pntNode = pointsNode->getChild(i);
				Vec3 pos;
				if (pntNode->getAttr( "Pos",pos ))
					m_localPoints.push_back(pos);
			}
			m_pArea->SetAreaType( ENTITY_AREA_TYPE_SHAPE );

			areaNode->getAttr( "Height",fHeight );
			m_pArea->SetHeight(fHeight);
			// Set points.
			OnMove();
		}
		else if (areaNode->getAttr("SphereRadius",m_fRadius))
		{
			// Sphere.
			areaNode->getAttr("SphereCenter",m_vCenter);
			m_pArea->SetSphere( m_pEntity->GetWorldTM().TransformPoint(m_vCenter),m_fRadius );
		}
		else if (areaNode->getAttr("VolumeRadius",m_fRadius))
		{
			areaNode->getAttr("Gravity",m_fGravity);
			areaNode->getAttr("DontDisableInvisible", m_bDontDisableInvisible);

			AABB box;
			box.Reset();

			// Bezier Volume.
			pointsNode = areaNode->findChild( "BezierPoints" );
			if (pointsNode)
			{
				for (int i = 0; i < pointsNode->getChildCount(); i++)
				{
					XmlNodeRef pntNode = pointsNode->getChild(i);
					Vec3 pt;
					if (pntNode->getAttr( "Pos",pt))
					{
						m_bezierPoints.push_back(pt);
						box.Add( pt );
					}
				}
			}
			m_pArea->SetAreaType( ENTITY_AREA_TYPE_GRAVITYVOLUME );
			if (!m_pEntity->GetRenderProxy())
			{
				IEntityRenderProxy * pRenderProxy = (IEntityRenderProxy*)m_pEntity->CreateProxy( ENTITY_PROXY_RENDER );
				m_pEntity->SetFlags(m_pEntity->GetFlags() | ENTITY_FLAG_SEND_RENDER_EVENT);

				if (box.min.x > box.max.x)
					box.min = box.max = Vec3(0,0,0);
				box.min-=Vec3(m_fRadius, m_fRadius, m_fRadius);
				box.max+=Vec3(m_fRadius, m_fRadius, m_fRadius);

				Matrix34 tm = m_pEntity->GetWorldTM_Fast();
				
				box.SetTransformedAABB( m_pEntity->GetWorldTM_Fast().GetInverted(),box );
				
				pRenderProxy->SetLocalBounds(box, true);
			}

			OnEnable(m_bIsEnable);
		}
		else
		{
			// Box.
			Vec3 bmin(0,0,0),bmax(0,0,0);
			areaNode->getAttr("BoxMin",bmin);
			areaNode->getAttr("BoxMax",bmax);
			m_pArea->SetBox( bmin,bmax,m_pEntity->GetWorldTM() );
		}

		m_pArea->ClearEntities();
		XmlNodeRef entitiesNode = areaNode->findChild( "Entities" );
		// Export Entities.
		if (entitiesNode)
		{
			for (int i = 0; i < entitiesNode->getChildCount(); i++)
			{
				XmlNodeRef entNode = entitiesNode->getChild(i);
				int entityId;
				if (entNode->getAttr( "Id",entityId ))
					m_pArea->AddEntity( entityId );
			}
		}
	}
	else
	{
		// Save points.
		XmlNodeRef areaNode = entityNode->newChild( "Area" );
		areaNode->setAttr( "Id",m_pArea->GetID() );
		areaNode->setAttr( "Group",m_pArea->GetGroup() );
		areaNode->setAttr( "Proximity",m_pArea->GetProximity() );
		areaNode->setAttr( "Priority", m_pArea->GetPriority() );
		EEntityAreaType type = m_pArea->GetAreaType();
		if (type == ENTITY_AREA_TYPE_SHAPE)
		{
			XmlNodeRef pointsNode = areaNode->newChild( "Points" );
			for (unsigned int i = 0; i < m_localPoints.size(); i++)
			{
				XmlNodeRef pntNode = pointsNode->newChild("Point");
				pntNode->setAttr( "Pos",m_localPoints[i] );
			}
			areaNode->setAttr( "Height",m_pArea->GetHeight() );
		}
		else if (type == ENTITY_AREA_TYPE_SPHERE)
		{
			// Box.
			areaNode->setAttr("SphereCenter",m_vCenter);
			areaNode->setAttr("SphereRadius",m_fRadius);
		}
		else if (type == ENTITY_AREA_TYPE_BOX)
		{
			// Box.
			Vec3 bmin,bmax;
			m_pArea->GetBox(bmin,bmax);
			areaNode->setAttr("BoxMin",bmin);
			areaNode->setAttr("BoxMax",bmax);
		}
		else if (type == ENTITY_AREA_TYPE_GRAVITYVOLUME)
		{
			areaNode->setAttr("VolumeRadius",m_fRadius);
			areaNode->setAttr("Gravity",m_fGravity);
			areaNode->setAttr("DontDisableInvisible", m_bDontDisableInvisible);
			XmlNodeRef pointsNode = areaNode->newChild( "BezierPoints" );
			for (unsigned int i = 0; i < m_bezierPoints.size(); i++)
			{
				XmlNodeRef pntNode = pointsNode->newChild("Point");
				pntNode->setAttr( "Pos",m_bezierPoints[i] );
			}
		}

		std::vector<EntityId> entIDs;
		m_pArea->GetEntites( entIDs );
		// Export Entities.
		if (!entIDs.empty())
		{
			XmlNodeRef nodes = areaNode->newChild( "Entities" );
			for (int i = 0; i < entIDs.size(); i++)
			{
				int entityId = entIDs[i];
				XmlNodeRef entNode = nodes->newChild( "Entity" );
				entNode->setAttr( "Id",entityId );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::Serialize( TSerialize ser )
{
	if (m_nFlags & FLAG_NOT_SERIALIZE)
		return;
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::SetPoints(const Vec3* const vPoints, int count,float fHeight )
{
	m_localPoints.resize(count);
	if (count > 0)
		memcpy( &m_localPoints[0],vPoints,count*sizeof(Vec3) );
	m_pArea->SetAreaType( ENTITY_AREA_TYPE_SHAPE );
	m_pArea->SetHeight(fHeight);
	OnMove();
}

//////////////////////////////////////////////////////////////////////////
const Vec3* CAreaProxy::GetPoints()
{
	if (m_localPoints.empty())
		return 0;
	return &m_localPoints[0];
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::SetBox( const Vec3& min,const Vec3& max )
{
	m_localPoints.clear();
	m_pArea->SetBox(min,max,m_pEntity->GetWorldTM());
	m_pArea->SetAreaType( ENTITY_AREA_TYPE_BOX );
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::SetSphere( const Vec3& vCenter,float fRadius )
{
	m_vCenter = vCenter;
	m_fRadius = fRadius;
	m_localPoints.clear();
	m_pArea->SetSphere( m_pEntity->GetWorldTM().TransformPoint(m_vCenter),fRadius );
	m_pArea->SetAreaType( ENTITY_AREA_TYPE_SPHERE );
}

//////////////////////////////////////////////////////////////////////////
void CAreaProxy::SetGravityVolume(const Vec3 * pPoints, int nNumPoints, float fRadius, float fGravity, bool bDontDisableInvisible, float fFalloff, float fDamping)
{
	m_bIsEnableInternal = true;
	m_fRadius = fRadius;
	m_fGravity = fGravity;
	m_fFalloff = fFalloff;
	m_fDamping = fDamping;
	m_bDontDisableInvisible = bDontDisableInvisible;

	m_bezierPoints.resize(nNumPoints);
	if (nNumPoints > 0)
		memcpy( &m_bezierPoints[0],pPoints,nNumPoints*sizeof(Vec3) );

	if(!bDontDisableInvisible)
		m_pEntity->SetTimer(0, 11000);

	m_pArea->SetAreaType( ENTITY_AREA_TYPE_GRAVITYVOLUME );
}
