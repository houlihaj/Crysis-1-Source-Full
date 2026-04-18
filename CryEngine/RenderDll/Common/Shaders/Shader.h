/*=============================================================================
  Shader.h : Shaders declarations.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#ifndef __SHADER_H__
#define __SHADER_H__

#include "../Defs.h"
#include "ShaderComponents.h"
#include "ShaderGraph.h"

// bump this value up if you want to invalidate shader cache (e.g. changed cfi include file))
// #### VIP NOTE ####: DON'T USE MORE THAN ONE DECIMAL PLACE!!!! else it doens't work...
#define FX_CACHE_VER   4.6

// Maximum 1 digit here
#define SHADER_LIST_VER 1

#define USE_PER_MATERIAL_PARAMS 1

#define NUM_VS_PM 2
#define NUM_PS_PM 3
#define NUM_GS_PM 2

#ifndef DIRECT3D10
const int FIRST_REG_PM[3] = {8, 12, 0};
#else
const int FIRST_REG_PM[3] = {0, 0, 0};
#endif

// Shader.h
// Shader pipeline common declarations.

struct SShaderPass;
class CShader;
class CRendElement;
struct SEnvTexture;
struct SParserFrame;

//=========================================================

template< class T > class TDList : public T
{
public:
  TDList* Next;
  TDList** PrevLink;
  void Unlink()
  {
    if (Next)
      Next->PrevLink = PrevLink;
    if (PrevLink)
      *PrevLink = Next;
  }
  void Link (TDList*& Before)
  {
    if (Before)
      Before->PrevLink = &Next;
    Next     = Before;
    PrevLink = &Before;
    Before   = this;
  }
};


//=========================================================

enum eCompareFunc
{
  eCF_Disable,
  eCF_Never,
  eCF_Less,
  eCF_Equal,
  eCF_LEqual,
  eCF_Greater,
  eCF_NotEqual,
  eCF_GEqual,
  eCF_Always
};


struct SPair
{
  string m_szMacroName;
  string m_szMacro;
  uint m_nMask;
};

enum EHWShaderClass
{
  eHWSC_Vertex,
  eHWSC_Pixel,
  eHWSC_Geometry,
  eHWSC_Max,
};

// In Matrix 3x4: m_nParams = 3, m_nComps = 4
struct SFXParam
{
  string m_Name;        // Parameter name
  DynArray <uint32> m_dwName;
  uint32 m_nFlags;
  short m_nParameters;  // Number of paramters
  short m_nComps;       // Number of components in single parameter
  string m_Annotations; // Additional parameters (between <>)
  string m_Assign;      // Parameter app handling type (after ':')
  string m_Values;      // Parameter values (after '=')
  byte   m_bWasMerged;
  byte   m_bAffected;
  byte   m_eType;       // EParamType
  int8   m_nCB;
  short  m_nRegister[3]; // VS, PS, GS
  SFXParam()
  {
    m_nParameters = 0;
    m_nComps = 0;
    m_bWasMerged = 0;
    m_bAffected = 0;
    m_nCB = -1;
    m_nFlags = 0;
    m_nRegister[0] = 10000;
    m_nRegister[1] = 10000;
    m_nRegister[2] = 10000;
  }
  uint   GetComponent(EHWShaderClass eSHClass);
  string GetParamComp(uint nOffset);
  uint   GetParamFlags() { return m_nFlags; }
  string GetCompName(uint nId);
  string GetValueForName(const char *szName, EParamType& eType);
  void PostLoad(class CParserBin& Parser, SParserFrame& Name, SParserFrame& Annotations, SParserFrame& Values, SParserFrame& Assign);
  void PostLoad();
};

struct STokenD
{
  std::vector<int> Offsets;
  string SToken;
};
typedef std::map<uint32, STokenD> FXShaderToken;
typedef FXShaderToken::iterator FXShaderTokenItor;


struct SFXStruct
{
  string m_Name;
  string m_Struct;
  SFXStruct()
  {
  }
};

enum ETexFilter
{
  eTEXF_None,
  eTEXF_Point,
  eTEXF_Linear,
  eTEXF_Anisotropic,
};

//=============================================================================

struct SCGParam;

#ifdef WIN64
#pragma warning( push )                 //AMD Port
#pragma warning( disable : 4267 )
#endif

#ifdef WIN64
#pragma warning( pop )                  //AMD Port
#endif

//=============================================================================
// Vertex programms / Vertex shaders (VP/VS)

//=====================================================================

static _inline float *sfparam(Vec3 param)
{
  static float sparam[4];
  sparam[0] = param.x;
  sparam[1] = param.y;
  sparam[2] = param.z;
  sparam[3] = 1.0f;

  return &sparam[0];
}

static _inline float *sfparam(float param)
{
  static float sparam[4];
  sparam[0] = param;
  sparam[1] = 0;
  sparam[2] = 0;
  sparam[3] = 1.0f;
  return &sparam[0];
}

static _inline float *sfparam(float param0, float param1, float param2, float param3)
{
  static float sparam[4];
  sparam[0] = param0;
  sparam[1] = param1;
  sparam[2] = param2;
  sparam[3] = param3;
  return &sparam[0];
}

_inline char *sGetFuncName(const char *pFunc)
{
  static char func[128];
  const char *b = pFunc;
  if (*b == '[')
  {
    const char *s = strchr(b, ']');
    if (s)
      b = s+1;
    while(*b <= 0x20)  { b++; }
  }
  while(*b > 0x20)  { b++; }
  while(*b <= 0x20)  { b++; }
  int n = 0;
  while(*b > 0x20 && *b != '(')  { func[n++] = *b++; }
  func[n] = 0;

  return func;
}

enum ERenderOrder
{
  eRO_PreProcess,
  eRO_PostProcess,
  eRO_PreDraw
};

enum ERTUpdate
{
  eRTUpdate_Unknown,
  eRTUpdate_Always,
  eRTUpdate_WaterReflect
};
/*
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
#endif
  }
  ~SD3DSurface();
};
*/
struct SHRenderTarget : public IRenderTarget
{
  int m_nRefCount;
  ERenderOrder m_eOrder;
  int m_nProcessFlags;     // FSPR_ flags
  string m_TargetName;
  int m_nWidth;
  int m_nHeight;
  ETEX_Format m_eTF;
  int m_nIDInPool;
  ERTUpdate m_eUpdateType;
  CTexture *m_pTarget[2];
  bool m_bTempDepth;
  ColorF m_ClearColor;
  float m_fClearDepth;
  uint m_nFlags;
  uint m_nFilterFlags;
	int m_refSamplerID;
  SHRenderTarget()
  {
    m_nRefCount = 1;
    m_eOrder = eRO_PreProcess;
    m_pTarget[0] = NULL;
    m_pTarget[1] = NULL;
    m_bTempDepth = true;
    m_ClearColor = Col_Black;
    m_fClearDepth = 1.f;
    m_nFlags = 0;
    m_nFilterFlags = 0xffffffff;
    m_nProcessFlags = 0;
    m_nIDInPool = -1;
    m_nWidth = 256;
    m_nHeight = 256;
    m_eTF = eTF_A8R8G8B8;
    m_eUpdateType = eRTUpdate_Unknown;
		m_refSamplerID = -1;
  }
  virtual void Release()
  {
    m_nRefCount--;
    if (m_nRefCount)
      return;
    delete this;
  }
  virtual void AddRef()
  {
    m_nRefCount++;
  }
  SEnvTexture *GetEnv2D();
  SEnvTexture *GetEnvCM();
};

