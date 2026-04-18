
#ifndef __CRECommon_H__
#define __CRECommon_H__

//=============================================================
//class CTerrain;

class CRECommon : public CRendElement
{
  friend class CRender3D;

public:

  CRECommon()
  {
    mfSetType(eDATA_TerrainSector);
    mfUpdateFlags(FCEF_TRANSFORM);
  }

  virtual ~CRECommon()
  {
  }

  virtual void mfPrepare();
	virtual bool mfDraw(CShader *ef, SShaderPass *sfm) { return true; }
};

class CREFarTreeSprites : public CRendElement
{
public:
  CREFarTreeSprites()
  {
    mfSetType(eDATA_FarTreeSprites);
    mfUpdateFlags(FCEF_TRANSFORM);
  }
  virtual bool mfDraw(CShader *ef, SShaderPass *sfm);
  virtual void mfPrepare();
  virtual float mfDistanceToCameraSquared(Matrix34& matInst) { return 999999999.0f; }
};

class CRETerrainDetailTextureLayers: public CRECommon
{
public:
  CRETerrainDetailTextureLayers()
  {
    mfSetType(eDATA_TerrainDetailTextureLayers);
    mfUpdateFlags(FCEF_TRANSFORM);
  }
  virtual bool mfDraw(CShader *ef, SShaderPass *sfm);
};

class CRETerrainParticles: public CRECommon
{
public:
  CRETerrainParticles()
  {
    mfSetType(eDATA_TerrainParticles);
    mfUpdateFlags(FCEF_TRANSFORM);
  }
  virtual bool mfDraw(CShader *ef, SShaderPass *sfm);
};

class CREShadowMapGen: public CRECommon
{
public:

  CREShadowMapGen()
  {
    mfSetType(eDATA_ShadowMapGen);
    mfUpdateFlags(FCEF_TRANSFORM);
  }

	virtual bool mfDraw(CShader *ef, SShaderPass *sfm) { return true; }
};


#endif  // __CRECommon_H__
