/*=============================================================================
  RLSurface.cpp : 
  Copyright 2004 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Nick Kasyan
=============================================================================*/
#include "stdafx.h"

#include "RLSurface.h"
#include "BitmapDilation.h"
#include "SpanBuffer.h"
#include "FreeSpaceSpanBuffer.h"

//===================================================================================
// Constructor
//===================================================================================
CRLSurface::CRLSurface(int32 nWidth,int32 nHeight)
{
		m_nLastActiveChannel = 0;
		m_nMaxUsedOcclusionChannel_Backup = m_nMaxUsedOcclusionChannel = 0;
		m_bFinished = false;
		m_nWidth = nWidth;
		m_nHeight = nHeight;

		CSceneContext& sceneCtx = CSceneContext::GetInstance();
		if (sceneCtx.m_numComponents == 16)
		{
			m_nComponentWidth = nWidth / 2;
			m_nComponentHeight = nHeight / 2;
		}
		else
		{
			m_nComponentWidth = nWidth;
			m_nComponentHeight = nHeight;
		}

		m_pDistributedMap = NULL;
		m_pRAE = NULL;
		m_pOcclusionMap = NULL;
		m_pDiffuseLightmap = NULL;

		Init_Allocations( );
}

//===================================================================================
// Destructor
//===================================================================================
CRLSurface::~CRLSurface()	
{
	ReleaseBuffers();
	ReleaseSpanBuffer();
};

//===================================================================================
// Method				:	Init_Alloactions()
// Description	:	Reserve all necessary memory for allocations
//===================================================================================
bool CRLSurface::Init_Allocations()
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	m_FreeSpaceAllocator = new CFreeSpaceSpanBuffer( m_nComponentWidth, m_nComponentHeight, sceneCtx.m_numComponents );
	m_nLastActiveChannel = sceneCtx.m_nSlidingChannelNumber;
	return  true;
}

//===================================================================================
// Method				:	GetFirstActiveChannel
// Description	:	Give back the last active channel
//===================================================================================
int32	 CRLSurface::GetFirstActiveChannel()
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	return ( m_nLastActiveChannel > sceneCtx.m_nSlidingChannelNumber ) ? m_nLastActiveChannel - sceneCtx.m_nSlidingChannelNumber : 0;
}




//===================================================================================
// Method				:	BeginAlloc
// Description	:	Create a backup of the allocations
//===================================================================================
void CRLSurface::BeginAlloc()
{
	m_FreeSpaceAllocator->BeginAllocations();
	m_RenderableGroupsBackup.assign(m_RenderableGroups.begin(), m_RenderableGroups.end());
	m_OneMeshRenderableGroupsBackup.assign(m_OneMeshRenderableGroups.begin(), m_OneMeshRenderableGroups.end());
	m_nMaxUsedOcclusionChannel_Backup = m_nMaxUsedOcclusionChannel;
}


//===================================================================================
// Method				:	RevertAlloc
// Description	:	Revert the allocation to backup position
//===================================================================================
void CRLSurface::RevertAlloc()
{
	m_FreeSpaceAllocator->RevertAllocations();
	m_RenderableGroups.assign(m_RenderableGroupsBackup.begin(), m_RenderableGroupsBackup.end());
	m_OneMeshRenderableGroups.assign(m_OneMeshRenderableGroupsBackup.begin(), m_OneMeshRenderableGroupsBackup.end());
	m_nMaxUsedOcclusionChannel = m_nMaxUsedOcclusionChannel_Backup;
}

//===================================================================================
// Method				:	AddGroup
// Description	:	Put the group into surface to the given offset (nX,nY)
//===================================================================================
void CRLSurface::AddGroup(CRLUnwrapGroup& upwrapGroup, int32 nX, int32 nY, int32 nDirection )
{
/* BROKEN CODE! - mask not working...
	assert( upwrapGroup.m_pSpanBuffer[0] );

	CSpanBuffer *pSpanBuffer;
	pSpanBuffer = upwrapGroup.m_pSpanBuffer[nDirection];

	upwrapGroup.m_SpanDirection = (eSpanDirections)nDirection;
	//save the offset and the group for a later time..
	t_pairSurfaceGroup SurfaceGroup;
	SurfaceGroup.first = &upwrapGroup;
	SurfaceOffset *pOffset = &SurfaceGroup.second;
	pOffset->X = nX;
	pOffset->Y = nY;

	m_RenderableGroups.push_back( SurfaceGroup );
	m_OneMeshRenderableGroups.push_back( SurfaceGroup );

	//insert into the span lists
	for( int i = 0; i < NUM_COMPONENTS*4; ++i )
		m_FreeSpaceAllocator[i]->Insert( *pSpanBuffer, nX,nY );
*/
}