//=============================================================================
// Hardware shaders

#if defined(OPENGL)
// For OpenGL/Cg we'll store parameter handles in the binding field (instead
// of register indices).  These may exceed 0x4000, so we need some more
// headroom here.
#define SHADER_BIND_SAMPLER 0x400000
#else
#define SHADER_BIND_SAMPLER 0x4000
#endif

//=============================================================================

struct SShaderCacheHeader
{
  int m_SizeOf;
  ushort m_MajorVer;
  ushort m_MinorVer;
  char m_szVer[16];
  uint32 m_CRC32;
  bool m_bOptimised;
  bool m_bReserved1;
  bool m_bReserved2;
  bool m_bReserved3;
  SShaderCacheHeader()
  {
    m_SizeOf = sizeof(SShaderCacheHeader);
    m_bOptimised = false;
    m_bReserved1 = false;
    m_bReserved2 = false;
    m_bReserved3 = false;
  }
};

struct SShaderCacheHeaderItem
{
  byte m_nVertexFormat;
  byte m_Profile;
  byte m_nInstBinds;
  byte m_StreamMask_Stream;
  ushort m_StreamMask_Decl;
  short m_nInstructions;
  uint m_CRC32;
  int m_DeviceObjectID;
  SShaderCacheHeaderItem()
  {
    memset(this, 0, sizeof(SShaderCacheHeaderItem));
  }
};

#define MAX_VAR_NAME 512
struct SShaderCacheHeaderItemVar
{
  int m_Reg;
  short m_nCount;
  char m_Name[MAX_VAR_NAME];
  SShaderCacheHeaderItemVar()
  {
    memset(this, 0, sizeof(SShaderCacheHeaderItemVar));
  }
};

struct SShaderCacheHeaderItemRemap
{
  short m_Reg;
  short m_NewReg;
  short m_CB;
  short m_NewCB;
  SShaderCacheHeaderItemRemap()
  {
    memset(this, 0, sizeof(SShaderCacheHeaderItemRemap));
  }
};

typedef std::map<int, struct SD3DShader *> FXDeviceShader;
typedef FXDeviceShader::iterator FXDeviceShaderItor;
#define SH_UNIQ_ID   0x4000
#define SH_SHARED_ID 0x2000

#define CACHE_USER 0
#define CACHE_READONLY 1

struct SShaderCache
{
  int m_nRefCount;
  CCryName m_Name;
  SShaderCacheHeader m_Header[2];
  class CResFile *m_pRes[2];
  FXDeviceShader m_DeviceShaders;
  int m_nDeviceShadersCounterUniq;
  int m_nDeviceShadersCounterShared;
  bool m_bD3D10;
  bool m_bReadOnly[2];
  bool m_bNeedPrecache;
  SShaderCache()
  {
    m_bD3D10 = false;
    m_nRefCount = 1;
    m_nDeviceShadersCounterUniq = SH_UNIQ_ID;
    m_nDeviceShadersCounterShared = SH_SHARED_ID;
    m_pRes[0] = m_pRes[1] = NULL;
    m_bNeedPrecache = false;
  }
  int Size();
  int Release()
  {
    m_nRefCount--;
    if (m_nRefCount)
      return m_nRefCount;
    delete this;
    return 0;
  }
  ~SShaderCache();
};

