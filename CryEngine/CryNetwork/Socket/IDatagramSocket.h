#ifndef __IDATAGRAMSOCKET_H__
#define __IDATAGRAMSOCKET_H__

#pragma once

#include "NetAddress.h"
#include "TimeValue.h"
#include "ISocketIOManager.h"
#include "MementoMemoryManager.h"

#include "SocketError.h"

enum ESocketFlags
{
	eSF_BroadcastSend    = 0x00000001u,
	eSF_BroadcastReceive = 0x00000002u,
	eSF_BigBuffer        = 0x00000004u
};

struct IDatagramListener
{
	virtual void OnPacket( const TNetAddress& addr, const uint8 * pData, uint32 nLength ) = 0;
	virtual void OnError( const TNetAddress& addr, ESocketError error ) = 0;
};

struct IDatagramSocket : public CMultiThreadRefCount
{
	IDatagramSocket()
	{
		++g_objcnt.abstractSocket;
	}
	virtual ~IDatagramSocket()
	{
		--g_objcnt.abstractSocket;
	}

	virtual void SetListener( IDatagramListener * pListener ) = 0;
	virtual void GetSocketAddresses( TNetAddressVec& addrs ) = 0;
	virtual ESocketError Send( const uint8 * pBuffer, size_t nLength, const TNetAddress& to ) = 0;
	virtual void Die() = 0;
	virtual bool IsDead() = 0;
	virtual void RegisterBackoffAddress( TNetAddress addr ) = 0;
	virtual void UnregisterBackoffAddress( TNetAddress addr ) = 0;
  virtual SOCKET GetSysSocket() {return INVALID_SOCKET;}

	virtual void GetMemoryStatistics(ICrySizer* pSizer) { /* do nothing */ }
};

TYPEDEF_AUTOPTR(IDatagramSocket);
typedef IDatagramSocket_AutoPtr IDatagramSocketPtr;

extern IDatagramSocketPtr OpenSocket( const TNetAddress& addr, uint32 flags );

#endif
