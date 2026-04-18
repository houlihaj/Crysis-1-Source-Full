#include "StdAfx.h"
#include "CCryDXPSGI.hpp"

HRESULT CryCreateDXPSGIFactory(REFIID riid,CCryDXPSGIFactory **ppFactory)
{
	*ppFactory	=	new	CCryDXPSGIFactory();
	return *ppFactory?0:-1;
}
