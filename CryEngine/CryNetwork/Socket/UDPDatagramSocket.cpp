#include "StdAfx.h"
#include "UDPDatagramSocket.h"
#include "NetCVars.h"

#include "Network.h"

#ifdef PS3
#include <netdb.h>
#endif

uint64 g_bytesIn = 0, g_bytesOut = 0;

bool MakeSocketNonBlocking( SOCKET sock )
{
#if defined(WIN32) || defined(XENON)
	unsigned long nTrue = 1;
	if (ioctlsocket(sock, FIONBIO, &nTrue) == SOCKET_ERROR)
		return false;
#elif defined(PS3)
	int nonblocking = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_NBIO, &nonblocking, sizeof(int)) == -1)
		return false;
#else
	int nFlags = fcntl(sock, F_GETFL);
	if (nFlags == -1)
		return false;
	nFlags |= O_NONBLOCK;
	if (fcntl(sock, F_SETFL, nFlags) == -1)
		return false;
#endif
	return true;
}

template <class T>
static bool SetSockOpt( SOCKET s, int level, int optname, const T& value )
{
	return 0 == setsockopt( s, level, optname, (const char *)&value, sizeof(T) );
}

union USockAddr
{
	sockaddr_in ip4;
};

CUDPDatagramSocket::CUDPDatagramSocket() : m_socket(INVALID_SOCKET), m_pListener(0)
{
}

CUDPDatagramSocket::~CUDPDatagramSocket()
{
	NET_ASSERT( m_socket == INVALID_SOCKET );
}

bool CUDPDatagramSocket::Init( SIPv4Addr addr, uint32 flags )
{
	ASSERT_GLOBAL_LOCK;
	m_socket = -1;
	m_bIsIP4 = true;
	m_pSockIO = &CNetwork::Get()->GetSocketIOManager();

	sockaddr_in saddr;
	memset( &saddr, 0, sizeof(saddr) );

	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(addr.port);
	S_ADDR_IP4(saddr) = htonl(addr.addr);

	if (Init( AF_INET, flags, &saddr, sizeof(saddr) ))
	{
#if 0 && defined(WIN32) && !CHECK_ENCODING
		if (!SetSockOpt(m_socket, IPPROTO_IP, IP_DONTFRAGMENT, TRUE))
			return InitWinError();
#endif // WIN32

		m_sockid = m_pSockIO->RegisterSocket( m_socket );
		if (!m_sockid)
		{
			CloseSocket();
			return false;
		}
		m_pSockIO->SetRecvFromTarget( m_sockid, this );
		m_pSockIO->SetSendToTarget( m_sockid, this );
		for (int i=0; i<640; i++)
			m_pSockIO->RequestRecvFrom(m_sockid);

		return true;
	}

	m_socket = -1;
	return false;
}

void CUDPDatagramSocket::Die()
{
	if (m_sockid)
	{
		m_pSockIO->UnregisterSocket(m_sockid);
	}
	if (m_socket != INVALID_SOCKET)
	{
		CloseSocket();
	}
}

bool CUDPDatagramSocket::IsDead()
{
	return m_socket == INVALID_SOCKET;
}

SOCKET CUDPDatagramSocket::GetSysSocket()
{
    return m_socket;
}

bool CUDPDatagramSocket::Init( int af, uint32 flags, void * pSockAddr, size_t sizeSockAddr )
{
	m_socket = socket( af, SOCK_DGRAM, IPPROTO_UDP );
	if (m_socket == INVALID_SOCKET)
		return InitWinError();

	if (!MakeSocketNonBlocking(m_socket))
		return false;

	enum EFatality
	{
		eF_Fail,
		eF_Log,
		eF_Ignore
	};

	struct 
	{
		int so_level;
		int so_opt;
		ESocketFlags flag;
		int trueVal;
		int falseVal;
		EFatality fatality;
	} 
	allflags[] = 
	{
		{ SOL_SOCKET, SO_BROADCAST, eSF_BroadcastSend, 1, 0, eF_Fail },
		{ SOL_SOCKET, SO_RCVBUF, eSF_BigBuffer, ((CNetwork::Get()->GetSocketIOManager().caps & eSIOMC_NoBuffering)==0) * 1024*1024, 4096, eF_Ignore },
		{ SOL_SOCKET, SO_SNDBUF, eSF_BigBuffer, ((CNetwork::Get()->GetSocketIOManager().caps & eSIOMC_NoBuffering)==0) * 1024*1024, 4096, eF_Ignore },
#if defined(WIN32)
		{ IPPROTO_IP, IP_RECEIVE_BROADCAST, eSF_BroadcastReceive, 1, 0, eF_Ignore },
#endif
	};

	for (size_t i=0; i<sizeof(allflags)/sizeof(*allflags); i++)
	{
		if (!SetSockOpt(m_socket, allflags[i].so_level, allflags[i].so_opt, ((flags&allflags[i].flag)==allflags[i].flag)? allflags[i].trueVal : allflags[i].falseVal))
		{
			switch (allflags[i].fatality)
			{
			case eF_Fail:
				return InitWinError();
			case eF_Log:
				LogWinError();
			case eF_Ignore:
				break;
			}
		}
	}

	if (bind(m_socket, static_cast<sockaddr*>(pSockAddr), sizeSockAddr))
		return InitWinError();

	return true;
}

void CUDPDatagramSocket::CloseSocket()
{
	if (m_socket != INVALID_SOCKET)
	{
#if defined(WIN32) || defined(XENON)
		closesocket( m_socket );
#elif defined(PS3)
		socketclose( m_socket );
#else
		close( m_socket );
#endif
		m_socket = INVALID_SOCKET;
	}
}

