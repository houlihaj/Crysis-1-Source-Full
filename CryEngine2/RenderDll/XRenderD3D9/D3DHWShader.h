/*=============================================================================
  D3DCGPShader.h : Direct3D9 CG pixel shaders interface declaration.
  Copyright (c) 2001-2005 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#ifndef __D3DHWSHADER_H__
#define __D3DHWSHADER_H__

#if !defined(OPENGL)
	#define MERGE_SHADER_PARAMETERS 1
#endif
// Streams redefinitions for OpenGL/PS3 (TEXCOORD#)
#define VSTR_COLOR2        2  //SH Stream (VSF_SH_INFO)
#define VSTR_COLOR3        3  //SH Stream (VSF_SH_INFO)
#define VSTR_COLOR1        4  //Base Stream
#define VSTR_NORMAL1       5  //Base Stream
#define VSTR_PSIZE         6  //Base particles stream
#define VSTR_MORPHTARGETDELTA 7 // MorphTarget stream (VSF_HWSKIN_MORPHTARGET_INFO)
#define VSTR_TANGENT       8  // Tangents stream (VSF_TANGENTS)
#define VSTR_BINORMAL      9  // Tangents stream (VSF_TANGENTS)
#define VSTR_BLENDWEIGHTS 10  // HWSkin stream (VSF_HWSKIN_INFO)
#define VSTR_BLENDINDICES 11  // HWSkin stream (VSF_HWSKIN_INFO)
#define VSTR_BONESPACE    12  // HWSkin stream (VSF_HWSKIN_INFO)
#define VSTR_SHAPEDEFORMINFO 13 // ShapeDeform stream (VSF_HWSKIN_SHAPEDEFORM)
#define VSTR_THIN         14  // ShapeDeform stream (VSF_HWSKIN_SHAPEDEFORM)
#define VSTR_FAT          15  // ShapeDeform stream (VSF_HWSKIN_SHAPEDEFORM)

#if defined (DIRECT3D9) || defined (OPENGL)
  #define MAX_CONSTANTS_PS 64
  #define MAX_CONSTANTS_VS 256
#else
#if !defined(PS3)
  #define MAX_CONSTANTS_PS 128
  #define MAX_CONSTANTS_VS 512
  #define MAX_CONSTANTS_GS 128
#else
  #define MAX_CONSTANTS 512
#endif
#endif

//==============================================================================

int D3DXGetSHParamHandle(void *pSH, SCGBind *pParam);

struct SCGConstant
{
  int nReg;
  Vec4 vConst;
};

enum ED3DShError
{
  ED3DShError_NotCompiled,
  ED3DShError_CompilingError,
  ED3DShError_Fake,
  ED3DShError_Ok,
  ED3DShError_Compiling,
};

struct SD3DShader
{
  int m_nRef;
  void *m_pHandle;

  SD3DShader()
  {
    m_nRef = 1;
    m_pHandle = NULL;
  }
  int AddRef()
  {
    return m_nRef++;
  }
  int Release(EHWShaderClass eSHClass)
  {
    m_nRef--;
    if (m_nRef)
			return m_nRef;
    void *pHandle = m_pHandle;
    delete this;
		if (!pHandle)
			return 0;
#if defined (DIRECT3D9) || defined (OPENGL)
    if (eSHClass == eHWSC_Pixel)
      return ((IDirect3DPixelShader9*)pHandle)->Release();
    else
      return ((IDirect3DVertexShader9*)pHandle)->Release();
#elif defined (DIRECT3D10)
    if (eSHClass == eHWSC_Pixel)
      return ((ID3D10PixelShader*)pHandle)->Release();
    else
    if (eSHClass == eHWSC_Vertex)
      return ((ID3D10VertexShader*)pHandle)->Release();
    else
    if (eSHClass == eHWSC_Geometry)
      return ((ID3D10GeometryShader*)pHandle)->Release();
    else
    {
      assert(0);
      return 0;
    }
#endif
  }
};

struct SD3DShaderHandle
{
  SD3DShader *m_pShader;
  byte *m_pData;
  int m_nData;
  byte m_bStatus;
  SD3DShaderHandle()
  {
    m_pShader = NULL;
    m_bStatus = 0;
    m_nData = 0;
    m_pData = NULL;
  }
  void SetShader(SD3DShader *pShader)
  {
    m_bStatus = 0;
    m_pShader = pShader;
  }
  void SetFake()
  {
    m_bStatus = 2;
  }
  void SetNonCompilable()
  {
    m_bStatus = 1;
  }
  int AddRef()
  {
    if (!m_pShader)
      return 0;
    return m_pShader->AddRef();
  }
  int Release(EHWShaderClass eSHClass)
  {
    if (!m_pShader)
      return 0;
    return m_pShader->Release(eSHClass);
  }

};

struct SShaderAsyncInfo
{
  SShaderAsyncInfo *m_Next;           //!<
  SShaderAsyncInfo *m_Prev;           //!<
  _inline void Unlink()
  {
    m_Next->m_Prev = m_Prev;
    m_Prev->m_Next = m_Next;
    m_Next = m_Prev = this;
  }
  _inline void Link(SShaderAsyncInfo* Before)
  {
    m_Next = Before->m_Next;
    Before->m_Next->m_Prev = this;
    Before->m_Next = this;
    m_Prev = Before;
  }
#if !defined (XENON) && defined (WIN32)
  PROCESS_INFORMATION m_ProcessInfo;
  HANDLE m_hPipeOutputRead;
	HANDLE m_hPipeOutputWrite;
#endif
  int m_nOwner;
  class CHWShader_D3D *m_pShader;
  CShader *m_pFXShader;

  LPD3DXBUFFER m_pDevShader;
  LPD3DXBUFFER m_pErrors;
  LPD3DXCONSTANTTABLE m_pConstants;
  CCryName m_Name;
  string m_Text;
  string m_Errors;
  string m_Profile;
  //CShaderThread *m_pThread;
  DynArray<SCGBind> m_InstBindVars;
  bool m_bPending;
  bool m_bPendedFlush;
  bool m_bPendedSamplers;
  bool m_bPendedPrint;
  float m_fMinDistance;
  int m_nFrame;

  SShaderAsyncInfo()
  {
    m_Next = m_Prev = this;
#if !defined (XENON) && defined (WIN32)
    memset(&m_ProcessInfo, 0, sizeof(PROCESS_INFORMATION));
    m_hPipeOutputRead = NULL;
    m_hPipeOutputWrite = NULL;
#endif
    m_fMinDistance = 0;
    m_nOwner = -1;
    m_pShader = NULL;
    m_pFXShader = NULL;
    m_pDevShader = NULL;
    m_pErrors = NULL;
    m_pConstants = NULL;
    m_bPending = true; //false; - this flag is now used as an atomic indication that if the async shader has been compiled

    m_bPendedFlush = false;
    m_bPendedPrint = false;
    m_bPendedSamplers = false;
  }
  ~SShaderAsyncInfo();
  static volatile LONG s_nPendingAsyncShaders;
};

#if defined (WIN32) || defined(XENON)

#include "CryThread.h"

class CAsyncShaderManager
{
	friend class CD3D9Renderer; // so it can instantiate us

public:
	void InsertPendingShader(SShaderAsyncInfo* pAsync);

private:
	CAsyncShaderManager();

	void FlushPendingShaders();

	typedef CryLock<CRYLOCK_FAST> FastLock;
	class CScopedLock
	{
	public:
		CScopedLock(FastLock& lock) : m_lock(lock) { m_lock.Lock(); }
		~CScopedLock() { m_lock.Unlock(); }
	private:
		FastLock& m_lock;
	};

	FastLock m_lock;

	SShaderAsyncInfo m_build_list;
	SShaderAsyncInfo m_flush_list;

	class CShaderThread : CrySimpleThread<>
	{
	public:
		CShaderThread(CAsyncShaderManager* man) : m_man(man), m_quit(false)
		{	
			Start();
		}

		~CShaderThread()
		{
			m_quit = true;
			Join();
		}

	private:
		void Run();

		CAsyncShaderManager* m_man;
		volatile bool m_quit;
	};

	CShaderThread m_thread;

	static void CompileAsyncShader(SShaderAsyncInfo* pAsync);
};
#endif

class CHWShader_D3D : public CHWShader
{
  friend class CD3D9Renderer;
  friend class CAsyncShaderManager;

  SShaderCache *m_pCache;

  struct SHWSInstance
  {
    SD3DShaderHandle m_Handle;
    EHWSProfile m_eProfileType;

    uint64 m_RTMask;      // run-time mask
    uint m_GLMask;        // global mask
    uint m_LightMask;     // light mask
    uint m_MDMask;        // texture coordinates modificator mask
    uint m_MDVMask;       // vertex modificator mask

    std::vector<SCGParam> *m_pParams[2];   // 0: Instance independent; 1: Object depended
    DynArray<STexSampler> *m_pSamplers;
    std::vector<SCGBind> *m_pBindVars;
    std::vector<SCGParam> *m_pParams_Inst;
#if defined (DIRECT3D10)
    void *m_pShaderData;
		size_t m_nShaderByteCodeSize;
    int m_nMaxVecs[3];
#endif
    short m_nInstMatrixID;
    short m_nInstIndex;
    short m_nInstructions;
    ushort m_VStreamMask_Stream;
    ushort m_VStreamMask_Decl;
    short m_nCache;
    bool m_bShared;
    bool m_bHasPMParams;
    bool m_bFallback;
    byte m_nVertexFormat;

    byte m_nNumInstAttributes;

    int m_DeviceObjectID;
    SShaderAsyncInfo *m_pAsync;
    SHWSInstance()
    {
      m_nNumInstAttributes = 0;
      m_RTMask = 0;
      m_GLMask = 0;
      m_LightMask = 0;
      m_MDMask = 0;
      m_MDVMask = 0;
      m_pParams_Inst = NULL;
      m_pParams[0] = NULL;
      m_pParams[1] = NULL;
      m_pSamplers = NULL;
      m_pBindVars = NULL;
      m_nInstructions = 0;
      m_bFallback = false;
      m_nInstMatrixID = 1;
      m_nCache = -1;
#if defined (DIRECT3D10)
      m_pShaderData = NULL;
			m_nShaderByteCodeSize = 0;
#endif
#if !defined (XENON) && defined (WIN32)
      m_pAsync = NULL;
#endif

      m_DeviceObjectID = -1;
      m_nInstIndex = -1;
      m_nVertexFormat = (byte)1;
      m_VStreamMask_Decl = 0;
      m_VStreamMask_Stream = 0;

      m_bShared = false;
      m_bHasPMParams = false;
    }
    void Release(SShaderCache *pCache=NULL);
    void CopyFrom(SHWSInstance *pInst, EHWShaderClass Class, SShaderCache *pCache);
    void AddParam(SCGParam *pParm, int n)
    {
      if (!m_pParams[n])
        m_pParams[n] = new std::vector<SCGParam>;
      m_pParams[n]->push_back(*pParm);
    }

    int Size()
    {
      int nSize = sizeof(*this);
      if (m_pParams[0])
        nSize += sizeofVector(*m_pParams[0]);
      if (m_pParams[1])
        nSize += sizeofVector(*m_pParams[1]);
      if (m_pParams_Inst)
        nSize += sizeofVector(*m_pParams_Inst);
      if (m_pSamplers)
        nSize += m_pSamplers->get_alloc_size();
      if (m_pBindVars)
        nSize += sizeofVector(*m_pBindVars);

      return nSize;
    }
    bool IsAsyncCompiling()
    {
#ifdef WIN32
      if (m_pAsync)
        return true;
#endif
      return false;
    }
  };
  struct SHWSSharedInstance
  {
    uint m_GLMask;
    TArray<SHWSInstance> m_Insts;
  };
  struct SHWSSharedName
  {
    string m_Name;
    uint32 m_CRC32;
  };
  struct SHWSSharedList
  {
    DynArray<SHWSSharedName> m_SharedNames;
    TArray<SHWSSharedInstance> m_SharedInsts;
    ~SHWSSharedList();
  };
  typedef std::map<CCryName, SHWSSharedList *> InstanceMap;
  typedef InstanceMap::iterator InstanceMapItor;

public:

#if defined(OPENGL)	
	#define VSCONST_0_025_05_1_OPENGL 1
	#define VSCONST_INSTDATA_OPENGL 4
	//#define VSCONST_SKINATRIX_OPENGL 5 // obsolete
	#define VSCONST_SKINQUATSL_OPENGL 5
	#define VSCONST_SHAPEDEFORMATION_OPENGL 6
	static const unsigned int scSGlobalConstMapCount = 7;	//total number of constants
	static const unsigned int scSGlobalConstVecCount = 4;	//number of vector constants
	//create global register mapping for handles, cg does not allow to map registers directly
	static SGlobalConstMap sGlobalConsts[scSGlobalConstMapCount];//used currently
#endif

  SHWSInstance *m_pCurInst;
  TArray<SHWSInstance> m_Insts;

  static InstanceMap m_SharedInsts;

  static int m_FrameObj;

  // FX support
  DynArray<STexSampler> m_Samplers;
  DynArray<SFXParam> m_Params;
  int m_nCurInstFrame;

  // Bin FX support
  DynArray<uint32> m_TokenData;
  FXShaderToken m_Table;

  virtual int Size()
  {
		int nSize = sizeof(*this) + m_Insts.GetMemoryUsage();
    for (uint i=0; i<m_Insts.Num(); i++)
    {
      nSize += m_Insts[i].Size();
    }
    nSize += m_TokenData.get_alloc_size();
    nSize += m_Samplers.get_alloc_size();
    nSize += m_Params.get_alloc_size();
    nSize += m_NameShader.capacity();
    nSize += m_NameSourceFX.capacity();
    nSize += sizeofArray(m_Table);

    return nSize;
  }
  CHWShader_D3D()
  {
    mfInit();
  }
  void mfInit()
  {
		if(ms_bInitShaders)
		{
			memset(m_CurPSParams,0,sizeof(Vec4)*32);
			memset(m_CurVSParams,0,sizeof(Vec4)*256);
			ms_bInitShaders = false;
		}

    m_pCurInst = NULL;
    m_FrameObj = -1;
    m_pCache = NULL;
    m_nCurInstFrame = 0;

    m_dwShaderType = 0;
  }

  void mfFree(uint32 CRC32)
  {
    m_Flags = 0;
    mfReset(CRC32);
  }

  //============================================================================
  // Binary cache support
  SShaderCacheHeaderItem *mfGetCacheItem(uint nFlags, uint& nSize);
  static bool mfAddCacheItem(SShaderCache *pCache, SShaderCacheHeaderItem *pItem, const byte *pData, int nLen, bool bFlush, CCryName Name);
  static bool mfFreeCacheItem(SHWSInstance *pInst, SShaderCache *pCache);

  bool mfCloseCacheFile()
  {
    SAFE_RELEASE (m_pCache);

    return true;
  }

  static byte *mfBindsToCache(SHWSInstance *pInst, DynArray<SCGBind>* Binds, int nParams, byte *pP);
  byte *mfBindsFromCache(DynArray<SCGBind>*& Binds, int nParams, byte *pP);

  bool mfActivateCacheItem(SShaderCacheHeaderItem *pItem, uint nSize);
  static bool mfCreateCacheItem(SHWSInstance *pInst, DynArray<SCGBind>& InstBinds, byte *pData, int nLen, int nDevObjectID, CHWShader_D3D *pSH, bool bShaderThread);

  //============================================================================

  std::vector<SCGParam> *mfGetParams(int Type)
  {
    assert(m_pCurInst);
    return m_pCurInst->m_pParams[Type];
  }

  bool mfSetHWStartProfile(uint nFlags);
  void mfSaveCGFile(const char *scr, const char *path);
  void mfOutputCompilerError(string& strErr, const char *szSrc);
  bool mfNextProfile(uint nFlags);
  static bool mfCreateShaderEnv(SHWSInstance *pInst, LPD3DXBUFFER pShader, LPD3DXCONSTANTTABLE pConstantTable, LPD3DXBUFFER pErrorMsgs, DynArray<SCGBind>& InstBindVars, CHWShader_D3D *pSH, bool bShaderThread);
  void mfPrintCompileInfo(SHWSInstance *pInst);
  bool mfCompileHLSL_Int(char *prog_text, LPD3DXBUFFER* ppShader, LPD3DXCONSTANTTABLE *ppConstantTable, LPD3DXBUFFER* ppErrorMsgs, string& strErr, DynArray<SCGBind>& InstBindVars);
  int mfAsyncCompileReady(SHWSInstance *pInst);
  bool mfRequestAsync(SHWSInstance *pInst, DynArray<SCGBind>& InstBindVars, const char *prog_text, const char *szProfile, const char *szEntry);
#ifdef WIN32
  bool mfPostCompiling(const char *szNameSrc, const char *szNameDst, LPD3DXBUFFER* ppShader, LPD3DXCONSTANTTABLE *ppConstantTable);
#endif
  LPD3DXBUFFER mfCompileHLSL(char *prog_text, LPD3DXCONSTANTTABLE *ppConstantTable, LPD3DXBUFFER* ppErrorMsgs, uint nFlags, DynArray<SCGBind>& InstBindVars);
  bool mfUploadHW(SHWSInstance *pInst, byte *pBuf, uint nSize);
  bool mfUploadHW(LPD3DXBUFFER pShader);
  ED3DShError mfIsValid(SHWSInstance *&pInst, bool bFinalise);
  ED3DShError mfFallBack(SHWSInstance *&pInst, int nStatus);

  void mfBind()
  {
    HRESULT hr = S_OK;
    if (mfIsValid(m_pCurInst, true) == ED3DShError_Ok)
    {
#if defined(DIRECT3D9) || defined(OPENGL)
      if (m_eSHClass == eHWSC_Pixel)
        hr = gcpRendD3D->GetD3DDevice()->SetPixelShader((IDirect3DPixelShader9 *)m_pCurInst->m_Handle.m_pShader->m_pHandle);
      else
        hr = gcpRendD3D->GetD3DDevice()->SetVertexShader((IDirect3DVertexShader9 *)m_pCurInst->m_Handle.m_pShader->m_pHandle);
#else
      if (m_eSHClass == eHWSC_Pixel)
        gcpRendD3D->GetD3DDevice()->PSSetShader((ID3D10PixelShader *)m_pCurInst->m_Handle.m_pShader->m_pHandle);
      else
      if (m_eSHClass == eHWSC_Vertex)
        gcpRendD3D->GetD3DDevice()->VSSetShader((ID3D10VertexShader *)m_pCurInst->m_Handle.m_pShader->m_pHandle);
      else
      if (m_eSHClass == eHWSC_Geometry)
        gcpRendD3D->GetD3DDevice()->GSSetShader((ID3D10GeometryShader *)m_pCurInst->m_Handle.m_pShader->m_pHandle);
#endif
#if defined(OPENGL)
			LPDIRECT3DDEVICE9 dv = gcpRendD3D->GetD3DDevice();
			for(unsigned int i=0; i<scSGlobalConstVecCount; ++i)
				sGlobalConsts[i].InitHandle(m_pCurInst->m_Handle.m_pShader->m_pHandle, dv, false/*throw handle away*/, false/*its a vector*/);
			for(unsigned int i=scSGlobalConstVecCount; i<scSGlobalConstMapCount; ++i)
			{
				if(sGlobalConsts[i].toBeUpdated == 1 || !sGlobalConsts[i].IsHandleValid())
					sGlobalConsts[i].InitHandle(m_pCurInst->m_Handle.m_pShader->m_pHandle, dv, false, true);//inits handle(if not already) and sets constants
			}
