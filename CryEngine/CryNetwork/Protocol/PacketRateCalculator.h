/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  manages our packet rate and bandwidth usage locally for one
               channel
 -------------------------------------------------------------------------
 History:
 - 12/08/2004   12:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __PACKETRATECALCULATOR_H__
#define __PACKETRATECALCULATOR_H__

#pragma once

#include <queue>
#include "TimeValue.h"
#include "Config.h"
#include "PMTUDiscovery.h"
#include "INetwork.h"
#include "Config.h"

template <int N>
class CTimeRegression
{
public:
	CTimeRegression() : m_count(0), m_cached(false) {}

	void AddSample( CTimeValue x, CTimeValue y )
	{
		int s = m_count++ % N;
		m_x[s] = x;
		m_y[s] = y;
		m_cached = false;
	}

	bool Empty() const
	{
		return m_count == 0;
	}

	size_t Size() const
	{
		return m_count > N? N : m_count;
	}

	size_t Capacity() const
	{
		return N;
	}

	class CResult
	{
	public:
		CResult() : m_zeroTime(0.0f), m_intercept(0.0f), m_slope(0.0f) {}
		CResult( CTimeValue zeroTime, CTimeValue intercept, float slope ) : m_zeroTime(zeroTime), m_intercept(intercept), m_slope(slope) {}

		CTimeValue RemoteTimeAt( CTimeValue localTime ) const
		{
			return m_intercept + (localTime - m_zeroTime).GetSeconds() * m_slope;
		}

		CTimeValue GetZeroTime() const
		{
			return m_zeroTime;
		}

		float GetSlope() const
		{
			return m_slope;
		}

	private:
		CTimeValue m_zeroTime;
		CTimeValue m_intercept;
		float m_slope;
	};

	bool GetRegression( CTimeValue zeroTime, CResult& result ) const
	{
		if (m_cached)
		{
			if (m_result && fabsf((zeroTime - m_cache.GetZeroTime()).GetSeconds()) > 1.0f)
				;
			else
			{
				result = m_cache;
				return m_result;
			}
		}

		m_cached = true;
		STATIC_CHECK(N > 3, NotEnoughElements);
		int sz = Size();
		if (sz < 3)
			return m_result = false;
		CTimeValue averageX = 0.0f;
		CTimeValue averageY = 0.0f;
		for (int i=0; i<sz; i++)
		{
			averageX += (m_x[i] - zeroTime);
			averageY += m_y[i];
		}
		averageX /= sz;
		averageY /= sz;
		float slopeNumer = 0.0f;
		float slopeDenom = 0.0f;
		for (int i=0; i<sz; i++)
		{
			float xfactor = ((m_x[i] - zeroTime) - averageX).GetSeconds();
			slopeNumer += xfactor * (m_y[i] - averageY).GetSeconds();
			slopeDenom += xfactor * xfactor;
		}
		if (fabsf(slopeDenom) < 1e-6)
			return m_result = false;
		float slope = slopeNumer / slopeDenom;
		CTimeValue intercept = averageY - slope * averageX.GetSeconds();
		result = m_cache = CResult(zeroTime, intercept, slope);
		return m_result = true;
	}

private:
	CTimeValue m_x[N];
	CTimeValue m_y[N];
	int m_count;

	mutable bool m_cached;
	mutable bool m_result;
	mutable CResult m_cache;
};

template <class T, int N>
class CCyclicStatsBuffer
{
public:
	CCyclicStatsBuffer() : m_count(0), m_haveMedian(false), m_haveTotal(false) {}

	void Clear()
	{
		m_count = 0;
		m_haveMedian = m_haveTotal = false;
	}

	void AddSample( T x )
	{
		m_values[m_count++ % N] = x;
		m_haveMedian = m_haveTotal = false;
	}

	bool Empty() const
	{
		return m_count == 0;
	}

	size_t Size() const
	{
		return m_count > N? N : m_count;
	}

