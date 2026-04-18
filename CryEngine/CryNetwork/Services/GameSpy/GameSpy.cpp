#include "StdAfx.h"
#include "GameSpy.h"
#include "Network.h"

#include "GSServerReport.h"
#include "GSChat.h"
#include "GSLogin.h"
#include "GSStatsTrack.h"
#include "GSNat.h"
#include "GSFileDownload.h"
#include "GSNetworkProfile.h"
#include "GSPatchCheck.h"

#include "GameSpy/common/gsPlatform.h"
#include "GameSpy/common/gsAvailable.h"

#include "IGameFramework.h"

#include "GSContextViewExt.h"

#include "GSStorage.h"

static const float INIT_CHECK_INTERVAL = 0.2f;
static const float WAIT_CHECK_INTERVAL = 2*60.0f;



static void GSDebugCallback(GSIDebugCategory c, GSIDebugType t,GSIDebugLevel l,
                                 const char* format, va_list list)
{
#if ENABLE_DEBUG_KIT
  if(CVARS.GSDebugOutput == 0)
    return;
  static char temp[4096];
  vsnprintf(temp,4096,format,list); 
  SCOPED_GLOBAL_LOCK;
  NetLog(temp);
#endif
}


CGameSpy::CGameSpy()
{
  qr2_registered_key_list[GAMESPY_SERVER_KEY_ANTICHEAT] = "anticheat";
  qr2_registered_key_list[GAMESPY_SERVER_KEY_OFFICIAL] = "official";
  qr2_registered_key_list[GAMESPY_SERVER_KEY_VOICECOMM] = "voicecomm";
  qr2_registered_key_list[GAMESPY_SERVER_KEY_FRIENDLYFIRE] = "friendlyfire";
  qr2_registered_key_list[GAMESPY_SERVER_KEY_DEDICATED] = "dedicated";
  qr2_registered_key_list[GAMESPY_SERVER_KEY_DX10] = "dx10";
	qr2_registered_key_list[GAMESPY_SERVER_KEY_GAMEPADSONLY] = "gamepadsonly";
	qr2_registered_key_list[GAMESPY_SERVER_KEY_COUNTRY] = "country";
	qr2_registered_key_list[GAMESPY_SERVER_KEY_MODNAME] = "modname";
	qr2_registered_key_list[GAMESPY_SERVER_KEY_MODVERSION] = "modversion";

  GSIStartAvailableCheck(GAMESPY_GAME_NAME);
	m_state = eNSS_Initializing;

  gsSetDebugCallback(&GSDebugCallback);
  gsSetDebugLevel(GSIDebugCat_NatNeg,GSIDebugType_All,GSIDebugLevel_Hardcore);
	gsSetDebugLevel(GSIDebugCat_HTTP,GSIDebugType_All,GSIDebugLevel_Hardcore);

	SCOPED_GLOBAL_LOCK;
	m_timer = TIMER.AddTimer( g_time + INIT_CHECK_INTERVAL, TimerCallback, this );
}

CGameSpy::~CGameSpy()
{
	Cleanup();
}

void CGameSpy::Close()
{
	Cleanup();
}

ENetworkServiceState CGameSpy::GetState()
{
	SCOPED_GLOBAL_LOCK;
	return m_state;
}

/*void CGameSpy::QueryInterface( ENetworkServiceInterface iface, void ** pInterface )
{
	*pInterface = NULL;
}*/

void CGameSpy::CreateContextViewExtensions(bool server, IContextViewExtensionAdder* adder)
{
  CGSCVExtension* pExt = new CGSCVExtension(server,this);
  adder->AddExtension(pExt);
  m_extensions.push_back(pExt);
}

IServerBrowser* CGameSpy::GetServerBrowser()
{
	if(!m_serverBrowser)
		m_serverBrowser=new CGSServerBrowser();
	return m_serverBrowser;
}

INetworkChat* CGameSpy::GetNetworkChat()
{
	if(!m_networkChat)
		m_networkChat=new CGSChat(this);
	return m_networkChat;
}

