#ifndef _CREWATEROCEAN_
#define _CREWATEROCEAN_

struct struct_VERTEX_FORMAT_P3F;
class CWater;

class CREWaterOcean : public CRendElement
{
public:
  CREWaterOcean();
  virtual ~CREWaterOcean();

  virtual void mfPrepare();  
  virtual bool mfDraw( CShader* ef, SShaderPass* sfm );
  virtual void mfGetPlane(Plane& pl);

  virtual void Create( uint32 nVerticesCount, struct_VERTEX_FORMAT_P3F *pVertices, uint32 nIndicesCount, uint16 *pIndices );
  virtual void Release();

  virtual Vec3 GetPositionAt( float x, float y ) const;
  virtual Vec4 *GetDisplaceGrid( ) const;

private:

  uint32 m_nVerticesCount;
  uint32 m_nIndicesCount;

  void *m_pVertDecl;
  void *m_pVertices;
  void *m_pIndices;

  CWater *m_pWaterSim;

private:

  void UpdateFFT();
  void FrameUpdate();

};


#endif