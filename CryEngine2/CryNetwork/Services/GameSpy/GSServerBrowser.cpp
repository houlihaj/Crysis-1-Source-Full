#include "StdAfx.h"
#include "GameSpy.h"
#include "GSServerBrowser.h"
#include "Network.h"

#include "GameSpy/qr2/qr2regkeys.h"

static const float UPDATE_INTERVAL = 0.005f;

static const bool VERBOSE=false;

static unsigned char BASIC_FIELDS[] = {HOSTNAME_KEY, GAMETYPE_KEY,  MAPNAME_KEY, NUMPLAYERS_KEY, MAXPLAYERS_KEY,GAMEVER_KEY,PASSWORD_KEY,HOSTPORT_KEY,GAMESPY_SERVER_KEY_ANTICHEAT,GAMESPY_SERVER_KEY_OFFICIAL,GAMESPY_SERVER_KEY_VOICECOMM,GAMESPY_SERVER_KEY_FRIENDLYFIRE,GAMESPY_SERVER_KEY_DX10,GAMESPY_SERVER_KEY_DEDICATED,GAMESPY_SERVER_KEY_GAMEPADSONLY,GAMESPY_SERVER_KEY_COUNTRY,GAMESPY_SERVER_KEY_MODNAME,GAMESPY_SERVER_KEY_MODVERSION};
static int NUM_BASIC_FIELDS = sizeof(BASIC_FIELDS) / sizeof(BASIC_FIELDS[0]);

static const bool REPORT_VALUES=true;
static const bool REPORT_PING=true;

CGSServerBrowser::CGSServerBrowser():
m_updateTimer(0),
m_pServerListener(0),
m_serverCount(0),
m_serverPending(0),
m_dead(false),
m_serverBrowser(0),
m_lastId(1)
{
}

CGSServerBrowser::~CGSServerBrowser()
{
	SCOPED_GLOBAL_LOCK;

	m_dead=true;

	if(m_updateTimer)
	{
		TIMER.CancelTimer(m_updateTimer);
		m_updateTimer=0;
	}

	if(m_serverBrowser)
	{
		ServerBrowserFree(m_serverBrowser);
		m_serverBrowser=0;
	}

	if(VERBOSE)
		NetLog("SB destroyed");
}

//interface method implementations 

bool CGSServerBrowser::IsAvailable()const
{
	return true;
}

void	CGSServerBrowser::Start(bool browseLAN)
{
    FROM_GAME(&CGSServerBrowser::NStart,this,browseLAN);
}

void  CGSServerBrowser::SetListener(IServerListener* lst)
{
  SCOPED_GLOBAL_LOCK;
  m_pServerListener = lst;
}

void	CGSServerBrowser::UpdateServerInfo(int id)
{
	FROM_GAME(&CGSServerBrowser::NUpdateServerInfo,this,id);
}

void CGSServerBrowser::BrowseForServer(uint ip, ushort port)
{
  FROM_GAME(&CGSServerBrowser::NBrowseForServer,this,ip,port);
}

void  CGSServerBrowser::BrowseForServer(const char* address, ushort port)
{
  FROM_GAME(&CGSServerBrowser::NBrowseForServerAddr,this,string(address),port);
}

void	CGSServerBrowser::Stop()
{
	FROM_GAME(&CGSServerBrowser::NStop,this);
}

void	CGSServerBrowser::Update()
{
	FROM_GAME(&CGSServerBrowser::NUpdate,this);
}

void  CGSServerBrowser::SendNatCookie(uint ip, ushort port,int cookie)
{
  FROM_GAME(&CGSServerBrowser::NSendNatCookie, this, ip, port, cookie);
}

void  CGSServerBrowser::CheckDirectConnect(int id, ushort port)
{
  FROM_GAME(&CGSServerBrowser::NCheckDirectConnect, this, id, port);
}

int		CGSServerBrowser::GetServerCount()
{
	return m_serverCount;
}

int		CGSServerBrowser::GetPendingQueryCount()
{
	return m_serverPending;
}