bool CUDPDatagramSocket::InitWinError()
{
	CloseSocket();
	LogWinError();
	return false;
}

void CUDPDatagramSocket::LogWinError()
{	
#if defined(WIN32) || defined(XENON)
	int error = WSAGetLastError();
#elif defined(PS3)
	int error = sys_net_errno;
#else
	int error = errno;
#endif
	LogWinError( error );
}

void CUDPDatagramSocket::LogWinError( int error )
{
	// ugly
	const char * msg = ((CNetwork*)(GetISystem()->GetINetwork()))->EnumerateError( MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, error) );
	NetWarning( "[net] socket error: %s", msg );
}

void CUDPDatagramSocket::GetSocketAddresses( TNetAddressVec& addrs )
{
	if (m_socket == INVALID_SOCKET)
		return;

#if defined(WIN32) || defined(WIN64) || defined(XENON)
	char addrBuf[_SS_MAXSIZE];
	int addrLen = _SS_MAXSIZE;
	if (0 == getsockname(m_socket, (sockaddr*)addrBuf, &addrLen))
	{
		TNetAddress addr = ConvertAddr((sockaddr*)addrBuf, addrLen);
		bool valid = true;
		if (addr.GetPtr<SNullAddr>())
			valid = false;
		else if (SIPv4Addr * pIPv4 = addr.GetPtr<SIPv4Addr>())
		{
			if (!pIPv4->addr)
				valid = false;
		}
		if (valid)
		{
			addrs.push_back(addr);
			return;
		}
	}
#endif // defined(XENON) || defined(WIN32) || defined(WIN64)

#if !defined(XENON)
	std::vector<string> hostnames;

	uint16 nPort;

	USockAddr sockAddr;
	socklen_t sockAddrSize = sizeof(sockAddr);

	if (0 != getsockname( m_socket, (sockaddr*)&sockAddr, &sockAddrSize ))
	{
		InitWinError();
		return;
	}
	if (sockAddrSize == sizeof(sockaddr_in))
	{
		if (!S_ADDR_IP4(sockAddr.ip4))
			hostnames.push_back("localhost");
		nPort = ntohs( sockAddr.ip4.sin_port );
	}
	else
	{
#ifdef _DEBUG
		CryError( "Unhandled sockaddr type" );
#endif
		return;
	}

#if !defined(PS3) // FIXME ?
	char hostnameBuffer[NI_MAXHOST];
	if (!gethostname(hostnameBuffer, sizeof(hostnameBuffer)))
		hostnames.push_back( hostnameBuffer );
#endif

	if (hostnames.empty())
		return;

	for (std::vector<string>::const_iterator iter = hostnames.begin(); iter != hostnames.end(); ++iter)
	{
		hostent * hp = gethostbyname( iter->c_str() );
		if (hp)
		{
			switch (hp->h_addrtype)
			{
			case AF_INET:
				{
					SIPv4Addr addr;
					NET_ASSERT( sizeof(addr.addr) == hp->h_length );
					addr.port = nPort;
					for (size_t i=0; hp->h_addr_list[i]; i++)
					{
						addr.addr = ntohl( *(uint32*)hp->h_addr_list[i] );
						addrs.push_back( TNetAddress(addr) );
					}
				}
				break;
			default:
				NetWarning("Unhandled network address type %d length %d bytes", hp->h_addrtype, hp->h_length);
			}
		}
	}
#endif // defined(XENON)
}

void CUDPDatagramSocket::RegisterBackoffAddress( TNetAddress addr )
{
	m_pSockIO->RegisterBackoffAddressForSocket( addr, m_sockid );
}

void CUDPDatagramSocket::UnregisterBackoffAddress( TNetAddress addr )
{
	m_pSockIO->UnregisterBackoffAddressForSocket( addr, m_sockid );
}

ESocketError CUDPDatagramSocket::Send( const uint8 * pBuffer, size_t nLength, const TNetAddress& to )
{
#if ENABLE_DEBUG_KIT
	CAutoCorruptAndRestore acr(pBuffer, nLength, CVARS.RandomPacketCorruption == 1);
#endif

	g_bytesOut += nLength + UDP_HEADER_SIZE;
	if (g_time > CNetCVars::Get().StallEndTime)
		m_pSockIO->RequestSendTo( m_sockid, to, pBuffer, nLength );
	return eSE_Ok;
}

void CUDPDatagramSocket::OnRecvFromComplete( const TNetAddress& from, const uint8 * pData, uint32 len )
{
	g_bytesIn += len + UDP_HEADER_SIZE;

	if (m_pListener)
	{
		m_pListener->OnPacket( from, pData, len );
	}

	m_pSockIO->RequestRecvFrom(m_sockid);
}

void CUDPDatagramSocket::OnRecvFromException( const TNetAddress& from, ESocketError err )
{
	if (err != eSE_Cancelled)
	{
		if (m_pListener)
			m_pListener->OnError( from, err );

		m_pSockIO->RequestRecvFrom(m_sockid);
	}
}

void CUDPDatagramSocket::OnSendToException( const TNetAddress& from, ESocketError err )
{
	if (err != eSE_Cancelled)
	{
		if (m_pListener)
			m_pListener->OnError( from, err );
	}
}

void CUDPDatagramSocket::SetListener( IDatagramListener * pListener )
{
	m_pListener = pListener;
}
