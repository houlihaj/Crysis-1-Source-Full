#ifndef _BASICAREA_H_
#define _BASICAREA_H_

#define COPY_MEMBER(_dst,_src,_name) { (_dst)->_name = (_src)->_name; }

enum EObjList
{
	DYNAMIC_OBJECTS = 0,
	STATIC_OBJECTS,
	PROC_OBJECTS,
	ENTITY_LISTS_NUM
};

// streaming status
enum ESStatus
{
  eSStatus_Unloaded,
  eSStatus_Ready
};

struct SRNInfo
{
	SRNInfo()
	{
		memset(this,0,sizeof(*this));
	}

	SRNInfo(IRenderNode*_pNode)
	{
		fMaxViewDist = _pNode->m_fWSMaxViewDist;
		aabbBox = _pNode->GetBBox();
		pNode = _pNode;

/*#ifdef _DEBUG
		erType = _pNode->GetRenderNodeType();
		strncpy(szName, _pNode->GetName(), sizeof(szName));
		szName[sizeof(szName)-1] = 0;
#endif*/
	}

	bool operator == (const IRenderNode*_pNode) const { return (pNode == _pNode); }
	bool operator == (const SRNInfo & rOther) const { return (pNode == rOther.pNode); }

	float fMaxViewDist;
	AABB aabbBox;
	IRenderNode*pNode;

/*#ifdef _DEBUG
	EERType erType;
	char szName[32];
#endif*/
};

#define UPDATE_PTR_AND_SIZE(_pData,_nDataSize,_SIZE_PLUS)\
{\
	_pData += (_SIZE_PLUS);\
	_nDataSize -= (_SIZE_PLUS);\
	assert(_nDataSize>=0);\
}\

enum EAreaType
{
	eAreaType_Undefined,
	eAreaType_OcNode,
	eAreaType_VisArea
};

struct CBasicArea : public Cry3DEngineBase
{
  CBasicArea() 
	{ 
		m_nObjectsLastFrameId = 0; 
		m_eSStatus = eSStatus_Unloaded; 
		m_boxArea.min = m_boxArea.max = Vec3(0,0,0); 
//		memset(m_arrbEntitiesCompiled,0,sizeof(m_arrbEntitiesCompiled));
//		memset(m_arrfObjectsMaxViewDist,0,sizeof(m_arrfObjectsMaxViewDist));
//		m_bVegetBrigIsSet = true;
//		m_plstAreaBrush = NULL;
//		m_eAreaType = eAreaType_Undefined;
	//	m_nMergedBrushesOffset = 0;
		m_bMergeBrushes = false;
		m_pObjectsTree = NULL;
	}

	~CBasicArea();

	void CompileObjects(int nListId); // optimize objects lists for rendering
//	void ReadVegetationsBrigtnessFromTerrainLM();
//	void RenderObjects(int nDLightMask, const CCamera & EntViewCamera, const Vec3 & vAmbColor,
	//	struct SFogVolume * pFogVolume, bool bAllInside, float fSectorMinDist, uint nEntListId);
//	void MakeAreaBrush();
	//void FreeAreaBrushes();

	// area streaming
//  bool CheckUnload();
//  void CheckPhysicalized();
//  void UnloadStaticObjects();
//	void PreloadResources(Vec3 vPrevPortalPos, float fPrevPortalDistance);
//	int SerializeObjectsIntoMemBlock(bool bSave, class CMemoryBlock * pMemBlock);

	// member variables
//	PodArray<SRNInfo> m_lstEntities[ENTITY_LISTS_NUM]; // lists of objects in sector
//	PodArray<SRNInfo> m_lstShadowMapCasters[ENTITY_LISTS_NUM]; // lists of objects having 'cast shadow' flag
//	float m_arrfObjectsMaxViewDist[ENTITY_LISTS_NUM]; // maximum max view distance of objects in each list
//	bool m_arrbEntitiesCompiled[ENTITY_LISTS_NUM]; // is list compiled
//	bool m_bVegetBrigIsSet; // is brightness set
	bool m_bMergeBrushes;
	class COctreeNode * m_pObjectsTree;

	// min distance from current camera to m_boxHeigtmap and m_boxArea
	float m_arrfDistance[MAX_RECURSION_LEVELS];

	AABB m_boxArea; // bbox containing everything in sector including child sectors
	AABB m_boxStatics; // bbox containing only objects in STATIC_OBJECTS list of this node and height-map

	int m_nObjectsLastFrameId; // last frame id when objects of area was accessed for rendering or physics
	ESStatus m_eSStatus; // streaming status

//	PodArray<class CBrush *> * m_plstAreaBrush;
//	EAreaType m_eAreaType;
//	int m_nMergedBrushesOffset;
};

#endif // _BASICAREA_H_