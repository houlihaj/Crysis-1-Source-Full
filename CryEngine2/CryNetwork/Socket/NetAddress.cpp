#include "StdAfx.h"
#include "NetAddress.h"
#include "Network.h"

#if defined(LINUX)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

static const TAddressString local_connection = LOCAL_CONNECTION_STRING;

// implementation of inet_aton from freebsd, for platforms that need it
#if defined(XENON)
static int
inet_aton(const char * cp, struct in_addr * addr)
/* [<][>][^][v][top][bottom][index][help] */
{
	typedef uint32 in_addr_t;

	u_long parts[4];
	in_addr_t val;
	const char *c;
	char *endptr;
	int gotend, n;

	c = (const char *)cp;
	n = 0;
	/*
	* Run through the string, grabbing numbers until
	* the end of the string, or some error
	*/
	gotend = 0;
	while (!gotend) {
		unsigned long l;

		l = strtoul(c, &endptr, 0);

		if (l == ULONG_MAX || l == 0)
			return (0);

		val = (in_addr_t)l;     
		/* 
		* If the whole string is invalid, endptr will equal
		* c.. this way we can make sure someone hasn't
		* gone '.12' or something which would get past
		* the next check.
		*/
		if (endptr == c)
			return (0);
		parts[n] = val;
		c = endptr;

		/* Check the next character past the previous number's end */
		switch (*c) {
case '.' :
	/* Make sure we only do 3 dots .. */
	if (n == 3)     /* Whoops. Quit. */
		return (0);
	n++;
	c++;
	break;

case '\0':
	gotend = 1;
	break;

default:
	if (isspace((unsigned char)*c)) {
		gotend = 1;
		break;
	} else
		return (0);     /* Invalid character, so fail */
		}

	}

	/*
	* Concoct the address according to
	* the number of parts specified.
	*/

	switch (n) {
case 0:                         /* a -- 32 bits */
	/*
	* Nothing is necessary here.  Overflow checking was
	* already done in strtoul().
	*/
	break;
case 1:                         /* a.b -- 8.24 bits */
	if (val > 0xffffff || parts[0] > 0xff)
		return (0);
	val |= parts[0] << 24;
	break;

case 2:                         /* a.b.c -- 8.8.16 bits */
	if (val > 0xffff || parts[0] > 0xff || parts[1] > 0xff)
		return (0);
	val |= (parts[0] << 24) | (parts[1] << 16);
	break;

case 3:                         /* a.b.c.d -- 8.8.8.8 bits */
	if (val > 0xff || parts[0] > 0xff || parts[1] > 0xff ||
		parts[2] > 0xff)
		return (0);
	val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
	break;
	}

	if (addr != NULL)
		addr->s_addr = htonl(val);
	return (1);
}
#endif

class CResolverToNumericStringVisitor
{
public:
	CResolverToNumericStringVisitor( TAddressString& result ) : m_result(result) {}

	bool Ok() const { return true; }

	void Visit( TLocalNetAddress addr )
	{
		static const size_t bufLen = 128;
		char buf[bufLen];
		_snprintf( buf, bufLen, "%s:%d", LOCAL_CONNECTION_STRING, addr );
		m_result = buf;
	}

	void Visit( SNullAddr )
	{
		m_result = NULL_CONNECTION_STRING;
	}

	void Visit( SIPv4Addr addr )
	{
		uint32 a = (addr.addr >> 24) & 0xff;
		uint32 b = (addr.addr >> 16) & 0xff;
		uint32 c = (addr.addr >>  8) & 0xff;
		uint32 d = (addr.addr      ) & 0xff;
		m_result.Format("%d.%d.%d.%d:%d", a,b,c,d, addr.port);
	}

private:
	TAddressString& m_result;
};

class CResolverToNumericStringVisitor_LocalBuffer
{
public:
	CResolverToNumericStringVisitor_LocalBuffer( char * result ) : m_result(result) {}

	bool Ok() const { return true; }

	void Visit( TLocalNetAddress addr )
	{
		strcpy( m_result, "localhost" );
	}

