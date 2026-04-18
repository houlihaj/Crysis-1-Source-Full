/*=============================================================================
  TexMan.h : Common texture manager declarations.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Khonich Andrey

=============================================================================*/

      
#ifndef _TEXTURE_H
#define _TEXTURE_H

#include "../ResFile.h"
#include "PowerOf2BlockPacker.h"
#include "STLPoolAllocator.h"

class CTexture;
struct IFlashPlayer;
struct IVideoPlayer;

#define TEXPOOL_LOADSIZE 6*1024*1024

// Custom Textures IDs
enum 
{
  TO_RT_2D = 1,
  TO_RT_LCM,

  TO_FROMLIGHT,
  TO_FOG,
  TO_FROMOBJ0,
  TO_FROMOBJ1,

  TO_SHADOWID0,
  TO_SHADOWID1,
  TO_SHADOWID2,
  TO_SHADOWID3,
  TO_SHADOWID4,
  TO_SHADOWID5,
  TO_SHADOWID6,
  TO_SHADOWID7,

  TO_FROMRE0,
  TO_FROMRE1,

  TO_FROMRE0_FROM_CONTAINER,
  TO_FROMRE1_FROM_CONTAINER,

  TO_SCREENMAP,
  TO_SCREENSHADOWMAP,
  TO_SCATTER_LAYER,
  TO_TERRAIN_LM,
  TO_RT_CM,
  TO_CLOUDS_LM,
  TO_BACKBUFFERMAP,
  TO_MIPCOLORS_DIFFUSE,
  TO_MIPCOLORS_BUMP,
  TO_LIGHTINFO,

  TO_FROMSF0,
  TO_FROMSF1,

  TO_SCREEN_AO_TARGET,
  TO_DOWNSCALED_ZTARGET_FOR_AO,

  TO_WATEROCEANMAP,
  TO_WATERVOLUMEMAP,

  TO_WATERPUDDLESMAP,

	TO_VOLOBJ_DENSITY,
	TO_VOLOBJ_SHADOW,

  TO_ZTARGET_MS
};


#define NUM_HDR_TONEMAP_TEXTURES 4
#define NUM_HDR_BLOOM_TEXTURES   3
#define NUM_SCALEFORM_TEXTURES   2

#define DYNTEXTURE_TEXCACHE_LIMIT 32

inline int LogBaseTwo(int iNum)
{
  int i, n;
  for(i = iNum-1, n = 0; i > 0; i >>= 1, n++ );
  return n;
}

enum EShadowBuffers_Pool
{
  SBP_DF16,
  SBP_DF24,
  SBP_D16,
  SBP_D24S8,
  SBP_G16R16,
  SBP_D32F,
  SBP_MAX,
  SBP_UNKNOWN
};

//dx9 usages
enum ETexture_Usage
{
  eTXUSAGE_AUTOGENMIPMAP,
  eTXUSAGE_DEPTHSTENCIL,
  eTXUSAGE_RENDERTARGET
};



#define TEX_POOL_BLOCKLOGSIZE 5  // 32
#define TEX_POOL_BLOCKSIZE   (1 << TEX_POOL_BLOCKLOGSIZE)

struct SDynTexture_Shadow;

//======================================================================
// Dynamic textures
struct SDynTexture : public IDynTexture
{
  static int              m_nMemoryOccupied;
  static SDynTexture      m_Root;

  SDynTexture *           m_Next;           //!<
  SDynTexture *           m_Prev;           //!<
  char                    m_sSource[128];   //!< pointer to the given name in the constructor call

  CTexture *              m_pTexture;
  ETEX_Format             m_eTF;
  ETEX_Type               m_eTT;
  uint32                  m_nWidth;
  uint32                  m_nHeight;
  uint32                  m_nReqWidth;
  uint32                  m_nReqHeight;
  int                     m_nTexFlags;
	uint                    m_nFrameReset;


  bool                    m_bLocked;
  byte                    m_nUpdateMask;
  byte                    m_nPriority;

	//////////////////////////////////////////////////////////////////////////
	// Shadow specific vars.
	//////////////////////////////////////////////////////////////////////////
	int m_nUniqueID;

	SDynTexture_Shadow *m_NextShadow;           //!<
	SDynTexture_Shadow *m_PrevShadow;           //!<

	IRenderNode * pLightOwner;
	int nObjectsRenderedCount;
	//////////////////////////////////////////////////////////////////////////

  SDynTexture(const char *szSource);
  SDynTexture(int nWidth, int nHeight, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char *szSource, int8 nPriority=-1);
  ~SDynTexture();

	//////////////////////////////////////////////////////////////////////////
	// Custom new/delete for pool allocator.
	//////////////////////////////////////////////////////////////////////////
	ILINE void* operator new( size_t nSize );
	ILINE void operator delete( void *ptr );
	//////////////////////////////////////////////////////////////////////////

	virtual int GetTextureID();
  virtual bool Update(int nNewWidth, int nNewHeight);
  virtual void Apply(int nTUnit, int nTS=-1);
  virtual bool SetRT(int nRT, bool bPush, SD3DSurface *pDepthSurf);
  virtual bool SetRectStates() { return true; }
  virtual bool RestoreRT(int nRT, bool bPop);
	virtual ITexture *GetTexture() { return (ITexture *)m_pTexture; }
	virtual void Release() { delete this; }
  virtual void SetUpdateMask();
  virtual void ResetUpdateMask();
  virtual bool IsSecondFrame() { return m_nUpdateMask == 3; }
	virtual void GetSubImageRect(uint32 & nX, uint32 & nY, uint32 & nWidth, uint32 & nHeight)
	{
		nX = 0;
		nY = 0;
		nWidth = m_nWidth;
		nHeight = m_nHeight;
	}
  virtual void GetImageRect(uint32 & nX, uint32 & nY, uint32 & nWidth, uint32 & nHeight);
	virtual int GetWidth() { return m_nWidth; }
	virtual int GetHeight() { return m_nHeight; }

  void Lock() { m_bLocked = true; }
  void UnLock() { m_bLocked = false; }

  virtual void AdjustRealSize();
  virtual bool IsValid();

  _inline void UnlinkGlobal()
  {
    if (!m_Next || !m_Prev)
      return;
    m_Next->m_Prev = m_Prev;
    m_Prev->m_Next = m_Next;
    m_Next = m_Prev = NULL;
  }
  _inline void LinkGlobal(SDynTexture* Before)
  {
    if (m_Next || m_Prev)
      return;
    m_Next = Before->m_Next;
    Before->m_Next->m_Prev = this;
    Before->m_Next = this;
    m_Prev = Before;
  }
  virtual void Unlink()
  {
    UnlinkGlobal();
  }
  virtual void Link()
  {
    LinkGlobal(&m_Root);
  }
  ETEX_Format GetFormat() { return m_eTF; }
  bool FreeTextures(bool bOldOnly, int nNeedSpace);

	typedef std::multimap<unsigned int, CTexture*, std::less<unsigned int>, stl::STLPoolAllocator<std::pair<unsigned int, CTexture*>, stl::PoolAllocatorSynchronizationSinglethreaded> > TextureSubset;
  typedef TextureSubset::iterator TextureSubsetItor;
  typedef std::multimap<unsigned int, TextureSubset*, std::less<unsigned int>, stl::STLPoolAllocator<std::pair<unsigned int, TextureSubset*>, stl::PoolAllocatorSynchronizationSinglethreaded> >  TextureSet;
  typedef TextureSet::iterator TextureSetItor;
  static TextureSet      m_availableTexturePool2D_A8R8G8B8;
  static TextureSubset   m_checkedOutTexturePool2D_A8R8G8B8;
  static TextureSet      m_availableTexturePool2D_R32F;
  static TextureSubset   m_checkedOutTexturePool2D_R32F;
	static TextureSet      m_availableTexturePool2D_R16F;
	static TextureSubset   m_checkedOutTexturePool2D_R16F;
  static TextureSet      m_availableTexturePool2D_G16R16F;
  static TextureSubset   m_checkedOutTexturePool2D_G16R16F;
  static TextureSet      m_availableTexturePool2D_A16B16G16R16F;
  static TextureSubset   m_checkedOutTexturePool2D_A16B16G16R16F;

