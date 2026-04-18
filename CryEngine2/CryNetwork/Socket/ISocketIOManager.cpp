#include "StdAfx.h"
#include "ISocketIOManager.h"
#include "SocketIOManagerIOCP.h"
#include "SocketIOManagerNull.h"
#include "SocketIOManagerSelect.h"

ISocketIOManager * CreateSocketIOManager( int ncpus )
{
#if defined(HAS_SOCKETIOMANAGER_IOCP)
	if (ncpus >= 2 || gEnv->pSystem->IsDedicated())
	{
		CSocketIOManagerIOCP * pMgr = new CSocketIOManagerIOCP();
		if (pMgr->Init())
			return pMgr;
		delete pMgr;
	}
#endif
#if defined(HAS_SOCKETIOMANAGER_SELECT)
	CSocketIOManagerSelect * pSelectMgr = new CSocketIOManagerSelect();
	if (pSelectMgr->Init())
		return pSelectMgr;
	delete pSelectMgr;
#endif
	return new CSocketIOManagerNull();
}
