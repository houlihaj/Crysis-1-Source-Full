
#ifndef __CSHADER_H__
#define __CSHADER_H__

#include <map>
#include "CShaderBin.h"

struct SRenderBuf;
class CRendElement;
struct SEmitter;
struct SParticleInfo;
struct SPartMoveStage;
struct SSunFlare;

#include <crc32.h>

Crc32Gen& GetCRC32Gen();

//===================================================================

#define MAX_ENVLIGHTCUBEMAPS 16
#define ENVLIGHTCUBEMAP_SIZE 16
#define MAX_ENVLIGHTCUBEMAPSCANDIST_UPDATE 16
#define MAX_ENVLIGHTCUBEMAPSCANDIST_THRESHOLD 2

#define MAX_ENVCUBEMAPS 16
#define MAX_ENVCUBEMAPSCANDIST_THRESHOLD 1

#define MAX_ENVTEXTURES 16
#define MAX_ENVTEXSCANDIST 0.1f

//===============================================================================

struct SMinMax
{
  int nMin, nMax;
};

struct SDevID
{
  string szName;
  int nVendorID;
  DynArray<SMinMax> DevMinMax;
};
struct SMSAAMode
{
  int nSamples;
  int nQuality;
  string szDescr;
};
struct SMSAAProfile
{
  string szName;
  SDevID *pDeviceGroup;
  DynArray<SMSAAMode> Modes;
};

//===============================================================================

struct SNameAlias
{
  CCryName m_Alias;
  CCryName m_Name;
  uint m_nGenMask;
  SNameAlias()
  {
    m_nGenMask = 0;
  }
};

struct SShaderTechParseParams
{
  CCryName techName[TTYPE_MAX];
};

struct SCacheCombination
{
  CCryName Name;
  CCryName CacheName;
  uint nGL;
  //uint nGLMaskAuto;
  //uint nGLAuto;
  int64 nRT;
  int nLT;
  int nMD;
  int nMDV;
  EHWSProfile ePR;
  SCacheCombination()
  {
    nGL = 0;
    //nGLMaskAuto = 0;
    //nGLAuto = 0;
    nRT = 0;
    nLT = 0;
    nMD = 0;
    nMDV = 0;
    ePR = eHWSP_Unknown;
  }
};

struct SShaderGenComb
{
  CCryName Name;
  SShaderGen *pGen;
  SShaderGenComb()
  {
    pGen = NULL;
  }
  ~SShaderGenComb()
  {
    SAFE_RELEASE(pGen);
  }

  _inline SShaderGenComb (const SShaderGenComb& src)
  {
    Name = src.Name;
    pGen = src.pGen;
    if (pGen)
      pGen->m_nRefCount++;
  }
  _inline SShaderGenComb& operator = (const SShaderGenComb& src)
  {
    this->~SShaderGenComb();
    new(this) SShaderGenComb(src);
    return *this;
  }
};

typedef std::map<CCryName, SCacheCombination> FXShaderCacheCombinations;
typedef FXShaderCacheCombinations::iterator FXShaderCacheCombinationsItor;

//===================================================================

struct SMacroFX
{
  string m_szMacro;
  uint m_nMask;
};

//typedef stl::hash_map<string,SMacroFX,stl::hash_strcmp<string> > FXMacro;
//typedef std::map<string,SMacroFX> FXMacro;
#ifdef USE_HASH_MAP
typedef stl::hash_map<string,SMacroFX,stl::hash_strcmp<const char*> > FXMacro;
#else
typedef std::map<string,SMacroFX,stl::less_strcmp<const char*> > FXMacro;
#endif

typedef FXMacro::iterator FXMacroItor;

//////////////////////////////////////////////////////////////////////////
// Helper class for shader parser, holds temporary strings vector etc...
//////////////////////////////////////////////////////////////////////////
struct CShaderParserHelper
{
	CShaderParserHelper()
	{
	}
	~CShaderParserHelper()
	{
	}