//game side methods

void CGSServerBrowser::GOnServer(int id,SNServerInfo si,bool update)
{
	SBasicServerInfo bi;
	bi.m_numPlayers	=si.m_numPlayers;
	bi.m_maxPlayers	=si.m_maxPlayers;
	bi.m_private	=si.m_private;				
	bi.m_hostPort	=si.m_hostPort;
  bi.m_publicPort = si.m_publicPort;
	bi.m_publicIP=	si.m_publicIP;
	bi.m_privateIP	=si.m_privateIP;
  bi.m_anticheat = si.m_anticheat;
  bi.m_official = si.m_official;

  bi.m_dx10 = si.m_dx10;
  bi.m_friendlyfire = si.m_friendlyfire;
  bi.m_voicecomm = si.m_voicecomm;
  bi.m_dedicated = si.m_dedicated;
	bi.m_gamepadsonly = si.m_gamepadsonly;

	bi.m_hostName		=si.m_hostName.c_str();
	bi.m_mapName		=si.m_mapName.c_str();
	bi.m_gameVersion	=si.m_gameVersion.c_str();
	bi.m_gameType		=si.m_gameType.c_str();
	bi.m_modName = si.m_modName.c_str();
	bi.m_modVersion = si.m_modVersion.c_str();
	bi.m_country =si.m_country.c_str();

  if(m_pServerListener)
  {
    if(update)
      m_pServerListener->UpdateServer(id,&bi);
    else
      m_pServerListener->NewServer(id,&bi);
  }
}



/*void	CGSServerBrowser::GUpdateBasicServerInfo(const int id,const SBasicServerInfo& info)
{
	m_pServerListener->UpdateBasicServerInfo(id,info);
}*/

void CGSServerBrowser::GServerPinged(int id,int ping)
{
    if(!m_pServerListener)
        return;
	m_pServerListener->UpdatePing(id,ping);
}

void CGSServerBrowser::GUpdateValue(int id,string name,string value)
{
    if(!m_pServerListener)
        return;
    m_pServerListener->UpdateValue(id,name.c_str(),value.c_str());
}

void CGSServerBrowser::GUpdatePlayerValue(int id, int playerNum,string name,string value)
{
    if(!m_pServerListener)
        return;
    m_pServerListener->UpdatePlayerValue(id,playerNum,name.c_str(),value.c_str());
}

void CGSServerBrowser::GUpdateTeamValue(int id, int teamNum,string name,string value)
{
    if(!m_pServerListener)
        return;
    m_pServerListener->UpdateTeamValue(id,teamNum,name.c_str(),value.c_str());
}

void CGSServerBrowser::GServerUpdateFailed(int id)
{
    if(!m_pServerListener)
        return;
    m_pServerListener->ServerUpdateFailed(id);
}

void CGSServerBrowser::GServerUpdateComplete(int id)
{
  if(!m_pServerListener)
    return;
  m_pServerListener->ServerUpdateComplete(id);
}

void CGSServerBrowser::GOnError(EServerBrowserError err)
{
    if(!m_pServerListener)
        return;
    m_pServerListener->OnError(err);
}

void CGSServerBrowser::GUpdateComplete(bool cancel)
{
    if(!m_pServerListener)
        return;
    m_pServerListener->UpdateComplete(cancel);
}

void CGSServerBrowser::GRemoveServer(int id)
{
    if(!m_pServerListener)
        return;
    m_pServerListener->RemoveServer(id);
}

void CGSServerBrowser::GUpdateProgressCounters(int pending, int total)
{
	m_serverCount=total;
	m_serverPending=pending;
}

void CGSServerBrowser::GDirectConnect(bool neednat, uint ip, ushort port)
{
  if(m_pServerListener)
    m_pServerListener->ServerDirectConnect(neednat,ip,port);
}

//network side methods

void CGSServerBrowser::TimerCallback(NetTimerId,void* instance,CTimeValue)
{
	SCOPED_GLOBAL_LOCK;
	//m_updateTimer = TIMER.AddTimer( g_time + INIT_CHECK_INTERVAL, TimerCallback, this );
	CGSServerBrowser *object=(CGSServerBrowser*)instance; 
	object->OnTimerCallback();
}

