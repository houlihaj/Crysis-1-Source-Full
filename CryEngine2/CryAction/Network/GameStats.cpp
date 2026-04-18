/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 17:11:2006   15:38 : Created by Stas Spivakov

*************************************************************************/
#include "StdAfx.h"
#include "GameStats.h"
#include <INetwork.h>
#include <INetworkService.h>
#include "IGameFramework.h"
#include "IGameRulesSystem.h"
#include "CryAction.h"
#include "GameContext.h"
#include "ILevelSystem.h"
#include "GameServerNub.h"
#include "GameStatsConfig.h"

const float REPORT_INTERVAL = 1.0f;//each second send state to network engine
const float UPDATE_INTERVAL = 20.0f;

const bool	DEBUG_VERBOSE = true;

struct CGameStats::Listener
	: public IServerReportListener, 
		public IStatsTrackListener
{
	virtual void OnError(EServerReportError err)
	{
		switch(err)
		{
		case eSRE_socket:
			GameWarning("Server report error : socket.");
			break;
		case eSRE_connect:
			GameWarning("Server report error : could not connect to master server.");
			break;
		case eSRE_noreply:
			GameWarning("Server report error : no reply from master server, check internet connection or firewall settings.");
			break;
		}
	}

	virtual void OnPublicIP(uint ip,unsigned short port)
	{
		string ip_str;
		ip_str.Format("%d.%d.%d.%d",(ip>>24)&0xFF,(ip>>16)&0xFF,(ip>>8)&0xFF,ip&0xFF);
		CryLog("Server's public address is %s:%d",ip_str.c_str(),port);
		ip_str += ":";
		ip_str += gEnv->pConsole->GetCVar("sv_port")->GetString();
		CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_serverIp, port, ip_str));
	}

	virtual void OnError(EStatsTrackError)
	{
	}
	CGameStats *m_pStats;
};

static const int MAX_INT = 0x7FFFFFFF;//maximum int (spelled by nonosuit voice)

struct CGameStats::SStatsTrackHelper
{
	SStatsTrackHelper(CGameStats* p):m_parent(p),m_errors(0){}

	template<class T>
	void ServerValue(const char* key, T value)
	{
		int id = m_parent->m_config->GetKeyId(key);
		if(id != -1)
			m_parent->m_statsTrack->SetServerValue(id,value);
		else
			m_errors.push_back(key);
	}

	template<class T>
	void PlayerValue(int idx, const char* key, T value)
	{
		int id = m_parent->m_config->GetKeyId(key);
		if(id != -1)
			m_parent->m_statsTrack->SetPlayerValue(idx, id, value);
		else
			m_errors.push_back(key);
	}

	void PlayerCatValue(int idx, const char* cat, const char* key, int value)
	{
		int mod = m_parent->m_config->GetCategoryMod(cat);
		int	code = m_parent->m_config->GetCodeByKeyName(cat, key);
		int id = m_parent->m_config->GetKeyId(key);
		if(id != -1 && code != -1 && mod > 1)
		{
			int64 val = (value*mod + code);
			if(val > MAX_INT)
			{//we need to clamp value so it will be biggest possible value with correct remainder from division by mod
				val = MAX_INT - (MAX_INT%mod) + code;
				if(val > MAX_INT)//still too big
					val -= mod;
			}
			m_parent->m_statsTrack->SetPlayerValue(idx, id, val);
		}
		else
			m_errors.push_back(key);
	}

	template<class T>
	void TeamValue(int idx, const char* key, T value)
	{
		int id = m_parent->m_config->GetKeyId(key);
		if(id != -1)
			m_parent->m_statsTrack->SetTeamValue(idx, id, value);
		else
			m_errors.push_back(key);
	}
	
	CGameStats*					m_parent;
	std::vector<string>	m_errors;
};

struct CGameStats::SPlayerStats : public _reference_target_t
{
	struct SVehicles
	{
		struct SVehicle
		{
			SVehicle(const string& name):m_name(name){}
			string			m_name;//class Name
			CTimeValue	m_used;
		};
		struct SVehicleCompare
		{
			const bool operator()(const SVehicle& a, const string& b)	{ return a.m_name<b; }
			const bool operator()(const string& a, const SVehicle& b)	{ return a<b.m_name; }
		};
		typedef std::vector<SVehicle> TVehicles;
		SVehicles():m_curVehicle(m_vehicles.end()), m_inVehicle(false)
		{}

		void Enter(const char* vehicle, const CTimeValue& t)
		{
			if(m_curVehicle != m_vehicles.end() && m_curVehicle->m_name == vehicle)//entered current vehicle
			{
				return;
			}
			//find it and add if needed
			TVehicles::iterator it = std::lower_bound(m_vehicles.begin(), m_vehicles.end(), CONST_TEMP_STRING(vehicle), SVehicleCompare());
			if(it == m_vehicles.end())
			{
				m_curVehicle = m_vehicles.insert(m_vehicles.end(), SVehicle(vehicle));
			}
			else
			{
				if(it->m_name == vehicle)
				{
					m_curVehicle = it;
				}
				else
				{
					m_curVehicle = m_vehicles.insert(it, SVehicle(vehicle));
				}
			}
			m_inVehicle = true;
			m_enteredVehicle = t;
		}

		void Leave(const char* vehicle, const CTimeValue& t)
		{
			if(m_curVehicle != m_vehicles.end())
			{
				assert(!vehicle || m_curVehicle->m_name == vehicle);
				m_curVehicle->m_used += t-m_enteredVehicle;
				m_curVehicle = m_vehicles.end();
			}
			m_inVehicle = false;
		}

		bool	m_inVehicle;
		CTimeValue m_enteredVehicle;
		TVehicles m_vehicles;
		TVehicles::iterator m_curVehicle;
	};
	