	void Visit( SNullAddr )
	{
		m_result[0] = 0;
	}

	void Visit( SIPv4Addr addr )
	{
		FUNCTION_PROFILER(gEnv->pSystem, PROFILE_NETWORK);
		uint32 a = (addr.addr >> 24) & 0xff;
		uint32 b = (addr.addr >> 16) & 0xff;
		uint32 c = (addr.addr >>  8) & 0xff;
		uint32 d = (addr.addr      ) & 0xff;
		// TODO: maybe make this faster (punkbuster will call it often)
		sprintf(m_result, "%d.%d.%d.%d:%d", a,b,c,d, addr.port);
	}

private:
	char * m_result;
};

#if defined(PS3) || defined(XENON)
	//TODO: IMPLEMENT M CORRECTLY
	#define CResolverToStringVisitor CResolverToNumericStringVisitor
#else
class CResolverToStringVisitor
{
public:
	CResolverToStringVisitor( TAddressString& result ) : m_result(result), m_ok(false) {}

	bool Ok() const { return m_ok; }

	void Visit( TLocalNetAddress addr )
	{
		static const size_t bufLen = 128;
		char buf[bufLen];
		_snprintf( buf, bufLen, "%s:%d", LOCAL_CONNECTION_STRING, addr );
		m_result = buf;
		m_ok = true;
	}

	void Visit( SNullAddr )
	{
		m_result = NULL_CONNECTION_STRING;
		m_ok = true;
	}

	void Visit( SIPv4Addr addr )
	{
		sockaddr_in saddr;
		memset( &saddr, 0, sizeof(saddr) );
		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(addr.port);
		S_ADDR_IP4(saddr) = htonl(addr.addr);
		m_ok = VisitIP( (sockaddr*)&saddr, sizeof(saddr), false );
	}

private:
	TAddressString& m_result;
	bool m_ok;

	bool VisitIP( const sockaddr * pAddr, size_t len, bool bracket )
	{
		char host[NI_MAXHOST];
		char serv[NI_MAXSERV];
		static const size_t addrLen = NI_MAXHOST+3+NI_MAXSERV+1;
		char addr[addrLen];
		int rv = getnameinfo( pAddr, len, host, NI_MAXHOST, serv, NI_MAXSERV, NI_DGRAM|NI_NUMERICSERV );
		if (0 != rv)
		{
#ifdef WIN32
			int error = WSAGetLastError();
#else
			int error = 0;
			switch (rv)
			{
			case EAI_AGAIN:
				error = WSATRY_AGAIN;
				break;
			case EAI_NONAME:
				error = WSAHOST_NOT_FOUND;
				break;
			default:
				error = WSANO_RECOVERY;
				break;
			}
#endif
			const char * msg = ((CNetwork*)(GetISystem()->GetINetwork()))->EnumerateError( MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, error) );
			NetWarning( "[net] getnameinfo fails: %s", msg );
			m_result = "0:0";
			return false;
		}
		else
		{
			_snprintf( addr, addrLen, bracket? "[%s]:%s" : "%s:%s", host, serv );
			m_result = addr;
			return true;
		}
	}
};
#endif //PS3 || XENON

TAddressString CNetAddressResolver::ToString( const TNetAddress& addr, float timeout )
{
	m_lock.Lock();
	TAddressToNameCache::iterator iter = m_addressToNameCache.find(addr);
	if (iter != m_addressToNameCache.end())
	{
		// force reallocation!
		TAddressString s = iter->second.c_str();
		m_lock.Unlock();
		return s;
	}
	_smart_ptr<CToStringRequest> pReq = new CToStringRequest(addr, m_lock, m_condOut);
	m_requests.push(&*pReq);
	m_cond.Notify();
	m_lock.Unlock();
	if (pReq->TimedWait(timeout))
	{
		TAddressString temp;
		if (pReq->GetResult(temp) == eNRR_Succeeded)
			return temp.c_str(); // force reallocation!
	}
	TAddressString fallback;
	CResolverToNumericStringVisitor visitor(fallback);
	addr.Visit(visitor);
	return fallback;
}

