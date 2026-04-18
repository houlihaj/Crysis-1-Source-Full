////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   EntityClass.h
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __EntityClass_h__
#define __EntityClass_h__
#pragma once

#include <IEntityClass.h>

//////////////////////////////////////////////////////////////////////////
// Description:
//    Standard implementation of the IEntityClass interface.
//////////////////////////////////////////////////////////////////////////
class CEntityClass : public IEntityClass
{
public:
	CEntityClass();
	~CEntityClass();
	
	//////////////////////////////////////////////////////////////////////////
	// IEntityClass interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void Release() { delete this; };

	virtual uint32 GetFlags() const { return m_nFlags; };
	virtual void SetFlags( uint32 nFlags ) { m_nFlags = nFlags; };

	virtual const char* GetName() const { return m_sName.c_str(); }
	virtual const char* GetScriptFile() const { return m_sScriptFile.c_str(); }
	virtual IEntityScript* GetIEntityScript() const { return m_pEntityScript; }
	virtual bool LoadScript( bool bForceReload );
	virtual UserProxyCreateFunc GetUserProxyCreateFunc() const { return m_pfnUserProxyCreate; };
	virtual void* GetUserProxyData() const { return m_pfnUserData; };
	virtual void SetUserProxyCreateFunc( UserProxyCreateFunc pFunc,void *pUserData=NULL ) { m_pfnUserProxyCreate = pFunc; m_pfnUserData=pUserData; };
	virtual int GetEventCount();
	virtual SEventInfo GetEventInfo( int nIndex );
	virtual bool FindEventInfo( const char *sEvent,SEventInfo &event );
	//////////////////////////////////////////////////////////////////////////

	void SetName( const char *sName ) { m_sName = sName; };
	void SetScriptFile( const char *sScriptFile ) { m_sScriptFile = sScriptFile; };
	void SetEntityScript( IEntityScript *pScript ) { m_pEntityScript = pScript; };

private:
	uint32 m_nFlags;
	string m_sName;
	string m_sScriptFile;
	IEntityScript *m_pEntityScript;
	UserProxyCreateFunc m_pfnUserProxyCreate;
	void*  m_pfnUserData;
	bool m_bScriptLoaded;
};

#endif // __EntityClass_h__