	struct SWeapons
	{
		struct SWeapon
		{
			SWeapon(const string& n):m_name(n),m_kills(0),m_shots(0),m_hits(0),m_reloads(0){}
			string			m_name;//class Name
			CTimeValue	m_used;

			int					m_kills;
			int					m_shots;
			int					m_hits;
			int					m_reloads;
		};
		typedef std::vector<SWeapon> TWeapons;
		
		SWeapons():m_curWeapon(m_weapons.end())
		{
		}

		struct SWeaponCompare
		{
			const bool operator()(const SWeapon& a, const string& b)	{ return a.m_name<b; }
			const bool operator()(const string& a, const SWeapon& b)	{ return a<b.m_name; }
		};

		void SelectWeapon(const char* name, const CTimeValue& time)
		{
			if(m_curWeapon != m_weapons.end() && m_curWeapon->m_name == name)//entered current vehicle
			{
				return;
			}
			DeselectWeapon(time);//deselect old one
			//find it and add if needed
			TWeapons::iterator it = std::lower_bound(m_weapons.begin(), m_weapons.end(), CONST_TEMP_STRING(name), SWeaponCompare());
			if(it == m_weapons.end())
			{
				m_curWeapon = m_weapons.insert(m_weapons.end(), SWeapon(name));
			}
			else
			{
				if(it->m_name == name)
				{
					m_curWeapon = it;
				}
				else
				{
					m_curWeapon = m_weapons.insert(it, SWeapon(name));
				}
			}
			m_selected = time;
		}

		void DeselectWeapon(const CTimeValue& t)
		{
			if(m_curWeapon!=m_weapons.end())
			{
				m_curWeapon->m_used += t-m_selected; 
				m_curWeapon = m_weapons.end();
			}
		}

		void Kill(const char* name)
		{
			if(m_curWeapon!=m_weapons.end() && m_curWeapon->m_name == name)
			{
				m_curWeapon->m_kills++;
			}
			else
			{
				TWeapons::iterator it = std::lower_bound(m_weapons.begin(), m_weapons.end(), CONST_TEMP_STRING(name), SWeaponCompare());
				if(it != m_weapons.end() && it->m_name == name)
					it->m_kills++;
			}
		}

		void Shot(int pel)
		{
			if(m_curWeapon!=m_weapons.end())
			{
				m_curWeapon->m_shots += pel;
			}
		}

		void Hit()
		{
			if(m_curWeapon!=m_weapons.end())
			{
				m_curWeapon->m_hits++;
			}
		}

		void Reload()
		{
			if(m_curWeapon!=m_weapons.end())
			{
				m_curWeapon->m_reloads++;
			}
		}

		TWeapons		m_weapons;
		TWeapons::iterator	m_curWeapon;
		CTimeValue	m_selected;
	};

	struct SSuitMode
	{
		SSuitMode():
			m_currentMode(-1)
		{
			std::fill(m_kills, m_kills+4, 0);
		}
		void SwitchMode(int mode, const CTimeValue& time)
		{
			if (m_currentMode>=0 && m_currentMode<4)
			{
				m_used[m_currentMode] += time-m_lastTime;
			}
			m_currentMode = mode;
			m_lastTime = time;
		}
		void OnKill()
		{
			if (m_currentMode>=0 && m_currentMode<4)
				m_kills[m_currentMode]++;
		}
		CTimeValue	m_lastTime;
		int					m_currentMode;
		int					m_kills[4];
		CTimeValue	m_used[4];
	};

	SPlayerStats():
		m_kills(0),
		m_deaths(0),
		m_shots(0),
		m_hits(0),
		m_melee(0),
		m_revives(0),
		m_inVehicle(false)
	{}
	
	void EnterVehicle(const char* vehicle, const CTimeValue& t)
	{
		m_weapons.DeselectWeapon(t); //de-select weapon while in vehicle
		m_vehicles.Enter(vehicle, t);
	}

	void LeaveVehicle(const char* vehicle, const CTimeValue& t)
	{
		m_vehicles.Leave(vehicle, t);
	}

	void SelectSuitMode(int mode, const CTimeValue& t)
	{
		m_suit.SwitchMode(mode, t);
	}

	void ClientKill(const char* wep, const CTimeValue& time)
	{
		m_suit.OnKill();
		if(wep)
			m_weapons.Kill(wep);
	}

	void ClientDeath(const CTimeValue& time)
	{
		m_suit.SwitchMode(-1, time);//switch off
		m_vehicles.Leave(0, time);
	}

	void ClientRevive(const CTimeValue& time)
	{	
		
	}
	
	void SelectWeapon(const char* name, const CTimeValue& time)
	{
		m_weapons.SelectWeapon(name, time);
	}

	void Spectator(bool spectator, const CTimeValue& time)
	{
		if(spectator)
			m_weapons.DeselectWeapon(time);
	}

	void End(const CTimeValue& time)
	{
		m_weapons.DeselectWeapon(time);
		m_vehicles.Leave(0, time);
		m_suit.SwitchMode(-1, time);
	}

