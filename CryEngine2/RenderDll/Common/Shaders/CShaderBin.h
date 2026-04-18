
#ifndef __CSHADERBIN_H__
#define __CSHADERBIN_H__

#include <map>
#include "ParserBin.h"

#ifndef FOURCC
	typedef DWORD FOURCC;
#endif

struct SShaderTechParseParams;
class CShaderMan;

struct SShaderBinHeader
{
  FOURCC m_Magic;
  uint32 m_CRC32;
  uint16 m_VersionLow;
  uint16 m_VersionHigh;
  uint32 m_OffsetStringTable;
  uint32 m_nTokens;

	AUTO_STRUCT_INFO
};

#if defined (XENON) || defined (PS3)
# define MAX_FXBIN_CACHE 16
#else
# define MAX_FXBIN_CACHE 32
#endif

struct SShaderBin
{
  static SShaderBin m_Root;
  static uint32 m_nCache;

  SShaderBin *m_Next;
  SShaderBin *m_Prev;

  uint32 m_CRC32;
  uint32 m_dwName;
  string m_szName;
  uint64 m_Time;
  bool m_bLocked;
  FXShaderToken m_Table;
  DynArray<uint32> m_Tokens;

  SShaderBin()
  {
    m_bLocked = false;
    m_Next = NULL;
    m_Prev = NULL;
    if (!m_Root.m_Next)
    {
      m_Root.m_Next = &m_Root;
      m_Root.m_Prev = &m_Root;
    }
    m_Time = 0;
    m_dwName = 0;
    m_CRC32 = 0;
  }

  _inline void Unlink()
  {
    if (!m_Next || !m_Prev)
      return;
    m_Next->m_Prev = m_Prev;
    m_Prev->m_Next = m_Next;
    m_Next = m_Prev = NULL;
  }
  _inline void Link( SShaderBin* Before )
  {
    if (m_Next || m_Prev)
      return;
    m_Next = Before->m_Next;
    Before->m_Next->m_Prev = this;
    Before->m_Next = this;
    m_Prev = Before;
  }
  _inline void Lock() { m_bLocked = true; }
  _inline void Unlock() { m_bLocked = false; }

  void UpdateCRC(bool bStore);
  void CryptData();
};

typedef std::map<CCryName, uint32> FXShaderBinCRC;
typedef FXShaderBinCRC::iterator FXShaderBinCRCItor;

typedef std::map<CCryName, string> FXShaderBinPath;
typedef FXShaderBinPath::iterator FXShaderBinPathItor;

class CShaderManBin
{
  SShaderBin *LoadBinShader(FILE *fpBin, const char *szName, const char *szNameBin);
  SShaderBin *SaveBinShader(const char *szName, bool bInclude, FILE *fpSrc);
  void AddGenMacroses(SShaderGen *shG, CParserBin& Parser, uint nMaskGen);

  bool ParseBinFX_Global_Annotations(CParserBin& Parser, SParserFrame& Frame, bool *bPublic, CCryName techStart[2]);
  bool ParseBinFX_Global(CParserBin& Parser, SParserFrame& Frame, bool *bPublic, CCryName techStart[2]);
  bool ParseBinFX_Sampler_Annotations_Script(CParserBin& Parser, SParserFrame& Frame, STexSampler *pSampler);
  bool ParseBinFX_Sampler_Annotations(CParserBin& Parser, SParserFrame& Frame, STexSampler *pSampler);
  bool ParseBinFX_Sampler(CParserBin& Parser, SParserFrame& Data, uint32 dwName, SParserFrame Annotations);

