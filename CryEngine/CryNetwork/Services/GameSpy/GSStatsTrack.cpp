/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:  Gamespy StatsTrack Interface implementation
-------------------------------------------------------------------------
History:
- 11/16/2006   : Stas Spivakov, Created
*************************************************************************/
#include "StdAfx.h"
#include "Network.h"
#include "GameSpy.h"
#include "GSNetworkProfile.h"
#include "GSStatsTrack.h"

const float UPDATE_INTERVAL = 1.0f;
const bool	VERBOSE	= true;

CGSStatsTrack::CGSStatsTrack(CGameSpy* gs,int version):
m_updateTimer(0),
m_listener(0),
m_interface(0),
m_loggedIn(false),
m_host(false),
m_sessionCreated(false),
m_haveConnectionId(false),
m_parent(gs),
m_playerIdPool(1),
m_teamIdPool(1),
m_version(version)
{
	m_parent->GetStorage();//needed 
}

CGSStatsTrack::~CGSStatsTrack()
{
	CleanUp();
}

int	CGSStatsTrack::GetVersion()const
{
	return m_version;
}

uint  CGSStatsTrack::Login(const char* uniquenick, const char* password)
{
	return wsLoginUnique(WSLogin_PARTNERCODE_GAMESPY, GAMESPY_NAMESPACE_ID, uniquenick, password, 0, LoginCallback, this);
}

void  CGSStatsTrack::Logoff()
{
	memset(&m_certificate,0,sizeof(m_certificate));
	memset(&m_privateData,0,sizeof(m_privateData));
	m_loggedIn = false;
}

void  CGSStatsTrack::SetListener(IStatsTrackListener* lst)
{
	SCOPED_GLOBAL_LOCK;
	m_listener = lst;
}

bool CGSStatsTrack::IsPending(EStatsPendingAction pa)
{
	return stl::find(m_pending,pa);
}

void CGSStatsTrack::AddAction(EStatsPendingAction pa)
{
	if(VERBOSE)
	{
		NetLog("GameSpy Stats : AddAction %d",int(pa));
	}
	bool idle = m_pending.empty();
	m_pending.push_back(pa);
	if(idle)
		ProcessPending();
}

void CGSStatsTrack::OnCompleted(EStatsPendingAction pa)
{
	if(VERBOSE)
	{
		NetLog("GameSpy Stats : OnCompleted %d",int(pa));
	}
	if(m_pending.front() == pa)
	{
		m_pending.erase(m_pending.begin());
	}
	else
	{
		NET_ASSERT(m_pending.front() == pa);
	}
	ProcessPending();
}

