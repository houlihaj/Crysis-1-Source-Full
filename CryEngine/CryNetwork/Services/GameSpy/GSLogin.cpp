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
#include "StdAfx.h"
#include "Network.h"
#include "GSLogin.h"

/*static const float UPDATE_INTERVAL = 0.1f;
static bool  VERBOSE = true;

CGSLogin::CGSLogin(CGameSpy* gamespy, IGSLoginListener* lst):
m_listener(lst),
m_gamespy(gamespy),
m_connection(0),
m_updateTimer(0)
{
}

CGSLogin::~CGSLogin()
{
	CleanUp();
}

void CGSLogin::Register(const char* nick, const char* email, const char* password)
{
	FROM_GAME(&CGSLogin::NC_Register,this, string(nick), string(email), string(password) );
}

void CGSLogin::Login(const char* nick, const char* email, const char* password)
{
	FROM_GAME(&CGSLogin::NC_Login,this, string(nick), string(email), string(password) );
	SCOPED_GLOBAL_LOCK;
	m_updateTimer = TIMER.AddTimer(g_time+UPDATE_INTERVAL,TimerCallback,this);
}

void CGSLogin::EnumUserNicks(const char* email, const char* password)
{
	FROM_GAME(&CGSLogin::NC_EnumNicks,this, string(email), string(password) );
}

bool	CGSLogin::IsDead()const
{
	return false;
}

void	CGSLogin::Init()
{
	GPResult res;
	res = gpInitialize(&m_connection, 10846, 0, 0);
	if(res == GP_NO_ERROR)
	{
		gpSetCallback(&m_connection,GP_ERROR,ErrorCallback,this);
	}
	else
	{
		TO_GAME(&CGSLogin::GC_Error,this,eGSLR_fatal);
	}
}

void	CGSLogin::Think()
{
	if(m_connection!=0)
	{
		gpProcess(&m_connection);
	}
}

void	CGSLogin::CleanUp()
{
	if(m_connection)
	{
		GPEnum res;
		gpIsConnected(&m_connection,&res);
		if(res == GP_CONNECTED)
			gpDisconnect(&m_connection);
		gpDestroy(&m_connection);
		m_connection = 0;
	}
}
	
void	CGSLogin::NC_Login(string nick, string email, string pwd)
{
	if(m_connection == 0)
		Init();
	if(m_connection)
		gpConnect(&m_connection, nick.c_str(),email.c_str(),pwd.c_str(),GP_FIREWALL,GP_NON_BLOCKING,ConnectCallback,this);
}

void	CGSLogin::NC_Register(string nick, string email, string pwd)
{
	if(m_connection == 0)
		Init();
	if(m_connection)
		gpConnectNewUser(&m_connection,nick.c_str(),0,email.c_str(),pwd.c_str(),0,GP_FIREWALL,GP_NON_BLOCKING,ConnectCallback,this);
}

void	CGSLogin::NC_EnumNicks(string email, string pwd)
{
	if(m_connection == 0)
		Init();
	if(m_connection)
		gpGetUserNicks(&m_connection,email.c_str(),pwd.c_str(),GP_NON_BLOCKING,EnumNicksCallback,this);
}

void	CGSLogin::GC_Error(EGSLoginResult res)
{
	if(m_listener)
		m_listener->LoginResult(res);
}

void	CGSLogin::GC_AddNick(string nick)
{
	if(m_listener)
		m_listener->AddNick(nick);
}

void	CGSLogin::ErrorCallback(GPConnection * connection, void * arg, void * obj)
{
	CGSLogin* pLogin = (CGSLogin*)obj;
	GPErrorArg* pError = (GPErrorArg*)arg;
	if(VERBOSE)
		NetWarning("GS Login error : %s",pError->errorString);
	TO_GAME(&CGSLogin::GC_Error,pLogin,eGSLR_fatal);
}

void	CGSLogin::ConnectCallback(GPConnection * connection, void * arg, void * obj)
{
	CGSLogin* pLogin = (CGSLogin*)obj;
	GPConnectResponseArg* pResp = (GPConnectResponseArg*)arg;
	if(pResp->result==GP_NO_ERROR)
	{
		if(VERBOSE)
			NetLog("GS Login: logged in.");
		pLogin->m_gamespy->OnLogedIn();
		TO_GAME(&CGSLogin::GC_Error,pLogin,eGSLR_ok);
	}
	else
	{
		TO_GAME(&CGSLogin::GC_Error,pLogin,eGSLR_fatal);
	}
}

void	CGSLogin::EnumNicksCallback(GPConnection * connection, void * arg, void * obj )
{
	CGSLogin* pLogin = (CGSLogin*)obj;
	GPGetUserNicksResponseArg* pResp = (GPGetUserNicksResponseArg*)arg;
	if(pResp->numNicks != 0)
	{
		for(uint i=0;i<pResp->numNicks;i++)
		{
			TO_GAME(&CGSLogin::GC_AddNick,pLogin,string(pResp->nicks[i]));
		}
	}
	else
	{
		TO_GAME(&CGSLogin::GC_Error,pLogin,eGSLR_password);
	}
}

void	CGSLogin::TimerCallback(NetTimerId,void* obj,CTimeValue)
{
	CGSLogin* pLogin = (CGSLogin*)obj;
	
	if(!pLogin->IsDead())
	{
		pLogin->Think();
		SCOPED_GLOBAL_LOCK;
		pLogin->m_updateTimer = TIMER.AddTimer( g_time+UPDATE_INTERVAL, TimerCallback, pLogin );
	}
}*/