#ifndef _CVegetation_H_
#define _CVegetation_H_

#define VEGETATION_SCALE_RATIO (1.f/50.f) // max vegetation scale is 5

struct SBoxExtends
{
  void Set(const AABB& aabb, const Vec3& vPos);
  AABB Get(const Vec3& vPos) const;

  byte m_data[6];
};

// Warning: Average outdoor level has about 200.000 objects of this class allocated - so keep it small
class CVegetation : public IRenderNode, public Cry3DEngineBase
{
public:
  Vec3 m_vPos;
  IPhysicalEntity * m_pPhysEnt;
	IFoliage * m_pFoliage;
	uchar m_nObjectTypeID;
	uchar m_ucAngle;						// 0..255 -> 0..2*PI rotation
	uchar m_ucSunDotTerrain;
  uchar m_ucScale;
  SBoxExtends m_boxExtends;

	CVegetation();

  virtual ~CVegetation();

  void SetStatObjGroupId(int nVegetationanceGroupId) { m_nObjectTypeID = nVegetationanceGroupId; }

  const char *GetEntityClassName(void) const { return "Vegetation"; }
	Vec3 GetPos(bool bWorldOnly = true) const { assert(bWorldOnly); return m_vPos; }
  float GetScale(void) const { return VEGETATION_SCALE_RATIO*m_ucScale; }
  void SetScale(float fScale) { m_ucScale = (uchar)SATURATEB(fScale/VEGETATION_SCALE_RATIO); }
  const char *GetName(void) const;
  bool Render(const SRendParams & rendParams);
  IPhysicalEntity *GetPhysics(void) const { return m_pPhysEnt; }
  void SetPhysics(IPhysicalEntity * pPhysEnt) { m_pPhysEnt = pPhysEnt; }
  void SetMaterial(IMaterial *) {}
  IMaterial *GetMaterial(Vec3 * pHitPos = NULL);
  void SetMatrix( const Matrix34& mat ) { m_vPos = mat.GetTranslation(); }

  //  bool Render(const struct SRendParams & _EntDrawParams);//bool bNotAllIN, const CCamera & cam, int nDynLightMaskNoSun, struct SFogVolume * pFogVolume);
//  float GetBendingRandomFactor();
//  float Interpolate(float& pprev, float& prev, float& next, float& nnext, float ppweight, float pweight, float nweight, float nnweight);
  virtual void Physicalize( bool bInstant = false );
	bool PhysicalizeFoliage(bool bPhysicalize = true, int iSource=0);
	IPhysicalEntity* GetBranchPhys(int idx) { 
		return m_pFoliage && (unsigned int)idx < (unsigned int)((CStatObjFoliage*)m_pFoliage)->m_nRopes ? 
			((CStatObjFoliage*)m_pFoliage)->m_pRopes[idx] : 0; 
	}
	IFoliage *GetFoliage() { return m_pFoliage; }
	bool IsBreakable()
	{
		pe_params_part pp; pp.ipart=0;
		return m_pPhysEnt && m_pPhysEnt->GetParams(&pp) && pp.idmatBreakable>=0;
	}
	void AddBending(Vec3 const& v);

  virtual float GetMaxViewDist();
  IStatObj * GetEntityStatObj( unsigned int nPartId, unsigned int nSubPartId = 0, Matrix34* pMatrix = NULL, bool bReturnOnlyVisible = false);

//  virtual void Serialize(bool bSave, ICryPak * pPak, FILE * f);
  virtual EERType GetRenderNodeType() { return eERType_Vegetation; }
  virtual void Dephysicalize(bool bKeepIfReferenced=false);
  void Dematerialize( );
  virtual void GetMemoryUsage(ICrySizer * pSizer);
  virtual const AABB GetBBox() const;
  virtual void SetBBox( const AABB& WSBBox );

	void UpdateRndFlags();

	virtual void PreloadInstanceResources(Vec3 vPrevPortalPos, float fPrevPortalDistance, float fTime);

	ILINE CStatObj* GetStatObj() const { return GetObjManager()->m_lstStaticTypes[m_nObjectTypeID].GetStatObj(); }
	ILINE float GetZAngle() const;
	AABB CalcBBox();
	void CalcTerrainAdaption();
	void CalcMatrix( Matrix34 &tm, int * pTransFags = NULL );

	virtual uint8 GetMaterialLayers() const { return GetObjManager()->m_lstStaticTypes[m_nObjectTypeID].nMaterialLayers; }
	float GetLodForDistance(float fDistance);
	void UpdateInstanceBrightness();
	void Init();
	void ShutDown();
  void OnRenderNodeBecomeVisible();
  bool AddVegetationIntoSpritesList(float fDistance, SSectorTextureSet * pTerrainTexInfo);

protected:
};

#endif // _CVegetation_H_
