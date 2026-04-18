#ifndef __COMPOSITEDATAGRAMSOCKET_H__
#define __COMPOSITEDATAGRAMSOCKET_H__

#pragma once

#include "IDatagramSocket.h"

class CCompositeDatagramSocket : public IDatagramSocket
{
public:
	CCompositeDatagramSocket();
	~CCompositeDatagramSocket();

	void AddChild( IDatagramSocketPtr );

	// IDatagramSocketPtr
	virtual void GetSocketAddresses( TNetAddressVec& addrs );
	virtual ESocketError Send( const uint8 * pBuffer, size_t nLength, const TNetAddress& to );
	virtual void SetListener( IDatagramListener * pListener );
	virtual void Die();
	virtual bool IsDead();
	virtual SOCKET GetSysSocket();
	virtual void RegisterBackoffAddress( TNetAddress addr );
	virtual void UnregisterBackoffAddress( TNetAddress addr );
	// ~IDatagramSocketPtr

private:
	typedef std::vector<IDatagramSocketPtr> ChildVec;
	typedef ChildVec::iterator ChildVecIter;
	ChildVec m_children;
};

#endif
