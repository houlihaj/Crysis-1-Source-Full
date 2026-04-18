/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2006.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  Gamespy ServerReport Interface impl
 -------------------------------------------------------------------------
 History:
 - 11/01/2006   : Stas Spivakov, Created
 
*************************************************************************/
#include "StdAfx.h"
#include "Network.h"
#include "Protocol/NetChannel.h"
#include "Protocol/NetNub.h"
#include "GSServerReport.h"
#include "GameSpy.h"
#include "GSContextViewExt.h"
#include "Context/ContextView.h"
#include "Cryptography/Whirlpool.h"

#include "GameSpy/gcdkey/gcdkeys.h"
#include "GameSpy/gcdkey/gcdkeyc.h"

static const float UPDATE_INTERVAL = 0.01f;
static const bool VERBOSE = true;


ILINE static uint32 GetRandomNumber()
{
#if USE_DEFENCE
	IDataProbe * pDataProbe = GetISystem()->GetIDataProbe();
	if (pDataProbe)
		return pDataProbe->GetRand();
	else
	{
#endif
//		NetWarning( "Using low quality random numbers: security compromised" );
		return (rand()<<16) | rand();
#if USE_DEFENCE
	}
#endif
}

//Other callbacks
static void QR2ClientMessageCallback(gsi_char* data, int len, void* obj);

static int g_GSServerReportCreated = 0;


CGSServerReport::CGSServerReport( CGameSpy* gs ):
m_parent(gs),
m_pListener(0),
m_netNub(0),
m_QR(0),
m_dead(false),
m_updateTimer(0),
m_reporting(false),
m_cdkeyChecking(false),
m_ip(0),
m_port(0)
{
	//TODO: Check here for some paranoid bs
	if(g_GSServerReportCreated)
	{
		NetError("GS ReportServer interface already exsists");
		m_dead = true;
	}
	else
	{
		InitStandartKeys();
	}
	g_GSServerReportCreated++;
	m_seed = GetRandomNumber();
}

CGSServerReport::~CGSServerReport()
{
	g_GSServerReportCreated--;
	NC_StopReport();
}

void	CGSServerReport::SetReportParams(int numplayers, int numteams)
{
	FROM_GAME(&CGSServerReport::NC_SetReportParams,this, numplayers, numteams);
}

void	CGSServerReport::AuthPlayer(int playerid, uint ip, const char* challenge, const char* responce)
{
	FROM_GAME(&CGSServerReport::NC_AuthPlayer, this, playerid, ip, string(challenge), string(responce));
}

void	CGSServerReport::ReAuthPlayer(int playerid, const char* responce)
{
	FROM_GAME(&CGSServerReport::NC_ReAuthPlayer, this, playerid, string(responce));
}

void	CGSServerReport::SetServerValue(const char* key, const char* value)
{
  if(!key || !*key)
    return;
	FROM_GAME(&CGSServerReport::NC_SetServerValue, this, TGSFixedString(key), TGSFixedString(value));
}

void	CGSServerReport::SetPlayerValue(int idx, const char* key, const char* value)
{
  if(!key || !*key)
    return;
	FROM_GAME(&CGSServerReport::NC_SetPlayerValue, this, idx, TGSFixedString(key), TGSFixedString(value));
}

void	CGSServerReport::SetTeamValue(int idx, const char* key, const char* value)
{
  if(!key || !*key)
    return;
	FROM_GAME(&CGSServerReport::NC_SetTeamValue, this, idx, TGSFixedString(key), TGSFixedString(value));
}

void	CGSServerReport::Update()
{
	FROM_GAME(&CGSServerReport::NC_Update,this);
}

void	CGSServerReport::StartReporting(INetNub* nub, IServerReportListener* listener)
{
  FROM_GAME(&CGSServerReport::NC_StartReport,this,nub,listener);
}

