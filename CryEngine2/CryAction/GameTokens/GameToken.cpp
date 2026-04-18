////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   GameToken.cpp
//  Version:     v1.00
//  Created:     20/10/2005 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GameToken.h"
#include "GameTokenSystem.h"
#include "FlowSystem/FlowSerialize.h"

//////////////////////////////////////////////////////////////////////////
CGameTokenSystem* CGameToken::g_pGameTokenSystem = 0;

//////////////////////////////////////////////////////////////////////////
CGameToken::CGameToken()
{
	m_nFlags = 0;
}

//////////////////////////////////////////////////////////////////////////
CGameToken::~CGameToken()
{
	Notify(EGAMETOKEN_EVENT_DELETE);
}

//////////////////////////////////////////////////////////////////////////
void CGameToken::Notify( EGameTokenEvent event )
{
	// Notify all listeners about game token event.
	for (Listeneres::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		(*it)->OnGameTokenEvent(event,this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameToken::SetName( const char *sName )
{
	m_name = sName;
}

//////////////////////////////////////////////////////////////////////////
void CGameToken::SetType( EFlowDataTypes dataType )
{
	//@TODO: implement
	//m_value.SetType(dataType);
}

//////////////////////////////////////////////////////////////////////////
void CGameToken::SetValue( const TFlowInputData& val )
{
	if((0 == (m_nFlags&EGAME_TOKEN_MODIFIED)) || !IsEqualFlowInputData(val,m_value))
	{
		m_value = val;
		m_nFlags |= EGAME_TOKEN_MODIFIED;
		
		// debug
		if (g_pGameTokenSystem->sDebug && strnicmp(m_name.c_str(), "hud.", 4) != 0)
			GameWarning("CGameToken: %s=%s", m_name.c_str(), GetValueAsString());

		g_pGameTokenSystem->Notify( EGAMETOKEN_EVENT_CHANGE,this );
	}
	else if (g_pGameTokenSystem->sDebug && strnicmp(m_name.c_str(), "hud.", 4) != 0)
		GameWarning("CGameToken: %s=%s No change!", m_name.c_str(), GetValueAsString());
}

//////////////////////////////////////////////////////////////////////////
bool CGameToken::GetValue( TFlowInputData& val ) const
{
	val = m_value;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CGameToken::SetValueAsString( const char* sValue,bool bDefault )
{
	if(bDefault)
	{
		SetFromString( m_value,sValue );
	}
	else
	{
		// if(strcmp(sValue,GetValueAsString()))
		{
			SetFromString( m_value,sValue );
			m_nFlags |= EGAME_TOKEN_MODIFIED;
			g_pGameTokenSystem->Notify( EGAMETOKEN_EVENT_CHANGE,this );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
const char* CGameToken::GetValueAsString() const
{
	static string temp;
	temp = ConvertToString(m_value);
	return temp;
}

void CGameToken::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->Add(m_name);
	m_value.GetMemoryStatistics(s);
	s->AddContainer(m_listeners);
}