typedef std::map<CCryName, SShaderCache *> FXShaderCache;
typedef FXShaderCache::iterator FXShaderCacheItor;

typedef std::map<string, uint32> FXShaderCacheNames;
typedef FXShaderCacheNames::iterator FXShaderCacheNamesItor;

#define CB_PER_BATCH     0
#define CB_PER_INSTANCE  1
#define CB_MAX_PER_SH    2
#define CB_PER_FRAME     2
#define CB_PER_MATERIAL  3
#define CB_PER_LIGHT     4
#define CB_PER_SHADOWGEN 5
#define CB_SKIN_DATA     6
#define CB_SHAPE_DATA    7
#define CB_INSTANCE_DATA 8
#define CB_MAX           9

//====================================================================

struct SSkyInfo
{
  CTexture *m_SkyBox[3];
  float m_fSkyLayerHeight;

  int Size()
  {
    int nSize = sizeof(SSkyInfo);
    return nSize;
  }
  SSkyInfo()
  {
    memset(this, 0, sizeof(SSkyInfo));
  }
  ~SSkyInfo();
};


//====================================================================

#define PS_DIFFUSE_COL    0
#define PS_SPECULAR_COL   1
#define PS_EMISSIVE_COL   2

struct SRenderShaderResources : SBaseShaderResources, IRenderShaderResources
{
  uint m_Id;
  int m_nRefCounter;
  SEfResTexture *m_Textures[EFTT_MAX];
  TArray<struct SHRenderTarget *> m_RTargets;
  CCamera *m_pCamera;
  SSkyInfo *m_pSky;
  SDeformInfo *m_pDeformInfo;  
  DynArray<Vec4> m_Constants[3];
  void *m_pCB[3];                 // PM Constant Buffers (DX10 only)

  int m_nLastTexture;
  int m_nFrameLoad;
  uint8 m_nMtlLayerNoDrawFlags;
  float m_fMinDistanceLoad;

  void AddTextureMap(int Id)
  {
    assert (Id >=0 && Id < EFTT_MAX);
    m_Textures[Id] = new SEfResTexture;
  }
  int Size()
  {
    int nSize = sizeof(SRenderShaderResources);
    for (int i=0; i<EFTT_MAX; i++)
    {
      if (m_Textures[i])
        nSize += m_Textures[i]->Size();
    }
    nSize += m_Constants[0].get_alloc_size();
    nSize += m_Constants[1].get_alloc_size();
    nSize += m_RTargets.GetMemoryUsage();
    if (m_pDeformInfo)
      nSize += m_pDeformInfo->Size();
    return nSize;
  }
  SRenderShaderResources& operator=(const SRenderShaderResources& src);
  SRenderShaderResources(struct SInputShaderResources *pSrc);

  void PostLoad(CShader *pSH);
  virtual void UpdateConstants(IShader *pSH);
  virtual void CloneConstants(const IRenderShaderResources* pSrc);
  virtual void ExportModificators(IRenderShaderResources* pTrg, CRenderObject *pObj);
  virtual int GetResFlags() { return m_ResFlags; }
  virtual void SetMaterialName(const char *szName) { m_szMaterialName = szName; }
  virtual CCamera *GetCamera() { return m_pCamera; }
  virtual void SetCamera(CCamera *pCam) { m_pCamera = pCam; }
  virtual float& GetGlow() { return Glow(); }
  virtual float& GetOpacity() { return Opacity(); }
  virtual float& GetAlphaRef() { return m_AlphaRef; }
  virtual SEfResTexture *GetTexture(int nSlot) const { return m_Textures[nSlot]; }
  virtual DynArray<SShaderParam>& GetParameters() { return m_ShaderParams; }

  virtual void SetMtlLayerNoDrawFlags(uint8 nFlags) { m_nMtlLayerNoDrawFlags = nFlags; }
  virtual uint8 GetMtlLayerNoDrawFlags() const { return m_nMtlLayerNoDrawFlags; }

  void ReleaseConstants();
  inline float& Opacity()
  {
    return m_Constants[1][0][3];
  }
  inline float& Glow()
  {
    return m_Constants[1][2][3];
  }

  void Reset()
  {
    for (int i=0; i<EFTT_MAX; i++)
    {
      m_Textures[i] = NULL;
    }
    m_Id = 0;
    m_nLastTexture = 0;
    m_pDeformInfo = NULL;    
    m_pCamera = NULL;
    m_pSky = NULL;
    m_pCB[0] = m_pCB[1] = m_pCB[2] = NULL;
    m_nMtlLayerNoDrawFlags = 0;
  }
  SRenderShaderResources()
  {
    Reset();
  }
  bool IsEmpty(int nTSlot) const
  {
    if (!m_Textures[nTSlot])
      return true;
    return false;
  }
  virtual void SetInputLM(const CInputLightMaterial& lm);
  virtual void ToInputLM(CInputLightMaterial& lm);
  virtual ColorF& GetDiffuseColor();
  virtual ColorF& GetSpecularColor();
  virtual ColorF& GetEmissiveColor();
  virtual float& GetSpecularShininess();

  ~SRenderShaderResources();
  virtual void Release()
  {
#ifdef NULL_RENDERER
    return;
#endif
    m_nRefCounter--;
    if (!m_nRefCounter)
      delete this;
  }
  virtual void AddRef() { m_nRefCounter++; }
  virtual void ConvertToInputResource(SInputShaderResources *pDst);
  virtual SRenderShaderResources *Clone();
  virtual void SetShaderParams(SInputShaderResources *pDst, IShader *pSH);
};


