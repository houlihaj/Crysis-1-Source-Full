/*=============================================================================
CREPostProcess.h : Post processing RenderElement common data
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

=============================================================================*/

#ifndef __CREPOSTPROCESS_H__
#define __CREPOSTPROCESS_H__

class CREPostProcessData;

class CREPostProcess : public CRendElement
{
  friend class CD3D9Renderer;

public:

  CREPostProcess();
  virtual ~CREPostProcess();

  virtual void mfPrepare();
  virtual bool mfDraw(CShader *ef, SShaderPass *sfm);
	    
  // Use for setting numeric values, vec4 (colors, position, vectors, wtv), strings
  virtual int mfSetParameter(const char *pszParam, float fValue) const;
  virtual int mfSetParameterVec4(const char *pszParam, const Vec4 &pValue) const;
  virtual int mfSetParameterString(const char *pszParam, const char *pszArg) const;  

  virtual void mfGetParameter(const char *pszParam, float &fValue) const;
  virtual void mfGetParameterVec4(const char *pszParam, Vec4 &pValue) const;    
  virtual void mfGetParameterString(const char *pszParam, const char *pszArg) const;

  // Reset all post processing effects
  virtual void mfReset();  
	virtual void mfResetDelayed();

private:

	bool m_resetDelayed;
  CREPostProcessData *m_pREData;
};

#endif