	char *GetTempStringArray( int nIndex,int nLen )
	{
		m_tempString.reserve(nLen+1);
		return (char*)&(m_tempStringArray[nIndex])[0];
	}

	std::vector<char> m_tempStringArray[32];
	std::vector<char> m_tempString;
};
extern CShaderParserHelper* g_pShaderParserHelper;

//==================================================================================

#define PD_INDEXED 1
#define PD_MERGED  4

struct SParamDB
{
  const char *szName;
  const char *szAliasName;
  ECGParam eParamType;
  uint nFlags;
  void (*ParserFunc)(const char *szScr, const char *szAnnotations, DynArray<STexSampler>* pSamplers, SCGParam *vpp, int nComp, CShader *ef);
  SParamDB()
  {
    szName = NULL;
    nFlags = 0;
    ParserFunc = NULL;
    eParamType = ECGP_Unknown;
  }
  SParamDB(const char *inName, ECGParam ePrmType, uint inFlags)
  {
    szName = inName;
    szAliasName = NULL;
    nFlags = inFlags;
    ParserFunc = NULL;
    eParamType = ePrmType;
  }
  SParamDB(const char *inName, ECGParam ePrmType, uint inFlags, void (*InParserFunc)(const char *szScr, const char *szAnnotations, DynArray<STexSampler>* pSamplers, SCGParam *vpp, int nComp, CShader *ef))
  {
    szName = inName;
    szAliasName = NULL;
    nFlags = inFlags;
    ParserFunc = InParserFunc;
    eParamType = ePrmType;
  }
};


//////////////////////////////////////////////////////////////////////////
class CShaderMan
{
friend class CShader;
friend class CParserBin;

private:
  int mfReadTexSequence(TArray<CTexture *>& txs, const char *name, byte eTT, int Flags, float fAmount1=-1.0f, float fAmount2=-1.0f);

  CShader *mfNewShader(const char *szName);

  bool mfCompileShaderGen(SShaderGen *shg, char *scr);
  SShaderGenBit *mfCompileShaderGenProperty(char *scr);

  CTexture *mfTryToLoadTexture(const char *nameTex, int Flags, byte eTT, float fAmount1=-1.0f, float fAmount2=-1.0f);
  CTexture *mfLoadResourceTexture(const char *nameTex, const char *path, int Flags, byte eTT, SEfResTexture *Tex, float fAmount1=-1.0f, float fAmount2=-1.0f);
  void mfCheckShaderResTextures(TArray<SShaderPass> &Dst, CShader *ef, SRenderShaderResources *Res);
  void mfCheckShaderResTexturesHW(TArray<SShaderPass> &Dst, CShader *ef, SRenderShaderResources *Res);
  bool mfCheckAnimatedSequence(STexSampler *tl, CTexture *tx);
  CTexture *mfCheckTemplateTexName(const char *mapname, ETEX_Type eTT, short &nFlags);

  CShader *mfCompile(CShader *ef, char *scr);

  bool mfUpdateMergeStatus(SShaderTechnique *hs, std::vector<SCGParam> *p);  
  void mfRefreshResources(SRenderShaderResources *Res, const bool bLoadNormalAlpha );

  bool mfReloadShaderFile(const char *szName, int nFlags);
	bool CheckAllFilesAreWritable( const char *szDir ) const;

  bool ParseFSAADevices(char *scr, SDevID *pDev);
  bool ParseFSAADevGroup(char *scr, SDevID *pDev);
  bool ParseFSAAMode(char *scr, SMSAAMode *pMSAA);
  bool ParseFSAAProfile(char *scr, SMSAAProfile *pMSAA);

public:
  char *m_pCurScript;
  CShaderManBin m_Bin;

  const char *mfTemplateTexIdToName(int Id);
  SShaderGenComb *CShaderMan::mfGetShaderGenInfo(const char *nmFX);

  bool mfReloadAllShaders(int nFlags, uint nFlagsHW);
  bool mfReloadFile(const char *szPath, const char *szName, int nFlags);

  void ParseShaderProfiles();
  void ParseShaderProfile(char *scr, SShaderProfile *pr);