	void Dump()const
	{
		CTimeValue time = gEnv->pTimer->GetFrameStartTime();
		CryLog("$3=====Dumping player stats====");
		CryLog("Kills: %d",m_kills);
		CryLog("Deaths: %d",m_deaths);
		CryLog("Shots: %d",m_shots);
		CryLog("Hits: %d",m_hits);
		CryLog("Melee: %d",m_melee);
		CryLog("Revive: %d",m_revives);
		CryLog("Playtime: %d sec", int((time-m_started).GetSeconds()));
		CryLog("$3====Suit modes :");
		for(int i=0;i<4;++i)
		{
			int suit_time = 0;
			if(m_suit.m_currentMode == i)
			{
				suit_time = int((time - m_suit.m_lastTime + m_suit.m_used[i]).GetSeconds());
			}
			else
			{
				suit_time = int(m_suit.m_used[i].GetSeconds());
			}

			CryLog("   Mode (%d) Time: %d sec Kills: %d", i, suit_time, m_suit.m_kills[i]);
		}

		CryLog("$3====Vehicles :");
		for(int i=0;i<m_vehicles.m_vehicles.size();++i)
		{
			int vehicle_time = 0;
			if(m_vehicles.m_inVehicle && m_vehicles.m_curVehicle == (m_vehicles.m_vehicles.begin()+i))
			{
				vehicle_time = int((time - m_vehicles.m_enteredVehicle + m_vehicles.m_vehicles[i].m_used).GetSeconds());
			}
			else
			{
				vehicle_time = int(m_vehicles.m_vehicles[i].m_used.GetSeconds());
			}
			CryLog("   Vehicle %s Time: %d sec", m_vehicles.m_vehicles[i].m_name.c_str(), vehicle_time);
		}

		CryLog("$3====Weapons :");
		for(int i=0;i<m_weapons.m_weapons.size();++i)
		{
			const SWeapons::SWeapon &wep =m_weapons.m_weapons[i];
			int weapon_time = 0;
			if(!m_vehicles.m_inVehicle && m_weapons.m_curWeapon == (m_weapons.m_weapons.begin()+i))
			{
				weapon_time = int((wep.m_used + time - m_weapons.m_selected).GetSeconds());
			}
			else
			{
				weapon_time = int(wep.m_used.GetSeconds());
			}
			CryLog("   Weapon %s Time: %d sec shots %d hits %d kills %d", wep.m_name.c_str(), weapon_time, wep.m_shots, wep.m_hits, wep.m_kills);
		}
		CryLog("$3=====End player stats dump===");
	}

	int		m_kills;
	int		m_deaths;
	int		m_shots;
	int		m_hits;
	int   m_melee;
	int		m_revives;
	
	bool	m_inVehicle;
	
	CTimeValue	m_started;
	SVehicles		m_vehicles;
	SWeapons		m_weapons;
	SSuitMode		m_suit;
	bool				m_dead;

};


CGameStats::CGameStats(CCryAction *pGameFramework)
: m_pGameFramework(pGameFramework),
	m_pActorSystem(pGameFramework->GetIActorSystem()),
	m_statsTrack(0),
	m_serverReport(0),
	m_pListener(new Listener()),
	m_lastUpdate(0.0f),
	m_lastReport(0.0f),
  m_playing(false),
	m_stateChanged(false),
	m_startReportNeeded(false),
	m_reportStarted(false)
{
	m_pListener->m_pStats = this;

	CCryAction::GetCryAction()->GetIGameplayRecorder()->RegisterListener(this);
	CCryAction::GetCryAction()->GetILevelSystem()->AddListener(this);
	CCryAction::GetCryAction()->RegisterListener(this, "GameStats",FRAMEWORKLISTENERPRIORITY_DEFAULT);
	m_config = CCryAction::GetCryAction()->GetGameStatsConfig();
	if(!gEnv->bServer)//we may not have a chance to do that
		Init();
}

CGameStats::~CGameStats()
{
	if(m_serverReport)
		m_serverReport->StopReporting();

	delete m_pListener;
	m_pListener=0;

	CCryAction::GetCryAction()->GetIGameplayRecorder()->UnregisterListener(this);
	CCryAction::GetCryAction()->GetILevelSystem()->RemoveListener(this);
	CCryAction::GetCryAction()->UnregisterListener(this);
}

static const struct  
{
	EGameplayEvent key;
	const char*    name;
}
gEventNames[]={{eGE_GameReset,						"eGE_GameReset"},
							{eGE_GameStarted,						"eGE_GameStarted"},
							{eGE_GameEnd,								"eGE_GameEnd"},
							{eGE_Connected,							"eGE_Connected"},
							{eGE_Disconnected,					"eGE_Disconnected"},
							{eGE_Renamed,								"eGE_Renamed"},
							{eGE_ChangedTeam,						"eGE_ChangedTeam"},
							{eGE_Died,									"eGE_Died"},
							{eGE_Scored,								"eGE_Scored"},
							{eGE_Currency,							"eGE_Currency"},
							{eGE_Rank,									"eGE_Rank"},
							{eGE_Spectator,							"eGE_Spectator"},
							{eGE_ScoreReset,						"eGE_ScoreReset"},
																					
							{eGE_AttachedAccessory,			"eGE_AttachedAccessory"},
																					
							{eGE_ZoomedIn,							"eGE_ZoomedIn"},
							{eGE_ZoomedOut,							"eGE_ZoomedOut"},
																					
							{eGE_Kill,									"eGE_Kill"},
							{eGE_Death,									"eGE_Death"},
							{eGE_Revive,								"eGE_Revive"},
																					
							{eGE_SuitModeChanged,				"eGE_SuitModeChanged"},
																					
							{eGE_Hit,										"eGE_Hit"},
							{eGE_Damage,								"eGE_Damage"},
																					
							{eGE_WeaponHit,							"eGE_WeaponHit"},
							{eGE_WeaponReload,					"eGE_WeaponReload"},
							{eGE_WeaponShot,						"eGE_WeaponShot"},
							{eGE_WeaponMelee,						"eGE_WeaponMelee"},
							{eGE_WeaponFireModeChanged,	"eGE_WeaponFireModeChanged"},
																					
							{eGE_ItemSelected,					"eGE_ItemSelected"},
							{eGE_ItemPickedUp,					"eGE_ItemPickedUp"},
							{eGE_ItemDropped,						"eGE_ItemDropped"},
							{eGE_ItemBought,						"eGE_ItemBought"},
																					
							{eGE_EnteredVehicle,				"eGE_EnteredVehicle"},
							{eGE_LeftVehicle,						"eGE_LeftVehicle"}};

static const unsigned int gEventNamesNum = sizeof(gEventNames)/sizeof(gEventNames[0]);

