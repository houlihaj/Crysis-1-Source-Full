#pragma once

#define OCTREENODE_RENDER_FLAG_OBJECTS		1
#define OCTREENODE_RENDER_FLAG_OCCLUDERS	2
#define OCTREENODE_RENDER_FLAG_CASTERS		4
#define OCTREENODE_RENDER_FLAG_OBJECTS_ONLY_ENTITIES 8

enum EOcTeeNodeListType
{
  eMain,
  eCasters,
  eOccluders,
  eMergedCache,
};

template <class T> struct TDoublyLinkedList
{
  T * m_pFirstNode, * m_pLastNode;

  TDoublyLinkedList() 
  { 
    m_pFirstNode = m_pLastNode = NULL; 
  }

  ~TDoublyLinkedList() 
  { 
    assert(!m_pFirstNode && !m_pLastNode); 
  }

  void insertAfter(T * pAfter, T * pObj)
  {
    pObj->m_pPrev = pAfter;
    pObj->m_pNext = pAfter->m_pNext;
    if (pAfter->m_pNext == NULL)
      m_pLastNode = pObj;
    else
      pAfter->m_pNext->m_pPrev = pObj;
    pAfter->m_pNext = pObj;
  }

  void insertBefore(T * pBefore, T * pObj)
  {
    pObj->m_pPrev = pBefore->m_pPrev;
    pObj->m_pNext = pBefore;
    if (pBefore->m_pPrev == NULL)
      m_pFirstNode = pObj;
    else
      pBefore->m_pPrev->m_pNext = pObj;
    pBefore->m_pPrev    = pObj;
  }

  void insertBeginning(T * pObj)
  {
    if (m_pFirstNode == NULL)
    {
      m_pFirstNode = pObj;
      m_pLastNode  = pObj;
      pObj->m_pPrev = NULL;
      pObj->m_pNext = NULL;
    }
    else
      insertBefore(m_pFirstNode, pObj);
  }

  void insertEnd(T * pObj)
  {
    if (m_pLastNode == NULL)
      insertBeginning(pObj);
    else
      insertAfter(m_pLastNode, pObj);
  }

  void remove(T * pObj)
  {
    if (pObj->m_pPrev == NULL)
      m_pFirstNode = pObj->m_pNext;
    else
      pObj->m_pPrev->m_pNext = pObj->m_pNext;

    if (pObj->m_pNext == NULL)
      m_pLastNode = pObj->m_pPrev;
    else
      pObj->m_pNext->m_pPrev = pObj->m_pPrev;

    pObj->m_pPrev = pObj->m_pNext = NULL;
  }
};

struct SInstancingInfo
{
  SInstancingInfo() { pStatInstGroup=0; aabb.Reset(); fMinSpriteDistance = 10000.f; bInstancingInUse=0; }
  StatInstGroup * pStatInstGroup;
  DynArray<CVegetation*> arrInstances;
  DynArray16<SInstanceInfo> arrMats;
  DynArray16<SVegetationSpriteInfo> arrSprites;
  AABB aabb;
  float fMinSpriteDistance;
  bool bInstancingInUse;
};

class COctreeNode : public IOctreeNode, Cry3DEngineBase
{
public:
	COctreeNode(const AABB & box, struct CVisArea * pVisArea, COctreeNode * pParent = NULL);
	~COctreeNode();
	
	void InsertObject(IRenderNode * pObj, const AABB & objBox, const float fObjRadius, const Vec3 & vObjCenter);
	bool DeleteObject(IRenderNode * pObj);
	bool Render(const CCamera & rCam, CCullBuffer & rCB, int nRenderMask, const Vec3 & vAmbColor = Vec3(0,0,0));
  PodArray<CDLight*> * GetAffectingLights();
  void AddLightSource(CDLight * pSource);
  void CheckInitAffectingLights();
	void FillShadowCastersList(CDLight * pLight, struct ShadowMapFrustum * pFr, PodArray<SPlaneObject> * pShadowHull, bool bUseFrustumTest);
	void MarkAsUncompiled();
	COctreeNode * FindNodeContainingBox(const AABB & objBox);
	void MoveObjectsIntoList(PodArray<SRNInfo> * plstResultEntities, const AABB * pAreaBox, bool bRemoveObjects = false, bool bSkipDecals = false, bool bSkip_ERF_NO_DECALNODE_DECALS = false, bool bSkipDynamicObjects = false, EERType eRNType = eERType_Last);
	int PhysicalizeVegetationInBox(const AABB &bbox);
	int DephysicalizeVegetationInBox(const AABB &bbox);
	int GetData(byte * & pData, int & nDataSize, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable);

