/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:  GameSpy network profile implementation
-------------------------------------------------------------------------
History:
- 01/11/2007   : Stas Spivakov, Created
*************************************************************************/
#include "StdAfx.h"
#include "Network.h"
#include "INetworkService.h"
#include "GSNetworkProfile.h"
#include "GSStorage.h"

#include "GameSpy/gcdkey/gcdkeyc.h"
#include "GameSpy/pr/PasswordReminder.h"

struct SStatusInfo
{
	EUserStatus status;
	GPEnum		value;
	const char* text;
};

static SStatusInfo gStatusInfo[] = {	{ eUS_offline,	GP_OFFLINE,   "Offline"},
										                  { eUS_online,	  GP_ONLINE,    "Online"},
										                  { eUS_playing,	GP_PLAYING,   "Playing"},
										                  { eUS_staging,	GP_STAGING,   "Stagging"},
										                  { eUS_chatting,	GP_CHATTING,  "Chatting"},
										                  { eUS_away,		  GP_AWAY,      "Away"},
										               };
const int gStatusInfoSize = sizeof(gStatusInfo)/sizeof(gStatusInfo[0]);


static const float UPDATE_INTERVAL = 0.1f;
static bool  VERBOSE = true;

CGSNetworkProfile::SBuddyInfo::SBuddyInfo():
m_profile(0),
m_updated(false),
m_foreign(false),
m_status(GP_OFFLINE)
{
}

CGSNetworkProfile::CGSNetworkProfile(CGameSpy* gamespy):
m_gamespy(gamespy),
m_connection(0),
m_updateTimer(0),
m_loggedIn(false),
m_loggingIn(false)
{
}

CGSNetworkProfile::~CGSNetworkProfile()
{
	CleanUp();
}

bool	CGSNetworkProfile::IsDead()const
{
	return false;
}

void	CGSNetworkProfile::Init()
{
  GPResult res;
	res = gpInitialize(&m_connection, GAMESPY_PRODUCT_ID, GAMESPY_NAMESPACE_ID, 0);
	if(res == GP_NO_ERROR)
	{
		res = gpSetCallback(&m_connection, GP_ERROR,ErrorCallback, this);
		res = gpSetCallback(&m_connection, GP_RECV_BUDDY_STATUS, BuddyStatusCallback, this);
		res = gpSetCallback(&m_connection, GP_RECV_BUDDY_MESSAGE, BuddyMessageCallback, this);
		res = gpSetCallback(&m_connection, GP_RECV_BUDDY_REQUEST, BuddyAuthCallback, this);
    res = gpSetCallback(&m_connection, GP_RECV_BUDDY_REVOKE, BuddyRevokeCallback, this);
		res = gpSetCallback(&m_connection, GP_RECV_GAME_INVITE, GameInviteCallback, this);
		res = gpSetCallback(&m_connection, GP_TRANSFER_CALLBACK, FileTransferCallback, this);
		
		if(!m_updateTimer)
		{
			SCOPED_GLOBAL_LOCK;
			m_updateTimer = TIMER.AddTimer(g_time+UPDATE_INTERVAL,TimerCallback,this);
		}
	}
	else
	{
    GPErrorCode err_code;
    gpGetErrorCode(&m_connection, &err_code);
		char errorString[GP_ERROR_STRING_LEN+1];
		gpGetErrorString(&m_connection,errorString);
    TO_GAME(&CGSNetworkProfile::GC_Error,this,res,err_code,string(errorString));
	}
}

void	CGSNetworkProfile::Think()
{
	if(m_connection!=0)
	{
		gpProcess(&m_connection);
	}
}

void	CGSNetworkProfile::CleanUp()
{
	m_loggingIn = false;
	m_loggedIn = false;
	if(m_connection)
	{
		GPEnum res;
		gpIsConnected(&m_connection,&res);
		if(res == GP_CONNECTED)
			gpDisconnect(&m_connection);
		gpDestroy(&m_connection);
		m_connection = 0;
	}
	if(m_updateTimer)
	{
		SCOPED_GLOBAL_LOCK;
		TIMER.CancelTimer(m_updateTimer);
		m_updateTimer = 0;
	}
}

void CGSNetworkProfile::AddListener(INetworkProfileListener* lst)
{
  SCOPED_GLOBAL_LOCK;
  m_listeners.push_back(lst);
}

void CGSNetworkProfile::RemoveListener(INetworkProfileListener* lst)
{
  SCOPED_GLOBAL_LOCK;
  stl::find_and_erase(m_listeners,lst);
}

bool CGSNetworkProfile::CheckNick(const char* nick)
{
	int n_l = strlen(nick);
	if(!n_l)
	{
		LoginError(eNPE_nickEmpty, "Please, enter your nickname.");
		return false;
	}
	if(n_l>20)
	{
		LoginError(eNPE_nickTooLong, "Nickname should be no longer than 20 characters.");
		return false;
	}
	if(isdigit(*nick))
	{
		LoginError(eNPE_nickFirstNumber, "Nickname should not start with number.");
		return false;
	}
	if(strchr(nick,'\\')!=0)
	{
		LoginError(eNPE_nickSlash, "Nickname should not contain slash.");
		return false;
	}
	if(strchr("@+#:",nick[0])!=0)
	{
		LoginError(eNPE_nickFirstSpecial, "Nickname should not start with @, +, # or : symbols.");
		return false;
	}
	if(strchr(nick,' ')!=0)
	{
		LoginError(eNPE_nickNoSpaces, "Nickname should not contain spaces.");
		return false;
	}
	return true;
}

