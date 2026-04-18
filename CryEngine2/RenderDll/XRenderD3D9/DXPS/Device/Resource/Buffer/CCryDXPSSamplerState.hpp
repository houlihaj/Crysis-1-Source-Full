#ifndef __CRYDXPSSAMPLERSTATE__
#define __CRYDXPSSAMPLERSTATE__

#include "../CCryDXPSResource.hpp"
#include "../../../Layer0/CCryDXPS.hpp"

class CCryDXPSSamplerState;
class CCryDXPSSamplerState		:		public CCryDXPSResource	,	public CCryRefAndWeak<CCryDXPSSamplerState>
{
private:
	// D3D10_FILTER Filter;
	// FLOAT MipLODBias;
	// cellGcmSetTextureFilter
	uint16		m_Bias;
	uint8			m_FilterMin;
	uint8			m_FilterMag;

  // UINT MaxAnisotropy;
	// cellGcmSetTextureControl
	// cellGcmSetTextureFilter
	uint8			m_AnisotropicLevel;

	// D3D10_TEXTURE_ADDRESS_MODE AddressU;
	// D3D10_TEXTURE_ADDRESS_MODE AddressV;
	// D3D10_TEXTURE_ADDRESS_MODE AddressW;
	// cellGcmSetTextureAddress
	uint8			m_WrapS;
	uint8			m_WrapT;
	uint8			m_WrapR;

	//D3D10_COMPARISON_FUNC ComparisonFunc;
	//FLOAT BorderColor[4];

  //FLOAT MinLOD;
	//FLOAT MaxLOD;
	//cellGcmSetTextureControl
	uint16	m_LODMin;
	uint16	m_LODMax;

public:
	CCryDXPSSamplerState(const D3D10_SAMPLER_DESC&	rDesc);

	inline	void	ReleaseResources()
	{
	}
  inline void GetDesc( D3D10_SAMPLER_DESC *pDesc)
	{
//		*pDesc	=	m_Desc;
	}

	inline void Set(uint32 Idx)
	{
		using namespace cell::Gcm;
		GCM_CHECK_CMDBUFFER
		cellGcmSetTextureControl(Idx,CELL_GCM_TRUE,m_LODMin,m_LODMax,m_AnisotropicLevel);
		cellGcmSetTextureFilter(Idx,m_Bias,m_FilterMin,m_FilterMag, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
		cellGcmSetTextureAddress(Idx,m_WrapS,m_WrapT,m_WrapR,CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL,CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
	}
	unsigned long		AddRef(){return IncRef();}
	unsigned long		Release(){return DecRef();}
};

typedef CCryDXPSSamplerState ID3D10SamplerState;
typedef CCryAPtrWeakCnt<CCryDXPSSamplerState>	APWeakSamplerState;

#endif

