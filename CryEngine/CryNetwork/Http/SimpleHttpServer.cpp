#include "StdAfx.h"
#include "Network.h"
#include "SimpleHttpServer.h"

//#include <sstream>

#pragma warning(disable:4355)

#define CR '\r'
#define LF '\n'
#define CRLF "\r\n"

CSimpleHttpServer CSimpleHttpServer::s_singleton;

CSimpleHttpServer& CSimpleHttpServer::GetSingleton()
{
	return s_singleton;
}

CSimpleHttpServer::CSimpleHttpServer() : m_internal(this)
{
	m_pListener = NULL;
}

CSimpleHttpServer::~CSimpleHttpServer()
{

}

void CSimpleHttpServer::Start(uint16 port, const string& password, IHttpServerListener* pListener)
{
	m_pListener = pListener;

	FROM_GAME(&CSimpleHttpServerInternal::Start, &m_internal, port, password);
}

void CSimpleHttpServer::Stop()
{
	FROM_GAME(&CSimpleHttpServerInternal::Stop, &m_internal);
}

void CSimpleHttpServer::SendResponse(EStatusCode statusCode, EContentType contentType, const string& content, bool closeConnection)
{
	FROM_GAME(&CSimpleHttpServerInternal::SendResponse, &m_internal, statusCode, contentType, content, closeConnection);
}

void CSimpleHttpServer::SendWebpage(const string& webpage)
{
	FROM_GAME(&CSimpleHttpServerInternal::SendWebpage, &m_internal, webpage);
}

// three-digit HTTP response status codes in the form of strings, the order must match that of EStatusCode
const string CSimpleHttpServerInternal::s_statusCodes[] = {
	"200 OK",
	"400 Bad Request",
	"404 Not Found",
	"408 Request Timeout",
	"501 Not Implemented",
	"503 Service Unavailable",
	"505 Bad HTTP Version"
};

const string CSimpleHttpServerInternal::s_contentTypes[] = {"html", "xml"};

CSimpleHttpServerInternal::CSimpleHttpServerInternal(CSimpleHttpServer* pServer)
{
	m_pServer = pServer;

	m_sessionState = eSS_Unsessioned;

	m_authenticationTimeoutStart = 0.0f;

	m_lineReadingTimeoutStart = 0.0f;

	m_receivingState = eRS_ReceivingHeaders;

	m_remainingContentBytes = 0;

	m_requestType = eRT_None;
}

CSimpleHttpServerInternal::~CSimpleHttpServerInternal()
{

}

void CSimpleHttpServerInternal::Start(uint16 port, string password)
{
	if (m_pSocketListen != NULL)
	{
		TO_GAME(&IHttpServerListener::OnStartResult, m_pServer->m_pListener, false, IHttpServerListener::eRD_AlreadyStarted);
		return;
	}

	m_pSocketListen = CreateStreamSocket(TNetAddress(SIPv4Addr(0, port)));
	NET_ASSERT(m_pSocketListen != NULL);

	m_pSocketListen->SetListener(this);

	bool r = m_pSocketListen->Listen(TNetAddress(SIPv4Addr(0, port)));
	if (!r)
	{
		m_pSocketListen->Close();
		m_pSocketListen = NULL;
		TO_GAME(&IHttpServerListener::OnStartResult, m_pServer->m_pListener,
			false, IHttpServerListener::eRD_Failed);
		return;
	}

	m_password = password;

	m_hostname = CNetwork::Get()->GetHostName();
	CNameRequestPtr pNR = RESOLVER.RequestNameLookup(m_hostname);
	pNR->Wait();
	if (eNRR_Succeeded == pNR->GetResult())
		m_hostname = pNR->GetAddrString();

	m_realm = "CryHttp@" + m_hostname;

	TO_GAME(&IHttpServerListener::OnStartResult, m_pServer->m_pListener,
		true, IHttpServerListener::eRD_Okay);
}

void CSimpleHttpServerInternal::Stop()
{
	if (m_pSocketSession)
	{
		m_pSocketSession->Close();
		m_pSocketSession = NULL;
	}

	if (m_pSocketListen)
	{
		m_pSocketListen->Close();
		m_pSocketListen = NULL;
	}

	m_remoteAddr = TNetAddress(SIPv4Addr());

	m_password = "";

	m_sessionState = eSS_Unsessioned;
}