void CGameStats::OnGameplayEvent(IEntity *pEntity, const GameplayEvent &event)
{
/*	for(int i=0;i<gEventNamesNum;++i)
	{
		if(gEventNames[i].key == event.event)
		{
			CryLog("GameStats : Event %s",gEventNames[i].name);
		}
	}*/

	int e_id = pEntity ? (int) pEntity->GetId() : 0;
	switch(event.event)
	{
	case eGE_GameStarted:
		StartGame(event.value!=0);
		break;
	case eGE_GameEnd:
		EndGame(event.value!=0);
		break;
	case eGE_Renamed:
		SetName(e_id, event.description);
		break;
	case eGE_Scored:
		SetScore(e_id, event.description, (int)event.value);
		break;
  case eGE_Kill:
	case eGE_Death:
	case eGE_WeaponShot:
	case eGE_WeaponHit:
	case eGE_SuitModeChanged:
	case eGE_WeaponMelee:
	case eGE_LeftVehicle:
	case eGE_EnteredVehicle:
	case eGE_ItemSelected:
	case eGE_WeaponReload:
		if(pEntity)
			ProcessPlayerStat(pEntity,event);
		break;
	case eGE_Connected:
		{
			bool restored = event.value!=0.0f;
			int* status = (int*)event.extra;
			NewPlayer(e_id, status[0], status[1]!=0, restored);
		}
		break;
  case eGE_ChangedTeam:
    SetTeam(e_id,(int)event.value);
    break;
  case eGE_Spectator:
    SetSpectator(e_id,(int)event.value);
		if(pEntity)
			ProcessPlayerStat(pEntity,event);
    break;
	case eGE_Disconnected:
		RemovePlayer(e_id, event.value!=0);
		break;
	case eGE_ScoreReset:
		ResetScore(e_id);
		break;
  case eGE_Rank:
    SetRank(e_id,(int)event.value);
    break;
	case eGE_GameReset:
		GameReset();
		break;
	default:
		break;
	}
}


void CGameStats::OnActionEvent(const SActionEvent& event)
{
	if(event.m_event == eAE_disconnected && !gEnv->bServer)//on disconnect submit our stats
	{
		if(m_statsTrack && m_playing)
		{
			/*for(PlayerStatsMap::iterator it = m_playerMap.begin(), eit = m_playerMap.end();it!=eit; ++it)
			{
				SubmitPlayerStats(it->second);
				m_statsTrack->PlayerDisconnected(it->second.id);
				it->second.stats = new SPlayerStats();
			}*/
			//clients are not submitting any date if disconnected occasionally
			m_statsTrack->Reset();
		}
	}
}

void CGameStats::OnLevelNotFound(const char *levelName)
{
	m_startReportNeeded = false;
}

void CGameStats::OnLoadingStart(ILevelInfo *pLevel)
{
	if(gEnv->bServer)
	{
		ReportGame();

		if (pLevel && m_serverReport)
		{
			if(*(pLevel->GetDisplayName()))
				m_serverReport->SetServerValue("mapname",pLevel->GetDisplayName());
			else
				m_serverReport->SetServerValue("mapname",pLevel->GetName());
		}

		if(m_startReportNeeded)
		{
			if(m_serverReport && !m_reportStarted)
			{
				m_serverReport->StartReporting(gEnv->pGame->GetIGameFramework()->GetServerNetNub(), m_pListener);
				ReportSession();
				m_serverReport->Update();
				
				//need to do this to fire out update packets
				gEnv->pNetwork->SyncWithGame(eNGS_FrameStart);
				gEnv->pNetwork->SyncWithGame(eNGS_FrameEnd);
				gEnv->pTimer->UpdateOnFrameStart();

				m_reportStarted = true;
			}
		}

		if(m_serverReport)
		{
			m_serverReport->Update();
			m_stateChanged = false;
		}
	}
}

void CGameStats::OnLoadingComplete(ILevel *pLevel)
{
}

void CGameStats::OnLoadingError(ILevelInfo *pLevel, const char *error)
{
	m_startReportNeeded = false;
}

void CGameStats::OnLoadingProgress(ILevelInfo *pLevel, int progressAmount)
{
}

void CGameStats::StartSession()
{
  if(!m_statsTrack || !m_serverReport) 
    Init();
	m_startReportNeeded = true;
	ReportSession();
}

void CGameStats::EndSession()
{
	m_startReportNeeded = false;
	m_reportStarted = false;
  if(m_serverReport)
  {
    m_serverReport->StopReporting();
  }
}

void CGameStats::StartGame(bool server)
{
	if(!server && gEnv->bServer)//we simply ignore client events on server
		return;

	if(IGameRulesSystem* pGR = gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem())
	{
		IGameRules *pR = pGR->GetCurrentGameRules();
		if(pR)
		{
			IEntityScriptProxy *pScriptProxy=static_cast<IEntityScriptProxy *>(pR->GetEntity()->GetProxy(ENTITY_PROXY_SCRIPT));
			if (pScriptProxy)
			{
				string gameState = pScriptProxy->GetState();
				if(gameState=="InGame")
				{
					m_playing=true;
					m_gameMode = pR->GetEntity()->GetClass()->GetName();
				}
			}		
		}		
	}

	if(!m_statsTrack || !m_serverReport) 
		Init();
  
	if(m_serverReport)
	{
		ReportGame();
		m_serverReport->Update();
	}
	
	if(m_playing)
	{
		if (ILevel * pLevel = gEnv->pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel())
			if (ILevelInfo * pLevelInfo = pLevel->GetLevelInfo())
			{
				m_mapName = pLevelInfo->GetName();//we only need pure level name here
				const char* p = strrchr(m_mapName.c_str(),'/');
				if(p && strlen(p)>1)
				{
					m_mapName = p+1;
					char* pStr = const_cast<char*>(m_mapName.data());
					pStr[0] = toupper(m_mapName[0]);
					for(int i=1; i<m_mapName.size(); ++i)
						pStr[i] = tolower(m_mapName[i]);
				}
			}
	}
	

	if(m_statsTrack && m_playing  && gEnv->bServer)
		m_statsTrack->StartGame();
}

