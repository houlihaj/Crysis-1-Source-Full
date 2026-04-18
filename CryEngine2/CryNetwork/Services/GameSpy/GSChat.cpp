/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2006.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  Gamespy Chat Interface implementation
 -------------------------------------------------------------------------
 History:
 - 11/01/2006   : Stas Spivakov, Created
 
*************************************************************************/
#include "StdAfx.h"
#include "Network.h"
#include "GameSpy.h"
#include "GSChat.h"
#include "GSNetworkProfile.h"

static const float UPDATE_INTERVAL = 0.01f;
static const bool VERBOSE = true;
static const bool	ALLOW_GS_NICKS = false;

CGSChat::CGSChat(CGameSpy* gs):
m_listener(0),
m_chat(0),
m_updateTimer(0),
m_gamespy(gs)
{
	
}

CGSChat::~CGSChat()
{
	NC_Disconnect();
}


void	CGSChat::Join()
{
	FROM_GAME(&CGSChat::NC_Connect,this);
}

void	CGSChat::Leave()
{
	FROM_GAME(&CGSChat::NC_Disconnect,this);
}

void  CGSChat::SetListener(IChatListener* lst)
{
  SCOPED_GLOBAL_LOCK;
  m_listener = lst;
}

void	CGSChat::Say(const char* message)
{
	FROM_GAME(&CGSChat::NC_Say,this, string(message));
}

void	CGSChat::SendData(const char* nick,const char* message)
{
	FROM_GAME(&CGSChat::NC_Whisper,this, string(nick), string(message));
}

void	CGSChat::ListUsers()
{
	FROM_GAME(&CGSChat::NC_ListUsers,this);
}

void CGSChat::SetChatKeys(int num, const char** keys, const char** values)
{
	std::vector<string> t_keys(num);
	std::vector<string> t_values(num);
	for(int i=0;i<num;++i)
	{
		t_keys[i] = keys[i];
		t_values[i] = values[i];
	}
	FROM_GAME(&CGSChat::NC_SetChatKeys, this, t_keys, t_values);
}

void CGSChat::GetChatKeys(const char* user, int num, const char** keys)
{
	std::vector<string> t_keys(num);
	for(int i=0;i<num;++i)
	{
		t_keys[i] = keys[i];
	}
	FROM_GAME(&CGSChat::NC_GetChatKeys, this, string(user), t_keys);
}

bool	CGSChat::IsDead()const
{
	return false;
}

bool	CGSChat::IsAvailable()const
{
	return true;
}

void	CGSChat::LoggedIn()
{
	
}

void	CGSChat::Think()
{
	if(m_chat)
		chatThink(m_chat);
}


void	CGSChat::NC_Connect()
{
	chatGlobalCallbacks cgc;
	cgc.disconnected = DisconnectedCallback;
	cgc.invited = InvitedCallback;
	cgc.privateMessage = PrivateMessageCallback;
	cgc.raw = RawCallback;
	cgc.param = this;

  if(m_gamespy)
  {
    CGSNetworkProfile* m_prof = (CGSNetworkProfile*)m_gamespy->GetNetworkProfile();
    const SProfileInfo& pr = m_prof->GetProfileInfo();
    if(!pr.m_uniquenick.empty())
      m_chat = chatConnectLogin(0,0, GAMESPY_NAMESPACE_ID,"","",pr.m_uniquenick,pr.m_password,0,GAMESPY_GAME_NAME,"zKbZiM",&cgc,NickErrorCallback,0,ConnectCallback,this,CHATFalse);
    else
    {
      TO_GAME(&CGSChat::GC_Error,this,0);
    }
  }

	if(m_chat != 0)
	{
		m_updateTimer = TIMER.AddTimer( g_time+UPDATE_INTERVAL, TimerCallback, this );
	}
}

void CGSChat::NC_Disconnect()
{
	if( m_chat!= 0 )
	{
    chatLeaveChannel(m_chat,GAMESPY_CHANNEL_NAME,"Logged off");
		chatDisconnect(m_chat);
		m_chat = 0;
		if(m_updateTimer)
		{
			SCOPED_GLOBAL_LOCK;
			TIMER.CancelTimer(m_updateTimer);
			m_updateTimer = 0;
		}
	}
}

