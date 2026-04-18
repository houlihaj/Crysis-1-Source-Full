#ifndef __SIMPLEHTTPSERVER_H__
#define __SIMPLEHTTPSERVER_H__

#pragma once

#include "ISimpleHttpServer.h"

static const string USERNAME = "anonymous";

class CSimpleHttpServer;

class CSimpleHttpServerInternal : public IStreamListener
{
	friend class CSimpleHttpServer;

public:
	void Start(uint16 port, string password);
	void Stop();
	void SendResponse(ISimpleHttpServer::EStatusCode statusCode, ISimpleHttpServer::EContentType contentType, string content, bool closeConnection);
	void SendWebpage(string webpage);

	void OnConnectionAccepted(IStreamSocketPtr pStreamSocket);
	void OnConnectCompleted(bool succeeded) { NET_ASSERT(0); }
	void OnConnectionClosed(bool graceful);
	void OnIncomingData(const uint8* pData, size_t nSize);

	void AddRef() const {}
	void Release() const {}
	bool IsDead() const { return false; }

	void BuildNormalResponse(string& response, ISimpleHttpServer::EStatusCode statusCode, ISimpleHttpServer::EContentType contentType, const string& content);
	void BuildUnauthorizedResponse(string& response);

	void BadRequest(); // helper
	void NotImplemented();

	string NextToken(string& line);
	bool ParseHeaderLine(string& line);
	void IgnorePending() { m_sessionState=eSS_IgnorePending; };

	bool ParseDigestAuthorization(string& line);

private:
	CSimpleHttpServerInternal(CSimpleHttpServer* pServer);
	~CSimpleHttpServerInternal();

	CSimpleHttpServer* m_pServer;

	IStreamSocketPtr m_pSocketListen;
	IStreamSocketPtr m_pSocketSession;

	TNetAddress m_remoteAddr;

	string m_password;
	string m_hostname;

	string m_realm;
	string m_nonce;
	string m_opaque;

	enum ESessionState {eSS_Unsessioned, eSS_WaitFirstRequest, eSS_ChallengeSent, eSS_Authorized, eSS_IgnorePending};
	ESessionState m_sessionState;

	enum EReceivingState {eRS_ReceivingHeaders, eRS_ReceivingContent, eRS_Ignoring};
	EReceivingState m_receivingState;

	enum ERequestType {eRT_None, eRT_Get, eRT_Rpc};
	ERequestType m_requestType;

	size_t m_remainingContentBytes;

	float m_authenticationTimeoutStart;
	float m_lineReadingTimeoutStart;

	string m_line, m_content;

	static const string s_statusCodes[];
	static const string s_contentTypes[];
};

class CSimpleHttpServer : public ISimpleHttpServer
{
	friend class CSimpleHttpServerInternal;

public:
	static CSimpleHttpServer& GetSingleton();

	void Start(uint16 port, const string& password, IHttpServerListener* pListener);
	void Stop();
	void SendResponse(EStatusCode statusCode, EContentType contentType, const string& content, bool closeConnection);
	void SendWebpage(const string& webpage);

private:
	CSimpleHttpServer();
	~CSimpleHttpServer();

	CSimpleHttpServerInternal m_internal;

	IHttpServerListener* m_pListener;

	static CSimpleHttpServer s_singleton;
};

#endif

