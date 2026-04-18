
/*=============================================================================
RenderPipeline.h : Shaders pipeline declarations.
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
  * Created by Honitch Andrey
  
=============================================================================*/

#ifndef __RENDERPIPELINE_H__
#define __RENDERPIPELINE_H__

//====================================================================

#define MAX_HWINST_PARAMS 32768

#define MAX_REND_RECURSION_LEVELS 4
#define MAX_REND_OBJECTS 16384
#define MAX_REND_SHADERS 4096
#define MAX_REND_SHADER_RES 16384
#define MAX_REND_LIGHTS  32
#define MAX_SORT_GROUPS  64
#define MAX_REND_LIGHT_GROUPS (MAX_REND_LIGHTS/4)
#define MAX_INSTANCES_THRESHOLD_HW  8
#define MAX_LIST_ORDER   2


typedef union UnINT64
{
  uint64 SortVal;
  struct
  {
    uint Low;
    uint High;
  }i;
} UnINT64;

struct SGroupProperty
{
  uint32 m_nBatchMask;
  int m_nSortGroups;
  SGroupProperty()
  {
    m_nSortGroups = 0;
    m_nBatchMask = 0;
  }
};

struct SRendLightGroup
{
  DynArray<uint> RendItemsShadows[4];
  DynArray<uint> RendItemsLights;

  uint m_GroupLightMask;
  SRendLightGroup()
  {
    Reset();
  }

  void Reset()
  {
    for (int i=0; i<4; i++)
    {
      RendItemsShadows[i].resize(0);
    }
    RendItemsLights.resize(0);
    m_GroupLightMask = 0;
  }
};

struct SRendItem
{  
  uint32 SortVal;
  CRendElement *Item;
  union
  {
    uint32 ObjSort;
    float fDist;
  };
  CRenderObject *pObj;
  uint32 DynLMask;
  uint32 nBatchFlags;

  //==================================================
  static void *mfGetPointerCommon(ESrcPointer ePT, int *Stride, EParamType Type, ESrcPointer Dst, int Flags);

  static _inline void mfAdd(CRendElement *Item, CRenderObject *pObj, const SShaderItem& pSH, uint nList, int nAW, uint32 nBatchFlags)
  {
    SRendItem* ri = m_RendItems[nAW][nList].AddIndex(1);
    ri->pObj = pObj;
    ri->ObjSort = (pObj->m_ObjFlags & 0xffff0000) | pObj->m_nRAEId;
    ri->DynLMask = pObj->m_DynLMMask;
    assert (!(pSH.m_nPreprocessFlags & 0xffff));
    ri->nBatchFlags = nBatchFlags | pSH.m_nPreprocessFlags;
    int nResID = pSH.m_pShaderResources ? ((SRenderShaderResources *)(pSH.m_pShaderResources))->m_Id : 0;

		assert(nResID<CShader::m_ShaderResources_known.size());

    CShader *pShader = (CShader *)pSH.m_pShader;
    ri->SortVal = (nResID<<18) | (pShader->mfGetID()<<6) | (pSH.m_nTechnique&0x3f);
    ri->Item = Item;
  }
  static _inline void mfGet(uint32 nVal, int& nTechnique, CShader*& Shader, SRenderShaderResources*& Res)
  {
    Shader = (CShader *)CShaderMan::m_pContainer->m_RList[(nVal>>6) & (MAX_REND_SHADERS-1)];
    nTechnique = (nVal & 0x3f);
    if (nTechnique == 0x3f)
      nTechnique = -1;
    int nRes = (nVal>>18) & (MAX_REND_SHADER_RES-1);
    Res = (nRes) ? CShader::m_ShaderResources_known[nRes] : NULL;
  }
  static _inline CShader *mfGetShader(uint flag)
  {
    return (CShader *)CShaderMan::m_pContainer->m_RList[(flag>>6) & (MAX_REND_SHADERS-1)];
  }
  static bool IsListEmpty(int nList)
  {
    int nR = SRendItem::m_RecurseLevel-1;
    int nREs = SRendItem::m_EndRI[nR][0][nList] - SRendItem::m_StartRI[nR][0][nList];
    nREs += SRendItem::m_EndRI[nR][1][nList] - SRendItem::m_StartRI[nR][1][nList];
     
    if (!nREs)
      return true;
    return false;
  }
  static uint32 BatchFlags(int nList)
  {
    int nR = SRendItem::m_RecurseLevel-1;
    uint32 nBatchFlags = 0;
    int nREs = SRendItem::m_EndRI[nR][0][nList] - SRendItem::m_StartRI[nR][0][nList];
    if (nREs)
      nBatchFlags |= SRendItem::m_GroupProperty[nR+1][0][nList].m_nBatchMask;
    nREs = SRendItem::m_EndRI[nR][1][nList] - SRendItem::m_StartRI[nR][1][nList];
    if (nREs)
      nBatchFlags |= SRendItem::m_GroupProperty[nR+1][1][nList].m_nBatchMask;
    return nBatchFlags;
  }