void	CGSChat::NC_Say(string msg)
{
	if( !m_chat )
		return;
  if(m_currentchannel.empty())
    return;
	chatSendChannelMessage(m_chat, m_currentchannel, msg, 0);
}

void	CGSChat::NC_Whisper(string to, string msg)
{
	if( !m_chat )
		return;
	chatSendUserMessage(m_chat, to, msg, 0);
}

void	CGSChat::NC_ListUsers()
{
	if( !m_chat )
		return;
	chatEnumUsers(m_chat,m_currentchannel.c_str(),EnumUsersCallback,this,CHATFalse);
}

void  CGSChat::NC_SetChatKeys(std::vector<string> keys, std::vector<string> values)
{
	if(!m_chat)
		return;
	std::vector<const char*> p_keys(keys.size());
	std::vector<const char*> p_values(values.size());
	for(int i=0;i<keys.size();++i)
	{
		p_keys[i] = keys[i].c_str();
		p_values[i] = values[i].c_str();
	}
	CGSNetworkProfile* m_prof = (CGSNetworkProfile*)m_gamespy->GetNetworkProfile();
	const SProfileInfo& pr = m_prof->GetProfileInfo();
	if(!pr.m_uniquenick.empty())
	{
		CryFixedStringT<32> nick = pr.m_uniquenick;
		nick += GAMESPY_NAMESPACE_EXT;
		chatSetChannelKeys(m_chat,GAMESPY_CHANNEL_NAME, nick.c_str(), keys.size(),&p_keys[0],&p_values[0]);
	}
}

void  CGSChat::NC_GetChatKeys(string user, std::vector<string> keys)
{
	if(!m_chat)
		return;
	std::vector<const char*> p_keys(keys.size());
	for(int i=0;i<keys.size();++i)
	{
		p_keys[i] = keys[i].c_str();
	}
	CryFixedStringT<32> nick = user;
	nick += GAMESPY_NAMESPACE_EXT;
	SGetKeys cb;
	cb.pChat = this;
	cb.user = user;
	m_getkeysList.push_front(cb);
	chatGetChannelKeys(m_chat,GAMESPY_CHANNEL_NAME, nick.c_str(), keys.size(),&p_keys[0], GetKeysCallback, &m_getkeysList.front(), CHATFalse);
}

void	CGSChat::GC_Joined(CHATEnterResult res)
{
	if(!m_listener)
		return;
	switch(res)
	{
		case CHATEnterSuccess:
			m_listener->Joined(eCJR_Success);
			break;
		case CHATBadChannelName:
			m_listener->Joined(eCJR_ChannelError);
			break;
		case CHATChannelIsFull:
			m_listener->Joined(eCJR_ChannelError);
			break;
		case CHATInviteOnlyChannel:
			m_listener->Joined(eCJR_ChannelError);
			break;
		case CHATBannedFromChannel:
			m_listener->Joined(eCJR_ChannelError);
			break;
		case CHATBadChannelPassword:
			m_listener->Joined(eCJR_ChannelError);
			break;
		case CHATTooManyChannels:
			m_listener->Joined(eCJR_ChannelError);
			break;
		case CHATEnterTimedOut:
			m_listener->Joined(eCJR_ChannelError);
			break;
	}
}

void	CGSChat::GC_Message(string from, string msg)
{
	if(m_listener)
		m_listener->Message(from.c_str(),msg.c_str(),eNCMT_say);
}

void	CGSChat::GC_PrivateMessage(string from, string msg)
{
	if(m_listener)
		m_listener->Message(from.c_str(),msg.c_str(),eNCMT_data);
}

void	CGSChat::GC_User(string nick, EChatUserStatus what)
{
	if(m_listener)
		m_listener->ChatUser(nick,what);
}

void	CGSChat::GC_OnKeys(string user, std::vector<string> keys, std::vector<string> values)
{
	if(m_listener)
	{
		std::vector<const char*> p_keys(keys.size());
		std::vector<const char*> p_values(values.size());
		for(int i=0;i<keys.size();++i)
		{
			p_keys[i] = keys[i].c_str();
			p_values[i] = values[i].c_str();
		}
		m_listener->OnChatKeys(user,keys.size(),&p_keys[0], &p_values[0]);
	}
}

void	CGSChat::GC_OnGetKeysError(string user)
{
	if(m_listener)
		m_listener->OnGetKeysFailed(user.c_str());
}

