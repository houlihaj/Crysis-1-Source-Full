#ifndef __SIMPLEHTTPSERVERLISTENER_H__
#define __SIMPLEHTTPSERVERLISTENER_H__

#pragma once

class CSimpleHttpServerListener : public IHttpServerListener, public IOutputPrintSink
{
public:
	static CSimpleHttpServerListener& GetSingleton(ISimpleHttpServer* http_server);
	static CSimpleHttpServerListener& GetSingleton();

	void Update();

private:
	CSimpleHttpServerListener();
	~CSimpleHttpServerListener();

	void OnStartResult(bool started, EResultDesc desc);

	void OnClientConnected(string client);
	void OnClientDisconnected();

	void OnGetRequest(string url);
	void OnRpcRequest(string xml);

	string m_output;
	void Print(const char* inszText);

	typedef std::deque<string> TCommandsVec;
	TCommandsVec m_commands;

	enum EAuthorizationState {eAS_Disconnected, eAS_WaitChallengeRequest, eAS_WaitAuthenticationRequest, eAS_Authorized};
	EAuthorizationState m_state;

	string m_client; // current session client
	string m_challenge;

	static CSimpleHttpServerListener s_singleton;
	static ISimpleHttpServer* s_http_server;
};

#endif