IServerReport* CGameSpy::GetServerReport()
{
	if(!m_serverReport)
	{
		m_serverReport=new CGSServerReport(this);
	}
	return m_serverReport;
}

IStatsTrack* CGameSpy::GetStatsTrack(int version)
{
	if(!m_stats)
	{
		m_stats=new CGSStatsTrack(this, version);
	}
	if(m_stats->GetVersion() != version)
		return 0;
	else
		return m_stats;
}

INetworkProfile*	CGameSpy::GetNetworkProfile()
{
	if(!m_profile)
	{
		m_profile=new CGSNetworkProfile(this);
	}
	return m_profile;
}

INatNeg*    CGameSpy::GetNatNeg(INatNegListener*const listener)
{
    if(!m_natNeg)
    {
        m_natNeg = new CGSNatNeg(listener);
    }
    return m_natNeg;
}

IFileDownloader* CGameSpy::GetFileDownloader()
{
	if(!m_pFileDownload)
	{
		m_pFileDownload = new CGSFileDownloader;
	}
	return m_pFileDownload;
}

IPatchCheck* CGameSpy::GetPatchCheck()
{
	if(!m_pPatchCheck)
	{
		m_pPatchCheck = new CGSPatchCheck;
	}

	return m_pPatchCheck;
}

void CGameSpy::RegisterExtension(CGSCVExtension* ext)
{
  m_extensions.push_back(ext);
}

void CGameSpy::UnregisterExtension(CGSCVExtension* ext)
{
  m_extensions.erase(std::remove(m_extensions.begin(),m_extensions.end(),ext),m_extensions.end());
}

CGSStorage* CGameSpy::GetStorage()
{
  if(!m_storage)
  {
    m_storage = new CGSStorage;
  }
  return m_storage;
}

CGSStatsTrack* CGameSpy::GetCurrentStatsTrack()
{
	return m_stats;
}

bool CGameSpy::SendSessionId(int plr, const gsi_u8 sessionId[])//SC_SESSION_GUID_SIZE
{
	for(int i=0;i<m_extensions.size();++i)
	{		
		if( m_extensions[i]->IsServer() && m_extensions[i]->GetPlayerId() == plr)
		{
			m_extensions[i]->SendSessionId(sessionId);
			return true;
		}
	}
	return false;
}

void CGameSpy::SetLocalStatsConnectionId(const gsi_u8 connectionId[])
{
	for(int i=0;i<m_extensions.size();++i)
	{		
		if(!m_extensions[i]->IsServer())
		{
			m_extensions[i]->SendConnectionId(connectionId);
			if(m_stats.get() && !gEnv->bServer)//on client we want to know id as well
				m_stats->SetConnectionId(m_extensions[i]->GetPlayerId(), connectionId);
		}
	}
}

void CGameSpy::SetStatsConnectionId(int plrid, const gsi_u8 connectionId[])
{
	if(m_stats.get())
		m_stats->SetConnectionId(plrid, connectionId);
}

class CGSTestInterface:public ITestInterface
{
	struct ServerListener:public IServerListener
	{
		IServerBrowser *SB;
		int Pings;
		DWORD TickCount;
		int SrvCount;
		IGameQueryListener *GQListener;
		std::map<int, string> Servers;

		ServerListener(IGameQueryListener *listener):SB(0),Pings(0),SrvCount(0),GQListener(listener)
		{
		}

