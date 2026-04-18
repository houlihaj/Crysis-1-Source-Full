////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   RenderMesh.h
//  Version:     v1.00
//  Created:     11/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __RenderMesh_h__
#define __RenderMesh_h__
#pragma once

struct CRenderMesh : public IRenderMesh
{
	//! constructor
	//! /param szSource this pointer is stored - make sure the memory stays
	CRenderMesh( const char *szType,const char *szSourceName );

	//! destructor
	~CRenderMesh();

	virtual void AddRef() { m_nRefCounter++; }
	virtual int Release()
	{
		if (--m_nRefCounter <= 0)
    {
			delete this;
      return 0;
    }

    return m_nRefCounter;
	}
  void ReleaseForce()
  {
    while (true)
    {
      int nRef = Release();
      if (nRef <= 0)
        return;
    }
  }

	virtual const char* GetTypeName() { return m_sType; };
	virtual const char* GetSourceName() { return m_sSource; };

	// --------------------------------------------------------------

	int m_nRefCounter;
	static CRenderMesh      m_Root;

	CRenderMesh *           m_Next;           //!<
	CRenderMesh *           m_Prev;           //!<
	const char*             m_sType;          //!< pointer to the type name in the constructor call
	const char*             m_sSource;        //!< pointer to the source  name in the constructor call
	_smart_ptr<IMaterial>   m_pMaterial;      // Default material for this render mesh.

	_inline void Unlink()
	{
		if (!m_Next || !m_Prev)
			return;
		m_Next->m_Prev = m_Prev;
		m_Prev->m_Next = m_Next;
		m_Next = m_Prev = NULL;
	}
	_inline void Link( CRenderMesh* Before )
	{
		if (m_Next || m_Prev)
			return;
		m_Next = Before->m_Next;
		Before->m_Next->m_Prev = this;
		Before->m_Next = this;
		m_Prev = Before;
	}

	static CRenderMesh      m_RootGlobal;             //!<
	CRenderMesh *           m_NextGlobal;             //!<
	CRenderMesh *           m_PrevGlobal;             //!<

	_inline void UnlinkGlobal()
	{
		if (!m_NextGlobal || !m_PrevGlobal)
			return;
		m_NextGlobal->m_PrevGlobal = m_PrevGlobal;
		m_PrevGlobal->m_NextGlobal = m_NextGlobal;
		m_NextGlobal = m_PrevGlobal = NULL;
	}
	_inline void LinkGlobal( CRenderMesh* Before )
	{
		if (m_NextGlobal || m_PrevGlobal)
			return;
		m_NextGlobal = Before->m_NextGlobal;
		Before->m_NextGlobal->m_PrevGlobal = this;
		Before->m_NextGlobal = this;
		m_PrevGlobal = Before;
	}

  static CRenderMesh      m_RootCache;              //!<
  CRenderMesh *           m_NextCache;             //!<
  CRenderMesh *           m_PrevCache;             //!<
  _inline void UnlinkCache()
  {
    if (!m_NextCache || !m_PrevCache)
      return;
    m_NextCache->m_PrevCache = m_PrevCache;
    m_PrevCache->m_NextCache = m_NextCache;
    m_NextCache = m_PrevCache = NULL;
  }
  _inline void LinkCache( CRenderMesh* Before )
  {
    if (m_NextCache || m_PrevCache)
      return;
    m_NextCache = Before->m_NextCache;
    Before->m_NextCache->m_PrevCache = this;
    Before->m_NextCache = this;
    m_PrevCache = Before;
  }


	Vec2 *              m_TempTexCoords;              //!<
	UCol *              m_TempColors;                 //!<

	void *              m_pCustomData;                //!< userdata, useful for PrepareBufferCallback, currently this is used only for CDetailGrass

	bool (*PrepareBufferCallback)(IRenderMesh *Data, bool bNeedTangents);

	_inline void SetVertexContainer(IRenderMesh *pBuf)
	{
		if(m_pVertexContainer)
			((CRenderMesh *)m_pVertexContainer)->m_lstVertexContainerUsers.Delete(this);

		m_pVertexContainer = (CRenderMesh *)pBuf;

		if(m_pVertexContainer && ((CRenderMesh *)m_pVertexContainer)->m_lstVertexContainerUsers.Find(this)<0)
			((CRenderMesh *)m_pVertexContainer)->m_lstVertexContainerUsers.Add(this);
	}

	IRenderMesh* GetVertexContainer()
	{
		if (m_pVertexContainer)
			return m_pVertexContainer;
		return this;
	}
	CRenderMesh* GetVertexContainerInternal()
	{
		if (m_pVertexContainer)
			return m_pVertexContainer;
		return this;
	}
  inline int GetStride(int nStream)
  {
    int size;
    if (nStream == VSF_GENERAL)
      size = m_VertexSize[m_nVertexFormat];
    else
      size = m_StreamSize[nStream];
    return size;
  }

