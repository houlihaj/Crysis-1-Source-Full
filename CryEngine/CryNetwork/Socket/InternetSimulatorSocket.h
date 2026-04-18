#ifndef __INTERNET_SIMULATOR_SOCKET_H__
#define __INTERNET_SIMULATOR_SOCKET_H__

#pragma once

#include "Config.h"

#if INTERNET_SIMULATOR

#include "IDatagramSocket.h"
#include "NetTimer.h"

class CInternetSimulatorSocket : public IDatagramSocket
{
public:
	CInternetSimulatorSocket( IDatagramSocketPtr pChild );
	~CInternetSimulatorSocket();

	// IDatagramSocket
	virtual void SetListener( IDatagramListener * pListener );
	virtual void GetSocketAddresses( TNetAddressVec& addrs );
	virtual ESocketError Send( const uint8 * pBuffer, size_t nLength, const TNetAddress& to );
	virtual void Die();
	virtual bool IsDead();
	virtual SOCKET GetSysSocket();
	virtual void RegisterBackoffAddress( TNetAddress addr ) { m_pChild->RegisterBackoffAddress(addr); }
	virtual void UnregisterBackoffAddress( TNetAddress addr ) { m_pChild->UnregisterBackoffAddress(addr); }
	// ~IDatagramSocket

private:
	IDatagramSocketPtr m_pChild;

	float m_packetLossRateAccumulator;

	struct SPendingSend
	{
		size_t nLength;
		uint8 * pBuffer;
		TNetAddress to;
		_smart_ptr<CInternetSimulatorSocket> pThis;
	};
	std::set<NetTimerId> m_pendingSends;

	static void SimulatorUpdate( NetTimerId, void *, CTimeValue );
};

#endif

#endif