#endif
		}
    assert (!hr);
  }

  static void mfCommitParams(bool bSetPM);

#if defined (DIRECT3D9) || defined (OPENGL)
  static _inline void mfSetPSConst(int nReg, const float *vData, int nParams)
  {
#ifdef OPENGL
    gcpRendD3D->GetD3DDevice()->SetPixelShaderConstantF(nReg, vData, nParams);
    return;
#else
    int i;
    const float *v = vData;
    for (i=0; i<nParams; i++)
    {
      int n = nReg+i;
      float *pC = &m_CurPSParams[n].x;
//#ifdef XENON
//      float vCur[4];
//      gcpRendD3D->GetD3DDevice()->GetPixelShaderConstantF(n, vCur, 1);
//      pC = vCur;
//#endif
      if (pC[0] != v[0] || pC[1] != v[1] || pC[2] != v[2] || pC[3] != v[3])
      {
        memcpy(&m_CurPSParams[nReg], vData, sizeof(Vec4)*nParams);
#if defined (MERGE_SHADER_PARAMETERS)
        int nID = m_NumPSParamsToCommit;
        if (nID+nParams < 64)
        {
          for (i=0; i<nParams; i++)
          {
            m_PSParamsToCommit[nID++] = i+nReg;
          }
          m_NumPSParamsToCommit = nID;
        }
#else
        gcpRendD3D->GetD3DDevice()->SetPixelShaderConstantF(nReg, vData, nParams);
#endif
        break;
      }
      v += 4;
    }
#endif
  }

  static _inline void mfSetPSConstA(int nReg, const float *vData, int nParams)
  {
#ifdef OPENGL
    gcpRendD3D->GetD3DDevice()->SetPixelShaderConstantF(nReg, vData, nParams);
    return;
#else
    int i;
    const float *v = vData;
    for (i=0; i<nParams; i++)
    {
      int n = nReg+i;
      float *pC = &m_CurPSParams[n].x;
 #if defined(_CPU_SSE) && !defined(_DEBUG)
      __m128 a = _mm_load_ps(&v[0]);
      __m128 b = _mm_load_ps(&pC[0]);
      __m128 mask = _mm_cmpneq_ps(a, b);
      int maskBits = _mm_movemask_ps(mask);
      if (maskBits != 0)
      {
        int nID = m_NumPSParamsToCommit;
        assert(nID+nParams < 64);
        for (i=0; i<nParams; i++, nReg++)
        {
          a = _mm_load_ps(&vData[i*4]);
          m_PSParamsToCommit[nID++] = nReg;
          _mm_store_ps(&m_CurPSParams[nReg].x, a);
        }
        m_NumPSParamsToCommit = nID;
  #if !defined (MERGE_SHADER_PARAMETERS)
        gcpRendD3D->GetD3DDevice()->SetPixelShaderConstantF(nReg, vData, nParams);
  #endif
        break;
      }
 #else
      if (pC[0] != v[0] || pC[1] != v[1] || pC[2] != v[2] || pC[3] != v[3])
      {
        memcpy(&m_CurPSParams[nReg], vData, sizeof(Vec4)*nParams);

  #if defined (MERGE_SHADER_PARAMETERS)
        int nID = m_NumPSParamsToCommit;
        if (nID+nParams < 64)
        {
          for (i=0; i<nParams; i++)
          {
            m_PSParamsToCommit[nID++] = i+nReg;
          }
          m_NumPSParamsToCommit = nID;
        }
  #else
        gcpRendD3D->GetD3DDevice()->SetPixelShaderConstantF(nReg, vData, nParams);
  #endif
        break;
      }
 #endif
      v += 4;
    }
#endif
  }

  static _inline void mfSetVSConst(int nReg, const float *vData, int nParams)
  {
#ifdef OPENGL
    gcpRendD3D->GetD3DDevice()->SetVertexShaderConstantF(nReg, vData, nParams);
    return;
#else
    int i;
    const float *v = vData;
    for (i=0; i<nParams; i++)
    {
      int n = nReg+i;
      assert(n>=0 && n<MAX_CONSTANTS_VS);
      float *pC = &m_CurVSParams[n].x;
//#ifdef XENON
//      float vCur[4];
//      gcpRendD3D->GetD3DDevice()->GetVertexShaderConstantF(n, vCur, 1);
//      pC = vCur;
//#endif
      if (pC[0] != v[0] || pC[1] != v[1] || pC[2] != v[2] || pC[3] != v[3])
      {
        memcpy(&m_CurVSParams[nReg], vData, sizeof(Vec4)*nParams);
#if defined (MERGE_SHADER_PARAMETERS)
        int nID = m_NumVSParamsToCommit;
        for (i=0; i<nParams; i++)
        {
          m_VSParamsToCommit[nID++] = i+nReg;
        }
        m_NumVSParamsToCommit = nID;
#else
        gcpRendD3D->GetD3DDevice()->SetVertexShaderConstantF(nReg, vData, nParams);
#endif
        break;
      }
      v += 4;
    }
#endif
  }

  static _inline void mfSetVSConstA(int nReg, const float *vData, int nParams)
  {
#ifdef OPENGL
    gcpRendD3D->GetD3DDevice()->SetVertexShaderConstantF(nReg, vData, nParams);
    return;
#else

    int i;
    const float *v = vData;
    for (i=0; i<nParams; i++)
    {
      int n = nReg+i;
      assert(n>=0 && n<MAX_CONSTANTS_VS);

			//check if "m_CurVSParams" has a valid initialization  
			//assert(m_CurVSParams[n].x != 999999.12345f);
			//assert(m_CurVSParams[n].y != 999999.12345f);
			//assert(m_CurVSParams[n].z != 999999.12345f);
			//assert(m_CurVSParams[n].w != 999999.12345f);

      float *pC = &m_CurVSParams[n].x;
 //#ifdef XENON
 //     float vCur[4];
 //     gcpRendD3D->GetD3DDevice()->GetVertexShaderConstantF(n, vCur, 1);
 //     pC = vCur;
 //#endif
 #if defined(_CPU_SSE) && !defined(_DEBUG)
      __m128 a = _mm_load_ps(&v[0]);
      __m128 b = _mm_load_ps(&pC[0]);
      __m128 mask = _mm_cmpneq_ps(a, b);
      int maskBits = _mm_movemask_ps(mask);
      if (maskBits != 0)
      {
        int nID = m_NumVSParamsToCommit;
        assert(nID+nParams < 256);
        for (i=0; i<nParams; i++, nReg++)
        {
          a = _mm_load_ps(&vData[i*4]);
          m_VSParamsToCommit[nID++] = nReg;
          _mm_store_ps(&m_CurVSParams[nReg].x, a);
        }
        m_NumVSParamsToCommit = nID;
  #if !defined (MERGE_SHADER_PARAMETERS)
        gcpRendD3D->GetD3DDevice()->SetVertexShaderConstantF(nReg, vData, nParams);
  #endif
        break;
      }
 #else
      if (pC[0] != v[0] || pC[1] != v[1] || pC[2] != v[2] || pC[3] != v[3])
      {
        memcpy(&m_CurVSParams[nReg], vData, sizeof(Vec4)*nParams);
  #if defined (MERGE_SHADER_PARAMETERS)
        int nID = m_NumVSParamsToCommit;
        for (i=0; i<nParams; i++)
        {
          m_VSParamsToCommit[nID++] = i+nReg;
        }
        m_NumVSParamsToCommit = nID;
  #else
        gcpRendD3D->GetD3DDevice()->SetVertexShaderConstantF(nReg, vData, nParams);
  #endif
        break;
      }
 #endif
      v += 4;
    }
#endif
  }

  static _inline void mfParameterReg(int nReg, const float *v, int nComps, EHWShaderClass eSHClass)
  {
#if !defined(OPENGL) && !defined(PS3)
    assert(nReg>=0 && nReg<MAX_CONSTANTS_VS && nReg+nComps<MAX_CONSTANTS_VS);
#endif
    if (eSHClass == eHWSC_Pixel)
      mfSetPSConst(nReg, v, nComps);
    else
    if (eSHClass == eHWSC_Vertex)
      mfSetVSConst(nReg, v, nComps);
#if defined (DIRECT3D10)
    else
    if (eSHClass == eHWSC_Geometry)
      mfSetGSConst(nReg, v, nComps);
#endif
  }
  static _inline void mfParameterRegA(int nReg, const float *v, int nComps, EHWShaderClass eSHClass)
  {
#if !defined(OPENGL) && !defined(PS3)
    if (eSHClass == eHWSC_Vertex && (nReg<0 || nReg+nComps>MAX_CONSTANTS_VS))
    {
      assert(0);
      iLog->Log("Exceed maximum number of constants (%d) for vertex shader", MAX_CONSTANTS_VS);
    }
    else
    if (eSHClass == eHWSC_Pixel && (nReg<0 || nReg+nComps>MAX_CONSTANTS_PS))
    {
      assert(0);
      iLog->Log("Exceed maximum number of constants (%d) for pixel shader", MAX_CONSTANTS_PS);
    }
#endif
    if (eSHClass == eHWSC_Pixel)
      mfSetPSConstA(nReg, v, nComps);
    else
    if (eSHClass == eHWSC_Vertex)
      mfSetVSConstA(nReg, v, nComps);
#if defined (DIRECT3D10)
    else
    if (eSHClass == eHWSC_Geometry)
      mfSetGSConst(nReg, v, nComps);
#endif
  }


  static _inline void mfParameterReg_NoCheck(int nReg, const float *v, int nComps, EHWShaderClass eSHClass)
  {
    if (eSHClass == eHWSC_Pixel)
      gcpRendD3D->GetD3DDevice()->SetPixelShaderConstantF(nReg, v, nComps);
    else
      gcpRendD3D->GetD3DDevice()->SetVertexShaderConstantF(nReg, v, nComps);
  }

  static void mfParameterf(const SCGBind *ParamBind, const float *v, int nComps, EHWShaderClass eSHClass)
  {
    if(!ParamBind || ParamBind->m_dwBind < 0)
      return;
		int nReg = ParamBind->m_dwBind;
#if defined(OPENGL)
		nReg |= (ParamBind->m_isMatrix & scIs2x4Matrix)?scParam2x4MatrixBit :
						(ParamBind->m_isMatrix & scIs3x4Matrix)?scParam3x4MatrixBit :
						(ParamBind->m_isMatrix & scIs4x4Matrix)?scParam4x4MatrixBit : 0;
		nReg |= ((ParamBind->m_isMatrix & scIsArray)?scParamArrayBit : 0);
#endif
    mfParameterReg(nReg, v, nComps, eSHClass);
  }
  static void mfParameterfA(const SCGBind *ParamBind, const float *v, int nComps, EHWShaderClass eSHClass)
  {
    if(!ParamBind || ParamBind->m_dwBind < 0)
      return;
    int nReg = ParamBind->m_dwBind;
#if defined(OPENGL)
    nReg |= (ParamBind->m_isMatrix & scIs2x4Matrix)?scParam2x4MatrixBit :
      (ParamBind->m_isMatrix & scIs3x4Matrix)?scParam3x4MatrixBit :
      (ParamBind->m_isMatrix & scIs4x4Matrix)?scParam4x4MatrixBit : 0;
    nReg |= ((ParamBind->m_isMatrix & scIsArray)?scParamArrayBit : 0);
#endif
    mfParameterRegA(nReg, v, nComps, eSHClass);
  }

  static void mfParameterf(const SCGBind *ParamBind, const float *v, EHWShaderClass eSHClass)
  {
		if(!ParamBind || ParamBind->m_dwBind < 0)
			return;
		int nReg = ParamBind->m_dwBind;
#if defined(OPENGL)
		nReg |= (ParamBind->m_isMatrix == scIs2x4Matrix)?scParam2x4MatrixBit :
						(ParamBind->m_isMatrix == scIs3x4Matrix)?scParam3x4MatrixBit :
						(ParamBind->m_isMatrix == scIs4x4Matrix)?scParam4x4MatrixBit : 0;
		nReg |= ((ParamBind->m_isMatrix & scIsArray)?scParamArrayBit : 0);
#endif
    mfParameterReg(nReg, v, ParamBind->m_nParameters, eSHClass);
  }
  static void mfParameterfA(const SCGBind *ParamBind, const float *v, EHWShaderClass eSHClass)
  {
    if(!ParamBind || ParamBind->m_dwBind < 0)
      return;
    int nReg = ParamBind->m_dwBind;
#if defined(OPENGL)
    nReg |= (ParamBind->m_isMatrix == scIs2x4Matrix)?scParam2x4MatrixBit :
      (ParamBind->m_isMatrix == scIs3x4Matrix)?scParam3x4MatrixBit :
      (ParamBind->m_isMatrix == scIs4x4Matrix)?scParam4x4MatrixBit : 0;
    nReg |= ((ParamBind->m_isMatrix & scIsArray)?scParamArrayBit : 0);
#endif
    mfParameterRegA(nReg, v, ParamBind->m_nParameters, eSHClass);
  }
  static void mfParameteri(const SCGBind *ParamBind, const float *v, EHWShaderClass eSHClass)
  {
    if(!ParamBind)
      return;
    assert(ParamBind->m_dwBind >= 0 && ParamBind->m_nParameters >= 1);
    if ((int)ParamBind->m_dwBind < 0)
      return;
    int iparms[4];
    int n = ParamBind->m_dwBind;
    if (eSHClass == eHWSC_Pixel)
    {
      if (m_CurPSParamsI[n].x != v[0] || m_CurPSParamsI[n].y != v[1] || m_CurPSParamsI[n].z != v[2] || m_CurPSParamsI[n].w != v[3])
      {
        m_CurPSParamsI[n].x = v[0];
        m_CurPSParamsI[n].y = v[1];
        m_CurPSParamsI[n].z = v[2];
        m_CurPSParamsI[n].w = v[3];

        iparms[0] = (int)v[0];
        iparms[1] = (int)v[1];
        iparms[2] = (int)v[2];
        iparms[3] = (int)v[3];
        gcpRendD3D->GetD3DDevice()->SetPixelShaderConstantI(n, iparms, 1);
      }
    }
    else
    {
      if (m_CurVSParamsI[n].x != v[0] || m_CurVSParamsI[n].y != v[1] || m_CurVSParamsI[n].z != v[2] || m_CurVSParamsI[n].w != v[3])
      {
        m_CurVSParamsI[n].x = v[0];
        m_CurVSParamsI[n].y = v[1];
        m_CurVSParamsI[n].z = v[2];
        m_CurVSParamsI[n].w = v[3];

        iparms[0] = (int)v[0];
        iparms[1] = (int)v[1];
        iparms[2] = (int)v[2];
        iparms[3] = (int)v[3];
        gcpRendD3D->GetD3DDevice()->SetVertexShaderConstantI(n, iparms, 1);
      }
    }
    v += 4;
  }
  static void mfParameterb(const SCGBind *ParamBind, const float *v, EHWShaderClass eSHClass)
  {
    if(!ParamBind)
      return;

    assert(ParamBind->m_dwBind >= 0 && ParamBind->m_nParameters >= 1);
    if ((int)ParamBind->m_dwBind < 0)
      return;

    BOOL iparms;
    int n = ParamBind->m_dwBind;
    if (eSHClass == eHWSC_Pixel)
    {
      if (m_CurPSParams[n].x != v[0] || m_CurPSParams[n].y != v[1] || m_CurPSParams[n].z != v[2] || m_CurPSParams[n].w != v[3])
      {
        m_CurPSParams[n].x = v[0];
        m_CurPSParams[n].y = v[1];
        m_CurPSParams[n].z = v[2];
        m_CurPSParams[n].w = v[3];

        iparms = (BOOL)v[0];
        gcpRendD3D->GetD3DDevice()->SetPixelShaderConstantB(n, &iparms, 1);
      }
    }
    else
    {
      if (m_CurVSParams[n].x != v[0] || m_CurVSParams[n].y != v[1] || m_CurVSParams[n].z != v[2] || m_CurVSParams[n].w != v[3])
      {
        m_CurVSParams[n].x = v[0];
        m_CurVSParams[n].y = v[1];
        m_CurVSParams[n].z = v[2];
        m_CurVSParams[n].w = v[3];

        iparms = (BOOL)v[0];
        gcpRendD3D->GetD3DDevice()->SetVertexShaderConstantB(n, &iparms, 1);
      }
    }
    v += 4;
  }
  static _inline void mfBindGS(SD3DShader *pShader, void *pHandle){}