void CSimpleHttpServerInternal::SendResponse(ISimpleHttpServer::EStatusCode statusCode, ISimpleHttpServer::EContentType contentType, string content, bool closeConnection)
{
	if (!m_pSocketSession /*|| m_sessionState != eSS_Authorized*/)
		return;

	string response;
	BuildNormalResponse(response, statusCode, contentType, content);
	m_pSocketSession->Send((const uint8*)response.data(), response.size());

	if (closeConnection)
	{
		m_pSocketSession->Close();
		m_pSocketSession = NULL;
		m_sessionState = eSS_Unsessioned;
		m_remoteAddr = TNetAddress(SIPv4Addr());
	}
}

void CSimpleHttpServerInternal::SendWebpage(string webpage)
{
	if (!m_pSocketSession)
		return;

	string response = "HTTP/1.1 200 OK" CRLF;
	response += "Content-Type: text/rfc822" CRLF;
	//std::stringstream contentLength; contentLength << webpage.size();
	char contentLength[33]; _snprintf(contentLength, sizeof(contentLength)-1, "%u", (unsigned)webpage.size()); contentLength[sizeof(contentLength)-1] = 0;
	response += string("Content-Length: ") + string(contentLength) + string(CRLF) + string(CRLF);

	response += webpage;
	m_pSocketSession->Send((const uint8*)response.data(), response.size());
}

void CSimpleHttpServerInternal::OnConnectionAccepted(IStreamSocketPtr pStreamSocket)
{
	if (m_pSocketSession)
	{
		string response;
		BuildNormalResponse(response, ISimpleHttpServer::eSC_ServiceUnavailable, ISimpleHttpServer::eCT_HTML, "<HTML><HEAD><TITLE>Service Unavailable</TITLE></HEAD></HTML>");
		pStreamSocket->Send((const uint8*)response.data(), response.size());
		pStreamSocket->Close();
	}
	else
	{
		m_pSocketSession = pStreamSocket;

		m_pSocketSession->SetListener(this); // in this specific application, the same listener is used for both sockets
		m_pSocketSession->GetPeerAddr(m_remoteAddr);

		//m_sessionState = eSS_WaitFirstRequest;
		//m_authenticationTimeoutStart = gEnv->pTimer->GetAsyncCurTime();
		//m_lineReadingTimeoutStart = gEnv->pTimer->GetAsyncCurTime();
		// TODO: should reset all the relevant states here

		m_requestType = eRT_None;
		m_receivingState = eRS_ReceivingHeaders;
		m_remainingContentBytes = 0;
		m_line = "";
		m_content = "";

		TO_GAME(&IHttpServerListener::OnClientConnected, m_pServer->m_pListener, string(RESOLVER.ToString(m_remoteAddr)));
	}
}

void CSimpleHttpServerInternal::OnConnectionClosed(bool graceful)
{
	if (m_pSocketSession)
	{
		m_pSocketSession->Close();
		m_pSocketSession = NULL;
		//if (m_sessionState == eSS_Authorized)
		TO_GAME(&IHttpServerListener::OnClientDisconnected, m_pServer->m_pListener);
		m_remoteAddr = TNetAddress(SIPv4Addr());
		m_nonce = "";
		m_opaque = "";
		m_sessionState = eSS_Unsessioned;
	}
}

#define TOKENSEPAS ":=" // separators that are also tokens
#define SEPARATORS " \t,;" // separators that are not tokens

string CSimpleHttpServerInternal::NextToken(string& line)
{
	if (line.empty())
		return "";

	// first skip leading non-token separators
	size_t start = line.find_first_not_of(SEPARATORS);
	line = line.substr(start);

	if (line.empty())
		return "";

	size_t index = line.find_first_of(TOKENSEPAS);
	if (0 == index)
	{
		string token = line.substr(0, 1);
		line = line.substr(1);
		return token;
	}

	index = line.find_first_of(SEPARATORS TOKENSEPAS);
	string token = line.substr(0, index);
	line = line.substr(index);
	return token;
}

