// !A TCP stream socket implementation - lluo

#include "StdAfx.h"
#include "Network.h"
#include "TCPStreamSocket.h"

void CTCPStreamSocket::SetSocketState( ESocketState state )
{
	if (m_socketState == state)
		return;

	ISendTarget	* pSendTarget = 0;
	IRecvTarget * pRecvTarget = 0;
	IConnectTarget * pConnectTarget = 0;
	IAcceptTarget * pAcceptTarget = 0;

	switch (state)
	{
	case eSS_Listening:
		pAcceptTarget = this;
		break;
	case eSS_Connecting:
		pConnectTarget = this;
		break;
	case eSS_Established:
		pRecvTarget = this;
		pSendTarget = this;
		break;
	}

	m_pSockIO->SetRecvTarget(m_sockid, pRecvTarget);
	m_pSockIO->SetSendTarget(m_sockid, pSendTarget);
	m_pSockIO->SetConnectTarget(m_sockid, pConnectTarget);
	m_pSockIO->SetAcceptTarget(m_sockid, pAcceptTarget);

	if (pAcceptTarget)
		m_pSockIO->RequestAccept(m_sockid);
	if (pRecvTarget)
		m_pSockIO->RequestRecv(m_sockid);

	m_socketState = state;
}

//void SListenVisitor::Visit(const SIPv4Addr& addr)
//{
//	m_result = false;
//
//	if (!m_pStreamSocket)
//		return;
//
//#if defined(WIN32) || defined(WIN64)
//	sockaddr_in sain;
//	ZeroMemory(&sain, sizeof(sain));
//	sain.sin_family = AF_INET;
//	sain.sin_addr.s_addr = htonl(addr.addr);
//	sain.sin_port = htons(addr.port);
//	if ( SOCKET_ERROR != bind( m_pStreamSocket->m_socket, (sockaddr*)&sain, sizeof(sain) ) )
//		if ( SOCKET_ERROR != listen(m_pStreamSocket->m_socket, SOMAXCONN) )
//		{
//			m_pStreamSocket->SetSocketState( CTCPStreamSocket::eSS_Listening );
//			m_result = true;
//		}
//#elif defined(LINUX)
//	// TODO:
//#endif
//}
//
//void SConnectVisitor::Visit(const SIPv4Addr& addr)
//{
//	m_result = false;
//
//	if (!m_pStreamSocket)
//		return;
//
//#if defined(WIN32) || defined(WIN64)
//	u_long nonblocking = 1;
//	ioctlsocket(m_pStreamSocket->m_socket, FIONBIO, &nonblocking);
//
//	sockaddr_in sain;
//	ZeroMemory( &sain, sizeof(sain) );
//	sain.sin_family = AF_INET;
//	sain.sin_addr.s_addr = htonl(addr.addr);
//	sain.sin_port = htons(addr.port);
//	int ir = connect( m_pStreamSocket->m_socket, (sockaddr*)&sain, sizeof(sain) );
//	if ( SOCKET_ERROR != ir || WSAEWOULDBLOCK == WSAGetLastError() )
//	{
//		m_pStreamSocket->SetSocketState(CTCPStreamSocket::eSS_Established);
//		if (m_pStreamSocket->m_pListener)
//			m_pStreamSocket->m_pListener->OnConnectCompleted(true);
//		m_result = true;
//	}
//#elif defined(LINUX)
//	// TODO:
//#endif
//}

CTCPStreamSocket::CTCPStreamSocket()
{
#if defined(WIN32) || defined(WIN64)
	m_socket = INVALID_SOCKET;
#elif defined(LINUX)
	// TODO:
#endif

	m_socketState = eSS_Closed;
	m_pListener = NULL;
	m_pSockIO = &CNetwork::Get()->GetSocketIOManager();
}

CTCPStreamSocket::~CTCPStreamSocket()
{

}

bool CTCPStreamSocket::Init()
{
#if defined(WIN32) || defined(WIN64) || defined(XENON)
	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == m_socket)
		return false;

	m_sockid = m_pSockIO->RegisterSocket(m_socket);
	if (!m_sockid)
		return false;

#elif defined(LINUX)
	// TODO:
#endif

	return true;
}

void CTCPStreamSocket::SetListener(IStreamListener* pListener)
{
	m_pListener = pListener;
}

void CTCPStreamSocket::GetPeerAddr(TNetAddress& addr)
{
	SIPv4Addr addr4;

#if defined(WIN32) || defined(WIN64)
	sockaddr_in sain;
	ZeroMemory(&sain, sizeof(sain)); int slen = sizeof(sain);
	if ( SOCKET_ERROR != getpeername(m_socket, (sockaddr*)&sain, &slen) )
	{
		addr4.addr = ntohl(sain.sin_addr.s_addr);
		addr4.port = ntohs(sain.sin_port);
	}
#elif defined(LINUX)
	// TODO:
#endif

	addr = TNetAddress(addr4);
}

