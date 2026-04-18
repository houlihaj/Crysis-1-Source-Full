#ifndef __BOOLCOMPRESS_H__
#define __BOOLCOMPRESS_H__

#pragma once

#include "Streams/CommStream.h"
#include "Streams/ByteStream.h"

class CArithModel;

class CBoolCompress
{
public:
	CBoolCompress(){}

	static const int AdaptBits = 5;
	static const uint8 StateRange = (1u<<AdaptBits)-1;
	static const uint8 StateMidpoint = (1u<<(AdaptBits-1))-1;

	void ReadMemento( CByteInputStream& stm ) const;
	void WriteMemento( CByteOutputStream& stm ) const;
	void NoMemento() const;
	bool ReadValue( CCommInputStream& stm) const;
	void WriteValue( CCommOutputStream& stm, bool value) const;

private:
	mutable bool m_lastValue;
	mutable uint8 m_prob;
};

#endif