  // Sort by SortVal member of RI
  static void mfSortPreprocess(SRendItem *First, int Num, int nList, int nAW);
  // Sort by distance
  static void mfSortByDist(SRendItem *First, int Num, int nList, int nAW, bool bDecals);
  // Sort by light
  static void mfSortByLight(SRendItem *First, int Num, int nList, bool bSort, const bool bDirectLightsOnly, const bool bIgnoreRePtr, bool bSortDecals, int nAW);

  static int m_RecurseLevel;
  static int m_StartRI[MAX_REND_RECURSION_LEVELS][MAX_LIST_ORDER][EFSLIST_NUM];
  static int m_EndRI[MAX_REND_RECURSION_LEVELS][MAX_LIST_ORDER][EFSLIST_NUM];
  static TArray<SRendItem> m_RendItems[MAX_LIST_ORDER][EFSLIST_NUM];
  static SRendLightGroup m_RenderLightGroups[MAX_REND_RECURSION_LEVELS][EFSLIST_NUM][MAX_LIST_ORDER][MAX_SORT_GROUPS][MAX_REND_LIGHT_GROUPS+1];
  static uint m_ShadowsValidMask[MAX_REND_RECURSION_LEVELS][MAX_REND_LIGHT_GROUPS];

  static SGroupProperty m_GroupProperty[MAX_REND_RECURSION_LEVELS][MAX_LIST_ORDER][EFSLIST_NUM];
};

struct SRefSprite
{
  CRenderObject *m_pObj;
};

//==================================================================

struct SShaderPass;

union UPipeVertex
{
  void               *Ptr;
  byte               *PtrB;
  float              *PtrF;

  Vec3                                    *VBPtr_0;
  struct_VERTEX_FORMAT_P3F                *VBPtr_1;
  struct_VERTEX_FORMAT_P3F_COL4UB         *VBPtr_2;
  struct_VERTEX_FORMAT_P3F_TEX2F          *VBPtr_3;
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F   *VBPtr_4;
  struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F *VBPtr_5;
  SPipTangents                            *VBPtr_6;
  struct_VERTEX_FORMAT_TEX2F              *VBPtr_7;
  struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F *VBPtr_8;
};

//==================================================================

#define MAX_DYNVBS 4

//==================================================================
// StencilStates

#define FSS_STENCFUNC_ALWAYS   0x0
#define FSS_STENCFUNC_NEVER    0x1
#define FSS_STENCFUNC_LESS     0x2
#define FSS_STENCFUNC_LEQUAL   0x3
#define FSS_STENCFUNC_GREATER  0x4
#define FSS_STENCFUNC_GEQUAL   0x5
#define FSS_STENCFUNC_EQUAL    0x6
#define FSS_STENCFUNC_NOTEQUAL 0x7
#define FSS_STENCFUNC_MASK     0x7

#define FSS_STENCIL_TWOSIDED   0x8

#define FSS_CCW_SHIFT          16

#define FSS_STENCOP_KEEP    0x0
#define FSS_STENCOP_REPLACE 0x1
#define FSS_STENCOP_INCR    0x2
#define FSS_STENCOP_DECR    0x3
#define FSS_STENCOP_ZERO    0x4
#define FSS_STENCOP_INCR_WRAP 0x5
#define FSS_STENCOP_DECR_WRAP 0x6

#define FSS_STENCFAIL_SHIFT   4
#define FSS_STENCFAIL_MASK    (0x7 << FSS_STENCFAIL_SHIFT)

#define FSS_STENCZFAIL_SHIFT  8
#define FSS_STENCZFAIL_MASK   (0x7 << FSS_STENCZFAIL_SHIFT)

