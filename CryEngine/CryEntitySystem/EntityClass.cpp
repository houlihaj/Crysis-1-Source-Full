////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   EntityClass.cpp
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "EntityClass.h"
#include "EntityScript.h"

//////////////////////////////////////////////////////////////////////////
CEntityClass::CEntityClass()
{
	m_pfnUserProxyCreate = NULL;
	m_pfnUserData = NULL;
	m_pEntityScript = NULL;
	m_bScriptLoaded = false;
}

//////////////////////////////////////////////////////////////////////////
CEntityClass::~CEntityClass()
{
	SAFE_RELEASE(m_pEntityScript);
}

//////////////////////////////////////////////////////////////////////////
bool CEntityClass::LoadScript( bool bForceReload )
{
	bool bRes = true;
	if (m_pEntityScript)
	{
		CEntityScript *pScript = (CEntityScript*)m_pEntityScript;
		bool bRes = pScript->LoadScript(bForceReload);

		m_bScriptLoaded = true;
	}
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
int CEntityClass::GetEventCount()
{
	if (!m_bScriptLoaded)
		LoadScript(false);
	if (!m_pEntityScript)
		return 0;
	return ((CEntityScript*)m_pEntityScript)->GetEventCount();
}

//////////////////////////////////////////////////////////////////////////
CEntityClass::SEventInfo CEntityClass::GetEventInfo( int nIndex )
{
	if (!m_bScriptLoaded)
		LoadScript(false);

	assert( nIndex >= 0 && nIndex < GetEventCount() );

	SEventInfo event;

	if (m_pEntityScript)
	{
		const SEntityScriptEvent &scriptEvent = ((CEntityScript*)m_pEntityScript)->GetEvent( nIndex );
		event.name = scriptEvent.name.c_str();
		event.bOutput = scriptEvent.bOutput;
		event.type = scriptEvent.valueType;
	}
	else
	{
		event.name = "";
		event.bOutput = false;
	}
	return event;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityClass::FindEventInfo( const char *sEvent,SEventInfo &event )
{
	if (!m_bScriptLoaded)
		LoadScript(false);

	if (!m_pEntityScript)
		return false;

	const SEntityScriptEvent *pScriptEvent = ((CEntityScript*)m_pEntityScript)->FindEvent( sEvent );
	if (!pScriptEvent)
		return false;

	event.name = pScriptEvent->name.c_str();
	event.bOutput = pScriptEvent->bOutput;
	event.type = pScriptEvent->valueType;

	return true;
}