bool CGSNetworkProfile::CheckPassword(const char* pass)
{
	int p_l = strlen(pass);
	if(!p_l)
	{
		LoginError(eNPE_passEmpty,"Please, enter your account password.");
		return false;
	}
	if(p_l>30)
	{
		LoginError(eNPE_passTooLong, "Password should be no longer than 30 characters.");
		return false;
	}	
	return true;
}

bool CGSNetworkProfile::CheckEmail(const char* email)
{
	int m_l = strlen(email);
	if(!m_l)
	{
		LoginError(eNPE_mailEmpty, "Please, enter your email address.");
		return false;
	}

	if(m_l>50)
	{
		LoginError(eNPE_mailTooLong, "Email should be no longer than 50 characters.");
		return false;
	}
	return true;
}

void CGSNetworkProfile::Register(const char* nick, const char* email, const char* password, const char* country, SRegisterDayOfBirth dob)
{
  //check parameters
	if(!CheckNick(nick))
		return;
	if(!CheckPassword(password))
		return;
	if(!CheckEmail(email))
		return;

  FROM_GAME(&CGSNetworkProfile::NC_Register, this, string(nick), string(email), string(password), string(country), uint32(dob));
}

void CGSNetworkProfile::Login(const char* nick, const char* password)
{
	if(!CheckNick(nick))
		return;
	if(!CheckPassword(password))
		return;
  FROM_GAME(&CGSNetworkProfile::NC_Login,this, string(nick), string(password) );
}

void CGSNetworkProfile::LoginProfile(const char* email, const char* password, const char* profile)
{
	if(!CheckEmail(email))
		return;
	if(!CheckPassword(password))
		return;
	if(!CheckNick(profile))
		return;

	FROM_GAME(&CGSNetworkProfile::NC_LoginProfile,this, string(email), string(password), string(profile) );
}

void CGSNetworkProfile::Logoff()
{
  m_buddies.resize(0);
  FROM_GAME(&CGSNetworkProfile::NC_Logoff,this);
}

void CGSNetworkProfile::SetStatus(EUserStatus status, const char* location)
{
  string loc;
  if(status==eUS_chatting)
  {
    loc.Format("%s://%s",GAMESPY_GAME_NAME,GAMESPY_CHANNEL_NAME+1);
  }
  else
  {
    loc.Format("%s://",GAMESPY_GAME_NAME);
  }
  
  FROM_GAME(&CGSNetworkProfile::NC_SetStatus,this,status,string(loc+location));
}

void CGSNetworkProfile::EnumUserNicks(const char* email, const char* password)
{
  FROM_GAME(&CGSNetworkProfile::NC_EnumNicks,this, string(email), string(password));
}

void CGSNetworkProfile::AuthFriend(int id, bool auth)
{
  FROM_GAME(&CGSNetworkProfile::NC_AuthBuddy,this, id, auth);
}

void CGSNetworkProfile::RemoveFriend(int id, bool ignore)
{
  for(int i=0;i<m_buddies.size();++i)
    if(m_buddies[i].m_profile == id)
    {
      m_buddies.erase(m_buddies.begin()+i);
      GC_RevokeBuddy(id);
    }
  FROM_GAME(&CGSNetworkProfile::NC_RemoveBuddy,this,id);
}

void CGSNetworkProfile::AddFriend(int id, const char* reason)
{
  FROM_GAME(&CGSNetworkProfile::NC_AddBuddy,this,id,string(reason));
}

void CGSNetworkProfile::SendFriendMessage(int id, const char* message)
{
  FROM_GAME(&CGSNetworkProfile::NC_SendBuddyMessage,this,id,string(message));
}

bool CGSNetworkProfile::IsLoggedIn()
{
  return m_loggedIn;
}

bool CGSNetworkProfile::IsLoggingIn()
{
	SCOPED_GLOBAL_LOCK;
	return m_connection != 0 && m_loggingIn;
}

void CGSNetworkProfile::UpdateBuddies()
{
  for(int i=0;i<m_buddies.size();++i)
    UpdateUser(i);
}

void CGSNetworkProfile::GetProfileInfo(int id)
{
  FROM_GAME(&CGSNetworkProfile::NC_GetProfileInfo,this, id);
}

void CGSNetworkProfile::SearchFriends(const char* request)
{
  FROM_GAME(&CGSNetworkProfile::NC_Search,this,string(request));
}

void CGSNetworkProfile::GetUserId(const char* nick)
{
  FROM_GAME(&CGSNetworkProfile::NC_GetId,this,string(nick));
}

void CGSNetworkProfile::GetUserNick(int id)
{
  FROM_GAME(&CGSNetworkProfile::NC_GetNick,this,id);
}