void  CGSChat::GC_Error(int err)
{
  if(m_listener)
    m_listener->OnError(err);
}
  
void	CGSChat::RawCallback(CHAT chat, const gsi_char * raw, void * obj )
{
	CGSChat* pChat = (CGSChat*)obj;
	//NetLog(raw);
}

void	CGSChat::DisconnectedCallback(CHAT chat, const gsi_char * reason, void * obj )
{
	CGSChat* pChat = (CGSChat*)obj;
	pChat->m_chat = 0;
}

void	CGSChat::PrivateMessageCallback(CHAT chat, const gsi_char * user, const gsi_char * message, int type, void * obj )
{
	CGSChat* pChat = (CGSChat*)obj;
	TO_GAME(&CGSChat::GC_PrivateMessage, pChat, string(user), string(message));
}

void	CGSChat::InvitedCallback(CHAT chat, const gsi_char * channel, const gsi_char * user, void * obj )
{
	CGSChat* pChat = (CGSChat*)obj;
}

void	CGSChat::NickErrorCallback(CHAT chat, int type, const gsi_char * nick, int numSuggestedNicks, const gsi_char ** suggestedNicks, void * obj )
{
	CGSChat* pChat = (CGSChat*)obj;
}

void	CGSChat::ConnectCallback(CHAT chat, CHATBool success, int failureReason, void * obj )
{
	CGSChat* pChat = (CGSChat*)obj;
	if(success)
	{
		pChat->m_chat = chat;
		chatEnumChannels(pChat->m_chat,"*",ChannelEnumCallbackEach, ChannelEnumCallbackAll,pChat,CHATFalse);
		
		chatChannelCallbacks chan_callbacks;
		chan_callbacks.broadcastKeyChanged = 0;
		chan_callbacks.channelMessage = ChannelMessage;
		chan_callbacks.channelModeChanged = 0;
		chan_callbacks.kicked = ChannelKicked;
		chan_callbacks.newUserList = 0;
		chan_callbacks.param = pChat;
		chan_callbacks.topicChanged = 0;
		chan_callbacks.userChangedNick = 0;
		chan_callbacks.userListUpdated = 0;
		chan_callbacks.userModeChanged = 0;
		chan_callbacks.userParted = ChannelUserLeft;
    chan_callbacks.userJoined = ChannelUserJoined;

		chatEnterChannel(pChat->m_chat,GAMESPY_CHANNEL_NAME,0,&chan_callbacks,EnterChannelCallback,pChat,CHATFalse);
	}
	else
	{
		switch(failureReason)
		{
		case CHAT_DISCONNECTED:
			NetWarning("GS Chat : ChatConnect failed. Reason : CHAT_DISCONNECTED");
			break;
		case CHAT_NICK_ERROR:
			NetWarning("GS Chat : ChatConnect failed. Reason : CHAT_NICK_ERROR");
			break;
		case CHAT_LOGIN_FAILED:
			NetWarning("GS Chat : ChatConnect failed. Reason : CHAT_LOGIN_FAILED");
			break;
		}
	}
}

void CGSChat::ChannelEnumCallbackEach(CHAT chat, CHATBool success, int index, const gsi_char * channel, const gsi_char * topic, int numUsers, void * obj )
{
	
}

void CGSChat::ChannelEnumCallbackAll(CHAT chat, CHATBool success, int numChannels, const gsi_char ** channels, const gsi_char ** topics, int * numUsers, void * obj )
{

}

void CGSChat::EnterChannelCallback(CHAT chat, CHATBool success, CHATEnterResult result, const gsi_char * channel, void * obj )
{
	CGSChat* pChat = (CGSChat*)obj;
	if(success)
	{
		NetLog("GS Chat : Joined channel \'%s\'",channel);
		pChat->m_currentchannel = channel;
    pChat->NC_ListUsers();
	}
	TO_GAME(&CGSChat::GC_Joined,pChat,result);
}

void CGSChat::ChannelMessage(CHAT chat, const gsi_char * channel, const gsi_char * user, const gsi_char * message, int type, void * obj )
{
	CGSChat* pChat = (CGSChat*)obj;
  CryFixedStringT<32> nick = user;
	const char* tr = chatTranslateNick((char*)nick.c_str(),GAMESPY_NAMESPACE_EXT);
	if(tr || ALLOW_GS_NICKS)
	{
		TO_GAME(&CGSChat::GC_Message, pChat, string(tr?tr:user), string(message));
	}
}