void CGSServerReport::TimerCallback(NetTimerId,void* obj,CTimeValue)
{
	CGSServerReport* pReport = static_cast<CGSServerReport*>(obj);
	
	if(!pReport->IsDead())
	{
		pReport->Think();
		SCOPED_GLOBAL_LOCK;
		pReport->m_updateTimer = TIMER.AddTimer( gEnv->pTimer->GetAsyncTime()+UPDATE_INTERVAL, TimerCallback, pReport );
	}
}

void	CGSServerReport::StopReporting()
{
	FROM_GAME(&CGSServerReport::NC_StopReport,this);
}

void CGSServerReport::OnClientConnected(CGSCVExtension* ext)
{
#if !ALWAYS_CHECK_CD_KEYS
	if(CVARS.CheckCDKeys != 0)
#endif
	{
		float fTime = gEnv->pTimer->GetCurrTime();
		uint32 nRand =	GetRandomNumber();
		char n1[32];
		char n2[32];
		sprintf(n1, "%f", fTime);
		sprintf(n2, "%.8x", nRand);
		string buffer = n1;
		buffer += n2;
		buffer = CWhirlpoolHash( buffer ).GetHumanReadable();
		if(buffer.size()>32)//getting only quarter of the string
			buffer = buffer.substr(0,32);
    ext->RequestCDKeyAuth(buffer, false);
	}
}

void CGSServerReport::OnClientDisconnected(CGSCVExtension* ext)
{
	int playerId = ext->GetLocalId();
	FROM_GAME(&CGSServerReport::NC_PlayerDisconnected, this, playerId);
	//PlayerDisconnected(ext->GetLocalId());
}


void CGSServerReport::ProcessQuery(char* data, int len, sockaddr_in* addr)
{
  if(m_QR)
    qr2_parse_query(m_QR,data,len,(sockaddr*)addr);
}

void	CGSServerReport::Think()
{
  if(m_dead)
    return;
	if(m_QR)
		qr2_think(m_QR);
	
  if(m_cdkeyChecking)
  {
		gcd_think();

    for(int i=0;i<m_parent->m_extensions.size();++i)
    {
      if(m_parent->m_extensions[i]->IsCDKeyCheckTimedOut())
      {
        m_parent->m_extensions[i]->CancelCDKeyCheck();
        m_netNub->OnCDKeyAuthResult(m_parent->m_extensions[i]->GetLocalId(),false,"CD Key check timeout");
      }
    }
  }
}

bool	CGSServerReport::IsDead()const
{
	return m_dead;
}

bool	CGSServerReport::IsAvailable()const
{
	return true;
}

void CGSServerReport::InitStandartKeys()
{
	for( int i = HOSTNAME_KEY; i< GAMESPY_SERVER_NUM_RESERVED_KEYS; i++ )
	{
    if(qr2_registered_key_list[i] && strlen(qr2_registered_key_list[i]))
		  m_keyIdMap[qr2_registered_key_list[i]] = i;
	}

	m_lastKey = GAMESPY_SERVER_NUM_RESERVED_KEYS;
}

void	CGSServerReport::UnregisterKeys()
{
	for(std::vector<char*>::iterator it = m_keyNames.begin(); it !=	m_keyNames.end(); ++it)
	{
		delete [] *it;
	}

	for(std::map<string,int>::iterator it = m_keyIdMap.begin(); it != m_keyIdMap.end();)
	{
		std::map<string,int>::iterator next = it;
		++next;
		if(it->second >= GAMESPY_SERVER_NUM_RESERVED_KEYS)
		{
			qr2_register_key(it->second,0);
			m_keyIdMap.erase(it);
		}
		it = next;
	}

	m_keyNames.resize(0);
}

int	CGSServerReport::GetKeyId(const string& k, EKeyType type)
{
	static string key;
	key = k;
	if(type == eKT_Player)
		key+="_";
	else if(type  == eKT_Team)
		key+="_t";
	
	std::map<string,int>::iterator it = m_keyIdMap.find(key);
	if(it!=m_keyIdMap.end())
	{
		return it->second;
	}
	else
	{
		int new_idx = m_lastKey++;
		m_keyIdMap[key] = new_idx;
		char* name = new char[key.size()+1];
		name[key.size()] = 0;
		memcpy(name,key.c_str(),key.size());
		qr2_register_key(new_idx,key);
		m_keyNames.push_back(name);
		if(VERBOSE)
			NetLog("GS Server Report : registered key %s", key.c_str());
		return new_idx;
	}
}

