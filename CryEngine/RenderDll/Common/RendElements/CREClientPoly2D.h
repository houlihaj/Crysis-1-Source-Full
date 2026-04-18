
#ifndef __CRECLIENTPOLY2D_H__
#define __CRECLIENTPOLY2D_H__

//=============================================================

struct SClientPolyStat2D
{
  int NumOccPolys;
  int NumRendPolys;
  int NumVerts;
  int NumIndices;
};


class CREClientPoly2D : public CRendElement
{
public:
  SShaderItem m_Shader;
  short m_sNumVerts;
  short m_sNumIndices;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F m_Verts[16];
  byte m_Indices[(16-2)*3];

  static SClientPolyStat2D mRS;
  static void mfPrintStat();

public:
  CREClientPoly2D()
  {
    mfSetType(eDATA_ClientPoly2D);
    m_sNumVerts = 0;
    mfUpdateFlags(FCEF_TRANSFORM | FCEF_NEEDFILLBUF);
  }

  virtual ~CREClientPoly2D() {};

  virtual void mfPrepare();
  virtual CRendElement *mfCopyConstruct(void);

  static TArray<CREClientPoly2D *> m_PolysStorage;
};

#endif  // __CRECLIENTPOLY_H__
