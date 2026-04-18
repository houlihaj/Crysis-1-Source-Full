#ifndef __CRYDXPSGIOUTPUT__
#define __CRYDXPSGIOUTPUT__

#include "../Layer0/CCryDXPS.hpp"



class CCryDXPSGIOutput;
class CCryDXPSGIOutput	:		public	CCryRefAndWeak<CCryDXPSGIOutput>
{
public:
		CCryDXPSGIOutput();
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_OUTPUT_DESC *pDesc);
    HRESULT STDMETHODCALLTYPE GetDisplayModeList(DXGI_FORMAT EnumFormat,UINT Flags,UINT *pNumModes,DXGI_MODE_DESC *pDesc);
    HRESULT STDMETHODCALLTYPE FindClosestMatchingMode( const DXGI_MODE_DESC *pModeToMatch,DXGI_MODE_DESC *pClosestMatch,void *pConcernedDevice);
    HRESULT STDMETHODCALLTYPE WaitForVBlank( void);
    HRESULT STDMETHODCALLTYPE TakeOwnership( void *pDevice,BOOL Exclusive);
    void STDMETHODCALLTYPE ReleaseOwnership( void);
    HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities( DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps);
    HRESULT STDMETHODCALLTYPE SetGammaControl( const DXGI_GAMMA_CONTROL *pArray);
    HRESULT STDMETHODCALLTYPE GetGammaControl( DXGI_GAMMA_CONTROL *pArray);
    HRESULT STDMETHODCALLTYPE SetDisplaySurface( IDXGISurface *pScanoutSurface);
    HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData( IDXGISurface *pDestination);
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats);
		void Release();
 		void ReleaseResources(){};
   
};

typedef CCryDXPSGIOutput	IDXGIOutput;

#endif