void	CGSServerReport::NC_SetServerValue(TGSFixedString key, TGSFixedString value)
{
	int	idx = GetKeyId(key,eKT_Server);
	if(idx!=-1)
		m_serverInfo.SetValue(idx,value);
}

void	CGSServerReport::NC_SetPlayerValue(int idx, TGSFixedString key, TGSFixedString value)
{
	int	key_id = GetKeyId(key,eKT_Player);
	if(idx < m_playersInfo.size())
	{
		m_playersInfo[idx].SetValue(key_id,value);
		m_playerKeys.SetValue(key_id,"");
	}
	else
	{
		NET_ASSERT(idx < m_playersInfo.size());
	}	
}

void	CGSServerReport::NC_SetTeamValue(int idx, TGSFixedString key, TGSFixedString value)
{
	int	key_id = GetKeyId(key,eKT_Team);
	if(idx < m_teamsInfo.size())
	{
		m_teamsInfo[idx].SetValue(key_id,value);
		m_teamKeys.SetValue(key_id,"");
	}
	else
	{
		NET_ASSERT(idx < m_teamsInfo.size());
	}
}

void	CGSServerReport::NC_SetReportParams(int numplayers, int numteams)
{
	m_playersInfo.resize(0);
	if(numplayers)
		m_playersInfo.resize(numplayers);
	m_teamsInfo.resize(0);
	if(numteams)
		m_teamsInfo.resize(numteams);
}

void  CGSServerReport::NC_StartReport(INetNub* net_nub, IServerReportListener* listener)
{
	if(m_reporting)
	{
		NetWarning("GS Server Report : StartReport called while report active, calling StopReporting...");
		NC_StopReport();
	}

  //set some server info
  NC_SetServerValue("official",CVARS.RankedServer!=0?"1":"0");
#ifdef __WITH_PB__
  NC_SetServerValue("anticheat",isPbSvEnabled()?"1":"0");
#endif

  NC_SetServerValue("voicecomm",CVARS.EnableVoiceChat!=0?"1":"0");
  NC_SetServerValue("dedicated",gEnv->pSystem->IsDedicated()?"1":"0");

	if ( ICVar * pCVar = gEnv->pConsole->GetCVar("sv_requireinputdevice") )
		NC_SetServerValue("gamepadsonly", strcmpi(pCVar->GetString(),"gamepad")==0?"1":"0");
	else
		NC_SetServerValue("gamepadsonly", "0");

	ICVar* pLAN = gEnv->pConsole->GetCVar("sv_lanonly");
	bool lan = false;
	if(pLAN && pLAN->GetIVal())
	{
		lan = true;
	}

  m_netNub = net_nub;
  m_pListener = listener;
  if(VERBOSE)
    NetLog("GS Server Report : StartReporting started");
  static char secret_code[7];//"zKbZiM"
  secret_code[0] = 'z';
  secret_code[1] = 'K';
  secret_code[2] = 'b';
  secret_code[3] = 'Z';
  secret_code[4] = 'i';
  secret_code[5] = 'M';
  secret_code[6] = 0;

  CNetNub* nub = (CNetNub*)m_netNub;
  int socket = nub->GetSysSocket();

  ushort port = 0;
  TNetAddressVec ips;
  nub->GetLocalIPs(ips);
  for(int i=0;i<ips.size();++i)
  {
    if(SIPv4Addr *addr = ips[i].GetPtr<SIPv4Addr>())
    {
      port = addr->port;
      break;
    }
  }

  qr2_error_t error = qr2_init_socket(&m_QR,socket,port,GAMESPY_GAME_NAME,secret_code,!lan,1,
    &QR2ServerKeyCallback,
    &QR2PlayerKeyCallback,
    &QR2TeamKeyCallback,
    &QR2KeyListCallback,
    &QR2CountCallback,
    &QR2ErrorCallback,
    this);

  if(error != e_qrnoerror)
  {
    NC_Error(error,"qr2_init_socket failed");
    return;
  }

  qr2_register_natneg_callback(m_QR,&QR2NatNegCallback);
  qr2_register_publicaddress_callback(m_QR,&QR2PublicAddresCallback);
  m_reporting = true;

  int cdkey_error = gcd_init_qr2(m_QR,CDKEY_PRODUCT_ID);
  if(cdkey_error == 0)
  {
		if(lan)
		{
			NetLog("GameSpy CDKey check : checks disabled for LAN server" );
		}
		else
		{
			m_cdkeyChecking = true;
		}
  }

  m_updateTimer = TIMER.AddTimer( gEnv->pTimer->GetAsyncTime()+UPDATE_INTERVAL, TimerCallback, this );
}

