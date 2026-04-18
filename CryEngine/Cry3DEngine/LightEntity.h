#ifndef LIGHT_ENTITY_H
#define LIGHT_ENTITY_H

struct CLightEntity : public ILightSource, public Cry3DEngineBase
{
	virtual EERType GetRenderNodeType() { return eERType_Light; }
	virtual const char * GetEntityClassName(void) const { return "LightEntityClass"; }
	virtual const char *GetName(void) const;
	virtual Vec3 GetPos(bool) const;
	virtual bool Render(const SRendParams &) { return true; };
	virtual IPhysicalEntity *GetPhysics(void) const { return 0; }
	virtual void SetPhysics(IPhysicalEntity *) { }
	virtual void SetMaterial(IMaterial * pMat) { m_pMaterial = pMat; }
	virtual IMaterial *GetMaterial(Vec3 * pHitPos = NULL) { return m_pMaterial; }
	virtual float GetMaxViewDist();
	virtual void SetLightProperties(const CDLight & light);
	virtual CDLight& GetLightProperties() { return m_light; };
	virtual void Release(bool);
	virtual void SetMatrix( const Matrix34& mat );
	virtual const Matrix34& GetMatrix( ) { return m_Matrix; }
	virtual struct ShadowMapFrustum * GetShadowFrustum(int nId = 0);
	virtual void GetMemoryUsage(ICrySizer * pSizer);
  virtual const AABB GetBBox() const { return m_WSBBox; }
  virtual void SetBBox( const AABB& WSBBox ) { m_WSBBox = WSBBox; }
  virtual void SetCastingException(IRenderNode * pNotCaster) { m_pNotCaster = pNotCaster; }
  virtual bool IsLightAreasVisible();

	void InitEntityShadowMapInfoStructure();
	void UpdateGSMLightSourceShadowFrustum();
	bool ProcessFrustum(int nLod, float fCamBoxSize, float fDistanceFromView, PodArray<struct SPlaneObject> & lstCastersHull);
  void InitShadowFrustum_SUN_Conserv(ShadowMapFrustum * pFr, int dwAllowedTypes, float fGSMBoxSize, float fDistance, int nLod);
	void InitShadowFrustum_SUN(ShadowMapFrustum * pFr, int dwAllowedTypes, float fCamBoxSize, int nLod);
	void InitShadowFrustum_PROJECTOR(ShadowMapFrustum * pFr, int dwAllowedTypes);
	void InitShadowFrustum_OMNI(ShadowMapFrustum * pFr, int dwAllowedTypes);
	void FillFrustumCastersList_SUN(ShadowMapFrustum * pFr, int dwAllowedTypes, PodArray<struct SPlaneObject> & lstCastersHull);
	void FillFrustumCastersList_PROJECTOR(ShadowMapFrustum * pFr, int dwAllowedTypes);
	void FillFrustumCastersList_OMNI(ShadowMapFrustum * pFr, int dwAllowedTypes);
	void CheckValidFrustums_OMNI(ShadowMapFrustum * pFr);
	bool CheckFrustumsIntersect(CLightEntity* lightEnt);
  bool GetGsmFrustumBounds(const CCamera& viewFrustum, ShadowMapFrustum* pShadowFrustum);
	void DetectCastersListChanges(ShadowMapFrustum * pFr);
	void DebugCoverageMask();
	void OnCasterDeleted(IRenderNode * pCaster);
	int MakeShadowCastersHull(PodArray<SPlaneObject> & lstCastersHull);
  Vec3 GSM_GetNextScreenEdge(float fPrevRadius, float fPrevDistanceFromView);
  float GSM_GetLODProjectionCenter(const Vec3& vEdgeScreen, float fRadius);

	CLightEntity();
	~CLightEntity();

	CDLight m_light;
	_smart_ptr<IMaterial> m_pMaterial;
	Matrix34 m_Matrix;
  IRenderNode * m_pNotCaster;

	// used for shadow maps
	struct ShadowMapInfo
	{
		ShadowMapInfo() { memset(this,0,sizeof(ShadowMapInfo)); }
		void Release(struct IRenderer * pRenderer);
		struct ShadowMapFrustum * pGSM[MAX_GSM_LODS_NUM];
	} * m_pShadowMapInfo;

	void CheckUpdateCoverageMask();
	bool IsBoxAffected(const AABB & box);
	class CCullBuffer * m_arrCB[6];
	CCamera m_covCameras[6];
	bool m_bCoverageBufferDirty;
  AABB m_WSBBox;
};
#endif