#define FSS_STENCPASS_SHIFT   12
#define FSS_STENCPASS_MASK    (0x7 << FSS_STENCPASS_SHIFT)

#define STENC_FUNC(op) (op)
#define STENC_CCW_FUNC(op) (op << FSS_CCW_SHIFT)
#define STENCOP_FAIL(op) (op << FSS_STENCFAIL_SHIFT)
#define STENCOP_ZFAIL(op) (op << FSS_STENCZFAIL_SHIFT)
#define STENCOP_PASS(op) (op << FSS_STENCPASS_SHIFT)
#define STENCOP_CCW_FAIL(op) (op << (FSS_STENCFAIL_SHIFT+FSS_CCW_SHIFT))
#define STENCOP_CCW_ZFAIL(op) (op << (FSS_STENCZFAIL_SHIFT+FSS_CCW_SHIFT))
#define STENCOP_CCW_PASS(op) (op << (FSS_STENCPASS_SHIFT+FSS_CCW_SHIFT))

//==================================================================

#if defined (DIRECT3D9) || defined (OPENGL)
struct SD3DVertexDeclaration
{
  TArray<D3DVERTEXELEMENT9> m_Declaration;  
  LPDIRECT3DVERTEXDECLARATION9 m_pDeclaration;
  SD3DVertexDeclaration *m_pMorphDeclaration;
};
#elif defined (DIRECT3D10)
struct SD3DVertexDeclaration
{
  TArray<D3D10_INPUT_ELEMENT_DESC> m_Declaration;  
  ID3D10InputLayout *m_pDeclaration;
  SD3DVertexDeclaration *m_pMorphDeclaration;
};
#endif


#if defined(DIRECT3D9) || defined(DIRECT3D10) || defined (OPENGL)

template <class IndexType> class DynamicIB;

template < class VertexType > class DynamicVB;
union UDynamicVB
{
  DynamicVB <Vec3>                                    *VBPtr_0;
  DynamicVB <struct_VERTEX_FORMAT_P3F>                *VBPtr_1;
  DynamicVB <struct_VERTEX_FORMAT_P3F_COL4UB>         *VBPtr_2;
  DynamicVB <struct_VERTEX_FORMAT_P3F_TEX2F>          *VBPtr_3;
  DynamicVB <struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F>   *VBPtr_4;
  DynamicVB <struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F>   *VBPtr_5;
  DynamicVB <struct_VERTEX_FORMAT_TRP3F_COL4UB_TEX2F> *VBPtr_6;
  DynamicVB <SPipTangents>                            *VBPtr_11;
  DynamicVB <struct_VERTEX_FORMAT_TEX2F>              *VBPtr_12;
};
struct SVertexDeclaration
{
  int StreamMask;
  int VertFormat;
  int InstAttrMask;
#if defined (DIRECT3D9) || defined (OPENGL)
  TArray<D3DVERTEXELEMENT9> m_Declaration;  
  LPDIRECT3DVERTEXDECLARATION9 m_pDeclaration;
#elif defined (DIRECT3D10)
  TArray<D3D10_INPUT_ELEMENT_DESC> m_Declaration;  
  ID3D10InputLayout* m_pDeclaration;
#endif
};

#endif

#if defined(DIRECT3D9) || defined(OPENGL)
struct SFSAA
{
  D3DMULTISAMPLE_TYPE Type;
  DWORD Quality;
  LPDIRECT3DSURFACE9 m_pZBuffer;
};
#elif defined(DIRECT3D10)
struct SFSAA
{
  UINT Type;
  DWORD Quality;
  ID3D10Texture2D *m_pDepthTex;
  ID3D10DepthStencilView *m_pZBuffer;
};
#endif

struct SProfInfo
{
  int NumPolys;
  int NumDips;
  CShader *pShader;
  SShaderTechnique *pTechnique;
  double Time;
  int m_nItems;
  SProfInfo()
  {
    NumPolys = 0;
    NumDips = 0;
    pShader = NULL;
    pTechnique = NULL;
    m_nItems = 0;
  }
};

struct SPlane
{
  byte m_Type;
  byte m_SignBits;
  Vec3 m_Normal;
  float m_Dist;
  void Init()
  {
    if ( m_Normal[0] == 1.0f )
      m_Type = PLANE_X;
    if ( m_Normal[1] == 1.0f )
      m_Type = PLANE_Y;
    if ( m_Normal[2] == 1.0f )
      m_Type = PLANE_Z;
    else
      m_Type = PLANE_NON_AXIAL;

    // for fast box on planeside test
    int bits = 0;
    int j;
    for (j=0; j<3; j++)
    {
      if (m_Normal[j] < 0)
        bits |= 1<<j;
    }
    m_SignBits = bits;
  }
};