void	CGSServerReport::NC_StopReport()
{
	ICVar* pLAN = gEnv->pConsole->GetCVar("sv_lanonly");
  m_netNub = 0;
	if(m_cdkeyChecking || (pLAN && pLAN->GetIVal()))
	{
		gcd_disconnect_all(CDKEY_PRODUCT_ID);
		gcd_shutdown();
		m_cdkeyChecking = false;
	}
	if(m_reporting)
  {
    if(m_updateTimer!=0)
	  {
		  SCOPED_GLOBAL_LOCK;
		  TIMER.CancelTimer(m_updateTimer);
	  }
	  qr2_shutdown(m_QR);
	  m_QR = 0;

	  UnregisterKeys();
		m_reporting = false;
		m_serverInfo.Values.clear();
		m_playerKeys.Values.clear();
		m_teamKeys.Values.clear();
		m_playersInfo.resize(0);
	  m_teamsInfo.resize(0);
  }
  if(VERBOSE)
    NetLog("GS Server Report : StopReporting finished");
}

void	CGSServerReport::NC_Update()
{
	if(!m_reporting)
		return;

	qr2_send_statechanged(m_QR);
}

void	CGSServerReport::NC_AuthPlayer(int playerid, uint ip, string challenge, string response)
{
  if(!m_cdkeyChecking)
    return;
  uint userip = ip;
  if(userip==0)
    userip = m_ip;
	uint32 localId = playerid ^ m_seed;
  gcd_authenticate_user(CDKEY_PRODUCT_ID, localId, userip, challenge, response, GCDAuthorizeCallback, GCDReAuthorizeCallback, this);
  const char * k_hash = gcd_getkeyhash(CDKEY_PRODUCT_ID, localId);

  for(int i=0;i<m_parent->m_extensions.size();++i)
    if(m_parent->m_extensions[i]->GetLocalId() == playerid)
    {
      m_parent->m_extensions[i]->GetParent()->Parent()->SetCDKeyHash(k_hash);
      break;
    }
}

void	CGSServerReport::NC_ReAuthPlayer(int playerid, string response)
{
  if(!m_cdkeyChecking)
    return;
  int sid = 0;
  for(TReauthVector::iterator it = m_reathSId.begin(), eit = m_reathSId.end();it!= eit; ++ it)
	{
		if(it->first == playerid)
		{
			sid = it->second;
			m_reathSId.erase(it);
			break;
		}
	}
	if(sid)
	{
		NetLog("CDKey re-auth for %d with sid=%d processed", playerid, sid);
		uint32 localId = playerid ^ m_seed;
		gcd_process_reauth(CDKEY_PRODUCT_ID, localId, sid, response);
	}
}

