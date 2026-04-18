////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   RopeProxy.h
//  Version:     v1.00
//  Created:     23/10/2006 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __RopeProxy_h__
#define __RopeProxy_h__
#pragma once

// forward declarations.
struct SEntityEvent;
struct IPhysicalEntity;
struct IPhysicalWorld;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Implements rope proxy class for entity.
//////////////////////////////////////////////////////////////////////////
class CRopeProxy : public IEntityRopeProxy
{
public:
	CRopeProxy( CEntity *pEntity );
	~CRopeProxy() {};
	CEntity* GetEntity() const { return m_pEntity; };

	//////////////////////////////////////////////////////////////////////////
	// IEntityProxy interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetType() { return ENTITY_PROXY_ROPE; }
	virtual void Release();
	virtual void Done() {};
	virtual	void Update( SEntityUpdateContext &ctx );
	virtual	void ProcessEvent( SEntityEvent &event );
	virtual void Init( IEntity *pEntity,SEntitySpawnParams &params ) {};
	virtual void SerializeXML( XmlNodeRef &entityNode,bool bLoading );
	virtual bool NeedSerialize();
	virtual void Serialize( TSerialize ser );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	/// IEntityRopeProxy
	//////////////////////////////////////////////////////////////////////////
	virtual IRopeRenderNode* GetRopeRendeNode() { return m_pRopeRenderNode; };
	//////////////////////////////////////////////////////////////////////////

protected:
	CEntity *m_pEntity;

	IRopeRenderNode *m_pRopeRenderNode;
};

#endif // __RopeProxy_h__