  SParamDB *mfGetShaderParamDB(const char *szSemantic);
  const char *mfGetShaderParamName(ECGParam ePR);
  bool mfParseParamComp(int comp, SCGParam *pCurParam, DynArray<SFXParam>& Params, std::vector<SCGParam>* pParams, char *szSemantic, char *params, const char *szAnnotations, DynArray<STexSampler>* pSamplers, CShader *ef, int nComps, uint nParamFlags, EHWShaderClass eSHClass, bool bExpressionOperand);
  bool mfParseCGParam(char *scr, const char *szAnnotations, DynArray<SFXParam>& Params, DynArray<STexSampler>* pSamplers, CShader *ef, std::vector<SCGParam>* pParams, int nComps, uint nParamFlags, EHWShaderClass eSHClass, bool bExpressionOperand);
  bool mfParseFXParameter(DynArray<SFXParam>& Params, SFXParam *pr, DynArray<STexSampler>* pSamplers, const char *ParamName, CShader *ef, bool bInstParam, int nParams, std::vector<SCGParam>* pParams, EHWShaderClass eSHClass, bool bExpressionOperand);

  void ParseFSAAProfiles();

  void mfCheckObjectDependParams(std::vector<SCGParam>* PNoObj, std::vector<SCGParam>*& PObj, EHWShaderClass eSH);
  
  void mfBeginFrame();

  CShaderGraph m_GR;

public:
  bool m_bInitialized;

  const char *m_ShadersPath;
  const char *m_ShadersCache;
  const char *m_ShadersFilter;
  const char *m_ShadersMergeCachePath;
  string m_szUserPath;

  int m_nFrameForceReload;

  char m_HWPath[128];
  
  CShader *m_pCurShader;
  static SResourceContainer *m_pContainer;  // List/Map of objects for shaders resource class

  std::vector<SNameAlias> m_AliasNames;
  std::vector<SNameAlias> m_CustomAliasNames;
  std::vector<string> m_ShaderNames;

  static CShader *m_DefaultShader;
  static CShader *m_shPostEffects;  // engine specific post process effects

#ifndef NULL_RENDERER
  static CShader *m_ShaderFPEmu;
  static CShader *m_ShaderFallback;
  static CShader *m_ShaderTreeSprites;
  static CShader *m_ShaderShadowBlur;
	static CShader *m_ShaderShadowMaskGen;
  static CShader *m_ShaderAmbientOcclusion;
	static CShader *m_ShaderCloudShadowMaskGen;
	static CShader *m_ShaderPostProcess;  
  static CShader *m_shPostEffectsGame;  // game specific post process effects
  static CShader *m_ShaderDebug;
  static CShader *m_ShaderLightFlares;
  static SShaderItem m_ShaderLightStyles;
  static CShader *m_ShaderCommon;
  static CShader *m_ShaderOcclTest;
#else
  static SShaderItem m_DefaultShaderItem;
#endif

  int m_Frame;
  const SInputShaderResources *m_pCurInputResources;
  SShaderGen *m_pGlobalExt;

  Vec4 m_TempVecs[16];
  Vec4 m_RTRect;
  FILE *m_FPCacheCombinations;
  std::vector<SShaderGenComb> m_SGC;
  int m_nCombinations;
  bool m_bActivatePhase;
  int m_nCombination;
  char *m_szShaderPrecache;
  FXShaderCacheCombinations m_ShaderCacheCombinations;
  SShaderProfile m_ShaderProfiles[eST_Max];
  SShaderProfile m_ShaderFixedProfiles[eSQ_Max];

  DynArray<SDevID *> m_FSAADevGroups;
  DynArray<SMSAAProfile> m_FSAAModes;

	int m_bActivated;

	CShaderParserHelper m_shaderParserHelper;

  bool m_bReload;

public:
  CShaderMan()
  {
    m_bInitialized = false;
    m_DefaultShader = NULL;
    m_pGlobalExt = NULL;
		g_pShaderParserHelper = &m_shaderParserHelper;
    m_nCombinations = -1;
    m_nCombination = -1;
    m_szShaderPrecache = NULL;
		memset(m_TempVecs,0,sizeof(Vec4)*16);
		memset(&m_RTRect,0,sizeof(Vec4));
  }

