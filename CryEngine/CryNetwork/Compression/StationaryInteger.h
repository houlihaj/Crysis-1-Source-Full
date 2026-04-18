#ifndef __STATIONARYINTEGER_H__
#define __STATIONARYINTEGER_H__

#pragma once

#include "Streams/CommStream.h"
#include "Streams/ByteStream.h"

class CStationaryInteger
{
public:
	CStationaryInteger( int32 nMin, int32 nMax ) : m_nMin(nMin), m_nMax(nMax) 
	{
		NET_ASSERT( m_nMax > m_nMin );
	}
	CStationaryInteger() : m_nMin(-1000), m_nMax(1000) {}

	void SetValues( int32 nMin, int32 nMax )
	{
		m_nMin = nMin;
		m_nMax = nMax;
		NET_ASSERT( m_nMax > m_nMin );
	}

	bool Load( XmlNodeRef node, const string& filename, const string& child = "Params" );

	bool WriteMemento( CByteOutputStream& stm ) const;
	bool ReadMemento( CByteInputStream& stm ) const;
	void NoMemento() const;
	void WriteValue( CCommOutputStream& stm, int32 value ) const;
	int32 ReadValue( CCommInputStream& stm ) const;

private:
	uint32 Quantize( int32 x ) const;
	int32 Dequantize( uint32 x ) const;

	// static data
	int64 m_nMin;
	int64 m_nMax;

	// memento data
	mutable uint32 m_oldValue;
	mutable uint32 m_probabilitySame;
};

#endif
