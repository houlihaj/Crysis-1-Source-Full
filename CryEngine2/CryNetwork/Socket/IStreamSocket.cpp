#include "StdAfx.h"
#include "Network.h"
#include "TCPStreamSocket.h"

struct SCreateStreamSocketVisitor
{
	IStreamSocketPtr pStreamSocket;

	template<typename T>
	ILINE void Visit(const T&)
	{
		NetWarning("unsupported address type for CreateStreamSocket");
	}

	void Visit(const SIPv4Addr&)
	{
		CTCPStreamSocket* pSocket = new CTCPStreamSocket();
		if ( !pSocket->Init() )
		{
			delete pSocket;
			pSocket = NULL;
		}
		pStreamSocket = pSocket;
	}
};

IStreamSocketPtr CreateStreamSocket(const TNetAddress& addr)
{
	SCreateStreamSocketVisitor visitor;
	addr.Visit(visitor);
	return visitor.pStreamSocket;
}

