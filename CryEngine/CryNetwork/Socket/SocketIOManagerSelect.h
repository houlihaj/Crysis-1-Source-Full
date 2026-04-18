#ifndef __SOCKETIOMANAGERSELECT_H__
#define __SOCKETIOMANAGERSELECT_H__

#pragma once

#if defined(WIN32) || defined(WIN64)
#define HAS_SOCKETIOMANAGER_SELECT
#endif

#if defined(HAS_SOCKETIOMANAGER_SELECT)

#include "ISocketIOManager.h"
#include "STLPoolAllocator.h"
#include "WatchdogTimer.h"

class CSocketIOManagerSelect : public ISocketIOManager
{
public:
	virtual const char * GetName() { return "Select"; }

	bool Init();
	CSocketIOManagerSelect();
	~CSocketIOManagerSelect();

	virtual int Poll( float waitTime, bool& performedWork );

	virtual SSocketID RegisterSocket( SOCKET sock );
	virtual void SetRecvFromTarget( SSocketID sockid, IRecvFromTarget * pTarget );
	virtual void SetConnectTarget( SSocketID sockid, IConnectTarget * pTarget );
	virtual void SetSendToTarget( SSocketID sockid, ISendToTarget * pTarget );
	virtual void SetAcceptTarget( SSocketID sockid, IAcceptTarget * pTarget );
	virtual void SetRecvTarget( SSocketID sockid, IRecvTarget * pTarget );
	virtual void SetSendTarget( SSocketID sockid, ISendTarget * pTarget );
	virtual void RegisterBackoffAddressForSocket( TNetAddress addr, SSocketID sockid );
	virtual void UnregisterBackoffAddressForSocket( TNetAddress addr, SSocketID sockid );
	virtual void UnregisterSocket( SSocketID sockid );

	virtual bool RequestRecvFrom( SSocketID sockid );
	virtual bool RequestSendTo( SSocketID sockid, const TNetAddress& addr, const uint8 * pData, size_t len );

	virtual bool RequestConnect( SSocketID sockid, const TNetAddress& addr );
	virtual bool RequestAccept( SSocketID sock );
	virtual bool RequestSend( SSocketID sockid, const uint8 * pData, size_t len );
	virtual bool RequestRecv( SSocketID sockid );

	virtual void PushUserMessage( int msg );

	virtual bool HasPendingData() { return m_watchdog.HasStalled(); }

private:
	CWatchdogTimer m_watchdog;

	struct SOutgoingData
	{
		int nLength;
		uint8 data[MAX_UDP_PACKET_SIZE];
	};
#if USE_SYSTEM_ALLOCATOR
	typedef std::list<SOutgoingData> TOutgoingDataList;
#else
	typedef std::list<SOutgoingData, stl::STLPoolAllocator<SOutgoingData, stl::PoolAllocatorSynchronizationSinglethreaded> > TOutgoingDataList;
#endif
	struct SOutgoingAddressedData : public SOutgoingData
	{
		TNetAddress addr;
	};
#if USE_SYSTEM_ALLOCATOR
	typedef std::list<SOutgoingAddressedData> TOutgoingAddressedDataList;
#else
	typedef std::list<SOutgoingAddressedData, stl::STLPoolAllocator<SOutgoingAddressedData, stl::PoolAllocatorSynchronizationSinglethreaded> > TOutgoingAddressedDataList;
#endif

	struct SSocketInfo
	{
		SSocketInfo()
		{
			salt = 1;
			isActive = false;
			sock = INVALID_SOCKET;
			nRecvFrom = nRecv = nListen = 0;
			pRecvFromTarget = NULL;
			pSendToTarget = NULL;
			pConnectTarget = NULL;
			pAcceptTarget = NULL;
			pRecvTarget = NULL;
			pSendTarget = NULL;
		}

		uint16 salt;
		bool isActive;
		SOCKET sock;
		int nRecvFrom;
		int nRecv;
		int nListen;
		TOutgoingDataList outgoing;
		TOutgoingAddressedDataList outgoingAddressed;

		IRecvFromTarget * pRecvFromTarget;
		ISendToTarget * pSendToTarget;
		IConnectTarget * pConnectTarget;
		IAcceptTarget * pAcceptTarget;
		IRecvTarget * pRecvTarget;
		ISendTarget * pSendTarget;

		bool NeedRead() const { return nRecv || nRecvFrom || nListen; }
		bool NeedWrite() const { return !outgoing.empty() || !outgoingAddressed.empty(); }
	};
	std::vector<SSocketInfo> m_socketInfo;

	SSocketInfo * GetSocketInfo( SSocketID id );
	void WakeUp();

	std::queue<int> m_pendingReturns;
	sockaddr_in m_wakeupAddr;
	SOCKET m_wakeupSocket;
	SOCKET m_wakeupSender;
};

#endif

#endif