  char *AddAffectedOperand(CParserBin& Parser, DynArray<SFXParam>& AffectedParams, DynArray<int>& AffectedFunc, char *szOp, int& nCB);
  char *AddAffectedParametersForExpression(CParserBin& Parser, DynArray<SFXParam>& AffectedParams, DynArray<int>& AffectedFunc, const char *szExpr, int& nCB);
  void AddAffectedParameter(CParserBin& Parser, DynArray<SFXParam>& AffectedParams, DynArray<int>& AffectedFunc, SFXParam *pParam, EHWShaderClass eSHClass, uint32 dwType, SShaderTechnique *pShTech);
  void CheckFunctionsAffect_r(CParserBin& Parser, SCodeFragment *pFunc, DynArray<byte>& bChecked, DynArray<int>& AffectedFuncs);
  void CheckFragmentsAffect_r(CParserBin& Parser, DynArray<byte>& bChecked, DynArray<int>& AffectedFuncs);
  void CheckStructureAffect_r(CParserBin& Parser, SCodeFragment *pFrag, DynArray<byte>& bChecked, DynArray<int>& AffectedFunc);
  void AddParameterToScript(CParserBin& Parser, SFXParam *pr, DynArray<uint32>& SHData, EHWShaderClass eSHClass, int nCB);
  void GeneratePublicParameters(CParserBin& Parser, DynArray<SFXParam>& AffectedParams, DynArray<uint32>& NewTokens);
  bool ParseBinFX_Technique_Pass_PackParameters (CParserBin& Parser, DynArray<SFXParam>&AffectedParams, DynArray<int>& AffectedFunc, SCodeFragment *pFunc, EHWShaderClass eSHClass, uint32 dwSHName, DynArray<SFXParam>& PackedParams, DynArray<SCodeFragment>& Replaces, DynArray<uint32>& NewTokens);
  bool ParseBinFX_Technique_Pass_GenerateShaderData(CParserBin& Parser, FXMacroBin& Macros, DynArray<SFXParam>& AffectedParams, uint32 dwSHName, EHWShaderClass eSHClass, uint32& nGenMask, uint32 dwSHType, DynArray<uint32>& SHData, SShaderTechnique *pShTech);
  bool ParseBinFX_Technique_Pass_LoadShader(CParserBin& Parser, FXMacroBin& Macros, SParserFrame& SHFrame, SShaderTechnique *pShTech, SShaderPass *pPass, EHWShaderClass eSHClass);
  bool ParseBinFX_Technique_Pass(CParserBin& Parser, SParserFrame& Frame, SShaderTechnique *pTech);
  bool ParseBinFX_Technique_Annotations_String(CParserBin& Parser, SParserFrame& Frame, SShaderTechnique *pSHTech, DynArray<SShaderTechParseParams>& techParams, bool *bPublic);
  bool ParseBinFX_Technique_Annotations(CParserBin& Parser, SParserFrame& Frame, SShaderTechnique *pSHTech, DynArray<SShaderTechParseParams>& techParams, bool *bPublic);
  bool ParseBinFX_Technique_CustomRE(CParserBin& Parser, SParserFrame& Frame, SParserFrame& Name, SShaderTechnique *pShTech);
  SShaderTechnique *ParseBinFX_Technique(CParserBin& Parser, SParserFrame& Data, SParserFrame Annotations, DynArray<SShaderTechParseParams>& techParams, bool *bPublic);
  bool ParseBinFX_LightStyle_Val(CParserBin& Parser, SParserFrame& Frame, CLightStyle *ls);
  bool ParseBinFX_LightStyle(CParserBin& Parser, SParserFrame& Frame, int nStyle);

  SShaderBin *SearchInCache(const char *szName, bool bInclude);
  bool AddToCache(SShaderBin *pSB);
  bool DeleteFromCache(SShaderBin *pSB);

public:
  CShaderManBin();
  SShaderBin *GetBinShader(const char *szName, bool bInclude, bool *pbChanged=NULL);
  bool ParseBinFX(SShaderBin *pBin, CShader *ef, CShader *efGen, uint32 nMaskGen);
  bool ParseBinFX_Dummy(SShaderBin *pBin, std::vector<string>& ShaderNames, const char *szName);

  void InvalidateCache();

  CShaderMan *m_pCEF;
  FXShaderBinPath m_BinPaths;
  FXShaderBinCRC m_BinCRCs;
};

//=====================================================================

#endif  // __CSHADERBIN_H__
