#include "StdAfx.h"
#include "../../../Layer0/CCryDXPS.hpp"
#include "../../../CCryDXPSMisc.hpp"
#include "CCryDXPSSamplerState.hpp"
//#include <assert.h>

static uint8 g_AnisoLevel[16]	=	
{
	CELL_GCM_TEXTURE_MAX_ANISO_1,
	CELL_GCM_TEXTURE_MAX_ANISO_1,
	CELL_GCM_TEXTURE_MAX_ANISO_2,
	CELL_GCM_TEXTURE_MAX_ANISO_2,
	CELL_GCM_TEXTURE_MAX_ANISO_4,
	CELL_GCM_TEXTURE_MAX_ANISO_4,
	CELL_GCM_TEXTURE_MAX_ANISO_6,
	CELL_GCM_TEXTURE_MAX_ANISO_6,
	CELL_GCM_TEXTURE_MAX_ANISO_8,
	CELL_GCM_TEXTURE_MAX_ANISO_8,
	CELL_GCM_TEXTURE_MAX_ANISO_10,
	CELL_GCM_TEXTURE_MAX_ANISO_10,
	CELL_GCM_TEXTURE_MAX_ANISO_12,
	CELL_GCM_TEXTURE_MAX_ANISO_12,
	CELL_GCM_TEXTURE_MAX_ANISO_16,
	CELL_GCM_TEXTURE_MAX_ANISO_16
};

static uint8 g_Wrap2Gcm[]	=	
{
	0,
  CELL_GCM_TEXTURE_WRAP,			//D3D10_TEXTURE_ADDRESS_WRAP = 1,
  CELL_GCM_TEXTURE_MIRROR,		//D3D10_TEXTURE_ADDRESS_MIRROR = 2,
  CELL_GCM_TEXTURE_CLAMP,			//D3D10_TEXTURE_ADDRESS_CLAMP = 3,
  CELL_GCM_TEXTURE_BORDER,		//D3D10_TEXTURE_ADDRESS_BORDER = 4,
  CELL_GCM_TEXTURE_MIRROR			//D3D10_TEXTURE_ADDRESS_MIRROR_ONCE = 5,
};


CCryDXPSSamplerState::CCryDXPSSamplerState(const D3D10_SAMPLER_DESC&	rDesc):
CCryDXPSResource(EDXPS_RT_SAMPLERSTATE),
CCryRefAndWeak<CCryDXPSSamplerState>(this)
{
	m_AnisotropicLevel	=	CELL_GCM_TEXTURE_MAX_ANISO_1;
	switch(rDesc.Filter)	//weird enum values, so no simple direct mapping possible -> switch
	{
		case						D3D10_FILTER_MIN_MAG_MIP_POINT:
		case D3D10_FILTER_COMPARISON_MIN_MAG_MIP_POINT:
			m_FilterMin	=	CELL_GCM_TEXTURE_NEAREST_NEAREST;
			m_FilterMag	=	CELL_GCM_TEXTURE_NEAREST;
			break;
		case						D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR:
		case D3D10_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR:
			m_FilterMin	=	CELL_GCM_TEXTURE_NEAREST_LINEAR;
			m_FilterMag	=	CELL_GCM_TEXTURE_NEAREST;
			break;
		case						D3D10_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:
		case D3D10_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:
			m_FilterMin	=	CELL_GCM_TEXTURE_NEAREST_NEAREST;
			m_FilterMag	=	CELL_GCM_TEXTURE_LINEAR;
			break;
		case						D3D10_FILTER_MIN_POINT_MAG_MIP_LINEAR:
		case D3D10_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR:
			m_FilterMin	=	CELL_GCM_TEXTURE_NEAREST_LINEAR;
			m_FilterMag	=	CELL_GCM_TEXTURE_LINEAR;
			break;
		case						D3D10_FILTER_MIN_LINEAR_MAG_MIP_POINT:
		case D3D10_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT:
			m_FilterMin	=	CELL_GCM_TEXTURE_LINEAR_NEAREST;
			m_FilterMag	=	CELL_GCM_TEXTURE_NEAREST;
			break;
		case						D3D10_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
		case D3D10_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
			m_FilterMin	=	CELL_GCM_TEXTURE_LINEAR_LINEAR;
			m_FilterMag	=	CELL_GCM_TEXTURE_NEAREST;
			break;
		case						D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT:
		case D3D10_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT:
			m_FilterMin	=	CELL_GCM_TEXTURE_LINEAR_NEAREST;
			m_FilterMag	=	CELL_GCM_TEXTURE_LINEAR;
			break;
		case						D3D10_FILTER_MIN_MAG_MIP_LINEAR:
		case D3D10_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR:
			m_FilterMin	=	CELL_GCM_TEXTURE_LINEAR_LINEAR;
			m_FilterMag	=	CELL_GCM_TEXTURE_LINEAR;
			break;
		case						D3D10_FILTER_ANISOTROPIC:
		case D3D10_FILTER_COMPARISON_ANISOTROPIC:
			m_FilterMin	=	CELL_GCM_TEXTURE_LINEAR_LINEAR;
			m_FilterMag	=	CELL_GCM_TEXTURE_LINEAR;
			CRY_ASSERT_MESSAGE(rDesc.MaxAnisotropy-1<sizeof(g_AnisoLevel),"MaxAnisotropy-1<sizeof(g_AnisoLevel)");
			m_AnisotropicLevel	=	g_AnisoLevel[(rDesc.MaxAnisotropy-1)&0xf];
			break;
		default:
			CRY_ASSERT_MESSAGE(false,"rDesc.Filter unsupported value");
	};

	CRY_ASSERT_MESSAGE(rDesc.AddressU<sizeof(g_Wrap2Gcm),"rDesc.AddressU<sizeof(g_Wrap2Gcm)");
	CRY_ASSERT_MESSAGE(rDesc.AddressV<sizeof(g_Wrap2Gcm),"rDesc.AddressV<sizeof(g_Wrap2Gcm)");
	CRY_ASSERT_MESSAGE(rDesc.AddressW<sizeof(g_Wrap2Gcm),"rDesc.AddressW<sizeof(g_Wrap2Gcm)");
	m_WrapS	=	g_Wrap2Gcm[rDesc.AddressU];
	m_WrapT	=	g_Wrap2Gcm[rDesc.AddressV];
	m_WrapR	=	g_Wrap2Gcm[rDesc.AddressW];

	m_Bias	=	static_cast<int16>(rDesc.MipLODBias*static_cast<f32>(1<<8));
	CRY_ASSERT_MESSAGE(m_Bias<(1<<12),"m_Bias<(1<<12)");


	m_LODMin=	static_cast<int16>(rDesc.MinLOD*static_cast<f32>(1<<8));
	m_LODMax=	static_cast<int16>(rDesc.MaxLOD*static_cast<f32>(1<<8));
	CRY_ASSERT_MESSAGE(m_LODMin<(1<<12),"m_LODMin<(1<<12)");
	CRY_ASSERT_MESSAGE(m_LODMax<(1<<12),"m_LODMax<(1<<12)");



}