void CGSServerBrowser::OnTimerCallback()
{
	if(!m_serverBrowser)
		return;
	//m_dbgServerCount=0;
	HandleSBError(ServerBrowserThink(m_serverBrowser));
  //NetLog("serverbrowser think %d",g_time);
	//if(m_dbgServerCount)
	//	NetLog("SRVCOUNT:%d",m_dbgServerCount);
	NUpdateProgressCounters();
	m_updateTimer = TIMER.AddTimer( g_time+UPDATE_INTERVAL, TimerCallback, this );
}

void CGSServerBrowser::SBCallback(ServerBrowser sb, SBCallbackReason reason, SBServer server,void *instance)
{
	CGSServerBrowser *object=(CGSServerBrowser*)instance; 
	object->OnSBCallback(reason,server);
}

static void KeyEnumLog(gsi_char* key,gsi_char* value,void*)
{
	NetLog("\t %s = %s",key,value);
}

static void LogKeys(SBServer server)
{
	SBServerEnumKeys(server,KeyEnumLog,0);
}

void CGSServerBrowser::KeyEnumCallback(const gsi_char* key,const gsi_char* value,void* instance)
{
	CGSServerBrowser *sb=(CGSServerBrowser*)instance;

	int k,l;
	k=l=strlen(key)-1;
	while(k>0 && key[k]>='0' && key[k]<='9')
		k--;

	if(k!=l && key[k]=='_')
	{
		//player
		int n=atoi(key+k+1);
		TO_GAME(&CGSServerBrowser::GUpdatePlayerValue,sb,sb->m_currentServerId,n,string(key,key+k),string(value));
	}
	else if(k!=l && k>0 && key[k]=='t' && key[k-1]=='_')
	{
		//team
		int n=atoi(key+k+1);
		TO_GAME(&CGSServerBrowser::GUpdateTeamValue,sb,sb->m_currentServerId,n,string(key,key+k-1),string(value));
	}
	else
	{
		//other key
		TO_GAME(&CGSServerBrowser::GUpdateValue,sb,sb->m_currentServerId,string(key),string(value));
	}

//	SBServerHasPrivateAddress
}

