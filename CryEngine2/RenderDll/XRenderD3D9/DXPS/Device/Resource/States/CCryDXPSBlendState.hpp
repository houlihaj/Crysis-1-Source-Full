#ifndef __CRYDXPSBLENDSTATE__
#define __CRYDXPSBLENDSTATE__

#include "../CCryDXPSResource.hpp"
#include "../../../CCryDXPSAPtr.hpp"
#include "../../../Layer0/CCryDXPS.hpp"

class CCryDXPSBlendState;
class CCryDXPSBlendState	:	public CCryDXPSResource	,	public	CCryRefAndWeak<CCryDXPSBlendState>
{
	//cellGcmSetBlendEnable
	uint32		m_BlendEnable;

	//cellGcmSetBlendEquation
	uint16		m_BlendFuncColor;
	uint16		m_BlendFuncAlpha;

	//cellGcmSetBlendFunc
	uint16		m_BlendDstColorFunc;
	uint16		m_BlendSrcColorFunc;
	uint16		m_BlendDstAlphaFunc;
	uint16		m_BlendSrcAlphaFunc;

	//cellGcmSetColorMask
	uint32		m_ColorMask;
	//cellGcmSetColorMaskMrt
	uint32		m_ColorMaskMrt;

public:
	CCryDXPSBlendState(const D3D10_BLEND_DESC& rState);

	inline void Set()
	{
		using namespace cell::Gcm;
		GCM_CHECK_CMDBUFFER
		cellGcmSetBlendEnable(m_BlendEnable);
		if(m_BlendEnable)
		{
			cellGcmSetBlendEquation(m_BlendFuncColor,m_BlendFuncAlpha);
			cellGcmSetBlendFunc(m_BlendSrcColorFunc,m_BlendDstColorFunc,m_BlendSrcAlphaFunc,m_BlendDstAlphaFunc);
		}
		cellGcmSetColorMask(m_ColorMask);
		cellGcmSetColorMaskMrt(m_ColorMaskMrt);
	}

	inline	void		ReleaseResources()
	{
	}

	inline void GetDesc(D3D10_BLEND_DESC *pDesc)
	{
	}
};

typedef CCryDXPSBlendState ID3D10BlendState;
typedef CCryAPtrWeakCnt<CCryDXPSBlendState>	APWeakBlendState;

#endif