void CGSStatsTrack::ProcessPending()
{
	if(m_pending.empty())
		return;

	do
	{
		bool fail = false;
		EStatsPendingAction pa = m_pending.front();
		switch(pa)
		{
		case eSPA_Login:
			{
				string nick, pass;

				CGSNetworkProfile* profile = static_cast<CGSNetworkProfile*>(m_parent->GetNetworkProfile());
				if(profile && profile->IsLoggedIn())
				{
					profile->GetLoginPassword(nick, pass);
				}
				/*else if(gEnv->pSystem->IsDedicated())
				{
					nick = CVARS.StatsLogin->GetString();
					pass = CVARS.StatsPassword->GetString();
				}*/

				if(!nick.empty() && !pass.empty())
				{
					uint32 res = Login(nick,pass);
					if(res)
					{
						TO_GAME(&CGSStatsTrack::GC_LoginError, this, WSLoginValue(res));
						fail = true;
						break;
					}
				}
				else
				{
					TO_GAME(&CGSStatsTrack::GC_LoginError, this, WSLogin_UserNotFound);
					fail = true;
					break;
				}					
			}
			break;
		case eSPA_StartSession:
			{
				if(!m_loggedIn)
				{
					NetWarning("GameSpy Stats : Cannot create session: not logged in");
					TO_GAME(&CGSStatsTrack::GC_Error, this, SCResult_NOT_INITIALIZED);
					fail = true;
					break;
				}
				m_host = true;
				Init();
				if(!m_interface)
				{
					if(VERBOSE)
						NetWarning("GameSpy Stats : Cannot create session: init : not initialized");
					TO_GAME(&CGSStatsTrack::GC_Error, this, SCResult_NOT_INITIALIZED);
					fail = true;
					break;
				}
				SCResult res = scCreateSession(m_interface,&m_certificate,&m_privateData,CreateSessionCallback, 0, this);
				if(res != SCResult_NO_ERROR)
				{
					TO_GAME(&CGSStatsTrack::GC_Error, this, res);
					fail = true;
					break;
				}
			}
			break;
		case eSPA_SetIntention:
			{
				if(!m_loggedIn)
				{
					NetWarning("GameSpy Stats : Cannot set intention: not logged in");
					TO_GAME(&CGSStatsTrack::GC_Error, this, SCResult_NOT_INITIALIZED);
					fail = true;
					break;
				}
				Init();
				if(!m_interface)
				{
					if(VERBOSE)
						NetWarning("GameSpy Stats : Cannot set intention: init not initialized");
					TO_GAME(&CGSStatsTrack::GC_Error, this, SCResult_NOT_INITIALIZED);
					fail = true;
					break;
				}

				SCResult res = SCResult_NO_ERROR;

				if(!m_host)
				{
					res = scSetSessionId(m_interface,m_sessionId);
					if(res != SCResult_NO_ERROR)
					{
						TO_GAME(&CGSStatsTrack::GC_Error, this, res);
						fail = true;
						break;
					}
				}

				res = scSetReportIntention(m_interface, /*m_host?gsi_true:gsi_false*/gsi_true, &m_certificate, &m_privateData, m_host?SetIntentionCallback:SetClientIntentionCallback, 0, this);
				if(res != SCResult_NO_ERROR)
				{
					TO_GAME(&CGSStatsTrack::GC_Error, this, res);
					fail = true;
					break;
				}
			}
			break;
		case eSPA_Report:
			{
				if(!m_interface || !m_loggedIn || !m_haveConnectionId)
				{
					if(VERBOSE)
						NetWarning("GameSpy Stats : Cannot report session: init : %s login : %s",m_interface?"good":"nil",m_loggedIn?"true":"false", m_haveConnectionId?"true":"false");
					TO_GAME(&CGSStatsTrack::GC_Error, this, SCResult_INVALID_PARAMETERS);
					fail = true;
					break;
				}
				
				SCResult res = scCreateReport(m_interface, m_version, m_toReport.size(), m_teamData.size(), &m_report);
				if(res != SCResult_NO_ERROR)
				{
					TO_GAME(&CGSStatsTrack::GC_Error, this, res);
					fail = true;
					break;
				}
				/*res = scReportSetAsMatchless(report);
				if(res != SCResult_NO_ERROR)
				{
					TO_GAME(&CGSStatsTrack::GC_Error, this, res);
					fail = true;
					break;
				}*/
				res = FillReport(m_report);
				if(res != SCResult_NO_ERROR)
				{
					TO_GAME(&CGSStatsTrack::GC_Error, this, res);
					fail = true;
					break;
				}
				res = scSubmitReport(m_interface, m_report, /*m_host?gsi_true:gsi_false*/gsi_true, &m_certificate, &m_privateData, SubmitReportCallback, 0, this);				
				if(res != SCResult_NO_ERROR)
				{
					TO_GAME(&CGSStatsTrack::GC_Error, this, res);
					fail = true;
					break;
				}
			}
			break;
		case eSPA_Reset:
			{
				m_toReport.resize(0);
				CleanUp();
				//this should/will always 'fail'
				fail = true;
			}
			break;
		}
		if(fail)
		{
			if(VERBOSE)
				NetLog("GameSpy Stats : failed %d",pa);
			m_pending.erase(m_pending.begin());
		}
		else
		{
			break;
		}
	}while( !m_pending.empty() );
}



void CGSStatsTrack::PrepareReport()
{
	//iterate through players and get rid of ones with invalid CID
	
	for(TPlayerStatsMap::iterator it = m_playerData.begin(), eit = m_playerData.end(); it!=eit; ++it)
	{
		if(it->second.m_connectionId[0] != 0)//only valid connection id
		{
			m_toReport.push_back(it->second);
			it->second = SPlayerData(it->second.m_playerId);
		}
	}
	/* matchless sessions test code
	if(m_connectionId[0])
	{
		bool found = false;
		for(int i=0; i<m_toReport.size(); ++i)
		{
			bool equal = true;
			for(int j=0; j<SC_CONNECTION_GUID_SIZE; ++j)
			{
				if(m_toReport[i].m_connectionId[j] != m_connectionId[j])
				{
					equal = false;
					break;
				}
			}
			if(equal)
				found = true;
		}
		if(!found)
		{
			SPlayerData data;
			memcpy(data.m_connectionId, m_connectionId, SC_CONNECTION_GUID_SIZE);
			data.m_values.insert(std::make_pair(int(4), SValue("Server")));
			m_toReport.push_back(data);
		}
	}
	*/
}