void CGSServerBrowser::PumpKeys(SBServer server, bool failed)
{
	TSrvMap::iterator i=m_serverMap.find(server);
	NET_ASSERT(i!=m_serverMap.end());
	int id=i->second.m_id;
  bool report_basic = ( SBServerHasBasicKeys(server) || SBServerHasFullKeys(server) || failed);
  
	if(REPORT_VALUES && report_basic )
	{

		const char** keys=qr2_registered_key_list;

		SNServerInfo si;
		si.m_numPlayers=SBServerGetIntValue(server,keys[NUMPLAYERS_KEY],0);
		si.m_maxPlayers=SBServerGetIntValue(server,keys[MAXPLAYERS_KEY],0);
		si.m_private=(SBServerGetBoolValue(server,keys[PASSWORD_KEY],SBFalse)==SBTrue);
    si.m_hostPort=failed?SBServerGetPublicQueryPort(server):SBServerGetIntValue(server,keys[HOSTPORT_KEY],0);
    si.m_publicPort=SBServerGetPublicQueryPort(server);
		si.m_publicIP=SBServerGetPublicInetAddress(server);
		si.m_privateIP=SBServerHasPrivateAddress(server) ? SBServerGetPrivateInetAddress(server) :0;
		si.m_hostName=SBServerGetStringValue(server,keys[HOSTNAME_KEY],"");
		si.m_mapName=SBServerGetStringValue(server,keys[MAPNAME_KEY],"");
		si.m_gameVersion=SBServerGetStringValue(server,keys[GAMEVER_KEY],"");
		si.m_gameType=SBServerGetStringValue(server,keys[GAMETYPE_KEY],"");
		si.m_country = SBServerGetStringValue(server,keys[GAMESPY_SERVER_KEY_COUNTRY],"");
    si.m_official=(SBServerGetBoolValue(server,keys[GAMESPY_SERVER_KEY_OFFICIAL],SBFalse)==SBTrue);
    si.m_anticheat=(SBServerGetBoolValue(server,keys[GAMESPY_SERVER_KEY_ANTICHEAT],SBFalse)==SBTrue);
    si.m_dx10 = (SBServerGetBoolValue(server,keys[GAMESPY_SERVER_KEY_DX10],SBFalse)==SBTrue);
    si.m_friendlyfire = (SBServerGetBoolValue(server,keys[GAMESPY_SERVER_KEY_FRIENDLYFIRE],SBFalse)==SBTrue);
    si.m_voicecomm = (SBServerGetBoolValue(server,keys[GAMESPY_SERVER_KEY_VOICECOMM],SBFalse)==SBTrue);
    si.m_dedicated = (SBServerGetBoolValue(server,keys[GAMESPY_SERVER_KEY_DEDICATED],SBFalse)==SBTrue);
		si.m_gamepadsonly = (SBServerGetBoolValue(server,keys[GAMESPY_SERVER_KEY_GAMEPADSONLY],SBFalse)==SBTrue);
		si.m_modName = SBServerGetStringValue(server, keys[GAMESPY_SERVER_KEY_MODNAME],"");
		si.m_modVersion = SBServerGetStringValue(server, keys[GAMESPY_SERVER_KEY_MODVERSION],"");

    TO_GAME(&CGSServerBrowser::GOnServer,this,i->second.m_id,si,i->second.m_reported); 

		//NetLog("%d has basic keys",i->second.m_id);
    i->second.m_reported = true;
	}

	if(REPORT_VALUES && SBServerHasFullKeys(server))
	{
		m_currentServerId=id;
		SBServerEnumKeys(server,(SBServerKeyEnumFn)KeyEnumCallback,this);
	}
    
  if(SBServerHasValidPing(server))
  {
      i->second.m_pinged=true;
      int ping=SBServerGetPing(server);
      TO_GAME(&CGSServerBrowser::GServerPinged,this,id,ping);
  }
}

void CGSServerBrowser::OnSBCallback(SBCallbackReason reason,SBServer server)
{
	char adr[128];
	adr[0]=0;
	SBBool nat=SBFalse;

	if(reason!=sbc_updatecomplete && reason!=sbc_queryerror)
		nat=SBServerGetConnectionInfo(m_serverBrowser,server,0,adr);

	switch(reason)
	{
	case sbc_serveradded:
		{
			if(VERBOSE)
				NetLog("GS server added: %s %x",adr,(unsigned)server);

			m_serverMap.insert(TSrvMap::value_type(server,SServerInfo(m_lastId)));

			m_lastId++;

			m_dbgServerCount++;

			PumpKeys(server,false);
		}		
		break;
	case sbc_serverupdated:
    {
      if(VERBOSE)
			  NetLog("GS server updated: %s",adr);
		  //LogKeys(server);
		  PumpKeys(server,false);
      if(SBServerHasFullKeys(server))
      {
        TSrvMap::iterator i=m_serverMap.find(server);
        NET_ASSERT(i!=m_serverMap.end() && "sbc_serverdeleted - server id not found");
        TO_GAME(&CGSServerBrowser::GServerUpdateComplete,this,i->second.m_id);
      }
    }
		break;
	case sbc_serverupdatefailed:
		{
			if(VERBOSE)
				NetLog("GS server update failed: %s",adr);
			TSrvMap::iterator i=m_serverMap.find(server);
      PumpKeys(server,true);
      NET_ASSERT(i!=m_serverMap.end() && "sbc_serverdeleted - server id not found");
      TO_GAME(&CGSServerBrowser::GServerUpdateFailed,this,i->second.m_id);
		}
		break;
	case sbc_serverdeleted:
		{
			if(VERBOSE)
				NetLog("GS server deleted: %s %x",adr,(unsigned)server);
			TSrvMap::iterator i=m_serverMap.find(server);
			NET_ASSERT(i!=m_serverMap.end() && "sbc_serverdeleted - server id not found");
			TO_GAME(&CGSServerBrowser::GRemoveServer,this,i->second.m_id);
		}
		break;
	case sbc_updatecomplete:
		if(VERBOSE)
			NetLog("Update complete!");
		NUpdateProgressCounters();//to make counters consistent with update finish callback
		TO_GAME(&CGSServerBrowser::GUpdateComplete,this,false);
		break;
	case sbc_queryerror:
		{
			NetLog("Gamespy Server Browser Query Error: %s",ServerBrowserListQueryError(m_serverBrowser));
			TO_GAME(&CGSServerBrowser::GOnError,this,eSBE_General);
		}
		break;
  case sbc_serverchallengereceived:
    break;
	default:
		NET_ASSERT(!"invalid reason in server browser callback");
		break;
	}
}