  static TextureSet    m_availableTexturePool2D_Shadows[SBP_MAX];
  static TextureSubset m_checkedOutTexturePool2D_Shadows[SBP_MAX];
  static TextureSet    m_availableTexturePoolCube_Shadows[SBP_MAX];
  static TextureSubset m_checkedOutTexturePoolCube_Shadows[SBP_MAX];

  static TextureSet      m_availableTexturePool2DCustom_G16R16F;
  static TextureSubset   m_checkedOutTexturePool2DCustom_G16R16F;


  static TextureSet      m_availableTexturePoolCube_A8R8G8B8;
  static TextureSubset   m_checkedOutTexturePoolCube_A8R8G8B8;
  static TextureSet      m_availableTexturePoolCube_R32F;
  static TextureSubset   m_checkedOutTexturePoolCube_R32F;
	static TextureSet      m_availableTexturePoolCube_R16F;
	static TextureSubset   m_checkedOutTexturePoolCube_R16F;
  static TextureSet      m_availableTexturePoolCube_G16R16F;
  static TextureSubset   m_checkedOutTexturePoolCube_G16R16F;
  static TextureSet      m_availableTexturePoolCube_A16B16G16R16F;
  static TextureSubset   m_checkedOutTexturePoolCube_A16B16G16R16F;

  static TextureSet      m_availableTexturePoolCubeCustom_G16R16F;
  static TextureSubset   m_checkedOutTexturePoolCubeCustom_G16R16F;

  static uint    m_iNumTextureBytesCheckedOut;
  static uint    m_iNumTextureBytesCheckedIn;

  CTexture *CreateDynamicRT();
  CTexture *GetDynamicRT();
  void ReleaseDynamicRT(bool bForce);

  EShadowBuffers_Pool ConvertTexFormatToShadowsPool( ETEX_Format e );

  static bool FreeAvailableDynamicRT(int nNeedSpace, TextureSet *pSet, bool bOldOnly);

  static void Init();
};


typedef stl::PoolAllocatorNoMT<sizeof(SDynTexture)> SDynTexture_PoolAlloc;
extern SDynTexture_PoolAlloc* g_pSDynTexture_PoolAlloc;

//////////////////////////////////////////////////////////////////////////
// Custom new/delete for pool allocator.
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
inline void* SDynTexture::operator new( size_t nSize )
{
	void *ptr = g_pSDynTexture_PoolAlloc->Allocate();
	if (ptr)
		memset( ptr,0,nSize ); // Clear objects memory.
	return ptr;
}
//////////////////////////////////////////////////////////////////////////
inline void SDynTexture::operator delete( void *ptr )
{
	if (ptr)
		g_pSDynTexture_PoolAlloc->Deallocate(ptr);
}


//==============================================================================

struct STextureSetFormat
{
  struct SDynTexture2    *m_pRoot;
  ETEX_Format             m_eTF;
  ETEX_Type               m_eTT;
  int                     m_nTexFlags;
  std::vector<CPowerOf2BlockPacker *> m_TexPools;

  STextureSetFormat(ETEX_Format eTF, uint32 nTexFlags)
  {
    m_eTF = eTF;
    m_eTT = eTT_2D;
    m_pRoot = NULL;
    m_nTexFlags = nTexFlags;
  }
};

enum ETexPool
{
  eTP_Clouds,
  eTP_Sprites,

  eTP_Max
};

struct SDynTexture2 : public IDynTexture
{
#ifndef _DEBUG
  char                   *m_sSource;        //!< pointer to the given name in the constructor call
#else
  char                   m_sSource[128];        //!< pointer to the given name in the constructor call
#endif
  STextureSetFormat      *m_pOwner;
  CPowerOf2BlockPacker   *m_pAllocator;
  CTexture               *m_pTexture;
  uint32                  m_nBlockID;
  ETexPool               m_eTexPool;

  SDynTexture2           *m_Next;           //!<
  SDynTexture2          **m_PrevLink;           //!<

  _inline void UnlinkGlobal()
  {
    if (m_Next)
      m_Next->m_PrevLink = m_PrevLink;
    if (m_PrevLink)
      *m_PrevLink = m_Next;
  }
  _inline void LinkGlobal(SDynTexture2*& Before)
  {
    if (Before)
      Before->m_PrevLink = &m_Next;
    m_Next = Before;
    m_PrevLink = &Before;
    Before = this;
  }
  void Link()
  {
    LinkGlobal(m_pOwner->m_pRoot);
  }
  void Unlink()
  {
    UnlinkGlobal();
    m_Next = NULL;
    m_PrevLink = NULL;
  }
  bool Remove();

  uint32                  m_nX;
  uint32                  m_nY;
  uint32                  m_nWidth;
  uint32                  m_nHeight;

  bool                    m_bLocked;
  byte                    m_nUpdateMask;  // Crossfire odd/even frames
	uint                    m_nFrameReset;
  uint                    m_nAccessFrame;

  SDynTexture2(int nWidth, int nHeight, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char *szSource, ETexPool eTexPool);
  SDynTexture2(const char *szSource, ETexPool eTexPool);
  ~SDynTexture2();

  bool UpdateAtlasSize(int nNewWidth, int nNewHeight);

  virtual bool Update(int nNewWidth, int nNewHeight);
  virtual void Apply(int nTUnit, int nTS=-1);
  virtual bool SetRT(int nRT, bool bPush, SD3DSurface *pDepthSurf);
  virtual bool SetRectStates();
  virtual bool RestoreRT(int nRT, bool bPop);
	virtual ITexture *GetTexture() { return (ITexture *)m_pTexture; }
  ETEX_Format GetFormat() { return m_pOwner->m_eTF; }
	virtual void SetUpdateMask();
  virtual void ResetUpdateMask();
  virtual bool IsSecondFrame() { return m_nUpdateMask == 3; }

	// IDynTexture implementation
	virtual void Release() { delete this; }
	virtual void GetSubImageRect(uint32 & nX, uint32 & nY, uint32 & nWidth, uint32 & nHeight)
	{
		nX = m_nX;
		nY = m_nY;
		nWidth = m_nWidth;
		nHeight = m_nHeight;
	}
  virtual void GetImageRect(uint32 & nX, uint32 & nY, uint32 & nWidth, uint32 & nHeight);
	virtual int GetTextureID();
  virtual bool IsValid();
  virtual void Lock() { m_bLocked = true; }
  virtual void UnLock() { m_bLocked = false; }
	virtual int GetWidth() { return m_nWidth; }
	virtual int GetHeight() { return m_nHeight; }

  typedef std::map<uint32, STextureSetFormat *>  TextureSet2;
  typedef TextureSet2::iterator TextureSet2Itor;
  static TextureSet2      m_TexturePool[eTP_Max];
  static int              m_nMemoryOccupied[eTP_Max];

  static void Init(ETexPool eTexPool);
};

//////////////////////////////////////////////////////////////////////////
// Dynamic texture for the shadow.
// This class must not contain any non static member variables, 
//  because SDynTexture allocated used constant size pool.
//////////////////////////////////////////////////////////////////////////
struct SDynTexture_Shadow : public SDynTexture
{
	static SDynTexture_Shadow  m_RootShadow;

	//////////////////////////////////////////////////////////////////////////
  SDynTexture_Shadow(int nWidth, int nHeight, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char *szSource);
  SDynTexture_Shadow(const char *szSource);
  ~SDynTexture_Shadow();

