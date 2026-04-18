/*=============================================================================
RenderLight.h : 
Copyright 2004 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Nick Kasyan
* Taken over by Tamas Schlagl
=============================================================================*/
#ifndef __RENDERLIGHT_H__
#define __RENDERLIGHT_H__

#if _MSC_VER > 1000
#pragma once
#endif

//#define REDUCE_DOT3LIGHTMAPS_TO_CLASSIC	// for debugging
//#define DISPLAY_MORE_INFO

//typedef float real;
typedef double real;//typedef controlling the accuracy

#define HDR_EXP_BASE    (1.04)
#define HDR_EXP_OFFSET  (64.0)
#define HDR_LOG(a, b)   ( log(b) / log(a) )

#include <float.h>
#include "LMCompCommon.h"
#include "LMCompStructures.h"
#include "IEntityRenderState.h"
#include "..\Objects\BrushObject.h"
#include "..\Objects\VoxelObject.h"
#include "I3dEngine.h"
#include <direct.h>

//--------------------------------------------------------
#include "PhotonTool.h"
#include "PhotonViewTool.h"
#include "GeomCore.h"
#include "GeomMesh.h"
#include "RLMesh.h"
#include "RLSurface.h"
#include "DirectIlluminance.h"
#include "IndirectIlluminance.h"
#include "SunDirectIlluminance.h"
#include "OcclusionIntegrator.h"
#include "AmbientOcclusionIntegrator.h"

//Editor GUI
#include "RLWaitProgress.h"

static const float scfMaxGridSize = 2.f;

#define	MIN_LIGHTMAP_SIZE	4

//flags for CRadPoly
#define	NOLIGHTMAP_FLAG		1
#define	MERGE_FLAG			2	
#define	SHARED_FLAG			4		
#define	MERGE_SOURCE_FLAG	8	
#define	DECAL_FLAG	64	
#define	DO_NOT_COMPRESS_FLAG 128	//flag for patches


#define	WRONG_NORMALS_FLAG	16	
#define	NOT_NORMALIZED_NORMALS_FLAG	32	
#define	ONLY_SUNLIGHT 256			//flag for mesh
#define	DOUBLE_SIDED_MAT 512		//flag for mesh
#define	REBUILD_USED 1024			//flag for mesh
#define	CAST_LIGHTMAP 2048			//flag for mesh
#define	RECEIVES_SUNLIGHT 4096		//flag for mesh
#define	HASH_CHANGED 8192			//flag for mesh
#define	RECEIVES_LIGHTMAP (8192<<1)	//flag for mesh

//forces the patches to align on 4 pixel boundaries
#define MAKE_BLOCK_ALIGN	

struct SUV
{
	float u,v;				//texture coordinates
};

inline const AABB MakeSafeAABB(const Vec3& rMin, const Vec3& rMax)
{
	static const float scfMargin = 0.1f;//margin to add 
	Vec3 vMin(rMin);			Vec3 vMax(rMax); 
	if(vMin.x == vMax.x)		vMax.x += scfMargin;
	if(vMin.y == vMax.y)		vMax.y += scfMargin;
	if(vMin.z == vMax.z)		vMax.z += scfMargin;
	return AABB(vMin, vMax);
}

//used for warning gathering
typedef enum
{
	EWARNING_EXPORT_FAILED = 0,		//!< export of lightmaps has failed
	EWARNING_LIGHT_EXPORT_FAILED,	//!< export of lightsources has failed
	EWARNING_DOUBLE_SIDED,			//!< double sided material
	EWARNING_TOO_MANY_OCCL_LIGHTS,	//!< more than 4 active occlusion map light sources at the same time on glm
	EWARNING_NO_FIT,				//!< glm does not fit into a single lightmap
	EWARNING_HUGE_PATCH,			//!< has patch(es) which are larger than halve a lightmap wide or high
	EWARNING_WRONG_NORMALS,			//!< glm has wrong normals
	EWARNING_DENORM_NORMALS,		//!< glm has denormalized normals
	EWARNING_LIGHT_RADIUS,			//!< light has a too little radius
	EWARNING_LIGHT_INTENSITY,		//!< light has a too little intensity
	EWARNING_LIGHT_FRUSTUM			//!< spotlight has invalid frustum
}EWARNING_TYPE;