		// IServerListener ***********************************************************************
		virtual void NewServer(const int id,const SBasicServerInfo* info)
		{
			/*CryLog("New server: %s %s %s (%d/%d) %s",
				info->m_hostName,
				info->m_mapName,
				info->m_gameType,
				info->m_numPlayers,
				info->m_maxPlayers,
				info->m_private ? "private":"public");*/
			//SB->UpdateServerInfo(id);

			SB->UpdateServerInfo(id);

			char buffer[256];
			string description(info->m_hostName);
			itoa(id, buffer, 10);
			description.append(buffer);

			string address(info->m_hostName);
			itoa(info->m_hostPort, buffer, 10);
			address.append(":");
			address.append(buffer);

			string serverInfo("<game levelname=\"");
			serverInfo.append(info->m_mapName);
			serverInfo.append("\" curPlayers=\"");
			itoa(info->m_numPlayers, buffer, 10);
			serverInfo.append(buffer);
			serverInfo.append("\" gameRules=\"");
			serverInfo.append(info->m_gameType);
			serverInfo.append("\" version=\"");
			serverInfo.append(info->m_gameVersion);
			serverInfo.append("\" maxPlayers=\"");
			itoa(info->m_maxPlayers, buffer, 10);
			serverInfo.append(buffer);
			serverInfo.append("\"\\>");

			Servers[id] = address;
			GQListener->AddServer(description.c_str(), address.c_str(), serverInfo, 9999);
		}

    virtual void UpdateServer(const int id,const SBasicServerInfo*)
    {

    }

		virtual void UpdatePing(const int id,const int ping)
		{
			Pings++;

			GQListener->AddPong(Servers[id], ping);

			if(!(Pings%100))
				CryLog("%d/%d",SB->GetServerCount()-SB->GetPendingQueryCount(),SB->GetServerCount());
		}

		//remove server from UI server list
		virtual void RemoveServer(const int id)
		{
			GQListener->RemoveServer(Servers[id]);
			std::map<int, string>::iterator it = Servers.begin();
			for(; it != Servers.end(); ++it)
				if(it->first == id)
				{
					Servers.erase(it);
					return;
				}
		}

		//extended data - can be dependent on game type etc, retrieved only by request via IServerBrowser::QueryServerInfo
		virtual void UpdateValue(const int id,const char* name,const char* value)
		{
			//CryLog("Value(%d): %s %s",id,name,value);
		}
		virtual void UpdatePlayerValue(const int id,const int playerNum,const char* name,const char* value)
		{
			//CryLog("Player value(%d): %d %s %s",id,playerNum,name,value);
		}

		virtual void UpdateTeamValue(const int id,const int teamNum,const char *name,const char* value)
		{
			//CryLog("Team value(%d): %d %s %s",id,teamNum,name,value);
		}

		virtual void OnError(const EServerBrowserError err)
		{
			CryLog("Server browser error: %d",err);
		}

    virtual void UpdateComplete(bool cancel)
    {
			CryLog("Server list update complete (%d/%d)",SB->GetPendingQueryCount(),SB->GetServerCount());
			CryLog("Time: %ld ms",(long)(GetTickCount()-TickCount));
			//SB->Update();
		}

    virtual void ServerUpdateComplete(const int id)
    {

    }

		virtual void ServerUpdateFailed(const int id)
		{
			CryLog("Server Query Failed (%d) - server down or unreachable",id);
		}

    virtual void ServerDirectConnect(bool,uint,ushort)
    {
    }
		// ~IServerListener ***********************************************************************
	};

	// 
	virtual void TestBrowser(IGameQueryListener *GQListener)
	{
		CryLog("TestNSServerBrowser");
		INetworkService *serv=GetISystem()->GetINetwork()->GetService("GameSpy");

		DWORD t=GetTickCount();

		static bool first_time=true;

		if(first_time)
		{
			ENetworkServiceState state;
			do
			{
				state=serv->GetState();
			}while(state==eNSS_Initializing);

			if(state==eNSS_Failed)
				return;

			first_time=false;
		}


		static ServerListener l(GQListener);
		IServerBrowser *sb=serv->GetServerBrowser();

		l.SB=sb;
		
		l.TickCount=GetTickCount();
    sb->SetListener(&l);
		sb->Start(false);

		//sb->Stop();
		//sb->Start(false);
		CryLog("I:%ld",(GetTickCount()-t));

	}