  _inline void UnlinkShadow()
  {
    if (!m_NextShadow || !m_PrevShadow)
      return;
    m_NextShadow->m_PrevShadow = m_PrevShadow;
    m_PrevShadow->m_NextShadow = m_NextShadow;
    m_NextShadow = m_PrevShadow = NULL;
  }
  _inline void LinkShadow(SDynTexture_Shadow* Before)
  {
    if (m_NextShadow || m_PrevShadow)
      return;
    m_NextShadow = Before->m_NextShadow;
    Before->m_NextShadow->m_PrevShadow = this;
    Before->m_NextShadow = this;
    m_PrevShadow = Before;
  }

  SDynTexture_Shadow* GetByID(int nID)
  {
    SDynTexture_Shadow *pTX = SDynTexture_Shadow::m_RootShadow.m_NextShadow;
    for (pTX=SDynTexture_Shadow::m_RootShadow.m_NextShadow; pTX != &SDynTexture_Shadow::m_RootShadow; pTX=pTX->m_NextShadow)
    {
      if (pTX->m_nUniqueID == nID)
        return pTX;
    }
    return NULL;
  }

  virtual void Unlink()
  {
    UnlinkGlobal();
    UnlinkShadow();
  }
  virtual void Link()
  {
    LinkGlobal(&m_Root);
    LinkShadow(&m_RootShadow);
  }
  virtual void AdjustRealSize();
};

struct SDynTextureArray/* : public SDynTexture*/
{

  //SDynTexture members

  //////////////////////////////////////////////////////////////////////////
  char                    m_sSource[128];   //!< pointer to the given name in the constructor call

  CTexture *              m_pTexture;
  ETEX_Format             m_eTF;
  ETEX_Type               m_eTT;
  uint32                  m_nWidth;
  uint32                  m_nHeight;
  uint32                  m_nReqWidth;
  uint32                  m_nReqHeight;
  int                     m_nTexFlags;
  int                     m_nPool;
  uint                    m_nFrameReset;


  bool                    m_bLocked;
  byte                    m_nUpdateMask;
  byte                    m_nPriority;

  //////////////////////////////////////////////////////////////////////////
  // Shadow specific vars.
  //////////////////////////////////////////////////////////////////////////
  IRenderNode * pLightOwner;
  int nObjectsRenderedCount;

  int m_nUniqueID;
  //////////////////////////////////////////////////////////////////////////

  uint32 m_nArraySize;

  SDynTextureArray(int nWidth, int nHeight, int nArraySize, ETEX_Format eTF, int nTexFlags, const char *szSource);
  ~SDynTextureArray();

  bool Update(int nNewWidth, int nNewHeight);

  /*_inline void UnlinkShadow()
  {
    if (!m_NextShadow || !m_PrevShadow)
      return;
    m_NextShadow->m_PrevShadow = m_PrevShadow;
    m_PrevShadow->m_NextShadow = m_NextShadow;
    m_NextShadow = m_PrevShadow = NULL;
  }
  _inline void LinkShadow(SDynTexture_Shadow* Before)
  {
    if (m_NextShadow || m_PrevShadow)
      return;
    m_NextShadow = Before->m_NextShadow;
    Before->m_NextShadow->m_PrevShadow = this;
    Before->m_NextShadow = this;
    m_PrevShadow = Before;
  }

  SDynTexture_Shadow* GetByID(int nID)
  {
    SDynTexture_Shadow *pTX = SDynTexture_Shadow::m_RootShadow.m_NextShadow;
    for (pTX=SDynTexture_Shadow::m_RootShadow.m_NextShadow; pTX != &SDynTexture_Shadow::m_RootShadow; pTX=pTX->m_NextShadow)
    {
      if (pTX->m_nUniqueID == nID)
        return pTX;
    }
    return NULL;
  }

  virtual void Unlink()
  {
    UnlinkGlobal();
    UnlinkShadow();
  }
  virtual void Link()
  {
    LinkGlobal(&m_Root);
    LinkShadow(&m_RootShadow);
  }
  virtual void AdjustRealSize();*/
};


//=======================================================================

// round up sigh
#define BILERP_PROTECTION	2
// attenuation texture function width ranges from 0-1 with linear interpolation between sample
#define ATTENUATION_WIDTH			512
// space for 512 different attuentation functions
#define NUM_ATTENUATION_FUNCTIONS	8

/* Some attenuation shapes
*/
enum ATTENUATION_FUNCTION
{
  AF_LINEAR,
  AF_SQUARED,
  AF_SHAPE1,
  AF_SHAPE2,
};

////////////////////////////////////////////////////////////////////////////////////
// CBaseMap: Base class so that different implementations can be stored together //
////////////////////////////////////////////////////////////////////////////////////
class CTexFunc
{
protected:
  uint m_dwWidth, m_dwHeight;
  int m_nLevels;

public:
  CTexture *m_pTex;
  CTexFunc()
  {
    m_dwWidth = m_dwHeight = 1;
    m_nLevels = -1;
    m_pTex = NULL;
  }
  ~CTexFunc()
  {
    m_pTex = NULL;
  }

  virtual int Initialize() = 0;
};


//==========================================================================
// Texture

struct STexPool;

struct STexPoolItem
{
  CTexture *m_pTex;
  STexPool *m_pOwner;
  STexPoolItem *m_Next;
  STexPoolItem *m_Prev;
  STexPoolItem *m_NextFree;
  STexPoolItem *m_PrevFree;
  void *m_pAPITexture;
  float m_fLastTimeUsed;

  STexPoolItem ()
  {
    m_pTex = NULL;
    m_pAPITexture = NULL;
    m_pOwner = NULL;
    m_Next = NULL;
    m_Prev = NULL;
    m_NextFree = NULL;
    m_PrevFree = NULL;
  }

  _inline void Unlink()
  {
    if (!m_Next || !m_Prev)
      return;
    m_Next->m_Prev = m_Prev;
    m_Prev->m_Next = m_Next;
    m_Next = m_Prev = NULL;
  }

  _inline void Link( STexPoolItem* Before )
  {
    if (m_Next || m_Prev)
      return;
    m_Next = Before->m_Next;
    Before->m_Next->m_Prev = this;
    Before->m_Next = this;
    m_Prev = Before;
  }

  _inline void UnlinkFree()
  {
    if (!m_NextFree || !m_PrevFree)
      return;
    m_NextFree->m_PrevFree = m_PrevFree;
    m_PrevFree->m_NextFree = m_NextFree;
    m_NextFree = m_PrevFree = NULL;
  }
  _inline void LinkFree( STexPoolItem* Before )
  {
    if (m_NextFree || m_PrevFree)
      return;
    m_NextFree = Before->m_NextFree;
    Before->m_NextFree->m_PrevFree = this;
    Before->m_NextFree = this;
    m_PrevFree = Before;
  }
};

struct STexPool
{
  int m_Width;
  int m_Height;
  ETEX_Format m_eFormat;
  ETEX_Type m_eTT;
  int m_nMips;
  int m_Size;
#ifndef XENON
  void *m_pSysTexture;           // System texture for updating
  TArray <void *> m_SysSurfaces; // List of system surfaces for updating
#endif
  STexPoolItem m_ItemsList;
};

struct STexUploadInfo
{
  int nStartMip;
  int nEndMip;
  int m_TexId;
  CTexture *m_pTex;
  int m_LoadedSize;
};

#define TEXCACHE_VERSION 1

struct STexCacheFileHeader
{
  int m_Version;
  int m_SizeOf;
  byte m_nSides;
  byte m_nMipsPersistent;
  char m_sETF[14];
  ETEX_Format m_DstFormat;
  STexCacheFileHeader()
  {
    memset(this, 0, sizeof(STexCacheFileHeader));
  }
};

