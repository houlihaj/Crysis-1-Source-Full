#ifndef __GSSERVERBROWSER_H__
#define __GSSERVERBROWSER_H__

#pragma once

#include "INetwork.h"
#include "INetworkService.h"
#include "NetTimer.h"

// peterb: hack to make gamespy compile on linux...
#ifdef _HASHTABLE_H
#undef _HASHTABLE_H
#endif

#include "GameSpy/serverbrowsing/sb_internal.h"
#include "GameSpy/serverbrowsing/sb_serverbrowsing.h"

class CGSServerBrowser:public CMultiThreadRefCount,public IServerBrowser
{
private:
	struct SServerInfo
	{
		int m_id;
		bool m_pinged;
    bool m_reported;

		SServerInfo(const int id):m_id(id),m_pinged(false),m_reported(false)
		{
		}
	};

	typedef std::map<SBServer,SServerInfo> TSrvMap;

	struct SNServerInfo
	{
    SNServerInfo():
    m_numPlayers(0),
    m_maxPlayers(0),
    m_hostPort(0),
    m_publicPort(0),
    m_publicIP(0),
    m_privateIP(0),
    m_dx10(false),
    m_friendlyfire(false),
    m_official(false),
    m_private(false),
    m_voicecomm(false),
    m_dedicated(false),
		m_gamepadsonly(false)
    {

    }

		int				  m_numPlayers;
		int				  m_maxPlayers;
		bool			  m_private;
    bool        m_official;
    bool        m_anticheat;
    bool        m_dx10;
    bool        m_dedicated;
    bool        m_friendlyfire;
    bool        m_voicecomm;
		bool				m_gamepadsonly;
		ushort			m_hostPort;
    ushort      m_publicPort;
		uint32			m_publicIP; 
		uint32			m_privateIP;
		string			m_hostName;   
		string			m_mapName;
		string			m_gameVersion;
		string			m_gameType;
		string			m_country;
		string			m_modName;
		string			m_modVersion;
	};
public:
	CGSServerBrowser();   
	~CGSServerBrowser();

    //start of IServerBrowser
  virtual void	Start(bool browseLAN);
  virtual void  SetListener(IServerListener* lst);
  virtual void	UpdateServerInfo(int id);
  virtual void  BrowseForServer(const char* address, ushort port);
  virtual void  BrowseForServer(uint ip, ushort port);
  virtual void	Stop();
  virtual void	Update();
  virtual int		GetServerCount();
  virtual int		GetPendingQueryCount();
  virtual void  SendNatCookie(uint ip, ushort port, int cookie);
  virtual void  CheckDirectConnect(int id, ushort port);
    //end of IServerBrowser

	bool			    IsDead()const;
	virtual bool	IsAvailable()const;

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CGSServerBrowser");

		pSizer->Add(*this);
		if(m_serverBrowser)
			pSizer->AddObject(m_serverBrowser, sizeof(_ServerBrowser));
		pSizer->AddContainer(m_serverMap);
	}

private:
	void	NStart(bool browseLAN);
	void	NUpdateServerInfo(int id);
	void	NStop();
	void	NUpdate();
	void	NUpdateProgressCounters();
  void  NSendNatCookie(uint ip, ushort port, int cookie);
  void  NCheckDirectConnect(int id, ushort port);
  void  NBrowseForServer(uint ip, ushort port);
  void  NBrowseForServerAddr(string addr, ushort port);

	static void TimerCallback(NetTimerId,void*,CTimeValue);
	static void SBCallback(ServerBrowser sb, SBCallbackReason reason, SBServer server,void *instance);

	void GUpdateProgressCounters(int pending,int total);
	void GOnServer(int id,SNServerInfo s_info,bool update);
	void GUpdateValue(int id,string name,string value);
	void GUpdatePlayerValue(int id, int playerNum, string name, string value);
	void GUpdateTeamValue(int id, int teamNum, string name, string value);
	void GServerUpdateFailed(int id);
  void GServerUpdateComplete(int id);
	void GOnError(EServerBrowserError);
	void GUpdateComplete(bool cancel);
	void GRemoveServer(int id);
	void GServerPinged(int id, int ping);
  void GDirectConnect(bool neednat, uint ip, ushort port);


	void OnTimerCallback();
	void OnSBCallback(SBCallbackReason reason, SBServer server);
	void PumpKeys(SBServer server, bool failed);
	void HandleSBError(SBError error);
	static void KeyEnumCallback(const gsi_char* key,const gsi_char* value,void* instance);


  IServerListener*	m_pServerListener;
  NetTimerId			  m_updateTimer;
  int					      m_serverCount;
  int					      m_serverPending;
  ServerBrowser		  m_serverBrowser;
  TSrvMap				    m_serverMap;
  int					      m_lastId;
  int					      m_currentServerId;
  bool              m_lanOnly;

  int					      m_dbgServerCount;
  bool				      m_dead;
};

typedef _smart_ptr<CGSServerBrowser> CGSServerBrowserPtr;

#endif