void CRLSurface::LazyUpdates()
{
	if( m_FreeSpaceAllocator )
		m_FreeSpaceAllocator->RegenerateBiggestSpanList();
}

//===================================================================================
// Method				:	AllocateGroup
// Description	:	Try to include a group into surface
//===================================================================================
bool CRLSurface::AllocateGroup(CRLUnwrapGroup& upwrapGroup, sOcclusionChannelMask& OCM)
{
	assert( upwrapGroup.m_pPixelMask );
	assert( upwrapGroup.m_pSpanBuffer[0] );

	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	//check that...
	if( OCM.m_FirstChannel + OCM.m_ActiveChannelNumber > sceneCtx.m_numComponents )
		return false;

	//get the span buffer...
	int32 nDirections;
	CSpanBuffer *pSpanBuffer;

	//is there any chance to pack this group into this surface ?
	for( nDirections = 0; nDirections < SD_MAX; ++nDirections )
	{
		pSpanBuffer = upwrapGroup.m_pSpanBuffer[nDirections];
		if( m_FreeSpaceAllocator->IsInsertable_EarlyCheck( *pSpanBuffer ) ) 
			break;
	}

	if( nDirections == SD_MAX )
		return false;

	int32 X,Y;
	for( Y = 0; Y < m_nComponentHeight; ++Y )			//try the "used, dirty" place...
	{
		for( X = 0; X < m_nComponentWidth; )
		{
				int32 nDifference = m_nComponentWidth;

				//try all directions...
				for( nDirections = 0; nDirections < SD_MAX; ++nDirections )
				{
					eSpanDirections SpanDirection = upwrapGroup.m_DefaultSpanDirectionOrder[ nDirections ];
					pSpanBuffer = upwrapGroup.m_pSpanBuffer[ SpanDirection ];

					int32 nDirectionDifference = m_FreeSpaceAllocator->IsInsertable( *pSpanBuffer, X, Y, OCM.m_FirstChannel, OCM.m_ActiveChannelNumber );

					//this is an good position
					if( 0 == nDirectionDifference )
					{
						upwrapGroup.m_SpanDirection = SpanDirection;

						//save the offset and the group for a later time..
						t_pairSurfaceGroup SurfaceGroup;
						SurfaceGroup.first = &upwrapGroup;
						SurfaceData *pOffset = &SurfaceGroup.second;
						pOffset->X = X;
						pOffset->Y = Y;
						pOffset->Mask = OCM;

  					m_RenderableGroups.push_back( SurfaceGroup );
  					m_OneMeshRenderableGroups.push_back( SurfaceGroup );

  					//insert into the span lists
						m_FreeSpaceAllocator->Insert( *pSpanBuffer, X,Y, OCM.m_FirstChannel, OCM.m_ActiveChannelNumber );
  					return true;
  				}//nDifference == 0
  				nDifference = ( nDirectionDifference < nDifference ) ? nDirectionDifference : nDifference;
  			}
 				X += nDifference;
 		}//Span list
	}//for y

	return false;
}


//===================================================================================
// Method				:	IsGroupInsertable
// Description	:	Fast check : is there any chance to allocate engouht space in the surface for the group
//===================================================================================
bool CRLSurface::IsGroupInsertable( const int32 nGroupBiggestWidth )
{ 
	return ( nGroupBiggestWidth < m_FreeSpaceAllocator->GetBiggestFreeSpan() );
}