int CGSStatsTrack::AddPlayer(int plr)
{
	int id = m_playerIdPool++;

	FROM_GAME(&CGSStatsTrack::NC_AddPlayer, this, id, plr);
	return id;
}

int CGSStatsTrack::AddTeam(int team)
{
	int id = m_teamIdPool++;

	FROM_GAME(&CGSStatsTrack::NC_AddTeam, this, id, team);
	return id;
}

void  CGSStatsTrack::PlayerDisconnected(int id)
{
	FROM_GAME(&CGSStatsTrack::NC_PlayerDisconnected, this, id);
}

void  CGSStatsTrack::PlayerConnected(int id)
{
	FROM_GAME(&CGSStatsTrack::NC_PlayerConnected, this, id);
}

void  CGSStatsTrack::StartGame()
{
	FROM_GAME(&CGSStatsTrack::NC_StartGame, this);
}

void  CGSStatsTrack::EndGame()
{
	FROM_GAME(&CGSStatsTrack::NC_EndGame, this);
}

void  CGSStatsTrack::Reset()
{
	m_playerIdPool = 1;
	m_teamIdPool = 1;
	FROM_GAME(&CGSStatsTrack::NC_Reset, this);
}

void CGSStatsTrack::SetServerValue(int key, const char* value)
{
	FROM_GAME(&CGSStatsTrack::NC_SetServerValue, this, key, TGSFixedString(value));
}

void CGSStatsTrack::SetPlayerValue(int id, int key, const char* value)
{
	FROM_GAME(&CGSStatsTrack::NC_SetPlayerValue, this, id, key, TGSFixedString(value));
}

void CGSStatsTrack::SetTeamValue(int id, int key, const char* value)
{
	FROM_GAME(&CGSStatsTrack::NC_SetTeamValue, this, id, key, TGSFixedString(value));
}

void CGSStatsTrack::SetServerValue(int key, int value)
{
	FROM_GAME(&CGSStatsTrack::NC_SetServerIntValue, this, key, value);
}

void CGSStatsTrack::SetPlayerValue(int id, int key, int value)
{
	FROM_GAME(&CGSStatsTrack::NC_SetPlayerIntValue, this, id, key, value);
}

void CGSStatsTrack::SetTeamValue(int id, int key, int value)
{
	FROM_GAME(&CGSStatsTrack::NC_SetTeamIntValue, this, id, key, value);
}

void CGSStatsTrack::SetServerValue(int key, float value)
{
	FROM_GAME(&CGSStatsTrack::NC_SetServerFloatValue, this, key, value);
}

void CGSStatsTrack::SetPlayerValue(int id, int key, float value)
{
	FROM_GAME(&CGSStatsTrack::NC_SetPlayerFloatValue, this, id, key, value);
}

void CGSStatsTrack::SetTeamValue(int id, int key, float value)
{
	FROM_GAME(&CGSStatsTrack::NC_SetTeamFloatValue, this, id, key, value);
}

bool  CGSStatsTrack::IsLoggedIn()const
{
	return m_loggedIn;
}

bool  CGSStatsTrack::IsSessionStarted()const
{
	return m_sessionCreated;
}

bool  CGSStatsTrack::HasConnectionId()const
{
	return m_haveConnectionId;
}

bool  CGSStatsTrack::GetLocalCertificate(GSLoginCertificate& cert)
{
	if(m_loggedIn)
		cert = m_certificate;
	else
		return false;
	return true;
}

void CGSStatsTrack::SetSessionId(const gsi_u8 sessionId[])
{
	if(VERBOSE)
		NetLog("GameSpy Stats : session id received");
	if(!gEnv->bServer)
	{
		memcpy(m_sessionId,sessionId,SC_CONNECTION_GUID_SIZE);
		AddAction(eSPA_Login);
		AddAction(eSPA_SetIntention);
	}
}

