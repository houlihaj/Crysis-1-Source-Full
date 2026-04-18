/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description:	Implementation of the IGameFramework interface. CCryAction
								provides a generic game framework for action based games
								such as 1st and 3rd person shooters.
  
 -------------------------------------------------------------------------
  History:
  - 20:7:2004   10:51 : Created by Marco Koegler
	- 3:8:2004		11:11 : Taken-over by Marcio Martins

*************************************************************************/
#ifndef __CRYACTION_H__
#define __CRYACTION_H__

#if _MSC_VER > 1000
#	pragma once
#endif


#include <ISystem.h>
#include <ICmdLine.h>

#include "IGameFramework.h"
#include "ICryPak.h"
#include "ISaveGame.h"
#include "ModManager/ModManager.h"

struct IFlowSystem;
struct IGameTokenSystem;
struct IEffectSystem;

class CGameRulesSystem;
class CScriptBind_Action;
class CScriptBind_ActorSystem;
class CScriptBind_ItemSystem;
class CScriptBind_ActionMapManager;
class CScriptBind_Network;
class CScriptBind_AI;
class CScriptBind_FlowSystem;
class CScriptBind_VehicleSystem;
class CScriptBind_Vehicle;
class CScriptBind_VehicleSeat;
class CScriptBind_Inventory;
class CScriptBind_DialogSystem;
class CScriptBind_MaterialEffects;

class CDevMode;
class CTimeDemoRecorder;
class CGameQueryListener;
class CScriptRMI;
class CAnimationGraphManager;
class CGameSerialize;
class CMaterialEffects; 
class CMaterialEffectsCVars;
class CGameObjectSystem;
class CActionMapManager;
class CActionGame;
class CActorSystem;
class CCallbackTimer;
class CGameClientNub;
class CGameContext;
class CGameServerNub;
class CItemSystem;
class CLevelSystem;
//class CUISystem;
class CUIDraw;
class CVehicleSystem;
class CViewSystem;
class CGameplayRecorder;
class CPersistantDebug;
class CPlayerProfileManager;
class CDialogSystem;
class CSubtitleManager;
class CGameplayAnalyst;
class CTimeOfDayScheduler;
class CNetworkCVars;
class CCryActionCVars;
class CGameStatsConfig;

struct IAnimationGraphState;

class CCryAction : public IGameFramework
{
public:
	CCryAction();
	virtual ~CCryAction();

	void UpdateTimer(void);										// update the timers
	int AddTimer(CTimeValue nInterval, bool bRepeat, void (*pFunction)(void*, int), void *pUserData);		// add a new timer and return its handle; interval is in ms
	void* RemoveTimer(int nIndex);								// remove an existing timer, returns user data

	// IGameFramework
	virtual void RegisterFactory(const char *name, IActorCreator * pCreator, bool isAI);
	virtual void RegisterFactory(const char *name, IItemCreator * pCreator, bool isAI);
	virtual void RegisterFactory(const char *name, IVehicleCreator * pCreator, bool isAI);
	virtual void RegisterFactory(const char *name, IGameObjectExtensionCreator * pCreator, bool isAI );
	virtual void RegisterFactory(const char *name, IAnimationStateNodeFactory *(*func)(), bool isAI );
	virtual void RegisterFactory(const char *name, ISaveGame *(*func)(), bool);
	virtual void RegisterFactory(const char *name, ILoadGame *(*func)(), bool);

	virtual bool Init(SSystemInitParams &startupParams);
	virtual bool CompleteInit();
	virtual void Shutdown();
	virtual bool PreUpdate(bool haveFocus, unsigned int updateFlags);
	virtual void PostUpdate(bool haveFocus, unsigned int updateFlags);
	virtual void Reset(bool clients);
	virtual void GetMemoryStatistics(ICrySizer *);

	virtual void PauseGame(bool pause, bool force);
	virtual bool IsGamePaused();
	virtual bool IsGameStarted();
	virtual bool IsInLevelLoad();
	virtual bool IsLoadingSaveGame();
	virtual const char * GetLevelName();
	virtual const char * GetAbsLevelPath();
	virtual bool IsInTimeDemo(); 	// Check if time demo is playing;

