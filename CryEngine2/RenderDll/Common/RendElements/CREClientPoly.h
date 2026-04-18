
#ifndef __CRECLIENTPOLY_H__
#define __CRECLIENTPOLY_H__

//=============================================================

struct SClientPolyStat
{
  int NumOccPolys;
  int NumRendPolys;
  int NumVerts;
  int NumIndices;
};


class CREClientPoly : public CRendElement
{
public:
  SShaderItem m_Shader;
  CRenderObject *m_pObject;
  short m_sNumVerts;
  short m_sNumIndices;
  byte m_nAW;
  int m_nOffsVert;
  int m_nOffsTang;
  int m_nOffsInd;

  static SClientPolyStat mRS;
  static void mfPrintStat();

public:
  CREClientPoly()
  {
    mfSetType(eDATA_ClientPoly);
    m_sNumVerts = 0;
    mfUpdateFlags(FCEF_TRANSFORM | FCEF_NEEDFILLBUF);
  }

  virtual ~CREClientPoly() {};

  virtual void mfPrepare();
  virtual CRendElement *mfCopyConstruct(void);

  static TArray<CREClientPoly *> m_PolysStorage[4];
};

#endif  // __CRECLIENTPOLY2D_H__