struct SMipData
{
public:
  TArray<byte> DataArray; // Data.
  bool m_bUploaded;
  bool m_bLoading;
  SMipData() : m_bUploaded(false), m_bLoading(false) {}
  void Clear()
  {
    DataArray.Clear();
    m_bUploaded = false;
    m_bLoading = false;
  }
  void Free()
  {
    DataArray.Free();
    m_bUploaded = false;
    m_bLoading = false;
  }
  void Init(int InSize)
  {
    DataArray.Reserve(InSize);
    m_bUploaded = false;
    m_bLoading = false;
  }
};


struct STexCacheMipHeader
{
  int m_SizeOf;
  short m_USize;
  short m_VSize;
  int m_Size;
  int m_SizeWithMips;
  int m_Seek;

  SMipData m_Mips[6];
};

class CTextureStreamCallback : public IStreamCallback
{
  virtual void StreamOnComplete (IReadStream* pStream, unsigned nError);
};

// streaming data - is attached to the asynchronous read request to pass data to the StreamOnComplete() callback
struct STexCacheInfo
{
  CTextureStreamCallback m_Callback;
  int m_TexId;
  int m_nStartLoadMip;
  int m_nEndLoadMip;
  int m_nSizeToLoad;
  int m_nCubeSide;
  void *m_pTempBufferToStream;
  float m_fStartTime;
  STexCacheInfo()
  {
    memset(&m_TexId, 0, sizeof(*this)-sizeof(CTextureStreamCallback));
  }
  ~STexCacheInfo()
  {
		char *pPtr = (char*)m_pTempBufferToStream;
    SAFE_DELETE_ARRAY(pPtr);
  }
};


//===================================================================

struct SEnvTexture
{
  bool m_bInprogress;
  bool m_bReady;
  bool m_bWater;
  bool m_bReflected;
  bool m_bReserved;
  int m_MaskReady;
  int m_Id;
  int m_TexSize;
  // Result Cube-Map or 2D RT texture
  SDynTexture *m_pTex;
  float m_TimeLastUpdated;
  Vec3 m_CamPos;
  Vec3 m_ObjPos;
  Ang3 m_Angle;
  int m_nFrameReset;
  //void *m_pQuery[6];
  //void *m_pRenderTargets[6];

  // Cube maps average colors (used for RT radiosity)
  int m_nFrameCreated[6];
  ColorF m_EnvColors[6];

  SEnvTexture()
  {
    m_bInprogress = false;
    m_bReady = false;
    m_bWater = false;
    m_bReflected = false;
    m_bReserved = false;
    m_MaskReady = 0;
    m_Id = 0;
    m_pTex = NULL;
    m_nFrameReset = -1;
    for (int i=0; i<6; i++)
    {
      //m_pQuery[i] = NULL;
      m_nFrameCreated[i] = -1;
      //m_pRenderTargets[i] = NULL;
    }
  }

  void Release();
  void ReleaseDeviceObjects();
};


//===============================================================================

#ifndef XENON
#define D3DFMT_3DC (MAKEFOURCC('A', 'T', 'I', '2'))
#else
#define D3DFMT_3DC D3DFMT_DXN
#endif

//ATI's native depth buffers
#define D3DFMT_DF16 (MAKEFOURCC('D','F','1','6'))
#define D3DFMT_DF24 (MAKEFOURCC('D','F','2','4')) //TODO: support for NVIDIA: D3DFMT_D24S8
//NVIDIA's NULL format
#define D3DFMT_NULL (MAKEFOURCC('N','U','L','L')) 

struct SPixFormat
{
  // Pixel format info.
  int             DeviceFormat; // Pixel format from Direct3D/OpenGL.
  const char*     Desc;     // Stat: Human readable name for stats.
  int             BitsPerPixel; // Total bits per pixel.
  bool            bCanAutoGenMips;
  int             MaxWidth;
  int             MaxHeight;
  SPixFormat     *Next;

  SPixFormat()
  {
    Init();
  }
  void Init()
  {
    BitsPerPixel = 0;
    DeviceFormat = 0;
    Desc = NULL;
    Next = NULL;
    bCanAutoGenMips = false;
  }
  bool IsValid()
  {
    if (BitsPerPixel)
      return true;
    return false;
  }
  bool CheckSupport(int Format, const char *szDescr, ETexture_Usage eTxUsage = eTXUSAGE_AUTOGENMIPMAP);
};

struct STexStageInfo
{
  int m_nCurState;
  STexState  m_State;

  // Per-stage color operations
  byte       m_CO;
  byte       m_CA;
  byte       m_AO;
  byte       m_AA;
  CTexture   *m_Texture;
#if defined(DIRECT3D10)
  int32      m_nCurResView;
#endif
  STexStageInfo()
  {
    Flush();
  }
  void Flush()
  {
    m_nCurState = -1;
    m_State.m_nMipFilter = -1;
    m_State.m_nMinFilter = -1;
    m_State.m_nMagFilter = -1;
    m_State.m_nAddressU = -1;
    m_State.m_nAddressV = -1;
    m_State.m_nAddressW = -1;
    m_State.m_nAnisotropy = 0;
#if defined(DIRECT3D10)
    m_nCurResView = -1;
#endif
    m_Texture = NULL;
  }
};


struct STexData
{
  byte *m_pData;
  int m_nWidth;
  int m_nHeight;
  int m_nDepth;
  ETEX_Format m_eTF;
  int m_nMips;
  int m_nSize;
  int m_nFlags;
  bool m_bReallocated;
  float m_fAmount;
  ColorF m_AvgColor;
  string m_FileName;

  STexData()
  {
    m_pData = NULL;
    m_eTF = eTF_Unknown;
    m_nFlags = 0;
    m_bReallocated = false;
    m_fAmount = 100.0f;
    m_nWidth = 0;
    m_nHeight = 0;
    m_nDepth = 1;
    m_nSize = 0;
	m_AvgColor = ColorF(0.5f,0.5f,0.5f,1.0f);
  }
  void AssignData(byte *pNewData)
  {
    if (m_bReallocated)
      delete [] m_pData;
    m_pData = pNewData;
    m_bReallocated = true;
  }
};

struct STexGrid
{
  int m_Width;
  int m_Height;
  CTexture *m_TP;
};

class CTexture : public ITexture, public CBaseResource
{
  friend class CFnMap;
  friend class CFnMap3D;
  friend struct SDynTexture;
  friend class CD3D9Renderer;

private:
  static CCryName m_sClassName;

  static int m_nMaxtextureSize;

  static CCryName GenName(const char *name, uint nFlags, float fAmount1=-1, float fAmount2=-1);
  static CTexture *NewTexture(const char *name, uint nFlags, ETEX_Format eTFDst, bool& bFound, float fAmount1=-1, float fAmount2=-1, int8 nPriority=-1);

  void *m_pDeviceTexture;
  void *m_pDeviceRTV;
#if defined (DIRECT3D10)
  void *m_pDeviceShaderResource;
  void *m_pShaderResourceViews[4]; //fix replace static array by malloc
  void *m_pDeviceTextureMSAA;
  void *m_pDeviceDepthStencilSurf;
  void *m_pDeviceDepthStencilCMSurfs[6];
  void *m_pDeviceShaderResourceMS;
#endif

  short m_nWidth;
  short m_nHeight;
  short m_nDepth;
  uint m_nArraySize;
  ETEX_Format m_eTFDst;
  uint m_nFlags;									// e.g. FT_USAGE_DYNAMIC
  uint m_nDeviceSize;
  uint m_nSize;

  uint m_bHasAlphaChannel : 1;
  uint m_bIsSingleSide : 1;
  uint m_bIsRectangle : 1;
  uint m_bIsLocked : 1;
  uint m_bNeedRestoring : 1;
  uint m_bNoTexture : 1;
  uint m_bIsPowerByTwo : 1;
  uint m_bResolved : 1;