TAddressString CNetAddressResolver::ToNumericString( const TNetAddress& addr )
{
	TAddressString output;
	CResolverToNumericStringVisitor visitor( output );
	addr.Visit( visitor );
	return output;
}

bool CNetAddressResolver::SlowLookupName( TAddressString str, TNetAddressVec& addrs )
{
	str.Trim();

	if (str == NULL_CONNECTION_STRING)
	{
		addrs.push_back(TNetAddress(SNullAddr()));
	}

	addrs.resize(0);

	if (str.empty())
		return false;
	string::size_type leftBracketPos = str.find('[');
	string::size_type rightBracketPos = str.find(']');
	switch (leftBracketPos)
	{
	case 0:
		// ipv6 style address string... we can't handle this yet
		// TODO: ipv6 support
	default:
		NetWarning( "Invalid address: %s", str.c_str() );
		return false;
	case string::npos:
		if (rightBracketPos != string::npos)
		{
			NetWarning( "Invalid address (contains ']'): %s", str.c_str() );
			return false;
		}
		break;
	}

	TAddressString::size_type lastColonPos = str.find(':');
	TAddressString port;
	if (lastColonPos != string::npos && (lastColonPos > rightBracketPos || rightBracketPos == string::npos))
	{
		port = str.substr( lastColonPos+1 );
		str = str.substr( 0, lastColonPos );
	}
	// TODO: ipv6 stripping of []

	if (str == local_connection)
	{
		char * end;
		TLocalNetAddress portId = 0;
		if (!port.empty())
		{
			long portNum = strtol( port.c_str(), &end, 0 );
			if (portNum < 0 || portNum >= (1u << (8*sizeof(TLocalNetAddress))) || *end != 0)
			{
				NetWarning( "Invalid local address port %s", port.c_str() );
				return false;
			}
			portId = (TLocalNetAddress)portNum;
		}
		addrs.push_back( TNetAddress(portId) );
	}
	else
	{
#if defined(LINUX)
		char * end;
		long portNum = strtol( port.c_str(), &end, 0 );
		if (portNum < 0 || portNum > 65535 || *end != 0)
		{
			NetWarning( "Invalid ipv4 address port %s", port.c_str() );
			return false;
		}

		in_addr inaddr;
		memset(&inaddr, 0, sizeof(inaddr));
		if (str == "0.0.0.0")
		{
			SIPv4Addr addr; // Defaults to 0, 0
			addr.addr = 0;
			addr.port = portNum;
			addrs.push_back( TNetAddress(addr) );
		}
		else if (inet_aton( str.c_str(), &inaddr )) // try dotted decimal
		{
			SIPv4Addr addr;
			addr.addr = ntohl( inaddr.s_addr );
			addr.port = portNum;
			addrs.push_back( TNetAddress(addr) );
		}
		else
		{
			NetWarning("DNS lookup not implemented for Linux (yet!) [got '%s']", str.c_str());
		}
#elif defined(XENON)
		char * end;
		long portNum = strtol( port.c_str(), &end, 0 );
		if (portNum < 0 || portNum > 65535 || *end != 0)
		{
			NetWarning( "Invalid ipv4 address port %s", port.c_str() );
			return false;
		}

		in_addr inaddr;
		memset(&inaddr, 0, sizeof(inaddr));
		if (str == "0.0.0.0") // very special case
		{
			SIPv4Addr addr; // Defaults to 0, 0
			addr.addr = 0;
			addr.port = portNum;
			addrs.push_back( TNetAddress(addr) );
		}
		else if (inet_aton( str.c_str(), &inaddr )) // try dotted decimal
		{
			SIPv4Addr addr;
			addr.addr = ntohl( inaddr.S_un.S_addr );
			addr.port = portNum;
			addrs.push_back( TNetAddress(addr) );
		}
		else // not dotted decimal, try dns lookup
		{
			WSAEVENT wsaEvent = WSACreateEvent();
			XNDNS *pXndns = NULL;
			int err = XNetDnsLookup( str.c_str(), wsaEvent, &pXndns );
			WaitForSingleObject( wsaEvent, INFINITE );
			if ( pXndns->iStatus == 0 )
			{
				for ( int i = 0; i < pXndns->cina; i++ )
				{
					SIPv4Addr addr; // Defaults to 0, 0
					addr.addr = ntohl( pXndns->aina[i].S_un.S_addr );
					addr.port = portNum;
					addrs.push_back( TNetAddress(addr) );
				}
			}
			else
			{
				NetWarning( "XNetDnsLookup() failed, address: %s, port: %s", str.c_str(), !port.empty() ? port.c_str() : "<default>" );
				WSACloseEvent(wsaEvent);
				XNetDnsRelease( pXndns );
				return false;
			}
			WSACloseEvent(wsaEvent);
			XNetDnsRelease( pXndns );
		}
#elif defined(WIN32)
		struct addrinfo * result = NULL;
		struct addrinfo hints;
		memset( &hints, 0, sizeof(hints) );
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		int err = getaddrinfo( str.c_str(), port.empty()? NULL : port.c_str(), &hints, &result );
		if (!err)
		{
			for (struct addrinfo * p = result; p; p = p->ai_next)
			{
				switch (p->ai_addr->sa_family)
				{
				case AF_INET:
					{
						sockaddr_in * pAddr = (sockaddr_in *) p->ai_addr;
						SIPv4Addr addr;
						addr.addr = ntohl( S_ADDR_IP4(*pAddr) );
						addr.port = ntohs( pAddr->sin_port );
						addrs.push_back( TNetAddress(addr) );
					}
					break;

				default:
					NetWarning( "Unhandled address family in name lookup: %d", p->ai_addr->sa_family );
				}
			}
		}
		else
		{
			NetWarning( "%s", gai_strerror(err) );
			return false;
		}

		if (result)
			freeaddrinfo( result );
#elif defined(PS3)
		return false;
#endif
	}

	return true;
}

