#ifndef __CRYDXPSRASTERIZERSTATE__
#define __CRYDXPSRASTERIZERSTATE__

#include "../CCryDXPSResource.hpp"
#include "../../../CCryDXPSAPtr.hpp"
#include "../../../Layer0/CCryDXPS.hpp"

class CCryDXPSRasterizerState;
class CCryDXPSRasterizerState	:	public CCryDXPSResource	,	public	CCryRefAndWeak<CCryDXPSRasterizerState>
{

	//D3D10_FILL_MODE FillMode
	//cellGcmSetFrontPolygonMode
	uint32	m_FillMode;

	//D3D10_CULL_MODE CullMode
	//BOOL FrontCounterClockwise //bs
	//cellGcmSetCullFace
	uint32	m_CullFace;
	uint32	m_CullFaceEnable;

	//INT DepthBias;
	//FLOAT DepthBiasClamp
	//FLOAT SlopeScaledDepthBias
	//currently ignored


	//BOOL DepthClipEnable
	//cellGcmSetDepthTestEnable
	uint32	m_DepthTestEnable;

	//BOOL ScissorEnable
	//cellGcmSetScissor
	uint32 m_ScissorTestEnable;// if(false)cellGcmSetScissor(0,0,4095,4095);

	//BOOL MultisampleEnable;
	//BOOL AntialiasedLineEnable;
	//currently ignored

public:
	CCryDXPSRasterizerState(const D3D10_RASTERIZER_DESC& rState);

	inline void Set()
	{
		using namespace cell::Gcm;
		GCM_CHECK_CMDBUFFER
		cellGcmSetFrontPolygonMode(m_FillMode);
		cellGcmSetCullFace(m_CullFace);
		cellGcmSetCullFaceEnable(m_CullFaceEnable);
		if(!m_ScissorTestEnable)
		{
			cellGcmSetScissor(0,0,4095,4095);
		}
	}

	inline	void		ReleaseResources()
	{
	}

	inline void GetDesc(D3D10_RASTERIZER_DESC *pDesc)
	{
	}
};

typedef CCryDXPSRasterizerState ID3D10RasterizerState;
typedef CCryAPtrWeakCnt<CCryDXPSRasterizerState>	APWeakRasterizerState;

#endif

