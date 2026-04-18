#include "StdAfx.h"
#include "CCryDXPSGIAdapter.hpp"


CCryDXPSGIAdapter::CCryDXPSGIAdapter():
CCryRefAndWeak<CCryDXPSGIAdapter>(this)
{
}

HRESULT CCryDXPSGIAdapter::EnumOutputs( uint32 Output,IDXGIOutput **ppOutput)
{
	*ppOutput	=	&m_Output;
	return Output==0?0:DXGI_ERROR_NOT_FOUND;
}

HRESULT CCryDXPSGIAdapter::GetDesc( class DXGI_ADAPTER_DESC *pDesc)
{
	memset(pDesc,0,sizeof(DXGI_ADAPTER_DESC));
	memcpy(pDesc->Description,L"RSX PS3 GPU",sizeof(L"RSX PS3 GPU"));
	pDesc->DedicatedVideoMemory	=	224*1024*1024;
		//pDesc->DedicatedSystemMemory ??
		//pDesc->SharedSystemMemory??
		//pDesc->AdapterLuid??

	return 0;
}

HRESULT CCryDXPSGIAdapter::CheckInterfaceSupport( REFGUID InterfaceName,uint32 *pUMDVersion)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

void CCryDXPSGIAdapter::Release()
{
	DecRef();
}