namespace
{
	class CIsPrivateAddrVisitor
	{
	public:
		CIsPrivateAddrVisitor() : m_private(false) {}

		bool IsPrivateAddr() { return m_private; }

		void Visit(TLocalNetAddress addr) { m_private = true; }

		void Visit(SNullAddr) {}

		void Visit(SIPv4Addr addr)
		{
			uint32 ip = addr.addr; // in host byte order, should NOT be in net byte order, since the masks below are all in host byte order
			m_private =
				(ip & 0xff000000U) == 0x7f000000U || // 127.0.0.0 ~ 127.255.255.255
				(ip & 0xff000000U) == 0x0a000000U || // 10.0.0.0 ~ 10.255.255.255
				(ip & 0xfff00000U) == 0xac100000U || // 172.16.0.0 ~ 172.31.255.255
				(ip & 0xffff0000U) == 0xc0a80000U; // 192.168.0.0 ~ 192.168.255.255
		}

	private:
		bool m_private;
	};

}

bool CNetAddressResolver::IsPrivateAddr(const TNetAddress& addr)
{
	CIsPrivateAddrVisitor visitor;
	addr.Visit(visitor);
	return visitor.IsPrivateAddr();
}

bool CNetAddressResolver::PopulateCacheFor( const TAddressString& str, TNetAddressVec& addrs )
{
	m_lock.Unlock();
	TNetAddressVec addrsTemp;
	bool ok = SlowLookupName(str, addrsTemp) && !addrsTemp.empty();
	m_lock.Lock();

	if (ok)
	{
		addrs = addrsTemp;
		for (TNetAddressVec::iterator iter = addrs.begin(); iter != addrs.end(); ++iter)
			m_nameToAddressCache.insert(std::make_pair(str, *iter));
	}

	return ok;
}

bool CNetAddressResolver::PopulateCacheFor( const TNetAddress& addr, TAddressString& str )
{
	bool cache = true;
	m_lock.Unlock();
	CResolverToStringVisitor visitor( str );
	addr.Visit( visitor );
	if (!visitor.Ok())
	{
		CResolverToNumericStringVisitor visitor2( str );
		addr.Visit(visitor2);
		cache = false;
	}
	m_lock.Lock();

	if (cache)
	{
		m_addressToNameCache.insert(std::make_pair(addr, str));
	}

	return true;
}