void CGSNetworkProfile::ReadStats(IStatsReader* reader)
{
  if(!m_loggedIn)
  {
    reader->End(false);
    return;
  }
  FROM_GAME(&CGSNetworkProfile::NC_ReadStats,this,reader);
}

void CGSNetworkProfile::WriteStats(IStatsWriter* writer)
{
  if(!m_loggedIn)
	{
		writer->End(false);
    return;
	}
  FROM_GAME(&CGSNetworkProfile::NC_WriteStats,this,writer);
}

void CGSNetworkProfile::DeleteStats(IStatsDeleter* deleter)
{
	if(!m_loggedIn)
	{
		deleter->End(false);
		return;
	}
	FROM_GAME(&CGSNetworkProfile::NC_DeleteStats,this,deleter);
}

void CGSNetworkProfile::ReadStats(int id, IStatsReader* reader)
{
  if(!m_loggedIn)
	{
		reader->End(false);
    return;
	}
  FROM_GAME(&CGSNetworkProfile::NC_ReadStatsWithId,this,id,reader);
}

void CGSNetworkProfile::RetrievePassword(const char* email)
{
	FROM_GAME(&CGSNetworkProfile::NC_RetrievePassword,this, string(email));
}

void CGSNetworkProfile::GetAuthInfo(int &profile, string& ticket)
{
	RenewTicket();
	profile = m_profile.m_profile;
  ticket = m_profile.m_ticket;
}

void CGSNetworkProfile::GetLoginPassword(string& login, string& password)
{
	login = m_profile.m_uniquenick;
	password = m_profile.m_password;
}

void  CGSNetworkProfile::RenewTicket()
{
	char loginticket[GP_LOGIN_TICKET_LEN];
	gpGetLoginTicket(&m_connection, loginticket);
	m_profile.m_ticket = loginticket;
}

bool CGSNetworkProfile::IsAvailable()const
{
	return true;
}
	
void	CGSNetworkProfile::NC_Login(string nick,  string pwd)
{
	if(m_connection == 0)
		Init();
	if(m_connection)
	{
		GPResult res = gpConnectUniqueNick(&m_connection, nick.c_str(),pwd.c_str(),GP_NO_FIREWALL,GP_NON_BLOCKING,ConnectCallback,this);
    m_profile.m_password = pwd;
		m_loggingIn = true;
		if(res != GP_NO_ERROR)
		{
      NC_LoggedIn(res, 0, 0);
		}
	}
}

void	CGSNetworkProfile::NC_LoginProfile(string email, string pwd, string profile)
{
	if(m_connection == 0)	
		Init();
	if(m_connection)
	{
		m_register.m_email = email;
		m_register.m_password = pwd;
		m_register.m_uniquenick = profile;
		m_loggingIn = true;
		GPResult res = gpConnect(&m_connection, profile.c_str(), email.c_str(), pwd.c_str(), GP_NO_FIREWALL, GP_NON_BLOCKING, ConnectProfileCallback, this);
		m_profile.m_password = pwd;
		if(res != GP_NO_ERROR)
		{
			NC_LoggedIn(res, 0, 0);
		}
	}
}

void  CGSNetworkProfile::NC_Logoff()
{
	if(VERBOSE)
		NetLog("GS Login : logged off.");
  m_profile = SProfileInfo();
  gpDisconnect(&m_connection);
	m_loggedIn = false;
	m_loggingIn = false;
}

void	CGSNetworkProfile::NC_Register(string nick, string email, string pwd, string country, uint32 dob)
{
	if(m_connection == 0)
		Init();
	if(m_connection)
  {
    m_profile.m_password = pwd;

    m_register.m_uniquenick = nick;
    m_register.m_email = email;
    m_register.m_password = pwd;
		m_register.m_country = country;
		m_register.m_dob = dob;
		m_loggingIn = true;
		GPResult res = gpGetUserNicks(&m_connection, email.c_str(), pwd.c_str(), GP_NON_BLOCKING, EnumNicksCallback, this);
		if(res != GP_NO_ERROR)
		{
			NC_LoggedIn(res, 0, 0);
		}
  }
}

void	CGSNetworkProfile::NC_EnumNicks(string email, string pwd)
{
	if(m_connection == 0)
		Init();
	if(m_connection)
	{
		GPResult res = gpGetUserNicks(&m_connection, email.c_str(), pwd.c_str(), GP_NON_BLOCKING, EnumNicksCallback, this);
		if(res != GP_NO_ERROR)
		{
			if(VERBOSE)
			{
				gsi_char errorString[GP_ERROR_STRING_LEN];
				errorString[0] = 0;
				gpGetErrorString(&m_connection,errorString);
				NetWarning("GS enum nicks error : %s",errorString);
			}
      
		}
	}
}

void	CGSNetworkProfile::NC_GetProfileInfo(int id)
{
	if(m_connection == 0)
		Init();
	if(m_connection)
		gpGetInfo(&m_connection, id,GP_CHECK_CACHE,GP_NON_BLOCKING,ProfileInfoCallback,this);
}

