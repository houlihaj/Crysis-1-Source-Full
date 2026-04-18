#include "StdAfx.h"
#include "Config.h"
#include "IDatagramSocket.h"
#include "LocalDatagramSocket.h"
#include "UDPDatagramSocket.h"
#include "CompositeDatagramSocket.h"
#include "InternetSimulatorSocket.h"

class CNullSocket : public IDatagramSocket
{
public:
	virtual void SetListener( IDatagramListener * pListener ) {}
	virtual void GetSocketAddresses( TNetAddressVec& addrs ) { addrs.push_back(TNetAddress(SNullAddr())); }
	virtual ESocketError Send( const uint8 * pBuffer, size_t nLength, const TNetAddress& to ) { return eSE_MiscFatalError; }
	virtual void Die() {}
	virtual bool IsDead() { return true; }
	virtual void RegisterBackoffAddress( TNetAddress addr ) {}
	virtual void UnregisterBackoffAddress( TNetAddress addr ) {}
};

struct SOpenSocketVisitor
{
	IDatagramSocketPtr result;
	uint32 flags;

	template <class T>
	IDatagramSocketPtr CreateUDP( const T& addr )
	{
		CUDPDatagramSocket * pSocket = new CUDPDatagramSocket();
		if (!pSocket->Init(addr, flags))
		{
			delete pSocket;
			return NULL;
		}
		return pSocket;
	}

	IDatagramSocketPtr Create( TLocalNetAddress addr )
	{
		CLocalDatagramSocket * pSocket = new CLocalDatagramSocket();
		if (!pSocket->Init(addr))
		{
			delete pSocket;
			return NULL;
		}
		return pSocket;
	}

	IDatagramSocketPtr Create( const SIPv4Addr& addr )
	{
		IDatagramSocketPtr pLocal = Create( addr.port );
		if (!pLocal)
			return NULL;
		IDatagramSocketPtr pNormal = CreateUDP( addr );
		if (!pNormal)
			return NULL;

		CCompositeDatagramSocket * pSocket = new CCompositeDatagramSocket();
		pSocket->AddChild( pLocal );
		pSocket->AddChild( pNormal );
		return pSocket;
	}

	IDatagramSocketPtr Create( SNullAddr )
	{
		return new CNullSocket;
	}

	template <class T>
	ILINE void Visit( const T& type )
	{
		result = Create( type );
	}
};

IDatagramSocketPtr OpenSocket( const TNetAddress& addr, uint32 flags )
{
	SOpenSocketVisitor visitor;
	visitor.flags = flags;
	addr.Visit( visitor );
	IDatagramSocketPtr pSocket = visitor.result;

#if INTERNET_SIMULATOR
	if (pSocket)
		pSocket = new CInternetSimulatorSocket( pSocket );
#endif

	return pSocket;
}
