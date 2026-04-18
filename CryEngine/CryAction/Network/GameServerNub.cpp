/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 24:8:2004   10:54 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "GameServerNub.h"
#include "GameServerChannel.h"
#include "CryAction.h"
#include "GameContext.h"
#include "GameRulesSystem.h"


ICVar *CGameServerNub::sv_timeout_disconnect=0;


CGameServerNub::CGameServerNub()
: m_genId(0),
	m_pGameContext(0),
	m_maxPlayers(0)
{
	sv_timeout_disconnect=gEnv->pConsole->GetCVar("sv_timeout_disconnect");
	
	ICVar* pPass = gEnv->pConsole->GetCVar("sv_password");//password is set once per session
	if(pPass && gEnv->bMultiplayer)//protect only multiplayer context
		m_password = pPass->GetString();
}

//------------------------------------------------------------------------
void CGameServerNub::Release()
{
	delete this;
}
 
//------------------------------------------------------------------------
SCreateChannelResult CGameServerNub::CreateChannel(INetChannel *pChannel, const char *pRequest)
{
	assert(m_maxPlayers);

	CGameServerChannel *pNewChannel;

	if (!pRequest)
	{
		assert(false);
		SCreateChannelResult res(eDC_GameError);
		return res;
	}

	SParsedConnectionInfo info = m_pGameContext->ParseConnectionInfo(pRequest);
	if (!info.allowConnect)
	{
		GameWarning( "Not allowed to connect to server: %s", info.errmsg.c_str() );
		SCreateChannelResult res(info.cause);
		strcpy_s(res.errorMsg,sizeof(res.errorMsg),info.errmsg.c_str());
		return res;
	}

	if (int(m_channels.size()) >= m_maxPlayers)
	{
		SCreateChannelResult res(eDC_ServerFull);
		strcpy_s(res.errorMsg,sizeof(res.errorMsg),string().Format("Disallowing more than %d players",m_maxPlayers).c_str());
		return res;
	}

	pNewChannel = GetOnHoldChannelFor(pChannel);
	if (!pNewChannel)
	{
		pNewChannel = new CGameServerChannel(pChannel, m_pGameContext, this);
		pNewChannel->SetChannelId(++m_genId);
		
		m_channels.insert(TServerChannelMap::value_type(pNewChannel->GetChannelId(), pNewChannel));
	}
	else
	{
		pNewChannel->SetNetChannel(pChannel);
		pNewChannel->SetupNetChannel(pChannel);
	}

	pChannel->SetPassword(m_password);
	pChannel->SetNickname(info.playerName.c_str());
	
	m_netchannels.insert(TNetServerChannelMap::value_type(pChannel, pNewChannel->GetChannelId()));

	return SCreateChannelResult(pNewChannel);
}


//------------------------------------------------------------------------
void CGameServerNub::Update()
{
	const int timeout=MAX(0, sv_timeout_disconnect->GetIVal());

	CTimeValue now=gEnv->pTimer->GetFrameStartTime();

	THoldChannelMap::iterator it=m_onhold.begin();
	THoldChannelMap::iterator end=m_onhold.end();

	for (;it!=end;)
	{
		THoldChannelMap::iterator next=it;
		++next;
		if ((now-it->second.time).GetSeconds()>=timeout)
			RemoveOnHoldChannel(GetChannel(it->second.channelId), false);
		it=next;
	}
}

void CGameServerNub::FailedActiveConnect(EDisconnectionCause cause, const char * description)
{
  assert(false && "Shouldn't be called from here");
}

//------------------------------------------------------------------------
CGameServerChannel *CGameServerNub::GetChannel(uint16 channelId)
{
	TServerChannelMap::iterator it = m_channels.find(channelId);

	if (it != m_channels.end())
		return it->second;
	return 0;
}

//------------------------------------------------------------------------
CGameServerChannel *CGameServerNub::GetChannel(INetChannel *pNetChannel)
{
	TNetServerChannelMap::iterator it = m_netchannels.find(pNetChannel);

	if (it != m_netchannels.end())
		return GetChannel(it->second);
	return 0;
}