void CGameStats::EndGame(bool server)
{
	if(!server && gEnv->bServer)//we simply ignore client events on server
		return;

	if(m_statsTrack && m_playing)
	{
		if(gEnv->bServer)
			SubmitServerStats();
		for(PlayerStatsMap::iterator it = m_playerMap.begin(), eit = m_playerMap.end();it!=eit; ++it)
		{
			SubmitPlayerStats(it->second, gEnv->bServer, it->first == CCryAction::GetCryAction()->GetClientActorId());
			it->second.stats = new SPlayerStats();
		}
		m_statsTrack->EndGame();
	}

	m_playing = false;
	
  ReportGame();

	if(m_serverReport)
		m_serverReport->Update();
}

void CGameStats::NewPlayer(int plr, int team, bool spectator, bool restored)
{
	if(DEBUG_VERBOSE)
		CryLog("CGameStats::NewPlayer %08X %s", plr, restored?"restored":"");
	PlayerStatsMap::iterator it = m_playerMap.find(plr);
	if( it == m_playerMap.end() )
		it = m_playerMap.insert(std::make_pair(plr,SPlayerInfo())).first;
	else
	{
		if(DEBUG_VERBOSE)
			CryLog("CGameStats::NewPlayer restored not found");
	}

	if(m_statsTrack)
	{
		if(restored)
			m_statsTrack->PlayerConnected(it->second.id);
		else
			it->second.id = m_statsTrack->AddPlayer(plr);
	}
	else
		it->second.id = -1;
	it->second.stats = new SPlayerStats;
	it->second.stats->m_started = gEnv->pTimer->GetFrameStartTime();
  
	if(!restored)
	{
		it->second.team = team;
		it->second.rank = 1;
		it->second.spectator = spectator;
		it->second.name = gEnv->pEntitySystem->GetEntity(plr)->GetName();
	}
	
	m_stateChanged = true;

	Report();

	if(m_serverReport && m_playerMap.size() == gEnv->pConsole->GetCVar("sv_maxplayers")->GetIVal())//if server is full, report now
	{
		m_serverReport->Update();
		m_stateChanged = false;
	}
}

void CGameStats::RemovePlayer(int plr, bool keep)
{
	if(DEBUG_VERBOSE)
		CryLog("CGameStats::RemovePlayer %08X %s", plr, keep?"keep":"");
	PlayerStatsMap::iterator it = m_playerMap.find(plr);

	if(it == m_playerMap.end())
		return;

	if(m_statsTrack && m_playing)
	{
		SubmitPlayerStats(it->second, gEnv->bServer, false);
		m_statsTrack->PlayerDisconnected(it->second.id);
		it->second.stats = new SPlayerStats();
	}

	if(!keep)
	{
		m_stateChanged = true;

		int num_players = m_playerMap.size();

		m_playerMap.erase(it);

		Report();

		if(m_serverReport && num_players == gEnv->pConsole->GetCVar("sv_maxplayers")->GetIVal())//if server was full, report now
		{
			m_serverReport->Update();
			m_stateChanged = false;
		}
	}
}

void CGameStats::ResetScore(int plr)
{
	int i=0;
	for(PlayerStatsMap::iterator it = m_playerMap.begin(), eit = m_playerMap.end();it!=eit; ++it, ++i)
	{
		if(it->first != plr)
			continue;
		for (std::map<string, int>::iterator sit=it->second.scores.begin(); sit!=it->second.scores.end(); ++sit)
		{
			sit->second = 0;
			if(m_serverReport)
				m_serverReport->SetPlayerValue(i, sit->first, "0");
		}
		break;
	}
	m_stateChanged = true;
}

void CGameStats::SetName(int plr, const char* name)
{
	PlayerStatsMap::iterator it = m_playerMap.find(plr);

	if(it == m_playerMap.end())
		return;

	it->second.name = name;

	m_stateChanged = true;
}

void CGameStats::SetScore(int plr, const char *desc, int value)
{
	PlayerStatsMap::iterator it = m_playerMap.find(plr);
	
	if(it != m_playerMap.end())
	{
		it->second.scores[desc]=value;
		m_stateChanged = true;
	}
}

void CGameStats::SetTeam(int plr, int value)
{
  PlayerStatsMap::iterator it = m_playerMap.find(plr);

  if(it != m_playerMap.end())
	{
    it->second.team = value;
		m_stateChanged = true;
	}

}

void CGameStats::SetRank(int plr, int value)
{
  PlayerStatsMap::iterator it = m_playerMap.find(plr);

  if(it != m_playerMap.end())
  {
    it->second.rank = value;
		m_stateChanged = true;
  }
}

void CGameStats::SetSpectator(int plr,int value)
{
  PlayerStatsMap::iterator it = m_playerMap.find(plr);

  if(it != m_playerMap.end())
  {
    it->second.spectator = value!=0;
  }
	m_stateChanged = true;
}

void CGameStats::GameReset()
{
	m_playing = false;
	m_playerMap.clear();
	m_teamMap.clear();

	if(m_statsTrack)
		m_statsTrack->Reset();
	m_stateChanged = true;
}

void CGameStats::Connected()
{
	//should not be called
	if(gEnv->bClient && !gEnv->bServer)//pure clients
	{
		if(IGameRulesSystem* pGR = gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem())
		{
			IGameRules *pR = pGR->GetCurrentGameRules();
			if(pR)
			{
				IEntityScriptProxy *pScriptProxy=static_cast<IEntityScriptProxy *>(pR->GetEntity()->GetProxy(ENTITY_PROXY_SCRIPT));
				if (pScriptProxy)
				{
					string gameState = pScriptProxy->GetState();
					if(gameState=="InGame")
					{
						m_playing=true;
						m_gameMode = pR->GetEntity()->GetClass()->GetName();
					}
				}		
			}		
		}
		if(m_statsTrack)
			m_statsTrack->Reset();
	}
}

