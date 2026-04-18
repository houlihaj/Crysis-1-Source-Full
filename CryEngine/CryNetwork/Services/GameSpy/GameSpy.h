#ifndef __GAMESPY_H__
#define __GAMESPY_H__

#pragma once

#include "INetwork.h"
#include "INetworkService.h"
#include "NetTimer.h"
#include "GSServerBrowser.h"

class CGSServerReport;
class CGSChat;
class CGSStatsTrack;
class CGSNatNeg;
class CGSFileDownloader;
class CGSNetworkProfile;
class CGSCVExtension;
class CGSStorage;
class CGSPatchCheck;

#ifdef CRYSIS_BETA
static const int   CDKEY_PRODUCT_ID = 1599;
static const int	 PATCH_PRODUCT_ID = 10976;	// Crysis
//static const int PATCH_PRODUCT_ID = 10981;	// Crysis Demo
static const int PATCH_DISTRIBUTION_ID = 1331;	// Crysis Beta
//static const int	 PATCH_DISTRIBUTION_ID = 1330;		// Crysis Development

static const int   GAMESPY_PRODUCT_ID = 10846;
static const char* GAMESPY_GAME_NAME = "crysisb";
static const char* GAMESPY_CHANNEL_NAME = "#gsp!crysis";
static const int   GAMESPY_NAMESPACE_ID = 56;
static const char* GAMESPY_NAMESPACE_EXT = "-cry";

#else

static const int   CDKEY_PRODUCT_ID = 2208;
static const int	 PATCH_PRODUCT_ID = 10976;	// Crysis
//static const int PATCH_PRODUCT_ID = 10981;	// Crysis Demo
static const int	 PATCH_DISTRIBUTION_ID = 1332;	// Crysis Retail

static const int   GAMESPY_PRODUCT_ID = 11489;
static const char* GAMESPY_GAME_NAME = "crysiswars";
static const char* GAMESPY_CHANNEL_NAME = "#gsp!crysiswars";
static const int   GAMESPY_NAMESPACE_ID = 56;
static const char* GAMESPY_NAMESPACE_EXT = "-cry";

#endif

static const int   GAMESPY_SERVER_KEY_ANTICHEAT = NUM_RESERVED_KEYS+0;
static const int   GAMESPY_SERVER_KEY_OFFICIAL  = NUM_RESERVED_KEYS+1;
static const int   GAMESPY_SERVER_KEY_VOICECOMM  = NUM_RESERVED_KEYS+2;
static const int   GAMESPY_SERVER_KEY_FRIENDLYFIRE  = NUM_RESERVED_KEYS+3;
static const int   GAMESPY_SERVER_KEY_DEDICATED  = NUM_RESERVED_KEYS+4;
static const int   GAMESPY_SERVER_KEY_DX10  = NUM_RESERVED_KEYS+5;
static const int   GAMESPY_SERVER_KEY_GAMEPADSONLY  = NUM_RESERVED_KEYS+6;
static const int   GAMESPY_SERVER_KEY_COUNTRY = NUM_RESERVED_KEYS+7;
static const int	 GAMESPY_SERVER_KEY_MODNAME = NUM_RESERVED_KEYS+8;
static const int	 GAMESPY_SERVER_KEY_MODVERSION = NUM_RESERVED_KEYS+9;

static const int   GAMESPY_SERVER_NUM_RESERVED_KEYS = NUM_RESERVED_KEYS+10;

typedef CryFixedStringT<32>	TGSFixedString;

class CGameSpy : public INetworkService
{
public:
	CGameSpy();
	~CGameSpy();

	// INetworkService
	virtual ENetworkServiceState GetState();
	virtual void Close();
  virtual void CreateContextViewExtensions(bool server, IContextViewExtensionAdder* adder);


	virtual IServerBrowser*   GetServerBrowser();
	virtual INetworkChat*	    GetNetworkChat();
	virtual INetworkProfile*  GetNetworkProfile();
	virtual IServerReport*	  GetServerReport();
	virtual IStatsTrack*	    GetStatsTrack(int version);
  virtual INatNeg*          GetNatNeg(INatNegListener*const);
	virtual ITestInterface*	  GetTestInterface();
	virtual IFileDownloader*  GetFileDownloader();
	virtual IPatchCheck*			GetPatchCheck();

	void	OnAvailable();
  void  OnUnavailable();

	void GetMemoryStatistics(ICrySizer* pSizer);

  void RegisterExtension(CGSCVExtension* ext);
  void UnregisterExtension(CGSCVExtension* ext);

	std::vector<CGSCVExtension*> m_extensions;

  CGSStorage* GetStorage();
	CGSStatsTrack* GetCurrentStatsTrack();

	//STATS
	bool SendSessionId(int plr, const gsi_u8 sessionId[]);//server
	void SetLocalStatsConnectionId(const gsi_u8 connectionId[]);
	void SetStatsConnectionId(int plrid, const gsi_u8 connectionId[]);//from client

private:
	ENetworkServiceState m_state;
	NetTimerId m_timer;
	CGSServerBrowserPtr m_serverBrowser;
	_smart_ptr<CGSServerReport>	m_serverReport;
	_smart_ptr<CGSChat> m_networkChat;
	_smart_ptr<CGSNetworkProfile> m_profile;
	_smart_ptr<CGSStatsTrack> m_stats;
  _smart_ptr<CGSNatNeg> m_natNeg;
	_smart_ptr<CGSFileDownloader> m_pFileDownload;
	_smart_ptr<CGSPatchCheck> m_pPatchCheck;
  _smart_ptr<CGSStorage>  m_storage;
  
  static void TimerCallback( NetTimerId id, void * pUser, CTimeValue tm );
	void Cleanup();
};

#endif