//===================================================================================
// Method				:	AllocateMemory
// Description	:	Allocate all the surface memory (diffuse and occlusion maps)
//===================================================================================
bool CRLSurface::AllocateMemory()
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	if( sceneCtx.m_bDistributedMap )
	{
/*		m_pDistributedMap = new uint8[m_nWidth*m_nHeight*sceneCtx.m_nDistributedBlockSize];
		assert(m_pDistributedMap != NULL);
		if( NULL == m_pDistributedMap ) 
			return false;
		memset (m_pDistributedMap, 0, sizeof (uint8)*m_nWidth*m_nHeight*sceneCtx.m_nDistributedBlockSize);
*/
		//No other allowed
		return true;
	}

	//if no lightmap, just occlusion map we can create a smaller surface, if we don't use all the surfaces...
	if( ( sceneCtx.m_bMakeOcclMap || sceneCtx.m_bMakeRAE ) && false == sceneCtx.m_bMakeLightMap && sceneCtx.m_numComponents == 16 )
	{
		if( m_nMaxUsedOcclusionChannel <= 4 )
		{
			m_nWidth = m_nComponentWidth;
			m_nHeight = m_nComponentHeight;
			m_nChannelNumber = 1;
		}
		else
			if( m_nMaxUsedOcclusionChannel <= 8 )
			{
				m_nHeight = m_nComponentHeight;
				m_nChannelNumber = 2;
			}
			else
				m_nChannelNumber = ( m_nMaxUsedOcclusionChannel <= 12 ) ? 3 : 4;
	}
	else
		m_nChannelNumber = (sceneCtx.m_numComponents+3)/4;

	if( sceneCtx.m_bMakeLightMap )
	{
		//fix::allocate this memory later.
		m_pDiffuseLightmap = new uint8[4*m_nWidth*m_nHeight];
		assert(m_pDiffuseLightmap != NULL);
		if( NULL == m_pDiffuseLightmap ) 
			return false;
		memset (m_pDiffuseLightmap, 0, sizeof (uint8)*m_nWidth*m_nHeight*4);
	}

	if( sceneCtx.m_bMakeOcclMap )
	{
		//fix::allocate this memory later.
		m_pOcclusionMap = new uint8[4*m_nWidth*m_nHeight];
		assert(m_pOcclusionMap != NULL);
		if( NULL == m_pOcclusionMap ) 
			return false;
		memset (m_pOcclusionMap, 0, sizeof (uint8)*m_nWidth*m_nHeight*4);
	}

	if( sceneCtx.m_bMakeRAE )
	{
		//fix::allocate this memory later.
		m_pRAE = new uint8[4*m_nWidth*m_nHeight];
		assert(m_pRAE!= NULL);
		if( NULL == m_pRAE ) 
			return false;
		memset (m_pRAE, 0, sizeof (uint8)*m_nWidth*m_nHeight*4);
	}

	return true;
}


