////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Make vert buffers
//
////////////////////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "float.h"
#include "I3DEngine.h"
#include "IIndexedMesh.h"
#include "RenderMesh.h"
#include "CGFContent.h"
#include "GeomQuery.h"

CRenderMesh CRenderMesh::m_Root("Root","Root");
CRenderMesh CRenderMesh::m_RootGlobal("RootGlobal","RootGlobal");
CRenderMesh CRenderMesh::m_RootCache("RootCache","RootCache");

const int m_VertexSize[] =
{
  0,
  sizeof(struct_VERTEX_FORMAT_P3F),	
  sizeof(struct_VERTEX_FORMAT_P3F_COL4UB),
  sizeof(struct_VERTEX_FORMAT_P3F_TEX2F),
  sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F),
  sizeof(struct_VERTEX_FORMAT_P3F_N4B_COL4UB),
  sizeof(struct_VERTEX_FORMAT_P4S_TEX2F),
  sizeof(struct_VERTEX_FORMAT_P4S_COL4UB_TEX2F),

  sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F),
  sizeof(struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F),
  sizeof(struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F),
  sizeof(struct_VERTEX_FORMAT_P3F_TEX3F),
  sizeof(struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F),

  sizeof(struct_VERTEX_FORMAT_TEX2F),
	sizeof(struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F),
	sizeof(struct_VERTEX_FORMAT_COL4UB_COL4UB),
	sizeof(struct_VERTEX_FORMAT_2xP3F_INDEX4UB),
	sizeof(struct_VERTEX_FORMAT_S1F)
};

const int m_StreamSize[] =
{
  0,
  sizeof(SPipTangents),
  sizeof(struct_VERTEX_FORMAT_TEX2F),
  sizeof(struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F),
  sizeof(struct_VERTEX_FORMAT_COL4UB_COL4UB),
  sizeof(struct_VERTEX_FORMAT_2xP3F_INDEX4UB),
  sizeof(struct_VERTEX_FORMAT_P3F),
	sizeof(struct_VERTEX_FORMAT_S1F)
};


SBufInfoTable gBufInfoTable[VERTEX_FORMAT_NUMS] = 
{
  {
    0
  },
  {  //VERTEX_FORMAT_P3F
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_P3F *)0)->x)  
    0
#undef OOFS
  },
  {  //VERTEX_FORMAT_P3F_COL4UB
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_P3F_COL4UB *)0)->x)  
    0,
    OOFS(color.dcolor),
#undef OOFS
  },
  {  //VERTEX_FORMAT_P3F_TEX2F
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_P3F_TEX2F *)0)->x)  
    OOFS(st[0])
#undef OOFS
  },
  {  //VERTEX_FORMAT_P3F_COL4UB_TEX2F
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *)0)->x)  
    OOFS(st[0]),
    OOFS(color.dcolor)
#undef OOFS
  },
  {  //VERTEX_FORMAT_P3F_N4B_COL4UB
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_P3F_N4B_COL4UB *)0)->x)  
    0,
    OOFS(color.dcolor),
#undef OOFS
  },
  {  //VERTEX_FORMAT_P4S_TEX2F
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_P4S_TEX2F *)0)->x)  
    OOFS(st[0])
#undef OOFS
  },
  {  //VERTEX_FORMAT_P4S_COL4UB_TEX2F
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_P4S_COL4UB_TEX2F *)0)->x)  
    OOFS(st[0]),
    OOFS(color.dcolor)
#undef OOFS
  },

  {  // VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F *)0)->x)  
    OOFS(st[0]),
    OOFS(color.dcolor),
#undef OOFS
  },
  {  //VERTEX_FORMAT_TRP3F_COL4UB_TEX2F
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *)0)->x)  
    OOFS(st[0]),
    OOFS(color.dcolor),
#undef OOFS
  },
  {  //VERTEX_FORMAT_TRP3F_TEX2F_TEX3F
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F *)0)->x)  
    OOFS(st0[0]),
#undef OOFS
  },
  {  //VERTEX_FORMAT_P3F_TEX3F
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_P3F_TEX3F *)0)->x)  
    OOFS(st[0]),
#undef OOFS
  },
	{  //VERTEX_FORMAT_P3F_TEX2F_TEX3F
#define OOFS(x) (int)(INT_PTR)&(((struct_VERTEX_FORMAT_P3F_TEX2F_TEX3F *)0)->x)  
		OOFS(st0[0]),
#undef OOFS
	}
};

//#define MEM_CHECK assert(_CrtCheckMemory());
#define MEM_CHECK 

#define MAX_SUB_MATERIALS 32

CRenderMesh::CRenderMesh( const char *szType,const char *szSourceName )
{
	m_nRefCounter = 0;
  m_Indices.Reset();

	m_sType = szType;
  m_sSource = szSourceName;

  m_nVertCount=0;
  m_pSysVertBuffer=NULL;
  m_pVertexBuffer=NULL;

  m_pChunks = m_pChunksSkinned = m_pMergedDepthOnlyChunks = NULL;
  m_nPrimetiveType = R_PRIMV_TRIANGLES;

  m_TempTexCoords = NULL;
  m_TempColors = NULL;

  m_nClientTextureBindID = 0;
  m_bMaterialsWasCreatedInRenderer = 0;
  m_bDynamic = 0;
  m_bShort = 0;
  m_nPosOffset = -1;
  m_vScale = Vec3(1, 1, 1);
  m_Next = NULL;
  m_Prev = NULL;
  m_NextGlobal = NULL;
  m_PrevGlobal = NULL;
  m_NextCache = NULL;
  m_PrevCache = NULL;
  m_pVertexContainer = NULL;
  m_UpdateVBufferMask = ~0;
  m_UpdateFrame = 0;
  if (!m_Root.m_Next)
  {
    m_Root.m_Next = &m_Root;
    m_Root.m_Prev = &m_Root;
  }
  if (!m_RootGlobal.m_NextGlobal)
  {
    m_RootGlobal.m_NextGlobal = &m_RootGlobal;
    m_RootGlobal.m_PrevGlobal = &m_RootGlobal;

    m_RootCache.m_NextCache = &m_RootCache;
    m_RootCache.m_PrevCache = &m_RootCache;
  }
  if (!m_RootCache.m_NextCache)
  {
    m_RootCache.m_NextCache = &m_RootCache;
    m_RootCache.m_PrevCache = &m_RootCache;
  }
  m_vBoxMin = m_vBoxMax = Vec3(0,0,0); //used for hw occlusion test
  PrepareBufferCallback = NULL;
  if (this != &m_RootGlobal && this != &m_Root)
    LinkGlobal(&m_RootGlobal);
  m_arrVtxMap = 0;
	m_pMorphBuddy = 0;
}

//////////////////////////////////////////////////////////////////////////
CRenderMesh::~CRenderMesh()
{
  UnlinkGlobal();

	ReleaseChunks(m_pChunks);
  ReleaseChunks(m_pChunksSkinned);
	ReleaseChunks(m_pMergedDepthOnlyChunks);

  FreeSystemBuffer();

  Unload(false);

  m_SysIndices.Free();

	if(m_pVertexContainer)
		m_pVertexContainer->m_lstVertexContainerUsers.Delete(this);

	for(int i=0; i<m_lstVertexContainerUsers.Count(); i++)
		if(m_lstVertexContainerUsers[i]->GetVertexContainer() == this)
			m_lstVertexContainerUsers[i]->m_pVertexContainer = NULL;
}


void CRenderMesh::ShutDown()
{
  if (CRenderer::CV_r_releaseallresourcesonexit)
  {
    CRenderMesh *pRM = CRenderMesh::m_RootGlobal.m_NextGlobal;
    CRenderMesh *pNext;
    for (pRM=m_RootGlobal.m_NextGlobal; pRM != &CRenderMesh::m_RootGlobal; pRM=pNext)
    {
      pNext = pRM->m_NextGlobal;
      if (CRenderer::CV_r_printmemoryleaks)
      {
        float fSize = pRM->Size(0)/1024.0f/1024.0f;
        iLog->Log("Warning: CRenderMesh::ShutDown: RenderMesh leak %s: %0.3fMb", pRM->m_sSource, fSize);
      }
      if (pRM != &m_RootCache && pRM != &m_Root)
      {
        SAFE_RELEASE_FORCE(pRM);
      }
    }
  }
  CRenderMesh::m_Root.m_Next = &CRenderMesh::m_Root;
  CRenderMesh::m_Root.m_Prev = &CRenderMesh::m_Root;
  CRenderMesh::m_RootGlobal.m_NextGlobal = &CRenderMesh::m_RootGlobal;
  CRenderMesh::m_RootGlobal.m_PrevGlobal = &CRenderMesh::m_RootGlobal;
}

//static FILE *sFP;