	size_t Capacity() const
	{
		return N;
	}

	T GetTotal() const
	{
		if (!m_haveTotal)
		{
			T sum = 0;
			size_t last = Size();
			for (size_t i=0; i<last; i++)
				sum += m_values[i];
			m_total = sum;
			m_haveTotal = true;
		}
		return m_total;
	}

	T GetMedian() const
	{
		if (!m_haveMedian)
		{
			T temp[N];
			size_t last = Size();
			memcpy( temp, m_values, last * sizeof(T) );
			std::sort( temp, temp + last );
			m_median = temp[last/2];
			m_haveMedian = true;
		}
		return m_median;
	}

	float GetAverage() const
	{
		return float(GetTotal())/Size();
	}

	T GetFirst() const
	{
		NET_ASSERT( !Empty() );
		if (m_count > N)
			return m_values[m_count%N];
		else
			return m_values[0];
	}

	T GetLast() const
	{
		NET_ASSERT( !Empty() );
		if (m_count >= N)
			return m_values[(m_count+N-1)%N];
		else
			return m_values[m_count-1];
	}

private:
	size_t m_count;
	T m_values[N];

	mutable bool m_haveMedian;
	mutable bool m_haveTotal;
	mutable T m_median;
	mutable T m_total;
};

class CTimedCounter
{
public:
	CTimedCounter( CTimeValue nMaxAge ) : m_nMaxAge(nMaxAge) {}
	void AddSample( CTimeValue sample )
	{
		if (sample > m_nMaxAge)
			while ( !m_samples.Empty() && m_samples.Front() < sample - m_nMaxAge)
				m_samples.Pop();
		if (m_samples.Full())
			m_samples.Pop();

		if (!m_samples.Empty())
		{
			CTimeValue backSample = m_samples.Back();
			//NET_ASSERT(backSample <= sample);
			// disable assert for now, and just verify things are safe
			if (backSample > sample)
				return;
		}
		m_samples.Push( sample );
	}
	CTimeValue FirstSampleTime()
	{
		return m_samples.Empty()? 0.0f : m_samples.Front();
	}
	uint32 Size()
	{
		return uint32(m_samples.Size());
	}
	bool Empty() { return m_samples.Empty(); }
	float CountsPerSecond()
	{
		return CountsPerSecond( m_samples.Empty()? 0.0f : m_samples.Back() );
	}
	float CountsPerSecond( CTimeValue nTime )
	{
		CTimeValue nBegin = FirstSampleTime();
		NET_ASSERT(nTime >= nBegin);
		return float(Size()) / (nTime - nBegin + 1.0f*(nTime == nBegin)).GetSeconds();
	}
	float CountsPerSecond( CTimeValue nTime, CTimeValue from )
	{
		Q::SIterator iter = m_samples.End();
		do 
		{
			--iter;
		} while (*iter > from && iter != m_samples.Begin());
//		++iter;
		if (iter == m_samples.End())
			return 0.0f;
		// *iter, not from --> we go one event earlier than the 'from' timestamp
		return float(m_samples.End() - iter) / (nTime - *iter).GetSeconds();
	}
	CTimeValue GetLast()
	{
		NET_ASSERT(!Empty());
		return m_samples.Back();
	}

private:
	CTimeValue m_nMaxAge;
	typedef MiniQueue<CTimeValue, 128> Q;
	Q m_samples;
};

// this class is responsible for determining when we should send a packet
class CPacketRateCalculator
{
	typedef CTimeRegression<64> TTimeRegression;

public:
	// bandwidth range in bytes/second
	// (between 9600 bits/second and 100 megabits/second)
	static const uint32 MaxBandwidth = 100*1024*1024/8;
	static const uint32 MinBandwidth = 9600/8;