//===================================================================================
// Method				:	CopyGroup
// Description	:	Copy a group into surface at the given offset
//===================================================================================
bool CRLSurface::CopyGroup(int32 nOffsetX,int32 nOffsetY, CRLUnwrapGroup& upwrapGroup, FILE * pCacheFile, sOcclusionChannelMask& OCM )
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	int32 nCompOffsetY,nCompOffsetX;
	int32 nWidth = upwrapGroup.iWidth;
	int32 nHeight = upwrapGroup.iHeight;

	//read back the rasterized datas....
	upwrapGroup.Load( pCacheFile );

	if( sceneCtx.m_bDistributedMap )
	{	
/*		if( NULL == m_pDistributedMap )
		{
			assert( m_pDistributedMap != NULL );
			return false;
		}
		int32 nComponentOffset = m_nWidth * nOffsetY + nOffsetX;


		for(int32 Y = 0; Y < nHeight; Y++)
			for(int32 X = 0; X < nWidth; X++)
			{
				int32 nIndSrc = (nWidth * Y + X);
				int32 nIndDst = upwrapGroup.GenerateIndexFromCoordiatesToSurface( upwrapGroup.m_SpanDirection, X, Y, m_nWidth ) + nComponentOffset;
				assert(nIndDst<(m_nHeight*m_nWidth));

				memcpy( &m_pDistributedMap[nIndDst*sceneCtx.m_nDistributedBlockSize], &upwrapGroup.m_pDistributedMap[nIndSrc*sceneCtx.m_nDistributedBlockSize], sizeof(uint8)*sceneCtx.m_nDistributedBlockSize);
			}//X*/
	}
	else
	{
		if( sceneCtx.m_bMakeLightMap && NULL == m_pDiffuseLightmap )
		{
				assert(m_pDiffuseLightmap != NULL);
				return false;
		}
		if( sceneCtx.m_bMakeOcclMap && NULL == m_pOcclusionMap )
		{
				assert(m_pOcclusionMap != NULL);
				return false;
		}

		if( sceneCtx.m_bMakeRAE && NULL == m_pRAE )
		{
			assert(m_pRAE != NULL);
			return false;
		}

		//static format for lightmaps
		if( sceneCtx.m_bMakeLightMap )
		{
			for (int32 nComp =0; nComp<m_nChannelNumber; nComp++)
			{
				if (m_nChannelNumber < 5)
				{
					nCompOffsetY = nOffsetY + (m_nComponentHeight * (nComp/2));
					nCompOffsetX = nOffsetX + (m_nComponentWidth * (nComp%2));
				}
				else
				{
					//problematic case...
					nCompOffsetY = nOffsetY;
					nCompOffsetX = nOffsetX;
				}
				int32 nComponentOffset = m_nWidth * nCompOffsetY + nCompOffsetX;

				for(int32 Y = 0; Y < nHeight; Y++)
				{
					for(int32 X = 0; X < nWidth; X++)
					{
						int32 nIndSrc = (nWidth * Y + X);
						int32 nIndDst = upwrapGroup.GenerateIndexFromCoordiatesToSurface( upwrapGroup.m_SpanDirection, X, Y, m_nWidth ) + nComponentOffset;
						assert(nIndDst<(m_nHeight*m_nWidth));

						if( upwrapGroup.m_pPixelMask->IsValid( nIndSrc ) )
						{
							nIndSrc *= 4;
							nIndDst *= 4;
							m_pDiffuseLightmap[nIndDst+0] =  upwrapGroup.ppRastImage[nComp][nIndSrc+0];
							m_pDiffuseLightmap[nIndDst+1] =  upwrapGroup.ppRastImage[nComp][nIndSrc+1];
							m_pDiffuseLightmap[nIndDst+2] =  upwrapGroup.ppRastImage[nComp][nIndSrc+2];
							m_pDiffuseLightmap[nIndDst+3] =  upwrapGroup.ppRastImage[nComp][nIndSrc+3];
						}//isValid
					}//X
				}//Y
			}//nComp
		}//bLightmap

		//dynamic channels for occlusion maps
		if( sceneCtx.m_bMakeOcclMap )
		{
			for ( int32 nComp = 0; nComp<OCM.m_ActiveChannelNumber; ++nComp )
			{
				int32 nDestComp = OCM.m_FirstChannel+nComp;
				assert( nDestComp < NUM_COMPONENTS*4 );

				if (m_nChannelNumber < 5)
				{
					nCompOffsetY = nOffsetY + (m_nComponentHeight * ((nDestComp/4)/2));
					nCompOffsetX = nOffsetX + (m_nComponentWidth * ((nDestComp/4)%2));
				}
				else
				{
					//problematic case...
					nCompOffsetY = nOffsetY;
					nCompOffsetX = nOffsetX;
				}
				int32 nComponentOffset = m_nWidth * nCompOffsetY + nCompOffsetX;
				int32 nDestByteOffset;

				//file format specific!
				switch( nDestComp % 4 )
				{
					case 0:
						nDestByteOffset = 2;
						break;
					case 1:
						nDestByteOffset = 1;
						break;
					case 2:
						nDestByteOffset = 0;
						break;
					case 3:
						nDestByteOffset = 3;
						break;
				}

				int32 nSrcChannel = nComp / 4;
				int32 nSrcByteOffset = nComp % 4;

				for(int32 Y = 0; Y < nHeight; Y++)
				{
					for(int32 X = 0; X < nWidth; X++)
					{
						int32 nIndSrc = (nWidth * Y + X);
						int32 nIndDst = upwrapGroup.GenerateIndexFromCoordiatesToSurface( upwrapGroup.m_SpanDirection, X, Y, m_nWidth ) + nComponentOffset;
						assert(nIndDst<(m_nHeight*m_nWidth));

						if( upwrapGroup.m_pPixelMask->IsValid( nIndSrc ) )
							m_pOcclusionMap[nIndDst*4+nDestByteOffset] =  upwrapGroup.m_pOcclusionMap[nSrcChannel][nIndSrc*4+nSrcByteOffset];
					}//X
				}//Y
			}//nComp
		}//bOcclusionmap

		if( sceneCtx.m_bMakeRAE )
		{
			assert(upwrapGroup.m_pPixelMask);
			for ( int32 nComp = 0; nComp<sceneCtx.m_numRAEComponents; ++nComp )
			{
				int32 nDestComp = OCM.m_FirstChannel+nComp;
				assert( nDestComp < NUM_COMPONENTS*4 );

				if (m_nChannelNumber < 5)
				{
					nCompOffsetY = nOffsetY + (m_nComponentHeight * ((nDestComp/4)/2));
					nCompOffsetX = nOffsetX + (m_nComponentWidth * ((nDestComp/4)%2));
				}
				else
				{
					//problematic case...
					nCompOffsetY = nOffsetY;
					nCompOffsetX = nOffsetX;
				}
				int32 nComponentOffset = m_nWidth * nCompOffsetY + nCompOffsetX;
				int32 nDestByteOffset = 0;

				//file format specific!
				switch( nDestComp % 4 )
				{
				case 0:
					nDestByteOffset = 0;//dunno what this should do or why it was 2, but it definitively caused bad results 
					break;
				case 1:
					nDestByteOffset = 1;
					break;
				case 2:
					nDestByteOffset = 0;
					break;
				case 3:
					nDestByteOffset = 3;
					break;
				}

				int32 nSrcByteOffset = nComp % 4;

				for(int32 Y = 0; Y < nHeight; Y++)
				{
					for(int32 X = 0; X < nWidth; X++)
					{
						int32 nIndSrc = (nWidth * Y + X);
						int32 nIndDst = upwrapGroup.GenerateIndexFromCoordiatesToSurface( upwrapGroup.m_SpanDirection, X, Y, m_nWidth ) + nComponentOffset;
						assert(nIndDst<(m_nHeight*m_nWidth));

						if( upwrapGroup.m_pPixelMask->IsValid( nIndSrc ) )
						{
							m_pRAE[nIndDst*4+nDestByteOffset+0] =  upwrapGroup.m_pRAE[nIndSrc*4+nSrcByteOffset+0];
							m_pRAE[nIndDst*4+nDestByteOffset+1] =  upwrapGroup.m_pRAE[nIndSrc*4+nSrcByteOffset+1];
							m_pRAE[nIndDst*4+nDestByteOffset+2] =  upwrapGroup.m_pRAE[nIndSrc*4+nSrcByteOffset+2];
							m_pRAE[nIndDst*4+nDestByteOffset+3] =  upwrapGroup.m_pRAE[nIndSrc*4+nSrcByteOffset+3];
						}
					}//X
				}//Y
			}//nComp
		}//bRAE
	}

	//release the group memory again...
