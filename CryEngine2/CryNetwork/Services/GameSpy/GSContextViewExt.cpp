#include "StdAfx.h"
#include "Context/ContextView.h"
#include "GSContextViewExt.h"
#include "Network.h"
#include "GSServerReport.h"
#include "GSNetworkProfile.h"
#include "GameSpy.h"
#include "GSStorage.h"

#include "GameSpy/gcdkey/gcdkeyc.h"
#include "GameSpy/gcdkey/gcdkeys.h"

const float CDKEY_CHECK_TIMEOUT = 60.0f;

#ifdef CRYSIS_BETA
	static const bool READ_PREORDER_FLAG = false;
#else
	static const bool READ_PREORDER_FLAG = true;
#endif


struct CGSCVExtension::SPreorderReader : public IStatsReader
{
	SPreorderReader(CGSCVExtension* p):m_ext(p),preorder(false){}
	virtual const char* GetTableName()
	{
		return "Unlocks";
	}
	virtual int         GetFieldsNum()
	{
		return 2;
	}
	virtual const char* GetFieldName(int i)
	{
		return i?"ownerid":"PreOrder";
	}
	virtual void        NextRecord(int id)
	{		
	}
	virtual void        OnValue(int field, const char* val)
	{		
	}
	virtual void        OnValue(int field, int val)
	{
		if(field == 0)
		{
			preorder = val!=0;
		}
	}
	virtual void        OnValue(int field, float val)
	{		
	}
	virtual void        End(bool ok)
	{
		if(ok)
		{
			if(m_ext)
			{
				m_ext->SetPreordered(preorder, false);
			}
		}
		if(m_ext)
			stl::find_and_erase(m_ext->m_requests,this);
		delete this;//we're done
	}
	bool						preorder;
	CGSCVExtension* m_ext;
};


CGSCVExtension::CGSCVExtension(bool server, CGameSpy* gs):
m_server(server),
m_service(gs),
m_parent(0),
m_playerId(0),
m_localId(0),
m_report(0),
m_cdkeyCheckWaiting(false),
m_profileId(0),
m_dead(false)
{
  m_report = static_cast<CGSServerReport*>(m_service->GetServerReport());
}

CGSCVExtension::~CGSCVExtension()
{
	for(int i=0;i<m_requests.size();++i)
	{
		m_requests[i]->m_ext = 0;
	}
}

void CGSCVExtension::SetParent(CContextView* cv)
{
  m_parent = cv;
	m_localId = cv->Parent()->GetLocalChannelID();
	if(m_server && m_report && m_report->IsAvailable())
		m_report->OnClientConnected(this);
}

void CGSCVExtension::OnWitnessDeclared(EntityId plr)
{
	NetLog("OnWitnessDeclared for channel %d, witness %d",m_localId,m_playerId);
  m_playerId = plr;
}

void CGSCVExtension::Die()
{
	if(m_dead)
		return;
	if(m_server)
	{
		if(m_report && m_report->IsAvailable())
			m_report->OnClientDisconnected(this);
	}
	m_service->UnregisterExtension(this);
	m_dead = true;
}

void CGSCVExtension::Release()
{
	if(!m_dead)
		Die();
	delete this;
}

void CGSCVExtension::DefineProtocol( IProtocolBuilder * pBuilder )
{
  pBuilder->AddMessageSink( this, GetProtocolDef(), GetProtocolDef() );
}

bool CGSCVExtension::EnterState( EContextViewState state )
{
  if(m_server)
  {
    switch(state)
    {
    case 	eCVS_Initial:
			break;
    case eCVS_Begin:
      //
      if(CVARS.RankedServer)
      {
        //check if we got the stuff
        if(m_profileId==-1)
        {
          m_parent->Parent()->Disconnect(eDC_NotLoggedIn, "You must be logged in to play on official servers");
          return false;
        }
      }
      break;
    }
  }
  else//client
  {
    switch(state)
    {
    case 	eCVS_Initial:
      {
        CGSNetworkProfile* prof = static_cast<CGSNetworkProfile*>(m_service->GetNetworkProfile());
        const SProfileInfo& pi = prof->GetProfileInfo();
        if(pi.m_complete)
        {
          SendProfileInfo(pi.m_profile,pi.m_uniquenick);
        }
      }
      break;
    case eCVS_Begin:
      break;
    }
  }
  return true;    
}

void CGSCVExtension::RequestCDKeyAuth(const char* challenge, bool reauth)
{
	NET_ASSERT(m_server);

	SCDKeyAuthMessage msg;
  m_challenge = challenge;
  msg.m_message = challenge;
  msg.m_type = reauth?eCMT_rechallenge:eCMT_challenge;
  CGSCVExtension::SendCDKeyCheckWith(msg,m_parent->Parent());
  m_cdkeyCheckSent = g_time;
  m_cdkeyCheckWaiting = true;
}

EntityId CGSCVExtension::GetPlayerId()const
{
  return m_playerId;
}

TNetChannelID CGSCVExtension::GetLocalId()const
{
	return m_localId;
}

bool CGSCVExtension::IsServer()const
{
  return m_server;
}

bool CGSCVExtension::IsCDKeyCheckTimedOut()const
{
  if(m_cdkeyCheckWaiting)
  {
    NET_ASSERT(m_server);
    if((g_time - m_cdkeyCheckSent).GetSeconds()>CDKEY_CHECK_TIMEOUT)
      return true;
  }
  return false;
}