//====================================================================
// HWShader run-time flags
#define HWSR_FOG        0
#define HWSR_AMBIENT    1
#define HWSR_SHADOW_MIXED_MAP_G16R16 2
#define HWSR_HDR_SYSLUMINANCE      3
#define HWSR_HDR_HISTOGRAMM        4
#define HWSR_ALPHATEST  5
#define HWSR_FSAA       6
#define HWSR_HDR_MODE   7
#define HWSR_HDR_FAKE   8
#define HWSR_SAMPLE1    9
#define HWSR_SAMPLE2    10
#define HWSR_SAMPLE3    11
#define HWSR_SHADOW_FILTER  12
#define HWSR_ALPHABLEND 13
#define HWSR_QUALITY    14
#define HWSR_QUALITY1   15
#define HWSR_INSTANCING_ROT   16
#define HWSR_INSTANCING_ATTR  17
#define HWSR_HW_PCF_COMPARE   18
#define HWSR_ENVLIGHT   19
#define HWSR_SPHERICAL  20
#define HWSR_SHAPEDEFORM 21
#define HWSR_MORPHTARGET 22
#define HWSR_SKELETON_SSD  23
#define HWSR_NORMAL_DXT5   24
#define HWSR_OBJ_IDENTITY  25
#define HWSR_NEAREST       26
#define HWSR_NOZPASS       27
#define HWSR_DISSOLVE      28
#define HWSR_SOFT_PARTICLE 29
#define HWSR_SHADOW_JITTERING 30
#define HWSR_HDR_ENCODE 31
#define HWSR_SAMPLE0			32
#define HWSR_RAE_GEOMTERM 33
#define HWSR_PSM		    34
#define HWSR_HDR_MRT_ALPHABLEND 35
#define HWSR_SKYLIGHT_BASED_FOG 36

#define HWSR_DEBUG0     37
#define HWSR_DEBUG1     38
#define HWSR_DEBUG2     39
#define HWSR_DEBUG3     40

#define HWSR_CUBEMAP0   41
#define HWSR_CUBEMAP1   42
#define HWSR_CUBEMAP2   43
#define HWSR_CUBEMAP3   44

#define HWSR_VEGETATION 45
#define HWSR_DECAL_TEXGEN_2D 46
#define HWSR_DECAL_TEXGEN_3D 47

#define HWSR_SCATTERSHADE 48
#define HWSR_GSM_COMBINED 49

#define HWSR_BLEND_ALPHA_MULTI 50

#define HWSR_VARIANCE_SM       51

#define HWSR_BLEND_WITH_TERRAIN_COLOR 52
#define HWSR_AMBIENT_OCCLUSION 53

#define HWSR_PARTICLE       54

#define HWSR_LIGHT_TEX_PROJ 55

#define HWSR_SAMPLE4        56
#define HWSR_SPECULAR       57
#define HWSR_SHADER_LOD     58

#define HWSR_TEX_ARR_SAMPLE 59
#define HWSR_POINT_LIGHT     60

#define HWSR_OCEAN_PARTICLE 61

#define HWSR_VERTEX_SCATTER 62

#define HWSR_FSAA_QUALITY   63

#define HWSR_MAX            64

extern uint64 g_HWSR_MaskBit[HWSR_MAX];

// HWShader global flags (m_Flags)
#define HWSG_20ONLY     2
#define HWSG_30ONLY     4
#define HWSG_2XONLY     8
#define HWSG_40ONLY     0x10
#define HWSG_SUPPORTS_LIGHTING    0x20
#define HWSG_SUPPORTS_MULTILIGHTS 0x40
#define HWSG_SUPPORTS_MODIF  0x80
#define HWSG_SUPPORTS_VMODIF 0x100
#define HWSG_WASGENERATED    0x200
#define HWSG_NOSPECULAR      0x400
#define HWSG_SYNC            0x800
#define HWSG_AUTOENUMTC      0x1000
#define HWSG_UNIFIEDPOS      0x2000
#define HWSG_DEFAULTPOS      0x4000
#define HWSG_PROJECTED       0x8000
#define HWSG_NOISE           0x10000
#define HWSG_PRECACHEPHASE   0x20000
#define HWSG_FP_EMULATION    0x40000
#define HWSG_SHARED          0x80000

// HWShader per-instance Modificator flags (SHWSInstance::m_MDMask)
// Vertex shader specific

// Texture projected flags (4)
#define HWMD_TCPROJ0    0x1
// Texture type flags (4) (0: 2D, 1: CubeMap)
#define HWMD_TCTYPE0    0x10
// Texture transform flags (4)
#define HWMD_TCM0       0x100
// Object linear texgen flags (4)
#define HWMD_TCGOL0     0x1000
// Reflection map texgen flags (4)
#define HWMD_TCGRM0     0x10000
// Normal map texgen flags (4)
#define HWMD_TCGNM0     0x100000
// Sphere map texgen flags (4)
#define HWMD_TCGSM0     0x1000000

#define HWMD_TCG        0xfffff000
#define HWMD_TCM        0xf00
#define HWMD_TCMASK     (HWMD_TCG | HWMD_TCM)


// HWShader per-instance vertex modificator flags (SHWSInstance::m_MDVMask)
// Texture projected flags (4 bits)
#define HWMDV_TYPE      0