	CPacketRateCalculator();
	void SentPacket( CTimeValue nTime, uint32 nSeq, uint16 nSize );
	void GotPacket( CTimeValue nTime, uint16 nSize );
	void AckedPacket( CTimeValue nTime, uint32 nSeq, bool bAck );
	void FragmentedPacket( CTimeValue nTime ) { m_pmtuDiscovery.FragmentedPacket( nTime ); }
	void AddPingSample( CTimeValue nTime, CTimeValue nPing, CTimeValue nRemoteTime );
	float GetPing( bool smoothed ) const;
	bool NeedMorePings() const { return m_ping.Size() < m_ping.Capacity() / 2; }
	float GetTcpFriendlyBitRate();
	uint32 GetBandwidthUsage( CTimeValue nTime, bool incoming );
	float GetPacketLossPerSecond( CTimeValue nTime );
	float GetPacketLossPerPacketSent( CTimeValue nTime );
	float GetPacketRate( bool idle, CTimeValue nTime );
	void SetPerformanceMetrics( const INetChannel::SPerformanceMetrics& metrics );
	CTimeValue GetRemoteTime() const;
	bool IsTimeReady() const
	{
		TTimeRegression::CResult r;
		return m_timeRegression.GetRegression(g_time, r);
	}

	CTimeValue PingMeasureDelay() { return CTimeValue(1.0f); }

	uint16 GetMaxPacketSize( CTimeValue nTime ) { return m_pmtuDiscovery.GetMaxPacketSize(nTime); }
	uint16 GetIdealPacketSize( int age, bool idle, uint16 maxPacketSize );
	CTimeValue GetNextPacketTime( int age, bool idle );

	float GetCurrentPacketRate( CTimeValue nTime )
	{
		return m_sentPackets.CountsPerSecond( nTime );
	}

	void UpdateLatencyLab(CTimeValue nTime);

	bool IsSufferingHighLatency(CTimeValue nTime) const;

	void GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false)
	{
		SIZER_COMPONENT_NAME(pSizer, "CPacketRateCalculator");

		if (countingThis)
			pSizer->Add(*this);
		m_pmtuDiscovery.GetMemoryStatistics(pSizer);
	}

private:
	struct SPacketSizerParams;
	struct SPacketSizerResult;

	static void PacketSizer( SPacketSizerResult& res, const SPacketSizerParams& p );
	SPacketSizerResult NextPacket( int age, bool idle );

	INetChannel::SPerformanceMetrics m_metrics;

	CCyclicStatsBuffer<float, 16> m_ping;
	CCyclicStatsBuffer<CTimeValue, 128> m_bandwidthUsedTime[2];
	CCyclicStatsBuffer<uint32, 128> m_bandwidthUsedAmount[2];
	CCyclicStatsBuffer<float, 256> m_tfrcHist;
	float m_lastSlowStartRate;
	bool m_hadAnyLoss;
	bool m_allowSlowStartIncrease;
	float m_rttEstimate;
	CTimeValue m_lastThroughputCalcTime;

	CTimedCounter m_sentPackets;
	CTimedCounter m_lostPackets;

	TTimeRegression m_timeRegression;

	mutable CTimeValue m_remoteTimeUpdated;
	mutable CTimeValue m_remoteTimeEstimate;
	mutable CTimeValue m_lastRemoteTimeEstimate;
	mutable CCyclicStatsBuffer<float, 16> m_timeVelocityBuffer;

	CPMTUDiscovery m_pmtuDiscovery;

	// number of milliseconds per second to move our clock to
	// sync to a remote clock
	const CTimeValue m_minAdvanceTimeRate;
	const CTimeValue m_maxAdvanceTimeRate;

	CTimeValue m_highLatencyStartTime;
	//std::map<uint32, CTimeValue> m_latencyLab;
	MiniQueue<CTimeValue, 127> m_latencyLab; // NOTE: the max value of N must NOT be bigger than 127 which is half the uint8 space (-128 ~ 127)!!!
	uint32 m_latencyLabHighestSequence;
};

#endif