void	CGSServerBrowser::NStart(bool browseLAN)
{
	ASSERT_GLOBAL_LOCK;
  if(!m_updateTimer)
    m_updateTimer = TIMER.AddTimer( g_time + UPDATE_INTERVAL, TimerCallback, this );
  if(!m_serverBrowser || m_lanOnly != browseLAN)
  {
    if(m_serverBrowser)
      ServerBrowserFree(m_serverBrowser);

    gsi_char  secret_key[9];
    secret_key[0] = 'z';
    secret_key[1] = 'K';
    secret_key[2] = 'b';
    secret_key[3] = 'Z';
    secret_key[4] = 'i';
    secret_key[5] = 'M';
    secret_key[6] = '\0';

    m_serverBrowser=ServerBrowserNew(GAMESPY_GAME_NAME,GAMESPY_GAME_NAME, secret_key,0,40,QVERSION_QR2,m_lanOnly?SBTrue:SBFalse,SBCallback,this);

    m_lanOnly = browseLAN;
  }
 
  //NUpdate();
}

void	CGSServerBrowser::NUpdateServerInfo(const int id)
{
	TSrvMap::iterator i;
	for(i=m_serverMap.begin();i!=m_serverMap.end();++i)
	{
		if(i->second.m_id==id)
			break;
	}

	if(i==m_serverMap.end())
	{
		NET_ASSERT(false);
		return;
	}

  if(!m_updateTimer)
	  m_updateTimer = TIMER.AddTimer( g_time+UPDATE_INTERVAL, TimerCallback, this );
  
  ServerBrowserAuxUpdateServer(m_serverBrowser,i->first,SBTrue,SBTrue);
}

void	CGSServerBrowser::NStop()
{
	if(m_serverBrowser)
		ServerBrowserHalt(m_serverBrowser);
  TO_GAME(&CGSServerBrowser::GUpdateComplete,this,true);
}

void CGSServerBrowser::HandleSBError(const SBError error)
{
	if(error==sbe_noerror)
		return;

	NetLog("Gamespy Server Browser Error: %s",ServerBrowserErrorDesc(m_serverBrowser,error));
	
	EServerBrowserError sbe;

	switch(error)
	{
	case sbe_socketerror:
	case sbe_dnserror:
	case sbe_connecterror:
		sbe=eSBE_ConnectionFailed;
		break;
	case sbe_dataerror:
	case sbe_allocerror:
	case sbe_paramerror:
		sbe=eSBE_ConnectionFailed;
		break;
	case sbe_duplicateupdateerror:
		sbe=eSBE_DuplicateUpdate;
		break;
	default:
		NET_ASSERT(false && "invalid SBError in HandleSBError");
	}

	TO_GAME(&CGSServerBrowser::GOnError,this,sbe);
}