	virtual ISystem *GetISystem() { return m_pSystem; };
	virtual ILanQueryListener *GetILanQueryListener() {return m_pLanQueryListener;}
	virtual IUIDraw *GetIUIDraw();	
	virtual ILevelSystem *GetILevelSystem();
	virtual IActorSystem *GetIActorSystem();
	virtual IItemSystem *GetIItemSystem();
	virtual IVehicleSystem *GetIVehicleSystem();
	virtual IActionMapManager *GetIActionMapManager();
	virtual IViewSystem *GetIViewSystem();
	virtual IGameplayRecorder *GetIGameplayRecorder();
	virtual IGameRulesSystem *GetIGameRulesSystem();
	virtual IGameObjectSystem *GetIGameObjectSystem();
	virtual IFlowSystem *GetIFlowSystem();
	virtual IGameTokenSystem *GetIGameTokenSystem();
	virtual IEffectSystem *GetIEffectSystem();
	virtual IMaterialEffects *GetIMaterialEffects();
	virtual IPlayerProfileManager *GetIPlayerProfileManager();
	virtual ISubtitleManager *GetISubtitleManager();
	virtual IDialogSystem *GetIDialogSystem();

	virtual bool StartGameContext( const SGameStartParams * pGameStartParams );
	virtual bool ChangeGameContext( const SGameContextParams * pGameContextParams );
	virtual void EndGameContext();
	virtual bool StartedGameContext() const { return m_pGame != 0; }
	virtual bool BlockingSpawnPlayer();

	virtual void ResetBrokenGameObjects();
	void Serialize(TSerialize ser); // defined in ActionGame.cpp
	void FlushBreakableObjects();                   // defined in ActionGame.cpp
	void ClearBreakHistory(); 

	virtual void SetEditorLevel(const char *levelName, const char *levelFolder);
	virtual void GetEditorLevel(char **levelName, char **levelFolder);

	virtual void BeginLanQuery();
	virtual void EndCurrentQuery();

	virtual IActor * GetClientActor() const;
	virtual EntityId GetClientActorId() const;
	virtual INetChannel * GetClientChannel() const;
	virtual CTimeValue GetServerTime();
	virtual uint16 GetGameChannelId(INetChannel *pNetChannel);
	virtual INetChannel *GetNetChannel(uint16 channelId);
	virtual bool IsChannelOnHold(uint16 channelId);
	virtual IGameObject * GetGameObject(EntityId id);
	virtual bool GetNetworkSafeClassId(uint16 &id, const char *className);
	virtual bool GetNetworkSafeClassName(char *className, size_t maxn, uint16 id);
	virtual IGameObjectExtension * QueryGameObjectExtension( EntityId id, const char * name);

	virtual INetContext* GetNetContext();

	virtual bool SaveGame( const char * path, bool bQuick = false, bool bForceImmediate=true, ESaveGameReason reason = eSGR_QuickSave, bool ignoreDelay = false, const char* checkpointName = NULL);
	virtual bool LoadGame( const char * path, bool quick = false, bool ignoreDelay = false);
	virtual void ScheduleEndLevel( const char* nextLevel = "");

	virtual void OnEditorSetGameMode( int iMode );
	virtual bool IsEditing(){return m_isEditing;}

	bool IsImmersiveMPEnabled();

	virtual void AllowSave(bool bAllow = true)
	{
		m_bAllowSave = bAllow;
	}

	virtual void AllowLoad(bool bAllow = true)
	{
		m_bAllowLoad = bAllow;
	}

	virtual bool CanSave();
	virtual bool CanLoad();

	virtual bool CanCheat();

	// Music Logic
	virtual IAnimationGraphState * GetMusicGraphState() { return m_pMusicGraphState; }
	virtual IMusicLogic * GetMusicLogic() {return m_pMusicLogic; }

	INetNub * GetServerNetNub();

	void SetGameGUID( const char * gameGUID);
	const char* GetGameGUID() { return m_gameGUID; }

	virtual bool IsVoiceRecordingEnabled() {return m_VoiceRecordingEnabled!=0;}

	virtual void StoreServerInfo(SServerConnectionInfo& info);
	virtual SServerConnectionInfo* GetStoredServerInfo();
	virtual void ClearStoredServerInfo();