  uint m_bDynamic : 1;
  uint m_bWasUnloaded : 1;
  uint m_bPartyallyLoaded : 1;
  uint m_bVersionWasChecked : 1;
  uint m_bDiscardInCache : 1;
  uint m_bStream : 1;
  uint m_bStreamPrepared : 1;
  uint m_bStreamFromDDS : 1;
  uint m_bStreamingInProgress : 1;
  uint m_bStreamRequested : 1;
  uint m_bVertexTexture : 1;
	uint m_bUseDecalBorderCol : 1;
  byte m_nMips;
  byte m_nMips3DC;
  int8 m_nPriority;
  uint m_bReleasePostpouned;

  ETEX_Type m_eTT;

  short m_nSrcWidth;
  short m_nSrcHeight;
  short m_nSrcDepth;
  ETEX_Format m_eTFSrc;
  ColorF m_AvgColor;
  string m_SrcName;

  int m_nCustomID;
  CTexFunc *m_pFunc;
  SPixFormat *m_pPixelFormat;

  int m_nFSAASamples;
  int m_nFSAAQuality;

  static void Resample(byte *pDst, int nNewWidth, int nNewHeight, const byte *pSrc, int nSrcWidth, int nSrcHeight);
  static void Resample8(byte *pDst, int nNewWidth, int nNewHeight, const byte *pSrc, int nSrcWidth, int nSrcHeight);
	static void Resample4Bit(byte *pDst, int nNewWidth, int nNewHeight, const byte *pSrc, int nSrcWidth, int nSrcHeight);

  int GenerateMipsForNormalmap(TArray<Vec4>* Normals, int nWidth, int nHeight);
  void ConvertFloatNormalMap_To_ARGB8(TArray<Vec4>* Normals, int nWidth, int nHeight, int nMips, byte *pDst);
  void MergeNormalMaps(STexData Data[]);
  void GenerateNormalMap(STexData *pData);
  void GenerateNormalMaps(STexData Data[6]);
  void ImagePreprocessing(STexData Data[6]);

public:
  CTexture()
  {
    m_nFlags = 0;
    m_eTFDst = eTF_Unknown;
    m_eTFSrc = eTF_Unknown;
    m_pFunc = NULL;
    m_nMips = 1;
    m_nMips3DC = 1;
    m_nWidth = 0;
    m_nHeight = 0;
    m_eTT = eTT_2D;
    m_Matrix = NULL;
    m_nSrcDepth = 1;
    m_nDepth = 1;
    m_nArraySize = 1;
    m_nDeviceSize = 0;
    m_nSize = 0;
    m_AvgColor = Col_White;
    m_nPriority = -1;

    m_nFSAASamples = 0;
    m_nFSAAQuality = 0;

    m_nUpdateFrameID = -1;
    m_nAccessFrameID = -1;
    m_nRTSetFrameID = -1;
    m_nCustomID = -1;
    m_pPixelFormat = NULL;
    m_pDeviceTexture = NULL;
    m_pDeviceRTV = NULL;
#if defined (DIRECT3D10)
    m_pDeviceShaderResource = NULL;
    m_pDeviceTextureMSAA = NULL;
    for( int i = 0; i < 4; ++i ) m_pShaderResourceViews[i] = NULL;
    m_pDeviceDepthStencilSurf = NULL;
    for( int i = 0; i < 6; ++i ) m_pDeviceDepthStencilCMSurfs[i] = NULL;
    m_pDeviceShaderResourceMS = NULL;
#endif
    m_NextTxt = NULL;

    m_bHasAlphaChannel = false;
    m_bIsSingleSide = false;
    m_bIsRectangle = false;
    m_bDynamic = false;
    m_bIsLocked = false;
    m_bNeedRestoring = false;
    m_bNoTexture = false;
    m_bIsPowerByTwo = true;
    m_bResolved = true;

    m_bWasUnloaded = false;
    m_bPartyallyLoaded = false;
    m_bVersionWasChecked = false;
    m_bDiscardInCache = false;
    m_bStreamFromDDS = false;
    m_bStreamingInProgress = false;
    m_bStream = false;
    m_bStreamPrepared = false;
    m_bStreamRequested = false;
    m_bReleasePostpouned = false;
    m_bVertexTexture = false;
		m_bUseDecalBorderCol = false;
    m_nAsyncStartMip = 0;
    m_nFrameCache = 0;
    m_Next = NULL;
    m_Prev = NULL;
    m_fMinDistance = 0;
    m_nMinMipCur = 0;
    m_nMinMipSysUploaded = 100;
    m_nMinMipVidUploaded = 100;

    m_pPoolItem = NULL;
    m_nTexSizeHistory = 0;
    m_nPhaseProcessingTextures = 0;
    m_pFileTexMips = NULL;
  }
  virtual ~CTexture();

  int m_nDefState;
  CTexture *m_NextTxt;
  float m_fAnimSpeed;
  float *m_Matrix;
  int m_nAccessFrameID;					// last read access, compare with GetFrameID(false)
  int m_nRTSetFrameID;					// last read access, compare with GetFrameID(false)
  int m_nUpdateFrameID;					// last write access, compare with GetFrameID(false)
  int m_nPool;

  // ITexture interface
  virtual int AddRef() { return CBaseResource::AddRef(); }
  virtual int Release()
  {
    if (!(m_nFlags & FT_DONT_RELEASE) || IsLocked())
    {
      if (m_bStreamingInProgress)
      {
        if (GetRefCounter() > 1)
          return CBaseResource::Release();
        m_bReleasePostpouned = true;
        return 0;
      }
      return CBaseResource::Release();
    }
    return -1;
  }
  virtual int ReleaseForce()
  {
    m_nFlags &= ~FT_DONT_RELEASE;
    int nRef = 0;
    while (true)
    {
      int nRef = Release();
      if (nRef <= 0)
        break;
    }
    return nRef;
  }
  virtual const char *GetName() { return GetSourceName(); }
  virtual int GetWidth() { return m_nWidth; }
  virtual int GetHeight() { return m_nHeight; }
  virtual int GetSourceWidth() { return m_nSrcWidth; }
  virtual int GetSourceHeight() { return m_nSrcHeight; }
  virtual int GetTextureID() { return GetID(); }
  virtual uint GetFlags() { return m_nFlags; }
	virtual int GetNumMips() { return m_nMips; }
  _inline int GetNumDevMips() { return m_nMips3DC; }
  virtual ETEX_Type GetTextureType() { return m_eTT; }
  virtual void SetClamp(bool bEnable)
  {
    int nMode = bEnable ? TADDR_CLAMP : TADDR_WRAP;
    SetClampingMode(nMode, nMode, nMode);
  }
  virtual bool IsTextureLoaded() { return IsLoaded(); }
  virtual void PrecacheAsynchronously(float fDist, int Flags) {}
  virtual void Preload (int nFlags) {};
  virtual byte *GetData32(int nSide=0, int nLevel=0);
  virtual int GetDeviceDataSize() { return m_nDeviceSize; }
  virtual int GetDataSize() { return m_nSize; }
  virtual byte *LockData(int& nPitch, int nSide=0, int nLevel=0);
  virtual void UnlockData(int nSide=0, int nLevel=0);
  virtual bool SaveTGA(const char *szName, bool bMips=false);
  virtual bool SaveJPG(const char *szName, bool bMips=false);
  virtual bool SaveDDS(const char *szName, bool bMips=true);
  virtual bool SetFilter(int nFilter) { return SetFilterMode(nFilter); }
  virtual bool Fill(const ColorF& color);
  virtual ColorF GetAvgColor() { return m_AvgColor; }

	virtual void GetMemoryUsage( ICrySizer *pSizer );
	virtual const char* GetFormatName();
	virtual const char* GetTypeName();

	virtual bool IsShared() const
	{
		return CBaseResource::GetRefCounter() > 1;
	}