static const unsigned int scuiWarningTextAllocation = 300;//number of chars allocated on stack for warning string

static inline const bool IsNormalized(const float cfSqLen)
{
	static const float scfThreshold = 0.1f;
	if(fabs(cfSqLen - 1.f) < scfThreshold)
		return true;
	return false; 
}

//supports flexible subsampling patterns, but for simplicity just use 9x or nothing for now
class CAASettings
{
protected:
	unsigned int	m_uiScale;
	float			m_fInvScale;
public:

	bool m_bEnabled;
	CAASettings():m_bEnabled(false),m_uiScale(1),m_fInvScale(1.0f){}
	void SetScale(const unsigned int cuiScale)
	{
		assert(cuiScale != 0);
		m_uiScale = cuiScale;
		m_fInvScale = 1.0f / (float)cuiScale;
	}
	const float GetInvScale()const{return m_fInvScale;}
	const unsigned int GetScale()const{return m_uiScale;}
	const float RetrieveRealSamplePos(const real cfOrig)
	{
		switch(m_uiScale)
		{
		case 1:
			return cfOrig;
			break;
		case 2:
			{
				const real cfNumber = (real)((int)(cfOrig * 0.5f));
				return (cfNumber - (real)1.0f/(real)3.0 + (real)2.0/(real)3.0 * (cfOrig - cfNumber * (real)2.0));//map onto -1/3, 1/3, 2/3, 4/3,...
			}
			break;
		case 3:
			return ((real)1.0f/(real)3.0 * cfOrig - (real)1.0f/(real)3.0);//map onto -1/3, 0/3, 1/3, 2/3, 3/3, 4/3,...
			break;
		default:
			return m_fInvScale * cfOrig;
		}
	}
	//returns the middle index, or tells whether this is the one or not
	const bool IsMiddle(const unsigned int cuiX, const unsigned int cuiY)
	{
		switch(m_uiScale)
		{
		case 1:
			return true;
		case 2:
			return (cuiX == 1 && cuiY == 1);
		case 3:
			return (cuiX == 1 && cuiY == 1);
		}
		return true;
	}
};

class		CLightPatch;
class		CRadPoly;  
class		CRadVertex;
class		CRadMesh;
struct		IStatObj;
class		CLightScene; 


//////////////////////////////////////////////////////////////////////
class CLightScene : public CRLWaitProgress
{
public:

	//Association "RenderNodeID" --- "CRLMesh"
	//used by Lightmap Packing
	typedef std::pair<std::pair<IRenderNode*,IRenderMesh*>, CRLMesh*> t_pairRLMesh;
	typedef std::multimap<std::pair<IRenderNode*,IRenderMesh*>, CRLMesh* > t_mmapRLMeshes;
	typedef std::pair<t_mmapRLMeshes::iterator, t_mmapRLMeshes::iterator> t_mmapRLMeshRange;

	//Association "RenderNodeID" --- "CGeomMesh"
	//used by Lightmap Packing
	typedef std::pair<IRenderNode*, CGeomMesh*> t_pairGeomMesh;
	typedef std::multimap<IRenderNode*, CGeomMesh* > t_mmapGeomMeshes;
	typedef std::pair<t_mmapGeomMeshes::iterator, t_mmapGeomMeshes::iterator> t_mmapGeomMeshRange;


	typedef std::pair<IRenderNode*, CRLUnwrapGroup* > t_pairUnwrapGroup;
	typedef std::multimap<IRenderNode*, CRLUnwrapGroup* > t_mmapUnwrapGroups;

	typedef std::pair<int32, int32 > t_pairInt;

	typedef std::pair<uint16,struct TexCoord2Comp> t_pairIndTexCoord;
	//	typedef std::map<uint16,struct TexCoord2Comp> t_mapIndTexCoord;
	typedef std::list<t_pairIndTexCoord> t_mapIndTexCoord;

	typedef std::pair<IRenderNode*,std::vector<struct TexCoord2Comp> > t_pairNodeTexCoord;
	typedef std::map<IRenderNode*,std::vector<struct TexCoord2Comp> > t_mapNodeTexCoord;