#else // defined(DIRECT3D9) || defined(OPENGL)

  static _inline void mfBindGS(SD3DShader *pShader, void *pHandle)
  {
    if (m_pCurGS != pShader)
    {
      m_pCurGS = pShader;
      gcpRendD3D->m_RP.m_PS.m_NumGShadChanges++;
      gcpRendD3D->GetD3DDevice()->GSSetShader((ID3D10GeometryShader *)pHandle);
    }
    if (!m_pCurGS)
      m_pCurInstGS = NULL;
  }
  static _inline void mfSetCB(int eClass, int nSlot, ID3D10Buffer *pBuf)
  {
//PS3HACK
#if !defined(PS3)
    if (m_pCurDevCB[eClass][nSlot] != pBuf)
#endif
    {
      m_pCurDevCB[eClass][nSlot] = pBuf;
      switch (eClass)
      {
        case eHWSC_Vertex:
          gcpRendD3D->m_pd3dDevice->VSSetConstantBuffers(nSlot, 1, &pBuf);
          break;
        case eHWSC_Pixel:
          gcpRendD3D->m_pd3dDevice->PSSetConstantBuffers(nSlot, 1, &pBuf);
          break;
        case eHWSC_Geometry:
          gcpRendD3D->m_pd3dDevice->GSSetConstantBuffers(nSlot, 1, &pBuf);
          break;
      }
    }
  }
  static _inline void mfCommitCB(int nCBufSlot, EHWShaderClass eSH)
  {
    if (!m_pDataCB[eSH][nCBufSlot])
      return;
    int cbUpdateMethod(gcpRendD3D->CV_d3d10_CBUpdateMethod);
    ID3D10Device* dv = gcpRendD3D->GetD3DDevice();
    assert(m_pCurReqCB[eSH][nCBufSlot]);
    if ((int)m_pDataCB[eSH][nCBufSlot] != 1)
    {
      if (cbUpdateMethod)
      {
        dv->UpdateSubresource(m_pCurReqCB[eSH][nCBufSlot], 0, 0, m_pDataCB[eSH][nCBufSlot], 0, 0);
        m_pDataCB[eSH][nCBufSlot] = NULL;
      }
      else
      {
        m_pDataCB[eSH][nCBufSlot] = NULL;
        m_pCurReqCB[eSH][nCBufSlot]->Unmap();
      }
    }
    else
      m_pDataCB[eSH][nCBufSlot] = NULL;
    mfSetCB(eSH, nCBufSlot, m_pCurReqCB[eSH][nCBufSlot]);
  }
  static _inline void mfSetCBConst(int nReg, int nCBufSlot, EHWShaderClass eSH, const float *vData, int nFloats, int nMaxVecs)
  {
//PS3HACK
#if defined(PS3)
		static int UseCounter			=	0;
		static int UseTestCounter	=	0;
		UseCounter++;
		if(UseCounter==UseTestCounter)
		{
			int a=0;
		}
#endif
    assert(nCBufSlot >= 0 || nCBufSlot < CB_MAX);
    assert(m_pCB[eSH][nCBufSlot]);
		assert(m_pCBShadow[eSH][nCBufSlot]);
		//assert((nReg>>2)+(nFloats>>2) <= nMaxVecs);
		assert(nReg + nFloats <= (nMaxVecs<<2));
    if (nReg+nFloats > (nMaxVecs<<2))
    {
      iLog->Log("ERROR: Attempt to modify CB: %d outside of the range (%d+%d > %d) (Shader: %s)", nCBufSlot, nReg, nFloats, nMaxVecs<<2, gRenDev->m_RP.m_pShader ? gRenDev->m_RP.m_pShader->GetName() : "Unknown");
      return;
    }
    if (m_pDataCB[eSH][nCBufSlot] && m_nCurMaxVecs[eSH][nCBufSlot] != nMaxVecs)
      mfCommitCB(nCBufSlot, eSH);
		if (!m_pDataCB[eSH][nCBufSlot])
    {
			int cbUpdateMethod(gcpRendD3D->CV_d3d10_CBUpdateMethod);

      int nMaxVecs4 = (nMaxVecs+3)>>2;
      m_nCurMaxVecs[eSH][nCBufSlot] = nMaxVecs;

      if (!m_pCB[eSH][nCBufSlot][nMaxVecs] && nMaxVecs)
      {
        D3D10_BUFFER_DESC bd;
        ZeroStruct(bd);
        HRESULT hr;

				bd.Usage = cbUpdateMethod ? D3D10_USAGE_DEFAULT : D3D10_USAGE_DYNAMIC;
        bd.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
				bd.CPUAccessFlags = cbUpdateMethod ? 0 : D3D10_CPU_ACCESS_WRITE;
        bd.MiscFlags = 0;
        bd.ByteWidth = nMaxVecs * sizeof(Vec4);
        hr = gcpRendD3D->m_pd3dDevice->CreateBuffer(&bd, NULL, &m_pCB[eSH][nCBufSlot][nMaxVecs]);
        assert(SUCCEEDED(hr));

				if (cbUpdateMethod)
				{
					m_pCBShadow[eSH][nCBufSlot][nMaxVecs] = new char[bd.ByteWidth];
					assert(m_pCBShadow[eSH][nCBufSlot][nMaxVecs]);
				}
      }

			if (cbUpdateMethod)
			{
				m_pCurReqCB[eSH][nCBufSlot] = m_pCB[eSH][nCBufSlot][nMaxVecs];
				m_pDataCB[eSH][nCBufSlot] = (float*) m_pCBShadow[eSH][nCBufSlot][nMaxVecs];
			}
			else
			{
				m_pCurReqCB[eSH][nCBufSlot] = m_pCB[eSH][nCBufSlot][nMaxVecs];
				m_pCurReqCB[eSH][nCBufSlot]->Map(D3D10_MAP_WRITE_DISCARD, NULL, (void **)&m_pDataCB[eSH][nCBufSlot]);
			}

      if (CD3D9Renderer::CV_d3d10_CBUpdateStats)
			{
				static unsigned int s_lastFrame(0);
				static unsigned int s_numCalls(0);
				static unsigned int s_minUpdateBytes(0);
				static unsigned int s_maxUpdateBytes(0);
				static unsigned int s_totalUpdateBytes(0);

				unsigned int updateBytes = (unsigned int) (nMaxVecs * sizeof(Vec4));
				unsigned int curFrame = gcpRendD3D->GetFrameID(false);
				if (s_lastFrame != curFrame)
				{
					if (s_lastFrame != 0)
					{
						unsigned int avgUpdateBytes = s_totalUpdateBytes / s_numCalls;
						gEnv->pLog->Log("-------------------------------------------------------");
						gEnv->pLog->Log("CB update statistics for frame %d:", s_lastFrame);
						gEnv->pLog->Log("#UpdateSubresource() = %d calls", s_numCalls);
						gEnv->pLog->Log("SmallestTransfer = %d kb (%d bytes)", (s_minUpdateBytes + 1023) >> 10, s_minUpdateBytes);
						gEnv->pLog->Log("BiggestTransfer = %d kb (%d bytes)", (s_maxUpdateBytes + 1023) >> 10, s_maxUpdateBytes);
						gEnv->pLog->Log("AvgTransfer = %d kb (%d bytes)", (avgUpdateBytes + 1023) >> 10, avgUpdateBytes);
						gEnv->pLog->Log("TotalTransfer = %d kb (%d bytes)", (s_totalUpdateBytes + 1023) >> 10, s_totalUpdateBytes);						
					}

					s_lastFrame = curFrame;
					s_numCalls = 1;
					s_minUpdateBytes = updateBytes;
					s_maxUpdateBytes = updateBytes;
					s_totalUpdateBytes = updateBytes;
				}
				else
				{
					++s_numCalls;
					s_minUpdateBytes = min(updateBytes, s_minUpdateBytes);
					s_maxUpdateBytes = max(updateBytes, s_maxUpdateBytes);
					s_totalUpdateBytes += updateBytes;
				}
			}
    }
    else
    {
      assert(m_nCurMaxVecs[eSH][nCBufSlot] == nMaxVecs);
    }
#ifdef DO_RENDERLOG
    if (CRenderer::CV_r_log >= 3)
    {
      const float *pData = vData;
      for (int i=0; i<nFloats>>2; i++)
      {
        gcpRendD3D->Logv(SRendItem::m_RecurseLevel, " (%.3f, %.3f, %.3f, %.3f)", pData[0], pData[1], pData[2], pData[3]);
        pData += 4;
      }
      gcpRendD3D->Logv(SRendItem::m_RecurseLevel, "\n");
    }
#endif

    memcpy(&m_pDataCB[eSH][nCBufSlot][nReg], vData, nFloats<<2);
    if (nCBufSlot == CB_PER_FRAME && eSH == eHWSC_Vertex && vData != &m_CurVSParams[0][0])
    {
      float *pDst = &m_CurVSParams[0][0];
      memcpy(&pDst[nReg], vData, nFloats<<2);
    }
  }

  static _inline void mfSetGSConst(int nReg, int nCBufSlot, const float *vData, int nParams, int nMaxVecs=32)
  {
    mfSetCBConst(nReg, nCBufSlot, eHWSC_Geometry, vData, nParams, nMaxVecs);
  }

  static _inline void mfSetPSConst(int nReg, int nCBufSlot, const float *vData, int nParams, int nMaxVecs=32)
  {
    mfSetCBConst(nReg, nCBufSlot, eHWSC_Pixel, vData, nParams, nMaxVecs);
  }

  static _inline void mfSetPSConstA(int nReg, int nCBufSlot, const float *vData, int nParams, int nMaxVecs=32)
  {
    mfSetCBConst(nReg, nCBufSlot, eHWSC_Pixel, vData, nParams, nMaxVecs);
  }

  static _inline void mfSetVSConst(int nReg, int nCBufSlot, const float *vData, int nParams, int nMaxVecs=32)
  {
    mfSetCBConst(nReg, nCBufSlot, eHWSC_Vertex, vData, nParams, nMaxVecs);
  }

  static _inline void mfSetVSConstA(int nReg, int nCBufSlot, const float *vData, int nParams, int nMaxVecs=32)
  {
    mfSetCBConst(nReg, nCBufSlot, eHWSC_Vertex, vData, nParams, nMaxVecs);
  }
  static _inline void mfSetPSConst(int nReg, const float *vData, int nParams, int nMaxVecs=32)
  {
    mfSetPSConst(nReg, CB_PER_BATCH, vData, nParams, nMaxVecs);
  }
  static _inline void mfSetVSConst(int nReg, const float *vData, int nParams, int nMaxVecs=32)
  {
    mfSetVSConst(nReg, CB_PER_BATCH, vData, nParams, nMaxVecs);
  }

  static _inline void mfSetGSConst(int nReg, const float *vData, int nParams, int nMaxVecs=32)
  {
    mfSetGSConst(nReg, CB_PER_BATCH, vData, nParams, nMaxVecs);
  }

  static _inline void mfParameterReg(int nReg, int nCBufSlot, EHWShaderClass eSH, const float *v, int nComps, int nMaxVecs)
  {
    mfSetCBConst(nReg, nCBufSlot, eSH, v, nComps, nMaxVecs);
  }

  static void mfParameterf(const SCGBind *ParamBind, const float *v, int nComps, EHWShaderClass eSH, int nMaxVecs)
  {
    if(!ParamBind || ParamBind->m_dwBind < 0)
      return;
		int nReg = ParamBind->m_dwBind;
    mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, nComps, nMaxVecs);
  }
  static void mfParameterfA(const SCGBind *ParamBind, const float *v, int nComps, EHWShaderClass eSH, int nMaxVecs)
  {
    if(!ParamBind || ParamBind->m_dwBind < 0)
      return;
    int nReg = ParamBind->m_dwBind;
    mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, nComps, nMaxVecs);
  }

  static void mfParameterf(const SCGBind *ParamBind, const float *v, EHWShaderClass eSH, int nMaxVecs)
  {
		if(!ParamBind || ParamBind->m_dwBind < 0)
			return;
		int nReg = ParamBind->m_dwBind;
    mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, ParamBind->m_nParameters, nMaxVecs);
  }
  static void mfParameterfA(const SCGBind *ParamBind, const float *v, EHWShaderClass eSH, int nMaxVecs)
  {
    if(!ParamBind || ParamBind->m_dwBind < 0)
      return;
    int nReg = ParamBind->m_dwBind;
    mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, ParamBind->m_nParameters, nMaxVecs);
  }
  static void mfParameteri(const SCGBind *ParamBind, const float *v, EHWShaderClass eSH, int nMaxVecs)
  {
    if(!ParamBind)
      return;
    int nReg = ParamBind->m_dwBind;
    mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, ParamBind->m_nParameters, nMaxVecs);
  }
  static void mfParameterb(const SCGBind *ParamBind, const float *v, EHWShaderClass eSH, int nMaxVecs)
  {
    if(!ParamBind)
      return;

    assert(ParamBind->m_dwBind >= 0 && ParamBind->m_nParameters >= 1);
    if ((int)ParamBind->m_dwBind < 0)
      return;

    int nReg = ParamBind->m_dwBind;
    mfParameterReg(nReg, ParamBind->m_dwCBufSlot, eSH, v, ParamBind->m_nParameters, nMaxVecs);
  }
