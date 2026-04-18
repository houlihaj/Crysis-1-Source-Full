
#ifndef __CRETEMPMESH_H__
#define __CRETEMPMESH_H__

//=============================================================


class CRETempMesh : public CRendElement
{
public:
  int m_nVertices;
  CVertexBuffer *m_pVBuffer;
  int m_nIndices;
  SVertexStream m_Inds;

public:
  CRETempMesh()
  {
    m_nVertices = 0;
    m_nIndices = 0;
    m_pVBuffer = NULL;
    m_Inds.Reset();
    mfSetType(eDATA_TempMesh);
    mfUpdateFlags(FCEF_TRANSFORM);
  }

  virtual ~CRETempMesh()
  {
    if (m_pVBuffer)
    {
      gRenDev->ReleaseBuffer(m_pVBuffer, m_nVertices);
      m_pVBuffer = NULL;
    }
    gRenDev->ReleaseIndexBuffer(&m_Inds, m_nIndices);
    m_Inds.Reset();
  }

  virtual void mfPrepare();
  virtual bool mfDraw(CShader *ef, SShaderPass *sfm);
  virtual void *mfGetPointer(ESrcPointer ePT, int *Stride, EParamType Type, ESrcPointer Dst, int Flags);
  virtual bool mfPreDraw(SShaderPass *sl);
  virtual void mfReset();
  virtual int Size()
  {
    int nSize = sizeof(*this);
    if (m_pVBuffer)
      nSize += m_pVBuffer->Size(0, m_nVertices);
    return nSize;
  }
};

#endif  // __CRETEMPMESH_H__
