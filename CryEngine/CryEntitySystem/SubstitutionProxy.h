////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   SubstitutionProxy.h
//  Version:     v1.00
//  Created:     7/6/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SubstitutionProxy_h__
#define __SubstitutionProxy_h__
#pragma once


//////////////////////////////////////////////////////////////////////////
// Description:
//    Implements base substitution proxy class for entity.
//////////////////////////////////////////////////////////////////////////
struct CSubstitutionProxy : IEntitySubstitutionProxy
{
	CSubstitutionProxy( CEntity *pEntity ) { m_pSubstitute = 0; }
	~CSubstitutionProxy() {};

	//////////////////////////////////////////////////////////////////////////
	// IEntityProxy interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetType() { return ENTITY_PROXY_SUBSTITUTION; }
	virtual void Release() {}
	virtual void Done();
	virtual	void Update( SEntityUpdateContext &ctx ) {}
	virtual	void ProcessEvent( SEntityEvent &event ) {}
	virtual void Init( IEntity *pEntity,SEntitySpawnParams &params ) {}
	virtual void SerializeXML( XmlNodeRef &entityNode,bool bLoading ) {}
	virtual void Serialize( TSerialize ser );
	virtual bool NeedSerialize();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntitySubstitutionProxy interface.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetSubstitute(IRenderNode *pSubstitute);
	virtual IRenderNode *GetSubstitute() { return m_pSubstitute; }
	//////////////////////////////////////////////////////////////////////////

protected:
	IRenderNode *m_pSubstitute;
};

#endif // __SubstitutionProxy_h__