void CGameStats::Update(CTimeValue gametime)
{
	if(gametime>m_lastUpdate+UPDATE_INTERVAL)
	{
		m_lastUpdate = gametime;
		m_lastReport = gametime;
		Report();

		if(m_stateChanged)
		{
			if(m_serverReport)
				m_serverReport->Update();
			m_stateChanged = false;
		}
	}
	else
	{
		if(gametime>m_lastReport+REPORT_INTERVAL)
		{
			Report();
			m_lastReport = gametime;
		}
	}
}

void CGameStats::Dump()
{
	//dump some info for debug purposes
	if(gEnv->bServer)
	{
		for(PlayerStatsMap::const_iterator it = m_playerMap.begin(), eit = m_playerMap.end(); it != eit; ++it)
		{
			CryLog("$2Stats for player %s", it->second.name.c_str());
			it->second.stats->Dump();
		}
	}
	else
	{
		IActor* pActor = CCryAction::GetCryAction()->GetClientActor();
		if(!pActor)
			return;

		PlayerStatsMap::iterator it = m_playerMap.find(pActor->GetEntityId());
		if(it == m_playerMap.end())
			return;
		const SPlayerStats* plr = it->second.stats;
		plr->Dump();
	}
}

void CGameStats::ReportSession()
{
	if(!m_serverReport)
		return;

	string name;
	ICVar* pName = gEnv->pConsole->GetCVar("sv_servername");
	if(pName)
		name = pName->GetString();
	if(name.empty())
		name = gEnv->pNetwork->GetHostName();

	CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_serverName, 0, name));

	if(gEnv->pConsole->GetCVar("sv_lanonly")->GetIVal())//we're on LAN so report our name
	{
		uint32 ip = gEnv->pNetwork->GetHostLocalIp();
		string ip_str;
		ip_str.Format("%d.%d.%d.%d",(ip>>24)&0xFF,(ip>>16)&0xFF,(ip>>8)&0xFF,ip&0xFF);
		ip_str += ":";
		ip_str += gEnv->pConsole->GetCVar("sv_port")->GetString();
		CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_serverIp, 0, ip_str.c_str()));
	}

	if(m_serverReport)
	{
		m_serverReport->SetServerValue("hostname",name);
		m_serverReport->SetServerValue("hostport",gEnv->pConsole->GetCVar("sv_port")->GetString());

		char strProductVersion[256];
		gEnv->pSystem->GetProductVersion().ToString(strProductVersion);
		m_serverReport->SetServerValue("gamever", strProductVersion);
		m_serverReport->SetServerValue("maxplayers",gEnv->pConsole->GetCVar("sv_maxplayers")->GetString());
		ICVar* pFF = gEnv->pConsole->GetCVar("g_friendlyfireratio");
		m_serverReport->SetServerValue("friendlyfire",pFF?((pFF->GetFVal()!=0)?"1":"0"):0);
		m_serverReport->SetServerValue("dx10",CCryAction::GetCryAction()->GetGameContext()->HasContextFlag(eGSF_ImmersiveMultiplayer)?"1":"0");
		ICVar* pPass = gEnv->pConsole->GetCVar("sv_password");
		bool priv = false;
		if(pPass && strlen(pPass->GetString())!=0)
		{
			priv = true;
		}
		m_serverReport->SetServerValue("password",priv?"1":"0");

		// send current mod details to server too
		SModInfo info;
		if(CCryAction::GetCryAction()->GetModInfo(&info))
		{
			m_serverReport->SetServerValue("modname",info.m_name);
			m_serverReport->SetServerValue("modversion", info.m_version);
		}
	}

	ReportGame();

	if((CCryAction::GetCryAction()->GetILevelSystem()->IsLevelLoaded() && CCryAction::GetCryAction()->IsGameStarted()) && m_startReportNeeded)//otherwise, OnLoadingStart will report it
	{
		if(m_serverReport && !m_reportStarted)//report now
		{
			m_serverReport->StartReporting(gEnv->pGame->GetIGameFramework()->GetServerNetNub(), m_pListener);
			m_reportStarted = true;
		}
	}
}

void CGameStats::ReportGame()
{
	string mode;
	if(IEntity* pGRE = gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRulesEntity())
	{
		mode = pGRE->GetClass()->GetName();
	}
	else
		mode = gEnv->pConsole->GetCVar("sv_gamerules")->GetString();

	string name;
	ICVar* pName = gEnv->pConsole->GetCVar("sv_servername");
	if(pName)
		name = pName->GetString();

	string timelimit;
	ICVar* pVar = gEnv->pConsole->GetCVar("g_timelimit");
	timelimit = pVar?pVar->GetString():"0";

	if(m_serverReport)
	{
		string levelname;
		if(GetLevelName(levelname))
			m_serverReport->SetServerValue("mapname",levelname.c_str());
		m_serverReport->SetServerValue("gametype",mode);

		ICVar* pVar = gEnv->pConsole->GetCVar("g_timelimit");
		m_serverReport->SetServerValue("timelimit",pVar?pVar->GetString():"0");
	}

	Report();
}