void	CGSServerReport::NC_PlayerDisconnected(int plr)
{
  if(!m_cdkeyChecking)
    return;
#if !ALWAYS_CHECK_CD_KEYS
	if(CVARS.CheckCDKeys)
#endif
		NetLog("CDKey reported disconnected for %d", plr);

	uint32 localId = plr ^ m_seed;
  gcd_disconnect_user(CDKEY_PRODUCT_ID, localId);
}

void	CGSServerReport::GC_OnNatCookie(int cookie)
{
  if(m_netNub)
    m_netNub->OnNatCookieReceived(cookie);
}

void	CGSServerReport::GC_OnPublicIP(unsigned int ip,unsigned short port)
{
	if(!m_pListener)
        return;
  m_ip = ip;
  m_port = port;
  m_pListener->OnPublicIP(ip,port);	
}

void	CGSServerReport::NC_ReAuthRequest(int playerId, const char* challenge)
{
  NET_ASSERT(m_parent);
  for(int i=0;i<m_parent->m_extensions.size();++i)
	{
    if(m_parent->m_extensions[i]->GetLocalId() == playerId)
      m_parent->m_extensions[i]->RequestCDKeyAuth(challenge,true);
	}
}

void	CGSServerReport::GC_Error(qr2_error_t error)
{
	if(!m_pListener)
		return;
	switch(error)
	{
	case e_qrnoerror:
		m_pListener->OnError(eSRE_noerror);
		break;
	case e_qrwsockerror:
	case e_qrbinderror:
		m_pListener->OnError(eSRE_socket);
		break;
	case e_qrdnserror:
	case e_qrconnerror:
		m_pListener->OnError(eSRE_connect);
		break;
	case e_qrnochallengeerror:
		m_pListener->OnError(eSRE_noreply);
		break;
	default:
		m_pListener->OnError(eSRE_other);
	}
}

void  CGSServerReport::NC_Error(qr2_error_t error, const char* errmsg)
{
	if(VERBOSE)
	{
		switch(error)
		{
		case e_qrwsockerror:
			NetWarning("GS Server Report : (A standard socket call failed, e.g. exhausted resources. Error: %s)", errmsg);
			break;
		case e_qrbinderror:
			NetWarning("GS Server Report : (The SDK was unable to find an available port to bind on.) %s", errmsg);
			break;
		case e_qrdnserror:
			NetWarning("GS Server Report : (A DNS lookup (for the master server) failed.) %s", errmsg);
			break;
		case e_qrconnerror:
			NetWarning("GS Server Report : (The server is behind a NAT and does not support negotiation.) %s", errmsg);
			break;
		case e_qrnochallengeerror:
			NetWarning("GS Server Report : (No challenge was received from the master - either the master is down, or a firewall is blocking UDP.) %s", errmsg);
			break;
		}
	}
	TO_GAME(&CGSServerReport::GC_Error, this, error);
}

//Callbacks implementation

void CGSServerReport::QR2ErrorCallback(qr2_error_t error, gsi_char* errmsg, void* obj)
{
	CGSServerReport* pReport = static_cast<CGSServerReport*>(obj);
	pReport->NC_Error(error,errmsg);
}

int CGSServerReport::QR2CountCallback(qr2_key_type keytype, void* obj)
{
	CGSServerReport* pReport = static_cast<CGSServerReport*>(obj);
	switch(keytype)
	{
	case key_player:
		return pReport->m_playersInfo.size();
	case key_team:
		return pReport->m_teamsInfo.size();
	default:
		return 0;
	}
}

void CGSServerReport::QR2KeyListCallback(qr2_key_type keytype, qr2_keybuffer_t keybuffer, void* obj)
{
	CGSServerReport* pReport = static_cast<CGSServerReport*>(obj);
	switch(keytype)
	{
	case key_server:
		pReport->m_serverInfo.ListKeys(keybuffer);
		break;
	case key_player:
		if(!pReport->m_playersInfo.empty())
			pReport->m_playerKeys.ListKeys(keybuffer);
		break;
	case key_team:
		if(!pReport->m_teamsInfo.empty())
			pReport->m_teamKeys.ListKeys(keybuffer);
		break;
	default:
		break;
	}
}

