#include "StdAfx.h"
#include "CCryDXPSGIOutput.hpp"


CCryDXPSGIOutput::CCryDXPSGIOutput():
CCryRefAndWeak<CCryDXPSGIOutput>(this)
{
}

HRESULT CCryDXPSGIOutput::GetDesc(DXGI_OUTPUT_DESC *pDesc)
{
	memset(pDesc,0,sizeof(DXGI_OUTPUT_DESC));
	memcpy(pDesc->DeviceName,L"RSX PS3",sizeof(L"RSX PS3"));
	pDesc->DesktopCoordinates.top	=	0;
	pDesc->DesktopCoordinates.left	=	0;
	pDesc->DesktopCoordinates.bottom	=	720;
	pDesc->DesktopCoordinates.right	=	1280;
	pDesc->Rotation	=	DXGI_MODE_ROTATION_IDENTITY;
	return 0;
}

HRESULT CCryDXPSGIOutput::GetDisplayModeList(DXGI_FORMAT EnumFormat,UINT Flags,UINT *pNumModes,DXGI_MODE_DESC *pDesc)
{
	if(EnumFormat!=DXGI_FORMAT_R16G16B16A16_FLOAT &&
		EnumFormat!=DXGI_FORMAT_R8G8B8A8_UNORM)
		return DXGI_ERROR_NOT_FOUND;

	*pNumModes	=	1;
	if(pDesc)
	{
		pDesc->Width=1280;
    pDesc->Height=720;
    pDesc->RefreshRate.Numerator=60;
    pDesc->RefreshRate.Denominator=1;
    pDesc->Format=EnumFormat;
    pDesc->ScanlineOrdering=(DXGI_MODE_SCANLINE_ORDER)0;
    pDesc->Scaling=(DXGI_MODE_SCALING)0;
	}
	return 0;
}

HRESULT CCryDXPSGIOutput::FindClosestMatchingMode( const DXGI_MODE_DESC *pModeToMatch,DXGI_MODE_DESC *pClosestMatch,void *pConcernedDevice)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

HRESULT CCryDXPSGIOutput::WaitForVBlank( void)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

HRESULT CCryDXPSGIOutput::TakeOwnership( void *pDevice,BOOL Exclusive)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

void CCryDXPSGIOutput::ReleaseOwnership( void)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
}

HRESULT CCryDXPSGIOutput::GetGammaControlCapabilities( DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

HRESULT CCryDXPSGIOutput::SetGammaControl( const DXGI_GAMMA_CONTROL *pArray)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

HRESULT CCryDXPSGIOutput::GetGammaControl( DXGI_GAMMA_CONTROL *pArray)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

HRESULT CCryDXPSGIOutput::SetDisplaySurface( IDXGISurface *pScanoutSurface)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

HRESULT CCryDXPSGIOutput::GetDisplaySurfaceData( IDXGISurface *pDestination)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

HRESULT CCryDXPSGIOutput::GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats)
{
	CRY_ASSERT_MESSAGE(0,"Not implemented yet!");
	return -1;
}

void CCryDXPSGIOutput::Release()
{
	DecRef();
}
