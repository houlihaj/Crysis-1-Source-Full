/*=============================================================================
CREPostProcess.cpp : Post processing RenderElement
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

=============================================================================*/

#include "StdAfx.h"

using namespace NPostEffects;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

CREPostProcess::CREPostProcess()
{
  mfSetType(eDATA_PostProcess);
  mfUpdateFlags(FCEF_TRANSFORM);

	m_resetDelayed = false;

  m_pREData=new CREPostProcessData;
  m_pREData->Create();
}

CREPostProcess::~CREPostProcess()
{  
  SAFE_DELETE(m_pREData)
}

void CREPostProcess:: mfPrepare()
{
  gRenDev->EF_CheckOverflow(0, 0, this);
  gRenDev->m_RP.m_pRE = this;
  gRenDev->m_RP.m_FlagsPerFlush |= RBSI_DRAWAS2D;
  gRenDev->m_RP.m_RendNumIndices = 0;
  gRenDev->m_RP.m_RendNumVerts = 0;

  if( CRenderer::CV_r_postprocess_effects_reset )
  {
    CRenderer::CV_r_postprocess_effects_reset = 0;
    mfReset();
  }

	if (m_resetDelayed)
	{
		m_resetDelayed = false;
		mfReset();
	}
}

void CREPostProcess::mfReset()
{
  if(m_pREData)
  {
    m_pREData->Reset();  
  }
}

void CREPostProcess::mfResetDelayed()
{
	m_resetDelayed = true;
}

int CREPostProcess:: mfSetParameter(const char *pszParam, float fValue) const
{ 
  assert((pszParam) && "mfSetParameter: null parameter");

  CEffectParam *pParam = PostEffectMgr().GetByName(pszParam);
  if(!pParam)
  {
    return 0;
  }

  pParam->SetParam( fValue );

  return 1;
}

void CREPostProcess:: mfGetParameter(const char *pszParam, float &fValue) const
{
  assert((pszParam) && "mfGetParameter: null parameter");

  CEffectParam *pParam = PostEffectMgr().GetByName(pszParam);
  if(!pParam)
  {
    return;
  }

  fValue = pParam->GetParam();  
}

int CREPostProcess:: mfSetParameterVec4(const char *pszParam, const Vec4 &pValue) const
{ 
  assert((pszParam) && "mfSetParameter: null parameter");

  CEffectParam *pParam = PostEffectMgr().GetByName(pszParam);
  if(!pParam)
  {
    return 0;
  }

  pParam->SetParamVec4( pValue );

  return 1;
}

void CREPostProcess:: mfGetParameterVec4(const char *pszParam, Vec4 &pValue) const
{
  assert((pszParam) && "mfGetParameter: null parameter");

  CEffectParam *pParam = PostEffectMgr().GetByName(pszParam);
  if(!pParam)
  {
    return;
  }

  pValue = pParam->GetParamVec4();  
}

int CREPostProcess::mfSetParameterString(const char *pszParam, const char *pszArg) const
{
  assert((pszParam || pszArg) && "mfSetParameter: null parameter");

  CEffectParam *pParam = PostEffectMgr().GetByName(pszParam);
  if(!pParam)
  {
    return 0;
  }

  pParam->SetParamString(pszArg);
  return 1;
}

void CREPostProcess::mfGetParameterString(const char *pszParam, const char *pszArg) const
{
  assert((pszParam || pszArg) && "mfGetParameter: null parameter");
  
  CEffectParam *pParam = PostEffectMgr().GetByName(pszParam);
  if(!pParam)
  {
    return;
  }

  pszArg = pParam->GetParamString();
}

