 
#ifndef __CRESKY_H__
#define __CRESKY_H__

//=============================================================

#include "VertexFormats.h"

class CStars;

class CRESky : public CRendElement
{
  friend class CRender3D;

public:

  float m_fTerrainWaterLevel;
	float m_fSkyBoxStretching;
  float m_fAlpha;
  int m_nSphereListId;
	PodArray<struct_VERTEX_FORMAT_P3F_COL4UB> * m_parrFogLayer;
	PodArray<struct_VERTEX_FORMAT_P3F_COL4UB> * m_parrFogLayer2;

#define MAX_SKY_OCCLAREAS_NUM	8
	struct_VERTEX_FORMAT_P3F_COL4UB m_arrvPortalVerts[MAX_SKY_OCCLAREAS_NUM][4];

public:
  CRESky()
  {
    mfSetType( eDATA_Sky );
    mfUpdateFlags( FCEF_TRANSFORM );
    m_fTerrainWaterLevel = 0;
    m_fAlpha = 1;
    m_nSphereListId = 0;
		m_parrFogLayer = m_parrFogLayer2 = NULL;
		m_fSkyBoxStretching=1.f;
		memset(m_arrvPortalVerts,0,sizeof(m_arrvPortalVerts));
  }

  virtual ~CRESky();
  virtual void mfPrepare();
  virtual bool mfDraw(CShader *ef, SShaderPass *sfm);
  virtual float mfDistanceToCameraSquared(Matrix34& matInst);
  void DrawSkySphere(float fHeight);
	bool DrawFogLayer();
	bool DrawBlackPortal();
};

class CREHDRSky : public CRendElement
{
public:
	CREHDRSky()
	: m_pRenderParams(0)
	, m_skyDomeTextureLastTimeStamp(-1)
	, m_frameReset(0)
	, m_pStars(0)
	, m_pTmpStorage(0)
	{
		mfSetType(eDATA_HDRSky);
		mfUpdateFlags(FCEF_TRANSFORM);
	}

	virtual ~CREHDRSky();
	virtual void mfPrepare();
	virtual bool mfDraw(CShader *ef, SShaderPass *sfm);
	virtual float mfDistanceToCameraSquared(Matrix34& matInst);

public:
	const SSkyLightRenderParams* m_pRenderParams;
	int m_moonTexId;

private:
	int m_skyDomeTextureLastTimeStamp;
	int m_frameReset;
	CStars* m_pStars;
	unsigned char* m_pTmpStorage;
};


#endif  // __CRESKY_H__