  // Internal functions
  ETEX_Format GetDstFormat() { return m_eTFDst; }
  ETEX_Format GetSrcFormat() { return m_eTFSrc; }
  ETEX_Type GetTexType() { return m_eTT; }

  bool IsNoTexture() { return m_bNoTexture; };
  void SetNeedRestoring() { m_bNeedRestoring = true; }
  void SetWasUnload(bool bSet) { m_bWasUnloaded = bSet; }
  bool IsUnloaded(void) { return m_bWasUnloaded; }
  void SetPartyallyLoaded(bool bSet) { m_bPartyallyLoaded = bSet; }
  void SetStreamingInProgress(bool bSet) { m_bStreamingInProgress = bSet; }
  void ResetNeedRestoring() { m_bNeedRestoring = false; }
  bool IsNeedRestoring() { return m_bNeedRestoring; }
  bool IsPowerByTwo() { return m_bIsPowerByTwo; }
  bool IsResolved() { return m_bResolved; }
  bool IsVertexTexture() { return m_bVertexTexture; }
  void SetVertexTexture( bool bEnable ) { m_bVertexTexture = bEnable; }  
  bool IsDynamic() { return ((m_nFlags & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET)) != 0); }
  bool IsStreamFromDDS() { return m_bStreamFromDDS; }
  void Lock() { m_bIsLocked = true; }
  void Unlock() { m_bIsLocked = false; }
  bool IsLoaded() { return ((m_nFlags & FT_FAILED) == 0); }
  bool IsLocked() { return m_bIsLocked; }
  bool IsStreamed() { return m_bStream; }
  void SetResolved(bool bResolved) { m_bResolved = bResolved; }
  int8 GetPriority() const { return m_nPriority; }
  int GetCustomID() { return m_nCustomID; }
  void SetCustomID(int nID) { m_nCustomID = nID; }
  void SetFlags(uint nFlags) { m_nFlags |= nFlags; }
  void ResetFlags(uint nFlags) { m_nFlags &= ~nFlags; }
  int GetDefState() const { return m_nDefState; }
  bool HasAlphaChannel() { return m_bHasAlphaChannel; }
	bool UseDecalBorderCol() { return m_bUseDecalBorderCol; }
  bool IsSingleSide() { return m_bIsSingleSide; }
  bool IsRectangle() { return m_bIsRectangle; }
  void *GetDeviceTexture() const { return m_pDeviceTexture; }
  void *GetDeviceRT() const { return m_pDeviceRTV; }
#if defined (DIRECT3D10)
  void *GetDeviceResource() const { return m_pDeviceShaderResource; }
  void *GetDeviceTextureMSAA() const { return m_pDeviceTextureMSAA; }
  void *GetDeviceResourceMS() const { return m_pDeviceShaderResourceMS; }
  void *GetDeviceResource(int nSlice) const { assert(nSlice >= 0 && nSlice < 4); return m_pShaderResourceViews[nSlice]; } 
  void *GetDeviceDepthStencilSurf() const { return m_pDeviceDepthStencilSurf; }
  void *GetDeviceDepthStencilCMSurf(int nCMSide) const { assert(nCMSide >= 0 && nCMSide < 6); return m_pDeviceDepthStencilCMSurfs[nCMSide]; }
