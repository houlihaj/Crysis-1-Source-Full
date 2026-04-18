#include "StdAfx.h"
#include "InternetSimulatorSocket.h"

#if INTERNET_SIMULATOR

#include "Network.h"

CInternetSimulatorSocket::CInternetSimulatorSocket( IDatagramSocketPtr pChild ) : m_pChild(pChild), m_packetLossRateAccumulator(0.0f)
{
}

CInternetSimulatorSocket::~CInternetSimulatorSocket()
{
	for (std::set<NetTimerId>::iterator iter = m_pendingSends.begin(); iter != m_pendingSends.end(); ++iter)
	{
		NetTimerId oldTimer = *iter;
		SPendingSend * pPS = static_cast<SPendingSend*>(TIMER.CancelTimer(oldTimer));
		delete[] pPS->pBuffer;
		delete pPS;
	}
}

void CInternetSimulatorSocket::SimulatorUpdate( NetTimerId tid, void * p, CTimeValue t )
{
	SPendingSend * pPS = static_cast<SPendingSend*>(p);

	if (!pPS->pThis->IsDead())
		pPS->pThis->m_pChild->Send( pPS->pBuffer, pPS->nLength, pPS->to );

	delete[] pPS->pBuffer;
	delete pPS;
}

void CInternetSimulatorSocket::Die()
{
	m_pChild->Die();
}

bool CInternetSimulatorSocket::IsDead()
{
	return m_pChild->IsDead();
}

void CInternetSimulatorSocket::GetSocketAddresses( TNetAddressVec& addrs )
{
	m_pChild->GetSocketAddresses(addrs);
}

//static CMTRand_int32 r;
ESocketError CInternetSimulatorSocket::Send( const uint8 * pBuffer, size_t nLength, const TNetAddress& to )
{
	m_packetLossRateAccumulator += CVARS.PacketLossRate; // * r.GenerateFloat();
	if (m_packetLossRateAccumulator >= 1.0f)
	{
		m_packetLossRateAccumulator -= 1.0f;
		return eSE_Ok;
	}

	SPendingSend * pPS = new SPendingSend;
	CTimeValue sendTime = g_time + CVARS.PacketExtraLag; // * r.GenerateFloat();
	pPS->pBuffer = new uint8[nLength];
	memcpy( pPS->pBuffer, pBuffer, nLength );
	pPS->nLength = nLength;
	pPS->to = to;
	pPS->pThis = this;
	NetTimerId timer = TIMER.AddTimer( sendTime, SimulatorUpdate, pPS );
	return eSE_Ok;
}

void CInternetSimulatorSocket::SetListener( IDatagramListener * pListener )
{
	m_pChild->SetListener(pListener);
}

SOCKET CInternetSimulatorSocket::GetSysSocket()
{
	return m_pChild->GetSysSocket();
}

#endif