void CGameStats::Report()
{
	if(!m_serverReport)
		return;

	int playerCount = m_playerMap.size();
	if (CGameServerNub * pServerNub = CCryAction::GetCryAction()->GetGameServerNub())
		playerCount = pServerNub->GetPlayerCount();

	//All server reporting is done here 
	m_serverReport->SetReportParams(playerCount,m_teamMap.size());
  m_serverReport->SetServerValue("gamemode",m_playing?"game":"pre-game");

	string timeleft("-");
	if(IGameRulesSystem* pGR = gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem())
	{
		IGameRules *pR = pGR->GetCurrentGameRules();
		if(pR && pR->IsTimeLimited() && m_playing)
		{
				timeleft.Format("%.0f",pR->GetRemainingGameTime());
		}
	}

	m_serverReport->SetServerValue("timeleft",timeleft);

	m_serverReport->SetServerValue("numplayers",string().Format("%d",playerCount));
  
  int i=0;

	string mode;
  for(PlayerStatsMap::const_iterator it=m_playerMap.begin();it!=m_playerMap.end();++it)
	{
		static string value;
		m_serverReport->SetPlayerValue(i, "player", it->second.name);
		value.Format("%d",it->second.rank);
    m_serverReport->SetPlayerValue(i, "rank", value);
		value.Format("%d",it->second.team?it->second.team:(it->second.spectator?0:1));
    m_serverReport->SetPlayerValue(i, "team", value);
		for (std::map<string, int>::const_iterator sit=it->second.scores.begin(); sit!=it->second.scores.end(); ++sit)
			m_serverReport->SetPlayerValue(i, sit->first, string().Format("%d",sit->second));
    ++i;
	}
	while (i < playerCount)
	{
		m_serverReport->SetPlayerValue(i, "player", "<connecting>");
		++i;
	}
} 

void CGameStats::ProcessPlayerStat(IEntity* pEntity, const GameplayEvent &event)
{
	PlayerStatsMap::iterator it = m_playerMap.find(pEntity->GetId());
	if(it == m_playerMap.end())
		return;
	SPlayerStats& plr = *(it->second.stats);
	if(gEnv->bServer)
	{
		switch(event.event)
		{
		case eGE_Kill:
			plr.m_kills++;
			break;
		case eGE_Death:
			plr.m_deaths++;
			plr.m_inVehicle = false;
			break;
		case eGE_WeaponShot:
			if(!plr.m_inVehicle)
			{
				if(strstr(event.description, "bullet")!=0 || strcmp(event.description,"shotgunshell")==0 || strcmp(event.description,"alienmount_acmo")==0)//only counting these
					plr.m_shots += int(event.value);
			}
			break;
		case eGE_WeaponHit:
			if(!plr.m_inVehicle)
			{
				EntityId* params = (EntityId*)event.extra;
				IEntity* pTarget = gEnv->pEntitySystem->GetEntity(params[1]);
				if( pTarget && !strcmp(pTarget->GetClass()->GetName(),"Player") )
					plr.m_hits++;
			}
			break;
		case eGE_WeaponMelee:
			plr.m_melee++;
			break;
		case eGE_EnteredVehicle:
			plr.m_inVehicle = true;
			break;
		case eGE_LeftVehicle:
			plr.m_inVehicle = false;
			break;
		}
	}
	if(gEnv->bClient)
	{
		struct SGetTime
		{
			operator const CTimeValue&()
			{
				t = gEnv->pTimer->GetFrameStartTime();
				return t;
			}
			CTimeValue t;
		};

		SGetTime time;

		static string vehicle_name;

		switch(event.event)
		{
		case eGE_Kill:
			{
				IEntity* pW = gEnv->pEntitySystem->GetEntity(EntityId(event.extra));
				plr.ClientKill(pW?pW->GetClass()->GetName():event.description, time);
			}
			break;
		case eGE_Death:
			plr.ClientDeath(time);
			break;
		case eGE_Revive:
			plr.ClientRevive(time);
			break;
		case eGE_SuitModeChanged:
			plr.SelectSuitMode(int(event.value), time);
			break;
		case eGE_EnteredVehicle:
			{
				if( IVehicle* pV = CCryAction::GetCryAction()->GetIVehicleSystem()->GetVehicle((EntityId)event.extra))
				{
					vehicle_name.Format("%s_%s", pV->GetEntity()->GetClass()->GetName(), pV->GetModification());
					plr.EnterVehicle(vehicle_name, time);
				}
			}
			break;
		case eGE_LeftVehicle:
			{
				if( IVehicle* pV = CCryAction::GetCryAction()->GetIVehicleSystem()->GetVehicle((EntityId)event.extra))
				{
					vehicle_name.Format("%s_%s", pV->GetEntity()->GetClass()->GetName(), pV->GetModification());
					plr.LeaveVehicle(vehicle_name, time);
				}
			}
			break;
		case eGE_ItemSelected:
			{
				if( IEntity* pI = gEnv->pEntitySystem->GetEntity(EntityId(event.extra)) )
					plr.SelectWeapon(pI->GetClass()->GetName(), time);
			}			
			break;
		case eGE_Spectator:
			plr.Spectator(event.value != 0, time);
			break;
		}
	}
}

bool CGameStats::GetLevelName(string& mapname)
{
	string levelname;
	if (ILevel * pLevel = gEnv->pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel())
		if (ILevelInfo * pLevelInfo = pLevel->GetLevelInfo())
		{
			levelname = pLevelInfo->GetDisplayName();
			if(levelname.empty())
				levelname = gEnv->pGame->GetIGameFramework()->GetLevelName();
		}
	if(levelname.empty())
		return false;
	else
	{
		mapname = levelname;
		return true;
	}
}