void CGSStatsTrack::SetConnectionId(int plridx, const gsi_u8 connectionId[])
{
	if(VERBOSE)
		NetLog("GameSpy Stats : connection id received for %d", plridx);
	for(TPlayerStatsMap::iterator it = m_playerData.begin(),e = m_playerData.end();it!=e;++it)
	{
		SPlayerData &plr_data = it->second;
		if(plr_data.m_playerId == plridx)
		{
			memcpy(plr_data.m_connectionId, connectionId, SC_CONNECTION_GUID_SIZE);
			break;
		}
	}
}

void CGSStatsTrack::SessionCreated()
{
	m_sessionCreated = true;
	//broadcast session id
	for(TPlayerStatsMap::iterator it = m_playerData.begin(),e = m_playerData.end();it!=e;++it)
	{
		SPlayerData &plr_data = it->second;
		if(!m_parent->SendSessionId(plr_data.m_playerId, m_sessionId))
		{
			if(VERBOSE)
				NetWarning("GameSpy Stats : SendSessionId failed for player %d", plr_data.m_playerId);
		}
		else
		{
			NetLog("GameSpy Stats : SendSessionId send for player %d", plr_data.m_playerId);
		}
	}
	OnCompleted(eSPA_StartSession);	
}

bool	CGSStatsTrack::IsDead()const
{
	return false;
}

bool	CGSStatsTrack::IsAvailable()const
{
	return true;
}

void	CGSStatsTrack::Update()
{
	SCResult res = scThink(m_interface);
}

void	CGSStatsTrack::TranslateError(int err)
{
	if(!m_listener)
		return;
	if(VERBOSE)
		NetWarning("GameSpy Stats : Error: code %d.",err);
	
	switch(err)
	{
	case SCResult_NO_AVAILABILITY_CHECK:
		m_listener->OnError(eSTE_other);
		break;
	case SCResult_INVALID_PARAMETERS:
		m_listener->OnError(eSTE_other);
		break;
	case SCResult_NOT_INITIALIZED:
		m_listener->OnError(eSTE_other);
		break;
	case SCResult_CORE_NOT_INITIALIZED:
		m_listener->OnError(eSTE_other);
		break;
	case SCResult_OUT_OF_MEMORY:
		m_listener->OnError(eSTE_other);
		break;
	case SCResult_CALLBACK_PENDING:
		m_listener->OnError(eSTE_other);
		break;
	case SCResult_HTTP_ERROR:
		m_listener->OnError(eSTE_other);
		break;
	case SCResult_UNKNOWN_RESPONSE:
		m_listener->OnError(eSTE_other);
		break;
	case SCResult_RESPONSE_INVALID:
		m_listener->OnError(eSTE_other);
		break;
	case SCResult_REPORT_INCOMPLETE:
		m_listener->OnError(eSTE_other);
		break;
	case SCResult_REPORT_INVALID:
		m_listener->OnError(eSTE_other);
		break;
	case SCResult_SUBMISSION_FAILED:
		m_listener->OnError(eSTE_other);
		break;
	}
}

void	CGSStatsTrack::NC_AddPlayer(int id, int plr_id)
{
	TPlayerStatsMap::iterator it = m_playerData.insert(std::make_pair(id,SPlayerData(plr_id))).first;
	if(VERBOSE)
		NetLog("GameSpy Stats : AddPlayer %d %d", id, plr_id);
	
	NC_PlayerConnected(id);
}

void	CGSStatsTrack::NC_AddTeam(int id, int team_id)
{
	TTeamStatsMap::iterator it = m_teamData.insert(std::make_pair(id,STeamData(team_id))).first;
	if(VERBOSE)
		NetLog("GameSpy Stats : AddTeam %d %d", id, team_id);
}

void  CGSStatsTrack::NC_PlayerDisconnected(int id)
{
	TPlayerStatsMap::iterator it = m_playerData.find(id);
	if(it != m_playerData.end())
	{
		it->second.m_disconnected = true;
		if(it->second.m_connectionId[0] != 0)//we cannot report it
		{
			m_toReport.push_back(it->second);
			it->second = SPlayerData(it->second.m_playerId);
		}
	}
}

