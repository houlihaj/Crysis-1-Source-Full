
#ifndef __CREBEAM_H__
#define __CREBEAM_H__

//=============================================================


class CREBeam : public CRendElement
{
public:
  struct CRenderMesh * m_pBuffer;
  float m_fFogScale;
  TArray<SShaderParam *> m_ShaderParams;

  ColorF m_StartColor;
  ColorF m_EndColor;
  float m_fStartRadius;
  float m_fEndRadius;
  int m_LightStyle;
  float m_fLength;
  float m_fLengthScale;
  float m_fWidthScale;
  string m_ModelName;

public:
  CREBeam()
  {
    mfSetType(eDATA_Beam);
    m_fFogScale = 0;
    m_pBuffer = NULL;
    m_LightStyle = 0;
    m_fLength = 1.0f;
    mfUpdateFlags(FCEF_TRANSFORM);
  }

  virtual ~CREBeam()
  {
    for (uint i=0; i<m_ShaderParams.Num(); i++)
    {
      SShaderParam *pr = m_ShaderParams[i];
      SAFE_DELETE(pr);
    }
    m_ShaderParams.Free();
  }

  virtual void mfPrepare();
  virtual bool mfCompile(CParserBin& Parser, SParserFrame& Frame);
  virtual float mfDistanceToCameraSquared(Matrix34& matInst);
  virtual bool mfDraw(CShader *ef, SShaderPass *sl);
};

#endif  // __CREBEAM_H__