// HWShader input flags (passed via mfSet function)
#define HWSF_SETPOINTERSFORSHADER 1
#define HWSF_SETPOINTERSFORPASS   2
#define HWSF_PRECACHE             4
#define HWSF_SETTEXTURES          8

#define HWSF_INSTANCED            0x20
#define HWSF_NEXT                 0x100
#define HWSF_PRECACHE_INST        0x200

enum EHWSProfile
{
  eHWSP_Unknown,
  eHWSP_VS_1_1,
  eHWSP_VS_2_0,
  eHWSP_VS_3_0,
  eHWSP_VS_4_0,
  eHWSP_VS_Auto,

  eHWSP_PS_1_1,
  eHWSP_PS_1_3,
  eHWSP_PS_1_4,
  eHWSP_PS_2_0,
  eHWSP_PS_2_X,
  eHWSP_PS_3_0,
  eHWSP_PS_4_0,
  eHWSP_PS_Auto,

  eHWSP_GS_4_0,
};

class CHWShader : public CBaseResource
{
  static CCryName m_sClassNameVS;
  static CCryName m_sClassNamePS;

public:
  EHWShaderClass m_eSHClass;

  static struct SD3DShader *m_pCurPS;
  static struct SD3DShader *m_pCurVS;
  static struct SD3DShader *m_pCurGS;

  string m_NameShader;
  string m_NameSourceFX;
  int m_nFrame;
  CCryName m_EntryFunc;
  uint64 m_nMaskAffect_RT;
  uint m_nMaskGenShader;          // Masked/Optimised m_nMaskGenFX for this specific HW shader
  uint m_nMaskGenFX;              // FX Shader should be parsed with this flags

  int m_nFrameLoad;
  int m_Flags;
  uint32 m_CRC32;
  uint32 m_dwShaderType;

public:
  CHWShader()
  {
    m_nFrame = 0;
    m_nFrameLoad = 0;
    m_Flags = 0;
    m_nMaskGenShader = 0;
    m_nMaskAffect_RT = 0;
    m_CRC32 = 0;
    m_nMaskGenFX = 0;
  }
  virtual ~CHWShader() {}

#if !defined (NULL_RENDERER)

  static CHWShader *mfForName(const char *name, const char *nameSource, uint32 CRC32, DynArray<STexSampler>& Samplers, DynArray<SFXParam>& Params, uint32 dwEntryFunc, EHWShaderClass eClass, EHWSProfile eSHV, DynArray<uint32>& SHData, FXShaderToken& m_Table, uint32 dwType, uint nMaskGen=0, uint nMaskGenFX=0);
#endif
  static void mfReloadScript(const char *szPath, const char *szName, int nFlags, uint nMaskGen);
  static void mfFlushPendedShaders();

  virtual int  Size() = 0;
  virtual void mfReset(uint32 CRC32) {}
  virtual bool mfSet(int nFlags=0) = 0;
  virtual const char *mfGetCurScript() {return NULL;}
  virtual const char *mfGetEntryName() = 0;
  virtual uint mfGetPreprocessFlags(SShaderTechnique *pTech) = 0;
  virtual bool mfFlushCacheFile() = 0;

  // Vertex shader specific functions
  virtual void mfSetParameters(int nType) = 0;
  virtual int  mfVertexFormat(bool &bUseTangents, bool &bUseLM, bool &bUseHWSkin, bool& bUseSH) = 0;

  virtual const char * mfGetActivatedCombinations() = 0;

  static const char *mfProfileToString(EHWSProfile eProfile);
  static EHWSProfile mfStringToProfile(const char *profile);

  static void mfBeginFrame(int nMaxFlush);

  static CCryName mfGetClassName(EHWShaderClass eClass)
  {
    if (eClass == eHWSC_Vertex)
      return m_sClassNameVS;
    else
      return m_sClassNamePS;
  }

  static const char *GetCurrentShaderCombinations(void);
  static bool SetCurrentShaderCombinations(const char *szCombinations);

  static byte *mfIgnoreRemapsFromCache(int nRemaps, byte *pP);
  static byte *mfIgnoreBindsFromCache(int nParams, byte *pP);
  static bool mfOptimiseCacheFile(SShaderCache *pCache);
  static bool mfMarkCacheOptimised(bool bOptimised, SShaderCache *pCache);
  static SShaderCache *mfInitCache(const char *name, CHWShader *pSH, bool bCheckValid, uint32 CRC32, bool bDontUseUserFolder, bool bReadOnly);
  static bool _OpenCacheFile(float fVersion, SShaderCache *pCache, CHWShader *pSH, bool bCheckValid, uint32 CRC32, int nCache, CResFile *pRF, bool bReadOnly);
  static bool mfOpenCacheFile(const char *szName, float fVersion, SShaderCache *pCache, CHWShader *pSH, bool bCheckValid, uint32 CRC32, bool bDontUseUserFolder, bool bReadOnly);
  static bool mfIsSharedCacheValid(CResFile *pRF);
  static bool mfInsertSharedIdent(SShaderCache *pCache, uint32 CRC32, const char *szNameFX);
  static FXShaderCacheNames m_ShaderCacheList;
  static FXShaderCache m_ShaderCache;
};