void  CGSStatsTrack::NC_PlayerConnected(int id)
{
	TPlayerStatsMap::iterator it = m_playerData.find(id);
	if(it != m_playerData.end())
	{
		int plr_id = it->second.m_playerId;
		it->second.m_disconnected = false;
		if(m_sessionCreated)//game is started, so just 
		{
			if(!m_parent->SendSessionId(plr_id, m_sessionId))
			{
				if(VERBOSE)
					NetWarning("GameSpy Stats : SendSessionId failed for player %d", plr_id);
			}
			else
			{
				NetLog("GameSpy Stats : SendSessionId send for player %d", plr_id);
			}
		}
	}
	else
	{
		NetLog("GameSpy Stats : Connected player not found - id:%d", id);
	}
}

void	CGSStatsTrack::NC_SetServerValue(int key, TGSFixedString value)
{
	m_serverData.m_values.insert(std::make_pair(key, SValue(value)));
}

void	CGSStatsTrack::NC_SetPlayerValue(int id, int key, TGSFixedString value)
{
	TPlayerStatsMap::iterator it = m_playerData.find(id);
	if(it!= m_playerData.end())
		it->second.m_values.insert(std::make_pair(key, SValue(value)));
	else
	{
		NET_ASSERT(it!= m_playerData.end());
	}
}

void	CGSStatsTrack::NC_SetTeamValue(int id, int key, TGSFixedString value)
{
	TTeamStatsMap::iterator it = m_teamData.find(id);
	if(it!= m_teamData.end())
		it->second.m_values.insert(std::make_pair(key, SValue(value)));
	else
	{
		NET_ASSERT(it!= m_teamData.end());
	}
}

void	CGSStatsTrack::NC_SetServerIntValue(int key, int value)
{
	m_serverData.m_values.insert(std::make_pair(key,SValue(value)));
}

void	CGSStatsTrack::NC_SetPlayerIntValue(int id, int key, int value)
{
	TPlayerStatsMap::iterator it = m_playerData.find(id);
	if(it!= m_playerData.end())
		it->second.m_values.insert(std::make_pair(key, SValue(value)));
	else
	{
		NET_ASSERT(it!= m_playerData.end());
	}
}

void	CGSStatsTrack::NC_SetTeamIntValue(int id, int key, int value)
{
	TTeamStatsMap::iterator it = m_teamData.find(id);
	if(it!= m_teamData.end())
		it->second.m_values.insert(std::make_pair(key, SValue(value)));
	else
	{
		NET_ASSERT(it!= m_teamData.end());
	}
}

void	CGSStatsTrack::NC_SetServerFloatValue(int key, float value)
{
	m_serverData.m_values.insert(std::make_pair(key, SValue(value)));
}

void	CGSStatsTrack::NC_SetPlayerFloatValue(int id, int key, float value)
{
	TPlayerStatsMap::iterator it = m_playerData.find(id);
	if(it!= m_playerData.end())
		it->second.m_values.insert(std::make_pair(key, SValue(value)));
	else
	{
		NET_ASSERT(it!= m_playerData.end());
	}
}

void	CGSStatsTrack::NC_SetTeamFloatValue(int id, int key, float value)
{
	TTeamStatsMap::iterator it = m_teamData.find(id);
	if(it!= m_teamData.end())
		it->second.m_values.insert(std::make_pair(key,SValue(value)));
	else
	{
		NET_ASSERT(it!= m_teamData.end());
	}
}

void	CGSStatsTrack::NC_StartGame()
{
	AddAction(eSPA_Login);
	AddAction(eSPA_StartSession);
	AddAction(eSPA_SetIntention);
}

void	CGSStatsTrack::NC_EndGame()
{
	PrepareReport();
	
	AddAction(eSPA_Report);	
	AddAction(eSPA_Reset);
}

void	CGSStatsTrack::NC_Reset()
{
	m_playerData.clear();
	m_teamData.clear();
	AddAction(eSPA_Reset);
}

//////////////////////////////////////////////////////////////////////////
#define CHECKED_CALL(x)																					\
{																																\
	SCResult res = x;																							\
	if(SCResult_NO_ERROR!=res)																		\
	{																															\
		NET_ASSERT(false && "GameSpy Stats : Report fill error");		\
		return res;																									\
	}																															\
}
//////////////////////////////////////////////////////////////////////////