void CGSChat::ChannelKicked(CHAT chat, const gsi_char * channel, const gsi_char * user, const gsi_char * reason, void * obj )
{
  CGSChat* pChat = (CGSChat*)obj;
  CryFixedStringT<32> nick = user;
	const char* tr = chatTranslateNick((char*)nick.c_str(),GAMESPY_NAMESPACE_EXT);
	NetWarning("Kicked from channel %s by %s. Reason : %s",channel, tr?tr:user, reason);
  pChat->m_currentchannel = "";
}

void CGSChat::EnumUsersCallback(CHAT chat, CHATBool success, const gsi_char * channel, int numUsers, const gsi_char ** users, int * modes, void * obj )
{
  CGSChat* pChat = (CGSChat*)obj;
  if(success == CHATTrue)
  {
    for(uint i=0; i<numUsers; i++)
    {
			CryFixedStringT<32> nick = users[i];
			const char* tr = chatTranslateNick((char*)nick.c_str(),GAMESPY_NAMESPACE_EXT);
			if(tr || ALLOW_GS_NICKS)
			{
				TO_GAME(&CGSChat::GC_User,pChat,string(tr?tr:users[i]),eCUS_inchannel);
			}
    }
  }
}

void CGSChat::ChannelUserLeft(CHAT chat, const gsi_char * channel, const gsi_char* user, int why, const gsi_char* reason, const gsi_char* kicker, void * obj )
{
  CGSChat* pChat = (CGSChat*)obj;
  CryFixedStringT<32> nick = user;
	const char* tr = chatTranslateNick((char*)nick.c_str(),GAMESPY_NAMESPACE_EXT);
	if(tr || ALLOW_GS_NICKS)
	{
		TO_GAME(&CGSChat::GC_User,pChat,string(tr?tr:user),eCUS_left);
	}
}

void CGSChat::ChannelUserJoined(CHAT chat, const gsi_char * channel, const gsi_char* user, int mode, void * obj )
{
	CGSChat* pChat = (CGSChat*)obj;
	CryFixedStringT<32> nick = user;
	const char* tr = chatTranslateNick((char*)nick.c_str(),GAMESPY_NAMESPACE_EXT);
	if(tr || ALLOW_GS_NICKS)
	{
		TO_GAME(&CGSChat::GC_User,pChat,string(tr?tr:user),eCUS_joined);
	}
}

void CGSChat::GetKeysCallback(CHAT chat, CHATBool success, const gsi_char * channel, const gsi_char* user, int num,	const gsi_char ** keys,	const gsi_char ** values, void* obj)
{
	SGetKeys* pCB = static_cast<SGetKeys*>(obj);
	CGSChat* pChat = pCB->pChat;
	string name = pCB->user;
	for(std::list<SGetKeys>::iterator it = pChat->m_getkeysList.begin(),eit = pChat->m_getkeysList.end();it!=eit;++it )
	{
		if(&(*it) == pCB)
		{
			pChat->m_getkeysList.erase(it);
			break;
		}
	}

	if(!success)
	{
		TO_GAME(&CGSChat::GC_OnGetKeysError, pChat, name );
		return;
	}
 
	std::vector<string> t_keys(num);
	std::vector<string> t_values(num);
	for(int i=0;i<num;++i)
	{
		t_keys[i] = keys[i];
		t_values[i] = values[i];
	}
	CryFixedStringT<32> nick = user;
	const char* tr = chatTranslateNick((char*)nick.c_str(),GAMESPY_NAMESPACE_EXT);
	if(tr || ALLOW_GS_NICKS)
	{
		TO_GAME(&CGSChat::GC_OnKeys,pChat,string(tr),t_keys, t_values);
	}
}

void CGSChat::TimerCallback(NetTimerId id, void* obj, CTimeValue)
{
	CGSChat* pChat = (CGSChat*)obj;
	
	if(!pChat->IsDead() && pChat->m_updateTimer == id)
	{
		pChat->Think();
		SCOPED_GLOBAL_LOCK;
		pChat->m_updateTimer = TIMER.AddTimer( g_time+UPDATE_INTERVAL, TimerCallback, pChat );
	}
}