struct SStatItem
{
  int m_Nums;
  double m_fTime;
};

#define STARTPROFILE(Item) { ticks(Item.m_fTime); Item.m_Nums++; }
#define ENDPROFILE(Item) { unticks(Item.m_fTime); }

struct SRTargetStat
{
  string m_Name;
  uint m_nSize;
  uint m_nWidth;
  uint m_nHeight;
  ETEX_Format m_eTF;
};

struct SPipeStat
{
  int m_NumRendBatches;
  int m_NumRendInstances;
  int m_NumRendHWInstances;
  int m_RendHWInstancesPolysAll;
  int m_RendHWInstancesPolysOne;
  int m_RendHWInstancesDIPs;
  int m_NumLightSetups;
  int m_NumTextChanges;
  int m_NumRTChanges;
  int m_NumStateChanges;
  int m_NumRendSkinnedObjects;
  int m_NumVShadChanges;
  int m_NumPShadChanges;
  int m_NumGShadChanges;
  int m_NumVShaders;
  int m_NumPShaders;
  int m_NumGShaders;
  int m_NumRTs;
  int m_RTCleared;
  int m_RTClearedSize;
  int m_RTCopied;
  int m_RTCopiedFSAA;
  int m_RTCopiedSize;
  int m_RTSize;
  int m_NumSprites;
  int m_NumSpriteDIPS;
  int m_NumSpritePolys;
  int m_NumSpriteUpdates;

  int m_NumMergedPolys;

  int m_NumPSInstructions;
  int m_NumVSInstructions;
  CHWShader *m_pMaxPShader;
  CHWShader *m_pMaxVShader;
  void *m_pMaxPSInstance;
  void *m_pMaxVSInstance;

  int m_NumTextures;
  int m_ManagedTexturesSize;
  int m_ManagedTexturesFullSize;
  int m_DynTexturesSize;
  int m_MeshUpdateBytes;
  int m_DynMeshUpdateBytes;
  float m_fOverdraw;
  float m_fSkinningTime;
  float m_fPreprocessTime;
  float m_fFlushTime;
  float m_fTexUploadTime;
  float m_fTexRestoreTime;
  float m_fOcclusionTime;
  int m_NumNotFinishFences;
  int m_NumFences;
  float m_fEnvCMapUpdateTime;
  float m_fEnvTextUpdateTime;

  int m_NumImpostersUpdates;
  int m_NumCloudImpostersUpdates;
  int m_NumImpostersDraw;
  int m_NumCloudImpostersDraw;
  int m_ImpostersSizeUpdate;
  int m_CloudImpostersSizeUpdate;

  int m_nDIPs[EFSLIST_NUM];
  int m_nPolygons[EFSLIST_NUM];
};

struct SSplash
{
  int m_Id;
  Vec3  m_Pos;
  float m_fForce;
  eSplashType m_eType;
  float m_fStartTime;
  float m_fLastTime;
  float m_fCurRadius;
};

//Batch flags
#define FB_GENERAL        1
#define FB_TRANSPARENT    2
#define FB_DETAIL         4
#define FB_Z              8
#define FB_GLOW           0x10
#define FB_SCATTER        0x20
#define FB_PREPROCESS     0x40
#define FB_MOTIONBLUR     0x80
#define FB_REFRACTIVE     0x100
#define FB_MULTILAYERS    0x200
#define FB_CAUSTICS       0x400
#define FB_CUSTOM_RENDER  0x800
#define FB_RAIN     0x1000

// m_RP.m_Flags
#define RBF_2D               0x10

#define RBF_MODIF_TC         0x1000
#define RBF_MODIF_VERT       0x2000
#define RBF_MODIF_COL        0x4000
#define RBF_MODIF_MASK       0xf000

#define RBF_NEAREST          0x10000
#define RBF_CUSTOM_CAMERA    0x20000
#define RBF_3D               0x40000
#define RBF_SHOWLINES        0x80000