#endif

  SCGBind *mfGetParameterBind(const CCryName& Name)
  {
    if (!m_pCurInst)
      return NULL;

    std::vector<SCGBind> *pBinds = m_pCurInst->m_pBindVars;
    if (!pBinds)
      return NULL;
    uint i;
    int nSize = pBinds->size();
    for (i=0; i<nSize; i++)
    {
      if (Name == (*pBinds)[i].m_Name)
        return &(*pBinds)[i];
    }
    return NULL;
  }
  void mfParameterf(const CCryName& Name, const float *v)
  {
    SCGBind *pBind = mfGetParameterBind(Name);
    if (pBind)
    {
#if defined (DIRECT3D10)
      mfParameterf(pBind, v, m_eSHClass, m_pCurInst->m_nMaxVecs[CB_PER_BATCH]);
#else
      mfParameterf(pBind, v, m_eSHClass);
#endif
    }
  }

#if defined (DIRECT3D10)
  static float *mfSetParameters(SCGParam *pParams, int nParams, float *pDst, EHWShaderClass eSH, int nMaxRegs);
#else
  static float *mfSetParameters(SCGParam *pParams, int nParams, float *pDst, EHWShaderClass eSH);
#endif

  //============================================================================

  void mfLostDevice(SHWSInstance *pInst, byte *pBuf, int nSize)
  {
    pInst->m_Handle.SetFake();
    pInst->m_Handle.m_pData = new byte[nSize];
    memcpy(pInst->m_Handle.m_pData, pBuf, nSize);
    pInst->m_Handle.m_nData = nSize;
  }

  int mfCheckActivation(SHWSInstance *&pInst, uint nFlags)
  {
    ED3DShError eError = mfIsValid(pInst, true);
    if (eError ==  ED3DShError_NotCompiled)
    {
      if (!mfActivate(nFlags))
      {
        pInst = m_pCurInst;
        if (gRenDev->m_cEF.m_bActivatePhase)
          return 0;
        if (!pInst->IsAsyncCompiling())
          pInst->m_Handle.SetNonCompilable();
        else
        {
          eError = mfIsValid(pInst, true);
          if (eError == ED3DShError_CompilingError)
            return 0;
          if (m_eSHClass == eHWSC_Vertex)
            return 1;
          else
            return -1;
        }
        return 0;
      }
      if (gRenDev->m_RP.m_pCurTechnique)
        mfGetPreprocessFlags(gRenDev->m_RP.m_pCurTechnique);
      pInst = m_pCurInst;
    }
    else
    if (eError == ED3DShError_Fake)
    {
      if (pInst->m_Handle.m_pData)
      {
        if (gRenDev && !gRenDev->CheckDeviceLost())
        {
          mfUploadHW(pInst, pInst->m_Handle.m_pData, pInst->m_Handle.m_nData);
          SAFE_DELETE_ARRAY(pInst->m_Handle.m_pData);
          pInst->m_Handle.m_nData = 0;
        }
        else
          eError = ED3DShError_CompilingError;
      }
    }
    if (eError == ED3DShError_CompilingError)
      return 0;
    return 1;
  }

  void mfSetParameters(int nType);
  bool mfSetSamplers();
  TArray<SHWSInstance> *mfGetSharedInstContainer(bool bCreate, uint GLMask, bool bPrecache);
  SHWSInstance *mfGetInstance(uint64 RTMask, uint LightMask, uint GLMask, uint MDMask, uint MDVMask, uint nFlags);
  SHWSInstance *mfGetInstance(int nInstance, uint GLMask);
  static void mfPrepareShaderDebugInfo(SHWSInstance *pInst, CHWShader_D3D *pSH, const char *szAsm, DynArray<SCGBind>& InstBindVars, LPD3DXCONSTANTTABLE pBuffer);
  void mfGetSrcFileName(char *srcName, int nSize);
  static void mfGetDstFileName(SHWSInstance *pInst, CHWShader_D3D *pSH, char *dstname, int nSize, byte bType);
  static void mfGenName(SHWSInstance *pInst, char *dstname, int nSize, byte bType);
  void CorrectScriptEnums(CParserBin& Parser, SHWSInstance *pInst, DynArray<SCGBind>& InstBindVars);
  void RemoveUnaffectedParameters_D3D10(CParserBin& Parser, SHWSInstance *pInst, DynArray<SCGBind>& InstBindVars);

