/*=============================================================================
  IShader.h : Shaders common interface.
  Copyright (c) 2001-2008 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Anton Kaplanyan

=============================================================================*/

#ifndef _ITEXTURE_H_
#define _ITEXTURE_H_

#include "Cry_Math.h"
#include "Cry_Color.h"
#include "Tarray.h"

class CTexture;

#ifndef BOOL
typedef int                 BOOL;
#endif
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef int                 INT;
typedef unsigned int        UINT;

#ifndef uchar
typedef unsigned char   uchar;
typedef unsigned int    uint;
typedef unsigned short  ushort;
#endif

enum ETEX_Type
{
	eTT_1D = 0,
	eTT_2D,
	eTT_3D,
	eTT_Cube,
	eTT_AutoCube,
	eTT_Auto2D,
	eTT_User,
};

// Texture formats
enum ETEX_Format
{
	eTF_Unknown = 0,
	eTF_R8G8B8,
	eTF_A8R8G8B8,
	eTF_X8R8G8B8,
	eTF_A8,
	eTF_A8L8,
	eTF_L8,
	eTF_A4R4G4B4,
	eTF_R5G6B5,
	eTF_R5G5B5,
	eTF_V8U8,
	eTF_CxV8U8,
	eTF_X8L8V8U8,
	eTF_L8V8U8,
	eTF_L6V5U5,
	eTF_V16U16,
	eTF_A16B16G16R16,
	eTF_A16B16G16R16F,
	eTF_A32B32G32R32F,
	eTF_G16R16F,
	eTF_R16F,
	eTF_R32F,
	eTF_DXT1,
	eTF_DXT3,
	eTF_DXT5,
	eTF_3DC,

	eTF_G16R16,

	eTF_NULL,

	//hardware depth buffers
	eTF_DF16,
	eTF_DF24,
	eTF_D16,
	eTF_D24S8,

	eTF_D32F,

	eTF_DEPTH16,
	eTF_DEPTH24,

#if defined(XENON)
	eTF_A8_LIN,
#endif
  eTF_A2R10G10B10,
};

#define FT_NOMIPS           0x1
#define FT_TEX_NORMAL_MAP   0x2
#define FT_TEX_SKY          0x4
#define FT_USAGE_DEPTHSTENCIL 0x8
#define FT_TEX_LM           0x10
#define FT_FILESINGLE			  0x20				// supress loading of attached alpha
#define FT_TEX_FONT         0x40
#define FT_HAS_ATTACHED_ALPHA 0x80
#define FT_DONTSYNCMULTIGPU 0x100				// through NVAPI we tell driver not to sync
#define FT_FORCE_CUBEMAP    0x200
#define FT_USAGE_FSAA       0x400
#define FT_FORCE_MIPS       0x800
#define FT_USAGE_RENDERTARGET 0x1000
#define FT_USAGE_DYNAMIC    0x2000
#define FT_DONT_RESIZE      0x4000
#define FT_DONT_ANISO       0x8000
#define FT_DONT_RELEASE     0x10000
#define FT_DONT_GENNAME     0x20000
#define FT_DONT_STREAM      0x40000
#define FT_USAGE_PREDICATED_TILING 0x80000
#define FT_FAILED           			0x100000
#define FT_FROMIMAGE        			0x200000
#define FT_STATE_CLAMP      			0x400000
#define FT_USAGE_ATLAS      			0x800000
#define FT_ALPHA            			0x1000000
#define FT_REPLICATE_TO_ALL_SIDES 0x2000000
#define FT_BIG_ENDIANNESS					0x4000000
#define FT_FILTER_POINT     			0x10000000
#define FT_FILTER_LINEAR    			0x20000000
#define FT_FILTER_BILINEAR  			0x30000000
#define FT_FILTER_TRILINEAR 			0x40000000
#define FT_FILTER_ANISO2    			0x50000000
#define FT_FILTER_ANISO4    			0x60000000
#define FT_FILTER_ANISO8    			0x70000000
#define FT_FILTER_ANISO16   			0x80000000
#define FT_FILTER_MASK      			0xf0000000

//#define FT_AFFECT_INSTANCE  (FT_ALLOW_3DC | FT_NOMIPS | FT_TEX_NORMAL_MAP | FT_FORCE_DXT | FT_FORCE_CUBEMAP | FT_REPLICATE_TO_ALL_SIDES | FT_FILTER_MASK | FT_ALPHA)
#define FT_AFFECT_INSTANCE  (FT_NOMIPS | FT_TEX_NORMAL_MAP | FT_USAGE_FSAA | FT_FORCE_CUBEMAP | FT_REPLICATE_TO_ALL_SIDES | FT_FILTER_MASK | FT_ALPHA)

struct SD3DSurface
{
	int nWidth;
	int nHeight;
	bool bBusy;
	int nFrameAccess;
#ifdef XENON
	int nTilesX;
	int nTilesY;
	int nTilesOffset;

	uint32 HiZBase;
#endif
	void *pSurf;
	void *pTex;
	SD3DSurface()
	{
		pSurf = NULL;
		pTex = NULL;
		bBusy = false;
		nFrameAccess = -1;
#ifdef XENON
		nTilesX = 0;
		nTilesY = 0;
		nTilesOffset = 0;
		HiZBase = 0xFFFFFFFF;
#endif
	}
	~SD3DSurface();
};

