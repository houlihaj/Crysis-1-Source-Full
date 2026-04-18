#include "StdAfx.h"
#include "SocketIOManagerSelect.h"
#include "Network.h"
#include "UDPDatagramSocket.h"

#if defined(HAS_SOCKETIOMANAGER_SELECT)

CSocketIOManagerSelect::CSocketIOManagerSelect() : ISocketIOManager(eSIOMC_SupportsBackoff)
{
	m_wakeupSocket = INVALID_SOCKET;
	m_wakeupSender = INVALID_SOCKET;
}

CSocketIOManagerSelect::~CSocketIOManagerSelect()
{
	if (m_wakeupSocket != INVALID_SOCKET)
		closesocket(m_wakeupSocket);
	if (m_wakeupSender != INVALID_SOCKET)
		closesocket(m_wakeupSender);
}

bool CSocketIOManagerSelect::Init()
{
	class CAutoCloseSocket
	{
	public:
		CAutoCloseSocket( SOCKET sock ) : m_sock(sock) {}

		~CAutoCloseSocket()
		{
			if (m_sock != INVALID_SOCKET)
				closesocket(m_sock);
		}

		void Release()
		{
			m_sock = INVALID_SOCKET;
		}

	private:
		SOCKET m_sock;
	};

	int i;
	for (i=1025; i<65536; i++)
	{
		if (i == 0xed17 || i == 0xfa57)
			continue;
		m_wakeupSocket = socket(PF_INET, SOCK_DGRAM, 0);
		if (m_wakeupSocket == INVALID_SOCKET)
			return false;
		CAutoCloseSocket closer(m_wakeupSocket);
		memset(&m_wakeupAddr, 0, sizeof(m_wakeupAddr));
		m_wakeupAddr.sin_family = AF_INET;
		m_wakeupAddr.sin_port = htons(i);
		S_ADDR_IP4(m_wakeupAddr) = htonl(0x7f000001);
		if (!bind(m_wakeupSocket, (sockaddr*)&m_wakeupAddr, sizeof(m_wakeupAddr)))
		{
			closer.Release();
			break;
		}
	}
	if (i == 65536)
		return false;

	m_wakeupSender = socket(PF_INET, SOCK_DGRAM, 0);
	if (m_wakeupSender == INVALID_SOCKET)
		return false;
	sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = 0;
	S_ADDR_IP4(saddr) = htonl(0x7f000001);
	if (bind(m_wakeupSender, (sockaddr*)&saddr, sizeof(saddr)))
		return false;

	if (!MakeSocketNonBlocking(m_wakeupSender))
		return false;
	if (!MakeSocketNonBlocking(m_wakeupSocket))
		return false;

	return true;
}

static SOCKET smax( SOCKET a, SOCKET b )
{
	if (a == INVALID_SOCKET)
		return b;
	if (b == INVALID_SOCKET)
		return a;
	if (a < b)
		return b;
	else
		return a;
}