public:
  char *mfGenerateScript(SHWSInstance *&pInst, DynArray<SCGBind>& InstBindVars, uint nFlags);
  bool mfActivate(uint nFlags);
  void mfPrecache();

  void SetTokenFlags(uint32 nToken);
  uint64 CheckToken(uint32 nToken);
  uint64 CheckIfExpr_r(uint32 *pTokens, uint32& nCur, uint32 nSize);
  uint64 mfConstructFX_AffectMask_RT();
  void mfConstructFX();
 
  static void mfAddFXParameter(SHWSInstance *pInst, DynArray<SFXParam>& Params, DynArray<STexSampler>& Samplers, SFXParam *pr, const char *ParamName, SCGBind *pBind, CShader *ef, bool bInstParam, EHWShaderClass eSHClass);
  static bool mfAddFXParameter(SHWSInstance *pInst, DynArray<SFXParam>& Params, DynArray<STexSampler>& Samplers, const char *param, const char *paramINT, SCGBind *bn, bool bInstParam, EHWShaderClass eSHClass);
  static void mfGatherFXParameters(SHWSInstance *pInst, std::vector<SCGBind>* BindVars, DynArray<SCGBind> *InstBindVars, CHWShader_D3D *pSH, int nFlags);

#if !defined(PS3)
  static void AnalyzeSemantic(SHWSInstance *pInst, DynArray<SFXParam>& Params, D3DXSEMANTIC *pSM, bool bUsed, bool& bPos, byte& bNormal, bool bTangent[], bool& bHWSkin, bool& bShapeDeform, bool& bMorphTarget, bool& bBoneSpace, bool& bPSize, bool bSH[], bool& bMorph, bool& bTC0, bool bTC1[], bool& bCol, bool& bSecCol, bool& bVertexScatter, DynArray<SCGBind>& InstBindVars);
