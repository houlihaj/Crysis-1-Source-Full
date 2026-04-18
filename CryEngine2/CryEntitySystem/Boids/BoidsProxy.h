////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   BoidsProxy.h
//  Version:     v1.00
//  Created:     2/10/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __BoidsProxy_h__
#define __BoidsProxy_h__
#pragma once

#include <IEntityProxy.h>

class CFlock;
class CBoidObject;
class CEntity;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles sounds in the entity.
//////////////////////////////////////////////////////////////////////////
struct CBoidsProxy : public IEntityBoidsProxy
{
	CBoidsProxy( CEntity *pEntity );
	~CBoidsProxy();
	CEntity* GetEntity() const { return m_pEntity; };

	//////////////////////////////////////////////////////////////////////////
	// IEntityProxy interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetType() { return ENTITY_PROXY_BOIDS; }
	virtual void Release();
	virtual void Done() {};
	virtual	void Update( SEntityUpdateContext &ctx );
	virtual	void ProcessEvent( SEntityEvent &event );
	virtual void Init( IEntity *pEntity,SEntitySpawnParams &params ) {};
	virtual void SerializeXML( XmlNodeRef &entityNode,bool bLoading ) {};
	virtual void Serialize( TSerialize ser );
	virtual bool NeedSerialize() { return false; };
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	void SetFlock( CFlock *pFlock );
	CFlock* GetFlock() { return m_pFlock; }
	void OnTrigger( bool bEnter,SEntityEvent &event );

private:
	void OnMove();

private:
	//////////////////////////////////////////////////////////////////////////
	// Private member variables.
	//////////////////////////////////////////////////////////////////////////
	// Host entity.
	CEntity *m_pEntity;

	// Flock of items.
	CFlock *m_pFlock;

	int m_playersInCount;
};

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles sounds in the entity.
//////////////////////////////////////////////////////////////////////////
struct CBoidObjectProxy : public IEntityProxy
{
	CBoidObjectProxy( CEntity *pEntity ) { m_pBoid = 0; m_pEntity = pEntity; };
	~CBoidObjectProxy() {};
	CEntity* GetEntity() const { return m_pEntity; };

	//////////////////////////////////////////////////////////////////////////
	// IEntityProxy interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetType() { return ENTITY_PROXY_BOID_OBJECT; }
	virtual void Release() { delete this; };
	virtual void Done() {};
	virtual	void Update( SEntityUpdateContext &ctx ){};
	virtual	void ProcessEvent( SEntityEvent &event );
	virtual void Init( IEntity *pEntity,SEntitySpawnParams &params ) {};
	virtual void SerializeXML( XmlNodeRef &entityNode,bool bLoading ) {};
	virtual void Serialize( TSerialize ser );
	virtual bool NeedSerialize() { return false; };
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	void SetBoid( CBoidObject *pBoid ) { m_pBoid = pBoid; };
	CBoidObject* GetBoid() { return m_pBoid; }

private:
	//////////////////////////////////////////////////////////////////////////
	// Private member variables.
	//////////////////////////////////////////////////////////////////////////
	// Host entity.
	CEntity *m_pEntity;
	// Host Flock.
	CBoidObject *m_pBoid;
};

#endif //__BoidsProxy_h__