bool ConvertAddr( const TNetAddress& in, sockaddr_in * pOut )
{
	NET_ASSERT(pOut);

	const SIPv4Addr * pAddr = in.GetPtr<SIPv4Addr>();
	if (!pAddr)
		return false;

	memset( pOut, 0, sizeof(*pOut) );
	pOut->sin_family = AF_INET;
	pOut->sin_port = htons(pAddr->port);
	S_ADDR_IP4(*pOut) = htonl(pAddr->addr);
	return true;
}

TNetAddress ConvertAddr( const sockaddr * pSockAddr, int addrLength )
{
	switch (addrLength)
	{
	case sizeof(sockaddr_in):
		if (pSockAddr->sa_family == AF_INET)
		{
			sockaddr_in * pSA = (sockaddr_in *)pSockAddr;
			SIPv4Addr addr;
			addr.addr = ntohl(S_ADDR_IP4(*pSA));
			addr.port = ntohs(pSA->sin_port);
			return TNetAddress(addr);
		}
	}

	return TNetAddress(SNullAddr());
}

class CConvertAddrVisitor
{
public:
	CConvertAddrVisitor( sockaddr * pOut, int * pOutLen ) : m_pOut(pOut), m_pOutLen(pOutLen), m_result(false) {}

	bool Ok() const { return m_result; }

	void Visit( TLocalNetAddress addr )
	{
	}

	void Visit( SNullAddr )
	{
	}

	void Visit( SIPv4Addr addr )
	{
		if (*m_pOutLen < sizeof(sockaddr_in))
			return;
		sockaddr_in * pOut = (sockaddr_in*)m_pOut;
		memset(pOut, 0, sizeof(*pOut));
		pOut->sin_family = AF_INET;
		S_ADDR_IP4(*pOut) = htonl(addr.addr);
		pOut->sin_port = htons(addr.port);
		*m_pOutLen = sizeof(sockaddr_in);
		m_result = true;
	}

private:
	sockaddr * m_pOut;
	int * m_pOutLen;
	bool m_result;
};

bool ConvertAddr( const TNetAddress& addrIn, sockaddr * pSockAddr, int * addrLen )
{
	CConvertAddrVisitor v(pSockAddr, addrLen);
	addrIn.Visit(v);
	return v.Ok();
}

CNetAddressResolver::CNetAddressResolver() : m_die(false)
{
	Start();
}

CNetAddressResolver::~CNetAddressResolver()
{
	Cancel();
	Join();
}

void CNetAddressResolver::Run()
{
	CryThreadSetName( -1, "NetAddressResolver" );

	while (true)
	{
		m_lock.Lock();
		while (!m_die && m_requests.empty())
			m_cond.Wait(m_lock);
		if (m_die)
			break;
		TRequestPtr pReq = m_requests.front();
		m_requests.pop();
		m_lock.Unlock();

		pReq->Execute(this);
	}

	while (!m_requests.empty())
	{
		m_requests.front()->Fail();
		m_requests.pop();
	}
	m_lock.Unlock();
}

void CNetAddressResolver::Cancel()
{
	m_lock.Lock();
	m_die = true;
	m_cond.Notify();
	m_lock.Unlock();
}

CNameRequestPtr CNetAddressResolver::RequestNameLookup( const string& name )
{
	CNameRequestPtr pNR = new CNameRequest(name, m_lock, m_condOut);
	m_lock.Lock();
	m_requests.push(&*pNR);
	m_cond.Notify();
	m_lock.Unlock();
	return pNR;
}