void  CGSCVExtension::CancelCDKeyCheck()
{
  if(m_cdkeyCheckWaiting)
    m_cdkeyCheckWaiting = false;
}

void  CGSCVExtension::SendProfileInfo(int profileid, const char *nick)
{
  SProfileInfoMessage msg;
  msg.m_profileId = profileid;
  msg.m_nick = nick;
  CGSCVExtension::SendProfileInfoWith(msg,m_parent->Parent());
}

void  CGSCVExtension::SetPreordered(bool pr, bool fromcache)
{
	if(m_dead)
		return;
	if(!fromcache)//cache value
	{
		m_service->GetStorage()->CachePreorderFlag(m_profileId, pr);
	}
	NetLog("User %d is %spre-ordered", m_profileId, pr?"":"not ");
	if(m_parent && m_parent->Parent())
	{
		m_parent->Parent()->SetPreordered(pr);
		if(pr && m_server)
		{
			CGSCVExtension::SendPreorderStatusWith(SPreorderedStatusMessage(),m_parent->Parent());
		}
	}
}

void  CGSCVExtension::SendSessionId(const uint8 sessionId[SC_SESSION_GUID_SIZE])
{
	SSessionIdMessage msg;
	memcpy(msg.id,sessionId,sizeof(msg.id));
	CGSCVExtension::SendSessionIdWith(msg,m_parent->Parent());
}

void CGSCVExtension::SendConnectionId(const uint8 connectionId[SC_CONNECTION_GUID_SIZE])
{
	SConnectionIdMessage msg;	
	memcpy(msg.id,connectionId,sizeof(msg.id));
	CGSCVExtension::SendConnectionIdWith(msg,m_parent->Parent());
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CGSCVExtension, CDKeyCheck, eNRT_ReliableUnordered, 0)
{
	if(m_dead)
		return true;

  bool challenge = param.m_type == eCMT_challenge || param.m_type == eCMT_rechallenge;
  bool reauth = param.m_type == eCMT_reresponse || param.m_type == eCMT_rechallenge;

  NET_ASSERT(m_parent);
  NET_ASSERT(m_service);

  SCDKeyAuthMessage msg;

  if(challenge)
  {
    NET_ASSERT(!m_server);
    //client here
    char response[RESPONSE_SIZE];
    string key = CNetwork::Get()->GetCDKey();
    gcd_compute_response((char*)key.c_str(),(char*)param.m_message.c_str(),response,reauth?CDResponseMethod_REAUTH:CDResponseMethod_NEWAUTH);
    msg.m_message = response;
    msg.m_type = reauth?eCMT_reresponse:eCMT_response;
    CGSCVExtension::SendCDKeyCheckWith(msg,m_parent->Parent());
  }
  else
  {
    NET_ASSERT(m_server);
    m_cdkeyCheckWaiting = false;
    if(reauth)
      m_service->GetServerReport()->ReAuthPlayer(GetLocalId(),param.m_message);
    else
    {
      uint ip = 0;
      ushort port = 0;
      m_parent->Parent()->GetRemoteNetAddress(ip, port, false);
      m_service->GetServerReport()->AuthPlayer(GetLocalId(),ip,m_challenge,param.m_message);
    }
  }
  return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CGSCVExtension, ProfileInfo, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	if(m_dead)
		return true;

  NET_ASSERT(m_server);
  m_profileId = param.m_profileId;
	if(READ_PREORDER_FLAG)
	{
		CGSStorage* storage = m_service->GetStorage();
		if(storage)
		{
			bool pre;
			if(storage->CheckPreorderCache(m_profileId,pre))
			{
				SetPreordered(pre,true);
			}
			else
			{
				CGSNetworkProfile* prof = static_cast<CGSNetworkProfile*>(m_service->GetNetworkProfile());
				const SProfileInfo& pi = prof->GetProfileInfo();
				if(pi.m_complete)
				{
					SPreorderReader *rd = new SPreorderReader(this);
					storage->SetUser(pi.m_profile, pi.m_ticket);
					storage->SearchRecords(rd, m_profileId);
					m_requests.push_back(rd);
				}
			}
		}
	}
  m_parent->Parent()->SetProfileId(m_profileId);

	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CGSCVExtension, SessionId, eNRT_ReliableUnordered, 0)
{
	if(m_dead)
		return true;
	NET_ASSERT(!m_server);
	if(m_service)
	{
		CGSStatsTrack* pstats = m_service->GetCurrentStatsTrack();
		if(pstats)
			pstats->SetSessionId(param.id);
	}
	return true;
}


NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CGSCVExtension, ConnectionId, eNRT_ReliableUnordered, 0)
{
	if(m_dead)
		return true;
	NET_ASSERT(m_server);
	if(m_service)
	{
		CGSStatsTrack* pstats = m_service->GetCurrentStatsTrack();
		if(pstats)
			pstats->SetConnectionId(GetPlayerId(),param.id);
	}
	return true;
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CGSCVExtension, PreorderStatus, eNRT_ReliableUnordered, 0)
{
	if(m_dead)
		return true;
	NET_ASSERT(!m_server);
	if(m_parent && m_parent->Parent())
		m_parent->Parent()->SetPreordered(true);
	return true;
}