_inline void SortLightTypes(int Types[4], int nCount)
{
  switch(nCount)
  {
    case 2:
      if (Types[0] > Types[1])
        Exchange(Types[0], Types[1]);
      break;
    case 3:
      if (Types[0] > Types[1])
        Exchange(Types[0], Types[1]);
      if (Types[0] > Types[2])
        Exchange(Types[0], Types[2]);
      if (Types[1] > Types[2])
        Exchange(Types[1], Types[2]);
      break;
    case 4:
      {
        for (int i=0; i<4; i++)
        {
          for (int j=i; j<4; j++)
          {
            if (Types[i] > Types[j])
              Exchange(Types[i], Types[j]);
          }
        }
      }
      break;
  }
}

//=========================================================================
// Dynamic lights evaluating via shaders

enum ELightStyle
{
  eLS_Intensity,
  eLS_RGB,
};

enum ELightMoveType
{
  eLMT_Wave,
  eLMT_Patch,
};

struct SLightMove
{
  ELightMoveType m_eLMType;
  SWaveForm m_Wave;
  Vec3 m_Dir;
  float m_fSpeed;

  int Size()
  {
    int nSize = sizeof(SLightMove);
    return nSize;
  }
};

//
struct SLightEval
{
  SLightEval()
  {
    memset(this, 0, sizeof(SLightEval));
  }

  SLightMove *m_LightMove;
  ELightStyle m_EStyleType;  
  int m_LightStyle;
  SCGParam m_ProjRotate;
  Ang3 m_LightRotate;
  Vec3 m_LightOffset;

  int Size()
  {
    int nSize = sizeof(SLightEval);
    if (m_LightMove)
      nSize += m_LightMove->Size();

    return nSize;
  }
  ~SLightEval()
  {
    SAFE_DELETE(m_LightMove);
  }
};

class CLightStyle
{
public:
  TArray<ColorF> m_Map;
  float m_TimeIncr;
  ColorF m_Color;
  float m_fIntensity;
  float m_LastTime;

  static TArray <CLightStyle *> m_LStyles;  

  int Size()
  {
    int nSize = sizeof(CLightStyle);
    nSize += m_Map.GetMemoryUsage();
    return nSize;
  }

  CLightStyle()
  {
    m_LastTime = 0;
    m_fIntensity = 1.0f;
    m_Color = Col_White;
    m_TimeIncr = 60.0f;
  }
  static _inline CLightStyle *mfGetStyle(uint nStyle, float fTime)
  {
    if (nStyle >= m_LStyles.Num() || !m_LStyles[nStyle])
      return NULL;
    m_LStyles[nStyle]->mfUpdate(fTime);
    return m_LStyles[nStyle];
  }
  _inline void mfUpdate(float fTime)
  {
    float m = fTime * m_TimeIncr;
    if (m != m_LastTime)
    {
      m_LastTime = m;
      if (m_Map.Num())
      {
        if (m_Map.Num() == 1)
        {
          m_Color = m_Map[0];
        }
        else
        {
          int first = (int)QInt(m);
          int second = (first + 1);
          ColorF A = m_Map[first % m_Map.Num()];
          ColorF B = m_Map[second % m_Map.Num()];
          float fLerp = m-(float)first;
          m_Color = LERP(A, B, fLerp);
        }
        m_fIntensity = (m_Color.r * 0.3f) + (m_Color.g * 0.59f) + (m_Color.b * 0.11f);
      }
    }
  }
};


//=========================================================================
// HW Shader Layer

struct ICVar;

#define SHPF_AMBIENT         0x100
#define SHPF_HASLM           0x200
#define SHPF_SHADOW          0x400
#define SHPF_RADIOSITY       0x800
#define SHPF_ALLOW_SPECANTIALIAS 0x1000
#define SHPF_BUMP            0x2000
#define SHPF_NOMATSTATE      0x4000
#define SHPF_FORCEZFUNC      0x8000

struct CVarCond
{
  ICVar *m_Var;
  float m_Val;

  int Size()
  {
    int nSize = sizeof(CVarCond);
    return nSize;
  }
};

// Shader pass definition for HW shaders
struct SShaderPass
{ 
  uint m_RenderState;          // Render state flags
  signed char m_eCull;
  uchar m_AlphaRef;

  ushort m_PassFlags;              // Different usefull Pass flags (SHPF_)

  CHWShader *m_VShader;         // Pointer to the vertex shader for the current pass
  CHWShader *m_PShader;         // Pointer to fragment shader
  CHWShader *m_GShader;         // Pointer to the geometry shader for the current pass
  SShaderPass();

  int Size()
  {
    int nSize = sizeof(SShaderPass);
    return nSize;
  }
  void mfFree()
  {
    SAFE_RELEASE(m_VShader);
    SAFE_RELEASE(m_PShader);
    SAFE_RELEASE(m_GShader);
  }

  SShaderPass& operator = (const SShaderPass& sl)
  {
    memcpy(this, &sl, sizeof(this));
    if (sl.m_VShader)
      sl.m_VShader->AddRef();
    if (sl.m_PShader)
      sl.m_PShader->AddRef();
    if (sl.m_GShader)
      sl.m_GShader->AddRef();

    return *this;
  }
};

//===================================================================================
// Hardware Stage for HW only Shaders