// m_RP.m_PersFlags
#define RBPF_USESTREAM       (1<<0)
#define RBPF_USESTREAM_MASK  ((1<<VSF_NUM)-1)
#define RBPF_DRAWTOTEXTURE   (1<<16)
#define RBPF_MIRRORCAMERA    (1<<17)
#define RBPF_MIRRORCULL      (1<<18)

#define RBPF_ZPASS             (1<<19)
#define RBPF_SHADOWGEN         (1<<20)

#define RBPF_FP_DIRTY          (1<<21)
#define RBPF_FP_MATRIXDIRTY    (1<<22)

#define RBPF_WASWORLDSPACE     (1<<23)
#define RBPF_MAKESPRITE        (1<<24)
#define RBPF_MULTILIGHTS       (1<<25)
#define RBPF_HDR               (1<<26)
#define RBPF_POSTPROCESS       (1<<27)
#define RBPF_IN_CLEAR          (1<<28)
#define RBPF_ALLOW_AO          (1<<29)
#define RBPF_OBLIQUE_FRUSTUM_CLIPPING  (1<<30)


// m_RP.m_PersFlags2
#define RBPF2_NOSHADERFOG      (1<<0)			// 0x001
#define RBPF2_SCENEPASS        (1<<1)			// 0x002
#define RBPF2_NOALPHABLEND     (1<<2)			// 0x004
#define RBPF2_IMPOSTERGEN      (1<<3)			// 0x008
#define RBPF2_RAINPASS         (1<<4)			// 0x020
#define RBPF2_ONLYREFRACTED    (1<<5)			// 0x020
#define RBPF2_IGNOREREFRACTED  (1<<7)			// 0x080
#define RBPF2_VSM			         (1<<8)			// 0x100
#define RBPF2_HDR_FP16         (1<<9)			// 0x200
#define RBPF2_IN_PREDICATED_TILING (1<<10)			// 0x400
#define RBPF2_COMMIT_CM        (1<<13)
#define RBPF2_GLOWPASS         (1<<14)
#define RBPF2_SCATTERPASS      (1<<15)
#define RBPF2_CLEAR_SHADOW_MASK (1<<16)

#define RBPF2_DONTDRAWNEAREST (1<<17)
#define RBPF2_NOALPHATEST     (1<<18)
#define RBPF2_DRAWLIGHTS      (1<<19)
#define RBPF2_NOCLEARBUF      (1<<20)

#define RBPF2_COMMIT_PF       (1<<21)
#define RBPF2_SETCLIPPLANE    (1<<22)
#define RBPF2_DRAWTOCUBE      (1<<23)

#define RBPF2_MOTIONBLURPASS     (1<<24)
#define RBPF2_MATERIALLAYERPASS  (1<<25)
#define RBPF2_DISABLECOLORWRITES (1<<26)
#define RBPF2_CUSTOM_RENDER_PASS (1<<27)
#define RBPF2_ENCODE_HDR         (1<<28)
#define RBPF2_LIGHTSHAFTS        (1<<29)
#define RBPF2_ATOC              (1<<30)
#define RBPF2_LIGHTSTENCILCULL  (1<<31)

// m_RP.m_FlagsPerFlush
#define RBSI_SHORTPOS        0x1
#define RBSI_NOCULL          0x2
#define RBSI_DRAWAS2D        0x8
#define RBSI_FOGVOLUME       0x80
#define RBSI_INDEXSTREAM     0x1000
#define RBSI_SHADOWPASS      0x10000
#define RBSI_VERTSMERGED     0x100000
#define RBSI_LMTCMERGED      0x200000
#define RBSI_TANGSMERGED     0x400000
#define RBSI_FURPASS         0x800000
#define RBSI_USE_LM          0x2000000
#define RBSI_USE_HDRLM       0x4000000
#define RBSI_INSTANCED       0x10000000

// m_RP.m_ShaderLightMask
#define SLMF_DIRECT       0
#define SLMF_POINT        1
#define SLMF_PROJECTED    2
#define SLMF_TYPE_MASK    (SLMF_POINT | SLMF_PROJECTED)

#define SLMF_LTYPE_SHIFT  8
#define SLMF_LTYPE_BITS   4

struct SLightPass
{
  CDLight *pLights[4];
  uint nStencLTMask;
  uint nLights;
  uint nLTMask;
  bool bRect;
  RectI rc;
};

#define MAX_STREAMS 16

struct SStreamInfo
{
  void *pStream;
  int nOffset;
  int nFreq;
};