	virtual byte *GetStridedPosPtr(int& Stride, int Id=0, bool bWrite=false);
  virtual byte *GetStridedColorPtr(int & Stride, int Id=0, bool bWrite=false);
  virtual byte *GetStridedUVPtr(int & Stride, int Id=0, bool bWrite=false);


	virtual byte *GetStridedHWSkinPtr(int& Stride, int Id=0, bool bWrite=false);
	virtual byte *GetStridedShapePtr(int& Stride, int Id=0, bool bWrite=false);

	virtual byte *GetStridedBinormalPtr(int& Stride, int Id=0, bool bWrite=false);
	virtual byte *GetStridedTangentPtr(int& Stride, int Id=0, bool bWrite=false);

  virtual void UnlockStream(int nStream);

	CVertexBuffer *         m_pVertexBuffer;            //!< video memory

	CRenderMesh *           m_pVertexContainer;         //!<
	PodArray<CRenderMesh*>  m_lstVertexContainerUsers;

	uint                    m_bMaterialsWasCreatedInRenderer : 1;
	uint                    m_bDynamic : 1;
  uint                    m_bShort : 1;

  int                     m_nPosOffset;
  Vec3                    m_vScale;
	uint                    m_UpdateVBufferMask;        //!<
	uint                    m_UpdateFrame;
	uint                    m_SortFrame;								//!< to prevent unneccessary sorting during one frame
	int											m_nVertCount;					      //!< number of vertices Render Mesh
	CVertexBuffer *					m_pSysVertBuffer;						//!< system memory

	SVertexStream           m_Indices;
	TArray <ushort>         m_SysIndices;
	int                     m_nNumVidIndices;

	int                     m_nVertexFormat;
	int                     m_nPrimetiveType;           //!< R_PRIMV_TRIANGLES, R_PRIMV_TRIANGLE_STRIP, R_PRIMV...

	PodArray<CRenderChunk> *		m_pChunks;			  					//!<
  PodArray<CRenderChunk> *		m_pChunksSkinned;			  					//!<
	PodArray<CRenderChunk> *		m_pMergedDepthOnlyChunks;									//!<

	uint *                  m_arrVtxMap;                //!< [Anton] mapping table RenderMesh vertex idx->original vertex idx
	_smart_ptr<CRenderMesh> m_pMorphBuddy;

  Vec3                    m_vBoxMin, m_vBoxMax;             //!< used for hw occlusion test


	virtual void AddRE(CRenderObject* pObj, IShader* pEf, int nList=0, int nAW=1);

	void SaveTexCoords(byte *pD, SBufInfoTable *pOffs, int Size);
	void SaveColors(byte *pD, SBufInfoTable *pOffs, int Size);

	void SortTris();

	bool ReCreateSystemBuffer(int VertFormat);
	void UpdateDynBufPtr(int VertFormat);
	bool CheckUpdate(int VertFormat, int Flags, int nVerts, int nInds, int nOffsV, int nOffsI);
	virtual void InvalidateVideoBuffer(int flags=-1) { m_UpdateVBufferMask |= flags; };
	virtual void CopyTo(IRenderMesh *pDst, bool bUseSysBuf, int nAppendVtx=0);

	virtual void SetMesh( CMesh &mesh,int nSecColorsSetOffset=0, uint32 flags=0, Vec3 *pBSStreamTemp=0);
	virtual void SetSkinningDataVegetation( struct SMeshBoneMapping *pBoneMapping, Vec3 *pBSStreamTemp=0);
	virtual void SetSkinningDataCharacter(CMesh& mesh, struct SMeshBoneMapping *pBoneMapping, Vec3 *pBSStreamTemp=0);
	virtual IIndexedMesh *GetIndexedMesh(IIndexedMesh *pIdxMesh=0);

	virtual int GetAllocatedBytes( bool bVideoMem );

	virtual void Unload(bool bRestoreSys);

	virtual void AddRenderElements(CRenderObject * pObj=0, int nList=EFSLIST_GENERAL, int nAW=1, IMaterial * pIMatInfo=NULL, int nTechniqueID=0);

	virtual void SetSysIndicesCount( uint32 nIndices );
	virtual uint32 GetSysIndicesCount() { return m_SysIndices.Num(); }

	virtual unsigned short *GetIndices(int *pIndicesCount);
	virtual void DestroyIndices();
  void DestroyVidIndices(bool bRestoreSys);

	void UpdateVidIndices(const ushort *pNewInds, int nInds, bool bReleaseSys);
	virtual void UpdateSysIndices(const ushort *pNewInds, int nInds);

	virtual void UpdateSysVertices(void * pNewVertices, int nNewVerticesCount, int nVertFormat, int nStream);
	virtual void UpdateVidVertices(void * pNewVertices, int nNewVerticesCount, int nStream, bool bReleaseSys);
	virtual bool CreateSysVertices(int nVerts=0, int VertFormat=-1, int nStream=VSF_GENERAL);