#endif
  static void AddMissedInstancedParam(SHWSInstance *pInst, DynArray<SFXParam>& Params, int nIndex, DynArray<SCGBind>& InstBindVars);
  static void mfCreateBinds(SHWSInstance *pInst, LPD3DXCONSTANTTABLE pConstantTable, byte* pShader, int nSize);
  bool mfUpdateSamplers();
  static void mfPostVertexFormat(SHWSInstance *pInst, bool bCol, byte bNormal, bool bTC0, bool bTC1[2], bool bPSize, bool bTangent[2], bool bHWSkin, bool bSH[2], bool bShapeDeform, bool bMorphTarget, bool bMorph, bool bVertexScatter);
  EHWSProfile mfGetCurrentProfile()
  {
    return m_pCurInst->m_eProfileType;
  }

public:
  virtual ~CHWShader_D3D();
  virtual bool mfSet(int nFlags=0);
  virtual void mfReset(uint32 CRC32);
  virtual const char *mfGetEntryName() { return m_EntryFunc.c_str(); }
  virtual bool mfFlushCacheFile();

  // Vertex shader specific functions
  virtual int  mfVertexFormat(bool &bUseTangents, bool &bUseLM, bool &bUseHWSkin, bool& bUseSH);
  static int  mfVertexFormat(SHWSInstance *pInst, CHWShader_D3D *pSH, LPD3DXBUFFER pBuffer, DynArray<SCGBind>& InstBindVars);
  virtual uint mfGetPreprocessFlags(SShaderTechnique *pTech);

  virtual const char * mfGetActivatedCombinations();
  static  const char * mfGetSharedActivatedCombinations();

  static void mfSetLightParams(int nPass);
  static void mfSetGlobalParams();
  static void mfSetCameraParams();
  static void mfSetPF();
  static void mfSetCM();
  static bool mfAddGlobalParameter(SCGParam& Param, EHWShaderClass eSH, bool bSG, bool bCam);

  static void ShutDown();

  DEFINE_ALIGNED_DATA_STATIC(Vec4, m_CurPSParams[], 16);
  DEFINE_ALIGNED_DATA_STATIC(Vec4, m_CurVSParams[], 16);