SCResult CGSStatsTrack::AddValues(SCReportPtr report, const TValueMap& values)
{
	for(TValueMap::const_iterator v_it = values.begin(),v_e = values.end();v_it!=v_e;++v_it)
	{
		const SValue& value = v_it->second;
		switch(value.m_type)
		{
		case eVT_Int:
			CHECKED_CALL(scReportAddIntValue(report,v_it->first,value.m_int));
			break;
		case eVT_Float:
			CHECKED_CALL(scReportAddFloatValue(report,v_it->first,value.m_float));
			break;
		case eVT_String:
			CHECKED_CALL(scReportAddStringValue(report,v_it->first,value.m_string.c_str()));
			break;				
		}
	}

	return SCResult_NO_ERROR;
}

SCResult CGSStatsTrack::FillReport(SCReportPtr report)
{
	bool complete = false;//clients will always submit partial report, so ours can't be full as well
	CHECKED_CALL(scReportBeginGlobalData(report));

	CHECKED_CALL(AddValues(report,m_serverData.m_values));

	CHECKED_CALL(scReportBeginPlayerData(report));

	for(int i=0; i<m_toReport.size(); ++i)
	{
		const SPlayerData &plr_data = m_toReport[i];
		CHECKED_CALL(scReportBeginNewPlayer(report));

		CHECKED_CALL(scReportSetPlayerData(report, i, plr_data.m_connectionId, 0, plr_data.m_disconnected?SCGameResult_DISCONNECT:SCGameResult_DRAW, 0, &m_certificate, m_statsAuthdata));

		CHECKED_CALL(AddValues(report,plr_data.m_values));

		complete = complete && !plr_data.m_disconnected;
	}

	CHECKED_CALL(scReportBeginTeamData(report));

	for(TTeamStatsMap::const_iterator it = m_teamData.begin(),e = m_teamData.end();it!=e;++it)
	{
		scReportBeginNewTeam(report);
		const STeamData& team_data = it->second;
		CHECKED_CALL(AddValues(report,team_data.m_values));		
	}

	CHECKED_CALL(scReportEnd(report, /*m_host?gsi_true:gsi_false*/gsi_true, complete?SCGameStatus_COMPLETE:SCGameStatus_PARTIAL));

	return SCResult_NO_ERROR;
}

#undef CHECKED_CALL

void	CGSStatsTrack::GC_Error(SCResult err)
{
	TranslateError(err);
}

void  CGSStatsTrack::GC_LoginError(WSLoginValue err)
{
	if(VERBOSE)
		NetWarning("GameSpy Stats : login error : %d", err);
	/*
		WSLogin_ServerInitFailed,

		WSLogin_UserNotFound,
		WSLogin_InvalidPassword,
		WSLogin_InvalidProfile,
		WSLogin_UniqueNickExpired,

		WSLogin_DBError,
		WSLogin_ServerError,
		WSLogin_FailureMax, // must be the last failure

		// Login result (mLoginResult)
		WSLogin_HttpError = 100,    // ghttp reported an error, response ignored
		WSLogin_ParseError,         // couldn't parse http response
		WSLogin_InvalidCertificate, // login success but certificate was invalid!
		WSLogin_LoginFailed,        // failed login or other error condition
		WSLogin_OutOfMemory,        // could not process due to insufficient memory
		WSLogin_InvalidParameters,  // check the function arguments
		WSLogin_NoAvailabilityCheck,// No availability check was performed
		WSLogin_Cancelled,          // login request was cancelled
		WSLogin_UnknownError        // error occured, but detailed information not available
		*/
}

void  CGSStatsTrack::Init()
{
	if(m_interface)
		return;//already there
	scInitialize(CDKEY_PRODUCT_ID,&m_interface);
	if(!m_updateTimer)
	{
		SCOPED_GLOBAL_LOCK;
		m_updateTimer = TIMER.AddTimer( g_time+UPDATE_INTERVAL, TimerCallback, this );
	}
}

void  CGSStatsTrack::CleanUp()
{
	m_sessionCreated = false;
	m_loggedIn = false;
	m_host = false;
	m_haveConnectionId = false;
	if(m_interface)
	{
		scShutdown(&m_interface);
		m_interface = 0;
	}
	if(m_updateTimer)
	{
		SCOPED_GLOBAL_LOCK;
		TIMER.CancelTimer( m_updateTimer );
		m_updateTimer = 0;
	}
}

