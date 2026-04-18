#ifndef __CRYDXPSGIDEVICE__
#define __CRYDXPSGIDEVICE__

#include "../Layer0/CCryDXPS.hpp"



class CCryDXPSGIDevice;
class CCryDXPSGIDevice	:		public	CCryRefAndWeak<CCryDXPSGIDevice>
{
public:
		CCryDXPSGIDevice();
    HRESULT GetAdapter( IDXGIAdapter **pAdapter);
    HRESULT CreateSurface( const DXGI_SURFACE_DESC *pDesc,UINT NumSurfaces,DXGI_USAGE Usage,const DXGI_SHARED_RESOURCE *pSharedResource,IDXGISurface **ppSurface);
    HRESULT QueryResourceResidency( void *const *ppResources,DXGI_RESIDENCY *pResidencyStatus,UINT NumResources);
    HRESULT SetGPUThreadPriority( INT Priority);
    HRESULT GetGPUThreadPriority( INT *pPriority);
		void Release();
 		void ReleaseResources(){};
   
};

typedef CCryDXPSGIDevice	IDXGIDevice;

#endif

