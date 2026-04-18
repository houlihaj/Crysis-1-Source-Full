/*=============================================================================
PostProcessEffects.cpp : 3d engine post processing acess/scripts interfaces
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

Notes:
* Check PostEffects.h for list of available effects

=============================================================================*/

#include "StdAfx.h"

void C3DEngine::SetPostEffectParam(const char *pParam, float fValue) const
{  
  if(pParam && m_pREPostProcess)
  {
    m_pREPostProcess->mfSetParameter(pParam, fValue); 
  }    
}

void C3DEngine::GetPostEffectParam(const char *pParam, float &fValue) const
{
  if(pParam  && m_pREPostProcess)
  {
    m_pREPostProcess->mfGetParameter(pParam, fValue); 
  }  
}

void C3DEngine::SetPostEffectParamVec4(const char *pParam, const Vec4 &pValue) const
{  
  if(pParam && m_pREPostProcess)
  {
    m_pREPostProcess->mfSetParameterVec4(pParam, pValue); 
  }    
}

void C3DEngine::GetPostEffectParamVec4(const char *pParam, Vec4 &pValue) const
{
  if(pParam  && m_pREPostProcess)
  {
    m_pREPostProcess->mfGetParameterVec4(pParam, pValue); 
  }  
}

void C3DEngine::SetPostEffectParamString(const char *pParam, const char *pszArg) const
{  
  if(pParam && pszArg && m_pREPostProcess)
  {
    m_pREPostProcess->mfSetParameterString(pParam, pszArg); 
  }    
}

void C3DEngine::GetPostEffectParamString(const char *pParam, const char *pszArg) const
{
  if(pParam && m_pREPostProcess)
  {
    m_pREPostProcess->mfGetParameterString(pParam, pszArg); 
  }  
}

void C3DEngine::ResetPostEffects(bool delayed) const
{
  if(m_pREPostProcess)
  {
		if (delayed)
		{
			m_pREPostProcess->mfResetDelayed();
		}
		else
		{
    m_pREPostProcess->mfReset();
  }  
}
}