#endif
  void *GetSurface(int nCMSide, int nLevel);
  SPixFormat *GetPixelFormat() { return m_pPixelFormat; }
  void AssignDeviceTexture(void *pNewTex) { m_pDeviceTexture = pNewTex; }
  bool Invalidate(int nNewWidth, int nNewHeight, ETEX_Format eTF);
  const char *GetSourceName() const { return m_SrcName.c_str(); }
  int GetSize();
  void PostCreate();

  //===================================================================
  // Streaming support
  static CTexture m_Root;
  
  static int m_nTexSizeHistory;
  static int m_TexSizeHistory[8];
  static int m_nPhaseProcessingTextures;
  static int m_nProcessedTextureID1;
  static CTexture *m_pProcessedTexture1;
  static int m_nProcessedTextureID2;
  static CTexture *m_pProcessedTexture2;
  static TArray<CTexture *> m_Requested;

  CTexture *m_Next;           //!<
  CTexture *m_Prev;           //!<
  STexCacheFileHeader m_CacheFileHeader;
  STexCacheMipHeader *m_pFileTexMips;
  int m_nFrameCache;
  float m_fMinDistance;
  int m_nAsyncStartMip;
  int m_nMinMipCur;
  int m_nMinMipSysUploaded;
  int m_nMinMipVidUploaded;
  STexPoolItem *m_pPoolItem;

  static void Precache();

  static void StreamCheckTexLimits(CTexture *pExclude);
  static bool StreamValidateTexSize();
  static STexPool *StreamCreatePool(int nWidth, int nHeight, int nMips, ETEX_Format eTF, ETEX_Type eTT);
  static void CreatePools();
  static void StreamUnloadOldTextures(CTexture *pExclude);
  static void AsyncRequestsProcessing();

  void StreamLoadSynchronously(int nStartMip, int nEndMip);
  void StreamRestore();
  bool StreamRestoreMipsData();
  bool StreamCopyMips(int nStartMip, int nEndMip, bool bVideoResource);
  void StreamReleaseMipsData(int nStartMip, int nEndMip);
  void  StreamUpdateMip(float fDist);
  bool StreamUploadMips(int nStartMip, int nEndMip);
  int StreamUnload();
  int StreamSetLod(int nToMip, bool bUnload);
  bool StreamLoadFromCache(int nFlags, float fDist);
  bool StreamPrepare(bool bReload, ETEX_Format eTFDst);
  void StreamGetCacheName(char *name);
  bool StreamValidateMipsData(int nStartMip, int nEndMip);
  void StreamRemoveFromPool();
  bool StreamAddToPool(int nStartMip, int nMips);
  void StreamPrecacheAsynchronously(float fDist, int Flags);

  _inline void Relink(CTexture* Before)
  {
    if (!IsStreamed())
      return;
    if (m_Next && m_Prev)
    {
      m_Next->m_Prev = m_Prev;
      m_Prev->m_Next = m_Next;
    }
    m_Next = Before->m_Next;
    Before->m_Next->m_Prev = this;
    Before->m_Next = this;
    m_Prev = Before;
  }
  _inline void Unlink()
  {
    if (!m_Next || !m_Prev)
      return;
    m_Next->m_Prev = m_Prev;
    m_Prev->m_Next = m_Next;
    m_Next = m_Prev = NULL;
  }
  _inline void Link(CTexture* Before)
  {
    if (m_Next || m_Prev)
      return;
    m_Next = Before->m_Next;
    Before->m_Next->m_Prev = this;
    Before->m_Next = this;
    m_Prev = Before;
  }
  //=======================================================

  static void ApplyForID(int nID, int nTS=-1, int nSamplerSlot=-1)
  {
    CTexture *pTex = GetByID(nID);
    assert (pTex);
    if (pTex)
      pTex->Apply(-1, nTS, -1, nSamplerSlot);
  }
  static CCryName mfGetClassName();
  static CTexture *GetByID(int nID);
  static CTexture *GetByName(const char *szName);
  static CTexture *ForName(const char *name, uint nFlags, ETEX_Format eTFDst, float fAmount1=-1, float fAmount2=-1, int8 m_nPriority=-1);
  static CTexture *CreateTextureArray(const char *name, uint nWidth, uint nHeight, uint nArraySize, uint nFlags, ETEX_Format eTF, int nCustomID=-1, int8 nPriority=-1);
  static CTexture *CreateRenderTarget(const char *name, uint nWidth, uint nHeight, ETEX_Type eTT, uint nFlags, ETEX_Format eTF, int nCustomID=-1, int8 nPriority=-1);
  static CTexture *CreateTextureObject(const char *name, uint nWidth, uint nHeight, int nDepth, ETEX_Type eTT, uint nFlags, ETEX_Format eTF, int nCustomID=-1, int8 nPriority=-1);
  static int  ComponentsForFormat(ETEX_Format eTF);
  static bool SetDefTexFilter(const char *szFilter);
  
  static void InitStreaming();
  static void Init();
  static void ShutDown();

  static void LoadDefaultTextures();
  static void GenerateFlareMap();
  static void GenerateNoiseVolumeMap();
  static void GenerateNormalizationCubeMap();
  static void GenerateVectorNoiseVolMap();
  static CTexture *GenerateMipsColorMap(int nWidth, int nHeight);
  static void GenerateFuncTextures();
	static void Generate_16_PointsOnSphere();

  static CTexture *MakeSpecularTexture(float fExp);
  static bool ReloadFile(const char *szFileName, uint nFlags);
  static void ReloadTextures();
  static CTexture *Create2DTexture(const char *szName, int nWidth, int nHeight, int nMips, int nFlags, byte *pData, ETEX_Format eTFSrc, ETEX_Format eTFDst, int8 nPriority=-1);
  static CTexture *Create3DTexture(const char *szName, int nWidth, int nHeight, int nDepth, int nMips, int nFlags, byte *pData, ETEX_Format eTFSrc, ETEX_Format eTFDst, int8 nPriority=-1);
  static void Update();    

  // Loading/creating functions
  bool SaveCube(string& name);
  bool Load(ETEX_Format eTFDst, float fAmount1, float fAmount2);
  bool LoadFromImage(const char *name, const bool bReload, ETEX_Format eTFDst, float fAmount1=-1, float fAmount2=-1);
  bool Reload(uint nFlags);
  bool ToggleStreaming(int bOn);
  bool CreateTexture(STexData Data[6]);

  // API depended functions
  bool Resolve();
  bool CreateDeviceTexture(byte *pData[6]);
  bool CreateRenderTarget(ETEX_Format eTF);
  void ReleaseDeviceTexture(bool bKeepLastMips);
  void Apply(int nTUnit=-1, int nTS=-1, int nTSlot=-1, int nSSlot=-1, int nResView=-1);
  void SetSamplerState(int nTUnit, int nTS, int nSSlot, int nOffSet = 0);
  ETEX_Format ClosestFormatSupported(ETEX_Format eTFDst);
  void SetTexStates();
	void UpdateTexStates();
  bool SetFilterMode(int nFilter);
  bool SetClampingMode(int nAddressU, int nAddressV, int nAddressW);
  void UpdateTextureRegion(byte *data, int X, int Y, int USize, int VSize, ETEX_Format eTFSrc);
  bool Create2DTexture(int nWidth, int nHeight, int nMips, int nFlags, byte *pData, ETEX_Format eTFSrc, ETEX_Format eTFDst);
	bool Create3DTexture(int nWidth, int nHeight, int nDepth, int nMips, int nFlags, byte *pData, ETEX_Format eTFSrc, ETEX_Format eTFDst);
  bool SetNoTexture();
  bool IsFSAAChanged();

  // Helper functions
  static bool SetProjector(int nUnit, int nState, int nSamplerSlot);
  static bool IsFormatSupported(ETEX_Format eTFDst);
  static void GenerateZMaps();
  static void DestroyZMaps();
  static void GenerateHDRMaps();
	// allocate or deallocate star maps
	static void DestroyHDRMaps();
  static void GenerateSceneMap(ETEX_Format eTF);
  static void DestroySceneMap();	

  static void DestroyLightInfo();
  static void GenerateLightInfo(ETEX_Format eTF, int nWidth, int nHeight);

  static void GenerateSkyDomeTextures(uint32 width, uint32 height);
	static void DestroySkyDomeTextures();
  static int GetMaxTextureSize() { return m_nMaxtextureSize; }
	static bool IsFourBit(ETEX_Format eTF)
	{
		return (16 == BitsPerPixel(eTF));
	}
  static bool IsDXTCompressed(ETEX_Format eTF)
  {
    if (eTF == eTF_DXT1 || eTF == eTF_DXT3 || eTF == eTF_DXT5 || eTF == eTF_3DC)
      return true;
    return false;
  }
  static void BindNULLFrom(int nForm);
  static byte *Convert(byte *pSrc, int nWidth, int nHeight, int nMips, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nOutMips, int& nOutSize, bool bLinear);
  static int CalcNumMips(int nWidth, int nHeight);
  static int TextureDataSize(int nWidth, int nHeight, int nDepth, int nMips, ETEX_Format eTF);
  static int BitsPerPixel(ETEX_Format eTF);
  static const char *NameForTextureFormat(ETEX_Format eTF);
  static const char *NameForTextureType(ETEX_Type eTT);
  static ETEX_Format TextureFormatForName(const char *str);
  static ETEX_Type TextureTypeForName(const char *str);
  static int DeviceFormatFromTexFormat(ETEX_Format eTF);
  static ETEX_Format TexFormatFromDeviceFormat(int nFormat);
  static int GetD3DLinFormat(int nFormat);
  static int ConvertToDepthStencilFmt(int nFormat);
  static int ConvertToShaderResourceFmt(int nFormat);

  static SEnvTexture *FindSuitableEnvLCMap(Vec3& Pos, bool bMustExist, int RendFlags, float fDistToCam, CRenderObject *pObj=NULL);
  static SEnvTexture *FindSuitableEnvCMap(Vec3& Pos, bool bMustExist, int RendFlags, float fDistToCam);
  static SEnvTexture *FindSuitableEnvTex(Vec3& Pos, Ang3& Angs, bool bMustExist, int RendFlags, bool bUseExistingREs, CShader *pSH, SRenderShaderResources *pRes, CRenderObject *pObj, bool bReflect, CRendElement *pRE, bool *bMustUpdate);
  static bool ScanEnvironmentCM(const char *name, int size, Vec3& Pos);
  static bool ScanEnvironmentCube(SEnvTexture *pEnv, uint nRendFlags, int Size, bool bLightCube);
  static void DrawCubeSide(Vec3& Pos, int tex_size, int side, int RendFlags, float fMaxDist);
  static void GetAverageColor(SEnvTexture *cm, int nSide);

  static TArray<STexGrid> m_TGrids;
  static void SetGridTexture(int nTUnit, CTexture *pThis, int nTSlot);

  static void GetBackBuffer(CTexture *pTex, int nRendFlags);