//////////////////////////////////////////////////////////////////////////
// Make Render mesh from CMesh.
//////////////////////////////////////////////////////////////////////////
void CRenderMesh::SetMesh( CMesh &mesh,int _nSecColorsSetOffset, uint32 flags, Vec3 *pBSStreamTemp)
{
  if (m_pChunks)
    delete m_pChunks;
  if (m_pChunksSkinned)
    delete m_pChunksSkinned;
  m_pChunks = new PodArray<CRenderChunk>;

  m_vBoxMin = mesh.m_bbox.min;
  m_vBoxMax = mesh.m_bbox.max;


  m_bMaterialsWasCreatedInRenderer = true;

  //////////////////////////////////////////////////////////////////////////
  // Initialize Render Chunks.
  //////////////////////////////////////////////////////////////////////////
  uint32 numSubsets = mesh.GetSubSetCount();
  for (uint32 i=0; i<numSubsets; i++)
  {
    CRenderChunk ChunkInfo;

		if (mesh.m_subsets[i].nNumIndices == 0)
			continue;

		if(mesh.m_subsets[i].nMatFlags & MTL_FLAG_NODRAW)
			continue;

    //add empty chunk, because PodArray is not working with STL-vectors
    m_pChunks->Add(ChunkInfo);

    uint32 num = m_pChunks->Count();
    CRenderChunk* pChunk = m_pChunks->Get(num-1);

    pChunk->m_vCenter     = mesh.m_subsets[i].vCenter;
    pChunk->m_fRadius     = mesh.m_subsets[i].fRadius;
    pChunk->nFirstIndexId = mesh.m_subsets[i].nFirstIndexId;
    pChunk->nNumIndices   = mesh.m_subsets[i].nNumIndices;
    pChunk->nFirstVertId  = mesh.m_subsets[i].nFirstVertId;
    pChunk->nNumVerts     = mesh.m_subsets[i].nNumVerts;
    pChunk->m_nMatID      = mesh.m_subsets[i].nMatID;
    pChunk->m_nMatFlags   = mesh.m_subsets[i].nMatFlags;
		if (mesh.m_subsets[i].nPhysicalizeType==PHYS_GEOM_TYPE_NONE)
			pChunk->m_nMatFlags |= MTL_FLAG_NOPHYSICALIZE;
    
#define VALIDATE_CHUCKS
#if defined(_DEBUG) && defined(VALIDATE_CHUCKS)
		size_t indStart( pChunk->nFirstIndexId );
		size_t indEnd( pChunk->nFirstIndexId + pChunk->nNumIndices );
		for( size_t j( indStart ); j < indEnd; ++j )
		{
			size_t vtxStart( pChunk->nFirstVertId );
			size_t vtxEnd( pChunk->nFirstVertId + pChunk->nNumVerts );
			size_t curIndex0( mesh.m_pIndices[ j ] ); // absolute indexing
			size_t curIndex1( mesh.m_pIndices[ j ] + vtxStart ); // relative indexing using base vertex index
			assert( ( curIndex0 >= vtxStart && curIndex0 < vtxEnd ) || ( curIndex1 >= vtxStart && curIndex1 < vtxEnd ) ) ;
		}
#endif
    
		if (mesh.m_pBoneMapping)
    {
   /*   FILE* sFP;
				sFP = fopen("c:\\Bones.txt", "w");
      fprintf(sFP, "SetMesh: %d subset\n", i);
			*/


			pChunk->m_arrChunkBoneIDs  = mesh.m_subsets[i].m_arrGlobalBonesPerSubset;
			uint32 numIds=pChunk->m_arrChunkBoneIDs.size();
			for (uint32 i=0; i<numIds; i++)
			{
				assert( pChunk->m_arrChunkBoneIDs[i]<0x100 );
			}

	/*		for (int jj=0; jj<pChunk->m_arrBoneIDs.size(); jj++)
      {
        uint32 BoneID=pChunk->m_arrBoneIDs[jj];
        fprintf(sFP, "%d ", BoneID);
      }
      fprintf(sFP, "\n");*/

    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Create RenderElements.
  //////////////////////////////////////////////////////////////////////////
  const bool cbSetDecompressionMatrix = (mesh.m_pSHInfo && mesh.m_pSHInfo->nDecompressionCount == mesh.GetSubSetCount());
	int nCurChunk = 0;
  for (int i = 0; i < mesh.GetSubSetCount(); i++)
  {
    SMeshSubset &subset = mesh.m_subsets[i];
  	if (subset.nNumIndices == 0)
			continue;

		if(subset.nMatFlags & MTL_FLAG_NODRAW)
			continue;

		CRenderChunk *pRenderChunk = &(*m_pChunks)[nCurChunk++];

    CREMesh *pRenderElement = (CREMesh*)gRenDev->EF_CreateRE(eDATA_Mesh);

    // Cross link render chunk with render element.
    pRenderChunk->pRE = pRenderElement;
    pRenderElement->m_pChunk  = pRenderChunk;
    pRenderElement->m_pRenderMesh = this;
    if (subset.nNumVerts <= 500 && !mesh.m_pBoneMapping && !mesh.m_pShapeDeformation && !(flags & FSM_VOXELS) && !(flags & FSM_NO_TANGENTS))
      pRenderElement->mfUpdateFlags(FCEF_MERGABLE);

    bool bTwoSided = (pRenderChunk->m_nMatFlags & MTL_FLAG_2SIDED) != 0;
  }

  uint nVerts = mesh.GetVertexCount();

  //////////////////////////////////////////////////////////////////////////
  // Create system vertex buffer in system memory.
  //////////////////////////////////////////////////////////////////////////
  m_nVertCount = nVerts;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F * pVBuff = NULL;
  struct_VERTEX_FORMAT_P3F_N4B_COL4UB * pVBuffV = NULL;
  if (!(flags & FSM_VOXELS))
  {
    m_nVertexFormat = VERTEX_FORMAT_P3F_COL4UB_TEX2F;
    pVBuff = new struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F[nVerts];
  }
  else
  {
    m_nVertexFormat = VERTEX_FORMAT_P3F_N4B_COL4UB;
    pVBuffV = new struct_VERTEX_FORMAT_P3F_N4B_COL4UB[nVerts];
  }
  SPipTangents *pTBuff = NULL;
  if (!(flags & FSM_NO_TANGENTS))
    pTBuff = new SPipTangents[nVerts];

  m_pSysVertBuffer = new CVertexBuffer;
  m_pSysVertBuffer->m_nVertexFormat = m_nVertexFormat;
  if (pVBuff)
    m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData = pVBuff;
  else
    m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData = pVBuffV;
  m_pSysVertBuffer->m_VS[VSF_TANGENTS].m_VData = pTBuff;


  //////////////////////////////////////////////////////////////////////////
  // Copy sh coefficient stream.
  //////////////////////////////////////////////////////////////////////////
  if (mesh.m_pSHInfo && mesh.m_pSHInfo->pSHCoeffs)
  {
    struct_VERTEX_FORMAT_COL4UB_COL4UB *pSHBuf = new struct_VERTEX_FORMAT_COL4UB_COL4UB[nVerts];
    m_pSysVertBuffer->m_VS[VSF_SH_INFO].m_VData = pSHBuf;
    for (uint i = 0; i < nVerts; ++i)
    {
      pSHBuf[i].coef0 = *(UCol*)(&mesh.m_pSHInfo->pSHCoeffs[i].coeffs[0]);
      pSHBuf[i].coef1 = *(UCol*)(&mesh.m_pSHInfo->pSHCoeffs[i].coeffs[4]);
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Copy positions and normals stream.
  //////////////////////////////////////////////////////////////////////////
  if (pVBuff)
  {
    for (uint i = 0; i < nVerts; ++i)
    {
      pVBuff[i].xyz = mesh.m_pPositions[i];
    }
  }
  else
  {
    for (uint i = 0; i < nVerts; ++i)
    {
      pVBuffV[i].xyz = mesh.m_pPositions[i];
      pVBuffV[i].normal.bcolor[0] = (byte)(mesh.m_pNorms[i][0] * 127.5f + 128.0f);
      pVBuffV[i].normal.bcolor[1] = (byte)(mesh.m_pNorms[i][1] * 127.5f + 128.0f);
      pVBuffV[i].normal.bcolor[2] = (byte)(mesh.m_pNorms[i][2] * 127.5f + 128.0f);
      SwapEndian(pVBuffV[i].normal.dcolor);
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Copy Texture coords stream.
  //////////////////////////////////////////////////////////////////////////
  if (mesh.m_pTexCoord)
  {
    if (pVBuff)
    {
      for (uint i = 0; i < nVerts; ++i)
      {
        pVBuff[i].st[0] = mesh.m_pTexCoord[i].s;
        pVBuff[i].st[1] = mesh.m_pTexCoord[i].t;
      }
    }
    else
    {
      assert(0);
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Copy color streams.
  //////////////////////////////////////////////////////////////////////////
  if (mesh.m_pColor0)
  {
    if (pVBuff)
    {
      for (uint i = 0; i < nVerts; ++i)
      {
        pVBuff[i].color.bcolor[0] = mesh.m_pColor0[i].b;
        pVBuff[i].color.bcolor[1] = mesh.m_pColor0[i].g;
        pVBuff[i].color.bcolor[2] = mesh.m_pColor0[i].r;
        pVBuff[i].color.bcolor[3] = mesh.m_pColor0[i].a;
        SwapEndian(pVBuff[i].color.dcolor);
      }
    }
    else
    {
      for (uint i = 0; i < nVerts; ++i)
      {
        pVBuffV[i].color.bcolor[0] = mesh.m_pColor0[i].b;
        pVBuffV[i].color.bcolor[1] = mesh.m_pColor0[i].g;
        pVBuffV[i].color.bcolor[2] = mesh.m_pColor0[i].r;
        pVBuffV[i].color.bcolor[3] = mesh.m_pColor0[i].a;
        SwapEndian(pVBuffV[i].color.dcolor);
      }
    }
  }
  else
  {
    if (pVBuff)
    {
      for (uint i = 0; i < nVerts; ++i)
      {
        pVBuff[i].color.dcolor = ~0;
      }
    }
    else
    {
      assert(0);
    }
  }

  if (mesh.m_pColor1)
  {
    if (pVBuff)
    {
      assert(0);
    }
    else
    {
      for (uint i = 0; i < nVerts; ++i)
      {
#ifndef XENON
        pVBuffV[i].normal.bcolor[3] = mesh.m_pColor1[i].b;
#else
        pVBuffV[i].normal.bcolor[3] = mesh.m_pColor1[i].g;
#endif
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Copy tangent space stream.
  //////////////////////////////////////////////////////////////////////////
  if (mesh.m_pTangents && pTBuff)
  {
    for (uint i = 0; i < nVerts; ++i)
    {
      pTBuff[i].Binormal = mesh.m_pTangents[i].Binormal;
      pTBuff[i].Tangent = mesh.m_pTangents[i].Tangent;
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Copy skin-streams.
  //////////////////////////////////////////////////////////////////////////
  if (mesh.m_pBoneMapping) 
		SetSkinningDataCharacter(mesh,mesh.m_pBoneMapping,pBSStreamTemp);

  //////////////////////////////////////////////////////////////////////////
  // Copy shape deformation data.
  //////////////////////////////////////////////////////////////////////////
  if (mesh.m_pShapeDeformation) 
  {
    struct_VERTEX_FORMAT_2xP3F_INDEX4UB *pShapeDeformBuff = new struct_VERTEX_FORMAT_2xP3F_INDEX4UB[nVerts];
    m_pSysVertBuffer->m_VS[VSF_HWSKIN_SHAPEDEFORM_INFO].m_VData = pShapeDeformBuff;
    for (uint32 e=0; e<nVerts; e++ )
    {
      pShapeDeformBuff[e].thin            = mesh.m_pShapeDeformation[e].thin;
      pShapeDeformBuff[e].fat             = mesh.m_pShapeDeformation[e].fat;
      pShapeDeformBuff[e].index.bcolor[0] = mesh.m_pShapeDeformation[e].index.b;
      pShapeDeformBuff[e].index.bcolor[1] = mesh.m_pShapeDeformation[e].index.g;
      pShapeDeformBuff[e].index.bcolor[2] = mesh.m_pShapeDeformation[e].index.r;
      pShapeDeformBuff[e].index.bcolor[3] = mesh.m_pShapeDeformation[e].index.a;
      SwapEndian(pShapeDeformBuff[e].index.dcolor);
    }
  }

	//////////////////////////////////////////////////////////////////////////
	// create buffer for morph-targets.
	//////////////////////////////////////////////////////////////////////////
	if (flags&FSM_MORPH_TARGETS)
	{
		struct_VERTEX_FORMAT_P3F *pMorphTargets = new struct_VERTEX_FORMAT_P3F[nVerts];
		m_pSysVertBuffer->m_VS[VSF_HWSKIN_MORPHTARGET_INFO].m_VData = pMorphTargets;
		m_pSysVertBuffer->m_VS[VSF_HWSKIN_MORPHTARGET_INFO].m_bDynamic = true;
		// Initialize morph targets buffer.
		memset( pMorphTargets,0,sizeof(struct_VERTEX_FORMAT_P3F)*nVerts );
	}

  if (pVBuff && CRenderer::CV_r_meshshort)
  {
    Vec3 vMax = Vec3(-100000.0f, -100000.0f, -100000.0f);
    uint i;
    for (i=0; i<nVerts; ++i)
    {
      vMax.CheckMax(pVBuff[i].xyz.abs());
    }
    struct_VERTEX_FORMAT_P4S_COL4UB_TEX2F *pVB = new struct_VERTEX_FORMAT_P4S_COL4UB_TEX2F[nVerts];
    m_nVertexFormat = VERTEX_FORMAT_P4S_COL4UB_TEX2F;
    m_bShort = true;
    m_vScale = vMax;
    Vec3 vNorm;
    vNorm.x = 1.0f / vMax.x;
    vNorm.y = 1.0f / vMax.y;
    vNorm.z = 1.0f / vMax.z;
    for (i=0; i<nVerts; ++i)
    {
      Vec3 v = pVBuff[i].xyz.CompMul(vNorm);
      pVB[i].xyz = tPackF2Bv(v);
      pVB[i].st = pVBuff[i].st;
      pVB[i].color = pVBuff[i].color;
    }
    SAFE_DELETE_ARRAY(pVBuff);
    m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData = pVB;
    m_pSysVertBuffer->m_nVertexFormat = m_nVertexFormat;
  }

  //////////////////////////////////////////////////////////////////////////
  // Copy indices.
  //////////////////////////////////////////////////////////////////////////
  m_nNumVidIndices = 0;
  m_SysIndices.Reserve(mesh.GetIndexCount());
  for (int i = 0; i < mesh.GetIndexCount(); i++)
  {
    m_SysIndices[i] = mesh.m_pIndices[i];
  }

  InvalidateVideoBuffer(FMINV_ALL);

  if (gRenDev && !gRenDev->CheckDeviceLost())
  {
    if (flags & FSM_CREATE_DEVICE_MESH)
    {
      int nStreamFlags = 0;
      for (int i=0; i<VSF_NUM; i++)
      {
        if (m_pSysVertBuffer->m_VS[i].m_VData)
          nStreamFlags |= (1<<i);
      }
      //int nFormat = mesh.m_pColor0 ? VERTEX_FORMAT_P3F_COL4UB_TEX2F : VERTEX_FORMAT_P3F_TEX2F;
  #ifndef XENON
      int nFormat = VERTEX_FORMAT_P3F_TEX2F;
  #else
      // On X360 we have to use maximum vertex format since Mesh updating/deleting
      // is not allowed with predicated tiling
      int nFormat = VERTEX_FORMAT_P3F_COL4UB_TEX2F;
  #endif
      if (flags & FSM_VOXELS)
        nFormat = VERTEX_FORMAT_P3F_N4B_COL4UB;
      CheckUpdate(nFormat, nStreamFlags, 0, 0, 0, 0);
    }
  }
}


void CRenderMesh::SetSkinningDataVegetation(struct SMeshBoneMapping *pBoneMapping, Vec3 *pBSStreamTemp)
{
//  struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB *pSkinBuff = new struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB[nVerts];
	struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F *pSkinBuff = new struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F[m_nVertCount];
  m_pSysVertBuffer->m_VS[VSF_HWSKIN_INFO].m_VData = pSkinBuff;
	for (uint i = 0; i < m_nVertCount; i++ )
	{
		// get bone IDs
		uint16 b0 = pBoneMapping[i].boneIDs[0];
		uint16 b1 = pBoneMapping[i].boneIDs[1];
		uint16 b2 = pBoneMapping[i].boneIDs[2];
		uint16 b3 = pBoneMapping[i].boneIDs[3];

		// get weights
		f32 w0 = pBoneMapping[i].weights[0];
		f32 w1 = pBoneMapping[i].weights[1];
		f32 w2 = pBoneMapping[i].weights[2];
		f32 w3 = pBoneMapping[i].weights[3];

		// if weight is zero set bone ID to zero as the bone has no influence anyway,
		// this will fix some issue with incorrectly exported models (e.g. system freezes on ATI cards when access invalid bones)
		if (w0 == 0) b0 = 0;
		if (w1 == 0) b1 = 0;
		if (w2 == 0) b2 = 0;
		if (w3 == 0) b3 = 0;											


		pSkinBuff[i].indices.bcolor[0] = b0;
		pSkinBuff[i].indices.bcolor[1] = b1;
		pSkinBuff[i].indices.bcolor[2] = b2;
		pSkinBuff[i].indices.bcolor[3] = b3;

		// copy weights
		pSkinBuff[i].weights.bcolor[0] = (uint8)w0;
		pSkinBuff[i].weights.bcolor[1] = (uint8)w1;
		pSkinBuff[i].weights.bcolor[2] = (uint8)w2;
		pSkinBuff[i].weights.bcolor[3] = (uint8)w3;

		if (pBSStreamTemp)
			pSkinBuff[i].boneSpace  = pBSStreamTemp[i];
	}

}


void CRenderMesh::SetSkinningDataCharacter(CMesh& mesh, struct SMeshBoneMapping *pBoneMapping, Vec3 *pBSStreamTemp)
{
	//  struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB *pSkinBuff = new struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB[nVerts];
	struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F *pSkinBuff = new struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F[m_nVertCount];
	m_pSysVertBuffer->m_VS[VSF_HWSKIN_INFO].m_VData = pSkinBuff;
	for (uint i = 0; i < m_nVertCount; i++ )
	{
		// get bone IDs
		uint16 b0 = pBoneMapping[i].boneIDs[0];
		uint16 b1 = pBoneMapping[i].boneIDs[1];
		uint16 b2 = pBoneMapping[i].boneIDs[2];
		uint16 b3 = pBoneMapping[i].boneIDs[3];

		// get weights
		f32 w0 = pBoneMapping[i].weights[0];
		f32 w1 = pBoneMapping[i].weights[1];
		f32 w2 = pBoneMapping[i].weights[2];
		f32 w3 = pBoneMapping[i].weights[3];

		// if weight is zero set bone ID to zero as the bone has no influence anyway,
		// this will fix some issue with incorrectly exported models (e.g. system freezes on ATI cards when access invalid bones)
		if (w0 == 0) b0 = 0;
		if (w1 == 0) b1 = 0;
		if (w2 == 0) b2 = 0;
		if (w3 == 0) b3 = 0;											


		pSkinBuff[i].indices.bcolor[0] = b0;
		pSkinBuff[i].indices.bcolor[1] = b1;
		pSkinBuff[i].indices.bcolor[2] = b2;
		pSkinBuff[i].indices.bcolor[3] = b3;

		// copy weights
		pSkinBuff[i].weights.bcolor[0] = (uint8)w0;
		pSkinBuff[i].weights.bcolor[1] = (uint8)w1;
		pSkinBuff[i].weights.bcolor[2] = (uint8)w2;
		pSkinBuff[i].weights.bcolor[3] = (uint8)w3;

		if (pBSStreamTemp)
			pSkinBuff[i].boneSpace  = pBSStreamTemp[i];
	}


	//#ifdef _DEBUG
	if (1)
	{
		//--------------------------------------------------------------------------------------
		//--- this code is checking if all bone-mappings in the subsets are in a valid range ---
		//--------------------------------------------------------------------------------------
		uint32 numSubsets = mesh.m_subsets.size();
		for (uint32 s=0; s<numSubsets; s++)
		{
			uint32 nStartIndex = mesh.m_subsets[s].nFirstIndexId;
			uint32 nNumIndices = mesh.m_subsets[s].nNumIndices;
			uint32 BonesPerSubset = mesh.m_subsets[s].m_arrGlobalBonesPerSubset.size();

			//if (BonesPerSubset==0)
			//	continue;

			assert(BonesPerSubset<=NUM_MAX_BONES_PER_GROUP); //if this is bigger then the max-value, then we get a GPU crash
			//if (BonesPerSubset>NUM_MAX_BONES_PER_GROUP)
			//	CryError("Fatal Error: BonesPerSubset exeeds maximum of bones: %d (contact: Ivo@Crytek.de)",BonesPerSubset );

			for (uint32 i=nStartIndex; i<(nStartIndex+nNumIndices); i++)
			{
				uint32 v = mesh.m_pIndices[i];
				// get bone-ids
				uint16 b0 = pBoneMapping[v].boneIDs[0];
				uint16 b1 = pBoneMapping[v].boneIDs[1];
				uint16 b2 = pBoneMapping[v].boneIDs[2];
				uint16 b3 = pBoneMapping[v].boneIDs[3];
				// get weights
				f32 w0 = pBoneMapping[v].weights[0];
				f32 w1 = pBoneMapping[v].weights[1];
				f32 w2 = pBoneMapping[v].weights[2];
				f32 w3 = pBoneMapping[v].weights[3];
				// if weight is zero set bone ID to zero as the bone has no influence anyway,
				if (w0 == 0) b0 = 0;
				if (w1 == 0) b1 = 0;
				if (w2 == 0) b2 = 0;
				if (w3 == 0) b3 = 0;											
				assert( b0 < BonesPerSubset );  
				assert( b1 < BonesPerSubset );
				assert( b2 < BonesPerSubset );
				assert( b3 < BonesPerSubset );

				
				if (b0 >= BonesPerSubset)
					CryError("Fatal Error: index out of range: Index: %d   BonesPerSubset: %d (contact: Ivo@Crytek.de)",b0,BonesPerSubset );
				if (b1 >= BonesPerSubset)
					CryError("Fatal Error: index out of range: Index: %d   BonesPerSubset: %d (contact: Ivo@Crytek.de)",b1,BonesPerSubset );
				if (b2 >= BonesPerSubset)
					CryError("Fatal Error: index out of range: Index: %d   BonesPerSubset: %d (contact: Ivo@Crytek.de)",b2,BonesPerSubset );
				if (b3 >= BonesPerSubset)
					CryError("Fatal Error: index out of range: Index: %d   BonesPerSubset: %d (contact: Ivo@Crytek.de)",b3,BonesPerSubset );
					
			}
		}
	}	
	//#endif
}


IIndexedMesh *CRenderMesh::GetIndexedMesh(IIndexedMesh *pIdxMesh)
{
	if (!pIdxMesh)
		pIdxMesh = iSystem->GetI3DEngine()->CreateIndexedMesh();
	CMesh *pMesh = pIdxMesh->GetMesh();
	int i,j;

	pIdxMesh->SetVertexCount(m_nVertCount);
	pIdxMesh->SetTexCoordsAndTangentsCount(m_nVertCount);
	pIdxMesh->SetIndexCount(m_SysIndices.Num());
	pIdxMesh->SetSubSetCount(m_pChunks->size());

	strided_pointer<Vec3> pVtx,pNorm;
	strided_pointer<Vec4sf> pTang,pBinorm;
	strided_pointer<Vec2> pTex;
	pVtx.data = (Vec3*)GetStridedPosPtr(pVtx.iStride);
	//pNorm.data = (Vec3*)GetNormalPtr(pNorm.iStride);
	pTex.data = (Vec2*)GetStridedUVPtr(pTex.iStride);
	pTang.data = (Vec4sf*)GetStridedTangentPtr(pTang.iStride);
	pBinorm.data = (Vec4sf*)GetStridedBinormalPtr(pBinorm.iStride);

	for(i=0;i<m_nVertCount;i++)
	{
    pMesh->m_pPositions[i] = pVtx[i];
    pMesh->m_pNorms[i] = Vec3(0,0,1); //pNorm[i];
  }

	if (pTex.data != NULL && pTang.data != NULL && pBinorm.data != NULL)
	{
		for(i=0;i<m_nVertCount;i++)
		{
		pMesh->m_pTexCoord[i].s = pTex[i].x;
    pMesh->m_pTexCoord[i].t = pTex[i].y;
		pMesh->m_pTangents[i].Binormal = pBinorm[i];
    pMesh->m_pTangents[i].Tangent = pTang[i];
  }
	}
	else
	{
		for(i=0;i<m_nVertCount;i++)
		{
			pMesh->m_pTexCoord[i].s = 0.0f;
			pMesh->m_pTexCoord[i].t = 0.0f;
			pMesh->m_pTangents[i].Binormal = Vec4sf(0,0,0,0);
			pMesh->m_pTangents[i].Tangent = Vec4sf(0,0,0,0);
		}
	}

	if (m_nVertexFormat==VERTEX_FORMAT_P3F_COL4UB_TEX2F || m_nVertexFormat==VERTEX_FORMAT_P3F_N4B_COL4UB ||
			m_nVertexFormat==VERTEX_FORMAT_P4S_COL4UB_TEX2F || m_nVertexFormat==VERTEX_FORMAT_P3F_COL4UB)
	{
		strided_pointer<SMeshColor> pColors;
		pColors.data = (SMeshColor*)GetStridedColorPtr(pColors.iStride);
		pIdxMesh->SetColorsCount(m_nVertCount);
		for(i=0;i<m_nVertCount;i++)
			pMesh->m_pColor0[i] = pColors[i];
	}

	for(i=0;i<m_SysIndices.Num();i++)
    pMesh->m_pIndices[i] = m_SysIndices[i];

	struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F *pSkinBuff = (struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F*)m_pSysVertBuffer->m_VS[VSF_HWSKIN_INFO].m_VData;
	if (pSkinBuff)
	{
		pIdxMesh->AllocateBoneMapping();
		for(i=0;i<m_nVertCount;i++) for(j=0;j<4;j++)
		{
			pMesh->m_pBoneMapping[i].boneIDs[j] = pSkinBuff[i].indices.bcolor[j];
			pMesh->m_pBoneMapping[i].weights[j] = pSkinBuff[i].weights.bcolor[j];
		}
	}

	struct_VERTEX_FORMAT_COL4UB_COL4UB *pSHBuf = 
		(struct_VERTEX_FORMAT_COL4UB_COL4UB*)m_pSysVertBuffer->m_VS[VSF_SH_INFO].m_VData;
	if (pSHBuf) 
	{
		pIdxMesh->AllocateSHData();
		for(i=0;i<m_nVertCount;i++) for(j=0;j<8;j++)
			pMesh->m_pSHInfo->pSHCoeffs[i].coeffs[j] = ((uchar*)&pSHBuf[i].coef0)[j];
	}

	for(i=0;i<m_pChunks->size();i++)
	{
		SMeshSubset &mss = pIdxMesh->GetSubSet(i);
		mss.vCenter				= (*m_pChunks)[i].m_vCenter;
    mss.fRadius				= (*m_pChunks)[i].m_fRadius;
    mss.nFirstIndexId = (*m_pChunks)[i].nFirstIndexId;
    mss.nNumIndices		= (*m_pChunks)[i].nNumIndices;
    mss.nFirstVertId	= (*m_pChunks)[i].nFirstVertId;
    mss.nNumVerts			= (*m_pChunks)[i].nNumVerts;
    mss.nMatID				= (*m_pChunks)[i].m_nMatID;
    mss.nMatFlags			= (*m_pChunks)[i].m_nMatFlags;
		mss.nPhysicalizeType = ((*m_pChunks)[i].m_nMatFlags & MTL_FLAG_NOPHYSICALIZE) ? PHYS_GEOM_TYPE_NONE :
			(((*m_pChunks)[i].m_nMatFlags & MTL_FLAG_NODRAW) ? PHYS_GEOM_TYPE_OBSTRUCT : PHYS_GEOM_TYPE_DEFAULT);
		pIdxMesh->SetSubsetBoneIds(i,(*m_pChunks)[i].m_arrChunkBoneIDs);
	}

	return pIdxMesh;
}

void CRenderMesh::CreateChunksSkinned()
{
  PodArray<CRenderChunk>& arrSrcMats = *(m_pChunks);
  PodArray<CRenderChunk>& arrNewMats = *(m_pChunksSkinned = new PodArray<CRenderChunk>);
  arrNewMats.resize (arrSrcMats.Size());
  for (int i=0; i<arrSrcMats.size(); ++i)
  {
    CRenderChunk& rSrcMat = arrSrcMats[i]; 
    CRenderChunk& rNewMat = arrNewMats[i];
    rNewMat = rSrcMat;
    CREMesh *re = rSrcMat.pRE;
    if (re)
    {
      rNewMat.pRE = (CREMesh *)gRenDev->EF_CreateRE(eDATA_Mesh);
      CRendElement *pNext = rNewMat.pRE->m_NextGlobal;
      CRendElement *pPrev = rNewMat.pRE->m_PrevGlobal;
      *rNewMat.pRE = *re;
      if (rNewMat.pRE->m_pChunk) // affects the source mesh!! will only work correctly if the source is deleted after copying
        rNewMat.pRE->m_pChunk = &rNewMat;
      rNewMat.pRE->m_NextGlobal = pNext;
      rNewMat.pRE->m_PrevGlobal = pPrev;
      rNewMat.pRE->m_pRenderMesh = this;
      rNewMat.pRE->m_CustomData = NULL;
    }
  }
}

void CRenderMesh::CopyTo(IRenderMesh *_pDst, bool bUseSysBuf, int nAppendVtx)
{
  CRenderMesh *pDst = (CRenderMesh *)_pDst;
  PodArray<CRenderChunk>& arrSrcMats = *(m_pChunks);
  PodArray<CRenderChunk>& arrNewMats = *(pDst->m_pChunks = new PodArray<CRenderChunk>);
  pDst->m_bMaterialsWasCreatedInRenderer  = true;
  arrNewMats.resize (arrSrcMats.Size());
  unsigned i;
  for (i = 0; i < arrSrcMats.size(); ++i)
  {
    CRenderChunk& rSrcMat = arrSrcMats[i]; 
    CRenderChunk& rNewMat = arrNewMats[i];
    rNewMat = rSrcMat;
		rNewMat.nNumVerts	+= ((m_nVertCount-2-rNewMat.nNumVerts-rNewMat.nFirstVertId)>>31) & nAppendVtx;
    CREMesh *re = rSrcMat.pRE;
    if (re)
    {
      rNewMat.pRE = (CREMesh *)gRenDev->EF_CreateRE(eDATA_Mesh);
      CRendElement *pNext = rNewMat.pRE->m_NextGlobal;
      CRendElement *pPrev = rNewMat.pRE->m_PrevGlobal;
      *rNewMat.pRE = *re;
			if (rNewMat.pRE->m_pChunk) // affects the source mesh!! will only work correctly if the source is deleted after copying
      {
        rNewMat.pRE->m_pChunk = &rNewMat;
				rNewMat.pRE->m_pChunk->nNumVerts += ((m_nVertCount-2-re->m_pChunk->nNumVerts-re->m_pChunk->nFirstVertId)>>31) & nAppendVtx;
      }
      rNewMat.pRE->m_NextGlobal = pNext;
      rNewMat.pRE->m_PrevGlobal = pPrev;
      rNewMat.pRE->m_pRenderMesh = pDst;
			//assert(rNewMat.pRE->m_CustomData);
			rNewMat.pRE->m_CustomData = NULL;
    }
  }
  pDst->m_Indices.Reset();
  pDst->m_SysIndices.Copy(m_SysIndices);
  pDst->m_nNumVidIndices = 0;
  pDst->InvalidateVideoBuffer(FMINV_ALL);
  pDst->m_nVertCount = m_nVertCount+nAppendVtx;
  pDst->m_nVertexFormat = m_nVertexFormat;
  pDst->m_bShort = m_bShort;
  pDst->m_vScale = m_vScale;
  if (m_TempColors)
  {
    pDst->m_TempColors = new UCol[m_nVertCount];
    memcpy(pDst->m_TempColors, m_TempColors, sizeof(UCol)*m_nVertCount);
  }
  pDst->AllocateSystemBuffer(m_nVertCount+nAppendVtx);
  CVertexBuffer* pSecVertBuffer  = pDst->m_pSysVertBuffer;
  void * pTmp = pSecVertBuffer->m_VS[VSF_GENERAL].m_VData;
  memcpy(pSecVertBuffer, m_pSysVertBuffer, sizeof(*pSecVertBuffer));
  pSecVertBuffer->m_VS[VSF_GENERAL].m_VData=pTmp;
  if (m_pVertexBuffer)
  {
    if (m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData)
    {
      cryMemcpy(pSecVertBuffer->m_VS[VSF_GENERAL].m_VData, m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData, GetStride(VSF_GENERAL)*m_nVertCount);
    }
    else
    {
      gRenDev->UpdateBuffer(m_pVertexBuffer, NULL, m_nVertCount, false, 0, VSF_GENERAL);
      cryMemcpy(pSecVertBuffer->m_VS[VSF_GENERAL].m_VData, m_pVertexBuffer->m_VS[VSF_GENERAL].m_VData, GetStride(VSF_GENERAL)*m_nVertCount);
      gRenDev->UnlockBuffer(m_pVertexBuffer, VSF_GENERAL, m_nVertCount);
    }
    for (i=1; i<VSF_NUM; i++)
    {
      if (m_pSysVertBuffer->m_VS[i].m_VData)
      {
        pSecVertBuffer->m_VS[i].m_VData = new byte [GetStride(i)*(m_nVertCount+nAppendVtx)];
        cryMemcpy(pSecVertBuffer->m_VS[i].m_VData, m_pSysVertBuffer->m_VS[i].m_VData, GetStride(i)*m_nVertCount);
      }
      else
      if (m_pVertexBuffer->GetStream(i, NULL))
      {
        gRenDev->UpdateBuffer(m_pVertexBuffer, NULL, m_nVertCount, false, 0, i);
        if (m_pVertexBuffer->m_VS[i].m_VData)
        {
          pSecVertBuffer->m_VS[i].m_VData = new byte [GetStride(i)*(m_nVertCount+nAppendVtx)];
          cryMemcpy(pSecVertBuffer->m_VS[i].m_VData, m_pVertexBuffer->m_VS[i].m_VData, GetStride(i)*m_nVertCount);
        }
        gRenDev->UnlockBuffer(m_pVertexBuffer, i, m_nVertCount);
      }
    }
  }
  else
  {
    cryMemcpy(pSecVertBuffer->m_VS[VSF_GENERAL].m_VData, m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData, GetStride(VSF_GENERAL)*m_nVertCount);
    for (i=1; i<VSF_NUM; i++)
    {
      if (pSecVertBuffer->m_VS[i].m_VData)
      {
        pSecVertBuffer->m_VS[i].m_VData = new byte [GetStride(i)*(m_nVertCount+nAppendVtx)];
        cryMemcpy(pSecVertBuffer->m_VS[i].m_VData, m_pSysVertBuffer->m_VS[i].m_VData, GetStride(i)*m_nVertCount);
      }
    }
  }
  /*
  if (CRenderer::CV_r_precachemesh)
  {
    int nVertFormat = -1;
    int Flags = 0;
    for (int i=0; i<(*pDst->m_pChunks).Count(); i++)
    {
      CRenderChunk *pChunk = &(*pDst->m_pChunks)[i];
      if (!pChunk->pRE)
        continue;
      SShaderItem Sh = pChunk->GetShaderItem();
      if (Sh.m_pShader)
      {
        IShader *pSH = Sh.m_pShader->GetTemplate(-1);
        int nShVertFormat = pSH->GetVertexFormat();
        if (nVertFormat < 0)
          nVertFormat = nShVertFormat;
        else
          nVertFormat = gRenDev->m_RP.m_VFormatsMerge[nShVertFormat][nVertFormat];
        if (pSH->GetFlags() & EF_NEEDTANGENTS)
          Flags |= SHPF_TANGENTS;
      }
    }
    if (nVertFormat >= 0)
      pDst->CheckUpdate(nVertFormat, Flags, false);
  }*/
}

void CRenderMesh::AllocateSystemBuffer( int nVertCount )
{
  m_pSysVertBuffer = new CVertexBuffer;
  m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData = CreateVertexBuffer(m_nVertexFormat, nVertCount);
}


//////////////////////////////////////////////////////////////////////////
void CRenderMesh::SaveColors(byte *pD, SBufInfoTable *pOffs, int Size)
{
  if (!pOffs->OffsColor)
    return;
  if (!m_TempColors)
  {
    m_TempColors = new UCol[m_nVertCount];
    for (int i=0; i<m_nVertCount; i++, pD+=Size)
    {
      m_TempColors[i].dcolor = *(uint *)&pD[pOffs->OffsColor];
    }
  }
}
void CRenderMesh::SaveTexCoords(byte *pD, SBufInfoTable *pOffs, int Size)
{
  if (!pOffs->OffsTC)
    return;
  if (!m_TempTexCoords)
  {
    m_TempTexCoords = new Vec2[m_nVertCount];
    for (int i=0; i<m_nVertCount; i++, pD+=Size)
    {
      m_TempTexCoords[i][0] = *(float *)&pD[pOffs->OffsTC];
      m_TempTexCoords[i][1] = *(float *)&pD[pOffs->OffsTC+4];
    }
  }
}

bool CRenderMesh::ReCreateSystemBuffer(int VertFormat)
{
  SBufInfoTable *pOffsOld, *pOffsNew;
  byte *pOld = (byte *)m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData;
  int nFormatOld = m_pSysVertBuffer->m_nVertexFormat;
  pOffsOld = &gBufInfoTable[nFormatOld];
  int SizeOld = m_VertexSize[nFormatOld];
  byte *pNew = NULL;
  m_pSysVertBuffer->m_nVertexFormat = VertFormat;
  pOffsNew = &gBufInfoTable[VertFormat];
  int SizeNew = m_VertexSize[VertFormat];

  bool bRes = true;

  if (pOld)
  {
    byte *p = (byte *)CreateVertexBuffer(VertFormat, m_nVertCount);
    pNew = (byte *)p;
    byte *pS = pOld;
    byte *pD = pNew;
    if (m_bShort)
    {
      for (int i=0; i<m_nVertCount; i++, pD+=SizeNew, pS+=SizeOld)
      {
        *(Vec4sf *)pD = *(Vec4sf *)pS;
      }
    }
    else
    {
      for (int i=0; i<m_nVertCount; i++, pD+=SizeNew, pS+=SizeOld)
      {
        *(Vec3 *)pD = *(Vec3 *)pS;
      }
    }
    if (pOffsNew->OffsColor)
    {
      // Restore colors
      pD = &pNew[pOffsNew->OffsColor];
      if (pOffsOld->OffsColor)
      {
        pS = &pOld[pOffsOld->OffsColor];
        for (int i=0; i<m_nVertCount; i++, pD+=SizeNew, pS+=SizeOld)
        {
          *(DWORD *)pD = *(DWORD *)pS;
        }
      }
      else
      if (m_TempColors)
      {
        for (int i=0; i<m_nVertCount; i++, pD+=SizeNew)
        {
          *(DWORD *)pD = m_TempColors[i].dcolor;
        }
        SAFE_DELETE_ARRAY(m_TempColors);
      }
      else
      {
        if (gRenDev->m_RP.m_pShader)
          iLog->Log("Warning: we lost colors for shader '%s:%s'\n", gRenDev->m_RP.m_pShader->GetName(), gRenDev->m_RP.m_pCurTechnique ? gRenDev->m_RP.m_pCurTechnique->m_Name.c_str() : "Unknown");
        bRes = false;
      }
    }
    else
      SaveColors(pOld, pOffsOld, SizeOld);

    if (pOffsNew->OffsTC)
    {
      // Restore colors
      pD = &pNew[pOffsNew->OffsTC];
      if (pOffsOld->OffsTC)
      {
        pS = &pOld[pOffsOld->OffsTC];
        for (int i=0; i<m_nVertCount; i++, pD+=SizeNew, pS+=SizeOld)
        {
          *(float *)pD = *(float *)pS;
          *(float *)&pD[4] = *(float *)&pS[4];
        }
      }
      else
      if (m_TempTexCoords)
      {
        for (int i=0; i<m_nVertCount; i++, pD+=SizeNew)
        {
          *(float *)pD = m_TempTexCoords[i][0];
          *(float *)&pD[4] = m_TempTexCoords[i][1];
        }
        SAFE_DELETE_ARRAY(m_TempTexCoords);
      }
      else
      {
        if (gRenDev->m_RP.m_pShader)
          iLog->Log("Warning: we lost texture coords for shader '%s:%s'\n", gRenDev->m_RP.m_pShader->GetName(), gRenDev->m_RP.m_pCurTechnique ? gRenDev->m_RP.m_pCurTechnique->m_Name.c_str() : "Unknown");
        bRes = false;
      }
    }
    else
      SaveTexCoords(pOld, pOffsOld, SizeOld);

    if (VertFormat == VERTEX_FORMAT_P3F_N4B_COL4UB)
    {
      UCol col;
      col.bcolor[0] = 128; col.bcolor[1] = 128; col.bcolor[2] = 255; col.bcolor[3] = 255;
      pD = &pNew[12];
      for (int i=0; i<m_nVertCount; i++, pD+=SizeNew)
      {
        *(UCol *)&pD = col;
      }

    }
  }

  delete [] pOld;
  m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData = pNew;
  if (!pNew)
    return false;

  return bRes;
}


void CRenderMesh::Unload(bool bRestoreSys)
{
  Unlink();
  if (gRenDev)
  {
    DestroyVidVertices(bRestoreSys);
    DestroyVidIndices(bRestoreSys);
  }
  m_pVertexBuffer = NULL;
}

void CRenderMesh::UpdateDynBufPtr(int VertFormat)
{
  DestroyVidVertices(false);
  m_pVertexBuffer = new CVertexBuffer;

  void *vBufTangs, *vBufGen;
  int nOffsTangs, nOffsGen;
  vBufGen = gRenDev->GetDynVBPtr(m_nVertCount, nOffsGen, 1);
  vBufTangs = gRenDev->GetDynVBPtr(m_nVertCount, nOffsTangs, 2);
  assert (vBufTangs && vBufGen && nOffsTangs == nOffsGen);
  m_pVertexBuffer->m_VS[VSF_GENERAL].m_VData = vBufGen;
  m_pVertexBuffer->m_VS[VSF_TANGENTS].m_VData = vBufTangs;
  m_pVertexBuffer->m_VS[VSF_GENERAL].m_bLocked = true;
  m_pVertexBuffer->m_VS[VSF_TANGENTS].m_bLocked = true;
  m_pVertexBuffer->m_nVertexFormat = VERTEX_FORMAT_P3F_TEX2F;
  Unlink();
}


struct SSortTri
{
  int nTri;
  float fDist;
};

static TArray<SSortTri> sSortTris;

_inline bool Compare(const SSortTri &a, const SSortTri &b)
{
  float fDistA = a.fDist;
  float fDistB = b.fDist;
  if (fDistA+0.01f < fDistB)
    return true;
  return false;
}

void CRenderMesh::SortTris()
{
  int i, n;

  CRenderer *rd = gRenDev;
  if (m_SortFrame == rd->m_RP.m_TransformFrame)
    return;
  m_SortFrame = rd->m_RP.m_TransformFrame;
  ushort *pInds = GetIndices(NULL);
  CRenderObject *pObj = rd->m_RP.m_pCurObject;
  Vec3 vCam = rd->GetRCamera().Orig; //pObj->GetInvMatrix().TransformPoint(rd->GetRCamera().Orig);
  ushort *pDst = NULL;
  int nPosPtr;
  byte *pPosPtr = GetStridedPosPtr(nPosPtr);
  bool bGlobalTransp = false;
  if (rd->m_RP.m_pShaderResources && rd->m_RP.m_pShaderResources->Opacity() != 1.0f)
    bGlobalTransp = true;

  for(i=0; i<m_pChunks->Count(); i++)
  {
    CRenderChunk *pMI = m_pChunks->Get(i);

    if (!pMI->pRE)
      continue;

    const SShaderItem &shaderItem = m_pMaterial->GetShaderItem(pMI->m_nMatID);
    IShader *pSH = shaderItem.m_pShader; //->GetTemplate(-1);
    if (!bGlobalTransp)
    {
      if (!(pSH->GetFlags2() & EF2_HASALPHATEST))
      {
        if (shaderItem.IsZWrite())
          continue;
      }
    }

    if (!m_Indices.m_bLocked)
    {
      rd->UpdateIndexBuffer(&m_Indices, NULL, 0, 0, false);
      pDst = (ushort *)m_Indices.m_VData;
    }
    int nStart = pMI->nFirstIndexId;
    int nEnd   = nStart + pMI->nNumIndices;
    int nTris = pMI->nNumIndices/3;

    assert(nEnd <= m_SysIndices.Num());

    sSortTris.SetUse(nStart);
    for(n=nStart; n<nEnd; n+=3)
    {
      Vec3 *v0 = (Vec3 *)&pPosPtr[pInds[n+0]*nPosPtr];
      Vec3 *v1 = (Vec3 *)&pPosPtr[pInds[n+1]*nPosPtr];
      Vec3 *v2 = (Vec3 *)&pPosPtr[pInds[n+2]*nPosPtr];
      float d0 = (*v0).GetSquaredDistance(vCam);
      float d1 = (*v1).GetSquaredDistance(vCam);
      float d2 = (*v2).GetSquaredDistance(vCam);
      SSortTri st;
      st.nTri = n;
      st.fDist = max(d2, max(d1, d0));
      sSortTris.AddElem(st);
    }
    std::sort(&sSortTris[nStart], &sSortTris[nStart+nTris], Compare);
    for(n=0; n<nTris; n++)
    {
      int m = sSortTris[n].nTri;
      pDst[nStart+n*3+0] = pInds[m+0];
      pDst[nStart+n*3+1] = pInds[m+1];
      pDst[nStart+n*3+2] = pInds[m+2];
    }
  }
  if (m_Indices.m_bLocked)
    rd->UpdateIndexBuffer(&m_Indices, NULL, 0, 0, true);
}

bool CRenderMesh::CheckUpdate(int VertFormat, int Flags, int nVerts, int nInds, int nOffsV, int nOffsI)
{
  CRenderMesh *pRM = GetVertexContainerInternal();

  bool bWasReleased = false;

  CRenderer *rd = gRenDev;

  if (!pRM->m_pVertexBuffer || (pRM->m_UpdateVBufferMask & (FMINV_STREAM | FMINV_INDICES)) || pRM->m_nVertexFormat != VertFormat)
  {
    int RequestedVertFormat = VertFormat;

    if (pRM->m_nVertexFormat == VERTEX_FORMAT_P3F_N4B_COL4UB)
      RequestedVertFormat = VERTEX_FORMAT_P3F_N4B_COL4UB;

    // Create the video buffer
    bool bCreate = true;
    if (pRM->m_pVertexBuffer)
    {
      if (RequestedVertFormat != pRM->m_nVertexFormat)
      {
        RequestedVertFormat = rd->m_RP.m_VFormatsMerge[pRM->m_nVertexFormat][RequestedVertFormat];
        if (pRM->m_nVertexFormat == RequestedVertFormat)
          bCreate = false;
        else
        {
          pRM->DestroyVidVertices(true);
          pRM->InvalidateVideoBuffer(FMINV_STREAM_MASK);
          bWasReleased = true;
          pRM->m_pVertexBuffer = NULL;
        }
      }
    }
    else
    if (pRM->m_bShort)
    {
      if (RequestedVertFormat == VERTEX_FORMAT_P3F_TEX2F)
        RequestedVertFormat = VERTEX_FORMAT_P4S_TEX2F;
      else
        RequestedVertFormat = VERTEX_FORMAT_P4S_COL4UB_TEX2F;
    }
    if (bCreate || (pRM->m_UpdateVBufferMask & (FMINV_STREAM | FMINV_INDICES)))
    {
      pRM->m_UpdateFrame = gRenDev->GetFrameID();
      if (bCreate)
        pRM->m_UpdateVBufferMask |= FMINV_STREAM;
      if (!pRM->m_pVertexBuffer)
      {
        PROFILE_FRAME(Mesh_CheckUpdateCreateGBuf);
        pRM->CreateVidVertices(pRM->m_nVertCount, RequestedVertFormat);
        pRM->m_nVertexFormat = RequestedVertFormat;
      }
      if (pRM->PrepareBufferCallback)
      {
        PROFILE_FRAME(Mesh_CheckUpdateCallback);
        if (!pRM->PrepareBufferCallback(pRM, 0))
          return false;
        bWasReleased = true;
        pRM->m_UpdateVBufferMask &= ~FMINV_INDICES;
      }
      else
      {
        assert(pRM->m_pSysVertBuffer);
        if (pRM->m_pSysVertBuffer && pRM->m_pSysVertBuffer->m_nVertexFormat != RequestedVertFormat)
        {
          PROFILE_FRAME(Mesh_CheckUpdateRecreateSystem);
          pRM->ReCreateSystemBuffer(RequestedVertFormat);
        }
      }
      if (!pRM->m_pVertexBuffer)
        return false;
    } 
    if ((pRM->m_UpdateVBufferMask & FMINV_STREAM) && pRM->m_pSysVertBuffer && pRM->m_pVertexBuffer)
    {
      PROFILE_FRAME(Mesh_CheckUpdateUpdateGBuf);
      //if (!pRM->m_bDynamic)
      pRM->UpdateVidVertices(NULL, 0, VSF_GENERAL, true);
    }
    pRM->m_UpdateVBufferMask &= ~FMINV_STREAM;
  }

  // Additional streams updating
  if (Flags & VSM_MASK)
  {
    int i;
    for (i=1; i<VSF_NUM; i++)
    {
      if (Flags & (1<<i))
      {
        if (/*!pRM->m_bDynamic && */(!pRM->m_pVertexBuffer->GetStream(i, NULL) || pRM->m_pSysVertBuffer->m_VS[i].m_bDynamic || (pRM->m_UpdateVBufferMask & (FMINV_STREAM<<i))))
        {
          pRM->m_UpdateFrame = gRenDev->GetFrameID();
          pRM->m_UpdateVBufferMask |= FMINV_STREAM << i;
				  pRM->m_pVertexBuffer->m_VS[i].m_bDynamic = pRM->m_pSysVertBuffer->m_VS[i].m_bDynamic;
          if (!pRM->m_pVertexBuffer->m_VS[i].m_bDynamic && !pRM->m_pVertexBuffer->GetStream(i, NULL))
          {
            PROFILE_FRAME(Mesh_CheckUpdateStream);
            gRenDev->CreateBuffer(pRM->m_nVertCount*m_StreamSize[i], 0, pRM->m_pVertexBuffer, i, "RenderMesh Stream", pRM->m_pSysVertBuffer->m_VS[i].m_bDynamic);
          }
          if ((pRM->m_UpdateVBufferMask & (FMINV_STREAM << i)) && pRM->m_pSysVertBuffer->m_VS[i].m_VData && pRM->m_pVertexBuffer)
          {
            PROFILE_FRAME(Mesh_CheckUpdateUpdateStream);
            pRM->UpdateVidVertices(NULL, 0, i, true);
          }
        }
        pRM->m_UpdateVBufferMask &= ~(FMINV_STREAM << i);
      }
    }
  }
  if (m_SysIndices.Num())
  {
    if (/*!pRM->m_bDynamic && */(!m_Indices.m_VData || (m_UpdateVBufferMask & FMINV_INDICES)))
    {
      PROFILE_FRAME(Mesh_CheckUpdate_UpdateInds);
      //if (!pRM->m_bDynamic)
        UpdateVidIndices(NULL, 0, true);
    }
  }

  return bWasReleased;
}

int CRenderMesh::GetAllocatedBytes(bool bVideoBuf)
{
	if (!bVideoBuf)
		return Size(0);
	else
		return Size(0x3);
	/*
  if (bVideoBuf)
  {
    int size = 0;
    if (m_pVertexBuffer)
    {
      size += m_VertexSize[m_pVertexBuffer->m_vertexformat];
      for (int i=1; i<VSF_NUM; i++)
      {
        if (m_pVertexBuffer->m_VS[i].m_VData)
          size += m_SysVertCount * m_StreamSize[i];
      }
    }
    return size;
  }
  int size = sizeof(*this);
  
  if(m_pSysVertBuffer)
    size += m_VertexSize[m_pSysVertBuffer->m_vertexformat]*m_SysVertCount;

  return size;
	*/
}

void CRenderMesh::FreeSystemBuffer()
{
	MEM_CHECK
  if (m_pSysVertBuffer) 
  {
    for (int i=0; i<VSF_NUM; i++)
    {
			char* pPtr = (char*)m_pSysVertBuffer->m_VS[i].m_VData;
      SAFE_DELETE_ARRAY(pPtr);
    }
    SAFE_DELETE(m_pSysVertBuffer);
  }
	MEM_CHECK

  SAFE_DELETE_ARRAY(m_TempTexCoords);
  SAFE_DELETE_ARRAY(m_TempColors);

	MEM_CHECK
  SAFE_DELETE_ARRAY(m_arrVtxMap);
	MEM_CHECK
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::SetSysIndicesCount( uint32 nIndices )
{
	if (m_SysIndices.Num() != nIndices)
		UpdateSysIndices(NULL,nIndices);
}

unsigned short *CRenderMesh::GetIndices(int * pIndicesCount)
{
  if (pIndicesCount)
    *pIndicesCount = m_SysIndices.Num();
	if (!m_SysIndices.empty())
		return &m_SysIndices[0];

	return 0;
}
void CRenderMesh::DestroyIndices()
{
	MEM_CHECK
  DestroyVidIndices(false);
  m_SysIndices.Free();
	MEM_CHECK
}

void CRenderMesh::DestroyVidIndices(bool bRestoreSys)
{
  MEM_CHECK
  if (gRenDev)
    gRenDev->ReleaseIndexBuffer(&m_Indices, m_nNumVidIndices);
  MEM_CHECK
}

void CRenderMesh::UpdateVidIndices(const ushort *pNewInds, int nInds, bool bReleaseSys)
{
	MEM_CHECK

  if (!gRenDev || gRenDev->CheckDeviceLost())
    return;
  int nOld = m_nNumVidIndices;
  if (!pNewInds && !nInds)
  {
    nInds = m_SysIndices.Num();
    pNewInds = &m_SysIndices[0];
  }
  m_nNumVidIndices = nInds;
  gRenDev->UpdateIndexBuffer(&m_Indices, pNewInds, nInds, nOld, true, m_bDynamic);
  m_UpdateVBufferMask &= ~0x100;
	MEM_CHECK
}

void CRenderMesh::UpdateSysIndices(const ushort *pNewInds, int nInds)
{
	uint32 dwCurrentSize = m_SysIndices.Num();
	uint32 dwThresholdSize = dwCurrentSize-dwCurrentSize/4;			// 3/4

	MEM_CHECK

  if (nInds<dwThresholdSize || nInds>dwCurrentSize)
  {
    m_SysIndices.Reserve(nInds);
  }
	MEM_CHECK
	if (pNewInds)
		cryMemcpy(m_SysIndices.begin(), pNewInds, nInds*2);

	MEM_CHECK
  InvalidateVideoBuffer(FMINV_INDICES);
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::SetVertCount( int nCount )
{
	if (nCount != m_nVertCount)
		UpdateSysVertices( NULL,nCount,m_nVertexFormat,VSF_GENERAL );
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::UpdateSysVertices(void * pNewVertices, int nNewVerticesCount, int nVertFormat, int nStream)
{
	MEM_CHECK
	if (nStream == VSF_GENERAL)
	{
		if ((nVertFormat>=0 && nVertFormat != m_nVertexFormat) || nNewVerticesCount > m_nVertCount)
		{
			m_nVertexFormat = nVertFormat;
			SAFE_DELETE(m_pSysVertBuffer);
      DestroyVidVertices(false);
		}
		if (!m_pSysVertBuffer)
			CreateSysVertices(nNewVerticesCount, m_nVertexFormat);
	}
  else
  {
    SAFE_DELETE_VOID_ARRAY((m_pSysVertBuffer->m_VS[nStream].m_VData));
    
		int nSize = m_StreamSize[nStream];
		if (m_nVertCount > 0)
			m_pSysVertBuffer->m_VS[nStream].m_VData = new byte[m_nVertCount*nSize];
  }

	int size = GetStride(nStream);
  if (m_pSysVertBuffer->m_VS[nStream].m_VData && pNewVertices)
	{
		int nVerts = min(m_nVertCount,nNewVerticesCount);
    cryMemcpy(m_pSysVertBuffer->m_VS[nStream].m_VData, pNewVertices, size*nVerts);
	}
	MEM_CHECK

  InvalidateVideoBuffer(1<<nStream);
}
void CRenderMesh::UpdateVidVertices(void * pNewVertices, int nNewVerticesCount, int nStream, bool bReleaseSys)
{
	MEM_CHECK

  bool bChanged = false;
  int nOldVCount = m_nVertCount;
  if (nNewVerticesCount > 0 && m_pSysVertBuffer && m_nVertCount != nNewVerticesCount)
  {
    for (int i=0; i<VSF_NUM; i++)
    {
      SAFE_DELETE_VOID_ARRAY(m_pSysVertBuffer->m_VS[i].m_VData);//deleting void* is undefined
    }
    m_nVertCount = nNewVerticesCount;
    bChanged = true;
  }

  if (!nNewVerticesCount && !pNewVertices)
  {
    nNewVerticesCount = m_nVertCount;
    pNewVertices = m_pSysVertBuffer->m_VS[nStream].m_VData;
  }
  if (!m_pVertexBuffer && !nStream)
  {
    m_nVertCount = nNewVerticesCount;
    CreateVidVertices(nNewVerticesCount, m_nVertexFormat);
  }
  else
  if (m_pVertexBuffer && (bChanged || !m_pVertexBuffer->GetStream(nStream, NULL)))
  {
	  MEM_CHECK
    if (bChanged)
    {
      gRenDev->ReleaseBuffer(m_pVertexBuffer, nOldVCount);
      m_pVertexBuffer = NULL;
    }
    CreateVidVertices(nNewVerticesCount, m_nVertexFormat, nStream);
	  MEM_CHECK
  }

	MEM_CHECK
  if (pNewVertices && m_pVertexBuffer)
    gRenDev->UpdateBuffer(m_pVertexBuffer, pNewVertices, nNewVerticesCount, true, 0, nStream);

  //bReleaseSys = false;
  if (bReleaseSys && !m_pSysVertBuffer->m_VS[nStream].m_bDynamic)
  {
#if defined (DIRECT3D9) || defined(OPENGL)
    assert(!pNewVertices || pNewVertices == m_pSysVertBuffer->m_VS[nStream].m_VData);
    SAFE_DELETE_VOID_ARRAY(m_pSysVertBuffer->m_VS[nStream].m_VData); //deleting void* is undefined
#endif
  }

	MEM_CHECK
}

void CRenderMesh::CreateVidVertices(int nVerts, int VertFormat, int nStream)
{
	MEM_CHECK

  if (gEnv->pSystem->IsDedicated())
		return;
  if (gRenDev->CheckDeviceLost())
    return;
  if (!nVerts && VertFormat<0)
  {
    nVerts = m_nVertCount;
    VertFormat = m_nVertexFormat;
  }
  Unlink();
#ifndef NULL_RENDERER
  if (nStream == VSF_GENERAL)
    assert(!m_pVertexBuffer);
  else
    assert(m_pVertexBuffer);
#endif
  if (nStream == VSF_GENERAL || !m_pVertexBuffer)
  {
    if (VertFormat)
      m_pVertexBuffer = gRenDev->CreateBuffer(nVerts, VertFormat, m_sSource ? m_sSource : "RenderMesh", m_bDynamic);
  }
  if (nStream != VSF_GENERAL)
  {
    int nSize = GetStride(nStream)*nVerts;
    gRenDev->CreateBuffer(nSize, VertFormat, m_pVertexBuffer, nStream, m_sSource ? m_sSource : "RenderMesh", m_pVertexBuffer->m_VS[nStream].m_bDynamic);
  }
  if (!m_pVertexBuffer)
    return;

  Link(&m_Root);
	MEM_CHECK
}

void CRenderMesh::DestroyVidVertices(bool bRestoreSys)
{
  MEM_CHECK
  if (m_pVertexBuffer)
  {
    //bRestoreSys = false;
#if defined (DIRECT3D9) || defined(OPENGL)
    if (bRestoreSys)
    {
      for (int i=0; i<VSF_NUM; i++)
      {
        if (m_pSysVertBuffer->m_VS[i].m_bDynamic)
          continue;
        if (m_pVertexBuffer->GetStream(i, NULL))
        {
          if (!m_pSysVertBuffer->GetStream(i, NULL))
          {
            int nStride = GetStride(i);
            int nSize = nStride*m_nVertCount;
            m_pSysVertBuffer->m_VS[i].m_VData = new byte[nSize];
            // Lock the buffer
            gRenDev->UpdateBuffer(m_pVertexBuffer, NULL, m_nVertCount, false, 0, i);
            assert(m_pVertexBuffer->m_VS[i].m_VData);
            if (m_pVertexBuffer->m_VS[i].m_VData)
              cryMemcpy(m_pSysVertBuffer->m_VS[i].m_VData, m_pVertexBuffer->m_VS[i].m_VData, nSize);
            // Unlock the buffer
            gRenDev->UnlockBuffer(m_pVertexBuffer, i, m_nVertCount);
          }
        }
      }
    }
#endif

    gRenDev->ReleaseBuffer(m_pVertexBuffer, m_nVertCount);
    m_pVertexBuffer = NULL;
  }
  MEM_CHECK
}

bool CRenderMesh::CreateSysVertices(int nVerts, int VertFormat, int nStream)
{
	MEM_CHECK
  if (!nVerts && VertFormat<0)
  {
    assert(m_pSysVertBuffer);
    assert (m_pVertexBuffer->GetStream(nStream, NULL));
    assert(!m_pSysVertBuffer->m_VS[nStream].m_VData);
    int nStride = GetStride(nStream);
    int nSize = nStride*m_nVertCount;
    m_pSysVertBuffer->m_VS[nStream].m_VData = new byte[nSize];
    // Lock the buffer
    gRenDev->UpdateBuffer(m_pVertexBuffer, NULL, m_nVertCount, false, 0, nStream);
    cryMemcpy(m_pSysVertBuffer->m_VS[nStream].m_VData, m_pVertexBuffer->m_VS[nStream].m_VData, nSize);
    // Unlock the buffer
    gRenDev->UnlockBuffer(m_pVertexBuffer, nStream, m_nVertCount);
  }
  else
  {
    if (!m_pSysVertBuffer)
      m_pSysVertBuffer = new CVertexBuffer;
    else
    if (m_nVertCount != nVerts)
    {
      SAFE_DELETE(m_pSysVertBuffer);
      m_pSysVertBuffer = new CVertexBuffer;
    }
    m_nVertCount = nVerts;
    m_pSysVertBuffer->m_nVertexFormat = VertFormat;
    int Size = GetStride(nStream);
    m_pSysVertBuffer->m_VS[nStream].m_VData = new byte[m_nVertCount*Size];
    memset(m_pSysVertBuffer->m_VS[nStream].m_VData, 0, m_nVertCount*Size);
  }
	MEM_CHECK

  return true;
}


void * CRenderMesh::GetSecVerticesPtr(int * pVerticesCount) 
{ 
  *pVerticesCount = m_nVertCount;
//  return m_pSysVertBuffer->m_data; 

  return m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData;
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::SetChunk( IMaterial *pNewMat,int nFirstVertId,int nVertCount,int nFirstIndexId,int nIndexCount,
														int nIndex,bool bForceInitChunk )
{
	CRenderChunk chunk;

	if (pNewMat)
		chunk.m_nMatFlags = pNewMat->GetFlags();
	
	if (nIndex < 0 || nIndex >= m_pChunks->Count())
	{
		chunk.m_nMatID = m_pChunks->Count();
	}
	else
		chunk.m_nMatID = nIndex;
	
	chunk.nFirstVertId = nFirstVertId;
	chunk.nNumVerts = nVertCount;

	chunk.nFirstIndexId = nFirstIndexId;
	chunk.nNumIndices = nIndexCount;

	SetChunk( nIndex,chunk,bForceInitChunk );
}

// add new chunk
//////////////////////////////////////////////////////////////////////////
void CRenderMesh::SetChunk( int nIndex,CRenderChunk &inChunk,bool bForceInitChunk )
{
  if(!inChunk.nNumIndices || !inChunk.nNumVerts)
    return;

  CRenderChunk * pMat = 0;

  if(nIndex < 0 || nIndex >= m_pChunks->Count())
  {
    // add new chunk
    CRenderChunk matinfo;
    m_pChunks->Add(matinfo);
    pMat = &m_pChunks->Last();

    if(m_pChunks->Count()>1 && !bForceInitChunk)
    {
      pMat->pRE = 0;
    }
    else
    {
      // TODO: fix leaks here
      pMat->pRE = (CREMesh*)gRenDev->EF_CreateRE(eDATA_Mesh);
      pMat->pRE->m_CustomTexBind[0] = m_nClientTextureBindID;
    }
    nIndex = m_pChunks->Count()-1;
  }
  else
  {
    // use present chunk
    pMat = m_pChunks->Get(nIndex);
  }

  // update chunk
  if(pMat->pRE)
  {
    pMat->pRE->m_pChunk = pMat;
    pMat->pRE->m_pRenderMesh = this;
  }

  pMat->m_nMatID = inChunk.m_nMatID;
	pMat->m_nMatFlags = inChunk.m_nMatFlags;

  assert(!pMat->pRE || pMat->pRE->m_pChunk->nFirstIndexId<60000);

  pMat->nFirstIndexId	= inChunk.nFirstIndexId;
  pMat->nNumIndices		= max(inChunk.nNumIndices,0);
  pMat->nFirstVertId	= inChunk.nFirstVertId;
  pMat->nNumVerts			= max(inChunk.nNumVerts,0);

	assert(pMat->nFirstIndexId + pMat->nNumIndices <= m_SysIndices.Num());

/*#ifdef _DEBUG
  ushort *pInds = (ushort *)m_Indices.m_VData;
  if (pInds)
  {
    for(int i=pMat->nFirstIndexId; i<pMat->nFirstIndexId+pMat->nNumIndices; i++)
    {
      int id = pInds[i];
      assert(id>=pMat->nFirstVertId && id<(pMat->nFirstVertId+pMat->nNumVerts));
    }
  }
#endif // _DEBUG*/
}

// set effector for all chunks
void CRenderMesh::SetMaterial( IMaterial * pNewMat, int nCustomTID )
{
  m_pMaterial = pNewMat;

  if (m_pChunks && nCustomTID != 0)
  {
    for(int i=0; i<m_pChunks->Count(); i++)
    {
      CRenderChunk *pChunk = m_pChunks->Get(i);
      //    pChunk->shaderItem.m_pShader = pShader;
      if(pChunk->pRE)
        pChunk->pRE->m_CustomTexBind[0] = nCustomTID;
    }
  }
}

void CRenderMesh::SetRECustomData(float * pfCustomData, float fFogScale, float fAlpha)
{
  for(int i=0; i<m_pChunks->Count(); i++)
  {
    if(m_pChunks->Get(i)->pRE)
    {
      m_pChunks->Get(i)->pRE->m_CustomData = pfCustomData;
    }
  }
}

int CVertexBuffer::Size(int Flags, int nVerts)
{
  int nSize = 0;
  if ((Flags & 3) == 0)
    nSize = sizeof(*this);
  if ((Flags & 4) == 0)
  {
    for (int i=0; i<VSF_NUM; i++)
    {
//      int nOffs;
      if (m_VS[i].m_VData )//	GetStream(i, &nOffs))
      {
        if (i == VSF_GENERAL)
          nSize += nVerts * m_VertexSize[m_nVertexFormat];
        else
          nSize += nVerts * m_StreamSize[i];
      }
    }
  }
  return nSize;
}

int CRenderChunk::Size()
{
  int nSize = sizeof(*this);
  return nSize;
}

int CRenderMesh::Size(int Flags)
{
  int nSize;
  if ((Flags & 3) == 0)
  {
    nSize = sizeof(*this);
    if (m_pSysVertBuffer)
      nSize += m_pSysVertBuffer->Size(Flags, m_nVertCount);
    if (m_pVertexBuffer)
			nSize += sizeof(*m_pVertexBuffer);
      //nSize += m_pVertexBuffer->Size(Flags, m_SysVertCount);
    if (m_TempTexCoords)
      nSize += sizeof(Vec2) * m_nVertCount;
    if (m_TempColors)
      nSize += sizeof(UCol) * m_nVertCount;
    if (m_SysIndices.Num() && !Flags)
      nSize += m_SysIndices.GetMemoryUsage();
    if (m_pChunks)
    {
      for (int i=0; i<(int)m_pChunks->capacity(); i++)
      {
        if (i < m_pChunks->Count())
          nSize += m_pChunks->Get(i)->Size();
        else
          nSize += sizeof(CRenderChunk);
      }
    }
    if (m_pChunksSkinned)
    {
      for (int i=0; i<(int)m_pChunksSkinned->capacity(); i++)
      {
        if (i < m_pChunksSkinned->Count())
          nSize += m_pChunksSkinned->Get(i)->Size();
        else
          nSize += sizeof(CRenderChunk);
      }
    }
  }
  if (Flags & 1)
  {
    nSize = 0;
    if (m_pVertexBuffer)
      nSize += m_pVertexBuffer->Size(Flags, m_nVertCount);
  }
  if (Flags & 2)
  {
    nSize = 0;
    if (m_Indices.GetStream(NULL))
      nSize += m_SysIndices.Num()*2;
  }

  return nSize;
}

//////////////////////////////////////////////////////////////////////////
byte* CRenderMesh::GetStridedPosPtr(int& Stride, int Id, bool bWrite)
{
	MEM_CHECK
  CRenderMesh *pRM = GetVertexContainerInternal();
  byte *pData = NULL;

  if (pRM->m_pSysVertBuffer && pRM->m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData)
  {
    pData = (byte *)pRM->m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData;
    Stride = m_VertexSize[pRM->m_pSysVertBuffer->m_nVertexFormat];
    if (bWrite)
      InvalidateVideoBuffer(1<<VSF_GENERAL);
  }
  else
  if (pRM->m_pVertexBuffer)
  {
    if (bWrite)
      gRenDev->UpdateBuffer(m_pVertexBuffer, NULL, m_nVertCount, false, Id, VSF_GENERAL);
    pData = (byte *)pRM->m_pVertexBuffer->m_VS[VSF_GENERAL].m_VData;
    Stride = m_VertexSize[pRM->m_pVertexBuffer->m_nVertexFormat];
    if (!pData && bWrite)
    {
      assert(0);
    }
  }
	else
		return NULL;
  /*if (m_bShort)
  {
    int nSize = m_SysVertCount * sizeof(Vec3);
    if (m_nPosOffset >= 0)
    {
      Stride = sizeof(Vec3);
      pData = &gRenDev->m_RP.m_VertPosCache.m_pBuf[m_nPosOffset];
    }
    else
    if (nSize > 0)
    {
      LinkCache(&m_RootCache);
      CRenderMesh *pRM = CRenderMesh::m_RootCache.m_PrevCache;
      while (true)
      {
        alloc_info_struct *pAI = gRenDev->GetFreeChunk(nSize, gRenDev->m_RP.m_VertPosCache.m_nBufSize, gRenDev->m_RP.m_VertPosCache.m_alloc_info, "Vert cache");
        if (pAI)
        {
          m_nPosOffset = pAI->ptr;
          byte *pD = &gRenDev->m_RP.m_VertPosCache.m_pBuf[m_nPosOffset];
          for (int i=0; i<m_SysVertCount; i++)
          {
            Vec3 *pDst = (Vec3 *)&pD[i*sizeof(Vec3)];
            Vec4sf *pSrc = (Vec4sf *)&pData[i*Stride];
            tPackB2F(*pSrc, *pDst);
            pDst->x *= m_vScale.x;
            pDst->y *= m_vScale.y;
            pDst->z *= m_vScale.z;
          }
          Stride = sizeof(Vec3);
          pData = pD;
          break;
        }
        if (pRM == &CRenderMesh::m_RootCache)
        {
          assert(0);
          break;
        }
        CRenderMesh *pNext = pRM->m_PrevCache;
        pRM->UnlinkCache();
        assert(pRM->m_nPosOffset >= 0);
        gRenDev->ReleaseChunk(pRM->m_nPosOffset, gRenDev->m_RP.m_VertPosCache.m_alloc_info);
        pRM->m_nPosOffset = -1;
        pRM = pNext;
      }
    }
  }*/

  assert(pData);

	MEM_CHECK
  return &pData[Id*Stride];
}

//////////////////////////////////////////////////////////////////////////
byte* CRenderMesh::GetStridedColorPtr(int & Stride, int Id, bool bWrite)
{
  CRenderMesh *pRM = GetVertexContainerInternal();
  SBufInfoTable * pOffs;
  byte *pData = NULL;

  if (pRM->m_pSysVertBuffer && pRM->m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData)
  {
    pData = (byte *)pRM->m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData;
    Stride = m_VertexSize[pRM->m_pSysVertBuffer->m_nVertexFormat];
    pOffs = &gBufInfoTable[pRM->m_pSysVertBuffer->m_nVertexFormat];
    if (bWrite)
      InvalidateVideoBuffer(1<<VSF_GENERAL);
  }
  else
  if (pRM->m_pVertexBuffer)
  {
    if (bWrite || !pRM->m_pVertexBuffer->m_VS[VSF_GENERAL].m_VData)
      gRenDev->UpdateBuffer(m_pVertexBuffer, NULL, m_nVertCount, false, Id, VSF_GENERAL);
    pData = (byte *)pRM->m_pVertexBuffer->m_VS[VSF_GENERAL].m_VData;
    pOffs = &gBufInfoTable[pRM->m_pVertexBuffer->m_nVertexFormat];
    Stride = m_VertexSize[pRM->m_pVertexBuffer->m_nVertexFormat];
    if (!pData && bWrite)
    {
      assert(0);
    }
  }
	else
		return NULL;
  assert(pData);
  if (pData && pOffs->OffsColor)
  {
    return &pData[Id*Stride+pOffs->OffsColor];
  }

  Stride = sizeof(UCol);
  return (byte*)&pRM->m_TempColors[Id];
}

//////////////////////////////////////////////////////////////////////////
byte* CRenderMesh::GetStridedUVPtr(int & Stride, int Id, bool bWrite)
{
  MEM_CHECK
  CRenderMesh *pRM = GetVertexContainerInternal();
  SBufInfoTable * pOffs;
  byte *pData = NULL;
  if (pRM->m_pSysVertBuffer && pRM->m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData)
  {
    pData = (byte *)pRM->m_pSysVertBuffer->m_VS[VSF_GENERAL].m_VData;
    pOffs = &gBufInfoTable[pRM->m_pSysVertBuffer->m_nVertexFormat];
    Stride = m_VertexSize[pRM->m_pSysVertBuffer->m_nVertexFormat];
    if (bWrite)
      InvalidateVideoBuffer(1<<VSF_GENERAL);
  }
  else
  if (pRM->m_pVertexBuffer)
  {
    if (bWrite)
      gRenDev->UpdateBuffer(m_pVertexBuffer, NULL, m_nVertCount, false, Id, VSF_GENERAL);
    pData = (byte *)pRM->m_pVertexBuffer->m_VS[VSF_GENERAL].m_VData;
    pOffs = &gBufInfoTable[pRM->m_pVertexBuffer->m_nVertexFormat];
    Stride = m_VertexSize[pRM->m_pVertexBuffer->m_nVertexFormat];
    if (!pData && bWrite)
    {
      assert(0);
    }
  }
	else
		return NULL;

  assert(pData);
  if (pOffs->OffsTC)
  {
    return &pData[Id*Stride+pOffs->OffsTC];
  }

  MEM_CHECK
  Stride = sizeof(Vec2);
  return (byte*)&pRM->m_TempTexCoords[Id];
}

//////////////////////////////////////////////////////////////////////////
byte* CRenderMesh::GetStridedShapePtr(int& Stride, int Id, bool bWrite)
{
	CRenderMesh *pRM = GetVertexContainerInternal();
	byte *pData = NULL;

	if (pRM->m_pSysVertBuffer && pRM->m_pSysVertBuffer->m_VS[VSF_HWSKIN_SHAPEDEFORM_INFO].m_VData)
	{
		pData = (byte *)pRM->m_pSysVertBuffer->m_VS[VSF_HWSKIN_SHAPEDEFORM_INFO].m_VData;
		if (bWrite)
			InvalidateVideoBuffer(1<<VSF_HWSKIN_SHAPEDEFORM_INFO);
	}
	else
	if (pRM->m_pVertexBuffer)
	{
		if (bWrite)
			gRenDev->UpdateBuffer(m_pVertexBuffer, NULL, m_nVertCount, false, Id, VSF_HWSKIN_SHAPEDEFORM_INFO);
		pData = (byte *)pRM->m_pVertexBuffer->m_VS[VSF_HWSKIN_SHAPEDEFORM_INFO].m_VData;
		if (!pData && bWrite)
		{
			assert(0);
		}
	}
	else
		return NULL;
	assert(pData);

	Stride = sizeof(struct_VERTEX_FORMAT_2xP3F_INDEX4UB);
	return &pData[Id*Stride];
}


//////////////////////////////////////////////////////////////////////////
byte* CRenderMesh::GetStridedHWSkinPtr(int& Stride, int Id, bool bWrite)
{
	CRenderMesh *pRM = GetVertexContainerInternal();
	byte *pData = NULL;

	if (pRM->m_pSysVertBuffer && pRM->m_pSysVertBuffer->m_VS[VSF_HWSKIN_INFO].m_VData)
	{
		pData = (byte *)pRM->m_pSysVertBuffer->m_VS[VSF_HWSKIN_INFO].m_VData;
		if (bWrite)
			InvalidateVideoBuffer(1<<VSF_HWSKIN_INFO);
	}
	else
	if (pRM->m_pVertexBuffer)
	{
		if (bWrite)
			gRenDev->UpdateBuffer(m_pVertexBuffer, NULL, m_nVertCount, false, Id, VSF_HWSKIN_INFO);
		pData = (byte *)pRM->m_pVertexBuffer->m_VS[VSF_HWSKIN_INFO].m_VData;
		if (!pData && bWrite)
		{
			assert(0);
		}
	}
	else
		return NULL;
	assert(pData);

	Stride = sizeof(struct_VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F);
	return &pData[Id*Stride];
}

//////////////////////////////////////////////////////////////////////////
byte* CRenderMesh::GetStridedBinormalPtr(int& Stride, int Id, bool bWrite)
{
  CRenderMesh *pRM = GetVertexContainerInternal();
  byte *pData = NULL;

  if (pRM->m_pSysVertBuffer && pRM->m_pSysVertBuffer->m_VS[VSF_TANGENTS].m_VData)
  {
    pData = (byte *)pRM->m_pSysVertBuffer->m_VS[VSF_TANGENTS].m_VData;
    if (bWrite)
      InvalidateVideoBuffer(1<<VSF_TANGENTS);
  }
  else
  if (pRM->m_pVertexBuffer)
  {
    if (bWrite)
      gRenDev->UpdateBuffer(m_pVertexBuffer, NULL, m_nVertCount, false, Id, VSF_TANGENTS);
    pData = (byte *)pRM->m_pVertexBuffer->m_VS[VSF_TANGENTS].m_VData;
    if (!pData && bWrite)
    {
      assert(0);
    }
  }
	else
		return NULL;
  assert(pData);

  Stride = sizeof(SPipTangents);
  return &pData[Id*Stride+sizeof(Vec4sf)];
}

//////////////////////////////////////////////////////////////////////////
byte* CRenderMesh::GetStridedTangentPtr(int& Stride, int Id, bool bWrite)
{
  CRenderMesh *pRM = GetVertexContainerInternal();
  byte *pData = NULL;
  if (pRM->m_pSysVertBuffer && pRM->m_pSysVertBuffer->m_VS[VSF_TANGENTS].m_VData)
  {
    pData = (byte *)pRM->m_pSysVertBuffer->m_VS[VSF_TANGENTS].m_VData;
    if (bWrite)
      InvalidateVideoBuffer(1<<VSF_TANGENTS);
  }
  else
  if (pRM->m_pVertexBuffer)
  {
    if (bWrite)
      gRenDev->UpdateBuffer(m_pVertexBuffer, NULL, m_nVertCount, false, Id, VSF_TANGENTS);
    pData = (byte *)pRM->m_pVertexBuffer->m_VS[VSF_TANGENTS].m_VData;
    if (!pData && bWrite)
    {
      assert(0);
    }
  }
	else
		return NULL;
  assert(pData);

  Stride = sizeof(SPipTangents);
  return &pData[Id*Stride];
}

void CRenderMesh::UnlockStream(int nStream)
{
  if (m_pVertexBuffer)
    gRenDev->UnlockBuffer(m_pVertexBuffer, nStream, m_nVertCount);
}

void CRenderMesh::AddRE(CRenderObject *obj, IShader *ef, int nList, int nAW)
{
  if (!m_nVertCount || !m_pChunks->Count())
    return;

  for(int i=0; i<(*m_pChunks).Count(); i++)
  {
    if (!(*m_pChunks)[i].pRE)
      continue;

    SShaderItem SH = m_pMaterial->GetShaderItem();
    if (ef)
      SH.m_pShader = ef;
    if (SH.m_pShader)
    {
      assert((*m_pChunks)[i].pRE->m_pChunk->nFirstIndexId<60000);

      TArray<CRendElement *> *pRE = SH.m_pShader->GetREs(SH.m_nTechnique);
      if (!pRE || !pRE->Num())
        gRenDev->EF_AddEf_NotVirtual((*m_pChunks)[i].pRE, SH, obj, nList, nAW);
      else
        gRenDev->EF_AddEf_NotVirtual(SH.m_pShader->GetREs(SH.m_nTechnique)->Get(0), SH, obj, nList, nAW);
    }
  }
}

/*
void CRenderMesh::UpdateColorInBufer(const Vec3 & vColor)
{
byte *pData = (byte *)m_pSysVertBuffer->m_data;

int nColorStride;
uchar*pColor = GetColorPtr(nColorStride);

for(int i=0; i<m_SysVertCount; i++)
{
pColor[0] = (uchar)(vColor[0]*255.0f);
pColor[1] = (uchar)(vColor[1]*255.0f);
pColor[2] = (uchar)(vColor[2]*255.0f);
pColor[3] = 255;

pColor += nColorStride;
}

if(m_pVertexBuffer)
gRenDev->ReleaseBuffer(m_pVertexBuffer);
m_pVertexBuffer=0;
}
*/

void CRenderMesh::AddRenderElements(CRenderObject * pObj, int nList, int nAW, IMaterial * pIMatInfo, int nTechniqueID)
{
  if (!pIMatInfo)
    pIMatInfo = m_pMaterial;

  if(gRenDev->m_pDefaultMaterial && gRenDev->m_pTerrainDefaultMaterial)
  {
    if(nList == EFSLIST_TERRAINLAYER && pObj->GetMatrix().GetTranslation().GetLength()>1)
    {
      if(gRenDev->m_pTerrainDefaultMaterial && pIMatInfo)
        pIMatInfo = gRenDev->m_pTerrainDefaultMaterial;
    }
    else
    {
      if(gRenDev->m_pDefaultMaterial && pIMatInfo)
        pIMatInfo = gRenDev->m_pDefaultMaterial;
    }
  }

  //assert(pIMatInfo);
  assert(m_pChunks);

  if (!GetVertexContainerInternal()->m_nVertCount || !m_pChunks->Count() || !pIMatInfo)
    return;

  for (int i=0; i<m_pChunks->Count(); i++)
  {
    CRenderChunk * pChunk = m_pChunks->Get(i);
    CREMesh * pOrigRE = pChunk->pRE;

    // get material

    SShaderItem shaderItem = pIMatInfo->GetShaderItem(pChunk->m_nMatID);

		if (nTechniqueID > 0)
			shaderItem.m_nTechnique = shaderItem.m_pShader->GetTechniqueID(shaderItem.m_nTechnique, nTechniqueID);

    if (shaderItem.m_pShader && pOrigRE)// && pMat->nNumIndices)
    {
      TArray<CRendElement *> *pREs = shaderItem.m_pShader->GetREs(shaderItem.m_nTechnique);

      assert(pOrigRE->m_pChunk->nFirstIndexId<60000);

      if (!pREs || !pREs->Num())
        gRenDev->EF_AddEf_NotVirtual(pOrigRE, shaderItem, pObj, nList, nAW);
      else
        gRenDev->EF_AddEf_NotVirtual(pREs->Get(0), shaderItem, pObj, nList, nAW);

      if(m_nClientTextureBindID && (GetPrimetiveType() == R_PRIMV_MULTI_STRIPS))
        break;
    }
  } //i
}
/*
void CRenderMesh::GenerateParticles(CRenderObject * pObj, ParticleParams * pParticleParams)
{
I3DEngine * pEng = (I3DEngine *)iSystem->GetIProcess();

// spawn particles
PipVertex * pDst = (PipVertex *)m_pSysVertBuffer->m_data;
for(int sn=0; sn<m_SysVertCount; sn++, pDst++)
{
for(int r=0; r<33 || r<99.f*rand()/RAND_MAX; r++)
sn++, pDst++;

if(sn<m_SysVertCount && pDst->nor.nz>0.5)
{
pParticleParams->vPosition.x = pDst->pos.x + pObj->m_Trans.x;
pParticleParams->vPosition.y = pDst->pos.y + pObj->m_Trans.y;
pParticleParams->vPosition.z = pDst->pos.z + pObj->m_Trans.z;
pEng->SpawnParticles( *pParticleParams );
}
}
}
*/

inline bool CompareItemFirstIndexId( const CRenderChunk &a, const CRenderChunk &b )
{
  return a.nFirstIndexId < b.nFirstIndexId;
}

void CRenderMesh::CreateDepthChunks()
{
  int i;
	m_pMergedDepthOnlyChunks = new PodArray<CRenderChunk>;

  // add only important for z-pass chunks
  PodArray<CRenderChunk> inputZChunks;
  for(i=0; i<m_pChunks->Count(); i++)
  {
    CRenderChunk & chunk = *m_pChunks->Get(i);

    if(chunk.m_nMatFlags & MTL_FLAG_NODRAW || !chunk.pRE || !chunk.nNumIndices)
      continue;

    IMaterial *pCustMat;

    if(m_pMaterial!=NULL && chunk.m_nMatID>=0 && chunk.m_nMatID < m_pMaterial->GetSubMtlCount())
      pCustMat = m_pMaterial->GetSubMtl(chunk.m_nMatID);
    else
      pCustMat = m_pMaterial;

    if(!pCustMat)
      continue;

    SShaderItem &SI = pCustMat->GetShaderItem();
    IShader *pShader = SI.m_pShader;

    if(pShader)
    {
      if(pShader->GetFlags2()&EF2_NODRAW)
        continue;

      if(pShader->GetFlags()&EF_REFRACTIVE)
        continue;

      if(pShader->GetFlags()&EF_DECAL)
        continue;

      if(!SI.IsZWrite())
        continue;
    }
    else
      continue;

    inputZChunks.Add(chunk);
  }

  // sort by first index id
  CRenderChunk *pCH = inputZChunks.Get(0);
  std::sort(pCH, pCH+inputZChunks.Count(), CompareItemFirstIndexId);

  SShaderItem PrevSI;
  int nPrevMatId=0;
	int nFirstRealIndex = 10000000;
	int nLastRealIndex = -1;

  for(i=0; i<inputZChunks.Count(); i++)
	{
		CRenderChunk & chunk = *inputZChunks.Get(i);

    bool bBreak = false;

		IMaterial *pCustMat;        
		if(m_pMaterial!=NULL && chunk.m_nMatID>=0 && chunk.m_nMatID < m_pMaterial->GetSubMtlCount())
			pCustMat = m_pMaterial->GetSubMtl(chunk.m_nMatID);
		else
			pCustMat = m_pMaterial;

    SShaderItem &SI = pCustMat->GetShaderItem();
    if (!SI.IsMergable(PrevSI))
      bBreak = true;

    // check if chunk indices are continuous
    if(i && inputZChunks.Get(i)->nFirstIndexId != (inputZChunks.Get(i-1)->nFirstIndexId + inputZChunks.Get(i-1)->nNumIndices))
      bBreak = true;

    if (bBreak)
    {
      if(nLastRealIndex-nFirstRealIndex>0)
      {
        CRenderChunk ZChunk;
        ZChunk.m_nMatID = nPrevMatId;
        ZChunk.nFirstIndexId = nFirstRealIndex;
        ZChunk.nNumIndices = nLastRealIndex-nFirstRealIndex;
        ZChunk.nFirstVertId	= 0;
        ZChunk.nNumVerts = m_nVertCount;
        ZChunk.pRE = (CREMesh*)gRenDev->EF_CreateRE(eDATA_Mesh);
        ZChunk.pRE->m_pRenderMesh = this;
        m_pMergedDepthOnlyChunks ->Add(ZChunk);
        ZChunk.pRE->m_pChunk = &m_pMergedDepthOnlyChunks->Last();
      }

      nFirstRealIndex = 10000000;
      nLastRealIndex = -1;
    }

    PrevSI = SI;
    nPrevMatId = inputZChunks.Get(i)->m_nMatID;

		nFirstRealIndex = min(nFirstRealIndex, chunk.nFirstIndexId);
		nLastRealIndex = max(nLastRealIndex, chunk.nFirstIndexId + chunk.nNumIndices);
	}

	if(nLastRealIndex-nFirstRealIndex>0)
  {
    CRenderChunk ZChunk;
    ZChunk.m_nMatID = nPrevMatId;
    ZChunk.nFirstIndexId = nFirstRealIndex;
    ZChunk.nNumIndices = nLastRealIndex-nFirstRealIndex;
    ZChunk.nFirstVertId	= 0;
    ZChunk.nNumVerts = m_nVertCount;
    ZChunk.pRE = (CREMesh*)gRenDev->EF_CreateRE(eDATA_Mesh);
    ZChunk.pRE->m_pRenderMesh = this;
    m_pMergedDepthOnlyChunks->Add(ZChunk);
    ZChunk.pRE->m_pChunk = &m_pMergedDepthOnlyChunks->Last();
  }

  // check if makes sense to use
  if(m_pMergedDepthOnlyChunks->Count() >= m_pChunks->Count())
    m_pMergedDepthOnlyChunks->Reset();
}

void CRenderMesh::ReleaseChunks(PodArray<CRenderChunk> * & pChunks)
{
	if(!pChunks)
		return;

	for(int i=0; i<pChunks->Count(); i++)
	{
		CRenderChunk *pChunk = pChunks->Get(i);
		if(pChunks->Get(i)->pRE)
			pChunks->Get(i)->pRE->Release();
	}
	delete pChunks;
	pChunks = NULL;
}

void CRenderMesh::Render(const SRendParams& rParams, CRenderObject * pObj, IMaterial *pMaterial, bool bSkinned)
{
  FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_RENDERER, g_bProfilerEnabled);

  int nList = rParams.nRenderList;
  int nAW = rParams.nAfterWater;

  if (!pMaterial)
    pMaterial = m_pMaterial;

	if(gRenDev->m_pDefaultMaterial && pMaterial)
		pMaterial = gRenDev->m_pDefaultMaterial;

  assert(pMaterial);

  if(!pMaterial)
    return;

  pObj->m_pID = rParams.pRenderNode;
  pObj->m_pWeights = rParams.pWeights;
  pObj->m_pCurrMaterial = pMaterial;

  bool bAllowDepthChunksCreation = m_pChunks->Count()>1 && CRenderer::CV_r_MergeRenderChunksForDepth;

  // update depth chunks
  if (bAllowDepthChunksCreation && !m_pMergedDepthOnlyChunks)
		CreateDepthChunks();
	if (m_pMergedDepthOnlyChunks && !CRenderer::CV_r_MergeRenderChunksForDepth)
		ReleaseChunks(m_pMergedDepthOnlyChunks);

  // check if makes sense to use
	bool bUseDepthChunks = !bSkinned &&  bAllowDepthChunksCreation && m_pMergedDepthOnlyChunks && m_pMergedDepthOnlyChunks->Count() && !pObj->m_pCharInstance && (pMaterial == m_pMaterial);

	// if valid depth chunks are present - use it for depth passes
	if (bUseDepthChunks)
		pObj->m_ObjFlags |= FOB_NO_Z_PASS;

  PodArray<CRenderChunk> *pChunks = bSkinned ? m_pChunksSkinned : m_pChunks;
 
	if (!bUseDepthChunks || !(rParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP))
	{
    const uint ni = (uint)pChunks->Count();
		for (uint i=0; i<ni; i++)     
		{
			CRenderChunk * pChunk = pChunks->Get(i);
			CRendElement * pREMesh = pChunk->pRE;

			SShaderItem shaderItem = pMaterial->GetShaderItem(pChunk->m_nMatID);
	    
			if (pREMesh && shaderItem.m_pShader && shaderItem.m_pShaderResources)
			{
        SRenderShaderResources *pR = (SRenderShaderResources *)shaderItem.m_pShaderResources;
        CShader *pS = (CShader *)shaderItem.m_pShader;
				if (pS->m_Flags2 & EF2_NODRAW)
					continue;

        if (rParams.nTechniqueID > 0)
					shaderItem.m_nTechnique = pS->GetTechniqueID(shaderItem.m_nTechnique, rParams.nTechniqueID);

				if((rParams.dwFObjFlags&FOB_RENDER_INTO_SHADOWMAP) && (pR->m_ResFlags & MTL_FLAG_NOSHADOW))
					continue;
	
				gRenDev->EF_AddEf_NotVirtual(pREMesh, shaderItem, pObj, nList, nAW);  
			}
		} 
	}

	// add depth chunks
	if (bUseDepthChunks)
	{
		pObj->m_ObjFlags &= ~FOB_NO_Z_PASS;

		if(!(rParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP))
			pObj->m_ObjFlags |= FOB_ONLY_Z_PASS;
	
		for (uint i=0; i<(uint)m_pMergedDepthOnlyChunks->Count(); i++)     
		{
			CRenderChunk * pChunk = m_pMergedDepthOnlyChunks->Get(i);
			CRendElement * pREOcLeaf = pChunk->pRE;

			SShaderItem shaderItem = pMaterial->GetShaderItem(pChunk->m_nMatID);

			if (pREOcLeaf && shaderItem.m_pShader && shaderItem.m_pShaderResources)
			{
        SRenderShaderResources *pR = (SRenderShaderResources *)shaderItem.m_pShaderResources;
        CShader *pS = (CShader *)shaderItem.m_pShader;
				if (pS->m_Flags2 & EF2_NODRAW)
        { assert(0); continue; }

				if (rParams.nTechniqueID > 0)
					shaderItem.m_nTechnique = pS->GetTechniqueID(shaderItem.m_nTechnique, rParams.nTechniqueID);

				if((rParams.dwFObjFlags&FOB_RENDER_INTO_SHADOWMAP) && (pR->m_ResFlags & MTL_FLAG_NOSHADOW))
        { assert(1); continue; }

				gRenDev->EF_AddEf_NotVirtual(pREOcLeaf, shaderItem, pObj, nList, nAW);  
			}
		} 
	}
} 

void CRenderMesh::RenderDebugLightPass(const Matrix34 & mat, const int nLightMask, const float fAlpha, const int nMaxLightNumber )
 {
  int nLightsNum = 0;
  for(int i=0; i<32; i++)
    if(nLightMask & (1<<i))
      nLightsNum++;

  CRenderObject * pObj = gRenDev->EF_GetObject(true);
  pObj->m_II.m_Matrix = mat;
	pObj->m_ObjFlags |= FOB_TRANS_SCALE | FOB_TRANS_ROTATE | FOB_TRANS_TRANSLATE;

  SShaderItem si( gRenDev->EF_LoadShaderItem( "DebugLight", true, EF_SYSTEM ) );

	//!!! The 16 are hardcoded in the CObjManager::RenderObject() function !!!
	if( nLightsNum > nMaxLightNumber || nLightsNum > 16 )
	{
		bool bBlink = (int)(gEnv->pTimer->GetCurrTime()*6)%6 > 3;
		pObj->m_II.m_AmbColor = ColorF(bBlink ? 1.f : 0.05f,bBlink ? 1.f : 0.05f, bBlink ? 0.25f : 0.f,fAlpha);
	}
	else
		pObj->m_II.m_AmbColor = ColorF(nLightsNum>=3,nLightsNum==2 ? 0.8 : 0,nLightsNum==1,fAlpha);

  for (int i=0; i<m_pChunks->Count(); i++)
  {
    CRendElement * pREOcLeaf = m_pChunks->Get(i)->pRE;
    if (pREOcLeaf)
      gRenDev->EF_AddEf_NotVirtual(pREOcLeaf, si, pObj, EFSLIST_GENERAL, 1);
  }
}

float CRenderMesh::ComputeExtent(GeomQuery& geo, EGeomForm eForm)
{
	if (eForm == GeomForm_Vertices)
		return (float)m_nVertCount;
	else
	{
		int nTris = m_SysIndices.Num()/3;
		geo.SetNumParts(nTris);
		int nPStride;
		byte* pPos = GetStridedPosPtr(nPStride);
		for (int i = 0; i < nTris; i++)
		{
			Vec3 const& v0 = *(Vec3*)(pPos+m_SysIndices[i*3+0]*nPStride);
			Vec3 const& v1 = *(Vec3*)(pPos+m_SysIndices[i*3+1]*nPStride);
			Vec3 const& v2 = *(Vec3*)(pPos+m_SysIndices[i*3+2]*nPStride);
			geo.SetPartExtent(i, TriExtent(eForm, v0, v1, v2));
		}
		return geo.GetExtent();
	}
}

void CRenderMesh::GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm)
{
	int nPStride;
	byte* pPos = GetStridedPosPtr(nPStride);

	if (eForm == GeomForm_Vertices)
	{
		int nVert = Random(m_nVertCount);
		ran.vPos = *(Vec3*)(pPos+nVert*nPStride);
		ran.vNorm = Vec3(0,0,1);
		return;
	}

	geo.GetExtent(this, eForm);
	int nTri = geo.GetRandomPart(this, eForm);

	int nIndex = nTri*3;

	// Generate interpolators for verts. Volume gen not supported (use surface).
	float t[3];
	TriRandomPoint(t, eForm);

	ran.vPos = *(Vec3*)(pPos+m_SysIndices[nIndex+0]*nPStride) * t[0]
					 + *(Vec3*)(pPos+m_SysIndices[nIndex+1]*nPStride) * t[1]
					 + *(Vec3*)(pPos+m_SysIndices[nIndex+2]*nPStride) * t[2];
	ran.vNorm = Vec3(0,0,1);
	ran.vNorm.Normalize();
}

//////////////////////////////////////////////////////////////////////////
int CRenderMesh::GetMemoryUsage( ICrySizer *pSizer,EMemoryUsageArgument nType )
{
	int nSize = 0;
	switch (nType)
	{
	case MEM_USAGE_COMBINED:
		nSize = Size(0) + Size(1) + Size(2);
		break;
	case MEM_USAGE_ONLY_SYSTEM:
		nSize = Size(0);
		break;
	case MEM_USAGE_ONLY_VIDEO:
		nSize = Size(0x3);
		break;
	case MEM_USAGE_NO_BUFFERS:
		nSize = Size(4);
		break;
	case MEM_USAGE_INDEX_BUFFERS:
		nSize = m_SysIndices.Num() ? m_SysIndices.GetMemoryUsage() : 0;
		if (nSize)
			pSizer->AddObject((void *)&m_SysIndices, nSize);
		return 0;
	default:
		if (m_pSysVertBuffer)
		{
			int vertex_format = nType - MEM_USAGE_VERTEX_BUFFERS;

			if (m_pSysVertBuffer->m_VS[0].m_VData && vertex_format == m_pSysVertBuffer->m_nVertexFormat)
			{
				pSizer->AddObject(m_pSysVertBuffer->m_VS[0].m_VData, m_nVertCount * m_VertexSize[vertex_format]);
			}

			for (int i=0; i<VSF_NUM; i++)
			{
				const static int stream_formats[] =
				{
					0,
					0, //SPipTangents,
					VERTEX_FORMAT_TEX2F,
					VERTEX_FORMAT_WEIGHTS4UB_INDICES4UB_P3F,
					VERTEX_FORMAT_COL4UB_COL4UB,
					VERTEX_FORMAT_2xP3F_INDEX4UB,
					VERTEX_FORMAT_P3F,
					VERTEX_FORMAT_SCATTER,
				};

				if (m_pSysVertBuffer->m_VS[i].m_VData && vertex_format == stream_formats[i])
				{
					pSizer->AddObject(m_pSysVertBuffer->m_VS[i].m_VData, m_nVertCount * m_StreamSize[i]);
				}
			}
		}
		return 0;
	}
	if (pSizer)
	{
		pSizer->AddObject((void *)this, nSize);
	}

  return nSize;
}

//////////////////////////////////////////////////////////////////////////
int CRenderMesh::GetTextureMemoryUsage( IMaterial *pMaterial,ICrySizer *pSizer )
{
	// If no input material use internal render mesh material.
	if (!pMaterial)
		pMaterial = m_pMaterial;
	if (!pMaterial)
		return 0;

	int textureSize = 0;
	std::set<CTexture*> used;
	for (uint i=0; i<(uint)m_pChunks->Count(); i++)
	{
		CRenderChunk * pChunk = m_pChunks->Get(i);

		// Override default material
		SShaderItem shaderItem = pMaterial->GetShaderItem(pChunk->m_nMatID);
		if (!shaderItem.m_pShaderResources)
			continue;

		SRenderShaderResources *pRes = (SRenderShaderResources *)shaderItem.m_pShaderResources;
		
		for (int i = 0; i < EFTT_MAX; i++)
		{
			if (!pRes->m_Textures[i])
				continue;

			CTexture *pTexture = pRes->m_Textures[i]->m_Sampler.m_pTex;
			if (!pTexture)
				continue;

			if (used.find(pTexture) != used.end()) // Already used in size calculation.
				continue;
			used.insert(pTexture);
			
			int nTexSize = pTexture->GetDeviceDataSize();
			textureSize += nTexSize;

			if (pSizer)
				pSizer->AddObject( pTexture,nTexSize );
		}
	}

	return textureSize;
}

float CRenderMesh::GetAverageTrisNumPerChunk(IMaterial * pMat)
{
	float fTrisNum = 0;
	float fChunksNum = 0;

	for(int m=0; m<GetChunks()->Count(); m++)
	{
		CRenderChunk & chunk = *GetChunks()->Get(m);
		if(chunk.m_nMatFlags & MTL_FLAG_NODRAW || !chunk.pRE)
			continue;

		IMaterial *pCustMat;        
		if(pMat && chunk.m_nMatID>=0 && chunk.m_nMatID < pMat->GetSubMtlCount())
			pCustMat = pMat->GetSubMtl(chunk.m_nMatID);
		else
			pCustMat = pMat;

		if(!pCustMat)
			continue;

		IShader * pShader = pCustMat->GetShaderItem().m_pShader;

		if(!pShader)
			continue;

		if(pShader->GetFlags2()&EF2_NODRAW)
			continue;

		fTrisNum += chunk.nNumIndices/3;
		fChunksNum ++;
	}

	return fChunksNum ? (fTrisNum/fChunksNum) : 0;
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::DebugDraw( const Matrix34 &mat,int nFlags,uint32 nVisibleChunksMask,ColorB *pColor )
{
	IRenderAuxGeom *pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	bool bNoCull = false;
	bool bNoLines = false;
	SAuxGeomRenderFlags prevRenderFlags = pRenderAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags = prevRenderFlags;
	renderFlags.SetAlphaBlendMode( e_AlphaBlended );
	if (nFlags & 1)
	{
		bNoCull = true;
		renderFlags.SetCullMode(e_CullModeNone);
	}
	if (nFlags & 2)
	{
		bNoLines = true;
	}
	pRenderAuxGeom->SetRenderFlags(renderFlags);

	ColorB lineColor(255,255,0,255);
	if (bNoCull)
	{
		lineColor = ColorB(255,0,255,255);
	}
	if (pColor)
		lineColor = *pColor;
	ColorB c;

	for (uint32 i=0; i<(uint32)m_pChunks->Count(); i++)     
	{
		CRenderChunk * pChunk = m_pChunks->Get(i);
		
		if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
			continue;

		if (!((1<<i) & nVisibleChunksMask))
			continue;

		const int nVertCount = GetVertCount();
		int nIndCount = 0, nPosStride=0;
		const ushort * pIndices = GetIndices(&nIndCount);
		const byte * pPositions = GetStridedPosPtr(nPosStride,0,false);

		uint32 nFirstIndex = pChunk->nFirstIndexId;
		uint32 nLastIndex = pChunk->nFirstIndexId + pChunk->nNumIndices;
    int nIndexStep = (GetPrimetiveType() == R_PRIMV_MULTI_STRIPS) ? 1 : 3;
		for (uint32 i = nFirstIndex; i < nLastIndex-2; i+=nIndexStep)
		{
      int32 I0	=	pIndices[i];
      int32 I1	=	pIndices[i+1];
      int32 I2	=	pIndices[i+2];
      if(nIndexStep==1 && (i&1))
      {
        I1	=	pIndices[i+2];
        I2	=	pIndices[i+1];
      }
      assert(I0<GetVertCount());
      assert(I1<GetVertCount());
      assert(I2<GetVertCount());

			Vec3 v0 = mat.TransformPoint( *(Vec3*)&pPositions[nPosStride*I0] );
			Vec3 v1 = mat.TransformPoint( *(Vec3*)&pPositions[nPosStride*I1] );
			Vec3 v2 = mat.TransformPoint( *(Vec3*)&pPositions[nPosStride*I2] );
			//uint32 clr = 150 + i&63;
			uint32 clr = 220;
			if (bNoCull)
				c = ColorB(clr-50,0,clr,120);
			else
				c = ColorB(clr,clr,clr,120);
			
			if (pColor)
			{
				c = *pColor;
			}
			pRenderAuxGeom->DrawTriangle( v0,c,v1,c,v2,c );

			if (!bNoLines)
			{
				pRenderAuxGeom->DrawLine( v0,lineColor,v1,lineColor );
				pRenderAuxGeom->DrawLine( v1,lineColor,v2,lineColor );
				pRenderAuxGeom->DrawLine( v2,lineColor,v0,lineColor );
			}
		}
	}
	pRenderAuxGeom->SetRenderFlags(prevRenderFlags);
}

alloc_info_struct *CRenderer::GetFreeChunk(int bytes_count, int nBufSize, PodArray<alloc_info_struct>& alloc_info, const char *szSource)
{
  int best_i = -1;
  int min_size = 10000000;

  // find best chunk
  for(int i=0; i<alloc_info.Count(); i++)
  {
    if(!alloc_info[i].busy)
    {
      if(alloc_info[i].bytes_num >= bytes_count)
      {
        if(alloc_info[i].bytes_num < min_size)
        {
          best_i = i;
          min_size = alloc_info[i].bytes_num;
        }
      }
    }
  }

  if(best_i>=0)
  { // use best free chunk
    alloc_info[best_i].busy = true;
    alloc_info[best_i].szSource = szSource;

    int bytes_free = alloc_info[best_i].bytes_num - bytes_count;
    if(bytes_free>0)
    { 
      // modify reused shunk
      alloc_info[best_i].bytes_num = bytes_count;

      // insert another free shunk
      alloc_info_struct new_chunk;
      new_chunk.bytes_num = bytes_free;
      new_chunk.ptr = alloc_info[best_i].ptr + alloc_info[best_i].bytes_num;
      new_chunk.busy = false;

      if(best_i < alloc_info.Count()-1) // if not last
        alloc_info.InsertBefore(new_chunk, best_i+1);
      else
        alloc_info.Add(new_chunk);
    }

    return &alloc_info[best_i];
  }

  int res_ptr = 0;

  int piplevel = alloc_info.Count() ? (alloc_info.Last().ptr - alloc_info[0].ptr) + alloc_info.Last().bytes_num : 0;
  if(piplevel + bytes_count >= nBufSize)
    return NULL;
  else
    res_ptr = piplevel;

  // register new chunk
  alloc_info_struct ai;
  ai.ptr = res_ptr;
  ai.szSource = szSource;
  ai.bytes_num  = bytes_count;
  ai.busy = true;
  alloc_info.Add(ai);

  return &alloc_info[alloc_info.Count()-1];
}

bool CRenderer::ReleaseChunk(int p, PodArray<alloc_info_struct>& alloc_info)
{
  for(int i=0; i<alloc_info.Count(); i++)
  {
    if(alloc_info[i].ptr == p)
    {
      alloc_info[i].busy = false;

      // delete info about last unused chunks
      while(alloc_info.Count() && alloc_info.Last().busy == false)
        alloc_info.Delete(alloc_info.Count()-1);

      // merge unused chunks
      for(int s=0; s<alloc_info.Count()-1; s++)
      {
        assert(alloc_info[s].ptr < alloc_info[s+1].ptr);

        if(alloc_info[s].busy == false)
        {
          if(alloc_info[s+1].busy == false)
          {
            alloc_info[s].bytes_num += alloc_info[s+1].bytes_num;
            alloc_info.Delete(s+1);
            s--;
          }
        }
      }

      return true;
    }
  }

  return false;
}

void CRenderMesh::UpdateBBoxFromMesh()
{
  PROFILE_FRAME(UpdateBBoxFromMesh);

  AABB aabb; 
  aabb.Reset();

  int nVertCount = GetVertexContainerInternal()->GetVertCount();
  int nIndCount = 0, nPosStride=0;
  const byte * pPositions = GetStridedPosPtr(nPosStride,0,false);
  const ushort * pIndices = GetIndices(&nIndCount);

  if(!pIndices || !pPositions)
  {
    assert(!"Mesh is not ready");
    return;
  }

  for (uint32 i=0; i<(uint32)m_pChunks->Count(); i++)     
  {
    CRenderChunk * pChunk = m_pChunks->Get(i);

    if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
      continue;

    uint32 nFirstIndex = pChunk->nFirstIndexId;
    uint32 nLastIndex = pChunk->nFirstIndexId + pChunk->nNumIndices;

    for (uint32 i = nFirstIndex; i < nLastIndex; i++)
    {
      uint32 I0	=	pIndices[i];
      if(I0 < nVertCount)
      {
        Vec3 v0 = *(Vec3*)&pPositions[nPosStride*I0];
        aabb.Add(v0);
      }
      else
        assert(!"Index is out of range");
    }
  }

  if(!aabb.IsReset())
  {
    m_vBoxMax = aabb.max;
    m_vBoxMin = aabb.min;
  }
}