void	CGSServerBrowser::NUpdate()
{
  FRAME_PROFILER("CGSServerBrowser::NUpdate", gEnv->pSystem, PROFILE_NETWORK);

	m_serverMap.clear();
	m_lastId=1;

  if(m_serverBrowser)
  {
	  ServerBrowserHalt(m_serverBrowser);
	  ServerBrowserClear(m_serverBrowser);
    SBError err;

    if(m_lanOnly)
    {
      ushort first = CVARS.LanScanPortFirst;
      ushort last = max(first, first + min(0,CVARS.LanScanPortNum) );
      err = ServerBrowserLANUpdate(m_serverBrowser,SBTrue,first,last);
    }
    else
      err = ServerBrowserUpdate(m_serverBrowser,SBTrue,SBFalse,BASIC_FIELDS,NUM_BASIC_FIELDS,0);

	  HandleSBError(err);
  }
}

void  CGSServerBrowser::NUpdateProgressCounters()
{
	int pending=ServerBrowserPendingQueryCount(m_serverBrowser);
	int total=ServerBrowserCount(m_serverBrowser);
	TO_GAME(&CGSServerBrowser::GUpdateProgressCounters,this,pending,total);
}

void  CGSServerBrowser::NSendNatCookie(uint ip, ushort port, const int cookie)
{
    string Ip;
    Ip.Format("%d.%d.%d.%d",ip&0xFF,(ip>>8)&0xFF,(ip>>16)&0xFF,(ip>>24)&0xFF);
    ServerBrowserSendNatNegotiateCookieToServer(m_serverBrowser,Ip,port,cookie);
}

void  CGSServerBrowser::NCheckDirectConnect(int id, ushort port)
{
  TSrvMap::iterator i;
  for(i=m_serverMap.begin();i!=m_serverMap.end();++i)
  {
    if(i->second.m_id==id)
      break;
  }
  if(i==m_serverMap.end())
  {
    if(VERBOSE)
      NetError("Server with id %d was not found.",id);
    return;
  }
  if(m_lanOnly)
  {
    TO_GAME(&CGSServerBrowser::GDirectConnect,this,false,SBServerGetPublicInetAddress(i->first),port);
    return;
  }
  SBBool priv_addr = SBServerHasPrivateAddress(i->first);
  uint public_ip = SBServerGetPublicInetAddress(i->first);
  if((priv_addr && public_ip == ServerBrowserGetMyPublicIPAddr(m_serverBrowser)))
  {
    TO_GAME(&CGSServerBrowser::GDirectConnect,this,false,SBServerGetPrivateInetAddress(i->first),port);
  }
  else if(SBServerDirectConnect(i->first) && !priv_addr)
  {
    TO_GAME(&CGSServerBrowser::GDirectConnect,this,false,public_ip,SBServerGetPublicQueryPort(i->first));
  }
  else
  {
    TO_GAME(&CGSServerBrowser::GDirectConnect,this,true,public_ip,SBServerGetPublicQueryPort(i->first));
  }
}


void  CGSServerBrowser::NBrowseForServer(uint ip, ushort port)
{
  bool found = false;
  TSrvMap::iterator i;
  const char** keys=qr2_registered_key_list;

  for(i=m_serverMap.begin();i!=m_serverMap.end();++i)
  {
    uint ip_ = SBServerGetPublicInetAddress(i->first);
    ushort port_ = SBServerGetIntValue(i->first,keys[HOSTPORT_KEY],0);
    if(ip_==ip && port_==port)
    {
      ServerBrowserAuxUpdateServer(m_serverBrowser,i->first,SBTrue,SBFalse);
      found = true;
    }
  }
  if(found)
    return;

  string ip_str(32);
  ip_str.Format("%d.%d.%d.%d",ip&0xFF,(ip>>8)&0xFF,(ip>>16)&0xFF,(ip>>24)&0xFF);
  ServerBrowserAuxUpdateIP(m_serverBrowser, ip_str.c_str(), port, SBTrue, SBTrue, SBFalse);
}

void CGSServerBrowser::NBrowseForServerAddr(string addr, ushort port)
{
  ServerBrowserAuxUpdateIP(m_serverBrowser, addr.c_str(), port, SBTrue, SBTrue, SBFalse);
}

bool CGSServerBrowser::IsDead()const
{
	return false;
}