	virtual void ReadCDKey();
	virtual void SaveCDKey(const char* cdKey);
	virtual bool WasCDKeyReadSuccessfully() { return m_cdKeyOK; }

	// ~IGameFramework

	static CCryAction * GetCryAction() { return m_pThis; }

	bool ControlsEntity( EntityId id ) const;

	virtual CGameServerNub * GetGameServerNub();
	CGameClientNub * GetGameClientNub();
	CGameContext * GetGameContext();
	CAnimationGraphManager * GetAnimationGraphManager() { return m_pAnimationGraphManager; }
	CScriptBind_Vehicle *GetVehicleScriptBind() { return m_pScriptBindVehicle; }
  CScriptBind_VehicleSeat *GetVehicleSeatScriptBind() { return m_pScriptBindVehicleSeat; }
	CScriptBind_Inventory *GetInventoryScriptBind() { return m_pScriptInventory; }
	CPersistantDebug *GetPersistantDebug() { return m_pPersistantDebug; }
	virtual IPersistantDebug * GetIPersistantDebug();
	virtual IGameStatsConfig* GetIGameStatsConfig();

	virtual void RegisterListener		(IGameFrameworkListener *pGameFrameworkListener, const char *name, EFRAMEWORKLISTENERPRIORITY eFrameworkListenerPriority);
	virtual void UnregisterListener	(IGameFrameworkListener *pGameFrameworkListener);

	CDialogSystem* GetDialogSystem() { return m_pDialogSystem; }
	CTimeOfDayScheduler* GetTimeOfDayScheduler() const { return m_pTimeOfDayScheduler; }
	IDebrisMgr* GetDebrisMgr() { return m_pDebrisMgr; }
	CGameStatsConfig* GetGameStatsConfig();

//	INetQueryListener* GetLanQueryListener() {return m_pLanQueryListener;}
	bool LoadingScreenEnabled() const;


	// this is a bit of a hack
	int NetworkExposeClass( IFunctionHandler * pFH );

	void ResetMusicGraph();

	void NotifyGameFrameworkListeners(ISaveGame* pSaveGame);
	void NotifyGameFrameworkListeners(ILoadGame* pLoadGame);
	virtual void EnableVoiceRecording(const bool enable);
	virtual void MutePlayerById(EntityId mutePlayer);
  virtual IDebugHistoryManager* CreateDebugHistoryManager();
  virtual void ExecuteCommandNextFrame(const char* cmd);
	virtual void PrefetchLevelAssets( const bool bEnforceAll );

	virtual bool GetModInfo(SModInfo *modInfo, const char *modPath = 0);

  virtual void ShowPageInBrowser(const char* URL);
  virtual bool StartProcess(const char* cmd_line);
  virtual bool SaveServerConfig(const char* path);

  void  OnActionEvent(const SActionEvent& ev);

	bool IsPbSvEnabled() const { return m_pbSvEnabled; }
	bool IsPbClEnabled() const { return m_pbClEnabled; }

	void DumpMemInfo(const char* format, ...) PRINTF_PARAMS(2, 3);

	#ifdef SP_DEMO
		//TODO: Remove Demo code from engine
		void CreateDevMode();
		void RemoveDevMode();
	#endif
private:
	void InitScriptBinds();
	void ReleaseScriptBinds();

	void InitCVars();
	void ReleaseCVars();

	void InitCommands();

	// TODO: remove
	static void FlowTest(IConsoleCmdArgs*);