bool CTCPStreamSocket::Listen(const TNetAddress& addr)
{
	//SListenVisitor visitor(this);
	//addr.Visit(visitor);
	//return visitor.m_result;

#if defined(WIN32) || defined(WIN64)
	sockaddr sa; int sl = sizeof(sa);
	if ( !ConvertAddr(addr, &sa, &sl) )
		return false;
	if ( SOCKET_ERROR != bind(m_socket, &sa, sl) )
		if ( SOCKET_ERROR != listen(m_socket, SOMAXCONN) )
		{
			SetSocketState( eSS_Listening );
			return true;
		}
#elif defined(LINUX)
	// TODO:
#endif
	return false;
}

bool CTCPStreamSocket::Connect(const TNetAddress& addr)
{
	//SConnectVisitor visitor(this);
	//addr.Visit(visitor);
	//return visitor.m_result;
#if defined(WIN32) || defined(WIN64)
	sockaddr sa; int sl = sizeof(sa);
	if ( !ConvertAddr(TNetAddress(SIPv4Addr()), &sa, &sl) )
		return false;
	if ( SOCKET_ERROR == bind(m_socket, &sa, sl) )
		return false;
	SetSocketState( eSS_Connecting );
	return m_pSockIO->RequestConnect( m_sockid, addr );
#elif defined(LINUX)
	return false; // TODO:
#endif
	return false;
}

bool CTCPStreamSocket::Send(const uint8* pBuffer, size_t nLength)
{
	if (pBuffer == NULL || nLength == 0)
		return false;

	return m_pSockIO->RequestSend( m_sockid, pBuffer, nLength );
}

void CTCPStreamSocket::Shutdown()
{
#if defined(WIN32) || defined(WIN64)
	shutdown(m_socket, SD_SEND);
#elif defined(LINUX)
	// TODO:
#endif
}

void CTCPStreamSocket::Close()
{
#if defined(WIN32) || defined(WIN64)
	if (m_sockid)
	{
		m_pSockIO->UnregisterSocket(m_sockid);
	}
	if (INVALID_SOCKET != m_socket)
	{
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
		m_socketState = eSS_Closed;
	}
#elif defined(LINUX)
	// TODO:
#endif
}

bool CTCPStreamSocket::IsDead()
{
#if defined(WIN32) || defined(WIN64) || defined(XENON)
	return INVALID_SOCKET == m_socket;
#elif defined(LINUX)
	// TODO:
	NetWarning("CTCPStreamSocket is not implemented currently");
	return true;
#else
	return true;
#endif
}

void CTCPStreamSocket::OnRecvComplete( const uint8 * pData, uint32 nSize )
{
	m_pListener->OnIncomingData(pData, nSize);
	m_pSockIO->RequestRecv(m_sockid);
}

void CTCPStreamSocket::OnRecvException( ESocketError err )
{
	m_pListener->OnConnectionClosed(false);
}

void CTCPStreamSocket::OnSendException( ESocketError err )
{
	m_pListener->OnConnectionClosed(false);
}

void CTCPStreamSocket::OnConnectComplete()
{
	SetSocketState( eSS_Established );
	m_pListener->OnConnectCompleted(true);
}

void CTCPStreamSocket::OnConnectException( ESocketError err )
{
	SetSocketState( eSS_Closed );
	m_pListener->OnConnectCompleted(false);
}

void CTCPStreamSocket::OnAccept( const TNetAddress& from, SOCKET s )
{
#if defined(WIN32) || defined(WIN64)
	u_long nonblocking = 1;
	ioctlsocket(s, FIONBIO, &nonblocking);
#endif

	CTCPStreamSocket* pStreamSocket = new CTCPStreamSocket();
#if defined(WIN32) || defined(WIN64) || defined(XENON)
	pStreamSocket->m_socket = s;
#endif
	pStreamSocket->m_sockid = m_pSockIO->RegisterSocket( s );
	pStreamSocket->SetSocketState( eSS_Established );
	m_pListener->OnConnectionAccepted(pStreamSocket); // a listener must be specified for the newly accepted socket inside the callback
	m_pSockIO->RequestAccept(m_sockid);
}

void CTCPStreamSocket::OnAcceptException( ESocketError err )
{
	NetWarning("AcceptException:%d", err);
	m_pSockIO->RequestAccept(m_sockid);
}