void CGSServerReport::QR2ServerKeyCallback(int keyid, qr2_buffer_t outbuf, void* obj)
{
	CGSServerReport* pReport = static_cast<CGSServerReport*>(obj);
	string val;
	if(pReport->m_serverInfo.GetValue(keyid,val))
		qr2_buffer_add(outbuf,val.c_str());
}

void CGSServerReport::QR2PlayerKeyCallback(int keyid, int index, qr2_buffer_t outbuf, void* obj)
{
	CGSServerReport* pReport = static_cast<CGSServerReport*>(obj);
	if(index>=pReport->m_playersInfo.size())
	{
		if(VERBOSE)
			NetWarning("GS Server report : attempt to request value for out of bounds player");
		return;
	}
	string val;
	if(pReport->m_playersInfo[index].GetValue(keyid,val))
		qr2_buffer_add(outbuf, val.c_str());
	else
		qr2_buffer_add(outbuf, " ");
}

void CGSServerReport::QR2TeamKeyCallback(int keyid, int index, qr2_buffer_t outbuf, void* obj)
{
	CGSServerReport* pReport = static_cast<CGSServerReport*>(obj);
	if(index>=pReport->m_teamsInfo.size())
	{
		if(VERBOSE)
			NetWarning("GS Server report : attempt to request value for out of bounds team");
		return;
	}
	string val;
	if(pReport->m_teamsInfo[index].GetValue(keyid,val))
		qr2_buffer_add(outbuf,val.c_str());
//	else
//		qr2_buffer_add(outbuf,"");
}

void CGSServerReport::QR2NatNegCallback(int cookie, void* obj)
{
  CGSServerReport* pReport = static_cast<CGSServerReport*>(obj);
  TO_GAME(&CGSServerReport::GC_OnNatCookie, pReport, cookie);
}

void CGSServerReport::QR2PublicAddresCallback(unsigned int ip, unsigned short port, void* obj)
{
	CGSServerReport* pReport = static_cast<CGSServerReport*>(obj);
	ip = ntohl(ip);
	TO_GAME(&CGSServerReport::GC_OnPublicIP, pReport, ip, port);
}

void CGSServerReport::GCDReAuthorizeCallback(int gameid, int localid, int hint, char *challenge, void *obj)
{
	CGSServerReport* pReport = static_cast<CGSServerReport*>(obj);
	int plrId = localid ^ pReport->m_seed;
	pReport->m_reathSId.push_back(std::make_pair(plrId, hint));
  NetLog("CDKey re-auth for %d with sid=%d was sent", plrId, hint);
  pReport->NC_ReAuthRequest(plrId, challenge);
}

void CGSServerReport::GCDAuthorizeCallback(int gameid, int localid, int authenticated, char *errmsg, void *obj)
{
  CGSServerReport* pReport = static_cast<CGSServerReport*>(obj);
	int plrId = localid ^ pReport->m_seed;
  if(VERBOSE)
  {
	  if(authenticated)
      NetLog("GameSpy CDKey check : Client %d authorized by server. (%s)", plrId, errmsg);
	  else
      NetLog("GameSpy CDKey check : Client %d was not authorized by server. Reason : %s", plrId, errmsg);
  }
  if(pReport->m_netNub)
    pReport->m_netNub->OnCDKeyAuthResult(plrId, authenticated!=0,errmsg);
}

static void QR2ClientMessageCallback(gsi_char* data, int len, void* obj)
{

}


void CGSServerReport::GetMemoryStatistics(ICrySizer* pSizer)
{
  SIZER_COMPONENT_NAME(pSizer, "CGSServerReport");

  pSizer->Add(*this);
  pSizer->AddContainer(m_serverInfo.Values);
  pSizer->AddContainer(m_playersInfo);
  pSizer->AddContainer(m_teamsInfo);
  pSizer->AddContainer(m_keyIdMap);
  pSizer->AddContainer(m_keyNames);
} 