//////////////////////////////////////////////////////////////////////
// Texture object interface
class ITexture
{
public:
	virtual int AddRef()=0;
	virtual int Release()=0;
	virtual int ReleaseForce()=0;

	virtual const char *GetName()=0;
	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;
	virtual int GetSourceWidth() = 0;
	virtual int GetSourceHeight() = 0;
	virtual int GetTextureID() = 0;
	virtual uint GetFlags() = 0;
	virtual int GetNumMips() = 0;
	virtual int GetDeviceDataSize() = 0;
	virtual ETEX_Type GetTextureType() = 0;
	virtual bool IsTextureLoaded() = 0;
	virtual void PrecacheAsynchronously(float fDist, int nFlags) = 0;
	virtual void Preload (int nFlags)=0;
	//virtual byte *GetData32(int nSide=0, int nLevel=0, byte * pDst=NULL)=0;
	virtual byte *GetData32(int nSide=0, int nLevel=0) = 0;
	virtual byte *LockData(int& nPitch, int nSide=0, int nLevel=0)=0;
	virtual void UnlockData(int nSide=0, int nLevel=0)=0;
	virtual bool SaveTGA(const char *szName, bool bMips=false)=0;
	virtual bool SaveJPG(const char *szName, bool bMips=false)=0;
	virtual bool SetFilter(int nFilter)=0;   // FILTER_ flags
	virtual void SetClamp(bool bEnable) = 0; // Texture addressing set
	virtual ColorF GetAvgColor() = 0;

	// Used for debugging/profiling.
	virtual const char* GetFormatName() = 0;
	virtual const char* GetTypeName() = 0;
	virtual bool IsShared() const = 0;
};

//=========================================================================================

struct IDynTextureSource
{
	virtual void AddRef() = 0;
	virtual void Release() = 0;

	virtual bool Update(float distToCamera) = 0;
	virtual bool Apply(int nTUnit, int nTS = -1) = 0;	
	virtual void GetTexGenInfo(float& offsX, float& offsY, float& scaleX, float& scaleY) const = 0;

	enum EDynTextureSource
	{
		DTS_I_FLASHPLAYER,
		DTS_I_VIDEOPLAYER
	};	
	virtual void GetDynTextureSource(void*& pIDynTextureSource, EDynTextureSource& dynTextureSource) = 0;
};

//=========================================================================================

class IDynTexture
{
public:
	virtual void Release() = 0;
	virtual void GetSubImageRect(uint32 & nX, uint32 & nY, uint32 & nWidth, uint32 & nHeight) = 0;
	virtual void GetImageRect(uint32 & nX, uint32 & nY, uint32 & nWidth, uint32 & nHeight) = 0;
	virtual int GetTextureID() = 0;
	virtual void Lock() = 0;
	virtual void UnLock() = 0;
	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;
	virtual bool IsValid() = 0;
	virtual bool Update(int nNewWidth, int nNewHeight)=0;
	virtual void Apply(int nTUnit, int nTS=-1)=0;
	//virtual bool SetRT(int nRT, bool bPush, struct SD3DSurface *pDepthSurf, bool bScreenVP=false)=0;
	virtual bool SetRT(int nRT, bool bPush, SD3DSurface *pDepthSurf) = 0;
	virtual bool SetRectStates()=0;
	virtual bool RestoreRT(int nRT, bool bPop)=0;
	virtual ITexture *GetTexture()=0;
	virtual void SetUpdateMask()=0;
	virtual void ResetUpdateMask()=0;
	virtual bool IsSecondFrame()=0;
};

// Animating Texture sequence definition
struct STexAnim
{
	TArray<CTexture *> m_TexPics;
	int m_Rand;
	int m_NumAnimTexs;
	bool m_bLoop;
	float m_Time;

	int Size()
	{
		int nSize = sizeof(STexAnim);
		nSize += m_TexPics.GetMemoryUsage();
		return nSize;
	}

	STexAnim()
	{
		m_Rand = 0;
		m_NumAnimTexs = 0;
		m_bLoop = false;
		m_Time = 0.0f;
	}

	~STexAnim()
	{     
		for (uint i=0; i<m_TexPics.Num(); i++)
		{
			ITexture *pTex = (ITexture *) m_TexPics[i];
			SAFE_RELEASE(pTex);
		}
		m_TexPics.Free();
	}

	STexAnim& operator = (const STexAnim& sl)
	{
		// make sure not same object
		if(this == &sl)   
		{
			return *this;
		}

		for (uint i=0; i<m_TexPics.Num(); i++)
		{
			ITexture *pTex = (ITexture *)m_TexPics[i];
			SAFE_RELEASE(pTex);
		}
		m_TexPics.Free();

		for (uint i=0; i<sl.m_TexPics.Num(); i++)
		{
			ITexture *pTex = (ITexture *)sl.m_TexPics[i];
			if(pTex)
			{
				pTex->AddRef();
			}

			m_TexPics.AddElem(sl.m_TexPics[i]);
		}

		m_Rand = sl.m_Rand;
		m_NumAnimTexs = sl.m_NumAnimTexs;
		m_bLoop = sl.m_bLoop;
		m_Time = sl.m_Time;

		return *this;
	}
};

#endif// _ITEXTURE_H_