void	CGSNetworkProfile::NC_SetStatus(EUserStatus status, string location)
{
	if(m_connection == 0)
		Init();
	GPEnum user_status;
  const char* status_str = "";
	
	for(int i=0;i<gStatusInfoSize;++i)
	{
		if(gStatusInfo[i].status == status)
		{
			user_status = gStatusInfo[i].value;
			status_str = gStatusInfo[i].text;
		}
	}
	status_str = "Playing Crysis Wars";
	if(m_connection)
		gpSetStatus(&m_connection, user_status, status_str, location.c_str());
}

void  CGSNetworkProfile::NC_Error(GPResult res)
{
  GPErrorCode err_code;
	gpGetErrorCode(&m_connection,&err_code);
	char errorString[GP_ERROR_STRING_LEN+1];
	gpGetErrorString(&m_connection,errorString);
	TO_GAME(&CGSNetworkProfile::GC_Error,this,res,err_code,string(errorString));
}

void  CGSNetworkProfile::NC_AuthBuddy(int id, bool auth)
{
  if(auth)
    gpAuthBuddyRequest(&m_connection,id);
  else
    gpDenyBuddyRequest(&m_connection,id);
}

void  CGSNetworkProfile::NC_AddBuddy(int id, string reason)
{
  gpSendBuddyRequest(&m_connection,id,reason.c_str());
}

void  CGSNetworkProfile::NC_RemoveBuddy(int id)
{
  gpDeleteBuddy(&m_connection,id);
  gpRevokeBuddyAuthorization(&m_connection,id);
}

void  CGSNetworkProfile::NC_SendBuddyMessage(int id, string text)
{
  gpSendBuddyMessage(&m_connection,id,text.c_str());
}

void  CGSNetworkProfile::NC_Search(string request)
{
  gpProfileSearch(&m_connection,"",request,"","","",0,GP_NON_BLOCKING,SearchCallback,this);
}

void CGSNetworkProfile::NC_GetId(string nick)
{
  gpProfileSearch(&m_connection,"",nick,"","","",0,GP_NON_BLOCKING,GetIdCallback,this);
}

void  CGSNetworkProfile::NC_GetNick(int id)
{
  gpGetInfo(&m_connection,id,GP_CHECK_CACHE,GP_NON_BLOCKING,GetNickCallback,this);
}

void  CGSNetworkProfile::NC_ReadStats(IStatsReader* reader)
{
  if(!reader)
    return;
  CGSStorage* st = m_gamespy->GetStorage();
  if(!st)
  {
		TO_GAME(&CGSNetworkProfile::GC_RequestFailed, this, (IStatsAccessor*)reader);
    return;
  }
	RenewTicket();
  st->SetUser(m_profile.m_profile,m_profile.m_ticket);
  st->ReadRecords(reader);
}

void  CGSNetworkProfile::NC_WriteStats(IStatsWriter* writer)
{
  if(!writer)
    return;
  CGSStorage* st = m_gamespy->GetStorage();
  if(!st)
  {
    TO_GAME(&CGSNetworkProfile::GC_RequestFailed, this, (IStatsAccessor*)writer);
    return;
  }
	RenewTicket();
  st->SetUser(m_profile.m_profile,m_profile.m_ticket);
  st->WriteRecords(writer);
}

void  CGSNetworkProfile::NC_DeleteStats(IStatsDeleter* deleter)
{
	if(!deleter)
		return;
	CGSStorage* st = m_gamespy->GetStorage();
	if(!st)
	{
		TO_GAME(&CGSNetworkProfile::GC_RequestFailed, this, (IStatsAccessor*)deleter);
		return;
	}
	RenewTicket();
	st->SetUser(m_profile.m_profile,m_profile.m_ticket);
	st->DeleteRecords(deleter);	
}

void  CGSNetworkProfile::NC_ReadStatsWithId(int id,IStatsReader* reader)
{
	if(!reader)
		return;
	CGSStorage* st = m_gamespy->GetStorage();
	if(!st)
	{
		TO_GAME(&CGSNetworkProfile::GC_RequestFailed, this, (IStatsAccessor*)reader);
		return;
	}
	RenewTicket();
	st->SetUser(m_profile.m_profile,m_profile.m_ticket);
	st->SearchRecords(reader, id);	
}

void  CGSNetworkProfile::NC_LoggedIn(GPResult result, int profile, const char* nick)
{
  if(result == GP_NO_ERROR)
  {
    m_profile.m_profile = profile;
    m_profile.m_uniquenick = nick;
		RenewTicket();

    m_loggedIn = true;
    gpGetInfo(&m_connection,profile,GP_DONT_CHECK_CACHE,GP_NON_BLOCKING,ProfileInfoCallback,this);
  }
	else
	{
		gpDisconnect(&m_connection);
	}
	m_loggingIn = false;
  GPErrorCode err_code;
  gpGetErrorCode(&m_connection, &err_code);
  gsi_char errorString[GP_ERROR_STRING_LEN];
  errorString[0] = 0;
  gpGetErrorString(&m_connection,errorString);
  TO_GAME(&CGSNetworkProfile::GC_LoginResult, this, result, err_code, string(errorString));  
}