int CSocketIOManagerSelect::Poll( float waitTime, bool& performedWork )
{
	performedWork = false;

	fd_set rd;
	fd_set wr;
	SOCKET fdmax = INVALID_SOCKET;

	m_watchdog.ClearStalls();

	{
		SCOPED_GLOBAL_LOCK;

		if (!m_pendingReturns.empty())
		{
			int r = m_pendingReturns.front();
			m_pendingReturns.pop();
			return r;
		}

		FD_ZERO(&rd);
		FD_ZERO(&wr);

		FD_SET(m_wakeupSocket, &rd);
		fdmax = smax(m_wakeupSocket, fdmax);

		for (size_t i=0; i<m_socketInfo.size(); i++)
		{
			SSocketInfo& si = m_socketInfo[i];
			if (!si.isActive)
				continue;
			if (si.NeedRead())
			{
				FD_SET(si.sock, &rd);
				fdmax = smax(si.sock, fdmax);
			}
			if (si.NeedWrite())
			{
				FD_SET(si.sock, &wr);
				fdmax = smax(si.sock, fdmax);
			}
		}
	}

	if (fdmax >= 0)
	{
		timeval tv;
		waitTime /= 1000.0f;
		tv.tv_sec = (int)waitTime;
		tv.tv_usec = (int)((waitTime - tv.tv_sec) * 1e6f);
		int r = select( fdmax+1, &rd, &wr, NULL, &tv );

		SCOPED_GLOBAL_LOCK;

		switch (r)
		{
		case 0:
			return 1;
		case SOCKET_ERROR:
			return -666;
		default:
			performedWork = true;
			if (FD_ISSET(m_wakeupSocket, &rd))
			{
				char buf[256];
				char addr[_SS_MAXSIZE];
				int addrlen = _SS_MAXSIZE;
				recvfrom(m_wakeupSocket, buf, 256, 0, (sockaddr*)addr, &addrlen);
			}
			for (size_t i=0; i<m_socketInfo.size(); i++)
			{
				SSocketInfo& si = m_socketInfo[i];
				if (!si.isActive)
					continue;
				if (si.NeedRead() && FD_ISSET(si.sock, &rd))
				{
					char buffer[MAX_UDP_PACKET_SIZE];
					if (si.nRecvFrom)
					{
						char address[_SS_MAXSIZE];
						int addrlen = _SS_MAXSIZE;
						r = recvfrom( si.sock, buffer, MAX_UDP_PACKET_SIZE, 0, (sockaddr*)address, &addrlen );
						switch (r)
						{
						case 0:
							si.pRecvFromTarget->OnRecvFromException( ConvertAddr((sockaddr*)address, addrlen), eSE_ZeroLengthPacket );
							si.nRecvFrom --;
							break;
						case SOCKET_ERROR:
							{
								uint32 err = GetLastError();
								if (err != WSAEWOULDBLOCK)
								{
									si.pRecvFromTarget->OnRecvFromException( ConvertAddr((sockaddr*)address, addrlen), OSErrorToSocketError(err) );
									si.nRecvFrom --;
								}
							}
							break;
						default:
							CNetwork::Get()->ReportGotPacket();
							si.pRecvFromTarget->OnRecvFromComplete( ConvertAddr((sockaddr*)address, addrlen), (uint8*)buffer, r );
							si.nRecvFrom --;
							break;
						}
					}
					if (si.nRecv)
					{
						r = recv( si.sock, buffer, MAX_UDP_PACKET_SIZE, 0 );
						switch (r)
						{
						case 0:
							si.pRecvTarget->OnRecvException( eSE_ZeroLengthPacket );
							si.nRecv --;
							break;
						case SOCKET_ERROR:
							{
								uint32 err = WSAGetLastError();
								if (err != WSAEWOULDBLOCK)
								{
									si.pRecvTarget->OnRecvException( OSErrorToSocketError(err) );
									si.nRecv --;
								}
							}
							break;
						default:
							CNetwork::Get()->ReportGotPacket();
							si.pRecvTarget->OnRecvComplete( (uint8*)buffer, r );
							si.nRecv --;
							break;
						}
					}
					if (si.nListen)
					{
						char address[_SS_MAXSIZE];
						int addrlen = _SS_MAXSIZE;
						SOCKET sock = accept(si.sock, (sockaddr*)address, &addrlen);
						if (sock != INVALID_SOCKET)
						{
							si.pAcceptTarget->OnAccept( ConvertAddr((sockaddr*)address, addrlen), sock );
							si.nListen--;
						}
						else
						{
							uint32 err = WSAGetLastError();
							if (err != WSAEWOULDBLOCK)
							{
								si.pAcceptTarget->OnAcceptException( OSErrorToSocketError(err) );
								si.nListen--;
							}
						}
					}
				}
				if (si.NeedWrite() && FD_ISSET(si.sock, &wr))
				{
					bool done = false;
					while (!done && !si.outgoing.empty())
					{
						r = send(si.sock, (char*)si.outgoing.front().data, si.outgoing.front().nLength, 0);
						switch (r)
						{
						case 0:
							si.pSendTarget->OnSendException(eSE_ZeroLengthPacket);
							break;
						case SOCKET_ERROR:
							{
								uint32 err = WSAGetLastError();
								if (err != WSAEWOULDBLOCK)
									si.pSendTarget->OnSendException(OSErrorToSocketError(err));
								else
									done = true;
								break;
							}
						}
						if (!done)
							si.outgoing.pop_front();
					}
					done = false;
					while (!done && !si.outgoingAddressed.empty())
					{
						char address[_SS_MAXSIZE];
						int addrlen = _SS_MAXSIZE;
						if (ConvertAddr( si.outgoingAddressed.front().addr, (sockaddr*)address, &addrlen ))
						{
							r = sendto(si.sock, (char*)si.outgoingAddressed.front().data, si.outgoingAddressed.front().nLength, 0, (sockaddr*)address, addrlen);
							switch (r)
							{
							case 0:
								si.pSendToTarget->OnSendToException(si.outgoingAddressed.front().addr, eSE_ZeroLengthPacket);
								break;
							case SOCKET_ERROR:
								{
									uint32 err = WSAGetLastError();
									if (err != WSAEWOULDBLOCK)
										si.pSendToTarget->OnSendToException(si.outgoingAddressed.front().addr, OSErrorToSocketError(err));
									else
										done = true;
									break;
								}
							}
							if (!done)
								si.outgoingAddressed.pop_front();
						}
					}
				}
			}
			return 1;
		}
	}
	else
	{
		return 1;
	}
}