	struct CServerReportListener : public IServerReportListener
	{
		virtual void OnNatCookie(int cookie)
		{
		  gEnv->pGame->GetIGameFramework()->GetServerNetNub()->OnNatCookieReceived(cookie);
		}

		virtual void OnError(EServerReportError code)
		{
			NetWarning("ServerReport error - code %d",int(code));
		}

		virtual void OnPublicIP(uint ip, unsigned short port)
		{
			NetWarning("ServerReport public ip: %d.%d.%d.%d:%d",(ip>>24)&0xFF,(ip>>16)&0xFF,(ip>>8)&0xFF,ip&0xFF,port);
		}

		virtual void OnAuthRequest(int playerid, const char* challenge)
		{

		}

		virtual void OnAuthResult(int playerid, bool ok)
		{

		}
	};

	virtual void TestReport()
	{
		CryLog("TestNSServerReport");

		static bool Reporting = false;

		INetworkService *serv=GetISystem()->GetINetwork()->GetService("GameSpy");

		ENetworkServiceState state;
		do
		{
			state=serv->GetState();
		}while(state==eNSS_Initializing);

		if(state==eNSS_Failed)
			return;
		static CServerReportListener listener;
		IServerReport *sr=serv->GetServerReport();
				
		if(!Reporting)
		{
			
			string cdkey = gEnv->pConsole->GetCVar("cl_gs_cdkey")->GetString();

			sr->SetReportParams(10,2);
			sr->SetServerValue("hostname","Stas");
			sr->SetServerValue("gamever","2 weeks to alpha");
			sr->SetServerValue("mapname","crytek");
			sr->SetServerValue("numplayers","10");
			sr->SetServerValue("numteams","2");
			sr->SetServerValue("maxplayers","16");
			for(int i=0;i<10;i++)
			{
				sr->SetPlayerValue(i,"player",string().Format("Player%d",i));
				sr->SetPlayerValue(i,"team",(i%2)?"Red":"Blue");
			}
			for(int i=0;i<2;i++)
			{
				sr->SetTeamValue(i,"score",string().Format("%d",rand()));
				sr->SetTeamValue(i,"team",(i%2)?"Red":"Blue");
			}
			sr->StartReporting(GetISystem()->GetIGame()->GetIGameFramework()->GetServerNetNub(),&listener);

			Reporting = true;
		}
		else
		{
			sr->StopReporting();
			Reporting = false;
		}
	}
	
	class CChatListener : public IChatListener
	{
	public:
		CChatListener():
		m_chat(0)
		{
		}
		virtual void Joined(EChatJoinResult r)
		{
			if(r == eCJR_Success)
			{
				CryLog("Joined game channel");
				if(m_chat)
				{
					m_chat->Say("Hi ALL!");
				}
			}

		}
		virtual void Message(const char* from,const char* message, ENetworkChatMessageType type)
		{
			switch(type)
			{
      case eNCMT_server:
        CryLog("CHAT SERVER: <%s>: %s", from, message);
        break;
      case eNCMT_say:
				CryLog("CHAT: <%s>: %s", from, message);
				break;
			case eNCMT_data:
				CryLog("CHAT: From <%s> to you: %s", from, message);
				break;
			}
		}
		virtual void ChatUser(const char* nick,EChatUserStatus st)
		{
			CryLog("CHAT: User %s is here.", nick);
		}
    virtual void OnError(int)
    {

    }
		void	SetChat(INetworkChat* chat)
		{
			m_chat = chat;
		}

		void OnChatKeys(const char* user, int num, const char** keys, const char** values)
		{

		}
		void OnGetKeysFailed(const char* user)
		{

		}
	private:
		INetworkChat* m_chat;
	};

	class CLoginListener : public INetworkProfileListener
	{
	public:

		CLoginListener():m_profile(0)
		{
			
		}

		void SetProfilePtr(INetworkProfile* prof)
		{
			m_profile = prof;
		}
		virtual void AddNick(const char* nick)
		{
			CryLog("Available nick : %s" , nick);
		}