	const AABB & GetBBox() { return m_nodeBox; }
	const AABB & GetObjectsBBox() { return m_objectsBox; }
	void DeleteObjectsByFlag(int nRndFlag);
	uint GetLastVisFrameId() { return m_nLastVisFrameId; }
	void GetObjectsByType(PodArray<IRenderNode*> & lstObjects, EERType objType, AABB * pBBox);
	void CollectScatteringRenderNodes(PodArray<IRenderNode*>& RenderNodes) const;
	bool CleanUpTree();
	int GetObjectsCount(EOcTeeNodeListType eListType);
	int	 SerializeObjects(bool bSave, class CMemoryBlock * pMemBlock, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable);
	bool IsRightNode(const AABB & objBox, const float fObjRadius, float fObjMaxViewDist);
	int GetMemoryUsage(ICrySizer * pSizer);
	IRenderNode * FindTerrainSectorVoxObject(const AABB & objBox);
	void UpdateMergingTimeLimit();
	void FreeAreaBrushes(bool bRecursive = false);
	CTerrainNode * GetTerrainNode() { return m_pTerrainNode; }
	void UpdateTerrainNodes();
	bool RayObjectsIntersection2D( Vec3 vStart, Vec3 vEnd, Vec3 & vClosestHitPoint, float & fClosestHitDistance, EERType eERType );
  bool RayVoxelIntersection2D( Vec3 vStart, Vec3 vEnd, Vec3 & vClosestHitPoint, float & fClosestHitDistance );

  template <class T>
  int Load_T(T * & f, int & nDataSize, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable);
  int Load(FILE * & f, int & nDataSize, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable);
  int Load(uchar * & f, int & nDataSize, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable);

  static void FreeLoadingCache();
  void GenerateStatObjAndMatTables(std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable);
  static void ReleaseEmptyNodes();
  bool IsEmpty();

protected:
	AABB GetChildBBox(int nChildId);
	void CompileObjects();
  void CompileCharacter( ICharacterInstance * pChar, unsigned char &nInternalFlags);
	void CompileObjectsBrightness();
	float GetNodeObjectsMaxViewDistance();
	void CheckUpdateAreaBrushes();
	
	// Check if min spec specified in render node passes current server config spec.
	bool CheckRenderFlagsMinSpec( uint32 dwRndFlags );
  
  void LinkObject(IRenderNode * pObj);
  void UnlinkObject(IRenderNode * pObj);

  static int Cmp_OctreeNodeSize(const void* v1, const void* v2);

	COctreeNode * m_arrChilds[8];
  TDoublyLinkedList<IRenderNode> m_lstObjects;
	PodArray<IRenderNode *> m_lstOccluders;
	PodArray<IRenderNode *> m_lstMergedObjects;
	AABB m_nodeBox;
  Vec3 m_vNodeCenter;
  float m_fNodeRadius;
  AABB m_objectsBox;
	uint m_nOccludedFrameId;
	uint m_nLastVisFrameId;
  PodArray<CDLight*> m_lstAffectingLights; uint m_nLightMaskFrameId;
	bool m_bHasIndoorObjects : 1, m_bHasOutdoorObjects : 1, m_bAreaMeshReady : 1, m_bHasLights : 1, m_bHasVoxels : 1;
	bool m_bHasOccluders : 1;
	bool m_bHasShadowCasters : 1;
	Vec3 m_vSunDirReady;
	float m_fObjectsMaxViewDist;
	OcclusionTestClient m_occlTestState;
	PodArray<class CBrush *> * m_plstAreaBrush;
	EAreaType m_eAreaType;
	static float m_fAreaMergingTimeLimit;
	float m_fReadyMergingDistance;
  static CMemoryBlock m_mbLoadingCache;

  DynArray<SInstancingInfo> * m_pCompiledInstances;
  AABB m_aabbCompiledInstances;

  COctreeNode * m_pParent;

public:
  uint32 m_dwSpritesAngle;
  static PodArray<COctreeNode*> m_arrEmptyNodes;
};