void CGSStatsTrack::LoginCallback(GHTTPResult httpResult, WSLoginResponse * response, void * userData)
{
	CGSStatsTrack* pStats = (CGSStatsTrack*)userData;
	if (httpResult != GHTTPSuccess)
	{
		if(VERBOSE)
		{
			NetWarning("GameSpy Stats : GHTTP error %d", httpResult);
		}
	}
	else if(response->mLoginResult != WSLogin_Success)
	{
		if(VERBOSE)
		{
			NetWarning("GameSpy Stats : Login error %d", response->mLoginResult);
		}
		TO_GAME(&CGSStatsTrack::GC_LoginError, pStats, response->mLoginResult);
	}
	else
	{
		memcpy(&pStats->m_certificate, &response->mCertificate, sizeof(GSLoginCertificate));
		memcpy(&pStats->m_privateData, &response->mPrivateData, sizeof(GSLoginPrivateData));
		//once we logged in we can mark ourselves to be ready to share
		pStats->m_loggedIn = true;
	}
	pStats->OnCompleted(eSPA_Login);
}

void CGSStatsTrack::CreateSessionCallback(const SCInterfacePtr interf, GHTTPResult httpResult,  SCResult result,  void *userData)
{
	CGSStatsTrack* pStats = (CGSStatsTrack*)userData;
	if (httpResult == GHTTPSuccess && result == SCResult_NO_ERROR)
	{
		memcpy(pStats->m_sessionId, scGetSessionId(interf), SC_SESSION_GUID_SIZE);
		pStats->SessionCreated();
	}
	else
	{
		if(VERBOSE)
		{
			NetWarning("GameSpy Stats : Session creation error. GHTTP : %d, SCResult : %d", httpResult, result);
		}
	}
}

void CGSStatsTrack::SetIntentionCallback(const SCInterfacePtr interf, GHTTPResult httpResult, SCResult result, void *userData)
{
	CGSStatsTrack* pStats = (CGSStatsTrack*)userData;
	if (httpResult == GHTTPSuccess && result == SCResult_NO_ERROR)
	{		
		memcpy(pStats->m_connectionId, scGetConnectionId(interf), SC_CONNECTION_GUID_SIZE);
		pStats->m_parent->SetLocalStatsConnectionId(pStats->m_connectionId);
		pStats->m_sessionCreated = true;
		pStats->m_haveConnectionId = true;
	}
	else
	{
		if(VERBOSE)
		{
			NetWarning("GameSpy Stats : Intention setting error. GHTTP : %d, SCResult : %d", httpResult, result);
		}
	}
	pStats->OnCompleted(eSPA_SetIntention);
}

void CGSStatsTrack::SetClientIntentionCallback(const SCInterfacePtr interf, GHTTPResult httpResult, SCResult result, void *userData)
{
	CGSStatsTrack* pStats = (CGSStatsTrack*)userData;
	if (httpResult == GHTTPSuccess && result == SCResult_NO_ERROR)
	{		
		memcpy(pStats->m_connectionId, scGetConnectionId(interf), SC_CONNECTION_GUID_SIZE);
		if(pStats->m_parent)
			pStats->m_parent->SetLocalStatsConnectionId(pStats->m_connectionId);
		pStats->m_haveConnectionId = true;
	}
	else
	{
		if(VERBOSE)
		{
			NetWarning("GameSpy Stats : Intention setting error. GHTTP : %d, SCResult : %d", httpResult, result);
		}
	}
	pStats->OnCompleted(eSPA_SetIntention);
}

void CGSStatsTrack::SubmitReportCallback(const SCInterfacePtr interf, GHTTPResult httpResult, SCResult result, void *userData)
{
	CGSStatsTrack* pStats = (CGSStatsTrack*)userData;
	if (httpResult != GHTTPSuccess || result != SCResult_NO_ERROR)
	{	
		if(VERBOSE)
		{
			NetWarning("GameSpy Stats : Submit report error. GHTTP : %d, SCResult : %d", httpResult, result);
		}
	}
	scDestroyReport(pStats->m_report);
	pStats->OnCompleted(eSPA_Report);
}

void CGSStatsTrack::TimerCallback(NetTimerId id,void* obj,CTimeValue)
{
	
	CGSStatsTrack* pStats = (CGSStatsTrack*)obj;
	if(id!= pStats->m_updateTimer)
		return;

	if(!pStats->IsDead())
	{
		pStats->Update();
		SCOPED_GLOBAL_LOCK;
		pStats->m_updateTimer = TIMER.AddTimer( g_time+UPDATE_INTERVAL, TimerCallback, pStats );
	}
}