bool CSimpleHttpServerInternal::ParseHeaderLine(string& line)
{
	string token = NextToken(line).MakeLower();

	if (m_requestType == eRT_None)
	{
		if ("get" == token)
		{
			m_requestType = eRT_Get;
			string uri = NextToken(line);
			if ( !uri.empty() )
			{
				token = NextToken(line).MakeLower();
				if ("http/1.0" == token || "http/1.1" == token || "" == token)
				{
					//string from(RESOLVER.ToString(m_remoteAddr).c_str());
					TO_GAME(&IHttpServerListener::OnGetRequest, m_pServer->m_pListener, uri);
					//if (m_sessionState == eSS_Authorized)
					//{
					//	TO_GAME_FEEDBACK_QUEUE1(m_pServer, &IHttpServerListener::OnGetRequest, m_pServer->m_pListener, uri);
					//}
					//else if (m_sessionState == eSS_WaitFirstRequest)
					//{
					//	string response;
					//	BuildUnauthorizedResponse(response);
					//	m_pSocketSession->Send((uint8*)response.data(), response.size());

					//	m_sessionState = eSS_ChallengeSent;
					//}
					//m_lineReadingTimeoutStart = gEnv->pTimer->GetAsyncCurTime();
					return true;
				}
			}
			TO_GAME(&CSimpleHttpServerInternal::BadRequest, this);
			return false;
		}

		if ("post" == token)
		{
			string uri = NextToken(line).MakeLower();
			if ("/rpc2" == uri)
			{
				token = NextToken(line).MakeLower();
				if ("http/1.0" == token || "http/1.1" == token || "" == token)
				{
					m_requestType = eRT_Rpc;
					//if (m_sessionState == eSS_WaitFirstRequest)
					//{
					//	string response;
					//	BuildUnauthorizedResponse(response);
					//	m_pSocketSession->Send((uint8*)response.data(), response.size());

					//	m_sessionState = eSS_ChallengeSent;
					//}
					//m_lineReadingTimeoutStart = gEnv->pTimer->GetAsyncCurTime();
					return true;
				}
			}
			TO_GAME(&CSimpleHttpServerInternal::BadRequest, this);
			return false;
		}

		// other requests go here (but we probably won't support them)
		TO_GAME(&CSimpleHttpServerInternal::NotImplemented, this);
		return false;
	}

	//if ("authorization" == token)
	//{
	//	if ( ":" == NextToken(line) )
	//	{
	//		token = NextToken(line).MakeLower();
	//		if ("digest" == token)
	//		{
	//			if ( ParseDigestAuthorization(line) )
	//				return true;
	//		}
	//	}
	//	BadRequest();
	//	return false;
	//}

	if ("content-length" == token)
	{
		if (m_requestType != eRT_Rpc)
		{
			TO_GAME(&CSimpleHttpServerInternal::BadRequest, this);
			return false;
		}

		if ( ":" == NextToken(line) )
		{
			token = NextToken(line);
			m_remainingContentBytes = atoi(token.c_str());
			if (m_remainingContentBytes > 0)
				return true;
		}

		TO_GAME(&CSimpleHttpServerInternal::BadRequest, this);
		return false;
	}

	if ("content-type" == token)
	{
		if (m_requestType != eRT_Rpc)
		{
			TO_GAME(&CSimpleHttpServerInternal::BadRequest, this);
			return false;
		}

		if ( ":" == NextToken(line) )
			if ( "text/xml" == NextToken(line).MakeLower() )
				return true;

		TO_GAME(&CSimpleHttpServerInternal::BadRequest, this);
		return false;
	}

	// check for an empty line
	if ("" == token)
	{
		switch (m_requestType)
		{
		case eRT_Get:
			m_requestType = eRT_None;
			break;

		case eRT_Rpc:
			m_receivingState = eRS_ReceivingContent;
			break;

		default:
			TO_GAME(&CSimpleHttpServerInternal::BadRequest, this);
			return false;
		}

		return true;
	}

	// ignore the remaining header lines
	return true;
}

