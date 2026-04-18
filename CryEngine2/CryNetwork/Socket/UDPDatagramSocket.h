#ifndef __UDPDATAGRAMSOCKET_H__
#define __UDPDATAGRAMSOCKET_H__

#pragma once

#include "IDatagramSocket.h"

bool MakeSocketNonBlocking( SOCKET sock );

class CUDPDatagramSocket : public IDatagramSocket, private IRecvFromTarget, ISendToTarget
{
public:
	CUDPDatagramSocket();
	~CUDPDatagramSocket();

	bool Init( SIPv4Addr addr, uint32 flags );

	// IDatagramSocket
	virtual void GetSocketAddresses( TNetAddressVec& addrs );
	virtual ESocketError Send( const uint8 * pBuffer, size_t nLength, const TNetAddress& to );
	virtual void SetListener( IDatagramListener * pListener );
	virtual void Die();
	virtual bool IsDead();
	virtual SOCKET GetSysSocket();
	virtual void RegisterBackoffAddress( TNetAddress addr );
	virtual void UnregisterBackoffAddress( TNetAddress addr );
	// ~IDatagramSocket

private:
	SOCKET m_socket;
	SSocketID m_sockid;
	bool m_bIsIP4;
	TNetAddress m_lastError;
	IDatagramListener * m_pListener;
	ISocketIOManager * m_pSockIO;

	bool Init( int af, uint32 flags, void * pSockAddr, size_t sizeSockAddr );
	bool InitWinError();
	void LogWinError( int err );
	void LogWinError();

	void CloseSocket();

	// IRecvFromTarget
	virtual void OnRecvFromComplete( const TNetAddress& from, const uint8 * pData, uint32 len );
	virtual void OnRecvFromException( const TNetAddress& from, ESocketError err );
	// ~IRecvFromTarget

	// ISendToTarget
	virtual void OnSendToException( const TNetAddress& from, ESocketError err );
	// ~ISendToTarget
};

#endif
