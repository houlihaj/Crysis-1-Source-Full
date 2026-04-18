#ifndef __DEBUGOUTPUT_H__
#define __DEBUGOUTPUT_H__

#pragma once

#include <deque>
#include "CryThread.h"
#include "Context/SessionID.h"
#include "Socket/NetAddress.h"

class CDebugOutput
{
public:
	void Put( uint8 val );

	void Run( SOCKET sock );

	template <class T>
	void Write( const T& value )
	{
		CryAutoLock<CDebugOutput> lk(*this);
		const uint8 * pBuffer = (uint8*)&value;
		for (int i=0; i<sizeof(value); i++)
			Put(pBuffer[i]);
	}

	void Write( const string& value )
	{
		CryAutoLock<CDebugOutput> lk(*this);
		Write( value.length() );
		for (int i=0; i<value.length(); i++)
			Put( value[i] );
	}

	CSessionID m_sessionID;

	void WriteValue( const TNetAddressVec& vec )
	{
		CryAutoLock<CDebugOutput> lk(*this);
		Write( vec.size() );
		for (size_t i=0; i<vec.size(); i++)
			Write( CNetAddressResolver().ToString(vec[i]) );
	}

	virtual void Lock() = 0;
	virtual void Unlock() = 0;

private:
	static const int DATA_SIZE = 128 - sizeof(int);
	struct SBuf
	{
		SBuf() : ready(true), sz(0), pos(0) {}
		bool ready;
		int sz;
		int pos;
		char data[DATA_SIZE];
	};

	std::deque<SBuf> m_buffers;
};

#endif