	// console commands provided by CryAction
	static void DumpMapsCmd(IConsoleCmdArgs* args);
	static void MapCmd(IConsoleCmdArgs* args);
	static void UnloadCmd(IConsoleCmdArgs* args);
	static void PlayCmd(IConsoleCmdArgs* args);
	static void ConnectCmd(IConsoleCmdArgs* args);
	static void DisconnectCmd(IConsoleCmdArgs* args);
	static void StatusCmd(IConsoleCmdArgs* args);
	static void SaveTagCmd(IConsoleCmdArgs* args);
	static void LoadTagCmd(IConsoleCmdArgs* args);
	static void SaveGameCmd(IConsoleCmdArgs* args);
	static void GenStringsSaveGameCmd(IConsoleCmdArgs* args);
	static void LoadGameCmd(IConsoleCmdArgs* args);
  static void KickPlayerCmd(IConsoleCmdArgs* args);
  static void KickPlayerByIdCmd(IConsoleCmdArgs* args);
	static void BanPlayerCmd(IConsoleCmdArgs* args);
	static void BanStatusCmd(IConsoleCmdArgs* args);
	static void UnbanPlayerCmd(IConsoleCmdArgs* args);
	static void OpenURLCmd(IConsoleCmdArgs* args);
	static void TestResetCmd(IConsoleCmdArgs* args);
	
	static void DumpAnalysisStatsCmd(IConsoleCmdArgs* args);

	static void TestTimeout(IConsoleCmdArgs* args);
	static void TestNSServerBrowser(IConsoleCmdArgs* args);
	static void TestNSServerReport(IConsoleCmdArgs* args);
	static void TestNSChat(IConsoleCmdArgs* args);
	static void TestNSStats(IConsoleCmdArgs* args);
  static void TestNSNat(IConsoleCmdArgs* args);
  static void TestPlayerBoundsCmd(IConsoleCmdArgs* args);
	static void DumpStatsCmd(IConsoleCmdArgs* args);

	// console commands for the remote control system
	//static void rcon_password(IConsoleCmdArgs* args);
	static void rcon_startserver(IConsoleCmdArgs* args);
	static void rcon_stopserver(IConsoleCmdArgs* args);
	static void rcon_connect(IConsoleCmdArgs* args);
	static void rcon_disconnect(IConsoleCmdArgs* args);
	static void rcon_command(IConsoleCmdArgs* args);

	static void SvPasswordChanged(ICVar *sv_password);

	static struct IRemoteControlServer* s_rcon_server;
	static struct IRemoteControlClient* s_rcon_client;

	static class CRConClientListener* s_rcon_client_listener;

	//static string s_rcon_password;

	// console commands for the simple http server
	static void http_startserver(IConsoleCmdArgs* args);
	static void http_stopserver(IConsoleCmdArgs* args);

	static struct ISimpleHttpServer* s_http_server;

	// change the game query (better than setting it explicitly)
	void SetGameQueryListener( CGameQueryListener * );

	void CheckEndLevelSchedule();
  
	static void MutePlayer(IConsoleCmdArgs* pArgs);
  
	static void VerifyMaxPlayers( ICVar * pVar );

	static void StaticSetPbSvEnabled(IConsoleCmdArgs* pArgs);
	static void StaticSetPbClEnabled(IConsoleCmdArgs* pArgs);
  
	// NOTE: anything owned by this class should be a pointer or a simple
	// type - nothing that will need its constructor called when CryAction's
	// constructor is called (we don't have access to malloc() at that stage)

	bool							m_paused;
	bool							m_forcedpause;

	static CCryAction *m_pThis;

	ISystem						*m_pSystem;
	INetwork					*m_pNetwork;
	I3DEngine					*m_p3DEngine;
	IScriptSystem			*m_pScriptSystem;
	IEntitySystem			*m_pEntitySystem;
	ITimer						*m_pTimer;
	ILog							*m_pLog;
	void							*m_systemDll;

	_smart_ptr<CActionGame>       m_pGame;

	char							m_editorLevelName[512];	// to avoid having to call string constructor, or allocating memory.
	char							m_editorLevelFolder[512];
	char              m_gameGUID[128];

