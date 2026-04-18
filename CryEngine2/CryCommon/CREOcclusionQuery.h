
#ifndef __CREOCCLUSIONQUERY_H__
#define __CREOCCLUSIONQUERY_H__

//=============================================================

#define MAX_OCCLUSIONQUERIES_MGPU_COUNT 4

class CREOcclusionQuery : public CRendElement
{
  UINT_PTR m_nOcclusionID[MAX_OCCLUSIONQUERIES_MGPU_COUNT]; // this will carry a pointer LPDIRECT3DQUERY9, so it needs to be 64-bit on WIN64 
  int m_nVisSamples[MAX_OCCLUSIONQUERIES_MGPU_COUNT];
  int m_nCheckFrame[MAX_OCCLUSIONQUERIES_MGPU_COUNT];
  int m_nDrawFrame[MAX_OCCLUSIONQUERIES_MGPU_COUNT];
  int m_nCurrQuery;

  Vec3 m_vBoxMin;
  Vec3 m_vBoxMax;

  CRenderMesh * m_pRMBox;

public:

  static uint m_nQueriesPerFrameCounter;
  static uint m_nReadResultNowCounter;
  static uint m_nReadResultTryCounter;

  CREOcclusionQuery()
  {
    memset(m_nOcclusionID, 0, sizeof(m_nOcclusionID));
    memset(m_nVisSamples, 800*600, sizeof(m_nVisSamples));
    memset(m_nCheckFrame, 0, sizeof(m_nCheckFrame));
    memset(m_nDrawFrame, 0, sizeof(m_nDrawFrame));

    m_nCurrQuery = 0;

    m_vBoxMin=Vec3(0,0,0);
    m_vBoxMax=Vec3(0,0,0);
    m_pRMBox=NULL;

    mfSetType(eDATA_OcclusionQuery);
    mfUpdateFlags(FCEF_TRANSFORM);
  }

  virtual ~CREOcclusionQuery();

  virtual void mfPrepare();
  virtual bool mfDraw(CShader *ef, SShaderPass *sfm);
  virtual bool mfDrawQuad( struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *vQuad, int nOffs );
  virtual void mfReset();
  virtual bool mfReadResult_Try();
  virtual bool mfReadResult_Now();

  void SetRenderMeshBox( CRenderMesh *pRMBox )
  {
    assert( pRMBox );
    m_pRMBox = pRMBox;
  }

  void SetBBoxMin( const Vec3 &vBoxMin )
  {
    m_vBoxMin = vBoxMin;
  }

  void SetBBoxMax( const Vec3 &vBoxMax )
  {
    m_vBoxMax = vBoxMax;
  }

  virtual uint32 GetVisSamplesCount() const;

  uint32 GetDrawnFrame() const
  {
    return m_nDrawFrame[m_nCurrQuery];
  }

  uint32 GetCheckFrame() const
  {
    return m_nCheckFrame[m_nCurrQuery];
  }

};

#endif  // __CREOCCLUSIONQUERY_H__