  void ShutDown();
  void mfReleaseShaders();
  
  SShaderGen *mfCreateShaderGenInfo(const char *szName);
  void mfInitGlobal(void);
  void mfInit(void);
  void mfSortResources();
  uint mfShaderNameForAlias(const char *nameAlias, char *nameEf, int nSize, uint nGenMask);
  SRenderShaderResources *mfCreateShaderResources(const SInputShaderResources *Res, bool bShare);
  SShaderItem mfShaderItemForName (const char *szName, bool bShare, int flags, const SInputShaderResources *Res=NULL, uint nMaskGen=0);
  CShader *mfForName (const char *name, int flags, const SRenderShaderResources *Res=NULL, uint nMaskGen=0);

  SFXParam *mfGetFXParameter(DynArray<SFXParam>& Params, const char *param);
  const char *mfParseFX_Parameter (const string& buf, EParamType eType, const char *szName);
  void    mfParseFX_Annotations_Script (char *buf, CShader *ef, std::vector<SFXStruct>& Structs, bool *bPublic, CCryName techStart[2]);
  void    mfParseFX_Annotations (char *buf, CShader *ef, std::vector<SFXStruct>& Structs, bool *bPublic, CCryName techStart[2]);
  void    mfParseFXTechnique_Annotations_Script (char *buf, CShader *ef, std::vector<SFXStruct>& Structs, SShaderTechnique *pShTech, bool *bPublic, DynArray<SShaderTechParseParams>& techParams);
  void    mfParseFXTechnique_Annotations (char *buf, CShader *ef, std::vector<SFXStruct>& Structs, SShaderTechnique *pShTech, bool *bPublic, DynArray<SShaderTechParseParams>& techParams);
  void    mfParseFXSampler_Annotations_Script (char *buf, CShader *ef, std::vector<SFXStruct>& Structs, STexSampler *pSamp);
  void    mfParseFXSampler_Annotations (char *buf, CShader *ef, std::vector<SFXStruct>& Structs, STexSampler *pSamp);
  void    mfParseFX_Global (SFXParam& pr, CShader *ef, std::vector<SFXStruct>& Structs, CCryName techStart[2]);
  bool    mfParseDummyFX_Global (std::vector<SFXStruct>& Structs, char *annot, CCryName techStart[2]);
  const string& mfParseFXTechnique_GenerateShaderScript (std::vector<SFXStruct>& Structs, FXMacro& Macros, DynArray<SFXParam>& Params, DynArray<SFXParam>& AffectedParams, const char *szEntryFunc, CShader *ef, EHWShaderClass eSHClass, const char *szShaderName, uint& nAffectMask, const char *szType);
  bool    mfParseFXTechnique_MergeParameters (std::vector<SFXStruct>& Structs, DynArray<SFXParam>& Params, std::vector<int>& AffectedFunc, SFXStruct *pMainFunc, CShader *ef, EHWShaderClass eSHClass, const char *szShaderName, DynArray<SFXParam>& NewParams);
  CTexture *mfParseFXTechnique_LoadShaderTexture (STexSampler *smp, SShaderPass *pShPass, CShader *ef, int nIndex, byte ColorOp, byte AlphaOp, byte ColorArg, byte AlphaArg);
  bool    mfParseFXTechnique_LoadShader (const char *szShaderCom, SShaderPass *pShPass, CShader *ef, DynArray<STexSampler>& Samplers, std::vector<SFXStruct>& Structs, DynArray<SFXParam>& Params, FXMacro& Macros, EHWShaderClass eSHClass);
  bool    mfParseFXTechniquePass (char *buf, char *annotations, SShaderTechnique *pShTech, CShader *ef, DynArray<STexSampler>& Samplers, std::vector<SFXStruct>& Structs, DynArray<SFXParam>& Params);
  bool    mfParseFXTechnique_CustomRE (char *buf, const char *name, SShaderTechnique *pShTech, CShader *ef);
  SShaderTechnique *mfParseFXTechnique (char *buf, char *annotations, CShader *ef, DynArray<STexSampler>& Samplers, std::vector<SFXStruct>& Structs, DynArray<SFXParam>& Params, bool *bPublic, DynArray<SShaderTechParseParams>& techParams);
  bool    mfParseFXSampler(char *buf, char *name, char *annotations, CShader *ef, DynArray<STexSampler>& Samplers, std::vector<SFXStruct>& Structs);
  bool    mfParseLightStyle(CLightStyle *ls, char *buf);
  bool    mfParseFXLightStyle(char *buf, int nID, CShader *ef, std::vector<SFXStruct>& Structs);
  CShader *mfParseFX (char *buf, CShader *ef, CShader *efGen, uint nMaskGen);
  void    mfPostLoadFX(CShader *efT, DynArray<SShaderTechParseParams>& techParams, CCryName techStart[2]);
  bool     mfParseDummyFX(char *buf, std::vector<string>& ShaderNames, const char *szName);
  bool     mfAddFXShaderNames(const char *szName, std::vector<string>& ShaderNames, bool bUpdateCRC);