struct SMemPool
{
  int m_nBufSize;
  byte *m_pBuf;
  PodArray<alloc_info_struct> m_alloc_info;
};

// Render pipeline structure
struct SRenderPipeline
{
  CShader *m_pShader;        
  CShader *m_pReplacementShader;        
  CRenderObject *m_pCurObject;
  SInstanceInfo *m_pCurInstanceInfo;
  CRendElement *m_pRE;
  SShaderTechnique *m_pCurTechnique;
  SShaderTechnique *m_pRootTechnique;
  SShaderPass *m_pCurPass;
  int m_nShaderTechnique;

  SRenderShaderResources *m_pShaderResources;
  CRenderObject *m_pPrevObject;
  TArray<CRenderObject *> m_ObjectInstances;

  short m_TransformFrame;
  ColorF m_CurGlobalColor;
  
  float m_RealTime;

  float m_fMinDistance;
  int m_ObjFlags;               // Instances flag for batch (merged)
  int m_Flags;                  // Reset on start pipeline
  int m_PersFlags;              // Never reset
  int m_PersFlags2;             // Never reset
  int m_FlagsPerFlush;          // Flags which resets for each shader flush
  uint m_FlagsStreams_Decl;
  uint m_FlagsStreams_Stream;
  uint m_FlagsShader_LT;        // Shader light mask
  uint64 m_FlagsShader_RT;      // Shader runtime mask
  uint m_FlagsShader_MD;        // Shader texture modificator mask
  uint m_FlagsShader_MDV;       // Shader vertex modificator mask
  uint m_nShaderQuality;

  void (*m_pRenderFunc)();

  int m_CurState;
  int m_CurAlphaRef;
  int m_MaterialState;
  int m_MaterialAlphaRef;
  bool m_bIgnoreObjectAlpha;

  int m_FrameObject;
  ECull m_eCull;

  int m_CurStencilState;
  uint m_CurStencMask;
  uint m_CurStencWriteMask;
  uint m_CurStencRef;
  int m_CurStencilSide;

  SStreamInfo m_VertexStreams[MAX_STREAMS];
  int m_ReqStreamFrequence[MAX_STREAMS];
  void *m_pIndexStream;

  CRenderObject *m_pIgnoreObject;
  
  bool m_bNotFirstPass;
  int m_RenderFrame;
  uint m_nNumRendPasses;
  int m_NumShaderInstructions;

  string m_sExcludeShader;
  string m_sShowOnlyShader;
  
  TArray<SProfInfo> m_Profile;
  
  CCamera m_PrevCamera;

  uint32 m_nRendFlags;
  bool m_bUseHDR;
  int m_nBatchFilter;             // Batch flags ( FB_ )
  int m_nPassGroupID;             // EFSLIST_ pass type
  int m_nPassGroupDIP;            // EFSLIST_ pass type
  int m_nSortGroupID;
  int m_nShadowChannel;
  uint m_nFlagsShaderBegin;

  ERenderQuality m_eQuality;

  uint m_DynLMask;
  uint m_PrevLMask;
  int m_nCurLightPasses;
  short m_nCurLightGroup;
  int m_nMaxLightsPerPass;
  SRendLightGroup* m_pCurrentLightGroup;
  short m_nCurLightChan;
  int m_nCurLightPass;
  TArray<CDLight *> m_DLights[MAX_REND_RECURSION_LEVELS];
  SLightPass m_LPasses[MAX_REND_LIGHTS];
  float m_fProfileTime;
  
  ShadowMapFrustum* m_pCurShadowFrustum;
  Vec4 m_vFrustumInfo;
  TArray<SFillLight> m_FillLights[MAX_REND_RECURSION_LEVELS];

  //SLightIndicies *m_pCurLightIndices;
  //SLightIndicies  m_FakeLightIndices;

  //SMemPool m_VertPosCache;

  UPipeVertex m_PtrTang;
  UPipeVertex m_NextPtrTang;

  UPipeVertex m_Ptr;
  UPipeVertex m_NextPtr;
  int m_Stride;
  int m_OffsT;
  int m_OffsD;
  int m_CurVFormat;

//	Matrix44 m_mLastWaterMatrix;
	Vec3 m_vLastWaterFrustumPoints[4];
//  float m_fLastWaterFOVUpdate;
//  Vec3 m_LastWaterViewdirUpdate;
//  Vec3 m_LastWaterUpdirUpdate;
//  Vec3 m_LastWaterPosUpdate;
  float m_fLastWaterUpdate;
  int m_nLastWaterFrameID;
  bool m_bLastWaterAnisotropicRefl;

