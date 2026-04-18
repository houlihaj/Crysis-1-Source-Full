/*=============================================================================
  RLSurface.h : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#ifndef __RLSURFACE_H__
#define __RLSURFACE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "RLUnwrapGroup.h"
#include "RLMesh.h"

#define	COLOR_EPSILON	( 0.00001f )


/////////////////////////////////////////////////////////////////////////////////////////////////////
// sOcclusionChannelMask
//
// Neccessary info for the occlusion maps per brush
/////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct 
{
	int8		m_FirstChannel;
	int8		m_ActiveChannelNumber;
	int8		m_ChannelMap[NUM_COMPONENTS*4];
} sOcclusionChannelMask;

/////////////////////////////////////////////////////////////////////////////////////////////////////
// SurfaceData
//
// All information about all the stored data
// RAE: if occlusionmap or LM are enabled useing all the 16 channel if not useing only 4 channel
// Occlusion maps & LM: useing 16 channel
// The system search an options where all the information can be stored:
// if LM enabled: using all channels for every group
// if not & Occlusion is enabled: using all occlusion-light number or if RAE enabled - at least 1 channel.
/////////////////////////////////////////////////////////////////////////////////////////////////////
struct  SurfaceData {
	int32	X,Y;
	sOcclusionChannelMask Mask;
};


typedef std::pair<CRLUnwrapGroup*, SurfaceData > t_pairSurfaceGroup;

class CFreeSpaceSpanBuffer;
class CFreeSpaceBuffer;

class CRLSurface
{

public:
	CRLSurface(int32 nWidth,int32 nHeight);																			/// Constructors
	virtual ~CRLSurface();																											/// Destructor

	//managing of allocations
	bool Init_Allocations();																										/// Reserve all necessary memory for allocations
	void BeginAlloc();																													/// Create a backup of the allocations
	void RevertAlloc();																													/// Revert the allocation to backup position (Need to use BeginAlloc() before!)
	bool AllocateGroup(CRLUnwrapGroup& upwrapGroup, sOcclusionChannelMask& OCM);				/// Try to include a group into surface
	void AddGroup(CRLUnwrapGroup& upwrapGroup, int32 nX, int32 nY, int32 nDirection );	/// Put the group into surface to the given offset (nX,nY)

	//Group management
	bool IsGroupInsertable( const int32 nGroupBiggestWidth );							/// Fast check : is there any chance to allocate engouht space in the surface for the group
	bool AllocateMemory();																											/// Allocate all the surface memory (diffuse and occlusion maps)
	bool CopyGroup(int32 nOffsetX,int32 nOffsetY,CRLUnwrapGroup& upwrapGroup, FILE * pCacheFile, sOcclusionChannelMask& OCM); /// Copy a group into surface at the given offset
	void AddObjectId(std::pair<int32,int32> nObjectId);																					/// Store the object id
	void AddRenderNodePtr(IRenderNode* pRenderNode);														/// Store the RenderNode pointer
	int32	GetFirstActiveChannel();																							/// Give back the first active channel
	int32	GetLastActiveChannel() const { return m_nLastActiveChannel; }					/// Give back the last active channel

	void ResetMeshBuffer()																											/// Restart the mesh buffer (reset per mesh!)
	{
		m_OneMeshRenderableGroups.resize(0);
	}

	void AttachMesh( CRLMesh *pMesh )																						/// Attach a mesh to the surface - mesh lifetime management
	{
		m_AttachedMeshes.push_back( pMesh );
	}

	void ReleaseAttachedMeshes();																								/// After allocation the RLSufrace handle the CRLmeshes lifetime...

	uint8* GetDiffuseLightmap() const																						/// Give back the pointer of the lightmap datas
	{
		return m_pDiffuseLightmap;
	}

	uint8* GetRAE() const																												/// Give back the pointer of the RAE datas
	{
		return m_pRAE;
	}

	void SaveDistributedMap( const int32 nID ) const;														/// Save the distributedmap to a file

	uint8* GetOcclusionMap() const																							/// Give back the pointer of the occlusion map datas
	{
		return m_pOcclusionMap;
	}

	void	SetLastUsedOcclusionChannel( int32 nONum )
	{
		m_nMaxUsedOcclusionChannel = m_nMaxUsedOcclusionChannel > nONum ? m_nMaxUsedOcclusionChannel : nONum;
	}

	void	ReleaseSpanBuffer();																										/// Release the used span buffer memory
	void	ReleaseBuffers();																												/// Release all big buffers

	//Debug functions
	void WriteRaster();																													/// Generate tga files from lightmap and occlusion map for help bugfixes
	void GenerateDebugJPG();																										/// Generate a debug JPG for help the visualisation the span buffer
	void GenerateDebugDatas();																									/// Generate random colors into the buffers, help debug the save functions

	bool  IsFinished() const
	{
		return m_bFinished;
	}
	void	Finished();
	void	LazyUpdates();

public:
	bool																m_bFinished;																/// Can I add new groups to surface ?
	int32																m_nWidth, m_nHeight;												/// Surface width & height
	int32																m_nComponentWidth, m_nComponentHeight;			/// Surface sizes / Component number
	int32																m_nMaxUsedOcclusionChannel;									/// How much occlusion map channel used
	int32																m_nMaxUsedOcclusionChannel_Backup;					/// The backup version of the m_nMaxUsedOcclusionChannel

	std::vector<std::pair<int32,int32>>	m_ObjectIds;								/// The ids and indices of the stored (sub)objects
	std::vector<IRenderNode*>						m_arrRenderNodes;														/// The RenderNodes of the stored objects

	std::vector<t_pairSurfaceGroup> 		m_RenderableGroups;													/// The stored groups (all)
	std::vector<t_pairSurfaceGroup> 		m_OneMeshRenderableGroups;									/// The stored groups from the last mesh

	int																	m_pOcclusionChannelMapping[NUM_COMPONENTS*4];	/// The mapping between the calculated and the real datas

private:
	std::vector<CRLMesh*>								m_AttachedMeshes;														/// The array of attached (have allocated space) meshes - needed for lifetime management
	std::vector<t_pairSurfaceGroup> 		m_RenderableGroupsBackup;										/// Backup of the renderablegroups vector
	std::vector<t_pairSurfaceGroup> 		m_OneMeshRenderableGroupsBackup;						/// Backup of the renderablegroups of the last mesh vector
	CFreeSpaceSpanBuffer*								m_FreeSpaceAllocator;												/// The span buffer of the surface
	uint8*															m_pDiffuseLightmap;													/// The lightmap datas
	uint8*															m_pRAE;																			/// The RAE reflection map datas
	uint8*															m_pOcclusionMap;														/// The occlusion map datas
	uint8*															m_pDistributedMap;													/// The data map for distributed compiling (contain the ( world pos + normal ) * Supersampling number + OctreeID )
	int32																m_nChannelNumber;														/// The used channel number
	int32																m_nLastActiveChannel;												/// The last channel, which have SpanBuffer
};

#endif
