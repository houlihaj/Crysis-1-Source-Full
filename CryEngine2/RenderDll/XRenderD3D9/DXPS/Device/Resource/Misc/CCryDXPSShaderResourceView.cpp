#include "StdAfx.h"
#include "../../../Layer0/CCryDXPS.hpp"
#include "CCryDXPSShaderResourceView.hpp"
//#include <assert.h>

void CCryDXPSShaderResourceView::GetDesc(D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc)
{
	*pDesc	=	m_ShaderResourceViewDesc;
}

void CCryDXPSShaderResourceView::GetResource(ID3D10Resource** pResource)
{
	*pResource	=	m_pTex;
}