void CSocketIOManagerSelect::PushUserMessage( int msg )
{
	if (msg <= -666 || msg >= 0)
		CryError("Invalid SocketIO message");
	m_pendingReturns.push(msg);
	WakeUp();
}

void CSocketIOManagerSelect::WakeUp()
{
	char buf[1] = {0};
	sendto(m_wakeupSender, buf, 0, 0, (sockaddr*)&m_wakeupAddr, sizeof(m_wakeupAddr));
}

SSocketID CSocketIOManagerSelect::RegisterSocket( SOCKET sock )
{
	int id;
	for (id=0; id<m_socketInfo.size(); id++)
		if (!m_socketInfo[id].isActive)
			break;
	if (id == m_socketInfo.size())
		m_socketInfo.push_back(SSocketInfo());
	m_socketInfo[id].isActive = true;
	do m_socketInfo[id].salt ++; while (!m_socketInfo[id].salt);
	m_socketInfo[id].sock = sock;
	return SSocketID(id, m_socketInfo[id].salt);
}

void CSocketIOManagerSelect::UnregisterSocket( SSocketID sockid )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		uint16 salt = pSI->salt;
		*pSI = SSocketInfo();
		pSI->salt = salt;
		do pSI->salt++; while (!pSI->salt);
	}
}

void CSocketIOManagerSelect::RegisterBackoffAddressForSocket( TNetAddress addr, SSocketID sockid )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
		m_watchdog.RegisterTarget( pSI->sock, addr );
}

void CSocketIOManagerSelect::UnregisterBackoffAddressForSocket( TNetAddress addr, SSocketID sockid )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
		m_watchdog.UnregisterTarget( pSI->sock, addr );
}

CSocketIOManagerSelect::SSocketInfo * CSocketIOManagerSelect::GetSocketInfo( SSocketID id )
{
	if (id.id >= m_socketInfo.size())
		return 0;
	if (m_socketInfo[id.id].salt != id.salt)
		return 0;
	if (!m_socketInfo[id.id].isActive)
		return 0;
	return &m_socketInfo[id.id];
}

void CSocketIOManagerSelect::SetRecvFromTarget( SSocketID sockid, IRecvFromTarget * pTarget )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		pSI->pRecvFromTarget = pTarget;
		pSI->nRecvFrom *= (pTarget != NULL);
	}
}

void CSocketIOManagerSelect::SetConnectTarget( SSocketID sockid, IConnectTarget * pTarget )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		pSI->pConnectTarget = pTarget;
	}
}

void CSocketIOManagerSelect::SetSendToTarget( SSocketID sockid, ISendToTarget * pTarget )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		pSI->pSendToTarget = pTarget;
		if (!pTarget)
			pSI->outgoingAddressed.clear();
	}
}

