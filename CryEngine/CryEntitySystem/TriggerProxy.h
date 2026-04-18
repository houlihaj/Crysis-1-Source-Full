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

#ifndef __TriggerProxy_h__
#define __TriggerProxy_h__
#pragma once

#include "Entity.h"
#include "EntitySystem.h"

struct SProximityElement;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles sounds in the entity.
//////////////////////////////////////////////////////////////////////////
class CTriggerProxy : public IEntityTriggerProxy
{
public:
	CTriggerProxy( CEntity *pEntity );
	~CTriggerProxy();

	CEntity* GetEntity() const { return m_pEntity; };

	//////////////////////////////////////////////////////////////////////////
	// IEntityProxy interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetType() { return ENTITY_PROXY_TRIGGER; }
	virtual void Release();
	virtual void Done() {};
	virtual	void Update( SEntityUpdateContext &ctx );
	virtual	void ProcessEvent( SEntityEvent &event );
	virtual void Init( IEntity *pEntity,SEntitySpawnParams &params ) {};
	virtual void SerializeXML( XmlNodeRef &entityNode,bool bLoading ) {};
	virtual void Serialize( TSerialize ser );
	virtual bool NeedSerialize();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityTriggerProxy
	//////////////////////////////////////////////////////////////////////////
	virtual void SetTriggerBounds( const AABB &bbox ) { SetAABB(bbox); };
	virtual void GetTriggerBounds( AABB &bbox ) { bbox = m_aabb; };
	virtual void ForwardEventsTo( EntityId id ) { m_forwardingEntity = id; }
	virtual void InvalidateTrigger();
	//////////////////////////////////////////////////////////////////////////

	const AABB& GetAABB() const { return m_aabb; }
	void SetAABB( const AABB &aabb );

	CProximityTriggerSystem* GetTriggerSystem() { return m_pEntity->GetCEntitySystem()->GetProximityTriggerSystem(); }

private:
	void OnMove(bool invalidateAABB=false);

private:
	//////////////////////////////////////////////////////////////////////////
	// Private member variables.
	//////////////////////////////////////////////////////////////////////////
	// Host entity.
	CEntity *m_pEntity;
	AABB m_aabb;
	SProximityElement *m_pProximityTrigger;
	EntityId m_forwardingEntity;
};

#endif // __TriggerProxy_h__