		virtual void LoginResult(ENetworkProfileError res, const char*, int, const char*)
		{
			if(res == eNPE_ok)
			{
				CryLog("GS Login : OK.");
				if(m_profile)
          m_profile->SetStatus(eUS_playing,"crysiswars://123.123.12.3:1");
			}
			else
				CryLog("GS Login : failed.");
		}

    virtual void OnFriendRequest(int id, const char* message)
    {

    }

		virtual void OnMessage(int id, const char* message)
		{
			CryLog("GameSpy buddy : %d %s",id ,message);
		}

		virtual void OnError(ENetworkProfileError error, const char* d)
		{
		}

		virtual void OnCDKeyResult(bool result)
		{
		}

		void UpdateFriend(int id, const char* nick, EUserStatus status, const char* str, bool f)
		{
			CryLog("GS Buddy: %d %s",id,nick);
		}

    void RemoveFriend(int id)
    {
    }

		void OnAuthResponce(const char* responce)
		{
		}

    void OnProfileInfo(int id, const char* key, const char* value)
    {

    }

    void OnProfileComplete(int id)
    {

    }

    virtual void OnSearchResult(int id, const char* nick)
    {
    }

    virtual void OnSearchComplete()
    {
    }

    virtual void OnUserId(const char* nick, int id)
    {

    }

    virtual void OnUserNick(int id, const char* nick, bool f_n)
    {

    }
		virtual void RetrievePasswordResult(bool ok)
		{

		}
		INetworkProfile* m_profile;
	};

	virtual void TestChat()
	{
		CryLog("TestNSChat");

		static bool Connected = false;

		INetworkService *serv=GetISystem()->GetINetwork()->GetService("GameSpy");
		ENetworkServiceState state;
		do
		{
			state=serv->GetState();
		}while(state==eNSS_Initializing);

		if(state==eNSS_Failed)
			return;

		static CChatListener listener;
		INetworkChat *sc=serv->GetNetworkChat();
    sc->SetListener(&listener);
		listener.SetChat(sc);
		if(sc->IsAvailable())
		{
			if(!Connected)
			{
				sc->Join();
				Connected = true;
			}
			else
			{
				sc->Leave();
				Connected = false;
			}
		}
		else
		{
			static CLoginListener loginListener;
			INetworkProfile* login = serv->GetNetworkProfile();
      login->AddListener(&loginListener);
			loginListener.SetProfilePtr(login);
			string email = gEnv->pConsole->GetCVar("cl_gs_email")->GetString();
			string pwd = gEnv->pConsole->GetCVar("cl_gs_password")->GetString();
			string nick = gEnv->pConsole->GetCVar("cl_gs_nick")->GetString();
			if(!nick.empty())
				login->Login(nick,pwd);
			else
				login->EnumUserNicks(email,pwd);
		}
	}

	virtual void TestStats()
	{
		CryLog("TestNSStats");

		INetworkService *serv=GetISystem()->GetINetwork()->GetService("GameSpy");
		if(!serv)
			return;
		IStatsTrack *st = serv->GetStatsTrack(1);
		if(!st)
			return;
		st->EndGame();
		
	/*	INetworkService *serv=GetISystem()->GetINetwork()->GetService("GameSpy");

		if(serv->GetState()!=eNSS_Ok)
			return;

		IStatsTrack *st = serv->GetStatsTrack();

		g_track2.reset(new CGSStatsTrack((CGameSpy*)serv));
		g_track2->Login("Kuliado","iddqd");
		while(!g_track2->IsLoggedIn())
		{
			gEnv->pNetwork->SyncWithGame(eNGS_FrameStart);
			gEnv->pNetwork->SyncWithGame(eNGS_FrameEnd);
			gEnv->pTimer->UpdateOnFrameStart();
		}
		g_track2->GetLocalCertificate(g_temp2);

		CGSStatsTrack *track = (CGSStatsTrack*)st;
		track->Login("StasClown","123456");
		while(!track->IsLoggedIn())
		{
			gEnv->pNetwork->SyncWithGame(eNGS_FrameStart);
			gEnv->pNetwork->SyncWithGame(eNGS_FrameEnd);
			gEnv->pTimer->UpdateOnFrameStart();
		}
		track->GetLocalCertificate(g_temp1);

		st->StartGame();

		int plr1 = st->AddPlayer(0);
		int plr2 = st->AddPlayer(1);
		st->SetPlayerValue(plr1, 1, 10);
		st->SetPlayerValue(plr2, 1, 20);

		while(!track->IsSessionStarted())
		{
			gEnv->pNetwork->SyncWithGame(eNGS_FrameStart);
			gEnv->pNetwork->SyncWithGame(eNGS_FrameEnd);
			gEnv->pTimer->UpdateOnFrameStart();
		}

		while(!g_track2->HasConnectionId())
		{
			gEnv->pNetwork->SyncWithGame(eNGS_FrameStart);
			gEnv->pNetwork->SyncWithGame(eNGS_FrameEnd);
			gEnv->pTimer->UpdateOnFrameStart();
		}

		st->EndGame();*/
	}
};

