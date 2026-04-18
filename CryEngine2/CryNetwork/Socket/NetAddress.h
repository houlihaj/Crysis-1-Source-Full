#ifndef __NETADDRESS_H__
#define __NETADDRESS_H__

#pragma once

#include "ConfigurableVariant.h"
#include "CryThread.h"
#include <queue>

#if defined(XENON) || defined(__GNUC__)
#define _SS_MAXSIZE 256
#endif

typedef uint16 TLocalNetAddress;
class CNetAddressResolver;

typedef CryFixedStringT<128> TAddressString;

struct SIPv4Addr
{
	ILINE SIPv4Addr() : addr(0), port(0) {}
	ILINE SIPv4Addr( uint32 addr, uint16 port ) { this->addr = addr; this->port = port; }

	uint32 addr;
	uint16 port;

	ILINE bool operator<( const SIPv4Addr& rhs ) const
	{
		return addr < rhs.addr || (addr == rhs.addr && port < rhs.port);
	}
};

struct SNullAddr 
{
	ILINE bool operator<( const SNullAddr& rhs ) const
	{
		return false;
	}
};

typedef NTypelist::CConstruct<
	SNullAddr, TLocalNetAddress, SIPv4Addr
>::TType TNetAddressTypes;

typedef 
	CConfigurableVariant<TNetAddressTypes, NTypelist::MaximumSize<TNetAddressTypes>::value> TNetAddress;

inline bool operator==( const TNetAddress& a, const TNetAddress& b )
{
	std::less<TNetAddress> c;
	return !c(a,b) && !c(b,a);
}

typedef std::vector<TNetAddress> TNetAddressVec;

struct IResolverRequest : public CMultiThreadRefCount
{
protected:
	IResolverRequest()
	{
		++g_objcnt.nameResolutionRequest;
	}
	~IResolverRequest()
	{
		--g_objcnt.nameResolutionRequest;
	}


private:
	friend class CNetAddressResolver;

	virtual void Execute( CNetAddressResolver * pR ) = 0;
	virtual void Fail() = 0;
};

enum ENameRequestResult
{
	eNRR_Pending,
	eNRR_Succeeded,
	eNRR_Failed
};

typedef CryCondLock<CRYLOCK_RECURSIVE> TNameRequestLock;

class CNameRequest : public IResolverRequest
{
public:
	~CNameRequest();

	ENameRequestResult GetResult( TNetAddressVec& res );
	ENameRequestResult GetResult();
	string GetAddrString();
	void Wait();
	bool TimedWait( float seconds );
	
private:
	friend class CNetAddressResolver;

	CNameRequest( const string& name, TNameRequestLock& parentLock, CryCond<TNameRequestLock>& parentCond );

	void Execute( CNetAddressResolver * pR );
	void Fail();

	typedef TNameRequestLock TLock;
	TLock& m_parentLock;
	CryCond<TLock>& m_parentCond;
	TNetAddressVec m_addrs;
	TAddressString m_str;
	volatile ENameRequestResult m_state;
};

class CToStringRequest : public IResolverRequest
{
public:
	~CToStringRequest();

	ENameRequestResult GetResult( TAddressString& res );
	ENameRequestResult GetResult();
	void Wait();
	bool TimedWait( float seconds );

private:
	friend class CNetAddressResolver;

	CToStringRequest( const TNetAddress& addr, TNameRequestLock& parentLock, CryCond<TNameRequestLock>& parentCond );

	void Execute( CNetAddressResolver * pR );
	void Fail();

	typedef TNameRequestLock TLock;
	TLock& m_parentLock;
	CryCond<TLock>& m_parentCond;
	TNetAddress m_addr;
	TAddressString m_str;
	volatile ENameRequestResult m_state;
};

typedef _smart_ptr<CNameRequest> CNameRequestPtr;

typedef std::multimap<TAddressString, TNetAddress> TNameToAddressCache;
typedef std::map<TNetAddress, TAddressString> TAddressToNameCache;

class CNetAddressResolver : public CrySimpleThread<>
{
public:
	CNetAddressResolver();
	~CNetAddressResolver();
	void Run();
	void Cancel();

	TAddressString ToString( const TNetAddress& addr, float timeout = 0.01f );
	TAddressString ToNumericString( const TNetAddress& addr );
	CNameRequestPtr RequestNameLookup( const string& name );

	bool IsPrivateAddr(const TNetAddress& addr);

private:
	friend class CNameRequest;
	friend class CToStringRequest;

	TNameToAddressCache m_nameToAddressCache;
	TAddressToNameCache m_addressToNameCache;

	bool PopulateCacheFor( const TAddressString& str, TNetAddressVec& addrs );
	bool PopulateCacheFor( const TNetAddress& addr, TAddressString& str );
	bool SlowLookupName( TAddressString str, TNetAddressVec& addrs );

	typedef TNameRequestLock TLock;
	typedef _smart_ptr<IResolverRequest> TRequestPtr;
	TLock m_lock;
	CryCond<TLock> m_cond;
  CryCond<TLock> m_condOut;
	bool m_die;
	std::queue<TRequestPtr> m_requests;
};

bool ConvertAddr( const TNetAddress& addrIn, sockaddr_in * pSockAddr );
bool ConvertAddr( const TNetAddress& addrIn, sockaddr * pSockAddr, int * addrLen );
TNetAddress ConvertAddr( const sockaddr * pSockAddr, int addrLength );

#endif
