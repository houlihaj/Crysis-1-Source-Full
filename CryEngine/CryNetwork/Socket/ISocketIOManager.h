#ifndef __ISOCKETIOMANAGER_H__
#define __ISOCKETIOMANAGER_H__

#pragma once

#include "NetAddress.h"
#include "Config.h"
#include "SocketError.h"

struct SSocketID
{
	static const uint16 InvalidId = ~uint16(0);

	SSocketID() : id(InvalidId), salt(0) {}
	SSocketID(uint16 i, uint16 s) : id(i), salt(s) {}

	uint16 id;
	uint16 salt;

	ILINE bool operator!() const
	{
		return id==InvalidId;
	}
	typedef uint16 (SSocketID::*unknown_bool_type);
	ILINE operator unknown_bool_type() const
	{
		return !!(*this)? &SSocketID::id : NULL;
	}
	ILINE bool operator!=( const SSocketID& rhs ) const
	{
		return !(*this == rhs);
	}
	ILINE bool operator==( const SSocketID& rhs ) const
	{
		return id==rhs.id && salt==rhs.salt;
	}
	ILINE bool operator<( const SSocketID& rhs ) const
	{
		return id<rhs.id || (id==rhs.id && salt<rhs.salt);
	}
	ILINE bool operator>( const SSocketID& rhs ) const
	{
		return id>rhs.id || (id==rhs.id && salt>rhs.salt);
	}

	uint32 AsInt() const
	{
		uint32 x = id;
		x <<= 16;
		x |= salt;
		return x;
	}
	static SSocketID FromInt( uint32 x )
	{
		return SSocketID(x>>16, x&0xffffu);
	}

	bool IsLegal() const
	{
		return salt != 0;
	}

	const char * GetText( char * tmpBuf = 0 ) const
	{
		static char singlebuf[64];
		if (!tmpBuf)
			tmpBuf = singlebuf;
		if (id == InvalidId)
			sprintf(tmpBuf, "<nil>");
		else if (!salt)
			sprintf(tmpBuf, "illegal:%d:%d", id, salt);
		else
			sprintf(tmpBuf, "%d:%d", id, salt);
		return tmpBuf;
	}

	uint32 GetAsUint32() const
	{
		return (uint32(salt)<<16) | id;
	}
};

struct IRecvFromTarget
{
	virtual void OnRecvFromComplete( const TNetAddress& from, const uint8 * pData, uint32 len ) = 0;
	virtual void OnRecvFromException( const TNetAddress& from, ESocketError err ) = 0;
};

struct ISendToTarget
{
	virtual void OnSendToException( const TNetAddress& to, ESocketError err ) = 0;
};

struct IConnectTarget
{
	virtual void OnConnectComplete() = 0;
	virtual void OnConnectException( ESocketError err ) = 0;
};

struct IAcceptTarget
{
	virtual void OnAccept( const TNetAddress& from, SOCKET sock ) = 0;
	virtual void OnAcceptException( ESocketError err ) = 0;
};

struct IRecvTarget
{
	virtual void OnRecvComplete( const uint8 * pData, uint32 len ) = 0;
	virtual void OnRecvException( ESocketError err ) = 0;
};

struct ISendTarget
{
	virtual void OnSendException( ESocketError err ) = 0;
};

enum ESocketIOManagerCaps
{
	eSIOMC_SupportsBackoff = BIT(0),
	eSIOMC_NoBuffering = BIT(1)
};

struct ISocketIOManager
{
	const uint32 caps;

	ISocketIOManager( uint32 c ) : caps(c) {}
	virtual ~ISocketIOManager() {}

	virtual const char * GetName() = 0;

	virtual int Poll( float waitTime, bool& performedWork ) = 0;

	virtual SSocketID RegisterSocket( SOCKET sock ) = 0;
	virtual void SetRecvFromTarget( SSocketID sockid, IRecvFromTarget * pTarget ) = 0;
	virtual void SetConnectTarget( SSocketID sockid, IConnectTarget * pTarget ) = 0;
	virtual void SetSendToTarget( SSocketID sockid, ISendToTarget * pTarget ) = 0;
	virtual void SetAcceptTarget( SSocketID sockid, IAcceptTarget * pTarget ) = 0;
	virtual void SetRecvTarget( SSocketID sockid, IRecvTarget * pTarget ) = 0;
	virtual void SetSendTarget( SSocketID sockid, ISendTarget * pTarget ) = 0;
	virtual void RegisterBackoffAddressForSocket( TNetAddress addr, SSocketID sockid ) = 0;
	virtual void UnregisterBackoffAddressForSocket( TNetAddress addr, SSocketID sockid ) = 0;
	virtual void UnregisterSocket( SSocketID sockid ) = 0;

	virtual bool RequestRecvFrom( SSocketID sockid ) = 0;
	virtual bool RequestSendTo( SSocketID sockid, const TNetAddress& addr, const uint8 * pData, size_t len ) = 0;

	virtual bool RequestConnect( SSocketID sockid, const TNetAddress& addr ) = 0;
	virtual bool RequestAccept( SSocketID sock ) = 0;
	virtual bool RequestSend( SSocketID sockid, const uint8 * pData, size_t len ) = 0;
	virtual bool RequestRecv( SSocketID sockid ) = 0;

	virtual void PushUserMessage( int msg ) = 0;

	virtual bool HasPendingData() = 0;
};

ISocketIOManager * CreateSocketIOManager( int ncpus );

#endif