	void CreateVidVertices(int nVerts=0, int VertFormat=-1, int nStream=VSF_GENERAL);
  void DestroyVidVertices(bool bRestoreSys);

	virtual void SetRECustomData(float * pfCustomData, float fFogScale=0, float fAlpha=1);

	virtual void SetChunk( int nIndex,CRenderChunk &chunk,bool bForceInitChunk=false );
	virtual void SetChunk( IMaterial *pNewMat,int nFirstVertId,int nVertCount,int nFirstIndexId,int nIndexCount,int nIndex = 0,bool bForceInitChunk=false );

	int                 m_nClientTextureBindID;

	virtual void SetMaterial( IMaterial * pNewMat, int nCustomTID = 0 );

	virtual void   * GetSecVerticesPtr(int * pVerticesCount);

	//! /param Flags ????
	//! /return memory consumption of the object in bytes
	int Size( int Flags );

	virtual void AllocateSystemBuffer( int nVertCount );
	virtual void FreeSystemBuffer( void );

	virtual void DrawImmediately( void );
	/*  bool LoadMaterial(int m, 
	const char *szFileName, const char *szFolderName, 
	PodArray<CRenderChunk> & lstMatTable, IRenderer * pRenderer,
	MAT_ENTITY * me, bool bFake);*/
	static int SetTexType(struct TextureMap3 *tm);
	bool IsEmpty() { return !m_nVertCount	|| !m_pSysVertBuffer || !m_SysIndices.Num(); }
	virtual void RenderDebugLightPass(const Matrix34 & mat, const int nLightMask, const float fAlpha, const int nMaxLightNumber);
	virtual void Render(const struct SRendParams & rParams, CRenderObject * pObj, IMaterial *pMaterial, bool bSkinned=false);
	virtual void SetVertCount( int nCount );
	virtual int GetVertCount() { return m_nVertCount; }
	virtual PodArray<CRenderChunk> *	GetChunks() { return m_pChunks;	}
  virtual PodArray<CRenderChunk> *	GetChunksSkinned() { return m_pChunksSkinned;	}
  virtual void CreateChunksSkinned();
	CVertexBuffer * GetSysVertBuffer() { return m_pSysVertBuffer; }
	void * GetCustomData() { return m_pCustomData; }
	CVertexBuffer * GetVideoVertBuffer() { return m_pVertexBuffer; }
	void SetBBox(const Vec3 & vBoxMin, const Vec3 & vBoxMax) { m_vBoxMin = vBoxMin; m_vBoxMax = vBoxMax; }
	void GetBBox(Vec3 & vBoxMin, Vec3 & vBoxMax) { vBoxMin = m_vBoxMin; vBoxMax = m_vBoxMax; };
  void UpdateBBoxFromMesh();
	IMaterial* GetMaterial() { return m_pMaterial; };
	uint GetUpdateFrame() { return m_UpdateFrame; }
	void SetUpdateFrame(uint nUpdateFrame) { m_UpdateFrame = nUpdateFrame; }
	bool IsMaterialsWasCreatedInRenderer() { return m_bMaterialsWasCreatedInRenderer; }
	int	GetPrimetiveType() { return m_nPrimetiveType; }
	int GetVertexFormat() { return m_nVertexFormat; }
	uint * GetPhysVertexMap() { return m_arrVtxMap; }
	void SetPhysVertexMap(uint * pVtxMap) { m_arrVtxMap = pVtxMap; }
	void SetMorphBuddy(IRenderMesh *pMorph) { m_pMorphBuddy = (CRenderMesh*)pMorph; }
	IRenderMesh *GetMorphBuddy() { return m_pMorphBuddy; }
	IRenderMesh *GenerateMorphWeights() {
		if (!m_pMorphBuddy)
			return 0;
		Vec2 *pWeights; memset(pWeights=new Vec2[GetVertCount()], 0, GetVertCount()*sizeof(Vec2));
		IRenderMesh *pWeightsMesh = gRenDev->CreateRenderMeshInitialized(pWeights,GetVertCount(),VERTEX_FORMAT_TEX2F,0,0,R_PRIMV_TRIANGLES,"MorphWeights","MorphWeights");
		delete[] pWeights;
		return pWeightsMesh;
	}

	virtual float ComputeExtent(GeomQuery& geo, EGeomForm eForm);
	virtual void GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm);

	virtual void DebugDraw( const Matrix34 &mat,int nFlags,uint32 nVisibleChunksMask=-1,ColorB *pColor=0 );
	// Return memory usage.
	virtual int GetMemoryUsage( ICrySizer *pSizer,EMemoryUsageArgument nType );

	// Caluclates texture memory usage of this render mesh for specified material.
	virtual int GetTextureMemoryUsage( IMaterial *pMaterial,ICrySizer *pSizer=NULL );
	virtual float GetAverageTrisNumPerChunk(IMaterial * pMat);
	void CreateDepthChunks();
	void ReleaseChunks(PodArray<CRenderChunk> * & pChunks);

  static void ShutDown();
};

#endif // __RenderMesh_h__