#if defined (DIRECT3D9) || defined(OPENGL)
  DEFINE_ALIGNED_DATA_STATIC(Vec4, m_CurPSParamsI[], 16);
  DEFINE_ALIGNED_DATA_STATIC(Vec4, m_CurVSParamsI[], 16);
#elif defined (DIRECT3D10)
  static ID3D10Buffer **m_pCB[eHWSC_Max][CB_MAX];
	static void **m_pCBShadow[eHWSC_Max][CB_MAX];
  static ID3D10Buffer *m_pCurReqCB[eHWSC_Max][CB_MAX];
  static ID3D10Buffer *m_pCurDevCB[eHWSC_Max][CB_MAX];
  static float *m_pDataCB[eHWSC_Max][CB_MAX];
  static int m_nCurMaxVecs[eHWSC_Max][CB_MAX];
  static int m_nMax_PF_Vecs[eHWSC_Max];
  static int m_nMax_SG_Vecs[eHWSC_Max];
  static ID3D10Buffer *m_pLightCB[eHWSC_Max];

  static CHWShader_D3D::SHWSInstance *m_pCurInstVS;
  static CHWShader_D3D::SHWSInstance *m_pCurInstPS;
  static CHWShader_D3D::SHWSInstance *m_pCurInstGS;
#endif

	static bool ms_bInitShaders;

  static int m_PSParamsToCommit[];
  static int m_NumPSParamsToCommit;
  static int m_VSParamsToCommit[];
  static int m_NumVSParamsToCommit;

  static int m_nResetDeviceFrame;
  static int m_nInstFrame;

  static std::vector<SCGParam> m_CM_Params[eHWSC_Max]; // Per-frame parameters
  static std::vector<SCGParam> m_PF_Params[eHWSC_Max]; // Per-frame parameters
  static std::vector<SCGParam> m_SG_Params[eHWSC_Max]; // Shadow-gen parameters

  friend struct SShaderTechniqueStat;
};

#if defined(DIRECT3D10)
  bool PatchDXBCShaderCode(LPD3D10BLOB& pShader, CHWShader_D3D *pSh);
#endif

  struct SShaderTechniqueStat
  {
    SShaderTechnique *pTech;
    CShader *pShader;
    CHWShader_D3D *pVS;
    CHWShader_D3D *pPS;
    CHWShader_D3D::SHWSInstance *pVSInst;
    CHWShader_D3D::SHWSInstance *pPSInst;
  };

  extern DynArray<SShaderTechniqueStat> g_SelectedTechs;

#endif  // __D3DHWSHADER_H__
