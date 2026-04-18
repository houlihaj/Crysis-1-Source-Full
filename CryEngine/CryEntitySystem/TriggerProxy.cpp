////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   TriggerProxy.h
//  Version:     v1.00
//  Created:     5/12/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TriggerProxy.h"
#include "ISerialize.h"
#include "ProximityTriggerSystem.h"

//////////////////////////////////////////////////////////////////////////
CTriggerProxy::CTriggerProxy( CEntity *pEntity )
{
	m_pEntity = pEntity;
	m_pEntity->m_bTrigger = true;
	m_pProximityTrigger = GetTriggerSystem()->CreateTrigger();
	m_pProximityTrigger->id = m_pEntity->GetId();

	// Release existing proximity entity if present, triggers should not trigger themself.
	if (m_pEntity->m_pProximityEntity)
	{
		GetTriggerSystem()->RemoveEntity( m_pEntity->m_pProximityEntity );
		m_pEntity->m_pProximityEntity = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
CTriggerProxy::~CTriggerProxy()
{
	GetTriggerSystem()->RemoveTrigger( m_pProximityTrigger );
}

//////////////////////////////////////////////////////////////////////////
void CTriggerProxy::Release()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CTriggerProxy::Update( SEntityUpdateContext &ctx )
{
}

//////////////////////////////////////////////////////////////////////////
void CTriggerProxy::ProcessEvent( SEntityEvent &event )
{
	switch (event.event) {
	case ENTITY_EVENT_XFORM:
		OnMove();
		break;
	case ENTITY_EVENT_ENTERAREA:
	case ENTITY_EVENT_LEAVEAREA:
		if (m_forwardingEntity)
			if (IEntity * pEntity = gEnv->pEntitySystem->GetEntity(m_forwardingEntity))
				pEntity->SendEvent( event );
		break;
	case ENTITY_EVENT_PRE_SERIALIZE:
		break;
	case ENTITY_EVENT_POST_SERIALIZE:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTriggerProxy::NeedSerialize()
{
	return true;
};

//////////////////////////////////////////////////////////////////////////
void CTriggerProxy::Serialize( TSerialize ser )
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		if (ser.BeginOptionalGroup("TriggerProxy",true))
		{
			ser.Value("BoxMin",m_aabb.min);
			ser.Value("BoxMax",m_aabb.max);
			ser.EndGroup();
		}
		
		if (ser.IsReading())
			OnMove();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTriggerProxy::OnMove(bool invalidateAABB)
{
	Vec3 pos = m_pEntity->GetWorldPos();
	AABB box = m_aabb;
	box.min += pos;
	box.max += pos;
	GetTriggerSystem()->MoveTrigger( m_pProximityTrigger, box, invalidateAABB );
}

//////////////////////////////////////////////////////////////////////////
void CTriggerProxy::InvalidateTrigger()
{
	OnMove(true);
}

//////////////////////////////////////////////////////////////////////////
void CTriggerProxy::SetAABB( const AABB &aabb )
{
	m_aabb = aabb;
	OnMove();
}