	CLevelSystem			*m_pLevelSystem;
	CActorSystem			*m_pActorSystem;
	CItemSystem				*m_pItemSystem;
	CVehicleSystem    *m_pVehicleSystem;
	CActionMapManager	*m_pActionMapManager;
	CViewSystem				*m_pViewSystem;
	CGameplayRecorder	*m_pGameplayRecorder;
	CGameRulesSystem  *m_pGameRulesSystem;
	IFlowSystem       *m_pFlowSystem;
//	CUISystem					*m_pUISystem;
	CUIDraw						*m_pUIDraw;
	CGameObjectSystem *m_pGameObjectSystem;
	CScriptRMI        *m_pScriptRMI;
	CAnimationGraphManager * m_pAnimationGraphManager;
	CMaterialEffects *m_pMaterialEffects;
	CPlayerProfileManager *m_pPlayerProfileManager;
	CDialogSystem     *m_pDialogSystem;
	IDebrisMgr				*m_pDebrisMgr;
	CSubtitleManager  *m_pSubtitleManager;
	IGameTokenSystem  *m_pGameTokenSystem;
	IEffectSystem			*m_pEffectSystem;
	CGameSerialize    *m_pGameSerialize;
	CCallbackTimer    *m_pCallbackTimer;
	CGameplayAnalyst	*m_pGameplayAnalyst;

 //	INetQueryListener *m_pLanQueryListener;
	ILanQueryListener *m_pLanQueryListener;

	CGameStatsConfig	*m_pGameStatsConfig;

	// developer mode
	CDevMode          *m_pDevMode;

	// TimeDemo recorder.
	CTimeDemoRecorder *m_pTimeDemoRecorder;

	// game queries
	CGameQueryListener*m_pGameQueryListener;

	// script binds
	CScriptBind_Action            *m_pScriptA;
	CScriptBind_ItemSystem				*m_pScriptIS;
	CScriptBind_ActorSystem				*m_pScriptAS;
	CScriptBind_AI								*m_pScriptAI;
	CScriptBind_Network						*m_pScriptNet;
	CScriptBind_ActionMapManager	*m_pScriptAMM;
	//CScriptBind_FlowSystem        *m_pScriptFS;
	CScriptBind_VehicleSystem     *m_pScriptVS;
	CScriptBind_Vehicle           *m_pScriptBindVehicle;
  CScriptBind_VehicleSeat       *m_pScriptBindVehicleSeat;
	CScriptBind_Inventory					*m_pScriptInventory;
	CScriptBind_DialogSystem      *m_pScriptBindDS;
  CScriptBind_MaterialEffects   *m_pScriptBindMFX;
	CTimeOfDayScheduler           *m_pTimeOfDayScheduler;
	CPersistantDebug              *m_pPersistantDebug;

	CNetworkCVars * m_pNetworkCVars;
	CCryActionCVars * m_pCryActionCVars;

	// Console Variables with some CryAction as owner
	CMaterialEffectsCVars         *m_pMaterialEffectsCVars;

	IAnimationGraphState					*m_pMusicGraphState;
	IMusicLogic										*m_pMusicLogic;

	// console variables
	ICVar *m_pEnableLoadingScreen;
	ICVar *m_pCheats;
	ICVar *m_pShowLanBrowserCVAR;

	bool m_bShowLanBrowser;
	//
	bool m_isEditing;
	bool m_bScheduleLevelEnd;
	int  m_delayedSaveGameMethod;     // 0 -> no save, 1=quick save, 2=save, not quick
	ESaveGameReason m_delayedSaveGameReason;

	struct SLocalAllocs
	{
		string m_delayedSaveGameName;
		string m_checkPointName;
		string m_nextLevelToLoad;
	};
	SLocalAllocs* m_pLocalAllocs;

	struct SGameFrameworkListener
	{
		IGameFrameworkListener * pListener;
		string name;
		EFRAMEWORKLISTENERPRIORITY eFrameworkListenerPriority;
		SGameFrameworkListener() : pListener(0), eFrameworkListenerPriority(FRAMEWORKLISTENERPRIORITY_DEFAULT) {}
	};

	typedef std::vector<SGameFrameworkListener> TGameFrameworkListeners;
	TGameFrameworkListeners *m_pGameFrameworkListeners;
	TGameFrameworkListeners *m_pGameFrameworkListenersTemp;

	int m_VoiceRecordingEnabled;

	bool m_bAllowSave;
	bool m_bAllowLoad;
  string  *m_nextFrameCommand;
  string  *m_connectServer;

	float		m_lastSaveLoad;

	bool m_pbSvEnabled;
	bool m_pbClEnabled;

	bool m_cdKeyOK;	// did we get a cd key at startup.

	SServerConnectionInfo* m_pStoredServerConnectionInfo;
};

#endif //__CRYACTION_H__