	typedef std::pair<IRenderNode*, t_pairEntityId > t_pairRenderNodeEntityIdPair;
	typedef	std::map<IRenderNode*, t_pairEntityId > t_mapRenderNodeLightIDs;
	typedef t_mapRenderNodeLightIDs::iterator				 t_mapRenderNodeLightIDsIterator;

	typedef std::pair<IRenderNode*, sOcclusionChannelMask >			t_pairRenderNodeOcclusionChannelMask;
	typedef	std::map<IRenderNode*, sOcclusionChannelMask >			t_mapRenderNodeOcclusionChannelMask;
	typedef t_mapRenderNodeOcclusionChannelMask::iterator				t_mapRenderNodeOcclusionChannelMaskIterator;

	CLightScene();
	~CLightScene();

	//+++++++++++++++++++++++++++++++++++++++++++++
	bool AddRLMesh(IRenderNode* pRenderNode,IRenderMesh* pRenderMesh,const bool bRasterize, const bool bOpaque, const f32 fLightmapQuality, const bool bOptimizeByLights, const bool bJustCreateMeshInfos, const bool bNeedToAutoGenerateTextureCoordinates,Matrix34& mNodeMatrix);
	bool CreateRLMeshes(IRenderNode* pRenderNode,CBaseObject* pObject, const bool bRasterize = true, const bool bOpaque = true, const f32 fLightmapQuality = 1, const bool bOptimizeByLights = true, const bool bJustCreateMeshInfos = false, const bool bNeedToAutoGenerateTextureCoordinates = false );
	void UpdateRLSurface(CRLSurface* rlSurface);
	void GenerateOcclusionMasks( IRenderNode* pRenderNode, bool bForceAllChannel = false );
	bool LoadSurfaceSpace(t_mmapRLMeshRange mmapMeshRange, CRLSurface** ppAllocRLSurface, FILE *pFile, const bool bNeedDebugInfo = false);
	bool AllocateSurfaceSpace(t_mmapRLMeshRange mmapMeshRange, CRLSurface** ppAllocRLSurface, FILE *pCacheFile, FILE *pFile, const bool bNeedDebugInfo, sOcclusionChannelMask* OCM, std::vector<CRLSurface*>* pSurfaceList );
	bool GenerateSurface(CRLSurface* pRLSurface, FILE * pCacheFile);
	void RescaleTexCoords(CRLSurface* pRLSurface);
	void AddTexCoords(int32 nOffsetX,int32 nOffsetY,int32 nSurfWidth,int32 nSurfHeight,CRLUnwrapGroup& group);
	//---------------------------------------------

	//+++++++++++++++++++++++++++++++++++++++++++++
	std::vector<CRLSurface*> m_rlSurfaces;
	t_mapIndTexCoord m_mapValidTexCoords; //valid tex coords for this mesh

	t_mapNodeTexCoord m_mapNodeTexCoord;
	//---------------------------------------------

	bool GenerateLightVolume(
		const IndoorBaseInterface &pInterface, 
		LMGenParam sParam,	
		std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, 
	struct ICompilerProgress *pIProgress, 
		bool &rErrorsOccured );

	bool GenerateFalseLightList(
		const IndoorBaseInterface &pInterface, 
		LMGenParam sParam,	
		std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, 
	struct ICompilerProgress *pIProgress, 
		const ELMMode Mode, 
		volatile SSharedLMEditorData* pSharedData, 
		const std::set<const CBaseObject*>& vSelectionIndices,
		bool &rErrorsOccured, const bool bShowTexelDensity );

	bool GenerateMaps(
		const IndoorBaseInterface &pInterface, 
		LMGenParam sParam,	
		std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, 
	struct ICompilerProgress *pIProgress, 
		const ELMMode Mode, 
		volatile SSharedLMEditorData* pSharedData, 
		const std::set<const CBaseObject*>& vSelectionIndices,
		bool &rErrorsOccured, const bool bShowTexelDensity );

