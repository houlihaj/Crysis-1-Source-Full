#include "StdAfx.h"
#include "CompositeDatagramSocket.h"

CCompositeDatagramSocket::CCompositeDatagramSocket()
{
}

CCompositeDatagramSocket::~CCompositeDatagramSocket()
{
}

void CCompositeDatagramSocket::AddChild( IDatagramSocketPtr child )
{
	stl::push_back_unique( m_children, child );
}

void CCompositeDatagramSocket::GetSocketAddresses( TNetAddressVec& addrs )
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		(*iter)->GetSocketAddresses( addrs );
}

ESocketError CCompositeDatagramSocket::Send( const uint8 * pBuffer, size_t nLength, const TNetAddress& to )
{
	ESocketError err = eSE_Ok;
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
	{
		ESocketError childErr = (*iter)->Send( pBuffer, nLength, to );
		if (childErr > err)
			err = childErr;
	}
	return err;
}

void CCompositeDatagramSocket::SetListener( IDatagramListener * pListener )
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		(*iter)->SetListener( pListener );
}

void CCompositeDatagramSocket::Die()
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		(*iter)->Die();
}

bool CCompositeDatagramSocket::IsDead()
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		if ((*iter)->IsDead())
			return true;
	return false;
}

SOCKET CCompositeDatagramSocket::GetSysSocket()
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		if ((*iter)->GetSysSocket() != INVALID_SOCKET)
			return (*iter)->GetSysSocket();
	return INVALID_SOCKET;
}

void CCompositeDatagramSocket::RegisterBackoffAddress( TNetAddress addr )
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		(*iter)->RegisterBackoffAddress(addr);
}

void CCompositeDatagramSocket::UnregisterBackoffAddress( TNetAddress addr )
{
	for (ChildVecIter iter = m_children.begin(); iter != m_children.end(); ++iter)
		(*iter)->UnregisterBackoffAddress(addr);
}