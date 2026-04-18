////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   visareas.h
//  Version:     v1.00
//  Created:     18/12/2002 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: visibility areas header
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef VisArea_H
#define VisArea_H

#include "BasicArea.h"
#include "CRAMSolution.h"

struct CVisArea : public CBasicArea, public IVisArea
{
	// editor interface
	virtual void Update(const Vec3 * pPoints, int nCount, const char * szName, const SVisAreaInfo & info);

	CVisArea();
	~CVisArea();
	bool IsSphereInsideVisArea(const Vec3 & vPos, const f32 fRadius);
	bool IsPointInsideVisArea(const Vec3 & vPos);
  bool IsBoxOverlapVisArea(const AABB & objBox);
	bool ClipToVisArea(bool bInside, Sphere& sphere, Vec3 const& vNormal);
	bool FindVisArea(IVisArea * pAnotherArea, int nMaxRecursion, bool bSkipDisabledPortals);
	bool FindVisAreaReqursive(IVisArea * pAnotherArea, int nMaxReqursion, bool bSkipDisabledPortals, PodArray<CVisArea*> & arrVisitedParents);
	void FindSurroundingVisArea( int nMaxReqursion, bool bSkipDisabledPortals, PodArray<IVisArea*> * pVisitedAreas = NULL, int nMaxVisitedAreas = 0, int nDeepness = 0);
	void FindSurroundingVisAreaReqursive( int nMaxReqursion, bool bSkipDisabledPortals, PodArray<IVisArea*> * pVisitedAreas, int nMaxVisitedAreas, int nDeepness, PodArray<CVisArea*> * pUnavailableAreas );
	int GetVisFrameId();
	Vec3 GetConnectionNormal(CVisArea * pPortal);
	void PreRender(int nReqursionLevel, CCamera CurCamera, CVisArea * pParent, CVisArea * pCurPortal, bool * pbOutdoorVisible, PodArray<CCamera> * plstOutPortCameras, bool * pbSkyVisible, bool * pbOceanVisible, PodArray<CVisArea*> & lstVisibleAreas);
	void UpdatePortalCameraPlanes(CCamera & cam, Vec3 * pVerts, bool bMergeFrustums);
	int GetVisAreaConnections(IVisArea ** pAreas, int nMaxConnNum, bool bSkipDisabledPortals = false);
  int GetRealConnections(IVisArea ** pAreas, int nMaxConnNum, bool bSkipDisabledPortals = false);
	bool IsPortalValid();
	bool IsPortalIntersectAreaInValidWay(CVisArea * pPortal);
	bool IsPortal();
	bool IsShapeClockwise();
	bool IsAffectedByOutLights() { return m_bAffectedByOutLights; }
	bool IsActive() { return m_bActive || (GetCVars()->e_portals==4); }
	void UpdateGeometryBBox();
  void DrawAreaBoundsIntoCBuffer(class CCullBuffer * pCBuffer);
  void ClipPortalVerticesByCameraFrustum(PodArray<Vec3> * pPolygon, const CCamera & cam);
  void GetMemoryUsage(ICrySizer*pSizer);
	bool IsConnectedToOutdoor();
	const char * GetName() { return m_sName; }
	int GetData(byte * & pData, int & nDataSize, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable);
	int Load(FILE * & f, int & nDataSize, std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable);
	const AABB* GetAABBox() const;
	const AABB* GetObjectsBBox() const;
  void UpdateOcclusionFlagInTerrain();

	//RAM
	void		CalcAmbientCube(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax);
	void		CalcHQAmbientCube(f32* pAmbientCube,const Vec3& rAABBMin,const Vec3& rAABBMax);
	void		RAMAssign(CDLight* pLight);
	void		RAMRelease(CDLight* pLight);


	char m_sName[32];
	PodArray<CVisArea*> m_lstConnections;
	Vec3 m_vConnNormals[2];
	int m_nRndFrameId;
	bool m_bActive;
	CRAMSolution	m_RAMSolution[2];	//double buffered
	int m_RAMFrame;
	PodArray<Vec3> m_lstShapePoints;
	float m_fHeight;
	Vec3 m_vAmbColor;
	bool m_bAffectedByOutLights;
	float m_fDistance;
  bool m_bSkyOnly;
  bool m_bOceanVisible;
	float m_fViewDistRatio;
	bool m_bDoubleSide;
	bool m_bUseDeepness;
	CCamera * m_arrOcclCamera[MAX_RECURSION_LEVELS];
	OcclusionTestClient m_OcclState;
	Vec3 m_arrvActiveVerts[4];
	bool m_bUseInIndoors;
	bool m_bThisIsPortal;
  bool m_bIgnoreSky;
	float m_fVolumetricFogDensityMultiplier;
	PodArray<CCamera> m_lstCurCameras;
};

struct SAABBTreeNode
{
	SAABBTreeNode( PodArray<CVisArea*> & lstAreas, AABB box, int nMaxRecursion = 0);
	~SAABBTreeNode( ) ;
	CVisArea * FindVisarea(const Vec3 & vPos);
	SAABBTreeNode* GetTopNode(const AABB& box, void** pNodeCache);
	bool IntersectsVisAreas(const AABB& box);
	int ClipOutsideVisAreas(Sphere& sphere, Vec3 const& vNormal);

	AABB nodeBox;
	PodArray<CVisArea*> nodeAreas;
	SAABBTreeNode * arrChilds[2];
};

struct CVisAreaManager : public IVisAreaManager, Cry3DEngineBase
{
	CVisArea * m_pCurArea, * m_pCurPortal;
	PodArray<CVisArea * > m_lstActiveEntransePortals;