public:
  static SEnvTexture *m_pCurEnvTexture;

  static int m_nCurState;
  static std::vector<STexState> m_TexStates;
  static int GetTexState(const STexState& TS)
  {
    int i;

    for (i=0; i<m_TexStates.size(); i++)
    {
      STexState *pTS = &m_TexStates[i];
      if (*pTS == TS)
        break;
    }
    if (i == m_TexStates.size())
    {
      m_TexStates.push_back(TS);
      m_TexStates[i].PostCreate();
    }
    return i;
  }


  static CTexture *m_pCurrent;
  static string m_GlobalTextureFilter;
  static int m_nGlobalDefState;
  static STexState m_sDefState;
  static STexState m_sGlobalDefState;
  static STexStageInfo m_TexStages[MAX_TMU];
  static int m_TexStateIDs[MAX_TMU];
  static int m_CurStage;
  static int m_nStreamed;
  static int m_nStreamingMode;
  static bool m_bStreamOnlyVideo;
  static bool m_bStreamDontKeepSystem;
  static bool m_bPrecachePhase;
  static bool m_bStreamingShow;
  static int m_nCurStages;

  static byte *m_pLoadData;
  static int m_nLoadOffset;

  static ETEX_Format m_eTFZ;
  static int m_LoadBytes;
  static int m_UploadBytes;
  static float m_fAdaptiveStreamDistRatio;
  static float m_fResolutionRatio;
  static int m_StatsCurManagedStreamedTexMem;
  static int m_StatsCurManagedNonStreamedTexMem;
  static int m_StatsCurDynamicTexMem;

  static int m_nCurTexResolution;
  static int m_nCurTexBumpResolution;

  // ==============================================================================
  static CTexture* m_Text_NoTexture;
  static CTexture *m_Text_White;
  static CTexture *m_Text_Gray;
  static CTexture *m_Text_Black;
  static CTexture *m_Text_FlatBump;
  static CTexture *m_Text_PaletteDebug;
  static CTexture *m_Text_IconShaderCompiling;
  static CTexture *m_Text_IconStreaming;
  static CTexture *m_Text_IconStreamingTerrainTexture;
  static CTexture *m_Text_MipColors_Diffuse;
  static CTexture *m_Text_MipColors_Bump;
  static CTexture *m_Text_ScreenNoiseMap;
	static CTexture *m_Text_Noise2DMap;
	static CTexture *m_Text_NoiseBump2DMap;
  static CTexture *m_Text_NoiseVolumeMap;
  static CTexture *m_Text_NormalizationCubeMap;
  static CTexture *m_Text_VectorNoiseVolMap;
  static CTexture *m_Text_LightCMap;
  static CTexture *m_Text_FromRE[8];
  static CTexture *m_Text_ShadowID[8];
  static CTexture *m_Text_FromRE_FromContainer[2];
  static CTexture *m_Text_FromObj[2];
  static CTexture *m_Text_FromLight;
  static CTexture *m_Text_RT_2D;
  static CTexture *m_Text_RT_CM;
  static CTexture *m_Text_RT_LCM;
  static CTexture *m_Text_Flare;  
	static CTexture *m_Text_16_PointsOnSphere;  
  static CTexture *m_Text_ScreenMap;                 // screen buffer
  static CTexture *m_Text_ScreenShadowMap[MAX_REND_LIGHT_GROUPS]; // screen render targets for shadow masks
  static CTexture *m_Tex_CurrentScreenShadowMap[MAX_REND_LIGHT_GROUPS]; // screen render targets for shadow masks
  static CTexture *m_Text_AOTarget;             
  
  static CTexture *m_Text_BackBuffer;                // back buffer copy  
  static CTexture *m_Text_BackBufferScaled[3];       // backbuffer low-resolution/blured version. 2x and 4x smaller than screen
  static CTexture *m_Text_BackBufferLum[2];          // back buffer average luminance, current and previous frame
  static CTexture *m_Text_BackBufferAvgCol[2];       // back buffer average color, current and previous frame
  
  // depreceated - will be removed
  static CTexture *m_Text_EffectsAccum;              // shared effects accumulation texture  (rgb = unused atm, alpha = blood splats displacement)
  // depreceated - will be removed
  static CTexture *m_Text_Glow;  

  static CTexture *m_Text_WaterOcean;                // water ocean vertex texture
  static CTexture *m_Text_WaterVolumeDDN;               // water volume heightmap
  static CTexture *m_Text_WaterVolumeTemp;               // water volume heightmap
  static CTexture *m_Text_WaterPuddles[2];           // water wave propagation maps
  static CTexture *m_Text_WaterPuddlesDDN;           // final propagated waves normal map (and height in alpha)

  static CTexture *m_Text_CurrentScatterLayer;
  static CTexture *m_Text_ScatterLayer;
  static CTexture *m_Text_TranslucenceLayer;

  static CTexture *m_Text_RT_ShadowPool;
  static CTexture *m_Text_RT_NULL;
  static CTexture *m_Text_TerrainLM;
  static CTexture *m_Text_CloudsLM;

  static CTexture *m_Text_LightInfo[4];
  static CTexture *m_Text_SceneTarget;  // Scene for refraction
  static CTexture *m_Text_SceneTargetFSAA;  
  
  static CTexture *m_Text_ZTarget;
  static CTexture *m_Text_ZTargetMS;
  static CTexture *m_Text_ZTargetScaled;

  static CTexture *m_Text_HDRTarget;
  static CTexture *m_Text_HDRTargetScaled[3];
  static CTexture *m_Text_HDRBrightPass[2];
  static CTexture *m_Text_HDRAdaptedLuminanceCur[8];
  static int m_nCurLumTexture;
  static CTexture *m_pCurLumTexture;
  static CTexture *m_pIssueLumTexture;
  static CTexture *m_Text_HDRToneMaps[NUM_HDR_TONEMAP_TEXTURES];
	static CTexture *m_SkyDomeTextureMie;
	static CTexture *m_SkyDomeTextureRayleigh;
	static CTexture *m_Text_FromSF[NUM_SCALEFORM_TEXTURES];

	static CTexture *m_Text_VolObj_Density;
	static CTexture *m_Text_VolObj_Shadow;

  static SEnvTexture m_EnvLCMaps[MAX_ENVLIGHTCUBEMAPS];
  static SEnvTexture m_EnvCMaps[MAX_ENVCUBEMAPS];
  static SEnvTexture m_EnvTexts[MAX_ENVTEXTURES];  
  static TArray<SEnvTexture> m_CustomRT_CM;
  static TArray<SEnvTexture> m_CustomRT_2D;

  static TArray<STexPool *> m_TexPools;
  static STexPoolItem m_FreeTexPoolItems;

  static CTexture m_Templates[EFTT_MAX];

  static CTexture *m_pBackBuffer;
  static CTexture m_FrontTexture[2];

private: // ------------------------------------------------------------------

	static std::vector<CTexture *> m_Text_HDRStarMaps;		// access through GetStarTexture()
};

int sLimitSizeByScreenRes(uint size);

bool WriteTGA(byte*dat, int wdt, int hgt, const char*name, int src_bits_per_pixel, int dest_bits_per_pixel );
bool WriteJPG(byte*dat, int wdt, int hgt, const char*name, int bpp);
byte * WriteDDS(byte*dat, int wdt, int hgt, int dpth, const char*name, ETEX_Format eTF, int nMips, ETEX_Type eTT, bool bToMemory=false, int* nSize=NULL);

//////////////////////////////////////////////////////////////////////////
class CDynTextureSource : public IDynTextureSource
{
public:
	CDynTextureSource(ETexPool eTexPool);

	virtual void AddRef();
	virtual void Release();
	
	virtual bool Apply(int nTUnit, int nTS=-1);	
	virtual void GetTexGenInfo(float& offsX, float& offsY, float& scaleX, float& scaleY) const;

protected:
	virtual ~CDynTextureSource();
	/*virtual*/ void CalcSize(int& width, int& height, float distToCamera = -1) const;

protected:
	int m_refCount;
	float m_lastUpdateTime;
	SDynTexture2* m_pDynTexture;
};

//////////////////////////////////////////////////////////////////////////
class CFlashTextureSource : public CDynTextureSource
{
public:
	CFlashTextureSource(const char* pFlashFileName);

	virtual bool Update(float distToCamera);
	virtual void GetDynTextureSource(void*& pIDynTextureSource, EDynTextureSource& dynTextureSource);

protected:
	virtual ~CFlashTextureSource();

private:
	IFlashPlayer* m_pFlashPlayer;
#ifdef _DEBUG
	CCryName m_flashSrcFile;
#endif
};

//////////////////////////////////////////////////////////////////////////
class CVideoTextureSource : public CDynTextureSource
{
public:
	CVideoTextureSource(const char* pVideoFileName);

	virtual bool Update(float distToCamera);
	virtual void GetDynTextureSource(void*& pIDynTextureSource, EDynTextureSource& dynTextureSource);

protected:
	virtual ~CVideoTextureSource();

private:
	IVideoPlayer* m_pVideoPlayer;
#ifdef _DEBUG
	CCryName m_videoSrcFile;
#endif
};

#endif
