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

#ifndef __GSSERVERREPORT_H__
#define __GSSERVERREPORT_H__

#pragma once

#include "INetwork.h"
#include "INetworkService.h"
#include <map>
#include "GameSpy.h"
#include "GameSpy/qr2/qr2.h"

struct SValueMap
{
	void	SetValue(int idx, string value)
	{
		Values[idx] = value;
	}
	bool	GetValue(int idx, string& value)
	{
		std::map<int,string>::iterator it = Values.find(idx);
		if(it != Values.end())
		{
			value = it->second;
			return true;
		}
		else
			return false;
	}
	uint	GetCount()const
	{
		return Values.size();
	}

	void ListKeys(qr2_keybuffer_t buf)
	{
		for( std::map<int,string>::iterator it = Values.begin(); it != Values.end(); ++it)
		{
			qr2_keybuffer_add(buf, it->first);	
		}
	}
	std::map<int,string>	Values;
};

class CGameSpy;
class CGSCVExtension;

class CGSServerReport : public CMultiThreadRefCount, public IServerReport
{
private:
	enum	EKeyType
	{
		eKT_Server,
		eKT_Player,
		eKT_Team
	};
	typedef std::vector<std::pair<int,int> > TReauthVector;
public:
	CGSServerReport(CGameSpy* gs);
	~CGSServerReport();

	virtual void AuthPlayer(int playerid, uint ip, const char* challenge, const char* responce);
	virtual void ReAuthPlayer(int playerid, const char* responce);
  //virtual void PlayerDisconnected(int playerid);
	
	virtual void SetReportParams(int numplayers, int numteams);
 
	virtual void SetServerValue(const char* key, const char*);
	virtual void SetPlayerValue(int, const char* key, const char*);
	virtual void SetTeamValue(int, const char* key, const char*);

	virtual void Update();
	virtual void StartReporting(INetNub*, IServerReportListener* listener);//Start reporting
	virtual void StopReporting();//Stop reporting

  virtual void OnClientConnected(CGSCVExtension*);
	virtual void OnClientDisconnected(CGSCVExtension*);

  virtual void ProcessQuery(char* data, int len, struct sockaddr_in* addr);

  void	Think();

	bool	IsDead()const;
	bool	IsAvailable()const;

	void GetMemoryStatistics(ICrySizer* pSizer);

private:
	void	InitStandartKeys();
	void	UnregisterKeys();

	int		GetKeyId(const string& key,EKeyType type);

	void	NC_SetServerValue(TGSFixedString key, TGSFixedString value);
	void	NC_SetPlayerValue(int, TGSFixedString key, TGSFixedString value);
	void	NC_SetTeamValue(int, TGSFixedString key, TGSFixedString value);
	void	NC_SetReportParams(int numplayers, int numteams);
  void  NC_StartReport(INetNub* nub,IServerReportListener* lst);
	void	NC_StopReport();
	void	NC_Update();
	void	NC_AuthPlayer(int playerid, uint ip, string challenge, string responce);
	void	NC_ReAuthPlayer(int playerid, string responce);
  void	NC_PlayerDisconnected(int playerid);
  void	NC_ReAuthRequest(int localid, const char* challenge);
	void  NC_Error(qr2_error_t err, const char* errmsg);
    
	void	GC_Error(qr2_error_t);
  void	GC_OnNatCookie(int);
	void	GC_OnPublicIP(unsigned int,unsigned short);
  

	//Callbacks
	static void QR2ServerKeyCallback(int keyid, qr2_buffer_t outbuf, void* obj); 
	static void QR2PlayerKeyCallback(int keyid, int index, qr2_buffer_t outbuf, void* obj);
	static void QR2TeamKeyCallback(int keyid, int index, qr2_buffer_t outbuf, void* obj);
	static void QR2KeyListCallback(qr2_key_type keytype, qr2_keybuffer_t keybuffer, void* obj);
	static int  QR2CountCallback(qr2_key_type keytype, void* obj);
	static void QR2ErrorCallback(qr2_error_t error, gsi_char* errmsg, void* obj);
  static void QR2NatNegCallback(int cookie, void* obj);
	static void QR2PublicAddresCallback(unsigned int ip, unsigned short port, void* obj);
	static void GCDReAuthorizeCallback(int gameid, int localid, int hint, char *challenge, void *obj);
	static void GCDAuthorizeCallback(int gameid, int localid, int authenticated, char *errmsg, void *obj);


	static void TimerCallback(NetTimerId,void*,CTimeValue);

  INetNub*      m_netNub;
	IServerReportListener*	m_pListener;
	qr2_t					m_QR;

  SValueMap				m_serverInfo;
	SValueMap				m_playerKeys; // dummy to collect all possible keys
	SValueMap				m_teamKeys; // dummy to collect all possible keys
	std::vector<SValueMap>  m_playersInfo;
	std::vector<SValueMap>  m_teamsInfo;
	std::map<string,int>	m_keyIdMap;
	std::vector<char*>		m_keyNames;
	TReauthVector m_reathSId;
	int						m_lastKey;
	bool					m_dead;
	bool					m_reporting;
	bool					m_cdkeyChecking;
	NetTimerId		m_updateTimer;
  uint          m_ip;
  ushort        m_port;
	uint32				m_seed;
  CGameSpy*     m_parent;
};


#endif /*__GSSERVERREPORT_H__*/