	bool SetupLightmapQuality(
		const IndoorBaseInterface &pInterface, 
		LMGenParam sParam,	
		std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, 
	struct ICompilerProgress *pIProgress, 
		const ELMMode Mode, 
		volatile SSharedLMEditorData* pSharedData, 
		const std::set<const CBaseObject*>& vSelectionIndices,
		bool &rErrorsOccured);

	LMGenParam				m_sParam;

	// The assign queue system
	struct LMAssignQueueItem
	{
		IRenderNode *								pI_GLM;						//!
		std::vector<struct TexCoord2Comp>			vSortedTexCoords;			//!
		DWORD										m_dwHashValue;				//!< hashing value to detect changes in the data
		std::vector<std::pair<EntityId, EntityId> >	vOcclIDs;					//!< occlusion indices corresponding to the 0..4 colour channels
	};

	std::list<LMAssignQueueItem>					m_lstAssignQueue;			//!

	std::vector<CString>& GetLogInfo()
	{
		return m_LogInfo;
	} 

	std::multimap<unsigned int,string>& GetWarningInfo()
	{
		return m_WarningInfo;
	}

	const float ComputeHalvedLightmapQuality(const float fOldValue);

protected:
	void Init()
	{
		MEMORYSTATUS sStats;
		GlobalMemoryStatus(&sStats);
		m_uiStartMemoryConsumption = ((sStats.dwTotalVirtual - sStats.dwAvailVirtual) / 1024/1024);
	}
	const unsigned int GetUsedMemory()
	{
		MEMORYSTATUS sStats;
		GlobalMemoryStatus(&sStats);
		const unsigned int cuiCurrentTotal((sStats.dwTotalVirtual - sStats.dwAvailVirtual) / 1024/1024);
		if(cuiCurrentTotal <= m_uiStartMemoryConsumption)
			return 0;
		return (cuiCurrentTotal - m_uiStartMemoryConsumption);
	};
	void DumpSysMemStats()
	{
		_TRACE(m_LogInfo, true, "System memory usage: %iMB\r\n", GetUsedMemory());
	};
	void DisplayMemStats(volatile SSharedLMEditorData *pSharedData)
	{
		if(pSharedData != NULL)
		{
			::SendMessage( (HWND)pSharedData->hwnd, pSharedData->uiMemUsageMessage, min((int)GetUsedMemory(),1000), 0 );//update progress bar
			::SendMessage( (HWND)pSharedData->hwnd, pSharedData->uiMemUsageStatic, min((int)GetUsedMemory(),1000), 0 );//update progress bar static element
			MSG msg;
			while( FALSE != ::PeekMessage( &msg, 0, 0, 0, PM_REMOVE ) )
			{ 
				::TranslateMessage( &msg );
				::DispatchMessage( &msg );
			}
		}
		else
			DumpSysMemStats();
	}

	void	SetupLights();																	//!< Setup the light list
	bool	SetupBrushes(const bool bOptimizeByLights, const bool bUseLightmapQuality, 
		std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes,
		const std::set<const CBaseObject*>& vSelectionIndices, const ELMMode Mode,const IVisArea *pVisArea = NULL, IRenderNode** pLastRenderNode = NULL, int32 nVisAreaID = 0, int32 nVisAreaNumber = 0 );	//!< Setup and unwrap the brushes
	void	GenerateLightListPerRenderNode(  std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes,bool bAddOccluders );							//!< Generate a light list per rendernode (rendering speedup & needed for occlusion mapping )
	bool	SubSurfacePacker(FILE *pSurfaceOutputFile,FILE *pCacheFile,IRenderNode* pRenderNode,int32 SubObjIdx,IRenderMesh* pRenderMesh,IVisArea *pVisArea);
	bool	SurfacePacker( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, IVisArea *pVisArea );																//!< Generate the lightmap / occlusionmap surfaces
	int32 GetNeededTriangles(IRenderNode* pRenderNode,CBaseObject* pObject, const bool bRasterize = true, const bool bOpaque = true, const bool bOptimizeByLights = true );	//!< Give back how many triangles included into octree (just the lightmapped ones)
	void	ReleaseMeshes();																																																									//!< Release the memory used by meshes (don't release meshes witch attached to surface)
	void	ReleaseAllMeshesDirectly();																																																				//!< Release the memory used by meshes

