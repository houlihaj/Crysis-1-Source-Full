#include "StdAfx.h"
#include "../../../Layer0/CCryDXPS.hpp"
#include "CCryDXPSRasterizerState.hpp"

using namespace cell::Gcm;


CCryDXPSRasterizerState::CCryDXPSRasterizerState(const D3D10_RASTERIZER_DESC& rState):
CCryDXPSResource(EDXPS_RT_RASTERIZERSTATE),
CCryRefAndWeak<CCryDXPSRasterizerState>(this)
{
		//D3D10_FILL_MODE FillMode
	//cellGcmSetFrontPolygonMode
	m_FillMode	=	rState.FillMode==D3D10_FILL_WIREFRAME?CELL_GCM_POLYGON_MODE_LINE:CELL_GCM_POLYGON_MODE_FILL;


	//D3D10_CULL_MODE CullMode
	//BOOL FrontCounterClockwise //bs
	//cellGcmSetCullFace
	m_CullFace	=	rState.CullMode==D3D10_CULL_FRONT?CELL_GCM_FRONT:CELL_GCM_BACK;
	m_CullFaceEnable	=	rState.CullMode!=D3D10_CULL_NONE;
	
		



	//INT DepthBias;
	//FLOAT DepthBiasClamp
	//FLOAT SlopeScaledDepthBias
	//currently ignored


	//BOOL DepthClipEnable
	//currently ignored

	//BOOL ScissorEnable
	//cellGcmSetScissor
	m_ScissorTestEnable	=	rState.ScissorEnable;// if(false)cellGcmSetScissor(0,0,4095,4095);

	//BOOL MultisampleEnable;
	//BOOL AntialiasedLineEnable;
	//currently ignored
}