void CSimpleHttpServerInternal::OnIncomingData(const uint8* pData, size_t nSize)
{
	const uint8* pBuffer = pData;
	size_t nLength = nSize;;
	while (nLength > 0)
	{
		if (m_receivingState == eRS_Ignoring)
			break;
		else if (m_receivingState == eRS_ReceivingHeaders)
		{
			if (CR != pBuffer[0] && LF != pBuffer[0] && '\0' != pBuffer[0])
				m_line += pBuffer[0];
			else if (CR == pBuffer[0])
			{
				if ( m_line.empty() || CR != m_line.c_str()[m_line.length()-1] )
					m_line += pBuffer[0];
				else
				{
					TO_GAME(&CSimpleHttpServerInternal::BadRequest, this);
					break;
				}
			}
			else if ( LF == pBuffer[0])
			{
				if ( !m_line.empty() && CR == m_line.c_str()[m_line.length()-1] )
				{
					string sub = m_line.substr(0, m_line.length()-1);
					if ( !ParseHeaderLine( sub ) )
					{
						m_receivingState = eRS_Ignoring;
						break;
					}

					m_line = "";
				}
				else
				{
					m_receivingState = eRS_Ignoring;
					TO_GAME(&CSimpleHttpServerInternal::BadRequest, this);
					break;
				}
			}
			else // '\0'
			{
				m_receivingState = eRS_Ignoring;
				TO_GAME(&CSimpleHttpServerInternal::BadRequest, this);
				break;
			}

			++pBuffer;
			--nLength;
		}
		else if (m_receivingState == eRS_ReceivingContent)
		{
			size_t toAppend = min(nLength, m_remainingContentBytes);
			m_content.append((char*)pBuffer, toAppend);
			pBuffer += toAppend;
			nLength -= toAppend;
			m_remainingContentBytes -= toAppend;
			if (m_remainingContentBytes == 0)
			{
				switch (m_requestType)
				{
				case eRT_Rpc:
					//if (m_sessionState == eSS_Authorized)
					//string from(RESOLVER.ToString(m_remoteAddr).c_str());
					TO_GAME(&IHttpServerListener::OnRpcRequest, m_pServer->m_pListener, m_content);
					break;
				}

				m_content = ""; // discard content even if not handled
				m_requestType = eRT_None;
				m_receivingState = eRS_ReceivingHeaders;
			}
		}
	}
}

void CSimpleHttpServerInternal::BuildNormalResponse(string& response, ISimpleHttpServer::EStatusCode statusCode, ISimpleHttpServer::EContentType contentType, const string& content)
{
	response = "";

	response += "HTTP/1.1 ";
	response += s_statusCodes[statusCode];
	response += CRLF;

	response += "Server: CryHttp/0.0.1";
	response += CRLF;

	//response += "Connection: close";
	//response += CRLF;

	response += "Content-Type: text/";
	response += s_contentTypes[contentType];
	response += CRLF;

	char buf[33]; ltoa(content.size(), buf, 10);
	response += "Content-Length: ";
	response += buf;
	response += CRLF;
	response += CRLF;

	response += content;
	//response += CRLF;
	//response += CRLF;
}

void CSimpleHttpServerInternal::BuildUnauthorizedResponse(string& response)
{
	response = "";

	response += "HTTP/1.1 401 Unauthorized";
	response += CRLF;

	response += "Server: CryHttp/0.0.1";
	response += CRLF;

	char timestamp[33]; _snprintf(timestamp, sizeof(timestamp)-1, "%f", gEnv->pTimer->GetAsyncCurTime()); timestamp[sizeof(timestamp)-1] = 0;
	string raw = RESOLVER.ToNumericString(m_remoteAddr) + ":" + timestamp + ":" + "luolin";
	{ CWhirlpoolHash hash(raw); m_nonce = hash.GetHumanReadable(); }
	{ CWhirlpoolHash hash((uint8*)timestamp, strlen(timestamp)); m_opaque = hash.GetHumanReadable(); }
	response += "WWW-Authenticate: Digest ";
	response += "realm=\"";
	response += m_realm;
	response += "\", ";
	response += "nonce=\"";
	response += m_nonce;
	response += "\", ";
	response += "opaque=\"";
	response += m_opaque;
	response += "\"";
	response += CRLF;
	response += CRLF;
}

void CSimpleHttpServerInternal::BadRequest()
{
	if (!m_pSocketSession)
		return;

	string response;
	BuildNormalResponse(response, ISimpleHttpServer::eSC_BadRequest, ISimpleHttpServer::eCT_HTML, "<HTML><HEAD><TITLE>Bad Request</TITLE></HEAD></HTML>");
	m_pSocketSession->Send((uint8*)response.data(), response.size());
	m_pSocketSession->Close();
	m_pSocketSession = NULL;
	m_sessionState = eSS_Unsessioned;
	m_remoteAddr = TNetAddress(SIPv4Addr());
}

void CSimpleHttpServerInternal::NotImplemented()
{
	if (!m_pSocketSession)
		return;

	string response;
	BuildNormalResponse(response, ISimpleHttpServer::eSC_NotImplemented, ISimpleHttpServer::eCT_HTML, "<HTML><HEAD><TITLE>Not Implemented</TITLE></HEAD></HTML>");
	m_pSocketSession->Send((uint8*)response.data(), response.size());
	m_pSocketSession->Close();
	m_pSocketSession = NULL;
	m_sessionState = eSS_Unsessioned;
	m_remoteAddr = TNetAddress(SIPv4Addr());
}

bool CSimpleHttpServerInternal::ParseDigestAuthorization(string& line)
{
	// TODO:
	return false;
}