void	CGSNetworkProfile::NC_RetrievePassword(string email)
{
	PasswordReminder pw;
	bool ok = pw.Initialize(false,"",0,0);
	if(ok && pw.RequestMethod())
	{
		ok = pw.SendReminderUsingEmail(string("default"), string(email));
	}
	else
		ok = false;

	TO_GAME(&CGSNetworkProfile::GC_GetPasswordStatus, this, ok);
}

static ENetworkProfileError TranslateError(GPErrorCode code)
{
	ENetworkProfileError err = eNPE_ok;

	switch(code)
	{
	case GP_FORCED_DISCONNECT:
		err = eNPE_anotherLogin;
		break;

	case GP_PARSE:
	case GP_NOT_LOGGED_IN:
	case GP_BAD_SESSKEY:
	case GP_DATABASE:
	case GP_NETWORK:
	case GP_CONNECTION_CLOSED:
		err = eNPE_disconnected;
		break;

	case GP_LOGIN_BAD_NICK:
	case GP_LOGIN_BAD_PASSWORD:
	case GP_LOGIN_BAD_PROFILE:
		err = eNPE_loginFailed;
		break;

	case GP_LOGIN_BAD_EMAIL:
	case GP_LOGIN_PROFILE_DELETED:
	case GP_LOGIN_SERVER_AUTH_FAILED:
	case GP_LOGIN_BAD_UNIQUENICK:
	case GP_LOGIN_BAD_PREAUTH:
		err = eNPE_loginFailed;
		break;

	case GP_LOGIN_TIMEOUT:
		err = eNPE_loginTimeout;
		break;

	case GP_LOGIN_CONNECTION_FAILED:
		err = eNPE_connectFailed;
		break;

	case GP_REGISTERUNIQUENICK_TAKEN:
	case GP_REGISTERUNIQUENICK_RESERVED:
	case GP_NEWUSER_UNIQUENICK_INUSE:
		err = eNPE_nickTaken;
		break;

	case GP_NEWUSER_BAD_PASSWORD:
		err = eNPE_registerAccountError;
		break;

	case GP_CHECK_BAD_EMAIL:
	case GP_CHECK_BAD_NICK:
	case GP_CHECK_BAD_PASSWORD:
		err = eNPE_loginFailed;
		break;

	case GP_NEWUSER_BAD_NICK:
	case GP_NEWUSER_UNIQUENICK_INVALID:
	case GP_NEWPROFILE_BAD_NICK:
	case GP_NEWPROFILE_BAD_OLD_NICK:
	case GP_UPDATEPRO_BAD_NICK:
	case GP_REGISTERUNIQUENICK_BAD_NAMESPACE:
		err = eNPE_registerGeneric;
		break;
	case GP_ADDBUDDY_BAD_FROM:
	case GP_ADDBUDDY_BAD_NEW:
	case GP_ADDBUDDY_ALREADY_BUDDY:
	case GP_BM_NOT_BUDDY:
	case GP_GETPROFILE_BAD_PROFILE:
	case GP_DELBUDDY_NOT_BUDDY:
	case GP_REVOKE_NOT_BUDDY:
	case GP_DELPROFILE_LAST_PROFILE:
	case GP_SEARCH_CONNECTION_FAILED:
	case GP_SEARCH_TIMED_OUT:
		err = eNPE_actionFailed;
		break;
	default:
		err = eNPE_actionFailed;
	}
	return err;
}


void	CGSNetworkProfile::GC_Error(GPResult res, GPErrorCode code, string reason)
{
	ENetworkProfileError err = eNPE_ok;

	if(res != GP_NO_ERROR)
	{
		err = TranslateError(code);
	}

	if(VERBOSE)
		NetWarning("Profile error : %s", reason.c_str());

	for(int i=0;i<m_listeners.size();++i)
	{
    m_listeners[i]->OnError(err,reason);
	}
}

void  CGSNetworkProfile::GC_LoginResult(GPResult res, GPErrorCode code, string descr)
{
	ENetworkProfileError err = eNPE_ok;

	if(res != GP_NO_ERROR)
	{
		err = TranslateError(code);
	}

  LoginError(err,descr);
}

void  CGSNetworkProfile::LoginError(ENetworkProfileError err, const char* descr)
{
	if(gEnv->pSystem->IsDedicated() && err != eNPE_ok)
		NetWarning("Login error : %s", descr);
	for(int i=0;i<m_listeners.size();++i)
		m_listeners[i]->LoginResult(err, descr, m_profile.m_profile, m_profile.m_uniquenick);
}

void	CGSNetworkProfile::GC_AddNick(string nick)
{
	for(int i=0;i<m_listeners.size();++i)
		m_listeners[i]->AddNick(nick);
}

void	CGSNetworkProfile::GC_BuddyStatus(int id, int index, GPEnum status, string location, string statusString)
{
	if(m_buddies.size() <= index)
	{
		m_buddies.resize(index+1);
	}
	SBuddyInfo& bi = m_buddies[index];
	if(bi.m_profile != id)
	{
		bi.m_profile = id;
		bi.m_updated = false;
	}
	if(bi.m_status != status || bi.m_location!=location)
	{
		bi.m_status = status;
		bi.m_location = location;
    bi.m_statusString = statusString;
				
		if(bi.m_updated)
		{
			UpdateUser(index);
		}
	}
}