#define FHF_FIRSTLIGHT   8
#define FHF_FORANIM      0x10
#define FHF_TERRAIN      0x20
#define FHF_NOMERGE      0x40
#define FHF_DETAILPASS   0x80
#define FHF_LIGHTPASS    0x100
#define FHF_FOGPASS      0x200
#define FHF_PUBLIC       0x400
#define FHF_NOLIGHTS     0x800
#define FHF_POSITION_INVARIANT 0x1000
#define FHF_RE_FLARE     0x10000
#define FHF_RE_CLOUD     0x20000
#define FHF_TRANSPARENT  0x40000
#define FHF_WASZWRITE    0x80000
#define FHF_USE_GEOMETRY_SHADER 0x100000

struct SShaderTechnique
{
  CCryName m_Name;
  TArray <SShaderPass> m_Passes;         // General passes
  int m_Flags;                           // Different flags (FHF_)
  int8 m_nTechnique[TTYPE_MAX];          // Next technique in sequence
  TArray<CRendElement *> m_REs;          // List of all render elements registered in the shader
  TArray<SHRenderTarget *> m_RTargets;
  SLightEval *m_EvalLights;              // Light evaluation parameters (only for light shaders)
  float m_fProfileTime;

  int Size()
  {
    uint i;
    int nSize = sizeof(SShaderTechnique);
    for (i=0; i<m_Passes.Num(); i++)
    {
      nSize += m_Passes[i].Size();
    }
    nSize += m_RTargets.GetMemoryUsage();
    return nSize;
  }
  SShaderTechnique()
  {
    uint i;
    for (i=0; i<TTYPE_MAX; i++)
    {
      m_nTechnique[i] = -1;
    }
    for (i=0; i<m_REs.Num(); i++)
    {
      SAFE_DELETE(m_REs[i]);
    }
    m_REs.Free();

    m_Flags = 0;
    m_EvalLights = NULL;
    m_fProfileTime = 0;
  }
  SShaderTechnique& operator = (const SShaderTechnique& sl)
  {
    uint i;
    memcpy(this, &sl, sizeof(SShaderTechnique));
    if (sl.m_Passes.Num())
    {
      m_Passes.Copy(sl.m_Passes);
      for (uint i=0; i<sl.m_Passes.Num(); i++)
      {
        const SShaderPass *s = &sl.m_Passes[i];
        SShaderPass *d = &m_Passes[i];
        *d = *s;
      }
    }
    if (sl.m_REs.Num())
    {
      m_REs.Create(sl.m_REs.Num());
      for (i=0; i<sl.m_REs.Num(); i++)
      {
        if (sl.m_REs[i])
          m_REs[i] = sl.m_REs[i]->mfCopyConstruct();
      }
    }

    return *this;
  }

  ~SShaderTechnique()
  {
    uint i;
    for (i=0; i<m_Passes.Num(); i++)
    {
      SShaderPass *sl = &m_Passes[i];
      
      sl->mfFree();
    }
    for (i=0; i<m_REs.Num(); i++)
    {
      CRendElement *pRE = m_REs[i];
      SAFE_RELEASE(pRE);
    }
    m_REs.Free();
    m_Passes.Free();
    SAFE_DELETE(m_EvalLights);
  }
  uint GetPreprocessFlags(CShader *pSH);

  void* operator new( size_t Size ) { void *ptr = malloc(Size); memset(ptr, 0, Size); return ptr; }
	void* operator new( size_t Size, const std::nothrow_t& nothrow ) { void *ptr = malloc(Size); if (ptr) memset(ptr, 0, Size); return ptr; }
  void operator delete( void *Ptr ) { free(Ptr); }
};

//===============================================================================

//Timur[9/16/2002]
struct SMapTexInfoOld
{
  char name[64];
  float shift[2];
  float rotate;
  float scale[2];
  int   contents;
  int   flags;
  int   value;

  SMapTexInfoOld()
  {
    memset(this, 0, sizeof(SMapTexInfoOld));
    strcpy(name, "T00/notex");
    scale[0] = scale[1] = 0.5f;
  }
  ~SMapTexInfoOld()
  {
  }
  void SetName(const char *p)
  {
    strcpy(name, p);
  }
};

struct STechniqueSelector
{
  ICVar *m_pCVarTech;
  float m_fValRefTech;
  eCompareFunc m_eCompTech;
  int m_nTech[2];
  STechniqueSelector()
  {
    m_nTech[0] = -1;
    m_nTech[1] = -1;
    m_pCVarTech = NULL;
  }
};

struct SSideMaterial;

enum EShaderDrawType
{
  eSHDT_General,
  eSHDT_Light,
  eSHDT_Shadow,
  eSHDT_Terrain,
  eSHDT_Overlay,
  eSHDT_OceanShore,  
  eSHDT_Fur,
  eSHDT_NoDraw,
  eSHDT_CustomDraw,
  eSHDT_Sky
};

// General Shader structure
class CShader : public IShader, public CBaseResource
{
  static CCryName m_sClassName;
public:
  string m_NameFile;     // } FIXME: This fields order is very important
  string m_NameShader;   
  EShaderDrawType m_eSHDType;     // } Check CShader::operator = in ShaderCore.cpp for more info
  
  uint32 m_Flags;              // Different flags EF_  (see IShader.h)
  uint32 m_Flags2;             // Different flags EF2_ (see IShader.h)
  uint32 m_nMDV;            // Vertex modificator flags

  int m_VertexFormatId;     // Base vertex format for the shader (see VertexFormats.h)
  ECull m_eCull;            // Global culling type
  
  STechniqueSelector *m_pSelectTech;
  TArray <SShaderTechnique *> m_HWTechniques;  // Hardware techniques
  DynArray<SShaderParam> m_PublicParams;         // Shader public params (can be enumerated by func.: GetPublicParams)
    
