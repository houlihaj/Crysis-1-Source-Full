#ifndef ROAD_RENDERNODE_H
#define ROAD_RENDERNODE_H

class CRoadRenderNode : public IRoadRenderNode, public Cry3DEngineBase
{
public:
	CRoadRenderNode();
	virtual ~CRoadRenderNode();
	
	// IRoadRenderNode implementation
	virtual void SetVertices( const Vec3 * pVerts, int nVertsNum, float fTexCoordBegin, float fTexCoordEnd, float fTexCoordBeginGlobal, float fTexCoordEndGlobal);
	virtual void SetSortPriority(uint8 sortPrio);

	// IRenderNode implementation
	virtual const char * GetEntityClassName(void) const { return "RoadObjectClass"; }
	virtual const char *GetName(void) const { return "RoadObjectName"; }
	virtual Vec3 GetPos(bool) const { return m_vPos; }
	virtual bool Render(const SRendParams &RendParams);
	virtual IPhysicalEntity *GetPhysics(void) const { return m_pPhysEnt; }
	virtual void SetPhysics(IPhysicalEntity * pPhys) { m_pPhysEnt = pPhys; }
	virtual void SetMaterial(IMaterial * pMat);
	virtual IMaterial *GetMaterial(Vec3 * pHitPos = NULL) { return m_pMaterial; }
	virtual float GetMaxViewDist();
	virtual EERType GetRenderNodeType() { return eERType_RoadObject_NEW; }
	virtual struct IRenderMesh * GetRenderMesh(int nLod) { return m_pRenderMesh; }
	virtual void GetMemoryUsage(ICrySizer * pSizer);
  virtual const AABB GetBBox() const { return m_WSBBox; }
  virtual void SetBBox( const AABB& WSBBox ) { m_WSBBox = WSBBox; }
  virtual void OnRenderNodeBecomeVisible();
	static bool ClipTriangle(PodArray<Vec3> & lstVerts, PodArray<ushort> & lstInds, int nStartIdxId, Plane * pPlanes);
  void Physicalize(ushort *pIndices, int nIndices, Vec3 *pVerts, int nPhysMatId);
	using IRenderNode::Physicalize;
  void DePhysicalize();
  void Compile();
  void ScheduleRebuild();
  void OnTerrainChanged();

	Vec3 m_vPos;
	IRenderMesh * m_pRenderMesh;
	_smart_ptr<IMaterial> m_pMaterial;
	PodArray<Vec3> m_arrVerts; 
	float m_arrTexCoors[2];
	float m_arrTexCoorsGlobal[2];

	uint8 m_sortPrio;

  // physical representation
  IPhysicalEntity * m_pPhysEnt;
  phys_geometry * m_pPhysGeom;
  AABB m_WSBBox;
};
#endif

