#include "StdAfx.h"
#include "../../../Layer0/CCryDXPS.hpp"
#include "CCryDXPSDepthStencilState.hpp"

using namespace cell::Gcm;


static uint32 g_Comp2Gcm[9]	=
{
	0,
CELL_GCM_NEVER,			//D3D10_COMPARISON_NEVER = 1,
CELL_GCM_LESS,			//D3D10_COMPARISON_LESS = 2,
CELL_GCM_EQUAL,			//D3D10_COMPARISON_EQUAL = 3,
CELL_GCM_LEQUAL,		//D3D10_COMPARISON_LESS_EQUAL = 4,
CELL_GCM_GREATER,		//D3D10_COMPARISON_GREATER = 5,
CELL_GCM_NOTEQUAL,	//D3D10_COMPARISON_NOT_EQUAL = 6,
CELL_GCM_GEQUAL,		//D3D10_COMPARISON_GREATER_EQUAL = 7,
CELL_GCM_ALWAYS			//D3D10_COMPARISON_ALWAYS = 8,
};
static uint32 g_StencilOp2Gcm[9]	=
{
	0,
CELL_GCM_KEEP,			//D3D10_STENCIL_OP_KEEP = 1,
CELL_GCM_ZERO,			//D3D10_STENCIL_OP_ZERO = 2,
CELL_GCM_REPLACE,		//D3D10_STENCIL_OP_REPLACE = 3,
CELL_GCM_INCR,			//D3D10_STENCIL_OP_INCR_SAT = 4,
CELL_GCM_DECR,			//D3D10_STENCIL_OP_DECR_SAT = 5,
CELL_GCM_INVERT,		//D3D10_STENCIL_OP_INVERT = 6,
CELL_GCM_INCR_WRAP,	//D3D10_STENCIL_OP_INCR = 7,
CELL_GCM_DECR_WRAP	//D3D10_STENCIL_OP_DECR = 8,
};

CCryDXPSDepthStencilState::CCryDXPSDepthStencilState(const D3D10_DEPTH_STENCIL_DESC& rState):
CCryDXPSResource(EDXPS_RT_DEPTHSTENCILSTATE),
CCryRefAndWeak<CCryDXPSDepthStencilState>(this)
{
	if(rState.DepthEnable)
	{
		m_DepthTestEnable	=	CELL_GCM_TRUE;
		m_DepthMask	=	rState.DepthWriteMask==D3D10_DEPTH_WRITE_MASK_ALL?CELL_GCM_TRUE:CELL_GCM_FALSE;
		m_DepthFunc	=	g_Comp2Gcm[rState.DepthFunc];
	}
	else
		m_DepthTestEnable	=	CELL_GCM_FALSE;

	if(rState.StencilEnable)
	{
		m_StencilTestEnable	=	CELL_GCM_TRUE;
		m_StencilMaskW			=	rState.StencilWriteMask;
		m_StencilMaskR			=	rState.StencilReadMask;

		m_StencilFunc				=	g_Comp2Gcm[rState.FrontFace.StencilFunc];
		m_StencilOpFail			=	g_StencilOp2Gcm[rState.FrontFace.StencilFailOp];
		m_StencilOpDepthFail=	g_StencilOp2Gcm[rState.FrontFace.StencilDepthFailOp];
		m_StencilOpDepthPass=	g_StencilOp2Gcm[rState.FrontFace.StencilPassOp];
	}
	else
		m_StencilTestEnable	=	CELL_GCM_FALSE;

}