void	CGSNetworkProfile::GC_ProfileInfo(int id, string nick, string country, bool foreign_name)
{
	SBuddyInfo* bi = 0;
	int idx;
	for(int i = 0;i<m_buddies.size();++i)
	{
		if(m_buddies[i].m_profile == id)
		{
			bi = &m_buddies[i];
			idx = i;
			break;
		}
	}
	if(bi)
  {
	  bi->m_nick = nick;
	  bi->m_country = country;
	  bi->m_updated = true;
		bi->m_foreign = foreign_name;
	  UpdateUser(idx);
  }
  
  for(int i=0;i<m_listeners.size();++i)
  {
		m_listeners[i]->OnProfileInfo(id,"namespace",foreign_name?"foreign":"home");
    m_listeners[i]->OnProfileInfo(id,"nick",nick);
    m_listeners[i]->OnProfileInfo(id,"country",country);
    m_listeners[i]->OnProfileComplete(id);
  }
}

void	CGSNetworkProfile::GC_BuddyMessage(int id, string message)
{
	//TODO:check ignore
  for(int i=0;i<m_listeners.size();++i)
    m_listeners[i]->OnMessage(id, message);
}

void	CGSNetworkProfile::GC_RevokeBuddy(int id)
{
  for(int i=0;i<m_listeners.size();++i)
    m_listeners[i]->RemoveFriend(id);
}

void  CGSNetworkProfile::GC_BuddyRequest(int id, string message)
{
  for(int i=0;i<m_listeners.size();++i)
    m_listeners[i]->OnFriendRequest(id, message);
}

void  CGSNetworkProfile::GC_SearchResult(int id, string nick)
{
  if(nick.empty())
    return;
  for(int i=0;i<m_listeners.size();++i)
  {
    m_listeners[i]->OnSearchResult(id, nick);
  }
}

void  CGSNetworkProfile::GC_SearchComplete()
{
  for(int i=0;i<m_listeners.size();++i)
  {
     m_listeners[i]->OnSearchComplete();
  }
}

void  CGSNetworkProfile::GC_GetIdResult(string nick,int id)
{
  for(int i=0;i<m_listeners.size();++i)
  {
    m_listeners[i]->OnUserId(nick, id);
  }
}

void  CGSNetworkProfile::GC_GetNickResult(int id, string nick, bool foreign_name)
{
  for(int i=0;i<m_listeners.size();++i)
  {
    m_listeners[i]->OnUserNick(id, nick, foreign_name);
  }
}

void  CGSNetworkProfile::GC_RequestFailed(IStatsAccessor* acc)
{
	acc->End(false);
}

void	CGSNetworkProfile::GC_GetPasswordStatus(bool ok)
{
	for(int i=0;i<m_listeners.size();++i)
	{
		m_listeners[i]->RetrievePasswordResult(ok);
	}
}

void	CGSNetworkProfile::UpdateUser(int idx)
{
	if(m_listeners.empty())
		return;
	SBuddyInfo& bi = m_buddies[idx];
	if(!bi.m_updated)
		return;

	EUserStatus user_status = eUS_offline;
	for(int i=0;i<gStatusInfoSize;i++)
	{
		if(gStatusInfo[i].value == bi.m_status)
		{
			user_status = gStatusInfo[i].status;
			break;
		}
	}

  for(int i=0;i<m_listeners.size();++i)
		m_listeners[i]->UpdateFriend(bi.m_profile,bi.m_nick,user_status,bi.m_location,bi.m_foreign);
}

void	CGSNetworkProfile::ErrorCallback(GPConnection * connection, void * arg, void * obj)
{
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	GPErrorArg* pError = (GPErrorArg*)arg;
	TO_GAME(&CGSNetworkProfile::GC_Error,pProfile,pError->result,pError->errorCode,string(pError->errorString));
}

void	CGSNetworkProfile::ConnectCallback(GPConnection * connection, void * arg, void * obj)
{
	CGSNetworkProfile* pLogin = (CGSNetworkProfile*)obj;
	GPConnectResponseArg* pResp = (GPConnectResponseArg*)arg;
	if(pResp->result==GP_NO_ERROR)
	{
		if(VERBOSE)
			NetLog("GS Login : logged in as %s.",pResp->uniquenick);
	}
	else
		gpDisconnect(connection);
  pLogin->NC_LoggedIn(pResp->result, pResp->profile,pResp->uniquenick);
}

void	CGSNetworkProfile::ConnectCallbackRegister(GPConnection * connection, void * arg, void * obj)
{
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	GPConnectResponseArg* pResp = (GPConnectResponseArg*)arg;
	if(pResp->result==GP_NO_ERROR)
	{
		if(VERBOSE)
			NetLog("GS Login : logged in as %s.", pResp->uniquenick);
		if(!pProfile->m_register.m_country.empty())
		{
			gpSetInfos(connection, GP_COUNTRYCODE, pProfile->m_register.m_country);		
		}
		if(pProfile->m_register.m_dob)
			gpSetInfod(connection, GP_BIRTHDAY, pProfile->m_register.m_dob.day, pProfile->m_register.m_dob.month, pProfile->m_register.m_dob.year);
	}
	else
		gpDisconnect(connection);

	pProfile->NC_LoggedIn(pResp->result, pResp->profile,pResp->uniquenick);
}