  uint64   mfGetRTForName(char *buf);
  uint     mfGetGLForName(char *buf, CShader *ef);

  void mfFillGenMacroses(SShaderGen *shG, TArray<char>& buf, uint nMaskGen);
  bool mfModifyGenFlags(CShader *efGen, const SRenderShaderResources *pRes, uint& nMaskGen, uint& nMaskGenHW);

  bool mfGatherShadersList(const char *szPath, bool bCheckIncludes, bool bUpdateCRC);
  void mfGatherFilesList(const char *szPath, std::vector<CCryName>& Names, int nLevel, bool bUseFilter, bool bMaterial=false);
  int  mfInitShadersList();
  void mfSetDefaults(void);
  void mfCloseShadersCache();
  void mfInitShadersCache(FXShaderCacheCombinations *Combinations=NULL, const char* pCombinations=NULL);
  const char *mfInsertNewCombination(uint nGL, uint64 nRT, uint nLT, uint nMD, uint nMDV, EHWSProfile ePR, const char *name, bool bStore=true);
  void _PrecacheShaders(FXShaderCacheCombinations *Combinations=NULL, bool bListOnly=false);
  void mfPrecacheShaders(FXShaderCacheCombinations *Combinations=NULL, bool bListOnly=false);
  void mfOptimiseShaders(const char *szFolder);
  void mfMergeShaders();
  void mfFixMaterials();
  void _MergeShaders();
  void mfAddRTCombinations(FXShaderCacheCombinations& CmbsMapSrc, FXShaderCacheCombinations& CmbsMapDst, CHWShader *pSH, bool bListOnly);
  void mfAddRTCombination_r(int nComb, FXShaderCacheCombinations& CmbsMapDst, SCacheCombination *cmb, CHWShader *pSH, bool bAutoPrecache);
  void mfAddLTCombinations(SCacheCombination *cmb, FXShaderCacheCombinations& CmbsMapDst);
  void mfAddLTCombination(SCacheCombination *cmb, FXShaderCacheCombinations& CmbsMapDst, DWORD dwL);

#ifdef WIN64
#pragma warning( push )							//AMD Port
#pragma warning( disable : 4267 )				// conversion from 'size_t' to 'XXX', possible loss of data
#endif

  int Size()
  {
    int nSize = sizeof(*this);

    nSize += m_AliasNames.size()*sizeof(SNameAlias);
    nSize += m_SGC.capacity();

    return nSize;
  }

  static float EvalWaveForm(SWaveForm *wf);
  static float EvalWaveForm(SWaveForm2 *wf);
  static float EvalWaveForm2(SWaveForm *wf, float frac);
};

#ifdef WIN64
#pragma warning( pop )							//AMD Port
#endif

//=====================================================================

#endif  // __CSHADER_H__
