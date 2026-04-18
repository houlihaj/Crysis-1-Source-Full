#ifndef __CRYDXPSDEPTHSTENCILSTATE__
#define __CRYDXPSDEPTHSTENCILSTATE__

#include "../CCryDXPSResource.hpp"
#include "../../../CCryDXPSAPtr.hpp"
#include "../../../Layer0/CCryDXPS.hpp"

class CCryDXPSDepthStencilState;
class CCryDXPSDepthStencilState	:	public CCryDXPSResource	,	public	CCryRefAndWeak<CCryDXPSDepthStencilState>
{
	//BOOL DepthEnable;
	//cellGcmSetDepthTestEnable
	uint32 m_DepthTestEnable;

  //D3D10_DEPTH_WRITE_MASK DepthWriteMask;
	//cellGcmSetDepthMask
	uint32	m_DepthMask;

	//D3D10_COMPARISON_FUNC DepthFunc;
	//cellGcmSetDepthFunc
	uint32	m_DepthFunc;

	//BOOL StencilEnable;
	//cellGcmSetStencilEnable
	uint32 m_StencilTestEnable;

	//UINT8 StencilReadMask;
  //UINT8 StencilWriteMask;
	//cellGcmSetStencilMask
	uint32 m_StencilMaskW;

	//cellGcmSetStencilFunc
	uint32 m_StencilFunc;
	//uint32 m_StencilRef; //is will be passed on setting of the state
	uint32 m_StencilMaskR;

	//cellGcmSetStencilOp
	uint32 m_StencilOpFail;
	uint32 m_StencilOpDepthFail;
	uint32 m_StencilOpDepthPass;

  //D3D10_DEPTH_STENCILOP_DESC FrontFace;
  //D3D10_DEPTH_STENCILOP_DESC BackFace;

public:
	CCryDXPSDepthStencilState(const D3D10_DEPTH_STENCIL_DESC& rState);

	inline void Set(uint32 StencilRef)
	{
		using namespace cell::Gcm;


		GCM_CHECK_CMDBUFFER
		cellGcmSetDepthTestEnable(m_DepthTestEnable);
		if(m_DepthTestEnable)
		{
			cellGcmSetDepthMask(m_DepthMask);
#if defined(_DEBUG)
			CRY_ASSERT_MESSAGE(m_DepthFunc >= CELL_GCM_NEVER && m_DepthFunc <= CELL_GCM_ALWAYS, "Trying to set an invalid depth func");
			if(m_DepthFunc < CELL_GCM_NEVER || m_DepthFunc > CELL_GCM_ALWAYS)//invalid
				m_DepthFunc = CELL_GCM_LESS;
#endif
			cellGcmSetDepthFunc(m_DepthFunc);
		}

		cellGcmSetStencilTestEnable(m_StencilTestEnable);
		if(m_StencilTestEnable)
		{
			cellGcmSetStencilMask(m_StencilMaskW);
#if defined(_DEBUG)
			CRY_ASSERT_MESSAGE(m_StencilFunc >= CELL_GCM_NEVER && m_StencilFunc <= CELL_GCM_ALWAYS, "Trying to set an invalid stencil func");
			if(m_StencilFunc < CELL_GCM_NEVER || m_StencilFunc > CELL_GCM_ALWAYS)//invalid
				m_StencilFunc = CELL_GCM_ALWAYS;
#endif

			//PS3HACK, CELL_GCM_NV4097_SET_STENCIL_FUNC_REF, should be removed for SDK190
#if defined(CELL_GCM_DEBUG)
			cellGcmDebugCheckEnable(CELL_GCM_FALSE);
#endif
			cellGcmSetStencilFunc(m_StencilFunc,StencilRef,m_StencilMaskR);
#if defined(CELL_GCM_DEBUG)
			cellGcmDebugCheckEnable(CELL_GCM_TRUE);
#endif
			cellGcmSetStencilOp(m_StencilOpFail,m_StencilOpDepthFail,m_StencilOpDepthPass);
		}
	}

	inline	void		ReleaseResources()
	{
	}

	inline void GetDesc(D3D10_DEPTH_STENCIL_DESC *pDesc)
	{
	}
};

typedef CCryDXPSDepthStencilState ID3D10DepthStencilState;
typedef CCryAPtrWeakCnt<CCryDXPSDepthStencilState>	APWeakDepthStencilState;

#endif