void	CGSNetworkProfile::ConnectProfileCallback(GPConnection * connection, void * arg, void * obj )
{
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	GPConnectResponseArg* pResp = (GPConnectResponseArg*)arg;
	if(pResp->result == GP_NO_ERROR)
	{
		pProfile->m_register.m_profile = pResp->profile;
		if(strcmp(pResp->uniquenick,"@unregistered"))
		{
			pProfile->NC_LoggedIn(pResp->result,pProfile->m_register.m_profile,pResp->uniquenick);
		}
		else
		{
			gpRegisterUniqueNick(connection, pProfile->m_register.m_uniquenick,0,GP_NON_BLOCKING,RegisterNickCallback,pProfile);
		}
	}
	else
	{
		gpDisconnect(connection);
		pProfile->NC_LoggedIn(pResp->result,0,0);
	}
}

void	CGSNetworkProfile::ConnectRegisterCallback(GPConnection * connection, void * arg, void * obj )
{
  CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
  GPConnectResponseArg* pResp = (GPConnectResponseArg*)arg;
  if(pResp->result == GP_NO_ERROR)
  {
    pProfile->m_register.m_profile = pResp->profile;
		if(!stricmp(pResp->uniquenick,pProfile->m_register.m_uniquenick.c_str()))
		{
			pProfile->NC_LoggedIn(pResp->result,pProfile->m_register.m_profile,pProfile->m_register.m_uniquenick);
		}
		else
		{
			gpRegisterUniqueNick(connection, pProfile->m_register.m_uniquenick,0,GP_NON_BLOCKING,RegisterNickCallback,pProfile);
		}
  }
  else
  {
		gpDisconnect(connection);
    pProfile->NC_LoggedIn(pResp->result,0,0);
  }
}

void	CGSNetworkProfile::RegisterNickCallback(GPConnection * connection, void * arg, void * obj )
{
  CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
  GPRegisterUniqueNickResponseArg* pResp = (GPRegisterUniqueNickResponseArg*)arg;

  if(pResp->result != GP_NO_ERROR)
    gpDisconnect(connection);

	if(!pProfile->m_register.m_country.empty())
		gpSetInfos(connection,GP_COUNTRYCODE,pProfile->m_register.m_country);

  pProfile->NC_LoggedIn(pResp->result,pProfile->m_register.m_profile,pProfile->m_register.m_uniquenick);
}

void	CGSNetworkProfile::ConnectNewCallback(GPConnection * connection, void * arg, void * obj )
{
  CGSNetworkProfile* pLogin = (CGSNetworkProfile*)obj;
  GPConnectResponseArg* pResp = (GPConnectResponseArg*)arg;
  if(pResp->result==GP_NO_ERROR)
  {
    if(VERBOSE)
      NetLog("GS Login : logged in.");
  }
  pLogin->NC_LoggedIn(pResp->result, pResp->profile, pResp->uniquenick);
}

void	CGSNetworkProfile::EnumNicksCallback(GPConnection * connection, void * arg, void * obj )
{ 
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	GPGetUserNicksResponseArg* pResp = (GPGetUserNicksResponseArg*)arg;

  if(pResp->result == GP_NO_ERROR)
	{
		for(uint i=0;i<pResp->numNicks;i++)
		{
			if(!(pResp->uniquenicks[i]) || !*(pResp->uniquenicks[i]) && !stricmp(pResp->nicks[i],pProfile->m_register.m_uniquenick))
			{
				gpConnect(connection, pResp->nicks[i], pProfile->m_register.m_email, pProfile->m_register.m_password, GP_NO_FIREWALL, GP_NON_BLOCKING, ConnectRegisterCallback, pProfile);
				return;
			}
		}

		for(uint i=0;i<pResp->numNicks;i++)
		{
      if(!(pResp->uniquenicks[i]) || !*(pResp->uniquenicks[i]))
      {
        gpConnect(connection, pResp->nicks[i], pProfile->m_register.m_email, pProfile->m_register.m_password, GP_NO_FIREWALL, GP_NON_BLOCKING, ConnectRegisterCallback, pProfile);
        return;
      }
		}
	}
  //final
	gpConnectNewUser(&pProfile->m_connection, pProfile->m_register.m_uniquenick, pProfile->m_register.m_uniquenick, pProfile->m_register.m_email,pProfile->m_register.m_password, 0, GP_NO_FIREWALL, GP_NON_BLOCKING, ConnectCallbackRegister, pProfile);
}

void	CGSNetworkProfile::BuddyStatusCallback(GPConnection * connection, void * arg, void * obj )
{
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	GPRecvBuddyStatusArg *pArg = (GPRecvBuddyStatusArg*)arg;
	GPBuddyStatus status;
	gpGetBuddyStatus(connection,pArg->index,&status);
	gpGetInfo(connection,pArg->profile,GP_CHECK_CACHE,GP_NON_BLOCKING,&CGSNetworkProfile::ProfileInfoCallback,obj);
	TO_GAME(&CGSNetworkProfile::GC_BuddyStatus, pProfile, pArg->profile, pArg->index, status.status, string(status.locationString),string(status.statusString));
}

