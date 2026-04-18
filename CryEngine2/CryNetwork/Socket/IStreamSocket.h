#ifndef __ISTREAMSOCKET_H__
#define __ISTREAMSOCKET_H__

#pragma once

#include "NetAddress.h"
#include "TimeValue.h"
#include "ISocketIOManager.h"

#include "SocketError.h"

struct IStreamSocket;

TYPEDEF_AUTOPTR(IStreamSocket);
typedef IStreamSocket_AutoPtr IStreamSocketPtr;

struct IStreamListener
{
	virtual void OnConnectionAccepted(IStreamSocketPtr pStreamSocket) = 0;
	virtual void OnConnectCompleted(bool succeeded) = 0;
	virtual void OnConnectionClosed(bool graceful) = 0;
	virtual void OnIncomingData(const uint8* pData, size_t nSize) = 0;
};

struct IStreamSocket : public CMultiThreadRefCount
{
	virtual void SetListener(IStreamListener* pListener) = 0;
	virtual bool Listen(const TNetAddress& addr) = 0;
	virtual bool Connect(const TNetAddress& addr) = 0;
	virtual bool Send(const uint8* pBuffer, size_t nLength) = 0;
	virtual void GetPeerAddr(TNetAddress& addr) = 0;
	virtual void Shutdown() = 0;
	virtual void Close() = 0;
	virtual bool IsDead() = 0;
};

extern IStreamSocketPtr CreateStreamSocket(const TNetAddress& addr);

#endif

