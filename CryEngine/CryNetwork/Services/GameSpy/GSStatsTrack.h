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

#ifndef __GSSTATSTRACK_H__
#define __GSSTATSTRACK_H__

#pragma once

#include "INetwork.h"
#include "INetworkService.h"
#include "GameSpy.h"
#include "GameSpy/gstats/gstats.h"
#include "GameSpy/sc/sc.h"

enum EStatsPendingAction
{
	eSPA_Login,					//login to service
	eSPA_StartSession,	//start session
	eSPA_SetIntention,	//obtain connectionId
	eSPA_Report,				//submit report
	eSPA_Reset,					//reset all state - be ready to start new session
};

class CGSStatsTrack : public CMultiThreadRefCount, public IStatsTrack
{
private:
	enum EValueType
	{
		eVT_Int,
		eVT_Float,
		eVT_String
	};
	struct SValue
	{
		SValue(const char* str):m_type(eVT_String),m_string(str),m_int(0),m_float(0){}
		SValue(int n):m_type(eVT_Int),m_int(n),m_float(0){}
		SValue(float f):m_type(eVT_Float),m_float(f),m_int(0){}
		SValue(const SValue& v):m_type(v.m_type),m_int(v.m_int),m_float(v.m_float),m_string(v.m_string){}
		EValueType	m_type;
		string			m_string;
		int					m_int;
		float				m_float;
	};
	
	typedef std::map<int,SValue> TValueMap;
	
	struct SValueMap
	{
		TValueMap m_values;
	};

	struct STeamData : public SValueMap
	{
		STeamData(int team):m_teamId(team){}
		int       m_teamId;
	};

	struct SPlayerData : public SValueMap
	{
		SPlayerData():m_playerId(0), m_disconnected(false)
		{
			memset(&m_connectionId, 0, sizeof(m_connectionId));
		}
		SPlayerData(int plr):m_playerId(plr), m_disconnected(false)
		{
			memset(&m_connectionId, 0, sizeof(m_connectionId));
		}

		gsi_u8              m_connectionId[SC_CONNECTION_GUID_SIZE];
		bool								m_disconnected;
		int                 m_playerId;
	};

	typedef std::map<int,SValueMap>   TStatsMap;
	typedef std::map<int,STeamData>   TTeamStatsMap;
	typedef std::map<int,SPlayerData> TPlayerStatsMap;
	typedef std::vector<SPlayerData>  TPlayerStatsVector;

public:
	CGSStatsTrack(CGameSpy* gs, int version);
	~CGSStatsTrack();
	
	int	GetVersion()const;

	virtual int   AddPlayer(int plr_id);//player
	virtual int   AddTeam(int id);//team
	virtual void  PlayerDisconnected(int id);
	virtual void  PlayerConnected(int id);

	virtual void  StartGame();//clear data
	virtual void  EndGame();//send data off
	virtual void  Reset();//reset game... don't send anything

	virtual void  SetServerValue(int key, const char* value);
	virtual void  SetPlayerValue(int, int key, const char* value);
	virtual void  SetTeamValue(int, int key, const char* value);

	virtual void  SetServerValue(int key, int value);
	virtual void  SetPlayerValue(int, int key, int value);
	virtual void  SetTeamValue(int, int key, int value);

	virtual void  SetServerValue(int key, float value);
	virtual void  SetPlayerValue(int, int key, float value);
	virtual void  SetTeamValue(int, int key, float value);

	uint  Login(const char* uniquenick, const char* password);//called from within CryNetwork
	//do we need this?
	void  Logoff();//called from within CryNetwork

	bool IsLoggedIn()const;
	bool IsSessionStarted()const;
	bool HasConnectionId()const;
	bool GetLocalCertificate(GSLoginCertificate& cert);
	void SetSessionId(const gsi_u8 sessionId[]);
	void SetConnectionId(int plridx, const gsi_u8 connectionId[]);
	void SessionCreated();

	void  SetListener(IStatsTrackListener* lst);

	bool IsPending(EStatsPendingAction pa);
	void OnCompleted(EStatsPendingAction pa);
	void AddAction(EStatsPendingAction pa);
	void ProcessPending();

	void PrepareReport();

	bool	IsDead()const;
	virtual bool IsAvailable()const;

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CGSStatsTrack");

		pSizer->Add(*this);
	}
private:
	void	Update();
	void	TranslateError(int err);

	void	NC_AddPlayer(int id, int plr_id);
	void	NC_AddTeam(int id, int team_id);
	void  NC_PlayerDisconnected(int id);
	void  NC_PlayerConnected(int id);

	void	NC_SetServerValue(int key, TGSFixedString value);
	void	NC_SetPlayerValue(int id, int key, TGSFixedString value);
	void	NC_SetTeamValue(int id, int key, TGSFixedString value);

	void	NC_SetServerIntValue(int key, int value);
	void	NC_SetPlayerIntValue(int id, int key, int value);
	void	NC_SetTeamIntValue(int id, int key, int value);

	void	NC_SetServerFloatValue(int key, float value);
	void	NC_SetPlayerFloatValue(int id, int key, float value);
	void	NC_SetTeamFloatValue(int id, int key, float value);

	void	NC_StartGame();
	void	NC_Reset();
	void	NC_EndGame();
	

	void  Init();
	void  CleanUp();
	SCResult  FillReport(SCReportPtr report);
	SCResult  AddValues(SCReportPtr report, const TValueMap& values);

	void	GC_Error(SCResult);
	void  GC_LoginError(WSLoginValue);

	//GameSpy callbacks

	static void LoginCallback(GHTTPResult httpResult, WSLoginResponse * response, void * userData);
	static void CreateSessionCallback(const SCInterfacePtr interf, GHTTPResult httpResult, SCResult result, void *userData);
	static void SetIntentionCallback(const SCInterfacePtr interf, GHTTPResult httpResult, SCResult result, void *userData);
	static void SetClientIntentionCallback(const SCInterfacePtr interf, GHTTPResult httpResult, SCResult result, void *userData);
	static void SubmitReportCallback(const SCInterfacePtr interf, GHTTPResult httpResult, SCResult result, void *userData);

	static void TimerCallback(NetTimerId,void*,CTimeValue);

	NetTimerId			      m_updateTimer;

	SValueMap             m_serverData;
	TTeamStatsMap         m_teamData;
	TPlayerStatsMap       m_playerData;
	IStatsTrackListener*  m_listener;

	TPlayerStatsVector		m_toReport;

	// GameSpy stuff
	SCInterfacePtr        m_interface;

	//Login
	GSLoginCertificate    m_certificate;//local data
	GSLoginPrivateData    m_privateData;//local data
	bool                  m_loggedIn;
	bool									m_host;

	//Stats data
	gsi_u8  m_sessionId[SC_SESSION_GUID_SIZE];
	gsi_u8  m_connectionId[SC_CONNECTION_GUID_SIZE];
	gsi_u8  m_statsAuthdata[16];
	bool    m_sessionCreated;
	bool    m_haveConnectionId;

	std::vector<EStatsPendingAction>	m_pending;//requests to be executed
	
	CGameSpy*             m_parent;
	int										m_version;

	SCReportPtr						m_report;


	//CAUTION: these two fields are owned by main thread  
	int	    m_playerIdPool;
	int	    m_teamIdPool;
};

#endif /*__GSSTATSTRACK_H__*/