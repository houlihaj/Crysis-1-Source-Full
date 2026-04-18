#ifndef __CREOCLEAF_H__
#define __CREOCLEAF_H__

//=============================================================

struct SLightIndicies
{
  CDLight m_AssociatedLight;
  TArray<ushort> m_pIndicies;
  TArray<ushort> m_pIndiciesAttenFrustr;
  SVertexStream m_IndexBuffer;            // used for DirectX only
  unsigned short *GetIndices(int& nInds)
  {
    if (m_AssociatedLight.m_NumCM & 2)
    {
      nInds = m_pIndiciesAttenFrustr.Num();
      if (nInds)
        return &m_pIndiciesAttenFrustr[0];
      return NULL;
    }
    else
    {
      nInds = m_pIndicies.Num();
      if (nInds)
        return &m_pIndicies[0];
      return NULL;
    }
  }
  SVertexStream *GetIndexBuffer()
  {
    return &m_IndexBuffer;
  }
};


#define FCEF_CALCCENTER 0x10000
#define FCEF_DYNAMIC    0x20000

class CREMesh : public CRendElement
{
  friend class CRenderer;
  Vec3 m_Center;
public:
  struct CRenderChunk *m_pChunk;
  struct CRenderMesh  *m_pRenderMesh;

  CREMesh()
  {
    mfSetType(eDATA_Mesh);
    mfUpdateFlags(FCEF_TRANSFORM);
    m_pChunk = NULL;
    m_pRenderMesh = NULL;
    m_Center.Set(0,0,0);
  }

  virtual ~CREMesh()
  {
  }
  virtual struct CRenderChunk *mfGetMatInfo();
  virtual PodArray<struct CRenderChunk> *mfGetMatInfoList();
  virtual int mfGetMatId();
  virtual bool mfPreDraw(SShaderPass *sl);
  virtual bool mfIsHWSkinned();
  virtual void mfGetPlane(Plane& pl);
  virtual void mfPrepare();
  virtual void mfReset();
  virtual bool mfCullByClipPlane(CRenderObject *pObj);
  virtual void mfCenter(Vec3& Pos, CRenderObject*pObj);
  virtual bool mfDraw(CShader *ef, SShaderPass *sfm);
  virtual void *mfGetPointer(ESrcPointer ePT, int *Stride, EParamType Type, ESrcPointer Dst, int Flags);
  virtual float mfMinDistanceToCamera(Matrix34& matInst);
  virtual float mfDistanceToCameraSquared(Matrix34& matInst);
  virtual bool mfCheckUpdate(int nVertFormat, int Flags);
  virtual void mfGetBBox(Vec3& vMins, Vec3& vMaxs);
  virtual void mfPrecache(const SShaderItem& SH);
  virtual int Size()
  {
    int nSize = sizeof(*this);
    return nSize;
  }
};

#endif  // __CREOCLEAF_H__
