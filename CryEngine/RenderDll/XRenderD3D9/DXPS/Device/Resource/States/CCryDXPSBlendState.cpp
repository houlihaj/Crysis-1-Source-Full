#include "StdAfx.h"
#include "../../../Layer0/CCryDXPS.hpp"
#include "CCryDXPSBlendState.hpp"

using namespace cell::Gcm;

static uint16 BlendOpD3D2Gcm[]=
{
	0,
	CELL_GCM_FUNC_ADD,								//D3D10_BLEND_OP_ADD = 1,
  CELL_GCM_FUNC_SUBTRACT,						//D3D10_BLEND_OP_SUBTRACT = 2,
  CELL_GCM_FUNC_REVERSE_SUBTRACT,	  //D3D10_BLEND_OP_REV_SUBTRACT = 3,
  CELL_GCM_MIN,											//D3D10_BLEND_OP_MIN = 4,
	CELL_GCM_MAX											//D3D10_BLEND_OP_MAX = 5,
};

static uint16 BlendFuncD3D2Gcm[]=
{
	0,
	CELL_GCM_ZERO,//D3D10_BLEND_ZERO = 1,
	CELL_GCM_ONE,//D3D10_BLEND_ONE = 2,
	CELL_GCM_SRC_COLOR,//D3D10_BLEND_SRC_COLOR = 3,
	CELL_GCM_ONE_MINUS_SRC_COLOR,//D3D10_BLEND_INV_SRC_COLOR = 4,
	CELL_GCM_SRC_ALPHA,//D3D10_BLEND_SRC_ALPHA = 5,
	CELL_GCM_ONE_MINUS_SRC_ALPHA,//D3D10_BLEND_INV_SRC_ALPHA = 6,
	CELL_GCM_DST_ALPHA,//D3D10_BLEND_DEST_ALPHA = 7,
	CELL_GCM_ONE_MINUS_DST_ALPHA,//D3D10_BLEND_INV_DEST_ALPHA = 8,
	CELL_GCM_DST_COLOR,//D3D10_BLEND_DEST_COLOR = 9,
	CELL_GCM_ONE_MINUS_DST_COLOR,//D3D10_BLEND_INV_DEST_COLOR = 10,
	CELL_GCM_SRC_ALPHA_SATURATE,//D3D10_BLEND_SRC_ALPHA_SAT = 11,
	CELL_GCM_CONSTANT_COLOR,//D3D10_BLEND_BLEND_FACTOR = 14,
	CELL_GCM_ONE_MINUS_CONSTANT_COLOR,//D3D10_BLEND_INV_BLEND_FACTOR = 15,
	CELL_GCM_SRC_COLOR,//D3D10_BLEND_SRC1_COLOR = 16,
	CELL_GCM_ONE_MINUS_SRC_COLOR,//D3D10_BLEND_INV_SRC1_COLOR = 17,
	CELL_GCM_SRC_ALPHA,//D3D10_BLEND_SRC1_ALPHA = 18,
	CELL_GCM_ONE_MINUS_SRC_ALPHA//D3D10_BLEND_INV_SRC1_ALPHA = 19,
};


CCryDXPSBlendState::CCryDXPSBlendState(const D3D10_BLEND_DESC& rState):
CCryDXPSResource(EDXPS_RT_BLENDSTATE),
CCryRefAndWeak<CCryDXPSBlendState>(this)
{
	m_BlendEnable			=	rState.BlendEnable[0]?CELL_GCM_TRUE:CELL_GCM_FALSE;
	m_BlendFuncColor	=	BlendOpD3D2Gcm[rState.BlendOp];
	m_BlendFuncAlpha	=	BlendOpD3D2Gcm[rState.BlendOpAlpha];

	m_BlendSrcColorFunc	=	BlendFuncD3D2Gcm[rState.SrcBlend];
	m_BlendDstColorFunc	=	BlendFuncD3D2Gcm[rState.DestBlend];
	m_BlendSrcAlphaFunc	=	BlendFuncD3D2Gcm[rState.SrcBlendAlpha];
	m_BlendDstAlphaFunc	=	BlendFuncD3D2Gcm[rState.DestBlendAlpha];

	m_ColorMask			=
	m_ColorMaskMrt	=	0;

	if(rState.RenderTargetWriteMask[0]&D3D10_COLOR_WRITE_ENABLE_RED)
		m_ColorMask	|=	CELL_GCM_COLOR_MASK_R;
	if(rState.RenderTargetWriteMask[0]&D3D10_COLOR_WRITE_ENABLE_GREEN)
		m_ColorMask	|=	CELL_GCM_COLOR_MASK_G;
	if(rState.RenderTargetWriteMask[0]&D3D10_COLOR_WRITE_ENABLE_BLUE)
		m_ColorMask	|=	CELL_GCM_COLOR_MASK_B;
	if(rState.RenderTargetWriteMask[0]&D3D10_COLOR_WRITE_ENABLE_ALPHA)
		m_ColorMask	|=	CELL_GCM_COLOR_MASK_A;

	if(rState.RenderTargetWriteMask[1]&D3D10_COLOR_WRITE_ENABLE_RED)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT1_R;
	if(rState.RenderTargetWriteMask[1]&D3D10_COLOR_WRITE_ENABLE_GREEN)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT1_G;
	if(rState.RenderTargetWriteMask[1]&D3D10_COLOR_WRITE_ENABLE_BLUE)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT1_B;
	if(rState.RenderTargetWriteMask[1]&D3D10_COLOR_WRITE_ENABLE_ALPHA)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT1_A;

	if(rState.RenderTargetWriteMask[2]&D3D10_COLOR_WRITE_ENABLE_RED)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT2_R;
	if(rState.RenderTargetWriteMask[2]&D3D10_COLOR_WRITE_ENABLE_GREEN)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT2_G;
	if(rState.RenderTargetWriteMask[2]&D3D10_COLOR_WRITE_ENABLE_BLUE)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT2_B;
	if(rState.RenderTargetWriteMask[2]&D3D10_COLOR_WRITE_ENABLE_ALPHA)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT2_A;

	if(rState.RenderTargetWriteMask[3]&D3D10_COLOR_WRITE_ENABLE_RED)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT3_R;
	if(rState.RenderTargetWriteMask[3]&D3D10_COLOR_WRITE_ENABLE_GREEN)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT3_G;
	if(rState.RenderTargetWriteMask[3]&D3D10_COLOR_WRITE_ENABLE_BLUE)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT3_B;
	if(rState.RenderTargetWriteMask[3]&D3D10_COLOR_WRITE_ENABLE_ALPHA)
		m_ColorMaskMrt	|=	CELL_GCM_COLOR_MASK_MRT3_A;
}
