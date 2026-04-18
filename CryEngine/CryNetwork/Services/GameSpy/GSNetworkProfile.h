/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:  Gamespy network profile implementation
-------------------------------------------------------------------------
History:
- 01/11/2007   : Stas Spivakov, Created
*************************************************************************/
#ifndef __GSNETWORKPROFILE_H__
#define __GSNETWORKPROFILE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "GameSpy.h"
#include "GameSpy/gp/gp.h"

struct SProfileInfo
{
  SProfileInfo():m_profile(0),m_complete(false)
  {}
  int     m_profile;
  string  m_ticket;
  string  m_email;
  string  m_profilenick;
  string  m_uniquenick;
  string  m_password;
  bool    m_complete;
};

class CGSNetworkProfile : public CMultiThreadRefCount, public INetworkProfile
{
private:
	struct SBuddyInfo
	{
		SBuddyInfo();
		int		  m_profile;
		string	m_nick;
		string  m_location;
		GPEnum	m_status;
    string  m_statusString;
    string  m_country;
		bool    m_updated;
		bool		m_foreign;
	};
	typedef std::vector<SBuddyInfo> BuddyVector;

  struct SRegisterInfo
  {
    string m_email;
    string m_uniquenick;
    string m_password;
		string m_country;
    int    m_profile;
		SRegisterDayOfBirth	m_dob;
  };
public:
	CGSNetworkProfile(CGameSpy* gamespy);
	~CGSNetworkProfile();
  virtual void AddListener(INetworkProfileListener* );
  virtual void RemoveListener(INetworkProfileListener* );
	virtual void Register(const char* nick, const char* email, const char* password, const char* country, SRegisterDayOfBirth dob);
	virtual void Login(const char* nick, const char* password);
	virtual void LoginProfile(const char* email, const char* password, const char* profile);
  virtual void Logoff();
	virtual void SetStatus(EUserStatus status, const char* location);
	virtual void EnumUserNicks(const char* email, const char* password);
  virtual void AuthFriend(int id, bool auth);
  virtual void RemoveFriend(int id, bool ignore);
	virtual void AddFriend(int id, const char* reason);
  virtual void SendFriendMessage(int id, const char* message);
	virtual bool IsLoggedIn();
	virtual bool IsLoggingIn();
  virtual void UpdateBuddies();
  virtual void GetProfileInfo(int id);
  virtual void SearchFriends(const char* request);
  virtual void GetUserId(const char* nick);
  virtual void GetUserNick(int id);


  virtual void ReadStats(IStatsReader* reader);
  virtual void WriteStats(IStatsWriter* writer);
	virtual void ReadStats(int id,IStatsReader* reader);
	virtual void DeleteStats(IStatsDeleter* deleter);

	virtual void RetrievePassword(const char* email);
  
	bool CheckNick(const char* nick);
	bool CheckPassword(const char* pass);
	bool CheckEmail(const char* mail);

	//CryNetwork functions
  void    GetAuthInfo(int &profile, string& ticket);
	void		GetLoginPassword(string& login, string& password);

  void GetMemoryStatistics(ICrySizer *);

	virtual bool    IsAvailable()const;
  bool            IsDead()const;

	void	Init();
	void	Think();
	void	CleanUp();

  const SProfileInfo& GetProfileInfo()
  {
    return m_profile;
  }

private:

	void	NC_Login(string nick, string pwd);
	void	NC_LoginProfile(string email, string pwd, string profile);
  void  NC_Logoff();
	void	NC_Register(string nick, string email, string pwd, string country, uint32 dob);
	void	NC_EnumNicks(string email, string pwd);
	void	NC_GetProfileInfo(int id);
	void	NC_SetStatus(EUserStatus status, string location);
  void  NC_Error(GPResult);
  void  NC_AuthBuddy(int id, bool auth);
  void  NC_AddBuddy(int id, string reason);
  void  NC_RemoveBuddy(int id);
  void  NC_SendBuddyMessage(int id, string text);
  void  NC_Search(string request);
  void  NC_GetId(string nick);
  void  NC_GetNick(int id);
  void  NC_ReadStats(IStatsReader* reader);
  void  NC_WriteStats(IStatsWriter* writer);
	void  NC_DeleteStats(IStatsDeleter* deleter);
  void  NC_ReadStatsWithId(int id,IStatsReader* reader);

  void  NC_LoggedIn(GPResult result, int profile, const char* nick);

	void	NC_RetrievePassword(string email);
  
  void	GC_Error(GPResult, GPErrorCode, string reason);
  void  GC_LoginResult(GPResult, GPErrorCode, string);
	void  LoginError(ENetworkProfileError, const char*);
	void	GC_AddNick(string nick);
	void	GC_BuddyStatus(int id, int index, GPEnum status, string location, string statusString);
	void	GC_ProfileInfo(int id, string nick, string country, bool foreign_name); 
	void	GC_BuddyMessage(int id, string message);
  void	GC_RevokeBuddy(int id);
  void  GC_BuddyRequest(int id, string message);
  void  GC_SearchResult(int id, string nick);
  void  GC_SearchComplete();
  void  GC_GetIdResult(string nick,int id);
  void  GC_GetNickResult(int id, string nick, bool foreign_name);
	void  GC_RequestFailed(IStatsAccessor* acc);
	void	GC_GetPasswordStatus(bool ok);

	void	UpdateUser(int idx);
	void  RenewTicket();
	
	//Gamespycallbacks
	static void	ErrorCallback(GPConnection * connection, void * arg, void * obj );
	static void	ConnectCallback(GPConnection * connection, void * arg, void * obj );
	static void	ConnectCallbackRegister(GPConnection * connection, void * arg, void * obj );
	static void	ConnectProfileCallback(GPConnection * connection, void * arg, void * obj );
  static void	ConnectRegisterCallback(GPConnection * connection, void * arg, void * obj );
  static void	RegisterNickCallback(GPConnection * connection, void * arg, void * obj );
  static void	ConnectNewCallback(GPConnection * connection, void * arg, void * obj );
	static void	EnumNicksCallback(GPConnection * connection, void * arg, void * obj );
	static void	BuddyStatusCallback(GPConnection * connection, void * arg, void * obj );
	static void	BuddyMessageCallback(GPConnection * connection, void * arg, void * obj );
	static void	BuddyAuthCallback(GPConnection * connection, void * arg, void * obj );
	static void	BuddyRevokeCallback(GPConnection * connection, void * arg, void * obj );
	static void	GameInviteCallback(GPConnection * connection, void * arg, void * obj );
	static void	ProfileInfoCallback(GPConnection * connection, void * arg, void * obj );
  static void	FileTransferCallback(GPConnection * connection, void * arg, void * obj );
  static void	SearchCallback(GPConnection * connection, void * arg, void * obj );
  static void	GetIdCallback(GPConnection * connection, void * arg, void * obj );
  static void	GetNickCallback(GPConnection * connection, void * arg, void * obj );

	static void TimerCallback(NetTimerId,void*,CTimeValue);

  std::vector<INetworkProfileListener*> m_listeners;
	CGameSpy*					m_gamespy;
	GPConnection			m_connection;
	NetTimerId				m_updateTimer;

  SProfileInfo      m_profile;

	BuddyVector				m_buddies;
  SRegisterInfo     m_register;
  bool              m_loggedIn;
	bool							m_loggingIn;
};

#endif /*__GSNETWORKPROFILE_H__*/