  bool m_bFrameUpdated[10];
  uint32 m_nCurrUpdateType;
  float m_fCurrUpdateFactor;
  float m_fCurrUpdateDistance;

	int m_nAOMaskUpdateLastFrameId;

#if defined(DIRECT3D9) || defined(DIRECT3D10) || defined(OPENGL)
  SFSAA   m_FSAAData;
	bool IsFSAAEnabled() const { return m_FSAAData.Type>0; }

  UDynamicVB m_VBs[MAX_DYNVBS];
  DynamicIB <ushort> *m_IndexBuf;
  UDynamicVB m_MergedStreams[3];
  int m_nStreamOffset[3];
  SD3DVertexDeclaration m_D3DVertexDeclaration[1<<VSF_NUM][VERTEX_FORMAT_NUMS]; 
  DynamicVB <vec4_t> *m_VB_Inst;
  TArray<SVertexDeclaration *> m_CustomVD;
#else
	bool IsFSAAEnabled() const { return false; }
#endif

  ushort *m_RendIndices;        
  ushort *m_SysRendIndices;        
  byte   *m_SysArray;     
  int    m_SizeSysArray;     

  TArray<byte>  m_SysVertexPool;        
  TArray<ushort>  m_SysIndexPool;        

  int m_CurVB;

  int m_RendNumGroup;             
  int m_RendNumIndices;             
  int m_RendNumVerts;
  int m_FirstIndex;
  int m_IndexOffset;
  int m_FirstVertex;

  int m_VFormatsMerge[VERTEX_FORMAT_NUMS][VERTEX_FORMAT_NUMS];
  Vec4 m_LightInfo[256];

  bool m_bClipPlaneRefract;
  SPlane m_CurClipPlane;
  SPlane m_CurClipPlaneCull;
  int m_ClipPlaneEnabled;

  Plane m_pObliqueClipPlane;
  bool m_bObliqueClipPlane;

  int m_FT;
  
  SEfResTexture *m_ShaderTexResources[MAX_TMU];
  
  int m_Frame;
  int m_FrameMerge;
  
  int m_nMaxPasses;
  float m_fCurOpacity;

  SPipeStat m_PS;
  DynArray<SRTargetStat> m_RTStats;

  int m_MaxVerts;
  int m_MaxTris;

  int m_RECustomTexBind[8];
  int m_ShadowCustomTexBind[8];
  int m_ShadowCustomResViewID[8];
  ColorF m_REColor;
  float m_RECustomData[64];

//===================================================================
// Input render data
  TArray<SSplash> m_Splashes;
  int m_NumVisObjects;
  CRenderObject **m_VisObjects;
  CDLight *m_pSunLight;
  TArray <CRenderObject *> m_RejectedObjects;
  TArray <CRenderObject *> m_Objects;
  TArray <CRenderObject *> m_TempObjects;
  TArray <CREMesh *> m_TempREs;
  CRenderObject *m_ObjectsPool;
  uint m_nNumObjectsInPool;

  //================================================================
  // Portals support
  float m_fMinDepthRange;
  float m_fMaxDepthRange;

  int m_RecurseLevel;

  //================================================================
  // Sprites
  TArray<SRefSprite> m_Sprites;    // Used for particles (AddSpriteToScene)
  TArray<SRefSprite> m_Polygons;   // Used for decals mostly (AddPolygonToScene)

  //================================================================
  // Temporary mesh
  TArray<class CRETempMesh *> m_TempMeshes[2];
  TArray<class CRETempMesh *> *m_CurTempMeshes;

  //================================================================
  // Hdr
  class CREHDRProcess *m_pREHDR;
  
  int m_HeatSize;
  int m_NightSize;  
  int m_RainMapSize;
  
  //=================================================================
  // WaveForm tables
  float m_tSinTable[1024];
  float m_tHalfSinTable[1024];
  float m_tCosTable[1024];
  float m_tSquareTable[1024];
  float m_tTriTable[1024];
  float m_tSawtoothTable[1024];
  float m_tInvSawtoothTable[1024];
  float m_tHillTable[1024];

  float m_tRandFloats[256];
  byte  m_tRandBytes[256];
};

#endif	// __RENDERPIPELINE_H__