	PodArray<CVisArea*> m_lstVisAreas;
  PodArray<CVisArea*> m_lstPortals;
  PodArray<CVisArea*> m_lstOcclAreas;
	PodArray<CVisArea*> m_lstActiveOcclVolumes;
	PodArray<CVisArea*> m_lstIndoorActiveOcclVolumes;
	PodArray<CVisArea*> m_lstVisibleAreas;
  bool m_bOutdoorVisible;
  bool m_bSkyVisible;
  bool m_bOceanVisible;
	bool m_bSunIsNeeded;
  PodArray<CCamera> m_lstOutdoorPortalCameras;
	PodArray<IVisAreaCallback*> m_lstCallbacks;
	SAABBTreeNode * m_pAABBTree;
	float m_fVolumetricFogGlobalDensity;

	CVisArea * m_pPrevArea;
	bool m_bFadeGlobalDensityMultiplier;
	float m_fStartGlobalDensityMultiplier;
	float m_fEndGlobalDensityMultiplier;
	float m_fStartTime;
	float m_fEndTime;

	CVisAreaManager();
	~CVisAreaManager();
	void UpdateAABBTree();
	void SetCurAreas();
	PodArray<CVisArea*> * GetActiveEntransePortals() { return &m_lstActiveEntransePortals; }
	void PortalsDrawDebug();
	bool IsEntityVisible(IRenderNode * pEnt);
	bool IsOutdoorAreasVisible();
  bool IsSkyVisible();
  bool IsOceanVisible();
	CVisArea * CreateVisArea();
	bool DeleteVisArea(CVisArea * pVisArea);
	bool SetEntityArea(IRenderNode* pEnt, const AABB & objBox, const float fObjRadius);
	void CheckVis();
	void DrawVisibleSectors();
	void ActivatePortal(const Vec3 &vPos, bool bActivate, const char * szEntityName);
	void UpdateVisArea(CVisArea * pArea, const Vec3 * pPoints, int nCount, const char * szName, const SVisAreaInfo & info);
	void UpdateConnections();
	void MoveObjectsIntoList(PodArray<SRNInfo> * plstVisAreasEntities, const AABB & boxArea, bool bRemoveObjects = false);
	IVisArea * GetVisAreaFromPos(const Vec3 &vPos);
	bool IntersectsVisAreas(const AABB& box, void** pNodeCache = 0);
	bool ClipOutsideVisAreas(Sphere& sphere, Vec3 const& vNormal, void* pNodeCache = 0);
	bool IsEntityVisAreaVisible(IRenderNode * pEnt, int nMaxReqursion, const CDLight * pLight );
	void MakeActiveEntransePortalsList(const CCamera & CurCamera, PodArray<CVisArea *> & lstActiveEntransePortals, CVisArea * pThisPortal);
  void MergeCameras(CCamera & cam, const CCamera & camPlus);
  void DrawOcclusionAreasIntoCBuffer(CCullBuffer * pCBuffer);
  bool IsValidVisAreaPointer(CVisArea * pVisArea);
  void GetStreamingStatus(int & nLoadedSectors, int & nTotalSectors);
  void GetMemoryUsage(ICrySizer*pSizer);
	bool IsOccludedByOcclVolumes(const AABB objBox, bool bCheckOnlyIndoorVolumes = false);
	void GetObjectsAround(Vec3 vExploPos, float fExploRadius, PodArray<SRNInfo> * pEntList, bool bSkip_ERF_NO_DECALNODE_DECALS = false, bool bSkipDynamicObjects = false);
	void DoVoxelShape(Vec3 vWSPos, float fRadius, int nSurfaceTypeId, Vec3 vBaseColor, EVoxelEditOperation eOperation, EVoxelBrushShape eShape, EVoxelEditTarget eTarget, PodArray<CVoxelObject*> * pAffectedVoxAreas);
	void IntersectWithBox(const AABB & aabbBox, PodArray<CVisArea*> * plstResult, bool bOnlyIfVisible);
	virtual bool Load(FILE * & f, int & nDataSize, struct SVisAreaManChunkHeader * pVisAreaManagerChunkHeader, std::vector<struct CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable);
	virtual bool GetCompiledData(byte * pData, int nDataSize, std::vector<struct CStatObj*> ** ppStatObjTable, std::vector<IMaterial*> ** ppMatTable);
	virtual int GetCompiledDataSize();
	void PrecacheLevel(bool bPrecacheAllVisAreas, Vec3 * pPrecachePoints, int nPrecachePointsNum, int *pPrecachedPointsCount = NULL);
	void LoadLightmap();
	void AddLightSource(CDLight * pLight);
	void AddLightSourceReqursive(CDLight * pLight, CVisArea* pArea, const int32 nDeepness);
	bool IsEntityVisAreaVisibleReqursive(CVisArea * pVisArea, int nMaxReqursion, PodArray<CVisArea*> * pUnavailableAreas, const CDLight * pLight );

	int				GetNumberOfVisArea() const;																				// the function give back the accumlated number of visareas and portals
	IVisArea *GetVisAreaById( int nID ) const;																	// give back the visarea interface based on the id (0..GetNumberOfVisArea()) it can be a visarea or a portal

	virtual void AddListener( IVisAreaCallback *pListener );
	virtual void RemoveListener( IVisAreaCallback *pListener );
	
	void MarkAllSectorsAsUncompiled();

	// -------------------------------------

	void GetObjectsByType(PodArray<IRenderNode*> & lstObjects, EERType objType );
  CVisArea * GetCurVisArea() { return m_pCurArea ? m_pCurArea : m_pCurPortal; }
  void GenerateStatObjAndMatTables(std::vector<CStatObj*> * pStatObjTable, std::vector<IMaterial*> * pMatTable);

	void FadeGlobalDensityMultiplier(CVisArea * pArea);
};

#endif // VisArea_H