ITestInterface*	CGameSpy::GetTestInterface()
{
	static CGSTestInterface test;
	return &test;
}


void	CGameSpy::OnAvailable()
{
	m_storage = new CGSStorage();
	if(gEnv->pSystem->IsDedicated())
	{
		GetNetworkProfile();
		if(strlen(CVARS.StatsLogin->GetString()) > 0)
			m_profile->Login(CVARS.StatsLogin->GetString(), CVARS.StatsPassword->GetString());
	}
}

void	CGameSpy::OnUnavailable()
{

}

void CGameSpy::Cleanup()
{
	if (m_state == eNSS_Initializing)
		GSICancelAvailableCheck();
	m_state = eNSS_Closed;

	if (m_timer)
	{
    SCOPED_GLOBAL_LOCK;
		TIMER.CancelTimer( m_timer );
		m_timer = 0;
	}
}

void CGameSpy::TimerCallback( NetTimerId id, void * pUser, CTimeValue tm )
{
	CGameSpy * pThis = static_cast<CGameSpy*>(pUser);

	pThis->m_timer = 0;
	float timeout = -1;

	// state evolution
	switch (pThis->m_state)
	{
	case eNSS_Initializing:
		switch (GSIAvailableCheckThink())
		{
		case GSIACWaiting:
			pThis->m_state = eNSS_Initializing;
			timeout = INIT_CHECK_INTERVAL;
			break;
		case GSIACAvailable:
			pThis->m_state = eNSS_Ok;
			NetLog("Gamespy service available");
			pThis->OnAvailable();
			break;
		case GSIACTemporarilyUnavailable:
			pThis->m_state = eNSS_Initializing;
			timeout = WAIT_CHECK_INTERVAL;
			NetLog("Gamespy temporarily unavailable");
			break;
		case GSIACUnavailable:
			NetLog("Gamespy service unavailable");
			pThis->m_state = eNSS_Failed;
			break;
		}
		break;
	}

	// update timer
	if (timeout >= 0)
		pThis->m_timer = TIMER.AddTimer( g_time + timeout, TimerCallback, pThis );
}

void CGameSpy::GetMemoryStatistics(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "CGameSpy");

	pSizer->Add(*this);
  if(m_serverBrowser.get())
	  m_serverBrowser->GetMemoryStatistics(pSizer);
  if(m_serverReport.get())
	  m_serverReport->GetMemoryStatistics(pSizer);
  if(m_networkChat.get())
	  m_networkChat->GetMemoryStatistics(pSizer);
  if(m_profile.get())
	  m_profile->GetMemoryStatistics(pSizer);
  if(m_stats.get())
	  m_stats->GetMemoryStatistics(pSizer);
  if(m_natNeg.get())
	  m_natNeg->GetMemoryStatistics(pSizer);
  if(m_storage)
    m_storage->GetMemoryStatistics(pSizer);
}
