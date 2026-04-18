#include "StdAfx.h"
#include "CCryDXPSGIDevice.hpp"


CCryDXPSGIDevice::CCryDXPSGIDevice():
CCryRefAndWeak<CCryDXPSGIDevice>(this)
{
}

HRESULT CCryDXPSGIDevice::GetAdapter( IDXGIAdapter **pAdapter)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

HRESULT CCryDXPSGIDevice::CreateSurface( const DXGI_SURFACE_DESC *pDesc,UINT NumSurfaces,DXGI_USAGE Usage,const DXGI_SHARED_RESOURCE *pSharedResource,IDXGISurface **ppSurface)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

HRESULT CCryDXPSGIDevice::QueryResourceResidency( void *const *ppResources,DXGI_RESIDENCY *pResidencyStatus,UINT NumResources)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

HRESULT CCryDXPSGIDevice::SetGPUThreadPriority( INT Priority)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

HRESULT CCryDXPSGIDevice::GetGPUThreadPriority( INT *pPriority)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

void CCryDXPSGIDevice::Release()
{
	DecRef();
}