void CSocketIOManagerSelect::SetAcceptTarget( SSocketID sockid, IAcceptTarget * pTarget )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		pSI->pAcceptTarget = pTarget;
		pSI->nListen *= (pTarget != NULL);
	}
}

void CSocketIOManagerSelect::SetRecvTarget( SSocketID sockid, IRecvTarget * pTarget )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		pSI->pRecvTarget = pTarget;
		pSI->nRecv *= (pTarget != NULL);
	}
}

void CSocketIOManagerSelect::SetSendTarget( SSocketID sockid, ISendTarget * pTarget )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		pSI->pSendTarget = pTarget;
		if (!pTarget)
			pSI->outgoing.clear();
	}
}

bool CSocketIOManagerSelect::RequestRecvFrom( SSocketID sockid )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		if (pSI->pRecvFromTarget)
		{
			pSI->nRecvFrom++;
			return true;
		}
	}
	return false;
}

bool CSocketIOManagerSelect::RequestSendTo( SSocketID sockid, const TNetAddress& addr, const uint8 * pData, size_t len )
{
	if (len > MAX_UDP_PACKET_SIZE)
		return false;

	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		if (pSI->pSendToTarget)
		{
			if (!pSI->outgoingAddressed.empty())
			{
delaysend:
				pSI->outgoingAddressed.push_back(SOutgoingAddressedData());
				pSI->outgoingAddressed.back().nLength = len;
				memcpy(pSI->outgoingAddressed.back().data, pData, len);
				WakeUp();
			}
			else
			{
				char address[_SS_MAXSIZE];
				int addrlen = _SS_MAXSIZE;
				if (ConvertAddr( addr, (sockaddr*)address, &addrlen ))
				{
					int r = sendto(pSI->sock, (char*)pData, len, 0, (sockaddr*)address, addrlen);
					switch (r)
					{
					case 0:
						pSI->pSendToTarget->OnSendToException(addr, eSE_ZeroLengthPacket);
						break;
					case SOCKET_ERROR:
						{
							uint32 err = WSAGetLastError();
							if (err != WSAEWOULDBLOCK)
								pSI->pSendToTarget->OnSendToException(addr, OSErrorToSocketError(err));
							else
								goto delaysend;
							break;
						}
					}
				}
			}
			return true;
		}
	}
	return false;
}

bool CSocketIOManagerSelect::RequestConnect( SSocketID sockid, const TNetAddress& addr )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		if (pSI->pConnectTarget)
		{
			char address[_SS_MAXSIZE];
			int addrlen = _SS_MAXSIZE;
			if (ConvertAddr(addr, (sockaddr*)address, &addrlen))
			{
				if (connect(pSI->sock, (sockaddr*)address, addrlen))
					pSI->pConnectTarget->OnConnectException( OSErrorToSocketError(WSAGetLastError()) );
				else
					pSI->pConnectTarget->OnConnectComplete();
			}
			return true;
		}
	}
	return false;
}

bool CSocketIOManagerSelect::RequestAccept( SSocketID sockid )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		if (pSI->pAcceptTarget)
		{
			pSI->nListen++;
			return true;
		}
	}
	return false;
}

bool CSocketIOManagerSelect::RequestSend( SSocketID sockid, const uint8 * pData, size_t len )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		if (pSI->pSendTarget)
		{
			while (len)
			{
				pSI->outgoing.push_back(SOutgoingData());
				size_t ncp = std::min(len, size_t(MAX_UDP_PACKET_SIZE));
				pSI->outgoing.back().nLength = ncp;
				memcpy(pSI->outgoing.back().data, pData, ncp);
				pData += ncp;
				len -= ncp;
			}
			return true;
		}
	}
	return false;
}

bool CSocketIOManagerSelect::RequestRecv( SSocketID sockid )
{
	if (SSocketInfo * pSI = GetSocketInfo(sockid))
	{
		if (pSI->pRecvTarget)
		{
			pSI->nRecv++;
			return true;
		}
	}
	return false;
}

#endif