//	upwrapGroup.ClearBuffers();

	if(!sceneCtx.m_bDistributedMap)
		upwrapGroup.ReleaseItemArray();

	return true;
}


//===================================================================================
// Method				:	AddObjectId
// Description	:	Store the object id
//===================================================================================
void CRLSurface::AddObjectId(std::pair<int32,int32> nObjectId)
{
	m_ObjectIds.push_back(nObjectId);
	
}


//===================================================================================
// Method				:	AddRenderNodePtr
// Description	:	Store the RenderNode pointer
//===================================================================================
void CRLSurface::AddRenderNodePtr(IRenderNode* pRenderNode)
{
	m_arrRenderNodes.push_back(pRenderNode);

}


//===================================================================================
// Method				:	GenerateDebugJPG
// Description	:	Generate a debug JPG for help the visualization the span buffer
//===================================================================================
void CRLSurface::GenerateDebugJPG()
{
	if( m_bFinished )
		return;
//	m_FreeSpaceAllocator[GetFirstActiveChannel()]->GenerateDebugJPG();
	m_FreeSpaceAllocator->GenerateDebugJPG();
}


//===================================================================================
// Method				:	GenerateDebugDatas
// Description	:	Generate random colors into the buffers, help debug the save functions
//===================================================================================
void CRLSurface::GenerateDebugDatas()
{
	int32 x,y;
	int32 nIndex;

	nIndex = 0;
	if(m_pDiffuseLightmap)
		for( y = 0; y < m_nHeight; ++y )
			for( x = 0; x < m_nWidth; ++x )
			{
				m_pDiffuseLightmap[nIndex++] = rand() & 255;
				m_pDiffuseLightmap[nIndex++] = rand() & 255;
				m_pDiffuseLightmap[nIndex++] = rand() & 255;
				m_pDiffuseLightmap[nIndex++] = 255;
			}

	nIndex = 0;
	if(m_pOcclusionMap)
		for( y = 0; y < m_nHeight; ++y )
			for( x = 0; x < m_nWidth; ++x )
			{
				m_pOcclusionMap[nIndex++] = rand() & 255;
				m_pOcclusionMap[nIndex++] = rand() & 255;
				m_pOcclusionMap[nIndex++] = rand() & 255;
				m_pOcclusionMap[nIndex++] = 255;
			}
}