	void	AddSubOccluders(IRenderMesh* pRenderMesh, std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, const Vec3& vLightSpherePosition, const float fLightSphereRadius, const bool bHandleLocalLight, IVisArea* pLightVisArea, const bool bCalculateTriNumber, uint8 *pOccluderBitMask,int nBrushID,IRenderNode *pIEtyRend,Matrix34& mNodeMatrix );
	void	AddOccluders( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, const Vec3& vLightSpherePosition, const float fLightSphereRadius, const bool bHandleLocalLight, IVisArea* pLightVisArea, const bool bCalculateTriNumber, uint8 *pOccluderBitMask );
	void	SortVisAreas( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes );																											//!< Try to minimalize the distances between the vis areas

	bool	DoRenderingSequential( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, IVisArea *pVisArea, const bool bShowTexelDensity, const int32 nVisAreaID );
	bool	DoRenderingParalell( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, IVisArea *pVisArea, const bool bShowTexelDensity, const int32 nVisAreaID );
	void  Rought_AttachOcclusionLightToList( t_pairEntityId &LightIDs, IRenderNode *pRenderNode ) const;

	void	CreateLightList( t_pairEntityId &LightIDs, IRenderNode *pRenderNode );																														///! Give back the same light list as the 3dEngine
	void	CheckExactLightObjInteraction( t_pairEntityId &LightIDs, IRenderNode *pRenderNode, CBaseObject* pObject, t_pairEntityId &FalseLightIDs );								///! Create a false light list (list of lights, which have a crosssection with the object bbox, but not realy affect the object itself)

	bool	GenerateSpartialListForOccluders( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes );			//!< Generate a per vNodeID based list from the object based on the positions of them - help to speedup the occlusion selection
	bool	CLightScene::SubParameterizer(IRenderNode* pRenderNode,IRenderMesh* pRenderMesh);
	bool	Parametrizer( std::vector<std::pair<IRenderNode*, CBaseObject*> >& vNodes, IVisArea *pVisArea );
private:
	std::multimap<unsigned int,string>		m_WarningInfo;					//!< will contain warning summary for logfile
	std::vector<CString>									m_LogInfo;						//!< will contain some logging info

	struct ILMSerializationManager				*m_pISerializationManager;
	int32																	m_nTriNumber;									//!< number of triangles in the octree
	unsigned int													*m_puiIndicator;							//!< index of each pixel whether a value has been set or not
	float																	*m_pfColours;								//!< colours
	unsigned int													m_uiSampleCount;							//!< subsample count for one single texel

	unsigned int													m_uiStartMemoryConsumption;
	IndoorBaseInterface										m_IndInterface;

	t_mmapGeomMeshes											m_mmapGeomMeshesTemporaly;
	t_mmapRLMeshes												m_mmapRLMeshes;
	t_mmapGeomMeshes											m_mmapGeomMeshes;
	t_mapRenderNodeLightIDs								m_OcclusionLightPairs;
	t_mapRenderNodeLightIDs								m_LightmapLightPairs;
	t_mapRenderNodeOcclusionChannelMask		m_OcclusionChannelMaskMap;
	std::vector<uint8>										m_BrushRenderable;
	std::vector<uint8>										m_BrushOcclusion;
	CIlluminanceIntegrator*								pDirectIllumStatement;
	CIlluminanceIntegrator*								pIndirectIllumStatement;
	CIlluminanceIntegrator*								pSolarIllumStatement;
	CIlluminanceIntegrator*								pOcclusionIllumStatement;
	CIlluminanceIntegrator*								pAmbientOcclusionIllumStatement;


	//Spartial list
	uint32**															m_pOccluderGrid;											//!< The list of the occluders
	uint32*																m_pOccluderGridCellSize;							//!< The size of a cell
	AABB																	m_AABoxOfScene;												//!< The size of the scene
	int32																	m_nOccluderGridWith;									//!< The size of the grid in X
	int32																	m_nOccluderGridHeight;								//!< The size of the grid in Y
	int32																	m_nOccluderGridDeepness;							//!< The size of the grid in Z
	Vec3																	m_vOccluderCellSize;									//!< The size of a grid cell
};//CLightScene


#endif 