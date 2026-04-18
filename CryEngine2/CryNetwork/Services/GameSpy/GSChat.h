/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2006.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  Gamespy Chat Interface implementation
 -------------------------------------------------------------------------
 History:
 - 11/03/2006   : Stas Spivakov, Created
 *************************************************************************/

#ifndef __GSCHAT_H__
#define __GSCHAT_H__

#pragma once

#include "INetwork.h"
#include "INetworkService.h"
#include "GameSpy/chat/chat.h"

class CGameSpy;

class CGSChat : public CMultiThreadRefCount, public INetworkChat
{
private:
	struct SGetKeys
	{
		CGSChat* pChat;
		string   user;
	};
public:
	CGSChat(CGameSpy* gs);
	~CGSChat();

	virtual void Join();
	virtual void Leave();
  virtual void SetListener(IChatListener* lst);
	virtual void Say(const char* message);
	virtual void SendData(const char* nick,const char* message);
	virtual void ListUsers();
	virtual void SetChatKeys(int num, const char** keys, const char** value);
	virtual void GetChatKeys(const char* user, int num, const char** keys);

	bool	IsDead()const;
	virtual bool IsAvailable()const;

	void	LoggedIn();
	void	Think();

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CGSChat");

		pSizer->Add(*this);
	}

private:
	//from game callbacks
	void	NC_Connect();
	void	NC_Disconnect();
	void	NC_Say(string msg);
	void	NC_Whisper(string to, string msg);
	void	NC_ListUsers();
	void  NC_SetChatKeys(std::vector<string> keys, std::vector<string> values);
	void  NC_GetChatKeys(string user, std::vector<string> keys);

	//to game callbacks
	void	GC_Joined(CHATEnterResult res);
	void	GC_Message(string from, string msg);
	void	GC_PrivateMessage(string from, string msg);
	void	GC_User(string nick, EChatUserStatus what);
	void	GC_OnKeys(string user, std::vector<string> keys, std::vector<string> values);
	void	GC_OnGetKeysError(string user);
  void  GC_Error(int err);
	
	//GS Callbacks
	static void RawCallback(CHAT chat, const gsi_char * raw, void * obj );
	static void DisconnectedCallback(CHAT chat, const gsi_char * reason, void * obj );
	static void PrivateMessageCallback(CHAT chat, const gsi_char * user, const gsi_char * message, int type, void * obj );
	static void InvitedCallback(CHAT chat, const gsi_char * channel, const gsi_char * user, void * obj );
	static void NickErrorCallback(CHAT chat, int type, const gsi_char * nick, int numSuggestedNicks, const gsi_char ** suggestedNicks, void * obj );
	static void ConnectCallback(CHAT chat, CHATBool success, int failureReason, void * obj );
	static void ChannelEnumCallbackEach(CHAT chat, CHATBool success, int index, const gsi_char * channel, const gsi_char * topic, int numUsers, void * obj );
	static void ChannelEnumCallbackAll(CHAT chat, CHATBool success, int numChannels, const gsi_char ** channels, const gsi_char ** topics, int * numUsers, void * obj );
	//channel callbacks
	static void EnterChannelCallback(CHAT chat, CHATBool success, CHATEnterResult result, const gsi_char * channel, void * obj );
	static void ChannelMessage(CHAT chat, const gsi_char * channel, const gsi_char * user, const gsi_char * message, int type, void * obj );
	static void ChannelKicked(CHAT chat, const gsi_char * channel, const gsi_char * user, const gsi_char * reason, void * obj ); 
	static void EnumUsersCallback(CHAT chat, CHATBool success, const gsi_char * channel, int numUsers, const gsi_char ** users, int * modes, void * obj );
  static void ChannelUserLeft(CHAT chat, const gsi_char * channel, const gsi_char* user, int why, const gsi_char* reason, const gsi_char* kicker, void * obj );
  static void ChannelUserJoined(CHAT chat, const gsi_char * channel, const gsi_char* user, int mode, void * obj );
	static void GetKeysCallback(CHAT chat, CHATBool success, const gsi_char * channel, const gsi_char* user, int num,	const gsi_char ** keys,	const gsi_char ** values, void* obj);
	
	
	static void TimerCallback(NetTimerId,void*,CTimeValue);
	
	IChatListener*  m_listener;
	CHAT			      m_chat;
	NetTimerId		  m_updateTimer;
	string			    m_currentchannel;
  CGameSpy*       m_gamespy;
	std::list<SGetKeys>	m_getkeysList;
};

#endif //__GSCHAT_H__