//===================================================================================
// Method				:	WriteRaster
// Description	:	Generate tga files from lightmap and occlusion map for help bugfixes
//===================================================================================
void CRLSurface::WriteRaster()
{
	char szFile[128];

	if(m_pDiffuseLightmap)
	{
		sprintf(szFile, "Rast%i.tga",(uint32) (this));
		GetIEditor()->GetRenderer()->WriteTGA(m_pDiffuseLightmap, m_nWidth, m_nHeight, szFile,32,32);
	}
	if(m_pOcclusionMap)
	{
		sprintf(szFile, "Rast_occl_%i.tga",(uint32) (this));
		GetIEditor()->GetRenderer()->WriteTGA(m_pOcclusionMap, m_nWidth, m_nHeight, szFile,32,32);
	}
}

//===================================================================================
// Method				:	ReleaseSpanBuffer
// Description	:	Release the used span buffer memory
//===================================================================================
void	CRLSurface::ReleaseSpanBuffer()
{
		SAFE_DELETE( m_FreeSpaceAllocator );
}

//===================================================================================
// Method				:	ReleaseBuffers
// Description	:	Release the buffers
//===================================================================================
void CRLSurface::ReleaseBuffers()
{
	SAFE_DELETE_ARRAY(m_pRAE);
	SAFE_DELETE_ARRAY(m_pOcclusionMap);
	SAFE_DELETE_ARRAY(m_pDiffuseLightmap);
	SAFE_DELETE_ARRAY(m_pDistributedMap);
}

//===================================================================================
// Method				:	Finished
// Description	:	Finalizing the surface, after that you can't add any new mesh into the surface, but it will use much less memory.
//===================================================================================
void	CRLSurface::Finished()
{
	if( m_bFinished )
			return;
	m_bFinished = true;
	SAFE_DELETE( m_FreeSpaceAllocator );
}


//===================================================================================
// Method				:	ReleaseAttachedMeshes
// Description	:	After allocation the RLSufrace handle the CRLMeshes lifetime...
//===================================================================================
void	CRLSurface::ReleaseAttachedMeshes()
{
	return;
	int nSize= m_AttachedMeshes.size();
	for( int i = 0; i < nSize; ++i )
		SAFE_DELETE( m_AttachedMeshes[i] );

	m_AttachedMeshes.clear();
}


#define DISTRIBUTED_MAP_VERSION	1