  EShaderType m_eShaderType;

  uint m_nMaskGenFX;
  SShaderGen *m_ShaderGenParams;   // BitMask params used in automatic script generation
  TArray<CShader *> *m_DerivedShaders;
  CShader *m_pGenShader;

  int m_nRefreshFrame; // Current frame for shader reloading (to avoid multiple reloading)
  int64 m_ModifTime;
  uint32 m_CRC32;

//============================================================================

  inline int mfGetID() { return CBaseResource::GetID(); }

  void mfFree();
  CShader()
  {
    m_eCull = (ECull)-1;
    m_CRC32 = 0;
    m_nMDV = 0;
  }
  virtual ~CShader();

  //===================================================================================

  // IShader interface
  virtual int AddRef() { return CBaseResource::AddRef(); }
  virtual int Release()
  {
    if (m_Flags & EF_SYSTEM)
      return -1;
    return CBaseResource::Release();
  }
  virtual int ReleaseForce()
  {
    m_Flags &= ~EF_SYSTEM;
    int nRef = 0;
    while (true)
    {
      int nRef = Release();
      if (nRef <= 0)
        break;
    }
    return nRef;
  }
  virtual int GetID() { return CBaseResource::GetID(); }
  virtual int GetRefCounter() const { return CBaseResource::GetRefCounter(); }
  virtual const char *GetName() { return m_NameShader.c_str(); }
  virtual const char *GetName() const { return m_NameShader.c_str(); }

  // D3D Effects interface
  bool FXSetTechnique(const CCryName& Name);
  bool FXSetPSFloat(const CCryName& NameParam, const Vec4 fParams[], int nParams);
  bool FXSetVSFloat(const CCryName& NameParam, const Vec4 fParams[], int nParams);
  bool FXBegin(uint *uiPassCount, uint nFlags);
  bool FXBeginPass(uint uiPass);
  bool FXCommit(const uint nFlags);
  bool FXEndPass();
  bool FXEnd();

  virtual int GetFlags() { return m_Flags; }
  virtual int GetFlags2() { return m_Flags2; }
  virtual void SetFlags2(int Flags) { m_Flags2 |= Flags; }

  virtual bool Reload(int nFlags, const char *szShaderName);
  virtual void mfFlushCache();
  virtual int GetTechniqueID(int nTechnique, int nRegisteredTechnique)
  {
    if (nTechnique < 0)
      nTechnique = 0;
    if ((int)m_HWTechniques.Num() <= nTechnique)
      return -1;
    SShaderTechnique *pTech = m_HWTechniques[nTechnique];
    return pTech->m_nTechnique[nRegisteredTechnique];
  }
  virtual TArray<CRendElement *> *GetREs (int nTech)
  {
    if (nTech < 0)
      nTech = 0;
    if (nTech < (int)m_HWTechniques.Num())
    {
      SShaderTechnique *pTech = m_HWTechniques[nTech];
      return &pTech->m_REs;
    }
    return NULL;
  }
  virtual int GetTexId ();
  virtual unsigned int GetUsedTextureTypes (void);
  virtual int GetVertexFormat(void) { return m_VertexFormatId; }
  virtual uint GetGenerationMask() { return m_nMaskGenFX; }
  virtual ECull GetCull(void)
  {
    if (m_HWTechniques.Num())
    {
      SShaderTechnique *pTech = m_HWTechniques[0];
      if (pTech->m_Passes.Num())
        return (ECull)pTech->m_Passes[0].m_eCull;
    }
    return eCULL_None;
  }
  virtual SShaderGen* GetGenerationParams()
  {
    if (m_ShaderGenParams)
      return m_ShaderGenParams;
    if (m_pGenShader)
      return m_pGenShader->m_ShaderGenParams;
    return NULL;
  }

  virtual DynArray<SShaderParam>& GetPublicParams()
  {
    return m_PublicParams;
  }
  virtual EShaderType GetShaderType() { return m_eShaderType; }
  virtual uint32 GetVertexModificator() { return m_nMDV; }

  //=======================================================================================

  SShaderTechnique *mfFindTechnique(CCryName name)
  {
    uint i;
    for (i=0; i<m_HWTechniques.Num(); i++)
    {
      SShaderTechnique *pTech = m_HWTechniques[i];
      if (pTech->m_Name == name)
        return pTech;
    }
    return NULL;
  }
  SShaderTechnique *mfGetStartTechnique(int nTechnique);

  virtual ITexture *GetBaseTexture(int *nPass, int *nTU);
  
  CShader& operator = (const CShader& src);
  CTexture *mfFindBaseTexture(TArray<SShaderPass>& Passes, int *nPass, int *nTU);

  int mfSize();

  // All loaded shaders resources list
  static TArray<SRenderShaderResources *> m_ShaderResources_known;
  
  virtual int Size(int Flags)
  {
    return mfSize();
  }

  void* operator new( size_t Size ) { void *ptr = malloc(Size); memset(ptr, 0, Size); return ptr; }
	void* operator new( size_t Size, const std::nothrow_t &nothrow ) { void *ptr = malloc(Size); if (ptr) memset(ptr, 0, Size); return ptr; }
  void operator delete( void *Ptr ) { free(Ptr); }

  static CCryName mfGetClassName()
  {
    return m_sClassName;
  }
};


//////////////////////////////////////////////////////////////////////////


#endif

