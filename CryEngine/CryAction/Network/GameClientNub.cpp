/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 24:8:2004   10:58 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "GameClientNub.h"
#include "GameClientChannel.h"

CGameClientNub::~CGameClientNub()
{
}

void CGameClientNub::Release()
{
	// don't delete because it's not dynamically allocated
}

SCreateChannelResult CGameClientNub::CreateChannel(INetChannel *pChannel, const char *pRequest)
{
	if (pRequest)
	{
		GameWarning("CGameClientNub::CreateChannel: pRequest is non-null, it should not be");
		assert(false);
		SCreateChannelResult res(eDC_GameError);
		strcpy_s(res.errorMsg,sizeof(res.errorMsg),"CGameClientNub::CreateChannel: pRequest is non-null, it should not be");
		return res;
	}

	if (m_pClientChannel)
	{
		GameWarning("CGameClientNub::CreateChannel: m_pClientChannel is non-null, it should not be");
		assert(false);
		SCreateChannelResult res(eDC_GameError);
		strcpy_s(res.errorMsg,sizeof(res.errorMsg),"CGameClientNub::CreateChannel: m_pClientChannel is non-null, it should not be");
		return res;
	}

	m_pClientChannel = new CGameClientChannel(pChannel, m_pGameContext, this);

  ICVar* pPass = gEnv->pConsole->GetCVar("sv_password");
  if(pPass && gEnv->bMultiplayer)
    pChannel->SetPassword(pPass->GetString());

	return SCreateChannelResult(m_pClientChannel);
}

void CGameClientNub::FailedActiveConnect(EDisconnectionCause cause, const char * description)
{
	GameWarning("Failed connecting to server: %s", description);
  CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_connectFailed,int(cause),description));
}

void CGameClientNub::Disconnect( EDisconnectionCause cause, const char * msg )
{
	if (m_pClientChannel) 
		m_pClientChannel->GetNetChannel()->Disconnect( cause, msg );
}

void CGameClientNub::ClientChannelClosed()
{
	assert( m_pClientChannel );
	m_pClientChannel = NULL;
}

void CGameClientNub::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	if (m_pClientChannel)
		m_pClientChannel->GetMemoryStatistics(s);
}