void	CGSNetworkProfile::BuddyMessageCallback(GPConnection * connection, void * arg, void * obj )
{
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	GPRecvBuddyMessageArg* pArg = (GPRecvBuddyMessageArg*)arg;
	TO_GAME(&CGSNetworkProfile::GC_BuddyMessage, pProfile, pArg->profile, string(pArg->message));
}

void	CGSNetworkProfile::BuddyAuthCallback(GPConnection * connection, void * arg, void * obj )
{
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	GPRecvBuddyRequestArg* pArg = (GPRecvBuddyRequestArg*)arg;
  TO_GAME(&CGSNetworkProfile::GC_BuddyRequest, pProfile, pArg->profile, string(pArg->reason));
}

void	CGSNetworkProfile::BuddyRevokeCallback(GPConnection * connection, void * arg, void * obj )
{
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	GPRecvBuddyRevokeArg* pArg = (GPRecvBuddyRevokeArg*)arg;
  TO_GAME(&CGSNetworkProfile::GC_RevokeBuddy, pProfile, pArg->profile);
}

void	CGSNetworkProfile::GameInviteCallback(GPConnection * connection, void * arg, void * obj )
{
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	GPRecvGameInviteArg* pArg = (GPRecvGameInviteArg*)arg;
}


void	CGSNetworkProfile::ProfileInfoCallback(GPConnection * connection, void * arg, void * obj )
{
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	GPGetInfoResponseArg* pArg = (GPGetInfoResponseArg*)arg;
  if(pArg->profile == pProfile->m_profile.m_profile)///yay it's me!
  {
    pProfile->m_profile.m_uniquenick = pArg->uniquenick;
    pProfile->m_profile.m_email = pArg->email;
    pProfile->m_profile.m_profilenick = pArg->nick;
    pProfile->m_profile.m_complete = true;
  }
	TO_GAME(&CGSNetworkProfile::GC_ProfileInfo, pProfile,pArg->profile, pArg->uniquenick[0]?string(pArg->uniquenick):string(pArg->nick), string(pArg->countrycode),pArg->uniquenick[0]==0);
}

void	CGSNetworkProfile::FileTransferCallback(GPConnection * connection, void * arg, void * obj )
{
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	GPTransferCallbackArg* pArg = (GPTransferCallbackArg*)arg;
	gpRejectTransfer(connection, pArg->transfer, "In-game file transfers are not supported");
}

void	CGSNetworkProfile::SearchCallback(GPConnection * connection, void * arg, void * obj )
{
  CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
  GPProfileSearchResponseArg* pArg = (GPProfileSearchResponseArg*)arg;
  if(pArg->result == GP_NO_ERROR)
  {
    for(int i=0;i<pArg->numMatches;++i)
    {
      TO_GAME(&CGSNetworkProfile::GC_SearchResult,pProfile,pArg->matches[i].profile,string(pArg->matches[i].uniquenick));
    }

    if(pArg->more==GP_DONE)
    {
      TO_GAME(&CGSNetworkProfile::GC_SearchComplete,pProfile);
    }
  }
}

void	CGSNetworkProfile::GetIdCallback(GPConnection * connection, void * arg, void * obj )
{   
  CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
  GPProfileSearchResponseArg* pArg = (GPProfileSearchResponseArg*)arg;
  if(pArg->result == GP_NO_ERROR && pArg->numMatches == 1)
  {
    TO_GAME(&CGSNetworkProfile::GC_GetIdResult,pProfile,string(pArg->matches->uniquenick),pArg->matches->profile);
  }
  else
  {
    pProfile->NC_Error(pArg->result!=GP_NO_ERROR?pArg->result:GP_PARAMETER_ERROR);
  }
}

void	CGSNetworkProfile::GetNickCallback(GPConnection * connection, void * arg, void * obj )
{
  CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
  GPGetInfoResponseArg* pArg = (GPGetInfoResponseArg*)arg;
	TO_GAME(&CGSNetworkProfile::GC_GetNickResult, pProfile,pArg->profile, pArg->uniquenick[0]?string(pArg->uniquenick):string(pArg->nick),pArg->uniquenick[0]==0);
}

void	CGSNetworkProfile::TimerCallback(NetTimerId id, void* obj, CTimeValue)
{
	CGSNetworkProfile* pProfile = (CGSNetworkProfile*)obj;
	
	if(!pProfile->IsDead() && pProfile->m_updateTimer == id)
	{
		pProfile->Think();
		SCOPED_GLOBAL_LOCK;
		pProfile->m_updateTimer = TIMER.AddTimer( g_time+UPDATE_INTERVAL, TimerCallback, pProfile );
	}
}

void CGSNetworkProfile::GetMemoryStatistics(ICrySizer * s)
{
  s->Add(*this);
  s->AddContainer(m_buddies);
}