void CGameStats::SubmitPlayerStats(const SPlayerInfo& plr, bool server, bool client)
{
	if(!m_statsTrack)
		return;
	SStatsTrackHelper hlp(this);
	//server stats 

	if(server)
	{
		hlp.PlayerValue(plr.id, "name", plr.name.c_str());
		hlp.PlayerValue(plr.id, "rank", plr.rank);
		hlp.PlayerValue(plr.id, "kills", plr.stats->m_kills);
		hlp.PlayerValue(plr.id, "deaths", plr.stats->m_deaths);
		hlp.PlayerValue(plr.id, "shots", plr.stats->m_shots);
		hlp.PlayerValue(plr.id, "hits", plr.stats->m_hits);
		hlp.PlayerValue(plr.id, "melee", plr.stats->m_melee);
	}

	//here are some client-only stats - mainly times
	if(client)
	{
		CTimeValue time = gEnv->pTimer->GetFrameStartTime();
		plr.stats->End(time);
		int played = int((time - plr.stats->m_started).GetSeconds()+0.5f);
		int played_minutes = played/60;
		hlp.PlayerValue(plr.id,"played",played_minutes);
		static string keyName;
	
		keyName.Format("Played%s",m_gameMode.c_str());
		hlp.PlayerCatValue(plr.id, "mode", keyName, played);
		
		keyName.Format("PlayedMap%s",m_mapName.c_str());
		hlp.PlayerCatValue(plr.id, "map", keyName, played);

		SPlayerStats& stats = *(plr.stats);
		
		for(int i=0; i<4; ++i)
		{
			int used = int(stats.m_suit.m_used[i].GetSeconds()+0.5f);
			if(!used)
				continue;
			keyName.Format("UsedSuit%d",i);
			hlp.PlayerCatValue(plr.id, "suit_mode", keyName, used);
		}

		for(int i=0; i<stats.m_vehicles.m_vehicles.size(); ++i)
		{
			const SPlayerStats::SVehicles::SVehicle& veh = stats.m_vehicles.m_vehicles[i];
			int used = int(veh.m_used.GetSeconds() + 0.5f);
			keyName.Format("UsedVehicle%s",veh.m_name.c_str());
			hlp.PlayerCatValue(plr.id, "vehicle", keyName, used);
		}

		for(int i=0; i<stats.m_weapons.m_weapons.size(); ++i)
		{
			const SPlayerStats::SWeapons::SWeapon& wep= stats.m_weapons.m_weapons[i];
			if(!wep.m_kills)
				continue;

			keyName.Format("WeaponKills%s", wep.m_name.c_str());
			hlp.PlayerCatValue(plr.id, "weapon", keyName, wep.m_kills);
		}
	}

	//Debugging stuff
	if(!hlp.m_errors.empty())
	{
		GameWarning("Stats tracking : SubmitPlayerStats tried to use %d missing key(s)", hlp.m_errors.size());
		for(int i=0; i<hlp.m_errors.size(); ++i)
		{
			GameWarning("     Key %s not found",hlp.m_errors[i].c_str());
		}
	}
}

void CGameStats::SubmitServerStats()
{
	SStatsTrackHelper hlp(this);
	
	string mode;
	if(IEntity* pGRE = gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRulesEntity())
	{
		mode = pGRE->GetClass()->GetName();
	}
	else
		mode = gEnv->pConsole->GetCVar("sv_gamerules")->GetString();

	char strProductVersion[256];
	gEnv->pSystem->GetProductVersion().ToString(strProductVersion);
	
	string name;
	ICVar* pName = gEnv->pConsole->GetCVar("sv_servername");
	if(pName)
		name = pName->GetString();
	if(name.empty())
		name = gEnv->pNetwork->GetHostName();

	string levelname;
	GetLevelName(levelname);

	hlp.ServerValue("mapname",levelname);
	hlp.ServerValue("gametype",mode);
	hlp.ServerValue("hostname",name);
	hlp.ServerValue("gamever",strProductVersion);
}

void CGameStats::Init()
{
  if(!gEnv->bMultiplayer )
    return;

	ICVar* pReport = gEnv->pConsole->GetCVar("sv_gs_report");
	bool need_report = pReport && pReport->GetIVal()!=0 && m_serverReport == 0;
	if(!gEnv->bServer)
	{
		need_report = false;//no report on clients
	}

	ICVar* pStats = gEnv->pConsole->GetCVar("sv_gs_trackstats");
	bool need_stats = pStats && pStats->GetIVal()!=0 && m_statsTrack == 0;
	
	if(gEnv->bServer)
	{
		ICVar* pLan = gEnv->pConsole->GetCVar("sv_lanonly");
		if(pLan && pLan->GetIVal()==1)
		{
			need_stats = false;//no stats tracking on LAN game
		}
	}

	if(!need_stats && !need_report)
		return;

	INetworkService* serv = GetISystem()->GetINetwork()->GetService("GameSpy");
  if(!serv)
    return;

  ENetworkServiceState state = serv->GetState();

	INetwork* pNetwork = gEnv->pNetwork;
  while(state == eNSS_Initializing)
	{
		pNetwork->SyncWithGame(eNGS_FrameStart);
		pNetwork->SyncWithGame(eNGS_FrameEnd);
		gEnv->pTimer->UpdateOnFrameStart();
		state = serv->GetState();
	}

	if(state != eNSS_Ok)
		return;

	if(!gEnv->bServer)//pure client
	{
		INetworkProfile* prof = serv->GetNetworkProfile();
		if(prof && prof->IsLoggedIn())
		{
			need_stats = true;
		}
		else
		{
			need_stats = false;
		}
	}
  
// 	if(need_stats)
// 	{
// 		IStatsTrack *st = serv->GetStatsTrack(m_config->GetStatsVersion());
// 		if(st->IsAvailable())
// 		{
// 			m_statsTrack = st;
// 		}
// 	}

  if(need_report)
  {
	  IServerReport* sr = serv->GetServerReport();
	  if(sr->IsAvailable())
    {
		  m_serverReport = sr;
    }
  }
}

void CGameStats::GetMemoryStatistics(ICrySizer *s)
{
	s->AddContainer(m_playerMap);
	
	for (PlayerStatsMap::iterator iter = m_playerMap.begin(); iter != m_playerMap.end(); ++iter)
	{
		s->Add(iter->second.name);
		s->AddContainer(iter->second.scores);
	}
}
