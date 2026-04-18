/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2006.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  Gamespy Chat Interface implementation
 -------------------------------------------------------------------------
 History:
 - 11/06/2006   : Stas Spivakov, Created
 *************************************************************************/

#ifndef __GSLOGIN_H__
#define __GSLOGIN_H__
#pragma once

#include "GameSpy.h"
#include "GameSpy/gp/gp.h"
/*
class CGSLogin : public CMultiThreadRefCount, public IGSLogin
{
public:
	CGSLogin(CGameSpy* gamespy, IGSLoginListener* lst);
	~CGSLogin();
	
	virtual void Register(const char* nick, const char* email, const char* password);
	virtual void Login(const char* nick, const char* email, const char* password);
	virtual void EnumUserNicks(const char* email, const char* password);

	bool	IsDead()const;

	void	Init();
	void	Think();
	void	CleanUp();

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CGSLogin");

		pSizer->Add(*this);
	}
	
private:

	void	NC_Login(string nick, string email, string pwd);
	void	NC_Register(string nick, string email, string pwd);
	void	NC_EnumNicks(string email, string pwd);
	
	void	GC_Error(EGSLoginResult);
	void	GC_AddNick(string nick);

	//Gamespycallbacks
	static void	ErrorCallback(GPConnection * connection, void * arg, void * obj );
	static void	ConnectCallback(GPConnection * connection, void * arg, void * obj );
	static void	EnumNicksCallback(GPConnection * connection, void * arg, void * obj );

	static void TimerCallback(NetTimerId,void*,CTimeValue);

	IGSLoginListener*		m_listener;
	_smart_ptr<CGameSpy>	m_gamespy;
	GPConnection			m_connection;
	NetTimerId				m_updateTimer;
};
*/
#endif /*__GSLOGIN_H__*/