void CRLSurface::SaveDistributedMap( const int32 nID ) const
{
//	if( NULL == m_pDistributedMap )
//		return;

	char szFileName[64];
	sprintf( szFileName, "%d.DM", nID );
	FILE* f = fopen( szFileName, "wb"	);
	assert( f );
	if( NULL == f)
		return;

	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	uint8 nChunk = 1;
	fwrite( &nChunk, sizeof(uint8), 1, f);

	uint32 nData = DISTRIBUTED_MAP_VERSION;
	fwrite( &nData, sizeof(uint32), 1, f);

	fwrite( &m_nHeight, sizeof(int32), 1, f);
	fwrite( &m_nWidth, sizeof(int32), 1, f);

	uint32 nSampleNumber = sceneCtx.m_bUseSuperSampling ? (5+sceneCtx.m_numJitterSamples*sceneCtx.m_numJitterSamples) : 0;
	fwrite( &nSampleNumber, sizeof(uint32), 1, f);

	uint32 nBlockSize = sceneCtx.m_nDistributedBlockSize;
	fwrite( &nBlockSize, sizeof(uint32), 1, f);

#ifdef DISTRIBUTED_OCCLUSIONMAP
	fwrite( &m_nMaxUsedOcclusionChannel, sizeof(int32), 1, f	);
#endif


	//iterate over the unwrapgroups attached to the surface, and copy the patches to the surface
	std::vector<t_pairSurfaceGroup>::const_iterator groupIt;
	std::vector<t_pairSurfaceGroup>::const_iterator groupEnd = m_RenderableGroups.end();

	int32 nCacheFilePosition = 0;
	nChunk = 2;
	for(groupIt = m_RenderableGroups.begin(); groupIt!=groupEnd; groupIt++ )
	{

		t_pairSurfaceGroup SurfaceGroup = *groupIt;
		CRLUnwrapGroup *Group = SurfaceGroup.first;

		//generate that data.. as late as possible...
		f32 fLightmapQuality = Group->m_fLumenPerUnitU / CSceneContext::GetInstance().m_fLumenPerUnitU;
		CRLMesh FakeMesh( NULL, NULL, NULL, NULL, false, true, fLightmapQuality );

		//generate group datas
		if( false == FakeMesh.RasterizeGroup( Group, NULL,NULL,NULL,NULL, NULL, NULL, NULL, nCacheFilePosition, NULL,NULL,0, Group->m_nOctreeID, false ) )
		{
			CLogFile::FormatLine("*** MEMORY PROBLEM IN DISTRIBUTED FILE SAVE PROCEDURE\n");
			Group->ClearBuffers();
			Group->ReleaseItemArray();
			continue;
	 	}

		if( NULL == Group->m_pDistributedMap )
		{
			Group->ClearBuffers();
			Group->ReleaseItemArray();
			continue;
		}

		SurfaceData Offset = SurfaceGroup.second;

		int32 nComponentOffset = m_nWidth * Offset.Y + Offset.X;
		int32 nWidth = Group->iWidth;
		int32 nHeight = Group->iHeight;
		int32 nOctreeID = Group->m_nOctreeID;
		uint8 nSpanDirection = (uint8)Group->m_SpanDirection;

		fwrite( &nChunk, sizeof(uint8), 1, f);

		fwrite( &nWidth, sizeof(int32), 1, f);
		fwrite( &nHeight, sizeof(int32), 1, f);
		fwrite( &nComponentOffset, sizeof(int32), 1, f);
		fwrite( &nSpanDirection, sizeof(int8), 1, f);
		fwrite( &nOctreeID, sizeof(int32), 1, f);

#ifdef DISTRIBUTED_OCCLUSIONMAP
		fwrite( &Offset.Mask.m_FirstChannel, sizeof(int8), 1, f);
		fwrite( &Offset.Mask.m_ActiveChannelNumber, sizeof(int8), 1, f);
		fwrite( Group->m_LightCoordiates, sizeof(float)*4*16, 1, f);
#endif


		const unsigned long maxChunk = 1 << 20;
		unsigned long sizeLeft = sizeof(uint8) * sceneCtx.m_nDistributedBlockSize * nHeight * nWidth;
		unsigned long sizeToWrite;
		uint8 * ptr = Group->m_pDistributedMap;
		while ( (sizeToWrite = min(sizeLeft, maxChunk)) != 0 )
		{
			if (fwrite (ptr, sizeToWrite, 1, f) != 1)
			{
				CryWarning( VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR,
					"Cannot write to distributed map file!! error = (%d): %s", errno, strerror(errno));
				fclose(f);
				return;
			}
			ptr += sizeToWrite;
			sizeLeft -= sizeToWrite;
		}

		//fwrite( Group->m_pDistributedMap, sizeof(uint8) * sceneCtx.m_nDistributedBlockSize * nHeight * nWidth, 1, f );

		Group->ClearBuffers();
		Group->ReleaseItemArray();
	}

	nChunk = 0;
	fwrite( &nChunk, sizeof(uint8), 1, f);

	fclose(f);
}