void CNameRequest::Execute( CNetAddressResolver * pR )
{
	CryAutoLock<TLock> lkP(m_parentLock);
	NET_ASSERT(m_state == eNRR_Pending);

	TNameToAddressCache::iterator iter = pR->m_nameToAddressCache.lower_bound(m_str);
	if (iter != pR->m_nameToAddressCache.end() && iter->first == m_str)
	{
		do 
		{
			m_addrs.push_back( iter->second );
			++iter;
		}
		while (iter != pR->m_nameToAddressCache.end() && iter->first == m_str);
		m_state = eNRR_Succeeded;
		m_parentCond.Notify();
		return;
	}
	
	if (pR->PopulateCacheFor( m_str, m_addrs ))
		m_state = eNRR_Succeeded;
	else
		m_state = eNRR_Failed;

	m_parentCond.Notify();
}

CNameRequest::CNameRequest( const string& name, TNameRequestLock& parentLock, CryCond<TNameRequestLock>& parentCond ) : 
	m_str(name.c_str() /* force realloc */), 
	m_state(eNRR_Pending), 
	m_parentLock(parentLock),
	m_parentCond(parentCond)
{
}

CNameRequest::~CNameRequest()
{
	NET_ASSERT(m_state != eNRR_Pending);
}

void CNameRequest::Fail()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	NET_ASSERT(m_state == eNRR_Pending);
	m_state = eNRR_Failed;
	m_parentCond.Notify();
}

void CNameRequest::Wait()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	while (m_state == eNRR_Pending)
		m_parentCond.Wait(m_parentLock);
}

bool CNameRequest::TimedWait( float seconds )
{
	CryAutoLock<TLock> lkP(m_parentLock);
	bool done = true;
	if (m_state == eNRR_Pending)
		done = m_parentCond.TimedWait(m_parentLock, uint32(1000 * seconds));
	return done;
}

ENameRequestResult CNameRequest::GetResult()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	return m_state;
}

ENameRequestResult CNameRequest::GetResult( TNetAddressVec& v )
{
	CryAutoLock<TLock> lkP(m_parentLock);
	if (m_state == eNRR_Succeeded)
		v = m_addrs;
	return m_state;
}

string CNameRequest::GetAddrString()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	return m_str.c_str(); // force realloc
}

void CToStringRequest::Execute( CNetAddressResolver * pR )
{
	CryAutoLock<TLock> lkP(m_parentLock);
	NET_ASSERT(m_state == eNRR_Pending);

	TAddressToNameCache::iterator iter = pR->m_addressToNameCache.lower_bound(m_addr);
	if (iter != pR->m_addressToNameCache.end() && iter->first == m_addr)
	{
		m_str = iter->second;
		m_state = eNRR_Succeeded;
		m_parentCond.Notify();
		return;
	}

	if (pR->PopulateCacheFor( m_addr, m_str ))
		m_state = eNRR_Succeeded;
	else
		m_state = eNRR_Failed;

	m_parentCond.Notify();
}

CToStringRequest::CToStringRequest( const TNetAddress& name, TNameRequestLock& parentLock, CryCond<TNameRequestLock>& parentCond ) : 
	m_addr(name), 
	m_state(eNRR_Pending), 
	m_parentLock(parentLock),
	m_parentCond(parentCond)
{
}

CToStringRequest::~CToStringRequest()
{
	NET_ASSERT(m_state != eNRR_Pending);
}

void CToStringRequest::Fail()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	NET_ASSERT(m_state == eNRR_Pending);
	m_state = eNRR_Failed;
	m_parentCond.Notify();
}

void CToStringRequest::Wait()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	while (m_state == eNRR_Pending)
		m_parentCond.Wait(m_parentLock);
}

bool CToStringRequest::TimedWait( float seconds )
{
	CryAutoLock<TLock> lkP(m_parentLock);
	bool done = true;
	if (m_state == eNRR_Pending)
		done = m_parentCond.TimedWait(m_parentLock, uint32(1000 * seconds));
	return done;
}

ENameRequestResult CToStringRequest::GetResult()
{
	CryAutoLock<TLock> lkP(m_parentLock);
	return m_state;
}

ENameRequestResult CToStringRequest::GetResult( TAddressString& s )
{
	CryAutoLock<TLock> lkP(m_parentLock);
	if (m_state == eNRR_Succeeded)
		s = m_str;
	return m_state;
}