//------------------------------------------------------------------------
uint16 CGameServerNub::GetChannelId(INetChannel *pNetChannel) const
{
	TNetServerChannelMap::const_iterator it = m_netchannels.find(pNetChannel);

	if (it != m_netchannels.end())
		return it->second;
	return 0;
}
//------------------------------------------------------------------------
void CGameServerNub::RemoveChannel(uint16 channelId)
{
	TServerChannelMap::iterator it = m_channels.find(channelId);

	if (it != m_channels.end())
	{
		m_channels.erase(it);

		for (TNetServerChannelMap::iterator it=m_netchannels.begin(); it!=m_netchannels.end(); ++it)
		{
			if (it->second == channelId)
			{
				m_netchannels.erase(it);
				break;
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameServerNub::SetMaxPlayers( int maxPlayers )
{
	assert(maxPlayers);
	m_maxPlayers = maxPlayers;
	
	if (m_maxPlayers < int(m_channels.size()))
	{
		int kick = int(m_channels.size()) - m_maxPlayers;
		GameWarning( "Kicking %d players due to maximum player count reduction",
			kick );
		for (TServerChannelMap::const_reverse_iterator it = m_channels.rbegin();
			kick != 0; ++it)
		{
			it->second->GetNetChannel()->Disconnect( eDC_ServerFull, "" );
			kick --;
		}
	}
}

//------------------------------------------------------------------------
bool CGameServerNub::IsPreordered(uint16 channelId)
{
  for(TNetServerChannelMap::const_iterator nit = m_netchannels.begin();nit!=m_netchannels.end();++nit)
    if(nit->second == channelId)
    {
      return nit->first->IsPreordered();
    }
  
  return false;
}

//------------------------------------------------------------------------
bool CGameServerNub::PutChannelOnHold(CGameServerChannel *pServerChannel)
{
	uint userId=pServerChannel->GetNetChannel()->GetProfileId();
	if (userId)
	{
		for (TNetServerChannelMap::iterator it=m_netchannels.begin(); it!=m_netchannels.end(); ++it)
		{
			if (it->second == pServerChannel->GetChannelId())
			{
				m_netchannels.erase(it);
				break;
			}
		}

		m_onhold.insert(THoldChannelMap::value_type(userId, SHoldChannel(pServerChannel->GetChannelId(), gEnv->pTimer->GetFrameStartTime())));

		pServerChannel->SetNetChannel(0);
		pServerChannel->SetOnHold(true);

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CGameServerNub::RemoveOnHoldChannel(CGameServerChannel *pServerChannel, bool renewed)
{
	for (THoldChannelMap::iterator it=m_onhold.begin(); it!=m_onhold.end(); ++it)
	{
		if (it->second.channelId == pServerChannel->GetChannelId())
		{
			m_onhold.erase(it);
			if(!renewed)//let gamerules know we should get rid of it...
			{
				IGameRules *pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
				pGameRules->OnClientDisconnect(pServerChannel->GetChannelId(),eDC_Timeout,"OnHold timed out",false);
			}				
			break;
		}
	}

	if(!renewed)
	{
		//
		TServerChannelMap::iterator ch_it = m_channels.find(pServerChannel->GetChannelId());
		if(ch_it!=m_channels.end())
		{
			m_channels.erase(ch_it);
		}

		delete pServerChannel;
	}	
}

//------------------------------------------------------------------------
void CGameServerNub::ResetOnHoldChannels()
{
	for (THoldChannelMap::iterator it=m_onhold.begin(); it!=m_onhold.end(); ++it)
	{
		IGameRules *pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
		pGameRules->OnClientDisconnect(it->first,eDC_Timeout,"OnHold reseted",false);
		TServerChannelMap::iterator ch_it = m_channels.find(it->first);
		if(ch_it!=m_channels.end())
		{
			delete ch_it->second;
			m_channels.erase(ch_it);
		}
	}
	m_onhold.clear();
}

//------------------------------------------------------------------------
CGameServerChannel *CGameServerNub::GetOnHoldChannelFor(INetChannel *pNetChannel)
{
	int userId=pNetChannel->GetProfileId();
	if (userId)
	{
		THoldChannelMap::iterator it=m_onhold.find(userId);
		if (it!=m_onhold.end())
		{
			CGameServerChannel *pChannel = GetChannel(it->second.channelId);
			RemoveOnHoldChannel(pChannel, true);
			return pChannel;
		}
	}

	return 0;
}


void CGameServerNub::BanPlayer(uint16 channelId, const char* reason)
{
	int userId = 0;
	int ip = 0;
	for(TNetServerChannelMap::const_iterator nit = m_netchannels.begin();nit!=m_netchannels.end();++nit)
		if(nit->second == channelId)
		{
			userId = nit->first->GetProfileId();
			nit->first->Disconnect(eDC_Kicked, reason);
			break;
		}
	int timeout = 30;
	if(ICVar* pV = gEnv->pConsole->GetCVar("ban_timeout"))
		timeout = pV->GetIVal();
	if(userId)
	{
		m_banned.push_back(SBannedPlayer(userId,0,gEnv->pTimer->GetFrameStartTime()+CTimeValue(timeout*60.0f)));
	}
	else if(ip)
	{
		m_banned.push_back(SBannedPlayer(0,ip,gEnv->pTimer->GetFrameStartTime()+CTimeValue(timeout*60.0f)));
	}
}

bool CGameServerNub::CheckBanned(INetChannel* pNetChannel)
{
	int userId = pNetChannel->GetProfileId();
	if(!userId)
		return false;
	for(int i=0;i<m_banned.size();++i)
	{
		if(m_banned[i].profileId == userId)
		{
			if(m_banned[i].time < gEnv->pTimer->GetFrameStartTime()) // ban timed out
			{
				m_banned.erase(m_banned.begin()+i);
				return false;
			}
			else
			{
				pNetChannel->Disconnect(eDC_Banned, "You're banned from server.");
				return true;
			}
		}
	}
	return false;
}

void CGameServerNub::BannedStatus()
{
	CryLogAlways("-----------------------------------------");
	CryLogAlways("Banned players : ");
	for(int i=0;i<m_banned.size();++i)
	{
		if(m_banned[i].time < gEnv->pTimer->GetFrameStartTime())
		{
			m_banned.erase(m_banned.begin()+i);
			--i;
		}
		else
		{
			int left = int((m_banned[i].time - gEnv->pTimer->GetFrameStartTime()).GetSeconds()+0.5f);
			CryLogAlways("profile : %d, time left : %d:%02d",m_banned[i].profileId,left/60,left%60);
		}
	}
	CryLogAlways("-----------------------------------------");
}

void CGameServerNub::UnbanPlayer(int profileId)
{
	for(int i=0;i<m_banned.size();++i)
	{
		if(m_banned[i].profileId == profileId)
		{
			m_banned.erase(m_banned.begin()+i);
			break;
		}
	}
}

//------------------------------------------------------------------------
void CGameServerNub::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_channels);
	s->AddContainer(m_netchannels);
	for (TServerChannelMap::iterator iter = m_channels.begin(); iter != m_channels.end(); ++iter)
		iter->second->GetMemoryStatistics(s);
}
