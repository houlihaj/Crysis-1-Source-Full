////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   ScriptProxy.h
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ScriptProxy_h__
#define __ScriptProxy_h__
#pragma once

// forward declarations.
class  CEntityScript;
class  CEntity;
struct IScriptTable;
struct SScriptState;

#include "EntityScript.h"

//////////////////////////////////////////////////////////////////////////
// Description:
//    CScriptProxy object handles all the interaction of the entity with 
//    the entity script.
//////////////////////////////////////////////////////////////////////////
class CScriptProxy : public IEntityScriptProxy
{
public:
	CScriptProxy( CEntity *pEntity,CEntityScript *pScript,SEntitySpawnParams *pSpawnParams );
	~CScriptProxy();

	//////////////////////////////////////////////////////////////////////////
	// IEntityProxy implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetType() { return ENTITY_PROXY_SCRIPT; }
	virtual void Release() { delete this; };
	virtual void Init( IEntity *pEntity,SEntitySpawnParams &params );
	virtual void Done();
	virtual	void Update( SEntityUpdateContext &ctx );
	virtual	void ProcessEvent( SEntityEvent &event );
	virtual void SerializeXML( XmlNodeRef &entityNode,bool bLoading );
	virtual void Serialize( TSerialize ser );
	virtual bool NeedSerialize();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityScriptProxy implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetScriptUpdateRate( float fUpdateEveryNSeconds ) { m_fScriptUpdateRate = fUpdateEveryNSeconds; };
	virtual IScriptTable* GetScriptTable() { return m_pThis; };
	
	virtual void CallEvent( const char *sEvent );
	virtual void CallEvent( const char *sEvent,float fValue );
	virtual void CallEvent( const char *sEvent,bool bValue );
	virtual void CallEvent( const char *sEvent,const char *sValue );
	virtual void CallEvent( const char *sEvent,EntityId nEntityId );
	virtual void CallEvent( const char *sEvent,const Vec3 &vValue );

	virtual void SendScriptEvent( int Event, IScriptTable *pParamters, bool *pRet=NULL);
	virtual void SendScriptEvent( int Event, const char *str, bool *pRet=NULL );
	virtual void SendScriptEvent( int Event, int nParam, bool *pRet=NULL );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	virtual void OnStartAnimation(const char *sAnimation);
	virtual void OnEndAnimation(const char *sAnimation);
	//////////////////////////////////////////////////////////////////////////

	virtual void OnCollision(CEntity *pTarget, int matId, const Vec3 &pt, const Vec3 &n, const Vec3 &vel, const Vec3 &targetVel, int partId, float mass);

	//////////////////////////////////////////////////////////////////////////
	// State Management public interface.
	//////////////////////////////////////////////////////////////////////////
	virtual bool GotoState( const char *sStateName );
	virtual bool GotoStateId( int nState ) { return GotoState(nState); };
	bool GotoState( int nState );
	bool IsInState( const char *sStateName );
	bool IsInState( int nState );
	const char* GetState();
	int GetStateId();

	void SerializeProperties( TSerialize ser );

private:
	SScriptState* CurrentState() { return m_pScript->GetState(m_nCurrStateId); }
	void CreateScriptTable( SEntitySpawnParams *pSpawnParams );
	void SetEventTargets( XmlNodeRef &eventTargets );
	IScriptSystem* GetIScriptSystem() const { return gEnv->pScriptSystem; }

	void SerializeTable( TSerialize ser, const char * name );
	bool HaveTable( const char * name );

private:
	CEntity *m_pEntity;
	CEntityScript *m_pScript;
	IScriptTable* m_pThis;

	float m_fScriptUpdateTimer;
	float m_fScriptUpdateRate;

	// Cache Tables.
	SmartScriptTable m_hitTable;

	int m_nCurrStateId  : 8;
	int m_bUpdateScript : 1;
};

#endif // __ScriptProxy_h__